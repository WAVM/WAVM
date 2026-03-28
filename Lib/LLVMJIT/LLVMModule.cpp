#include <stddef.h>
#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "LLVMJITPrivate.h"
#include "WAVM/DWARF/DWARF.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/ObjectLinker/ObjectLinker.h"
#include "WAVM/Platform/Diagnostics.h"
#include "WAVM/Platform/Memory.h"
#include "WAVM/Platform/RWMutex.h"
#include "WAVM/Platform/Unwind.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"

#ifndef _WIN32
#include <cxxabi.h>
#include <typeinfo>
#define USE_WINDOWS_SEH 0
#else
#define USE_WINDOWS_SEH 1
#endif

using namespace WAVM;
using namespace WAVM::LLVMJIT;

namespace WAVM { namespace Runtime {
	struct ExceptionType;
}}

struct LLVMJIT::GlobalModuleState
{
	// A map from address to loaded JIT symbols.
	Platform::RWMutex addressToModuleMapMutex;
	std::map<Uptr, LLVMJIT::Module*> addressToModuleMap;

	static const std::shared_ptr<GlobalModuleState>& get()
	{
		static std::shared_ptr<GlobalModuleState> singleton = std::make_shared<GlobalModuleState>();
		return singleton;
	}

	GlobalModuleState() {}
	~GlobalModuleState() {}
};

// Tracks the linked image memory for cleanup.
struct LLVMJIT::ModuleMemoryManager
{
	ModuleMemoryManager() : imageBase(nullptr), numPages(0) {}

	~ModuleMemoryManager()
	{
		if(imageBase)
		{
			Platform::freeVirtualPages(imageBase, numPages);
			Platform::deregisterVirtualAllocation(numPages << Platform::getBytesPerPageLog2());
		}
	}

	U8* imageBase;
	Uptr numPages;
};

Module::Module(std::vector<U8> objectBytes,
			   const HashMap<std::string, Uptr>& importedSymbolMap,
			   bool shouldLogMetrics,
			   std::string&& inDebugName)
: debugName(std::move(inDebugName))
, memoryManager(new ModuleMemoryManager())
, globalModuleState(GlobalModuleState::get())
{
	Timing::Timer loadObjectTimer;

	// Link the object using the ObjectLinker.
	ObjectLinker::LinkResult linkResult;
	ObjectLinker::linkObject(objectBytes.data(), objectBytes.size(), importedSymbolMap, linkResult);

	// Transfer ownership of the linked image to the memory manager.
	memoryManager->imageBase = linkResult.imageBase;
	memoryManager->numPages = linkResult.numImagePages;

	// Flush the instruction cache for the code segment.
	if(linkResult.numCodeBytes > 0)
	{
		Platform::flushInstructionCache(linkResult.imageBase, linkResult.numCodeBytes);
	}

	// Register unwind sections for exception handling.
	unwindRegistration = Platform::registerUnwindData(
		linkResult.imageBase, linkResult.numImagePages, linkResult.unwindInfo);

	// Iterate over the defined symbols from the linker.
	Uptr imageBase = reinterpret_cast<Uptr>(linkResult.imageBase);
	Uptr imageEnd = imageBase + (linkResult.numImagePages << Platform::getBytesPerPageLog2());

	for(const auto& symbolInfo : linkResult.definedSymbols)
	{
		const std::string& name = symbolInfo.name;
		Uptr loadedAddress = symbolInfo.address;
		Uptr symbolSize = symbolInfo.size;

		// Add the function to the module's name and address to function maps.
		// The symbol name from the linker is already demangled.
		Runtime::Function* function
			= (Runtime::Function*)(loadedAddress - offsetof(Runtime::Function, code));
		nameToFunctionMap.addOrFail(std::string(name), function);
		addressToFunctionMap.emplace(loadedAddress + symbolSize, function);

		// Initialize the function mutable data.
		WAVM_ASSERT(function->mutableData);
		function->mutableData->jitModule = this;
		function->mutableData->function = function;
		function->mutableData->numCodeBytes = symbolSize;
	}

	// Only insert into the address map if we have valid memory allocated.
	if(linkResult.imageBase)
	{
		const Uptr moduleEndAddress = imageEnd;
		Platform::RWMutex::ExclusiveLock addressToModuleMapLock(
			globalModuleState->addressToModuleMapMutex);
		globalModuleState->addressToModuleMap.emplace(moduleEndAddress, this);
	}

	// Store DWARF section pointers for signal-safe source location lookup.
	dwarfSections = linkResult.dwarf;

	// Keep the object bytes for GDB. On ELF, section addresses are patched in the bytes
	// so GDB sees runtime addresses directly.
	this->objectBytes = std::move(objectBytes);

	// Register the object with GDB for debugging. GDB reads debug info from the object bytes
	// on demand. The linker has already patched section addresses so GDB can correlate
	// debug info with the loaded code.
	// On macOS, skip GDB JIT registration for MachO objects: system libunwind scans objects
	// registered via __jit_debug_descriptor for DWARF FDEs. MachO objects without __eh_frame
	// cause libunwind to misinterpret other sections (e.g. __debug_line) as FDE data, leading
	// to crashes in decodeFDE during stack unwinding.
#if !defined(__APPLE__)
	gdbRegistrationHandle
		= registerObjectWithGDB(this->objectBytes.data(), this->objectBytes.size());
#endif

	if(shouldLogMetrics)
	{
		Timing::logRatePerSecond((std::string("Loaded ") + debugName).c_str(),
								 loadObjectTimer,
								 (F64)this->objectBytes.size() / 1024.0 / 1024.0,
								 "MiB");
		Log::printf(Log::Category::metrics,
					"Code: %.1f KiB, read-only data: %.1f KiB, read-write data: %.1f KiB\n",
					linkResult.numCodeBytes / 1024.0,
					linkResult.numReadOnlyBytes / 1024.0,
					linkResult.numReadWriteBytes / 1024.0);
	}
}

Module::~Module()
{
	// Deregister the unwind data.
	if(unwindRegistration) { Platform::deregisterUnwindData(unwindRegistration); }

	// Unregister from GDB.
	unregisterObjectWithGDB(gdbRegistrationHandle);

	// Remove the module from the global address to module map (only if we inserted it).
	if(memoryManager->imageBase)
	{
		Platform::RWMutex::ExclusiveLock addressToModuleMapLock(
			globalModuleState->addressToModuleMapMutex);
		globalModuleState->addressToModuleMap.erase(
			globalModuleState->addressToModuleMap.find(reinterpret_cast<Uptr>(
				memoryManager->imageBase
				+ (memoryManager->numPages << Platform::getBytesPerPageLog2()))));
	}

	// Free the FunctionMutableData objects.
	for(const auto& pair : addressToFunctionMap) { delete pair.second->mutableData; }

	// Delete the memory manager.
	delete memoryManager;
}

std::shared_ptr<LLVMJIT::Module> LLVMJIT::loadModule(
	const std::vector<U8>& objectFileBytes,
	HashMap<std::string, FunctionBinding>&& wavmIntrinsicsExportMap,
	std::vector<IR::FunctionType>&& types,
	std::vector<FunctionBinding>&& functionImports,
	std::vector<TableBinding>&& tables,
	std::vector<MemoryBinding>&& memories,
	std::vector<GlobalBinding>&& globals,
	std::vector<ExceptionTypeBinding>&& exceptionTypes,
	InstanceBinding instance,
	Uptr tableReferenceBias,
	const std::vector<Runtime::FunctionMutableData*>& functionDefMutableDatas,
	std::string&& debugName)
{
	// Bind undefined symbols in the compiled object to values.
	HashMap<std::string, Uptr> importedSymbolMap;

	// Bind the wavmIntrinsic function symbols; the compiled module assumes they have the intrinsic
	// calling convention, so no thunking is necessary.
	for(auto exportMapPair : wavmIntrinsicsExportMap)
	{
		importedSymbolMap.addOrFail(exportMapPair.key,
									reinterpret_cast<Uptr>(exportMapPair.value.code));
	}

	// Bind the type ID symbols.
	for(Uptr typeIndex = 0; typeIndex < types.size(); ++typeIndex)
	{
		importedSymbolMap.addOrFail(getExternalName("typeId", typeIndex),
									types[typeIndex].getEncoding().impl);
	}

	// Bind imported function symbols.
	for(Uptr importIndex = 0; importIndex < functionImports.size(); ++importIndex)
	{
		importedSymbolMap.addOrFail(getExternalName("functionImport", importIndex),
									reinterpret_cast<Uptr>(functionImports[importIndex].code));
	}

	// Bind the table symbols.
	for(Uptr tableIndex = 0; tableIndex < tables.size(); ++tableIndex)
	{
		const TableBinding& table = tables[tableIndex];
		importedSymbolMap.addOrFail(getExternalName("tableOffset", tableIndex),
									offsetof(Runtime::CompartmentRuntimeData, tables)
										+ sizeof(Runtime::TableRuntimeData) * table.id);
		importedSymbolMap.addOrFail(getExternalName("tableId", tableIndex), table.id);
	}

	// Bind the memory symbols.
	for(Uptr memoryIndex = 0; memoryIndex < memories.size(); ++memoryIndex)
	{
		const MemoryBinding& memory = memories[memoryIndex];
		importedSymbolMap.addOrFail(getExternalName("memoryOffset", memoryIndex),
									offsetof(Runtime::CompartmentRuntimeData, memories)
										+ sizeof(Runtime::MemoryRuntimeData) * memory.id);
		importedSymbolMap.addOrFail(getExternalName("memoryId", memoryIndex), memory.id);
	}

	// Bind the globals symbols.
	for(Uptr globalIndex = 0; globalIndex < globals.size(); ++globalIndex)
	{
		const GlobalBinding& globalSpec = globals[globalIndex];
		Uptr value;
		if(globalSpec.type.isMutable)
		{
			value = offsetof(Runtime::ContextRuntimeData, mutableGlobals)
					+ globalSpec.mutableGlobalIndex * sizeof(IR::UntaggedValue);
		}
		else
		{
			value = reinterpret_cast<Uptr>(globalSpec.immutableValuePointer);
		}
		importedSymbolMap.addOrFail(getExternalName("global", globalIndex), value);
	}

	// Bind exception type symbols.
	for(Uptr exceptionTypeIndex = 0; exceptionTypeIndex < exceptionTypes.size();
		++exceptionTypeIndex)
	{
		importedSymbolMap.addOrFail(getExternalName("exceptionTypeId", exceptionTypeIndex),
									exceptionTypes[exceptionTypeIndex].id);
	}

	// Bind FunctionMutableData symbols.
	for(Uptr functionDefIndex = 0; functionDefIndex < functionDefMutableDatas.size();
		++functionDefIndex)
	{
		Runtime::FunctionMutableData* functionMutableData
			= functionDefMutableDatas[functionDefIndex];
		importedSymbolMap.addOrFail(getExternalName("functionDefMutableDatas", functionDefIndex),
									reinterpret_cast<Uptr>(functionMutableData));
	}

	// Bind the instance symbol.
	WAVM_ASSERT(instance.id != UINTPTR_MAX);
	importedSymbolMap.addOrFail("instanceId", instance.id);

	// Bind the tableReferenceBias symbol.
	importedSymbolMap.addOrFail("tableReferenceBias", tableReferenceBias);

#if !USE_WINDOWS_SEH
	// Get the std::type_info for Runtime::Exception* without enabling RTTI.
	std::type_info* runtimeExceptionPointerTypeInfo = nullptr;
	try
	{
		throw (Runtime::Exception*)nullptr;
	}
	catch(Runtime::Exception*)
	{
		runtimeExceptionPointerTypeInfo = __cxxabiv1::__cxa_current_exception_type();
	}

	importedSymbolMap.addOrFail("runtimeExceptionTypeInfo",
								reinterpret_cast<Uptr>(runtimeExceptionPointerTypeInfo));
#endif

	// Add LLVM runtime symbols (memcpy, personality function, etc.)
	// that LLVM-generated code may reference.
	addLLVMRuntimeSymbols(importedSymbolMap);

	// Load the module.
	return std::make_shared<Module>(objectFileBytes, importedSymbolMap, true, std::move(debugName));
}

Uptr LLVMJIT::getInstructionSourceByAddress(Uptr address,
											InstructionSource* outSources,
											Uptr maxSources)
{
	if(maxSources == 0) { return 0; }

	auto globalModuleState = GlobalModuleState::get();

	Module* jitModule;
	{
		Platform::RWMutex::ShareableLock addressToModuleMapLock(
			globalModuleState->addressToModuleMapMutex);

		auto moduleIt = globalModuleState->addressToModuleMap.upper_bound(address);
		if(moduleIt == globalModuleState->addressToModuleMap.end()) { return 0; }
		jitModule = moduleIt->second;
	}

	auto functionIt = jitModule->addressToFunctionMap.upper_bound(address);
	if(functionIt == jitModule->addressToFunctionMap.end()) { return 0; }
	Runtime::Function* function = functionIt->second;
	const Uptr codeAddress = reinterpret_cast<Uptr>(function->code);
	if(address < codeAddress || address >= codeAddress + function->mutableData->numCodeBytes)
	{
		return 0;
	}

	// Query DWARF info using the signal-safe, zero-allocation DWARF parser.
	if(jitModule->dwarfSections.debugInfo)
	{
		DWARF::SourceLocation locations[16];
		Uptr numLocations
			= DWARF::getSourceLocations(jitModule->dwarfSections, address, locations, 16);

		Log::printf(Log::traceDwarf,
					"DWARF lookup: address=0x%" WAVM_PRIxPTR
					" function=%s numLocations=%" WAVM_PRIuPTR "\n",
					address,
					function->mutableData->debugName.c_str(),
					numLocations);
		for(Uptr i = 0; i < numLocations; ++i)
		{
			Log::printf(Log::traceDwarf,
						"  location[%" WAVM_PRIuPTR "]: linkageName=%s name=%s line=%" WAVM_PRIuPTR
						"\n",
						i,
						locations[i].linkageName ? locations[i].linkageName : "(null)",
						locations[i].name ? locations[i].name : "(null)",
						locations[i].line);
		}

		if(numLocations > 0)
		{
			Uptr numResults = 0;
			for(Uptr i = 0; i < numLocations && numResults < maxSources; ++i)
			{
				Runtime::Function* frameFunction = function;
				if(numLocations > 1 && locations[i].linkageName)
				{
					const char* name = locations[i].linkageName;
					std::string_view lookupName(name);
					Log::printf(Log::traceDwarf,
								"  Looking up '%.*s' in nameToFunctionMap\n",
								(int)lookupName.size(),
								lookupName.data());
					Runtime::Function** resolvedFunction
						= jitModule->nameToFunctionMap.get(lookupName);
					if(resolvedFunction) { frameFunction = *resolvedFunction; }
					else
					{
						Log::printf(Log::traceDwarf, "  Name lookup FAILED\n");
						// Name lookup failed; fall through to fallback.
						break;
					}
				}
				outSources[numResults++] = {frameFunction, locations[i].line};
			}
			if(numResults > 0) { return numResults; }
		}
	}
	else
	{
		Log::printf(Log::traceDwarf,
					"No DWARF sections for address 0x%" WAVM_PRIxPTR " function=%s\n",
					address,
					function->mutableData->debugName.c_str());
	}

	// Fallback: no DWARF frames available.
	Log::printf(Log::traceDwarf,
				"Falling back to {function=%s, 0}\n",
				function->mutableData->debugName.c_str());
	outSources[0] = {function, 0};
	return 1;
}

#include "LLVMJIT.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/HashMap.h"
#include "Inline/Lock.h"
#include "Inline/Timing.h"
#include "Logging/Logging.h"
#include "RuntimePrivate.h"

#include "LLVMPreInclude.h"

#include "llvm/Analysis/Passes.h"
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/ExecutionEngine/ObjectMemoryBuffer.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/SymbolSize.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Memory.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"

#include "LLVMPostInclude.h"

// This needs to be 1 to allow debuggers such as Visual Studio to place breakpoints and step through
// the JITed code.
#define USE_WRITEABLE_JIT_CODE_PAGES WAVM_DEBUG

#define DUMP_UNOPTIMIZED_MODULE WAVM_DEBUG
#define VERIFY_MODULE WAVM_DEBUG
#define DUMP_OPTIMIZED_MODULE WAVM_DEBUG
#define DUMP_OBJECT WAVM_DEBUG
#define PRINT_DISASSEMBLY 0

using namespace IR;
using namespace LLVMJIT;
using namespace Runtime;

llvm::LLVMContext* LLVMJIT::llvmContext = nullptr;
llvm::Type* LLVMJIT::llvmValueTypes[(Uptr)ValueType::num];

llvm::Type* LLVMJIT::llvmI8Type;
llvm::Type* LLVMJIT::llvmI16Type;
llvm::Type* LLVMJIT::llvmI32Type;
llvm::Type* LLVMJIT::llvmI64Type;
llvm::Type* LLVMJIT::llvmI128Type;
llvm::Type* LLVMJIT::llvmF32Type;
llvm::Type* LLVMJIT::llvmF64Type;
llvm::Type* LLVMJIT::llvmVoidType;
llvm::Type* LLVMJIT::llvmBoolType;
llvm::Type* LLVMJIT::llvmI8PtrType;
llvm::Type* LLVMJIT::llvmIptrType;

llvm::Type* LLVMJIT::llvmI8x16Type;
llvm::Type* LLVMJIT::llvmI16x8Type;
llvm::Type* LLVMJIT::llvmI32x4Type;
llvm::Type* LLVMJIT::llvmI64x2Type;
llvm::Type* LLVMJIT::llvmI128x1Type;
llvm::Type* LLVMJIT::llvmF32x4Type;
llvm::Type* LLVMJIT::llvmF64x2Type;

llvm::Type* LLVMJIT::llvmExceptionPointersStructType;

llvm::Constant* LLVMJIT::typedZeroConstants[(Uptr)ValueType::num];

Platform::Mutex LLVMJIT::llvmMutex;

// The JIT compilation unit for a WebAssembly module instance.
struct JITModule : JITUnit, JITModuleBase
{
	const ModuleInstance* moduleInstance;

	JITModule(const ModuleInstance* inModuleInstance) : moduleInstance(inModuleInstance) {}

	virtual JITSymbol* notifySymbolLoaded(const char* name,
										  Uptr baseAddress,
										  Uptr numBytes,
										  std::map<U32, U32>&& offsetToOpIndexMap) override
	{
		// Save the address range this function was loaded at for future address->symbol lookups.
		Uptr functionIndex;
		if(!getFunctionIndexFromExternalName(name, functionIndex)) { return nullptr; }
		else
		{
			wavmAssert(moduleInstance);
			wavmAssert(functionIndex < moduleInstance->functions.size());
			wavmAssert(functionIndex
					   >= moduleInstance->functions.size() - moduleInstance->functionDefs.size());
			FunctionInstance* function = moduleInstance->functions[functionIndex];
			function->nativeFunction   = reinterpret_cast<void*>(baseAddress);

			return new JITSymbol(function, baseAddress, numBytes, std::move(offsetToOpIndexMap));
		}
	}
};

static std::map<std::string, const char*> runtimeSymbolMap = {
#ifdef _WIN32
	// the LLVM X86 code generator calls __chkstk when allocating more than 4KB of stack space
	{"__chkstk", "__chkstk"},
	{"__C_specific_handler", "__C_specific_handler"},
#ifndef _WIN64
	{"__aullrem", "_aullrem"},
	{"__allrem", "_allrem"},
	{"__aulldiv", "_aulldiv"},
	{"__alldiv", "_alldiv"},
#endif
#else
	{"__CxxFrameHandler3", "__CxxFrameHandler3"},
	{"__cxa_begin_catch", "__cxa_begin_catch"},
	{"__gxx_personality_v0", "__gxx_personality_v0"},
#endif
#ifdef __arm__
	{"__aeabi_uidiv", "__aeabi_uidiv"},
	{"__aeabi_idiv", "__aeabi_idiv"},
	{"__aeabi_idivmod", "__aeabi_idivmod"},
	{"__aeabi_uldiv", "__aeabi_uldiv"},
	{"__aeabi_uldivmod", "__aeabi_uldivmod"},
	{"__aeabi_unwind_cpp_pr0", "__aeabi_unwind_cpp_pr0"},
	{"__aeabi_unwind_cpp_pr1", "__aeabi_unwind_cpp_pr1"},
#endif
};

llvm::JITEvaluatedSymbol LLVMJIT::resolveJITImport(llvm::StringRef name)
{
	// Allow some intrinsics used by LLVM
	auto runtimeSymbolNameIt = runtimeSymbolMap.find(name);
	if(runtimeSymbolNameIt == runtimeSymbolMap.end()) { return llvm::JITEvaluatedSymbol(nullptr); }

	const char* lookupName = runtimeSymbolNameIt->second;
	void* addr             = llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(lookupName);
	if(!addr)
	{
		Errors::fatalf("LLVM generated code references undefined external symbol: %s\n",
					   lookupName);
	}
	return llvm::JITEvaluatedSymbol(reinterpret_cast<Uptr>(addr), llvm::JITSymbolFlags::None);
}

static Uptr printedModuleId = 0;

static void printModule(const llvm::Module& llvmModule, const char* filename)
{
	std::error_code errorCode;
	std::string augmentedFilename
		= std::string(filename) + std::to_string(printedModuleId++) + ".ll";
	llvm::raw_fd_ostream dumpFileStream(
		augmentedFilename, errorCode, llvm::sys::fs::OpenFlags::F_Text);
	llvmModule.print(dumpFileStream, nullptr);
	Log::printf(Log::debug, "Dumped LLVM module to: %s\n", augmentedFilename.c_str());
}

// Define a LLVM raw output stream that can write directly to a std::vector.
struct LLVMArrayOutputStream : llvm::raw_pwrite_stream
{
	LLVMArrayOutputStream() { SetUnbuffered(); }

	~LLVMArrayOutputStream() override = default;

	void flush() = delete;

	std::vector<U8>&& getOutput() { return std::move(output); }

protected:
	virtual void write_impl(const char* data, size_t numBytes) override
	{
		const Uptr startOffset = output.size();
		output.resize(output.size() + numBytes);
		memcpy(output.data() + startOffset, data, numBytes);
	}
	virtual void pwrite_impl(const char* data, size_t numBytes, U64 offset) override
	{
		wavmAssert(offset + numBytes > offset && offset + numBytes <= U64(output.size()));
		memcpy(output.data() + offset, data, numBytes);
	}
	virtual U64 current_pos() const override { return output.size(); }

private:
	std::vector<U8> output;
};

static void optimizeLLVMModule(llvm::Module& llvmModule, bool shouldLogMetrics)
{
	// Run some optimization on the module's functions.
	Timing::Timer optimizationTimer;

	llvm::legacy::FunctionPassManager fpm(&llvmModule);
	fpm.add(llvm::createPromoteMemoryToRegisterPass());
	fpm.add(llvm::createInstructionCombiningPass());
	fpm.add(llvm::createCFGSimplificationPass());
	fpm.add(llvm::createJumpThreadingPass());
	fpm.add(llvm::createConstantPropagationPass());
	fpm.doInitialization();
	for(auto functionIt = llvmModule.begin(); functionIt != llvmModule.end(); ++functionIt)
	{ fpm.run(*functionIt); }

	if(shouldLogMetrics)
	{
		Timing::logRatePerSecond(
			"Optimized LLVM module", optimizationTimer, (F64)llvmModule.size(), "functions");
	}

	// Dump the optimized module if desired.
	if(shouldLogMetrics && DUMP_OPTIMIZED_MODULE) { printModule(llvmModule, "llvmOptimizedDump"); }
}

std::vector<U8> LLVMJIT::compileLLVMModule(llvm::Module&& llvmModule, bool shouldLogMetrics)
{
	auto targetTriple = llvm::sys::getProcessTriple();
#ifdef __APPLE__
	// Didn't figure out exactly why, but this works around a problem with the MacOS dynamic loader.
	// Without it, our symbols can't be found in the JITed object file.
	targetTriple += "-elf";
#endif
	std::unique_ptr<llvm::TargetMachine> targetMachine(llvm::EngineBuilder().selectTarget(
		llvm::Triple(targetTriple),
		"",
		llvm::sys::getHostCPUName(),
		llvm::SmallVector<std::string, 0>{LLVM_TARGET_ATTRIBUTES}));

	// Get a target machine object for this host, and set the module to use its data layout.
	llvmModule.setDataLayout(targetMachine->createDataLayout());

	// Dump the module if desired.
	if(shouldLogMetrics && DUMP_UNOPTIMIZED_MODULE) { printModule(llvmModule, "llvmDump"); }

	// Verify the module.
	if(shouldLogMetrics && VERIFY_MODULE)
	{
		std::string verifyOutputString;
		llvm::raw_string_ostream verifyOutputStream(verifyOutputString);
		if(llvm::verifyModule(llvmModule, &verifyOutputStream))
		{
			verifyOutputStream.flush();
			Errors::fatalf("LLVM verification errors:\n%s\n", verifyOutputString.c_str());
		}
		Log::printf(Log::debug, "Verified LLVM module\n");
	}

	// Optimize the module;
	optimizeLLVMModule(llvmModule, shouldLogMetrics);

	// Generate machine code for the module.
	Timing::Timer machineCodeTimer;
	std::vector<U8> objectBytes;
	{
		llvm::legacy::PassManager passManager;
		llvm::MCContext* mcContext;
		LLVMArrayOutputStream objectStream;
		errorUnless(!targetMachine->addPassesToEmitMC(passManager, mcContext, objectStream));
		passManager.run(llvmModule);
		objectBytes = objectStream.getOutput();
	}
	if(shouldLogMetrics)
	{
		Timing::logRatePerSecond(
			"Generated machine code", machineCodeTimer, (F64)llvmModule.size(), "functions");
	}

	if(shouldLogMetrics && DUMP_OBJECT)
	{
		// Dump the object file.
		std::error_code errorCode;
		static Uptr dumpedObjectId = 0;
		std::string augmentedFilename
			= std::string("jitObject") + std::to_string(dumpedObjectId++) + ".o";
		llvm::raw_fd_ostream dumpFileStream(
			augmentedFilename, errorCode, llvm::sys::fs::OpenFlags::F_None);
		dumpFileStream.write((const char*)objectBytes.data(), objectBytes.size());
		Log::printf(Log::Category::debug, "Dumped object file to: %s\n", augmentedFilename.c_str());
	}

	return objectBytes;
}

std::vector<U8> LLVMJIT::compileModule(const IR::Module& irModule)
{
	Lock<Platform::Mutex> llvmLock(llvmMutex);

	initLLVM();

	// Emit LLVM IR for the module.
	llvm::Module llvmModule("", *llvmContext);
	emitModule(irModule, llvmModule);

	// Compile the LLVM IR to object code.
	return compileLLVMModule(std::move(llvmModule), true);
}

std::shared_ptr<LLVMJIT::JITModuleBase> LLVMJIT::instantiateModule(
	const Runtime::Module* module,
	const Runtime::ModuleInstance* moduleInstance)
{
	// Bind undefined symbols in the compiled object to values.
	HashMap<std::string, Uptr> importedSymbolMap;

	// Bind the wavmIntrinsic function symbols; the compiled module assumes they have the intrinsic
	// calling convention, so no thunking is necessary.
	for(const auto& exportMapPair : moduleInstance->compartment->wavmIntrinsics->exportMap)
	{
		wavmAssert(exportMapPair.value->kind == Runtime::ObjectKind::function);
		FunctionInstance* function = asFunction(exportMapPair.value);
		wavmAssert(function->callingConvention == CallingConvention::intrinsic);
		importedSymbolMap.add(exportMapPair.key, reinterpret_cast<Uptr>(function->nativeFunction));
	}

	// Bind imported function symbols.
	for(Uptr functionIndex = 0;
		functionIndex < moduleInstance->functions.size() - moduleInstance->functionDefs.size();
		++functionIndex)
	{
		const FunctionInstance* functionInstance = moduleInstance->functions[functionIndex];

		void* nativeFunction = functionInstance->nativeFunction;
		if(functionInstance->callingConvention != CallingConvention::wasm)
		{
			// If trying to import an intrinsic function, import a thunk instead that calls the
			// intrinsic function with the right calling convention.
			nativeFunction = LLVMJIT::getIntrinsicThunk(nativeFunction,
														functionInstance->type,
														functionInstance->callingConvention,
														moduleInstance->defaultMemory,
														moduleInstance->defaultTable);
		}

		importedSymbolMap.add(getExternalFunctionName(functionIndex),
							  reinterpret_cast<Uptr>(nativeFunction));
	}

	// Bind the table symbols. The compiled module uses the symbol's value as an offset into
	// CompartmentRuntimeData to the table's entry in CompartmentRuntimeData::tableBases.
	for(Uptr tableIndex = 0; tableIndex < moduleInstance->tables.size(); ++tableIndex)
	{
		const TableInstance* tableInstance = moduleInstance->tables[tableIndex];
		importedSymbolMap.add(
			"tableOffset" + std::to_string(tableIndex),
			offsetof(CompartmentRuntimeData, tableBases) + sizeof(void*) * tableInstance->id);
	}

	// Bind the memory symbols. The compiled module uses the symbol's value as an offset into
	// CompartmentRuntimeData to the memory's entry in CompartmentRuntimeData::memoryBases.
	for(Uptr memoryIndex = 0; memoryIndex < moduleInstance->memories.size(); ++memoryIndex)
	{
		const MemoryInstance* memoryInstance = moduleInstance->memories[memoryIndex];
		importedSymbolMap.add(
			"memoryOffset" + std::to_string(memoryIndex),
			offsetof(CompartmentRuntimeData, memoryBases) + sizeof(void*) * memoryInstance->id);
	}

	// Bind the globals symbols.
	for(Uptr globalIndex = 0; globalIndex < moduleInstance->globals.size(); ++globalIndex)
	{
		const GlobalInstance* globalInstance = moduleInstance->globals[globalIndex];
		Uptr value;
		if(globalInstance->type.isMutable)
		{
			// If the global is mutable, bind the symbol to the offset into
			// ContextRuntimeData::globalData where it is stored.
			value = offsetof(ContextRuntimeData, globalData) + globalInstance->mutableDataOffset;
		}
		else
		{
			// Otherwise, bind the symbol to a pointer to the global's immutable value.
			value = reinterpret_cast<Uptr>(&globalInstance->initialValue);
		}
		importedSymbolMap.add("global" + std::to_string(globalIndex), value);
	}

	// Bind exception type symbols to point to the exception type instance.
	for(Uptr exceptionTypeIndex = 0; exceptionTypeIndex < moduleInstance->exceptionTypes.size();
		++exceptionTypeIndex)
	{
		const ExceptionTypeInstance* exceptionTypeInstance
			= moduleInstance->exceptionTypes[exceptionTypeIndex];
		importedSymbolMap.add("exceptionType" + std::to_string(exceptionTypeIndex),
							  reinterpret_cast<Uptr>(exceptionTypeInstance));
	}

	Lock<Platform::Mutex> llvmLock(llvmMutex);

	initLLVM();

	std::shared_ptr<JITModule> jitModule = std::make_shared<JITModule>(moduleInstance);
	jitModule->load(module->objectFileBytes, importedSymbolMap, true);
	return jitModule;
}

std::string LLVMJIT::getExternalFunctionName(Uptr functionIndex)
{
	return "function" + std::to_string(functionIndex);
}

bool LLVMJIT::getFunctionIndexFromExternalName(const char* externalName, Uptr& outFunctionDefIndex)
{
#if(defined(_WIN32) && !defined(_WIN64))
	const char wasmFuncPrefix[] = "_function";
#else
	const char wasmFuncPrefix[] = "function";
#endif
	const Uptr numPrefixChars = sizeof(wasmFuncPrefix) - 1;
	if(!strncmp(externalName, wasmFuncPrefix, numPrefixChars))
	{
		char* numberEnd        = nullptr;
		U64 functionDefIndex64 = std::strtoull(externalName + numPrefixChars, &numberEnd, 10);
		if(functionDefIndex64 > UINTPTR_MAX) { return false; }
		outFunctionDefIndex = Uptr(functionDefIndex64);
		return true;
	}
	else
	{
		return false;
	}
}

void LLVMJIT::initLLVM()
{
	if(llvmContext) { return; }
	llvmContext = new llvm::LLVMContext();

	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetDisassembler();
	llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

	llvmI8Type    = llvm::Type::getInt8Ty(*llvmContext);
	llvmI16Type   = llvm::Type::getInt16Ty(*llvmContext);
	llvmI32Type   = llvm::Type::getInt32Ty(*llvmContext);
	llvmI64Type   = llvm::Type::getInt64Ty(*llvmContext);
	llvmI128Type  = llvm::Type::getInt128Ty(*llvmContext);
	llvmF32Type   = llvm::Type::getFloatTy(*llvmContext);
	llvmF64Type   = llvm::Type::getDoubleTy(*llvmContext);
	llvmVoidType  = llvm::Type::getVoidTy(*llvmContext);
	llvmBoolType  = llvm::Type::getInt1Ty(*llvmContext);
	llvmI8PtrType = llvmI8Type->getPointerTo();
	switch(sizeof(Uptr))
	{
	case 4: llvmIptrType = llvmI32Type; break;
	case 8: llvmIptrType = llvmI64Type; break;
	default: Errors::unreachable();
	}

	auto llvmExceptionRecordStructType = llvm::StructType::create({
		llvmI32Type,   // DWORD ExceptionCode
		llvmI32Type,   // DWORD ExceptionFlags
		llvmI8PtrType, // _EXCEPTION_RECORD* ExceptionRecord
		llvmI8PtrType, // PVOID ExceptionAddress
		llvmI32Type,   // DWORD NumParameters
		llvm::ArrayType::get(llvmI64Type,
							 15) // ULONG_PTR ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS]
	});
	llvmExceptionPointersStructType
		= llvm::StructType::create({llvmExceptionRecordStructType->getPointerTo(), llvmI8PtrType});

	llvmI8x16Type  = llvm::VectorType::get(llvmI8Type, 16);
	llvmI16x8Type  = llvm::VectorType::get(llvmI16Type, 8);
	llvmI32x4Type  = llvm::VectorType::get(llvmI32Type, 4);
	llvmI64x2Type  = llvm::VectorType::get(llvmI64Type, 2);
	llvmI128x1Type = llvm::VectorType::get(llvmI128Type, 1);
	llvmF32x4Type  = llvm::VectorType::get(llvmF32Type, 4);
	llvmF64x2Type  = llvm::VectorType::get(llvmF64Type, 2);

	llvmValueTypes[(Uptr)ValueType::i32]  = llvmI32Type;
	llvmValueTypes[(Uptr)ValueType::i64]  = llvmI64Type;
	llvmValueTypes[(Uptr)ValueType::f32]  = llvmF32Type;
	llvmValueTypes[(Uptr)ValueType::f64]  = llvmF64Type;
	llvmValueTypes[(Uptr)ValueType::v128] = llvmI128x1Type;

	// Create zero constants of each type.
	typedZeroConstants[(Uptr)ValueType::any] = nullptr;
	typedZeroConstants[(Uptr)ValueType::i32] = emitLiteral((U32)0);
	typedZeroConstants[(Uptr)ValueType::i64] = emitLiteral((U64)0);
	typedZeroConstants[(Uptr)ValueType::f32] = emitLiteral((F32)0.0f);
	typedZeroConstants[(Uptr)ValueType::f64] = emitLiteral((F64)0.0);

	U64 i64x2Zero[2]                          = {0, 0};
	typedZeroConstants[(Uptr)ValueType::v128] = llvm::ConstantVector::get(
		{llvm::ConstantInt::get(llvmI128Type, llvm::APInt(128, 2, i64x2Zero))});
}

namespace LLVMJIT
{
	RUNTIME_API void deinit()
	{
		Lock<Platform::Mutex> llvmLock(llvmMutex);

		if(llvmContext)
		{
			delete llvmContext;
			llvmContext = nullptr;

			llvmI8Type = llvmI16Type = llvmI32Type = llvmI64Type = nullptr;
			llvmF32Type = llvmF64Type = nullptr;
			llvmVoidType = llvmBoolType = llvmI8PtrType = llvmIptrType = nullptr;
			llvmExceptionPointersStructType                            = nullptr;
			llvmI8x16Type = llvmI16x8Type = llvmI32x4Type = llvmI64x2Type = nullptr;

			memset(llvmValueTypes, 0, sizeof(llvmValueTypes));
			memset(typedZeroConstants, 0, sizeof(typedZeroConstants));
		}
	}
}

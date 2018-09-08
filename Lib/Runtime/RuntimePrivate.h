#pragma once

#include "IR/Module.h"
#include "Inline/BasicTypes.h"
#include "Inline/DenseStaticIntSet.h"
#include "Inline/HashMap.h"
#include "Inline/HashSet.h"
#include "Inline/IndexMap.h"
#include "LLVMJIT/LLVMJIT.h"
#include "Runtime/Intrinsics.h"
#include "Runtime/Runtime.h"
#include "Runtime/RuntimeData.h"

#include <atomic>
#include <functional>
#include <memory>

namespace Intrinsics
{
	struct Module;
}

namespace Runtime
{
	enum class CallingConvention;
}

namespace Runtime
{
	// A private root for all runtime objects that handles garbage collection.
	struct ObjectImpl : Object
	{
		std::atomic<Uptr> numRootReferences;

		ObjectImpl(ObjectKind inKind);

		// Called on all objects that are about to be deleted before any of them are deleted.
		virtual void finalize() {}
	};

	// An instance of a function: a function defined in an instantiated module, or an intrinsic
	// function.
	struct FunctionInstance : ObjectImpl
	{
		ModuleInstance* moduleInstance;
		IR::FunctionType type;
		void* nativeFunction;
		IR::CallingConvention callingConvention;
		std::string debugName;

		FunctionInstance(ModuleInstance* inModuleInstance,
						 IR::FunctionType inType,
						 void* inNativeFunction,
						 IR::CallingConvention inCallingConvention,
						 std::string&& inDebugName)
		: ObjectImpl(ObjectKind::function)
		, moduleInstance(inModuleInstance)
		, type(inType)
		, nativeFunction(inNativeFunction)
		, callingConvention(inCallingConvention)
		, debugName(std::move(inDebugName))
		{
		}
	};

	// An instance of a WebAssembly Table.
	struct TableInstance : ObjectImpl
	{
		struct FunctionElement
		{
			IR::FunctionType::Encoding typeEncoding;
			void* value;
		};

		Compartment* const compartment;
		Uptr id;

		const IR::TableType type;

		FunctionElement* baseAddress;
		Uptr endOffset;

		// The Objects corresponding to the FunctionElements at baseAddress.
		Platform::Mutex elementsMutex;
		std::vector<Object*> elements;

		TableInstance(Compartment* inCompartment, const IR::TableType& inType)
		: ObjectImpl(ObjectKind::table)
		, compartment(inCompartment)
		, id(UINTPTR_MAX)
		, type(inType)
		, baseAddress(nullptr)
		, endOffset(0)
		{
		}
		~TableInstance() override;
		virtual void finalize() override;
	};

	// An instance of a WebAssembly Memory.
	struct MemoryInstance : ObjectImpl
	{
		Compartment* const compartment;
		Uptr id;

		IR::MemoryType type;

		U8* baseAddress;
		std::atomic<Uptr> numPages;
		Uptr endOffset;

		MemoryInstance(Compartment* inCompartment, const IR::MemoryType& inType)
		: ObjectImpl(ObjectKind::memory)
		, compartment(inCompartment)
		, id(UINTPTR_MAX)
		, type(inType)
		, baseAddress(nullptr)
		, numPages(0)
		, endOffset(0)
		{
		}
		~MemoryInstance() override;
		virtual void finalize() override;
	};

	// An instance of a WebAssembly global.
	struct GlobalInstance : ObjectImpl
	{
		Compartment* const compartment;

		const IR::GlobalType type;
		const U32 mutableGlobalId;
		const IR::UntaggedValue initialValue;

		GlobalInstance(Compartment* inCompartment,
					   IR::GlobalType inType,
					   U32 inMutableGlobalId,
					   IR::UntaggedValue inInitialValue)
		: ObjectImpl(ObjectKind::global)
		, compartment(inCompartment)
		, type(inType)
		, mutableGlobalId(inMutableGlobalId)
		, initialValue(inInitialValue)
		{
		}
		virtual void finalize() override;
	};

	// An instance of a WebAssembly exception type.
	struct ExceptionTypeInstance : ObjectImpl
	{
		IR::ExceptionType type;
		std::string debugName;

		ExceptionTypeInstance(IR::ExceptionType inType, std::string&& inDebugName)
		: ObjectImpl(ObjectKind::exceptionTypeInstance)
		, type(inType)
		, debugName(std::move(inDebugName))
		{
		}
	};

	// A compiled WebAssembly module.
	struct Module : ObjectImpl
	{
		IR::IndexSpace<IR::FunctionType, IR::FunctionType> functions;

		IR::IndexSpace<IR::TableDef, IR::TableType> tables;
		IR::IndexSpace<IR::MemoryDef, IR::MemoryType> memories;
		IR::IndexSpace<IR::GlobalDef, IR::GlobalType> globals;
		IR::IndexSpace<IR::ExceptionTypeDef, IR::ExceptionType> exceptionTypes;

		std::vector<IR::Export> exports;
		std::vector<IR::DataSegment> dataSegments;
		std::vector<IR::TableSegment> tableSegments;

		Uptr startFunctionIndex;

		IR::DisassemblyNames disassemblyNames;

		std::vector<U8> objectFileBytes;

		Module(IR::IndexSpace<IR::FunctionType, IR::FunctionType>&& inFunctions,
			   IR::IndexSpace<IR::TableDef, IR::TableType>&& inTables,
			   IR::IndexSpace<IR::MemoryDef, IR::MemoryType>&& inMemories,
			   IR::IndexSpace<IR::GlobalDef, IR::GlobalType>&& inGlobals,
			   IR::IndexSpace<IR::ExceptionTypeDef, IR::ExceptionType>&& inExceptionTypes,
			   std::vector<IR::Export>&& inExports,
			   std::vector<IR::DataSegment>&& inDataSegments,
			   std::vector<IR::TableSegment>&& inTableSegments,
			   Uptr inStartFunctionIndex,
			   IR::DisassemblyNames&& inDisassemblyNames,
			   std::vector<U8>&& inObjectFileBytes)
		: ObjectImpl(ObjectKind::module)
		, functions(inFunctions)
		, tables(inTables)
		, memories(inMemories)
		, globals(inGlobals)
		, exceptionTypes(inExceptionTypes)
		, exports(inExports)
		, dataSegments(inDataSegments)
		, tableSegments(inTableSegments)
		, startFunctionIndex(inStartFunctionIndex)
		, disassemblyNames(inDisassemblyNames)
		, objectFileBytes(std::move(inObjectFileBytes))
		{
		}
	};

	// An instance of a WebAssembly module.
	struct ModuleInstance : ObjectImpl
	{
		Compartment* compartment;

		HashMap<std::string, Object*> exportMap;

		std::vector<FunctionInstance*> functionDefs;

		std::vector<FunctionInstance*> functions;
		std::vector<TableInstance*> tables;
		std::vector<MemoryInstance*> memories;
		std::vector<GlobalInstance*> globals;
		std::vector<ExceptionTypeInstance*> exceptionTypes;

		FunctionInstance* startFunction;
		MemoryInstance* defaultMemory;
		TableInstance* defaultTable;

		Platform::Mutex passiveDataSegmentsMutex;
		HashMap<Uptr, std::vector<U8>> passiveDataSegments;

		Platform::Mutex passiveTableSegmentsMutex;
		HashMap<Uptr, std::vector<Uptr>> passiveTableSegments;

		LLVMJIT::LoadedModule* jitModule;

		std::string debugName;

		ModuleInstance(Compartment* inCompartment,
					   std::vector<FunctionInstance*>&& inFunctionImports,
					   std::vector<TableInstance*>&& inTableImports,
					   std::vector<MemoryInstance*>&& inMemoryImports,
					   std::vector<GlobalInstance*>&& inGlobalImports,
					   std::vector<ExceptionTypeInstance*>&& inExceptionTypeImports,
					   std::string&& inDebugName)
		: ObjectImpl(ObjectKind::moduleInstance)
		, compartment(inCompartment)
		, functions(inFunctionImports)
		, tables(inTableImports)
		, memories(inMemoryImports)
		, globals(inGlobalImports)
		, exceptionTypes(inExceptionTypeImports)
		, startFunction(nullptr)
		, defaultMemory(nullptr)
		, defaultTable(nullptr)
		, jitModule(nullptr)
		, debugName(std::move(inDebugName))
		{
		}

		virtual ~ModuleInstance() override;
	};

	struct Context : ObjectImpl
	{
		Compartment* compartment;
		Uptr id;
		struct ContextRuntimeData* runtimeData;

		Context(Compartment* inCompartment)
		: ObjectImpl(ObjectKind::context)
		, compartment(inCompartment)
		, id(UINTPTR_MAX)
		, runtimeData(nullptr)
		{
		}

		virtual void finalize() override;
	};

	struct Compartment : ObjectImpl
	{
		mutable Platform::Mutex mutex;

		struct CompartmentRuntimeData* runtimeData;
		U8* unalignedRuntimeData;

		// These are weak references that aren't followed by the garbage collector.
		// If the referenced object is deleted, it will remove the reference here.
		HashSet<GlobalInstance*> globals;
		IndexMap<Uptr, MemoryInstance*> memories;
		IndexMap<Uptr, TableInstance*> tables;
		IndexMap<Uptr, Context*> contexts;

		DenseStaticIntSet<U32, maxMutableGlobals> globalDataAllocationMask;

		IR::UntaggedValue initialContextMutableGlobals[maxMutableGlobals];

		ModuleInstance* wavmIntrinsics;

		Compartment();
		~Compartment() override;
	};

	DECLARE_INTRINSIC_MODULE(wavmIntrinsics);

	void dummyReferenceAtomics();

	// Initializes global state used by the WAVM intrinsics.
	Runtime::ModuleInstance* instantiateWAVMIntrinsics(Compartment* compartment);

	// Checks whether an address is owned by a table or memory.
	bool isAddressOwnedByTable(U8* address);
	bool isAddressOwnedByMemory(U8* address);

	// Clones a memory or table with the same ID in a new compartment.
	TableInstance* cloneTable(TableInstance* memory, Compartment* newCompartment);
	MemoryInstance* cloneMemory(MemoryInstance* memory, Compartment* newCompartment);

	// Clone a global with same ID and mutable data offset (if mutable) in a new compartment.
	GlobalInstance* cloneGlobal(GlobalInstance* global, Compartment* newCompartment);
}

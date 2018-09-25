#pragma once

#include "WAVM/IR/Module.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/DenseStaticIntSet.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/HashSet.h"
#include "WAVM/Inline/IndexMap.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/Runtime/RuntimeData.h"

#include <atomic>
#include <functional>
#include <memory>

namespace WAVM { namespace Intrinsics {
	struct Module;
}}

namespace WAVM { namespace Runtime {

	// A private base class for all runtime objects that are garbage collected.
	struct GCObject : Object
	{
		Compartment* const compartment;
		std::atomic<Uptr> numRootReferences;

		GCObject(ObjectKind inKind, Compartment* inCompartment);
		virtual ~GCObject() { wavmAssert(numRootReferences.load(std::memory_order_acquire) == 0); }
	};

	// An instance of a WebAssembly Table.
	struct TableInstance : GCObject
	{
		struct Element
		{
			std::atomic<Uptr> biasedValue;
		};

		Uptr id = UINTPTR_MAX;
		const IR::TableType type;
		std::string debugName;

		Element* elements = nullptr;
		Uptr numReservedBytes = 0;
		Uptr numReservedElements = 0;

		mutable Platform::Mutex resizingMutex;
		std::atomic<Uptr> numElements{0};

		TableInstance(Compartment* inCompartment,
					  const IR::TableType& inType,
					  std::string&& inDebugName)
		: GCObject(ObjectKind::table, inCompartment)
		, type(inType)
		, debugName(std::move(inDebugName))
		{
		}
		~TableInstance() override;
	};

	// This is used as a sentinel value for table elements that are out-of-bounds. The address of
	// this Object is subtracted from every address stored in the table, so zero-initialized pages
	// at the end of the array will, when re-adding this Function's address, point to this Object.
	extern Object* getOutOfBoundsElement();

	// An instance of a WebAssembly Memory.
	struct MemoryInstance : GCObject
	{
		Uptr id = UINTPTR_MAX;
		IR::MemoryType type;
		std::string debugName;

		U8* baseAddress = nullptr;
		Uptr numReservedBytes = 0;

		mutable Platform::Mutex resizingMutex;
		std::atomic<Uptr> numPages{0};

		MemoryInstance(Compartment* inCompartment,
					   const IR::MemoryType& inType,
					   std::string&& inDebugName)
		: GCObject(ObjectKind::memory, inCompartment)
		, type(inType)
		, debugName(std::move(inDebugName))
		{
		}
		~MemoryInstance() override;
	};

	// An instance of a WebAssembly global.
	struct GlobalInstance : GCObject
	{
		const IR::GlobalType type;
		const U32 mutableGlobalId;
		const IR::UntaggedValue initialValue;

		GlobalInstance(Compartment* inCompartment,
					   IR::GlobalType inType,
					   U32 inMutableGlobalId,
					   IR::UntaggedValue inInitialValue)
		: GCObject(ObjectKind::global, inCompartment)
		, type(inType)
		, mutableGlobalId(inMutableGlobalId)
		, initialValue(inInitialValue)
		{
		}
		~GlobalInstance() override;
	};

	// An instance of a WebAssembly exception type.
	struct ExceptionTypeInstance : GCObject
	{
		Uptr id = UINTPTR_MAX;

		IR::ExceptionType type;
		std::string debugName;

		ExceptionTypeInstance(Compartment* inCompartment,
							  IR::ExceptionType inType,
							  std::string&& inDebugName)
		: GCObject(ObjectKind::exceptionType, inCompartment)
		, type(inType)
		, debugName(std::move(inDebugName))
		{
		}

		~ExceptionTypeInstance() override;
	};

	// A compiled WebAssembly module.
	struct Module
	{
		IR::Module ir;
		std::vector<U8> objectCode;

		Module(IR::Module&& inIR, std::vector<U8>&& inObjectCode)
		: ir(inIR), objectCode(std::move(inObjectCode))
		{
		}
	};

	// An instance of a WebAssembly module.
	struct ModuleInstance : GCObject
	{
		Uptr id = UINTPTR_MAX;

		HashMap<std::string, Object*> exportMap;

		std::vector<FunctionInstance*> functions;
		std::vector<TableInstance*> tables;
		std::vector<MemoryInstance*> memories;
		std::vector<GlobalInstance*> globals;
		std::vector<ExceptionTypeInstance*> exceptionTypes;

		FunctionInstance* startFunction = nullptr;
		MemoryInstance* defaultMemory = nullptr;
		TableInstance* defaultTable = nullptr;

		mutable Platform::Mutex passiveDataSegmentsMutex;
		HashMap<Uptr, std::shared_ptr<const std::vector<U8>>> passiveDataSegments;

		mutable Platform::Mutex passiveElemSegmentsMutex;
		HashMap<Uptr, std::shared_ptr<const std::vector<Object*>>> passiveElemSegments;

		LLVMJIT::LoadedModule* jitModule = nullptr;

		std::string debugName;

		ModuleInstance(Compartment* inCompartment,
					   std::vector<FunctionInstance*>&& inFunctionImports,
					   std::vector<TableInstance*>&& inTableImports,
					   std::vector<MemoryInstance*>&& inMemoryImports,
					   std::vector<GlobalInstance*>&& inGlobalImports,
					   std::vector<ExceptionTypeInstance*>&& inExceptionTypeImports,
					   std::string&& inDebugName)
		: GCObject(ObjectKind::moduleInstance, inCompartment)
		, functions(inFunctionImports)
		, tables(inTableImports)
		, memories(inMemoryImports)
		, globals(inGlobalImports)
		, exceptionTypes(inExceptionTypeImports)
		, debugName(std::move(inDebugName))
		{
		}

		virtual ~ModuleInstance() override;
	};

	struct Context : GCObject
	{
		Uptr id = UINTPTR_MAX;
		struct ContextRuntimeData* runtimeData = nullptr;

		Context(Compartment* inCompartment) : GCObject(ObjectKind::context, inCompartment) {}
		~Context();
	};

	struct Compartment : GCObject
	{
		mutable Platform::Mutex mutex;

		struct CompartmentRuntimeData* runtimeData;
		U8* unalignedRuntimeData;

		IndexMap<Uptr, ModuleInstance*> moduleInstances;
		IndexMap<Uptr, MemoryInstance*> memories;
		IndexMap<Uptr, TableInstance*> tables;
		IndexMap<Uptr, Context*> contexts;
		IndexMap<Uptr, ExceptionTypeInstance*> exceptionTypes;

		HashSet<GlobalInstance*> globals;

		DenseStaticIntSet<U32, maxMutableGlobals> globalDataAllocationMask;
		IR::UntaggedValue initialContextMutableGlobals[maxMutableGlobals];

		Compartment();
		~Compartment();
	};

	DECLARE_INTRINSIC_MODULE(wavmIntrinsics);

	void dummyReferenceAtomics();
	void dummyReferenceWAVMIntrinsics();

	// Initializes global state used by the WAVM intrinsics.
	Runtime::ModuleInstance* instantiateWAVMIntrinsics(Compartment* compartment);

	// Checks whether an address is owned by a table or memory.
	bool isAddressOwnedByTable(U8* address, TableInstance*& outTable, Uptr& outTableIndex);
	bool isAddressOwnedByMemory(U8* address, MemoryInstance*& outMemory, Uptr& outMemoryAddress);

	// Clones a memory or table with the same ID in a new compartment.
	TableInstance* cloneTable(TableInstance* memory, Compartment* newCompartment);
	MemoryInstance* cloneMemory(MemoryInstance* memory, Compartment* newCompartment);

	// Clone a global with same ID and mutable data offset (if mutable) in a new compartment.
	GlobalInstance* cloneGlobal(GlobalInstance* global, Compartment* newCompartment);

	ModuleInstance* getModuleInstanceFromRuntimeData(ContextRuntimeData* contextRuntimeData,
													 Uptr moduleInstanceId);
	TableInstance* getTableFromRuntimeData(ContextRuntimeData* contextRuntimeData, Uptr tableId);
	MemoryInstance* getMemoryFromRuntimeData(ContextRuntimeData* contextRuntimeData, Uptr memoryId);
}}

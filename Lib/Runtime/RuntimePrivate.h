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
		std::atomic<Uptr> numRootReferences{0};
		void* userData{nullptr};

		GCObject(ObjectKind inKind, Compartment* inCompartment);
		virtual ~GCObject() { wavmAssert(numRootReferences.load(std::memory_order_acquire) == 0); }
	};

	// An instance of a WebAssembly Table.
	struct Table : GCObject
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

		Table(Compartment* inCompartment, const IR::TableType& inType, std::string&& inDebugName)
		: GCObject(ObjectKind::table, inCompartment)
		, type(inType)
		, debugName(std::move(inDebugName))
		{
		}
		~Table() override;
	};

	// This is used as a sentinel value for table elements that are out-of-bounds. The address of
	// this Object is subtracted from every address stored in the table, so zero-initialized pages
	// at the end of the array will, when re-adding this Function's address, point to this Object.
	extern Object* getOutOfBoundsElement();

	// An instance of a WebAssembly Memory.
	struct Memory : GCObject
	{
		Uptr id = UINTPTR_MAX;
		IR::MemoryType type;
		std::string debugName;

		U8* baseAddress = nullptr;
		Uptr numReservedBytes = 0;

		mutable Platform::Mutex resizingMutex;
		std::atomic<Uptr> numPages{0};

		Memory(Compartment* inCompartment, const IR::MemoryType& inType, std::string&& inDebugName)
		: GCObject(ObjectKind::memory, inCompartment)
		, type(inType)
		, debugName(std::move(inDebugName))
		{
		}
		~Memory() override;
	};

	// An instance of a WebAssembly global.
	struct Global : GCObject
	{
		Uptr id = UINTPTR_MAX;

		const IR::GlobalType type;
		const U32 mutableGlobalIndex;
		IR::UntaggedValue initialValue;
		bool hasBeenInitialized;

		Global(Compartment* inCompartment,
			   IR::GlobalType inType,
			   U32 inMutableGlobalId,
			   IR::UntaggedValue inInitialValue = IR::UntaggedValue())
		: GCObject(ObjectKind::global, inCompartment)
		, type(inType)
		, mutableGlobalIndex(inMutableGlobalId)
		, initialValue(inInitialValue)
		, hasBeenInitialized(false)
		{
		}
		~Global() override;
	};

	// An instance of a WebAssembly exception type.
	struct ExceptionType : GCObject
	{
		Uptr id = UINTPTR_MAX;

		IR::ExceptionType sig;
		std::string debugName;

		ExceptionType(Compartment* inCompartment,
					  IR::ExceptionType inSig,
					  std::string&& inDebugName)
		: GCObject(ObjectKind::exceptionType, inCompartment)
		, sig(inSig)
		, debugName(std::move(inDebugName))
		{
		}

		~ExceptionType() override;
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

	typedef HashMap<Uptr, std::shared_ptr<std::vector<U8>>> PassiveDataSegmentMap;
	typedef HashMap<Uptr, std::shared_ptr<std::vector<Object*>>> PassiveElemSegmentMap;

	// An instance of a WebAssembly module.
	struct ModuleInstance : GCObject
	{
		const Uptr id;
		const std::string debugName;

		const HashMap<std::string, Object*> exportMap;

		const std::vector<Function*> functions;
		const std::vector<Table*> tables;
		const std::vector<Memory*> memories;
		const std::vector<Global*> globals;
		const std::vector<ExceptionType*> exceptionTypes;

		Function* const startFunction;

		mutable Platform::Mutex passiveDataSegmentsMutex;
		PassiveDataSegmentMap passiveDataSegments;

		mutable Platform::Mutex passiveElemSegmentsMutex;
		PassiveElemSegmentMap passiveElemSegments;

		const std::shared_ptr<LLVMJIT::Module> jitModule;

		ModuleInstance(Compartment* inCompartment,
					   Uptr inID,
					   HashMap<std::string, Object*>&& inExportMap,
					   std::vector<Function*>&& inFunctions,
					   std::vector<Table*>&& inTables,
					   std::vector<Memory*>&& inMemories,
					   std::vector<Global*>&& inGlobals,
					   std::vector<ExceptionType*>&& inExceptionTypes,
					   Function* inStartFunction,
					   PassiveDataSegmentMap&& inPassiveDataSegments,
					   PassiveElemSegmentMap&& inPassiveElemSegments,
					   std::shared_ptr<LLVMJIT::Module>&& inJITModule,
					   std::string&& inDebugName)
		: GCObject(ObjectKind::moduleInstance, inCompartment)
		, id(inID)
		, debugName(std::move(inDebugName))
		, exportMap(std::move(inExportMap))
		, functions(std::move(inFunctions))
		, tables(std::move(inTables))
		, memories(std::move(inMemories))
		, globals(std::move(inGlobals))
		, exceptionTypes(std::move(inExceptionTypes))
		, startFunction(inStartFunction)
		, passiveDataSegments(std::move(inPassiveDataSegments))
		, passiveElemSegments(std::move(inPassiveElemSegments))
		, jitModule(std::move(inJITModule))
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

		IndexMap<Uptr, Table*> tables;
		IndexMap<Uptr, Memory*> memories;
		IndexMap<Uptr, Global*> globals;
		IndexMap<Uptr, ExceptionType*> exceptionTypes;
		IndexMap<Uptr, ModuleInstance*> moduleInstances;
		IndexMap<Uptr, Context*> contexts;

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
	bool isAddressOwnedByTable(U8* address, Table*& outTable, Uptr& outTableIndex);
	bool isAddressOwnedByMemory(U8* address, Memory*& outMemory, Uptr& outMemoryAddress);

	// Clones objects into a new compartment with the same ID.
	Table* cloneTable(Table* memory, Compartment* newCompartment);
	Memory* cloneMemory(Memory* memory, Compartment* newCompartment);
	ExceptionType* cloneExceptionType(ExceptionType* exceptionType, Compartment* newCompartment);
	ModuleInstance* cloneModuleInstance(ModuleInstance* moduleInstance,
										Compartment* newCompartment);

	// Clone a global with same ID and mutable data offset (if mutable) in a new compartment.
	Global* cloneGlobal(Global* global, Compartment* newCompartment);

	ModuleInstance* getModuleInstanceFromRuntimeData(ContextRuntimeData* contextRuntimeData,
													 Uptr moduleInstanceId);
	Table* getTableFromRuntimeData(ContextRuntimeData* contextRuntimeData, Uptr tableId);
	Memory* getMemoryFromRuntimeData(ContextRuntimeData* contextRuntimeData, Uptr memoryId);
}}

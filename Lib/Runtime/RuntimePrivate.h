#pragma once

#include "IR/Module.h"
#include "Inline/BasicTypes.h"
#include "Inline/DenseStaticIntSet.h"
#include "Inline/HashMap.h"
#include "Inline/HashSet.h"
#include "Inline/IndexMap.h"
#include "LLVMJIT/LLVMJIT.h"
#include "Platform/Mutex.h"
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

		virtual const AnyReferee* getAnyRef() const = 0;
	};

	struct ObjectImplWithAnyRef : ObjectImpl
	{
		ObjectImplWithAnyRef(ObjectKind inKind) : ObjectImpl(inKind), anyRef{this} {}

		virtual const AnyReferee* getAnyRef() const override { return &anyRef; }

	private:
		AnyReferee anyRef;
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

		virtual const AnyReferee* getAnyRef() const override
		{
			return &asAnyFunc(this, nullptr, nullptr)->anyRef;
		}
	};

	// An instance of a WebAssembly Table.
	struct TableInstance : ObjectImplWithAnyRef
	{
		struct Element
		{
			std::atomic<Uptr> biasedValue;
		};

		Compartment* const compartment;
		Uptr id;

		const IR::TableType type;

		Element* elements;
		Uptr numReservedBytes;
		Uptr numReservedElements;

		Platform::Mutex resizingMutex;
		std::atomic<Uptr> numElements;

		TableInstance(Compartment* inCompartment, const IR::TableType& inType)
		: ObjectImplWithAnyRef(ObjectKind::table)
		, compartment(inCompartment)
		, id(UINTPTR_MAX)
		, type(inType)
		, elements(nullptr)
		, numReservedBytes(0)
		, numReservedElements(0)
		, numElements(0)
		{
		}
		~TableInstance() override;
		virtual void finalize() override;
	};

	// This is used as a sentinel value for table elements that are out-of-bounds. The address of
	// this AnyFunc is subtracted from every address stored in the table, so zero-initialized pages
	// at the end of the array will, when re-adding this AnyFunc's address, point to this AnyFunc.
	extern const AnyFunc* getOutOfBoundsAnyFunc();

	// A sentinel value that is used for null values of type anyfunc.
	extern const AnyFunc* getUninitializedAnyFunc();

	// An instance of a WebAssembly Memory.
	struct MemoryInstance : ObjectImplWithAnyRef
	{
		Compartment* const compartment;
		Uptr id;

		IR::MemoryType type;

		U8* baseAddress;
		Uptr numReservedBytes;

		Platform::Mutex resizingMutex;
		std::atomic<Uptr> numPages;

		MemoryInstance(Compartment* inCompartment, const IR::MemoryType& inType)
		: ObjectImplWithAnyRef(ObjectKind::memory)
		, compartment(inCompartment)
		, id(UINTPTR_MAX)
		, type(inType)
		, baseAddress(nullptr)
		, numReservedBytes(0)
		, numPages(0)
		{
		}
		~MemoryInstance() override;
		virtual void finalize() override;
	};

	// An instance of a WebAssembly global.
	struct GlobalInstance : ObjectImplWithAnyRef
	{
		Compartment* const compartment;

		const IR::GlobalType type;
		const U32 mutableGlobalId;
		const IR::UntaggedValue initialValue;

		GlobalInstance(Compartment* inCompartment,
					   IR::GlobalType inType,
					   U32 inMutableGlobalId,
					   IR::UntaggedValue inInitialValue)
		: ObjectImplWithAnyRef(ObjectKind::global)
		, compartment(inCompartment)
		, type(inType)
		, mutableGlobalId(inMutableGlobalId)
		, initialValue(inInitialValue)
		{
		}
		virtual void finalize() override;
	};

	// An instance of a WebAssembly exception type.
	struct ExceptionTypeInstance : ObjectImplWithAnyRef
	{
		IR::ExceptionType type;
		std::string debugName;

		ExceptionTypeInstance(IR::ExceptionType inType, std::string&& inDebugName)
		: ObjectImplWithAnyRef(ObjectKind::exceptionTypeInstance)
		, type(inType)
		, debugName(std::move(inDebugName))
		{
		}
	};

	// A compiled WebAssembly module.
	struct Module : ObjectImplWithAnyRef
	{
		IR::Module ir;
		std::vector<U8> objectCode;

		Module(IR::Module&& inIR, std::vector<U8>&& inObjectCode)
		: ObjectImplWithAnyRef(ObjectKind::module), ir(inIR), objectCode(std::move(inObjectCode))
		{
		}
	};

	// An instance of a WebAssembly module.
	struct ModuleInstance : ObjectImplWithAnyRef
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
		HashMap<Uptr, std::shared_ptr<const std::vector<U8>>> passiveDataSegments;

		Platform::Mutex passiveTableSegmentsMutex;
		HashMap<Uptr, std::shared_ptr<const std::vector<Object*>>> passiveTableSegments;

		LLVMJIT::LoadedModule* jitModule;

		std::string debugName;

		ModuleInstance(Compartment* inCompartment,
					   std::vector<FunctionInstance*>&& inFunctionImports,
					   std::vector<TableInstance*>&& inTableImports,
					   std::vector<MemoryInstance*>&& inMemoryImports,
					   std::vector<GlobalInstance*>&& inGlobalImports,
					   std::vector<ExceptionTypeInstance*>&& inExceptionTypeImports,
					   std::string&& inDebugName)
		: ObjectImplWithAnyRef(ObjectKind::moduleInstance)
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
		virtual void finalize() override;
	};

	struct Context : ObjectImplWithAnyRef
	{
		Compartment* compartment;
		Uptr id;
		struct ContextRuntimeData* runtimeData;

		Context(Compartment* inCompartment)
		: ObjectImplWithAnyRef(ObjectKind::context)
		, compartment(inCompartment)
		, id(UINTPTR_MAX)
		, runtimeData(nullptr)
		{
		}

		virtual void finalize() override;
	};

	struct Compartment : ObjectImplWithAnyRef
	{
		mutable Platform::Mutex mutex;

		struct CompartmentRuntimeData* runtimeData;
		U8* unalignedRuntimeData;

		// These are weak references that aren't followed by the garbage collector.
		// If the referenced object is deleted, it will remove the reference here.
		HashSet<ModuleInstance*> modules;
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

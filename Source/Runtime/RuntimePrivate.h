#pragma once

#include "Inline/BasicTypes.h"
#include "Platform/Platform.h"
#include "Runtime.h"

#include <functional>
#include <map>
#include <atomic>

namespace Intrinsics { struct Module; }

namespace LLVMJIT
{
	using namespace Runtime;
	
	struct JITModuleBase
	{
		virtual ~JITModuleBase() {}
	};

	void instantiateModule(const IR::Module& module,Runtime::ModuleInstance* moduleInstance);
	bool describeInstructionPointer(Uptr ip,std::string& outDescription);
	
	typedef void (*InvokeFunctionPointer)(void*,void*,V128*);

	// Generates an invoke thunk for a specific function type.
	InvokeFunctionPointer getInvokeThunk(const IR::FunctionType* functionType);

	// Generates a thunk to call a native function from generated code.
	void* getIntrinsicThunk(void* nativeFunction,const IR::FunctionType* functionType);
}

namespace Runtime
{
	using namespace IR;

	// A private root for all runtime objects that handles garbage collection.
	struct ObjectImpl : Object
	{
		std::atomic<Uptr> numRootReferences;

		ObjectImpl(ObjectKind inKind);

		// Called on all objects that are about to be deleted before any of them are deleted.
		virtual void finalize() {}
	};

	// An instance of a function: a function defined in an instantiated module, or an intrinsic function.
	struct FunctionInstance : ObjectImpl
	{
		ModuleInstance* moduleInstance;
		const FunctionType* type;
		void* nativeFunction;
		std::string debugName;

		FunctionInstance(
			ModuleInstance* inModuleInstance,
			const FunctionType* inType,
			void* inNativeFunction = nullptr,
			const char* inDebugName = "<unidentified FunctionInstance>")
		: ObjectImpl(ObjectKind::function)
		, moduleInstance(inModuleInstance)
		, type(inType)
		, nativeFunction(inNativeFunction)
		, debugName(inDebugName)
		{}
	};

	// An instance of a WebAssembly Table.
	struct TableInstance : ObjectImpl
	{
		struct FunctionElement
		{
			const FunctionType* type;
			void* value;
		};

		Compartment* compartment;
		Uptr id;

		TableType type;

		FunctionElement* baseAddress;
		Uptr endOffset;

		// The Objects corresponding to the FunctionElements at baseAddress.
		std::vector<Object*> elements;

		TableInstance(Compartment* inCompartment,const TableType& inType)
		: ObjectImpl(ObjectKind::table)
		, compartment(inCompartment)
		, type(inType)
		, baseAddress(nullptr)
		, endOffset(0) {}
		~TableInstance() override;
		virtual void finalize() override;
	};

	// An instance of a WebAssembly Memory.
	struct MemoryInstance : ObjectImpl
	{
		Compartment* compartment;
		Uptr id;

		MemoryType type;

		U8* baseAddress;
		std::atomic<Uptr> numPages;
		Uptr endOffset;

		MemoryInstance(Compartment* inCompartment,const MemoryType& inType)
		: ObjectImpl(ObjectKind::memory)
		, compartment(inCompartment)
		, type(inType)
		, baseAddress(nullptr)
		, numPages(0)
		, endOffset(0) {}
		~MemoryInstance() override;
		virtual void finalize() override;
	};

	// An instance of a WebAssembly global.
	struct GlobalInstance : ObjectImpl
	{
		Compartment* const compartment;
		const GlobalType type;
		const U32 mutableDataOffset;
		const UntaggedValue immutableValue;

		GlobalInstance(Compartment* inCompartment,GlobalType inType,U32 inMutableDataOffset,UntaggedValue inImmutableValue)
		: ObjectImpl(ObjectKind::global)
		, compartment(inCompartment)
		, type(inType)
		, mutableDataOffset(inMutableDataOffset)
		, immutableValue(inImmutableValue) {}
	};

	struct ExceptionData
	{
		ExceptionTypeInstance* typeInstance;
		U8 isUserException;
		UntaggedValue arguments[1];

		static Uptr calcNumBytes(Uptr numArguments)
		{
			return sizeof(ExceptionData) + (numArguments - 1) * sizeof(UntaggedValue);
		}
	};

	// An instance of a WebAssembly exception type.
	struct ExceptionTypeInstance : ObjectImpl
	{
		const TupleType* parameters;

		ExceptionTypeInstance(const TupleType* inParameters)
		: ObjectImpl(ObjectKind::exceptionType), parameters(inParameters)
		{}
	};

	// An instance of a WebAssembly module.
	struct ModuleInstance : ObjectImpl
	{
		Compartment* compartment;

		std::map<std::string,Object*> exportMap;

		std::vector<FunctionInstance*> functionDefs;

		std::vector<FunctionInstance*> functions;
		std::vector<TableInstance*> tables;
		std::vector<MemoryInstance*> memories;
		std::vector<GlobalInstance*> globals;
		std::vector<ExceptionTypeInstance*> exceptionTypes;

		FunctionInstance* startFunction;
		MemoryInstance* defaultMemory;
		TableInstance* defaultTable;

		LLVMJIT::JITModuleBase* jitModule;

		ModuleInstance(
			Compartment* inCompartment,
			std::vector<FunctionInstance*>&& inFunctionImports,
			std::vector<TableInstance*>&& inTableImports,
			std::vector<MemoryInstance*>&& inMemoryImports,
			std::vector<GlobalInstance*>&& inGlobalImports,
			std::vector<ExceptionTypeInstance*>&& inExceptionTypeImports
			)
		: ObjectImpl(ObjectKind::module)
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
		{}

		~ModuleInstance() override;
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
		{}

		~Context() override;
	};
	
	struct Compartment : ObjectImpl
	{
		Platform::Mutex* mutex;

		struct CompartmentRuntimeData* runtimeData;
		U8* unalignedRuntimeData;
		U32 numGlobalBytes;

		// These are weak references that aren't followed by the garbage collector.
		// If the referenced object is deleted, it will null the reference here.
		std::vector<MemoryInstance*> memories;
		std::vector<TableInstance*> tables;
		std::vector<Context*> contexts;

		ModuleInstance* wavmIntrinsics;

		Compartment();
		~Compartment() override;
	};

	#define compartmentReservedBytes (4ull * 1024 * 1024 * 1024)
	enum { maxGlobalBytes = 4096 };
	enum { maxMemories = 255 };
	enum { maxTables = 256 };
	enum { compartmentRuntimeDataAlignmentLog2 = 32 };
	enum { contextRuntimeDataAlignment = 4096 };

	struct ContextRuntimeData
	{
		U8 globalData[maxGlobalBytes];
	};

	struct CompartmentRuntimeData
	{
		Compartment* compartment;
		U8* memories[maxMemories];
		TableInstance::FunctionElement* tables[maxTables];
		ContextRuntimeData contexts[1]; // Actually [maxContexts], but at least MSVC doesn't allow declaring arrays that large.
	};

	enum { maxContexts = 1024 * 1024 - offsetof(CompartmentRuntimeData,contexts) / sizeof(ContextRuntimeData) };

	static_assert(sizeof(ContextRuntimeData) == 4096,"");
	static_assert(offsetof(CompartmentRuntimeData,contexts) % 4096 == 0,"CompartmentRuntimeData::contexts isn't page-aligned");
	static_assert(offsetof(CompartmentRuntimeData,contexts[maxContexts]) == 4ull * 1024 * 1024 * 1024,"CompartmentRuntimeData isn't the expected size");

	inline CompartmentRuntimeData* getCompartmentRuntimeData(ContextRuntimeData* contextRuntimeData)
	{
		return reinterpret_cast<CompartmentRuntimeData*>(reinterpret_cast<Uptr>(contextRuntimeData) & 0xffffffff00000000);
	}

	extern Intrinsics::Module* wavmIntrinsics;

	// Initializes global state used by the WAVM intrinsics.
	Runtime::ModuleInstance* instantiateWAVMIntrinsics(Compartment* compartment);

	// Checks whether an address is owned by a table or memory.
	bool isAddressOwnedByTable(U8* address);
	bool isAddressOwnedByMemory(U8* address);
}

#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"
#include "Runtime.h"

#include <functional>
#include <map>

#define HAS_64BIT_ADDRESS_SPACE (sizeof(uintp) == 8 && !PRETEND_32BIT_ADDRESS_SPACE)

namespace LLVMJIT
{
	using namespace Runtime;
	
	struct JITModuleBase
	{
		virtual ~JITModuleBase() {}
	};

	void init();
	void instantiateModule(const IR::Module& module,Runtime::ModuleInstance* moduleInstance);
	bool describeInstructionPointer(uintp ip,std::string& outDescription);
	
	typedef void (*InvokeFunctionPointer)(void*,uint64*);

	// Generates an invoke thunk for a specific function type.
	InvokeFunctionPointer getInvokeThunk(const IR::FunctionType* functionType);
}

namespace Runtime
{
	using namespace IR;
	
	// A private root for all runtime objects that handles garbage collection.
	struct GCObject : ObjectInstance
	{
		GCObject(ObjectKind inKind);
		~GCObject() override;
	};

	// An instance of a function: a function defined in an instantiated module, or an intrinsic function.
	struct FunctionInstance : GCObject
	{
		ModuleInstance* moduleInstance;
		const FunctionType* type;
		void* nativeFunction;
		std::string debugName;

		FunctionInstance(ModuleInstance* inModuleInstance,const FunctionType* inType,void* inNativeFunction = nullptr,const char* inDebugName = "<unidentified FunctionInstance>")
		: GCObject(ObjectKind::function), moduleInstance(inModuleInstance), type(inType), nativeFunction(inNativeFunction), debugName(inDebugName) {}
	};

	// An instance of a WebAssembly Table.
	struct TableInstance : GCObject
	{
		struct FunctionElement
		{
			const FunctionType* type;
			void* value;
		};

		TableType type;

		FunctionElement* baseAddress;
		size_t endOffset;

		uint8* reservedBaseAddress;
		size_t reservedNumPlatformPages;

		// The Objects corresponding to the FunctionElements at baseAddress.
		std::vector<ObjectInstance*> elements;

		TableInstance(const TableType& inType): GCObject(ObjectKind::table), type(inType), baseAddress(nullptr), endOffset(0), reservedBaseAddress(nullptr), reservedNumPlatformPages(0) {}
		~TableInstance() override;
	};

	// An instance of a WebAssembly Memory.
	struct MemoryInstance : GCObject
	{
		MemoryType type;

		uint8* baseAddress;
		size_t numPages;
		size_t endOffset;

		uint8* reservedBaseAddress;
		size_t reservedNumPlatformPages;

		MemoryInstance(const MemoryType& inType): GCObject(ObjectKind::memory), type(inType), baseAddress(nullptr), numPages(0), endOffset(0), reservedBaseAddress(nullptr), reservedNumPlatformPages(0) {}
		~MemoryInstance() override;
	};

	// An instance of a WebAssembly global.
	struct GlobalInstance : GCObject
	{
		GlobalType type;
		UntaggedValue value;

		GlobalInstance(GlobalType inType,UntaggedValue inValue): GCObject(ObjectKind::global), type(inType), value(inValue) {}
	};

	// An instance of a WebAssembly module.
	struct ModuleInstance : GCObject
	{
		std::map<std::string,ObjectInstance*> exportMap;

		std::vector<FunctionInstance*> functionDefs;

		std::vector<FunctionInstance*> functions;
		std::vector<TableInstance*> tables;
		std::vector<MemoryInstance*> memories;
		std::vector<GlobalInstance*> globals;

		MemoryInstance* defaultMemory;
		TableInstance* defaultTable;

		LLVMJIT::JITModuleBase* jitModule;

		ModuleInstance(
			std::vector<FunctionInstance*>&& inFunctionImports,
			std::vector<TableInstance*>&& inTableImports,
			std::vector<MemoryInstance*>&& inMemoryImports,
			std::vector<GlobalInstance*>&& inGlobalImports
			)
		: GCObject(ObjectKind::module)
		, functions(inFunctionImports)
		, tables(inTableImports)
		, memories(inMemoryImports)
		, globals(inGlobalImports)
		, defaultMemory(nullptr)
		, defaultTable(nullptr)
		, jitModule(nullptr)
		{}

		~ModuleInstance() override;
	};

	// Initializes global state used by the WAVM intrinsics.
	void initWAVMIntrinsics();

	// Checks whether an address is owned by a table or memory.
	bool isAddressOwnedByTable(uint8* address);
	bool isAddressOwnedByMemory(uint8* address);
	
	// Allocates virtual pages with alignBytes of padding, and returns an aligned base address.
	// The unaligned allocation address and size are written to outUnalignedBaseAddress and outUnalignedNumPlatformPages.
	uint8* allocateVirtualPagesAligned(size_t numBytes,size_t alignmentBytes,uint8*& outUnalignedBaseAddress,size_t& outUnalignedNumPlatformPages);
}

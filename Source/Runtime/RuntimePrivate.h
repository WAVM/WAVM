#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"
#include "Runtime.h"
#include "WebAssembly/WebAssembly.h"
#include "WebAssembly/Module.h"

#include <functional>

namespace LLVMJIT
{
	using namespace Runtime;
	
	struct JITModuleBase
	{
		virtual ~JITModuleBase() {}
	};

	void init();
	void instantiateModule(const WebAssembly::Module& module,Runtime::ModuleInstance* moduleInstance);
	bool describeInstructionPointer(uintp ip,std::string& outDescription);
	
	typedef void (*InvokeFunctionPointer)(void*,uint64*);

	// Generates an invoke thunk for a specific function type.
	InvokeFunctionPointer getInvokeThunk(const WebAssembly::FunctionType* functionType);
}

namespace Runtime
{
	using namespace WebAssembly;
	
	struct GCObject : Object
	{
		GCObject(ObjectKind inKind);
		~GCObject() override;
	};

	struct FunctionInstance : GCObject
	{
		ModuleInstance* moduleInstance;
		const FunctionType* type;
		void* nativeFunction;
		std::string debugName;

		FunctionInstance(ModuleInstance* inModuleInstance,const FunctionType* inType,void* inNativeFunction = nullptr,const char* inDebugName = "<unidentified FunctionInstance>")
		: GCObject(ObjectKind::function), moduleInstance(inModuleInstance), type(inType), nativeFunction(inNativeFunction), debugName(inDebugName) {}
	};

	struct Table : GCObject
	{
		struct FunctionElement
		{
			const FunctionType* type;
			void* value;
		};

		TableType type;
		FunctionElement* baseAddress;
		size_t maxPlatformPages;
		
		uint8* reservedBaseAddress;
		size_t reservedNumPlatformPages;

		std::vector<Object*> elements;

		Table(const TableType& inType): GCObject(ObjectKind::table), type(inType), baseAddress(nullptr), maxPlatformPages(0), reservedBaseAddress(nullptr), reservedNumPlatformPages(0) {}
		~Table() override;
	};

	struct Memory : GCObject
	{
		MemoryType type;
		uint8* baseAddress;
		size_t numPages;
		size_t maxPages;
		
		uint8* reservedBaseAddress;
		size_t reservedNumPlatformPages;
		size_t reservedNumBytes;

		Memory(const MemoryType& inType): GCObject(ObjectKind::memory), type(inType), baseAddress(nullptr), numPages(0), maxPages(0), reservedBaseAddress(nullptr), reservedNumPlatformPages(0), reservedNumBytes(0) {}
		~Memory() override;
	};

	struct GlobalInstance : GCObject
	{
		GlobalType type;
		UntaggedValue value;

		GlobalInstance(GlobalType inType,UntaggedValue inValue): GCObject(ObjectKind::global), type(inType), value(inValue) {}
	};

	struct ModuleInstance : GCObject
	{
		std::vector<Object*> imports;
		std::map<std::string,Object*> exportMap;

		std::vector<FunctionInstance*> functionDefs;

		std::vector<FunctionInstance*> functions;
		std::vector<Table*> tables;
		std::vector<Memory*> memories;
		std::vector<GlobalInstance*> globals;

		Memory* defaultMemory;
		Table* defaultTable;

		LLVMJIT::JITModuleBase* jitModule;

		ModuleInstance(std::vector<Object*>&& inImports)
		: GCObject(ObjectKind::module)
		, imports(inImports)
		, defaultMemory(nullptr)
		, defaultTable(nullptr)
		, jitModule(nullptr)
		{}

		~ModuleInstance() override;
	};

	// Initializes global state used by the WAVM intrinsics.
	void initWAVMIntrinsics();

	// Describes an execution context. Returns an array of strings, one for each stack frame.
	std::vector<std::string> describeExecutionContext(const Platform::ExecutionContext& executionContext);

	// Checks whether an address is owned by a table or memory.
	bool isAddressOwnedByTable(uint8* address);
	bool isAddressOwnedByMemory(uint8* address);

}

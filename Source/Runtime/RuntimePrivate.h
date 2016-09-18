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

	struct JITModule;

	void init();
	bool instantiateModule(const WebAssembly::Module& module,Runtime::ModuleInstance* moduleInstance);
	bool describeInstructionPointer(uintptr ip,std::string& outDescription);
	
	typedef void (*InvokeFunctionPointer)(void*,uint64*);

	// Generates an invoke thunk for a specific function type.
	InvokeFunctionPointer getInvokeThunk(const WebAssembly::FunctionType* functionType);
}

namespace Runtime
{
	using namespace WebAssembly;

	struct FunctionInstance : Object
	{
		const FunctionType* type;
		void* nativeFunction;
		std::string debugName;

		FunctionInstance(const FunctionType* inType,void* inNativeFunction = nullptr,const char* inDebugName = "<unidentified FunctionInstance>")
		: Object(ObjectKind::function), type(inType), nativeFunction(inNativeFunction), debugName(inDebugName) {}
	};

	struct Table : Object
	{
		struct Element
		{
			const FunctionType* type;
			void* value;
		};

		TableType type;
		Element* baseAddress;
		size_t numElements;
		size_t maxPlatformPages;
		
		uint8* reservedBaseAddress;
		size_t reservedNumPlatformPages;

		Table(const TableType& inType): Object(ObjectKind::table), type(inType), baseAddress(nullptr), numElements(0), maxPlatformPages(0), reservedBaseAddress(nullptr), reservedNumPlatformPages(0) {}
		~Table() override;
	};

	struct Memory : Object
	{
		MemoryType type;
		uint8* baseAddress;
		size_t numPages;
		size_t maxPages;
		
		uint8* reservedBaseAddress;
		size_t reservedNumPlatformPages;
		size_t reservedNumBytes;

		Memory(const MemoryType& inType): Object(ObjectKind::memory), type(inType), baseAddress(nullptr), numPages(0), maxPages(0), reservedBaseAddress(nullptr), reservedNumPlatformPages(0), reservedNumBytes(0) {}
		~Memory() override;
	};

	struct GlobalInstance : Object
	{
		GlobalType type;
		UntaggedValue value;

		GlobalInstance(GlobalType inType,UntaggedValue inValue): Object(ObjectKind::global), type(inType), value(inValue) {}
	};
	
	struct ModuleInstance : Object
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

		LLVMJIT::JITModule* jitModule;

		ModuleInstance(std::vector<Object*>&& inImports)
		: Object({ObjectKind::module})
		, imports(inImports)
		, defaultMemory(nullptr)
		, defaultTable(nullptr)
		, jitModule(nullptr)
		{}
	};

	// Initializes global state used by the WAVM intrinsics.
	void initWAVMIntrinsics();

	// Describes an execution context. Returns an array of strings, one for each stack frame.
	std::vector<std::string> describeExecutionContext(const Platform::ExecutionContext& executionContext);

	// Checks whether an address is owned by a table.
	bool isAddressOwnedByTable(uint8* address);
}

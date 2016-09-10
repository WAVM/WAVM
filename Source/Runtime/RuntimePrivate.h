#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"
#include "Runtime.h"
#include "WebAssembly/WebAssembly.h"
#include "WebAssembly/Module.h"

#include <functional>

namespace Runtime
{
	using namespace WebAssembly;

	typedef void (*InvokeFunctionPointer)(uint64*);

	struct StackFrame
	{
		uintptr ip;
		uintptr bp;
	};

	struct ExecutionContext
	{
		std::vector<StackFrame> stackFrames;
	};
	
	typedef void (*InvokeThunk)(void*);
	struct FunctionInstance : Object
	{
		const FunctionType* type;
		void* nativeFunction;
		InvokeThunk invokeThunk;
		std::string debugName;

		FunctionInstance(const FunctionType* inType,void* inNativeFunction = nullptr,InvokeThunk inInvokeThunk = nullptr,const char* inDebugName = "<unidentified FunctionInstance>")
		: Object(ObjectKind::function), type(inType), nativeFunction(inNativeFunction), invokeThunk(inInvokeThunk), debugName(inDebugName) {}
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
		const Module& module;

		std::vector<Object*> imports;
		std::map<std::string,Object*> exportMap;

		std::vector<FunctionInstance*> functionDefs;

		std::vector<FunctionInstance*> functions;
		std::vector<Table*> tables;
		std::vector<Memory*> memories;
		std::vector<GlobalInstance*> globals;

		Memory* defaultMemory;
		Table* defaultTable;

		ModuleInstance(const Module& inModule,std::vector<Object*>&& inImports)
		: Object({ObjectKind::module})
		, module(inModule)
		, imports(inImports)
		, defaultMemory(nullptr)
		, defaultTable(nullptr)
		{}
	};

	// Initializes global state used by the WAVM intrinsics.
	void initWAVMIntrinsics();

	// Describes a stack frame.
	std::string describeStackFrame(const StackFrame& frame);

	// Describes an execution context. Returns an array of strings, one for each stack frame.
	std::vector<std::string> describeExecutionContext(const ExecutionContext& executionContext);

	// Checks whether an address is owned by a table.
	bool isAddressOwnedByTable(uint8* address);
}

namespace RuntimePlatform
{
	// Initializes thread-specific state.
	void initThread();

	// Calls a thunk and catches any runtime exception thrown by it.
	Runtime::Value catchRuntimeExceptions(const std::function<Runtime::Value()>& thunk);
	
	// Raises a runtime exception.
	[[noreturn]] void raiseException(Runtime::Exception* exception);

	// Describes an instruction pointer 
	bool describeInstructionPointer(uintptr ip,std::string& outDescription);

	// Captures the execution context of the caller.
	Runtime::ExecutionContext captureExecutionContext();

	#ifdef _WIN32
		// Registers the data used by Windows SEH to unwind stack frames.
		void registerSEHUnwindInfo(uintptr textLoadAddress,uintptr xdataLoadAddress,uintptr pdataLoadAddress,size_t pdataNumBytes);
	#endif
}

namespace LLVMJIT
{
	using namespace Runtime;

	void init();
	bool instantiateModule(const WebAssembly::Module& module,ModuleInstance* moduleInstance);
	bool describeInstructionPointer(uintptr ip,std::string& outDescription);
}

#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"
#include "Runtime.h"

#include <functional>

namespace Runtime
{
	struct StackFrame
	{
		uintptr_t ip;
		uintptr_t bp;
	};

	struct ExecutionContext
	{
		std::vector<StackFrame> stackFrames;
	};

	// Initializes the instance memory.
	bool initInstanceMemory();
	
	// Initializes the various intrinsic modules.
	bool initEmscriptenIntrinsics(const AST::Module* module);
	void initWebAssemblyIntrinsics();
	void initWAVMIntrinsics();

	// Describes a stack frame.
	std::string describeStackFrame(const StackFrame& frame);

	// Describes an execution context. Returns an array of strings, one for each stack frame.
	std::vector<std::string> describeExecutionContext(const ExecutionContext& executionContext);

	// Causes an exception.
	void causeException(Exception::Cause cause);
}

namespace RuntimePlatform
{
	// Calls a thunk and catches any runtime exception thrown by it.
	Runtime::Value catchRuntimeExceptions(const std::function<Runtime::Value()>& thunk);
	
	// Raises a runtime exception.
	void raiseException(Runtime::Exception* exception);

	// Describes an instruction pointer 
	bool describeInstructionPointer(uintptr_t ip,std::string& outDescription);

	// Captures the execution context of the caller.
	Runtime::ExecutionContext captureExecutionContext();

	#ifdef _WIN32
		// Registers the data used by Windows SEH to unwind stack frames.
		void registerSEHUnwindInfo(uintptr textLoadAddress,uintptr xdataLoadAddress,uintptr pdataLoadAddress,size_t pdataNumBytes);
	#endif
}

namespace LLVMJIT
{
	typedef void (*InvokeFunctionPointer)(uint64*);

	void init();

	bool compileModule(const AST::Module* astModule);
	InvokeFunctionPointer getInvokeFunctionPointer(const AST::Module* module,uintptr functionIndex,bool invokeThunk);
	
	bool describeInstructionPointer(uintptr_t ip,std::string& outDescription);
}

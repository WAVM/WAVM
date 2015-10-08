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

	// The base of the virtual address space allocated for the VM.
	// This is never changed after it is initialized.
	extern uint8* instanceMemoryBase;

	// The number of bytes of address-space reserved (but not necessarily committed) for the VM.
	// This should be a power of two, and is never changed after it is initialized.
	extern size_t instanceAddressSpaceMaxBytes;
	
	// Commits or decommits memory in the VM virtual address space.
	uint32 vmSbrk(int32 numBytes);

	// Given an address as a byte index, returns a typed reference to that address of VM memory.
	template<typename memoryType> memoryType& instanceMemoryRef(uintptr address)
	{
		return *(memoryType*)(instanceMemoryBase + address);
	}
	
	// Initializes the instance memory.
	bool initInstanceMemory();
	
	// Initializes the various intrinsic modules.
	void initEmscriptenIntrinsics();
	void initWebAssemblyIntrinsics();
	void initWAVMIntrinsics();

	// Describes a stack frame.
	std::string describeStackFrame(const StackFrame& frame);

	// Describes an execution context. Returns an array of strings, one for each stack frame.
	std::vector<std::string> describeExecutionContext(const ExecutionContext& executionContext);
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
	void init();

	bool compileModule(const AST::Module* astModule);
	void* getFunctionPointer(const AST::Module* module,uintptr functionIndex);
	
	bool describeInstructionPointer(uintptr_t ip,std::string& outDescription);
}

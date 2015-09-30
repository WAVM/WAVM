#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"
#include "Runtime.h"

namespace Runtime
{
	enum class ExceptionCause
	{
		AccessViolation,
		StackOverflow,
		IntegerDivideByZero,
		IntegerOverflow,
		Unknown
	};
	
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
	
	// Initializes intrinsics used by Emscripten.
	void initEmscriptenIntrinsics();

	// Initializes intrinsics used by WebAssembly.
	void initWebAssemblyIntrinsics();
	
	// Handles an exception.
	void handleGlobalException(ExceptionCause cause,const ExecutionContext& context);

	// Prints an execution context.
	void printExecutionContext(const ExecutionContext& context);
}

namespace RuntimePlatform
{
	// Initializes the platform's global exception handler.
	void initGlobalExceptionHandlers();

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

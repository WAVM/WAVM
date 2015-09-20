#pragma once

#include "Core/Core.h"

namespace AST { struct Module; }

namespace Runtime
{
	// The base of the virtual address space allocated for the VM.
	// This is never changed after it is initialized.
	extern uint8* instanceMemoryBase;

	// The number of bytes of address-space reserved (but not necessarily committed) for the VM.
	// This should be a power of two, and is never changed after it is initialized.
	extern size_t instanceAddressSpaceMaxBytes;

	// Initializes the instance memory.
	extern bool initInstanceMemory(size_t maxBytes);

	// Commits or decommits memory in the VM virtual address space.
	extern uint32 vmSbrk(int32 numBytes);

	// Initializes intrinsics used by Emscripten.
	void initEmscriptenIntrinsics();

	// Initializes intrinsics used by WebAssembly.
	void initWebAssemblyIntrinsics();

	// Given an address as a byte index, returns a typed reference to that address of VM memory.
	template<typename memoryType> memoryType& instanceMemoryRef(uint32 address)
	{
		return *(memoryType*)(instanceMemoryBase + address);
	}
	
	// Generates native code for an AST module.
	bool compileModule(const AST::Module* module);

	// Gets a pointer to the native code for the given function of a module.
	// If the module hasn't yet been passed to jitCompileModule, will return nullptr.
	void* getFunctionPointer(const AST::Module* module,uintptr_t functionIndex);
}
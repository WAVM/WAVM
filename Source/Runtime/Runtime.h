#pragma once

#include "Core/Core.h"

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

	// Initializes intrinsic values used by WASM from Emscripten.
	void initEmscriptenIntrinsics();

	// Given an address as a byte index, returns a typed reference to that address of VM memory.
	template<typename memoryType> memoryType& instanceMemoryRef(uint32 address)
	{
		return *(memoryType*)(instanceMemoryBase + address);
	}
}
#pragma once

#include "Common/WAVM.h"
#include "AST/AST.h"

namespace LLVMJIT
{
	// Generates native code for a WAVM module.
	bool compileModule(const AST::Module* module);

	// Gets a pointer to the native code for the given function of a module.
	// If the module hasn't yet been passed to jitCompileModule, will return nullptr.
	void* getFunctionPointer(const AST::Module* module,uintptr_t functionIndex);
}

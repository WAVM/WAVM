#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"
#include "RuntimePrivate.h"
#include "AST/AST.h"
#include "AST/ASTExpressions.h"
#include "AST/ASTDispatch.h"
#include "Intrinsics.h"
#include "Core/MemoryArena.h"

#ifdef _WIN32
	#pragma warning(push)
	#pragma warning (disable:4267)
	#pragma warning (disable:4800)
	#pragma warning (disable:4291)
	#pragma warning (disable:4244)
	#pragma warning (disable:4351)
	#pragma warning (disable:4065)
	#pragma warning (disable:4624)
	#pragma warning (disable:4245)	// conversion from 'int' to 'unsigned int', signed/unsigned mismatch
	#pragma warning(disable:4146) // unary minus operator applied to unsigned type, result is still unsigned
	#pragma warning(disable:4458) // declaration of 'x' hides class member
	#pragma warning(disable:4510) // default constructor could not be generated
	#pragma warning(disable:4610) // struct can never be instantiated - user defined constructor required
#endif

#include "llvm/Analysis/Passes.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/LambdaResolver.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/SymbolSize.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>

#ifdef _WIN32
	#pragma warning(pop)
#endif

namespace LLVMJIT
{
	std::string getExternalFunctionName(uintptr_t functionIndex,bool invokeThunk);
	bool getFunctionIndexFromExternalName(const char* externalName,uintptr_t& outFunctionIndex);

	// Emits LLVM IR for a module.
	llvm::Module* emitModule(const AST::Module* astModule);
}

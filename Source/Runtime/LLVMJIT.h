#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"
#include "Runtime.h"
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
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Vectorize.h"
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

using namespace AST;

namespace LLVMJIT
{
	// The global LLVM context.
	extern llvm::LLVMContext& context;
	
	// Information about a JITed module.
	struct JITModule
	{
		const Module* astModule;
		llvm::Module* llvmModule;
		std::vector<llvm::Function*> functions;
		std::vector<llvm::GlobalVariable*> functionImportPointers;
		std::vector<llvm::GlobalVariable*> functionTablePointers;
		llvm::Value* instanceMemoryBase;
		llvm::Value* instanceMemoryAddressMask;
		llvm::orc::ObjectLinkingLayerBase::ObjSetHandleT handle;

		JITModule(const Module* inASTModule)
		:	astModule(inASTModule)
		,	llvmModule(new llvm::Module("",context))
		,	instanceMemoryBase(nullptr)
		,	instanceMemoryAddressMask(nullptr)
		,	handle()
		{}
	};

	// A dummy constant to use as the unique value inhabiting the void type.
	extern llvm::Constant* voidDummy;

	// Zero constants of each type.
	extern llvm::Constant* typedZeroConstants[(size_t)TypeId::num];

	// Converts an AST type to a LLVM type.
	llvm::Type* asLLVMType(TypeId type);
	
	// Converts an AST function type to a LLVM type.
	llvm::FunctionType* asLLVMType(const FunctionType& functionType);
	
	// Overloaded functions that compile a literal value to a LLVM constant of the right type.
	inline llvm::ConstantInt* compileLiteral(uint8 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(asLLVMType(TypeId::I8),llvm::APInt(8,(uint64)value,false)); }
	inline llvm::ConstantInt* compileLiteral(uint16 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(asLLVMType(TypeId::I16),llvm::APInt(16,(uint64)value,false)); }
	inline llvm::ConstantInt* compileLiteral(uint32 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(asLLVMType(TypeId::I32),llvm::APInt(32,(uint64)value,false)); }
	inline llvm::ConstantInt* compileLiteral(uint64 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(asLLVMType(TypeId::I64),llvm::APInt(64,(uint64)value,false)); }
	inline llvm::Constant* compileLiteral(float32 value) { return llvm::ConstantFP::get(context,llvm::APFloat(value)); }
	inline llvm::Constant* compileLiteral(float64 value) { return llvm::ConstantFP::get(context,llvm::APFloat(value)); }
	inline llvm::Constant* compileLiteral(bool value) { return llvm::ConstantInt::get(asLLVMType(TypeId::Bool),llvm::APInt(1,value ? 1 : 0,false)); }
	
	// Emits LLVM IR for a module.
	JITModule* emitModule(const Module* astModule);
}
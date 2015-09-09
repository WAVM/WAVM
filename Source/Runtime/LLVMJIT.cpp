#include "WAVM.h"
#include "AST/AST.h"
#include "AST/ASTExpressions.h"
#include "AST/ASTDispatch.h"
#include "Intrinsics.h"
#include "Memory.h"

#pragma warning(push)
#pragma warning (disable:4267)
#pragma warning (disable:4800)
#pragma warning (disable:4291)
#pragma warning (disable:4244)
#pragma warning (disable:4351)
#pragma warning (disable:4065)
#pragma warning (disable:4624)
#pragma warning (disable:4245)	// conversion from 'int' to 'unsigned int', signed/unsigned mismatch

#include "llvm/Analysis/Passes.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Host.h"
#include "llvm/Transforms/Scalar.h"
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>

#pragma warning(pop)

namespace LLVMJIT
{
	using namespace AST;
	
	llvm::LLVMContext& context = llvm::getGlobalContext();
	bool isInitialized = false;

	// Maps a type ID to the corresponding LLVM type.
	llvm::Type* llvmTypesByTypeId[(size_t)TypeId::num];

	// All the modules that have been JITted.
	std::vector<struct JITModule*> jitModules;
	
	// Converts an AST type to a LLVM type.
	llvm::Type* asLLVMType(TypeId type) { return llvmTypesByTypeId[(uintptr_t)type]; }
	
	// Converts an AST function type to a LLVM type.
	llvm::FunctionType* asLLVMType(const FunctionType& functionType)
	{
		auto llvmArgTypes = (llvm::Type**)alloca(sizeof(llvm::Type*) * functionType.parameters.size());
		for(uintptr_t argIndex = 0;argIndex < functionType.parameters.size();++argIndex)
		{
			llvmArgTypes[argIndex] = asLLVMType(functionType.parameters[argIndex]);
		}
		auto llvmReturnType = asLLVMType(functionType.returnType);
		return llvm::FunctionType::get(llvmReturnType,llvm::ArrayRef<llvm::Type*>(llvmArgTypes,functionType.parameters.size()),false);
	}

	// Converts an AST name to a LLVM name. Ensures that the name is not-null, and prefixes it to ensure it doesn't conflict with export names.
	llvm::Twine getLLVMName(const char* nullableName) { return nullableName ? (llvm::Twine('_') + llvm::Twine(nullableName)) : ""; }

	// Overloaded functions that compile a literal value to a LLVM constant of the right type.
	llvm::ConstantInt* compileLiteral(uint8_t value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(asLLVMType(TypeId::I8),llvm::APInt(8,(uint64_t)value,false)); }
	llvm::ConstantInt* compileLiteral(uint16_t value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(asLLVMType(TypeId::I16),llvm::APInt(16,(uint64_t)value,false)); }
	llvm::ConstantInt* compileLiteral(uint32_t value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(asLLVMType(TypeId::I32),llvm::APInt(32,(uint64_t)value,false)); }
	llvm::ConstantInt* compileLiteral(uint64_t value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(asLLVMType(TypeId::I64),llvm::APInt(64,(uint64_t)value,false)); }
	llvm::Constant* compileLiteral(float value) { return llvm::ConstantFP::get(context,llvm::APFloat(value)); }
	llvm::Constant* compileLiteral(double value) { return llvm::ConstantFP::get(context,llvm::APFloat(value)); }
	llvm::Constant* compileLiteral(bool value) { return llvm::ConstantInt::get(asLLVMType(TypeId::Bool),llvm::APInt(1,value ? 1 : 0,false)); }
	
	// Information about a JITed module.
	struct JITModule
	{
		const Module* astModule;
		llvm::Module* llvmModule;
		std::vector<llvm::Function*> functions;
		std::vector<llvm::GlobalVariable*> globalVariablePointers;
		std::vector<llvm::GlobalVariable*> functionImportPointers;
		std::vector<llvm::GlobalVariable*> functionTablePointers;
		llvm::GlobalVariable* virtualAddressBase;
		llvm::ExecutionEngine* executionEngine;

		JITModule(const Module* inASTModule)
		:	astModule(inASTModule)
		,	llvmModule(new llvm::Module("",context))
		,	executionEngine(nullptr)
		{}
	};

	// The context used by functions involved in JITing a single AST function.
	struct JITFunctionContext
	{
		JITModule& jitModule;
		const Module* astModule;
		Function* astFunction;
		llvm::Function* llvmFunction;
		llvm::IRBuilder<> irBuilder;

		llvm::Value** localVariablePointers;
		
		// An arena for allocations that can be discarded after compiling the function.
		Memory::ScopedArena scopedArena;

		// Information about an incoming edge to a branch target.
		struct BranchResult
		{
			llvm::BasicBlock* incomingBlock;
			llvm::Value* value;
			BranchResult* next;
		};

		// Information about an in-scope branch target.
		struct BranchContext
		{
			BranchTarget* branchTarget;
			llvm::BasicBlock* basicBlock;
			BranchContext* outerContext;
			BranchResult* results;
		};
	
		// A linked list of in-scope branch targets.
		BranchContext* branchContext;

		JITFunctionContext(JITModule& inJITModule,uintptr_t functionIndex)
		: jitModule(inJITModule)
		, astModule(inJITModule.astModule)
		, astFunction(astModule->functions[functionIndex])
		, llvmFunction(jitModule.functions[functionIndex])
		, branchContext(nullptr)
		, irBuilder(context)
		{}

		void compile();
		
		// Allows this object to be used as a visitor by the AST dispatcher.
		typedef llvm::Value* DispatchResult;
		
		// Inserts a branch, and returns the old basic block.
		llvm::BasicBlock* compileBranch(llvm::BasicBlock* dest)
		{
			irBuilder.CreateBr(dest);
			return irBuilder.GetInsertBlock();
		}

		// Inserts a conditional branch, and returns the old basic block.
		llvm::BasicBlock* compileCondBranch(llvm::Value* condition,llvm::BasicBlock* trueDest,llvm::BasicBlock* falseDest)
		{
			irBuilder.CreateCondBr(condition,trueDest,falseDest);
			return irBuilder.GetInsertBlock();
		}

		// Returns a LLVM intrinsic with the given id and argument types.
		DispatchResult getLLVMIntrinsic(const std::initializer_list<llvm::Type*>& argTypes,llvm::Intrinsic::ID id)
		{
			return llvm::Intrinsic::getDeclaration(jitModule.llvmModule,id,llvm::ArrayRef<llvm::Type*>(argTypes.begin(),argTypes.end()));
		}

		DispatchResult compileAddress(Expression<IntClass>* address,bool isFarAddress)
		{
			if(isFarAddress)
			{
				// Until we reserve more than 4GB of virtual address space, mask a 64-bit address to just the lower 32-bits.
				return irBuilder.CreateAnd(dispatch(*this,address,TypeId::I64),compileLiteral((uint64_t)0xffffffff));
			}
			else
			{
				// If the address is 32-bits, zext it to 64-bits.
				// This is crucial for security, as LLVM will otherwise implicitly sign extend it to 64-bits in the GEP below,
				// interpreting it as a signed offset and allowing access to memory outside the sandboxed memory range.
				return irBuilder.CreateZExt(dispatch(*this,address,TypeId::I32),llvm::Type::getInt64Ty(context));
			}
		}

		DispatchResult compileCall(const FunctionType& functionType,llvm::Value* function,UntypedExpression** args)
		{
			// Compile the parameter values for the call.
			auto llvmArgs = (llvm::Value**)alloca(sizeof(llvm::Value*) * functionType.parameters.size());
			for(size_t argIndex = 0;argIndex < functionType.parameters.size();++argIndex)
				{ llvmArgs[argIndex] = dispatch(*this,args[argIndex],functionType.parameters[argIndex]); }
			// Create the call instruction.
			return irBuilder.CreateCall(function,llvm::ArrayRef<llvm::Value*>(llvmArgs,functionType.parameters.size()));
		}
		
		template<typename Type> DispatchResult visitLiteral(const Literal<Type>* literal) { return compileLiteral(literal->value); }

		template<typename Class>
		DispatchResult visitError(TypeId type,const Error<Class>* error)
		{
			std::cerr << "Found error node while compiling:" << std::endl;
			std::cerr << error->message << std::endl;
			throw;
		}

		template<typename Class>
		DispatchResult visitLoadVariable(TypeId type,const LoadVariable<Class>* loadVariable,typename OpTypes<Class>::getLocal)
		{
			assert(loadVariable->variableIndex < astFunction->locals.size());
			return irBuilder.CreateLoad(localVariablePointers[loadVariable->variableIndex]);
		}
		template<typename Class>
		DispatchResult visitLoadVariable(TypeId type,const LoadVariable<Class>* loadVariable,typename OpTypes<Class>::loadGlobal)
		{
			assert(loadVariable->variableIndex < jitModule.globalVariablePointers.size());
			return irBuilder.CreateLoad(jitModule.globalVariablePointers[loadVariable->variableIndex]);
		}
		template<typename Class>
		DispatchResult visitLoadMemory(TypeId type,const LoadMemory<Class>* getMemory)
		{
			// Compile the address as a byte index, get a pointer to that byte index relative to the virtualAddressBase.
			auto byteIndex = compileAddress(getMemory->address,getMemory->isFarAddress);
			auto bytePointer = irBuilder.CreateInBoundsGEP(irBuilder.CreateLoad(jitModule.virtualAddressBase),byteIndex);
			// Cast the pointer to the appopriate type and load it.
			auto typedPointer = irBuilder.CreatePointerCast(bytePointer,asLLVMType(type)->getPointerTo());
			return irBuilder.CreateLoad(typedPointer);
		}

		template<typename Class>
		DispatchResult visitCall(TypeId type,const Call<Class>* call,typename OpTypes<Class>::callDirect)
		{
			auto astFunction = astModule->functions[call->functionIndex];
			assert(astFunction->type.returnType == type);
			return compileCall(astFunction->type,jitModule.functions[call->functionIndex],call->parameters);
		}
		template<typename Class>
		DispatchResult visitCall(TypeId type,const Call<Class>* call,typename OpTypes<Class>::callImport)
		{
			auto astFunctionImport = astModule->functionImports[call->functionIndex];
			assert(astFunctionImport.type.returnType == type);
			auto function = jitModule.functionImportPointers[call->functionIndex];
			return compileCall(astFunctionImport.type,function,call->parameters);
		}
		template<typename Class>
		DispatchResult visitCallIndirect(TypeId type,const CallIndirect<Class>* callIndirect)
		{
			assert(callIndirect->tableIndex < astModule->functionTables.size());
			auto functionTablePointer = jitModule.functionTablePointers[callIndirect->tableIndex];
			auto astFunctionTable = astModule->functionTables[callIndirect->tableIndex];
			assert(astFunctionTable.type.returnType == type);
			assert(astFunctionTable.numFunctions > 0);

			// Compile the function index and mask it to be within the function table's bounds (which are already verified to be 2^N).
			auto functionIndex = dispatch(*this,callIndirect->functionIndex,TypeId::I32);
			auto functionIndexMask = compileLiteral((uint32_t)astFunctionTable.numFunctions-1);
			auto maskedFunctionIndex = irBuilder.CreateAnd(functionIndex,functionIndexMask);

			// Get a pointer to the function pointer in the function table using the masked function index.
			llvm::Value* gepIndices[2] = {compileLiteral((uint32_t)0),maskedFunctionIndex};

			// Load the function pointer from the table and call it.
			auto function = irBuilder.CreateLoad(irBuilder.CreateInBoundsGEP(functionTablePointer,gepIndices));
			return compileCall(astFunctionTable.type,function,callIndirect->parameters);
		}
		
		template<typename Class>
		DispatchResult visitSwitch(TypeId type,const Switch<Class>* switchExpression)
		{
			auto value = dispatch(*this,switchExpression->key);		

			// Create the basic blocks for each arm of the switch so they can be forward referenced by fallthrough branches.
			auto armEntryBlocks = new(scopedArena) llvm::BasicBlock*[switchExpression->numArms + 1];
			for(uint32_t armIndex = 0;armIndex < switchExpression->numArms;++armIndex)
			{ armEntryBlocks[armIndex] = llvm::BasicBlock::Create(context,"switchArm",llvmFunction); }

			// Create and link the context for this switch's branch target into the list of in-scope contexts.
			auto successorBlock = llvm::BasicBlock::Create(context,"switchSucc",llvmFunction);
			armEntryBlocks[switchExpression->numArms] = successorBlock;
			auto outerBranchContext = branchContext;
			BranchContext endBranchContext = {switchExpression->endTarget,successorBlock,outerBranchContext,nullptr};
			branchContext = &endBranchContext;

			// Compile each arm of the switch.
			assert(switchExpression->numArms > 0);
			assert(switchExpression->defaultArmIndex < switchExpression->numArms);
			auto defaultBlock = armEntryBlocks[switchExpression->defaultArmIndex];
			auto switchInstruction = irBuilder.CreateSwitch(value,defaultBlock,(uint32_t)switchExpression->numArms);
			for(uint32_t armIndex = 0;armIndex < switchExpression->numArms;++armIndex)
			{
				const SwitchArm& arm = switchExpression->arms[armIndex];
				llvm::ConstantInt* armKey;
				switch(switchExpression->key.type)
				{
				case TypeId::I8: armKey = compileLiteral((uint8_t)arm.key); break;
				case TypeId::I16: armKey = compileLiteral((uint16_t)arm.key); break;
				case TypeId::I32: armKey = compileLiteral((uint32_t)arm.key); break;
				case TypeId::I64: armKey = compileLiteral((uint64_t)arm.key); break;
				default: throw;
				}
				switchInstruction->addCase(armKey,armEntryBlocks[armIndex]);
				irBuilder.SetInsertPoint(armEntryBlocks[armIndex]);
				assert(arm.value);
				if(armIndex + 1 == switchExpression->numArms && type != TypeId::Void)
				{
					// The final arm is an expression of the same type as the switch.
					auto value = dispatch(*this,arm.value,type);
					auto exitBlock = compileBranch(successorBlock);
					assert(value);
					assert(exitBlock);
					endBranchContext.results = new(scopedArena) BranchResult {exitBlock,value,endBranchContext.results};
				}
				else
				{
					// The other arms yield void.
					dispatch(*this,arm.value,TypeId::Void);
					compileBranch(armEntryBlocks[armIndex + 1]);
				}
			}

			// Remove the switch's branch target from the in-scope context list.
			assert(branchContext == &endBranchContext);
			branchContext = outerBranchContext;

			irBuilder.SetInsertPoint(successorBlock);
			if(type == TypeId::Void) { return nullptr; }
			else
			{
				// Create a phi node that merges the results from all the branches out of the switch.
				auto phi = irBuilder.CreatePHI(asLLVMType(type),(uint32_t)switchExpression->numArms);
				for(auto result = endBranchContext.results;result;result = result->next) { phi->addIncoming(result->value,result->incomingBlock); }
				return phi;
			}
		}
		template<typename Class>
		DispatchResult visitIfElse(TypeId type,const IfElse<Class>* ifElse)
		{
			auto condition = dispatch(*this,ifElse->condition,TypeId::Bool);

			auto trueBlock = llvm::BasicBlock::Create(context,"ifThen",llvmFunction);
			auto falseBlock = llvm::BasicBlock::Create(context,"ifElse",llvmFunction);
			auto successorBlock = llvm::BasicBlock::Create(context,"ifSucc",llvmFunction);

			compileCondBranch(condition,trueBlock,falseBlock);

			irBuilder.SetInsertPoint(trueBlock);
			auto trueValue = dispatch(*this,ifElse->thenExpression,type);
			auto trueExitBlock = compileBranch(successorBlock);

			irBuilder.SetInsertPoint(falseBlock);
			auto falseValue = dispatch(*this,ifElse->elseExpression,type);
			auto falseExitBlock = compileBranch(successorBlock);

			irBuilder.SetInsertPoint(successorBlock);
			if(type == TypeId::Void) { return nullptr; }
			else
			{
				auto phi = irBuilder.CreatePHI(trueValue->getType(),2);
				phi->addIncoming(trueValue,trueExitBlock);
				phi->addIncoming(falseValue,falseExitBlock);			
				return phi;
			}
		}
		template<typename Class>
		DispatchResult visitLabel(TypeId type,const Label<Class>* label)
		{
			auto labelBlock = llvm::BasicBlock::Create(context,"labelContinue",llvmFunction);
			auto successorBlock = llvm::BasicBlock::Create(context,"labelBreak",llvmFunction);
			
			compileBranch(labelBlock);
			irBuilder.SetInsertPoint(labelBlock);

			// Create and link the context for this label's branch target into the list of in-scope contexts.
			auto outerBranchContext = branchContext;
			BranchContext endBranchContext = {label->endTarget,successorBlock,outerBranchContext,nullptr};
			branchContext = &endBranchContext;
			
			// Compile the label's value.
			auto value = dispatch(*this,label->expression,type);

			// Remove the label's branch target from the in-scope context list.
			assert(branchContext == &endBranchContext);
			branchContext = outerBranchContext;

			// Branch to the successor block.
			auto exitBlock = compileBranch(successorBlock);
			irBuilder.SetInsertPoint(successorBlock);

			// Create a phi node that merges all the possible values yielded by the label into one.
			if(type == TypeId::Void) { return nullptr; }
			else
			{
				auto phi = irBuilder.CreatePHI(asLLVMType(type),2);
				if(value) { phi->addIncoming(value,exitBlock); }
				for(auto result = endBranchContext.results;result;result = result->next) { phi->addIncoming(result->value,result->incomingBlock); }
				return phi;
			}
		}
		template<typename Class>
		DispatchResult visitSequence(TypeId type,const Sequence<Class>* seq)
		{
			dispatch(*this,seq->voidExpression);
			return dispatch(*this,seq->resultExpression,type);
		}
		template<typename Class>
		DispatchResult visitReturn(const Return<Class>* ret)
		{
			if(astFunction->type.returnType == TypeId::Void) { irBuilder.CreateRetVoid(); }
			else { irBuilder.CreateRet(dispatch(*this,ret->value,astFunction->type.returnType)); }
			
			// Create a new block to insert unreachable instructions into.
			irBuilder.SetInsertPoint(llvm::BasicBlock::Create(context,"returnUnreachable",llvmFunction));

			return nullptr;
		}
		template<typename Class>
		DispatchResult visitLoop(TypeId type,const Loop<Class>* loop)
		{
			auto loopBlock = llvm::BasicBlock::Create(context,"doLoop",llvmFunction);
			auto successorBlock = llvm::BasicBlock::Create(context,"doSucc",llvmFunction);
			
			// Create and link the contexts for this label's branch targets into the list of in-scope contexts.
			auto outerBranchContext = branchContext;
			BranchContext continueBranchContext = {loop->continueTarget,loopBlock,outerBranchContext,nullptr};
			BranchContext breakBranchContext = {loop->breakTarget,successorBlock,&continueBranchContext,nullptr};
			branchContext = &breakBranchContext;
			
			compileBranch(loopBlock);

			irBuilder.SetInsertPoint(loopBlock);
			dispatch(*this,loop->expression);
			compileBranch(loopBlock);
			
			// Remove the loop's branch targets from the in-scope context list.
			assert(branchContext == &breakBranchContext);
			branchContext = outerBranchContext;

			irBuilder.SetInsertPoint(successorBlock);
			if(type == TypeId::Void) { return nullptr; }
			else
			{
				auto phi = irBuilder.CreatePHI(asLLVMType(type),1);
				for(auto result = breakBranchContext.results;result;result = result->next) { phi->addIncoming(result->value,result->incomingBlock); }
				return phi;
			}
		}
		template<typename Class>
		DispatchResult visitBranch(const Branch<Class>* branch)
		{
			// Find the branch target context for this branch's target.
			auto targetContext = branchContext;
			while(targetContext && targetContext->branchTarget != branch->branchTarget) { targetContext = targetContext->outerContext; }
			if(!targetContext) { throw; }
			
			// If the branch target has a non-void type, compile the branch's value.
			auto value = branch->value ? dispatch(*this,branch->value,targetContext->branchTarget->type) : nullptr;
			
			// Insert the branch instruction.
			auto exitBlock = compileBranch(targetContext->basicBlock);
			
			// Add the branch's value to the list of incoming values for the branch target.
			if(value) { targetContext->results = new(scopedArena) BranchResult {exitBlock,value,targetContext->results}; }

			// Create a new block to insert unreachable instructions following the branch into.
			irBuilder.SetInsertPoint(llvm::BasicBlock::Create(context,"branchUnreachable",llvmFunction));

			return nullptr;
		}

		DispatchResult visitNop(const Nop*)
		{
			return nullptr;
		}
		DispatchResult visitDiscardResult(const DiscardResult* discardResult)
		{
			dispatch(*this,discardResult->expression);
			return nullptr;
		}
		
		DispatchResult visitStoreVariable(const StoreVariable* storeVariable,OpTypes<VoidClass>::setLocal)
		{
			assert(storeVariable->variableIndex < astFunction->locals.size());
			auto value = dispatch(*this,storeVariable->value,astFunction->locals[storeVariable->variableIndex].type);
			return irBuilder.CreateStore(value,localVariablePointers[storeVariable->variableIndex]);
		}
		DispatchResult visitStoreVariable(const StoreVariable* storeVariable,OpTypes<VoidClass>::storeGlobal)
		{
			assert(storeVariable->variableIndex < jitModule.globalVariablePointers.size());
			auto value = dispatch(*this,storeVariable->value,astModule->globals[storeVariable->variableIndex].type);
			return irBuilder.CreateStore(value,jitModule.globalVariablePointers[storeVariable->variableIndex]);
		}
		DispatchResult visitStoreMemory(const StoreMemory* storeMemory)
		{
			// Compile the value.
			auto value = dispatch(*this,storeMemory->value);
			
			// Compile the address as a byte index, get a pointer to that byte index relative to the virtualAddressBase.
			auto byteIndex = compileAddress(storeMemory->address,storeMemory->isFarAddress);
			auto bytePointer = irBuilder.CreateInBoundsGEP(irBuilder.CreateLoad(jitModule.virtualAddressBase),byteIndex);

			// Cast the pointer to the appopriate type and store the value to it.
			auto typedPointer = irBuilder.CreatePointerCast(bytePointer,asLLVMType(storeMemory->value.type)->getPointerTo());
			return irBuilder.CreateStore(value,typedPointer);
		}
		
		DispatchResult compileIntrinsic(llvm::Intrinsic::ID intrinsicId,llvm::Value* firstOperand)
		{
			auto operandType = firstOperand->getType();
			auto intrinsic = getLLVMIntrinsic({operandType},intrinsicId);
			return irBuilder.CreateCall(intrinsic,firstOperand);
		}
		DispatchResult compileIntrinsic(llvm::Intrinsic::ID intrinsicId,llvm::Value* firstOperand,llvm::Value* secondOperand)
		{
			auto intrinsic = getLLVMIntrinsic({firstOperand->getType(),secondOperand->getType()},intrinsicId);
			return irBuilder.CreateCall(intrinsic,llvm::ArrayRef<llvm::Value*>({firstOperand,secondOperand}));
		}
		
		llvm::Value* compileIntAbs(llvm::Value* operand)
		{
			auto mask = irBuilder.CreateAShr(operand,operand->getType()->getScalarSizeInBits() - 1);
			return irBuilder.CreateXor(irBuilder.CreateAdd(operand,mask),mask);
		}

		template<typename Class,typename OpAsType> DispatchResult visitUnary(TypeId type,const Unary<Class>* unary,OpAsType);
		template<typename Class,typename OpAsType> DispatchResult visitBinary(TypeId type,const Binary<Class>* binary,OpAsType);
		template<typename Class,typename OpAsType> DispatchResult visitCast(TypeId type,const Cast<Class>* cast,OpAsType);

		#define IMPLEMENT_UNARY_OP(class,op,llvmOp) \
			template<> DispatchResult visitUnary(TypeId type,const Unary<class>* unary,OpTypes<class>::op) \
			{ \
				auto operand = dispatch(*this,unary->operand,type); \
				return llvmOp; \
			}
		#define IMPLEMENT_BINARY_OP(class,op,llvmOp) \
			template<> DispatchResult visitBinary(TypeId type,const Binary<class>* binary,OpTypes<class>::op) \
			{ \
				auto left = dispatch(*this,binary->left,type); \
				auto right = dispatch(*this,binary->right,type); \
				return llvmOp; \
			}
		#define IMPLEMENT_CAST_OP(class,op,llvmOp) \
			DispatchResult visitCast(TypeId type,const Cast<class>* cast,OpTypes<class>::op) \
			{ \
				auto source = dispatch(*this,cast->source); \
				auto destType = asLLVMType(type); \
				return llvmOp; \
			}
		#define IMPLEMENT_COMPARE_OP(op,llvmOp) \
			DispatchResult visitComparison(const Comparison* compare,OpTypes<BoolClass>::op) \
			{ \
				auto left = dispatch(*this,compare->left,compare->operandType); \
				auto right = dispatch(*this,compare->right,compare->operandType); \
				return llvmOp; \
			}

		IMPLEMENT_UNARY_OP(IntClass,neg,irBuilder.CreateNeg(operand))
		IMPLEMENT_UNARY_OP(IntClass,abs,compileIntAbs(operand))
		IMPLEMENT_UNARY_OP(IntClass,not,irBuilder.CreateNot(operand))
		IMPLEMENT_UNARY_OP(IntClass,clz,irBuilder.CreateCall(getLLVMIntrinsic({operand->getType()},llvm::Intrinsic::ctlz),llvm::ArrayRef<llvm::Value*>({operand,compileLiteral(false)})))
		IMPLEMENT_UNARY_OP(IntClass,ctz,irBuilder.CreateCall(getLLVMIntrinsic({operand->getType()},llvm::Intrinsic::cttz),llvm::ArrayRef<llvm::Value*>({operand,compileLiteral(false)})))
		IMPLEMENT_UNARY_OP(IntClass,popcnt,compileIntrinsic(llvm::Intrinsic::ctpop,operand))
		IMPLEMENT_BINARY_OP(IntClass,add,irBuilder.CreateAdd(left,right))
		IMPLEMENT_BINARY_OP(IntClass,sub,irBuilder.CreateSub(left,right))
		IMPLEMENT_BINARY_OP(IntClass,mul,irBuilder.CreateMul(left,right))
		IMPLEMENT_BINARY_OP(IntClass,divs,irBuilder.CreateSDiv(left,right))
		IMPLEMENT_BINARY_OP(IntClass,divu,irBuilder.CreateUDiv(left,right))
		IMPLEMENT_BINARY_OP(IntClass,rems,irBuilder.CreateSRem(left,right))
		IMPLEMENT_BINARY_OP(IntClass,remu,irBuilder.CreateURem(left,right))
		IMPLEMENT_BINARY_OP(IntClass,mins,irBuilder.CreateSelect(irBuilder.CreateICmpSLT(left,right),left,right))
		IMPLEMENT_BINARY_OP(IntClass,minu,irBuilder.CreateSelect(irBuilder.CreateICmpULT(left,right),left,right))
		IMPLEMENT_BINARY_OP(IntClass,maxs,irBuilder.CreateSelect(irBuilder.CreateICmpSGT(left,right),left,right))
		IMPLEMENT_BINARY_OP(IntClass,maxu,irBuilder.CreateSelect(irBuilder.CreateICmpUGT(left,right),left,right))
		IMPLEMENT_BINARY_OP(IntClass,and,irBuilder.CreateAnd(left,right))
		IMPLEMENT_BINARY_OP(IntClass,or,irBuilder.CreateOr(left,right))
		IMPLEMENT_BINARY_OP(IntClass,xor,irBuilder.CreateXor(left,right))
		IMPLEMENT_BINARY_OP(IntClass,shl,irBuilder.CreateShl(left,right))
		IMPLEMENT_BINARY_OP(IntClass,shr,irBuilder.CreateLShr(left,right))
		IMPLEMENT_BINARY_OP(IntClass,sar,irBuilder.CreateAShr(left,right))
		IMPLEMENT_CAST_OP(IntClass,wrap,irBuilder.CreateTrunc(source,destType))
		IMPLEMENT_CAST_OP(IntClass,truncSignedFloat,irBuilder.CreateFPToSI(source,destType))
		IMPLEMENT_CAST_OP(IntClass,truncUnsignedFloat,irBuilder.CreateFPToUI(source,destType))
		IMPLEMENT_CAST_OP(IntClass,sext,irBuilder.CreateSExt(source,destType))
		IMPLEMENT_CAST_OP(IntClass,zext,irBuilder.CreateSExt(source,destType))
		IMPLEMENT_CAST_OP(IntClass,reinterpretFloat,irBuilder.CreateBitCast(source,destType))
		IMPLEMENT_CAST_OP(IntClass,reinterpretBool,irBuilder.CreateZExt(source,destType))
		
		IMPLEMENT_UNARY_OP(FloatClass,neg,irBuilder.CreateFNeg(operand))
		IMPLEMENT_UNARY_OP(FloatClass,abs,compileIntrinsic(llvm::Intrinsic::fabs,operand))
		IMPLEMENT_UNARY_OP(FloatClass,ceil,compileIntrinsic(llvm::Intrinsic::ceil,operand))
		IMPLEMENT_UNARY_OP(FloatClass,floor,compileIntrinsic(llvm::Intrinsic::floor,operand))
		IMPLEMENT_UNARY_OP(FloatClass,trunc,compileIntrinsic(llvm::Intrinsic::trunc,operand))
		IMPLEMENT_UNARY_OP(FloatClass,nearestInt,compileIntrinsic(llvm::Intrinsic::nearbyint,operand))
		IMPLEMENT_UNARY_OP(FloatClass,cos,compileIntrinsic(llvm::Intrinsic::cos,operand))
		IMPLEMENT_UNARY_OP(FloatClass,sin,compileIntrinsic(llvm::Intrinsic::sin,operand))
		IMPLEMENT_UNARY_OP(FloatClass,sqrt,compileIntrinsic(llvm::Intrinsic::sqrt,operand))
		IMPLEMENT_UNARY_OP(FloatClass,exp,compileIntrinsic(llvm::Intrinsic::exp,operand))
		IMPLEMENT_UNARY_OP(FloatClass,log,compileIntrinsic(llvm::Intrinsic::log,operand))
		IMPLEMENT_BINARY_OP(FloatClass,pow,compileIntrinsic(llvm::Intrinsic::pow,left,right))
		IMPLEMENT_BINARY_OP(FloatClass,add,irBuilder.CreateFAdd(left,right))
		IMPLEMENT_BINARY_OP(FloatClass,sub,irBuilder.CreateFSub(left,right))
		IMPLEMENT_BINARY_OP(FloatClass,mul,irBuilder.CreateFMul(left,right))
		IMPLEMENT_BINARY_OP(FloatClass,div,irBuilder.CreateFDiv(left,right))
		IMPLEMENT_BINARY_OP(FloatClass,rem,irBuilder.CreateFRem(left,right))
		IMPLEMENT_BINARY_OP(FloatClass,min,compileIntrinsic(llvm::Intrinsic::minnum,left,right))
		IMPLEMENT_BINARY_OP(FloatClass,max,compileIntrinsic(llvm::Intrinsic::maxnum,left,right))
		IMPLEMENT_BINARY_OP(FloatClass,copySign,compileIntrinsic(llvm::Intrinsic::copysign,left,right))
		IMPLEMENT_CAST_OP(FloatClass,convertSignedInt,irBuilder.CreateSIToFP(source,destType))
		IMPLEMENT_CAST_OP(FloatClass,convertUnsignedInt,irBuilder.CreateUIToFP(source,destType))
		IMPLEMENT_CAST_OP(FloatClass,promote,irBuilder.CreateFPExt(source,destType))
		IMPLEMENT_CAST_OP(FloatClass,demote,irBuilder.CreateFPTrunc(source,destType))
		IMPLEMENT_CAST_OP(FloatClass,reinterpretInt,irBuilder.CreateBitCast(source,destType))

		IMPLEMENT_UNARY_OP(BoolClass,not,irBuilder.CreateNot(operand))
		IMPLEMENT_BINARY_OP(BoolClass,and,irBuilder.CreateAnd(left,right))
		IMPLEMENT_BINARY_OP(BoolClass,or,irBuilder.CreateOr(left,right))

		IMPLEMENT_COMPARE_OP(eq,isTypeClass(compare->operandType,TypeClassId::Float) ? irBuilder.CreateFCmpUEQ(left,right) : irBuilder.CreateICmpEQ(left,right))
		IMPLEMENT_COMPARE_OP(neq,isTypeClass(compare->operandType,TypeClassId::Float) ? irBuilder.CreateFCmpUNE(left,right) : irBuilder.CreateICmpNE(left,right))
		IMPLEMENT_COMPARE_OP(lt,irBuilder.CreateFCmpULT(left,right))
		IMPLEMENT_COMPARE_OP(lts,irBuilder.CreateICmpSLT(left,right))
		IMPLEMENT_COMPARE_OP(ltu,irBuilder.CreateICmpULT(left,right))
		IMPLEMENT_COMPARE_OP(le,irBuilder.CreateFCmpULE(left,right))
		IMPLEMENT_COMPARE_OP(les,irBuilder.CreateICmpSLE(left,right))
		IMPLEMENT_COMPARE_OP(leu,irBuilder.CreateICmpULE(left,right))
		IMPLEMENT_COMPARE_OP(gt,irBuilder.CreateFCmpUGT(left,right))
		IMPLEMENT_COMPARE_OP(gts,irBuilder.CreateICmpSGT(left,right))
		IMPLEMENT_COMPARE_OP(gtu,irBuilder.CreateICmpUGT(left,right))
		IMPLEMENT_COMPARE_OP(ge,irBuilder.CreateFCmpUGE(left,right))
		IMPLEMENT_COMPARE_OP(ges,irBuilder.CreateICmpSGE(left,right))
		IMPLEMENT_COMPARE_OP(geu,irBuilder.CreateICmpUGE(left,right))

		#undef IMPLEMENT_UNARY_OP
		#undef IMPLEMENT_BINARY_OP
		#undef IMPLEMENT_CAST_OP
		#undef IMPLEMENT_COMPARE_OP
	};


	struct MCJITMemoryManager : public llvm::SectionMemoryManager
	{
		// Called by MCJIT to resolve symbols by name.
		virtual uint64_t getSymbolAddress(const std::string& name)
		{
			// We assume JITModule::finalize has validated the types of any imports against the intrinsic functions available.
			const Intrinsics::Function* intrinsicFunction = Intrinsics::findFunction(name.c_str());
			if(intrinsicFunction) { return *(uint64_t*)&intrinsicFunction->value; }
			
			const Intrinsics::Value* intrinsicValue = Intrinsics::findValue(name.c_str());
			if(intrinsicValue) { return *(uint64_t*)&intrinsicValue->value; }

			std::cerr << "getSymbolAddress: " << name << " not found" << std::endl;
			throw;
		}
	};
	
	void JITFunctionContext::compile()
	{
		// Create an initial basic block for the function.
		auto entryBasicBlock = llvm::BasicBlock::Create(context,"entry",llvmFunction);
		irBuilder.SetInsertPoint(entryBasicBlock);

		// Create allocas for all the locals and initialize them to zero.
		localVariablePointers = new(scopedArena) llvm::Value*[astFunction->locals.size()];
		for(uintptr_t localIndex = 0;localIndex < astFunction->locals.size();++localIndex)
		{
			auto localVariable = astFunction->locals[localIndex];
			localVariablePointers[localIndex] = irBuilder.CreateAlloca(asLLVMType(localVariable.type),nullptr,getLLVMName(localVariable.name));
			switch(localVariable.type)
			{
			case TypeId::I8: irBuilder.CreateStore(compileLiteral((uint8_t)0),localVariablePointers[localIndex]); break;
			case TypeId::I16: irBuilder.CreateStore(compileLiteral((uint16_t)0),localVariablePointers[localIndex]); break;
			case TypeId::I32: irBuilder.CreateStore(compileLiteral((uint32_t)0),localVariablePointers[localIndex]); break;
			case TypeId::I64: irBuilder.CreateStore(compileLiteral((uint64_t)0),localVariablePointers[localIndex]); break;
			case TypeId::F32: irBuilder.CreateStore(compileLiteral((float)0.0f),localVariablePointers[localIndex]); break;
			case TypeId::F64: irBuilder.CreateStore(compileLiteral((double)0.0),localVariablePointers[localIndex]); break;
			case TypeId::Bool: irBuilder.CreateStore(compileLiteral((bool)false),localVariablePointers[localIndex]); break;
			default: throw;
			}
		}

		// Move the function arguments into the corresponding local variable allocas.
		uintptr_t parameterIndex = 0;
		for(auto llvmArgIt = llvmFunction->arg_begin();llvmArgIt != llvmFunction->arg_end();++parameterIndex,++llvmArgIt)
		{
			auto localIndex = astFunction->parameterLocalIndices[parameterIndex];
			irBuilder.CreateStore(llvmArgIt,localVariablePointers[localIndex]);
		}

		// Traverse the function's expressions.
		auto value = dispatch(*this,astFunction->expression,astFunction->type.returnType);

		if(irBuilder.GetInsertBlock())
		{
			if(astFunction->type.returnType == TypeId::Void) { irBuilder.CreateRetVoid(); }
			else if(value) { irBuilder.CreateRet(value); }
			else { irBuilder.CreateUnreachable(); }
		}
	}
	
	static void init()
	{
		isInitialized = true;

		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
		llvm::InitializeNativeTargetAsmParser();

		llvmTypesByTypeId[(size_t)TypeId::None] = nullptr;
		llvmTypesByTypeId[(size_t)TypeId::I8] = llvm::Type::getInt8Ty(context);
		llvmTypesByTypeId[(size_t)TypeId::I16] = llvm::Type::getInt16Ty(context);
		llvmTypesByTypeId[(size_t)TypeId::I32] = llvm::Type::getInt32Ty(context);
		llvmTypesByTypeId[(size_t)TypeId::I64] = llvm::Type::getInt64Ty(context);
		llvmTypesByTypeId[(size_t)TypeId::F32] = llvm::Type::getFloatTy(context);
		llvmTypesByTypeId[(size_t)TypeId::F64] = llvm::Type::getDoubleTy(context);
		llvmTypesByTypeId[(size_t)TypeId::Bool] = llvm::Type::getInt1Ty(context);
		llvmTypesByTypeId[(size_t)TypeId::Void] = llvm::Type::getVoidTy(context);
		
		// For memory protection, allocate a full 32-bit address space of virtual pages.
		const size_t numAllocatedVirtualPages = 1ull << (32 - Memory::getPreferredVirtualPageSizeLog2());
		WAVM::vmVirtualAddressBase = Memory::allocateVirtualPages(numAllocatedVirtualPages);
		assert(WAVM::vmVirtualAddressBase);
	}

	bool compileModule(const Module* astModule)
	{
		if(!isInitialized)
		{
			init();
		}

		// Create a JIT module.
		WAVM::Timer llvmGenTimer;
		JITModule* jitModule = new JITModule(astModule);
		jitModules.push_back(jitModule);
	
		// Bind the memory buffer to the global variable used by the module as the base address for memory accesses.
		auto virtualAddressBaseValue = llvm::Constant::getIntegerValue(
			llvm::Type::getInt8PtrTy(context),
			llvm::APInt(64,*(uint64_t*)&WAVM::vmVirtualAddressBase)
			);
		jitModule->virtualAddressBase = new llvm::GlobalVariable(*jitModule->llvmModule,llvm::Type::getInt8PtrTy(context),true,llvm::GlobalVariable::PrivateLinkage,virtualAddressBaseValue,"virtualAddressBase");

		// Create the LLVM functions.
		jitModule->functions.resize(astModule->functions.size());
		for(uintptr_t functionIndex = 0;functionIndex < astModule->functions.size();++functionIndex)
		{
			auto astFunction = astModule->functions[functionIndex];
			auto llvmFunctionType = asLLVMType(astFunction->type);
			jitModule->functions[functionIndex] = llvm::Function::Create(llvmFunctionType,llvm::Function::PrivateLinkage,getLLVMName(astFunction->name),jitModule->llvmModule);
		}

		// Give exported functions the appropriate name and linkage.
		for(auto exportIt : astModule->exportNameToFunctionIndexMap)
		{
			assert(exportIt.second < jitModule->functions.size());
			jitModule->functions[exportIt.second]->setLinkage(llvm::GlobalValue::ExternalLinkage);
			jitModule->functions[exportIt.second]->setName(exportIt.first);
		}

		// Create the global variables.
		jitModule->globalVariablePointers.resize(astModule->globals.size());
		for(uintptr_t importIndex = 0;importIndex < jitModule->globalVariablePointers.size();++importIndex)
		{
			auto globalVariable = astModule->globals[importIndex];
			llvm::Constant* initializer;
			switch(globalVariable.type)
			{
			case TypeId::I8: initializer = compileLiteral((uint8_t)0); break;
			case TypeId::I16: initializer = compileLiteral((uint16_t)0); break;
			case TypeId::I32: initializer = compileLiteral((uint32_t)0); break;
			case TypeId::I64: initializer = compileLiteral((uint64_t)0); break;
			case TypeId::F32: initializer = compileLiteral((float)0.0f); break;
			case TypeId::F64: initializer = compileLiteral((double)0.0); break;
			case TypeId::Bool: initializer = compileLiteral(false); break;
			default: throw;
			}
			jitModule->globalVariablePointers[importIndex] = new llvm::GlobalVariable(*jitModule->llvmModule,asLLVMType(globalVariable.type),false,llvm::GlobalValue::PrivateLinkage,initializer,getLLVMName(globalVariable.name));
		}
		
		// Set imported global variables to have external linkage and give them the unmangled import name.
		for(uintptr_t variableImportIndex = 0;variableImportIndex < astModule->variableImports.size();++variableImportIndex)
		{
			auto variableImport = astModule->variableImports[variableImportIndex];
			jitModule->globalVariablePointers[variableImport.globalIndex]->setName(variableImport.name);
			jitModule->globalVariablePointers[variableImport.globalIndex]->setLinkage(llvm::GlobalValue::ExternalLinkage);
			jitModule->globalVariablePointers[variableImport.globalIndex]->setInitializer(nullptr);
		}

		// Create the function import globals.
		jitModule->functionImportPointers.resize(astModule->functionImports.size());
		for(uintptr_t importIndex = 0;importIndex < jitModule->functionImportPointers.size();++importIndex)
		{
			auto functionImport = astModule->functionImports[importIndex];
			jitModule->functionImportPointers[importIndex] = new llvm::GlobalVariable(*jitModule->llvmModule,asLLVMType(functionImport.type),true,llvm::GlobalValue::ExternalLinkage,nullptr,functionImport.name);
		}

		// Create the function table globals.
		jitModule->functionTablePointers.resize(astModule->functionTables.size());
		for(uintptr_t tableIndex = 0;tableIndex < astModule->functionTables.size();++tableIndex)
		{
			auto astFunctionTable = astModule->functionTables[tableIndex];
			std::vector<llvm::Constant*> llvmFunctionTableElements;
			llvmFunctionTableElements.resize(astFunctionTable.numFunctions);
			for(uint32_t functionIndex = 0;functionIndex < astFunctionTable.numFunctions;++functionIndex)
			{
				assert(astFunctionTable.functionIndices[functionIndex] < jitModule->functions.size());
				llvmFunctionTableElements[functionIndex] = jitModule->functions[astFunctionTable.functionIndices[functionIndex]];
			}
			// Verify that the number of elements is a power of two, so we can use bitwise and to prevent out-of-bounds accesses.
			assert((astFunctionTable.numFunctions & (astFunctionTable.numFunctions-1)) == 0);

			// Create a LLVM global variable that holds the array of function pointers.
			auto llvmFunctionTablePointerType = llvm::ArrayType::get(asLLVMType(astFunctionTable.type)->getPointerTo(),llvmFunctionTableElements.size());
			auto llvmFunctionTablePointer = new llvm::GlobalVariable(
				*jitModule->llvmModule,llvmFunctionTablePointerType,true,llvm::GlobalValue::PrivateLinkage,
				llvm::ConstantArray::get(llvmFunctionTablePointerType,llvmFunctionTableElements)
				);
			jitModule->functionTablePointers[tableIndex] = llvmFunctionTablePointer;
		}

		// Compile each function in the module.
		for(uintptr_t functionIndex = 0;functionIndex < astModule->functions.size();++functionIndex)
		{
			JITFunctionContext(*jitModule,functionIndex).compile();
		}
		std::cout << "Generated LLVM code for module in " << llvmGenTimer.getMilliseconds() << "ms" << std::endl;
		
		// Work around a problem with LLVM generating a COFF file that MCJIT can't parse. Adding -elf to the target triple forces it to use ELF instead of COFF.
		#if WIN32
			jitModule->llvmModule->setTargetTriple(llvm::sys::getProcessTriple() + "-elf");
		#endif

		// Create the MCJIT execution engine for this module.
		std::string errStr;
		jitModule->executionEngine = llvm::EngineBuilder(std::unique_ptr<llvm::Module>(jitModule->llvmModule))
			.setErrorStr(&errStr)
			.setOptLevel(llvm::CodeGenOpt::Aggressive)
			.setMCJITMemoryManager(std::unique_ptr<llvm::RTDyldMemoryManager>(new MCJITMemoryManager()))
			.create();
		if(!jitModule->executionEngine)
		{
			std::cerr << "Could not create ExecutionEngine: " << errStr << std::endl;
			return false;
		}

		// Look up intrinsic functions that match the name+type of functions imported by the module, and bind them to the global variable used by the module to access the import.
		bool missingImport = false;
		for(uintptr_t functionImportIndex = 0;functionImportIndex < astModule->functionImports.size();++functionImportIndex)
		{
			auto functionImport = astModule->functionImports[functionImportIndex];
			const Intrinsics::Function* intrinsicFunction = Intrinsics::findFunction(functionImport.name);
			void* functionPointer = intrinsicFunction && intrinsicFunction->type == functionImport.type ? intrinsicFunction->value : nullptr;
			if(!functionPointer)
			{
				std::cerr << "Missing imported function " << functionImport.name << " : (";
				for(auto argIt = functionImport.type.parameters.begin();argIt != functionImport.type.parameters.end();++argIt)
				{
					if(argIt != functionImport.type.parameters.begin()) { std::cerr << ","; }
					std::cerr << getTypeName(*argIt);
				}
				std::cerr << ") -> " << getTypeName(functionImport.type.returnType) << std::endl;
				missingImport = true;
			}
			else { jitModule->executionEngine->addGlobalMapping(jitModule->functionImportPointers[functionImportIndex],functionPointer); }
		}

		// Look up intrinsic values that match the name+type of values imported by the module, and bind them to the global variable used by the module to access the import.
		for(uintptr_t variableImportIndex = 0;variableImportIndex < astModule->variableImports.size();++variableImportIndex)
		{
			auto variableImport = astModule->variableImports[variableImportIndex];
			const Intrinsics::Value* intrinsicValue = Intrinsics::findValue(variableImport.name);
			if(intrinsicValue && variableImport.type != intrinsicValue->type) { intrinsicValue = NULL; }
			if(!intrinsicValue)
			{
				std::cerr << "Missing imported variable " << variableImport.name << " : " << getTypeName(variableImport.type) << std::endl;
				missingImport = true;
			}
			else { jitModule->executionEngine->addGlobalMapping(jitModule->globalVariablePointers[variableImport.globalIndex],intrinsicValue->value); }
		}

		// Fail if there were any missing imports.
		if(missingImport) { return false; }

		// Copy the module's data segments into VM memory.
		if(astModule->initialNumBytesMemory >= (1ull<<32)) { throw; }
		WAVM::vmSbrk((int32_t)astModule->initialNumBytesMemory);
		for(auto dataSegment : astModule->dataSegments)
		{
			assert(dataSegment.baseAddress + dataSegment.numBytes <= astModule->initialNumBytesMemory);
			memcpy(WAVM::vmVirtualAddressBase + dataSegment.baseAddress,dataSegment.data,dataSegment.numBytes);
		}

		// Initialize the Emscripten intrinsics.
		WAVM::initEmscriptenIntrinsics();

		// Run some optimization on the module's functions.		
		WAVM::Timer optimizationTimer;
		auto fpm = new llvm::legacy::FunctionPassManager(jitModule->llvmModule);
		jitModule->llvmModule->setDataLayout(*jitModule->executionEngine->getDataLayout());
		fpm->add(llvm::createPromoteMemoryToRegisterPass());
		fpm->add(llvm::createBasicAliasAnalysisPass());
		fpm->add(llvm::createInstructionCombiningPass());
		fpm->add(llvm::createReassociatePass());
		//fpm->add(llvm::createGVNPass()); - doesn't improve the generated code enough to justify the cost
		fpm->add(llvm::createCFGSimplificationPass());
		fpm->doInitialization();
		for(auto functionIt = jitModule->llvmModule->begin();functionIt != jitModule->llvmModule->end();++functionIt)
		{ fpm->run(*functionIt); }
		delete fpm;
		std::cout << "Optimized LLVM code in " << optimizationTimer.getMilliseconds() << "ms" << std::endl;

		// Generate native machine code.
		WAVM::Timer machineCodeTimer;
		jitModule->executionEngine->finalizeObject();
		std::cout << "Generated machine code in " << machineCodeTimer.getMilliseconds() << "ms" << std::endl;

		return true;
	}

	void* getFunctionPointer(const Module* module,uintptr_t functionIndex)
	{
		for(auto jitModule : jitModules)
		{
			if(jitModule->astModule == module)
			{
				return (void*)jitModule->executionEngine->getFunctionAddress(jitModule->functions[functionIndex]->getName());
			}
		}
		return nullptr;
	}
}
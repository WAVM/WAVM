#include "LLVMJIT.h"

using namespace AST;

namespace LLVMJIT
{
	// The global LLVM context.
	llvm::LLVMContext& context = llvm::getGlobalContext();

	// Maps a type ID to the corresponding LLVM type.
	llvm::Type* llvmTypesByTypeId[(size_t)TypeId::num];

	// Zero constants of each type.
	llvm::Constant* typedZeroConstants[(size_t)TypeId::num];
	
	// A dummy constant to use as the unique value inhabiting the void type.
	llvm::Constant* voidDummy;

	// Converts an AST type to a LLVM type.
	llvm::Type* asLLVMType(TypeId type) { return llvmTypesByTypeId[(uintptr)type]; }
	
	// Converts an AST function type to a LLVM type.
	llvm::FunctionType* asLLVMType(const FunctionType& functionType)
	{
		auto llvmArgTypes = (llvm::Type**)alloca(sizeof(llvm::Type*) * functionType.parameters.size());
		for(uintptr argIndex = 0;argIndex < functionType.parameters.size();++argIndex)
		{
			llvmArgTypes[argIndex] = asLLVMType(functionType.parameters[argIndex]);
		}
		auto llvmReturnType = asLLVMType(functionType.returnType);
		return llvm::FunctionType::get(llvmReturnType,llvm::ArrayRef<llvm::Type*>(llvmArgTypes,functionType.parameters.size()),false);
	}
	
	// Converts an AST name to a LLVM name. Ensures that the name is not-null, and prefixes it to ensure it doesn't conflict with export names.
	llvm::Twine getLLVMName(const char* nullableName) { return nullableName ? (llvm::Twine('_') + llvm::Twine(nullableName)) : ""; }
	
	// Overloaded functions that compile a literal value to a LLVM constant of the right type.
	inline llvm::ConstantInt* compileLiteral(uint8 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(asLLVMType(TypeId::I8),llvm::APInt(8,(uint64)value,false)); }
	inline llvm::ConstantInt* compileLiteral(uint16 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(asLLVMType(TypeId::I16),llvm::APInt(16,(uint64)value,false)); }
	inline llvm::ConstantInt* compileLiteral(uint32 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(asLLVMType(TypeId::I32),llvm::APInt(32,(uint64)value,false)); }
	inline llvm::ConstantInt* compileLiteral(uint64 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(asLLVMType(TypeId::I64),llvm::APInt(64,(uint64)value,false)); }
	inline llvm::Constant* compileLiteral(float32 value) { return llvm::ConstantFP::get(context,llvm::APFloat(value)); }
	inline llvm::Constant* compileLiteral(float64 value) { return llvm::ConstantFP::get(context,llvm::APFloat(value)); }
	inline llvm::Constant* compileLiteral(bool value) { return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context),llvm::APInt(1,value ? 1 : 0,false)); }

	// The LLVM IR for a module.
	struct ModuleIR
	{
		llvm::Module* llvmModule;
		std::vector<llvm::Function*> functions;
		std::vector<llvm::GlobalVariable*> functionImportPointers;
		llvm::GlobalVariable* functionTablePointer;
		llvm::Value* numFunctionTableEntries;
		llvm::Value* instanceMemoryBase;
		llvm::Value* instanceMemoryAddressMask;
		
		std::map<FunctionType,uintptr> functionTypeToSignatureIndexMap;

		ModuleIR()
		:	llvmModule(new llvm::Module("",context))
		,	functionTablePointer(nullptr)
		,	numFunctionTableEntries(nullptr)
		,	instanceMemoryBase(nullptr)
		,	instanceMemoryAddressMask(nullptr)
		{}

		uintptr getSignatureIndex(const FunctionType& type)
		{
			auto signatureIt = functionTypeToSignatureIndexMap.find(type);
			uintptr signatureIndex;
			if(signatureIt != functionTypeToSignatureIndexMap.end()) { signatureIndex = signatureIt->second; }
			else
			{
				signatureIndex = functionTypeToSignatureIndexMap.size();
				functionTypeToSignatureIndexMap[type] = signatureIndex;
			}
			return signatureIndex;
		}
	};

	// The context used by functions involved in JITing a single AST function.
	struct EmitFunctionContext
	{
		ModuleIR& moduleIR;
		const Module* astModule;
		Function* astFunction;
		llvm::Function* llvmFunction;
		llvm::IRBuilder<> irBuilder;

		llvm::Value** localVariablePointers;

		llvm::BasicBlock* unreachableBlock;
		
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

		EmitFunctionContext(ModuleIR& inModuleIR,const Module* inASTModule,uintptr functionIndex)
		: moduleIR(inModuleIR)
		, astModule(inASTModule)
		, astFunction(astModule->functions[functionIndex])
		, llvmFunction(inModuleIR.functions[functionIndex])
		, irBuilder(context)
		, localVariablePointers(nullptr)
		, branchContext(nullptr)
		{
			unreachableBlock = llvm::BasicBlock::Create(context,"unreachable",llvmFunction);
		}

		void emit();
		
		// The result of the functions called by the AST dispatcher.
		// If the inner expression doesn't return control to the outer, then it will be null.
		typedef llvm::Value* DispatchResult;
		
		// Inserts a branch, and returns the old basic block.
		llvm::BasicBlock* compileBranch(llvm::BasicBlock* dest)
		{
			auto exitBlock = irBuilder.GetInsertBlock();
			if(exitBlock == unreachableBlock) { return nullptr; }
			else
			{
				irBuilder.CreateBr(dest);
				return exitBlock;
			}
		}

		// Coerces an I32 value to a boolean value.
		llvm::Value* coerceI32ToBool(llvm::Value* i32Value)
		{
			return irBuilder.CreateICmpNE(i32Value,typedZeroConstants[(size_t)TypeId::I32]);
		}

		// Inserts a conditional branch, and returns the old basic block.
		llvm::BasicBlock* compileCondBranch(llvm::Value* booleanCondition,llvm::BasicBlock* trueDest,llvm::BasicBlock* falseDest)
		{
			auto exitBlock = irBuilder.GetInsertBlock();
			if(exitBlock == unreachableBlock) { return nullptr; }
			else
			{
				irBuilder.CreateCondBr(booleanCondition,trueDest,falseDest);
				return exitBlock;
			}
		}
		
		// Compiles an if-else expression using thunks to define the true and false branches.
		template<typename TrueValueThunk,typename FalseValueThunk>
		llvm::Value* compileIfElse(TypeId type,llvm::Value* booleanCondition,TrueValueThunk trueValueThunk,FalseValueThunk falseValueThunk)
		{
			auto trueBlock = llvm::BasicBlock::Create(context,"ifThen",llvmFunction);
			auto falseBlock = llvm::BasicBlock::Create(context,"ifElse",llvmFunction);
			auto successorBlock = llvm::BasicBlock::Create(context,"ifSucc",llvmFunction);
			
			compileCondBranch(booleanCondition,trueBlock,falseBlock);

			irBuilder.SetInsertPoint(trueBlock);
			auto trueValue = trueValueThunk();
			auto trueExitBlock = compileBranch(successorBlock);

			irBuilder.SetInsertPoint(falseBlock);
			auto falseValue = falseValueThunk();
			auto falseExitBlock = compileBranch(successorBlock);

			irBuilder.SetInsertPoint(successorBlock);
			if(type == TypeId::Void) { return voidDummy; }
			else if(!trueExitBlock && !falseExitBlock) { return typedZeroConstants[(uintptr)type]; }
			else
			{
				auto phi = irBuilder.CreatePHI(trueValue->getType(),2);
				if(trueExitBlock) { phi->addIncoming(trueValue,trueExitBlock); }
				if(falseExitBlock) { phi->addIncoming(falseValue,falseExitBlock); }
				return phi;
			}
		}

		// Returns a LLVM intrinsic with the given id and argument types.
		DispatchResult getLLVMIntrinsic(const std::initializer_list<llvm::Type*>& argTypes,llvm::Intrinsic::ID id)
		{
			return llvm::Intrinsic::getDeclaration(moduleIR.llvmModule,id,llvm::ArrayRef<llvm::Type*>(argTypes.begin(),argTypes.end()));
		}

		DispatchResult compileAddress(Expression<IntClass>* address,uint64 offset,bool isFarAddress,TypeId memoryType)
		{
			// On a 64 bit runtime, if the address is 32-bits, zext it to 64-bits.
			// This is crucial for security, as LLVM will otherwise implicitly sign extend it to 64-bits in the GEP below,
			// interpreting it as a signed offset and allowing access to memory outside the sandboxed memory range.
			// There are no 'far addresses' in a 32 bit runtime.
			auto byteIndex = dispatch(*this,address,isFarAddress ? TypeId::I64 : TypeId::I32);
			auto offsetByteIndex = offset ? irBuilder.CreateAdd(byteIndex,isFarAddress ? compileLiteral((uint64)offset) : compileLiteral((uint32)offset)) : byteIndex;
			auto nativeByteIndex =
					sizeof(uintptr) == 8 && !isFarAddress ? irBuilder.CreateZExt(offsetByteIndex,llvm::Type::getInt64Ty(context))
				:	sizeof(uintptr) == 4 && isFarAddress ? irBuilder.CreateTrunc(offsetByteIndex,llvm::Type::getInt32Ty(context))
				:	offsetByteIndex;

			// Mask the index to the address-space size.
			auto maskedByteIndex = irBuilder.CreateAnd(nativeByteIndex,moduleIR.instanceMemoryAddressMask);

			// Cast the pointer to the appropriate type.
			auto bytePointer = irBuilder.CreateInBoundsGEP(moduleIR.instanceMemoryBase,maskedByteIndex);
			return irBuilder.CreatePointerCast(bytePointer,asLLVMType(memoryType)->getPointerTo());
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

		DispatchResult visitError(TypeId type,const Error* error)
		{
			std::cerr << "Found error node while compiling:" << std::endl;
			std::cerr << error->message << std::endl;
			throw;
		}

		// Local get/set
		DispatchResult visitGetLocal(TypeId type,const GetLocal* getVariable)
		{
			assert(getVariable->variableIndex < astFunction->locals.size());
			return irBuilder.CreateLoad(localVariablePointers[getVariable->variableIndex]);
		}
		DispatchResult visitSetLocal(const SetLocal* setVariable)
		{
			assert(setVariable->variableIndex < astFunction->locals.size());
			auto value = dispatch(*this,setVariable->value,astFunction->locals[setVariable->variableIndex].type);
			irBuilder.CreateStore(value,localVariablePointers[setVariable->variableIndex]);
			return value;
		}

		// Memory load/store
		template<typename Class>
		DispatchResult visitLoad(TypeId type,const Load<Class>* load,typename OpTypes<AnyClass>::load)
		{
			assert(type == load->memoryType);
			auto llvmLoad = irBuilder.CreateLoad(compileAddress(load->address,load->offset,load->isFarAddress,load->memoryType),true);
			llvmLoad->setAlignment(1<<load->alignmentLog2);
			return llvmLoad;
		}
		DispatchResult visitLoad(TypeId type,const Load<IntClass>* load,OpTypes<AnyClass>::load)
		{
			auto memoryValue = irBuilder.CreateLoad(compileAddress(load->address,load->offset,load->isFarAddress,load->memoryType),true);
			memoryValue->setAlignment(1<<load->alignmentLog2);
			assert(isTypeClass(load->memoryType,TypeClassId::Int));
			return type == load->memoryType ? memoryValue
				: irBuilder.CreateTrunc(memoryValue,asLLVMType(type));
		}
		DispatchResult visitLoad(TypeId type,const Load<IntClass>* load,OpTypes<IntClass>::loadZExt)
		{
			auto memoryValue = irBuilder.CreateLoad(compileAddress(load->address,load->offset,load->isFarAddress,load->memoryType),true);
			memoryValue->setAlignment(1<<load->alignmentLog2);
			return irBuilder.CreateZExt(memoryValue,asLLVMType(type));
		}
		DispatchResult visitLoad(TypeId type,const Load<IntClass>* load,OpTypes<IntClass>::loadSExt)
		{
			auto memoryValue = irBuilder.CreateLoad(compileAddress(load->address,load->offset,load->isFarAddress,load->memoryType),true);
			memoryValue->setAlignment(1<<load->alignmentLog2);
			return irBuilder.CreateSExt(memoryValue,asLLVMType(type));
		}
		template<typename Class>
		DispatchResult visitStore(const Store<Class>* store)
		{
			auto address = compileAddress(store->address,store->offset,store->isFarAddress,store->memoryType);
			auto value = dispatch(*this,store->value);
			auto llvmStore = irBuilder.CreateStore(value,address,true);
			llvmStore->setAlignment(1<<store->alignmentLog2);
			return value;
		}
		DispatchResult visitStore(const Store<IntClass>* store)
		{
			auto address = compileAddress(store->address,store->offset,store->isFarAddress,store->memoryType);
			auto value = dispatch(*this,store->value);
			llvm::Value* memoryValue = value;
			if(store->value.type != store->memoryType)
			{
				assert(isTypeClass(store->memoryType,TypeClassId::Int));
				memoryValue = irBuilder.CreateTrunc(value,asLLVMType(store->memoryType));
			}
			irBuilder.CreateStore(memoryValue,address,true);
			return value;
		}

		DispatchResult visitCall(TypeId type,const Call* call,OpTypes<AnyClass>::callDirect)
		{
			auto calledFunction = astModule->functions[call->functionIndex];
			assert(calledFunction->type.returnType == type);
			return compileCall(calledFunction->type,moduleIR.functions[call->functionIndex],call->parameters);
		}
		DispatchResult visitCall(TypeId type,const Call* call,OpTypes<AnyClass>::callImport)
		{
			auto astFunctionImport = astModule->functionImports[call->functionIndex];
			assert(astFunctionImport.type.returnType == type);
			auto function = moduleIR.functionImportPointers[call->functionIndex];
			return compileCall(astFunctionImport.type,function,call->parameters);
		}
		DispatchResult visitCallIndirect(TypeId type,const CallIndirect* callIndirect)
		{
			auto functionTablePointer = moduleIR.functionTablePointer;
			assert(callIndirect->functionType.returnType == type);

			auto expectedSignatureIndex = moduleIR.getSignatureIndex(callIndirect->functionType);
			auto functionPointerType = asLLVMType(callIndirect->functionType)->getPointerTo()->getPointerTo();

			// Compile the function index.
			auto functionIndex = dispatch(*this,callIndirect->functionIndex,TypeId::I32);

			// Check whether the index is within the bounds of the function table.
			return compileIfElse(
				type,
				irBuilder.CreateICmpUGT(functionIndex,moduleIR.numFunctionTableEntries),
				// If the function index is larger than the function table size, trap.
				[&]
				{
					compileRuntimeIntrinsic("wavmIntrinsics.indirectCallIndexOutOfBounds",FunctionType(),{});
					irBuilder.CreateUnreachable();
					irBuilder.SetInsertPoint(unreachableBlock);
					return typedZeroConstants[(uintptr)type];
				},
				[&]
				{
					// Zero extend the function index to the pointer size.
					auto functionIndexZExt = irBuilder.CreateZExt(functionIndex,sizeof(uintptr) == 4 ? llvm::Type::getInt32Ty(context) : llvm::Type::getInt64Ty(context));

					// Load the function table entry.
					auto functionPointerPointer = irBuilder.CreateInBoundsGEP(functionTablePointer,{compileLiteral((uint32)0),functionIndexZExt,compileLiteral((uint32)0)});
					auto signatureIndexPointer = irBuilder.CreateInBoundsGEP(functionTablePointer,{compileLiteral((uint32)0),functionIndexZExt,compileLiteral((uint32)1)});
					auto signatureIndex = irBuilder.CreateLoad(signatureIndexPointer);
					auto functionPointer = irBuilder.CreateLoad(irBuilder.CreatePointerCast(functionPointerPointer,functionPointerType));

					// Check whether the function table entry has the correct signature.
					return compileIfElse(
						type,
						irBuilder.CreateICmpNE(compileLiteral((uintptr)expectedSignatureIndex),signatureIndex),
						// If the signature ID doesn't match, trap.
						[&]
						{
							compileRuntimeIntrinsic("wavmIntrinsics.indirectCallSignatureMismatch",FunctionType(),{});
							irBuilder.CreateUnreachable();
							irBuilder.SetInsertPoint(unreachableBlock);
							return typedZeroConstants[(uintptr)type];
						},
						// If the signature ID does match, call the function loaded from the table.
						[&] { return compileCall(callIndirect->functionType,functionPointer,callIndirect->parameters); }
						);
				});
		}

		template<typename Class>
		DispatchResult visitConditional(TypeId type,const Conditional<Class>* ifElse,typename OpTypes<Class>::ifElse)
		{
			return compileIfElse(
				type,
				coerceI32ToBool(dispatch(*this,ifElse->condition,TypeId::I32)),
				[&] { return dispatch(*this,ifElse->trueValue,type); },
				[&] { return dispatch(*this,ifElse->falseValue,type); }
				);
		}
		template<typename Class>
		DispatchResult visitConditional(TypeId type,const Conditional<Class>* select,typename OpTypes<Class>::select)
		{
			auto trueValue = dispatch(*this,select->trueValue,type);
			auto falseValue = dispatch(*this,select->falseValue,type);
			auto condition = coerceI32ToBool(dispatch(*this,select->condition,TypeId::I32));
			return irBuilder.CreateSelect(condition,trueValue,falseValue);
		}
		template<typename Class>
		DispatchResult visitLabel(TypeId type,const Label<Class>* label)
		{
			auto labelBlock = llvm::BasicBlock::Create(context,"label",llvmFunction);

			// Create and link the context for this label's branch target into the list of in-scope contexts.
			auto outerBranchContext = branchContext;
			BranchContext endBranchContext = {label->endTarget,labelBlock,outerBranchContext,nullptr};
			branchContext = &endBranchContext;
			
			// Compile the label's value.
			auto value = dispatch(*this,label->expression,type);

			// Remove the label's branch target from the in-scope context list.
			assert(branchContext == &endBranchContext);
			branchContext = outerBranchContext;

			// Branch to the successor block.
			auto exitBlock = compileBranch(labelBlock);
			irBuilder.SetInsertPoint(labelBlock);

			// Create a phi node that merges all the possible values yielded by the label into one.
			if(type == TypeId::Void) { return voidDummy; }
			else if(!exitBlock && !endBranchContext.results) { return typedZeroConstants[(uintptr)type]; }
			else
			{
				auto phi = irBuilder.CreatePHI(asLLVMType(type),2);
				if(exitBlock) { phi->addIncoming(value,exitBlock); }
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
		DispatchResult visitLoop(TypeId type,const Loop<Class>* loop)
		{
			auto loopBlock = llvm::BasicBlock::Create(context,"loop",llvmFunction);
			auto successorBlock = llvm::BasicBlock::Create(context,"succ",llvmFunction);
			
			// Create and link the contexts for this label's branch targets into the list of in-scope contexts.
			auto outerBranchContext = branchContext;
			BranchContext continueBranchContext = {loop->continueTarget,loopBlock,outerBranchContext,nullptr};
			BranchContext breakBranchContext = {loop->breakTarget,successorBlock,&continueBranchContext,nullptr};
			branchContext = &breakBranchContext;
			
			compileBranch(loopBlock);

			irBuilder.SetInsertPoint(loopBlock);
			auto bodyValue = dispatch(*this,loop->expression,type);
			auto bodyExitBlock = compileBranch(successorBlock);
			
			// Remove the loop's branch targets from the in-scope context list.
			assert(branchContext == &breakBranchContext);
			branchContext = outerBranchContext;

			irBuilder.SetInsertPoint(successorBlock);
			if(type == TypeId::Void) { return voidDummy; }
			else if(!bodyExitBlock && !breakBranchContext.results) { return typedZeroConstants[(uintptr)type]; }
			else
			{
				auto phi = irBuilder.CreatePHI(asLLVMType(type),1);
				if(bodyExitBlock) { phi->addIncoming(bodyValue,bodyExitBlock); }
				for(auto result = breakBranchContext.results;result;result = result->next) { phi->addIncoming(result->value,result->incomingBlock); }
				return phi;
			}
		}
		BranchContext* findBranchTargetContext(const BranchTarget* branchTarget)
		{
			// Find the branch target context for this branch's target.
			auto targetContext = branchContext;
			while(targetContext && targetContext->branchTarget != branchTarget) { targetContext = targetContext->outerContext; }
			if(!targetContext) { throw; }
			return targetContext;
		}
		DispatchResult visitBranch(TypeId type,const Branch* branch)
		{
			// If the branch target has a non-void type, compile the branch's value.
			auto value = branch->value ? dispatch(*this,branch->value,branch->branchTarget->type) : voidDummy;

			// Find the branch target context for this branch's target.
			auto targetContext = findBranchTargetContext(branch->branchTarget);
	
			// Insert the branch instruction.
			auto exitBlock = compileBranch(targetContext->basicBlock);
			
			// Add the branch's value to the list of incoming values for the branch target.
			if(exitBlock) { targetContext->results = new(scopedArena) BranchResult {exitBlock,value,targetContext->results}; }

			// Set the insert point to the unreachable block.
			irBuilder.SetInsertPoint(unreachableBlock);

			return typedZeroConstants[(size_t)type];
		}
		DispatchResult visitBranchIf(const BranchIf* branchIf)
		{
			// If the branch target has a non-void type, compile the branch's value.
			auto value = branchIf->value ? dispatch(*this,branchIf->value,branchIf->branchTarget->type) : voidDummy;

			// Compile the branch's condition.
			auto condition = dispatch(*this,branchIf->condition,TypeId::I32);

			// Find the branch target context for this branch's target.
			auto targetContext = findBranchTargetContext(branchIf->branchTarget);
	
			// Insert the branch instruction.
			auto successorBlock = llvm::BasicBlock::Create(context,"branchIfSucc",llvmFunction);
			auto exitBlock = compileCondBranch(coerceI32ToBool(condition),targetContext->basicBlock,successorBlock);
			
			// Add the branch's value to the list of incoming values for the branch target.
			if(exitBlock) { targetContext->results = new(scopedArena) BranchResult {exitBlock,value,targetContext->results}; }

			// Set the insert point to the unreachable block.
			irBuilder.SetInsertPoint(successorBlock);

			return typedZeroConstants[(size_t)TypeId::Void];
		}
		DispatchResult visitBranchTable(TypeId type,const BranchTable* branchTable)
		{
			// If the branch target has a non-void type, compile the branch's value.
			auto value = branchTable->value ? dispatch(*this,branchTable->value) : voidDummy;

			auto index = dispatch(*this,branchTable->index);

			auto exitBlock = irBuilder.GetInsertBlock();
			if(exitBlock != unreachableBlock)
			{
				// Look up the default branch target context and add the branch's value to the list of incoming values.
				auto defaultContext = findBranchTargetContext(branchTable->defaultTarget);
				defaultContext->results = new(scopedArena) BranchResult {exitBlock,value,defaultContext->results};

				// Create the switch instruction.
				auto switchInstruction = irBuilder.CreateSwitch(index,defaultContext->basicBlock,(unsigned int)branchTable->numTableTargets);

				for(uintptr tableIndex = 0;tableIndex < branchTable->numTableTargets;++tableIndex)
				{
					// Find the branch target context for this branch's target.
					auto targetContext = findBranchTargetContext(branchTable->tableTargets[tableIndex]);
			
					// Create a literal int that matches the type of the table index.
					llvm::ConstantInt* caseIndex;
					switch(branchTable->index.type)
					{
					case TypeId::I8: caseIndex = compileLiteral((uint8)tableIndex); break;
					case TypeId::I16: caseIndex = compileLiteral((uint16)tableIndex); break;
					case TypeId::I32: caseIndex = compileLiteral((uint32)tableIndex); break;
					case TypeId::I64: caseIndex = compileLiteral((uint64)tableIndex); break;
					default: throw;
					}

					// Add the case to the switch instruction.
					switchInstruction->addCase(caseIndex,targetContext->basicBlock);

					// Add the branch's value to the list of incoming values for the branch target.
					targetContext->results = new(scopedArena) BranchResult {exitBlock,value,targetContext->results};
				}

				// Set the insert point to the unreachable block.
				irBuilder.SetInsertPoint(unreachableBlock);
			}
		
			return typedZeroConstants[(size_t)type];
		}
		DispatchResult visitReturn(TypeId type,const Return* ret)
		{
			auto returnValue = ret->value ? dispatch(*this,ret->value,astFunction->type.returnType) : voidDummy;

			if(irBuilder.GetInsertBlock() != unreachableBlock)
			{
				if(astFunction->type.returnType == TypeId::Void) { irBuilder.CreateRetVoid(); }
				else { irBuilder.CreateRet(returnValue); }
			
				// Set the insert point to the unreachable block.
				irBuilder.SetInsertPoint(unreachableBlock);
			}

			return typedZeroConstants[(size_t)type];
		}
		DispatchResult visitUnreachable(TypeId type,const Unreachable* unreachable)
		{
			if(irBuilder.GetInsertBlock() != unreachableBlock)
			{
				compileRuntimeIntrinsic("wavmIntrinsics.unreachableTrap",FunctionType(),{});
				irBuilder.CreateUnreachable();
				irBuilder.SetInsertPoint(unreachableBlock);
			}
			return typedZeroConstants[(size_t)type];
		}

		DispatchResult visitNop(const Nop*)
		{
			return voidDummy;
		}
		DispatchResult visitDiscardResult(const DiscardResult* discardResult)
		{
			dispatch(*this,discardResult->expression);
			return voidDummy;
		}
		
		DispatchResult compileLLVMIntrinsic(llvm::Intrinsic::ID intrinsicId,llvm::Value* firstOperand)
		{
			auto operandType = firstOperand->getType();
			auto intrinsic = getLLVMIntrinsic({operandType},intrinsicId);
			return irBuilder.CreateCall(intrinsic,firstOperand);
		}
		DispatchResult compileLLVMIntrinsic(llvm::Intrinsic::ID intrinsicId,llvm::Value* firstOperand,llvm::Value* secondOperand)
		{
			auto intrinsic = getLLVMIntrinsic({firstOperand->getType()},intrinsicId);
			return irBuilder.CreateCall(intrinsic,llvm::ArrayRef<llvm::Value*>({firstOperand,secondOperand}));
		}

		DispatchResult compileRuntimeIntrinsic(const char* intrinsicName,const FunctionType& functionType,const std::initializer_list<llvm::Value*>& args)
		{
			auto decoratedIntrinsicName = Intrinsics::getDecoratedFunctionName(intrinsicName,functionType);
			auto intrinsic = moduleIR.llvmModule->getGlobalVariable(decoratedIntrinsicName);
			if(!intrinsic)
			{
				intrinsic = new llvm::GlobalVariable(*moduleIR.llvmModule,asLLVMType(functionType),true,llvm::GlobalValue::ExternalLinkage,nullptr,decoratedIntrinsicName);
			}
			return irBuilder.CreateCall(intrinsic,llvm::ArrayRef<llvm::Value*>(args.begin(),args.end()));
		}
		
		llvm::Value* compileIntAbs(llvm::Value* operand)
		{
			auto mask = irBuilder.CreateAShr(operand,operand->getType()->getScalarSizeInBits() - 1);
			return irBuilder.CreateXor(irBuilder.CreateAdd(operand,mask),mask);
		}

		template<typename NonZeroThunk>
		llvm::Value* trapDivideByZero(TypeId type,llvm::Value* divisor,NonZeroThunk nonZeroThunk)
		{
			return compileIfElse(
				type,
				irBuilder.CreateICmpEQ(divisor,typedZeroConstants[(uintptr)type]),
				[&]
				{
					compileRuntimeIntrinsic("wavmIntrinsics.divideByZeroTrap",FunctionType(),{});
					irBuilder.CreateUnreachable();
					irBuilder.SetInsertPoint(unreachableBlock);
					return typedZeroConstants[(uintptr)type];
				},
				nonZeroThunk
				);
		}

		llvm::Value* compileSRem(TypeId type,llvm::Value* left,llvm::Value* right)
		{
			// LLVM's srem has undefined behavior where WebAssembly's rem_s defines that it should not trap if the corresponding
			// division would overflow a signed integer. To avoid this case, we just branch if the srem(INT_MAX,-1) case that overflows
			// is detected.
			
			llvm::Value* intMin = type == TypeId::I32 ? compileLiteral((uint32)INT_MIN) : compileLiteral((uint64)INT64_MIN);
			llvm::Value* negativeOne = type == TypeId::I32 ? compileLiteral((uint32)-1) : compileLiteral((uint64)-1);
			llvm::Value* zero = typedZeroConstants[(size_t)type];

			return compileIfElse(
				type,
				irBuilder.CreateAnd(
					irBuilder.CreateICmpEQ(left,intMin),
					irBuilder.CreateICmpEQ(right,negativeOne)
					),
				[&] { return zero; },
				[&] { return trapDivideByZero(type,right,[&] { return irBuilder.CreateSRem(left,right); }); }
				);
		}

		llvm::Value* compileShiftCountMask(TypeId type,llvm::Value* shiftCount)
		{
			// LLVM's shifts have undefined behavior where WebAssembly specifies that the shift count will wrap numbers
			// grather than the bit count of the operands. This matches x86's native shift instructions, but explicitly mask
			// the shift count anyway to support other platforms, and ensure the optimizer doesn't take advantage of the UB.
			auto bitsMinusOne = irBuilder.CreateZExt(compileLiteral((uint8)(getTypeBitWidth(type) - 1)),llvmTypesByTypeId[(size_t)type]);
			return irBuilder.CreateAnd(shiftCount,bitsMinusOne);
		}

		llvm::Value* compileRotl(TypeId type,llvm::Value* left,llvm::Value* right)
		{
			auto bitWidthMinusRight = irBuilder.CreateSub(
				irBuilder.CreateZExt(compileLiteral((uint8)getTypeBitWidth(type)),llvmTypesByTypeId[(size_t)type]),
				right
				);
			return irBuilder.CreateOr(
				irBuilder.CreateShl(left,compileShiftCountMask(type,right)),
				irBuilder.CreateLShr(left,compileShiftCountMask(type,bitWidthMinusRight))
				);
		}
		
		llvm::Value* compileRotr(TypeId type,llvm::Value* left,llvm::Value* right)
		{
			auto bitWidthMinusRight = irBuilder.CreateSub(
				irBuilder.CreateZExt(compileLiteral((uint8)getTypeBitWidth(type)),llvmTypesByTypeId[(size_t)type]),
				right
				);
			return irBuilder.CreateOr(
				irBuilder.CreateShl(left,compileShiftCountMask(type,bitWidthMinusRight)),
				irBuilder.CreateLShr(left,compileShiftCountMask(type,right))
				);
		}

		template<typename Class,typename OpAsType> DispatchResult visitUnary(TypeId type,const Unary<Class>* unary,OpAsType);
		template<typename Class,typename OpAsType> DispatchResult visitBinary(TypeId type,const Binary<Class>* binary,OpAsType);
		template<typename Class,typename OpAsType> DispatchResult visitCast(TypeId type,const Cast<Class>* cast,OpAsType);

		#define IMPLEMENT_UNARY_OP(class,op,llvmOp) \
			DispatchResult visitUnary(TypeId type,const Unary<class>* unary,OpTypes<class>::op) \
			{ \
				auto operand = dispatch(*this,unary->operand,type); \
				return llvmOp; \
			}
		#define IMPLEMENT_BINARY_OP(class,op,llvmOp) \
			DispatchResult visitBinary(TypeId type,const Binary<class>* binary,OpTypes<class>::op) \
			{ \
				auto left = dispatch(*this,binary->left,type); \
				auto right = dispatch(*this,binary->right,type); \
				return llvmOp; \
			}
		#define IMPLEMENT_CAST_OP(class,op,llvmOp) \
			DispatchResult visitCast(TypeId type,const Cast<class>* cast,OpTypes<class>::op) \
			{ \
				auto source = dispatch(*this,cast->source); \
				auto destType = asLLVMType(type); while(!destType) {} \
				return llvmOp; \
			}
		#define IMPLEMENT_COMPARE_OP(op,llvmOp) \
			DispatchResult visitComparison(const Comparison* compare,OpTypes<IntClass>::op) \
			{ \
				auto left = dispatch(*this,compare->left,compare->operandType); \
				auto right = dispatch(*this,compare->right,compare->operandType); \
				return irBuilder.CreateZExt(llvmOp,llvmTypesByTypeId[(size_t)TypeId::I32]); \
			}

		IMPLEMENT_UNARY_OP(IntClass,neg,irBuilder.CreateNeg(operand))
		IMPLEMENT_UNARY_OP(IntClass,abs,compileIntAbs(operand))
		IMPLEMENT_UNARY_OP(IntClass,bitwiseNot,irBuilder.CreateNot(operand))
		IMPLEMENT_UNARY_OP(IntClass,clz,irBuilder.CreateCall(getLLVMIntrinsic({operand->getType()},llvm::Intrinsic::ctlz),llvm::ArrayRef<llvm::Value*>({operand,compileLiteral(false)})))
		IMPLEMENT_UNARY_OP(IntClass,ctz,irBuilder.CreateCall(getLLVMIntrinsic({operand->getType()},llvm::Intrinsic::cttz),llvm::ArrayRef<llvm::Value*>({operand,compileLiteral(false)})))
		IMPLEMENT_UNARY_OP(IntClass,popcnt,compileLLVMIntrinsic(llvm::Intrinsic::ctpop,operand))
		IMPLEMENT_BINARY_OP(IntClass,add,irBuilder.CreateAdd(left,right))
		IMPLEMENT_BINARY_OP(IntClass,sub,irBuilder.CreateSub(left,right))
		IMPLEMENT_BINARY_OP(IntClass,mul,irBuilder.CreateMul(left,right))
		IMPLEMENT_BINARY_OP(IntClass,divs,trapDivideByZero(type,right,[&] { return irBuilder.CreateSDiv(left,right); }))
		IMPLEMENT_BINARY_OP(IntClass,divu,trapDivideByZero(type,right,[&] { return irBuilder.CreateUDiv(left,right); }))
		IMPLEMENT_BINARY_OP(IntClass,rems,compileSRem(type,left,right))
		IMPLEMENT_BINARY_OP(IntClass,remu,trapDivideByZero(type,right,[&] { return irBuilder.CreateURem(left,right); }))
		IMPLEMENT_BINARY_OP(IntClass,bitwiseAnd,irBuilder.CreateAnd(left,right))
		IMPLEMENT_BINARY_OP(IntClass,bitwiseOr,irBuilder.CreateOr(left,right))
		IMPLEMENT_BINARY_OP(IntClass,bitwiseXor,irBuilder.CreateXor(left,right))
		IMPLEMENT_BINARY_OP(IntClass,shl,irBuilder.CreateShl(left,compileShiftCountMask(type,right)))
		IMPLEMENT_BINARY_OP(IntClass,shrSExt,irBuilder.CreateAShr(left,compileShiftCountMask(type,right)))
		IMPLEMENT_BINARY_OP(IntClass,shrZExt,irBuilder.CreateLShr(left,compileShiftCountMask(type,right)))
		IMPLEMENT_BINARY_OP(IntClass,rotl,compileRotl(type,left,right))
		IMPLEMENT_BINARY_OP(IntClass,rotr,compileRotr(type,left,right))
		IMPLEMENT_CAST_OP(IntClass,wrap,irBuilder.CreateTrunc(source,destType))
		IMPLEMENT_CAST_OP(IntClass,truncSignedFloat,compileRuntimeIntrinsic("wavmIntrinsics.floatToSignedInt",FunctionType(type,{cast->source.type}),{source}))
		IMPLEMENT_CAST_OP(IntClass,truncUnsignedFloat,compileRuntimeIntrinsic("wavmIntrinsics.floatToUnsignedInt",FunctionType(type,{cast->source.type}),{source}))
		IMPLEMENT_CAST_OP(IntClass,sext,irBuilder.CreateSExt(source,destType))
		IMPLEMENT_CAST_OP(IntClass,zext,irBuilder.CreateZExt(source,destType))
		IMPLEMENT_CAST_OP(IntClass,reinterpretFloat,irBuilder.CreateBitCast(source,destType))
		
		IMPLEMENT_UNARY_OP(FloatClass,neg,irBuilder.CreateFNeg(operand))
		IMPLEMENT_UNARY_OP(FloatClass,abs,compileLLVMIntrinsic(llvm::Intrinsic::fabs,operand))
		IMPLEMENT_UNARY_OP(FloatClass,ceil,compileRuntimeIntrinsic("wavmIntrinsics.floatCeil",FunctionType(type,{type}),{operand}))
		IMPLEMENT_UNARY_OP(FloatClass,floor,compileRuntimeIntrinsic("wavmIntrinsics.floatFloor",FunctionType(type,{type}),{operand}))
		IMPLEMENT_UNARY_OP(FloatClass,trunc,compileRuntimeIntrinsic("wavmIntrinsics.floatTrunc",FunctionType(type,{type}),{operand}))
		IMPLEMENT_UNARY_OP(FloatClass,nearestInt,compileRuntimeIntrinsic("wavmIntrinsics.floatNearest",FunctionType(type,{type}),{operand}))
		IMPLEMENT_UNARY_OP(FloatClass,sqrt,compileLLVMIntrinsic(llvm::Intrinsic::sqrt,operand))
		IMPLEMENT_BINARY_OP(FloatClass,add,irBuilder.CreateFAdd(left,right))
		IMPLEMENT_BINARY_OP(FloatClass,sub,irBuilder.CreateFSub(left,right))
		IMPLEMENT_BINARY_OP(FloatClass,mul,irBuilder.CreateFMul(left,right))
		IMPLEMENT_BINARY_OP(FloatClass,div,irBuilder.CreateFDiv(left,right))
		IMPLEMENT_BINARY_OP(FloatClass,rem,irBuilder.CreateFRem(left,right))
		IMPLEMENT_BINARY_OP(FloatClass,min,compileRuntimeIntrinsic("wavmIntrinsics.floatMin",FunctionType(type,{type,type}),{left,right}))
		IMPLEMENT_BINARY_OP(FloatClass,max,compileRuntimeIntrinsic("wavmIntrinsics.floatMax",FunctionType(type,{type,type}),{left,right}))
		IMPLEMENT_BINARY_OP(FloatClass,copySign,compileLLVMIntrinsic(llvm::Intrinsic::copysign,left,right))
		IMPLEMENT_CAST_OP(FloatClass,convertSignedInt,irBuilder.CreateSIToFP(source,destType))
		IMPLEMENT_CAST_OP(FloatClass,convertUnsignedInt,irBuilder.CreateUIToFP(source,destType))
		IMPLEMENT_CAST_OP(FloatClass,promote,irBuilder.CreateFPExt(source,destType))
		IMPLEMENT_CAST_OP(FloatClass,demote,irBuilder.CreateFPTrunc(source,destType))
		IMPLEMENT_CAST_OP(FloatClass,reinterpretInt,irBuilder.CreateBitCast(source,destType))

		IMPLEMENT_COMPARE_OP(eq,isTypeClass(compare->operandType,TypeClassId::Float) ? irBuilder.CreateFCmpOEQ(left,right) : irBuilder.CreateICmpEQ(left,right))
		IMPLEMENT_COMPARE_OP(ne,isTypeClass(compare->operandType,TypeClassId::Float) ? irBuilder.CreateFCmpUNE(left,right) : irBuilder.CreateICmpNE(left,right))
		IMPLEMENT_COMPARE_OP(lt,irBuilder.CreateFCmpOLT(left,right))
		IMPLEMENT_COMPARE_OP(lts,irBuilder.CreateICmpSLT(left,right))
		IMPLEMENT_COMPARE_OP(ltu,irBuilder.CreateICmpULT(left,right))
		IMPLEMENT_COMPARE_OP(le,irBuilder.CreateFCmpOLE(left,right))
		IMPLEMENT_COMPARE_OP(les,irBuilder.CreateICmpSLE(left,right))
		IMPLEMENT_COMPARE_OP(leu,irBuilder.CreateICmpULE(left,right))
		IMPLEMENT_COMPARE_OP(gt,irBuilder.CreateFCmpOGT(left,right))
		IMPLEMENT_COMPARE_OP(gts,irBuilder.CreateICmpSGT(left,right))
		IMPLEMENT_COMPARE_OP(gtu,irBuilder.CreateICmpUGT(left,right))
		IMPLEMENT_COMPARE_OP(ge,irBuilder.CreateFCmpOGE(left,right))
		IMPLEMENT_COMPARE_OP(ges,irBuilder.CreateICmpSGE(left,right))
		IMPLEMENT_COMPARE_OP(geu,irBuilder.CreateICmpUGE(left,right))

		#undef IMPLEMENT_UNARY_OP
		#undef IMPLEMENT_BINARY_OP
		#undef IMPLEMENT_CAST_OP
		#undef IMPLEMENT_COMPARE_OP
	};
	
	void EmitFunctionContext::emit()
	{
		// Create an initial basic block for the function.
		auto entryBasicBlock = llvm::BasicBlock::Create(context,"entry",llvmFunction);
		irBuilder.SetInsertPoint(entryBasicBlock);

		// Create allocas for all the locals and initialize them to zero.
		localVariablePointers = new(scopedArena) llvm::Value*[astFunction->locals.size()];
		for(uintptr localIndex = 0;localIndex < astFunction->locals.size();++localIndex)
		{
			auto localVariable = astFunction->locals[localIndex];
			localVariablePointers[localIndex] = irBuilder.CreateAlloca(asLLVMType(localVariable.type),nullptr,getLLVMName(localVariable.name));
			irBuilder.CreateStore(typedZeroConstants[(uintptr)localVariable.type],localVariablePointers[localIndex]);
		}

		// Move the function arguments into the corresponding local variable allocas.
		uintptr parameterIndex = 0;
		for(auto llvmArgIt = llvmFunction->arg_begin();llvmArgIt != llvmFunction->arg_end();++parameterIndex,++llvmArgIt)
		{
			auto localIndex = astFunction->parameterLocalIndices[parameterIndex];
			irBuilder.CreateStore((llvm::Argument*)llvmArgIt,localVariablePointers[localIndex]);
		}

		// Create and link the context for this function's return branch target into the list of in-scope contexts.
		auto returnBasicBlock = llvm::BasicBlock::Create(context,"return",llvmFunction);
		BranchContext returnBranchContext = {astFunction->returnTarget,returnBasicBlock,nullptr,nullptr};
		branchContext = &returnBranchContext;

		// Traverse the function's expressions.
		auto value = dispatch(*this,astFunction->expression,astFunction->type.returnType);

		// Branch to the return block.
		auto exitBlock = compileBranch(returnBasicBlock);
		irBuilder.SetInsertPoint(returnBasicBlock);

		// Create a phi node that merges all the possible values yielded by the function into one.
		if(astFunction->type.returnType == TypeId::Void) { irBuilder.CreateRetVoid(); }
		else if(!exitBlock && !returnBranchContext.results) { irBuilder.CreateRet(typedZeroConstants[(uintptr)astFunction->type.returnType]); }
		else
		{
			auto phi = irBuilder.CreatePHI(asLLVMType(astFunction->type.returnType),2);
			if(exitBlock) { phi->addIncoming(value,exitBlock); }
			for(auto result = returnBranchContext.results;result;result = result->next) { phi->addIncoming(result->value,result->incomingBlock); }
			irBuilder.CreateRet(phi);
		}

		// Delete the unreachable block.
		unreachableBlock->eraseFromParent();
		unreachableBlock = nullptr;
	}

	llvm::Module* emitModule(const Module* astModule)
	{
		// Create a JIT module.
		#if WAVM_TIMER_OUTPUT
		Core::Timer emitTimer;
		#endif
		ModuleIR moduleIR;

		// Create literals for the virtual memory base and mask.
		llvm::APInt instanceMemoryBaseVal = llvm::APInt(sizeof(uintptr) == 8 ? 64 : 32,reinterpret_cast<uintptr>(Runtime::instanceMemoryBase));
		moduleIR.instanceMemoryBase = llvm::Constant::getIntegerValue(llvm::Type::getInt8PtrTy(context),instanceMemoryBaseVal);
		auto instanceMemoryAddressMask = Runtime::instanceAddressSpaceMaxBytes - 1;
		moduleIR.instanceMemoryAddressMask = sizeof(uintptr) == 8 ? compileLiteral((uint64)instanceMemoryAddressMask) : compileLiteral((uint32)instanceMemoryAddressMask);

		// Create the LLVM functions.
		moduleIR.functions.resize(astModule->functions.size());
		for(uintptr functionIndex = 0;functionIndex < astModule->functions.size();++functionIndex)
		{
			auto astFunction = astModule->functions[functionIndex];
			auto llvmFunctionType = asLLVMType(astFunction->type);
			auto externalName = getExternalFunctionName(functionIndex,false);
			moduleIR.functions[functionIndex] = llvm::Function::Create(llvmFunctionType,llvm::Function::ExternalLinkage,externalName,moduleIR.llvmModule);

		}

		// Create the function import globals.
		// Make sure the import with the same decorated name use the same llvm::GlobalVariable to ensure that LLVM doesn't further mangle the name
		// to maintain uniqueness.
		moduleIR.functionImportPointers.resize(astModule->functionImports.size());
		std::map<std::string,llvm::GlobalVariable*> uniqueFunctionImportGlobals;
		for(uintptr importIndex = 0;importIndex < moduleIR.functionImportPointers.size();++importIndex)
		{
			auto functionImport = astModule->functionImports[importIndex];
			auto functionType = asLLVMType(functionImport.type);
			auto functionName = Intrinsics::getDecoratedFunctionName((std::string(functionImport.module) + "." + functionImport.name).c_str(),functionImport.type);
			auto existingImportGlobalIt = uniqueFunctionImportGlobals.find(functionName);
			if(existingImportGlobalIt != uniqueFunctionImportGlobals.end())
			{
				moduleIR.functionImportPointers[importIndex] = existingImportGlobalIt->second;
			}
			else
			{
				auto functionImportGlobal = new llvm::GlobalVariable(*moduleIR.llvmModule,functionType,true,llvm::GlobalValue::ExternalLinkage,nullptr,functionName);
				moduleIR.functionImportPointers[importIndex] = functionImportGlobal;
				uniqueFunctionImportGlobals[functionName] = functionImportGlobal;
			}
		}

		// Create the function table global.
		auto functionPointerTableElementType = llvm::StructType::get(context,{
			llvm::Type::getInt8PtrTy(context),
			sizeof(uintptr) == 8 ? llvm::Type::getInt64Ty(context) : llvm::Type::getInt32Ty(context)
			});
		auto astFunctionTable = astModule->functionTable;
		std::vector<llvm::Constant*> llvmFunctionTableElements;
		llvmFunctionTableElements.resize(astFunctionTable.numFunctions);
		for(uint32 functionIndex = 0;functionIndex < astFunctionTable.numFunctions;++functionIndex)
		{
			assert(astFunctionTable.functionIndices[functionIndex] < moduleIR.functions.size());
			auto signatureIndex = moduleIR.getSignatureIndex(astModule->functions[astFunctionTable.functionIndices[functionIndex]]->type);
			auto llvmFunction = moduleIR.functions[astFunctionTable.functionIndices[functionIndex]];
			llvmFunctionTableElements[functionIndex] = llvm::ConstantStruct::get(
				functionPointerTableElementType,
				{
					llvm::ConstantExpr::getPointerBitCastOrAddrSpaceCast(llvmFunction,llvm::Type::getInt8PtrTy(context)),
					compileLiteral((uintptr)signatureIndex)
				});
		}

		// Create a LLVM global variable that holds the array of function pointers.
		auto llvmFunctionTablePointerType = llvm::ArrayType::get(functionPointerTableElementType,llvmFunctionTableElements.size());
		auto llvmFunctionTablePointer = new llvm::GlobalVariable(
			*moduleIR.llvmModule,llvmFunctionTablePointerType,true,llvm::GlobalValue::PrivateLinkage,
			llvm::ConstantArray::get(llvmFunctionTablePointerType,llvmFunctionTableElements)
			);
		moduleIR.functionTablePointer = llvmFunctionTablePointer;
		moduleIR.numFunctionTableEntries = compileLiteral((uint32)llvmFunctionTableElements.size());

		// Compile each function in the module.
		for(uintptr functionIndex = 0;functionIndex < astModule->functions.size();++functionIndex)
		{
			EmitFunctionContext(moduleIR,astModule,functionIndex).emit();
		}

		// Create thunks for invoking each function.
		for(uintptr functionIndex = 0;functionIndex < astModule->functions.size();++functionIndex)
		{
			auto astFunction = astModule->functions[functionIndex];

			llvm::Type* const i64PointerType = llvm::Type::getInt64PtrTy(context);
			auto llvmFunctionType = llvm::FunctionType::get(llvm::Type::getVoidTy(context),llvm::ArrayRef<llvm::Type*>(&i64PointerType,(size_t)1),false);

			auto thunk = llvm::Function::Create(llvmFunctionType,llvm::Function::ExternalLinkage,getExternalFunctionName(functionIndex,true),moduleIR.llvmModule);
			auto thunkEntryBlock = llvm::BasicBlock::Create(context,"entry",thunk);
			llvm::IRBuilder<> irBuilder(thunkEntryBlock);
			
			// Load the function's arguments from an array of 64-bit values at an address provided by the caller.
			llvm::Value* argBaseAddress = (llvm::Argument*)thunk->args().begin();
			std::vector<llvm::Value*> structArgLoads;
			for(uintptr parameterIndex = 0;parameterIndex < astFunction->type.parameters.size();++parameterIndex)
			{
				structArgLoads.push_back(irBuilder.CreateLoad(
					irBuilder.CreatePointerCast(
						irBuilder.CreateInBoundsGEP(argBaseAddress,{compileLiteral((uintptr)parameterIndex)}),
						asLLVMType(astFunction->type.parameters[parameterIndex])->getPointerTo()
						)
					));
			}

			// Call the llvm function with the actual implementation.
			auto returnValue = irBuilder.CreateCall(moduleIR.functions[functionIndex],structArgLoads);

			// If the function has a return value, write it to the end of the argument array.
			if(astFunction->type.returnType != TypeId::Void)
			{
				auto llvmReturnType = asLLVMType(astFunction->type.returnType);
				irBuilder.CreateStore(
					returnValue,
					irBuilder.CreatePointerCast(
						irBuilder.CreateInBoundsGEP(argBaseAddress,{compileLiteral((uintptr)astFunction->type.parameters.size())}),
						llvmReturnType->getPointerTo()
						)
					);
			}

			irBuilder.CreateRetVoid();
		}

		#if WAVM_TIMER_OUTPUT
		std::cout << "Emitted LLVM IR for module in " << emitTimer.getMilliseconds() << "ms" << std::endl;
		#endif

		return moduleIR.llvmModule;
	}
	
	void init()
	{
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
		llvm::InitializeNativeTargetAsmParser();
		llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

		llvmTypesByTypeId[(size_t)TypeId::None] = nullptr;
		llvmTypesByTypeId[(size_t)TypeId::I8] = llvm::Type::getInt8Ty(context);
		llvmTypesByTypeId[(size_t)TypeId::I16] = llvm::Type::getInt16Ty(context);
		llvmTypesByTypeId[(size_t)TypeId::I32] = llvm::Type::getInt32Ty(context);
		llvmTypesByTypeId[(size_t)TypeId::I64] = llvm::Type::getInt64Ty(context);
		llvmTypesByTypeId[(size_t)TypeId::F32] = llvm::Type::getFloatTy(context);
		llvmTypesByTypeId[(size_t)TypeId::F64] = llvm::Type::getDoubleTy(context);
		llvmTypesByTypeId[(size_t)TypeId::Void] = llvm::Type::getVoidTy(context);
		
		// Create a null pointer constant to use as the void dummy value.
		llvm::APInt voidDummyVal = llvm::APInt(sizeof(uintptr) == 8 ? 64 : 32,0);
		voidDummy = llvm::Constant::getIntegerValue(llvm::Type::getInt8Ty(context),voidDummyVal);
		
		// Create zero constants of each type.
		typedZeroConstants[(size_t)TypeId::None] = nullptr;
		typedZeroConstants[(size_t)TypeId::I8] = compileLiteral((uint8)0);
		typedZeroConstants[(size_t)TypeId::I16] = compileLiteral((uint16)0);
		typedZeroConstants[(size_t)TypeId::I32] = compileLiteral((uint32)0);
		typedZeroConstants[(size_t)TypeId::I64] = compileLiteral((uint64)0);
		typedZeroConstants[(size_t)TypeId::F32] = compileLiteral((float32)0.0f);
		typedZeroConstants[(size_t)TypeId::F64] = compileLiteral((float64)0.0);
		typedZeroConstants[(size_t)TypeId::Void] = voidDummy;
	}
}

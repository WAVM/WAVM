#include "LLVMJIT.h"
#include "llvm/ADT/SmallVector.h"
#include "WebAssembly/Operations.h"
#include "WebAssembly/OperatorLoggingProxy.h"

#define ENABLE_LOGGING 0
#define ENABLE_FUNCTION_ENTER_EXIT_HOOKS 0

#if ENABLE_LOGGING
	#include <cstdio>
#endif

using namespace WebAssembly;

namespace LLVMJIT
{
	// The global LLVM context.
	llvm::LLVMContext& context = llvm::getGlobalContext();

	// Maps a type ID to the corresponding LLVM type.
	llvm::Type* llvmResultTypes[(size_t)ResultType::num];
	llvm::Type* llvmI8Type;
	llvm::Type* llvmI16Type;
	llvm::Type* llvmI32Type;
	llvm::Type* llvmI64Type;
	llvm::Type* llvmF32Type;
	llvm::Type* llvmF64Type;
	llvm::Type* llvmVoidType;
	llvm::Type* llvmBoolType;
	llvm::Type* llvmI8PtrType;

	// Zero constants of each type.
	llvm::Constant* typedZeroConstants[(size_t)ValueType::num];

	// Converts a WebAssembly type to a LLVM type.
	inline llvm::Type* asLLVMType(ValueType type) { return llvmResultTypes[(uintptr)asResultType(type)]; }
	inline llvm::Type* asLLVMType(ResultType type) { return llvmResultTypes[(uintptr)type]; }

	// Converts a WebAssembly function type to a LLVM type.
	llvm::FunctionType* asLLVMType(const FunctionType* functionType)
	{
		auto llvmArgTypes = (llvm::Type**)alloca(sizeof(llvm::Type*) * functionType->parameters.size());
		for(uintptr argIndex = 0;argIndex < functionType->parameters.size();++argIndex)
		{
			llvmArgTypes[argIndex] = asLLVMType(functionType->parameters[argIndex]);
		}
		auto llvmResultType = asLLVMType(functionType->ret);
		return llvm::FunctionType::get(llvmResultType,llvm::ArrayRef<llvm::Type*>(llvmArgTypes,functionType->parameters.size()),false);
	}
	
	// Overloaded functions that compile a literal value to a LLVM constant of the right type.
	inline llvm::ConstantInt* emitLiteral(uint32 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(llvmI32Type,llvm::APInt(32,(uint64)value,false)); }
	inline llvm::ConstantInt* emitLiteral(int32 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(llvmI32Type,llvm::APInt(32,(int64)value,false)); }
	inline llvm::ConstantInt* emitLiteral(uint64 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(llvmI64Type,llvm::APInt(64,value,false)); }
	inline llvm::ConstantInt* emitLiteral(int64 value) { return (llvm::ConstantInt*)llvm::ConstantInt::get(llvmI64Type,llvm::APInt(64,value,false)); }
	inline llvm::Constant* emitLiteral(float32 value) { return llvm::ConstantFP::get(context,llvm::APFloat(value)); }
	inline llvm::Constant* emitLiteral(float64 value) { return llvm::ConstantFP::get(context,llvm::APFloat(value)); }
	inline llvm::Constant* emitLiteral(bool value) { return llvm::ConstantInt::get(llvmBoolType,llvm::APInt(1,value ? 1 : 0,false)); }
	inline llvm::Constant* emitLiteralPointer(const void* pointer,llvm::Type* type)
	{
		auto pointerInt = llvm::APInt(sizeof(uintptr) == 8 ? 64 : 32,reinterpret_cast<uintptr>(pointer));
		return llvm::Constant::getIntegerValue(type,pointerInt);
	}

	// The LLVM IR for a module.
	struct EmitModuleContext
	{
		const Module& module;
		ModuleInstance* moduleInstance;

		llvm::Module* llvmModule;
		std::vector<llvm::Function*> functionDefs;
		std::vector<llvm::Constant*> importedFunctionPointers;
		std::vector<llvm::Constant*> globalPointers;
		llvm::Constant* defaultTablePointer;
		llvm::Constant* defaultTableMaxElements;
		llvm::Constant* defaultMemoryBase;
		llvm::Constant* defaultMemoryAddressMask;

		EmitModuleContext(const Module& inModule,ModuleInstance* inModuleInstance)
		: module(inModule)
		, moduleInstance(inModuleInstance)
		{}

		llvm::Module* emit();
		void emitInvokeThunk(uintptr functionIndex);
	};

	// The context used by functions involved in JITing a single AST function.
	struct EmitFunctionContext
	{
		EmitModuleContext& moduleContext;
		const Module& module;
		const Function& function;
		const FunctionType* functionType;
		uintptr functionDefIndex;
		llvm::Function* llvmFunction;
		llvm::IRBuilder<> irBuilder;

		std::vector<ValueType> localTypes;
		std::vector<llvm::Value*> localPointers;

		// Information about an in-scope branch target.
		struct ControlContext
		{
			enum class Type : uint8
			{
				function,
				block,
				ifWithoutElse,
				ifThen,
				ifElse,
				loop
			};

			Type type;
			llvm::BasicBlock* endBlock;
			llvm::PHINode* endPHI;
			llvm::BasicBlock* branchTargetBlock;
			llvm::PHINode* branchTargetPHI;
			llvm::BasicBlock* elseBlock;
			ResultType branchArgumentType;
			ResultType resultType;
			uintptr outerStackSize;
		};

		std::vector<ControlContext> controlStack;
		std::vector<llvm::Value*> stack;

		EmitFunctionContext(EmitModuleContext& inEmitModuleContext,const Module& inModule,uintptr inFunctionDefIndex)
		: moduleContext(inEmitModuleContext)
		, module(inModule)
		, function(module.functionDefs[inFunctionDefIndex])
		, functionType(module.types[module.functionDefs[inFunctionDefIndex].typeIndex])
		, functionDefIndex(inFunctionDefIndex)
		, llvmFunction(inEmitModuleContext.functionDefs[inFunctionDefIndex])
		, irBuilder(context)
		{}

		void emit();

		llvm::Value* pop()
		{
			assert(stack.size() - (controlStack.size() ? controlStack.back().outerStackSize : 0) >= 1);
			llvm::Value* result = stack.back();
			stack.pop_back();
			return result;
		}

		void popMultiple(llvm::Value** outValues,size_t num)
		{
			assert(stack.size() - (controlStack.size() ? controlStack.back().outerStackSize : 0) >= num);
			std::copy(stack.end() - num,stack.end(),outValues);
			stack.resize(stack.size() - num);
		}
		template<size_t num> void popMultiple(llvm::Value* (&outValues)[num]) { popMultiple(outValues,num); }

		llvm::Value* getTopValue() const
		{
			return stack.back();
		}

		void push(llvm::Value* value)
		{
			stack.push_back(value);
		}

		llvm::PHINode* createPHI(llvm::BasicBlock* basicBlock,ResultType type)
		{
			if(type == ResultType::none) { return nullptr; }
			else
			{
				auto originalBlock = irBuilder.GetInsertBlock();
				irBuilder.SetInsertPoint(basicBlock);
				auto phi = irBuilder.CreatePHI(asLLVMType(type),2);
				if(originalBlock) { irBuilder.SetInsertPoint(originalBlock); }
				return phi;
			}
		}

		void pushControlStack(
			ControlContext::Type type,
			ResultType branchArgumentType,
			ResultType resultType,
			llvm::BasicBlock* endBlock,
			llvm::PHINode* endPHI,
			llvm::BasicBlock* branchTargetBlock,
			llvm::PHINode* branchTargetPHI,
			llvm::BasicBlock* elseBlock = nullptr
			)
		{
			controlStack.push_back({type,endBlock,endPHI,branchTargetBlock,branchTargetPHI,elseBlock,branchArgumentType,resultType,stack.size()});
		}

		void popControlStack()
		{
			assert(controlStack.size());

			ControlContext& currentContext = controlStack.back();
			stack.resize(currentContext.outerStackSize);
			if(currentContext.type == ControlContext::Type::ifThen)
			{
				assert(currentContext.elseBlock);
				irBuilder.SetInsertPoint(currentContext.elseBlock);

				currentContext.type = ControlContext::Type::ifElse;
			}
			else
			{
				currentContext.endBlock->moveAfter(irBuilder.GetInsertBlock());
				irBuilder.SetInsertPoint(currentContext.endBlock);
				
				if(currentContext.endPHI)
				{
					if(currentContext.endPHI->getNumIncomingValues()) { push(currentContext.endPHI); }
					else
					{
						currentContext.endPHI->removeFromParent();
						assert(currentContext.resultType != ResultType::none);
						push(typedZeroConstants[(uintptr)asValueType(currentContext.resultType)]);
					}
				}
				controlStack.pop_back();
			}
		}
		
		ControlContext& getBranchTargetByDepth(uintptr depth)
		{
			assert(depth < controlStack.size());
			return controlStack[controlStack.size() - depth - 1];
		}
		
		void logOperator(const std::string& operatorDescription)
		{
			#if ENABLE_LOGGING
				std::string controlStackString;
				for(uintptr stackIndex = 0;stackIndex < controlStack.size();++stackIndex)
				{
					switch(controlStack[stackIndex].type)
					{
					case ControlContext::Type::function: controlStackString += "F"; break;
					case ControlContext::Type::block: controlStackString += "B"; break;
					case ControlContext::Type::ifWithoutElse: controlStackString += "I"; break;
					case ControlContext::Type::ifThen: controlStackString += "T"; break;
					case ControlContext::Type::ifElse: controlStackString += "E"; break;
					case ControlContext::Type::loop: controlStackString += "L"; break;
					default: throw;
					};
				}

				std::string stackString;
				const uintptr stackBase = controlStack.size() == 0 ? 0 : controlStack.back().outerStackSize;
				for(uintptr stackIndex = 0;stackIndex < stack.size();++stackIndex)
				{
					if(stackIndex == stackBase) { stackString += "| "; }
					{
						llvm::raw_string_ostream stackTypeStream(stackString);
						stack[stackIndex]->getType()->print(stackTypeStream,true);
					}
					stackString += " ";
				}
				if(stack.size() == stackBase) { stackString += "|"; }

				std::printf("%-50s %-50s %-50s\n",controlStackString.c_str(),operatorDescription.c_str(),stackString.c_str());
				std::fflush(stdout);
			#endif
		}

		// Operation dispatch methods.
		void unknown(Opcode opcode)
		{
			throw InstantiationException(InstantiationException::Cause::codeGenFailed);
		}
		void beginBlock(ControlStructureImm imm)
		{
			auto endBlock = llvm::BasicBlock::Create(context,"blockEnd",llvmFunction);
			auto endPHI = createPHI(endBlock,imm.resultType);
			pushControlStack(ControlContext::Type::block,imm.resultType,imm.resultType,endBlock,endPHI,endBlock,endPHI);
		}
		void beginLoop(ControlStructureImm imm)
		{
			auto loopBodyBlock = llvm::BasicBlock::Create(context,"loopBody",llvmFunction);
			auto endBlock = llvm::BasicBlock::Create(context,"loopEnd",llvmFunction);
			auto endPHI = createPHI(endBlock,imm.resultType);
			irBuilder.CreateBr(loopBodyBlock);
			irBuilder.SetInsertPoint(loopBodyBlock);
			pushControlStack(ControlContext::Type::loop,ResultType::none,imm.resultType,endBlock,endPHI,loopBodyBlock,nullptr);
		}
		void beginIf(NoImm)
		{
			auto thenBlock = llvm::BasicBlock::Create(context,"ifThen",llvmFunction);
			auto endBlock = llvm::BasicBlock::Create(context,"ifEnd",llvmFunction);
			auto condition = pop();
			irBuilder.CreateCondBr(coerceI32ToBool(condition),thenBlock,endBlock);
			irBuilder.SetInsertPoint(thenBlock);
			pushControlStack(ControlContext::Type::ifWithoutElse,ResultType::none,ResultType::none,endBlock,nullptr,endBlock,nullptr);
		}
		void beginIfElse(ControlStructureImm imm)
		{
			auto thenBlock = llvm::BasicBlock::Create(context,"ifThen",llvmFunction);
			auto elseBlock = llvm::BasicBlock::Create(context,"ifElse",llvmFunction);
			auto endBlock = llvm::BasicBlock::Create(context,"ifElseEnd",llvmFunction);
			auto endPHI = createPHI(endBlock,imm.resultType);
			auto condition = pop();
			irBuilder.CreateCondBr(coerceI32ToBool(condition),thenBlock,elseBlock);
			irBuilder.SetInsertPoint(thenBlock);
			pushControlStack(ControlContext::Type::ifThen,imm.resultType,imm.resultType,endBlock,endPHI,endBlock,endPHI,elseBlock);
		}
		void end(NoImm)
		{
			if(controlStack.back().resultType != ResultType::none)
			{
				llvm::Value* result = pop();
				controlStack.back().endPHI->addIncoming(result,irBuilder.GetInsertBlock());
			}
			irBuilder.CreateBr(controlStack.back().endBlock);
			popControlStack();
		}
		
		void ret(NoImm)
		{
			if(functionType->ret != ResultType::none)
			{
				llvm::Value* result = pop();
				controlStack[0].branchTargetPHI->addIncoming(result,irBuilder.GetInsertBlock());
			}
			irBuilder.CreateBr(controlStack[0].endBlock);
			popControlStack();
		}

		void br(BranchImm imm)
		{
			ControlContext& targetContext = getBranchTargetByDepth(imm.targetDepth);
			if(targetContext.branchArgumentType != ResultType::none)
			{
				llvm::Value* argument = pop();
				targetContext.branchTargetPHI->addIncoming(argument,irBuilder.GetInsertBlock());
			}
			irBuilder.CreateBr(targetContext.branchTargetBlock);
			popControlStack();
		}
		void br_table(BranchTableImm imm)
		{
			auto index = pop();
			
			ControlContext& defaultTargetContext = getBranchTargetByDepth(imm.defaultTargetDepth);
			const ResultType argumentType = defaultTargetContext.branchArgumentType;
			llvm::Value* argument = nullptr;
			if(argumentType != ResultType::none)
			{
				argument = pop();
				defaultTargetContext.branchTargetPHI->addIncoming(argument,irBuilder.GetInsertBlock());
			}
			
			auto llvmSwitch = irBuilder.CreateSwitch(index,defaultTargetContext.branchTargetBlock,(unsigned int)imm.targetDepths.size());

			for(uintptr targetIndex = 0;targetIndex < imm.targetDepths.size();++targetIndex)
			{
				ControlContext& targetContext = getBranchTargetByDepth(imm.targetDepths[targetIndex]);
				if(argumentType != ResultType::none) { targetContext.branchTargetPHI->addIncoming(argument,irBuilder.GetInsertBlock()); }
				llvmSwitch->addCase(emitLiteral((uint32)targetIndex),targetContext.branchTargetBlock);
			}

			popControlStack();
		}
		void br_if(BranchImm imm)
		{
			auto condition = pop();

			ControlContext& targetContext = getBranchTargetByDepth(imm.targetDepth);
			if(targetContext.branchArgumentType != ResultType::none)
			{
				llvm::Value* argument = getTopValue();
				targetContext.branchTargetPHI->addIncoming(argument,irBuilder.GetInsertBlock());
			}
			auto falseBlock = llvm::BasicBlock::Create(context,"br_ifElse",llvmFunction);
			irBuilder.CreateCondBr(coerceI32ToBool(condition),targetContext.branchTargetBlock,falseBlock);
			irBuilder.SetInsertPoint(falseBlock);
		}

		void nop(NoImm) {}
		void unreachable(NoImm)
		{
			emitRuntimeIntrinsic("wavmIntrinsics.unreachableTrap",FunctionType::get(),{});
			irBuilder.CreateUnreachable();
			popControlStack();
		}
		void drop(NoImm) { stack.pop_back(); }

		void select(NoImm)
		{
			auto condition = pop();
			auto falseValue = pop();
			auto trueValue = pop();
			push(irBuilder.CreateSelect(coerceI32ToBool(condition),trueValue,falseValue));
		}

		void get_local(GetOrSetVariableImm imm)
		{
			assert(imm.variableIndex < localPointers.size());
			push(irBuilder.CreateLoad(localPointers[imm.variableIndex]));
		}
		void set_local(GetOrSetVariableImm imm)
		{
			assert(imm.variableIndex < localPointers.size());
			auto value = pop();
			irBuilder.CreateStore(value,localPointers[imm.variableIndex]);
		}
		void tee_local(GetOrSetVariableImm imm)
		{
			assert(imm.variableIndex < localPointers.size());
			auto value = getTopValue();
			irBuilder.CreateStore(value,localPointers[imm.variableIndex]);
		}
		
		void get_global(GetOrSetVariableImm imm)
		{
			assert(imm.variableIndex < moduleContext.globalPointers.size());
			push(irBuilder.CreateLoad(moduleContext.globalPointers[imm.variableIndex]));
		}
		void set_global(GetOrSetVariableImm imm)
		{
			assert(imm.variableIndex < moduleContext.globalPointers.size());
			auto value = pop();
			irBuilder.CreateStore(value,moduleContext.globalPointers[imm.variableIndex]);
		}

		void call(CallImm imm)
		{
			llvm::Value* callee;
			const FunctionType* calleeType;
			if(imm.functionIndex < moduleContext.importedFunctionPointers.size())
			{
				assert(imm.functionIndex < moduleContext.moduleInstance->functions.size());
				callee = moduleContext.importedFunctionPointers[imm.functionIndex];
				calleeType = moduleContext.moduleInstance->functions[imm.functionIndex]->type;
			}
			else
			{
				const uintptr calleeIndex = imm.functionIndex - moduleContext.importedFunctionPointers.size();
				assert(calleeIndex < moduleContext.functionDefs.size());
				callee = moduleContext.functionDefs[calleeIndex];
				calleeType = module.types[module.functionDefs[calleeIndex].typeIndex];
			}
			auto llvmArgs = (llvm::Value**)alloca(sizeof(llvm::Value*) * calleeType->parameters.size());
			popMultiple(llvmArgs,calleeType->parameters.size());
			auto result = irBuilder.CreateCall(callee,llvm::ArrayRef<llvm::Value*>(llvmArgs,calleeType->parameters.size()));
			if(calleeType->ret != ResultType::none) { push(result); }
		}
		void call_indirect(CallIndirectImm imm)
		{
			assert(imm.typeIndex < module.types.size());
			
			auto calleeType = module.types[imm.typeIndex];
			auto functionPointerType = asLLVMType(calleeType)->getPointerTo()->getPointerTo();

			// Compile the function index.
			auto tableElementIndex = pop();
			
			// Compile the call arguments.
			auto llvmArgs = (llvm::Value**)alloca(sizeof(llvm::Value*) * calleeType->parameters.size());
			popMultiple(llvmArgs,calleeType->parameters.size());

			// Zero extend the function index to the pointer size.
			auto functionIndexZExt = irBuilder.CreateZExt(tableElementIndex,sizeof(uintptr) == 4 ? llvmI32Type : llvmI64Type);

			// Check whether the index is within the bounds of the function table.
			auto result = emitIfElse(
				"checkTableMax",
				irBuilder.CreateICmpULE(functionIndexZExt,moduleContext.defaultTableMaxElements),
				[&]
				{
					// Load the type for this table entry.
					auto functionTypePointerPointer = irBuilder.CreateInBoundsGEP(moduleContext.defaultTablePointer,{functionIndexZExt,emitLiteral((uint32)0)});
					auto functionTypePointer = irBuilder.CreateLoad(functionTypePointerPointer);
					auto llvmCalleeType = emitLiteralPointer(calleeType,llvmI8PtrType);

					// Check whether the function table entry has the correct signature.
					return emitIfElse(
						"checkIndirectCallType",
						irBuilder.CreateICmpEQ(llvmCalleeType,functionTypePointer),
						// If the function type matches, call the function loaded from the table.
						[&]
						{
							auto functionPointerPointer = irBuilder.CreateInBoundsGEP(moduleContext.defaultTablePointer,{functionIndexZExt,emitLiteral((uint32)1)});
							auto functionPointer = irBuilder.CreateLoad(irBuilder.CreatePointerCast(functionPointerPointer,functionPointerType));
							return irBuilder.CreateCall(functionPointer,llvm::ArrayRef<llvm::Value*>(llvmArgs,calleeType->parameters.size()));
						},
						// If the function type doesn't match, trap.
						[&]
						{
							emitRuntimeIntrinsic(
								"wavmIntrinsics.indirectCallSignatureMismatch",
								FunctionType::get(ResultType::none,{ValueType::i32,ValueType::i64,ValueType::i64}),
								{	tableElementIndex,
									irBuilder.CreatePtrToInt(llvmCalleeType,llvmI64Type),
									emitLiteral(reinterpret_cast<uint64>(moduleContext.moduleInstance->defaultTable))	}
								);
							irBuilder.CreateUnreachable();
							return (llvm::Value*)nullptr;
						}
						);
				},
				// If the function index is larger than the function table size, trap.
				[&]
				{
					emitRuntimeIntrinsic("wavmIntrinsics.indirectCallIndexOutOfBounds",FunctionType::get(),{});
					irBuilder.CreateUnreachable();
					return (llvm::Value*)nullptr;
				});
			
			if(calleeType->ret != ResultType::none) { push(result); }
		}

		void grow_memory(NoImm)
		{
			auto deltaNumPages = pop();
			auto previousNumPages = emitRuntimeIntrinsic(
				"wavmIntrinsics.growMemory",
				FunctionType::get(ResultType::i32,{ValueType::i32,ValueType::i64}),
				{deltaNumPages,emitLiteral(reinterpret_cast<uint64>(moduleContext.moduleInstance->defaultMemory))});
			push(previousNumPages);
		}
		void current_memory(NoImm)
		{
			auto currentNumPages = emitRuntimeIntrinsic(
				"wavmIntrinsics.currentMemory",
				FunctionType::get(ResultType::i32,{ValueType::i64}),
				{emitLiteral(reinterpret_cast<uint64>(moduleContext.moduleInstance->defaultMemory))});
			push(currentNumPages);
		}

		void error(ErrorImm imm)
		{
			throw InstantiationException(InstantiationException::Cause::codeGenFailed);
		}
		
		template<typename TrueValueThunk,typename FalseValueThunk>
		llvm::Value* emitIfElse(const char* blockBaseName,llvm::Value* booleanCondition,TrueValueThunk trueValueThunk,FalseValueThunk falseValueThunk)
		{
			auto trueBlock = llvm::BasicBlock::Create(context,llvm::Twine(blockBaseName) + "Then",llvmFunction);
			auto falseBlock = llvm::BasicBlock::Create(context,llvm::Twine(blockBaseName) + "Else",llvmFunction);
			auto endBlock = llvm::BasicBlock::Create(context,llvm::Twine(blockBaseName) + "End",llvmFunction);

			irBuilder.CreateCondBr(booleanCondition,trueBlock,falseBlock);

			irBuilder.SetInsertPoint(trueBlock);
			auto trueValue = trueValueThunk();
			auto trueExitBlock = irBuilder.GetInsertBlock();
			if(trueValue) { irBuilder.CreateBr(endBlock); }
			endBlock->moveAfter(trueExitBlock);

			irBuilder.SetInsertPoint(falseBlock);
			auto falseValue = falseValueThunk();
			auto falseExitBlock = irBuilder.GetInsertBlock();
			if(falseValue) { irBuilder.CreateBr(endBlock); }
			endBlock->moveAfter(falseExitBlock);

			irBuilder.SetInsertPoint(endBlock);
			if(!trueValue && !falseValue) { return nullptr; }
			else if(!falseValue) { return trueValue; }
			else if(!trueValue) { return falseValue; }
			else
			{
				auto phi = irBuilder.CreatePHI(trueValue->getType(),2);
				if(trueValue && trueExitBlock) { phi->addIncoming(trueValue,trueExitBlock); }
				if(falseValue && falseExitBlock) { phi->addIncoming(falseValue,falseExitBlock); }
				return phi;
			}
		}
		
		llvm::Value* coerceI32ToBool(llvm::Value* i32Value)
		{
			return irBuilder.CreateICmpNE(i32Value,typedZeroConstants[(size_t)ValueType::i32]);
		}
		llvm::Value* coerceBoolToI32(llvm::Value* boolValue)
		{
			return irBuilder.CreateZExt(boolValue,llvmI32Type);
		}
		
		llvm::Value* coerceByteIndexToPointer(llvm::Value* byteIndex,uint32 offset,llvm::Type* memoryType)
		{
			// On a 64 bit runtime, if the address is 32-bits, zext it to 64-bits.
			// This is crucial for security, as LLVM will otherwise implicitly sign extend it to 64-bits in the GEP below,
			// interpreting it as a signed offset and allowing access to memory outside the sandboxed memory range.
			// There are no 'far addresses' in a 32 bit runtime.
			llvm::Value* nativeByteIndex = sizeof(uintptr) == 4 ? byteIndex : irBuilder.CreateZExt(byteIndex,llvmI64Type);
			llvm::Value* offsetByteIndex = nativeByteIndex;
			if(offset)
			{
				auto nativeOffset = sizeof(uintptr) == 4 ? emitLiteral((uint32)offset) : irBuilder.CreateZExt(emitLiteral((uint32)offset),llvmI64Type);
				offsetByteIndex = irBuilder.CreateAdd(nativeByteIndex,nativeOffset);
			}

			// Mask the index to the address-space size.
			auto maskedByteIndex = irBuilder.CreateAnd(offsetByteIndex,moduleContext.defaultMemoryAddressMask);

			// Cast the pointer to the appropriate type.
			auto bytePointer = irBuilder.CreateInBoundsGEP(moduleContext.defaultMemoryBase,maskedByteIndex);
			return irBuilder.CreatePointerCast(bytePointer,memoryType->getPointerTo());
		}
		
		llvm::Value* compileIntAbs(llvm::Value* operand)
		{
			auto mask = irBuilder.CreateAShr(operand,operand->getType()->getScalarSizeInBits() - 1);
			return irBuilder.CreateXor(irBuilder.CreateAdd(operand,mask),mask);
		}

		template<typename NonZeroThunk>
		llvm::Value* trapDivideByZero(ValueType type,llvm::Value* divisor,NonZeroThunk nonZeroThunk)
		{
			return emitIfElse(
				"checkDivideByZero",
				irBuilder.CreateICmpNE(divisor,typedZeroConstants[(uintptr)type]),
				nonZeroThunk,
				[&]
				{
					emitRuntimeIntrinsic("wavmIntrinsics.divideByZeroTrap",FunctionType::get(),{});
					irBuilder.CreateUnreachable();
					return (llvm::Value*)nullptr;
				});
		}

		llvm::Value* emitSRem(ValueType type,llvm::Value* left,llvm::Value* right)
		{
			// LLVM's srem has undefined behavior where WebAssembly's rem_s defines that it should not trap if the corresponding
			// division would overflow a signed integer. To avoid this case, we just branch if the srem(INT_MAX,-1) case that overflows
			// is detected.
			
			llvm::Value* intMin = type == ValueType::i32 ? emitLiteral((uint32)INT_MIN) : emitLiteral((uint64)INT64_MIN);
			llvm::Value* negativeOne = type == ValueType::i32 ? emitLiteral((uint32)-1) : emitLiteral((uint64)-1);
			llvm::Value* zero = typedZeroConstants[(size_t)type];

			return emitIfElse(
				"checkSRemIntMinNegativeOne",
				irBuilder.CreateNot(irBuilder.CreateAnd(
					irBuilder.CreateICmpEQ(left,intMin),
					irBuilder.CreateICmpEQ(right,negativeOne)
					)),
				[&] { return trapDivideByZero(type,right,[&] { return irBuilder.CreateSRem(left,right); }); },
				[&] { return zero; }
				);
		}

		llvm::Value* emitShiftCountMask(ValueType type,llvm::Value* shiftCount)
		{
			// LLVM's shifts have undefined behavior where WebAssembly specifies that the shift count will wrap numbers
			// grather than the bit count of the operands. This matches x86's native shift instructions, but explicitly mask
			// the shift count anyway to support other platforms, and ensure the optimizer doesn't take advantage of the UB.
			auto bitsMinusOne = irBuilder.CreateZExt(emitLiteral((uint8)(getTypeBitWidth(type) - 1)),asLLVMType(type));
			return irBuilder.CreateAnd(shiftCount,bitsMinusOne);
		}

		llvm::Value* emitRotl(ValueType type,llvm::Value* left,llvm::Value* right)
		{
			auto bitWidthMinusRight = irBuilder.CreateSub(
				irBuilder.CreateZExt(emitLiteral(getTypeBitWidth(type)),asLLVMType(type)),
				right
				);
			return irBuilder.CreateOr(
				irBuilder.CreateShl(left,emitShiftCountMask(type,right)),
				irBuilder.CreateLShr(left,emitShiftCountMask(type,bitWidthMinusRight))
				);
		}
		
		llvm::Value* emitRotr(ValueType type,llvm::Value* left,llvm::Value* right)
		{
			auto bitWidthMinusRight = irBuilder.CreateSub(
				irBuilder.CreateZExt(emitLiteral(getTypeBitWidth(type)),asLLVMType(type)),
				right
				);
			return irBuilder.CreateOr(
				irBuilder.CreateShl(left,emitShiftCountMask(type,bitWidthMinusRight)),
				irBuilder.CreateLShr(left,emitShiftCountMask(type,right))
				);
		}

		llvm::Value* identityConversion(llvm::Value* value,llvm::Type* type) { return value; }

		llvm::Value* getLLVMIntrinsic(const std::initializer_list<llvm::Type*>& argTypes,llvm::Intrinsic::ID id)
		{
			return llvm::Intrinsic::getDeclaration(moduleContext.llvmModule,id,llvm::ArrayRef<llvm::Type*>(argTypes.begin(),argTypes.end()));
		}
		
		llvm::Value* emitRuntimeIntrinsic(const char* intrinsicName,const FunctionType* intrinsicType,const std::initializer_list<llvm::Value*>& args)
		{
			Object* intrinsicObject = Intrinsics::find(intrinsicName,intrinsicType);
			assert(intrinsicObject);
			FunctionInstance* intrinsicFunction = asFunction(intrinsicObject);
			assert(intrinsicFunction->type == intrinsicType);
			auto intrinsicFunctionPointer = emitLiteralPointer(intrinsicFunction->nativeFunction,asLLVMType(intrinsicType)->getPointerTo());
			return irBuilder.CreateCall(intrinsicFunctionPointer,llvm::ArrayRef<llvm::Value*>(args.begin(),args.end()));
		}

		#define EMIT_CONST(typeId,nativeType) void typeId##_const(LiteralImm<nativeType> imm) { push(emitLiteral(imm.value)); }
		EMIT_CONST(i32,int32) EMIT_CONST(i64,int64)
		EMIT_CONST(f32,float32) EMIT_CONST(f64,float64)

		#define EMIT_LOAD_OPCODE(valueTypeId,name,llvmMemoryType,conversionOp) void valueTypeId##_##name(LoadOrStoreImm imm) \
			{ \
				auto byteIndex = pop(); \
				auto pointer = coerceByteIndexToPointer(byteIndex,imm.offset,llvmMemoryType); \
				auto load = irBuilder.CreateLoad(pointer); \
				load->setAlignment(1<<imm.alignmentLog2); \
				load->setVolatile(true); \
				push(conversionOp(load,asLLVMType(ValueType::valueTypeId))); \
			}
		#define EMIT_STORE_OPCODE(valueTypeId,name,llvmMemoryType,conversionOp) void valueTypeId##_##name(LoadOrStoreImm imm) \
			{ \
				auto value = pop(); \
				auto byteIndex = pop(); \
				auto pointer = coerceByteIndexToPointer(byteIndex,imm.offset,llvmMemoryType); \
				auto memoryValue = conversionOp(value,llvmMemoryType); \
				auto store = irBuilder.CreateStore(memoryValue,pointer); \
				store->setVolatile(true); \
				store->setAlignment(1<<imm.alignmentLog2); \
			}

		EMIT_LOAD_OPCODE(i32,load8_s,llvmI8Type,irBuilder.CreateSExt)  EMIT_LOAD_OPCODE(i32,load8_u,llvmI8Type,irBuilder.CreateZExt)
		EMIT_LOAD_OPCODE(i32,load16_s,llvmI16Type,irBuilder.CreateSExt) EMIT_LOAD_OPCODE(i32,load16_u,llvmI16Type,irBuilder.CreateZExt)
		EMIT_LOAD_OPCODE(i64,load8_s,llvmI8Type,irBuilder.CreateSExt)  EMIT_LOAD_OPCODE(i64,load8_u,llvmI8Type,irBuilder.CreateZExt)
		EMIT_LOAD_OPCODE(i64,load16_s,llvmI16Type,irBuilder.CreateSExt)  EMIT_LOAD_OPCODE(i64,load16_u,llvmI16Type,irBuilder.CreateZExt)
		EMIT_LOAD_OPCODE(i64,load32_s,llvmI32Type,irBuilder.CreateSExt)  EMIT_LOAD_OPCODE(i64,load32_u,llvmI32Type,irBuilder.CreateZExt)

		EMIT_LOAD_OPCODE(i32,load,llvmI32Type,identityConversion) EMIT_LOAD_OPCODE(i64,load,llvmI64Type,identityConversion)
		EMIT_LOAD_OPCODE(f32,load,llvmF32Type,identityConversion) EMIT_LOAD_OPCODE(f64,load,llvmF64Type,identityConversion)

		EMIT_STORE_OPCODE(i32,store8,llvmI8Type,irBuilder.CreateTrunc) EMIT_STORE_OPCODE(i64,store8,llvmI8Type,irBuilder.CreateTrunc)
		EMIT_STORE_OPCODE(i32,store16,llvmI16Type,irBuilder.CreateTrunc) EMIT_STORE_OPCODE(i64,store16,llvmI16Type,irBuilder.CreateTrunc)
		EMIT_STORE_OPCODE(i32,store,llvmI32Type,irBuilder.CreateTrunc) EMIT_STORE_OPCODE(i64,store32,llvmI32Type,irBuilder.CreateTrunc)
		EMIT_STORE_OPCODE(i64,store,llvmI64Type,identityConversion)
		EMIT_STORE_OPCODE(f32,store,llvmF32Type,identityConversion) EMIT_STORE_OPCODE(f64,store,llvmF64Type,identityConversion)

		#define EMIT_BINARY_OPCODE(typeId,name,emitCode) void typeId##_##name(NoImm) \
			{ \
				UNUSED const ValueType type = ValueType::typeId; \
				auto right = pop(); \
				auto left = pop(); \
				push(emitCode); \
			}
		#define EMIT_INT_BINARY_OPCODE(name,emitCode) EMIT_BINARY_OPCODE(i32,name,emitCode) EMIT_BINARY_OPCODE(i64,name,emitCode)
		#define EMIT_FP_BINARY_OPCODE(name,emitCode) EMIT_BINARY_OPCODE(f32,name,emitCode) EMIT_BINARY_OPCODE(f64,name,emitCode)

		#define EMIT_UNARY_OPCODE(typeId,name,emitCode) void typeId##_##name(NoImm) \
			{ \
				UNUSED const ValueType type = ValueType::typeId; \
				auto operand = pop(); \
				push(emitCode); \
			}
		#define EMIT_INT_UNARY_OPCODE(name,emitCode) EMIT_UNARY_OPCODE(i32,name,emitCode) EMIT_UNARY_OPCODE(i64,name,emitCode)
		#define EMIT_FP_UNARY_OPCODE(name,emitCode) EMIT_UNARY_OPCODE(f32,name,emitCode) EMIT_UNARY_OPCODE(f64,name,emitCode)

		EMIT_INT_BINARY_OPCODE(add,irBuilder.CreateAdd(left,right))
		EMIT_INT_BINARY_OPCODE(sub,irBuilder.CreateSub(left,right))
		EMIT_INT_BINARY_OPCODE(mul,irBuilder.CreateMul(left,right))
		EMIT_INT_BINARY_OPCODE(div_s,trapDivideByZero(type,right,[&] { return irBuilder.CreateSDiv(left,right); }))
		EMIT_INT_BINARY_OPCODE(div_u,trapDivideByZero(type,right,[&] { return irBuilder.CreateUDiv(left,right); }))
		EMIT_INT_BINARY_OPCODE(rem_u,trapDivideByZero(type,right,[&] { return irBuilder.CreateURem(left,right); }))
		EMIT_INT_BINARY_OPCODE(rem_s,emitSRem(type,left,right))
		EMIT_INT_BINARY_OPCODE(and,irBuilder.CreateAnd(left,right))
		EMIT_INT_BINARY_OPCODE(or,irBuilder.CreateOr(left,right))
		EMIT_INT_BINARY_OPCODE(xor,irBuilder.CreateXor(left,right))
		EMIT_INT_BINARY_OPCODE(shl,irBuilder.CreateShl(left,emitShiftCountMask(type,right)))
		EMIT_INT_BINARY_OPCODE(shr_s,irBuilder.CreateAShr(left,emitShiftCountMask(type,right)))
		EMIT_INT_BINARY_OPCODE(shr_u,irBuilder.CreateLShr(left,emitShiftCountMask(type,right)))
		EMIT_INT_BINARY_OPCODE(rotr,emitRotr(type,left,right))
		EMIT_INT_BINARY_OPCODE(rotl,emitRotl(type,left,right))
		
		EMIT_INT_BINARY_OPCODE(eq,coerceBoolToI32(irBuilder.CreateICmpEQ(left,right)))
		EMIT_INT_BINARY_OPCODE(ne,coerceBoolToI32(irBuilder.CreateICmpNE(left,right)))
		EMIT_INT_BINARY_OPCODE(lt_s,coerceBoolToI32(irBuilder.CreateICmpSLT(left,right)))
		EMIT_INT_BINARY_OPCODE(lt_u,coerceBoolToI32(irBuilder.CreateICmpULT(left,right)))
		EMIT_INT_BINARY_OPCODE(le_s,coerceBoolToI32(irBuilder.CreateICmpSLE(left,right)))
		EMIT_INT_BINARY_OPCODE(le_u,coerceBoolToI32(irBuilder.CreateICmpULE(left,right)))
		EMIT_INT_BINARY_OPCODE(gt_s,coerceBoolToI32(irBuilder.CreateICmpSGT(left,right)))
		EMIT_INT_BINARY_OPCODE(gt_u,coerceBoolToI32(irBuilder.CreateICmpUGT(left,right)))
		EMIT_INT_BINARY_OPCODE(ge_s,coerceBoolToI32(irBuilder.CreateICmpSGE(left,right)))
		EMIT_INT_BINARY_OPCODE(ge_u,coerceBoolToI32(irBuilder.CreateICmpUGE(left,right)))

		EMIT_INT_UNARY_OPCODE(clz,irBuilder.CreateCall(getLLVMIntrinsic({operand->getType()},llvm::Intrinsic::ctlz),llvm::ArrayRef<llvm::Value*>({operand,emitLiteral(false)})))
		EMIT_INT_UNARY_OPCODE(ctz,irBuilder.CreateCall(getLLVMIntrinsic({operand->getType()},llvm::Intrinsic::cttz),llvm::ArrayRef<llvm::Value*>({operand,emitLiteral(false)})))
		EMIT_INT_UNARY_OPCODE(popcnt,irBuilder.CreateCall(getLLVMIntrinsic({operand->getType()},llvm::Intrinsic::ctpop),llvm::ArrayRef<llvm::Value*>({operand})))
		EMIT_INT_UNARY_OPCODE(eqz,coerceBoolToI32(irBuilder.CreateICmpEQ(operand,typedZeroConstants[(uintptr)type])))

		EMIT_FP_BINARY_OPCODE(add,irBuilder.CreateFAdd(left,right))
		EMIT_FP_BINARY_OPCODE(sub,irBuilder.CreateFSub(left,right))
		EMIT_FP_BINARY_OPCODE(mul,irBuilder.CreateFMul(left,right))
		EMIT_FP_BINARY_OPCODE(div,irBuilder.CreateFDiv(left,right))
		EMIT_FP_BINARY_OPCODE(min,emitRuntimeIntrinsic("wavmIntrinsics.floatMin",FunctionType::get(asResultType(type),{type,type}),{left,right}))
		EMIT_FP_BINARY_OPCODE(max,emitRuntimeIntrinsic("wavmIntrinsics.floatMax",FunctionType::get(asResultType(type),{type,type}),{left,right}))
		EMIT_FP_BINARY_OPCODE(copysign,irBuilder.CreateCall(getLLVMIntrinsic({left->getType()},llvm::Intrinsic::copysign),llvm::ArrayRef<llvm::Value*>({left,right})))
			
		EMIT_FP_UNARY_OPCODE(neg,irBuilder.CreateFNeg(operand))
		EMIT_FP_UNARY_OPCODE(abs,irBuilder.CreateCall(getLLVMIntrinsic({operand->getType()},llvm::Intrinsic::fabs),llvm::ArrayRef<llvm::Value*>({operand})))
		EMIT_FP_UNARY_OPCODE(ceil,emitRuntimeIntrinsic("wavmIntrinsics.floatCeil",FunctionType::get(asResultType(type),{type}),{operand}))
		EMIT_FP_UNARY_OPCODE(floor,emitRuntimeIntrinsic("wavmIntrinsics.floatFloor",FunctionType::get(asResultType(type),{type}),{operand}))
		EMIT_FP_UNARY_OPCODE(trunc,emitRuntimeIntrinsic("wavmIntrinsics.floatTrunc",FunctionType::get(asResultType(type),{type}),{operand}))
		EMIT_FP_UNARY_OPCODE(nearest,emitRuntimeIntrinsic("wavmIntrinsics.floatNearest",FunctionType::get(asResultType(type),{type}),{operand}))
		EMIT_FP_UNARY_OPCODE(sqrt,irBuilder.CreateCall(getLLVMIntrinsic({operand->getType()},llvm::Intrinsic::sqrt),llvm::ArrayRef<llvm::Value*>({operand})))

		EMIT_FP_BINARY_OPCODE(eq,coerceBoolToI32(irBuilder.CreateFCmpOEQ(left,right)))
		EMIT_FP_BINARY_OPCODE(ne,coerceBoolToI32(irBuilder.CreateFCmpUNE(left,right)))
		EMIT_FP_BINARY_OPCODE(lt,coerceBoolToI32(irBuilder.CreateFCmpOLT(left,right)))
		EMIT_FP_BINARY_OPCODE(le,coerceBoolToI32(irBuilder.CreateFCmpOLE(left,right)))
		EMIT_FP_BINARY_OPCODE(gt,coerceBoolToI32(irBuilder.CreateFCmpOGT(left,right)))
		EMIT_FP_BINARY_OPCODE(ge,coerceBoolToI32(irBuilder.CreateFCmpOGE(left,right)))

		EMIT_INT_UNARY_OPCODE(trunc_s_f32,emitRuntimeIntrinsic("wavmIntrinsics.floatToSignedInt",FunctionType::get(asResultType(type),{ValueType::f32}),{operand}))
		EMIT_INT_UNARY_OPCODE(trunc_s_f64,emitRuntimeIntrinsic("wavmIntrinsics.floatToSignedInt",FunctionType::get(asResultType(type),{ValueType::f64}),{operand}))
		EMIT_INT_UNARY_OPCODE(trunc_u_f32,emitRuntimeIntrinsic("wavmIntrinsics.floatToUnsignedInt",FunctionType::get(asResultType(type),{ValueType::f32}),{operand}))
		EMIT_INT_UNARY_OPCODE(trunc_u_f64,emitRuntimeIntrinsic("wavmIntrinsics.floatToUnsignedInt",FunctionType::get(asResultType(type),{ValueType::f64}),{operand}))
		EMIT_UNARY_OPCODE(i32,wrap_i64,irBuilder.CreateTrunc(operand,llvmI32Type))
		EMIT_UNARY_OPCODE(i64,extend_s_i32,irBuilder.CreateSExt(operand,llvmI64Type))
		EMIT_UNARY_OPCODE(i64,extend_u_i32,irBuilder.CreateZExt(operand,llvmI64Type))

		EMIT_FP_UNARY_OPCODE(convert_s_i32,irBuilder.CreateSIToFP(operand,asLLVMType(type)))
		EMIT_FP_UNARY_OPCODE(convert_s_i64,irBuilder.CreateSIToFP(operand,asLLVMType(type)))
		EMIT_FP_UNARY_OPCODE(convert_u_i32,irBuilder.CreateUIToFP(operand,asLLVMType(type)))
		EMIT_FP_UNARY_OPCODE(convert_u_i64,irBuilder.CreateUIToFP(operand,asLLVMType(type)))

		EMIT_UNARY_OPCODE(f32,demote_f64,irBuilder.CreateFPTrunc(operand,llvmF32Type))
		EMIT_UNARY_OPCODE(f64,promote_f32,irBuilder.CreateFPExt(operand,llvmF64Type))
		EMIT_UNARY_OPCODE(f32,reinterpret_i32,irBuilder.CreateBitCast(operand,llvmF32Type))
		EMIT_UNARY_OPCODE(f64,reinterpret_i64,irBuilder.CreateBitCast(operand,llvmF64Type))
		EMIT_UNARY_OPCODE(i32,reinterpret_f32,irBuilder.CreateBitCast(operand,llvmI32Type))
		EMIT_UNARY_OPCODE(i64,reinterpret_f64,irBuilder.CreateBitCast(operand,llvmI64Type))
	};

	void EmitFunctionContext::emit()
	{
		// Create the return basic block, and push the root control context for the function.
		auto returnBlock = llvm::BasicBlock::Create(context,"return",llvmFunction);
		auto returnPHI = createPHI(returnBlock,functionType->ret);
		pushControlStack(ControlContext::Type::function,functionType->ret,functionType->ret,returnBlock,returnPHI,returnBlock,returnPHI);

		// Create an initial basic block for the function.
		auto entryBasicBlock = llvm::BasicBlock::Create(context,"entry",llvmFunction);
		irBuilder.SetInsertPoint(entryBasicBlock);

		// If enabled, emit a call to the WAVM function enter hook (for debugging).
		#if ENABLE_FUNCTION_ENTER_EXIT_HOOKS
			emitRuntimeIntrinsic(
				"wavmIntrinsics.debugEnterFunction",
				FunctionType::get(ResultType::none,{ValueType::i64}),
				{emitLiteral(reinterpret_cast<uint64>(moduleContext.moduleInstance->functionDefs[functionDefIndex]))}
				);
		#endif

		// Create allocas for all the locals and parameters.
		localTypes = functionType->parameters;
		localTypes.insert(localTypes.end(),function.nonParameterLocalTypes.begin(),function.nonParameterLocalTypes.end());
		localPointers.resize(localTypes.size());
		auto llvmArgIt = llvmFunction->arg_begin();
		for(uintptr localIndex = 0;localIndex < localTypes.size();++localIndex)
		{
			llvm::Twine localName = 
				//functionDefIndex < module.disassemblyInfo.functions.size() && localIndex < module.disassemblyInfo.functions[functionDefIndex].localNames.size()
				//? llvm::Twine("local") + llvm::Twine(localIndex) + "_" + module.disassemblyInfo.functions[functionDefIndex].localNames[localIndex]
				//: "";
				"";
			auto localType = localTypes[localIndex];
			localPointers[localIndex] = irBuilder.CreateAlloca(asLLVMType(localType),nullptr,localName);

			if(localIndex < functionType->parameters.size())
			{
				// Copy the parameter value into the local that stores it.
				irBuilder.CreateStore((llvm::Argument*)llvmArgIt,localPointers[localIndex]);
				++llvmArgIt;
			}
			else
			{
				// Initialize non-parameter locals to zero.
				irBuilder.CreateStore(typedZeroConstants[(uintptr)localType],localPointers[localIndex]);
			}
		}

		// Decode the WebAssembly opcodes and emit LLVM IR for them.
		Serialization::InputStream codeStream(module.code.data() + function.code.offset,function.code.numBytes);
		OperationDecoder decoder(codeStream);
		#if ENABLE_LOGGING
			OperatorLoggingProxy<EmitFunctionContext> loggingProxy(module,*this);
			while(decoder && controlStack.size()) { decoder.decodeOp(loggingProxy); };
		#else
			while(decoder && controlStack.size()) { decoder.decodeOp(*this); };
		#endif
		assert(irBuilder.GetInsertBlock() == returnBlock);
		
		// If enabled, emit a call to the WAVM function enter hook (for debugging).
		#if ENABLE_FUNCTION_ENTER_EXIT_HOOKS
			emitRuntimeIntrinsic(
				"wavmIntrinsics.debugExitFunction",
				FunctionType::get(ResultType::none,{ValueType::i64}),
				{emitLiteral(reinterpret_cast<uint64>(moduleContext.moduleInstance->functionDefs[functionDefIndex]))}
				);
		#endif

		// Emit the function return.
		if(functionType->ret == ResultType::none) { irBuilder.CreateRetVoid(); }
		else { irBuilder.CreateRet(pop()); }
	}

	llvm::Module* EmitModuleContext::emit()
	{
		#if WAVM_TIMER_OUTPUT
		Core::Timer emitTimer;
		#endif
		
		llvmModule = new llvm::Module("",context);

		// Create literals for the default memory base and mask.
		if(moduleInstance->defaultMemory)
		{
			defaultMemoryBase = emitLiteralPointer(moduleInstance->defaultMemory->baseAddress,llvmI8PtrType);
			auto defaultMemoryAddressMaskValue = (moduleInstance->defaultMemory->maxPages << WebAssembly::numBytesPerPageLog2) - 1;
			defaultMemoryAddressMask = sizeof(uintptr) == 8 ? emitLiteral((uint64)defaultMemoryAddressMaskValue) : emitLiteral((uint32)defaultMemoryAddressMaskValue);
		}
		else { defaultMemoryBase = defaultMemoryAddressMask = nullptr; }

		// Set up the LLVM values used to access the global table.
		if(moduleInstance->defaultTable)
		{
			auto tableElementType = llvm::StructType::get(context,{
				llvmI8PtrType,
				llvmI8PtrType
				});
			defaultTablePointer = emitLiteralPointer(moduleInstance->defaultTable->baseAddress,tableElementType->getPointerTo());
			defaultTableMaxElements = emitLiteral(uintptr((moduleInstance->defaultTable->maxPlatformPages << Platform::getPageSizeLog2()) / sizeof(Table::Element)));
		}
		else
		{
			defaultTablePointer = defaultTableMaxElements = nullptr;
		}

		// Create LLVM pointer constants for the module's imported functions.
		for(uintptr functionIndex = 0;functionIndex < moduleInstance->functions.size() - module.functionDefs.size();++functionIndex)
		{
			const FunctionInstance* functionInstance = moduleInstance->functions[functionIndex];
			importedFunctionPointers.push_back(emitLiteralPointer(functionInstance->nativeFunction,asLLVMType(functionInstance->type)->getPointerTo()));
		}

		// Create LLVM pointer constants for the module's globals.
		for(auto global : moduleInstance->globals)
		{ globalPointers.push_back(emitLiteralPointer(&global->value,asLLVMType(global->type.valueType)->getPointerTo())); }
		
		// Create the LLVM functions.
		functionDefs.resize(module.functionDefs.size());
		for(uintptr functionDefIndex = 0;functionDefIndex < module.functionDefs.size();++functionDefIndex)
		{
			const Function& function = module.functionDefs[functionDefIndex];
			const FunctionType* functionType = module.types[function.typeIndex];
			auto llvmFunctionType = asLLVMType(functionType);
			auto externalName = getExternalFunctionName(moduleInstance,importedFunctionPointers.size() + functionDefIndex,false);
			functionDefs[functionDefIndex] = llvm::Function::Create(llvmFunctionType,llvm::Function::ExternalLinkage,externalName,llvmModule);
		}

		// Compile each function in the module.
		for(uintptr functionDefIndex = 0;functionDefIndex < module.functionDefs.size();++functionDefIndex)
		{ EmitFunctionContext(*this,module,functionDefIndex).emit(); }

		// Create thunks for invoking each export and the start function (if it exists).
		for(auto& exportIt : module.exports)
		{
			if(exportIt.kind == ObjectKind::function)
			{
				emitInvokeThunk(exportIt.index);
			}
		}
		if(module.startFunctionIndex != UINTPTR_MAX) { emitInvokeThunk(module.startFunctionIndex); }

		#if WAVM_TIMER_OUTPUT
		std::cout << "Emitted LLVM IR for module in " << emitTimer.getMilliseconds() << "ms" << std::endl;
		#endif

		return llvmModule;
	}

	void EmitModuleContext::emitInvokeThunk(uintptr functionIndex)
	{
		// Allow this function to be called multiple times for the same function, but only create one thunk.
		auto thunkName = getExternalFunctionName(moduleInstance,functionIndex,true);
		if(llvmModule->getNamedGlobal(thunkName)) { return; }

		llvm::Value* functionValue;
		const FunctionType* functionType;
		if(functionIndex < importedFunctionPointers.size())
		{
			assert(functionIndex < moduleInstance->functions.size());
			functionValue = importedFunctionPointers[functionIndex];
			functionType = moduleInstance->functions[functionIndex]->type;

			// Don't create thunks for imported functions that already have an invoke thunk.
			if(moduleInstance->functions[functionIndex]->invokeThunk) { return; }
		}
		else
		{
			const uintptr functionDefIndex = functionIndex - importedFunctionPointers.size();
			assert(functionDefIndex < functionDefs.size());
			functionValue = functionDefs[functionDefIndex];
			functionType = module.types[module.functionDefs[functionDefIndex].typeIndex];
		}

		llvm::Type* const i64PointerType = llvmI64Type->getPointerTo();
		auto llvmFunctionType = llvm::FunctionType::get(llvmVoidType,llvm::ArrayRef<llvm::Type*>(&i64PointerType,(size_t)1),false);

		auto thunk = llvm::Function::Create(llvmFunctionType,llvm::Function::ExternalLinkage,thunkName,llvmModule);
		auto thunkEntryBlock = llvm::BasicBlock::Create(context,"entry",thunk);
		llvm::IRBuilder<> irBuilder(thunkEntryBlock);

		// Load the function's arguments from an array of 64-bit values at an address provided by the caller.
		llvm::Value* argBaseAddress = (llvm::Argument*)thunk->args().begin();
		std::vector<llvm::Value*> structArgLoads;
		for(uintptr parameterIndex = 0;parameterIndex < functionType->parameters.size();++parameterIndex)
		{
			structArgLoads.push_back(irBuilder.CreateLoad(
				irBuilder.CreatePointerCast(
					irBuilder.CreateInBoundsGEP(argBaseAddress,{emitLiteral((uintptr)parameterIndex)}),
					asLLVMType(functionType->parameters[parameterIndex])->getPointerTo()
					)
				));
		}

		// Call the llvm function with the actual implementation.
		auto returnValue = irBuilder.CreateCall(functionValue,structArgLoads);

		// If the function has a return value, write it to the end of the argument array.
		if(getArity(functionType->ret))
		{
			auto llvmResultType = asLLVMType(functionType->ret);
			irBuilder.CreateStore(
				returnValue,
				irBuilder.CreatePointerCast(
					irBuilder.CreateInBoundsGEP(argBaseAddress,{emitLiteral((uintptr)functionType->parameters.size())}),
					llvmResultType->getPointerTo()
					)
				);
		}

		irBuilder.CreateRetVoid();
	}

	llvm::Module* emitModule(const Module& module,ModuleInstance* moduleInstance)
	{
		return EmitModuleContext(module,moduleInstance).emit();
	}
	
	void init()
	{
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();
		llvm::InitializeNativeTargetAsmParser();
		llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
		
		llvmI8Type = llvm::Type::getInt8Ty(context);
		llvmI16Type = llvm::Type::getInt16Ty(context);
		llvmI32Type = llvm::Type::getInt32Ty(context);
		llvmI64Type = llvm::Type::getInt64Ty(context);
		llvmF32Type = llvm::Type::getFloatTy(context);
		llvmF64Type = llvm::Type::getDoubleTy(context);
		llvmVoidType = llvm::Type::getVoidTy(context);
		llvmBoolType = llvm::Type::getInt1Ty(context);
		llvmI8PtrType = llvmI8Type->getPointerTo();

		llvmResultTypes[(size_t)ResultType::none] = llvm::Type::getVoidTy(context);
		llvmResultTypes[(size_t)ResultType::i32] = llvmI32Type;
		llvmResultTypes[(size_t)ResultType::i64] = llvmI64Type;
		llvmResultTypes[(size_t)ResultType::f32] = llvmF32Type;
		llvmResultTypes[(size_t)ResultType::f64] = llvmF64Type;

		// Create zero constants of each type.
		typedZeroConstants[(size_t)ValueType::invalid] = nullptr;
		typedZeroConstants[(size_t)ValueType::i32] = emitLiteral((uint32)0);
		typedZeroConstants[(size_t)ValueType::i64] = emitLiteral((uint64)0);
		typedZeroConstants[(size_t)ValueType::f32] = emitLiteral((float32)0.0f);
		typedZeroConstants[(size_t)ValueType::f64] = emitLiteral((float64)0.0);
	}
}
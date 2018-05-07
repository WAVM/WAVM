#include "LLVMJIT.h"
#include "llvm/ADT/SmallVector.h"
#include "Inline/Assert.h"
#include "Inline/Timing.h"
#include "IR/Operators.h"
#include "IR/OperatorPrinter.h"
#include "Logging/Logging.h"

#define ENABLE_LOGGING 0
#define ENABLE_FUNCTION_ENTER_EXIT_HOOKS 0

using namespace IR;

namespace LLVMJIT
{
	// The LLVM IR for a module.
	struct EmitModuleContext
	{
		const Module& module;
		ModuleInstance* moduleInstance;

		std::shared_ptr<llvm::Module> llvmModuleSharedPtr;
		llvm::Module* llvmModule;
		std::vector<llvm::Function*> functionDefs;
		
		std::unique_ptr<llvm::DIBuilder> diBuilder;
		llvm::DICompileUnit* diCompileUnit;
		llvm::DIFile* diModuleScope;

		llvm::DIType* diValueTypes[(Uptr)ValueType::num];

		llvm::MDNode* likelyFalseBranchWeights;
		llvm::MDNode* likelyTrueBranchWeights;
		
		llvm::Value* fpRoundingModeMetadata;
		llvm::Value* fpExceptionMetadata;

		#ifdef _WIN32
			llvm::Function* tryPrologueDummyFunction;
		#else
			llvm::Function* cxaBeginCatchFunction;
		#endif

		EmitModuleContext(const Module& inModule,ModuleInstance* inModuleInstance)
		: module(inModule)
		, moduleInstance(inModuleInstance)
		{
			llvmModuleSharedPtr = std::make_shared<llvm::Module>("",*llvmContext);
			llvmModule = llvmModuleSharedPtr.get();
			diBuilder = llvm::make_unique<llvm::DIBuilder>(*llvmModule);

			diModuleScope = diBuilder->createFile("unknown","unknown");
			diCompileUnit = diBuilder->createCompileUnit(0xffff,diModuleScope,"WAVM",true,"",0);

			diValueTypes[(Uptr)ValueType::any] = nullptr;
			diValueTypes[(Uptr)ValueType::i32] = diBuilder->createBasicType("i32",32,llvm::dwarf::DW_ATE_signed);
			diValueTypes[(Uptr)ValueType::i64] = diBuilder->createBasicType("i64",64,llvm::dwarf::DW_ATE_signed);
			diValueTypes[(Uptr)ValueType::f32] = diBuilder->createBasicType("f32",32,llvm::dwarf::DW_ATE_float);
			diValueTypes[(Uptr)ValueType::f64] = diBuilder->createBasicType("f64",64,llvm::dwarf::DW_ATE_float);
			diValueTypes[(Uptr)ValueType::v128] = diBuilder->createBasicType("v128",128,llvm::dwarf::DW_ATE_signed);
			
			auto zeroAsMetadata = llvm::ConstantAsMetadata::get(emitLiteral(I32(0)));
			auto i32MaxAsMetadata = llvm::ConstantAsMetadata::get(emitLiteral(I32(INT32_MAX)));
			likelyFalseBranchWeights = llvm::MDTuple::getDistinct(*llvmContext,{llvm::MDString::get(*llvmContext,"branch_weights"),zeroAsMetadata,i32MaxAsMetadata});
			likelyTrueBranchWeights = llvm::MDTuple::getDistinct(*llvmContext,{llvm::MDString::get(*llvmContext,"branch_weights"),i32MaxAsMetadata,zeroAsMetadata});

			fpRoundingModeMetadata = llvm::MetadataAsValue::get(*llvmContext,llvm::MDString::get(*llvmContext,"round.tonearest"));
			fpExceptionMetadata = llvm::MetadataAsValue::get(*llvmContext,llvm::MDString::get(*llvmContext,"fpexcept.strict"));

			#ifdef _WIN32
				tryPrologueDummyFunction = nullptr;
			#else
				cxaBeginCatchFunction = llvm::Function::Create(
					llvm::FunctionType::get(llvmI8PtrType,{llvmI8PtrType},false),
					llvm::GlobalValue::LinkageTypes::ExternalLinkage,
					"__cxa_begin_catch",
					llvmModule
					);
			#endif
		}

		std::shared_ptr<llvm::Module> emit();
	};

	// The context used by functions involved in JITing a single AST function.
	struct EmitFunctionContext : EmitContext
	{
		typedef void Result;

		EmitModuleContext& moduleContext;
		const Module& module;
		const FunctionDef& functionDef;
		const FunctionType* functionType;
		FunctionInstance* functionInstance;
		llvm::Function* llvmFunction;

		std::vector<llvm::Value*> localPointers;
		
		llvm::DISubprogram* diFunction;
		
		llvm::BasicBlock* localEscapeBlock;
		std::vector<llvm::Value*> pendingLocalEscapes;

		// Information about an in-scope control structure.
		struct ControlContext
		{
			enum class Type : U8
			{
				function,
				block,
				ifThen,
				ifElse,
				loop,
				try_,
				catch_
			};

			Type type;
			llvm::BasicBlock* endBlock;
			llvm::PHINode* endPHI;
			llvm::BasicBlock* elseBlock;
			ResultType resultType;
			Uptr outerStackSize;
			Uptr outerBranchTargetStackSize;
			bool isReachable;
			
			llvm::Value* outerCatchpadToken;
			llvm::BasicBlock* catchBlock;
		};

		struct BranchTarget
		{
			ResultType argumentType;
			llvm::BasicBlock* block;
			llvm::PHINode* phi;
		};

		std::vector<ControlContext> controlStack;
		std::vector<BranchTarget> branchTargetStack;
		std::vector<llvm::Value*> stack;

		EmitFunctionContext(
			EmitModuleContext& inModuleContext,
			const Module& inModule,
			const FunctionDef& inFunctionDef,
			FunctionInstance* inFunctionInstance,
			llvm::Function* inLLVMFunction)
		: EmitContext(inModuleContext.moduleInstance->defaultMemory, inModuleContext.moduleInstance->defaultTable)
		, moduleContext(inModuleContext)
		, module(inModule)
		, functionDef(inFunctionDef)
		, functionType(inModule.types[inFunctionDef.type.index])
		, functionInstance(inFunctionInstance)
		, llvmFunction(inLLVMFunction)
		, localEscapeBlock(nullptr)
		{}

		void emit();

		// Operand stack manipulation
		llvm::Value* pop()
		{
			wavmAssert(stack.size() - (controlStack.size() ? controlStack.back().outerStackSize : 0) >= 1);
			llvm::Value* result = stack.back();
			stack.pop_back();
			return result;
		}

		void popMultiple(llvm::Value** outValues,Uptr num)
		{
			wavmAssert(stack.size() - (controlStack.size() ? controlStack.back().outerStackSize : 0) >= num);
			std::copy(stack.end() - num,stack.end(),outValues);
			stack.resize(stack.size() - num);
		}

		llvm::Value* getTopValue() const
		{
			return stack.back();
		}

		void push(llvm::Value* value)
		{
			stack.push_back(value);
		}

		// Creates a PHI node for the argument of branches to a basic block.
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

		// Bitcasts a LLVM value to a canonical type for the corresponding WebAssembly type.
		// This is currently just used to map all the various vector types to a canonical type for the
		// vector width.
		llvm::Value* coerceToCanonicalType(llvm::Value* value)
		{
			return value->getType()->isVectorTy() || value->getType()->isX86_MMXTy()
				? irBuilder.CreateBitCast(value,llvmI64x2Type)
				: value;
		}

		// Debug logging.
		void logOperator(const std::string& operatorDescription)
		{
			if(ENABLE_LOGGING)
			{
				std::string controlStackString;
				for(Uptr stackIndex = 0;stackIndex < controlStack.size();++stackIndex)
				{
					if(!controlStack[stackIndex].isReachable) { controlStackString += "("; }
					switch(controlStack[stackIndex].type)
					{
					case ControlContext::Type::function: controlStackString += "F"; break;
					case ControlContext::Type::block: controlStackString += "B"; break;
					case ControlContext::Type::ifThen: controlStackString += "I"; break;
					case ControlContext::Type::ifElse: controlStackString += "E"; break;
					case ControlContext::Type::loop: controlStackString += "L"; break;
					case ControlContext::Type::try_: controlStackString += "T"; break;
					case ControlContext::Type::catch_: controlStackString += "C"; break;
					default: Errors::unreachable();
					};
					if(!controlStack[stackIndex].isReachable) { controlStackString += ")"; }
				}

				std::string stackString;
				const Uptr stackBase = controlStack.size() == 0 ? 0 : controlStack.back().outerStackSize;
				for(Uptr stackIndex = 0;stackIndex < stack.size();++stackIndex)
				{
					if(stackIndex == stackBase) { stackString += "| "; }
					{
						llvm::raw_string_ostream stackTypeStream(stackString);
						stack[stackIndex]->getType()->print(stackTypeStream,true);
					}
					stackString += " ";
				}
				if(stack.size() == stackBase) { stackString += "|"; }

				Log::printf(Log::Category::debug,"%-50s %-50s %-50s\n",controlStackString.c_str(),operatorDescription.c_str(),stackString.c_str());
			}
		}
		
		// Coerces an I32 value to an I1, and vice-versa.
		llvm::Value* coerceI32ToBool(llvm::Value* i32Value)
		{
			return irBuilder.CreateICmpNE(i32Value,typedZeroConstants[(Uptr)ValueType::i32]);
		}
		llvm::Value* coerceBoolToI32(llvm::Value* boolValue)
		{
			return irBuilder.CreateZExt(boolValue,llvmI32Type);
		}
		
		// Bounds checks and converts a memory operation I32 address operand to a LLVM pointer.
		llvm::Value* coerceByteIndexToPointer(llvm::Value* byteIndex,U32 offset,llvm::Type* memoryType)
		{
			// zext the 32-bit address to 64-bits.
			// This is crucial for security, as LLVM will otherwise implicitly sign extend it to 64-bits in the GEP below,
			// interpreting it as a signed offset and allowing access to memory outside the sandboxed memory range.
			// There are no 'far addresses' in a 32 bit runtime.
			byteIndex = irBuilder.CreateZExt(byteIndex,llvmI64Type);

			// Add the offset to the byte index.
			if(offset)
			{
				byteIndex = irBuilder.CreateAdd(byteIndex,irBuilder.CreateZExt(emitLiteral(offset),llvmI64Type));
			}

			// If HAS_64BIT_ADDRESS_SPACE, the memory has enough virtual address space allocated to
			// ensure that any 32-bit byte index + 32-bit offset will fall within the virtual address sandbox,
			// so no explicit bounds check is necessary.

			// Cast the pointer to the appropriate type.
			auto bytePointer = irBuilder.CreateInBoundsGEP(irBuilder.CreateLoad(memoryBasePointerVariable),byteIndex);
			return irBuilder.CreatePointerCast(bytePointer,memoryType->getPointerTo());
		}

		// Traps a divide-by-zero
		void trapDivideByZero(ValueType type,llvm::Value* divisor)
		{
			emitConditionalTrapIntrinsic(
				irBuilder.CreateICmpEQ(divisor,typedZeroConstants[(Uptr)type]),
				"divideByZeroOrIntegerOverflowTrap",FunctionType::get(),{});
		}

		// Traps on (x / 0) or (INT_MIN / -1).
		void trapDivideByZeroOrIntegerOverflow(ValueType type,llvm::Value* left,llvm::Value* right)
		{
			emitConditionalTrapIntrinsic(
				irBuilder.CreateOr(
					irBuilder.CreateAnd(
						irBuilder.CreateICmpEQ(left,type == ValueType::i32 ? emitLiteral((U32)INT32_MIN) : emitLiteral((U64)INT64_MIN)),
						irBuilder.CreateICmpEQ(right,type == ValueType::i32 ? emitLiteral((U32)-1) : emitLiteral((U64)-1))
						),
					irBuilder.CreateICmpEQ(right,typedZeroConstants[(Uptr)type])
					),
				"divideByZeroOrIntegerOverflowTrap",FunctionType::get(),{});
		}

		llvm::Function* getLLVMIntrinsic(llvm::ArrayRef<llvm::Type*> typeArguments,llvm::Intrinsic::ID id)
		{
			return llvm::Intrinsic::getDeclaration(
				moduleContext.llvmModule,
				id,
				typeArguments
				);
		}

		llvm::Value* callLLVMIntrinsic(
			const std::initializer_list<llvm::Type*>& typeArguments,
			llvm::Intrinsic::ID id,
			llvm::ArrayRef<llvm::Value*> arguments)
		{
			return irBuilder.CreateCall(getLLVMIntrinsic(typeArguments,id),arguments);
		}
		
		// Emits a call to a WAVM intrinsic function.
		llvm::Value* emitRuntimeIntrinsic(
			const char* intrinsicName,
			const FunctionType* intrinsicType,
			const std::initializer_list<llvm::Value*>& args)
		{
			Object* intrinsicObject = Runtime::getInstanceExport(
				moduleContext.moduleInstance->compartment->wavmIntrinsics,
				intrinsicName);
			wavmAssert(intrinsicObject);
			wavmAssert(isA(intrinsicObject,intrinsicType));
			FunctionInstance* intrinsicFunction = asFunction(intrinsicObject);
			wavmAssert(intrinsicFunction->type == intrinsicType);
			auto intrinsicFunctionPointer = emitLiteralPointer(
				intrinsicFunction->nativeFunction,
				asLLVMType(intrinsicType,intrinsicFunction->callingConvention)->getPointerTo());

			return emitCallOrInvoke(
				intrinsicFunctionPointer,
				args,
				intrinsicType,
				intrinsicFunction->callingConvention,
				getInnermostUnwindToBlock());
		}

		// A helper function to emit a conditional call to a non-returning intrinsic function.
		void emitConditionalTrapIntrinsic(llvm::Value* booleanCondition,const char* intrinsicName,const FunctionType* intrinsicType,const std::initializer_list<llvm::Value*>& args)
		{
			auto trueBlock = llvm::BasicBlock::Create(*llvmContext,llvm::Twine(intrinsicName) + "Trap",llvmFunction);
			auto endBlock = llvm::BasicBlock::Create(*llvmContext,llvm::Twine(intrinsicName) + "Skip",llvmFunction);

			irBuilder.CreateCondBr(booleanCondition,trueBlock,endBlock,moduleContext.likelyFalseBranchWeights);

			irBuilder.SetInsertPoint(trueBlock);
			emitRuntimeIntrinsic(intrinsicName,intrinsicType,args);
			irBuilder.CreateUnreachable();

			irBuilder.SetInsertPoint(endBlock);
		}

		//
		// Misc operators
		//

		void nop(NoImm) {}
		void unknown(Opcode opcode) { Errors::unreachable(); }
		
		//
		// Control structure operators
		//
		
		void pushControlStack(
			ControlContext::Type type,
			ResultType resultType,
			llvm::BasicBlock* endBlock,
			llvm::PHINode* endPHI,
			llvm::BasicBlock* elseBlock = nullptr
			)
		{
			// The unreachable operator filtering should filter out any opcodes that call pushControlStack.
			if(controlStack.size()) { errorUnless(controlStack.back().isReachable); }

			controlStack.push_back({type,endBlock,endPHI,elseBlock,resultType,stack.size(),branchTargetStack.size(),true});
		}

		void pushBranchTarget(ResultType branchArgumentType,llvm::BasicBlock* branchTargetBlock,llvm::PHINode* branchTargetPHI)
		{
			branchTargetStack.push_back({branchArgumentType,branchTargetBlock,branchTargetPHI});
		}

		void branchToEndOfControlContext()
		{
			ControlContext& currentContext = controlStack.back();

			if(currentContext.isReachable)
			{
				// If the control context expects a result, take it from the operand stack and add it to the
				// control context's end PHI.
				if(currentContext.resultType != ResultType::none)
				{
					llvm::Value* result = pop();
					currentContext.endPHI->addIncoming(coerceToCanonicalType(result),irBuilder.GetInsertBlock());
				}

				// Branch to the control context's end.
				irBuilder.CreateBr(currentContext.endBlock);
			}
			wavmAssert(stack.size() == currentContext.outerStackSize);
		}

		void block(ControlStructureImm imm)
		{
			// Create an end block+phi for the block result.
			auto endBlock = llvm::BasicBlock::Create(*llvmContext,"blockEnd",llvmFunction);
			auto endPHI = createPHI(endBlock,imm.resultType);

			// Push a control context that ends at the end block/phi.
			pushControlStack(ControlContext::Type::block,imm.resultType,endBlock,endPHI);
			
			// Push a branch target for the end block/phi.
			pushBranchTarget(imm.resultType,endBlock,endPHI);
		}
		void loop(ControlStructureImm imm)
		{
			// Create a loop block, and an end block+phi for the loop result.
			auto loopBodyBlock = llvm::BasicBlock::Create(*llvmContext,"loopBody",llvmFunction);
			auto endBlock = llvm::BasicBlock::Create(*llvmContext,"loopEnd",llvmFunction);
			auto endPHI = createPHI(endBlock,imm.resultType);
			
			// Branch to the loop body and switch the IR builder to emit there.
			irBuilder.CreateBr(loopBodyBlock);
			irBuilder.SetInsertPoint(loopBodyBlock);

			// Push a control context that ends at the end block/phi.
			pushControlStack(ControlContext::Type::loop,imm.resultType,endBlock,endPHI);
			
			// Push a branch target for the loop body start.
			pushBranchTarget(ResultType::none,loopBodyBlock,nullptr);
		}
		void if_(ControlStructureImm imm)
		{
			// Create a then block and else block for the if, and an end block+phi for the if result.
			auto thenBlock = llvm::BasicBlock::Create(*llvmContext,"ifThen",llvmFunction);
			auto elseBlock = llvm::BasicBlock::Create(*llvmContext,"ifElse",llvmFunction);
			auto endBlock = llvm::BasicBlock::Create(*llvmContext,"ifElseEnd",llvmFunction);
			auto endPHI = createPHI(endBlock,imm.resultType);

			// Pop the if condition from the operand stack.
			auto condition = pop();
			irBuilder.CreateCondBr(coerceI32ToBool(condition),thenBlock,elseBlock);
			
			// Switch the IR builder to emit the then block.
			irBuilder.SetInsertPoint(thenBlock);

			// Push an ifThen control context that ultimately ends at the end block/phi, but may
			// be terminated by an else operator that changes the control context to the else block.
			pushControlStack(ControlContext::Type::ifThen,imm.resultType,endBlock,endPHI,elseBlock);
			
			// Push a branch target for the if end.
			pushBranchTarget(imm.resultType,endBlock,endPHI);
			
		}
		void else_(NoImm imm)
		{
			wavmAssert(controlStack.size());
			ControlContext& currentContext = controlStack.back();
			
			branchToEndOfControlContext();

			// Switch the IR emitter to the else block.
			wavmAssert(currentContext.elseBlock);
			wavmAssert(currentContext.type == ControlContext::Type::ifThen);
			currentContext.elseBlock->moveAfter(irBuilder.GetInsertBlock());
			irBuilder.SetInsertPoint(currentContext.elseBlock);

			// Change the top of the control stack to an else clause.
			currentContext.type = ControlContext::Type::ifElse;
			currentContext.isReachable = true;
			currentContext.elseBlock = nullptr;
		}
		void end(NoImm)
		{
			wavmAssert(controlStack.size());
			ControlContext& currentContext = controlStack.back();

			branchToEndOfControlContext();

			if(currentContext.elseBlock)
			{
				// If this is the end of an if without an else clause, create a dummy else clause.
				currentContext.elseBlock->moveAfter(irBuilder.GetInsertBlock());
				irBuilder.SetInsertPoint(currentContext.elseBlock);
				irBuilder.CreateBr(currentContext.endBlock);
			}

			if(currentContext.type == ControlContext::Type::try_) { endTry(); }
			else if(currentContext.type == ControlContext::Type::catch_) { endCatch(); }

			// Switch the IR emitter to the end block.
			currentContext.endBlock->moveAfter(irBuilder.GetInsertBlock());
			irBuilder.SetInsertPoint(currentContext.endBlock);

			if(currentContext.endPHI)
			{
				// If the control context yields a result, take the PHI that merges all the control flow
				// to the end and push it onto the operand stack.
				if(currentContext.endPHI->getNumIncomingValues()) { push(currentContext.endPHI); }
				else
				{
					// If there weren't any incoming values for the end PHI, remove it and push a dummy value.
					currentContext.endPHI->eraseFromParent();
					wavmAssert(currentContext.resultType != ResultType::none);
					push(typedZeroConstants[(Uptr)asValueType(currentContext.resultType)]);
				}
			}

			// Pop and branch targets introduced by this control context.
			wavmAssert(currentContext.outerBranchTargetStackSize <= branchTargetStack.size());
			branchTargetStack.resize(currentContext.outerBranchTargetStackSize);

			// Pop this control context.
			controlStack.pop_back();
		}
		
		//
		// Control flow operators
		//
		
		BranchTarget& getBranchTargetByDepth(Uptr depth)
		{
			wavmAssert(depth < branchTargetStack.size());
			return branchTargetStack[branchTargetStack.size() - depth - 1];
		}
		
		// This is called after unconditional control flow to indicate that operators following it are unreachable until the control stack is popped.
		void enterUnreachable()
		{
			// Unwind the operand stack to the outer control context.
			wavmAssert(controlStack.back().outerStackSize <= stack.size());
			stack.resize(controlStack.back().outerStackSize);

			// Mark the current control context as unreachable: this will cause the outer loop to stop dispatching operators to us
			// until an else/end for the current control context is reached.
			controlStack.back().isReachable = false;
		}
		
		void br_if(BranchImm imm)
		{
			// Pop the condition from operand stack.
			auto condition = pop();

			BranchTarget& target = getBranchTargetByDepth(imm.targetDepth);
			if(target.argumentType != ResultType::none)
			{
				// Use the stack top as the branch argument (don't pop it) and add it to the target phi's incoming values.
				llvm::Value* argument = getTopValue();
				target.phi->addIncoming(coerceToCanonicalType(argument),irBuilder.GetInsertBlock());
			}

			// Create a new basic block for the case where the branch is not taken.
			auto falseBlock = llvm::BasicBlock::Create(*llvmContext,"br_ifElse",llvmFunction);

			// Emit a conditional branch to either the falseBlock or the target block.
			irBuilder.CreateCondBr(coerceI32ToBool(condition),target.block,falseBlock);

			// Resume emitting instructions in the falseBlock.
			irBuilder.SetInsertPoint(falseBlock);
		}
		
		void br(BranchImm imm)
		{
			BranchTarget& target = getBranchTargetByDepth(imm.targetDepth);
			if(target.argumentType != ResultType::none)
			{
				// Pop the branch argument from the stack and add it to the target phi's incoming values.
				llvm::Value* argument = pop();
				target.phi->addIncoming(coerceToCanonicalType(argument),irBuilder.GetInsertBlock());
			}

			// Branch to the target block.
			irBuilder.CreateBr(target.block);

			enterUnreachable();
		}
		void br_table(BranchTableImm imm)
		{
			// Pop the table index from the operand stack.
			auto index = pop();
			
			// Look up the default branch target, and assume its argument type applies to all targets.
			// (this is guaranteed by the validator)
			BranchTarget& defaultTarget = getBranchTargetByDepth(imm.defaultTargetDepth);
			const ResultType argumentType = defaultTarget.argumentType;
			llvm::Value* argument = nullptr;
			if(argumentType != ResultType::none)
			{
				// Pop the branch argument from the stack and add it to the default target phi's incoming values.
				argument = pop();
				defaultTarget.phi->addIncoming(coerceToCanonicalType(argument),irBuilder.GetInsertBlock());
			}

			// Create a LLVM switch instruction.
			wavmAssert(imm.branchTableIndex < functionDef.branchTables.size());
			const std::vector<U32>& targetDepths = functionDef.branchTables[imm.branchTableIndex];
			auto llvmSwitch = irBuilder.CreateSwitch(index,defaultTarget.block,(unsigned int)targetDepths.size());

			for(Uptr targetIndex = 0;targetIndex < targetDepths.size();++targetIndex)
			{
				BranchTarget& target = getBranchTargetByDepth(targetDepths[targetIndex]);

				// Add this target to the switch instruction.
				llvmSwitch->addCase(emitLiteral((U32)targetIndex),target.block);

				if(argumentType != ResultType::none)
				{
					// If this is the first case in the table for this branch target, add the branch argument to
					// the target phi's incoming values.
					target.phi->addIncoming(coerceToCanonicalType(argument),irBuilder.GetInsertBlock());
				}
			}

			enterUnreachable();
		}
		void return_(NoImm)
		{
			if(functionType->ret != ResultType::none)
			{
				// Pop the return value from the stack and add it to the return phi's incoming values.
				llvm::Value* result = pop();
				controlStack[0].endPHI->addIncoming(coerceToCanonicalType(result),irBuilder.GetInsertBlock());
			}

			// Branch to the return block.
			irBuilder.CreateBr(controlStack[0].endBlock);

			enterUnreachable();
		}

		void unreachable(NoImm)
		{
			// Call an intrinsic that causes a trap, and insert the LLVM unreachable terminator.
			emitRuntimeIntrinsic("unreachableTrap",FunctionType::get(ResultType::none),{});
			irBuilder.CreateUnreachable();

			enterUnreachable();
		}

		//
		// Polymorphic operators
		//

		void drop(NoImm) { stack.pop_back(); }

		void select(NoImm)
		{
			auto condition = pop();
			auto falseValue = pop();
			auto trueValue = pop();
			push(irBuilder.CreateSelect(coerceI32ToBool(condition),trueValue,falseValue));
		}

		//
		// Call operators
		//

		void call(CallImm imm)
		{
			// Map the callee function index to either an imported function pointer or a function in this module.
			llvm::Value* callee;
			const FunctionType* calleeType;
			CallingConvention callingConvention;
			if(imm.functionIndex < module.functions.imports.size())
			{
				wavmAssert(imm.functionIndex < moduleContext.moduleInstance->functions.size());
				FunctionInstance* importedCallee = moduleContext.moduleInstance->functions[imm.functionIndex];
				calleeType = importedCallee->type;
				callee = emitLiteralPointer(
					importedCallee->nativeFunction,
					asLLVMType(importedCallee->type,importedCallee->callingConvention)->getPointerTo());
				callingConvention = importedCallee->callingConvention;
			}
			else
			{
				const Uptr functionDefIndex = imm.functionIndex - module.functions.imports.size();
				wavmAssert(functionDefIndex < moduleContext.functionDefs.size());
				callee = moduleContext.functionDefs[functionDefIndex];
				calleeType = module.types[module.functions.defs[functionDefIndex].type.index];
				callingConvention = CallingConvention::wasm;
			}

			// Pop the call arguments from the operand stack.
			const Uptr numArguments = calleeType->parameters.size();
			auto llvmArgs = (llvm::Value**)alloca(sizeof(llvm::Value*) * numArguments);
			popMultiple(llvmArgs,numArguments);

			// Call the function.
			auto result = emitCallOrInvoke(
				callee,
				llvm::ArrayRef<llvm::Value*>(llvmArgs,numArguments),
				calleeType,
				callingConvention,
				getInnermostUnwindToBlock());

			// Push the result on the operand stack.
			if(calleeType->ret != ResultType::none) { push(result); }
		}
		void call_indirect(CallIndirectImm imm)
		{
			wavmAssert(imm.type.index < module.types.size());
			
			auto calleeType = module.types[imm.type.index];

			// Compile the function index.
			auto tableElementIndex = pop();

			// Pop the call arguments from the operand stack.
			const Uptr numArguments = calleeType->parameters.size();
			auto llvmArgs = (llvm::Value**)alloca(sizeof(llvm::Value*) * numArguments);
			popMultiple(llvmArgs,numArguments);

			// Zero extend the function index to the pointer size.
			auto functionIndexZExt = irBuilder.CreateZExt(tableElementIndex,sizeof(Uptr) == 4 ? llvmI32Type : llvmI64Type);

			auto tableElementType = llvm::StructType::get(*llvmContext,{
				llvmI8PtrType,
				llvmI8PtrType
				});
			auto typedTableBasePointer = irBuilder.CreatePointerCast(
				irBuilder.CreateLoad(tableBasePointerVariable),
				tableElementType->getPointerTo());

			// Load the type for this table entry.
			auto functionTypePointerPointer = irBuilder.CreateInBoundsGEP(typedTableBasePointer,{functionIndexZExt,emitLiteral((U32)0)});
			auto functionTypePointer = irBuilder.CreateLoad(functionTypePointerPointer);
			auto llvmCalleeType = emitLiteralPointer(calleeType,llvmI8PtrType);
			
			// If the function type doesn't match, trap.
			emitConditionalTrapIntrinsic(
				irBuilder.CreateICmpNE(llvmCalleeType,functionTypePointer),
				"indirectCallSignatureMismatch",
				FunctionType::get(ResultType::none,{ValueType::i32,ValueType::i64,ValueType::i64}),
				{	tableElementIndex,
					irBuilder.CreatePtrToInt(llvmCalleeType,llvmI64Type),
					emitLiteral(U64(moduleContext.moduleInstance->defaultTable->id))	}
				);

			// Call the function loaded from the table.
			auto functionPointerPointer = irBuilder.CreateInBoundsGEP(typedTableBasePointer,{functionIndexZExt,emitLiteral((U32)1)});
			auto functionPointer = loadFromUntypedPointer(
				functionPointerPointer,
				asLLVMType(calleeType,CallingConvention::wasm)->getPointerTo());
			auto result = emitCallOrInvoke(
				functionPointer,
				llvm::ArrayRef<llvm::Value*>(llvmArgs,numArguments),
				calleeType,
				CallingConvention::wasm,
				getInnermostUnwindToBlock());

			// Push the result on the operand stack.
			if(calleeType->ret != ResultType::none) { push(result); }
		}
		
		//
		// Local/global operators
		//

		void get_local(GetOrSetVariableImm<false> imm)
		{
			wavmAssert(imm.variableIndex < localPointers.size());
			push(irBuilder.CreateLoad(localPointers[imm.variableIndex]));
		}
		void set_local(GetOrSetVariableImm<false> imm)
		{
			wavmAssert(imm.variableIndex < localPointers.size());
			auto value = irBuilder.CreateBitCast(pop(),localPointers[imm.variableIndex]->getType()->getPointerElementType());
			irBuilder.CreateStore(value,localPointers[imm.variableIndex]);
		}
		void tee_local(GetOrSetVariableImm<false> imm)
		{
			wavmAssert(imm.variableIndex < localPointers.size());
			auto value = irBuilder.CreateBitCast(getTopValue(),localPointers[imm.variableIndex]->getType()->getPointerElementType());
			irBuilder.CreateStore(value,localPointers[imm.variableIndex]);
		}
		
		void get_global(GetOrSetVariableImm<true> imm)
		{
			wavmAssert(imm.variableIndex < moduleContext.moduleInstance->globals.size());
			GlobalInstance* global = moduleContext.moduleInstance->globals[imm.variableIndex];
			llvm::Type* llvmValueType = asLLVMType(global->type.valueType);
			
			if(global->type.isMutable)
			{
				llvm::Value* globalPointer = irBuilder.CreatePointerCast(
					irBuilder.CreateInBoundsGEP(
						irBuilder.CreateLoad(contextPointerVariable),
						{ emitLiteral(Uptr(offsetof(ContextRuntimeData,globalData)) + global->mutableDataOffset) }),
						llvmValueType->getPointerTo());
				push(irBuilder.CreateLoad(globalPointer));
			}
			else
			{
				if(getTypeByteWidth(global->type.valueType) > sizeof(void*))
				{
					llvm::Value* globalPointer = emitLiteralPointer(&global->initialValue,llvmValueType->getPointerTo());
					push(irBuilder.CreateLoad(globalPointer));
				}
				else
				{
					llvm::Constant* immutableValue;
					switch(global->type.valueType)
					{
					case ValueType::i32: immutableValue = emitLiteral(global->initialValue.i32); break;
					case ValueType::i64: immutableValue = emitLiteral(global->initialValue.i64); break;
					case ValueType::f32: immutableValue = emitLiteral(global->initialValue.f32); break;
					case ValueType::f64: immutableValue = emitLiteral(global->initialValue.f64); break;
					default: Errors::unreachable();
					};
					push(immutableValue);
				}
			}
		}
		void set_global(GetOrSetVariableImm<true> imm)
		{
			wavmAssert(imm.variableIndex < moduleContext.moduleInstance->globals.size());
			GlobalInstance* global = moduleContext.moduleInstance->globals[imm.variableIndex];
			wavmAssert(global->type.isMutable);
			llvm::Type* llvmValueType = asLLVMType(global->type.valueType);
			llvm::Value* globalPointer = irBuilder.CreatePointerCast(
				irBuilder.CreateInBoundsGEP(
					irBuilder.CreateLoad(contextPointerVariable),
					{ emitLiteral(Uptr(offsetof(ContextRuntimeData,globalData) + global->mutableDataOffset)) }),
				llvmValueType->getPointerTo());
			auto value = irBuilder.CreateBitCast(pop(),llvmValueType);
			irBuilder.CreateStore(value,globalPointer);
		}

		//
		// Memory size operators
		// These just call out to wavmIntrinsics.growMemory/currentMemory, passing a pointer to the default memory for the module.
		//

		void memory_grow(MemoryImm)
		{
			llvm::Value* deltaNumPages = pop();
			llvm::Value* previousNumPages = emitRuntimeIntrinsic(
				"growMemory",
				FunctionType::get(ResultType::i32,{ValueType::i32,ValueType::i64}),
				{deltaNumPages,emitLiteral(U64(moduleContext.moduleInstance->defaultMemory->id))});
			push(previousNumPages);
		}
		void memory_size(MemoryImm)
		{
			llvm::Value* currentNumPages = emitRuntimeIntrinsic(
				"currentMemory",
				FunctionType::get(ResultType::i32,{ValueType::i64}),
				{ emitLiteral(U64(moduleContext.moduleInstance->defaultMemory->id))});
			push(currentNumPages);
		}

		//
		// Constant operators
		//

		#define EMIT_CONST(typeId,nativeType) void typeId##_const(LiteralImm<nativeType> imm) { push(emitLiteral(imm.value)); }
		EMIT_CONST(i32,I32) EMIT_CONST(i64,I64)
		EMIT_CONST(f32,F32) EMIT_CONST(f64,F64)

		//
		// Load/store operators
		//

		#define EMIT_LOAD_OP(valueTypeId,name,llvmMemoryType,naturalAlignmentLog2,conversionOp) \
			void valueTypeId##_##name(LoadOrStoreImm<naturalAlignmentLog2> imm) \
			{ \
				auto byteIndex = pop(); \
				auto pointer = coerceByteIndexToPointer(byteIndex,imm.offset,llvmMemoryType); \
				auto load = irBuilder.CreateLoad(pointer); \
				load->setAlignment(1<<imm.alignmentLog2); \
				load->setVolatile(true); \
				push(conversionOp(load,asLLVMType(ValueType::valueTypeId))); \
			}
		#define EMIT_STORE_OP(valueTypeId,name,llvmMemoryType,naturalAlignmentLog2,conversionOp) \
			void valueTypeId##_##name(LoadOrStoreImm<naturalAlignmentLog2> imm) \
			{ \
				auto value = pop(); \
				auto byteIndex = pop(); \
				auto pointer = coerceByteIndexToPointer(byteIndex,imm.offset,llvmMemoryType); \
				auto memoryValue = conversionOp(value,llvmMemoryType); \
				auto store = irBuilder.CreateStore(memoryValue,pointer); \
				store->setVolatile(true); \
				store->setAlignment(1<<imm.alignmentLog2); \
			}
			
		llvm::Value* identityConversion(llvm::Value* value,llvm::Type* type) { return value; }

		EMIT_LOAD_OP(i32,load8_s,llvmI8Type,0,irBuilder.CreateSExt)  EMIT_LOAD_OP(i32,load8_u,llvmI8Type,0,irBuilder.CreateZExt)
		EMIT_LOAD_OP(i32,load16_s,llvmI16Type,1,irBuilder.CreateSExt) EMIT_LOAD_OP(i32,load16_u,llvmI16Type,1,irBuilder.CreateZExt)
		EMIT_LOAD_OP(i64,load8_s,llvmI8Type,0,irBuilder.CreateSExt)  EMIT_LOAD_OP(i64,load8_u,llvmI8Type,0,irBuilder.CreateZExt)
		EMIT_LOAD_OP(i64,load16_s,llvmI16Type,1,irBuilder.CreateSExt)  EMIT_LOAD_OP(i64,load16_u,llvmI16Type,1,irBuilder.CreateZExt)
		EMIT_LOAD_OP(i64,load32_s,llvmI32Type,2,irBuilder.CreateSExt)  EMIT_LOAD_OP(i64,load32_u,llvmI32Type,2,irBuilder.CreateZExt)

		EMIT_LOAD_OP(i32,load,llvmI32Type,2,identityConversion) EMIT_LOAD_OP(i64,load,llvmI64Type,3,identityConversion)
		EMIT_LOAD_OP(f32,load,llvmF32Type,2,identityConversion) EMIT_LOAD_OP(f64,load,llvmF64Type,3,identityConversion)

		EMIT_STORE_OP(i32,store8,llvmI8Type,0,irBuilder.CreateTrunc) EMIT_STORE_OP(i64,store8,llvmI8Type,0,irBuilder.CreateTrunc)
		EMIT_STORE_OP(i32,store16,llvmI16Type,1,irBuilder.CreateTrunc) EMIT_STORE_OP(i64,store16,llvmI16Type,1,irBuilder.CreateTrunc)
		EMIT_STORE_OP(i32,store,llvmI32Type,2,irBuilder.CreateTrunc) EMIT_STORE_OP(i64,store32,llvmI32Type,2,irBuilder.CreateTrunc)
		EMIT_STORE_OP(i64,store,llvmI64Type,3,identityConversion)
		EMIT_STORE_OP(f32,store,llvmF32Type,2,identityConversion) EMIT_STORE_OP(f64,store,llvmF64Type,3,identityConversion)

		//
		// Numeric operator macros
		//

		#define EMIT_BINARY_OP(typeId,name,emitCode) void typeId##_##name(NoImm) \
			{ \
				const ValueType type = ValueType::typeId; SUPPRESS_UNUSED(type); \
				auto right = pop(); \
				auto left = pop(); \
				push(emitCode); \
			}
		#define EMIT_INT_BINARY_OP(name,emitCode) EMIT_BINARY_OP(i32,name,emitCode) EMIT_BINARY_OP(i64,name,emitCode)
		#define EMIT_FP_BINARY_OP(name,emitCode) EMIT_BINARY_OP(f32,name,emitCode) EMIT_BINARY_OP(f64,name,emitCode)

		#define EMIT_UNARY_OP(typeId,name,emitCode) void typeId##_##name(NoImm) \
			{ \
				const ValueType type = ValueType::typeId; SUPPRESS_UNUSED(type); \
				auto operand = pop(); \
				push(emitCode); \
			}
		#define EMIT_INT_UNARY_OP(name,emitCode) EMIT_UNARY_OP(i32,name,emitCode) EMIT_UNARY_OP(i64,name,emitCode)
		#define EMIT_FP_UNARY_OP(name,emitCode) EMIT_UNARY_OP(f32,name,emitCode) EMIT_UNARY_OP(f64,name,emitCode)

		//
		// Int operators
		//

		llvm::Value* emitSRem(ValueType type,llvm::Value* left,llvm::Value* right)
		{
			// Trap if the dividend is zero.
			trapDivideByZero(type,right); 

			// LLVM's srem has undefined behavior where WebAssembly's rem_s defines that it should not trap if the corresponding
			// division would overflow a signed integer. To avoid this case, we just branch around the srem if the INT_MAX%-1 case
			// that overflows is detected.
			auto preOverflowBlock = irBuilder.GetInsertBlock();
			auto noOverflowBlock = llvm::BasicBlock::Create(*llvmContext,"sremNoOverflow",llvmFunction);
			auto endBlock = llvm::BasicBlock::Create(*llvmContext,"sremEnd",llvmFunction);
			auto noOverflow = irBuilder.CreateOr(
				irBuilder.CreateICmpNE(left,type == ValueType::i32 ? emitLiteral((U32)INT32_MIN) : emitLiteral((U64)INT64_MIN)),
				irBuilder.CreateICmpNE(right,type == ValueType::i32 ? emitLiteral((U32)-1) : emitLiteral((U64)-1))
				);
			irBuilder.CreateCondBr(noOverflow,noOverflowBlock,endBlock,moduleContext.likelyTrueBranchWeights);

			irBuilder.SetInsertPoint(noOverflowBlock);
			auto noOverflowValue = irBuilder.CreateSRem(left,right);
			irBuilder.CreateBr(endBlock);

			irBuilder.SetInsertPoint(endBlock);
			auto phi = irBuilder.CreatePHI(asLLVMType(type),2);
			phi->addIncoming(typedZeroConstants[(Uptr)type],preOverflowBlock);
			phi->addIncoming(noOverflowValue,noOverflowBlock);
			return phi;
		}
		
		llvm::Value* emitShiftCountMask(ValueType type,llvm::Value* shiftCount)
		{
			// LLVM's shifts have undefined behavior where WebAssembly specifies that the shift count will wrap numbers
			// grather than the bit count of the operands. This matches x86's native shift instructions, but explicitly mask
			// the shift count anyway to support other platforms, and ensure the optimizer doesn't take advantage of the UB.
			auto bitsMinusOne = irBuilder.CreateZExt(emitLiteral((U8)(getTypeBitWidth(type) - 1)),asLLVMType(type));
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

		EMIT_INT_BINARY_OP(add,irBuilder.CreateAdd(left,right))
		EMIT_INT_BINARY_OP(sub,irBuilder.CreateSub(left,right))
		EMIT_INT_BINARY_OP(mul,irBuilder.CreateMul(left,right))
		EMIT_INT_BINARY_OP(and,irBuilder.CreateAnd(left,right))
		EMIT_INT_BINARY_OP(or,irBuilder.CreateOr(left,right))
		EMIT_INT_BINARY_OP(xor,irBuilder.CreateXor(left,right))
		EMIT_INT_BINARY_OP(rotr,emitRotr(type,left,right))
		EMIT_INT_BINARY_OP(rotl,emitRotl(type,left,right))
			
		// Divides use trapDivideByZero to avoid the undefined behavior in LLVM's division instructions.
		EMIT_INT_BINARY_OP(div_s, (trapDivideByZeroOrIntegerOverflow(type,left,right), irBuilder.CreateSDiv(left,right)) )
		EMIT_INT_BINARY_OP(rem_s, emitSRem(type,left,right) )
		EMIT_INT_BINARY_OP(div_u, (trapDivideByZero(type,right), irBuilder.CreateUDiv(left,right)) )
		EMIT_INT_BINARY_OP(rem_u, (trapDivideByZero(type,right), irBuilder.CreateURem(left,right)) )

		// Explicitly mask the shift amount operand to the word size to avoid LLVM's undefined behavior.
		EMIT_INT_BINARY_OP(shl,irBuilder.CreateShl(left,emitShiftCountMask(type,right)))
		EMIT_INT_BINARY_OP(shr_s,irBuilder.CreateAShr(left,emitShiftCountMask(type,right)))
		EMIT_INT_BINARY_OP(shr_u,irBuilder.CreateLShr(left,emitShiftCountMask(type,right)))
		
		EMIT_INT_BINARY_OP(eq,coerceBoolToI32(irBuilder.CreateICmpEQ(left,right)))
		EMIT_INT_BINARY_OP(ne,coerceBoolToI32(irBuilder.CreateICmpNE(left,right)))
		EMIT_INT_BINARY_OP(lt_s,coerceBoolToI32(irBuilder.CreateICmpSLT(left,right)))
		EMIT_INT_BINARY_OP(lt_u,coerceBoolToI32(irBuilder.CreateICmpULT(left,right)))
		EMIT_INT_BINARY_OP(le_s,coerceBoolToI32(irBuilder.CreateICmpSLE(left,right)))
		EMIT_INT_BINARY_OP(le_u,coerceBoolToI32(irBuilder.CreateICmpULE(left,right)))
		EMIT_INT_BINARY_OP(gt_s,coerceBoolToI32(irBuilder.CreateICmpSGT(left,right)))
		EMIT_INT_BINARY_OP(gt_u,coerceBoolToI32(irBuilder.CreateICmpUGT(left,right)))
		EMIT_INT_BINARY_OP(ge_s,coerceBoolToI32(irBuilder.CreateICmpSGE(left,right)))
		EMIT_INT_BINARY_OP(ge_u,coerceBoolToI32(irBuilder.CreateICmpUGE(left,right)))

		EMIT_INT_UNARY_OP(clz,callLLVMIntrinsic({operand->getType()},llvm::Intrinsic::ctlz,{operand,emitLiteral(false)}))
		EMIT_INT_UNARY_OP(ctz,callLLVMIntrinsic({operand->getType()},llvm::Intrinsic::cttz,{operand,emitLiteral(false)}))
		EMIT_INT_UNARY_OP(popcnt,callLLVMIntrinsic({operand->getType()},llvm::Intrinsic::ctpop,{operand}))
		EMIT_INT_UNARY_OP(eqz,coerceBoolToI32(irBuilder.CreateICmpEQ(operand,typedZeroConstants[(Uptr)type])))

		//
		// FP operators
		//

		EMIT_FP_BINARY_OP(add,callLLVMIntrinsic({left->getType()},llvm::Intrinsic::experimental_constrained_fadd,{left,right,moduleContext.fpRoundingModeMetadata,moduleContext.fpExceptionMetadata}))
		EMIT_FP_BINARY_OP(sub,callLLVMIntrinsic({left->getType()},llvm::Intrinsic::experimental_constrained_fsub,{left,right,moduleContext.fpRoundingModeMetadata,moduleContext.fpExceptionMetadata}))
		EMIT_FP_BINARY_OP(mul,callLLVMIntrinsic({left->getType()},llvm::Intrinsic::experimental_constrained_fmul,{left,right,moduleContext.fpRoundingModeMetadata,moduleContext.fpExceptionMetadata}))
		EMIT_FP_BINARY_OP(div,callLLVMIntrinsic({left->getType()},llvm::Intrinsic::experimental_constrained_fdiv,{left,right,moduleContext.fpRoundingModeMetadata,moduleContext.fpExceptionMetadata}))
		EMIT_FP_BINARY_OP(copysign,callLLVMIntrinsic({left->getType()},llvm::Intrinsic::copysign,{left,right}))

		EMIT_FP_UNARY_OP(neg,irBuilder.CreateFNeg(operand))
		EMIT_FP_UNARY_OP(abs,callLLVMIntrinsic({operand->getType()},llvm::Intrinsic::fabs,{operand}))
		EMIT_FP_UNARY_OP(sqrt,callLLVMIntrinsic({operand->getType()},llvm::Intrinsic::experimental_constrained_sqrt,{operand,moduleContext.fpRoundingModeMetadata,moduleContext.fpExceptionMetadata}))

		EMIT_FP_BINARY_OP(eq,coerceBoolToI32(irBuilder.CreateFCmpOEQ(left,right)))
		EMIT_FP_BINARY_OP(ne,coerceBoolToI32(irBuilder.CreateFCmpUNE(left,right)))
		EMIT_FP_BINARY_OP(lt,coerceBoolToI32(irBuilder.CreateFCmpOLT(left,right)))
		EMIT_FP_BINARY_OP(le,coerceBoolToI32(irBuilder.CreateFCmpOLE(left,right)))
		EMIT_FP_BINARY_OP(gt,coerceBoolToI32(irBuilder.CreateFCmpOGT(left,right)))
		EMIT_FP_BINARY_OP(ge,coerceBoolToI32(irBuilder.CreateFCmpOGE(left,right)))

		EMIT_UNARY_OP(i32,wrap_i64,irBuilder.CreateTrunc(operand,llvmI32Type))
		EMIT_UNARY_OP(i64,extend_s_i32,irBuilder.CreateSExt(operand,llvmI64Type))
		EMIT_UNARY_OP(i64,extend_u_i32,irBuilder.CreateZExt(operand,llvmI64Type))

		EMIT_FP_UNARY_OP(convert_s_i32,irBuilder.CreateSIToFP(operand,asLLVMType(type)))
		EMIT_FP_UNARY_OP(convert_s_i64,irBuilder.CreateSIToFP(operand,asLLVMType(type)))
		EMIT_FP_UNARY_OP(convert_u_i32,irBuilder.CreateUIToFP(operand,asLLVMType(type)))
		EMIT_FP_UNARY_OP(convert_u_i64,irBuilder.CreateUIToFP(operand,asLLVMType(type)))

		EMIT_UNARY_OP(f32,demote_f64,irBuilder.CreateFPTrunc(operand,llvmF32Type))
		EMIT_UNARY_OP(f64,promote_f32,emitF64Promote(operand))
		EMIT_UNARY_OP(f32,reinterpret_i32,irBuilder.CreateBitCast(operand,llvmF32Type))
		EMIT_UNARY_OP(f64,reinterpret_i64,irBuilder.CreateBitCast(operand,llvmF64Type))
		EMIT_UNARY_OP(i32,reinterpret_f32,irBuilder.CreateBitCast(operand,llvmI32Type))
		EMIT_UNARY_OP(i64,reinterpret_f64,irBuilder.CreateBitCast(operand,llvmI64Type))

		llvm::Value* emitF64Promote(llvm::Value* operand)
		{
			// Emit an nop experimental.constrained.fadd intrinsic on the result of the promote to make sure
			// the promote can't be optimized away.
			llvm::Value* f64Operand = irBuilder.CreateFPExt(operand,llvmF64Type);
			return callLLVMIntrinsic(
				{llvmF64Type}, llvm::Intrinsic::experimental_constrained_fmul,
				{
					f64Operand,
					emitLiteral(F64(1.0)),
					moduleContext.fpRoundingModeMetadata,
					moduleContext.fpExceptionMetadata
				});
		}

		template<typename Float>
		llvm::Value* emitTruncFloatToInt(ValueType destType,bool isSigned,Float minBounds,Float maxBounds,llvm::Value* operand)
		{
			auto nanBlock = llvm::BasicBlock::Create(*llvmContext,"FPToInt_nan",llvmFunction);
			auto notNaNBlock = llvm::BasicBlock::Create(*llvmContext,"FPToInt_notNaN",llvmFunction);
			auto overflowBlock = llvm::BasicBlock::Create(*llvmContext,"FPToInt_overflow",llvmFunction);
			auto noOverflowBlock = llvm::BasicBlock::Create(*llvmContext,"FPToInt_noOverflow",llvmFunction);

			auto isNaN = irBuilder.CreateFCmpUNO(operand,operand);
			irBuilder.CreateCondBr(isNaN,nanBlock,notNaNBlock,moduleContext.likelyFalseBranchWeights);

			irBuilder.SetInsertPoint(nanBlock);
			emitRuntimeIntrinsic("invalidFloatOperationTrap",FunctionType::get(),{});
			irBuilder.CreateUnreachable();

			irBuilder.SetInsertPoint(notNaNBlock);
			auto isOverflow = irBuilder.CreateOr(
				irBuilder.CreateFCmpOGE(operand,emitLiteral(maxBounds)),
				irBuilder.CreateFCmpOLE(operand,emitLiteral(minBounds))
				);
			irBuilder.CreateCondBr(isOverflow,overflowBlock,noOverflowBlock,moduleContext.likelyFalseBranchWeights);

			irBuilder.SetInsertPoint(overflowBlock);
			emitRuntimeIntrinsic("divideByZeroOrIntegerOverflowTrap",FunctionType::get(),{});
			irBuilder.CreateUnreachable();

			irBuilder.SetInsertPoint(noOverflowBlock);
			return isSigned
				? irBuilder.CreateFPToSI(operand,asLLVMType(destType))
				: irBuilder.CreateFPToUI(operand,asLLVMType(destType));
		}

		// We want the widest floating point bounds that can't be truncated to an integer.
		// This isn't simply the min/max integer values converted to float, but the next greater(or lesser) float that would be truncated
		// to an integer out of range of the target type.

		EMIT_UNARY_OP(i32,trunc_s_f32,emitTruncFloatToInt<F32>(type,true,-2147483904.0f,2147483648.0f,operand))
		EMIT_UNARY_OP(i32,trunc_s_f64,emitTruncFloatToInt<F64>(type,true,-2147483649.0, 2147483648.0,operand))
		EMIT_UNARY_OP(i32,trunc_u_f32,emitTruncFloatToInt<F32>(type,false,-1.0f,4294967296.0f,operand))
		EMIT_UNARY_OP(i32,trunc_u_f64,emitTruncFloatToInt<F64>(type,false,-1.0, 4294967296.0,operand))

		EMIT_UNARY_OP(i64,trunc_s_f32,emitTruncFloatToInt<F32>(type,true,-9223373136366403584.0f,9223372036854775808.0f,operand))
		EMIT_UNARY_OP(i64,trunc_s_f64,emitTruncFloatToInt<F64>(type,true,-9223372036854777856.0, 9223372036854775808.0,operand))
		EMIT_UNARY_OP(i64,trunc_u_f32,emitTruncFloatToInt<F32>(type,false,-1.0f,18446744073709551616.0f,operand))
		EMIT_UNARY_OP(i64,trunc_u_f64,emitTruncFloatToInt<F64>(type,false,-1.0, 18446744073709551616.0,operand))

		template<typename Int,typename Float>
		llvm::Value* emitTruncFloatToIntSat(
			llvm::Type* destType,
			bool isSigned,
			Float minFloatBounds, Float maxFloatBounds, 
			Int minIntBounds, Int maxIntBounds,
			Int nanResult,
			llvm::Value* operand)
		{
			return irBuilder.CreateSelect(
				irBuilder.CreateFCmpUNO(operand,operand),
				emitLiteral(nanResult),
				irBuilder.CreateSelect(
					irBuilder.CreateFCmpOLE(operand,emitLiteral(minFloatBounds)),
					emitLiteral(minIntBounds),
					irBuilder.CreateSelect(
						irBuilder.CreateFCmpOGE(operand,emitLiteral(maxFloatBounds)),
						emitLiteral(maxIntBounds),
						isSigned
						? irBuilder.CreateFPToSI(operand,destType)
						: irBuilder.CreateFPToUI(operand,destType)
					)));
		}

		EMIT_UNARY_OP(i32,trunc_s_sat_f32,(emitTruncFloatToIntSat<I32,F32>)(llvmI32Type,true,F32(INT32_MIN),F32(INT32_MAX),INT32_MIN,INT32_MAX,U32(0),operand))
		EMIT_UNARY_OP(i32,trunc_s_sat_f64,(emitTruncFloatToIntSat<I32,F64>)(llvmI32Type,true,F64(INT32_MIN),F64(INT32_MAX),INT32_MIN,INT32_MAX,U32(0),operand))
		EMIT_UNARY_OP(i32,trunc_u_sat_f32,(emitTruncFloatToIntSat<I32,F32>)(llvmI32Type,false,0.0f,F32(UINT32_MAX),U32(0),UINT32_MAX,U32(0),operand))
		EMIT_UNARY_OP(i32,trunc_u_sat_f64,(emitTruncFloatToIntSat<I32,F64>)(llvmI32Type,false,0.0,F64(UINT32_MAX),U32(0),UINT32_MAX,U32(0),operand))
		EMIT_UNARY_OP(i64,trunc_s_sat_f32,(emitTruncFloatToIntSat<I64,F32>)(llvmI64Type,true,F32(INT64_MIN),F32(INT64_MAX),INT64_MIN,INT64_MAX,U64(0),operand))
		EMIT_UNARY_OP(i64,trunc_s_sat_f64,(emitTruncFloatToIntSat<I64,F64>)(llvmI64Type,true,F64(INT64_MIN),F64(INT64_MAX),INT64_MIN,INT64_MAX,U64(0),operand))
		EMIT_UNARY_OP(i64,trunc_u_sat_f32,(emitTruncFloatToIntSat<I64,F32>)(llvmI64Type,false,0.0f,F32(UINT64_MAX),U64(0),UINT64_MAX,U64(0),operand))
		EMIT_UNARY_OP(i64,trunc_u_sat_f64,(emitTruncFloatToIntSat<I64,F64>)(llvmI64Type,false,0.0,F64(UINT64_MAX),U64(0),UINT64_MAX,U64(0),operand))

		// These operations don't match LLVM's semantics exactly, so just call out to C++ implementations.
		EMIT_FP_BINARY_OP(min   ,emitRuntimeIntrinsic(type == ValueType::f32 ? "f32.min"     : "f64.min",FunctionType::get(asResultType(type),{type,type}),{left,right}))
		EMIT_FP_BINARY_OP(max   ,emitRuntimeIntrinsic(type == ValueType::f32 ? "f32.max"     : "f64.max",FunctionType::get(asResultType(type),{type,type}),{left,right}))
		EMIT_FP_UNARY_OP(ceil   ,emitRuntimeIntrinsic(type == ValueType::f32 ? "f32.ceil"    : "f64.ceil",FunctionType::get(asResultType(type),{type}),{operand}))
		EMIT_FP_UNARY_OP(floor  ,emitRuntimeIntrinsic(type == ValueType::f32 ? "f32.floor"   : "f64.floor",FunctionType::get(asResultType(type),{type}),{operand}))
		EMIT_FP_UNARY_OP(trunc  ,emitRuntimeIntrinsic(type == ValueType::f32 ? "f32.trunc"   : "f64.trunc",FunctionType::get(asResultType(type),{type}),{operand}))
		EMIT_FP_UNARY_OP(nearest,emitRuntimeIntrinsic(type == ValueType::f32 ? "f32.nearest" : "f64.nearest",FunctionType::get(asResultType(type),{type}),{operand}))

		llvm::Value* emitAnyTrue(llvm::Value* boolVector)
		{
			const Uptr numLanes = boolVector->getType()->getVectorNumElements();
			llvm::Value* result = nullptr;
			for(Uptr laneIndex = 0;laneIndex < numLanes;++laneIndex)
			{
				llvm::Value* scalar = irBuilder.CreateExtractElement(boolVector,laneIndex);
				result = result ? irBuilder.CreateOr(result,scalar) : scalar;
			}
			return result;
		}
		llvm::Value* emitAllTrue(llvm::Value* boolVector)
		{
			const Uptr numLanes = boolVector->getType()->getVectorNumElements();
			llvm::Value* result = nullptr;
			for(Uptr laneIndex = 0;laneIndex < numLanes;++laneIndex)
			{
				llvm::Value* scalar = irBuilder.CreateExtractElement(boolVector,laneIndex);
				result = result ? irBuilder.CreateAnd(result,scalar) : scalar;
			}
			return result;
		}

		#define EMIT_SIMD_SPLAT(vectorType,coerceScalar,numLanes) \
			void vectorType##_splat(NoImm) \
			{ \
				auto scalar = pop(); \
				push(irBuilder.CreateVectorSplat(numLanes,coerceScalar)); \
			}
		EMIT_SIMD_SPLAT(i8x16,irBuilder.CreateTrunc(scalar,llvmI8Type),16)
		EMIT_SIMD_SPLAT(i16x8,irBuilder.CreateTrunc(scalar,llvmI16Type),8)
		EMIT_SIMD_SPLAT(i32x4,scalar,4) 
		EMIT_SIMD_SPLAT(i64x2,scalar,2)
		EMIT_SIMD_SPLAT(f32x4,scalar,4)
		EMIT_SIMD_SPLAT(f64x2,scalar,2)

		EMIT_STORE_OP(v128,store,value->getType(),4,identityConversion)
		EMIT_LOAD_OP(v128,load,llvmI64x2Type,4,identityConversion)

		#define EMIT_SIMD_BINARY_OP(name,llvmType,emitCode) \
			void name(NoImm) \
			{ \
				auto right = irBuilder.CreateBitCast(pop(),llvmType); SUPPRESS_UNUSED(right); \
				auto left = irBuilder.CreateBitCast(pop(),llvmType); SUPPRESS_UNUSED(left); \
				push(emitCode); \
			}
		#define EMIT_SIMD_UNARY_OP(name,llvmType,emitCode) \
			void name(NoImm) \
			{ \
				auto operand = irBuilder.CreateBitCast(pop(),llvmType); SUPPRESS_UNUSED(operand); \
				push(emitCode); \
			}
		#define EMIT_SIMD_INT_BINARY_OP(name,emitCode) \
			EMIT_SIMD_BINARY_OP(i8x16##_##name,llvmI8x16Type,emitCode) \
			EMIT_SIMD_BINARY_OP(i16x8##_##name,llvmI16x8Type,emitCode) \
			EMIT_SIMD_BINARY_OP(i32x4##_##name,llvmI32x4Type,emitCode) \
			EMIT_SIMD_BINARY_OP(i64x2##_##name,llvmI64x2Type,emitCode)
		#define EMIT_SIMD_FP_BINARY_OP(name,emitCode) \
			EMIT_SIMD_BINARY_OP(f32x4##_##name,llvmF32x4Type,emitCode) \
			EMIT_SIMD_BINARY_OP(f64x2##_##name,llvmF64x2Type,emitCode)
		#define EMIT_SIMD_INT_UNARY_OP(name,emitCode) \
			EMIT_SIMD_UNARY_OP(i8x16##_##name,llvmI8x16Type,emitCode) \
			EMIT_SIMD_UNARY_OP(i16x8##_##name,llvmI16x8Type,emitCode) \
			EMIT_SIMD_UNARY_OP(i32x4##_##name,llvmI32x4Type,emitCode) \
			EMIT_SIMD_UNARY_OP(i64x2##_##name,llvmI64x2Type,emitCode)
		#define EMIT_SIMD_FP_UNARY_OP(name,emitCode) \
			EMIT_SIMD_UNARY_OP(f32x4##_##name,llvmF32x4Type,emitCode) \
			EMIT_SIMD_UNARY_OP(f64x2##_##name,llvmF64x2Type,emitCode)
		EMIT_SIMD_INT_BINARY_OP(add,irBuilder.CreateAdd(left,right))
		EMIT_SIMD_INT_BINARY_OP(sub,irBuilder.CreateSub(left,right))

		EMIT_SIMD_INT_BINARY_OP(shl,irBuilder.CreateShl(left,right))
		EMIT_SIMD_INT_BINARY_OP(shr_s,irBuilder.CreateAShr(left,right))
		EMIT_SIMD_INT_BINARY_OP(shr_u,irBuilder.CreateLShr(left,right))
		EMIT_SIMD_INT_BINARY_OP(mul,irBuilder.CreateMul(left,right))
		EMIT_SIMD_INT_BINARY_OP(div_s,irBuilder.CreateSDiv(left,right))
		EMIT_SIMD_INT_BINARY_OP(div_u,irBuilder.CreateUDiv(left,right))

		EMIT_SIMD_INT_BINARY_OP(eq,irBuilder.CreateICmpEQ(left,right))
		EMIT_SIMD_INT_BINARY_OP(ne,irBuilder.CreateICmpNE(left,right))
		EMIT_SIMD_INT_BINARY_OP(lt_s,irBuilder.CreateICmpSLT(left,right))
		EMIT_SIMD_INT_BINARY_OP(lt_u,irBuilder.CreateICmpULT(left,right))
		EMIT_SIMD_INT_BINARY_OP(le_s,irBuilder.CreateICmpSLE(left,right))
		EMIT_SIMD_INT_BINARY_OP(le_u,irBuilder.CreateICmpULE(left,right))
		EMIT_SIMD_INT_BINARY_OP(gt_s,irBuilder.CreateICmpSGT(left,right))
		EMIT_SIMD_INT_BINARY_OP(gt_u,irBuilder.CreateICmpUGT(left,right))
		EMIT_SIMD_INT_BINARY_OP(ge_s,irBuilder.CreateICmpSGE(left,right))
		EMIT_SIMD_INT_BINARY_OP(ge_u,irBuilder.CreateICmpUGE(left,right))

		EMIT_SIMD_INT_UNARY_OP(neg,irBuilder.CreateNeg(operand))

		EMIT_SIMD_BINARY_OP(i8x16_add_saturate_s,llvmI8x16Type,callLLVMIntrinsic({},llvm::Intrinsic::x86_sse2_padds_b,{left,right}))
		EMIT_SIMD_BINARY_OP(i8x16_add_saturate_u,llvmI8x16Type,callLLVMIntrinsic({},llvm::Intrinsic::x86_sse2_paddus_b,{left,right}))
		EMIT_SIMD_BINARY_OP(i8x16_sub_saturate_s,llvmI8x16Type,callLLVMIntrinsic({},llvm::Intrinsic::x86_sse2_psubs_b,{left,right}))
		EMIT_SIMD_BINARY_OP(i8x16_sub_saturate_u,llvmI8x16Type,callLLVMIntrinsic({},llvm::Intrinsic::x86_sse2_psubus_b,{left,right}))
		EMIT_SIMD_BINARY_OP(i16x8_add_saturate_s,llvmI16x8Type,callLLVMIntrinsic({},llvm::Intrinsic::x86_sse2_padds_w,{left,right}))
		EMIT_SIMD_BINARY_OP(i16x8_add_saturate_u,llvmI16x8Type,callLLVMIntrinsic({},llvm::Intrinsic::x86_sse2_paddus_w,{left,right}))
		EMIT_SIMD_BINARY_OP(i16x8_sub_saturate_s,llvmI16x8Type,callLLVMIntrinsic({},llvm::Intrinsic::x86_sse2_psubs_w,{left,right}))
		EMIT_SIMD_BINARY_OP(i16x8_sub_saturate_u,llvmI16x8Type,callLLVMIntrinsic({},llvm::Intrinsic::x86_sse2_psubus_w,{left,right}))

		llvm::Value* emitBitSelect(llvm::Value* mask,llvm::Value* trueValue,llvm::Value* falseValue)
		{
			return irBuilder.CreateOr(
				irBuilder.CreateAnd(trueValue,mask),
				irBuilder.CreateAnd(falseValue,irBuilder.CreateNot(mask))
				);
		}

		std::string getLLVMTypeName(llvm::Type* type)
		{
			std::string result;
			llvm::raw_string_ostream typeStream(result);
			type->print(typeStream,true);
			typeStream.flush();
			return result;
		}

		llvm::Value* emitVectorSelect(llvm::Value* condition,llvm::Value* trueValue,llvm::Value* falseValue)
		{
			llvm::Type* maskType;
			switch(condition->getType()->getVectorNumElements())
			{
			case 2: maskType = llvmI64x2Type; break;
			case 4: maskType = llvmI32x4Type; break;
			case 8: maskType = llvmI16x8Type; break;
			case 16: maskType = llvmI8x16Type; break;
			default: Errors::fatalf("unsupported vector length %u",condition->getType()->getVectorNumElements());
			};
			llvm::Value* mask = irBuilder.CreateSExt(condition,maskType);

			return irBuilder.CreateBitCast(
				emitBitSelect(
					mask,
					irBuilder.CreateBitCast(trueValue,maskType),
					irBuilder.CreateBitCast(falseValue,maskType)),
				trueValue->getType());
		}

		template<typename Int,typename Float,Uptr numElements>
		llvm::Value* emitTruncVectorFloatToIntSat(
			llvm::Type* destType,
			bool isSigned,
			Float minFloatBounds, Float maxFloatBounds, 
			Int minIntBounds, Int maxIntBounds,
			Int nanResult,
			llvm::Value* operand)
		{
			return emitVectorSelect(
				irBuilder.CreateFCmpUNO(operand,operand),
				irBuilder.CreateVectorSplat(numElements,emitLiteral(nanResult)),
				emitVectorSelect(
					irBuilder.CreateFCmpOLE(operand,irBuilder.CreateVectorSplat(numElements,emitLiteral(minFloatBounds))),
					irBuilder.CreateVectorSplat(numElements,emitLiteral(minIntBounds)),
					emitVectorSelect(
						irBuilder.CreateFCmpOGE(operand,irBuilder.CreateVectorSplat(numElements,emitLiteral(maxFloatBounds))),
						irBuilder.CreateVectorSplat(numElements,emitLiteral(maxIntBounds)),
						isSigned
							? irBuilder.CreateFPToSI(operand,destType)
							: irBuilder.CreateFPToUI(operand,destType)
					)));
		}

		EMIT_SIMD_UNARY_OP(i32x4_trunc_s_sat_f32x4,llvmF32x4Type,(emitTruncVectorFloatToIntSat<I32,F32,4>)(llvmI32x4Type,true,F32(INT32_MIN),F32(INT32_MAX),INT32_MIN,INT32_MAX,U32(0),operand))
		EMIT_SIMD_UNARY_OP(i32x4_trunc_u_sat_f32x4,llvmF32x4Type,(emitTruncVectorFloatToIntSat<I32,F32,4>)(llvmI32x4Type,false,0.0f,F32(UINT32_MAX),U32(0),UINT32_MAX,U32(0),operand))
		EMIT_SIMD_UNARY_OP(i64x2_trunc_s_sat_f64x2,llvmF64x2Type,(emitTruncVectorFloatToIntSat<I64,F64,2>)(llvmI64x2Type,true,F64(INT64_MIN),F64(INT64_MAX),INT64_MIN,INT64_MAX,U64(0),operand))
		EMIT_SIMD_UNARY_OP(i64x2_trunc_u_sat_f64x2,llvmF64x2Type,(emitTruncVectorFloatToIntSat<I64,F64,2>)(llvmI64x2Type,false,0.0,F64(UINT64_MAX),U64(0),UINT64_MAX,U64(0),operand))

		EMIT_SIMD_FP_BINARY_OP(add,irBuilder.CreateFAdd(left,right))
		EMIT_SIMD_FP_BINARY_OP(sub,irBuilder.CreateFSub(left,right))
		EMIT_SIMD_FP_BINARY_OP(mul,irBuilder.CreateFMul(left,right))
		EMIT_SIMD_FP_BINARY_OP(div,irBuilder.CreateFDiv(left,right))
			
		EMIT_SIMD_FP_BINARY_OP(eq,irBuilder.CreateFCmpOEQ(left,right))
		EMIT_SIMD_FP_BINARY_OP(ne,irBuilder.CreateFCmpUNE(left,right))
		EMIT_SIMD_FP_BINARY_OP(lt,irBuilder.CreateFCmpOLT(left,right))
		EMIT_SIMD_FP_BINARY_OP(le,irBuilder.CreateFCmpOLE(left,right))
		EMIT_SIMD_FP_BINARY_OP(gt,irBuilder.CreateFCmpOGT(left,right))
		EMIT_SIMD_FP_BINARY_OP(ge,irBuilder.CreateFCmpOGE(left,right))
		EMIT_SIMD_BINARY_OP(f32x4_min,llvmF32x4Type,callLLVMIntrinsic({},llvm::Intrinsic::x86_sse_min_ps,{left,right}))
		EMIT_SIMD_BINARY_OP(f64x2_min,llvmF64x2Type,callLLVMIntrinsic({},llvm::Intrinsic::x86_sse2_min_pd,{left,right}))
		EMIT_SIMD_BINARY_OP(f32x4_max,llvmF32x4Type,callLLVMIntrinsic({},llvm::Intrinsic::x86_sse_max_ps,{left,right}))
		EMIT_SIMD_BINARY_OP(f64x2_max,llvmF64x2Type,callLLVMIntrinsic({},llvm::Intrinsic::x86_sse2_max_pd,{left,right}))

		EMIT_SIMD_FP_UNARY_OP(neg,irBuilder.CreateFNeg(operand))
		EMIT_SIMD_FP_UNARY_OP(abs,callLLVMIntrinsic({operand->getType()},llvm::Intrinsic::fabs,{operand}))
		EMIT_SIMD_FP_UNARY_OP(sqrt,callLLVMIntrinsic({operand->getType()},llvm::Intrinsic::sqrt,{operand}))

		EMIT_SIMD_UNARY_OP(f32x4_convert_s_i32x4,llvmI32x4Type,irBuilder.CreateSIToFP(operand,llvmF32x4Type));
		EMIT_SIMD_UNARY_OP(f32x4_convert_u_i32x4,llvmI32x4Type,irBuilder.CreateUIToFP(operand,llvmF32x4Type));
		EMIT_SIMD_UNARY_OP(f64x2_convert_s_i64x2,llvmI64x2Type,irBuilder.CreateSIToFP(operand,llvmF64x2Type));
		EMIT_SIMD_UNARY_OP(f64x2_convert_u_i64x2,llvmI64x2Type,irBuilder.CreateUIToFP(operand,llvmF64x2Type));

		EMIT_SIMD_UNARY_OP(i8x16_any_true,llvmI8x16Type,emitAnyTrue(operand))
		EMIT_SIMD_UNARY_OP(i16x8_any_true,llvmI16x8Type,emitAnyTrue(operand))
		EMIT_SIMD_UNARY_OP(i32x4_any_true,llvmI32x4Type,emitAnyTrue(operand))
		EMIT_SIMD_UNARY_OP(i64x2_any_true,llvmI64x2Type,emitAnyTrue(operand))

		EMIT_SIMD_UNARY_OP(i8x16_all_true,llvmI8x16Type,emitAllTrue(operand))
		EMIT_SIMD_UNARY_OP(i16x8_all_true,llvmI16x8Type,emitAllTrue(operand))
		EMIT_SIMD_UNARY_OP(i32x4_all_true,llvmI32x4Type,emitAllTrue(operand))
		EMIT_SIMD_UNARY_OP(i64x2_all_true,llvmI64x2Type,emitAllTrue(operand))

		void v128_and(NoImm)
		{
			auto right = pop();
			auto left = irBuilder.CreateBitCast(pop(),right->getType());
			push(irBuilder.CreateAnd(left,right));
		}
		void v128_or(NoImm)
		{
			auto right = pop();
			auto left = irBuilder.CreateBitCast(pop(),right->getType());
			push(irBuilder.CreateOr(left,right));
		}
		void v128_xor(NoImm)
		{
			auto right = pop();
			auto left = irBuilder.CreateBitCast(pop(),right->getType());
			push(irBuilder.CreateXor(left,right));
		}
		void v128_not(NoImm)
		{
			auto operand = pop();
			push(irBuilder.CreateNot(operand));
		}

		#define EMIT_SIMD_EXTRACT_LANE_OP(name,llvmType,numLanes,coerceScalar) \
			void name(LaneIndexImm<numLanes> imm) \
			{ \
				auto operand = irBuilder.CreateBitCast(pop(),llvmType); \
				auto scalar = irBuilder.CreateExtractElement(operand,imm.laneIndex); \
				push(coerceScalar); \
			}
		EMIT_SIMD_EXTRACT_LANE_OP(i8x16_extract_lane_s,llvmI8x16Type,16,irBuilder.CreateSExt(scalar,llvmI32Type))
		EMIT_SIMD_EXTRACT_LANE_OP(i8x16_extract_lane_u,llvmI8x16Type,16,irBuilder.CreateZExt(scalar,llvmI32Type))
		EMIT_SIMD_EXTRACT_LANE_OP(i16x8_extract_lane_s,llvmI16x8Type,8,irBuilder.CreateSExt(scalar,llvmI32Type))
		EMIT_SIMD_EXTRACT_LANE_OP(i16x8_extract_lane_u,llvmI16x8Type,8,irBuilder.CreateZExt(scalar,llvmI32Type))
		EMIT_SIMD_EXTRACT_LANE_OP(i32x4_extract_lane,llvmI32x4Type,4,scalar)
		EMIT_SIMD_EXTRACT_LANE_OP(i64x2_extract_lane,llvmI64x2Type,2,scalar)

		EMIT_SIMD_EXTRACT_LANE_OP(f32x4_extract_lane,llvmF32x4Type,4,scalar)
		EMIT_SIMD_EXTRACT_LANE_OP(f64x2_extract_lane,llvmF64x2Type,2,scalar)
		
		#define EMIT_SIMD_REPLACE_LANE_OP(typePrefix,llvmType,numLanes,coerceScalar) \
			void typePrefix##_replace_lane(LaneIndexImm<numLanes> imm) \
			{ \
				auto vector = irBuilder.CreateBitCast(pop(),llvmType); \
				auto scalar = pop(); \
				push(irBuilder.CreateInsertElement(vector,coerceScalar,imm.laneIndex)); \
			}

		EMIT_SIMD_REPLACE_LANE_OP(i8x16,llvmI8x16Type,16,irBuilder.CreateTrunc(scalar,llvmI8Type))
		EMIT_SIMD_REPLACE_LANE_OP(i16x8,llvmI16x8Type,8,irBuilder.CreateTrunc(scalar,llvmI16Type))
		EMIT_SIMD_REPLACE_LANE_OP(i32x4,llvmI32x4Type,4,scalar)
		EMIT_SIMD_REPLACE_LANE_OP(i64x2,llvmI64x2Type,2,scalar)

		EMIT_SIMD_REPLACE_LANE_OP(f32x4,llvmF32x4Type,4,scalar)
		EMIT_SIMD_REPLACE_LANE_OP(f64x2,llvmF64x2Type,2,scalar)

		void v8x16_shuffle(ShuffleImm<16> imm)
		{
			auto right = irBuilder.CreateBitCast(pop(),llvmI8x16Type);
			auto left = irBuilder.CreateBitCast(pop(),llvmI8x16Type);
			unsigned int laneIndices[16];
			for(Uptr laneIndex = 0;laneIndex < 16;++laneIndex)
			{
				laneIndices[laneIndex] = imm.laneIndices[laneIndex];
			}
			push(irBuilder.CreateShuffleVector(left,right,llvm::ArrayRef<unsigned int>(laneIndices,16)));
		}
		
		void v128_const(LiteralImm<V128> imm)
		{
			push(llvm::ConstantVector::get({emitLiteral(imm.value.u64[0]),emitLiteral(imm.value.u64[1])}));
		}

		void v128_bitselect(NoImm)
		{
			auto mask = irBuilder.CreateBitCast(pop(),llvmI64x2Type);
			auto falseValue = irBuilder.CreateBitCast(pop(),llvmI64x2Type);
			auto trueValue = irBuilder.CreateBitCast(pop(),llvmI64x2Type);
			push(emitBitSelect(mask,trueValue,falseValue));
		}

		void atomic_wake(AtomicLoadOrStoreImm<2>)
		{
			auto numWaiters = pop();
			auto address = pop();
			auto memoryId = emitLiteral(U64(moduleContext.moduleInstance->defaultMemory->id));
			push(emitRuntimeIntrinsic(
				"atomic_wake",
				FunctionType::get(ResultType::i32,{ValueType::i32,ValueType::i32,ValueType::i64}),
				{address,numWaiters,memoryId}));
		}
		void i32_atomic_wait(AtomicLoadOrStoreImm<2>)
		{
			auto timeout = pop();
			auto expectedValue = pop();
			auto address = pop();
			auto memoryId = emitLiteral(U64(moduleContext.moduleInstance->defaultMemory->id));
			push(emitRuntimeIntrinsic(
				"atomic_wait_i32",
				FunctionType::get(ResultType::i32,{ValueType::i32,ValueType::i32,ValueType::f64,ValueType::i64}),
				{address,expectedValue,timeout,memoryId}));
		}
		void i64_atomic_wait(AtomicLoadOrStoreImm<3>)
		{
			auto timeout = pop();
			auto expectedValue = pop();
			auto address = pop();
			auto memoryId = emitLiteral(U64(moduleContext.moduleInstance->defaultMemory->id));
			push(emitRuntimeIntrinsic(
				"atomic_wait_i64",
				FunctionType::get(ResultType::i32,{ValueType::i32,ValueType::i64,ValueType::f64,ValueType::i64}),
				{address,expectedValue,timeout,memoryId}));
		}

		void trapIfMisalignedAtomic(llvm::Value* address,U32 naturalAlignmentLog2)
		{
			if(naturalAlignmentLog2 > 0)
			{
				emitConditionalTrapIntrinsic(
					irBuilder.CreateICmpNE(
						typedZeroConstants[(Uptr)ValueType::i32],
						irBuilder.CreateAnd(address,emitLiteral((U32(1) << naturalAlignmentLog2) - 1))),
					"misalignedAtomicTrap",
					FunctionType::get(ResultType::none,{ValueType::i32}),
					{address});
			}
		}

		EMIT_UNARY_OP(i32,extend8_s,irBuilder.CreateSExt(irBuilder.CreateTrunc(operand,llvmI8Type),llvmI32Type))
		EMIT_UNARY_OP(i32,extend16_s,irBuilder.CreateSExt(irBuilder.CreateTrunc(operand,llvmI16Type),llvmI32Type))
		EMIT_UNARY_OP(i64,extend8_s,irBuilder.CreateSExt(irBuilder.CreateTrunc(operand,llvmI8Type),llvmI64Type))
		EMIT_UNARY_OP(i64,extend16_s,irBuilder.CreateSExt(irBuilder.CreateTrunc(operand,llvmI16Type),llvmI64Type))
		EMIT_UNARY_OP(i64,extend32_s,irBuilder.CreateSExt(irBuilder.CreateTrunc(operand,llvmI32Type),llvmI64Type))

		#define EMIT_ATOMIC_LOAD_OP(valueTypeId,name,llvmMemoryType,naturalAlignmentLog2,conversionOp) \
			void valueTypeId##_##name(AtomicLoadOrStoreImm<naturalAlignmentLog2> imm) \
			{ \
				auto byteIndex = pop(); \
				trapIfMisalignedAtomic(byteIndex,naturalAlignmentLog2); \
				auto pointer = coerceByteIndexToPointer(byteIndex,imm.offset,llvmMemoryType); \
				auto load = irBuilder.CreateLoad(pointer); \
				load->setAlignment(1<<imm.alignmentLog2); \
				load->setVolatile(true); \
				load->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent); \
				push(conversionOp(load,asLLVMType(ValueType::valueTypeId))); \
			}
		#define EMIT_ATOMIC_STORE_OP(valueTypeId,name,llvmMemoryType,naturalAlignmentLog2,conversionOp) \
			void valueTypeId##_##name(AtomicLoadOrStoreImm<naturalAlignmentLog2> imm) \
			{ \
				auto value = pop(); \
				auto byteIndex = pop(); \
				trapIfMisalignedAtomic(byteIndex,naturalAlignmentLog2); \
				auto pointer = coerceByteIndexToPointer(byteIndex,imm.offset,llvmMemoryType); \
				auto memoryValue = conversionOp(value,llvmMemoryType); \
				auto store = irBuilder.CreateStore(memoryValue,pointer); \
				store->setVolatile(true); \
				store->setAlignment(1<<imm.alignmentLog2); \
				store->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent); \
			}
		EMIT_ATOMIC_LOAD_OP(i32,atomic_load,llvmI32Type,2,identityConversion)
		EMIT_ATOMIC_LOAD_OP(i64,atomic_load,llvmI64Type,3,identityConversion)

		EMIT_ATOMIC_LOAD_OP(i32,atomic_load8_s,llvmI8Type,0,irBuilder.CreateSExt)
		EMIT_ATOMIC_LOAD_OP(i32,atomic_load8_u,llvmI8Type,0,irBuilder.CreateZExt)
		EMIT_ATOMIC_LOAD_OP(i32,atomic_load16_s,llvmI16Type,1,irBuilder.CreateSExt)
		EMIT_ATOMIC_LOAD_OP(i32,atomic_load16_u,llvmI16Type,1,irBuilder.CreateZExt)
		EMIT_ATOMIC_LOAD_OP(i64,atomic_load8_s,llvmI8Type,0,irBuilder.CreateSExt)
		EMIT_ATOMIC_LOAD_OP(i64,atomic_load8_u,llvmI8Type,0,irBuilder.CreateZExt)
		EMIT_ATOMIC_LOAD_OP(i64,atomic_load16_s,llvmI16Type,1,irBuilder.CreateSExt)
		EMIT_ATOMIC_LOAD_OP(i64,atomic_load16_u,llvmI16Type,1,irBuilder.CreateZExt)
		EMIT_ATOMIC_LOAD_OP(i64,atomic_load32_s,llvmI32Type,2,irBuilder.CreateSExt)
		EMIT_ATOMIC_LOAD_OP(i64,atomic_load32_u,llvmI32Type,2,irBuilder.CreateZExt)

		EMIT_ATOMIC_STORE_OP(i32,atomic_store,llvmI32Type,2,identityConversion)
		EMIT_ATOMIC_STORE_OP(i64,atomic_store,llvmI64Type,3,identityConversion)
			
		EMIT_ATOMIC_STORE_OP(i32,atomic_store8,llvmI8Type,0,irBuilder.CreateTrunc)
		EMIT_ATOMIC_STORE_OP(i32,atomic_store16,llvmI16Type,1,irBuilder.CreateTrunc)
		EMIT_ATOMIC_STORE_OP(i64,atomic_store8,llvmI8Type,0,irBuilder.CreateTrunc)
		EMIT_ATOMIC_STORE_OP(i64,atomic_store16,llvmI16Type,1,irBuilder.CreateTrunc)
		EMIT_ATOMIC_STORE_OP(i64,atomic_store32,llvmI32Type,2,irBuilder.CreateTrunc)

		#define EMIT_ATOMIC_CMPXCHG(valueTypeId,name,llvmMemoryType,naturalAlignmentLog2,memoryToValueConversion,valueToMemoryConversion) \
			void valueTypeId##_##name(AtomicLoadOrStoreImm<naturalAlignmentLog2> imm) \
			{ \
				auto replacementValue = valueToMemoryConversion(pop(),llvmMemoryType); \
				auto expectedValue = valueToMemoryConversion(pop(),llvmMemoryType); \
				auto byteIndex = pop(); \
				trapIfMisalignedAtomic(byteIndex,naturalAlignmentLog2); \
				auto pointer = coerceByteIndexToPointer(byteIndex,imm.offset,llvmMemoryType); \
				auto atomicCmpXchg = irBuilder.CreateAtomicCmpXchg( \
					pointer, \
					expectedValue, \
					replacementValue, \
					llvm::AtomicOrdering::SequentiallyConsistent, \
					llvm::AtomicOrdering::SequentiallyConsistent); \
				atomicCmpXchg->setVolatile(true); \
				auto previousValue = irBuilder.CreateExtractValue(atomicCmpXchg,{0}); \
				push(memoryToValueConversion(previousValue,asLLVMType(ValueType::valueTypeId))); \
			}

		EMIT_ATOMIC_CMPXCHG(i32,atomic_rmw8_u_cmpxchg,llvmI8Type,0,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_CMPXCHG(i32,atomic_rmw16_u_cmpxchg,llvmI16Type,1,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_CMPXCHG(i32,atomic_rmw_cmpxchg,llvmI32Type,2,identityConversion,identityConversion)
			
		EMIT_ATOMIC_CMPXCHG(i64,atomic_rmw8_u_cmpxchg,llvmI8Type,0,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_CMPXCHG(i64,atomic_rmw16_u_cmpxchg,llvmI16Type,1,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_CMPXCHG(i64,atomic_rmw32_u_cmpxchg,llvmI32Type,2,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_CMPXCHG(i64,atomic_rmw_cmpxchg,llvmI64Type,3,identityConversion,identityConversion)
			
		#define EMIT_ATOMIC_RMW(valueTypeId,name,rmwOpId,llvmMemoryType,naturalAlignmentLog2,memoryToValueConversion,valueToMemoryConversion) \
			void valueTypeId##_##name(AtomicLoadOrStoreImm<naturalAlignmentLog2> imm) \
			{ \
				auto value = valueToMemoryConversion(pop(),llvmMemoryType); \
				auto byteIndex = pop(); \
				trapIfMisalignedAtomic(byteIndex,naturalAlignmentLog2); \
				auto pointer = coerceByteIndexToPointer(byteIndex,imm.offset,llvmMemoryType); \
				auto atomicRMW = irBuilder.CreateAtomicRMW( \
					llvm::AtomicRMWInst::BinOp::rmwOpId, \
					pointer, \
					value, \
					llvm::AtomicOrdering::SequentiallyConsistent); \
				atomicRMW->setVolatile(true); \
				push(memoryToValueConversion(atomicRMW,asLLVMType(ValueType::valueTypeId))); \
			}

		EMIT_ATOMIC_RMW(i32,atomic_rmw8_u_xchg,Xchg,llvmI8Type,0,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i32,atomic_rmw16_u_xchg,Xchg,llvmI16Type,1,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i32,atomic_rmw_xchg,Xchg,llvmI32Type,2,identityConversion,identityConversion)

		EMIT_ATOMIC_RMW(i64,atomic_rmw8_u_xchg,Xchg,llvmI8Type,0,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw16_u_xchg,Xchg,llvmI16Type,1,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw32_u_xchg,Xchg,llvmI32Type,2,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw_xchg,Xchg,llvmI64Type,3,identityConversion,identityConversion)

		EMIT_ATOMIC_RMW(i32,atomic_rmw8_u_add,Add,llvmI8Type,0,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i32,atomic_rmw16_u_add,Add,llvmI16Type,1,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i32,atomic_rmw_add,Add,llvmI32Type,2,identityConversion,identityConversion)

		EMIT_ATOMIC_RMW(i64,atomic_rmw8_u_add,Add,llvmI8Type,0,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw16_u_add,Add,llvmI16Type,1,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw32_u_add,Add,llvmI32Type,2,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw_add,Add,llvmI64Type,3,identityConversion,identityConversion)
			
		EMIT_ATOMIC_RMW(i32,atomic_rmw8_u_sub,Sub,llvmI8Type,0,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i32,atomic_rmw16_u_sub,Sub,llvmI16Type,1,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i32,atomic_rmw_sub,Sub,llvmI32Type,2,identityConversion,identityConversion)

		EMIT_ATOMIC_RMW(i64,atomic_rmw8_u_sub,Sub,llvmI8Type,0,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw16_u_sub,Sub,llvmI16Type,1,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw32_u_sub,Sub,llvmI32Type,2,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw_sub,Sub,llvmI64Type,3,identityConversion,identityConversion)
			
		EMIT_ATOMIC_RMW(i32,atomic_rmw8_u_and,And,llvmI8Type,0,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i32,atomic_rmw16_u_and,And,llvmI16Type,1,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i32,atomic_rmw_and,And,llvmI32Type,2,identityConversion,identityConversion)

		EMIT_ATOMIC_RMW(i64,atomic_rmw8_u_and,And,llvmI8Type,0,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw16_u_and,And,llvmI16Type,1,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw32_u_and,And,llvmI32Type,2,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw_and,And,llvmI64Type,3,identityConversion,identityConversion)
			
		EMIT_ATOMIC_RMW(i32,atomic_rmw8_u_or,Or,llvmI8Type,0,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i32,atomic_rmw16_u_or,Or,llvmI16Type,1,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i32,atomic_rmw_or,Or,llvmI32Type,2,identityConversion,identityConversion)

		EMIT_ATOMIC_RMW(i64,atomic_rmw8_u_or,Or,llvmI8Type,0,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw16_u_or,Or,llvmI16Type,1,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw32_u_or,Or,llvmI32Type,2,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw_or,Or,llvmI64Type,3,identityConversion,identityConversion)
			
		EMIT_ATOMIC_RMW(i32,atomic_rmw8_u_xor,Xor,llvmI8Type,0,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i32,atomic_rmw16_u_xor,Xor,llvmI16Type,1,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i32,atomic_rmw_xor,Xor,llvmI32Type,2,identityConversion,identityConversion)

		EMIT_ATOMIC_RMW(i64,atomic_rmw8_u_xor,Xor,llvmI8Type,0,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw16_u_xor,Xor,llvmI16Type,1,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw32_u_xor,Xor,llvmI32Type,2,irBuilder.CreateZExt,irBuilder.CreateTrunc)
		EMIT_ATOMIC_RMW(i64,atomic_rmw_xor,Xor,llvmI64Type,3,identityConversion,identityConversion)

		struct TryContext
		{
			llvm::BasicBlock* unwindToBlock;
		};

		struct CatchContext
		{
			#ifdef _WIN64
				llvm::CatchSwitchInst* catchSwitchInst;
			#else
				llvm::LandingPadInst* landingPadInst;
				llvm::BasicBlock* nextHandlerBlock;
				llvm::Value* exceptionTypeInstance;
			#endif
			llvm::Value* exceptionPointer;
		};
		
		std::vector<TryContext> tryStack;
		std::vector<CatchContext> catchStack;

		void endTry()
		{
			wavmAssert(tryStack.size());
			tryStack.pop_back();
			catchStack.pop_back();
		}
		
		void endCatch()
		{
			wavmAssert(catchStack.size());
			#ifndef _WIN64
				CatchContext& catchContext = catchStack.back();

				irBuilder.SetInsertPoint(catchContext.nextHandlerBlock);
				emitThrow(
					loadFromUntypedPointer(catchContext.exceptionPointer,llvmI64Type),
					irBuilder.CreatePtrToInt(
						irBuilder.CreateInBoundsGEP(
							catchContext.exceptionPointer,
							{emitLiteral(I32(offsetof(ExceptionData,arguments)))}),
						llvmI64Type
						),
					false);
				irBuilder.CreateUnreachable();
			#endif

			catchStack.pop_back();
		}

		llvm::BasicBlock* getInnermostUnwindToBlock()
		{
			if(tryStack.size()) { return tryStack.back().unwindToBlock; }
			else { return nullptr; }
		}
		
		#ifdef _WIN32

			void try_(ControlStructureImm imm)
			{
				auto catchSwitchBlock = llvm::BasicBlock::Create(*llvmContext,"catchSwitch",llvmFunction);
				auto originalInsertBlock = irBuilder.GetInsertBlock();
				irBuilder.SetInsertPoint(catchSwitchBlock);
				auto catchSwitchInst = irBuilder.CreateCatchSwitch(
					llvm::ConstantTokenNone::get(*llvmContext),
					nullptr,
					1
					);
				irBuilder.SetInsertPoint(originalInsertBlock);
				tryStack.push_back(TryContext {catchSwitchBlock});
				catchStack.push_back(CatchContext {catchSwitchInst,nullptr});

				// Create an end try+phi for the try result.
				auto endBlock = llvm::BasicBlock::Create(*llvmContext,"tryEnd",llvmFunction);
				auto endPHI = createPHI(endBlock,imm.resultType);

				// Push a control context that ends at the end block/phi.
				pushControlStack(ControlContext::Type::try_,imm.resultType,endBlock,endPHI);
				
				// Push a branch target for the end block/phi.
				pushBranchTarget(imm.resultType,endBlock,endPHI);

				// Call a dummy function to workaround a LLVM bug on Windows with the
				// recoverfp intrinsic if the try block doesn't contain any calls.
				if(!moduleContext.tryPrologueDummyFunction)
				{
					moduleContext.tryPrologueDummyFunction = llvm::Function::Create(
						llvm::FunctionType::get(llvmVoidType,false),
						llvm::GlobalValue::LinkageTypes::InternalLinkage,
						"__try_prologue",
						moduleContext.llvmModule
						);
					auto entryBasicBlock = llvm::BasicBlock::Create(*llvmContext,"entry",moduleContext.tryPrologueDummyFunction);
					llvm::IRBuilder<> dummyIRBuilder(*llvmContext);
					dummyIRBuilder.SetInsertPoint(entryBasicBlock);
					dummyIRBuilder.CreateRetVoid();
				}
				emitCallOrInvoke(
					moduleContext.tryPrologueDummyFunction,
					{},
					FunctionType::get(),
					CallingConvention::c,
					getInnermostUnwindToBlock());
			}

			llvm::Function* createSEHFilterFunction(
				const ExceptionTypeInstance* catchTypeInstance,
				llvm::Value*& outExceptionDataAlloca
				)
			{
				// Insert an alloca for the exception point at the beginning of the function, and add it as a localescape.
				llvm::BasicBlock* savedInsertBlock = irBuilder.GetInsertBlock();
				if(!localEscapeBlock) { localEscapeBlock = llvm::BasicBlock::Create(*llvmContext,"alloca",llvmFunction); }
				irBuilder.SetInsertPoint(localEscapeBlock);
				const Uptr exceptionDataLocalEscapeIndex = pendingLocalEscapes.size();
				outExceptionDataAlloca = irBuilder.CreateAlloca(llvmI8PtrType);
				pendingLocalEscapes.push_back(outExceptionDataAlloca);
				irBuilder.SetInsertPoint(savedInsertBlock);

				// Create a SEH filter function that decides whether to handle an exception.
				llvm::FunctionType* filterFunctionType = llvm::FunctionType::get(llvmI32Type,{llvmI8PtrType,llvmI8PtrType},false);
				auto filterFunction = llvm::Function::Create(filterFunctionType,llvm::GlobalValue::LinkageTypes::InternalLinkage,"sehFilter",moduleContext.llvmModule);
				auto filterEntryBasicBlock = llvm::BasicBlock::Create(*llvmContext,"entry",filterFunction);
				auto argIt = filterFunction->arg_begin();
				llvm::IRBuilder<> filterIRBuilder(filterEntryBasicBlock);

				// Get the pointer to the Windows EXCEPTION_RECORD struct.
				auto exceptionPointersArg = filterIRBuilder.CreatePointerCast(
					(llvm::Argument*)&(*argIt++),llvmExceptionPointersStructType->getPointerTo());
				auto exceptionRecordPointer = filterIRBuilder.CreateLoad(filterIRBuilder.CreateInBoundsGEP(
					exceptionPointersArg,
					{emitLiteral(0),emitLiteral(0)}));
				
				// Recover the frame pointer of the catching frame, and the escaped local to write the exception pointer to.
				auto framePointer = filterIRBuilder.CreateCall(
					getLLVMIntrinsic({},llvm::Intrinsic::x86_seh_recoverfp),
					{
						filterIRBuilder.CreatePointerCast(llvmFunction,llvmI8PtrType),
						(llvm::Value*)&(*argIt++)
					});
				auto exceptionDataAlloca = filterIRBuilder.CreateCall(
					getLLVMIntrinsic({},llvm::Intrinsic::localrecover),
					{
						filterIRBuilder.CreatePointerCast(llvmFunction,llvmI8PtrType),
						framePointer,
						emitLiteral(I32(exceptionDataLocalEscapeIndex))
					});

				// Check if the exception code is SEH_WAVM_EXCEPTION
				// If it does not match, return 0 from the filter function.
				auto nonWebAssemblyExceptionBlock = llvm::BasicBlock::Create(*llvmContext,"nonWebAssemblyException",filterFunction);
				auto exceptionTypeCheckBlock = llvm::BasicBlock::Create(*llvmContext,"exceptionTypeCheck",filterFunction);
				auto exceptionCode = filterIRBuilder.CreateLoad(filterIRBuilder.CreateInBoundsGEP(
					exceptionRecordPointer,
					{emitLiteral(0),emitLiteral(0)}));
				auto isWebAssemblyException = filterIRBuilder.CreateICmpEQ(exceptionCode,emitLiteral(I32(Platform::SEH_WAVM_EXCEPTION)));
				filterIRBuilder.CreateCondBr(isWebAssemblyException,exceptionTypeCheckBlock,nonWebAssemblyExceptionBlock);
				filterIRBuilder.SetInsertPoint(nonWebAssemblyExceptionBlock);
				filterIRBuilder.CreateRet(emitLiteral(I32(0)));
				filterIRBuilder.SetInsertPoint(exceptionTypeCheckBlock);
				
				// Copy the pointer to the exception data to the alloca in the catch frame.
				auto exceptionDataGEP = filterIRBuilder.CreateInBoundsGEP(exceptionRecordPointer,{emitLiteral(0),emitLiteral(5),emitLiteral(0)});
				auto exceptionData = filterIRBuilder.CreateLoad(exceptionDataGEP);
				filterIRBuilder.CreateStore(
					exceptionData,
					filterIRBuilder.CreatePointerCast(exceptionDataAlloca,llvmI64Type->getPointerTo()));
				
				if(!catchTypeInstance)
				{
					// If the exception code is SEH_WAVM_EXCEPTION, and the exception is a user exception,
					// return 1 from the filter function.
					auto isUserExceptionI8 = filterIRBuilder.CreateLoad(filterIRBuilder.CreateInBoundsGEP(
						filterIRBuilder.CreateIntToPtr(exceptionData,llvmI8Type->getPointerTo()),
						{emitLiteral(offsetof(ExceptionData,isUserException))}));
					filterIRBuilder.CreateRet(filterIRBuilder.CreateZExt(isUserExceptionI8,llvmI32Type));
				}
				else
				{
					// If the exception code is SEH_WAVM_EXCEPTION, and the thrown exception matches the catch exception type,
					// return 1 from the filter function.
					auto exceptionTypeInstance = filterIRBuilder.CreateLoad(filterIRBuilder.CreateIntToPtr(exceptionData,llvmI64Type->getPointerTo()));
					auto isExpectedTypeInstance = filterIRBuilder.CreateICmpEQ(
						exceptionTypeInstance,
						emitLiteral(reinterpret_cast<I64>(catchTypeInstance)));
					filterIRBuilder.CreateRet(filterIRBuilder.CreateZExt(isExpectedTypeInstance,llvmI32Type));
				}

				return filterFunction;
			}

			void catch_(CatchImm imm)
			{
				wavmAssert(controlStack.size());
				wavmAssert(catchStack.size());
				ControlContext& controlContext = controlStack.back();
				CatchContext& catchContext = catchStack.back();
				wavmAssert(controlContext.type == ControlContext::Type::try_
					|| controlContext.type == ControlContext::Type::catch_);
				if(controlContext.type == ControlContext::Type::try_)
				{
					wavmAssert(tryStack.size());
					tryStack.pop_back();
				}

				branchToEndOfControlContext();

				// Look up the exception type instance to be caught
				wavmAssert(imm.exceptionTypeIndex < moduleContext.moduleInstance->exceptionTypes.size());
				const Runtime::ExceptionTypeInstance* catchTypeInstance
					= moduleContext.moduleInstance->exceptionTypes[imm.exceptionTypeIndex];
				
				// Create a filter function that returns 1 for the specific exception type this instruction catches.
				llvm::Value* exceptionDataAlloca = nullptr;
				auto filterFunction = createSEHFilterFunction(catchTypeInstance,exceptionDataAlloca);

				// Create a block+catchpad that the catchswitch will transfer control to if the filter function returns 1.
				auto catchPadBlock = llvm::BasicBlock::Create(*llvmContext,"catchPad",llvmFunction);
				catchContext.catchSwitchInst->addHandler(catchPadBlock);
				irBuilder.SetInsertPoint(catchPadBlock);
				auto catchPadInst = irBuilder.CreateCatchPad(catchContext.catchSwitchInst,{filterFunction});

				// Create a catchret that immediately returns from the catch "funclet" to a new non-funclet basic block.
				auto catchBlock = llvm::BasicBlock::Create(*llvmContext,"catch",llvmFunction);
				irBuilder.CreateCatchRet(catchPadInst,catchBlock);
				irBuilder.SetInsertPoint(catchBlock);

				catchContext.exceptionPointer = irBuilder.CreateLoad(exceptionDataAlloca);
				for(Uptr argumentIndex = 0;argumentIndex < catchTypeInstance->parameters->elements.size();++argumentIndex)
				{
					const ValueType argumentType = catchTypeInstance->parameters->elements[argumentIndex];
					auto argument = loadFromUntypedPointer(
						irBuilder.CreateInBoundsGEP(catchContext.exceptionPointer,
							{emitLiteral(offsetof(ExceptionData,arguments[catchTypeInstance->parameters->elements.size() - argumentIndex - 1]))}),
						asLLVMType(argumentType));
					push(argument);
				}

				// Change the top of the control stack to a catch clause.
				controlContext.type = ControlContext::Type::catch_;
				controlContext.isReachable = true;
			}
			void catch_all(NoImm)
			{
				wavmAssert(controlStack.size());
				wavmAssert(catchStack.size());
				ControlContext& controlContext = controlStack.back();
				CatchContext& catchContext = catchStack.back();
				wavmAssert(controlContext.type == ControlContext::Type::try_
					|| controlContext.type == ControlContext::Type::catch_);
				if(controlContext.type == ControlContext::Type::try_)
				{
					wavmAssert(tryStack.size());
					tryStack.pop_back();
				}

				branchToEndOfControlContext();
				
				// Create a filter function that returns 1 for any WebAssembly exception.
				llvm::Value* exceptionDataAlloca = nullptr;
				auto filterFunction = createSEHFilterFunction(nullptr,exceptionDataAlloca);

				// Create a block+catchpad that the catchswitch will transfer control to if the filter function returns 1.
				auto catchPadBlock = llvm::BasicBlock::Create(*llvmContext,"catchPad",llvmFunction);
				catchContext.catchSwitchInst->addHandler(catchPadBlock);
				irBuilder.SetInsertPoint(catchPadBlock);
				auto catchPadInst = irBuilder.CreateCatchPad(catchContext.catchSwitchInst,{filterFunction});

				// Create a catchret that immediately returns from the catch "funclet" to a new non-funclet basic block.
				auto catchBlock = llvm::BasicBlock::Create(*llvmContext,"catch",llvmFunction);
				irBuilder.CreateCatchRet(catchPadInst,catchBlock);
				irBuilder.SetInsertPoint(catchBlock);

				catchContext.exceptionPointer = irBuilder.CreateLoad(exceptionDataAlloca);

				// Change the top of the control stack to a catch clause.
				controlContext.type = ControlContext::Type::catch_;
				controlContext.isReachable = true;
			}
		#else
			void try_(ControlStructureImm imm)
			{
				auto landingPadBlock = llvm::BasicBlock::Create(*llvmContext,"landingPad",llvmFunction);
				auto originalInsertBlock = irBuilder.GetInsertBlock();
				irBuilder.SetInsertPoint(landingPadBlock);
				auto landingPadInst = irBuilder.CreateLandingPad(
					llvm::StructType::get(*llvmContext,{llvmI8PtrType,llvmI32Type}),
					1
					);
				auto exceptionPointer = loadFromUntypedPointer(
						irBuilder.CreateCall(
							moduleContext.cxaBeginCatchFunction,
							{irBuilder.CreateExtractValue(landingPadInst,{0})}),
						llvmI8Type->getPointerTo());
				auto exceptionTypeInstance = loadFromUntypedPointer(
						irBuilder.CreateInBoundsGEP(
							exceptionPointer,
							{emitLiteral(Uptr(offsetof(ExceptionData,typeInstance)))}),
						llvmI64Type);

				irBuilder.SetInsertPoint(originalInsertBlock);
				tryStack.push_back(TryContext {landingPadBlock});
				catchStack.push_back(CatchContext {landingPadInst,landingPadBlock,exceptionTypeInstance,exceptionPointer});

				// Add the platform exception type to the landing pad's type filter.
				auto platformExceptionTypeInfo = (llvm::Constant*)irBuilder.CreateIntToPtr(
					emitLiteral(reinterpret_cast<Uptr>(Platform::getUserExceptionTypeInfo())),
					llvmI8PtrType);
				landingPadInst->addClause(platformExceptionTypeInfo);

				// Create an end try+phi for the try result.
				auto endBlock = llvm::BasicBlock::Create(*llvmContext,"tryEnd",llvmFunction);
				auto endPHI = createPHI(endBlock,imm.resultType);

				// Push a control context that ends at the end block/phi.
				pushControlStack(ControlContext::Type::try_,imm.resultType,endBlock,endPHI);
				
				// Push a branch target for the end block/phi.
				pushBranchTarget(imm.resultType,endBlock,endPHI);
			}

			void catch_(CatchImm imm)
			{
				wavmAssert(controlStack.size());
				wavmAssert(catchStack.size());
				ControlContext& controlContext = controlStack.back();
				CatchContext& catchContext = catchStack.back();
				wavmAssert(controlContext.type == ControlContext::Type::try_
					|| controlContext.type == ControlContext::Type::catch_);
				if(controlContext.type == ControlContext::Type::try_)
				{
					wavmAssert(tryStack.size());
					tryStack.pop_back();
				}

				branchToEndOfControlContext();

				// Look up the exception type instance to be caught
				wavmAssert(imm.exceptionTypeIndex < moduleContext.moduleInstance->exceptionTypes.size());
				const Runtime::ExceptionTypeInstance* catchTypeInstance
					= moduleContext.moduleInstance->exceptionTypes[imm.exceptionTypeIndex];

				irBuilder.SetInsertPoint(catchContext.nextHandlerBlock);
				auto isExceptionType = irBuilder.CreateICmpEQ(
					catchContext.exceptionTypeInstance,
					emitLiteral(reinterpret_cast<Uptr>(catchTypeInstance)));

				auto catchBlock = llvm::BasicBlock::Create(*llvmContext,"catch",llvmFunction);
				auto unhandledBlock = llvm::BasicBlock::Create(*llvmContext,"unhandled",llvmFunction);
				irBuilder.CreateCondBr(isExceptionType,catchBlock,unhandledBlock);
				catchContext.nextHandlerBlock = unhandledBlock;
				irBuilder.SetInsertPoint(catchBlock);

				for(Iptr argumentIndex = catchTypeInstance->parameters->elements.size() - 1
				   ;argumentIndex >= 0
				   ;--argumentIndex)
				{
					const ValueType argumentType = catchTypeInstance->parameters->elements[argumentIndex];
					const Uptr argumentOffset = 
						  offsetof(ExceptionData,arguments)
						+ sizeof(ExceptionData::arguments[0]) * argumentIndex;
					auto argument = loadFromUntypedPointer(
						irBuilder.CreateInBoundsGEP(catchContext.exceptionPointer,{emitLiteral(argumentOffset)}),
						asLLVMType(argumentType));
					push(argument);
				}

				// Change the top of the control stack to a catch clause.
				controlContext.type = ControlContext::Type::catch_;
				controlContext.isReachable = true;
			}
			void catch_all(NoImm)
			{
				wavmAssert(controlStack.size());
				wavmAssert(catchStack.size());
				ControlContext& controlContext = controlStack.back();
				CatchContext& catchContext = catchStack.back();
				wavmAssert(controlContext.type == ControlContext::Type::try_
					|| controlContext.type == ControlContext::Type::catch_);
				if(controlContext.type == ControlContext::Type::try_)
				{
					wavmAssert(tryStack.size());
					tryStack.pop_back();
				}

				branchToEndOfControlContext();

				irBuilder.SetInsertPoint(catchContext.nextHandlerBlock);
				auto isUserExceptionType = irBuilder.CreateICmpNE(
					loadFromUntypedPointer(
						irBuilder.CreateInBoundsGEP(
							catchContext.exceptionPointer,
							{emitLiteral(Uptr(offsetof(ExceptionData,isUserException)))}),
						llvmI8Type),
					llvm::ConstantInt::get(llvmI8Type,llvm::APInt(8,0,false)));

				auto catchBlock = llvm::BasicBlock::Create(*llvmContext,"catch",llvmFunction);
				auto unhandledBlock = llvm::BasicBlock::Create(*llvmContext,"unhandled",llvmFunction);
				irBuilder.CreateCondBr(isUserExceptionType,catchBlock,unhandledBlock);
				catchContext.nextHandlerBlock = unhandledBlock;
				irBuilder.SetInsertPoint(catchBlock);

				// Change the top of the control stack to a catch clause.
				controlContext.type = ControlContext::Type::catch_;
				controlContext.isReachable = true;
			}
		#endif

		void emitThrow(llvm::Value* exceptionTypeInstanceI64,llvm::Value* argumentsPointerI64,bool isUserException)
		{
			emitRuntimeIntrinsic(
				"throwException",
				FunctionType::get(ResultType::none,{ValueType::i64,ValueType::i64,ValueType::i32}),
				{exceptionTypeInstanceI64,argumentsPointerI64,emitLiteral(I32(isUserException ? 1 : 0))});
		}

		void throw_(ThrowImm imm)
		{
			const Runtime::ExceptionTypeInstance* exceptionTypeInstance
				= moduleContext.moduleInstance->exceptionTypes[imm.exceptionTypeIndex];
			
			const Uptr numArgs = exceptionTypeInstance->parameters->elements.size();
			const Uptr numArgBytes = numArgs * sizeof(UntaggedValue);
			auto argBaseAddress = irBuilder.CreateAlloca(llvmI8Type,emitLiteral(numArgBytes));

			for(Uptr argIndex = 0;argIndex < exceptionTypeInstance->parameters->elements.size();++argIndex)
			{
				auto elementValue = pop();
				irBuilder.CreateStore(
					elementValue,
					irBuilder.CreatePointerCast(
						irBuilder.CreateInBoundsGEP(argBaseAddress,{emitLiteral((numArgs - argIndex - 1) * sizeof(UntaggedValue))}),
						elementValue->getType()->getPointerTo()
						)
					);
			}

			emitThrow(
				emitLiteral((U64)reinterpret_cast<Uptr>(exceptionTypeInstance)),
				sizeof(Uptr) == 8
					? irBuilder.CreatePtrToInt(argBaseAddress,llvmI64Type)
					: irBuilder.CreateZExt(irBuilder.CreatePtrToInt(argBaseAddress,llvmI32Type),llvmI64Type),
				true
				);

			irBuilder.CreateUnreachable();
			enterUnreachable();
		}
		void rethrow(RethrowImm imm)
		{
			wavmAssert(imm.catchDepth < catchStack.size());
			CatchContext& catchContext = catchStack[catchStack.size() - imm.catchDepth - 1];
			emitThrow(
				loadFromUntypedPointer(catchContext.exceptionPointer,llvmI64Type),
				irBuilder.CreatePtrToInt(
					irBuilder.CreateInBoundsGEP(
						catchContext.exceptionPointer,
						{emitLiteral(I32(offsetof(ExceptionData,arguments)))}),
					llvmI64Type
					),
				true
				);

			irBuilder.CreateUnreachable();
			enterUnreachable();
		}
	};
	
	// A do-nothing visitor used to decode past unreachable operators (but supporting logging, and passing the end operator through).
	struct UnreachableOpVisitor
	{
		typedef void Result;

		UnreachableOpVisitor(EmitFunctionContext& inContext): context(inContext), unreachableControlDepth(0) {}
		#define VISIT_OP(opcode,name,nameString,Imm,...) void name(Imm imm) {}
		ENUM_NONCONTROL_OPERATORS(VISIT_OP)
		VISIT_OP(_,unknown,"unknown",Opcode)
		#undef VISIT_OP

		// Keep track of control structure nesting level in unreachable code, so we know when we reach the end of the unreachable code.
		void block(ControlStructureImm) { ++unreachableControlDepth; }
		void loop(ControlStructureImm) { ++unreachableControlDepth; }
		void if_(ControlStructureImm) { ++unreachableControlDepth; }
		
		// If an else or end opcode would signal an end to the unreachable code, then pass it through to the IR emitter.
		void else_(NoImm imm)
		{
			if(!unreachableControlDepth) { context.else_(imm); }
		}
		void end(NoImm imm)
		{
			if(!unreachableControlDepth) { context.end(imm); }
			else { --unreachableControlDepth; }
		}
		
		void try_(ControlStructureImm imm)
		{
			++unreachableControlDepth;
		}
		void catch_(CatchImm imm)
		{
			if(!unreachableControlDepth) { context.catch_(imm); }
		}
		void catch_all(NoImm imm)
		{
			if(!unreachableControlDepth) { context.catch_all(imm); }
		}

	private:
		EmitFunctionContext& context;
		Uptr unreachableControlDepth;
	};

	void EmitFunctionContext::emit()
	{
		// Create debug info for the function.
		llvm::SmallVector<llvm::Metadata*,10> diFunctionParameterTypes;
		for(auto parameterType : functionType->parameters) { diFunctionParameterTypes.push_back(moduleContext.diValueTypes[(Uptr)parameterType]); }
		auto diFunctionType = moduleContext.diBuilder->createSubroutineType(moduleContext.diBuilder->getOrCreateTypeArray(diFunctionParameterTypes));
		diFunction = moduleContext.diBuilder->createFunction(
			moduleContext.diModuleScope,
			functionInstance->debugName,
			llvmFunction->getName(),
			moduleContext.diModuleScope,
			0,
			diFunctionType,
			false,
			true,
			0);
		llvmFunction->setSubprogram(diFunction);

		// Create the return basic block, and push the root control context for the function.
		auto returnBlock = llvm::BasicBlock::Create(*llvmContext,"return",llvmFunction);
		auto returnPHI = createPHI(returnBlock,functionType->ret);
		pushControlStack(ControlContext::Type::function,functionType->ret,returnBlock,returnPHI);
		pushBranchTarget(functionType->ret,returnBlock,returnPHI);

		// Create an initial basic block for the function.
		auto entryBasicBlock = llvm::BasicBlock::Create(*llvmContext,"entry",llvmFunction);
		irBuilder.SetInsertPoint(entryBasicBlock);

		// Create and initialize allocas for the memory and table base parameters.
		auto llvmArgIt = llvmFunction->arg_begin();
		memoryBasePointerVariable = irBuilder.CreateAlloca(llvmI8PtrType,nullptr,"memoryBase");
		tableBasePointerVariable = irBuilder.CreateAlloca(llvmI8PtrType,nullptr,"tableBase");
		contextPointerVariable = irBuilder.CreateAlloca(llvmI8PtrType,nullptr,"context");
		irBuilder.CreateStore(&*llvmArgIt++,contextPointerVariable);
		reloadMemoryAndTableBase();

		// Create and initialize allocas for all the locals and parameters.
		for(Uptr localIndex = 0;localIndex < functionType->parameters.size() + functionDef.nonParameterLocalTypes.size();++localIndex)
		{
			auto localType = localIndex < functionType->parameters.size()
				? functionType->parameters[localIndex]
				: functionDef.nonParameterLocalTypes[localIndex - functionType->parameters.size()];
			auto localPointer = irBuilder.CreateAlloca(asLLVMType(localType),nullptr,"");
			localPointers.push_back(localPointer);

			if(localIndex < functionType->parameters.size())
			{
				// Copy the parameter value into the local that stores it.
				irBuilder.CreateStore(&*llvmArgIt,localPointer);
				++llvmArgIt;
			}
			else
			{
				// Initialize non-parameter locals to zero.
				irBuilder.CreateStore(typedZeroConstants[(Uptr)localType],localPointer);
			}
		}

		// If enabled, emit a call to the WAVM function enter hook (for debugging).
		if(ENABLE_FUNCTION_ENTER_EXIT_HOOKS)
		{
			emitRuntimeIntrinsic(
				"debugEnterFunction",
				FunctionType::get(ResultType::none,{ValueType::i64}),
				{emitLiteral(reinterpret_cast<U64>(functionInstance))}
				);
		}

		// Decode the WebAssembly opcodes and emit LLVM IR for them.
		OperatorDecoderStream decoder(functionDef.code);
		UnreachableOpVisitor unreachableOpVisitor(*this);
		OperatorPrinter operatorPrinter(module,functionDef);
		Uptr opIndex = 0;
		while(decoder && controlStack.size())
		{
			irBuilder.SetCurrentDebugLocation(llvm::DILocation::get(*llvmContext,(unsigned int)opIndex++,0,diFunction));
			if(ENABLE_LOGGING)
			{
				logOperator(decoder.decodeOpWithoutConsume(operatorPrinter));
			}

			if(controlStack.back().isReachable) { decoder.decodeOp(*this); }
			else { decoder.decodeOp(unreachableOpVisitor); }
		};
		wavmAssert(irBuilder.GetInsertBlock() == returnBlock);
		
		// If enabled, emit a call to the WAVM function enter hook (for debugging).
		if(ENABLE_FUNCTION_ENTER_EXIT_HOOKS)
		{
			emitRuntimeIntrinsic(
				"debugExitFunction",
				FunctionType::get(ResultType::none,{ValueType::i64}),
				{emitLiteral(reinterpret_cast<U64>(functionInstance))}
				);
		}

		// Emit the function return.
		llvm::Value* returnStruct = getZeroedLLVMReturnStruct(functionType->ret);
		returnStruct = irBuilder.CreateInsertValue(returnStruct,irBuilder.CreateLoad(contextPointerVariable),{0});
		if(functionType->ret != ResultType::none) { returnStruct = irBuilder.CreateInsertValue(returnStruct,pop(),{1}); }

		irBuilder.CreateRet(returnStruct);

		// If a local escape block was created, add a localescape intrinsic to it with the accumulated
		// local escape allocas, and insert it before the function's entry block.
		if(localEscapeBlock)
		{
			irBuilder.SetInsertPoint(localEscapeBlock);
			callLLVMIntrinsic({},llvm::Intrinsic::localescape,pendingLocalEscapes);
			irBuilder.CreateBr(&llvmFunction->getEntryBlock());
			localEscapeBlock->moveBefore(&llvmFunction->getEntryBlock());
		}
	}

	std::shared_ptr<llvm::Module> EmitModuleContext::emit()
	{
		Timing::Timer emitTimer;

		// Create an external reference to the appropriate exception personality function.
		auto personalityFunction = llvm::Function::Create(
			llvm::FunctionType::get(llvmI32Type,{},false),
			llvm::GlobalValue::LinkageTypes::ExternalLinkage,
			#ifdef _WIN32
				"__C_specific_handler",
			#else
				"__gxx_personality_v0",
			#endif
			llvmModule
			);

		// Create the LLVM functions.
		functionDefs.resize(module.functions.defs.size());
		for(Uptr functionDefIndex = 0;functionDefIndex < module.functions.defs.size();++functionDefIndex)
		{
			const FunctionType* functionType = module.types[module.functions.defs[functionDefIndex].type.index];
			auto llvmFunctionType = asLLVMType(functionType,CallingConvention::wasm);
			auto externalName = getExternalFunctionName(moduleInstance,functionDefIndex);
			functionDefs[functionDefIndex] = llvm::Function::Create(llvmFunctionType,llvm::Function::ExternalLinkage,externalName,llvmModule);
			functionDefs[functionDefIndex]->setPersonalityFn(personalityFunction);
			functionDefs[functionDefIndex]->setCallingConv(asLLVMCallingConv(CallingConvention::wasm));
		}

		// Compile each function in the module.
		for(Uptr functionDefIndex = 0;functionDefIndex < module.functions.defs.size();++functionDefIndex)
		{ EmitFunctionContext(*this,module,module.functions.defs[functionDefIndex],moduleInstance->functionDefs[functionDefIndex],functionDefs[functionDefIndex]).emit(); }
		
		// Finalize the debug info.
		diBuilder->finalize();

		Timing::logRatePerSecond("Emitted LLVM IR",emitTimer,(F64)llvmModule->size(),"functions");

		return llvmModuleSharedPtr;
	}

	std::shared_ptr<llvm::Module> emitModule(const Module& module,ModuleInstance* moduleInstance)
	{
		return EmitModuleContext(module,moduleInstance).emit();
	}
}
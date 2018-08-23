#pragma once

#include "LLVMEmitModuleContext.h"
#include "LLVMJIT.h"

#include "LLVMPreInclude.h"

#include "llvm/IR/Intrinsics.h"

#include "LLVMPostInclude.h"

namespace LLVMJIT
{
	struct EmitFunctionContext : EmitContext
	{
		typedef void Result;

		struct EmitModuleContext& moduleContext;
		const IR::Module& module;
		const IR::FunctionDef& functionDef;
		IR::FunctionType functionType;
		Runtime::FunctionInstance* functionInstance;
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
			PHIVector endPHIs;
			llvm::BasicBlock* elseBlock;
			ValueVector elseArgs;
			IR::TypeTuple resultTypes;
			Uptr outerStackSize;
			Uptr outerBranchTargetStackSize;
			bool isReachable;
		};

		struct BranchTarget
		{
			TypeTuple params;
			llvm::BasicBlock* block;
			PHIVector phis;
		};

		std::vector<ControlContext> controlStack;
		std::vector<BranchTarget> branchTargetStack;
		std::vector<llvm::Value*> stack;

		EmitFunctionContext(EmitModuleContext& inModuleContext,
							const Module& inModule,
							const FunctionDef& inFunctionDef,
							FunctionInstance* inFunctionInstance,
							llvm::Function* inLLVMFunction)
		: EmitContext(inModuleContext.moduleInstance->defaultMemory,
					  inModuleContext.moduleInstance->defaultTable)
		, moduleContext(inModuleContext)
		, module(inModule)
		, functionDef(inFunctionDef)
		, functionType(inModule.types[inFunctionDef.type.index])
		, functionInstance(inFunctionInstance)
		, llvmFunction(inLLVMFunction)
		, localEscapeBlock(nullptr)
		{
		}

		void emit();

		// Operand stack manipulation
		llvm::Value* pop()
		{
			wavmAssert(stack.size() - (controlStack.size() ? controlStack.back().outerStackSize : 0)
					   >= 1);
			llvm::Value* result = stack.back();
			stack.pop_back();
			return result;
		}

		void popMultiple(llvm::Value** outValues, Uptr num)
		{
			wavmAssert(stack.size() - (controlStack.size() ? controlStack.back().outerStackSize : 0)
					   >= num);
			std::copy(stack.end() - num, stack.end(), outValues);
			stack.resize(stack.size() - num);
		}

		llvm::Value* getValueFromTop(Uptr offset = 0) const
		{
			return stack[stack.size() - offset - 1];
		}

		void push(llvm::Value* value) { stack.push_back(value); }

		void pushMultiple(llvm::Value** values, Uptr numValues)
		{
			for(Uptr valueIndex = 0; valueIndex < numValues; ++valueIndex)
			{ push(values[valueIndex]); }
		}

		// Creates a PHI node for the argument of branches to a basic block.
		PHIVector createPHIs(llvm::BasicBlock* basicBlock, TypeTuple type);

		// Bitcasts a LLVM value to a canonical type for the corresponding WebAssembly type.
		// This is currently just used to map all the various vector types to a canonical type for
		// the vector width.
		llvm::Value* coerceToCanonicalType(llvm::Value* value);

		// Debug logging.
		void logOperator(const std::string& operatorDescription);

		// Coerces an I32 value to an I1, and vice-versa.
		llvm::Value* coerceI32ToBool(llvm::Value* i32Value)
		{
			return irBuilder.CreateICmpNE(i32Value, typedZeroConstants[(Uptr)ValueType::i32]);
		}

		llvm::Value* coerceBoolToI32(llvm::Value* boolValue)
		{
			return zext(boolValue, llvmI32Type);
		}

		// Bounds checks and converts a memory operation I32 address operand to a LLVM pointer.
		llvm::Value* coerceByteIndexToPointer(llvm::Value* byteIndex,
											  U32 offset,
											  llvm::Type* memoryType);

		// Traps a divide-by-zero
		void trapDivideByZero(ValueType type, llvm::Value* divisor);

		// Traps on (x / 0) or (INT_MIN / -1).
		void trapDivideByZeroOrIntegerOverflow(ValueType type,
											   llvm::Value* left,
											   llvm::Value* right);

		llvm::Value* callLLVMIntrinsic(const std::initializer_list<llvm::Type*>& typeArguments,
									   llvm::Intrinsic::ID id,
									   llvm::ArrayRef<llvm::Value*> arguments)
		{
			return irBuilder.CreateCall(moduleContext.getLLVMIntrinsic(typeArguments, id),
										arguments);
		}

		// Emits a call to a WAVM intrinsic function.
		ValueVector emitRuntimeIntrinsic(const char* intrinsicName,
										 FunctionType intrinsicType,
										 const std::initializer_list<llvm::Value*>& args);

		// A helper function to emit a conditional call to a non-returning intrinsic function.
		void emitConditionalTrapIntrinsic(llvm::Value* booleanCondition,
										  const char* intrinsicName,
										  FunctionType intrinsicType,
										  const std::initializer_list<llvm::Value*>& args);

		void pushControlStack(ControlContext::Type type,
							  TypeTuple resultTypes,
							  llvm::BasicBlock* endBlock,
							  const PHIVector& endPHIs,
							  llvm::BasicBlock* elseBlock = nullptr,
							  const ValueVector& elseArgs = {});

		void pushBranchTarget(TypeTuple branchArgumentType,
							  llvm::BasicBlock* branchTargetBlock,
							  const PHIVector& branchTargetPHIs);

		void branchToEndOfControlContext();

		BranchTarget& getBranchTargetByDepth(Uptr depth)
		{
			wavmAssert(depth < branchTargetStack.size());
			return branchTargetStack[branchTargetStack.size() - depth - 1];
		}

		// This is called after unconditional control flow to indicate that operators following it
		// are unreachable until the control stack is popped.
		void enterUnreachable();

		llvm::Value* identity(llvm::Value* value, llvm::Type* type) { return value; }

		llvm::Value* sext(llvm::Value* value, llvm::Type* type)
		{
			return irBuilder.CreateSExt(value, type);
		}

		llvm::Value* zext(llvm::Value* value, llvm::Type* type)
		{
			return irBuilder.CreateZExt(value, type);
		}

		llvm::Value* trunc(llvm::Value* value, llvm::Type* type)
		{
			return irBuilder.CreateTrunc(value, type);
		}

		llvm::Value* emitSRem(ValueType type, llvm::Value* left, llvm::Value* right);
		llvm::Value* emitRotl(ValueType type, llvm::Value* left, llvm::Value* right);
		llvm::Value* emitRotr(ValueType type, llvm::Value* left, llvm::Value* right);
		llvm::Value* emitF64Promote(llvm::Value* operand);

		template<typename Float>
		llvm::Value* emitTruncFloatToInt(ValueType destType,
										 bool isSigned,
										 Float minBounds,
										 Float maxBounds,
										 llvm::Value* operand);

		template<typename Int, typename Float>
		llvm::Value* emitTruncFloatToIntSat(llvm::Type* destType,
											bool isSigned,
											Float minFloatBounds,
											Float maxFloatBounds,
											Int minIntBounds,
											Int maxIntBounds,
											llvm::Value* operand);

		template<typename Int, typename Float, Uptr numElements>
		llvm::Value* emitTruncVectorFloatToIntSat(llvm::Type* destType,
												  bool isSigned,
												  Float minFloatBounds,
												  Float maxFloatBounds,
												  Int minIntBounds,
												  Int maxIntBounds,
												  Int nanResult,
												  llvm::Value* operand);

		llvm::Value* emitBitSelect(llvm::Value* mask,
								   llvm::Value* trueValue,
								   llvm::Value* falseValue);

		llvm::Value* emitVectorSelect(llvm::Value* condition,
									  llvm::Value* trueValue,
									  llvm::Value* falseValue);

		void trapIfMisalignedAtomic(llvm::Value* address, U32 naturalAlignmentLog2);

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

		void endTry();
		void endCatch();

		llvm::BasicBlock* getInnermostUnwindToBlock();

		void emitThrow(llvm::Value* exceptionTypeInstanceI64,
					   llvm::Value* argumentsPointerI64,
					   bool isUserException);

#define VISIT_OPCODE(encoding, name, nameString, Imm, ...) void name(Imm imm);
		ENUM_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE

		void unknown(Opcode opcode) { Errors::unreachable(); }
	};
}

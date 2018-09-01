#include "IR/Operators.h"
#include "IR/Types.h"
#include "Inline/Assert.h"
#include "LLVMEmitFunctionContext.h"
#include "LLVMEmitModuleContext.h"
#include "LLVMEmitWorkarounds.h"
#include "LLVMJITPrivate.h"

using namespace IR;
using namespace LLVMJIT;

#define EMIT_UNARY_OP(name, emitCode)                                                              \
	void EmitFunctionContext::name(NoImm)                                                          \
	{                                                                                              \
		auto operand = pop();                                                                      \
		push(emitCode);                                                                            \
	}

EMIT_UNARY_OP(i32_wrap_i64, trunc(operand, llvmI32Type))
EMIT_UNARY_OP(i64_extend_s_i32, sext(operand, llvmI64Type))
EMIT_UNARY_OP(i64_extend_u_i32, zext(operand, llvmI64Type))

EMIT_UNARY_OP(f32_convert_s_i32, irBuilder.CreateSIToFP(operand, llvmF32Type))
EMIT_UNARY_OP(f64_convert_s_i32, irBuilder.CreateSIToFP(operand, llvmF64Type))
EMIT_UNARY_OP(f32_convert_s_i64, irBuilder.CreateSIToFP(operand, llvmF32Type))
EMIT_UNARY_OP(f64_convert_s_i64, irBuilder.CreateSIToFP(operand, llvmF64Type))
EMIT_UNARY_OP(f32_convert_u_i32, irBuilder.CreateUIToFP(operand, llvmF32Type))
EMIT_UNARY_OP(f64_convert_u_i32, irBuilder.CreateUIToFP(operand, llvmF64Type))
EMIT_UNARY_OP(f32_convert_u_i64, irBuilder.CreateUIToFP(operand, llvmF32Type))
EMIT_UNARY_OP(f64_convert_u_i64, irBuilder.CreateUIToFP(operand, llvmF64Type))

EMIT_UNARY_OP(f32x4_convert_s_i32x4,
			  irBuilder.CreateSIToFP(irBuilder.CreateBitCast(operand, llvmI32x4Type),
									 llvmF32x4Type));
EMIT_UNARY_OP(f32x4_convert_u_i32x4,
			  irBuilder.CreateUIToFP(irBuilder.CreateBitCast(operand, llvmI32x4Type),
									 llvmF32x4Type));
EMIT_UNARY_OP(f64x2_convert_s_i64x2,
			  irBuilder.CreateSIToFP(irBuilder.CreateBitCast(operand, llvmI64x2Type),
									 llvmF64x2Type));
EMIT_UNARY_OP(f64x2_convert_u_i64x2,
			  irBuilder.CreateUIToFP(irBuilder.CreateBitCast(operand, llvmI64x2Type),
									 llvmF64x2Type));

EMIT_UNARY_OP(f32_demote_f64, irBuilder.CreateFPTrunc(operand, llvmF32Type))
EMIT_UNARY_OP(f64_promote_f32, emitF64Promote(operand))
EMIT_UNARY_OP(f32_reinterpret_i32, irBuilder.CreateBitCast(operand, llvmF32Type))
EMIT_UNARY_OP(f64_reinterpret_i64, irBuilder.CreateBitCast(operand, llvmF64Type))
EMIT_UNARY_OP(i32_reinterpret_f32, irBuilder.CreateBitCast(operand, llvmI32Type))
EMIT_UNARY_OP(i64_reinterpret_f64, irBuilder.CreateBitCast(operand, llvmI64Type))

llvm::Value* EmitFunctionContext::emitF64Promote(llvm::Value* operand)
{
	// Emit an nop experimental.constrained.fadd intrinsic on the result of the promote to make sure
	// the promote can't be optimized away.
	llvm::Value* f64Operand = irBuilder.CreateFPExt(operand, llvmF64Type);
	return callLLVMIntrinsic({llvmF64Type},
							 llvm::Intrinsic::experimental_constrained_fmul,
							 {f64Operand,
							  emitLiteral(F64(1.0)),
							  moduleContext.fpRoundingModeMetadata,
							  moduleContext.fpExceptionMetadata});
}

template<typename Float>
llvm::Value* EmitFunctionContext::emitTruncFloatToInt(ValueType destType,
													  bool isSigned,
													  Float minBounds,
													  Float maxBounds,
													  llvm::Value* operand)
{
	auto nanBlock = llvm::BasicBlock::Create(*llvmContext, "FPToInt_nan", llvmFunction);
	auto notNaNBlock = llvm::BasicBlock::Create(*llvmContext, "FPToInt_notNaN", llvmFunction);
	auto overflowBlock = llvm::BasicBlock::Create(*llvmContext, "FPToInt_overflow", llvmFunction);
	auto noOverflowBlock
		= llvm::BasicBlock::Create(*llvmContext, "FPToInt_noOverflow", llvmFunction);

	auto isNaN = createFCmpWithWorkaround(irBuilder, llvm::CmpInst::FCMP_UNO, operand, operand);
	irBuilder.CreateCondBr(isNaN, nanBlock, notNaNBlock, moduleContext.likelyFalseBranchWeights);

	irBuilder.SetInsertPoint(nanBlock);
	emitRuntimeIntrinsic("invalidFloatOperationTrap", FunctionType(), {});
	irBuilder.CreateUnreachable();

	irBuilder.SetInsertPoint(notNaNBlock);
	auto isOverflow = irBuilder.CreateOr(irBuilder.CreateFCmpOGE(operand, emitLiteral(maxBounds)),
										 irBuilder.CreateFCmpOLE(operand, emitLiteral(minBounds)));
	irBuilder.CreateCondBr(
		isOverflow, overflowBlock, noOverflowBlock, moduleContext.likelyFalseBranchWeights);

	irBuilder.SetInsertPoint(overflowBlock);
	emitRuntimeIntrinsic("divideByZeroOrIntegerOverflowTrap", FunctionType(), {});
	irBuilder.CreateUnreachable();

	irBuilder.SetInsertPoint(noOverflowBlock);
	return isSigned ? irBuilder.CreateFPToSI(operand, asLLVMType(destType))
					: irBuilder.CreateFPToUI(operand, asLLVMType(destType));
}

// We want the widest floating point bounds that can't be truncated to an integer.
// This isn't simply the min/max integer values converted to float, but the next greater(or lesser)
// float that would be truncated to an integer out of range of the target type.

EMIT_UNARY_OP(
	i32_trunc_s_f32,
	emitTruncFloatToInt<F32>(ValueType::i32, true, -2147483904.0f, 2147483648.0f, operand))
EMIT_UNARY_OP(i32_trunc_s_f64,
			  emitTruncFloatToInt<F64>(ValueType::i32, true, -2147483649.0, 2147483648.0, operand))
EMIT_UNARY_OP(i32_trunc_u_f32,
			  emitTruncFloatToInt<F32>(ValueType::i32, false, -1.0f, 4294967296.0f, operand))
EMIT_UNARY_OP(i32_trunc_u_f64,
			  emitTruncFloatToInt<F64>(ValueType::i32, false, -1.0, 4294967296.0, operand))

EMIT_UNARY_OP(i64_trunc_s_f32,
			  emitTruncFloatToInt<F32>(ValueType::i64,
									   true,
									   -9223373136366403584.0f,
									   9223372036854775808.0f,
									   operand))
EMIT_UNARY_OP(i64_trunc_s_f64,
			  emitTruncFloatToInt<F64>(ValueType::i64,
									   true,
									   -9223372036854777856.0,
									   9223372036854775808.0,
									   operand))
EMIT_UNARY_OP(
	i64_trunc_u_f32,
	emitTruncFloatToInt<F32>(ValueType::i64, false, -1.0f, 18446744073709551616.0f, operand))
EMIT_UNARY_OP(
	i64_trunc_u_f64,
	emitTruncFloatToInt<F64>(ValueType::i64, false, -1.0, 18446744073709551616.0, operand))

template<typename Int, typename Float>
llvm::Value* EmitFunctionContext::emitTruncFloatToIntSat(llvm::Type* destType,
														 bool isSigned,
														 Float minFloatBounds,
														 Float maxFloatBounds,
														 Int minIntBounds,
														 Int maxIntBounds,
														 llvm::Value* operand)
{
	llvm::Value* result = isSigned ? irBuilder.CreateFPToSI(operand, destType)
								   : irBuilder.CreateFPToUI(operand, destType);

	result = irBuilder.CreateSelect(irBuilder.CreateFCmpOGE(operand, emitLiteral(maxFloatBounds)),
									emitLiteral(maxIntBounds),
									result);
	result = irBuilder.CreateSelect(irBuilder.CreateFCmpOLE(operand, emitLiteral(minFloatBounds)),
									emitLiteral(minIntBounds),
									result);
	result = irBuilder.CreateSelect(
		createFCmpWithWorkaround(irBuilder, llvm::CmpInst::FCMP_UNO, operand, operand),
		emitLiteral(Int(0)),
		result);

	return result;
}

EMIT_UNARY_OP(i32_trunc_s_sat_f32,
			  emitTruncFloatToIntSat(llvmI32Type,
									 true,
									 F32(INT32_MIN),
									 F32(INT32_MAX),
									 INT32_MIN,
									 INT32_MAX,
									 operand))
EMIT_UNARY_OP(i32_trunc_s_sat_f64,
			  emitTruncFloatToIntSat(llvmI32Type,
									 true,
									 F64(INT32_MIN),
									 F64(INT32_MAX),
									 INT32_MIN,
									 INT32_MAX,
									 operand))
EMIT_UNARY_OP(
	i32_trunc_u_sat_f32,
	emitTruncFloatToIntSat(llvmI32Type, false, 0.0f, F32(UINT32_MAX), U32(0), UINT32_MAX, operand))
EMIT_UNARY_OP(
	i32_trunc_u_sat_f64,
	emitTruncFloatToIntSat(llvmI32Type, false, 0.0, F64(UINT32_MAX), U32(0), UINT32_MAX, operand))
EMIT_UNARY_OP(i64_trunc_s_sat_f32,
			  emitTruncFloatToIntSat(llvmI64Type,
									 true,
									 F32(INT64_MIN),
									 F32(INT64_MAX),
									 INT64_MIN,
									 INT64_MAX,
									 operand))
EMIT_UNARY_OP(i64_trunc_s_sat_f64,
			  emitTruncFloatToIntSat(llvmI64Type,
									 true,
									 F64(INT64_MIN),
									 F64(INT64_MAX),
									 INT64_MIN,
									 INT64_MAX,
									 operand))
EMIT_UNARY_OP(
	i64_trunc_u_sat_f32,
	emitTruncFloatToIntSat(llvmI64Type, false, 0.0f, F32(UINT64_MAX), U64(0), UINT64_MAX, operand))
EMIT_UNARY_OP(
	i64_trunc_u_sat_f64,
	emitTruncFloatToIntSat(llvmI64Type, false, 0.0, F64(UINT64_MAX), U64(0), UINT64_MAX, operand))

template<typename Int, typename Float, Uptr numElements>
llvm::Value* EmitFunctionContext::emitTruncVectorFloatToIntSat(llvm::Type* destType,
															   bool isSigned,
															   Float minFloatBounds,
															   Float maxFloatBounds,
															   Int minIntBounds,
															   Int maxIntBounds,
															   Int nanResult,
															   llvm::Value* operand)
{
	auto result = isSigned ? irBuilder.CreateFPToSI(operand, destType)
						   : irBuilder.CreateFPToUI(operand, destType);

	auto minFloatBoundsVec = irBuilder.CreateVectorSplat(numElements, emitLiteral(minFloatBounds));
	auto maxFloatBoundsVec = irBuilder.CreateVectorSplat(numElements, emitLiteral(maxFloatBounds));

	result = emitVectorSelect(irBuilder.CreateFCmpOGE(operand, maxFloatBoundsVec),
							  irBuilder.CreateVectorSplat(numElements, emitLiteral(maxIntBounds)),
							  result);
	result = emitVectorSelect(irBuilder.CreateFCmpOLE(operand, minFloatBoundsVec),
							  irBuilder.CreateVectorSplat(numElements, emitLiteral(minIntBounds)),
							  result);
	result = emitVectorSelect(
		createFCmpWithWorkaround(irBuilder, llvm::CmpInst::FCMP_UNO, operand, operand),
		irBuilder.CreateVectorSplat(numElements, emitLiteral(nanResult)),
		result);
	return result;
}

EMIT_UNARY_OP(i32x4_trunc_s_sat_f32x4,
			  (emitTruncVectorFloatToIntSat<I32, F32, 4>)(llvmI32x4Type,
														  true,
														  F32(INT32_MIN),
														  F32(INT32_MAX),
														  INT32_MIN,
														  INT32_MAX,
														  I32(0),
														  irBuilder.CreateBitCast(operand,
																				  llvmF32x4Type)))
EMIT_UNARY_OP(i32x4_trunc_u_sat_f32x4,
			  (emitTruncVectorFloatToIntSat<U32, F32, 4>)(llvmI32x4Type,
														  false,
														  0.0f,
														  F32(UINT32_MAX),
														  U32(0),
														  UINT32_MAX,
														  U32(0),
														  irBuilder.CreateBitCast(operand,
																				  llvmF32x4Type)))
EMIT_UNARY_OP(i64x2_trunc_s_sat_f64x2,
			  (emitTruncVectorFloatToIntSat<I64, F64, 2>)(llvmI64x2Type,
														  true,
														  F64(INT64_MIN),
														  F64(INT64_MAX),
														  INT64_MIN,
														  INT64_MAX,
														  I64(0),
														  irBuilder.CreateBitCast(operand,
																				  llvmF64x2Type)))
EMIT_UNARY_OP(i64x2_trunc_u_sat_f64x2,
			  (emitTruncVectorFloatToIntSat<U64, F64, 2>)(llvmI64x2Type,
														  false,
														  0.0,
														  F64(UINT64_MAX),
														  U64(0),
														  UINT64_MAX,
														  U64(0),
														  irBuilder.CreateBitCast(operand,
																				  llvmF64x2Type)))

EMIT_UNARY_OP(i32_extend8_s, sext(trunc(operand, llvmI8Type), llvmI32Type))
EMIT_UNARY_OP(i32_extend16_s, sext(trunc(operand, llvmI16Type), llvmI32Type))
EMIT_UNARY_OP(i64_extend8_s, sext(trunc(operand, llvmI8Type), llvmI64Type))
EMIT_UNARY_OP(i64_extend16_s, sext(trunc(operand, llvmI16Type), llvmI64Type))
EMIT_UNARY_OP(i64_extend32_s, sext(trunc(operand, llvmI32Type), llvmI64Type))

#define EMIT_SIMD_SPLAT(vectorType, coerceScalar, numLanes)                                        \
	void EmitFunctionContext::vectorType##_splat(IR::NoImm)                                        \
	{                                                                                              \
		auto scalar = pop();                                                                       \
		push(irBuilder.CreateVectorSplat(numLanes, coerceScalar));                                 \
	}
EMIT_SIMD_SPLAT(i8x16, trunc(scalar, llvmI8Type), 16)
EMIT_SIMD_SPLAT(i16x8, trunc(scalar, llvmI16Type), 8)
EMIT_SIMD_SPLAT(i32x4, scalar, 4)
EMIT_SIMD_SPLAT(i64x2, scalar, 2)
EMIT_SIMD_SPLAT(f32x4, scalar, 4)
EMIT_SIMD_SPLAT(f64x2, scalar, 2)

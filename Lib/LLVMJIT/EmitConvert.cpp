#include <stdint.h>
#include "EmitFunctionContext.h"
#include "EmitModuleContext.h"
#include "EmitWorkarounds.h"
#include "LLVMJITPrivate.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/BasicTypes.h"

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Intrinsics.h>
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

namespace llvm {
	class Type;
	class Value;
}

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::LLVMJIT;

#define EMIT_UNARY_OP(name, emitCode)                                                              \
	void EmitFunctionContext::name(NoImm)                                                          \
	{                                                                                              \
		auto operand = pop();                                                                      \
		push(emitCode);                                                                            \
	}

EMIT_UNARY_OP(i32_wrap_i64, trunc(operand, llvmContext.i32Type))
EMIT_UNARY_OP(i64_extend_i32_s, sext(operand, llvmContext.i64Type))
EMIT_UNARY_OP(i64_extend_i32_u, zext(operand, llvmContext.i64Type))

EMIT_UNARY_OP(f32_convert_i32_s, irBuilder.CreateSIToFP(operand, llvmContext.f32Type))
EMIT_UNARY_OP(f64_convert_i32_s, irBuilder.CreateSIToFP(operand, llvmContext.f64Type))
EMIT_UNARY_OP(f32_convert_i64_s, irBuilder.CreateSIToFP(operand, llvmContext.f32Type))
EMIT_UNARY_OP(f64_convert_i64_s, irBuilder.CreateSIToFP(operand, llvmContext.f64Type))
EMIT_UNARY_OP(f32_convert_i32_u, irBuilder.CreateUIToFP(operand, llvmContext.f32Type))
EMIT_UNARY_OP(f64_convert_i32_u, irBuilder.CreateUIToFP(operand, llvmContext.f64Type))
EMIT_UNARY_OP(f32_convert_i64_u, irBuilder.CreateUIToFP(operand, llvmContext.f32Type))
EMIT_UNARY_OP(f64_convert_i64_u, irBuilder.CreateUIToFP(operand, llvmContext.f64Type))

EMIT_UNARY_OP(f32x4_convert_i32x4_s,
			  irBuilder.CreateSIToFP(irBuilder.CreateBitCast(operand, llvmContext.i32x4Type),
									 llvmContext.f32x4Type));
EMIT_UNARY_OP(f32x4_convert_i32x4_u,
			  irBuilder.CreateUIToFP(irBuilder.CreateBitCast(operand, llvmContext.i32x4Type),
									 llvmContext.f32x4Type));
EMIT_UNARY_OP(f64x2_convert_i64x2_s,
			  irBuilder.CreateSIToFP(irBuilder.CreateBitCast(operand, llvmContext.i64x2Type),
									 llvmContext.f64x2Type));
EMIT_UNARY_OP(f64x2_convert_i64x2_u,
			  irBuilder.CreateUIToFP(irBuilder.CreateBitCast(operand, llvmContext.i64x2Type),
									 llvmContext.f64x2Type));

EMIT_UNARY_OP(f32_demote_f64, irBuilder.CreateFPTrunc(operand, llvmContext.f32Type))
EMIT_UNARY_OP(f64_promote_f32, emitF64Promote(operand))
EMIT_UNARY_OP(f32_reinterpret_i32, irBuilder.CreateBitCast(operand, llvmContext.f32Type))
EMIT_UNARY_OP(f64_reinterpret_i64, irBuilder.CreateBitCast(operand, llvmContext.f64Type))
EMIT_UNARY_OP(i32_reinterpret_f32, irBuilder.CreateBitCast(operand, llvmContext.i32Type))
EMIT_UNARY_OP(i64_reinterpret_f64, irBuilder.CreateBitCast(operand, llvmContext.i64Type))

llvm::Value* EmitFunctionContext::emitF64Promote(llvm::Value* operand)
{
	// Emit an nop experimental.constrained.fadd intrinsic on the result of the promote to make sure
	// the promote can't be optimized away.
	llvm::Value* f64Operand = irBuilder.CreateFPExt(operand, llvmContext.f64Type);
	return callLLVMIntrinsic({llvmContext.f64Type},
							 llvm::Intrinsic::experimental_constrained_fmul,
							 {f64Operand,
							  emitLiteral(llvmContext, F64(1.0)),
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
	auto nanBlock = llvm::BasicBlock::Create(llvmContext, "FPToInt_nan", function);
	auto notNaNBlock = llvm::BasicBlock::Create(llvmContext, "FPToInt_notNaN", function);
	auto overflowBlock = llvm::BasicBlock::Create(llvmContext, "FPToInt_overflow", function);
	auto noOverflowBlock = llvm::BasicBlock::Create(llvmContext, "FPToInt_noOverflow", function);

	auto isNaN = createFCmpWithWorkaround(irBuilder, llvm::CmpInst::FCMP_UNO, operand, operand);
	irBuilder.CreateCondBr(isNaN, nanBlock, notNaNBlock, moduleContext.likelyFalseBranchWeights);

	irBuilder.SetInsertPoint(nanBlock);
	emitRuntimeIntrinsic(
		"invalidFloatOperationTrap", FunctionType({}, {}, IR::CallingConvention::intrinsic), {});
	irBuilder.CreateUnreachable();

	irBuilder.SetInsertPoint(notNaNBlock);
	auto isOverflow
		= irBuilder.CreateOr(irBuilder.CreateFCmpOGE(operand, emitLiteral(llvmContext, maxBounds)),
							 irBuilder.CreateFCmpOLE(operand, emitLiteral(llvmContext, minBounds)));
	irBuilder.CreateCondBr(
		isOverflow, overflowBlock, noOverflowBlock, moduleContext.likelyFalseBranchWeights);

	irBuilder.SetInsertPoint(overflowBlock);
	emitRuntimeIntrinsic("divideByZeroOrIntegerOverflowTrap",
						 FunctionType({}, {}, IR::CallingConvention::intrinsic),
						 {});
	irBuilder.CreateUnreachable();

	irBuilder.SetInsertPoint(noOverflowBlock);
	return isSigned ? irBuilder.CreateFPToSI(operand, asLLVMType(llvmContext, destType))
					: irBuilder.CreateFPToUI(operand, asLLVMType(llvmContext, destType));
}

// We want the widest floating point bounds that can't be truncated to an integer.
// This isn't simply the min/max integer values converted to float, but the next greater(or lesser)
// float that would be truncated to an integer out of range of the target type.

EMIT_UNARY_OP(
	i32_trunc_f32_s,
	emitTruncFloatToInt<F32>(ValueType::i32, true, -2147483904.0f, 2147483648.0f, operand))
EMIT_UNARY_OP(i32_trunc_f64_s,
			  emitTruncFloatToInt<F64>(ValueType::i32, true, -2147483649.0, 2147483648.0, operand))
EMIT_UNARY_OP(i32_trunc_f32_u,
			  emitTruncFloatToInt<F32>(ValueType::i32, false, -1.0f, 4294967296.0f, operand))
EMIT_UNARY_OP(i32_trunc_f64_u,
			  emitTruncFloatToInt<F64>(ValueType::i32, false, -1.0, 4294967296.0, operand))

EMIT_UNARY_OP(i64_trunc_f32_s,
			  emitTruncFloatToInt<F32>(ValueType::i64,
									   true,
									   -9223373136366403584.0f,
									   9223372036854775808.0f,
									   operand))
EMIT_UNARY_OP(i64_trunc_f64_s,
			  emitTruncFloatToInt<F64>(ValueType::i64,
									   true,
									   -9223372036854777856.0,
									   9223372036854775808.0,
									   operand))
EMIT_UNARY_OP(
	i64_trunc_f32_u,
	emitTruncFloatToInt<F32>(ValueType::i64, false, -1.0f, 18446744073709551616.0f, operand))
EMIT_UNARY_OP(
	i64_trunc_f64_u,
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

	result = irBuilder.CreateSelect(
		irBuilder.CreateFCmpOGE(operand, emitLiteral(llvmContext, maxFloatBounds)),
		emitLiteral(llvmContext, maxIntBounds),
		result);
	result = irBuilder.CreateSelect(
		irBuilder.CreateFCmpOLE(operand, emitLiteral(llvmContext, minFloatBounds)),
		emitLiteral(llvmContext, minIntBounds),
		result);
	result = irBuilder.CreateSelect(
		createFCmpWithWorkaround(irBuilder, llvm::CmpInst::FCMP_UNO, operand, operand),
		emitLiteral(llvmContext, Int(0)),
		result);

	return result;
}

EMIT_UNARY_OP(i32_trunc_sat_f32_s,
			  emitTruncFloatToIntSat(llvmContext.i32Type,
									 true,
									 F32(INT32_MIN),
									 F32(INT32_MAX),
									 INT32_MIN,
									 INT32_MAX,
									 operand))
EMIT_UNARY_OP(i32_trunc_sat_f64_s,
			  emitTruncFloatToIntSat(llvmContext.i32Type,
									 true,
									 F64(INT32_MIN),
									 F64(INT32_MAX),
									 INT32_MIN,
									 INT32_MAX,
									 operand))
EMIT_UNARY_OP(i32_trunc_sat_f32_u,
			  emitTruncFloatToIntSat(llvmContext.i32Type,
									 false,
									 0.0f,
									 F32(UINT32_MAX),
									 U32(0),
									 UINT32_MAX,
									 operand))
EMIT_UNARY_OP(i32_trunc_sat_f64_u,
			  emitTruncFloatToIntSat(llvmContext.i32Type,
									 false,
									 0.0,
									 F64(UINT32_MAX),
									 U32(0),
									 UINT32_MAX,
									 operand))
EMIT_UNARY_OP(i64_trunc_sat_f32_s,
			  emitTruncFloatToIntSat(llvmContext.i64Type,
									 true,
									 F32(INT64_MIN),
									 F32(INT64_MAX),
									 INT64_MIN,
									 INT64_MAX,
									 operand))
EMIT_UNARY_OP(i64_trunc_sat_f64_s,
			  emitTruncFloatToIntSat(llvmContext.i64Type,
									 true,
									 F64(INT64_MIN),
									 F64(INT64_MAX),
									 INT64_MIN,
									 INT64_MAX,
									 operand))
EMIT_UNARY_OP(i64_trunc_sat_f32_u,
			  emitTruncFloatToIntSat(llvmContext.i64Type,
									 false,
									 0.0f,
									 F32(UINT64_MAX),
									 U64(0),
									 UINT64_MAX,
									 operand))
EMIT_UNARY_OP(i64_trunc_sat_f64_u,
			  emitTruncFloatToIntSat(llvmContext.i64Type,
									 false,
									 0.0,
									 F64(UINT64_MAX),
									 U64(0),
									 UINT64_MAX,
									 operand))

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

	auto minFloatBoundsVec
		= irBuilder.CreateVectorSplat(numElements, emitLiteral(llvmContext, minFloatBounds));
	auto maxFloatBoundsVec
		= irBuilder.CreateVectorSplat(numElements, emitLiteral(llvmContext, maxFloatBounds));

	result = emitVectorSelect(
		irBuilder.CreateFCmpOGE(operand, maxFloatBoundsVec),
		irBuilder.CreateVectorSplat(numElements, emitLiteral(llvmContext, maxIntBounds)),
		result);
	result = emitVectorSelect(
		irBuilder.CreateFCmpOLE(operand, minFloatBoundsVec),
		irBuilder.CreateVectorSplat(numElements, emitLiteral(llvmContext, minIntBounds)),
		result);
	result = emitVectorSelect(
		createFCmpWithWorkaround(irBuilder, llvm::CmpInst::FCMP_UNO, operand, operand),
		irBuilder.CreateVectorSplat(numElements, emitLiteral(llvmContext, nanResult)),
		result);
	return result;
}

EMIT_UNARY_OP(
	i32x4_trunc_sat_f32x4_s,
	(emitTruncVectorFloatToIntSat<I32, F32, 4>)(llvmContext.i32x4Type,
												true,
												F32(INT32_MIN),
												F32(INT32_MAX),
												INT32_MIN,
												INT32_MAX,
												I32(0),
												irBuilder.CreateBitCast(operand,
																		llvmContext.f32x4Type)))
EMIT_UNARY_OP(
	i32x4_trunc_sat_f32x4_u,
	(emitTruncVectorFloatToIntSat<U32, F32, 4>)(llvmContext.i32x4Type,
												false,
												0.0f,
												F32(UINT32_MAX),
												U32(0),
												UINT32_MAX,
												U32(0),
												irBuilder.CreateBitCast(operand,
																		llvmContext.f32x4Type)))
EMIT_UNARY_OP(
	i64x2_trunc_sat_f64x2_s,
	(emitTruncVectorFloatToIntSat<I64, F64, 2>)(llvmContext.i64x2Type,
												true,
												F64(INT64_MIN),
												F64(INT64_MAX),
												INT64_MIN,
												INT64_MAX,
												I64(0),
												irBuilder.CreateBitCast(operand,
																		llvmContext.f64x2Type)))
EMIT_UNARY_OP(
	i64x2_trunc_sat_f64x2_u,
	(emitTruncVectorFloatToIntSat<U64, F64, 2>)(llvmContext.i64x2Type,
												false,
												0.0,
												F64(UINT64_MAX),
												U64(0),
												UINT64_MAX,
												U64(0),
												irBuilder.CreateBitCast(operand,
																		llvmContext.f64x2Type)))

EMIT_UNARY_OP(i32_extend8_s, sext(trunc(operand, llvmContext.i8Type), llvmContext.i32Type))
EMIT_UNARY_OP(i32_extend16_s, sext(trunc(operand, llvmContext.i16Type), llvmContext.i32Type))
EMIT_UNARY_OP(i64_extend8_s, sext(trunc(operand, llvmContext.i8Type), llvmContext.i64Type))
EMIT_UNARY_OP(i64_extend16_s, sext(trunc(operand, llvmContext.i16Type), llvmContext.i64Type))
EMIT_UNARY_OP(i64_extend32_s, sext(trunc(operand, llvmContext.i32Type), llvmContext.i64Type))

#define EMIT_SIMD_SPLAT(vectorType, coerceScalar, numLanes)                                        \
	void EmitFunctionContext::vectorType##_splat(IR::NoImm)                                        \
	{                                                                                              \
		auto scalar = pop();                                                                       \
		push(irBuilder.CreateVectorSplat(numLanes, coerceScalar));                                 \
	}
EMIT_SIMD_SPLAT(i8x16, trunc(scalar, llvmContext.i8Type), 16)
EMIT_SIMD_SPLAT(i16x8, trunc(scalar, llvmContext.i16Type), 8)
EMIT_SIMD_SPLAT(i32x4, scalar, 4)
EMIT_SIMD_SPLAT(i64x2, scalar, 2)
EMIT_SIMD_SPLAT(f32x4, scalar, 4)
EMIT_SIMD_SPLAT(f64x2, scalar, 2)

#define EMIT_SIMD_NARROW(name, sourceType, halfDestType, x86IntrinsicId, aarch64IntrinsicId)       \
	void EmitFunctionContext::name(IR::NoImm)                                                      \
	{                                                                                              \
		auto right = irBuilder.CreateBitCast(pop(), sourceType);                                   \
		auto left = irBuilder.CreateBitCast(pop(), sourceType);                                    \
		const llvm::Triple::ArchType targetArch                                                    \
			= moduleContext.targetMachine->getTargetTriple().getArch();                            \
		if(targetArch == llvm::Triple::x86_64 || targetArch == llvm::Triple::x86)                  \
		{ push(callLLVMIntrinsic({}, x86IntrinsicId, {left, right})); }                            \
		else if(targetArch == llvm::Triple::aarch64)                                               \
		{                                                                                          \
			llvm::Value* halfInput[2]{left, right};                                                \
			llvm::Value* result = llvm::UndefValue::get(llvmContext.i64x2Type);                    \
			for(U64 halfIndex = 0; halfIndex < 2; ++halfIndex)                                     \
			{                                                                                      \
				result = irBuilder.CreateInsertElement(                                            \
					result,                                                                        \
					irBuilder.CreateExtractElement(                                                \
						irBuilder.CreateBitCast(                                                   \
							callLLVMIntrinsic(                                                     \
								{halfDestType}, aarch64IntrinsicId, {halfInput[halfIndex]}),       \
							llvmContext.i64x1Type),                                                \
						U64(0)),                                                                   \
					halfIndex);                                                                    \
			}                                                                                      \
			push(result);                                                                          \
		}                                                                                          \
	}

EMIT_SIMD_NARROW(i8x16_narrow_i16x8_s,
				 llvmContext.i16x8Type,
				 llvmContext.i8x8Type,
				 llvm::Intrinsic::x86_sse2_packsswb_128,
				 llvm::Intrinsic::aarch64_neon_sqxtn)
EMIT_SIMD_NARROW(i8x16_narrow_i16x8_u,
				 llvmContext.i16x8Type,
				 llvmContext.i8x8Type,
				 llvm::Intrinsic::x86_sse2_packuswb_128,
				 llvm::Intrinsic::aarch64_neon_sqxtun)
EMIT_SIMD_NARROW(i16x8_narrow_i32x4_s,
				 llvmContext.i32x4Type,
				 llvmContext.i16x4Type,
				 llvm::Intrinsic::x86_sse2_packssdw_128,
				 llvm::Intrinsic::aarch64_neon_sqxtn)
EMIT_SIMD_NARROW(i16x8_narrow_i32x4_u,
				 llvmContext.i32x4Type,
				 llvmContext.i16x4Type,
				 llvm::Intrinsic::x86_sse41_packusdw,
				 llvm::Intrinsic::aarch64_neon_sqxtun)

#define EMIT_SIMD_WIDEN(name, destType, sourceType, baseSourceElementIndex, numElements, extend)   \
	void EmitFunctionContext::name(IR::NoImm)                                                      \
	{                                                                                              \
		auto operand = irBuilder.CreateBitCast(pop(), sourceType);                                 \
		llvm::Value* result = llvm::UndefValue::get(destType);                                     \
		for(Uptr index = 0; index < numElements; ++index)                                          \
		{                                                                                          \
			auto scalar = irBuilder.CreateExtractElement(operand, baseSourceElementIndex + index); \
			result = irBuilder.CreateInsertElement(                                                \
				result, extend(scalar, destType->getScalarType()), index);                         \
		}                                                                                          \
		push(result);                                                                              \
	}

EMIT_SIMD_WIDEN(i16x8_widen_low_i8x16_s, llvmContext.i16x8Type, llvmContext.i8x16Type, 0, 8, sext)
EMIT_SIMD_WIDEN(i16x8_widen_high_i8x16_s, llvmContext.i16x8Type, llvmContext.i8x16Type, 8, 8, sext)
EMIT_SIMD_WIDEN(i16x8_widen_low_i8x16_u, llvmContext.i16x8Type, llvmContext.i8x16Type, 0, 8, zext)
EMIT_SIMD_WIDEN(i16x8_widen_high_i8x16_u, llvmContext.i16x8Type, llvmContext.i8x16Type, 8, 8, zext)

EMIT_SIMD_WIDEN(i32x4_widen_low_i16x8_s, llvmContext.i32x4Type, llvmContext.i16x8Type, 0, 4, sext)
EMIT_SIMD_WIDEN(i32x4_widen_high_i16x8_s, llvmContext.i32x4Type, llvmContext.i16x8Type, 4, 4, sext)
EMIT_SIMD_WIDEN(i32x4_widen_low_i16x8_u, llvmContext.i32x4Type, llvmContext.i16x8Type, 0, 4, zext)
EMIT_SIMD_WIDEN(i32x4_widen_high_i16x8_u, llvmContext.i32x4Type, llvmContext.i16x8Type, 4, 4, zext)

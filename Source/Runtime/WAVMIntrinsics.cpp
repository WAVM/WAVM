#include "Core/Core.h"
#include "Core/Floats.h"
#include "Intrinsics.h"
#include "RuntimePrivate.h"

#include <math.h>

namespace Runtime
{
	float32 quietNaN(float32 value)
	{
		Floats::F32Components components;
		components.value = value;
		components.bits.significand |= 1 << 22;
		return components.value;
	}
	
	float64 quietNaN(float64 value)
	{
		Floats::F64Components components;
		components.value = value;
		components.bits.significand |= 1ull << 51;
		return components.value;
	}

	template<typename Float,typename FloatComponents>
	Float floatMin(Float left,Float right)
	{
		// If either operand is a NaN, convert it to a quiet NaN and return it.
		if(left != left) { return quietNaN(left); }
		else if(right != right) { return quietNaN(right); }
		// If either operand is less than the other, return it.
		else if(left < right) { return left; }
		else if(right < left) { return right; }
		else
		{
			// Finally, if the operands are apparently equal, compare their integer values to distinguish -0.0 from +0.0
			FloatComponents leftComponents;
			leftComponents.value = left;
			FloatComponents rightComponents;
			rightComponents.value = right;
			return leftComponents.bitcastInt < rightComponents.bitcastInt ? right : left;
		}
	}
	
	template<typename Float,typename FloatComponents>
	Float floatMax(Float left,Float right)
	{
		// If either operand is a NaN, convert it to a quiet NaN and return it.
		if(left != left) { return quietNaN(left); }
		else if(right != right) { return quietNaN(right); }
		// If either operand is less than the other, return it.
		else if(left > right) { return left; }
		else if(right > left) { return right; }
		else
		{
			// Finally, if the operands are apparently equal, compare their integer values to distinguish -0.0 from +0.0
			FloatComponents leftComponents;
			leftComponents.value = left;
			FloatComponents rightComponents;
			rightComponents.value = right;
			return leftComponents.bitcastInt > rightComponents.bitcastInt ? right : left;
		}
	}

	template<typename Float>
	Float floatCeil(Float value)
	{
		if(value != value) { return quietNaN(value); }
		else { return ceil(value); }
	}
	
	template<typename Float>
	Float floatFloor(Float value)
	{
		if(value != value) { return quietNaN(value); }
		else { return floor(value); }
	}
	
	template<typename Float>
	Float floatTrunc(Float value)
	{
		if(value != value) { return quietNaN(value); }
		else { return trunc(value); }
	}
	
	template<typename Float>
	Float floatNearest(Float value)
	{
		if(value != value) { return quietNaN(value); }
		else { return nearbyint(value); }
	}

	DEFINE_INTRINSIC_FUNCTION2(wavmIntrinsics,floatMin,floatMin,F32,F32,left,F32,right) { return floatMin<float32,Floats::F32Components>(left,right); }
	DEFINE_INTRINSIC_FUNCTION2(wavmIntrinsics,floatMin,floatMin,F64,F64,left,F64,right) { return floatMin<float64,Floats::F64Components>(left,right); }
	DEFINE_INTRINSIC_FUNCTION2(wavmIntrinsics,floatMax,floatMax,F32,F32,left,F32,right) { return floatMax<float32,Floats::F32Components>(left,right); }
	DEFINE_INTRINSIC_FUNCTION2(wavmIntrinsics,floatMax,floatMax,F64,F64,left,F64,right) { return floatMax<float64,Floats::F64Components>(left,right); }

	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatCeil,floatCeil,F32,F32,value) { return floatCeil<float32>(value); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatCeil,floatCeil,F64,F64,value) { return floatCeil<float64>(value); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatFloor,floatFloor,F32,F32,value) { return floatFloor<float32>(value); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatFloor,floatFloor,F64,F64,value) { return floatFloor<float64>(value); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatTrunc,floatTrunc,F32,F32,value) { return floatTrunc<float32>(value); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatTrunc,floatTrunc,F64,F64,value) { return floatTrunc<float64>(value); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatNearest,floatNearest,F32,F32,value) { return floatNearest<float32>(value); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatNearest,floatNearest,F64,F64,value) { return floatNearest<float64>(value); }

	template<typename Dest,typename Source,bool isMinInclusive>
	Dest floatToInt(Source sourceValue,Source minValue,Source maxValue)
	{
		if(sourceValue != sourceValue)
		{
			causeException(Exception::Cause::InvalidFloatOperation);
		}
		else if(sourceValue >= maxValue || (isMinInclusive ? sourceValue <= minValue : sourceValue < minValue))
		{
			causeException(Exception::Cause::IntegerDivideByZeroOrIntegerOverflow);
		}
		return (Dest)sourceValue;
	}

	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToSignedInt,floatToSignedInt,I32,F32,source) { return floatToInt<int32,float32,false>(source,(float32)INT32_MIN,-(float32)INT32_MIN); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToSignedInt,floatToSignedInt,I32,F64,source) { return floatToInt<int32,float64,false>(source,(float64)INT32_MIN,-(float64)INT32_MIN); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToSignedInt,floatToSignedInt,I64,F32,source) { return floatToInt<int64,float32,false>(source,(float32)INT64_MIN,-(float32)INT64_MIN); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToSignedInt,floatToSignedInt,I64,F64,source) { return floatToInt<int64,float64,false>(source,(float64)INT64_MIN,-(float64)INT64_MIN); }
	
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToUnsignedInt,floatToUnsignedInt,I32,F32,source) { return floatToInt<uint32,float32,true>(source,-1.0f,-2.0f * INT32_MIN); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToUnsignedInt,floatToUnsignedInt,I32,F64,source) { return floatToInt<uint32,float64,true>(source,-1.0,-2.0 * INT32_MIN); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToUnsignedInt,floatToUnsignedInt,I64,F32,source) { return floatToInt<uint64,float32,true>(source,-1.0f,-2.0f * INT64_MIN); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToUnsignedInt,floatToUnsignedInt,I64,F64,source) { return floatToInt<uint64,float64,true>(source,-1.0,-2.0 * INT64_MIN); }

	DEFINE_INTRINSIC_FUNCTION0(wavmIntrinsics,divideByZeroTrap,divideByZeroTrap,Void)
	{
		causeException(Exception::Cause::IntegerDivideByZeroOrIntegerOverflow);
	}

	DEFINE_INTRINSIC_FUNCTION0(wavmIntrinsics,unreachableTrap,unreachableTrap,Void)
	{
		causeException(Exception::Cause::ReachedUnreachable);
	}

	DEFINE_INTRINSIC_FUNCTION0(wavmIntrinsics,indirectCallSignatureMismatch,indirectCallSignatureMismatch,Void)
	{
		causeException(Exception::Cause::IndirectCallSignatureMismatch);
	}

	DEFINE_INTRINSIC_FUNCTION0(wavmIntrinsics,indirectCallIndexOutOfBounds,indirectCallIndexOutOfBounds,Void)
	{
		causeException(Exception::Cause::OutOfBoundsFunctionTableIndex);
	}

	void initWAVMIntrinsics()
	{
	}
}

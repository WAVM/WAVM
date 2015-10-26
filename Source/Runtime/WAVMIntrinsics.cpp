#include "Core/Core.h"
#include "Core/Floats.h"
#include "Intrinsics.h"
#include "RuntimePrivate.h"

namespace Runtime
{
	void causeException(Exception::Cause cause)
	{
		auto callStack = describeExecutionContext(RuntimePlatform::captureExecutionContext());
		RuntimePlatform::raiseException(new Exception {cause,callStack});
	}

	template<typename Float,typename FloatComponents>
	Float floatMin(Float left,Float right)
	{
		// If either operand is a NaN, return it.
		if(left != left) { return left; }
		else if(right != right) { return right; }
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
		// If either operand is a NaN, return it.
		if(left != left) { return left; }
		else if(right != right) { return right; }
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

	DEFINE_INTRINSIC_FUNCTION2(wavmIntrinsics,floatMin,F32,F32,left,F32,right)
	{
		return floatMin<float32,Floats::F32Components>(left,right);
	}
	DEFINE_INTRINSIC_FUNCTION2(wavmIntrinsics,floatMin,F64,F64,left,F64,right)
	{
		return floatMin<float64,Floats::F64Components>(left,right);
	}
	DEFINE_INTRINSIC_FUNCTION2(wavmIntrinsics,floatMax,F32,F32,left,F32,right)
	{
		return floatMax<float32,Floats::F32Components>(left,right);
	}
	DEFINE_INTRINSIC_FUNCTION2(wavmIntrinsics,floatMax,F64,F64,left,F64,right)
	{
		return floatMax<float64,Floats::F64Components>(left,right);
	}

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

	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToSignedInt,I32,F32,source) { return floatToInt<int32,float32,false>(source,(float32)INT32_MIN,-(float32)INT32_MIN); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToSignedInt,I32,F64,source) { return floatToInt<int32,float64,false>(source,(float64)INT32_MIN,-(float64)INT32_MIN); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToSignedInt,I64,F32,source) { return floatToInt<int64,float32,false>(source,(float32)INT64_MIN,-(float32)INT64_MIN); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToSignedInt,I64,F64,source) { return floatToInt<int64,float64,false>(source,(float64)INT64_MIN,-(float64)INT64_MIN); }
	
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToUnsignedInt,I32,F32,source) { return floatToInt<uint32,float32,true>(source,-1.0f,-2.0f * INT32_MIN); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToUnsignedInt,I32,F64,source) { return floatToInt<uint32,float64,true>(source,-1.0,-2.0 * INT32_MIN); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToUnsignedInt,I64,F32,source) { return floatToInt<uint64,float32,true>(source,-1.0f,-2.0f * INT64_MIN); }
	DEFINE_INTRINSIC_FUNCTION1(wavmIntrinsics,floatToUnsignedInt,I64,F64,source) { return floatToInt<uint64,float64,true>(source,-1.0,-2.0 * INT64_MIN); }

	DEFINE_INTRINSIC_FUNCTION0(wavmIntrinsics,divideByZeroTrap,Void)
	{
		causeException(Exception::Cause::IntegerDivideByZeroOrIntegerOverflow);
	}

	void initWAVMIntrinsics()
	{
	}
}

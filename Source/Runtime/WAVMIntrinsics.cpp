#include "Core/Core.h"
#include "Core/Floats.h"
#include "Intrinsics.h"
#include "RuntimePrivate.h"

namespace Runtime
{
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

	void initWAVMIntrinsics()
	{
	}
}

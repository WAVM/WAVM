#include <stdint.h>
#include <cmath>
#include <string>
#include <vector>

#include "RuntimePrivate.h"
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/FloatComponents.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/Runtime/RuntimeData.h"

using namespace WAVM;
using namespace WAVM::Runtime;

namespace WAVM { namespace Runtime {
	DEFINE_INTRINSIC_MODULE(wavmIntrinsics)
}}

template<typename Float> Float quietNaN(Float value)
{
	FloatComponents<Float> components;
	components.value = value;
	components.bits.significand |= typename FloatComponents<Float>::Bits(1)
								   << (FloatComponents<Float>::numSignificandBits - 1);
	return components.value;
}

template<typename Float> Float floatCeil(Float value)
{
	if(value != value) { return quietNaN(value); }
	else
	{
		return ceil(value);
	}
}

template<typename Float> Float floatFloor(Float value)
{
	if(value != value) { return quietNaN(value); }
	else
	{
		return floor(value);
	}
}

template<typename Float> Float floatTrunc(Float value)
{
	if(value != value) { return quietNaN(value); }
	else
	{
		return trunc(value);
	}
}

template<typename Float> Float floatNearest(Float value)
{
	if(value != value) { return quietNaN(value); }
	else
	{
		return nearbyint(value);
	}
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "f32.ceil", F32, f32Ceil, F32 value)
{
	return floatCeil(value);
}
DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "f64.ceil", F64, f64Ceil, F64 value)
{
	return floatCeil(value);
}
DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "f32.floor", F32, f32Floor, F32 value)
{
	return floatFloor(value);
}
DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "f64.floor", F64, f64Floor, F64 value)
{
	return floatFloor(value);
}
DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "f32.trunc", F32, f32Trunc, F32 value)
{
	return floatTrunc(value);
}
DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "f64.trunc", F64, f64Trunc, F64 value)
{
	return floatTrunc(value);
}
DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "f32.nearest", F32, f32Nearest, F32 value)
{
	return floatNearest(value);
}
DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "f64.nearest", F64, f64Nearest, F64 value)
{
	return floatNearest(value);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "divideByZeroOrIntegerOverflowTrap",
						  void,
						  divideByZeroOrIntegerOverflowTrap)
{
	throwException(ExceptionTypes::integerDivideByZeroOrOverflow);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "unreachableTrap", void, unreachableTrap)
{
	throwException(ExceptionTypes::reachedUnreachable);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "invalidFloatOperationTrap",
						  void,
						  invalidFloatOperationTrap)
{
	throwException(ExceptionTypes::invalidFloatOperation);
}

static thread_local Uptr indentLevel = 0;

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "debugEnterFunction",
						  void,
						  debugEnterFunction,
						  const Function* function)
{
	Log::printf(Log::debug,
				"ENTER: %*s\n",
				U32(indentLevel * 4 + function->mutableData->debugName.size()),
				function->mutableData->debugName.c_str());
	++indentLevel;
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "debugExitFunction",
						  void,
						  debugExitFunction,
						  const Function* function)
{
	--indentLevel;
	Log::printf(Log::debug,
				"EXIT:  %*s\n",
				U32(indentLevel * 4 + function->mutableData->debugName.size()),
				function->mutableData->debugName.c_str());
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "debugBreak", void, debugBreak)
{
	Log::printf(Log::debug, "================== wavmIntrinsics.debugBreak\n");
}

void Runtime::dummyReferenceWAVMIntrinsics()
{
	// This is just here make sure the static initializers for the intrinsic function registrations
	// aren't removed as dead code.
}

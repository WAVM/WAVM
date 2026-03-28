#include <string>
#include <vector>
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"

using namespace WAVM;
using namespace WAVM::Runtime;

namespace WAVM { namespace Runtime {
	WAVM_DEFINE_INTRINSIC_MODULE(wavmIntrinsics)
}}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
							   "divideByZeroOrIntegerOverflowTrap",
							   void,
							   divideByZeroOrIntegerOverflowTrap)
{
	throwException(ExceptionTypes::integerDivideByZeroOrOverflow, {}, 1);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "unreachableTrap", void, unreachableTrap)
{
	throwException(ExceptionTypes::reachedUnreachable, {}, 1);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
							   "invalidFloatOperationTrap",
							   void,
							   invalidFloatOperationTrap)
{
	throwException(ExceptionTypes::invalidFloatOperation, {}, 1);
}

static thread_local Uptr indentLevel = 0;

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
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

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
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

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics, "debugBreak", void, debugBreak)
{
	Log::printf(Log::debug, "================== wavmIntrinsics.debugBreak\n");
}

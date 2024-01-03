#include "./WASIPrivate.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/WASI/WASIABI64.h"

using namespace WAVM;
using namespace WAVM::WASI;
using namespace WAVM::Runtime;

namespace WAVM { namespace WASI {
	WAVM_DEFINE_INTRINSIC_MODULE(wasiArgsEnvs)
}}
#include "WASIdefineIntrinsicsI32.h"
#include "WASIArgsEnvs.h"
#if UINT32_MAX < SIZE_MAX
#include "WASIdefineIntrinsicsI64.h"
#include "WASIArgsEnvs.h"
#endif

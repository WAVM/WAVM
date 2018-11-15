#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <initializer_list>
#include <memory>
#include <new>
#include <string>
#include <utility>
#ifndef _WIN32
#include <sys/uio.h>
#endif

#include "WAVM/Emscripten/Emscripten.h"
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/FloatComponents.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

DEFINE_INTRINSIC_MODULE(env)
DEFINE_INTRINSIC_MODULE(asm2wasm)
DEFINE_INTRINSIC_MODULE(global)

static U32 coerce32bitAddress(Memory* memory, Uptr address)
{
	if(address >= UINT32_MAX)
	{ throwException(Exception::outOfBoundsMemoryAccessType, {asObject(memory), U64(address)}); }
	return (U32)address;
}

static I32 coerce32bitAddressSigned(Memory* memory, Uptr address)
{
	if(address >= INT32_MAX)
	{ throwException(Exception::outOfBoundsMemoryAccessType, {asObject(memory), U64(address)}); }
	return (I32)address;
}

//  0..62  = static data
// 63..63  = MutableGlobals
// 64..128 = aliased stack
// 129..   = dynamic memory
enum
{
	minStaticEmscriptenMemoryPages = 128
};

enum ErrNo
{
	einval = 22
};

struct MutableGlobals
{
	enum
	{
		address = 63 * IR::numBytesPerPage
	};

	U32 DYNAMICTOP_PTR;
	F64 tempDoublePtr;
	I32 _stderr;
	I32 _stdin;
	I32 _stdout;
};

DEFINE_INTRINSIC_GLOBAL(env, "STACKTOP", I32, STACKTOP, 64 * IR::numBytesPerPage);
DEFINE_INTRINSIC_GLOBAL(env, "STACK_MAX", I32, STACK_MAX, 128 * IR::numBytesPerPage);
DEFINE_INTRINSIC_GLOBAL(env,
						"tempDoublePtr",
						I32,
						tempDoublePtr,
						MutableGlobals::address + offsetof(MutableGlobals, tempDoublePtr));
DEFINE_INTRINSIC_GLOBAL(env, "ABORT", I32, ABORT, 0);
DEFINE_INTRINSIC_GLOBAL(env, "cttz_i8", I32, cttz_i8, 0);
DEFINE_INTRINSIC_GLOBAL(env, "___dso_handle", U32, ___dso_handle, 0);
DEFINE_INTRINSIC_GLOBAL(env,
						"_stderr",
						I32,
						_stderr,
						MutableGlobals::address + offsetof(MutableGlobals, _stderr));
DEFINE_INTRINSIC_GLOBAL(env,
						"_stdin",
						I32,
						_stdin,
						MutableGlobals::address + offsetof(MutableGlobals, _stdin));
DEFINE_INTRINSIC_GLOBAL(env,
						"_stdout",
						I32,
						_stdout,
						MutableGlobals::address + offsetof(MutableGlobals, _stdout));

DEFINE_INTRINSIC_GLOBAL(env, "__memory_base", U32, memory_base, 1024);
DEFINE_INTRINSIC_GLOBAL(env, "memoryBase", U32, emscriptenMemoryBase, 1024);

DEFINE_INTRINSIC_GLOBAL(env, "__table_base", U32, table_base, 0);
DEFINE_INTRINSIC_GLOBAL(env, "tableBase", U32, emscriptenTableBase, 0);

DEFINE_INTRINSIC_GLOBAL(env,
						"DYNAMICTOP_PTR",
						U32,
						DYNAMICTOP_PTR,
						MutableGlobals::address + offsetof(MutableGlobals, DYNAMICTOP_PTR))
DEFINE_INTRINSIC_GLOBAL(env, "_environ", U32, em_environ, 0)
DEFINE_INTRINSIC_GLOBAL(env, "EMTSTACKTOP", U32, EMTSTACKTOP, 0)
DEFINE_INTRINSIC_GLOBAL(env, "EMT_STACK_MAX", U32, EMT_STACK_MAX, 0)
DEFINE_INTRINSIC_GLOBAL(env, "eb", I32, eb, 0)

static thread_local Memory* emscriptenMemory = nullptr;

static U32 dynamicAlloc(Memory* memory, U32 numBytes)
{
	MutableGlobals& mutableGlobals = memoryRef<MutableGlobals>(memory, MutableGlobals::address);

	const U32 allocationAddress = mutableGlobals.DYNAMICTOP_PTR;
	const U32 endAddress = (allocationAddress + numBytes + 15) & -16;

	mutableGlobals.DYNAMICTOP_PTR = endAddress;

	const Uptr endPage = (endAddress + IR::numBytesPerPage - 1) / IR::numBytesPerPage;
	if(endPage >= getMemoryNumPages(memory) && endPage < getMemoryMaxPages(memory))
	{ growMemory(memory, endPage - getMemoryNumPages(memory) + 1); }

	return allocationAddress;
}

DEFINE_INTRINSIC_FUNCTION(env, "getTotalMemory", U32, getTotalMemory)
{
	wavmAssert(emscriptenMemory);
	return coerce32bitAddress(emscriptenMemory,
							  Runtime::getMemoryMaxPages(emscriptenMemory) * IR::numBytesPerPage);
}

DEFINE_INTRINSIC_FUNCTION(env, "abortOnCannotGrowMemory", I32, abortOnCannotGrowMemory)
{
	throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
}
DEFINE_INTRINSIC_FUNCTION(env, "enlargeMemory", I32, enlargeMemory)
{
	return abortOnCannotGrowMemory(contextRuntimeData);
}

DEFINE_INTRINSIC_FUNCTION(env, "_time", I32, _time, U32 address)
{
	wavmAssert(emscriptenMemory);
	time_t t = time(nullptr);
	if(address) { memoryRef<I32>(emscriptenMemory, address) = (I32)t; }
	return (I32)t;
}

DEFINE_INTRINSIC_FUNCTION(env, "___errno_location", I32, ___errno_location) { return 0; }

DEFINE_INTRINSIC_FUNCTION(env, "_sysconf", I32, _sysconf, I32 a)
{
	enum
	{
		sysConfPageSize = 30
	};
	switch(a)
	{
	case sysConfPageSize: return IR::numBytesPerPage;
	default: throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
	}
}

DEFINE_INTRINSIC_FUNCTION(env, "_pthread_cond_wait", I32, _pthread_cond_wait, I32 a, I32 b)
{
	return 0;
}
DEFINE_INTRINSIC_FUNCTION(env, "_pthread_cond_broadcast", I32, _pthread_cond_broadcast, I32 a)
{
	return 0;
}

static HashMap<U32, I32> pthreadSpecific = {};
static U32 pthreadSpecificNextKey = 0;

DEFINE_INTRINSIC_FUNCTION(env,
						  "_pthread_key_create",
						  I32,
						  _pthread_key_create,
						  U32 key,
						  I32 destructorPtr)
{
	if(key == 0) { return ErrNo::einval; }

	wavmAssert(emscriptenMemory);
	memoryRef<U32>(emscriptenMemory, key) = pthreadSpecificNextKey;
	pthreadSpecific.set(pthreadSpecificNextKey, 0);
	pthreadSpecificNextKey++;

	return 0;
}
DEFINE_INTRINSIC_FUNCTION(env, "_pthread_mutex_lock", I32, _pthread_mutex_lock, I32 a) { return 0; }
DEFINE_INTRINSIC_FUNCTION(env, "_pthread_mutex_unlock", I32, _pthread_mutex_unlock, I32 a)
{
	return 0;
}
DEFINE_INTRINSIC_FUNCTION(env,
						  "_pthread_setspecific",
						  I32,
						  _pthread_setspecific,
						  U32 key,
						  I32 value)
{
	if(!pthreadSpecific.contains(key)) { return ErrNo::einval; }
	pthreadSpecific.set(key, value);
	return 0;
}
DEFINE_INTRINSIC_FUNCTION(env, "_pthread_getspecific", I32, _pthread_getspecific, U32 key)
{
	const I32* value = pthreadSpecific.get(key);
	return value ? *value : 0;
}
DEFINE_INTRINSIC_FUNCTION(env, "_pthread_once", I32, _pthread_once, I32 a, I32 b)
{
	throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
}
DEFINE_INTRINSIC_FUNCTION(env, "_pthread_cleanup_push", void, _pthread_cleanup_push, I32 a, I32 b)
{
}
DEFINE_INTRINSIC_FUNCTION(env, "_pthread_cleanup_pop", void, _pthread_cleanup_pop, I32 a) {}
DEFINE_INTRINSIC_FUNCTION(env, "_pthread_self", I32, _pthread_self) { return 0; }

DEFINE_INTRINSIC_FUNCTION(env, "___ctype_b_loc", U32, ___ctype_b_loc)
{
	wavmAssert(emscriptenMemory);
	unsigned short data[384] = {
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     2,     2,
		2,     2,     2,     2,     2,     2,     2,     8195,  8194,  8194,  8194,  8194,  2,
		2,     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
		2,     2,     2,     2,     24577, 49156, 49156, 49156, 49156, 49156, 49156, 49156, 49156,
		49156, 49156, 49156, 49156, 49156, 49156, 49156, 55304, 55304, 55304, 55304, 55304, 55304,
		55304, 55304, 55304, 55304, 49156, 49156, 49156, 49156, 49156, 49156, 49156, 54536, 54536,
		54536, 54536, 54536, 54536, 50440, 50440, 50440, 50440, 50440, 50440, 50440, 50440, 50440,
		50440, 50440, 50440, 50440, 50440, 50440, 50440, 50440, 50440, 50440, 50440, 49156, 49156,
		49156, 49156, 49156, 49156, 54792, 54792, 54792, 54792, 54792, 54792, 50696, 50696, 50696,
		50696, 50696, 50696, 50696, 50696, 50696, 50696, 50696, 50696, 50696, 50696, 50696, 50696,
		50696, 50696, 50696, 50696, 49156, 49156, 49156, 49156, 2,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
		0,     0,     0,     0,     0,     0,     0};
	static U32 vmAddress = 0;
	if(vmAddress == 0)
	{
		vmAddress
			= coerce32bitAddress(emscriptenMemory, dynamicAlloc(emscriptenMemory, sizeof(data)));
		memcpy(memoryArrayPtr<U8>(emscriptenMemory, vmAddress, sizeof(data)), data, sizeof(data));
	}
	return vmAddress + sizeof(short) * 128;
}
DEFINE_INTRINSIC_FUNCTION(env, "___ctype_toupper_loc", U32, ___ctype_toupper_loc)
{
	wavmAssert(emscriptenMemory);
	I32 data[384]
		= {128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145,
		   146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163,
		   164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181,
		   182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199,
		   200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217,
		   218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235,
		   236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253,
		   254, -1,  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,
		   16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,
		   34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,
		   52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,
		   70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,
		   88,  89,  90,  91,  92,  93,  94,  95,  96,  65,  66,  67,  68,  69,  70,  71,  72,  73,
		   74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  123,
		   124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141,
		   142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
		   160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177,
		   178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195,
		   196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213,
		   214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231,
		   232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249,
		   250, 251, 252, 253, 254, 255};
	static U32 vmAddress = 0;
	if(vmAddress == 0)
	{
		vmAddress
			= coerce32bitAddress(emscriptenMemory, dynamicAlloc(emscriptenMemory, sizeof(data)));
		memcpy(memoryArrayPtr<U8>(emscriptenMemory, vmAddress, sizeof(data)), data, sizeof(data));
	}
	return vmAddress + sizeof(I32) * 128;
}
DEFINE_INTRINSIC_FUNCTION(env, "___ctype_tolower_loc", U32, ___ctype_tolower_loc)
{
	wavmAssert(emscriptenMemory);
	I32 data[384]
		= {128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145,
		   146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163,
		   164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181,
		   182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199,
		   200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217,
		   218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235,
		   236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253,
		   254, -1,  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,
		   16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,
		   34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,
		   52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  97,  98,  99,  100, 101,
		   102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
		   120, 121, 122, 91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105,
		   106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123,
		   124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141,
		   142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
		   160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177,
		   178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195,
		   196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213,
		   214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231,
		   232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249,
		   250, 251, 252, 253, 254, 255};
	static U32 vmAddress = 0;
	if(vmAddress == 0)
	{
		vmAddress
			= coerce32bitAddress(emscriptenMemory, dynamicAlloc(emscriptenMemory, sizeof(data)));
		memcpy(memoryArrayPtr<U8>(emscriptenMemory, vmAddress, sizeof(data)), data, sizeof(data));
	}
	return vmAddress + sizeof(I32) * 128;
}
DEFINE_INTRINSIC_FUNCTION(env,
						  "___assert_fail",
						  void,
						  ___assert_fail,
						  I32 condition,
						  I32 filename,
						  I32 line,
						  I32 function)
{
	throwException(Runtime::Exception::calledAbortType);
}

DEFINE_INTRINSIC_FUNCTION(env, "___cxa_atexit", I32, ___cxa_atexit, I32 a, I32 b, I32 c)
{
	return 0;
}
DEFINE_INTRINSIC_FUNCTION(env, "___cxa_guard_acquire", I32, ___cxa_guard_acquire, U32 address)
{
	wavmAssert(emscriptenMemory);
	if(!memoryRef<U8>(emscriptenMemory, address))
	{
		memoryRef<U8>(emscriptenMemory, address) = 1;
		return 1;
	}
	else
	{
		return 0;
	}
}
DEFINE_INTRINSIC_FUNCTION(env, "___cxa_guard_release", void, ___cxa_guard_release, I32 a) {}
DEFINE_INTRINSIC_FUNCTION(env, "___cxa_throw", void, ___cxa_throw, I32 a, I32 b, I32 c)
{
	throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
}
DEFINE_INTRINSIC_FUNCTION(env, "___cxa_begin_catch", I32, ___cxa_begin_catch, I32 a)
{
	throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
}
DEFINE_INTRINSIC_FUNCTION(env,
						  "___cxa_allocate_exception",
						  U32,
						  ___cxa_allocate_exception,
						  U32 size)
{
	wavmAssert(emscriptenMemory);
	return coerce32bitAddress(emscriptenMemory, dynamicAlloc(emscriptenMemory, size));
}
DEFINE_INTRINSIC_FUNCTION(env, "__ZSt18uncaught_exceptionv", I32, __ZSt18uncaught_exceptionv)
{
	throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
}
DEFINE_INTRINSIC_FUNCTION(env, "_abort", void, emscripten__abort)
{
	throwException(Runtime::Exception::calledAbortType);
}
DEFINE_INTRINSIC_FUNCTION(env, "_exit", void, emscripten__exit, I32 code)
{
	throwException(Runtime::Exception::calledAbortType);
}
DEFINE_INTRINSIC_FUNCTION(env, "abort", void, emscripten_abort, I32 code)
{
	Log::printf(Log::error, "env.abort(%i)\n", code);
	throwException(Runtime::Exception::calledAbortType);
}

static U32 currentLocale = 0;
DEFINE_INTRINSIC_FUNCTION(env, "_uselocale", I32, _uselocale, I32 locale)
{
	auto oldLocale = currentLocale;
	currentLocale = locale;
	return oldLocale;
}
DEFINE_INTRINSIC_FUNCTION(env, "_newlocale", U32, _newlocale, I32 mask, I32 locale, I32 base)
{
	wavmAssert(emscriptenMemory);
	if(!base) { base = coerce32bitAddress(emscriptenMemory, dynamicAlloc(emscriptenMemory, 4)); }
	return base;
}
DEFINE_INTRINSIC_FUNCTION(env, "_freelocale", void, emscripten__freelocale, I32 a) {}

DEFINE_INTRINSIC_FUNCTION(env,
						  "_strftime_l",
						  I32,
						  emscripten__strftime_l,
						  I32 a,
						  I32 b,
						  I32 c,
						  I32 d,
						  I32 e)
{
	throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
}
DEFINE_INTRINSIC_FUNCTION(env, "_strerror", I32, emscripten__strerror, I32 a)
{
	throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION(env, "_catopen", I32, emscripten__catopen, I32 a, I32 b) { return -1; }
DEFINE_INTRINSIC_FUNCTION(env,
						  "_catgets",
						  I32,
						  emscripten__catgets,
						  I32 catd,
						  I32 set_id,
						  I32 msg_id,
						  I32 s)
{
	return s;
}
DEFINE_INTRINSIC_FUNCTION(env, "_catclose", I32, emscripten__catclose, I32 a) { return 0; }

DEFINE_INTRINSIC_FUNCTION(env,
						  "_emscripten_memcpy_big",
						  U32,
						  _emscripten_memcpy_big,
						  U32 sourceAddress,
						  U32 destAddress,
						  U32 numBytes)
{
	wavmAssert(emscriptenMemory);
	memcpy(memoryArrayPtr<U8>(emscriptenMemory, sourceAddress, numBytes),
		   memoryArrayPtr<U8>(emscriptenMemory, destAddress, numBytes),
		   numBytes);
	return sourceAddress;
}

enum class ioStreamVMHandle
{
	StdErr = 1,
	StdIn = 2,
	StdOut = 3
};
FILE* vmFile(U32 vmHandle)
{
	switch((ioStreamVMHandle)vmHandle)
	{
	case ioStreamVMHandle::StdErr: return stderr;
	case ioStreamVMHandle::StdIn: return stdin;
	case ioStreamVMHandle::StdOut: return stdout;
	default: return stdout; // std::cerr << "invalid file handle " << vmHandle << std::endl; throw;
	}
}

DEFINE_INTRINSIC_FUNCTION(env,
						  "_vfprintf",
						  I32,
						  _vfprintf,
						  I32 file,
						  U32 formatPointer,
						  I32 argList)
{
	throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
}
DEFINE_INTRINSIC_FUNCTION(env, "_getc", I32, _getc, I32 file) { return getc(vmFile(file)); }
DEFINE_INTRINSIC_FUNCTION(env, "_ungetc", I32, _ungetc, I32 character, I32 file)
{
	return ungetc(character, vmFile(file));
}
DEFINE_INTRINSIC_FUNCTION(env,
						  "_fread",
						  U32,
						  _fread,
						  U32 destAddress,
						  U32 size,
						  U32 count,
						  I32 file)
{
	wavmAssert(emscriptenMemory);
	return coerce32bitAddress(
		emscriptenMemory,
		fread(memoryArrayPtr<U8>(emscriptenMemory, destAddress, U64(size) * U64(count)),
			  U64(size),
			  U64(count),
			  vmFile(file)));
}
DEFINE_INTRINSIC_FUNCTION(env,
						  "_fwrite",
						  U32,
						  _fwrite,
						  U32 sourceAddress,
						  U32 size,
						  U32 count,
						  I32 file)
{
	wavmAssert(emscriptenMemory);
	return coerce32bitAddress(
		emscriptenMemory,
		fwrite(memoryArrayPtr<U8>(emscriptenMemory, sourceAddress, U64(size) * U64(count)),
			   U64(size),
			   U64(count),
			   vmFile(file)));
}
DEFINE_INTRINSIC_FUNCTION(env, "_fputc", I32, _fputc, I32 character, I32 file)
{
	return fputc(character, vmFile(file));
}
DEFINE_INTRINSIC_FUNCTION(env, "_fflush", I32, _fflush, I32 file) { return fflush(vmFile(file)); }

DEFINE_INTRINSIC_FUNCTION(env, "___lock", void, ___lock, I32 a) {}
DEFINE_INTRINSIC_FUNCTION(env, "___unlock", void, ___unlock, I32 a) {}
DEFINE_INTRINSIC_FUNCTION(env, "___lockfile", I32, ___lockfile, I32 a) { return 1; }
DEFINE_INTRINSIC_FUNCTION(env, "___unlockfile", void, ___unlockfile, I32 a) {}

DEFINE_INTRINSIC_FUNCTION(env, "___syscall6", I32, ___syscall6, I32 a, I32 b)
{
	// close
	throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION(env, "___syscall54", I32, ___syscall54, I32 a, I32 b)
{
	// ioctl
	return 0;
}

DEFINE_INTRINSIC_FUNCTION(env, "___syscall140", I32, ___syscall140, I32 a, I32 b)
{
	// llseek
	throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION(env, "___syscall145", I32, ___syscall145, I32 file, I32 argsPtr)
{
	// readv
	throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
}

DEFINE_INTRINSIC_FUNCTION(env, "___syscall146", I32, ___syscall146, I32 file, U32 argsPtr)
{
	wavmAssert(emscriptenMemory);

	// writev
	U32* args = memoryArrayPtr<U32>(emscriptenMemory, argsPtr, 3);
	U32 iov = args[1];
	U32 iovcnt = args[2];
#ifdef _WIN32
	Uptr count = 0;
	for(U32 i = 0; i < iovcnt; i++)
	{
		U32 base = memoryRef<U32>(emscriptenMemory, iov + i * 8);
		U32 len = memoryRef<U32>(emscriptenMemory, iov + i * 8 + 4);
		U32 size
			= (U32)fwrite(memoryArrayPtr<U8>(emscriptenMemory, base, len), 1, len, vmFile(file));
		count += size;
		if(size < len) break;
	}
	return coerce32bitAddressSigned(emscriptenMemory, count);
#else
	struct iovec* native_iovec = new(alloca(sizeof(iovec) * iovcnt)) struct iovec[iovcnt];
	for(U32 i = 0; i < iovcnt; i++)
	{
		U32 base = memoryRef<U32>(emscriptenMemory, iov + i * 8);
		U32 len = memoryRef<U32>(emscriptenMemory, iov + i * 8 + 4);

		native_iovec[i].iov_base = memoryArrayPtr<U8>(emscriptenMemory, base, len);
		native_iovec[i].iov_len = len;
	}
	Iptr count = writev(fileno(vmFile(file)), native_iovec, iovcnt);
	return coerce32bitAddressSigned(emscriptenMemory, count);
#endif
}

DEFINE_INTRINSIC_FUNCTION(asm2wasm, "f64-to-int", I32, f64_to_int, F64 f) { return (I32)f; }

static F64 makeNaN()
{
	FloatComponents<F64> floatBits;
	floatBits.bits.sign = 0;
	floatBits.bits.exponent = FloatComponents<F64>::maxExponentBits;
	floatBits.bits.significand = FloatComponents<F64>::canonicalSignificand;
	return floatBits.value;
}
static F64 makeInf()
{
	FloatComponents<F64> floatBits;
	floatBits.bits.sign = 0;
	floatBits.bits.exponent = FloatComponents<F64>::maxExponentBits;
	floatBits.bits.significand = FloatComponents<F64>::maxSignificand;
	return floatBits.value;
}

DEFINE_INTRINSIC_GLOBAL(global, "NaN", F64, NaN, makeNaN())
DEFINE_INTRINSIC_GLOBAL(global, "Infinity", F64, Infinity, makeInf())

DEFINE_INTRINSIC_FUNCTION(asm2wasm, "i32u-rem", U32, I32_remu, U32 left, U32 right)
{
	return left % right;
}
DEFINE_INTRINSIC_FUNCTION(asm2wasm, "i32s-rem", I32, I32_rems, I32 left, I32 right)
{
	return left % right;
}
DEFINE_INTRINSIC_FUNCTION(asm2wasm, "i32u-div", U32, I32_divu, U32 left, U32 right)
{
	return left / right;
}
DEFINE_INTRINSIC_FUNCTION(asm2wasm, "i32s-div", I32, I32_divs, I32 left, I32 right)
{
	return left / right;
}
DEFINE_INTRINSIC_FUNCTION(asm2wasm, "f64-rem", F64, F64_rems, F64 left, F64 right)
{
	return (F64)fmod(left, right);
}

Emscripten::Instance* Emscripten::instantiate(Compartment* compartment, const IR::Module& module)
{
	MemoryType memoryType(false, SizeConstraints{0, 0});
	if(module.memories.imports.size() && module.memories.imports[0].moduleName == "env"
	   && module.memories.imports[0].exportName == "memory")
	{
		memoryType = module.memories.imports[0].type;
		if(memoryType.size.max >= minStaticEmscriptenMemoryPages)
		{
			if(memoryType.size.min <= minStaticEmscriptenMemoryPages)
			{
				// Enlarge the initial memory to make space for the stack and mutable globals.
				memoryType.size.min = minStaticEmscriptenMemoryPages;
			}
		}
		else
		{
			Log::printf(Log::error, "module's memory is too small for Emscripten emulation");
			return nullptr;
		}
	}
	else
	{
		return nullptr;
	}

	TableType tableType(ReferenceType::anyfunc, false, SizeConstraints{0, 0});
	if(module.tables.imports.size() && module.tables.imports[0].moduleName == "env"
	   && module.tables.imports[0].exportName == "table")
	{ tableType = module.tables.imports[0].type; }

	Memory* memory = Runtime::createMemory(compartment, memoryType, "env.memory");
	Table* table = Runtime::createTable(compartment, tableType, "env.table");

	HashMap<std::string, Runtime::Object*> extraEnvExports = {
		{"memory", Runtime::asObject(memory)},
		{"table", Runtime::asObject(table)},
	};

	Instance* instance = new Instance;
	instance->env = Intrinsics::instantiateModule(
		compartment, INTRINSIC_MODULE_REF(env), "env", extraEnvExports);
	instance->asm2wasm
		= Intrinsics::instantiateModule(compartment, INTRINSIC_MODULE_REF(asm2wasm), "asm2wasm");
	instance->global
		= Intrinsics::instantiateModule(compartment, INTRINSIC_MODULE_REF(global), "global");

	MutableGlobals& mutableGlobals = memoryRef<MutableGlobals>(memory, MutableGlobals::address);

	mutableGlobals.DYNAMICTOP_PTR = STACK_MAX.getValue().i32;
	mutableGlobals._stderr = (U32)ioStreamVMHandle::StdErr;
	mutableGlobals._stdin = (U32)ioStreamVMHandle::StdIn;
	mutableGlobals._stdout = (U32)ioStreamVMHandle::StdOut;

	instance->emscriptenMemory = memory;
	emscriptenMemory = instance->emscriptenMemory;

	return instance;
}

void Emscripten::initializeGlobals(Context* context,
								   const IR::Module& module,
								   ModuleInstance* moduleInstance)
{
	// Call the establishStackSpace function to set the Emscripten module's internal stack
	// pointers.
	Function* establishStackSpace
		= asFunctionNullable(getInstanceExport(moduleInstance, "establishStackSpace"));
	if(establishStackSpace
	   && getFunctionType(establishStackSpace)
			  == FunctionType(TypeTuple{}, TypeTuple{ValueType::i32, ValueType::i64}))
	{
		std::vector<IR::Value> parameters
			= {IR::Value(STACKTOP.getValue().i32), IR::Value(STACK_MAX.getValue().i32)};
		Runtime::invokeFunctionChecked(context, establishStackSpace, parameters);
	}

	// Call the global initializer functions.
	for(Uptr exportIndex = 0; exportIndex < module.exports.size(); ++exportIndex)
	{
		const Export& functionExport = module.exports[exportIndex];
		if(functionExport.kind == IR::ExternKind::function
		   && !strncmp(functionExport.name.c_str(), "__GLOBAL__", 10))
		{
			Function* function
				= asFunctionNullable(getInstanceExport(moduleInstance, functionExport.name));
			if(function) { Runtime::invokeFunctionChecked(context, function, {}); }
		}
	}
}

void Emscripten::injectCommandArgs(Emscripten::Instance* instance,
								   const std::vector<const char*>& argStrings,
								   std::vector<IR::Value>& outInvokeArgs)
{
	Memory* memory = instance->emscriptenMemory;
	U8* emscriptenMemoryBaseAdress = getMemoryBaseAddress(memory);

	U32* argvOffsets = (U32*)(emscriptenMemoryBaseAdress
							  + dynamicAlloc(memory, (U32)(sizeof(U32) * (argStrings.size() + 1))));
	for(Uptr argIndex = 0; argIndex < argStrings.size(); ++argIndex)
	{
		auto stringSize = strlen(argStrings[argIndex]) + 1;
		auto stringMemory = emscriptenMemoryBaseAdress + dynamicAlloc(memory, (U32)stringSize);
		memcpy(stringMemory, argStrings[argIndex], stringSize);
		argvOffsets[argIndex] = (U32)(stringMemory - emscriptenMemoryBaseAdress);
	}
	argvOffsets[argStrings.size()] = 0;
	outInvokeArgs = {(U32)argStrings.size(), (U32)((U8*)argvOffsets - emscriptenMemoryBaseAdress)};
}

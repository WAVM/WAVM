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
#include "WAVM/Platform/Clock.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/VFS/VFS.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

#define DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(module, nameString, Result, cName, ...)            \
	WAVM_DEFINE_INTRINSIC_FUNCTION(module, nameString, Result, cName, __VA_ARGS__)                 \
	{                                                                                              \
		Errors::fatalf("WAVM does not yet implement the Emscripten host function '%s'",            \
					   nameString);                                                                \
	}

WAVM_DEFINE_INTRINSIC_MODULE(env)
WAVM_DEFINE_INTRINSIC_MODULE(asm2wasm)
WAVM_DEFINE_INTRINSIC_MODULE(global)

static U32 coerce32bitAddress(Memory* memory, Uptr address)
{
	if(address >= UINT32_MAX)
	{ throwException(ExceptionTypes::outOfBoundsMemoryAccess, {asObject(memory), U64(address)}); }
	return (U32)address;
}

static I32 coerce32bitAddressSigned(Memory* memory, Iptr address)
{
	if(address >= INT32_MAX)
	{ throwException(ExceptionTypes::outOfBoundsMemoryAccess, {asObject(memory), U64(address)}); }
	return (I32)address;
}

//  0..62  = static data
// 63..63  = MutableGlobals
// 64..128 = aliased stack
// 129..   = dynamic memory
enum
{
	minStaticMemoryPages = 128
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

WAVM_DEFINE_INTRINSIC_GLOBAL(env, "STACKTOP", I32, STACKTOP, 64 * IR::numBytesPerPage);
WAVM_DEFINE_INTRINSIC_GLOBAL(env, "STACK_MAX", I32, STACK_MAX, 128 * IR::numBytesPerPage);
WAVM_DEFINE_INTRINSIC_GLOBAL(env,
							 "tempDoublePtr",
							 I32,
							 tempDoublePtr,
							 MutableGlobals::address + offsetof(MutableGlobals, tempDoublePtr));
WAVM_DEFINE_INTRINSIC_GLOBAL(env, "ABORT", I32, ABORT, 0);
WAVM_DEFINE_INTRINSIC_GLOBAL(env, "cttz_i8", I32, cttz_i8, 0);
WAVM_DEFINE_INTRINSIC_GLOBAL(env, "___dso_handle", U32, ___dso_handle, 0);
WAVM_DEFINE_INTRINSIC_GLOBAL(env,
							 "_stderr",
							 I32,
							 _stderr,
							 MutableGlobals::address + offsetof(MutableGlobals, _stderr));
WAVM_DEFINE_INTRINSIC_GLOBAL(env,
							 "_stdin",
							 I32,
							 _stdin,
							 MutableGlobals::address + offsetof(MutableGlobals, _stdin));
WAVM_DEFINE_INTRINSIC_GLOBAL(env,
							 "_stdout",
							 I32,
							 _stdout,
							 MutableGlobals::address + offsetof(MutableGlobals, _stdout));

WAVM_DEFINE_INTRINSIC_GLOBAL(env, "__memory_base", U32, memory_base, 1024);
WAVM_DEFINE_INTRINSIC_GLOBAL(env, "memoryBase", U32, emscriptenMemoryBase, 1024);

WAVM_DEFINE_INTRINSIC_GLOBAL(env, "__table_base", U32, table_base, 0);
WAVM_DEFINE_INTRINSIC_GLOBAL(env, "tableBase", U32, emscriptenTableBase, 0);

WAVM_DEFINE_INTRINSIC_GLOBAL(env,
							 "DYNAMICTOP_PTR",
							 U32,
							 DYNAMICTOP_PTR,
							 MutableGlobals::address + offsetof(MutableGlobals, DYNAMICTOP_PTR))
WAVM_DEFINE_INTRINSIC_GLOBAL(env, "_environ", U32, em_environ, 0)
WAVM_DEFINE_INTRINSIC_GLOBAL(env, "EMTSTACKTOP", U32, EMTSTACKTOP, 0)
WAVM_DEFINE_INTRINSIC_GLOBAL(env, "EMT_STACK_MAX", U32, EMT_STACK_MAX, 0)
WAVM_DEFINE_INTRINSIC_GLOBAL(env, "eb", I32, eb, 0)

static Emscripten::Instance* getEmscriptenInstance(Runtime::ContextRuntimeData* contextRuntimeData)
{
	auto instance = (Emscripten::Instance*)getUserData(
		getCompartmentFromContextRuntimeData(contextRuntimeData));
	wavmAssert(instance);
	wavmAssert(instance->memory);
	return instance;
}

static bool resizeHeap(Emscripten::Instance* instance, U32 desiredNumBytes)
{
	const Uptr desiredNumPages
		= (Uptr(desiredNumBytes) + IR::numBytesPerPage - 1) / IR::numBytesPerPage;
	const Uptr currentNumPages = Runtime::getMemoryNumPages(instance->memory);
	if(desiredNumPages > currentNumPages)
	{
		if(Runtime::growMemory(instance->memory, desiredNumPages - currentNumPages) == -1)
		{ return false; }

		return true;
	}
	else if(desiredNumPages < currentNumPages)
	{
		if(Runtime::shrinkMemory(instance->memory, currentNumPages - desiredNumPages) == -1)
		{ return false; }

		return true;
	}
	else
	{
		return true;
	}
}

static U32 dynamicAlloc(Emscripten::Instance* instance, U32 numBytes)
{
	MutableGlobals& mutableGlobals
		= memoryRef<MutableGlobals>(instance->memory, MutableGlobals::address);

	const U32 allocationAddress = mutableGlobals.DYNAMICTOP_PTR;
	const U32 endAddress = (allocationAddress + numBytes + 15) & -16;

	mutableGlobals.DYNAMICTOP_PTR = endAddress;

	if(endAddress > getMemoryNumPages(instance->memory) * IR::numBytesPerPage)
	{
		Uptr memoryMaxPages = getMemoryType(instance->memory).size.max;
		if(memoryMaxPages == UINT64_MAX) { memoryMaxPages = IR::maxMemoryPages; }

		if(endAddress > memoryMaxPages * IR::numBytesPerPage || !resizeHeap(instance, endAddress))
		{ throwException(ExceptionTypes::outOfMemory); }
	}

	return allocationAddress;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_emscripten_get_heap_size", U32, _emscripten_get_heap_size)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	return coerce32bitAddress(instance->memory,
							  Runtime::getMemoryNumPages(instance->memory) * IR::numBytesPerPage);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "getTotalMemory", U32, getTotalMemory)
{
	return _emscripten_get_heap_size(contextRuntimeData);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "abortOnCannotGrowMemory",
							   I32,
							   abortOnCannotGrowMemory,
							   I32 size)
{
	throwException(ExceptionTypes::calledAbort);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "abortStackOverflow", void, abortStackOverflow, I32 size)
{
	throwException(ExceptionTypes::stackOverflow);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_emscripten_resize_heap",
							   I32,
							   _emscripten_resize_heap,
							   U32 desiredNumBytes)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	return resizeHeap(instance, desiredNumBytes) ? 1 : 0;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_time", I32, _time, U32 address)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	time_t t = time(nullptr);
	if(address) { memoryRef<I32>(instance->memory, address) = (I32)t; }
	return (I32)t;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___errno_location", I32, ___errno_location) { return 0; }

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___setErrNo", void, ___seterrno, I32 value)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	if(instance->errnoAddress)
	{ memoryRef<I32>(instance->memory, instance->errnoAddress) = (I32)value; }
}

thread_local I32 tempRet0;
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "setTempRet0", void, setTempRet0, I32 value)
{
	tempRet0 = value;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "getTempRet0", I32, getTempRet0) { return tempRet0; }

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_sysconf", I32, _sysconf, I32 a)
{
	enum
	{
		sysConfPageSize = 30
	};
	switch(a)
	{
	case sysConfPageSize: return 16384;
	default: throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
	}
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_pthread_cond_wait", I32, _pthread_cond_wait, I32 a, I32 b)
{
	return 0;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_pthread_cond_broadcast", I32, _pthread_cond_broadcast, I32 a)
{
	return 0;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_pthread_equal", I32, _pthread_equal, I32 a, I32 b)
{
	return a == b;
}

static HashMap<U32, I32> pthreadSpecific = {};
static U32 pthreadSpecificNextKey = 0;

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_pthread_key_create",
							   I32,
							   _pthread_key_create,
							   U32 key,
							   I32 destructorPtr)
{
	if(key == 0) { return ErrNo::einval; }

	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	memoryRef<U32>(instance->memory, key) = pthreadSpecificNextKey;
	pthreadSpecific.set(pthreadSpecificNextKey, 0);
	pthreadSpecificNextKey++;

	return 0;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_pthread_mutex_lock", I32, _pthread_mutex_lock, I32 a)
{
	return 0;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_pthread_mutex_unlock", I32, _pthread_mutex_unlock, I32 a)
{
	return 0;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
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
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_pthread_getspecific", I32, _pthread_getspecific, U32 key)
{
	const I32* value = pthreadSpecific.get(key);
	return value ? *value : 0;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_pthread_once", I32, _pthread_once, I32 a, I32 b)
{
	throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_pthread_cleanup_push",
							   void,
							   _pthread_cleanup_push,
							   I32 a,
							   I32 b)
{
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_pthread_cleanup_pop", void, _pthread_cleanup_pop, I32 a) {}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_pthread_self", I32, _pthread_self) { return 0; }

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_pthread_attr_init", I32, _pthread_attr_init, I32 address)
{
	return 0;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_pthread_attr_destroy",
							   I32,
							   _pthread_attr_destroy,
							   I32 address)
{
	return 0;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_pthread_getattr_np",
							   I32,
							   _pthread_getattr_np,
							   I32 thread,
							   I32 address)
{
	return 0;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_pthread_attr_getstack",
							   I32,
							   _pthread_attr_getstack,
							   U32 attrAddress,
							   U32 stackBaseAddress,
							   U32 stackSizeAddress)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	memoryRef<U32>(instance->memory, stackBaseAddress) = STACKTOP.getValue().u32;
	memoryRef<U32>(instance->memory, stackSizeAddress)
		= STACK_MAX.getValue().u32 - STACKTOP.getValue().u32;
	return 0;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___ctype_b_loc", U32, ___ctype_b_loc)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
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
		vmAddress = coerce32bitAddress(instance->memory, dynamicAlloc(instance, sizeof(data)));
		memcpy(memoryArrayPtr<U8>(instance->memory, vmAddress, sizeof(data)), data, sizeof(data));
	}
	return vmAddress + sizeof(short) * 128;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___ctype_toupper_loc", U32, ___ctype_toupper_loc)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
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
		vmAddress = coerce32bitAddress(instance->memory, dynamicAlloc(instance, sizeof(data)));
		memcpy(memoryArrayPtr<U8>(instance->memory, vmAddress, sizeof(data)), data, sizeof(data));
	}
	return vmAddress + sizeof(I32) * 128;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___ctype_tolower_loc", U32, ___ctype_tolower_loc)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
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
		vmAddress = coerce32bitAddress(instance->memory, dynamicAlloc(instance, sizeof(data)));
		memcpy(memoryArrayPtr<U8>(instance->memory, vmAddress, sizeof(data)), data, sizeof(data));
	}
	return vmAddress + sizeof(I32) * 128;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "___assert_fail",
							   void,
							   ___assert_fail,
							   I32 condition,
							   I32 filename,
							   I32 line,
							   I32 function)
{
	throwException(Runtime::ExceptionTypes::calledAbort);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___cxa_atexit", I32, ___cxa_atexit, I32 a, I32 b, I32 c)
{
	return 0;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___cxa_guard_acquire", I32, ___cxa_guard_acquire, U32 address)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	if(!memoryRef<U8>(instance->memory, address))
	{
		memoryRef<U8>(instance->memory, address) = 1;
		return 1;
	}
	else
	{
		return 0;
	}
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___cxa_guard_release", void, ___cxa_guard_release, I32 a) {}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___cxa_throw", void, ___cxa_throw, I32 a, I32 b, I32 c)
{
	throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___cxa_begin_catch", I32, ___cxa_begin_catch, I32 a)
{
	throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "___cxa_allocate_exception",
							   U32,
							   ___cxa_allocate_exception,
							   U32 size)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	return coerce32bitAddress(instance->memory, dynamicAlloc(instance, size));
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "__ZSt18uncaught_exceptionv", I32, __ZSt18uncaught_exceptionv)
{
	throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_abort", void, emscripten__abort)
{
	throwException(Runtime::ExceptionTypes::calledAbort);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_exit", void, emscripten__exit, I32 code)
{
	throwException(Runtime::ExceptionTypes::calledAbort);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "abort", void, emscripten_abort, I32 code)
{
	Log::printf(Log::error, "env.abort(%i)\n", code);
	throwException(Runtime::ExceptionTypes::calledAbort);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_i", void, emscripten_nullFunc_i, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_ii", void, emscripten_nullFunc_ii, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_iii", void, emscripten_nullFunc_iii, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_iiii", void, emscripten_nullFunc_iiii, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_iiiii", void, emscripten_nullFunc_iiiii, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_iiiiii", void, emscripten_nullFunc_iiiiii, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_iiiiiii", void, emscripten_nullFunc_iiiiiii, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_v", void, emscripten_nullFunc_v, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_vi", void, emscripten_nullFunc_vi, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_vii", void, emscripten_nullFunc_vii, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_viii", void, emscripten_nullFunc_viii, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_viiii", void, emscripten_nullFunc_viiii, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_viiiii", void, emscripten_nullFunc_viiiii, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_viiiiii", void, emscripten_nullFunc_viiiiii, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_iidiiii", void, emscripten_nullFunc_iidiiii, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "nullFunc_jiji", void, emscripten_nullFunc_jiji, I32 code)
{
	throwException(Runtime::ExceptionTypes::uninitializedTableElement);
}

static U32 currentLocale = 0;
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_uselocale", I32, _uselocale, I32 locale)
{
	auto oldLocale = currentLocale;
	currentLocale = locale;
	return oldLocale;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_newlocale", U32, _newlocale, I32 mask, I32 locale, I32 base)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	if(!base) { base = coerce32bitAddress(instance->memory, dynamicAlloc(instance, 4)); }
	return base;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_freelocale", void, emscripten__freelocale, I32 a) {}

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_strftime_l",
							   I32,
							   emscripten__strftime_l,
							   I32 a,
							   I32 b,
							   I32 c,
							   I32 d,
							   I32 e)
{
	throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_strerror", I32, emscripten__strerror, I32 a)
{
	throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_catopen", I32, emscripten__catopen, I32 a, I32 b)
{
	return -1;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
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
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_catclose", I32, emscripten__catclose, I32 a) { return 0; }

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_emscripten_memcpy_big",
							   U32,
							   _emscripten_memcpy_big,
							   U32 sourceAddress,
							   U32 destAddress,
							   U32 numBytes)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	memcpy(memoryArrayPtr<U8>(instance->memory, sourceAddress, numBytes),
		   memoryArrayPtr<U8>(instance->memory, destAddress, numBytes),
		   numBytes);
	return sourceAddress;
}

enum class ioStreamVMHandle
{
	StdIn = 0,
	StdOut = 1,
	StdErr = 2,
};
static VFS::VFD* getFD(Emscripten::Instance* instance, U32 vmHandle)
{
	switch((ioStreamVMHandle)vmHandle)
	{
	case ioStreamVMHandle::StdIn: return instance->stdIn;
	case ioStreamVMHandle::StdOut: return instance->stdOut;
	case ioStreamVMHandle::StdErr: return instance->stdErr;
	default: return nullptr;
	}
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_vfprintf",
							   I32,
							   _vfprintf,
							   I32 file,
							   U32 formatPointer,
							   I32 argList)
{
	throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_fread",
							   U32,
							   _fread,
							   U32 destAddress,
							   U32 size,
							   U32 count,
							   I32 file)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	VFS::VFD* fd = getFD(instance, file);
	if(!fd) { return 0; }

	const U64 numBytes64 = U64(size) * U64(count);
	errorUnless(numBytes64 <= UINTPTR_MAX);
	const Uptr numBytes = Uptr(numBytes64);
	Uptr numBytesRead = 0;
	const VFS::Result result = fd->read(
		memoryArrayPtr<U8>(instance->memory, destAddress, numBytes), numBytes, &numBytesRead);
	if(result != VFS::Result::success) { return 0; }
	else
	{
		wavmAssert(numBytesRead < UINT32_MAX);
		return U32(numBytesRead);
	}
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_fwrite",
							   U32,
							   _fwrite,
							   U32 sourceAddress,
							   U32 size,
							   U32 count,
							   I32 file)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	VFS::VFD* fd = getFD(instance, file);
	if(!fd) { return 0; }

	const U64 numBytes64 = U64(size) * U64(count);
	errorUnless(numBytes64 <= UINTPTR_MAX);
	const Uptr numBytes = Uptr(numBytes64);
	Uptr numBytesWritten = 0;
	const VFS::Result result = fd->write(
		memoryArrayPtr<U8>(instance->memory, sourceAddress, numBytes), numBytes, &numBytesWritten);
	if(result != VFS::Result::success) { return 0; }
	else
	{
		wavmAssert(numBytesWritten < UINT32_MAX);
		return U32(numBytesWritten);
	}
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_fputc", I32, _fputc, I32 character, I32 file)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	VFS::VFD* fd = getFD(instance, file);
	if(!fd) { return -1; }

	char c = char(character);

	Uptr numBytesWritten = 0;
	const VFS::Result result = fd->write(&c, 1, &numBytesWritten);
	return result == VFS::Result::success ? character : -1;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_fflush", I32, _fflush, I32 file)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	VFS::VFD* fd = getFD(instance, file);
	if(!fd) { return -1; }

	return fd->sync(VFS::SyncType::contentsAndMetadata) == VFS::Result::success ? 0 : -1;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___lock", void, ___lock, I32 a) {}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___unlock", void, ___unlock, I32 a) {}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___lockfile", I32, ___lockfile, I32 a) { return 1; }
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___unlockfile", void, ___unlockfile, I32 a) {}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___syscall6", I32, ___syscall6, I32 a, I32 b)
{
	// close
	throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___syscall54", I32, ___syscall54, I32 a, I32 b)
{
	// ioctl
	return 0;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___syscall140", I32, ___syscall140, I32 a, I32 b)
{
	// llseek
	throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___syscall145", I32, _readv, I32, I32 argsPtr)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);

	U32* args = memoryArrayPtr<U32>(instance->memory, argsPtr, 3);
	U32 file = args[0];
	U32 iov = args[1];
	U32 iovcnt = args[2];

	VFS::VFD* fd = getFD(instance, file);
	if(!fd) { return -9; /* EBADF */ }

	Uptr totalNumBytesRead = 0;
	for(U32 i = 0; i < iovcnt; i++)
	{
		const U32 destAddress = memoryRef<U32>(instance->memory, iov + i * 8);
		const U32 numBytes = memoryRef<U32>(instance->memory, iov + i * 8 + 4);

		Uptr numBytesRead = 0;
		if(numBytes > 0
		   && fd->read(memoryArrayPtr<U8>(instance->memory, destAddress, numBytes),
					   numBytes,
					   &numBytesRead)
				  != VFS::Result::success)
		{ return -1; }
		totalNumBytesRead += numBytesRead;
		if(numBytesRead < numBytes) { break; }
	}
	return coerce32bitAddressSigned(instance->memory, totalNumBytesRead);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___syscall146", I32, _writev, I32, U32 argsPtr)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);

	U32* args = memoryArrayPtr<U32>(instance->memory, argsPtr, 3);
	U32 file = args[0];
	U32 iov = args[1];
	U32 iovcnt = args[2];

	VFS::VFD* fd = getFD(instance, file);
	if(!fd) { return -9; /* EBADF */ }

	Uptr totalNumBytesWritten = 0;
	for(U32 i = 0; i < iovcnt; i++)
	{
		const U32 sourceAddress = memoryRef<U32>(instance->memory, iov + i * 8);
		const U32 numBytes = memoryRef<U32>(instance->memory, iov + i * 8 + 4);

		Uptr numBytesWritten = 0;
		if(fd->write(memoryArrayPtr<U8>(instance->memory, sourceAddress, numBytes),
					 numBytes,
					 &numBytesWritten)
		   != VFS::Result::success)
		{ return -1; }
		totalNumBytesWritten += numBytesWritten;
		if(numBytesWritten < numBytes) { break; }
	}
	errorUnless(totalNumBytesWritten <= INT32_MAX);
	return I32(totalNumBytesWritten);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(asm2wasm, "f64-to-int", I32, f64_to_int, F64 f) { return (I32)f; }

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

WAVM_DEFINE_INTRINSIC_GLOBAL(global, "NaN", F64, NaN, makeNaN())
WAVM_DEFINE_INTRINSIC_GLOBAL(global, "Infinity", F64, Infinity, makeInf())

WAVM_DEFINE_INTRINSIC_FUNCTION(asm2wasm, "i32u-rem", U32, I32_remu, U32 left, U32 right)
{
	return left % right;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(asm2wasm, "i32s-rem", I32, I32_rems, I32 left, I32 right)
{
	return left % right;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(asm2wasm, "i32u-div", U32, I32_divu, U32 left, U32 right)
{
	return left / right;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(asm2wasm, "i32s-div", I32, I32_divs, I32 left, I32 right)
{
	return left / right;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(asm2wasm, "f64-rem", F64, F64_rems, F64 left, F64 right)
{
	return (F64)fmod(left, right);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_getenv", I32, _getenv, I32 a) { return 0; }

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_gettimeofday", I32, _gettimeofday, I32 timevalAddress, I32)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);

	const I128 realtimeClock = Platform::getRealtimeClock();
	memoryRef<U32>(instance->memory, timevalAddress + 0) = U32(realtimeClock / 1000000000);
	memoryRef<U32>(instance->memory, timevalAddress + 4) = U32(realtimeClock / 1000 % 1000000000);
	return 0;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_sem_init", I32, _sem_init, I32 a, I32 b, I32 c) { return 0; }

DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "invoke_ii", U32, invoke_ii, U32 index, U32 a);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
										"invoke_iii",
										U32,
										invoke_iii,
										U32 index,
										U32 a,
										U32 b);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "invoke_v", void, invoke_v, U32 index);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
										"invoke_vii",
										void,
										invoke_vii,
										U32 index,
										U32 a,
										U32 b);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "___buildEnvironment", void, ___buildEnvironment, U32);

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___cxa_pure_virtual", void, ___cxa_pure_virtual)
{
	Errors::fatal("env.__cxa_pure_virtual called");
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___syscall192", I32, _mmap2, U32 which, U32 varargsAddress)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);

	// const U32 address = memoryRef<U32>(instance->memory, varargsAddress + 0);
	const U32 numBytes = memoryRef<U32>(instance->memory, varargsAddress + 4);
	// const U32 protection = memoryRef<U32>(instance->memory, varargsAddress + 8);
	// const U32 flags = memoryRef<U32>(instance->memory, varargsAddress + 12);
	const I32 file = memoryRef<U32>(instance->memory, varargsAddress + 16);
	// const U32 offset = memoryRef<U32>(instance->memory, varargsAddress + 20);

	if(file != -1) { return -38; /* ENOSYS */ }
	else
	{
		Function* memalignFunction
			= asFunctionNullable(getInstanceExport(instance->moduleInstance, "_memalign"));
		if(!memalignFunction
		   || getFunctionType(memalignFunction)
				  != FunctionType({ValueType::i32}, {ValueType::i32, ValueType::i32}))
		{ return -38; /* ENOSYS */ }

		UntaggedValue memalignArgs[2] = {U32(16384), numBytes};
		const U32 memalignResult
			= invokeFunctionUnchecked(
				  getContextFromRuntimeData(contextRuntimeData), memalignFunction, memalignArgs)
				  ->i32;
		if(!memalignResult) { return -12; /* ENOMEM */ }
		memset(memoryArrayPtr<char>(instance->memory, memalignResult, numBytes), 0, numBytes);

		errorUnless(memalignResult < INT32_MAX);
		return I32(memalignResult);
	}
}
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "___syscall195", U32, ___syscall195, U32, U32);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "___syscall20", U32, ___syscall20, U32, U32);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "___syscall221", U32, ___syscall221, U32, U32);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "___syscall5", U32, ___syscall5, U32, U32);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "___syscall91", U32, ___syscall91, U32, U32);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "_atexit", U32, _atexit, U32);

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_clock_gettime",
							   I32,
							   _clock_gettime,
							   U32 clockId,
							   U32 timespecAddress)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);

	I128 clock128;
	switch(clockId)
	{
	case 0:
		// CLOCK_REALTIME
		clock128 = Platform::getRealtimeClock();
		break;
	case 1:
		// CLOCK_MONOTONIC
		clock128 = Platform::getMonotonicClock();
		break;
	case 2:
		// CLOCK_PROCESS_CPUTIME_ID
		clock128 = Platform::getRealtimeClock();
		break;
	case 3:
		// CLOCK_THREAD_CPUTIME_ID
		clock128 = Platform::getProcessClock();
		break;
	default: return -1;
	}
	const I128 realtimeClock = Platform::getRealtimeClock();

	memoryRef<U32>(instance->memory, timespecAddress + 0) = U32(realtimeClock / 1000000000);
	memoryRef<U32>(instance->memory, timespecAddress + 4) = U32(realtimeClock % 1000000000);

	return 0;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_getpagesize", U32, _getpagesize) { return 16384; }
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "_llvm_log10_f64", F64, _llvm_log10_f64, F64);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "_llvm_log2_f64", F64, _llvm_log2_f64, F64);
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_llvm_trap", void, _llvm_trap)
{
	Errors::fatal("env._llvm_trap called");
}
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "_llvm_trunc_f64", F64, _llvm_trunc_f64, F64);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "_localtime_r", U32, _localtime_r, U32, U32);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "_longjmp", void, _longjmp, U32, U32);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "_mktime", U32, _mktime, U32);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
										"_pthread_cond_destroy",
										U32,
										_pthread_cond_destroy,
										U32);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
										"_pthread_cond_init",
										U32,
										_pthread_cond_init,
										U32,
										U32);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
										"_pthread_cond_signal",
										U32,
										_pthread_cond_signal,
										U32);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
										"_pthread_cond_timedwait",
										U32,
										_pthread_cond_timedwait,
										U32,
										U32,
										U32);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
										"_pthread_create",
										U32,
										_pthread_create,
										U32,
										U32,
										U32,
										U32);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "_pthread_detach", U32, _pthread_detach, U32);
DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "_pthread_join", U32, _pthread_join, U32, U32);
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_pthread_mutexattr_destroy",
							   U32,
							   _pthread_mutexattr_destroy,
							   U32 attrAddress)
{
	return 0;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_pthread_mutexattr_init",
							   U32,
							   _pthread_mutexattr_init,
							   U32 attrAddress)
{
	return 0;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_pthread_mutexattr_settype",
							   U32,
							   _pthread_mutexattr_settype,
							   U32 attrAddress,
							   U32 type)
{
	return 0;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_sched_yield", U32, _sched_yield) { return 0; }
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_sem_destroy", U32, _sem_destroy, U32) { return 0; }
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_strftime", U32, _strftime, U32, U32, U32, U32) { return 0; }
// WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_tzset", void, _tzset) { }

Emscripten::Instance* Emscripten::instantiate(Compartment* compartment, const IR::Module& module)
{
	MemoryType memoryType(false, SizeConstraints{0, 0});
	if(module.memories.imports.size() && module.memories.imports[0].moduleName == "env"
	   && module.memories.imports[0].exportName == "memory")
	{
		memoryType = module.memories.imports[0].type;
		if(memoryType.size.max >= minStaticMemoryPages)
		{
			if(memoryType.size.min <= minStaticMemoryPages)
			{
				// Enlarge the initial memory to make space for the stack and mutable globals.
				memoryType.size.min = minStaticMemoryPages;
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

	TableType tableType(ReferenceType::funcref, false, SizeConstraints{0, 0});
	if(module.tables.imports.size() && module.tables.imports[0].moduleName == "env"
	   && module.tables.imports[0].exportName == "table")
	{ tableType = module.tables.imports[0].type; }

	Memory* memory = Runtime::createMemory(compartment, memoryType, "env.memory");
	Table* table = Runtime::createTable(compartment, tableType, nullptr, "env.table");

	HashMap<std::string, Runtime::Object*> extraEnvExports = {
		{"memory", Runtime::asObject(memory)},
		{"table", Runtime::asObject(table)},
	};

	Instance* instance = new Instance;
	instance->env = Intrinsics::instantiateModule(
		compartment, {WAVM_INTRINSIC_MODULE_REF(env)}, "env", extraEnvExports);
	instance->asm2wasm = Intrinsics::instantiateModule(
		compartment, {WAVM_INTRINSIC_MODULE_REF(asm2wasm)}, "asm2wasm");
	instance->global
		= Intrinsics::instantiateModule(compartment, {WAVM_INTRINSIC_MODULE_REF(global)}, "global");

	unwindSignalsAsExceptions([=] {
		MutableGlobals& mutableGlobals = memoryRef<MutableGlobals>(memory, MutableGlobals::address);

		mutableGlobals.DYNAMICTOP_PTR = STACK_MAX.getValue().i32;
		mutableGlobals._stderr = (U32)ioStreamVMHandle::StdErr;
		mutableGlobals._stdin = (U32)ioStreamVMHandle::StdIn;
		mutableGlobals._stdout = (U32)ioStreamVMHandle::StdOut;
	});

	instance->memory = memory;

	setUserData(compartment, instance, nullptr);

	return instance;
}

void Emscripten::initializeGlobals(Emscripten::Instance* instance,
								   Context* context,
								   const IR::Module& module,
								   ModuleInstance* moduleInstance)
{
	instance->moduleInstance = moduleInstance;

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

	// Call the global initializer functions: newer Emscripten uses a single globalCtors function,
	// and older Emscripten uses a __GLOBAL__* function for each translation unit.
	if(Function* globalCtors = asFunctionNullable(getInstanceExport(moduleInstance, "globalCtors")))
	{ Runtime::invokeFunctionChecked(context, globalCtors, {}); }

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

	// Store ___errno_location.
	Function* errNoLocation
		= asFunctionNullable(getInstanceExport(moduleInstance, "___errno_location"));
	if(errNoLocation
	   && getFunctionType(errNoLocation) == FunctionType(TypeTuple{ValueType::i32}, TypeTuple{}))
	{
		IR::ValueTuple errNoResult = Runtime::invokeFunctionChecked(context, errNoLocation, {});
		if(errNoResult.size() == 1 && errNoResult[0].type == ValueType::i32)
		{ instance->errnoAddress = errNoResult[0].i32; }
	}
}

void Emscripten::injectCommandArgs(Emscripten::Instance* instance,
								   const std::vector<const char*>& argStrings,
								   std::vector<IR::Value>& outInvokeArgs)
{
	U8* memoryBase = getMemoryBaseAddress(instance->memory);

	U32* argvOffsets
		= (U32*)(memoryBase + dynamicAlloc(instance, (U32)(sizeof(U32) * (argStrings.size() + 1))));
	for(Uptr argIndex = 0; argIndex < argStrings.size(); ++argIndex)
	{
		auto stringSize = strlen(argStrings[argIndex]) + 1;
		auto stringMemory = memoryBase + dynamicAlloc(instance, (U32)stringSize);
		memcpy(stringMemory, argStrings[argIndex], stringSize);
		argvOffsets[argIndex] = (U32)(stringMemory - memoryBase);
	}
	argvOffsets[argStrings.size()] = 0;
	outInvokeArgs = {(U32)argStrings.size(), (U32)((U8*)argvOffsets - memoryBase)};
}

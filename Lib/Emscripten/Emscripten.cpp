#include "WAVM/Emscripten/Emscripten.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <atomic>
#include <initializer_list>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include "EmscriptenABI.h"
#include "EmscriptenPrivate.h"
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/FloatComponents.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/Inline/Time.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Clock.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/VFS/VFS.h"

#ifndef _WIN32
#include <sys/uio.h>
#endif

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;
using namespace WAVM::Emscripten;

WAVM_DEFINE_INTRINSIC_MODULE(env)
WAVM_DEFINE_INTRINSIC_MODULE(asm2wasm)
WAVM_DEFINE_INTRINSIC_MODULE(global)

static emabi::Result asEmscriptenErrNo(VFS::Result result)
{
	switch(result)
	{
	case VFS::Result::success: return emabi::esuccess;
	case VFS::Result::ioPending: return emabi::einprogress;
	case VFS::Result::ioDeviceError: return emabi::eio;
	case VFS::Result::interruptedBySignal: return emabi::eintr;
	case VFS::Result::interruptedByCancellation: return emabi::eintr;
	case VFS::Result::wouldBlock: return emabi::eagain;
	case VFS::Result::inaccessibleBuffer: return emabi::efault;
	case VFS::Result::invalidOffset: return emabi::einval;
	case VFS::Result::notSeekable: return emabi::espipe;
	case VFS::Result::notPermitted: return emabi::eperm;
	case VFS::Result::notAccessible: return emabi::eacces;
	case VFS::Result::notSynchronizable: return emabi::einval;
	case VFS::Result::tooManyBufferBytes: return emabi::einval;
	case VFS::Result::notEnoughBufferBytes: return emabi::einval;
	case VFS::Result::tooManyBuffers: return emabi::einval;
	case VFS::Result::notEnoughBits: return emabi::eoverflow;
	case VFS::Result::exceededFileSizeLimit: return emabi::efbig;
	case VFS::Result::outOfSystemFDs: return emabi::enfile;
	case VFS::Result::outOfProcessFDs: return emabi::emfile;
	case VFS::Result::outOfMemory: return emabi::enomem;
	case VFS::Result::outOfQuota: return emabi::edquot;
	case VFS::Result::outOfFreeSpace: return emabi::enospc;
	case VFS::Result::outOfLinksToParentDir: return emabi::emlink;
	case VFS::Result::invalidNameCharacter: return emabi::eacces;
	case VFS::Result::nameTooLong: return emabi::enametoolong;
	case VFS::Result::tooManyLinksInPath: return emabi::eloop;
	case VFS::Result::alreadyExists: return emabi::eexist;
	case VFS::Result::doesNotExist: return emabi::enoent;
	case VFS::Result::isDirectory: return emabi::eisdir;
	case VFS::Result::isNotDirectory: return emabi::enotdir;
	case VFS::Result::isNotEmpty: return emabi::enotempty;
	case VFS::Result::brokenPipe: return emabi::epipe;
	case VFS::Result::missingDevice: return emabi::enxio;
	case VFS::Result::busy: return emabi::ebusy;

	default: WAVM_UNREACHABLE();
	};
}

//  0..62 = static data
// 63..63 = MutableGlobals
// 64..96 = aliased stack
// 97..   = dynamic memory
static constexpr U64 minStaticMemoryPages = 97;

static constexpr emabi::Address mainThreadStackAddress = 64 * IR::numBytesPerPage;
static constexpr emabi::Address mainThreadNumStackBytes = 32 * IR::numBytesPerPage;

struct MutableGlobals
{
	enum
	{
		address = 63 * IR::numBytesPerPage
	};

	emabi::Address DYNAMICTOP_PTR;
	F64 tempDoublePtr;
	emabi::FD _stderr;
	emabi::FD _stdin;
	emabi::FD _stdout;
};

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

bool Emscripten::resizeHeap(Emscripten::Instance* instance, U32 desiredNumBytes)
{
	const Uptr desiredNumPages
		= (Uptr(desiredNumBytes) + IR::numBytesPerPage - 1) / IR::numBytesPerPage;
	const Uptr currentNumPages = Runtime::getMemoryNumPages(instance->memory);
	if(desiredNumPages > currentNumPages)
	{
		if(!Runtime::growMemory(instance->memory, desiredNumPages - currentNumPages))
		{ return false; }

		return true;
	}
	else if(desiredNumPages < currentNumPages)
	{
		return false;
	}
	else
	{
		return true;
	}
}

emabi::Address Emscripten::dynamicAlloc(Emscripten::Instance* instance, emabi::Size numBytes)
{
	MutableGlobals& mutableGlobals
		= memoryRef<MutableGlobals>(instance->memory, MutableGlobals::address);

	const emabi::Address allocationAddress = mutableGlobals.DYNAMICTOP_PTR;
	const emabi::Address endAddress = (allocationAddress + numBytes + 15) & -16;

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

void Emscripten::initThreadLocals(Thread* thread)
{
	// Call the establishStackSpace function exported by the module to set the thread's stack
	// address and size.
	Function* establishStackSpace = asFunctionNullable(
		getInstanceExport(thread->instance->moduleInstance, "establishStackSpace"));
	if(establishStackSpace
	   && getFunctionType(establishStackSpace)
			  == FunctionType(TypeTuple{}, TypeTuple{ValueType::i32, ValueType::i32}))
	{
		IR::UntaggedValue args[2]{thread->stackAddress, thread->numStackBytes};
		invokeFunction(thread->context,
					   establishStackSpace,
					   FunctionType({}, {ValueType::i32, ValueType::i32}),
					   args);
	}
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_emscripten_get_heap_size",
							   emabi::Result,
							   _emscripten_get_heap_size)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	return coerce32bitAddress(instance->memory,
							  Runtime::getMemoryNumPages(instance->memory) * IR::numBytesPerPage);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "getTotalMemory", emabi::Result, getTotalMemory)
{
	return _emscripten_get_heap_size(contextRuntimeData);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "abortOnCannotGrowMemory",
							   emabi::Result,
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
							   emabi::Result,
							   _emscripten_resize_heap,
							   U32 desiredNumBytes)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	return resizeHeap(instance, desiredNumBytes) ? 1 : 0;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_time", emabi::Result, _time, U32 address)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	time_t t = time(nullptr);
	if(address) { memoryRef<I32>(instance->memory, address) = (I32)t; }
	return (emabi::Result)t;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___errno_location", emabi::Result, ___errno_location)
{
	return emabi::esuccess;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___setErrNo", void, ___seterrno, I32 value)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	if(instance->errnoAddress)
	{ memoryRef<I32>(instance->memory, instance->errnoAddress) = (I32)value; }
}

static thread_local I32 tempRet0;
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "setTempRet0", void, setTempRet0, I32 value)
{
	tempRet0 = value;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "getTempRet0", emabi::Result, getTempRet0) { return tempRet0; }

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_sysconf", emabi::Result, _sysconf, I32 a)
{
	enum : U32
	{
		sysConfPageSize = 30
	};
	switch(a)
	{
	case sysConfPageSize: return 16384;
	default: throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
	}
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___ctype_b_loc", emabi::Address, ___ctype_b_loc)
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
	static emabi::Address vmAddress = 0;
	if(vmAddress == 0)
	{
		vmAddress = coerce32bitAddress(instance->memory, dynamicAlloc(instance, sizeof(data)));
		memcpy(memoryArrayPtr<U8>(instance->memory, vmAddress, sizeof(data)), data, sizeof(data));
	}
	return vmAddress + sizeof(short) * 128;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___ctype_toupper_loc", emabi::Address, ___ctype_toupper_loc)
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
	static emabi::Address vmAddress = 0;
	if(vmAddress == 0)
	{
		vmAddress = coerce32bitAddress(instance->memory, dynamicAlloc(instance, sizeof(data)));
		memcpy(memoryArrayPtr<U8>(instance->memory, vmAddress, sizeof(data)), data, sizeof(data));
	}
	return vmAddress + sizeof(I32) * 128;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___ctype_tolower_loc", emabi::Address, ___ctype_tolower_loc)
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
	static emabi::Address vmAddress = 0;
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

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "___cxa_atexit",
							   emabi::Result,
							   ___cxa_atexit,
							   I32 a,
							   I32 b,
							   I32 c)
{
	return emabi::esuccess;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "___cxa_guard_acquire",
							   emabi::Result,
							   ___cxa_guard_acquire,
							   U32 address)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	if(!memoryRef<U8>(instance->memory, address))
	{
		memoryRef<U8>(instance->memory, address) = 1;
		return 1;
	}
	else
	{
		return emabi::esuccess;
	}
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___cxa_guard_release", void, ___cxa_guard_release, I32 a) {}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___cxa_throw", void, ___cxa_throw, I32 a, I32 b, I32 c)
{
	throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___cxa_begin_catch", emabi::Address, ___cxa_begin_catch, I32 a)
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
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "__ZSt18uncaught_exceptionv",
							   emabi::Result,
							   __ZSt18uncaught_exceptionv)
{
	throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_abort", void, emscripten__abort)
{
	throwException(Runtime::ExceptionTypes::calledAbort);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_exit", void, emscripten__exit, U32 code)
{
	throw Emscripten::ExitException{code};
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

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_uselocale",
							   emabi::Address,
							   _uselocale,
							   emabi::Address newLocale)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	return instance->currentLocale.exchange(newLocale);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_newlocale",
							   emabi::Address,
							   _newlocale,
							   I32 mask,
							   emabi::Address locale,
							   emabi::Address base)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	if(!base) { base = coerce32bitAddress(instance->memory, dynamicAlloc(instance, 4)); }
	return base;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_freelocale", void, emscripten__freelocale, emabi::Address a)
{
}

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
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_strerror", emabi::Address, emscripten__strerror, I32 a)
{
	throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_catopen", emabi::Result, emscripten__catopen, I32 a, I32 b)
{
	return emabi::enosys;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_catgets",
							   emabi::Result,
							   emscripten__catgets,
							   I32 catd,
							   I32 set_id,
							   I32 msg_id,
							   I32 s)
{
	return s;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_catclose", emabi::Result, emscripten__catclose, I32 a)
{
	return emabi::esuccess;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_emscripten_memcpy_big",
							   emabi::Address,
							   _emscripten_memcpy_big,
							   emabi::Address sourceAddress,
							   emabi::Address destAddress,
							   emabi::Size numBytes)
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
static VFS::VFD* getFD(Emscripten::Instance* instance, emabi::FD vmHandle)
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
							   emabi::Result,
							   _vfprintf,
							   emabi::FD file,
							   emabi::Address formatStringAddress,
							   emabi::Address argListAddress)
{
	throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_fread",
							   emabi::Result,
							   _fread,
							   U32 destAddress,
							   U32 size,
							   U32 count,
							   emabi::FD file)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	VFS::VFD* fd = getFD(instance, file);
	if(!fd) { return emabi::esuccess; }

	const U64 numBytes64 = U64(size) * U64(count);
	WAVM_ERROR_UNLESS(numBytes64 <= UINTPTR_MAX);
	const Uptr numBytes = Uptr(numBytes64);
	Uptr numBytesRead = 0;
	const VFS::Result result = fd->read(
		memoryArrayPtr<U8>(instance->memory, destAddress, numBytes), numBytes, &numBytesRead);
	if(result != VFS::Result::success) { return emabi::esuccess; }
	else
	{
		WAVM_ASSERT(numBytesRead < UINT32_MAX);
		return U32(numBytesRead);
	}
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_fwrite",
							   emabi::Result,
							   _fwrite,
							   emabi::Address sourceAddress,
							   emabi::Size size,
							   emabi::Size count,
							   emabi::FD file)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	VFS::VFD* fd = getFD(instance, file);
	if(!fd) { return emabi::esuccess; }

	const U64 numBytes64 = U64(size) * U64(count);
	const Uptr numBytes = Uptr(numBytes64);
	if(numBytes > emabi::sizeMax || numBytes > emabi::resultMax) { return emabi::eoverflow; }
	Uptr numBytesWritten = 0;
	const VFS::Result result = fd->write(
		memoryArrayPtr<U8>(instance->memory, sourceAddress, numBytes), numBytes, &numBytesWritten);
	if(result != VFS::Result::success) { return emabi::esuccess; }
	else
	{
		WAVM_ASSERT(numBytesWritten < emabi::resultMax);
		return emabi::Result(numBytesWritten);
	}
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_fputc", emabi::Result, _fputc, I32 character, emabi::FD file)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	VFS::VFD* fd = getFD(instance, file);
	if(!fd) { return emabi::ebadf; }

	char c = char(character);

	Uptr numBytesWritten = 0;
	const VFS::Result result = fd->write(&c, 1, &numBytesWritten);
	return result == VFS::Result::success ? emabi::esuccess : asEmscriptenErrNo(result);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_fflush", emabi::Result, _fflush, emabi::FD file)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	VFS::VFD* fd = getFD(instance, file);
	if(!fd) { return emabi::ebadf; }

	return fd->sync(VFS::SyncType::contentsAndMetadata) == VFS::Result::success ? 0 : -1;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___lock", void, ___lock, I32 a) {}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___unlock", void, ___unlock, I32 a) {}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___lockfile", emabi::Result, ___lockfile, I32 a)
{
	return emabi::enosys;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___unlockfile", void, ___unlockfile, I32 a) {}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___syscall6", I32, emscripten_close, emabi::FD file, I32)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);
	VFS::VFD* fd = getFD(instance, file);
	if(!fd) { return emabi::ebadf; }

	return emabi::enosys;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___syscall54", I32, emscripten_ioctl, I32 a, I32 b)
{
	// ioctl
	return emabi::esuccess;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___syscall140", I32, emscripten_llseek, I32 a, I32 b)
{
	// llseek
	throwException(Runtime::ExceptionTypes::calledUnimplementedIntrinsic);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "___syscall145",
							   emabi::Result,
							   emscripten_readv,
							   I32,
							   emabi::Address argsPtr)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);

	U32* args = memoryArrayPtr<U32>(instance->memory, argsPtr, 3);
	emabi::FD file = args[0];
	U32 iov = args[1];
	U32 iovcnt = args[2];

	VFS::VFD* fd = getFD(instance, file);
	if(!fd) { return emabi::ebadf; }

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
	return coerce32bitAddressResult(instance->memory, totalNumBytesRead);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "___syscall4",
							   emabi::Result,
							   emscripten_write,
							   I32,
							   emabi::Address argsAddress)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);

	emabi::Result result = 0;
	Runtime::catchRuntimeExceptions(
		[&] {
			struct ArgStruct
			{
				emabi::FD file;
				emabi::Address bufferAddress;
				emabi::Address numBytes;
			};
			ArgStruct args = memoryRef<ArgStruct>(instance->memory, argsAddress);

			VFS::VFD* fd = getFD(instance, args.file);
			if(!fd) { result = emabi::ebadf; }
			else
			{
				U8* buffer
					= memoryArrayPtr<U8>(instance->memory, args.bufferAddress, args.numBytes);

				// Ensure that it will be possible to return the number of bytes in a successful
				// write in the I32 return value.
				if(args.numBytes > INT32_MAX) { result = emabi::eoverflow; }
				else
				{
					// Do the write.
					U64 numBytesWritten = 0;
					VFS::Result vfsResult
						= fd->write(buffer, args.numBytes, &numBytesWritten, nullptr);
					if(vfsResult != VFS::Result::success) { result = asEmscriptenErrNo(vfsResult); }
					else
					{
						WAVM_ASSERT(numBytesWritten <= emabi::resultMax);
						result = I32(numBytesWritten);
					}
				}
			}
		},
		[&](Exception* exception) {
			// If we catch an out-of-bounds memory exception, return EFAULT.
			WAVM_ERROR_UNLESS(getExceptionType(exception)
							  == ExceptionTypes::outOfBoundsMemoryAccess);
			Log::printf(Log::debug,
						"Caught runtime exception while accessing memory at address 0x%" PRIx64,
						getExceptionArgument(exception, 1).i64);
			destroyException(exception);
			result = emabi::efault;
		});

	return result;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "___syscall146",
							   emabi::Result,
							   emscripten_writev,
							   I32,
							   emabi::Address argsAddress)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);

	// Catch any out-of-bounds memory access exceptions that are thrown.
	VFS::IOWriteBuffer* vfsWriteBuffers = nullptr;
	emabi::Result result = 0;
	Runtime::catchRuntimeExceptions(
		[&] {
			struct ArgStruct
			{
				emabi::FD file;
				emabi::Address iovsAddress;
				emabi::Address numIOVs;
			};
			ArgStruct args = memoryRef<ArgStruct>(instance->memory, argsAddress);

			VFS::VFD* fd = getFD(instance, args.file);
			if(!fd) { result = emabi::ebadf; }
			else
			{
				// Translate the IOVs to IOWriteBuffers
				vfsWriteBuffers
					= (VFS::IOWriteBuffer*)malloc(args.numIOVs * sizeof(VFS::IOWriteBuffer));
				const emabi::iovec* ioBuffers = memoryArrayPtr<emabi::iovec>(
					instance->memory, args.iovsAddress, args.numIOVs);
				U64 numBufferBytes = 0;
				for(U32 iovIndex = 0; iovIndex < args.numIOVs; ++iovIndex)
				{
					const emabi::iovec& ioBuffer = ioBuffers[iovIndex];
					vfsWriteBuffers[iovIndex].data = memoryArrayPtr<const U8>(
						instance->memory, ioBuffer.address, ioBuffer.numBytes);
					vfsWriteBuffers[iovIndex].numBytes = ioBuffer.numBytes;
					numBufferBytes += ioBuffer.numBytes;
				}

				// Ensure that it will be possible to return the number of bytes in a successful
				// write in the I32 return value.
				if(numBufferBytes > INT32_MAX) { result = emabi::eoverflow; }
				else
				{
					// Do the write.
					U64 numBytesWritten = 0;
					VFS::Result vfsResult
						= fd->writev(vfsWriteBuffers, args.numIOVs, &numBytesWritten, nullptr);
					if(vfsResult != VFS::Result::success) { result = asEmscriptenErrNo(vfsResult); }
					else
					{
						WAVM_ASSERT(numBytesWritten <= INT32_MAX);
						result = emabi::Result(numBytesWritten);
					}
				}
			}
		},
		[&](Exception* exception) {
			// If we catch an out-of-bounds memory exception, return EFAULT.
			WAVM_ERROR_UNLESS(getExceptionType(exception)
							  == ExceptionTypes::outOfBoundsMemoryAccess);
			Log::printf(Log::debug,
						"Caught runtime exception while accessing memory at address 0x%" PRIx64,
						getExceptionArgument(exception, 1).i64);
			destroyException(exception);
			result = emabi::efault;
		});

	// Free the VFS write buffers.
	free(vfsWriteBuffers);

	return result;
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

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_getenv", emabi::Result, _getenv, I32 a)
{
	return emabi::esuccess;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_gettimeofday",
							   emabi::Result,
							   _gettimeofday,
							   I32 timevalAddress,
							   I32)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);

	const Time realtimeClock = Platform::getClockTime(Platform::Clock::realtime);
	memoryRef<U32>(instance->memory, timevalAddress + 0) = U32(realtimeClock.ns / 1000000000);
	memoryRef<U32>(instance->memory, timevalAddress + 4)
		= U32(realtimeClock.ns / 1000 % 1000000000);
	return emabi::esuccess;
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_gmtime", emabi::Address, emscripten_gmtime, I32 timeAddress)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);

	emabi::time_t emscriptenTime = 0;
	Runtime::unwindSignalsAsExceptions(
		[&] { emscriptenTime = memoryRef<emabi::time_t>(instance->memory, timeAddress); });

	time_t hostTime = (time_t)emscriptenTime;
	struct tm* tmPtr = WAVM_SCOPED_DISABLE_SECURE_CRT_WARNINGS(gmtime(&hostTime));
	if(!tmPtr) { return 0; }
	else
	{
		struct tm hostTM = *tmPtr;

		emabi::Address emTMAddress = dynamicAlloc(instance, sizeof(emabi::tm));
		emabi::tm& emTM = memoryRef<emabi::tm>(instance->memory, emTMAddress);

		emTM.tm_sec = hostTM.tm_sec;
		emTM.tm_min = hostTM.tm_min;
		emTM.tm_hour = hostTM.tm_hour;
		emTM.tm_mday = hostTM.tm_mday;
		emTM.tm_mon = hostTM.tm_mon;
		emTM.tm_year = hostTM.tm_year;
		emTM.tm_wday = hostTM.tm_wday;
		emTM.tm_yday = hostTM.tm_yday;
		emTM.tm_isdst = hostTM.tm_isdst;
		emTM.__tm_gmtoff = 0;
		emTM.__tm_zone = 0;

		return emTMAddress;
	}
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_sem_init", emabi::Result, _sem_init, I32 a, I32 b, I32 c)
{
	return emabi::esuccess;
}

WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "invoke_ii", U32, invoke_ii, U32 index, U32 a);
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
											 "invoke_iii",
											 U32,
											 invoke_iii,
											 U32 index,
											 U32 a,
											 U32 b);
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "invoke_v", void, invoke_v, U32 index);
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
											 "invoke_vii",
											 void,
											 invoke_vii,
											 U32 index,
											 U32 a,
											 U32 b);
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
											 "___buildEnvironment",
											 void,
											 ___buildEnvironment,
											 U32);

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "___cxa_pure_virtual", void, ___cxa_pure_virtual)
{
	Errors::fatal("env.__cxa_pure_virtual called");
}

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "___syscall192",
							   emabi::Result,
							   _mmap2,
							   U32 which,
							   U32 varargsAddress)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);

	// const U32 address = memoryRef<U32>(instance->memory, varargsAddress + 0);
	const emabi::Size numBytes = memoryRef<emabi::Size>(instance->memory, varargsAddress + 4);
	// const U32 protection = memoryRef<U32>(instance->memory, varargsAddress + 8);
	// const U32 flags = memoryRef<U32>(instance->memory, varargsAddress + 12);
	const emabi::FD file = memoryRef<emabi::FD>(instance->memory, varargsAddress + 16);
	// const U32 offset = memoryRef<U32>(instance->memory, varargsAddress + 20);

	if(file != -1) { return emabi::enosys; }
	else
	{
		Function* memalignFunction
			= asFunctionNullable(getInstanceExport(instance->moduleInstance, "_memalign"));
		if(!memalignFunction
		   || getFunctionType(memalignFunction)
				  != FunctionType({ValueType::i32}, {ValueType::i32, ValueType::i32}))
		{ return emabi::enosys; }

		UntaggedValue memalignArgs[2] = {U32(16384), numBytes};
		UntaggedValue memalignResult;
		invokeFunction(getContextFromRuntimeData(contextRuntimeData),
					   memalignFunction,
					   FunctionType(ValueType::i32, {ValueType::i32, ValueType::i32}),
					   memalignArgs,
					   &memalignResult);
		if(!memalignResult.u32) { return emabi::enomem; }
		memset(memoryArrayPtr<char>(instance->memory, memalignResult.i32, numBytes), 0, numBytes);

		WAVM_ERROR_UNLESS(memalignResult.u32 < emabi::resultMax);
		return (emabi::Result)memalignResult.u32;
	}
}
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
											 "___syscall195",
											 emabi::Result,
											 emscripten_stat64,
											 U32,
											 U32);
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
											 "___syscall20",
											 emabi::Result,
											 emscripten_getpid,
											 U32,
											 U32);
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
											 "___syscall221",
											 emabi::Result,
											 emscriptenfcntl64,
											 U32,
											 U32);
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
											 "___syscall5",
											 emabi::Result,
											 emscripten_open,
											 U32,
											 U32);
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
											 "___syscall91",
											 emabi::Result,
											 emscripten_munmap,
											 U32,
											 U32);
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "_atexit", emabi::Result, _atexit, U32);

WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_clock_gettime",
							   emabi::Result,
							   _clock_gettime,
							   U32 clockId,
							   U32 timespecAddress)
{
	Emscripten::Instance* instance = getEmscriptenInstance(contextRuntimeData);

	Platform::Clock platformClock;
	switch(clockId)
	{
	case 0:
		// CLOCK_REALTIME
		platformClock = Platform::Clock::realtime;
		break;
	case 1:
		// CLOCK_MONOTONIC
		platformClock = Platform::Clock::monotonic;
		break;
	case 2:
		// CLOCK_PROCESS_CPUTIME_ID
		platformClock = Platform::Clock::processCPUTime;
		break;
	case 3:
		// CLOCK_THREAD_CPUTIME_ID
		platformClock = Platform::Clock::processCPUTime;
		break;
	default: return emabi::einval;
	}
	const Time clockTime = Platform::getClockTime(platformClock);

	memoryRef<U32>(instance->memory, timespecAddress + 0) = U32(clockTime.ns / 1000000000);
	memoryRef<U32>(instance->memory, timespecAddress + 4) = U32(clockTime.ns % 1000000000);

	return emabi::esuccess;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_getpagesize", U32, emscripten_getpagesize) { return 16384; }
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
											 "_llvm_log10_f64",
											 F64,
											 emscripten_llvm_log10_f64,
											 F64);
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
											 "_llvm_log2_f64",
											 F64,
											 emscripten_llvm_log2_f64,
											 F64);
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_llvm_trap", void, emscripten_llvm_trap)
{
	throwException(ExceptionTypes::calledAbort);
}
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
											 "_llvm_trunc_f64",
											 F64,
											 emscripten_llvm_trunc_f64,
											 F64);
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env,
											 "_localtime_r",
											 U32,
											 emscripten_localtime_r,
											 U32,
											 U32);
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "_longjmp", void, emscripten_longjmp, U32, U32);
WAVM_DEFINE_UNIMPLEMENTED_INTRINSIC_FUNCTION(env, "_mktime", U32, emscripten_mktime, U32);

WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_sched_yield", emabi::Result, emscripten_sched_yield)
{
	return emabi::esuccess;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_sem_destroy", emabi::Result, emscripten_sem_destroy, U32)
{
	return emabi::esuccess;
}
WAVM_DEFINE_INTRINSIC_FUNCTION(env,
							   "_strftime",
							   emabi::Result,
							   emscripten_strftime,
							   U32,
							   U32,
							   U32,
							   U32)
{
	return emabi::esuccess;
}
// WAVM_DEFINE_INTRINSIC_FUNCTION(env, "_tzset", void, _tzset) { }

Emscripten::Instance::~Instance()
{
	// Instead of allowing an Instance to live on until all its threads exit, wait for all threads
	// to exit before destroying the Instance.
	joinAllThreads(this);
}

std::shared_ptr<Emscripten::Instance> Emscripten::instantiate(Compartment* compartment,
															  const IR::Module& module,
															  VFS::VFD* stdIn,
															  VFS::VFD* stdOut,
															  VFS::VFD* stdErr)
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
			Log::printf(Log::error, "module's memory is too small for Emscripten emulation\n");
			return nullptr;
		}
	}
	else
	{
		Log::printf(Log::error, "module does not import Emscripten's env.memory\n");
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

	std::shared_ptr<Instance> instance = std::make_shared<Instance>();
	instance->env = Intrinsics::instantiateModule(
		compartment,
		{WAVM_INTRINSIC_MODULE_REF(env), WAVM_INTRINSIC_MODULE_REF(envThreads)},
		"env",
		extraEnvExports);
	instance->asm2wasm = Intrinsics::instantiateModule(
		compartment, {WAVM_INTRINSIC_MODULE_REF(asm2wasm)}, "asm2wasm");
	instance->global
		= Intrinsics::instantiateModule(compartment, {WAVM_INTRINSIC_MODULE_REF(global)}, "global");

	unwindSignalsAsExceptions([=] {
		MutableGlobals& mutableGlobals = memoryRef<MutableGlobals>(memory, MutableGlobals::address);

		mutableGlobals.DYNAMICTOP_PTR = minStaticMemoryPages * IR::numBytesPerPage;
		mutableGlobals._stderr = (U32)ioStreamVMHandle::StdErr;
		mutableGlobals._stdin = (U32)ioStreamVMHandle::StdIn;
		mutableGlobals._stdout = (U32)ioStreamVMHandle::StdOut;
	});

	instance->compartment = compartment;
	instance->memory = memory;
	instance->table = table;

	instance->stdIn = stdIn;
	instance->stdOut = stdOut;
	instance->stdErr = stdErr;

	setUserData(compartment, instance.get());

	return instance;
}

void Emscripten::initializeGlobals(const std::shared_ptr<Instance>& instance,
								   Context* context,
								   const IR::Module& module,
								   ModuleInstance* moduleInstance)
{
	instance->moduleInstance = moduleInstance;

	// Create an Emscripten "main thread" and associate it with this context.
	instance->mainThread = new Emscripten::Thread(instance.get(), context, nullptr, 0);
	setUserData(context, instance->mainThread);

	instance->mainThread->stackAddress = mainThreadStackAddress;
	instance->mainThread->numStackBytes = mainThreadNumStackBytes;

	// Initialize the Emscripten "thread local" state.
	initThreadLocals(instance->mainThread);

	// Call the global initializer functions: newer Emscripten uses a single globalCtors function,
	// and older Emscripten uses a __GLOBAL__* function for each translation unit.
	if(Function* globalCtors = asFunctionNullable(getInstanceExport(moduleInstance, "globalCtors")))
	{ invokeFunction(context, globalCtors); }

	for(Uptr exportIndex = 0; exportIndex < module.exports.size(); ++exportIndex)
	{
		const Export& functionExport = module.exports[exportIndex];
		if(functionExport.kind == IR::ExternKind::function
		   && !strncmp(functionExport.name.c_str(), "__GLOBAL__", 10))
		{
			Function* function
				= asFunctionNullable(getInstanceExport(moduleInstance, functionExport.name));
			if(function) { invokeFunction(context, function); }
		}
	}

	// Store ___errno_location.
	Function* errNoLocation
		= asFunctionNullable(getInstanceExport(moduleInstance, "___errno_location"));
	if(errNoLocation && getFunctionType(errNoLocation) == FunctionType({ValueType::i32}, {}))
	{
		IR::UntaggedValue errNoResult;
		invokeFunction(
			context, errNoLocation, FunctionType({ValueType::i32}, {}), nullptr, &errNoResult);
		instance->errnoAddress = errNoResult.i32;
	}
}

std::vector<IR::Value> Emscripten::injectCommandArgs(const std::shared_ptr<Instance>& instance,
													 const std::vector<std::string>& argStrings)
{
	U8* memoryBase = getMemoryBaseAddress(instance->memory);

	U32* argvOffsets
		= (U32*)(memoryBase
				 + dynamicAlloc(instance.get(), (U32)(sizeof(U32) * (argStrings.size() + 1))));
	for(Uptr argIndex = 0; argIndex < argStrings.size(); ++argIndex)
	{
		auto stringSize = argStrings[argIndex].size() + 1;
		auto stringMemory = memoryBase + dynamicAlloc(instance.get(), (U32)stringSize);
		memcpy(stringMemory, argStrings[argIndex].c_str(), stringSize);
		argvOffsets[argIndex] = (U32)(stringMemory - memoryBase);
	}
	argvOffsets[argStrings.size()] = 0;
	return {(U32)argStrings.size(), (U32)((U8*)argvOffsets - memoryBase)};
}

Runtime::Resolver& Emscripten::getInstanceResolver(const std::shared_ptr<Instance>& instance)
{
	return *instance.get();
}

bool Emscripten::Instance::resolve(const std::string& moduleName,
								   const std::string& exportName,
								   IR::ExternType type,
								   Runtime::Object*& outObject)
{
	ModuleInstance* intrinsicModuleInstance
		= moduleName == "env"
			  ? env
			  : moduleName == "asm2wasm" ? asm2wasm : moduleName == "global" ? global : nullptr;
	if(intrinsicModuleInstance)
	{
		outObject = getInstanceExport(intrinsicModuleInstance, exportName);
		if(outObject)
		{
			if(isA(outObject, type)) { return true; }
			else
			{
				Log::printf(Log::debug,
							"Resolved import %s.%s to a %s, but was expecting %s\n",
							moduleName.c_str(),
							exportName.c_str(),
							asString(getExternType(outObject)).c_str(),
							asString(type).c_str());
			}
		}
	}

	return false;
}

I32 Emscripten::catchExit(std::function<I32()>&& thunk)
{
	try
	{
		return std::move(thunk)();
	}
	catch(ExitException const& exitException)
	{
		return I32(exitException.exitCode);
	}
}

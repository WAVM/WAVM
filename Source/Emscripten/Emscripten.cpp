#include "Inline/BasicTypes.h"
#include "Inline/HashMap.h"
#include "Logging/Logging.h"
#include "IR/IR.h"
#include "IR/Module.h"
#include "Runtime/Runtime.h"
#include "Runtime/Intrinsics.h"
#include "Emscripten.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <sys/uio.h>
#endif

namespace Emscripten
{
	using namespace IR;
	using namespace Runtime;

	DEFINE_INTRINSIC_MODULE(env)
	DEFINE_INTRINSIC_MODULE(asm2wasm)
	DEFINE_INTRINSIC_MODULE(global)

	static U32 coerce32bitAddress(Uptr address)
	{
		if(address >= UINT32_MAX) { throwException(Exception::accessViolationType); }
		return (U32)address;
	}

	struct MutableGlobals
	{
		enum { address = 63 * IR::numBytesPerPage };

		U32 DYNAMICTOP_PTR;
		F64 tempDoublePtr;
		I32 _stderr;
		I32 _stdin;
		I32 _stdout;
	};

	DEFINE_INTRINSIC_GLOBAL(env,"STACKTOP",I32,STACKTOP          ,64 * IR::numBytesPerPage);
	DEFINE_INTRINSIC_GLOBAL(env,"STACK_MAX",I32,STACK_MAX        ,128 * IR::numBytesPerPage);
	DEFINE_INTRINSIC_GLOBAL(env,"tempDoublePtr",I32,tempDoublePtr,MutableGlobals::address + offsetof(MutableGlobals,tempDoublePtr));
	DEFINE_INTRINSIC_GLOBAL(env,"ABORT",I32,ABORT                ,0);
	DEFINE_INTRINSIC_GLOBAL(env,"cttz_i8",I32,cttz_i8            ,0);
	DEFINE_INTRINSIC_GLOBAL(env,"___dso_handle",I32,___dso_handle,0);
	DEFINE_INTRINSIC_GLOBAL(env,"_stderr",I32,_stderr            ,MutableGlobals::address + offsetof(MutableGlobals,_stderr));
	DEFINE_INTRINSIC_GLOBAL(env,"_stdin",I32,_stdin              ,MutableGlobals::address + offsetof(MutableGlobals,_stdin));
	DEFINE_INTRINSIC_GLOBAL(env,"_stdout",I32,_stdout            ,MutableGlobals::address + offsetof(MutableGlobals,_stdout));

	DEFINE_INTRINSIC_GLOBAL(env,"memoryBase",I32,emscriptenMemoryBase,1024);
	DEFINE_INTRINSIC_GLOBAL(env,"tableBase",I32,emscriptenTableBase,0);

	DEFINE_INTRINSIC_GLOBAL(env,"DYNAMICTOP_PTR",I32,DYNAMICTOP_PTR,MutableGlobals::address + offsetof(MutableGlobals,DYNAMICTOP_PTR))
	DEFINE_INTRINSIC_GLOBAL(env,"_environ",I32,em_environ,0)
	DEFINE_INTRINSIC_GLOBAL(env,"EMTSTACKTOP",I32,EMTSTACKTOP,0)
	DEFINE_INTRINSIC_GLOBAL(env,"EMT_STACK_MAX",I32,EMT_STACK_MAX,0)
	DEFINE_INTRINSIC_GLOBAL(env,"eb",I32,eb,0)

	static U32 dynamicAlloc(MemoryInstance* memory, U32 numBytes)
	{
		MutableGlobals& mutableGlobals = memoryRef<MutableGlobals>(memory, MutableGlobals::address);
		
		const U32 allocationAddress = mutableGlobals.DYNAMICTOP_PTR;
		const U32 endAddress = (allocationAddress + numBytes + 15) & -16;

		mutableGlobals.DYNAMICTOP_PTR = endAddress;

		const Uptr TOTAL_MEMORY = Runtime::getMemoryNumPages(memory) << IR::numBytesPerPageLog2;
		errorUnless(endAddress <= TOTAL_MEMORY);

		return allocationAddress;
	}

	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(env,"getTotalMemory",I32,getTotalMemory)
	{
		MemoryInstance* memory = Runtime::getMemoryFromRuntimeData(contextRuntimeData, defaultMemoryId.id);
		return coerce32bitAddress(Runtime::getMemoryMaxPages(memory) << IR::numBytesPerPageLog2);
	}
	
	DEFINE_INTRINSIC_FUNCTION(env,"abortOnCannotGrowMemory",I32,abortOnCannotGrowMemory)
	{
		throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
	}
	DEFINE_INTRINSIC_FUNCTION(env,"enlargeMemory",I32,enlargeMemory)
	{
		return abortOnCannotGrowMemory(contextRuntimeData);
	}

	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(env,"_time",I32,_time,I32 address)
	{
		MemoryInstance* memory = Runtime::getMemoryFromRuntimeData(contextRuntimeData, defaultMemoryId.id);
		time_t t = time(nullptr);
		if(address)
		{
			memoryRef<I32>(memory,address) = (I32)t;
		}
		return (I32)t;
	}

	DEFINE_INTRINSIC_FUNCTION(env,"___errno_location",I32,___errno_location)
	{
		return 0;
	}

	DEFINE_INTRINSIC_FUNCTION(env,"_sysconf",I32,_sysconf,I32 a)
	{
		enum { sysConfPageSize = 30 };
		switch(a)
		{
		case sysConfPageSize: return IR::numBytesPerPage;
		default: throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
		}
	}

	DEFINE_INTRINSIC_FUNCTION(env,"_pthread_cond_wait",I32,_pthread_cond_wait,I32 a,I32 b) { return 0; }
	DEFINE_INTRINSIC_FUNCTION(env,"_pthread_cond_broadcast",I32,_pthread_cond_broadcast,I32 a) { return 0; }
	DEFINE_INTRINSIC_FUNCTION(env,"_pthread_key_create",I32,_pthread_key_create,I32 a,I32 b) { throwException(Runtime::Exception::calledUnimplementedIntrinsicType); }
	DEFINE_INTRINSIC_FUNCTION(env,"_pthread_mutex_lock",I32,_pthread_mutex_lock,I32 a) { return 0; }
	DEFINE_INTRINSIC_FUNCTION(env,"_pthread_mutex_unlock",I32,_pthread_mutex_unlock,I32 a) { return 0; }
	DEFINE_INTRINSIC_FUNCTION(env,"_pthread_setspecific",I32,_pthread_setspecific,I32 a,I32 b) { throwException(Runtime::Exception::calledUnimplementedIntrinsicType); }
	DEFINE_INTRINSIC_FUNCTION(env,"_pthread_getspecific",I32,_pthread_getspecific,I32 a) { throwException(Runtime::Exception::calledUnimplementedIntrinsicType); }
	DEFINE_INTRINSIC_FUNCTION(env,"_pthread_once",I32,_pthread_once,I32 a,I32 b) { throwException(Runtime::Exception::calledUnimplementedIntrinsicType); }
	DEFINE_INTRINSIC_FUNCTION(env,"_pthread_cleanup_push",void,_pthread_cleanup_push,I32 a,I32 b) { }
	DEFINE_INTRINSIC_FUNCTION(env,"_pthread_cleanup_pop",void,_pthread_cleanup_pop,I32 a) { }
	DEFINE_INTRINSIC_FUNCTION(env,"_pthread_self",I32,_pthread_self) { return 0; }

	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(env,"___ctype_b_loc",I32,___ctype_b_loc)
	{
		MemoryInstance* memory = Runtime::getMemoryFromRuntimeData(contextRuntimeData, defaultMemoryId.id);
		unsigned short data[384] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,2,8195,8194,8194,8194,8194,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,24577,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,55304,55304,55304,55304,55304,55304,55304,55304,55304,55304,49156,49156,49156,49156,49156,49156,49156,54536,54536,54536,54536,54536,54536,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,49156,49156,49156,49156,49156,49156,54792,54792,54792,54792,54792,54792,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,49156,49156,49156,49156,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		static U32 vmAddress = 0;
		if(vmAddress == 0)
		{
			vmAddress = coerce32bitAddress(dynamicAlloc(memory,sizeof(data)));
			memcpy(memoryArrayPtr<U8>(memory,vmAddress,sizeof(data)),data,sizeof(data));
		}
		return vmAddress + sizeof(short)*128;
	}
	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(env,"___ctype_toupper_loc",I32,___ctype_toupper_loc)
	{
		MemoryInstance* memory = Runtime::getMemoryFromRuntimeData(contextRuntimeData, defaultMemoryId.id);
		I32 data[384] = {128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255};
		static U32 vmAddress = 0;
		if(vmAddress == 0)
		{
			vmAddress = coerce32bitAddress(dynamicAlloc(memory,sizeof(data)));
			memcpy(memoryArrayPtr<U8>(memory,vmAddress,sizeof(data)),data,sizeof(data));
		}
		return vmAddress + sizeof(I32)*128;
	}
	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(env,"___ctype_tolower_loc",I32,___ctype_tolower_loc)
	{
		MemoryInstance* memory = Runtime::getMemoryFromRuntimeData(contextRuntimeData, defaultMemoryId.id);
		I32 data[384] = {128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255};
		static U32 vmAddress = 0;
		if(vmAddress == 0)
		{
			vmAddress = coerce32bitAddress(dynamicAlloc(memory,sizeof(data)));
			memcpy(memoryArrayPtr<U8>(memory,vmAddress,sizeof(data)),data,sizeof(data));
		}
		return vmAddress + sizeof(I32)*128;
	}
	DEFINE_INTRINSIC_FUNCTION(env,"___assert_fail",void,___assert_fail,I32 condition,I32 filename,I32 line,I32 function)
	{
		throwException(Runtime::Exception::calledAbortType);
	}

	DEFINE_INTRINSIC_FUNCTION(env,"___cxa_atexit",I32,___cxa_atexit,I32 a,I32 b,I32 c)
	{
		return 0;
	}
	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(env,"___cxa_guard_acquire",I32,___cxa_guard_acquire,I32 address)
	{
		MemoryInstance* memory = Runtime::getMemoryFromRuntimeData(contextRuntimeData, defaultMemoryId.id);
		if(!memoryRef<U8>(memory,address))
		{
			memoryRef<U8>(memory,address) = 1;
			return 1;
		}
		else
		{
			return 0;
		}
	}
	DEFINE_INTRINSIC_FUNCTION(env,"___cxa_guard_release",void,___cxa_guard_release,I32 a)
	{}
	DEFINE_INTRINSIC_FUNCTION(env,"___cxa_throw",void,___cxa_throw,I32 a,I32 b,I32 c)
	{
		throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
	}
	DEFINE_INTRINSIC_FUNCTION(env,"___cxa_begin_catch",I32,___cxa_begin_catch,I32 a)
	{
		throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
	}
	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(env,"___cxa_allocate_exception",I32,___cxa_allocate_exception,I32 size)
	{
		MemoryInstance* memory = Runtime::getMemoryFromRuntimeData(contextRuntimeData, defaultMemoryId.id);
		return coerce32bitAddress(dynamicAlloc(memory,size));
	}
	DEFINE_INTRINSIC_FUNCTION(env,"__ZSt18uncaught_exceptionv",I32,__ZSt18uncaught_exceptionv)
	{
		throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
	}
	DEFINE_INTRINSIC_FUNCTION(env,"_abort",void,_abort)
	{
		throwException(Runtime::Exception::calledAbortType);
	}
	DEFINE_INTRINSIC_FUNCTION(env,"_exit",void,_exit,I32 code)
	{
		throwException(Runtime::Exception::calledAbortType);
	}
	DEFINE_INTRINSIC_FUNCTION(env,"abort",void,abort,I32 code)
	{
		Log::printf(Log::Category::error,"env.abort(%i)\n",code);
		throwException(Runtime::Exception::calledAbortType);
	}

	static U32 currentLocale = 0;
	DEFINE_INTRINSIC_FUNCTION(env,"_uselocale",I32,_uselocale,I32 locale)
	{
		auto oldLocale = currentLocale;
		currentLocale = locale;
		return oldLocale;
	}
	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(env,"_newlocale",I32,_newlocale,I32 mask,I32 locale,I32 base)
	{
		MemoryInstance* memory = Runtime::getMemoryFromRuntimeData(contextRuntimeData, defaultMemoryId.id);
		if(!base)
		{
			base = coerce32bitAddress(dynamicAlloc(memory,4));
		}
		return base;
	}
	DEFINE_INTRINSIC_FUNCTION(env,"_freelocale",void,_freelocale,I32 a)
	{}

	DEFINE_INTRINSIC_FUNCTION(env,"_strftime_l",I32,_strftime_l,I32 a,I32 b,I32 c,I32 d,I32 e) { throwException(Runtime::Exception::calledUnimplementedIntrinsicType); }
	DEFINE_INTRINSIC_FUNCTION(env,"_strerror",I32,_strerror,I32 a) { throwException(Runtime::Exception::calledUnimplementedIntrinsicType); }

	DEFINE_INTRINSIC_FUNCTION(env,"_catopen",I32,_catopen,I32 a,I32 b) { return (U32)-1; }
	DEFINE_INTRINSIC_FUNCTION(env,"_catgets",I32,_catgets,I32 catd,I32 set_id,I32 msg_id,I32 s) { return s; }
	DEFINE_INTRINSIC_FUNCTION(env,"_catclose",I32,_catclose,I32 a) { return 0; }

	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(env,"_emscripten_memcpy_big",I32,_emscripten_memcpy_big,I32 a,I32 b,I32 c)
	{
		MemoryInstance* memory = Runtime::getMemoryFromRuntimeData(contextRuntimeData, defaultMemoryId.id);
		memcpy(memoryArrayPtr<U8>(memory,a,c),memoryArrayPtr<U8>(memory,b,c),U32(c));
		return a;
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
		default: return stdout;//std::cerr << "invalid file handle " << vmHandle << std::endl; throw;
		}
	}

	DEFINE_INTRINSIC_FUNCTION(env,"_vfprintf",I32,_vfprintf,I32 file,I32 formatPointer,I32 argList)
	{
		throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
	}
	DEFINE_INTRINSIC_FUNCTION(env,"_getc",I32,_getc,I32 file)
	{
		return getc(vmFile(file));
	}
	DEFINE_INTRINSIC_FUNCTION(env,"_ungetc",I32,_ungetc,I32 character,I32 file)
	{
		return ungetc(character,vmFile(file));
	}
	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(env,"_fread",I32,_fread,I32 pointer,I32 size,I32 count,I32 file)
	{
		MemoryInstance* memory = Runtime::getMemoryFromRuntimeData(contextRuntimeData, defaultMemoryId.id);
		return (I32)fread(memoryArrayPtr<U8>(memory,pointer,U64(size) * U64(count)),U64(size),U64(count),vmFile(file));
	}
	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(env,"_fwrite",I32,_fwrite,I32 pointer,I32 size,I32 count,I32 file)
	{
		MemoryInstance* memory = Runtime::getMemoryFromRuntimeData(contextRuntimeData, defaultMemoryId.id);
		return (I32)fwrite(memoryArrayPtr<U8>(memory,pointer,U64(size) * U64(count)),U64(size),U64(count),vmFile(file));
	}
	DEFINE_INTRINSIC_FUNCTION(env,"_fputc",I32,_fputc,I32 character,I32 file)
	{
		return fputc(character,vmFile(file));
	}
	DEFINE_INTRINSIC_FUNCTION(env,"_fflush",I32,_fflush,I32 file)
	{
		return fflush(vmFile(file));
	}

	DEFINE_INTRINSIC_FUNCTION(env,"___lock",void,___lock,I32 a)
	{
	}
	DEFINE_INTRINSIC_FUNCTION(env,"___unlock",void,___unlock,I32 a)
	{
	}
	DEFINE_INTRINSIC_FUNCTION(env,"___lockfile",I32,___lockfile,I32 a)
	{
		return 1;
	}
	DEFINE_INTRINSIC_FUNCTION(env,"___unlockfile",void,___unlockfile,I32 a)
	{
	}

	DEFINE_INTRINSIC_FUNCTION(env,"___syscall6",I32,___syscall6,I32 a,I32 b)
	{
		// close
		throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
	}

	DEFINE_INTRINSIC_FUNCTION(env,"___syscall54",I32,___syscall54,I32 a,I32 b)
	{
		// ioctl
		return 0;
	}

	DEFINE_INTRINSIC_FUNCTION(env,"___syscall140",I32,___syscall140,I32 a,I32 b)
	{
		// llseek
		throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
	}

	DEFINE_INTRINSIC_FUNCTION(env,"___syscall145",I32,___syscall145,I32 file,I32 argsPtr)
	{
		// readv
		throwException(Runtime::Exception::calledUnimplementedIntrinsicType);
	}

	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(env,"___syscall146",I32,___syscall146,I32 file,I32 argsPtr)
	{
		MemoryInstance* memory = Runtime::getMemoryFromRuntimeData(contextRuntimeData, defaultMemoryId.id);

		// writev
		U32* args = memoryArrayPtr<U32>(memory,argsPtr,3);
		U32 iov = args[1];
		U32 iovcnt = args[2];
#ifdef _WIN32
		U32 count = 0;
		for(U32 i = 0; i < iovcnt; i++)
		{
			U32 base = memoryRef<U32>(memory,iov + i * 8);
			U32 len = memoryRef<U32>(memory,iov + i * 8 + 4);
			U32 size = (U32)fwrite(memoryArrayPtr<U8>(memory,base,len), 1, len, vmFile(file));
			count += size;
			if (size < len)
				break;
		}
#else
		struct iovec *native_iovec = new(alloca(sizeof(iovec)*iovcnt)) struct iovec [iovcnt];
		for(U32 i = 0; i < iovcnt; i++)
		{
			U32 base = memoryRef<U32>(memory,iov + i * 8);
			U32 len = memoryRef<U32>(memory,iov + i * 8 + 4);

			native_iovec[i].iov_base = memoryArrayPtr<U8>(memory,base,len);
			native_iovec[i].iov_len = len;
		}
		Iptr count = writev(fileno(vmFile(file)), native_iovec, iovcnt);
#endif
		return count;
	}

	DEFINE_INTRINSIC_FUNCTION(asm2wasm,"f64-to-int",I32,f64_to_int,F64 f) { return (I32)f; }

	static F64 zero = 0.0;

	static F64 makeNaN() { return zero / zero; }
	static F64 makeInf() { return 1.0/zero; }

	DEFINE_INTRINSIC_GLOBAL(global,"NaN",F64,NaN,makeNaN())
	DEFINE_INTRINSIC_GLOBAL(global,"Infinity",F64,Infinity,makeInf())

	DEFINE_INTRINSIC_FUNCTION(asm2wasm,"i32u-rem",I32,I32_remu,I32 left,I32 right)
	{
		return (I32)((U32)left % (U32)right);
	}
	DEFINE_INTRINSIC_FUNCTION(asm2wasm,"i32s-rem",I32,I32_rems,I32 left,I32 right)
	{
		return left % right;
	}
	DEFINE_INTRINSIC_FUNCTION(asm2wasm,"i32u-div",I32,I32_divu,I32 left,I32 right)
	{
		return (I32)((U32)left / (U32)right);
	}
	DEFINE_INTRINSIC_FUNCTION(asm2wasm,"i32s-div",I32,I32_divs,I32 left,I32 right)
	{
		return left / right;
	}

	EMSCRIPTEN_API Instance* instantiate(Compartment* compartment, const Module& module)
	{
		MemoryType memoryType(false, SizeConstraints {0,0});
		if(module.memories.imports.size()
			&& module.memories.imports[0].moduleName == "env"
			&& module.memories.imports[0].exportName == "memory")
		{
			memoryType = module.memories.imports[0].type;
		}
		else { return nullptr; }

		TableType tableType(TableElementType::anyfunc, false, SizeConstraints {0,0});
		if(module.tables.imports.size()
			&& module.tables.imports[0].moduleName == "env"
			&& module.tables.imports[0].exportName == "table")
		{
			tableType = module.tables.imports[0].type;
		}

		MemoryInstance* memory = Runtime::createMemory(compartment,memoryType);
		TableInstance* table = Runtime::createTable(compartment,tableType);

		HashMap<std::string, Runtime::Object*> extraEnvExports =
		{
			{ "memory", Runtime::asObject(memory) },
			{ "table", Runtime::asObject(table) },
		};

		Instance* instance = new Instance;
		instance->env = Intrinsics::instantiateModule(compartment, INTRINSIC_MODULE_REF(env), "env", extraEnvExports);
		instance->asm2wasm = Intrinsics::instantiateModule(compartment,INTRINSIC_MODULE_REF(asm2wasm), "asm2wasm");
		instance->global = Intrinsics::instantiateModule(compartment,INTRINSIC_MODULE_REF(global), "global");

		MutableGlobals& mutableGlobals = memoryRef<MutableGlobals>(memory, MutableGlobals::address);

		mutableGlobals.DYNAMICTOP_PTR = STACK_MAX.getValue().i32;
		mutableGlobals._stderr = (U32)ioStreamVMHandle::StdErr;
		mutableGlobals._stdin = (U32)ioStreamVMHandle::StdIn;
		mutableGlobals._stdout = (U32)ioStreamVMHandle::StdOut;

		instance->emscriptenMemory = memory;

		return instance;
	}

	EMSCRIPTEN_API void initializeGlobals(Context* context,const Module& module,ModuleInstance* moduleInstance)
	{
		// Call the establishStackSpace function to set the Emscripten module's internal stack pointers.
		FunctionInstance* establishStackSpace = asFunctionNullable(getInstanceExport(moduleInstance,"establishStackSpace"));
		if(establishStackSpace
		&& getFunctionType(establishStackSpace) == FunctionType::get(ResultType::none,{ValueType::i32,ValueType::i64}))
		{
			std::vector<Runtime::Value> parameters =
			{
				Runtime::Value(STACKTOP.getValue().i32),
				Runtime::Value(STACK_MAX.getValue().i32)
			};
			Runtime::invokeFunctionChecked(context,establishStackSpace,parameters);
		}

		// Call the global initializer functions.
		for(Uptr exportIndex = 0;exportIndex < module.exports.size();++exportIndex)
		{
			const Export& functionExport = module.exports[exportIndex];
			if(functionExport.kind == IR::ObjectKind::function && !strncmp(functionExport.name.c_str(),"__GLOBAL__",10))
			{
				FunctionInstance* functionInstance = asFunctionNullable(getInstanceExport(moduleInstance,functionExport.name));
				if(functionInstance) { Runtime::invokeFunctionChecked(context,functionInstance,{}); }
			}
		}
	}

	EMSCRIPTEN_API void injectCommandArgs(Emscripten::Instance* instance,const std::vector<const char*>& argStrings,std::vector<Runtime::Value>& outInvokeArgs)
	{
		MemoryInstance* memory = instance->emscriptenMemory;
		U8* emscriptenMemoryBaseAdress = getMemoryBaseAddress(memory);

		U32* argvOffsets = (U32*)(emscriptenMemoryBaseAdress + dynamicAlloc(memory, (U32)(sizeof(U32) * (argStrings.size() + 1))));
		for(Uptr argIndex = 0;argIndex < argStrings.size();++argIndex)
		{
			auto stringSize = strlen(argStrings[argIndex])+1;
			auto stringMemory = emscriptenMemoryBaseAdress + dynamicAlloc(memory, (U32)stringSize);
			memcpy(stringMemory,argStrings[argIndex],stringSize);
			argvOffsets[argIndex] = (U32)(stringMemory - emscriptenMemoryBaseAdress);
		}
		argvOffsets[argStrings.size()] = 0;
		outInvokeArgs = {(U32)argStrings.size(), (U32)((U8*)argvOffsets - emscriptenMemoryBaseAdress) };
	}
}

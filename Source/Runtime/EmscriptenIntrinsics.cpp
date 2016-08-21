#include "Core/Core.h"
#include "Intrinsics.h"
#include "RuntimePrivate.h"
#include "Core/Platform.h"
#include <time.h>
#include <stdio.h>
#include <iostream>
#include <limits.h>

#ifndef _WIN32
#include <sys/uio.h>
#endif

namespace Runtime
{
	static uint32 coerce32bitAddress(uintptr_t address)
	{
		if(address >= UINT_MAX)
		{
			throw;
		}
		return (uint32)address;
	}

	DEFINE_INTRINSIC_VALUE(env,STACKTOP,STACKTOP,I32,=0);
	DEFINE_INTRINSIC_VALUE(env,STACK_MAX,STACK_MAX,I32,=0);
	DEFINE_INTRINSIC_VALUE(env,tempDoublePtr,tempDoublePtr,I32,=0);
	DEFINE_INTRINSIC_VALUE(env,ABORT,ABORT,I32,=0);
	DEFINE_INTRINSIC_VALUE(env,cttz_i8,cttz_i8,I32,=0);
	DEFINE_INTRINSIC_VALUE(env,___dso_handle,___dso_handle,I32,=0);
	DEFINE_INTRINSIC_VALUE(env,_stderr,_stderr,I32,);
	DEFINE_INTRINSIC_VALUE(env,_stdin,_stdin,I32,);
	DEFINE_INTRINSIC_VALUE(env,_stdout,_stdout,I32,);

	DEFINE_INTRINSIC_FUNCTION1(env,_sbrk,_sbrk,I32,I32,numBytes)
	{
		int32 signedNumBytes = (int32)numBytes;
		auto result = signedNumBytes < 0
			? vmShrinkMemory((size_t)-(int64)signedNumBytes)
			: vmGrowMemory((size_t)numBytes);
		return coerce32bitAddress(result);
	}

	DEFINE_INTRINSIC_FUNCTION1(env,_time,_time,I32,I32,address)
	{
		time_t t = time(nullptr);
		if(address)
		{
			instanceMemoryRef<int32>(address) = (int32)t;
		}
		return (int32)t;
	}

	DEFINE_INTRINSIC_FUNCTION0(env,___errno_location,___errno_location,I32)
	{
		return 0;
	}

	DEFINE_INTRINSIC_FUNCTION1(env,_sysconf,_sysconf,I32,I32,a)
	{
		enum { sysConfPageSize = 30 };
		switch(a)
		{
		case sysConfPageSize: return 1 << Platform::getPageSizeLog2();
		default: throw;
		}
	}

	DEFINE_INTRINSIC_FUNCTION2(env,_pthread_cond_wait,_pthread_cond_wait,I32,I32,a,I32,b) { return 0; }
	DEFINE_INTRINSIC_FUNCTION1(env,_pthread_cond_broadcast,_pthread_cond_broadcast,I32,I32,a) { return 0; }
	DEFINE_INTRINSIC_FUNCTION2(env,_pthread_key_create,_pthread_key_create,I32,I32,a,I32,b) { throw "_pthread_key_create"; }
	DEFINE_INTRINSIC_FUNCTION1(env,_pthread_mutex_lock,_pthread_mutex_lock,I32,I32,a) { return 0; }
	DEFINE_INTRINSIC_FUNCTION1(env,_pthread_mutex_unlock,_pthread_mutex_unlock,I32,I32,a) { return 0; }
	DEFINE_INTRINSIC_FUNCTION2(env,_pthread_setspecific,_pthread_setspecific,I32,I32,a,I32,b) { throw "_pthread_setspecific"; }
	DEFINE_INTRINSIC_FUNCTION1(env,_pthread_getspecific,_pthread_getspecific,I32,I32,a) { throw "_pthread_getspecific"; }
	DEFINE_INTRINSIC_FUNCTION2(env,_pthread_once,_pthread_once,I32,I32,a,I32,b) { throw "_pthread_once"; }
	DEFINE_INTRINSIC_FUNCTION2(env,_pthread_cleanup_push,_pthread_cleanup_push,Void,I32,a,I32,b) { }
	DEFINE_INTRINSIC_FUNCTION1(env,_pthread_cleanup_pop,_pthread_cleanup_pop,Void,I32,a) { }
	DEFINE_INTRINSIC_FUNCTION0(env,_pthread_self,_pthread_self,I32) { return 0; }

	DEFINE_INTRINSIC_FUNCTION0(env,___ctype_b_loc,___ctype_b_loc,I32)
	{
		unsigned short data[384] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,2,8195,8194,8194,8194,8194,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,24577,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,55304,55304,55304,55304,55304,55304,55304,55304,55304,55304,49156,49156,49156,49156,49156,49156,49156,54536,54536,54536,54536,54536,54536,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,49156,49156,49156,49156,49156,49156,54792,54792,54792,54792,54792,54792,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,49156,49156,49156,49156,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		static uint32 vmAddress = 0;
		if(vmAddress == 0)
		{
			vmAddress = coerce32bitAddress(vmGrowMemory(sizeof(data)));
			memcpy(&instanceMemoryRef<short>(vmAddress),data,sizeof(data));
		}
		return vmAddress + sizeof(short)*128;
	}
	DEFINE_INTRINSIC_FUNCTION0(env,___ctype_toupper_loc,___ctype_toupper_loc,I32)
	{
		int32 data[384] = {128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255};
		static uint32 vmAddress = 0;
		if(vmAddress == 0)
		{
			vmAddress = coerce32bitAddress(vmGrowMemory(sizeof(data)));
			memcpy(&instanceMemoryRef<int32>(vmAddress),data,sizeof(data));
		}
		return vmAddress + sizeof(int32)*128;
	}
	DEFINE_INTRINSIC_FUNCTION0(env,___ctype_tolower_loc,___ctype_tolower_loc,I32)
	{
		int32 data[384] = {128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255};
		static uint32 vmAddress = 0;
		if(vmAddress == 0)
		{
			vmAddress = coerce32bitAddress(vmGrowMemory(sizeof(data)));
			memcpy(&instanceMemoryRef<int32>(vmAddress),data,sizeof(data));
		}
		return vmAddress + sizeof(int32)*128;
	}
	DEFINE_INTRINSIC_FUNCTION4(env,___assert_fail,___assert_fail,Void,I32,condition,I32,filename,I32,line,I32,function)
	{
		ABORT = 1;
		throw;
	}

	DEFINE_INTRINSIC_FUNCTION3(env,___cxa_atexit,___cxa_atexit,I32,I32,a,I32,b,I32,c)
	{
		return 0;
	}
	DEFINE_INTRINSIC_FUNCTION1(env,___cxa_guard_acquire,___cxa_guard_acquire,I32,I32,address)
	{
		if(!instanceMemoryRef<uint8>(address))
		{
			instanceMemoryRef<uint8>(address) = 1;
			return 1;
		}
		else
		{
			return 0;
		}
	}
	DEFINE_INTRINSIC_FUNCTION1(env,___cxa_guard_release,___cxa_guard_release,Void,I32,a)
	{}
	DEFINE_INTRINSIC_FUNCTION3(env,___cxa_throw,___cxa_throw,Void,I32,a,I32,b,I32,c)
	{
		throw "___cxa_throw";
	}
	DEFINE_INTRINSIC_FUNCTION1(env,___cxa_begin_catch,___cxa_begin_catch,I32,I32,a)
	{
		throw "___cxa_begin_catch";
	}
	DEFINE_INTRINSIC_FUNCTION1(env,___cxa_allocate_exception,___cxa_allocate_exception,I32,I32,size)
	{
		return coerce32bitAddress(vmGrowMemory(size));
	}
	DEFINE_INTRINSIC_FUNCTION0(env,__ZSt18uncaught_exceptionv,__ZSt18uncaught_exceptionv,I32)
	{
		throw;
	}
	DEFINE_INTRINSIC_FUNCTION0(env,_abort,_abort,Void)
	{
		throw "_abort";
	}
	DEFINE_INTRINSIC_FUNCTION1(env,abort,abort,Void,I32,code)
	{
		std::cerr << "abort(" << code << ")" << std::endl;
		throw "abort";
	}

	static uint32 currentLocale = 0;
	DEFINE_INTRINSIC_FUNCTION1(env,_uselocale,_uselocale,I32,I32,locale)
	{
		auto oldLocale = currentLocale;
		currentLocale = locale;
		return oldLocale;
	}
	DEFINE_INTRINSIC_FUNCTION3(env,_newlocale,_newlocale,I32,I32,mask,I32,locale,I32,base)
	{
		if(!base)
		{
			base = coerce32bitAddress(vmGrowMemory(4));
		}
		return base;
	}
	DEFINE_INTRINSIC_FUNCTION1(env,_freelocale,_freelocale,Void,I32,a)
	{}

	DEFINE_INTRINSIC_FUNCTION5(env,_strftime_l,_strftime_l,I32,I32,a,I32,b,I32,c,I32,d,I32,e) { throw "_strftime_l"; }
	DEFINE_INTRINSIC_FUNCTION1(env,_strerror,_strerror,I32,I32,a) { throw "_strerror"; }

	DEFINE_INTRINSIC_FUNCTION2(env,_catopen,_catopen,I32,I32,a,I32,b) { return (uint32)-1; }
	DEFINE_INTRINSIC_FUNCTION4(env,_catgets,_catgets,I32,I32,catd,I32,set_id,I32,msg_id,I32,s) { return s; }
	DEFINE_INTRINSIC_FUNCTION1(env,_catclose,_catclose,I32,I32,a) { return 0; }

	DEFINE_INTRINSIC_FUNCTION3(env,_emscripten_memcpy_big,_emscripten_memcpy_big,I32,I32,a,I32,b,I32,c)
	{
		if (uint64(a) + uint64(c) >= instanceAddressSpaceMaxBytes ||
		    uint64(b) + uint64(c) >= instanceAddressSpaceMaxBytes)
			throw "_emscripten_memcpy_big";
		memcpy(&instanceMemoryRef<uint32>(a),&instanceMemoryRef<uint32>(b),uint32(c));
		return a;
	}

	enum class ioStreamVMHandle
	{
		StdErr = 1,
		StdIn = 2,
		StdOut = 3
	};
	FILE* vmFile(uint32 vmHandle)
	{
		switch((ioStreamVMHandle)vmHandle)
		{
		case ioStreamVMHandle::StdErr: return stderr;
		case ioStreamVMHandle::StdIn: return stdin;
		case ioStreamVMHandle::StdOut: return stdout;
		default: return stdout;//std::cerr << "invalid file handle " << vmHandle << std::endl; throw;
		}
	}

	DEFINE_INTRINSIC_FUNCTION3(env,_vfprintf,_vfprintf,I32,I32,file,I32,formatPointer,I32,argList)
	{
		throw;
	}
	DEFINE_INTRINSIC_FUNCTION1(env,_getc,_getc,I32,I32,file)
	{
		return getc(vmFile(file));
	}
	DEFINE_INTRINSIC_FUNCTION2(env,_ungetc,_ungetc,I32,I32,character,I32,file)
	{
		return ungetc(character,vmFile(file));
	}
	DEFINE_INTRINSIC_FUNCTION4(env,_fread,_fread,I32,I32,pointer,I32,size,I32,count,I32,file)
	{
		if(pointer >= instanceAddressSpaceMaxBytes ||
		  (instanceAddressSpaceMaxBytes - pointer) < (uint64(size) * (uint64(count) + 1)))
		{
			throw;
		}
		else
		{
			return (int32)fread(&instanceMemoryRef<uint8>(pointer),size,count,vmFile(file));
		}
	}
	DEFINE_INTRINSIC_FUNCTION4(env,_fwrite,_fwrite,I32,I32,pointer,I32,size,I32,count,I32,file)
	{
		if(pointer >= instanceAddressSpaceMaxBytes ||
		  (instanceAddressSpaceMaxBytes - pointer) < (uint64(size) * (uint64(count) + 1)))
		{
			throw;
		}
		else
		{
			return (int32)fwrite(&instanceMemoryRef<uint8>(pointer),size,count,vmFile(file));
		}
	}
	DEFINE_INTRINSIC_FUNCTION2(env,_fputc,_fputc,I32,I32,character,I32,file)
	{
		return fputc(character,vmFile(file));
	}
	DEFINE_INTRINSIC_FUNCTION1(env,_fflush,_fflush,I32,I32,file)
	{
		return fflush(vmFile(file));
	}

	DEFINE_INTRINSIC_FUNCTION1(env,___lock,___lock,Void,I32,a)
	{
	}
	DEFINE_INTRINSIC_FUNCTION1(env,___unlock,___unlock,Void,I32,a)
	{
	}
	DEFINE_INTRINSIC_FUNCTION1(env,___lockfile,___lockfile,I32,I32,a)
	{
		return 1;
	}
	DEFINE_INTRINSIC_FUNCTION1(env,___unlockfile,___unlockfile,Void,I32,a)
	{
	}

	DEFINE_INTRINSIC_FUNCTION2(env,___syscall6,___syscall6,I32,I32,a,I32,b)
	{
		// close
		throw "___syscall6";
	}

	DEFINE_INTRINSIC_FUNCTION2(env,___syscall54,___syscall54,I32,I32,a,I32,b)
	{
		// ioctl
		return 0;
	}

	DEFINE_INTRINSIC_FUNCTION2(env,___syscall140,___syscall140,I32,I32,a,I32,b)
	{
		// llseek
		throw "___syscall140";
	}

	DEFINE_INTRINSIC_FUNCTION2(env,___syscall145,___syscall145,I32,I32,file,I32,argsPtr)
	{
		// readv
		throw "___syscall145";
	}

	DEFINE_INTRINSIC_FUNCTION2(env,___syscall146,___syscall146,I32,I32,file,I32,argsPtr)
	{
		// writev
		uint32 *args = &instanceMemoryRef<uint32>(argsPtr);
		uint32 iov = args[1];
		uint32 iovcnt = args[2];
#ifdef _WIN32
		uint32 count = 0;
		for(size_t i = 0; i < iovcnt; i++)
		{
			if(iov + (i+1) * 8 > instanceAddressSpaceMaxBytes) { throw; }
			uint32 base = instanceMemoryRef<uint32>(iov + i * 8);
			uint32 len = instanceMemoryRef<uint32>(iov + i * 8 + 4);
			if(base + len + 1 > instanceAddressSpaceMaxBytes) { throw; }

			uint32 size = (uint32)fwrite(&instanceMemoryRef<char>(base), 1, len, vmFile(file));
			count += size;
			if (size < len)
				break;
		}
#else
		struct iovec *native_iovec = new struct iovec [iovcnt];
		for(size_t i = 0; i < iovcnt; i++)
		{
			if(iov + (i+1) * 8 > instanceAddressSpaceMaxBytes) { throw; }
			uint32 base = instanceMemoryRef<uint32>(iov + i * 8);
			uint32 len = instanceMemoryRef<uint32>(iov + i * 8 + 4);
			if(base + len + 1 > instanceAddressSpaceMaxBytes) { throw; }

			native_iovec[i].iov_base = &instanceMemoryRef<char>(base);
			native_iovec[i].iov_len = len;
		}
		ssize_t count = writev(fileno(vmFile(file)), native_iovec, iovcnt);
		delete[] native_iovec;
#endif
		return count;
	}

	DEFINE_INTRINSIC_FUNCTION1(asm2wasm,f64_to_int,f64-to-int,I32,F64,f) { return (int32)f; }

	bool initEmscriptenIntrinsics(const AST::Module* module)
	{
		// Allocate a 5MB stack.
		STACKTOP = coerce32bitAddress(vmGrowMemory(5*1024*1024));
		STACK_MAX = coerce32bitAddress(vmGrowMemory(0));

		// Allocate some 8 byte memory region for tempDoublePtr.
		tempDoublePtr = coerce32bitAddress(vmGrowMemory(8));

		// Setup IO stream handles.
		_stderr = coerce32bitAddress(vmGrowMemory(sizeof(uint32)));
		_stdin = coerce32bitAddress(vmGrowMemory(sizeof(uint32)));
		_stdout = coerce32bitAddress(vmGrowMemory(sizeof(uint32)));
		instanceMemoryRef<uint32>(_stderr) = (uint32)ioStreamVMHandle::StdErr;
		instanceMemoryRef<uint32>(_stdin) = (uint32)ioStreamVMHandle::StdIn;
		instanceMemoryRef<uint32>(_stdout) = (uint32)ioStreamVMHandle::StdOut;
		
		// Call the global initializer functions.
		for(auto exportIt : module->exportNameToFunctionIndexMap)
		{
			if(!strncmp(exportIt.first,"__GLOBAL__",10)
				&& !module->functions[exportIt.second].type.parameters.size()
				&& module->functions[exportIt.second].type.returnType == AST::TypeId::Void)
			{
				auto result = Runtime::invokeFunction(module,exportIt.second,nullptr);
				if(result.type == Runtime::TypeId::Exception)
				{
					std::cerr << exportIt.first << " threw exception: " << Runtime::describeExceptionCause(result.exception->cause) << std::endl;
					for(auto function : result.exception->callStack) { std::cerr << "  " << function << std::endl; }
					return false;
				}
			}
		}

		// Call the establishStackSpace function to set the Emscripten module's internal stack pointers.
		auto establishStackSpaceExport = module->exportNameToFunctionIndexMap.find("establishStackSpace");
		if(establishStackSpaceExport != module->exportNameToFunctionIndexMap.end())
		{
			const AST::Function& establishStackSpaceFunction = module->functions[establishStackSpaceExport->second];
			if(establishStackSpaceFunction.type.parameters.size() != 2
				|| establishStackSpaceFunction.type.parameters[0] != AST::TypeId::I32
				|| establishStackSpaceFunction.type.parameters[1] != AST::TypeId::I32
				|| establishStackSpaceFunction.type.returnType != AST::TypeId::Void)
			{
				std::cerr << "Module exports 'establishStackSpace' but it isn't the right type?!" << std::endl;
				return false;
			}
			else
			{
				Runtime::Value parameters[2] = {STACKTOP,STACK_MAX};
				auto establishStackSpaceResult = Runtime::invokeFunction(module,establishStackSpaceExport->second,parameters);
				if(establishStackSpaceResult.type == Runtime::TypeId::Exception)
				{
					std::cerr << "establishStackSpace threw exception: " << Runtime::describeExceptionCause(establishStackSpaceResult.exception->cause) << std::endl;
					for(auto function : establishStackSpaceResult.exception->callStack) { std::cerr << "  " << function << std::endl; }
					return false;
				}
			}
		}

		return true;
	}
}

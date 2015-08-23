#include "WAVM.h"
#include "Intrinsics.h"
#include <time.h>
#include <stdio.h>
#include <iostream>

namespace WAVM
{
	static size_t numCommittedVirtualPages = 0;
	static uint32_t numAllocatedBytes = 0;

	DEFINE_INTRINSIC_VALUE(STACKTOP,I32,=512*1024);
	DEFINE_INTRINSIC_VALUE(STACK_MAX,I32,=5*1024*1024);
	DEFINE_INTRINSIC_VALUE(tempDoublePtr,I32,=0);
	DEFINE_INTRINSIC_VALUE(ABORT,I32,=0);
	DEFINE_INTRINSIC_VALUE(cttz_i8,I32,=0);
	DEFINE_INTRINSIC_VALUE(___dso_handle,I32,=0);
	DEFINE_INTRINSIC_VALUE(_stderr,I32,);
	DEFINE_INTRINSIC_VALUE(_stdin,I32,);
	DEFINE_INTRINSIC_VALUE(_stdout,I32,);

	template<typename memoryType> memoryType& vmMemoryRef(uint32_t address)
	{
		return *(memoryType*)(vmVirtualAddressBase + address);
	}

	uint32_t vmSbrk(int32_t numBytes)
	{
		const uint32_t existingNumBytes = numAllocatedBytes;
		if(numBytes > 0)
		{
			if(uint64_t(existingNumBytes) + numBytes > (1ull<<32))
			{
				return -1;
			}

			const uint32_t pageSizeLog2 = getPreferredVirtualPageSizeLog2();
			const uint32_t pageSize = 1ull << pageSizeLog2;
			const uint32_t numDesiredPages = (numAllocatedBytes + numBytes + pageSize - 1) >> pageSizeLog2;
			const uint32_t numNewPages = numDesiredPages - numCommittedVirtualPages;
			if(numNewPages > 0)
			{
				bool successfullyCommittedPhysicalMemory = commitVirtualPages(vmVirtualAddressBase + (numCommittedVirtualPages << pageSizeLog2),numNewPages);
				if(!successfullyCommittedPhysicalMemory)
				{
					return -1;
				}
				numAllocatedBytes += numBytes;
				numCommittedVirtualPages += numNewPages;
			}
		}
		else if(numBytes < 0)
		{
			numAllocatedBytes -= numBytes;
		}
		return (int32_t)existingNumBytes;
	}

	DEFINE_INTRINSIC_FUNCTION1(_sbrk,I32,I32,numBytes)
	{
		return vmSbrk(numBytes);
	}

	DEFINE_INTRINSIC_FUNCTION1(_time,I32,I32,address)
	{
		time_t t = time(nullptr);
		if(address)
		{
			vmMemoryRef<int32_t>(address) = t;
		}
		return t;
	}

	DEFINE_INTRINSIC_FUNCTION0(___errno_location,I32)
	{
		return 0;
	}

	DEFINE_INTRINSIC_FUNCTION1(_sysconf,I32,I32,a)
	{
		enum { _SC_PAGE_SIZE = 30 };
		switch(a)
		{
		case _SC_PAGE_SIZE: return 1 << getPreferredVirtualPageSizeLog2();
		default: throw;
		}
	}

	DEFINE_INTRINSIC_FUNCTION2(_pthread_cond_wait,I32,I32,a,I32,b) { return 0; }
	DEFINE_INTRINSIC_FUNCTION1(_pthread_cond_broadcast,I32,I32,a) { return 0; }
	DEFINE_INTRINSIC_FUNCTION2(_pthread_key_create,I32,I32,a,I32,b) { throw "_pthread_key_create"; }
	DEFINE_INTRINSIC_FUNCTION1(_pthread_mutex_lock,I32,I32,a) { return 0; }
	DEFINE_INTRINSIC_FUNCTION1(_pthread_mutex_unlock,I32,I32,a) { return 0; }
	DEFINE_INTRINSIC_FUNCTION2(_pthread_setspecific,I32,I32,a,I32,b) { throw "_pthread_setspecific"; }
	DEFINE_INTRINSIC_FUNCTION1(_pthread_getspecific,I32,I32,a) { throw "_pthread_getspecific"; }
	DEFINE_INTRINSIC_FUNCTION2(_pthread_once,I32,I32,a,I32,b) { throw "_pthread_once"; }

	DEFINE_INTRINSIC_FUNCTION0(___ctype_b_loc,I32)
	{
		unsigned short data[384] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,2,8195,8194,8194,8194,8194,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,24577,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,49156,55304,55304,55304,55304,55304,55304,55304,55304,55304,55304,49156,49156,49156,49156,49156,49156,49156,54536,54536,54536,54536,54536,54536,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,50440,49156,49156,49156,49156,49156,49156,54792,54792,54792,54792,54792,54792,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,50696,49156,49156,49156,49156,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		static uint32_t vmAddress = 0;
		if(vmAddress == 0)
		{
			vmAddress = vmSbrk(sizeof(data));
			memcpy(&vmMemoryRef<short>(vmAddress),data,sizeof(data));
		}
		return vmAddress + sizeof(short)*128;
	}
	DEFINE_INTRINSIC_FUNCTION0(___ctype_toupper_loc,I32)
	{
		int32_t data[384] = {128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255};
		static uint32_t vmAddress = 0;
		if(vmAddress == 0)
		{
			vmAddress = vmSbrk(sizeof(data));
			memcpy(&vmMemoryRef<int32_t>(vmAddress),data,sizeof(data));
		}
		return vmAddress + sizeof(int32_t)*128;
	}
	DEFINE_INTRINSIC_FUNCTION0(___ctype_tolower_loc,I32)
	{
		int32_t data[384] = {128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255};
		static uint32_t vmAddress = 0;
		if(vmAddress == 0)
		{
			vmAddress = vmSbrk(sizeof(data));
			memcpy(&vmMemoryRef<int32_t>(vmAddress),data,sizeof(data));
		}
		return vmAddress + sizeof(int32_t)*128;
	}
	DEFINE_INTRINSIC_FUNCTION4(___assert_fail,Void,I32,condition,I32,filename,I32,line,I32,function)
	{
		ABORTValue = 1;
		const char* conditionChars = &vmMemoryRef<char>(condition);
		const char* filenameChars = &vmMemoryRef<char>(filename);
		const char* functionChars = &vmMemoryRef<char>(function);
		throw;
	}

	DEFINE_INTRINSIC_FUNCTION3(___cxa_atexit,I32,I32,a,I32,b,I32,c)
	{
		return 0;
	}
	DEFINE_INTRINSIC_FUNCTION1(___cxa_guard_acquire,I32,I32,address)
	{
		if(!vmMemoryRef<uint8_t>(address))
		{
			vmMemoryRef<uint8_t>(address) = 1;
			return 1;
		}
		else
		{
			return 0;
		}
	}
	DEFINE_INTRINSIC_FUNCTION1(___cxa_guard_release,Void,I32,a)
	{}
	DEFINE_INTRINSIC_FUNCTION3(___cxa_throw,Void,I32,a,I32,b,I32,c)
	{
		throw "___cxa_throw";
	}
	DEFINE_INTRINSIC_FUNCTION1(___cxa_begin_catch,I32,I32,a)
	{
		throw "___cxa_begin_catch";
	}
	DEFINE_INTRINSIC_FUNCTION1(___cxa_allocate_exception,I32,I32,size)
	{
		return _sbrkIntrinsicFunc(size);
	}
	DEFINE_INTRINSIC_FUNCTION0(__ZSt18uncaught_exceptionv,I32)
	{
		throw;
	}
	DEFINE_INTRINSIC_FUNCTION0(_abort,Void)
	{
		throw "_abort";
	}
	DEFINE_INTRINSIC_FUNCTION1(abort,Void,I32,code)
	{
		std::cerr << "abort(" << code << ")" << std::endl;
		throw "abort";
	}

	static uint32_t currentLocale = 0;
	DEFINE_INTRINSIC_FUNCTION1(_uselocale,I32,I32,locale)
	{
		auto oldLocale = currentLocale;
		currentLocale = locale;
		return oldLocale;
	}
	DEFINE_INTRINSIC_FUNCTION3(_newlocale,I32,I32,mask,I32,locale,I32,base)
	{
		if(!base)
		{
			base = vmSbrk(4);
		}
		return base;
	}
	DEFINE_INTRINSIC_FUNCTION1(_freelocale,Void,I32,a)
	{}

	DEFINE_INTRINSIC_FUNCTION5(_strftime_l,I32,I32,a,I32,b,I32,c,I32,d,I32,e) { throw "_strftime_l"; }
	DEFINE_INTRINSIC_FUNCTION1(_strerror,I32,I32,a) { throw "_strerror"; }

	DEFINE_INTRINSIC_FUNCTION2(_catopen,I32,I32,a,I32,b) { return -1; }
	DEFINE_INTRINSIC_FUNCTION4(_catgets,I32,I32,catd,I32,set_id,I32,msg_id,I32,s) { return s; }
	DEFINE_INTRINSIC_FUNCTION1(_catclose,I32,I32,a) { return 0; }

	DEFINE_INTRINSIC_FUNCTION3(_emscripten_memcpy_big,I32,I32,a,I32,b,I32,c) { throw "_emscripten_memcpy_big"; }

	enum class ioStreamVMHandle
	{
		StdErr = 1,
		StdIn = 2,
		StdOut = 3
	};
	FILE* vmFile(uint32_t vmHandle)
	{
		switch((ioStreamVMHandle)vmHandle)
		{
		case ioStreamVMHandle::StdErr: return stderr;
		case ioStreamVMHandle::StdIn: return stdin;
		case ioStreamVMHandle::StdOut: return stdout;
		default: return stdout;//std::cerr << "invalid file handle " << vmHandle << std::endl; throw;
		}
	}

	DEFINE_INTRINSIC_FUNCTION3(_vfprintf,I32,I32,file,I32,formatPointer,I32,argList)
	{
		throw;
	}
	DEFINE_INTRINSIC_FUNCTION1(_getc,I32,I32,file)
	{
		return getc(vmFile(file));
	}
	DEFINE_INTRINSIC_FUNCTION2(_ungetc,I32,I32,character,I32,file)
	{
		return ungetc(character,vmFile(file));
	}
	DEFINE_INTRINSIC_FUNCTION4(_fwrite,I32,I32,pointer,I32,size,I32,count,I32,file)
	{
		if(pointer + uint64_t(size) * count > (1ull << 32))
		{
			throw;
		}
		else
		{
			return fwrite(&vmMemoryRef<uint8_t>(pointer),size,count,vmFile(file));
		}
	}
	DEFINE_INTRINSIC_FUNCTION2(_fputc,I32,I32,character,I32,file)
	{
		return fputc(character,vmFile(file));
	}
	DEFINE_INTRINSIC_FUNCTION1(_fflush,I32,I32,file)
	{
		return fflush(vmFile(file));
	}

	void initEmscriptenIntrinsics()
	{
		// Allocate a 5MB stack.
		STACKTOPValue = _sbrkIntrinsicFunc(5*1024*1024);
		STACK_MAXValue = _sbrkIntrinsicFunc(0);

		// Setup IO stream handles.
		_stderrValue = _sbrkIntrinsicFunc(sizeof(uint32_t));
		_stdinValue = _sbrkIntrinsicFunc(sizeof(uint32_t));
		_stdoutValue = _sbrkIntrinsicFunc(sizeof(uint32_t));
		vmMemoryRef<uint32_t>(_stderrValue) = (uint32_t)ioStreamVMHandle::StdErr;
		vmMemoryRef<uint32_t>(_stdinValue) = (uint32_t)ioStreamVMHandle::StdIn;
		vmMemoryRef<uint32_t>(_stdoutValue) = (uint32_t)ioStreamVMHandle::StdOut;
	}
}
set(GDTOA_COMPILE_OPTIONS)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(GDTOA_THREAD_LOCAL "__declspec(thread)")
	list(APPEND GDTOA_COMPILE_OPTIONS "/wd4244") # conversion from 'x' to 'y', possible loss of data
else()
	set(GDTOA_THREAD_LOCAL "_Thread_local")
endif()

WAVM_ADD_THIRD_PARTY_LIBRARY(
	WAVMgdtoa
	SOURCES dmisc.c
			gdtoa.c
			gethex.c
			gmisc.c
			g__fmt.c
			hd_init.c
			hexnan.c
			misc.c
			smisc.c
			strtod.c
			strtodg.c
			strtof.c
			strtord.c
			sum.c
			ulp.c
			arith.h
			gdtoa.h
			gdtoaimp.h
			gdtoa_fltrnds.h
			gd_qnan.h
			stdio1.h
	PRIVATE_DEFINITIONS "-DMULTIPLE_THREADS" "-DTHREAD_LOCAL=${GDTOA_THREAD_LOCAL}"
	PRIVATE_COMPILE_OPTIONS ${GDTOA_COMPILE_OPTIONS}
)

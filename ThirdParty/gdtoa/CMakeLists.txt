add_library(gdtoa STATIC
	dmisc.c
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
	)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(gdtoa PRIVATE "/wd4244") # conversion from 'x' to 'y', possible loss of data

	target_compile_definitions(gdtoa PRIVATE "-DTHREAD_LOCAL=__declspec(thread)")
else()

	target_compile_definitions(gdtoa PRIVATE "-DTHREAD_LOCAL=_Thread_local")

	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		# Compile as a static library, but use -fPIC to make sure it can be linked into a shared object.
		target_compile_options(gdtoa PRIVATE "-fPIC")
	endif()
endif()

target_compile_definitions(gdtoa PRIVATE "-DMULTIPLE_THREADS")

# When using static linking for the WAVM libraries, the gdtoa library needs to be
# installed. Otherwise, it will just be linked into the WAVM .so/.dylib files.
if(WAVM_ENABLE_STATIC_LINKING)
	install(TARGETS gdtoa DESTINATION lib/WAVM EXPORT WAVMInstallTargets)
endif()

WAVM_SET_TARGET_SANITIZER_OPTIONS(gdtoa)

set_target_properties(gdtoa PROPERTIES FOLDER "Third party")

set(Sources
	src/AddressSpace.hpp
	src/assembly.h
	src/CompactUnwinder.hpp
	src/config.h
	src/dwarf2.h
	src/DwarfInstructions.hpp
	src/DwarfParser.hpp
	src/EHHeaderParser.hpp
	src/libunwind_ext.h
	src/libunwind.cpp
	src/Registers.hpp
	src/RWMutex.hpp
	src/Unwind-sjlj.c
	src/UnwindCursor.hpp
	src/UnwindLevel1-gcc-ext.c
	src/UnwindLevel1.c
	src/UnwindRegistersRestore.S
	src/UnwindRegistersSave.S
	)

set(AppleSources
	src/Unwind_AppleExtras.cpp
	src/Unwind-EHABI.cpp
	src/Unwind-EHABI.h
	)

set(PublicHeaders
	include/__libunwind_config.h
	include/libunwind.h
	include/unwind.h
	include/mach-o/compact_unwind_encoding.h
	)

if(MSVC)
	add_custom_target(WAVMUnwind SOURCES ${Sources} ${AppleSources} ${PublicHeaders})
else()
	if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		set(Sources ${Sources} ${AppleSources})
	endif()
	
	add_library(WAVMUnwind STATIC ${Sources} ${PublicHeaders})
	target_include_directories(WAVMUnwind PRIVATE include)

	# When using static linking for the WAVM libraries, the WAVMUnwind library needs to be
	# installed. Otherwise, it will just be linked into the WAVM .so/.dylib files.
	if(WAVM_ENABLE_STATIC_LINKING)
		install(TARGETS WAVMUnwind DESTINATION lib/WAVM EXPORT WAVMInstallTargets)
	endif()

	# Target C++11.
	target_compile_features(WAVMUnwind PRIVATE cxx_std_11)

	# Compile as a static library, but use -fPIC to make sure it can be linked into a shared object.
	target_compile_options(WAVMUnwind PRIVATE "-fPIC")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	# disable warning: 'variable' may be used uninitialized in this function
	target_compile_options(WAVMUnwind PRIVATE "-Wno-maybe-uninitialized")
	# disable warning: 'variable' set but not used
	target_compile_options(WAVMUnwind PRIVATE "-Wno-unused-but-set-variable")
endif()

set_target_properties(WAVMUnwind PROPERTIES FOLDER "Third party")
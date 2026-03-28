set(Sources
	src/AddressSpace.hpp
	src/assembly.h
	src/CompactUnwinder.hpp
	src/config.h
	src/dwarf2.h
	src/DwarfInstructions.hpp
	src/DwarfParser.hpp
	src/EHHeaderParser.hpp
	src/FrameHeaderCache.hpp
	src/libunwind_ext.h
	src/libunwind.cpp
	src/Registers.hpp
	src/RWMutex.hpp
	src/shadow_stack_unwind.h
	src/Unwind-sjlj.c
	src/UnwindCursor.hpp
	src/UnwindLevel1-gcc-ext.c
	src/UnwindLevel1.c
	src/UnwindRegistersRestore.S
	src/UnwindRegistersSave.S
	)

set(AppleSources
	src/Unwind-EHABI.cpp
	src/Unwind-EHABI.h
	)

set(PublicHeaders
	include/__libunwind_config.h
	include/libunwind.h
	include/unwind.h
	include/unwind_arm_ehabi.h
	include/unwind_itanium.h
	include/mach-o/compact_unwind_encoding.h
	)

if(MSVC)
	add_custom_target(WAVMUnwind SOURCES ${Sources} ${AppleSources} ${PublicHeaders})
else()
	if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		set(Sources ${Sources} ${AppleSources})
	endif()

	WAVM_ADD_THIRD_PARTY_LIBRARY(WAVMUnwind
		SOURCES ${Sources} ${PublicHeaders}
		PRIVATE_INCLUDE_DIRECTORIES include
		PRIVATE_DEFINITIONS _LIBUNWIND_IS_NATIVE_ONLY
	)
endif()

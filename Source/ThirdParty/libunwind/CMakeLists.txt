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

include_directories(include)

if(MSVC)
	add_custom_target(WAVMUnwind SOURCES ${Sources} ${AppleSources} ${PublicHeaders})
else()
	if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		set(Sources ${Sources} ${AppleSources})
	else()
		# disable warning: 'variable' may be used uninitialized in this function
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized")
	endif()
	
	WAVM_ADD_LIBRARY(WAVMUnwind ${Sources} ${PublicHeaders})
endif()

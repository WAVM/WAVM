set(BLAKE2_COMPILER_OPTIONS)
set(BLAKE2_PRIVATE_DEFINITIONS)

set(RefSources
	ref/blake2bp-ref.c
	ref/blake2b-ref.c
	ref/blake2-impl.h
	ref/blake2sp-ref.c
	ref/blake2s-ref.c
	ref/blake2xb-ref.c
	ref/blake2xs-ref.c
	)

set(SSESources
	sse/blake2b.c
	sse/blake2b-load-sse2.h
	sse/blake2b-load-sse41.h
	sse/blake2bp.c
	sse/blake2b-round.h
	sse/blake2-config.h
	sse/blake2-impl.h
	sse/blake2s.c
	sse/blake2s-load-sse2.h
	sse/blake2s-load-sse41.h
	sse/blake2s-load-xop.h
	sse/blake2sp.c
	sse/blake2s-round.h
	sse/blake2xb.c
	sse/blake2xs.c
	)

set(NeonSources
	neon/blake2b.c
	neon/blake2b-load-neon.h
	neon/blake2b-neon.c
	neon/blake2bp.c
	neon/blake2b-round.h
	neon/blake2-impl.h
	neon/blake2s.c
	neon/blake2s-load-neon.h
	neon/blake2s-neon.c
	neon/blake2sp.c
	neon/blake2s-round.h
	neon/blake2xb.c
	neon/blake2xs.c
	)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86")
	set(Sources ${SSESources})
	list(APPEND BLAKE2_PRIVATE_DEFINITIONS "__SSE2__")
	list(APPEND BLAKE2_PRIVATE_DEFINITIONS "__SSE4_1__")

	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		list(APPEND BLAKE2_COMPILER_OPTIONS "-msse4.1")
	endif()
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
	set(Sources ${NeonSources})
else()
	set(Sources ${RefSources})
endif()

set(PublicHeaders
	include/blake2.h
	)

if(MSVC)
	list(APPEND BLAKE2_COMPILER_OPTIONS "/wd4244") # conversion from 'uint64_t' to 'uint32_t', possible loss of data
	list(APPEND BLAKE2_COMPILER_OPTIONS "/wd4267") # conversion from 'size_t' to 'uint8_t', possible loss of data
endif()

WAVM_ADD_THIRD_PARTY_LIBRARY(WAVMBLAKE2
	SOURCES ${Sources} ${PublicHeaders}
	PUBLIC_INCLUDE_DIRECTORIES $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	PRIVATE_COMPILE_OPTIONS ${BLAKE2_COMPILER_OPTIONS}
	PRIVATE_DEFINITIONS ${BLAKE2_PRIVATE_DEFINITIONS})

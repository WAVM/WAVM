set(Sources
	ObjectCache.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/ObjectCache/ObjectCache.h)

WAVM_ADD_LIB_COMPONENT(ObjectCache
	SOURCES ${Sources}
	HEADERS ${PublicHeaders}
	PRIVATE_LIBS WAVMlmdb WAVMBLAKE2)

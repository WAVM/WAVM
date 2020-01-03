set(Sources
	ObjectCache.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/ObjectCache/ObjectCache.h)

WAVM_ADD_LIB_COMPONENT(ObjectCache
	SOURCES ${Sources} ${PublicHeaders}
	PRIVATE_LIB_COMPONENTS Platform Logging Runtime
	PRIVATE_LIBS WAVMlmdb WAVMBLAKE2)
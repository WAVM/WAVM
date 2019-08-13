set(PublicHeaders
	RuntimeABI.h)

WAVM_ADD_LIB_COMPONENT(RuntimeABI
	NONCOMPILED_SOURCES ${PublicHeaders})
	
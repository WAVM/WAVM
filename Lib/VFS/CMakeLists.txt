set(Sources
	VFS.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/VFS/VFS.h)

WAVM_ADD_LIB_COMPONENT(VFS
	SOURCES ${Sources} ${PublicHeaders})

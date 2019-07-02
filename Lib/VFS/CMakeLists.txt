set(Sources
	SandboxFS.cpp
	VFS.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/VFS/SandboxFS.h
	${WAVM_INCLUDE_DIR}/VFS/VFS.h)

WAVM_ADD_LIB_COMPONENT(VFS
	SOURCES ${Sources} ${PublicHeaders})

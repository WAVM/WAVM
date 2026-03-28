set(Sources
	ObjectLinker.cpp
	COFF.cpp
	ELF.cpp
	MachO.cpp)
set(PrivateHeaders
	ObjectLinkerPrivate.h)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/ObjectLinker/ObjectLinker.h)

WAVM_ADD_LIB_COMPONENT(ObjectLinker
	SOURCES ${Sources}
	HEADERS ${PublicHeaders} ${PrivateHeaders})

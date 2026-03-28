set(Sources
	DWARF.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/DWARF/DWARF.h)

WAVM_ADD_LIB_COMPONENT(DWARF
	SOURCES ${Sources}
	HEADERS ${PublicHeaders})

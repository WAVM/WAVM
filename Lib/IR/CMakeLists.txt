set(Sources
	DisassemblyNames.cpp
	Operators.cpp
	Types.cpp
	Validate.cpp)

set(PublicHeaders
	${WAVM_INCLUDE_DIR}/IR/IR.h
	${WAVM_INCLUDE_DIR}/IR/Module.h
	${WAVM_INCLUDE_DIR}/IR/OperatorPrinter.h
	${WAVM_INCLUDE_DIR}/IR/Operators.h
	${WAVM_INCLUDE_DIR}/IR/OperatorTable.h
	${WAVM_INCLUDE_DIR}/IR/Value.h
	${WAVM_INCLUDE_DIR}/IR/Types.h
	${WAVM_INCLUDE_DIR}/IR/Types.natvis
	${WAVM_INCLUDE_DIR}/IR/Validate.h)

WAVM_ADD_LIBRARY(IR ${Sources} ${PublicHeaders})
set_target_properties(IR PROPERTIES FOLDER Libraries)

add_definitions(-DIR_API=DLL_EXPORT)
target_link_libraries(IR Logging)

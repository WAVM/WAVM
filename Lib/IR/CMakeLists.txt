set(Sources
	DisassemblyNames.cpp
	Operators.cpp
	FloatPrinting.cpp
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
target_link_libraries(IR PRIVATE Logging Platform)

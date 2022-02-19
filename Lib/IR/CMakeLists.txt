set(Sources
	DisassemblyNames.cpp
	FeatureSpec.cpp
	FloatPrinting.cpp
	Operators.cpp
	Module.cpp
	RandomModule.cpp
	Types.cpp
	Validate.cpp)

set(PublicHeaders
	${WAVM_INCLUDE_DIR}/IR/FeatureSpec.h
	${WAVM_INCLUDE_DIR}/IR/IR.h
	${WAVM_INCLUDE_DIR}/IR/Module.h
	${WAVM_INCLUDE_DIR}/IR/OperatorPrinter.h
	${WAVM_INCLUDE_DIR}/IR/OperatorSignatures.h
	${WAVM_INCLUDE_DIR}/IR/Operators.h
	${WAVM_INCLUDE_DIR}/IR/OperatorTable.h
	${WAVM_INCLUDE_DIR}/IR/Value.h
	${WAVM_INCLUDE_DIR}/IR/RandomModule.h
	${WAVM_INCLUDE_DIR}/IR/Types.h
	${WAVM_INCLUDE_DIR}/IR/Types.natvis
	${WAVM_INCLUDE_DIR}/IR/Validate.h)

WAVM_ADD_LIB_COMPONENT(IR
	SOURCES ${Sources} ${PublicHeaders}
	PRIVATE_LIB_COMPONENTS Logging Platform)

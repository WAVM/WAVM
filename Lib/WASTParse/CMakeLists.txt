set(Sources
	Lexer.cpp
	Lexer.h
	Parse.cpp
	Parse.h
	ParseFunction.cpp
	ParseNumbers.cpp
	ParseModule.cpp
	ParseTests.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/WASTParse/WASTParse.h
	${WAVM_INCLUDE_DIR}/WASTParse/TestScript.h)

WAVM_ADD_LIB_COMPONENT(WASTParse
	SOURCES ${Sources} ${PublicHeaders}
	PRIVATE_LIB_COMPONENTS IR NFA Platform RegExp WASM Logging)
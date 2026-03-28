set(Sources
	Lexer.cpp
	Parse.cpp
	ParseFunction.cpp
	ParseNumbers.cpp
	ParseModule.cpp
	ParseTests.cpp)
set(PrivateHeaders
	Lexer.h
	Parse.h)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/WASTParse/WASTParse.h
	${WAVM_INCLUDE_DIR}/WASTParse/TestScript.h)

WAVM_ADD_LIB_COMPONENT(WASTParse
	SOURCES ${Sources}
	HEADERS ${PublicHeaders} ${PrivateHeaders})

set(Sources
	NFA.cpp
	NFA.h
	Lexer.cpp
	Lexer.h
	Parse.cpp
	Parse.h
	ParseFunction.cpp
	ParseNumbers.cpp
	ParseModule.cpp
	ParseTests.cpp
	Print.cpp
	Regexp.cpp
	Regexp.h)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/WAST/WAST.h
	${WAVM_INCLUDE_DIR}/WAST/TestScript.h)
include_directories(${WAVM_INCLUDE_DIR}/WAST)

WAVM_ADD_LIBRARY(WAST ${Sources} ${PublicHeaders})
add_definitions(-DWAST_API=DLL_EXPORT)
target_link_libraries(WAST Logging IR WASM)

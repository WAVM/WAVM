set(Sources
	IsNameChar.h
	Lexer.cpp
	Lexer.h
	NFA.cpp
	NFA.h
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
set_target_properties(WAST PROPERTIES FOLDER Libraries)

add_definitions(-DWAST_API=DLL_EXPORT)
target_link_libraries(WAST Logging IR Platform WASM)

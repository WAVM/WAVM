set(Sources
	IsNameChar.h
	Lexer.cpp
	Lexer.h
	Parse.cpp
	Parse.h
	ParseFunction.cpp
	ParseNumbers.cpp
	ParseModule.cpp
	ParseTests.cpp
	Print.cpp)
set(PublicHeaders
	${WAVM_INCLUDE_DIR}/WAST/WAST.h
	${WAVM_INCLUDE_DIR}/WAST/TestScript.h)

WAVM_ADD_LIBRARY(WAST ${Sources} ${PublicHeaders})
set_target_properties(WAST PROPERTIES FOLDER Libraries)

add_definitions(-DWAST_API=DLL_EXPORT)
target_link_libraries(WAST Logging IR NFA Platform RegExp WASM)

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

WAVM_ADD_LIBRARY(WASTParse ${Sources} ${PublicHeaders})
set_target_properties(WASTParse PROPERTIES FOLDER Libraries)

add_definitions(-DWASTPARSE_API=DLL_EXPORT)
target_link_libraries(WASTParse Logging IR NFA Platform RegExp WASM)

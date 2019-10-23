set(PrivateLibComponents Logging IR WASTParse WASM)
set(NonRuntimeSources Testing/DumpTestModules.cpp
					  Testing/TestHashMap.cpp
					  Testing/TestHashSet.cpp
					  Testing/TestI128.cpp
					  Testing/wavm-test.cpp
					  Testing/wavm-test.h
					  wavm.cpp
					  wavm.h
					  wavm-assemble.cpp
					  wavm-disassemble.cpp)

set(RuntimeOnlySources
			Testing/Benchmark.cpp
			Testing/RunTestScript.cpp
			Testing/TestCAPI.c
			wavm-compile.cpp
			wavm-run.cpp)

set(CompiledSources ${NonRuntimeSources})
set(NonCompiledSources)
if(WAVM_ENABLE_RUNTIME)
	list(APPEND PRIVATE_LIB_COMPONENTS Runtime Emscripten ThreadTest)
	list(APPEND CompiledSources ${RuntimeOnlySources})
else()
	list(APPEND NonCompiledSources ${RuntimeOnlySources})
endif()

WAVM_ADD_EXECUTABLE(wavm
	FOLDER Programs
	SOURCES ${CompiledSources}
	NONCOMPILED_SOURCES ${NonCompiledSources}
	PRIVATE_LIB_COMPONENTS ${PRIVATE_LIB_COMPONENTS})
WAVM_INSTALL_TARGET(wavm)

add_test(NAME HashMap COMMAND $<TARGET_FILE:wavm> test hashmap)
add_test(NAME HashSet COMMAND $<TARGET_FILE:wavm> test hashset)
add_test(NAME I128 COMMAND $<TARGET_FILE:wavm> test i128)

if(WAVM_ENABLE_RUNTIME)
	add_test(NAME C-API COMMAND $<TARGET_FILE:wavm> test c-api)
endif()

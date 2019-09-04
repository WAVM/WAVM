set(PrivateLibComponents Logging IR WASTParse WASM)
set(NonRuntimeSources DumpTestModules.cpp
					  TestHashMap.cpp
					  TestHashSet.cpp
					  TestI128.cpp
					  wavm-test.cpp
					  wavm-test.h)

set(RuntimeOnlySources
			InvokeBench.cpp
			RunTestScript.cpp
			TestCAPI.c)

set(CompiledSources ${NonRuntimeSources})
set(NonCompiledSources)
if(WAVM_ENABLE_RUNTIME)
	list(APPEND PRIVATE_LIB_COMPONENTS Runtime)
	list(APPEND CompiledSources ${RuntimeOnlySources})
else()
	list(APPEND NonCompiledSources ${RuntimeOnlySources})
endif()

WAVM_ADD_EXECUTABLE(wavm-test
	FOLDER Programs
	SOURCES ${CompiledSources}
	NONCOMPILED_SOURCES ${NonCompiledSources}
	PRIVATE_LIB_COMPONENTS ${PRIVATE_LIB_COMPONENTS})

add_test(NAME HashMap COMMAND $<TARGET_FILE:wavm-test> hashmap)
add_test(NAME HashSet COMMAND $<TARGET_FILE:wavm-test> hashset)
add_test(NAME I128 COMMAND $<TARGET_FILE:wavm-test> i128)

if(WAVM_ENABLE_RUNTIME)
	add_test(NAME C-API COMMAND $<TARGET_FILE:wavm-test> c-api)
endif()

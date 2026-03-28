set(NonRuntimeSources Testing/DumpTestModules.cpp
					  Testing/TestHashMap.cpp
					  Testing/TestHashSet.cpp
					  Testing/TestI128.cpp
					  Testing/TestLEB128.cpp
					  Testing/wavm-test.cpp
					  Testing/wavm-test.h
					  wavm.cpp
					  wavm.h
					  wavm-assemble.cpp
					  wavm-disassemble.cpp)

set(RuntimeOnlySources
			Testing/Benchmark.cpp
			Testing/RunTestScript.cpp
			Testing/TestAPI.cpp
			Testing/TestCAPI.c
			Testing/TestDWARF.cpp
			Testing/TestObjectLinker.cpp
			wavm-compile.cpp
			wavm-run.cpp)

set(CompiledSources ${NonRuntimeSources})
set(NonCompiledSources)
if(WAVM_ENABLE_RUNTIME)
	list(APPEND CompiledSources ${RuntimeOnlySources})
else()
	list(APPEND NonCompiledSources ${RuntimeOnlySources})
endif()

WAVM_ADD_EXECUTABLE(wavm
	FOLDER Programs
	SOURCES ${CompiledSources}
	NONCOMPILED_SOURCES ${NonCompiledSources})
WAVM_INSTALL_TARGET(wavm)

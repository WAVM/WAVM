#pragma once

#include "WAVM/Inline/Config.h"

int execDumpTestModules(int argc, char** argv);
int execHashMapTest(int argc, char** argv);
int execHashSetTest(int argc, char** argv);
int execI128Test(int argc, char** argv);
int execLEB128Test(int argc, char** argv);

#if WAVM_ENABLE_RUNTIME
int execAPITest(int argc, char** argv);
int execBenchmark(int argc, char** argv);
int execDWARFTest(int argc, char** argv);
int execObjectLinkerTest(int argc, char** argv);
int execRunTestScript(int argc, char** argv);

#ifdef __cplusplus
extern "C"
#endif
	int execCAPITest(int argc, char** argv);
#endif

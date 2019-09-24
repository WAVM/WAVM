#include "wavm-test.h"
#include <string.h>
#include "WAVM/Inline/CLI.h"
#include "WAVM/Logging/Logging.h"

using namespace WAVM;

enum class Command
{
	invalid,

	dumpModules,
	hashMap,
	hashSet,
	i128,

#if WAVM_ENABLE_RUNTIME
	cAPI,
	benchmark,
	script,
#endif
};

static const char* getCommandListHelpText()
{
	return "Commands:\n"
#if WAVM_ENABLE_RUNTIME
		   "  c-api         Test the C API\n"
#endif
		   "  dumpmodules   Dump WAST/WASM modules from WAST test scripts\n"
		   "  hashmap       Test HashMap\n"
		   "  hashset       Test HashSet\n"
		   "  i128          Test I128\n"
#if WAVM_ENABLE_RUNTIME
		   "  benchmark     Benchmark WAVM\n"
		   "  script        Run WAST test scripts\n"
#endif
		;
}

void showTestHelp(Log::Category outputCategory)
{
	Log::printf(outputCategory,
				"Usage: wavm test <command> [command arguments]\n"
				"\n"
				"%s",
				getCommandListHelpText());
}

static Command parseCommand(const char* string)
{
	if(!strcmp(string, "dumpmodules")) { return Command::dumpModules; }
	else if(!strcmp(string, "hashmap"))
	{
		return Command::hashMap;
	}
	else if(!strcmp(string, "hashset"))
	{
		return Command::hashSet;
	}
	else if(!strcmp(string, "i128"))
	{
		return Command::i128;
	}
#if WAVM_ENABLE_RUNTIME
	else if(!strcmp(string, "c-api"))
	{
		return Command::cAPI;
	}
	else if(!strcmp(string, "benchmark"))
	{
		return Command::benchmark;
	}
	else if(!strcmp(string, "script"))
	{
		return Command::script;
	}
#endif
	else
	{
		return Command::invalid;
	}
}

int execTestCommand(int argc, char** argv)
{
	if(argc < 1)
	{
		showTestHelp(Log::Category::error);
		return EXIT_FAILURE;
	}
	else
	{
		const Command command = parseCommand(argv[0]);
		switch(command)
		{
		case Command::dumpModules: return execDumpTestModules(argc - 1, argv + 1);
		case Command::hashMap: return execHashMapTest(argc - 1, argv + 1);
		case Command::hashSet: return execHashSetTest(argc - 1, argv + 1);
		case Command::i128: return execI128Test(argc - 1, argv + 1);
#if WAVM_ENABLE_RUNTIME
		case Command::cAPI: return execCAPITest(argc - 1, argv + 1);
		case Command::benchmark: return execBenchmark(argc - 1, argv + 1);
		case Command::script: return execRunTestScript(argc - 1, argv + 1);
#endif

		case Command::invalid:
			Log::printf(Log::error,
						"Invalid command: %s\n"
						"\n"
						"%s",
						argv[0],
						getCommandListHelpText());
			return EXIT_FAILURE;

		default: WAVM_UNREACHABLE();
		};
	}
}

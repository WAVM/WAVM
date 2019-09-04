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
	invokeBench,
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
		   "  invoke-bench  Benchmark invoke performance\n"
		   "  script        Run WAST test scripts\n"
#endif
		;
}

static void showTopLevelHelp(Log::Category outputCategory)
{
	Log::printf(outputCategory,
				"Usage: wavm-test <command> [command arguments]\n"
				"\n"
				"%s",
				getCommandListHelpText());
}

Command parseCommand(const char* string)
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
	else if(!strcmp(string, "invoke-bench"))
	{
		return Command::invokeBench;
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

int main(int argc, char** argv)
{
	if(!initLogFromEnvironment()) { return EXIT_FAILURE; }

	if(argc < 2)
	{
		showTopLevelHelp(Log::Category::error);
		return EXIT_FAILURE;
	}
	else
	{
		const Command command = parseCommand(argv[1]);
		switch(command)
		{
		case Command::dumpModules: return execDumpTestModules(argc - 2, argv + 2);
		case Command::hashMap: return execHashMapTest(argc - 2, argv + 2);
		case Command::hashSet: return execHashSetTest(argc - 2, argv + 2);
		case Command::i128: return execI128Test(argc - 2, argv + 2);
#if WAVM_ENABLE_RUNTIME
		case Command::cAPI: return execCAPITest(argc - 2, argv + 2);
		case Command::invokeBench: return execInvokeBench(argc - 2, argv + 2);
		case Command::script: return execRunTestScript(argc - 2, argv + 2);
#endif

		case Command::invalid:
			Log::printf(Log::error,
						"Invalid command: %s\n"
						"\n"
						"%s",
						argv[1],
						getCommandListHelpText());
			return EXIT_FAILURE;

		default: WAVM_UNREACHABLE();
		};
	}
}

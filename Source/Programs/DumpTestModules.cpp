#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Hash.h"
#include "Inline/HashMap.h"
#include "Inline/Serialization.h"
#include "WASM/WASM.h"
#include "WAST/TestScript.h"
#include "WAST/WAST.h"

#include "CLI.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

using namespace WAST;
using namespace IR;

static void dumpModule(const Module& module, const char* outputDir)
{
	const std::string wastString = WAST::print(module);
	const Uptr wastHash          = Hash<std::string>()(wastString);

	Platform::File* wastFile = Platform::openFile(
		std::string(outputDir) + "/" + std::to_string(wastHash) + ".wast",
		Platform::FileAccessMode::writeOnly,
		Platform::FileCreateMode::createAlways);
	errorUnless(Platform::writeFile(wastFile, (const U8*)wastString.c_str(), wastString.size()));
	Platform::closeFile(wastFile);

	saveBinaryModule(
		(std::string(outputDir) + "/" + std::to_string(wastHash) + ".wasm").c_str(), module);
}

static void dumpCommandModules(const Command* command, const char* outputDir)
{
	switch(command->type)
	{
	case Command::action:
	{
		auto actionCommand = (ActionCommand*)command;
		switch(actionCommand->action->type)
		{
		case ActionType::_module:
		{
			auto moduleAction = (ModuleAction*)actionCommand->action.get();
			dumpModule(*moduleAction->module, outputDir);
			break;
		}
		default: break;
		}
		break;
	}
	case Command::assert_unlinkable:
	{
		auto assertUnlinkableCommand = (AssertUnlinkableCommand*)command;
		dumpModule(*assertUnlinkableCommand->moduleAction->module, outputDir);
		break;
	}
	default: break;
	};
}

int main(int argc, char** argv)
{
	const char* filename  = nullptr;
	const char* outputDir = ".";
	bool showHelpAndExit  = false;
	if(argc < 2) { showHelpAndExit = true; }
	else
	{
		for(Iptr argumentIndex = 1; argumentIndex < argc; ++argumentIndex)
		{
			if(!strcmp(argv[argumentIndex], "--output-dir"))
			{
				++argumentIndex;
				if(argumentIndex < argc) { outputDir = argv[argumentIndex]; }
				else
				{
					std::cerr << "Expected directory after '--output-dir'" << std::endl;
					showHelpAndExit = true;
					break;
				}
			}
			else if(!filename)
			{
				filename = argv[argumentIndex];
			}
			else
			{
				std::cerr << "Unrecognized argument: " << argv[argumentIndex] << std::endl;
				showHelpAndExit = true;
				break;
			}
		}
	}

	if(showHelpAndExit)
	{
		std::cerr << "Usage: Test [--output-dir <directory>] <input .wast>" << std::endl;
		return EXIT_FAILURE;
	}

	wavmAssert(filename);

	// Always enable debug logging for tests.
	Log::setCategoryEnabled(Log::Category::debug, true);

	// Read the file into a string.
	const std::string testScriptString = loadFile(filename);
	if(!testScriptString.size()) { return EXIT_FAILURE; }

	// Process the test script.
	std::vector<std::unique_ptr<Command>> testCommands;
	std::vector<WAST::Error> testErrors;

	// Parse the test script.
	WAST::parseTestCommands(
		testScriptString.c_str(),
		testScriptString.size(),
		IR::FeatureSpec(),
		testCommands,
		testErrors);
	if(!testErrors.size())
	{
		for(auto& command : testCommands) { dumpCommandModules(command.get(), outputDir); }
		return EXIT_SUCCESS;
	}
	else
	{
		return EXIT_FAILURE;
	}
}

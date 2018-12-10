#include <stdlib.h>
#include <string.h>
#include <memory>
#include <string>
#include <vector>

#include "WAVM/IR/Module.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/File.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTParse/TestScript.h"
#include "WAVM/WASTParse/WASTParse.h"
#include "WAVM/WASTPrint/WASTPrint.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::WAST;

enum class DumpFormat
{
	wast,
	wasm,
	both
};

static void dumpWAST(const std::string& wastString, const char* outputDir)
{
	const Uptr wastHash = Hash<std::string>()(wastString);

	Platform::File* wastFile
		= Platform::openFile(std::string(outputDir) + "/" + std::to_string(wastHash) + ".wast",
							 Platform::FileAccessMode::writeOnly,
							 Platform::FileCreateMode::createAlways);
	errorUnless(wastFile);
	errorUnless(Platform::writeFile(wastFile, (const U8*)wastString.c_str(), wastString.size()));
	errorUnless(Platform::closeFile(wastFile));
}

static void dumpWASM(const U8* wasmBytes, Uptr numBytes, const char* outputDir)
{
	const Uptr wasmHash = XXH<Uptr>(wasmBytes, numBytes, 0);

	Platform::File* wasmFile
		= Platform::openFile(std::string(outputDir) + "/" + std::to_string(wasmHash) + ".wasm",
							 Platform::FileAccessMode::writeOnly,
							 Platform::FileCreateMode::createAlways);
	errorUnless(wasmFile);
	errorUnless(Platform::writeFile(wasmFile, wasmBytes, numBytes));
	errorUnless(Platform::closeFile(wasmFile));
}

static void dumpModule(const Module& module, const char* outputDir, DumpFormat dumpFormat)
{
	if(dumpFormat == DumpFormat::wast || dumpFormat == DumpFormat::both)
	{
		const std::string wastString = WAST::print(module);
		dumpWAST(wastString, outputDir);
	}

	if(dumpFormat == DumpFormat::wasm || dumpFormat == DumpFormat::both)
	{
		std::vector<U8> wasmBytes;
		try
		{
			// Serialize the WebAssembly module.
			Serialization::ArrayOutputStream stream;
			WASM::serialize(stream, module);
			wasmBytes = stream.getBytes();
		}
		catch(Serialization::FatalSerializationException const& exception)
		{
			Log::printf(Log::error,
						"Error serializing WebAssembly binary file:\n%s\n",
						exception.message.c_str());
			return;
		}

		dumpWASM(wasmBytes.data(), wasmBytes.size(), outputDir);
	}
}

static void dumpCommandModules(const Command* command, const char* outputDir, DumpFormat dumpFormat)
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
			dumpModule(*moduleAction->module, outputDir, dumpFormat);
			break;
		}
		default: break;
		}
		break;
	}
	case Command::assert_unlinkable:
	{
		auto assertUnlinkableCommand = (AssertUnlinkableCommand*)command;
		dumpModule(*assertUnlinkableCommand->moduleAction->module, outputDir, dumpFormat);
		break;
	}
	case Command::assert_invalid:
	case Command::assert_malformed:
	{
		auto assertInvalidOrMalformedCommand = (AssertInvalidOrMalformedCommand*)command;
		if(assertInvalidOrMalformedCommand->quotedModuleType == QuotedModuleType::text
		   && (dumpFormat == DumpFormat::wast || dumpFormat == DumpFormat::both))
		{ dumpWAST(assertInvalidOrMalformedCommand->quotedModuleString, outputDir); }
		else if(assertInvalidOrMalformedCommand->quotedModuleType == QuotedModuleType::binary
				&& (dumpFormat == DumpFormat::wasm || dumpFormat == DumpFormat::both))
		{
			dumpWASM((const U8*)assertInvalidOrMalformedCommand->quotedModuleString.data(),
					 assertInvalidOrMalformedCommand->quotedModuleString.size(),
					 outputDir);
		}

		break;
	}
	default: break;
	};
}

int main(int argc, char** argv)
{
	const char* filename = nullptr;
	const char* outputDir = ".";
	DumpFormat dumpFormat = DumpFormat::both;
	bool showHelpAndExit = false;

	for(Iptr argumentIndex = 1; argumentIndex < argc; ++argumentIndex)
	{
		if(!strcmp(argv[argumentIndex], "--output-dir"))
		{
			++argumentIndex;
			if(argumentIndex < argc) { outputDir = argv[argumentIndex]; }
			else
			{
				Log::printf(Log::error, "Expected directory after '--output-dir'\n");
				showHelpAndExit = true;
				break;
			}
		}
		else if(!strcmp(argv[argumentIndex], "--wast"))
		{
			dumpFormat = dumpFormat == DumpFormat::wasm ? DumpFormat::both : DumpFormat::wast;
		}
		else if(!strcmp(argv[argumentIndex], "--wasm"))
		{
			dumpFormat = dumpFormat == DumpFormat::wast ? DumpFormat::both : DumpFormat::wasm;
		}
		else if(!filename)
		{
			filename = argv[argumentIndex];
		}
		else
		{
			Log::printf(Log::error, "Unrecognized argument: %s\n", argv[argumentIndex]);
			showHelpAndExit = true;
			break;
		}
	}

	if(!filename) { showHelpAndExit = true; }

	if(showHelpAndExit)
	{
		Log::printf(
			Log::error,
			"Usage: DumpTestModule [--output-dir <directory>] [--wast] [--wasm] <input .wast>\n");
		return EXIT_FAILURE;
	}

	wavmAssert(filename);

	// Always enable debug logging for tests.
	Log::setCategoryEnabled(Log::debug, true);

	// Read the file into a vector.
	std::vector<U8> testScriptBytes;
	if(!loadFile(filename, testScriptBytes)) { return EXIT_FAILURE; }

	// Make sure the file is null terminated.
	testScriptBytes.push_back(0);

	// Process the test script.
	std::vector<std::unique_ptr<Command>> testCommands;
	std::vector<WAST::Error> testErrors;

	// Parse the test script.
	IR::FeatureSpec featureSpec;
	featureSpec.requireSharedFlagForAtomicOperators = true;
	WAST::parseTestCommands((const char*)testScriptBytes.data(),
							testScriptBytes.size(),
							featureSpec,
							testCommands,
							testErrors);
	if(!testErrors.size())
	{
		for(auto& command : testCommands)
		{ dumpCommandModules(command.get(), outputDir, dumpFormat); }
		return EXIT_SUCCESS;
	}
	else
	{
		return EXIT_FAILURE;
	}
}

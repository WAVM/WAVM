#include <string>
#include <vector>
#include "WAVM/IR/FeatureSpec.h"
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/RandomModule.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/RandomStream.h"
#include "WAVM/Platform/File.h"
#include "WAVM/VFS/VFS.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTPrint/WASTPrint.h"

namespace WAVM { namespace IR {
	struct Module;
}}

using namespace WAVM;
using namespace WAVM::IR;

I32 main(int argc, char** argv)
{
	if(argc != 4)
	{
		Log::printf(
			Log::error,
			"Usage: translate-compile-model-corpus <in dir> <out WASM dir> <out WAST dir>\n");
		return EXIT_FAILURE;
	}
	const std::string inputDir = argv[1];
	const std::string wasmOutputDir = argv[2];
	const std::string wastOutputDir = argv[3];

	VFS::DirEntStream* dirEntStream = nullptr;
	VFS::Result result = Platform::getHostFS().openDir(inputDir, dirEntStream);
	if(result != VFS::Result::success)
	{
		Log::printf(Log::error,
					"Error opening directory '%s': %s\n",
					inputDir.c_str(),
					VFS::describeResult(result));
		return EXIT_FAILURE;
	}

	I32 exitCode = EXIT_SUCCESS;
	VFS::DirEnt dirEnt;
	while(dirEntStream->getNext(dirEnt))
	{
		const std::string inputFilePath = inputDir + '/' + dirEnt.name;

		// loadFile might fail if dirEnt was a symbolic link to a directory, so just continue to
		// the next dirent if it fails.
		std::vector<U8> inputBytes;
		if(loadFile(inputFilePath.c_str(), inputBytes))
		{
			RandomStream random(inputBytes.data(), inputBytes.size());

			IR::Module module(IR::FeatureLevel::wavm);
			IR::generateValidModule(module, random);

			std::vector<U8> wasmBytes = WASM::saveBinaryModule(module);
			const std::string wasmFilePath
				= wasmOutputDir + "/compile-model-translated-" + dirEnt.name + ".wasm";
			if(!saveFile(wasmFilePath.c_str(), wasmBytes.data(), wasmBytes.size()))
			{
				exitCode = EXIT_FAILURE;
				break;
			}

			std::string wastString = WAST::print(module);
			const std::string wastFilePath
				= wastOutputDir + "/compile-model-translated-" + dirEnt.name + ".wast";
			if(!saveFile(wastFilePath.c_str(), wastString.data(), wastString.size()))
			{
				exitCode = EXIT_FAILURE;
				break;
			}
		}
	};

	dirEntStream->close();

	return exitCode;
}

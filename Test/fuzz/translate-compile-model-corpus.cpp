#include <string>
#include <vector>
#include "./RandomModule.h"
#include "WAVM/IR/IR.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Platform/File.h"
#include "WAVM/VFS/VFS.h"
#include "WAVM/WASM/WASM.h"

namespace WAVM { namespace IR {
	struct Module;
}}

using namespace WAVM;
using namespace WAVM::IR;

I32 main(int argc, char** argv)
{
	if(argc != 3)
	{
		Log::printf(Log::error,
					"Usage: translate-compile-model-corpus <in directory> <out directory>\n");
		return EXIT_FAILURE;
	}
	const std::string inputDir = argv[1];
	const std::string outputDir = argv[2];

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

			IR::Module module(IR::FeatureSpec(true));
			module.featureSpec.setWAVMFeatures(true);
			generateValidModule(module, random);

			std::vector<U8> wasmBytes = WASM::saveBinaryModule(module);

			const std::string outputFilePath
				= outputDir + "/compile-model-translated-" + dirEnt.name;
			if(!saveFile(outputFilePath.c_str(), wasmBytes.data(), wasmBytes.size()))
			{
				exitCode = EXIT_FAILURE;
				break;
			}
		}
	};

	dirEntStream->close();

	return exitCode;
}

#pragma once

#include "WAVM/Inline/Config.h"

#if !WAVM_ENABLE_LIBFUZZER

#include <algorithm>
#include <atomic>
#include "WAVM/Inline/CLI.h"
#include "WAVM/Platform/File.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Platform/Thread.h"
#include "WAVM/VFS/VFS.h"

using namespace WAVM;

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes);

// Given either a directory or file path, builds a list of file paths. If the path is to a
// directory, the directory is recursively enumerated.
// If any file system error occurs, the enumeration is aborted and the error is returned.
static VFS::Result enumInputs(const char* fileOrDirectoryPath,
							  std::vector<std::string>& outFilePaths,
							  Uptr depth = 0)
{
	if(depth > 1000) { return VFS::Result::tooManyLinksInPath; }

	VFS::DirEntStream* dirEntStream = nullptr;
	VFS::Result result = Platform::getHostFS().openDir(fileOrDirectoryPath, dirEntStream);
	if(result != VFS::Result::success)
	{
		// If the path couldn't be opened as a directory, assume that it must be a file.
		outFilePaths.push_back(fileOrDirectoryPath);
		return VFS::Result::success;
	}
	else
	{
		VFS::DirEnt dirEnt;
		while(dirEntStream->getNext(dirEnt))
		{
			if(dirEnt.name != "." && dirEnt.name != "..")
			{
				const std::string dirEntPath = std::string(fileOrDirectoryPath) + '/' + dirEnt.name;
				result = enumInputs(dirEntPath.c_str(), outFilePaths, depth + 1);
				if(result != VFS::Result::success) { break; }
			}
		};

		dirEntStream->close();

		return result;
	}
}

struct SharedState
{
	Platform::Mutex mutex;
	std::vector<std::string> inputFilePaths;

	std::atomic<Uptr> numErrors{0};
};

static I64 threadMain(void* sharedStateVoid)
{
	SharedState* sharedState = (SharedState*)sharedStateVoid;

	I64 numInputsProcessed = 0;
	while(true)
	{
		std::string inputFilePath;
		{
			Platform::Mutex::Lock sharedStateLock(sharedState->mutex);
			if(!sharedState->inputFilePaths.size()) { break; }
			inputFilePath = std::move(sharedState->inputFilePaths.back());
			sharedState->inputFilePaths.pop_back();
		}

		std::vector<U8> inputBytes;
		if(!loadFile(inputFilePath.c_str(), inputBytes)) { ++sharedState->numErrors; }
		else
		{
			LLVMFuzzerTestOneInput(inputBytes.data(), inputBytes.size());
			++numInputsProcessed;
		}
	};

	return numInputsProcessed;
}

I32 main(int argc, char** argv)
{
	if(!initLogFromEnvironment()) { return EXIT_FAILURE; }

	if(argc != 2)
	{
		Log::printf(Log::error, "Usage: %s <input file or directory>\n", argv[0]);
		return EXIT_FAILURE;
	}
	const char* inputPath = argv[1];

	SharedState sharedState;
	VFS::Result result = enumInputs(inputPath, sharedState.inputFilePaths);
	if(result != VFS::Result::success)
	{
		Log::printf(Log::error, "Error reading '%s': %s\n", inputPath, VFS::describeResult(result));
		return EXIT_FAILURE;
	}

	// Create a thread for each hardware thread.
	std::vector<Platform::Thread*> threads;
	const Uptr numHardwareThreads = Platform::getNumberOfHardwareThreads();
	const Uptr numThreads = std::min(numHardwareThreads, Uptr(sharedState.inputFilePaths.size()));
	for(Uptr threadIndex = 0; threadIndex < numThreads; ++threadIndex)
	{ threads.push_back(Platform::createThread(8 * 1024 * 1024, threadMain, &sharedState)); }

	// Wait for the threads to exit.
	I64 numInputsProcessed = 0;
	for(Platform::Thread* thread : threads) { numInputsProcessed += Platform::joinThread(thread); }
	Log::printf(Log::output, "%" PRIi64 " inputs processed.\n", numInputsProcessed);

	return sharedState.numErrors == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif

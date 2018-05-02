#pragma once

#include "Inline/BasicTypes.h"
#include "Platform/Platform.h"
#include "Runtime/Runtime.h"
#include "wavix.h"

#include <string>
#include <vector>

namespace Wavix
{
	struct Process
	{
		Runtime::GCPointer<Runtime::Compartment> compartment;

		Uptr id;

		Platform::Mutex cwdMutex;
		std::string cwd;

		Platform::Mutex fileMutex;
		std::vector<Platform::File*> files;
		std::vector<I32> freeFileIndices;

		std::vector<std::string> args;
		std::vector<std::string> envs;

		Platform::Mutex threadsMutex;
		std::vector<Thread*> threads;

		Process(
			Runtime::Compartment* inCompartment,
			const std::vector<std::string>& inArgs,
			const std::vector<std::string>& inEnvs,
			const std::string& inCWD
			);
		~Process();
	};

	extern Process* spawnProcess(
		const char* filename,
		const std::vector<std::string>& args,
		const std::vector<std::string>& env,
		const std::string& cwd
		);
}
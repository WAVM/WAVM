#pragma once

#include "Inline/BasicTypes.h"
#include "Inline/IndexAllocator.h"
#include "Platform/Platform.h"
#include "Runtime/Runtime.h"
#include "file.h"
#include "wavix.h"

#include <string>
#include <vector>

namespace Wavix
{
	struct Process
	{
		Runtime::GCPointer<Runtime::Compartment> compartment;
		Process* parent;
		I32 id;

		Platform::Mutex cwdMutex;
		std::string cwd;

		Platform::Mutex filesMutex;
		std::vector<Platform::File*> files;
		IndexAllocator<I32, INT32_MAX> filesAllocator;

		Platform::Mutex childrenMutex;
		std::vector<Process*> children;

		Platform::Mutex argsEnvMutex;
		std::vector<std::string> args;
		std::vector<std::string> envs;

		Platform::Mutex threadsMutex;
		std::vector<Thread*> threads;

		Platform::Mutex waitersMutex;
		std::vector<Thread*> waiters;
	};

	extern Process* spawnProcess(
		Process* parent,
		const char* hostFilename,
		const std::vector<std::string>& args,
		const std::vector<std::string>& envs,
		const std::string& cwd);
}
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <utility>
#include <vector>

#include "WAVM/IR/Module.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Validate.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Runtime/Linker.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/VFS/VFS.h"
#include "WAVM/WASI/WASI.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTParse/WASTParse.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;
using namespace WAVM::VFS;

struct SandboxedFileSystem : FileSystem
{
	SandboxedFileSystem(std::string&& inRootPath) : rootPath(std::move(inRootPath))
	{
		if(rootPath.back() != '/' && rootPath.back() != '\\') { rootPath += '/'; }
	}

	virtual OpenResult open(const std::string& absolutePathName,
							FileAccessMode accessMode,
							FileCreateMode createMode,
							FD*& outFD,
							FDImplicitSync implicitSync)
	{
		return Platform::openHostFile(
			getHostPath(absolutePathName), accessMode, createMode, outFD, implicitSync);
	}

	virtual GetInfoByPathResult getInfo(const std::string& absolutePathName, FileInfo& outInfo)
	{
		return Platform::getHostFileInfo(getHostPath(absolutePathName), outInfo);
	}

private:
	std::string rootPath;

	std::string getHostPath(const std::string& sandboxedAbsolutePathName)
	{
		return rootPath + sandboxedAbsolutePathName;
	}
};

static bool loadModule(const char* filename, IR::Module& outModule)
{
	// Read the specified file into an array.
	std::vector<U8> fileBytes;
	if(!loadFile(filename, fileBytes)) { return false; }

	// If the file starts with the WASM binary magic number, load it as a binary irModule.
	static const U8 wasmMagicNumber[4] = {0x00, 0x61, 0x73, 0x6d};
	if(fileBytes.size() >= 4 && !memcmp(fileBytes.data(), wasmMagicNumber, 4))
	{ return WASM::loadBinaryModule(fileBytes.data(), fileBytes.size(), outModule); }
	else
	{
		// Make sure the WAST file is null terminated.
		fileBytes.push_back(0);

		// Load it as a text irModule.
		std::vector<WAST::Error> parseErrors;
		if(!WAST::parseModule(
			   (const char*)fileBytes.data(), fileBytes.size(), outModule, parseErrors))
		{
			Log::printf(Log::error, "Error parsing WebAssembly text file:\n");
			WAST::reportParseErrors(filename, parseErrors);
			return false;
		}

		return true;
	}
}

struct CommandLineOptions
{
	const char* filename = nullptr;
	const char* rootMountPath = nullptr;
	std::vector<std::string> args;
	bool onlyCheck = false;
	bool precompiled = false;
};

static int run(const CommandLineOptions& options)
{
	IR::Module irModule;

	// Load the module.
	if(!loadModule(options.filename, irModule)) { return EXIT_FAILURE; }
	if(options.onlyCheck) { return EXIT_SUCCESS; }

	// Compile the module.
	Runtime::ModuleRef module = nullptr;
	if(!options.precompiled) { module = Runtime::compileModule(irModule); }
	else
	{
		const UserSection* precompiledObjectSection = nullptr;
		for(const UserSection& userSection : irModule.userSections)
		{
			if(userSection.name == "wavm.precompiled_object")
			{
				precompiledObjectSection = &userSection;
				break;
			}
		}

		if(!precompiledObjectSection)
		{
			Log::printf(Log::error,
						"Input file did not contain 'wavm.precompiled_object' section.\n");
			return EXIT_FAILURE;
		}
		else
		{
			module = Runtime::loadPrecompiledModule(irModule, precompiledObjectSection->data);
		}
	}

	// If a directory to mount as the root filesystem was passed on the command-line, create a
	// SandboxedFileSystem for it.
	SandboxedFileSystem* sandboxedFS = nullptr;
	if(options.rootMountPath)
	{
		std::string rootPath;
		if(options.rootMountPath[0] == '/' || options.rootMountPath[0] == '\\'
		   || options.rootMountPath[0] == '~' || options.rootMountPath[1] == ':')
		{ rootPath = options.rootMountPath; }
		else
		{
			rootPath = Platform::getCurrentWorkingDirectory() + '/' + options.rootMountPath;
		}
		sandboxedFS = new SandboxedFileSystem(std::move(rootPath));
	}

	std::vector<std::string> args = options.args;
	args.insert(args.begin(), "/proc/1/exe");

	I32 exitCode = 0;
	WASI::RunResult result = WASI::run(module,
									   std::move(args),
									   {},
									   sandboxedFS,
									   Platform::getStdFD(Platform::StdDevice::in),
									   Platform::getStdFD(Platform::StdDevice::out),
									   Platform::getStdFD(Platform::StdDevice::err),
									   exitCode);

	if(sandboxedFS) { delete sandboxedFS; }

	switch(result)
	{
	case WASI::RunResult::success: return exitCode;
	case WASI::RunResult::linkError:
		Log::printf(Log::error, "WASM module is unlinkable.\n");
		return EXIT_FAILURE;
	case WASI::RunResult::noStartFunction:
		Log::printf(Log::error, "WASM module doesn't export WASI _start function.\n");
		return EXIT_FAILURE;
	case WASI::RunResult::mistypedStartFunction:
		Log::printf(Log::error,
					"WASM module exports a _start function, but it is not the correct type.\n.");
		return EXIT_FAILURE;
	case WASI::RunResult::doesNotExportMemory:
		Log::printf(Log::error, "WASM module does not export a memory.\n");
		return EXIT_FAILURE;
	default: Errors::unreachable();
	}
}

static void showHelp()
{
	Log::printf(Log::error,
				"Usage: wavm-run-wasi [switches] [programfile] [--] [arguments]\n"
				"  in.wast|in.wasm             Specify program file (.wast/.wasm)\n"
				"  -c|--check                  Exit after checking that the program is valid\n"
				"  -d|--debug                  Write additional debug information to stdout\n"
				"  -h|--help                   Display this message\n"
				"  --precompiled               Use precompiled object code in programfile\n"
				"  --metrics                   Write benchmarking information to stdout\n"
				"  --trace-syscalls            Trace WASI syscalls to stdout\n"
				"  --trace-syscall-callstacks  Trace WASI syscalls w/ callstacks to stdout\n"
				"  --mount-root <directory>    Mounts directory as a "
				"  --                          Stop parsing arguments\n");
}

int main(int argc, char** argv)
{
	CommandLineOptions options;
	char** nextArg = argv;
	while(*++nextArg)
	{
		if(!strcmp(*nextArg, "--check") || !strcmp(*nextArg, "-c")) { options.onlyCheck = true; }
		else if(!strcmp(*nextArg, "--debug") || !strcmp(*nextArg, "-d"))
		{
			Log::setCategoryEnabled(Log::debug, true);
		}
		else if(!strcmp(*nextArg, "--metrics"))
		{
			Log::setCategoryEnabled(Log::metrics, true);
		}
		else if(!strcmp(*nextArg, "--precompiled"))
		{
			options.precompiled = true;
		}
		else if(!strcmp(*nextArg, "--trace-syscalls"))
		{
			WASI::setSyscallTraceLevel(WASI::SyscallTraceLevel::syscalls);
		}
		else if(!strcmp(*nextArg, "--trace-syscall-callstacks"))
		{
			WASI::setSyscallTraceLevel(WASI::SyscallTraceLevel::syscallsWithCallstacks);
		}
		else if(!strcmp(*nextArg, "--mount-root"))
		{
			if(!*++nextArg)
			{
				showHelp();
				return EXIT_FAILURE;
			}
			options.rootMountPath = *nextArg;
		}
		else if(!strcmp(*nextArg, "--"))
		{
			++nextArg;
			break;
		}
		else if(!strcmp(*nextArg, "--help") || !strcmp(*nextArg, "-h"))
		{
			showHelp();
			return EXIT_SUCCESS;
		}
		else if(!options.filename)
		{
			options.filename = *nextArg;
		}
		else
		{
			break;
		}
	}

	while(*nextArg) { options.args.push_back(*nextArg++); };

	if(!options.filename)
	{
		showHelp();
		return EXIT_FAILURE;
	}

	int result = EXIT_SUCCESS;
	Runtime::catchRuntimeExceptions([&result, options]() { result = run(options); },
									[](Runtime::Exception* exception) {
										// Treat any unhandled exception as a fatal error.
										Errors::fatalf("Runtime exception: %s",
													   describeException(exception).c_str());
									});
	return result;
}

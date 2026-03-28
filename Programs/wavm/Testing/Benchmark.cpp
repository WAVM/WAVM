#include <stdlib.h>
#include <string.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../wavm.h"
#include "WAVM/IR/FeatureSpec.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/File.h"
#include "WAVM/Platform/Memory.h"
#include "WAVM/Runtime/Linker.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/WASI/WASI.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTParse/WASTParse.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

static std::string getFilenameAndExtension(const char* path)
{
	const char* filenameBegin = path;
	for(Uptr charIndex = 0; path[charIndex]; ++charIndex)
	{
		if(path[charIndex] == '/' || path[charIndex] == '\\' || path[charIndex] == ':')
		{
			filenameBegin = path + charIndex + 1;
		}
	}
	return std::string(filenameBegin);
}

void showBenchmarkHelp(WAVM::Log::Category outputCategory)
{
	Log::printf(outputCategory,
				"Usage: wavm test benchmark [options] <file.wasm|wast> [program args...]\n"
				"\n"
				"Options:\n"
				"  --json               Output results as JSON (for machine parsing)\n"
				"  --enable <feature>   Enable the specified WebAssembly feature\n"
				"\n"
				"Benchmarks load, compile, instantiate, and execute phases\n"
				"of a WASI module. Arguments after the filename are passed to the\n"
				"WASI program.\n");
}

int execBenchmark(int argc, char** argv)
{
	bool jsonOutput = false;
	const char* filename = nullptr;
	std::vector<std::string> programArgs;
	IR::FeatureSpec featureSpec;

	for(int i = 0; i < argc; ++i)
	{
		if(filename)
		{
			// Everything after the filename is a program argument.
			programArgs.push_back(argv[i]);
		}
		else if(!strcmp(argv[i], "--json")) { jsonOutput = true; }
		else if(!strcmp(argv[i], "--enable"))
		{
			++i;
			if(i >= argc)
			{
				Log::printf(Log::error, "Expected feature name following '--enable'.\n");
				return EXIT_FAILURE;
			}
			if(!parseAndSetFeature(argv[i], featureSpec, true))
			{
				Log::printf(Log::error,
							"Unknown feature '%s'. Supported features:\n%s\n",
							argv[i],
							getFeatureListHelpText().c_str());
				return EXIT_FAILURE;
			}
		}
		else if(argv[i][0] != '-') { filename = argv[i]; }
		else
		{
			Log::printf(Log::error, "Unknown option: %s\n", argv[i]);
			showBenchmarkHelp(Log::error);
			return EXIT_FAILURE;
		}
	}

	if(!filename)
	{
		showBenchmarkHelp(Log::error);
		return EXIT_FAILURE;
	}

	// Phase 1: Load the file
	Timing::Timer loadTimer;
	std::vector<U8> fileBytes;
	if(!loadFile(filename, fileBytes)) { return EXIT_FAILURE; }

	IR::Module irModule(featureSpec);
	if(fileBytes.size() >= sizeof(WASM::magicNumber)
	   && !memcmp(fileBytes.data(), WASM::magicNumber, sizeof(WASM::magicNumber)))
	{
		WASM::LoadError loadError;
		if(!WASM::loadBinaryModule(fileBytes.data(), fileBytes.size(), irModule, &loadError))
		{
			Log::printf(Log::error, "%s\n", loadError.message.c_str());
			return EXIT_FAILURE;
		}
	}
	else
	{
		fileBytes.push_back(0);
		std::vector<WAST::Error> parseErrors;
		if(!WAST::parseModule(
			   (const char*)fileBytes.data(), fileBytes.size(), irModule, parseErrors))
		{
			Log::printf(Log::error, "Error parsing WebAssembly text file:\n");
			WAST::reportParseErrors(filename, (const char*)fileBytes.data(), parseErrors);
			return EXIT_FAILURE;
		}
	}
	loadTimer.stop();
	const F64 loadMs = loadTimer.getMilliseconds();

	// Phase 2: Compile the module
	Timing::Timer compileTimer;
	auto module = Runtime::compileModule(irModule);
	compileTimer.stop();
	const F64 compileMs = compileTimer.getMilliseconds();

	// Phase 3: Instantiate the module with WASI
	GCPointer<Compartment> compartment = Runtime::createCompartment();

	std::vector<std::string> wasiArgs;
	wasiArgs.push_back(getFilenameAndExtension(filename));
	for(const auto& arg : programArgs) { wasiArgs.push_back(arg); }
	auto wasiProcess = WASI::createProcess(compartment,
										   std::move(wasiArgs),
										   {},
										   nullptr,
										   Platform::getStdFD(Platform::StdDevice::in),
										   Platform::getStdFD(Platform::StdDevice::out),
										   Platform::getStdFD(Platform::StdDevice::err));

	Timing::Timer instantiateTimer;

	LinkResult linkResult = linkModule(irModule, WASI::getProcessResolver(*wasiProcess));
	if(!linkResult.success)
	{
		Log::printf(Log::error, "Failed to link module:\n");
		for(auto& missingImport : linkResult.missingImports)
		{
			Log::printf(Log::error,
						"Missing import: module=\"%s\" export=\"%s\" type=\"%s\"\n",
						missingImport.moduleName.c_str(),
						missingImport.exportName.c_str(),
						asString(missingImport.type).c_str());
		}
		return EXIT_FAILURE;
	}

	Instance* instance
		= instantiateModule(compartment, module, std::move(linkResult.resolvedImports), filename);
	if(!instance) { return EXIT_FAILURE; }

	Memory* memory = asMemoryNullable(getInstanceExport(instance, "memory"));
	if(!memory)
	{
		Log::printf(Log::error, "WASM module doesn't export WASI memory.\n");
		return EXIT_FAILURE;
	}
	WASI::setProcessMemory(*wasiProcess, memory);

	instantiateTimer.stop();
	const F64 instantiateMs = instantiateTimer.getMilliseconds();

	// Phase 4: Execute
	Context* context = Runtime::createContext(compartment);
	Function* startFunction = getStartFunction(instance);
	if(startFunction) { invokeFunction(context, startFunction); }

	Function* entryFunction = getTypedInstanceExport(instance, "_start", FunctionType());
	if(!entryFunction)
	{
		Log::printf(Log::error, "WASM module doesn't export WASI _start function.\n");
		return EXIT_FAILURE;
	}

	F64 executeMs = 0;
	{
		Timing::Timer executeTimer;
		Runtime::catchRuntimeExceptions(
			[&]() {
				auto executeThunk = [&]() -> int {
					invokeFunction(context, entryFunction);
					return 0;
				};
				WASI::catchExit(std::move(executeThunk));
			},
			[](Runtime::Exception* exception) {
				Errors::fatalf("Runtime exception: %s", describeException(exception).c_str());
			});
		executeTimer.stop();
		executeMs = executeTimer.getMilliseconds();
	}

	Uptr peakMemoryBytes = Platform::getPeakMemoryUsageBytes();

	if(jsonOutput)
	{
		Log::printf(Log::output,
					"{\n"
					"  \"file\": \"%s\",\n"
					"  \"phases\": {\n"
					"    \"load_ms\": %.2f,\n"
					"    \"compile_ms\": %.2f,\n"
					"    \"instantiate_ms\": %.2f,\n"
					"    \"execute_ms\": %.2f\n"
					"  },\n"
					"  \"peak_memory_bytes\": %" WAVM_PRIuPTR
					"\n"
					"}\n",
					getFilenameAndExtension(filename).c_str(),
					loadMs,
					compileMs,
					instantiateMs,
					executeMs,
					peakMemoryBytes);
	}
	else
	{
		Log::printf(Log::output, "File: %s\n", filename);
		Log::printf(Log::output, "  load:        %.2fms\n", loadMs);
		Log::printf(Log::output, "  compile:     %.2fms\n", compileMs);
		Log::printf(Log::output, "  instantiate: %.2fms\n", instantiateMs);
		Log::printf(Log::output, "  execute:     %.2fms\n", executeMs);
		Log::printf(Log::output, "  peak memory: %" WAVM_PRIuPTR " bytes\n", peakMemoryBytes);
	}

	// Cleanup
	wasiProcess.reset();
	WAVM_ERROR_UNLESS(tryCollectCompartment(std::move(compartment)));

	return 0;
}

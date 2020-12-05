#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Platform/File.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Linker.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/VFS/SandboxFS.h"
#include "WAVM/WASI/WASI.h"
#include "WAVM/WASM/WASM.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;
using namespace WAVM::WASI;

static bool readFile(const char* path, std::vector<U8>& outBytes)
{
	FILE* file = fopen(path, "rb");
	if(!file)
	{
		fprintf(stderr, "Failed to open '%s' for reading: %s\n", path, strerror(errno));
		return false;
	}

	if(fseek(file, 0, SEEK_END))
	{
		fprintf(stderr, "Failed to seek to end of '%s': %s\n", path, strerror(errno));
		return false;
	}

	const long numBytes = ftell(file);
	if(numBytes < 0)
	{
		fprintf(stderr, "Failed to query position in '%s': %s\n", path, strerror(errno));
		return false;
	}
	const unsigned long numBytesUnsigned = (unsigned long)numBytes;

	if(fseek(file, 0, SEEK_SET))
	{
		fprintf(stderr, "Failed to seek to beginning of '%s': %s\n", path, strerror(errno));
		return false;
	}

	outBytes.resize(numBytesUnsigned);

	size_t numBytesRead = fread(outBytes.data(), 1, numBytesUnsigned, file);
	if(numBytesRead != numBytesUnsigned)
	{
		fprintf(stderr,
				"Failed to read %lu bytes from '%s': %s\n",
				numBytesUnsigned,
				path,
				strerror(errno));
		return false;
	}

	if(fclose(file))
	{
		fprintf(stderr, "Failed to close '%s': %s\n", path, strerror(errno));
		return false;
	}

	return true;
}

int main(int argc, char** argv)
{
	if(argc < 2)
	{
		fprintf(stderr, "Usage: %s <path to WASM binary> [WASI arguments]\n", argv[0]);
		return EXIT_FAILURE;
	}

	// Load the WASM file.
	std::vector<U8> wasmBytes;
	if(!readFile(argv[1], wasmBytes)) { return EXIT_FAILURE; }

	WASM::LoadError loadError;
	ModuleRef module;
	if(!loadBinaryModule(
		   wasmBytes.data(), wasmBytes.size(), module, FeatureLevel::mature, &loadError))
	{
		fprintf(stderr, "Couldn't load '%s': %s\n", argv[1], loadError.message.c_str());
		return EXIT_FAILURE;
	}

	// Create a WAVM compartment and context.
	GCPointer<Compartment> compartment = createCompartment();
	GCPointer<Context> context = createContext(compartment);

	// Create the WASI process.
	std::vector<std::string> wasiArgs;
	for(int argIndex = 1; argIndex < argc; ++argIndex) { wasiArgs.push_back(argv[argIndex]); }

	std::shared_ptr<VFS::FileSystem> sandboxFS
		= VFS::makeSandboxFS(&Platform::getHostFS(), Platform::getCurrentWorkingDirectory());

	std::shared_ptr<WASI::Process> process
		= createProcess(compartment,
						std::move(wasiArgs),
						{},
						sandboxFS.get(),
						Platform::getStdFD(Platform::StdDevice::in),
						Platform::getStdFD(Platform::StdDevice::out),
						Platform::getStdFD(Platform::StdDevice::err));

	// Link the WASM module with the WASI exports.
	LinkResult linkResult = linkModule(getModuleIR(module), getProcessResolver(*process));
	if(!linkResult.success)
	{
		fprintf(stderr, "Failed to link '%s':\n", argv[1]);
		for(const auto& missingImport : linkResult.missingImports)
		{
			fprintf(stderr,
					"Failed to resolve import: type=%s module=%s export=%s\n",
					asString(missingImport.type).c_str(),
					missingImport.moduleName.c_str(),
					missingImport.exportName.c_str());
		}
		return EXIT_FAILURE;
	}

	// Instantiate the linked module.
	GCPointer<Instance> instance = instantiateModule(
		compartment, module, std::move(linkResult.resolvedImports), std::string(argv[1]));

	// Link WASI with the memory exported by the WASM module.
	if(Memory* memory = asMemoryNullable(getInstanceExport(instance, "memory")))
	{ setProcessMemory(*process, memory); }
	else
	{
		fprintf(stderr, "Failed to find memory export in '%s'.\n", argv[1]);
		return EXIT_FAILURE;
	}

	// Call the WASM module's "_start" function, using WASI::catchExit to handle non-local returns
	// via the WASI exit API.
	Function* startFunction = getTypedInstanceExport(instance, "_start", FunctionType());
	const I32 exitCode = WASI::catchExit([&]() -> I32 {
		invokeFunction(context, startFunction, FunctionType());
		return 0;
	});

	// Clean up the WAVM runtime objects.
	process.reset();
	instance = nullptr;
	context = nullptr;
	WAVM_ERROR_UNLESS(tryCollectCompartment(std::move(compartment)));

	return exitCode;
}

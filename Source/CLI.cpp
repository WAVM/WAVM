#include "WAVM.h"
#include "Memory.h"
#include "AST/AST.h"
#include "Runtime/LLVMJIT.h"

#include <stdexcept>
#include <iostream>
#include <fstream>

using namespace WAVM;

namespace WAVM
{
	uint8_t* vmVirtualAddressBase = nullptr;
}

int main(int argc,char** argv) try
{
	AST::Module* module = nullptr;
	const char* functionName;
	if(!stricmp(argv[1],"-text") && argc == 4)
	{
		functionName = argv[3];
		// Load the module from a text WebAssembly file.
		std::ifstream wastStream(argv[2]);
		auto wastString = std::string(std::istreambuf_iterator<char>(wastStream),std::istreambuf_iterator<char>());
		Timer loadTimer;
		WebAssemblyText::File wastFile;
		if(!WebAssemblyText::parse(wastString.c_str(),wastFile))
		{
			// Print any parse errors;
			std::cerr << "Error parsing WebAssembly text file:" << std::endl;
			for(auto error : wastFile.errors) { std::cerr << error->message.c_str() << std::endl; }
			return -1;
		}
		if(!wastFile.modules.size())
		{
			std::cerr << "WebAssembly text file didn't contain any modules!" << std::endl;
			return -1;
		}
		module = wastFile.modules[0];
		std::cout << "Loaded in " << loadTimer.getMilliseconds() << "ms" << " (" << (wastString.size()/1024.0/1024.0 / loadTimer.getSeconds()) << " MB/s)" << std::endl;
	}
	else if(!stricmp(argv[1],"-binary") && argc == 5)
	{
		functionName = argv[4];
		// Read in packed .wasm file bytes.
		std::vector<uint8_t> wasmBytes;
		std::ifstream wasmStream(argv[2],std::ios::binary | std::ios::ate);
		wasmStream.exceptions(std::ios::failbit | std::ios::badbit);
		wasmBytes.resize((unsigned int)wasmStream.tellg());
		wasmStream.seekg(0);
		wasmStream.read((char*)wasmBytes.data(),wasmBytes.size());
		wasmStream.close();

		// Load the module from a binary WebAssembly file.
		Timer loadTimer;
		std::vector<AST::ErrorRecord*> errors;
		if(!WebAssemblyBinary::decode(wasmBytes.data(),wasmBytes.size(),module,errors))
		{
			std::cerr << "Error parsing WebAssembly binary file:" << std::endl;
			for(auto error : errors) { std::cerr << error->message.c_str() << std::endl; }
			return -1;
		}
		std::cout << "Loaded in " << loadTimer.getMilliseconds() << "ms" << " (" << (wasmBytes.size()/1024.0/1024.0 / loadTimer.getSeconds()) << " MB/s)" << std::endl;

		// Load the static data from the .mem file on the commandline.
		const char* staticMemoryFileName = argv[3];
		std::ifstream staticMemoryStream(staticMemoryFileName,std::ios::binary | std::ios::ate);
		staticMemoryStream.exceptions(std::ios::failbit | std::ios::badbit);
		const uint32_t numStaticMemoryBytes = staticMemoryStream.tellg();
		auto segmentMemory = module->arena.allocate<uint8_t>(numStaticMemoryBytes);
		staticMemoryStream.seekg(0);
		staticMemoryStream.read((char*)segmentMemory,numStaticMemoryBytes);
		staticMemoryStream.close();

		module->dataSegments.push_back({8,numStaticMemoryBytes,segmentMemory});
		module->initialNumBytesMemory = numStaticMemoryBytes + 8;
		module->maxNumBytesMemory = 1ull << 32;
	}
	else
	{
		std::cerr <<  "Usage: WAVM -binary in.wasm in.js.mem functionname" << std::endl;
		std::cerr <<  "       WAVM -text in.wast functionname" << std::endl;
		return -1;
	}

	std::cout << "Loaded module uses " << (module->arena.getTotalAllocatedBytes() / 1024) << "KB" << std::endl;

	// Look up the function specified on the command line in the module.
	auto exportIt = module->exportNameToFunctionIndexMap.find(functionName);
	if(exportIt == module->exportNameToFunctionIndexMap.end())
	{
		std::cerr << "module doesn't contain named export" << std::endl;
		return -1;
	}
	
	auto function = module->functions[exportIt->second];
	if(function->type.parameters.size() != 1 || function->type.returnType != AST::TypeId::I64)
	{
		std::cerr << "exported function isn't expected type" << std::endl;
		return -1;
	}

	// Generate machine code for the module.
	LLVMJIT::compileModule(module);

	void* functionPtr = LLVMJIT::getFunctionPointer(module,exportIt->second);
	assert(functionPtr);

	// Call the generated machine code for the function.
	try
	{
		// Look up the iostream initialization function.
		auto iostreamInitExport = module->exportNameToFunctionIndexMap.find("__GLOBAL__sub_I_iostream_cpp");
		if(iostreamInitExport != module->exportNameToFunctionIndexMap.end())
		{
			auto iostreamInitFunction = module->functions[iostreamInitExport->second];
			assert(iostreamInitFunction->type.parameters.size() == 0);
			assert(iostreamInitFunction->type.returnType == AST::TypeId::Void);

			void* iostreamInitFunctionPtr = LLVMJIT::getFunctionPointer(module,iostreamInitExport->second);
			assert(iostreamInitFunctionPtr);

			((void(__cdecl*)())iostreamInitFunctionPtr)();
		}

		// Call the function specified on the command-line.
		uint64_t returnCode = ((uint64_t(__cdecl*)(uint64_t))functionPtr)(25);
		std::cout << "Program returned: " << returnCode << std::endl;
	}
	catch(...)
	{
		std::cout << "Program threw exception." << std::endl;
	}

	return 0;
}
catch(const std::ios::failure& err)
{
	std::cerr << "Failed with iostream error: " << err.what() << std::endl;
	return -1;
}
catch(const std::runtime_error& err)
{
	std::cerr << "Failed with runtime error: " << err.what() << std::endl;
	return -1;
}

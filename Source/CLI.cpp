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

std::vector<uint8_t> loadFile(const char* filename)
{
	std::ifstream stream(filename,std::ios::binary | std::ios::ate);
	if(!stream.is_open())
	{
		std::cerr << "Failed to open " << filename << std::endl;
		return std::vector<uint8_t>();
	}
	std::vector<uint8_t> data;
	data.resize((unsigned int)stream.tellg());
	stream.seekg(0);
	stream.read((char*)data.data(),data.size());
	stream.close();
	return data;
}

AST::Module* loadTextModule(const char* filename)
{
	// Read the file into a string.
	auto wastBytes = loadFile(filename);
	if(!wastBytes.size()) { return nullptr; }
	auto wastString = std::string((const char*)wastBytes.data(),wastBytes.size());
	wastBytes.clear();

	Timer loadTimer;
	WebAssemblyText::File wastFile;
	if(!WebAssemblyText::parse(wastString.c_str(),wastFile))
	{
		// Print any parse errors;
		std::cerr << "Error parsing WebAssembly text file:" << std::endl;
		for(auto error : wastFile.errors) { std::cerr << error->message.c_str() << std::endl; }
		return nullptr;
	}
	if(!wastFile.modules.size())
	{
		std::cerr << "WebAssembly text file didn't contain any modules!" << std::endl;
		return nullptr;
	}
	std::cout << "Loaded in " << loadTimer.getMilliseconds() << "ms" << " (" << (wastString.size()/1024.0/1024.0 / loadTimer.getSeconds()) << " MB/s)" << std::endl;
	return wastFile.modules[0];
}

AST::Module* loadBinaryModule(const char* wasmFilename,const char* memFilename)
{
	// Read in packed .wasm file bytes.
	auto wasmBytes = loadFile(wasmFilename);
	if(!wasmBytes.size()) { return nullptr; }

	// Load the module from a binary WebAssembly file.
	Timer loadTimer;
	std::vector<AST::ErrorRecord*> errors;
	AST::Module* module;
	if(!WebAssemblyBinary::decode(wasmBytes.data(),wasmBytes.size(),module,errors))
	{
		std::cerr << "Error parsing WebAssembly binary file:" << std::endl;
		for(auto error : errors) { std::cerr << error->message.c_str() << std::endl; }
		return nullptr;
	}
	std::cout << "Loaded in " << loadTimer.getMilliseconds() << "ms" << " (" << (wasmBytes.size()/1024.0/1024.0 / loadTimer.getSeconds()) << " MB/s)" << std::endl;

	// Load the static data from the .mem file on the commandline.
	auto staticMemoryData = loadFile(memFilename);
	if(!staticMemoryData.size()) { return nullptr; }
	auto segmentArenaData = module->arena.copyToArena(staticMemoryData.data(),staticMemoryData.size());

	module->dataSegments.push_back({8,staticMemoryData.size(),segmentArenaData});
	module->initialNumBytesMemory = staticMemoryData.size() + 8;
	module->maxNumBytesMemory = 1ull << 32;

	return module;
}

bool callModuleFunction(const AST::Module* module,const char* functionName)
{
	std::cout << "Loaded module uses " << (module->arena.getTotalAllocatedBytes() / 1024) << "KB" << std::endl;

	// Look up the function specified on the command line in the module.
	auto exportIt = module->exportNameToFunctionIndexMap.find(functionName);
	if(exportIt == module->exportNameToFunctionIndexMap.end())
	{
		std::cerr << "module doesn't contain named export" << std::endl;
		return false;
	}
	
	auto function = module->functions[exportIt->second];
	if(function->type.parameters.size() != 1 || function->type.returnType != AST::TypeId::I64)
	{
		std::cerr << "exported function isn't expected type" << std::endl;
		return false;
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
			if(iostreamInitFunction->type.parameters.size() != 0) { throw; }
			if(iostreamInitFunction->type.returnType != AST::TypeId::Void) { throw; }

			void* iostreamInitFunctionPtr = LLVMJIT::getFunctionPointer(module,iostreamInitExport->second);
			assert(iostreamInitFunctionPtr);

			((void(__cdecl*)())iostreamInitFunctionPtr)();
		}

		// Call the function specified on the command-line.
		uint64_t returnCode = ((uint64_t(__cdecl*)(uint64_t))functionPtr)(25);
		std::cout << "Program returned: " << returnCode << std::endl;
		return true;
	}
	catch(...)
	{
		std::cout << "Program threw exception." << std::endl;
		return false;
	}
}

int main(int argc,char** argv)
{
	if(!stricmp(argv[1],"runText") && argc == 4)
	{
		// Run a function in a text module.
		auto module = loadTextModule(argv[2]);
		if(!module) { return -1; }
		if(!callModuleFunction(module,argv[3])) { return -1; }
	}
	else if(!stricmp(argv[1],"runBinary") && argc == 5)
	{
		// Run a function in a binary module.
		auto module = loadBinaryModule(argv[2],argv[3]);
		if(!module) { return -1; }
		if(!callModuleFunction(module,argv[4])) { return -1; }
	}
	else if(!stricmp(argv[1],"binaryToText") && argc == 5)
	{
		// Load a binary module and dump it to a text file.
		auto module = loadBinaryModule(argv[2],argv[3]);
		if(!module) { return -1; }
		
		std::ofstream outputStream(argv[4]);
		if(!outputStream.is_open())
		{
			std::cerr << "Failed to open " << argv[4] << std::endl;
			return -1;
		}
		outputStream << WebAssemblyText::print(module);
		outputStream.close();
	}
	else
	{
		std::cerr <<  "Usage: WAVM runBinary in.wasm in.js.mem functionname" << std::endl;
		std::cerr <<  "       WAVM runText in.wast functionname" << std::endl;
		std::cerr <<  "       WAVM binaryToText in.wasm in.js.mem out.wast" << std::endl;
		return -1;
	}

	return 0;
}
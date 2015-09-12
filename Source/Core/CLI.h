#pragma once

#include "Core/Core.h"
#include "AST/AST.h"
#include "WebAssembly/WebAssembly.h"

#include <iostream>
#include <fstream>

inline std::vector<uint8_t> loadFile(const char* filename)
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

inline AST::Module* loadTextModule(const char* filename)
{
	// Read the file into a string.
	auto wastBytes = loadFile(filename);
	if(!wastBytes.size()) { return nullptr; }
	auto wastString = std::string((const char*)wastBytes.data(),wastBytes.size());
	wastBytes.clear();

	Core::Timer loadTimer;
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

inline AST::Module* loadBinaryModule(const char* wasmFilename,const char* memFilename)
{
	// Read in packed .wasm file bytes.
	auto wasmBytes = loadFile(wasmFilename);
	if(!wasmBytes.size()) { return nullptr; }

	// Load the module from a binary WebAssembly file.
	Core::Timer loadTimer;
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

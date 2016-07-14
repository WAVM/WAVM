#pragma once

#include "Core/Core.h"
#include "AST/AST.h"
#include "WebAssembly/WebAssembly.h"

#include <iostream>
#include <fstream>

inline std::vector<uint8> loadFile(const char* filename)
{
	std::ifstream stream(filename,std::ios::binary | std::ios::ate);
	if(!stream.is_open())
	{
		std::cerr << "Failed to open " << filename << ": " << std::strerror(errno) << std::endl;
		return std::vector<uint8>();
	}
	std::vector<uint8> data;
	data.resize((unsigned int)stream.tellg());
	stream.seekg(0);
	stream.read((char*)data.data(),data.size());
	stream.close();
	return data;
}

inline bool loadTextFile(const char* filename,WebAssemblyText::File& outFile)
{
	// Read the file into a string.
	auto wastBytes = loadFile(filename);
	if(!wastBytes.size()) { return false; }
	auto wastString = std::string((const char*)wastBytes.data(),wastBytes.size());
	wastBytes.clear();

	Core::Timer loadTimer;
	if(!WebAssemblyText::parse(wastString.c_str(),outFile))
	{
		// Print any parse errors;
		std::cerr << "Error parsing WebAssembly text file:" << std::endl;
		for(auto error : outFile.errors) { std::cerr << error->message.c_str() << std::endl; }
		return false;
	}
	//std::cout << "Loaded in " << loadTimer.getMilliseconds() << "ms" << " (" << (wastString.size()/1024.0/1024.0 / loadTimer.getSeconds()) << " MB/s)" << std::endl;
	return true;
}

inline bool loadTextModule(const char* filename,WebAssemblyText::File& outFile)
{
	if(!loadTextFile(filename,outFile)) { return false; }
	if(!outFile.modules.size())
	{
		std::cerr << "WebAssembly text file '" << filename << "' didn't contain any modules!" << std::endl;
		return false;
	}
	//std::cout << "Loaded in " << loadTimer.getMilliseconds() << "ms" << " (" << (wastString.size()/1024.0/1024.0 / loadTimer.getSeconds()) << " MB/s)" << std::endl;
	return true;
}

inline AST::Module* loadBinaryModule(const char* wasmFilename,const char* memFilename)
{
	// Read in packed .wasm file bytes.
	auto wasmBytes = loadFile(wasmFilename);
	if(!wasmBytes.size()) { return nullptr; }
	
	// Load the static data from the .mem file on the commandline.
	auto staticMemoryData = loadFile(memFilename);
	if(!staticMemoryData.size()) { return nullptr; }

	// Load the module from a binary WebAssembly file.
	Core::Timer loadTimer;
	std::vector<AST::ErrorRecord*> errors;
	AST::Module* module;
	if(!WebAssemblyBinary::decode(wasmBytes.data(),wasmBytes.size(),staticMemoryData.data(),staticMemoryData.size(),module,errors))
	{
		std::cerr << "Error parsing WebAssembly binary file:" << std::endl;
		for(auto error : errors) { std::cerr << error->message.c_str() << std::endl; }
		return nullptr;
	}
	//std::cout << "Loaded in " << loadTimer.getMilliseconds() << "ms" << " (" << (wasmBytes.size()/1024.0/1024.0 / loadTimer.getSeconds()) << " MB/s)" << std::endl;

	return module;
}

#pragma once

#include "Core/Core.h"
#include "AST/AST.h"
#include "WebAssembly/WebAssembly.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

inline std::vector<uint8> loadFile(const char* filename)
{
	std::ifstream stream(filename,std::ios::binary | std::ios::ate);
	if(!stream.is_open())
	{
		std::cerr << "Failed to open " << filename << std::endl;
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

		std::vector<int> wastFileLineOffsets;
		size_t start = 0;
		wastFileLineOffsets.push_back(start);
		while((start = wastString.find('\n',start)) && start != std::string::npos) { wastFileLineOffsets.push_back(++start); }
		wastFileLineOffsets.push_back(wastString.length()+1);

		for(auto error : outFile.errors)
		{
			std::cerr << filename << ":" << error->message.c_str() << std::endl;
			auto lineNumber = error->locus.lineNumber();
			auto startLine = wastFileLineOffsets[lineNumber-1];
			auto endLine =  wastFileLineOffsets[lineNumber];
			auto line = wastString.substr(startLine, endLine-startLine-1);
			std::cerr << line << std::endl;
			std::cerr << std::setw(error->locus.column(8)) << "^" << std::endl;
		}
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
		std::cerr << "WebAssembly text file didn't contain any modules!" << std::endl;
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
		for(auto error : errors) { std::cerr << wasmFilename << ":" << error->message.c_str() << std::endl; }
		return nullptr;
	}
	//std::cout << "Loaded in " << loadTimer.getMilliseconds() << "ms" << " (" << (wasmBytes.size()/1024.0/1024.0 / loadTimer.getSeconds()) << " MB/s)" << std::endl;

	return module;
}

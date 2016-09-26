#pragma once

#include "Core/Core.h"
#include "Core/Floats.h"
#include "WAST/WAST.h"
#include "WebAssembly/WebAssembly.h"
#include "WebAssembly/Module.h"
#include "Runtime/Runtime.h"
#include "Runtime/Linker.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

inline std::string loadFile(const char* filename)
{
	std::ifstream stream(filename,std::ios::binary | std::ios::ate);
	if(!stream.is_open())
	{
		std::cerr << "Failed to open " << filename << ": " << std::strerror(errno) << std::endl;
		return std::string();
	}
	std::string data;
	data.resize((unsigned int)stream.tellg());
	stream.seekg(0);
	stream.read(const_cast<char*>(data.data()),data.size());
	stream.close();
	return data;
}

inline bool loadTextModule(const char* filename,const std::string& wastString,WebAssembly::Module& outModule)
{
	std::vector<WAST::Error> parseErrors;
	if(!WAST::parseModule(wastString.c_str(),outModule,parseErrors))
	{
		// Build an index of newline offsets in the file.
		std::vector<size_t> wastFileLineOffsets;
		wastFileLineOffsets.push_back(0);
		for(size_t charIndex = 0;charIndex < wastString.length();++charIndex)
		{ if(wastString[charIndex] == '\n') { wastFileLineOffsets.push_back(charIndex+1); } }
		wastFileLineOffsets.push_back(wastString.length()+1);

		// Print any parse errors;
		std::cerr << "Error parsing WebAssembly text file:" << std::endl;
		for(auto& error : parseErrors)
		{
			std::cerr << filename << ":" << error.locus.describe() << ": " << error.message.c_str() << std::endl;
			auto startLine = wastFileLineOffsets.at(error.locus.newlines);
			auto endLine =  wastFileLineOffsets.at(error.locus.newlines+1);
			std::cerr << wastString.substr(startLine, endLine-startLine-1) << std::endl;
			std::cerr << std::setw(error.locus.column(8)) << "^" << std::endl;
		}
		return false;
	}
	return true;
}

inline bool loadTextModule(const char* filename,WebAssembly::Module& outModule)
{
	// Read the file into a string.
	auto wastBytes = loadFile(filename);
	if(!wastBytes.size()) { return false; }
	const std::string wastString = std::move(wastBytes);

	return loadTextModule(filename,wastString,outModule);
}

inline bool loadBinaryModule(const std::string& wasmBytes,WebAssembly::Module& outModule)
{
	Core::Timer loadTimer;

	// Load the module from a binary WebAssembly file.
	try
	{
		Serialization::MemoryInputStream stream((const uint8*)wasmBytes.data(),wasmBytes.size());
		WebAssembly::serialize(stream,outModule);
	}
	catch(Serialization::FatalSerializationException exception)
	{
		std::cerr << "Error deserializing WebAssembly binary file:" << std::endl;
		std::cerr << exception.message << std::endl;
		return false;
	}
	catch(WebAssembly::ValidationException exception)
	{
		std::cerr << "Error validating WebAssembly binary file:" << std::endl;
		std::cerr << exception.message << std::endl;
		return false;
	}
	catch(std::bad_alloc)
	{
		std::cerr << "Memory allocation failed: input is likely malformed" << std::endl;
		return false;
	}

	Log::logRatePerSecond("Loaded WASM",loadTimer,wasmBytes.size()/1024.0/1024.0,"MB");
	return true;
}

inline bool loadBinaryModule(const char* wasmFilename,WebAssembly::Module& outModule)
{
	// Read in packed .wasm file bytes.
	auto wasmBytes = loadFile(wasmFilename);
	if(!wasmBytes.size()) { return false; }

	return loadBinaryModule(wasmBytes,outModule);
}

inline bool loadModule(const char* filename,WebAssembly::Module& outModule)
{
	// Read the specified file into an array.
	auto fileBytes = loadFile(filename);
	if(!fileBytes.size()) { return false; }

	// If the file starts with the WASM binary magic number, load it as a binary module.
	if(*(uint32*)fileBytes.data() == 0x6d736100) { return loadBinaryModule(fileBytes,outModule); }
	else
	{
		// Otherwise, load it as a text module.
		auto wastString = std::move(fileBytes);
		return loadTextModule(filename,wastString,outModule);
	}
}

inline bool saveBinaryModule(const char* wasmFilename,const WebAssembly::Module& module)
{
	Core::Timer saveTimer;

	std::vector<uint8> wasmBytes;
	try
	{
		// Serialize the WebAssembly module.
		Serialization::ArrayOutputStream stream;
		WebAssembly::serialize(stream,module);
		wasmBytes = stream.getBytes();
	}
	catch(Serialization::FatalSerializationException exception)
	{
		std::cerr << "Error deserializing WebAssembly binary file:" << std::endl;
		std::cerr << exception.message << std::endl;
		return false;
	}
	
	Log::logRatePerSecond("Saved WASM",saveTimer,wasmBytes.size()/1024.0/1024.0,"MB");

	// Write the serialized data to the output file.
	std::ofstream outputStream(wasmFilename,std::ios::binary);
	outputStream.write((char*)wasmBytes.data(),wasmBytes.size());
	outputStream.close();
	
	return true;
}

inline bool endsWith(const char *str, const char *suffix)
{
	if(!str || !suffix) { return false; }
	size_t lenstr = strlen(str);
	size_t lensuffix = strlen(suffix);
	if(lenstr < lensuffix) { return false; }
	return (strncmp(str+lenstr-lensuffix, suffix, lensuffix) == 0);
}

int commandMain(int argc,char** argv);

int main(int argc,char** argv)
{
	try
	{
		Platform::initThread();
		return commandMain(argc,argv);
	}
	catch(WebAssembly::ValidationException exception)
	{
		std::cerr << "Failed to validate module: " << exception.message << std::endl;
		return EXIT_FAILURE;
	}
	catch(Runtime::LinkException exception)
	{
		std::cerr << "Failed to link module:" << std::endl;
		for(auto& missingImport : exception.missingImports)
		{
			std::cerr << "Missing import: module=\"" << missingImport.moduleName
				<< "\" export=\"" << missingImport.exportName
				<< "\" type=\"" << asString(missingImport.type) << "\"" << std::endl;
		}
		return EXIT_FAILURE;
	}
	catch(Runtime::InstantiationException exception)
	{
		std::cerr << "Failed to instantiate module: cause=" << std::to_string((uintp)exception.cause) << std::endl;
		return EXIT_FAILURE;
	}
	catch(Runtime::Exception exception)
	{
		std::cerr << "Runtime exception: " << describeExceptionCause(exception.cause) << std::endl;
		for(auto calledFunction : exception.callStack) { std::cerr << "  " << calledFunction << std::endl; }
		return EXIT_FAILURE;
	}
	catch(Serialization::FatalSerializationException exception)
	{
		std::cerr << "Fatal serialization exception: " << exception.message << std::endl;
		return EXIT_FAILURE;
	}
}
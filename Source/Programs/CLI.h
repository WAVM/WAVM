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

inline bool loadTextModule(const char* filename,WebAssembly::Module& outModule)
{
	// Read the file into a string.
	auto wastBytes = loadFile(filename);
	if(!wastBytes.size()) { return false; }
	auto wastString = std::string((const char*)wastBytes.data(),wastBytes.size());
	wastBytes.clear();

	#if WAVM_TIMER_OUTPUT
	Core::Timer loadTimer;
	#endif
	std::vector<WAST::ParseError> parseErrors;
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
	#if WAVM_TIMER_OUTPUT
	std::cout << "Loaded in " << loadTimer.getMilliseconds() << "ms" << " (" << (wastString.size()/1024.0/1024.0 / loadTimer.getSeconds()) << " MB/s)" << std::endl;
	#endif
	return true;
}

inline bool loadBinaryModule(const char* wasmFilename,WebAssembly::Module& outModule)
{
	// Read in packed .wasm file bytes.
	auto wasmBytes = loadFile(wasmFilename);
	if(!wasmBytes.size()) { return false; }
	
	#if WAVM_TIMER_OUTPUT
	Core::Timer loadTimer;
	#endif

	// Load the module from a binary WebAssembly file.
	try
	{
		Serialization::MemoryInputStream stream(wasmBytes.data(),wasmBytes.size());
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

	#if WAVM_TIMER_OUTPUT
	std::cout << "Loaded in " << loadTimer.getMilliseconds() << "ms" << " (" << (wasmBytes.size()/1024.0/1024.0 / loadTimer.getSeconds()) << " MB/s)" << std::endl;
	#endif
	
	return true;
}

inline bool saveBinaryModule(const char* wasmFilename,const WebAssembly::Module& module)
{
	#if WAVM_TIMER_OUTPUT
	Core::Timer saveTimer;
	#endif

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
	
	#if WAVM_TIMER_OUTPUT
	std::cout << "Saved in " << saveTimer.getMilliseconds() << "ms" << " (" << (wasmBytes.size()/1024.0/1024.0 / saveTimer.getSeconds()) << " MB/s)" << std::endl;
	#endif

	// Write the serialized data to the output file.
	std::ofstream outputStream(wasmFilename,std::ios::binary);
	outputStream.write((char*)wasmBytes.data(),wasmBytes.size());
	outputStream.close();
	
	return true;
}

inline std::string describeRuntimeValue(const Runtime::Value& value)
{
    switch(value.type)
    {
    case Runtime::TypeId::none: return "()";
    case Runtime::TypeId::i32: return "i32(" + std::to_string(value.i32) + ")";
    case Runtime::TypeId::i64: return "i64(" + std::to_string(value.i64) + ")";
    case Runtime::TypeId::f32: return "f32(" + Floats::asString(value.f32) + ")";
    case Runtime::TypeId::f64: return "f64(" + Floats::asString(value.f64) + ")";
    case Runtime::TypeId::exception: return "Exception(" + std::string(Runtime::describeExceptionCause(value.exception->cause)) + ")";
    default: throw;
    }
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
				<< "\" type=\"" << getTypeName(missingImport.type) << "\"" << std::endl;
		}
		return EXIT_FAILURE;
	}
	catch(Runtime::InstantiationException exception)
	{
		std::cerr << "Failed to instantiate module: cause=" << std::to_string((uintptr)exception.cause) << std::endl;
		return EXIT_FAILURE;
	}
	catch(Runtime::Exception* exception)
	{
		std::cerr << "Runtime exception: " << describeExceptionCause(exception->cause) << std::endl;
		for(auto calledFunction : exception->callStack) { std::cerr << "  " << calledFunction << std::endl; }
		return EXIT_FAILURE;
	}
	catch(Serialization::FatalSerializationException exception)
	{
		std::cerr << "Fatal serialization exception: " << exception.message << std::endl;
		return EXIT_FAILURE;
	}
}
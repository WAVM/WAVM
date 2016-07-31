#pragma once

#include "Core/Core.h"
#include "Core/Floats.h"
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

	#if WAVM_TIMER_OUTPUT
	Core::Timer loadTimer;
	#endif
	if(!WebAssemblyText::parse(wastString.c_str(),outFile))
	{
		// Build an index of newline offsets in the file.
		std::vector<size_t> wastFileLineOffsets;
		wastFileLineOffsets.push_back(0);
		for(size_t charIndex = 0;charIndex < wastString.length();++charIndex)
		{ if(wastString[charIndex] == '\n') { wastFileLineOffsets.push_back(charIndex+1); } }
		wastFileLineOffsets.push_back(wastString.length()+1);

		// Print any parse errors;
		std::cerr << "Error parsing WebAssembly text file:" << std::endl;
		for(auto error : outFile.errors)
		{
			std::cerr << filename << ":" << error->locus.describe() << ": " << error->message.c_str() << std::endl;
			auto startLine = wastFileLineOffsets[error->locus.newlines];
			auto endLine =  wastFileLineOffsets[error->locus.newlines+1];
			std::cerr << wastString.substr(startLine, endLine-startLine-1) << std::endl;
			std::cerr << std::setw(error->locus.column(8)) << "^" << std::endl;
		}
		return false;
	}
	#if WAVM_TIMER_OUTPUT
	std::cout << "Loaded in " << loadTimer.getMilliseconds() << "ms" << " (" << (wastString.size()/1024.0/1024.0 / loadTimer.getSeconds()) << " MB/s)" << std::endl;
	#endif
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
	#if WAVM_TIMER_OUTPUT
	Core::Timer loadTimer;
	#endif
	std::vector<AST::ErrorRecord*> errors;
	AST::Module* module;
	if(!WebAssemblyBinary::decode(wasmBytes.data(),wasmBytes.size(),staticMemoryData.data(),staticMemoryData.size(),module,errors))
	{
		std::cerr << "Error parsing WebAssembly binary file:" << std::endl;
		for(auto error : errors) { std::cerr << wasmFilename << ":" << error->message.c_str() << std::endl; }
		return nullptr;
	}
	#if WAVM_TIMER_OUTPUT
	std::cout << "Loaded in " << loadTimer.getMilliseconds() << "ms" << " (" << (wasmBytes.size()/1024.0/1024.0 / loadTimer.getSeconds()) << " MB/s)" << std::endl;
	#endif

	return module;
}

std::string describeRuntimeValue(const Runtime::Value& value)
{
        switch(value.type)
        {
        case Runtime::TypeId::None: return "None";
        case Runtime::TypeId::I8: return "I8(" + std::to_string(value.i8) + ")";
        case Runtime::TypeId::I16: return "I16(" + std::to_string(value.i16) + ")";
        case Runtime::TypeId::I32: return "I32(" + std::to_string(value.i32) + ")";
        case Runtime::TypeId::I64: return "I64(" + std::to_string(value.i64) + ")";
        case Runtime::TypeId::F32: return "F32(" + Floats::asString(value.f32) + ")";
        case Runtime::TypeId::F64: return "F64(" + Floats::asString(value.f64) + ")";
        case Runtime::TypeId::Void: return "Void";
        case Runtime::TypeId::Exception: return "Exception(" + std::string(Runtime::describeExceptionCause(value.exception->cause)) + ")";
        default: throw;
        }
}

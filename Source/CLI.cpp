#include "WAVM.h"
#include "Intrinsics.h"

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <chrono>

using namespace std;
using namespace WAVM;

namespace WAVM
{
	uint8_t* vmVirtualAddressBase = nullptr;
}

int main(int argc,char** argv) try
{
	if(argc != 4)
	{
		cerr <<  "Usage: WAVM in.wasm in.js.mem functionname" << endl;
		return -1;
	}

	const char* wasmFileName = argv[1];
	const char* staticMemoryFileName = argv[2];

	// Read in packed .wasm file bytes.
	vector<uint8_t> wasmBytes;
	ifstream wasmStream(wasmFileName,ios::binary | ios::ate);
	wasmStream.exceptions(ios::failbit | ios::badbit);
	wasmBytes.resize((unsigned int)wasmStream.tellg());
	wasmStream.seekg(0);
	wasmStream.read((char*)wasmBytes.data(),wasmBytes.size());
	wasmStream.close();

	// For memory protection, allocate a full 32-bit address space of virtual pages.
	const size_t numAllocatedVirtualPages = 1ull << (32 - getPreferredVirtualPageSizeLog2());
	vmVirtualAddressBase = allocateVirtualPages(numAllocatedVirtualPages);
	assert(vmVirtualAddressBase);

	// Read the static memory file into the VM's memory at offset 8 (taken from the one Emscripten program I tested this on).
	ifstream staticMemoryStream(staticMemoryFileName,ios::binary | ios::ate);
	staticMemoryStream.exceptions(ios::failbit | ios::badbit);
	const uint32_t numStaticMemoryBytes = staticMemoryStream.tellg();
	vmSbrk(numStaticMemoryBytes + 8);
	staticMemoryStream.seekg(0);
	staticMemoryStream.read((char*)vmVirtualAddressBase + 8,numStaticMemoryBytes);
	staticMemoryStream.close();

	// Initialize the Emscripten intrinsics.
	initEmscriptenIntrinsics();

	// Unpack .wasm file into a LLVM module.
	Module module;
	auto decodeStartTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
	if(!decodeWASM(wasmBytes.data(),module))
	{
		cerr << wasmFileName << " isn't a binary WASM file" << endl;
		return -1;
	}

	// Bind the memory buffer to the global variable used by the module as the base address for memory accesses.
	module.llvmVirtualAddressBase->setInitializer(llvm::Constant::getIntegerValue(
		llvm::Type::getInt8PtrTy(llvm::getGlobalContext()),
		llvm::APInt(64,*(uint64_t*)&vmVirtualAddressBase)
		));

	// Look up intrinsic functions that match the name+type of functions imported by the module, and bind them to the global variable used by the module to access the import.
	bool missingImport = false;
	for(auto importIt = module.functionImports.begin();importIt != module.functionImports.end();++importIt)
	{
		const IntrinsicFunction* intrinsicFunction = findIntrinsicFunction(importIt->name.c_str());
		void* functionPointer = intrinsicFunction && intrinsicFunction->type == importIt->type ? intrinsicFunction->value : nullptr;
		if(!functionPointer)
		{
			if(importIt->isReferenced)
			{
				cerr << "Missing intrinsic function " << importIt->name << " : (";
				for(auto argIt = importIt->type.args.begin();argIt != importIt->type.args.end();++argIt)
				{
					if(argIt != importIt->type.args.begin())
					{
						cerr << ",";
					}
					cerr << WASM::getTypeName(*argIt);
				}
				cerr << ") -> " << WASM::getTypeName(importIt->type.ret) << endl;
				missingImport = true;
			}
		}
		importIt->llvmVariable->setInitializer(llvm::Constant::getIntegerValue(
			importIt->llvmVariable->getType()->getElementType(),
			llvm::APInt(64,*(uint64_t*)&functionPointer)
			));
	}

	// Look up intrinsic values that match the name+type of values imported by the module, and bind them to the global variable used by the module to access the import.
	bool missingIntrinsicValue = false;
	for(auto importIt = module.valueImports.begin();importIt != module.valueImports.end();++importIt)
	{
		const IntrinsicValue* intrinsicValue = findIntrinsicValue(importIt->name.c_str());
		if(intrinsicValue && importIt->type != intrinsicValue->type)
		{
			intrinsicValue = NULL;
		}
		if(!intrinsicValue)
		{
			cerr << "Missing intrinsic value " << importIt->name << " : " << WASM::getTypeName(importIt->type) << endl;
			missingImport = true;
		}
		switch(importIt->type)
		{
		case WASM::Type::I32: importIt->llvmVariable->setInitializer(llvm::Constant::getIntegerValue(llvm::Type::getInt32Ty(llvm::getGlobalContext()),llvm::APInt(32,intrinsicValue ? *(uint32_t*)intrinsicValue->value : 0))); break;
		case WASM::Type::F32: importIt->llvmVariable->setInitializer(llvm::ConstantFP::get(llvm::getGlobalContext(),llvm::APFloat(intrinsicValue ? *(float*)intrinsicValue->value : 0.0f))); break;
		case WASM::Type::F64: importIt->llvmVariable->setInitializer(llvm::ConstantFP::get(llvm::getGlobalContext(),llvm::APFloat(intrinsicValue ? *(double*)intrinsicValue->value : 0.0))); break;
		};
	}

	if(missingImport)
	{
		return -1;
	}

	// Generate machine code for the module.
	auto jitStartTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
	cout << "Decoded in " << (jitStartTime - decodeStartTime) << "ms" << endl;
	jitCompileModule(module);
	auto jitEndTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
	cout << "JITted in " << (jitEndTime - jitStartTime) << "ms" << endl;

	// Look up the function specified on the command line in the module.
	auto foundFunction = module.exports.find(argv[3]);
	if(foundFunction != module.exports.end())
	{
		const FunctionExport& function = foundFunction->second;
		assert(function.type.args.size() == 0);
		assert(function.type.ret == WASM::ReturnType::I32);

		void* functionPtr = getJITFunctionPointer(function.llvmFunction);
		assert(functionPtr);

		// Call the generate machine code for the function.
		auto evalStartTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
		try
		{
			// Look up the iostream initialization function.
			auto iostreamInitFunction = module.exports.find("__GLOBAL__sub_I_iostream_cpp");
			if(iostreamInitFunction != module.exports.end())
			{
				assert(iostreamInitFunction->second.type.args.size() == 0);
				assert(iostreamInitFunction->second.type.ret == WASM::ReturnType::Void);

				void* iostreamInitFunctionPtr = getJITFunctionPointer(iostreamInitFunction->second.llvmFunction);
				assert(iostreamInitFunctionPtr);

				((void(__cdecl*)())iostreamInitFunctionPtr)();
			}

			uint32_t returnCode = ((uint32_t(__cdecl*)())functionPtr)();
			cout << "Program returned: " << returnCode << endl;
		}
		catch(...)
		{
			cout << "Program threw exception." << endl;
		}
		auto evalEndTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
		cout << "Evaluated in " << (evalEndTime - evalStartTime) << "ms" << endl;
	}

	return 0;
}
catch(const ios::failure& err)
{
	cerr << "Failed with iostream error: " << err.what() << endl;
	return -1;
}
catch(const runtime_error& err)
{
	cerr << "Failed with runtime error: " << err.what() << endl;
	return -1;
}

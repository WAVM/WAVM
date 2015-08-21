#include "WAVM.h"
#include "Intrinsics.h"

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <chrono>

using namespace std;
using namespace WAVM;

// The base of the 4GB virtual address space allocated for the VM.
static uint8_t* vmVirtualAddressBase = nullptr;
static size_t numCommittedVirtualPages = 0;
static uint32_t numAllocatedBytes = 0;

DEFINE_INTRINSIC_FUNCTION0(_abort,Void)
{
	throw;
}

DEFINE_INTRINSIC_FUNCTION1(abort,Void,I32,a)
{
	throw;
}

DEFINE_INTRINSIC_FUNCTION3(___cxa_throw,Void,I32,a,I32,b,I32,c)
{
	throw;
}



DEFINE_INTRINSIC_FUNCTION1(_sbrk,I32,I32,numBytes)
{
	const uint32_t existingNumBytes = numAllocatedBytes;
	if(numBytes > 0)
	{
		if(uint64_t(existingNumBytes) + numBytes > (1ull<<32))
		{
			return -1;
		}
		
		const uint32_t pageSizeLog2 = getPreferredVirtualPageSizeLog2();
		const uint32_t pageSize = 1ull << pageSizeLog2;
		const uint32_t numDesiredPages = (numAllocatedBytes + numBytes + pageSize - 1) >> pageSizeLog2;
		const uint32_t numNewPages = numDesiredPages - numCommittedVirtualPages;
		if(numNewPages > 0)
		{
			bool successfullyCommittedPhysicalMemory = commitVirtualPages(vmVirtualAddressBase + (numCommittedVirtualPages << pageSizeLog2),numNewPages);
			if(!successfullyCommittedPhysicalMemory)
			{
				return -1;
			}
			numAllocatedBytes += numBytes;
			numCommittedVirtualPages += numNewPages;
		}
	}
	else if(numBytes < 0)
	{
		numAllocatedBytes -= numBytes;
	}
	return (int32_t)existingNumBytes;
}

DEFINE_INTRINSIC_FUNCTION1(_time,I32,I32,vmAddress)
{
	time_t t = time(nullptr);
	if(vmAddress)
	{
		int32_t* address = (int32_t*)(vmVirtualAddressBase + vmAddress);
		*address = t;
	}
	return t;
}

DEFINE_INTRINSIC_FUNCTION0(___errno_location,I32)
{
	return 0;
}

DEFINE_INTRINSIC_FUNCTION1(_sysconf,I32,I32,a)
{
	enum { _SC_PAGE_SIZE = 30 };
	switch(a)
	{
	case _SC_PAGE_SIZE: return 1 << getPreferredVirtualPageSizeLog2();
	default: throw;
	}
}

DEFINE_INTRINSIC_FUNCTION1(___cxa_allocate_exception,I32,I32,size)
{
	return _sbrkIntrinsicFunc(size);
}

DEFINE_INTRINSIC_VALUE(STACKTOP,I32,);

int main(int argc,char** argv) try
{
	if(argc != 4)
	{
		cerr <<  "Usage: WAVM in.wasm functionname" << endl;
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

	// For memory protection, allocate a full 32-bit address space of virtual pages, but only commit 1MB to start (enough for the static data segment and the stack).
	const size_t numAllocatedVirtualPages = 1ull << (32 - getPreferredVirtualPageSizeLog2());
	vmVirtualAddressBase = allocateVirtualPages(numAllocatedVirtualPages);
	assert(vmVirtualAddressBase);
	auto sbrkResult = _sbrkIntrinsicFunc(5*1024*1024);
	assert(sbrkResult != -1);

	// Read the static memory file into the VM's memory at offset 8 (taken from the one Emscripten program I tested this on).
	ifstream staticMemoryStream(staticMemoryFileName,ios::binary | ios::ate);
	staticMemoryStream.exceptions(ios::failbit | ios::badbit);
	const uint32_t numStaticMemoryBytes = staticMemoryStream.tellg();
	staticMemoryStream.seekg(0);
	staticMemoryStream.read((char*)vmVirtualAddressBase + 8,numStaticMemoryBytes);
	staticMemoryStream.close();

	// Put the stack at 512KB, which allows up to 512KB-8 of static data.
	assert(numStaticMemoryBytes <= 512*1024-8);
	STACKTOPValue = 512*1024;

	// Unpack .wasm file into a LLVM module.
	Module module;
	auto decodeStartTime = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
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
	for(auto importIt = module.functionImports.begin();importIt != module.functionImports.end();++importIt)
	{
		const IntrinsicFunction* intrinsicFunction = findIntrinsicFunction(importIt->name.c_str());
		void* functionPointer = intrinsicFunction && intrinsicFunction->type == importIt->type ? intrinsicFunction->value : nullptr;
		if(!functionPointer)
		{
			cerr << "Missing intrinsic function " << importIt->name << endl;
			return -1;
		}
		importIt->llvmVariable->setInitializer(llvm::Constant::getIntegerValue(
			importIt->llvmVariable->getType()->getElementType(),
			llvm::APInt(64,*(uint64_t*)&functionPointer)
			));
	}

	// Look up intrinsic values that match the name+type of values imported by the module, and bind them to the global variable used by the module to access the import.
	for(auto importIt = module.valueImports.begin();importIt != module.valueImports.end();++importIt)
	{
		const IntrinsicValue* intrinsicValue = findIntrinsicValue(importIt->name.c_str());
		if(intrinsicValue && importIt->type != intrinsicValue->type)
		{
			intrinsicValue = NULL;
		}
		if(!intrinsicValue)
		{
			cerr << "Missing intrinsic value " << importIt->name << endl;
			return -1;
		}
		switch(importIt->type)
		{
		case WASM::Type::I32: importIt->llvmVariable->setInitializer(llvm::Constant::getIntegerValue(llvm::Type::getInt32Ty(llvm::getGlobalContext()),llvm::APInt(32,intrinsicValue ? *(uint32_t*)intrinsicValue->value : 0))); break;
		case WASM::Type::F32: importIt->llvmVariable->setInitializer(llvm::ConstantFP::get(llvm::getGlobalContext(),llvm::APFloat(intrinsicValue ? *(float*)intrinsicValue->value : 0.0f))); break;
		case WASM::Type::F64: importIt->llvmVariable->setInitializer(llvm::ConstantFP::get(llvm::getGlobalContext(),llvm::APFloat(intrinsicValue ? *(double*)intrinsicValue->value : 0.0))); break;
		};
	}

	auto jitStartTime = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
	cout << "Decoded in " << (jitStartTime - decodeStartTime) << "us" << endl;

	jitCompileModule(module);

	auto jitEndTime = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
	cout << "JITted in " << (jitEndTime - jitStartTime) << "us" << endl;

	auto foundFunction = module.exports.find(argv[3]);
	if(foundFunction != module.exports.end())
	{
		const FunctionExport& function = foundFunction->second;
		void* functionPtr = getJITFunctionPointer(function.llvmFunction);
		assert(functionPtr);

		assert(function.type.args.size() == 0);
		assert(function.type.ret == WASM::ReturnType::I32);

		auto evalStartTime = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
		try
		{
			uint32_t result = ((uint32_t(__cdecl*)())functionPtr)();
			cout << "Program result: " << result << endl;
		}
		catch(...)
		{
			cout << "Program threw exception." << endl;
		}
		auto evalEndTime = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
		cout << "Evaluated in " << (evalEndTime - evalStartTime) << "us" << endl;
	}

	// Free the virtual and physical memory used by the WebAssembly module.
	decommitVirtualPages(vmVirtualAddressBase,numCommittedVirtualPages);
	freeVirtualPages(vmVirtualAddressBase,numAllocatedVirtualPages);

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

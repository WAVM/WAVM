#pragma once
#include <stdint.h>
#include <stddef.h>
#include <memory>
#include <vector>
#include <map>

#pragma warning (disable:4267)
#pragma warning (disable:4800)
#pragma warning (disable:4291)
#pragma warning (disable:4244)

#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Constants.h"
#include "WASM.h"

namespace WAVM
{
	// A platform-independent mutex. Allows calling the constructor during static initialization, unlike std::mutex.
	struct Mutex
	{
		Mutex();
		~Mutex();

		void Lock();
		void Unlock();

	private:
		void* handle;
	};

	// RAII-style lock for Mutex.
	struct Lock
	{
		Lock(Mutex& inMutex) : mutex(&inMutex) { mutex->Lock(); }
		~Lock() { Release(); }

		void Release()
		{
			if(mutex)
			{
				mutex->Unlock();
			}
			mutex = NULL;
		}

	private:
		Mutex* mutex;
	};

	// The type of a WASM function.
	struct FunctionType
	{
		WASM::ReturnType ret;
		std::vector<WASM::Type> args;

		FunctionType() {}
		FunctionType(WASM::ReturnType ret) : ret(ret) {}
		FunctionType(WASM::ReturnType ret,std::vector<WASM::Type>&& args) : ret(ret),args(move(args)) {}
		FunctionType(WASM::ReturnType ret,const std::initializer_list<WASM::Type>&& args) : ret(ret),args(move(args)) {}

		friend bool operator==(const FunctionType& left,const FunctionType& right)
		{
			return left.ret == right.ret && left.args == right.args;
		}
	};

	// Information about a function imported into a WASM module.
	struct FunctionImport
	{
		FunctionType type;
		std::string name;
		llvm::GlobalVariable* llvmVariable;
	};

	// Information about a global variable imported into a WASM module.
	struct GlobalImport
	{
		WASM::Type type;
		std::string name;
		llvm::GlobalVariable* llvmVariable;
	};

	// Information about a function exported from a WASM module.
	struct FunctionExport
	{
		FunctionType type;
		llvm::Function* llvmFunction;
	};

	struct Module
	{
		std::vector<FunctionImport> functionImports;
		std::vector<GlobalImport> valueImports;
		std::map<std::string,FunctionExport> exports;

		llvm::Module* llvmModule;

		llvm::GlobalVariable* llvmVirtualAddressBase;
	};

	// Decodes a buffer containing the WASM module into a WAVM Module object.
	bool decodeWASM(const uint8_t* packed,Module& outModule);

	// Generates native code for a WAVM module.
	void jitCompileModule(const Module& module);

	// Gets a pointer to the native code for the given LLVM function object.
	// If the module containing the function hasn't yet been passed to jitCompileModule, will return nullptr.
	void* getJITFunctionPointer(llvm::Function* function);

	// Returns the base 2 logarithm of the preferred virtual page size.
	uint32_t getPreferredVirtualPageSizeLog2();

	// Allocates virtual addresses without commiting physical pages to them.
	// Returns the base virtual address of the allocated addresses, or nullptr if the virtual address space has been exhausted.
	uint8_t* allocateVirtualPages(size_t numPages);

	// Commits physical memory to the specified virtual pages.
	// baseVirtualAddress must be a multiple of the preferred page size.
	// Return true if successful, or false if physical memory has been exhausted.
	bool commitVirtualPages(uint8_t* baseVirtualAddress,size_t numPages);

	// Decommits the physical memory that was committed to the specified virtual pages.
	// baseVirtualAddress must be a multiple of the preferred page size.
	void decommitVirtualPages(uint8_t* baseVirtualAddress,size_t numPages);

	// Frees virtual addresses. Any physical memory committed to the addresses must have already been decommitted.
	// baseVirtualAddress must be a multiple of the preferred page size.
	void freeVirtualPages(uint8_t* baseVirtualAddress,size_t numPages);
}
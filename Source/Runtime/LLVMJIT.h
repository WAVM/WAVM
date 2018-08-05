#pragma once

#include "IR/Module.h"
#include "IR/Operators.h"
#include "Inline/BasicTypes.h"
#include "Intrinsics.h"
#include "RuntimePrivate.h"

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4267)
#pragma warning(disable : 4800)
#pragma warning(disable : 4291)
#pragma warning(disable : 4244)
#pragma warning(disable : 4351)
#pragma warning(disable : 4065)
#pragma warning(disable : 4624)
#pragma warning(disable : 4245) // conversion from 'int' to 'unsigned int', signed/unsigned mismatch
#pragma warning( \
	disable : 4146) // unary minus operator applied to unsigned type, result is still unsigned
#pragma warning(disable : 4458) // declaration of 'x' hides class member
#pragma warning(disable : 4510) // default constructor could not be generated
#pragma warning( \
	disable : 4610) // struct can never be instantiated - user defined constructor required
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#pragma warning(disable : 4702) // unreachable code
#endif

#include <cctype>
#include <string>
#include <vector>
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/LambdaResolver.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/SymbolSize.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Memory.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"

#ifdef _WIN32
#undef and
#undef or
#undef xor
#pragma warning(pop)
#endif

namespace LLVMJIT
{
	typedef llvm::SmallVector<llvm::Value*, 1> ValueVector;
	typedef llvm::SmallVector<llvm::PHINode*, 1> PHIVector;

	// The global LLVM context.
	extern llvm::LLVMContext* llvmContext;

	// Maps a type ID to the corresponding LLVM type.
	extern llvm::Type* llvmValueTypes[Uptr(ValueType::num)];

	extern llvm::Type* llvmI8Type;
	extern llvm::Type* llvmI16Type;
	extern llvm::Type* llvmI32Type;
	extern llvm::Type* llvmI64Type;
	extern llvm::Type* llvmF32Type;
	extern llvm::Type* llvmF64Type;
	extern llvm::Type* llvmVoidType;
	extern llvm::Type* llvmBoolType;
	extern llvm::Type* llvmI8PtrType;

	extern llvm::Type* llvmI8x16Type;
	extern llvm::Type* llvmI16x8Type;
	extern llvm::Type* llvmI32x4Type;
	extern llvm::Type* llvmI64x2Type;
	extern llvm::Type* llvmF32x4Type;
	extern llvm::Type* llvmF64x2Type;

#if defined(_WIN64)
	extern llvm::Type* llvmExceptionPointersStructType;
#endif

	// Zero constants of each type.
	extern llvm::Constant* typedZeroConstants[(Uptr)ValueType::num];

	// Overloaded functions that compile a literal value to a LLVM constant of the right type.
	inline llvm::ConstantInt* emitLiteral(U32 value)
	{
		return (llvm::ConstantInt*)llvm::ConstantInt::get(
			llvmI32Type, llvm::APInt(32, (U64)value, false));
	}
	inline llvm::ConstantInt* emitLiteral(I32 value)
	{
		return (llvm::ConstantInt*)llvm::ConstantInt::get(
			llvmI32Type, llvm::APInt(32, (I64)value, false));
	}
	inline llvm::ConstantInt* emitLiteral(U64 value)
	{
		return (llvm::ConstantInt*)llvm::ConstantInt::get(
			llvmI64Type, llvm::APInt(64, value, false));
	}
	inline llvm::ConstantInt* emitLiteral(I64 value)
	{
		return (llvm::ConstantInt*)llvm::ConstantInt::get(
			llvmI64Type, llvm::APInt(64, value, false));
	}
	inline llvm::Constant* emitLiteral(F32 value)
	{
		return llvm::ConstantFP::get(*llvmContext, llvm::APFloat(value));
	}
	inline llvm::Constant* emitLiteral(F64 value)
	{
		return llvm::ConstantFP::get(*llvmContext, llvm::APFloat(value));
	}
	inline llvm::Constant* emitLiteral(bool value)
	{
		return llvm::ConstantInt::get(llvmBoolType, llvm::APInt(1, value ? 1 : 0, false));
	}
	inline llvm::Constant* emitLiteral(V128 value)
	{
		return llvm::ConstantVector::get(
			{llvm::ConstantInt::get(llvmI64Type, llvm::APInt(64, value.i64[0], false)),
			 llvm::ConstantInt::get(llvmI64Type, llvm::APInt(64, value.i64[1], false))});
	}
	inline llvm::Constant* emitLiteralPointer(const void* pointer, llvm::Type* type)
	{
		auto pointerInt = llvm::APInt(sizeof(Uptr) == 8 ? 64 : 32, reinterpret_cast<Uptr>(pointer));
		return llvm::Constant::getIntegerValue(type, pointerInt);
	}

	// Converts a WebAssembly type to a LLVM type.
	inline llvm::Type* asLLVMType(ValueType type)
	{
		wavmAssert(type < ValueType::num);
		return llvmValueTypes[Uptr(type)];
	}

	inline llvm::Type* asLLVMType(TypeTuple typeTuple)
	{
		llvm::Type** llvmTypes = (llvm::Type**)alloca(sizeof(llvm::Type*) * typeTuple.size());
		for(Uptr typeIndex = 0; typeIndex < typeTuple.size(); ++typeIndex)
		{ llvmTypes[typeIndex] = asLLVMType(typeTuple[typeIndex]); }
		return llvm::StructType::get(
			*llvmContext, llvm::ArrayRef<llvm::Type*>(llvmTypes, typeTuple.size()));
	}

	inline bool areResultsReturnedDirectly(TypeTuple results)
	{
		// On X64, the calling conventions can return up to 3 i64s and 4 float/vectors.
		// For simplicity, just allow up to 3 values to be returned, including the implicitly
		// returned context pointer.
		enum
		{
			maxDirectlyReturnedValues = 3
		};
		return results.size() + 1 <= maxDirectlyReturnedValues;
	}

	inline llvm::StructType* getLLVMReturnStructType(TypeTuple results)
	{
		if(areResultsReturnedDirectly(results))
		{
			// A limited number of results can be packed into a struct and returned directly.
			return llvm::StructType::get(llvmI8PtrType, asLLVMType(results));
		}
		else
		{
			// If there are too many results to be returned directly, they will be returned in
			// the context arg/return memory block.
			return llvm::StructType::get(llvmI8PtrType);
		}
	}

	inline llvm::Constant* getZeroedLLVMReturnStruct(TypeTuple resultType)
	{
		return llvm::Constant::getNullValue(getLLVMReturnStructType(resultType));
	}

	// Converts a WebAssembly function type to a LLVM type.
	inline llvm::FunctionType* asLLVMType(
		FunctionType functionType,
		Runtime::CallingConvention callingConvention)
	{
		const Uptr numImplicitParameters
			= callingConvention == CallingConvention::intrinsicWithMemAndTable
			? 3
			: callingConvention == CallingConvention::c ? 0 : 1;
		const Uptr numParameters = numImplicitParameters + functionType.params().size();
		auto llvmArgTypes        = (llvm::Type**)alloca(sizeof(llvm::Type*) * numParameters);
		if(callingConvention == CallingConvention::intrinsicWithMemAndTable)
		{
			llvmArgTypes[0] = llvmI8PtrType;
			llvmArgTypes[1] = llvmI64Type;
			llvmArgTypes[2] = llvmI64Type;
		}
		else if(callingConvention != CallingConvention::c)
		{
			llvmArgTypes[0] = llvmI8PtrType;
		}
		for(Uptr argIndex = 0; argIndex < functionType.params().size(); ++argIndex)
		{
			llvmArgTypes[argIndex + numImplicitParameters]
				= asLLVMType(functionType.params()[argIndex]);
		}

		llvm::Type* llvmReturnType;
		switch(callingConvention)
		{
		case CallingConvention::wasm:
			llvmReturnType = getLLVMReturnStructType(functionType.results());
			break;

		case CallingConvention::intrinsicWithContextSwitch: llvmReturnType = llvmI8PtrType; break;

		case CallingConvention::intrinsicWithMemAndTable:
		case CallingConvention::intrinsic:
		case CallingConvention::c:
			switch(functionType.results().size())
			{
			case 0: llvmReturnType = llvmVoidType; break;
			case 1: llvmReturnType = asLLVMType(functionType.results()[0]); break;
			default: Errors::fatal("intrinsics/C functions returning >1 result isn't supported");
			}
			break;

		default: Errors::unreachable();
		};
		return llvm::FunctionType::get(
			llvmReturnType, llvm::ArrayRef<llvm::Type*>(llvmArgTypes, numParameters), false);
	}

	inline llvm::CallingConv::ID asLLVMCallingConv(Runtime::CallingConvention callingConvention)
	{
		switch(callingConvention)
		{
		case CallingConvention::wasm: return llvm::CallingConv::Fast;

		case CallingConvention::intrinsic:
		case CallingConvention::intrinsicWithContextSwitch:
		case CallingConvention::intrinsicWithMemAndTable:
		case CallingConvention::c: return llvm::CallingConv::C;

		default: Errors::unreachable();
		}
	}

	// Code and state that is used for generating IR for both thunks and WASM functions.
	struct EmitContext
	{
		llvm::IRBuilder<> irBuilder;

		llvm::Value* contextPointerVariable;
		llvm::Value* memoryBasePointerVariable;
		llvm::Value* tableBasePointerVariable;

		EmitContext(MemoryInstance* inDefaultMemory, TableInstance* inDefaultTable)
		: irBuilder(*llvmContext)
		, contextPointerVariable(nullptr)
		, memoryBasePointerVariable(nullptr)
		, tableBasePointerVariable(nullptr)
		, defaultMemory(inDefaultMemory)
		, defaultTable(inDefaultTable)
		{
		}

		llvm::Value* loadFromUntypedPointer(llvm::Value* pointer, llvm::Type* valueType)
		{
			return irBuilder.CreateLoad(
				irBuilder.CreatePointerCast(pointer, valueType->getPointerTo()));
		}

		void reloadMemoryAndTableBase()
		{
			// Derive the compartment runtime data from the context address by masking off the lower
			// 32 bits.
			llvm::Value* compartmentAddress = irBuilder.CreateIntToPtr(
				irBuilder.CreateAnd(
					irBuilder.CreatePtrToInt(
						irBuilder.CreateLoad(contextPointerVariable), llvmI64Type),
					emitLiteral(~((U64(1) << 32) - 1))),
				llvmI8PtrType);

			// Load the defaultMemoryBase and defaultTableBase values from the runtime data for this
			// module instance.

			if(defaultMemory)
			{
				const Uptr defaultMemoryBaseOffset
					= offsetof(CompartmentRuntimeData, memories) + sizeof(U8*) * defaultMemory->id;
				irBuilder.CreateStore(
					loadFromUntypedPointer(
						irBuilder.CreateInBoundsGEP(
							compartmentAddress, {emitLiteral(defaultMemoryBaseOffset)}),
						llvmI8PtrType),
					memoryBasePointerVariable);
			}

			if(defaultTable)
			{
				const Uptr defaultTableBaseOffset = offsetof(CompartmentRuntimeData, tables)
					+ sizeof(TableInstance::FunctionElement*) * defaultTable->id;
				irBuilder.CreateStore(
					loadFromUntypedPointer(
						irBuilder.CreateInBoundsGEP(
							compartmentAddress, {emitLiteral(defaultTableBaseOffset)}),
						llvmI8PtrType),
					tableBasePointerVariable);
			}
		}

		// Creates either a call or an invoke if the call occurs inside a try.
		ValueVector emitCallOrInvoke(
			llvm::Value* callee,
			llvm::ArrayRef<llvm::Value*> args,
			FunctionType calleeType,
			CallingConvention callingConvention,
			llvm::BasicBlock* unwindToBlock = nullptr)
		{
			llvm::ArrayRef<llvm::Value*> augmentedArgs = args;

			if(callingConvention == CallingConvention::intrinsicWithMemAndTable)
			{
				// Augment the argument list with the context pointer, and the default memory and
				// table IDs.
				auto augmentedArgsAlloca
					= (llvm::Value**)alloca(sizeof(llvm::Value*) * (args.size() + 3));
				augmentedArgs = llvm::ArrayRef<llvm::Value*>(augmentedArgsAlloca, args.size() + 3);
				augmentedArgsAlloca[0] = irBuilder.CreateLoad(contextPointerVariable);
				augmentedArgsAlloca[1]
					= defaultMemory ? emitLiteral(I64(defaultMemory->id)) : emitLiteral(I64(-1));
				augmentedArgsAlloca[2]
					= defaultTable ? emitLiteral(I64(defaultTable->id)) : emitLiteral(I64(-1));
				for(Uptr argIndex = 0; argIndex < args.size(); ++argIndex)
				{ augmentedArgsAlloca[3 + argIndex] = args[argIndex]; }
			}
			else if(callingConvention != CallingConvention::c)
			{
				// Augment the argument list with the context pointer.
				auto augmentedArgsAlloca
					= (llvm::Value**)alloca(sizeof(llvm::Value*) * (args.size() + 1));
				augmentedArgs = llvm::ArrayRef<llvm::Value*>(augmentedArgsAlloca, args.size() + 1);
				augmentedArgsAlloca[0] = irBuilder.CreateLoad(contextPointerVariable);
				for(Uptr argIndex = 0; argIndex < args.size(); ++argIndex)
				{ augmentedArgsAlloca[1 + argIndex] = args[argIndex]; }
			}

			// Call or invoke the callee.
			llvm::Value* returnValue;
			if(!unwindToBlock)
			{
				auto call = irBuilder.CreateCall(callee, augmentedArgs);
				call->setCallingConv(asLLVMCallingConv(callingConvention));
				returnValue = call;
			}
			else
			{
				auto returnBlock = llvm::BasicBlock::Create(
					*llvmContext, "invokeReturn", irBuilder.GetInsertBlock()->getParent());
				auto invoke
					= irBuilder.CreateInvoke(callee, returnBlock, unwindToBlock, augmentedArgs);
				invoke->setCallingConv(asLLVMCallingConv(callingConvention));
				irBuilder.SetInsertPoint(returnBlock);
				returnValue = invoke;
			}

			ValueVector results;
			switch(callingConvention)
			{
			case CallingConvention::wasm:
			{
				// Update the context variable.
				auto newContextPointer = irBuilder.CreateExtractValue(returnValue, {0});
				irBuilder.CreateStore(newContextPointer, contextPointerVariable);

				// Reload the memory/table base pointers.
				reloadMemoryAndTableBase();

				if(areResultsReturnedDirectly(calleeType.results()))
				{
					// If the results are returned directly, extract them from the returned struct.
					for(Uptr resultIndex = 0; resultIndex < calleeType.results().size();
						++resultIndex)
					{
						results.push_back(
							irBuilder.CreateExtractValue(returnValue, {U32(1), U32(resultIndex)}));
					}
				}
				else
				{
					// Otherwise, load them from the context.
					Uptr resultOffset = 0;
					for(ValueType resultType : calleeType.results())
					{
						const U8 resultNumBytes = getTypeByteWidth(resultType);

						resultOffset = (resultOffset + resultNumBytes - 1) & -I8(resultNumBytes);
						wavmAssert(resultOffset < maxThunkArgAndReturnBytes);

						results.push_back(irBuilder.CreateLoad(irBuilder.CreatePointerCast(
							irBuilder.CreateInBoundsGEP(
								newContextPointer, {emitLiteral(resultOffset)}),
							asLLVMType(resultType)->getPointerTo())));

						resultOffset += resultNumBytes;
					}
				}

				break;
			}
			case CallingConvention::intrinsicWithContextSwitch:
			{
				auto newContextPointer = returnValue;

				// Update the context variable.
				irBuilder.CreateStore(newContextPointer, contextPointerVariable);

				// Reload the memory/table base pointers.
				reloadMemoryAndTableBase();

				// Load the call result from the returned context.
				wavmAssert(calleeType.results().size() <= 1);
				if(calleeType.results().size() == 1)
				{
					llvm::Type* llvmResultType = asLLVMType(calleeType.results()[0]);
					results.push_back(loadFromUntypedPointer(newContextPointer, llvmResultType));
				}

				break;
			}
			case CallingConvention::intrinsicWithMemAndTable:
			case CallingConvention::intrinsic:
			case CallingConvention::c:
			{
				wavmAssert(calleeType.results().size() <= 1);
				if(calleeType.results().size() == 1) { results.push_back(returnValue); }
				break;
			}
			default: Errors::unreachable();
			};

			return results;
		}

		void emitReturn(TypeTuple resultTypes, const llvm::ArrayRef<llvm::Value*>& results)
		{
			llvm::Value* returnStruct = getZeroedLLVMReturnStruct(resultTypes);
			returnStruct              = irBuilder.CreateInsertValue(
                returnStruct, irBuilder.CreateLoad(contextPointerVariable), {U32(0)});

			wavmAssert(resultTypes.size() == results.size());
			if(areResultsReturnedDirectly(resultTypes))
			{
				// If the results are returned directly, insert them into the return struct.
				for(Uptr resultIndex = 0; resultIndex < results.size(); ++resultIndex)
				{
					llvm::Value* result = results[resultIndex];
					returnStruct        = irBuilder.CreateInsertValue(
                        returnStruct, result, {U32(1), U32(resultIndex)});
				}
			}
			else
			{
				// Otherwise, store them in the context.
				Uptr resultOffset = 0;
				for(Uptr resultIndex = 0; resultIndex < results.size(); ++resultIndex)
				{
					const ValueType resultType = resultTypes[resultIndex];
					const U8 resultNumBytes    = getTypeByteWidth(resultType);

					resultOffset = (resultOffset + resultNumBytes - 1) & -I8(resultNumBytes);
					wavmAssert(resultOffset < maxThunkArgAndReturnBytes);

					irBuilder.CreateStore(
						results[resultIndex],
						irBuilder.CreatePointerCast(
							irBuilder.CreateInBoundsGEP(
								irBuilder.CreateLoad(contextPointerVariable),
								{emitLiteral(resultOffset)}),
							asLLVMType(resultType)->getPointerTo()));

					resultOffset += resultNumBytes;
				}
			}

			irBuilder.CreateRet(returnStruct);
		}

	private:
		MemoryInstance* defaultMemory;
		TableInstance* defaultTable;
	};

	// Functions that map between the symbols used for externally visible functions and the function
	std::string getExternalFunctionName(ModuleInstance* moduleInstance, Uptr functionDefIndex);
	bool getFunctionIndexFromExternalName(const char* externalName, Uptr& outFunctionDefIndex);

	// Emits LLVM IR for a module.
	std::shared_ptr<llvm::Module> emitModule(
		const IR::Module& module,
		ModuleInstance* moduleInstance);

	// Used to override LLVM's default behavior of looking up unresolved symbols in DLL exports.
	struct NullResolver : llvm::JITSymbolResolver
	{
		static std::shared_ptr<NullResolver> singleton;
		virtual llvm::JITSymbol findSymbol(const std::string& name) override;
		virtual llvm::JITSymbol findSymbolInLogicalDylib(const std::string& name) override;
	};

#ifdef _WIN64
	extern void processSEHTables(
		Uptr imageBaseAddress,
		const llvm::LoadedObjectInfo* loadedObject,
		const llvm::object::SectionRef& pdataSection,
		const U8* pdataCopy,
		Uptr pdataNumBytes,
		const llvm::object::SectionRef& xdataSection,
		const U8* xdataCopy,
		Uptr sehTrampolineAddress);
#endif
}

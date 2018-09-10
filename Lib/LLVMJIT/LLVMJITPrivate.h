#pragma once

#include "IR/Module.h"
#include "IR/Operators.h"
#include "Inline/BasicTypes.h"
#include "LLVMJIT/LLVMJIT.h"
#include "Runtime/RuntimeData.h"

#include <cctype>
#include <string>
#include <vector>

#include "LLVMPreInclude.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/DataTypes.h"

#include "LLVMPostInclude.h"

#ifdef _WIN32
#define USE_WINDOWS_SEH 1
#else
#define USE_WINDOWS_SEH 0
#endif

namespace llvm
{
	class LoadedObjectInfo;

	namespace object
	{
		class SectionRef;
	}
}

namespace LLVMJIT
{
	typedef llvm::SmallVector<llvm::Value*, 1> ValueVector;
	typedef llvm::SmallVector<llvm::PHINode*, 1> PHIVector;

	struct LLVMContext : llvm::LLVMContext
	{
		llvm::Type* i8Type;
		llvm::Type* i16Type;
		llvm::Type* i32Type;
		llvm::Type* i64Type;
		llvm::Type* f32Type;
		llvm::Type* f64Type;
		llvm::Type* i8PtrType;
		llvm::Type* iptrType;

		llvm::Type* i8x16Type;
		llvm::Type* i16x8Type;
		llvm::Type* i32x4Type;
		llvm::Type* i64x2Type;
		llvm::Type* f32x4Type;
		llvm::Type* f64x2Type;

		llvm::Type* exceptionPointersStructType;

		llvm::Type* anyrefType;

		// Zero constants of each type.
		llvm::Constant* typedZeroConstants[(Uptr)IR::ValueType::num];

		// Maps a type ID to the corresponding LLVM type.
		llvm::Type* valueTypes[Uptr(IR::ValueType::num)];

		LLVMContext();
	};

	// Overloaded functions that compile a literal value to a LLVM constant of the right type.
	inline llvm::ConstantInt* emitLiteral(llvm::LLVMContext& llvmContext, U32 value)
	{
		return llvm::ConstantInt::get(llvmContext, llvm::APInt(32, (U64)value, false));
	}
	inline llvm::ConstantInt* emitLiteral(llvm::LLVMContext& llvmContext, I32 value)
	{
		return llvm::ConstantInt::get(llvmContext, llvm::APInt(32, (I64)value, false));
	}
	inline llvm::ConstantInt* emitLiteral(llvm::LLVMContext& llvmContext, U64 value)
	{
		return llvm::ConstantInt::get(llvmContext, llvm::APInt(64, value, false));
	}
	inline llvm::ConstantInt* emitLiteral(llvm::LLVMContext& llvmContext, I64 value)
	{
		return llvm::ConstantInt::get(llvmContext, llvm::APInt(64, value, false));
	}
	inline llvm::Constant* emitLiteral(llvm::LLVMContext& llvmContext, F32 value)
	{
		return llvm::ConstantFP::get(llvmContext, llvm::APFloat(value));
	}
	inline llvm::Constant* emitLiteral(llvm::LLVMContext& llvmContext, F64 value)
	{
		return llvm::ConstantFP::get(llvmContext, llvm::APFloat(value));
	}
	inline llvm::Constant* emitLiteral(llvm::LLVMContext& llvmContext, bool value)
	{
		return llvm::ConstantInt::get(llvmContext, llvm::APInt(1, value ? 1 : 0, false));
	}
	inline llvm::Constant* emitLiteral(llvm::LLVMContext& llvmContext, V128 value)
	{
		return llvm::ConstantVector::get(
			{llvm::ConstantInt::get(llvmContext, llvm::APInt(64, value.u64[0], false)),
			 llvm::ConstantInt::get(llvmContext, llvm::APInt(64, value.u64[1], false))});
	}
	inline llvm::Constant* emitLiteralPointer(const void* pointer, llvm::Type* type)
	{
		auto pointerInt = llvm::APInt(sizeof(Uptr) == 8 ? 64 : 32, reinterpret_cast<Uptr>(pointer));
		return llvm::Constant::getIntegerValue(type, pointerInt);
	}

	// Converts a WebAssembly type to a LLVM type.
	inline llvm::Type* asLLVMType(LLVMContext& llvmContext, IR::ValueType type)
	{
		wavmAssert(type < IR::ValueType::num);
		return llvmContext.valueTypes[Uptr(type)];
	}

	inline llvm::Type* asLLVMType(LLVMContext& llvmContext, IR::TypeTuple typeTuple)
	{
		llvm::Type** llvmTypes = (llvm::Type**)alloca(sizeof(llvm::Type*) * typeTuple.size());
		for(Uptr typeIndex = 0; typeIndex < typeTuple.size(); ++typeIndex)
		{ llvmTypes[typeIndex] = asLLVMType(llvmContext, typeTuple[typeIndex]); }
		return llvm::StructType::get(llvmContext,
									 llvm::ArrayRef<llvm::Type*>(llvmTypes, typeTuple.size()));
	}

	inline bool areResultsReturnedDirectly(IR::TypeTuple results)
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

	inline llvm::StructType* getLLVMReturnStructType(LLVMContext& llvmContext,
													 IR::TypeTuple results)
	{
		if(areResultsReturnedDirectly(results))
		{
			// A limited number of results can be packed into a struct and returned directly.
			return llvm::StructType::get(llvmContext.i8PtrType, asLLVMType(llvmContext, results));
		}
		else
		{
			// If there are too many results to be returned directly, they will be returned in the
			// context arg/return memory block.
			return llvm::StructType::get(llvmContext.i8PtrType);
		}
	}

	inline llvm::Constant* getZeroedLLVMReturnStruct(LLVMContext& llvmContext,
													 IR::TypeTuple resultType)
	{
		return llvm::Constant::getNullValue(getLLVMReturnStructType(llvmContext, resultType));
	}

	// Converts a WebAssembly function type to a LLVM type.
	inline llvm::FunctionType* asLLVMType(LLVMContext& llvmContext,
										  IR::FunctionType functionType,
										  IR::CallingConvention callingConvention)
	{
		const Uptr numImplicitParameters
			= callingConvention == IR::CallingConvention::intrinsicWithMemAndTable
				  ? 3
				  : callingConvention == IR::CallingConvention::c ? 0 : 1;
		const Uptr numParameters = numImplicitParameters + functionType.params().size();
		auto llvmArgTypes = (llvm::Type**)alloca(sizeof(llvm::Type*) * numParameters);
		if(callingConvention == IR::CallingConvention::intrinsicWithMemAndTable)
		{
			llvmArgTypes[0] = llvmContext.i8PtrType;
			llvmArgTypes[1] = llvmContext.i64Type;
			llvmArgTypes[2] = llvmContext.i64Type;
		}
		else if(callingConvention != IR::CallingConvention::c)
		{
			llvmArgTypes[0] = llvmContext.i8PtrType;
		}
		for(Uptr argIndex = 0; argIndex < functionType.params().size(); ++argIndex)
		{
			llvmArgTypes[argIndex + numImplicitParameters]
				= asLLVMType(llvmContext, functionType.params()[argIndex]);
		}

		llvm::Type* llvmReturnType;
		switch(callingConvention)
		{
		case IR::CallingConvention::wasm:
			llvmReturnType = getLLVMReturnStructType(llvmContext, functionType.results());
			break;

		case IR::CallingConvention::intrinsicWithContextSwitch:
			llvmReturnType = llvmContext.i8PtrType;
			break;

		case IR::CallingConvention::intrinsicWithMemAndTable:
		case IR::CallingConvention::intrinsic:
		case IR::CallingConvention::c:
			switch(functionType.results().size())
			{
			case 0: llvmReturnType = llvm::Type::getVoidTy(llvmContext); break;
			case 1: llvmReturnType = asLLVMType(llvmContext, functionType.results()[0]); break;
			default: Errors::fatal("intrinsics/C functions returning >1 result isn't supported");
			}
			break;

		default: Errors::unreachable();
		};
		return llvm::FunctionType::get(
			llvmReturnType, llvm::ArrayRef<llvm::Type*>(llvmArgTypes, numParameters), false);
	}

	inline llvm::CallingConv::ID asLLVMCallingConv(IR::CallingConvention callingConvention)
	{
		switch(callingConvention)
		{
		case IR::CallingConvention::wasm: return llvm::CallingConv::Fast;

		case IR::CallingConvention::intrinsic:
		case IR::CallingConvention::intrinsicWithContextSwitch:
		case IR::CallingConvention::intrinsicWithMemAndTable:
		case IR::CallingConvention::c: return llvm::CallingConv::C;

		default: Errors::unreachable();
		}
	}

	inline llvm::Constant* getMemoryIdFromOffset(LLVMContext& llvmContext,
												 llvm::Constant* memoryOffset)
	{
		return llvm::ConstantExpr::getExactUDiv(
			llvm::ConstantExpr::getSub(
				memoryOffset,
				emitLiteral(llvmContext,
							Uptr(offsetof(Runtime::CompartmentRuntimeData, memoryBases)))),
			emitLiteral(llvmContext, Uptr(sizeof(Uptr))));
	}

	inline llvm::Constant* getTableIdFromOffset(LLVMContext& llvmContext,
												llvm::Constant* tableOffset)
	{
		return llvm::ConstantExpr::getExactUDiv(
			llvm::ConstantExpr::getSub(
				tableOffset,
				emitLiteral(llvmContext,
							Uptr(offsetof(Runtime::CompartmentRuntimeData, tableBases)))),
			emitLiteral(llvmContext, Uptr(sizeof(Uptr))));
	}

	// Functions that map between the symbols used for externally visible functions and the function
	inline std::string getExternalName(const char* baseName, Uptr index)
	{
		return std::string(baseName) + std::to_string(index);
	}

	// Emits LLVM IR for a module.
	void emitModule(const IR::Module& irModule,
					LLVMContext& llvmContext,
					llvm::Module& outLLVMModule);

	// Used to override LLVM's default behavior of looking up unresolved symbols in DLL exports.
	llvm::JITEvaluatedSymbol resolveJITImport(llvm::StringRef name);

	struct ModuleMemoryManager;

	// Encapsulates a loaded module.
	struct LoadedModule
	{
		std::vector<std::unique_ptr<JITFunction>> functions;
		std::map<Uptr, JITFunction*> addressToFunctionMap;
		HashMap<std::string, JITFunction*> nameToFunctionMap;

		LoadedModule(const std::vector<U8>& objectBytes,
					 const HashMap<std::string, Uptr>& importedSymbolMap,
					 bool shouldLogMetrics);
		~LoadedModule();

	private:
		ModuleMemoryManager* memoryManager;
	};

	extern std::vector<U8> compileLLVMModule(LLVMContext& llvmContext,
											 llvm::Module&& llvmModule,
											 bool shouldLogMetrics);

	extern void processSEHTables(U8* imageBase,
								 const llvm::LoadedObjectInfo& loadedObject,
								 const llvm::object::SectionRef& pdataSection,
								 const U8* pdataCopy,
								 Uptr pdataNumBytes,
								 const llvm::object::SectionRef& xdataSection,
								 const U8* xdataCopy,
								 Uptr sehTrampolineAddress);
}

#pragma once

#include "WAVM/IR/Module.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Runtime/RuntimeData.h"

#include <cctype>
#include <string>
#include <vector>

// Define some macros that can be used to wrap the LLVM includes and disable VC warnings.
#ifdef _MSC_VER
#define PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS                                                     \
	__pragma(warning(push));                                                                       \
	__pragma(warning(disable : 4267));                                                             \
	__pragma(warning(disable : 4800));                                                             \
	__pragma(warning(disable : 4291));                                                             \
	__pragma(warning(disable : 4244));                                                             \
	__pragma(warning(disable : 4351));                                                             \
	__pragma(warning(disable : 4065));                                                             \
	__pragma(warning(disable : 4624));                                                             \
	/* conversion from 'int' to 'unsigned int', signed/unsigned mismatch */                        \
	__pragma(warning(disable : 4245));                                                             \
	/* unary minus operator applied to unsigned type, result is still unsigned */                  \
	__pragma(warning(disable : 4146));                                                             \
	/* declaration of 'x' hides class member */                                                    \
	__pragma(warning(disable : 4458));                                                             \
	/* default constructor could not be generated */                                               \
	__pragma(warning(disable : 4510));                                                             \
	/* struct can never be instantiated - user defined constructor required */                     \
	__pragma(warning(disable : 4610));                                                             \
	/* structure was padded due to alignment specifier */                                          \
	__pragma(warning(disable : 4324));                                                             \
	/* unreachable code */                                                                         \
	__pragma(warning(disable : 4702));

#define POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS __pragma(warning(pop));
#else
#define PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#define POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#endif

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include "llvm/ADT/SmallVector.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/DataTypes.h"
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

#ifdef _WIN32
#define USE_WINDOWS_SEH 1
#else
#define USE_WINDOWS_SEH 0
#endif

namespace llvm {
	class LoadedObjectInfo;

	namespace object {
		class SectionRef;
	}
}

namespace WAVM { namespace LLVMJIT {
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
		llvm::Type* iptrType;

		llvm::PointerType* i8PtrType;

		llvm::Type* i8x16Type;
		llvm::Type* i16x8Type;
		llvm::Type* i32x4Type;
		llvm::Type* i64x2Type;
		llvm::Type* f32x4Type;
		llvm::Type* f64x2Type;

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
	inline llvm::Constant* emitLiteralPointer(const void* pointer, llvm::Type* intOrPointerType)
	{
		auto pointerInt = llvm::APInt(sizeof(Uptr) == 8 ? 64 : 32, reinterpret_cast<Uptr>(pointer));
		return llvm::Constant::getIntegerValue(intOrPointerType, pointerInt);
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
		const Uptr numImplicitParameters = callingConvention == IR::CallingConvention::c ? 0 : 1;
		const Uptr numParameters = numImplicitParameters + functionType.params().size();
		auto llvmArgTypes = (llvm::Type**)alloca(sizeof(llvm::Type*) * numParameters);
		if(callingConvention != IR::CallingConvention::c)
		{ llvmArgTypes[0] = llvmContext.i8PtrType; }
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

	inline void setRuntimeFunctionPrefix(LLVMContext& llvmContext,
										 llvm::Function* function,
										 llvm::Constant* mutableData,
										 llvm::Constant* moduleInstanceId,
										 llvm::Constant* typeId)
	{
		function->setPrefixData(
			llvm::ConstantArray::get(llvm::ArrayType::get(llvmContext.iptrType, 4),
									 {emitLiteral(llvmContext, Uptr(Runtime::ObjectKind::function)),
									  mutableData,
									  moduleInstanceId,
									  typeId}));
		static_assert(offsetof(Runtime::Function, object) == sizeof(Uptr) * 0,
					  "Function prefix must match Runtime::Function layout");
		static_assert(offsetof(Runtime::Function, mutableData) == sizeof(Uptr) * 1,
					  "Function prefix must match Runtime::Function layout");
		static_assert(offsetof(Runtime::Function, moduleInstanceId) == sizeof(Uptr) * 2,
					  "Function prefix must match Runtime::Function layout");
		static_assert(offsetof(Runtime::Function, encodedType) == sizeof(Uptr) * 3,
					  "Function prefix must match Runtime::Function layout");
		static_assert(offsetof(Runtime::Function, code) == sizeof(Uptr) * 4,
					  "Function prefix must match Runtime::Function layout");
	}

	inline void setFramePointerAttribute(llvm::Function* function)
	{
#ifndef _WIN32
		auto attrs = function->getAttributes();
		// LLVM 9+ has a more general purpose frame-pointer=(all|non-leaf|none) attribute that WAVM
		// should use once we can depend on it.
		attrs = attrs.addAttribute(function->getContext(),
								   llvm::AttributeList::FunctionIndex,
								   "no-frame-pointer-elim",
								   "true");
		function->setAttributes(attrs);
#endif
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
	struct Module
	{
		std::map<Uptr, Runtime::Function*> addressToFunctionMap;
		HashMap<std::string, Runtime::Function*> nameToFunctionMap;

		Module(const std::vector<U8>& inObjectBytes,
			   const HashMap<std::string, Uptr>& importedSymbolMap,
			   bool shouldLogMetrics);
		~Module();

	private:
		ModuleMemoryManager* memoryManager;

		// Have to keep copies of these around because until LLVM 8, GDB registration listener uses
		// their pointers as keys for deregistration.
#if LLVM_VERSION_MAJOR < 8
		std::vector<U8> objectBytes;
		std::unique_ptr<llvm::object::ObjectFile> object;
#endif
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
}}

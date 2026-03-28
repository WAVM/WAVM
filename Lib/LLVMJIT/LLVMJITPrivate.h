#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "WAVM/DWARF/Sections.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Platform/Alloca.h"
#include "WAVM/Platform/Unwind.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"

#if defined(_MSC_VER) && defined(__clang__)
#define PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS                                                     \
	_Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wswitch-enum\"")
#define POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
// Disable all VC warnings in the LLVM headers
#define PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS                                                     \
	__pragma(warning(push, 0));                                                                    \
	__pragma(warning(disable : 4702));                                                             \
	__pragma(warning(disable : 4244));
#define POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS __pragma(warning(pop));
#else
#define PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#define POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#endif

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/Alignment.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Target/TargetMachine.h>
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

using FixedVectorType = llvm::FixedVectorType;

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

		llvm::PointerType* ptrType;
		llvm::PointerType* funcptrType;

		FixedVectorType* i8x8Type;
		FixedVectorType* i16x4Type;
		FixedVectorType* i32x2Type;
		FixedVectorType* i64x1Type;
		FixedVectorType* f32x2Type;
		FixedVectorType* f64x1Type;

		FixedVectorType* i8x16Type;
		FixedVectorType* i16x8Type;
		FixedVectorType* i32x4Type;
		FixedVectorType* i64x2Type;
		FixedVectorType* f32x4Type;
		FixedVectorType* f64x2Type;

		FixedVectorType* i8x32Type;
		FixedVectorType* i16x16Type;
		FixedVectorType* i32x8Type;
		FixedVectorType* i64x4Type;

		FixedVectorType* i8x48Type;
		FixedVectorType* i16x24Type;
		FixedVectorType* i32x12Type;
		FixedVectorType* i64x6Type;

		FixedVectorType* i8x64Type;
		FixedVectorType* i16x32Type;
		FixedVectorType* i32x16Type;
		FixedVectorType* i64x8Type;

		llvm::Type* externrefType;

		// Zero constants of each type.
		llvm::Constant* typedZeroConstants[IR::numValueTypes];

		// Maps a type ID to the corresponding LLVM type.
		llvm::Type* valueTypes[IR::numValueTypes];

		LLVMContext();
	};

	// Overloaded functions that compile a literal value to a LLVM constant of the right type.
	inline llvm::ConstantInt* emitLiteral(llvm::LLVMContext& llvmContext, U32 value)
	{
		return llvm::ConstantInt::get(llvmContext, llvm::APInt(32, (U64)value, false));
	}
	inline llvm::ConstantInt* emitLiteral(llvm::LLVMContext& llvmContext, I32 value)
	{
		return llvm::ConstantInt::get(llvmContext, llvm::APInt(32, (I64)value, true));
	}
	inline llvm::ConstantInt* emitLiteral(llvm::LLVMContext& llvmContext, U64 value)
	{
		return llvm::ConstantInt::get(llvmContext, llvm::APInt(64, value, false));
	}
	inline llvm::ConstantInt* emitLiteral(llvm::LLVMContext& llvmContext, I64 value)
	{
		return llvm::ConstantInt::get(llvmContext, llvm::APInt(64, value, true));
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
			{llvm::ConstantInt::get(llvmContext, llvm::APInt(64, value.u64x2[0], false)),
			 llvm::ConstantInt::get(llvmContext, llvm::APInt(64, value.u64x2[1], false))});
	}
	inline llvm::Constant* emitLiteralIptr(U64 iptrValue, llvm::Type* iptrType)
	{
		return llvm::ConstantInt::get(iptrType, iptrValue);
	}

	// Converts a WebAssembly type to a LLVM type.
	inline llvm::Type* asLLVMType(LLVMContext& llvmContext, IR::ValueType type)
	{
		WAVM_ERROR_UNLESS(type < (IR::ValueType)IR::numValueTypes);
		return llvmContext.valueTypes[Uptr(type)];
	}

	inline llvm::Type* asLLVMType(LLVMContext& llvmContext, IR::TypeTuple typeTuple)
	{
		llvm::Type** llvmTypes = (llvm::Type**)alloca(sizeof(llvm::Type*) * typeTuple.size());
		for(Uptr typeIndex = 0; typeIndex < typeTuple.size(); ++typeIndex)
		{
			llvmTypes[typeIndex] = asLLVMType(llvmContext, typeTuple[typeIndex]);
		}
		return llvm::StructType::get(llvmContext,
									 llvm::ArrayRef<llvm::Type*>(llvmTypes, typeTuple.size()));
	}

	inline bool areResultsReturnedDirectly(IR::TypeTuple results)
	{
		// On X64, the calling conventions can return up to 3 i64s and 4 float/vectors.
		// For simplicity, just allow up to 3 values to be returned, including the implicitly
		// returned context pointer.
		static constexpr Uptr maxDirectlyReturnedValues = 3;
		return results.size() + 1 <= maxDirectlyReturnedValues;
	}

	inline llvm::StructType* getLLVMReturnStructType(LLVMContext& llvmContext,
													 IR::TypeTuple results)
	{
		if(areResultsReturnedDirectly(results))
		{
			// A limited number of results can be packed into a struct and returned directly.
			return llvm::StructType::get(llvmContext.ptrType, asLLVMType(llvmContext, results));
		}
		else
		{
			// If there are too many results to be returned directly, they will be returned in the
			// context arg/return memory block.
			return llvm::StructType::get(llvmContext.ptrType);
		}
	}

	inline llvm::Constant* getZeroedLLVMReturnStruct(LLVMContext& llvmContext,
													 IR::TypeTuple resultType)
	{
		return llvm::Constant::getNullValue(getLLVMReturnStructType(llvmContext, resultType));
	}

	// Converts a WebAssembly function type to a LLVM type.
	inline llvm::FunctionType* asLLVMType(LLVMContext& llvmContext, IR::FunctionType functionType)
	{
		const IR::CallingConvention callingConvention = functionType.callingConvention();

		Uptr numParameters;
		llvm::Type** llvmArgTypes;
		if(callingConvention == IR::CallingConvention::cAPICallback)
		{
			numParameters = 2;
			llvmArgTypes = (llvm::Type**)alloca(sizeof(llvm::Type*) * numParameters);
			llvmArgTypes[0] = llvmContext.ptrType;
			llvmArgTypes[1] = llvmContext.ptrType;
		}
		else
		{
			const Uptr numImplicitParameters
				= callingConvention == IR::CallingConvention::c ? 0 : 1;
			numParameters = numImplicitParameters + functionType.params().size();
			llvmArgTypes = (llvm::Type**)alloca(sizeof(llvm::Type*) * numParameters);
			if(callingConvention != IR::CallingConvention::c)
			{
				llvmArgTypes[0] = llvmContext.ptrType;
			}

			for(Uptr argIndex = 0; argIndex < functionType.params().size(); ++argIndex)
			{
				llvmArgTypes[argIndex + numImplicitParameters]
					= asLLVMType(llvmContext, functionType.params()[argIndex]);
			}
		}

		llvm::Type* llvmReturnType;
		switch(callingConvention)
		{
		case IR::CallingConvention::wasm:
			llvmReturnType = getLLVMReturnStructType(llvmContext, functionType.results());
			break;

		case IR::CallingConvention::cAPICallback:
		case IR::CallingConvention::intrinsicWithContextSwitch:
			llvmReturnType = llvmContext.ptrType;
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

		default: WAVM_UNREACHABLE();
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
		case IR::CallingConvention::cAPICallback:
		case IR::CallingConvention::c: return llvm::CallingConv::C;

		default: WAVM_UNREACHABLE();
		}
	}

	inline llvm::Type* getIptrType(LLVMContext& llvmContext, U32 numPointerBytes)
	{
		switch(numPointerBytes)
		{
		case 4: return llvmContext.i32Type;
		case 8: return llvmContext.i64Type;
		default: Errors::fatalf("Unexpected pointer size: %u bytes", numPointerBytes);
		};
	}

	inline void setRuntimeFunctionPrefix(LLVMContext& llvmContext,
										 llvm::Type* iptrType,
										 llvm::Function* function,
										 llvm::Constant* mutableData,
										 llvm::Constant* instanceId,
										 llvm::Constant* typeId)
	{
		function->setPrefixData(
			llvm::ConstantArray::get(llvm::ArrayType::get(iptrType, 4),
									 {emitLiteralIptr(U64(Runtime::ObjectKind::function), iptrType),
									  mutableData,
									  instanceId,
									  typeId}));

		// The prefix contains pointer-sized fields, so the function entry point (and thus the
		// prefix before it) must be aligned to at least alignof(Uptr). On AArch64, LLVM's
		// default function alignment is 4, but we need 8 for the prefix pointers.
		llvm::Align prefixAlign(sizeof(Uptr));
		if(function->getAlign().valueOrOne() < prefixAlign) { function->setAlignment(prefixAlign); }
	}

	inline void setFunctionAttributes(llvm::TargetMachine* targetMachine, llvm::Function* function)
	{
		// For now, no attributes need to be set on Win32.
		if(targetMachine->getTargetTriple().getOS() != llvm::Triple::Win32)
		{
			auto attrs = function->getAttributes();

			// Use frame-pointer=all to ensure frame pointers are always generated.
			// This is needed for reliable stack unwinding during exception handling.
			attrs = attrs.addAttributeAtIndex(
				function->getContext(), llvm::AttributeList::FunctionIndex, "frame-pointer", "all");

			// Enable asynchronous unwind tables so that CFI is correct at every instruction.
			// This is required for signal handling (e.g., stack overflow) where a signal can
			// occur at any instruction, not just at call sites.
			attrs
				= attrs.addAttributeAtIndex(function->getContext(),
											llvm::AttributeList::FunctionIndex,
											llvm::Attribute::getWithUWTableKind(
												function->getContext(), llvm::UWTableKind::Async));

			// Set the probe-stack attribute to ensure functions that allocate more than a page
			// of stack space touch each page in order, preventing the stack from skipping over
			// the guard page.
			attrs = attrs.addAttributeAtIndex(function->getContext(),
											  llvm::AttributeList::FunctionIndex,
											  "probe-stack",
											  "inline-asm");

			function->setAttributes(attrs);
		}
	}

	// Maps a base name and index to an externally visible symbol name.
	inline std::string getExternalName(const char* baseName, Uptr index)
	{
		return std::string(baseName) + std::to_string(index);
	}

	// Emits LLVM IR for a module.
	void emitModule(const IR::Module& irModule,
					LLVMContext& llvmContext,
					llvm::Module& outLLVMModule,
					llvm::TargetMachine* targetMachine);

	// Adds LLVM runtime symbols (memcpy, personality function, etc.) to an import map.
	void addLLVMRuntimeSymbols(HashMap<std::string, Uptr>& importedSymbolMap);

	// GDB JIT interface registration (see GDBRegistration.cpp).
	void* registerObjectWithGDB(const U8* objectBytes, Uptr objectSize);
	void unregisterObjectWithGDB(void* handle);

	struct ModuleMemoryManager;
	struct GlobalModuleState;

	// Encapsulates a loaded module.
	struct Module
	{
		HashMap<std::string, Runtime::Function*> nameToFunctionMap;
		std::map<Uptr, Runtime::Function*> addressToFunctionMap;
		std::string debugName;

		// Object bytes (patched with section addresses on ELF), used for GDB.
		std::vector<U8> objectBytes;

		// DWARF sections (pointers into the loaded image), used for signal-safe source lookup.
		DWARF::Sections dwarfSections = {};

		Module(std::vector<U8> inObjectBytes,
			   const HashMap<std::string, Uptr>& importedSymbolMap,
			   bool shouldLogMetrics,
			   std::string&& inDebugName);
		~Module();

	private:
		ModuleMemoryManager* memoryManager;

		// Module holds a shared pointer to GlobalModuleState to ensure that on exit it is not
		// destructed until after all Modules have been destructed.
		std::shared_ptr<GlobalModuleState> globalModuleState;

		// Opaque handle for deregistering unwind data.
		Platform::UnwindRegistration* unwindRegistration = nullptr;

		void* gdbRegistrationHandle = nullptr;
	};

	extern void initLLVM();
	extern std::string getTriple(const TargetSpec& targetSpec);
	extern std::unique_ptr<llvm::TargetMachine> getTargetMachine(const TargetSpec& targetSpec);
	extern TargetValidationResult validateTargetMachine(
		const std::unique_ptr<llvm::TargetMachine>& targetMachine,
		const IR::FeatureSpec& featureSpec);

	extern std::vector<U8> compileLLVMModule(LLVMContext& llvmContext,
											 llvm::Module&& llvmModule,
											 bool shouldLogMetrics,
											 llvm::TargetMachine* targetMachine);
}}

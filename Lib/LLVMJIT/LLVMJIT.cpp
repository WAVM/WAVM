#include "LLVMJIT/LLVMJIT.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/HashMap.h"
#include "Inline/Lock.h"
#include "Inline/Timing.h"
#include "LLVMJITPrivate.h"
#include "Logging/Logging.h"

#include "LLVMPreInclude.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/TargetSelect.h"

#include "LLVMPostInclude.h"

using namespace IR;
using namespace LLVMJIT;

llvm::LLVMContext* LLVMJIT::llvmContext = nullptr;
llvm::Type* LLVMJIT::llvmValueTypes[(Uptr)ValueType::num];

llvm::Type* LLVMJIT::llvmI8Type;
llvm::Type* LLVMJIT::llvmI16Type;
llvm::Type* LLVMJIT::llvmI32Type;
llvm::Type* LLVMJIT::llvmI64Type;
llvm::Type* LLVMJIT::llvmI128Type;
llvm::Type* LLVMJIT::llvmF32Type;
llvm::Type* LLVMJIT::llvmF64Type;
llvm::Type* LLVMJIT::llvmVoidType;
llvm::Type* LLVMJIT::llvmBoolType;
llvm::Type* LLVMJIT::llvmI8PtrType;
llvm::Type* LLVMJIT::llvmIptrType;

llvm::Type* LLVMJIT::llvmI8x16Type;
llvm::Type* LLVMJIT::llvmI16x8Type;
llvm::Type* LLVMJIT::llvmI32x4Type;
llvm::Type* LLVMJIT::llvmI64x2Type;
llvm::Type* LLVMJIT::llvmI128x1Type;
llvm::Type* LLVMJIT::llvmF32x4Type;
llvm::Type* LLVMJIT::llvmF64x2Type;

llvm::Type* LLVMJIT::llvmExceptionPointersStructType;

llvm::Constant* LLVMJIT::typedZeroConstants[(Uptr)ValueType::num];

Platform::Mutex LLVMJIT::llvmMutex;

static std::map<std::string, const char*> runtimeSymbolMap = {
#ifdef _WIN32
	// the LLVM X86 code generator calls __chkstk when allocating more than 4KB of stack space
	{"__chkstk", "__chkstk"},
	{"__C_specific_handler", "__C_specific_handler"},
#ifndef _WIN64
	{"__aullrem", "_aullrem"},
	{"__allrem", "_allrem"},
	{"__aulldiv", "_aulldiv"},
	{"__alldiv", "_alldiv"},
#endif
#else
	{"__CxxFrameHandler3", "__CxxFrameHandler3"},
	{"__cxa_begin_catch", "__cxa_begin_catch"},
	{"__gxx_personality_v0", "__gxx_personality_v0"},
#endif
#ifdef __arm__
	{"__aeabi_uidiv", "__aeabi_uidiv"},
	{"__aeabi_idiv", "__aeabi_idiv"},
	{"__aeabi_idivmod", "__aeabi_idivmod"},
	{"__aeabi_uldiv", "__aeabi_uldiv"},
	{"__aeabi_uldivmod", "__aeabi_uldivmod"},
	{"__aeabi_unwind_cpp_pr0", "__aeabi_unwind_cpp_pr0"},
	{"__aeabi_unwind_cpp_pr1", "__aeabi_unwind_cpp_pr1"},
#endif
};

llvm::JITEvaluatedSymbol LLVMJIT::resolveJITImport(llvm::StringRef name)
{
	// Allow some intrinsics used by LLVM
	auto runtimeSymbolNameIt = runtimeSymbolMap.find(name);
	if(runtimeSymbolNameIt == runtimeSymbolMap.end()) { return llvm::JITEvaluatedSymbol(nullptr); }

	const char* lookupName = runtimeSymbolNameIt->second;
	void* addr             = llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(lookupName);
	if(!addr)
	{
		Errors::fatalf("LLVM generated code references undefined external symbol: %s\n",
					   lookupName);
	}
	return llvm::JITEvaluatedSymbol(reinterpret_cast<Uptr>(addr), llvm::JITSymbolFlags::None);
}

void LLVMJIT::initLLVM()
{
	if(llvmContext) { return; }
	llvmContext = new llvm::LLVMContext();

	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetDisassembler();
	llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

	llvmI8Type    = llvm::Type::getInt8Ty(*llvmContext);
	llvmI16Type   = llvm::Type::getInt16Ty(*llvmContext);
	llvmI32Type   = llvm::Type::getInt32Ty(*llvmContext);
	llvmI64Type   = llvm::Type::getInt64Ty(*llvmContext);
	llvmI128Type  = llvm::Type::getInt128Ty(*llvmContext);
	llvmF32Type   = llvm::Type::getFloatTy(*llvmContext);
	llvmF64Type   = llvm::Type::getDoubleTy(*llvmContext);
	llvmVoidType  = llvm::Type::getVoidTy(*llvmContext);
	llvmBoolType  = llvm::Type::getInt1Ty(*llvmContext);
	llvmI8PtrType = llvmI8Type->getPointerTo();
	switch(sizeof(Uptr))
	{
	case 4: llvmIptrType = llvmI32Type; break;
	case 8: llvmIptrType = llvmI64Type; break;
	default: Errors::unreachable();
	}

	auto llvmExceptionRecordStructType = llvm::StructType::create({
		llvmI32Type,   // DWORD ExceptionCode
		llvmI32Type,   // DWORD ExceptionFlags
		llvmI8PtrType, // _EXCEPTION_RECORD* ExceptionRecord
		llvmI8PtrType, // PVOID ExceptionAddress
		llvmI32Type,   // DWORD NumParameters
		llvm::ArrayType::get(llvmI64Type,
							 15) // ULONG_PTR ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS]
	});
	llvmExceptionPointersStructType
		= llvm::StructType::create({llvmExceptionRecordStructType->getPointerTo(), llvmI8PtrType});

	llvmI8x16Type  = llvm::VectorType::get(llvmI8Type, 16);
	llvmI16x8Type  = llvm::VectorType::get(llvmI16Type, 8);
	llvmI32x4Type  = llvm::VectorType::get(llvmI32Type, 4);
	llvmI64x2Type  = llvm::VectorType::get(llvmI64Type, 2);
	llvmI128x1Type = llvm::VectorType::get(llvmI128Type, 1);
	llvmF32x4Type  = llvm::VectorType::get(llvmF32Type, 4);
	llvmF64x2Type  = llvm::VectorType::get(llvmF64Type, 2);

	llvmValueTypes[(Uptr)ValueType::i32]  = llvmI32Type;
	llvmValueTypes[(Uptr)ValueType::i64]  = llvmI64Type;
	llvmValueTypes[(Uptr)ValueType::f32]  = llvmF32Type;
	llvmValueTypes[(Uptr)ValueType::f64]  = llvmF64Type;
	llvmValueTypes[(Uptr)ValueType::v128] = llvmI128x1Type;

	// Create zero constants of each type.
	typedZeroConstants[(Uptr)ValueType::any] = nullptr;
	typedZeroConstants[(Uptr)ValueType::i32] = emitLiteral((U32)0);
	typedZeroConstants[(Uptr)ValueType::i64] = emitLiteral((U64)0);
	typedZeroConstants[(Uptr)ValueType::f32] = emitLiteral((F32)0.0f);
	typedZeroConstants[(Uptr)ValueType::f64] = emitLiteral((F64)0.0);

	U64 i64x2Zero[2]                          = {0, 0};
	typedZeroConstants[(Uptr)ValueType::v128] = llvm::ConstantVector::get(
		{llvm::ConstantInt::get(llvmI128Type, llvm::APInt(128, 2, i64x2Zero))});
}

namespace LLVMJIT
{
	LLVMJIT_API void deinit()
	{
		Lock<Platform::Mutex> llvmLock(llvmMutex);

		if(llvmContext)
		{
			delete llvmContext;
			llvmContext = nullptr;

			llvmI8Type = llvmI16Type = llvmI32Type = llvmI64Type = nullptr;
			llvmF32Type = llvmF64Type = nullptr;
			llvmVoidType = llvmBoolType = llvmI8PtrType = llvmIptrType = nullptr;
			llvmExceptionPointersStructType                            = nullptr;
			llvmI8x16Type = llvmI16x8Type = llvmI32x4Type = llvmI64x2Type = nullptr;

			memset(llvmValueTypes, 0, sizeof(llvmValueTypes));
			memset(typedZeroConstants, 0, sizeof(typedZeroConstants));
		}
	}
}

#include <utility>

#include "LLVMJITPrivate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/TargetSelect.h"
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

namespace llvm {
	class Constant;
}

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::LLVMJIT;

static HashMap<std::string, const char*> runtimeSymbolMap = {
#ifdef _WIN32
	// the LLVM X86 code generator calls __chkstk when allocating more than 4KB of stack space
	{"__chkstk", "__chkstk"},
	{"__CxxFrameHandler3", "__CxxFrameHandler3"},
#ifndef _WIN64
	{"__aullrem", "_aullrem"},
	{"__allrem", "_allrem"},
	{"__aulldiv", "_aulldiv"},
	{"__alldiv", "_alldiv"},
#endif
#else
	{"__cxa_begin_catch", "__cxa_begin_catch"},
	{"__cxa_end_catch", "__cxa_end_catch"},
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
	const char* const* runtimeSymbolName = runtimeSymbolMap.get(name.str());
	if(!runtimeSymbolName) { return llvm::JITEvaluatedSymbol(nullptr); }

	void* addr = llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(*runtimeSymbolName);
	if(!addr)
	{
		Errors::fatalf("LLVM generated code references undefined external symbol: %s",
					   *runtimeSymbolName);
	}
	return llvm::JITEvaluatedSymbol(reinterpret_cast<Uptr>(addr), llvm::JITSymbolFlags::None);
}

static bool globalInitLLVM()
{
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetDisassembler();
	llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
	return true;
}

LLVMContext::LLVMContext()
{
	static bool isLLVMInitialized = globalInitLLVM();
	wavmAssert(isLLVMInitialized);

	i8Type = llvm::Type::getInt8Ty(*this);
	i16Type = llvm::Type::getInt16Ty(*this);
	i32Type = llvm::Type::getInt32Ty(*this);
	i64Type = llvm::Type::getInt64Ty(*this);
	f32Type = llvm::Type::getFloatTy(*this);
	f64Type = llvm::Type::getDoubleTy(*this);
	i8PtrType = i8Type->getPointerTo();
	switch(sizeof(Uptr))
	{
	case 4: iptrType = i32Type; break;
	case 8: iptrType = i64Type; break;
	default: Errors::unreachable();
	}

	anyrefType = llvm::StructType::create("Object", i8Type)->getPointerTo();

	i8x16Type = llvm::VectorType::get(i8Type, 16);
	i16x8Type = llvm::VectorType::get(i16Type, 8);
	i32x4Type = llvm::VectorType::get(i32Type, 4);
	i64x2Type = llvm::VectorType::get(i64Type, 2);
	f32x4Type = llvm::VectorType::get(f32Type, 4);
	f64x2Type = llvm::VectorType::get(f64Type, 2);

	valueTypes[(Uptr)ValueType::none] = valueTypes[(Uptr)ValueType::any]
		= valueTypes[(Uptr)ValueType::nullref] = nullptr;
	valueTypes[(Uptr)ValueType::i32] = i32Type;
	valueTypes[(Uptr)ValueType::i64] = i64Type;
	valueTypes[(Uptr)ValueType::f32] = f32Type;
	valueTypes[(Uptr)ValueType::f64] = f64Type;
	valueTypes[(Uptr)ValueType::v128] = i64x2Type;
	valueTypes[(Uptr)ValueType::anyref] = anyrefType;
	valueTypes[(Uptr)ValueType::funcref] = anyrefType;

	// Create zero constants of each type.
	typedZeroConstants[(Uptr)ValueType::none] = nullptr;
	typedZeroConstants[(Uptr)ValueType::any] = nullptr;
	typedZeroConstants[(Uptr)ValueType::i32] = emitLiteral(*this, (U32)0);
	typedZeroConstants[(Uptr)ValueType::i64] = emitLiteral(*this, (U64)0);
	typedZeroConstants[(Uptr)ValueType::f32] = emitLiteral(*this, (F32)0.0f);
	typedZeroConstants[(Uptr)ValueType::f64] = emitLiteral(*this, (F64)0.0);
	typedZeroConstants[(Uptr)ValueType::v128] = emitLiteral(*this, V128());
	typedZeroConstants[(Uptr)ValueType::anyref] = typedZeroConstants[(Uptr)ValueType::funcref]
		= typedZeroConstants[(Uptr)ValueType::nullref] = llvm::Constant::getNullValue(anyrefType);
}

#include "WAVM/LLVMJIT/LLVMJIT.h"
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include "LLVMJITPrivate.h"
#include "WAVM/IR/FeatureSpec.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Platform/CPU.h"

#ifndef _WIN32
#include "WAVM/Platform/Unwind.h"
#endif

PUSH_DISABLE_WARNINGS_FOR_LLVM_HEADERS
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/CodeGen/TargetSubtargetInfo.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/MC/MCTargetOptions.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
POP_DISABLE_WARNINGS_FOR_LLVM_HEADERS

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::LLVMJIT;

namespace LLVMRuntimeSymbols {
#ifdef _WIN32
	// the LLVM X86 code generator calls __chkstk when allocating more than 4KB of stack space
	extern "C" void __chkstk();
	extern "C" void __CxxFrameHandler3();
	// the LLVM X86 code generator references _fltused when floating-point code is present
	extern "C" int _fltused;
#else
#if defined(__APPLE__) && (defined(__i386__) || defined(__x86_64__))
	// LLVM's memset intrinsic lowers to __bzero on x86 Darwin.
	extern "C" void __bzero();
#endif
#if defined(__APPLE__) && defined(__aarch64__)
	// LLVM's memset intrinsic lowers to bzero on AArch64 Darwin.
	extern "C" void bzero(void*, size_t);
#endif
	extern "C" void* __cxa_begin_catch(void*) throw();
	extern "C" void __cxa_end_catch();
#endif

	static HashMap<std::string, void*> map = {
		{"memcpy", (void*)&memcpy},
		{"memmove", (void*)&memmove},
		{"memset", (void*)&memset},
#ifdef _WIN32
		{"__chkstk", (void*)&__chkstk},
		{"__CxxFrameHandler3", (void*)&__CxxFrameHandler3},
		{"_fltused", (void*)&_fltused},
#else
#if defined(__APPLE__) && (defined(__i386__) || defined(__x86_64__))
		{"__bzero", (void*)&__bzero},
#endif
#if defined(__APPLE__) && defined(__aarch64__)
		{"bzero", (void*)&bzero},
#endif
		{"__gxx_personality_v0", (void*)&Platform::DebugPersonalityFunction},
		{"__cxa_begin_catch", (void*)&__cxa_begin_catch},
		{"__cxa_end_catch", (void*)&__cxa_end_catch},
#endif
	};
}

void LLVMJIT::addLLVMRuntimeSymbols(HashMap<std::string, Uptr>& importedSymbolMap)
{
	for(const auto& pair : LLVMRuntimeSymbols::map)
	{
		if(!importedSymbolMap.contains(pair.key))
		{
			importedSymbolMap.add(pair.key, reinterpret_cast<Uptr>(pair.value));
		}
	}
}

static bool globalInitLLVM()
{
	// Disable shrink-wrapping to work around LLVM CFI generation issues.
	// When shrink-wrapping moves frame setup to a cold path, the CFI at the return address
	// after a non-returning call may describe the wrong stack state, causing unwind failures.
	const char* argv[] = {"wavm", "-enable-shrink-wrap=false"};
	llvm::cl::ParseCommandLineOptions(2, argv, "", nullptr, nullptr, false);

	llvm::InitializeAllTargetInfos();
	llvm::InitializeAllTargets();
	llvm::InitializeAllTargetMCs();
	llvm::InitializeAllAsmPrinters();
	llvm::InitializeAllAsmParsers();
	llvm::InitializeAllDisassemblers();
	llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
	return true;
}

static void globalInitLLVMOnce()
{
	static bool isLLVMInitialized = globalInitLLVM();
	WAVM_ASSERT(isLLVMInitialized);
}

void LLVMJIT::initLLVM() { globalInitLLVMOnce(); }

LLVMContext::LLVMContext()
{
	globalInitLLVMOnce();

	i8Type = llvm::Type::getInt8Ty(*this);
	i16Type = llvm::Type::getInt16Ty(*this);
	i32Type = llvm::Type::getInt32Ty(*this);
	i64Type = llvm::Type::getInt64Ty(*this);
	f32Type = llvm::Type::getFloatTy(*this);
	f64Type = llvm::Type::getDoubleTy(*this);

	ptrType = llvm::PointerType::get(*this, 0);
	externrefType = ptrType;
	funcptrType = ptrType;

	i8x8Type = FixedVectorType::get(i8Type, 8);
	i16x4Type = FixedVectorType::get(i16Type, 4);
	i32x2Type = FixedVectorType::get(i32Type, 2);
	i64x1Type = FixedVectorType::get(i64Type, 1);
	f32x2Type = FixedVectorType::get(f32Type, 2);
	f64x1Type = FixedVectorType::get(f64Type, 1);

	i8x16Type = FixedVectorType::get(i8Type, 16);
	i16x8Type = FixedVectorType::get(i16Type, 8);
	i32x4Type = FixedVectorType::get(i32Type, 4);
	i64x2Type = FixedVectorType::get(i64Type, 2);
	f32x4Type = FixedVectorType::get(f32Type, 4);
	f64x2Type = FixedVectorType::get(f64Type, 2);

	i8x32Type = FixedVectorType::get(i8Type, 32);
	i16x16Type = FixedVectorType::get(i16Type, 16);
	i32x8Type = FixedVectorType::get(i32Type, 8);
	i64x4Type = FixedVectorType::get(i64Type, 4);

	i8x48Type = FixedVectorType::get(i8Type, 48);
	i16x24Type = FixedVectorType::get(i16Type, 24);
	i32x12Type = FixedVectorType::get(i32Type, 12);
	i64x6Type = FixedVectorType::get(i64Type, 6);

	i8x64Type = FixedVectorType::get(i8Type, 64);
	i16x32Type = FixedVectorType::get(i16Type, 32);
	i32x16Type = FixedVectorType::get(i32Type, 16);
	i64x8Type = FixedVectorType::get(i64Type, 8);

	valueTypes[(Uptr)ValueType::none] = valueTypes[(Uptr)ValueType::any] = nullptr;
	valueTypes[(Uptr)ValueType::i32] = i32Type;
	valueTypes[(Uptr)ValueType::i64] = i64Type;
	valueTypes[(Uptr)ValueType::f32] = f32Type;
	valueTypes[(Uptr)ValueType::f64] = f64Type;
	valueTypes[(Uptr)ValueType::v128] = i64x2Type;
	valueTypes[(Uptr)ValueType::externref] = externrefType;
	valueTypes[(Uptr)ValueType::funcref] = funcptrType;

	// Create zero constants of each type.
	typedZeroConstants[(Uptr)ValueType::none] = nullptr;
	typedZeroConstants[(Uptr)ValueType::any] = nullptr;
	typedZeroConstants[(Uptr)ValueType::i32] = emitLiteral(*this, (U32)0);
	typedZeroConstants[(Uptr)ValueType::i64] = emitLiteral(*this, (U64)0);
	typedZeroConstants[(Uptr)ValueType::f32] = emitLiteral(*this, (F32)0.0f);
	typedZeroConstants[(Uptr)ValueType::f64] = emitLiteral(*this, (F64)0.0);
	typedZeroConstants[(Uptr)ValueType::v128] = emitLiteral(*this, V128());
	typedZeroConstants[(Uptr)ValueType::externref] = llvm::Constant::getNullValue(externrefType);
	typedZeroConstants[(Uptr)ValueType::funcref] = llvm::Constant::getNullValue(funcptrType);
}

static const char* archToString(TargetArch arch)
{
	switch(arch)
	{
	case TargetArch::x86_64: return "x86_64";
	case TargetArch::aarch64: return "aarch64";
	default: WAVM_UNREACHABLE();
	}
}

static const char* osToString(TargetOS os)
{
	switch(os)
	{
	case TargetOS::linux_: return "linux";
	case TargetOS::macos: return "macos";
	case TargetOS::windows: return "windows";
	default: WAVM_UNREACHABLE();
	}
}

std::string LLVMJIT::asString(const TargetSpec& targetSpec)
{
	std::string result = std::string("arch=") + archToString(targetSpec.arch)
						 + " os=" + osToString(targetSpec.os) + " cpu=" + targetSpec.cpu;

	std::string featuresStr;
	switch(targetSpec.arch)
	{
	case TargetArch::x86_64: featuresStr = asString(targetSpec.x86FeatureOverrides); break;
	case TargetArch::aarch64: featuresStr = asString(targetSpec.aarch64FeatureOverrides); break;
	default: WAVM_UNREACHABLE();
	}

	if(!featuresStr.empty()) { result += ' ' + featuresStr; }

	return result;
}

std::string LLVMJIT::getTriple(const TargetSpec& targetSpec)
{
	switch(targetSpec.arch)
	{
	case TargetArch::x86_64:
		switch(targetSpec.os)
		{
		case TargetOS::linux_: return "x86_64-unknown-linux-gnu";
		case TargetOS::macos: return "x86_64-apple-darwin";
		case TargetOS::windows: return "x86_64-pc-windows-msvc";
		default: WAVM_UNREACHABLE();
		}
	case TargetArch::aarch64:
		switch(targetSpec.os)
		{
		case TargetOS::linux_: return "aarch64-unknown-linux-gnu";
		case TargetOS::macos: return "aarch64-apple-darwin";
		case TargetOS::windows: return "aarch64-pc-windows-msvc";
		default: WAVM_UNREACHABLE();
		}
	default: WAVM_UNREACHABLE();
	}
}

TargetSpec LLVMJIT::getHostTargetSpec()
{
	TargetSpec result;
	result.cpu = std::string(llvm::sys::getHostCPUName());

	auto hostFeatures = Platform::getHostCPUFeatures();
	auto toOptional = [](bool b) { return std::make_optional(b); };

#if WAVM_CPU_ARCH_X86
	result.arch = TargetArch::x86_64;
	result.x86FeatureOverrides = hostFeatures.mapFeatures(toOptional);
#elif WAVM_CPU_ARCH_ARM
	result.arch = TargetArch::aarch64;
	result.aarch64FeatureOverrides = hostFeatures.mapFeatures(toOptional);
#endif

#if defined(__linux__)
	result.os = TargetOS::linux_;
#elif defined(__APPLE__)
	result.os = TargetOS::macos;
#elif defined(_WIN32)
	result.os = TargetOS::windows;
#endif

	return result;
}

std::unique_ptr<llvm::TargetMachine> LLVMJIT::getTargetMachine(const TargetSpec& targetSpec)
{
	globalInitLLVMOnce();

	std::string tripleStr = LLVMJIT::getTriple(targetSpec);
	llvm::Triple triple(tripleStr);
	llvm::SmallVector<std::string, 1> targetAttributes;

	auto addFeatureOverrides = [&](const char* name, const std::optional<bool>& value) {
		if(value.has_value())
		{
			std::string attr = (*value ? "+" : "-");
			attr += name;
			targetAttributes.push_back(std::move(attr));
		}
	};

	switch(targetSpec.arch)
	{
	case TargetArch::x86_64:
		targetSpec.x86FeatureOverrides.forEachFeature(addFeatureOverrides);
		break;
	case TargetArch::aarch64:
		targetSpec.aarch64FeatureOverrides.forEachFeature(addFeatureOverrides);
		break;
	default: WAVM_UNREACHABLE();
	}

	llvm::TargetOptions targetOptions;

	// On Darwin/arm64, LLVM defaults to generating compact-unwind instead of DWARF eh_frame.
	// However, __register_frame only works with DWARF eh_frame, so we need to force LLVM to
	// always emit DWARF unwind info for JIT code to enable C++ exception unwinding.
	// See: https://github.com/llvm/llvm-project/issues/52921
	if(triple.isOSDarwin() && triple.getArch() == llvm::Triple::aarch64)
	{
		targetOptions.MCOptions.EmitDwarfUnwind = llvm::EmitDwarfUnwindType::Always;
	}

	return std::unique_ptr<llvm::TargetMachine>(
		llvm::EngineBuilder()
			.setTargetOptions(targetOptions)
			.selectTarget(triple, "", targetSpec.cpu, targetAttributes));
}

TargetValidationResult LLVMJIT::validateTargetMachine(
	const std::unique_ptr<llvm::TargetMachine>& targetMachine,
	const FeatureSpec& featureSpec)
{
	const llvm::Triple::ArchType targetArch = targetMachine->getTargetTriple().getArch();
	if(targetArch == llvm::Triple::x86_64)
	{
		// If the SIMD feature is enabled, then require the SSE4.1 CPU feature.
		if(featureSpec.simd && !targetMachine->getMCSubtargetInfo()->checkFeatures("+sse4.1"))
		{
			return TargetValidationResult::x86CPUDoesNotSupportSSE41;
		}

		return TargetValidationResult::valid;
	}
	else if(targetArch == llvm::Triple::aarch64)
	{
		if(featureSpec.simd && !targetMachine->getMCSubtargetInfo()->checkFeatures("+neon"))
		{
			return TargetValidationResult::wavmDoesNotSupportSIMDOnArch;
		}

		return TargetValidationResult::valid;
	}
	else
	{
		if(featureSpec.simd) { return TargetValidationResult::wavmDoesNotSupportSIMDOnArch; }
		if(featureSpec.memory64) { return TargetValidationResult::memory64Requires64bitTarget; }
		if(featureSpec.table64) { return TargetValidationResult::table64Requires64bitTarget; }
		return TargetValidationResult::unsupportedArchitecture;
	}
}

TargetValidationResult LLVMJIT::validateTarget(const TargetSpec& targetSpec,
											   const IR::FeatureSpec& featureSpec)
{
	std::unique_ptr<llvm::TargetMachine> targetMachine = getTargetMachine(targetSpec);
	if(!targetMachine) { return TargetValidationResult::invalidTargetSpec; }
	return validateTargetMachine(targetMachine, featureSpec);
}

Version LLVMJIT::getVersion()
{
	return Version{LLVM_VERSION_MAJOR, LLVM_VERSION_MINOR, LLVM_VERSION_PATCH, 7};
}

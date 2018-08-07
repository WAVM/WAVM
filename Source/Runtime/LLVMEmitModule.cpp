#include "IR/OperatorPrinter.h"
#include "IR/Operators.h"
#include "Inline/Assert.h"
#include "Inline/Timing.h"
#include "LLVMEmitFunctionContext.h"
#include "LLVMJIT.h"

using namespace LLVMJIT;
using namespace IR;

EmitModuleContext::EmitModuleContext(const Module& inModule, ModuleInstance* inModuleInstance)
: module(inModule)
, moduleInstance(inModuleInstance)
{
	llvmModuleSharedPtr = std::make_shared<llvm::Module>("", *llvmContext);
	llvmModule          = llvmModuleSharedPtr.get();
	diBuilder           = llvm::make_unique<llvm::DIBuilder>(*llvmModule);

	diModuleScope = diBuilder->createFile("unknown", "unknown");
	diCompileUnit = diBuilder->createCompileUnit(0xffff, diModuleScope, "WAVM", true, "", 0);

	diValueTypes[(Uptr)ValueType::any] = nullptr;
	diValueTypes[(Uptr)ValueType::i32]
		= diBuilder->createBasicType("i32", 32, llvm::dwarf::DW_ATE_signed);
	diValueTypes[(Uptr)ValueType::i64]
		= diBuilder->createBasicType("i64", 64, llvm::dwarf::DW_ATE_signed);
	diValueTypes[(Uptr)ValueType::f32]
		= diBuilder->createBasicType("f32", 32, llvm::dwarf::DW_ATE_float);
	diValueTypes[(Uptr)ValueType::f64]
		= diBuilder->createBasicType("f64", 64, llvm::dwarf::DW_ATE_float);
	diValueTypes[(Uptr)ValueType::v128]
		= diBuilder->createBasicType("v128", 128, llvm::dwarf::DW_ATE_signed);

	auto zeroAsMetadata      = llvm::ConstantAsMetadata::get(emitLiteral(I32(0)));
	auto i32MaxAsMetadata    = llvm::ConstantAsMetadata::get(emitLiteral(I32(INT32_MAX)));
	likelyFalseBranchWeights = llvm::MDTuple::getDistinct(
		*llvmContext,
		{llvm::MDString::get(*llvmContext, "branch_weights"), zeroAsMetadata, i32MaxAsMetadata});
	likelyTrueBranchWeights = llvm::MDTuple::getDistinct(
		*llvmContext,
		{llvm::MDString::get(*llvmContext, "branch_weights"), i32MaxAsMetadata, zeroAsMetadata});

	fpRoundingModeMetadata = llvm::MetadataAsValue::get(
		*llvmContext, llvm::MDString::get(*llvmContext, "round.tonearest"));
	fpExceptionMetadata = llvm::MetadataAsValue::get(
		*llvmContext, llvm::MDString::get(*llvmContext, "fpexcept.strict"));

#ifdef _WIN32
	tryPrologueDummyFunction = nullptr;
#else
	cxaBeginCatchFunction = llvm::Function::Create(
		llvm::FunctionType::get(llvmI8PtrType, {llvmI8PtrType}, false),
		llvm::GlobalValue::LinkageTypes::ExternalLinkage,
		"__cxa_begin_catch",
		llvmModule);
#endif
}

std::shared_ptr<llvm::Module> EmitModuleContext::emit()
{
	Timing::Timer emitTimer;

	// Create an external reference to the appropriate exception personality function.
	auto personalityFunction = llvm::Function::Create(
		llvm::FunctionType::get(llvmI32Type, {}, false),
		llvm::GlobalValue::LinkageTypes::ExternalLinkage,
#ifdef _WIN32
		"__C_specific_handler",
#else
		"__gxx_personality_v0",
#endif
		llvmModule);

	// Create the LLVM functions.
	functionDefs.resize(module.functions.defs.size());
	for(Uptr functionDefIndex = 0; functionDefIndex < module.functions.defs.size();
		++functionDefIndex)
	{
		FunctionType functionType
			= module.types[module.functions.defs[functionDefIndex].type.index];
		auto llvmFunctionType          = asLLVMType(functionType, CallingConvention::wasm);
		auto externalName              = getExternalFunctionName(moduleInstance, functionDefIndex);
		functionDefs[functionDefIndex] = llvm::Function::Create(
			llvmFunctionType, llvm::Function::ExternalLinkage, externalName, llvmModule);
		functionDefs[functionDefIndex]->setPersonalityFn(personalityFunction);
		functionDefs[functionDefIndex]->setCallingConv(asLLVMCallingConv(CallingConvention::wasm));
	}

	// Compile each function in the module.
	for(Uptr functionDefIndex = 0; functionDefIndex < module.functions.defs.size();
		++functionDefIndex)
	{
		EmitFunctionContext(
			*this,
			module,
			module.functions.defs[functionDefIndex],
			moduleInstance->functionDefs[functionDefIndex],
			functionDefs[functionDefIndex])
			.emit();
	}

	// Finalize the debug info.
	diBuilder->finalize();

	Timing::logRatePerSecond("Emitted LLVM IR", emitTimer, (F64)llvmModule->size(), "functions");

	return llvmModuleSharedPtr;
}

std::shared_ptr<llvm::Module> LLVMJIT::emitModule(
	const Module& module,
	ModuleInstance* moduleInstance)
{
	return EmitModuleContext(module, moduleInstance).emit();
}
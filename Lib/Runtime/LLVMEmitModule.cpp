#include "IR/OperatorPrinter.h"
#include "IR/Operators.h"
#include "Inline/Assert.h"
#include "Inline/Timing.h"
#include "LLVMEmitFunctionContext.h"
#include "LLVMJIT.h"

using namespace LLVMJIT;
using namespace IR;

EmitModuleContext::EmitModuleContext(const Module& inModule,
									 ModuleInstance* inModuleInstance,
									 llvm::Module* inLLVMModule)
: module(inModule)
, moduleInstance(inModuleInstance)
, llvmModule(inLLVMModule)
, diBuilder(*inLLVMModule)
{
	diModuleScope = diBuilder.createFile("unknown", "unknown");
	diCompileUnit = diBuilder.createCompileUnit(0xffff, diModuleScope, "WAVM", true, "", 0);

	diValueTypes[(Uptr)ValueType::any] = nullptr;
	diValueTypes[(Uptr)ValueType::i32]
		= diBuilder.createBasicType("i32", 32, llvm::dwarf::DW_ATE_signed);
	diValueTypes[(Uptr)ValueType::i64]
		= diBuilder.createBasicType("i64", 64, llvm::dwarf::DW_ATE_signed);
	diValueTypes[(Uptr)ValueType::f32]
		= diBuilder.createBasicType("f32", 32, llvm::dwarf::DW_ATE_float);
	diValueTypes[(Uptr)ValueType::f64]
		= diBuilder.createBasicType("f64", 64, llvm::dwarf::DW_ATE_float);
	diValueTypes[(Uptr)ValueType::v128]
		= diBuilder.createBasicType("v128", 128, llvm::dwarf::DW_ATE_signed);

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

	tryPrologueDummyFunction = nullptr;
	cxaBeginCatchFunction    = nullptr;
	if(!USE_WINDOWS_SEH)
	{
		cxaBeginCatchFunction
			= llvm::Function::Create(llvm::FunctionType::get(llvmI8PtrType, {llvmI8PtrType}, false),
									 llvm::GlobalValue::LinkageTypes::ExternalLinkage,
									 "__cxa_begin_catch",
									 llvmModule);
	}
}

void LLVMJIT::emitModule(const Module& module,
						 ModuleInstance* moduleInstance,
						 llvm::Module& outLLVMModule)
{
	Timing::Timer emitTimer;
	EmitModuleContext moduleContext(module, moduleInstance, &outLLVMModule);

	// Create an external reference to the appropriate exception personality function.
	auto personalityFunction
		= llvm::Function::Create(llvm::FunctionType::get(llvmI32Type, {}, false),
								 llvm::GlobalValue::LinkageTypes::ExternalLinkage,
#ifdef _WIN32
								 "__C_specific_handler",
#else
								 "__gxx_personality_v0",
#endif
								 &outLLVMModule);

	// Create the LLVM functions.
	moduleContext.functionDefs.resize(module.functions.defs.size());
	for(Uptr functionDefIndex = 0; functionDefIndex < module.functions.defs.size();
		++functionDefIndex)
	{
		FunctionType functionType
			= module.types[module.functions.defs[functionDefIndex].type.index];
		auto llvmFunctionType = asLLVMType(functionType, CallingConvention::wasm);
		auto externalName     = getExternalFunctionName(moduleInstance, functionDefIndex);
		auto llvmFunction     = llvm::Function::Create(
            llvmFunctionType, llvm::Function::ExternalLinkage, externalName, &outLLVMModule);
		llvmFunction->setPersonalityFn(personalityFunction);
		llvmFunction->setCallingConv(asLLVMCallingConv(CallingConvention::wasm));
		moduleContext.functionDefs[functionDefIndex] = llvmFunction;
	}

	// Compile each function in the module.
	for(Uptr functionDefIndex = 0; functionDefIndex < module.functions.defs.size();
		++functionDefIndex)
	{
		EmitFunctionContext(moduleContext,
							module,
							module.functions.defs[functionDefIndex],
							moduleInstance->functionDefs[functionDefIndex],
							moduleContext.functionDefs[functionDefIndex])
			.emit();
	}

	// Finalize the debug info.
	moduleContext.diBuilder.finalize();

	Timing::logRatePerSecond("Emitted LLVM IR", emitTimer, (F64)outLLVMModule.size(), "functions");
}

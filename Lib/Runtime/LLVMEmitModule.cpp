#include "IR/OperatorPrinter.h"
#include "IR/Operators.h"
#include "Inline/Assert.h"
#include "Inline/Timing.h"
#include "LLVMEmitFunctionContext.h"
#include "LLVMJIT.h"

using namespace IR;
using namespace LLVMJIT;
using namespace Runtime;

EmitModuleContext::EmitModuleContext(const IR::Module& inIRModule, llvm::Module* inLLVMModule)
: irModule(inIRModule)
, llvmModule(inLLVMModule)
, defaultMemoryOffset(nullptr)
, defaultTableOffset(nullptr)
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

static llvm::Constant* createImportedConstant(llvm::Module& llvmModule, llvm::Twine externalName)
{
	return new llvm::GlobalVariable(llvmModule,
									llvmI8Type,
									false,
									llvm::GlobalVariable::ExternalLinkage,
									nullptr,
									externalName);
}

void LLVMJIT::emitModule(const IR::Module& irModule, llvm::Module& outLLVMModule)
{
	Timing::Timer emitTimer;
	EmitModuleContext moduleContext(irModule, &outLLVMModule);

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

	// Create LLVM external globals corresponding to offsets to table base pointers in
	// CompartmentRuntimeData for the module's declared table objects.
	for(Uptr tableIndex = 0; tableIndex < irModule.tables.size(); ++tableIndex)
	{
		moduleContext.tableOffsets.push_back(llvm::ConstantExpr::getPtrToInt(
			createImportedConstant(outLLVMModule,
								   llvm::Twine("tableOffset") + llvm::Twine(tableIndex)),
			llvmIptrType));
	}
	if(moduleContext.tableOffsets.size())
	{ moduleContext.defaultTableOffset = moduleContext.tableOffsets[0]; }

	// Create LLVM external globals corresponding to offsets to memory base pointers in
	// CompartmentRuntimeData for the module's declared memory objects.
	for(Uptr memoryIndex = 0; memoryIndex < irModule.memories.size(); ++memoryIndex)
	{
		moduleContext.memoryOffsets.push_back(llvm::ConstantExpr::getPtrToInt(
			createImportedConstant(outLLVMModule,
								   llvm::Twine("memoryOffset") + llvm::Twine(memoryIndex)),
			llvmIptrType));
	}
	if(moduleContext.memoryOffsets.size())
	{ moduleContext.defaultMemoryOffset = moduleContext.memoryOffsets[0]; }

	// Create LLVM external globals for the module's globals.
	for(Uptr globalIndex = 0; globalIndex < irModule.globals.size(); ++globalIndex)
	{
		moduleContext.globals.push_back(createImportedConstant(
			outLLVMModule, llvm::Twine("global") + llvm::Twine(globalIndex)));
	}

	// Create LLVM external globals corresponding to pointers to ExceptionTypeInstances for the
	// module's declared exception types.
	for(Uptr exceptionTypeIndex = 0; exceptionTypeIndex < irModule.exceptionTypes.size();
		++exceptionTypeIndex)
	{
		moduleContext.exceptionTypeInstances.push_back(createImportedConstant(
			outLLVMModule, llvm::Twine("exceptionType") + llvm::Twine(exceptionTypeIndex)));
	}

	// Create the LLVM functions.
	moduleContext.functions.resize(irModule.functions.size());
	for(Uptr functionIndex = 0; functionIndex < irModule.functions.size(); ++functionIndex)
	{
		FunctionType functionType = irModule.types[irModule.functions.getType(functionIndex).index];

		llvm::Function* llvmFunction
			= llvm::Function::Create(asLLVMType(functionType, CallingConvention::wasm),
									 llvm::Function::ExternalLinkage,
									 getExternalFunctionName(functionIndex),
									 &outLLVMModule);
		llvmFunction->setCallingConv(asLLVMCallingConv(CallingConvention::wasm));
		moduleContext.functions[functionIndex] = llvmFunction;

		if(functionIndex >= irModule.functions.imports.size())
		{ llvmFunction->setPersonalityFn(personalityFunction); }
	}

	// Compile each function in the module.
	for(Uptr functionDefIndex = 0; functionDefIndex < irModule.functions.defs.size();
		++functionDefIndex)
	{
		EmitFunctionContext(
			moduleContext,
			irModule,
			irModule.functions.defs[functionDefIndex],
			moduleContext.functions[irModule.functions.imports.size() + functionDefIndex])
			.emit();
	}

	// Finalize the debug info.
	moduleContext.diBuilder.finalize();

	Timing::logRatePerSecond("Emitted LLVM IR", emitTimer, (F64)outLLVMModule.size(), "functions");
}

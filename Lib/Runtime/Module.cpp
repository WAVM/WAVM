#include <string.h>
#include <atomic>
#include <memory>
#include <utility>

#include "RuntimePrivate.h"
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Platform/Intrinsic.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Runtime/Runtime.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

static Value evaluateInitializer(const std::vector<Global*>& moduleGlobals,
								 InitializerExpression expression)
{
	switch(expression.type)
	{
	case InitializerExpression::Type::i32_const: return expression.i32;
	case InitializerExpression::Type::i64_const: return expression.i64;
	case InitializerExpression::Type::f32_const: return expression.f32;
	case InitializerExpression::Type::f64_const: return expression.f64;
	case InitializerExpression::Type::v128_const: return expression.v128;
	case InitializerExpression::Type::get_global:
	{
		// Find the import this refers to.
		errorUnless(expression.globalIndex < moduleGlobals.size());
		Global* global = moduleGlobals[expression.globalIndex];
		errorUnless(global);
		errorUnless(!global->type.isMutable);
		return IR::Value(global->type.valueType, global->initialValue);
	}
	case InitializerExpression::Type::ref_null: return nullptr;
	default: Errors::unreachable();
	};
}

ModuleRef Runtime::compileModule(const IR::Module& irModule)
{
	std::vector<U8> objectCode = LLVMJIT::compileModule(irModule);
	return std::make_shared<Module>(IR::Module(irModule), std::move(objectCode));
}

std::vector<U8> Runtime::getObjectCode(ModuleConstRefParam module) { return module->objectCode; }

ModuleRef Runtime::loadPrecompiledModule(const IR::Module& irModule,
										 const std::vector<U8>& objectCode)
{
	return std::make_shared<Module>(IR::Module(irModule), std::vector<U8>(objectCode));
}

ModuleInstance::~ModuleInstance()
{
	if(id != UINTPTR_MAX)
	{
		wavmAssertMutexIsLockedByCurrentThread(compartment->mutex);
		compartment->moduleInstances.removeOrFail(id);
	}
}

ModuleInstance* Runtime::instantiateModule(Compartment* compartment,
										   ModuleConstRefParam module,
										   ImportBindings&& imports,
										   std::string&& moduleDebugName)
{
	dummyReferenceAtomics();
	dummyReferenceWAVMIntrinsics();

	// Create the ModuleInstance and add it to the compartment's modules list.
	ModuleInstance* moduleInstance = new ModuleInstance(compartment,
														std::move(imports.functions),
														std::move(imports.tables),
														std::move(imports.memories),
														std::move(imports.globals),
														std::move(imports.exceptionTypes),
														std::move(moduleDebugName));
	{
		Lock<Platform::Mutex> compartmentLock(compartment->mutex);
		moduleInstance->id = compartment->moduleInstances.add(0, moduleInstance);
		errorUnless(moduleInstance->id != UINTPTR_MAX);
	}

	// Check the type of the ModuleInstance's imports.
	errorUnless(moduleInstance->functions.size() == module->ir.functions.imports.size());
	for(Uptr importIndex = 0; importIndex < module->ir.functions.imports.size(); ++importIndex)
	{
		Object* importObject = asObject(moduleInstance->functions[importIndex]);
		errorUnless(
			isA(importObject, module->ir.types[module->ir.functions.getType(importIndex).index]));
		errorUnless(isInCompartment(importObject, compartment));
	}
	errorUnless(moduleInstance->tables.size() == module->ir.tables.imports.size());
	for(Uptr importIndex = 0; importIndex < module->ir.tables.imports.size(); ++importIndex)
	{
		Object* importObject = asObject(moduleInstance->tables[importIndex]);
		errorUnless(isA(importObject, module->ir.tables.getType(importIndex)));
		errorUnless(isInCompartment(importObject, compartment));
	}
	errorUnless(moduleInstance->memories.size() == module->ir.memories.imports.size());
	for(Uptr importIndex = 0; importIndex < module->ir.memories.imports.size(); ++importIndex)
	{
		Object* importObject = asObject(moduleInstance->memories[importIndex]);
		errorUnless(isA(importObject, module->ir.memories.getType(importIndex)));
		errorUnless(isInCompartment(importObject, compartment));
	}
	errorUnless(moduleInstance->globals.size() == module->ir.globals.imports.size());
	for(Uptr importIndex = 0; importIndex < module->ir.globals.imports.size(); ++importIndex)
	{
		Object* importObject = asObject(moduleInstance->globals[importIndex]);
		errorUnless(isA(importObject, module->ir.globals.getType(importIndex)));
		errorUnless(isInCompartment(importObject, compartment));
	}
	errorUnless(moduleInstance->exceptionTypes.size() == module->ir.exceptionTypes.imports.size());
	for(Uptr importIndex = 0; importIndex < module->ir.exceptionTypes.imports.size(); ++importIndex)
	{
		Object* importObject = asObject(moduleInstance->exceptionTypes[importIndex]);
		errorUnless(isA(importObject, module->ir.exceptionTypes.getType(importIndex)));
		errorUnless(isInCompartment(importObject, compartment));
	}

	// Deserialize the disassembly names.
	DisassemblyNames disassemblyNames;
	getDisassemblyNames(module->ir, disassemblyNames);

	// Instantiate the module's memory and table definitions.
	for(Uptr tableDefIndex = 0; tableDefIndex < module->ir.tables.defs.size(); ++tableDefIndex)
	{
		std::string debugName
			= disassemblyNames.tables[module->ir.tables.imports.size() + tableDefIndex];
		auto table = createTable(
			compartment, module->ir.tables.defs[tableDefIndex].type, std::move(debugName));
		if(!table) { throwException(Exception::outOfMemoryType); }
		moduleInstance->tables.push_back(table);
	}
	for(Uptr memoryDefIndex = 0; memoryDefIndex < module->ir.memories.defs.size(); ++memoryDefIndex)
	{
		std::string debugName
			= disassemblyNames.memories[module->ir.memories.imports.size() + memoryDefIndex];
		auto memory = createMemory(
			compartment, module->ir.memories.defs[memoryDefIndex].type, std::move(debugName));
		if(!memory) { throwException(Exception::outOfMemoryType); }
		moduleInstance->memories.push_back(memory);
	}

	// Find the default memory and table for the module and initialize the runtime data memory/table
	// base pointers.
	if(moduleInstance->memories.size() != 0)
	{
		wavmAssert(moduleInstance->memories.size() == 1);
		moduleInstance->defaultMemory = moduleInstance->memories[0];
	}
	if(moduleInstance->tables.size() != 0)
	{ moduleInstance->defaultTable = moduleInstance->tables[0]; }

	// Instantiate the module's global definitions.
	for(const GlobalDef& globalDef : module->ir.globals.defs)
	{
		const Value initialValue
			= evaluateInitializer(moduleInstance->globals, globalDef.initializer);
		errorUnless(isSubtype(initialValue.type, globalDef.type.valueType));
		moduleInstance->globals.push_back(createGlobal(compartment, globalDef.type, initialValue));
	}

	// Instantiate the module's exception types.
	for(Uptr exceptionTypeDefIndex = 0;
		exceptionTypeDefIndex < module->ir.exceptionTypes.defs.size();
		++exceptionTypeDefIndex)
	{
		const ExceptionTypeDef& exceptionTypeDef
			= module->ir.exceptionTypes.defs[exceptionTypeDefIndex];
		std::string debugName
			= disassemblyNames
				  .exceptionTypes[module->ir.exceptionTypes.imports.size() + exceptionTypeDefIndex];
		moduleInstance->exceptionTypes.push_back(
			createExceptionType(compartment, exceptionTypeDef.type, std::move(debugName)));
	}

	HashMap<std::string, LLVMJIT::FunctionBinding> wavmIntrinsicsExportMap;
	for(const HashMapPair<std::string, Intrinsics::Function*>& intrinsicFunctionPair :
		Intrinsics::getUninstantiatedFunctions(INTRINSIC_MODULE_REF(wavmIntrinsics)))
	{
		LLVMJIT::FunctionBinding functionBinding{
			intrinsicFunctionPair.value->getCallingConvention(),
			intrinsicFunctionPair.value->getNativeFunction()};
		wavmIntrinsicsExportMap.add(intrinsicFunctionPair.key, functionBinding);
	}

	std::vector<FunctionType> jitTypes = module->ir.types;
	LLVMJIT::MemoryBinding jitDefaultMemory{
		moduleInstance->defaultMemory ? moduleInstance->defaultMemory->id : UINTPTR_MAX};
	LLVMJIT::TableBinding jitDefaultTable{
		moduleInstance->defaultTable ? moduleInstance->defaultTable->id : UINTPTR_MAX};

	std::vector<LLVMJIT::FunctionBinding> jitFunctionImports;
	for(Uptr importIndex = 0; importIndex < module->ir.functions.imports.size(); ++importIndex)
	{
		jitFunctionImports.push_back(
			{CallingConvention::wasm,
			 const_cast<U8*>(moduleInstance->functions[importIndex]->code)});
	}

	std::vector<LLVMJIT::TableBinding> jitTables;
	for(Table* table : moduleInstance->tables) { jitTables.push_back({table->id}); }

	std::vector<LLVMJIT::MemoryBinding> jitMemories;
	for(Memory* memory : moduleInstance->memories) { jitMemories.push_back({memory->id}); }

	std::vector<LLVMJIT::GlobalBinding> jitGlobals;
	for(Global* global : moduleInstance->globals)
	{
		LLVMJIT::GlobalBinding globalSpec;
		globalSpec.type = global->type;
		if(global->type.isMutable) { globalSpec.mutableGlobalId = global->mutableGlobalId; }
		else
		{
			globalSpec.immutableValuePointer = &global->initialValue;
		}
		jitGlobals.push_back(globalSpec);
	}

	std::vector<LLVMJIT::ExceptionTypeBinding> jitExceptionTypes;
	for(ExceptionType* exceptionType : moduleInstance->exceptionTypes)
	{ jitExceptionTypes.push_back({exceptionType->id}); }

	// Create a FunctionMutableData for each function definition.
	std::vector<FunctionMutableData*> functionDefMutableDatas;
	for(Uptr functionDefIndex = 0; functionDefIndex < module->ir.functions.defs.size();
		++functionDefIndex)
	{
		std::string debugName
			= disassemblyNames.functions[module->ir.functions.imports.size() + functionDefIndex]
				  .name;
		if(!debugName.size())
		{ debugName = "<function #" + std::to_string(functionDefIndex) + ">"; }
		debugName = "wasm!" + moduleInstance->debugName + '!' + debugName;

		functionDefMutableDatas.push_back(new FunctionMutableData(std::move(debugName)));
	}

	// Load the compiled module's object code with this module instance's imports.
	std::vector<Runtime::Function*> jitFunctionDefs;
	jitFunctionDefs.resize(module->ir.functions.defs.size(), nullptr);
	moduleInstance->jitModule = LLVMJIT::loadModule(module->objectCode,
													std::move(wavmIntrinsicsExportMap),
													std::move(jitTypes),
													std::move(jitFunctionImports),
													std::move(jitTables),
													std::move(jitMemories),
													std::move(jitGlobals),
													std::move(jitExceptionTypes),
													jitDefaultMemory,
													jitDefaultTable,
													{moduleInstance->id},
													reinterpret_cast<Uptr>(getOutOfBoundsElement()),
													functionDefMutableDatas);

	// LLVMJIT::loadModule filled in the functionDefMutableDatas' function pointers with the
	// compiled functions. Add those functions to the module.
	for(FunctionMutableData* functionMutableData : functionDefMutableDatas)
	{ moduleInstance->functions.push_back(functionMutableData->function); }

	// Set up the instance's exports.
	for(const Export& exportIt : module->ir.exports)
	{
		Object* exportedObject = nullptr;
		switch(exportIt.kind)
		{
		case IR::ExternKind::function:
			exportedObject = asObject(moduleInstance->functions[exportIt.index]);
			break;
		case IR::ExternKind::table: exportedObject = moduleInstance->tables[exportIt.index]; break;
		case IR::ExternKind::memory:
			exportedObject = moduleInstance->memories[exportIt.index];
			break;
		case IR::ExternKind::global:
			exportedObject = moduleInstance->globals[exportIt.index];
			break;
		case IR::ExternKind::exceptionType:
			exportedObject = moduleInstance->exceptionTypes[exportIt.index];
			break;
		default: Errors::unreachable();
		}
		moduleInstance->exportMap.addOrFail(exportIt.name, exportedObject);
	}

	// Copy the module's data segments into their designated memory instances.
	for(const DataSegment& dataSegment : module->ir.dataSegments)
	{
		if(dataSegment.isActive)
		{
			Memory* memory = moduleInstance->memories[dataSegment.memoryIndex];

			const Value baseOffsetValue
				= evaluateInitializer(moduleInstance->globals, dataSegment.baseOffset);
			errorUnless(baseOffsetValue.type == ValueType::i32);
			const U32 baseOffset = baseOffsetValue.i32;

			if(dataSegment.data.size())
			{
				Platform::bytewiseMemCopy(memory->baseAddress + baseOffset,
										  dataSegment.data.data(),
										  dataSegment.data.size());
			}
			else
			{
				// WebAssembly still expects out-of-bounds errors if the segment base offset is
				// out-of-bounds, even if the segment is empty.
				if(baseOffset > memory->numPages * IR::numBytesPerPage)
				{
					throwException(Runtime::Exception::outOfBoundsMemoryAccessType,
								   {memory, U64(baseOffset)});
				}
			}
		}
	}

	// Copy the module's elem segments into their designated table instances.
	for(const ElemSegment& elemSegment : module->ir.elemSegments)
	{
		if(elemSegment.isActive)
		{
			Table* table = moduleInstance->tables[elemSegment.tableIndex];

			const Value baseOffsetValue
				= evaluateInitializer(moduleInstance->globals, elemSegment.baseOffset);
			errorUnless(baseOffsetValue.type == ValueType::i32);
			const U32 baseOffset = baseOffsetValue.i32;

			if(elemSegment.indices.size())
			{
				for(Uptr index = 0; index < elemSegment.indices.size(); ++index)
				{
					const Uptr functionIndex = elemSegment.indices[index];
					wavmAssert(functionIndex < moduleInstance->functions.size());
					Function* function = moduleInstance->functions[functionIndex];
					setTableElement(table, baseOffset + index, asObject(function));
				}
			}
			else
			{
				// WebAssembly still expects out-of-bounds errors if the segment base offset is
				// out-of-bounds, even if the segment is empty.
				if(baseOffset > getTableNumElements(table))
				{
					throwException(Runtime::Exception::outOfBoundsTableAccessType,
								   {table, U64(baseOffset)});
				}
			}
		}
	}

	// Copy the module's passive data and table segments into the ModuleInstance for later use.
	for(Uptr segmentIndex = 0; segmentIndex < module->ir.dataSegments.size(); ++segmentIndex)
	{
		const DataSegment& dataSegment = module->ir.dataSegments[segmentIndex];
		if(!dataSegment.isActive)
		{
			moduleInstance->passiveDataSegments.add(
				segmentIndex, std::make_shared<std::vector<U8>>(dataSegment.data));
		}
	}
	for(Uptr segmentIndex = 0; segmentIndex < module->ir.elemSegments.size(); ++segmentIndex)
	{
		const ElemSegment& elemSegment = module->ir.elemSegments[segmentIndex];
		if(!elemSegment.isActive)
		{
			auto passiveElemSegmentObjects = std::make_shared<std::vector<Object*>>();
			for(Uptr functionIndex : elemSegment.indices)
			{
				passiveElemSegmentObjects->push_back(
					asObject(moduleInstance->functions[functionIndex]));
			}
			moduleInstance->passiveElemSegments.add(segmentIndex, passiveElemSegmentObjects);
		}
	}

	// Look up the module's start function.
	if(module->ir.startFunctionIndex != UINTPTR_MAX)
	{
		moduleInstance->startFunction = moduleInstance->functions[module->ir.startFunctionIndex];
		wavmAssert(FunctionType(moduleInstance->startFunction->encodedType) == FunctionType());
	}

	return moduleInstance;
}

Function* Runtime::getStartFunction(ModuleInstance* moduleInstance)
{
	return moduleInstance->startFunction;
}

Memory* Runtime::getDefaultMemory(ModuleInstance* moduleInstance)
{
	return moduleInstance->defaultMemory;
}
Table* Runtime::getDefaultTable(ModuleInstance* moduleInstance)
{
	return moduleInstance->defaultTable;
}

Object* Runtime::getInstanceExport(ModuleInstance* moduleInstance, const std::string& name)
{
	wavmAssert(moduleInstance);
	Object* const* exportedObjectPtr = moduleInstance->exportMap.get(name);
	return exportedObjectPtr ? *exportedObjectPtr : nullptr;
}

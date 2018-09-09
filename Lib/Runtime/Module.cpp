#include <string.h>
#include <atomic>
#include <memory>
#include <utility>

#include "IR/IR.h"
#include "IR/Module.h"
#include "IR/Types.h"
#include "IR/Value.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Hash.h"
#include "Inline/HashMap.h"
#include "Inline/Lock.h"
#include "LLVMJIT/LLVMJIT.h"
#include "Platform/Intrinsic.h"
#include "Platform/Mutex.h"
#include "Runtime/Runtime.h"
#include "RuntimePrivate.h"

using namespace IR;
using namespace Runtime;

static Value evaluateInitializer(ModuleInstance* moduleInstance, InitializerExpression expression)
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
		errorUnless(expression.globalIndex < moduleInstance->globals.size());
		GlobalInstance* globalInstance = moduleInstance->globals[expression.globalIndex];
		errorUnless(!globalInstance->type.isMutable);
		return IR::Value(globalInstance->type.valueType, globalInstance->initialValue);
	}
	case InitializerExpression::Type::ref_null: return nullptr;
	default: Errors::unreachable();
	};
}

Runtime::Module* Runtime::compileModule(const IR::Module& irModule)
{
	IndexSpace<FunctionType, FunctionType> functions;
	for(const Import<IndexedFunctionType>& import : irModule.functions.imports)
	{
		functions.imports.push_back(
			{irModule.types[import.type.index], import.moduleName, import.exportName});
	}
	for(const FunctionDef& functionDef : irModule.functions.defs)
	{ functions.defs.push_back(irModule.types[functionDef.type.index]); }

	IndexSpace<TableDef, TableType> tables = irModule.tables;
	IndexSpace<MemoryDef, MemoryType> memories = irModule.memories;
	IndexSpace<GlobalDef, GlobalType> globals = irModule.globals;
	IndexSpace<ExceptionTypeDef, ExceptionType> exceptionTypes = irModule.exceptionTypes;
	std::vector<Export> exports = irModule.exports;
	std::vector<DataSegment> dataSegments = irModule.dataSegments;
	std::vector<TableSegment> tableSegments = irModule.tableSegments;

	DisassemblyNames disassemblyNames;
	getDisassemblyNames(irModule, disassemblyNames);

	std::vector<U8> objectFileBytes = LLVMJIT::compileModule(irModule);
	return new Module(std::move(functions),
					  std::move(tables),
					  std::move(memories),
					  std::move(globals),
					  std::move(exceptionTypes),
					  std::move(exports),
					  std::move(dataSegments),
					  std::move(tableSegments),
					  irModule.startFunctionIndex,
					  std::move(disassemblyNames),
					  std::move(objectFileBytes));
}

ModuleInstance::~ModuleInstance()
{
	if(jitModule)
	{
		LLVMJIT::unloadModule(jitModule);
		jitModule = nullptr;
	}
}

ModuleInstance* Runtime::instantiateModule(Compartment* compartment,
										   Module* module,
										   ImportBindings&& imports,
										   std::string&& moduleDebugName)
{
	ModuleInstance* moduleInstance = new ModuleInstance(compartment,
														std::move(imports.functions),
														std::move(imports.tables),
														std::move(imports.memories),
														std::move(imports.globals),
														std::move(imports.exceptionTypes),
														std::move(moduleDebugName));

	// Check the type of the ModuleInstance's imports.
	errorUnless(moduleInstance->functions.size() == module->functions.imports.size());
	for(Uptr importIndex = 0; importIndex < module->functions.imports.size(); ++importIndex)
	{
		errorUnless(isA(moduleInstance->functions[importIndex],
						module->functions.imports[importIndex].type));
	}
	errorUnless(moduleInstance->tables.size() == module->tables.imports.size());
	for(Uptr importIndex = 0; importIndex < module->tables.imports.size(); ++importIndex)
	{
		errorUnless(
			isA(moduleInstance->tables[importIndex], module->tables.imports[importIndex].type));
	}
	errorUnless(moduleInstance->memories.size() == module->memories.imports.size());
	for(Uptr importIndex = 0; importIndex < module->memories.imports.size(); ++importIndex)
	{
		errorUnless(
			isA(moduleInstance->memories[importIndex], module->memories.imports[importIndex].type));
	}
	errorUnless(moduleInstance->globals.size() == module->globals.imports.size());
	for(Uptr importIndex = 0; importIndex < module->globals.imports.size(); ++importIndex)
	{
		errorUnless(
			isA(moduleInstance->globals[importIndex], module->globals.imports[importIndex].type));
	}
	errorUnless(moduleInstance->exceptionTypes.size() == module->exceptionTypes.imports.size());
	for(Uptr importIndex = 0; importIndex < module->exceptionTypes.imports.size(); ++importIndex)
	{
		errorUnless(isA(moduleInstance->exceptionTypes[importIndex],
						module->exceptionTypes.imports[importIndex].type));
	}

	// Instantiate the module's memory and table definitions.
	for(const TableDef& tableDef : module->tables.defs)
	{
		auto table = createTable(compartment, tableDef.type);
		if(!table) { throwException(Exception::outOfMemoryType); }
		moduleInstance->tables.push_back(table);
	}
	for(const MemoryDef& memoryDef : module->memories.defs)
	{
		auto memory = createMemory(compartment, memoryDef.type);
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
	for(const GlobalDef& globalDef : module->globals.defs)
	{
		const Value initialValue = evaluateInitializer(moduleInstance, globalDef.initializer);
		errorUnless(isSubtype(initialValue.type, globalDef.type.valueType));
		moduleInstance->globals.push_back(createGlobal(compartment, globalDef.type, initialValue));
	}

	// Instantiate the module's exception types.
	for(const ExceptionTypeDef& exceptionTypeDef : module->exceptionTypes.defs)
	{
		moduleInstance->exceptionTypes.push_back(
			createExceptionTypeInstance(exceptionTypeDef.type, "wasmException"));
	}

	HashMap<std::string, LLVMJIT::FunctionBinding> wavmIntrinsicsExportMap;
	for(auto intrinsicExport : compartment->wavmIntrinsics->exportMap)
	{
		FunctionInstance* intrinsicFunction = asFunction(intrinsicExport.value);
		wavmAssert(intrinsicFunction);
		wavmAssert(intrinsicFunction->callingConvention == IR::CallingConvention::intrinsic);
		errorUnless(wavmIntrinsicsExportMap.add(
			intrinsicExport.key, LLVMJIT::FunctionBinding{intrinsicFunction->nativeFunction}));
	}

	LLVMJIT::MemoryBinding jitDefaultMemory{
		moduleInstance->defaultMemory ? moduleInstance->defaultMemory->id : UINTPTR_MAX};
	LLVMJIT::TableBinding jitDefaultTable{
		moduleInstance->defaultTable ? moduleInstance->defaultTable->id : UINTPTR_MAX};

	std::vector<LLVMJIT::FunctionBinding> jitFunctionImports;
	for(Uptr importIndex = 0; importIndex < module->functions.imports.size(); ++importIndex)
	{
		FunctionInstance* functionImport = moduleInstance->functions[importIndex];
		void* nativeFunction = functionImport->nativeFunction;
		if(functionImport->callingConvention != IR::CallingConvention::wasm)
		{
			nativeFunction = LLVMJIT::getIntrinsicThunk(nativeFunction,
														functionImport->type,
														functionImport->callingConvention,
														jitDefaultMemory,
														jitDefaultTable);
		}
		jitFunctionImports.push_back({nativeFunction});
	}

	std::vector<LLVMJIT::TableBinding> jitTables;
	for(TableInstance* table : moduleInstance->tables) { jitTables.push_back({table->id}); }

	std::vector<LLVMJIT::MemoryBinding> jitMemories;
	for(MemoryInstance* memory : moduleInstance->memories) { jitMemories.push_back({memory->id}); }

	std::vector<LLVMJIT::GlobalBinding> jitGlobals;
	for(GlobalInstance* global : moduleInstance->globals)
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

	std::vector<ExceptionTypeInstance*> jitExceptionTypes;
	for(ExceptionTypeInstance* exceptionTypeInstance : moduleInstance->exceptionTypes)
	{ jitExceptionTypes.push_back(exceptionTypeInstance); }

	// Load the compiled module's object code with this module instance's imports.
	std::vector<LLVMJIT::JITFunction*> jitFunctionDefs;
	moduleInstance->jitModule = LLVMJIT::loadModule(module->objectFileBytes,
													std::move(wavmIntrinsicsExportMap),
													std::move(jitFunctionImports),
													std::move(jitTables),
													std::move(jitMemories),
													std::move(jitGlobals),
													std::move(jitExceptionTypes),
													jitDefaultMemory,
													jitDefaultTable,
													moduleInstance,
													module->functions.defs.size(),
													jitFunctionDefs);

	// Create the FunctionInstance objects for the module's function definitions.
	for(Uptr functionDefIndex = 0; functionDefIndex < module->functions.defs.size();
		++functionDefIndex)
	{
		const DisassemblyNames::Function& functionNames
			= module->disassemblyNames
				  .functions[module->functions.imports.size() + functionDefIndex];
		std::string debugName = functionNames.name;
		if(!debugName.size())
		{ debugName = "<function #" + std::to_string(functionDefIndex) + ">"; }

		LLVMJIT::JITFunction* jitFunction = jitFunctionDefs[functionDefIndex];

		auto functionInstance
			= new FunctionInstance(moduleInstance,
								   module->functions.defs[functionDefIndex],
								   reinterpret_cast<void*>(jitFunction->baseAddress),
								   IR::CallingConvention::wasm,
								   std::move(debugName));
		moduleInstance->functionDefs.push_back(functionInstance);
		moduleInstance->functions.push_back(functionInstance);

		jitFunction->type = LLVMJIT::JITFunction::Type::wasmFunction;
		jitFunction->functionInstance = functionInstance;
	}

	// Set up the instance's exports.
	for(const Export& exportIt : module->exports)
	{
		Object* exportedObject = nullptr;
		switch(exportIt.kind)
		{
		case IR::ObjectKind::function:
			exportedObject = moduleInstance->functions[exportIt.index];
			break;
		case IR::ObjectKind::table: exportedObject = moduleInstance->tables[exportIt.index]; break;
		case IR::ObjectKind::memory:
			exportedObject = moduleInstance->memories[exportIt.index];
			break;
		case IR::ObjectKind::global:
			exportedObject = moduleInstance->globals[exportIt.index];
			break;
		case IR::ObjectKind::exceptionType:
			exportedObject = moduleInstance->exceptionTypes[exportIt.index];
			break;
		default: Errors::unreachable();
		}
		moduleInstance->exportMap.addOrFail(exportIt.name, exportedObject);
	}

	// Copy the module's data segments into the module's default memory.
	for(const DataSegment& dataSegment : module->dataSegments)
	{
		if(dataSegment.isActive)
		{
			MemoryInstance* memory = moduleInstance->memories[dataSegment.memoryIndex];

			const Value baseOffsetValue
				= evaluateInitializer(moduleInstance, dataSegment.baseOffset);
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
				{ throwException(Runtime::Exception::accessViolationType); }
			}
		}
	}

	// Copy the module's table segments into the module's default table.
	for(const TableSegment& tableSegment : module->tableSegments)
	{
		if(tableSegment.isActive)
		{
			TableInstance* table = moduleInstance->tables[tableSegment.tableIndex];

			const Value baseOffsetValue
				= evaluateInitializer(moduleInstance, tableSegment.baseOffset);
			errorUnless(baseOffsetValue.type == ValueType::i32);
			const U32 baseOffset = baseOffsetValue.i32;

			if(tableSegment.indices.size())
			{
				for(Uptr index = 0; index < tableSegment.indices.size(); ++index)
				{
					const Uptr functionIndex = tableSegment.indices[index];
					wavmAssert(functionIndex < moduleInstance->functions.size());
					setTableElement(table,
									baseOffset + index,
									moduleInstance->functions[functionIndex],
									moduleInstance->defaultMemory,
									moduleInstance->defaultTable);
				}
			}
			else
			{
				// WebAssembly still expects out-of-bounds errors if the segment base offset is
				// out-of-bounds, even if the segment is empty.
				Lock<Platform::Mutex> elementsLock(table->elementsMutex);
				if(baseOffset > table->elements.size())
				{ throwException(Runtime::Exception::accessViolationType); }
			}
		}
	}

	// Copy the module's passive data and table segments into the ModuleInstance for later use.
	for(Uptr segmentIndex = 0; segmentIndex < module->dataSegments.size(); ++segmentIndex)
	{
		const DataSegment& dataSegment = module->dataSegments[segmentIndex];
		if(!dataSegment.isActive)
		{
			moduleInstance->passiveDataSegments.add(
				segmentIndex, std::make_shared<std::vector<U8>>(dataSegment.data));
		}
	}
	for(Uptr segmentIndex = 0; segmentIndex < module->tableSegments.size(); ++segmentIndex)
	{
		const TableSegment& tableSegment = module->tableSegments[segmentIndex];
		if(!tableSegment.isActive)
		{ moduleInstance->passiveTableSegments.add(segmentIndex, tableSegment.indices); }
	}

	// Look up the module's start function.
	if(module->startFunctionIndex != UINTPTR_MAX)
	{
		moduleInstance->startFunction = moduleInstance->functions[module->startFunctionIndex];
		wavmAssert(moduleInstance->startFunction->type == IR::FunctionType());
	}

	return moduleInstance;
}

FunctionInstance* Runtime::getStartFunction(ModuleInstance* moduleInstance)
{
	return moduleInstance->startFunction;
}

MemoryInstance* Runtime::getDefaultMemory(ModuleInstance* moduleInstance)
{
	return moduleInstance->defaultMemory;
}
TableInstance* Runtime::getDefaultTable(ModuleInstance* moduleInstance)
{
	return moduleInstance->defaultTable;
}

Object* Runtime::getInstanceExport(ModuleInstance* moduleInstance, const std::string& name)
{
	wavmAssert(moduleInstance);
	Object* const* exportedObjectPtr = moduleInstance->exportMap.get(name);
	return exportedObjectPtr ? *exportedObjectPtr : nullptr;
}

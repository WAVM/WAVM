#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Runtime.h"
#include "RuntimePrivate.h"
#include "IR/Module.h"

#include <string.h>

namespace Runtime
{
	Value evaluateInitializer(ModuleInstance* moduleInstance,InitializerExpression expression)
	{
		switch(expression.type)
		{
		case InitializerExpression::Type::i32_const: return expression.i32;
		case InitializerExpression::Type::i64_const: return expression.i64;
		case InitializerExpression::Type::f32_const: return expression.f32;
		case InitializerExpression::Type::f64_const: return expression.f64;
		case InitializerExpression::Type::get_global:
		{
			// Find the import this refers to.
			errorUnless(expression.globalIndex < moduleInstance->globals.size());
			GlobalInstance* globalInstance = moduleInstance->globals[expression.globalIndex];
			errorUnless(!globalInstance->type.isMutable);
			return Runtime::Value(globalInstance->type.valueType,globalInstance->initialValue);
		}
		default: Errors::unreachable();
		};
	}

	ModuleInstance* instantiateModule(
		Compartment* compartment,
		const IR::Module& module,
		ImportBindings&& imports,
		std::string&& moduleDebugName)
	{
		ModuleInstance* moduleInstance = new ModuleInstance(
			compartment,
			std::move(imports.functions),
			std::move(imports.tables),
			std::move(imports.memories),
			std::move(imports.globals),
			std::move(imports.exceptionTypes),
			std::move(moduleDebugName)
			);

		// Get disassembly names for the module's objects.
		DisassemblyNames disassemblyNames;
		IR::getDisassemblyNames(module,disassemblyNames);

		// Check the type of the ModuleInstance's imports.
		errorUnless(moduleInstance->functions.size() == module.functions.imports.size());
		for(Uptr importIndex = 0;importIndex < module.functions.imports.size();++importIndex)
		{
			errorUnless(isA(moduleInstance->functions[importIndex],module.types[module.functions.imports[importIndex].type.index]));
		}
		errorUnless(moduleInstance->tables.size() == module.tables.imports.size());
		for(Uptr importIndex = 0;importIndex < module.tables.imports.size();++importIndex)
		{
			errorUnless(isA(moduleInstance->tables[importIndex],module.tables.imports[importIndex].type));
		}
		errorUnless(moduleInstance->memories.size() == module.memories.imports.size());
		for(Uptr importIndex = 0;importIndex < module.memories.imports.size();++importIndex)
		{
			errorUnless(isA(moduleInstance->memories[importIndex],module.memories.imports[importIndex].type));
		}
		errorUnless(moduleInstance->globals.size() == module.globals.imports.size());
		for(Uptr importIndex = 0;importIndex < module.globals.imports.size();++importIndex)
		{
			errorUnless(isA(moduleInstance->globals[importIndex],module.globals.imports[importIndex].type));
		}
		errorUnless(moduleInstance->exceptionTypes.size() == module.exceptionTypes.imports.size());
		for(Uptr importIndex = 0;importIndex < module.exceptionTypes.imports.size();++importIndex)
		{
			errorUnless(isA(moduleInstance->exceptionTypes[importIndex],module.exceptionTypes.imports[importIndex].type));
		}

		// Instantiate the module's memory and table definitions.
		for(const TableDef& tableDef : module.tables.defs)
		{
			auto table = createTable(compartment,tableDef.type);
			if(!table) { throwException(Exception::outOfMemoryType); }
			moduleInstance->tables.push_back(table);
		}
		for(const MemoryDef& memoryDef : module.memories.defs)
		{
			auto memory = createMemory(compartment,memoryDef.type);
			if(!memory) { throwException(Exception::outOfMemoryType); }
			moduleInstance->memories.push_back(memory);
		}

		// Find the default memory and table for the module and initialize the runtime data memory/table base pointers.
		if(moduleInstance->memories.size() != 0)
		{
			wavmAssert(moduleInstance->memories.size() == 1);
			moduleInstance->defaultMemory = moduleInstance->memories[0];
		}
		if(moduleInstance->tables.size() != 0)
		{
			wavmAssert(moduleInstance->tables.size() == 1);
			moduleInstance->defaultTable = moduleInstance->tables[0];
		}

		// If any memory or table segment doesn't fit, throw an exception before mutating any memory/table.
		for(auto& tableSegment : module.tableSegments)
		{
			TableInstance* table = moduleInstance->tables[tableSegment.tableIndex];
			const Value baseOffsetValue = evaluateInitializer(moduleInstance,tableSegment.baseOffset);
			errorUnless(baseOffsetValue.type == ValueType::i32);
			const U32 baseOffset = baseOffsetValue.i32;
			if(baseOffset > table->elements.size()
			|| table->elements.size() - baseOffset < tableSegment.indices.size())
			{ throwException(Exception::invalidSegmentOffsetType); }
		}
		for(auto& dataSegment : module.dataSegments)
		{
			MemoryInstance* memory = moduleInstance->memories[dataSegment.memoryIndex];

			const Value baseOffsetValue = evaluateInitializer(moduleInstance,dataSegment.baseOffset);
			errorUnless(baseOffsetValue.type == ValueType::i32);
			const U32 baseOffset = baseOffsetValue.i32;
			const Uptr numMemoryBytes = (memory->numPages << IR::numBytesPerPageLog2);
			if(baseOffset > numMemoryBytes
			|| numMemoryBytes - baseOffset < dataSegment.data.size())
			{ throwException(Exception::invalidSegmentOffsetType); }
		}

		// Copy the module's data segments into the module's default memory.
		for(const DataSegment& dataSegment : module.dataSegments)
		{
			MemoryInstance* memory = moduleInstance->memories[dataSegment.memoryIndex];

			const Value baseOffsetValue = evaluateInitializer(moduleInstance,dataSegment.baseOffset);
			errorUnless(baseOffsetValue.type == ValueType::i32);
			const U32 baseOffset = baseOffsetValue.i32;

			wavmAssert(baseOffset + dataSegment.data.size() <= (memory->numPages << IR::numBytesPerPageLog2));

			memcpy(memory->baseAddress + baseOffset,dataSegment.data.data(),dataSegment.data.size());
		}
		
		// Instantiate the module's global definitions.
		for(const GlobalDef& globalDef : module.globals.defs)
		{
			const Value initialValue = evaluateInitializer(moduleInstance,globalDef.initializer);
			errorUnless(initialValue.type == globalDef.type.valueType);
			moduleInstance->globals.push_back(createGlobal(compartment,globalDef.type,initialValue));
		}

		// Instantiate the module's exception types.
		for(const ExceptionTypeDef& exceptionTypeDef : module.exceptionTypes.defs)
		{
			moduleInstance->exceptionTypes.push_back(createExceptionTypeInstance(exceptionTypeDef.type));
		}
		
		// Create the FunctionInstance objects for the module's function definitions.
		for(Uptr functionDefIndex = 0;functionDefIndex < module.functions.defs.size();++functionDefIndex)
		{
			const Uptr functionIndex = moduleInstance->functions.size();
			const DisassemblyNames::Function& functionNames = disassemblyNames.functions[functionIndex];
			std::string debugName = functionNames.name;
			if(!debugName.size()) { debugName = "<function #" + std::to_string(functionDefIndex) + ">"; }
			auto functionInstance = new FunctionInstance(
				moduleInstance,
				module.types[module.functions.defs[functionDefIndex].type.index],
				nullptr,
				CallingConvention::wasm,
				std::move(debugName));
			moduleInstance->functionDefs.push_back(functionInstance);
			moduleInstance->functions.push_back(functionInstance);
		}

		// Generate machine code for the module.
		LLVMJIT::instantiateModule(module,moduleInstance);

		// Set up the instance's exports.
		for(const Export& exportIt : module.exports)
		{
			Object* exportedObject = nullptr;
			switch(exportIt.kind)
			{
			case IR::ObjectKind::function: exportedObject = moduleInstance->functions[exportIt.index]; break;
			case IR::ObjectKind::table: exportedObject = moduleInstance->tables[exportIt.index]; break;
			case IR::ObjectKind::memory: exportedObject = moduleInstance->memories[exportIt.index]; break;
			case IR::ObjectKind::global: exportedObject = moduleInstance->globals[exportIt.index]; break;
			case IR::ObjectKind::exceptionType: exportedObject = moduleInstance->exceptionTypes[exportIt.index]; break;
			default: Errors::unreachable();
			}
			errorUnless(moduleInstance->exportMap.add(exportIt.name, exportedObject));
		}
		
		// Copy the module's table segments into the module's default table.
		for(const TableSegment& tableSegment : module.tableSegments)
		{
			TableInstance* table = moduleInstance->tables[tableSegment.tableIndex];
			
			const Value baseOffsetValue = evaluateInitializer(moduleInstance,tableSegment.baseOffset);
			errorUnless(baseOffsetValue.type == ValueType::i32);
			const U32 baseOffset = baseOffsetValue.i32;
			wavmAssert(baseOffset + tableSegment.indices.size() <= table->elements.size());

			for(Uptr index = 0;index < tableSegment.indices.size();++index)
			{
				const Uptr functionIndex = tableSegment.indices[index];
				wavmAssert(functionIndex < moduleInstance->functions.size());
				setTableElement(table,baseOffset + index,moduleInstance->functions[functionIndex]);
			}
		}

		// Look up the module's start function.
		if(module.startFunctionIndex != UINTPTR_MAX)
		{
			moduleInstance->startFunction = moduleInstance->functions[module.startFunctionIndex];
			wavmAssert(moduleInstance->startFunction->type == IR::FunctionType::get());
		}

		return moduleInstance;
	}

	ModuleInstance::~ModuleInstance()
	{
		if(jitModule) { delete jitModule; }
	}

	FunctionInstance* getStartFunction(ModuleInstance* moduleInstance) { return moduleInstance->startFunction; }

	MemoryInstance* getDefaultMemory(ModuleInstance* moduleInstance) { return moduleInstance->defaultMemory; }
	TableInstance* getDefaultTable(ModuleInstance* moduleInstance) { return moduleInstance->defaultTable; }
	
	Object* getInstanceExport(ModuleInstance* moduleInstance,const std::string& name)
	{
		wavmAssert(moduleInstance);
		Object* const* exportedObjectPtr = moduleInstance->exportMap.get(name);
		return exportedObjectPtr ? *exportedObjectPtr : nullptr;
	}
}
#include "Core/Core.h"
#include "Runtime.h"
#include "RuntimePrivate.h"

namespace Runtime
{
	std::vector<ModuleInstance*> moduleInstances;
	
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
			uintp numGlobalImportsSeen = 0;
			for(Object* import : moduleInstance->imports)
			{
				if(import->kind == ObjectKind::global)
				{
					if(numGlobalImportsSeen++ == expression.globalIndex)
					{
						GlobalInstance* global = asGlobal(import);
						return Runtime::Value(global->type.valueType,global->value);
					}
				}
			}
			throw InstantiationException(InstantiationException::invalidInitializerGlobalRef);
		}
		default: Core::unreachable();
		};
	}

	ModuleInstance* instantiateModule(const Module& module,std::vector<Object*>&& imports)
	{
		ModuleInstance* moduleInstance = new ModuleInstance(std::move(imports));
		
		// Get disassembly names for the module's objects.
		DisassemblyNames disassemblyNames;
		getDisassemblyNames(module,disassemblyNames);

		// Validate the types of the imports.
		if(imports.size() != module.imports.size()) { throw InstantiationException(InstantiationException::importCountMismatch); }
		for(uintp importIndex = 0;importIndex < module.imports.size();++importIndex)
		{
			const Import& import = module.imports[importIndex];
			Object* importObject = imports[importIndex];
			if(!isA(importObject,resolveImportType(module,import.type))) { throw InstantiationException(InstantiationException::importTypeMismatch); }
			switch(importObject->kind)
			{
			case ObjectKind::function: moduleInstance->functions.push_back(asFunction(importObject)); break;
			case ObjectKind::table: moduleInstance->tables.push_back(asTable(importObject)); break;
			case ObjectKind::memory: moduleInstance->memories.push_back(asMemory(importObject)); break;
			case ObjectKind::global: moduleInstance->globals.push_back(asGlobal(importObject)); break;
			default: Core::unreachable();
			};
		}

		// Instantiate the module's memory and table definitions.
		for(auto memoryType : module.memoryDefs)
		{
			auto memory = createMemory(memoryType);
			if(!memory) { throw InstantiationException(InstantiationException::outOfMemory); }
			moduleInstance->memories.push_back(memory);
		}
		for(auto tableType : module.tableDefs)
		{
			auto table = createTable(tableType);
			if(!table) { throw InstantiationException(InstantiationException::outOfMemory); }
			moduleInstance->tables.push_back(table);
		}

		// Find the default memory and table for the module.
		if(moduleInstance->memories.size() != 0)
		{
			assert(moduleInstance->memories.size() == 1);
			moduleInstance->defaultMemory = moduleInstance->memories[0];
		}
		if(moduleInstance->tables.size() != 0)
		{
			assert(moduleInstance->tables.size() == 1);
			moduleInstance->defaultTable = moduleInstance->tables[0];
		}

		// Copy the module's data segments into the module's default memory.
		for(auto& dataSegment : module.dataSegments)
		{
			Memory* memory = moduleInstance->memories[dataSegment.memoryIndex];

			const Value baseOffsetValue = evaluateInitializer(moduleInstance,dataSegment.baseOffset);
			assert(baseOffsetValue.type == ValueType::i32);
			const uint32 baseOffset = baseOffsetValue.i32;

			if(baseOffset + dataSegment.data.size() > (memory->numPages << WebAssembly::numBytesPerPageLog2))
			{ throw InstantiationException(InstantiationException::invalidDataSegmentBase); }

			memcpy(memory->baseAddress + baseOffset,dataSegment.data.data(),dataSegment.data.size());
		}
		
		// Instantiate the module's global definitions.
		for(auto global : module.globalDefs)
		{
			const Value initialValue = evaluateInitializer(moduleInstance,global.initializer);
			assert(initialValue.type == global.type.valueType);
			moduleInstance->globals.push_back(new GlobalInstance(global.type,initialValue));
		}
		
		// Create the FunctionInstance objects for the module's function definitions.
		for(uintp functionDefIndex = 0;functionDefIndex < module.functionDefs.size();++functionDefIndex)
		{
			const uintp functionIndex = moduleInstance->functions.size();
			auto disassemblyName = disassemblyNames.functions[functionIndex];
			if(!disassemblyName.size()) { disassemblyName = "<function #" + std::to_string(functionDefIndex) + ">"; }
			auto functionInstance = new FunctionInstance(moduleInstance,module.types[module.functionDefs[functionDefIndex].typeIndex],nullptr,disassemblyName.c_str());
			moduleInstance->functionDefs.push_back(functionInstance);
			moduleInstance->functions.push_back(functionInstance);
		}

		// Generate machine code for the module.
		LLVMJIT::instantiateModule(module,moduleInstance);

		// Set up the instance's exports.
		for(auto& exportIt : module.exports)
		{
			Object* exportedObject = nullptr;
			switch(exportIt.kind)
			{
			case ObjectKind::function: exportedObject = moduleInstance->functions[exportIt.index]; break;
			case ObjectKind::table: exportedObject = moduleInstance->tables[exportIt.index]; break;
			case ObjectKind::memory: exportedObject = moduleInstance->memories[exportIt.index]; break;
			case ObjectKind::global: exportedObject = moduleInstance->globals[exportIt.index]; break;
			default: Core::unreachable();
			}
			moduleInstance->exportMap[exportIt.name] = exportedObject;
		}
		
		// Copy the module's table segments into the module's default table.
		for(auto& tableSegment : module.tableSegments)
		{
			Table* table = moduleInstance->tables[tableSegment.tableIndex];
			
			const Value baseOffsetValue = evaluateInitializer(moduleInstance,tableSegment.baseOffset);
			assert(baseOffsetValue.type == ValueType::i32);
			const uint32 baseOffset = baseOffsetValue.i32;

			if(baseOffset + tableSegment.indices.size() > table->elements.size())
			{ throw InstantiationException(InstantiationException::invalidDataSegmentBase); }

			for(uintp index = 0;index < tableSegment.indices.size();++index)
			{
				const uintp functionIndex = tableSegment.indices[index];
				assert(functionIndex < moduleInstance->functions.size());
				setTableElement(table,baseOffset + index,moduleInstance->functions[functionIndex]);
			}
		}

		// Call the module's start function.
		if(module.startFunctionIndex != UINTPTR_MAX)
		{
			assert(moduleInstance->functions[module.startFunctionIndex]->type == WebAssembly::FunctionType::get());
			invokeFunction(moduleInstance->functions[module.startFunctionIndex],{});
		}
		

		moduleInstances.push_back(moduleInstance);
		return moduleInstance;
	}

	ModuleInstance::~ModuleInstance()
	{
		delete jitModule;
	}

	Memory* getDefaultMemory(ModuleInstance* moduleInstance) { return moduleInstance->defaultMemory; }
	Table* getDefaultTable(ModuleInstance* moduleInstance) { return moduleInstance->defaultTable; }
	
	Object* getInstanceExport(ModuleInstance* moduleInstance,const char* name)
	{
		auto mapIt = moduleInstance->exportMap.find(name);
		return mapIt == moduleInstance->exportMap.end() ? nullptr : mapIt->second;
	}
}
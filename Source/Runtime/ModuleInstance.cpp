#include "Core/Core.h"
#include "Runtime.h"
#include "RuntimePrivate.h"

#include <iostream>

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
			uintptr numGlobalImportsSeen = 0;
			for(Object* import : moduleInstance->imports)
			{
				if(import->kind == ObjectKind::global)
				{
					if(numGlobalImportsSeen++ == expression.globalIndex)
					{
						GlobalInstance* global = asGlobal(import);
						return Runtime::Value(asRuntimeValueType(global->type.valueType),global->value);
					}
				}
			}
			throw InstantiationException(InstantiationException::invalidInitializerGlobalRef);
		}
		default:
			throw;
		};
	}

	ModuleInstance* instantiateModule(const Module& module,std::vector<Object*>&& imports)
	{
		ModuleInstance* moduleInstance = new ModuleInstance(module,std::move(imports));

		// Validate the types of the imports.
		if(imports.size() != module.imports.size()) { throw InstantiationException(InstantiationException::importCountMismatch); }
		for(uintptr importIndex = 0;importIndex < module.imports.size();++importIndex)
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
			default: throw;
			};
		}

		// Instantiate the module's memory and table definitions.
		for(auto memoryType : module.memoryDefs) { moduleInstance->memories.push_back(createMemory(memoryType)); }
		for(auto tableType : module.tableDefs) { moduleInstance->tables.push_back(createTable(tableType)); }

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
			assert(moduleInstance->defaultMemory);

			const Value baseOffsetValue = evaluateInitializer(moduleInstance,dataSegment.baseOffset);
			assert(baseOffsetValue.type == TypeId::i32);
			const uint32 baseOffset = baseOffsetValue.i32;

			if(baseOffset + dataSegment.data.size() > (moduleInstance->defaultMemory->numPages << WebAssembly::numBytesPerPageLog2))
			{ throw InstantiationException(InstantiationException::invalidDataSegmentBase); }

			memcpy(moduleInstance->defaultMemory->baseAddress + baseOffset,dataSegment.data.data(),dataSegment.data.size());
		}
		
		// Instantiate the module's global definitions.
		for(auto global : module.globalDefs)
		{
			const Value initialValue = evaluateInitializer(moduleInstance,global.initializer);
			assert(initialValue.type == asRuntimeValueType(global.type.valueType));
			moduleInstance->globals.push_back(new GlobalInstance(global.type,initialValue));
		}
		
		// Create the FunctionInstance objects for the module's function definitions.
		for(uintptr functionDefIndex = 0;functionDefIndex < module.functionDefs.size();++functionDefIndex)
		{
			moduleInstance->functions.push_back(new FunctionInstance(
				module.types[module.functionDefs[functionDefIndex].typeIndex],
				nullptr,
				nullptr,
				functionDefIndex < module.disassemblyInfo.functions.size() ? module.disassemblyInfo.functions[functionDefIndex].name.c_str() : "<WebAssembly function>"
				));
		}

		// Generate machine code for the module.
		if(!LLVMJIT::instantiateModule(module,moduleInstance)) { throw InstantiationException(InstantiationException::Cause::codeGenFailed); }

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
			default: throw;
			}
			moduleInstance->exportMap[exportIt.name] = exportedObject;
		}
		
		// Copy the module's table segments into the module's default table.
		for(auto& tableSegment : module.tableSegments)
		{
			assert(moduleInstance->defaultTable);
			
			const Value baseOffsetValue = evaluateInitializer(moduleInstance,tableSegment.baseOffset);
			assert(baseOffsetValue.type == TypeId::i32);
			const uint32 baseOffset = baseOffsetValue.i32;

			if(baseOffset + tableSegment.indices.size() > moduleInstance->defaultTable->numElements)
			{ throw InstantiationException(InstantiationException::invalidDataSegmentBase); }

			Table::Element* defaultTableElements = moduleInstance->defaultTable->baseAddress;
			for(uintptr index = 0;index < tableSegment.indices.size();++index)
			{
				const uintptr functionIndex = tableSegment.indices[index];
				assert(functionIndex < moduleInstance->functions.size());
				assert(moduleInstance->functions[functionIndex]->nativeFunction);
				defaultTableElements[baseOffset + index].type = moduleInstance->functions[functionIndex]->type;
				defaultTableElements[baseOffset + index].value = moduleInstance->functions[functionIndex]->nativeFunction;
			}
		}

		// Initialize the intrinsics.
		initEmscriptenIntrinsics(module,moduleInstance);
		initSpecTestIntrinsics();
		initWAVMIntrinsics();

		// Call the module's start function.
		if(module.startFunctionIndex != UINTPTR_MAX)
		{
			assert(module.types[module.functionDefs[module.startFunctionIndex].typeIndex] == WebAssembly::FunctionType::get());
			auto result = invokeFunction(moduleInstance->functions[module.startFunctionIndex],{});
			if(result.type == Runtime::TypeId::exception)
			{
				std::cerr << "Module start function threw exception: " << Runtime::describeExceptionCause(result.exception->cause) << std::endl;
				for(auto calledFunction : result.exception->callStack) { std::cerr << "  " << calledFunction << std::endl; }
				throw InstantiationException(InstantiationException::Cause::startFunctionException);
			}
		}
		
		moduleInstances.push_back(moduleInstance);
		return moduleInstance;
	}

	Memory* getDefaultMemory(ModuleInstance* moduleInstance) { return moduleInstance->defaultMemory; }
	Table* getDefaultTable(ModuleInstance* moduleInstance) { return moduleInstance->defaultTable; }
	
	Object* getInstanceExport(ModuleInstance* moduleInstance,const char* name)
	{
		auto mapIt = moduleInstance->exportMap.find(name);
		return mapIt == moduleInstance->exportMap.end() ? nullptr : mapIt->second;
	}
}
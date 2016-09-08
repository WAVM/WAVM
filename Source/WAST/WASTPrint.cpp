#include "Core/Core.h"
#include "Core/MemoryArena.h"
#include "WAST.h"
#include "WASTSymbols.h"
#include "WebAssembly/Module.h"

#include <map>

using namespace WebAssembly;

namespace WAST
{
#if 0
	char nibbleToHexChar(uint8 value) { return value < 10 ? ('0' + value) : 'a' + value - 10; }

	std::string escapeString(const char* string,size_t numChars)
	{
		std::string result;
		for(uintptr charIndex = 0;charIndex < numChars;++charIndex)
		{
			auto c = string[charIndex];
			if(c == '\\') { result += "\\\\"; }
			else if(c == '\"') { result += "\\\""; }
			else if(c == '\n') { result += "\\n"; }
			else if(c < 0x20 || c > 0x7e)
			{
				result += '\\';
				result += nibbleToHexChar((c & 0xf0) >> 4);
				result += nibbleToHexChar((c & 0x0f) >> 0);
			}
			else { result += c; }
		}
		return result;
	}

	SNodeOutputStream& operator<<(SNodeOutputStream& stream,Symbol symbol)
	{
		auto symbolNode = new(stream.getArena()) SNode();
		symbolNode->type = SExp::NodeType::Symbol;
		symbolNode->symbol = (uintptr)symbol;
		return stream << symbolNode;
	}
	SNodeOutputStream& operator<<(SNodeOutputStream& stream,ValueType type)
	{
		switch(type)
		{
		case ValueType::i32: return stream << Symbol::_i32;
		case ValueType::i64: return stream << Symbol::_i64;
		case ValueType::f32: return stream << Symbol::_f32;
		case ValueType::f64: return stream << Symbol::_f64;
		default: throw;
		}
	}
	SNodeOutputStream& operator<<(SNodeOutputStream& stream,ReturnType type)
	{
		return stream << (ValueType)type;
	}

	struct PrintContext
	{
		MemoryArena::Arena& arena;

		PrintContext(MemoryArena::Arena& inArena): arena(inArena) {}

		SNodeOutputStream createSubtree()
		{
			SNodeOutputStream result(arena);
			result.enterSubtree();
			return result;
		}
		SNodeOutputStream createAttribute()
		{
			SNodeOutputStream result(arena);
			result.enterAttribute();
			return result;
		}
		SNodeOutputStream createTaggedSubtree(Symbol symbol) { auto subtree = createSubtree(); subtree << symbol; return subtree; }
	};

	struct ModulePrintContext : PrintContext
	{
		const Module& module;

		std::vector<uint32> functionTableBaseIndices;
		
		ModulePrintContext(MemoryArena::Arena& inArena,const Module& inModule)
		: PrintContext(inArena), module(inModule) {}
		
		std::string getFunctionImportName(uintptr importIndex) const
		{
			return std::to_string(importIndex);
		}
		std::string getFunctionName(uintptr functionIndex) const
		{
			if(functionIndex < module.disassemblyInfo.functions.size() && module.disassemblyInfo.functions[functionIndex].name.size())
				{ return std::string("$") + module.disassemblyInfo.functions[functionIndex].name; }
			else { return std::to_string(functionIndex); }
		}
		std::string getFunctionTableName(uintptr functionTableIndex) const
		{
			return std::to_string(functionTableIndex);
		}
		std::string getTypeName(uintptr typeIndex)
		{
			return std::to_string(typeIndex);
		}

		SNodeOutputStream printFunction(uintptr functionIndex);
		SNodeOutputStream printFunctionType(const FunctionType* functionType);
		SNodeOutputStream print();
	};

	SNodeOutputStream ModulePrintContext::printFunction(uintptr functionIndex)
	{
		// Before printing, lower the function's expressions into those supported by WAST.
		const Function& function = module.functionDefs[functionIndex];
		const FunctionType* functionType = module.types[function.typeIndex];
		
		//FunctionPrintContext functionContext(*this,function);
		auto functionStream = createTaggedSubtree(Symbol::_func);

		// Print the function name.
		functionStream << getFunctionName(functionIndex);

		// Print the function parameters.
		auto paramStream = createTaggedSubtree(Symbol::_param);
		for(auto parameterType : functionType->parameters) { paramStream << parameterType; }
		functionStream << paramStream;

		// Print the function return type.
		if(getArity(functionType->ret))
		{
			auto resultStream = createTaggedSubtree(Symbol::_result);
			resultStream << functionType->ret;
			functionStream << resultStream;
		}

		// Print the function's locals.
		for(uintptr nonParameterLocalIndex = 0;nonParameterLocalIndex < function.nonParameterLocalTypes.size();++nonParameterLocalIndex)
		{
			auto localStream = createTaggedSubtree(Symbol::_local);
			//localStream << functionContext.getLocalName(functionType.parameters.size() + nonParameterLocalIndex);
			localStream << function.nonParameterLocalTypes[nonParameterLocalIndex];
			functionStream << localStream;
		}

		// Print the function's expression.
		//functionStream << dispatch(functionContext,function.expression,returnType);

		return functionStream;
	}
	
	SNodeOutputStream ModulePrintContext::printFunctionType(const FunctionType* functionType)
	{
		auto functionStream = createTaggedSubtree(Symbol::_func);

		auto paramStream = createTaggedSubtree(Symbol::_param);
		for(auto parameterType : functionType->parameters) { paramStream << parameterType; }
		functionStream << paramStream;
		
		if(getArity(functionType->ret))
		{
			auto resultStream = createTaggedSubtree(Symbol::_result);
			resultStream << functionType->ret;
			functionStream << resultStream;
		}
		
		return functionStream;
	}

	SNodeOutputStream ModulePrintContext::print()
	{
		auto moduleStream = createTaggedSubtree(Symbol::_module);
		
		// Print the module memory declarations and data segments.
		for(auto& memory : module.memoryDefs)
		{
			auto memoryStream = createTaggedSubtree(Symbol::_memory);
			memoryStream << memory.size.min << memory.size.max;
			moduleStream << memoryStream;
		}
		for(auto dataSegment : module.dataSegments)
		{
			auto segmentStream = createTaggedSubtree(Symbol::_data);
			//todo: base address
			// segmentStream << (dataSegment.baseAddress + offset);
			segmentStream << SNodeOutputStream::StringAtom((const char*)dataSegment.data.data(),dataSegment.data.size());
			moduleStream << segmentStream;
		}

		#if 0
		// Print the module imports.
		for(uintptr importFunctionIndex = 0;importFunctionIndex < module.imports.size();++importFunctionIndex)
		{
			const auto& import = module.imports[importFunctionIndex];
			auto importStream = createTaggedSubtree(Symbol::_import);
			importStream << getFunctionImportName(importFunctionIndex);
			
			importStream << SNodeOutputStream::StringAtom(import.module);
			importStream << SNodeOutputStream::StringAtom(import.exportName);

			auto paramStream = createTaggedSubtree(Symbol::_param);
			const auto& importType = module.types[import.typeIndex];
			for(auto parameterType : importType.parameters) { paramStream << parameterType; }
			importStream << paramStream;

			if(getArity(importType.ret))
			{
				auto resultStream = createTaggedSubtree(Symbol::_result);
				for(auto returnType : importType.ret) { resultStream << returnType; }
				importStream << resultStream;
			}
			moduleStream << importStream;
		}

		// Print the indirect call signatures.
		for(uintptr typeIndex = 0;typeIndex < module.types.size();++typeIndex)
		{
			const FunctionType* functionType = module.types[typeIndex];
			moduleStream << (createTaggedSubtree(Symbol::_type)
				<< getTypeName(typeIndex)
				<< printFunctionType(functionType));
		}

		// Print the module function table.
		if (module.functionTable.size() > 0)
		{
			auto tableStream = createTaggedSubtree(Symbol::_table);
			for(uintptr elementIndex = 0;elementIndex < module.functionTable.size();++elementIndex)
			{ tableStream << getFunctionName(module.functionTable[elementIndex]); }
			moduleStream << tableStream;
		}

		// Print the module exports.
		for(auto functionExport : module.functionExports)
		{
			auto exportStream = createTaggedSubtree(Symbol::_export);
			exportStream << SNodeOutputStream::StringAtom(functionExport.name);
			exportStream << getFunctionName(functionExport.functionIndex);
			moduleStream << exportStream;
		}

		// Print the module functions.
		for(uintptr functionIndex = 0;functionIndex < module.functionDefs.size();++functionIndex)
		{
			moduleStream << printFunction(functionIndex);
		}
		#endif

		return moduleStream;
	}
#endif

	std::string print(const Module& module)
	{
		return "";
	}
}

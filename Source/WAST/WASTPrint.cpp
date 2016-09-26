#include "Core/Core.h"
#include "Core/MemoryArena.h"
#include "Core/Floats.h"
#include "WAST.h"
#include "WASTSymbols.h"
#include "WebAssembly/Module.h"
#include "WebAssembly/Operations.h"

#include <map>

using namespace WebAssembly;

namespace WAST
{
	#define INDENT_STRING "\xE0\x01"
	#define DEDENT_STRING "\xE0\x02"

	char nibbleToHexChar(uint8 value) { return value < 10 ? ('0' + value) : 'a' + value - 10; }

	std::string escapeString(const char* string,size_t numChars)
	{
		std::string result;
		for(uintp charIndex = 0;charIndex < numChars;++charIndex)
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

	std::string expandIndentation(std::string&& inString,uint8 spacesPerIndentLevel = 2)
	{
		std::string paddedInput = std::move(inString);
		paddedInput += '\0';

		std::string result;
		const char* next = paddedInput.data();
		const char* end = paddedInput.data() + paddedInput.size() - 1;
		uintp indentDepth = 0;
		while(next < end)
		{
			// Absorb INDENT_STRING and DEDENT_STRING, but keep track of the indentation depth,
			// and insert a proportional number of spaces following newlines. 
			if(*(uint16*)next == *(uint16*)INDENT_STRING) { ++indentDepth; next += 2; }
			else if(*(uint16*)next == *(uint16*)DEDENT_STRING) { errorUnless(indentDepth > 0); --indentDepth; next += 2; }
			else if(*next == '\n')
			{
				result += '\n';
				result.insert(result.end(),indentDepth*2,' ');
				++next;
			}
			else { result += *next++; }
		}
		return result;
	}
	
	struct ScopedTagPrinter
	{
		ScopedTagPrinter(std::string& inString,const char* tag): string(inString)
		{
			string += "(";
			string += tag;
			string += INDENT_STRING;
		}

		~ScopedTagPrinter()
		{
			string += DEDENT_STRING ")";
		}

	private:
		std::string& string;
	};

	void print(std::string& string,ValueType type) { string += asString(type); }
	void print(std::string& string,ResultType type) { string += asString(type); }
	void print(std::string& string,GlobalType type)
	{
		if(type.isMutable) { string += "(mut "; }
		print(string,type.valueType);
		if(type.isMutable) { string += ")"; }
	}

	void printSignature(std::string& string,const FunctionType* functionType)
	{
		// Print the function parameters.
		if(functionType->parameters.size())
		{
			ScopedTagPrinter paramTag(string,"param");
			for(uintp parameterIndex = 0;parameterIndex < functionType->parameters.size();++parameterIndex)
			{
				string += ' ';
				print(string,functionType->parameters[parameterIndex]);
			}
		}

		// Print the function return type.
		if(functionType->ret != ResultType::none)
		{
			string += ' ';
			ScopedTagPrinter resultTag(string,"result");
			string += ' ';
			print(string,functionType->ret);
		}
	}

	void printControlSignature(std::string& string,ResultType resultType)
	{
		if(resultType != ResultType::none)
		{
			string += ' ';
			print(string,resultType);
		}
	}

	void print(std::string& string,const SizeConstraints& size)
	{
		string += std::to_string(size.min);
		if(size.max != UINT64_MAX) { string += ' '; string += std::to_string(size.max); }
	}

	struct NameScope
	{
		NameScope(const char inSigil): sigil(inSigil) { nameToCountMap[""] = 0; }
		
		void map(std::string& name)
		{
			auto mapIt = nameToCountMap.find(name);
			if(mapIt == nameToCountMap.end()) { nameToCountMap[name] = 1; }
			else { name = name + std::to_string(mapIt->second++); }
			name = sigil + name;
		}

	private:

		char sigil;
		std::map<std::string,uintp> nameToCountMap;
	};

	struct ModulePrintContext
	{
		const Module& module;
		std::string& string;
		
		DisassemblyNames names;

		ModulePrintContext(const Module& inModule,std::string& inString)
		: module(inModule), string(inString)
		{
			// Start with the names from the module's user name section, but make sure they are unique, and add the "$" sigil.
			getDisassemblyNames(module,names);
			NameScope globalNameScope('$');
			for(auto& name : names.types) { globalNameScope.map(name); }
			for(auto& name : names.functions) { globalNameScope.map(name); }
			for(auto& name : names.tables) { globalNameScope.map(name); }
			for(auto& name : names.memories) { globalNameScope.map(name); }
			for(auto& name : names.globals) { globalNameScope.map(name); }
			for(auto& functionDef : names.functionDefs)
			{
				NameScope localNameScope('$');
				for(auto& name : functionDef.locals) { localNameScope.map(name); }
			}
		}

		void printModule();
		
		void printInitializerExpression(const InitializerExpression& expression)
		{
			switch(expression.type)
			{
			case InitializerExpression::Type::i32_const: string += "(i32.const " + std::to_string(expression.i32) + ')'; break;
			case InitializerExpression::Type::i64_const: string += "(i64.const " + std::to_string(expression.i64) + ')'; break;
			case InitializerExpression::Type::f32_const: string += "(f32.const " + Floats::asString(expression.f32) + ')'; break;
			case InitializerExpression::Type::f64_const: string += "(f64.const " + Floats::asString(expression.f64) + ')'; break;
			case InitializerExpression::Type::get_global: string += "(get_global " + names.globals[expression.globalIndex] + ')'; break;
			default: Core::unreachable();
			};
		}
	};
	
	struct FunctionPrintContext
	{
		ModulePrintContext& moduleContext;
		const Module& module;
		const Function& functionDef;
		const FunctionType* functionType;
		std::string& string;

		const std::vector<std::string>& localNames;
		NameScope labelNameScope;

		FunctionPrintContext(ModulePrintContext& inModuleContext,uintp functionDefIndex)
		: moduleContext(inModuleContext)
		, module(inModuleContext.module)
		, functionDef(inModuleContext.module.functionDefs[functionDefIndex])
		, functionType(inModuleContext.module.types[functionDef.typeIndex])
		, string(inModuleContext.string)
		, localNames(inModuleContext.names.functionDefs[functionDefIndex].locals)
		, labelNameScope('$')
		{}

		void printFunctionBody();
		
		void unknown(Opcode)
		{
			Core::unreachable();
		}
		void beginBlock(ControlStructureImm imm)
		{
			string += "\nblock";
			printControlSignature(string,imm.resultType);
			pushControlStack(ControlContext::Type::block,"block");
		}
		void beginLoop(ControlStructureImm imm)
		{
			string += "\nloop";
			printControlSignature(string,imm.resultType);
			pushControlStack(ControlContext::Type::loop,"loop");
		}
		void beginIf(ControlStructureImm imm)
		{
			string += "\nif";
			printControlSignature(string,imm.resultType);
			pushControlStack(ControlContext::Type::ifThen,"if");
		}
		void beginElse(NoImm imm)
		{
			popControlStack(true);
		}
		void end(NoImm)
		{
			popControlStack();
		}
		
		void ret(NoImm)
		{
			string += "\nreturn";
			enterUnreachable();
		}

		void br(BranchImm imm)
		{
			string += "\nbr " + getBranchTargetId(imm.targetDepth);
			enterUnreachable();
		}
		void br_table(BranchTableImm imm)
		{
			string += "\nbr_table" INDENT_STRING;
			enum { numTargetsPerLine = 16 };
			for(uintp targetIndex = 0;targetIndex < imm.targetDepths.size();++targetIndex)
			{
				if(targetIndex % numTargetsPerLine == 0) { string += '\n'; }
				else { string += ' '; }
				string += getBranchTargetId(imm.targetDepths[targetIndex]);
			}
			string += '\n';
			string += getBranchTargetId(imm.defaultTargetDepth);
			string += " ;; default" DEDENT_STRING;

			enterUnreachable();
		}
		void br_if(BranchImm imm)
		{
			string += "\nbr_if " + getBranchTargetId(imm.targetDepth);
		}

		void nop(NoImm) { string += "\nnop"; }
		void unreachable(NoImm) { string += "\nunreachable"; enterUnreachable(); }
		void drop(NoImm) { string += "\ndrop"; }

		void select(NoImm)
		{
			string += "\nselect";
		}

		void get_local(GetOrSetVariableImm imm)
		{
			string += "\nget_local " + localNames[imm.variableIndex];
		}
		void set_local(GetOrSetVariableImm imm)
		{
			string += "\nset_local " + localNames[imm.variableIndex];
		}
		void tee_local(GetOrSetVariableImm imm)
		{
			string += "\ntee_local " + localNames[imm.variableIndex];
		}
		
		void get_global(GetOrSetVariableImm imm)
		{
			string += "\nget_global " + moduleContext.names.globals[imm.variableIndex];
		}
		void set_global(GetOrSetVariableImm imm)
		{
			string += "\nset_global " + moduleContext.names.globals[imm.variableIndex];
		}

		void call(CallImm imm)
		{
			string += "\ncall " + moduleContext.names.functions[imm.functionIndex];
		}
		void call_indirect(CallIndirectImm imm)
		{
			string += "\ncall_indirect " + moduleContext.names.types[imm.typeIndex];
		}

		void grow_memory(NoImm) { string += "\ngrow_memory"; }
		void current_memory(NoImm) { string += "\ncurrent_memory"; }

		void error(ErrorImm imm) { string += "\nerror \"" + escapeString(imm.message.data(),imm.message.size()) + "\""; popControlStack(); }

		#define PRINT_CONST(typeId,nativeType,toString) \
			void typeId##_const(LiteralImm<nativeType> imm) { string += "\n" #typeId; string += ".const "; string += toString(imm.value); }
		PRINT_CONST(i32,int32,std::to_string); PRINT_CONST(i64,int64,std::to_string);
		PRINT_CONST(f32,float32,Floats::asString); PRINT_CONST(f64,float64,Floats::asString);

		#define PRINT_LOAD_OPCODE(typeId,name,naturalAlignmentLog2,resultType) void typeId##_##name(LoadOrStoreImm imm) \
			{ \
				string += "\n" #typeId "." #name " align="; \
				string += std::to_string(1 << imm.alignmentLog2); \
				if(imm.offset != 0) { string += " offset=" + std::to_string(imm.offset); } \
			}

		PRINT_LOAD_OPCODE(i32,load8_s,1,i32)  PRINT_LOAD_OPCODE(i32,load8_u,1,i32)
		PRINT_LOAD_OPCODE(i32,load16_s,2,i32) PRINT_LOAD_OPCODE(i32,load16_u,2,i32)
		PRINT_LOAD_OPCODE(i64,load8_s,1,i64)  PRINT_LOAD_OPCODE(i64,load8_u,1,i64)
		PRINT_LOAD_OPCODE(i64,load16_s,2,i64)  PRINT_LOAD_OPCODE(i64,load16_u,2,i64)
		PRINT_LOAD_OPCODE(i64,load32_s,4,i64)  PRINT_LOAD_OPCODE(i64,load32_u,4,i64)

		PRINT_LOAD_OPCODE(i32,load,4,i32) PRINT_LOAD_OPCODE(i64,load,8,i64)
		PRINT_LOAD_OPCODE(f32,load,4,f32) PRINT_LOAD_OPCODE(f64,load,8,f64)
			
		#define PRINT_STORE_OPCODE(typeId,name,naturalAlignmentLog2,valueTypeId) void typeId##_##name(LoadOrStoreImm imm) \
			{ \
				string += "\n" #typeId "." #name " align="; \
				string += std::to_string(1 << imm.alignmentLog2); \
				if(imm.offset != 0) { string += " offset=" + std::to_string(imm.offset); } \
			}

		PRINT_STORE_OPCODE(i32,store8,1,i32) PRINT_STORE_OPCODE(i32,store16,2,i32) PRINT_STORE_OPCODE(i32,store,4,i32)
		PRINT_STORE_OPCODE(i64,store8,1,i64) PRINT_STORE_OPCODE(i64,store16,2,i64) PRINT_STORE_OPCODE(i64,store32,4,i64) PRINT_STORE_OPCODE(i64,store,8,i64)
		PRINT_STORE_OPCODE(f32,store,4,f32) PRINT_STORE_OPCODE(f64,store,8,f64)

		#define PRINT_CONVERSION_OPCODE(name,operandTypeId,resultTypeId) void resultTypeId##_##name##_##operandTypeId(NoImm) \
			{ string += "\n" #resultTypeId "." #name "/" #operandTypeId; }
		#define PRINT_COMPARE_OPCODE(name,operandTypeId,resultTypeId) void operandTypeId##_##name(NoImm) \
			{ string += "\n" #operandTypeId "." #name; }
		#define PRINT_BASIC_OPCODE(name,operandTypeId,resultTypeId) void resultTypeId##_##name(NoImm) \
			{ string += "\n" #resultTypeId "." #name; }

		PRINT_BASIC_OPCODE(add,i32,i32) PRINT_BASIC_OPCODE(add,i64,i64)
		PRINT_BASIC_OPCODE(sub,i32,i32) PRINT_BASIC_OPCODE(sub,i64,i64)
		PRINT_BASIC_OPCODE(mul,i32,i32) PRINT_BASIC_OPCODE(mul,i64,i64)
		PRINT_BASIC_OPCODE(div_s,i32,i32) PRINT_BASIC_OPCODE(div_s,i64,i64)
		PRINT_BASIC_OPCODE(div_u,i32,i32) PRINT_BASIC_OPCODE(div_u,i64,i64)
		PRINT_BASIC_OPCODE(rem_s,i32,i32) PRINT_BASIC_OPCODE(rem_s,i64,i64)
		PRINT_BASIC_OPCODE(rem_u,i32,i32) PRINT_BASIC_OPCODE(rem_u,i64,i64)
		PRINT_BASIC_OPCODE(and,i32,i32) PRINT_BASIC_OPCODE(and,i64,i64)
		PRINT_BASIC_OPCODE(or,i32,i32) PRINT_BASIC_OPCODE(or,i64,i64)
		PRINT_BASIC_OPCODE(xor,i32,i32) PRINT_BASIC_OPCODE(xor,i64,i64)
		PRINT_BASIC_OPCODE(shl,i32,i32) PRINT_BASIC_OPCODE(shl,i64,i64)
		PRINT_BASIC_OPCODE(shr_u,i32,i32) PRINT_BASIC_OPCODE(shr_u,i64,i64)
		PRINT_BASIC_OPCODE(shr_s,i32,i32) PRINT_BASIC_OPCODE(shr_s,i64,i64)
		PRINT_BASIC_OPCODE(rotr,i32,i32) PRINT_BASIC_OPCODE(rotr,i64,i64)
		PRINT_BASIC_OPCODE(rotl,i32,i32) PRINT_BASIC_OPCODE(rotl,i64,i64)

		PRINT_COMPARE_OPCODE(eq,i32,i32) PRINT_COMPARE_OPCODE(eq,i64,i32)
		PRINT_COMPARE_OPCODE(ne,i32,i32) PRINT_COMPARE_OPCODE(ne,i64,i32)
		PRINT_COMPARE_OPCODE(lt_s,i32,i32) PRINT_COMPARE_OPCODE(lt_s,i64,i32)
		PRINT_COMPARE_OPCODE(le_s,i32,i32) PRINT_COMPARE_OPCODE(le_s,i64,i32)
		PRINT_COMPARE_OPCODE(lt_u,i32,i32) PRINT_COMPARE_OPCODE(lt_u,i64,i32)
		PRINT_COMPARE_OPCODE(le_u,i32,i32) PRINT_COMPARE_OPCODE(le_u,i64,i32)
		PRINT_COMPARE_OPCODE(gt_s,i32,i32) PRINT_COMPARE_OPCODE(gt_s,i64,i32)
		PRINT_COMPARE_OPCODE(ge_s,i32,i32) PRINT_COMPARE_OPCODE(ge_s,i64,i32)
		PRINT_COMPARE_OPCODE(gt_u,i32,i32) PRINT_COMPARE_OPCODE(gt_u,i64,i32)
		PRINT_COMPARE_OPCODE(ge_u,i32,i32) PRINT_COMPARE_OPCODE(ge_u,i64,i32)
		PRINT_COMPARE_OPCODE(eqz,i32,i32) PRINT_COMPARE_OPCODE(eqz,i64,i32)

		PRINT_BASIC_OPCODE(clz,i32,i32) PRINT_BASIC_OPCODE(clz,i64,i64)
		PRINT_BASIC_OPCODE(ctz,i32,i32) PRINT_BASIC_OPCODE(ctz,i64,i64)
		PRINT_BASIC_OPCODE(popcnt,i32,i32) PRINT_BASIC_OPCODE(popcnt,i64,i64)

		PRINT_BASIC_OPCODE(add,f32,f32) PRINT_BASIC_OPCODE(add,f64,f64)
		PRINT_BASIC_OPCODE(sub,f32,f32) PRINT_BASIC_OPCODE(sub,f64,f64)
		PRINT_BASIC_OPCODE(mul,f32,f32) PRINT_BASIC_OPCODE(mul,f64,f64)
		PRINT_BASIC_OPCODE(div,f32,f32) PRINT_BASIC_OPCODE(div,f64,f64)
		PRINT_BASIC_OPCODE(min,f32,f32) PRINT_BASIC_OPCODE(min,f64,f64)
		PRINT_BASIC_OPCODE(max,f32,f32) PRINT_BASIC_OPCODE(max,f64,f64)
		PRINT_BASIC_OPCODE(copysign,f32,f32) PRINT_BASIC_OPCODE(copysign,f64,f64)

		PRINT_COMPARE_OPCODE(eq,f32,i32) PRINT_COMPARE_OPCODE(eq,f64,i32)
		PRINT_COMPARE_OPCODE(ne,f32,i32) PRINT_COMPARE_OPCODE(ne,f64,i32)
		PRINT_COMPARE_OPCODE(lt,f32,i32) PRINT_COMPARE_OPCODE(lt,f64,i32)
		PRINT_COMPARE_OPCODE(le,f32,i32) PRINT_COMPARE_OPCODE(le,f64,i32)
		PRINT_COMPARE_OPCODE(gt,f32,i32) PRINT_COMPARE_OPCODE(gt,f64,i32)
		PRINT_COMPARE_OPCODE(ge,f32,i32) PRINT_COMPARE_OPCODE(ge,f64,i32)

		PRINT_BASIC_OPCODE(abs,f32,f32) PRINT_BASIC_OPCODE(abs,f64,f64)
		PRINT_BASIC_OPCODE(neg,f32,f32) PRINT_BASIC_OPCODE(neg,f64,f64)
		PRINT_BASIC_OPCODE(ceil,f32,f32) PRINT_BASIC_OPCODE(ceil,f64,f64)
		PRINT_BASIC_OPCODE(floor,f32,f32) PRINT_BASIC_OPCODE(floor,f64,f64)
		PRINT_BASIC_OPCODE(trunc,f32,f32) PRINT_BASIC_OPCODE(trunc,f64,f64)
		PRINT_BASIC_OPCODE(nearest,f32,f32) PRINT_BASIC_OPCODE(nearest,f64,f64)
		PRINT_BASIC_OPCODE(sqrt,f32,f32) PRINT_BASIC_OPCODE(sqrt,f64,f64)

		PRINT_CONVERSION_OPCODE(trunc_s,f32,i32)
		PRINT_CONVERSION_OPCODE(trunc_s,f64,i32)
		PRINT_CONVERSION_OPCODE(trunc_u,f32,i32)
		PRINT_CONVERSION_OPCODE(trunc_u,f64,i32)
		PRINT_CONVERSION_OPCODE(wrap,i64,i32)
		PRINT_CONVERSION_OPCODE(trunc_s,f32,i64)
		PRINT_CONVERSION_OPCODE(trunc_s,f64,i64)
		PRINT_CONVERSION_OPCODE(trunc_u,f32,i64)
		PRINT_CONVERSION_OPCODE(trunc_u,f64,i64)
		PRINT_CONVERSION_OPCODE(extend_s,i32,i64)
		PRINT_CONVERSION_OPCODE(extend_u,i32,i64)
		PRINT_CONVERSION_OPCODE(convert_s,i32,f32)
		PRINT_CONVERSION_OPCODE(convert_u,i32,f32)
		PRINT_CONVERSION_OPCODE(convert_s,i64,f32)
		PRINT_CONVERSION_OPCODE(convert_u,i64,f32)
		PRINT_CONVERSION_OPCODE(demote,f64,f32)
		PRINT_CONVERSION_OPCODE(reinterpret,i32,f32)
		PRINT_CONVERSION_OPCODE(convert_s,i32,f64)
		PRINT_CONVERSION_OPCODE(convert_u,i32,f64)
		PRINT_CONVERSION_OPCODE(convert_s,i64,f64)
		PRINT_CONVERSION_OPCODE(convert_u,i64,f64)
		PRINT_CONVERSION_OPCODE(promote,f32,f64)
		PRINT_CONVERSION_OPCODE(reinterpret,i64,f64)
		PRINT_CONVERSION_OPCODE(reinterpret,f32,i32)
		PRINT_CONVERSION_OPCODE(reinterpret,f64,i64)

	private:
		
		struct ControlContext
		{
			enum class Type : uint8
			{
				function,
				block,
				ifThen,
				ifElse,
				loop
			};
			Type type;
			std::string labelId;
		};

		std::vector<ControlContext> controlStack;

		std::string getBranchTargetId(uintp depth)
		{
			const ControlContext& controlContext = controlStack[controlStack.size() - depth - 1];
			return controlContext.type == ControlContext::Type::function ? std::to_string(depth) : controlContext.labelId;
		}

		void pushControlStack(ControlContext::Type type,const char* labelIdBase)
		{
			std::string labelId;
			if(type != ControlContext::Type::function)
			{
				labelId = labelIdBase;
				labelNameScope.map(labelId);
				string += ' ';
				string += labelId;
			}
			controlStack.push_back({type,labelId});
			string += INDENT_STRING;
		}

		void popControlStack(bool isElse = false)
		{
			string += DEDENT_STRING;
			if(isElse)
			{
				controlStack.back().type = ControlContext::Type::ifElse;
				string += "\nelse" INDENT_STRING;
			}
			else
			{
				if(controlStack.back().type != ControlContext::Type::function) { string += "\nend ;; "; string += controlStack.back().labelId; }
				controlStack.pop_back();
			}
		}

		void enterUnreachable()
		{}
	};

	void ModulePrintContext::printModule()
	{
		ScopedTagPrinter moduleTag(string,"module");
		
		// Print the types.
		for(uintp typeIndex = 0;typeIndex < module.types.size();++typeIndex)
		{
			string += '\n';
			ScopedTagPrinter typeTag(string,"type");
			string += ' ';
			string += names.types[typeIndex];
			string += ' ';
			printSignature(string,module.types[typeIndex]);
		}

		// Print the module imports.
		{
			uintp importedFunctionIndex = 0;
			uintp importedTableIndex = 0;
			uintp importedMemoryIndex = 0;
			uintp importedGlobalIndex = 0;
			for(uintp importIndex = 0;importIndex < module.imports.size();++importIndex)
			{
				const Import& import = module.imports[importIndex];
			
				string += '\n';
				ScopedTagPrinter importTag(string,"import");
				string += " \"";
				string += escapeString(import.module.c_str(),import.module.length());
				string += "\" \"";
				string += escapeString(import.exportName.c_str(),import.exportName.length());
				string += "\" ";
				const char* typeTag;
				std::string typeBody;
				const char* importName;
				switch(import.type.kind)
				{
				case ObjectKind::function: typeTag = "func"; importName = names.functions[importedFunctionIndex++].c_str(); printSignature(typeBody,module.types[import.type.functionTypeIndex]); break;
				case ObjectKind::table: typeTag = "table"; importName = names.tables[importedTableIndex++].c_str(); print(typeBody,import.type.table.size); typeBody += " anyfunc"; break;
				case ObjectKind::memory: typeTag = "memory"; importName = names.memories[importedMemoryIndex++].c_str(); print(typeBody,import.type.memory.size); break;
				case ObjectKind::global: typeTag = "global"; importName = names.globals[importedGlobalIndex++].c_str(); print(typeBody,import.type.global); break;
				default: Core::unreachable();
				};
				string += '(';
				string += typeTag;
				string += ' ';
				string += importName;
				string += ' ';
				string += typeBody;
				string += ')';
			}
		}
		// Print the module exports.
		for(auto export_ : module.exports)
		{
			string += '\n';
			ScopedTagPrinter exportTag(string,"export");
			string += " \"";
			string += escapeString(export_.name.c_str(),export_.name.length());
			string += "\" (";
			switch(export_.kind)
			{
			case ObjectKind::function: string += "func " + names.functions[export_.index]; break;
			case ObjectKind::table: string += "table " + names.tables[export_.index]; break;
			case ObjectKind::memory: string += "memory " + names.memories[export_.index]; break;
			case ObjectKind::global: string += "global " + names.globals[export_.index]; break;
			default: Core::unreachable();
			};
			string += ')';
		}

		// Print the module memory definitions.
		for(uintp memoryDefIndex = 0;memoryDefIndex < module.memoryDefs.size();++memoryDefIndex)
		{
			const MemoryType& memory = module.memoryDefs[memoryDefIndex];
			string += '\n';
			ScopedTagPrinter memoryTag(string,"memory");
			string += ' ';
			string += names.memories[names.memories.size() - module.memoryDefs.size() + memoryDefIndex];
			string += ' ';
			string += std::to_string(memory.size.min);
			if(memory.size.max != UINT64_MAX) { string += std::to_string(memory.size.max); }
		}
		
		// Print the module table definitions and elem segments.
		for(uintp tableDefIndex = 0;tableDefIndex < module.tableDefs.size();++tableDefIndex)
		{
			const TableType& table = module.tableDefs[tableDefIndex];
			string += '\n';
			ScopedTagPrinter memoryTag(string,"table");
			string += ' ';
			string += names.tables[names.tables.size() - module.tableDefs.size() + tableDefIndex];
			string += ' ';
			string += std::to_string(table.size.min);
			if(table.size.max != UINT64_MAX) { string += std::to_string(table.size.max); }
			string += " anyfunc";
		}
		
		// Print the module global definitions.
		for(uintp globalDefIndex = 0;globalDefIndex < module.globalDefs.size();++globalDefIndex)
		{
			const Global& global = module.globalDefs[globalDefIndex];
			string += '\n';
			ScopedTagPrinter memoryTag(string,"global");
			string += ' ';
			string += names.globals[names.globals.size() - module.globalDefs.size() + globalDefIndex];
			string += ' ';
			print(string,global.type);
			string += ' ';
			printInitializerExpression(global.initializer);
		}
		
		// Print the data and table segment definitions.
		for(auto tableSegment : module.tableSegments)
		{
			string += '\n';
			ScopedTagPrinter dataTag(string,"elem");
			string += ' ';
			string += names.tables[tableSegment.tableIndex];
			string += ' ';
			printInitializerExpression(tableSegment.baseOffset);
			enum { numElemsPerLine = 8 };
			for(uintp elementIndex = 0;elementIndex < tableSegment.indices.size();++elementIndex)
			{
				if(elementIndex % numElemsPerLine == 0) { string += '\n'; }
				else { string += ' '; }
				string += names.functions[tableSegment.indices[elementIndex]];
			}
		}
		for(auto dataSegment : module.dataSegments)
		{
			string += '\n';
			ScopedTagPrinter dataTag(string,"data");
			string += ' ';
			string += names.memories[dataSegment.memoryIndex];
			string += ' ';
			printInitializerExpression(dataSegment.baseOffset);
			enum { numBytesPerLine = 64 };
			for(uintp offset = 0;offset < dataSegment.data.size();offset += numBytesPerLine)
			{
				string += "\n\"";
				string += escapeString((const char*)dataSegment.data.data() + offset,std::min(dataSegment.data.size() - offset,(uintp)numBytesPerLine));
				string += "\"";
			}
		}

		const uintp baseFunctionDefIndex = names.functions.size() - names.functionDefs.size();
		for(uintp functionDefIndex = 0;functionDefIndex < module.functionDefs.size();++functionDefIndex)
		{
			const uintp functionIndex = baseFunctionDefIndex + functionDefIndex;
			const Function& functionDef = module.functionDefs[functionDefIndex];
			const FunctionType* functionType = module.types[functionDef.typeIndex];
			FunctionPrintContext functionContext(*this,functionDefIndex);
		
			string += "\n\n";
			ScopedTagPrinter funcTag(string,"func");

			string += ' ';
			string += names.functions[functionIndex];
			
			// Print the function parameters.
			if(functionType->parameters.size())
			{
				for(uintp parameterIndex = 0;parameterIndex < functionType->parameters.size();++parameterIndex)
				{
					string += '\n';
					ScopedTagPrinter paramTag(string,"param");
					string += ' ';
					string += functionContext.localNames[parameterIndex];
					string += ' ';
					print(string,functionType->parameters[parameterIndex]);
				}
			}

			// Print the function return type.
			if(functionType->ret != ResultType::none)
			{
				string += '\n';
				ScopedTagPrinter resultTag(string,"result");
				string += ' ';
				print(string,functionType->ret);
			}

			// Print the function's locals.
			for(uintp localIndex = 0;localIndex < functionDef.nonParameterLocalTypes.size();++localIndex)
			{
				string += '\n';
				ScopedTagPrinter localTag(string,"local");
				string += ' ';
				string += functionContext.localNames[functionType->parameters.size() + localIndex];
				string += ' ';
				print(string,functionDef.nonParameterLocalTypes[localIndex]);
			}

			functionContext.printFunctionBody();
		}

		// Print user sections (other than the name section).
		for(const auto& userSection : module.userSections)
		{
			if(userSection.name != "name")
			{
				string += '\n';
				ScopedTagPrinter dataTag(string,"user_section");
				string += " \"";
				string += escapeString(userSection.name.c_str(),userSection.name.length());
				string += "\" ";
				enum { numBytesPerLine = 64 };
				for(uintp offset = 0;offset < userSection.data.size();offset += numBytesPerLine)
				{
					string += "\n\"";
					string += escapeString((const char*)userSection.data.data() + offset,std::min(userSection.data.size() - offset,(uintp)numBytesPerLine));
					string += "\"";
				}
			}
		}
	}

	void FunctionPrintContext::printFunctionBody()
	{
		//string += "(";
		pushControlStack(ControlContext::Type::function,nullptr);
		string += DEDENT_STRING;

		Serialization::MemoryInputStream codeStream(module.code.data() + functionDef.code.offset,functionDef.code.numBytes);
		OperationDecoder decoder(codeStream);
		while(decoder && controlStack.size())
		{
			decoder.decodeOp(*this);
		};

		string += INDENT_STRING;
	}

	std::string print(const Module& module)
	{
		std::string string;
		ModulePrintContext context(module,string);
		context.printModule();
		return expandIndentation(std::move(string));
	}
}

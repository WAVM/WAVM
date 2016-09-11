#include "Core/Core.h"
#include "Core/MemoryArena.h"
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

	std::string expandIndentation(std::string&& inString,uint8 spacesPerIndentLevel = 2)
	{
		std::string paddedInput = std::move(inString);
		paddedInput += '\0';

		std::string result;
		const char* next = paddedInput.data();
		const char* end = paddedInput.data() + paddedInput.size() - 1;
		uintptr indentDepth = 0;
		while(next < end)
		{
			if(*(uint16*)next == *(uint16*)INDENT_STRING) { ++indentDepth; next += 2; }
			else if(*(uint16*)next == *(uint16*)DEDENT_STRING) { assert(indentDepth > 0); --indentDepth; next += 2; }
			else if(*next == '\n')
			{
				result += '\n';
				result.insert(result.end(),indentDepth,'\t');
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

	void print(std::string& string,ValueType type) { string += getTypeName(type); }
	void print(std::string& string,ResultType type) { string += getTypeName(type); }

	void printSignature(std::string& string,const FunctionType* functionType)
	{
		// Print the function parameters.
		if(functionType->parameters.size())
		{
			string += ' ';
			ScopedTagPrinter paramTag(string,"param");
			for(auto parameterType : functionType->parameters) { string += ' '; print(string,parameterType); }
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

	struct ModulePrintContext
	{
		const Module& module;
		std::string& string;

		ModulePrintContext(const Module& inModule,std::string& inString)
		: module(inModule), string(inString) {}

		void printModule();
	};
	
	struct FunctionPrintContext
	{
		const ModulePrintContext& moduleContext;
		const Module& module;
		const Function& functionDef;
		const FunctionType* functionType;
		std::string& string;

		FunctionPrintContext(const ModulePrintContext& inModuleContext,const Function& inFunctionDef)
		: moduleContext(inModuleContext), module(inModuleContext.module), functionDef(inFunctionDef), functionType(inModuleContext.module.types[inFunctionDef.typeIndex]), string(inModuleContext.string)
		{}

		void printFunctionBody();
		
		void unknown(Opcode)
		{
			throw;
		}
		void beginBlock(ControlStructureImm imm)
		{
			string += "\nblock";
			printControlSignature(string,imm.resultType);
			pushControlStack(ControlContextType::block);
		}
		void beginLoop(ControlStructureImm imm)
		{
			string += "\nloop";
			printControlSignature(string,imm.resultType);
			pushControlStack(ControlContextType::loop);
		}
		void beginIf(NoImm)
		{
			string += "\nif";
			pushControlStack(ControlContextType::ifWithoutElse);
		}
		void beginIfElse(ControlStructureImm imm)
		{
			string += "\nif";
			printControlSignature(string,imm.resultType);
			pushControlStack(ControlContextType::ifThen);
		}
		void end(NoImm)
		{
			popControlStack();
		}
		
		void ret(NoImm)
		{
			string += "\nreturn";
			popControlStack();
		}

		void br(BranchImm imm)
		{
			string += "\nbr " + std::to_string(imm.targetDepth);
			popControlStack();
		}
		void br_table(BranchTableImm imm)
		{
			string += "\nbr_table";
			for(auto targetDepth : imm.targetDepths)
			{
				string += ' ';
				string += std::to_string(targetDepth);
			}
			string += ' ';
			string += std::to_string(imm.defaultTargetDepth);

			popControlStack();
		}
		void br_if(BranchImm imm)
		{
			string += "\nbr_if " + std::to_string(imm.targetDepth);
		}

		void nop(NoImm) { string += "\nnop"; }
		void unreachable(NoImm) { string += "\nunreachable"; popControlStack(); }
		void drop(NoImm) { string += "\ndrop"; }

		void select(NoImm)
		{
			string += "\nselect";
		}

		void get_local(GetOrSetVariableImm imm)
		{
			string += "\nget_local " + std::to_string(imm.variableIndex);
		}
		void set_local(GetOrSetVariableImm imm)
		{
			string += "\nset_local " + std::to_string(imm.variableIndex);
		}
		void tee_local(GetOrSetVariableImm imm)
		{
			string += "\ntee_local " + std::to_string(imm.variableIndex);
		}
		
		void get_global(GetOrSetVariableImm imm)
		{
			string += "\nget_global " + std::to_string(imm.variableIndex);
		}
		void set_global(GetOrSetVariableImm imm)
		{
			string += "\nset_global " + std::to_string(imm.variableIndex);
		}

		void call(CallImm imm)
		{
			string += "\ncall " + std::to_string(imm.functionIndex);
		}
		void call_indirect(CallIndirectImm imm)
		{
			string += "\ncall_indirect " + getTypeName(module.types[imm.typeIndex]);
		}

		void grow_memory(NoImm) { string += "\ngrow_memory"; }
		void current_memory(NoImm) { string += "\ncurrent_memory"; }

		void error(ErrorImm imm) { string += "\nerror \"" + escapeString(imm.message.data(),imm.message.size()) + "\""; popControlStack(); }

		#define PRINT_CONST(typeId,nativeType) \
			void typeId##_const(LiteralImm<nativeType> imm) { string += "\n" #typeId; string += ".const "; string += std::to_string(imm.value); }
		PRINT_CONST(i32,int32); PRINT_CONST(i64,int64);
		PRINT_CONST(f32,float32); PRINT_CONST(f64,float64);

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
		
		enum class ControlContextType : uint8
		{
			function,
			block,
			ifWithoutElse,
			ifThen,
			ifElse,
			loop
		};

		std::vector<ControlContextType> controlStack;

		void pushControlStack(ControlContextType type)
		{
			controlStack.push_back(type);
			string += INDENT_STRING;
		}

		void popControlStack()
		{
			string += DEDENT_STRING;
			if(controlStack.back() == ControlContextType::ifThen)
			{
				controlStack.back() = ControlContextType::ifElse;
				string += "\nelse" INDENT_STRING;
			}
			else
			{
				if(controlStack.back() != ControlContextType::function) { string += "\nend"; }
				controlStack.pop_back();
			}
		}
	};

	void ModulePrintContext::printModule()
	{
		ScopedTagPrinter moduleTag(string,"module");
		
		// Print the module memory declarations and data segments.
		for(auto& memory : module.memoryDefs)
		{
			string += '\n';
			ScopedTagPrinter memoryTag(string,"memory");
			string += ' ';
			string += std::to_string(memory.size.min);
			if(memory.size.max != UINT64_MAX) { string += std::to_string(memory.size.max); }
		}
		for(auto dataSegment : module.dataSegments)
		{
			string += '\n';
			ScopedTagPrinter dataTag(string,"data");
			for(uintptr offset = 0;offset < dataSegment.data.size();offset += 32)
			{
				string += "\n\"";
				string += escapeString((const char*)dataSegment.data.data() + offset,std::min(dataSegment.data.size() - offset,(uintptr)32));
				string += "\"";
			}
		}

		for(auto functionDef : module.functionDefs)
		{
			const FunctionType* functionType = module.types[functionDef.typeIndex];
		
			string += "\n\n";
			ScopedTagPrinter funcTag(string,"func");

			// Print the function's parameters and return type.
			printSignature(string,functionType);

			// Print the function's locals.
			for(auto localType : functionDef.nonParameterLocalTypes)
			{
				string += '\n';
				ScopedTagPrinter localTag(string,"local");
				string += ' ';
				print(string,localType);
			}

			FunctionPrintContext functionContext(*this,functionDef);
			functionContext.printFunctionBody();
		}
	}

	void FunctionPrintContext::printFunctionBody()
	{
		//string += "(";
		pushControlStack(ControlContextType::function);
		string += DEDENT_STRING;

		Serialization::InputStream codeStream(module.code.data() + functionDef.code.offset,functionDef.code.numBytes);
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

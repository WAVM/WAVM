#include <stdint.h>
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/HashSet.h"
#include "WAVM/Inline/IsNameChar.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/WASTPrint/WASTPrint.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Serialization;

#define INDENT_STRING "\xE0\x01"
#define DEDENT_STRING "\xE0\x02"

static char nibbleToHexChar(U8 value) { return value < 10 ? ('0' + value) : 'a' + value - 10; }

static std::string escapeString(const char* string, Uptr numChars)
{
	std::string result;
	for(Uptr charIndex = 0; charIndex < numChars; ++charIndex)
	{
		auto c = string[charIndex];
		if(c == '\\') { result += "\\\\"; }
		else if(c == '\"')
		{
			result += "\\\"";
		}
		else if(c == '\n')
		{
			result += "\\n";
		}
		else if(c < 0x20 || c > 0x7e)
		{
			result += '\\';
			result += nibbleToHexChar((c & 0xf0) >> 4);
			result += nibbleToHexChar((c & 0x0f) >> 0);
		}
		else
		{
			result += c;
		}
	}
	return result;
}

static std::string escapeName(const std::string& name)
{
	std::string escapedName;
	for(char c : name)
	{
		if(c == '(') { escapedName += "_lparen_"; }
		else if(c == ')')
		{
			escapedName += "_rparen_";
		}
		else if(c == ' ')
		{
			escapedName += '_';
		}
		else if(!isNameChar(c))
		{
			escapedName += '_';
			escapedName += nibbleToHexChar((c & 0xf0) >> 4);
			escapedName += nibbleToHexChar((c & 0x0f) >> 0);
			escapedName += '_';
		}
		else
		{
			escapedName += c;
		}
	}

	return escapedName;
}

static std::string expandIndentation(std::string&& inString, U8 spacesPerIndentLevel = 2)
{
	std::string paddedInput = std::move(inString);
	paddedInput += '\0';

	std::string result;
	result.reserve(paddedInput.size() * 2);
	const char* next = paddedInput.data();
	const char* end = paddedInput.data() + paddedInput.size() - 1;
	Uptr indentDepth = 0;
	while(next < end)
	{
		// Absorb INDENT_STRING and DEDENT_STRING, but keep track of the indentation depth, and
		// insert a proportional number of spaces following newlines.
		if(next[0] == INDENT_STRING[0] && next[1] == INDENT_STRING[1])
		{
			++indentDepth;
			next += 2;
		}
		else if(next[0] == DEDENT_STRING[0] && next[1] == DEDENT_STRING[1])
		{
			errorUnless(indentDepth > 0);
			--indentDepth;
			next += 2;
		}
		else if(*next == '\n')
		{
			result += '\n';
			result.insert(result.end(), indentDepth * spacesPerIndentLevel, ' ');
			++next;
		}
		else
		{
			result += *next++;
		}
	}
	return result;
}

struct ScopedTagPrinter
{
	ScopedTagPrinter(std::string& inString, const char* tag) : string(inString)
	{
		string += "(";
		string += tag;
		string += INDENT_STRING;
	}

	~ScopedTagPrinter() { string += DEDENT_STRING ")"; }

private:
	std::string& string;
};

static void print(std::string& string, ValueType type) { string += asString(type); }

static void print(std::string& string, const SizeConstraints& size)
{
	string += std::to_string(size.min);
	if(size.max != UINT64_MAX)
	{
		string += ' ';
		string += std::to_string(size.max);
	}
}

static void print(std::string& string, FunctionType functionType)
{
	// Print the function parameters.
	if(functionType.params().size())
	{
		string += ' ';
		ScopedTagPrinter paramTag(string, "param");
		for(Uptr paramIndex = 0; paramIndex < functionType.params().size(); ++paramIndex)
		{
			string += ' ';
			print(string, functionType.params()[paramIndex]);
		}
	}

	// Print the function return types.
	if(functionType.results().size())
	{
		string += ' ';
		ScopedTagPrinter paramTag(string, "result");
		for(Uptr resultIndex = 0; resultIndex < functionType.results().size(); ++resultIndex)
		{
			string += ' ';
			print(string, functionType.results()[resultIndex]);
		}
	}
}

static void print(std::string& string, ReferenceType type)
{
	switch(type)
	{
	case ReferenceType::anyfunc: string += "anyfunc"; break;
	case ReferenceType::anyref: string += "anyref"; break;
	default: Errors::unreachable();
	}
}

static void print(std::string& string, const TableType& type)
{
	string += ' ';
	print(string, type.size);
	if(type.isShared) { string += " shared"; }
	string += ' ';
	print(string, type.elementType);
}

static void print(std::string& string, const MemoryType& type)
{
	string += ' ';
	print(string, type.size);
	if(type.isShared) { string += " shared"; }
}

static void print(std::string& string, GlobalType type)
{
	string += ' ';
	if(type.isMutable) { string += "(mut "; }
	print(string, type.valueType);
	if(type.isMutable) { string += ")"; }
}

static void print(std::string& string, const ExceptionType& type)
{
	for(ValueType param : type.params)
	{
		string += ' ';
		print(string, param);
	}
}

struct NameScope
{
	NameScope(const char inSigil, bool inAllowQuotedNames, Uptr estimatedNumElements)
	: sigil(inSigil)
	, allowQuotedNames(inAllowQuotedNames)
	, nameSet(estimatedNumElements)
	, nameToUniqueIndexMap()
	{
	}

	void map(std::string& name)
	{
		std::string baseName = name.size() ? name + '_' : name;

		// If the name hasn't been taken yet, use it without a suffix.
		// Otherwise, find the first instance of the name with a numeric suffix that isn't taken.
		if(!name.size() || !nameSet.add(name))
		{
			Uptr& numPrecedingDuplicates = nameToUniqueIndexMap.getOrAdd(name, 0);
			do
			{
				name = baseName + std::to_string(numPrecedingDuplicates);
				++numPrecedingDuplicates;
			} while(!nameSet.add(name));
		}

		if(!allowQuotedNames) { name = escapeName(name); }
		else
		{
			bool needsQuotes = false;
			for(char c : name)
			{
				if(!isNameChar(c))
				{
					needsQuotes = true;
					break;
				}
			}
			if(needsQuotes) { name = '\"' + escapeString(name.data(), name.size()) + '\"'; }
		}

		name = sigil + name;
	}

private:
	char sigil;
	bool allowQuotedNames;
	HashSet<std::string> nameSet;
	HashMap<std::string, Uptr> nameToUniqueIndexMap;
};

struct ModulePrintContext
{
	const Module& module;
	std::string& string;

	DisassemblyNames names;

	ModulePrintContext(const Module& inModule, std::string& inString)
	: module(inModule), string(inString)
	{
		// Start with the names from the module's user name section, but make sure they are unique,
		// and add the "$" sigil.
		IR::getDisassemblyNames(module, names);
		const Uptr numGlobalNames = names.types.size() + names.tables.size() + names.memories.size()
									+ names.globals.size() + names.exceptionTypes.size();
		NameScope globalNameScope('$', module.featureSpec.quotedNamesInTextFormat, numGlobalNames);
		for(auto& name : names.types) { globalNameScope.map(name); }
		for(auto& name : names.tables) { globalNameScope.map(name); }
		for(auto& name : names.memories) { globalNameScope.map(name); }
		for(auto& name : names.globals) { globalNameScope.map(name); }
		for(auto& name : names.exceptionTypes) { globalNameScope.map(name); }
		for(auto& function : names.functions)
		{
			globalNameScope.map(function.name);

			NameScope localNameScope(
				'$', module.featureSpec.quotedNamesInTextFormat, function.locals.size());
			for(auto& name : function.locals) { localNameScope.map(name); }
		}
	}

	void printModule();

	void printLinkingSection(const IR::UserSection& linkingSection);

	void printInitializerExpression(const InitializerExpression& expression)
	{
		switch(expression.type)
		{
		case InitializerExpression::Type::i32_const:
			string += "(i32.const " + std::to_string(expression.i32) + ')';
			break;
		case InitializerExpression::Type::i64_const:
			string += "(i64.const " + std::to_string(expression.i64) + ')';
			break;
		case InitializerExpression::Type::f32_const:
			string += "(f32.const " + asString(expression.f32) + ')';
			break;
		case InitializerExpression::Type::f64_const:
			string += "(f64.const " + asString(expression.f64) + ')';
			break;
		case InitializerExpression::Type::v128_const:
			string += "(v128.const " + asString(expression.v128) + ')';
			break;
		case InitializerExpression::Type::get_global:
			string += "(get_global " + names.globals[expression.globalRef] + ')';
			break;
		case InitializerExpression::Type::ref_null: string += "(ref.null)"; break;
		default: Errors::unreachable();
		};
	}
};

struct FunctionPrintContext
{
	typedef void Result;

	ModulePrintContext& moduleContext;
	const Module& module;
	const FunctionDef& functionDef;
	FunctionType functionType;
	std::string& string;

	const std::vector<std::string>& labelNames;
	const std::vector<std::string>& localNames;
	NameScope labelNameScope;
	Uptr labelIndex;

	FunctionPrintContext(ModulePrintContext& inModuleContext, Uptr functionDefIndex)
	: moduleContext(inModuleContext)
	, module(inModuleContext.module)
	, functionDef(inModuleContext.module.functions.defs[functionDefIndex])
	, functionType(inModuleContext.module.types[functionDef.type.index])
	, string(inModuleContext.string)
	, labelNames(inModuleContext.names.functions[module.functions.imports.size() + functionDefIndex]
					 .labels)
	, localNames(inModuleContext.names.functions[module.functions.imports.size() + functionDefIndex]
					 .locals)
	, labelNameScope('$', inModuleContext.module.featureSpec.quotedNamesInTextFormat, 4)
	, labelIndex(0)
	{
	}

	void printFunctionBody();

	void unknown(Opcode) { Errors::unreachable(); }
	void block(ControlStructureImm imm)
	{
		string += "\nblock";
		std::string labelId = printControlLabel("block");
		printControlSignature(imm.type);
		pushControlStack(ControlContext::Type::block, labelId);
	}
	void loop(ControlStructureImm imm)
	{
		string += "\nloop";
		std::string labelId = printControlLabel("loop");
		printControlSignature(imm.type);
		pushControlStack(ControlContext::Type::loop, labelId);
	}
	void if_(ControlStructureImm imm)
	{
		string += "\nif";
		std::string labelId = printControlLabel("if");
		printControlSignature(imm.type);
		pushControlStack(ControlContext::Type::ifThen, labelId);
	}
	void else_(NoImm imm)
	{
		string += DEDENT_STRING;
		controlStack.back().type = ControlContext::Type::ifElse;
		string += "\nelse" INDENT_STRING;
	}
	void end(NoImm)
	{
		string += DEDENT_STRING;
		if(controlStack.back().type != ControlContext::Type::function)
		{
			string += "\nend ;; ";
			string += controlStack.back().labelId;
		}
		controlStack.pop_back();
	}

	void return_(NoImm)
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
		enum
		{
			numTargetsPerLine = 16
		};
		wavmAssert(imm.branchTableIndex < functionDef.branchTables.size());
		const std::vector<Uptr>& targetDepths = functionDef.branchTables[imm.branchTableIndex];
		for(Uptr targetIndex = 0; targetIndex < targetDepths.size(); ++targetIndex)
		{
			if(targetIndex % numTargetsPerLine == 0) { string += '\n'; }
			else
			{
				string += ' ';
			}
			string += getBranchTargetId(targetDepths[targetIndex]);
		}
		string += '\n';
		string += getBranchTargetId(imm.defaultTargetDepth);
		string += " ;; default" DEDENT_STRING;

		enterUnreachable();
	}
	void br_if(BranchImm imm) { string += "\nbr_if " + getBranchTargetId(imm.targetDepth); }

	void unreachable(NoImm)
	{
		string += "\nunreachable";
		enterUnreachable();
	}
	void drop(NoImm) { string += "\ndrop"; }

	void select(NoImm) { string += "\nselect"; }

	void get_local(GetOrSetVariableImm<false> imm)
	{
		string += "\nget_local " + localNames[imm.variableIndex];
	}
	void set_local(GetOrSetVariableImm<false> imm)
	{
		string += "\nset_local " + localNames[imm.variableIndex];
	}
	void tee_local(GetOrSetVariableImm<false> imm)
	{
		string += "\ntee_local " + localNames[imm.variableIndex];
	}

	void get_global(GetOrSetVariableImm<true> imm)
	{
		string += "\nget_global " + moduleContext.names.globals[imm.variableIndex];
	}
	void set_global(GetOrSetVariableImm<true> imm)
	{
		string += "\nset_global " + moduleContext.names.globals[imm.variableIndex];
	}

	void table_get(TableImm imm)
	{
		string += "\ntable.get " + moduleContext.names.tables[imm.tableIndex];
	}
	void table_set(TableImm imm)
	{
		string += "\ntable.set " + moduleContext.names.tables[imm.tableIndex];
	}

	void throw_(ExceptionTypeImm imm)
	{
		string += "\nthrow " + moduleContext.names.exceptionTypes[imm.exceptionTypeIndex];
	}

	void rethrow(RethrowImm imm)
	{
		string += "\nrethrow ";

		Uptr catchDepth = 0;
		for(Uptr targetDepth = controlStack.size() - 1; targetDepth > 0; --targetDepth)
		{
			if(controlStack[targetDepth].type == ControlContext::Type::catch_)
			{
				if(catchDepth == imm.catchDepth)
				{
					string += getBranchTargetId(targetDepth);
					return;
				}
				++catchDepth;
			}
		}

		Errors::unreachable();
	}

	void call(FunctionImm imm)
	{
		string += "\ncall " + moduleContext.names.functions[imm.functionIndex].name;
	}
	void call_indirect(CallIndirectImm imm)
	{
		string += "\ncall_indirect " + moduleContext.names.tables[imm.tableIndex];
		string += " (type " + moduleContext.names.types[imm.type.index] + ')';
	}

	void printControlSignature(IndexedBlockType indexedSignature)
	{
		FunctionType signature = resolveBlockType(module, indexedSignature);
		print(string, signature);
	}

	void printImm(NoImm) {}
	void printImm(MemoryImm imm) { errorUnless(imm.memoryIndex == 0); }
	void printImm(TableImm imm)
	{
		string += ' ';
		string += moduleContext.names.tables[imm.tableIndex];
	}
	void printImm(FunctionImm imm)
	{
		string += ' ';
		string += moduleContext.names.functions[imm.functionIndex].name;
	}

	void printImm(LiteralImm<I32> imm)
	{
		string += ' ';
		string += std::to_string(imm.value);
	}
	void printImm(LiteralImm<I64> imm)
	{
		string += ' ';
		string += std::to_string(imm.value);
	}
	void printImm(LiteralImm<F32> imm)
	{
		string += ' ';
		string += asString(imm.value);
	}
	void printImm(LiteralImm<F64> imm)
	{
		string += ' ';
		string += asString(imm.value);
	}

	template<Uptr naturalAlignmentLog2> void printImm(LoadOrStoreImm<naturalAlignmentLog2> imm)
	{
		if(imm.offset != 0)
		{
			string += " offset=";
			string += std::to_string(imm.offset);
		}
		if(imm.alignmentLog2 != naturalAlignmentLog2)
		{
			string += " align=";
			string += std::to_string(1 << imm.alignmentLog2);
		}
	}

	void printImm(LiteralImm<V128> imm)
	{
		string += ' ';
		string += asString(imm.value);
	}

	template<Uptr numLanes> void printImm(LaneIndexImm<numLanes> imm)
	{
		string += ' ';
		string += std::to_string(imm.laneIndex);
	}

	template<Uptr numLanes> void printImm(ShuffleImm<numLanes> imm)
	{
		string += " (";
		for(Uptr laneIndex = 0; laneIndex < numLanes; ++laneIndex)
		{
			if(laneIndex != 0) { string += ' '; }
			string += std::to_string(imm.laneIndices[laneIndex]);
		}
		string += ')';
	}

	template<Uptr naturalAlignmentLog2>
	void printImm(AtomicLoadOrStoreImm<naturalAlignmentLog2> imm)
	{
		if(imm.offset != 0)
		{
			string += " offset=";
			string += std::to_string(imm.offset);
		}
		wavmAssert(imm.alignmentLog2 == naturalAlignmentLog2);
	}

	void printImm(DataSegmentAndMemImm imm)
	{
		string += " " + std::to_string(imm.dataSegmentIndex);
		string += " " + std::to_string(imm.memoryIndex);
	}
	void printImm(DataSegmentImm imm) { string += " " + std::to_string(imm.dataSegmentIndex); }

	void printImm(ElemSegmentAndTableImm imm)
	{
		string += " " + std::to_string(imm.elemSegmentIndex);
		string += " " + std::to_string(imm.tableIndex);
	}
	void printImm(ElemSegmentImm imm) { string += " " + std::to_string(imm.elemSegmentIndex); }

	void try_(ControlStructureImm imm)
	{
		string += "\ntry";
		std::string labelId = printControlLabel("try");
		pushControlStack(ControlContext::Type::try_, labelId);
		printControlSignature(imm.type);
	}
	void catch_(ExceptionTypeImm imm)
	{
		string += DEDENT_STRING;
		controlStack.back().type = ControlContext::Type::catch_;
		string += "\ncatch ";
		string += moduleContext.names.exceptionTypes[imm.exceptionTypeIndex];
		string += INDENT_STRING;
	}
	void catch_all(NoImm)
	{
		string += DEDENT_STRING;
		controlStack.back().type = ControlContext::Type::catch_;
		string += "\ncatch_all" INDENT_STRING;
	}

#define PRINT_OP(opcode, name, nameString, Imm, printOperands, requiredFeature)                    \
	void name(Imm imm)                                                                             \
	{                                                                                              \
		wavmAssert(module.featureSpec.requiredFeature);                                            \
		string += "\n" nameString;                                                                 \
		printImm(imm);                                                                             \
	}
	ENUM_NONCONTROL_NONPARAMETRIC_OPERATORS(PRINT_OP)
#undef VALIDATE_OP

private:
	struct ControlContext
	{
		enum class Type : U8
		{
			function,
			block,
			ifThen,
			ifElse,
			loop,
			try_,
			catch_,
		};
		Type type;
		std::string labelId;
	};

	std::vector<ControlContext> controlStack;

	std::string getBranchTargetId(Uptr depth)
	{
		const ControlContext& controlContext = controlStack[controlStack.size() - depth - 1];
		return controlContext.type == ControlContext::Type::function ? std::to_string(depth)
																	 : controlContext.labelId;
	}

	std::string printControlLabel(const char* labelIdBase)
	{
		std::string labelId = labelIndex < labelNames.size() ? labelNames[labelIndex] : labelIdBase;
		labelNameScope.map(labelId);
		string += ' ';
		string += labelId;
		++labelIndex;
		return labelId;
	}

	void pushControlStack(ControlContext::Type type, std::string labelId)
	{
		controlStack.push_back({type, labelId});
		string += INDENT_STRING;
	}

	void enterUnreachable() {}
};

template<typename Type> void printImportType(std::string& string, const Module& module, Type type)
{
	print(string, type);
}
template<>
void printImportType<IndexedFunctionType>(std::string& string,
										  const Module& module,
										  IndexedFunctionType type)
{
	print(string, module.types[type.index]);
}

template<typename Type>
void printImport(std::string& string,
				 const Module& module,
				 const Import<Type>& import,
				 Uptr importIndex,
				 const char* name,
				 const char* typeTag)
{
	string += '\n';
	ScopedTagPrinter importTag(string, "import");
	string += " \"";
	string += escapeString(import.moduleName.c_str(), import.moduleName.length());
	string += "\" \"";
	string += escapeString(import.exportName.c_str(), import.exportName.length());
	string += "\" (";
	string += typeTag;
	string += ' ';
	string += name;
	printImportType(string, module, import.type);
	string += ')';
}

void ModulePrintContext::printModule()
{
	ScopedTagPrinter moduleTag(string, "module");

	// Print the types.
	for(Uptr typeIndex = 0; typeIndex < module.types.size(); ++typeIndex)
	{
		string += '\n';
		ScopedTagPrinter typeTag(string, "type");
		string += ' ';
		string += names.types[typeIndex];
		string += " (func";
		print(string, module.types[typeIndex]);
		string += ')';
	}

	// Print the module imports.
	for(Uptr importIndex = 0; importIndex < module.functions.imports.size(); ++importIndex)
	{
		printImport(string,
					module,
					module.functions.imports[importIndex],
					importIndex,
					names.functions[importIndex].name.c_str(),
					"func");
	}
	for(Uptr importIndex = 0; importIndex < module.tables.imports.size(); ++importIndex)
	{
		printImport(string,
					module,
					module.tables.imports[importIndex],
					importIndex,
					names.tables[importIndex].c_str(),
					"table");
	}
	for(Uptr importIndex = 0; importIndex < module.memories.imports.size(); ++importIndex)
	{
		printImport(string,
					module,
					module.memories.imports[importIndex],
					importIndex,
					names.memories[importIndex].c_str(),
					"memory");
	}
	for(Uptr importIndex = 0; importIndex < module.globals.imports.size(); ++importIndex)
	{
		printImport(string,
					module,
					module.globals.imports[importIndex],
					importIndex,
					names.globals[importIndex].c_str(),
					"global");
	}
	for(Uptr importIndex = 0; importIndex < module.exceptionTypes.imports.size(); ++importIndex)
	{
		printImport(string,
					module,
					module.exceptionTypes.imports[importIndex],
					importIndex,
					names.exceptionTypes[importIndex].c_str(),
					"exception_type");
	}
	// Print the module exports.
	for(auto export_ : module.exports)
	{
		string += '\n';
		ScopedTagPrinter exportTag(string, "export");
		string += " \"";
		string += escapeString(export_.name.c_str(), export_.name.length());
		string += "\" (";
		switch(export_.kind)
		{
		case ExternKind::function: string += "func " + names.functions[export_.index].name; break;
		case ExternKind::table: string += "table " + names.tables[export_.index]; break;
		case ExternKind::memory: string += "memory " + names.memories[export_.index]; break;
		case ExternKind::global: string += "global " + names.globals[export_.index]; break;
		case ExternKind::exceptionType:
			string += "exception_type " + names.exceptionTypes[export_.index];
			break;
		default: Errors::unreachable();
		};
		string += ')';
	}

	// Print the module memory definitions.
	for(Uptr defIndex = 0; defIndex < module.memories.defs.size(); ++defIndex)
	{
		const MemoryDef& memoryDef = module.memories.defs[defIndex];
		string += '\n';
		ScopedTagPrinter memoryTag(string, "memory");
		string += ' ';
		string += names.memories[module.memories.imports.size() + defIndex];
		string += ' ';
		print(string, memoryDef.type);
	}

	// Print the module table definitions and elem segments.
	for(Uptr defIndex = 0; defIndex < module.tables.defs.size(); ++defIndex)
	{
		const TableDef& tableDef = module.tables.defs[defIndex];
		string += '\n';
		ScopedTagPrinter memoryTag(string, "table");
		string += ' ';
		string += names.tables[module.tables.imports.size() + defIndex];
		string += ' ';
		print(string, tableDef.type);
	}

	// Print the module global definitions.
	for(Uptr defIndex = 0; defIndex < module.globals.defs.size(); ++defIndex)
	{
		const GlobalDef& globalDef = module.globals.defs[defIndex];
		string += '\n';
		ScopedTagPrinter memoryTag(string, "global");
		string += ' ';
		string += names.globals[module.globals.imports.size() + defIndex];
		string += ' ';
		print(string, globalDef.type);
		string += ' ';
		printInitializerExpression(globalDef.initializer);
	}

	// Print the module exception type definitions.
	for(Uptr defIndex = 0; defIndex < module.exceptionTypes.defs.size(); ++defIndex)
	{
		const ExceptionTypeDef& exceptionTypeDef = module.exceptionTypes.defs[defIndex];
		string += '\n';
		ScopedTagPrinter exceptionTypeTag(string, "exception_type");
		string += ' ';
		string += names.exceptionTypes[module.exceptionTypes.imports.size() + defIndex];
		print(string, exceptionTypeDef.type);
	}

	// Print the data and elem segment definitions.
	for(auto elemSegment : module.elemSegments)
	{
		string += '\n';
		ScopedTagPrinter dataTag(string, "elem");
		string += ' ';
		if(!elemSegment.isActive) { string += "passive"; }
		else
		{
			string += names.tables[elemSegment.tableIndex];
			string += ' ';
			printInitializerExpression(elemSegment.baseOffset);
		}
		enum
		{
			numElemsPerLine = 8
		};
		for(Uptr elementIndex = 0; elementIndex < elemSegment.indices.size(); ++elementIndex)
		{
			if(elementIndex % numElemsPerLine == 0) { string += '\n'; }
			else
			{
				string += ' ';
			}
			string += names.functions[elemSegment.indices[elementIndex]].name;
		}
	}
	for(auto dataSegment : module.dataSegments)
	{
		string += '\n';
		ScopedTagPrinter dataTag(string, "data");
		string += ' ';
		if(!dataSegment.isActive) { string += "passive"; }
		else
		{
			string += names.memories[dataSegment.memoryIndex];
			string += ' ';
			printInitializerExpression(dataSegment.baseOffset);
		}
		enum
		{
			numBytesPerLine = 64
		};
		for(Uptr offset = 0; offset < dataSegment.data.size(); offset += numBytesPerLine)
		{
			string += "\n\"";
			string += escapeString(
				(const char*)dataSegment.data.data() + offset,
				std::min(Uptr(dataSegment.data.size()) - offset, Uptr(numBytesPerLine)));
			string += "\"";
		}
	}

	// Print the start function.
	if(module.startFunctionIndex != UINTPTR_MAX)
	{
		string += '\n';
		ScopedTagPrinter startTag(string, "start");
		string += ' ';
		string += names.functions[module.startFunctionIndex].name;
	}

	// Print the function definitions.
	for(Uptr functionDefIndex = 0; functionDefIndex < module.functions.defs.size();
		++functionDefIndex)
	{
		const Uptr functionIndex = module.functions.imports.size() + functionDefIndex;
		const FunctionDef& functionDef = module.functions.defs[functionDefIndex];
		FunctionType functionType = module.types[functionDef.type.index];
		FunctionPrintContext functionContext(*this, functionDefIndex);

		string += "\n\n";
		ScopedTagPrinter funcTag(string, "func");

		string += ' ';
		string += names.functions[functionIndex].name;

		// Print the function's type.
		string += " (type ";
		string += names.types[functionDef.type.index];
		string += ')';

		// Print the function parameters.
		if(functionType.params().size())
		{
			for(Uptr parameterIndex = 0; parameterIndex < functionType.params().size();
				++parameterIndex)
			{
				string += '\n';
				ScopedTagPrinter paramTag(string, "param");
				string += ' ';
				string += functionContext.localNames[parameterIndex];
				string += ' ';
				print(string, functionType.params()[parameterIndex]);
			}
		}

		// Print the function return type.
		if(functionType.results().size())
		{
			string += '\n';
			ScopedTagPrinter resultTag(string, "result");
			for(Uptr resultIndex = 0; resultIndex < functionType.results().size(); ++resultIndex)
			{
				string += ' ';
				print(string, functionType.results()[resultIndex]);
			}
		}

		// Print the function's locals.
		for(Uptr localIndex = 0; localIndex < functionDef.nonParameterLocalTypes.size();
			++localIndex)
		{
			string += '\n';
			ScopedTagPrinter localTag(string, "local");
			string += ' ';
			string += functionContext.localNames[functionType.params().size() + localIndex];
			string += ' ';
			print(string, functionDef.nonParameterLocalTypes[localIndex]);
		}

		functionContext.printFunctionBody();
	}

	// Print user sections (other than the name section).
	for(const auto& userSection : module.userSections)
	{
		if(userSection.name == "linking") { printLinkingSection(userSection); }
		else if(userSection.name != "name")
		{
			string += '\n';
			string += ";; User section \"";
			string += escapeString(userSection.name.c_str(), userSection.name.length());
			string += "\":" INDENT_STRING;
			enum
			{
				numBytesPerLine = 32
			};
			for(Uptr offset = 0; offset < userSection.data.size(); offset += numBytesPerLine)
			{
				string += "\n;; \"";
				string += escapeString(
					(const char*)userSection.data.data() + offset,
					std::min(Uptr(userSection.data.size()) - offset, Uptr(numBytesPerLine)));
				string += "\"";
			}
			string += DEDENT_STRING "\n";
		}
	}
}

void ModulePrintContext::printLinkingSection(const IR::UserSection& linkingSection)
{
	enum class LinkingSubsectionType
	{
		invalid = 0,
		segmentInfo = 5,
		initFuncs = 6,
		comdatInfo = 7,
		symbolTable = 8,
	};

	enum class COMDATKind
	{
		data = 0,
		function = 1,
		global = 2,
	};

	enum class SymbolKind
	{
		function = 0,
		data = 1,
		global = 2,
		section = 3,
	};

	// Print a comment that describes the contents of the linking section.
	std::string linkingSectionString;
	Uptr indentDepth = 1;
	linkingSectionString += "\n;; linking section:" INDENT_STRING;
	try
	{
		MemoryInputStream stream(linkingSection.data.data(), linkingSection.data.size());

		U32 version = 1;
		serializeVarUInt32(stream, version);
		linkingSectionString += "\n;; Version: " + std::to_string(version);

		while(stream.capacity())
		{
			U8 subsectionType = (U8)LinkingSubsectionType::invalid;
			serializeNativeValue(stream, subsectionType);

			Uptr numSubsectionBytes = 0;
			serializeVarUInt32(stream, numSubsectionBytes);

			MemoryInputStream substream(stream.advance(numSubsectionBytes), numSubsectionBytes);
			switch((LinkingSubsectionType)subsectionType)
			{
			case LinkingSubsectionType::segmentInfo:
			{
				linkingSectionString += "\n;; Segments:" INDENT_STRING;
				++indentDepth;

				Uptr numSegments = 0;
				serializeVarUInt32(substream, numSegments);
				for(Uptr segmentIndex = 0; segmentIndex < numSegments; ++segmentIndex)
				{
					std::string segmentName;
					serialize(substream, segmentName);

					Uptr alignment = 0;
					Uptr flags = 0;
					serializeVarUInt32(substream, alignment);
					serializeVarUInt32(substream, flags);

					linkingSectionString += "\n;; ";
					linkingSectionString += segmentName;
					linkingSectionString += " alignment=" + std::to_string(1 << alignment);
					linkingSectionString += " flags=" + std::to_string(flags);
				}

				linkingSectionString += DEDENT_STRING;
				--indentDepth;
				break;
			}
			case LinkingSubsectionType::initFuncs:
			{
				linkingSectionString += "\n;; Init funcs:" INDENT_STRING;
				++indentDepth;

				Uptr numInitFuncs = 0;
				serializeVarUInt32(substream, numInitFuncs);
				for(Uptr initFuncIndex = 0; initFuncIndex < numInitFuncs; ++initFuncIndex)
				{
					Uptr functionIndex = 0;
					serializeVarUInt32(substream, functionIndex);

					linkingSectionString += ";; \n";
					if(functionIndex < names.functions.size())
					{ linkingSectionString += ' ' + names.functions[functionIndex].name; }
					else
					{
						linkingSectionString
							+= " <invalid function index " + std::to_string(functionIndex) + ">";
					}
				}

				linkingSectionString += DEDENT_STRING;
				--indentDepth;
				break;
			}
			case LinkingSubsectionType::comdatInfo:
			{
				linkingSectionString += "\n;; Comdats:" INDENT_STRING;
				++indentDepth;

				Uptr numComdats = 0;
				serializeVarUInt32(substream, numComdats);
				for(Uptr comdatIndex = 0; comdatIndex < numComdats; ++comdatIndex)
				{
					std::string comdatName;
					serialize(substream, comdatName);

					U32 flags = 0;
					serializeVarUInt32(substream, flags);

					linkingSectionString += "\n;; ";
					linkingSectionString += comdatName;

					if(flags) { linkingSectionString += " OtherFlags=" + std::to_string(flags); }

					linkingSectionString += INDENT_STRING;
					++indentDepth;

					Uptr numSymbols = 0;
					serializeVarUInt32(substream, numSymbols);
					for(Uptr symbolIndex = 0; symbolIndex < numSymbols; ++symbolIndex)
					{
						U32 kind = 0;
						U32 index = 0;
						serializeVarUInt32(substream, kind);
						serializeVarUInt32(substream, index);

						linkingSectionString += "\n;; Symbol: ";
						switch((COMDATKind)kind)
						{
						case COMDATKind::data:
							linkingSectionString += "data segment ";
							linkingSectionString += std::to_string(index);
							break;
						case COMDATKind::function:
							linkingSectionString += "function ";
							if(index >= names.functions.size())
							{
								linkingSectionString
									+= "Invalid COMDAT function index " + std::to_string(index);
								throw FatalSerializationException("Invalid COMDAT function index");
							}
							linkingSectionString += names.functions[index].name;
							break;
						case COMDATKind::global:
							linkingSectionString += "global ";
							if(index >= names.globals.size())
							{
								linkingSectionString
									+= "Invalid COMDAT global index " + std::to_string(index);
								throw FatalSerializationException("Invalid COMDAT global index");
							}
							linkingSectionString += names.globals[index];
							break;
						default:
							linkingSectionString
								+= "\nUnknown comdat kind: " + std::to_string(kind);
							throw FatalSerializationException("Unknown COMDAT kind");
							break;
						};
					}

					linkingSectionString += DEDENT_STRING;
					--indentDepth;
				}

				linkingSectionString += DEDENT_STRING;
				--indentDepth;
				break;
			}
			case LinkingSubsectionType::symbolTable:
			{
				linkingSectionString += "\n;; Symbols:" INDENT_STRING;
				++indentDepth;

				Uptr numSymbols = 0;
				serializeVarUInt32(substream, numSymbols);
				for(Uptr symbolIndex = 0; symbolIndex < numSymbols; ++symbolIndex)
				{
					U8 kind = 0;
					serializeNativeValue(substream, kind);

					U32 flags = 0;
					serializeVarUInt32(substream, flags);

					const char* kindName = nullptr;
					std::string symbolName;
					U32 index = 0;
					U32 offset = 0;
					U32 numBytes = 0;

					switch(SymbolKind(kind))
					{
					case SymbolKind::function:
					{
						kindName = "function ";
						serializeVarUInt32(substream, index);
						if(index < module.functions.imports.size())
						{
							symbolName = module.functions.imports[index].moduleName + "."
										 + module.functions.imports[index].exportName;
						}
						else
						{
							serialize(substream, symbolName);
						}
						break;
					}
					case SymbolKind::global:
					{
						kindName = "global ";
						serializeVarUInt32(substream, index);
						if(index < module.globals.imports.size())
						{
							symbolName = module.globals.imports[index].moduleName + "."
										 + module.globals.imports[index].exportName;
						}
						else
						{
							serialize(substream, symbolName);
						}
						break;
					}
					case SymbolKind::data:
					{
						kindName = "data ";
						serialize(substream, symbolName);
						serializeVarUInt32(substream, index);
						serializeVarUInt32(substream, offset);
						serializeVarUInt32(substream, numBytes);
						break;
					}
					case SymbolKind::section:
					{
						kindName = "section ";
						serializeVarUInt32(substream, index);

						if(index < module.userSections.size())
						{ symbolName = module.userSections[index].name; }
						else
						{
							symbolName = "*invalid index*";
						}

						break;
					}
					default:
						linkingSectionString += "\nUnknown symbol kind: " + std::to_string(kind);
						throw FatalSerializationException("Unknown symbol kind");
					};

					linkingSectionString += "\n;; ";
					linkingSectionString += kindName;
					linkingSectionString += symbolName;

					switch(SymbolKind(kind))
					{
					case SymbolKind::function:
						linkingSectionString += " " + names.functions[index].name;
						break;
					case SymbolKind::global:
						linkingSectionString += " " + names.globals[index];
						break;
					case SymbolKind::data:
					case SymbolKind::section:
						linkingSectionString += " index=" + std::to_string(index);
						break;
					}

					if(SymbolKind(kind) == SymbolKind::data)
					{
						linkingSectionString += " offset=" + std::to_string(offset);
						linkingSectionString += " size=" + std::to_string(numBytes);
					}

					if(flags & 1)
					{
						linkingSectionString += " *WEAK*";
						flags &= ~1;
					}
					if(flags & 2)
					{
						linkingSectionString += " *LOCAL*";
						flags &= ~2;
					}
					if(flags & 4)
					{
						linkingSectionString += " *HIDDEN*";
						flags &= ~4;
					}
					if(flags & 16)
					{
						linkingSectionString += " *UNDEFINED*";
						flags &= ~16;
					}
					if(flags) { linkingSectionString += " OtherFlags=" + std::to_string(flags); }
				}

				linkingSectionString += DEDENT_STRING;
				--indentDepth;
				break;
			}
			default:
				linkingSectionString
					+= "\nUnknown WASM linking subsection type: " + std::to_string(subsectionType);
				throw FatalSerializationException("Unknown linking subsection type");
				break;
			};
		};
	}
	catch(FatalSerializationException)
	{
		linkingSectionString += "\n;; Fatal serialization exception!";
		while(indentDepth > 1)
		{
			linkingSectionString += DEDENT_STRING;
			--indentDepth;
		};
	}
	wavmAssert(indentDepth == 1);
	linkingSectionString += DEDENT_STRING "\n";

	string += linkingSectionString;
}

void FunctionPrintContext::printFunctionBody()
{
	// string += "(";
	pushControlStack(ControlContext::Type::function, "");
	string += DEDENT_STRING;

	OperatorDecoderStream decoder(functionDef.code);
	while(decoder && controlStack.size()) { decoder.decodeOp(*this); };

	string += INDENT_STRING "\n";
}

std::string WAST::print(const Module& module)
{
	std::string string;
	ModulePrintContext context(module, string);
	context.printModule();
	return expandIndentation(std::move(string));
}

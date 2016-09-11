#include "Core/Core.h"
#include "Core/SExpressions.h"
#include "Core/Platform.h"
#include "WAST/WAST.h"
#include "WAST/WASTSymbols.h"
#include "WebAssembly/WebAssembly.h"
#include "Runtime/Runtime.h"
#include "Runtime/Linker.h"
#include "Runtime/Intrinsics.h"

#include "CLI.h"

using namespace SExp;
using namespace WAST;
using namespace WebAssembly;
using namespace Runtime;

uintptr numErrors = 0;

struct TestScriptState : private Resolver
{
	std::vector<ParseError> errors;

	TestScriptState(const char* inFilename): filename(inFilename) {}

	bool process();

private:

	const char* filename;

	std::vector<Module*> modules;
	std::vector<ModuleInstance*> moduleInstances;
	
	std::map<std::string,uintptr> moduleInternalNameToIndexMap;
	std::map<std::string,uintptr> moduleNameToIndexMap;

	bool resolve(const char* moduleName,const char* exportName,ObjectType type,Object*& outObject) override
	{
		// Try to resolve an intrinsic first.
		if(IntrinsicResolver::singleton.resolve(moduleName,exportName,type,outObject)) { return true; }

		// Then look for a named module.
		auto mapIt = moduleNameToIndexMap.find(moduleName);
		if(mapIt != moduleNameToIndexMap.end())
		{
			outObject = getInstanceExport(moduleInstances[mapIt->second],exportName);
			return outObject != nullptr && isA(outObject,type);
		}

		return false;
	}
	
	// Creates and records an error with the given message and location.
	void recordError(const Core::TextFileLocus& locus,std::string&& message)
	{
		errors.push_back({locus,std::move(message)});
	}
	// Creates and record a S-expression error node.
	void recordSExpError(SNode* node)
	{
		recordError(node->startLocus,node->error);
	}

	// Creates and record a S-expression error node.
	void recordError(SNodeIt nodeIt,std::string&& message)
	{
		// If the referenced node is a S-expression error, pass it through as well.
		if(nodeIt && nodeIt->type == SNodeType::Error) { recordSExpError(nodeIt); }

		const Core::TextFileLocus& locus = nodeIt.node ? nodeIt.node->startLocus : nodeIt.previousLocus;
		errors.push_back({locus,std::move(message)});
	}
	
	void recordExcessInputError(SNodeIt nodeIt,const char* errorContext)
	{
		auto message = std::string("unexpected input following ") + errorContext;
		recordError(nodeIt,std::move(message));
	}

	Runtime::Value parseRuntimeValue(SNodeIt nodeIt)
	{
		SNodeIt childNodeIt;
		Symbol symbol;
		if(parseTreeNode(nodeIt,childNodeIt) && parseSymbol(childNodeIt,symbol))
		{
			switch(symbol)
			{
			case Symbol::_i32_const:
			{
				int32 i32Value;
				if(!parseInt(childNodeIt,i32Value)) { recordError(childNodeIt,"const: expected i32 literal"); return Runtime::Value(); }
				else { return Runtime::Value(i32Value); }
			}
			case Symbol::_i64_const:
			{
				int64 i64Value;
				if(!parseInt(childNodeIt,i64Value)) { recordError(childNodeIt,"const: expected i64 literal"); return Runtime::Value(); }
				else { return Runtime::Value(i64Value); }
			}
			case Symbol::_f32_const:
			{
				float32 f32Value;
				if(!parseFloat(childNodeIt,f32Value)) { recordError(childNodeIt,"const: expected f32 literal"); return Runtime::Value(); }
				else { return Runtime::Value(f32Value); }
			}
			case Symbol::_f64_const:
			{
				float64 f64Value;
				if(!parseFloat(childNodeIt,f64Value)) { recordError(childNodeIt,"const: expected f64 literal"); return Runtime::Value(); }
				else { return Runtime::Value(f64Value); }
			}
			default:;
			};
		}

		recordError(nodeIt,"expected const expression");
		return Runtime::Value();
	}

	bool processAction(SNodeIt nodeIt,Runtime::Value& outValue)
	{
		SNodeIt childNodeIt;
		if(parseTaggedNode(nodeIt,Symbol::_invoke,childNodeIt))
		{
			if(!modules.size()) { recordError(nodeIt,"no module to use in invoke"); return false; }

			// Parse an optional module name.
			SNodeIt moduleNameIt = childNodeIt;
			const char* moduleName;
			uintptr moduleIndex = modules.size() - 1;
			if(parseName(childNodeIt,moduleName))
			{
				auto mapIt = moduleInternalNameToIndexMap.find(moduleName);
				if(mapIt == moduleInternalNameToIndexMap.end())
				{
					recordError(moduleNameIt,std::string("unknown module name: ") + moduleName);
					return true;
				}
				moduleIndex = mapIt->second;
			}

			// Parse the export name to invoke.
			std::string invokeExportName;
			SNodeIt savedExportNameIt = childNodeIt;
			if(!parseString(childNodeIt,invokeExportName))
				{ recordError(childNodeIt,"expected export name string"); return false; }

			// Find the named export in the module instance and get its type.
			auto functionInstance = asFunctionNullable(getInstanceExport(moduleInstances[moduleIndex],invokeExportName.c_str()));
			if(!functionInstance) { recordError(savedExportNameIt,std::string("couldn't find exported function with name: ") + invokeExportName); return false; }
			auto functionType = getFunctionType(functionInstance);

			// Parse the invoke's parameters.
			std::vector<Runtime::Value> parameters(functionType->parameters.size());
			for(uintptr parameterIndex = 0;parameterIndex < functionType->parameters.size();++parameterIndex)
				{ parameters[parameterIndex] = parseRuntimeValue(childNodeIt++); }

			// Verify that all of the invoke's operands were parsed.
			if(childNodeIt) { recordExcessInputError(childNodeIt,"invoke unexpected argument"); }

			// Execute the invoke
			outValue = invokeFunction(functionInstance,parameters);
			return true;
		}
		else if(parseTaggedNode(nodeIt,Symbol::_get,childNodeIt))
		{
			if(!modules.size()) { recordError(nodeIt,"no module to use in get"); return false; }

			// Parse an optional module name.
			SNodeIt moduleNameIt = childNodeIt;
			const char* moduleName;
			uintptr moduleIndex = modules.size() - 1;
			if(parseName(childNodeIt,moduleName))
			{
				auto mapIt = moduleInternalNameToIndexMap.find(moduleName);
				if(mapIt == moduleInternalNameToIndexMap.end())
				{
					recordError(moduleNameIt,std::string("unknown module name: ") + moduleName);
					return false;
				}
				moduleIndex = mapIt->second;
			}

			// Parse the export name to get.
			std::string getExportName;
			SNodeIt savedExportNameIt = childNodeIt;
			if(!parseString(childNodeIt,getExportName))
				{ recordError(childNodeIt,"expected export name string"); return false; }

			// Find the named export in the module.
			auto global = asGlobalNullable(getInstanceExport(moduleInstances[moduleIndex],getExportName.c_str()));
			if(!global)
			{ recordError(savedExportNameIt,std::string("couldn't find exported global with name: ") + getExportName); return false; }

			// Verify that all of the get's operands were parsed.
			if(childNodeIt) { recordExcessInputError(childNodeIt,"get unexpected argument"); }

			// Get the value of the specified global.
			outValue = getGlobalValue(global);
			return true;
		}
		else { recordError(nodeIt,"expected invoke or get"); return false; }
	}

	void processAssertReturn(Core::TextFileLocus locus,SNodeIt nodeIt)
	{
		// Process the action.
		Runtime::Value result;
		if(!processAction(nodeIt++,result)) { return; }

		// Parse the expected value of the action.
		auto expectedValue = nodeIt ? parseRuntimeValue(nodeIt++) : Runtime::Value();
	
		// Check that the action result matched the expected value.
		if(describeRuntimeValue(result) != describeRuntimeValue(expectedValue))
		{
			recordError(locus,"assert_return: expected " + describeRuntimeValue(expectedValue)
				+ " but got " + describeRuntimeValue(result)
				);
		}

		// Verify that all of the assert_return consumed all its input.
		if(nodeIt) { recordExcessInputError(nodeIt,"assert_return unexpected input"); }
	}
	
	void processAssertReturnNaN(Core::TextFileLocus locus,SNodeIt nodeIt)
	{
		// Process the action.
		Runtime::Value result;
		if(!processAction(nodeIt++,result)) { return; }

		// Check that the action result was a NaN.
		if(result.type != TypeId::f32 && result.type != TypeId::f64)
		{ recordError(locus,"assert_return_nan: expected floating-point number but got " + describeRuntimeValue(result)); }
		else if(	(result.type == TypeId::f32 && (result.f32 == result.f32))
		||		(result.type == TypeId::f64 && (result.f64 == result.f64)))
		{ recordError(locus,"assert_return_nan: expected NaN but got " + describeRuntimeValue(result)); }

		// Verify that all of the assert_return_nan consumed all its input.
		if(nodeIt) { recordExcessInputError(nodeIt,"assert_return_nan unexpected input"); }
	}
	
	void processAssertTrap(Core::TextFileLocus locus,SNodeIt nodeIt)
	{
		// Process the action.
		Runtime::Value result;
		if(!processAction(nodeIt++,result)) { return; }

		// Parse the expected value of the invoke.
		std::string message;
		if(!parseString(nodeIt,message)) { recordError(nodeIt,"expected trap message"); return; }
		
		// Try to map the trap message to an exception cause.
		Runtime::Exception::Cause expectedCause = Runtime::Exception::Cause::unknown;
		if(!strcmp(message.c_str(),"out of bounds memory access")) { expectedCause = Runtime::Exception::Cause::accessViolation; }
		else if(!strcmp(message.c_str(),"callstack exhausted")) { expectedCause = Runtime::Exception::Cause::stackOverflow; }
		else if(!strcmp(message.c_str(),"integer overflow")) { expectedCause = Runtime::Exception::Cause::integerDivideByZeroOrIntegerOverflow; }
		else if(!strcmp(message.c_str(),"integer divide by zero")) { expectedCause = Runtime::Exception::Cause::integerDivideByZeroOrIntegerOverflow; }
		else if(!strcmp(message.c_str(),"invalid conversion to integer")) { expectedCause = Runtime::Exception::Cause::invalidFloatOperation; }
		else if(!strcmp(message.c_str(),"call stack exhausted")) { expectedCause = Runtime::Exception::Cause::stackOverflow; }
		else if(!strcmp(message.c_str(),"unreachable executed")) { expectedCause = Runtime::Exception::Cause::reachedUnreachable; }
		else if(!strcmp(message.c_str(),"unreachable")) { expectedCause = Runtime::Exception::Cause::reachedUnreachable; }
		else if(!strcmp(message.c_str(),"indirect call signature mismatch")) { expectedCause = Runtime::Exception::Cause::indirectCallSignatureMismatch; }
		else if(!strcmp(message.c_str(),"indirect call")) { expectedCause = Runtime::Exception::Cause::indirectCallSignatureMismatch; }
		else if(!strcmp(message.c_str(),"undefined element")) { expectedCause = Runtime::Exception::Cause::undefinedTableElement; }
		else if(!strcmp(message.c_str(),"undefined")) { expectedCause = Runtime::Exception::Cause::undefinedTableElement; }
		else if(!strcmp(message.c_str(),"uninitialized")) { expectedCause = Runtime::Exception::Cause::undefinedTableElement; }
		else if(!strcmp(message.c_str(),"uninitialized element")) { expectedCause = Runtime::Exception::Cause::undefinedTableElement; }
		const std::string expectedCauseDescription = std::string("Exception(") + describeExceptionCause(expectedCause) + ")";

		// Check that the action result was an exception of the expected type.
		if(result.type != Runtime::TypeId::exception) { recordError(locus,"assert_trap: expected " + expectedCauseDescription + " but got " + describeRuntimeValue(result)); }
		else if(result.exception->cause != expectedCause)
		{ recordError(locus,std::string("assert_trap: expected ") + expectedCauseDescription + " but got " + describeRuntimeValue(result)); }

		// Verify that all of the assert_trap's parameters were matched.
		if(nodeIt) { recordExcessInputError(nodeIt,"assert_trap unexpected input"); }
	}

	void processAssertInvalid(SNodeIt nodeIt)
	{
		SNodeIt moduleNodeIt = nodeIt;
		SNodeIt childNodeIt;
		if(!parseTaggedNode(nodeIt,Symbol::_module,childNodeIt)) { recordError(moduleNodeIt,"assert_invalid: expected module definition"); return; }
		++nodeIt;
		
		std::vector<WAST::ParseError> invalidModuleErrors;
		Module invalidModule;
		const bool hasParseSucceeded = WAST::parseModule(childNodeIt,invalidModule,invalidModuleErrors);

		std::string description;
		if(!parseString(nodeIt,description)) { recordError(nodeIt,"expected assert_invalid description"); return; }

		if(hasParseSucceeded) { recordError(moduleNodeIt,"expected invalid module(" + description + ") but got a valid module"); }
	}

	void processAssertUnlinkable(SNodeIt nodeIt)
	{
		SNodeIt moduleNodeIt = nodeIt;
		SNodeIt childNodeIt;
		if(!parseTaggedNode(nodeIt,Symbol::_module,childNodeIt)) { recordError(moduleNodeIt,"assert_invalid: expected module definition"); return; }
		++nodeIt;
		
		Module* unlinkableModule = new Module();
		std::vector<ParseError> unlinkableModuleErrors;
		if(!WAST::parseModule(childNodeIt,*unlinkableModule,unlinkableModuleErrors))
		{
			errors.insert(errors.end(),unlinkableModuleErrors.begin(),unlinkableModuleErrors.end());
			return;
		}

		std::string description;
		if(!parseString(nodeIt,description)) { recordError(nodeIt,"expected assert_unlinkable description"); return; }

		try
		{
			linkModule(*unlinkableModule,*this);
			recordError(moduleNodeIt,"expected unlinkable module, but link succeeded");
		}
		catch(LinkException exception)
		{
		}
	}
};

std::string describeRuntimeException(const Runtime::Exception* exception)
{
	std::string result = describeExceptionCause(exception->cause);
	for(auto function : exception->callStack) { result += "\n"; result += function; }
	return result;
}

bool TestScriptState::process()
{
	// Read the file into a string.
	auto wastBytes = loadFile(filename);
	if(!wastBytes.size()) { return false; }
	auto wastString = std::string((const char*)wastBytes.data(),wastBytes.size());
	wastBytes.clear();
		
	const SExp::SymbolIndexMap& symbolIndexMap = getWASTSymbolIndexMap();
		
	// Parse S-expressions from the string.
	MemoryArena::ScopedArena scopedArena;
	auto rootNode = SExp::parse(wastString.c_str(),scopedArena,symbolIndexMap);

	// Parse modules from S-expressions.
	for(auto rootNodeIt = SNodeIt(rootNode);rootNodeIt;++rootNodeIt)
	{
		SNodeIt childNodeIt;
		if(parseTaggedNode(rootNodeIt,Symbol::_assert_invalid,childNodeIt))
		{
			processAssertInvalid(childNodeIt);
		}
		else if(parseTaggedNode(rootNodeIt,Symbol::_module,childNodeIt))
		{
			// Parse an optional module name.
			const char* moduleInternalName = nullptr;
			bool hasName = parseName(childNodeIt,moduleInternalName);

			// Parse a module definition.
			Module* module = new Module();
			std::vector<ParseError> moduleParseErrors;
			if(!WAST::parseModule(childNodeIt,*module,moduleParseErrors))
			{
				errors.insert(errors.end(),moduleParseErrors.begin(),moduleParseErrors.end());
				break;
			}
			else
			{
				modules.push_back(module);
				moduleInstances.push_back(linkAndInstantiateModule(*module,*this));
				if(hasName)
				{
					// Don't check for duplicate names for now, since the ml-proto tests rely on name shadowing.
					/*if(moduleInternalNameToIndexMap.count(moduleInternalName)) { recordError(moduleInternalNameIt,std::string("duplicate module name: ") + moduleInternalName); }
					else*/ { moduleInternalNameToIndexMap[moduleInternalName] = modules.size() - 1; }
				}
			}
		}
		else if(parseTaggedNode(rootNodeIt,Symbol::_register,childNodeIt))
		{
			// Parse the public name of the module.
			std::string moduleName;
			if(!parseString(childNodeIt,moduleName)) { recordError(childNodeIt,"expected module name string"); continue; }

			// Parse the internal name of the module.
			const char* moduleInternalName = nullptr;
			if(parseName(childNodeIt,moduleInternalName))
			{
				auto mapIt = moduleInternalNameToIndexMap.find(moduleInternalName);
				if(mapIt == moduleInternalNameToIndexMap.end()) { recordError(childNodeIt,"unknown module internal name"); continue; }
				const uintptr moduleIndex = mapIt->second;
				moduleNameToIndexMap[moduleName] = moduleIndex;
			}
		}
		else if(rootNodeIt->type == SNodeType::Error)
		{
			// Pass through top-level errors from the S-expression parser.
			recordError(rootNodeIt->startLocus,rootNodeIt->error);
		}
		else if(parseTaggedNode(rootNodeIt,Symbol::_assert_return,childNodeIt))
		{
			processAssertReturn(rootNodeIt->startLocus,childNodeIt);
		}
		else if(parseTaggedNode(rootNodeIt,Symbol::_assert_return_nan,childNodeIt))
		{
			processAssertReturnNaN(rootNodeIt->startLocus,childNodeIt);
		}
		else if(parseTaggedNode(rootNodeIt,Symbol::_assert_trap,childNodeIt))
		{
			processAssertTrap(rootNodeIt->startLocus,childNodeIt);
		}
		else if(parseTaggedNode(rootNodeIt,Symbol::_assert_unlinkable,childNodeIt))
		{
			processAssertUnlinkable(childNodeIt);
		}
		else if(parseTaggedNode(rootNodeIt,Symbol::_invoke,childNodeIt) || parseTaggedNode(rootNodeIt,Symbol::_get,childNodeIt))
		{
			Runtime::Value actionResult;
			if(processAction(rootNodeIt,actionResult))
			{
				if(actionResult.type == TypeId::exception) { recordError(rootNodeIt,"unexpected trap: " + describeRuntimeException(actionResult.exception)); }
			}
		}
		else { recordError(rootNodeIt,"unrecognized input"); }
	}

	if(!errors.size()) { return true; }
	else
	{
		// Build an index of newline offsets in the file.
		std::vector<size_t> wastFileLineOffsets;
		wastFileLineOffsets.push_back(0);
		for(size_t charIndex = 0;charIndex < wastString.length();++charIndex)
		{ if(wastString[charIndex] == '\n') { wastFileLineOffsets.push_back(charIndex+1); } }
		wastFileLineOffsets.push_back(wastString.length()+1);

		// Print any parse errors;
		std::cerr << "Error processing WebAssembly text file:" << std::endl;
		for(auto error : errors)
		{
			std::cerr << filename << ":" << error.locus.describe() << ": " << error.message.c_str() << std::endl;
			auto startLine = wastFileLineOffsets.at(error.locus.newlines);
			auto endLine =  wastFileLineOffsets.at(error.locus.newlines+1);
			std::cerr << wastString.substr(startLine, endLine-startLine-1) << std::endl;
			std::cerr << std::setw(error.locus.column(8)) << "^" << std::endl;
		}
		return false;
	}
}

DEFINE_INTRINSIC_FUNCTION1(spectest,spectest_print,print,none,i32,a)
{
	std::cout << a << " : i32" << std::endl;
}

DEFINE_INTRINSIC_FUNCTION1(spectest,spectest_print,print,none,i64,a)
{
	std::cout << a << " : i64" << std::endl;
}

DEFINE_INTRINSIC_FUNCTION1(spectest,spectest_print,print,none,f32,a)
{
	std::cout << a << " : f32" << std::endl;
}
	
DEFINE_INTRINSIC_FUNCTION1(spectest,spectest_print,print,none,f64,a)
{
	std::cout << a << " : f64" << std::endl;
}

DEFINE_INTRINSIC_FUNCTION2(spectest,spectest_print,print,none,i32,a,f32,b)
{
	std::cout << a << " : i32, " << b << " : f32" << std::endl;
}

DEFINE_INTRINSIC_FUNCTION2(spectest,spectest_print,print,none,i64,a,f64,b)
{
	std::cout << a << " : i64, " << b << " : f64" << std::endl;
}

DEFINE_INTRINSIC_VARIABLE(spectest,spectest_globalI32,global,i32,false,666)
DEFINE_INTRINSIC_VARIABLE(spectest,spectest_globalI64,global,i64,false,0)
DEFINE_INTRINSIC_VARIABLE(spectest,spectest_globalF32,global,f32,false,0.0f)
DEFINE_INTRINSIC_VARIABLE(spectest,spectest_globalF64,global,f64,false,0.0)

DEFINE_INTRINSIC_TABLE(spectest,spectest_table,table,TableType(TableElementType::anyfunc,SizeConstraints {10,20}))
DEFINE_INTRINSIC_MEMORY(spectest,spectest_memory,memory,MemoryType(SizeConstraints {1,2}))

int commandMain(int argc,char** argv)
{
	if(argc != 2)
	{
		std::cerr <<  "Usage: Test in.wast" << std::endl;
		return EXIT_FAILURE;
	}
	const char* filename = argv[1];
	
	Runtime::init();
	
	TestScriptState scriptState(filename);
	if(!scriptState.process())
	{
		std::cerr << filename << ": testing failed!" << std::endl;
		return EXIT_FAILURE;
	}
	else
	{
		std::cout << filename << ": all tests passed." << std::endl;
		return EXIT_SUCCESS;
	}
}

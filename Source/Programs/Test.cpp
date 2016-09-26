#include "Core/Core.h"
#include "Core/SExpressions.h"
#include "Core/Serialization.h"
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

struct TestScriptState : private Resolver
{
	std::vector<WAST::Error> errors;

	TestScriptState(const char* inFilename): filename(inFilename), lastModuleInstance(nullptr) {}

	bool process();

private:

	const char* filename;

	ModuleInstance* lastModuleInstance;
	
	std::map<std::string,ModuleInstance*> moduleInternalNameToIndexMap;
	std::map<std::string,ModuleInstance*> moduleNameToIndexMap;

	void collectGarbage()
	{
		std::vector<Object*> rootObjects;
		rootObjects.push_back(asObject(lastModuleInstance));
		for(auto& mapIt : moduleInternalNameToIndexMap) { rootObjects.push_back(asObject(mapIt.second)); }
		for(auto& mapIt : moduleNameToIndexMap) { rootObjects.push_back(asObject(mapIt.second)); }
		freeUnreferencedObjects(rootObjects);
	}

	bool resolve(const char* moduleName,const char* exportName,ObjectType type,Object*& outObject) override
	{
		// Try to resolve an intrinsic first.
		if(IntrinsicResolver::singleton.resolve(moduleName,exportName,type,outObject)) { return true; }

		// Then look for a named module.
		auto mapIt = moduleNameToIndexMap.find(moduleName);
		if(mapIt != moduleNameToIndexMap.end())
		{
			outObject = getInstanceExport(mapIt->second,exportName);
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

	Value parseRuntimeValue(SNodeIt nodeIt)
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
				if(!parseInt(childNodeIt,i32Value)) { recordError(childNodeIt,"const: expected i32 literal"); return Value(); }
				else { return Value(i32Value); }
			}
			case Symbol::_i64_const:
			{
				int64 i64Value;
				if(!parseInt(childNodeIt,i64Value)) { recordError(childNodeIt,"const: expected i64 literal"); return Value(); }
				else { return Value(i64Value); }
			}
			case Symbol::_f32_const:
			{
				float32 f32Value;
				if(!parseFloat(childNodeIt,f32Value)) { recordError(childNodeIt,"const: expected f32 literal"); return Value(); }
				else { return Value(f32Value); }
			}
			case Symbol::_f64_const:
			{
				float64 f64Value;
				if(!parseFloat(childNodeIt,f64Value)) { recordError(childNodeIt,"const: expected f64 literal"); return Value(); }
				else { return Value(f64Value); }
			}
			default:;
			};
		}

		recordError(nodeIt,"expected const expression");
		return Value();
	}

	bool processAction(SNodeIt nodeIt,Result& outResult)
	{
		SNodeIt childNodeIt;
		if(parseTaggedNode(nodeIt,Symbol::_invoke,childNodeIt))
		{
			if(!lastModuleInstance) { recordError(nodeIt,"no module to use in invoke"); return false; }

			// Parse an optional module name.
			SNodeIt moduleNameIt = childNodeIt;
			const char* moduleName;
			ModuleInstance* moduleInstance = lastModuleInstance;
			if(parseName(childNodeIt,moduleName))
			{
				auto mapIt = moduleInternalNameToIndexMap.find(moduleName);
				if(mapIt == moduleInternalNameToIndexMap.end())
				{
					recordError(moduleNameIt,std::string("unknown module name: ") + moduleName);
					return true;
				}
				moduleInstance = mapIt->second;
			}

			// Look up the module this invoke uses, and bail without an error if there was an error parsing the module.
			if(moduleInstance)
			{
				// Parse the export name to invoke.
				std::string invokeExportName;
				SNodeIt savedExportNameIt = childNodeIt;
				if(!parseString(childNodeIt,invokeExportName))
					{ recordError(childNodeIt,"expected export name string"); return false; }

				// Find the named export in the module instance and get its type.
				auto functionInstance = asFunctionNullable(getInstanceExport(moduleInstance,invokeExportName.c_str()));
				if(!functionInstance) { recordError(savedExportNameIt,std::string("couldn't find exported function with name: ") + invokeExportName); return false; }
				auto functionType = getFunctionType(functionInstance);

				// Parse the invoke's parameters.
				std::vector<Value> parameters(functionType->parameters.size());
				for(uintp parameterIndex = 0;parameterIndex < functionType->parameters.size();++parameterIndex)
					{ parameters[parameterIndex] = parseRuntimeValue(childNodeIt++); }

				// Verify that all of the invoke's operands were parsed.
				if(childNodeIt) { recordExcessInputError(childNodeIt,"invoke unexpected argument"); }

				// Execute the invoke
				outResult = invokeFunction(functionInstance,parameters);
			}

			return true;
		}
		else if(parseTaggedNode(nodeIt,Symbol::_get,childNodeIt))
		{
			if(!lastModuleInstance) { recordError(nodeIt,"no module to use in get"); return false; }

			// Parse an optional module name.
			SNodeIt moduleNameIt = childNodeIt;
			const char* moduleName;
			ModuleInstance* moduleInstance = lastModuleInstance;
			if(parseName(childNodeIt,moduleName))
			{
				auto mapIt = moduleInternalNameToIndexMap.find(moduleName);
				if(mapIt == moduleInternalNameToIndexMap.end())
				{
					recordError(moduleNameIt,std::string("unknown module name: ") + moduleName);
					return false;
				}
				moduleInstance = mapIt->second;
			}

			// Look up the module this invoke uses, and bail without an error if there was an error parsing the module.
			if(moduleInstance)
			{
				// Parse the export name to get.
				std::string getExportName;
				SNodeIt savedExportNameIt = childNodeIt;
				if(!parseString(childNodeIt,getExportName)) { recordError(childNodeIt,"expected export name string"); return false; }

				// Find the named export in the module.
				auto global = asGlobalNullable(getInstanceExport(moduleInstance,getExportName.c_str()));
				if(!global) { recordError(savedExportNameIt,std::string("couldn't find exported global with name: ") + getExportName); return false; }

				// Verify that all of the get's operands were parsed.
				if(childNodeIt) { recordExcessInputError(childNodeIt,"get unexpected argument"); }

				// Get the value of the specified global.
				outResult = getGlobalValue(global);
			}

			return true;
		}
		else { recordError(nodeIt,"expected invoke or get"); return false; }
	}

	void processAssertReturn(Core::TextFileLocus locus,SNodeIt nodeIt)
	{
		SNodeIt actionNodeIt = nodeIt++;

		// Parse the expected value of the action.
		Result expectedResult = nodeIt ? parseRuntimeValue(nodeIt++) : Result();

		// Process the action.
		try
		{
			Result result;
			if(!processAction(actionNodeIt,result)) { return; }
	
			// Check that the action result matched the expected value.
			if(!areBitsEqual(result,expectedResult))
			{ recordError(locus,"assert_return: expected " + asString(expectedResult) + " but got " + asString(result)); }
		}
		catch(Exception exception) { recordError(locus,std::string("assert_return: unexpected trap: ") + describeExceptionCause(exception.cause)); }

		// Verify that all of the assert_return consumed all its input.
		if(nodeIt) { recordExcessInputError(nodeIt,"assert_return unexpected input"); }
	}
	
	void processAssertReturnNaN(Core::TextFileLocus locus,SNodeIt nodeIt)
	{
		// Process the action.
		try
		{
			Result result;
			if(!processAction(nodeIt++,result)) { return; }
			
			// Check that the action result was a NaN.
			if(result.type != ResultType::f32 && result.type != ResultType::f64)
			{ recordError(locus,"assert_return_nan: expected floating-point number but got " + asString(result)); }
			else if(	(result.type == ResultType::f32 && (result.f32 == result.f32))
			||		(result.type == ResultType::f64 && (result.f64 == result.f64)))
			{ recordError(locus,"assert_return_nan: expected NaN but got " + asString(result)); }
		}
		catch(Exception exception) { recordError(locus,std::string("assert_return_nan: unexpected trap: ") + describeExceptionCause(exception.cause)); }

		// Verify that all of the assert_return_nan consumed all its input.
		if(nodeIt) { recordExcessInputError(nodeIt,"assert_return_nan unexpected input"); }
	}
	
	void processAssertTrap(Core::TextFileLocus locus,SNodeIt nodeIt)
	{
		SNodeIt actionNodeIt = nodeIt++;
		
		// Parse the expected value of the invoke.
		std::string message;
		if(!parseString(nodeIt,message)) { recordError(nodeIt,"expected trap message"); return; }
		
		// Try to map the trap message to an exception cause.
		Exception::Cause expectedCause = Exception::Cause::unknown;
		if(!strcmp(message.c_str(),"out of bounds memory access")) { expectedCause = Exception::Cause::accessViolation; }
		else if(!strcmp(message.c_str(),"callstack exhausted")) { expectedCause = Exception::Cause::stackOverflow; }
		else if(!strcmp(message.c_str(),"integer overflow")) { expectedCause = Exception::Cause::integerDivideByZeroOrIntegerOverflow; }
		else if(!strcmp(message.c_str(),"integer divide by zero")) { expectedCause = Exception::Cause::integerDivideByZeroOrIntegerOverflow; }
		else if(!strcmp(message.c_str(),"invalid conversion to integer")) { expectedCause = Exception::Cause::invalidFloatOperation; }
		else if(!strcmp(message.c_str(),"call stack exhausted")) { expectedCause = Exception::Cause::stackOverflow; }
		else if(!strcmp(message.c_str(),"unreachable executed")) { expectedCause = Exception::Cause::reachedUnreachable; }
		else if(!strcmp(message.c_str(),"unreachable")) { expectedCause = Exception::Cause::reachedUnreachable; }
		else if(!strcmp(message.c_str(),"indirect call signature mismatch")) { expectedCause = Exception::Cause::indirectCallSignatureMismatch; }
		else if(!strcmp(message.c_str(),"indirect call")) { expectedCause = Exception::Cause::indirectCallSignatureMismatch; }
		else if(!strcmp(message.c_str(),"undefined element")) { expectedCause = Exception::Cause::undefinedTableElement; }
		else if(!strcmp(message.c_str(),"undefined")) { expectedCause = Exception::Cause::undefinedTableElement; }
		else if(!strcmp(message.c_str(),"uninitialized")) { expectedCause = Exception::Cause::undefinedTableElement; }
		else if(!strcmp(message.c_str(),"uninitialized element")) { expectedCause = Exception::Cause::undefinedTableElement; }
		const char* expectedCauseDescription = describeExceptionCause(expectedCause);

		// Process the action.
		try
		{
			Result result;
			if(!processAction(actionNodeIt,result)) { return; }
			recordError(locus,std::string("assert_trap: expected ") + expectedCauseDescription + " trap but got " + asString(result));
		}
		catch(Exception exception)
		{
			// Check that the action result was an exception of the expected type.
			if(exception.cause != expectedCause)
			{ recordError(locus,std::string("assert_trap: expected ") + expectedCauseDescription + " trap but got " + describeExceptionCause(exception.cause) + " trap"); }
		}

		// Verify that all of the assert_trap's parameters were matched.
		if(nodeIt) { recordExcessInputError(nodeIt,"assert_trap unexpected input"); }
	}

	void processAssertInvalid(SNodeIt nodeIt)
	{
		SNodeIt moduleNodeIt = nodeIt;
		SNodeIt childNodeIt;
		if(!parseTaggedNode(nodeIt,Symbol::_module,childNodeIt)) { recordError(moduleNodeIt,"assert_invalid: expected module definition"); return; }
		++nodeIt;
		
		std::vector<WAST::Error> invalidModuleErrors;
		Module invalidModule;
		const char* internalModuleName = nullptr;
		const bool hasParseSucceeded = parseTextOrBinaryModule(childNodeIt,invalidModule,invalidModuleErrors,internalModuleName);

		std::string description;
		if(!parseString(nodeIt,description)) { recordError(nodeIt,"expected assert_invalid description"); return; }

		if(hasParseSucceeded) { recordError(moduleNodeIt,"expected invalid module(" + description + ") but got a valid module"); }
	}

	void processAssertMalformed(SNodeIt nodeIt)
	{
		SNodeIt moduleNodeIt = nodeIt;
		SNodeIt childNodeIt;
		if(!parseTaggedNode(nodeIt,Symbol::_module,childNodeIt)) { recordError(moduleNodeIt,"assert_malformed: expected module definition"); return; }
		++nodeIt;
		
		std::vector<WAST::Error> malformedModuleErrors;
		Module invalidModule;
		const char* internalModuleName = nullptr;
		const bool hasParseSucceeded = parseBinaryModule(childNodeIt,invalidModule,malformedModuleErrors,internalModuleName);

		std::string description;
		if(!parseString(nodeIt,description)) { recordError(nodeIt,"expected assert_malformed description"); return; }

		if(hasParseSucceeded) { recordError(moduleNodeIt,"expected invalid module(" + description + ") but got a valid module"); }
	}

	void processAssertUnlinkable(SNodeIt nodeIt)
	{
		SNodeIt moduleNodeIt = nodeIt;
		SNodeIt childNodeIt;
		if(!parseTaggedNode(nodeIt,Symbol::_module,childNodeIt)) { recordError(moduleNodeIt,"assert_invalid: expected module definition"); return; }
		++nodeIt;
		
		Module* unlinkableModule = new Module();
		std::vector<Error> unlinkableModuleErrors;
		const char* internalModuleName = nullptr;
		if(!parseTextOrBinaryModule(childNodeIt,*unlinkableModule,unlinkableModuleErrors,internalModuleName))
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
	
	bool parseStringSequence(SNodeIt& nodeIt,std::string& outString)
	{
		bool result = false;
		std::string tempString;
		while(parseString(nodeIt,tempString)) { outString += tempString; result = true; }
		return result;
	}

	bool deserializeBinaryModule(SNodeIt binaryNodeIt,const std::string& binaryString,WebAssembly::Module& outModule,std::vector<Error>& outModuleErrors)
	{
		Serialization::MemoryInputStream stringStream((uint8*)binaryString.data(),binaryString.size());
		try { serialize(stringStream,outModule); }
		catch(Serialization::FatalSerializationException exception)
		{
			outModuleErrors.push_back({binaryNodeIt->startLocus,"failed to deserialize binary module: " + exception.message});
			return false;
		}
		catch(ValidationException exception)
		{
			outModuleErrors.push_back({binaryNodeIt->startLocus,"failed to validate binary module: " + exception.message});
			return false;
		}
		return true;
	}

	bool parseBinaryModule(SNodeIt firstChildNodeIt,WebAssembly::Module& outModule,std::vector<Error>& outModuleErrors,const char*& outInternalName)
	{
		// Parse an optional internal module name.
		if(!parseName(firstChildNodeIt,outInternalName)) { outInternalName = nullptr;}

		// Parse and deserialize a binary module.
		SNodeIt binaryNodeIt = firstChildNodeIt;
		std::string binaryString;
		if(parseStringSequence(firstChildNodeIt,binaryString)) { return deserializeBinaryModule(binaryNodeIt,binaryString,outModule,outModuleErrors); }
		else { return false; }
	}

	bool parseTextOrBinaryModule(SNodeIt firstChildNodeIt,WebAssembly::Module& outModule,std::vector<Error>& outModuleErrors,const char*& outInternalName)
	{
		// Parse an optional internal module name.
		if(!parseName(firstChildNodeIt,outInternalName)) { outInternalName = nullptr;}

		// Try parsing and deserializing a binary module first.
		SNodeIt binaryNodeIt = firstChildNodeIt;
		std::string binaryString;
		if(parseStringSequence(firstChildNodeIt,binaryString)) { return deserializeBinaryModule(binaryNodeIt,binaryString,outModule,outModuleErrors); }
		else
		{
			// If it wasn't a binary module, then try parsing a text module.
			return WAST::parseModule(firstChildNodeIt,outModule,outModuleErrors);
		}
	}
};

std::string describeRuntimeException(const Exception& exception)
{
	std::string result = describeExceptionCause(exception.cause);
	for(auto function : exception.callStack) { result += "\n"; result += function; }
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
		else if(parseTaggedNode(rootNodeIt,Symbol::_assert_malformed,childNodeIt))
		{
			processAssertMalformed(childNodeIt);
		}
		else if(parseTaggedNode(rootNodeIt,Symbol::_module,childNodeIt))
		{
			// Clear the previous module.
			lastModuleInstance = nullptr;
			collectGarbage();

			// Parse a module definition.
			const char* moduleInternalName = nullptr;
			Module* module = new Module();
			std::vector<WAST::Error> moduleErrors;
			if(parseTextOrBinaryModule(childNodeIt,*module,moduleErrors,moduleInternalName))
			{
				// Link and instantiate the module.
				lastModuleInstance = linkAndInstantiateModule(*module,*this);
			}
			else
			{
				errors.insert(errors.end(),moduleErrors.begin(),moduleErrors.end());
				// Otherwise insert a nullptr in modules and moduleInstances so that tests that reference the most recent module realize there was a problem.
				delete module;
				module = nullptr;
			}

			if(moduleInternalName)
			{
				// Don't check for duplicate names for now, since the ml-proto tests rely on name shadowing.
				/*if(moduleInternalNameToIndexMap.count(moduleInternalName)) { recordError(moduleInternalNameIt,std::string("duplicate module name: ") + moduleInternalName); }
				else*/ { moduleInternalNameToIndexMap[moduleInternalName] = lastModuleInstance; }
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
				moduleNameToIndexMap[moduleName] = mapIt->second;
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
			Result actionResult;
			try { processAction(rootNodeIt,actionResult); }
			catch(Exception exception) { recordError(rootNodeIt,"unexpected trap: " + describeRuntimeException(exception)); }
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

DEFINE_INTRINSIC_GLOBAL(spectest,spectest_globalI32,global,i32,false,666)
DEFINE_INTRINSIC_GLOBAL(spectest,spectest_globalI64,global,i64,false,0)
DEFINE_INTRINSIC_GLOBAL(spectest,spectest_globalF32,global,f32,false,0.0f)
DEFINE_INTRINSIC_GLOBAL(spectest,spectest_globalF64,global,f64,false,0.0)

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
	
	// Always enable debug logging for tests.
	Log::setCategoryEnabled(Log::Category::debug,true);

	init();
	
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

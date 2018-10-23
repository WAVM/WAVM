#include <inttypes.h>
#include <stdlib.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/CLI.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/FloatComponents.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Platform/Thread.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Linker.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/Runtime/RuntimeData.h"
#include "WAVM/ThreadTest/ThreadTest.h"
#include "WAVM/WASTParse/TestScript.h"
#include "WAVM/WASTParse/WASTParse.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;
using namespace WAVM::WAST;

DEFINE_INTRINSIC_MODULE(spectest);

struct TestScriptState
{
	bool hasInstantiatedModule;
	GCPointer<ModuleInstance> lastModuleInstance;
	GCPointer<Compartment> compartment;
	GCPointer<Context> context;

	HashMap<std::string, GCPointer<ModuleInstance>> moduleInternalNameToInstanceMap;
	HashMap<std::string, GCPointer<ModuleInstance>> moduleNameToInstanceMap;

	std::vector<WAST::Error> errors;

	TestScriptState()
	: hasInstantiatedModule(false)
	, compartment(Runtime::createCompartment())
	, context(Runtime::createContext(compartment))
	{
		moduleNameToInstanceMap.set(
			"spectest",
			Intrinsics::instantiateModule(compartment, INTRINSIC_MODULE_REF(spectest), "spectest"));
		moduleNameToInstanceMap.set("threadTest", ThreadTest::instantiate(compartment));
	}

	~TestScriptState()
	{
		// Ensure that the compartment is garbage-collected after clearing all the references held
		// by the script state.
		lastModuleInstance = nullptr;
		context = nullptr;
		moduleInternalNameToInstanceMap.clear();
		moduleNameToInstanceMap.clear();
		errorUnless(tryCollectCompartment(std::move(compartment)));
	}
};

struct TestScriptResolver : Resolver
{
	TestScriptResolver(const TestScriptState& inState) : state(inState) {}
	bool resolve(const std::string& moduleName,
				 const std::string& exportName,
				 ExternType type,
				 Object*& outObject) override
	{
		auto namedModule = state.moduleNameToInstanceMap.get(moduleName);
		if(namedModule && *namedModule)
		{
			outObject = getInstanceExport(*namedModule, exportName);
			return outObject != nullptr && isA(outObject, type);
		}

		return false;
	}

private:
	const TestScriptState& state;
};

VALIDATE_AS_PRINTF(3, 4)
static void testErrorf(TestScriptState& state,
					   const TextFileLocus& locus,
					   const char* messageFormat,
					   ...)
{
	va_list messageArgs;
	va_start(messageArgs, messageFormat);

	// Call vsnprintf to determine how many bytes the formatted string will be.
	// vsnprintf consumes the va_list passed to it, so make a copy of it.
	va_list messageArgsProbe;
	va_copy(messageArgsProbe, messageArgs);
	int numFormattedChars = std::vsnprintf(nullptr, 0, messageFormat, messageArgsProbe);
	va_end(messageArgsProbe);

	// Allocate a buffer for the formatted message.
	errorUnless(numFormattedChars >= 0);
	std::string formattedMessage;
	formattedMessage.resize(numFormattedChars);

	// Print the formatted message
	int numWrittenChars = std::vsnprintf(
		(char*)formattedMessage.data(), numFormattedChars + 1, messageFormat, messageArgs);
	wavmAssert(numWrittenChars == numFormattedChars);
	va_end(messageArgs);

	// Add the error to the cursor's error list.
	state.errors.push_back({locus, std::move(formattedMessage)});
}

static ModuleInstance* getModuleContextByInternalName(TestScriptState& state,
													  const TextFileLocus& locus,
													  const char* context,
													  const std::string& internalName)
{
	// Look up the module this invoke uses.
	if(!state.hasInstantiatedModule)
	{
		testErrorf(state, locus, "no module to use in %s", context);
		return nullptr;
	}
	ModuleInstance* moduleInstance = state.lastModuleInstance;
	if(internalName.size())
	{
		auto namedModule = state.moduleInternalNameToInstanceMap.get(internalName);
		if(!namedModule)
		{
			testErrorf(state, locus, "unknown %s module name: %s", context, internalName.c_str());
			return nullptr;
		}
		moduleInstance = *namedModule;
	}
	return moduleInstance;
}

static Runtime::ExceptionType* getExpectedExceptionType(WAST::ExpectedTrapType expectedType)
{
	switch(expectedType)
	{
	case WAST::ExpectedTrapType::outOfBoundsMemoryAccess:
		return Runtime::Exception::outOfBoundsMemoryAccessType;
	case WAST::ExpectedTrapType::outOfBoundsTableAccess:
		return Runtime::Exception::outOfBoundsTableAccessType;
	case WAST::ExpectedTrapType::outOfBoundsDataSegmentAccess:
		return Runtime::Exception::outOfBoundsDataSegmentAccessType;
	case WAST::ExpectedTrapType::outOfBoundsElemSegmentAccess:
		return Runtime::Exception::outOfBoundsElemSegmentAccessType;
	case WAST::ExpectedTrapType::stackOverflow: return Runtime::Exception::stackOverflowType;
	case WAST::ExpectedTrapType::integerDivideByZeroOrIntegerOverflow:
		return Runtime::Exception::integerDivideByZeroOrOverflowType;
	case WAST::ExpectedTrapType::invalidFloatOperation:
		return Runtime::Exception::invalidFloatOperationType;
	case WAST::ExpectedTrapType::invokeSignatureMismatch:
		return Runtime::Exception::invokeSignatureMismatchType;
	case WAST::ExpectedTrapType::reachedUnreachable:
		return Runtime::Exception::reachedUnreachableType;
	case WAST::ExpectedTrapType::indirectCallSignatureMismatch:
		return Runtime::Exception::indirectCallSignatureMismatchType;
	case WAST::ExpectedTrapType::uninitializedTableElement:
		return Runtime::Exception::uninitializedTableElementType;
	case WAST::ExpectedTrapType::outOfMemory: return Runtime::Exception::outOfMemoryType;
	case WAST::ExpectedTrapType::misalignedAtomicMemoryAccess:
		return Runtime::Exception::misalignedAtomicMemoryAccessType;
	case WAST::ExpectedTrapType::invalidArgument: return Runtime::Exception::invalidArgumentType;
	default: Errors::unreachable();
	};
}

static bool processAction(TestScriptState& state, Action* action, IR::ValueTuple& outResults)
{
	outResults = IR::ValueTuple();

	switch(action->type)
	{
	case ActionType::_module:
	{
		auto moduleAction = (ModuleAction*)action;

		// Clear the previous module.
		state.lastModuleInstance = nullptr;
		collectCompartmentGarbage(state.compartment);

		// Link and instantiate the module.
		TestScriptResolver resolver(state);
		LinkResult linkResult = linkModule(*moduleAction->module, resolver);
		if(linkResult.success)
		{
			state.hasInstantiatedModule = true;
			state.lastModuleInstance = instantiateModule(state.compartment,
														 compileModule(*moduleAction->module),
														 std::move(linkResult.resolvedImports),
														 "test module");

			// Call the module start function, if it has one.
			Function* startFunction = getStartFunction(state.lastModuleInstance);
			if(startFunction) { invokeFunctionChecked(state.context, startFunction, {}); }
		}
		else
		{
			// Create an error for each import that couldn't be linked.
			for(auto& missingImport : linkResult.missingImports)
			{
				testErrorf(state,
						   moduleAction->locus,
						   "missing import module=\"%s\" export=\"%s\" type=\"%s\"",
						   missingImport.moduleName.c_str(),
						   missingImport.exportName.c_str(),
						   asString(missingImport.type).c_str());
			}
		}

		// Register the module under its internal name.
		if(moduleAction->internalModuleName.size())
		{
			state.moduleInternalNameToInstanceMap.set(moduleAction->internalModuleName,
													  state.lastModuleInstance);
		}

		return true;
	}
	case ActionType::invoke:
	{
		auto invokeAction = (InvokeAction*)action;

		// Look up the module this invoke uses.
		ModuleInstance* moduleInstance = getModuleContextByInternalName(
			state, invokeAction->locus, "invoke", invokeAction->internalModuleName);

		// A null module instance at this point indicates a module that failed to link or
		// instantiate, so don't produce further errors.
		if(!moduleInstance) { return false; }

		// Find the named export in the module instance.
		auto function
			= asFunctionNullable(getInstanceExport(moduleInstance, invokeAction->exportName));
		if(!function)
		{
			testErrorf(state,
					   invokeAction->locus,
					   "couldn't find exported function with name: %s",
					   invokeAction->exportName.c_str());
			return false;
		}

		// Execute the invoke
		outResults = invokeFunctionChecked(state.context, function, invokeAction->arguments.values);

		return true;
	}
	case ActionType::get:
	{
		auto getAction = (GetAction*)action;

		// Look up the module this get uses.
		ModuleInstance* moduleInstance = getModuleContextByInternalName(
			state, getAction->locus, "get", getAction->internalModuleName);

		// A null module instance at this point indicates a module that failed to link or
		// instantiate, so just return without further errors.
		if(!moduleInstance) { return false; }

		// Find the named export in the module instance.
		auto global = asGlobalNullable(getInstanceExport(moduleInstance, getAction->exportName));
		if(!global)
		{
			testErrorf(state,
					   getAction->locus,
					   "couldn't find exported global with name: %s",
					   getAction->exportName.c_str());
			return false;
		}

		// Get the value of the specified global.
		outResults = getGlobalValue(state.context, global);

		return true;
	}
	default: Errors::unreachable();
	}
}

// Tests whether a float is a "canonical" NaN, which just means that it's a NaN only the MSB of its
// significand set.
template<typename Float> bool isCanonicalOrArithmeticNaN(Float value, bool requireCanonical)
{
	FloatComponents<Float> components;
	components.value = value;
	return components.bits.exponent == FloatComponents<Float>::maxExponentBits
		   && (!requireCanonical
			   || components.bits.significand == FloatComponents<Float>::canonicalSignificand);
}

static void processCommand(TestScriptState& state, const Command* command)
{
	switch(command->type)
	{
	case Command::_register:
	{
		auto registerCommand = (RegisterCommand*)command;

		// Look up a module by internal name, and bind the result to the public name.
		ModuleInstance* moduleInstance = getModuleContextByInternalName(
			state, registerCommand->locus, "register", registerCommand->internalModuleName);
		state.moduleNameToInstanceMap.set(registerCommand->moduleName, moduleInstance);
		break;
	}
	case Command::action:
	{
		IR::ValueTuple results;
		processAction(state, ((ActionCommand*)command)->action.get(), results);
		break;
	}
	case Command::assert_return:
	{
		auto assertCommand = (AssertReturnCommand*)command;
		// Execute the action and do a bitwise comparison of the result to the expected result.
		IR::ValueTuple actionResults;
		if(processAction(state, assertCommand->action.get(), actionResults)
		   && actionResults != assertCommand->expectedResults)
		{
			testErrorf(state,
					   assertCommand->locus,
					   "expected %s but got %s",
					   asString(assertCommand->expectedResults).c_str(),
					   asString(actionResults).c_str());
		}
		break;
	}
	case Command::assert_return_canonical_nan:
	case Command::assert_return_arithmetic_nan:
	{
		auto assertCommand = (AssertReturnNaNCommand*)command;
		// Execute the action and check that the result is a NaN of the expected type.
		IR::ValueTuple actionResults;
		if(processAction(state, assertCommand->action.get(), actionResults))
		{
			if(actionResults.size() != 1)
			{
				testErrorf(state,
						   assertCommand->locus,
						   "expected single floating-point result, but got %s",
						   asString(actionResults).c_str());
			}
			else
			{
				IR::Value actionResult = actionResults[0];
				const bool requireCanonicalNaN
					= assertCommand->type == Command::assert_return_canonical_nan;
				const bool isError
					= actionResult.type == ValueType::f32
						  ? !isCanonicalOrArithmeticNaN(actionResult.f32, requireCanonicalNaN)
						  : actionResult.type == ValueType::f64
								? !isCanonicalOrArithmeticNaN(actionResult.f64, requireCanonicalNaN)
								: true;
				if(isError)
				{
					testErrorf(state,
							   assertCommand->locus,
							   requireCanonicalNaN ? "expected canonical float NaN but got %s"
												   : "expected float NaN but got %s",
							   asString(actionResult).c_str());
				}
			}
		}
		break;
	}
	case Command::assert_return_func:
	{
		auto assertCommand = (AssertReturnNaNCommand*)command;
		// Execute the action and check that the result is a function.
		IR::ValueTuple actionResults;
		if(processAction(state, assertCommand->action.get(), actionResults))
		{
			if(actionResults.size() != 1 || !isReferenceType(actionResults[0].type)
			   || !asFunctionNullable(actionResults[0].object))
			{
				testErrorf(state,
						   assertCommand->locus,
						   "expected single reference result but got %s",
						   asString(actionResults).c_str());
			}
		}
		break;
	}
	case Command::assert_trap:
	{
		auto assertCommand = (AssertTrapCommand*)command;
		Runtime::catchRuntimeExceptions(
			[&] {
				IR::ValueTuple actionResults;
				if(processAction(state, assertCommand->action.get(), actionResults))
				{
					testErrorf(state,
							   assertCommand->locus,
							   "expected trap but got %s",
							   asString(actionResults).c_str());
				}
			},
			[&](Runtime::Exception&& exception) {
				Runtime::ExceptionType* expectedType
					= getExpectedExceptionType(assertCommand->expectedType);
				if(exception.type != expectedType)
				{
					testErrorf(state,
							   assertCommand->action->locus,
							   "expected %s trap but got %s trap",
							   describeExceptionType(expectedType).c_str(),
							   describeExceptionType(exception.type).c_str());
				}
			});
		break;
	}
	case Command::assert_throws:
	{
		auto assertCommand = (AssertThrowsCommand*)command;

		// Look up the module containing the expected exception type.
		ModuleInstance* moduleInstance
			= getModuleContextByInternalName(state,
											 assertCommand->locus,
											 "assert_throws",
											 assertCommand->exceptionTypeInternalModuleName);

		// A null module instance at this point indicates a module that failed to link or
		// instantiate, so don't produce further errors.
		if(!moduleInstance) { break; }

		// Find the named export in the module instance.
		auto expectedExceptionType = asExceptionTypeNullable(
			getInstanceExport(moduleInstance, assertCommand->exceptionTypeExportName));
		if(!expectedExceptionType)
		{
			testErrorf(state,
					   assertCommand->locus,
					   "couldn't find exported exception type with name: %s",
					   assertCommand->exceptionTypeExportName.c_str());
			break;
		}

		Runtime::catchRuntimeExceptions(
			[&] {
				IR::ValueTuple actionResults;
				if(processAction(state, assertCommand->action.get(), actionResults))
				{
					testErrorf(state,
							   assertCommand->locus,
							   "expected trap but got %s",
							   asString(actionResults).c_str());
				}
			},
			[&](Runtime::Exception&& exception) {
				if(exception.type != expectedExceptionType)
				{
					testErrorf(state,
							   assertCommand->action->locus,
							   "expected %s exception but got %s exception",
							   describeExceptionType(expectedExceptionType).c_str(),
							   describeExceptionType(exception.type).c_str());
				}
				else
				{
					TypeTuple exceptionParameterTypes
						= getExceptionTypeParameters(expectedExceptionType);
					wavmAssert(exception.arguments.size() == exceptionParameterTypes.size());

					for(Uptr argumentIndex = 0; argumentIndex < exception.arguments.size();
						++argumentIndex)
					{
						IR::Value argumentValue(exceptionParameterTypes[argumentIndex],
												exception.arguments[argumentIndex]);
						if(argumentValue != assertCommand->expectedArguments[argumentIndex])
						{
							testErrorf(
								state,
								assertCommand->locus,
								"expected %s for exception argument %" PRIuPTR " but got %s",
								asString(assertCommand->expectedArguments[argumentIndex]).c_str(),
								argumentIndex,
								asString(argumentValue).c_str());
						}
					}
				}
			});
		break;
	}
	case Command::assert_invalid:
	case Command::assert_malformed:
	{
		auto assertCommand = (AssertInvalidOrMalformedCommand*)command;
		if(!assertCommand->wasInvalidOrMalformed)
		{
			testErrorf(state,
					   assertCommand->locus,
					   "module was %s",
					   assertCommand->type == Command::assert_invalid ? "valid" : "well formed");
		}
		break;
	}
	case Command::assert_unlinkable:
	{
		auto assertCommand = (AssertUnlinkableCommand*)command;
		Runtime::catchRuntimeExceptions(
			[&] {
				TestScriptResolver resolver(state);
				LinkResult linkResult = linkModule(*assertCommand->moduleAction->module, resolver);
				if(linkResult.success)
				{
					auto moduleInstance
						= instantiateModule(state.compartment,
											compileModule(*assertCommand->moduleAction->module),
											std::move(linkResult.resolvedImports),
											"test module");

					// Call the module start function, if it has one.
					Function* startFunction = getStartFunction(moduleInstance);
					if(startFunction) { invokeFunctionChecked(state.context, startFunction, {}); }

					testErrorf(state, assertCommand->locus, "module was linkable");
				}
			},
			[&](Runtime::Exception&& exception) {
				// If the instantiation throws an exception, the assert_unlinkable succeeds.
			});
		break;
	}
	};
}

DEFINE_INTRINSIC_FUNCTION(spectest, "print", void, spectest_print) {}
DEFINE_INTRINSIC_FUNCTION(spectest, "print_i32", void, spectest_print_i32, I32 a)
{
	Log::printf(Log::debug, "%s : i32\n", asString(a).c_str());
}
DEFINE_INTRINSIC_FUNCTION(spectest, "print_i64", void, spectest_print_i64, I64 a)
{
	Log::printf(Log::debug, "%s : i64\n", asString(a).c_str());
}
DEFINE_INTRINSIC_FUNCTION(spectest, "print_f32", void, spectest_print_f32, F32 a)
{
	Log::printf(Log::debug, "%s : f32\n", asString(a).c_str());
}
DEFINE_INTRINSIC_FUNCTION(spectest, "print_f64", void, spectest_print_f64, F64 a)
{
	Log::printf(Log::debug, "%s : f64\n", asString(a).c_str());
}
DEFINE_INTRINSIC_FUNCTION(spectest, "print_f64_f64", void, spectest_print_f64_f64, F64 a, F64 b)
{
	Log::printf(Log::debug, "%s : f64\n%s : f64\n", asString(a).c_str(), asString(b).c_str());
}
DEFINE_INTRINSIC_FUNCTION(spectest, "print_i32_f32", void, spectest_print_i32_f32, I32 a, F32 b)
{
	Log::printf(Log::debug, "%s : i32\n%s : f32\n", asString(a).c_str(), asString(b).c_str());
}
DEFINE_INTRINSIC_FUNCTION(spectest, "print_i64_f64", void, spectest_print_i64_f64, I64 a, F64 b)
{
	Log::printf(Log::debug, "%s : i64\n%s : f64\n", asString(a).c_str(), asString(b).c_str());
}

DEFINE_INTRINSIC_GLOBAL(spectest, "global_i32", I32, spectest_global_i32, 666)
DEFINE_INTRINSIC_GLOBAL(spectest, "global_i64", I64, spectest_global_i64, 0)
DEFINE_INTRINSIC_GLOBAL(spectest, "global_f32", F32, spectest_global_f32, 0.0f)
DEFINE_INTRINSIC_GLOBAL(spectest, "global_f64", F64, spectest_global_f64, 0.0)

DEFINE_INTRINSIC_TABLE(spectest,
					   spectest_table,
					   table,
					   TableType(ReferenceType::anyfunc, false, SizeConstraints{10, 20}))
DEFINE_INTRINSIC_MEMORY(spectest, spectest_memory, memory, MemoryType(false, SizeConstraints{1, 2}))
DEFINE_INTRINSIC_MEMORY(spectest,
						spectest_shared_memory,
						shared_memory,
						MemoryType(true, SizeConstraints{1, 2}))

struct SharedState
{
	Platform::Mutex mutex;
	std::vector<const char*> pendingFilenames;
};

static I64 threadMain(void* sharedStateVoid)
{
	auto sharedState = (SharedState*)sharedStateVoid;

	// Process files from sharedState->pendingFilenames until they've all been processed.
	I64 numErrors = 0;
	while(true)
	{
		const char* filename;
		{
			Lock<Platform::Mutex> sharedStateLock(sharedState->mutex);
			if(!sharedState->pendingFilenames.size()) { break; }
			filename = sharedState->pendingFilenames.back();
			sharedState->pendingFilenames.pop_back();
		}

		// Read the file into a vector.
		std::vector<U8> testScriptBytes;
		if(!loadFile(filename, testScriptBytes))
		{
			++numErrors;
			continue;
		}

		// Make sure the file is null terminated.
		testScriptBytes.push_back(0);

		// Process the test script.
		TestScriptState testScriptState;
		std::vector<std::unique_ptr<Command>> testCommands;

		// Use a WebAssembly standard-compliant feature spec.
		FeatureSpec featureSpec;
		featureSpec.requireSharedFlagForAtomicOperators = true;

		// Parse the test script.
		WAST::parseTestCommands((const char*)testScriptBytes.data(),
								testScriptBytes.size(),
								featureSpec,
								testCommands,
								testScriptState.errors);
		if(!testScriptState.errors.size())
		{
			// Process the test script commands.
			for(auto& command : testCommands)
			{
				Log::printf(Log::debug,
							"Evaluating test command at %s(%s)\n",
							filename,
							command->locus.describe().c_str());
				catchRuntimeExceptions(
					[&testScriptState, &command] {
						processCommand(testScriptState, command.get());
					},
					[&testScriptState, &command](Runtime::Exception&& exception) {
						testErrorf(testScriptState,
								   command->locus,
								   "unexpected trap: %s",
								   describeExceptionType(exception.type).c_str());
					});
			}
		}
		numErrors += testScriptState.errors.size();

		// Print any errors.
		reportParseErrors(filename, testScriptState.errors);
	}

	return numErrors;
}

static void showHelp()
{
	Log::printf(Log::error,
				"Usage: RunTestScript [options] in.wast [options]\n"
				"  -h|--help   Display this message\n"
				"  -d|--debug  Print verbose debug output to stdout\n");
}

int main(int argc, char** argv)
{
	Timing::Timer timer;

	// Treat any unhandled exception (e.g. in a thread) as a fatal error.
	Runtime::setUnhandledExceptionHandler([](Runtime::Exception&& exception) {
		Errors::fatalf("Unhandled runtime exception: %s", describeException(exception).c_str());
	});

	// Suppress metrics output.
	Log::setCategoryEnabled(Log::metrics, false);

	// Parse the command-line.
	SharedState sharedState;
	for(int argIndex = 1; argIndex < argc; ++argIndex)
	{
		if(!strcmp(argv[argIndex], "--help") || !strcmp(argv[argIndex], "-h"))
		{
			showHelp();
			return EXIT_SUCCESS;
		}
		else if(!strcmp(argv[argIndex], "--debug") || !strcmp(argv[argIndex], "-d"))
		{
			Log::setCategoryEnabled(Log::debug, true);
		}
		else
		{
			sharedState.pendingFilenames.push_back(argv[argIndex]);
		}
	}

	if(!sharedState.pendingFilenames.size())
	{
		showHelp();
		return EXIT_FAILURE;
	}

	// Create a thread for each hardware thread.
	std::vector<Platform::Thread*> threads;
	const Uptr numHardwareThreads = Platform::getNumberOfHardwareThreads();
	const Uptr numTestThreads
		= std::min(numHardwareThreads, Uptr(sharedState.pendingFilenames.size()));
	for(Uptr threadIndex = 0; threadIndex < numTestThreads; ++threadIndex)
	{ threads.push_back(Platform::createThread(1024 * 1024, threadMain, &sharedState)); }

	// Wait for the threads to exit, summing up their return code, which will be the number of
	// errors found by the thread.
	I64 numErrors = 0;
	for(Platform::Thread* thread : threads) { numErrors += Platform::joinThread(thread); }

	if(!numErrors)
	{
		Log::printf(Log::output, "All tests succeeded in %.1fms!\n", timer.getMilliseconds());
		return EXIT_SUCCESS;
	}
	else
	{
		Log::printf(Log::error, "Testing failed with %" PRIi64 " error(s)\n", numErrors);
		return EXIT_FAILURE;
	}
}

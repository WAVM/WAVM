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
#include "WAVM/Inline/Timing.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Memory.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Platform/Thread.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Linker.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"
#include "WAVM/ThreadTest/ThreadTest.h"
#include "WAVM/WASTParse/TestScript.h"
#include "WAVM/WASTParse/WASTParse.h"
#include "wavm-test.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;
using namespace WAVM::WAST;

WAVM_DEFINE_INTRINSIC_MODULE(spectest);

struct Config
{
	bool strictAssertInvalid{false};
	bool strictAssertMalformed{false};
	bool testCloning{false};
};

struct TestScriptState
{
	const Config& config;

	bool hasInstantiatedModule;
	GCPointer<ModuleInstance> lastModuleInstance;
	GCPointer<Compartment> compartment;
	GCPointer<Context> context;

	HashMap<std::string, GCPointer<ModuleInstance>> moduleInternalNameToInstanceMap;
	HashMap<std::string, GCPointer<ModuleInstance>> moduleNameToInstanceMap;

	std::vector<WAST::Error> errors;

	TestScriptState(const Config& inConfig)
	: config(inConfig)
	, hasInstantiatedModule(false)
	, compartment(Runtime::createCompartment())
	, context(Runtime::createContext(compartment))
	{
		moduleNameToInstanceMap.set(
			"spectest",
			Intrinsics::instantiateModule(
				compartment, {WAVM_INTRINSIC_MODULE_REF(spectest)}, "spectest"));
		moduleNameToInstanceMap.set("threadTest", ThreadTest::instantiate(compartment));
	}

	TestScriptState(const TestScriptState& copyee)
	: config(copyee.config), hasInstantiatedModule(copyee.hasInstantiatedModule)
	{
		compartment = Runtime::cloneCompartment(copyee.compartment);
		context = Runtime::cloneContext(copyee.context, compartment);

		lastModuleInstance
			= Runtime::remapToClonedCompartment(copyee.lastModuleInstance, compartment);

		for(const auto& pair : copyee.moduleInternalNameToInstanceMap)
		{
			moduleInternalNameToInstanceMap.addOrFail(
				pair.key, Runtime::remapToClonedCompartment(pair.value, compartment));
		}

		for(const auto& pair : copyee.moduleNameToInstanceMap)
		{
			moduleNameToInstanceMap.addOrFail(
				pair.key, Runtime::remapToClonedCompartment(pair.value, compartment));
		}

		errors = copyee.errors;
	}

	~TestScriptState()
	{
		// Ensure that the compartment is garbage-collected after clearing all the references held
		// by the script state.
		lastModuleInstance = nullptr;
		context = nullptr;
		moduleInternalNameToInstanceMap.clear();
		moduleNameToInstanceMap.clear();
		WAVM_ERROR_UNLESS(tryCollectCompartment(std::move(compartment)));
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

WAVM_VALIDATE_AS_PRINTF(3, 4)
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
	WAVM_ERROR_UNLESS(numFormattedChars >= 0);
	std::string formattedMessage;
	formattedMessage.resize(numFormattedChars);

	// Print the formatted message
	int numWrittenChars = std::vsnprintf(
		(char*)formattedMessage.data(), numFormattedChars + 1, messageFormat, messageArgs);
	WAVM_ASSERT(numWrittenChars == numFormattedChars);
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

static Runtime::ExceptionType* getExpectedTrapType(WAST::ExpectedTrapType expectedType)
{
	switch(expectedType)
	{
	case WAST::ExpectedTrapType::outOfBoundsMemoryAccess:
		return Runtime::ExceptionTypes::outOfBoundsMemoryAccess;
	case WAST::ExpectedTrapType::outOfBoundsTableAccess:
		return Runtime::ExceptionTypes::outOfBoundsTableAccess;
	case WAST::ExpectedTrapType::outOfBoundsDataSegmentAccess:
		return Runtime::ExceptionTypes::outOfBoundsDataSegmentAccess;
	case WAST::ExpectedTrapType::outOfBoundsElemSegmentAccess:
		return Runtime::ExceptionTypes::outOfBoundsElemSegmentAccess;
	case WAST::ExpectedTrapType::stackOverflow: return Runtime::ExceptionTypes::stackOverflow;
	case WAST::ExpectedTrapType::integerDivideByZeroOrIntegerOverflow:
		return Runtime::ExceptionTypes::integerDivideByZeroOrOverflow;
	case WAST::ExpectedTrapType::invalidFloatOperation:
		return Runtime::ExceptionTypes::invalidFloatOperation;
	case WAST::ExpectedTrapType::invokeSignatureMismatch:
		return Runtime::ExceptionTypes::invokeSignatureMismatch;
	case WAST::ExpectedTrapType::reachedUnreachable:
		return Runtime::ExceptionTypes::reachedUnreachable;
	case WAST::ExpectedTrapType::indirectCallSignatureMismatch:
		return Runtime::ExceptionTypes::indirectCallSignatureMismatch;
	case WAST::ExpectedTrapType::uninitializedTableElement:
		return Runtime::ExceptionTypes::uninitializedTableElement;
	case WAST::ExpectedTrapType::outOfMemory: return Runtime::ExceptionTypes::outOfMemory;
	case WAST::ExpectedTrapType::misalignedAtomicMemoryAccess:
		return Runtime::ExceptionTypes::misalignedAtomicMemoryAccess;
	case WAST::ExpectedTrapType::invalidArgument: return Runtime::ExceptionTypes::invalidArgument;

	case WAST::ExpectedTrapType::outOfBounds:
	default: WAVM_UNREACHABLE();
	};
}

static std::string describeExpectedTrapType(WAST::ExpectedTrapType expectedType)
{
	if(expectedType == WAST::ExpectedTrapType::outOfBounds) { return "wavm.outOfBounds*"; }
	else
	{
		return describeExceptionType(getExpectedTrapType(expectedType));
	}
}

static bool isExpectedExceptionType(WAST::ExpectedTrapType expectedType,
									Runtime::ExceptionType* actualType)
{
	// WAVM has more precise trap types for out-of-bounds accesses than the spec tests.
	if(expectedType == WAST::ExpectedTrapType::outOfBounds)
	{
		return actualType == Runtime::ExceptionTypes::outOfBoundsMemoryAccess
			   || actualType == Runtime::ExceptionTypes::outOfBoundsDataSegmentAccess
			   || actualType == Runtime::ExceptionTypes::outOfBoundsTableAccess
			   || actualType == Runtime::ExceptionTypes::outOfBoundsElemSegmentAccess;
	}
	else if(expectedType == WAST::ExpectedTrapType::outOfBoundsMemoryAccess)
	{
		return actualType == Runtime::ExceptionTypes::outOfBoundsMemoryAccess
			   || actualType == Runtime::ExceptionTypes::outOfBoundsDataSegmentAccess;
	}
	else if(expectedType == WAST::ExpectedTrapType::outOfBoundsTableAccess)
	{
		return actualType == Runtime::ExceptionTypes::outOfBoundsTableAccess
			   || actualType == Runtime::ExceptionTypes::outOfBoundsElemSegmentAccess;
	}
	else
	{
		return getExpectedTrapType(expectedType) == actualType;
	}
}

static bool processAction(TestScriptState& state, Action* action, std::vector<Value>& outResults)
{
	outResults.clear();

	switch(action->type)
	{
	case ActionType::_module: {
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
			if(startFunction) { invokeFunction(state.context, startFunction); }
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
	case ActionType::invoke: {
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

		// Split the tagged argument values into their types and untagged values.
		std::vector<ValueType> argTypes;
		std::vector<UntaggedValue> untaggedArgs;
		for(const Value& arg : invokeAction->arguments)
		{
			argTypes.push_back(arg.type);
			untaggedArgs.push_back(arg);
		}

		// Infer the expected type of the function from the number and type of the invoke's
		// arguments and the function's actual result types.
		const FunctionType invokeSig(getFunctionType(function).results(), TypeTuple(argTypes));

		// Allocate an array to receive the invoke results.
		std::vector<UntaggedValue> untaggedResults;
		untaggedResults.resize(invokeSig.results().size());

		// Invoke the function.
		invokeFunction(
			state.context, function, invokeSig, untaggedArgs.data(), untaggedResults.data());

		// Convert the untagged result values to tagged values.
		outResults.resize(invokeSig.results().size());
		for(Uptr resultIndex = 0; resultIndex < untaggedResults.size(); ++resultIndex)
		{
			const ValueType resultType = invokeSig.results()[resultIndex];
			const UntaggedValue& untaggedResult = untaggedResults[resultIndex];
			outResults[resultIndex] = Value(resultType, untaggedResult);
		}

		return true;
	}
	case ActionType::get: {
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
		outResults = {getGlobalValue(state.context, global)};

		return true;
	}
	default: WAVM_UNREACHABLE();
	}
}

// Tests whether a float is an "arithmetic" NaN, which have the MSB of the significand set.
template<typename Float> bool isArithmeticNaN(Float value)
{
	FloatComponents<Float> components;
	components.value = value;
	return components.bits.exponent == FloatComponents<Float>::maxExponentBits
		   && components.bits.significand >= FloatComponents<Float>::canonicalSignificand;
}

// Tests whether a float is a "canonical" NaN, which *only* have the MSB of the significand set.
template<typename Float> bool isCanonicalNaN(Float value)
{
	FloatComponents<Float> components;
	components.value = value;
	return components.bits.exponent == FloatComponents<Float>::maxExponentBits
		   && components.bits.significand == FloatComponents<Float>::canonicalSignificand;
}

static void processCommand(TestScriptState& state, const Command* command)
{
	switch(command->type)
	{
	case Command::_register: {
		auto registerCommand = (RegisterCommand*)command;

		// Look up a module by internal name, and bind the result to the public name.
		ModuleInstance* moduleInstance = getModuleContextByInternalName(
			state, registerCommand->locus, "register", registerCommand->internalModuleName);
		state.moduleNameToInstanceMap.set(registerCommand->moduleName, moduleInstance);
		break;
	}
	case Command::action: {
		std::vector<Value> results;
		processAction(state, ((ActionCommand*)command)->action.get(), results);
		break;
	}
	case Command::assert_return: {
		auto assertCommand = (AssertReturnCommand*)command;
		// Execute the action and do a bitwise comparison of the result to the expected result.
		std::vector<Value> actionResults;
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
	case Command::assert_return_canonical_nan_f32x4:
	case Command::assert_return_arithmetic_nan_f32x4:
	case Command::assert_return_canonical_nan_f64x2:
	case Command::assert_return_arithmetic_nan_f64x2: {
		auto assertCommand = (AssertReturnNaNCommand*)command;
		// Execute the action and check that the result is a NaN of the expected type.
		std::vector<Value> actionResults;
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
				Value actionResult = actionResults[0];
				bool requireCanonicalNaN = false;
				bool isError;
				if(assertCommand->type == Command::assert_return_canonical_nan)
				{
					requireCanonicalNaN = true;
					isError = actionResult.type == ValueType::f32
								  ? !isCanonicalNaN(actionResult.f32)
								  : actionResult.type == ValueType::f64
										? !isCanonicalNaN(actionResult.f64)
										: true;
				}
				else if(assertCommand->type == Command::assert_return_arithmetic_nan)
				{
					isError = actionResult.type == ValueType::f32
								  ? !isArithmeticNaN(actionResult.f32)
								  : actionResult.type == ValueType::f64
										? !isArithmeticNaN(actionResult.f64)
										: true;
				}
				else if(assertCommand->type == Command::assert_return_canonical_nan_f32x4)
				{
					requireCanonicalNaN = true;
					isError = !isCanonicalNaN(actionResult.v128.f32[0])
							  || !isCanonicalNaN(actionResult.v128.f32[1])
							  || !isCanonicalNaN(actionResult.v128.f32[2])
							  || !isCanonicalNaN(actionResult.v128.f32[3]);
				}
				else if(assertCommand->type == Command::assert_return_arithmetic_nan_f32x4)
				{
					isError = !isArithmeticNaN(actionResult.v128.f32[0])
							  || !isArithmeticNaN(actionResult.v128.f32[1])
							  || !isArithmeticNaN(actionResult.v128.f32[2])
							  || !isArithmeticNaN(actionResult.v128.f32[3]);
				}
				else if(assertCommand->type == Command::assert_return_canonical_nan_f64x2)
				{
					requireCanonicalNaN = true;
					isError = !isCanonicalNaN(actionResult.v128.f64[0])
							  || !isCanonicalNaN(actionResult.v128.f64[1]);
				}
				else if(assertCommand->type == Command::assert_return_arithmetic_nan_f64x2)
				{
					isError = !isArithmeticNaN(actionResult.v128.f64[0])
							  || !isArithmeticNaN(actionResult.v128.f64[1]);
				}
				else
				{
					WAVM_UNREACHABLE();
				}

				if(isError)
				{
					testErrorf(state,
							   assertCommand->locus,
							   requireCanonicalNaN ? "expected canonical NaN but got %s"
												   : "expected arithmetic NaN but got %s",
							   asString(actionResult).c_str());
				}
			}
		}
		break;
	}
	case Command::assert_return_func: {
		auto assertCommand = (AssertReturnNaNCommand*)command;
		// Execute the action and check that the result is a function.
		std::vector<Value> actionResults;
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
	case Command::assert_trap: {
		auto assertCommand = (AssertTrapCommand*)command;
		Runtime::catchRuntimeExceptions(
			[&] {
				std::vector<Value> actionResults;
				if(processAction(state, assertCommand->action.get(), actionResults))
				{
					testErrorf(state,
							   assertCommand->locus,
							   "expected trap but got %s",
							   asString(actionResults).c_str());
				}
			},
			[&](Runtime::Exception* exception) {
				if(!isExpectedExceptionType(assertCommand->expectedType, exception->type))
				{
					testErrorf(state,
							   assertCommand->action->locus,
							   "expected %s trap but got %s trap",
							   describeExpectedTrapType(assertCommand->expectedType).c_str(),
							   describeExceptionType(exception->type).c_str());
				}
				destroyException(exception);
			});
		break;
	}
	case Command::assert_throws: {
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
				std::vector<Value> actionResults;
				if(processAction(state, assertCommand->action.get(), actionResults))
				{
					testErrorf(state,
							   assertCommand->locus,
							   "expected trap but got %s",
							   asString(actionResults).c_str());
				}
			},
			[&](Runtime::Exception* exception) {
				if(exception->type != expectedExceptionType)
				{
					testErrorf(state,
							   assertCommand->action->locus,
							   "expected %s exception but got %s exception",
							   describeExceptionType(expectedExceptionType).c_str(),
							   describeExceptionType(exception->type).c_str());
				}
				else
				{
					TypeTuple exceptionParameterTypes
						= getExceptionTypeParameters(expectedExceptionType);

					for(Uptr argumentIndex = 0; argumentIndex < exceptionParameterTypes.size();
						++argumentIndex)
					{
						Value argumentValue(exceptionParameterTypes[argumentIndex],
											exception->arguments[argumentIndex]);
						if(argumentValue != assertCommand->expectedArguments[argumentIndex])
						{
							testErrorf(
								state,
								assertCommand->locus,
								"expected %s for exception argument %" WAVM_PRIuPTR " but got %s",
								asString(assertCommand->expectedArguments[argumentIndex]).c_str(),
								argumentIndex,
								asString(argumentValue).c_str());
						}
					}
				}
				destroyException(exception);
			});
		break;
	}
	case Command::assert_invalid: {
		auto assertCommand = (AssertInvalidOrMalformedCommand*)command;
		switch(assertCommand->wasInvalidOrMalformed)
		{
		case InvalidOrMalformed::wellFormedAndValid:
			testErrorf(state, assertCommand->locus, "module was well formed and valid");
			break;
		case InvalidOrMalformed::malformed:
			if(state.config.strictAssertInvalid)
			{ testErrorf(state, assertCommand->locus, "module was malformed"); }
			break;

		case InvalidOrMalformed::invalid:
		default: break;
		};
		break;
	}
	case Command::assert_malformed: {
		auto assertCommand = (AssertInvalidOrMalformedCommand*)command;
		switch(assertCommand->wasInvalidOrMalformed)
		{
		case InvalidOrMalformed::wellFormedAndValid:
			testErrorf(state, assertCommand->locus, "module was well formed and valid");
			break;

		case InvalidOrMalformed::invalid:
			if(state.config.strictAssertMalformed)
			{ testErrorf(state, assertCommand->locus, "module was invalid"); }
			break;

		case InvalidOrMalformed::malformed:
		default: break;
		};
		break;
	}
	case Command::assert_unlinkable: {
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
					if(startFunction) { invokeFunction(state.context, startFunction); }

					testErrorf(state, assertCommand->locus, "module was linkable");
				}
			},
			[&](Runtime::Exception* exception) {
				destroyException(exception);
				// If the instantiation throws an exception, the assert_unlinkable succeeds.
			});
		break;
	}

	default: WAVM_UNREACHABLE();
	};
}

static void processCommandWithCloning(TestScriptState& state, const Command* command)
{
	const Uptr originalNumErrors = state.errors.size();

	// Clone the test compartment and state.
	TestScriptState clonedState(state);

	// Process the command in both the original and the cloned compartments.
	processCommand(state, command);
	processCommand(clonedState, command);

	// Check that the command produced the same errors in both the original and cloned compartments.
	if(state.errors.size() != clonedState.errors.size())
	{
		testErrorf(state,
				   command->locus,
				   "Command produced different number of errors in cloned compartment");
	}
	else
	{
		for(Uptr errorIndex = originalNumErrors; errorIndex < state.errors.size(); ++errorIndex)
		{
			if(state.errors[errorIndex] != clonedState.errors[errorIndex])
			{
				testErrorf(state,
						   clonedState.errors[errorIndex].locus,
						   "Error only occurs in cloned state: %s",
						   clonedState.errors[errorIndex].message.c_str());
			}
		}
	}

	// Check that the original and cloned memory are the same after processing the command.
	if(state.lastModuleInstance && clonedState.lastModuleInstance)
	{
		WAVM_ASSERT(state.lastModuleInstance != clonedState.lastModuleInstance);

		Memory* memory = getDefaultMemory(state.lastModuleInstance);
		Memory* clonedMemory = getDefaultMemory(clonedState.lastModuleInstance);
		if(memory && clonedMemory)
		{
			WAVM_ASSERT(memory != clonedMemory);

			const Uptr numMemoryPages = getMemoryNumPages(memory);
			const Uptr numClonedMemoryPages = getMemoryNumPages(clonedMemory);

			if(numMemoryPages != numClonedMemoryPages)
			{
				testErrorf(state,
						   command->locus,
						   "Cloned memory size doesn't match (original = %" WAVM_PRIuPTR
						   " pages, cloned = %" WAVM_PRIuPTR " pages",
						   numMemoryPages,
						   numClonedMemoryPages);
			}
			else
			{
				const Uptr numMemoryBytes = numMemoryPages * IR::numBytesPerPage;
				U8* memoryBytes = memoryArrayPtr<U8>(memory, 0, numMemoryBytes);
				U8* clonedMemoryBytes = memoryArrayPtr<U8>(clonedMemory, 0, numMemoryBytes);
				if(memcmp(memoryBytes, clonedMemoryBytes, numMemoryBytes))
				{
					for(Uptr byteIndex = 0; byteIndex < numMemoryBytes; ++byteIndex)
					{
						const U8 value = memoryBytes[byteIndex];
						const U8 clonedValue = clonedMemoryBytes[byteIndex];
						if(value != clonedValue)
						{
							testErrorf(
								state,
								command->locus,
								"Memory differs from cloned memory at address 0x08%" WAVM_PRIxPTR
								": 0x%02x vs 0x%02x",
								byteIndex,
								value,
								clonedValue);
						}
					}
				}
			}
		}
	}
}

WAVM_DEFINE_INTRINSIC_FUNCTION(spectest, "print", void, spectest_print) {}
WAVM_DEFINE_INTRINSIC_FUNCTION(spectest, "print_i32", void, spectest_print_i32, I32 a)
{
	Log::printf(Log::debug, "%s : i32\n", asString(a).c_str());
}
WAVM_DEFINE_INTRINSIC_FUNCTION(spectest, "print_i64", void, spectest_print_i64, I64 a)
{
	Log::printf(Log::debug, "%s : i64\n", asString(a).c_str());
}
WAVM_DEFINE_INTRINSIC_FUNCTION(spectest, "print_f32", void, spectest_print_f32, F32 a)
{
	Log::printf(Log::debug, "%s : f32\n", asString(a).c_str());
}
WAVM_DEFINE_INTRINSIC_FUNCTION(spectest, "print_f64", void, spectest_print_f64, F64 a)
{
	Log::printf(Log::debug, "%s : f64\n", asString(a).c_str());
}
WAVM_DEFINE_INTRINSIC_FUNCTION(spectest,
							   "print_f64_f64",
							   void,
							   spectest_print_f64_f64,
							   F64 a,
							   F64 b)
{
	Log::printf(Log::debug, "%s : f64\n%s : f64\n", asString(a).c_str(), asString(b).c_str());
}
WAVM_DEFINE_INTRINSIC_FUNCTION(spectest,
							   "print_i32_f32",
							   void,
							   spectest_print_i32_f32,
							   I32 a,
							   F32 b)
{
	Log::printf(Log::debug, "%s : i32\n%s : f32\n", asString(a).c_str(), asString(b).c_str());
}
WAVM_DEFINE_INTRINSIC_FUNCTION(spectest,
							   "print_i64_f64",
							   void,
							   spectest_print_i64_f64,
							   I64 a,
							   F64 b)
{
	Log::printf(Log::debug, "%s : i64\n%s : f64\n", asString(a).c_str(), asString(b).c_str());
}

WAVM_DEFINE_INTRINSIC_GLOBAL(spectest, "global_i32", I32, spectest_global_i32, 666)
WAVM_DEFINE_INTRINSIC_GLOBAL(spectest, "global_i64", I64, spectest_global_i64, 0)
WAVM_DEFINE_INTRINSIC_GLOBAL(spectest, "global_f32", F32, spectest_global_f32, 0.0f)
WAVM_DEFINE_INTRINSIC_GLOBAL(spectest, "global_f64", F64, spectest_global_f64, 0.0)

WAVM_DEFINE_INTRINSIC_TABLE(spectest,
							spectest_table,
							table,
							TableType(ReferenceType::funcref, false, SizeConstraints{10, 20}))
WAVM_DEFINE_INTRINSIC_MEMORY(spectest,
							 spectest_memory,
							 memory,
							 MemoryType(false, SizeConstraints{1, 2}))
WAVM_DEFINE_INTRINSIC_MEMORY(spectest,
							 spectest_shared_memory,
							 shared_memory,
							 MemoryType(true, SizeConstraints{1, 2}))

struct SharedState
{
	Config config;

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
			Platform::Mutex::Lock sharedStateLock(sharedState->mutex);
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
		TestScriptState testScriptState(sharedState->config);
		std::vector<std::unique_ptr<Command>> testCommands;

		// Use a WebAssembly standard-compliant feature spec that includes all proposed extensions.
		FeatureSpec featureSpec(true);
		featureSpec.requireSharedFlagForAtomicOperators = true;
		featureSpec.customSectionsInTextFormat = true;

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
						if(testScriptState.config.testCloning)
						{ processCommandWithCloning(testScriptState, command.get()); }
						else
						{
							processCommand(testScriptState, command.get());
						}
					},
					[&testScriptState, &command](Runtime::Exception* exception) {
						testErrorf(testScriptState,
								   command->locus,
								   "unexpected trap: %s",
								   describeExceptionType(exception->type).c_str());
						destroyException(exception);
					});
			}
		}
		numErrors += testScriptState.errors.size();

		// Print any errors.
		reportParseErrors(filename, (const char*)testScriptBytes.data(), testScriptState.errors);
	}

	return numErrors;
}

static void showHelp()
{
	Log::printf(
		Log::error,
		"Usage: wavm test script [options] in.wast [options]\n"
		"  -h|--help                  Display this message\n"
		"  -l <N>|--loop <N>          Run tests N times in a loop until an error occurs\n"
		"  --strict-assert-invalid    Strictly evaluate assert_invalid, failing if the\n"
		"                             module was malformed\n"
		"  --strict-assert-malformed  Strictly evaluate assert_malformed, failing if the\n"
		"                             module was invalid\n"
		"  --test-cloning             Run each test command in the original compartment\n"
		"                             and a clone of it, and compare the resulting state\n"
		"  --trace                    Prints instructions to stdout as they are compiled.\n");
}

int execRunTestScript(int argc, char** argv)
{
	// Parse the command-line.
	Uptr numLoops = 1;
	std::vector<const char*> filenames;
	Config config;
	for(int argIndex = 0; argIndex < argc; ++argIndex)
	{
		if(!strcmp(argv[argIndex], "--help") || !strcmp(argv[argIndex], "-h"))
		{
			showHelp();
			return EXIT_SUCCESS;
		}
		else if(!strcmp(argv[argIndex], "--loop") || !strcmp(argv[argIndex], "-l"))
		{
			if(argIndex + 1 >= argc)
			{
				showHelp();
				return EXIT_FAILURE;
			}
			++argIndex;
			long int numLoopsLongInt = strtol(argv[argIndex], nullptr, 10);
			if(numLoopsLongInt <= 0)
			{
				showHelp();
				return EXIT_FAILURE;
			}
			numLoops = Uptr(numLoopsLongInt);
		}
		else if(!strcmp(argv[argIndex], "--strict-assert-invalid"))
		{
			config.strictAssertInvalid = true;
			++argIndex;
		}
		else if(!strcmp(argv[argIndex], "--strict-assert-malformed"))
		{
			config.strictAssertMalformed = true;
			++argIndex;
		}
		else if(!strcmp(argv[argIndex], "--test-cloning"))
		{
			config.testCloning = true;
		}
		else if(!strcmp(argv[argIndex], "--trace"))
		{
			Log::setCategoryEnabled(Log::traceValidation, true);
			Log::setCategoryEnabled(Log::traceCompilation, true);
		}
		else
		{
			filenames.push_back(argv[argIndex]);
		}
	}

	if(!filenames.size())
	{
		showHelp();
		return EXIT_FAILURE;
	}

	Uptr loopIndex = 0;
	while(true)
	{
		Timing::Timer timer;

		SharedState sharedState;
		sharedState.config = config;
		sharedState.pendingFilenames = filenames;

		// Try to use the default thread stack size to make sure WAVM passes tests with it, but on
		// MacOS the default thread stack size of 512KB is not enough for sanitized builds.
#if defined(__APPLE__) && (WAVM_ENABLE_ASAN || WAVM_ENABLE_UBSAN)
		constexpr Uptr threadStackNumBytes = 1 * 1024 * 1024;
#else
		constexpr Uptr threadStackNumBytes = 0;
#endif

		// Create a thread for each hardware thread.
		std::vector<Platform::Thread*> threads;
		const Uptr numHardwareThreads = Platform::getNumberOfHardwareThreads();
		const Uptr numTestThreads
			= std::min(numHardwareThreads, Uptr(sharedState.pendingFilenames.size()));
		for(Uptr threadIndex = 0; threadIndex < numTestThreads; ++threadIndex)
		{
			threads.push_back(
				Platform::createThread(threadStackNumBytes, threadMain, &sharedState));
		}

		// Wait for the threads to exit, summing up their return code, which will be the number of
		// errors found by the thread.
		I64 numErrors = 0;
		for(Platform::Thread* thread : threads) { numErrors += Platform::joinThread(thread); }

		if(numErrors)
		{
			Log::printf(Log::error, "Testing failed with %" PRIi64 " error(s)\n", numErrors);
			return EXIT_FAILURE;
		}
		else
		{
			Log::printf(Log::output, "All tests succeeded in %.1fms!\n", timer.getMilliseconds());
			if(++loopIndex >= numLoops) { return EXIT_SUCCESS; }
		}
	}
}

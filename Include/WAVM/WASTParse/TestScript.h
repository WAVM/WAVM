#pragma once

#include <memory>
#include <vector>
#include "WAVM/IR/FeatureSpec.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/WASTParse/WASTParse.h"

namespace WAVM { namespace WAST {
	struct Command
	{
		enum Type
		{
			_register,
			action,
			assert_return,
			assert_return_arithmetic_nan,
			assert_return_canonical_nan,
			assert_return_arithmetic_nan_f32x4,
			assert_return_canonical_nan_f32x4,
			assert_return_arithmetic_nan_f64x2,
			assert_return_canonical_nan_f64x2,
			assert_return_func,
			assert_trap,
			assert_throws,
			assert_invalid,
			assert_malformed,
			assert_unlinkable,
		};
		const Type type;
		const TextFileLocus locus;

		Command(Type inType, TextFileLocus&& inLocus) : type(inType), locus(inLocus) {}

		virtual ~Command() {}
	};

	// Parse a test script from a string. Returns true if it succeeds, and writes the test commands
	// to outTestCommands.
	WAVM_API void parseTestCommands(const char* string,
									Uptr stringLength,
									const IR::FeatureSpec& featureSpec,
									std::vector<std::unique_ptr<Command>>& outTestCommands,
									std::vector<Error>& outErrors);

	// Actions

	enum class ActionType
	{
		_module,
		invoke,
		get,
	};

	enum class ExpectedTrapType
	{
		outOfBounds,
		outOfBoundsMemoryAccess,
		outOfBoundsTableAccess,
		outOfBoundsDataSegmentAccess,
		outOfBoundsElemSegmentAccess,
		stackOverflow,
		integerDivideByZeroOrIntegerOverflow,
		invalidFloatOperation,
		invokeSignatureMismatch,
		reachedUnreachable,
		indirectCallSignatureMismatch,
		uninitializedTableElement,
		outOfMemory,
		misalignedAtomicMemoryAccess,
		invalidArgument
	};

	struct Action
	{
		const ActionType type;
		const TextFileLocus locus;

		Action(ActionType inType, TextFileLocus&& inLocus) : type(inType), locus(inLocus) {}

		virtual ~Action() {}
	};

	struct ModuleAction : Action
	{
		std::string internalModuleName;
		std::unique_ptr<IR::Module> module;
		ModuleAction(TextFileLocus&& inLocus,
					 std::string&& inInternalModuleName,
					 std::unique_ptr<IR::Module>&& inModule)
		: Action(ActionType::_module, std::move(inLocus))
		, internalModuleName(inInternalModuleName)
		, module(std::move(inModule))
		{
		}
	};

	struct InvokeAction : Action
	{
		std::string internalModuleName;
		std::string exportName;
		std::vector<IR::Value> arguments;
		InvokeAction(TextFileLocus&& inLocus,
					 std::string&& inInternalModuleName,
					 std::string&& inExportName,
					 std::vector<IR::Value>&& inArguments)
		: Action(ActionType::invoke, std::move(inLocus))
		, internalModuleName(inInternalModuleName)
		, exportName(inExportName)
		, arguments(inArguments)
		{
		}
	};

	struct GetAction : Action
	{
		std::string internalModuleName;
		std::string exportName;
		GetAction(TextFileLocus&& inLocus,
				  std::string&& inInternalModuleName,
				  std::string&& inExportName)
		: Action(ActionType::get, std::move(inLocus))
		, internalModuleName(inInternalModuleName)
		, exportName(inExportName)
		{
		}
	};

	// Commands

	struct RegisterCommand : Command
	{
		std::string moduleName;
		std::string internalModuleName;
		RegisterCommand(TextFileLocus&& inLocus,
						std::string&& inModuleName,
						std::string&& inInternalModuleName)
		: Command(Command::_register, std::move(inLocus))
		, moduleName(inModuleName)
		, internalModuleName(inInternalModuleName)
		{
		}
	};

	struct ActionCommand : Command
	{
		std::unique_ptr<Action> action;
		ActionCommand(TextFileLocus&& inLocus, std::unique_ptr<Action>&& inAction)
		: Command(Command::action, std::move(inLocus)), action(std::move(inAction))
		{
		}
	};

	struct AssertReturnCommand : Command
	{
		std::unique_ptr<Action> action;
		std::vector<IR::Value> expectedResults;
		AssertReturnCommand(TextFileLocus&& inLocus,
							std::unique_ptr<Action>&& inAction,
							std::vector<IR::Value> inExpectedResults)
		: Command(Command::assert_return, std::move(inLocus))
		, action(std::move(inAction))
		, expectedResults(inExpectedResults)
		{
		}
	};

	struct AssertReturnNaNCommand : Command
	{
		std::unique_ptr<Action> action;
		AssertReturnNaNCommand(Command::Type inType,
							   TextFileLocus&& inLocus,
							   std::unique_ptr<Action>&& inAction)
		: Command(inType, std::move(inLocus)), action(std::move(inAction))
		{
		}
	};

	struct AssertReturnFuncCommand : Command
	{
		std::unique_ptr<Action> action;
		AssertReturnFuncCommand(TextFileLocus&& inLocus, std::unique_ptr<Action>&& inAction)
		: Command(Command::assert_return_func, std::move(inLocus)), action(std::move(inAction))
		{
		}
	};

	struct AssertTrapCommand : Command
	{
		std::unique_ptr<Action> action;
		ExpectedTrapType expectedType;
		AssertTrapCommand(TextFileLocus&& inLocus,
						  std::unique_ptr<Action>&& inAction,
						  ExpectedTrapType inExpectedType)
		: Command(Command::assert_trap, std::move(inLocus))
		, action(std::move(inAction))
		, expectedType(inExpectedType)
		{
		}
	};

	struct AssertThrowsCommand : Command
	{
		std::unique_ptr<Action> action;
		std::string exceptionTypeInternalModuleName;
		std::string exceptionTypeExportName;
		std::vector<IR::Value> expectedArguments;
		AssertThrowsCommand(TextFileLocus&& inLocus,
							std::unique_ptr<Action>&& inAction,
							std::string&& inExceptionTypeInternalModuleName,
							std::string&& inExceptionTypeExportName,
							std::vector<IR::Value>&& inExpectedArguments)
		: Command(Command::assert_throws, std::move(inLocus))
		, action(std::move(inAction))
		, exceptionTypeInternalModuleName(inExceptionTypeInternalModuleName)
		, exceptionTypeExportName(inExceptionTypeExportName)
		, expectedArguments(inExpectedArguments)
		{
		}
	};

	enum class QuotedModuleType
	{
		none,
		text,
		binary
	};

	enum class InvalidOrMalformed
	{
		wellFormedAndValid,
		invalid,
		malformed
	};

	struct AssertInvalidOrMalformedCommand : Command
	{
		InvalidOrMalformed wasInvalidOrMalformed;
		QuotedModuleType quotedModuleType;
		std::string quotedModuleString;
		AssertInvalidOrMalformedCommand(Command::Type inType,
										TextFileLocus&& inLocus,
										InvalidOrMalformed inWasInvalidOrMalformed,
										QuotedModuleType inQuotedModuleType,
										std::string&& inQuotedModuleString)
		: Command(inType, std::move(inLocus))
		, wasInvalidOrMalformed(inWasInvalidOrMalformed)
		, quotedModuleType(inQuotedModuleType)
		, quotedModuleString(inQuotedModuleString)
		{
		}
	};

	struct AssertUnlinkableCommand : Command
	{
		std::unique_ptr<ModuleAction> moduleAction;
		AssertUnlinkableCommand(TextFileLocus&& inLocus,
								std::unique_ptr<ModuleAction> inModuleAction)
		: Command(Command::assert_unlinkable, std::move(inLocus))
		, moduleAction(std::move(inModuleAction))
		{
		}
	};
}}

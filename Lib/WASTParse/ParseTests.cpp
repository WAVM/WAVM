#include <string.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Lexer.h"
#include "Parse.h"
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Validate.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Platform/Diagnostics.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Runtime/RuntimeData.h"
#include "WAVM/WASM/WASM.h"
#include "WAVM/WASTParse/TestScript.h"
#include "WAVM/WASTParse/WASTParse.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::WAST;

static Runtime::Function* makeHostRef(Uptr index)
{
	static Platform::Mutex indexToHostRefMapMutex;
	static HashMap<Uptr, Runtime::Function*> indexToHostRefMap;
	Lock<Platform::Mutex> lock(indexToHostRefMapMutex);
	Runtime::Function*& function = indexToHostRefMap.getOrAdd(index, nullptr);
	if(!function)
	{
		Runtime::FunctionMutableData* functionMutableData
			= new Runtime::FunctionMutableData("test!ref.host!" + std::to_string(index));
		function
			= new Runtime::Function(functionMutableData, UINTPTR_MAX, FunctionType::Encoding{0});
		functionMutableData->function = function;

		Platform::expectLeakedObject(functionMutableData);
		Platform::expectLeakedObject(function);
	}
	return function;
}

static IR::Value parseConstExpression(CursorState* cursor)
{
	IR::Value result;
	parseParenthesized(cursor, [&] {
		switch(cursor->nextToken->type)
		{
		case t_i32_const:
		{
			++cursor->nextToken;
			result = parseI32(cursor);
			break;
		}
		case t_i64_const:
		{
			++cursor->nextToken;
			result = parseI64(cursor);
			break;
		}
		case t_f32_const:
		{
			++cursor->nextToken;
			result = parseF32(cursor);
			break;
		}
		case t_f64_const:
		{
			++cursor->nextToken;
			result = parseF64(cursor);
			break;
		}
		case t_v128_const:
		{
			++cursor->nextToken;
			result.type = ValueType::v128;
			result.v128 = parseV128(cursor);
			break;
		}
		case t_ref_host:
		{
			++cursor->nextToken;
			result.type = ValueType::funcref;
			result.function = makeHostRef(parseIptr(cursor));
			break;
		}
		case t_ref_null:
		{
			++cursor->nextToken;
			result.type = ValueType::nullref;
			result.object = nullptr;
			break;
		}
		default:
			parseErrorf(cursor->parseState, cursor->nextToken, "expected const expression");
			throw RecoverParseException();
		};
	});
	return result;
}

static IR::ValueTuple parseConstExpressionTuple(CursorState* cursor)
{
	IR::ValueTuple tuple;
	while(cursor->nextToken->type == t_leftParenthesis)
	{ tuple.values.push_back(parseConstExpression(cursor)); };
	return tuple;
}

static std::string parseOptionalNameAsString(CursorState* cursor)
{
	Name name;
	return tryParseName(cursor, name) ? name.getString() : std::string();
}

static void parseTestScriptModule(CursorState* cursor,
								  IR::Module& outModule,
								  std::string& outInternalModuleName,
								  QuotedModuleType& outQuotedModuleType,
								  std::string& outQuotedModuleString)
{
	outInternalModuleName = parseOptionalNameAsString(cursor);

	if(cursor->nextToken->type == t_quote || cursor->nextToken->type == t_binary)
	{
		// Parse a quoted module: (module quote|binary "...")
		const Token* quoteToken = cursor->nextToken;
		++cursor->nextToken;

		if(!tryParseString(cursor, outQuotedModuleString))
		{ parseErrorf(cursor->parseState, cursor->nextToken, "expected string"); }
		else
		{
			while(tryParseString(cursor, outQuotedModuleString)) {};
		}

		if(quoteToken->type == t_quote)
		{
			outQuotedModuleType = QuotedModuleType::text;

			std::vector<Error> quotedErrors;
			parseModule(outQuotedModuleString.c_str(),
						outQuotedModuleString.size() + 1,
						outModule,
						quotedErrors);
			for(auto&& error : quotedErrors)
			{
				cursor->parseState->unresolvedErrors.push_back(
					{quoteToken->begin, std::move(error.message)});
			}
		}
		else
		{
			outQuotedModuleType = QuotedModuleType::binary;

			try
			{
				Serialization::MemoryInputStream wasmInputStream(
					(const U8*)outQuotedModuleString.data(), outQuotedModuleString.size());
				WASM::serialize(wasmInputStream, outModule);
			}
			catch(Serialization::FatalSerializationException const& exception)
			{
				parseErrorf(cursor->parseState,
							quoteToken,
							"error deserializing binary module: %s",
							exception.message.c_str());
			}
			catch(ValidationException const& exception)
			{
				parseErrorf(cursor->parseState,
							quoteToken,
							"error validating binary module: %s",
							exception.message.c_str());
			}
		}
	}
	else
	{
		const U32 startCharOffset = cursor->nextToken->begin;
		parseModuleBody(cursor, outModule);
		const U32 endCharOffset = cursor->nextToken->begin;

		outQuotedModuleType = QuotedModuleType::text;
		outQuotedModuleString = std::string(cursor->parseState->string + startCharOffset,
											cursor->parseState->string + endCharOffset);
	}
}

static Action* parseAction(CursorState* cursor, const IR::FeatureSpec& featureSpec)
{
	Action* result = nullptr;
	parseParenthesized(cursor, [&] {
		TextFileLocus locus = calcLocusFromOffset(
			cursor->parseState->string, cursor->parseState->lineInfo, cursor->nextToken->begin);

		switch(cursor->nextToken->type)
		{
		case t_get:
		{
			++cursor->nextToken;

			std::string nameString = parseOptionalNameAsString(cursor);
			std::string exportName = parseUTF8String(cursor);

			result = new GetAction(std::move(locus), std::move(nameString), std::move(exportName));
			break;
		}
		case t_invoke:
		{
			++cursor->nextToken;

			std::string nameString = parseOptionalNameAsString(cursor);
			std::string exportName = parseUTF8String(cursor);

			IR::ValueTuple arguments = parseConstExpressionTuple(cursor);
			result = new InvokeAction(std::move(locus),
									  std::move(nameString),
									  std::move(exportName),
									  std::move(arguments));
			break;
		}
		case t_module:
		{
			++cursor->nextToken;

			std::string internalModuleName;
			Module* module = new Module(featureSpec);

			QuotedModuleType quotedModuleType = QuotedModuleType::none;
			std::string quotedModuleString;
			parseTestScriptModule(
				cursor, *module, internalModuleName, quotedModuleType, quotedModuleString);

			result = new ModuleAction(std::move(locus), std::move(internalModuleName), module);
			break;
		}
		default:
			parseErrorf(cursor->parseState, cursor->nextToken, "expected 'get' or 'invoke'");
			throw RecoverParseException();
		};
	});

	return result;
}

template<Uptr numPrefixChars>
static bool stringStartsWith(const char* string, const char (&prefix)[numPrefixChars])
{
	return !strncmp(string, prefix, numPrefixChars - 1);
}

static Command* parseCommand(CursorState* cursor, const IR::FeatureSpec& featureSpec)
{
	Command* result = nullptr;

	if(cursor->nextToken[0].type == t_leftParenthesis
	   && (cursor->nextToken[1].type == t_module || cursor->nextToken[1].type == t_invoke
		   || cursor->nextToken[1].type == t_get))
	{
		Action* action = parseAction(cursor, featureSpec);
		if(action)
		{
			TextFileLocus locus = action->locus;
			result = new ActionCommand(std::move(locus), action);
		}
	}
	else
	{
		parseParenthesized(cursor, [&] {
			TextFileLocus locus = calcLocusFromOffset(
				cursor->parseState->string, cursor->parseState->lineInfo, cursor->nextToken->begin);

			switch(cursor->nextToken->type)
			{
			case t_register:
			{
				++cursor->nextToken;

				std::string moduleName = parseUTF8String(cursor);
				std::string nameString = parseOptionalNameAsString(cursor);

				result = new RegisterCommand(
					std::move(locus), std::move(moduleName), std::move(nameString));
				break;
			}
			case t_assert_return:
			{
				++cursor->nextToken;

				Action* action = parseAction(cursor, featureSpec);
				IR::ValueTuple expectedResults = parseConstExpressionTuple(cursor);
				result = new AssertReturnCommand(std::move(locus), action, expectedResults);
				break;
			}
			case t_assert_return_arithmetic_nan:
			case t_assert_return_canonical_nan:
			{
				const Command::Type commandType
					= cursor->nextToken->type == t_assert_return_canonical_nan
						  ? Command::assert_return_canonical_nan
						  : Command::assert_return_arithmetic_nan;
				++cursor->nextToken;

				Action* action = parseAction(cursor, featureSpec);
				result = new AssertReturnNaNCommand(commandType, std::move(locus), action);
				break;
			}
			case t_assert_return_func:
			{
				++cursor->nextToken;

				Action* action = parseAction(cursor, featureSpec);
				result = new AssertReturnFuncCommand(std::move(locus), action);
				break;
			}
			case t_assert_exhaustion:
			case t_assert_trap:
			{
				++cursor->nextToken;

				Action* action = parseAction(cursor, featureSpec);

				const Token* errorToken = cursor->nextToken;
				std::string expectedErrorMessage;
				if(!tryParseString(cursor, expectedErrorMessage))
				{
					parseErrorf(cursor->parseState, cursor->nextToken, "expected string literal");
					throw RecoverParseException();
				}
				ExpectedTrapType expectedType;
				if(!strcmp(expectedErrorMessage.c_str(), "out of bounds memory access"))
				{ expectedType = ExpectedTrapType::outOfBoundsMemoryAccess; }
				else if(stringStartsWith(expectedErrorMessage.c_str(),
										 "out of bounds data segment access"))
				{
					expectedType = ExpectedTrapType::outOfBoundsDataSegmentAccess;
				}
				else if(stringStartsWith(expectedErrorMessage.c_str(),
										 "out of bounds elem segment access"))
				{
					expectedType = ExpectedTrapType::outOfBoundsElemSegmentAccess;
				}
				else if(stringStartsWith(expectedErrorMessage.c_str(), "out of bounds"))
				{
					expectedType = ExpectedTrapType::outOfBounds;
				}
				else if(!strcmp(expectedErrorMessage.c_str(), "call stack exhausted"))
				{
					expectedType = ExpectedTrapType::stackOverflow;
				}
				else if(!strcmp(expectedErrorMessage.c_str(), "integer overflow"))
				{
					expectedType = ExpectedTrapType::integerDivideByZeroOrIntegerOverflow;
				}
				else if(!strcmp(expectedErrorMessage.c_str(), "integer divide by zero"))
				{
					expectedType = ExpectedTrapType::integerDivideByZeroOrIntegerOverflow;
				}
				else if(!strcmp(expectedErrorMessage.c_str(), "invalid conversion to integer"))
				{
					expectedType = ExpectedTrapType::invalidFloatOperation;
				}
				else if(!strcmp(expectedErrorMessage.c_str(), "unaligned atomic"))
				{
					expectedType = ExpectedTrapType::misalignedAtomicMemoryAccess;
				}
				else if(stringStartsWith(expectedErrorMessage.c_str(), "unreachable"))
				{
					expectedType = ExpectedTrapType::reachedUnreachable;
				}
				else if(stringStartsWith(expectedErrorMessage.c_str(), "indirect call"))
				{
					expectedType = ExpectedTrapType::indirectCallSignatureMismatch;
				}
				else if(stringStartsWith(expectedErrorMessage.c_str(), "undefined"))
				{
					expectedType = ExpectedTrapType::outOfBoundsTableAccess;
				}
				else if(stringStartsWith(expectedErrorMessage.c_str(), "uninitialized"))
				{
					expectedType = ExpectedTrapType::uninitializedTableElement;
				}
				else if(stringStartsWith(expectedErrorMessage.c_str(), "invalid argument"))
				{
					expectedType = ExpectedTrapType::invalidArgument;
				}
				else if(!strcmp(expectedErrorMessage.c_str(), "element segment dropped"))
				{
					expectedType = ExpectedTrapType::invalidArgument;
				}
				else if(!strcmp(expectedErrorMessage.c_str(), "data segment dropped"))
				{
					expectedType = ExpectedTrapType::invalidArgument;
				}
				else
				{
					parseErrorf(cursor->parseState, errorToken, "unrecognized trap type");
					throw RecoverParseException();
				}

				result = new AssertTrapCommand(std::move(locus), action, expectedType);
				break;
			}
			case t_assert_throws:
			{
				++cursor->nextToken;

				Action* action = parseAction(cursor, featureSpec);

				std::string exceptionTypeInternalModuleName = parseOptionalNameAsString(cursor);
				std::string exceptionTypeExportName = parseUTF8String(cursor);

				IR::ValueTuple expectedArguments = parseConstExpressionTuple(cursor);
				result = new AssertThrowsCommand(std::move(locus),
												 action,
												 std::move(exceptionTypeInternalModuleName),
												 std::move(exceptionTypeExportName),
												 std::move(expectedArguments));
				break;
			}
			case t_assert_unlinkable:
			{
				++cursor->nextToken;

				if(cursor->nextToken[0].type != t_leftParenthesis
				   || cursor->nextToken[1].type != t_module)
				{
					parseErrorf(cursor->parseState, cursor->nextToken, "expected module");
					throw RecoverParseException();
				}

				ModuleAction* moduleAction = (ModuleAction*)parseAction(cursor, featureSpec);

				std::string expectedErrorMessage;
				if(!tryParseString(cursor, expectedErrorMessage))
				{
					parseErrorf(cursor->parseState, cursor->nextToken, "expected string literal");
					throw RecoverParseException();
				}

				result = new AssertUnlinkableCommand(std::move(locus), moduleAction);
				break;
			}
			case t_assert_invalid:
			case t_assert_malformed:
			{
				const Command::Type commandType = cursor->nextToken->type == t_assert_invalid
													  ? Command::assert_invalid
													  : Command::assert_malformed;
				++cursor->nextToken;

				std::string internalModuleName;
				Module module;
				module.featureSpec = featureSpec;
				ParseState* outerParseState = cursor->parseState;
				ParseState malformedModuleParseState(outerParseState->string,
													 outerParseState->lineInfo);

				QuotedModuleType quotedModuleType = QuotedModuleType::none;
				std::string quotedModuleString;
				try
				{
					cursor->parseState = &malformedModuleParseState;
					parseParenthesized(cursor, [&] {
						require(cursor, t_module);

						parseTestScriptModule(cursor,
											  module,
											  internalModuleName,
											  quotedModuleType,
											  quotedModuleString);
					});
				}
				catch(RecoverParseException const&)
				{
					cursor->parseState = outerParseState;
					throw RecoverParseException();
				}
				cursor->parseState = outerParseState;

				std::string expectedErrorMessage;
				if(!tryParseString(cursor, expectedErrorMessage))
				{
					parseErrorf(cursor->parseState, cursor->nextToken, "expected string literal");
					throw RecoverParseException();
				}

				result = new AssertInvalidOrMalformedCommand(
					commandType,
					std::move(locus),
					malformedModuleParseState.unresolvedErrors.size() != 0,
					quotedModuleType,
					std::move(quotedModuleString));
				break;
			};
			default:
				parseErrorf(cursor->parseState, cursor->nextToken, "unknown script command");
				throw RecoverParseException();
			};
		});
	}

	return result;
}

void WAST::parseTestCommands(const char* string,
							 Uptr stringLength,
							 const FeatureSpec& featureSpec,
							 std::vector<std::unique_ptr<Command>>& outTestCommands,
							 std::vector<Error>& outErrors)
{
	// Lex the input string.
	LineInfo* lineInfo = nullptr;
	Token* tokens = lex(string, stringLength, lineInfo, featureSpec.allowLegacyOperatorNames);
	ParseState parseState(string, lineInfo);
	CursorState cursor(tokens, &parseState);

	try
	{
		// Support test scripts that are just an inline module.
		if(cursor.nextToken[0].type == t_leftParenthesis
		   && (cursor.nextToken[1].type == t_import || cursor.nextToken[1].type == t_export
			   || cursor.nextToken[1].type == t_exception_type
			   || cursor.nextToken[1].type == t_global || cursor.nextToken[1].type == t_memory
			   || cursor.nextToken[1].type == t_table || cursor.nextToken[1].type == t_type
			   || cursor.nextToken[1].type == t_data || cursor.nextToken[1].type == t_elem
			   || cursor.nextToken[1].type == t_func || cursor.nextToken[1].type == t_start))
		{
			const TextFileLocus locus
				= calcLocusFromOffset(string, lineInfo, cursor.nextToken[0].begin);
			Module* module = new Module(featureSpec);
			parseModuleBody(&cursor, *module);
			auto moduleAction = new ModuleAction(TextFileLocus(locus), "", module);
			auto actionCommand = new ActionCommand(TextFileLocus(locus), moduleAction);
			outTestCommands.emplace_back(actionCommand);
		}
		else
		{
			// (command)*<eof>
			while(cursor.nextToken->type == t_leftParenthesis)
			{ outTestCommands.emplace_back(parseCommand(&cursor, featureSpec)); };
		}

		require(&cursor, t_eof);
	}
	catch(RecoverParseException const&)
	{
	}
	catch(FatalParseException const&)
	{
	}

	// Resolve line information for any errors, and write them to outErrors.
	for(auto& unresolvedError : parseState.unresolvedErrors)
	{
		TextFileLocus locus = calcLocusFromOffset(string, lineInfo, unresolvedError.charOffset);
		outErrors.push_back({std::move(locus), std::move(unresolvedError.message)});
	}

	// Free the tokens and line info.
	freeTokens(tokens);
	freeLineInfo(lineInfo);
}

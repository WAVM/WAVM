#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "WAST.h"
#include "TestScript.h"
#include "Lexer.h"
#include "IR/Module.h"
#include "IR/Validate.h"
#include "IR/TaggedValue.h"
#include "Parse.h"
#include "Inline/Serialization.h"
#include "WASM/WASM.h"

using namespace WAST;
using namespace IR;

static Runtime::Value parseConstExpression(CursorState* cursor)
{
	Runtime::Value result;
	parseParenthesized(cursor,[&]
	{
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
		default:
			parseErrorf(cursor->parseState,cursor->nextToken,"expected const expression");
			throw RecoverParseException();
		};
	});
	return result;
}

static std::string parseOptionalNameAsString(CursorState* cursor)
{
	Name name;
	return tryParseName(cursor,name) ? name.getString() : std::string();
}

static void parseTestScriptModule(CursorState* cursor,IR::Module& outModule,std::string& outInternalModuleName)
{
	outInternalModuleName = parseOptionalNameAsString(cursor);
	
	outModule.featureSpec.importExportMutableGlobals = false;

	if(cursor->nextToken->type == t_quote || cursor->nextToken->type == t_binary)
	{
		// Parse a quoted module: (module quote|binary "...")
		const Token* quoteToken = cursor->nextToken;
		++cursor->nextToken;

		std::string moduleQuotedString;
		if(!tryParseString(cursor,moduleQuotedString))
		{
			parseErrorf(cursor->parseState,cursor->nextToken,"expected string");
		}
		else
		{
			while(tryParseString(cursor,moduleQuotedString)) {};
		}

		if(quoteToken->type == t_quote)
		{
			moduleQuotedString = "(module " + moduleQuotedString + ")";

			std::vector<Error> quotedErrors;
			parseModule(moduleQuotedString.c_str(),moduleQuotedString.size(),outModule,quotedErrors);
			for(auto&& error : quotedErrors)
			{
				cursor->parseState->unresolvedErrors.emplace_back(quoteToken->begin,std::move(error.message));
			}
		}
		else
		{
			try
			{
				Serialization::MemoryInputStream wasmInputStream((const U8*)moduleQuotedString.data(),moduleQuotedString.size());
				WASM::serialize(wasmInputStream,outModule);
			}
			catch(Serialization::FatalSerializationException exception)
			{
				parseErrorf(cursor->parseState,quoteToken,"error deserializing binary module: %s",exception.message.c_str());
			}
			catch(ValidationException exception)
			{
				parseErrorf(cursor->parseState,quoteToken,"error validating binary module: %s",exception.message.c_str());
			}
		}
	}
	else
	{
		parseModuleBody(cursor,outModule);
	}
}

static Action* parseAction(CursorState* cursor)
{
	Action* result = nullptr;
	parseParenthesized(cursor,[&]
	{
		TextFileLocus locus = calcLocusFromOffset(cursor->parseState->string,cursor->parseState->lineInfo,cursor->nextToken->begin);

		switch(cursor->nextToken->type)
		{
		case t_get:
		{
			++cursor->nextToken;

			std::string nameString = parseOptionalNameAsString(cursor);
			std::string exportName = parseUTF8String(cursor);
			
			result = new GetAction(std::move(locus),std::move(nameString),std::move(exportName));
			break;
		}
		case t_invoke:
		{
			++cursor->nextToken;

			std::string nameString = parseOptionalNameAsString(cursor);
			std::string exportName = parseUTF8String(cursor);

			std::vector<Runtime::Value> arguments;
			while(cursor->nextToken->type == t_leftParenthesis)
			{
				arguments.push_back(parseConstExpression(cursor));
			};

			result = new InvokeAction(std::move(locus),std::move(nameString),std::move(exportName),std::move(arguments));
			break;
		}
		case t_module:
		{
			++cursor->nextToken;

			std::string internalModuleName;
			Module* module = new Module;
			parseTestScriptModule(cursor,*module,internalModuleName);

			result = new ModuleAction(std::move(locus),std::move(internalModuleName),module);
			break;
		}
		default:
			parseErrorf(cursor->parseState,cursor->nextToken,"expected 'get' or 'invoke'");
			throw RecoverParseException();
		};
	});

	return result;
}

template<Uptr numPrefixChars>
static bool stringStartsWith(const char* string,const char (&prefix)[numPrefixChars])
{
	return !strncmp(string,prefix,numPrefixChars - 1);
}

static Command* parseCommand(CursorState* cursor)
{
	Command* result = nullptr;

	if(cursor->nextToken[0].type == t_leftParenthesis
	&& (cursor->nextToken[1].type == t_module
		|| cursor->nextToken[1].type == t_invoke
		|| cursor->nextToken[1].type == t_get))
	{
		Action* action = parseAction(cursor);
		if(action)
		{
			TextFileLocus locus = action->locus;
			result = new ActionCommand(std::move(locus),action);
		}
	}
	else
	{
		parseParenthesized(cursor,[&]
		{
			TextFileLocus locus = calcLocusFromOffset(cursor->parseState->string,cursor->parseState->lineInfo,cursor->nextToken->begin);

			switch(cursor->nextToken->type)
			{
			case t_register:
			{
				++cursor->nextToken;

				std::string moduleName = parseUTF8String(cursor);
				std::string nameString = parseOptionalNameAsString(cursor);

				result = new RegisterCommand(std::move(locus),std::move(moduleName),std::move(nameString));
				break;
			}
			case t_assert_return:
			{
				++cursor->nextToken;

				Action* action = parseAction(cursor);
				Runtime::Result expectedReturn = cursor->nextToken->type == t_leftParenthesis ? parseConstExpression(cursor) : Runtime::Result();
				result = new AssertReturnCommand(std::move(locus),action,expectedReturn);
				break;
			}
			case t_assert_return_canonical_nan:
			case t_assert_return_arithmetic_nan:
			{
				const Command::Type commandType = cursor->nextToken->type == t_assert_return_canonical_nan
					? Command::assert_return_canonical_nan
					: Command::assert_return_arithmetic_nan;
				++cursor->nextToken;

				Action* action = parseAction(cursor);
				result = new AssertReturnNaNCommand(commandType,std::move(locus),action);
				break;
			}
			case t_assert_exhaustion:
			case t_assert_trap:
			{
				++cursor->nextToken;

				Action* action = parseAction(cursor);

				const Token* errorToken = cursor->nextToken;
				std::string expectedErrorMessage;
				if(!tryParseString(cursor,expectedErrorMessage))
				{
					parseErrorf(cursor->parseState,cursor->nextToken,"expected string literal");
					throw RecoverParseException();
				}
				ExpectedTrapType expectedType;
				if(!strcmp(expectedErrorMessage.c_str(),"out of bounds memory access")) { expectedType = ExpectedTrapType::accessViolation; }
				else if(!strcmp(expectedErrorMessage.c_str(),"call stack exhausted")) { expectedType = ExpectedTrapType::stackOverflow; }
				else if(!strcmp(expectedErrorMessage.c_str(),"integer overflow")) { expectedType = ExpectedTrapType::integerDivideByZeroOrIntegerOverflow; }
				else if(!strcmp(expectedErrorMessage.c_str(),"integer divide by zero")) { expectedType = ExpectedTrapType::integerDivideByZeroOrIntegerOverflow; }
				else if(!strcmp(expectedErrorMessage.c_str(),"invalid conversion to integer")) { expectedType = ExpectedTrapType::invalidFloatOperation; }
				else if(!strcmp(expectedErrorMessage.c_str(),"unaligned atomic")) { expectedType = ExpectedTrapType::misalignedAtomicMemoryAccess; }
				else if(stringStartsWith(expectedErrorMessage.c_str(),"unreachable")) { expectedType = ExpectedTrapType::reachedUnreachable; }
				else if(stringStartsWith(expectedErrorMessage.c_str(),"indirect call")) { expectedType = ExpectedTrapType::indirectCallSignatureMismatch; }
				else if(stringStartsWith(expectedErrorMessage.c_str(),"undefined")) { expectedType = ExpectedTrapType::undefinedTableElement; }
				else if(stringStartsWith(expectedErrorMessage.c_str(),"uninitialized")) { expectedType = ExpectedTrapType::undefinedTableElement; }
				else { parseErrorf(cursor->parseState,errorToken,"unrecognized trap type"); throw RecoverParseException(); }

				result = new AssertTrapCommand(std::move(locus),action,expectedType);
				break;
			}
			case t_assert_throws:
			{
				++cursor->nextToken;

				Action* action = parseAction(cursor);

				std::string exceptionTypeInternalModuleName = parseOptionalNameAsString(cursor);
				std::string exceptionTypeExportName = parseUTF8String(cursor);

				std::vector<Runtime::Value> expectedArguments;
				while(cursor->nextToken->type == t_leftParenthesis)
				{
					expectedArguments.push_back(parseConstExpression(cursor));
				};

				result = new AssertThrowsCommand(
					std::move(locus),
					action,
					std::move(exceptionTypeInternalModuleName),
					std::move(exceptionTypeExportName),
					std::move(expectedArguments));
				break;
			}
			case t_assert_unlinkable:
			{
				++cursor->nextToken;

				if(cursor->nextToken[0].type != t_leftParenthesis || cursor->nextToken[1].type != t_module)
				{
					parseErrorf(cursor->parseState,cursor->nextToken,"expected module");
					throw RecoverParseException();
				}

				ModuleAction* moduleAction = (ModuleAction*)parseAction(cursor);
						
				std::string expectedErrorMessage;
				if(!tryParseString(cursor,expectedErrorMessage))
				{
					parseErrorf(cursor->parseState,cursor->nextToken,"expected string literal");
					throw RecoverParseException();
				}

				result = new AssertUnlinkableCommand(std::move(locus),moduleAction);
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
				ParseState* outerParseState = cursor->parseState;
				ParseState malformedModuleParseState(outerParseState->string,outerParseState->lineInfo);

				try
				{
					cursor->parseState = &malformedModuleParseState;
					parseParenthesized(cursor,[&]
					{
						require(cursor,t_module);

						parseTestScriptModule(cursor,module,internalModuleName);
					});
				}
				catch(RecoverParseException)
				{
					cursor->parseState = outerParseState;
					throw RecoverParseException();
				}
				cursor->parseState = outerParseState;

				std::string expectedErrorMessage;
				if(!tryParseString(cursor,expectedErrorMessage))
				{
					parseErrorf(cursor->parseState,cursor->nextToken,"expected string literal");
					throw RecoverParseException();
				}

				result = new AssertInvalidOrMalformedCommand(
					commandType,
					std::move(locus),
					malformedModuleParseState.unresolvedErrors.size() != 0);
				break;
			};
			default:
				parseErrorf(cursor->parseState,cursor->nextToken,"unknown script command");
				throw RecoverParseException();
			};
		});
	}
	
	return result;
}

namespace WAST
{
	void parseTestCommands(const char* string,Uptr stringLength,std::vector<std::unique_ptr<Command>>& outTestCommands,std::vector<Error>& outErrors)
	{
		// Lex the input string.
		LineInfo* lineInfo = nullptr;
		Token* tokens = lex(string,stringLength,lineInfo);
		ParseState parseState(string,lineInfo);
		CursorState cursor(tokens,&parseState);

		try
		{
			// (command)*<eof>
			while(cursor.nextToken->type == t_leftParenthesis)
			{
				outTestCommands.emplace_back(parseCommand(&cursor));
			};
			require(&cursor,t_eof);
		}
		catch(RecoverParseException) {}
		catch(FatalParseException) {}
		
		// Resolve line information for any errors, and write them to outErrors.
		for(auto& unresolvedError : parseState.unresolvedErrors)
		{
			TextFileLocus locus = calcLocusFromOffset(string,lineInfo,unresolvedError.charOffset);
			outErrors.push_back({std::move(locus),std::move(unresolvedError.message)});
		}

		// Free the tokens and line info.
		freeTokens(tokens);
		freeLineInfo(lineInfo);
	}
}
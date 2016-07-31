#include "Core/Core.h"
#include "Core/MemoryArena.h"
#include "Core/Platform.h"
#include "AST/AST.h"
#include "AST/ASTExpressions.h"
#include "Runtime/Runtime.h"

#include "CLI.h"

using namespace WebAssemblyText;

int main(int argc,char** argv)
{
	if(argc != 2)
	{
		std::cerr <<  "Usage: Test in.wast" << std::endl;
		return EXIT_FAILURE;
	}
	
	const char* filename = argv[1];
	File wastFile;
	if(!loadTextFile(filename,wastFile)) { return EXIT_FAILURE; }
	
	if(!Runtime::init()) { return EXIT_FAILURE; }
	
	uintptr numTestsFailed = 0;
	for(uintptr moduleIndex = 0;moduleIndex < wastFile.modules.size();++moduleIndex)
	{
		auto module = wastFile.modules[moduleIndex];
		auto& testStatements = wastFile.moduleTests[moduleIndex];
		if(!testStatements.size()) { continue; }

		// Initialize the module runtime environment.
		if(!Runtime::loadModule(module)) { return EXIT_FAILURE; }
		
		// Evaluate each test statement.
		for(uintptr statementIndex = 0;statementIndex < testStatements.size();++statementIndex)
		{
			auto statement = testStatements[statementIndex];
			auto statementLocus = std::string(filename) + ":" + statement->locus.describe();
			switch(statement->op)
			{
			case TestOp::Invoke:
			{
				auto invoke = (Invoke*)statement;
				auto result = Runtime::invokeFunction(module,invoke->functionIndex,invoke->parameters.data());
				if(result.type == Runtime::TypeId::Exception)
				{
					std::cerr << statementLocus << ": invoke unexpectedly trapped: " << Runtime::describeExceptionCause(result.exception->cause) << std::endl;
					for(auto function : result.exception->callStack) { std::cerr << "  " << function << std::endl; }
					++numTestsFailed;
				}
				break;
			}
			case TestOp::Assert:
			{
				auto assertStatement = (Assert*)statement;
				auto invoke = assertStatement->invoke;
				auto result = Runtime::invokeFunction(module,invoke->functionIndex,invoke->parameters.data());
				if(describeRuntimeValue(result) != describeRuntimeValue(assertStatement->value))
				{
					std::cerr << statementLocus << ": assertion failure: expected "
						<< describeRuntimeValue(assertStatement->value)
						<< " but got " << describeRuntimeValue(result) << std::endl;
					++numTestsFailed;
				}
				break;
			}
			case TestOp::AssertNaN:
			{
				auto assertStatement = (AssertNaN*)statement;
				auto invoke = assertStatement->invoke;
				auto result = Runtime::invokeFunction(module,invoke->functionIndex,invoke->parameters.data());
				if(result.type != Runtime::TypeId::F32 && result.type != Runtime::TypeId::F64)
				{
					std::cerr << statementLocus << ": assertion failure: expected floating-point number but got " << describeRuntimeValue(result) << std::endl;
					++numTestsFailed;
				}
				else if(	(result.type == Runtime::TypeId::F32 && (result.f32 == result.f32))
				||		(result.type == Runtime::TypeId::F64 && (result.f64 == result.f64)))
				{
					std::cerr << statementLocus << ": assertion failure: expected NaN but got " << describeRuntimeValue(result) << std::endl;
					++numTestsFailed;
				}
				break;
			}
			default: throw;
			}
		}
	}

	// Print the results.
	if(numTestsFailed)
	{
		std::cerr << numTestsFailed << " tests failed!" << std::endl;
		return EXIT_FAILURE;
	}
	else
	{
		std::cout << filename << ": all tests passed." << std::endl;
		return EXIT_SUCCESS;
	}	
}

#include "Core/Core.h"
#include "Core/MemoryArena.h"
#include "Core/Floats.h"
#include "Core/Platform.h"
#include "AST/AST.h"
#include "AST/ASTExpressions.h"
#include "Runtime/Runtime.h"

#include "CLI.h"

using namespace WebAssemblyText;

std::string describeRuntimeValue(const Runtime::Value& value)
{
	switch(value.type)
	{
	case Runtime::TypeId::None: return "None";
	case Runtime::TypeId::I8: return "I8(" + std::to_string(value.i8) + ")";
	case Runtime::TypeId::I16: return "I16(" + std::to_string(value.i16) + ")";
	case Runtime::TypeId::I32: return "I32(" + std::to_string(value.i32) + ")";
	case Runtime::TypeId::I64: return "I64(" + std::to_string(value.i64) + ")";
	case Runtime::TypeId::F32: return "F32(" + Floats::asString(value.f32) + ")";
	case Runtime::TypeId::F64: return "F64(" + Floats::asString(value.f64) + ")";
	case Runtime::TypeId::Bool: return value.bool_ ? "Bool(true)" : "Bool(false)";
	case Runtime::TypeId::Void: return "Void";
	case Runtime::TypeId::Exception: return "Exception(" + std::string(Runtime::describeExceptionCause(value.exception->cause)) + ")";
	default: throw;
	}
}

int main(int argc,char** argv)
{
	if(argc != 2)
	{
		std::cerr <<  "Usage: Test in.wast" << std::endl;
		return -1;
	}
	
	const char* filename = argv[1];
	File wastFile;
	if(!loadTextModule(filename,wastFile)) { return -1; }
	
	// Initialize the runtime.
	if(!Runtime::init())
	{
		std::cerr << "Couldn't initialize runtime" << std::endl;
		return false;
	}
	
	uintptr numTestsFailed = 0;
	for(uintptr moduleIndex = 0;moduleIndex < wastFile.modules.size();++moduleIndex)
	{
		auto module = wastFile.modules[moduleIndex];
		auto& testStatements = wastFile.moduleTests[moduleIndex];
		if(!testStatements.size()) { continue; }

		// Initialize the module runtime environment.
		if(!Runtime::loadModule(module)) { return -1; }
		
		// Evaluate each test statement.
		for(uintptr statementIndex = 0;statementIndex < testStatements.size();++statementIndex)
		{
			auto statement = testStatements[statementIndex];
			auto statementLocus = filename + statement->locus.describe();
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
				else if(	result.type == Runtime::TypeId::F32 && (result.f32 == result.f32)
				||		result.type == Runtime::TypeId::F64 && (result.f64 == result.f64))
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
		return -1;
	}
	else
	{
		std::cout << filename << ": all tests passed." << std::endl;
		return 0;
	}	
}

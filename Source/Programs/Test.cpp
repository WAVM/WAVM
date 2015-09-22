#include "Core/Core.h"
#include "Core/MemoryArena.h"
#include "Core/Platform.h"
#include "AST/AST.h"
#include "AST/ASTExpressions.h"
#include "Runtime/Runtime.h"

#include "CLI.h"
#include "RuntimeCLI.h"

uintptr createTestFunction(AST::Module* module,const char* name,AST::TypedExpression expression)
{
	auto function = new AST::Function();
	function->name = name;
	function->type.returnType = expression.type;
	function->expression = expression.expression;

	auto functionIndex = module->functions.size();
	module->functions.push_back(function);
	module->exportNameToFunctionIndexMap[name] = functionIndex;
	return functionIndex;
}

template<typename Type>
bool callTestFunction(AST::Module* module,const char* name,const char* locus,AST::TypedExpression typedExpectedValue)
{
	typename Type::NativeType returnValue;
	if(!callModuleFunction(module,name,returnValue)) { return false; }

	auto expectedValue = AST::as<typename Type::Class>(typedExpectedValue);
	if(expectedValue->op() != Type::Op::lit)
	{
		std::cerr << locus << ": assert_eq expected value expression must be const" << std::endl;
		return false;
	}
	auto expectedValueLiteral = (const AST::Literal<Type>*)expectedValue;

	if(returnValue != expectedValueLiteral->value)
	{
		std::cerr << locus <<  ": assert_eq expected " << expectedValueLiteral->value << " but function returned " << returnValue << std::endl;
		return false;
	}

	return true;
}

bool callTestFunction(AST::Module* module,const char* name,const char* locus,AST::TypedExpression expectedValue)
{
	switch(expectedValue.type)
	{
	case AST::TypeId::I8: return callTestFunction<AST::I8Type>(module,name,locus,expectedValue);
	case AST::TypeId::I16: return callTestFunction<AST::I16Type>(module,name,locus,expectedValue);
	case AST::TypeId::I32: return callTestFunction<AST::I32Type>(module,name,locus,expectedValue);
	case AST::TypeId::I64: return callTestFunction<AST::I64Type>(module,name,locus,expectedValue);
	case AST::TypeId::F32: return callTestFunction<AST::F32Type>(module,name,locus,expectedValue);
	case AST::TypeId::F64: return callTestFunction<AST::F64Type>(module,name,locus,expectedValue);
	case AST::TypeId::Bool: return callTestFunction<AST::BoolType>(module,name,locus,expectedValue);
	case AST::TypeId::Void: std::cerr << locus << ": Why are you trying to assert the equality of two void values?" << std::endl; return true;
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
	WebAssemblyText::File wastFile;
	if(!loadTextModule(filename,wastFile)) { return -1; }
	
	uintptr numTestsFailed = 0;
	for(auto assertEq : wastFile.assertEqs)
	{
		// Make a copy of the module that the assertion invokes from.
		auto testModule = new AST::Module(*assertEq.invokeFunctionModule);

		// Add an exported function to that module that just calls the invoke function with the provided parameters and returns the result.
		auto invokedFunction = testModule->functions[assertEq.invokeFunctionIndex];
		auto invokeType = invokedFunction->type.returnType;
		auto invokeParameters = new(testModule->arena) AST::UntypedExpression*[invokedFunction->type.parameters.size()];
		for(uintptr parameterIndex = 0;parameterIndex < invokedFunction->type.parameters.size();++parameterIndex)
		{
			assert(assertEq.parameters[parameterIndex].type == invokedFunction->type.parameters[parameterIndex]);
			invokeParameters[parameterIndex] = assertEq.parameters[parameterIndex].expression;
		}
		auto invokeExpression = new(testModule->arena) AST::Call(AST::AnyOp::callDirect,getPrimaryTypeClass(invokeType),(uint32)assertEq.invokeFunctionIndex,invokeParameters);
		createTestFunction(testModule,"test",AST::TypedExpression(invokeExpression,invokeType));
		
		// Initialize the module runtime environment and call the test function.
		if(!initModuleRuntime(testModule)) { return -1; }
		auto assertLocus = filename + assertEq.locus.describe();
		if(!callTestFunction(testModule,"test",assertLocus.c_str(),assertEq.value)) { ++numTestsFailed; }
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
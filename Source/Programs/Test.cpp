#include "Core/Core.h"
#include "Core/MemoryArena.h"
#include "CLI.h"
#include "Core/Platform.h"
#include "AST/AST.h"
#include "AST/ASTExpressions.h"
#include "Runtime/Runtime.h"

struct Void {};

template<typename NativeValue> struct NativeToASTType;
template<> struct NativeToASTType<uint8> { typedef AST::I8Type ASTType; };
template<> struct NativeToASTType<int8> { typedef AST::I8Type ASTType; };
template<> struct NativeToASTType<uint16> { typedef AST::I16Type ASTType; };
template<> struct NativeToASTType<int16> { typedef AST::I16Type ASTType; };
template<> struct NativeToASTType<uint32> { typedef AST::I32Type ASTType; };
template<> struct NativeToASTType<int32> { typedef AST::I32Type ASTType; };
template<> struct NativeToASTType<uint64> { typedef AST::I64Type ASTType; };
template<> struct NativeToASTType<int64> { typedef AST::I64Type ASTType; };
template<> struct NativeToASTType<float32> { typedef AST::F32Type ASTType; };
template<> struct NativeToASTType<float64> { typedef AST::F64Type ASTType; };
template<> struct NativeToASTType<bool> { typedef AST::BoolType ASTType; };
template<> struct NativeToASTType<Void> { typedef AST::VoidType ASTType; };

template<typename... Args>
bool validateArgTypes(const AST::FunctionType& functionType,uintptr_t argIndex,Args...)
{
	return argIndex == functionType.parameters.size();
}

template<typename Arg0,typename... Args>
bool validateArgTypes(const AST::FunctionType& functionType,uintptr_t argIndex,Arg0,Args...)
{
	return validateArgTypes<Args...>(functionType,argIndex + 1)
		&& functionType.parameters[argIndex] == NativeToASTType<Arg0>::ASTType::id;
}

template<typename Return,typename... Args>
bool callModuleFunction(const AST::Module* module,const char* functionName,Return& outReturn,Args... args)
{
	// Look up the function specified on the command line in the module.
	auto exportIt = module->exportNameToFunctionIndexMap.find(functionName);
	if(exportIt == module->exportNameToFunctionIndexMap.end())
	{
		std::cerr << "module doesn't contain named export " << functionName << std::endl;
		return false;
	}

	auto function = module->functions[exportIt->second];
	if(!validateArgTypes(function->type,0,args...) || function->type.returnType != NativeToASTType<Return>::ASTType::id)
	{
		std::cerr << "exported function " << functionName << " isn't expected type" << std::endl;
		return false;
	}

	void* functionPtr = Runtime::getFunctionPointer(module,exportIt->second);
	assert(functionPtr);

	// Call the generated machine code for the function.
	try
	{
		// Call the function specified on the command-line.
		outReturn = ((Return(*)(Args...))functionPtr)(args...);
		return true;
	}
	catch(...)
	{
		std::cout << functionName << " threw exception." << std::endl;
		return false;
	}
}

bool initModuleRuntime(const AST::Module* module)
{
	// Allocate address-space for the VM.
	if(!Runtime::initInstanceMemory(module->maxNumBytesMemory))
	{
		std::cerr << "Couldn't initialize address-space for module instance (" << module->maxNumBytesMemory/1024 << "KB requested)" << std::endl;
		return false;
	}

	// Initialize the module's requested initial memory.
	if(Runtime::vmSbrk((int32)module->initialNumBytesMemory) != 0)
	{
		std::cerr << "Failed to commit the requested initial memory for module instance (" << module->initialNumBytesMemory/1024 << "KB requested)" << std::endl;
		return false;
	}

	// Copy the module's data segments into VM memory.
	if(module->initialNumBytesMemory >= (1ull<<32)) { throw; }
	for(auto dataSegment : module->dataSegments)
	{
		if(dataSegment.baseAddress + dataSegment.numBytes > module->initialNumBytesMemory)
		{
			std::cerr << "Module data segment exceeds initial memory allocation" << std::endl;
			return false;
		}
		memcpy(Runtime::instanceMemoryBase + dataSegment.baseAddress,dataSegment.data,dataSegment.numBytes);
	}

	// Generate machine code for the module.
	if(!Runtime::compileModule(module))
	{
		std::cerr << "Couldn't compile module." << std::endl;
		return false;
	}

	return true;
}

uintptr_t createTestFunction(AST::Module* module,const char* name,AST::TypedExpression expression)
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
	
	uintptr_t numTestsFailed = 0;
	for(auto assertEq : wastFile.assertEqs)
	{
		// Make a copy of the module that the assertion invokes from.
		auto testModule = new AST::Module(*assertEq.invokeFunctionModule);

		// Add an exported function to that module that just calls the invoke function with the provided parameters and returns the result.
		auto invokedFunction = testModule->functions[assertEq.invokeFunctionIndex];
		auto invokeType = invokedFunction->type.returnType;
		auto invokeParameters = new(testModule->arena) AST::UntypedExpression*[invokedFunction->type.parameters.size()];
		for(uintptr_t parameterIndex = 0;parameterIndex < invokedFunction->type.parameters.size();++parameterIndex)
		{
			assert(assertEq.parameters[parameterIndex].type == invokedFunction->type.parameters[parameterIndex]);
			invokeParameters[parameterIndex] = assertEq.parameters[parameterIndex].expression;
		}
		auto invokeExpression = new(testModule->arena) AST::Call(AST::AnyOp::callDirect,getPrimaryTypeClass(invokeType),assertEq.invokeFunctionIndex,invokeParameters);
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
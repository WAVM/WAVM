#pragma once

#include "Core/Core.h"
#include "AST/AST.h"
#include "Runtime/Runtime.h"

#include <iostream>

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
bool validateArgTypes(const AST::FunctionType& functionType,uintptr argIndex,Args...)
{
	return argIndex == functionType.parameters.size();
}

template<typename Arg0,typename... Args>
bool validateArgTypes(const AST::FunctionType& functionType,uintptr argIndex,Arg0,Args... args)
{
	return validateArgTypes(functionType,argIndex + 1,args...)
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
		outReturn = ((Return(*)(Args...))functionPtr)(args...);
		return true;
	}
	catch(...)
	{
		std::cerr << "Caught exception!" << std::endl;
		return false;
	}
}

#include "Common/WAVM.h"
#include "Common/Memory.h"
#include "Common/CLI.h"
#include "AST/AST.h"
#include "Runtime/LLVMJIT.h"

struct Void {};

template<typename NativeValue> struct NativeToASTType;
template<> struct NativeToASTType<uint8_t> { typedef AST::I8Type ASTType; };
template<> struct NativeToASTType<int8_t> { typedef AST::I8Type ASTType; };
template<> struct NativeToASTType<uint16_t> { typedef AST::I16Type ASTType; };
template<> struct NativeToASTType<int16_t> { typedef AST::I16Type ASTType; };
template<> struct NativeToASTType<uint32_t> { typedef AST::I32Type ASTType; };
template<> struct NativeToASTType<int32_t> { typedef AST::I32Type ASTType; };
template<> struct NativeToASTType<uint64_t> { typedef AST::I64Type ASTType; };
template<> struct NativeToASTType<int64_t> { typedef AST::I64Type ASTType; };
template<> struct NativeToASTType<float> { typedef AST::F32Type ASTType; };
template<> struct NativeToASTType<double> { typedef AST::F64Type ASTType; };
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

	void* functionPtr = LLVMJIT::getFunctionPointer(module,exportIt->second);
	assert(functionPtr);

	// Call the generated machine code for the function.
	try
	{
		// Call the function specified on the command-line.
		outReturn = ((Return(__cdecl*)(Args...))functionPtr)(args...);
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
	std::cout << "Loaded module uses " << (module->arena.getTotalAllocatedBytes() / 1024) << "KB" << std::endl;

	// Generate machine code for the module.
	if(!LLVMJIT::compileModule(module))
	{
		std::cerr << "Couldn't compile module." << std::endl;
		return false;
	}
	
	Void result;
	callModuleFunction(module,"__GLOBAL__sub_I_iostream_cpp",result);
	return true;
}

int main(int argc,char** argv)
{
	AST::Module* module;
	const char* functionName;
	if(!stricmp(argv[1],"-text") && argc == 4)
	{
		module = loadTextModule(argv[2]);
		functionName = argv[3];
	}
	else if(!stricmp(argv[1],"-binary") && argc == 5)
	{
		module = loadBinaryModule(argv[2],argv[3]);
		functionName = argv[4];
	}
	else
	{
		std::cerr <<  "Usage: Run -binary in.wasm in.js.mem functionname" << std::endl;
		std::cerr <<  "       Run -text in.wast functionname" << std::endl;
		return -1;
	}
	
	if(!module) { return -1; }

	if(!initModuleRuntime(module)) { return -1; }

	uint32_t returnCode;
	WAVM::Timer executionTime;
	if(!callModuleFunction(module,argv[3],returnCode)) { return -1; }
	executionTime.stop();

	std::cout << "Program returned: " << returnCode << std::endl;
	std::cout << "Execution time: " << executionTime.getMilliseconds() << "ms" << std::endl;

	return 0;
}
#include "Inline/BasicTypes.h"
#include "Platform/Platform.h"
#include "Logging/Logging.h"
#include "Runtime.h"
#include "RuntimePrivate.h"

namespace Runtime
{
	void init()
	{
		LLVMJIT::init();
		initWAVMIntrinsics();
	}
	
	bool isA(ObjectInstance* object,const ObjectType& type)
	{
		if(type.kind != object->kind) { return false; }

		switch(type.kind)
		{
		case ObjectKind::function: return asFunctionType(type) == asFunction(object)->type;
		case ObjectKind::global: return asGlobalType(type) == asGlobal(object)->type;
		case ObjectKind::table: return isSubset(asTableType(type),asTable(object)->type);
		case ObjectKind::memory: return isSubset(asMemoryType(type),asMemory(object)->type);
		default: Errors::unreachable();
		}
	}

	Result invokeFunction(FunctionInstance* function,const std::vector<Value>& parameters)
	{
		const FunctionType* functionType = function->type;
		
		// Check that the parameter types match the function, and copy them into a memory block that stores each as a 64-bit value.
		if(parameters.size() != functionType->parameters.size())
		{ throwException(Exception::invokeSignatureMismatchType); }

		V128* thunkMemory = (V128*)alloca((functionType->parameters.size() + getArity(functionType->ret)) * sizeof(V128));
		for(Uptr parameterIndex = 0;parameterIndex < functionType->parameters.size();++parameterIndex)
		{
			if(functionType->parameters[parameterIndex] != parameters[parameterIndex].type)
			{
				throwException(Exception::invokeSignatureMismatchType);
			}

			thunkMemory[parameterIndex] = parameters[parameterIndex].v128;
		}
		
		// Get the invoke thunk for this function type.
		LLVMJIT::InvokeFunctionPointer invokeFunctionPointer = LLVMJIT::getInvokeThunk(functionType);

		// Call the invoke thunk.
		(*invokeFunctionPointer)(function->nativeFunction,thunkMemory);

		// Read the return value out of the thunk memory block.
		Result result;
		if(functionType->ret != ResultType::none)
		{
			result.type = functionType->ret;
			result.v128 = thunkMemory[functionType->parameters.size()];
		}
		return result;
	}
	
	const FunctionType* getFunctionType(FunctionInstance* function)
	{
		return function->type;
	}

	GlobalInstance* createGlobal(GlobalType type,Value initialValue)
	{
		return new GlobalInstance(type,initialValue);
	}

	Value getGlobalValue(GlobalInstance* global)
	{
		return Value(global->type.valueType,global->value);
	}

	Value setGlobalValue(GlobalInstance* global,Value newValue)
	{
		assert(newValue.type == global->type.valueType);
		assert(global->type.isMutable);
		const Value previousValue = Value(global->type.valueType,global->value);
		global->value = newValue;
		return previousValue;
	}
}
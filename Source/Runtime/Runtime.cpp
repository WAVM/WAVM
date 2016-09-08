#include "Core/Core.h"
#include "Core/Platform.h"
#include "Runtime.h"
#include "RuntimePrivate.h"

namespace Runtime
{
	void init()
	{
		LLVMJIT::init();
	}

	[[noreturn]] void causeException(Exception::Cause cause)
	{
		auto callStack = describeExecutionContext(RuntimePlatform::captureExecutionContext());
		RuntimePlatform::raiseException(new Exception {cause,callStack});
	}

	std::string describeStackFrame(const StackFrame& frame)
	{
		std::string frameDescription;
		const bool hasDescripton = 
			LLVMJIT::describeInstructionPointer(frame.ip,frameDescription)
		||	RuntimePlatform::describeInstructionPointer(frame.ip,frameDescription);
		if(hasDescripton) { return frameDescription; }
		else { return "<unknown function>"; }
	}

	std::vector<std::string> describeExecutionContext(const ExecutionContext& executionContext)
	{
		std::vector<std::string> frameDescriptions;
		for(auto stackFrame : executionContext.stackFrames) { frameDescriptions.push_back(describeStackFrame(stackFrame)); }
		return frameDescriptions;
	}

	bool isA(Object* object,const ObjectType& type)
	{
		if(type.kind != object->kind) { return false; }

		switch(type.kind)
		{
		case ObjectKind::function: return type.function == asFunction(object)->type;
		case ObjectKind::global: return type.global == asGlobal(object)->type;
		case ObjectKind::table:
		{
			auto table = asTable(object);
			return type.table.elementType == table->type.elementType
				&&	isSubset(type.table.size,table->type.size);
		}
		case ObjectKind::memory:
		{
			auto memory = asMemory(object);
			return isSubset(type.memory.size,memory->type.size);
		}
		default: throw;
		}
	}

	Value invokeFunction(FunctionInstance* function,const std::vector<Value>& parameters)
	{
		assert(function->invokeThunk);

		const FunctionType* functionType = function->type;

		if(parameters.size() != functionType->parameters.size())
		{
			return Value(new Exception {Exception::Cause::invokeSignatureMismatch});
		}

		// Check that the parameter types match the function, and copy them into a memory block that stores each as a 64-bit value.
		uint64* thunkMemory = (uint64*)alloca((functionType->parameters.size() + getArity(functionType->ret)) * sizeof(uint64));
		for(uintptr parameterIndex = 0;parameterIndex < functionType->parameters.size();++parameterIndex)
		{
			if(asRuntimeValueType(functionType->parameters[parameterIndex]) != parameters[parameterIndex].type)
			{
				return Value(new Exception {Exception::Cause::invokeSignatureMismatch});
			}

			thunkMemory[parameterIndex] = parameters[parameterIndex].i64;
		}

		// Catch platform-specific runtime exceptions and turn them into Runtime::Values.
		return RuntimePlatform::catchRuntimeExceptions([&]
		{
			// Call the invoke thunk.
			(*function->invokeThunk)(thunkMemory);

			// Read the return value out of the thunk memory block.
			Value returnValue;
			if(functionType->ret != ReturnType::unit)
			{
				returnValue.type = asRuntimeValueType(functionType->ret);
				returnValue.i64 = thunkMemory[functionType->parameters.size()];
			}
			return returnValue;
		});
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
		return Value(asRuntimeValueType(global->type.valueType),global->value);
	}

	Value setGlobalValue(GlobalInstance* global,Value newValue)
	{
		assert(newValue.type == asRuntimeValueType(global->type.valueType));
		assert(global->type.isMutable);
		const Value previousValue = Value(asRuntimeValueType(global->type.valueType),global->value);
		global->value = newValue;
		return previousValue;
	}
}
#include "Core/Core.h"
#include "Core/Platform.h"
#include "Runtime.h"
#include "RuntimePrivate.h"

namespace Runtime
{
	void init()
	{
		LLVMJIT::init();
		initWAVMIntrinsics();
	}

	[[noreturn]] void causeException(Exception::Cause cause)
	{
		auto callStack = describeExecutionContext(Platform::captureExecutionContext());
		throw Exception {cause,callStack};
	}

	std::string describeStackFrame(const Platform::ExecutionContext::StackFrame& frame)
	{
		std::string frameDescription;
		const bool hasDescripton = 
			LLVMJIT::describeInstructionPointer(frame.ip,frameDescription)
		||	Platform::describeInstructionPointer(frame.ip,frameDescription);
		if(hasDescripton) { return frameDescription; }
		else { return "<unknown function>"; }
	}

	std::vector<std::string> describeExecutionContext(const Platform::ExecutionContext& executionContext)
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
		default: Core::unreachable();
		}
	}

	Result invokeFunction(FunctionInstance* function,const std::vector<Value>& parameters)
	{
		const FunctionType* functionType = function->type;

		if(parameters.size() != functionType->parameters.size())
		{
			throw Exception {Exception::Cause::invokeSignatureMismatch};
		}

		// Check that the parameter types match the function, and copy them into a memory block that stores each as a 64-bit value.
		uint64* thunkMemory = (uint64*)alloca((functionType->parameters.size() + getArity(functionType->ret)) * sizeof(uint64));
		for(uintp parameterIndex = 0;parameterIndex < functionType->parameters.size();++parameterIndex)
		{
			if(functionType->parameters[parameterIndex] != parameters[parameterIndex].type)
			{
				throw Exception {Exception::Cause::invokeSignatureMismatch};
			}

			thunkMemory[parameterIndex] = parameters[parameterIndex].i64;
		}
		
		// Get the invoke thunk for this function type.
		LLVMJIT::InvokeFunctionPointer invokeFunctionPointer = LLVMJIT::getInvokeThunk(functionType);

		// Catch platform-specific runtime exceptions and turn them into Runtime::Values.
		Result result;
		Platform::HardwareTrapType trapType;
		Platform::ExecutionContext trapContext;
		Platform::ExecutionContext callingContext;
		uintp trapOperand;
		trapType = Platform::catchHardwareTraps(trapContext,trapOperand,
			[&]
			{
				callingContext = Platform::captureExecutionContext();

				// Call the invoke thunk.
				(*invokeFunctionPointer)(function->nativeFunction,thunkMemory);

				// Read the return value out of the thunk memory block.
				if(functionType->ret != ResultType::none)
				{
					result.type = functionType->ret;
					result.i64 = thunkMemory[functionType->parameters.size()];
				}
			});
		
		// Truncate the stack frame to the native code invoking the function.
		if(trapContext.stackFrames.size() >= callingContext.stackFrames.size() + 1)
		{
			trapContext.stackFrames.resize(trapContext.stackFrames.size() - callingContext.stackFrames.size() -1);
		}

		std::vector<std::string> callStack = describeExecutionContext(trapContext);

		switch(trapType)
		{
		case Platform::HardwareTrapType::accessViolation:
		{
			auto cause = isAddressOwnedByTable(reinterpret_cast<uint8*>(trapOperand))
				? Exception::Cause::undefinedTableElement
				: Exception::Cause::accessViolation;
			if(cause == Exception::Cause::accessViolation && !isAddressOwnedByMemory(reinterpret_cast<uint8*>(trapOperand)))
			{
				Log::printf(Log::Category::error,"Access violation outside of table or memory reserved addresses. Call stack:\n");
				for(auto calledFunction : callStack) { Log::printf(Log::Category::error,"  %s\n",calledFunction.c_str()); }
				Core::fatalError("Aborting.");
			}
			throw Exception { cause, callStack };
		}
		case Platform::HardwareTrapType::stackOverflow:
			throw Exception { Exception::Cause::stackOverflow, callStack };
		case Platform::HardwareTrapType::intDivideByZeroOrOverflow:
			throw Exception { Exception::Cause::integerDivideByZeroOrIntegerOverflow, callStack };
		case Platform::HardwareTrapType::none: break;
		default: Core::unreachable();
		};

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
		assert(newValue.type == asRuntimeValueType(global->type.valueType));
		assert(global->type.isMutable);
		const Value previousValue = Value(global->type.valueType,global->value);
		global->value = newValue;
		return previousValue;
	}
}
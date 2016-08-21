#include "Core/Core.h"
#include "Core/Platform.h"
#include "AST/AST.h"
#include "Runtime.h"
#include "RuntimePrivate.h"

#include <iostream>

namespace AST { struct Module; }

namespace Runtime
{
	bool init()
	{
		LLVMJIT::init();
		return initInstanceMemory();
	}

	void causeException(Exception::Cause cause)
	{
		auto callStack = describeExecutionContext(RuntimePlatform::captureExecutionContext());
		RuntimePlatform::raiseException(new Exception {cause,callStack});
	}

	const char* describeExceptionCause(Exception::Cause cause)
	{
		switch(cause)
		{
		case Exception::Cause::Unknown: return "unknown";
		case Exception::Cause::AccessViolation: return "access violation";
		case Exception::Cause::StackOverflow: return "stack overflow";
		case Exception::Cause::IntegerDivideByZeroOrIntegerOverflow: return "integer divide by zero or signed integer overflow";
		case Exception::Cause::InvalidFloatOperation: return "invalid floating point operation";
		case Exception::Cause::InvokeSignatureMismatch: return "invoke signature mismatch";
		case Exception::Cause::OutOfMemory: return "out of memory";
		case Exception::Cause::GrowMemoryNotPageAligned: return "grow_memory called with size that isn't a multiple of page_size";
		case Exception::Cause::ReachedUnreachable: return "reached unreachable code";
		case Exception::Cause::IndirectCallSignatureMismatch: return "call_indirect to function with wrong signature";
		case Exception::Cause::OutOfBoundsFunctionTableIndex: return "out-of-bounds index for function table";
		default: return "unknown";
		}
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

	bool loadModule(const AST::Module* module)
	{
		// Free any existing memory.
		vmShrinkMemory(vmGrowMemory(0));

		// Initialize the module's requested initial memory.
		if(vmGrowMemory((int32)module->initialNumPagesMemory * AST::numBytesPerPage) != 0)
		{
			std::cerr << "Failed to commit the requested initial memory for module instance (" << module->initialNumPagesMemory*AST::numBytesPerPage/1024 << "KB requested)" << std::endl;
			return false;
		}

		// Copy the module's data segments into VM memory.
		if(module->initialNumPagesMemory * AST::numBytesPerPage >= (1ull<<32)) { throw; }
		for(auto dataSegment : module->dataSegments)
		{
			if(dataSegment.baseAddress + dataSegment.numBytes > module->initialNumPagesMemory * AST::numBytesPerPage)
			{
				std::cerr << "Module data segment exceeds initial memory allocation" << std::endl;
				return false;
			}
			memcpy(instanceMemoryBase + dataSegment.baseAddress,dataSegment.data,dataSegment.numBytes);
		}

		// Generate machine code for the module.
		if(!LLVMJIT::compileModule(module)) { return false; }

		// Initialize the intrinsics.
		if(!initEmscriptenIntrinsics(module)) { return false; }
		initWebAssemblyIntrinsics();
		initWAVMIntrinsics();

		// Call the module's start function.
		if(module->startFunctionIndex != UINTPTR_MAX)
		{
			assert(module->functions[module->startFunctionIndex].type == AST::FunctionType());
			auto result = invokeFunction(module,module->startFunctionIndex,nullptr);
			if(result.type == Runtime::TypeId::Exception)
			{
				std::cerr << "Module start function threw exception: " << Runtime::describeExceptionCause(result.exception->cause) << std::endl;
				for(auto calledFunction : result.exception->callStack) { std::cerr << "  " << calledFunction << std::endl; }
				return false;
			}
		}

		return true;
	}

	Value invokeFunction(const AST::Module* module,uintptr functionIndex,const Value* parameters)
	{
		// Check that the parameter types match the function, and copy them into a memory block that stores each as a 64-bit value.
		const AST::Function& function = module->functions[functionIndex];
		uint64* thunkMemory = (uint64*)alloca((function.type.parameters.size() + 1) * sizeof(uint64));
		for(uintptr parameterIndex = 0;parameterIndex < function.type.parameters.size();++parameterIndex)
		{
			if((Runtime::TypeId)(function.type.parameters[parameterIndex]) != parameters[parameterIndex].type)
			{
				return Value(new Exception {Exception::Cause::InvokeSignatureMismatch});
			}

			thunkMemory[parameterIndex] = parameters[parameterIndex].i64;
		}

		// Get a pointer to the invoke thunk for the JITted code.
		LLVMJIT::InvokeFunctionPointer functionPtr = LLVMJIT::getInvokeFunctionPointer(module,functionIndex,true);
		assert(functionPtr);

		// Catch platform-specific runtime exceptions and turn them into Runtime::Values.
		return RuntimePlatform::catchRuntimeExceptions([&]
		{
			// Call the invoke thunk.
			functionPtr(thunkMemory);

			// Read the return value out of the thunk memory block.
			Value returnValue;
			returnValue.type = (Runtime::TypeId)function.type.returnType;
			returnValue.i64 = thunkMemory[function.type.parameters.size()];
			return returnValue;
		});
	}
}
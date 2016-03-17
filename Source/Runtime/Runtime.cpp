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
		initEmscriptenIntrinsics(module);
		initWebAssemblyIntrinsics();
		initWAVMIntrinsics();

		// Call the module's start function.
		if(module->startFunctionIndex != UINTPTR_MAX)
		{
			assert(module->functions[module->startFunctionIndex]->type == AST::FunctionType());
			invokeFunction(module,module->startFunctionIndex,nullptr);
		}

		return true;
	}

	// This is called to recursively turn the boxed values in untypedArgs into C++ values.
	template<size_t numUntypedArgs,typename... Args>
	struct RecursiveInvoke
	{
		static Value invoke(const AST::FunctionType& type,void* functionPtr,const Value* untypedArgs,size_t numTypedArgs,Args... typedArgs)
		{
			switch(type.parameters[numTypedArgs])
			{
			case AST::TypeId::I8: return RecursiveInvoke<numUntypedArgs-1,Args...,uint8>::invoke(type,functionPtr,untypedArgs,numTypedArgs+1,typedArgs...,untypedArgs[numTypedArgs].i8);
			case AST::TypeId::I16: return RecursiveInvoke<numUntypedArgs-1,Args...,uint16>::invoke(type,functionPtr,untypedArgs,numTypedArgs+1,typedArgs...,untypedArgs[numTypedArgs].i16);
			case AST::TypeId::I32: return RecursiveInvoke<numUntypedArgs-1,Args...,uint32>::invoke(type,functionPtr,untypedArgs,numTypedArgs+1,typedArgs...,untypedArgs[numTypedArgs].i32);
			case AST::TypeId::I64: return RecursiveInvoke<numUntypedArgs-1,Args...,uint64>::invoke(type,functionPtr,untypedArgs,numTypedArgs+1,typedArgs...,untypedArgs[numTypedArgs].i64);
			case AST::TypeId::F32: return RecursiveInvoke<numUntypedArgs-1,Args...,float32>::invoke(type,functionPtr,untypedArgs,numTypedArgs+1,typedArgs...,untypedArgs[numTypedArgs].f32);
			case AST::TypeId::F64: return RecursiveInvoke<numUntypedArgs-1,Args...,float64>::invoke(type,functionPtr,untypedArgs,numTypedArgs+1,typedArgs...,untypedArgs[numTypedArgs].f64);
			default: throw;
			};
		}
	};

	// This partial specialization terminates the induction on parameter types, and dispatches to the
	// appropriate C++ function type depending on the AST function return type.
	template<typename... Args>
	struct RecursiveInvoke<0,Args...>
	{
		static Value invoke(const AST::FunctionType& type,void* functionPtr,const Value* untypedArgs,size_t numTypedArgs,Args... typedArgs)
		{
			switch(type.returnType)
			{
			case AST::TypeId::I8: return Value(((uint8(*)(Args...))functionPtr)(typedArgs...));
			case AST::TypeId::I16: return Value(((uint16(*)(Args...))functionPtr)(typedArgs...));
			case AST::TypeId::I32: return Value(((uint32(*)(Args...))functionPtr)(typedArgs...));
			case AST::TypeId::I64: return Value(((uint64(*)(Args...))functionPtr)(typedArgs...));
			case AST::TypeId::F32: return Value(((float32(*)(Args...))functionPtr)(typedArgs...));
			case AST::TypeId::F64: return Value(((float64(*)(Args...))functionPtr)(typedArgs...));
			case AST::TypeId::Void: ((void(*)(Args...))functionPtr)(typedArgs...); return Value(Void());
			default: throw;
			}
		}
	};

	Value invokeFunction(const AST::Module* module,uintptr functionIndex,const Value* parameters)
	{
		// Check that the parameter types match the function.
		auto function = module->functions[functionIndex];
		for(uintptr parameterIndex = 0;parameterIndex < function->type.parameters.size();++parameterIndex)
		{
			if((Runtime::TypeId)(function->type.parameters[parameterIndex]) != parameters[parameterIndex].type)
			{
				return Value(new Exception {Exception::Cause::InvokeSignatureMismatch});
			}
		}

		// Get a pointer to the JITed function code.
		void* functionPtr = LLVMJIT::getFunctionPointer(module,functionIndex);
		assert(functionPtr);

		// Catch platform-specific runtime exceptions and turn them into Runtime::Values.
		return RuntimePlatform::catchRuntimeExceptions([&]
		{
			// Dispatch the invoke by number of parameters in the function.
			// We're practically limited in how many parameters can be handled because this instantiates RecursiveInvoke 7^N times.
			switch(function->type.parameters.size())
			{
			case 0: return RecursiveInvoke<0>::invoke(function->type,functionPtr,parameters,0);
			case 1: return RecursiveInvoke<1>::invoke(function->type,functionPtr,parameters,0);
			case 2: return RecursiveInvoke<2>::invoke(function->type,functionPtr,parameters,0);
			case 3: return RecursiveInvoke<3>::invoke(function->type,functionPtr,parameters,0);
			case 4: return RecursiveInvoke<4>::invoke(function->type,functionPtr,parameters,0);
			default: return Value(new Exception {Exception::Cause::InvokeSignatureMismatch});
			}
		});
	}
}
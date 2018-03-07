#include "Inline/BasicTypes.h"
#include "Logging/Logging.h"
#include "Runtime.h"
#include "RuntimePrivate.h"

namespace Runtime
{
	bool isA(Object* object,const ObjectType& type)
	{
		if(Runtime::ObjectKind(type.kind) != object->kind) { return false; }

		switch(type.kind)
		{
		case IR::ObjectKind::function: return asFunctionType(type) == asFunction(object)->type;
		case IR::ObjectKind::global: return asGlobalType(type) == asGlobal(object)->type;
		case IR::ObjectKind::table: return isSubset(asTableType(type),asTable(object)->type);
		case IR::ObjectKind::memory: return isSubset(asMemoryType(type),asMemory(object)->type);
		case IR::ObjectKind::exceptionType: return asExceptionTypeType(type) == asExceptionType(object)->parameters;
		default: Errors::unreachable();
		}
	}

	IR::ObjectType getObjectType(Object* object)
	{
		switch(object->kind)
		{
		case Runtime::ObjectKind::function: return asFunction(object)->type;
		case Runtime::ObjectKind::global: return asGlobal(object)->type;
		case Runtime::ObjectKind::table: return asTable(object)->type;
		case Runtime::ObjectKind::memory: return asMemory(object)->type;
		case Runtime::ObjectKind::exceptionType: return asExceptionType(object)->parameters;
		default: Errors::unreachable();
		};
	}

	Result invokeFunction(Context* context,FunctionInstance* function,const std::vector<Value>& parameters)
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

		ContextRuntimeData* contextRuntimeData = &context->compartment->runtimeData->contexts[context->id];

		// Call the invoke thunk.
		(*invokeFunctionPointer)(function->nativeFunction,contextRuntimeData,thunkMemory);

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

	GlobalInstance* createGlobal(Compartment* compartment,GlobalType type,Value initialValue)
	{
		assert(initialValue.type == type.valueType);

		// Allow immutable globals to be created without a compartment.
		errorUnless(!type.isMutable || compartment);

		U32 dataOffset = U32(-1);
		if(type.isMutable)
		{
			// Allocate a naturally aligned address to store the global at in the per-context data.
			const U32 numBytes = getTypeByteWidth(type.valueType);
			{
				Platform::Lock compartmentLock(compartment->mutex);
				dataOffset = (compartment->numGlobalBytes + numBytes - 1) & ~(numBytes - 1);
				if(dataOffset + numBytes >= maxGlobalBytes) { return nullptr; }
				compartment->numGlobalBytes = dataOffset + numBytes;
			}

			// Initialize the global value for each context.
			for(Context* context : compartment->contexts)
			{
				memcpy(context->runtimeData->globalData + dataOffset,&initialValue,numBytes);
			}
		}

		return new GlobalInstance(compartment,type,dataOffset,initialValue);
	}

	Value getGlobalValue(Context* context,GlobalInstance* global)
	{
		assert(context || !global->type.isMutable);
		return Value(
			global->type.valueType,
			global->type.isMutable
			? *(UntaggedValue*)(context->runtimeData->globalData + global->mutableDataOffset)
			: global->immutableValue);
	}

	Value setGlobalValue(Context* context,GlobalInstance* global,Value newValue)
	{
		assert(context);
		assert(newValue.type == global->type.valueType);
		assert(global->type.isMutable);
		UntaggedValue& value = *(UntaggedValue*)(context->runtimeData->globalData + global->mutableDataOffset);
		const Value previousValue = Value(global->type.valueType,value);
		value = newValue;
		return previousValue;
	}

	Compartment::Compartment()
	: ObjectImpl(ObjectKind::compartment)
	, mutex(Platform::createMutex())
	, numGlobalBytes(0)
	{
		runtimeData = (CompartmentRuntimeData*)Platform::allocateAlignedVirtualPages(
			compartmentReservedBytes >> Platform::getPageSizeLog2(),
			compartmentRuntimeDataAlignmentLog2,
			unalignedRuntimeData);

		errorUnless(Platform::commitVirtualPages(
			(U8*)runtimeData,
			offsetof(CompartmentRuntimeData,contexts) >> Platform::getPageSizeLog2()));

		runtimeData->compartment = this;

		wavmIntrinsics = instantiateWAVMIntrinsics(this);
	}

	Compartment::~Compartment()
	{
		Platform::destroyMutex(mutex);
		Platform::decommitVirtualPages((U8*)runtimeData,compartmentReservedBytes >> Platform::getPageSizeLog2());
		Platform::freeAlignedVirtualPages(unalignedRuntimeData,
			compartmentReservedBytes >> Platform::getPageSizeLog2(),
			compartmentRuntimeDataAlignmentLog2);
		runtimeData = nullptr;
		unalignedRuntimeData = nullptr;
	}

	Compartment* createCompartment()
	{
		return new Compartment();
	}

	Context* createContext(Compartment* compartment)
	{
		assert(compartment);
		Context* context = new Context(compartment);
		{
			Platform::Lock lock(compartment->mutex);
			context->id = compartment->contexts.size();
			context->runtimeData = &compartment->runtimeData->contexts[context->id];
			compartment->contexts.push_back(context);
		}

		errorUnless(Platform::commitVirtualPages(
			(U8*)context->runtimeData,
			sizeof(ContextRuntimeData) >> Platform::getPageSizeLog2()));

		return context;
	}

	Context::~Context()
	{
		Platform::Lock compartmentLock(compartment->mutex);
		compartment->contexts[id] = nullptr;
	}

	Compartment* getCompartmentFromContext(Context* context)
	{
		return context->compartment;
	}
}
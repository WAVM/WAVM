#include "Inline/Assert.h"
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

	UntaggedValue* invokeFunctionUnchecked(Context* context,FunctionInstance* function,const UntaggedValue* arguments)
	{
		const FunctionType* functionType = function->type;
		
		// Get the invoke thunk for this function type.
		auto invokeFunctionPointer = LLVMJIT::getInvokeThunk(functionType,function->callingConvention);

		// Copy the arguments into the thunk arguments buffer in ContextRuntimeData.
		ContextRuntimeData* contextRuntimeData = &context->compartment->runtimeData->contexts[context->id];
		U8* argData = contextRuntimeData->thunkArgAndReturnData;
		Uptr argDataOffset = 0;
		for(Uptr argumentIndex = 0;argumentIndex < functionType->parameters.size();++argumentIndex)
		{
			const ValueType type = functionType->parameters[argumentIndex];
			const UntaggedValue& argument = arguments[argumentIndex];
			if(type == ValueType::v128)
			{
				// Use 16-byte alignment for V128 arguments.
				argDataOffset = (argDataOffset + 15) & ~15;
			}
			if(argDataOffset >= maxThunkArgAndReturnBytes)
			{
				// Throw an exception if the invoke uses too much memory for arguments.
				throwException(Exception::outOfMemoryType);
			}
			memcpy(argData + argDataOffset,argument.bytes,getTypeByteWidth(type));
			argDataOffset += type == ValueType::v128 ? 16 : 8;
		}

		// Call the invoke thunk.
		contextRuntimeData = (ContextRuntimeData*)(*invokeFunctionPointer)(
			function->nativeFunction,
			contextRuntimeData);

		// Return a pointer to the return value that was written to the ContextRuntimeData.
		return (UntaggedValue*)contextRuntimeData->thunkArgAndReturnData;
	}

	Result invokeFunctionChecked(Context* context,FunctionInstance* function,const std::vector<Value>& arguments)
	{
		const FunctionType* functionType = function->type;

		// Check that the parameter types match the function, and copy them into a memory block that stores each as a 64-bit value.
		if(arguments.size() != functionType->parameters.size())
		{ throwException(Exception::invokeSignatureMismatchType); }

		// Convert the arguments from a vector of TaggedValues to a stack-allocated block of UntaggedValues.
		UntaggedValue* untaggedArguments = (UntaggedValue*)alloca(arguments.size() * sizeof(UntaggedValue));
		for(Uptr argumentIndex = 0;argumentIndex < arguments.size();++argumentIndex)
		{
			if(functionType->parameters[argumentIndex] != arguments[argumentIndex].type)
			{
				throwException(Exception::invokeSignatureMismatchType);
			}

			untaggedArguments[argumentIndex] = arguments[argumentIndex];
		}

		return Result(functionType->ret,*invokeFunctionUnchecked(context,function,untaggedArguments));
	}
	
	const FunctionType* getFunctionType(FunctionInstance* function)
	{
		return function->type;
	}

	GlobalInstance* createGlobal(Compartment* compartment,GlobalType type,Value initialValue)
	{
		wavmAssert(initialValue.type == type.valueType);

		// Allow immutable globals to be created without a compartment.
		errorUnless(!type.isMutable || compartment);

		Platform::Lock compartmentLock(compartment->mutex);
		GlobalInstance* globalInstance;
		if(!type.isMutable) { globalInstance = new GlobalInstance(compartment,type,UINT32_MAX,initialValue); }
		else
		{
			// Allocate a naturally aligned address to store the global at in the per-context data.
			const U32 numBytes = getTypeByteWidth(type.valueType);
			U32 dataOffset = (compartment->numGlobalBytes + numBytes - 1) & ~(numBytes - 1);
			if(dataOffset + numBytes >= maxGlobalBytes) { return nullptr; }
			compartment->numGlobalBytes = dataOffset + numBytes;

			// Initialize the global value for each context, and the data used to initialize new contexts.
			memcpy(compartment->initialContextGlobalData + dataOffset,&initialValue,numBytes);
			for(Context* context : compartment->contexts)
			{
				memcpy(context->runtimeData->globalData + dataOffset,&initialValue,numBytes);
			}

			globalInstance = new GlobalInstance(compartment,type,dataOffset,initialValue);
		}

		globalInstance->id = compartment->globals.size();
		compartment->globals.push_back(globalInstance);

		return globalInstance;
	}

	GlobalInstance* cloneGlobal(GlobalInstance* global,Compartment* newCompartment)
	{
		return createGlobal(newCompartment,global->type,Value(global->type.valueType,global->initialValue));
	}

	void GlobalInstance::finalize()
	{
		Platform::Lock compartmentLock(compartment->mutex);
		compartment->globals[id] = nullptr;
	}

	Value getGlobalValue(Context* context,GlobalInstance* global)
	{
		wavmAssert(context || !global->type.isMutable);
		return Value(
			global->type.valueType,
			global->type.isMutable
			? *(UntaggedValue*)(context->runtimeData->globalData + global->mutableDataOffset)
			: global->initialValue);
	}

	Value setGlobalValue(Context* context,GlobalInstance* global,Value newValue)
	{
		wavmAssert(context);
		wavmAssert(newValue.type == global->type.valueType);
		wavmAssert(global->type.isMutable);
		UntaggedValue& value = *(UntaggedValue*)(context->runtimeData->globalData + global->mutableDataOffset);
		const Value previousValue = Value(global->type.valueType,value);
		value = newValue;
		return previousValue;
	}

	Compartment::Compartment()
	: ObjectImpl(ObjectKind::compartment)
	, unalignedRuntimeData(nullptr)
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

	Compartment* cloneCompartment(Compartment* compartment)
	{
		Compartment* newCompartment = new Compartment;

		Platform::Lock lock(compartment->mutex);
		
		// Clone globals.
		for(Uptr globalIndex = 0;globalIndex < compartment->globals.size();++globalIndex)
		{
			GlobalInstance* global = compartment->globals[globalIndex];
			GlobalInstance* newGlobal = cloneGlobal(global,newCompartment);
			SUPPRESS_UNUSED(newGlobal);
			wavmAssert(newGlobal->id == global->id);
			wavmAssert(newGlobal->mutableDataOffset == global->mutableDataOffset);
		}
		wavmAssert(newCompartment->numGlobalBytes == compartment->numGlobalBytes);

		// Clone memories.
		for(Uptr memoryIndex = 0;memoryIndex < compartment->memories.size();++memoryIndex)
		{
			MemoryInstance* memory = compartment->memories[memoryIndex];
			MemoryInstance* newMemory = cloneMemory(memory,newCompartment);
			SUPPRESS_UNUSED(newMemory);
			wavmAssert(newMemory->id == memory->id);
		}

		// Clone tables.
		for(Uptr tableIndex = 0;tableIndex < compartment->tables.size();++tableIndex)
		{
			TableInstance* table = compartment->tables[tableIndex];
			TableInstance* newTable = cloneTable(table,newCompartment);
			SUPPRESS_UNUSED(newTable);
			wavmAssert(newTable->id == table->id);
		}

		return newCompartment;
	}

	Context* createContext(Compartment* compartment)
	{
		wavmAssert(compartment);
		Context* context = new Context(compartment);
		{
			Platform::Lock lock(compartment->mutex);

			// Allocate an ID for the context in the compartment.
			context->id = compartment->contexts.size();
			context->runtimeData = &compartment->runtimeData->contexts[context->id];
			compartment->contexts.push_back(context);

			// Commit the page(s) for the context's runtime data.
			errorUnless(Platform::commitVirtualPages(
				(U8*)context->runtimeData,
				sizeof(ContextRuntimeData) >> Platform::getPageSizeLog2()));

			// Initialize the context's global data.
			memcpy(
				context->runtimeData->globalData,
				compartment->initialContextGlobalData,
				compartment->numGlobalBytes);
		}

		return context;
	}

	void Context::finalize()
	{
		Platform::Lock compartmentLock(compartment->mutex);
		compartment->contexts[id] = nullptr;
	}

	Compartment* getCompartmentFromContext(Context* context)
	{
		return context->compartment;
	}

	Context* cloneContext(Context* context,Compartment* newCompartment)
	{
		// Create a new context and initialize its runtime data with the values from the source context.
		Context* clonedContext = createContext(newCompartment);
		const Uptr numGlobalBytes = context->compartment->numGlobalBytes;
		wavmAssert(numGlobalBytes <= newCompartment->numGlobalBytes);
		memcpy(
			clonedContext->runtimeData->globalData,
			context->runtimeData->globalData,
			numGlobalBytes);
		return clonedContext;
	}

	Context* getContextFromRuntimeData(ContextRuntimeData* contextRuntimeData)
	{
		const CompartmentRuntimeData* compartmentRuntimeData = getCompartmentRuntimeData(contextRuntimeData);
		const Uptr contextId = contextRuntimeData - compartmentRuntimeData->contexts;
		Platform::Lock compartmentLock(compartmentRuntimeData->compartment->mutex);
		return compartmentRuntimeData->compartment->contexts[contextId];
	}

	ContextRuntimeData* getContextRuntimeData(Context* context)
	{
		return context->runtimeData;
	}

	TableInstance* getTableFromRuntimeData(ContextRuntimeData* contextRuntimeData,Uptr tableId)
	{
		Compartment* compartment = getCompartmentRuntimeData(contextRuntimeData)->compartment;
		Platform::Lock compartmentLock(compartment->mutex);
		wavmAssert(tableId < compartment->tables.size());
		return compartment->tables[tableId];
	}

	MemoryInstance* getMemoryFromRuntimeData(ContextRuntimeData* contextRuntimeData,Uptr memoryId)
	{
		Compartment* compartment = getCompartmentRuntimeData(contextRuntimeData)->compartment;
		Platform::Lock compartmentLock(compartment->mutex);
		return compartment->memories[memoryId];
	}
}
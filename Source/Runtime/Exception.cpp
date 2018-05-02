#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Logging/Logging.h"
#include "Runtime.h"
#include "RuntimePrivate.h"
#include "Intrinsics.h"

namespace Runtime
{
	static const TupleType* unitTupleType = TupleType::get({});
	const GCPointer<ExceptionTypeInstance> Exception::accessViolationType = createExceptionTypeInstance(unitTupleType);
	const GCPointer<ExceptionTypeInstance> Exception::stackOverflowType = createExceptionTypeInstance(unitTupleType);
	const GCPointer<ExceptionTypeInstance> Exception::integerDivideByZeroOrIntegerOverflowType = createExceptionTypeInstance(unitTupleType);
	const GCPointer<ExceptionTypeInstance> Exception::invalidFloatOperationType = createExceptionTypeInstance(unitTupleType);
	const GCPointer<ExceptionTypeInstance> Exception::invokeSignatureMismatchType = createExceptionTypeInstance(unitTupleType);
	const GCPointer<ExceptionTypeInstance> Exception::reachedUnreachableType = createExceptionTypeInstance(unitTupleType);
	const GCPointer<ExceptionTypeInstance> Exception::indirectCallSignatureMismatchType = createExceptionTypeInstance(unitTupleType);
	const GCPointer<ExceptionTypeInstance> Exception::undefinedTableElementType = createExceptionTypeInstance(unitTupleType);
	const GCPointer<ExceptionTypeInstance> Exception::calledAbortType = createExceptionTypeInstance(unitTupleType);
	const GCPointer<ExceptionTypeInstance> Exception::calledUnimplementedIntrinsicType = createExceptionTypeInstance(unitTupleType);
	const GCPointer<ExceptionTypeInstance> Exception::outOfMemoryType = createExceptionTypeInstance(unitTupleType);
	const GCPointer<ExceptionTypeInstance> Exception::invalidSegmentOffsetType = createExceptionTypeInstance(unitTupleType);
	const GCPointer<ExceptionTypeInstance> Exception::misalignedAtomicMemoryAccessType = createExceptionTypeInstance(unitTupleType);
	const GCPointer<ExceptionTypeInstance> Exception::invalidArgumentType = createExceptionTypeInstance(unitTupleType);
	
	// Returns a vector of strings, each element describing a frame of the call stack.
	// If the frame is a JITed function, use the JIT's information about the function
	// to describe it, otherwise fallback to whatever platform-specific symbol resolution
	// is available.
	std::vector<std::string> describeCallStack(const Platform::CallStack& callStack)
	{
		std::vector<std::string> frameDescriptions;
		for(auto frame : callStack.stackFrames)
		{
			std::string frameDescription;
			if(	LLVMJIT::describeInstructionPointer(frame.ip,frameDescription)
			||	Platform::describeInstructionPointer(frame.ip,frameDescription))
			{
				frameDescriptions.push_back(frameDescription);
			}
			else { frameDescriptions.push_back("<unknown function>"); }
		}
		return frameDescriptions;
	}

	ExceptionTypeInstance* createExceptionTypeInstance(const IR::TupleType* parameters)
	{
		return new ExceptionTypeInstance(parameters);
	}
	std::string describeExceptionType(const ExceptionTypeInstance* type)
	{
		wavmAssert(type);
		if(type == Exception::accessViolationType) { return "access violation"; }
		else if(type == Exception::stackOverflowType) { return "stack overflow"; }
		else if(type == Exception::integerDivideByZeroOrIntegerOverflowType) { return "integer divide-by-zero or overflow"; }
		else if(type == Exception::invalidFloatOperationType) { return "invalid float operation"; }
		else if(type == Exception::invokeSignatureMismatchType) { return "invoke signature mismatch"; }
		else if(type == Exception::reachedUnreachableType) { return "reached unreachable"; }
		else if(type == Exception::indirectCallSignatureMismatchType) { return "indirect call signature mismatch"; }
		else if(type == Exception::undefinedTableElementType) { return "undefined table element"; }
		else if(type == Exception::calledAbortType) { return "called abort"; }
		else if(type == Exception::calledUnimplementedIntrinsicType) { return "called unimplemented intrinsic"; }
		else if(type == Exception::outOfMemoryType) { return "out of memory"; }
		else if(type == Exception::invalidSegmentOffsetType) { return "invalid segment offset"; }
		else if(type == Exception::misalignedAtomicMemoryAccessType) { return "misaligned atomic memory access"; }
		else if(type == Exception::invalidArgumentType) { return "invalid argument"; }
		else
		{
			std::string result = "user exception<" + std::to_string(reinterpret_cast<Uptr>(type)) + ">(";
			for(Uptr parameterIndex = 0;parameterIndex < type->parameters->elements.size();++parameterIndex)
			{
				if(parameterIndex != 0) { result += ','; }
				result += asString(type->parameters->elements[parameterIndex]);
			}
			result += ')';
			return result;
		}
	}
	
	const IR::TupleType* getExceptionTypeParameters(const ExceptionTypeInstance* type)
	{
		return type->parameters;
	}

	std::string describeException(const Exception& exception)
	{
		std::string result = describeExceptionType(exception.type);
		wavmAssert(exception.arguments.size() == exception.type->parameters->elements.size());
		if(exception.arguments.size())
		{
			result += '(';
			for(Uptr argumentIndex = 0;argumentIndex < exception.arguments.size();++argumentIndex)
			{
				if(argumentIndex != 0) { result += ','; }
				result += asString(Value(exception.type->parameters->elements[argumentIndex],exception.arguments[argumentIndex]));
			}
			result += ')';
		}
		std::vector<std::string> callStackDescription = describeCallStack(exception.callStack);
		result += "\nCall stack:\n";
		for(auto calledFunction : callStackDescription) { result += "  "; result += calledFunction.c_str(); result += '\n'; }
		return result;
	}

	[[noreturn]] void throwException(ExceptionTypeInstance* type,std::vector<UntaggedValue>&& arguments)
	{
		wavmAssert(arguments.size() == type->parameters->elements.size());
		ExceptionData* exceptionData = (ExceptionData*)malloc(ExceptionData::calcNumBytes(type->parameters->elements.size()));
		exceptionData->typeInstance = type;
		exceptionData->isUserException = 0;
		memcpy(exceptionData->arguments,arguments.data(),sizeof(UntaggedValue) * arguments.size());
		Platform::raisePlatformException(exceptionData);
	}
	
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"throwException",void,intrinsicThrowException,
		I64 exceptionTypeInstanceBits, I64 argsBits, I32 isUserException)
	{
		auto typeInstance = reinterpret_cast<ExceptionTypeInstance*>(Uptr(exceptionTypeInstanceBits));
		auto args = reinterpret_cast<const UntaggedValue*>(Uptr(argsBits));
		
		ExceptionData* exceptionData = (ExceptionData*)malloc(ExceptionData::calcNumBytes(typeInstance->parameters->elements.size()));
		exceptionData->typeInstance = typeInstance;
		exceptionData->isUserException = isUserException ? 1 : 0;
		memcpy(exceptionData->arguments,args,sizeof(UntaggedValue) * typeInstance->parameters->elements.size());
		Platform::raisePlatformException(exceptionData);
	}

	static Exception translateExceptionDataToException(
		const ExceptionData* exceptionData,
		const Platform::CallStack& callStack)
	{
		ExceptionTypeInstance* runtimeType = exceptionData->typeInstance;
		std::vector<UntaggedValue> arguments(
			exceptionData->arguments,
			exceptionData->arguments + exceptionData->typeInstance->parameters->elements.size());
		return Exception { runtimeType, std::move(arguments), callStack };
	}

	static bool translateSignalToRuntimeException(
		const Platform::Signal& signal,
		const Platform::CallStack& callStack,
		Runtime::Exception& outException)
	{
		switch(signal.type)
		{
		case Platform::Signal::Type::accessViolation:
		{
			// If the access violation occured in a Table's reserved pages, treat it as an undefined table element runtime error.
			if(isAddressOwnedByTable(reinterpret_cast<U8*>(signal.accessViolation.address)))
			{
				outException = Exception { Exception::undefinedTableElementType, {}, callStack };
				return true;
			}
			// If the access violation occured in a Memory's reserved pages, treat it as an access violation runtime error.
			else if(isAddressOwnedByMemory(reinterpret_cast<U8*>(signal.accessViolation.address)))
			{
				outException = Exception { Exception::accessViolationType, {}, callStack };
				return true;
			}
			return false;
		}
		case Platform::Signal::Type::stackOverflow:
			outException = Exception { Exception::stackOverflowType, {}, callStack };
			return true;
		case Platform::Signal::Type::intDivideByZeroOrOverflow:
			outException = Exception { Exception::integerDivideByZeroOrIntegerOverflowType, {}, callStack };
			return true;
		case Platform::Signal::Type::unhandledException:
			outException = translateExceptionDataToException(
				reinterpret_cast<const ExceptionData*>(signal.unhandledException.data),
				callStack);
			return true;
		default: Errors::unreachable();
		}
	}

	void catchRuntimeExceptions(
		const std::function<void()>& thunk,
		const std::function<void(Exception&&)>& catchThunk
		)
	{
		// Catch platform exceptions and translate them into C++ exceptions.
		Result result;
		Platform::catchPlatformExceptions(
			[thunk,catchThunk]
			{
				Platform::catchSignals(
					thunk,
					[catchThunk](Platform::Signal signal,const Platform::CallStack& callStack) -> bool
					{
						Exception exception;
						if(translateSignalToRuntimeException(signal,callStack,exception))
						{
							catchThunk(std::move(exception));
							return true;
						}
						else { return false; }
					});
			},
			[catchThunk](void* exceptionData,const Platform::CallStack& callStack)
			{
				catchThunk(translateExceptionDataToException(
					reinterpret_cast<ExceptionData*>(exceptionData),
					callStack));
			});
	}

	static std::atomic<UnhandledExceptionHandler> unhandledExceptionHandler;

	static bool globalSignalHandler(
		Platform::Signal signal,
		const Platform::CallStack& callStack)
	{
		Exception exception;
		if(translateSignalToRuntimeException(signal,callStack,exception))
		{
			(unhandledExceptionHandler.load())(std::move(exception));
			return true;
		}
		else { return false; }
	}

	void setUnhandledExceptionHandler(UnhandledExceptionHandler handler)
	{
		struct SignalHandlerRegistrar
		{
			SignalHandlerRegistrar()
			{
				Platform::setSignalHandler(globalSignalHandler);
			}
		} signalHandlerRegistrar;

		unhandledExceptionHandler.store(handler);
	}
}
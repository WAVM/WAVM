#include "Inline/BasicTypes.h"
#include "Platform/Platform.h"
#include "Logging/Logging.h"
#include "Runtime.h"
#include "RuntimePrivate.h"
#include "Intrinsics.h"

namespace Runtime
{
	const GCPointer<ExceptionTypeInstance> Exception::accessViolationType = new ExceptionTypeInstance(TupleType {{}});
	const GCPointer<ExceptionTypeInstance> Exception::stackOverflowType = new ExceptionTypeInstance(TupleType {{}});
	const GCPointer<ExceptionTypeInstance> Exception::integerDivideByZeroOrIntegerOverflowType = new ExceptionTypeInstance(TupleType {{}});
	const GCPointer<ExceptionTypeInstance> Exception::invalidFloatOperationType = new ExceptionTypeInstance(TupleType {{}});
	const GCPointer<ExceptionTypeInstance> Exception::invokeSignatureMismatchType = new ExceptionTypeInstance(TupleType {{}});
	const GCPointer<ExceptionTypeInstance> Exception::reachedUnreachableType = new ExceptionTypeInstance(TupleType {{}});
	const GCPointer<ExceptionTypeInstance> Exception::indirectCallSignatureMismatchType = new ExceptionTypeInstance(TupleType {{}});
	const GCPointer<ExceptionTypeInstance> Exception::undefinedTableElementType = new ExceptionTypeInstance(TupleType {{}});
	const GCPointer<ExceptionTypeInstance> Exception::calledAbortType = new ExceptionTypeInstance(TupleType {{}});
	const GCPointer<ExceptionTypeInstance> Exception::calledUnimplementedIntrinsicType = new ExceptionTypeInstance(TupleType {{}});
	const GCPointer<ExceptionTypeInstance> Exception::outOfMemoryType = new ExceptionTypeInstance(TupleType {{}});
	const GCPointer<ExceptionTypeInstance> Exception::invalidSegmentOffsetType = new ExceptionTypeInstance(TupleType {{}});
	const GCPointer<ExceptionTypeInstance> Exception::misalignedAtomicMemoryAccessType = new ExceptionTypeInstance(TupleType {{}});
	
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

	std::string describeExceptionType(const ExceptionTypeInstance* type)
	{
		assert(type);
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
		else
		{
			std::string result = "user exception<" + std::to_string(reinterpret_cast<Uptr>(type)) + ">(";
			for(Uptr parameterIndex = 0;parameterIndex < type->parameters.elements.size();++parameterIndex)
			{
				if(parameterIndex != 0) { result += ','; }
				result += asString(type->parameters.elements[parameterIndex]);
			}
			result += ')';
			return result;
		}
	}
	
	IR::TupleType getExceptionTypeParameters(const ExceptionTypeInstance* type)
	{
		return type->parameters;
	}

	std::string describeException(const Exception& exception)
	{
		std::string result = describeExceptionType(exception.type);
		assert(exception.arguments.size() == exception.type->parameters.elements.size());
		if(exception.arguments.size())
		{
			result += '(';
			for(Uptr argumentIndex = 0;argumentIndex < exception.arguments.size();++argumentIndex)
			{
				if(argumentIndex != 0) { result += ','; }
				result += asString(Value(exception.type->parameters.elements[argumentIndex],exception.arguments[argumentIndex]));
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
		assert(arguments.size() == type->parameters.elements.size());
		ExceptionData* exceptionData = (ExceptionData*)malloc(ExceptionData::calcNumBytes(type->parameters.elements.size()));
		exceptionData->typeInstance = type;
		exceptionData->isUserException = 0;
		memcpy(exceptionData->arguments,arguments.data(),sizeof(UntaggedValue) * arguments.size());
		Platform::raisePlatformException(exceptionData);
	}
	
	DEFINE_INTRINSIC_FUNCTION3(
		wavmIntrinsics,throwException,throwException,none,
		i64,exceptionTypeInstanceBits,
		i64,argsBits,
		i32,isUserException)
	{
		auto typeInstance = reinterpret_cast<ExceptionTypeInstance*>(Uptr(exceptionTypeInstanceBits));
		auto args = reinterpret_cast<const UntaggedValue*>(Uptr(argsBits));
		
		ExceptionData* exceptionData = (ExceptionData*)malloc(ExceptionData::calcNumBytes(typeInstance->parameters.elements.size()));
		exceptionData->typeInstance = typeInstance;
		exceptionData->isUserException = isUserException ? 1 : 0;
		memcpy(exceptionData->arguments,args,sizeof(UntaggedValue) * typeInstance->parameters.elements.size());
		Platform::raisePlatformException(exceptionData);
	}

	void catchRuntimeExceptions(
		const std::function<void()>& thunk,
		const std::function<void(Exception&&)>& catchThunk
		)
	{
		// Catch platform exceptions and translate them into C++ exceptions.
		Result result;
		Platform::catchPlatformExceptions(
			[&]
			{
				Platform::catchSignals(
					thunk,
					[&](Platform::SignalType signalType,void* signalData,const Platform::CallStack& callStack)
					{
						switch(signalType)
						{
						case Platform::SignalType::accessViolation:
						{
							auto accessViolationData = (Platform::AccessViolationSignalData*)signalData;
							// If the access violation occured in a Table's reserved pages, treat it as an undefined table element runtime error.
							if(isAddressOwnedByTable(reinterpret_cast<U8*>(accessViolationData->address))) { catchThunk(Exception { Exception::undefinedTableElementType, {}, std::move(callStack) }); }
							// If the access violation occured in a Memory's reserved pages, treat it as an access violation runtime error.
							else if(isAddressOwnedByMemory(reinterpret_cast<U8*>(accessViolationData->address))) { catchThunk(Exception { Exception::accessViolationType, {}, std::move(callStack) }); }
							else
							{
								// If the access violation occured outside of a Table or Memory, treat it as a bug (possibly a security hole)
								// rather than a runtime error in the WebAssembly code.
								std::vector<std::string> callStackDescription = describeCallStack(callStack);
								Log::printf(Log::Category::error,"Access violation outside of table or memory reserved addresses. Call stack:\n");
								for(auto calledFunction : callStackDescription) { Log::printf(Log::Category::error,"  %s\n",calledFunction.c_str()); }
								Errors::fatalf("Unsandboxed access violation!\n");
							}
							break;
						}
						case Platform::SignalType::stackOverflow:
							catchThunk(Exception { Exception::stackOverflowType, {}, std::move(callStack) });
							break;
						case Platform::SignalType::intDivideByZeroOrOverflow:
							catchThunk(Exception { Exception::integerDivideByZeroOrIntegerOverflowType, {}, std::move(callStack) });
							break;
						}
					});
			},
			[&](void* exceptionData)
			{
				ExceptionData* runtimeExceptionData = reinterpret_cast<ExceptionData*>(exceptionData);
				ExceptionTypeInstance* runtimeType = runtimeExceptionData->typeInstance;
				std::vector<UntaggedValue> arguments(
					runtimeExceptionData->arguments,
					runtimeExceptionData->arguments + runtimeExceptionData->typeInstance->parameters.elements.size());
				catchThunk(Exception { runtimeType, std::move(arguments), Platform::CallStack() });
			});
	}
}
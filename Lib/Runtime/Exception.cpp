#include <stdlib.h>
#include <string.h>
#include <atomic>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "IR/Types.h"
#include "IR/Value.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Errors.h"
#include "LLVMJIT/LLVMJIT.h"
#include "Platform/Diagnostics.h"
#include "Platform/Exception.h"
#include "Runtime/Intrinsics.h"
#include "Runtime/Runtime.h"
#include "Runtime/RuntimeData.h"
#include "RuntimePrivate.h"

using namespace Runtime;

#define DEFINE_STATIC_EXCEPTION_TYPE(name)                                                         \
	const GCPointer<ExceptionTypeInstance> Runtime::Exception::name##Type                          \
		= createExceptionTypeInstance(IR::ExceptionType{IR::TypeTuple()}, "wavm." #name);

DEFINE_STATIC_EXCEPTION_TYPE(accessViolation)
DEFINE_STATIC_EXCEPTION_TYPE(stackOverflow)
DEFINE_STATIC_EXCEPTION_TYPE(integerDivideByZeroOrOverflow)
DEFINE_STATIC_EXCEPTION_TYPE(invalidFloatOperation)
DEFINE_STATIC_EXCEPTION_TYPE(invokeSignatureMismatch)
DEFINE_STATIC_EXCEPTION_TYPE(reachedUnreachable)
DEFINE_STATIC_EXCEPTION_TYPE(indirectCallSignatureMismatch)
DEFINE_STATIC_EXCEPTION_TYPE(undefinedTableElement)
DEFINE_STATIC_EXCEPTION_TYPE(calledAbort)
DEFINE_STATIC_EXCEPTION_TYPE(calledUnimplementedIntrinsic)
DEFINE_STATIC_EXCEPTION_TYPE(outOfMemory)
DEFINE_STATIC_EXCEPTION_TYPE(invalidSegmentOffset)
DEFINE_STATIC_EXCEPTION_TYPE(misalignedAtomicMemoryAccess)
DEFINE_STATIC_EXCEPTION_TYPE(invalidArgument)

#undef DEFINE_STATIC_EXCEPTION_TYPE

bool Runtime::describeInstructionPointer(Uptr ip, std::string& outDescription)
{
	LLVMJIT::JITFunction* jitFunction = LLVMJIT::getJITFunctionByAddress(ip);
	if(!jitFunction) { return Platform::describeInstructionPointer(ip, outDescription); }
	else
	{
		switch(jitFunction->type)
		{
		case LLVMJIT::JITFunction::Type::wasmFunction:
		{
			outDescription = "wasm!";
			outDescription += jitFunction->functionInstance->moduleInstance->debugName;
			outDescription += '!';
			outDescription += jitFunction->functionInstance->debugName;
			outDescription += '+';

			// Find the highest entry in the offsetToOpIndexMap whose offset is <= the
			// symbol-relative IP.
			U32 ipOffset = (U32)(ip - jitFunction->baseAddress);
			Iptr opIndex = -1;
			for(auto offsetMapIt : jitFunction->offsetToOpIndexMap)
			{
				if(offsetMapIt.first <= ipOffset) { opIndex = offsetMapIt.second; }
				else
				{
					break;
				}
			}
			outDescription += std::to_string(opIndex >= 0 ? opIndex : 0);

			return true;
		}
		case LLVMJIT::JITFunction::Type::invokeThunk:
			outDescription = "thnk!";
			outDescription += asString(jitFunction->invokeThunkType);
			outDescription += '+';
			outDescription += std::to_string(ip - jitFunction->baseAddress);
			return true;
		case LLVMJIT::JITFunction::Type::intrinsicThunk:
			outDescription = "thnk!intrinsic+" + std::to_string(ip - jitFunction->baseAddress);
			return true;
		default: Errors::unreachable();
		};
	}
}

// Returns a vector of strings, each element describing a frame of the call stack. If the frame is a
// JITed function, use the JIT's information about the function to describe it, otherwise fallback
// to whatever platform-specific symbol resolution is available.
std::vector<std::string> Runtime::describeCallStack(const Platform::CallStack& callStack)
{
	std::vector<std::string> frameDescriptions;
	for(auto frame : callStack.stackFrames)
	{
		std::string frameDescription;
		if(describeInstructionPointer(frame.ip, frameDescription))
		{ frameDescriptions.push_back(frameDescription); }
		else
		{
			frameDescriptions.push_back("<unknown function>");
		}
	}
	return frameDescriptions;
}

ExceptionTypeInstance* Runtime::createExceptionTypeInstance(IR::ExceptionType type,
															std::string&& debugName)
{
	return new ExceptionTypeInstance(type, std::move(debugName));
}
std::string Runtime::describeExceptionType(const ExceptionTypeInstance* typeInstance)
{
	wavmAssert(typeInstance);
	return typeInstance->debugName;
}

IR::TypeTuple Runtime::getExceptionTypeParameters(const ExceptionTypeInstance* typeInstance)
{
	return typeInstance->type.params;
}

std::string Runtime::describeException(const Exception& exception)
{
	std::string result = describeExceptionType(exception.typeInstance);
	wavmAssert(exception.arguments.size() == exception.typeInstance->type.params.size());
	if(exception.arguments.size())
	{
		result += '(';
		for(Uptr argumentIndex = 0; argumentIndex < exception.arguments.size(); ++argumentIndex)
		{
			if(argumentIndex != 0) { result += ", "; }
			result += asString(IR::Value(exception.typeInstance->type.params[argumentIndex],
										 exception.arguments[argumentIndex]));
		}
		result += ')';
	}
	std::vector<std::string> callStackDescription = describeCallStack(exception.callStack);
	result += "\nCall stack:\n";
	for(auto calledFunction : callStackDescription)
	{
		result += "  ";
		result += calledFunction.c_str();
		result += '\n';
	}
	return result;
}

[[noreturn]] void Runtime::throwException(ExceptionTypeInstance* typeInstance,
										  std::vector<IR::UntaggedValue>&& arguments)
{
	wavmAssert(arguments.size() == typeInstance->type.params.size());
	ExceptionData* exceptionData
		= (ExceptionData*)malloc(ExceptionData::calcNumBytes(typeInstance->type.params.size()));
	exceptionData->typeInstance = typeInstance;
	exceptionData->isUserException = 0;
	if(arguments.size())
	{
		memcpy(exceptionData->arguments,
			   arguments.data(),
			   sizeof(IR::UntaggedValue) * arguments.size());
	}
	Platform::raisePlatformException(exceptionData);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "throwException",
						  void,
						  intrinsicThrowException,
						  Iptr exceptionTypeInstanceBits,
						  Iptr argsBits,
						  I32 isUserException)
{
	auto typeInstance = reinterpret_cast<ExceptionTypeInstance*>(Uptr(exceptionTypeInstanceBits));
	auto args = reinterpret_cast<const IR::UntaggedValue*>(Uptr(argsBits));

	ExceptionData* exceptionData
		= (ExceptionData*)malloc(ExceptionData::calcNumBytes(typeInstance->type.params.size()));
	exceptionData->typeInstance = typeInstance;
	exceptionData->isUserException = isUserException ? 1 : 0;
	memcpy(exceptionData->arguments,
		   args,
		   sizeof(IR::UntaggedValue) * typeInstance->type.params.size());
	Platform::raisePlatformException(exceptionData);
}

static Exception translateExceptionDataToException(const ExceptionData* exceptionData,
												   const Platform::CallStack& callStack)
{
	ExceptionTypeInstance* runtimeType = exceptionData->typeInstance;
	std::vector<IR::UntaggedValue> arguments(
		exceptionData->arguments,
		exceptionData->arguments + exceptionData->typeInstance->type.params.size());
	return Exception{runtimeType, std::move(arguments), callStack};
}

static bool translateSignalToRuntimeException(const Platform::Signal& signal,
											  const Platform::CallStack& callStack,
											  Runtime::Exception& outException)
{
	switch(signal.type)
	{
	case Platform::Signal::Type::accessViolation:
	{
		// If the access violation occured in a Table's reserved pages, treat it as an undefined
		// table element runtime error.
		if(isAddressOwnedByTable(reinterpret_cast<U8*>(signal.accessViolation.address)))
		{
			outException = Exception{Exception::undefinedTableElementType, {}, callStack};
			return true;
		}
		// If the access violation occured in a Memory's reserved pages, treat it as an access
		// violation runtime error.
		else if(isAddressOwnedByMemory(reinterpret_cast<U8*>(signal.accessViolation.address)))
		{
			outException = Exception{Exception::accessViolationType, {}, callStack};
			return true;
		}
		return false;
	}
	case Platform::Signal::Type::stackOverflow:
		outException = Exception{Exception::stackOverflowType, {}, callStack};
		return true;
	case Platform::Signal::Type::intDivideByZeroOrOverflow:
		outException = Exception{Exception::integerDivideByZeroOrOverflowType, {}, callStack};
		return true;
	case Platform::Signal::Type::unhandledException:
		outException = translateExceptionDataToException(
			reinterpret_cast<const ExceptionData*>(signal.unhandledException.data), callStack);
		return true;
	default: Errors::unreachable();
	}
}

void Runtime::catchRuntimeExceptions(const std::function<void()>& thunk,
									 const std::function<void(Exception&&)>& catchThunk)
{
	// Catch platform exceptions and translate them into C++ exceptions.
	Platform::catchPlatformExceptions(
		[thunk, catchThunk] {
			Platform::catchSignals(
				thunk,
				[catchThunk](Platform::Signal signal,
							 const Platform::CallStack& callStack) -> bool {
					Exception exception;
					if(translateSignalToRuntimeException(signal, callStack, exception))
					{
						catchThunk(std::move(exception));
						return true;
					}
					else
					{
						return false;
					}
				});
		},
		[catchThunk](void* exceptionData, const Platform::CallStack& callStack) {
			catchThunk(translateExceptionDataToException(
				reinterpret_cast<ExceptionData*>(exceptionData), callStack));
		});
}

static std::atomic<UnhandledExceptionHandler> unhandledExceptionHandler;

static bool globalSignalHandler(Platform::Signal signal, const Platform::CallStack& callStack)
{
	Exception exception;
	if(translateSignalToRuntimeException(signal, callStack, exception))
	{
		(unhandledExceptionHandler.load())(std::move(exception));
		return true;
	}
	else
	{
		return false;
	}
}

void Runtime::setUnhandledExceptionHandler(UnhandledExceptionHandler handler)
{
	struct SignalHandlerRegistrar
	{
		SignalHandlerRegistrar() { Platform::setSignalHandler(globalSignalHandler); }
	} signalHandlerRegistrar;

	unhandledExceptionHandler.store(handler);
}

#include <stdlib.h>
#include <string.h>
#include <atomic>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "RuntimePrivate.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Platform/Diagnostics.h"
#include "WAVM/Platform/Exception.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/Runtime/RuntimeData.h"

using namespace WAVM;
using namespace WAVM::Runtime;

#define DEFINE_STATIC_EXCEPTION_TYPE(name, ...)                                                    \
	const GCPointer<ExceptionTypeInstance> Runtime::Exception::name##Type                          \
		= createExceptionTypeInstance(IR::ExceptionType{IR::TypeTuple({__VA_ARGS__})},             \
									  "wavm." #name);

DEFINE_STATIC_EXCEPTION_TYPE(outOfBoundsMemoryAccess, IR::ValueType::anyref, IR::ValueType::i64)
DEFINE_STATIC_EXCEPTION_TYPE(outOfBoundsTableAccess, IR::ValueType::anyref, IR::ValueType::i64)

DEFINE_STATIC_EXCEPTION_TYPE(outOfBoundsDataSegmentAccess,
							 IR::ValueType::anyref,
							 IR::ValueType::i64,
							 IR::ValueType::i64)

DEFINE_STATIC_EXCEPTION_TYPE(outOfBoundsElemSegmentAccess,
							 IR::ValueType::anyref,
							 IR::ValueType::i64,
							 IR::ValueType::i64)

DEFINE_STATIC_EXCEPTION_TYPE(stackOverflow)
DEFINE_STATIC_EXCEPTION_TYPE(integerDivideByZeroOrOverflow)
DEFINE_STATIC_EXCEPTION_TYPE(invalidFloatOperation)
DEFINE_STATIC_EXCEPTION_TYPE(invokeSignatureMismatch)
DEFINE_STATIC_EXCEPTION_TYPE(reachedUnreachable)
DEFINE_STATIC_EXCEPTION_TYPE(indirectCallSignatureMismatch)
DEFINE_STATIC_EXCEPTION_TYPE(uninitializedTableElement, IR::ValueType::anyref, IR::ValueType::i64)
DEFINE_STATIC_EXCEPTION_TYPE(calledAbort)
DEFINE_STATIC_EXCEPTION_TYPE(calledUnimplementedIntrinsic)
DEFINE_STATIC_EXCEPTION_TYPE(outOfMemory)
DEFINE_STATIC_EXCEPTION_TYPE(misalignedAtomicMemoryAccess, IR::ValueType::i64)
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
	Uptr runIP = 0;
	Uptr runLength = 0;
	for(Uptr frameIndex = 0; frameIndex < callStack.stackFrames.size(); ++frameIndex)
	{
		const Platform::CallStack::Frame& frame = callStack.stackFrames[frameIndex];
		if(frameIndex > 0 && frame.ip == runIP) { ++runLength; }
		else
		{
			if(runLength > 0)
			{
				frameDescriptions.push_back("<" + std::to_string(runLength)
											+ " identical frames omitted>");
			}

			runIP = frame.ip;
			runLength = 0;

			std::string frameDescription;
			if(!describeInstructionPointer(frame.ip, frameDescription))
			{ frameDescription = "<unknown function>"; }

			frameDescriptions.push_back(frameDescription);
		}
	}
	if(runLength > 0)
	{
		frameDescriptions.push_back("<" + std::to_string(runLength) + "identical frames omitted>");
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
	if(exception.typeInstance == Exception::outOfBoundsMemoryAccessType)
	{
		wavmAssert(exception.arguments.size() == 2);
		MemoryInstance* memory = asMemory(exception.arguments[0].anyRef->object);
		result += '(';
		result += memory->debugName;
		result += '+';
		result += std::to_string(exception.arguments[1].u64);
		result += ')';
	}
	else if(exception.typeInstance == Exception::outOfBoundsTableAccessType
			|| exception.typeInstance == Exception::uninitializedTableElementType)
	{
		wavmAssert(exception.arguments.size() == 2);
		TableInstance* table = asTable(exception.arguments[0].anyRef->object);
		result += '(';
		result += table->debugName;
		result += '[';
		result += std::to_string(exception.arguments[1].u64);
		result += "])";
	}
	else if(exception.arguments.size())
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
		TableInstance* table = nullptr;
		Uptr tableIndex = 0;
		MemoryInstance* memory = nullptr;
		Uptr memoryAddress = 0;
		if(isAddressOwnedByTable(
			   reinterpret_cast<U8*>(signal.accessViolation.address), table, tableIndex))
		{
			outException = Exception{Exception::outOfBoundsTableAccessType,
									 {asAnyRef(table), U64(tableIndex)},
									 callStack};
			return true;
		}
		// If the access violation occured in a Memory's reserved pages, treat it as an access
		// violation runtime error.
		else if(isAddressOwnedByMemory(
					reinterpret_cast<U8*>(signal.accessViolation.address), memory, memoryAddress))
		{
			outException = Exception{Exception::outOfBoundsMemoryAccessType,
									 {asAnyRef(memory), U64(memoryAddress)},
									 callStack};
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

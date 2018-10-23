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
#include "WAVM/Inline/Lock.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Platform/Diagnostics.h"
#include "WAVM/Platform/Exception.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/Runtime/RuntimeData.h"

using namespace WAVM;
using namespace WAVM::Runtime;

#define DEFINE_INTRINSIC_EXCEPTION_TYPE(name, ...)                                                 \
	ExceptionType* Runtime::Exception::name##Type = new ExceptionType(                             \
		nullptr, IR::ExceptionType{IR::TypeTuple({__VA_ARGS__})}, "wavm." #name);
ENUM_INTRINSIC_EXCEPTION_TYPES(DEFINE_INTRINSIC_EXCEPTION_TYPE)
#undef DEFINE_INTRINSIC_EXCEPTION_TYPE

bool Runtime::describeInstructionPointer(Uptr ip, std::string& outDescription)
{
	Runtime::Function* function = LLVMJIT::getFunctionByAddress(ip);
	if(!function) { return Platform::describeInstructionPointer(ip, outDescription); }
	else
	{
		outDescription = function->mutableData->debugName;
		outDescription += '+';

		// Find the highest entry in the offsetToOpIndexMap whose offset is <= the
		// symbol-relative IP.
		U32 ipOffset = (U32)(ip - reinterpret_cast<Uptr>(function->code));
		Iptr opIndex = -1;
		for(auto offsetMapIt : function->mutableData->offsetToOpIndexMap)
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

ExceptionType* Runtime::createExceptionType(Compartment* compartment,
											IR::ExceptionType sig,
											std::string&& debugName)
{
	auto exceptionType = new ExceptionType(compartment, sig, std::move(debugName));

	Lock<Platform::Mutex> compartmentLock(compartment->mutex);
	exceptionType->id = compartment->exceptionTypes.add(UINTPTR_MAX, exceptionType);
	if(exceptionType->id == UINTPTR_MAX)
	{
		delete exceptionType;
		return nullptr;
	}

	return exceptionType;
}

ExceptionType* Runtime::cloneExceptionType(ExceptionType* exceptionType,
										   Compartment* newCompartment)
{
	auto newExceptionType = new ExceptionType(
		newCompartment, exceptionType->sig, std::string(exceptionType->debugName));
	newExceptionType->id = exceptionType->id;

	Lock<Platform::Mutex> compartmentLock(newCompartment->mutex);
	newCompartment->exceptionTypes.insertOrFail(exceptionType->id, newExceptionType);
	return newExceptionType;
}

Runtime::ExceptionType::~ExceptionType()
{
	if(id != UINTPTR_MAX)
	{
		wavmAssertMutexIsLockedByCurrentThread(compartment->mutex);
		compartment->exceptionTypes.removeOrFail(id);
	}
}

std::string Runtime::describeExceptionType(const ExceptionType* type)
{
	wavmAssert(type);
	return type->debugName;
}

IR::TypeTuple Runtime::getExceptionTypeParameters(const ExceptionType* type)
{
	return type->sig.params;
}

std::string Runtime::describeException(const Exception& exception)
{
	std::string result = describeExceptionType(exception.type);
	wavmAssert(exception.arguments.size() == exception.type->sig.params.size());
	if(exception.type == Exception::outOfBoundsMemoryAccessType)
	{
		wavmAssert(exception.arguments.size() == 2);
		Memory* memory = asMemoryNullable(exception.arguments[0].object);
		result += '(';
		result += memory ? memory->debugName : "<unknown memory>";
		result += '+';
		result += std::to_string(exception.arguments[1].u64);
		result += ')';
	}
	else if(exception.type == Exception::outOfBoundsTableAccessType
			|| exception.type == Exception::uninitializedTableElementType)
	{
		wavmAssert(exception.arguments.size() == 2);
		Table* table = asTableNullable(exception.arguments[0].object);
		result += '(';
		result += table ? table->debugName : "<unknown table>";
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
			result += asString(IR::Value(exception.type->sig.params[argumentIndex],
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

[[noreturn]] void Runtime::throwException(ExceptionType* type,
										  std::vector<IR::UntaggedValue>&& arguments)
{
	wavmAssert(arguments.size() == type->sig.params.size());
	ExceptionData* exceptionData
		= (ExceptionData*)malloc(ExceptionData::calcNumBytes(type->sig.params.size()));
	exceptionData->typeId = type->id;
	exceptionData->type = type;
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
						  Uptr exceptionTypeId,
						  Uptr argsBits,
						  U32 isUserException)
{
	ExceptionType* exceptionType;
	{
		Compartment* compartment = getCompartmentRuntimeData(contextRuntimeData)->compartment;
		Lock<Platform::Mutex> compartmentLock(compartment->mutex);
		exceptionType = compartment->exceptionTypes[exceptionTypeId];
	}
	auto args = reinterpret_cast<const IR::UntaggedValue*>(Uptr(argsBits));

	ExceptionData* exceptionData
		= (ExceptionData*)malloc(ExceptionData::calcNumBytes(exceptionType->sig.params.size()));
	exceptionData->typeId = exceptionTypeId;
	exceptionData->type = exceptionType;
	exceptionData->isUserException = isUserException ? 1 : 0;
	memcpy(exceptionData->arguments,
		   args,
		   sizeof(IR::UntaggedValue) * exceptionType->sig.params.size());
	Platform::raisePlatformException(exceptionData);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "rethrowException",
						  void,
						  rethrowException,
						  Uptr exceptionBits)
{
	ExceptionData* exception = reinterpret_cast<ExceptionData*>(exceptionBits);
	Platform::raisePlatformException(exception);
}

static Exception translateExceptionDataToException(const ExceptionData* exceptionData,
												   const Platform::CallStack& callStack)
{
	ExceptionType* runtimeType = exceptionData->type;
	std::vector<IR::UntaggedValue> arguments(
		exceptionData->arguments,
		exceptionData->arguments + exceptionData->type->sig.params.size());
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
		Table* table = nullptr;
		Uptr tableIndex = 0;
		Memory* memory = nullptr;
		Uptr memoryAddress = 0;
		if(isAddressOwnedByTable(
			   reinterpret_cast<U8*>(signal.accessViolation.address), table, tableIndex))
		{
			outException = Exception{
				Exception::outOfBoundsTableAccessType, {table, U64(tableIndex)}, callStack};
			return true;
		}
		// If the access violation occured in a Memory's reserved pages, treat it as an access
		// violation runtime error.
		else if(isAddressOwnedByMemory(
					reinterpret_cast<U8*>(signal.accessViolation.address), memory, memoryAddress))
		{
			outException = Exception{
				Exception::outOfBoundsMemoryAccessType, {memory, U64(memoryAddress)}, callStack};
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

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
#include "WAVM/Platform/Signal.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/Runtime/RuntimeData.h"

using namespace WAVM;
using namespace WAVM::Runtime;

#define DEFINE_INTRINSIC_EXCEPTION_TYPE(name, ...)                                                 \
	ExceptionType* Runtime::ExceptionTypes::name = new ExceptionType(                              \
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

Exception* Runtime::createException(ExceptionType* type,
									const IR::UntaggedValue* arguments,
									Uptr numArguments,
									Platform::CallStack&& callStack)
{
	const IR::TypeTuple& params = type->sig.params;
	wavmAssert(numArguments == params.size());

	const bool isUserException = type->compartment != nullptr;
	Exception* exception = new(malloc(Exception::calcNumBytes(params.size())))
		Exception{type->id, type, isUserException ? U8(1) : U8(0), std::move(callStack)};
	if(params.size())
	{ memcpy(exception->arguments, arguments, sizeof(IR::UntaggedValue) * params.size()); }
	return exception;
}

void Runtime::destroyException(Exception* exception)
{
	exception->~Exception();
	free(exception);
}

ExceptionType* Runtime::getExceptionType(const Exception* exception) { return exception->type; }

IR::UntaggedValue Runtime::getExceptionArgument(const Exception* exception, Uptr argIndex)
{
	errorUnless(argIndex < exception->type->sig.params.size());
	return exception->arguments[argIndex];
}

std::string Runtime::describeException(const Exception* exception)
{
	std::string result = describeExceptionType(exception->type);
	if(exception->type == ExceptionTypes::outOfBoundsMemoryAccess)
	{
		Memory* memory = asMemoryNullable(exception->arguments[0].object);
		result += '(';
		result += memory ? memory->debugName : "<unknown memory>";
		result += '+';
		result += std::to_string(exception->arguments[1].u64);
		result += ')';
	}
	else if(exception->type == ExceptionTypes::outOfBoundsTableAccess
			|| exception->type == ExceptionTypes::uninitializedTableElement)
	{
		Table* table = asTableNullable(exception->arguments[0].object);
		result += '(';
		result += table ? table->debugName : "<unknown table>";
		result += '[';
		result += std::to_string(exception->arguments[1].u64);
		result += "])";
	}
	else if(exception->type->sig.params.size())
	{
		result += '(';
		for(Uptr argumentIndex = 0; argumentIndex < exception->type->sig.params.size();
			++argumentIndex)
		{
			if(argumentIndex != 0) { result += ", "; }
			result += asString(IR::Value(exception->type->sig.params[argumentIndex],
										 exception->arguments[argumentIndex]));
		}
		result += ')';
	}
	std::vector<std::string> callStackDescription = describeCallStack(exception->callStack);
	result += "\nCall stack:\n";
	for(auto calledFunction : callStackDescription)
	{
		result += "  ";
		result += calledFunction.c_str();
		result += '\n';
	}
	return result;
}

[[noreturn]] void Runtime::throwException(Exception* exception) { throw exception; }

[[noreturn]] void Runtime::createAndThrowException(ExceptionType* type,
												   const std::vector<IR::UntaggedValue>& arguments)
{
	wavmAssert(type->sig.params.size() == arguments.size());
	throwException(
		createException(type, arguments.data(), arguments.size(), Platform::captureCallStack(1)));
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "createException",
						  Uptr,
						  intrinsicCreateException,
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

	Exception* exception = createException(
		exceptionType, args, exceptionType->sig.params.size(), Platform::captureCallStack(1));

	return reinterpret_cast<Uptr>(exception);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "destroyException",
						  void,
						  intrinsicDestroyException,
						  Uptr exceptionBits)
{
	Exception* exception = reinterpret_cast<Exception*>(exceptionBits);
	destroyException(exception);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "throwException",
						  void,
						  intrinsicThrowException,
						  Uptr exceptionBits)
{
	Exception* exception = reinterpret_cast<Exception*>(exceptionBits);
	throw exception;
}

static bool translateSignalToRuntimeException(const Platform::Signal& signal,
											  Platform::CallStack&& callStack,
											  Runtime::Exception*& outException)
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
			IR::UntaggedValue exceptionArguments[2] = {table, U64(tableIndex)};
			outException = createException(ExceptionTypes::outOfBoundsTableAccess,
										   exceptionArguments,
										   2,
										   std::move(callStack));
			return true;
		}
		// If the access violation occured in a Memory's reserved pages, treat it as an
		// out-of-bounds memory access.
		else if(isAddressOwnedByMemory(
					reinterpret_cast<U8*>(signal.accessViolation.address), memory, memoryAddress))
		{
			IR::UntaggedValue exceptionArguments[2] = {memory, U64(memoryAddress)};
			outException = createException(ExceptionTypes::outOfBoundsMemoryAccess,
										   exceptionArguments,
										   2,
										   std::move(callStack));
			return true;
		}
		return false;
	}
	case Platform::Signal::Type::stackOverflow:
		outException
			= createException(ExceptionTypes::stackOverflow, nullptr, 0, std::move(callStack));
		return true;
	case Platform::Signal::Type::intDivideByZeroOrOverflow:
		outException = createException(
			ExceptionTypes::integerDivideByZeroOrOverflow, nullptr, 0, std::move(callStack));
		return true;
	default: Errors::unreachable();
	}
}

void Runtime::catchRuntimeExceptions(const std::function<void()>& thunk,
									 const std::function<void(Exception*)>& catchThunk)
{
	// Catch platform exceptions and translate them into C++ exceptions.
	try
	{
		Exception* exception = nullptr;
		if(Platform::catchSignals(
			   thunk,
			   [&exception](Platform::Signal signal, Platform::CallStack&& callStack) -> bool {
				   return translateSignalToRuntimeException(
					   signal, std::move(callStack), exception);
			   }))
		{ throw exception; }
	}
	catch(Exception* exception)
	{
		catchThunk(exception);
		destroyException(exception);
	}
}

void Runtime::unwindSignalsAsExceptions(const std::function<void()>& thunk)
{
	// Catch signals and translate them into runtime exceptions.
	Exception* exception = nullptr;
	if(Platform::catchSignals(
		   thunk, [&exception](Platform::Signal signal, Platform::CallStack&& callStack) -> bool {
			   return translateSignalToRuntimeException(signal, std::move(callStack), exception);
		   }))
	{ throw exception; }
}

static std::atomic<UnhandledExceptionHandler> unhandledExceptionHandler;

static void globalSignalHandler(Platform::Signal signal, Platform::CallStack&& callStack)
{
	Exception* exception;
	if(translateSignalToRuntimeException(signal, std::move(callStack), exception))
	{ (unhandledExceptionHandler.load())(exception); }
}

std::terminate_handler outerTerminateHandler;

static void terminateHandler()
{
	try
	{
		std::rethrow_exception(std::current_exception());
	}
	catch(Runtime::Exception* exception)
	{
		(unhandledExceptionHandler.load())(exception);
	}
	catch(...)
	{
		outerTerminateHandler();
	}
}

void Runtime::setUnhandledExceptionHandler(UnhandledExceptionHandler handler)
{
	unhandledExceptionHandler.store(handler);

	static struct SignalHandlerRegistrar
	{
		SignalHandlerRegistrar() { Platform::setSignalHandler(globalSignalHandler); }
	} signalHandlerRegistrar;

	static struct TerminateHandlerRegistrar
	{
		TerminateHandlerRegistrar()
		{
			outerTerminateHandler = std::get_terminate();
			std::set_terminate(terminateHandler);
		}
	} terminateHandlerRegistrar;
}

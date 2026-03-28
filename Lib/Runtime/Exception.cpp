#include <stdlib.h>
#include <string.h>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "RuntimePrivate.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Platform/Error.h"
#include "WAVM/Platform/RWMutex.h"
#include "WAVM/Platform/Signal.h"
#include "WAVM/Platform/Unwind.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"

using namespace WAVM;
using namespace WAVM::Runtime;

namespace WAVM { namespace Runtime {
	WAVM_DEFINE_INTRINSIC_MODULE(wavmIntrinsicsException)
}}

#define DEFINE_INTRINSIC_EXCEPTION_TYPE(name, ...)                                                 \
	ExceptionType* Runtime::ExceptionTypes::name = new ExceptionType(                              \
		nullptr, IR::ExceptionType{IR::TypeTuple({__VA_ARGS__})}, "wavm." #name);
WAVM_ENUM_INTRINSIC_EXCEPTION_TYPES(DEFINE_INTRINSIC_EXCEPTION_TYPE)
#undef DEFINE_INTRINSIC_EXCEPTION_TYPE

Runtime::Exception::~Exception()
{
	if(finalizeUserData) { (*finalizeUserData)(userData); }
}

void Runtime::setUserData(Exception* exception, void* userData, void (*finalizer)(void*))
{
	exception->userData = userData;
	exception->finalizeUserData = finalizer;
}
void* Runtime::getUserData(const Exception* exception) { return exception->userData; }

ExceptionType* Runtime::createExceptionType(Compartment* compartment,
											IR::ExceptionType sig,
											std::string&& debugName)
{
	auto exceptionType = new ExceptionType(compartment, sig, std::move(debugName));

	Platform::RWMutex::ExclusiveLock compartmentLock(compartment->mutex);
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

	Platform::RWMutex::ExclusiveLock compartmentLock(newCompartment->mutex);
	newCompartment->exceptionTypes.insertOrFail(exceptionType->id, newExceptionType);
	return newExceptionType;
}

Runtime::ExceptionType::~ExceptionType()
{
	if(id != UINTPTR_MAX)
	{
		WAVM_ASSERT_RWMUTEX_IS_EXCLUSIVELY_LOCKED_BY_CURRENT_THREAD(compartment->mutex);
		compartment->exceptionTypes.removeOrFail(id);
	}
}

std::string Runtime::describeExceptionType(const ExceptionType* type)
{
	WAVM_ASSERT(type);
	return type->debugName;
}

IR::TypeTuple Runtime::getExceptionTypeParameters(const ExceptionType* type)
{
	return type->sig.params;
}

Exception* Runtime::createException(ExceptionType* type,
									const IR::UntaggedValue* arguments,
									Uptr numArguments,
									CallStack&& callStack)
{
	const IR::TypeTuple& params = type->sig.params;
	WAVM_ASSERT(numArguments == params.size());

	const bool isUserException = type->compartment != nullptr;
	Exception* exception = new(malloc(Exception::calcNumBytes(params.size())))
		Exception(type->id, type, isUserException, std::move(callStack));
	if(params.size())
	{
		memcpy(exception->arguments, arguments, sizeof(IR::UntaggedValue) * params.size());
	}
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
	WAVM_ERROR_UNLESS(argIndex < exception->type->sig.params.size());
	return exception->arguments[argIndex];
}

const CallStack& Runtime::getExceptionCallStack(const Exception* exception)
{
	return exception->callStack;
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
	else if(exception->type == ExceptionTypes::indirectCallSignatureMismatch)
	{
		Function* function = exception->arguments[0].function;
		IR::FunctionType expectedSignature(
			IR::FunctionType::Encoding{Uptr(exception->arguments[1].u64)});
		result += '(';
		if(!function) { result += "<unknown function>"; }
		else
		{
			result += function->mutableData->debugName;
			result += " : ";
			result += asString(getFunctionType(function));
		}
		result += ", ";
		result += asString(expectedSignature);
		result += ')';
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
	std::vector<std::string> callStackDescription
		= Runtime::describeCallStack(exception->callStack);
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

[[noreturn]] WAVM_FORCENOINLINE void Runtime::throwException(
	ExceptionType* type,
	const std::vector<IR::UntaggedValue>& arguments,
	Uptr numOmittedFramesFromCaller)
{
	WAVM_ASSERT(type->sig.params.size() == arguments.size());
	// Skip throwException + caller's requested frames.
	Platform::UnwindState state = Platform::UnwindState::capture(1 + numOmittedFramesFromCaller);
	throwException(
		createException(type, arguments.data(), arguments.size(), Runtime::unwindCallStack(state)));
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsException,
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
		Platform::RWMutex::ExclusiveLock compartmentLock(compartment->mutex);
		exceptionType = compartment->exceptionTypes[exceptionTypeId];
	}
	auto args = reinterpret_cast<const IR::UntaggedValue*>(Uptr(argsBits));

	// Skip intrinsicCreateException.
	Platform::UnwindState state = Platform::UnwindState::capture(1);
	Exception* exception = createException(
		exceptionType, args, exceptionType->sig.params.size(), Runtime::unwindCallStack(state));

	return reinterpret_cast<Uptr>(exception);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsException,
							   "destroyException",
							   void,
							   intrinsicDestroyException,
							   Uptr exceptionBits)
{
	Exception* exception = reinterpret_cast<Exception*>(exceptionBits);
	destroyException(exception);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsException,
							   "throwException",
							   void,
							   intrinsicThrowException,
							   Uptr exceptionBits)
{
	Exception* exception = reinterpret_cast<Exception*>(exceptionBits);
	throw exception;
}

static bool isRuntimeException(const Platform::Signal& signal)
{
	switch(signal.type)
	{
	case Platform::Signal::Type::accessViolation: {
		// If the access violation occured in a Memory or Table's reserved pages, it's a runtime
		// exception.
		Table* table = nullptr;
		Uptr tableIndex = 0;
		Memory* memory = nullptr;
		Uptr memoryAddress = 0;
		U8* badPointer = reinterpret_cast<U8*>(signal.accessViolation.address);
		return isAddressOwnedByTable(badPointer, table, tableIndex)
			   || isAddressOwnedByMemory(badPointer, memory, memoryAddress);
	}
	case Platform::Signal::Type::stackOverflow:
	case Platform::Signal::Type::intDivideByZeroOrOverflow: return true;

	case Platform::Signal::Type::invalid:
	default: WAVM_UNREACHABLE();
	}
}

static void translateSignalToRuntimeException(const Platform::Signal& signal,
											  CallStack&& callStack,
											  Runtime::Exception*& outException)
{
	switch(signal.type)
	{
	case Platform::Signal::Type::accessViolation: {
		// If the access violation occured in a Table's reserved pages, treat it as an undefined
		// table element runtime error.
		Table* table = nullptr;
		Uptr tableIndex = 0;
		Memory* memory = nullptr;
		Uptr memoryAddress = 0;
		U8* const badPointer = reinterpret_cast<U8*>(signal.accessViolation.address);
		if(isAddressOwnedByTable(badPointer, table, tableIndex))
		{
			IR::UntaggedValue exceptionArguments[2] = {table, U64(tableIndex)};
			outException = createException(ExceptionTypes::outOfBoundsTableAccess,
										   exceptionArguments,
										   2,
										   std::move(callStack));
		}
		// If the access violation occured in a Memory's reserved pages, treat it as an
		// out-of-bounds memory access.
		else if(isAddressOwnedByMemory(badPointer, memory, memoryAddress))
		{
			IR::UntaggedValue exceptionArguments[2] = {memory, U64(memoryAddress)};
			outException = createException(ExceptionTypes::outOfBoundsMemoryAccess,
										   exceptionArguments,
										   2,
										   std::move(callStack));
		}
		break;
	}
	case Platform::Signal::Type::stackOverflow:
		outException
			= createException(ExceptionTypes::stackOverflow, nullptr, 0, std::move(callStack));
		break;
	case Platform::Signal::Type::intDivideByZeroOrOverflow:
		outException = createException(
			ExceptionTypes::integerDivideByZeroOrOverflow, nullptr, 0, std::move(callStack));
		break;

	case Platform::Signal::Type::invalid:
	default: WAVM_UNREACHABLE();
	}
}

void Runtime::catchRuntimeExceptions(const std::function<void()>& thunk,
									 const std::function<void(Exception*)>& catchThunk)
{
	try
	{
		unwindSignalsAsExceptions(thunk);
	}
	catch(Exception* exception)
	{
		catchThunk(exception);
	}
}

void Runtime::handleError(const char* message, bool printCallStack, Platform::UnwindState&& state)
{
	std::fprintf(stderr, "%s\n", message);
	if(printCallStack)
	{
		std::fprintf(stderr, "Call stack:\n");
		CallStack callStack = Runtime::unwindCallStack(state);
		std::vector<std::string> descriptions = Runtime::describeCallStack(callStack);
		for(const auto& desc : descriptions) { std::fprintf(stderr, "  %s\n", desc.c_str()); }
	}
	std::fflush(stderr);
}

void Runtime::unwindSignalsAsExceptions(const std::function<void()>& thunk)
{
	// Pre-create trace-unwind resources before entering the signal-catching scope.
	initTraceUnwindState();

	// Catch signals and translate them into runtime exceptions.
	struct UnwindContext
	{
		const std::function<void()>* thunk;
		Platform::Signal signal;
		CallStack callStack;
	} context;
	context.thunk = &thunk;
	if(Platform::catchSignals(
		   [](void* contextVoid) {
			   UnwindContext& context = *(UnwindContext*)contextVoid;
			   (*context.thunk)();
		   },
		   [](void* contextVoid, Platform::Signal signal, Platform::UnwindState&& state) {
			   if(!isRuntimeException(signal)) { return false; }
			   else
			   {
				   UnwindContext& context = *(UnwindContext*)contextVoid;
				   context.signal = signal;
				   context.callStack = Runtime::unwindCallStack(state);
				   return true;
			   }
		   },
		   &context))
	{
		Exception* exception = nullptr;
		translateSignalToRuntimeException(context.signal, std::move(context.callStack), exception);
		throw exception;
	}
}

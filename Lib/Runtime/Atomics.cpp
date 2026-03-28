#include <stdint.h>
#include <algorithm>
#include <atomic>
#include "RuntimePrivate.h"
#include "WAVM/IR/IR.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/I128.h"
#include "WAVM/Inline/Time.h"
#include "WAVM/Platform/Clock.h"
#include "WAVM/Platform/ConditionVariable.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/RuntimeABI/RuntimeABI.h"

using namespace WAVM;
using namespace WAVM::Runtime;

namespace WAVM { namespace Runtime {
	WAVM_DEFINE_INTRINSIC_MODULE(wavmIntrinsicsAtomics)
}}

// Holds a list of threads waiting on a specific address.
struct WaitList
{
	Platform::Mutex mutex;
	Platform::ConditionVariable condVar;
	U32 numWaiters{0};
	U32 pendingWakes{0};
	std::atomic<Uptr> numReferences;

	WaitList() : numReferences(1) {}
};

// A map from address to a list of threads waiting on that address.
static Platform::Mutex addressToWaitListMapMutex;
static HashMap<Uptr, WaitList*> addressToWaitListMap;

// Opens the wait list for a given address.
// Increases the wait list's reference count, and returns a pointer to it.
// Note that it does not lock the wait list mutex.
// A call to openWaitList should be followed by a call to closeWaitList to avoid leaks.
static WaitList* openWaitList(Uptr address)
{
	Platform::Mutex::Lock mapLock(addressToWaitListMapMutex);
	auto waitListPtr = addressToWaitListMap.get(address);
	if(waitListPtr)
	{
		++(*waitListPtr)->numReferences;
		return *waitListPtr;
	}
	else
	{
		WaitList* waitList = new WaitList();
		addressToWaitListMap.set(address, waitList);
		return waitList;
	}
}

// Closes a wait list, deleting it and removing it from the global map if it was the last reference.
static void closeWaitList(Uptr address, WaitList* waitList)
{
	if(--waitList->numReferences == 0)
	{
		Platform::Mutex::Lock mapLock(addressToWaitListMapMutex);
		if(!waitList->numReferences)
		{
			WAVM_ASSERT(!waitList->numWaiters);
			delete waitList;
			addressToWaitListMap.remove(address);
		}
	}
}

// Loads a value from memory with seq_cst memory order.
// The caller must ensure that the pointer is naturally aligned.
template<typename Value> static Value atomicLoad(const Value* valuePointer)
{
	static_assert(sizeof(std::atomic<Value>) == sizeof(Value), "relying on non-standard behavior");
	std::atomic<Value>* valuePointerAtomic = (std::atomic<Value>*)valuePointer;
	return valuePointerAtomic->load();
}

template<typename Value>
static U32 waitOnAddress(Value* valuePointer, Value expectedValue, I64 timeout)
{
	// Open the wait list for this address.
	const Uptr address = reinterpret_cast<Uptr>(valuePointer);
	WaitList* waitList = openWaitList(address);

	{
		Platform::Mutex::Lock waitListLock(waitList->mutex);

		// Use unwindSignalsAsExceptions to ensure that an access violation signal produced by the
		// load will be thrown as a Runtime::Exception and unwind the stack (e.g. the locks).
		Value value;
		Runtime::unwindSignalsAsExceptions(
			[valuePointer, &value] { value = atomicLoad(valuePointer); });

		if(value != expectedValue)
		{
			// If *valuePointer wasn't the expected value, unlock the wait list and return.
			waitListLock.unlock();
			closeWaitList(address, waitList);
			return 1;
		}

		++waitList->numWaiters;

		if(timeout < 0)
		{
			// Infinite wait: loop until a wake is pending, handling spurious wakeups.
			while(waitList->pendingWakes == 0)
			{
				waitList->condVar.wait(waitList->mutex, Time::infinity());
			}
		}
		else
		{
			// Timed wait: compute a deadline, then loop until a wake is pending or timed out.
			const I128 deadlineNS
				= Platform::getClockTime(Platform::Clock::monotonic).ns + I128(timeout);
			I128 remainingNS = I128(timeout);
			while(waitList->pendingWakes == 0 && remainingNS > 0
				  && waitList->condVar.wait(waitList->mutex, Time{remainingNS}))
			{
				remainingNS = deadlineNS - Platform::getClockTime(Platform::Clock::monotonic).ns;
			}
		}

		const bool wasWoken = waitList->pendingWakes > 0;
		if(wasWoken) { --waitList->pendingWakes; }
		--waitList->numWaiters;
		waitListLock.unlock();

		closeWaitList(address, waitList);
		return wasWoken ? 0 : 2;
	}
}

static U32 wakeAddress(void* pointer, U32 numToWake)
{
	if(numToWake == 0) { return 0; }

	// Open the wait list for this address.
	const Uptr address = reinterpret_cast<Uptr>(pointer);
	WaitList* waitList = openWaitList(address);
	Uptr actualNumToWake;
	{
		Platform::Mutex::Lock waitListLock(waitList->mutex);

		// Determine how many threads to wake.
		// Can only wake threads that aren't already pending a wake.
		const U32 wakeable = waitList->numWaiters > waitList->pendingWakes
								 ? waitList->numWaiters - waitList->pendingWakes
								 : 0;
		actualNumToWake = (numToWake == UINT32_MAX) ? wakeable : std::min(U32(numToWake), wakeable);

		waitList->pendingWakes += U32(actualNumToWake);

		if(actualNumToWake > 0)
		{
			if(actualNumToWake == waitList->numWaiters) { waitList->condVar.broadcast(); }
			else
			{
				for(Uptr i = 0; i < actualNumToWake; ++i) { waitList->condVar.signal(); }
			}
		}
	}
	closeWaitList(address, waitList);

	if(actualNumToWake > UINT32_MAX)
	{
		throwException(ExceptionTypes::integerDivideByZeroOrOverflow, {}, 2);
	}
	return U32(actualNumToWake);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsAtomics,
							   "misalignedAtomicTrap",
							   void,
							   misalignedAtomicTrap,
							   U64 address)
{
	throwException(ExceptionTypes::misalignedAtomicMemoryAccess, {address}, 1);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsAtomics,
							   "memory.atomic.notify",
							   I32,
							   atomic_notify,
							   Uptr address,
							   I32 numToWake,
							   Uptr memoryId)
{
	Memory* memory = getMemoryFromRuntimeData(contextRuntimeData, memoryId);

	// Validate that the address is within the memory's bounds.
	const U64 memoryNumBytes = U64(memory->numPages) * IR::numBytesPerPage;
	if(U64(address) + 4 > memoryNumBytes)
	{
		throwException(ExceptionTypes::outOfBoundsMemoryAccess, {memory, memoryNumBytes}, 1);
	}

	// The alignment check is done by the caller.
	WAVM_ASSERT(!(address & 3));

	return wakeAddress(memory->baseAddress + address, numToWake);
}

WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsAtomics,
							   "memory.atomic.wait32",
							   I32,
							   atomic_wait_I32,
							   Uptr address,
							   I32 expectedValue,
							   I64 timeout,
							   Uptr memoryId)
{
	Memory* memory = getMemoryFromRuntimeData(contextRuntimeData, memoryId);

	// Throw a waitOnUnsharedMemory exception if the memory is not shared.
	if(!memory->isShared) { throwException(ExceptionTypes::waitOnUnsharedMemory, {memory}, 1); }

	// Assume that the caller has validated the alignment.
	WAVM_ASSERT(!(address & 3));

	// Validate that the address is within the memory's bounds, and convert it to a pointer.
	I32* valuePointer = &memoryRef<I32>(memory, address);

	return waitOnAddress(valuePointer, expectedValue, timeout);
}
WAVM_DEFINE_INTRINSIC_FUNCTION(wavmIntrinsicsAtomics,
							   "memory.atomic.wait64",
							   I32,
							   atomic_wait_i64,
							   Uptr address,
							   I64 expectedValue,
							   I64 timeout,
							   Uptr memoryId)
{
	Memory* memory = getMemoryFromRuntimeData(contextRuntimeData, memoryId);

	// Throw a waitOnUnsharedMemory exception if the memory is not shared.
	if(!memory->isShared) { throwException(ExceptionTypes::waitOnUnsharedMemory, {memory}, 1); }

	// Assume that the caller has validated the alignment.
	WAVM_ASSERT(!(address & 7));

	// Validate that the address is within the memory's bounds, and convert it to a pointer.
	I64* valuePointer = &memoryRef<I64>(memory, address);

	return waitOnAddress(valuePointer, expectedValue, timeout);
}

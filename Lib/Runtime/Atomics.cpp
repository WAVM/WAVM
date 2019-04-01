#include <stdint.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "RuntimePrivate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/Platform/Clock.h"
#include "WAVM/Platform/Event.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/Runtime/RuntimeData.h"

using namespace WAVM;
using namespace WAVM::Runtime;

// Holds a list of threads (in the form of events that will wake them) that
// are waiting on a specific address.
struct WaitList
{
	Platform::Mutex mutex;
	std::vector<Platform::Event*> wakeEvents;
	std::atomic<Uptr> numReferences;

	WaitList() : numReferences(1) {}
};

// An event that is reused within a thread when it waits on a WaitList.
thread_local std::unique_ptr<Platform::Event> threadWakeEvent = nullptr;

// A map from address to a list of threads waiting on that address.
static Platform::Mutex addressToWaitListMapMutex;
static HashMap<Uptr, WaitList*> addressToWaitListMap;

// Opens the wait list for a given address.
// Increases the wait list's reference count, and returns a pointer to it.
// Note that it does not lock the wait list mutex.
// A call to openWaitList should be followed by a call to closeWaitList to avoid leaks.
static WaitList* openWaitList(Uptr address)
{
	Lock<Platform::Mutex> mapLock(addressToWaitListMapMutex);
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
		Lock<Platform::Mutex> mapLock(addressToWaitListMapMutex);
		if(!waitList->numReferences)
		{
			wavmAssert(!waitList->wakeEvents.size());
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

// Stores a value to memory with seq_cst memory order.
// The caller must ensure that the pointer is naturally aligned.
template<typename Value> static void atomicStore(Value* valuePointer, Value newValue)
{
	static_assert(sizeof(std::atomic<Value>) == sizeof(Value), "relying on non-standard behavior");
	std::atomic<Value>* valuePointerAtomic = (std::atomic<Value>*)valuePointer;
	valuePointerAtomic->store(newValue);
}

// Decodes a floating-point timeout value relative to startTime.
static U64 getEndTimeFromTimeout(U64 startTimeMicroseconds, I64 timeoutNanoseconds)
{
	U64 endTimeMicroseconds = UINT64_MAX;
	if(timeoutNanoseconds >= 0)
	{
		// Convert the timeout to microseconds, rounding up.
		const U64 timeoutMicroseconds = (U64(timeoutNanoseconds) + 999) / 1000;

		endTimeMicroseconds = startTimeMicroseconds + timeoutMicroseconds;
	}
	return endTimeMicroseconds;
}

template<typename Value>
static U32 waitOnAddress(Value* valuePointer, Value expectedValue, I64 timeout)
{
	const U64 endTime = getEndTimeFromTimeout(Platform::getMonotonicClock(), timeout);

	// Open the wait list for this address.
	const Uptr address = reinterpret_cast<Uptr>(valuePointer);
	WaitList* waitList = openWaitList(address);

	// Lock the wait list, and check that *valuePointer is still what the caller expected it to be.
	{
		Lock<Platform::Mutex> waitListLock(waitList->mutex);

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
		else
		{
			// If the thread hasn't yet created a wake event, do so.
			if(!threadWakeEvent)
			{ threadWakeEvent = std::unique_ptr<Platform::Event>(new Platform::Event()); }

			// Add the wake event to the wait list, and unlock the wait list.
			waitList->wakeEvents.push_back(threadWakeEvent.get());
			waitListLock.unlock();
		}
	}

	// Wait for the thread's wake event to be signaled.
	bool timedOut = false;
	if(!threadWakeEvent->wait(endTime))
	{
		// If the wait timed out, lock the wait list and check if the thread's wake event is still
		// in the wait list.
		Lock<Platform::Mutex> waitListLock(waitList->mutex);
		auto wakeEventIt = std::find(
			waitList->wakeEvents.begin(), waitList->wakeEvents.end(), threadWakeEvent.get());
		if(wakeEventIt != waitList->wakeEvents.end())
		{
			// If the event was still on the wait list, remove it, and return the "timed out"
			// result.
			waitList->wakeEvents.erase(wakeEventIt);
			timedOut = true;
		}
		else
		{
			// In between the wait timing out and locking the wait list, some other thread tried to
			// wake this thread. The event will now be signaled, so use an immediately expiring wait
			// on it to reset it.
			errorUnless(threadWakeEvent->wait(Platform::getMonotonicClock()));
		}
	}

	closeWaitList(address, waitList);
	return timedOut ? 2 : 0;
}

static U32 wakeAddress(void* pointer, U32 numToWake)
{
	if(numToWake == 0) { return 0; }

	// Open the wait list for this address.
	const Uptr address = reinterpret_cast<Uptr>(pointer);
	WaitList* waitList = openWaitList(address);
	Uptr actualNumToWake = numToWake;
	{
		Lock<Platform::Mutex> waitListLock(waitList->mutex);

		// Determine how many threads to wake.
		// numToWake==UINT32_MAX means wake all waiting threads.
		if(actualNumToWake == UINT32_MAX || actualNumToWake > waitList->wakeEvents.size())
		{ actualNumToWake = waitList->wakeEvents.size(); }

		// Signal the events corresponding to the oldest waiting threads.
		for(Uptr wakeIndex = 0; wakeIndex < actualNumToWake; ++wakeIndex)
		{ waitList->wakeEvents[wakeIndex]->signal(); }

		// Remove the events from the wait list.
		waitList->wakeEvents.erase(waitList->wakeEvents.begin(),
								   waitList->wakeEvents.begin() + actualNumToWake);
	}
	closeWaitList(address, waitList);

	if(actualNumToWake > UINT32_MAX)
	{ throwException(ExceptionTypes::integerDivideByZeroOrOverflow); }
	return U32(actualNumToWake);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "misalignedAtomicTrap",
						  void,
						  misalignedAtomicTrap,
						  U64 address)
{
	throwException(ExceptionTypes::misalignedAtomicMemoryAccess, {address});
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "atomic_notify",
						  I32,
						  atomic_notify,
						  U32 address,
						  I32 numToWake,
						  Uptr memoryId)
{
	Memory* memory = getMemoryFromRuntimeData(contextRuntimeData, memoryId);

	// Validate that the address is within the memory's bounds.
	const U64 memoryNumBytes = U64(memory->numPages) * IR::numBytesPerPage;
	if(U64(address) + 4 > memoryNumBytes)
	{ throwException(ExceptionTypes::outOfBoundsMemoryAccess, {memory, memoryNumBytes}); }

	// The alignment check is done by the caller.
	wavmAssert(!(address & 3));

	return wakeAddress(memory->baseAddress + address, numToWake);
}

DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "atomic_wait_i32",
						  I32,
						  atomic_wait_I32,
						  U32 address,
						  I32 expectedValue,
						  I64 timeout,
						  Uptr memoryId)
{
	Memory* memory = getMemoryFromRuntimeData(contextRuntimeData, memoryId);

	// Assume that the caller has validated the alignment.
	wavmAssert(!(address & 3));

	// Validate that the address is within the memory's bounds, and convert it to a pointer.
	I32* valuePointer = &memoryRef<I32>(memory, address);

	return waitOnAddress(valuePointer, expectedValue, timeout);
}
DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,
						  "atomic_wait_i64",
						  I32,
						  atomic_wait_i64,
						  I32 address,
						  I64 expectedValue,
						  I64 timeout,
						  Uptr memoryId)
{
	Memory* memory = getMemoryFromRuntimeData(contextRuntimeData, memoryId);

	// Assume that the caller has validated the alignment.
	wavmAssert(!(address & 7));

	// Validate that the address is within the memory's bounds, and convert it to a pointer.
	I64* valuePointer = &memoryRef<I64>(memory, address);

	return waitOnAddress(valuePointer, expectedValue, timeout);
}

void Runtime::dummyReferenceAtomics()
{
	// This is just here make sure the static initializers for the intrinsic function registrations
	// aren't removed as dead code.
}

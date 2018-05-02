#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Logging/Logging.h"
#include "Intrinsics.h"
#include "RuntimePrivate.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

// Holds a list of threads (in the form of events that will wake them) that
// are waiting on a specific address.
struct WaitList
{
	Platform::Mutex mutex;
	std::vector<Platform::Event*> wakeEvents;
	std::atomic<Uptr> numReferences;

	WaitList(): numReferences(1) {}
};

// Define a unique_ptr to a Platform::Event.
struct EventDeleter
{
	void operator()(Platform::Event* event) { Platform::destroyEvent(event); }
};
typedef std::unique_ptr<Platform::Event,EventDeleter> UniqueEventPtr;

// An event that is reused within a thread when it waits on a WaitList.
thread_local UniqueEventPtr threadWakeEvent = nullptr;

// A map from address to a list of threads waiting on that address.
static std::map<Uptr,WaitList*> addressToWaitListMap;
static Platform::Mutex addressToWaitListMapMutex;

// Opens the wait list for a given address.
// Increases the wait list's reference count, and returns a pointer to it.
// Note that it does not lock the wait list mutex.
// A call to openWaitList should be followed by a call to closeWaitList to avoid leaks.
static WaitList* openWaitList(Uptr address)
{
	Platform::Lock mapLock(addressToWaitListMapMutex);
	auto mapIt = addressToWaitListMap.find(address);
	if(mapIt != addressToWaitListMap.end())
	{
		++mapIt->second->numReferences;
		return mapIt->second;
	}
	else
	{
		WaitList* waitList = new WaitList();
		addressToWaitListMap.emplace(address,waitList);
		return waitList;
	}
}

// Closes a wait list, deleting it and removing it from the global map if it was the last reference.
static void closeWaitList(Uptr address,WaitList* waitList)
{
	if(--waitList->numReferences == 0)
	{
		Platform::Lock mapLock(addressToWaitListMapMutex);
		if(!waitList->numReferences)
		{
			wavmAssert(!waitList->wakeEvents.size());
			delete waitList;
			addressToWaitListMap.erase(address);
		}
	}
}

// Loads a value from memory with seq_cst memory order.
// The caller must ensure that the pointer is naturally aligned.
template<typename Value>
static Value atomicLoad(const Value* valuePointer)
{
	static_assert(sizeof(std::atomic<Value>) == sizeof(Value),"relying on non-standard behavior");
	std::atomic<Value>* valuePointerAtomic = (std::atomic<Value>*)valuePointer;
	return valuePointerAtomic->load();
}

// Stores a value to memory with seq_cst memory order.
// The caller must ensure that the pointer is naturally aligned.
template<typename Value>
static void atomicStore(Value* valuePointer,Value newValue)
{
	static_assert(sizeof(std::atomic<Value>) == sizeof(Value),"relying on non-standard behavior");
	std::atomic<Value>* valuePointerAtomic = (std::atomic<Value>*)valuePointer;
	valuePointerAtomic->store(newValue);
}

// Decodes a floating-point timeout value relative to startTime.
static U64 getEndTimeFromTimeout(U64 startTime,F64 timeout)
{
	const F64 timeoutMicroseconds = timeout * 1000.0;
	U64 endTime = UINT64_MAX;
	if(!std::isnan(timeoutMicroseconds) && std::isfinite(timeoutMicroseconds))
	{
		if(timeoutMicroseconds <= 0.0)
		{
			endTime = startTime;
		}
		else if(timeoutMicroseconds <= F64(UINT64_MAX - 1))
		{
			endTime = startTime + U64(timeoutMicroseconds);
			errorUnless(endTime >= startTime);
		}
	}
	return endTime;
}

template<typename Value>
static U32 waitOnAddress(Value* valuePointer,Value expectedValue,F64 timeout)
{
	const U64 endTime = getEndTimeFromTimeout(Platform::getMonotonicClock(),timeout);

	// Open the wait list for this address.
	const Uptr address = reinterpret_cast<Uptr>(valuePointer);
	WaitList* waitList = openWaitList(address);

	// Lock the wait list, and check that *valuePointer is still what the caller expected it to be.
	{
		Platform::Lock waitListLock(waitList->mutex);
		if(atomicLoad(valuePointer) != expectedValue)
		{
			// If *valuePointer wasn't the expected value, unlock the wait list and return.
			waitListLock.unlock();
			closeWaitList(address,waitList);
			return 1;
		}
		else
		{
			// If the thread hasn't yet created a wake event, do so.
			if(!threadWakeEvent) { threadWakeEvent = UniqueEventPtr(Platform::createEvent()); }

			// Add the wake event to the wait list, and unlock the wait list.
			waitList->wakeEvents.push_back(threadWakeEvent.get());
			waitListLock.unlock();
		}
	}

	// Wait for the thread's wake event to be signaled.
	bool timedOut = false;
	if(!Platform::waitForEvent(threadWakeEvent.get(),endTime))
	{
		// If the wait timed out, lock the wait list and check if the thread's wake event is still in the wait list.
		Platform::Lock waitListLock(waitList->mutex);
		auto wakeEventIt = std::find(waitList->wakeEvents.begin(),waitList->wakeEvents.end(),threadWakeEvent.get());
		if(wakeEventIt != waitList->wakeEvents.end())
		{
			// If the event was still on the wait list, remove it, and return the "timed out" result.
			waitList->wakeEvents.erase(wakeEventIt);
			timedOut = true;
		}
		else
		{
			// In between the wait timing out and locking the wait list, some other thread tried to wake this thread.
			// The event will now be signaled, so use an immediately expiring wait on it to reset it.
			errorUnless(Platform::waitForEvent(threadWakeEvent.get(),Platform::getMonotonicClock()));
		}
	}

	closeWaitList(address,waitList);
	return timedOut ? 2 : 0;
}

static U32 wakeAddress(Uptr address,U32 numToWake)
{
	if(numToWake == 0) { return 0; }

	// Open the wait list for this address.
	WaitList* waitList = openWaitList(address);
	Uptr actualNumToWake = numToWake;
	{
		Platform::Lock waitListLock(waitList->mutex);

		// Determine how many threads to wake.
		// numToWake==UINT32_MAX means wake all waiting threads.
		if(actualNumToWake == UINT32_MAX || actualNumToWake > waitList->wakeEvents.size())
		{
			actualNumToWake = waitList->wakeEvents.size();
		}

		// Signal the events corresponding to the oldest waiting threads.
		for(Uptr wakeIndex = 0;wakeIndex < actualNumToWake;++wakeIndex)
		{
			signalEvent(waitList->wakeEvents[wakeIndex]);
		}

		// Remove the events from the wait list.
		waitList->wakeEvents.erase(waitList->wakeEvents.begin(),waitList->wakeEvents.begin() + actualNumToWake);
	}
	closeWaitList(address,waitList);

	if(actualNumToWake > UINT32_MAX)
	{
		Runtime::throwException(Runtime::Exception::integerDivideByZeroOrIntegerOverflowType);
	}
	return U32(actualNumToWake);
}

namespace Runtime
{
	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavmIntrinsics,"misalignedAtomicTrap",
		void,misalignedAtomicTrap,I32 address)
	{
		throwException(Exception::misalignedAtomicMemoryAccessType);
	}

	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavmIntrinsics,"atomic_wake",I32,atomic_wake,
		I32 addressOffset,I32 numToWake,I64 memoryId)
	{
		MemoryInstance* memoryInstance = getMemoryFromRuntimeData(contextRuntimeData,memoryId);

		// Validate that the address is within the memory's bounds and 4-byte aligned.
		if(U32(addressOffset) > memoryInstance->endOffset) { throwException(Exception::accessViolationType); }
		if(addressOffset & 3) { throwException(Exception::misalignedAtomicMemoryAccessType); }

		const Uptr address = reinterpret_cast<Uptr>(&memoryRef<U8>(memoryInstance,addressOffset));
		return wakeAddress(address,numToWake);
	}

	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavmIntrinsics,"atomic_wait_i32",I32,atomic_wait_I32,
		I32 addressOffset, I32 expectedValue, F64 timeout, I64 memoryId)
	{
		MemoryInstance* memoryInstance = getMemoryFromRuntimeData(contextRuntimeData,memoryId);

		// Validate that the address is within the memory's bounds and naturally aligned.
		if(U32(addressOffset) > memoryInstance->endOffset) { throwException(Exception::accessViolationType); }
		if(addressOffset & 3) { throwException(Exception::misalignedAtomicMemoryAccessType); }

		I32* valuePointer = &memoryRef<I32>(memoryInstance,addressOffset);
		return waitOnAddress(valuePointer,expectedValue,timeout);
	}
	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(wavmIntrinsics,"atomic_wait_i64",I32,atomic_wait_i64,
		I32 addressOffset, I64 expectedValue, F64 timeout, I64 memoryId)
	{
		MemoryInstance* memoryInstance = getMemoryFromRuntimeData(contextRuntimeData,memoryId);

		// Validate that the address is within the memory's bounds and naturally aligned.
		if(U32(addressOffset) > memoryInstance->endOffset) { throwException(Exception::accessViolationType); }
		if(addressOffset & 7) { throwException(Exception::misalignedAtomicMemoryAccessType); }

		I64* valuePointer = &memoryRef<I64>(memoryInstance,addressOffset);
		return waitOnAddress(valuePointer,expectedValue,timeout);
	}

	void dummyReferenceAtomics()
	{
		// This is just here make sure the static initializers for the intrinsic function registrations aren't removed
		// as dead code.
	}
}

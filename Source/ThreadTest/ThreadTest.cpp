#include "Inline/Assert.h"
#include "Inline/IntrusiveSharedPtr.h"
#include "IR/Types.h"
#include "Platform/Platform.h"
#include "Runtime/Intrinsics.h"
#include "Runtime/Runtime.h"
#include "ThreadTest/ThreadTest.h"

#include <atomic>

using namespace IR;
using namespace Runtime;

namespace ThreadTest
{
	enum { numStackBytes = 1 * 1024 * 1024 };
	
	// Keeps track of the entry and error functions used by a running WebAssembly-spawned thread.
	// Used to find garbage collection roots.
	struct Thread
	{
		Uptr id = UINTPTR_MAX;
		std::atomic<Uptr> numRefs {0};

		Platform::Thread* platformThread = nullptr;
		Runtime::GCPointer<Runtime::Context> context;
		Runtime::GCPointer<Runtime::FunctionInstance> entryFunction;

		Runtime::Value argument;

		FORCENOINLINE Thread(
			Runtime::Context* inContext,
			Runtime::FunctionInstance* inEntryFunction,
			const Runtime::Value& inArgument)
		: context(inContext)
		, entryFunction(inEntryFunction)
		, argument(inArgument)
		{}

		void addRef(Uptr delta = 1) { numRefs += delta; }
		void removeRef()
		{
			if(--numRefs == 0)
			{
				delete this;
			}
		}
	};

	// A global list of running threads created by WebAssembly code.
	// Will always contain a null pointer in the first element so that 0 won't be allocated as a thread ID.
	static Platform::Mutex threadsMutex;
	static std::vector<IntrusiveSharedPtr<Thread>> threads = {{nullptr}};
	static std::vector<Uptr> freeThreadIds;

	// A shared pointer to the current thread.
	// This is used to decrement the thread's reference count when the thread exits.
	thread_local IntrusiveSharedPtr<Thread> currentThread = nullptr;

	// Adds the thread to the global thread array, assigning it an ID corresponding to its index in the array.
	FORCENOINLINE static Uptr allocateThreadId(Thread* thread)
	{
		Platform::Lock threadsLock(threadsMutex);
		wavmAssert(threads.size() > 0);
		if(freeThreadIds.size())
		{
			thread->id = freeThreadIds.back();
			freeThreadIds.pop_back();

			wavmAssert(threads[thread->id] == nullptr);
			threads[thread->id] = thread;

			return thread->id;
		}
		else
		{
			thread->id = threads.size();
			threads.push_back(thread);

			return thread->id;
		}
	}

	// This function is just to provide a way to write to the currentThread thread-local variable in a way that the
	// compiler can't cache across a call to Platform::forkCurrentThread.
	FORCENOINLINE static void setCurrentThread(Thread* thread)
	{
		currentThread = thread;
	}

	// Validates that a thread ID is valid. i.e. 0 < threadId < threads.size(), and threads[threadId] != null
	// If the thread ID is invalid, throws an invalid argument exception.
	// The caller must have already locked threadsMutex before calling validateThreadId.
	static void validateThreadId(Uptr threadId)
	{
		if(threadId == 0
		|| threadId >= threads.size()
		|| !threads[threadId])
		{
			throwException(Exception::invalidArgumentType);
		}
	}

	DEFINE_INTRINSIC_MODULE(threadTest);

	static I64 threadEntry(Thread* thread)
	{
		currentThread = thread;
		thread->removeRef();

		return invokeFunctionUnchecked(thread->context,thread->entryFunction,&thread->argument)->i64;
	}

	DEFINE_INTRINSIC_FUNCTION_WITH_MEM_AND_TABLE(threadTest,"createThread",I64,createThread,
		I32 entryFunctionIndex,
		I32 entryArgument)
	{
		if(defaultTableId.id == UINT32_MAX)
		{
			// If createThread is called from a module that doesn't handle a default table, throw an exception.
			throwException(Exception::undefinedTableElementType);
		}

		// Look up the index provided in the default table to get the thread entry function.
		TableInstance* defaultTable = getTableFromRuntimeData(contextRuntimeData,defaultTableId.id);
		FunctionInstance* entryFunction = asFunctionNullable(Runtime::getTableElement(defaultTable,entryFunctionIndex));

		// Validate that the entry function wasn't null, and it has the correct type (i32)->i64
		if(!entryFunction) { throwException(Runtime::Exception::undefinedTableElementType); }
		else if(Runtime::getFunctionType(entryFunction) != FunctionType::get(ResultType::i64,{ValueType::i32}))
		{
			throwException(Runtime::Exception::indirectCallSignatureMismatchType);
		}

		// Create a thread object that will expose its entry and error functions to the garbage collector as roots.
		auto newContext = createContext(getCompartmentFromContext(getContextFromRuntimeData(contextRuntimeData)));
		Thread* thread = new Thread(newContext,entryFunction,entryArgument);

		allocateThreadId(thread);

		// Increment the Thread's reference count for the pointer passed to the thread's entry function.
		// threadFunc calls the corresponding removeRef.
		thread->addRef();

		// Spawn and detach a platform thread that calls threadFunc.
		thread->platformThread = Platform::createThread(numStackBytes,(I64(*)(void*))threadEntry,thread);

		return thread->id;
	}
	
	DEFINE_INTRINSIC_FUNCTION_WITH_CONTEXT_SWITCH(threadTest,"forkThread",I64,forkThread)
	{
		auto oldContext = getContextFromRuntimeData(contextRuntimeData);
		auto compartment = getCompartmentFromContext(oldContext);
		auto newContext = cloneContext(oldContext,compartment);

		wavmAssert(currentThread);
		Thread* childThread = new Thread(newContext,currentThread->entryFunction,currentThread->argument);

		// Increment the Thread's reference count twice to account for the reference to the Thread on the stack which
		// is about to be forked. Each fork calls removeRef separately below.
		childThread->addRef(2);

		Platform::Thread* platformThread = Platform::forkCurrentThread();
		if(platformThread)
		{
			// Initialize the child thread's platform thread pointer, and allocate a thread ID for it.
			childThread->platformThread = platformThread;
			const Uptr threadId = allocateThreadId(childThread);
			childThread->removeRef();

			return Intrinsics::resultInContextRuntimeData<I64>(contextRuntimeData,threadId);
		}
		else
		{
			// Move the childThread pointer into the thread-local currentThread variable.
			// Since some compilers will cache a pointer to thread-local data that's accessed multiple times in one
			// function, and currentThread is accessed before calling forkCurrentThread, we can't directly write to it
			// in this function in case the compiler tries to write to the original thread's currentThread variable.
			// Instead, call a FORCENOINLINE function (setCurrentThread) to set the variable.
			setCurrentThread(childThread);
			childThread->removeRef();
			childThread = nullptr;

			// Switch the contextRuntimeData to point to the new context's runtime data.
			contextRuntimeData = getContextRuntimeData(newContext);

			return Intrinsics::resultInContextRuntimeData<I64>(contextRuntimeData,0);
		}
	}

	DEFINE_INTRINSIC_FUNCTION(threadTest,"exitThread",void,exitThread,I64 code)
	{
		Platform::exitThread(code);
		Errors::unreachable();
	}

	// Validates a thread ID, removes the corresponding thread from the threads array, and returns it.
	static IntrusiveSharedPtr<Thread> removeThreadById(Uptr threadId)
	{
		IntrusiveSharedPtr<Thread> thread;

		Platform::Lock threadsLock(threadsMutex);
		validateThreadId(threadId);
		thread = std::move(threads[threadId]);
		threads[threadId] = nullptr;
		freeThreadIds.push_back(threadId);

		wavmAssert(thread->id == Uptr(threadId));
		thread->id = UINTPTR_MAX;

		return thread;
	}

	DEFINE_INTRINSIC_FUNCTION(threadTest,"joinThread",I64,joinThread,I64 threadId)
	{
		IntrusiveSharedPtr<Thread> thread = removeThreadById(threadId);
		const I64 result = Platform::joinThread(thread->platformThread);
		thread->platformThread = nullptr;
		return result;
	}

	DEFINE_INTRINSIC_FUNCTION(threadTest,"detachThread",void,detachThread,I64 threadId)
	{
		IntrusiveSharedPtr<Thread> thread = removeThreadById(threadId);
		Platform::detachThread(thread->platformThread);
		thread->platformThread = nullptr;
	}

	ModuleInstance* instantiate(Compartment* compartment)
	{
		return Intrinsics::instantiateModule(compartment,INTRINSIC_MODULE_REF(threadTest),"threadTest");
	}
}

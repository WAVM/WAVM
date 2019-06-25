#include <pthread.h>

#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Platform/Mutex.h"

using namespace WAVM;
using namespace WAVM::Platform;

Platform::Mutex::Mutex()
{
	static_assert(sizeof(pthreadMutex) == sizeof(pthread_mutex_t), "");
	static_assert(alignof(PthreadMutex) >= alignof(pthread_mutex_t), "");

#if WAVM_DEBUG || WAVM_ENABLE_RELEASE_ASSERTS
	// Use a recursive mutex in debug builds to support isLockedByCurrentThread.
	pthread_mutexattr_t pthreadMutexAttr;
	errorUnless(!pthread_mutexattr_init(&pthreadMutexAttr));
	errorUnless(!pthread_mutexattr_settype(&pthreadMutexAttr, PTHREAD_MUTEX_RECURSIVE));
	errorUnless(!pthread_mutex_init((pthread_mutex_t*)&pthreadMutex, &pthreadMutexAttr));
	errorUnless(!pthread_mutexattr_destroy(&pthreadMutexAttr));
	isLocked = false;
#else
	errorUnless(!pthread_mutex_init((pthread_mutex_t*)&pthreadMutex, nullptr));
#endif
}

Platform::Mutex::~Mutex() { errorUnless(!pthread_mutex_destroy((pthread_mutex_t*)&pthreadMutex)); }

void Platform::Mutex::lock()
{
	errorUnless(!pthread_mutex_lock((pthread_mutex_t*)&pthreadMutex));
#if WAVM_DEBUG || WAVM_ENABLE_RELEASE_ASSERTS
	if(isLocked) { Errors::fatal("Recursive mutex lock"); }
	isLocked = true;
#endif
}

void Platform::Mutex::unlock()
{
#if WAVM_DEBUG || WAVM_ENABLE_RELEASE_ASSERTS
	isLocked = false;
#endif
	errorUnless(!pthread_mutex_unlock((pthread_mutex_t*)&pthreadMutex));
}

#if WAVM_DEBUG || WAVM_ENABLE_RELEASE_ASSERTS
bool Platform::Mutex::isLockedByCurrentThread()
{
	// Try to lock the mutex, and if EDEADLK is returned, it means the mutex was already locked by
	// this thread.
	int tryLockResult = pthread_mutex_trylock((pthread_mutex_t*)&pthreadMutex);
	if(tryLockResult) { return false; }

	const bool result = isLocked;
	errorUnless(!pthread_mutex_unlock((pthread_mutex_t*)&pthreadMutex));
	return result;
}
#endif

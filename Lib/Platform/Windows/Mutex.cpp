#include "WAVM/Platform/Mutex.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Platform/Defines.h"

#define NOMINMAX
#include <Windows.h>

using namespace WAVM;
using namespace WAVM::Platform;

Platform::Mutex::Mutex()
{
	static_assert(sizeof(criticalSection) == sizeof(CRITICAL_SECTION), "");
	static_assert(alignof(CriticalSection) >= alignof(CRITICAL_SECTION), "");
	InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)&criticalSection, 4000);
	isLocked = false;
}

Platform::Mutex::~Mutex()
{
	if(!TryEnterCriticalSection((CRITICAL_SECTION*)&criticalSection) || isLocked)
	{ Errors::fatal("Destroying mutex that was locked"); }
	DeleteCriticalSection((CRITICAL_SECTION*)&criticalSection);
}

void Platform::Mutex::lock()
{
	EnterCriticalSection((CRITICAL_SECTION*)&criticalSection);
	if(isLocked) { Errors::fatal("Recursive mutex lock"); }
	isLocked = true;
}

void Platform::Mutex::unlock()
{
	isLocked = false;
	LeaveCriticalSection((CRITICAL_SECTION*)&criticalSection);
}

#if WAVM_DEBUG || WAVM_ENABLE_RELEASE_ASSERTS
bool Platform::Mutex::isLockedByCurrentThread()
{
	// Windows critical sections are recursive, so TryEnterCriticalSection on a mutex already locked
	// by the current thread will succeed, and then the isLocked flag can be used to determine
	// whether the mutex was not locked or recursively locked.
	BOOL entered = TryEnterCriticalSection((CRITICAL_SECTION*)&criticalSection);
	if(!entered) { return false; }

	const bool result = isLocked;
	LeaveCriticalSection((CRITICAL_SECTION*)&criticalSection);
	return result;
}
#endif

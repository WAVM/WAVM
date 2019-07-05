#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/Clock.h"
#include "WAVM/Platform/Event.h"

#define NOMINMAX
#include <Windows.h>

using namespace WAVM;
using namespace WAVM::Platform;

Platform::Event::Event()
{
	handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	errorUnless(handle);
}

Platform::Event::~Event() { errorUnless(CloseHandle(handle)); }

bool Platform::Event::wait(I128 waitDuration)
{
	if(waitDuration == I128::nan())
	{
		const U32 waitResult = WaitForSingleObject(handle, INFINITE);
		errorUnless(waitResult == WAIT_OBJECT_0);
		return true;
	}
	else
	{
		I128 currentTime = getMonotonicClock();
		const I128 untilTime = currentTime + waitDuration;
		while(true)
		{
			const I128 durationMS
				= currentTime > untilTime ? 0 : (untilTime - currentTime) / 1000000;
			const U32 durationMS32
				= durationMS <= 0 ? 0
								  : durationMS >= UINT32_MAX ? (UINT32_MAX - 1) : U32(durationMS);

			const U32 waitResult = WaitForSingleObject(handle, durationMS32);
			if(waitResult != WAIT_TIMEOUT)
			{
				errorUnless(waitResult == WAIT_OBJECT_0);
				return true;
			}
			else
			{
				currentTime = getMonotonicClock();
				if(currentTime >= untilTime) { return false; }
			}
		};
	}
}

void Platform::Event::signal() { errorUnless(SetEvent(handle)); }

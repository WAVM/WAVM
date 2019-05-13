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

bool Platform::Event::wait(I128 untilTime)
{
	I128 currentTime = getMonotonicClock();
	while(true)
	{
		const I128 timeout128 = currentTime > untilTime ? 0 : (untilTime - currentTime) / 1000000;
		const U32 timeout
			= timeout128 <= 0 ? 0 : timeout128 > UINT32_MAX ? UINT32_MAX : U32(timeout128);

		const U32 waitResult = WaitForSingleObject(handle, timeout);
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

void Platform::Event::signal() { errorUnless(SetEvent(handle)); }

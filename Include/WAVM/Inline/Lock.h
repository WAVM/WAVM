#pragma once

namespace WAVM {
	// RAII-style lock.
	template<typename Mutex> struct Lock
	{
		Lock(Mutex& inMutex) : mutex(&inMutex) { mutex->lock(); }
		~Lock() { unlock(); }

		void unlock()
		{
			if(mutex)
			{
				mutex->unlock();
				mutex = nullptr;
			}
		}

	private:
		Mutex* mutex;
	};
}

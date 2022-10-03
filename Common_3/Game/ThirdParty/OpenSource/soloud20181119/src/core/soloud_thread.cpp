/*
SoLoud audio engine
Copyright (c) 2013-2015 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include "soloud.h"
#include "../../../../../ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "soloud_thread.h"

namespace SoLoud {
namespace Thread {

void* createMutex()
{
	Mutex* m = tf_new(Mutex);
	::initMutex(m);
	return (void*)m;
}

void destroyMutex(void* aHandle)
{
	Mutex* m = (Mutex*)aHandle;
	::destroyMutex(m);
	tf_delete(m);
}

void lockMutex(void* aHandle)
{
	Mutex* m = (Mutex*)aHandle;
	if (m)
	{
		::acquireMutex(m);
	}
}

void unlockMutex(void* aHandle)
{
	Mutex* m = (Mutex*)aHandle;
	if (m)
	{
		::releaseMutex(m);
	}
}

ThreadHandle createThread(ThreadFunction aThreadFunction, void* aParameter)
{
	ThreadDesc threadDesc = {};
	threadDesc.pFunc = aThreadFunction;
	threadDesc.pData = aParameter;
	strncpy(threadDesc.mThreadName, "Soloud", sizeof(threadDesc.mThreadName));
	ThreadHandle ret;
	::initThread(&threadDesc, &ret);
	return ret;
}

void sleep(int aMSec) { threadSleep((unsigned)aMSec); }

void release(ThreadHandle aThreadHandle)
{
	::joinThread(aThreadHandle);
}
}    // namespace Thread
}    // namespace SoLoud

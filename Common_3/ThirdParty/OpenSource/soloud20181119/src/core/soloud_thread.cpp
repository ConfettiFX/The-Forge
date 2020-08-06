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

#if defined(_WIN32)||defined(_WIN64)
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include "soloud.h"
#include "../../../../../ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "soloud_thread.h"

namespace SoLoud
{
	namespace Thread
	{
		eastl::unordered_map<ThreadHandle, ThreadDesc*> gThreadDataToCleanup;

		void * createMutex()
		{
			Mutex* m = tf_new(Mutex);
			m->Init();
			return (void*)m;
		}

		void destroyMutex(void *aHandle)
		{
			Mutex* m = (Mutex*)aHandle;
			m->Destroy();
			tf_delete(m);
		}

		void lockMutex(void *aHandle)
		{
			Mutex* m = (Mutex*)aHandle;
			if (m)
			{
				m->Acquire();
			}
		}

		void unlockMutex(void *aHandle)
		{
			Mutex* m = (Mutex*)aHandle;
			if (m)
			{
				m->Release();
			}
		}

        ThreadHandle createThread(ThreadFunction aThreadFunction, void *aParameter)
		{
			ThreadDesc* td = tf_new(ThreadDesc);
			td->pFunc = aThreadFunction;
			td->pData = aParameter;
			ThreadHandle ret = create_thread(td);
			gThreadDataToCleanup[ret] = td;
			return ret;
		}

		void sleep(int aMSec)
		{
			::Thread::Sleep((unsigned)aMSec);
		}		      

        void release(ThreadHandle aThreadHandle)
        {
			eastl::unordered_map<ThreadHandle, ThreadDesc*>::iterator it = gThreadDataToCleanup.find(aThreadHandle);
			ASSERT(it != gThreadDataToCleanup.end());
			destroy_thread(aThreadHandle);
			tf_delete(it->second);
			gThreadDataToCleanup.erase(it);
			if (gThreadDataToCleanup.empty())
				gThreadDataToCleanup.clear(true);
        }		
	}
}

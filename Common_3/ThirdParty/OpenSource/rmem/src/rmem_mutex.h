/*
 * Copyright (c) 2019 by Milos Tosic. All Rights Reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef RMEM_MUTEX_H
#define RMEM_MUTEX_H

#include "rmem_platform.h"

namespace rmem {

#if RMEM_PLATFORM_WINDOWS || RMEM_PLATFORM_XBOX360 || RMEM_PLATFORM_XBOXONE
	typedef CRITICAL_SECTION mtuner_mutex;

	static inline void mtuner_mutex_init(mtuner_mutex* _mutex) {
		InitializeCriticalSection(_mutex);
	}

	static inline void mtuner_mutex_destroy(mtuner_mutex* _mutex) {
		DeleteCriticalSection(_mutex);
	}

	static inline void mtuner_mutex_lock(mtuner_mutex* _mutex) {
		EnterCriticalSection(_mutex);
	}

	static inline int mtuner_mutex_trylock(mtuner_mutex* _mutex)	{
		return TryEnterCriticalSection(_mutex) ? 0 : 1;
	}

	static inline void mtuner_mutex_unlock(mtuner_mutex* _mutex)	{
		LeaveCriticalSection(_mutex);
	}

#elif RMEM_PLATFORM_LINUX || RMEM_PLATFORM_OSX || RMEM_PLATFORM_ANDROID
	typedef pthread_mutex_t mtuner_mutex;

	static inline void mtuner_mutex_init(mtuner_mutex* _mutex) {
		pthread_mutex_init(_mutex, NULL);
	}

	static inline void mtuner_mutex_destroy(mtuner_mutex* _mutex) {
		pthread_mutex_destroy(_mutex);
	}

	static inline void mtuner_mutex_lock(mtuner_mutex* _mutex) {
		pthread_mutex_lock(_mutex);
	}

	static inline int mtuner_mutex_trylock(mtuner_mutex* _mutex) {
		return pthread_mutex_trylock(_mutex);
	}

	static inline void mtuner_mutex_unlock(mtuner_mutex* _mutex) {
		pthread_mutex_unlock(_mutex);
	}
	
#elif RMEM_PLATFORM_PS3
	typedef sys_lwmutex_t mtuner_mutex;

	static inline void mtuner_mutex_init(mtuner_mutex* _mutex) {
		sys_lwmutex_attribute_t	mutexAttr;
		sys_lwmutex_attribute_initialize(mutexAttr);
		mutexAttr.attr_recursive = SYS_SYNC_RECURSIVE;
		sys_lwmutex_create(_mutex, &mutexAttr);
	}

	static inline void mtuner_mutex_destroy(mtuner_mutex* _mutex) {
		sys_lwmutex_destroy(_mutex);
	}

	static inline void mtuner_mutex_lock(mtuner_mutex* _mutex) {
		sys_lwmutex_lock(_mutex, 0);
	}

	static inline int mtuner_mutex_trylock(mtuner_mutex* _mutex) {
		return (sys_lwmutex_trylock(_mutex) == CELL_OK) ? 0 : 1;
	}

	static inline void mtuner_mutex_unlock(mtuner_mutex* _mutex) {
		sys_lwmutex_unlock(_mutex);
	}

#elif RMEM_PLATFORM_PS4
	typedef ScePthreadMutex mtuner_mutex;

	static inline void mtuner_mutex_init(mtuner_mutex* _mutex) {
		ScePthreadMutexattr mutexAttr;
		scePthreadMutexattrInit(&mutexAttr);
		scePthreadMutexattrSettype(&mutexAttr, SCE_PTHREAD_MUTEX_RECURSIVE);
		scePthreadMutexInit(_mutex, &mutexAttr, 0);
		scePthreadMutexattrDestroy(&mutexAttr);
	}

	static inline void mtuner_mutex_destroy(mtuner_mutex* _mutex) {
		scePthreadMutexDestroy(_mutex);
	}

	static inline void mtuner_mutex_lock(mtuner_mutex* _mutex) {
		scePthreadMutexLock(_mutex);
	}

	static inline int mtuner_mutex_trylock(mtuner_mutex* _mutex) {
		return (scePthreadMutexTrylock(_mutex) == 0) ? 0 : 1;
	}

	static inline void mtuner_mutex_unlock(mtuner_mutex* _mutex) {
		scePthreadMutexUnlock(_mutex);
	}
	
#endif

	class Mutex
	{
		mtuner_mutex m_mutex;

		Mutex(const Mutex& _rhs);
		Mutex& operator=(const Mutex& _rhs);
		
	public:

		inline Mutex()
		{
			mtuner_mutex_init(&m_mutex);
		}

		inline ~Mutex() 
		{
			mtuner_mutex_destroy(&m_mutex);
		}

		inline void lock()
		{
			mtuner_mutex_lock(&m_mutex);
		}

		inline void unlock()
		{
			mtuner_mutex_unlock(&m_mutex);
		}

		inline bool tryLock()
		{
			return (mtuner_mutex_trylock(&m_mutex) == 0);
		}
	};

	class ScopedMutexLocker
	{
		Mutex& m_mutex;

		ScopedMutexLocker();
		ScopedMutexLocker(const ScopedMutexLocker&);
		ScopedMutexLocker& operator = (const ScopedMutexLocker&);

	public:

		inline ScopedMutexLocker(Mutex& _mutex) :
			m_mutex(_mutex)
		{
			m_mutex.lock();
		}

		inline ~ScopedMutexLocker()
		{
			m_mutex.unlock();
		}
	};

} // namespace rmem

#endif // RMEM_MUTEX_H

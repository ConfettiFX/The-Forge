/*
 * Copyright (c) 2019 by Milos Tosic. All Rights Reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef RMEM_PLATFORM_H
#define RMEM_PLATFORM_H

#include "../inc/rmem.h"
#include "rmem_config.h"

//--------------------------------------------------------------------------
/// Alignment macros
//--------------------------------------------------------------------------
#if RMEM_COMPILER_MSVC
#define RMEM_ALIGN(x)	__declspec(align(x))
#elif RMEM_COMPILER_GCC || RMEM_COMPILER_CLANG
#define RMEM_ALIGN(x)	__attribute__((aligned(x)))
#endif

#define RMEM_ALIGNTO(val, a)	((val+a) & ~(a-1))

//--------------------------------------------------------------------------
/// Platform specific headers
//--------------------------------------------------------------------------
#if RMEM_PLATFORM_XBOXONE
	#include <windows.h>
#elif RMEM_PLATFORM_XBOX360
	#ifndef _XBOX
	#define _XBOX
	#endif
	#include <ppcintrinsics.h>
	#include <xtl.h>
	#include <xbdm.h>
#elif RMEM_PLATFORM_WINDOWS
	#define WINDOWS_LEAN_AND_MEAN
	#ifndef _WIN32_WINNT
		#define _WIN32_WINNT 0x601
	#endif
	#include <windows.h>
	#if RMEM_COMPILER_MSVC	
	unsigned __int64 __rdtsc(void);
	#pragma intrinsic(__rdtsc)
	#endif
#elif RMEM_PLATFORM_PS3
	#include <sys/synchronization.h>
	#include <sys/ppu_thread.h>
	#include <sys/sys_time.h>
	#include <cell/dbg.h>
#elif RMEM_PLATFORM_PS4
	#define _SYS__STDINT_H_
	#include <kernel.h>
	#include <stdio.h>	// FILE
#elif RMEM_PLATFORM_LINUX || RMEM_PLATFORM_OSX
	#include <pthread.h>
	#include <unistd.h> // syscall
	#include <sys/time.h>
	#include <sys/syscall.h>
	#include <execinfo.h>
#elif RMEM_PLATFORM_ANDROID
	#include <unwind.h>
	#include <pthread.h>
	#include <time.h>
#else
	#error "Unsupported compiler!"
#endif

//--------------------------------------------------------------------------
static inline uint64_t getThreadID()
{
#if RMEM_PLATFORM_WINDOWS || RMEM_PLATFORM_XBOX360 || RMEM_PLATFORM_XBOXONE
	return (uint64_t)GetCurrentThreadId();
#elif RMEM_PLATFORM_LINUX
	return (uint64_t)syscall(SYS_gettid);
#elif RMEM_PLATFORM_IOS || RMEM_PLATFORM_OSX
	return (mach_port_t)::pthread_mach_thread_np(pthread_self() );
#elif RMEM_PLATFORM_PS3
	sys_ppu_thread_t tid;
	sys_ppu_thread_get_id(&tid);
	return (uint64_t)tid;
#elif RMEM_PLATFORM_PS4
	return (uint64_t)scePthreadSelf();
#elif RMEM_PLATFORM_ANDROID
	return pthread_self();
#else
	#error "Undefined platform!"
#endif
}

//--------------------------------------------------------------------------
#if RMEM_PLATFORM_ANDROID
struct unwindArg
{
	int			m_tracesToSkip;
	int			m_numTraces;
	int			m_framesSize;
	uintptr_t*	m_frames;
};
static _Unwind_Reason_Code unwindTraceFunc(struct _Unwind_Context* _context, void* _arg) 
{
	unwindArg& arg = *(unwindArg*)_arg;
	
	if (arg.m_tracesToSkip)
	{
		--arg.m_tracesToSkip;
		return _URC_NO_REASON;
	}

	void* ip = (void*)_Unwind_GetIP(_context);

	if (nullptr == ip) 
		return _URC_END_OF_STACK;
	else
	{
		if (arg.m_numTraces < arg.m_framesSize)
		{
			arg.m_frames[arg.m_numTraces++] = (uintptr_t)ip;
			return _URC_NO_REASON;
		}
		return _URC_END_OF_STACK;
	}
}
#endif // RMEM_PLATFORM_ANDROID

static inline uint32_t getStackTrace(uintptr_t _traces[], uint32_t _numFrames, uint32_t _skip)
{
#if RMEM_PLATFORM_WINDOWS || RMEM_PLATFORM_XBOXONE

	#if RMEM_COMPILER_MSVC || RMEM_COMPILER_GCC
		return (uint32_t)RtlCaptureStackBackTrace((ULONG)_skip, (ULONG)_numFrames, (PVOID*)_traces, NULL);
	#else
		#error "Unsupported compiler!"
	#endif

#elif RMEM_PLATFORM_PS3

	uint32_t count;
	if (CELL_OK != cellDbgPpuThreadCountStackFrames(&count))
		return 0;

	count -= _skip;
	uint32_t num = (count > _numFrames) ? _numFrames : count;
	if (CELL_OK != cellDbgPpuThreadGetStackBackTrace(_skip, count, _traces, NULL))
		return 0;

	return num;

#elif RMEM_PLATFORM_PS4

	uint32_t num = 0;
	void** ptr = (void**)__builtin_frame_address(0);
	while (_skip)
	{
		ptr = (void**)(*ptr);
		--_skip;
	}
	while (ptr && num < _numFrames)
	{
		_traces[num++] = (uintptr_t)(*(ptr + 1)) - (uintptr_t)0x00400000;
		ptr = (void**)(*ptr);
	}
	return num;

#elif RMEM_PLATFORM_ANDROID
	
	unwindArg arg;
	arg.m_tracesToSkip	= _skip;
	arg.m_numTraces		= 0;
	arg.m_framesSize	= _numFrames;
	arg.m_frames		= _traces;
	_Unwind_Backtrace(unwindTraceFunc, &arg);
	return arg.m_numTraces;

#elif RMEM_PLATFORM_LINUX || RMEM_PLATFORM_OSX
	void* trace[256];
	uint32_t numTraces = (uint32_t)backtrace(trace, 256);
	if (_skip >= numTraces)
		return 0;
	const uint32_t retTraces = numTraces - _skip;
	for (uint32_t i=0; i<retTraces && i<_numFrames; ++i)
		_traces[i] = (uintptr_t)trace[i+_skip];
	return retTraces;

#elif RMEM_PLATFORM_XBOX360
	void* trace[256];
	HRESULT hr = DmCaptureStackBackTrace(256, trace);
	if (hr == XBDM_NOERR)
	{
		uint32_t retTraces = 0;
		for (uint32_t i=0; i<256; ++i)
		{
			if (trace[i] == 0) break;
			if (i>=_skip)
			{
				_traces[i-_skip] = (uintptr_t)trace[i];
				++retTraces;
			}
		}
		return retTraces;
	}
	else
		return 0;
#else
	#error "Unsupported platform!"
#endif
}

//--------------------------------------------------------------------------

inline uint64_t getCPUClock()
{
#if RMEM_PLATFORM_WINDOWS || RMEM_PLATFORM_XBOX360 || RMEM_PLATFORM_XBOXONE
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	int64_t q = li.QuadPart;
#elif RMEM_PLATFORM_PS3
	int64_t q = (int64_t)sys_time_get_system_time();
#elif RMEM_PLATFORM_PS4
	int64_t q = sceKernelReadTsc();
#elif RMEM_PLATFORM_ANDROID
	int64_t q = clock();
#else
	struct timeval now;
	gettimeofday(&now, 0);
	int64_t q = now.tv_sec*1000000 + now.tv_usec;
#endif
	return q;
}

inline uint64_t getCPUFrequency()
{
#if RMEM_PLATFORM_WINDOWS || RMEM_PLATFORM_XBOX360 || RMEM_PLATFORM_XBOXONE
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	return li.QuadPart;
#elif RMEM_PLATFORM_PS4
	return sceKernelGetTscFrequency();
#elif RMEM_PLATFORM_ANDROID
	return CLOCKS_PER_SEC;
#else
	return 1000000;
#endif
}

#endif // RMEM_PLATFORM_H

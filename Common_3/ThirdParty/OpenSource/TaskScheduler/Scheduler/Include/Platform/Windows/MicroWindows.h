// The MIT License (MIT)
// 
// 	Copyright (c) 2015 Sergey Makeev, Vadim Slyusarev
// 
// 	Permission is hereby granted, free of charge, to any person obtaining a copy
// 	of this software and associated documentation files (the "Software"), to deal
// 	in the Software without restriction, including without limitation the rights
// 	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// 	copies of the Software, and to permit persons to whom the Software is
// 	furnished to do so, subject to the following conditions:
// 
//  The above copyright notice and this permission notice shall be included in
// 	all copies or substantial portions of the Software.
// 
// 	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// 	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// 	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// 	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// 	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// 	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// 	THE SOFTWARE.
#pragma once

#include "MTConfig.h"
#include "MTTypes.h"


//
// micro windows header is used to avoid including heavy windows header to MTPlatform.h
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#define MW_WINBASEAPI __declspec(dllimport)
#define MW_WINAPI __stdcall

#if defined(_WINDOWS_) || defined(_WINBASE_)

//
// if windows.h is already included simply create aliases to the MW_ types
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef LARGE_INTEGER MW_LARGE_INTEGER;
typedef BOOL MW_BOOL;
typedef HANDLE MW_HANDLE;

typedef DWORD MW_DWORD;
typedef WORD MW_WORD;
typedef DWORD64 MW_DWORD64;
typedef ULONG_PTR MW_ULONG_PTR;

typedef LPTHREAD_START_ROUTINE TThreadStartFunc;

typedef SYSTEM_INFO MW_SYSTEM_INFO;

typedef CRITICAL_SECTION MW_CRITICAL_SECTION;
typedef CONDITION_VARIABLE MW_CONDITION_VARIABLE;

typedef CONTEXT MW_CONTEXT;

#define MW_INFINITE (INFINITE)
#define MW_WAIT_OBJECT_0 (WAIT_OBJECT_0)
#define MW_MEM_COMMIT (MEM_COMMIT)
#define MW_PAGE_READWRITE (PAGE_READWRITE)
#define MW_PAGE_NOACCESS (PAGE_NOACCESS)
#define MW_MEM_RELEASE (MEM_RELEASE)
#define MW_ERROR_TIMEOUT (ERROR_TIMEOUT)

#define MW_CURRENT_FIBER_OFFSET (FIELD_OFFSET(NT_TIB, FiberData))
#define MW_STACK_BASE_OFFSET (FIELD_OFFSET(NT_TIB, StackBase))
#define MW_STACK_STACK_LIMIT_OFFSET (FIELD_OFFSET(NT_TIB, StackLimit))
#define MW_CONTEXT_FULL (CONTEXT_FULL)


#define MW_FIBER_FLAG_FLOAT_SWITCH (FIBER_FLAG_FLOAT_SWITCH)

#define MW_THREAD_PRIORITY_HIGHEST (THREAD_PRIORITY_HIGHEST) 
#define MW_THREAD_PRIORITY_NORMAL (THREAD_PRIORITY_NORMAL) 
#define MW_THREAD_PRIORITY_LOWEST (THREAD_PRIORITY_LOWEST) 

#define MW_CREATE_SUSPENDED (CREATE_SUSPENDED)

#define MW_MAXIMUM_PROCESSORS (MAXIMUM_PROCESSORS)

#else

// windows.h is not included, so declare types

//
// define types
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct MW_LARGE_INTEGER
{
	int64 QuadPart;
};

typedef int MW_BOOL;
typedef void* MW_HANDLE;

typedef unsigned long MW_DWORD;
typedef unsigned short MW_WORD;
typedef unsigned __int64 MW_DWORD64;

#if MT_PTR64
typedef unsigned __int64 MW_ULONG_PTR;
#else
typedef unsigned __int32 MW_ULONG_PTR;
#endif

//
// define thread function
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef MW_DWORD ( MW_WINAPI *TThreadStartFunc )(void* lpThreadParameter);

//
// define fiber function
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef void ( MW_WINAPI *TFiberStartFunc)(void* lpFiberParameter);




//
// system info structure, only used members are declared
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct MW_SYSTEM_INFO
{
	uint8 _unused_01[4];
	MW_DWORD dwPageSize;
	void* _unused_02[3];
	MW_DWORD dwNumberOfProcessors;
	uint8 _unused_03[12];
};


// Condition variable
typedef void* MW_CONDITION_VARIABLE;


#if MT_PTR64

//
// x64 critical section, only used members are declared
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct MW_CRITICAL_SECTION
{
	uint8 _unused[40];
};

//
// x64 machine context, only used members are declared
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct __declspec(align(16)) MW_CONTEXT
{
	uint8 _unused_01[48];
	MW_DWORD ContextFlags;
	uint8 _unused_02[100];
	MW_DWORD64 Rsp;
	uint8 _unused_03[88];
	MW_DWORD64 Rip;
	uint8 _unused_04[976];
};

static_assert(__alignof(MW_CONTEXT) == 16, "MW_CONTEXT align requirements must be 16 bytes");

#define MW_CURRENT_FIBER_OFFSET (32)
#define MW_STACK_BASE_OFFSET (8)
#define MW_STACK_STACK_LIMIT_OFFSET (16)
#define MW_CONTEXT_FULL (0x10000B)

#else

//
// x86 critical section, only used members are declared
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct MW_CRITICAL_SECTION
{
	uint8 _unused[24];
};

//
// x86 machine context, only used members are declared
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct MW_CONTEXT
{
	MW_DWORD ContextFlags;
	uint8 _unused_01[180];
	MW_DWORD   Eip;
	uint8 _unused_02[8];
	MW_DWORD   Esp;
	uint8 _unused_03[516];
};


#define MW_CURRENT_FIBER_OFFSET (16)
#define MW_STACK_BASE_OFFSET (4)
#define MW_STACK_STACK_LIMIT_OFFSET (8)
#define MW_CONTEXT_FULL (0x10007)


#endif


//
// defines and flags
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define MW_INFINITE (0xFFFFFFFF)
#define MW_WAIT_OBJECT_0 (0)
#define MW_MEM_COMMIT (0x1000)
#define MW_PAGE_READWRITE (0x04)
#define MW_PAGE_NOACCESS (0x01)
#define MW_MEM_RELEASE (0x8000)
#define MW_ERROR_TIMEOUT (1460L)

#define MW_THREAD_PRIORITY_HIGHEST (2) 
#define MW_THREAD_PRIORITY_NORMAL (0) 
#define MW_THREAD_PRIORITY_LOWEST (-2) 

#define MW_CREATE_SUSPENDED (0x00000004)


#if MT_PTR64
#define MW_MAXIMUM_PROCESSORS (64)
#define MW_FIBER_FLAG_FLOAT_SWITCH (0x1)
#else
#define MW_MAXIMUM_PROCESSORS (32)
#define MW_FIBER_FLAG_FLOAT_SWITCH (0x1)
#endif


#endif




#if !defined(MW_SKIP_FUNCTIONS) && !defined(_WINDOWS_)

//
// functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////


extern "C" {

MW_WINBASEAPI MW_BOOL MW_WINAPI QueryPerformanceFrequency(MW_LARGE_INTEGER* lpFrequency);
MW_WINBASEAPI MW_BOOL MW_WINAPI QueryPerformanceCounter(MW_LARGE_INTEGER* lpPerformanceCount);

MW_WINBASEAPI MW_ULONG_PTR MW_WINAPI SetThreadAffinityMask(MW_HANDLE hThread, MW_ULONG_PTR dwThreadAffinityMask );
MW_WINBASEAPI MW_DWORD MW_WINAPI SetThreadIdealProcessor(MW_HANDLE hThread, MW_DWORD dwIdealProcessor );
MW_WINBASEAPI MW_BOOL MW_WINAPI SetThreadPriority(MW_HANDLE hThread, int nPriority );
MW_WINBASEAPI MW_HANDLE MW_WINAPI CreateThread(void* lpThreadAttributes, size_t dwStackSize, TThreadStartFunc lpStartAddress, void* lpParameter, MW_DWORD dwCreationFlags, MW_DWORD* lpThreadId);
MW_WINBASEAPI MW_BOOL MW_WINAPI CloseHandle(MW_HANDLE hObject);
MW_WINBASEAPI MW_HANDLE MW_WINAPI GetCurrentThread();
MW_WINBASEAPI MW_DWORD MW_WINAPI GetCurrentThreadId();
MW_WINBASEAPI MW_DWORD MW_WINAPI ResumeThread(MW_HANDLE hThread);
MW_WINBASEAPI MW_BOOL MW_WINAPI SwitchToThread();


MW_WINBASEAPI void MW_WINAPI GetSystemInfo(MW_SYSTEM_INFO* lpSystemInfo);

MW_WINBASEAPI void MW_WINAPI Sleep(MW_DWORD dwMilliseconds);
MW_WINBASEAPI MW_DWORD MW_WINAPI WaitForSingleObject(MW_HANDLE hHandle, MW_DWORD dwMilliseconds);

MW_WINBASEAPI	void MW_WINAPI InitializeConditionVariable (MW_CONDITION_VARIABLE* lpConditionVariable);
MW_WINBASEAPI	void MW_WINAPI WakeConditionVariable (MW_CONDITION_VARIABLE* lpConditionVariable);
MW_WINBASEAPI	void MW_WINAPI WakeAllConditionVariable (MW_CONDITION_VARIABLE* lpConditionVariable);
MW_WINBASEAPI	MW_BOOL	MW_WINAPI SleepConditionVariableCS (MW_CONDITION_VARIABLE* lpConditionVariable,	MW_CRITICAL_SECTION* lpCriticalSection, MW_DWORD dwMilliseconds);

MW_WINBASEAPI bool MW_WINAPI InitializeCriticalSectionAndSpinCount(MW_CRITICAL_SECTION* lpCriticalSection, MW_DWORD dwSpinCount );
MW_WINBASEAPI void MW_WINAPI DeleteCriticalSection(MW_CRITICAL_SECTION* lpCriticalSection );
MW_WINBASEAPI void MW_WINAPI EnterCriticalSection(MW_CRITICAL_SECTION* lpCriticalSection );
MW_WINBASEAPI void MW_WINAPI LeaveCriticalSection(MW_CRITICAL_SECTION* lpCriticalSection );

MW_WINBASEAPI MW_HANDLE MW_WINAPI CreateEventA(MW_CRITICAL_SECTION* lpEventAttributes, MW_BOOL bManualReset, MW_BOOL bInitialState, const char* lpName );
MW_WINBASEAPI MW_HANDLE MW_WINAPI CreateEventW(MW_CRITICAL_SECTION* lpEventAttributes, MW_BOOL bManualReset, MW_BOOL bInitialState, const wchar_t* lpName );
MW_WINBASEAPI MW_BOOL MW_WINAPI SetEvent( MW_HANDLE hEvent );
MW_WINBASEAPI MW_BOOL MW_WINAPI ResetEvent( MW_HANDLE hEvent );

MW_WINBASEAPI MW_BOOL MW_WINAPI GetThreadContext( MW_HANDLE hThread, MW_CONTEXT* lpContext );
MW_WINBASEAPI MW_BOOL MW_WINAPI SetThreadContext( MW_HANDLE hThread, const MW_CONTEXT* lpContext );

MW_WINBASEAPI void* MW_WINAPI VirtualAlloc( void* lpAddress, size_t dwSize, MW_DWORD flAllocationType, MW_DWORD flProtect );
MW_WINBASEAPI MW_BOOL MW_WINAPI VirtualProtect( void* lpAddress, size_t dwSize, MW_DWORD flNewProtect, MW_DWORD* lpflOldProtect );
MW_WINBASEAPI MW_BOOL MW_WINAPI VirtualFree( void* lpAddress, size_t dwSize, MW_DWORD dwFreeType );


MW_WINBASEAPI void MW_WINAPI DeleteFiber( void* lpFiber );
MW_WINBASEAPI void* MW_WINAPI ConvertThreadToFiberEx( void* lpParameter, MW_DWORD dwFlags );
MW_WINBASEAPI void* MW_WINAPI CreateFiber( size_t dwStackSize, TFiberStartFunc lpStartAddress, void* lpParameter );
MW_WINBASEAPI void MW_WINAPI SwitchToFiber( void* lpFiber );
MW_WINBASEAPI MW_BOOL MW_WINAPI IsThreadAFiber();

MW_WINBASEAPI void MW_WINAPI RaiseException(MW_DWORD dwExceptionCode, MW_DWORD dwExceptionFlags, MW_DWORD nNumberOfArguments, const MW_ULONG_PTR* lpArguments );

MW_WINBASEAPI MW_DWORD MW_WINAPI GetLastError();




}

#endif

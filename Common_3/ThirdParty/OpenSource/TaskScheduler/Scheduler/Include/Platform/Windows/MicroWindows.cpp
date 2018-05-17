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


#include "MTConfig.h"

#if MT_PLATFORM_WINDOWS 

#include <type_traits>

#define MW_SKIP_FUNCTIONS
#include "MicroWindows.h"

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif

#ifdef _WIN32_WINNT
	#undef _WIN32_WINNT
#endif


#define _WIN32_WINNT 0x0403
#include <windows.h>



//
// Here we will check that the MicroWindows.h is fully compatible with the standard Windows.h
//

// Check types
static_assert(sizeof(MW_DWORD) == sizeof(DWORD), "MW_DWORD != DWORD");
static_assert(sizeof(MW_DWORD) == sizeof(uint32), "MW_DWORD != uint32");
static_assert(sizeof(MW_WORD) == sizeof(WORD), "MW_WORD != WORD");
static_assert(sizeof(MW_WORD) == sizeof(uint16), "MW_WORD != uint16");
static_assert(sizeof(MW_DWORD64) == sizeof(DWORD64), "MW_DWORD64 != DWORD64");
static_assert(sizeof(MW_DWORD64) == sizeof(uint64), "MW_DWORD64 != uint64");
static_assert(sizeof(MW_BOOL) == sizeof(BOOL), "MW_BOOL != BOOL");
static_assert(sizeof(MW_HANDLE) == sizeof(HANDLE), "MW_HANDLE != HANDLE");
static_assert(sizeof(MW_LARGE_INTEGER) == sizeof(LARGE_INTEGER), "MW_LARGE_INTEGER != LARGE_INTEGER");
static_assert(sizeof(MW_SYSTEM_INFO) == sizeof(SYSTEM_INFO), "MW_SYSTEM_INFO != SYSTEM_INFO");
static_assert(sizeof(MW_CRITICAL_SECTION) == sizeof(CRITICAL_SECTION), "MW_CRITICAL_SECTION != CRITICAL_SECTION");
static_assert(sizeof(MW_CONDITION_VARIABLE) == sizeof(CONDITION_VARIABLE), "MW_CRITICAL_SECTION != CRITICAL_SECTION");
static_assert(sizeof(MW_CONTEXT) == sizeof(CONTEXT), "MW_CONTEXT != CONTEXT");

// Check defines and flags
static_assert(MW_ERROR_TIMEOUT == ERROR_TIMEOUT, "MW_ERROR_TIMEOUT != ERROR_TIMEOUT");
static_assert(MW_THREAD_PRIORITY_LOWEST == THREAD_PRIORITY_LOWEST, "MW_THREAD_PRIORITY_LOWEST != THREAD_PRIORITY_LOWEST");
static_assert(MW_THREAD_PRIORITY_NORMAL == THREAD_PRIORITY_NORMAL, "MW_THREAD_PRIORITY_NORMAL != THREAD_PRIORITY_NORMAL");
static_assert(MW_THREAD_PRIORITY_HIGHEST == THREAD_PRIORITY_HIGHEST, "MW_THREAD_PRIORITY_HIGHEST != THREAD_PRIORITY_HIGHEST");
static_assert(MW_CREATE_SUSPENDED == CREATE_SUSPENDED, "MW_CREATE_SUSPENDED != CREATE_SUSPENDED");
static_assert(MW_MAXIMUM_PROCESSORS == MAXIMUM_PROCESSORS, "MW_MAXIMUM_PROCESSORS != MAXIMUM_PROCESSORS");
static_assert(MW_INFINITE == INFINITE, "MW_INFINITE != INFINITE");
static_assert(MW_WAIT_OBJECT_0 == WAIT_OBJECT_0, "MW_WAIT_OBJECT_0 != WAIT_OBJECT_0");
static_assert(MW_CONTEXT_FULL == CONTEXT_FULL, "MW_CONTEXT_FULL != CONTEXT_FULL");
static_assert(MW_MEM_COMMIT == MEM_COMMIT, "MW_MEM_COMMIT != MEM_COMMIT");
static_assert(MW_PAGE_READWRITE == PAGE_READWRITE, "MW_PAGE_READWRITE != PAGE_READWRITE");
static_assert(MW_PAGE_NOACCESS == PAGE_NOACCESS, "MW_PAGE_NOACCESS != PAGE_NOACCESS");
static_assert(MW_MEM_RELEASE == MEM_RELEASE, "MW_MEM_RELEASE != MEM_RELEASE");
//static_assert(MW_FIBER_FLAG_FLOAT_SWITCH == FIBER_FLAG_FLOAT_SWITCH, "MW_FIBER_FLAG_FLOAT_SWITCH != FIBER_FLAG_FLOAT_SWITCH");


// Check offsets
static_assert(MW_CURRENT_FIBER_OFFSET == FIELD_OFFSET(NT_TIB, FiberData), "MW_STACK_BASE_OFFSET != FIELD_OFFSET(NT_TIB, StackBase)");
static_assert(MW_STACK_BASE_OFFSET == FIELD_OFFSET(NT_TIB, StackBase), "MW_STACK_BASE_OFFSET != FIELD_OFFSET(NT_TIB, StackBase)");
static_assert(MW_STACK_STACK_LIMIT_OFFSET == FIELD_OFFSET(NT_TIB, StackLimit), "MW_STACK_STACK_LIMIT_OFFSET != FIELD_OFFSET(NT_TIB, StackLimit)");
static_assert(FIELD_OFFSET(MW_SYSTEM_INFO, dwPageSize) == FIELD_OFFSET(SYSTEM_INFO, dwPageSize), "FIELD_OFFSET(MW_SYSTEM_INFO, dwPageSize) != FIELD_OFFSET(SYSTEM_INFO, dwPageSize)");
static_assert(FIELD_OFFSET(MW_SYSTEM_INFO, dwNumberOfProcessors) == FIELD_OFFSET(SYSTEM_INFO, dwNumberOfProcessors), "FIELD_OFFSET(MW_SYSTEM_INFO, dwNumberOfProcessors) != FIELD_OFFSET(SYSTEM_INFO, dwNumberOfProcessors)");
static_assert(FIELD_OFFSET(MW_CONTEXT, ContextFlags) == FIELD_OFFSET(CONTEXT, ContextFlags), "FIELD_OFFSET(MW_CONTEXT, ContextFlags) != FIELD_OFFSET(CONTEXT, ContextFlags)");

#if MT_PTR64

static_assert(FIELD_OFFSET(MW_CONTEXT, Rsp) == FIELD_OFFSET(CONTEXT, Rsp), "FIELD_OFFSET(MW_CONTEXT, Rsp) != FIELD_OFFSET(CONTEXT, Rsp)");
static_assert(FIELD_OFFSET(MW_CONTEXT, Rip) == FIELD_OFFSET(CONTEXT, Rip), "FIELD_OFFSET(MW_CONTEXT, Rip) != FIELD_OFFSET(CONTEXT, Rip)");

#else

static_assert(FIELD_OFFSET(MW_CONTEXT, Esp) == FIELD_OFFSET(CONTEXT, Esp), "FIELD_OFFSET(MW_CONTEXT, Esp) != FIELD_OFFSET(CONTEXT, Esp)");
static_assert(FIELD_OFFSET(MW_CONTEXT, Eip) == FIELD_OFFSET(CONTEXT, Eip), "FIELD_OFFSET(MW_CONTEXT, Eip) != FIELD_OFFSET(CONTEXT, Eip)");

#endif



void DummyAssignableCheck()
{
	// Visual Studio 2010 is not support std::is_same<>, but we just need a compile error when types are different
	// so dummy assignable check here

	TThreadStartFunc mw_func = nullptr;
	LPTHREAD_START_ROUTINE func = nullptr;
	
	//check TThreadStartFunc is equal to LPTHREAD_START_ROUTINE
	mw_func = func;
}

#endif

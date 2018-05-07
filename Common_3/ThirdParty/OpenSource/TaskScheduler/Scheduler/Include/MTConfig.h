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


// Target Platform
////////////////////////////////////////////////////////////////////////
#if   _WIN32

#define MT_PLATFORM_WINDOWS (1)

#elif __APPLE_CC__

#define MT_PLATFORM_OSX (1)

#else

#define MT_PLATFORM_POSIX (1)

#endif


// Compiler support for SSE intrinsics
////////////////////////////////////////////////////////////////////////
#if (defined(__SSE__) || defined(_M_IX86) || defined(_M_X64))

#define MT_SSE_INTRINSICS_SUPPORTED (1)

#endif


// Compiler support for C++11
////////////////////////////////////////////////////////////////////////

#if __STDC_VERSION__ >= 201112L

#define MT_CPP11_SUPPORTED (1)

#endif


// Compiler family
////////////////////////////////////////////////////////////////////////

#ifdef __clang__

#define MT_CLANG_COMPILER_FAMILY (1)
#define MT_GCC_COMPILER_FAMILY (1)

#elif __GNUC__

#define MT_GCC_COMPILER_FAMILY (1)

#elif defined(_MSC_VER)

#define MT_MSVC_COMPILER_FAMILY (1)

#endif


// Debug / Release
////////////////////////////////////////////////////////////////////////

#if defined(_DEBUG) || defined(DEBUG)

#define MT_DEBUG (1)

#else

#define MT_RELEASE (1)

#endif


// x64 / x86
////////////////////////////////////////////////////////////////////////
#if defined(_M_X64)

#define MT_PTR64 (1)

#endif



//
// x86-64 CPU has strong memory model, so we don't need to define acquire/release fences here
// 

//
// Acquire semantics is a property which can only apply to operations which read from shared memory,
// whether they are read-modify-write operations or plain loads. The operation is then considered a read-acquire.
// Acquire semantics prevent memory reordering of the read-acquire with any read or write operation which follows it in program order.
//
// Acquire fence is a fence which does not permit subsequent memory operations to be advanced before it.
//

#if MT_MSVC_COMPILER_FAMILY
// MSVC compiler barrier
#define mt_acquire_fence() _ReadWriteBarrier()
#elif MT_GCC_COMPILER_FAMILY
// GCC compiler barrier
#define mt_acquire_fence() asm volatile("" ::: "memory")
#else
#error Platform is not supported!
#endif


	
//
// Release semantics is a property which can only apply to operations which write to shared memory,
// whether they are read-modify-write operations or plain stores. The operation is then considered a write-release.
// Release semantics prevent memory reordering of the write-release with any read or write operation which precedes it in program order.
//
// Release fence is a fence which does not permit preceding memory operations to be delayed past it.
//

#if MT_MSVC_COMPILER_FAMILY
// MSVC compiler barrier
#define mt_release_fence() _ReadWriteBarrier()
#elif MT_GCC_COMPILER_FAMILY
// GCC compiler barrier
#define mt_release_fence() asm volatile("" ::: "memory")
#else
#error Platform is not supported!
#endif



//
// mt_forceinline
//
#if MT_MSVC_COMPILER_FAMILY
#define mt_forceinline __forceinline
#elif MT_GCC_COMPILER_FAMILY
#define mt_forceinline __attribute__((always_inline)) inline
#else
#error Can not define mt_forceinline. Unknown platform.
#endif


// Enable Windows XP support (disable conditional variables)
//#if !MT_PTR64 && MT_PLATFORM_WINDOWS
//#define MT_ENABLE_LEGACY_WINDOWSXP_SUPPORT (1)
//#endif


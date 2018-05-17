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
#include "../Include/MTConfig.h"
#include "../Include/MTAppInterop.h"
#include "../Include/MTTools.h"

#include <stdio.h>

#if MT_SSE_INTRINSICS_SUPPORTED
#include <xmmintrin.h>
#endif




#if MT_PLATFORM_WINDOWS 

inline void ThrowException()
{
	__debugbreak();
}

#elif MT_PLATFORM_POSIX

#include<signal.h>
inline void ThrowException()
{
	raise(SIGTRAP);
	// force access violation error
	char* pBadAddr = (char*)0x0;
	*pBadAddr = 0;
}

#elif MT_PLATFORM_OSX

inline void ThrowException()
{
	__builtin_trap();
}

#else

#error Platform is not supported!

#endif



namespace MT
{

	void* Memory::Alloc(size_t size, size_t align)
	{
		void* p = nullptr;
#if MT_SSE_INTRINSICS_SUPPORTED
		p = _mm_malloc(size, align);
#else
        if (posix_memalign(&p, size, align) != 0)
        {
            p = nullptr;
        }
#endif
		MT_ASSERT(p, "Can't allocate memory");
		return p;
	}

	void Memory::Free(void* p)
	{
#if MT_SSE_INTRINSICS_SUPPORTED
		_mm_free(p);
#else
		free(p);
#endif
	}

	Memory::StackDesc Memory::AllocStack(size_t size)
	{
		StackDesc desc;

#if MT_PLATFORM_WINDOWS 

		MW_SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);

		int pageSize = (int)systemInfo.dwPageSize;
		int pagesCount = (int)size / pageSize;

		//need additional page for stack guard
		if ((size % pageSize) > 0)
		{
			pagesCount++;
		}

		//protected guard page
		pagesCount++;

		desc.stackMemoryBytesCount = pagesCount * pageSize;
		desc.stackMemory = (char*)VirtualAlloc(NULL, desc.stackMemoryBytesCount, MW_MEM_COMMIT, MW_PAGE_READWRITE);
		MT_ASSERT(desc.stackMemory != NULL, "Can't allocate memory");

		desc.stackBottom = desc.stackMemory + pageSize;
		desc.stackTop = desc.stackMemory + desc.stackMemoryBytesCount;

		MW_DWORD oldProtect = 0;
		MW_BOOL res = VirtualProtect(desc.stackMemory, pageSize, MW_PAGE_NOACCESS, &oldProtect);
		MT_USED_IN_ASSERT(res);
		MT_ASSERT(res != 0, "Can't protect memory");

#elif MT_PLATFORM_POSIX || MT_PLATFORM_OSX

		int pageSize = (int)sysconf(_SC_PAGE_SIZE);
		int pagesCount = (int)(size / pageSize);

		//need additional page for stack tail
		if ((size % pageSize) > 0)
		{
			pagesCount++;
		}

		//protected guard page
		pagesCount++;

		desc.stackMemoryBytesCount = pagesCount * pageSize;
		desc.stackMemory = (char*)mmap(NULL, desc.stackMemoryBytesCount, PROT_READ | PROT_WRITE,  MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);

		MT_ASSERT((void *)desc.stackMemory != (void *)-1, "Can't allocate memory");

		desc.stackBottom = desc.stackMemory + pageSize;
		desc.stackTop = desc.stackMemory + desc.stackMemoryBytesCount;

		int res = mprotect(desc.stackMemory, pageSize, PROT_NONE);
		MT_USED_IN_ASSERT(res);
		MT_ASSERT(res == 0, "Can't protect memory");
#else
		#error Platform is not supported!
#endif

		return desc;
	}

	void Memory::FreeStack(const Memory::StackDesc & desc)
	{
#if MT_PLATFORM_WINDOWS 

		int res = VirtualFree(desc.stackMemory, 0, MW_MEM_RELEASE);
		MT_USED_IN_ASSERT(res);
		MT_ASSERT(res != 0, "Can't free memory");

#elif MT_PLATFORM_POSIX || MT_PLATFORM_OSX

		int res = munmap(desc.stackMemory, desc.stackMemoryBytesCount);
		MT_USED_IN_ASSERT(res);
		MT_ASSERT(res == 0, "Can't free memory");
#else
		#error Platform is not supported!
#endif
	}

	void Diagnostic::ReportAssert(const char* condition, const char* description, const char* sourceFile, int sourceLine)
	{
		printf("Assertion failed : %s. File %s, line %d. Condition %s\n", description, sourceFile, sourceLine, condition);
		ThrowException();
	}




}

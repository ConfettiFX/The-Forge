/////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////


#ifndef EASTL_ALLOCATOR_FORGE_H
#define EASTL_ALLOCATOR_FORGE_H

#include "internal/config.h"

#if EASTL_ALLOCATOR_FORGE

extern void* conf_malloc(size_t size);
extern void* conf_memalign(size_t align, size_t size);
extern void conf_free(void* ptr);

namespace eastl
{

	///////////////////////////////////////////////////////////////////////////////
	// allocator_forge
	//
	// Implements an EASTL allocator that uses malloc/free as opposed to
	// new/delete or PPMalloc Malloc/Free.
	//
	// Example usage:
	//      vector<int, allocator_forge> intVector;
	//
	class allocator_forge
	{
	public:
		allocator_forge(const char* = NULL) {}

		allocator_forge(const allocator_forge&) {}

		allocator_forge(const allocator_forge&, const char*) {}

		allocator_forge& operator=(const allocator_forge&) { return *this; }

		bool operator==(const allocator_forge&) { return true; }

		bool operator!=(const allocator_forge&) { return false; }

		void* allocate(size_t n, int /*flags*/ = 0) { return conf_malloc(n); }

		void* allocate(size_t n, size_t alignment, size_t alignmentOffset, int /*flags*/ = 0)
		{
			if((alignmentOffset % alignment) == 0) // We check for (offset % alignmnent == 0) instead of (offset == 0) because any block which is aligned on e.g. 64 also is aligned at an offset of 64 by definition. 
				return conf_memalign(alignment, n);

			return NULL;
		}

		void deallocate(void* p, size_t /*n*/) { conf_free(p); }

		const char* get_name() const { return "allocator_forge"; }

		void set_name(const char*) {}
	};

	EASTL_API allocator_forge* GetDefaultAllocatorForge();
	EASTL_API allocator_forge* SetDefaultAllocatorForge(allocator_forge* pAllocator);

} // namespace eastl

#endif

#endif // Header include guard

#include "internal/config.h"
#include "allocator_forge.h"

#if EASTL_ALLOCATOR_FORGE
#include "../../../OS/Interfaces/IMemory.h"

	namespace eastl
	{

		void* allocator_forge::allocate(size_t n, int /*flags*/)
		{ 
			return tf_malloc(n);
		}

		void* allocator_forge::allocate(size_t n, size_t alignment, size_t alignmentOffset, int /*flags*/)
		{
		    if ((alignmentOffset % alignment) == 0) // We check for (offset % alignmnent == 0) instead of (offset == 0) because any block which is
													// aligned on e.g. 64 also is aligned at an offset of 64 by definition.
			    return tf_memalign(alignment, n);

		    return NULL;
		}

		void allocator_forge::deallocate(void* p, size_t /*n*/)
		{ 
			tf_free(p);
		}

		/// gDefaultAllocator
		/// Default global allocator_forge instance. 
		EASTL_API allocator_forge  gDefaultAllocatorForge;
		EASTL_API allocator_forge* gpDefaultAllocatorForge = &gDefaultAllocatorForge;

		EASTL_API allocator_forge* GetDefaultAllocatorForge()
		{
			return gpDefaultAllocatorForge;
		}

		EASTL_API allocator_forge* SetDefaultAllocatorForge(allocator_forge* pAllocator)
		{
			allocator_forge* const pPrevAllocator = gpDefaultAllocatorForge;
			gpDefaultAllocatorForge = pAllocator;
			return pPrevAllocator;
		}

	} // namespace eastl


#endif // EASTL_USER_DEFINED_ALLOCATOR












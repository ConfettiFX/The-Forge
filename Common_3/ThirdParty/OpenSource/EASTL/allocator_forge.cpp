#include "internal/config.h"
#include "allocator_forge.h"

#if EASTL_ALLOCATOR_FORGE

	namespace eastl
	{

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












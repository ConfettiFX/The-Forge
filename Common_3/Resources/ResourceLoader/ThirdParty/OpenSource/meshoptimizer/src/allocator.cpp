// This file is part of meshoptimizer library; see meshoptimizer.h for version/license details
#include "meshoptimizer.h"
#include "../../../../../../Utilities/Interfaces/ILog.h"

#include "../../../../../../Utilities/ThirdParty/OpenSource/ModifiedSonyMath/vectormath_settings.hpp"
#define MEM_MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN_ALLOC_ALIGNMENT MEM_MAX(VECTORMATH_MIN_ALIGN, MIN_MALLOC_ALIGNMENT)

size_t buffer_length = 0;
size_t current_offset = 0;
char* buffer; //Scratch-Pad memory

void* Allocate(size_t size)
{
	// make the current offset always aligned
	current_offset += current_offset % MIN_ALLOC_ALIGNMENT;

	if (current_offset + size <= buffer_length)
	{
		void* ptr = &buffer[current_offset];
		current_offset += size;
		return ptr;
	}

	LOGF(LogLevel::eWARNING, "Mesh Optimizer out of memory");
	return NULL;
}

void DeAllocate(void* b)
{
	current_offset = 0;
}

void meshopt_SetScratchMemory(size_t size, void* memory)
{
	buffer_length = size;
	buffer = (char*)memory;

	meshopt_setAllocator();
}

//void meshopt_setAllocator(void* (*allocate)(size_t), void (*deallocate)(void*))
void meshopt_setAllocator()
{
	//meshopt_Allocator::Storage::allocate = allocate;
	//meshopt_Allocator::Storage::deallocate = deallocate;

	meshopt_Allocator::Storage::allocate = Allocate;
	meshopt_Allocator::Storage::deallocate = DeAllocate;
}

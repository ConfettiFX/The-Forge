// This file is part of meshoptimizer library; see meshoptimizer.h for version/license details
#include "../../../../../../Utilities/Interfaces/ILog.h"
#include "meshoptimizer.h"

#include "../../../../../../Utilities/Math/MathTypes.h"

#define MEM_MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN_ALLOC_ALIGNMENT (MEM_MAX(VECTORMATH_MIN_ALIGN, MIN_MALLOC_ALIGNMENT))

static size_t buffer_length = 0;
static size_t current_offset = 0;
static size_t current_buffer = 0;
static uint8_t* buffer[8]; //Scratch-Pad memory

void* Allocate(size_t size)
{
	// make the current offset always aligned
	size = round_up_64(size, MIN_ALLOC_ALIGNMENT);
	if (current_offset + size <= buffer_length)
	{
		void* ptr = &buffer[current_buffer][current_offset];
		current_offset += size;
		return ptr;
	}

	// out of space, allocate a larger block.
	current_buffer++;
	buffer_length = max(buffer_length * 2, size);
	current_offset = 0;
	buffer[current_buffer] = (uint8_t*)tf_malloc(buffer_length);

	return Allocate(size);
}

void DeAllocate(void* b)
{
	UNREF_PARAM(b); 
	// we have a larger buffer, use it as default for the next memory allocations
	if (current_buffer > 0)
	{
		uint8_t* temp = buffer[0];
		buffer[0] = buffer[current_buffer];
		buffer[current_buffer] = temp;

		for (uint32_t i = 1; i <= current_buffer; i++)
		{
			tf_free(buffer[i]);
		}
	}

	current_buffer = 0;
	current_offset = 0;
}

void meshopt_SetScratchMemory(size_t size, void* memory)
{
	buffer_length = size;
	current_buffer = 0;
	current_offset = 0;
	buffer[0] = (uint8_t*)memory;

	meshopt_setAllocator();
}

void meshopt_FreeScratchMemory()
{
	for (uint32_t i = 0; i <= current_buffer; i++)
	{
		tf_free(buffer[i]);
	}
}

//void meshopt_setAllocator(void* (*allocate)(size_t), void (*deallocate)(void*))
void meshopt_setAllocator()
{
	//meshopt_Allocator::Storage::allocate = allocate;
	//meshopt_Allocator::Storage::deallocate = deallocate;

	meshopt_Allocator::Storage::allocate = Allocate;
	meshopt_Allocator::Storage::deallocate = DeAllocate;
}

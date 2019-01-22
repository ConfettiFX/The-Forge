/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

#ifndef RESOURCE_RESOURCE_H
#define RESOURCE_RESOURCE_H

#include "../../OS/Interfaces/ILogManager.h"
#include "../../OS/Interfaces/IMemoryManager.h"

typedef struct ResourceAllocator MemoryAllocator;

typedef struct BufferCreateInfo
{
	const uint64_t mSize;
	//const uint64_t	mAlignment;
} BufferCreateInfo;

typedef struct TextureCreateInfo
{
	MTLTextureDescriptor* pDesc;
	const bool            mIsRT;
	const bool            mIsMS;
} TextureCreateInfo;

// -------------------------------------------------------------------------------------------------
// General enums and structs
// -------------------------------------------------------------------------------------------------

// Define some of these macros before each #include of this header or change them
// here if you need other then default behavior depending on your environment.

// Normal assert to check for programmer's errors, especially in Debug configuration.
#ifndef ASSERT
#ifdef _DEBUG
#define ASSERT(expr) ASSERT(expr)
#else
#define ASSERT(expr)
#endif
#endif

// Assert that will be called very often, like inside data structures e.g. operator[].
// Making it non-empty can make program slow.
#ifndef RESOURCE_HEAVY_ASSERT
#ifdef _DEBUG
#define RESOURCE_HEAVY_ASSERT(expr)    //ASSERT(expr)
#else
#define RESOURCE_HEAVY_ASSERT(expr)
#endif
#endif

#ifndef RESOURCE_NULL
// Value used as null pointer. Define it to e.g.: nullptr, NULL, 0, (void*)0.
#define RESOURCE_NULL nil
#endif

#ifndef RESOURCE_SYSTEM_ALIGNED_MALLOC
#define RESOURCE_SYSTEM_ALIGNED_MALLOC(size) (conf_malloc((size)))
#endif

#ifndef RESOURCE_SYSTEM_FREE
#define RESOURCE_SYSTEM_FREE(ptr) conf_free(ptr)
#endif

template <typename T>
static inline void swap(T& a, T& b)
{
	T c = a;
	a = b;
	b = c;
}

#define min(x, y) x < y ? x : y
#define max(x, y) x > y ? x : y

#ifndef RESOURCE_MIN
#define RESOURCE_MIN(v1, v2) (min((v1), (v2)))
#endif

#ifndef RESOURCE_MAX
#define RESOURCE_MAX(v1, v2) (max((v1), (v2)))
#endif

#ifndef RESOURCE_SWAP
#define RESOURCE_SWAP(v1, v2) swap((v1), (v2))
#endif

#ifdef DEBUG_LOG_MEMORY
#ifndef RESOURCE_DEBUG_LOG
#define RESOURCE_DEBUG_LOG LOGDEBUGF
#endif
#else
#define RESOURCE_DEBUG_LOG(format, ...) ((void)0)
#endif

// Define this macro to 1 to enable functions: resourceAllocBuildStatsString, resourceAllocFreeStatsString.
#define RESOURCE_STATS_STRING_ENABLED 1
#if RESOURCE_STATS_STRING_ENABLED
static inline void AllocatorUint32ToStr(char* outStr, size_t strLen, uint32_t num)
{
	char*    p;         /* pointer to traverse string */
	char*    firstdig;  /* pointer to first digit */
	char     temp;      /* temp char */
	uint32_t val = num; /* full value */
	uint32_t digval;    /* value of digit */

	p = outStr;
	firstdig = p; /* save pointer to first digit */
	do
	{
		digval = (uint32_t)(num % 10UL);
		num /= 10UL; /* get next digit */

		/* convert to ascii and store */
		if (digval > 9)
			*p++ = (char)(digval - 10 + 'a'); /* a letter */
		else
			*p++ = (char)(digval + '0'); /* a digit */
	} while (val > 0);

	/* We now have the digit of the number in the buffer, but in reverse
	 order.  Thus we reverse them now. */
	*p-- = '\0'; /* terminate string; p points to last digit */
	do
	{
		temp = *p;
		*p = *firstdig;
		*firstdig = temp; /* swap *p and *firstdig */
		--p;
		++firstdig;         /* advance to next two digits */
	} while (firstdig < p); /* repeat until halfway */
}
static inline void AllocatorUint64ToStr(char* outStr, size_t strLen, uint64_t num)
{
	char*              p;         /* pointer to traverse string */
	char*              firstdig;  /* pointer to first digit */
	char               temp;      /* temp char */
	uint64_t           val = num; /* full value */
	unsigned long long digval;    /* value of digit */

	p = outStr;
	firstdig = p; /* save pointer to first digit */
	do
	{
		digval = (uint64_t)(val % 10ULL);
		val /= 10ULL; /* get next digit */

		/* convert to ascii and store */
		if (digval > 9)
			*p++ = (char)(digval - 10 + 'a'); /* a letter */
		else
			*p++ = (char)(digval + '0'); /* a digit */
	} while (val > 0);

	/* We now have the digit of the number in the buffer, but in reverse
	 order.  Thus we reverse them now. */
	*p-- = '\0'; /* terminate string; p points to last digit */
	do
	{
		temp = *p;
		*p = *firstdig;
		*firstdig = temp; /* swap *p and *firstdig */
		--p;
		++firstdig;         /* advance to next two digits */
	} while (firstdig < p); /* repeat until halfway */
}
#endif

#ifndef RESOURCE_MUTEX
#define RESOURCE_MUTEX Mutex
#endif

#ifndef RESOURCE_BEST_FIT
/**
 Main parameter for function assessing how good is a free suballocation for a new
 allocation request.

 - Set to 1 to use Best-Fit algorithm - prefer smaller blocks, as close to the
 size of requested allocations as possible.
 - Set to 0 to use Worst-Fit algorithm - prefer larger blocks, as large as
 possible.

 Experiments in special testing environment showed that Best-Fit algorithm is
 better.
 */
#define RESOURCE_BEST_FIT (1)
#endif

#ifndef RESOURCE_DEBUG_ALWAYS_OWN_MEMORY
/**
 Every object will have its own allocation.
 Define to 1 for debugging purposes only.
 */
#define RESOURCE_DEBUG_ALWAYS_OWN_MEMORY \
	(0)    // NOTE: Set this to 1 on Intel GPUs. It seems like suballocating buffers from heaps can give us problems on Intel hardware.
#endif

#ifndef RESOURCE_DEBUG_ALIGNMENT
/**
 Minimum alignment of all suballocations, in bytes.
 Set to more than 1 for debugging purposes only. Must be power of two.
 */
#define RESOURCE_DEBUG_ALIGNMENT (1)
#endif

#ifndef RESOURCE_DEBUG_MARGIN
/**
 Minimum margin between suballocations, in bytes.
 Set nonzero for debugging purposes only.
 */
#define RESOURCE_DEBUG_MARGIN (0)
#endif

#ifndef RESOURCE_DEBUG_GLOBAL_MUTEX
/**
 Set this to 1 for debugging purposes only, to enable single mutex protecting all
 entry calls to the library. Can be useful for debugging multithreading issues.
 */
#define RESOURCE_DEBUG_GLOBAL_MUTEX (0)
#endif

#ifndef RESOURCE_DEBUG_MIN_BUFFER_IMAGE_GRANULARITY
/**
 Minimum value for VkPhysicalDeviceLimits::bufferImageGranularity.
 Set to more than 1 for debugging purposes only. Must be power of two.
 */
#define RESOURCE_DEBUG_MIN_BUFFER_IMAGE_GRANULARITY (1)
#endif

#ifndef RESOURCE_SMALL_HEAP_MAX_SIZE
/// Maximum size of a memory heap in Metal to consider it "small".
#define RESOURCE_SMALL_HEAP_MAX_SIZE (512 * 1024 * 1024)
#endif

#ifndef RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE
/// Default size of a block allocated as single VkDeviceMemory from a "large" heap.
#define RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE (64 * 1024 * 1024)    // 64 MB
#endif

#ifndef RESOURCE_DEFAULT_SMALL_HEAP_BLOCK_SIZE
/// Default size of a block allocated as single VkDeviceMemory from a "small" heap.
#define RESOURCE_DEFAULT_SMALL_HEAP_BLOCK_SIZE (16 * 1024 * 1024)    // 16 MB
#endif

// -------------------------------------------------------------------------------------------------
// General enums and structs
// -------------------------------------------------------------------------------------------------

/// Allocation memory type.
typedef enum AllocatorMemoryType
{
	RESOURCE_MEMORY_TYPE_DEFAULT_BUFFER = 0,
	RESOURCE_MEMORY_TYPE_UPLOAD_BUFFER,
	RESOURCE_MEMORY_TYPE_READBACK_BUFFER,
	RESOURCE_MEMORY_TYPE_TEXTURE_SMALL,
	RESOURCE_MEMORY_TYPE_TEXTURE_DEFAULT,
	RESOURCE_MEMORY_TYPE_TEXTURE_MS,
	RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV,
	RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_MS,
	RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_SHARED,
	RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_SHARED_MS,
	RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_SHARED_ADAPTER,
	RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_SHARED_ADAPTER_MS,
	RESOURCE_MEMORY_TYPE_DEFAULT_UAV,
	RESOURCE_MEMORY_TYPE_UPLOAD_UAV,
	RESOURCE_MEMORY_TYPE_READBACK_UAV,
	RESOURCE_MEMORY_TYPE_NUM_TYPES
} AllocatorMemoryType;

/// Suballocation type.
typedef enum AllocatorSuballocationType
{
	RESOURCE_SUBALLOCATION_TYPE_BUFFER = 2,
	RESOURCE_SUBALLOCATION_TYPE_FREE = 0,
	RESOURCE_SUBALLOCATION_TYPE_UNKNOWN = 1,
	RESOURCE_SUBALLOCATION_TYPE_BUFFER_SRV_UAV = 3,
	RESOURCE_SUBALLOCATION_TYPE_IMAGE_UNKNOWN = 4,
	RESOURCE_SUBALLOCATION_TYPE_IMAGE_LINEAR = 5,
	RESOURCE_SUBALLOCATION_TYPE_IMAGE_OPTIMAL = 6,
	RESOURCE_SUBALLOCATION_TYPE_IMAGE_RTV_DSV = 7,
	RESOURCE_SUBALLOCATION_TYPE_IMAGE_RTV_DSV_SHARED = 8,
	RESOURCE_SUBALLOCATION_TYPE_IMAGE_RTV_DSV_SHARED_ADAPTER = 9,
	RESOURCE_SUBALLOCATION_TYPE_MAX_ENUM = 0x7FFFFFFF
} AllocatorSuballocationType;

// Size and properties for the MTLResourceHeaps allocated for each resource type.
typedef struct AllocatorHeapProperties
{
	uint64_t        mBlockSize;
	MTLStorageMode  mStorageMode;
	MTLCPUCacheMode mCPUCacheMode;
	const char*     pName;
} AllocatorHeapProperties;

static const AllocatorHeapProperties gHeapProperties[RESOURCE_MEMORY_TYPE_NUM_TYPES] = {
	/// Default Buffer
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		MTLStorageModePrivate,
		MTLCPUCacheModeDefaultCache,
		"Default Buffers Heap",
	},
	// Upload Buffer
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		MTLStorageModeShared,
		MTLCPUCacheModeWriteCombined,
		"Upload Buffers Heap",
	},
	// Readback Buffer
	{
		RESOURCE_DEFAULT_SMALL_HEAP_BLOCK_SIZE,
		MTLStorageModeShared,
		MTLCPUCacheModeDefaultCache,
		"Readback Buffers Heap",
	},
	/// Texture Small
	{
		RESOURCE_DEFAULT_SMALL_HEAP_BLOCK_SIZE,
		MTLStorageModePrivate,
		MTLCPUCacheModeDefaultCache,
		"Small Textures Heap",
	},
	/// Texture Default
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		MTLStorageModePrivate,
		MTLCPUCacheModeDefaultCache,
		"Default Textures Heap",
	},
	/// Texture MSAA
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		MTLStorageModePrivate,
		MTLCPUCacheModeDefaultCache,
		"MSAA Textures Heap",
	},
	/// RTV DSV
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		MTLStorageModePrivate,
		MTLCPUCacheModeDefaultCache,
		"RenderTargets Heap",
	},
	/// RTV DSV MSAA
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		MTLStorageModePrivate,
		MTLCPUCacheModeDefaultCache,
		"MSAA RenderTargets Heap",
	},
	/// RTV DSV Shared
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		MTLStorageModePrivate,
		MTLCPUCacheModeDefaultCache,
		"Shared RenderTargets Heap",
	},
	/// RTV DSV Shared MSAA
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		MTLStorageModePrivate,
		MTLCPUCacheModeDefaultCache,
		"Shared MSAA RenderTargets Heap",
	},
	/// RTV DSV Shared Adapter
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		MTLStorageModePrivate,
		MTLCPUCacheModeDefaultCache,
		"Shared Adapter RenderTargets Heap",
	},
	/// RTV DSV Shared Adapter MSAA
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		MTLStorageModePrivate,
		MTLCPUCacheModeDefaultCache,
		"Shared Adapter MSAA RenderTargets Heap",
	},
	/// Default UAV Buffer
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		MTLStorageModePrivate,
		MTLCPUCacheModeDefaultCache,
		"Default UAV Buffers Heap",
	},
	/// Upload UAV Buffer
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		MTLStorageModeShared,
		MTLCPUCacheModeWriteCombined,
		"Upload UAV Buffers Heap",
	},
	/// Readback UAV Buffer
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		MTLStorageModeShared,
		MTLCPUCacheModeDefaultCache,
		"Readback UAV Buffers Heap",
	},
};

// Info needed for each Metal allocation.
typedef struct AllocationInfo
{
	MTLSizeAndAlign mSizeAlign;
	bool            mIsRT;
	bool            mIsMS;
} AllocationInfo;

// Minimum size of a free suballocation to register it in the free suballocation collection.
static const uint64_t RESOURCE_MIN_FREE_SUBALLOCATION_SIZE_TO_REGISTER = 16;

/// Flags for created Allocator.
typedef enum AllocatorFlagBits
{
	/** Allocator and all objects created from it will not be synchronized internally, so you must guarantee they are used from only one thread at a time or synchronized externally by you.
	 Using this flag may increase performance because internal mutexes are not used.*/
	RESOURCE_ALLOCATOR_EXTERNALLY_SYNCHRONIZED_BIT = 0x00000001,
	RESOURCE_ALLOCATOR_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} AllocatorFlagBits;
typedef uint32_t AllocatorFlags;

/// Description of a Allocator to be created.
typedef struct AllocatorCreateInfo
{
	/// Flags for created allocator. Use AllocatorFlagBits enum.
	AllocatorFlags flags;
	/// Metal device.
	/** It must be valid throughout whole lifetime of created Allocator. */
	id<MTLDevice> device;
	/// Size of a single memory block to allocate for resources.
	/** Set to 0 to use default, which is currently 256 MB. */
	uint64_t preferredLargeHeapBlockSize;
	/// Size of a single memory block to allocate for resources from a small heap <= 512 MB.
	/** Set to 0 to use default, which is currently 64 MB. */
	uint64_t preferredSmallHeapBlockSize;
} AllocatorCreateInfo;

/// Struct containing resource allocation statistics.
typedef struct AllocatorStatInfo
{
	uint32_t AllocationCount;
	uint32_t SuballocationCount;
	uint32_t UnusedRangeCount;
	uint64_t UsedBytes;
	uint64_t UnusedBytes;
	uint64_t SuballocationSizeMin, SuballocationSizeAvg, SuballocationSizeMax;
	uint64_t UnusedRangeSizeMin, UnusedRangeSizeAvg, UnusedRangeSizeMax;
} AllocatorStatInfo;

/// General statistics from current state of Allocator.
struct AllocatorStats
{
	AllocatorStatInfo memoryType[RESOURCE_MEMORY_TYPE_NUM_TYPES];
	AllocatorStatInfo memoryHeap[RESOURCE_MEMORY_TYPE_NUM_TYPES];
	AllocatorStatInfo total;
};

/// Flags to be passed as AllocatorMemoryRequirements::flags.
// TODO: Review this enum.
typedef enum AllocatorMemoryRequirementFlagBits
{
	/** Set this flag if the allocation should have its own memory block.

	 Use it for special, big resources, like fullscreen images used as attachments.

	 This flag must also be used for host visible resources that you want to map
	 simultaneously because otherwise they might end up as regions of the same
	 VkDeviceMemory, while mapping same VkDeviceMemory multiple times is illegal.
	 */
	RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT = 0x00000001,

	/** \brief Set this flag to only try to allocate from existing VkDeviceMemory blocks and never create new such block.

	 If new allocation cannot be placed in any of the existing blocks, allocation
	 fails with VK_ERROR_OUT_OF_DEVICE_MEMORY error.

	 It makes no sense to set RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT and
	 RESOURCE_MEMORY_REQUIREMENT_NEVER_ALLOCATE_BIT at the same time. */
	RESOURCE_MEMORY_REQUIREMENT_NEVER_ALLOCATE_BIT = 0x00000002,
	/** \brief Set to use a memory that will be persistently mapped and retrieve pointer to it.

	 Pointer to mapped memory will be returned through AllocatorAllocationInfo::pMappedData. You cannot
	 map the memory on your own as multiple maps of a single VkDeviceMemory are
	 illegal.
	 */
	RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT = 0x00000004,
	/** \brief Set to use a memory that can be shared with multiple processes.
	 */
	RESOURCE_MEMORY_REQUIREMENT_SHARED_BIT = 0x00000008,
	/** \brief Set to use a memory that can be shared with multiple gpus.
	 */
	RESOURCE_MEMORY_REQUIREMENT_SHARED_ADAPTER_BIT = 0x00000010,
	RESOURCE_MEMORY_REQUIREMENT_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} AllocatorMemoryRequirementFlagBits;
typedef uint32_t AllocatorMemoryRequirementFlags;

typedef struct AllocatorMemoryRequirements
{
	AllocatorMemoryRequirementFlags flags;
	ResourceMemoryUsage             usage;
	void*                           pUserData;
} AllocatorMemoryRequirements;

/** Parameters of AllocatorAllocation objects, that can be retrieved using function resourceAllocGetAllocationInfo().*/
typedef struct ResourceAllocationInfo
{
	/** \brief Memory type index that this allocation was allocated from.

	 It never changes.
	 */
	uint32_t memoryType;
	/** \brief Handle to Metal Heap.

	 Same memory object can be shared by multiple allocations.

	 It can change after call to resourceAllocDefragment() if this allocation is passed to the function.
	 */
	id<MTLHeap> deviceMemory;
	/** \brief Handle to Metal Buffer.

	 Same memory object can be shared by multiple allocations.

	 It can change after call to resourceAllocDefragment() if this allocation is passed to the function.
	 */
	id<MTLBuffer> resource;
	/** \brief Offset into deviceMemory object to the beginning of this allocation, in bytes. (deviceMemory, offset) pair is unique to this allocation.

	 It can change after call to resourceAllocDefragment() if this allocation is passed to the function.
	 */
	uint64_t offset;
	/** \brief Size of this allocation, in bytes.

	 It never changes.
	 */
	uint64_t size;
	/** \brief Pointer to the beginning of this allocation as mapped data. Null if this alloaction is not persistently mapped.

	 It can change after call to resourceAllocUnmapPersistentlyMappedMemory(), resourceAllocMapPersistentlyMappedMemory().
	 It can also change after call to resourceAllocDefragment() if this allocation is passed to the function.
	 */
	void* pMappedData;
	/** \brief Custom general-purpose pointer that was passed as AllocatorMemoryRequirements::pUserData or set using resourceAllocSetAllocationUserData().

	 It can change after call to resourceAllocSetAllocationUserData() for this allocation.
	 */
	void* pUserData;
} ResourceAllocationInfo;

// -------------------------------------------------------------------------------------------------
// Helper functions and classes
// -------------------------------------------------------------------------------------------------

// Aligns given value up to nearest multiply of align value. For example: AllocatorAlignUp(11, 8) = 16.
// Use types like uint32_t, uint64_t as T.
template <typename T>
static inline T AllocatorAlignUp(T val, T align)
{
	return (val + align - 1) / align * align;
}

// Division with mathematical rounding to nearest number.
template <typename T>
inline T AllocatorRoundDiv(T x, T y)
{
	return (x + (y / (T)2)) / y;
}

#ifndef RESOURCE_SORT

template <typename Iterator, typename Compare>
Iterator AllocatorQuickSortPartition(Iterator beg, Iterator end, Compare cmp)
{
	Iterator centerValue = end;
	--centerValue;
	Iterator insertIndex = beg;
	for (Iterator i = beg; i < centerValue; ++i)
	{
		if (cmp(*i, *centerValue))
		{
			if (insertIndex != i)
			{
				RESOURCE_SWAP(*i, *insertIndex);
			}
			++insertIndex;
		}
	}
	if (insertIndex != centerValue)
	{
		RESOURCE_SWAP(*insertIndex, *centerValue);
	}
	return insertIndex;
}

template <typename Iterator, typename Compare>
void AllocatorQuickSort(Iterator beg, Iterator end, Compare cmp)
{
	if (beg < end)
	{
		Iterator it = AllocatorQuickSortPartition<Iterator, Compare>(beg, end, cmp);
		AllocatorQuickSort<Iterator, Compare>(beg, it, cmp);
		AllocatorQuickSort<Iterator, Compare>(it + 1, end, cmp);
	}
}

#define RESOURCE_SORT(beg, end, cmp) AllocatorQuickSort(beg, end, cmp)

#endif    // #ifndef RESOURCE_SORT

/*
 Returns true if two memory blocks occupy overlapping pages.
 ResourceA must be in less memory offset than ResourceB.

 Algorithm is based on "Vulkan 1.0.39 - A Specification (with all registered Vulkan extensions)"
 chapter 11.6 "Resource Memory Association", paragraph "Buffer-Image Granularity".
 */
static inline bool AllocatorBlocksOnSamePage(uint64_t resourceAOffset, uint64_t resourceASize, uint64_t resourceBOffset, uint64_t pageSize)
{
	ASSERT(resourceAOffset + resourceASize <= resourceBOffset && resourceASize > 0 && pageSize > 0);
	uint64_t resourceAEnd = resourceAOffset + resourceASize - 1;
	uint64_t resourceAEndPage = resourceAEnd & ~(pageSize - 1);
	uint64_t resourceBStart = resourceBOffset;
	uint64_t resourceBStartPage = resourceBStart & ~(pageSize - 1);
	return resourceAEndPage == resourceBStartPage;
}

/*
 Returns true if given suballocation types could conflict and must respect
 VkPhysicalDeviceLimits::bufferImageGranularity. They conflict if one is buffer
 or linear image and another one is optimal image. If type is unknown, behave
 conservatively.
 */
// TODO: Review this function.
static inline bool
	AllocatorIsBufferImageGranularityConflict(AllocatorSuballocationType suballocType1, AllocatorSuballocationType suballocType2)
{
	if (suballocType1 > suballocType2)
	{
		RESOURCE_SWAP(suballocType1, suballocType2);
	}

	switch (suballocType1)
	{
		case RESOURCE_SUBALLOCATION_TYPE_FREE: return false;
		case RESOURCE_SUBALLOCATION_TYPE_UNKNOWN: return true;
		case RESOURCE_SUBALLOCATION_TYPE_BUFFER:
			return suballocType2 == RESOURCE_SUBALLOCATION_TYPE_IMAGE_UNKNOWN || suballocType2 == RESOURCE_SUBALLOCATION_TYPE_IMAGE_OPTIMAL;
		case RESOURCE_SUBALLOCATION_TYPE_IMAGE_UNKNOWN:
			return suballocType2 == RESOURCE_SUBALLOCATION_TYPE_IMAGE_UNKNOWN ||
				   suballocType2 == RESOURCE_SUBALLOCATION_TYPE_IMAGE_LINEAR || suballocType2 == RESOURCE_SUBALLOCATION_TYPE_IMAGE_OPTIMAL;
		case RESOURCE_SUBALLOCATION_TYPE_IMAGE_LINEAR: return suballocType2 == RESOURCE_SUBALLOCATION_TYPE_IMAGE_OPTIMAL;
		case RESOURCE_SUBALLOCATION_TYPE_IMAGE_OPTIMAL: return false;
		default: ASSERT(0); return true;
	}
}

// Helper RAII class to lock a mutex in constructor and unlock it in destructor (at the end of scope).
struct AllocatorMutexLock
{
	public:
	AllocatorMutexLock(RESOURCE_MUTEX& mutex, bool useMutex): m_pMutex(useMutex ? &mutex : RESOURCE_NULL)
	{
		if (m_pMutex)
		{
			m_pMutex->Acquire();
		}
	}

	~AllocatorMutexLock()
	{
		if (m_pMutex)
		{
			m_pMutex->Release();
		}
	}

	private:
	RESOURCE_MUTEX* m_pMutex;
};

#if RESOURCE_DEBUG_GLOBAL_MUTEX
static RESOURCE_MUTEX gDebugGlobalMutex;
#define RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK AllocatorMutexLock debugGlobalMutexLock(gDebugGlobalMutex);
#else
#define RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK
#endif

/*
 Performs binary search and returns iterator to first element that is greater or
 equal to (key), according to comparison (cmp).

 Cmp should return true if first argument is less than second argument.

 Returned value is the found element, if present in the collection or place where
 new element with value (key) should be inserted.
 */
template <typename IterT, typename KeyT, typename CmpT>
static IterT AllocatorBinaryFindFirstNotLess(IterT beg, IterT end, const KeyT& key, CmpT cmp)
{
	size_t down = 0, up = (end - beg);
	while (down < up)
	{
		const size_t mid = (down + up) / 2;
		if (cmp(*(beg + mid), key))
		{
			down = mid + 1;
		}
		else
		{
			up = mid;
		}
	}
	return beg + down;
}

// -------------------------------------------------------------------------------------------------
// Memory allocation functions.
// -------------------------------------------------------------------------------------------------

static void* AllocatorMalloc(size_t size) { return RESOURCE_SYSTEM_ALIGNED_MALLOC(size); }

static void AllocatorFree(void* ptr) { RESOURCE_SYSTEM_FREE(ptr); }

template <typename T>
static T* AllocatorAllocate()
{
	return (T*)AllocatorMalloc(sizeof(T));
}

template <typename T>
static T* AllocatorAllocateArray(size_t count)
{
	return (T*)AllocatorMalloc(sizeof(T) * count);
}

template <typename T>
static void resourceAlloc_delete(T* ptr)
{
	ptr->~T();
	AllocatorFree(ptr);
}

template <typename T>
static void resourceAlloc_delete_array(T* ptr, size_t count)
{
	if (ptr != RESOURCE_NULL)
	{
		for (size_t i = count; i--;)
		{
			ptr[i].~T();
		}
		AllocatorFree(ptr);
	}
}

#define AllocatorVector tinystl::vector

template <typename T>
static void VectorInsert(AllocatorVector<T>& vec, size_t index, const T& item)
{
	vec.insert(vec.begin() + index, item);
}

template <typename T>
static void VectorRemove(AllocatorVector<T>& vec, size_t index)
{
	vec.erase(vec.begin() + index);
}

#define AllocatorPair tinystl::pair
#define RESOURCE_MAP_TYPE(KeyT, ValueT) tinystl::unordered_map<KeyT, ValueT>

// -------------------------------------------------------------------------------------------------
// Allocator pool class.
// -------------------------------------------------------------------------------------------------

/*
 Allocator for objects of type T using a list of arrays (pools) to speed up
 allocation. Number of elements that can be allocated is not bounded because
 allocator can create multiple blocks.
 */
template <typename T>
class AllocatorPoolAllocator
{
	public:
	AllocatorPoolAllocator(size_t itemsPerBlock);
	~AllocatorPoolAllocator();
	void Clear();
	T*   Alloc();
	void Free(T* ptr);

	private:
	union Item
	{
		Item(){};
		~Item(){};

		uint32_t NextFreeIndex;
		T        Value;
	};

	struct ItemBlock
	{
		Item*    pItems;
		uint32_t FirstFreeIndex;
	};

	size_t                     m_ItemsPerBlock;
	AllocatorVector<ItemBlock> m_ItemBlocks;

	ItemBlock& CreateNewBlock();
};

template <typename T>
AllocatorPoolAllocator<T>::AllocatorPoolAllocator(size_t itemsPerBlock): m_ItemsPerBlock(itemsPerBlock)
{
	ASSERT(itemsPerBlock > 0);
}

template <typename T>
AllocatorPoolAllocator<T>::~AllocatorPoolAllocator()
{
	Clear();
}

template <typename T>
void AllocatorPoolAllocator<T>::Clear()
{
	for (size_t i = m_ItemBlocks.size(); i--;)
		resourceAlloc_delete_array(m_ItemBlocks[i].pItems, m_ItemsPerBlock);
	m_ItemBlocks.clear();
}

template <typename T>
T* AllocatorPoolAllocator<T>::Alloc()
{
	for (size_t i = m_ItemBlocks.size(); i--;)
	{
		ItemBlock& block = m_ItemBlocks[i];
		// This block has some free items: Use first one.
		if (block.FirstFreeIndex != UINT32_MAX)
		{
			Item* const pItem = &block.pItems[block.FirstFreeIndex];
			block.FirstFreeIndex = pItem->NextFreeIndex;
			return &pItem->Value;
		}
	}

	// No block has free item: Create new one and use it.
	ItemBlock&  newBlock = CreateNewBlock();
	Item* const pItem = &newBlock.pItems[0];
	newBlock.FirstFreeIndex = pItem->NextFreeIndex;
	return &pItem->Value;
}

template <typename T>
void AllocatorPoolAllocator<T>::Free(T* ptr)
{
	// Search all memory blocks to find ptr.
	for (size_t i = 0; i < m_ItemBlocks.size(); ++i)
	{
		ItemBlock& block = m_ItemBlocks[i];

		// Casting to union.
		Item* pItemPtr;
		memcpy(&pItemPtr, &ptr, sizeof(pItemPtr));

		// Check if pItemPtr is in address range of this block.
		if ((pItemPtr >= block.pItems) && (pItemPtr < block.pItems + m_ItemsPerBlock))
		{
			const uint32_t index = static_cast<uint32_t>(pItemPtr - block.pItems);
			pItemPtr->NextFreeIndex = block.FirstFreeIndex;
			block.FirstFreeIndex = index;
			return;
		}
	}
	ASSERT(0 && "Pointer doesn't belong to this memory pool.");
}

template <typename T>
typename AllocatorPoolAllocator<T>::ItemBlock& AllocatorPoolAllocator<T>::CreateNewBlock()
{
	ItemBlock newBlock = { conf_placement_new<Item>(AllocatorAllocateArray<Item>((m_ItemsPerBlock))), 0 };

	m_ItemBlocks.push_back(newBlock);

	// Setup singly-linked list of all free items in this block.
	for (uint32_t i = 0; i < m_ItemsPerBlock - 1; ++i)
		newBlock.pItems[i].NextFreeIndex = i + 1;
	newBlock.pItems[m_ItemsPerBlock - 1].NextFreeIndex = UINT32_MAX;
	return m_ItemBlocks.back();
}

// -------------------------------------------------------------------------------------------------
// AllocatorRawList and AllocatorList classes.
// -------------------------------------------------------------------------------------------------

template <typename T>
struct AllocatorListItem
{
	AllocatorListItem* pPrev;
	AllocatorListItem* pNext;
	T                  Value;
};

// Doubly linked list.
template <typename T>
class AllocatorRawList
{
	public:
	typedef AllocatorListItem<T> ItemType;

	AllocatorRawList();
	~AllocatorRawList();
	void Clear();

	size_t GetCount() const { return m_Count; }
	bool   IsEmpty() const { return m_Count == 0; }

	ItemType*       Front() { return m_pFront; }
	const ItemType* Front() const { return m_pFront; }
	ItemType*       Back() { return m_pBack; }
	const ItemType* Back() const { return m_pBack; }

	ItemType* PushBack();
	ItemType* PushFront();
	ItemType* PushBack(const T& value);
	ItemType* PushFront(const T& value);
	void      PopBack();
	void      PopFront();

	// Item can be null - it means PushBack.
	ItemType* InsertBefore(ItemType* pItem);
	// Item can be null - it means PushFront.
	ItemType* InsertAfter(ItemType* pItem);

	ItemType* InsertBefore(ItemType* pItem, const T& value);
	ItemType* InsertAfter(ItemType* pItem, const T& value);

	void Remove(ItemType* pItem);

	private:
	AllocatorPoolAllocator<ItemType> m_ItemAllocator;
	ItemType*                        m_pFront;
	ItemType*                        m_pBack;
	size_t                           m_Count;

	// Declared not defined, to block copy constructor and assignment operator.
	AllocatorRawList(const AllocatorRawList<T>& src);
	AllocatorRawList<T>& operator=(const AllocatorRawList<T>& rhs);
};

template <typename T>
AllocatorRawList<T>::AllocatorRawList(): m_ItemAllocator(128), m_pFront(RESOURCE_NULL), m_pBack(RESOURCE_NULL), m_Count(0)
{
}

template <typename T>
AllocatorRawList<T>::~AllocatorRawList()
{
	// Intentionally not calling Clear, because that would be unnecessary
	// computations to return all items to m_ItemAllocator as free.
}

template <typename T>
void AllocatorRawList<T>::Clear()
{
	if (IsEmpty() == false)
	{
		ItemType* pItem = m_pBack;
		while (pItem != RESOURCE_NULL)
		{
			ItemType* const pPrevItem = pItem->pPrev;
			m_ItemAllocator.Free(pItem);
			pItem = pPrevItem;
		}
		m_pFront = RESOURCE_NULL;
		m_pBack = RESOURCE_NULL;
		m_Count = 0;
	}
}

template <typename T>
AllocatorListItem<T>* AllocatorRawList<T>::PushBack()
{
	ItemType* const pNewItem = m_ItemAllocator.Alloc();
	pNewItem->pNext = RESOURCE_NULL;
	if (IsEmpty())
	{
		pNewItem->pPrev = RESOURCE_NULL;
		m_pFront = pNewItem;
		m_pBack = pNewItem;
		m_Count = 1;
	}
	else
	{
		pNewItem->pPrev = m_pBack;
		m_pBack->pNext = pNewItem;
		m_pBack = pNewItem;
		++m_Count;
	}
	return pNewItem;
}

template <typename T>
AllocatorListItem<T>* AllocatorRawList<T>::PushFront()
{
	ItemType* const pNewItem = m_ItemAllocator.Alloc();
	pNewItem->pPrev = RESOURCE_NULL;
	if (IsEmpty())
	{
		pNewItem->pNext = RESOURCE_NULL;
		m_pFront = pNewItem;
		m_pBack = pNewItem;
		m_Count = 1;
	}
	else
	{
		pNewItem->pNext = m_pFront;
		m_pFront->pPrev = pNewItem;
		m_pFront = pNewItem;
		++m_Count;
	}
	return pNewItem;
}

template <typename T>
AllocatorListItem<T>* AllocatorRawList<T>::PushBack(const T& value)
{
	ItemType* const pNewItem = PushBack();
	pNewItem->Value = value;
	return pNewItem;
}

template <typename T>
AllocatorListItem<T>* AllocatorRawList<T>::PushFront(const T& value)
{
	ItemType* const pNewItem = PushFront();
	pNewItem->Value = value;
	return pNewItem;
}

template <typename T>
void AllocatorRawList<T>::PopBack()
{
	RESOURCE_HEAVY_ASSERT(m_Count > 0);
	ItemType* const pBackItem = m_pBack;
	ItemType* const pPrevItem = pBackItem->pPrev;
	if (pPrevItem != RESOURCE_NULL)
	{
		pPrevItem->pNext = RESOURCE_NULL;
	}
	m_pBack = pPrevItem;
	m_ItemAllocator.Free(pBackItem);
	--m_Count;
}

template <typename T>
void AllocatorRawList<T>::PopFront()
{
	RESOURCE_HEAVY_ASSERT(m_Count > 0);
	ItemType* const pFrontItem = m_pFront;
	ItemType* const pNextItem = pFrontItem->pNext;
	if (pNextItem != RESOURCE_NULL)
	{
		pNextItem->pPrev = RESOURCE_NULL;
	}
	m_pFront = pNextItem;
	m_ItemAllocator.Free(pFrontItem);
	--m_Count;
}

template <typename T>
void AllocatorRawList<T>::Remove(ItemType* pItem)
{
	RESOURCE_HEAVY_ASSERT(pItem != RESOURCE_NULL);
	RESOURCE_HEAVY_ASSERT(m_Count > 0);

	if (pItem->pPrev != RESOURCE_NULL)
	{
		pItem->pPrev->pNext = pItem->pNext;
	}
	else
	{
		RESOURCE_HEAVY_ASSERT(m_pFront == pItem);
		m_pFront = pItem->pNext;
	}

	if (pItem->pNext != RESOURCE_NULL)
	{
		pItem->pNext->pPrev = pItem->pPrev;
	}
	else
	{
		RESOURCE_HEAVY_ASSERT(m_pBack == pItem);
		m_pBack = pItem->pPrev;
	}

	m_ItemAllocator.Free(pItem);
	--m_Count;
}

template <typename T>
AllocatorListItem<T>* AllocatorRawList<T>::InsertBefore(ItemType* pItem)
{
	if (pItem != RESOURCE_NULL)
	{
		ItemType* const prevItem = pItem->pPrev;
		ItemType* const newItem = m_ItemAllocator.Alloc();
		newItem->pPrev = prevItem;
		newItem->pNext = pItem;
		pItem->pPrev = newItem;
		if (prevItem != RESOURCE_NULL)
		{
			prevItem->pNext = newItem;
		}
		else
		{
			RESOURCE_HEAVY_ASSERT(m_pFront == pItem);
			m_pFront = newItem;
		}
		++m_Count;
		return newItem;
	}
	else
		return PushBack();
}

template <typename T>
AllocatorListItem<T>* AllocatorRawList<T>::InsertAfter(ItemType* pItem)
{
	if (pItem != RESOURCE_NULL)
	{
		ItemType* const nextItem = pItem->pNext;
		ItemType* const newItem = m_ItemAllocator.Alloc();
		newItem->pNext = nextItem;
		newItem->pPrev = pItem;
		pItem->pNext = newItem;
		if (nextItem != RESOURCE_NULL)
		{
			nextItem->pPrev = newItem;
		}
		else
		{
			RESOURCE_HEAVY_ASSERT(m_pBack == pItem);
			m_pBack = newItem;
		}
		++m_Count;
		return newItem;
	}
	else
		return PushFront();
}

template <typename T>
AllocatorListItem<T>* AllocatorRawList<T>::InsertBefore(ItemType* pItem, const T& value)
{
	ItemType* const newItem = InsertBefore(pItem);
	newItem->Value = value;
	return newItem;
}

template <typename T>
AllocatorListItem<T>* AllocatorRawList<T>::InsertAfter(ItemType* pItem, const T& value)
{
	ItemType* const newItem = InsertAfter(pItem);
	newItem->Value = value;
	return newItem;
}

template <typename T>
class AllocatorList
{
	public:
	// Forward declarations.
	class iterator;
	class const_iterator;

	class iterator
	{
		public:
		iterator(): m_pList(RESOURCE_NULL), m_pItem(RESOURCE_NULL) {}

		T& operator*() const
		{
			RESOURCE_HEAVY_ASSERT(m_pItem != RESOURCE_NULL);
			return m_pItem->Value;
		}
		T* operator->() const
		{
			RESOURCE_HEAVY_ASSERT(m_pItem != RESOURCE_NULL);
			return &m_pItem->Value;
		}

		iterator& operator++()
		{
			RESOURCE_HEAVY_ASSERT(m_pItem != RESOURCE_NULL);
			m_pItem = m_pItem->pNext;
			return *this;
		}
		iterator& operator--()
		{
			if (m_pItem != RESOURCE_NULL)
			{
				m_pItem = m_pItem->pPrev;
			}
			else
			{
				RESOURCE_HEAVY_ASSERT(!m_pList.IsEmpty());
				m_pItem = m_pList->Back();
			}
			return *this;
		}

		iterator operator++(int)
		{
			iterator result = *this;
			++*this;
			return result;
		}
		iterator operator--(int)
		{
			iterator result = *this;
			--*this;
			return result;
		}

		bool operator==(const iterator& rhs) const
		{
			RESOURCE_HEAVY_ASSERT(m_pList == rhs.m_pList);
			return m_pItem == rhs.m_pItem;
		}
		bool operator!=(const iterator& rhs) const
		{
			RESOURCE_HEAVY_ASSERT(m_pList == rhs.m_pList);
			return m_pItem != rhs.m_pItem;
		}

		private:
		AllocatorRawList<T>*  m_pList;
		AllocatorListItem<T>* m_pItem;

		iterator(AllocatorRawList<T>* pList, AllocatorListItem<T>* pItem): m_pList(pList), m_pItem(pItem) {}

		friend class AllocatorList<T>;
		friend class AllocatorList<T>::const_iterator;
	};

	class const_iterator
	{
		public:
		const_iterator(): m_pList(RESOURCE_NULL), m_pItem(RESOURCE_NULL) {}

		const_iterator(const iterator& src): m_pList(src.m_pList), m_pItem(src.m_pItem) {}

		const T& operator*() const
		{
			RESOURCE_HEAVY_ASSERT(m_pItem != RESOURCE_NULL);
			return m_pItem->Value;
		}
		const T* operator->() const
		{
			RESOURCE_HEAVY_ASSERT(m_pItem != RESOURCE_NULL);
			return &m_pItem->Value;
		}

		const_iterator& operator++()
		{
			RESOURCE_HEAVY_ASSERT(m_pItem != RESOURCE_NULL);
			m_pItem = m_pItem->pNext;
			return *this;
		}
		const_iterator& operator--()
		{
			if (m_pItem != RESOURCE_NULL)
			{
				m_pItem = m_pItem->pPrev;
			}
			else
			{
				RESOURCE_HEAVY_ASSERT(!m_pList->IsEmpty());
				m_pItem = m_pList->Back();
			}
			return *this;
		}

		const_iterator operator++(int)
		{
			const_iterator result = *this;
			++*this;
			return result;
		}
		const_iterator operator--(int)
		{
			const_iterator result = *this;
			--*this;
			return result;
		}

		bool operator==(const const_iterator& rhs) const
		{
			RESOURCE_HEAVY_ASSERT(m_pList == rhs.m_pList);
			return m_pItem == rhs.m_pItem;
		}
		bool operator!=(const const_iterator& rhs) const
		{
			RESOURCE_HEAVY_ASSERT(m_pList == rhs.m_pList);
			return m_pItem != rhs.m_pItem;
		}

		private:
		const_iterator(const AllocatorRawList<T>* pList, const AllocatorListItem<T>* pItem): m_pList(pList), m_pItem(pItem) {}

		const AllocatorRawList<T>*  m_pList;
		const AllocatorListItem<T>* m_pItem;

		friend class AllocatorList<T>;
	};

	AllocatorList(): m_RawList() {}

	bool   empty() const { return m_RawList.IsEmpty(); }
	size_t size() const { return m_RawList.GetCount(); }

	iterator begin() { return iterator(&m_RawList, m_RawList.Front()); }
	iterator end() { return iterator(&m_RawList, RESOURCE_NULL); }

	const_iterator cbegin() const { return const_iterator(&m_RawList, m_RawList.Front()); }
	const_iterator cend() const { return const_iterator(&m_RawList, RESOURCE_NULL); }

	void     clear() { m_RawList.Clear(); }
	void     push_back(const T& value) { m_RawList.PushBack(value); }
	void     erase(iterator it) { m_RawList.Remove(it.m_pItem); }
	iterator insert(iterator it, const T& value) { return iterator(&m_RawList, m_RawList.InsertBefore(it.m_pItem, value)); }

	private:
	AllocatorRawList<T> m_RawList;
};

// -------------------------------------------------------------------------------------------------
// Resource allocation/suballocation declarations.
// -------------------------------------------------------------------------------------------------

class AllocatorBlock;

enum RESOURCE_BLOCK_VECTOR_TYPE
{
	RESOURCE_BLOCK_VECTOR_TYPE_UNMAPPED,
	RESOURCE_BLOCK_VECTOR_TYPE_MAPPED,
	RESOURCE_BLOCK_VECTOR_TYPE_COUNT
};

static RESOURCE_BLOCK_VECTOR_TYPE AllocatorMemoryRequirementFlagsToBlockVectorType(AllocatorMemoryRequirementFlags flags)
{
	return (flags & RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT) != 0 ? RESOURCE_BLOCK_VECTOR_TYPE_MAPPED
																		 : RESOURCE_BLOCK_VECTOR_TYPE_UNMAPPED;
}

struct OwnAllocation
{
	id<MTLBuffer>  m_Buffer;
	id<MTLTexture> m_Texture;
	uint32_t       m_MemoryTypeIndex;
	bool           m_PersistentMap;
	void*          m_pMappedData;
};

struct BlockAllocation
{
	AllocatorBlock* m_Block;
	uint64_t        m_Offset;
};

struct ResourceAllocation
{
	public:
	enum ALLOCATION_TYPE
	{
		ALLOCATION_TYPE_NONE,
		ALLOCATION_TYPE_BLOCK,
		ALLOCATION_TYPE_OWN,
	};

	ResourceAllocation() { memset(this, 0, sizeof(ResourceAllocation)); }
	~ResourceAllocation(){};

	void InitBlockAllocation(
		AllocatorBlock* block, uint64_t offset, uint64_t alignment, uint64_t size, AllocatorSuballocationType suballocationType,
		void* pUserData)
	{
		ASSERT(m_Type == ALLOCATION_TYPE_NONE);
		ASSERT(block != RESOURCE_NULL);
		m_Type = ALLOCATION_TYPE_BLOCK;
		m_Alignment = alignment;
		m_Size = size;
		m_pUserData = pUserData;
		m_SuballocationType = suballocationType;
		m_BlockAllocation.m_Block = block;
		m_BlockAllocation.m_Offset = offset;
	}

	void ChangeBlockAllocation(AllocatorBlock* block, uint64_t offset)
	{
		ASSERT(block != RESOURCE_NULL);
		ASSERT(m_Type == ALLOCATION_TYPE_BLOCK);
		m_BlockAllocation.m_Block = block;
		m_BlockAllocation.m_Offset = offset;
	}

	void InitOwnAllocation(
		uint32_t memoryTypeIndex, AllocatorSuballocationType suballocationType, bool persistentMap, void* pMappedData, uint64_t size,
		void* pUserData)
	{
		ASSERT(m_Type == ALLOCATION_TYPE_NONE);
		m_Type = ALLOCATION_TYPE_OWN;
		m_Alignment = 0;
		m_Size = size;
		m_pUserData = pUserData;
		m_SuballocationType = suballocationType;
		m_OwnAllocation.m_MemoryTypeIndex = memoryTypeIndex;
		m_OwnAllocation.m_PersistentMap = persistentMap;
		m_OwnAllocation.m_pMappedData = pMappedData;
	}

	ALLOCATION_TYPE            GetType() const { return m_Type; }
	uint64_t                   GetAlignment() const { return m_Alignment; }
	uint64_t                   GetSize() const { return m_Size; }
	void*                      GetUserData() const { return m_pUserData; }
	void                       SetUserData(void* pUserData) { m_pUserData = pUserData; }
	AllocatorSuballocationType GetSuballocationType() const { return m_SuballocationType; }

	AllocatorBlock* GetBlock() const
	{
		ASSERT(m_Type == ALLOCATION_TYPE_BLOCK);
		return m_BlockAllocation.m_Block;
	}
	uint64_t                   GetOffset() const { return (m_Type == ALLOCATION_TYPE_BLOCK) ? m_BlockAllocation.m_Offset : 0; }
	id<MTLHeap>                GetMemory() const;
	id<MTLBuffer>              GetResource() const;
	uint32_t                   GetMemoryTypeIndex() const;
	RESOURCE_BLOCK_VECTOR_TYPE GetBlockVectorType() const;
	void*                      GetMappedData() const;
	OwnAllocation*             GetOwnAllocation() { return &m_OwnAllocation; }

	bool OwnAllocMapPersistentlyMappedMemory()
	{
		ASSERT(m_Type == ALLOCATION_TYPE_OWN);
		if (m_OwnAllocation.m_PersistentMap)
		{
			m_OwnAllocation.m_pMappedData = m_OwnAllocation.m_Buffer.contents;
		}
		return true;
	}
	void OwnAllocUnmapPersistentlyMappedMemory()
	{
		ASSERT(m_Type == ALLOCATION_TYPE_OWN);
		if (m_OwnAllocation.m_pMappedData)
		{
			ASSERT(m_OwnAllocation.m_PersistentMap);
			m_OwnAllocation.m_pMappedData = nil;
		}
	}

	private:
	uint64_t                   m_Alignment;
	uint64_t                   m_Size;
	void*                      m_pUserData;
	ALLOCATION_TYPE            m_Type;
	AllocatorSuballocationType m_SuballocationType;

	union
	{
		// Allocation out of AllocatorBlock.
		BlockAllocation m_BlockAllocation;

		// Allocation for an object that has its own private VkDeviceMemory.
		OwnAllocation m_OwnAllocation;
	};
};

/*
 Represents a region of AllocatorBlock that is either assigned and returned as
 allocated memory block or free.
 */
struct AllocatorSuballocation
{
	//id<MTLBuffer> bufferResource;
	//id<MTLTexture> textureResource;
	void*                      mappedData;
	uint64_t                   offset;
	uint64_t                   size;
	AllocatorSuballocationType type;
};

typedef AllocatorList<AllocatorSuballocation> AllocatorSuballocationList;

// Parameters of an allocation.
struct AllocatorAllocationRequest
{
	AllocatorSuballocationList::iterator freeSuballocationItem;
	uint64_t                             offset;
};

// -------------------------------------------------------------------------------------------------
// AllocatorBlock class declaration.
// -------------------------------------------------------------------------------------------------

/* Single block of memory with all the data about its regions assigned or free. */
class AllocatorBlock
{
	public:
	uint32_t                   m_MemoryTypeIndex;
	RESOURCE_BLOCK_VECTOR_TYPE m_BlockVectorType;
	id<MTLHeap>                m_hMemory;
	id<MTLBuffer>              m_Buffer;
	id<MTLTexture>             m_Texture;
	uint64_t                   m_Size;
	bool                       m_PersistentMap;
	void*                      m_pMappedData;
	uint32_t                   m_FreeCount;
	uint64_t                   m_SumFreeSize;
	AllocatorSuballocationList m_Suballocations;
	// Suballocations that are free and have size greater than certain threshold.
	// Sorted by size, ascending.
	AllocatorVector<AllocatorSuballocationList::iterator> m_FreeSuballocationsBySize;

	AllocatorBlock(ResourceAllocator* hAllocator);

	~AllocatorBlock() { ASSERT(m_hMemory == NULL); }

	// Always call after construction.
	void Init(
		uint32_t newMemoryTypeIndex, RESOURCE_BLOCK_VECTOR_TYPE newBlockVectorType, id<MTLHeap> newMemory, uint64_t newSize,
		bool persistentMap, void* pMappedData);
	void Init(
		uint32_t newMemoryTypeIndex, RESOURCE_BLOCK_VECTOR_TYPE newBlockVectorType, id<MTLBuffer> newMemory, uint64_t newSize,
		bool persistentMap, void* pMappedData);
	void Init(
		uint32_t newMemoryTypeIndex, RESOURCE_BLOCK_VECTOR_TYPE newBlockVectorType, id<MTLTexture> newMemory, uint64_t newSize,
		bool persistentMap, void* pMappedData);
	// Always call before destruction.
	void Destroy(ResourceAllocator* allocator);

	// Validates all data structures inside this object. If not valid, returns false.
	bool Validate() const;

	// Tries to find a place for suballocation with given parameters inside this allocation.
	// If succeeded, fills pAllocationRequest and returns true.
	// If failed, returns false.
	bool CreateAllocationRequest(
		uint64_t bufferImageGranularity, uint64_t allocSize, uint64_t allocAlignment, AllocatorSuballocationType allocType,
		AllocatorAllocationRequest* pAllocationRequest);

	// Checks if requested suballocation with given parameters can be placed in given pFreeSuballocItem.
	// If yes, fills pOffset and returns true. If no, returns false.
	bool CheckAllocation(
		uint64_t bufferImageGranularity, uint64_t allocSize, uint64_t allocAlignment, AllocatorSuballocationType allocType,
		AllocatorSuballocationList::const_iterator freeSuballocItem, uint64_t* pOffset) const;

	// Returns true if this allocation is empty - contains only single free suballocation.
	bool IsEmpty() const;

	// Makes actual allocation based on request. Request must already be checked
	// and valid.
	void Alloc(const AllocatorAllocationRequest& request, AllocatorSuballocationType type, uint64_t allocSize);

	// Frees suballocation assigned to given memory region.
	void Free(const ResourceAllocation* allocation);

#if RESOURCE_STATS_STRING_ENABLED
	void PrintDetailedMap(class AllocatorStringBuilder& sb) const;
#endif

	private:
	// Given free suballocation, it merges it with following one, which must also be free.
	void MergeFreeWithNext(AllocatorSuballocationList::iterator item);
	// Releases given suballocation, making it free. Merges it with adjacent free
	// suballocations if applicable.
	void FreeSuballocation(AllocatorSuballocationList::iterator suballocItem);
	// Given free suballocation, it inserts it into sorted list of
	// m_FreeSuballocationsBySize if it's suitable.
	void RegisterFreeSuballocation(AllocatorSuballocationList::iterator item);
	// Given free suballocation, it removes it from sorted list of
	// m_FreeSuballocationsBySize if it's suitable.
	void UnregisterFreeSuballocation(AllocatorSuballocationList::iterator item);
};

// -------------------------------------------------------------------------------------------------
// AllocatorBlockVector struct declaration.
// -------------------------------------------------------------------------------------------------

/* Sequence of AllocatorBlock. Represents memory blocks allocated for a specific
 Metal memory type. */
struct AllocatorBlockVector
{
	// Incrementally sorted by sumFreeSize, ascending.
	AllocatorVector<AllocatorBlock*> m_Blocks;

	AllocatorBlockVector(ResourceAllocator* hAllocator);
	~AllocatorBlockVector();

	bool IsEmpty() const { return m_Blocks.empty(); }

	// Finds and removes given block from vector.
	void Remove(AllocatorBlock* pBlock);

	// Performs single step in sorting m_Blocks. They may not be fully sorted
	// after this call.
	void IncrementallySortBlocks();

	// Adds statistics of this BlockVector to pStats.
	void AddStats(AllocatorStats* pStats, uint32_t memTypeIndex, uint32_t memHeapIndex) const;

#if RESOURCE_STATS_STRING_ENABLED
	void PrintDetailedMap(class AllocatorStringBuilder& sb) const;
#endif

	void UnmapPersistentlyMappedMemory();
	bool MapPersistentlyMappedMemory();

	private:
	ResourceAllocator* m_hAllocator;
};

// -------------------------------------------------------------------------------------------------
// ResourceAllocator struct declaration.
// -------------------------------------------------------------------------------------------------

// Main allocator object.
struct ResourceAllocator
{
	bool          m_UseMutex;
	id<MTLDevice> m_Device;
	bool          m_AllocationCallbacksSpecified;
	uint64_t      m_PreferredLargeHeapBlockSize;
	uint64_t      m_PreferredSmallHeapBlockSize;
	// Non-zero when we are inside UnmapPersistentlyMappedMemory...MapPersistentlyMappedMemory.
	// Counter to allow nested calls to these functions.
	uint32_t m_UnmapPersistentlyMappedMemoryCounter;

	AllocatorBlockVector* m_pBlockVectors[RESOURCE_MEMORY_TYPE_NUM_TYPES][RESOURCE_BLOCK_VECTOR_TYPE_COUNT];
	/* There can be at most one allocation that is completely empty - a
	 hysteresis to avoid pessimistic case of alternating creation and destruction
	 of a VkDeviceMemory. */
	bool           m_HasEmptyBlock[RESOURCE_MEMORY_TYPE_NUM_TYPES];
	RESOURCE_MUTEX m_BlocksMutex[RESOURCE_MEMORY_TYPE_NUM_TYPES];

	// Each vector is sorted by memory (handle value).
	typedef AllocatorVector<ResourceAllocation*> AllocationVectorType;
	AllocationVectorType*                        m_pOwnAllocations[RESOURCE_MEMORY_TYPE_NUM_TYPES][RESOURCE_BLOCK_VECTOR_TYPE_COUNT];
	RESOURCE_MUTEX                               m_OwnAllocationsMutex[RESOURCE_MEMORY_TYPE_NUM_TYPES];

	ResourceAllocator(const AllocatorCreateInfo* pCreateInfo);
	~ResourceAllocator();

	uint64_t GetPreferredBlockSize(ResourceMemoryUsage memUsage, uint32_t memTypeIndex) const;

	uint64_t GetBufferImageGranularity() const
	{
		return RESOURCE_MAX(static_cast<uint64_t>(RESOURCE_DEBUG_MIN_BUFFER_IMAGE_GRANULARITY), 1);
	}

	uint32_t GetMemoryHeapCount() const { return RESOURCE_MEMORY_TYPE_NUM_TYPES; }
	uint32_t GetMemoryTypeCount() const { return RESOURCE_MEMORY_TYPE_NUM_TYPES; }

	// Main allocation function.
	bool AllocateMemory(
		const AllocationInfo& info, const AllocatorMemoryRequirements& resourceAllocMemReq, AllocatorSuballocationType suballocType,
		ResourceAllocation** pAllocation);

	// Main deallocation function.
	void FreeMemory(ResourceAllocation* allocation);

	void CalculateStats(AllocatorStats* pStats);

#if RESOURCE_STATS_STRING_ENABLED
	void PrintDetailedMap(class AllocatorStringBuilder& sb);
#endif

	void UnmapPersistentlyMappedMemory();
	bool MapPersistentlyMappedMemory();

	static void GetAllocationInfo(ResourceAllocation* hAllocation, ResourceAllocationInfo* pAllocationInfo);

	private:
	bool AllocateMemoryOfType(
		const MTLSizeAndAlign& mtlMemReq, const AllocatorMemoryRequirements& resourceAllocMemReq, uint32_t memTypeIndex,
		AllocatorSuballocationType suballocType, ResourceAllocation** pAllocation);

	// Allocates and registers new VkDeviceMemory specifically for single allocation.
	bool AllocateOwnMemory(
		uint64_t size, AllocatorSuballocationType suballocType, uint32_t memTypeIndex, bool map, void* pUserData,
		ResourceAllocation** pAllocation);

	// Tries to free pMemory as Own Memory. Returns true if found and freed.
	void FreeOwnMemory(ResourceAllocation* allocation);
};

// -------------------------------------------------------------------------------------------------
// AllocatorStringBuilder class.
// -------------------------------------------------------------------------------------------------

#if RESOURCE_STATS_STRING_ENABLED

class AllocatorStringBuilder
{
	public:
	AllocatorStringBuilder(ResourceAllocator* alloc) {}
	size_t      GetLength() const { return m_Data.size(); }
	const char* GetData() const { return m_Data.data(); }

	void Add(char ch) { m_Data.push_back(ch); }
	void Add(const char* pStr);
	void AddNewLine() { Add('\n'); }
	void AddNumber(uint32_t num);
	void AddNumber(uint64_t num);
	void AddBool(bool b) { Add(b ? "true" : "false"); }
	void AddNull() { Add("null"); }
	void AddString(const char* pStr);

	private:
	AllocatorVector<char> m_Data;
};

void AllocatorStringBuilder::Add(const char* pStr)
{
	const size_t strLen = strlen(pStr);
	if (strLen > 0)
	{
		const size_t oldCount = m_Data.size();
		m_Data.resize(oldCount + strLen);
		memcpy(m_Data.data() + oldCount, pStr, strLen);
	}
}

void AllocatorStringBuilder::AddNumber(uint32_t num)
{
	char buf[11];
	AllocatorUint32ToStr(buf, sizeof(buf), num);
	Add(buf);
}

void AllocatorStringBuilder::AddNumber(uint64_t num)
{
	char buf[21];
	AllocatorUint64ToStr(buf, sizeof(buf), num);
	Add(buf);
}

void AllocatorStringBuilder::AddString(const char* pStr)
{
	Add('"');
	const size_t strLen = strlen(pStr);
	for (size_t i = 0; i < strLen; ++i)
	{
		char ch = pStr[i];
		if (ch == '\'')
		{
			Add("\\\\");
		}
		else if (ch == '"')
		{
			Add("\\\"");
		}
		else if (ch >= 32)
		{
			Add(ch);
		}
		else
			switch (ch)
			{
				case '\n': Add("\\n"); break;
				case '\r': Add("\\r"); break;
				case '\t': Add("\\t"); break;
				default: ASSERT(0 && "Character not currently supported."); break;
			}
	}
	Add('"');
}

// Correspond to values of enum AllocatorSuballocationType.
static const char* RESOURCE_SUBALLOCATION_TYPE_NAMES[] = {
	"FREE", "UNKNOWN", "BUFFER", "IMAGE_UNKNOWN", "IMAGE_LINEAR", "IMAGE_OPTIMAL", "IMAGE_RTV_DSV", "UAV",
};

static void AllocatorPrintStatInfo(AllocatorStringBuilder& sb, const AllocatorStatInfo& stat)
{
	sb.Add("{ \"Allocations\": ");
	sb.AddNumber(stat.AllocationCount);
	sb.Add(", \"Suballocations\": ");
	sb.AddNumber(stat.SuballocationCount);
	sb.Add(", \"UnusedRanges\": ");
	sb.AddNumber(stat.UnusedRangeCount);
	sb.Add(", \"UsedBytes\": ");
	sb.AddNumber(stat.UsedBytes);
	sb.Add(", \"UnusedBytes\": ");
	sb.AddNumber(stat.UnusedBytes);
	sb.Add(", \"SuballocationSize\": { \"Min\": ");
	sb.AddNumber(stat.SuballocationSizeMin);
	sb.Add(", \"Avg\": ");
	sb.AddNumber(stat.SuballocationSizeAvg);
	sb.Add(", \"Max\": ");
	sb.AddNumber(stat.SuballocationSizeMax);
	sb.Add(" }, \"UnusedRangeSize\": { \"Min\": ");
	sb.AddNumber(stat.UnusedRangeSizeMin);
	sb.Add(", \"Avg\": ");
	sb.AddNumber(stat.UnusedRangeSizeAvg);
	sb.Add(", \"Max\": ");
	sb.AddNumber(stat.UnusedRangeSizeMax);
	sb.Add(" } }");
}

#endif    // #if RESOURCE_STATS_STRING_ENABLED

static void InitStatInfo(AllocatorStatInfo& outInfo)
{
	memset(&outInfo, 0, sizeof(outInfo));
	outInfo.SuballocationSizeMin = UINT64_MAX;
	outInfo.UnusedRangeSizeMin = UINT64_MAX;
}

static void CalcAllocationStatInfo(AllocatorStatInfo& outInfo, const AllocatorBlock& alloc)
{
	outInfo.AllocationCount = 1;

	const uint32_t rangeCount = (uint32_t)alloc.m_Suballocations.size();
	outInfo.SuballocationCount = rangeCount - alloc.m_FreeCount;
	outInfo.UnusedRangeCount = alloc.m_FreeCount;

	outInfo.UnusedBytes = alloc.m_SumFreeSize;
	outInfo.UsedBytes = alloc.m_Size - outInfo.UnusedBytes;

	outInfo.SuballocationSizeMin = UINT64_MAX;
	outInfo.SuballocationSizeMax = 0;
	outInfo.UnusedRangeSizeMin = UINT64_MAX;
	outInfo.UnusedRangeSizeMax = 0;

	for (AllocatorSuballocationList::const_iterator suballocItem = alloc.m_Suballocations.cbegin();
		 suballocItem != alloc.m_Suballocations.cend(); ++suballocItem)
	{
		const AllocatorSuballocation& suballoc = *suballocItem;
		if (suballoc.type != RESOURCE_SUBALLOCATION_TYPE_FREE)
		{
			outInfo.SuballocationSizeMin = RESOURCE_MIN(outInfo.SuballocationSizeMin, suballoc.size);
			outInfo.SuballocationSizeMax = RESOURCE_MAX(outInfo.SuballocationSizeMax, suballoc.size);
		}
		else
		{
			outInfo.UnusedRangeSizeMin = RESOURCE_MIN(outInfo.UnusedRangeSizeMin, suballoc.size);
			outInfo.UnusedRangeSizeMax = RESOURCE_MAX(outInfo.UnusedRangeSizeMax, suballoc.size);
		}
	}
}

// Adds statistics srcInfo into inoutInfo, like: inoutInfo += srcInfo.
static void AllocatorAddStatInfo(AllocatorStatInfo& inoutInfo, const AllocatorStatInfo& srcInfo)
{
	inoutInfo.AllocationCount += srcInfo.AllocationCount;
	inoutInfo.SuballocationCount += srcInfo.SuballocationCount;
	inoutInfo.UnusedRangeCount += srcInfo.UnusedRangeCount;
	inoutInfo.UsedBytes += srcInfo.UsedBytes;
	inoutInfo.UnusedBytes += srcInfo.UnusedBytes;
	inoutInfo.SuballocationSizeMin = RESOURCE_MIN(inoutInfo.SuballocationSizeMin, srcInfo.SuballocationSizeMin);
	inoutInfo.SuballocationSizeMax = RESOURCE_MAX(inoutInfo.SuballocationSizeMax, srcInfo.SuballocationSizeMax);
	inoutInfo.UnusedRangeSizeMin = RESOURCE_MIN(inoutInfo.UnusedRangeSizeMin, srcInfo.UnusedRangeSizeMin);
	inoutInfo.UnusedRangeSizeMax = RESOURCE_MAX(inoutInfo.UnusedRangeSizeMax, srcInfo.UnusedRangeSizeMax);
}

static void AllocatorPostprocessCalcStatInfo(AllocatorStatInfo& inoutInfo)
{
	inoutInfo.SuballocationSizeAvg =
		(inoutInfo.SuballocationCount > 0) ? AllocatorRoundDiv<uint64_t>(inoutInfo.UsedBytes, inoutInfo.SuballocationCount) : 0;
	inoutInfo.UnusedRangeSizeAvg =
		(inoutInfo.UnusedRangeCount > 0) ? AllocatorRoundDiv<uint64_t>(inoutInfo.UnusedBytes, inoutInfo.UnusedRangeCount) : 0;
}

// -------------------------------------------------------------------------------------------------
// ResourceAllocation functionality implementation.
// -------------------------------------------------------------------------------------------------

id<MTLHeap> ResourceAllocation::GetMemory() const { return (m_Type == ALLOCATION_TYPE_BLOCK) ? m_BlockAllocation.m_Block->m_hMemory : nil; }

id<MTLBuffer> ResourceAllocation::GetResource() const
{
	if (m_Type == ALLOCATION_TYPE_OWN)
	{
		return NULL;
	}
	return (m_SuballocationType == RESOURCE_SUBALLOCATION_TYPE_BUFFER) ? m_BlockAllocation.m_Block->m_Buffer : nil;
}

uint32_t ResourceAllocation::GetMemoryTypeIndex() const
{
	return (m_Type == ALLOCATION_TYPE_BLOCK) ? m_BlockAllocation.m_Block->m_MemoryTypeIndex : m_OwnAllocation.m_MemoryTypeIndex;
}

RESOURCE_BLOCK_VECTOR_TYPE ResourceAllocation::GetBlockVectorType() const
{
	return (m_Type == ALLOCATION_TYPE_BLOCK)
			   ? m_BlockAllocation.m_Block->m_BlockVectorType
			   : (m_OwnAllocation.m_PersistentMap ? RESOURCE_BLOCK_VECTOR_TYPE_MAPPED : RESOURCE_BLOCK_VECTOR_TYPE_UNMAPPED);
}

void* ResourceAllocation::GetMappedData() const
{
	switch (m_Type)
	{
		case ALLOCATION_TYPE_BLOCK:
			if (m_BlockAllocation.m_Block->m_pMappedData != RESOURCE_NULL)
			{
				return (char*)m_BlockAllocation.m_Block->m_pMappedData + m_BlockAllocation.m_Offset;
			}
			else
			{
				return RESOURCE_NULL;
			}
			break;
		case ALLOCATION_TYPE_OWN: return m_OwnAllocation.m_pMappedData;
		default: ASSERT(0); return RESOURCE_NULL;
	}
}

// -------------------------------------------------------------------------------------------------
// AllocatorBlock functionality implementation.
// -------------------------------------------------------------------------------------------------

// Struct used in some AllocatorBlock functions.
struct AllocatorSuballocationItemSizeLess
{
	bool operator()(const AllocatorSuballocationList::iterator lhs, const AllocatorSuballocationList::iterator rhs) const
	{
		return lhs->size < rhs->size;
	}
	bool operator()(const AllocatorSuballocationList::iterator lhs, uint64_t rhsSize) const { return lhs->size < rhsSize; }
};

AllocatorBlock::AllocatorBlock(ResourceAllocator* hAllocator):
	m_MemoryTypeIndex(UINT32_MAX),
	m_BlockVectorType(RESOURCE_BLOCK_VECTOR_TYPE_COUNT),
	m_hMemory(NULL),
	m_Size(0),
	m_PersistentMap(false),
	m_pMappedData(RESOURCE_NULL),
	m_FreeCount(0),
	m_SumFreeSize(0)
{
}

void AllocatorBlock::Init(
	uint32_t newMemoryTypeIndex, RESOURCE_BLOCK_VECTOR_TYPE newBlockVectorType, id<MTLHeap> newMemory, uint64_t newSize, bool persistentMap,
	void* pMappedData)
{
	ASSERT(m_hMemory == nil);

	m_MemoryTypeIndex = newMemoryTypeIndex;
	m_BlockVectorType = newBlockVectorType;
	m_hMemory = newMemory;
	m_Size = newSize;
	m_PersistentMap = persistentMap;
	m_pMappedData = pMappedData;
	m_FreeCount = 1;
	m_SumFreeSize = newSize;

	m_Suballocations.clear();
	m_FreeSuballocationsBySize.clear();

	AllocatorSuballocation suballoc = {};
	suballoc.offset = 0;
	suballoc.size = newSize;
	suballoc.type = RESOURCE_SUBALLOCATION_TYPE_FREE;

	m_Suballocations.push_back(suballoc);
	AllocatorSuballocationList::iterator suballocItem = m_Suballocations.end();
	--suballocItem;
	m_FreeSuballocationsBySize.push_back(suballocItem);
}

void AllocatorBlock::Init(
	uint32_t newMemoryTypeIndex, RESOURCE_BLOCK_VECTOR_TYPE newBlockVectorType, id<MTLBuffer> newMemory, uint64_t newSize,
	bool persistentMap, void* pMappedData)
{
	ASSERT(m_hMemory == nil);

	m_MemoryTypeIndex = newMemoryTypeIndex;
	m_BlockVectorType = newBlockVectorType;
	m_Buffer = newMemory;
	m_Size = newSize;
	m_PersistentMap = persistentMap;
	m_pMappedData = pMappedData;
	m_FreeCount = 1;
	m_SumFreeSize = newSize;

	m_Suballocations.clear();
	m_FreeSuballocationsBySize.clear();

	AllocatorSuballocation suballoc = {};
	suballoc.offset = 0;
	suballoc.size = newSize;
	suballoc.type = RESOURCE_SUBALLOCATION_TYPE_FREE;

	m_Suballocations.push_back(suballoc);
	AllocatorSuballocationList::iterator suballocItem = m_Suballocations.end();
	--suballocItem;
	m_FreeSuballocationsBySize.push_back(suballocItem);
}

void AllocatorBlock::Init(
	uint32_t newMemoryTypeIndex, RESOURCE_BLOCK_VECTOR_TYPE newBlockVectorType, id<MTLTexture> newMemory, uint64_t newSize,
	bool persistentMap, void* pMappedData)
{
	ASSERT(m_hMemory == nil);

	m_MemoryTypeIndex = newMemoryTypeIndex;
	m_BlockVectorType = newBlockVectorType;
	m_Texture = newMemory;
	m_Size = newSize;
	m_PersistentMap = persistentMap;
	m_pMappedData = pMappedData;
	m_FreeCount = 1;
	m_SumFreeSize = newSize;

	m_Suballocations.clear();
	m_FreeSuballocationsBySize.clear();

	AllocatorSuballocation suballoc = {};
	suballoc.offset = 0;
	suballoc.size = newSize;
	suballoc.type = RESOURCE_SUBALLOCATION_TYPE_FREE;

	m_Suballocations.push_back(suballoc);
	AllocatorSuballocationList::iterator suballocItem = m_Suballocations.end();
	--suballocItem;
	m_FreeSuballocationsBySize.push_back(suballocItem);
}

void AllocatorBlock::Destroy(ResourceAllocator* allocator)
{
	ASSERT(m_Buffer != NULL || m_hMemory != NULL);
	if (m_pMappedData != RESOURCE_NULL)
	{
		m_pMappedData = RESOURCE_NULL;
	}

	if (m_hMemory)
		m_hMemory = nil;
	else if (m_Buffer)
		m_Buffer = nil;
	else
		m_Texture = nil;
	m_hMemory = nil;
}

bool AllocatorBlock::Validate() const
{
	if ((m_hMemory == nil) || (m_Size == 0) || m_Suballocations.empty())
	{
		return false;
	}

	// Expected offset of new suballocation as calculates from previous ones.
	uint64_t calculatedOffset = 0;
	// Expected number of free suballocations as calculated from traversing their list.
	uint32_t calculatedFreeCount = 0;
	// Expected sum size of free suballocations as calculated from traversing their list.
	uint64_t calculatedSumFreeSize = 0;
	// Expected number of free suballocations that should be registered in
	// m_FreeSuballocationsBySize calculated from traversing their list.
	size_t freeSuballocationsToRegister = 0;
	// True if previous visisted suballocation was free.
	bool prevFree = false;

	for (AllocatorSuballocationList::const_iterator suballocItem = m_Suballocations.cbegin(); suballocItem != m_Suballocations.cend();
		 ++suballocItem)
	{
		const AllocatorSuballocation& subAlloc = *suballocItem;

		// Actual offset of this suballocation doesn't match expected one.
		if (subAlloc.offset != calculatedOffset)
		{
			return false;
		}

		const bool currFree = (subAlloc.type == RESOURCE_SUBALLOCATION_TYPE_FREE);
		// Two adjacent free suballocations are invalid. They should be merged.
		if (prevFree && currFree)
		{
			return false;
		}
		prevFree = currFree;

		if (currFree)
		{
			calculatedSumFreeSize += subAlloc.size;
			++calculatedFreeCount;
			if (subAlloc.size >= RESOURCE_MIN_FREE_SUBALLOCATION_SIZE_TO_REGISTER)
			{
				++freeSuballocationsToRegister;
			}
		}

		calculatedOffset += subAlloc.size;
	}

	// Number of free suballocations registered in m_FreeSuballocationsBySize doesn't
	// match expected one.
	if (m_FreeSuballocationsBySize.size() != freeSuballocationsToRegister)
	{
		return false;
	}

	uint64_t lastSize = 0;
	for (size_t i = 0; i < m_FreeSuballocationsBySize.size(); ++i)
	{
		AllocatorSuballocationList::iterator suballocItem = m_FreeSuballocationsBySize[i];

		// Only free suballocations can be registered in m_FreeSuballocationsBySize.
		if (suballocItem->type != RESOURCE_SUBALLOCATION_TYPE_FREE)
		{
			return false;
		}
		// They must be sorted by size ascending.
		if (suballocItem->size < lastSize)
		{
			return false;
		}

		lastSize = suballocItem->size;
	}

	// Check if totals match calculacted values.
	return (calculatedOffset == m_Size) && (calculatedSumFreeSize == m_SumFreeSize) && (calculatedFreeCount == m_FreeCount);
}

bool AllocatorBlock::CreateAllocationRequest(
	uint64_t bufferImageGranularity, uint64_t allocSize, uint64_t allocAlignment, AllocatorSuballocationType allocType,
	AllocatorAllocationRequest* pAllocationRequest)
{
	ASSERT(allocSize > 0);
	ASSERT(allocType != RESOURCE_SUBALLOCATION_TYPE_FREE);
	ASSERT(pAllocationRequest != RESOURCE_NULL);
	RESOURCE_HEAVY_ASSERT(Validate());

	// There is not enough total free space in this allocation to fullfill the request: Early return.
	if (m_SumFreeSize < allocSize)
	{
		return false;
	}

	// New algorithm, efficiently searching freeSuballocationsBySize.
	const size_t freeSuballocCount = m_FreeSuballocationsBySize.size();
	if (freeSuballocCount > 0)
	{
		if (RESOURCE_BEST_FIT)
		{
			// Find first free suballocation with size not less than allocSize.
			AllocatorSuballocationList::iterator* const it = AllocatorBinaryFindFirstNotLess(
				m_FreeSuballocationsBySize.data(), m_FreeSuballocationsBySize.data() + freeSuballocCount, allocSize,
				AllocatorSuballocationItemSizeLess());
			size_t index = it - m_FreeSuballocationsBySize.data();
			for (; index < freeSuballocCount; ++index)
			{
				uint64_t                                   offset = 0;
				const AllocatorSuballocationList::iterator suballocItem = m_FreeSuballocationsBySize[index];
				if (CheckAllocation(bufferImageGranularity, allocSize, allocAlignment, allocType, suballocItem, &offset))
				{
					pAllocationRequest->freeSuballocationItem = suballocItem;
					pAllocationRequest->offset = offset;
					return true;
				}
			}
		}
		else
		{
			// Search staring from biggest suballocations.
			for (size_t index = freeSuballocCount; index--;)
			{
				uint64_t                                   offset = 0;
				const AllocatorSuballocationList::iterator suballocItem = m_FreeSuballocationsBySize[index];
				if (CheckAllocation(bufferImageGranularity, allocSize, allocAlignment, allocType, suballocItem, &offset))
				{
					pAllocationRequest->freeSuballocationItem = suballocItem;
					pAllocationRequest->offset = offset;
					return true;
				}
			}
		}
	}
	return false;
}

bool AllocatorBlock::CheckAllocation(
	uint64_t bufferImageGranularity, uint64_t allocSize, uint64_t allocAlignment, AllocatorSuballocationType allocType,
	AllocatorSuballocationList::const_iterator freeSuballocItem, uint64_t* pOffset) const
{
	ASSERT(allocSize > 0);
	ASSERT(allocType != RESOURCE_SUBALLOCATION_TYPE_FREE);
	ASSERT(freeSuballocItem != m_Suballocations.cend());
	ASSERT(pOffset != RESOURCE_NULL);

	const AllocatorSuballocation& suballoc = *freeSuballocItem;
	ASSERT(suballoc.type == RESOURCE_SUBALLOCATION_TYPE_FREE);

	// Size of this suballocation is too small for this request: Early return.
	if ([m_hMemory maxAvailableSizeWithAlignment:allocAlignment] < allocSize)
	{
		return false;
	}

	// Start from offset equal to beginning of this suballocation.
	*pOffset = suballoc.offset;

	// Apply RESOURCE_DEBUG_MARGIN at the beginning.
	if ((RESOURCE_DEBUG_MARGIN > 0) && freeSuballocItem != m_Suballocations.cbegin())
	{
		*pOffset += RESOURCE_DEBUG_MARGIN;
	}

	// Apply alignment.
	const uint64_t alignment = RESOURCE_MAX(allocAlignment, static_cast<uint64_t>(RESOURCE_DEBUG_ALIGNMENT));
	*pOffset = AllocatorAlignUp(*pOffset, alignment);

	// Check previous suballocations for BufferImageGranularity conflicts.
	// Make bigger alignment if necessary.
	if (bufferImageGranularity > 1)
	{
		bool                                       bufferImageGranularityConflict = false;
		AllocatorSuballocationList::const_iterator prevSuballocItem = freeSuballocItem;
		while (prevSuballocItem != m_Suballocations.cbegin())
		{
			--prevSuballocItem;
			const AllocatorSuballocation& prevSuballoc = *prevSuballocItem;
			if (AllocatorBlocksOnSamePage(prevSuballoc.offset, prevSuballoc.size, *pOffset, bufferImageGranularity))
			{
				if (AllocatorIsBufferImageGranularityConflict(prevSuballoc.type, allocType))
				{
					bufferImageGranularityConflict = true;
					break;
				}
			}
			else
				// Already on previous page.
				break;
		}
		if (bufferImageGranularityConflict)
		{
			*pOffset = AllocatorAlignUp(*pOffset, bufferImageGranularity);
		}
	}

	// Calculate padding at the beginning based on current offset.
	const uint64_t paddingBegin = *pOffset - suballoc.offset;

	// Calculate required margin at the end if this is not last suballocation.
	AllocatorSuballocationList::const_iterator next = freeSuballocItem;
	++next;
	const uint64_t requiredEndMargin = (next != m_Suballocations.cend()) ? RESOURCE_DEBUG_MARGIN : 0;

	// Fail if requested size plus margin before and after is bigger than size of this suballocation.
	if (paddingBegin + allocSize + requiredEndMargin > suballoc.size)
	{
		return false;
	}

	// Check next suballocations for BufferImageGranularity conflicts.
	// If conflict exists, allocation cannot be made here.
	if (bufferImageGranularity > 1)
	{
		AllocatorSuballocationList::const_iterator nextSuballocItem = freeSuballocItem;
		++nextSuballocItem;
		while (nextSuballocItem != m_Suballocations.cend())
		{
			const AllocatorSuballocation& nextSuballoc = *nextSuballocItem;
			if (AllocatorBlocksOnSamePage(*pOffset, allocSize, nextSuballoc.offset, bufferImageGranularity))
			{
				if (AllocatorIsBufferImageGranularityConflict(allocType, nextSuballoc.type))
				{
					return false;
				}
			}
			else
			{
				// Already on next page.
				break;
			}
			++nextSuballocItem;
		}
	}

	// All tests passed: Success. pOffset is already filled.
	return true;
}

bool AllocatorBlock::IsEmpty() const { return (m_Suballocations.size() == 1) && (m_FreeCount == 1); }

void AllocatorBlock::Alloc(const AllocatorAllocationRequest& request, AllocatorSuballocationType type, uint64_t allocSize)
{
	ASSERT(request.freeSuballocationItem != m_Suballocations.end());
	AllocatorSuballocation& suballoc = *request.freeSuballocationItem;
	// Given suballocation is a free block.
	ASSERT(suballoc.type == RESOURCE_SUBALLOCATION_TYPE_FREE);
	// Given offset is inside this suballocation.
	ASSERT(request.offset >= suballoc.offset);
	const uint64_t paddingBegin = request.offset - suballoc.offset;
	ASSERT(suballoc.size >= paddingBegin + allocSize);
	const uint64_t paddingEnd = suballoc.size - paddingBegin - allocSize;

	// Unregister this free suballocation from m_FreeSuballocationsBySize and update
	// it to become used.
	UnregisterFreeSuballocation(request.freeSuballocationItem);

	suballoc.offset = request.offset;
	suballoc.size = allocSize;
	suballoc.type = type;

	// If there are any free bytes remaining at the end, insert new free suballocation after current one.
	if (paddingEnd)
	{
		AllocatorSuballocation paddingSuballoc = {};
		paddingSuballoc.offset = request.offset + allocSize;
		paddingSuballoc.size = paddingEnd;
		paddingSuballoc.type = RESOURCE_SUBALLOCATION_TYPE_FREE;
		AllocatorSuballocationList::iterator next = request.freeSuballocationItem;
		++next;
		const AllocatorSuballocationList::iterator paddingEndItem = m_Suballocations.insert(next, paddingSuballoc);
		RegisterFreeSuballocation(paddingEndItem);
	}

	// If there are any free bytes remaining at the beginning, insert new free suballocation before current one.
	if (paddingBegin)
	{
		AllocatorSuballocation paddingSuballoc = {};
		paddingSuballoc.offset = request.offset - paddingBegin;
		paddingSuballoc.size = paddingBegin;
		paddingSuballoc.type = RESOURCE_SUBALLOCATION_TYPE_FREE;
		const AllocatorSuballocationList::iterator paddingBeginItem =
			m_Suballocations.insert(request.freeSuballocationItem, paddingSuballoc);
		RegisterFreeSuballocation(paddingBeginItem);
	}

	// Update totals.
	m_FreeCount = m_FreeCount - 1;
	if (paddingBegin > 0)
	{
		++m_FreeCount;
	}
	if (paddingEnd > 0)
	{
		++m_FreeCount;
	}
	m_SumFreeSize -= allocSize;
}

void AllocatorBlock::FreeSuballocation(AllocatorSuballocationList::iterator suballocItem)
{
	// Change this suballocation to be marked as free.
	AllocatorSuballocation& suballoc = *suballocItem;
	suballoc.type = RESOURCE_SUBALLOCATION_TYPE_FREE;

	// Update totals.
	++m_FreeCount;
	m_SumFreeSize += suballoc.size;

	// Merge with previous and/or next suballocation if it's also free.
	bool mergeWithNext = false;
	bool mergeWithPrev = false;

	AllocatorSuballocationList::iterator nextItem = suballocItem;
	++nextItem;
	if ((nextItem != m_Suballocations.end()) && (nextItem->type == RESOURCE_SUBALLOCATION_TYPE_FREE))
	{
		mergeWithNext = true;
	}

	AllocatorSuballocationList::iterator prevItem = suballocItem;
	if (suballocItem != m_Suballocations.begin())
	{
		--prevItem;
		if (prevItem->type == RESOURCE_SUBALLOCATION_TYPE_FREE)
		{
			mergeWithPrev = true;
		}
	}

	if (mergeWithNext)
	{
		UnregisterFreeSuballocation(nextItem);
		MergeFreeWithNext(suballocItem);
	}

	if (mergeWithPrev)
	{
		UnregisterFreeSuballocation(prevItem);
		MergeFreeWithNext(prevItem);
		RegisterFreeSuballocation(prevItem);
	}
	else
		RegisterFreeSuballocation(suballocItem);
}

void AllocatorBlock::Free(const ResourceAllocation* allocation)
{
	const uint64_t allocationOffset = allocation->GetOffset();
	for (AllocatorSuballocationList::iterator suballocItem = m_Suballocations.begin(); suballocItem != m_Suballocations.end();
		 ++suballocItem)
	{
		AllocatorSuballocation& suballoc = *suballocItem;
		if (suballoc.offset == allocationOffset)
		{
			FreeSuballocation(suballocItem);
			RESOURCE_HEAVY_ASSERT(Validate());
			return;
		}
	}
	ASSERT(0 && "Not found!");
}

#if RESOURCE_STATS_STRING_ENABLED

void AllocatorBlock::PrintDetailedMap(class AllocatorStringBuilder& sb) const
{
	sb.Add("{\n\t\t\t\"Bytes\": ");
	sb.AddNumber(m_Size);
	sb.Add(",\n\t\t\t\"FreeBytes\": ");
	sb.AddNumber(m_SumFreeSize);
	sb.Add(",\n\t\t\t\"Suballocations\": ");
	sb.AddNumber((uint64_t)m_Suballocations.size());
	sb.Add(",\n\t\t\t\"FreeSuballocations\": ");
	sb.AddNumber(m_FreeCount);
	sb.Add(",\n\t\t\t\"SuballocationList\": [");

	size_t i = 0;
	for (AllocatorSuballocationList::const_iterator suballocItem = m_Suballocations.cbegin(); suballocItem != m_Suballocations.cend();
		 ++suballocItem, ++i)
	{
		if (i > 0)
		{
			sb.Add(",\n\t\t\t\t{ \"Type\": ");
		}
		else
		{
			sb.Add("\n\t\t\t\t{ \"Type\": ");
		}
		sb.AddString(RESOURCE_SUBALLOCATION_TYPE_NAMES[suballocItem->type]);
		sb.Add(", \"Size\": ");
		sb.AddNumber(suballocItem->size);
		sb.Add(", \"Offset\": ");
		sb.AddNumber(suballocItem->offset);
		sb.Add(" }");
	}

	sb.Add("\n\t\t\t]\n\t\t}");
}

#endif    // #if RESOURCE_STATS_STRING_ENABLED

void AllocatorBlock::MergeFreeWithNext(AllocatorSuballocationList::iterator item)
{
	ASSERT(item != m_Suballocations.end());
	ASSERT(item->type == RESOURCE_SUBALLOCATION_TYPE_FREE);

	AllocatorSuballocationList::iterator nextItem = item;
	++nextItem;
	ASSERT(nextItem != m_Suballocations.end());
	ASSERT(nextItem->type == RESOURCE_SUBALLOCATION_TYPE_FREE);

	item->size += nextItem->size;
	--m_FreeCount;
	m_Suballocations.erase(nextItem);
}

void AllocatorBlock::RegisterFreeSuballocation(AllocatorSuballocationList::iterator item)
{
	ASSERT(item->type == RESOURCE_SUBALLOCATION_TYPE_FREE);
	ASSERT(item->size > 0);

	if (item->size >= RESOURCE_MIN_FREE_SUBALLOCATION_SIZE_TO_REGISTER)
	{
		if (m_FreeSuballocationsBySize.empty())
		{
			m_FreeSuballocationsBySize.push_back(item);
		}
		else
		{
			AllocatorSuballocationList::iterator* const it = AllocatorBinaryFindFirstNotLess(
				m_FreeSuballocationsBySize.data(), m_FreeSuballocationsBySize.data() + m_FreeSuballocationsBySize.size(), item,
				AllocatorSuballocationItemSizeLess());
			size_t index = it - m_FreeSuballocationsBySize.data();
			VectorInsert(m_FreeSuballocationsBySize, index, item);
		}
	}
}

void AllocatorBlock::UnregisterFreeSuballocation(AllocatorSuballocationList::iterator item)
{
	ASSERT(item->type == RESOURCE_SUBALLOCATION_TYPE_FREE);
	ASSERT(item->size > 0);

	if (item->size >= RESOURCE_MIN_FREE_SUBALLOCATION_SIZE_TO_REGISTER)
	{
		AllocatorSuballocationList::iterator* const it = AllocatorBinaryFindFirstNotLess(
			m_FreeSuballocationsBySize.data(), m_FreeSuballocationsBySize.data() + m_FreeSuballocationsBySize.size(), item,
			AllocatorSuballocationItemSizeLess());
		for (size_t index = it - m_FreeSuballocationsBySize.data(); index < m_FreeSuballocationsBySize.size(); ++index)
		{
			if (m_FreeSuballocationsBySize[index] == item)
			{
				VectorRemove(m_FreeSuballocationsBySize, index);
				return;
			}
			ASSERT((m_FreeSuballocationsBySize[index]->size == item->size) && "Not found.");
		}
		ASSERT(0 && "Not found.");
	}
}

// -------------------------------------------------------------------------------------------------
// AllocatorBlockVector functionality implementation.
// -------------------------------------------------------------------------------------------------

AllocatorBlockVector::AllocatorBlockVector(ResourceAllocator* hAllocator): m_hAllocator(hAllocator) {}

AllocatorBlockVector::~AllocatorBlockVector()
{
	for (size_t i = m_Blocks.size(); i--;)
	{
		m_Blocks[i]->Destroy(m_hAllocator);
		resourceAlloc_delete(m_Blocks[i]);
	}
}

void AllocatorBlockVector::Remove(AllocatorBlock* pBlock)
{
	for (uint32_t blockIndex = 0; blockIndex < m_Blocks.size(); ++blockIndex)
	{
		if (m_Blocks[blockIndex] == pBlock)
		{
			VectorRemove(m_Blocks, blockIndex);
			return;
		}
	}
	ASSERT(0);
}

void AllocatorBlockVector::IncrementallySortBlocks()
{
	// Bubble sort only until first swap.
	for (size_t i = 1; i < m_Blocks.size(); ++i)
	{
		if (m_Blocks[i - 1]->m_SumFreeSize > m_Blocks[i]->m_SumFreeSize)
		{
			RESOURCE_SWAP(m_Blocks[i - 1], m_Blocks[i]);
			return;
		}
	}
}

#if RESOURCE_STATS_STRING_ENABLED

void AllocatorBlockVector::PrintDetailedMap(class AllocatorStringBuilder& sb) const
{
	for (size_t i = 0; i < m_Blocks.size(); ++i)
	{
		if (i > 0)
		{
			sb.Add(",\n\t\t");
		}
		else
		{
			sb.Add("\n\t\t");
		}
		m_Blocks[i]->PrintDetailedMap(sb);
	}
}

#endif    // #if RESOURCE_STATS_STRING_ENABLED

void AllocatorBlockVector::UnmapPersistentlyMappedMemory()
{
	for (size_t i = m_Blocks.size(); i--;)
	{
		AllocatorBlock* pBlock = m_Blocks[i];
		if (pBlock->m_pMappedData != RESOURCE_NULL)
		{
			ASSERT(pBlock->m_PersistentMap != false);
			pBlock->m_pMappedData = RESOURCE_NULL;
		}
	}
}

bool AllocatorBlockVector::MapPersistentlyMappedMemory()
{
	bool finalResult = true;
	for (size_t i = 0, count = m_Blocks.size(); i < count; ++i)
	{
		AllocatorBlock* pBlock = m_Blocks[i];
		if (pBlock->m_PersistentMap)
		{
			pBlock->m_pMappedData = pBlock->m_Buffer.contents;
		}
	}
	return finalResult;
}

void AllocatorBlockVector::AddStats(AllocatorStats* pStats, uint32_t memTypeIndex, uint32_t memHeapIndex) const
{
	for (uint32_t allocIndex = 0; allocIndex < m_Blocks.size(); ++allocIndex)
	{
		const AllocatorBlock* const pBlock = m_Blocks[allocIndex];
		ASSERT(pBlock);
		RESOURCE_HEAVY_ASSERT(pBlock->Validate());
		AllocatorStatInfo allocationStatInfo;
		CalcAllocationStatInfo(allocationStatInfo, *pBlock);
		AllocatorAddStatInfo(pStats->total, allocationStatInfo);
		AllocatorAddStatInfo(pStats->memoryType[memTypeIndex], allocationStatInfo);
		AllocatorAddStatInfo(pStats->memoryHeap[memHeapIndex], allocationStatInfo);
	}
}

// -------------------------------------------------------------------------------------------------
// ResourceAllocator functionality implementation.
// -------------------------------------------------------------------------------------------------

struct AllocatorPointerLess
{
	bool operator()(const void* lhs, const void* rhs) const { return lhs < rhs; }
};

bool resourceAllocFindMemoryTypeIndex(
	ResourceAllocator* allocator, const AllocationInfo* pAllocInfo, const AllocatorMemoryRequirements* pMemoryRequirements,
	const AllocatorSuballocationType pSuballocType, uint32_t* pMemoryTypeIndex)
{
	ASSERT(allocator != RESOURCE_NULL);
	ASSERT(pMemoryRequirements != RESOURCE_NULL);
	ASSERT(pMemoryTypeIndex != RESOURCE_NULL);

	*pMemoryTypeIndex = UINT32_MAX;

	switch (pSuballocType)
	{
		case RESOURCE_SUBALLOCATION_TYPE_BUFFER:
			if (pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_DEFAULT_BUFFER;
			else if (
				pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_CPU_ONLY ||
				pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_UPLOAD_BUFFER;
			else if (pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_READBACK_BUFFER;
			break;
		case RESOURCE_SUBALLOCATION_TYPE_IMAGE_OPTIMAL:
			if (pAllocInfo->mSizeAlign.size <= 4096)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_SMALL;
			else if (pAllocInfo->mIsMS)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_MS;
			else
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_DEFAULT;
			break;
		case RESOURCE_SUBALLOCATION_TYPE_IMAGE_RTV_DSV:
			if (pAllocInfo->mIsMS)
			{
				if (pMemoryRequirements->flags & RESOURCE_MEMORY_REQUIREMENT_SHARED_ADAPTER_BIT)
					*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_SHARED_ADAPTER_MS;
				else if (pMemoryRequirements->flags & RESOURCE_MEMORY_REQUIREMENT_SHARED_BIT)
					*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_SHARED_MS;
				else
					*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_MS;
			}
			else
			{
				if (pMemoryRequirements->flags & RESOURCE_MEMORY_REQUIREMENT_SHARED_ADAPTER_BIT)
					*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_SHARED_ADAPTER;
				else if (pMemoryRequirements->flags & RESOURCE_MEMORY_REQUIREMENT_SHARED_BIT)
					*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_SHARED;
				else
					*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV;
			}
			break;
		case RESOURCE_SUBALLOCATION_TYPE_BUFFER_SRV_UAV:
			if (pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_DEFAULT_UAV;
			else if (
				pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_CPU_ONLY ||
				pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_UPLOAD_UAV;
			else if (pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_READBACK_UAV;
			break;
		default: break;
	}

	return (*pMemoryTypeIndex != UINT32_MAX) ? true : false;
}

ResourceAllocator::ResourceAllocator(const AllocatorCreateInfo* pCreateInfo):
	m_UseMutex((pCreateInfo->flags & RESOURCE_ALLOCATOR_EXTERNALLY_SYNCHRONIZED_BIT) == 0),
	m_Device(pCreateInfo->device),
	m_PreferredLargeHeapBlockSize(0),
	m_PreferredSmallHeapBlockSize(0),
	m_UnmapPersistentlyMappedMemoryCounter(0)
{
	ASSERT(pCreateInfo->device);

	memset(&m_pBlockVectors, 0, sizeof(m_pBlockVectors));
	memset(&m_HasEmptyBlock, 0, sizeof(m_HasEmptyBlock));
	memset(&m_pOwnAllocations, 0, sizeof(m_pOwnAllocations));

	m_PreferredLargeHeapBlockSize = (pCreateInfo->preferredLargeHeapBlockSize != 0)
										? pCreateInfo->preferredLargeHeapBlockSize
										: static_cast<uint64_t>(RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE);
	m_PreferredSmallHeapBlockSize = (pCreateInfo->preferredSmallHeapBlockSize != 0)
										? pCreateInfo->preferredSmallHeapBlockSize
										: static_cast<uint64_t>(RESOURCE_DEFAULT_SMALL_HEAP_BLOCK_SIZE);

	for (size_t i = 0; i < GetMemoryTypeCount(); ++i)
	{
		for (size_t j = 0; j < RESOURCE_BLOCK_VECTOR_TYPE_COUNT; ++j)
		{
			m_pBlockVectors[i][j] = conf_placement_new<AllocatorBlockVector>(AllocatorAllocate<AllocatorBlockVector>(), this);
			m_pOwnAllocations[i][j] = conf_placement_new<AllocationVectorType>(AllocatorAllocate<AllocationVectorType>());
		}
	}
}

ResourceAllocator::~ResourceAllocator()
{
	for (size_t i = GetMemoryTypeCount(); i--;)
	{
		for (size_t j = RESOURCE_BLOCK_VECTOR_TYPE_COUNT; j--;)
		{
			resourceAlloc_delete(m_pOwnAllocations[i][j]);
			resourceAlloc_delete(m_pBlockVectors[i][j]);
		}
	}
}

bool ResourceAllocator::AllocateMemoryOfType(
	const MTLSizeAndAlign& mtlMemReq, const AllocatorMemoryRequirements& resourceAllocMemReq, uint32_t memTypeIndex,
	AllocatorSuballocationType suballocType, ResourceAllocation** pAllocation)
{
	ASSERT(pAllocation != RESOURCE_NULL);
	RESOURCE_DEBUG_LOG("  AllocateMemory: MemoryTypeIndex=%u, Size=%llu", memTypeIndex, mtlMemReq.size);

	const uint64_t preferredBlockSize = GetPreferredBlockSize(resourceAllocMemReq.usage, memTypeIndex);

	// Only private storage heaps are supported on macOS, so any host visible memory must be allocated
	// as committed resource (own allocation).
	const bool hostVisible = resourceAllocMemReq.usage != RESOURCE_MEMORY_USAGE_GPU_ONLY;

	// Heuristics: Allocate own memory if requested size if greater than half of preferred block size.
	const bool ownMemory =
		hostVisible || (resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT) != 0 || RESOURCE_DEBUG_ALWAYS_OWN_MEMORY ||
		((resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_NEVER_ALLOCATE_BIT) == 0 && mtlMemReq.size > preferredBlockSize / 2);

	if (ownMemory)
	{
		if ((resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_NEVER_ALLOCATE_BIT) != 0)
		{
			return false;
		}
		else
		{
			return AllocateOwnMemory(
				mtlMemReq.size, suballocType, memTypeIndex,
				(resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT) != 0, resourceAllocMemReq.pUserData,
				pAllocation);
		}
	}
	else
	{
		uint32_t blockVectorType = AllocatorMemoryRequirementFlagsToBlockVectorType(resourceAllocMemReq.flags);

		AllocatorMutexLock          lock(m_BlocksMutex[memTypeIndex], m_UseMutex);
		AllocatorBlockVector* const blockVector = m_pBlockVectors[memTypeIndex][blockVectorType];
		ASSERT(blockVector);

		// 1. Search existing allocations.
		// Forward order - prefer blocks with smallest amount of free space.
		for (size_t allocIndex = 0; allocIndex < blockVector->m_Blocks.size(); ++allocIndex)
		{
			AllocatorBlock* const pBlock = blockVector->m_Blocks[allocIndex];
			ASSERT(pBlock);
			AllocatorAllocationRequest allocRequest = {};
			// Check if can allocate from pBlock.
			if (pBlock->CreateAllocationRequest(GetBufferImageGranularity(), mtlMemReq.size, mtlMemReq.align, suballocType, &allocRequest))
			{
				// We no longer have an empty Allocation.
				if (pBlock->IsEmpty())
				{
					m_HasEmptyBlock[memTypeIndex] = false;
				}
				// Allocate from this pBlock.
				pBlock->Alloc(allocRequest, suballocType, mtlMemReq.size);
				*pAllocation = conf_placement_new<ResourceAllocation>(AllocatorAllocate<ResourceAllocation>());
				(*pAllocation)
					->InitBlockAllocation(
						pBlock, allocRequest.offset, mtlMemReq.align, mtlMemReq.size, suballocType, resourceAllocMemReq.pUserData);
				RESOURCE_HEAVY_ASSERT(pBlock->Validate());
				RESOURCE_DEBUG_LOG("    Returned from existing allocation #%u", (uint32_t)allocIndex);
				return true;
			}
		}

		// 2. Create new Allocation.
		if ((resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_NEVER_ALLOCATE_BIT) != 0)
		{
			RESOURCE_DEBUG_LOG("    FAILED due to RESOURCE_MEMORY_REQUIREMENT_NEVER_ALLOCATE_BIT");
			return false;
		}
		else
		{
			bool res = false;

			const AllocatorHeapProperties* pHeapProps = &gHeapProperties[memTypeIndex];
			MTLHeapDescriptor*             allocInfo = [[MTLHeapDescriptor alloc] init];
			allocInfo.size = preferredBlockSize;
			allocInfo.cpuCacheMode = pHeapProps->mCPUCacheMode;
			allocInfo.storageMode = pHeapProps->mStorageMode;

			// Start with full preferredBlockSize.
			id<MTLHeap> mem = nil;
			mem = [m_Device newHeapWithDescriptor:allocInfo];
			mem.label = [NSString stringWithFormat:@"%s", pHeapProps->pName];
			if (mem == nil)
			{
				// 3. Try half the size.
				allocInfo.size /= 2;
				if (allocInfo.size >= mtlMemReq.size)
				{
					mem = [m_Device newHeapWithDescriptor:allocInfo];
					mem.label = [NSString stringWithFormat:@"%s", pHeapProps->pName];
					if (mem == nil)
					{
						// 4. Try quarter the size.
						allocInfo.size /= 2;
						if (allocInfo.size >= mtlMemReq.size)
						{
							mem = [m_Device newHeapWithDescriptor:allocInfo];
							mem.label = [NSString stringWithFormat:@"%s", pHeapProps->pName];
						}
					}
				}
			}
			if (mem == nil)
			{
				// 5. Try OwnAlloc.
				res = AllocateOwnMemory(
					mtlMemReq.size, suballocType, memTypeIndex,
					(resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT) != 0, resourceAllocMemReq.pUserData,
					pAllocation);
				if (res)
				{
					// Succeeded: AllocateOwnMemory function already filld pMemory, nothing more to do here.
					RESOURCE_DEBUG_LOG("    Allocated as OwnMemory");
				}
				else
				{
					// Everything failed: Return error code.
					RESOURCE_DEBUG_LOG("    Metal texture allocation FAILED");
				}

				return res;
			}

			// Create new Allocation for it.
			AllocatorBlock* const pBlock = conf_placement_new<AllocatorBlock>(AllocatorAllocate<AllocatorBlock>(), this);
			pBlock->Init(memTypeIndex, (RESOURCE_BLOCK_VECTOR_TYPE)blockVectorType, mem, allocInfo.size, false, NULL);
			blockVector->m_Blocks.push_back(pBlock);

			// Allocate from pBlock. Because it is empty, dstAllocRequest can be trivially filled.
			AllocatorAllocationRequest allocRequest = {};
			allocRequest.freeSuballocationItem = pBlock->m_Suballocations.begin();
			allocRequest.offset = 0;
			pBlock->Alloc(allocRequest, suballocType, mtlMemReq.size);
			*pAllocation = conf_placement_new<ResourceAllocation>(AllocatorAllocate<ResourceAllocation>());
			(*pAllocation)
				->InitBlockAllocation(
					pBlock, allocRequest.offset, mtlMemReq.align, mtlMemReq.size, suballocType, resourceAllocMemReq.pUserData);
			RESOURCE_HEAVY_ASSERT(pBlock->Validate());

			RESOURCE_DEBUG_LOG("    Created new allocation Size=%llu", allocInfo.SizeInBytes);

			return true;
		}
	}
	return false;
}

bool ResourceAllocator::AllocateOwnMemory(
	uint64_t size, AllocatorSuballocationType suballocType, uint32_t memTypeIndex, bool map, void* pUserData,
	ResourceAllocation** pAllocation)
{
	ASSERT(pAllocation);

	*pAllocation = conf_placement_new<ResourceAllocation>(AllocatorAllocate<ResourceAllocation>());
	(*pAllocation)->InitOwnAllocation(memTypeIndex, suballocType, map, NULL, size, pUserData);

	// Register it in m_pOwnAllocations.
	{
		AllocatorMutexLock    lock(m_OwnAllocationsMutex[memTypeIndex], m_UseMutex);
		AllocationVectorType* pOwnAllocations =
			m_pOwnAllocations[memTypeIndex][map ? RESOURCE_BLOCK_VECTOR_TYPE_MAPPED : RESOURCE_BLOCK_VECTOR_TYPE_UNMAPPED];
		ASSERT(pOwnAllocations);
		ResourceAllocation** const pOwnAllocationsBeg = pOwnAllocations->data();
		ResourceAllocation** const pOwnAllocationsEnd = pOwnAllocationsBeg + pOwnAllocations->size();
		const size_t               indexToInsert =
			AllocatorBinaryFindFirstNotLess(pOwnAllocationsBeg, pOwnAllocationsEnd, *pAllocation, AllocatorPointerLess()) -
			pOwnAllocationsBeg;
		VectorInsert(*pOwnAllocations, indexToInsert, *pAllocation);
	}

	RESOURCE_DEBUG_LOG("    Allocated OwnMemory MemoryTypeIndex=#%u", memTypeIndex);

	return true;
}

uint64_t ResourceAllocator::GetPreferredBlockSize(ResourceMemoryUsage memUsage, uint32_t memTypeIndex) const
{
	return gHeapProperties[memTypeIndex].mBlockSize;
}

bool ResourceAllocator::AllocateMemory(
	const AllocationInfo& info, const AllocatorMemoryRequirements& resourceAllocMemReq, AllocatorSuballocationType suballocType,
	ResourceAllocation** pAllocation)
{
	if ((resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT) != 0 &&
		(resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_NEVER_ALLOCATE_BIT) != 0)
	{
		ASSERT(
			0 &&
			"Specifying RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT together with RESOURCE_MEMORY_REQUIREMENT_NEVER_ALLOCATE_BIT makes no "
			"sense.");
		return false;
	}

	// Bit mask of memory Metal types acceptable for this allocation.
	uint32_t memTypeIndex = UINT32_MAX;
	bool     res = resourceAllocFindMemoryTypeIndex(this, &info, &resourceAllocMemReq, suballocType, &memTypeIndex);
	if (res)
	{
		res = AllocateMemoryOfType(info.mSizeAlign, resourceAllocMemReq, memTypeIndex, suballocType, pAllocation);
		// Succeeded on first try.
		if (res)
		{
			return res;
		}
		// Allocation from this memory type failed. Try other compatible memory types.
		else
		{
			return false;
		}
	}
	// Can't find any single memory type maching requirements. res is VK_ERROR_FEATURE_NOT_PRESENT.
	else
		return res;
}

void ResourceAllocator::FreeMemory(ResourceAllocation* allocation)
{
	ASSERT(allocation);

	if (allocation->GetType() == ResourceAllocation::ALLOCATION_TYPE_BLOCK)
	{
		AllocatorBlock* pBlockToDelete = RESOURCE_NULL;

		const uint32_t                   memTypeIndex = allocation->GetMemoryTypeIndex();
		const RESOURCE_BLOCK_VECTOR_TYPE blockVectorType = allocation->GetBlockVectorType();
		{
			AllocatorMutexLock lock(m_BlocksMutex[memTypeIndex], m_UseMutex);

			AllocatorBlockVector* pBlockVector = m_pBlockVectors[memTypeIndex][blockVectorType];
			AllocatorBlock*       pBlock = allocation->GetBlock();

			pBlock->Free(allocation);
			RESOURCE_HEAVY_ASSERT(pBlock->Validate());

			RESOURCE_DEBUG_LOG("  Freed from MemoryTypeIndex=%u", memTypeIndex);

			// pBlock became empty after this deallocation.
			if (pBlock->IsEmpty())
			{
				// Already has empty Allocation. We don't want to have two, so delete this one.
				if (m_HasEmptyBlock[memTypeIndex])
				{
					pBlockToDelete = pBlock;
					pBlockVector->Remove(pBlock);
				}
				// We now have first empty Allocation.
				else
				{
					m_HasEmptyBlock[memTypeIndex] = true;
				}
			}
			// Must be called after srcBlockIndex is used, because later it may become invalid!
			pBlockVector->IncrementallySortBlocks();
		}
		// Destruction of a free Allocation. Deferred until this point, outside of mutex
		// lock, for performance reason.
		if (pBlockToDelete != RESOURCE_NULL)
		{
			RESOURCE_DEBUG_LOG("    Deleted empty allocation");
			pBlockToDelete->Destroy(this);
			resourceAlloc_delete(pBlockToDelete);
		}

		resourceAlloc_delete(allocation);
	}
	else    // AllocatorAllocation_T::ALLOCATION_TYPE_OWN
	{
		FreeOwnMemory(allocation);
	}
}

void ResourceAllocator::CalculateStats(AllocatorStats* pStats)
{
	InitStatInfo(pStats->total);
	for (size_t i = 0; i < RESOURCE_MEMORY_TYPE_NUM_TYPES; ++i)
		InitStatInfo(pStats->memoryType[i]);
	for (size_t i = 0; i < RESOURCE_MEMORY_TYPE_NUM_TYPES; ++i)
		InitStatInfo(pStats->memoryHeap[i]);

	for (uint32_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
	{
		AllocatorMutexLock allocationsLock(m_BlocksMutex[memTypeIndex], m_UseMutex);
		const uint32_t     heapIndex = memTypeIndex;
		for (uint32_t blockVectorType = 0; blockVectorType < RESOURCE_BLOCK_VECTOR_TYPE_COUNT; ++blockVectorType)
		{
			const AllocatorBlockVector* const pBlockVector = m_pBlockVectors[memTypeIndex][blockVectorType];
			ASSERT(pBlockVector);
			pBlockVector->AddStats(pStats, memTypeIndex, heapIndex);
		}
	}

	AllocatorPostprocessCalcStatInfo(pStats->total);
	for (size_t i = 0; i < GetMemoryTypeCount(); ++i)
		AllocatorPostprocessCalcStatInfo(pStats->memoryType[i]);
	for (size_t i = 0; i < GetMemoryHeapCount(); ++i)
		AllocatorPostprocessCalcStatInfo(pStats->memoryHeap[i]);
}

static const uint32_t RESOURCE_VENDOR_ID_AMD = 4098;

void ResourceAllocator::UnmapPersistentlyMappedMemory()
{
	if (m_UnmapPersistentlyMappedMemoryCounter++ == 0)
	{
		/*if (m_PhysicalDeviceProperties.VendorId == RESOURCE_VENDOR_ID_AMD)
		{
			size_t memTypeIndex = D3D12_HEAP_TYPE_UPLOAD;
			{
				{
					// Process OwnAllocations.
					{
						AllocatorMutexLock lock(m_OwnAllocationsMutex[memTypeIndex], m_UseMutex);
						AllocationVectorType* pOwnAllocationsVector = m_pOwnAllocations[memTypeIndex][RESOURCE_BLOCK_VECTOR_TYPE_MAPPED];
						for (size_t ownAllocIndex = pOwnAllocationsVector->size(); ownAllocIndex--; )
						{
							ResourceAllocation* hAlloc = (*pOwnAllocationsVector)[ownAllocIndex];
							hAlloc->OwnAllocUnmapPersistentlyMappedMemory();
						}
					}

					// Process normal Allocations.
					{
						AllocatorMutexLock lock(m_BlocksMutex[memTypeIndex], m_UseMutex);
						AllocatorBlockVector* pBlockVector = m_pBlockVectors[memTypeIndex][RESOURCE_BLOCK_VECTOR_TYPE_MAPPED];
						pBlockVector->UnmapPersistentlyMappedMemory();
					}
				}
			}
		}*/
	}
}

bool ResourceAllocator::MapPersistentlyMappedMemory()
{
	ASSERT(m_UnmapPersistentlyMappedMemoryCounter > 0);
	if (--m_UnmapPersistentlyMappedMemoryCounter == 0)
	{
		bool finalResult = true;
		/*if (m_PhysicalDeviceProperties.VendorId == RESOURCE_VENDOR_ID_AMD)
		{
			size_t memTypeIndex = D3D12_HEAP_TYPE_UPLOAD;
			{
				{
					// Process OwnAllocations.
					{
						AllocatorMutexLock lock(m_OwnAllocationsMutex[memTypeIndex], m_UseMutex);
						AllocationVectorType* pAllocationsVector = m_pOwnAllocations[memTypeIndex][RESOURCE_BLOCK_VECTOR_TYPE_MAPPED];
						for (size_t ownAllocIndex = 0, ownAllocCount = pAllocationsVector->size(); ownAllocIndex < ownAllocCount; ++ownAllocIndex)
						{
							ResourceAllocation* hAlloc = (*pAllocationsVector)[ownAllocIndex];
							hAlloc->OwnAllocMapPersistentlyMappedMemory();
						}
					}

					// Process normal Allocations.
					{
						AllocatorMutexLock lock(m_BlocksMutex[memTypeIndex], m_UseMutex);
						AllocatorBlockVector* pBlockVector = m_pBlockVectors[memTypeIndex][RESOURCE_BLOCK_VECTOR_TYPE_MAPPED];
						HRESULT localResult = pBlockVector->MapPersistentlyMappedMemory();
						if (!SUCCEEDED(localResult))
						{
							finalResult = localResult;
						}
					}
				}
			}
		}*/
		return finalResult;
	}
	else
		return true;
}

void ResourceAllocator::GetAllocationInfo(ResourceAllocation* hAllocation, ResourceAllocationInfo* pAllocationInfo)
{
	pAllocationInfo->memoryType = hAllocation->GetMemoryTypeIndex();
	pAllocationInfo->deviceMemory = hAllocation->GetMemory();
	pAllocationInfo->resource = hAllocation->GetResource();
	pAllocationInfo->offset = hAllocation->GetOffset();
	pAllocationInfo->size = hAllocation->GetSize();
	pAllocationInfo->pMappedData = hAllocation->GetMappedData();
	pAllocationInfo->pUserData = hAllocation->GetUserData();
}

void ResourceAllocator::FreeOwnMemory(ResourceAllocation* allocation)
{
	ASSERT(allocation && allocation->GetType() == ResourceAllocation::ALLOCATION_TYPE_OWN);

	const uint32_t memTypeIndex = allocation->GetMemoryTypeIndex();
	{
		AllocatorMutexLock          lock(m_OwnAllocationsMutex[memTypeIndex], m_UseMutex);
		AllocationVectorType* const pOwnAllocations = m_pOwnAllocations[memTypeIndex][allocation->GetBlockVectorType()];
		ASSERT(pOwnAllocations);
		ResourceAllocation** const pOwnAllocationsBeg = pOwnAllocations->data();
		ResourceAllocation** const pOwnAllocationsEnd = pOwnAllocationsBeg + pOwnAllocations->size();
		ResourceAllocation** const pOwnAllocationIt =
			AllocatorBinaryFindFirstNotLess(pOwnAllocationsBeg, pOwnAllocationsEnd, allocation, AllocatorPointerLess());
		if (pOwnAllocationIt != pOwnAllocationsEnd)
		{
			const size_t ownAllocationIndex = pOwnAllocationIt - pOwnAllocationsBeg;
			VectorRemove(*pOwnAllocations, ownAllocationIndex);
		}
		else
		{
			ASSERT(0);
		}
	}

	RESOURCE_DEBUG_LOG("    Freed OwnMemory MemoryTypeIndex=%u", memTypeIndex);

	resourceAlloc_delete(allocation);
}

#if RESOURCE_STATS_STRING_ENABLED

void ResourceAllocator::PrintDetailedMap(AllocatorStringBuilder& sb)
{
	bool ownAllocationsStarted = false;
	for (size_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
	{
		AllocatorMutexLock ownAllocationsLock(m_OwnAllocationsMutex[memTypeIndex], m_UseMutex);
		for (uint32_t blockVectorType = 0; blockVectorType < RESOURCE_BLOCK_VECTOR_TYPE_COUNT; ++blockVectorType)
		{
			AllocationVectorType* const pOwnAllocVector = m_pOwnAllocations[memTypeIndex][blockVectorType];
			ASSERT(pOwnAllocVector);
			if (pOwnAllocVector->empty() == false)
			{
				if (ownAllocationsStarted)
				{
					sb.Add(",\n\t\"Type ");
				}
				else
				{
					sb.Add(",\n\"OwnAllocations\": {\n\t\"Type ");
					ownAllocationsStarted = true;
				}
				sb.AddNumber((uint64_t)memTypeIndex);
				if (blockVectorType == RESOURCE_BLOCK_VECTOR_TYPE_MAPPED)
				{
					sb.Add(" Mapped");
				}
				sb.Add("\": [");

				for (size_t i = 0; i < pOwnAllocVector->size(); ++i)
				{
					const ResourceAllocation* hAlloc = (*pOwnAllocVector)[i];
					if (i > 0)
					{
						sb.Add(",\n\t\t{ \"Size\": ");
					}
					else
					{
						sb.Add("\n\t\t{ \"Size\": ");
					}
					sb.AddNumber(hAlloc->GetSize());
					sb.Add(", \"Type\": ");
					sb.AddString(RESOURCE_SUBALLOCATION_TYPE_NAMES[hAlloc->GetSuballocationType()]);
					sb.Add(" }");
				}

				sb.Add("\n\t]");
			}
		}
	}
	if (ownAllocationsStarted)
	{
		sb.Add("\n}");
	}

	{
		bool allocationsStarted = false;
		for (size_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
		{
			AllocatorMutexLock globalAllocationsLock(m_BlocksMutex[memTypeIndex], m_UseMutex);
			for (uint32_t blockVectorType = 0; blockVectorType < RESOURCE_BLOCK_VECTOR_TYPE_COUNT; ++blockVectorType)
			{
				if (m_pBlockVectors[memTypeIndex][blockVectorType]->IsEmpty() == false)
				{
					if (allocationsStarted)
					{
						sb.Add(",\n\t\"Type ");
					}
					else
					{
						sb.Add(",\n\"Allocations\": {\n\t\"Type ");
						allocationsStarted = true;
					}
					sb.AddNumber((uint64_t)memTypeIndex);
					if (blockVectorType == RESOURCE_BLOCK_VECTOR_TYPE_MAPPED)
					{
						sb.Add(" Mapped");
					}
					sb.Add("\": [");

					m_pBlockVectors[memTypeIndex][blockVectorType]->PrintDetailedMap(sb);

					sb.Add("\n\t]");
				}
			}
		}
		if (allocationsStarted)
		{
			sb.Add("\n}");
		}
	}
}

#endif    // #if RESOURCE_STATS_STRING_ENABLED

// -------------------------------------------------------------------------------------------------
// Public interface
// -------------------------------------------------------------------------------------------------

bool createAllocator(const AllocatorCreateInfo* pCreateInfo, ResourceAllocator** pAllocator)
{
	ASSERT(pCreateInfo && pAllocator);
	RESOURCE_DEBUG_LOG("resourceAllocCreateAllocator");
	*pAllocator = conf_placement_new<ResourceAllocator>(AllocatorAllocate<ResourceAllocator>(), pCreateInfo);
	return true;
}

void destroyAllocator(ResourceAllocator* allocator)
{
	if (allocator != RESOURCE_NULL)
	{
		RESOURCE_DEBUG_LOG("resourceAllocDestroyAllocator");
		resourceAlloc_delete(allocator);
	}
}

void resourceAllocCalculateStats(ResourceAllocator* allocator, AllocatorStats* pStats)
{
	ASSERT(allocator && pStats);
	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK
	allocator->CalculateStats(pStats);
}

#if RESOURCE_STATS_STRING_ENABLED

void resourceAllocBuildStatsString(ResourceAllocator* allocator, char** ppStatsString, uint32_t detailedMap)
{
	ASSERT(allocator && ppStatsString);
	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	AllocatorStringBuilder sb(allocator);
	{
		AllocatorStats stats;
		allocator->CalculateStats(&stats);

		sb.Add("{\n\"Total\": ");
		AllocatorPrintStatInfo(sb, stats.total);

		for (uint32_t heapIndex = 0; heapIndex < allocator->GetMemoryHeapCount(); ++heapIndex)
		{
			sb.Add(",\n\"Heap ");
			sb.AddNumber(heapIndex);
			sb.Add("\": {\n\t\"Size\": ");
			//sb.AddNumber(allocator->m_MemProps.memoryHeaps[heapIndex].size);
			sb.Add(",\n\t\"Flags\": ");
			//if (gHeapProperties[heapIndex].mProps.Type == D3D12_HEAP_TYPE_DEFAULT)
			//{
			//  sb.AddString("DEVICE_LOCAL");
			//}
			//else
			//{
			//  sb.AddString("");
			//}
			if (stats.memoryHeap[heapIndex].AllocationCount > 0)
			{
				sb.Add(",\n\t\"Stats:\": ");
				AllocatorPrintStatInfo(sb, stats.memoryHeap[heapIndex]);
			}

			for (uint32_t typeIndex = 0; typeIndex < allocator->GetMemoryTypeCount(); ++typeIndex)
			{
				//if (allocator->m_MemProps.memoryTypes[typeIndex].heapIndex == heapIndex)
				{
					sb.Add(",\n\t\"Type ");
					sb.AddNumber(typeIndex);
					sb.Add("\": {\n\t\t\"Flags\": \"");
					/*if (gHeapProperties[typeIndex].mProps.Type == D3D12_HEAP_TYPE_DEFAULT)
					{
						sb.Add(" DEVICE_LOCAL");
					}
					if (gHeapProperties[typeIndex].mProps.Type == D3D12_HEAP_TYPE_UPLOAD)
					{
						sb.Add(" HOST_VISIBLE");
					}
					if (gHeapProperties[typeIndex].mProps.Type == D3D12_HEAP_TYPE_READBACK)
					{
						sb.Add(" HOST_COHERENT");
					}*/
					sb.Add("\"");
					if (stats.memoryType[typeIndex].AllocationCount > 0)
					{
						sb.Add(",\n\t\t\"Stats\": ");
						AllocatorPrintStatInfo(sb, stats.memoryType[typeIndex]);
					}
					sb.Add("\n\t}");
				}
			}
			sb.Add("\n}");
		}
		if (detailedMap == 1)
		{
			allocator->PrintDetailedMap(sb);
		}
		sb.Add("\n}\n");
	}

	const size_t len = sb.GetLength();
	char* const  pChars = conf_placement_new<char>(AllocatorAllocateArray<char>((len + 1)));
	if (len > 0)
	{
		memcpy(pChars, sb.GetData(), len);
	}
	pChars[len] = '\0';
	*ppStatsString = pChars;
}

void resourceAllocFreeStatsString(ResourceAllocator* allocator, char* pStatsString)
{
	if (pStatsString != RESOURCE_NULL)
	{
		ASSERT(allocator);
		size_t len = strlen(pStatsString);
		resourceAlloc_delete_array(pStatsString, len + 1);
	}
}

#endif    // #if RESOURCE_STATS_STRING_ENABLED

bool resourceAllocMapMemory(ResourceAllocator* allocator, ResourceAllocation* allocation, void** ppData)
{
	ASSERT(allocator && allocation && ppData);

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	if (allocation->GetResource())
		*ppData = allocation->GetResource().contents;

	return false;
}

void resourceAllocUnmapMemory(ResourceAllocator* allocator, ResourceAllocation* allocation)
{
	ASSERT(allocator && allocation);

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	//if (allocation->GetResource()) allocation->GetResource()->Unmap(0, NULL);
}

void resourceAllocUnmapPersistentlyMappedMemory(ResourceAllocator* allocator)
{
	ASSERT(allocator);

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	allocator->UnmapPersistentlyMappedMemory();
}

bool resourceAllocMapPersistentlyMappedMemory(ResourceAllocator* allocator)
{
	ASSERT(allocator);

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	return allocator->MapPersistentlyMappedMemory();
}

bool resourceAllocFindSuballocType(MTLTextureDescriptor* desc, AllocatorSuballocationType* suballocType)
{
	*suballocType = RESOURCE_SUBALLOCATION_TYPE_UNKNOWN;
	if (desc.usage & MTLTextureUsageRenderTarget)
		*suballocType = RESOURCE_SUBALLOCATION_TYPE_IMAGE_RTV_DSV;
	else
		*suballocType = RESOURCE_SUBALLOCATION_TYPE_IMAGE_OPTIMAL;

	return (*suballocType != RESOURCE_SUBALLOCATION_TYPE_UNKNOWN) ? true : false;
}

void resourceAllocFreeMemory(ResourceAllocator* allocator, ResourceAllocation* allocation)
{
	ASSERT(allocator && allocation);

	RESOURCE_DEBUG_LOG("resourceAllocFreeMemory");

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	allocator->FreeMemory(allocation);
}

void resourceAllocGetAllocationInfo(ResourceAllocator* allocator, ResourceAllocation* allocation, ResourceAllocationInfo* pAllocationInfo)
{
	ASSERT(allocator && allocation && pAllocationInfo);

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	allocator->GetAllocationInfo(allocation, pAllocationInfo);
}

void resourceAllocSetAllocationUserData(ResourceAllocator* allocator, ResourceAllocation* allocation, void* pUserData)
{
	ASSERT(allocator && allocation);

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	allocation->SetUserData(pUserData);
}

long createBuffer(
	ResourceAllocator* allocator, const BufferCreateInfo* pCreateInfo, const AllocatorMemoryRequirements* pMemoryRequirements,
	Buffer* pBuffer)
{
	ASSERT(allocator && pCreateInfo && pMemoryRequirements && pBuffer);
	RESOURCE_DEBUG_LOG("resourceAllocCreateBuffer");
	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	AllocatorSuballocationType suballocType = RESOURCE_SUBALLOCATION_TYPE_BUFFER;

	// For GPU buffers, use special memory type
	// For CPU mapped UAV / SRV buffers, just use suballocation strategy
	if (((pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER) || (pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_BUFFER)) &&
		pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
		suballocType = RESOURCE_SUBALLOCATION_TYPE_BUFFER_SRV_UAV;

	// Get the proper resource options for the buffer usage.
	MTLResourceOptions mtlResourceOptions = 0;
	switch (pMemoryRequirements->usage)
	{
		case RESOURCE_MEMORY_USAGE_GPU_ONLY: mtlResourceOptions = MTLResourceStorageModePrivate; break;
		case RESOURCE_MEMORY_USAGE_CPU_ONLY: mtlResourceOptions = MTLResourceCPUCacheModeDefaultCache | MTLResourceStorageModeShared; break;
		case RESOURCE_MEMORY_USAGE_CPU_TO_GPU:
			mtlResourceOptions = MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeShared;
			break;
		case RESOURCE_MEMORY_USAGE_GPU_TO_CPU:
			mtlResourceOptions = MTLResourceCPUCacheModeDefaultCache | MTLResourceStorageModeShared;
			break;
		default: assert(!"Unknown buffer usage type"); break;
	}

	// Get the proper size and alignment for the buffer's resource options.
	AllocationInfo info;
	info.mSizeAlign = [allocator->m_Device heapBufferSizeAndAlignWithLength:pCreateInfo->mSize options:mtlResourceOptions];
	info.mIsRT = false;
	info.mIsMS = false;

	// Allocate memory using allocator (either from a previously created heap, or from a new one).
	bool res = allocator->AllocateMemory(info, *pMemoryRequirements, suballocType, &pBuffer->pMtlAllocation);
	if (res)
	{
		if (pBuffer->pMtlAllocation->GetType() == ResourceAllocation::ALLOCATION_TYPE_BLOCK)
		{
			pBuffer->mtlBuffer = [pBuffer->pMtlAllocation->GetMemory() newBufferWithLength:pCreateInfo->mSize options:mtlResourceOptions];
			pBuffer->mtlBuffer.label = @"Placed Buffer";
			assert(pBuffer->mtlBuffer);

			if (pMemoryRequirements->flags & RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT)
			{
				if (pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
				{
					LOGWARNINGF(
						"Cannot map memory not visible on CPU. Use a readback buffer instead for reading the memory to a cpu visible "
						"buffer");
				}
				else
				{
					pBuffer->pMtlAllocation->GetBlock()->m_pMappedData = pBuffer->mtlBuffer.contents;
				}
			}
		}
		else
		{
			pBuffer->mtlBuffer = [allocator->m_Device newBufferWithLength:pCreateInfo->mSize options:mtlResourceOptions];
			pBuffer->mtlBuffer.label = @"Owned Buffer";
			assert(pBuffer->mtlBuffer);

			if (pMemoryRequirements->flags & RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT &&
				pMemoryRequirements->usage != RESOURCE_MEMORY_USAGE_GPU_ONLY)
			{
				pBuffer->pMtlAllocation->GetOwnAllocation()->m_pMappedData = pBuffer->mtlBuffer.contents;
			}
		}

		// Bind buffer with memory.
		if (pBuffer->pMtlAllocation)
		{
			// All steps succeeded.
			ResourceAllocationInfo allocInfo = {};
			allocator->GetAllocationInfo(pBuffer->pMtlAllocation, &allocInfo);
			pBuffer->pCpuMappedAddress = allocInfo.pMappedData;
			return true;
		}

		// If we failed to create a Metal allocation, free the temp memory and exit.
		allocator->FreeMemory(pBuffer->pMtlAllocation);
		return res;
	}

	// Exit (not properly allocated resource).
	return res;
}

void destroyBuffer(ResourceAllocator* allocator, Buffer* pBuffer)
{
	if (pBuffer->mtlBuffer != nil)
	{
		ASSERT(allocator);
		RESOURCE_DEBUG_LOG("resourceAllocDestroyBuffer");

		RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

		if (!pBuffer->pMtlAllocation->GetResource())
			pBuffer->mtlBuffer = nil;

		allocator->FreeMemory(pBuffer->pMtlAllocation);
	}
}

long createTexture(
	ResourceAllocator* allocator, const TextureCreateInfo* pCreateInfo, const AllocatorMemoryRequirements* pMemoryRequirements,
	Texture* pTexture)
{
	ASSERT(allocator && pCreateInfo->pDesc && pMemoryRequirements && pTexture);

	RESOURCE_DEBUG_LOG("resourceAllocCreateImage");

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	AllocatorSuballocationType suballocType;
	if (!resourceAllocFindSuballocType(pCreateInfo->pDesc, &suballocType))
		return false;

	// Allocate memory using allocator.
	AllocationInfo info;
	info.mSizeAlign = [allocator->m_Device heapTextureSizeAndAlignWithDescriptor:pCreateInfo->pDesc];
	info.mIsRT = pCreateInfo->mIsRT;
	info.mIsMS = pCreateInfo->mIsMS;

	bool res = allocator->AllocateMemory(info, *pMemoryRequirements, suballocType, &pTexture->pMtlAllocation);
	if (res)
	{
		if (pTexture->pMtlAllocation->GetType() == ResourceAllocation::ALLOCATION_TYPE_BLOCK)
		{
			pTexture->mtlTexture = [pTexture->pMtlAllocation->GetMemory() newTextureWithDescriptor:pCreateInfo->pDesc];
			assert(pTexture->mtlTexture);
			pTexture->mtlTexture.label = @"Placed Texture";
		}
		else
		{
			pTexture->mtlTexture = [allocator->m_Device newTextureWithDescriptor:pCreateInfo->pDesc];
			assert(pTexture->mtlTexture);
			pTexture->mtlTexture.label = @"Owned Texture";
		}

		// Bind texture with memory.
		if (pTexture->pMtlAllocation)
		{
			// All steps succeeded.
			ResourceAllocationInfo allocInfo = {};
			allocator->GetAllocationInfo(pTexture->pMtlAllocation, &allocInfo);
			return true;
		}

		pTexture->mtlTexture = nil;
		allocator->FreeMemory(pTexture->pMtlAllocation);
		return res;
	}
	return res;
}

void destroyTexture(ResourceAllocator* allocator, Texture* pTexture)
{
	if (pTexture->mtlTexture != nil)
	{
		ASSERT(allocator);

		RESOURCE_DEBUG_LOG("resourceAllocDestroyImage");

		RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

		pTexture->mtlTexture = nil;

		allocator->FreeMemory(pTexture->pMtlAllocation);
	}
}

#endif

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

#include "../../OS/Interfaces/IMemoryManager.h"

typedef struct ResourceAllocator MemoryAllocator;

typedef struct BufferCreateInfo
{
	const D3D12_RESOURCE_DESC* pDesc;
	D3D12_RESOURCE_STATES      mStartState;
	const wchar_t*             pDebugName;
} BufferCreateInfo;

typedef struct TextureCreateInfo
{
	const TextureDesc*         pTextureDesc;
	const D3D12_RESOURCE_DESC* pDesc;
	const D3D12_CLEAR_VALUE*   pClearValue;
	D3D12_RESOURCE_STATES      mStartState;
	const wchar_t*             pDebugName;
} TextureCreateInfo;

////////////////////////////////////////////////////////////////////////////////
/** \defgroup general General
@{
*/
struct ResourceAllocator;

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

/// Flags for created Allocator.
typedef enum AllocatorFlagBits
{
	/** \brief Allocator and all objects created from it will not be synchronized internally, so you must guarantee they are used from only one thread at a time or synchronized externally by you.

	Using this flag may increase performance because internal mutexes are not used.
	*/
	RESOURCE_ALLOCATOR_EXTERNALLY_SYNCHRONIZED_BIT = 0x00000001,

	RESOURCE_ALLOCATOR_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} AllocatorFlagBits;
typedef uint32_t AllocatorFlags;

/// Description of a Allocator to be created.
typedef struct AllocatorCreateInfo
{
	Renderer* pRenderer;
	/// Flags for created allocator. Use AllocatorFlagBits enum.
	AllocatorFlags flags;
	/// Vulkan physical device.
	/** It must be valid throughout whole lifetime of created Allocator. */
#if defined(_DURANGO)
	IDXGIAdapter* physicalDevice;
#else
	IDXGIAdapter3* physicalDevice;
#endif
	/// Vulkan device.
	/** It must be valid throughout whole lifetime of created Allocator. */
	ID3D12Device* device;
	/// Size of a single memory block to allocate for resources.
	/** Set to 0 to use default, which is currently 256 MB. */
	UINT64 preferredLargeHeapBlockSize;
	/// Size of a single memory block to allocate for resources from a small heap <= 512 MB.
	/** Set to 0 to use default, which is currently 64 MB. */
	UINT64 preferredSmallHeapBlockSize;
} AllocatorCreateInfo;

/// Creates Allocator object.
HRESULT createAllocator(const AllocatorCreateInfo* pCreateInfo, ResourceAllocator** pAllocator);

/// Destroys allocator object.
void destroyAllocator(ResourceAllocator* allocator);

typedef struct AllocatorStatInfo
{
	uint32_t AllocationCount;
	uint32_t SuballocationCount;
	uint32_t UnusedRangeCount;
	UINT64   UsedBytes;
	UINT64   UnusedBytes;
	UINT64   SuballocationSizeMin, SuballocationSizeAvg, SuballocationSizeMax;
	UINT64   UnusedRangeSizeMin, UnusedRangeSizeAvg, UnusedRangeSizeMax;
} AllocatorStatInfo;

/// General statistics from current state of Allocator.
struct AllocatorStats
{
	AllocatorStatInfo memoryType[RESOURCE_MEMORY_TYPE_NUM_TYPES];
	AllocatorStatInfo memoryHeap[RESOURCE_MEMORY_TYPE_NUM_TYPES];
	AllocatorStatInfo total;
};

/// Retrieves statistics from current state of the Allocator.
void resourceAllocCalculateStats(ResourceAllocator* allocator, AllocatorStats* pStats);

// Correspond to values of enum AllocatorSuballocationType.
static const char* RESOURCE_SUBALLOCATION_TYPE_NAMES[] = {
	"FREE", "UNKNOWN", "BUFFER", "IMAGE_UNKNOWN", "IMAGE_LINEAR", "IMAGE_OPTIMAL", "IMAGE_RTV_DSV", "UAV",
};

#define RESOURCE_STATS_STRING_ENABLED 1

#if RESOURCE_STATS_STRING_ENABLED

/// Builds and returns statistics as string in JSON format.
/** @param[out] ppStatsString Must be freed using resourceAllocFreeStatsString() function.
*/
void resourceAllocBuildStatsString(ResourceAllocator* allocator, char** ppStatsString, uint32_t detailedMap);

void resourceAllocFreeStatsString(ResourceAllocator* allocator, char* pStatsString);

#endif    // #if RESOURCE_STATS_STRING_ENABLED

/** @} */

////////////////////////////////////////////////////////////////////////////////
/** \defgroup layer1 Layer 1 Choosing Memory Type
@{
*/

/// Flags to be passed as AllocatorMemoryRequirements::flags.
typedef enum AllocatorMemoryRequirementFlagBits
{
	/** \brief Set this flag if the allocation should have its own memory block.

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

	RESOURCE_MEMORY_REQUIREMENT_ALLOW_INDIRECT_BUFFER = 0x00000020,    // This is used in the XBOX One runtime

	RESOURCE_MEMORY_REQUIREMENT_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} AllocatorMemoryRequirementFlagBits;
typedef uint32_t AllocatorMemoryRequirementFlags;

typedef struct AllocatorMemoryRequirements
{
	AllocatorMemoryRequirementFlags flags;
	/** \brief Intended usage of memory.

	Leave RESOURCE_MEMORY_USAGE_UNKNOWN if you specify requiredFlags. You can also use both.
	*/
	ResourceMemoryUsage usage;
	///** \brief Flags that must be set in a Memory Type chosen for an allocation.

	//Leave 0 if you specify requirement via usage. */
	//VkMemoryPropertyFlags requiredFlags;
	///** \brief Flags that preferably should be set in a Memory Type chosen for an allocation.

	//Set to 0 if no additional flags are prefered and only requiredFlags should be used.
	//If not 0, it must be a superset or equal to requiredFlags. */
	//VkMemoryPropertyFlags preferredFlags;
	/** \brief Custom general-purpose pointer that will be stored in AllocatorAllocation, can be read as AllocatorAllocationInfo::pUserData and changed using resourceAllocSetAllocationUserData(). */
	void* pUserData;
} AllocatorMemoryRequirements;

enum AllocatorSuballocationType
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
};

/**
This algorithm tries to find a memory type that:

- Is allowed by memoryTypeBits.
- Contains all the flags from pMemoryRequirements->requiredFlags.
- Matches intended usage.
- Has as many flags from pMemoryRequirements->preferredFlags as possible.

\return Returns VK_ERROR_FEATURE_NOT_PRESENT if not found. Receiving such result
from this function or any other allocating function probably means that your
device doesn't support any memory type with requested features for the specific
type of resource you want to use it for. Please check parameters of your
resource, like image layout (OPTIMAL versus LINEAR) or mip level count.
*/
HRESULT resourceAllocFindMemoryTypeIndex(
	ResourceAllocator* allocator, const D3D12_RESOURCE_ALLOCATION_INFO* pAllocInfo, const AllocatorMemoryRequirements* pMemoryRequirements,
	const AllocatorSuballocationType pSuballocType, uint32_t* pMemoryTypeIndex);

/** @} */

////////////////////////////////////////////////////////////////////////////////
/** \defgroup layer2 Layer 2 Allocating Memory
@{
*/

struct ResourceAllocation;

/** \brief Parameters of AllocatorAllocation objects, that can be retrieved using function resourceAllocGetAllocationInfo().
*/
typedef struct ResourceAllocationInfo
{
	/** \brief Memory type index that this allocation was allocated from.

	It never changes.
	*/
	uint32_t memoryType;
	/** \brief Handle to D3D12 Heap.

	Same memory object can be shared by multiple allocations.

	It can change after call to resourceAllocDefragment() if this allocation is passed to the function.
	*/
	ID3D12Heap* deviceMemory;
	/** \brief Handle to D3D12 Resource.

	Same memory object can be shared by multiple allocations.

	It can change after call to resourceAllocDefragment() if this allocation is passed to the function.
	*/
	ID3D12Resource* resource;
	/** \brief Offset into deviceMemory object to the beginning of this allocation, in bytes. (deviceMemory, offset) pair is unique to this allocation.

	It can change after call to resourceAllocDefragment() if this allocation is passed to the function.
	*/
	UINT64 offset;
	/** \brief Size of this allocation, in bytes.

	It never changes.
	*/
	UINT64 size;
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

/** \brief General purpose memory allocation.

@param[out] pAllocation Handle to allocated memory.
@param[out] pAllocationInfo Optional. Information about allocated memory. It can be later fetched using function AllocatorGetAllocationInfo().

You should free the memory using resourceAllocFreeMemory().

It is recommended to use resourceAllocAllocateMemoryForBuffer(), resourceAllocAllocateMemoryForImage(),
resourceAllocCreateBuffer(), resourceAllocCreateImage() instead whenever possible.
*/
HRESULT resourceAllocAllocateMemory(
	ResourceAllocator* allocator, const D3D12_RESOURCE_ALLOCATION_INFO* pVkMemoryRequirements,
	const AllocatorMemoryRequirements* pAllocatorMemoryRequirements, ResourceAllocation** pAllocation,
	ResourceAllocationInfo* pAllocationInfo);

/**
@param[out] pAllocation Handle to allocated memory.
@param[out] pAllocationInfo Optional. Information about allocated memory. It can be later fetched using function AllocatorGetAllocationInfo().

You should free the memory using resourceAllocFreeMemory().
*/
HRESULT resourceAllocAllocateMemoryForBuffer(
	ResourceAllocator* allocator, D3D12_RESOURCE_DESC* buffer, const AllocatorMemoryRequirements* pMemoryRequirements,
	ResourceAllocation** pAllocation, ResourceAllocationInfo* pAllocationInfo);

/// Function similar to resourceAllocAllocateMemoryForBuffer().
HRESULT resourceAllocAllocateMemoryForImage(
	ResourceAllocator* allocator, D3D12_RESOURCE_DESC* image, D3D12_RESOURCE_STATES resStates,
	const AllocatorMemoryRequirements* pMemoryRequirements, ResourceAllocation** pAllocation, ResourceAllocationInfo* pAllocationInfo);

/// Frees memory previously allocated using resourceAllocAllocateMemory(), resourceAllocAllocateMemoryForBuffer(), or resourceAllocAllocateMemoryForImage().
void resourceAllocFreeMemory(ResourceAllocator* allocator, ResourceAllocation* allocation);

/// Returns current information about specified allocation.
void resourceAllocGetAllocationInfo(ResourceAllocator* allocator, ResourceAllocation* allocation, ResourceAllocationInfo* pAllocationInfo);

/// Sets pUserData in given allocation to new value.
void resourceAllocSetAllocationUserData(ResourceAllocator* allocator, ResourceAllocation* allocation, void* pUserData);

/**
Feel free to use vkMapMemory on these memory blocks on you own if you want, but
just for convenience and to make sure correct offset and size is always
specified, usage of resourceAllocMapMemory() / resourceAllocUnmapMemory() is recommended.

Do not use it on memory allocated with RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT
as multiple maps to same VkDeviceMemory is illegal.
*/
HRESULT resourceAllocMapMemory(ResourceAllocator* allocator, ResourceAllocation* allocation, void** ppData);

void resourceAllocUnmapMemory(ResourceAllocator* allocator, ResourceAllocation* allocation);

/** \brief Unmaps persistently mapped memory of types that is HOST_COHERENT and DEVICE_LOCAL.

This is optional performance optimization. You should call it on Windows for
time of call to vkQueueSubmit and vkQueuePresent, for performance reasons,
because of the internal behavior of WDDM.

After this call AllocatorAllocationInfo::pMappedData of some allocations may become null.

This call is reference-counted. Memory is mapped again after you call
resourceAllocMapPersistentlyMappedMemory() same number of times that you called
resourceAllocUnmapPersistentlyMappedMemory().
*/
void resourceAllocUnmapPersistentlyMappedMemory(ResourceAllocator* allocator);

/** \brief Maps back persistently mapped memory of types that is HOST_COHERENT and DEVICE_LOCAL.

See resourceAllocUnmapPersistentlyMappedMemory().

After this call AllocatorAllocationInfo::pMappedData of some allocation may have value
different than before calling resourceAllocUnmapPersistentlyMappedMemory().
*/
HRESULT resourceAllocMapPersistentlyMappedMemory(ResourceAllocator* allocator);

////////////////////////////////////////////////////////////////////////////////
/** \defgroup layer3 Layer 3 Creating Buffers and Images
@{
*/

/**
@param[out] pBuffer Buffer that was created.
@param[out] pAllocation Allocation that was created.
@param[out] pAllocationInfo Optional. Information about allocated memory. It can be later fetched using function AllocatorGetAllocationInfo().

This function automatically:

-# Creates buffer/image.
-# Allocates appropriate memory for it.
-# Binds the buffer/image with the memory.

You do not (and should not) pass returned pMemory to resourceAllocFreeMemory. Only calling
resourceAllocDestroyBuffer() / resourceAllocDestroyImage() is required for objects created using
resourceAllocCreateBuffer() / resourceAllocCreateImage().
*/

/** @} */

#endif    // AMD_VULKAN_RESOURCE_H

//#ifdef RESOURCE_IMPLEMENTATION
#if 1
#undef RESOURCE_IMPLEMENTATION

#include <cstdint>
#include <cstdlib>

/*******************************************************************************
CONFIGURATION SECTION

Define some of these macros before each #include of this header or change them
here if you need other then default behavior depending on your environment.
*/

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
// Value used as null pointer. Define it to e.g.: NULL, NULL, 0, (void*)0.
#define RESOURCE_NULL NULL
#endif

#ifndef RESOURCE_ALIGN_OF
#define RESOURCE_ALIGN_OF(type) (__alignof(type))
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

//#define DEBUG_LOG_MEMORY
#ifdef DEBUG_LOG_MEMORY
#ifndef RESOURCE_DEBUG_LOG
#define RESOURCE_DEBUG_LOG LOGDEBUGF
#endif
#else
#define RESOURCE_DEBUG_LOG(format, ...) ((void)0)
#endif

// Define this macro to 1 to enable functions: resourceAllocBuildStatsString, resourceAllocFreeStatsString.
#if RESOURCE_STATS_STRING_ENABLED
static inline void AllocatorUint32ToStr(char* outStr, size_t strLen, uint32_t num) { _ultoa_s(num, outStr, strLen, 10); }
static inline void AllocatorUint64ToStr(char* outStr, size_t strLen, uint64_t num) { _ui64toa_s(num, outStr, strLen, 10); }
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
#define RESOURCE_DEBUG_ALWAYS_OWN_MEMORY (0)
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
/// Maximum size of a memory heap in Vulkan to consider it "small".
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

/*******************************************************************************
END OF CONFIGURATION
*/

//static VkAllocationCallbacks AllocatorEmptyAllocationCallbacks = {
//  RESOURCE_NULL, RESOURCE_NULL, RESOURCE_NULL, RESOURCE_NULL, RESOURCE_NULL, RESOURCE_NULL };

// Returns number of bits set to 1 in (v).
static inline uint32_t CountBitsSet(uint32_t v)
{
	uint32_t c = v - ((v >> 1) & 0x55555555);
	c = ((c >> 2) & 0x33333333) + (c & 0x33333333);
	c = ((c >> 4) + c) & 0x0F0F0F0F;
	c = ((c >> 8) + c) & 0x00FF00FF;
	c = ((c >> 16) + c) & 0x0000FFFF;
	return c;
}

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
static inline bool AllocatorBlocksOnSamePage(UINT64 resourceAOffset, UINT64 resourceASize, UINT64 resourceBOffset, UINT64 pageSize)
{
	ASSERT(resourceAOffset + resourceASize <= resourceBOffset && resourceASize > 0 && pageSize > 0);
	UINT64 resourceAEnd = resourceAOffset + resourceASize - 1;
	UINT64 resourceAEndPage = resourceAEnd & ~(pageSize - 1);
	UINT64 resourceBStart = resourceBOffset;
	UINT64 resourceBStartPage = resourceBStart & ~(pageSize - 1);
	return resourceAEndPage == resourceBStartPage;
}

/*
Returns true if given suballocation types could conflict and must respect
VkPhysicalDeviceLimits::bufferImageGranularity. They conflict if one is buffer
or linear image and another one is optimal image. If type is unknown, behave
conservatively.
*/
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

// Minimum size of a free suballocation to register it in the free suballocation collection.
static const UINT64 RESOURCE_MIN_FREE_SUBALLOCATION_SIZE_TO_REGISTER = 16;

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

////////////////////////////////////////////////////////////////////////////////
// Memory allocation

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

#define resourceAlloc_new(type, ...) conf_placement_new<type>(AllocatorAllocate<type>(), __VA_ARGS__)
#define resourceAlloc_new_array(type, count, ...) conf_placement_new<type>(AllocatorAllocateArray<type>((count)), __VA_ARGS__)

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

////////////////////////////////////////////////////////////////////////////////
// class AllocatorPoolAllocator

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
//m_ItemBlocks (AllocatorStlAllocator<ItemBlock> (pAllocationCallbacks))
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
	ItemBlock newBlock = { resourceAlloc_new_array(Item, m_ItemsPerBlock), 0 };

	m_ItemBlocks.push_back(newBlock);

	// Setup singly-linked list of all free items in this block.
	for (uint32_t i = 0; i < m_ItemsPerBlock - 1; ++i)
		newBlock.pItems[i].NextFreeIndex = i + 1;
	newBlock.pItems[m_ItemsPerBlock - 1].NextFreeIndex = UINT32_MAX;
	return m_ItemBlocks.back();
}

////////////////////////////////////////////////////////////////////////////////
// class AllocatorRawList, AllocatorList

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

////////////////////////////////////////////////////////////////////////////////
// class AllocatorMap

#define AllocatorPair tinystl::pair
#define RESOURCE_MAP_TYPE(KeyT, ValueT) tinystl::unordered_map<KeyT, ValueT>

////////////////////////////////////////////////////////////////////////////////

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
	ID3D12Resource* m_hResource;
	uint32_t        m_MemoryTypeIndex;
	bool            m_PersistentMap;
	void*           m_pMappedData;
};

struct BlockAllocation
{
	AllocatorBlock* m_Block;
	UINT64          m_Offset;
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

	void InitBlockAllocation(
		AllocatorBlock* block, UINT64 offset, UINT64 alignment, UINT64 size, AllocatorSuballocationType suballocationType, void* pUserData)
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

	void ChangeBlockAllocation(AllocatorBlock* block, UINT64 offset)
	{
		ASSERT(block != RESOURCE_NULL);
		ASSERT(m_Type == ALLOCATION_TYPE_BLOCK);
		m_BlockAllocation.m_Block = block;
		m_BlockAllocation.m_Offset = offset;
	}

	void InitOwnAllocation(
		uint32_t memoryTypeIndex, AllocatorSuballocationType suballocationType, bool persistentMap, void* pMappedData, UINT64 size,
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
	UINT64                     GetAlignment() const { return m_Alignment; }
	UINT64                     GetSize() const { return m_Size; }
	void*                      GetUserData() const { return m_pUserData; }
	void                       SetUserData(void* pUserData) { m_pUserData = pUserData; }
	AllocatorSuballocationType GetSuballocationType() const { return m_SuballocationType; }

	AllocatorBlock* GetBlock() const
	{
		ASSERT(m_Type == ALLOCATION_TYPE_BLOCK);
		return m_BlockAllocation.m_Block;
	}
	UINT64                     GetOffset() const { return (m_Type == ALLOCATION_TYPE_BLOCK) ? m_BlockAllocation.m_Offset : 0; }
	ID3D12Heap*                GetMemory() const;
	ID3D12Resource*            GetResource() const;
	uint32_t                   GetMemoryTypeIndex() const;
	RESOURCE_BLOCK_VECTOR_TYPE GetBlockVectorType() const;
	void*                      GetMappedData() const;
	OwnAllocation*             GetOwnAllocation() { return &m_OwnAllocation; }

	HRESULT OwnAllocMapPersistentlyMappedMemory()
	{
		ASSERT(m_Type == ALLOCATION_TYPE_OWN);
		if (m_OwnAllocation.m_PersistentMap)
		{
			return m_OwnAllocation.m_hResource->Map(0, NULL, &m_OwnAllocation.m_pMappedData);
		}
		return S_OK;
	}
	void OwnAllocUnmapPersistentlyMappedMemory()
	{
		ASSERT(m_Type == ALLOCATION_TYPE_OWN);
		if (m_OwnAllocation.m_pMappedData)
		{
			ASSERT(m_OwnAllocation.m_PersistentMap);
			m_OwnAllocation.m_hResource->Unmap(0, NULL);
			m_OwnAllocation.m_pMappedData = RESOURCE_NULL;
		}
	}

	private:
	UINT64                     m_Alignment;
	UINT64                     m_Size;
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
	ID3D12Resource*            resource;
	void*                      mappedData;
	UINT64                     offset;
	UINT64                     size;
	AllocatorSuballocationType type;
};

typedef AllocatorList<AllocatorSuballocation> AllocatorSuballocationList;

// Parameters of an allocation.
struct AllocatorAllocationRequest
{
	AllocatorSuballocationList::iterator freeSuballocationItem;
	UINT64                               offset;
};

/* Single block of memory - VkDeviceMemory with all the data about its regions
assigned or free. */
class AllocatorBlock
{
	public:
	uint32_t                   m_MemoryTypeIndex;
	RESOURCE_BLOCK_VECTOR_TYPE m_BlockVectorType;
	ID3D12Heap*                m_hMemory;
	ID3D12Resource*            m_hResource;
	UINT64                     m_Size;
	bool                       m_PersistentMap;
	void*                      m_pMappedData;
	uint32_t                   m_FreeCount;
	UINT64                     m_SumFreeSize;
	AllocatorSuballocationList m_Suballocations;
	// Suballocations that are free and have size greater than certain threshold.
	// Sorted by size, ascending.
	AllocatorVector<AllocatorSuballocationList::iterator> m_FreeSuballocationsBySize;

	AllocatorBlock(ResourceAllocator* hAllocator);

	~AllocatorBlock() { ASSERT(m_hMemory == NULL); }

	// Always call after construction.
	void Init(
		uint32_t newMemoryTypeIndex, RESOURCE_BLOCK_VECTOR_TYPE newBlockVectorType, ID3D12Heap* newMemory, UINT64 newSize,
		bool persistentMap, void* pMappedData);
	void Init(
		uint32_t newMemoryTypeIndex, RESOURCE_BLOCK_VECTOR_TYPE newBlockVectorType, ID3D12Resource* newMemory, UINT64 newSize,
		bool persistentMap, void* pMappedData);
	// Always call before destruction.
	void Destroy(ResourceAllocator* allocator);

	// Validates all data structures inside this object. If not valid, returns false.
	bool Validate() const;

	// Tries to find a place for suballocation with given parameters inside this allocation.
	// If succeeded, fills pAllocationRequest and returns true.
	// If failed, returns false.
	bool CreateAllocationRequest(
		UINT64 bufferImageGranularity, UINT64 allocSize, UINT64 allocAlignment, AllocatorSuballocationType allocType,
		AllocatorAllocationRequest* pAllocationRequest);

	// Checks if requested suballocation with given parameters can be placed in given pFreeSuballocItem.
	// If yes, fills pOffset and returns true. If no, returns false.
	bool CheckAllocation(
		UINT64 bufferImageGranularity, UINT64 allocSize, UINT64 allocAlignment, AllocatorSuballocationType allocType,
		AllocatorSuballocationList::const_iterator freeSuballocItem, UINT64* pOffset) const;

	// Returns true if this allocation is empty - contains only single free suballocation.
	bool IsEmpty() const;

	// Makes actual allocation based on request. Request must already be checked
	// and valid.
	void Alloc(const AllocatorAllocationRequest& request, AllocatorSuballocationType type, UINT64 allocSize);

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

struct AllocatorPointerLess
{
	bool operator()(const void* lhs, const void* rhs) const { return lhs < rhs; }
};

/* Sequence of AllocatorBlock. Represents memory blocks allocated for a specific
Vulkan memory type. */
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

	void    UnmapPersistentlyMappedMemory();
	HRESULT MapPersistentlyMappedMemory();

	private:
	ResourceAllocator* m_hAllocator;
};

#define RESOURCE_MAX_MEMORY_HEAPS D3D12_HEAP_TYPE_CUSTOM

// Main allocator object.
struct ResourceAllocator
{
	Renderer*     pRenderer;
	bool          m_UseMutex;
	ID3D12Device* m_hDevice;
	bool          m_AllocationCallbacksSpecified;
	//VkAllocationCallbacks m_AllocationCallbacks;
	//AllocatorDeviceMemoryCallbacks m_DeviceMemoryCallbacks;
	UINT64 m_PreferredLargeHeapBlockSize;
	UINT64 m_PreferredSmallHeapBlockSize;
	// Non-zero when we are inside UnmapPersistentlyMappedMemory...MapPersistentlyMappedMemory.
	// Counter to allow nested calls to these functions.
	uint32_t m_UnmapPersistentlyMappedMemoryCounter;

	DXGI_ADAPTER_DESC m_PhysicalDeviceProperties;
#ifndef _DURANGO
	DXGI_QUERY_VIDEO_MEMORY_INFO m_MemProps[2];
#endif
	//VkPhysicalDeviceMemoryProperties m_MemProps;

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

	//const VkAllocationCallbacks* GetAllocationCallbacks() const
	//{
	//  return m_AllocationCallbacksSpecified ? &m_AllocationCallbacks : 0;
	//}

	UINT64 GetPreferredBlockSize(ResourceMemoryUsage memUsage, uint32_t memTypeIndex) const;

	UINT64 GetBufferImageGranularity() const { return RESOURCE_MAX(static_cast<UINT64>(RESOURCE_DEBUG_MIN_BUFFER_IMAGE_GRANULARITY), 1); }

	uint32_t GetMemoryHeapCount() const { return RESOURCE_MEMORY_TYPE_NUM_TYPES; }
	uint32_t GetMemoryTypeCount() const { return RESOURCE_MEMORY_TYPE_NUM_TYPES; }

	// Main allocation function.
	HRESULT AllocateMemory(
		const D3D12_RESOURCE_ALLOCATION_INFO& vkMemReq, const AllocatorMemoryRequirements& resourceAllocMemReq,
		AllocatorSuballocationType suballocType, ResourceAllocation** pAllocation);

	// Main deallocation function.
	void FreeMemory(ResourceAllocation* allocation);

	void CalculateStats(AllocatorStats* pStats);

#if RESOURCE_STATS_STRING_ENABLED
	void PrintDetailedMap(class AllocatorStringBuilder& sb);
#endif

	void    UnmapPersistentlyMappedMemory();
	HRESULT MapPersistentlyMappedMemory();

	static void GetAllocationInfo(ResourceAllocation* hAllocation, ResourceAllocationInfo* pAllocationInfo);

	private:
#ifdef _DURANGO
	IDXGIAdapter* m_PhysicalDevice;
#else
	IDXGIAdapter3* m_PhysicalDevice;
#endif

	HRESULT AllocateMemoryOfType(
		const D3D12_RESOURCE_ALLOCATION_INFO& vkMemReq, const AllocatorMemoryRequirements& resourceAllocMemReq, uint32_t memTypeIndex,
		AllocatorSuballocationType suballocType, ResourceAllocation** pAllocation);

	// Allocates and registers new VkDeviceMemory specifically for single allocation.
	HRESULT AllocateOwnMemory(
		UINT64 size, AllocatorSuballocationType suballocType, uint32_t memTypeIndex, bool map, void* pUserData,
		ResourceAllocation** pAllocation);

	// Tries to free pMemory as Own Memory. Returns true if found and freed.
	void FreeOwnMemory(ResourceAllocation* allocation);
};

////////////////////////////////////////////////////////////////////////////////
// Memory allocation #2 after Allocator_T definition

////////////////////////////////////////////////////////////////////////////////
// AllocatorStringBuilder

#if RESOURCE_STATS_STRING_ENABLED

class AllocatorStringBuilder
{
	public:
	AllocatorStringBuilder(ResourceAllocator* alloc) { UNREF_PARAM(alloc); }
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

typedef struct AllocatorHeapProperties
{
	UINT64                mBlockSize;
	D3D12_HEAP_FLAGS      mFlags;
	D3D12_HEAP_PROPERTIES mProps;
} AllocatorHeapProperties;

static const AllocatorHeapProperties gHeapProperties[RESOURCE_MEMORY_TYPE_NUM_TYPES] = {
	/// Buffer
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
		{ D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 },
	},
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
		{ D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 },
	},
	{
		RESOURCE_DEFAULT_SMALL_HEAP_BLOCK_SIZE,
		D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
		{ D3D12_HEAP_TYPE_READBACK, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 },
	},
	/// Texture Small
	{
		RESOURCE_DEFAULT_SMALL_HEAP_BLOCK_SIZE,
		D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES,
		{ D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 },
	},
	/// Texture Default
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES,
		{ D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 },
	},
	/// Texture MSAA
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES,
		{ D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 },
	},
	/// RTV DSV
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES,
		{ D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 },
	},
	/// RTV DSV MSAA
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES,
		{ D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 },
	},
	/// RTV DSV Shared
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_SHARED,
		{ D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 },
	},
	/// RTV DSV Shared MSAA
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_SHARED,
		{ D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 },
	},
	/// RTV DSV Shared Adapter
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER,
		{ D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 },
	},
	/// RTV DSV Shared Adapter MSAA
	{
		RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
		D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER,
		{ D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 },
	},
	/// UAV Buffer
	{ RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
	  D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
	  { D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 } },
	{ RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
	  D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
	  { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 } },
	{ RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE,
	  D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
	  { D3D12_HEAP_TYPE_READBACK, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 } },
};

#endif    // #if RESOURCE_STATS_STRING_ENABLED

static HRESULT AllocateMemoryForImage(
	ResourceAllocator* allocator, const D3D12_RESOURCE_DESC* desc, const AllocatorMemoryRequirements* pMemoryRequirements,
	AllocatorSuballocationType suballocType, ResourceAllocation** pAllocation)
{
	ASSERT(allocator && desc && pMemoryRequirements && pAllocation);

	D3D12_RESOURCE_ALLOCATION_INFO info = allocator->m_hDevice->GetResourceAllocationInfo(0, 1, desc);
	if (fnHookResourceAllocationInfo != NULL)
		fnHookResourceAllocationInfo(info, desc->Alignment);

	return allocator->AllocateMemory(info, *pMemoryRequirements, suballocType, pAllocation);
}

////////////////////////////////////////////////////////////////////////////////
// Public interface

#endif    // #ifdef RESOURCE_IMPLEMENTATION
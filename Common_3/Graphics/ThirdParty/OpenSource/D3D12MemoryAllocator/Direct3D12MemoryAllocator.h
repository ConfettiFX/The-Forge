//
// Copyright (c) 2019-2022 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "../../../GraphicsConfig.h"
#include "../../../../Utilities/Interfaces/IThread.h"

#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#if defined(XBOX)
#define D3D12MA_DXGI_1_4 0
#endif

#define D3D12MAAllocator D3D12MA::Allocator
#define D3D12MAAllocation D3D12MA::Allocation

/** \mainpage D3D12 Memory Allocator

<b>Version 2.0.1</b> (2022-04-05)

Copyright (c) 2019-2022 Advanced Micro Devices, Inc. All rights reserved. \n
License: MIT

Documentation of all members: D3D12MemAlloc.h

\section main_table_of_contents Table of contents

- \subpage quick_start
    - [Project setup](@ref quick_start_project_setup)
    - [Creating resources](@ref quick_start_creating_resources)
    - [Resource reference counting](@ref quick_start_resource_reference_counting)
    - [Mapping memory](@ref quick_start_mapping_memory)
- \subpage custom_pools
- \subpage defragmentation
- \subpage statistics
- \subpage resource_aliasing
- \subpage linear_algorithm
- \subpage virtual_allocator
- \subpage configuration
  - [Custom CPU memory allocator](@ref custom_memory_allocator)
  - [Debug margins](@ref debug_margins)
- \subpage general_considerations
  - [Thread safety](@ref general_considerations_thread_safety)
  - [Versioning and compatibility](@ref general_considerations_versioning_and_compatibility)
  - [Features not supported](@ref general_considerations_features_not_supported)
		
\section main_see_also See also

- [Product page on GPUOpen](https://gpuopen.com/gaming-product/d3d12-memory-allocator/)
- [Source repository on GitHub](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator)
*/

// If using this library on a platform different than Windows PC or want to use different version of DXGI,
// you should include D3D12-compatible headers before this library on your own and define this macro.
#ifndef D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
    #include <d3d12.h>
    #include <dxgi1_4.h>
#endif

// Define this macro to 0 to disable usage of DXGI 1.4 (needed for IDXGIAdapter3 and query for memory budget).
#ifndef D3D12MA_DXGI_1_4
    #ifdef __IDXGIAdapter3_INTERFACE_DEFINED__
        #define D3D12MA_DXGI_1_4 1
    #else
        #define D3D12MA_DXGI_1_4 0
    #endif
#endif

/*
When defined to value other than 0, the library will try to use
D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT or D3D12_SMALL_MSAA_RESOURCE_PLACEMENT_ALIGNMENT
for created textures when possible, which can save memory because some small textures
may get their alignment 4K and their size a multiply of 4K instead of 64K.

#define D3D12MA_USE_SMALL_RESOURCE_PLACEMENT_ALIGNMENT 0
    Disables small texture alignment.
#define D3D12MA_USE_SMALL_RESOURCE_PLACEMENT_ALIGNMENT 1
    Enables conservative algorithm that will use small alignment only for some textures
    that are surely known to support it.
#define D3D12MA_USE_SMALL_RESOURCE_PLACEMENT_ALIGNMENT 2
    Enables query for small alignment to D3D12 (based on Microsoft sample) which will
    enable small alignment for more textures, but will also generate D3D Debug Layer
    error #721 on call to ID3D12Device::GetResourceAllocationInfo, which you should just
    ignore.
*/
#ifndef D3D12MA_USE_SMALL_RESOURCE_PLACEMENT_ALIGNMENT
    #define D3D12MA_USE_SMALL_RESOURCE_PLACEMENT_ALIGNMENT 1
#endif

/// \cond INTERNAL

#define D3D12MA_CLASS_NO_COPY(className) \
    private: \
        className(const className&) = delete; \
        className(className&&) = delete; \
        className& operator=(const className&) = delete; \
        className& operator=(className&&) = delete;

// To be used with MAKE_HRESULT to define custom error codes.
#define FACILITY_D3D12MA 3542

/*
If providing your own implementation, you need to implement a subset of std::atomic.
*/
#if !defined(D3D12MA_ATOMIC_UINT32) || !defined(D3D12MA_ATOMIC_UINT64)
    #include <atomic>
#endif

#ifndef D3D12MA_ATOMIC_UINT32
    #define D3D12MA_ATOMIC_UINT32 std::atomic<UINT>
#endif

#ifndef D3D12MA_ATOMIC_UINT64
    #define D3D12MA_ATOMIC_UINT64 std::atomic<UINT64>
#endif

#ifdef D3D12MA_EXPORTS
    #define D3D12MA_API __declspec(dllexport)
#elif defined(D3D12MA_IMPORTS)
    #define D3D12MA_API __declspec(dllimport)
#else
    #define D3D12MA_API
#endif

// Forward declaration if ID3D12ProtectedResourceSession is not defined inside the headers (older SDK, pre ID3D12Device4)
struct ID3D12ProtectedResourceSession;

namespace D3D12MA
{
class D3D12MA_API IUnknownImpl : public IUnknown
{
public:
    virtual ~IUnknownImpl() = default;
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();
protected:
    virtual void ReleaseThis() { delete this; }
private:
    D3D12MA_ATOMIC_UINT32 m_RefCount = 1;
};
} // namespace D3D12MA

/// \endcond

namespace D3D12MA
{
	/// \cond INTERNAL
	class DefragmentationContextPimpl;
	class AllocatorPimpl;
	class PoolPimpl;
	class NormalBlock;
	class BlockVector;
	class CommittedAllocationList;
	class JsonWriter;
	class VirtualBlockPimpl;
	/// \endcond
	
	class Pool;
	class Allocator;
	struct Statistics;
	struct DetailedStatistics;
	struct TotalStatistics;

/// \brief Unique identifier of single allocation done inside the memory heap.
typedef UINT64 AllocHandle;

/// Pointer to custom callback function that allocates CPU memory.
using ALLOCATE_FUNC_PTR = void* (*)(size_t Size, size_t Alignment, void* pPrivateData);
/**
\brief Pointer to custom callback function that deallocates CPU memory.

`pMemory = null` should be accepted and ignored.
*/
using FREE_FUNC_PTR = void (*)(void* pMemory, void* pPrivateData);

/// Custom callbacks to CPU memory allocation functions.
struct ALLOCATION_CALLBACKS
{
    /// %Allocation function.
    ALLOCATE_FUNC_PTR pAllocate;
    /// Dellocation function.
    FREE_FUNC_PTR pFree;
    /// Custom data that will be passed to allocation and deallocation functions as `pUserData` parameter.
    void* pPrivateData;
};


/// \brief Bit flags to be used with ALLOCATION_DESC::Flags.
enum ALLOCATION_FLAGS
{
    /// Zero
    ALLOCATION_FLAG_NONE = 0,

    /**
    Set this flag if the allocation should have its own dedicated memory allocation (committed resource with implicit heap).
    
    Use it for special, big resources, like fullscreen textures used as render targets.

    - When used with functions like D3D12MA::Allocator::CreateResource, it will use `ID3D12Device::CreateCommittedResource`,
      so the created allocation will contain a resource (D3D12MA::Allocation::GetResource() `!= NULL`) but will not have
      a heap (D3D12MA::Allocation::GetHeap() `== NULL`), as the heap is implicit.
    - When used with raw memory allocation like D3D12MA::Allocator::AllocateMemory, it will use `ID3D12Device::CreateHeap`,
      so the created allocation will contain a heap (D3D12MA::Allocation::GetHeap() `!= NULL`) and its offset will always be 0.
    */
    ALLOCATION_FLAG_COMMITTED = 0x1,

    /**
    Set this flag to only try to allocate from existing memory heaps and never create new such heap.

    If new allocation cannot be placed in any of the existing heaps, allocation
    fails with `E_OUTOFMEMORY` error.

    You should not use D3D12MA::ALLOCATION_FLAG_COMMITTED and
    D3D12MA::ALLOCATION_FLAG_NEVER_ALLOCATE at the same time. It makes no sense.
    */
    ALLOCATION_FLAG_NEVER_ALLOCATE = 0x2,

    /** Create allocation only if additional memory required for it, if any, won't exceed
    memory budget. Otherwise return `E_OUTOFMEMORY`.
    */
    ALLOCATION_FLAG_WITHIN_BUDGET = 0x4,

    /** Allocation will be created from upper stack in a double stack pool.

    This flag is only allowed for custom pools created with #POOL_FLAG_ALGORITHM_LINEAR flag.
    */
    ALLOCATION_FLAG_UPPER_ADDRESS = 0x8,

    /** Set this flag if the allocated memory will have aliasing resources.
    
    Use this when calling D3D12MA::Allocator::CreateResource() and similar to
    guarantee creation of explicit heap for desired allocation and prevent it from using `CreateCommittedResource`,
    so that new allocation object will always have `allocation->GetHeap() != NULL`.
    */
    ALLOCATION_FLAG_CAN_ALIAS = 0x10,

    /** Allocation strategy that chooses smallest possible free range for the allocation
    to minimize memory usage and fragmentation, possibly at the expense of allocation time.
    */
    ALLOCATION_FLAG_STRATEGY_MIN_MEMORY = 0x00010000,

    /** Allocation strategy that chooses first suitable free range for the allocation -
    not necessarily in terms of the smallest offset but the one that is easiest and fastest to find
    to minimize allocation time, possibly at the expense of allocation quality.
    */
    ALLOCATION_FLAG_STRATEGY_MIN_TIME = 0x00020000,

    /** Allocation strategy that chooses always the lowest offset in available space.
    This is not the most efficient strategy but achieves highly packed data.
    Used internally by defragmentation, not recomended in typical usage.
    */
    ALLOCATION_FLAG_STRATEGY_MIN_OFFSET = 0x0004000,

    /// Alias to #ALLOCATION_FLAG_STRATEGY_MIN_MEMORY.
    ALLOCATION_FLAG_STRATEGY_BEST_FIT = ALLOCATION_FLAG_STRATEGY_MIN_MEMORY,
    /// Alias to #ALLOCATION_FLAG_STRATEGY_MIN_TIME.
    ALLOCATION_FLAG_STRATEGY_FIRST_FIT = ALLOCATION_FLAG_STRATEGY_MIN_TIME,

    /// A bit mask to extract only `STRATEGY` bits from entire set of flags.
    ALLOCATION_FLAG_STRATEGY_MASK =
        ALLOCATION_FLAG_STRATEGY_MIN_MEMORY |
        ALLOCATION_FLAG_STRATEGY_MIN_TIME |
        ALLOCATION_FLAG_STRATEGY_MIN_OFFSET,
};

/// \brief Parameters of created D3D12MA::Allocation object. To be used with Allocator::CreateResource.
struct ALLOCATION_DESC
{
    /// Flags.
    ALLOCATION_FLAGS Flags;
    /** \brief The type of memory heap where the new allocation should be placed.

    It must be one of: `D3D12_HEAP_TYPE_DEFAULT`, `D3D12_HEAP_TYPE_UPLOAD`, `D3D12_HEAP_TYPE_READBACK`.

    When D3D12MA::ALLOCATION_DESC::CustomPool != NULL this member is ignored.
    */
    D3D12_HEAP_TYPE HeapType;
    /** \brief Additional heap flags to be used when allocating memory.

    In most cases it can be 0.
    
    - If you use D3D12MA::Allocator::CreateResource(), you don't need to care.
      Necessary flag `D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS`, `D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES`,
      or `D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES` is added automatically.
    - If you use D3D12MA::Allocator::AllocateMemory(), you should specify one of those `ALLOW_ONLY` flags.
      Except when you validate that D3D12MA::Allocator::GetD3D12Options()`.ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_1` -
      then you can leave it 0.
    - You can specify additional flags if needed. Then the memory will always be allocated as
      separate block using `D3D12Device::CreateCommittedResource` or `CreateHeap`, not as part of an existing larget block.

    When D3D12MA::ALLOCATION_DESC::CustomPool != NULL this member is ignored.
    */
    D3D12_HEAP_FLAGS ExtraHeapFlags;
    /** \brief Custom pool to place the new resource in. Optional.

    When not NULL, the resource will be created inside specified custom pool.
    It will then never be created as committed.
    */
    Pool* CustomPool;
    /// Custom general-purpose pointer that will be stored in D3D12MA::Allocation.
    void* pPrivateData;

    // CONFFX_BEGIN - Multi GPU
    UINT CreationNodeMask;
    UINT VisibleNodeMask;
    // CONFFX_END
};

/** \brief Calculated statistics of memory usage e.g. in a specific memory heap type,
memory segment group, custom pool, or total.

These are fast to calculate.
See functions: D3D12MA::Allocator::GetBudget(), D3D12MA::Pool::GetStatistics().
*/
struct Statistics
{
    /** \brief Number of D3D12 memory blocks allocated - `ID3D12Heap` objects and committed resources.
    */
    UINT BlockCount;
    /** \brief Number of D3D12MA::Allocation objects allocated.

    Committed allocations have their own blocks, so each one adds 1 to `AllocationCount` as well as `BlockCount`.
    */
    UINT AllocationCount;
    /** \brief Number of bytes allocated in memory blocks.
    */
    UINT64 BlockBytes;
    /** \brief Total number of bytes occupied by all D3D12MA::Allocation objects.

    Always less or equal than `BlockBytes`.
    Difference `(BlockBytes - AllocationBytes)` is the amount of memory allocated from D3D12
    but unused by any D3D12MA::Allocation.
    */
    UINT64 AllocationBytes;
};

/** \brief More detailed statistics than D3D12MA::Statistics.

These are slower to calculate. Use for debugging purposes.
See functions: D3D12MA::Allocator::CalculateStatistics(), D3D12MA::Pool::CalculateStatistics().

Averages are not provided because they can be easily calculated as:

\code
UINT64 AllocationSizeAvg = DetailedStats.Statistics.AllocationBytes / detailedStats.Statistics.AllocationCount;
UINT64 UnusedBytes = DetailedStats.Statistics.BlockBytes - DetailedStats.Statistics.AllocationBytes;
UINT64 UnusedRangeSizeAvg = UnusedBytes / DetailedStats.UnusedRangeCount;
\endcode
*/
struct DetailedStatistics
{
    /// Basic statistics.
    Statistics Stats;
    /// Number of free ranges of memory between allocations.
    UINT UnusedRangeCount;
    /// Smallest allocation size. `UINT64_MAX` if there are 0 allocations.
    UINT64 AllocationSizeMin;
    /// Largest allocation size. 0 if there are 0 allocations.
    UINT64 AllocationSizeMax;
    /// Smallest empty range size. `UINT64_MAX` if there are 0 empty ranges.
    UINT64 UnusedRangeSizeMin;
    /// Largest empty range size. 0 if there are 0 empty ranges.
    UINT64 UnusedRangeSizeMax;
};

/** \brief  General statistics from current state of the allocator -
total memory usage across all memory heaps and segments.

These are slower to calculate. Use for debugging purposes.
See function D3D12MA::Allocator::CalculateStatistics().
*/
struct TotalStatistics
{
    /** \brief One element for each type of heap located at the following indices:

    - 0 = `D3D12_HEAP_TYPE_DEFAULT`
    - 1 = `D3D12_HEAP_TYPE_UPLOAD`
    - 2 = `D3D12_HEAP_TYPE_READBACK`
    - 3 = `D3D12_HEAP_TYPE_CUSTOM`
    */
    DetailedStatistics HeapType[4];
    /** \brief One element for each memory segment group located at the following indices:

    - 0 = `DXGI_MEMORY_SEGMENT_GROUP_LOCAL`
    - 1 = `DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL`

    Meaning of these segment groups is:

    - When `IsUMA() == FALSE` (discrete graphics card):
      - `DXGI_MEMORY_SEGMENT_GROUP_LOCAL` (index 0) represents GPU memory
      (resources allocated in `D3D12_HEAP_TYPE_DEFAULT` or `D3D12_MEMORY_POOL_L1`).
      - `DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL` (index 1) represents system memory
      (resources allocated in `D3D12_HEAP_TYPE_UPLOAD`, `D3D12_HEAP_TYPE_READBACK`, or `D3D12_MEMORY_POOL_L0`).
    - When `IsUMA() == TRUE` (integrated graphics chip):
      - `DXGI_MEMORY_SEGMENT_GROUP_LOCAL` = (index 0) represents memory shared for all the resources.
      - `DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL` = (index 1) is unused and always 0.
    */
    DetailedStatistics MemorySegmentGroup[2];
    /// Total statistics from all memory allocated from D3D12.
    DetailedStatistics Total;
};

/** \brief %Statistics of current memory usage and available budget for a specific memory segment group.

These are fast to calculate. See function D3D12MA::Allocator::GetBudget().
*/
struct Budget
{
    /** \brief %Statistics fetched from the library.
    */
    Statistics Stats;
    /** \brief Estimated current memory usage of the program.

    Fetched from system using `IDXGIAdapter3::QueryVideoMemoryInfo` if possible.

    It might be different than `BlockBytes` (usually higher) due to additional implicit objects
    also occupying the memory, like swapchain, pipeline state objects, descriptor heaps, command lists, or
    heaps and resources allocated outside of this library, if any.
    */
    UINT64 UsageBytes;
    /** \brief Estimated amount of memory available to the program.

    Fetched from system using `IDXGIAdapter3::QueryVideoMemoryInfo` if possible.

    It might be different (most probably smaller) than memory capacity returned
    by D3D12MA::Allocator::GetMemoryCapacity() due to factors
    external to the program, decided by the operating system.
    Difference `BudgetBytes - UsageBytes` is the amount of additional memory that can probably
    be allocated without problems. Exceeding the budget may result in various problems.
    */
    UINT64 BudgetBytes;
};


/// \brief Represents single memory allocation done inside VirtualBlock.
struct D3D12MA_API VirtualAllocation
{
    /// \brief Unique idenitfier of current allocation. 0 means null/invalid.
    AllocHandle AllocHandle;
};

/** \brief Represents single memory allocation.

It may be either implicit memory heap dedicated to a single resource or a
specific region of a bigger heap plus unique offset.

To create such object, fill structure D3D12MA::ALLOCATION_DESC and call function
Allocator::CreateResource.

The object remembers size and some other information.
To retrieve this information, use methods of this class.

The object also remembers `ID3D12Resource` and "owns" a reference to it,
so it calls `%Release()` on the resource when destroyed.
*/
class D3D12MA_API Allocation : public IUnknownImpl
{
public:
    /** \brief Returns offset in bytes from the start of memory heap.

    You usually don't need to use this offset. If you create a buffer or a texture together with the allocation using function
    D3D12MA::Allocator::CreateResource, functions that operate on that resource refer to the beginning of the resource,
    not entire memory heap.

    If the Allocation represents committed resource with implicit heap, returns 0.
    */
    UINT64 GetOffset() const;

    /// Returns alignment that resource was created with.
    UINT64 GetAlignment() const { return m_Alignment; }

    /** \brief Returns size in bytes of the allocation.

    - If you created a buffer or a texture together with the allocation using function D3D12MA::Allocator::CreateResource,
      this is the size of the resource returned by `ID3D12Device::GetResourceAllocationInfo`.
    - For allocations made out of bigger memory blocks, this also is the size of the memory region assigned exclusively to this allocation.
    - For resources created as committed, this value may not be accurate. DirectX implementation may optimize memory usage internally
      so that you may even observe regions of `ID3D12Resource::GetGPUVirtualAddress()` + Allocation::GetSize() to overlap in memory and still work correctly.
    */
    UINT64 GetSize() const { return m_Size; }

    /** \brief Returns D3D12 resource associated with this object.

    Calling this method doesn't increment resource's reference counter.
    */
    ID3D12Resource* GetResource() const { return m_Resource; }

    /// Releases the resource currently pointed by the allocation (if any), sets it to new one, incrementing its reference counter (if not null).
    void SetResource(ID3D12Resource* pResource);

    /** \brief Returns memory heap that the resource is created in.

    If the Allocation represents committed resource with implicit heap, returns NULL.
    */
    ID3D12Heap* GetHeap() const;

    /// Changes custom pointer for an allocation to a new value.
    void SetPrivateData(void* pPrivateData) { m_pPrivateData = pPrivateData; }

    /// Get custom pointer associated with the allocation.
    void* GetPrivateData() const { return m_pPrivateData; }

    /** \brief Associates a name with the allocation object. This name is for use in debug diagnostics and tools.

    Internal copy of the string is made, so the memory pointed by the argument can be
    changed of freed immediately after this call.

    `Name` can be null.
    */
    void SetName(LPCWSTR Name);

    /** \brief Returns the name associated with the allocation object.

    Returned string points to an internal copy.

    If no name was associated with the allocation, returns null.
    */
    LPCWSTR GetName() const { return m_Name; }

    /** \brief Returns `TRUE` if the memory of the allocation was filled with zeros when the allocation was created.

    Returns `TRUE` only if the allocator is sure that the entire memory where the
    allocation was created was filled with zeros at the moment the allocation was made.
    
    Returns `FALSE` if the memory could potentially contain garbage data.
    If it's a render-target or depth-stencil texture, it then needs proper
    initialization with `ClearRenderTargetView`, `ClearDepthStencilView`, `DiscardResource`,
    or a copy operation, as described on page
    "ID3D12Device::CreatePlacedResource method - Notes on the required resource initialization" in Microsoft documentation.
    Please note that rendering a fullscreen triangle or quad to the texture as
    a render target is not a proper way of initialization!

    See also articles:

    - "Coming to DirectX 12: More control over memory allocation" on DirectX Developer Blog
    - ["Initializing DX12 Textures After Allocation and Aliasing"](https://asawicki.info/news_1724_initializing_dx12_textures_after_allocation_and_aliasing).
    */
    BOOL WasZeroInitialized() const { return m_PackedData.WasZeroInitialized(); }

    ALLOCATION_DESC GetAllocationDesc() const { return m_AllocDesc; }

protected:
    void ReleaseThis() override;

private:
    friend class AllocatorPimpl;
    friend class BlockVector;
    friend class CommittedAllocationList;
    friend class JsonWriter;
    friend class BlockMetadata_Linear;
    friend class DefragmentationContextPimpl;
    friend struct CommittedAllocationListItemTraits;
    template<typename T> friend void D3D12MA_DELETE(const ALLOCATION_CALLBACKS&, T*);
    template<typename T> friend class PoolAllocator;

    enum Type
    {
        TYPE_COMMITTED,
        TYPE_PLACED,
        TYPE_HEAP,
        TYPE_COUNT
    };

    AllocatorPimpl* m_Allocator;
    UINT64 m_Size;
    UINT64 m_Alignment;
    ID3D12Resource* m_Resource;
    void* m_pPrivateData;
    wchar_t* m_Name;
    // CONFFX_BEGIN - Linked multi gpu
    ALLOCATION_DESC m_AllocDesc;
    // CONFFX_END
    

    union
    {
        struct
        {
            CommittedAllocationList* list;
            Allocation* prev;
            Allocation* next;
        } m_Committed;

        struct
        {
            AllocHandle allocHandle;
            NormalBlock* block;
        } m_Placed;

        struct
        {
            // Beginning must be compatible with m_Committed.
            CommittedAllocationList* list;
            Allocation* prev;
            Allocation* next;
            ID3D12Heap* heap;
        } m_Heap;
    };

    struct PackedData
    {
    public:
        PackedData() :
            m_Type(0), m_ResourceDimension(0), m_ResourceFlags(0), m_TextureLayout(0), m_WasZeroInitialized(0) { }

        Type GetType() const { return (Type)m_Type; }
        D3D12_RESOURCE_DIMENSION GetResourceDimension() const { return (D3D12_RESOURCE_DIMENSION)m_ResourceDimension; }
        D3D12_RESOURCE_FLAGS GetResourceFlags() const { return (D3D12_RESOURCE_FLAGS)m_ResourceFlags; }
        D3D12_TEXTURE_LAYOUT GetTextureLayout() const { return (D3D12_TEXTURE_LAYOUT)m_TextureLayout; }
        BOOL WasZeroInitialized() const { return (BOOL)m_WasZeroInitialized; }

        void SetType(Type type);
        void SetResourceDimension(D3D12_RESOURCE_DIMENSION resourceDimension);
        void SetResourceFlags(D3D12_RESOURCE_FLAGS resourceFlags);
        void SetTextureLayout(D3D12_TEXTURE_LAYOUT textureLayout);
        void SetWasZeroInitialized(BOOL wasZeroInitialized) { m_WasZeroInitialized = wasZeroInitialized ? 1 : 0; }

    private:
        UINT m_Type : 2;               // enum Type
        UINT m_ResourceDimension : 3;  // enum D3D12_RESOURCE_DIMENSION
        UINT m_ResourceFlags : 24;     // flags D3D12_RESOURCE_FLAGS
        UINT m_TextureLayout : 9;      // enum D3D12_TEXTURE_LAYOUT
        UINT m_WasZeroInitialized : 1; // BOOL
    } m_PackedData;

    Allocation(AllocatorPimpl* allocator, UINT64 size, UINT64 alignment, BOOL wasZeroInitialized);
    //  Nothing here, everything already done in Release.
    virtual ~Allocation() = default;

    void InitCommitted(CommittedAllocationList* list);
    void InitPlaced(AllocHandle allocHandle, NormalBlock* block);
    void InitHeap(CommittedAllocationList* list, ID3D12Heap* heap);
    void SwapBlockAllocation(Allocation* allocation);
    // If the Allocation represents committed resource with implicit heap, returns UINT64_MAX.
    AllocHandle GetAllocHandle() const;
    NormalBlock* GetBlock();
    template<typename D3D12_RESOURCE_DESC_T>
    void SetResourcePointer(ID3D12Resource* resource, const D3D12_RESOURCE_DESC_T* pResourceDesc);
    void FreeName();

    D3D12MA_CLASS_NO_COPY(Allocation)
};


/// Flags to be passed as DEFRAGMENTATION_DESC::Flags.
enum DEFRAGMENTATION_FLAGS
{
    /** Use simple but fast algorithm for defragmentation.
    May not achieve best results but will require least time to compute and least allocations to copy.
    */
    DEFRAGMENTATION_FLAG_ALGORITHM_FAST = 0x1,
    /** Default defragmentation algorithm, applied also when no `ALGORITHM` flag is specified.
    Offers a balance between defragmentation quality and the amount of allocations and bytes that need to be moved.
    */
    DEFRAGMENTATION_FLAG_ALGORITHM_BALANCED = 0x2,
    /** Perform full defragmentation of memory.
    Can result in notably more time to compute and allocations to copy, but will achieve best memory packing.
    */
    DEFRAGMENTATION_FLAG_ALGORITHM_FULL = 0x4,

    /// A bit mask to extract only `ALGORITHM` bits from entire set of flags.
    DEFRAGMENTATION_FLAG_ALGORITHM_MASK =
        DEFRAGMENTATION_FLAG_ALGORITHM_FAST |
        DEFRAGMENTATION_FLAG_ALGORITHM_BALANCED |
        DEFRAGMENTATION_FLAG_ALGORITHM_FULL
};

/** \brief Parameters for defragmentation.

To be used with functions Allocator::BeginDefragmentation() and Pool::BeginDefragmentation().
*/
struct DEFRAGMENTATION_DESC
{
    /// Flags.
    DEFRAGMENTATION_FLAGS Flags;
    /** \brief Maximum numbers of bytes that can be copied during single pass, while moving allocations to different places.

    0 means no limit.
    */
    UINT64 MaxBytesPerPass;
    /** \brief Maximum number of allocations that can be moved during single pass to a different place.

    0 means no limit.
    */
    UINT32 MaxAllocationsPerPass;
};

/// Operation performed on single defragmentation move.
enum DEFRAGMENTATION_MOVE_OPERATION
{
    /** Resource has been recreated at `pDstTmpAllocation`, data has been copied, old resource has been destroyed.
    `pSrcAllocation` will be changed to point to the new place. This is the default value set by DefragmentationContext::BeginPass().
    */
    DEFRAGMENTATION_MOVE_OPERATION_COPY = 0,
    /// Set this value if you cannot move the allocation. New place reserved at `pDstTmpAllocation` will be freed. `pSrcAllocation` will remain unchanged.
    DEFRAGMENTATION_MOVE_OPERATION_IGNORE = 1,
    /// Set this value if you decide to abandon the allocation and you destroyed the resource. New place reserved `pDstTmpAllocation` will be freed, along with `pSrcAllocation`.
    DEFRAGMENTATION_MOVE_OPERATION_DESTROY = 2,
};

/// Single move of an allocation to be done for defragmentation.
struct DEFRAGMENTATION_MOVE
{
    /** \brief Operation to be performed on the allocation by DefragmentationContext::EndPass().
    Default value is #DEFRAGMENTATION_MOVE_OPERATION_COPY. You can modify it.
    */
    DEFRAGMENTATION_MOVE_OPERATION Operation;
    /// %Allocation that should be moved.
    Allocation* pSrcAllocation;
    /** \brief Temporary allocation pointing to destination memory that will replace `pSrcAllocation`.

    Use it to retrieve new `ID3D12Heap` and offset to create new `ID3D12Resource` and then store it here via Allocation::SetResource().

    \warning Do not store this allocation in your data structures! It exists only temporarily, for the duration of the defragmentation pass,
    to be used for storing newly created resource. DefragmentationContext::EndPass() will destroy it and make `pSrcAllocation` point to this memory.
    */
    Allocation* pDstTmpAllocation;
};

/** \brief Parameters for incremental defragmentation steps.

To be used with function DefragmentationContext::BeginPass().
*/
struct DEFRAGMENTATION_PASS_MOVE_INFO
{
    /// Number of elements in the `pMoves` array.
    UINT32 MoveCount;
    /** \brief Array of moves to be performed by the user in the current defragmentation pass.

    Pointer to an array of `MoveCount` elements, owned by %D3D12MA, created in DefragmentationContext::BeginPass(), destroyed in DefragmentationContext::EndPass().

    For each element, you should:

    1. Create a new resource in the place pointed by `pMoves[i].pDstTmpAllocation->GetHeap()` + `pMoves[i].pDstTmpAllocation->GetOffset()`.
    2. Store new resource in `pMoves[i].pDstTmpAllocation` by using Allocation::SetResource(). It will later replace old resource from `pMoves[i].pSrcAllocation`.
    3. Copy data from the `pMoves[i].pSrcAllocation` e.g. using `D3D12GraphicsCommandList::CopyResource`.
    4. Make sure these commands finished executing on the GPU.

    Only then you can finish defragmentation pass by calling DefragmentationContext::EndPass().
    After this call, the allocation will point to the new place in memory.

    Alternatively, if you cannot move specific allocation,
    you can set DEFRAGMENTATION_MOVE::Operation to D3D12MA::DEFRAGMENTATION_MOVE_OPERATION_IGNORE.

    Alternatively, if you decide you want to completely remove the allocation,
    set DEFRAGMENTATION_MOVE::Operation to D3D12MA::DEFRAGMENTATION_MOVE_OPERATION_DESTROY.
    Then, after DefragmentationContext::EndPass() the allocation will be released.
    */
    DEFRAGMENTATION_MOVE* pMoves;
};

/// %Statistics returned for defragmentation process by function DefragmentationContext::GetStats().
struct DEFRAGMENTATION_STATS
{
    /// Total number of bytes that have been copied while moving allocations to different places.
    UINT64 BytesMoved;
    /// Total number of bytes that have been released to the system by freeing empty heaps.
    UINT64 BytesFreed;
    /// Number of allocations that have been moved to different places.
    UINT32 AllocationsMoved;
    /// Number of empty `ID3D12Heap` objects that have been released to the system.
    UINT32 HeapsFreed;
};

/** \brief Represents defragmentation process in progress.

You can create this object using Allocator::BeginDefragmentation (for default pools) or
Pool::BeginDefragmentation (for a custom pool).
*/
class D3D12MA_API DefragmentationContext : public IUnknownImpl
{
public:
    /** \brief Starts single defragmentation pass.

    \param[out] pPassInfo Computed informations for current pass.
    \returns
    - `S_OK` if no more moves are possible. Then you can omit call to DefragmentationContext::EndPass() and simply end whole defragmentation.
    - `S_FALSE` if there are pending moves returned in `pPassInfo`. You need to perform them, call DefragmentationContext::EndPass(),
      and then preferably try another pass with DefragmentationContext::BeginPass().
    */
    HRESULT BeginPass(DEFRAGMENTATION_PASS_MOVE_INFO* pPassInfo);
    /** \brief Ends single defragmentation pass.

    \param pPassInfo Computed informations for current pass filled by DefragmentationContext::BeginPass() and possibly modified by you.
    \return Returns `S_OK` if no more moves are possible or `S_FALSE` if more defragmentations are possible.

    Ends incremental defragmentation pass and commits all defragmentation moves from `pPassInfo`.
    After this call:

    - %Allocation at `pPassInfo[i].pSrcAllocation` that had `pPassInfo[i].Operation ==` #DEFRAGMENTATION_MOVE_OPERATION_COPY
      (which is the default) will be pointing to the new destination place.
    - %Allocation at `pPassInfo[i].pSrcAllocation` that had `pPassInfo[i].operation ==` #DEFRAGMENTATION_MOVE_OPERATION_DESTROY
      will be released.

    If no more moves are possible you can end whole defragmentation.
    */
    HRESULT EndPass(DEFRAGMENTATION_PASS_MOVE_INFO* pPassInfo);
    /** \brief Returns statistics of the defragmentation performed so far.
    */
    void GetStats(DEFRAGMENTATION_STATS* pStats);

protected:
    void ReleaseThis() override;

private:
    friend class Pool;
    friend class Allocator;
    template<typename T> friend void D3D12MA_DELETE(const ALLOCATION_CALLBACKS&, T*);

    DefragmentationContextPimpl* m_Pimpl;

    DefragmentationContext(AllocatorPimpl* allocator,
        const DEFRAGMENTATION_DESC& desc,
        BlockVector* poolVector);
    ~DefragmentationContext();

    D3D12MA_CLASS_NO_COPY(DefragmentationContext)
};

/// \brief Bit flags to be used with POOL_DESC::Flags.
enum POOL_FLAGS
{
    /// Zero
    POOL_FLAG_NONE = 0,

    /** \brief Enables alternative, linear allocation algorithm in this pool.

    Specify this flag to enable linear allocation algorithm, which always creates
    new allocations after last one and doesn't reuse space from allocations freed in
    between. It trades memory consumption for simplified algorithm and data
    structure, which has better performance and uses less memory for metadata.

    By using this flag, you can achieve behavior of free-at-once, stack,
    ring buffer, and double stack.
    For details, see documentation chapter \ref linear_algorithm.
    */
    POOL_FLAG_ALGORITHM_LINEAR = 0x1,

    /** \brief Optimization, allocate MSAA textures as committed resources always.
    
    Specify this flag to create MSAA textures with implicit heaps, as if they were created
    with flag ALLOCATION_FLAG_COMMITTED. Usage of this flags enables pool to create its heaps
    on smaller alignment not suitable for MSAA textures.
    */
    POOL_FLAG_MSAA_TEXTURES_ALWAYS_COMMITTED = 0x2,

    // Bit mask to extract only `ALGORITHM` bits from entire set of flags.
    POOL_FLAG_ALGORITHM_MASK = POOL_FLAG_ALGORITHM_LINEAR
};

/// \brief Parameters of created D3D12MA::Pool object. To be used with D3D12MA::Allocator::CreatePool.
struct POOL_DESC
{
    /// Flags.
    POOL_FLAGS Flags;
    /** \brief The parameters of memory heap where allocations of this pool should be placed.

    In the simplest case, just fill it with zeros and set `Type` to one of: `D3D12_HEAP_TYPE_DEFAULT`,
    `D3D12_HEAP_TYPE_UPLOAD`, `D3D12_HEAP_TYPE_READBACK`. Additional parameters can be used e.g. to utilize UMA.
    */
    D3D12_HEAP_PROPERTIES HeapProperties;
    /** \brief Heap flags to be used when allocating heaps of this pool.

    It should contain one of these values, depending on type of resources you are going to create in this heap:
    `D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS`,
    `D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES`,
    `D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES`.
    Except if ResourceHeapTier = 2, then it may be `D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES` = 0.
    
    You can specify additional flags if needed.
    */
    D3D12_HEAP_FLAGS HeapFlags;
    /** \brief Size of a single heap (memory block) to be allocated as part of this pool, in bytes. Optional.

    Specify nonzero to set explicit, constant size of memory blocks used by this pool.
    Leave 0 to use default and let the library manage block sizes automatically.
    Then sizes of particular blocks may vary.
    */
    UINT64 BlockSize;
    /** \brief Minimum number of heaps (memory blocks) to be always allocated in this pool, even if they stay empty. Optional.

    Set to 0 to have no preallocated blocks and allow the pool be completely empty.
    */
    UINT MinBlockCount;
    /** \brief Maximum number of heaps (memory blocks) that can be allocated in this pool. Optional.

    Set to 0 to use default, which is `UINT64_MAX`, which means no limit.

    Set to same value as D3D12MA::POOL_DESC::MinBlockCount to have fixed amount of memory allocated
    throughout whole lifetime of this pool.
    */
    UINT MaxBlockCount;
    /** \brief Additional minimum alignment to be used for all allocations created from this pool. Can be 0.
    
    Leave 0 (default) not to impose any additional alignment. If not 0, it must be a power of two.
    */
    UINT64 MinAllocationAlignment;
    /** \brief Additional parameter allowing pool to create resources with passed protected session.
    
    If not null then all the heaps and committed resources will be created with this parameter.
    Valid only if ID3D12Device4 interface is present in current Windows SDK!
    */
    ID3D12ProtectedResourceSession* pProtectedSession;
};

/** \brief Custom memory pool

Represents a separate set of heaps (memory blocks) that can be used to create
D3D12MA::Allocation-s and resources in it. Usually there is no need to create custom
pools - creating resources in default pool is sufficient.

To create custom pool, fill D3D12MA::POOL_DESC and call D3D12MA::Allocator::CreatePool.
*/
class D3D12MA_API Pool : public IUnknownImpl
{
public:
    /** \brief Returns copy of parameters of the pool.

    These are the same parameters as passed to D3D12MA::Allocator::CreatePool.
    */
    POOL_DESC GetDesc() const;

    /** \brief Retrieves basic statistics of the custom pool that are fast to calculate.

    \param[out] pStats %Statistics of the current pool.
    */
    void GetStatistics(Statistics* pStats);

    /** \brief Retrieves detailed statistics of the custom pool that are slower to calculate.

    \param[out] pStats %Statistics of the current pool.
    */
    void CalculateStatistics(DetailedStatistics* pStats);

    /** \brief Associates a name with the pool. This name is for use in debug diagnostics and tools.

    Internal copy of the string is made, so the memory pointed by the argument can be
    changed of freed immediately after this call.

    `Name` can be NULL.
    */
    void SetName(LPCWSTR Name);

    /** \brief Returns the name associated with the pool object.

    Returned string points to an internal copy.

    If no name was associated with the allocation, returns NULL.
    */
    LPCWSTR GetName() const;

    /** \brief Begins defragmentation process of the current pool.

    \param pDesc Structure filled with parameters of defragmentation.
    \param[out] ppContext Context object that will manage defragmentation.
    \returns
    - `S_OK` if defragmentation can begin.
    - `E_NOINTERFACE` if defragmentation is not supported.

    For more information about defragmentation, see documentation chapter:
    [Defragmentation](@ref defragmentation).
    */
    HRESULT BeginDefragmentation(const DEFRAGMENTATION_DESC* pDesc, DefragmentationContext** ppContext);

protected:
    void ReleaseThis() override;

private:
    friend class Allocator;
    friend class AllocatorPimpl;
    template<typename T> friend void D3D12MA_DELETE(const ALLOCATION_CALLBACKS&, T*);

    PoolPimpl* m_Pimpl;

    Pool(Allocator* allocator, const POOL_DESC &desc);
    ~Pool();

    D3D12MA_CLASS_NO_COPY(Pool)
};


/// \brief Bit flags to be used with ALLOCATOR_DESC::Flags.
enum ALLOCATOR_FLAGS
{
    /// Zero
    ALLOCATOR_FLAG_NONE = 0,

    /**
    Allocator and all objects created from it will not be synchronized internally,
    so you must guarantee they are used from only one thread at a time or
    synchronized by you.

    Using this flag may increase performance because internal mutexes are not used.
    */
    ALLOCATOR_FLAG_SINGLETHREADED = 0x1,

    /**
    Every allocation will have its own memory block.
    To be used for debugging purposes.
    */
    ALLOCATOR_FLAG_ALWAYS_COMMITTED = 0x2,

    /**
    Heaps created for the default pools will be created with flag `D3D12_HEAP_FLAG_CREATE_NOT_ZEROED`,
    allowing for their memory to be not zeroed by the system if possible,
    which can speed up allocation.

    Only affects default pools.
    To use the flag with @ref custom_pools, you need to add it manually:

    \code
    poolDesc.heapFlags |= D3D12_HEAP_FLAG_CREATE_NOT_ZEROED;
    \endcode

    Only avaiable if `ID3D12Device8` is present. Otherwise, the flag is ignored.
    */
    ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED = 0x4,

    /** \brief Optimization, allocate MSAA textures as committed resources always.

    Specify this flag to create MSAA textures with implicit heaps, as if they were created
    with flag ALLOCATION_FLAG_COMMITTED. Usage of this flags enables all default pools
    to create its heaps on smaller alignment not suitable for MSAA textures.
    */
    ALLOCATOR_FLAG_MSAA_TEXTURES_ALWAYS_COMMITTED = 0x8,
};

/// \brief Parameters of created Allocator object. To be used with CreateAllocator().
struct ALLOCATOR_DESC
{
    /// Flags.
    ALLOCATOR_FLAGS Flags;
    
    /** Direct3D device object that the allocator should be attached to.

    Allocator is doing `AddRef`/`Release` on this object.
    */
    ID3D12Device* pDevice;
    
    /** \brief Preferred size of a single `ID3D12Heap` block to be allocated.
    
    Set to 0 to use default, which is currently 64 MiB.
    */
    UINT64 PreferredBlockSize;
    
    /** \brief Custom CPU memory allocation callbacks. Optional.

    Optional, can be null. When specified, will be used for all CPU-side memory allocations.
    */
    const ALLOCATION_CALLBACKS* pAllocationCallbacks;

    /** DXGI Adapter object that you use for D3D12 and this allocator.

    Allocator is doing `AddRef`/`Release` on this object.
    */
    IDXGIAdapter* pAdapter;
};

/**
\brief Represents main object of this library initialized for particular `ID3D12Device`.

Fill structure D3D12MA::ALLOCATOR_DESC and call function CreateAllocator() to create it.
Call method `Release()` to destroy it.

It is recommended to create just one object of this type per `ID3D12Device` object,
right after Direct3D 12 is initialized and keep it alive until before Direct3D device is destroyed.
*/
class D3D12MA_API Allocator : public IUnknownImpl
{
public:
    /// Returns cached options retrieved from D3D12 device.
    const D3D12_FEATURE_DATA_D3D12_OPTIONS& GetD3D12Options() const;
    /** \brief Returns true if `D3D12_FEATURE_DATA_ARCHITECTURE1::UMA` was found to be true.
    
    For more information about how to use it, see articles in Microsoft Docs articles:

    - "UMA Optimizations: CPU Accessible Textures and Standard Swizzle"
    - "D3D12_FEATURE_DATA_ARCHITECTURE structure (d3d12.h)"
    - "ID3D12Device::GetCustomHeapProperties method (d3d12.h)"
    */
    BOOL IsUMA() const;
    /** \brief Returns true if `D3D12_FEATURE_DATA_ARCHITECTURE1::CacheCoherentUMA` was found to be true.

    For more information about how to use it, see articles in Microsoft Docs articles:

    - "UMA Optimizations: CPU Accessible Textures and Standard Swizzle"
    - "D3D12_FEATURE_DATA_ARCHITECTURE structure (d3d12.h)"
    - "ID3D12Device::GetCustomHeapProperties method (d3d12.h)"
    */
    BOOL IsCacheCoherentUMA() const;
    /** \brief Returns total amount of memory of specific segment group, in bytes.
    
    \param memorySegmentGroup use `DXGI_MEMORY_SEGMENT_GROUP_LOCAL` or DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL`.

    This information is taken from `DXGI_ADAPTER_DESC`.
    It is not recommended to use this number.
    You should preferably call GetBudget() and limit memory usage to D3D12MA::Budget::BudgetBytes instead.

    - When IsUMA() `== FALSE` (discrete graphics card):
      - `GetMemoryCapacity(DXGI_MEMORY_SEGMENT_GROUP_LOCAL)` returns the size of the video memory.
      - `GetMemoryCapacity(DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL)` returns the size of the system memory available for D3D12 resources.
    - When IsUMA() `== TRUE` (integrated graphics chip):
      - `GetMemoryCapacity(DXGI_MEMORY_SEGMENT_GROUP_LOCAL)` returns the size of the shared memory available for all D3D12 resources.
        All memory is considered "local".
      - `GetMemoryCapacity(DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL)` is not applicable and returns 0.
    */
    UINT64 GetMemoryCapacity(UINT memorySegmentGroup) const;

    /** \brief Allocates memory and creates a D3D12 resource (buffer or texture). This is the main allocation function.

    The function is similar to `ID3D12Device::CreateCommittedResource`, but it may
    really call `ID3D12Device::CreatePlacedResource` to assign part of a larger,
    existing memory heap to the new resource, which is the main purpose of this
    whole library.

    If `ppvResource` is null, you receive only `ppAllocation` object from this function.
    It holds pointer to `ID3D12Resource` that can be queried using function D3D12MA::Allocation::GetResource().
    Reference count of the resource object is 1.
    It is automatically destroyed when you destroy the allocation object.

    If `ppvResource` is not null, you receive pointer to the resource next to allocation object.
    Reference count of the resource object is then increased by calling `QueryInterface`, so you need to manually `Release` it
    along with the allocation.

    \param pAllocDesc   Parameters of the allocation.
    \param pResourceDesc   Description of created resource.
    \param InitialResourceState   Initial resource state.
    \param pOptimizedClearValue   Optional. Either null or optimized clear value.
    \param[out] ppAllocation   Filled with pointer to new allocation object created.
    \param riidResource   IID of a resource to be returned via `ppvResource`.
    \param[out] ppvResource   Optional. If not null, filled with pointer to new resouce created.

    \note This function creates a new resource. Sub-allocation of parts of one large buffer,
    although recommended as a good practice, is out of scope of this library and could be implemented
    by the user as a higher-level logic on top of it, e.g. using the \ref virtual_allocator feature.
    */
    HRESULT CreateResource(
        const ALLOCATION_DESC* pAllocDesc,
        const D3D12_RESOURCE_DESC* pResourceDesc,
        D3D12_RESOURCE_STATES InitialResourceState,
        const D3D12_CLEAR_VALUE *pOptimizedClearValue,
        Allocation** ppAllocation,
        REFIID riidResource,
        void** ppvResource);

#ifdef __ID3D12Device8_INTERFACE_DEFINED__
    /** \brief Similar to Allocator::CreateResource, but supports new structure `D3D12_RESOURCE_DESC1`.
    
    It internally uses `ID3D12Device8::CreateCommittedResource2` or `ID3D12Device8::CreatePlacedResource1`.

    To work correctly, `ID3D12Device8` interface must be available in the current system. Otherwise, `E_NOINTERFACE` is returned.
    */
    HRESULT CreateResource2(
        const ALLOCATION_DESC* pAllocDesc,
        const D3D12_RESOURCE_DESC1* pResourceDesc,
        D3D12_RESOURCE_STATES InitialResourceState,
        const D3D12_CLEAR_VALUE *pOptimizedClearValue,
        Allocation** ppAllocation,
        REFIID riidResource,
        void** ppvResource);
#endif // #ifdef __ID3D12Device4_INTERFACE_DEFINED__

    /** \brief Allocates memory without creating any resource placed in it.

    This function is similar to `ID3D12Device::CreateHeap`, but it may really assign
    part of a larger, existing heap to the allocation.

    `pAllocDesc->heapFlags` should contain one of these values, depending on type of resources you are going to create in this memory:
    `D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS`,
    `D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES`,
    `D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES`.
    Except if you validate that ResourceHeapTier = 2 - then `heapFlags`
    may be `D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES` = 0.
    Additional flags in `heapFlags` are allowed as well.

    `pAllocInfo->SizeInBytes` must be multiply of 64KB.
    `pAllocInfo->Alignment` must be one of the legal values as described in documentation of `D3D12_HEAP_DESC`.

    If you use D3D12MA::ALLOCATION_FLAG_COMMITTED you will get a separate memory block -
    a heap that always has offset 0.
    */
    HRESULT AllocateMemory(
        const ALLOCATION_DESC* pAllocDesc,
        const D3D12_RESOURCE_ALLOCATION_INFO* pAllocInfo,
        Allocation** ppAllocation);

    /** \brief Creates a new resource in place of an existing allocation. This is useful for memory aliasing.

    \param pAllocation Existing allocation indicating the memory where the new resource should be created.
        It can be created using D3D12MA::Allocator::CreateResource and already have a resource bound to it,
        or can be a raw memory allocated with D3D12MA::Allocator::AllocateMemory.
        It must not be created as committed so that `ID3D12Heap` is available and not implicit.
    \param AllocationLocalOffset Additional offset in bytes to be applied when allocating the resource.
        Local from the start of `pAllocation`, not the beginning of the whole `ID3D12Heap`!
        If the new resource should start from the beginning of the `pAllocation` it should be 0.
    \param pResourceDesc Description of the new resource to be created.
    \param InitialResourceState
    \param pOptimizedClearValue
    \param riidResource
    \param[out] ppvResource Returns pointer to the new resource.
        The resource is not bound with `pAllocation`.
        This pointer must not be null - you must get the resource pointer and `Release` it when no longer needed.

    Memory requirements of the new resource are checked for validation.
    If its size exceeds the end of `pAllocation` or required alignment is not fulfilled
    considering `pAllocation->GetOffset() + AllocationLocalOffset`, the function
    returns `E_INVALIDARG`.
    */
    HRESULT CreateAliasingResource(
        Allocation* pAllocation,
        UINT64 AllocationLocalOffset,
        const D3D12_RESOURCE_DESC* pResourceDesc,
        D3D12_RESOURCE_STATES InitialResourceState,
        const D3D12_CLEAR_VALUE *pOptimizedClearValue,
        REFIID riidResource,
        void** ppvResource);

    /** \brief Creates custom pool.
    */
    HRESULT CreatePool(
        const POOL_DESC* pPoolDesc,
        Pool** ppPool);

    /** \brief Sets the index of the current frame.

    This function is used to set the frame index in the allocator when a new game frame begins.
    */
    void SetCurrentFrameIndex(UINT frameIndex);

    /** \brief Retrieves information about current memory usage and budget.

    \param[out] pLocalBudget Optional, can be null.
    \param[out] pNonLocalBudget Optional, can be null.

    - When IsUMA() `== FALSE` (discrete graphics card):
      - `pLocalBudget` returns the budget of the video memory.
      - `pNonLocalBudget` returns the budget of the system memory available for D3D12 resources.
    - When IsUMA() `== TRUE` (integrated graphics chip):
      - `pLocalBudget` returns the budget of the shared memory available for all D3D12 resources.
        All memory is considered "local".
      - `pNonLocalBudget` is not applicable and returns zeros.

    This function is called "get" not "calculate" because it is very fast, suitable to be called
    every frame or every allocation. For more detailed statistics use CalculateStatistics().

    Note that when using allocator from multiple threads, returned information may immediately
    become outdated.
    */
    void GetBudget(Budget* pLocalBudget, Budget* pNonLocalBudget);

    /** \brief Retrieves statistics from current state of the allocator.

    This function is called "calculate" not "get" because it has to traverse all
    internal data structures, so it may be quite slow. Use it for debugging purposes.
    For faster but more brief statistics suitable to be called every frame or every allocation,
    use GetBudget().

    Note that when using allocator from multiple threads, returned information may immediately
    become outdated.
    */
    void CalculateStatistics(TotalStatistics* pStats);

    /** \brief Builds and returns statistics as a string in JSON format.
    * 
    @param[out] ppStatsString Must be freed using Allocator::FreeStatsString.
    @param DetailedMap `TRUE` to include full list of allocations (can make the string quite long), `FALSE` to only return statistics.
    */
    void BuildStatsString(WCHAR** ppStatsString, BOOL DetailedMap) const;

    /// Frees memory of a string returned from Allocator::BuildStatsString.
    void FreeStatsString(WCHAR* pStatsString) const;

    /** \brief Begins defragmentation process of the default pools.

    \param pDesc Structure filled with parameters of defragmentation.
    \param[out] ppContext Context object that will manage defragmentation.

    For more information about defragmentation, see documentation chapter:
    [Defragmentation](@ref defragmentation).
    */
    void BeginDefragmentation(const DEFRAGMENTATION_DESC* pDesc, DefragmentationContext** ppContext);

protected:
    void ReleaseThis() override;

private:
    friend D3D12MA_API HRESULT CreateAllocator(const ALLOCATOR_DESC*, Allocator**);
    template<typename T> friend void D3D12MA_DELETE(const ALLOCATION_CALLBACKS&, T*);
    friend class DefragmentationContext;
    friend class Pool;

    Allocator(const ALLOCATION_CALLBACKS& allocationCallbacks, const ALLOCATOR_DESC& desc);
    ~Allocator();
    
    AllocatorPimpl* m_Pimpl;
    
    D3D12MA_CLASS_NO_COPY(Allocator)
};


/// \brief Bit flags to be used with VIRTUAL_BLOCK_DESC::Flags.
enum VIRTUAL_BLOCK_FLAGS
{
    /// Zero
    VIRTUAL_BLOCK_FLAG_NONE = 0,

    /** \brief Enables alternative, linear allocation algorithm in this virtual block.

    Specify this flag to enable linear allocation algorithm, which always creates
    new allocations after last one and doesn't reuse space from allocations freed in
    between. It trades memory consumption for simplified algorithm and data
    structure, which has better performance and uses less memory for metadata.

    By using this flag, you can achieve behavior of free-at-once, stack,
    ring buffer, and double stack.
    For details, see documentation chapter \ref linear_algorithm.
    */
    VIRTUAL_BLOCK_FLAG_ALGORITHM_LINEAR = POOL_FLAG_ALGORITHM_LINEAR,

    // Bit mask to extract only `ALGORITHM` bits from entire set of flags.
    VIRTUAL_BLOCK_FLAG_ALGORITHM_MASK = POOL_FLAG_ALGORITHM_MASK
};

/// Parameters of created D3D12MA::VirtualBlock object to be passed to CreateVirtualBlock().
struct VIRTUAL_BLOCK_DESC
{
    /// Flags.
    VIRTUAL_BLOCK_FLAGS Flags;
    /** \brief Total size of the block.

    Sizes can be expressed in bytes or any units you want as long as you are consistent in using them.
    For example, if you allocate from some array of structures, 1 can mean single instance of entire structure.
    */
    UINT64 Size;
    /** \brief Custom CPU memory allocation callbacks. Optional.

    Optional, can be null. When specified, will be used for all CPU-side memory allocations.
    */
    const ALLOCATION_CALLBACKS* pAllocationCallbacks;
};

/// \brief Bit flags to be used with VIRTUAL_ALLOCATION_DESC::Flags.
enum VIRTUAL_ALLOCATION_FLAGS
{
    /// Zero
    VIRTUAL_ALLOCATION_FLAG_NONE = 0,

    /** \brief Allocation will be created from upper stack in a double stack pool.

    This flag is only allowed for virtual blocks created with #VIRTUAL_BLOCK_FLAG_ALGORITHM_LINEAR flag.
    */
    VIRTUAL_ALLOCATION_FLAG_UPPER_ADDRESS = ALLOCATION_FLAG_UPPER_ADDRESS,

    /// Allocation strategy that tries to minimize memory usage.
    VIRTUAL_ALLOCATION_FLAG_STRATEGY_MIN_MEMORY = ALLOCATION_FLAG_STRATEGY_MIN_MEMORY,
    /// Allocation strategy that tries to minimize allocation time.
    VIRTUAL_ALLOCATION_FLAG_STRATEGY_MIN_TIME = ALLOCATION_FLAG_STRATEGY_MIN_TIME,
    /** \brief Allocation strategy that chooses always the lowest offset in available space.
    This is not the most efficient strategy but achieves highly packed data.
    */
    VIRTUAL_ALLOCATION_FLAG_STRATEGY_MIN_OFFSET = ALLOCATION_FLAG_STRATEGY_MIN_OFFSET,
    /** \brief A bit mask to extract only `STRATEGY` bits from entire set of flags.

    These strategy flags are binary compatible with equivalent flags in #ALLOCATION_FLAGS.
    */
    VIRTUAL_ALLOCATION_FLAG_STRATEGY_MASK = ALLOCATION_FLAG_STRATEGY_MASK,
};

/// Parameters of created virtual allocation to be passed to VirtualBlock::Allocate().
struct VIRTUAL_ALLOCATION_DESC
{
    /// Flags.
    VIRTUAL_ALLOCATION_FLAGS Flags;
    /** \brief Size of the allocation.
    
    Cannot be zero.
    */
    UINT64 Size;
    /** \brief Required alignment of the allocation.
    
    Must be power of two. Special value 0 has the same meaning as 1 - means no special alignment is required, so allocation can start at any offset.
    */
    UINT64 Alignment;
    /** \brief Custom pointer to be associated with the allocation.

    It can be fetched or changed later.
    */
    void* pPrivateData;
};

/// Parameters of an existing virtual allocation, returned by VirtualBlock::GetAllocationInfo().
struct VIRTUAL_ALLOCATION_INFO
{
    /// \brief Offset of the allocation.
    UINT64 Offset;
    /** \brief Size of the allocation.

    Same value as passed in VIRTUAL_ALLOCATION_DESC::Size.
    */
    UINT64 Size;
    /** \brief Custom pointer associated with the allocation.

    Same value as passed in VIRTUAL_ALLOCATION_DESC::pPrivateData or VirtualBlock::SetAllocationPrivateData().
    */
    void* pPrivateData;
};

/** \brief Represents pure allocation algorithm and a data structure with allocations in some memory block, without actually allocating any GPU memory.

This class allows to use the core algorithm of the library custom allocations e.g. CPU memory or
sub-allocation regions inside a single GPU buffer.

To create this object, fill in D3D12MA::VIRTUAL_BLOCK_DESC and call CreateVirtualBlock().
To destroy it, call its method `VirtualBlock::Release()`.
You need to free all the allocations within this block or call Clear() before destroying it.

This object is not thread-safe - should not be used from multiple threads simultaneously, must be synchronized externally.
*/
class D3D12MA_API VirtualBlock : public IUnknownImpl
{
public:
    /** \brief Returns true if the block is empty - contains 0 allocations.
    */
    BOOL IsEmpty() const;
    /** \brief Returns information about an allocation - its offset, size and custom pointer.
    */
    void GetAllocationInfo(VirtualAllocation allocation, VIRTUAL_ALLOCATION_INFO* pInfo) const;

    /** \brief Creates new allocation.
    \param pDesc
    \param[out] pAllocation Unique indentifier of the new allocation within single block.
    \param[out] pOffset Returned offset of the new allocation. Optional, can be null.
    \return `S_OK` if allocation succeeded, `E_OUTOFMEMORY` if it failed.

    If the allocation failed, `pAllocation->AllocHandle` is set to 0 and `pOffset`, if not null, is set to `UINT64_MAX`.
    */
    HRESULT Allocate(const VIRTUAL_ALLOCATION_DESC* pDesc, VirtualAllocation* pAllocation, UINT64* pOffset);
    /** \brief Frees the allocation.
    
    Calling this function with `allocation.AllocHandle == 0` is correct and does nothing.
    */
    void FreeAllocation(VirtualAllocation allocation);
    /** \brief Frees all the allocations.
    */
    void Clear();
    /** \brief Changes custom pointer for an allocation to a new value.
    */
    void SetAllocationPrivateData(VirtualAllocation allocation, void* pPrivateData);
    /** \brief Retrieves basic statistics of the virtual block that are fast to calculate.

    \param[out] pStats %Statistics of the virtual block.
    */
    void GetStatistics(Statistics* pStats) const;
    /** \brief Retrieves detailed statistics of the virtual block that are slower to calculate.

    \param[out] pStats %Statistics of the virtual block.
    */
    void CalculateStatistics(DetailedStatistics* pStats) const;

    /** \brief Builds and returns statistics as a string in JSON format, including the list of allocations with their parameters.
    @param[out] ppStatsString Must be freed using VirtualBlock::FreeStatsString.
    */
    void BuildStatsString(WCHAR** ppStatsString) const;

    /** \brief Frees memory of a string returned from VirtualBlock::BuildStatsString.
    */
    void FreeStatsString(WCHAR* pStatsString) const;
   
protected:
    void ReleaseThis() override;

private:
    friend D3D12MA_API HRESULT CreateVirtualBlock(const VIRTUAL_BLOCK_DESC*, VirtualBlock**);
    template<typename T> friend void D3D12MA_DELETE(const ALLOCATION_CALLBACKS&, T*);

    VirtualBlockPimpl* m_Pimpl;

    VirtualBlock(const ALLOCATION_CALLBACKS& allocationCallbacks, const VIRTUAL_BLOCK_DESC& desc);
    ~VirtualBlock();

    D3D12MA_CLASS_NO_COPY(VirtualBlock)
};


/** \brief Creates new main D3D12MA::Allocator object and returns it through `ppAllocator`.

You normally only need to call it once and keep a single Allocator object for your `ID3D12Device`.
*/
D3D12MA_API HRESULT CreateAllocator(const ALLOCATOR_DESC* pDesc, Allocator** ppAllocator);

/** \brief Creates new D3D12MA::VirtualBlock object and returns it through `ppVirtualBlock`.

Note you don't need to create D3D12MA::Allocator to use virtual blocks.
*/
D3D12MA_API HRESULT CreateVirtualBlock(const VIRTUAL_BLOCK_DESC* pDesc, VirtualBlock** ppVirtualBlock);

} // namespace D3D12MA

/// \cond INTERNAL
DEFINE_ENUM_FLAG_OPERATORS(D3D12MA::ALLOCATION_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(D3D12MA::DEFRAGMENTATION_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(D3D12MA::ALLOCATOR_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(D3D12MA::POOL_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(D3D12MA::VIRTUAL_BLOCK_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(D3D12MA::VIRTUAL_ALLOCATION_FLAGS);
/// \endcond

#ifdef D3D12MA_IMPLEMENTATION
//
// Copyright (c) 2019-2022 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//


#ifndef D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
    #include <dxgi.h>
    #if D3D12MA_DXGI_1_4
        #include <dxgi1_4.h>
    #endif
#endif

#include "../../../../Utilities/Threading/Atomics.h"
#include <algorithm>

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// Configuration Begin
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


#ifndef D3D12MA_SORT
  #define D3D12MA_SORT(beg, end, cmp)  std::sort(beg, end, cmp)
#endif

#ifndef D3D12MA_ASSERT
  #include "../../../../Utilities/Interfaces/ILog.h"
  #define D3D12MA_ASSERT(cond) ASSERT(cond)
#endif

// Assert that will be called very often, like inside data structures e.g. operator[].
// Making it non-empty can make program slow.
#ifndef D3D12MA_HEAVY_ASSERT
  #ifdef _DEBUG
    #define D3D12MA_HEAVY_ASSERT(expr)   //D3D12MA_ASSERT(expr)
  #else
    #define D3D12MA_HEAVY_ASSERT(expr)
  #endif
#endif

#ifndef D3D12MA_DEBUG_ALIGNMENT
  /*
  Minimum alignment of all allocations, in bytes.
  Set to more than 1 for debugging purposes only. Must be power of two.
  */
  #define D3D12MA_DEBUG_ALIGNMENT (1)
#endif

#ifndef D3D12MA_DEBUG_MARGIN
  // Minimum margin before and after every allocation, in bytes.
  // Set nonzero for debugging purposes only.
  #define D3D12MA_DEBUG_MARGIN (0)
#endif

#ifndef D3D12MA_DEBUG_GLOBAL_MUTEX
  /*
  Set this to 1 for debugging purposes only, to enable single mutex protecting all
  entry calls to the library. Can be useful for debugging multithreading issues.
  */
  #define D3D12MA_DEBUG_GLOBAL_MUTEX (0)
#endif

/*
Define this macro for debugging purposes only to force specific D3D12_RESOURCE_HEAP_TIER,
especially to test compatibility with D3D12_RESOURCE_HEAP_TIER_1 on modern GPUs.
*/
//#define D3D12MA_FORCE_RESOURCE_HEAP_TIER D3D12_RESOURCE_HEAP_TIER_1

#ifndef D3D12MA_DEFAULT_BLOCK_SIZE
  /// Default size of a block allocated as single ID3D12Heap.
  #define D3D12MA_DEFAULT_BLOCK_SIZE (64ull * 1024 * 1024)
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// Configuration End
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define D3D12MA_IID_PPV_ARGS(ppType)   __uuidof(**(ppType)), reinterpret_cast<void**>(ppType)

namespace D3D12MA
{
static constexpr UINT HEAP_TYPE_COUNT = 4;
static constexpr UINT STANDARD_HEAP_TYPE_COUNT = 3; // Only DEFAULT, UPLOAD, READBACK.
static constexpr UINT DEFAULT_POOL_MAX_COUNT = 9;
static const UINT NEW_BLOCK_SIZE_SHIFT_MAX = 3;
// Minimum size of a free suballocation to register it in the free suballocation collection.
static const UINT64 MIN_FREE_SUBALLOCATION_SIZE_TO_REGISTER = 16;

static const WCHAR* const HeapTypeNames[] =
{
    L"DEFAULT",
    L"UPLOAD",
    L"READBACK",
    L"CUSTOM",
};

static const D3D12_HEAP_FLAGS RESOURCE_CLASS_HEAP_FLAGS =
    D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;

#ifndef _D3D12MA_ENUM_DECLARATIONS

// Local copy of this enum, as it is provided only by <dxgi1_4.h>, so it may not be available.
enum DXGI_MEMORY_SEGMENT_GROUP_COPY
{
    DXGI_MEMORY_SEGMENT_GROUP_LOCAL_COPY = 0,
    DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL_COPY = 1,
    DXGI_MEMORY_SEGMENT_GROUP_COUNT
};

enum class ResourceClass
{
    Unknown, Buffer, Non_RT_DS_Texture, RT_DS_Texture
};

enum SuballocationType
{
    SUBALLOCATION_TYPE_FREE = 0,
    SUBALLOCATION_TYPE_ALLOCATION = 1,
};

#endif // _D3D12MA_ENUM_DECLARATIONS


#ifndef _D3D12MA_FUNCTIONS

static void* DefaultAllocate(size_t Size, size_t Alignment, void* /*pPrivateData*/)
{
#ifdef _WIN32
    return _aligned_malloc(Size, Alignment);
#else
    return aligned_alloc(Alignment, Size);
#endif
}
static void DefaultFree(void* pMemory, void* /*pPrivateData*/)
{
#ifdef _WIN32
    return _aligned_free(pMemory);
#else
    return free(pMemory);
#endif
}

static void* Malloc(const ALLOCATION_CALLBACKS& allocs, size_t size, size_t alignment)
{
    void* const result = (*allocs.pAllocate)(size, alignment, allocs.pPrivateData);
    D3D12MA_ASSERT(result);
    return result;
}
static void Free(const ALLOCATION_CALLBACKS& allocs, void* memory)
{
    (*allocs.pFree)(memory, allocs.pPrivateData);
}

template<typename T>
static T* Allocate(const ALLOCATION_CALLBACKS& allocs)
{
    return (T*)Malloc(allocs, sizeof(T), __alignof(T));
}
template<typename T>
static T* AllocateArray(const ALLOCATION_CALLBACKS& allocs, size_t count)
{
    return (T*)Malloc(allocs, sizeof(T) * count, __alignof(T));
}

#define D3D12MA_NEW(allocs, type) new(D3D12MA::Allocate<type>(allocs))(type)
#define D3D12MA_NEW_ARRAY(allocs, type, count) new(D3D12MA::AllocateArray<type>((allocs), (count)))(type)

template<typename T>
void D3D12MA_DELETE(const ALLOCATION_CALLBACKS& allocs, T* memory)
{
    if (memory)
    {
        memory->~T();
        Free(allocs, memory);
    }
}
template<typename T>
void D3D12MA_DELETE_ARRAY(const ALLOCATION_CALLBACKS& allocs, T* memory, size_t count)
{
    if (memory)
    {
        for (size_t i = count; i--; )
        {
            memory[i].~T();
        }
        Free(allocs, memory);
    }
}

static void SetupAllocationCallbacks(ALLOCATION_CALLBACKS& outAllocs, const ALLOCATION_CALLBACKS* allocationCallbacks)
{
    if (allocationCallbacks)
    {
        outAllocs = *allocationCallbacks;
        D3D12MA_ASSERT(outAllocs.pAllocate != NULL && outAllocs.pFree != NULL);
    }
    else
    {
        outAllocs.pAllocate = &DefaultAllocate;
        outAllocs.pFree = &DefaultFree;
        outAllocs.pPrivateData = NULL;
    }
}

#define SAFE_RELEASE(ptr)   do { if(ptr) { (ptr)->Release(); (ptr) = NULL; } } while(false)

#define D3D12MA_VALIDATE(cond) do { if(!(cond)) { \
    D3D12MA_ASSERT(0 && "Validation failed: " #cond); \
    return false; \
} } while(false)

template<typename T>
static T D3D12MA_MIN(const T& a, const T& b) { return a <= b ? a : b; }
template<typename T>
static T D3D12MA_MAX(const T& a, const T& b) { return a <= b ? b : a; }

template<typename T>
static void D3D12MA_SWAP(T& a, T& b) { T tmp = a; a = b; b = tmp; }

// Scans integer for index of first nonzero bit from the Least Significant Bit (LSB). If mask is 0 then returns UINT8_MAX
UINT8 BitScanLSB(UINT64 mask)
{
#if defined(_MSC_VER) && defined(_WIN64)
    unsigned long pos;
    if (_BitScanForward64(&pos, mask))
        return static_cast<UINT8>(pos);
    return UINT8_MAX;
#elif defined __GNUC__ || defined __clang__
    return static_cast<UINT8>(__builtin_ffsll(mask)) - 1U;
#else
    UINT8 pos = 0;
    UINT64 bit = 1;
    do
    {
        if (mask & bit)
            return pos;
        bit <<= 1;
    } while (pos++ < 63);
    return UINT8_MAX;
#endif
}
// Scans integer for index of first nonzero bit from the Least Significant Bit (LSB). If mask is 0 then returns UINT8_MAX
UINT8 BitScanLSB(UINT32 mask)
{
#ifdef _MSC_VER
    unsigned long pos;
    if (_BitScanForward(&pos, mask))
        return static_cast<UINT8>(pos);
    return UINT8_MAX;
#elif defined __GNUC__ || defined __clang__
    return static_cast<UINT8>(__builtin_ffs(mask)) - 1U;
#else
    UINT8 pos = 0;
    UINT32 bit = 1;
    do
    {
        if (mask & bit)
            return pos;
        bit <<= 1;
    } while (pos++ < 31);
    return UINT8_MAX;
#endif
}

// Scans integer for index of first nonzero bit from the Most Significant Bit (MSB). If mask is 0 then returns UINT8_MAX
UINT8 BitScanMSB(UINT64 mask)
{
#if defined(_MSC_VER) && defined(_WIN64)
    unsigned long pos;
    if (_BitScanReverse64(&pos, mask))
        return static_cast<UINT8>(pos);
#elif defined __GNUC__ || defined __clang__
    if (mask)
        return 63 - static_cast<UINT8>(__builtin_clzll(mask));
#else
    UINT8 pos = 63;
    UINT64 bit = 1ULL << 63;
    do
    {
        if (mask & bit)
            return pos;
        bit >>= 1;
    } while (pos-- > 0);
#endif
    return UINT8_MAX;
}
// Scans integer for index of first nonzero bit from the Most Significant Bit (MSB). If mask is 0 then returns UINT8_MAX
UINT8 BitScanMSB(UINT32 mask)
{
#ifdef _MSC_VER
    unsigned long pos;
    if (_BitScanReverse(&pos, mask))
        return static_cast<UINT8>(pos);
#elif defined __GNUC__ || defined __clang__
    if (mask)
        return 31 - static_cast<UINT8>(__builtin_clz(mask));
#else
    UINT8 pos = 31;
    UINT32 bit = 1UL << 31;
    do
    {
        if (mask & bit)
            return pos;
        bit >>= 1;
    } while (pos-- > 0);
#endif
    return UINT8_MAX;
}

/*
Returns true if given number is a power of two.
T must be unsigned integer number or signed integer but always nonnegative.
For 0 returns true.
*/
template <typename T>
static bool IsPow2(T x) { return (x & (x - 1)) == 0; }

// Aligns given value up to nearest multiply of align value. For example: AlignUp(11, 8) = 16.
// Use types like UINT, uint64_t as T.
template <typename T>
static T AlignUp(T val, T alignment)
{
    D3D12MA_HEAVY_ASSERT(IsPow2(alignment));
    return (val + alignment - 1) & ~(alignment - 1);
}
// Aligns given value down to nearest multiply of align value. For example: AlignUp(11, 8) = 8.
// Use types like UINT, uint64_t as T.
template <typename T>
static T AlignDown(T val, T alignment)
{
    D3D12MA_HEAVY_ASSERT(IsPow2(alignment));
    return val & ~(alignment - 1);
}

// Division with mathematical rounding to nearest number.
template <typename T>
static T RoundDiv(T x, T y) { return (x + (y / (T)2)) / y; }
template <typename T>
static T DivideRoundingUp(T x, T y) { return (x + y - 1) / y; }

static WCHAR HexDigitToChar(UINT8 digit)
{
    if(digit < 10)
        return L'0' + digit;
    else
        return L'A' + (digit - 10);
}

/*
Performs binary search and returns iterator to first element that is greater or
equal to `key`, according to comparison `cmp`.

Cmp should return true if first argument is less than second argument.

Returned value is the found element, if present in the collection or place where
new element with value (key) should be inserted.
*/
template <typename CmpLess, typename IterT, typename KeyT>
static IterT BinaryFindFirstNotLess(IterT beg, IterT end, const KeyT& key, const CmpLess& cmp)
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

/*
Performs binary search and returns iterator to an element that is equal to `key`,
according to comparison `cmp`.

Cmp should return true if first argument is less than second argument.

Returned value is the found element, if present in the collection or end if not
found.
*/
template<typename CmpLess, typename IterT, typename KeyT>
static IterT BinaryFindSorted(const IterT& beg, const IterT& end, const KeyT& value, const CmpLess& cmp)
{
    IterT it = BinaryFindFirstNotLess<CmpLess, IterT, KeyT>(beg, end, value, cmp);
    if (it == end ||
        (!cmp(*it, value) && !cmp(value, *it)))
    {
        return it;
    }
    return end;
}

static UINT HeapTypeToIndex(D3D12_HEAP_TYPE type)
{
    switch (type)
    {
    case D3D12_HEAP_TYPE_DEFAULT:  return 0;
    case D3D12_HEAP_TYPE_UPLOAD:   return 1;
    case D3D12_HEAP_TYPE_READBACK: return 2;
    case D3D12_HEAP_TYPE_CUSTOM:   return 3;
    default: D3D12MA_ASSERT(0); return UINT_MAX;
    }
}

static D3D12_HEAP_TYPE IndexToHeapType(UINT heapTypeIndex)
{
    D3D12MA_ASSERT(heapTypeIndex < 4);
    // D3D12_HEAP_TYPE_DEFAULT starts at 1.
    return (D3D12_HEAP_TYPE)(heapTypeIndex + 1);
}

static UINT64 HeapFlagsToAlignment(D3D12_HEAP_FLAGS flags, bool denyMsaaTextures)
{
    /*
    Documentation of D3D12_HEAP_DESC structure says:

    - D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT   defined as 64KB.
    - D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT   defined as 4MB. An
    application must decide whether the heap will contain multi-sample
    anti-aliasing (MSAA), in which case, the application must choose [this flag].

    https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/ns-d3d12-d3d12_heap_desc
    */

    if (denyMsaaTextures)
        return D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

    const D3D12_HEAP_FLAGS denyAllTexturesFlags =
        D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
    const bool canContainAnyTextures =
        (flags & denyAllTexturesFlags) != denyAllTexturesFlags;
    return canContainAnyTextures ?
        D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
}

static ResourceClass HeapFlagsToResourceClass(D3D12_HEAP_FLAGS heapFlags)
{
    const bool allowBuffers = (heapFlags & D3D12_HEAP_FLAG_DENY_BUFFERS) == 0;
    const bool allowRtDsTextures = (heapFlags & D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES) == 0;
    const bool allowNonRtDsTextures = (heapFlags & D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES) == 0;

    const uint8_t allowedGroupCount = (allowBuffers ? 1 : 0) + (allowRtDsTextures ? 1 : 0) + (allowNonRtDsTextures ? 1 : 0);
    if (allowedGroupCount != 1)
        return ResourceClass::Unknown;

    if (allowRtDsTextures)
        return ResourceClass::RT_DS_Texture;
    if (allowNonRtDsTextures)
        return ResourceClass::Non_RT_DS_Texture;
    return ResourceClass::Buffer;
}

static bool IsHeapTypeStandard(D3D12_HEAP_TYPE type)
{
    return type == D3D12_HEAP_TYPE_DEFAULT ||
        type == D3D12_HEAP_TYPE_UPLOAD ||
        type == D3D12_HEAP_TYPE_READBACK;
}

D3D12_HEAP_PROPERTIES StandardHeapTypeToHeapProperties(D3D12_HEAP_TYPE type)
{
    D3D12MA_ASSERT(IsHeapTypeStandard(type));
    D3D12_HEAP_PROPERTIES result = {};
    result.Type = type;
    return result;
}

static bool IsFormatCompressed(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return true;
    default:
        return false;
    }
}

// Only some formats are supported. For others it returns 0.
static UINT GetBitsPerPixel(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return 128;
    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
        return 96;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
        return 64;
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
        return 64;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return 64;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
        return 32;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
        return 32;
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
        return 32;
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
        return 32;
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return 32;
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
        return 16;
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
        return 16;
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM:
        return 8;
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
        return 4;
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
        return 8;
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
        return 8;
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        return 4;
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
        return 8;
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
        return 8;
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return 8;
    default:
        return 0;
    }
}
    
template<typename D3D12_RESOURCE_DESC_T>
static ResourceClass ResourceDescToResourceClass(const D3D12_RESOURCE_DESC_T& resDesc)
{
    if (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        return ResourceClass::Buffer;
    // Else: it's surely a texture.
    const bool isRenderTargetOrDepthStencil =
        (resDesc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) != 0;
    return isRenderTargetOrDepthStencil ? ResourceClass::RT_DS_Texture : ResourceClass::Non_RT_DS_Texture;
}
    
// This algorithm is overly conservative.
template<typename D3D12_RESOURCE_DESC_T>
static bool CanUseSmallAlignment(const D3D12_RESOURCE_DESC_T& resourceDesc)
{
    if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
        return false;
    if ((resourceDesc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) != 0)
        return false;
    if (resourceDesc.SampleDesc.Count > 1)
        return false;
    if (resourceDesc.DepthOrArraySize != 1)
        return false;

    UINT sizeX = (UINT)resourceDesc.Width;
    UINT sizeY = resourceDesc.Height;
    UINT bitsPerPixel = GetBitsPerPixel(resourceDesc.Format);
    if (bitsPerPixel == 0)
        return false;

    if (IsFormatCompressed(resourceDesc.Format))
    {
        sizeX = DivideRoundingUp(sizeX, 4u);
        sizeY = DivideRoundingUp(sizeY, 4u);
        bitsPerPixel *= 16;
    }

    UINT tileSizeX = 0, tileSizeY = 0;
    switch (bitsPerPixel)
    {
    case   8: tileSizeX = 64; tileSizeY = 64; break;
    case  16: tileSizeX = 64; tileSizeY = 32; break;
    case  32: tileSizeX = 32; tileSizeY = 32; break;
    case  64: tileSizeX = 32; tileSizeY = 16; break;
    case 128: tileSizeX = 16; tileSizeY = 16; break;
    default: return false;
    }

    const UINT tileCount = DivideRoundingUp(sizeX, tileSizeX) * DivideRoundingUp(sizeY, tileSizeY);
    return tileCount <= 16;
}
    
static bool ValidateAllocateMemoryParameters(
    const ALLOCATION_DESC* pAllocDesc,
    const D3D12_RESOURCE_ALLOCATION_INFO* pAllocInfo,
    Allocation** ppAllocation)
{
    return pAllocDesc &&
        pAllocInfo &&
        ppAllocation &&
        (pAllocInfo->Alignment == 0 ||
            pAllocInfo->Alignment == D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT ||
            pAllocInfo->Alignment == D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT) &&
        pAllocInfo->SizeInBytes != 0 &&
        pAllocInfo->SizeInBytes % (64ull * 1024) == 0;
}

#endif // _D3D12MA_FUNCTIONS
    
#ifndef _D3D12MA_STATISTICS_FUNCTIONS

static void ClearStatistics(Statistics& outStats)
{
    outStats.BlockCount = 0;
    outStats.AllocationCount = 0;
    outStats.BlockBytes = 0;
    outStats.AllocationBytes = 0;
}

static void ClearDetailedStatistics(DetailedStatistics& outStats)
{
    ClearStatistics(outStats.Stats);
    outStats.UnusedRangeCount = 0;
    outStats.AllocationSizeMin = UINT64_MAX;
    outStats.AllocationSizeMax = 0;
    outStats.UnusedRangeSizeMin = UINT64_MAX;
    outStats.UnusedRangeSizeMax = 0;
}

static void AddStatistics(Statistics& inoutStats, const Statistics& src)
{
    inoutStats.BlockCount += src.BlockCount;
    inoutStats.AllocationCount += src.AllocationCount;
    inoutStats.BlockBytes += src.BlockBytes;
    inoutStats.AllocationBytes += src.AllocationBytes;
}

static void AddDetailedStatistics(DetailedStatistics& inoutStats, const DetailedStatistics& src)
{
    AddStatistics(inoutStats.Stats, src.Stats);
    inoutStats.UnusedRangeCount += src.UnusedRangeCount;
    inoutStats.AllocationSizeMin = D3D12MA_MIN(inoutStats.AllocationSizeMin, src.AllocationSizeMin);
    inoutStats.AllocationSizeMax = D3D12MA_MAX(inoutStats.AllocationSizeMax, src.AllocationSizeMax);
    inoutStats.UnusedRangeSizeMin = D3D12MA_MIN(inoutStats.UnusedRangeSizeMin, src.UnusedRangeSizeMin);
    inoutStats.UnusedRangeSizeMax = D3D12MA_MAX(inoutStats.UnusedRangeSizeMax, src.UnusedRangeSizeMax);
}

static void AddDetailedStatisticsAllocation(DetailedStatistics& inoutStats, UINT64 size)
{
    inoutStats.Stats.AllocationCount++;
    inoutStats.Stats.AllocationBytes += size;
    inoutStats.AllocationSizeMin = D3D12MA_MIN(inoutStats.AllocationSizeMin, size);
    inoutStats.AllocationSizeMax = D3D12MA_MAX(inoutStats.AllocationSizeMax, size);
}

static void AddDetailedStatisticsUnusedRange(DetailedStatistics& inoutStats, UINT64 size)
{
    inoutStats.UnusedRangeCount++;
    inoutStats.UnusedRangeSizeMin = D3D12MA_MIN(inoutStats.UnusedRangeSizeMin, size);
    inoutStats.UnusedRangeSizeMax = D3D12MA_MAX(inoutStats.UnusedRangeSizeMax, size);
}
    
#endif // _D3D12MA_STATISTICS_FUNCTIONS


#ifndef _D3D12MA_MUTEX

#ifndef D3D12MA_MUTEX
    class D3D12MAMutex
    {
    public:
      D3D12MAMutex() { initMutex(&m_Mutex); }
      ~D3D12MAMutex() { destroyMutex(&m_Mutex); }
      void Lock() { acquireMutex(&m_Mutex); }
      void Unlock() { releaseMutex(&m_Mutex); }
    private:
      Mutex m_Mutex;
    };
    #define D3D12MA_MUTEX D3D12MAMutex
#endif

#ifndef D3D12MA_RW_MUTEX
#ifdef _WIN32
    class RWMutex
    {
    public:
        RWMutex() { InitializeSRWLock(&m_Lock); }
        void LockRead() { AcquireSRWLockShared(&m_Lock); }
        void UnlockRead() { ReleaseSRWLockShared(&m_Lock); }
        void LockWrite() { AcquireSRWLockExclusive(&m_Lock); }
        void UnlockWrite() { ReleaseSRWLockExclusive(&m_Lock); }
    
    private:
        SRWLOCK m_Lock;
    };
#else // #ifdef _WIN32
    class RWMutex
    {
    public:
        RWMutex() {}
        void LockRead() { m_Mutex.lock_shared(); }
        void UnlockRead() { m_Mutex.unlock_shared(); }
        void LockWrite() { m_Mutex.lock(); }
        void UnlockWrite() { m_Mutex.unlock(); }
    
    private:
        std::shared_timed_mutex m_Mutex;
    };
#endif // #ifdef _WIN32
    #define D3D12MA_RW_MUTEX RWMutex
#endif // #ifndef D3D12MA_RW_MUTEX

// Helper RAII class to lock a mutex in constructor and unlock it in destructor (at the end of scope).
struct MutexLock
{
    D3D12MA_CLASS_NO_COPY(MutexLock);
public:
    MutexLock(D3D12MA_MUTEX& mutex, bool useMutex = true) :
        m_pMutex(useMutex ? &mutex : NULL)
    {
        if (m_pMutex) m_pMutex->Lock(); 
    }
    ~MutexLock() { if (m_pMutex) m_pMutex->Unlock(); }

private:
    D3D12MA_MUTEX* m_pMutex;
};

// Helper RAII class to lock a RW mutex in constructor and unlock it in destructor (at the end of scope), for reading.
struct MutexLockRead
{
    D3D12MA_CLASS_NO_COPY(MutexLockRead);
public:
    MutexLockRead(D3D12MA_RW_MUTEX& mutex, bool useMutex)
        :  m_pMutex(useMutex ? &mutex : NULL)
    {
        if(m_pMutex)
        {
            m_pMutex->LockRead();
        }
    }
    ~MutexLockRead() { if (m_pMutex) m_pMutex->UnlockRead(); }

private:
    D3D12MA_RW_MUTEX* m_pMutex;
};

// Helper RAII class to lock a RW mutex in constructor and unlock it in destructor (at the end of scope), for writing.
struct MutexLockWrite
{
    D3D12MA_CLASS_NO_COPY(MutexLockWrite);
public:
    MutexLockWrite(D3D12MA_RW_MUTEX& mutex, bool useMutex)
        : m_pMutex(useMutex ? &mutex : NULL)
    {
        if (m_pMutex) m_pMutex->LockWrite(); 
    }
    ~MutexLockWrite() { if (m_pMutex) m_pMutex->UnlockWrite(); }

private:
    D3D12MA_RW_MUTEX* m_pMutex;
};

#if D3D12MA_DEBUG_GLOBAL_MUTEX
    static D3D12MA_MUTEX g_DebugGlobalMutex;
    #define D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK MutexLock debugGlobalMutexLock(g_DebugGlobalMutex, true);
#else
    #define D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
#endif
#endif // _D3D12MA_MUTEX

#ifndef _D3D12MA_VECTOR
/*
Dynamically resizing continuous array. Class with interface similar to std::vector.
T must be POD because constructors and destructors are not called and memcpy is
used for these objects.
*/
template<typename T>
class Vector
{
public:
    using value_type = T;
    using iterator = T*;

    // allocationCallbacks externally owned, must outlive this object.
    Vector(const ALLOCATION_CALLBACKS& allocationCallbacks);
    Vector(size_t count, const ALLOCATION_CALLBACKS& allocationCallbacks);
    Vector(const Vector<T>& src);
    ~Vector();

    const ALLOCATION_CALLBACKS& GetAllocs() const { return m_AllocationCallbacks; }
    bool empty() const { return m_Count == 0; }
    size_t size() const { return m_Count; }
    T* data() { return m_pArray; }
    const T* data() const { return m_pArray; }
    void clear(bool freeMemory = false) { resize(0, freeMemory); }

    iterator begin() { return m_pArray; }
    iterator end() { return m_pArray + m_Count; }
    iterator rend() { return begin() - 1; }
    iterator rbegin() { return end() - 1; }

    const iterator cbegin() const { return m_pArray; }
    const iterator cend() const { return m_pArray + m_Count; }
    const iterator crbegin() const { return cend() - 1; }
    const iterator crend() const { return cbegin() - 1; }

    void push_front(const T& src) { insert(0, src); }
    void push_back(const T& src);
    void pop_front();
    void pop_back();

    T& front();
    T& back();
    const T& front() const;
    const T& back() const;

    void reserve(size_t newCapacity, bool freeMemory = false);
    void resize(size_t newCount, bool freeMemory = false);
    void insert(size_t index, const T& src);
    void remove(size_t index);

    template<typename CmpLess>
    size_t InsertSorted(const T& value, const CmpLess& cmp);
    template<typename CmpLess>
    bool RemoveSorted(const T& value, const CmpLess& cmp);

    Vector& operator=(const Vector<T>& rhs);
    T& operator[](size_t index);
    const T& operator[](size_t index) const;

private:
    const ALLOCATION_CALLBACKS& m_AllocationCallbacks;
    T* m_pArray;
    size_t m_Count;
    size_t m_Capacity;
};

#ifndef _D3D12MA_VECTOR_FUNCTIONS
template<typename T>
Vector<T>::Vector(const ALLOCATION_CALLBACKS& allocationCallbacks)
    : m_AllocationCallbacks(allocationCallbacks),
    m_pArray(NULL),
    m_Count(0),
    m_Capacity(0) {}

template<typename T>
Vector<T>::Vector(size_t count, const ALLOCATION_CALLBACKS& allocationCallbacks)
    : m_AllocationCallbacks(allocationCallbacks),
    m_pArray(count ? AllocateArray<T>(allocationCallbacks, count) : NULL),
    m_Count(count),
    m_Capacity(count) {}

template<typename T>
Vector<T>::Vector(const Vector<T>& src)
    : m_AllocationCallbacks(src.m_AllocationCallbacks),
    m_pArray(src.m_Count ? AllocateArray<T>(src.m_AllocationCallbacks, src.m_Count) : NULL),
    m_Count(src.m_Count),
    m_Capacity(src.m_Count)
{
    if (m_Count > 0)
    {
        memcpy(m_pArray, src.m_pArray, m_Count * sizeof(T));
    }
}

template<typename T>
Vector<T>::~Vector()
{
    Free(m_AllocationCallbacks, m_pArray);
}

template<typename T>
void Vector<T>::push_back(const T& src)
{
    const size_t newIndex = size();
    resize(newIndex + 1);
    m_pArray[newIndex] = src;
}

template<typename T>
void Vector<T>::pop_front()
{
    D3D12MA_HEAVY_ASSERT(m_Count > 0);
    remove(0);
}

template<typename T>
void Vector<T>::pop_back()
{
    D3D12MA_HEAVY_ASSERT(m_Count > 0);
    resize(size() - 1);
}

template<typename T>
T& Vector<T>::front()
{
    D3D12MA_HEAVY_ASSERT(m_Count > 0);
    return m_pArray[0];
}

template<typename T>
T& Vector<T>::back()
{
    D3D12MA_HEAVY_ASSERT(m_Count > 0);
    return m_pArray[m_Count - 1];
}

template<typename T>
const T& Vector<T>::front() const
{
    D3D12MA_HEAVY_ASSERT(m_Count > 0);
    return m_pArray[0];
}

template<typename T>
const T& Vector<T>::back() const
{
    D3D12MA_HEAVY_ASSERT(m_Count > 0);
    return m_pArray[m_Count - 1];
}

template<typename T>
void Vector<T>::reserve(size_t newCapacity, bool freeMemory)
{
    newCapacity = D3D12MA_MAX(newCapacity, m_Count);

    if ((newCapacity < m_Capacity) && !freeMemory)
    {
        newCapacity = m_Capacity;
    }

    if (newCapacity != m_Capacity)
    {
        T* const newArray = newCapacity ? AllocateArray<T>(m_AllocationCallbacks, newCapacity) : NULL;
        if (m_Count != 0)
        {
            memcpy(newArray, m_pArray, m_Count * sizeof(T));
        }
        Free(m_AllocationCallbacks, m_pArray);
        m_Capacity = newCapacity;
        m_pArray = newArray;
    }
}

template<typename T>
void Vector<T>::resize(size_t newCount, bool freeMemory)
{
    size_t newCapacity = m_Capacity;
    if (newCount > m_Capacity)
    {
        newCapacity = D3D12MA_MAX(newCount, D3D12MA_MAX(m_Capacity * 3 / 2, (size_t)8));
    }
    else if (freeMemory)
    {
        newCapacity = newCount;
    }

    if (newCapacity != m_Capacity)
    {
        T* const newArray = newCapacity ? AllocateArray<T>(m_AllocationCallbacks, newCapacity) : NULL;
        const size_t elementsToCopy = D3D12MA_MIN(m_Count, newCount);
        if (elementsToCopy != 0)
        {
            memcpy(newArray, m_pArray, elementsToCopy * sizeof(T));
        }
        Free(m_AllocationCallbacks, m_pArray);
        m_Capacity = newCapacity;
        m_pArray = newArray;
    }

    m_Count = newCount;
}

template<typename T>
void Vector<T>::insert(size_t index, const T& src)
{
    D3D12MA_HEAVY_ASSERT(index <= m_Count);
    const size_t oldCount = size();
    resize(oldCount + 1);
    if (index < oldCount)
    {
        memmove(m_pArray + (index + 1), m_pArray + index, (oldCount - index) * sizeof(T));
    }
    m_pArray[index] = src;
}

template<typename T>
void Vector<T>::remove(size_t index)
{
    D3D12MA_HEAVY_ASSERT(index < m_Count);
    const size_t oldCount = size();
    if (index < oldCount - 1)
    {
        memmove(m_pArray + index, m_pArray + (index + 1), (oldCount - index - 1) * sizeof(T));
    }
    resize(oldCount - 1);
}

template<typename T> template<typename CmpLess>
size_t Vector<T>::InsertSorted(const T& value, const CmpLess& cmp)
{
    const size_t indexToInsert = BinaryFindFirstNotLess<CmpLess, iterator, T>(
        m_pArray,
        m_pArray + m_Count,
        value,
        cmp) - m_pArray;
    insert(indexToInsert, value);
    return indexToInsert;
}

template<typename T> template<typename CmpLess>
bool Vector<T>::RemoveSorted(const T& value, const CmpLess& cmp)
{
    const iterator it = BinaryFindFirstNotLess(
        m_pArray,
        m_pArray + m_Count,
        value,
        cmp);
    if ((it != end()) && !cmp(*it, value) && !cmp(value, *it))
    {
        size_t indexToRemove = it - begin();
        remove(indexToRemove);
        return true;
    }
    return false;
}

template<typename T>
Vector<T>& Vector<T>::operator=(const Vector<T>& rhs)
{
    if (&rhs != this)
    {
        resize(rhs.m_Count);
        if (m_Count != 0)
        {
            memcpy(m_pArray, rhs.m_pArray, m_Count * sizeof(T));
        }
    }
    return *this;
}

template<typename T>
T& Vector<T>::operator[](size_t index)
{
    D3D12MA_HEAVY_ASSERT(index < m_Count);
    return m_pArray[index];
}

template<typename T>
const T& Vector<T>::operator[](size_t index) const
{
    D3D12MA_HEAVY_ASSERT(index < m_Count);
    return m_pArray[index];
}
#endif // _D3D12MA_VECTOR_FUNCTIONS
#endif // _D3D12MA_VECTOR

#ifndef _D3D12MA_STRING_BUILDER
class StringBuilder
{
public:
    StringBuilder(const ALLOCATION_CALLBACKS& allocationCallbacks) : m_Data(allocationCallbacks) {}

    size_t GetLength() const { return m_Data.size(); }
    LPCWSTR GetData() const { return m_Data.data(); }

    void Add(WCHAR ch) { m_Data.push_back(ch); }
    void Add(LPCWSTR str);
    void AddNewLine() { Add(L'\n'); }
    void AddNumber(UINT num);
    void AddNumber(UINT64 num);
    void AddPointer(const void* ptr);

private:
    Vector<WCHAR> m_Data;
};

#ifndef _D3D12MA_STRING_BUILDER_FUNCTIONS
void StringBuilder::Add(LPCWSTR str)
{
    const size_t len = wcslen(str);
    if (len > 0)
    {
        const size_t oldCount = m_Data.size();
        m_Data.resize(oldCount + len);
        memcpy(m_Data.data() + oldCount, str, len * sizeof(WCHAR));
    }
}

void StringBuilder::AddNumber(UINT num)
{
    WCHAR buf[11];
    buf[10] = L'\0';
    WCHAR *p = &buf[10];
    do
    {
        *--p = L'0' + (num % 10);
        num /= 10;
    }
    while (num);
    Add(p);
}

void StringBuilder::AddNumber(UINT64 num)
{
    WCHAR buf[21];
    buf[20] = L'\0';
    WCHAR *p = &buf[20];
    do
    {
        *--p = L'0' + (num % 10);
        num /= 10;
    }
    while (num);
    Add(p);
}

void StringBuilder::AddPointer(const void* ptr)
{
    WCHAR buf[21];
    uintptr_t num = (uintptr_t)ptr;
    buf[20] = L'\0';
    WCHAR *p = &buf[20];
    do
    {
        *--p = HexDigitToChar((UINT8)(num & 0xF));
        num >>= 4;
    }
    while (num);
    Add(p);
}

#endif // _D3D12MA_STRING_BUILDER_FUNCTIONS
#endif // _D3D12MA_STRING_BUILDER

#ifndef _D3D12MA_JSON_WRITER
/*
Allows to conveniently build a correct JSON document to be written to the
StringBuilder passed to the constructor.
*/
class JsonWriter
{
public:
    // stringBuilder - string builder to write the document to. Must remain alive for the whole lifetime of this object.
    JsonWriter(const ALLOCATION_CALLBACKS& allocationCallbacks, StringBuilder& stringBuilder);
    ~JsonWriter();

    // Begins object by writing "{".
    // Inside an object, you must call pairs of WriteString and a value, e.g.:
    // j.BeginObject(true); j.WriteString("A"); j.WriteNumber(1); j.WriteString("B"); j.WriteNumber(2); j.EndObject();
    // Will write: { "A": 1, "B": 2 }
    void BeginObject(bool singleLine = false);
    // Ends object by writing "}".
    void EndObject();

    // Begins array by writing "[".
    // Inside an array, you can write a sequence of any values.
    void BeginArray(bool singleLine = false);
    // Ends array by writing "[".
    void EndArray();

    // Writes a string value inside "".
    // pStr can contain any UTF-16 characters, including '"', new line etc. - they will be properly escaped.
    void WriteString(LPCWSTR pStr);

    // Begins writing a string value.
    // Call BeginString, ContinueString, ContinueString, ..., EndString instead of
    // WriteString to conveniently build the string content incrementally, made of
    // parts including numbers.
    void BeginString(LPCWSTR pStr = NULL);
    // Posts next part of an open string.
    void ContinueString(LPCWSTR pStr);
    // Posts next part of an open string. The number is converted to decimal characters.
    void ContinueString(UINT num);
    void ContinueString(UINT64 num);
    void ContinueString_Pointer(const void* ptr);
    // Posts next part of an open string. Pointer value is converted to characters
    // using "%p" formatting - shown as hexadecimal number, e.g.: 000000081276Ad00
    // void ContinueString_Pointer(const void* ptr);
    // Ends writing a string value by writing '"'.
    void EndString(LPCWSTR pStr = NULL);

    // Writes a number value.
    void WriteNumber(UINT num);
    void WriteNumber(UINT64 num);
    // Writes a boolean value - false or true.
    void WriteBool(bool b);
    // Writes a null value.
    void WriteNull();

    void AddAllocationToObject(const Allocation& alloc);
    void AddDetailedStatisticsInfoObject(const DetailedStatistics& stats);

private:
    static const WCHAR* const INDENT;

    enum CollectionType
    {
        COLLECTION_TYPE_OBJECT,
        COLLECTION_TYPE_ARRAY,
    };
    struct StackItem
    {
        CollectionType type;
        UINT valueCount;
        bool singleLineMode;
    };

    StringBuilder& m_SB;
    Vector<StackItem> m_Stack;
    bool m_InsideString;

    void BeginValue(bool isString);
    void WriteIndent(bool oneLess = false);
};

#ifndef _D3D12MA_JSON_WRITER_FUNCTIONS
const WCHAR* const JsonWriter::INDENT = L"  ";

JsonWriter::JsonWriter(const ALLOCATION_CALLBACKS& allocationCallbacks, StringBuilder& stringBuilder)
    : m_SB(stringBuilder),
    m_Stack(allocationCallbacks),
    m_InsideString(false) {}

JsonWriter::~JsonWriter()
{
    D3D12MA_ASSERT(!m_InsideString);
    D3D12MA_ASSERT(m_Stack.empty());
}

void JsonWriter::BeginObject(bool singleLine)
{
    D3D12MA_ASSERT(!m_InsideString);

    BeginValue(false);
    m_SB.Add(L'{');

    StackItem stackItem;
    stackItem.type = COLLECTION_TYPE_OBJECT;
    stackItem.valueCount = 0;
    stackItem.singleLineMode = singleLine;
    m_Stack.push_back(stackItem);
}

void JsonWriter::EndObject()
{
    D3D12MA_ASSERT(!m_InsideString);
    D3D12MA_ASSERT(!m_Stack.empty() && m_Stack.back().type == COLLECTION_TYPE_OBJECT);
    D3D12MA_ASSERT(m_Stack.back().valueCount % 2 == 0);

    WriteIndent(true);
    m_SB.Add(L'}');

    m_Stack.pop_back();
}

void JsonWriter::BeginArray(bool singleLine)
{
    D3D12MA_ASSERT(!m_InsideString);

    BeginValue(false);
    m_SB.Add(L'[');

    StackItem stackItem;
    stackItem.type = COLLECTION_TYPE_ARRAY;
    stackItem.valueCount = 0;
    stackItem.singleLineMode = singleLine;
    m_Stack.push_back(stackItem);
}

void JsonWriter::EndArray()
{
    D3D12MA_ASSERT(!m_InsideString);
    D3D12MA_ASSERT(!m_Stack.empty() && m_Stack.back().type == COLLECTION_TYPE_ARRAY);

    WriteIndent(true);
    m_SB.Add(L']');

    m_Stack.pop_back();
}

void JsonWriter::WriteString(LPCWSTR pStr)
{
    BeginString(pStr);
    EndString();
}

void JsonWriter::BeginString(LPCWSTR pStr)
{
    D3D12MA_ASSERT(!m_InsideString);

    BeginValue(true);
    m_InsideString = true;
    m_SB.Add(L'"');
    if (pStr != NULL)
    {
        ContinueString(pStr);
    }
}

void JsonWriter::ContinueString(LPCWSTR pStr)
{
    D3D12MA_ASSERT(m_InsideString);
    D3D12MA_ASSERT(pStr);

    for (const WCHAR *p = pStr; *p; ++p)
    {
        // the strings we encode are assumed to be in UTF-16LE format, the native
        // windows wide character Unicode format. In this encoding Unicode code
        // points U+0000 to U+D7FF and U+E000 to U+FFFF are encoded in two bytes,
        // and everything else takes more than two bytes. We will reject any
        // multi wchar character encodings for simplicity.
        UINT val = (UINT)*p;
        D3D12MA_ASSERT(((val <= 0xD7FF) || (0xE000 <= val && val <= 0xFFFF)) &&
            "Character not currently supported.");
        switch (*p)
        {
        case L'"':  m_SB.Add(L'\\'); m_SB.Add(L'"');  break;
        case L'\\': m_SB.Add(L'\\'); m_SB.Add(L'\\'); break;
        case L'/':  m_SB.Add(L'\\'); m_SB.Add(L'/');  break;
        case L'\b': m_SB.Add(L'\\'); m_SB.Add(L'b');  break;
        case L'\f': m_SB.Add(L'\\'); m_SB.Add(L'f');  break;
        case L'\n': m_SB.Add(L'\\'); m_SB.Add(L'n');  break;
        case L'\r': m_SB.Add(L'\\'); m_SB.Add(L'r');  break;
        case L'\t': m_SB.Add(L'\\'); m_SB.Add(L't');  break;
        default:
            // conservatively use encoding \uXXXX for any Unicode character
            // requiring more than one byte.
            if (32 <= val && val < 256)
                m_SB.Add(*p);
            else
            {
                m_SB.Add(L'\\');
                m_SB.Add(L'u');
                for (UINT i = 0; i < 4; ++i)
                {
                    UINT hexDigit = (val & 0xF000) >> 12;
                    val <<= 4;
                    if (hexDigit < 10)
                        m_SB.Add(L'0' + (WCHAR)hexDigit);
                    else
                        m_SB.Add(L'A' + (WCHAR)hexDigit);
                }
            }
            break;
        }
    }
}

void JsonWriter::ContinueString(UINT num)
{
    D3D12MA_ASSERT(m_InsideString);
    m_SB.AddNumber(num);
}

void JsonWriter::ContinueString(UINT64 num)
{
    D3D12MA_ASSERT(m_InsideString);
    m_SB.AddNumber(num);
}

void JsonWriter::ContinueString_Pointer(const void* ptr)
{
    D3D12MA_ASSERT(m_InsideString);
    m_SB.AddPointer(ptr);
}

void JsonWriter::EndString(LPCWSTR pStr)
{
    D3D12MA_ASSERT(m_InsideString);

    if (pStr)
        ContinueString(pStr);
    m_SB.Add(L'"');
    m_InsideString = false;
}

void JsonWriter::WriteNumber(UINT num)
{
    D3D12MA_ASSERT(!m_InsideString);
    BeginValue(false);
    m_SB.AddNumber(num);
}

void JsonWriter::WriteNumber(UINT64 num)
{
    D3D12MA_ASSERT(!m_InsideString);
    BeginValue(false);
    m_SB.AddNumber(num);
}

void JsonWriter::WriteBool(bool b)
{
    D3D12MA_ASSERT(!m_InsideString);
    BeginValue(false);
    if (b)
        m_SB.Add(L"true");
    else
        m_SB.Add(L"false");
}

void JsonWriter::WriteNull()
{
    D3D12MA_ASSERT(!m_InsideString);
    BeginValue(false);
    m_SB.Add(L"null");
}

void JsonWriter::AddAllocationToObject(const Allocation& alloc)
{
    WriteString(L"Type");
    switch (alloc.m_PackedData.GetResourceDimension()) {
    case D3D12_RESOURCE_DIMENSION_UNKNOWN:
        WriteString(L"UNKNOWN");
        break;
    case D3D12_RESOURCE_DIMENSION_BUFFER:
        WriteString(L"BUFFER");
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        WriteString(L"TEXTURE1D");
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        WriteString(L"TEXTURE2D");
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
        WriteString(L"TEXTURE3D");
        break;
    default: D3D12MA_ASSERT(0); break;
    }

    WriteString(L"Size");
    WriteNumber(alloc.GetSize());
    WriteString(L"Usage");
    WriteNumber((UINT)alloc.m_PackedData.GetResourceFlags());

    void* privateData = alloc.GetPrivateData();
    if (privateData)
    {
        WriteString(L"CustomData");
        BeginString();
        ContinueString_Pointer(privateData);
        EndString();
    }

    LPCWSTR name = alloc.GetName();
    if (name != NULL)
    {
        WriteString(L"Name");
        WriteString(name);
    }
    if (alloc.m_PackedData.GetTextureLayout())
    {
        WriteString(L"Layout");
        WriteNumber((UINT)alloc.m_PackedData.GetTextureLayout());
    }
}

void JsonWriter::AddDetailedStatisticsInfoObject(const DetailedStatistics& stats)
{
    BeginObject();

    WriteString(L"BlockCount");
    WriteNumber(stats.Stats.BlockCount);
    WriteString(L"BlockBytes");
    WriteNumber(stats.Stats.BlockBytes);
    WriteString(L"AllocationCount");
    WriteNumber(stats.Stats.AllocationCount);
    WriteString(L"AllocationBytes");
    WriteNumber(stats.Stats.AllocationBytes);
    WriteString(L"UnusedRangeCount");
    WriteNumber(stats.UnusedRangeCount);

    if (stats.Stats.AllocationCount > 1)
    {
        WriteString(L"AllocationSizeMin");
        WriteNumber(stats.AllocationSizeMin);
        WriteString(L"AllocationSizeMax");
        WriteNumber(stats.AllocationSizeMax);
    }
    if (stats.UnusedRangeCount > 1)
    {
        WriteString(L"UnusedRangeSizeMin");
        WriteNumber(stats.UnusedRangeSizeMin);
        WriteString(L"UnusedRangeSizeMax");
        WriteNumber(stats.UnusedRangeSizeMax);
    }
    EndObject();
}

void JsonWriter::BeginValue(bool isString)
{
    if (!m_Stack.empty())
    {
        StackItem& currItem = m_Stack.back();
        if (currItem.type == COLLECTION_TYPE_OBJECT && currItem.valueCount % 2 == 0)
        {
            D3D12MA_ASSERT(isString);
        }

        if (currItem.type == COLLECTION_TYPE_OBJECT && currItem.valueCount % 2 == 1)
        {
            m_SB.Add(L':'); m_SB.Add(L' ');
        }
        else if (currItem.valueCount > 0)
        {
            m_SB.Add(L','); m_SB.Add(L' ');
            WriteIndent();
        }
        else
        {
            WriteIndent();
        }
        ++currItem.valueCount;
    }
}

void JsonWriter::WriteIndent(bool oneLess)
{
    if (!m_Stack.empty() && !m_Stack.back().singleLineMode)
    {
        m_SB.AddNewLine();

        size_t count = m_Stack.size();
        if (count > 0 && oneLess)
        {
            --count;
        }
        for (size_t i = 0; i < count; ++i)
        {
            m_SB.Add(INDENT);
        }
    }
}
#endif // _D3D12MA_JSON_WRITER_FUNCTIONS
#endif // _D3D12MA_JSON_WRITER

#ifndef _D3D12MA_POOL_ALLOCATOR
/*
Allocator for objects of type T using a list of arrays (pools) to speed up
allocation. Number of elements that can be allocated is not bounded because
allocator can create multiple blocks.
T should be POD because constructor and destructor is not called in Alloc or
Free.
*/
template<typename T>
class PoolAllocator
{
    D3D12MA_CLASS_NO_COPY(PoolAllocator)
public:
    // allocationCallbacks externally owned, must outlive this object.
    PoolAllocator(const ALLOCATION_CALLBACKS& allocationCallbacks, UINT firstBlockCapacity);
    ~PoolAllocator() { Clear(); }

    void Clear();
    template<typename... Types>
    T* Alloc(Types... args);
    void Free(T* ptr);

private:
    union Item
    {
        UINT NextFreeIndex; // UINT32_MAX means end of list.
        alignas(T) char Value[sizeof(T)];
    };

    struct ItemBlock
    {
        Item* pItems;
        UINT Capacity;
        UINT FirstFreeIndex;
    };

    const ALLOCATION_CALLBACKS& m_AllocationCallbacks;
    const UINT m_FirstBlockCapacity;
    Vector<ItemBlock> m_ItemBlocks;

    ItemBlock& CreateNewBlock();
};

#ifndef _D3D12MA_POOL_ALLOCATOR_FUNCTIONS
template<typename T>
PoolAllocator<T>::PoolAllocator(const ALLOCATION_CALLBACKS& allocationCallbacks, UINT firstBlockCapacity)
    : m_AllocationCallbacks(allocationCallbacks),
    m_FirstBlockCapacity(firstBlockCapacity),
    m_ItemBlocks(allocationCallbacks)
{
    D3D12MA_ASSERT(m_FirstBlockCapacity > 1);
}

template<typename T>
void PoolAllocator<T>::Clear()
{
    for(size_t i = m_ItemBlocks.size(); i--; )
    {
        D3D12MA_DELETE_ARRAY(m_AllocationCallbacks, m_ItemBlocks[i].pItems, m_ItemBlocks[i].Capacity);
    }
    m_ItemBlocks.clear(true);
}

template<typename T> template<typename... Types>
T* PoolAllocator<T>::Alloc(Types... args)
{
    for(size_t i = m_ItemBlocks.size(); i--; )
    {
        ItemBlock& block = m_ItemBlocks[i];
        // This block has some free items: Use first one.
        if(block.FirstFreeIndex != UINT32_MAX)
        {
            Item* const pItem = &block.pItems[block.FirstFreeIndex];
            block.FirstFreeIndex = pItem->NextFreeIndex;
            T* result = (T*)&pItem->Value;
            new(result)T(std::forward<Types>(args)...); // Explicit constructor call.
            return result;
        }
    }

    // No block has free item: Create new one and use it.
    ItemBlock& newBlock = CreateNewBlock();
    Item* const pItem = &newBlock.pItems[0];
    newBlock.FirstFreeIndex = pItem->NextFreeIndex;
    T* result = (T*)pItem->Value;
    new(result)T(std::forward<Types>(args)...); // Explicit constructor call.
    return result;
}

template<typename T>
void PoolAllocator<T>::Free(T* ptr)
{
    // Search all memory blocks to find ptr.
    for(size_t i = m_ItemBlocks.size(); i--; )
    {
        ItemBlock& block = m_ItemBlocks[i];

        Item* pItemPtr;
        memcpy(&pItemPtr, &ptr, sizeof(pItemPtr));

        // Check if pItemPtr is in address range of this block.
        if((pItemPtr >= block.pItems) && (pItemPtr < block.pItems + block.Capacity))
        {
            ptr->~T(); // Explicit destructor call.
            const UINT index = static_cast<UINT>(pItemPtr - block.pItems);
            pItemPtr->NextFreeIndex = block.FirstFreeIndex;
            block.FirstFreeIndex = index;
            return;
        }
    }
    D3D12MA_ASSERT(0 && "Pointer doesn't belong to this memory pool.");
}

template<typename T>
typename PoolAllocator<T>::ItemBlock& PoolAllocator<T>::CreateNewBlock()
{
    const UINT newBlockCapacity = m_ItemBlocks.empty() ?
        m_FirstBlockCapacity : m_ItemBlocks.back().Capacity * 3 / 2;

    const ItemBlock newBlock = {
        D3D12MA_NEW_ARRAY(m_AllocationCallbacks, Item, newBlockCapacity),
        newBlockCapacity,
        0 };

    m_ItemBlocks.push_back(newBlock);

    // Setup singly-linked list of all free items in this block.
    for(UINT i = 0; i < newBlockCapacity - 1; ++i)
    {
        newBlock.pItems[i].NextFreeIndex = i + 1;
    }
    newBlock.pItems[newBlockCapacity - 1].NextFreeIndex = UINT32_MAX;
    return m_ItemBlocks.back();
}
#endif // _D3D12MA_POOL_ALLOCATOR_FUNCTIONS
#endif // _D3D12MA_POOL_ALLOCATOR

#ifndef _D3D12MA_LIST
/*
Doubly linked list, with elements allocated out of PoolAllocator.
Has custom interface, as well as STL-style interface, including iterator and
const_iterator.
*/
template<typename T>
class List
{
    D3D12MA_CLASS_NO_COPY(List)
public:
    struct Item
    {
        Item* pPrev;
        Item* pNext;
        T Value;
    };

    class reverse_iterator;
    class const_reverse_iterator;
    class iterator
    {
        friend class List<T>;
        friend class const_iterator;

    public:
        iterator() = default;
        iterator(const reverse_iterator& src)
            : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

        T& operator*() const;
        T* operator->() const;

        iterator& operator++();
        iterator& operator--();
        iterator operator++(int);
        iterator operator--(int);

        bool operator==(const iterator& rhs) const;
        bool operator!=(const iterator& rhs) const;

    private:
        List<T>* m_pList = NULL;
        Item* m_pItem = NULL;

        iterator(List<T>* pList, Item* pItem) : m_pList(pList), m_pItem(pItem) {}
    };

    class reverse_iterator
    {
        friend class List<T>;
        friend class const_reverse_iterator;

    public:
        reverse_iterator() = default;
        reverse_iterator(const iterator& src)
            : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

        T& operator*() const;
        T* operator->() const;

        reverse_iterator& operator++();
        reverse_iterator& operator--();
        reverse_iterator operator++(int);
        reverse_iterator operator--(int);

        bool operator==(const reverse_iterator& rhs) const;
        bool operator!=(const reverse_iterator& rhs) const;

    private:
        List<T>* m_pList = NULL;
        Item* m_pItem = NULL;

        reverse_iterator(List<T>* pList, Item* pItem)
            : m_pList(pList), m_pItem(pItem) {}
    };

    class const_iterator
    {
        friend class List<T>;

    public:
        const_iterator() = default;
        const_iterator(const iterator& src)
            : m_pList(src.m_pList), m_pItem(src.m_pItem) {}
        const_iterator(const reverse_iterator& src)
            : m_pList(src.m_pList), m_pItem(src.m_pItem) {}
        const_iterator(const const_reverse_iterator& src)
            : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

        iterator dropConst() const;
        const T& operator*() const;
        const T* operator->() const;

        const_iterator& operator++();
        const_iterator& operator--();
        const_iterator operator++(int);
        const_iterator operator--(int);

        bool operator==(const const_iterator& rhs) const;
        bool operator!=(const const_iterator& rhs) const;

    private:
        const List<T>* m_pList = NULL;
        const Item* m_pItem = NULL;

        const_iterator(const List<T>* pList, const Item* pItem)
            : m_pList(pList), m_pItem(pItem) {}
    };

    class const_reverse_iterator
    {
        friend class List<T>;

    public:
        const_reverse_iterator() = default;
        const_reverse_iterator(const iterator& src)
            : m_pList(src.m_pList), m_pItem(src.m_pItem) {}
        const_reverse_iterator(const reverse_iterator& src)
            : m_pList(src.m_pList), m_pItem(src.m_pItem) {}
        const_reverse_iterator(const const_iterator& src)
            : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

        reverse_iterator dropConst() const;
        const T& operator*() const;
        const T* operator->() const;

        const_reverse_iterator& operator++();
        const_reverse_iterator& operator--();
        const_reverse_iterator operator++(int);
        const_reverse_iterator operator--(int);

        bool operator==(const const_reverse_iterator& rhs) const;
        bool operator!=(const const_reverse_iterator& rhs) const;

    private:
        const List<T>* m_pList = NULL;
        const Item* m_pItem = NULL;

        const_reverse_iterator(const List<T>* pList, const Item* pItem)
            : m_pList(pList), m_pItem(pItem) {}
    };

    // allocationCallbacks externally owned, must outlive this object.
    List(const ALLOCATION_CALLBACKS& allocationCallbacks);
    // Intentionally not calling Clear, because that would be unnecessary
    // computations to return all items to m_ItemAllocator as free.
    ~List() = default;
    
    size_t GetCount() const { return m_Count; }
    bool IsEmpty() const { return m_Count == 0; }

    Item* Front() { return m_pFront; }
    const Item* Front() const { return m_pFront; }
    Item* Back() { return m_pBack; }
    const Item* Back() const { return m_pBack; }

    bool empty() const { return IsEmpty(); }
    size_t size() const { return GetCount(); }
    void push_back(const T& value) { PushBack(value); }
    iterator insert(iterator it, const T& value) { return iterator(this, InsertBefore(it.m_pItem, value)); }
    void clear() { Clear(); }
    void erase(iterator it) { Remove(it.m_pItem); }

    iterator begin() { return iterator(this, Front()); }
    iterator end() { return iterator(this, NULL); }
    reverse_iterator rbegin() { return reverse_iterator(this, Back()); }
    reverse_iterator rend() { return reverse_iterator(this, NULL); }

    const_iterator cbegin() const { return const_iterator(this, Front()); }
    const_iterator cend() const { return const_iterator(this, NULL); }
    const_iterator begin() const { return cbegin(); }
    const_iterator end() const { return cend(); }

    const_reverse_iterator crbegin() const { return const_reverse_iterator(this, Back()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(this, NULL); }
    const_reverse_iterator rbegin() const { return crbegin(); }
    const_reverse_iterator rend() const { return crend(); }

    Item* PushBack();
    Item* PushFront();
    Item* PushBack(const T& value);
    Item* PushFront(const T& value);
    void PopBack();
    void PopFront();

    // Item can be null - it means PushBack.
    Item* InsertBefore(Item* pItem);
    // Item can be null - it means PushFront.
    Item* InsertAfter(Item* pItem);
    Item* InsertBefore(Item* pItem, const T& value);
    Item* InsertAfter(Item* pItem, const T& value);

    void Clear();
    void Remove(Item* pItem);

private:
    const ALLOCATION_CALLBACKS& m_AllocationCallbacks;
    PoolAllocator<Item> m_ItemAllocator;
    Item* m_pFront;
    Item* m_pBack;
    size_t m_Count;
};

#ifndef _D3D12MA_LIST_ITERATOR_FUNCTIONS
template<typename T>
T& List<T>::iterator::operator*() const
{
    D3D12MA_HEAVY_ASSERT(m_pItem != NULL);
    return m_pItem->Value;
}

template<typename T>
T* List<T>::iterator::operator->() const
{
    D3D12MA_HEAVY_ASSERT(m_pItem != NULL);
    return &m_pItem->Value;
}

template<typename T>
typename List<T>::iterator& List<T>::iterator::operator++()
{
    D3D12MA_HEAVY_ASSERT(m_pItem != NULL);
    m_pItem = m_pItem->pNext;
    return *this;
}

template<typename T>
typename List<T>::iterator& List<T>::iterator::operator--()
{
    if (m_pItem != NULL)
    {
        m_pItem = m_pItem->pPrev;
    }
    else
    {
        D3D12MA_HEAVY_ASSERT(!m_pList->IsEmpty());
        m_pItem = m_pList->Back();
    }
    return *this;
}

template<typename T>
typename List<T>::iterator List<T>::iterator::operator++(int)
{
    iterator result = *this;
    ++* this;
    return result;
}

template<typename T>
typename List<T>::iterator List<T>::iterator::operator--(int)
{
    iterator result = *this;
    --* this;
    return result;
}

template<typename T>
bool List<T>::iterator::operator==(const iterator& rhs) const
{
    D3D12MA_HEAVY_ASSERT(m_pList == rhs.m_pList);
    return m_pItem == rhs.m_pItem;
}

template<typename T>
bool List<T>::iterator::operator!=(const iterator& rhs) const
{
    D3D12MA_HEAVY_ASSERT(m_pList == rhs.m_pList);
    return m_pItem != rhs.m_pItem;
}
#endif // _D3D12MA_LIST_ITERATOR_FUNCTIONS

#ifndef _D3D12MA_LIST_REVERSE_ITERATOR_FUNCTIONS
template<typename T>
T& List<T>::reverse_iterator::operator*() const
{
    D3D12MA_HEAVY_ASSERT(m_pItem != NULL);
    return m_pItem->Value;
}

template<typename T>
T* List<T>::reverse_iterator::operator->() const
{
    D3D12MA_HEAVY_ASSERT(m_pItem != NULL);
    return &m_pItem->Value;
}

template<typename T>
typename List<T>::reverse_iterator& List<T>::reverse_iterator::operator++()
{
    D3D12MA_HEAVY_ASSERT(m_pItem != NULL);
    m_pItem = m_pItem->pPrev;
    return *this;
}

template<typename T>
typename List<T>::reverse_iterator& List<T>::reverse_iterator::operator--()
{
    if (m_pItem != NULL)
    {
        m_pItem = m_pItem->pNext;
    }
    else
    {
        D3D12MA_HEAVY_ASSERT(!m_pList->IsEmpty());
        m_pItem = m_pList->Front();
    }
    return *this;
}

template<typename T>
typename List<T>::reverse_iterator List<T>::reverse_iterator::operator++(int)
{
    reverse_iterator result = *this;
    ++* this;
    return result;
}

template<typename T>
typename List<T>::reverse_iterator List<T>::reverse_iterator::operator--(int)
{
    reverse_iterator result = *this;
    --* this;
    return result;
}

template<typename T>
bool List<T>::reverse_iterator::operator==(const reverse_iterator& rhs) const
{
    D3D12MA_HEAVY_ASSERT(m_pList == rhs.m_pList);
    return m_pItem == rhs.m_pItem;
}

template<typename T>
bool List<T>::reverse_iterator::operator!=(const reverse_iterator& rhs) const
{
    D3D12MA_HEAVY_ASSERT(m_pList == rhs.m_pList);
    return m_pItem != rhs.m_pItem;
}
#endif // _D3D12MA_LIST_REVERSE_ITERATOR_FUNCTIONS

#ifndef _D3D12MA_LIST_CONST_ITERATOR_FUNCTIONS
template<typename T>
typename List<T>::iterator List<T>::const_iterator::dropConst() const
{
    return iterator(const_cast<List<T>*>(m_pList), const_cast<Item*>(m_pItem));
}

template<typename T>
const T& List<T>::const_iterator::operator*() const
{
    D3D12MA_HEAVY_ASSERT(m_pItem != NULL);
    return m_pItem->Value;
}

template<typename T>
const T* List<T>::const_iterator::operator->() const
{
    D3D12MA_HEAVY_ASSERT(m_pItem != NULL);
    return &m_pItem->Value;
}

template<typename T>
typename List<T>::const_iterator& List<T>::const_iterator::operator++()
{
    D3D12MA_HEAVY_ASSERT(m_pItem != NULL);
    m_pItem = m_pItem->pNext;
    return *this;
}

template<typename T>
typename List<T>::const_iterator& List<T>::const_iterator::operator--()
{
    if (m_pItem != NULL)
    {
        m_pItem = m_pItem->pPrev;
    }
    else
    {
        D3D12MA_HEAVY_ASSERT(!m_pList->IsEmpty());
        m_pItem = m_pList->Back();
    }
    return *this;
}

template<typename T>
typename List<T>::const_iterator List<T>::const_iterator::operator++(int)
{
    const_iterator result = *this;
    ++* this;
    return result;
}

template<typename T>
typename List<T>::const_iterator List<T>::const_iterator::operator--(int)
{
    const_iterator result = *this;
    --* this;
    return result;
}

template<typename T>
bool List<T>::const_iterator::operator==(const const_iterator& rhs) const
{
    D3D12MA_HEAVY_ASSERT(m_pList == rhs.m_pList);
    return m_pItem == rhs.m_pItem;
}

template<typename T>
bool List<T>::const_iterator::operator!=(const const_iterator& rhs) const
{
    D3D12MA_HEAVY_ASSERT(m_pList == rhs.m_pList);
    return m_pItem != rhs.m_pItem;
}
#endif // _D3D12MA_LIST_CONST_ITERATOR_FUNCTIONS

#ifndef _D3D12MA_LIST_CONST_REVERSE_ITERATOR_FUNCTIONS
template<typename T>
typename List<T>::reverse_iterator List<T>::const_reverse_iterator::dropConst() const
{
    return reverse_iterator(const_cast<List<T>*>(m_pList), const_cast<Item*>(m_pItem));
}

template<typename T>
const T& List<T>::const_reverse_iterator::operator*() const
{
    D3D12MA_HEAVY_ASSERT(m_pItem != NULL);
    return m_pItem->Value;
}

template<typename T>
const T* List<T>::const_reverse_iterator::operator->() const
{
    D3D12MA_HEAVY_ASSERT(m_pItem != NULL);
    return &m_pItem->Value;
}

template<typename T>
typename List<T>::const_reverse_iterator& List<T>::const_reverse_iterator::operator++()
{
    D3D12MA_HEAVY_ASSERT(m_pItem != NULL);
    m_pItem = m_pItem->pPrev;
    return *this;
}

template<typename T>
typename List<T>::const_reverse_iterator& List<T>::const_reverse_iterator::operator--()
{
    if (m_pItem != NULL)
    {
        m_pItem = m_pItem->pNext;
    }
    else
    {
        D3D12MA_HEAVY_ASSERT(!m_pList->IsEmpty());
        m_pItem = m_pList->Front();
    }
    return *this;
}

template<typename T>
typename List<T>::const_reverse_iterator List<T>::const_reverse_iterator::operator++(int)
{
    const_reverse_iterator result = *this;
    ++* this;
    return result;
}

template<typename T>
typename List<T>::const_reverse_iterator List<T>::const_reverse_iterator::operator--(int)
{
    const_reverse_iterator result = *this;
    --* this;
    return result;
}

template<typename T>
bool List<T>::const_reverse_iterator::operator==(const const_reverse_iterator& rhs) const
{
    D3D12MA_HEAVY_ASSERT(m_pList == rhs.m_pList);
    return m_pItem == rhs.m_pItem;
}

template<typename T>
bool List<T>::const_reverse_iterator::operator!=(const const_reverse_iterator& rhs) const
{
    D3D12MA_HEAVY_ASSERT(m_pList == rhs.m_pList);
    return m_pItem != rhs.m_pItem;
}
#endif // _D3D12MA_LIST_CONST_REVERSE_ITERATOR_FUNCTIONS

#ifndef _D3D12MA_LIST_FUNCTIONS
template<typename T>
List<T>::List(const ALLOCATION_CALLBACKS& allocationCallbacks)
    : m_AllocationCallbacks(allocationCallbacks),
    m_ItemAllocator(allocationCallbacks, 128),
    m_pFront(NULL),
    m_pBack(NULL),
    m_Count(0) {}

template<typename T>
void List<T>::Clear()
{
    if(!IsEmpty())
    {
        Item* pItem = m_pBack;
        while(pItem != NULL)
        {
            Item* const pPrevItem = pItem->pPrev;
            m_ItemAllocator.Free(pItem);
            pItem = pPrevItem;
        }
        m_pFront = NULL;
        m_pBack = NULL;
        m_Count = 0;
    }
}

template<typename T>
typename List<T>::Item* List<T>::PushBack()
{
    Item* const pNewItem = m_ItemAllocator.Alloc();
    pNewItem->pNext = NULL;
    if(IsEmpty())
    {
        pNewItem->pPrev = NULL;
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

template<typename T>
typename List<T>::Item* List<T>::PushFront()
{
    Item* const pNewItem = m_ItemAllocator.Alloc();
    pNewItem->pPrev = NULL;
    if(IsEmpty())
    {
        pNewItem->pNext = NULL;
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

template<typename T>
typename List<T>::Item* List<T>::PushBack(const T& value)
{
    Item* const pNewItem = PushBack();
    pNewItem->Value = value;
    return pNewItem;
}

template<typename T>
typename List<T>::Item* List<T>::PushFront(const T& value)
{
    Item* const pNewItem = PushFront();
    pNewItem->Value = value;
    return pNewItem;
}

template<typename T>
void List<T>::PopBack()
{
    D3D12MA_HEAVY_ASSERT(m_Count > 0);
    Item* const pBackItem = m_pBack;
    Item* const pPrevItem = pBackItem->pPrev;
    if(pPrevItem != NULL)
    {
        pPrevItem->pNext = NULL;
    }
    m_pBack = pPrevItem;
    m_ItemAllocator.Free(pBackItem);
    --m_Count;
}

template<typename T>
void List<T>::PopFront()
{
    D3D12MA_HEAVY_ASSERT(m_Count > 0);
    Item* const pFrontItem = m_pFront;
    Item* const pNextItem = pFrontItem->pNext;
    if(pNextItem != NULL)
    {
        pNextItem->pPrev = NULL;
    }
    m_pFront = pNextItem;
    m_ItemAllocator.Free(pFrontItem);
    --m_Count;
}

template<typename T>
void List<T>::Remove(Item* pItem)
{
    D3D12MA_HEAVY_ASSERT(pItem != NULL);
    D3D12MA_HEAVY_ASSERT(m_Count > 0);

    if(pItem->pPrev != NULL)
    {
        pItem->pPrev->pNext = pItem->pNext;
    }
    else
    {
        D3D12MA_HEAVY_ASSERT(m_pFront == pItem);
        m_pFront = pItem->pNext;
    }

    if(pItem->pNext != NULL)
    {
        pItem->pNext->pPrev = pItem->pPrev;
    }
    else
    {
        D3D12MA_HEAVY_ASSERT(m_pBack == pItem);
        m_pBack = pItem->pPrev;
    }

    m_ItemAllocator.Free(pItem);
    --m_Count;
}

template<typename T>
typename List<T>::Item* List<T>::InsertBefore(Item* pItem)
{
    if(pItem != NULL)
    {
        Item* const prevItem = pItem->pPrev;
        Item* const newItem = m_ItemAllocator.Alloc();
        newItem->pPrev = prevItem;
        newItem->pNext = pItem;
        pItem->pPrev = newItem;
        if(prevItem != NULL)
        {
            prevItem->pNext = newItem;
        }
        else
        {
            D3D12MA_HEAVY_ASSERT(m_pFront == pItem);
            m_pFront = newItem;
        }
        ++m_Count;
        return newItem;
    }
    else
    {
        return PushBack();
    }
}

template<typename T>
typename List<T>::Item* List<T>::InsertAfter(Item* pItem)
{
    if(pItem != NULL)
    {
        Item* const nextItem = pItem->pNext;
        Item* const newItem = m_ItemAllocator.Alloc();
        newItem->pNext = nextItem;
        newItem->pPrev = pItem;
        pItem->pNext = newItem;
        if(nextItem != NULL)
        {
            nextItem->pPrev = newItem;
        }
        else
        {
            D3D12MA_HEAVY_ASSERT(m_pBack == pItem);
            m_pBack = newItem;
        }
        ++m_Count;
        return newItem;
    }
    else
        return PushFront();
}

template<typename T>
typename List<T>::Item* List<T>::InsertBefore(Item* pItem, const T& value)
{
    Item* const newItem = InsertBefore(pItem);
    newItem->Value = value;
    return newItem;
}

template<typename T>
typename List<T>::Item* List<T>::InsertAfter(Item* pItem, const T& value)
{
    Item* const newItem = InsertAfter(pItem);
    newItem->Value = value;
    return newItem;
}
#endif // _D3D12MA_LIST_FUNCTIONS
#endif // _D3D12MA_LIST

#ifndef _D3D12MA_INTRUSIVE_LINKED_LIST
/*
Expected interface of ItemTypeTraits:
struct MyItemTypeTraits
{
    using ItemType = MyItem;
    static ItemType* GetPrev(const ItemType* item) { return item->myPrevPtr; }
    static ItemType* GetNext(const ItemType* item) { return item->myNextPtr; }
    static ItemType*& AccessPrev(ItemType* item) { return item->myPrevPtr; }
    static ItemType*& AccessNext(ItemType* item) { return item->myNextPtr; }
};
*/
template<typename ItemTypeTraits>
class IntrusiveLinkedList
{
public:
    using ItemType = typename ItemTypeTraits::ItemType;
    static ItemType* GetPrev(const ItemType* item) { return ItemTypeTraits::GetPrev(item); }
    static ItemType* GetNext(const ItemType* item) { return ItemTypeTraits::GetNext(item); }

    // Movable, not copyable.
    IntrusiveLinkedList() = default;
    IntrusiveLinkedList(const IntrusiveLinkedList&) = delete;
    IntrusiveLinkedList(IntrusiveLinkedList&& src);
    IntrusiveLinkedList& operator=(const IntrusiveLinkedList&) = delete;
    IntrusiveLinkedList& operator=(IntrusiveLinkedList&& src);
    ~IntrusiveLinkedList() { D3D12MA_HEAVY_ASSERT(IsEmpty()); }

    size_t GetCount() const { return m_Count; }
    bool IsEmpty() const { return m_Count == 0; }

    ItemType* Front() { return m_Front; }
    ItemType* Back() { return m_Back; }
    const ItemType* Front() const { return m_Front; }
    const ItemType* Back() const { return m_Back; }

    void PushBack(ItemType* item);
    void PushFront(ItemType* item);
    ItemType* PopBack();
    ItemType* PopFront();

    // MyItem can be null - it means PushBack.
    void InsertBefore(ItemType* existingItem, ItemType* newItem);
    // MyItem can be null - it means PushFront.
    void InsertAfter(ItemType* existingItem, ItemType* newItem);

    void Remove(ItemType* item);
    void RemoveAll();

private:
    ItemType* m_Front = NULL;
    ItemType* m_Back = NULL;
    size_t m_Count = 0;
};

#ifndef _D3D12MA_INTRUSIVE_LINKED_LIST_FUNCTIONS
template<typename ItemTypeTraits>
IntrusiveLinkedList<ItemTypeTraits>::IntrusiveLinkedList(IntrusiveLinkedList&& src)
    : m_Front(src.m_Front), m_Back(src.m_Back), m_Count(src.m_Count)
{
    src.m_Front = src.m_Back = NULL;
    src.m_Count = 0;
}

template<typename ItemTypeTraits>
IntrusiveLinkedList<ItemTypeTraits>& IntrusiveLinkedList<ItemTypeTraits>::operator=(IntrusiveLinkedList&& src)
{
    if (&src != this)
    {
        D3D12MA_HEAVY_ASSERT(IsEmpty());
        m_Front = src.m_Front;
        m_Back = src.m_Back;
        m_Count = src.m_Count;
        src.m_Front = src.m_Back = NULL;
        src.m_Count = 0;
    }
    return *this;
}

template<typename ItemTypeTraits>
void IntrusiveLinkedList<ItemTypeTraits>::PushBack(ItemType* item)
{
    D3D12MA_HEAVY_ASSERT(ItemTypeTraits::GetPrev(item) == NULL && ItemTypeTraits::GetNext(item) == NULL);
    if (IsEmpty())
    {
        m_Front = item;
        m_Back = item;
        m_Count = 1;
    }
    else
    {
        ItemTypeTraits::AccessPrev(item) = m_Back;
        ItemTypeTraits::AccessNext(m_Back) = item;
        m_Back = item;
        ++m_Count;
    }
}

template<typename ItemTypeTraits>
void IntrusiveLinkedList<ItemTypeTraits>::PushFront(ItemType* item)
{
    D3D12MA_HEAVY_ASSERT(ItemTypeTraits::GetPrev(item) == NULL && ItemTypeTraits::GetNext(item) == NULL);
    if (IsEmpty())
    {
        m_Front = item;
        m_Back = item;
        m_Count = 1;
    }
    else
    {
        ItemTypeTraits::AccessNext(item) = m_Front;
        ItemTypeTraits::AccessPrev(m_Front) = item;
        m_Front = item;
        ++m_Count;
    }
}

template<typename ItemTypeTraits>
typename IntrusiveLinkedList<ItemTypeTraits>::ItemType* IntrusiveLinkedList<ItemTypeTraits>::PopBack()
{
    D3D12MA_HEAVY_ASSERT(m_Count > 0);
    ItemType* const backItem = m_Back;
    ItemType* const prevItem = ItemTypeTraits::GetPrev(backItem);
    if (prevItem != NULL)
    {
        ItemTypeTraits::AccessNext(prevItem) = NULL;
    }
    m_Back = prevItem;
    --m_Count;
    ItemTypeTraits::AccessPrev(backItem) = NULL;
    ItemTypeTraits::AccessNext(backItem) = NULL;
    return backItem;
}

template<typename ItemTypeTraits>
typename IntrusiveLinkedList<ItemTypeTraits>::ItemType* IntrusiveLinkedList<ItemTypeTraits>::PopFront()
{
    D3D12MA_HEAVY_ASSERT(m_Count > 0);
    ItemType* const frontItem = m_Front;
    ItemType* const nextItem = ItemTypeTraits::GetNext(frontItem);
    if (nextItem != NULL)
    {
        ItemTypeTraits::AccessPrev(nextItem) = NULL;
    }
    m_Front = nextItem;
    --m_Count;
    ItemTypeTraits::AccessPrev(frontItem) = NULL;
    ItemTypeTraits::AccessNext(frontItem) = NULL;
    return frontItem;
}

template<typename ItemTypeTraits>
void IntrusiveLinkedList<ItemTypeTraits>::InsertBefore(ItemType* existingItem, ItemType* newItem)
{
    D3D12MA_HEAVY_ASSERT(newItem != NULL && ItemTypeTraits::GetPrev(newItem) == NULL && ItemTypeTraits::GetNext(newItem) == NULL);
    if (existingItem != NULL)
    {
        ItemType* const prevItem = ItemTypeTraits::GetPrev(existingItem);
        ItemTypeTraits::AccessPrev(newItem) = prevItem;
        ItemTypeTraits::AccessNext(newItem) = existingItem;
        ItemTypeTraits::AccessPrev(existingItem) = newItem;
        if (prevItem != NULL)
        {
            ItemTypeTraits::AccessNext(prevItem) = newItem;
        }
        else
        {
            D3D12MA_HEAVY_ASSERT(m_Front == existingItem);
            m_Front = newItem;
        }
        ++m_Count;
    }
    else
        PushBack(newItem);
}

template<typename ItemTypeTraits>
void IntrusiveLinkedList<ItemTypeTraits>::InsertAfter(ItemType* existingItem, ItemType* newItem)
{
    D3D12MA_HEAVY_ASSERT(newItem != NULL && ItemTypeTraits::GetPrev(newItem) == NULL && ItemTypeTraits::GetNext(newItem) == NULL);
    if (existingItem != NULL)
    {
        ItemType* const nextItem = ItemTypeTraits::GetNext(existingItem);
        ItemTypeTraits::AccessNext(newItem) = nextItem;
        ItemTypeTraits::AccessPrev(newItem) = existingItem;
        ItemTypeTraits::AccessNext(existingItem) = newItem;
        if (nextItem != NULL)
        {
            ItemTypeTraits::AccessPrev(nextItem) = newItem;
        }
        else
        {
            D3D12MA_HEAVY_ASSERT(m_Back == existingItem);
            m_Back = newItem;
        }
        ++m_Count;
    }
    else
        return PushFront(newItem);
}

template<typename ItemTypeTraits>
void IntrusiveLinkedList<ItemTypeTraits>::Remove(ItemType* item)
{
    D3D12MA_HEAVY_ASSERT(item != NULL && m_Count > 0);
    if (ItemTypeTraits::GetPrev(item) != NULL)
    {
        ItemTypeTraits::AccessNext(ItemTypeTraits::AccessPrev(item)) = ItemTypeTraits::GetNext(item);
    }
    else
    {
        D3D12MA_HEAVY_ASSERT(m_Front == item);
        m_Front = ItemTypeTraits::GetNext(item);
    }

    if (ItemTypeTraits::GetNext(item) != NULL)
    {
        ItemTypeTraits::AccessPrev(ItemTypeTraits::AccessNext(item)) = ItemTypeTraits::GetPrev(item);
    }
    else
    {
        D3D12MA_HEAVY_ASSERT(m_Back == item);
        m_Back = ItemTypeTraits::GetPrev(item);
    }
    ItemTypeTraits::AccessPrev(item) = NULL;
    ItemTypeTraits::AccessNext(item) = NULL;
    --m_Count;
}

template<typename ItemTypeTraits>
void IntrusiveLinkedList<ItemTypeTraits>::RemoveAll()
{
    if (!IsEmpty())
    {
        ItemType* item = m_Back;
        while (item != NULL)
        {
            ItemType* const prevItem = ItemTypeTraits::AccessPrev(item);
            ItemTypeTraits::AccessPrev(item) = NULL;
            ItemTypeTraits::AccessNext(item) = NULL;
            item = prevItem;
        }
        m_Front = NULL;
        m_Back = NULL;
        m_Count = 0;
    }
}
#endif // _D3D12MA_INTRUSIVE_LINKED_LIST_FUNCTIONS
#endif // _D3D12MA_INTRUSIVE_LINKED_LIST

#ifndef _D3D12MA_ALLOCATION_OBJECT_ALLOCATOR
/*
Thread-safe wrapper over PoolAllocator free list, for allocation of Allocation objects.
*/
class AllocationObjectAllocator
{
    D3D12MA_CLASS_NO_COPY(AllocationObjectAllocator);
public:
    AllocationObjectAllocator(const ALLOCATION_CALLBACKS& allocationCallbacks)
        : m_Allocator(allocationCallbacks, 1024) {}

    template<typename... Types>
    Allocation* Allocate(Types... args);
    void Free(Allocation* alloc);

private:
    D3D12MA_MUTEX m_Mutex;
    PoolAllocator<Allocation> m_Allocator;
};

#ifndef _D3D12MA_ALLOCATION_OBJECT_ALLOCATOR_FUNCTIONS
template<typename... Types>
Allocation* AllocationObjectAllocator::Allocate(Types... args)
{
    MutexLock mutexLock(m_Mutex);
    return m_Allocator.Alloc(std::forward<Types>(args)...);
}

void AllocationObjectAllocator::Free(Allocation* alloc)
{
    MutexLock mutexLock(m_Mutex);
    m_Allocator.Free(alloc);
}
#endif // _D3D12MA_ALLOCATION_OBJECT_ALLOCATOR_FUNCTIONS
#endif // _D3D12MA_ALLOCATION_OBJECT_ALLOCATOR

#ifndef _D3D12MA_SUBALLOCATION
/*
Represents a region of NormalBlock that is either assigned and returned as
allocated memory block or free.
*/
struct Suballocation
{
    UINT64 offset;
    UINT64 size;
    void* privateData;
    SuballocationType type;
};
using SuballocationList = List<Suballocation>;

// Comparator for offsets.
struct SuballocationOffsetLess
{
    bool operator()(const Suballocation& lhs, const Suballocation& rhs) const
    {
        return lhs.offset < rhs.offset;
    }
};

struct SuballocationOffsetGreater
{
    bool operator()(const Suballocation& lhs, const Suballocation& rhs) const
    {
        return lhs.offset > rhs.offset;
    }
};

struct SuballocationItemSizeLess
{
    bool operator()(const SuballocationList::iterator lhs, const SuballocationList::iterator rhs) const
    {
        return lhs->size < rhs->size;
    }
    bool operator()(const SuballocationList::iterator lhs, UINT64 rhsSize) const
    {
        return lhs->size < rhsSize;
    }
};
#endif // _D3D12MA_SUBALLOCATION

#ifndef _D3D12MA_ALLOCATION_REQUEST
/*
Parameters of planned allocation inside a NormalBlock.
*/
struct AllocationRequest
{
    AllocHandle allocHandle;
    UINT64 size;
    UINT64 algorithmData;
    UINT64 sumFreeSize; // Sum size of free items that overlap with proposed allocation.
    UINT64 sumItemSize; // Sum size of items to make lost that overlap with proposed allocation.
    SuballocationList::iterator item;
    BOOL zeroInitialized;
};
#endif // _D3D12MA_ALLOCATION_REQUEST

#ifndef _D3D12MA_ZERO_INITIALIZED_RANGE
/*
Keeps track of the range of bytes that are surely initialized with zeros.
Everything outside of it is considered uninitialized memory that may contain
garbage data.

The range is left-inclusive.
*/
class ZeroInitializedRange
{
public:
    void Reset(UINT64 size);
    BOOL IsRangeZeroInitialized(UINT64 beg, UINT64 end) const;
    void MarkRangeAsUsed(UINT64 usedBeg, UINT64 usedEnd);

private:
    UINT64 m_ZeroBeg = 0, m_ZeroEnd = 0;
};

#ifndef _D3D12MA_ZERO_INITIALIZED_RANGE_FUNCTIONS
void ZeroInitializedRange::Reset(UINT64 size)
{
    D3D12MA_ASSERT(size > 0);
    m_ZeroBeg = 0;
    m_ZeroEnd = size;
}

BOOL ZeroInitializedRange::IsRangeZeroInitialized(UINT64 beg, UINT64 end) const
{
    D3D12MA_ASSERT(beg < end);
    return m_ZeroBeg <= beg && end <= m_ZeroEnd;
}

void ZeroInitializedRange::MarkRangeAsUsed(UINT64 usedBeg, UINT64 usedEnd)
{
    D3D12MA_ASSERT(usedBeg < usedEnd);
    // No new bytes marked.
    if (usedEnd <= m_ZeroBeg || m_ZeroEnd <= usedBeg)
    {
        return;
    }
    // All bytes marked.
    if (usedBeg <= m_ZeroBeg && m_ZeroEnd <= usedEnd)
    {
        m_ZeroBeg = m_ZeroEnd = 0;
    }
    // Some bytes marked.
    else
    {
        const UINT64 remainingZeroBefore = usedBeg > m_ZeroBeg ? usedBeg - m_ZeroBeg : 0;
        const UINT64 remainingZeroAfter = usedEnd < m_ZeroEnd ? m_ZeroEnd - usedEnd : 0;
        D3D12MA_ASSERT(remainingZeroBefore > 0 || remainingZeroAfter > 0);
        if (remainingZeroBefore > remainingZeroAfter)
        {
            m_ZeroEnd = usedBeg;
        }
        else
        {
            m_ZeroBeg = usedEnd;
        }
    }
}
#endif // _D3D12MA_ZERO_INITIALIZED_RANGE_FUNCTIONS
#endif // _D3D12MA_ZERO_INITIALIZED_RANGE

#ifndef _D3D12MA_BLOCK_METADATA
/*
Data structure used for bookkeeping of allocations and unused ranges of memory
in a single ID3D12Heap memory block.
*/
class BlockMetadata
{
public:
    BlockMetadata(const ALLOCATION_CALLBACKS* allocationCallbacks, bool isVirtual);
    virtual ~BlockMetadata() = default;

    virtual void Init(UINT64 size) { m_Size = size; }
    // Validates all data structures inside this object. If not valid, returns false.
    virtual bool Validate() const = 0;
    UINT64 GetSize() const { return m_Size; }
    bool IsVirtual() const { return m_IsVirtual; }
    virtual size_t GetAllocationCount() const = 0;
    virtual size_t GetFreeRegionsCount() const = 0;
    virtual UINT64 GetSumFreeSize() const = 0;
    virtual UINT64 GetAllocationOffset(AllocHandle allocHandle) const = 0;
    // Returns true if this block is empty - contains only single free suballocation.
    virtual bool IsEmpty() const = 0;

    virtual void GetAllocationInfo(AllocHandle allocHandle, VIRTUAL_ALLOCATION_INFO& outInfo) const = 0;

    // Tries to find a place for suballocation with given parameters inside this block.
    // If succeeded, fills pAllocationRequest and returns true.
    // If failed, returns false.
    virtual bool CreateAllocationRequest(
        UINT64 allocSize,
        UINT64 allocAlignment,
        bool upperAddress,
        UINT32 strategy,
        AllocationRequest* pAllocationRequest) = 0;

    // Makes actual allocation based on request. Request must already be checked and valid.
    virtual void Alloc(
        const AllocationRequest& request,
        UINT64 allocSize,
        void* PrivateData) = 0;

    virtual void Free(AllocHandle allocHandle) = 0;
    // Frees all allocations.
    // Careful! Don't call it if there are Allocation objects owned by pPrivateData of of cleared allocations!
    virtual void Clear() = 0;

    virtual AllocHandle GetAllocationListBegin() const = 0;
    virtual AllocHandle GetNextAllocation(AllocHandle prevAlloc) const = 0;
    virtual UINT64 GetNextFreeRegionSize(AllocHandle alloc) const = 0;
    virtual void* GetAllocationPrivateData(AllocHandle allocHandle) const = 0;
    virtual void SetAllocationPrivateData(AllocHandle allocHandle, void* privateData) = 0;

    virtual void AddStatistics(Statistics& inoutStats) const = 0;
    virtual void AddDetailedStatistics(DetailedStatistics& inoutStats) const = 0;
    virtual void WriteAllocationInfoToJson(JsonWriter& json) const = 0;

protected:
    const ALLOCATION_CALLBACKS* GetAllocs() const { return m_pAllocationCallbacks; }
    UINT64 GetDebugMargin() const { return IsVirtual() ? 0 : D3D12MA_DEBUG_MARGIN; }

    void PrintDetailedMap_Begin(JsonWriter& json,
        UINT64 unusedBytes,
        size_t allocationCount,
        size_t unusedRangeCount) const;
    void PrintDetailedMap_Allocation(JsonWriter& json,
        UINT64 offset, UINT64 size, void* privateData) const;
    void PrintDetailedMap_UnusedRange(JsonWriter& json,
        UINT64 offset, UINT64 size) const;
    void PrintDetailedMap_End(JsonWriter& json) const;

private:
    UINT64 m_Size;
    bool m_IsVirtual;
    const ALLOCATION_CALLBACKS* m_pAllocationCallbacks;

    D3D12MA_CLASS_NO_COPY(BlockMetadata);
};

#ifndef _D3D12MA_BLOCK_METADATA_FUNCTIONS
BlockMetadata::BlockMetadata(const ALLOCATION_CALLBACKS* allocationCallbacks, bool isVirtual)
    : m_Size(0),
    m_IsVirtual(isVirtual),
    m_pAllocationCallbacks(allocationCallbacks)
{
    D3D12MA_ASSERT(allocationCallbacks);
}

void BlockMetadata::PrintDetailedMap_Begin(JsonWriter& json,
    UINT64 unusedBytes, size_t allocationCount, size_t unusedRangeCount) const
{
    json.WriteString(L"TotalBytes");
    json.WriteNumber(GetSize());

    json.WriteString(L"UnusedBytes");
    json.WriteNumber(unusedBytes);

    json.WriteString(L"Allocations");
    json.WriteNumber(allocationCount);

    json.WriteString(L"UnusedRanges");
    json.WriteNumber(unusedRangeCount);

    json.WriteString(L"Suballocations");
    json.BeginArray();
}

void BlockMetadata::PrintDetailedMap_Allocation(JsonWriter& json,
    UINT64 offset, UINT64 size, void* privateData) const
{
    json.BeginObject(true);

    json.WriteString(L"Offset");
    json.WriteNumber(offset);

    if (IsVirtual())
    {
        json.WriteString(L"Size");
        json.WriteNumber(size);
        if (privateData)
        {
            json.WriteString(L"CustomData");
            json.WriteNumber((uintptr_t)privateData);
        }
    }
    else
    {
        const Allocation* const alloc = (const Allocation*)privateData;
        D3D12MA_ASSERT(alloc);
        json.AddAllocationToObject(*alloc);
    }
    json.EndObject();
}

void BlockMetadata::PrintDetailedMap_UnusedRange(JsonWriter& json,
    UINT64 offset, UINT64 size) const
{
    json.BeginObject(true);

    json.WriteString(L"Offset");
    json.WriteNumber(offset);

    json.WriteString(L"Type");
    json.WriteString(L"FREE");

    json.WriteString(L"Size");
    json.WriteNumber(size);

    json.EndObject();
}

void BlockMetadata::PrintDetailedMap_End(JsonWriter& json) const
{
    json.EndArray();
}
#endif // _D3D12MA_BLOCK_METADATA_FUNCTIONS
#endif // _D3D12MA_BLOCK_METADATA

#if 0
#ifndef _D3D12MA_BLOCK_METADATA_GENERIC
class BlockMetadata_Generic : public BlockMetadata
{
public:
    BlockMetadata_Generic(const ALLOCATION_CALLBACKS* allocationCallbacks, bool isVirtual);
    virtual ~BlockMetadata_Generic() = default;

    size_t GetAllocationCount() const override { return m_Suballocations.size() - m_FreeCount; }
    UINT64 GetSumFreeSize() const override { return m_SumFreeSize; }
    UINT64 GetAllocationOffset(AllocHandle allocHandle) const override { return (UINT64)allocHandle - 1; }

    void Init(UINT64 size) override;
    bool Validate() const override;
    bool IsEmpty() const override;
    void GetAllocationInfo(AllocHandle allocHandle, VIRTUAL_ALLOCATION_INFO& outInfo) const override;

    bool CreateAllocationRequest(
        UINT64 allocSize,
        UINT64 allocAlignment,
        bool upperAddress,
        AllocationRequest* pAllocationRequest) override;

    void Alloc(
        const AllocationRequest& request,
        UINT64 allocSize,
        void* privateData) override;

    void Free(AllocHandle allocHandle) override;
    void Clear() override;

    void SetAllocationPrivateData(AllocHandle allocHandle, void* privateData) override;

    void AddStatistics(Statistics& inoutStats) const override;
    void AddDetailedStatistics(DetailedStatistics& inoutStats) const override;
    void WriteAllocationInfoToJson(JsonWriter& json) const override;

private:
    UINT m_FreeCount;
    UINT64 m_SumFreeSize;
    SuballocationList m_Suballocations;
    // Suballocations that are free and have size greater than certain threshold.
    // Sorted by size, ascending.
    Vector<SuballocationList::iterator> m_FreeSuballocationsBySize;
    ZeroInitializedRange m_ZeroInitializedRange;

    SuballocationList::const_iterator FindAtOffset(UINT64 offset) const;
    bool ValidateFreeSuballocationList() const;

    // Checks if requested suballocation with given parameters can be placed in given pFreeSuballocItem.
    // If yes, fills pOffset and returns true. If no, returns false.
    bool CheckAllocation(
        UINT64 allocSize,
        UINT64 allocAlignment,
        SuballocationList::const_iterator suballocItem,
        AllocHandle* pAllocHandle,
        UINT64* pSumFreeSize,
        UINT64* pSumItemSize,
        BOOL *pZeroInitialized) const;
    // Given free suballocation, it merges it with following one, which must also be free.
    void MergeFreeWithNext(SuballocationList::iterator item);
    // Releases given suballocation, making it free.
    // Merges it with adjacent free suballocations if applicable.
    // Returns iterator to new free suballocation at this place.
    SuballocationList::iterator FreeSuballocation(SuballocationList::iterator suballocItem);
    // Given free suballocation, it inserts it into sorted list of
    // m_FreeSuballocationsBySize if it's suitable.
    void RegisterFreeSuballocation(SuballocationList::iterator item);
    // Given free suballocation, it removes it from sorted list of
    // m_FreeSuballocationsBySize if it's suitable.
    void UnregisterFreeSuballocation(SuballocationList::iterator item);

    D3D12MA_CLASS_NO_COPY(BlockMetadata_Generic)
};

#ifndef _D3D12MA_BLOCK_METADATA_GENERIC_FUNCTIONS
BlockMetadata_Generic::BlockMetadata_Generic(const ALLOCATION_CALLBACKS* allocationCallbacks, bool isVirtual)
    : BlockMetadata(allocationCallbacks, isVirtual),
    m_FreeCount(0),
    m_SumFreeSize(0),
    m_Suballocations(*allocationCallbacks),
    m_FreeSuballocationsBySize(*allocationCallbacks)
{
    D3D12MA_ASSERT(allocationCallbacks);
}

void BlockMetadata_Generic::Init(UINT64 size)
{
    BlockMetadata::Init(size);
    m_ZeroInitializedRange.Reset(size);

    m_FreeCount = 1;
    m_SumFreeSize = size;

    Suballocation suballoc = {};
    suballoc.offset = 0;
    suballoc.size = size;
    suballoc.type = SUBALLOCATION_TYPE_FREE;
    suballoc.privateData = NULL;

    D3D12MA_ASSERT(size > MIN_FREE_SUBALLOCATION_SIZE_TO_REGISTER);
    m_Suballocations.push_back(suballoc);
    SuballocationList::iterator suballocItem = m_Suballocations.end();
    --suballocItem;
    m_FreeSuballocationsBySize.push_back(suballocItem);
}

bool BlockMetadata_Generic::Validate() const
{
    D3D12MA_VALIDATE(!m_Suballocations.empty());

    // Expected offset of new suballocation as calculated from previous ones.
    UINT64 calculatedOffset = 0;
    // Expected number of free suballocations as calculated from traversing their list.
    UINT calculatedFreeCount = 0;
    // Expected sum size of free suballocations as calculated from traversing their list.
    UINT64 calculatedSumFreeSize = 0;
    // Expected number of free suballocations that should be registered in
    // m_FreeSuballocationsBySize calculated from traversing their list.
    size_t freeSuballocationsToRegister = 0;
    // True if previous visited suballocation was free.
    bool prevFree = false;

    for (const auto& subAlloc : m_Suballocations)
    {
        // Actual offset of this suballocation doesn't match expected one.
        D3D12MA_VALIDATE(subAlloc.offset == calculatedOffset);

        const bool currFree = (subAlloc.type == SUBALLOCATION_TYPE_FREE);
        // Two adjacent free suballocations are invalid. They should be merged.
        D3D12MA_VALIDATE(!prevFree || !currFree);

        const Allocation* const alloc = (Allocation*)subAlloc.privateData;
        if (!IsVirtual())
        {
            D3D12MA_VALIDATE(currFree == (alloc == NULL));
        }

        if (currFree)
        {
            calculatedSumFreeSize += subAlloc.size;
            ++calculatedFreeCount;
            if (subAlloc.size >= MIN_FREE_SUBALLOCATION_SIZE_TO_REGISTER)
            {
                ++freeSuballocationsToRegister;
            }

            // Margin required between allocations - every free space must be at least that large.
            D3D12MA_VALIDATE(subAlloc.size >= GetDebugMargin());
        }
        else
        {
            if (!IsVirtual())
            {
                D3D12MA_VALIDATE(alloc->GetOffset() == subAlloc.offset);
                D3D12MA_VALIDATE(alloc->GetSize() == subAlloc.size);
            }

            // Margin required between allocations - previous allocation must be free.
            D3D12MA_VALIDATE(GetDebugMargin() == 0 || prevFree);
        }

        calculatedOffset += subAlloc.size;
        prevFree = currFree;
    }

    // Number of free suballocations registered in m_FreeSuballocationsBySize doesn't
    // match expected one.
    D3D12MA_VALIDATE(m_FreeSuballocationsBySize.size() == freeSuballocationsToRegister);

    UINT64 lastSize = 0;
    for (size_t i = 0; i < m_FreeSuballocationsBySize.size(); ++i)
    {
        SuballocationList::iterator suballocItem = m_FreeSuballocationsBySize[i];

        // Only free suballocations can be registered in m_FreeSuballocationsBySize.
        D3D12MA_VALIDATE(suballocItem->type == SUBALLOCATION_TYPE_FREE);
        // They must be sorted by size ascending.
        D3D12MA_VALIDATE(suballocItem->size >= lastSize);

        lastSize = suballocItem->size;
    }

    // Check if totals match calculacted values.
    D3D12MA_VALIDATE(ValidateFreeSuballocationList());
    D3D12MA_VALIDATE(calculatedOffset == GetSize());
    D3D12MA_VALIDATE(calculatedSumFreeSize == m_SumFreeSize);
    D3D12MA_VALIDATE(calculatedFreeCount == m_FreeCount);

    return true;
}

bool BlockMetadata_Generic::IsEmpty() const
{
    return (m_Suballocations.size() == 1) && (m_FreeCount == 1);
}

void BlockMetadata_Generic::GetAllocationInfo(AllocHandle allocHandle, VIRTUAL_ALLOCATION_INFO& outInfo) const
{
    Suballocation& suballoc = *FindAtOffset((UINT64)allocHandle - 1).dropConst();
    outInfo.Offset = suballoc.offset;
    outInfo.Size = suballoc.size;
    outInfo.pPrivateData = suballoc.privateData;
}

bool BlockMetadata_Generic::CreateAllocationRequest(
    UINT64 allocSize,
    UINT64 allocAlignment,
    bool upperAddress,
    AllocationRequest* pAllocationRequest)
{
    D3D12MA_ASSERT(allocSize > 0);
    D3D12MA_ASSERT(!upperAddress && "ALLOCATION_FLAG_UPPER_ADDRESS can be used only with linear algorithm.");
    D3D12MA_ASSERT(pAllocationRequest != NULL);
    D3D12MA_HEAVY_ASSERT(Validate());

    // There is not enough total free space in this block to fullfill the request: Early return.
    if (m_SumFreeSize < allocSize + GetDebugMargin())
    {
        return false;
    }

    // New algorithm, efficiently searching freeSuballocationsBySize.
    const size_t freeSuballocCount = m_FreeSuballocationsBySize.size();
    if (freeSuballocCount > 0)
    {
        // Find first free suballocation with size not less than allocSize + GetDebugMargin().
        SuballocationList::iterator* const it = BinaryFindFirstNotLess(
            m_FreeSuballocationsBySize.data(),
            m_FreeSuballocationsBySize.data() + freeSuballocCount,
            allocSize + GetDebugMargin(),
            SuballocationItemSizeLess());
        size_t index = it - m_FreeSuballocationsBySize.data();
        for (; index < freeSuballocCount; ++index)
        {
            if (CheckAllocation(
                allocSize,
                allocAlignment,
                m_FreeSuballocationsBySize[index],
                &pAllocationRequest->allocHandle,
                &pAllocationRequest->sumFreeSize,
                &pAllocationRequest->sumItemSize,
                &pAllocationRequest->zeroInitialized))
            {
                pAllocationRequest->item = m_FreeSuballocationsBySize[index];
                return true;
            }
        }
    }

    return false;
}

void BlockMetadata_Generic::Alloc(
    const AllocationRequest& request,
    UINT64 allocSize,
    void* privateData)
{
    D3D12MA_ASSERT(request.item != m_Suballocations.end());
    Suballocation& suballoc = *request.item;
    // Given suballocation is a free block.
    D3D12MA_ASSERT(suballoc.type == SUBALLOCATION_TYPE_FREE);
    // Given offset is inside this suballocation.
    UINT64 offset = (UINT64)request.allocHandle - 1;
    D3D12MA_ASSERT(offset >= suballoc.offset);
    const UINT64 paddingBegin = offset - suballoc.offset;
    D3D12MA_ASSERT(suballoc.size >= paddingBegin + allocSize);
    const UINT64 paddingEnd = suballoc.size - paddingBegin - allocSize;

    // Unregister this free suballocation from m_FreeSuballocationsBySize and update
    // it to become used.
    UnregisterFreeSuballocation(request.item);

    suballoc.offset = offset;
    suballoc.size = allocSize;
    suballoc.type = SUBALLOCATION_TYPE_ALLOCATION;
    suballoc.privateData = privateData;

    // If there are any free bytes remaining at the end, insert new free suballocation after current one.
    if (paddingEnd)
    {
        Suballocation paddingSuballoc = {};
        paddingSuballoc.offset = offset + allocSize;
        paddingSuballoc.size = paddingEnd;
        paddingSuballoc.type = SUBALLOCATION_TYPE_FREE;
        SuballocationList::iterator next = request.item;
        ++next;
        const SuballocationList::iterator paddingEndItem =
            m_Suballocations.insert(next, paddingSuballoc);
        RegisterFreeSuballocation(paddingEndItem);
    }

    // If there are any free bytes remaining at the beginning, insert new free suballocation before current one.
    if (paddingBegin)
    {
        Suballocation paddingSuballoc = {};
        paddingSuballoc.offset = offset - paddingBegin;
        paddingSuballoc.size = paddingBegin;
        paddingSuballoc.type = SUBALLOCATION_TYPE_FREE;
        const SuballocationList::iterator paddingBeginItem =
            m_Suballocations.insert(request.item, paddingSuballoc);
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

    m_ZeroInitializedRange.MarkRangeAsUsed(offset, offset + allocSize);
}

void BlockMetadata_Generic::Free(AllocHandle allocHandle)
{
    FreeSuballocation(FindAtOffset((UINT64)allocHandle - 1).dropConst());
}

void BlockMetadata_Generic::Clear()
{
    m_FreeCount = 1;
    m_SumFreeSize = GetSize();

    m_Suballocations.clear();
    Suballocation suballoc = {};
    suballoc.offset = 0;
    suballoc.size = GetSize();
    suballoc.type = SUBALLOCATION_TYPE_FREE;
    m_Suballocations.push_back(suballoc);

    m_FreeSuballocationsBySize.clear();
    m_FreeSuballocationsBySize.push_back(m_Suballocations.begin());
}

SuballocationList::const_iterator BlockMetadata_Generic::FindAtOffset(UINT64 offset) const
{
    const UINT64 last = m_Suballocations.crbegin()->offset;
    if (last == offset)
        return m_Suballocations.crbegin();
    const UINT64 first = m_Suballocations.cbegin()->offset;
    if (first == offset)
        return m_Suballocations.cbegin();

    const size_t suballocCount = m_Suballocations.size();
    const UINT64 step = (last - first + m_Suballocations.cbegin()->size) / suballocCount;
    auto findSuballocation = [&](auto begin, auto end) -> SuballocationList::const_iterator
    {
        for (auto suballocItem = begin;
            suballocItem != end;
            ++suballocItem)
        {
            const Suballocation& suballoc = *suballocItem;
            if (suballoc.offset == offset)
                return suballocItem;
        }
        D3D12MA_ASSERT(false && "Not found!");
        return m_Suballocations.end();
    };
    // If requested offset is closer to the end of range, search from the end
    if ((offset - first) > suballocCount * step / 2)
    {
        return findSuballocation(m_Suballocations.crbegin(), m_Suballocations.crend());
    }
    return findSuballocation(m_Suballocations.cbegin(), m_Suballocations.cend());
}

bool BlockMetadata_Generic::ValidateFreeSuballocationList() const
{
    UINT64 lastSize = 0;
    for (size_t i = 0, count = m_FreeSuballocationsBySize.size(); i < count; ++i)
    {
        const SuballocationList::iterator it = m_FreeSuballocationsBySize[i];

        D3D12MA_VALIDATE(it->type == SUBALLOCATION_TYPE_FREE);
        D3D12MA_VALIDATE(it->size >= MIN_FREE_SUBALLOCATION_SIZE_TO_REGISTER);
        D3D12MA_VALIDATE(it->size >= lastSize);
        lastSize = it->size;
    }
    return true;
}

bool BlockMetadata_Generic::CheckAllocation(
    UINT64 allocSize,
    UINT64 allocAlignment,
    SuballocationList::const_iterator suballocItem,
    AllocHandle* pAllocHandle,
    UINT64* pSumFreeSize,
    UINT64* pSumItemSize,
    BOOL* pZeroInitialized) const
{
    D3D12MA_ASSERT(allocSize > 0);
    D3D12MA_ASSERT(suballocItem != m_Suballocations.cend());
    D3D12MA_ASSERT(pAllocHandle != NULL && pZeroInitialized != NULL);

    *pSumFreeSize = 0;
    *pSumItemSize = 0;
    *pZeroInitialized = FALSE;

    const Suballocation& suballoc = *suballocItem;
    D3D12MA_ASSERT(suballoc.type == SUBALLOCATION_TYPE_FREE);

    *pSumFreeSize = suballoc.size;

    // Size of this suballocation is too small for this request: Early return.
    if (suballoc.size < allocSize)
    {
        return false;
    }

    // Start from offset equal to beginning of this suballocation and debug margin of previous allocation if present.
    UINT64 offset = suballoc.offset + (suballocItem == m_Suballocations.cbegin() ? 0 : GetDebugMargin());

    // Apply alignment.
    offset = AlignUp(offset, allocAlignment);

    // Calculate padding at the beginning based on current offset.
    const UINT64 paddingBegin = offset - suballoc.offset;

    // Fail if requested size plus margin after is bigger than size of this suballocation.
    if (paddingBegin + allocSize + GetDebugMargin() > suballoc.size)
    {
        return false;
    }

    // All tests passed: Success. Offset is already filled.
    *pZeroInitialized = m_ZeroInitializedRange.IsRangeZeroInitialized(offset, offset + allocSize);
    *pAllocHandle = (AllocHandle)(offset + 1);
    return true;
}

void BlockMetadata_Generic::MergeFreeWithNext(SuballocationList::iterator item)
{
    D3D12MA_ASSERT(item != m_Suballocations.end());
    D3D12MA_ASSERT(item->type == SUBALLOCATION_TYPE_FREE);

    SuballocationList::iterator nextItem = item;
    ++nextItem;
    D3D12MA_ASSERT(nextItem != m_Suballocations.end());
    D3D12MA_ASSERT(nextItem->type == SUBALLOCATION_TYPE_FREE);

    item->size += nextItem->size;
    --m_FreeCount;
    m_Suballocations.erase(nextItem);
}

SuballocationList::iterator BlockMetadata_Generic::FreeSuballocation(SuballocationList::iterator suballocItem)
{
    // Change this suballocation to be marked as free.
    Suballocation& suballoc = *suballocItem;
    suballoc.type = SUBALLOCATION_TYPE_FREE;
    suballoc.privateData = NULL;

    // Update totals.
    ++m_FreeCount;
    m_SumFreeSize += suballoc.size;

    // Merge with previous and/or next suballocation if it's also free.
    bool mergeWithNext = false;
    bool mergeWithPrev = false;

    SuballocationList::iterator nextItem = suballocItem;
    ++nextItem;
    if ((nextItem != m_Suballocations.end()) && (nextItem->type == SUBALLOCATION_TYPE_FREE))
    {
        mergeWithNext = true;
    }

    SuballocationList::iterator prevItem = suballocItem;
    if (suballocItem != m_Suballocations.begin())
    {
        --prevItem;
        if (prevItem->type == SUBALLOCATION_TYPE_FREE)
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
        return prevItem;
    }
    else
    {
        RegisterFreeSuballocation(suballocItem);
        return suballocItem;
    }
}

void BlockMetadata_Generic::RegisterFreeSuballocation(SuballocationList::iterator item)
{
    D3D12MA_ASSERT(item->type == SUBALLOCATION_TYPE_FREE);
    D3D12MA_ASSERT(item->size > 0);

    // You may want to enable this validation at the beginning or at the end of
    // this function, depending on what do you want to check.
    D3D12MA_HEAVY_ASSERT(ValidateFreeSuballocationList());

    if (item->size >= MIN_FREE_SUBALLOCATION_SIZE_TO_REGISTER)
    {
        if (m_FreeSuballocationsBySize.empty())
        {
            m_FreeSuballocationsBySize.push_back(item);
        }
        else
        {
            m_FreeSuballocationsBySize.InsertSorted(item, SuballocationItemSizeLess());
        }
    }

    //D3D12MA_HEAVY_ASSERT(ValidateFreeSuballocationList());
}

void BlockMetadata_Generic::UnregisterFreeSuballocation(SuballocationList::iterator item)
{
    D3D12MA_ASSERT(item->type == SUBALLOCATION_TYPE_FREE);
    D3D12MA_ASSERT(item->size > 0);

    // You may want to enable this validation at the beginning or at the end of
    // this function, depending on what do you want to check.
    D3D12MA_HEAVY_ASSERT(ValidateFreeSuballocationList());

    if (item->size >= MIN_FREE_SUBALLOCATION_SIZE_TO_REGISTER)
    {
        SuballocationList::iterator* const it = BinaryFindFirstNotLess(
            m_FreeSuballocationsBySize.data(),
            m_FreeSuballocationsBySize.data() + m_FreeSuballocationsBySize.size(),
            item,
            SuballocationItemSizeLess());
        for (size_t index = it - m_FreeSuballocationsBySize.data();
            index < m_FreeSuballocationsBySize.size();
            ++index)
        {
            if (m_FreeSuballocationsBySize[index] == item)
            {
                m_FreeSuballocationsBySize.remove(index);
                return;
            }
            D3D12MA_ASSERT((m_FreeSuballocationsBySize[index]->size == item->size) && "Not found.");
        }
        D3D12MA_ASSERT(0 && "Not found.");
    }

    //D3D12MA_HEAVY_ASSERT(ValidateFreeSuballocationList());
}

void BlockMetadata_Generic::SetAllocationPrivateData(AllocHandle allocHandle, void* privateData)
{
    Suballocation& suballoc = *FindAtOffset((UINT64)allocHandle - 1).dropConst();
    suballoc.privateData = privateData;
}

void BlockMetadata_Generic::AddStatistics(Statistics& inoutStats) const
{
    inoutStats.BlockCount++;
    inoutStats.AllocationCount += (UINT)m_Suballocations.size() - m_FreeCount;
    inoutStats.BlockBytes += GetSize();
    inoutStats.AllocationBytes += GetSize() - m_SumFreeSize;
}

void BlockMetadata_Generic::AddDetailedStatistics(DetailedStatistics& inoutStats) const
{
    inoutStats.Stats.BlockCount++;
    inoutStats.Stats.BlockBytes += GetSize();

    for (const auto& suballoc : m_Suballocations)
    {
        if (suballoc.type == SUBALLOCATION_TYPE_FREE)
            AddDetailedStatisticsUnusedRange(inoutStats, suballoc.size);
        else
            AddDetailedStatisticsAllocation(inoutStats, suballoc.size);
    }
}

void BlockMetadata_Generic::WriteAllocationInfoToJson(JsonWriter& json) const
{
    PrintDetailedMap_Begin(json, GetSumFreeSize(), GetAllocationCount(), m_FreeCount);
    for (const auto& suballoc : m_Suballocations)
    {
        if (suballoc.type == SUBALLOCATION_TYPE_FREE)
            PrintDetailedMap_UnusedRange(json, suballoc.offset, suballoc.size);
        else
            PrintDetailedMap_Allocation(json, suballoc.offset, suballoc.size, suballoc.privateData);
    }
    PrintDetailedMap_End(json);
}
#endif // _D3D12MA_BLOCK_METADATA_GENERIC_FUNCTIONS
#endif // _D3D12MA_BLOCK_METADATA_GENERIC
#endif // #if 0

#ifndef _D3D12MA_BLOCK_METADATA_LINEAR
class BlockMetadata_Linear : public BlockMetadata
{
public:
    BlockMetadata_Linear(const ALLOCATION_CALLBACKS* allocationCallbacks, bool isVirtual);
    virtual ~BlockMetadata_Linear() = default;

    UINT64 GetSumFreeSize() const override { return m_SumFreeSize; }
    bool IsEmpty() const override { return GetAllocationCount() == 0; }
    UINT64 GetAllocationOffset(AllocHandle allocHandle) const override { return (UINT64)allocHandle - 1; };

    void Init(UINT64 size) override;
    bool Validate() const override;
    size_t GetAllocationCount() const override;
    size_t GetFreeRegionsCount() const override;
    void GetAllocationInfo(AllocHandle allocHandle, VIRTUAL_ALLOCATION_INFO& outInfo) const override;

    bool CreateAllocationRequest(
        UINT64 allocSize,
        UINT64 allocAlignment,
        bool upperAddress,
        UINT32 strategy,
        AllocationRequest* pAllocationRequest) override;

    void Alloc(
        const AllocationRequest& request,
        UINT64 allocSize,
        void* privateData) override;

    void Free(AllocHandle allocHandle) override;
    void Clear() override;

    AllocHandle GetAllocationListBegin() const override;
    AllocHandle GetNextAllocation(AllocHandle prevAlloc) const override;
    UINT64 GetNextFreeRegionSize(AllocHandle alloc) const override;
    void* GetAllocationPrivateData(AllocHandle allocHandle) const override;
    void SetAllocationPrivateData(AllocHandle allocHandle, void* privateData) override;

    void AddStatistics(Statistics& inoutStats) const override;
    void AddDetailedStatistics(DetailedStatistics& inoutStats) const override;
    void WriteAllocationInfoToJson(JsonWriter& json) const override;

private:
    /*
    There are two suballocation vectors, used in ping-pong way.
    The one with index m_1stVectorIndex is called 1st.
    The one with index (m_1stVectorIndex ^ 1) is called 2nd.
    2nd can be non-empty only when 1st is not empty.
    When 2nd is not empty, m_2ndVectorMode indicates its mode of operation.
    */
    typedef Vector<Suballocation> SuballocationVectorType;

    enum ALLOC_REQUEST_TYPE
    {
        ALLOC_REQUEST_UPPER_ADDRESS,
        ALLOC_REQUEST_END_OF_1ST,
        ALLOC_REQUEST_END_OF_2ND,
    };

    enum SECOND_VECTOR_MODE
    {
        SECOND_VECTOR_EMPTY,
        /*
        Suballocations in 2nd vector are created later than the ones in 1st, but they
        all have smaller offset.
        */
        SECOND_VECTOR_RING_BUFFER,
        /*
        Suballocations in 2nd vector are upper side of double stack.
        They all have offsets higher than those in 1st vector.
        Top of this stack means smaller offsets, but higher indices in this vector.
        */
        SECOND_VECTOR_DOUBLE_STACK,
    };

    UINT64 m_SumFreeSize;
    SuballocationVectorType m_Suballocations0, m_Suballocations1;
    UINT32 m_1stVectorIndex;
    SECOND_VECTOR_MODE m_2ndVectorMode;
    // Number of items in 1st vector with hAllocation = null at the beginning.
    size_t m_1stNullItemsBeginCount;
    // Number of other items in 1st vector with hAllocation = null somewhere in the middle.
    size_t m_1stNullItemsMiddleCount;
    // Number of items in 2nd vector with hAllocation = null.
    size_t m_2ndNullItemsCount;

    SuballocationVectorType& AccessSuballocations1st() { return m_1stVectorIndex ? m_Suballocations1 : m_Suballocations0; }
    SuballocationVectorType& AccessSuballocations2nd() { return m_1stVectorIndex ? m_Suballocations0 : m_Suballocations1; }
    const SuballocationVectorType& AccessSuballocations1st() const { return m_1stVectorIndex ? m_Suballocations1 : m_Suballocations0; }
    const SuballocationVectorType& AccessSuballocations2nd() const { return m_1stVectorIndex ? m_Suballocations0 : m_Suballocations1; }

    Suballocation& FindSuballocation(UINT64 offset) const;
    bool ShouldCompact1st() const;
    void CleanupAfterFree();

    bool CreateAllocationRequest_LowerAddress(
        UINT64 allocSize,
        UINT64 allocAlignment,
        AllocationRequest* pAllocationRequest);
    bool CreateAllocationRequest_UpperAddress(
        UINT64 allocSize,
        UINT64 allocAlignment,
        AllocationRequest* pAllocationRequest);

    D3D12MA_CLASS_NO_COPY(BlockMetadata_Linear)
};

#ifndef _D3D12MA_BLOCK_METADATA_LINEAR_FUNCTIONS
BlockMetadata_Linear::BlockMetadata_Linear(const ALLOCATION_CALLBACKS* allocationCallbacks, bool isVirtual)
    : BlockMetadata(allocationCallbacks, isVirtual),
    m_SumFreeSize(0),
    m_Suballocations0(*allocationCallbacks),
    m_Suballocations1(*allocationCallbacks),
    m_1stVectorIndex(0),
    m_2ndVectorMode(SECOND_VECTOR_EMPTY),
    m_1stNullItemsBeginCount(0),
    m_1stNullItemsMiddleCount(0),
    m_2ndNullItemsCount(0)
{
    D3D12MA_ASSERT(allocationCallbacks);
}

void BlockMetadata_Linear::Init(UINT64 size)
{
    BlockMetadata::Init(size);
    m_SumFreeSize = size;
}

bool BlockMetadata_Linear::Validate() const
{
    D3D12MA_VALIDATE(GetSumFreeSize() <= GetSize());
    const SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    const SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

    D3D12MA_VALIDATE(suballocations2nd.empty() == (m_2ndVectorMode == SECOND_VECTOR_EMPTY));
    D3D12MA_VALIDATE(!suballocations1st.empty() ||
        suballocations2nd.empty() ||
        m_2ndVectorMode != SECOND_VECTOR_RING_BUFFER);

    if (!suballocations1st.empty())
    {
        // Null item at the beginning should be accounted into m_1stNullItemsBeginCount.
        D3D12MA_VALIDATE(suballocations1st[m_1stNullItemsBeginCount].type != SUBALLOCATION_TYPE_FREE);
        // Null item at the end should be just pop_back().
        D3D12MA_VALIDATE(suballocations1st.back().type != SUBALLOCATION_TYPE_FREE);
    }
    if (!suballocations2nd.empty())
    {
        // Null item at the end should be just pop_back().
        D3D12MA_VALIDATE(suballocations2nd.back().type != SUBALLOCATION_TYPE_FREE);
    }

    D3D12MA_VALIDATE(m_1stNullItemsBeginCount + m_1stNullItemsMiddleCount <= suballocations1st.size());
    D3D12MA_VALIDATE(m_2ndNullItemsCount <= suballocations2nd.size());

    UINT64 sumUsedSize = 0;
    const size_t suballoc1stCount = suballocations1st.size();
    UINT64 offset = 0;

    if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
    {
        const size_t suballoc2ndCount = suballocations2nd.size();
        size_t nullItem2ndCount = 0;
        for (size_t i = 0; i < suballoc2ndCount; ++i)
        {
            const Suballocation& suballoc = suballocations2nd[i];
            const bool currFree = (suballoc.type == SUBALLOCATION_TYPE_FREE);

            const Allocation* alloc = (Allocation*)suballoc.privateData;
            if (!IsVirtual())
            {
                D3D12MA_VALIDATE(currFree == (alloc == NULL));
            }
            D3D12MA_VALIDATE(suballoc.offset >= offset);

            if (!currFree)
            {
                if (!IsVirtual())
                {
                    D3D12MA_VALIDATE(GetAllocationOffset(alloc->GetAllocHandle()) == suballoc.offset);
                    D3D12MA_VALIDATE(alloc->GetSize() == suballoc.size);
                }
                sumUsedSize += suballoc.size;
            }
            else
            {
                ++nullItem2ndCount;
            }

            offset = suballoc.offset + suballoc.size + GetDebugMargin();
        }

        D3D12MA_VALIDATE(nullItem2ndCount == m_2ndNullItemsCount);
    }

    for (size_t i = 0; i < m_1stNullItemsBeginCount; ++i)
    {
        const Suballocation& suballoc = suballocations1st[i];
        D3D12MA_VALIDATE(suballoc.type == SUBALLOCATION_TYPE_FREE &&
            suballoc.privateData == NULL);
    }

    size_t nullItem1stCount = m_1stNullItemsBeginCount;

    for (size_t i = m_1stNullItemsBeginCount; i < suballoc1stCount; ++i)
    {
        const Suballocation& suballoc = suballocations1st[i];
        const bool currFree = (suballoc.type == SUBALLOCATION_TYPE_FREE);

        const Allocation* alloc = (Allocation*)suballoc.privateData;
        if (!IsVirtual())
        {
            D3D12MA_VALIDATE(currFree == (alloc == NULL));
        }
        D3D12MA_VALIDATE(suballoc.offset >= offset);
        D3D12MA_VALIDATE(i >= m_1stNullItemsBeginCount || currFree);

        if (!currFree)
        {
            if (!IsVirtual())
            {
                D3D12MA_VALIDATE(GetAllocationOffset(alloc->GetAllocHandle()) == suballoc.offset);
                D3D12MA_VALIDATE(alloc->GetSize() == suballoc.size);
            }
            sumUsedSize += suballoc.size;
        }
        else
        {
            ++nullItem1stCount;
        }

        offset = suballoc.offset + suballoc.size + GetDebugMargin();
    }
    D3D12MA_VALIDATE(nullItem1stCount == m_1stNullItemsBeginCount + m_1stNullItemsMiddleCount);

    if (m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
    {
        const size_t suballoc2ndCount = suballocations2nd.size();
        size_t nullItem2ndCount = 0;
        for (size_t i = suballoc2ndCount; i--; )
        {
            const Suballocation& suballoc = suballocations2nd[i];
            const bool currFree = (suballoc.type == SUBALLOCATION_TYPE_FREE);

            const Allocation* alloc = (Allocation*)suballoc.privateData;
            if (!IsVirtual())
            {
                D3D12MA_VALIDATE(currFree == (alloc == NULL));
            }
            D3D12MA_VALIDATE(suballoc.offset >= offset);

            if (!currFree)
            {
                if (!IsVirtual())
                {
                    D3D12MA_VALIDATE(GetAllocationOffset(alloc->GetAllocHandle()) == suballoc.offset);
                    D3D12MA_VALIDATE(alloc->GetSize() == suballoc.size);
                }
                sumUsedSize += suballoc.size;
            }
            else
            {
                ++nullItem2ndCount;
            }

            offset = suballoc.offset + suballoc.size + GetDebugMargin();
        }

        D3D12MA_VALIDATE(nullItem2ndCount == m_2ndNullItemsCount);
    }

    D3D12MA_VALIDATE(offset <= GetSize());
    D3D12MA_VALIDATE(m_SumFreeSize == GetSize() - sumUsedSize);

    return true;
}

size_t BlockMetadata_Linear::GetAllocationCount() const
{
    return AccessSuballocations1st().size() - m_1stNullItemsBeginCount - m_1stNullItemsMiddleCount +
        AccessSuballocations2nd().size() - m_2ndNullItemsCount;
}

size_t BlockMetadata_Linear::GetFreeRegionsCount() const
{
    // Function only used for defragmentation, which is disabled for this algorithm
    D3D12MA_ASSERT(0);
    return SIZE_MAX;
}

void BlockMetadata_Linear::GetAllocationInfo(AllocHandle allocHandle, VIRTUAL_ALLOCATION_INFO& outInfo) const
{
    const Suballocation& suballoc = FindSuballocation((UINT64)allocHandle - 1);
    outInfo.Offset = suballoc.offset;
    outInfo.Size = suballoc.size;
    outInfo.pPrivateData = suballoc.privateData;
}

bool BlockMetadata_Linear::CreateAllocationRequest(
    UINT64 allocSize,
    UINT64 allocAlignment,
    bool upperAddress,
    UINT32 strategy,
    AllocationRequest* pAllocationRequest)
{
    UNREF_PARAM(strategy); 
    D3D12MA_ASSERT(allocSize > 0 && "Cannot allocate empty block!");
    D3D12MA_ASSERT(pAllocationRequest != NULL);
    D3D12MA_HEAVY_ASSERT(Validate());
    pAllocationRequest->size = allocSize;
    return upperAddress ?
        CreateAllocationRequest_UpperAddress(
            allocSize, allocAlignment, pAllocationRequest) :
        CreateAllocationRequest_LowerAddress(
            allocSize, allocAlignment, pAllocationRequest);
}

void BlockMetadata_Linear::Alloc(
    const AllocationRequest& request,
    UINT64 allocSize,
    void* privateData)
{
    UNREF_PARAM(allocSize); 
    UINT64 offset = (UINT64)request.allocHandle - 1;
    const Suballocation newSuballoc = { offset, request.size, privateData, SUBALLOCATION_TYPE_ALLOCATION };

    switch (request.algorithmData)
    {
    case ALLOC_REQUEST_UPPER_ADDRESS:
    {
        D3D12MA_ASSERT(m_2ndVectorMode != SECOND_VECTOR_RING_BUFFER &&
            "CRITICAL ERROR: Trying to use linear allocator as double stack while it was already used as ring buffer.");
        SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();
        suballocations2nd.push_back(newSuballoc);
        m_2ndVectorMode = SECOND_VECTOR_DOUBLE_STACK;
        break;
    }
    case ALLOC_REQUEST_END_OF_1ST:
    {
        SuballocationVectorType& suballocations1st = AccessSuballocations1st();

        D3D12MA_ASSERT(suballocations1st.empty() ||
            offset >= suballocations1st.back().offset + suballocations1st.back().size);
        // Check if it fits before the end of the block.
        D3D12MA_ASSERT(offset + request.size <= GetSize());

        suballocations1st.push_back(newSuballoc);
        break;
    }
    case ALLOC_REQUEST_END_OF_2ND:
    {
        SuballocationVectorType& suballocations1st = AccessSuballocations1st();
        // New allocation at the end of 2-part ring buffer, so before first allocation from 1st vector.
        D3D12MA_ASSERT(!suballocations1st.empty() &&
            offset + request.size <= suballocations1st[m_1stNullItemsBeginCount].offset);
        SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

        switch (m_2ndVectorMode)
        {
        case SECOND_VECTOR_EMPTY:
            // First allocation from second part ring buffer.
            D3D12MA_ASSERT(suballocations2nd.empty());
            m_2ndVectorMode = SECOND_VECTOR_RING_BUFFER;
            break;
        case SECOND_VECTOR_RING_BUFFER:
            // 2-part ring buffer is already started.
            D3D12MA_ASSERT(!suballocations2nd.empty());
            break;
        case SECOND_VECTOR_DOUBLE_STACK:
            D3D12MA_ASSERT(0 && "CRITICAL ERROR: Trying to use linear allocator as ring buffer while it was already used as double stack.");
            break;
        default:
            D3D12MA_ASSERT(0);
        }

        suballocations2nd.push_back(newSuballoc);
        break;
    }
    default:
        D3D12MA_ASSERT(0 && "CRITICAL INTERNAL ERROR.");
    }
    m_SumFreeSize -= newSuballoc.size;
}

void BlockMetadata_Linear::Free(AllocHandle allocHandle)
{
    SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();
    UINT64 offset = (UINT64)allocHandle - 1;

    if (!suballocations1st.empty())
    {
        // First allocation: Mark it as next empty at the beginning.
        Suballocation& firstSuballoc = suballocations1st[m_1stNullItemsBeginCount];
        if (firstSuballoc.offset == offset)
        {
            firstSuballoc.type = SUBALLOCATION_TYPE_FREE;
            firstSuballoc.privateData = NULL;
            m_SumFreeSize += firstSuballoc.size;
            ++m_1stNullItemsBeginCount;
            CleanupAfterFree();
            return;
        }
    }

    // Last allocation in 2-part ring buffer or top of upper stack (same logic).
    if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER ||
        m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
    {
        Suballocation& lastSuballoc = suballocations2nd.back();
        if (lastSuballoc.offset == offset)
        {
            m_SumFreeSize += lastSuballoc.size;
            suballocations2nd.pop_back();
            CleanupAfterFree();
            return;
        }
    }
    // Last allocation in 1st vector.
    else if (m_2ndVectorMode == SECOND_VECTOR_EMPTY)
    {
        Suballocation& lastSuballoc = suballocations1st.back();
        if (lastSuballoc.offset == offset)
        {
            m_SumFreeSize += lastSuballoc.size;
            suballocations1st.pop_back();
            CleanupAfterFree();
            return;
        }
    }

    Suballocation refSuballoc;
    refSuballoc.offset = offset;
    // Rest of members stays uninitialized intentionally for better performance.

    // Item from the middle of 1st vector.
    {
        const SuballocationVectorType::iterator it = BinaryFindSorted(
            suballocations1st.begin() + m_1stNullItemsBeginCount,
            suballocations1st.end(),
            refSuballoc,
            SuballocationOffsetLess());
        if (it != suballocations1st.end())
        {
            it->type = SUBALLOCATION_TYPE_FREE;
            it->privateData = NULL;
            ++m_1stNullItemsMiddleCount;
            m_SumFreeSize += it->size;
            CleanupAfterFree();
            return;
        }
    }

    if (m_2ndVectorMode != SECOND_VECTOR_EMPTY)
    {
        // Item from the middle of 2nd vector.
        const SuballocationVectorType::iterator it = m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER ?
            BinaryFindSorted(suballocations2nd.begin(), suballocations2nd.end(), refSuballoc, SuballocationOffsetLess()) :
            BinaryFindSorted(suballocations2nd.begin(), suballocations2nd.end(), refSuballoc, SuballocationOffsetGreater());
        if (it != suballocations2nd.end())
        {
            it->type = SUBALLOCATION_TYPE_FREE;
            it->privateData = NULL;
            ++m_2ndNullItemsCount;
            m_SumFreeSize += it->size;
            CleanupAfterFree();
            return;
        }
    }

    D3D12MA_ASSERT(0 && "Allocation to free not found in linear allocator!");
}

void BlockMetadata_Linear::Clear()
{
    m_SumFreeSize = GetSize();
    m_Suballocations0.clear();
    m_Suballocations1.clear();
    // Leaving m_1stVectorIndex unchanged - it doesn't matter.
    m_2ndVectorMode = SECOND_VECTOR_EMPTY;
    m_1stNullItemsBeginCount = 0;
    m_1stNullItemsMiddleCount = 0;
    m_2ndNullItemsCount = 0;
}

AllocHandle BlockMetadata_Linear::GetAllocationListBegin() const
{
    // Function only used for defragmentation, which is disabled for this algorithm
    D3D12MA_ASSERT(0);
    return (AllocHandle)0;
}

AllocHandle BlockMetadata_Linear::GetNextAllocation(AllocHandle prevAlloc) const
{
    UNREF_PARAM(prevAlloc); 
    // Function only used for defragmentation, which is disabled for this algorithm
    D3D12MA_ASSERT(0);
    return (AllocHandle)0;
}

UINT64 BlockMetadata_Linear::GetNextFreeRegionSize(AllocHandle alloc) const
{
    UNREF_PARAM(alloc); 
    // Function only used for defragmentation, which is disabled for this algorithm
    D3D12MA_ASSERT(0);
    return 0;
}

void* BlockMetadata_Linear::GetAllocationPrivateData(AllocHandle allocHandle) const
{
    return FindSuballocation((UINT64)allocHandle - 1).privateData;
}

void BlockMetadata_Linear::SetAllocationPrivateData(AllocHandle allocHandle, void* privateData)
{
    Suballocation& suballoc = FindSuballocation((UINT64)allocHandle - 1);
    suballoc.privateData = privateData;
}

void BlockMetadata_Linear::AddStatistics(Statistics& inoutStats) const
{
    inoutStats.BlockCount++;
    inoutStats.AllocationCount += (UINT)GetAllocationCount();
    inoutStats.BlockBytes += GetSize();
    inoutStats.AllocationBytes += GetSize() - m_SumFreeSize;
}

void BlockMetadata_Linear::AddDetailedStatistics(DetailedStatistics& inoutStats) const
{
    inoutStats.Stats.BlockCount++;
    inoutStats.Stats.BlockBytes += GetSize();

    const UINT64 size = GetSize();
    const SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    const SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();
    const size_t suballoc1stCount = suballocations1st.size();
    const size_t suballoc2ndCount = suballocations2nd.size();

    UINT64 lastOffset = 0;
    if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
    {
        const UINT64 freeSpace2ndTo1stEnd = suballocations1st[m_1stNullItemsBeginCount].offset;
        size_t nextAlloc2ndIndex = 0;
        while (lastOffset < freeSpace2ndTo1stEnd)
        {
            // Find next non-null allocation or move nextAllocIndex to the end.
            while (nextAlloc2ndIndex < suballoc2ndCount &&
                suballocations2nd[nextAlloc2ndIndex].privateData == NULL)
            {
                ++nextAlloc2ndIndex;
            }

            // Found non-null allocation.
            if (nextAlloc2ndIndex < suballoc2ndCount)
            {
                const Suballocation& suballoc = suballocations2nd[nextAlloc2ndIndex];

                // 1. Process free space before this allocation.
                if (lastOffset < suballoc.offset)
                {
                    // There is free space from lastOffset to suballoc.offset.
                    const UINT64 unusedRangeSize = suballoc.offset - lastOffset;
                    AddDetailedStatisticsUnusedRange(inoutStats, unusedRangeSize);
                }

                // 2. Process this allocation.
                // There is allocation with suballoc.offset, suballoc.size.
                AddDetailedStatisticsAllocation(inoutStats, suballoc.size);

                // 3. Prepare for next iteration.
                lastOffset = suballoc.offset + suballoc.size;
                ++nextAlloc2ndIndex;
            }
            // We are at the end.
            else
            {
                // There is free space from lastOffset to freeSpace2ndTo1stEnd.
                if (lastOffset < freeSpace2ndTo1stEnd)
                {
                    const UINT64 unusedRangeSize = freeSpace2ndTo1stEnd - lastOffset;
                    AddDetailedStatisticsUnusedRange(inoutStats, unusedRangeSize);
                }

                // End of loop.
                lastOffset = freeSpace2ndTo1stEnd;
            }
        }
    }

    size_t nextAlloc1stIndex = m_1stNullItemsBeginCount;
    const UINT64 freeSpace1stTo2ndEnd =
        m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK ? suballocations2nd.back().offset : size;
    while (lastOffset < freeSpace1stTo2ndEnd)
    {
        // Find next non-null allocation or move nextAllocIndex to the end.
        while (nextAlloc1stIndex < suballoc1stCount &&
            suballocations1st[nextAlloc1stIndex].privateData == NULL)
        {
            ++nextAlloc1stIndex;
        }

        // Found non-null allocation.
        if (nextAlloc1stIndex < suballoc1stCount)
        {
            const Suballocation& suballoc = suballocations1st[nextAlloc1stIndex];

            // 1. Process free space before this allocation.
            if (lastOffset < suballoc.offset)
            {
                // There is free space from lastOffset to suballoc.offset.
                const UINT64 unusedRangeSize = suballoc.offset - lastOffset;
                AddDetailedStatisticsUnusedRange(inoutStats, unusedRangeSize);
            }

            // 2. Process this allocation.
            // There is allocation with suballoc.offset, suballoc.size.
            AddDetailedStatisticsAllocation(inoutStats, suballoc.size);

            // 3. Prepare for next iteration.
            lastOffset = suballoc.offset + suballoc.size;
            ++nextAlloc1stIndex;
        }
        // We are at the end.
        else
        {
            // There is free space from lastOffset to freeSpace1stTo2ndEnd.
            if (lastOffset < freeSpace1stTo2ndEnd)
            {
                const UINT64 unusedRangeSize = freeSpace1stTo2ndEnd - lastOffset;
                AddDetailedStatisticsUnusedRange(inoutStats, unusedRangeSize);
            }

            // End of loop.
            lastOffset = freeSpace1stTo2ndEnd;
        }
    }

    if (m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
    {
        size_t nextAlloc2ndIndex = suballocations2nd.size() - 1;
        while (lastOffset < size)
        {
            // Find next non-null allocation or move nextAllocIndex to the end.
            while (nextAlloc2ndIndex != SIZE_MAX &&
                suballocations2nd[nextAlloc2ndIndex].privateData == NULL)
            {
                --nextAlloc2ndIndex;
            }

            // Found non-null allocation.
            if (nextAlloc2ndIndex != SIZE_MAX)
            {
                const Suballocation& suballoc = suballocations2nd[nextAlloc2ndIndex];

                // 1. Process free space before this allocation.
                if (lastOffset < suballoc.offset)
                {
                    // There is free space from lastOffset to suballoc.offset.
                    const UINT64 unusedRangeSize = suballoc.offset - lastOffset;
                    AddDetailedStatisticsUnusedRange(inoutStats, unusedRangeSize);
                }

                // 2. Process this allocation.
                // There is allocation with suballoc.offset, suballoc.size.
                AddDetailedStatisticsAllocation(inoutStats, suballoc.size);

                // 3. Prepare for next iteration.
                lastOffset = suballoc.offset + suballoc.size;
                --nextAlloc2ndIndex;
            }
            // We are at the end.
            else
            {
                // There is free space from lastOffset to size.
                if (lastOffset < size)
                {
                    const UINT64 unusedRangeSize = size - lastOffset;
                    AddDetailedStatisticsUnusedRange(inoutStats, unusedRangeSize);
                }

                // End of loop.
                lastOffset = size;
            }
        }
    }
}

void BlockMetadata_Linear::WriteAllocationInfoToJson(JsonWriter& json) const
{
    const UINT64 size = GetSize();
    const SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    const SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();
    const size_t suballoc1stCount = suballocations1st.size();
    const size_t suballoc2ndCount = suballocations2nd.size();

    // FIRST PASS

    size_t unusedRangeCount = 0;
    UINT64 usedBytes = 0;

    UINT64 lastOffset = 0;

    size_t alloc2ndCount = 0;
    if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
    {
        const UINT64 freeSpace2ndTo1stEnd = suballocations1st[m_1stNullItemsBeginCount].offset;
        size_t nextAlloc2ndIndex = 0;
        while (lastOffset < freeSpace2ndTo1stEnd)
        {
            // Find next non-null allocation or move nextAlloc2ndIndex to the end.
            while (nextAlloc2ndIndex < suballoc2ndCount &&
                suballocations2nd[nextAlloc2ndIndex].privateData == NULL)
            {
                ++nextAlloc2ndIndex;
            }

            // Found non-null allocation.
            if (nextAlloc2ndIndex < suballoc2ndCount)
            {
                const Suballocation& suballoc = suballocations2nd[nextAlloc2ndIndex];

                // 1. Process free space before this allocation.
                if (lastOffset < suballoc.offset)
                {
                    // There is free space from lastOffset to suballoc.offset.
                    ++unusedRangeCount;
                }

                // 2. Process this allocation.
                // There is allocation with suballoc.offset, suballoc.size.
                ++alloc2ndCount;
                usedBytes += suballoc.size;

                // 3. Prepare for next iteration.
                lastOffset = suballoc.offset + suballoc.size;
                ++nextAlloc2ndIndex;
            }
            // We are at the end.
            else
            {
                if (lastOffset < freeSpace2ndTo1stEnd)
                {
                    // There is free space from lastOffset to freeSpace2ndTo1stEnd.
                    ++unusedRangeCount;
                }

                // End of loop.
                lastOffset = freeSpace2ndTo1stEnd;
            }
        }
    }

    size_t nextAlloc1stIndex = m_1stNullItemsBeginCount;
    size_t alloc1stCount = 0;
    const UINT64 freeSpace1stTo2ndEnd =
        m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK ? suballocations2nd.back().offset : size;
    while (lastOffset < freeSpace1stTo2ndEnd)
    {
        // Find next non-null allocation or move nextAllocIndex to the end.
        while (nextAlloc1stIndex < suballoc1stCount &&
            suballocations1st[nextAlloc1stIndex].privateData == NULL)
        {
            ++nextAlloc1stIndex;
        }

        // Found non-null allocation.
        if (nextAlloc1stIndex < suballoc1stCount)
        {
            const Suballocation& suballoc = suballocations1st[nextAlloc1stIndex];

            // 1. Process free space before this allocation.
            if (lastOffset < suballoc.offset)
            {
                // There is free space from lastOffset to suballoc.offset.
                ++unusedRangeCount;
            }

            // 2. Process this allocation.
            // There is allocation with suballoc.offset, suballoc.size.
            ++alloc1stCount;
            usedBytes += suballoc.size;

            // 3. Prepare for next iteration.
            lastOffset = suballoc.offset + suballoc.size;
            ++nextAlloc1stIndex;
        }
        // We are at the end.
        else
        {
            if (lastOffset < size)
            {
                // There is free space from lastOffset to freeSpace1stTo2ndEnd.
                ++unusedRangeCount;
            }

            // End of loop.
            lastOffset = freeSpace1stTo2ndEnd;
        }
    }

    if (m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
    {
        size_t nextAlloc2ndIndex = suballocations2nd.size() - 1;
        while (lastOffset < size)
        {
            // Find next non-null allocation or move nextAlloc2ndIndex to the end.
            while (nextAlloc2ndIndex != SIZE_MAX &&
                suballocations2nd[nextAlloc2ndIndex].privateData == NULL)
            {
                --nextAlloc2ndIndex;
            }

            // Found non-null allocation.
            if (nextAlloc2ndIndex != SIZE_MAX)
            {
                const Suballocation& suballoc = suballocations2nd[nextAlloc2ndIndex];

                // 1. Process free space before this allocation.
                if (lastOffset < suballoc.offset)
                {
                    // There is free space from lastOffset to suballoc.offset.
                    ++unusedRangeCount;
                }

                // 2. Process this allocation.
                // There is allocation with suballoc.offset, suballoc.size.
                ++alloc2ndCount;
                usedBytes += suballoc.size;

                // 3. Prepare for next iteration.
                lastOffset = suballoc.offset + suballoc.size;
                --nextAlloc2ndIndex;
            }
            // We are at the end.
            else
            {
                if (lastOffset < size)
                {
                    // There is free space from lastOffset to size.
                    ++unusedRangeCount;
                }

                // End of loop.
                lastOffset = size;
            }
        }
    }

    const UINT64 unusedBytes = size - usedBytes;
    PrintDetailedMap_Begin(json, unusedBytes, alloc1stCount + alloc2ndCount, unusedRangeCount);

    // SECOND PASS
    lastOffset = 0;
    if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
    {
        const UINT64 freeSpace2ndTo1stEnd = suballocations1st[m_1stNullItemsBeginCount].offset;
        size_t nextAlloc2ndIndex = 0;
        while (lastOffset < freeSpace2ndTo1stEnd)
        {
            // Find next non-null allocation or move nextAlloc2ndIndex to the end.
            while (nextAlloc2ndIndex < suballoc2ndCount &&
                suballocations2nd[nextAlloc2ndIndex].privateData == NULL)
            {
                ++nextAlloc2ndIndex;
            }

            // Found non-null allocation.
            if (nextAlloc2ndIndex < suballoc2ndCount)
            {
                const Suballocation& suballoc = suballocations2nd[nextAlloc2ndIndex];

                // 1. Process free space before this allocation.
                if (lastOffset < suballoc.offset)
                {
                    // There is free space from lastOffset to suballoc.offset.
                    const UINT64 unusedRangeSize = suballoc.offset - lastOffset;
                    PrintDetailedMap_UnusedRange(json, lastOffset, unusedRangeSize);
                }

                // 2. Process this allocation.
                // There is allocation with suballoc.offset, suballoc.size.
                PrintDetailedMap_Allocation(json, suballoc.offset, suballoc.size, suballoc.privateData);

                // 3. Prepare for next iteration.
                lastOffset = suballoc.offset + suballoc.size;
                ++nextAlloc2ndIndex;
            }
            // We are at the end.
            else
            {
                if (lastOffset < freeSpace2ndTo1stEnd)
                {
                    // There is free space from lastOffset to freeSpace2ndTo1stEnd.
                    const UINT64 unusedRangeSize = freeSpace2ndTo1stEnd - lastOffset;
                    PrintDetailedMap_UnusedRange(json, lastOffset, unusedRangeSize);
                }

                // End of loop.
                lastOffset = freeSpace2ndTo1stEnd;
            }
        }
    }

    nextAlloc1stIndex = m_1stNullItemsBeginCount;
    while (lastOffset < freeSpace1stTo2ndEnd)
    {
        // Find next non-null allocation or move nextAllocIndex to the end.
        while (nextAlloc1stIndex < suballoc1stCount &&
            suballocations1st[nextAlloc1stIndex].privateData == NULL)
        {
            ++nextAlloc1stIndex;
        }

        // Found non-null allocation.
        if (nextAlloc1stIndex < suballoc1stCount)
        {
            const Suballocation& suballoc = suballocations1st[nextAlloc1stIndex];

            // 1. Process free space before this allocation.
            if (lastOffset < suballoc.offset)
            {
                // There is free space from lastOffset to suballoc.offset.
                const UINT64 unusedRangeSize = suballoc.offset - lastOffset;
                PrintDetailedMap_UnusedRange(json, lastOffset, unusedRangeSize);
            }

            // 2. Process this allocation.
            // There is allocation with suballoc.offset, suballoc.size.
            PrintDetailedMap_Allocation(json, suballoc.offset, suballoc.size, suballoc.privateData);

            // 3. Prepare for next iteration.
            lastOffset = suballoc.offset + suballoc.size;
            ++nextAlloc1stIndex;
        }
        // We are at the end.
        else
        {
            if (lastOffset < freeSpace1stTo2ndEnd)
            {
                // There is free space from lastOffset to freeSpace1stTo2ndEnd.
                const UINT64 unusedRangeSize = freeSpace1stTo2ndEnd - lastOffset;
                PrintDetailedMap_UnusedRange(json, lastOffset, unusedRangeSize);
            }

            // End of loop.
            lastOffset = freeSpace1stTo2ndEnd;
        }
    }

    if (m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
    {
        size_t nextAlloc2ndIndex = suballocations2nd.size() - 1;
        while (lastOffset < size)
        {
            // Find next non-null allocation or move nextAlloc2ndIndex to the end.
            while (nextAlloc2ndIndex != SIZE_MAX &&
                suballocations2nd[nextAlloc2ndIndex].privateData == NULL)
            {
                --nextAlloc2ndIndex;
            }

            // Found non-null allocation.
            if (nextAlloc2ndIndex != SIZE_MAX)
            {
                const Suballocation& suballoc = suballocations2nd[nextAlloc2ndIndex];

                // 1. Process free space before this allocation.
                if (lastOffset < suballoc.offset)
                {
                    // There is free space from lastOffset to suballoc.offset.
                    const UINT64 unusedRangeSize = suballoc.offset - lastOffset;
                    PrintDetailedMap_UnusedRange(json, lastOffset, unusedRangeSize);
                }

                // 2. Process this allocation.
                // There is allocation with suballoc.offset, suballoc.size.
                PrintDetailedMap_Allocation(json, suballoc.offset, suballoc.size, suballoc.privateData);

                // 3. Prepare for next iteration.
                lastOffset = suballoc.offset + suballoc.size;
                --nextAlloc2ndIndex;
            }
            // We are at the end.
            else
            {
                if (lastOffset < size)
                {
                    // There is free space from lastOffset to size.
                    const UINT64 unusedRangeSize = size - lastOffset;
                    PrintDetailedMap_UnusedRange(json, lastOffset, unusedRangeSize);
                }

                // End of loop.
                lastOffset = size;
            }
        }
    }

    PrintDetailedMap_End(json);
}

Suballocation& BlockMetadata_Linear::FindSuballocation(UINT64 offset) const
{
    const SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    const SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

    Suballocation refSuballoc;
    refSuballoc.offset = offset;
    // Rest of members stays uninitialized intentionally for better performance.

    // Item from the 1st vector.
    {
        const SuballocationVectorType::iterator it = BinaryFindSorted(
            suballocations1st.cbegin() + m_1stNullItemsBeginCount,
            suballocations1st.cend(),
            refSuballoc,
            SuballocationOffsetLess());
        if (it != suballocations1st.cend())
        {
            return *it;
        }
    }

    if (m_2ndVectorMode != SECOND_VECTOR_EMPTY)
    {
        // Rest of members stays uninitialized intentionally for better performance.
        const SuballocationVectorType::iterator it = m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER ?
            BinaryFindSorted(suballocations2nd.cbegin(), suballocations2nd.cend(), refSuballoc, SuballocationOffsetLess()) :
            BinaryFindSorted(suballocations2nd.cbegin(), suballocations2nd.cend(), refSuballoc, SuballocationOffsetGreater());
        if (it != suballocations2nd.cend())
        {
            return *it;
        }
    }

    D3D12MA_ASSERT(0 && "Allocation not found in linear allocator!");
    return *suballocations1st.crbegin(); // Should never occur.
}

bool BlockMetadata_Linear::ShouldCompact1st() const
{
    const size_t nullItemCount = m_1stNullItemsBeginCount + m_1stNullItemsMiddleCount;
    const size_t suballocCount = AccessSuballocations1st().size();
    return suballocCount > 32 && nullItemCount * 2 >= (suballocCount - nullItemCount) * 3;
}

void BlockMetadata_Linear::CleanupAfterFree()
{
    SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

    if (IsEmpty())
    {
        suballocations1st.clear();
        suballocations2nd.clear();
        m_1stNullItemsBeginCount = 0;
        m_1stNullItemsMiddleCount = 0;
        m_2ndNullItemsCount = 0;
        m_2ndVectorMode = SECOND_VECTOR_EMPTY;
    }
    else
    {
        const size_t suballoc1stCount = suballocations1st.size();
        const size_t nullItem1stCount = m_1stNullItemsBeginCount + m_1stNullItemsMiddleCount;
        D3D12MA_ASSERT(nullItem1stCount <= suballoc1stCount);

        // Find more null items at the beginning of 1st vector.
        while (m_1stNullItemsBeginCount < suballoc1stCount &&
            suballocations1st[m_1stNullItemsBeginCount].type == SUBALLOCATION_TYPE_FREE)
        {
            ++m_1stNullItemsBeginCount;
            --m_1stNullItemsMiddleCount;
        }

        // Find more null items at the end of 1st vector.
        while (m_1stNullItemsMiddleCount > 0 &&
            suballocations1st.back().type == SUBALLOCATION_TYPE_FREE)
        {
            --m_1stNullItemsMiddleCount;
            suballocations1st.pop_back();
        }

        // Find more null items at the end of 2nd vector.
        while (m_2ndNullItemsCount > 0 &&
            suballocations2nd.back().type == SUBALLOCATION_TYPE_FREE)
        {
            --m_2ndNullItemsCount;
            suballocations2nd.pop_back();
        }

        // Find more null items at the beginning of 2nd vector.
        while (m_2ndNullItemsCount > 0 &&
            suballocations2nd[0].type == SUBALLOCATION_TYPE_FREE)
        {
            --m_2ndNullItemsCount;
            suballocations2nd.remove(0);
        }

        if (ShouldCompact1st())
        {
            const size_t nonNullItemCount = suballoc1stCount - nullItem1stCount;
            size_t srcIndex = m_1stNullItemsBeginCount;
            for (size_t dstIndex = 0; dstIndex < nonNullItemCount; ++dstIndex)
            {
                while (suballocations1st[srcIndex].type == SUBALLOCATION_TYPE_FREE)
                {
                    ++srcIndex;
                }
                if (dstIndex != srcIndex)
                {
                    suballocations1st[dstIndex] = suballocations1st[srcIndex];
                }
                ++srcIndex;
            }
            suballocations1st.resize(nonNullItemCount);
            m_1stNullItemsBeginCount = 0;
            m_1stNullItemsMiddleCount = 0;
        }

        // 2nd vector became empty.
        if (suballocations2nd.empty())
        {
            m_2ndVectorMode = SECOND_VECTOR_EMPTY;
        }

        // 1st vector became empty.
        if (suballocations1st.size() - m_1stNullItemsBeginCount == 0)
        {
            suballocations1st.clear();
            m_1stNullItemsBeginCount = 0;

            if (!suballocations2nd.empty() && m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
            {
                // Swap 1st with 2nd. Now 2nd is empty.
                m_2ndVectorMode = SECOND_VECTOR_EMPTY;
                m_1stNullItemsMiddleCount = m_2ndNullItemsCount;
                while (m_1stNullItemsBeginCount < suballocations2nd.size() &&
                    suballocations2nd[m_1stNullItemsBeginCount].type == SUBALLOCATION_TYPE_FREE)
                {
                    ++m_1stNullItemsBeginCount;
                    --m_1stNullItemsMiddleCount;
                }
                m_2ndNullItemsCount = 0;
                m_1stVectorIndex ^= 1;
            }
        }
    }

    D3D12MA_HEAVY_ASSERT(Validate());
}

bool BlockMetadata_Linear::CreateAllocationRequest_LowerAddress(
    UINT64 allocSize,
    UINT64 allocAlignment,
    AllocationRequest* pAllocationRequest)
{
    const UINT64 blockSize = GetSize();
    SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

    if (m_2ndVectorMode == SECOND_VECTOR_EMPTY || m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
    {
        // Try to allocate at the end of 1st vector.

        UINT64 resultBaseOffset = 0;
        if (!suballocations1st.empty())
        {
            const Suballocation& lastSuballoc = suballocations1st.back();
            resultBaseOffset = lastSuballoc.offset + lastSuballoc.size + GetDebugMargin();
        }

        // Start from offset equal to beginning of free space.
        UINT64 resultOffset = resultBaseOffset;
        // Apply alignment.
        resultOffset = AlignUp(resultOffset, allocAlignment);

        const UINT64 freeSpaceEnd = m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK ?
            suballocations2nd.back().offset : blockSize;

        // There is enough free space at the end after alignment.
        if (resultOffset + allocSize + GetDebugMargin() <= freeSpaceEnd)
        {
            // All tests passed: Success.
            pAllocationRequest->allocHandle = (AllocHandle)(resultOffset + 1);
            // pAllocationRequest->item, customData unused.
            pAllocationRequest->algorithmData = ALLOC_REQUEST_END_OF_1ST;
            return true;
        }
    }

    // Wrap-around to end of 2nd vector. Try to allocate there, watching for the
    // beginning of 1st vector as the end of free space.
    if (m_2ndVectorMode == SECOND_VECTOR_EMPTY || m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
    {
        D3D12MA_ASSERT(!suballocations1st.empty());

        UINT64 resultBaseOffset = 0;
        if (!suballocations2nd.empty())
        {
            const Suballocation& lastSuballoc = suballocations2nd.back();
            resultBaseOffset = lastSuballoc.offset + lastSuballoc.size + GetDebugMargin();
        }

        // Start from offset equal to beginning of free space.
        UINT64 resultOffset = resultBaseOffset;

        // Apply alignment.
        resultOffset = AlignUp(resultOffset, allocAlignment);

        size_t index1st = m_1stNullItemsBeginCount;
        // There is enough free space at the end after alignment.
        if ((index1st == suballocations1st.size() && resultOffset + allocSize + GetDebugMargin() <= blockSize) ||
            (index1st < suballocations1st.size() && resultOffset + allocSize + GetDebugMargin() <= suballocations1st[index1st].offset))
        {
            // All tests passed: Success.
            pAllocationRequest->allocHandle = (AllocHandle)(resultOffset + 1);
            pAllocationRequest->algorithmData = ALLOC_REQUEST_END_OF_2ND;
            // pAllocationRequest->item, customData unused.
            return true;
        }
    }
    return false;
}

bool BlockMetadata_Linear::CreateAllocationRequest_UpperAddress(
    UINT64 allocSize,
    UINT64 allocAlignment,
    AllocationRequest* pAllocationRequest)
{
    const UINT64 blockSize = GetSize();
    SuballocationVectorType& suballocations1st = AccessSuballocations1st();
    SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

    if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
    {
        D3D12MA_ASSERT(0 && "Trying to use pool with linear algorithm as double stack, while it is already being used as ring buffer.");
        return false;
    }

    // Try to allocate before 2nd.back(), or end of block if 2nd.empty().
    if (allocSize > blockSize)
    {
        return false;
    }
    UINT64 resultBaseOffset = blockSize - allocSize;
    if (!suballocations2nd.empty())
    {
        const Suballocation& lastSuballoc = suballocations2nd.back();
        resultBaseOffset = lastSuballoc.offset - allocSize;
        if (allocSize > lastSuballoc.offset)
        {
            return false;
        }
    }

    // Start from offset equal to end of free space.
    UINT64 resultOffset = resultBaseOffset;
    // Apply debugMargin at the end.
    if (GetDebugMargin() > 0)
    {
        if (resultOffset < GetDebugMargin())
        {
            return false;
        }
        resultOffset -= GetDebugMargin();
    }

    // Apply alignment.
    resultOffset = AlignDown(resultOffset, allocAlignment);
    // There is enough free space.
    const UINT64 endOf1st = !suballocations1st.empty() ?
        suballocations1st.back().offset + suballocations1st.back().size : 0;

    if (endOf1st + GetDebugMargin() <= resultOffset)
    {
        // All tests passed: Success.
        pAllocationRequest->allocHandle = (AllocHandle)(resultOffset + 1);
        // pAllocationRequest->item unused.
        pAllocationRequest->algorithmData = ALLOC_REQUEST_UPPER_ADDRESS;
        return true;
    }
    return false;
}
#endif // _D3D12MA_BLOCK_METADATA_LINEAR_FUNCTIONS
#endif // _D3D12MA_BLOCK_METADATA_LINEAR

#ifndef _D3D12MA_BLOCK_METADATA_TLSF
class BlockMetadata_TLSF : public BlockMetadata
{
public:
    BlockMetadata_TLSF(const ALLOCATION_CALLBACKS* allocationCallbacks, bool isVirtual);
    virtual ~BlockMetadata_TLSF();

    size_t GetAllocationCount() const override { return m_AllocCount; }
    size_t GetFreeRegionsCount() const override { return m_BlocksFreeCount + 1; }
    UINT64 GetSumFreeSize() const override { return m_BlocksFreeSize + m_NullBlock->size; }
    bool IsEmpty() const override { return m_NullBlock->offset == 0; }
    UINT64 GetAllocationOffset(AllocHandle allocHandle) const override { return ((Block*)allocHandle)->offset; };

    void Init(UINT64 size) override;
    bool Validate() const override;
    void GetAllocationInfo(AllocHandle allocHandle, VIRTUAL_ALLOCATION_INFO& outInfo) const override;

    bool CreateAllocationRequest(
        UINT64 allocSize,
        UINT64 allocAlignment,
        bool upperAddress,
        UINT32 strategy,
        AllocationRequest* pAllocationRequest) override;

    void Alloc(
        const AllocationRequest& request,
        UINT64 allocSize,
        void* privateData) override;

    void Free(AllocHandle allocHandle) override;
    void Clear() override;

    AllocHandle GetAllocationListBegin() const override;
    AllocHandle GetNextAllocation(AllocHandle prevAlloc) const override;
    UINT64 GetNextFreeRegionSize(AllocHandle alloc) const override;
    void* GetAllocationPrivateData(AllocHandle allocHandle) const override;
    void SetAllocationPrivateData(AllocHandle allocHandle, void* privateData) override;

    void AddStatistics(Statistics& inoutStats) const override;
    void AddDetailedStatistics(DetailedStatistics& inoutStats) const override;
    void WriteAllocationInfoToJson(JsonWriter& json) const override;

private:
    // According to original paper it should be preferable 4 or 5:
    // M. Masmano, I. Ripoll, A. Crespo, and J. Real "TLSF: a New Dynamic Memory Allocator for Real-Time Systems"
    // http://www.gii.upv.es/tlsf/files/ecrts04_tlsf.pdf
    static const UINT8 SECOND_LEVEL_INDEX = 5;
    static const UINT16 SMALL_BUFFER_SIZE = 256;
    static const UINT INITIAL_BLOCK_ALLOC_COUNT = 16;
    static const UINT8 MEMORY_CLASS_SHIFT = 7;
    static const UINT8 MAX_MEMORY_CLASSES = 65 - MEMORY_CLASS_SHIFT;

    class Block
    {
    public:
        UINT64 offset;
        UINT64 size;
        Block* prevPhysical;
        Block* nextPhysical;

        void MarkFree() { prevFree = NULL; }
        void MarkTaken() { prevFree = this; }
        bool IsFree() const { return prevFree != this; }
        void*& PrivateData() { D3D12MA_HEAVY_ASSERT(!IsFree()); return privateData; }
        Block*& PrevFree() { return prevFree; }
        Block*& NextFree() { D3D12MA_HEAVY_ASSERT(IsFree()); return nextFree; }

    private:
        Block* prevFree; // Address of the same block here indicates that block is taken
        union
        {
            Block* nextFree;
            void* privateData;
        };
    };
    
    size_t m_AllocCount = 0;
    // Total number of free blocks besides null block
    size_t m_BlocksFreeCount = 0;
    // Total size of free blocks excluding null block
    UINT64 m_BlocksFreeSize = 0;
    UINT32 m_IsFreeBitmap = 0;
    UINT8 m_MemoryClasses = 0;
    UINT32 m_InnerIsFreeBitmap[MAX_MEMORY_CLASSES];
    UINT32 m_ListsCount = 0;
    /*
    * 0: 0-3 lists for small buffers
    * 1+: 0-(2^SLI-1) lists for normal buffers
    */
    Block** m_FreeList = NULL;
    PoolAllocator<Block> m_BlockAllocator;
    Block* m_NullBlock = NULL;

    UINT8 SizeToMemoryClass(UINT64 size) const;
    UINT16 SizeToSecondIndex(UINT64 size, UINT8 memoryClass) const;
    UINT32 GetListIndex(UINT8 memoryClass, UINT16 secondIndex) const;
    UINT32 GetListIndex(UINT64 size) const;

    void RemoveFreeBlock(Block* block);
    void InsertFreeBlock(Block* block);
    void MergeBlock(Block* block, Block* prev);

    Block* FindFreeBlock(UINT64 size, UINT32& listIndex) const;
    bool CheckBlock(
        Block& block,
        UINT32 listIndex,
        UINT64 allocSize,
        UINT64 allocAlignment,
        AllocationRequest* pAllocationRequest);

    D3D12MA_CLASS_NO_COPY(BlockMetadata_TLSF)
};

#ifndef _D3D12MA_BLOCK_METADATA_TLSF_FUNCTIONS
BlockMetadata_TLSF::BlockMetadata_TLSF(const ALLOCATION_CALLBACKS* allocationCallbacks, bool isVirtual)
    : BlockMetadata(allocationCallbacks, isVirtual),
    m_BlockAllocator(*allocationCallbacks, INITIAL_BLOCK_ALLOC_COUNT)
{
    D3D12MA_ASSERT(allocationCallbacks);
}

BlockMetadata_TLSF::~BlockMetadata_TLSF()
{
    D3D12MA_DELETE_ARRAY(*GetAllocs(), m_FreeList, m_ListsCount);
}

void BlockMetadata_TLSF::Init(UINT64 size)
{
    BlockMetadata::Init(size);

    m_NullBlock = m_BlockAllocator.Alloc();
    m_NullBlock->size = size;
    m_NullBlock->offset = 0;
    m_NullBlock->prevPhysical = NULL;
    m_NullBlock->nextPhysical = NULL;
    m_NullBlock->MarkFree();
    m_NullBlock->NextFree() = NULL;
    m_NullBlock->PrevFree() = NULL;
    UINT8 memoryClass = SizeToMemoryClass(size);
    UINT16 sli = SizeToSecondIndex(size, memoryClass);
    m_ListsCount = (memoryClass == 0 ? 0 : (memoryClass - 1) * (1UL << SECOND_LEVEL_INDEX) + sli) + 1;
    if (IsVirtual())
        m_ListsCount += 1UL << SECOND_LEVEL_INDEX;
    else
        m_ListsCount += 4;

    m_MemoryClasses = memoryClass + 2;
    memset(m_InnerIsFreeBitmap, 0, MAX_MEMORY_CLASSES * sizeof(UINT32));

    m_FreeList = D3D12MA_NEW_ARRAY(*GetAllocs(), Block*, m_ListsCount);
    memset(m_FreeList, 0, m_ListsCount * sizeof(Block*));
}

bool BlockMetadata_TLSF::Validate() const
{
    D3D12MA_VALIDATE(GetSumFreeSize() <= GetSize());

    UINT64 calculatedSize = m_NullBlock->size;
    UINT64 calculatedFreeSize = m_NullBlock->size;
    size_t allocCount = 0;
    size_t freeCount = 0;

    // Check integrity of free lists
    for (UINT32 list = 0; list < m_ListsCount; ++list)
    {
        Block* block = m_FreeList[list];
        if (block != NULL)
        {
            D3D12MA_VALIDATE(block->IsFree());
            D3D12MA_VALIDATE(block->PrevFree() == NULL);
            while (block->NextFree())
            {
                D3D12MA_VALIDATE(block->NextFree()->IsFree());
                D3D12MA_VALIDATE(block->NextFree()->PrevFree() == block);
                block = block->NextFree();
            }
        }
    }

    D3D12MA_VALIDATE(m_NullBlock->nextPhysical == NULL);
    if (m_NullBlock->prevPhysical)
    {
        D3D12MA_VALIDATE(m_NullBlock->prevPhysical->nextPhysical == m_NullBlock);
    }

    // Check all blocks
    UINT64 nextOffset = m_NullBlock->offset;
    for (Block* prev = m_NullBlock->prevPhysical; prev != NULL; prev = prev->prevPhysical)
    {
        D3D12MA_VALIDATE(prev->offset + prev->size == nextOffset);
        nextOffset = prev->offset;
        calculatedSize += prev->size;

        UINT32 listIndex = GetListIndex(prev->size);
        if (prev->IsFree())
        {
            ++freeCount;
            // Check if free block belongs to free list
            Block* freeBlock = m_FreeList[listIndex];
            D3D12MA_VALIDATE(freeBlock != NULL);

            bool found = false;
            do
            {
                if (freeBlock == prev)
                    found = true;

                freeBlock = freeBlock->NextFree();
            } while (!found && freeBlock != NULL);

            D3D12MA_VALIDATE(found);
            calculatedFreeSize += prev->size;
        }
        else
        {
            ++allocCount;
            // Check if taken block is not on a free list
            Block* freeBlock = m_FreeList[listIndex];
            while (freeBlock)
            {
                D3D12MA_VALIDATE(freeBlock != prev);
                freeBlock = freeBlock->NextFree();
            }
        }

        if (prev->prevPhysical)
        {
            D3D12MA_VALIDATE(prev->prevPhysical->nextPhysical == prev);
        }
    }

    D3D12MA_VALIDATE(nextOffset == 0);
    D3D12MA_VALIDATE(calculatedSize == GetSize());
    D3D12MA_VALIDATE(calculatedFreeSize == GetSumFreeSize());
    D3D12MA_VALIDATE(allocCount == m_AllocCount);
    D3D12MA_VALIDATE(freeCount == m_BlocksFreeCount);

    return true;
}

void BlockMetadata_TLSF::GetAllocationInfo(AllocHandle allocHandle, VIRTUAL_ALLOCATION_INFO& outInfo) const
{
    Block* block = (Block*)allocHandle;
    D3D12MA_ASSERT(!block->IsFree() && "Cannot get allocation info for free block!");
    outInfo.Offset = block->offset;
    outInfo.Size = block->size;
    outInfo.pPrivateData = block->PrivateData();
}

bool BlockMetadata_TLSF::CreateAllocationRequest(
    UINT64 allocSize,
    UINT64 allocAlignment,
    bool upperAddress,
    UINT32 strategy,
    AllocationRequest* pAllocationRequest)
{
    D3D12MA_ASSERT(allocSize > 0 && "Cannot allocate empty block!");
    D3D12MA_ASSERT(!upperAddress && "ALLOCATION_FLAG_UPPER_ADDRESS can be used only with linear algorithm.");
    D3D12MA_ASSERT(pAllocationRequest != NULL);
    D3D12MA_HEAVY_ASSERT(Validate());

    allocSize += GetDebugMargin();
    // Quick check for too small pool
    if (allocSize > GetSumFreeSize())
        return false;

    // If no free blocks in pool then check only null block
    if (m_BlocksFreeCount == 0)
        return CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, pAllocationRequest);

    // Round up to the next block
    UINT64 sizeForNextList = allocSize;
    UINT64 smallSizeStep = SMALL_BUFFER_SIZE / (IsVirtual() ? 1 << SECOND_LEVEL_INDEX : 4);
    if (allocSize > SMALL_BUFFER_SIZE)
    {
        sizeForNextList += (1ULL << (BitScanMSB(allocSize) - SECOND_LEVEL_INDEX));
    }
    else if (allocSize > SMALL_BUFFER_SIZE - smallSizeStep)
        sizeForNextList = SMALL_BUFFER_SIZE + 1;
    else
        sizeForNextList += smallSizeStep;

    UINT32 nextListIndex = 0;
    UINT32 prevListIndex = 0;
    Block* nextListBlock = NULL;
    Block* prevListBlock = NULL;

    // Check blocks according to strategies
    if (strategy & ALLOCATION_FLAG_STRATEGY_MIN_TIME)
    {
        // Quick check for larger block first
        nextListBlock = FindFreeBlock(sizeForNextList, nextListIndex);
        if (nextListBlock != NULL && CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, pAllocationRequest))
            return true;

        // If not fitted then null block
        if (CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, pAllocationRequest))
            return true;

        // Null block failed, search larger bucket
        while (nextListBlock)
        {
            if (CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, pAllocationRequest))
                return true;
            nextListBlock = nextListBlock->NextFree();
        }

        // Failed again, check best fit bucket
        prevListBlock = FindFreeBlock(allocSize, prevListIndex);
        while (prevListBlock)
        {
            if (CheckBlock(*prevListBlock, prevListIndex, allocSize, allocAlignment, pAllocationRequest))
                return true;
            prevListBlock = prevListBlock->NextFree();
        }
    }
    else if (strategy & ALLOCATION_FLAG_STRATEGY_MIN_MEMORY)
    {
        // Check best fit bucket
        prevListBlock = FindFreeBlock(allocSize, prevListIndex);
        while (prevListBlock)
        {
            if (CheckBlock(*prevListBlock, prevListIndex, allocSize, allocAlignment, pAllocationRequest))
                return true;
            prevListBlock = prevListBlock->NextFree();
        }

        // If failed check null block
        if (CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, pAllocationRequest))
            return true;

        // Check larger bucket
        nextListBlock = FindFreeBlock(sizeForNextList, nextListIndex);
        while (nextListBlock)
        {
            if (CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, pAllocationRequest))
                return true;
            nextListBlock = nextListBlock->NextFree();
        }
    }
    else if (strategy & ALLOCATION_FLAG_STRATEGY_MIN_OFFSET)
    {
        // Perform search from the start
        Vector<Block*> blockList(m_BlocksFreeCount, *GetAllocs());

        size_t i = m_BlocksFreeCount;
        for (Block* block = m_NullBlock->prevPhysical; block != NULL; block = block->prevPhysical)
        {
            if (block->IsFree() && block->size >= allocSize)
                blockList[--i] = block;
        }

        for (; i < m_BlocksFreeCount; ++i)
        {
            Block& block = *blockList[i];
            if (CheckBlock(block, GetListIndex(block.size), allocSize, allocAlignment, pAllocationRequest))
                return true;
        }

        // If failed check null block
        if (CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, pAllocationRequest))
            return true;

        // Whole range searched, no more memory
        return false;
    }
    else
    {
        // Check larger bucket
        nextListBlock = FindFreeBlock(sizeForNextList, nextListIndex);
        while (nextListBlock)
        {
            if (CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, pAllocationRequest))
                return true;
            nextListBlock = nextListBlock->NextFree();
        }

        // If failed check null block
        if (CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, pAllocationRequest))
            return true;

        // Check best fit bucket
        prevListBlock = FindFreeBlock(allocSize, prevListIndex);
        while (prevListBlock)
        {
            if (CheckBlock(*prevListBlock, prevListIndex, allocSize, allocAlignment, pAllocationRequest))
                return true;
            prevListBlock = prevListBlock->NextFree();
        }
    }

    // Worst case, full search has to be done
    while (++nextListIndex < m_ListsCount)
    {
        nextListBlock = m_FreeList[nextListIndex];
        while (nextListBlock)
        {
            if (CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, pAllocationRequest))
                return true;
            nextListBlock = nextListBlock->NextFree();
        }
    }

    // No more memory sadly
    return false;
}

void BlockMetadata_TLSF::Alloc(
    const AllocationRequest& request,
    UINT64 allocSize,
    void* privateData)
{
    UNREF_PARAM(allocSize); 
    // Get block and pop it from the free list
    Block* currentBlock = (Block*)request.allocHandle;
    UINT64 offset = request.algorithmData;
    D3D12MA_ASSERT(currentBlock != NULL);
    D3D12MA_ASSERT(currentBlock->offset <= offset);

    if (currentBlock != m_NullBlock)
        RemoveFreeBlock(currentBlock);

    // Append missing alignment to prev block or create new one
    UINT64 misssingAlignment = offset - currentBlock->offset;
    if (misssingAlignment)
    {
        Block* prevBlock = currentBlock->prevPhysical;
        D3D12MA_ASSERT(prevBlock != NULL && "There should be no missing alignment at offset 0!");

        if (prevBlock->IsFree() && prevBlock->size != GetDebugMargin())
        {
            UINT32 oldList = GetListIndex(prevBlock->size);
            prevBlock->size += misssingAlignment;
            // Check if new size crosses list bucket
            if (oldList != GetListIndex(prevBlock->size))
            {
                prevBlock->size -= misssingAlignment;
                RemoveFreeBlock(prevBlock);
                prevBlock->size += misssingAlignment;
                InsertFreeBlock(prevBlock);
            }
            else
                m_BlocksFreeSize += misssingAlignment;
        }
        else
        {
            Block* newBlock = m_BlockAllocator.Alloc();
            currentBlock->prevPhysical = newBlock;
            prevBlock->nextPhysical = newBlock;
            newBlock->prevPhysical = prevBlock;
            newBlock->nextPhysical = currentBlock;
            newBlock->size = misssingAlignment;
            newBlock->offset = currentBlock->offset;
            newBlock->MarkTaken();

            InsertFreeBlock(newBlock);
        }

        currentBlock->size -= misssingAlignment;
        currentBlock->offset += misssingAlignment;
    }

    UINT64 size = request.size + GetDebugMargin();
    if (currentBlock->size == size)
    {
        if (currentBlock == m_NullBlock)
        {
            // Setup new null block
            m_NullBlock = m_BlockAllocator.Alloc();
            m_NullBlock->size = 0;
            m_NullBlock->offset = currentBlock->offset + size;
            m_NullBlock->prevPhysical = currentBlock;
            m_NullBlock->nextPhysical = NULL;
            m_NullBlock->MarkFree();
            m_NullBlock->PrevFree() = NULL;
            m_NullBlock->NextFree() = NULL;
            currentBlock->nextPhysical = m_NullBlock;
            currentBlock->MarkTaken();
        }
    }
    else
    {
        D3D12MA_ASSERT(currentBlock->size > size && "Proper block already found, shouldn't find smaller one!");

        // Create new free block
        Block* newBlock = m_BlockAllocator.Alloc();
        newBlock->size = currentBlock->size - size;
        newBlock->offset = currentBlock->offset + size;
        newBlock->prevPhysical = currentBlock;
        newBlock->nextPhysical = currentBlock->nextPhysical;
        currentBlock->nextPhysical = newBlock;
        currentBlock->size = size;

        if (currentBlock == m_NullBlock)
        {
            m_NullBlock = newBlock;
            m_NullBlock->MarkFree();
            m_NullBlock->NextFree() = NULL;
            m_NullBlock->PrevFree() = NULL;
            currentBlock->MarkTaken();
        }
        else
        {
            newBlock->nextPhysical->prevPhysical = newBlock;
            newBlock->MarkTaken();
            InsertFreeBlock(newBlock);
        }
    }
    currentBlock->PrivateData() = privateData;

    if (GetDebugMargin() > 0)
    {
        currentBlock->size -= GetDebugMargin();
        Block* newBlock = m_BlockAllocator.Alloc();
        newBlock->size = GetDebugMargin();
        newBlock->offset = currentBlock->offset + currentBlock->size;
        newBlock->prevPhysical = currentBlock;
        newBlock->nextPhysical = currentBlock->nextPhysical;
        newBlock->MarkTaken();
        currentBlock->nextPhysical->prevPhysical = newBlock;
        currentBlock->nextPhysical = newBlock;
        InsertFreeBlock(newBlock);
    }
    ++m_AllocCount;
}

void BlockMetadata_TLSF::Free(AllocHandle allocHandle)
{
    Block* block = (Block*)allocHandle;
    Block* next = block->nextPhysical;
    D3D12MA_ASSERT(!block->IsFree() && "Block is already free!");

    --m_AllocCount;
    if (GetDebugMargin() > 0)
    {
        RemoveFreeBlock(next);
        MergeBlock(next, block);
        block = next;
        next = next->nextPhysical;
    }

    // Try merging
    Block* prev = block->prevPhysical;
    if (prev != NULL && prev->IsFree() && prev->size != GetDebugMargin())
    {
        RemoveFreeBlock(prev);
        MergeBlock(block, prev);
    }

    if (!next->IsFree())
        InsertFreeBlock(block);
    else if (next == m_NullBlock)
        MergeBlock(m_NullBlock, block);
    else
    {
        RemoveFreeBlock(next);
        MergeBlock(next, block);
        InsertFreeBlock(next);
    }
}

void BlockMetadata_TLSF::Clear()
{
    m_AllocCount = 0;
    m_BlocksFreeCount = 0;
    m_BlocksFreeSize = 0;
    m_IsFreeBitmap = 0;
    m_NullBlock->offset = 0;
    m_NullBlock->size = GetSize();
    Block* block = m_NullBlock->prevPhysical;
    m_NullBlock->prevPhysical = NULL;
    while (block)
    {
        Block* prev = block->prevPhysical;
        m_BlockAllocator.Free(block);
        block = prev;
    }
    memset(m_FreeList, 0, m_ListsCount * sizeof(Block*));
    memset(m_InnerIsFreeBitmap, 0, m_MemoryClasses * sizeof(UINT32));
}

AllocHandle BlockMetadata_TLSF::GetAllocationListBegin() const
{
    if (m_AllocCount == 0)
        return (AllocHandle)0;

    for (Block* block = m_NullBlock->prevPhysical; block; block = block->prevPhysical)
    {
        if (!block->IsFree())
            return (AllocHandle)block;
    }
    D3D12MA_ASSERT(false && "If m_AllocCount > 0 then should find any allocation!");
    return (AllocHandle)0;
}

AllocHandle BlockMetadata_TLSF::GetNextAllocation(AllocHandle prevAlloc) const
{
    Block* startBlock = (Block*)prevAlloc;
    D3D12MA_ASSERT(!startBlock->IsFree() && "Incorrect block!");

    for (Block* block = startBlock->prevPhysical; block; block = block->prevPhysical)
    {
        if (!block->IsFree())
            return (AllocHandle)block;
    }
    return (AllocHandle)0;
}

UINT64 BlockMetadata_TLSF::GetNextFreeRegionSize(AllocHandle alloc) const
{
    Block* block = (Block*)alloc;
    D3D12MA_ASSERT(!block->IsFree() && "Incorrect block!");

    if (block->prevPhysical)
        return block->prevPhysical->IsFree() ? block->prevPhysical->size : 0;
    return 0;
}

void* BlockMetadata_TLSF::GetAllocationPrivateData(AllocHandle allocHandle) const
{
    Block* block = (Block*)allocHandle;
    D3D12MA_ASSERT(!block->IsFree() && "Cannot get user data for free block!");
    return block->PrivateData();
}

void BlockMetadata_TLSF::SetAllocationPrivateData(AllocHandle allocHandle, void* privateData)
{
    Block* block = (Block*)allocHandle;
    D3D12MA_ASSERT(!block->IsFree() && "Trying to set user data for not allocated block!");
    block->PrivateData() = privateData;
}

void BlockMetadata_TLSF::AddStatistics(Statistics& inoutStats) const
{
    inoutStats.BlockCount++;
    inoutStats.AllocationCount += static_cast<UINT>(m_AllocCount);
    inoutStats.BlockBytes += GetSize();
    inoutStats.AllocationBytes += GetSize() - GetSumFreeSize();
}

void BlockMetadata_TLSF::AddDetailedStatistics(DetailedStatistics& inoutStats) const
{
    inoutStats.Stats.BlockCount++;
    inoutStats.Stats.BlockBytes += GetSize();

    for (Block* block = m_NullBlock->prevPhysical; block != NULL; block = block->prevPhysical)
    {
        if (block->IsFree())
            AddDetailedStatisticsUnusedRange(inoutStats, block->size);
        else
            AddDetailedStatisticsAllocation(inoutStats, block->size);
    }

    if (m_NullBlock->size > 0)
        AddDetailedStatisticsUnusedRange(inoutStats, m_NullBlock->size);
}

void BlockMetadata_TLSF::WriteAllocationInfoToJson(JsonWriter& json) const
{
    size_t blockCount = m_AllocCount + m_BlocksFreeCount;
    Vector<Block*> blockList(blockCount, *GetAllocs());

    size_t i = blockCount;
    if (m_NullBlock->size > 0)
    {
        ++blockCount;
        blockList.push_back(m_NullBlock);
    }
    for (Block* block = m_NullBlock->prevPhysical; block != NULL; block = block->prevPhysical)
    {
        blockList[--i] = block;
    }
    D3D12MA_ASSERT(i == 0);

    PrintDetailedMap_Begin(json, GetSumFreeSize(), GetAllocationCount(), m_BlocksFreeCount + static_cast<bool>(m_NullBlock->size));
    for (; i < blockCount; ++i)
    {
        Block* block = blockList[i];
        if (block->IsFree())
            PrintDetailedMap_UnusedRange(json, block->offset, block->size);
        else
            PrintDetailedMap_Allocation(json, block->offset, block->size, block->PrivateData());
    }
    PrintDetailedMap_End(json);
}

UINT8 BlockMetadata_TLSF::SizeToMemoryClass(UINT64 size) const
{
    if (size > SMALL_BUFFER_SIZE)
        return BitScanMSB(size) - MEMORY_CLASS_SHIFT;
    return 0;
}

UINT16 BlockMetadata_TLSF::SizeToSecondIndex(UINT64 size, UINT8 memoryClass) const
{
    if (memoryClass == 0)
    {
        if (IsVirtual())
            return static_cast<UINT16>((size - 1) / 8);
        else
            return static_cast<UINT16>((size - 1) / 64);
    }
    return static_cast<UINT16>((size >> (memoryClass + MEMORY_CLASS_SHIFT - SECOND_LEVEL_INDEX)) ^ (1U << SECOND_LEVEL_INDEX));
}

UINT32 BlockMetadata_TLSF::GetListIndex(UINT8 memoryClass, UINT16 secondIndex) const
{
    if (memoryClass == 0)
        return secondIndex;

    const UINT32 index = static_cast<UINT32>(memoryClass - 1) * (1 << SECOND_LEVEL_INDEX) + secondIndex;
    if (IsVirtual())
        return index + (1 << SECOND_LEVEL_INDEX);
    else
        return index + 4;
}

UINT32 BlockMetadata_TLSF::GetListIndex(UINT64 size) const
{
    UINT8 memoryClass = SizeToMemoryClass(size);
    return GetListIndex(memoryClass, SizeToSecondIndex(size, memoryClass));
}

void BlockMetadata_TLSF::RemoveFreeBlock(Block* block)
{
    D3D12MA_ASSERT(block != m_NullBlock);
    D3D12MA_ASSERT(block->IsFree());

    if (block->NextFree() != NULL)
        block->NextFree()->PrevFree() = block->PrevFree();
    if (block->PrevFree() != NULL)
        block->PrevFree()->NextFree() = block->NextFree();
    else
    {
        UINT8 memClass = SizeToMemoryClass(block->size);
        UINT16 secondIndex = SizeToSecondIndex(block->size, memClass);
        UINT32 index = GetListIndex(memClass, secondIndex);
        m_FreeList[index] = block->NextFree();
        if (block->NextFree() == NULL)
        {
            m_InnerIsFreeBitmap[memClass] &= ~(1U << secondIndex);
            if (m_InnerIsFreeBitmap[memClass] == 0)
                m_IsFreeBitmap &= ~(1UL << memClass);
        }
    }
    block->MarkTaken();
    block->PrivateData() = NULL;
    --m_BlocksFreeCount;
    m_BlocksFreeSize -= block->size;
}

void BlockMetadata_TLSF::InsertFreeBlock(Block* block)
{
    D3D12MA_ASSERT(block != m_NullBlock);
    D3D12MA_ASSERT(!block->IsFree() && "Cannot insert block twice!");

    UINT8 memClass = SizeToMemoryClass(block->size);
    UINT16 secondIndex = SizeToSecondIndex(block->size, memClass);
    UINT32 index = GetListIndex(memClass, secondIndex);
    block->PrevFree() = NULL;
    block->NextFree() = m_FreeList[index];
    m_FreeList[index] = block;
    if (block->NextFree() != NULL)
        block->NextFree()->PrevFree() = block;
    else
    {
        m_InnerIsFreeBitmap[memClass] |= 1U << secondIndex;
        m_IsFreeBitmap |= 1UL << memClass;
    }
    ++m_BlocksFreeCount;
    m_BlocksFreeSize += block->size;
}

void BlockMetadata_TLSF::MergeBlock(Block* block, Block* prev)
{
    D3D12MA_ASSERT(block->prevPhysical == prev && "Cannot merge seperate physical regions!");
    D3D12MA_ASSERT(!prev->IsFree() && "Cannot merge block that belongs to free list!");

    block->offset = prev->offset;
    block->size += prev->size;
    block->prevPhysical = prev->prevPhysical;
    if (block->prevPhysical)
        block->prevPhysical->nextPhysical = block;
    m_BlockAllocator.Free(prev);
}

BlockMetadata_TLSF::Block* BlockMetadata_TLSF::FindFreeBlock(UINT64 size, UINT32& listIndex) const
{
    UINT8 memoryClass = SizeToMemoryClass(size);
    UINT32 innerFreeMap = m_InnerIsFreeBitmap[memoryClass] & (~0U << SizeToSecondIndex(size, memoryClass));
    if (!innerFreeMap)
    {
        // Check higher levels for avaiable blocks
        UINT32 freeMap = m_IsFreeBitmap & (~0UL << (memoryClass + 1));
        if (!freeMap)
            return NULL; // No more memory avaible

        // Find lowest free region
        memoryClass = BitScanLSB(freeMap);
        innerFreeMap = m_InnerIsFreeBitmap[memoryClass];
        D3D12MA_ASSERT(innerFreeMap != 0);
    }
    // Find lowest free subregion
    listIndex = GetListIndex(memoryClass, BitScanLSB(innerFreeMap));
    return m_FreeList[listIndex];
}

bool BlockMetadata_TLSF::CheckBlock(
    Block& block,
    UINT32 listIndex,
    UINT64 allocSize,
    UINT64 allocAlignment,
    AllocationRequest* pAllocationRequest)
{
    D3D12MA_ASSERT(block.IsFree() && "Block is already taken!");

    UINT64 alignedOffset = AlignUp(block.offset, allocAlignment);
    if (block.size < allocSize + alignedOffset - block.offset)
        return false;

    // Alloc successful
    pAllocationRequest->allocHandle = (AllocHandle)&block;
    pAllocationRequest->size = allocSize - GetDebugMargin();
    pAllocationRequest->algorithmData = alignedOffset;

    // Place block at the start of list if it's normal block
    if (listIndex != m_ListsCount && block.PrevFree())
    {
        block.PrevFree()->NextFree() = block.NextFree();
        if (block.NextFree())
            block.NextFree()->PrevFree() = block.PrevFree();
        block.PrevFree() = NULL;
        block.NextFree() = m_FreeList[listIndex];
        m_FreeList[listIndex] = &block;
        if (block.NextFree())
            block.NextFree()->PrevFree() = &block;
    }

    return true;
}
#endif // _D3D12MA_BLOCK_METADATA_TLSF_FUNCTIONS
#endif // _D3D12MA_BLOCK_METADATA_TLSF

#ifndef _D3D12MA_MEMORY_BLOCK
/*
Represents a single block of device memory (heap).
Base class for inheritance.
Thread-safety: This class must be externally synchronized.
*/
class MemoryBlock
{
public:
    // Creates the ID3D12Heap.
    MemoryBlock(
        AllocatorPimpl* allocator,
        const D3D12_HEAP_PROPERTIES& heapProps,
        D3D12_HEAP_FLAGS heapFlags,
        UINT64 size,
        UINT id);
    virtual ~MemoryBlock();

    const D3D12_HEAP_PROPERTIES& GetHeapProperties() const { return m_HeapProps; }
    D3D12_HEAP_FLAGS GetHeapFlags() const { return m_HeapFlags; }
    UINT64 GetSize() const { return m_Size; }
    UINT GetId() const { return m_Id; }
    ID3D12Heap* GetHeap() const { return m_Heap; }

protected:
    AllocatorPimpl* const m_Allocator;
    const D3D12_HEAP_PROPERTIES m_HeapProps;
    const D3D12_HEAP_FLAGS m_HeapFlags;
    const UINT64 m_Size;
    const UINT m_Id;

    HRESULT Init(ID3D12ProtectedResourceSession* pProtectedSession, bool denyMsaaTextures);

private:
    ID3D12Heap* m_Heap = NULL;

    D3D12MA_CLASS_NO_COPY(MemoryBlock)
};
#endif // _D3D12MA_MEMORY_BLOCK

#ifndef _D3D12MA_NORMAL_BLOCK
/*
Represents a single block of device memory (heap) with all the data about its
regions (aka suballocations, Allocation), assigned and free.
Thread-safety: This class must be externally synchronized.
*/
class NormalBlock : public MemoryBlock
{
public:
    BlockMetadata* m_pMetadata;

    NormalBlock(
        AllocatorPimpl* allocator,
        BlockVector* blockVector,
        const D3D12_HEAP_PROPERTIES& heapProps,
        D3D12_HEAP_FLAGS heapFlags,
        UINT64 size,
        UINT id);
    virtual ~NormalBlock();

    BlockVector* GetBlockVector() const { return m_BlockVector; }

    // 'algorithm' should be one of the *_ALGORITHM_* flags in enums POOL_FLAGS or VIRTUAL_BLOCK_FLAGS
    HRESULT Init(UINT32 algorithm, ID3D12ProtectedResourceSession* pProtectedSession, bool denyMsaaTextures);

    // Validates all data structures inside this object. If not valid, returns false.
    bool Validate() const;

private:
    BlockVector* m_BlockVector;

    D3D12MA_CLASS_NO_COPY(NormalBlock)
};
#endif // _D3D12MA_NORMAL_BLOCK

#ifndef _D3D12MA_COMMITTED_ALLOCATION_LIST_ITEM_TRAITS
struct CommittedAllocationListItemTraits
{
    using ItemType = Allocation;

    static ItemType* GetPrev(const ItemType* item)
    {
        D3D12MA_ASSERT(item->m_PackedData.GetType() == Allocation::TYPE_COMMITTED || item->m_PackedData.GetType() == Allocation::TYPE_HEAP);
        return item->m_Committed.prev;
    }
    static ItemType* GetNext(const ItemType* item)
    {
        D3D12MA_ASSERT(item->m_PackedData.GetType() == Allocation::TYPE_COMMITTED || item->m_PackedData.GetType() == Allocation::TYPE_HEAP);
        return item->m_Committed.next;
    }
    static ItemType*& AccessPrev(ItemType* item)
    {
        D3D12MA_ASSERT(item->m_PackedData.GetType() == Allocation::TYPE_COMMITTED || item->m_PackedData.GetType() == Allocation::TYPE_HEAP);
        return item->m_Committed.prev;
    }
    static ItemType*& AccessNext(ItemType* item)
    {
        D3D12MA_ASSERT(item->m_PackedData.GetType() == Allocation::TYPE_COMMITTED || item->m_PackedData.GetType() == Allocation::TYPE_HEAP);
        return item->m_Committed.next;
    }
};
#endif // _D3D12MA_COMMITTED_ALLOCATION_LIST_ITEM_TRAITS

#ifndef _D3D12MA_COMMITTED_ALLOCATION_LIST
/*
Stores linked list of Allocation objects that are of TYPE_COMMITTED or TYPE_HEAP.
Thread-safe, synchronized internally.
*/
class CommittedAllocationList
{
public:
    CommittedAllocationList() = default;
    void Init(bool useMutex, D3D12_HEAP_TYPE heapType, PoolPimpl* pool);
    ~CommittedAllocationList();

    D3D12_HEAP_TYPE GetHeapType() const { return m_HeapType; }
    PoolPimpl* GetPool() const { return m_Pool; }
    UINT GetMemorySegmentGroup(AllocatorPimpl* allocator) const;
    
    void AddStatistics(Statistics& inoutStats);
    void AddDetailedStatistics(DetailedStatistics& inoutStats);
    // Writes JSON array with the list of allocations.
    void BuildStatsString(JsonWriter& json);

    void Register(Allocation* alloc);
    void Unregister(Allocation* alloc);

private:
    using CommittedAllocationLinkedList = IntrusiveLinkedList<CommittedAllocationListItemTraits>;

    bool m_UseMutex = true;
    D3D12_HEAP_TYPE m_HeapType = D3D12_HEAP_TYPE_CUSTOM;
    PoolPimpl* m_Pool = NULL;

    D3D12MA_RW_MUTEX m_Mutex;
    CommittedAllocationLinkedList m_AllocationList;
};
#endif // _D3D12MA_COMMITTED_ALLOCATION_LIST

#ifndef _D3D12M_COMMITTED_ALLOCATION_PARAMETERS
struct CommittedAllocationParameters
{
    CommittedAllocationList* m_List = NULL;
    D3D12_HEAP_PROPERTIES m_HeapProperties = {};
    D3D12_HEAP_FLAGS m_HeapFlags = D3D12_HEAP_FLAG_NONE;
    ID3D12ProtectedResourceSession* m_ProtectedSession = NULL;
    bool m_CanAlias = false;

    bool IsValid() const { return m_List != NULL; }
};
#endif // _D3D12M_COMMITTED_ALLOCATION_PARAMETERS

#ifndef _D3D12MA_BLOCK_VECTOR
/*
Sequence of NormalBlock. Represents memory blocks allocated for a specific
heap type and possibly resource type (if only Tier 1 is supported).

Synchronized internally with a mutex.
*/
class BlockVector
{
    friend class DefragmentationContextPimpl;
    D3D12MA_CLASS_NO_COPY(BlockVector)
public:
    BlockVector(
        AllocatorPimpl* hAllocator,
        const D3D12_HEAP_PROPERTIES& heapProps,
        D3D12_HEAP_FLAGS heapFlags,
        UINT64 preferredBlockSize,
        size_t minBlockCount,
        size_t maxBlockCount,
        bool explicitBlockSize,
        UINT64 minAllocationAlignment,
        UINT32 algorithm,
        bool denyMsaaTextures,
        ID3D12ProtectedResourceSession* pProtectedSession);
    ~BlockVector();

    const D3D12_HEAP_PROPERTIES& GetHeapProperties() const { return m_HeapProps; }
    D3D12_HEAP_FLAGS GetHeapFlags() const { return m_HeapFlags; }
    UINT64 GetPreferredBlockSize() const { return m_PreferredBlockSize; }
    UINT32 GetAlgorithm() const { return m_Algorithm; }
    bool DeniesMsaaTextures() const { return m_DenyMsaaTextures; }
    // To be used only while the m_Mutex is locked. Used during defragmentation.
    size_t GetBlockCount() const { return m_Blocks.size(); }
    // To be used only while the m_Mutex is locked. Used during defragmentation.
    NormalBlock* GetBlock(size_t index) const { return m_Blocks[index]; }
    D3D12MA_RW_MUTEX& GetMutex() { return m_Mutex; }

    HRESULT CreateMinBlocks();
    bool IsEmpty();

    HRESULT Allocate(
        UINT64 size,
        UINT64 alignment,
        const ALLOCATION_DESC& allocDesc,
        size_t allocationCount,
        Allocation** pAllocations);

    void Free(Allocation* hAllocation);

    HRESULT CreateResource(
        UINT64 size,
        UINT64 alignment,
        const ALLOCATION_DESC& allocDesc,
        const D3D12_RESOURCE_DESC& resourceDesc,
        D3D12_RESOURCE_STATES InitialResourceState,
        const D3D12_CLEAR_VALUE *pOptimizedClearValue,
        Allocation** ppAllocation,
        REFIID riidResource,
        void** ppvResource);

#ifdef __ID3D12Device8_INTERFACE_DEFINED__
    HRESULT CreateResource2(
        UINT64 size,
        UINT64 alignment,
        const ALLOCATION_DESC& allocDesc,
        const D3D12_RESOURCE_DESC1& resourceDesc,
        D3D12_RESOURCE_STATES InitialResourceState,
        const D3D12_CLEAR_VALUE *pOptimizedClearValue,
        Allocation** ppAllocation,
        REFIID riidResource,
        void** ppvResource);
#endif // #ifdef __ID3D12Device8_INTERFACE_DEFINED__

    void AddStatistics(Statistics& inoutStats);
    void AddDetailedStatistics(DetailedStatistics& inoutStats);

    void WriteBlockInfoToJson(JsonWriter& json);

private:
    AllocatorPimpl* const m_hAllocator;
    const D3D12_HEAP_PROPERTIES m_HeapProps;
    const D3D12_HEAP_FLAGS m_HeapFlags;
    const UINT64 m_PreferredBlockSize;
    const size_t m_MinBlockCount;
    const size_t m_MaxBlockCount;
    const bool m_ExplicitBlockSize;
    const UINT64 m_MinAllocationAlignment;
    const UINT32 m_Algorithm;
    const bool m_DenyMsaaTextures;
    ID3D12ProtectedResourceSession* const m_ProtectedSession;
    /* There can be at most one allocation that is completely empty - a
    hysteresis to avoid pessimistic case of alternating creation and destruction
    of a ID3D12Heap. */
    bool m_HasEmptyBlock;
    D3D12MA_RW_MUTEX m_Mutex;
    // Incrementally sorted by sumFreeSize, ascending.
    Vector<NormalBlock*> m_Blocks;
    UINT m_NextBlockId;
    bool m_IncrementalSort = true;

    // Disable incremental sorting when freeing allocations
    void SetIncrementalSort(bool val) { m_IncrementalSort = val; }

    UINT64 CalcSumBlockSize() const;
    UINT64 CalcMaxBlockSize() const;

    // Finds and removes given block from vector.
    void Remove(NormalBlock* pBlock);

    // Performs single step in sorting m_Blocks. They may not be fully sorted
    // after this call.
    void IncrementallySortBlocks();
    void SortByFreeSize();

    HRESULT AllocatePage(
        UINT64 size,
        UINT64 alignment,
        const ALLOCATION_DESC& allocDesc,
        Allocation** pAllocation);

    HRESULT AllocateFromBlock(
        NormalBlock* pBlock,
        UINT64 size,
        UINT64 alignment,
        ALLOCATION_FLAGS allocFlags,
        // CONFFX_BEGIN - Linked multi gpu
        const ALLOCATION_DESC& allocDesc,
        // CONFFX_END
        void* pPrivateData,
        UINT32 strategy,
        Allocation** pAllocation);

    HRESULT CommitAllocationRequest(
        AllocationRequest& allocRequest,
        NormalBlock* pBlock,
        UINT64 size,
        UINT64 alignment,
        // CONFFX_BEGIN - Linked multi gpu
        const ALLOCATION_DESC& allocDesc,
        // CONFFX_END
        void* pPrivateData,
        Allocation** pAllocation);

    HRESULT CreateBlock(
        UINT64 blockSize,
        // CONFFX_BEGIN - Linked multi gpu
        UINT creationNodeMask, UINT visibleNodeMask,
        // CONFFX_END
        size_t* pNewBlockIndex);
};
#endif // _D3D12MA_BLOCK_VECTOR

#ifndef _D3D12MA_CURRENT_BUDGET_DATA
class CurrentBudgetData
{
public:
    bool ShouldUpdateBudget() const { return m_OperationsSinceBudgetFetch >= 30; }

    void GetStatistics(Statistics& outStats, UINT group) const;
    void GetBudget(bool useMutex,
        UINT64* outLocalUsage, UINT64* outLocalBudget,
        UINT64* outNonLocalUsage, UINT64* outNonLocalBudget);

#if D3D12MA_DXGI_1_4
    HRESULT UpdateBudget(IDXGIAdapter3* adapter3, bool useMutex);
#endif

    void AddAllocation(UINT group, UINT64 allocationBytes);
    void RemoveAllocation(UINT group, UINT64 allocationBytes);

    void AddBlock(UINT group, UINT64 blockBytes);
    void RemoveBlock(UINT group, UINT64 blockBytes);

private:
    D3D12MA_ATOMIC_UINT32 m_BlockCount[DXGI_MEMORY_SEGMENT_GROUP_COUNT] = {};
    D3D12MA_ATOMIC_UINT32 m_AllocationCount[DXGI_MEMORY_SEGMENT_GROUP_COUNT] = {};
    D3D12MA_ATOMIC_UINT64 m_BlockBytes[DXGI_MEMORY_SEGMENT_GROUP_COUNT] = {};
    D3D12MA_ATOMIC_UINT64 m_AllocationBytes[DXGI_MEMORY_SEGMENT_GROUP_COUNT] = {};

    D3D12MA_ATOMIC_UINT32 m_OperationsSinceBudgetFetch = 0;
    D3D12MA_RW_MUTEX m_BudgetMutex;
    UINT64 m_D3D12Usage[DXGI_MEMORY_SEGMENT_GROUP_COUNT] = {};
    UINT64 m_D3D12Budget[DXGI_MEMORY_SEGMENT_GROUP_COUNT] = {};
    UINT64 m_BlockBytesAtD3D12Fetch[DXGI_MEMORY_SEGMENT_GROUP_COUNT] = {};
};

#ifndef _D3D12MA_CURRENT_BUDGET_DATA_FUNCTIONS
void CurrentBudgetData::GetStatistics(Statistics& outStats, UINT group) const
{
    outStats.BlockCount = m_BlockCount[group];
    outStats.AllocationCount = m_AllocationCount[group];
    outStats.BlockBytes = m_BlockBytes[group];
    outStats.AllocationBytes = m_AllocationBytes[group];
}

void CurrentBudgetData::GetBudget(bool useMutex,
    UINT64* outLocalUsage, UINT64* outLocalBudget,
    UINT64* outNonLocalUsage, UINT64* outNonLocalBudget)
{
    MutexLockRead lockRead(m_BudgetMutex, useMutex);

    if (outLocalUsage)
    {
        const UINT64 D3D12Usage = m_D3D12Usage[DXGI_MEMORY_SEGMENT_GROUP_LOCAL_COPY];
        const UINT64 blockBytes = m_BlockBytes[DXGI_MEMORY_SEGMENT_GROUP_LOCAL_COPY];
        const UINT64 blockBytesAtD3D12Fetch = m_BlockBytesAtD3D12Fetch[DXGI_MEMORY_SEGMENT_GROUP_LOCAL_COPY];
        *outLocalUsage = D3D12Usage + blockBytes > blockBytesAtD3D12Fetch ?
            D3D12Usage + blockBytes - blockBytesAtD3D12Fetch : 0;
    }
    if (outLocalBudget)
        *outLocalBudget = m_D3D12Budget[DXGI_MEMORY_SEGMENT_GROUP_LOCAL_COPY];

    if (outNonLocalUsage)
    {
        const UINT64 D3D12Usage = m_D3D12Usage[DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL_COPY];
        const UINT64 blockBytes = m_BlockBytes[DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL_COPY];
        const UINT64 blockBytesAtD3D12Fetch = m_BlockBytesAtD3D12Fetch[DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL_COPY];
        *outNonLocalUsage = D3D12Usage + blockBytes > blockBytesAtD3D12Fetch ?
            D3D12Usage + blockBytes - blockBytesAtD3D12Fetch : 0;
    }
    if (outNonLocalBudget)
        *outNonLocalBudget = m_D3D12Budget[DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL_COPY];
}

#if D3D12MA_DXGI_1_4
HRESULT CurrentBudgetData::UpdateBudget(IDXGIAdapter3* adapter3, bool useMutex)
{
    D3D12MA_ASSERT(adapter3);

    DXGI_QUERY_VIDEO_MEMORY_INFO infoLocal = {};
    DXGI_QUERY_VIDEO_MEMORY_INFO infoNonLocal = {};
    const HRESULT hrLocal = adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &infoLocal);
    const HRESULT hrNonLocal = adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &infoNonLocal);

    if (SUCCEEDED(hrLocal) || SUCCEEDED(hrNonLocal))
    {
        MutexLockWrite lockWrite(m_BudgetMutex, useMutex);

        if (SUCCEEDED(hrLocal))
        {
            m_D3D12Usage[0] = infoLocal.CurrentUsage;
            m_D3D12Budget[0] = infoLocal.Budget;
        }
        if (SUCCEEDED(hrNonLocal))
        {
            m_D3D12Usage[1] = infoNonLocal.CurrentUsage;
            m_D3D12Budget[1] = infoNonLocal.Budget;
        }

        m_BlockBytesAtD3D12Fetch[0] = m_BlockBytes[0];
        m_BlockBytesAtD3D12Fetch[1] = m_BlockBytes[1];
        m_OperationsSinceBudgetFetch = 0;
    }

    return FAILED(hrLocal) ? hrLocal : hrNonLocal;
}
#endif // #if D3D12MA_DXGI_1_4

void CurrentBudgetData::AddAllocation(UINT group, UINT64 allocationBytes)
{
    ++m_AllocationCount[group];
    m_AllocationBytes[group] += allocationBytes;
    ++m_OperationsSinceBudgetFetch;
}

void CurrentBudgetData::RemoveAllocation(UINT group, UINT64 allocationBytes)
{
    D3D12MA_ASSERT(m_AllocationBytes[group] >= allocationBytes);
    D3D12MA_ASSERT(m_AllocationCount[group] > 0);
    m_AllocationBytes[group] -= allocationBytes;
    --m_AllocationCount[group];
    ++m_OperationsSinceBudgetFetch;
}

void CurrentBudgetData::AddBlock(UINT group, UINT64 blockBytes)
{
    ++m_BlockCount[group];
    m_BlockBytes[group] += blockBytes;
    ++m_OperationsSinceBudgetFetch;
}

void CurrentBudgetData::RemoveBlock(UINT group, UINT64 blockBytes)
{
    D3D12MA_ASSERT(m_BlockBytes[group] >= blockBytes);
    D3D12MA_ASSERT(m_BlockCount[group] > 0);
    m_BlockBytes[group] -= blockBytes;
    --m_BlockCount[group];
    ++m_OperationsSinceBudgetFetch;
}
#endif // _D3D12MA_CURRENT_BUDGET_DATA_FUNCTIONS
#endif // _D3D12MA_CURRENT_BUDGET_DATA

#ifndef _D3D12MA_DEFRAGMENTATION_CONTEXT_PIMPL
class DefragmentationContextPimpl
{
    D3D12MA_CLASS_NO_COPY(DefragmentationContextPimpl)
public:
    DefragmentationContextPimpl(
        AllocatorPimpl* hAllocator,
        const DEFRAGMENTATION_DESC& desc,
        BlockVector* poolVector);
    ~DefragmentationContextPimpl();

    void GetStats(DEFRAGMENTATION_STATS& outStats) { outStats = m_GlobalStats; }
    const ALLOCATION_CALLBACKS& GetAllocs() const { return m_Moves.GetAllocs(); }

    HRESULT DefragmentPassBegin(DEFRAGMENTATION_PASS_MOVE_INFO& moveInfo);
    HRESULT DefragmentPassEnd(DEFRAGMENTATION_PASS_MOVE_INFO& moveInfo);

private:
    // Max number of allocations to ignore due to size constraints before ending single pass
    static const UINT8 MAX_ALLOCS_TO_IGNORE = 16;
    enum class CounterStatus { Pass, Ignore, End };

    struct FragmentedBlock
    {
        UINT32 data;
        NormalBlock* block;
    };
    struct StateBalanced
    {
        UINT64 avgFreeSize = 0;
        UINT64 avgAllocSize = UINT64_MAX;
    };
    struct MoveAllocationData
    {
        UINT64 size;
        UINT64 alignment;
        ALLOCATION_FLAGS flags;
        // CONFFX_BEGIN - Linked multi gpu
        ALLOCATION_DESC allocDesc;
        // CONFFX_END
        DEFRAGMENTATION_MOVE move = {};
    };

    const UINT64 m_MaxPassBytes;
    const UINT32 m_MaxPassAllocations;

    Vector<DEFRAGMENTATION_MOVE> m_Moves;

    UINT8 m_IgnoredAllocs = 0;
    UINT32 m_Algorithm;
    UINT32 m_BlockVectorCount;
    BlockVector* m_PoolBlockVector;
    BlockVector** m_pBlockVectors;
    size_t m_ImmovableBlockCount = 0;
    DEFRAGMENTATION_STATS m_GlobalStats = { 0 };
    DEFRAGMENTATION_STATS m_PassStats = { 0 };
    void* m_AlgorithmState = NULL;

    static MoveAllocationData GetMoveData(AllocHandle handle, BlockMetadata* metadata);
    CounterStatus CheckCounters(UINT64 bytes);
    bool IncrementCounters(UINT64 bytes);
    bool ReallocWithinBlock(BlockVector& vector, NormalBlock* block);
    bool AllocInOtherBlock(size_t start, size_t end, MoveAllocationData& data, BlockVector& vector);

    bool ComputeDefragmentation(BlockVector& vector, size_t index);
    bool ComputeDefragmentation_Fast(BlockVector& vector);
    bool ComputeDefragmentation_Balanced(BlockVector& vector, size_t index, bool update);
    bool ComputeDefragmentation_Full(BlockVector& vector);

    void UpdateVectorStatistics(BlockVector& vector, StateBalanced& state);
};
#endif // _D3D12MA_DEFRAGMENTATION_CONTEXT_PIMPL

#ifndef _D3D12MA_POOL_PIMPL
class PoolPimpl
{
    friend class Allocator;
    friend struct PoolListItemTraits;
public:
    PoolPimpl(AllocatorPimpl* allocator, const POOL_DESC& desc);
    ~PoolPimpl();

    AllocatorPimpl* GetAllocator() const { return m_Allocator; }
    const POOL_DESC& GetDesc() const { return m_Desc; }
    bool SupportsCommittedAllocations() const { return m_Desc.BlockSize == 0; }
    LPCWSTR GetName() const { return m_Name; }

    BlockVector* GetBlockVector() { return m_BlockVector; }
    CommittedAllocationList* GetCommittedAllocationList() { return SupportsCommittedAllocations() ? &m_CommittedAllocations : NULL; }

    HRESULT Init();
    void GetStatistics(Statistics& outStats);
    void CalculateStatistics(DetailedStatistics& outStats);
    void AddDetailedStatistics(DetailedStatistics& inoutStats);
    void SetName(LPCWSTR Name);

private:
    AllocatorPimpl* m_Allocator; // Externally owned object.
    POOL_DESC m_Desc;
    BlockVector* m_BlockVector; // Owned object.
    CommittedAllocationList m_CommittedAllocations;
    wchar_t* m_Name;
    PoolPimpl* m_PrevPool = NULL;
    PoolPimpl* m_NextPool = NULL;

    void FreeName();
};

struct PoolListItemTraits
{
    using ItemType = PoolPimpl;
    static ItemType* GetPrev(const ItemType* item) { return item->m_PrevPool; }
    static ItemType* GetNext(const ItemType* item) { return item->m_NextPool; }
    static ItemType*& AccessPrev(ItemType* item) { return item->m_PrevPool; }
    static ItemType*& AccessNext(ItemType* item) { return item->m_NextPool; }
};
#endif // _D3D12MA_POOL_PIMPL


#ifndef _D3D12MA_ALLOCATOR_PIMPL
class AllocatorPimpl
{
    friend class Allocator;
    friend class Pool;
public:
    std::atomic_uint32_t m_RefCount = 1;
    CurrentBudgetData m_Budget;

    AllocatorPimpl(const ALLOCATION_CALLBACKS& allocationCallbacks, const ALLOCATOR_DESC& desc);
    ~AllocatorPimpl();

    ID3D12Device* GetDevice() const { return m_Device; }
#ifdef __ID3D12Device4_INTERFACE_DEFINED__
    ID3D12Device4* GetDevice4() const { return m_Device4; }
#endif
#ifdef __ID3D12Device8_INTERFACE_DEFINED__
    ID3D12Device8* GetDevice8() const { return m_Device8; }
#endif
    // Shortcut for "Allocation Callbacks", because this function is called so often.
    const ALLOCATION_CALLBACKS& GetAllocs() const { return m_AllocationCallbacks; }
    const D3D12_FEATURE_DATA_D3D12_OPTIONS& GetD3D12Options() const { return m_D3D12Options; }
    BOOL IsUMA() const { return m_D3D12Architecture.UMA; }
    BOOL IsCacheCoherentUMA() const { return m_D3D12Architecture.CacheCoherentUMA; }
    bool SupportsResourceHeapTier2() const { return m_D3D12Options.ResourceHeapTier >= D3D12_RESOURCE_HEAP_TIER_2; }
    bool UseMutex() const { return m_UseMutex; }
    AllocationObjectAllocator& GetAllocationObjectAllocator() { return m_AllocationObjectAllocator; }
    UINT GetCurrentFrameIndex() const { return m_CurrentFrameIndex.load(); }
    /*
    If SupportsResourceHeapTier2():
        0: D3D12_HEAP_TYPE_DEFAULT
        1: D3D12_HEAP_TYPE_UPLOAD
        2: D3D12_HEAP_TYPE_READBACK
    else:
        0: D3D12_HEAP_TYPE_DEFAULT + buffer
        1: D3D12_HEAP_TYPE_DEFAULT + texture
        2: D3D12_HEAP_TYPE_DEFAULT + texture RT or DS
        3: D3D12_HEAP_TYPE_UPLOAD + buffer
        4: D3D12_HEAP_TYPE_UPLOAD + texture
        5: D3D12_HEAP_TYPE_UPLOAD + texture RT or DS
        6: D3D12_HEAP_TYPE_READBACK + buffer
        7: D3D12_HEAP_TYPE_READBACK + texture
        8: D3D12_HEAP_TYPE_READBACK + texture RT or DS
    */
    UINT GetDefaultPoolCount() const { return SupportsResourceHeapTier2() ? 3 : 9; }
    BlockVector** GetDefaultPools() { return m_BlockVectors; }

    HRESULT Init(const ALLOCATOR_DESC& desc);
    bool HeapFlagsFulfillResourceHeapTier(D3D12_HEAP_FLAGS flags) const;
    UINT StandardHeapTypeToMemorySegmentGroup(D3D12_HEAP_TYPE heapType) const;
    UINT HeapPropertiesToMemorySegmentGroup(const D3D12_HEAP_PROPERTIES& heapProps) const;
    UINT64 GetMemoryCapacity(UINT memorySegmentGroup) const;

    HRESULT CreateResource(
        const ALLOCATION_DESC* pAllocDesc,
        const D3D12_RESOURCE_DESC* pResourceDesc,
        D3D12_RESOURCE_STATES InitialResourceState,
        const D3D12_CLEAR_VALUE *pOptimizedClearValue,
        Allocation** ppAllocation,
        REFIID riidResource,
        void** ppvResource);

#ifdef __ID3D12Device8_INTERFACE_DEFINED__
    HRESULT CreateResource2(
        const ALLOCATION_DESC* pAllocDesc,
        const D3D12_RESOURCE_DESC1* pResourceDesc,
        D3D12_RESOURCE_STATES InitialResourceState,
        const D3D12_CLEAR_VALUE *pOptimizedClearValue,
        Allocation** ppAllocation,
        REFIID riidResource,
        void** ppvResource);
#endif // #ifdef __ID3D12Device8_INTERFACE_DEFINED__

    HRESULT CreateAliasingResource(
        Allocation* pAllocation,
        UINT64 AllocationLocalOffset,
        const D3D12_RESOURCE_DESC* pResourceDesc,
        D3D12_RESOURCE_STATES InitialResourceState,
        const D3D12_CLEAR_VALUE *pOptimizedClearValue,
        REFIID riidResource,
        void** ppvResource);

    HRESULT AllocateMemory(
        const ALLOCATION_DESC* pAllocDesc,
        const D3D12_RESOURCE_ALLOCATION_INFO* pAllocInfo,
        Allocation** ppAllocation);

    // Unregisters allocation from the collection of dedicated allocations.
    // Allocation object must be deleted externally afterwards.
    void FreeCommittedMemory(Allocation* allocation);
    // Unregisters allocation from the collection of placed allocations.
    // Allocation object must be deleted externally afterwards.
    void FreePlacedMemory(Allocation* allocation);
    // Unregisters allocation from the collection of dedicated allocations and destroys associated heap.
    // Allocation object must be deleted externally afterwards.
    void FreeHeapMemory(Allocation* allocation);

    void SetCurrentFrameIndex(UINT frameIndex);
    // For more deailed stats use outCutomHeaps to access statistics divided into L0 and L1 group
    void CalculateStatistics(TotalStatistics& outStats, DetailedStatistics outCutomHeaps[2] = NULL);

    void GetBudget(Budget* outLocalBudget, Budget* outNonLocalBudget);
    void GetBudgetForHeapType(Budget& outBudget, D3D12_HEAP_TYPE heapType);

    void BuildStatsString(WCHAR** ppStatsString, BOOL detailedMap);
    void FreeStatsString(WCHAR* pStatsString);

private:
    using PoolList = IntrusiveLinkedList<PoolListItemTraits>;

    const bool m_UseMutex;
    const bool m_AlwaysCommitted;
    const bool m_MsaaAlwaysCommitted;
    ID3D12Device* m_Device; // AddRef
#ifdef __ID3D12Device4_INTERFACE_DEFINED__
    ID3D12Device4* m_Device4 = NULL; // AddRef, optional
#endif
#ifdef __ID3D12Device8_INTERFACE_DEFINED__
    ID3D12Device8* m_Device8 = NULL; // AddRef, optional
#endif
    IDXGIAdapter* m_Adapter; // AddRef
#if D3D12MA_DXGI_1_4
    IDXGIAdapter3* m_Adapter3 = NULL; // AddRef, optional
#endif
    UINT64 m_PreferredBlockSize;
    ALLOCATION_CALLBACKS m_AllocationCallbacks;
    D3D12MA_ATOMIC_UINT32 m_CurrentFrameIndex;
    DXGI_ADAPTER_DESC m_AdapterDesc;
    D3D12_FEATURE_DATA_D3D12_OPTIONS m_D3D12Options;
    D3D12_FEATURE_DATA_ARCHITECTURE m_D3D12Architecture;
    AllocationObjectAllocator m_AllocationObjectAllocator;

    D3D12MA_RW_MUTEX m_PoolsMutex[HEAP_TYPE_COUNT];
    PoolList m_Pools[HEAP_TYPE_COUNT];
    // Default pools.
    BlockVector* m_BlockVectors[DEFAULT_POOL_MAX_COUNT];
    CommittedAllocationList m_CommittedAllocations[STANDARD_HEAP_TYPE_COUNT];

    /*
    Heuristics that decides whether a resource should better be placed in its own,
    dedicated allocation (committed resource rather than placed resource).
    */
    template<typename D3D12_RESOURCE_DESC_T>
    static bool PrefersCommittedAllocation(const D3D12_RESOURCE_DESC_T& resourceDesc);

    // Allocates and registers new committed resource with implicit heap, as dedicated allocation.
    // Creates and returns Allocation object and optionally D3D12 resource.
    HRESULT AllocateCommittedResource(
        const CommittedAllocationParameters& committedAllocParams,
        UINT64 resourceSize, bool withinBudget, void* pPrivateData,
        const D3D12_RESOURCE_DESC* pResourceDesc,
        D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue,
        Allocation** ppAllocation, REFIID riidResource, void** ppvResource);

#ifdef __ID3D12Device8_INTERFACE_DEFINED__
    HRESULT AllocateCommittedResource2(
        const CommittedAllocationParameters& committedAllocParams,
        UINT64 resourceSize, bool withinBudget, void* pPrivateData,
        const D3D12_RESOURCE_DESC1* pResourceDesc,
        D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue,
        Allocation** ppAllocation, REFIID riidResource, void** ppvResource);
#endif

    // Allocates and registers new heap without any resources placed in it, as dedicated allocation.
    // Creates and returns Allocation object.
    HRESULT AllocateHeap(
        const CommittedAllocationParameters& committedAllocParams,
        const D3D12_RESOURCE_ALLOCATION_INFO& allocInfo, bool withinBudget,
        void* pPrivateData, Allocation** ppAllocation);

    template<typename D3D12_RESOURCE_DESC_T>
    HRESULT CalcAllocationParams(const ALLOCATION_DESC& allocDesc, UINT64 allocSize,
        const D3D12_RESOURCE_DESC_T* resDesc, // Optional
        BlockVector*& outBlockVector, CommittedAllocationParameters& outCommittedAllocationParams, bool& outPreferCommitted);

    // Returns UINT32_MAX if index cannot be calculcated.
    UINT CalcDefaultPoolIndex(const ALLOCATION_DESC& allocDesc, ResourceClass resourceClass) const;
    void CalcDefaultPoolParams(D3D12_HEAP_TYPE& outHeapType, D3D12_HEAP_FLAGS& outHeapFlags, UINT index) const;

    // Registers Pool object in m_Pools.
    void RegisterPool(Pool* pool, D3D12_HEAP_TYPE heapType);
    // Unregisters Pool object from m_Pools.
    void UnregisterPool(Pool* pool, D3D12_HEAP_TYPE heapType);

    HRESULT UpdateD3D12Budget();
    
    D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfoNative(const D3D12_RESOURCE_DESC& resourceDesc) const;
#ifdef __ID3D12Device8_INTERFACE_DEFINED__
    D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfoNative(const D3D12_RESOURCE_DESC1& resourceDesc) const;
#endif

    template<typename D3D12_RESOURCE_DESC_T>
    D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfo(D3D12_RESOURCE_DESC_T& inOutResourceDesc) const;

    bool NewAllocationWithinBudget(D3D12_HEAP_TYPE heapType, UINT64 size);

    // Writes object { } with data of given budget.
    static void WriteBudgetToJson(JsonWriter& json, const Budget& budget);
};

#ifndef _D3D12MA_ALLOCATOR_PIMPL_FUNCTINOS
AllocatorPimpl::AllocatorPimpl(const ALLOCATION_CALLBACKS& allocationCallbacks, const ALLOCATOR_DESC& desc)
    : m_UseMutex((desc.Flags & ALLOCATOR_FLAG_SINGLETHREADED) == 0),
    m_AlwaysCommitted((desc.Flags & ALLOCATOR_FLAG_ALWAYS_COMMITTED) != 0),
    m_MsaaAlwaysCommitted((desc.Flags & ALLOCATOR_FLAG_MSAA_TEXTURES_ALWAYS_COMMITTED) != 0),
    m_Device(desc.pDevice),
    m_Adapter(desc.pAdapter),
    m_PreferredBlockSize(desc.PreferredBlockSize != 0 ? desc.PreferredBlockSize : D3D12MA_DEFAULT_BLOCK_SIZE),
    m_AllocationCallbacks(allocationCallbacks),
    m_CurrentFrameIndex(0),
    // Below this line don't use allocationCallbacks but m_AllocationCallbacks!!!
    m_AllocationObjectAllocator(m_AllocationCallbacks)
{
    // desc.pAllocationCallbacks intentionally ignored here, preprocessed by CreateAllocator.
    ZeroMemory(&m_D3D12Options, sizeof(m_D3D12Options));
    ZeroMemory(&m_D3D12Architecture, sizeof(m_D3D12Architecture));

    ZeroMemory(m_BlockVectors, sizeof(m_BlockVectors));

    for (UINT i = 0; i < STANDARD_HEAP_TYPE_COUNT; ++i)
    {
        m_CommittedAllocations[i].Init(
            m_UseMutex,
            (D3D12_HEAP_TYPE)(D3D12_HEAP_TYPE_DEFAULT + i),
            NULL); // pool
    }

    m_Device->AddRef();
    m_Adapter->AddRef();
}

HRESULT AllocatorPimpl::Init(const ALLOCATOR_DESC& desc)
{
    UNREF_PARAM(desc); 
#if D3D12MA_DXGI_1_4
    desc.pAdapter->QueryInterface(D3D12MA_IID_PPV_ARGS(&m_Adapter3));
#endif

#ifdef __ID3D12Device4_INTERFACE_DEFINED__
    m_Device->QueryInterface(D3D12MA_IID_PPV_ARGS(&m_Device4));
#endif

#ifdef __ID3D12Device8_INTERFACE_DEFINED__
    m_Device->QueryInterface(D3D12MA_IID_PPV_ARGS(&m_Device8));
#endif

    HRESULT hr = m_Adapter->GetDesc(&m_AdapterDesc);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &m_D3D12Options, sizeof(m_D3D12Options));
    if (FAILED(hr))
    {
        return hr;
    }
#ifdef D3D12MA_FORCE_RESOURCE_HEAP_TIER
    m_D3D12Options.ResourceHeapTier = (D3D12MA_FORCE_RESOURCE_HEAP_TIER);
#endif

    hr = m_Device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &m_D3D12Architecture, sizeof(m_D3D12Architecture));
    if (FAILED(hr))
    {
        m_D3D12Architecture.UMA = FALSE;
        m_D3D12Architecture.CacheCoherentUMA = FALSE;
    }

    D3D12_HEAP_PROPERTIES heapProps = {};
    const UINT defaultPoolCount = GetDefaultPoolCount();
    for (UINT i = 0; i < defaultPoolCount; ++i)
    {
        D3D12_HEAP_FLAGS heapFlags;
        CalcDefaultPoolParams(heapProps.Type, heapFlags, i);

#ifdef __ID3D12Device8_INTERFACE_DEFINED__
        if (desc.Flags & ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED)
            heapFlags |= D3D12_HEAP_FLAG_CREATE_NOT_ZEROED;
#endif

        m_BlockVectors[i] = D3D12MA_NEW(GetAllocs(), BlockVector)(
            this, // hAllocator
            heapProps, // heapType
            heapFlags, // heapFlags
            m_PreferredBlockSize,
            0, // minBlockCount
            SIZE_MAX, // maxBlockCount
            false, // explicitBlockSize
            D3D12MA_DEBUG_ALIGNMENT, // minAllocationAlignment
            0, // Default algorithm,
            m_MsaaAlwaysCommitted,
            NULL); // pProtectedSession
        // No need to call m_pBlockVectors[i]->CreateMinBlocks here, becase minBlockCount is 0.
    }

#if D3D12MA_DXGI_1_4
    UpdateD3D12Budget();
#endif

    return S_OK;
}

AllocatorPimpl::~AllocatorPimpl()
{
#ifdef __ID3D12Device8_INTERFACE_DEFINED__
    SAFE_RELEASE(m_Device8);
#endif
#ifdef __ID3D12Device4_INTERFACE_DEFINED__
    SAFE_RELEASE(m_Device4);
#endif
#if D3D12MA_DXGI_1_4
    SAFE_RELEASE(m_Adapter3);
#endif
    SAFE_RELEASE(m_Adapter);
    SAFE_RELEASE(m_Device);

    for (UINT i = DEFAULT_POOL_MAX_COUNT; i--; )
    {
        D3D12MA_DELETE(GetAllocs(), m_BlockVectors[i]);
    }

    for (UINT i = HEAP_TYPE_COUNT; i--; )
    {
        if (!m_Pools[i].IsEmpty())
        {
            D3D12MA_ASSERT(0 && "Unfreed pools found!");
        }
    }
}

bool AllocatorPimpl::HeapFlagsFulfillResourceHeapTier(D3D12_HEAP_FLAGS flags) const
{
    if (SupportsResourceHeapTier2())
    {
        return true;
    }
    else
    {
        const bool allowBuffers = (flags & D3D12_HEAP_FLAG_DENY_BUFFERS) == 0;
        const bool allowRtDsTextures = (flags & D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES) == 0;
        const bool allowNonRtDsTextures = (flags & D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES) == 0;
        const uint8_t allowedGroupCount = (allowBuffers ? 1 : 0) + (allowRtDsTextures ? 1 : 0) + (allowNonRtDsTextures ? 1 : 0);
        return allowedGroupCount == 1;
    }
}

UINT AllocatorPimpl::StandardHeapTypeToMemorySegmentGroup(D3D12_HEAP_TYPE heapType) const
{
    D3D12MA_ASSERT(IsHeapTypeStandard(heapType));
    if (IsUMA())
        return DXGI_MEMORY_SEGMENT_GROUP_LOCAL_COPY;
    return heapType == D3D12_HEAP_TYPE_DEFAULT ?
        DXGI_MEMORY_SEGMENT_GROUP_LOCAL_COPY : DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL_COPY;
}

UINT AllocatorPimpl::HeapPropertiesToMemorySegmentGroup(const D3D12_HEAP_PROPERTIES& heapProps) const
{
    if (IsUMA())
        return DXGI_MEMORY_SEGMENT_GROUP_LOCAL_COPY;
    if (heapProps.MemoryPoolPreference == D3D12_MEMORY_POOL_UNKNOWN)
        return StandardHeapTypeToMemorySegmentGroup(heapProps.Type);
    return heapProps.MemoryPoolPreference == D3D12_MEMORY_POOL_L1 ?
        DXGI_MEMORY_SEGMENT_GROUP_LOCAL_COPY : DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL_COPY;
}

UINT64 AllocatorPimpl::GetMemoryCapacity(UINT memorySegmentGroup) const
{
    switch (memorySegmentGroup)
    {
    case DXGI_MEMORY_SEGMENT_GROUP_LOCAL_COPY:
        return IsUMA() ?
            m_AdapterDesc.DedicatedVideoMemory + m_AdapterDesc.SharedSystemMemory : m_AdapterDesc.DedicatedVideoMemory;
    case DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL_COPY:
        return IsUMA() ? 0 : m_AdapterDesc.SharedSystemMemory;
    default:
        D3D12MA_ASSERT(0);
        return UINT64_MAX;
    }
}

HRESULT AllocatorPimpl::CreateResource(
    const ALLOCATION_DESC* pAllocDesc,
    const D3D12_RESOURCE_DESC* pResourceDesc,
    D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue,
    Allocation** ppAllocation,
    REFIID riidResource,
    void** ppvResource)
{
    D3D12MA_ASSERT(pAllocDesc && pResourceDesc && ppAllocation);

    *ppAllocation = NULL;
    if (ppvResource)
    {
        *ppvResource = NULL;
    }

    // CONFFX_BEGIN - Linked multi gpu
    // Can have only one bit set in creation node mask
    D3D12MA_ASSERT(IsPow2(pAllocDesc->CreationNodeMask));
    // CONFFX_END

    D3D12_RESOURCE_DESC finalResourceDesc = *pResourceDesc;

    // CONFFX_BEGIN - Xbox alignment fix
#if defined(XBOX)
    if (finalResourceDesc.Alignment != 0)
    {
      finalResourceDesc.Alignment = pResourceDesc->SampleDesc.Count > 1 ?
        D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    }
#endif
    // CONFFX_END

    D3D12_RESOURCE_ALLOCATION_INFO resAllocInfo = GetResourceAllocationInfo(finalResourceDesc);

    // CONFFX_BEGIN - Xbox alignment fix
#if defined(XBOX)
    resAllocInfo.Alignment = pResourceDesc->SampleDesc.Count > 1 ?
      D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
#endif
    // CONFFX_END

    D3D12MA_ASSERT(IsPow2(resAllocInfo.Alignment));
    D3D12MA_ASSERT(resAllocInfo.SizeInBytes > 0);

    BlockVector* blockVector = NULL;
    CommittedAllocationParameters committedAllocationParams = {};
    bool preferCommitted = false;
    HRESULT hr = CalcAllocationParams<D3D12_RESOURCE_DESC>(*pAllocDesc, resAllocInfo.SizeInBytes,
        pResourceDesc,
        blockVector, committedAllocationParams, preferCommitted);
    if (FAILED(hr))
        return hr;

    const bool withinBudget = (pAllocDesc->Flags & ALLOCATION_FLAG_WITHIN_BUDGET) != 0;
    hr = E_INVALIDARG;
    if (committedAllocationParams.IsValid() && preferCommitted)
    {
        hr = AllocateCommittedResource(committedAllocationParams,
            resAllocInfo.SizeInBytes, withinBudget, pAllocDesc->pPrivateData,
            &finalResourceDesc, InitialResourceState, pOptimizedClearValue,
            ppAllocation, riidResource, ppvResource);
        if (SUCCEEDED(hr))
            return hr;
    }
    if (blockVector != NULL)
    {
        hr = blockVector->CreateResource(resAllocInfo.SizeInBytes, resAllocInfo.Alignment,
            *pAllocDesc, finalResourceDesc,
            InitialResourceState, pOptimizedClearValue,
            ppAllocation, riidResource, ppvResource);
        if (SUCCEEDED(hr))
            return hr;
    }
    if (committedAllocationParams.IsValid() && !preferCommitted)
    {
        hr = AllocateCommittedResource(committedAllocationParams,
            resAllocInfo.SizeInBytes, withinBudget, pAllocDesc->pPrivateData,
            &finalResourceDesc, InitialResourceState, pOptimizedClearValue,
            ppAllocation, riidResource, ppvResource);
        if (SUCCEEDED(hr))
            return hr;
    }
    return hr;
}

#ifdef __ID3D12Device8_INTERFACE_DEFINED__
HRESULT AllocatorPimpl::CreateResource2(
    const ALLOCATION_DESC* pAllocDesc,
    const D3D12_RESOURCE_DESC1* pResourceDesc,
    D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue,
    Allocation** ppAllocation,
    REFIID riidResource,
    void** ppvResource)
{
    D3D12MA_ASSERT(pAllocDesc && pResourceDesc && ppAllocation);

    *ppAllocation = NULL;
    if (ppvResource)
    {
        *ppvResource = NULL;
    }
    if (m_Device8 == NULL)
    {
        return E_NOINTERFACE;
    }

    D3D12_RESOURCE_DESC1 finalResourceDesc = *pResourceDesc;
    D3D12_RESOURCE_ALLOCATION_INFO resAllocInfo = GetResourceAllocationInfo(finalResourceDesc);
    D3D12MA_ASSERT(IsPow2(resAllocInfo.Alignment));
    D3D12MA_ASSERT(resAllocInfo.SizeInBytes > 0);

    BlockVector* blockVector = NULL;
    CommittedAllocationParameters committedAllocationParams = {};
    bool preferCommitted = false;
    HRESULT hr = CalcAllocationParams<D3D12_RESOURCE_DESC1>(*pAllocDesc, resAllocInfo.SizeInBytes,
        pResourceDesc,
        blockVector, committedAllocationParams, preferCommitted);
    if (FAILED(hr))
        return hr;

    const bool withinBudget = (pAllocDesc->Flags & ALLOCATION_FLAG_WITHIN_BUDGET) != 0;
    hr = E_INVALIDARG;
    if (committedAllocationParams.IsValid() && preferCommitted)
    {
        hr = AllocateCommittedResource2(committedAllocationParams,
            resAllocInfo.SizeInBytes, withinBudget, pAllocDesc->pPrivateData,
            &finalResourceDesc, InitialResourceState, pOptimizedClearValue,
            ppAllocation, riidResource, ppvResource);
        if (SUCCEEDED(hr))
            return hr;
    }
    if (blockVector != NULL)
    {
        hr = blockVector->CreateResource2(resAllocInfo.SizeInBytes, resAllocInfo.Alignment,
            *pAllocDesc, finalResourceDesc,
            InitialResourceState, pOptimizedClearValue,
            ppAllocation, riidResource, ppvResource);
        if (SUCCEEDED(hr))
            return hr;
    }
    if (committedAllocationParams.IsValid() && !preferCommitted)
    {
        hr = AllocateCommittedResource2(committedAllocationParams,
            resAllocInfo.SizeInBytes, withinBudget, pAllocDesc->pPrivateData,
            &finalResourceDesc, InitialResourceState, pOptimizedClearValue,
            ppAllocation, riidResource, ppvResource);
        if (SUCCEEDED(hr))
            return hr;
    }
    return hr;
}
#endif // #ifdef __ID3D12Device8_INTERFACE_DEFINED__

HRESULT AllocatorPimpl::AllocateMemory(
    const ALLOCATION_DESC* pAllocDesc,
    const D3D12_RESOURCE_ALLOCATION_INFO* pAllocInfo,
    Allocation** ppAllocation)
{
    *ppAllocation = NULL;

    BlockVector* blockVector = NULL;
    CommittedAllocationParameters committedAllocationParams = {};
    bool preferCommitted = false;
    HRESULT hr = CalcAllocationParams<D3D12_RESOURCE_DESC>(*pAllocDesc, pAllocInfo->SizeInBytes,
        NULL, // pResDesc
        blockVector, committedAllocationParams, preferCommitted);
    if (FAILED(hr))
        return hr;

    const bool withinBudget = (pAllocDesc->Flags & ALLOCATION_FLAG_WITHIN_BUDGET) != 0;
    hr = E_INVALIDARG;
    if (committedAllocationParams.IsValid() && preferCommitted)
    {
        hr = AllocateHeap(committedAllocationParams, *pAllocInfo, withinBudget, pAllocDesc->pPrivateData, ppAllocation);
        if (SUCCEEDED(hr))
            return hr;
    }
    if (blockVector != NULL)
    {
        hr = blockVector->Allocate(pAllocInfo->SizeInBytes, pAllocInfo->Alignment,
            *pAllocDesc, 1, (Allocation**)ppAllocation);
        if (SUCCEEDED(hr))
            return hr;
    }
    if (committedAllocationParams.IsValid() && !preferCommitted)
    {
        hr = AllocateHeap(committedAllocationParams, *pAllocInfo, withinBudget, pAllocDesc->pPrivateData, ppAllocation);
        if (SUCCEEDED(hr))
            return hr;
    }
    return hr;
}

HRESULT AllocatorPimpl::CreateAliasingResource(
    Allocation* pAllocation,
    UINT64 AllocationLocalOffset,
    const D3D12_RESOURCE_DESC* pResourceDesc,
    D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue,
    REFIID riidResource,
    void** ppvResource)
{
    *ppvResource = NULL;

    D3D12_RESOURCE_DESC resourceDesc2 = *pResourceDesc;
    D3D12_RESOURCE_ALLOCATION_INFO resAllocInfo = GetResourceAllocationInfo(resourceDesc2);
    D3D12MA_ASSERT(IsPow2(resAllocInfo.Alignment));
    D3D12MA_ASSERT(resAllocInfo.SizeInBytes > 0);

    ID3D12Heap* const existingHeap = pAllocation->GetHeap();
    const UINT64 existingOffset = pAllocation->GetOffset();
    const UINT64 existingSize = pAllocation->GetSize();
    const UINT64 newOffset = existingOffset + AllocationLocalOffset;

    if (existingHeap == NULL ||
        AllocationLocalOffset + resAllocInfo.SizeInBytes > existingSize ||
        newOffset % resAllocInfo.Alignment != 0)
    {
        return E_INVALIDARG;
    }

    return m_Device->CreatePlacedResource(
        existingHeap,
        newOffset,
        &resourceDesc2,
        InitialResourceState,
        pOptimizedClearValue,
        riidResource,
        ppvResource);
}

void AllocatorPimpl::FreeCommittedMemory(Allocation* allocation)
{
    D3D12MA_ASSERT(allocation && allocation->m_PackedData.GetType() == Allocation::TYPE_COMMITTED);

    CommittedAllocationList* const allocList = allocation->m_Committed.list;
    allocList->Unregister(allocation);

    const UINT memSegmentGroup = allocList->GetMemorySegmentGroup(this);
    const UINT64 allocSize = allocation->GetSize();
    m_Budget.RemoveAllocation(memSegmentGroup, allocSize);
    m_Budget.RemoveBlock(memSegmentGroup, allocSize);
}

void AllocatorPimpl::FreePlacedMemory(Allocation* allocation)
{
    D3D12MA_ASSERT(allocation && allocation->m_PackedData.GetType() == Allocation::TYPE_PLACED);

    NormalBlock* const block = allocation->m_Placed.block;
    D3D12MA_ASSERT(block);
    BlockVector* const blockVector = block->GetBlockVector();
    D3D12MA_ASSERT(blockVector);
    m_Budget.RemoveAllocation(HeapPropertiesToMemorySegmentGroup(block->GetHeapProperties()), allocation->GetSize());
    blockVector->Free(allocation);
}

void AllocatorPimpl::FreeHeapMemory(Allocation* allocation)
{
    D3D12MA_ASSERT(allocation && allocation->m_PackedData.GetType() == Allocation::TYPE_HEAP);

    CommittedAllocationList* const allocList = allocation->m_Committed.list;
    allocList->Unregister(allocation);
    SAFE_RELEASE(allocation->m_Heap.heap);

    const UINT memSegmentGroup = allocList->GetMemorySegmentGroup(this);
    const UINT64 allocSize = allocation->GetSize();
    m_Budget.RemoveAllocation(memSegmentGroup, allocSize);
    m_Budget.RemoveBlock(memSegmentGroup, allocSize);
}

void AllocatorPimpl::SetCurrentFrameIndex(UINT frameIndex)
{
    m_CurrentFrameIndex.store(frameIndex);

#if D3D12MA_DXGI_1_4
    UpdateD3D12Budget();
#endif
}

void AllocatorPimpl::CalculateStatistics(TotalStatistics& outStats, DetailedStatistics outCutomHeaps[2])
{
    // Init stats
    for (size_t i = 0; i < HEAP_TYPE_COUNT; i++)
        ClearDetailedStatistics(outStats.HeapType[i]);
    for (size_t i = 0; i < DXGI_MEMORY_SEGMENT_GROUP_COUNT; i++)
        ClearDetailedStatistics(outStats.MemorySegmentGroup[i]);
    ClearDetailedStatistics(outStats.Total);
    if (outCutomHeaps)
    {
        ClearDetailedStatistics(outCutomHeaps[0]);
        ClearDetailedStatistics(outCutomHeaps[1]);
    }

    // Process default pools. 3 standard heap types only. Add them to outStats.HeapType[i].
    if (SupportsResourceHeapTier2())
    {
        // DEFAULT, UPLOAD, READBACK.
        for (size_t heapTypeIndex = 0; heapTypeIndex < STANDARD_HEAP_TYPE_COUNT; ++heapTypeIndex)
        {
            BlockVector* const pBlockVector = m_BlockVectors[heapTypeIndex];
            D3D12MA_ASSERT(pBlockVector);
            pBlockVector->AddDetailedStatistics(outStats.HeapType[heapTypeIndex]);
        }
    }
    else
    {
        // DEFAULT, UPLOAD, READBACK.
        for (size_t heapTypeIndex = 0; heapTypeIndex < STANDARD_HEAP_TYPE_COUNT; ++heapTypeIndex)
        {
            for (size_t heapSubType = 0; heapSubType < 3; ++heapSubType)
            {
                BlockVector* const pBlockVector = m_BlockVectors[heapTypeIndex * 3 + heapSubType];
                D3D12MA_ASSERT(pBlockVector);
                pBlockVector->AddDetailedStatistics(outStats.HeapType[heapTypeIndex]);
            }
        }
    }

    // Sum them up to memory segment groups.
    AddDetailedStatistics(
        outStats.MemorySegmentGroup[StandardHeapTypeToMemorySegmentGroup(D3D12_HEAP_TYPE_DEFAULT)],
        outStats.HeapType[0]);
    AddDetailedStatistics(
        outStats.MemorySegmentGroup[StandardHeapTypeToMemorySegmentGroup(D3D12_HEAP_TYPE_UPLOAD)],
        outStats.HeapType[1]);
    AddDetailedStatistics(
        outStats.MemorySegmentGroup[StandardHeapTypeToMemorySegmentGroup(D3D12_HEAP_TYPE_READBACK)],
        outStats.HeapType[2]);

    // Process custom pools.
    DetailedStatistics tmpStats;
    for (size_t heapTypeIndex = 0; heapTypeIndex < HEAP_TYPE_COUNT; ++heapTypeIndex)
    {
        MutexLockRead lock(m_PoolsMutex[heapTypeIndex], m_UseMutex);
        PoolList& poolList = m_Pools[heapTypeIndex];
        for (PoolPimpl* pool = poolList.Front(); pool != NULL; pool = poolList.GetNext(pool))
        {
            const D3D12_HEAP_PROPERTIES& poolHeapProps = pool->GetDesc().HeapProperties;
            ClearDetailedStatistics(tmpStats);
            pool->AddDetailedStatistics(tmpStats);
            AddDetailedStatistics(
                outStats.HeapType[heapTypeIndex], tmpStats);

            UINT memorySegment = HeapPropertiesToMemorySegmentGroup(poolHeapProps);
            AddDetailedStatistics(
                outStats.MemorySegmentGroup[memorySegment], tmpStats);

            if (outCutomHeaps)
                AddDetailedStatistics(outCutomHeaps[memorySegment], tmpStats);
        }
    }

    // Process committed allocations. 3 standard heap types only.
    for (UINT heapTypeIndex = 0; heapTypeIndex < STANDARD_HEAP_TYPE_COUNT; ++heapTypeIndex)
    {
        ClearDetailedStatistics(tmpStats);
        m_CommittedAllocations[heapTypeIndex].AddDetailedStatistics(tmpStats);
        AddDetailedStatistics(
            outStats.HeapType[heapTypeIndex], tmpStats);
        AddDetailedStatistics(
            outStats.MemorySegmentGroup[StandardHeapTypeToMemorySegmentGroup(IndexToHeapType(heapTypeIndex))], tmpStats);
    }

    // Sum up memory segment groups to totals.
    AddDetailedStatistics(outStats.Total, outStats.MemorySegmentGroup[0]);
    AddDetailedStatistics(outStats.Total, outStats.MemorySegmentGroup[1]);

    D3D12MA_ASSERT(outStats.Total.Stats.BlockCount ==
        outStats.MemorySegmentGroup[0].Stats.BlockCount + outStats.MemorySegmentGroup[1].Stats.BlockCount);
    D3D12MA_ASSERT(outStats.Total.Stats.AllocationCount ==
        outStats.MemorySegmentGroup[0].Stats.AllocationCount + outStats.MemorySegmentGroup[1].Stats.AllocationCount);
    D3D12MA_ASSERT(outStats.Total.Stats.BlockBytes ==
        outStats.MemorySegmentGroup[0].Stats.BlockBytes + outStats.MemorySegmentGroup[1].Stats.BlockBytes);
    D3D12MA_ASSERT(outStats.Total.Stats.AllocationBytes ==
        outStats.MemorySegmentGroup[0].Stats.AllocationBytes + outStats.MemorySegmentGroup[1].Stats.AllocationBytes);
    D3D12MA_ASSERT(outStats.Total.UnusedRangeCount ==
        outStats.MemorySegmentGroup[0].UnusedRangeCount + outStats.MemorySegmentGroup[1].UnusedRangeCount);

    D3D12MA_ASSERT(outStats.Total.Stats.BlockCount ==
        outStats.HeapType[0].Stats.BlockCount + outStats.HeapType[1].Stats.BlockCount +
        outStats.HeapType[2].Stats.BlockCount + outStats.HeapType[3].Stats.BlockCount);
    D3D12MA_ASSERT(outStats.Total.Stats.AllocationCount ==
        outStats.HeapType[0].Stats.AllocationCount + outStats.HeapType[1].Stats.AllocationCount +
        outStats.HeapType[2].Stats.AllocationCount + outStats.HeapType[3].Stats.AllocationCount);
    D3D12MA_ASSERT(outStats.Total.Stats.BlockBytes ==
        outStats.HeapType[0].Stats.BlockBytes + outStats.HeapType[1].Stats.BlockBytes +
        outStats.HeapType[2].Stats.BlockBytes + outStats.HeapType[3].Stats.BlockBytes);
    D3D12MA_ASSERT(outStats.Total.Stats.AllocationBytes ==
        outStats.HeapType[0].Stats.AllocationBytes + outStats.HeapType[1].Stats.AllocationBytes +
        outStats.HeapType[2].Stats.AllocationBytes + outStats.HeapType[3].Stats.AllocationBytes);
    D3D12MA_ASSERT(outStats.Total.UnusedRangeCount ==
        outStats.HeapType[0].UnusedRangeCount + outStats.HeapType[1].UnusedRangeCount +
        outStats.HeapType[2].UnusedRangeCount + outStats.HeapType[3].UnusedRangeCount);
}

void AllocatorPimpl::GetBudget(Budget* outLocalBudget, Budget* outNonLocalBudget)
{
    if (outLocalBudget)
        m_Budget.GetStatistics(outLocalBudget->Stats, DXGI_MEMORY_SEGMENT_GROUP_LOCAL_COPY);
    if (outNonLocalBudget)
        m_Budget.GetStatistics(outNonLocalBudget->Stats, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL_COPY);

#if D3D12MA_DXGI_1_4
    if (m_Adapter3)
    {
        if (!m_Budget.ShouldUpdateBudget())
        {
            m_Budget.GetBudget(m_UseMutex,
                outLocalBudget ? &outLocalBudget->UsageBytes : NULL,
                outLocalBudget ? &outLocalBudget->BudgetBytes : NULL,
                outNonLocalBudget ? &outNonLocalBudget->UsageBytes : NULL,
                outNonLocalBudget ? &outNonLocalBudget->BudgetBytes : NULL);
        }
        else
        {
            UpdateD3D12Budget();
            GetBudget(outLocalBudget, outNonLocalBudget); // Recursion
        }
    }
    else
#endif
    {
        if (outLocalBudget)
        {
            outLocalBudget->UsageBytes = outLocalBudget->Stats.BlockBytes;
            outLocalBudget->BudgetBytes = GetMemoryCapacity(DXGI_MEMORY_SEGMENT_GROUP_LOCAL_COPY) * 8 / 10; // 80% heuristics.
        }
        if (outNonLocalBudget)
        {
            outNonLocalBudget->UsageBytes = outNonLocalBudget->Stats.BlockBytes;
            outNonLocalBudget->BudgetBytes = GetMemoryCapacity(DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL_COPY) * 8 / 10; // 80% heuristics.
        }
    }
}

void AllocatorPimpl::GetBudgetForHeapType(Budget& outBudget, D3D12_HEAP_TYPE heapType)
{
    switch (heapType)
    {
    case D3D12_HEAP_TYPE_DEFAULT:
        GetBudget(&outBudget, NULL);
        break;
    case D3D12_HEAP_TYPE_UPLOAD:
    case D3D12_HEAP_TYPE_READBACK:
        GetBudget(NULL, &outBudget);
        break;
    default: D3D12MA_ASSERT(0);
    }
}

void AllocatorPimpl::BuildStatsString(WCHAR** ppStatsString, BOOL detailedMap)
{
    StringBuilder sb(GetAllocs());
    {
        Budget localBudget = {}, nonLocalBudget = {};
        GetBudget(&localBudget, &nonLocalBudget);

        TotalStatistics stats;
        DetailedStatistics customHeaps[2];
        CalculateStatistics(stats, customHeaps);

        JsonWriter json(GetAllocs(), sb);
        json.BeginObject();
        {
            json.WriteString(L"General");
            json.BeginObject();
            {
                json.WriteString(L"API");
                json.WriteString(L"Direct3D 12");

                json.WriteString(L"GPU");
                json.WriteString(m_AdapterDesc.Description);

                json.WriteString(L"DedicatedVideoMemory");
                json.WriteNumber(m_AdapterDesc.DedicatedVideoMemory);
                json.WriteString(L"DedicatedSystemMemory");
                json.WriteNumber(m_AdapterDesc.DedicatedSystemMemory);
                json.WriteString(L"SharedSystemMemory");
                json.WriteNumber(m_AdapterDesc.SharedSystemMemory);
                
                json.WriteString(L"ResourceHeapTier");
                json.WriteNumber(static_cast<UINT>(m_D3D12Options.ResourceHeapTier));

                json.WriteString(L"ResourceBindingTier");
                json.WriteNumber(static_cast<UINT>(m_D3D12Options.ResourceBindingTier));

                json.WriteString(L"TiledResourcesTier");
                json.WriteNumber(static_cast<UINT>(m_D3D12Options.TiledResourcesTier));

                json.WriteString(L"TileBasedRenderer");
                json.WriteBool(m_D3D12Architecture.TileBasedRenderer);

                json.WriteString(L"UMA");
                json.WriteBool(m_D3D12Architecture.UMA);
                json.WriteString(L"CacheCoherentUMA");
                json.WriteBool(m_D3D12Architecture.CacheCoherentUMA);
            }
            json.EndObject();
        }
        {
            json.WriteString(L"Total");
            json.AddDetailedStatisticsInfoObject(stats.Total);
        }
        {
            json.WriteString(L"MemoryInfo");
            json.BeginObject();
            {
                json.WriteString(L"L0");
                json.BeginObject();
                {
                    json.WriteString(L"Budget");
                    WriteBudgetToJson(json, IsUMA() ? localBudget : nonLocalBudget); // When UMA device only L0 present as local

                    json.WriteString(L"Stats");
                    json.AddDetailedStatisticsInfoObject(stats.MemorySegmentGroup[!IsUMA()]);

                    json.WriteString(L"MemoryPools");
                    json.BeginObject();
                    {
                        if (IsUMA())
                        {
                            json.WriteString(L"DEFAULT");
                            json.BeginObject();
                            {
                                json.WriteString(L"Stats");
                                json.AddDetailedStatisticsInfoObject(stats.HeapType[0]);
                            }
                            json.EndObject();
                        }
                        json.WriteString(L"UPLOAD");
                        json.BeginObject();
                        {
                            json.WriteString(L"Stats");
                            json.AddDetailedStatisticsInfoObject(stats.HeapType[1]);
                        }
                        json.EndObject();

                        json.WriteString(L"READBACK");
                        json.BeginObject();
                        {
                            json.WriteString(L"Stats");
                            json.AddDetailedStatisticsInfoObject(stats.HeapType[2]);
                        }
                        json.EndObject();

                        json.WriteString(L"CUSTOM");
                        json.BeginObject();
                        {
                            json.WriteString(L"Stats");
                            json.AddDetailedStatisticsInfoObject(customHeaps[!IsUMA()]);
                        }
                        json.EndObject();
                    }
                    json.EndObject();
                }
                json.EndObject();
                if (!IsUMA())
                {
                    json.WriteString(L"L1");
                    json.BeginObject();
                    {
                        json.WriteString(L"Budget");
                        WriteBudgetToJson(json, localBudget);

                        json.WriteString(L"Stats");
                        json.AddDetailedStatisticsInfoObject(stats.MemorySegmentGroup[0]);

                        json.WriteString(L"MemoryPools");
                        json.BeginObject();
                        {
                            json.WriteString(L"DEFAULT");
                            json.BeginObject();
                            {
                                json.WriteString(L"Stats");
                                json.AddDetailedStatisticsInfoObject(stats.HeapType[0]);
                            }
                            json.EndObject();

                            json.WriteString(L"CUSTOM");
                            json.BeginObject();
                            {
                                json.WriteString(L"Stats");
                                json.AddDetailedStatisticsInfoObject(customHeaps[0]);
                            }
                            json.EndObject();
                        }
                        json.EndObject();
                    }
                    json.EndObject();
                }
            }
            json.EndObject();
        }

        if (detailedMap)
        {
            const auto writeHeapInfo = [&](BlockVector* blockVector, CommittedAllocationList* committedAllocs, bool customHeap)
            {
                D3D12MA_ASSERT(blockVector);

                D3D12_HEAP_FLAGS flags = blockVector->GetHeapFlags();
                json.WriteString(L"Flags");
                json.BeginArray(true);
                {
                    if (flags & D3D12_HEAP_FLAG_SHARED)
                        json.WriteString(L"HEAP_FLAG_SHARED");
                    if (flags & D3D12_HEAP_FLAG_ALLOW_DISPLAY)
                        json.WriteString(L"HEAP_FLAG_ALLOW_DISPLAY");
                    if (flags & D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER)
                        json.WriteString(L"HEAP_FLAG_CROSS_ADAPTER");
                    if (flags & D3D12_HEAP_FLAG_HARDWARE_PROTECTED)
                        json.WriteString(L"HEAP_FLAG_HARDWARE_PROTECTED");
                    if (flags & D3D12_HEAP_FLAG_ALLOW_WRITE_WATCH)
                        json.WriteString(L"HEAP_FLAG_ALLOW_WRITE_WATCH");

                    // CONFFX_BEGIN - Xbox fix
#if !defined(DISABLE_HEAP_FLAG_ALLOW_SHADER_ATOMICS)
                    if (flags & D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS)
                        json.WriteString(L"HEAP_FLAG_ALLOW_SHADER_ATOMICS");
#endif
                    // CONFFX_END

#ifdef __ID3D12Device8_INTERFACE_DEFINED__
                    if (flags & D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT)
                        json.WriteString(L"HEAP_FLAG_CREATE_NOT_RESIDENT");
                    if (flags & D3D12_HEAP_FLAG_CREATE_NOT_ZEROED)
                        json.WriteString(L"HEAP_FLAG_CREATE_NOT_ZEROED");
#endif

                    if (flags & D3D12_HEAP_FLAG_DENY_BUFFERS)
                        json.WriteString(L"HEAP_FLAG_DENY_BUFFERS");
                    if (flags & D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES)
                        json.WriteString(L"HEAP_FLAG_DENY_RT_DS_TEXTURES");
                    if (flags & D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES)
                        json.WriteString(L"HEAP_FLAG_DENY_NON_RT_DS_TEXTURES");

                    flags &= ~(D3D12_HEAP_FLAG_SHARED
                        | D3D12_HEAP_FLAG_DENY_BUFFERS
                        | D3D12_HEAP_FLAG_ALLOW_DISPLAY
                        | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER
                        | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES
                        | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES
                        | D3D12_HEAP_FLAG_HARDWARE_PROTECTED
                        | D3D12_HEAP_FLAG_ALLOW_WRITE_WATCH);

                    // CONFFX_BEGIN - Xbox fix
#if !defined(DISABLE_HEAP_FLAG_ALLOW_SHADER_ATOMICS)
                    flags &= ~(D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS);
#endif
                    // CONFFX_END

#ifdef __ID3D12Device8_INTERFACE_DEFINED__
                    flags &= ~(D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT
                        | D3D12_HEAP_FLAG_CREATE_NOT_ZEROED);
#endif
                    if (flags != 0)
                        json.WriteNumber((UINT)flags);

                    if (customHeap)
                    {
                        const D3D12_HEAP_PROPERTIES& properties = blockVector->GetHeapProperties();
                        switch (properties.MemoryPoolPreference)
                        {
                        default:
                            D3D12MA_ASSERT(0);
                        case D3D12_MEMORY_POOL_UNKNOWN:
                            json.WriteString(L"MEMORY_POOL_UNKNOWN");
                            break;
                        case D3D12_MEMORY_POOL_L0:
                            json.WriteString(L"MEMORY_POOL_L0");
                            break;
                        case D3D12_MEMORY_POOL_L1:
                            json.WriteString(L"MEMORY_POOL_L1");
                            break;
                        }
                        switch (properties.CPUPageProperty)
                        {
                        default:
                            D3D12MA_ASSERT(0);
                        case D3D12_CPU_PAGE_PROPERTY_UNKNOWN:
                            json.WriteString(L"CPU_PAGE_PROPERTY_UNKNOWN");
                            break;
                        case D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE:
                            json.WriteString(L"CPU_PAGE_PROPERTY_NOT_AVAILABLE");
                            break;
                        case D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE:
                            json.WriteString(L"CPU_PAGE_PROPERTY_WRITE_COMBINE");
                            break;
                        case D3D12_CPU_PAGE_PROPERTY_WRITE_BACK:
                            json.WriteString(L"CPU_PAGE_PROPERTY_WRITE_BACK");
                            break;
                        }
                    }
                }
                json.EndArray();

                json.WriteString(L"PreferredBlockSize");
                json.WriteNumber(blockVector->GetPreferredBlockSize());

                json.WriteString(L"Blocks");
                blockVector->WriteBlockInfoToJson(json);

                json.WriteString(L"DedicatedAllocations");
                json.BeginArray();
                if (committedAllocs)
                    committedAllocs->BuildStatsString(json);
                json.EndArray();
            };

            json.WriteString(L"DefaultPools");
            json.BeginObject();
            {
                if (SupportsResourceHeapTier2())
                {
                    for (uint8_t heapType = 0; heapType < STANDARD_HEAP_TYPE_COUNT; ++heapType)
                    {
                        json.WriteString(HeapTypeNames[heapType]);
                        json.BeginObject();
                        writeHeapInfo(m_BlockVectors[heapType], m_CommittedAllocations + heapType, false);
                        json.EndObject();
                    }
                }
                else
                {
                    for (uint8_t heapType = 0; heapType < STANDARD_HEAP_TYPE_COUNT; ++heapType)
                    {
                        for (uint8_t heapSubType = 0; heapSubType < 3; ++heapSubType)
                        {
                            static const WCHAR* const heapSubTypeName[] = {
                                L" - Buffers",
                                L" - Textures",
                                L" - Textures RT/DS",
                            };
                            json.BeginString(HeapTypeNames[heapType]);
                            json.EndString(heapSubTypeName[heapSubType]);

                            json.BeginObject();
                            writeHeapInfo(m_BlockVectors[heapType + heapSubType], m_CommittedAllocations + heapType, false);
                            json.EndObject();
                        }
                    }
                }
            }
            json.EndObject();

            json.WriteString(L"CustomPools");
            json.BeginObject();
            for (uint8_t heapTypeIndex = 0; heapTypeIndex < HEAP_TYPE_COUNT; ++heapTypeIndex)
            {
                MutexLockRead mutex(m_PoolsMutex[heapTypeIndex], m_UseMutex);
                auto* item = m_Pools[heapTypeIndex].Front();
                if (item != NULL)
                {
                    size_t index = 0;
                    json.WriteString(HeapTypeNames[heapTypeIndex]);
                    json.BeginArray();
                    do
                    {
                        json.BeginObject();
                        json.WriteString(L"Name");
                        json.BeginString();
                        json.ContinueString(index++);
                        if (item->GetName())
                        {
                            json.ContinueString(L" - ");
                            json.ContinueString(item->GetName());
                        }
                        json.EndString();

                        writeHeapInfo(item->GetBlockVector(), item->GetCommittedAllocationList(), heapTypeIndex == 3);
                        json.EndObject();
                    } while ((item = PoolList::GetNext(item)) != NULL);
                    json.EndArray();
                }
            }
            json.EndObject();
        }
        json.EndObject();
    }

    const size_t length = sb.GetLength();
    WCHAR* result = AllocateArray<WCHAR>(GetAllocs(), length + 2);
    result[0] = 0xFEFF;
    memcpy(result + 1, sb.GetData(), length * sizeof(WCHAR));
    result[length + 1] = L'\0';
    *ppStatsString = result;
}

void AllocatorPimpl::FreeStatsString(WCHAR* pStatsString)
{
    D3D12MA_ASSERT(pStatsString);
    Free(GetAllocs(), pStatsString);
}

template<typename D3D12_RESOURCE_DESC_T>
bool AllocatorPimpl::PrefersCommittedAllocation(const D3D12_RESOURCE_DESC_T& resourceDesc)
{
    UNREF_PARAM(resourceDesc); 
    // Intentional. It may change in the future.
    return false;
}

HRESULT AllocatorPimpl::AllocateCommittedResource(
    const CommittedAllocationParameters& committedAllocParams,
    UINT64 resourceSize, bool withinBudget, void* pPrivateData,
    const D3D12_RESOURCE_DESC* pResourceDesc,
    D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE* pOptimizedClearValue,
    Allocation** ppAllocation, REFIID riidResource, void** ppvResource)
{
    D3D12MA_ASSERT(committedAllocParams.IsValid());

    HRESULT hr;
    ID3D12Resource* res = NULL;
    // Allocate aliasing memory with explicit heap
    if (committedAllocParams.m_CanAlias)
    {
        D3D12_RESOURCE_ALLOCATION_INFO heapAllocInfo = {};
        heapAllocInfo.SizeInBytes = resourceSize;
        heapAllocInfo.Alignment = HeapFlagsToAlignment(committedAllocParams.m_HeapFlags, m_MsaaAlwaysCommitted);
        hr = AllocateHeap(committedAllocParams, heapAllocInfo, withinBudget, pPrivateData, ppAllocation);
        if (SUCCEEDED(hr))
        {
            hr = m_Device->CreatePlacedResource((*ppAllocation)->GetHeap(), 0,
                pResourceDesc, InitialResourceState,
                pOptimizedClearValue, D3D12MA_IID_PPV_ARGS(&res));
            if (SUCCEEDED(hr))
            {
                if (ppvResource != NULL)
                    hr = res->QueryInterface(riidResource, ppvResource);
                if (SUCCEEDED(hr))
                {
                    (*ppAllocation)->SetResourcePointer(res, pResourceDesc);
                    return hr;
                }
                res->Release();
            }
            FreeHeapMemory(*ppAllocation);
        }
        return hr;
    }

    if (withinBudget &&
        !NewAllocationWithinBudget(committedAllocParams.m_HeapProperties.Type, resourceSize))
    {
        return E_OUTOFMEMORY;
    }

    /* D3D12 ERROR:
     * ID3D12Device::CreateCommittedResource:
     * When creating a committed resource, D3D12_HEAP_FLAGS must not have either
     *      D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES,
     *      D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES,
     *      nor D3D12_HEAP_FLAG_DENY_BUFFERS set.
     * These flags will be set automatically to correspond with the committed resource type.
     *
     * [ STATE_CREATION ERROR #640: CREATERESOURCEANDHEAP_INVALIDHEAPMISCFLAGS]
    */
#ifdef __ID3D12Device4_INTERFACE_DEFINED__
    if (m_Device4)
    {
            hr = m_Device4->CreateCommittedResource1(
                &committedAllocParams.m_HeapProperties,
                committedAllocParams.m_HeapFlags & ~RESOURCE_CLASS_HEAP_FLAGS,
                pResourceDesc, InitialResourceState,
                pOptimizedClearValue, committedAllocParams.m_ProtectedSession, D3D12MA_IID_PPV_ARGS(&res));
    }
    else
#endif
    {
        if (committedAllocParams.m_ProtectedSession == NULL)
        {
            hr = m_Device->CreateCommittedResource(
                &committedAllocParams.m_HeapProperties,
                committedAllocParams.m_HeapFlags & ~RESOURCE_CLASS_HEAP_FLAGS,
                pResourceDesc, InitialResourceState,
                pOptimizedClearValue, D3D12MA_IID_PPV_ARGS(&res));
        }
        else
            hr = E_NOINTERFACE;
    }

    if (SUCCEEDED(hr))
    {
        if (ppvResource != NULL)
        {
            hr = res->QueryInterface(riidResource, ppvResource);
        }
        if (SUCCEEDED(hr))
        {
            const BOOL wasZeroInitialized = TRUE;
            Allocation* alloc = m_AllocationObjectAllocator.Allocate(this, resourceSize, pResourceDesc->Alignment, wasZeroInitialized);
            alloc->InitCommitted(committedAllocParams.m_List);
            alloc->SetResourcePointer(res, pResourceDesc);
            alloc->SetPrivateData(pPrivateData);

            *ppAllocation = alloc;

            committedAllocParams.m_List->Register(alloc);

            const UINT memSegmentGroup = HeapPropertiesToMemorySegmentGroup(committedAllocParams.m_HeapProperties);
            m_Budget.AddBlock(memSegmentGroup, resourceSize);
            m_Budget.AddAllocation(memSegmentGroup, resourceSize);
        }
        else
        {
            res->Release();
        }
    }
    return hr;
}

#ifdef __ID3D12Device8_INTERFACE_DEFINED__
HRESULT AllocatorPimpl::AllocateCommittedResource2(
    const CommittedAllocationParameters& committedAllocParams,
    UINT64 resourceSize, bool withinBudget, void* pPrivateData,
    const D3D12_RESOURCE_DESC1* pResourceDesc,
    D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE* pOptimizedClearValue,
    Allocation** ppAllocation, REFIID riidResource, void** ppvResource)
{
    D3D12MA_ASSERT(committedAllocParams.IsValid());

    if (m_Device8 == NULL)
    {
        return E_NOINTERFACE;
    }

    HRESULT hr;
    ID3D12Resource* res = NULL;
    // Allocate aliasing memory with explicit heap
    if (committedAllocParams.m_CanAlias)
    {
        D3D12_RESOURCE_ALLOCATION_INFO heapAllocInfo = {};
        heapAllocInfo.SizeInBytes = resourceSize;
        heapAllocInfo.Alignment = HeapFlagsToAlignment(committedAllocParams.m_HeapFlags, m_MsaaAlwaysCommitted);
        hr = AllocateHeap(committedAllocParams, heapAllocInfo, withinBudget, pPrivateData, ppAllocation);
        if (SUCCEEDED(hr))
        {
            hr = m_Device8->CreatePlacedResource1((*ppAllocation)->GetHeap(), 0,
                pResourceDesc, InitialResourceState,
                pOptimizedClearValue, D3D12MA_IID_PPV_ARGS(&res));
            if (SUCCEEDED(hr))
            {
                if (ppvResource != NULL)
                    hr = res->QueryInterface(riidResource, ppvResource);
                if (SUCCEEDED(hr))
                {
                    (*ppAllocation)->SetResourcePointer(res, pResourceDesc);
                    return hr;
                }
                res->Release();
            }
            FreeHeapMemory(*ppAllocation);
        }
        return hr;
    }

    if (withinBudget &&
        !NewAllocationWithinBudget(committedAllocParams.m_HeapProperties.Type, resourceSize))
    {
        return E_OUTOFMEMORY;
    }

    hr = m_Device8->CreateCommittedResource2(
        &committedAllocParams.m_HeapProperties,
        committedAllocParams.m_HeapFlags & ~RESOURCE_CLASS_HEAP_FLAGS, // D3D12 ERROR: ID3D12Device::CreateCommittedResource: When creating a committed resource, D3D12_HEAP_FLAGS must not have either D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES, D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES, nor D3D12_HEAP_FLAG_DENY_BUFFERS set. These flags will be set automatically to correspond with the committed resource type. [ STATE_CREATION ERROR #640: CREATERESOURCEANDHEAP_INVALIDHEAPMISCFLAGS]
        pResourceDesc, InitialResourceState,
        pOptimizedClearValue, committedAllocParams.m_ProtectedSession, D3D12MA_IID_PPV_ARGS(&res));
    if (SUCCEEDED(hr))
    {
        if (ppvResource != NULL)
        {
            hr = res->QueryInterface(riidResource, ppvResource);
        }
        if (SUCCEEDED(hr))
        {
            const BOOL wasZeroInitialized = TRUE;
            Allocation* alloc = m_AllocationObjectAllocator.Allocate(this, resourceSize, pResourceDesc->Alignment, wasZeroInitialized);
            alloc->InitCommitted(committedAllocParams.m_List);
            alloc->SetResourcePointer(res, pResourceDesc);
            alloc->SetPrivateData(pPrivateData);

            *ppAllocation = alloc;

            committedAllocParams.m_List->Register(alloc);

            const UINT memSegmentGroup = HeapPropertiesToMemorySegmentGroup(committedAllocParams.m_HeapProperties);
            m_Budget.AddBlock(memSegmentGroup, resourceSize);
            m_Budget.AddAllocation(memSegmentGroup, resourceSize);
        }
        else
        {
            res->Release();
        }
    }
    return hr;
}
#endif // #ifdef __ID3D12Device8_INTERFACE_DEFINED__

HRESULT AllocatorPimpl::AllocateHeap(
    const CommittedAllocationParameters& committedAllocParams,
    const D3D12_RESOURCE_ALLOCATION_INFO& allocInfo, bool withinBudget,
    void* pPrivateData, Allocation** ppAllocation)
{
    D3D12MA_ASSERT(committedAllocParams.IsValid());

    *ppAllocation = nullptr;

    if (withinBudget &&
        !NewAllocationWithinBudget(committedAllocParams.m_HeapProperties.Type, allocInfo.SizeInBytes))
    {
        return E_OUTOFMEMORY;
    }

    D3D12_HEAP_DESC heapDesc = {};
    heapDesc.SizeInBytes = allocInfo.SizeInBytes;
    heapDesc.Properties = committedAllocParams.m_HeapProperties;
    heapDesc.Alignment = allocInfo.Alignment;
    heapDesc.Flags = committedAllocParams.m_HeapFlags;

    HRESULT hr;
    ID3D12Heap* heap = nullptr;
#ifdef __ID3D12Device4_INTERFACE_DEFINED__
    if (m_Device4)
        hr = m_Device4->CreateHeap1(&heapDesc, committedAllocParams.m_ProtectedSession, D3D12MA_IID_PPV_ARGS(&heap));
    else
#endif
    {
        if (committedAllocParams.m_ProtectedSession == NULL)
            hr = m_Device->CreateHeap(&heapDesc, D3D12MA_IID_PPV_ARGS(&heap));
        else
            hr = E_NOINTERFACE;
    }

    if (SUCCEEDED(hr))
    {
        const BOOL wasZeroInitialized = TRUE;
        (*ppAllocation) = m_AllocationObjectAllocator.Allocate(this, allocInfo.SizeInBytes, allocInfo.Alignment, wasZeroInitialized);
        (*ppAllocation)->InitHeap(committedAllocParams.m_List, heap);
        (*ppAllocation)->SetPrivateData(pPrivateData);
        committedAllocParams.m_List->Register(*ppAllocation);

        const UINT memSegmentGroup = HeapPropertiesToMemorySegmentGroup(committedAllocParams.m_HeapProperties);
        m_Budget.AddBlock(memSegmentGroup, allocInfo.SizeInBytes);
        m_Budget.AddAllocation(memSegmentGroup, allocInfo.SizeInBytes);
    }
    return hr;
}

template<typename D3D12_RESOURCE_DESC_T>
HRESULT AllocatorPimpl::CalcAllocationParams(const ALLOCATION_DESC& allocDesc, UINT64 allocSize,
    const D3D12_RESOURCE_DESC_T* resDesc,
    BlockVector*& outBlockVector, CommittedAllocationParameters& outCommittedAllocationParams, bool& outPreferCommitted)
{
    outBlockVector = NULL;
    outCommittedAllocationParams = CommittedAllocationParameters();
    outPreferCommitted = false;

    bool msaaAlwaysCommitted;
    if (allocDesc.CustomPool != NULL)
    {
        PoolPimpl* const pool = allocDesc.CustomPool->m_Pimpl;

        msaaAlwaysCommitted = pool->GetBlockVector()->DeniesMsaaTextures();
        outBlockVector = pool->GetBlockVector();

        outCommittedAllocationParams.m_ProtectedSession = pool->GetDesc().pProtectedSession;
        outCommittedAllocationParams.m_HeapProperties = pool->GetDesc().HeapProperties;
        outCommittedAllocationParams.m_HeapFlags = pool->GetDesc().HeapFlags;
        outCommittedAllocationParams.m_List = pool->GetCommittedAllocationList();
    }
    else
    {
        if (!IsHeapTypeStandard(allocDesc.HeapType))
        {
            return E_INVALIDARG;
        }
        msaaAlwaysCommitted = m_MsaaAlwaysCommitted;

        // CONFFX_BEGIN - Linked multi gpu
        outCommittedAllocationParams.m_HeapProperties.Type = allocDesc.HeapType;
        outCommittedAllocationParams.m_HeapProperties.CreationNodeMask = allocDesc.CreationNodeMask;
        outCommittedAllocationParams.m_HeapProperties.VisibleNodeMask = allocDesc.VisibleNodeMask;
        // CONFFX_END

        outCommittedAllocationParams.m_HeapFlags = allocDesc.ExtraHeapFlags;
        outCommittedAllocationParams.m_List = &m_CommittedAllocations[HeapTypeToIndex(allocDesc.HeapType)];

        const ResourceClass resourceClass = (resDesc != NULL) ?
            ResourceDescToResourceClass(*resDesc) : HeapFlagsToResourceClass(allocDesc.ExtraHeapFlags);
        const UINT defaultPoolIndex = CalcDefaultPoolIndex(allocDesc, resourceClass);
        if (defaultPoolIndex != UINT32_MAX)
        {
            outBlockVector = m_BlockVectors[defaultPoolIndex];
            const UINT64 preferredBlockSize = outBlockVector->GetPreferredBlockSize();
            if (allocSize > preferredBlockSize)
            {
                outBlockVector = NULL;
            }
            else if (allocSize > preferredBlockSize / 2)
            {
                // Heuristics: Allocate committed memory if requested size if greater than half of preferred block size.
                outPreferCommitted = true;
            }
        }

        const D3D12_HEAP_FLAGS extraHeapFlags = allocDesc.ExtraHeapFlags & ~RESOURCE_CLASS_HEAP_FLAGS;
        if (outBlockVector != NULL && extraHeapFlags != 0)
        {
            outBlockVector = NULL;
        }
    }

    if ((allocDesc.Flags & ALLOCATION_FLAG_COMMITTED) != 0 ||
        m_AlwaysCommitted)
    {
        outBlockVector = NULL;
    }
    if ((allocDesc.Flags & ALLOCATION_FLAG_NEVER_ALLOCATE) != 0)
    {
        outCommittedAllocationParams.m_List = NULL;
    }
    outCommittedAllocationParams.m_CanAlias = allocDesc.Flags & ALLOCATION_FLAG_CAN_ALIAS;

    if (resDesc != NULL)
    {
        if (resDesc->SampleDesc.Count > 1 && msaaAlwaysCommitted)
            outBlockVector = NULL;
        if (!outPreferCommitted && PrefersCommittedAllocation(*resDesc))
            outPreferCommitted = true;
    }

    return (outBlockVector != NULL || outCommittedAllocationParams.m_List != NULL) ? S_OK : E_INVALIDARG;
}

UINT AllocatorPimpl::CalcDefaultPoolIndex(const ALLOCATION_DESC& allocDesc, ResourceClass resourceClass) const
{
    const D3D12_HEAP_FLAGS extraHeapFlags = allocDesc.ExtraHeapFlags & ~RESOURCE_CLASS_HEAP_FLAGS;
    if (extraHeapFlags != 0)
    {
        return UINT32_MAX;
    }

    UINT poolIndex = UINT_MAX;
    switch (allocDesc.HeapType)
    {
    case D3D12_HEAP_TYPE_DEFAULT:  poolIndex = 0; break;
    case D3D12_HEAP_TYPE_UPLOAD:   poolIndex = 1; break;
    case D3D12_HEAP_TYPE_READBACK: poolIndex = 2; break;
    default: D3D12MA_ASSERT(0);
    }

    if (SupportsResourceHeapTier2())
        return poolIndex;
    else
    {
        switch (resourceClass)
        {
        case ResourceClass::Buffer:
            return poolIndex * 3;
        case ResourceClass::Non_RT_DS_Texture:
            return poolIndex * 3 + 1;
        case ResourceClass::RT_DS_Texture:
            return poolIndex * 3 + 2;
        default:
            return UINT32_MAX;
        }
    }
}

void AllocatorPimpl::CalcDefaultPoolParams(D3D12_HEAP_TYPE& outHeapType, D3D12_HEAP_FLAGS& outHeapFlags, UINT index) const
{
    outHeapType = D3D12_HEAP_TYPE_DEFAULT;
    outHeapFlags = D3D12_HEAP_FLAG_NONE;

    if (!SupportsResourceHeapTier2())
    {
        switch (index % 3)
        {
        case 0:
            outHeapFlags = D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
            break;
        case 1:
            outHeapFlags = D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
            break;
        case 2:
            outHeapFlags = D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
            break;
        }

        index /= 3;
    }

    switch (index)
    {
    case 0:
        outHeapType = D3D12_HEAP_TYPE_DEFAULT;
        break;
    case 1:
        outHeapType = D3D12_HEAP_TYPE_UPLOAD;
        break;
    case 2:
        outHeapType = D3D12_HEAP_TYPE_READBACK;
        break;
    default:
        D3D12MA_ASSERT(0);
    }
}

void AllocatorPimpl::RegisterPool(Pool* pool, D3D12_HEAP_TYPE heapType)
{
    const UINT heapTypeIndex = HeapTypeToIndex(heapType);

    MutexLockWrite lock(m_PoolsMutex[heapTypeIndex], m_UseMutex);
    m_Pools[heapTypeIndex].PushBack(pool->m_Pimpl);
}

void AllocatorPimpl::UnregisterPool(Pool* pool, D3D12_HEAP_TYPE heapType)
{
    const UINT heapTypeIndex = HeapTypeToIndex(heapType);

    MutexLockWrite lock(m_PoolsMutex[heapTypeIndex], m_UseMutex);
    m_Pools[heapTypeIndex].Remove(pool->m_Pimpl);
}

HRESULT AllocatorPimpl::UpdateD3D12Budget()
{
#if D3D12MA_DXGI_1_4
    if (m_Adapter3)
        return m_Budget.UpdateBudget(m_Adapter3, m_UseMutex);
    else
        return E_NOINTERFACE;
#else
    return S_OK;
#endif
}

D3D12_RESOURCE_ALLOCATION_INFO AllocatorPimpl::GetResourceAllocationInfoNative(const D3D12_RESOURCE_DESC& resourceDesc) const
{
    return m_Device->GetResourceAllocationInfo(0, 1, &resourceDesc);
}

#ifdef __ID3D12Device8_INTERFACE_DEFINED__
D3D12_RESOURCE_ALLOCATION_INFO AllocatorPimpl::GetResourceAllocationInfoNative(const D3D12_RESOURCE_DESC1& resourceDesc) const
{
    D3D12MA_ASSERT(m_Device8 != NULL);
    D3D12_RESOURCE_ALLOCATION_INFO1 info1Unused;
    return m_Device8->GetResourceAllocationInfo2(0, 1, &resourceDesc, &info1Unused);
}
#endif // #ifdef __ID3D12Device8_INTERFACE_DEFINED__

template<typename D3D12_RESOURCE_DESC_T>
D3D12_RESOURCE_ALLOCATION_INFO AllocatorPimpl::GetResourceAllocationInfo(D3D12_RESOURCE_DESC_T& inOutResourceDesc) const
{
    /* Optional optimization: Microsoft documentation says:
    https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getresourceallocationinfo

    Your application can forgo using GetResourceAllocationInfo for buffer resources
    (D3D12_RESOURCE_DIMENSION_BUFFER). Buffers have the same size on all adapters,
    which is merely the smallest multiple of 64KB that's greater or equal to
    D3D12_RESOURCE_DESC::Width.
    */
    if (inOutResourceDesc.Alignment == 0 &&
        inOutResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
        return {
            AlignUp<UINT64>(inOutResourceDesc.Width, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT), // SizeInBytes
            D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT }; // Alignment
    }

#if D3D12MA_USE_SMALL_RESOURCE_PLACEMENT_ALIGNMENT
    if (inOutResourceDesc.Alignment == 0 &&
        inOutResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
        (inOutResourceDesc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) == 0
#if D3D12MA_USE_SMALL_RESOURCE_PLACEMENT_ALIGNMENT == 1
        && CanUseSmallAlignment(inOutResourceDesc)
#endif
        )
    {
        /*
        The algorithm here is based on Microsoft sample: "Small Resources Sample"
        https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12SmallResources
        */
        const UINT64 smallAlignmentToTry = inOutResourceDesc.SampleDesc.Count > 1 ?
            D3D12_SMALL_MSAA_RESOURCE_PLACEMENT_ALIGNMENT :
            D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT;
        inOutResourceDesc.Alignment = smallAlignmentToTry;
        const D3D12_RESOURCE_ALLOCATION_INFO smallAllocInfo = GetResourceAllocationInfoNative(inOutResourceDesc);
        // Check if alignment requested has been granted.
        if (smallAllocInfo.Alignment == smallAlignmentToTry)
        {
            return smallAllocInfo;
        }
        inOutResourceDesc.Alignment = 0; // Restore original
    }
#endif // #if D3D12MA_USE_SMALL_RESOURCE_PLACEMENT_ALIGNMENT

    return GetResourceAllocationInfoNative(inOutResourceDesc);
}

bool AllocatorPimpl::NewAllocationWithinBudget(D3D12_HEAP_TYPE heapType, UINT64 size)
{
    Budget budget = {};
    GetBudgetForHeapType(budget, heapType);
    return budget.UsageBytes + size <= budget.BudgetBytes;
}

void AllocatorPimpl::WriteBudgetToJson(JsonWriter& json, const Budget& budget)
{
    json.BeginObject();
    {
        json.WriteString(L"BudgetBytes");
        json.WriteNumber(budget.BudgetBytes);
        json.WriteString(L"UsageBytes");
        json.WriteNumber(budget.UsageBytes);
    }
    json.EndObject();
}
#endif // _D3D12MA_ALLOCATOR_PIMPL
#endif // _D3D12MA_ALLOCATOR_PIMPL

#ifndef _D3D12MA_VIRTUAL_BLOCK_PIMPL
class VirtualBlockPimpl
{
public:
    const ALLOCATION_CALLBACKS m_AllocationCallbacks;
    const UINT64 m_Size;
    BlockMetadata* m_Metadata;

    VirtualBlockPimpl(const ALLOCATION_CALLBACKS& allocationCallbacks, const VIRTUAL_BLOCK_DESC& desc);
    ~VirtualBlockPimpl();
};

#ifndef _D3D12MA_VIRTUAL_BLOCK_PIMPL_FUNCTIONS
VirtualBlockPimpl::VirtualBlockPimpl(const ALLOCATION_CALLBACKS& allocationCallbacks, const VIRTUAL_BLOCK_DESC& desc)
    : m_AllocationCallbacks(allocationCallbacks), m_Size(desc.Size)
{
    switch (desc.Flags & VIRTUAL_BLOCK_FLAG_ALGORITHM_MASK)
    {
    case VIRTUAL_BLOCK_FLAG_ALGORITHM_LINEAR:
        m_Metadata = D3D12MA_NEW(allocationCallbacks, BlockMetadata_Linear)(&m_AllocationCallbacks, true);
        break;
    default:
        D3D12MA_ASSERT(0);
    case 0:
        m_Metadata = D3D12MA_NEW(allocationCallbacks, BlockMetadata_TLSF)(&m_AllocationCallbacks, true);
        break;
    }
    m_Metadata->Init(m_Size);
}

VirtualBlockPimpl::~VirtualBlockPimpl()
{
    D3D12MA_DELETE(m_AllocationCallbacks, m_Metadata);
}
#endif // _D3D12MA_VIRTUAL_BLOCK_PIMPL_FUNCTIONS
#endif // _D3D12MA_VIRTUAL_BLOCK_PIMPL


#ifndef _D3D12MA_MEMORY_BLOCK_FUNCTIONS
MemoryBlock::MemoryBlock(
    AllocatorPimpl* allocator,
    const D3D12_HEAP_PROPERTIES& heapProps,
    D3D12_HEAP_FLAGS heapFlags,
    UINT64 size,
    UINT id)
    : m_Allocator(allocator),
    m_HeapProps(heapProps),
    m_HeapFlags(heapFlags),
    m_Size(size),
    m_Id(id) {}

MemoryBlock::~MemoryBlock()
{
    if (m_Heap)
    {
        m_Heap->Release();
        m_Allocator->m_Budget.RemoveBlock(
            m_Allocator->HeapPropertiesToMemorySegmentGroup(m_HeapProps), m_Size);
    }
}

HRESULT MemoryBlock::Init(ID3D12ProtectedResourceSession* pProtectedSession, bool denyMsaaTextures)
{
    D3D12MA_ASSERT(m_Heap == NULL && m_Size > 0);

    D3D12_HEAP_DESC heapDesc = {};
    heapDesc.SizeInBytes = m_Size;
    heapDesc.Properties = m_HeapProps;
    heapDesc.Alignment = HeapFlagsToAlignment(m_HeapFlags, denyMsaaTextures);
    heapDesc.Flags = m_HeapFlags;

    HRESULT hr;
#ifdef __ID3D12Device4_INTERFACE_DEFINED__
    ID3D12Device4* const device4 = m_Allocator->GetDevice4();
    if (device4)
        hr = m_Allocator->GetDevice4()->CreateHeap1(&heapDesc, pProtectedSession, D3D12MA_IID_PPV_ARGS(&m_Heap));
    else
#endif
    {
        if (pProtectedSession == NULL)
            hr = m_Allocator->GetDevice()->CreateHeap(&heapDesc, D3D12MA_IID_PPV_ARGS(&m_Heap));
        else
            hr = E_NOINTERFACE;
    }

    if (SUCCEEDED(hr))
    {
        m_Allocator->m_Budget.AddBlock(
            m_Allocator->HeapPropertiesToMemorySegmentGroup(m_HeapProps), m_Size);
    }
    return hr;
}
#endif // _D3D12MA_MEMORY_BLOCK_FUNCTIONS

#ifndef _D3D12MA_NORMAL_BLOCK_FUNCTIONS
NormalBlock::NormalBlock(
    AllocatorPimpl* allocator,
    BlockVector* blockVector,
    const D3D12_HEAP_PROPERTIES& heapProps,
    D3D12_HEAP_FLAGS heapFlags,
    UINT64 size,
    UINT id)
    : MemoryBlock(allocator, heapProps, heapFlags, size, id),
    m_pMetadata(NULL),
    m_BlockVector(blockVector) {}

NormalBlock::~NormalBlock()
{
    if (m_pMetadata != NULL)
    {
        // THIS IS THE MOST IMPORTANT ASSERT IN THE ENTIRE LIBRARY!
        // Hitting it means you have some memory leak - unreleased Allocation objects.
        D3D12MA_ASSERT(m_pMetadata->IsEmpty() && "Some allocations were not freed before destruction of this memory block!");

        D3D12MA_DELETE(m_Allocator->GetAllocs(), m_pMetadata);
    }
}

HRESULT NormalBlock::Init(UINT32 algorithm, ID3D12ProtectedResourceSession* pProtectedSession, bool denyMsaaTextures)
{
    HRESULT hr = MemoryBlock::Init(pProtectedSession, denyMsaaTextures);
    if (FAILED(hr))
    {
        return hr;
    }

    switch (algorithm)
    {
    case POOL_FLAG_ALGORITHM_LINEAR:
        m_pMetadata = D3D12MA_NEW(m_Allocator->GetAllocs(), BlockMetadata_Linear)(&m_Allocator->GetAllocs(), false);
        break;
    default:
        D3D12MA_ASSERT(0);
    case 0:
        m_pMetadata = D3D12MA_NEW(m_Allocator->GetAllocs(), BlockMetadata_TLSF)(&m_Allocator->GetAllocs(), false);
        break;
    }
    m_pMetadata->Init(m_Size);

    return hr;
}

bool NormalBlock::Validate() const
{
    D3D12MA_VALIDATE(GetHeap() &&
        m_pMetadata &&
        m_pMetadata->GetSize() != 0 &&
        m_pMetadata->GetSize() == GetSize());
    return m_pMetadata->Validate();
}
#endif // _D3D12MA_NORMAL_BLOCK_FUNCTIONS

#ifndef _D3D12MA_COMMITTED_ALLOCATION_LIST_FUNCTIONS
void CommittedAllocationList::Init(bool useMutex, D3D12_HEAP_TYPE heapType, PoolPimpl* pool)
{
    m_UseMutex = useMutex;
    m_HeapType = heapType;
    m_Pool = pool;
}

CommittedAllocationList::~CommittedAllocationList()
{
    if (!m_AllocationList.IsEmpty())
    {
        D3D12MA_ASSERT(0 && "Unfreed committed allocations found!");
    }
}

UINT CommittedAllocationList::GetMemorySegmentGroup(AllocatorPimpl* allocator) const
{
    if (m_Pool)
        return allocator->HeapPropertiesToMemorySegmentGroup(m_Pool->GetDesc().HeapProperties);
    else
        return allocator->StandardHeapTypeToMemorySegmentGroup(m_HeapType);
}

void CommittedAllocationList::AddStatistics(Statistics& inoutStats)
{
    MutexLockRead lock(m_Mutex, m_UseMutex);

    for (Allocation* alloc = m_AllocationList.Front();
        alloc != NULL; alloc = m_AllocationList.GetNext(alloc))
    {
        const UINT64 size = alloc->GetSize();
        inoutStats.BlockCount++;
        inoutStats.AllocationCount++;
        inoutStats.BlockBytes += size;
        inoutStats.AllocationBytes += size;
    }
}

void CommittedAllocationList::AddDetailedStatistics(DetailedStatistics& inoutStats)
{
    MutexLockRead lock(m_Mutex, m_UseMutex);

    for (Allocation* alloc = m_AllocationList.Front();
        alloc != NULL; alloc = m_AllocationList.GetNext(alloc))
    {
        const UINT64 size = alloc->GetSize();
        inoutStats.Stats.BlockCount++;
        inoutStats.Stats.BlockBytes += size;
        AddDetailedStatisticsAllocation(inoutStats, size);
    }
}

void CommittedAllocationList::BuildStatsString(JsonWriter& json)
{
    MutexLockRead lock(m_Mutex, m_UseMutex);

    for (Allocation* alloc = m_AllocationList.Front();
        alloc != NULL; alloc = m_AllocationList.GetNext(alloc))
    {
        json.BeginObject(true);
        json.AddAllocationToObject(*alloc);
        json.EndObject();
    }
}

void CommittedAllocationList::Register(Allocation* alloc)
{
    MutexLockWrite lock(m_Mutex, m_UseMutex);
    m_AllocationList.PushBack(alloc);
}

void CommittedAllocationList::Unregister(Allocation* alloc)
{
    MutexLockWrite lock(m_Mutex, m_UseMutex);
    m_AllocationList.Remove(alloc);
}
#endif // _D3D12MA_COMMITTED_ALLOCATION_LIST_FUNCTIONS

#ifndef _D3D12MA_BLOCK_VECTOR_FUNCTIONS
BlockVector::BlockVector(
    AllocatorPimpl* hAllocator,
    const D3D12_HEAP_PROPERTIES& heapProps,
    D3D12_HEAP_FLAGS heapFlags,
    UINT64 preferredBlockSize,
    size_t minBlockCount,
    size_t maxBlockCount,
    bool explicitBlockSize,
    UINT64 minAllocationAlignment,
    UINT32 algorithm,
    bool denyMsaaTextures,
    ID3D12ProtectedResourceSession* pProtectedSession)
    : m_hAllocator(hAllocator),
    m_HeapProps(heapProps),
    m_HeapFlags(heapFlags),
    m_PreferredBlockSize(preferredBlockSize),
    m_MinBlockCount(minBlockCount),
    m_MaxBlockCount(maxBlockCount),
    m_ExplicitBlockSize(explicitBlockSize),
    m_MinAllocationAlignment(minAllocationAlignment),
    m_Algorithm(algorithm),
    m_DenyMsaaTextures(denyMsaaTextures),
    m_ProtectedSession(pProtectedSession),
    m_HasEmptyBlock(false),
    m_Blocks(hAllocator->GetAllocs()),
    m_NextBlockId(0) {}

BlockVector::~BlockVector()
{
    for (size_t i = m_Blocks.size(); i--; )
    {
        D3D12MA_DELETE(m_hAllocator->GetAllocs(), m_Blocks[i]);
    }
}

HRESULT BlockVector::CreateMinBlocks()
{
    for (size_t i = 0; i < m_MinBlockCount; ++i)
    {
        // CONFFX_BEGIN - Linked multi gpu
        // Create blocks for default node (node 0 or node mask 1)
        HRESULT hr = CreateBlock(m_PreferredBlockSize, 1, 1, NULL);
        // CONFFX_END
        if (FAILED(hr))
        {
            return hr;
        }
    }
    return S_OK;
}

bool BlockVector::IsEmpty()
{
    MutexLockRead lock(m_Mutex, m_hAllocator->UseMutex());
    return m_Blocks.empty();
}

HRESULT BlockVector::Allocate(
    UINT64 size,
    UINT64 alignment,
    const ALLOCATION_DESC& allocDesc,
    size_t allocationCount,
    Allocation** pAllocations)
{
    size_t allocIndex;
    HRESULT hr = S_OK;

    {
        MutexLockWrite lock(m_Mutex, m_hAllocator->UseMutex());
        for (allocIndex = 0; allocIndex < allocationCount; ++allocIndex)
        {
            hr = AllocatePage(
                size,
                alignment,
                allocDesc,
                pAllocations + allocIndex);
            if (FAILED(hr))
            {
                break;
            }
        }
    }

    if (FAILED(hr))
    {
        // Free all already created allocations.
        while (allocIndex--)
        {
            Free(pAllocations[allocIndex]);
        }
        ZeroMemory(pAllocations, sizeof(Allocation*) * allocationCount);
    }

    return hr;
}

void BlockVector::Free(Allocation* hAllocation)
{
    NormalBlock* pBlockToDelete = NULL;

    bool budgetExceeded = false;
    if (IsHeapTypeStandard(m_HeapProps.Type))
    {
        Budget budget = {};
        m_hAllocator->GetBudgetForHeapType(budget, m_HeapProps.Type);
        budgetExceeded = budget.UsageBytes >= budget.BudgetBytes;
    }

    // Scope for lock.
    {
        MutexLockWrite lock(m_Mutex, m_hAllocator->UseMutex());

        NormalBlock* pBlock = hAllocation->m_Placed.block;

        pBlock->m_pMetadata->Free(hAllocation->GetAllocHandle());
        D3D12MA_HEAVY_ASSERT(pBlock->Validate());

        const size_t blockCount = m_Blocks.size();
        // pBlock became empty after this deallocation.
        if (pBlock->m_pMetadata->IsEmpty())
        {
            // Already has empty Allocation. We don't want to have two, so delete this one.
            if ((m_HasEmptyBlock || budgetExceeded) &&
                blockCount > m_MinBlockCount)
            {
                pBlockToDelete = pBlock;
                Remove(pBlock);
            }
            // We now have first empty block.
            else
            {
                m_HasEmptyBlock = true;
            }
        }
        // pBlock didn't become empty, but we have another empty block - find and free that one.
        // (This is optional, heuristics.)
        else if (m_HasEmptyBlock && blockCount > m_MinBlockCount)
        {
            NormalBlock* pLastBlock = m_Blocks.back();
            if (pLastBlock->m_pMetadata->IsEmpty())
            {
                pBlockToDelete = pLastBlock;
                m_Blocks.pop_back();
                m_HasEmptyBlock = false;
            }
        }

        IncrementallySortBlocks();
    }

    // Destruction of a free Allocation. Deferred until this point, outside of mutex
    // lock, for performance reason.
    if (pBlockToDelete != NULL)
    {
        D3D12MA_DELETE(m_hAllocator->GetAllocs(), pBlockToDelete);
    }
}

HRESULT BlockVector::CreateResource(
    UINT64 size,
    UINT64 alignment,
    const ALLOCATION_DESC& allocDesc,
    const D3D12_RESOURCE_DESC& resourceDesc,
    D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue,
    Allocation** ppAllocation,
    REFIID riidResource,
    void** ppvResource)
{
    HRESULT hr = Allocate(size, alignment, allocDesc, 1, ppAllocation);
    if (SUCCEEDED(hr))
    {
        ID3D12Resource* res = NULL;
        hr = m_hAllocator->GetDevice()->CreatePlacedResource(
            (*ppAllocation)->m_Placed.block->GetHeap(),
            (*ppAllocation)->GetOffset(),
            &resourceDesc,
            InitialResourceState,
            pOptimizedClearValue,
            D3D12MA_IID_PPV_ARGS(&res));
        if (SUCCEEDED(hr))
        {
            if (ppvResource != NULL)
            {
                hr = res->QueryInterface(riidResource, ppvResource);
            }
            if (SUCCEEDED(hr))
            {
                (*ppAllocation)->SetResourcePointer(res, &resourceDesc);
            }
            else
            {
                res->Release();
                SAFE_RELEASE(*ppAllocation);
            }
        }
        else
        {
            SAFE_RELEASE(*ppAllocation);
        }
    }
    return hr;
}

#ifdef __ID3D12Device8_INTERFACE_DEFINED__
HRESULT BlockVector::CreateResource2(
    UINT64 size,
    UINT64 alignment,
    const ALLOCATION_DESC& allocDesc,
    const D3D12_RESOURCE_DESC1& resourceDesc,
    D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue,
    Allocation** ppAllocation,
    REFIID riidResource,
    void** ppvResource)
{
    ID3D12Device8* const device8 = m_hAllocator->GetDevice8();
    if (device8 == NULL)
    {
        return E_NOINTERFACE;
    }

    HRESULT hr = Allocate(size, alignment, allocDesc, 1, ppAllocation);
    if (SUCCEEDED(hr))
    {
        ID3D12Resource* res = NULL;
        hr = device8->CreatePlacedResource1(
            (*ppAllocation)->m_Placed.block->GetHeap(),
            (*ppAllocation)->GetOffset(),
            &resourceDesc,
            InitialResourceState,
            pOptimizedClearValue,
            D3D12MA_IID_PPV_ARGS(&res));
        if (SUCCEEDED(hr))
        {
            if (ppvResource != NULL)
            {
                hr = res->QueryInterface(riidResource, ppvResource);
            }
            if (SUCCEEDED(hr))
            {
                (*ppAllocation)->SetResourcePointer(res, &resourceDesc);
            }
            else
            {
                res->Release();
                SAFE_RELEASE(*ppAllocation);
            }
        }
        else
        {
            SAFE_RELEASE(*ppAllocation);
        }
    }
    return hr;
}
#endif // #ifdef __ID3D12Device8_INTERFACE_DEFINED__

void BlockVector::AddStatistics(Statistics& inoutStats)
{
    MutexLockRead lock(m_Mutex, m_hAllocator->UseMutex());

    for (size_t i = 0; i < m_Blocks.size(); ++i)
    {
        const NormalBlock* const pBlock = m_Blocks[i];
        D3D12MA_ASSERT(pBlock);
        D3D12MA_HEAVY_ASSERT(pBlock->Validate());
        pBlock->m_pMetadata->AddStatistics(inoutStats);
    }
}

void BlockVector::AddDetailedStatistics(DetailedStatistics& inoutStats)
{
    MutexLockRead lock(m_Mutex, m_hAllocator->UseMutex());

    for (size_t i = 0; i < m_Blocks.size(); ++i)
    {
        const NormalBlock* const pBlock = m_Blocks[i];
        D3D12MA_ASSERT(pBlock);
        D3D12MA_HEAVY_ASSERT(pBlock->Validate());
        pBlock->m_pMetadata->AddDetailedStatistics(inoutStats);
    }
}

void BlockVector::WriteBlockInfoToJson(JsonWriter& json)
{
    MutexLockRead lock(m_Mutex, m_hAllocator->UseMutex());

    json.BeginObject();

    for (size_t i = 0, count = m_Blocks.size(); i < count; ++i)
    {
        const NormalBlock* const pBlock = m_Blocks[i];
        D3D12MA_ASSERT(pBlock);
        D3D12MA_HEAVY_ASSERT(pBlock->Validate());
        json.BeginString();
        json.ContinueString(pBlock->GetId());
        json.EndString();

        json.BeginObject();
        pBlock->m_pMetadata->WriteAllocationInfoToJson(json);
        json.EndObject();
    }

    json.EndObject();
}

UINT64 BlockVector::CalcSumBlockSize() const
{
    UINT64 result = 0;
    for (size_t i = m_Blocks.size(); i--; )
    {
        result += m_Blocks[i]->m_pMetadata->GetSize();
    }
    return result;
}

UINT64 BlockVector::CalcMaxBlockSize() const
{
    UINT64 result = 0;
    for (size_t i = m_Blocks.size(); i--; )
    {
        result = D3D12MA_MAX(result, m_Blocks[i]->m_pMetadata->GetSize());
        if (result >= m_PreferredBlockSize)
        {
            break;
        }
    }
    return result;
}

void BlockVector::Remove(NormalBlock* pBlock)
{
    for (size_t blockIndex = 0; blockIndex < m_Blocks.size(); ++blockIndex)
    {
        if (m_Blocks[blockIndex] == pBlock)
        {
            m_Blocks.remove(blockIndex);
            return;
        }
    }
    D3D12MA_ASSERT(0);
}

void BlockVector::IncrementallySortBlocks()
{
    if (!m_IncrementalSort)
        return;
    // Bubble sort only until first swap.
    for (size_t i = 1; i < m_Blocks.size(); ++i)
    {
        if (m_Blocks[i - 1]->m_pMetadata->GetSumFreeSize() > m_Blocks[i]->m_pMetadata->GetSumFreeSize())
        {
            D3D12MA_SWAP(m_Blocks[i - 1], m_Blocks[i]);
            return;
        }
    }
}

void BlockVector::SortByFreeSize()
{
    D3D12MA_SORT(m_Blocks.begin(), m_Blocks.end(),
        [](auto* b1, auto* b2)
        {
            return b1->m_pMetadata->GetSumFreeSize() < b2->m_pMetadata->GetSumFreeSize();
        });
}

HRESULT BlockVector::AllocatePage(
    UINT64 size,
    UINT64 alignment,
    const ALLOCATION_DESC& allocDesc,
    Allocation** pAllocation)
{
    // Early reject: requested allocation size is larger that maximum block size for this block vector.
    if (size + D3D12MA_DEBUG_MARGIN > m_PreferredBlockSize)
    {
        return E_OUTOFMEMORY;
    }

    UINT64 freeMemory = UINT64_MAX;
    if (IsHeapTypeStandard(m_HeapProps.Type))
    {
        Budget budget = {};
        m_hAllocator->GetBudgetForHeapType(budget, m_HeapProps.Type);
        freeMemory = (budget.UsageBytes < budget.BudgetBytes) ? (budget.BudgetBytes - budget.UsageBytes) : 0;
    }

    const bool canCreateNewBlock =
        ((allocDesc.Flags & ALLOCATION_FLAG_NEVER_ALLOCATE) == 0) &&
        (m_Blocks.size() < m_MaxBlockCount) &&
        // Even if we don't have to stay within budget with this allocation, when the
        // budget would be exceeded, we don't want to allocate new blocks, but always
        // create resources as committed.
        freeMemory >= size;

    // 1. Search existing allocations
    {
        // Forward order in m_Blocks - prefer blocks with smallest amount of free space.
        for (size_t blockIndex = 0; blockIndex < m_Blocks.size(); ++blockIndex)
        {
            NormalBlock* const pCurrBlock = m_Blocks[blockIndex];
            D3D12MA_ASSERT(pCurrBlock);
            HRESULT hr = AllocateFromBlock(
                pCurrBlock,
                size,
                alignment,
                allocDesc.Flags,
                // CONFFX_BEGIN - Linked multi gpu
                allocDesc,
                // CONFFX_END
                allocDesc.pPrivateData,
                allocDesc.Flags & ALLOCATION_FLAG_STRATEGY_MASK,
                pAllocation);
            if (SUCCEEDED(hr))
            {
                return hr;
            }
        }
    }

    // 2. Try to create new block.
    if (canCreateNewBlock)
    {
        // Calculate optimal size for new block.
        UINT64 newBlockSize = m_PreferredBlockSize;
        UINT newBlockSizeShift = 0;

        if (!m_ExplicitBlockSize)
        {
            // Allocate 1/8, 1/4, 1/2 as first blocks.
            const UINT64 maxExistingBlockSize = CalcMaxBlockSize();
            for (UINT i = 0; i < NEW_BLOCK_SIZE_SHIFT_MAX; ++i)
            {
                const UINT64 smallerNewBlockSize = newBlockSize / 2;
                if (smallerNewBlockSize > maxExistingBlockSize && smallerNewBlockSize >= size * 2)
                {
                    newBlockSize = smallerNewBlockSize;
                    ++newBlockSizeShift;
                }
                else
                {
                    break;
                }
            }
        }

        size_t newBlockIndex = 0;
        HRESULT hr = newBlockSize <= freeMemory ?
          // CONFFX_BEGIN - Linked multi gpu
          CreateBlock(newBlockSize, allocDesc.CreationNodeMask, allocDesc.VisibleNodeMask, &newBlockIndex) : E_OUTOFMEMORY;
        // CONFFX_END
        // Allocation of this size failed? Try 1/2, 1/4, 1/8 of m_PreferredBlockSize.
        if (!m_ExplicitBlockSize)
        {
            while (FAILED(hr) && newBlockSizeShift < NEW_BLOCK_SIZE_SHIFT_MAX)
            {
                const UINT64 smallerNewBlockSize = newBlockSize / 2;
                if (smallerNewBlockSize >= size)
                {
                    newBlockSize = smallerNewBlockSize;
                    ++newBlockSizeShift;
                    hr = newBlockSize <= freeMemory ?
                      // CONFFX_BEGIN - Linked multi gpu
                      CreateBlock(newBlockSize, allocDesc.CreationNodeMask, allocDesc.VisibleNodeMask, &newBlockIndex) : E_OUTOFMEMORY;
                      // CONFFX_END
                }
                else
                {
                    break;
                }
            }
        }

        if (SUCCEEDED(hr))
        {
            NormalBlock* const pBlock = m_Blocks[newBlockIndex];
            D3D12MA_ASSERT(pBlock->m_pMetadata->GetSize() >= size);

            hr = AllocateFromBlock(
                pBlock,
                size,
                alignment,
                allocDesc.Flags,
                // CONFFX_BEGIN - Linked multi gpu
                allocDesc,
                // CONFFX_END
                allocDesc.pPrivateData,
                allocDesc.Flags & ALLOCATION_FLAG_STRATEGY_MASK,
                pAllocation);
            if (SUCCEEDED(hr))
            {
                return hr;
            }
            else
            {
                // Allocation from new block failed, possibly due to D3D12MA_DEBUG_MARGIN or alignment.
                return E_OUTOFMEMORY;
            }
        }
    }

    return E_OUTOFMEMORY;
}

HRESULT BlockVector::AllocateFromBlock(
    NormalBlock* pBlock,
    UINT64 size,
    UINT64 alignment,
    ALLOCATION_FLAGS allocFlags,
    // CONFFX_BEGIN - Linked multi gpu
    const ALLOCATION_DESC& allocDesc,
    // CONFFX_END
    void* pPrivateData,
    UINT32 strategy,
    Allocation** pAllocation)
{
    alignment = D3D12MA_MAX(alignment, m_MinAllocationAlignment);

    // CONFFX_BEGIN - Linked multi gpu
    // Check if node masks are compatible
    // #NOTE: Visible node mask only has to be a subset of the block's visible node mask so maybe we can think
    // of changing
    // (pBlock->GetVisibleNodeMask() != allocDesc.VisibleNodeMask) to (allocDesc.VisibleNodeMask & pBlock->GetVisibleNodeMask())
    // #NOTE: Virtual memory currently does not use CreationNodeMask and VisibileNodeMase, using above BITWISE AND will always fail since both will be 0
    if (pBlock->GetHeapProperties().CreationNodeMask != allocDesc.CreationNodeMask || pBlock->GetHeapProperties().VisibleNodeMask != allocDesc.VisibleNodeMask)
      return E_OUTOFMEMORY;
    // CONFFX_END

    AllocationRequest currRequest = {};
    if (pBlock->m_pMetadata->CreateAllocationRequest(
        size,
        alignment,
        allocFlags & ALLOCATION_FLAG_UPPER_ADDRESS,
        strategy,
        &currRequest))
    {
        return CommitAllocationRequest(currRequest, pBlock, size, alignment, allocDesc, pPrivateData, pAllocation);
    }
    return E_OUTOFMEMORY;
}

HRESULT BlockVector::CommitAllocationRequest(
    AllocationRequest& allocRequest,
    NormalBlock* pBlock,
    UINT64 size,
    UINT64 alignment,
    // CONFFX_BEGIN - Linked multi gpu
    const ALLOCATION_DESC& allocDesc,
    // CONFFX_END
    void* pPrivateData,
    Allocation** pAllocation)
{
    // We no longer have an empty Allocation.
    if (pBlock->m_pMetadata->IsEmpty())
        m_HasEmptyBlock = false;

    *pAllocation = m_hAllocator->GetAllocationObjectAllocator().Allocate(m_hAllocator, size, alignment, allocRequest.zeroInitialized);
    pBlock->m_pMetadata->Alloc(allocRequest, size, *pAllocation);

    (*pAllocation)->InitPlaced(allocRequest.allocHandle, pBlock);
    (*pAllocation)->SetPrivateData(pPrivateData);

    D3D12MA_HEAVY_ASSERT(pBlock->Validate());
    m_hAllocator->m_Budget.AddAllocation(m_hAllocator->HeapPropertiesToMemorySegmentGroup(m_HeapProps), size);

    (*pAllocation)->m_AllocDesc = allocDesc;

    return S_OK;
}

HRESULT BlockVector::CreateBlock(
    UINT64 blockSize,
    // CONFFX_BEGIN - Linked multi gpu
    UINT creationNodeMask, UINT visibleNodeMask,
    // CONFFX_END
    size_t* pNewBlockIndex)
{
    // CONFFX_BEGIN - Linked multi gpu
    D3D12_HEAP_PROPERTIES heapProps = m_HeapProps;
    heapProps.CreationNodeMask = creationNodeMask;
    heapProps.VisibleNodeMask = visibleNodeMask;
    // CONFFX_END
    
    NormalBlock* const pBlock = D3D12MA_NEW(m_hAllocator->GetAllocs(), NormalBlock)(
        m_hAllocator,
        this,
        // CONFFX_BEGIN - Linked multi gpu
        heapProps,
        // CONFFX_END
        m_HeapFlags,
        blockSize,
        m_NextBlockId++);
    HRESULT hr = pBlock->Init(m_Algorithm, m_ProtectedSession, m_DenyMsaaTextures);
    if (FAILED(hr))
    {
        D3D12MA_DELETE(m_hAllocator->GetAllocs(), pBlock);
        return hr;
    }

    m_Blocks.push_back(pBlock);
    if (pNewBlockIndex != NULL)
    {
        *pNewBlockIndex = m_Blocks.size() - 1;
    }

    return hr;
}
#endif // _D3D12MA_BLOCK_VECTOR_FUNCTIONS

#ifndef _D3D12MA_DEFRAGMENTATION_CONTEXT_PIMPL_FUNCTIONS
DefragmentationContextPimpl::DefragmentationContextPimpl(
    AllocatorPimpl* hAllocator,
    const DEFRAGMENTATION_DESC& desc,
    BlockVector* poolVector)
    : m_MaxPassBytes(desc.MaxBytesPerPass == 0 ? UINT64_MAX : desc.MaxBytesPerPass),
    m_MaxPassAllocations(desc.MaxAllocationsPerPass == 0 ? UINT32_MAX : desc.MaxAllocationsPerPass),
    m_Moves(hAllocator->GetAllocs())
{
    m_Algorithm = desc.Flags & DEFRAGMENTATION_FLAG_ALGORITHM_MASK;

    if (poolVector != NULL)
    {
        m_BlockVectorCount = 1;
        m_PoolBlockVector = poolVector;
        m_pBlockVectors = &m_PoolBlockVector;
        m_PoolBlockVector->SetIncrementalSort(false);
        m_PoolBlockVector->SortByFreeSize();
    }
    else
    {
        m_BlockVectorCount = hAllocator->GetDefaultPoolCount();
        m_PoolBlockVector = NULL;
        m_pBlockVectors = hAllocator->GetDefaultPools();
        for (UINT32 i = 0; i < m_BlockVectorCount; ++i)
        {
            BlockVector* vector = m_pBlockVectors[i];
            if (vector != NULL)
            {
                vector->SetIncrementalSort(false);
                vector->SortByFreeSize();
            }
        }
    }

    switch (m_Algorithm)
    {
    case 0: // Default algorithm
        m_Algorithm = DEFRAGMENTATION_FLAG_ALGORITHM_BALANCED;
    case DEFRAGMENTATION_FLAG_ALGORITHM_BALANCED:
    {
        m_AlgorithmState = D3D12MA_NEW_ARRAY(hAllocator->GetAllocs(), StateBalanced, m_BlockVectorCount);
        break;
    }
    }
}

DefragmentationContextPimpl::~DefragmentationContextPimpl()
{
    if (m_PoolBlockVector != NULL)
        m_PoolBlockVector->SetIncrementalSort(true);
    else
    {
        for (UINT32 i = 0; i < m_BlockVectorCount; ++i)
        {
            BlockVector* vector = m_pBlockVectors[i];
            if (vector != NULL)
                vector->SetIncrementalSort(true);
        }
    }

    if (m_AlgorithmState)
    {
        switch (m_Algorithm)
        {
        case DEFRAGMENTATION_FLAG_ALGORITHM_BALANCED:
            D3D12MA_DELETE_ARRAY(m_Moves.GetAllocs(), reinterpret_cast<StateBalanced*>(m_AlgorithmState), m_BlockVectorCount);
            break;
        default:
            D3D12MA_ASSERT(0);
        }
    }
}

HRESULT DefragmentationContextPimpl::DefragmentPassBegin(DEFRAGMENTATION_PASS_MOVE_INFO& moveInfo)
{
    if (m_PoolBlockVector != NULL)
    {
        MutexLockWrite lock(m_PoolBlockVector->GetMutex(), m_PoolBlockVector->m_hAllocator->UseMutex());

        if (m_PoolBlockVector->GetBlockCount() > 1)
            ComputeDefragmentation(*m_PoolBlockVector, 0);
        else if (m_PoolBlockVector->GetBlockCount() == 1)
            ReallocWithinBlock(*m_PoolBlockVector, m_PoolBlockVector->GetBlock(0));

        // Setup index into block vector
        for (size_t i = 0; i < m_Moves.size(); ++i)
            m_Moves[i].pDstTmpAllocation->SetPrivateData(0);
    }
    else
    {
        for (UINT32 i = 0; i < m_BlockVectorCount; ++i)
        {
            if (m_pBlockVectors[i] != NULL)
            {
                MutexLockWrite lock(m_pBlockVectors[i]->GetMutex(), m_pBlockVectors[i]->m_hAllocator->UseMutex());

                bool end = false;
                size_t movesOffset = m_Moves.size();
                if (m_pBlockVectors[i]->GetBlockCount() > 1)
                {
                    end = ComputeDefragmentation(*m_pBlockVectors[i], i);
                }
                else if (m_pBlockVectors[i]->GetBlockCount() == 1)
                {
                    end = ReallocWithinBlock(*m_pBlockVectors[i], m_pBlockVectors[i]->GetBlock(0));
                }

                // Setup index into block vector
                for (; movesOffset < m_Moves.size(); ++movesOffset)
                    m_Moves[movesOffset].pDstTmpAllocation->SetPrivateData(reinterpret_cast<void*>(static_cast<uintptr_t>(i)));

                if (end)
                    break;
            }
        }
    }

    moveInfo.MoveCount = static_cast<UINT32>(m_Moves.size());
    if (moveInfo.MoveCount > 0)
    {
        moveInfo.pMoves = m_Moves.data();
        return S_FALSE;
    }

    moveInfo.pMoves = NULL;
    return S_OK;
}

HRESULT DefragmentationContextPimpl::DefragmentPassEnd(DEFRAGMENTATION_PASS_MOVE_INFO& moveInfo)
{
    D3D12MA_ASSERT(moveInfo.MoveCount > 0 ? moveInfo.pMoves != NULL : true);

    HRESULT result = S_OK;
    Vector<FragmentedBlock> immovableBlocks(m_Moves.GetAllocs());

    for (uint32_t i = 0; i < moveInfo.MoveCount; ++i)
    {
        DEFRAGMENTATION_MOVE& move = moveInfo.pMoves[i];
        size_t prevCount = 0, currentCount = 0;
        UINT64 freedBlockSize = 0;

        UINT32 vectorIndex;
        BlockVector* vector;
        if (m_PoolBlockVector != NULL)
        {
            vectorIndex = 0;
            vector = m_PoolBlockVector;
        }
        else
        {
            vectorIndex = static_cast<UINT32>(reinterpret_cast<uintptr_t>(move.pDstTmpAllocation->GetPrivateData()));
            vector = m_pBlockVectors[vectorIndex];
            D3D12MA_ASSERT(vector != NULL);
        }

        switch (move.Operation)
        {
        case DEFRAGMENTATION_MOVE_OPERATION_COPY:
        {
            move.pSrcAllocation->SwapBlockAllocation(move.pDstTmpAllocation);

            // Scope for locks, Free have it's own lock
            {
                MutexLockRead lock(vector->GetMutex(), vector->m_hAllocator->UseMutex());
                prevCount = vector->GetBlockCount();
                freedBlockSize = move.pDstTmpAllocation->GetBlock()->m_pMetadata->GetSize();
            }
            move.pDstTmpAllocation->Release();
            {
                MutexLockRead lock(vector->GetMutex(), vector->m_hAllocator->UseMutex());
                currentCount = vector->GetBlockCount();
            }

            result = S_FALSE;
            break;
        }
        case DEFRAGMENTATION_MOVE_OPERATION_IGNORE:
        {
            m_PassStats.BytesMoved -= move.pSrcAllocation->GetSize();
            --m_PassStats.AllocationsMoved;
            move.pDstTmpAllocation->Release();

            NormalBlock* newBlock = move.pSrcAllocation->GetBlock();
            bool notPresent = true;
            for (const FragmentedBlock& block : immovableBlocks)
            {
                if (block.block == newBlock)
                {
                    notPresent = false;
                    break;
                }
            }
            if (notPresent)
                immovableBlocks.push_back({ vectorIndex, newBlock });
            break;
        }
        case DEFRAGMENTATION_MOVE_OPERATION_DESTROY:
        {
            m_PassStats.BytesMoved -= move.pSrcAllocation->GetSize();
            --m_PassStats.AllocationsMoved;
            // Scope for locks, Free have it's own lock
            {
                MutexLockRead lock(vector->GetMutex(), vector->m_hAllocator->UseMutex());
                prevCount = vector->GetBlockCount();
                freedBlockSize = move.pSrcAllocation->GetBlock()->m_pMetadata->GetSize();
            }
            move.pSrcAllocation->Release();
            {
                MutexLockRead lock(vector->GetMutex(), vector->m_hAllocator->UseMutex());
                currentCount = vector->GetBlockCount();
            }
            freedBlockSize *= prevCount - currentCount;

            UINT64 dstBlockSize;
            {
                MutexLockRead lock(vector->GetMutex(), vector->m_hAllocator->UseMutex());
                dstBlockSize = move.pDstTmpAllocation->GetBlock()->m_pMetadata->GetSize();
            }
            move.pDstTmpAllocation->Release();
            {
                MutexLockRead lock(vector->GetMutex(), vector->m_hAllocator->UseMutex());
                freedBlockSize += dstBlockSize * (currentCount - vector->GetBlockCount());
                currentCount = vector->GetBlockCount();
            }

            result = S_FALSE;
            break;
        }
        default:
            D3D12MA_ASSERT(0);
        }

        if (prevCount > currentCount)
        {
            size_t freedBlocks = prevCount - currentCount;
            m_PassStats.HeapsFreed += static_cast<UINT32>(freedBlocks);
            m_PassStats.BytesFreed += freedBlockSize;
        }
    }
    moveInfo.MoveCount = 0;
    moveInfo.pMoves = NULL;
    m_Moves.clear();

    // Update stats
    m_GlobalStats.AllocationsMoved += m_PassStats.AllocationsMoved;
    m_GlobalStats.BytesFreed += m_PassStats.BytesFreed;
    m_GlobalStats.BytesMoved += m_PassStats.BytesMoved;
    m_GlobalStats.HeapsFreed += m_PassStats.HeapsFreed;
    m_PassStats = { 0 };

    // Move blocks with immovable allocations according to algorithm
    if (immovableBlocks.size() > 0)
    {
        // Move to the begining
        for (const FragmentedBlock& block : immovableBlocks)
        {
            BlockVector* vector = m_pBlockVectors[block.data];
            MutexLockWrite lock(vector->GetMutex(), vector->m_hAllocator->UseMutex());

            for (size_t i = m_ImmovableBlockCount; i < vector->GetBlockCount(); ++i)
            {
                if (vector->GetBlock(i) == block.block)
                {
                    D3D12MA_SWAP(vector->m_Blocks[i], vector->m_Blocks[m_ImmovableBlockCount++]);
                    break;
                }
            }
        }
    }
    return result;
}

bool DefragmentationContextPimpl::ComputeDefragmentation(BlockVector& vector, size_t index)
{
    switch (m_Algorithm)
    {
    case DEFRAGMENTATION_FLAG_ALGORITHM_FAST:
        return ComputeDefragmentation_Fast(vector);
    default:
        D3D12MA_ASSERT(0);
    case DEFRAGMENTATION_FLAG_ALGORITHM_BALANCED:
        return ComputeDefragmentation_Balanced(vector, index, true);
    case DEFRAGMENTATION_FLAG_ALGORITHM_FULL:
        return ComputeDefragmentation_Full(vector);
    }
}

DefragmentationContextPimpl::MoveAllocationData DefragmentationContextPimpl::GetMoveData(
    AllocHandle handle, BlockMetadata* metadata)
{
    MoveAllocationData moveData;
    moveData.move.pSrcAllocation = (Allocation*)metadata->GetAllocationPrivateData(handle);
    moveData.size = moveData.move.pSrcAllocation->GetSize();
    moveData.alignment = moveData.move.pSrcAllocation->GetAlignment();
    moveData.allocDesc = moveData.move.pSrcAllocation->GetAllocationDesc();
    moveData.flags = ALLOCATION_FLAG_NONE;

    return moveData;
}

DefragmentationContextPimpl::CounterStatus DefragmentationContextPimpl::CheckCounters(UINT64 bytes)
{
    // Ignore allocation if will exceed max size for copy
    if (m_PassStats.BytesMoved + bytes > m_MaxPassBytes)
    {
        if (++m_IgnoredAllocs < MAX_ALLOCS_TO_IGNORE)
            return CounterStatus::Ignore;
        else
            return CounterStatus::End;
    }
    return CounterStatus::Pass;
}

bool DefragmentationContextPimpl::IncrementCounters(UINT64 bytes)
{
    m_PassStats.BytesMoved += bytes;
    // Early return when max found
    if (++m_PassStats.AllocationsMoved >= m_MaxPassAllocations || m_PassStats.BytesMoved >= m_MaxPassBytes)
    {
        D3D12MA_ASSERT(m_PassStats.AllocationsMoved == m_MaxPassAllocations ||
            m_PassStats.BytesMoved == m_MaxPassBytes && "Exceeded maximal pass threshold!");
        return true;
    }
    return false;
}

bool DefragmentationContextPimpl::ReallocWithinBlock(BlockVector& vector, NormalBlock* block)
{
    BlockMetadata* metadata = block->m_pMetadata;

    for (AllocHandle handle = metadata->GetAllocationListBegin();
        handle != (AllocHandle)0;
        handle = metadata->GetNextAllocation(handle))
    {
        MoveAllocationData moveData = GetMoveData(handle, metadata);
        // Ignore newly created allocations by defragmentation algorithm
        if (moveData.move.pSrcAllocation->GetPrivateData() == this)
            continue;
        switch (CheckCounters(moveData.move.pSrcAllocation->GetSize()))
        {
        case CounterStatus::Ignore:
            continue;
        case CounterStatus::End:
            return true;
        default:
            D3D12MA_ASSERT(0);
        case CounterStatus::Pass:
            break;
        }
        
        UINT64 offset = moveData.move.pSrcAllocation->GetOffset();
        if (offset != 0 && metadata->GetSumFreeSize() >= moveData.size)
        {
            AllocationRequest request = {};
            if (metadata->CreateAllocationRequest(
                moveData.size,
                moveData.alignment,
                false,
                ALLOCATION_FLAG_STRATEGY_MIN_OFFSET,
                &request))
            {
                if (metadata->GetAllocationOffset(request.allocHandle) < offset)
                {
                    if (SUCCEEDED(vector.CommitAllocationRequest(
                        request,
                        block,
                        moveData.size,
                        moveData.alignment,
                        moveData.allocDesc,
                        this,
                        &moveData.move.pDstTmpAllocation)))
                    {
                        m_Moves.push_back(moveData.move);
                        if (IncrementCounters(moveData.size))
                            return true;
                    }
                }
            }
        }
    }
    return false;
}

bool DefragmentationContextPimpl::AllocInOtherBlock(size_t start, size_t end, MoveAllocationData& data, BlockVector& vector)
{
    for (; start < end; ++start)
    {
        NormalBlock* dstBlock = vector.GetBlock(start);
        if (dstBlock->m_pMetadata->GetSumFreeSize() >= data.size)
        {
            if (SUCCEEDED(vector.AllocateFromBlock(dstBlock,
                data.size,
                data.alignment,
                data.flags,
                data.allocDesc,
                this,
                0,
                &data.move.pDstTmpAllocation)))
            {
                m_Moves.push_back(data.move);
                if (IncrementCounters(data.size))
                    return true;
                break;
            }
        }
    }
    return false;
}

bool DefragmentationContextPimpl::ComputeDefragmentation_Fast(BlockVector& vector)
{
    // Move only between blocks

    // Go through allocations in last blocks and try to fit them inside first ones
    for (size_t i = vector.GetBlockCount() - 1; i > m_ImmovableBlockCount; --i)
    {
        BlockMetadata* metadata = vector.GetBlock(i)->m_pMetadata;

        for (AllocHandle handle = metadata->GetAllocationListBegin();
            handle != (AllocHandle)0;
            handle = metadata->GetNextAllocation(handle))
        {
            MoveAllocationData moveData = GetMoveData(handle, metadata);
            // Ignore newly created allocations by defragmentation algorithm
            if (moveData.move.pSrcAllocation->GetPrivateData() == this)
                continue;
            switch (CheckCounters(moveData.move.pSrcAllocation->GetSize()))
            {
            case CounterStatus::Ignore:
                continue;
            case CounterStatus::End:
                return true;
            default:
                D3D12MA_ASSERT(0);
            case CounterStatus::Pass:
                break;
            }

            // Check all previous blocks for free space
            if (AllocInOtherBlock(0, i, moveData, vector))
                return true;
        }
    }
    return false;
}

bool DefragmentationContextPimpl::ComputeDefragmentation_Balanced(BlockVector& vector, size_t index, bool update)
{
    // Go over every allocation and try to fit it in previous blocks at lowest offsets,
    // if not possible: realloc within single block to minimize offset (exclude offset == 0),
    // but only if there are noticable gaps between them (some heuristic, ex. average size of allocation in block)
    D3D12MA_ASSERT(m_AlgorithmState != NULL);

    StateBalanced& vectorState = reinterpret_cast<StateBalanced*>(m_AlgorithmState)[index];
    if (update && vectorState.avgAllocSize == UINT64_MAX)
        UpdateVectorStatistics(vector, vectorState);

    const size_t startMoveCount = m_Moves.size();
    UINT64 minimalFreeRegion = vectorState.avgFreeSize / 2;
    for (size_t i = vector.GetBlockCount() - 1; i > m_ImmovableBlockCount; --i)
    {
        NormalBlock* block = vector.GetBlock(i);
        BlockMetadata* metadata = block->m_pMetadata;
        UINT64 prevFreeRegionSize = 0;

        for (AllocHandle handle = metadata->GetAllocationListBegin();
            handle != (AllocHandle)0;
            handle = metadata->GetNextAllocation(handle))
        {
            MoveAllocationData moveData = GetMoveData(handle, metadata);
            // Ignore newly created allocations by defragmentation algorithm
            if (moveData.move.pSrcAllocation->GetPrivateData() == this)
                continue;
            switch (CheckCounters(moveData.move.pSrcAllocation->GetSize()))
            {
            case CounterStatus::Ignore:
                continue;
            case CounterStatus::End:
                return true;
            default:
                D3D12MA_ASSERT(0);
            case CounterStatus::Pass:
                break;
            }

            // Check all previous blocks for free space
            const size_t prevMoveCount = m_Moves.size();
            if (AllocInOtherBlock(0, i, moveData, vector))
                return true;

            UINT64 nextFreeRegionSize = metadata->GetNextFreeRegionSize(handle);
            // If no room found then realloc within block for lower offset
            UINT64 offset = moveData.move.pSrcAllocation->GetOffset();
            if (prevMoveCount == m_Moves.size() && offset != 0 && metadata->GetSumFreeSize() >= moveData.size)
            {
                // Check if realloc will make sense
                if (prevFreeRegionSize >= minimalFreeRegion ||
                    nextFreeRegionSize >= minimalFreeRegion ||
                    moveData.size <= vectorState.avgFreeSize ||
                    moveData.size <= vectorState.avgAllocSize)
                {
                    AllocationRequest request = {};
                    if (metadata->CreateAllocationRequest(
                        moveData.size,
                        moveData.alignment,
                        false,
                        ALLOCATION_FLAG_STRATEGY_MIN_OFFSET,
                        &request))
                    {
                        if (metadata->GetAllocationOffset(request.allocHandle) < offset)
                        {
                            if (SUCCEEDED(vector.CommitAllocationRequest(
                                request,
                                block,
                                moveData.size,
                                moveData.alignment,
                                moveData.allocDesc,
                                this,
                                &moveData.move.pDstTmpAllocation)))
                            {
                                m_Moves.push_back(moveData.move);
                                if (IncrementCounters(moveData.size))
                                    return true;
                            }
                        }
                    }
                }
            }
            prevFreeRegionSize = nextFreeRegionSize;
        }
    }

    // No moves perfomed, update statistics to current vector state
    if (startMoveCount == m_Moves.size() && !update)
    {
        vectorState.avgAllocSize = UINT64_MAX;
        return ComputeDefragmentation_Balanced(vector, index, false);
    }
    return false;
}

bool DefragmentationContextPimpl::ComputeDefragmentation_Full(BlockVector& vector)
{
    // Go over every allocation and try to fit it in previous blocks at lowest offsets,
    // if not possible: realloc within single block to minimize offset (exclude offset == 0)

    for (size_t i = vector.GetBlockCount() - 1; i > m_ImmovableBlockCount; --i)
    {
        NormalBlock* block = vector.GetBlock(i);
        BlockMetadata* metadata = block->m_pMetadata;

        for (AllocHandle handle = metadata->GetAllocationListBegin();
            handle != (AllocHandle)0;
            handle = metadata->GetNextAllocation(handle))
        {
            MoveAllocationData moveData = GetMoveData(handle, metadata);
            // Ignore newly created allocations by defragmentation algorithm
            if (moveData.move.pSrcAllocation->GetPrivateData() == this)
                continue;
            switch (CheckCounters(moveData.move.pSrcAllocation->GetSize()))
            {
            case CounterStatus::Ignore:
                continue;
            case CounterStatus::End:
                return true;
            default:
                D3D12MA_ASSERT(0);
            case CounterStatus::Pass:
                break;
            }

            // Check all previous blocks for free space
            const size_t prevMoveCount = m_Moves.size();
            if (AllocInOtherBlock(0, i, moveData, vector))
                return true;

            // If no room found then realloc within block for lower offset
            UINT64 offset = moveData.move.pSrcAllocation->GetOffset();
            if (prevMoveCount == m_Moves.size() && offset != 0 && metadata->GetSumFreeSize() >= moveData.size)
            {
                AllocationRequest request = {};
                if (metadata->CreateAllocationRequest(
                    moveData.size,
                    moveData.alignment,
                    false,
                    ALLOCATION_FLAG_STRATEGY_MIN_OFFSET,
                    &request))
                {
                    if (metadata->GetAllocationOffset(request.allocHandle) < offset)
                    {
                        if (SUCCEEDED(vector.CommitAllocationRequest(
                            request,
                            block,
                            moveData.size,
                            moveData.alignment,
                            moveData.allocDesc,
                            this,
                            &moveData.move.pDstTmpAllocation)))
                        {
                            m_Moves.push_back(moveData.move);
                            if (IncrementCounters(moveData.size))
                                return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

void DefragmentationContextPimpl::UpdateVectorStatistics(BlockVector& vector, StateBalanced& state)
{
    size_t allocCount = 0;
    size_t freeCount = 0;
    state.avgFreeSize = 0;
    state.avgAllocSize = 0;

    for (size_t i = 0; i < vector.GetBlockCount(); ++i)
    {
        BlockMetadata* metadata = vector.GetBlock(i)->m_pMetadata;

        allocCount += metadata->GetAllocationCount();
        freeCount += metadata->GetFreeRegionsCount();
        state.avgFreeSize += metadata->GetSumFreeSize();
        state.avgAllocSize += metadata->GetSize();
    }

    state.avgAllocSize = (state.avgAllocSize - state.avgFreeSize) / allocCount;
    state.avgFreeSize /= freeCount;
}
#endif // _D3D12MA_DEFRAGMENTATION_CONTEXT_PIMPL_FUNCTIONS

#ifndef _D3D12MA_POOL_PIMPL_FUNCTIONS
PoolPimpl::PoolPimpl(AllocatorPimpl* allocator, const POOL_DESC& desc)
    : m_Allocator(allocator),
    m_Desc(desc),
    m_BlockVector(NULL),
    m_Name(NULL)
{
    const bool explicitBlockSize = desc.BlockSize != 0;
    const UINT64 preferredBlockSize = explicitBlockSize ? desc.BlockSize : D3D12MA_DEFAULT_BLOCK_SIZE;
    UINT maxBlockCount = desc.MaxBlockCount != 0 ? desc.MaxBlockCount : UINT_MAX;

#ifndef __ID3D12Device4_INTERFACE_DEFINED__
    D3D12MA_ASSERT(m_Desc.pProtectedSession == NULL);
#endif

    m_BlockVector = D3D12MA_NEW(allocator->GetAllocs(), BlockVector)(
        allocator, desc.HeapProperties, desc.HeapFlags,
        preferredBlockSize,
        desc.MinBlockCount, maxBlockCount,
        explicitBlockSize,
        D3D12MA_MAX(desc.MinAllocationAlignment, (UINT64)D3D12MA_DEBUG_ALIGNMENT),
        desc.Flags & POOL_FLAG_ALGORITHM_MASK,
        desc.Flags & POOL_FLAG_MSAA_TEXTURES_ALWAYS_COMMITTED,
        desc.pProtectedSession);
}

PoolPimpl::~PoolPimpl()
{
    D3D12MA_ASSERT(m_PrevPool == NULL && m_NextPool == NULL);
    FreeName();
    D3D12MA_DELETE(m_Allocator->GetAllocs(), m_BlockVector);
}

HRESULT PoolPimpl::Init()
{
    m_CommittedAllocations.Init(m_Allocator->UseMutex(), m_Desc.HeapProperties.Type, this);
    return m_BlockVector->CreateMinBlocks();
}

void PoolPimpl::GetStatistics(Statistics& outStats)
{
    ClearStatistics(outStats);
    m_BlockVector->AddStatistics(outStats);
    m_CommittedAllocations.AddStatistics(outStats);
}

void PoolPimpl::CalculateStatistics(DetailedStatistics& outStats)
{
    ClearDetailedStatistics(outStats);
    AddDetailedStatistics(outStats);
}

void PoolPimpl::AddDetailedStatistics(DetailedStatistics& inoutStats)
{
    m_BlockVector->AddDetailedStatistics(inoutStats);
    m_CommittedAllocations.AddDetailedStatistics(inoutStats);
}

void PoolPimpl::SetName(LPCWSTR Name)
{
    FreeName();

    if (Name)
    {
        const size_t nameCharCount = wcslen(Name) + 1;
        m_Name = D3D12MA_NEW_ARRAY(m_Allocator->GetAllocs(), WCHAR, nameCharCount);
        memcpy(m_Name, Name, nameCharCount * sizeof(WCHAR));
    }
}

void PoolPimpl::FreeName()
{
    if (m_Name)
    {
        const size_t nameCharCount = wcslen(m_Name) + 1;
        D3D12MA_DELETE_ARRAY(m_Allocator->GetAllocs(), m_Name, nameCharCount);
        m_Name = NULL;
    }
}
#endif // _D3D12MA_POOL_PIMPL_FUNCTIONS


#ifndef _D3D12MA_PUBLIC_INTERFACE
HRESULT CreateAllocator(const ALLOCATOR_DESC* pDesc, Allocator** ppAllocator)
{
    if (!pDesc || !ppAllocator || !pDesc->pDevice || !pDesc->pAdapter ||
        !(pDesc->PreferredBlockSize == 0 || (pDesc->PreferredBlockSize >= 16 && pDesc->PreferredBlockSize < 0x10000000000ull)))
    {
        D3D12MA_ASSERT(0 && "Invalid arguments passed to CreateAllocator.");
        return E_INVALIDARG;
    }

    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK

    ALLOCATION_CALLBACKS allocationCallbacks;
    SetupAllocationCallbacks(allocationCallbacks, pDesc->pAllocationCallbacks);

    *ppAllocator = D3D12MA_NEW(allocationCallbacks, Allocator)(allocationCallbacks, *pDesc);
    HRESULT hr = (*ppAllocator)->m_Pimpl->Init(*pDesc);
    if (FAILED(hr))
    {
        D3D12MA_DELETE(allocationCallbacks, *ppAllocator);
        *ppAllocator = NULL;
    }
    return hr;
}

HRESULT CreateVirtualBlock(const VIRTUAL_BLOCK_DESC* pDesc, VirtualBlock** ppVirtualBlock)
{
    if (!pDesc || !ppVirtualBlock)
    {
        D3D12MA_ASSERT(0 && "Invalid arguments passed to CreateVirtualBlock.");
        return E_INVALIDARG;
    }

    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK

    ALLOCATION_CALLBACKS allocationCallbacks;
    SetupAllocationCallbacks(allocationCallbacks, pDesc->pAllocationCallbacks);

    *ppVirtualBlock = D3D12MA_NEW(allocationCallbacks, VirtualBlock)(allocationCallbacks, *pDesc);
    return S_OK;
}

#ifndef _D3D12MA_IUNKNOWN_IMPL_FUNCTIONS
HRESULT STDMETHODCALLTYPE IUnknownImpl::QueryInterface(REFIID riid, void** ppvObject)
{
    if (ppvObject == NULL)
        return E_POINTER;
    if (riid == IID_IUnknown)
    {
        ++m_RefCount;
        *ppvObject = this;
        return S_OK;
    }
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE IUnknownImpl::AddRef()
{
    return ++m_RefCount;
}

ULONG STDMETHODCALLTYPE IUnknownImpl::Release()
{
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK

        const uint32_t newRefCount = --m_RefCount;
    if (newRefCount == 0)
        ReleaseThis();
    return newRefCount;
}
#endif // _D3D12MA_IUNKNOWN_IMPL_FUNCTIONS

#ifndef _D3D12MA_ALLOCATION_FUNCTIONS
void Allocation::PackedData::SetType(Type type)
{
    const UINT u = (UINT)type;
    D3D12MA_ASSERT(u < (1u << 2));
    m_Type = u;
}

void Allocation::PackedData::SetResourceDimension(D3D12_RESOURCE_DIMENSION resourceDimension)
{
    const UINT u = (UINT)resourceDimension;
    D3D12MA_ASSERT(u < (1u << 3));
    m_ResourceDimension = u;
}

void Allocation::PackedData::SetResourceFlags(D3D12_RESOURCE_FLAGS resourceFlags)
{
    const UINT u = (UINT)resourceFlags;
    D3D12MA_ASSERT(u < (1u << 24));
    m_ResourceFlags = u;
}

void Allocation::PackedData::SetTextureLayout(D3D12_TEXTURE_LAYOUT textureLayout)
{
    const UINT u = (UINT)textureLayout;
    D3D12MA_ASSERT(u < (1u << 9));
    m_TextureLayout = u;
}

UINT64 Allocation::GetOffset() const
{
    switch (m_PackedData.GetType())
    {
    case TYPE_COMMITTED:
    case TYPE_HEAP:
        return 0;
    case TYPE_PLACED:
        return m_Placed.block->m_pMetadata->GetAllocationOffset(m_Placed.allocHandle);
    default:
        D3D12MA_ASSERT(0);
        return 0;
    }
}

void Allocation::SetResource(ID3D12Resource* pResource)
{
    if (pResource != m_Resource)
    {
        if (m_Resource)
            m_Resource->Release();
        m_Resource = pResource;
        if (m_Resource)
            m_Resource->AddRef();
    }
}

ID3D12Heap* Allocation::GetHeap() const
{
    switch (m_PackedData.GetType())
    {
    case TYPE_COMMITTED:
        return NULL;
    case TYPE_PLACED:
        return m_Placed.block->GetHeap();
    case TYPE_HEAP:
        return m_Heap.heap;
    default:
        D3D12MA_ASSERT(0);
        return 0;
    }
}

void Allocation::SetName(LPCWSTR Name)
{
    FreeName();

    if (Name)
    {
        const size_t nameCharCount = wcslen(Name) + 1;
        m_Name = D3D12MA_NEW_ARRAY(m_Allocator->GetAllocs(), WCHAR, nameCharCount);
        memcpy(m_Name, Name, nameCharCount * sizeof(WCHAR));
    }
}

void Allocation::ReleaseThis()
{
    if (this == NULL)
    {
        return;
    }

    SAFE_RELEASE(m_Resource);

    switch (m_PackedData.GetType())
    {
    case TYPE_COMMITTED:
        m_Allocator->FreeCommittedMemory(this);
        break;
    case TYPE_PLACED:
        m_Allocator->FreePlacedMemory(this);
        break;
    case TYPE_HEAP:
        m_Allocator->FreeHeapMemory(this);
        break;
    }

    FreeName();

    m_Allocator->GetAllocationObjectAllocator().Free(this);
}

Allocation::Allocation(AllocatorPimpl* allocator, UINT64 size, UINT64 alignment, BOOL wasZeroInitialized)
    : m_Allocator{ allocator },
    m_Size{ size },
    m_Alignment{ alignment },
    m_Resource{ NULL },
    m_Name{ NULL }
{
    D3D12MA_ASSERT(allocator);

    m_PackedData.SetType(TYPE_COUNT);
    m_PackedData.SetResourceDimension(D3D12_RESOURCE_DIMENSION_UNKNOWN);
    m_PackedData.SetResourceFlags(D3D12_RESOURCE_FLAG_NONE);
    m_PackedData.SetTextureLayout(D3D12_TEXTURE_LAYOUT_UNKNOWN);
    m_PackedData.SetWasZeroInitialized(wasZeroInitialized);
}

void Allocation::InitCommitted(CommittedAllocationList* list)
{
    m_PackedData.SetType(TYPE_COMMITTED);
    m_Committed.list = list;
    m_Committed.prev = NULL;
    m_Committed.next = NULL;
}

void Allocation::InitPlaced(AllocHandle allocHandle, NormalBlock* block)
{
    m_PackedData.SetType(TYPE_PLACED);
    m_Placed.allocHandle = allocHandle;
    m_Placed.block = block;
}

void Allocation::InitHeap(CommittedAllocationList* list, ID3D12Heap* heap)
{
    m_PackedData.SetType(TYPE_HEAP);
    m_Heap.list = list;
    m_Committed.prev = NULL;
    m_Committed.next = NULL;
    m_Heap.heap = heap;
}

void Allocation::SwapBlockAllocation(Allocation* allocation)
{
    D3D12MA_ASSERT(allocation != NULL);
    D3D12MA_ASSERT(m_PackedData.GetType() == TYPE_PLACED);
    D3D12MA_ASSERT(allocation->m_PackedData.GetType() == TYPE_PLACED);

    D3D12MA_SWAP(m_Resource, allocation->m_Resource);
    m_PackedData.SetWasZeroInitialized(allocation->m_PackedData.WasZeroInitialized());
    m_Placed.block->m_pMetadata->SetAllocationPrivateData(m_Placed.allocHandle, allocation);
    D3D12MA_SWAP(m_Placed, allocation->m_Placed);
    m_Placed.block->m_pMetadata->SetAllocationPrivateData(m_Placed.allocHandle, this);
}

AllocHandle Allocation::GetAllocHandle() const
{
    switch (m_PackedData.GetType())
    {
    case TYPE_COMMITTED:
    case TYPE_HEAP:
        return (AllocHandle)0;
    case TYPE_PLACED:
        return m_Placed.allocHandle;
    default:
        D3D12MA_ASSERT(0);
        return (AllocHandle)0;
    }
}

NormalBlock* Allocation::GetBlock()
{
    switch (m_PackedData.GetType())
    {
    case TYPE_COMMITTED:
    case TYPE_HEAP:
        return NULL;
    case TYPE_PLACED:
        return m_Placed.block;
    default:
        D3D12MA_ASSERT(0);
        return NULL;
    }
}

template<typename D3D12_RESOURCE_DESC_T>
void Allocation::SetResourcePointer(ID3D12Resource* resource, const D3D12_RESOURCE_DESC_T* pResourceDesc)
{
    D3D12MA_ASSERT(m_Resource == NULL && pResourceDesc);
    m_Resource = resource;
    m_PackedData.SetResourceDimension(pResourceDesc->Dimension);
    m_PackedData.SetResourceFlags(pResourceDesc->Flags);
    m_PackedData.SetTextureLayout(pResourceDesc->Layout);
}

void Allocation::FreeName()
{
    if (m_Name)
    {
        const size_t nameCharCount = wcslen(m_Name) + 1;
        D3D12MA_DELETE_ARRAY(m_Allocator->GetAllocs(), m_Name, nameCharCount);
        m_Name = NULL;
    }
}
#endif // _D3D12MA_ALLOCATION_FUNCTIONS

#ifndef _D3D12MA_DEFRAGMENTATION_CONTEXT_FUNCTIONS
HRESULT DefragmentationContext::BeginPass(DEFRAGMENTATION_PASS_MOVE_INFO* pPassInfo)
{
    D3D12MA_ASSERT(pPassInfo);
    return m_Pimpl->DefragmentPassBegin(*pPassInfo);
}

HRESULT DefragmentationContext::EndPass(DEFRAGMENTATION_PASS_MOVE_INFO* pPassInfo)
{
    D3D12MA_ASSERT(pPassInfo);
    return m_Pimpl->DefragmentPassEnd(*pPassInfo);
}

void DefragmentationContext::GetStats(DEFRAGMENTATION_STATS* pStats)
{
    D3D12MA_ASSERT(pStats);
    m_Pimpl->GetStats(*pStats);
}

void DefragmentationContext::ReleaseThis()
{
    if (this == NULL)
    {
        return;
    }

    D3D12MA_DELETE(m_Pimpl->GetAllocs(), this);
}

DefragmentationContext::DefragmentationContext(AllocatorPimpl* allocator,
    const DEFRAGMENTATION_DESC& desc,
    BlockVector* poolVector)
    : m_Pimpl(D3D12MA_NEW(allocator->GetAllocs(), DefragmentationContextPimpl)(allocator, desc, poolVector)) {}

DefragmentationContext::~DefragmentationContext()
{
    D3D12MA_DELETE(m_Pimpl->GetAllocs(), m_Pimpl);
}
#endif // _D3D12MA_DEFRAGMENTATION_CONTEXT_FUNCTIONS

#ifndef _D3D12MA_POOL_FUNCTIONS
POOL_DESC Pool::GetDesc() const
{
    return m_Pimpl->GetDesc();
}

void Pool::GetStatistics(Statistics* pStats)
{
    D3D12MA_ASSERT(pStats);
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
        m_Pimpl->GetStatistics(*pStats);
}

void Pool::CalculateStatistics(DetailedStatistics* pStats)
{
    D3D12MA_ASSERT(pStats);
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
        m_Pimpl->CalculateStatistics(*pStats);
}

void Pool::SetName(LPCWSTR Name)
{
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
        m_Pimpl->SetName(Name);
}

LPCWSTR Pool::GetName() const
{
    return m_Pimpl->GetName();
}

HRESULT Pool::BeginDefragmentation(const DEFRAGMENTATION_DESC* pDesc, DefragmentationContext** ppContext)
{
    D3D12MA_ASSERT(pDesc && ppContext);

    // Check for support
    if (m_Pimpl->GetBlockVector()->GetAlgorithm() & POOL_FLAG_ALGORITHM_LINEAR)
        return E_NOINTERFACE;

    AllocatorPimpl* allocator = m_Pimpl->GetAllocator();
    *ppContext = D3D12MA_NEW(allocator->GetAllocs(), DefragmentationContext)(allocator, *pDesc, m_Pimpl->GetBlockVector());
    return S_OK;
}

void Pool::ReleaseThis()
{
    if (this == NULL)
    {
        return;
    }

    D3D12MA_DELETE(m_Pimpl->GetAllocator()->GetAllocs(), this);
}

Pool::Pool(Allocator* allocator, const POOL_DESC& desc)
    : m_Pimpl(D3D12MA_NEW(allocator->m_Pimpl->GetAllocs(), PoolPimpl)(allocator->m_Pimpl, desc)) {}

Pool::~Pool()
{
    m_Pimpl->GetAllocator()->UnregisterPool(this, m_Pimpl->GetDesc().HeapProperties.Type);

    D3D12MA_DELETE(m_Pimpl->GetAllocator()->GetAllocs(), m_Pimpl);
}
#endif // _D3D12MA_POOL_FUNCTIONS

#ifndef _D3D12MA_ALLOCATOR_FUNCTIONS
const D3D12_FEATURE_DATA_D3D12_OPTIONS& Allocator::GetD3D12Options() const
{
    return m_Pimpl->GetD3D12Options();
}

BOOL Allocator::IsUMA() const
{
    return m_Pimpl->IsUMA();
}

BOOL Allocator::IsCacheCoherentUMA() const
{
    return m_Pimpl->IsCacheCoherentUMA();
}

UINT64 Allocator::GetMemoryCapacity(UINT memorySegmentGroup) const
{
    return m_Pimpl->GetMemoryCapacity(memorySegmentGroup);
}

HRESULT Allocator::CreateResource(
    const ALLOCATION_DESC* pAllocDesc,
    const D3D12_RESOURCE_DESC* pResourceDesc,
    D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue,
    Allocation** ppAllocation,
    REFIID riidResource,
    void** ppvResource)
{
    if (!pAllocDesc || !pResourceDesc || !ppAllocation)
    {
        D3D12MA_ASSERT(0 && "Invalid arguments passed to Allocator::CreateResource.");
        return E_INVALIDARG;
    }
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
        return m_Pimpl->CreateResource(pAllocDesc, pResourceDesc, InitialResourceState, pOptimizedClearValue, ppAllocation, riidResource, ppvResource);
}

#ifdef __ID3D12Device8_INTERFACE_DEFINED__
HRESULT Allocator::CreateResource2(
    const ALLOCATION_DESC* pAllocDesc,
    const D3D12_RESOURCE_DESC1* pResourceDesc,
    D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue,
    Allocation** ppAllocation,
    REFIID riidResource,
    void** ppvResource)
{
    if (!pAllocDesc || !pResourceDesc || !ppAllocation)
    {
        D3D12MA_ASSERT(0 && "Invalid arguments passed to Allocator::CreateResource2.");
        return E_INVALIDARG;
    }
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
        return m_Pimpl->CreateResource2(pAllocDesc, pResourceDesc, InitialResourceState, pOptimizedClearValue, ppAllocation, riidResource, ppvResource);
}
#endif // #ifdef __ID3D12Device8_INTERFACE_DEFINED__

HRESULT Allocator::AllocateMemory(
    const ALLOCATION_DESC* pAllocDesc,
    const D3D12_RESOURCE_ALLOCATION_INFO* pAllocInfo,
    Allocation** ppAllocation)
{
    if (!ValidateAllocateMemoryParameters(pAllocDesc, pAllocInfo, ppAllocation))
    {
        D3D12MA_ASSERT(0 && "Invalid arguments passed to Allocator::AllocateMemory.");
        return E_INVALIDARG;
    }
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
        return m_Pimpl->AllocateMemory(pAllocDesc, pAllocInfo, ppAllocation);
}

HRESULT Allocator::CreateAliasingResource(
    Allocation* pAllocation,
    UINT64 AllocationLocalOffset,
    const D3D12_RESOURCE_DESC* pResourceDesc,
    D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue,
    REFIID riidResource,
    void** ppvResource)
{
    if (!pAllocation || !pResourceDesc || !ppvResource)
    {
        D3D12MA_ASSERT(0 && "Invalid arguments passed to Allocator::CreateAliasingResource.");
        return E_INVALIDARG;
    }
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
        return m_Pimpl->CreateAliasingResource(pAllocation, AllocationLocalOffset, pResourceDesc, InitialResourceState, pOptimizedClearValue, riidResource, ppvResource);
}

HRESULT Allocator::CreatePool(
    const POOL_DESC* pPoolDesc,
    Pool** ppPool)
{
    if (!pPoolDesc || !ppPool ||
        (pPoolDesc->MaxBlockCount > 0 && pPoolDesc->MaxBlockCount < pPoolDesc->MinBlockCount) ||
        (pPoolDesc->MinAllocationAlignment > 0 && !IsPow2(pPoolDesc->MinAllocationAlignment)))
    {
        D3D12MA_ASSERT(0 && "Invalid arguments passed to Allocator::CreatePool.");
        return E_INVALIDARG;
    }
    if (!m_Pimpl->HeapFlagsFulfillResourceHeapTier(pPoolDesc->HeapFlags))
    {
        D3D12MA_ASSERT(0 && "Invalid pPoolDesc->HeapFlags passed to Allocator::CreatePool. Did you forget to handle ResourceHeapTier=1?");
        return E_INVALIDARG;
    }
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
        * ppPool = D3D12MA_NEW(m_Pimpl->GetAllocs(), Pool)(this, *pPoolDesc);
    HRESULT hr = (*ppPool)->m_Pimpl->Init();
    if (SUCCEEDED(hr))
    {
        m_Pimpl->RegisterPool(*ppPool, pPoolDesc->HeapProperties.Type);
    }
    else
    {
        D3D12MA_DELETE(m_Pimpl->GetAllocs(), *ppPool);
        *ppPool = NULL;
    }
    return hr;
}

void Allocator::SetCurrentFrameIndex(UINT frameIndex)
{
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
        m_Pimpl->SetCurrentFrameIndex(frameIndex);
}

void Allocator::GetBudget(Budget* pLocalBudget, Budget* pNonLocalBudget)
{
    if (pLocalBudget == NULL && pNonLocalBudget == NULL)
    {
        return;
    }
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
        m_Pimpl->GetBudget(pLocalBudget, pNonLocalBudget);
}

void Allocator::CalculateStatistics(TotalStatistics* pStats)
{
    D3D12MA_ASSERT(pStats);
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
        m_Pimpl->CalculateStatistics(*pStats);
}

void Allocator::BuildStatsString(WCHAR** ppStatsString, BOOL DetailedMap) const
{
    D3D12MA_ASSERT(ppStatsString);
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
        m_Pimpl->BuildStatsString(ppStatsString, DetailedMap);
}

void Allocator::FreeStatsString(WCHAR* pStatsString) const
{
    if (pStatsString != NULL)
    {
        D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
            m_Pimpl->FreeStatsString(pStatsString);
    }
}

void Allocator::BeginDefragmentation(const DEFRAGMENTATION_DESC* pDesc, DefragmentationContext** ppContext)
{
    D3D12MA_ASSERT(pDesc && ppContext);

    *ppContext = D3D12MA_NEW(m_Pimpl->GetAllocs(), DefragmentationContext)(m_Pimpl, *pDesc, NULL);
}

void Allocator::ReleaseThis()
{
    // Copy is needed because otherwise we would call destructor and invalidate the structure with callbacks before using it to free memory.
    const ALLOCATION_CALLBACKS allocationCallbacksCopy = m_Pimpl->GetAllocs();
    D3D12MA_DELETE(allocationCallbacksCopy, this);
}

Allocator::Allocator(const ALLOCATION_CALLBACKS& allocationCallbacks, const ALLOCATOR_DESC& desc)
    : m_Pimpl(D3D12MA_NEW(allocationCallbacks, AllocatorPimpl)(allocationCallbacks, desc)) {}

Allocator::~Allocator()
{
    D3D12MA_DELETE(m_Pimpl->GetAllocs(), m_Pimpl);
}
#endif // _D3D12MA_ALLOCATOR_FUNCTIONS

#ifndef _D3D12MA_VIRTUAL_BLOCK_FUNCTIONS
BOOL VirtualBlock::IsEmpty() const
{
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK

        return m_Pimpl->m_Metadata->IsEmpty() ? TRUE : FALSE;
}

void VirtualBlock::GetAllocationInfo(VirtualAllocation allocation, VIRTUAL_ALLOCATION_INFO* pInfo) const
{
    D3D12MA_ASSERT(allocation.AllocHandle != (AllocHandle)0 && pInfo);

    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK

        m_Pimpl->m_Metadata->GetAllocationInfo(allocation.AllocHandle, *pInfo);
}

HRESULT VirtualBlock::Allocate(const VIRTUAL_ALLOCATION_DESC* pDesc, VirtualAllocation* pAllocation, UINT64* pOffset)
{
    if (!pDesc || !pAllocation || pDesc->Size == 0 || !IsPow2(pDesc->Alignment))
    {
        D3D12MA_ASSERT(0 && "Invalid arguments passed to VirtualBlock::Allocate.");
        return E_INVALIDARG;
    }

    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK

        const UINT64 alignment = pDesc->Alignment != 0 ? pDesc->Alignment : 1;
    AllocationRequest allocRequest = {};
    if (m_Pimpl->m_Metadata->CreateAllocationRequest(
        pDesc->Size,
        alignment,
        pDesc->Flags & VIRTUAL_ALLOCATION_FLAG_UPPER_ADDRESS,
        pDesc->Flags & VIRTUAL_ALLOCATION_FLAG_STRATEGY_MASK,
        &allocRequest))
    {
        m_Pimpl->m_Metadata->Alloc(allocRequest, pDesc->Size, pDesc->pPrivateData);
        D3D12MA_HEAVY_ASSERT(m_Pimpl->m_Metadata->Validate());
        pAllocation->AllocHandle = allocRequest.allocHandle;

        if (pOffset)
            *pOffset = m_Pimpl->m_Metadata->GetAllocationOffset(allocRequest.allocHandle);
        return S_OK;
    }

    pAllocation->AllocHandle = (AllocHandle)0;
    if (pOffset)
        *pOffset = UINT64_MAX;

    return E_OUTOFMEMORY;
}

void VirtualBlock::FreeAllocation(VirtualAllocation allocation)
{
    if (allocation.AllocHandle == (AllocHandle)0)
        return;

    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK

        m_Pimpl->m_Metadata->Free(allocation.AllocHandle);
    D3D12MA_HEAVY_ASSERT(m_Pimpl->m_Metadata->Validate());
}

void VirtualBlock::Clear()
{
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK

        m_Pimpl->m_Metadata->Clear();
    D3D12MA_HEAVY_ASSERT(m_Pimpl->m_Metadata->Validate());
}

void VirtualBlock::SetAllocationPrivateData(VirtualAllocation allocation, void* pPrivateData)
{
    D3D12MA_ASSERT(allocation.AllocHandle != (AllocHandle)0);

    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK

        m_Pimpl->m_Metadata->SetAllocationPrivateData(allocation.AllocHandle, pPrivateData);
}

void VirtualBlock::GetStatistics(Statistics* pStats) const
{
    D3D12MA_ASSERT(pStats);
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
        D3D12MA_HEAVY_ASSERT(m_Pimpl->m_Metadata->Validate());
    ClearStatistics(*pStats);
    m_Pimpl->m_Metadata->AddStatistics(*pStats);
}

void VirtualBlock::CalculateStatistics(DetailedStatistics* pStats) const
{
    D3D12MA_ASSERT(pStats);
    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
        D3D12MA_HEAVY_ASSERT(m_Pimpl->m_Metadata->Validate());
    ClearDetailedStatistics(*pStats);
    m_Pimpl->m_Metadata->AddDetailedStatistics(*pStats);
}

void VirtualBlock::BuildStatsString(WCHAR** ppStatsString) const
{
    D3D12MA_ASSERT(ppStatsString);

    D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK

        StringBuilder sb(m_Pimpl->m_AllocationCallbacks);
    {
        JsonWriter json(m_Pimpl->m_AllocationCallbacks, sb);
        D3D12MA_HEAVY_ASSERT(m_Pimpl->m_Metadata->Validate());
        m_Pimpl->m_Metadata->WriteAllocationInfoToJson(json);
    } // Scope for JsonWriter

    const size_t length = sb.GetLength();
    WCHAR* result = AllocateArray<WCHAR>(m_Pimpl->m_AllocationCallbacks, length + 1);
    memcpy(result, sb.GetData(), length * sizeof(WCHAR));
    result[length] = L'\0';
    *ppStatsString = result;
}

void VirtualBlock::FreeStatsString(WCHAR* pStatsString) const
{
    if (pStatsString != NULL)
    {
        D3D12MA_DEBUG_GLOBAL_MUTEX_LOCK
            D3D12MA::Free(m_Pimpl->m_AllocationCallbacks, pStatsString);
    }
}

void VirtualBlock::ReleaseThis()
{
    // Copy is needed because otherwise we would call destructor and invalidate the structure with callbacks before using it to free memory.
    const ALLOCATION_CALLBACKS allocationCallbacksCopy = m_Pimpl->m_AllocationCallbacks;
    D3D12MA_DELETE(allocationCallbacksCopy, this);
}

VirtualBlock::VirtualBlock(const ALLOCATION_CALLBACKS& allocationCallbacks, const VIRTUAL_BLOCK_DESC& desc)
    : m_Pimpl(D3D12MA_NEW(allocationCallbacks, VirtualBlockPimpl)(allocationCallbacks, desc)) {}

VirtualBlock::~VirtualBlock()
{
    // THIS IS AN IMPORTANT ASSERT!
    // Hitting it means you have some memory leak - unreleased allocations in this virtual block.
    D3D12MA_ASSERT(m_Pimpl->m_Metadata->IsEmpty() && "Some allocations were not freed before destruction of this virtual block!");

    D3D12MA_DELETE(m_Pimpl->m_AllocationCallbacks, m_Pimpl);
}
#endif // _D3D12MA_VIRTUAL_BLOCK_FUNCTIONS
#endif // _D3D12MA_PUBLIC_INTERFACE
} // namespace D3D12MA
#endif

/**
\page quick_start Quick start

\section quick_start_project_setup Project setup and initialization

This is a small, standalone C++ library. It consists of a pair of 2 files:
"D3D12MemAlloc.h" header file with public interface and "D3D12MemAlloc.cpp" with
internal implementation. The only external dependencies are WinAPI, Direct3D 12,
and parts of C/C++ standard library (but STL containers, exceptions, or RTTI are
not used).

The library is developed and tested using Microsoft Visual Studio 2019, but it
should work with other compilers as well. It is designed for 64-bit code.

To use the library in your project:

(1.) Copy files `D3D12MemAlloc.cpp`, `%D3D12MemAlloc.h` to your project.

(2.) Make `D3D12MemAlloc.cpp` compiling as part of the project, as C++ code.

(3.) Include library header in each CPP file that needs to use the library.

\code
#include "D3D12MemAlloc.h"
\endcode

(4.) Right after you created `ID3D12Device`, fill D3D12MA::ALLOCATOR_DESC
structure and call function D3D12MA::CreateAllocator to create the main
D3D12MA::Allocator object.

Please note that all symbols of the library are declared inside #D3D12MA namespace.

\code
IDXGIAdapter* adapter = (...)
ID3D12Device* device = (...)

D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
allocatorDesc.pDevice = device;
allocatorDesc.pAdapter = adapter;

D3D12MA::Allocator* allocator;
HRESULT hr = D3D12MA::CreateAllocator(&allocatorDesc, &allocator);
\endcode

(5.) Right before destroying the D3D12 device, destroy the allocator object.

Objects of this library must be destroyed by calling `Release` method.
They are somewhat compatible with COM: they implement `IUnknown` interface with its virtual methods: `AddRef`, `Release`, `QueryInterface`,
and they are reference-counted internally.
You can use smart pointers designed for COM with objects of this library - e.g. `CComPtr` or `Microsoft::WRL::ComPtr`.
The reference counter is thread-safe.
`QueryInterface` method supports only `IUnknown`, as classes of this library don't define their own GUIDs.

\code
allocator->Release();
\endcode


\section quick_start_creating_resources Creating resources

To use the library for creating resources (textures and buffers), call method
D3D12MA::Allocator::CreateResource in the place where you would previously call
`ID3D12Device::CreateCommittedResource`.

The function has similar syntax, but it expects structure D3D12MA::ALLOCATION_DESC
to be passed along with `D3D12_RESOURCE_DESC` and other parameters for created
resource. This structure describes parameters of the desired memory allocation,
including choice of `D3D12_HEAP_TYPE`.

The function returns a new object of type D3D12MA::Allocation.
It represents allocated memory and can be queried for size, offset, `ID3D12Heap`.
It also holds a reference to the `ID3D12Resource`, which can be accessed by calling D3D12MA::Allocation::GetResource().

\code
D3D12_RESOURCE_DESC resourceDesc = {};
resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
resourceDesc.Alignment = 0;
resourceDesc.Width = 1024;
resourceDesc.Height = 1024;
resourceDesc.DepthOrArraySize = 1;
resourceDesc.MipLevels = 1;
resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
resourceDesc.SampleDesc.Count = 1;
resourceDesc.SampleDesc.Quality = 0;
resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

D3D12MA::ALLOCATION_DESC allocationDesc = {};
allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

D3D12MA::Allocation* allocation;
HRESULT hr = allocator->CreateResource(
    &allocationDesc,
    &resourceDesc,
    D3D12_RESOURCE_STATE_COPY_DEST,
    NULL,
    &allocation,
    IID_NULL, NULL);

// Use allocation->GetResource()...
\endcode

You need to release the allocation object when no longer needed.
This will also release the D3D12 resource.

\code
allocation->Release();
\endcode

The advantage of using the allocator instead of creating committed resource, and
the main purpose of this library, is that it can decide to allocate bigger memory
heap internally using `ID3D12Device::CreateHeap` and place multiple resources in
it, at different offsets, using `ID3D12Device::CreatePlacedResource`. The library
manages its own collection of allocated memory blocks (heaps) and remembers which
parts of them are occupied and which parts are free to be used for new resources.

It is important to remember that resources created as placed don't have their memory
initialized to zeros, but may contain garbage data, so they need to be fully initialized
before usage, e.g. using Clear (`ClearRenderTargetView`), Discard (`DiscardResource`),
or copy (`CopyResource`).

The library also automatically handles resource heap tier.
When `D3D12_FEATURE_DATA_D3D12_OPTIONS::ResourceHeapTier` equals `D3D12_RESOURCE_HEAP_TIER_1`,
resources of 3 types: buffers, textures that are render targets or depth-stencil,
and other textures must be kept in separate heaps. When `D3D12_RESOURCE_HEAP_TIER_2`,
they can be kept together. By using this library, you don't need to handle this
manually.


\section quick_start_resource_reference_counting Resource reference counting

`ID3D12Resource` and other interfaces of Direct3D 12 use COM, so they are reference-counted.
Objects of this library are reference-counted as well.
An object of type D3D12MA::Allocation remembers the resource (buffer or texture)
that was created together with this memory allocation
and holds a reference to the `ID3D12Resource` object.
(Note this is a difference to Vulkan Memory Allocator, where a `VmaAllocation` object has no connection
with the buffer or image that was created with it.)
Thus, it is important to manage the resource reference counter properly.

<b>The simplest use case</b> is shown in the code snippet above.
When only D3D12MA::Allocation object is obtained from a function call like D3D12MA::Allocator::CreateResource,
it remembers the `ID3D12Resource` that was created with it and holds a reference to it.
The resource can be obtained by calling `allocation->GetResource()`, which doesn't increment the resource
reference counter.
Calling `allocation->Release()` will decrease the resource reference counter, which is = 1 in this case,
so the resource will be released.

<b>Second option</b> is to retrieve a pointer to the resource along with D3D12MA::Allocation.
Last parameters of the resource creation function can be used for this purpose.

\code
D3D12MA::Allocation* allocation;
ID3D12Resource* resource;
HRESULT hr = allocator->CreateResource(
    &allocationDesc,
    &resourceDesc,
    D3D12_RESOURCE_STATE_COPY_DEST,
    NULL,
    &allocation,
    IID_PPV_ARGS(&resource));

// Use resource...
\endcode

In this case, returned pointer `resource` is equal to `allocation->GetResource()`,
but the creation function additionally increases resource reference counter for the purpose of returning it from this call
(it actually calls `QueryInterface` internally), so the resource will have the counter = 2.
The resource then need to be released along with the allocation, in this particular order,
to make sure the resource is destroyed before its memory heap can potentially be freed.

\code
resource->Release();
allocation->Release();
\endcode

<b>More advanced use cases</b> are possible when we consider that an D3D12MA::Allocation object can just hold
a reference to any resource.
It can be changed by calling D3D12MA::Allocation::SetResource. This function
releases the old resource and calls `AddRef` on the new one.

Special care must be taken when performing <b>defragmentation</b>.
The new resource created at the destination place should be set as `pass.pMoves[i].pDstTmpAllocation->SetResource(newRes)`,
but it is moved to the source allocation at end of the defragmentation pass,
while the old resource accessible through `pass.pMoves[i].pSrcAllocation->GetResource()` is then released.
For more information, see documentation chapter \ref defragmentation.


\section quick_start_mapping_memory Mapping memory

The process of getting regular CPU-side pointer to the memory of a resource in
Direct3D is called "mapping". There are rules and restrictions to this process,
as described in D3D12 documentation of `ID3D12Resource::Map` method.

Mapping happens on the level of particular resources, not entire memory heaps,
and so it is out of scope of this library. Just as the documentation of the `Map` function says:

- Returned pointer refers to data of particular subresource, not entire memory heap.
- You can map same resource multiple times. It is ref-counted internally.
- Mapping is thread-safe.
- Unmapping is not required before resource destruction.
- Unmapping may not be required before using written data - some heap types on
  some platforms support resources persistently mapped.

When using this library, you can map and use your resources normally without
considering whether they are created as committed resources or placed resources in one large heap.

Example for buffer created and filled in `UPLOAD` heap type:

\code
const UINT64 bufSize = 65536;
const float* bufData = (...);

D3D12_RESOURCE_DESC resourceDesc = {};
resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
resourceDesc.Alignment = 0;
resourceDesc.Width = bufSize;
resourceDesc.Height = 1;
resourceDesc.DepthOrArraySize = 1;
resourceDesc.MipLevels = 1;
resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
resourceDesc.SampleDesc.Count = 1;
resourceDesc.SampleDesc.Quality = 0;
resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

D3D12MA::ALLOCATION_DESC allocationDesc = {};
allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

D3D12Resource* resource;
D3D12MA::Allocation* allocation;
HRESULT hr = allocator->CreateResource(
    &allocationDesc,
    &resourceDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    NULL,
    &allocation,
    IID_PPV_ARGS(&resource));

void* mappedPtr;
hr = resource->Map(0, NULL, &mappedPtr);

memcpy(mappedPtr, bufData, bufSize);

resource->Unmap(0, NULL);
\endcode


\page custom_pools Custom memory pools

A "pool" is a collection of memory blocks that share certain properties.
Allocator creates 3 default pools: for `D3D12_HEAP_TYPE_DEFAULT`, `UPLOAD`, `READBACK`.
A default pool automatically grows in size. Size of allocated blocks is also variable and managed automatically.
Typical allocations are created in these pools. You can also create custom pools.

\section custom_pools_usage Usage

To create a custom pool, fill in structure D3D12MA::POOL_DESC and call function D3D12MA::Allocator::CreatePool
to obtain object D3D12MA::Pool. Example:

\code
POOL_DESC poolDesc = {};
poolDesc.HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

Pool* pool;
HRESULT hr = allocator->CreatePool(&poolDesc, &pool);
\endcode

To allocate resources out of a custom pool, only set member D3D12MA::ALLOCATION_DESC::CustomPool.
Example:

\code
ALLOCATION_DESC allocDesc = {};
allocDesc.CustomPool = pool;

D3D12_RESOURCE_DESC resDesc = ...
Allocation* alloc;
hr = allocator->CreateResource(&allocDesc, &resDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &alloc, IID_NULL, NULL);
\endcode

All allocations must be released before releasing the pool.
The pool must be released before relasing the allocator.

\code
alloc->Release();
pool->Release();
\endcode

\section custom_pools_features_and_benefits Features and benefits

While it is recommended to use default pools whenever possible for simplicity and to give the allocator
more opportunities for internal optimizations, custom pools may be useful in following cases:

- To keep some resources separate from others in memory.
- To keep track of memory usage of just a specific group of resources. %Statistics can be queried using
  D3D12MA::Pool::CalculateStatistics.
- To use specific size of a memory block (`ID3D12Heap`). To set it, use member D3D12MA::POOL_DESC::BlockSize.
  When set to 0, the library uses automatically determined, variable block sizes.
- To reserve some minimum amount of memory allocated. To use it, set member D3D12MA::POOL_DESC::MinBlockCount.
- To limit maximum amount of memory allocated. To use it, set member D3D12MA::POOL_DESC::MaxBlockCount.
- To use extended parameters of the D3D12 memory allocation. While resources created from default pools
  can only specify `D3D12_HEAP_TYPE_DEFAULT`, `UPLOAD`, `READBACK`, a custom pool may use non-standard
  `D3D12_HEAP_PROPERTIES` (member D3D12MA::POOL_DESC::HeapProperties) and `D3D12_HEAP_FLAGS`
  (D3D12MA::POOL_DESC::HeapFlags), which is useful e.g. for cross-adapter sharing or UMA
  (see also D3D12MA::Allocator::IsUMA).

New versions of this library support creating **committed allocations in custom pools**.
It is supported only when D3D12MA::POOL_DESC::BlockSize = 0.
To use this feature, set D3D12MA::ALLOCATION_DESC::CustomPool to the pointer to your custom pool and
D3D12MA::ALLOCATION_DESC::Flags to D3D12MA::ALLOCATION_FLAG_COMMITTED. Example:

\code
ALLOCATION_DESC allocDesc = {};
allocDesc.CustomPool = pool;
allocDesc.Flags = ALLOCATION_FLAG_COMMITTED;

D3D12_RESOURCE_DESC resDesc = ...
Allocation* alloc;
ID3D12Resource* res;
hr = allocator->CreateResource(&allocDesc, &resDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &alloc, IID_PPV_ARGS(&res));
\endcode

This feature may seem unnecessary, but creating committed allocations from custom pools may be useful
in some cases, e.g. to have separate memory usage statistics for some group of resources or to use
extended allocation parameters, like custom `D3D12_HEAP_PROPERTIES`, which are available only in custom pools.


\page defragmentation Defragmentation

Interleaved allocations and deallocations of many objects of varying size can
cause fragmentation over time, which can lead to a situation where the library is unable
to find a continuous range of free memory for a new allocation despite there is
enough free space, just scattered across many small free ranges between existing
allocations.

To mitigate this problem, you can use defragmentation feature.
It doesn't happen automatically though and needs your cooperation,
because %D3D12MA is a low level library that only allocates memory.
It cannot recreate buffers and textures in a new place as it doesn't remember the contents of `D3D12_RESOURCE_DESC` structure.
It cannot copy their contents as it doesn't record any commands to a command list.

Example:

\code
D3D12MA::DEFRAGMENTATION_DESC defragDesc = {};
defragDesc.Flags = D3D12MA::DEFRAGMENTATION_FLAG_ALGORITHM_FAST;

D3D12MA::DefragmentationContext* defragCtx;
allocator->BeginDefragmentation(&defragDesc, &defragCtx);

for(;;)
{
    D3D12MA::DEFRAGMENTATION_PASS_MOVE_INFO pass;
    HRESULT hr = defragCtx->BeginPass(&pass);
    if(hr == S_OK)
        break;
    else if(hr != S_FALSE)
        // Handle error...

    for(UINT i = 0; i < pass.MoveCount; ++i)
    {
        // Inspect pass.pMoves[i].pSrcAllocation, identify what buffer/texture it represents.
        MyEngineResourceData* resData = (MyEngineResourceData*)pMoves[i].pSrcAllocation->GetPrivateData();
            
        // Recreate this buffer/texture as placed at pass.pMoves[i].pDstTmpAllocation.
        D3D12_RESOURCE_DESC resDesc = ...
        ID3D12Resource* newRes;
        hr = device->CreatePlacedResource(
            pass.pMoves[i].pDstTmpAllocation->GetHeap(),
            pass.pMoves[i].pDstTmpAllocation->GetOffset(), &resDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&newRes));
        // Check hr...

        // Store new resource in the pDstTmpAllocation.
        pass.pMoves[i].pDstTmpAllocation->SetResource(newRes);

        // Copy its content to the new place.
        cmdList->CopyResource(
            pass.pMoves[i].pDstTmpAllocation->GetResource(),
            pass.pMoves[i].pSrcAllocation->GetResource());
    }
        
    // Make sure the copy commands finished executing.
    cmdQueue->ExecuteCommandLists(...);
    // ...
    WaitForSingleObject(fenceEvent, INFINITE);

    // Update appropriate descriptors to point to the new places...
        
    hr = defragCtx->EndPass(&pass);
    if(hr == S_OK)
        break;
    else if(hr != S_FALSE)
        // Handle error...
}

defragCtx->Release();
\endcode

Although functions like D3D12MA::Allocator::CreateResource()
create an allocation and a buffer/texture at once, these are just a shortcut for
allocating memory and creating a placed resource.
Defragmentation works on memory allocations only. You must handle the rest manually.
Defragmentation is an iterative process that should repreat "passes" as long as related functions
return `S_FALSE` not `S_OK`.
In each pass:

1. D3D12MA::DefragmentationContext::BeginPass() function call:
   - Calculates and returns the list of allocations to be moved in this pass.
     Note this can be a time-consuming process.
   - Reserves destination memory for them by creating temporary destination allocations
     that you can query for their `ID3D12Heap` + offset using methods like D3D12MA::Allocation::GetHeap().
2. Inside the pass, **you should**:
   - Inspect the returned list of allocations to be moved.
   - Create new buffers/textures as placed at the returned destination temporary allocations.
   - Copy data from source to destination resources if necessary.
   - Store the pointer to the new resource in the temporary destination allocation.
3. D3D12MA::DefragmentationContext::EndPass() function call:
   - Frees the source memory reserved for the allocations that are moved.
   - Modifies source D3D12MA::Allocation objects that are moved to point to the destination reserved memory
     and destination resource, while source resource is released.
   - Frees `ID3D12Heap` blocks that became empty.

Defragmentation algorithm tries to move all suitable allocations.
You can, however, refuse to move some of them inside a defragmentation pass, by setting
`pass.pMoves[i].Operation` to D3D12MA::DEFRAGMENTATION_MOVE_OPERATION_IGNORE.
This is not recommended and may result in suboptimal packing of the allocations after defragmentation.
If you cannot ensure any allocation can be moved, it is better to keep movable allocations separate in a custom pool.

Inside a pass, for each allocation that should be moved:

- You should copy its data from the source to the destination place by calling e.g. `CopyResource()`.
  - You need to make sure these commands finished executing before the source buffers/textures are released by D3D12MA::DefragmentationContext::EndPass().
- If a resource doesn't contain any meaningful data, e.g. it is a transient render-target texture to be cleared,
  filled, and used temporarily in each rendering frame, you can just recreate this texture
  without copying its data.
- If the resource is in `D3D12_HEAP_TYPE_READBACK` memory, you can copy its data on the CPU
  using `memcpy()`.
- If you cannot move the allocation, you can set `pass.pMoves[i].Operation` to D3D12MA::DEFRAGMENTATION_MOVE_OPERATION_IGNORE.
  This will cancel the move.
  - D3D12MA::DefragmentationContext::EndPass() will then free the destination memory
    not the source memory of the allocation, leaving it unchanged.
- If you decide the allocation is unimportant and can be destroyed instead of moved (e.g. it wasn't used for long time),
  you can set `pass.pMoves[i].Operation` to D3D12MA::DEFRAGMENTATION_MOVE_OPERATION_DESTROY.
  - D3D12MA::DefragmentationContext::EndPass() will then free both source and destination memory, and will destroy the source D3D12MA::Allocation object.

You can defragment a specific custom pool by calling D3D12MA::Pool::BeginDefragmentation
or all the default pools by calling D3D12MA::Allocator::BeginDefragmentation (like in the example above).

Defragmentation is always performed in each pool separately.
Allocations are never moved between different heap types.
The size of the destination memory reserved for a moved allocation is the same as the original one.
Alignment of an allocation as it was determined using `GetResourceAllocationInfo()` is also respected after defragmentation.
Buffers/textures should be recreated with the same `D3D12_RESOURCE_DESC` parameters as the original ones.

You can perform the defragmentation incrementally to limit the number of allocations and bytes to be moved
in each pass, e.g. to call it in sync with render frames and not to experience too big hitches.
See members: D3D12MA::DEFRAGMENTATION_DESC::MaxBytesPerPass, D3D12MA::DEFRAGMENTATION_DESC::MaxAllocationsPerPass.

It is also safe to perform the defragmentation asynchronously to render frames and other Direct3D 12 and %D3D12MA
usage, possibly from multiple threads, with the exception that allocations
returned in D3D12MA::DEFRAGMENTATION_PASS_MOVE_INFO::pMoves shouldn't be released until the defragmentation pass is ended.

<b>Mapping</b> is out of scope of this library and so it is not preserved after an allocation is moved during defragmentation.
You need to map the new resource yourself if needed.

\note Defragmentation is not supported in custom pools created with D3D12MA::POOL_FLAG_ALGORITHM_LINEAR.


\page statistics Statistics

This library contains several functions that return information about its internal state,
especially the amount of memory allocated from D3D12.

\section statistics_numeric_statistics Numeric statistics

If you need to obtain basic statistics about memory usage per memory segment group, together with current budget,
you can call function D3D12MA::Allocator::GetBudget() and inspect structure D3D12MA::Budget.
This is useful to keep track of memory usage and stay withing budget.
Example:

\code
D3D12MA::Budget localBudget;
allocator->GetBudget(&localBudget, NULL);

printf("My GPU memory currently has %u allocations taking %llu B,\n",
    localBudget.Statistics.AllocationCount,
    localBudget.Statistics.AllocationBytes);
printf("allocated out of %u D3D12 memory heaps taking %llu B,\n",
    localBudget.Statistics.BlockCount,
    localBudget.Statistics.BlockBytes);
printf("D3D12 reports total usage %llu B with budget %llu B.\n",
    localBudget.UsageBytes,
    localBudget.BudgetBytes);
\endcode

You can query for more detailed statistics per heap type, memory segment group, and totals,
including minimum and maximum allocation size and unused range size,
by calling function D3D12MA::Allocator::CalculateStatistics() and inspecting structure D3D12MA::TotalStatistics.
This function is slower though, as it has to traverse all the internal data structures,
so it should be used only for debugging purposes.

You can query for statistics of a custom pool using function D3D12MA::Pool::GetStatistics()
or D3D12MA::Pool::CalculateStatistics().

You can query for information about a specific allocation using functions of the D3D12MA::Allocation class,
e.g. `GetSize()`, `GetOffset()`, `GetHeap()`.

\section statistics_json_dump JSON dump

You can dump internal state of the allocator to a string in JSON format using function D3D12MA::Allocator::BuildStatsString().
The result is guaranteed to be correct JSON.
It uses Windows Unicode (UTF-16) encoding.
Any strings provided by user (see D3D12MA::Allocation::SetName())
are copied as-is and properly escaped for JSON.
It must be freed using function D3D12MA::Allocator::FreeStatsString().

The format of this JSON string is not part of official documentation of the library,
but it will not change in backward-incompatible way without increasing library major version number
and appropriate mention in changelog.

The JSON string contains all the data that can be obtained using D3D12MA::Allocator::CalculateStatistics().
It can also contain detailed map of allocated memory blocks and their regions -
free and occupied by allocations.
This allows e.g. to visualize the memory or assess fragmentation.


\page resource_aliasing Resource aliasing (overlap)

New explicit graphics APIs (Vulkan and Direct3D 12), thanks to manual memory
management, give an opportunity to alias (overlap) multiple resources in the
same region of memory - a feature not available in the old APIs (Direct3D 11, OpenGL).
It can be useful to save video memory, but it must be used with caution.

For example, if you know the flow of your whole render frame in advance, you
are going to use some intermediate textures or buffers only during a small range of render passes,
and you know these ranges don't overlap in time, you can create these resources in
the same place in memory, even if they have completely different parameters (width, height, format etc.).

![Resource aliasing (overlap)](../gfx/Aliasing.png)

Such scenario is possible using D3D12MA, but you need to create your resources
using special function D3D12MA::Allocator::CreateAliasingResource.
Before that, you need to allocate memory with parameters calculated using formula:

- allocation size = max(size of each resource)
- allocation alignment = max(alignment of each resource)

Following example shows two different textures created in the same place in memory,
allocated to fit largest of them.

\code
D3D12_RESOURCE_DESC resDesc1 = {};
resDesc1.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
resDesc1.Alignment = 0;
resDesc1.Width = 1920;
resDesc1.Height = 1080;
resDesc1.DepthOrArraySize = 1;
resDesc1.MipLevels = 1;
resDesc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
resDesc1.SampleDesc.Count = 1;
resDesc1.SampleDesc.Quality = 0;
resDesc1.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
resDesc1.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

D3D12_RESOURCE_DESC resDesc2 = {};
resDesc2.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
resDesc2.Alignment = 0;
resDesc2.Width = 1024;
resDesc2.Height = 1024;
resDesc2.DepthOrArraySize = 1;
resDesc2.MipLevels = 0;
resDesc2.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
resDesc2.SampleDesc.Count = 1;
resDesc2.SampleDesc.Quality = 0;
resDesc2.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
resDesc2.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

const D3D12_RESOURCE_ALLOCATION_INFO allocInfo1 =
    device->GetResourceAllocationInfo(0, 1, &resDesc1);
const D3D12_RESOURCE_ALLOCATION_INFO allocInfo2 =
    device->GetResourceAllocationInfo(0, 1, &resDesc2);

D3D12_RESOURCE_ALLOCATION_INFO finalAllocInfo = {};
finalAllocInfo.Alignment = std::max(allocInfo1.Alignment, allocInfo2.Alignment);
finalAllocInfo.SizeInBytes = std::max(allocInfo1.SizeInBytes, allocInfo2.SizeInBytes);

D3D12MA::ALLOCATION_DESC allocDesc = {};
allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
allocDesc.ExtraHeapFlags = D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;

D3D12MA::Allocation* alloc;
hr = allocator->AllocateMemory(&allocDesc, &finalAllocInfo, &alloc);
assert(alloc != NULL && alloc->GetHeap() != NULL);

ID3D12Resource* res1;
hr = allocator->CreateAliasingResource(
    alloc,
    0, // AllocationLocalOffset
    &resDesc1,
    D3D12_RESOURCE_STATE_COMMON,
    NULL, // pOptimizedClearValue
    IID_PPV_ARGS(&res1));

ID3D12Resource* res2;
hr = allocator->CreateAliasingResource(
    alloc,
    0, // AllocationLocalOffset
    &resDesc2,
    D3D12_RESOURCE_STATE_COMMON,
    NULL, // pOptimizedClearValue
    IID_PPV_ARGS(&res2));

// You can use res1 and res2, but not at the same time!

res2->Release();
res1->Release();
alloc->Release();
\endcode

Remember that using resouces that alias in memory requires proper synchronization.
You need to issue a special barrier of type `D3D12_RESOURCE_BARRIER_TYPE_ALIASING`.
You also need to treat a resource after aliasing as uninitialized - containing garbage data.
For example, if you use `res1` and then want to use `res2`, you need to first initialize `res2`
using either Clear, Discard, or Copy to the entire resource.

Additional considerations:

- D3D12 also allows to interpret contents of memory between aliasing resources consistently in some cases,
  which is called "data inheritance". For details, see
  Microsoft documentation chapter "Memory Aliasing and Data Inheritance".
- You can create more complex layout where different textures and buffers are bound
  at different offsets inside one large allocation. For example, one can imagine
  a big texture used in some render passes, aliasing with a set of many small buffers
  used in some further passes. To bind a resource at non-zero offset of an allocation,
  call D3D12MA::Allocator::CreateAliasingResource with appropriate value of `AllocationLocalOffset` parameter.
- Resources of the three categories: buffers, textures with `RENDER_TARGET` or `DEPTH_STENCIL` flags, and all other textures,
  can be placed in the same memory only when `allocator->GetD3D12Options().ResourceHeapTier >= D3D12_RESOURCE_HEAP_TIER_2`.
  Otherwise they must be placed in different memory heap types, and thus aliasing them is not possible.


\page linear_algorithm Linear allocation algorithm

Each D3D12 memory block managed by this library has accompanying metadata that
keeps track of used and unused regions. By default, the metadata structure and
algorithm tries to find best place for new allocations among free regions to
optimize memory usage. This way you can allocate and free objects in any order.

![Default allocation algorithm](../gfx/Linear_allocator_1_algo_default.png)

Sometimes there is a need to use simpler, linear allocation algorithm. You can
create custom pool that uses such algorithm by adding flag
D3D12MA::POOL_FLAG_ALGORITHM_LINEAR to D3D12MA::POOL_DESC::Flags while creating
D3D12MA::Pool object. Then an alternative metadata management is used. It always
creates new allocations after last one and doesn't reuse free regions after
allocations freed in the middle. It results in better allocation performance and
less memory consumed by metadata.

![Linear allocation algorithm](../gfx/Linear_allocator_2_algo_linear.png)

With this one flag, you can create a custom pool that can be used in many ways:
free-at-once, stack, double stack, and ring buffer. See below for details.
You don't need to specify explicitly which of these options you are going to use - it is detected automatically.

\section linear_algorithm_free_at_once Free-at-once

In a pool that uses linear algorithm, you still need to free all the allocations
individually by calling `allocation->Release()`. You can free
them in any order. New allocations are always made after last one - free space
in the middle is not reused. However, when you release all the allocation and
the pool becomes empty, allocation starts from the beginning again. This way you
can use linear algorithm to speed up creation of allocations that you are going
to release all at once.

![Free-at-once](../gfx/Linear_allocator_3_free_at_once.png)

This mode is also available for pools created with D3D12MA::POOL_DESC::MaxBlockCount
value that allows multiple memory blocks.

\section linear_algorithm_stack Stack

When you free an allocation that was created last, its space can be reused.
Thanks to this, if you always release allocations in the order opposite to their
creation (LIFO - Last In First Out), you can achieve behavior of a stack.

![Stack](../gfx/Linear_allocator_4_stack.png)

This mode is also available for pools created with D3D12MA::POOL_DESC::MaxBlockCount
value that allows multiple memory blocks.

\section linear_algorithm_double_stack Double stack

The space reserved by a custom pool with linear algorithm may be used by two
stacks:

- First, default one, growing up from offset 0.
- Second, "upper" one, growing down from the end towards lower offsets.

To make allocation from the upper stack, add flag D3D12MA::ALLOCATION_FLAG_UPPER_ADDRESS
to D3D12MA::ALLOCATION_DESC::Flags.

![Double stack](../gfx/Linear_allocator_7_double_stack.png)

Double stack is available only in pools with one memory block -
D3D12MA::POOL_DESC::MaxBlockCount must be 1. Otherwise behavior is undefined.

When the two stacks' ends meet so there is not enough space between them for a
new allocation, such allocation fails with usual `E_OUTOFMEMORY` error.

\section linear_algorithm_ring_buffer Ring buffer

When you free some allocations from the beginning and there is not enough free space
for a new one at the end of a pool, allocator's "cursor" wraps around to the
beginning and starts allocation there. Thanks to this, if you always release
allocations in the same order as you created them (FIFO - First In First Out),
you can achieve behavior of a ring buffer / queue.

![Ring buffer](../gfx/Linear_allocator_5_ring_buffer.png)

Ring buffer is available only in pools with one memory block -
D3D12MA::POOL_DESC::MaxBlockCount must be 1. Otherwise behavior is undefined.

\section linear_algorithm_additional_considerations Additional considerations

Linear algorithm can also be used with \ref virtual_allocator.
See flag D3D12MA::VIRTUAL_BLOCK_FLAG_ALGORITHM_LINEAR.


\page virtual_allocator Virtual allocator

As an extra feature, the core allocation algorithm of the library is exposed through a simple and convenient API of "virtual allocator".
It doesn't allocate any real GPU memory. It just keeps track of used and free regions of a "virtual block".
You can use it to allocate your own memory or other objects, even completely unrelated to D3D12.
A common use case is sub-allocation of pieces of one large GPU buffer.

\section virtual_allocator_creating_virtual_block Creating virtual block

To use this functionality, there is no main "allocator" object.
You don't need to have D3D12MA::Allocator object created.
All you need to do is to create a separate D3D12MA::VirtualBlock object for each block of memory you want to be managed by the allocator:

-# Fill in D3D12MA::ALLOCATOR_DESC structure.
-# Call D3D12MA::CreateVirtualBlock. Get new D3D12MA::VirtualBlock object.

Example:

\code
D3D12MA::VIRTUAL_BLOCK_DESC blockDesc = {};
blockDesc.Size = 1048576; // 1 MB

D3D12MA::VirtualBlock *block;
HRESULT hr = CreateVirtualBlock(&blockDesc, &block);
\endcode

\section virtual_allocator_making_virtual_allocations Making virtual allocations

D3D12MA::VirtualBlock object contains internal data structure that keeps track of free and occupied regions
using the same code as the main D3D12 memory allocator.
A single allocation is identified by a lightweight structure D3D12MA::VirtualAllocation.
You will also likely want to know the offset at which the allocation was made in the block.

In order to make an allocation:

-# Fill in D3D12MA::VIRTUAL_ALLOCATION_DESC structure.
-# Call D3D12MA::VirtualBlock::Allocate. Get new D3D12MA::VirtualAllocation value that identifies the allocation.

Example:

\code
D3D12MA::VIRTUAL_ALLOCATION_DESC allocDesc = {};
allocDesc.Size = 4096; // 4 KB

D3D12MA::VirtualAllocation alloc;
UINT64 allocOffset;
hr = block->Allocate(&allocDesc, &alloc, &allocOffset);
if(SUCCEEDED(hr))
{
    // Use the 4 KB of your memory starting at allocOffset.
}
else
{
    // Allocation failed - no space for it could be found. Handle this error!
}
\endcode

\section virtual_allocator_deallocation Deallocation

When no longer needed, an allocation can be freed by calling D3D12MA::VirtualBlock::FreeAllocation.

When whole block is no longer needed, the block object can be released by calling `block->Release()`.
All allocations must be freed before the block is destroyed, which is checked internally by an assert.
However, if you don't want to call `block->FreeAllocation` for each allocation, you can use D3D12MA::VirtualBlock::Clear to free them all at once -
a feature not available in normal D3D12 memory allocator.

Example:

\code
block->FreeAllocation(alloc);
block->Release();
\endcode

\section virtual_allocator_allocation_parameters Allocation parameters

You can attach a custom pointer to each allocation by using D3D12MA::VirtualBlock::SetAllocationPrivateData.
Its default value is `NULL`.
It can be used to store any data that needs to be associated with that allocation - e.g. an index, a handle, or a pointer to some
larger data structure containing more information. Example:

\code
struct CustomAllocData
{
    std::string m_AllocName;
};
CustomAllocData* allocData = new CustomAllocData();
allocData->m_AllocName = "My allocation 1";
block->SetAllocationPrivateData(alloc, allocData);
\endcode

The pointer can later be fetched, along with allocation offset and size, by passing the allocation handle to function
D3D12MA::VirtualBlock::GetAllocationInfo and inspecting returned structure D3D12MA::VIRTUAL_ALLOCATION_INFO.
If you allocated a new object to be used as the custom pointer, don't forget to delete that object before freeing the allocation!
Example:

\code
VIRTUAL_ALLOCATION_INFO allocInfo;
block->GetAllocationInfo(alloc, &allocInfo);
delete (CustomAllocData*)allocInfo.pPrivateData;

block->FreeAllocation(alloc);
\endcode

\section virtual_allocator_alignment_and_units Alignment and units

It feels natural to express sizes and offsets in bytes.
If an offset of an allocation needs to be aligned to a multiply of some number (e.g. 4 bytes), you can fill optional member
D3D12MA::VIRTUAL_ALLOCATION_DESC::Alignment to request it. Example:

\code
D3D12MA::VIRTUAL_ALLOCATION_DESC allocDesc = {};
allocDesc.Size = 4096; // 4 KB
allocDesc.Alignment = 4; // Returned offset must be a multiply of 4 B

D3D12MA::VirtualAllocation alloc;
UINT64 allocOffset;
hr = block->Allocate(&allocDesc, &alloc, &allocOffset);
\endcode

Alignments of different allocations made from one block may vary.
However, if all alignments and sizes are always multiply of some size e.g. 4 B or `sizeof(MyDataStruct)`,
you can express all sizes, alignments, and offsets in multiples of that size instead of individual bytes.
It might be more convenient, but you need to make sure to use this new unit consistently in all the places:

- D3D12MA::VIRTUAL_BLOCK_DESC::Size
- D3D12MA::VIRTUAL_ALLOCATION_DESC::Size and D3D12MA::VIRTUAL_ALLOCATION_DESC::Alignment
- Using offset returned by D3D12MA::VirtualBlock::Allocate and D3D12MA::VIRTUAL_ALLOCATION_INFO::Offset

\section virtual_allocator_statistics Statistics

You can obtain brief statistics of a virtual block using D3D12MA::VirtualBlock::GetStatistics().
The function fills structure D3D12MA::Statistics - same as used by the normal D3D12 memory allocator.
Example:

\code
D3D12MA::Statistics stats;
block->GetStatistics(&stats);
printf("My virtual block has %llu bytes used by %u virtual allocations\n",
    stats.AllocationBytes, stats.AllocationCount);
\endcode

More detailed statistics can be obtained using function D3D12MA::VirtualBlock::CalculateStatistics(),
but they are slower to calculate.

You can also request a full list of allocations and free regions as a string in JSON format by calling
D3D12MA::VirtualBlock::BuildStatsString.
Returned string must be later freed using D3D12MA::VirtualBlock::FreeStatsString.
The format of this string may differ from the one returned by the main D3D12 allocator, but it is similar.

\section virtual_allocator_additional_considerations Additional considerations

Alternative, linear algorithm can be used with virtual allocator - see flag
D3D12MA::VIRTUAL_BLOCK_FLAG_ALGORITHM_LINEAR and documentation: \ref linear_algorithm.

Note that the "virtual allocator" functionality is implemented on a level of individual memory blocks.
Keeping track of a whole collection of blocks, allocating new ones when out of free space,
deleting empty ones, and deciding which one to try first for a new allocation must be implemented by the user.


\page configuration Configuration

Please check file `D3D12MemAlloc.cpp` lines between "Configuration Begin" and
"Configuration End" to find macros that you can define to change the behavior of
the library, primarily for debugging purposes.

\section custom_memory_allocator Custom CPU memory allocator

If you use custom allocator for CPU memory rather than default C++ operator `new`
and `delete` or `malloc` and `free` functions, you can make this library using
your allocator as well by filling structure D3D12MA::ALLOCATION_CALLBACKS and
passing it as optional member D3D12MA::ALLOCATOR_DESC::pAllocationCallbacks.
Functions pointed there will be used by the library to make any CPU-side
allocations. Example:

\code
#include <malloc.h>

void* CustomAllocate(size_t Size, size_t Alignment, void* pPrivateData)
{
    void* memory = _aligned_malloc(Size, Alignment);
    // Your extra bookkeeping here...
    return memory;
}

void CustomFree(void* pMemory, void* pPrivateData)
{
    // Your extra bookkeeping here...
    _aligned_free(pMemory);
}

(...)

D3D12MA::ALLOCATION_CALLBACKS allocationCallbacks = {};
allocationCallbacks.pAllocate = &CustomAllocate;
allocationCallbacks.pFree = &CustomFree;

D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
allocatorDesc.pDevice = device;
allocatorDesc.pAdapter = adapter;
allocatorDesc.pAllocationCallbacks = &allocationCallbacks;

D3D12MA::Allocator* allocator;
HRESULT hr = D3D12MA::CreateAllocator(&allocatorDesc, &allocator);
\endcode


\section debug_margins Debug margins

By default, allocations are laid out in memory blocks next to each other if possible
(considering required alignment returned by `ID3D12Device::GetResourceAllocationInfo`).

![Allocations without margin](../gfx/Margins_1.png)

Define macro `D3D12MA_DEBUG_MARGIN` to some non-zero value (e.g. 16) inside "D3D12MemAlloc.cpp"
to enforce specified number of bytes as a margin after every allocation.

![Allocations with margin](../gfx/Margins_2.png)

If your bug goes away after enabling margins, it means it may be caused by memory
being overwritten outside of allocation boundaries. It is not 100% certain though.
Change in application behavior may also be caused by different order and distribution
of allocations across memory blocks after margins are applied.

Margins work with all memory heap types.

Margin is applied only to placed allocations made out of memory heaps and not to committed
allocations, which have their own, implicit memory heap of specific size.
It is thus not applied to allocations made using D3D12MA::ALLOCATION_FLAG_COMMITTED flag
or those automatically decided to put into committed allocations, e.g. due to its large size.

Margins appear in [JSON dump](@ref statistics_json_dump) as part of free space.

Note that enabling margins increases memory usage and fragmentation.

Margins do not apply to \ref virtual_allocator.


\page general_considerations General considerations

\section general_considerations_thread_safety Thread safety

- The library has no global state, so separate D3D12MA::Allocator objects can be used independently.
  In typical applications there should be no need to create multiple such objects though - one per `ID3D12Device` is enough.
- All calls to methods of D3D12MA::Allocator class are safe to be made from multiple
  threads simultaneously because they are synchronized internally when needed.
- When the allocator is created with D3D12MA::ALLOCATOR_FLAG_SINGLETHREADED,
  calls to methods of D3D12MA::Allocator class must be made from a single thread or synchronized by the user.
  Using this flag may improve performance.
- D3D12MA::VirtualBlock is not safe to be used from multiple threads simultaneously.

\section general_considerations_versioning_and_compatibility Versioning and compatibility

The library uses [**Semantic Versioning**](https://semver.org/),
which means version numbers follow convention: Major.Minor.Patch (e.g. 2.3.0), where:

- Incremented Patch version means a release is backward- and forward-compatible,
  introducing only some internal improvements, bug fixes, optimizations etc.
  or changes that are out of scope of the official API described in this documentation.
- Incremented Minor version means a release is backward-compatible,
  so existing code that uses the library should continue to work, while some new
  symbols could have been added: new structures, functions, new values in existing
  enums and bit flags, new structure members, but not new function parameters.
- Incrementing Major version means a release could break some backward compatibility.

All changes between official releases are documented in file "CHANGELOG.md".

\warning Backward compatiblity is considered on the level of C++ source code, not binary linkage.
Adding new members to existing structures is treated as backward compatible if initializing
the new members to binary zero results in the old behavior.
You should always fully initialize all library structures to zeros and not rely on their
exact binary size.

\section general_considerations_features_not_supported Features not supported

Features deliberately excluded from the scope of this library:

- **Descriptor allocation.** Although also called "heaps", objects that represent
  descriptors are separate part of the D3D12 API from buffers and textures.
  You can still use \ref virtual_allocator to manage descriptors and their ranges inside a descriptor heap.
- **Support for reserved (tiled) resources.** We don't recommend using them.
- Support for `ID3D12Device::Evict` and `MakeResident`. We don't recommend using them.
  You can call them on the D3D12 objects manually.
  Plese keep in mind, however, that eviction happens on the level of entire `ID3D12Heap` memory blocks
  and not individual buffers or textures which may be placed inside them.
- **Handling CPU memory allocation failures.** When dynamically creating small C++
  objects in CPU memory (not the GPU memory), allocation failures are not
  handled gracefully, because that would complicate code significantly and
  is usually not needed in desktop PC applications anyway.
  Success of an allocation is just checked with an assert.
- **Code free of any compiler warnings.**
  There are many preprocessor macros that make some variables unused, function parameters unreferenced,
  or conditional expressions constant in some configurations.
  The code of this library should not be bigger or more complicated just to silence these warnings.
  It is recommended to disable such warnings instead.
- This is a C++ library. **Bindings or ports to any other programming languages** are welcome as external projects but
  are not going to be included into this repository.
*/

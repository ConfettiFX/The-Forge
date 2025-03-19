/*
 * Copyright (c) 2017-2025 The Forge Interactive Inc.
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

#include "../GraphicsConfig.h"

#ifdef DIRECT3D12
#define RENDERER_IMPLEMENTATION

// Pull in minimal Windows headers
#include <Windows.h>
#include <malloc.h> // _alloca

#include "../Interfaces/IGraphics.h"

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../../../Common_3/Graphics/ThirdParty/OpenSource/ags/AgsHelper.h"
#include "../../../Common_3/Graphics/ThirdParty/OpenSource/nvapi/NvApiHelper.h"
#include "../ThirdParty/OpenSource/renderdoc/renderdoc_app.h"

#include "../../Utilities/Interfaces/ILog.h"

#include "../../Utilities/Math/AlgorithmsImpl.h"

#include "Direct3D12CapBuilder.h"
#include "Direct3D12Hooks.h"
#include "Direct3D12_Cxx.h"

#if defined(AUTOMATED_TESTING)
#include "../../Application/Interfaces/IScreenshot.h"
#endif

#if defined(ENABLE_GRAPHICS_VALIDATION) && !defined(XBOX)
#include <dxgidebug.h>
#endif

#include <math.h> // pow, etc...

#include "../../Utilities/Interfaces/IMemory.h"

uint32_t round_up(uint32_t a, uint32_t b) { return (a + b - 1) & ~(b - 1); }
uint64_t round_up_64(uint64_t a, uint64_t b) { return (a + b - 1) & ~(b - 1); }

static inline size_t tf_mem_hash_uint8_t(const uint8_t* mem, size_t size, size_t prev)
{
    uint32_t result = (uint32_t)prev;
    while (size--)
        result = (result * 16777619) ^ *mem++;
    return (size_t)result;
}

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL    ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN ((D3D12_GPU_VIRTUAL_ADDRESS)-1)
#define D3D12_REQ_CONSTANT_BUFFER_SIZE    (D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16u)
#define D3D12_DESCRIPTOR_ID_NONE          ((int32_t)-1)

#define MAX_COMPILE_ARGS                  64

// stubs for durango because Direct3D12Raytracing.cpp is not used on XBOX
#if defined(D3D12_RAYTRACING_AVAILABLE)
extern void fillRaytracingDescriptorHandle(AccelerationStructure* pAccelerationStructure, DxDescriptorID* pOutId);
#endif

// Enabling DRED
#if defined(_WIN32) && defined(_DEBUG) && defined(DRED)
#define USE_DRED 1
#endif

typedef struct SubresourceDataDesc
{
    uint64_t mSrcOffset;
    uint32_t mMipLevel;
    uint32_t mArrayLayer;
} SubresourceDataDesc;

void getBufferSizeAlign(Renderer* pRenderer, const BufferDesc* pDesc, ResourceSizeAlign* pOut);
void getTextureSizeAlign(Renderer* pRenderer, const TextureDesc* pDesc, ResourceSizeAlign* pOut);
void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
void removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
void cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size);
void cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pSubresourceDesc);
void cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const SubresourceDataDesc* pSubresourceDesc);
void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture);
void removeTexture(Renderer* pRenderer, Texture* pTexture);

static void SetObjectName(ID3D12Object* pObject, const char* pName)
{
    UNREF_PARAM(pObject);
    UNREF_PARAM(pName);
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    if (!pName)
    {
        return;
    }

    wchar_t wName[MAX_DEBUG_NAME_LENGTH] = { 0 };
    size_t  numConverted = 0;
    mbstowcs_s(&numConverted, wName, MAX_DEBUG_NAME_LENGTH, pName, MAX_DEBUG_NAME_LENGTH);
    COM_CALL(SetName, pObject, wName);
#endif
}

// clang-format off
D3D12_BLEND_OP gDx12BlendOpTranslator[MAX_BLEND_MODES] =
{
	D3D12_BLEND_OP_ADD,
	D3D12_BLEND_OP_SUBTRACT,
	D3D12_BLEND_OP_REV_SUBTRACT,
	D3D12_BLEND_OP_MIN,
	D3D12_BLEND_OP_MAX,
};

D3D12_BLEND gDx12BlendConstantTranslator[MAX_BLEND_CONSTANTS] =
{
	D3D12_BLEND_ZERO,
	D3D12_BLEND_ONE,
	D3D12_BLEND_SRC_COLOR,
	D3D12_BLEND_INV_SRC_COLOR,
	D3D12_BLEND_DEST_COLOR,
	D3D12_BLEND_INV_DEST_COLOR,
	D3D12_BLEND_SRC_ALPHA,
	D3D12_BLEND_INV_SRC_ALPHA,
	D3D12_BLEND_DEST_ALPHA,
	D3D12_BLEND_INV_DEST_ALPHA,
	D3D12_BLEND_SRC_ALPHA_SAT,
	D3D12_BLEND_BLEND_FACTOR,
	D3D12_BLEND_INV_BLEND_FACTOR,
};

D3D12_COMPARISON_FUNC gDx12ComparisonFuncTranslator[MAX_COMPARE_MODES] =
{
	D3D12_COMPARISON_FUNC_NEVER,
	D3D12_COMPARISON_FUNC_LESS,
	D3D12_COMPARISON_FUNC_EQUAL,
	D3D12_COMPARISON_FUNC_LESS_EQUAL,
	D3D12_COMPARISON_FUNC_GREATER,
	D3D12_COMPARISON_FUNC_NOT_EQUAL,
	D3D12_COMPARISON_FUNC_GREATER_EQUAL,
	D3D12_COMPARISON_FUNC_ALWAYS,
};

D3D12_STENCIL_OP gDx12StencilOpTranslator[MAX_STENCIL_OPS] =
{
	D3D12_STENCIL_OP_KEEP,
	D3D12_STENCIL_OP_ZERO,
	D3D12_STENCIL_OP_REPLACE,
	D3D12_STENCIL_OP_INVERT,
	D3D12_STENCIL_OP_INCR,
	D3D12_STENCIL_OP_DECR,
	D3D12_STENCIL_OP_INCR_SAT,
	D3D12_STENCIL_OP_DECR_SAT,
};

D3D12_CULL_MODE gDx12CullModeTranslator[MAX_CULL_MODES] =
{
	D3D12_CULL_MODE_NONE,
	D3D12_CULL_MODE_BACK,
	D3D12_CULL_MODE_FRONT,
};

D3D12_FILL_MODE gDx12FillModeTranslator[MAX_FILL_MODES] =
{
	D3D12_FILL_MODE_SOLID,
	D3D12_FILL_MODE_WIREFRAME,
};

const D3D12_COMMAND_LIST_TYPE gDx12CmdTypeTranslator[MAX_QUEUE_TYPE] =
{
	D3D12_COMMAND_LIST_TYPE_DIRECT,
	D3D12_COMMAND_LIST_TYPE_COPY,
	D3D12_COMMAND_LIST_TYPE_COMPUTE
};

const D3D12_COMMAND_QUEUE_PRIORITY gDx12QueuePriorityTranslator[MAX_QUEUE_PRIORITY] =
{
	D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
	D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
#if !defined(XBOX)
	D3D12_COMMAND_QUEUE_PRIORITY_GLOBAL_REALTIME,
#endif
};
// clang-format on

// =================================================================================================
// IMPLEMENTATION
// =================================================================================================

#if defined(RENDERER_IMPLEMENTATION)

#if !defined(XBOX)
#pragma comment(lib, "dxguid.lib")
#endif

#if !defined(XBOX) && !defined(FORGE_D3D12_DYNAMIC_LOADING)
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#endif

// NOTE: dxguid.lib is missing some types, but these were added in a later Windows10SDK version.
// Until this can be verified to be consistent across machines, we just define the missing ones manually.
// See: https://github.com/microsoft/DirectX-Graphics-Samples/issues/622
// The GUIDs can be located in D3D headers (d3d12.h, dxgi.h, dxgi1_2.h, etc...), and are identical on all platforms.
// In TheForge, all headers with DirectX GUIDs can be found in this folder (search for `DEFINE_GUID(`):
// `Common_3/Graphics/ThirdParty/OpenSource/Direct3d12Agility/include`.
#define DO_DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    EXTERN_C const GUID name = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }
#if defined(XBOX)
// Missing from d3d12_x.lib/d3d12_xs.lib
DO_DEFINE_GUID(IID_ID3D12CommandSignature, 0xc36a797c, 0xec80, 0x4f0a, 0x89, 0x85, 0xa7, 0xb2, 0x47, 0x50, 0x82, 0xd1);
DO_DEFINE_GUID(IID_ID3D12StateObject, 0x47016943, 0xfca8, 0x4594, 0x93, 0xea, 0xaf, 0x25, 0x8b, 0x55, 0x34, 0x6d);
DO_DEFINE_GUID(IID_ID3D12GraphicsCommandList4, 0x8754318e, 0xd3a9, 0x4541, 0x98, 0xcf, 0x64, 0x5b, 0x50, 0xdc, 0x48, 0x74);
#if !defined(SCARLETT)
DO_DEFINE_GUID(IID_ID3D12PipelineLibrary, 0xc64226a8, 0x9201, 0x46af, 0xb4, 0xcc, 0x53, 0xfb, 0x9f, 0xf7, 0x41, 0x4f);
#endif
#endif
// Missing from both dxguid.lib and d3d12_x.lib/d3d12_xs.lib
DO_DEFINE_GUID(IID_ID3D12StateObjectProperties1, 0x460caac7, 0x1d24, 0x446a, 0xa1, 0x84, 0xca, 0x67, 0xdb, 0x49, 0x41, 0x38);
DO_DEFINE_GUID(IID_ID3D12WorkGraphProperties, 0x065acf71, 0xf863, 0x4b89, 0x82, 0xf4, 0x02, 0xe4, 0xd5, 0x88, 0x67, 0x57);
DO_DEFINE_GUID(IID_ID3D12GraphicsCommandList10, 0x7013c015, 0xd161, 0x4b63, 0xa0, 0x8c, 0x23, 0x85, 0x52, 0xdd, 0x8a, 0xcc);
// On Xbox, `IID_XXX` definitions are marked as `__declspec(dllimport)`, but on PC they are just regular extern variables.
// If you want to use PC headers (COM C-interface), but link Xbox libs, then the `IID` definitions must be marked correctly
// in the PC headers which is done via a small tweak to the DirectX Agility SDK header. This works for the majority of types,
// but there are a few differing types between the PC/Xbox DirectX libs that require manual workarounds.
DO_DEFINE_GUID(IID_ID3D12Device9_Copy, 0x4c80e962, 0xf032, 0x4f60, 0xbc, 0x9e, 0xeb, 0xc2, 0xcf, 0xa1, 0xd8, 0x3c);
DO_DEFINE_GUID(IID_ID3D12QueryHeap_Copy, 0x0d9658ae, 0xed45, 0x469e, 0xa6, 0x1d, 0x97, 0x0e, 0xc5, 0x83, 0xca, 0xb4);
DO_DEFINE_GUID(IID_ID3D12Device1_Copy, 0x77acce80, 0x638e, 0x4e65, 0x88, 0x95, 0xc1, 0xf2, 0x33, 0x86, 0x86, 0x3e);
DO_DEFINE_GUID(IID_ID3D12Device5_Copy, 0x8b4f173b, 0x2fea, 0x4b80, 0x8f, 0x58, 0x43, 0x07, 0x19, 0x1a, 0xb9, 0x5d);
DO_DEFINE_GUID(IID_IDXGIAdapter4_Copy, 0x3c8d99d1, 0x4fbf, 0x4181, 0xa8, 0x2c, 0xaf, 0x66, 0xbf, 0x7b, 0xd2, 0x4e);
DO_DEFINE_GUID(IID_IDXGIFactory6_Copy, 0xc1b6694f, 0xff09, 0x44a9, 0xb0, 0x3c, 0x77, 0x90, 0x0a, 0x0a, 0x1d, 0x17);
DO_DEFINE_GUID(IID_ID3D12InfoQueue1_Copy, 0x2852dd88, 0xb484, 0x4c0c, 0xb6, 0xb1, 0x67, 0x16, 0x85, 0x00, 0xe6, 0x00);
DO_DEFINE_GUID(IID_IDXGIOutput6_Copy, 0x068346e8, 0xaaec, 0x4b84, 0xad, 0xd7, 0x13, 0x7f, 0x51, 0x3f, 0x77, 0xa1);
#undef DO_DEFINE_GUID

//-V:SAFE_FREE:779
#define SAFE_FREE(p_var)         \
    if ((p_var))                 \
    {                            \
        tf_free((void*)(p_var)); \
        p_var = NULL;            \
    }

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p_var)       \
    if (p_var)                    \
    {                             \
        COM_CALL(Release, p_var); \
        p_var = NULL;             \
    }
#endif

#define CALC_SUBRESOURCE_INDEX(MipSlice, ArraySlice, PlaneSlice, MipLevels, ArraySize) \
    ((MipSlice) + ((ArraySlice) * (MipLevels)) + ((PlaneSlice) * (MipLevels) * (ArraySize)))

// Internal utility functions (may become external one day)
uint64_t                      util_dx12_determine_storage_counter_offset(uint64_t buffer_size);
DXGI_FORMAT                   util_to_dx12_uav_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                   util_to_dx12_dsv_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                   util_to_dx12_srv_format(DXGI_FORMAT defaultFormat, bool isStencil);
DXGI_FORMAT                   util_to_dx12_stencil_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                   util_to_dx12_swapchain_format(TinyImageFormat format);
D3D12_SHADER_VISIBILITY       util_to_dx12_shader_visibility(ShaderStage stages);
D3D12_DESCRIPTOR_RANGE_TYPE   util_to_dx12_descriptor_range(DescriptorType type);
D3D12_RESOURCE_STATES         util_to_dx12_resource_state(ResourceState state);
D3D12_FILTER                  util_to_dx12_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool aniso,
                                                  bool comparisonFilterEnabled);
D3D12_TEXTURE_ADDRESS_MODE    util_to_dx12_texture_address_mode(AddressMode addressMode);
D3D12_PRIMITIVE_TOPOLOGY_TYPE util_to_dx12_primitive_topology_type(PrimitiveTopology topology);

//
// internal functions start with a capital letter / API starts with a small letter

// internal functions are capital first letter and capital letter of the next word

// Functions points for functions that need to be loaded
PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER           fnD3D12CreateRootSignatureDeserializer = NULL;
PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE           fnD3D12SerializeVersionedRootSignature = NULL;
PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER fnD3D12CreateVersionedRootSignatureDeserializer = NULL;
/************************************************************************/
// Descriptor Heap Defines
/************************************************************************/
typedef struct DescriptorHeapProperties
{
    uint32_t                    mMaxDescriptors;
    D3D12_DESCRIPTOR_HEAP_FLAGS mFlags;
} DescriptorHeapProperties;

DescriptorHeapProperties gCpuDescriptorHeapProperties[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {
    { 1024 * 256, D3D12_DESCRIPTOR_HEAP_FLAG_NONE }, // CBV SRV UAV
    { 2048, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },       // Sampler
    { 512, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },        // RTV
    { 512, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },        // DSV
};

typedef struct NullDescriptors
{
    ID3D12CommandSignature* pDrawCommandSignature[MAX_LINKED_GPUS];
    ID3D12CommandSignature* pDrawIndexCommandSignature[MAX_LINKED_GPUS];
    ID3D12CommandSignature* pDispatchCommandSignature[MAX_LINKED_GPUS];
#if defined(XBOX)
    ID3D12CommandSignature* pAsyncDispatchCommandSignature[MAX_LINKED_GPUS];
#endif

    // Default NULL Descriptors for binding at empty descriptor slots to make sure all descriptors are bound at submit
    DxDescriptorID mNullTextureSRV[TEXTURE_DIM_COUNT];
    DxDescriptorID mNullTextureUAV[TEXTURE_DIM_COUNT];
    DxDescriptorID mNullBufferSRV;
    DxDescriptorID mNullBufferUAV;
    DxDescriptorID mNullBufferCBV;
    DxDescriptorID mNullSampler;
} NullDescriptors;
/************************************************************************/
// Descriptor Heap Structures
/************************************************************************/
/// CPU Visible Heap to store all the resources needing CPU read / write operations - Textures/Buffers/RTV
typedef struct DescriptorHeap
{
    /// DX Heap
    ID3D12DescriptorHeap*       pHeap;
    /// Lock for multi-threaded descriptor allocations
    Mutex                       mMutex;
    ID3D12Device*               pDevice;
    /// Start position in the heap
    D3D12_CPU_DESCRIPTOR_HANDLE mStartCpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE mStartGpuHandle;
    // Bitmask to track free regions (set bit means occupied)
    uint32_t*                   pFlags;
    /// Description
    D3D12_DESCRIPTOR_HEAP_TYPE  mType;
    uint32_t                    mNumDescriptors;
    /// Descriptor Increment Size
    uint32_t                    mDescriptorSize;
    // Usage
    uint32_t                    mUsedDescriptors;
} DescriptorHeap;

typedef struct DescriptorIndexMap
{
    char*    key;
    uint32_t value;
} DescriptorIndexMap;

/************************************************************************/
// Static Descriptor Heap Implementation
/************************************************************************/

// Descriptor handles are stored in blocks, and occupancy is kept track of using the bits of a 32-bit unsigned integer.
// This means that each block contains 32 descriptor handles, one for each bit.
// When `count` descriptor handles are allocated, the occupancy masks of blocks in the descriptor heap are scanned for `count` contiguous
// zeros. If a block is found that has enough contiguous empty slots, the handles are allocated from that block and returned. Otherwise, an
// assertion is triggered that informs the user they are out of descriptor handles. When descriptor handles are freed, the correspoding bits
// in the occupancy masks are toggled off.
#define DESCRIPTOR_HEAP_BLOCK_SIZE 32

static void add_descriptor_heap(ID3D12Device* pDevice, const D3D12_DESCRIPTOR_HEAP_DESC* pDesc, DescriptorHeap** ppDescHeap)
{
    uint32_t numDescriptors = pDesc->NumDescriptors;
    hook_modify_descriptor_heap_size(pDesc->Type, &numDescriptors);

    numDescriptors = round_up(numDescriptors, DESCRIPTOR_HEAP_BLOCK_SIZE);

    const size_t sizeInBytes = (numDescriptors / DESCRIPTOR_HEAP_BLOCK_SIZE) * sizeof(uint32_t);

    DescriptorHeap* pHeap = (DescriptorHeap*)tf_calloc(1, sizeof(*pHeap) + sizeInBytes);
    pHeap->pFlags = (uint32_t*)(pHeap + 1);
    pHeap->pDevice = pDevice;

    initMutex(&pHeap->mMutex);

    D3D12_DESCRIPTOR_HEAP_DESC desc = *pDesc;
    desc.NumDescriptors = numDescriptors;

    CHECK_HRESULT(COM_CALL(CreateDescriptorHeap, pDevice, &desc, IID_ARGS(ID3D12DescriptorHeap, &pHeap->pHeap)));

    COM_CALL_RETURN_NO_ARGS(GetCPUDescriptorHandleForHeapStart, pHeap->pHeap, pHeap->mStartCpuHandle);
    ASSERT(pHeap->mStartCpuHandle.ptr);
    if (desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        COM_CALL_RETURN_NO_ARGS(GetGPUDescriptorHandleForHeapStart, pHeap->pHeap, pHeap->mStartGpuHandle);
        ASSERT(pHeap->mStartGpuHandle.ptr);
    }
    pHeap->mNumDescriptors = desc.NumDescriptors;
    pHeap->mType = desc.Type;
    pHeap->mDescriptorSize = COM_CALL(GetDescriptorHandleIncrementSize, pDevice, pHeap->mType);

    *ppDescHeap = pHeap;
}

void reset_descriptor_heap(DescriptorHeap* pHeap)
{
    memset(pHeap->pFlags, 0, (pHeap->mNumDescriptors / DESCRIPTOR_HEAP_BLOCK_SIZE) * sizeof(uint32_t));
    pHeap->mUsedDescriptors = 0;
}

static void remove_descriptor_heap(DescriptorHeap* pHeap)
{
    SAFE_RELEASE(pHeap->pHeap);
    exitMutex(&pHeap->mMutex);
    SAFE_FREE(pHeap);
}

void return_descriptor_handles_unlocked(DescriptorHeap* pHeap, DxDescriptorID handle, uint32_t count)
{
    if (D3D12_DESCRIPTOR_ID_NONE == handle || !count)
    {
        return;
    }

    for (uint32_t id = handle; id < handle + count; ++id)
    {
        const uint32_t i = id / DESCRIPTOR_HEAP_BLOCK_SIZE;
        const uint32_t mask = ~(1 << (id % DESCRIPTOR_HEAP_BLOCK_SIZE));
        pHeap->pFlags[i] &= mask;
    }

    pHeap->mUsedDescriptors -= count;
}

void return_descriptor_handles(DescriptorHeap* pHeap, DxDescriptorID handle, uint32_t count)
{
    acquireMutex(&pHeap->mMutex);
    return_descriptor_handles_unlocked(pHeap, handle, count);
    releaseMutex(&pHeap->mMutex);
}

static DxDescriptorID consume_descriptor_handles(DescriptorHeap* pHeap, uint32_t descriptorCount)
{
    if (!descriptorCount)
    {
        return D3D12_DESCRIPTOR_ID_NONE;
    }

    acquireMutex(&pHeap->mMutex);

    DxDescriptorID result = D3D12_DESCRIPTOR_ID_NONE;
    DxDescriptorID firstResult = D3D12_DESCRIPTOR_ID_NONE;
    uint32_t       foundCount = 0;

    // Scan for block with `descriptorCount` contiguous free descriptor handles
    for (uint32_t i = 0; i < pHeap->mNumDescriptors / DESCRIPTOR_HEAP_BLOCK_SIZE; ++i)
    {
        const uint32_t flag = pHeap->pFlags[i];
        if (UINT32_MAX == flag)
        {
            return_descriptor_handles_unlocked(pHeap, firstResult, foundCount);
            foundCount = 0;
            result = D3D12_DESCRIPTOR_ID_NONE;
            firstResult = D3D12_DESCRIPTOR_ID_NONE;
            continue;
        }

        for (int32_t j = 0, mask = 1; j < DESCRIPTOR_HEAP_BLOCK_SIZE; ++j, mask <<= 1)
        {
            if (!(flag & mask))
            {
                pHeap->pFlags[i] |= mask;
                result = i * DESCRIPTOR_HEAP_BLOCK_SIZE + j;

                ASSERT(result != D3D12_DESCRIPTOR_ID_NONE && "Out of descriptors");

                if (D3D12_DESCRIPTOR_ID_NONE == firstResult)
                {
                    firstResult = result;
                }

                ++foundCount;
                ++pHeap->mUsedDescriptors;

                if (foundCount == descriptorCount)
                {
                    releaseMutex(&pHeap->mMutex);
                    return firstResult;
                }
            }
            // Non contiguous. Start scanning again from this point
            else if (foundCount)
            {
                return_descriptor_handles_unlocked(pHeap, firstResult, foundCount);
                foundCount = 0;
                result = D3D12_DESCRIPTOR_ID_NONE;
                firstResult = D3D12_DESCRIPTOR_ID_NONE;
            }
        }
    }
    releaseMutex(&pHeap->mMutex);

    ASSERT(result != D3D12_DESCRIPTOR_ID_NONE && "Out of descriptors");
    return firstResult;
}

static inline FORGE_CONSTEXPR D3D12_CPU_DESCRIPTOR_HANDLE descriptor_id_to_cpu_handle(DescriptorHeap* pHeap, DxDescriptorID id)
{
    return (D3D12_CPU_DESCRIPTOR_HANDLE){ pHeap->mStartCpuHandle.ptr + id * pHeap->mDescriptorSize };
}

static inline FORGE_CONSTEXPR D3D12_GPU_DESCRIPTOR_HANDLE descriptor_id_to_gpu_handle(DescriptorHeap* pHeap, DxDescriptorID id)
{
    return (D3D12_GPU_DESCRIPTOR_HANDLE){ pHeap->mStartGpuHandle.ptr + id * pHeap->mDescriptorSize };
}

static void copy_descriptor_handle(DescriptorHeap* pSrcHeap, DxDescriptorID srcId, DescriptorHeap* pDstHeap, DxDescriptorID dstId)
{
    ASSERT(pSrcHeap->mType == pDstHeap->mType);
    D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = descriptor_id_to_cpu_handle(pSrcHeap, srcId);
    D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = descriptor_id_to_cpu_handle(pDstHeap, dstId);
    COM_CALL(CopyDescriptorsSimple, pSrcHeap->pDevice, 1, dstHandle, srcHandle, pSrcHeap->mType);
}
/************************************************************************/
// Multi GPU Helper Functions
/************************************************************************/
uint32_t util_calculate_shared_node_mask(Renderer* pRenderer)
{
    if (pRenderer->mGpuMode == GPU_MODE_LINKED)
        return (1 << pRenderer->mLinkedNodeCount) - 1;
    else
        return 0;
}

uint32_t util_calculate_node_mask(Renderer* pRenderer, uint32_t i)
{
    if (pRenderer->mGpuMode == GPU_MODE_LINKED)
        return (1 << i);
    else
        return 0;
}
/************************************************************************/
/************************************************************************/
FORGE_CONSTEXPR D3D12_DEPTH_STENCIL_DESC util_to_depth_desc(const DepthStateDesc* pDesc)
{
    ASSERT(pDesc->mDepthFunc < MAX_COMPARE_MODES);
    ASSERT(pDesc->mStencilFrontFunc < MAX_COMPARE_MODES);
    ASSERT(pDesc->mStencilFrontFail < MAX_STENCIL_OPS);
    ASSERT(pDesc->mDepthFrontFail < MAX_STENCIL_OPS);
    ASSERT(pDesc->mStencilFrontPass < MAX_STENCIL_OPS);
    ASSERT(pDesc->mStencilBackFunc < MAX_COMPARE_MODES);
    ASSERT(pDesc->mStencilBackFail < MAX_STENCIL_OPS);
    ASSERT(pDesc->mDepthBackFail < MAX_STENCIL_OPS);
    ASSERT(pDesc->mStencilBackPass < MAX_STENCIL_OPS);

    return (D3D12_DEPTH_STENCIL_DESC){
        .DepthEnable = (BOOL)pDesc->mDepthTest,
        .DepthFunc = gDx12ComparisonFuncTranslator[pDesc->mDepthFunc],
        .DepthWriteMask = pDesc->mDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO,
        .StencilEnable = (BOOL)pDesc->mStencilTest,
        .StencilReadMask = pDesc->mStencilReadMask,
        .StencilWriteMask = pDesc->mStencilWriteMask,
        .BackFace =
            (D3D12_DEPTH_STENCILOP_DESC){
                .StencilFunc = gDx12ComparisonFuncTranslator[pDesc->mStencilBackFunc],
                .StencilDepthFailOp = gDx12StencilOpTranslator[pDesc->mDepthBackFail],
                .StencilFailOp = gDx12StencilOpTranslator[pDesc->mStencilBackFail],
                .StencilPassOp = gDx12StencilOpTranslator[pDesc->mStencilBackPass],
            },
        .FrontFace =
            (D3D12_DEPTH_STENCILOP_DESC){
                .StencilFunc = gDx12ComparisonFuncTranslator[pDesc->mStencilFrontFunc],
                .StencilDepthFailOp = gDx12StencilOpTranslator[pDesc->mDepthFrontFail],
                .StencilFailOp = gDx12StencilOpTranslator[pDesc->mStencilFrontFail],
                .StencilPassOp = gDx12StencilOpTranslator[pDesc->mStencilFrontPass],
            },
    };
}

static inline FORGE_CONSTEXPR uint8_t ToColorWriteMask(ColorMask mask)
{
    uint8_t ret = 0;
    if (mask & COLOR_MASK_RED)
    {
        ret |= D3D12_COLOR_WRITE_ENABLE_RED;
    }
    if (mask & COLOR_MASK_GREEN)
    {
        ret |= D3D12_COLOR_WRITE_ENABLE_GREEN;
    }
    if (mask & COLOR_MASK_BLUE)
    {
        ret |= D3D12_COLOR_WRITE_ENABLE_BLUE;
    }
    if (mask & COLOR_MASK_ALPHA)
    {
        ret |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
    }

    return ret;
}

FORGE_CONSTEXPR D3D12_BLEND_DESC util_to_blend_desc(const BlendStateDesc* pDesc)
{
    int blendDescIndex = 0;
#if defined(ENABLE_GRAPHICS_RUNTIME_CHECK)

    for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
    {
        if (pDesc->mRenderTargetMask & (1 << i))
        {
            ASSERT(pDesc->mSrcFactors[blendDescIndex] < MAX_BLEND_CONSTANTS);
            ASSERT(pDesc->mDstFactors[blendDescIndex] < MAX_BLEND_CONSTANTS);
            ASSERT(pDesc->mSrcAlphaFactors[blendDescIndex] < MAX_BLEND_CONSTANTS);
            ASSERT(pDesc->mDstAlphaFactors[blendDescIndex] < MAX_BLEND_CONSTANTS);
            ASSERT(pDesc->mBlendModes[blendDescIndex] < MAX_BLEND_MODES);
            ASSERT(pDesc->mBlendAlphaModes[blendDescIndex] < MAX_BLEND_MODES);
        }

        if (pDesc->mIndependentBlend)
            ++blendDescIndex;
    }

    blendDescIndex = 0;
#endif

    D3D12_BLEND_DESC ret = { 0 };
    ret.AlphaToCoverageEnable = (BOOL)pDesc->mAlphaToCoverage;
    ret.IndependentBlendEnable = TRUE;
    for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; i++)
    {
        if (pDesc->mRenderTargetMask & (1 << i))
        {
            BOOL blendEnable = (gDx12BlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]] != D3D12_BLEND_ONE ||
                                gDx12BlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]] != D3D12_BLEND_ZERO ||
                                gDx12BlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]] != D3D12_BLEND_ONE ||
                                gDx12BlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]] != D3D12_BLEND_ZERO);

            ret.RenderTarget[i] = (D3D12_RENDER_TARGET_BLEND_DESC){
                .BlendEnable = blendEnable,
                .RenderTargetWriteMask = ToColorWriteMask(pDesc->mColorWriteMasks[blendDescIndex]),
                .BlendOp = gDx12BlendOpTranslator[pDesc->mBlendModes[blendDescIndex]],
                .SrcBlend = gDx12BlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]],
                .DestBlend = gDx12BlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]],
                .BlendOpAlpha = gDx12BlendOpTranslator[pDesc->mBlendAlphaModes[blendDescIndex]],
                .SrcBlendAlpha = gDx12BlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]],
                .DestBlendAlpha = gDx12BlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]],
            };
        }

        if (pDesc->mIndependentBlend)
            ++blendDescIndex;
    }

    return ret;
}

FORGE_CONSTEXPR D3D12_RASTERIZER_DESC util_to_rasterizer_desc(const RasterizerStateDesc* pDesc)
{
    ASSERT(pDesc->mFillMode < MAX_FILL_MODES);
    ASSERT(pDesc->mCullMode < MAX_CULL_MODES);
    ASSERT(pDesc->mFrontFace == FRONT_FACE_CCW || pDesc->mFrontFace == FRONT_FACE_CW);

    return (D3D12_RASTERIZER_DESC){
        .FillMode = gDx12FillModeTranslator[pDesc->mFillMode],
        .CullMode = gDx12CullModeTranslator[pDesc->mCullMode],
        .FrontCounterClockwise = pDesc->mFrontFace == FRONT_FACE_CCW,
        .DepthBias = pDesc->mDepthBias,
        .DepthBiasClamp = 0.0f,
        .SlopeScaledDepthBias = pDesc->mSlopeScaledDepthBias,
        .DepthClipEnable = !pDesc->mDepthClampEnable,
        .MultisampleEnable = pDesc->mMultiSample ? TRUE : FALSE,
        .AntialiasedLineEnable = FALSE,
        .ForcedSampleCount = 0,
        .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
    };
}
/************************************************************************/
// Globals
/************************************************************************/
static const uint32_t gDescriptorTableDWORDS = 1;
static const uint32_t gRootDescriptorDWORDS = 2;
static const uint32_t gMaxRootConstantsPerRootParam = 4U;
/************************************************************************/
// Logging functions
/************************************************************************/
void AddSrv(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc,
            DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pOptionalHeap ? pOptionalHeap : pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    COM_CALL(CreateShaderResourceView, pRenderer->mDx.pDevice, pResource, pSrvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void AddBufferSrv(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pBuffer, bool raw, uint32_t firstElement,
                         uint32_t elementCount, uint32_t stride, DxDescriptorID* pOutSrv)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
        .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Format = DXGI_FORMAT_UNKNOWN,
        .Buffer =
            (D3D12_BUFFER_SRV){
                .FirstElement = firstElement,
                .NumElements = elementCount,
                .StructureByteStride = stride,
                .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
            },
    };
    if (raw)
    {
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.Buffer.StructureByteStride = 0;
        srvDesc.Buffer.Flags |= D3D12_BUFFER_SRV_FLAG_RAW;
    }

    AddSrv(pRenderer, pOptionalHeap, pBuffer, &srvDesc, pOutSrv);
}

static void AddTypedBufferSrv(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pBuffer, uint32_t firstElement,
                              uint32_t elementCount, TinyImageFormat format, DxDescriptorID* pOutSrv)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
        .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(format),
        .Buffer =
            (D3D12_BUFFER_SRV){
                .FirstElement = firstElement,
                .NumElements = elementCount,
                .StructureByteStride = 0,
                .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
            },
    };
    AddSrv(pRenderer, pOptionalHeap, pBuffer, &srvDesc, pOutSrv);
}

static void AddUav(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pResource, ID3D12Resource* pCounterResource,
                   const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc, DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pOptionalHeap ? pOptionalHeap : pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    COM_CALL(CreateUnorderedAccessView, pRenderer->mDx.pDevice, pResource, pCounterResource, pUavDesc,
             descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void AddBufferUav(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pBuffer, ID3D12Resource* pCounterBuffer,
                         uint32_t counterOffset, bool raw, uint32_t firstElement, uint32_t elementCount, uint32_t stride,
                         DxDescriptorID* pOutUav)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
        .Format = DXGI_FORMAT_UNKNOWN,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer =
            (D3D12_BUFFER_UAV){
                .FirstElement = firstElement,
                .NumElements = elementCount,
                .StructureByteStride = stride,
                .CounterOffsetInBytes = counterOffset,
                .Flags = D3D12_BUFFER_UAV_FLAG_NONE,
            },
    };
    if (raw)
    {
        uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        uavDesc.Buffer.StructureByteStride = 0;
        uavDesc.Buffer.Flags |= D3D12_BUFFER_UAV_FLAG_RAW;
    }

    AddUav(pRenderer, pOptionalHeap, pBuffer, pCounterBuffer, &uavDesc, pOutUav);
}

static void AddTypedBufferUav(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pBuffer, uint32_t firstElement,
                              uint32_t elementCount, TinyImageFormat format, DxDescriptorID* pOutUav)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(format),
        .Buffer =
            (D3D12_BUFFER_UAV){
                .FirstElement = firstElement,
                .NumElements = elementCount,
                .StructureByteStride = 0,
                .CounterOffsetInBytes = 0,
                .Flags = D3D12_BUFFER_UAV_FLAG_NONE,
            },
    };

    D3D12_FEATURE_DATA_FORMAT_SUPPORT FormatSupport = { uavDesc.Format, D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE };
    HRESULT hr = COM_CALL(CheckFeatureSupport, pRenderer->mDx.pDevice, D3D12_FEATURE_FORMAT_SUPPORT, &FormatSupport, sizeof(FormatSupport));
    if (!SUCCEEDED(hr) || !(FormatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) ||
        !(FormatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
    {
        // Format does not support UAV Typed Load
        LOGF(eWARNING, "Cannot use Typed UAV for buffer format %u", (uint32_t)format);
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    }

    AddUav(pRenderer, pOptionalHeap, pBuffer, NULL, &uavDesc, pOutUav);
}

static void AddCbv(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, const D3D12_CONSTANT_BUFFER_VIEW_DESC* pCbvDesc,
                   DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pOptionalHeap ? pOptionalHeap : pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    COM_CALL(CreateConstantBufferView, pRenderer->mDx.pDevice, pCbvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void AddRtv(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pResource, DXGI_FORMAT format, uint32_t mipSlice,
                   uint32_t arraySlice, DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pOptionalHeap ? pOptionalHeap : pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { 0 };
    D3D12_RESOURCE_DESC           desc;
    COM_CALL(GetDesc, pResource, &desc);
    D3D12_RESOURCE_DIMENSION type = desc.Dimension;

    rtvDesc.Format = format;

    switch (type)
    {
    case D3D12_RESOURCE_DIMENSION_BUFFER:
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        if (desc.DepthOrArraySize > 1)
        {
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
            rtvDesc.Texture1DArray.MipSlice = mipSlice;
            if (arraySlice != -1)
            {
                rtvDesc.Texture1DArray.ArraySize = 1;
                rtvDesc.Texture1DArray.FirstArraySlice = arraySlice;
            }
            else
            {
                rtvDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
            }
        }
        else
        {
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
            rtvDesc.Texture1D.MipSlice = mipSlice;
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        if (desc.SampleDesc.Count > 1)
        {
            if (desc.DepthOrArraySize > 1)
            {
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                if (arraySlice != -1)
                {
                    rtvDesc.Texture2DMSArray.ArraySize = 1;
                    rtvDesc.Texture2DMSArray.FirstArraySlice = arraySlice;
                }
                else
                {
                    rtvDesc.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
                }
            }
            else
            {
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            }
        }
        else
        {
            if (desc.DepthOrArraySize > 1)
            {
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice = mipSlice;
                if (arraySlice != -1)
                {
                    rtvDesc.Texture2DArray.ArraySize = 1;
                    rtvDesc.Texture2DArray.FirstArraySlice = arraySlice;
                }
                else
                {
                    rtvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                }
            }
            else
            {
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Texture2D.MipSlice = mipSlice;
            }
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
        rtvDesc.Texture3D.MipSlice = mipSlice;
        if (arraySlice != -1)
        {
            rtvDesc.Texture3D.WSize = 1;
            rtvDesc.Texture3D.FirstWSlice = arraySlice;
        }
        else
        {
            rtvDesc.Texture3D.WSize = desc.DepthOrArraySize;
        }
        break;
    default:
        break;
    }

    COM_CALL(CreateRenderTargetView, pRenderer->mDx.pDevice, pResource, &rtvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void AddDsv(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, ID3D12Resource* pResource, DXGI_FORMAT format, uint32_t mipSlice,
                   uint32_t arraySlice, DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pOptionalHeap ? pOptionalHeap : pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = { 0 };
    D3D12_RESOURCE_DESC           desc;
    COM_CALL(GetDesc, pResource, &desc);
    D3D12_RESOURCE_DIMENSION type = desc.Dimension;

    dsvDesc.Format = format;

    switch (type)
    {
    case D3D12_RESOURCE_DIMENSION_BUFFER:
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        if (desc.DepthOrArraySize > 1)
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
            dsvDesc.Texture1DArray.MipSlice = mipSlice;
            if (arraySlice != -1)
            {
                dsvDesc.Texture1DArray.ArraySize = 1;
                dsvDesc.Texture1DArray.FirstArraySlice = arraySlice;
            }
            else
            {
                dsvDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
            }
        }
        else
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
            dsvDesc.Texture1D.MipSlice = mipSlice;
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        if (desc.SampleDesc.Count > 1)
        {
            if (desc.DepthOrArraySize > 1)
            {
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                if (arraySlice != -1)
                {
                    dsvDesc.Texture2DMSArray.ArraySize = 1;
                    dsvDesc.Texture2DMSArray.FirstArraySlice = arraySlice;
                }
                else
                {
                    dsvDesc.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
                }
            }
            else
            {
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
            }
        }
        else
        {
            if (desc.DepthOrArraySize > 1)
            {
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                dsvDesc.Texture2DArray.MipSlice = mipSlice;
                if (arraySlice != -1)
                {
                    dsvDesc.Texture2DArray.ArraySize = 1;
                    dsvDesc.Texture2DArray.FirstArraySlice = arraySlice;
                }
                else
                {
                    dsvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                }
            }
            else
            {
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                dsvDesc.Texture2D.MipSlice = mipSlice;
            }
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
        ASSERT(false && "Cannot create 3D Depth Stencil");
        break;
    default:
        break;
    }

    COM_CALL(CreateDepthStencilView, pRenderer->mDx.pDevice, pResource, &dsvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void AddSampler(Renderer* pRenderer, DescriptorHeap* pOptionalHeap, const D3D12_SAMPLER_DESC* pSamplerDesc, DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pOptionalHeap ? pOptionalHeap : pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    COM_CALL(CreateSampler, pRenderer->mDx.pDevice, pSamplerDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

D3D12_DEPTH_STENCIL_DESC gDefaultDepthDesc = { 0 };
D3D12_BLEND_DESC         gDefaultBlendDesc = { 0 };
D3D12_RASTERIZER_DESC    gDefaultRasterizerDesc = { 0 };

static void add_default_resources(Renderer* pRenderer)
{
    pRenderer->pNullDescriptors = (NullDescriptors*)tf_calloc(1, sizeof(NullDescriptors));
    for (uint32_t i = 0; i < TEXTURE_DIM_COUNT; ++i)
    {
        pRenderer->pNullDescriptors->mNullTextureSRV[i] = D3D12_DESCRIPTOR_ID_NONE;
        pRenderer->pNullDescriptors->mNullTextureUAV[i] = D3D12_DESCRIPTOR_ID_NONE;
    }
    pRenderer->pNullDescriptors->mNullBufferSRV = D3D12_DESCRIPTOR_ID_NONE;
    pRenderer->pNullDescriptors->mNullBufferUAV = D3D12_DESCRIPTOR_ID_NONE;
    pRenderer->pNullDescriptors->mNullBufferCBV = D3D12_DESCRIPTOR_ID_NONE;
    pRenderer->pNullDescriptors->mNullSampler = D3D12_DESCRIPTOR_ID_NONE;

    // Create NULL descriptors in case user does not specify some descriptors we can bind null descriptor handles at those points
    D3D12_SAMPLER_DESC samplerDesc = { 0 };
    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    AddSampler(pRenderer, NULL, &samplerDesc, &pRenderer->pNullDescriptors->mNullSampler);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { 0 };
    srvDesc.Format = DXGI_FORMAT_R8_UINT;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = { 0 };
    uavDesc.Format = DXGI_FORMAT_R8_UINT;

    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_1D]);
    AddUav(pRenderer, NULL, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->mNullTextureUAV[TEXTURE_DIM_1D]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_2D]);
    AddUav(pRenderer, NULL, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->mNullTextureUAV[TEXTURE_DIM_2D]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_2DMS]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_3D]);
    AddUav(pRenderer, NULL, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->mNullTextureUAV[TEXTURE_DIM_3D]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_1D_ARRAY]);
    AddUav(pRenderer, NULL, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->mNullTextureUAV[TEXTURE_DIM_1D_ARRAY]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_2D_ARRAY]);
    AddUav(pRenderer, NULL, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->mNullTextureUAV[TEXTURE_DIM_2D_ARRAY]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_2DMS_ARRAY]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_CUBE]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_CUBE_ARRAY]);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    AddSrv(pRenderer, NULL, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullBufferSRV);
    AddUav(pRenderer, NULL, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->mNullBufferUAV);
    AddCbv(pRenderer, NULL, NULL, &pRenderer->pNullDescriptors->mNullBufferCBV);

    BlendStateDesc blendStateDesc = {
        .mDstAlphaFactors = { BC_ZERO },
        .mDstFactors = { BC_ZERO },
        .mSrcAlphaFactors = { BC_ONE },
        .mSrcFactors = { BC_ONE },
        .mColorWriteMasks = { COLOR_MASK_ALL },
        .mRenderTargetMask = BLEND_STATE_TARGET_ALL,
        .mIndependentBlend = false,
    };
    gDefaultBlendDesc = util_to_blend_desc(&blendStateDesc);

    DepthStateDesc depthStateDesc = {
        .mDepthFunc = CMP_LEQUAL,
        .mDepthTest = false,
        .mDepthWrite = false,
        .mStencilBackFunc = CMP_ALWAYS,
        .mStencilFrontFunc = CMP_ALWAYS,
        .mStencilReadMask = 0xFF,
        .mStencilWriteMask = 0xFF,
    };
    gDefaultDepthDesc = util_to_depth_desc(&depthStateDesc);

    RasterizerStateDesc rasterizerStateDesc = {
        .mCullMode = CULL_MODE_BACK,
    };
    gDefaultRasterizerDesc = util_to_rasterizer_desc(&rasterizerStateDesc);

    for (uint32_t l = 0; l < pRenderer->mLinkedNodeCount; ++l)
    {
        D3D12_INDIRECT_ARGUMENT_DESC arg = { 0 };
        D3D12_COMMAND_SIGNATURE_DESC desc = {
            .NodeMask = util_calculate_node_mask(pRenderer, l),
            .NumArgumentDescs = 1,
            .pArgumentDescs = &arg,
        };

        arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
        desc.ByteStride = sizeof(IndirectDrawArguments);
        CHECK_HRESULT(COM_CALL(CreateCommandSignature, pRenderer->mDx.pDevice, &desc, NULL,
                               IID_ARGS(ID3D12CommandSignature, &pRenderer->pNullDescriptors->pDrawCommandSignature[l])));

        arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        desc.ByteStride = sizeof(IndirectDrawIndexArguments);
        CHECK_HRESULT(COM_CALL(CreateCommandSignature, pRenderer->mDx.pDevice, &desc, NULL,
                               IID_ARGS(ID3D12CommandSignature, &pRenderer->pNullDescriptors->pDrawIndexCommandSignature[l])));

        arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        desc.ByteStride = sizeof(IndirectDispatchArguments);
        CHECK_HRESULT(COM_CALL(CreateCommandSignature, pRenderer->mDx.pDevice, &desc, NULL,
                               IID_ARGS(ID3D12CommandSignature, &pRenderer->pNullDescriptors->pDispatchCommandSignature[l])));

#if defined(XBOX)
        hook_fill_dispatch_indirect_argument_desc(&arg, true);
        desc.ByteStride = sizeof(IndirectDispatchArguments);
        CHECK_HRESULT(COM_CALL(CreateCommandSignature, pRenderer->mDx.pDevice, &desc, NULL,
                               IID_ARGS(ID3D12CommandSignature, &pRenderer->pNullDescriptors->pAsyncDispatchCommandSignature[l])));
#endif
    }
}

static void remove_default_resources(Renderer* pRenderer)
{
    return_descriptor_handles(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER],
                              pRenderer->pNullDescriptors->mNullSampler, 1);

    DescriptorHeap* heap = pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    for (uint32_t i = 0; i < TEXTURE_DIM_COUNT; ++i)
    {
        return_descriptor_handles(heap, pRenderer->pNullDescriptors->mNullTextureSRV[i], 1);
        return_descriptor_handles(heap, pRenderer->pNullDescriptors->mNullTextureUAV[i], 1);
    }
    return_descriptor_handles(heap, pRenderer->pNullDescriptors->mNullBufferSRV, 1);
    return_descriptor_handles(heap, pRenderer->pNullDescriptors->mNullBufferUAV, 1);
    return_descriptor_handles(heap, pRenderer->pNullDescriptors->mNullBufferCBV, 1);

    for (uint32_t l = 0; l < pRenderer->mLinkedNodeCount; ++l)
    {
        SAFE_RELEASE(pRenderer->pNullDescriptors->pDrawCommandSignature[l]);
        SAFE_RELEASE(pRenderer->pNullDescriptors->pDrawIndexCommandSignature[l]);
        SAFE_RELEASE(pRenderer->pNullDescriptors->pDispatchCommandSignature[l]);
#if defined(XBOX)
        SAFE_RELEASE(pRenderer->pNullDescriptors->pAsyncDispatchCommandSignature[l]);
#endif
    }

    SAFE_FREE(pRenderer->pNullDescriptors);
}
/************************************************************************/
// D3D12 Dynamic Loader
/************************************************************************/

typedef HRESULT (*PFN_D3D12_CREATE_DEVICE_FIXED)(_In_opt_ void* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, _In_ REFIID riid,
                                                 _COM_Outptr_opt_ void** ppDevice);

#if defined(FORGE_D3D12_DYNAMIC_LOADING)

typedef HRESULT(WINAPI* PFN_D3D12_EnableExperimentalFeatures)(UINT, const IID*, void*, UINT*);

typedef HRESULT(WINAPI* PFN_D3D12_SerializeVersionedRootSignature)(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC*, ID3DBlob**, ID3DBlob**);

typedef HRESULT(WINAPI* PFN_DXGI_CreateDXGIFactory2)(UINT, REFIID riid, void** ppFactory);

typedef HRESULT(WINAPI* PFN_DXGI_DXGIGetDebugInterface1)(UINT Flags, REFIID riid, _COM_Outptr_ void** pDebug);

static bool                                      gD3D12dllInited = false;
static PFN_D3D12_CREATE_DEVICE_FIXED             gPfnCreateDevice = NULL;
static PFN_D3D12_GET_DEBUG_INTERFACE             gPfnGetDebugInterface = NULL;
static PFN_D3D12_EnableExperimentalFeatures      gPfnEnableExperimentalFeatures = NULL;
static PFN_D3D12_SerializeVersionedRootSignature gPfnSerializeVersionedRootSignature = NULL;
static PFN_DXGI_CreateDXGIFactory2               gPfnCreateDXGIFactory2 = NULL;
static PFN_DXGI_DXGIGetDebugInterface1           gPfnDXGIGetDebugInterface1 = NULL;
static HMODULE                                   gD3D12dll = NULL;
static HMODULE                                   gDXGIdll = NULL;

#endif

void d3d12dll_exit()
{
#if defined(FORGE_D3D12_DYNAMIC_LOADING)
    if (gD3D12dll)
    {
        LOGF(eINFO, "Unloading d3d12.dll");
        gPfnCreateDevice = NULL;
        gPfnGetDebugInterface = NULL;
        gPfnEnableExperimentalFeatures = NULL;
        gPfnSerializeVersionedRootSignature = NULL;
        FreeLibrary(gD3D12dll);
        gD3D12dll = NULL;
    }

    if (gDXGIdll)
    {
        LOGF(eINFO, "Unloading dxgi.dll");
        gPfnCreateDXGIFactory2 = NULL;
        gPfnDXGIGetDebugInterface1 = NULL;
        FreeLibrary(gDXGIdll);
        gDXGIdll = NULL;
    }

    gD3D12dllInited = false;
#endif
}

bool d3d12dll_init()
{
#if defined(FORGE_D3D12_DYNAMIC_LOADING)

    if (gD3D12dllInited)
        return true;

    LOGF(eINFO, "Loading d3d12.dll");
    gD3D12dll = LoadLibraryExA("d3d12.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (gD3D12dll)
    {
        gPfnCreateDevice = (PFN_D3D12_CREATE_DEVICE_FIXED)(GetProcAddress(gD3D12dll, "D3D12CreateDevice"));
        gPfnGetDebugInterface = (PFN_D3D12_GET_DEBUG_INTERFACE)(GetProcAddress(gD3D12dll, "D3D12GetDebugInterface"));
        gPfnEnableExperimentalFeatures =
            (PFN_D3D12_EnableExperimentalFeatures)(GetProcAddress(gD3D12dll, "D3D12EnableExperimentalFeatures"));
        gPfnSerializeVersionedRootSignature =
            (PFN_D3D12_SerializeVersionedRootSignature)(GetProcAddress(gD3D12dll, "D3D12SerializeVersionedRootSignature"));
    }

    LOGF(eINFO, "Loading dxgi.dll");
    gDXGIdll = LoadLibraryExA("dxgi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (gDXGIdll)
    {
        gPfnCreateDXGIFactory2 = (PFN_DXGI_CreateDXGIFactory2)(GetProcAddress(gDXGIdll, "CreateDXGIFactory2"));
        gPfnDXGIGetDebugInterface1 = (PFN_DXGI_DXGIGetDebugInterface1)(GetProcAddress(gDXGIdll, "DXGIGetDebugInterface1"));
    }

    gD3D12dllInited = gDXGIdll && gD3D12dll;
    if (!gD3D12dllInited)
        d3d12dll_exit();
    return gD3D12dllInited;
#else
    return true;
#endif
}
HRESULT WINAPI d3d12dll_CreateDevice(void* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice) //-V835
{                                                                                                                         //-V835
#if defined(FORGE_D3D12_DYNAMIC_LOADING)
    if (gPfnCreateDevice)
        return gPfnCreateDevice(pAdapter, MinimumFeatureLevel, riid, ppDevice);
    return D3D12_ERROR_ADAPTER_NOT_FOUND;
#else
    return ((PFN_D3D12_CREATE_DEVICE_FIXED)D3D12CreateDevice)(pAdapter, MinimumFeatureLevel, riid, ppDevice);
#endif
}

HRESULT WINAPI d3d12dll_GetDebugInterface(REFIID riid, void** ppvDebug) //-V835
{                                                                       //-V835
#if defined(FORGE_D3D12_DYNAMIC_LOADING)
    if (gPfnGetDebugInterface)
        return gPfnGetDebugInterface(riid, ppvDebug);
    return E_FAIL;
#else
    return D3D12GetDebugInterface(riid, ppvDebug);
#endif
}

#if !defined(XBOX)
static HRESULT WINAPI d3d12dll_EnableExperimentalFeatures(UINT NumFeatures, const IID* pIIDs, void* pConfigurationStructs,
                                                          UINT* pConfigurationStructSizes)
{
#if defined(FORGE_D3D12_DYNAMIC_LOADING)
    if (gPfnEnableExperimentalFeatures)
        return gPfnEnableExperimentalFeatures(NumFeatures, pIIDs, pConfigurationStructs, pConfigurationStructSizes);
    return E_NOTIMPL;
#else
    return D3D12EnableExperimentalFeatures(NumFeatures, pIIDs, pConfigurationStructs, pConfigurationStructSizes);
#endif
}
#endif

HRESULT d3d12dll_SerializeVersionedRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* pRootSignature, ID3DBlob** ppBlob,
                                                 ID3DBlob** ppErrorBlob)
{
#if defined(FORGE_D3D12_DYNAMIC_LOADING)
    if (gPfnSerializeVersionedRootSignature)
        return gPfnSerializeVersionedRootSignature(pRootSignature, ppBlob, ppErrorBlob);
    return E_NOTIMPL;
#else
    return D3D12SerializeVersionedRootSignature(pRootSignature, ppBlob, ppErrorBlob);
#endif
}

#if !defined(XBOX)
static HRESULT WINAPI d3d12dll_CreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory) //-V835
{                                                                                            //-V835
#if defined(FORGE_D3D12_DYNAMIC_LOADING)
    if (gPfnCreateDXGIFactory2)
        return gPfnCreateDXGIFactory2(Flags, riid, ppFactory);
    return DXGI_ERROR_UNSUPPORTED;
#else
    return CreateDXGIFactory2(Flags, riid, ppFactory);
#endif
}

HRESULT WINAPI d3d12dll_DXGIGetDebugInterface1(UINT Flags, REFIID riid, void** ppFactory) //-V835
{                                                                                         //-V835
#if defined(FORGE_D3D12_DYNAMIC_LOADING)
    if (gPfnDXGIGetDebugInterface1)
        return gPfnDXGIGetDebugInterface1(Flags, riid, ppFactory);
    return DXGI_ERROR_UNSUPPORTED;
#else
    return DXGIGetDebugInterface1(Flags, riid, ppFactory);
#endif
}
#endif

/************************************************************************/
// Internal utility functions
/************************************************************************/
D3D12_FILTER
util_to_dx12_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool aniso, bool comparisonFilterEnabled)
{
    if (aniso)
        return (comparisonFilterEnabled ? D3D12_FILTER_COMPARISON_ANISOTROPIC : D3D12_FILTER_ANISOTROPIC);

    // control bit : minFilter  magFilter   mipMapMode
    //   point     :    00         00           00
    //   linear    :    01         01           01
    // ex : trilinear == 010101
    int filter = (minFilter << 4) | (magFilter << 2) | mipMapMode;
    int baseFilter = comparisonFilterEnabled ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_POINT;
    return (D3D12_FILTER)(baseFilter + filter);
}

D3D12_HEAP_TYPE util_to_heap_type(ResourceMemoryUsage memoryUsage)
{
    switch (memoryUsage)
    {
    case RESOURCE_MEMORY_USAGE_GPU_ONLY:
        return D3D12_HEAP_TYPE_DEFAULT;
    case RESOURCE_MEMORY_USAGE_CPU_ONLY:
    case RESOURCE_MEMORY_USAGE_CPU_TO_GPU:
        return D3D12_HEAP_TYPE_UPLOAD;
    case RESOURCE_MEMORY_USAGE_GPU_TO_CPU:
        return D3D12_HEAP_TYPE_READBACK;
    default:
        ASSERT(false);
        break;
    }

    return D3D12_HEAP_TYPE_DEFAULT;
}

D3D12_HEAP_FLAGS util_to_heap_flags(ResourceHeapCreationFlags flags)
{
    D3D12_HEAP_FLAGS outFlags = D3D12_HEAP_FLAG_NONE;

    if (flags & RESOURCE_HEAP_FLAG_SHARED)
        outFlags |= D3D12_HEAP_FLAG_SHARED;
    if (flags & RESOURCE_HEAP_FLAG_DENY_BUFFERS)
        outFlags |= D3D12_HEAP_FLAG_DENY_BUFFERS;
    if (flags & RESOURCE_HEAP_FLAG_ALLOW_DISPLAY)
        outFlags |= D3D12_HEAP_FLAG_ALLOW_DISPLAY;
    if (flags & RESOURCE_HEAP_FLAG_SHARED_CROSS_ADAPTER)
        outFlags |= D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER;
    if (flags & RESOURCE_HEAP_FLAG_DENY_RT_DS_TEXTURES)
        outFlags |= D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
    if (flags & RESOURCE_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES)
        outFlags |= D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
    if (flags & RESOURCE_HEAP_FLAG_HARDWARE_PROTECTED)
        outFlags |= D3D12_HEAP_FLAG_HARDWARE_PROTECTED;
    if (flags & RESOURCE_HEAP_FLAG_ALLOW_WRITE_WATCH)
        outFlags |= D3D12_HEAP_FLAG_ALLOW_WRITE_WATCH;

#if !defined(XBOX)
    if (flags & RESOURCE_HEAP_FLAG_ALLOW_SHADER_ATOMICS)
        outFlags |= D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
#endif

    if (flags == RESOURCE_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES)
        outFlags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;

    return outFlags;
}

D3D12_TEXTURE_ADDRESS_MODE util_to_dx12_texture_address_mode(AddressMode addressMode)
{
    switch (addressMode)
    {
    case ADDRESS_MODE_MIRROR:
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    case ADDRESS_MODE_REPEAT:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case ADDRESS_MODE_CLAMP_TO_EDGE:
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case ADDRESS_MODE_CLAMP_TO_BORDER:
        return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    default:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE util_to_dx12_primitive_topology_type(PrimitiveTopology topology)
{
    switch (topology)
    {
    case PRIMITIVE_TOPO_POINT_LIST:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    case PRIMITIVE_TOPO_LINE_LIST:
    case PRIMITIVE_TOPO_LINE_STRIP:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case PRIMITIVE_TOPO_TRI_LIST:
    case PRIMITIVE_TOPO_TRI_STRIP:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    case PRIMITIVE_TOPO_PATCH_LIST:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    default:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    }
}

uint64_t util_dx12_determine_storage_counter_offset(uint64_t buffer_size)
{
    uint64_t alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
    uint64_t result = (buffer_size + (alignment - 1)) & ~(alignment - 1);
    return result;
}

DXGI_FORMAT util_to_dx12_uav_format(DXGI_FORMAT defaultFormat)
{
    switch (defaultFormat)
    {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM;

    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8X8_UNORM;

    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;

#if defined(ENABLE_GRAPHICS_RUNTIME_CHECK)
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_D16_UNORM:
        LOGF(eERROR, "Requested a UAV format for a depth stencil format");
#endif

    default:
        return defaultFormat;
    }
}

DXGI_FORMAT util_to_dx12_dsv_format(DXGI_FORMAT defaultFormat)
{
    switch (defaultFormat)
    {
        // 32-bit Z w/ Stencil
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

        // No Stencil
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_D32_FLOAT;

        // 24-bit Z
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;

        // 16-bit Z w/o Stencil
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_D16_UNORM;

    default:
        return defaultFormat;
    }
}

DXGI_FORMAT util_to_dx12_srv_format(DXGI_FORMAT defaultFormat, bool isStencil)
{
    DXGI_FORMAT platformSpecificFormat = hook_to_dx12_srv_platform_format(defaultFormat, isStencil);
    if (platformSpecificFormat != DXGI_FORMAT_UNKNOWN)
    {
        return platformSpecificFormat;
    }

    switch (defaultFormat)
    {
        // 32-bit Z w/ Stencil
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return isStencil ? DXGI_FORMAT_X32_TYPELESS_G8X24_UINT : DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

        // No Stencil
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
        return isStencil ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_R32_FLOAT;

        // 24-bit Z
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return isStencil ? DXGI_FORMAT_X24_TYPELESS_G8_UINT : DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

        // 16-bit Z w/o Stencil
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
        return isStencil ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_R16_UNORM;

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    default:
        return defaultFormat;
    }
}

DXGI_FORMAT util_to_dx12_stencil_format(DXGI_FORMAT defaultFormat)
{
    switch (defaultFormat)
    {
        // 32-bit Z w/ Stencil
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

        // 24-bit Z
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT util_to_dx12_swapchain_format(TinyImageFormat const format)
{
    DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;

    // FLIP_DISCARD and FLIP_SEQEUNTIAL swapchain buffers only support these formats
    switch (format)
    {
    case TinyImageFormat_R16G16B16A16_SFLOAT:
        result = DXGI_FORMAT_R16G16B16A16_FLOAT;
        break;
    case TinyImageFormat_B8G8R8A8_UNORM:
    case TinyImageFormat_B8G8R8A8_SRGB:
        result = DXGI_FORMAT_B8G8R8A8_UNORM;
        break;
    case TinyImageFormat_R8G8B8A8_UNORM:
    case TinyImageFormat_R8G8B8A8_SRGB:
        result = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    case TinyImageFormat_R10G10B10A2_UNORM:
        result = DXGI_FORMAT_R10G10B10A2_UNORM;
        break;
    default:
        break;
    }

    if (result == DXGI_FORMAT_UNKNOWN)
    {
        LOGF(eERROR, "Image Format (%u) not supported for creating swapchain buffer", (uint32_t)format);
    }

    return result;
}

DXGI_COLOR_SPACE_TYPE util_to_dx12_colorspace(ColorSpace colorspace)
{
    switch (colorspace)
    {
    case COLOR_SPACE_SDR_LINEAR:
    case COLOR_SPACE_SDR_SRGB:
        return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    case COLOR_SPACE_P2020:
        return DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
    case COLOR_SPACE_EXTENDED_SRGB:
        return DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
    default:
        break;
    }

    LOGF(eERROR, "Color Space (%u) not supported for creating swapchain buffer", (uint32_t)colorspace);

    return DXGI_COLOR_SPACE_CUSTOM;
}

D3D12_SHADER_VISIBILITY util_to_dx12_shader_visibility(ShaderStage stages)
{
    D3D12_SHADER_VISIBILITY res = D3D12_SHADER_VISIBILITY_ALL;
    uint32_t                stageCount = 0;

    if (stages == SHADER_STAGE_COMP)
    {
        return D3D12_SHADER_VISIBILITY_ALL;
    }
#if defined(ENABLE_WORKGRAPH)
    else if (stages == SHADER_STAGE_WORKGRAPH)
    {
        return D3D12_SHADER_VISIBILITY_ALL;
    }
#endif

    if (stages & SHADER_STAGE_VERT)
    {
        res = D3D12_SHADER_VISIBILITY_VERTEX;
        ++stageCount;
    }
    if (stages & SHADER_STAGE_GEOM)
    {
        res = D3D12_SHADER_VISIBILITY_GEOMETRY;
        ++stageCount;
    }
    if (stages & SHADER_STAGE_HULL)
    {
        res = D3D12_SHADER_VISIBILITY_HULL;
        ++stageCount;
    }
    if (stages & SHADER_STAGE_DOMN)
    {
        res = D3D12_SHADER_VISIBILITY_DOMAIN;
        ++stageCount;
    }
    if (stages & SHADER_STAGE_FRAG)
    {
        res = D3D12_SHADER_VISIBILITY_PIXEL;
        ++stageCount;
    }
    ASSERT(stageCount > 0);
    return stageCount > 1 ? D3D12_SHADER_VISIBILITY_ALL : res;
}

D3D12_DESCRIPTOR_RANGE_TYPE util_to_dx12_descriptor_range(DescriptorType type)
{
    switch (type)
    {
    case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case DESCRIPTOR_TYPE_ROOT_CONSTANT:
        return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    case DESCRIPTOR_TYPE_RW_BUFFER:
    case DESCRIPTOR_TYPE_RW_TEXTURE:
        return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case DESCRIPTOR_TYPE_SAMPLER:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
#ifdef D3D12_RAYTRACING_AVAILABLE
    case DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE:
#endif
    case DESCRIPTOR_TYPE_TEXTURE:
    case DESCRIPTOR_TYPE_BUFFER:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

    default:
        ASSERT(false && "Invalid DescriptorInfo Type");
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    }
}

D3D12_RESOURCE_STATES util_to_dx12_resource_state(ResourceState state)
{
    D3D12_RESOURCE_STATES ret = D3D12_RESOURCE_STATE_COMMON;

    // These states cannot be combined with other states so we just do an == check
    if (state == RESOURCE_STATE_GENERIC_READ)
    {
        // On Xbox, `GENERIC_READ` does not include `INDIRECT_ARGUMENT`
#ifdef XBOX
        return D3D12_RESOURCE_STATE_GENERIC_READ & ~D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
#else
        return D3D12_RESOURCE_STATE_GENERIC_READ;
#endif
    }
    if (state == RESOURCE_STATE_COMMON)
        return D3D12_RESOURCE_STATE_COMMON;
    if (state == RESOURCE_STATE_PRESENT)
        return D3D12_RESOURCE_STATE_PRESENT;

    if (state & RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
        ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    if (state & RESOURCE_STATE_INDEX_BUFFER)
        ret |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
    if (state & RESOURCE_STATE_RENDER_TARGET)
        ret |= D3D12_RESOURCE_STATE_RENDER_TARGET;
    if (state & RESOURCE_STATE_UNORDERED_ACCESS)
        ret |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    if (state & RESOURCE_STATE_DEPTH_WRITE)
        ret |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
    else if (state & RESOURCE_STATE_DEPTH_READ)
        ret |= D3D12_RESOURCE_STATE_DEPTH_READ;
    if (state & RESOURCE_STATE_STREAM_OUT)
        ret |= D3D12_RESOURCE_STATE_STREAM_OUT;
    if (state & RESOURCE_STATE_INDIRECT_ARGUMENT)
        ret |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    if (state & RESOURCE_STATE_COPY_DEST)
        ret |= D3D12_RESOURCE_STATE_COPY_DEST;
    if (state & RESOURCE_STATE_COPY_SOURCE)
        ret |= D3D12_RESOURCE_STATE_COPY_SOURCE;
    if (state & RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
        ret |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    if (state & RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        ret |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
#ifdef D3D12_RAYTRACING_AVAILABLE
    if (state & (RESOURCE_STATE_ACCELERATION_STRUCTURE_READ | RESOURCE_STATE_ACCELERATION_STRUCTURE_WRITE))
        ret |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
#endif

    return ret;
}

static inline FORGE_CONSTEXPR D3D12_QUERY_HEAP_TYPE ToDX12QueryHeapType(QueryType type)
{
    switch (type)
    {
    case QUERY_TYPE_TIMESTAMP:
        return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    case QUERY_TYPE_PIPELINE_STATISTICS:
        return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
    case QUERY_TYPE_OCCLUSION:
        return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
    default:
        return (D3D12_QUERY_HEAP_TYPE)-1;
    }
}

static inline FORGE_CONSTEXPR D3D12_QUERY_TYPE ToDX12QueryType(QueryType type)
{
    switch (type)
    {
    case QUERY_TYPE_TIMESTAMP:
        return D3D12_QUERY_TYPE_TIMESTAMP;
    case QUERY_TYPE_PIPELINE_STATISTICS:
        return D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
    case QUERY_TYPE_OCCLUSION:
        return D3D12_QUERY_TYPE_OCCLUSION;
    default:
        return (D3D12_QUERY_TYPE)-1;
    }
}

static inline FORGE_CONSTEXPR uint32_t ToQueryWidth(QueryType type)
{
    switch (type)
    {
    case QUERY_TYPE_PIPELINE_STATISTICS:
        return sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);
    default:
        return sizeof(uint64_t);
    }
}

#if !defined(XBOX)
void util_enumerate_gpus(IDXGIFactory6* dxgiFactory, uint32_t* pGpuCount, DXGPUInfo* gpuDesc, bool* pFoundSoftwareAdapter)
{
    D3D_FEATURE_LEVEL feature_levels[4] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    uint32_t       gpuCount = 0;
    IDXGIAdapter4* adapter = NULL;
    bool           foundSoftwareAdapter = false;

    // Find number of usable GPUs
    // Use DXGI6 interface which lets us specify gpu preference so we dont need to use NVOptimus or AMDPowerExpress exports
    for (UINT i = 0; DXGI_ERROR_NOT_FOUND != COM_CALL(EnumAdapterByGpuPreference, dxgiFactory, i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                      IID_REF(IDXGIAdapter4_Copy), &adapter);
         ++i)
    {
        if (gpuCount >= MAX_MULTIPLE_GPUS)
        {
            break;
        }

        DXGI_ADAPTER_DESC3 desc = { 0 };
        COM_CALL(GetDesc3, adapter, &desc);

        // Ignore Microsoft Driver
        if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
        {
            for (uint32_t level = 0; level < sizeof(feature_levels) / sizeof(feature_levels[0]); ++level)
            {
                // Make sure the adapter can support a D3D12 device
                if (SUCCEEDED(d3d12dll_CreateDevice(adapter, feature_levels[level], IID_REF(ID3D12Device), NULL)))
                {
                    DXGPUInfo  gpuDescTmp = { 0 };
                    DXGPUInfo* pDXGPUInfo = gpuDesc ? &gpuDesc[gpuCount] : &gpuDescTmp;
                    HRESULT    hres = COM_CALL(QueryInterface, adapter, IID_REF(IDXGIAdapter4_Copy), &pDXGPUInfo->pGpu);
                    if (SUCCEEDED(hres))
                    {
                        if (gpuDesc)
                        {
                            ID3D12Device* device = NULL;
                            d3d12dll_CreateDevice(adapter, feature_levels[level], IID_ARGS(ID3D12Device, &device));
                            hook_fill_gpu_desc(device, feature_levels[level], pDXGPUInfo);
                            // get preset for current gpu description
                            pDXGPUInfo->mPreset = getGPUPresetLevel(pDXGPUInfo->mVendorId, pDXGPUInfo->mDeviceId,
                                                                    getGPUVendorName(pDXGPUInfo->mVendorId), pDXGPUInfo->mName);
                            SAFE_RELEASE(device);
                        }
                        else
                        {
                            SAFE_RELEASE(pDXGPUInfo->pGpu);
                        }
                        ++gpuCount;
                        break;
                    }
                }
            }
        }
        else
        {
            foundSoftwareAdapter = true;
        }

        COM_CALL(Release, adapter);
    }

    if (pGpuCount)
        *pGpuCount = gpuCount;

    if (pFoundSoftwareAdapter)
        *pFoundSoftwareAdapter = foundSoftwareAdapter;
}
#endif

static void QueryRaytracingSupport(ID3D12Device* pDevice, GpuDesc* pGpuDesc)
{
    UNREF_PARAM(pDevice);
    UNREF_PARAM(pGpuDesc);
#ifdef D3D12_RAYTRACING_AVAILABLE
    ASSERT(pDevice);
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 opts5 = { 0 };
    HRESULT                           hres = COM_CALL(CheckFeatureSupport, pDevice, D3D12_FEATURE_D3D12_OPTIONS5, &opts5, sizeof(opts5));
    if (SUCCEEDED(hres))
    {
        pGpuDesc->mRayPipelineSupported = (opts5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);
#if defined(SCARLETT)
        pGpuDesc->mRayQuerySupported = true;
#else
        pGpuDesc->mRayQuerySupported = (opts5.RaytracingTier > D3D12_RAYTRACING_TIER_1_0);
#endif
        pGpuDesc->mRaytracingSupported = pGpuDesc->mRayPipelineSupported || pGpuDesc->mRayQuerySupported;
    }
#endif
}

static void QueryWorkGraphSupport(ID3D12Device* pDevice, GpuDesc* pGpuDesc)
{
    UNREF_PARAM(pDevice);
    UNREF_PARAM(pGpuDesc);
#if defined(ENABLE_WORKGRAPH)
    ASSERT(pDevice);
    D3D12_FEATURE_DATA_D3D12_OPTIONS21 opts21 = { 0 };
    HRESULT hres = COM_CALL(CheckFeatureSupport, pDevice, D3D12_FEATURE_D3D12_OPTIONS21, &opts21, sizeof(opts21));
    if (SUCCEEDED(hres))
    {
        pGpuDesc->mWorkgraphSupported = (opts21.WorkGraphsTier != D3D12_WORK_GRAPHS_TIER_NOT_SUPPORTED);
    }
#endif
}

static void Query64BitAtomicsSupport(ID3D12Device* pDevice, GpuDesc* pGpuDesc)
{
    ASSERT(pDevice);
    D3D12_FEATURE_DATA_D3D12_OPTIONS9 opts9 = { 0 };
    HRESULT                           hres = COM_CALL(CheckFeatureSupport, pDevice, D3D12_FEATURE_D3D12_OPTIONS9, &opts9, sizeof(opts9));
    if (SUCCEEDED(hres))
    {
        pGpuDesc->m64BitAtomicsSupported = opts9.AtomicInt64OnTypedResourceSupported;
    }
}

void QueryGpuDesc(ID3D12Device* pDevice, const DXGPUInfo* pDXGPUInfo, GpuDesc* pGpuDesc)
{
    setDefaultGPUProperties(pGpuDesc);
    pGpuDesc->mUniformBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    pGpuDesc->mUploadBufferAlignment = 1;
    pGpuDesc->mUploadBufferTextureAlignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
    pGpuDesc->mUploadBufferTextureRowAlignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
    pGpuDesc->mMultiDrawIndirect = true;
    pGpuDesc->mMultiDrawIndirectCount = true;
    pGpuDesc->mPrimitiveIdPsSupported = true;
    pGpuDesc->mMaxVertexInputBindings = 32U;

    // assign device ID
    pGpuDesc->mGpuVendorPreset.mModelId = pDXGPUInfo->mDeviceId;
    // assign vendor ID
    pGpuDesc->mGpuVendorPreset.mVendorId = pDXGPUInfo->mVendorId;
    // assign Revision ID
    pGpuDesc->mGpuVendorPreset.mRevisionId = pDXGPUInfo->mRevisionId;
    // get name from api
    strncpy(pGpuDesc->mGpuVendorPreset.mGpuName, pDXGPUInfo->mName, MAX_GPU_VENDOR_STRING_LENGTH);
    // get preset
    pGpuDesc->mGpuVendorPreset.mPresetLevel = pDXGPUInfo->mPreset;
    // get VRAM
    pGpuDesc->mVRAM = pDXGPUInfo->mDedicatedVideoMemory;
    // get wave lane count
    pGpuDesc->mWaveLaneCount = pDXGPUInfo->mFeatureDataOptions1.WaveLaneCountMin;
    pGpuDesc->mROVsSupported = pDXGPUInfo->mFeatureDataOptions.ROVsSupported ? true : false;
#if defined(AMDAGS)
    pGpuDesc->mAmdAsicFamily = agsGetAsicFamily(pDXGPUInfo->mDeviceId);
#endif
    pGpuDesc->mTessellationSupported = pGpuDesc->mGeometryShaderSupported = true;

#if defined(XBOXONE)
    pGpuDesc->mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_BASIC_BIT | WAVE_OPS_SUPPORT_FLAG_VOTE_BIT | WAVE_OPS_SUPPORT_FLAG_BALLOT_BIT |
                                     WAVE_OPS_SUPPORT_FLAG_SHUFFLE_BIT;
    pGpuDesc->mWaveOpsSupportedStageFlags |= SHADER_STAGE_ALL_GRAPHICS | SHADER_STAGE_COMP;
#else
    pGpuDesc->mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_ALL;
    pGpuDesc->mWaveOpsSupportedStageFlags = SHADER_STAGE_ALL_GRAPHICS | SHADER_STAGE_COMP;
#endif

#if defined(XBOX)
    pGpuDesc->mUnifiedMemorySupport = UMA_SUPPORT_READ_WRITE;
#endif
    pGpuDesc->mGpuMarkers = true;
    pGpuDesc->mHDRSupported = true;
    pGpuDesc->mRootConstant = true;
    pGpuDesc->mIndirectRootConstant = true;
    pGpuDesc->mBuiltinDrawID = false;
    pGpuDesc->mTimestampQueries = true;
    pGpuDesc->mOcclusionQueries = true;
    pGpuDesc->mPipelineStatsQueries = true;
    pGpuDesc->mSoftwareVRSSupported = true;
    pGpuDesc->mAllowBufferTextureInSameHeap = pDXGPUInfo->mFeatureDataOptions.ResourceHeapTier >= D3D12_RESOURCE_HEAP_TIER_2;
    // compute shader group count
    pGpuDesc->mMaxTotalComputeThreads = D3D12_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;
    pGpuDesc->mMaxComputeThreads[0] = D3D12_CS_THREAD_GROUP_MAX_X;
    pGpuDesc->mMaxComputeThreads[1] = D3D12_CS_THREAD_GROUP_MAX_Y;
    pGpuDesc->mMaxComputeThreads[2] = D3D12_CS_THREAD_GROUP_MAX_Z;

    // Determine root signature size for this gpu driver
    DXGI_ADAPTER_DESC adapterDesc;
    COM_CALL(GetDesc, pDXGPUInfo->pGpu, &adapterDesc);

    // set default driver version as empty string
    pGpuDesc->mGpuVendorPreset.mGpuDriverVersion[0] = '\0';
    if (gpuVendorEquals(adapterDesc.VendorId, "nvidia"))
    {
#if defined(NVAPI)
        if (NVAPI_OK == gNvStatus)
        {
            snprintf(pGpuDesc->mGpuVendorPreset.mGpuDriverVersion, MAX_GPU_VENDOR_STRING_LENGTH, "%lu.%lu", gNvGpuInfo.driverVersion / 100,
                     gNvGpuInfo.driverVersion % 100);
        }
#endif
    }
    else if (gpuVendorEquals(adapterDesc.VendorId, "amd"))
    {
#if defined(AMDAGS)
        if (AGS_SUCCESS == gAgsStatus)
        {
            snprintf(pGpuDesc->mGpuVendorPreset.mGpuDriverVersion, MAX_GPU_VENDOR_STRING_LENGTH, "%s", gAgsGpuInfo.driverVersion);
        }
#endif
    }
    // fallback to windows version (device manager number), works for intel not for nvidia
    else
    {
        LARGE_INTEGER umdVersion;
        HRESULT       hr = COM_CALL(CheckInterfaceSupport, pDXGPUInfo->pGpu, IID_REF(IDXGIDevice), &umdVersion);
        if (SUCCEEDED(hr))
        {
            // 31.0.101.5074 -> 101.5074, only use build number like it's done for vulkan
            // WORD product = HIWORD(umdVersion.HighPart);
            // WORD version = LOWORD(umdVersion.HighPart);
            WORD subVersion = HIWORD(umdVersion.LowPart);
            WORD build = LOWORD(umdVersion.LowPart);
            snprintf(pGpuDesc->mGpuVendorPreset.mGpuDriverVersion, MAX_GPU_VENDOR_STRING_LENGTH, "%d.%d", subVersion, build);
        }
    }
    pGpuDesc->mMaxRootSignatureDWORDS = 13;
    pGpuDesc->mMaxBoundTextures = UINT32_MAX;

    QueryRaytracingSupport(pDevice, pGpuDesc);
    QueryWorkGraphSupport(pDevice, pGpuDesc);
    Query64BitAtomicsSupport(pDevice, pGpuDesc);
}

static void InitializeBufferDesc(Renderer* pRenderer, const BufferDesc* pDesc, D3D12_RESOURCE_DESC* desc)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(desc);

    uint64_t allocationSize = pDesc->mSize;
    // Align the buffer size to multiples of 256
    if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER))
    {
        allocationSize = round_up_64(allocationSize, pRenderer->pGpu->mUniformBufferAlignment);
    }

    desc->Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    // Alignment must be 64KB (D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) or 0, which is effectively 64KB.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dn903813(v=vs.85).aspx
    desc->Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    desc->Width = allocationSize;
    desc->Height = 1;
    desc->DepthOrArraySize = 1;
    desc->MipLevels = 1;
    desc->Format = DXGI_FORMAT_UNKNOWN;
    desc->SampleDesc.Count = 1;
    desc->SampleDesc.Quality = 0;
    desc->Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc->Flags = D3D12_RESOURCE_FLAG_NONE;

    hook_modify_buffer_resource_desc(pDesc, desc);

    if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER)
    {
        desc->Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    // Adjust for padding
    UINT64 padded_size = 0;
    COM_CALL(GetCopyableFootprints, pRenderer->mDx.pDevice, desc, 0, 1, 0, NULL, NULL, NULL, &padded_size);
    allocationSize = (uint64_t)padded_size;
    desc->Width = allocationSize;

    if (RESOURCE_MEMORY_USAGE_GPU_TO_CPU == pDesc->mMemoryUsage)
    {
        desc->Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }
}

static void InitializeTextureDesc(Renderer* pRenderer, const TextureDesc* pDesc, D3D12_RESOURCE_DESC* desc,
                                  ResourceState* pStartResourceState)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(desc);

    D3D12_RESOURCE_DIMENSION res_dim = D3D12_RESOURCE_DIMENSION_UNKNOWN;
    if (pDesc->mFlags & TEXTURE_CREATION_FLAG_FORCE_2D)
    {
        ASSERT(pDesc->mDepth == 1);
        res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    }
    else if (pDesc->mFlags & TEXTURE_CREATION_FLAG_FORCE_3D)
    {
        res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    }
    else
    {
        if (pDesc->mDepth > 1)
            res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        else if (pDesc->mHeight > 1)
            res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        else
            res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    }

    DXGI_FORMAT dxFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mFormat);

    desc->Dimension = res_dim;
    // On PC, If Alignment is set to 0, the runtime will use 4MB for MSAA textures and 64KB for everything else.
    // On XBox, We have to explicitlly assign D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT if MSAA is used
    desc->Alignment = (UINT)pDesc->mSampleCount > 1 ? D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT : 0;
    desc->Width = pDesc->mWidth;
    desc->Height = pDesc->mHeight;
    desc->DepthOrArraySize = (UINT16)(pDesc->mArraySize != 1 ? pDesc->mArraySize : pDesc->mDepth);
    desc->MipLevels = (UINT16)pDesc->mMipLevels;
    desc->Format = (DXGI_FORMAT)TinyImageFormat_DXGI_FORMATToTypeless((TinyImageFormat_DXGI_FORMAT)dxFormat);
    if (desc->Format == DXGI_FORMAT_UNKNOWN)
    {
        desc->Format = dxFormat;
    }
    desc->SampleDesc.Count = (UINT)pDesc->mSampleCount;
    desc->SampleDesc.Quality = (UINT)pDesc->mSampleQuality;
    desc->Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc->Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS data = {
        .Format = desc->Format,
        .Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,
        .SampleCount = desc->SampleDesc.Count,
    };
    COM_CALL(CheckFeatureSupport, pRenderer->mDx.pDevice, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(data));
    while (data.NumQualityLevels == 0 && data.SampleCount > 0)
    {
        LOGF(eWARNING, "Sample Count (%u) not supported. Trying a lower sample count (%u)", data.SampleCount, data.SampleCount / 2);
        data.SampleCount = desc->SampleDesc.Count / 2;
        COM_CALL(CheckFeatureSupport, pRenderer->mDx.pDevice, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(data));
    }
    desc->SampleDesc.Count = data.SampleCount;

    ResourceState actualStartState = pDesc->mStartState;

    // Decide UAV flags
    if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
    {
        desc->Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    // Decide render target flags
    if (pDesc->mStartState & RESOURCE_STATE_RENDER_TARGET)
    {
        desc->Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        actualStartState = (pDesc->mStartState > RESOURCE_STATE_RENDER_TARGET)
                               ? (pDesc->mStartState & (ResourceState)~RESOURCE_STATE_RENDER_TARGET)
                               : RESOURCE_STATE_RENDER_TARGET;
    }
    else if (pDesc->mStartState & RESOURCE_STATE_DEPTH_WRITE)
    {
        desc->Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        actualStartState = (pDesc->mStartState > RESOURCE_STATE_DEPTH_WRITE)
                               ? (pDesc->mStartState & (ResourceState)~RESOURCE_STATE_DEPTH_WRITE)
                               : RESOURCE_STATE_DEPTH_WRITE;
    }

    // Decide sharing flags
    if (pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT)
    {
        desc->Flags |= D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
        desc->Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    }

#if defined(XBOX)
    if (pDesc->mFlags & TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET)
    {
        desc->Format = dxFormat;
    }
#endif

    if (pDesc->mFlags & TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET)
    {
        actualStartState = RESOURCE_STATE_PRESENT;
    }

    if (pStartResourceState)
        *pStartResourceState = actualStartState;
}

#if defined(_WINDOWS) && defined(ENABLE_GRAPHICS_VALIDATION)
void DebugMessageCallback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR pDescription,
                          void* pContext)
{
    UNREF_PARAM(pContext);
    D3D12_MESSAGE_ID ignoreMessageIDs[] = {
        // Trying to load a PSO that isn't in loaded library is expected. New PSOs will always trigger this warning.
        D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND,
        // Required when we want to Alias the same memory from multiple buffers and use them at the same time
        // (we know from the App that we don't read/write on the same memory at the same time)
        D3D12_MESSAGE_ID_HEAP_ADDRESS_RANGE_INTERSECTS_MULTIPLE_BUFFERS,
        // Just a baggage
        D3D12_MESSAGE_ID_CREATE_COMMANDLIST12,
        D3D12_MESSAGE_ID_DESTROY_COMMANDLIST12,
        D3D12_MESSAGE_ID_INVALID_SUBRESOURCE_STATE,
        // Bug in validation when using acceleration structure in compute or graphics pipeline
        // D3D12 ERROR: ID3D12CommandList::Dispatch: Static Descriptor SRV resource dimensions (UNKNOWN (11)) differs from that expected by
        // shader (D3D12_SRV_DIMENSION_BUFFER) UNKNOWN (11) is D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE
        D3D12_MESSAGE_ID_COMMAND_LIST_STATIC_DESCRIPTOR_RESOURCE_DIMENSION_MISMATCH,
        D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
        D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED,
    };

    for (uint32_t m = 0; m < TF_ARRAY_COUNT(ignoreMessageIDs); ++m)
        if (ignoreMessageIDs[m] == id)
            return;

    if (severity == D3D12_MESSAGE_SEVERITY_CORRUPTION)
    {
        LOGF(eERROR, "[Corruption] [%d] : %s (%d)", category, pDescription, id);
    }
    else if (severity == D3D12_MESSAGE_SEVERITY_ERROR)
    {
        LOGF(eERROR, "[Error] [%d] : %s (%d)", category, pDescription, id);
    }
    else if (severity == D3D12_MESSAGE_SEVERITY_WARNING)
    {
        LOGF(eWARNING, "[%d] : %s (%d)", category, pDescription, id);
    }
    else if (severity == D3D12_MESSAGE_SEVERITY_INFO)
    {
        LOGF(eINFO, "[%d] : %s (%d)", category, pDescription, id);
    }
}
#endif

/************************************************************************/
// Internal init functions
/************************************************************************/
#if !defined(XBOX)

// Note that Windows 10 Creator Update SDK is required for enabling Shader Model 6 feature.
static HRESULT EnableExperimentalShaderModels()
{
    static const GUID D3D12ExperimentalShaderModelsID = { /* 76f5573e-f13a-40f5-b297-81ce9e18933f */
                                                          0x76f5573e,
                                                          0xf13a,
                                                          0x40f5,
                                                          { 0xb2, 0x97, 0x81, 0xce, 0x9e, 0x18, 0x93, 0x3f }
    };

    return d3d12dll_EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModelsID, NULL, NULL);
}
#endif

#if defined(XBOX)
UINT HANGBEGINCALLBACK(UINT64 Flags)
{
    LOGF(eINFO, "( %d )", Flags);
    return (UINT)Flags;
}

void HANGPRINTCALLBACK(const CHAR* strLine)
{
    LOGF(eINFO, "( %s )", strLine);
    return;
}

void HANGDUMPCALLBACK(const WCHAR* strFileName)
{
    UNREF_PARAM(strFileName);
    return;
}
#endif

static bool SelectBestGpu(Renderer* pRenderer, const RendererDesc* pDesc, D3D_FEATURE_LEVEL* pFeatureLevel, uint32_t* pOutGpuCount)
{
    UNREF_PARAM(pDesc);
    GpuDesc          gpuDesc[MAX_MULTIPLE_GPUS] = { 0 };
    RendererContext* pContext = pRenderer->pContext;
    for (uint32_t i = 0; i < pContext->mGpuCount; ++i)
    {
        gpuDesc[i] = pContext->mGpus[i];
    }
    uint32_t gpuIndex = util_select_best_gpu(gpuDesc, pContext->mGpuCount);
    ASSERT(gpuIndex < pContext->mGpuCount);

    bool gpuSupported = util_check_is_gpu_supported(&gpuDesc[gpuIndex]);
    if (!gpuSupported)
    {
        LOGF(eERROR, "Failed to Init Renderer: %s", getUnsupportedGPUMsg());
        return false;
    }

    // Get the latest and greatest feature level gpu
    pRenderer->pGpu = &pRenderer->pContext->mGpus[gpuIndex];
    ASSERT(pRenderer->pGpu != NULL);

    //  driver rejection rules from gpu.cfg
    bool driverValid = checkDriverRejectionSettings(&gpuDesc[gpuIndex]);
    if (!driverValid)
    {
        return false;
    }

    // Print selected GPU information
    LOGF(eINFO, "GPU[%u] is selected as default GPU", gpuIndex);
    LOGF(eINFO, "Name of selected gpu: %s", pRenderer->pGpu->mGpuVendorPreset.mGpuName);
    LOGF(eINFO, "Vendor id of selected gpu: %#x", pRenderer->pGpu->mGpuVendorPreset.mVendorId);
    LOGF(eINFO, "Model id of selected gpu: %#x", pRenderer->pGpu->mGpuVendorPreset.mModelId);
    LOGF(eINFO, "Revision id of selected gpu: %#x", pRenderer->pGpu->mGpuVendorPreset.mRevisionId);
    LOGF(eINFO, "Preset of selected gpu: %s", presetLevelToString(pRenderer->pGpu->mGpuVendorPreset.mPresetLevel));

    if (pFeatureLevel)
    {
        *pFeatureLevel = pContext->mGpus[gpuIndex].mFeatureLevel;
    }

    *pOutGpuCount = pContext->mGpuCount;

    return true;
}

static bool AddDevice(const RendererDesc* pDesc, Renderer* pRenderer)
{
    D3D_FEATURE_LEVEL supportedFeatureLevel = (D3D_FEATURE_LEVEL)0;
    uint32_t          gpuCount = 0;
    if (pDesc->pContext)
    {
        ASSERT(pDesc->mGpuIndex < pDesc->pContext->mGpuCount);

        pRenderer->pGpu = &pDesc->pContext->mGpus[pDesc->mGpuIndex];
        supportedFeatureLevel = pRenderer->pGpu->mFeatureLevel;
    }
    else
    {
        if (!SelectBestGpu(pRenderer, pDesc, &supportedFeatureLevel, &gpuCount))
        {
            return false;
        }
    }

    // Load functions
    {
        HMODULE module = hook_get_d3d12_module_handle();

        fnD3D12CreateRootSignatureDeserializer =
            (PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress(module, "D3D12SerializeVersionedRootSignature");

        fnD3D12SerializeVersionedRootSignature =
            (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress(module, "D3D12SerializeVersionedRootSignature");

        fnD3D12CreateVersionedRootSignatureDeserializer =
            (PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress(module, "D3D12CreateVersionedRootSignatureDeserializer");
    }

#if defined(XBOX)
    pRenderer->mDx.pDevice = pRenderer->pGpu->mDx.pDevice;
#else
    CHECK_HRESULT(d3d12dll_CreateDevice(pRenderer->pGpu->mDx.pGpu, supportedFeatureLevel, IID_ARGS(ID3D12Device, &pRenderer->mDx.pDevice)));
#endif

#if defined(_WINDOWS) && defined(ENABLE_GRAPHICS_VALIDATION)
    HRESULT hr = COM_CALL(QueryInterface, pRenderer->mDx.pDevice, IID_REF(ID3D12InfoQueue1_Copy), &pRenderer->mDx.pDebugValidation);
    pRenderer->mDx.mUseDebugCallback = true;
    if (!SUCCEEDED(hr))
    {
        SAFE_RELEASE(pRenderer->mDx.pDebugValidation);
        pRenderer->mDx.mUseDebugCallback = false;
        hr = COM_CALL(QueryInterface, pRenderer->mDx.pDevice, IID_REF(ID3D12InfoQueue), (void**)&pRenderer->mDx.pDebugValidation);
    }
    if (SUCCEEDED(hr))
    {
        COM_CALL(SetBreakOnSeverity, pRenderer->mDx.pDebugValidation, D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        COM_CALL(SetBreakOnSeverity, pRenderer->mDx.pDebugValidation, D3D12_MESSAGE_SEVERITY_ERROR, true);
        COM_CALL(SetBreakOnSeverity, pRenderer->mDx.pDebugValidation, D3D12_MESSAGE_SEVERITY_WARNING, false);
        COM_CALL(SetBreakOnSeverity, pRenderer->mDx.pDebugValidation, D3D12_MESSAGE_SEVERITY_INFO, false);
        COM_CALL(SetBreakOnSeverity, pRenderer->mDx.pDebugValidation, D3D12_MESSAGE_SEVERITY_MESSAGE, false);

        uint32_t         hideMessageCount = 0;
        D3D12_MESSAGE_ID hideMessages[32] = { 0 };

        // Trying to load a PSO that isn't in loaded library is expected. New PSOs will always trigger this warning.
        hideMessages[hideMessageCount++] = D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND;

        // On Windows 11 there's a bug in the DXGI debug layer that triggers a false-positive on hybrid GPU
        // laptops during Present. The problem appears to be a race condition, so it may or may not happen.
        // Suppressing D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE avoids this problem.
        // If we have >2 GPU's (eg. Laptop with integrated and dedicated GPU).
        if (gpuCount >= 2)
        {
            pRenderer->mDx.mSuppressMismatchingCommandListDuringPresent = true;
        }

        if (hideMessageCount) //-V547
        {
            D3D12_INFO_QUEUE_FILTER filter = { 0 };
            filter.DenyList.NumIDs = hideMessageCount;
            filter.DenyList.pIDList = hideMessages;
            COM_CALL(AddStorageFilterEntries, pRenderer->mDx.pDebugValidation, &filter);
        }

        D3D12_MESSAGE_ID hide[] = {
            // Required when we want to Alias the same memory from multiple buffers and use them at the same time
            // (we know from the App that we don't read/write on the same memory at the same time)
            D3D12_MESSAGE_ID_HEAP_ADDRESS_RANGE_INTERSECTS_MULTIPLE_BUFFERS,
            D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED,
        };
        D3D12_INFO_QUEUE_FILTER filter = { 0 };
        filter.DenyList.NumIDs = _countof(hide);
        filter.DenyList.pIDList = hide;
        COM_CALL(AddStorageFilterEntries, pRenderer->mDx.pDebugValidation, &filter);

        if (pRenderer->mDx.mUseDebugCallback)
        {
            COM_CALL(SetMuteDebugOutput, pRenderer->mDx.pDebugValidation, true);
            // D3D12_MESSAGE_CALLBACK_IGNORE_FILTERS, will enable all message filtering in the callback function, no need to use Push/Pop,
            // but we stick with FLAG_NONE for failsafe
            HRESULT res = COM_CALL(RegisterMessageCallback, pRenderer->mDx.pDebugValidation, DebugMessageCallback,
                                   D3D12_MESSAGE_CALLBACK_FLAG_NONE, pRenderer, &pRenderer->mDx.mCallbackCookie);
            if (!SUCCEEDED(res))
            {
                LOGF(eERROR, "AddDevice | RegisterMessageCallback failed - disabling DirectX12 ID3D12InfoQueue1 debug callbacks");
            }
        }
    }
#endif

#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    SetObjectName((ID3D12Object*)pRenderer->mDx.pDevice, "Main Device"); // -V1027
#endif                                                                   // ENABLE_GRAPHICS_DEBUG_ANNOTATION

    return true;
}

static void RemoveDevice(Renderer* pRenderer)
{
#if defined(_WINDOWS) && defined(ENABLE_GRAPHICS_VALIDATION)
    if (pRenderer->mDx.pDebugValidation && pRenderer->pGpu->mSuppressInvalidSubresourceStateAfterExit)
    {
        // bypass AMD driver issue with vk and dxgi swapchains resource states
        D3D12_MESSAGE_ID        hide[] = { D3D12_MESSAGE_ID_INVALID_SUBRESOURCE_STATE };
        D3D12_INFO_QUEUE_FILTER filter = { 0 };
        filter.DenyList.NumIDs = 1;
        filter.DenyList.pIDList = hide;
        COM_CALL(PushStorageFilter, pRenderer->mDx.pDebugValidation, &filter);
    }
    if (pRenderer->mDx.mUseDebugCallback)
        COM_CALL(UnregisterMessageCallback, pRenderer->mDx.pDebugValidation, pRenderer->mDx.mCallbackCookie);
    SAFE_RELEASE(pRenderer->mDx.pDebugValidation);
#endif

    SAFE_RELEASE(pRenderer->mDx.pDevice);
}

void InitCommon(const RendererContextDesc* pDesc, RendererContext* pContext)
{
    UNREF_PARAM(pDesc);
    UNREF_PARAM(pContext);
    d3d12dll_init();

    AGSReturnCode agsRet = agsInit();
    if (AGS_SUCCESS == agsRet)
    {
        agsPrintDriverInfo();
    }

    NvAPI_Status nvStatus = nvapiInit();
    if (NVAPI_OK == nvStatus)
    {
        nvapiPrintDriverInfo();
    }

#if defined(ENABLE_GRAPHICS_VALIDATION) && defined(_WINDOWS)
    // add debug layer if in debug mode
    if (SUCCEEDED(d3d12dll_GetDebugInterface(IID_ARGS(ID3D12Debug, &pContext->mDx.pDebug))))
    {
        hook_enable_debug_layer(pDesc, pContext);
    }
#endif

#if !defined(XBOX)
    UINT flags = 0;
#if defined(ENABLE_GRAPHICS_VALIDATION)
    flags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    CHECK_HRESULT(d3d12dll_CreateDXGIFactory2(flags, IID_REF(IDXGIFactory6_Copy), &pContext->mDx.pDXGIFactory));
#endif

#if defined(USE_DRED)
    if (SUCCEEDED(d3d12dll_GetDebugInterface(IID_ARGS(&pContext->mDx.pDredSettings))))
    {
        // Turn on AutoBreadcrumbs and Page Fault reporting
        pContext->mDx.pDredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        pContext->mDx.pDredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }
#endif
}

void ExitCommon(RendererContext* pContext)
{
    SAFE_RELEASE(pContext->mDx.pDXGIFactory);
#if defined(ENABLE_GRAPHICS_VALIDATION) && defined(_WINDOWS)
    SAFE_RELEASE(pContext->mDx.pDebug);
#endif
#if defined(USE_DRED)
    SAFE_RELEASE(pContext->pDredSettings);
#endif

#if defined(ENABLE_GRAPHICS_VALIDATION) && !defined(XBOX)
    IDXGIDebug1* dxgiDebug = NULL;
    if (SUCCEEDED(d3d12dll_DXGIGetDebugInterface1(0, IID_ARGS(IDXGIDebug1, &dxgiDebug))))
    {
        LOGF(eWARNING, "Printing live D3D12 objects to the debugger output window...");

        COM_CALL(ReportLiveObjects, dxgiDebug, DXGI_DEBUG_ALL,
                 (DXGI_DEBUG_RLO_FLAGS)(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        SAFE_RELEASE(dxgiDebug);
    }
    else
    {
        LOGF(eWARNING, "Unable to retrieve the D3D12 DXGI debug interface, cannot print live D3D12 objects.");
    }
#endif

    nvapiExit();
    agsExit();
    d3d12dll_exit();
}

/************************************************************************/
// Renderer Context Init Exit (multi GPU)
/************************************************************************/
static uint32_t gRendererCount = 0;

void initRendererContext(const char* appName, const RendererContextDesc* pDesc, RendererContext** ppContext)
{
    ASSERT(appName);
    ASSERT(pDesc);
    ASSERT(ppContext);
    ASSERT(gRendererCount == 0);

    RendererContext* pContext = (RendererContext*)tf_calloc_memalign(1, sizeof(void*), sizeof(RendererContext));
    ASSERT(pContext);

    for (uint32_t i = 0; i < TF_ARRAY_COUNT(pContext->mGpus); ++i)
    {
        setDefaultGPUProperties(&pContext->mGpus[i]);
    }

#if defined(XBOX)
    ID3D12Device* device = NULL;
    // Create the DX12 API device object.
    CHECK_HRESULT(hook_create_device(NULL, D3D_FEATURE_LEVEL_12_1, true, &device));

    // First, retrieve the underlying DXGI device from the D3D device.
    IDXGIDevice1* dxgiDevice;
    CHECK_HRESULT(COM_CALL(QueryInterface, device, IID_ARGS(IDXGIDevice1, &dxgiDevice)));

    // Identify the physical adapter (GPU or card) this device is running on.
    IDXGIAdapter* dxgiAdapter;
    CHECK_HRESULT(COM_CALL(GetAdapter, dxgiDevice, &dxgiAdapter));

    // And obtain the factory object that created it.
    CHECK_HRESULT(COM_CALL(GetParent, dxgiAdapter, IID_ARGS(IDXGIFactory2, &pContext->mDx.pDXGIFactory)));

    GpuDesc* gpu = &pContext->mGpus[0];
    pContext->mGpuCount = 1;
    COM_CALL(QueryInterface, dxgiAdapter, IID_ARGS(IDXGIAdapter, &gpu->mDx.pGpu));
    SAFE_RELEASE(dxgiAdapter);

    DXGPUInfo gpuDesc = { 0 };
    gpuDesc.pGpu = gpu->mDx.pGpu;
    hook_fill_gpu_desc(device, D3D_FEATURE_LEVEL_12_1, &gpuDesc);
    QueryGpuDesc(device, &gpuDesc, gpu);
    d3d12CapsBuilder(device, gpu);
    gpu->mDx.pDevice = device;
    applyGPUConfigurationRules(gpu);
#else
    InitCommon(pDesc, pContext);

    bool foundSoftwareAdapter = false;

    // Find number of usable GPUs
    util_enumerate_gpus(pContext->mDx.pDXGIFactory, &pContext->mGpuCount, NULL, &foundSoftwareAdapter);

    // If the only adapter we found is a software adapter, log error message for QA
    if (!pContext->mGpuCount && foundSoftwareAdapter)
    {
        LOGF(eERROR, "The only available GPU has DXGI_ADAPTER_FLAG_SOFTWARE. Early exiting");
        ASSERT(false);
        return;
    }

    ASSERT(pContext->mGpuCount);
    DXGPUInfo gpuDesc[MAX_MULTIPLE_GPUS] = { 0 };

    util_enumerate_gpus(pContext->mDx.pDXGIFactory, &pContext->mGpuCount, gpuDesc, NULL);
    ASSERT(pContext->mGpuCount > 0);
    for (uint32_t i = 0; i < pContext->mGpuCount; ++i)
    {
        ID3D12Device* device = NULL;
        // Create device to query additional properties.
        d3d12dll_CreateDevice(gpuDesc[i].pGpu, gpuDesc[i].mMaxSupportedFeatureLevel, IID_ARGS(ID3D12Device, &device));

        QueryGpuDesc(device, &gpuDesc[i], &pContext->mGpus[i]);
        d3d12CapsBuilder(device, &pContext->mGpus[i]);

        pContext->mGpus[i].mDx.pGpu = gpuDesc[i].pGpu;
        pContext->mGpus[i].mFeatureLevel = gpuDesc[i].mMaxSupportedFeatureLevel;
        pContext->mGpus[i].mMaxBoundTextures =
            gpuDesc[i].mFeatureDataOptions.ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_1 ? 128 : 1000000;

        applyGPUConfigurationRules(&pContext->mGpus[i]);

        LOGF(eINFO, "GPU[%u] detected. Vendor ID: %#x, Model ID: %#x, Revision ID: %#x, Preset: %s, GPU Name: %s", i,
             pContext->mGpus[i].mGpuVendorPreset.mVendorId, pContext->mGpus[i].mGpuVendorPreset.mModelId,
             pContext->mGpus[i].mGpuVendorPreset.mRevisionId, presetLevelToString(pContext->mGpus[i].mGpuVendorPreset.mPresetLevel),
             pContext->mGpus[i].mGpuVendorPreset.mGpuName);

        SAFE_RELEASE(device);
    }

#endif
    *ppContext = pContext;
}

void exitRendererContext(RendererContext* pContext)
{
    ASSERT(pContext);
    for (uint32_t i = 0; i < pContext->mGpuCount; ++i)
    {
        SAFE_RELEASE(pContext->mGpus[i].mDx.pGpu);
    }
    ExitCommon(pContext);
#if defined(XBOX)
    hook_remove_device(pContext->mGpus[0].mDx.pDevice);
#endif
    SAFE_FREE(pContext);
}
/************************************************************************/
// Renderer Init Remove
/************************************************************************/
void initRenderer(const char* appName, const RendererDesc* pDesc, Renderer** ppRenderer)
{
    ASSERT(appName);
    ASSERT(pDesc);
    ASSERT(ppRenderer);

    Renderer* pRenderer = (Renderer*)tf_calloc_memalign(1, ALIGN_Renderer, sizeof(Renderer));
    ASSERT(pRenderer);

    pRenderer->mRendererApi = RENDERER_API_D3D12;
    pRenderer->mGpuMode = pDesc->mGpuMode;
    pRenderer->mShaderTarget = pDesc->mShaderTarget;
    pRenderer->pName = appName;

    // Initialize the D3D12 bits
    {
        if (!pDesc->pContext)
        {
            RendererContextDesc contextDesc = { 0 };
            contextDesc.mEnableGpuBasedValidation = pDesc->mEnableGpuBasedValidation;
            contextDesc.mDx.mFeatureLevel = pDesc->mDx.mFeatureLevel;
            pRenderer->mOwnsContext = true;
            initRendererContext(appName, &contextDesc, &pRenderer->pContext);
        }
        else
        {
            pRenderer->pContext = pDesc->pContext;
            pRenderer->mUnlinkedRendererIndex = gRendererCount;
            pRenderer->mOwnsContext = false;
        }

        if (!AddDevice(pDesc, pRenderer))
        {
            *ppRenderer = NULL;
            return;
        }

#if !defined(XBOX)
        // anything below LOW preset is not supported and we will exit
        if (pRenderer->pGpu->mGpuVendorPreset.mPresetLevel < GPU_PRESET_VERYLOW)
        {
            // remove device and any memory we allocated in just above as this is the first function called
            // when initializing the forge
            RemoveDevice(pRenderer);
            SAFE_FREE(pRenderer);
            LOGF(eERROR, "Selected GPU has an Office Preset in gpu.cfg.");
            LOGF(eERROR, "Office preset is not supported by The Forge.");

            // have the condition in the assert as well so its cleared when the assert message box appears
            ASSERT(pRenderer->pGpu->mGpuVendorPreset.mPresetLevel >= GPU_PRESET_VERYLOW); //-V547 //-V522

            // return NULL pRenderer so that client can gracefully handle exit
            // This is better than exiting from here in case client has allocated memory or has fallbacks
            *ppRenderer = NULL;
            return;
        }

        if (pRenderer->mShaderTarget >= SHADER_TARGET_6_0)
        {
            // Query the level of support of Shader Model.
            D3D12_FEATURE_DATA_SHADER_MODEL   shaderModelSupport = { D3D_SHADER_MODEL_6_0 };
            D3D12_FEATURE_DATA_D3D12_OPTIONS1 waveIntrinsicsSupport = { 0 };
            if (!SUCCEEDED(COM_CALL(CheckFeatureSupport, pRenderer->mDx.pDevice, (D3D12_FEATURE)D3D12_FEATURE_SHADER_MODEL,
                                    &shaderModelSupport, sizeof(shaderModelSupport))))
            {
                return;
            }
            // Query the level of support of Wave Intrinsics.
            if (!SUCCEEDED(COM_CALL(CheckFeatureSupport, pRenderer->mDx.pDevice, (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1,
                                    &waveIntrinsicsSupport, sizeof(waveIntrinsicsSupport))))
            {
                return;
            }

            // If the device doesn't support SM6 or Wave Intrinsics, try enabling the experimental feature for Shader Model 6 and creating
            // the device again.
            if (shaderModelSupport.HighestShaderModel != D3D_SHADER_MODEL_6_0 || waveIntrinsicsSupport.WaveOps == FALSE)
            {
                RENDERDOC_API_1_1_2* rdoc_api = NULL;
                // At init, on windows
                HMODULE              mod = GetModuleHandleA("renderdoc.dll");
                if (mod)
                {
                    pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
                    RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);
                }

                // If RenderDoc is connected shader model 6 is not detected but it still works
                if (!rdoc_api || !rdoc_api->IsTargetControlConnected())
                {
                    // If the device still doesn't support SM6 or Wave Intrinsics after enabling the experimental feature, you could set up
                    // your application to use the highest supported shader model. For simplicity we just exit the application here.
                    if (shaderModelSupport.HighestShaderModel < D3D_SHADER_MODEL_6_0 ||
                        (waveIntrinsicsSupport.WaveOps == FALSE && !SUCCEEDED(EnableExperimentalShaderModels())))
                    {
                        RemoveDevice(pRenderer);
                        LOGF(eERROR, "Hardware does not support Shader Model 6.0");
                        return;
                    }
                }
                else
                {
                    LOGF(eWARNING,
                         "\nRenderDoc does not support SM 6.0 or higher. Application might work but you won't be able to debug the SM 6.0+ "
                         "shaders or view their bytecode.");
                }
            }
        }
#endif

        /************************************************************************/
        // Multi GPU - SLI Node Count
        /************************************************************************/
        uint32_t gpuCount = COM_CALL(GetNodeCount, pRenderer->mDx.pDevice);
        pRenderer->mLinkedNodeCount = (pRenderer->mGpuMode == GPU_MODE_LINKED) ? gpuCount : 1;
        if (pRenderer->mGpuMode == GPU_MODE_LINKED && pRenderer->mLinkedNodeCount < 2)
            pRenderer->mGpuMode = GPU_MODE_SINGLE;
        /************************************************************************/
        // Descriptor heaps
        /************************************************************************/
        pRenderer->mDx.pCPUDescriptorHeaps = (DescriptorHeap**)tf_malloc(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES * sizeof(DescriptorHeap*));
        pRenderer->mDx.pCbvSrvUavHeaps = (DescriptorHeap**)tf_malloc(pRenderer->mLinkedNodeCount * sizeof(DescriptorHeap*));
        pRenderer->mDx.pSamplerHeaps = (DescriptorHeap**)tf_malloc(pRenderer->mLinkedNodeCount * sizeof(DescriptorHeap*));

        for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {
                .Type = (D3D12_DESCRIPTOR_HEAP_TYPE)i,
                .Flags = gCpuDescriptorHeapProperties[i].mFlags,
                .NodeMask = 0, // CPU Descriptor Heap - Node mask is irrelevant
                .NumDescriptors = gCpuDescriptorHeapProperties[i].mMaxDescriptors,
            };
            add_descriptor_heap(pRenderer->mDx.pDevice, &desc, &pRenderer->mDx.pCPUDescriptorHeaps[i]);
        }

        // One shader visible heap for each linked node
        for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {
                .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                .NodeMask = util_calculate_node_mask(pRenderer, i),
                .NumDescriptors = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1,
            };
            add_descriptor_heap(pRenderer->mDx.pDevice, &desc, &pRenderer->mDx.pCbvSrvUavHeaps[i]);

            // Max sampler descriptor count
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            desc.NumDescriptors = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
            add_descriptor_heap(pRenderer->mDx.pDevice, &desc, &pRenderer->mDx.pSamplerHeaps[i]);
        }
        /************************************************************************/
        // Memory allocator
        /************************************************************************/
        CHECK_HRESULT(D3D12MA_CreateAllocator(pRenderer->mDx.pDevice, pRenderer->pGpu->mDx.pGpu, &pRenderer->mDx.pResourceAllocator));
    }
    /************************************************************************/
    /************************************************************************/
    add_default_resources(pRenderer);

    hook_post_init_renderer(pRenderer);

    ++gRendererCount;
    ASSERT(gRendererCount <= MAX_UNLINKED_GPUS);

    // Renderer is good!
    *ppRenderer = pRenderer;
}

void exitRenderer(Renderer* pRenderer)
{
    ASSERT(pRenderer);
    --gRendererCount;

    remove_default_resources(pRenderer);

    // Destroy the Direct3D12 bits
    for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        remove_descriptor_heap(pRenderer->mDx.pCPUDescriptorHeaps[i]);
    }

    for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
    {
        remove_descriptor_heap(pRenderer->mDx.pCbvSrvUavHeaps[i]);
        remove_descriptor_heap(pRenderer->mDx.pSamplerHeaps[i]);
    }

    D3D12MA_ReleaseAllocator(pRenderer->mDx.pResourceAllocator);

    RemoveDevice(pRenderer);

    hook_post_remove_renderer(pRenderer);

    if (pRenderer->mOwnsContext)
    {
        exitRendererContext(pRenderer->pContext);
    }

    // Free all the renderer components
    SAFE_FREE(pRenderer->mDx.pCPUDescriptorHeaps);
    SAFE_FREE(pRenderer->mDx.pCbvSrvUavHeaps);
    SAFE_FREE(pRenderer->mDx.pSamplerHeaps);
    SAFE_FREE(pRenderer);
}

/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void initFence(Renderer* pRenderer, Fence** ppFence)
{
    // ASSERT that renderer is valid
    ASSERT(pRenderer);
    ASSERT(ppFence);

    // create a Fence and ASSERT that it is valid
    Fence* pFence = (Fence*)tf_calloc(1, sizeof(Fence));
    ASSERT(pFence);

    CHECK_HRESULT(COM_CALL(CreateFence, pRenderer->mDx.pDevice, 0, D3D12_FENCE_FLAG_NONE, IID_ARGS(ID3D12Fence, &pFence->mDx.pFence)));
    pFence->mDx.mFenceValue = 0;

    pFence->mDx.pWaitIdleFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    *ppFence = pFence;
}

void exitFence(Renderer* pRenderer, Fence* pFence)
{
    // ASSERT that renderer is valid
    ASSERT(pRenderer);
    // ASSERT that given fence to remove is valid
    ASSERT(pFence);

    SAFE_RELEASE(pFence->mDx.pFence);
    CloseHandle(pFence->mDx.pWaitIdleFenceEvent);

    SAFE_FREE(pFence);
}

void initSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
    // ASSERT that renderer is valid
    ASSERT(pRenderer);
    ASSERT(ppSemaphore);

    // create a Fence and ASSERT that it is valid
    Semaphore* pSemaphore = (Semaphore*)tf_calloc(1, sizeof(Semaphore));
    ASSERT(pSemaphore);

    CHECK_HRESULT(COM_CALL(CreateFence, pRenderer->mDx.pDevice, 0, D3D12_FENCE_FLAG_NONE, IID_ARGS(ID3D12Fence, &pSemaphore->mDx.pFence)));
    pSemaphore->mDx.mFenceValue = 0;

    *ppSemaphore = pSemaphore;
}

void exitSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
    // ASSERT that renderer is valid
    ASSERT(pRenderer);
    // ASSERT that given fence to remove is valid
    ASSERT(pSemaphore);

    SAFE_RELEASE(pSemaphore->mDx.pFence);
    SAFE_FREE(pSemaphore);
}

void initQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppQueue);

    Queue* pQueue = (Queue*)tf_calloc(1, sizeof(Queue));
    ASSERT(pQueue);

    const uint32_t nodeIndex = pRenderer->mGpuMode == GPU_MODE_UNLINKED ? 0 : pDesc->mNodeIndex;
    if (nodeIndex)
    {
        ASSERT(pRenderer->mGpuMode == GPU_MODE_LINKED && "Node Masking can only be used with Linked Multi GPU");
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {
        .Type = gDx12CmdTypeTranslator[pDesc->mType],
        .Priority = gDx12QueuePriorityTranslator[pDesc->mPriority],
        .NodeMask = util_calculate_node_mask(pRenderer, nodeIndex),
    };
    if (pDesc->mFlag & QUEUE_FLAG_DISABLE_GPU_TIMEOUT)
        queueDesc.Flags |= D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;

    CHECK_HRESULT(hook_create_command_queue(pRenderer->mDx.pDevice, &queueDesc, &pQueue->mDx.pQueue));

    ULONG      refCount = COM_CALL(AddRef, pQueue->mDx.pQueue);
    const bool firstQueue = 2 == refCount;
    COM_CALL(Release, pQueue->mDx.pQueue);
    if (firstQueue)
    {
        char queueTypeBuffer[MAX_DEBUG_NAME_LENGTH] = { 0 };
        if (!pDesc->pName)
        {
            const char* queueNames[] = {
                "GRAPHICS QUEUE",
                "INVALID",
                "COMPUTE QUEUE",
                "COPY QUEUE",
            };
            snprintf(queueTypeBuffer, MAX_DEBUG_NAME_LENGTH, "%s %u", queueNames[queueDesc.Type], pDesc->mNodeIndex);
        }
        SetObjectName((ID3D12Object*)pQueue->mDx.pQueue, pDesc->pName ? pDesc->pName : queueTypeBuffer); // -V1027
    }

    pQueue->mType = pDesc->mType;
    pQueue->mNodeIndex = pDesc->mNodeIndex;
#if defined(_WINDOWS) && defined(FORGE_DEBUG)
    pQueue->mDx.pRenderer = pRenderer;
#endif

    // override node index
    if (pRenderer->mGpuMode == GPU_MODE_UNLINKED)
        pQueue->mNodeIndex = pRenderer->mUnlinkedRendererIndex;

    // Add queue fence. This fence will make sure we finish all GPU works before releasing the queue
    initFence(pRenderer, &pQueue->mDx.pFence);

    *ppQueue = pQueue;
}

void exitQueue(Renderer* pRenderer, Queue* pQueue)
{
    ASSERT(pQueue);

    // Make sure we finished all GPU works before we remove the queue
    waitQueueIdle(pQueue);

    exitFence(pRenderer, pQueue->mDx.pFence);

    SAFE_RELEASE(pQueue->mDx.pQueue);

    SAFE_FREE(pQueue);
}

void initCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
{
    // ASSERT that renderer is valid
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppCmdPool);

    // create one new CmdPool and add to renderer
    CmdPool* pCmdPool = (CmdPool*)tf_calloc(1, sizeof(CmdPool));
    ASSERT(pCmdPool);

    CHECK_HRESULT(COM_CALL(CreateCommandAllocator, pRenderer->mDx.pDevice, gDx12CmdTypeTranslator[pDesc->pQueue->mType],
                           IID_ARGS(ID3D12CommandAllocator, &pCmdPool->pCmdAlloc)));

    pCmdPool->pQueue = pDesc->pQueue;

    *ppCmdPool = pCmdPool;
}

void exitCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
    // check validity of given renderer and command pool
    ASSERT(pRenderer);
    ASSERT(pCmdPool);

    SAFE_RELEASE(pCmdPool->pCmdAlloc);
    SAFE_FREE(pCmdPool);
}

void initCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
{
    // verify that given pool is valid
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppCmd);

    Cmd* pCmd = (Cmd*)tf_calloc_memalign(1, ALIGN_Cmd, sizeof(Cmd));
    ASSERT(pCmd);

    // set command pool of new command
    pCmd->mDx.mNodeIndex = pRenderer->mGpuMode == GPU_MODE_LINKED ? pDesc->pPool->pQueue->mNodeIndex : 0;
    pCmd->mDx.mType = pDesc->pPool->pQueue->mType;
    pCmd->pQueue = pDesc->pPool->pQueue;
    pCmd->pRenderer = pRenderer;

    pCmd->mDx.pBoundHeaps[0] = pRenderer->mDx.pCbvSrvUavHeaps[pCmd->mDx.mNodeIndex];
    pCmd->mDx.pBoundHeaps[1] = pRenderer->mDx.pSamplerHeaps[pCmd->mDx.mNodeIndex];

    pCmd->mDx.pCmdPool = pDesc->pPool;

    uint32_t nodeMask = util_calculate_node_mask(pRenderer, pCmd->mDx.mNodeIndex);

    CHECK_HRESULT(hook_create_cmd(pRenderer->mDx.pDevice, pDesc->pPool->pQueue->mType, nodeMask, pDesc->pPool->pCmdAlloc, pCmd));

    // Command lists are addd in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    CHECK_HRESULT(COM_CALL(Close, pCmd->mDx.pCmdList));

#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    if (pDesc->pName)
    {
        SetObjectName((ID3D12Object*)pCmd->mDx.pCmdList, pDesc->pName); // -V1027
    }
#endif // ENABLE_GRAPHICS_DEBUG_ANNOTATION

#if defined(ENABLE_GRAPHICS_VALIDATION) && defined(_WINDOWS)
    COM_CALL(QueryInterface, pCmd->mDx.pCmdList, IID_ARGS(ID3D12DebugCommandList, &pCmd->mDx.pDebugCmdList));
#endif

    *ppCmd = pCmd;
}

void exitCmd(Renderer* pRenderer, Cmd* pCmd)
{
    // verify that given command and pool are valid
    ASSERT(pRenderer);
    ASSERT(pCmd);

#if defined(ENABLE_GRAPHICS_VALIDATION) && defined(_WINDOWS)
    SAFE_RELEASE(pCmd->mDx.pDebugCmdList);
#endif

    hook_remove_cmd(pCmd);

    SAFE_FREE(pCmd);
}

void initCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmd)
{
    // verify that ***cmd is valid
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(cmdCount);
    ASSERT(pppCmd);

    Cmd** ppCmds = (Cmd**)tf_calloc(cmdCount, sizeof(Cmd*));
    ASSERT(ppCmds);

    // add n new cmds to given pool
    for (uint32_t i = 0; i < cmdCount; ++i)
    {
        initCmd(pRenderer, pDesc, &ppCmds[i]);
    }

    *pppCmd = ppCmds;
}

void exitCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
{
    // verify that given command list is valid
    ASSERT(ppCmds);

    // remove every given cmd in array
    for (uint32_t i = 0; i < cmdCount; ++i)
    {
        exitCmd(pRenderer, ppCmds[i]);
    }

    SAFE_FREE(ppCmds);
}

void toggleVSync(Renderer* pRenderer, SwapChain** ppSwapChain)
{
    UNREF_PARAM(pRenderer);
    ASSERT(ppSwapChain);

    SwapChain* pSwapChain = *ppSwapChain;
    // set descriptor vsync boolean
    pSwapChain->mEnableVsync = !pSwapChain->mEnableVsync;
#if !defined(XBOX)
    if (!pSwapChain->mEnableVsync)
    {
        pSwapChain->mDx.mFlags |= DXGI_PRESENT_ALLOW_TEARING;
    }
    else
    {
        pSwapChain->mDx.mFlags &= ~DXGI_PRESENT_ALLOW_TEARING;
    }
#endif

    // toggle vsync present flag (this can go up to 4 but we don't need to refresh on nth vertical sync)
    pSwapChain->mDx.mSyncInterval = (pSwapChain->mDx.mSyncInterval + 1) % 2;
}

bool getSwapchainFormatSupport(Renderer* pRenderer, Queue* pQueue, TinyImageFormat format, ColorSpace colorspace);

void addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppSwapChain);

    LOGF(eINFO, "Adding D3D12 swapchain @ %ux%u", pDesc->mWidth, pDesc->mHeight);

    SwapChain* pSwapChain = (SwapChain*)tf_calloc(1, sizeof(SwapChain) + pDesc->mImageCount * sizeof(RenderTarget*));
    ASSERT(pSwapChain);
    pSwapChain->ppRenderTargets = (RenderTarget**)(pSwapChain + 1);
    ASSERT(pSwapChain->ppRenderTargets);

    pSwapChain->mColorSpace = pDesc->mColorSpace;
    pSwapChain->mFormat = pDesc->mColorFormat;

#if !defined(XBOX)
    pSwapChain->mDx.mSyncInterval = pDesc->mEnableVsync ? 1 : 0;

    DXGI_SWAP_CHAIN_DESC1 desc = {
        .Width = pDesc->mWidth,
        .Height = pDesc->mHeight,
        .Format = util_to_dx12_swapchain_format(pDesc->mColorFormat),
        .Stereo = false,
        .SampleDesc.Count = 1, // If multisampling is needed, we'll resolve it later
        .SampleDesc.Quality = 0,
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = pDesc->mImageCount,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
    };
    BOOL allowTearing = FALSE;
    COM_CALL(CheckFeatureSupport, pRenderer->pContext->mDx.pDXGIFactory, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing,
             sizeof(allowTearing));
    desc.Flags |= allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    pSwapChain->mDx.mFlags |= (!pDesc->mEnableVsync && allowTearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;

    IDXGISwapChain1* swapchain;

    HWND hwnd = (HWND)pDesc->mWindowHandle.window;

    IUnknown* pQueueDx = (IUnknown*)pDesc->ppPresentQueues[0]->mDx.pQueue; // -V1027
    CHECK_HRESULT(COM_CALL(CreateSwapChainForHwnd, pRenderer->pContext->mDx.pDXGIFactory, pQueueDx, hwnd, &desc, NULL, NULL, &swapchain));

    CHECK_HRESULT(COM_CALL(MakeWindowAssociation, pRenderer->pContext->mDx.pDXGIFactory, hwnd, DXGI_MWA_NO_ALT_ENTER));

    CHECK_HRESULT(COM_CALL(QueryInterface, swapchain, IID_ARGS(IDXGISwapChain3, &pSwapChain->mDx.pSwapChain)));
    COM_CALL(Release, swapchain);

    // Allowing multiple command queues to present for applications like Alternate Frame Rendering
    if (pRenderer->mGpuMode == GPU_MODE_LINKED && pDesc->mPresentQueueCount > 1)
    {
        IUnknown** ppQueues = (IUnknown**)_alloca(pDesc->mPresentQueueCount * sizeof(IUnknown*));
        UINT*      pCreationMasks = (UINT*)_alloca(pDesc->mPresentQueueCount * sizeof(UINT));
        for (uint32_t i = 0; i < pDesc->mPresentQueueCount; ++i)
        {
            ppQueues[i] = (IUnknown*)pDesc->ppPresentQueues[i]->mDx.pQueue; // -V1027
            pCreationMasks[i] = (1 << pDesc->ppPresentQueues[i]->mNodeIndex);
        }

        COM_CALL(ResizeBuffers1, pSwapChain->mDx.pSwapChain, desc.BufferCount, desc.Width, desc.Height, desc.Format, desc.Flags,
                 pCreationMasks, ppQueues);
    }

    ID3D12Resource** buffers = (ID3D12Resource**)_alloca(pDesc->mImageCount * sizeof(ID3D12Resource*));

    // Create rendertargets from swapchain
    for (uint32_t i = 0; i < pDesc->mImageCount; ++i)
    {
        CHECK_HRESULT(COM_CALL(GetBuffer, pSwapChain->mDx.pSwapChain, i, IID_ARGS(ID3D12Resource, &buffers[i])));
    }

    DXGI_COLOR_SPACE_TYPE colorSpace = util_to_dx12_colorspace(pDesc->mColorSpace);
    UINT                  colorSpaceSupport = 0;
    CHECK_HRESULT(COM_CALL(CheckColorSpaceSupport, pSwapChain->mDx.pSwapChain, colorSpace, &colorSpaceSupport));
    if ((colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)
    {
        COM_CALL(SetColorSpace1, pSwapChain->mDx.pSwapChain, colorSpace);
    }
#endif

    RenderTargetDesc descColor = {
        .mWidth = pDesc->mWidth,
        .mHeight = pDesc->mHeight,
        .mDepth = 1,
        .mArraySize = 1,
        .mFormat = pDesc->mColorFormat,
        .mClearValue = pDesc->mColorClearValue,
        .mSampleCount = SAMPLE_COUNT_1,
        .mSampleQuality = 0,
        .pNativeHandle = NULL,
        .mFlags = TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET,
        .mStartState = RESOURCE_STATE_PRESENT,
    };
#ifdef AUTOMATED_TESTING
    descColor.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
#endif
#if defined(XBOX)
    descColor.mFlags |= TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
    pSwapChain->mDx.pPresentQueue = pDesc->mPresentQueueCount ? pDesc->ppPresentQueues[0] : NULL;
#endif

    for (uint32_t i = 0; i < pDesc->mImageCount; ++i)
    {
#if !defined(XBOX)
        descColor.pNativeHandle = (void*)buffers[i];
#endif
        addRenderTarget(pRenderer, &descColor, &pSwapChain->ppRenderTargets[i]);
    }

    pSwapChain->mImageCount = pDesc->mImageCount;
    pSwapChain->mEnableVsync = pDesc->mEnableVsync;

    *ppSwapChain = pSwapChain;
}

void removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
#if defined(XBOX)
    hook_queue_present(pSwapChain->mDx.pPresentQueue, NULL, 0);
#endif

    for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
    {
        ID3D12Resource* resource = pSwapChain->ppRenderTargets[i]->pTexture->mDx.pResource;
        removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[i]);
#if !defined(XBOX)
        SAFE_RELEASE(resource);
#else
        (void)resource;
#endif
    }

#if !defined(XBOX)
    SAFE_RELEASE(pSwapChain->mDx.pSwapChain);
#endif
    SAFE_FREE(pSwapChain);
}

void addResourceHeap(Renderer* pRenderer, const ResourceHeapDesc* pDesc, ResourceHeap** ppHeap)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppHeap);

    uint64_t allocationSize = pDesc->mSize;

    if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER))
    {
        allocationSize = round_up_64(allocationSize, pRenderer->pGpu->mUniformBufferAlignment);
    }

    D3D12_HEAP_DESC heapDesc = { 0 };
    heapDesc.SizeInBytes = allocationSize;
    heapDesc.Alignment = pDesc->mAlignment;
    heapDesc.Properties.Type = util_to_heap_type(pDesc->mMemoryUsage);
    heapDesc.Flags = util_to_heap_flags(pDesc->mFlags);

    // Multi GPU
    if (pRenderer->mGpuMode == GPU_MODE_LINKED)
    {
        heapDesc.Properties.CreationNodeMask = (1 << pDesc->mNodeIndex);
        heapDesc.Properties.VisibleNodeMask = heapDesc.Properties.CreationNodeMask;
        for (uint32_t i = 0; i < pDesc->mSharedNodeIndexCount; ++i)
            heapDesc.Properties.VisibleNodeMask |= (1 << pDesc->pSharedNodeIndices[i]);
    }
    else
    {
        heapDesc.Properties.CreationNodeMask = 1;
        heapDesc.Properties.VisibleNodeMask = heapDesc.Properties.CreationNodeMask;
    }

    // Special heap flags
    hook_modify_heap_flags(pDesc->mDescriptors, &heapDesc.Flags);

    ID3D12Heap* pDxHeap = NULL;
    CHECK_HRESULT(COM_CALL(CreateHeap, pRenderer->mDx.pDevice, &heapDesc, IID_ARGS(ID3D12Heap, &pDxHeap)));
    ASSERT(pDxHeap);

    SetObjectName((ID3D12Object*)pDxHeap, pDesc->pName); // -V1027

    ResourceHeap* pHeap = (ResourceHeap*)tf_calloc(1, sizeof(ResourceHeap));
    pHeap->mDx.pHeap = pDxHeap;
    pHeap->mSize = pDesc->mSize;

#if defined(XBOX)
    {
        D3D12_RESOURCE_DESC resDesc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
            .Width = 16,
            .Height = 1,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc.Count = 1,
            .SampleDesc.Quality = 0,
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = D3D12_RESOURCE_FLAG_NONE,
        };

        ID3D12Resource* resource = NULL;
        CHECK_HRESULT(COM_CALL(CreatePlacedResource, pRenderer->mDx.pDevice, pDxHeap,
                               0, // AllocationLocalOffset
                               &resDesc, D3D12_RESOURCE_STATE_COMMON,
                               NULL, // pOptimizedClearValue
                               IID_ARGS(ID3D12Resource, &resource)));

        ASSERT(resource);
        pHeap->mDx.mPtr = COM_CALL(GetGPUVirtualAddress, resource);

        // We just needed to create this resource to get the address to the memory
        COM_CALL(Release, resource);
    }
#endif

    *ppHeap = pHeap;
}

void removeResourceHeap(Renderer* pRenderer, ResourceHeap* pHeap)
{
    UNREF_PARAM(pRenderer);

    SAFE_RELEASE(pHeap->mDx.pHeap);
    SAFE_FREE(pHeap);
}

void getBufferSizeAlign(Renderer* pRenderer, const BufferDesc* pDesc, ResourceSizeAlign* pOut)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(pOut);
    ASSERT(pRenderer->mGpuMode != GPU_MODE_UNLINKED || pDesc->mNodeIndex == pRenderer->mUnlinkedRendererIndex);

    D3D12_RESOURCE_DESC desc = { 0 };
    InitializeBufferDesc(pRenderer, pDesc, &desc);

    const UINT                     visibleMask = (1 << pDesc->mNodeIndex);
    D3D12_RESOURCE_ALLOCATION_INFO allocInfo;
    COM_CALL_RETURN_DST_FIRST(GetResourceAllocationInfo, pRenderer->mDx.pDevice, allocInfo, visibleMask, 1, &desc);

    pOut->mSize = allocInfo.SizeInBytes;
    pOut->mAlignment = allocInfo.Alignment;
}

void getTextureSizeAlign(Renderer* pRenderer, const TextureDesc* pDesc, ResourceSizeAlign* pOut)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(pOut);
    ASSERT(pRenderer->mGpuMode != GPU_MODE_UNLINKED || pDesc->mNodeIndex == pRenderer->mUnlinkedRendererIndex);

    D3D12_RESOURCE_DESC desc = { 0 };
    InitializeTextureDesc(pRenderer, pDesc, &desc, NULL);

    const UINT                     visibleMask = (1 << pDesc->mNodeIndex);
    D3D12_RESOURCE_ALLOCATION_INFO allocInfo;
    COM_CALL_RETURN_DST_FIRST(GetResourceAllocationInfo, pRenderer->mDx.pDevice, allocInfo, visibleMask, 1, &desc);

    pOut->mSize = allocInfo.SizeInBytes;
    pOut->mAlignment = allocInfo.Alignment;
}

void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
    // verify renderer validity
    ASSERT(pRenderer);
    // verify adding at least 1 buffer
    ASSERT(pDesc);
    ASSERT(ppBuffer);
    ASSERT(pDesc->mSize > 0);
    ASSERT(pRenderer->mGpuMode != GPU_MODE_UNLINKED || pDesc->mNodeIndex == pRenderer->mUnlinkedRendererIndex);

    Buffer* pBuffer = (Buffer*)tf_calloc_memalign(1, ALIGN_Buffer, sizeof(Buffer));
    pBuffer->mDx.mDescriptors = D3D12_DESCRIPTOR_ID_NONE;
    ASSERT(ppBuffer);

    // add to renderer

    D3D12_RESOURCE_DESC desc = { 0 };
    InitializeBufferDesc(pRenderer, pDesc, &desc);

    D3D12MA_ALLOCATION_DESC alloc_desc = {
        .HeapType = util_to_heap_type(pDesc->mMemoryUsage),
        .mUseDedicatedAllocation = (pDesc->mFlags & BUFFER_CREATION_FLAG_OWN_MEMORY_BIT) != 0,
    };

    // Multi GPU
    if (pRenderer->mGpuMode == GPU_MODE_LINKED)
    {
        alloc_desc.CreationNodeMask = (1 << pDesc->mNodeIndex);
        alloc_desc.VisibleNodeMask = alloc_desc.CreationNodeMask;
        for (uint32_t i = 0; i < pDesc->mSharedNodeIndexCount; ++i)
            alloc_desc.VisibleNodeMask |= (1 << pDesc->pSharedNodeIndices[i]);
    }
    else
    {
        alloc_desc.CreationNodeMask = 1;
        alloc_desc.VisibleNodeMask = alloc_desc.CreationNodeMask;
    }

    // Special heap flags
    hook_modify_heap_flags(pDesc->mDescriptors, &alloc_desc.ExtraHeapFlags);

    ResourceState start_state = pDesc->mStartState;
    if (pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU || pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_ONLY)
    {
        start_state = RESOURCE_STATE_GENERIC_READ;
    }
    else if (pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
    {
        start_state = RESOURCE_STATE_COPY_DEST;
    }

    D3D12_RESOURCE_STATES res_states = util_to_dx12_resource_state(start_state);
    alloc_desc.ResourceStates = res_states;

    // Create resource
    if (SUCCEEDED(hook_add_special_buffer(pRenderer, &desc, NULL, res_states, pDesc->mFlags, pBuffer)))
    {
        LOGF(eINFO, "Allocated memory in device-specific RAM");
    }
    else if (D3D12_HEAP_TYPE_READBACK == alloc_desc.HeapType && (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
    {
        ASSERT(!pDesc->pPlacement);
        LOGF(eWARNING, "Creating RWBuffer in Readback Heap. GPU access might be slower than default");
        D3D12_HEAP_PROPERTIES heapProps = { 0 };
        heapProps.Type = D3D12_HEAP_TYPE_CUSTOM;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
        heapProps.VisibleNodeMask = alloc_desc.VisibleNodeMask;
        heapProps.CreationNodeMask = alloc_desc.CreationNodeMask;
        CHECK_HRESULT(COM_CALL(CreateCommittedResource, pRenderer->mDx.pDevice, &heapProps, alloc_desc.ExtraHeapFlags, &desc,
                               D3D12_RESOURCE_STATE_COMMON, NULL, IID_ARGS(ID3D12Resource, &pBuffer->mDx.pResource)));
    }
    // #TODO: This is not at all good but seems like virtual textures are using this
    // Remove as soon as possible
    else if (D3D12_HEAP_TYPE_UPLOAD == alloc_desc.HeapType && (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
    {
        ASSERT(!pDesc->pPlacement);
        LOGF(eWARNING, "Creating RWBuffer in Upload heap. GPU access might be slower than default");
        D3D12_HEAP_PROPERTIES heapProps = {
            .Type = D3D12_HEAP_TYPE_CUSTOM,
            .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE,
            .MemoryPoolPreference = D3D12_MEMORY_POOL_L0,
            .VisibleNodeMask = alloc_desc.VisibleNodeMask,
            .CreationNodeMask = alloc_desc.CreationNodeMask,
        };
        CHECK_HRESULT(COM_CALL(CreateCommittedResource, pRenderer->mDx.pDevice, &heapProps, alloc_desc.ExtraHeapFlags, &desc,
                               D3D12_RESOURCE_STATE_COMMON, NULL, IID_ARGS(ID3D12Resource, &pBuffer->mDx.pResource)));
    }
    else
    {
        if (pDesc->pPlacement)
        {
            CHECK_HRESULT(hook_add_placed_resource(pRenderer, pDesc->pPlacement, &desc, NULL, res_states, &pBuffer->mDx.pResource));
        }
        else
        {
            CHECK_HRESULT(D3D12MA_CreateResource(pRenderer->mDx.pResourceAllocator, &alloc_desc, &desc, &pBuffer->mDx.pAllocation,
                                                 &pBuffer->mDx.pResource));
        }
    }

    if (pDesc->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && pDesc->mFlags & BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT)
    {
        COM_CALL(Map, pBuffer->mDx.pResource, 0, NULL, &pBuffer->pCpuMappedAddress);
    }

    pBuffer->mDx.mGpuAddress = COM_CALL(GetGPUVirtualAddress, pBuffer->mDx.pResource);
#if defined(XBOX)
    pBuffer->pCpuMappedAddress = (void*)pBuffer->mDx.mGpuAddress;
#endif

    if (!(pDesc->mFlags & BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION))
    {
        DescriptorHeap* pHeap = pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
        uint32_t        handleCount = ((pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER) ? 1 : 0) +
                               ((pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER) ? 1 : 0) +
                               ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER) ? 1 : 0);
        pBuffer->mDx.mDescriptors = consume_descriptor_handles(pHeap, handleCount);

        if (pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        {
            pBuffer->mDx.mSrvDescriptorOffset = 1;

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {
                .BufferLocation = pBuffer->mDx.mGpuAddress,
                .SizeInBytes = (UINT)desc.Width,
            };
            AddCbv(pRenderer, NULL, &cbvDesc, &pBuffer->mDx.mDescriptors);
        }

        if (pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER)
        {
            DxDescriptorID srv = pBuffer->mDx.mDescriptors + pBuffer->mDx.mSrvDescriptorOffset;
            pBuffer->mDx.mUavDescriptorOffset = pBuffer->mDx.mSrvDescriptorOffset + 1;
            if (pDesc->mFormat != TinyImageFormat_UNDEFINED)
            {
                AddTypedBufferSrv(pRenderer, NULL, pBuffer->mDx.pResource, pDesc->mFirstElement, pDesc->mElementCount, pDesc->mFormat,
                                  &srv);
            }
            else
            {
                const bool raw = DESCRIPTOR_TYPE_BUFFER_RAW == (pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER_RAW);
                AddBufferSrv(pRenderer, NULL, pBuffer->mDx.pResource, raw, pDesc->mFirstElement, pDesc->mElementCount, pDesc->mStructStride,
                             &srv);
            }
        }

        if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER)
        {
            DxDescriptorID uav = pBuffer->mDx.mDescriptors + pBuffer->mDx.mUavDescriptorOffset;
            if (pDesc->mFormat != TinyImageFormat_UNDEFINED)
            {
                AddTypedBufferUav(pRenderer, NULL, pBuffer->mDx.pResource, pDesc->mFirstElement, pDesc->mElementCount, pDesc->mFormat,
                                  &uav);
            }
            else
            {
                const bool      raw = DESCRIPTOR_TYPE_RW_BUFFER_RAW == (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER_RAW);
                ID3D12Resource* pCounterBuffer = pDesc->pCounterBuffer ? pDesc->pCounterBuffer->mDx.pResource : NULL;
                AddBufferUav(pRenderer, NULL, pBuffer->mDx.pResource, pCounterBuffer, 0, raw, pDesc->mFirstElement, pDesc->mElementCount,
                             pDesc->mStructStride, &uav);
            }
        }
    }

    // Set name
    SetObjectName((ID3D12Object*)pBuffer->mDx.pResource, pDesc->pName); // -V1027

    pBuffer->mSize = (uint32_t)pDesc->mSize;
    pBuffer->mMemoryUsage = pDesc->mMemoryUsage;
    pBuffer->mNodeIndex = pDesc->mNodeIndex;
    pBuffer->mDescriptors = pDesc->mDescriptors;

    *ppBuffer = pBuffer;
}

void removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRenderer);
    ASSERT(pBuffer);

    if (pBuffer->mDx.mDescriptors != D3D12_DESCRIPTOR_ID_NONE)
    {
        uint32_t handleCount = ((pBuffer->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER) ? 1 : 0) +
                               ((pBuffer->mDescriptors & DESCRIPTOR_TYPE_BUFFER) ? 1 : 0) +
                               ((pBuffer->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER) ? 1 : 0);
        return_descriptor_handles(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], pBuffer->mDx.mDescriptors,
                                  handleCount);
    }

#if !defined(XBOX)
    if (pBuffer->mDx.mMarkerBuffer)
    {
        SAFE_RELEASE(pBuffer->mDx.pMarkerBufferHeap);
        VirtualFree(pBuffer->pCpuMappedAddress, 0, MEM_DECOMMIT);
    }
    else
#endif
    {
        D3D12MA_ReleaseAllocation(pBuffer->mDx.pAllocation);
    }
    SAFE_RELEASE(pBuffer->mDx.pResource);

    SAFE_FREE(pBuffer);
}

void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to map non-cpu accessible resource");

    D3D12_RANGE range = { 0, pBuffer->mSize };
    if (pRange)
    {
        range.Begin += pRange->mOffset;
        range.End = range.Begin + pRange->mSize;
    }

    CHECK_HRESULT(COM_CALL(Map, pBuffer->mDx.pResource, 0, &range, &pBuffer->pCpuMappedAddress));
}

void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to unmap non-cpu accessible resource");

    COM_CALL(Unmap, pBuffer->mDx.pResource, 0, NULL);
    pBuffer->pCpuMappedAddress = NULL;
}

void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
{
    ASSERT(pRenderer);
    ASSERT(pDesc && pDesc->mWidth && pDesc->mHeight && (pDesc->mDepth || pDesc->mArraySize));
    ASSERT(pRenderer->mGpuMode != GPU_MODE_UNLINKED || pDesc->mNodeIndex == pRenderer->mUnlinkedRendererIndex);
    if (pDesc->mSampleCount > SAMPLE_COUNT_1 && pDesc->mMipLevels > 1)
    {
        LOGF(eERROR, "Multi-Sampled textures cannot have mip maps");
        ASSERT(false);
        return;
    }

    Texture* pTexture = (Texture*)tf_calloc_memalign(1, ALIGN_Texture, sizeof(Texture));
    pTexture->mDx.mDescriptors = D3D12_DESCRIPTOR_ID_NONE;
    pTexture->mDx.mStencilDescriptor = D3D12_DESCRIPTOR_ID_NONE;
    ASSERT(pTexture);

    if (pDesc->pNativeHandle)
    {
        pTexture->mOwnsImage = false;
        pTexture->mDx.pResource = (ID3D12Resource*)pDesc->pNativeHandle;
    }
    else
    {
        pTexture->mOwnsImage = true;
    }

    // add to gpu
    D3D12_RESOURCE_DESC desc = { 0 };

    DXGI_FORMAT dxFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mFormat);

    DescriptorType descriptors = pDesc->mDescriptors;

    ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

    if (NULL == pTexture->mDx.pResource)
    {
        ResourceState actualStartState = pDesc->mStartState;
        InitializeTextureDesc(pRenderer, pDesc, &desc, &actualStartState);

        hook_modify_texture_resource_flags(pDesc->mFlags, &desc.Flags);

        D3D12_CLEAR_VALUE clearValue = { 0 };
        clearValue.Format = dxFormat;
        if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        {
            clearValue.DepthStencil.Depth = pDesc->mClearValue.depth;
            clearValue.DepthStencil.Stencil = (UINT8)pDesc->mClearValue.stencil;
        }
        else
        {
            clearValue.Color[0] = pDesc->mClearValue.r;
            clearValue.Color[1] = pDesc->mClearValue.g;
            clearValue.Color[2] = pDesc->mClearValue.b;
            clearValue.Color[3] = pDesc->mClearValue.a;
        }

        D3D12_CLEAR_VALUE*    pClearValue = NULL;
        D3D12_RESOURCE_STATES res_states = util_to_dx12_resource_state(actualStartState);

        if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
        {
            pClearValue = &clearValue;
        }

        D3D12MA_ALLOCATION_DESC alloc_desc = {
            .ResourceStates = res_states,
            .HeapType = D3D12_HEAP_TYPE_DEFAULT,
            .mUseDedicatedAllocation = (pDesc->mFlags & TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT) != 0,
            .pOptimizedClearValue = pClearValue,
        };

#if defined(XBOX)
        if (pDesc->mFlags & TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET)
        {
            alloc_desc.ExtraHeapFlags |= D3D12_HEAP_FLAG_ALLOW_DISPLAY;
        }
#endif

        // Multi GPU
        if (pRenderer->mGpuMode == GPU_MODE_LINKED)
        {
            alloc_desc.CreationNodeMask = (1 << pDesc->mNodeIndex);
            alloc_desc.VisibleNodeMask = alloc_desc.CreationNodeMask;
            for (uint32_t i = 0; i < pDesc->mSharedNodeIndexCount; ++i)
                alloc_desc.VisibleNodeMask |= (1 << pDesc->pSharedNodeIndices[i]);
        }
        else
        {
            alloc_desc.CreationNodeMask = 1;
            alloc_desc.VisibleNodeMask = alloc_desc.CreationNodeMask;
        }

        // Create resource
        if (SUCCEEDED(hook_add_special_texture(pRenderer, &desc, pClearValue, res_states, pDesc->mFlags, pTexture)))
        {
            LOGF(eINFO, "Allocated memory in special platform-specific RAM");
        }
        else
        {
            if (pDesc->pPlacement)
            {
                CHECK_HRESULT(
                    hook_add_placed_resource(pRenderer, pDesc->pPlacement, &desc, pClearValue, res_states, &pTexture->mDx.pResource));
            }
            else
            {
                CHECK_HRESULT(D3D12MA_CreateResource(pRenderer->mDx.pResourceAllocator, &alloc_desc, &desc, &pTexture->mDx.pAllocation,
                                                     &pTexture->mDx.pResource));
            }
        }
    }
    else
    {
        COM_CALL(GetDesc, pTexture->mDx.pResource, &desc);
        if (pDesc->mFormat == TinyImageFormat_UNDEFINED)
        {
            LOGF(eWARNING, "Unsupported format is passed in. If requested,  views will be created in backing resource's format.");
            dxFormat = desc.Format;
        }
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC  srvDesc = { 0 };
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = { 0 };

    switch (desc.Dimension)
    {
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
    {
        if (desc.DepthOrArraySize > 1)
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
            // SRV
            srvDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
            srvDesc.Texture1DArray.FirstArraySlice = 0;
            srvDesc.Texture1DArray.MipLevels = desc.MipLevels;
            srvDesc.Texture1DArray.MostDetailedMip = 0;
            // UAV
            uavDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
            uavDesc.Texture1DArray.FirstArraySlice = 0;
            uavDesc.Texture1DArray.MipSlice = 0;
        }
        else
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            // SRV
            srvDesc.Texture1D.MipLevels = desc.MipLevels;
            srvDesc.Texture1D.MostDetailedMip = 0;
            // UAV
            uavDesc.Texture1D.MipSlice = 0;
        }
        break;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
    {
        if (DESCRIPTOR_TYPE_TEXTURE_CUBE == (descriptors & DESCRIPTOR_TYPE_TEXTURE_CUBE))
        {
            ASSERT(desc.DepthOrArraySize % 6 == 0);

            if (desc.DepthOrArraySize > 6)
            {
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                // SRV
                srvDesc.TextureCubeArray.First2DArrayFace = 0;
                srvDesc.TextureCubeArray.MipLevels = desc.MipLevels;
                srvDesc.TextureCubeArray.MostDetailedMip = 0;
                srvDesc.TextureCubeArray.NumCubes = desc.DepthOrArraySize / 6;
            }
            else
            {
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                // SRV
                srvDesc.TextureCube.MipLevels = desc.MipLevels;
                srvDesc.TextureCube.MostDetailedMip = 0;
            }

            // UAV
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
            uavDesc.Texture2DArray.FirstArraySlice = 0;
            uavDesc.Texture2DArray.MipSlice = 0;
            uavDesc.Texture2DArray.PlaneSlice = 0;
        }
        else
        {
            if (desc.DepthOrArraySize > 1)
            {
                if (desc.SampleDesc.Count > SAMPLE_COUNT_1)
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                    // Cannot create a multisampled uav
                    // SRV
                    srvDesc.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
                    srvDesc.Texture2DMSArray.FirstArraySlice = 0;
                    // No UAV
                }
                else
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                    // SRV
                    srvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                    srvDesc.Texture2DArray.FirstArraySlice = 0;
                    srvDesc.Texture2DArray.MipLevels = desc.MipLevels;
                    srvDesc.Texture2DArray.MostDetailedMip = 0;
                    srvDesc.Texture2DArray.PlaneSlice = 0;
                    // UAV
                    uavDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                    uavDesc.Texture2DArray.FirstArraySlice = 0;
                    uavDesc.Texture2DArray.MipSlice = 0;
                    uavDesc.Texture2DArray.PlaneSlice = 0;
                }
            }
            else
            {
                if (desc.SampleDesc.Count > SAMPLE_COUNT_1)
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
                    // Cannot create a multisampled uav
                }
                else
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                    // SRV
                    srvDesc.Texture2D.MipLevels = desc.MipLevels;
                    srvDesc.Texture2D.MostDetailedMip = 0;
                    srvDesc.Texture2D.PlaneSlice = 0;
                    // UAV
                    uavDesc.Texture2D.MipSlice = 0;
                    uavDesc.Texture2D.PlaneSlice = 0;
                }
            }
        }
        break;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
        // SRV
        srvDesc.Texture3D.MipLevels = desc.MipLevels;
        srvDesc.Texture3D.MostDetailedMip = 0;
        // UAV
        uavDesc.Texture3D.MipSlice = 0;
        uavDesc.Texture3D.FirstWSlice = 0;
        uavDesc.Texture3D.WSize = desc.DepthOrArraySize;
        break;
    }
    default:
        break;
    }

    DescriptorHeap* pHeap = pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    uint32_t        handleCount = (descriptors & DESCRIPTOR_TYPE_TEXTURE) ? 1 : 0;
    handleCount += (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE) ? pDesc->mMipLevels : 0;
    pTexture->mDx.mDescriptors = consume_descriptor_handles(pHeap, handleCount);

    if (descriptors & DESCRIPTOR_TYPE_TEXTURE)
    {
        ASSERT(srvDesc.ViewDimension != D3D12_SRV_DIMENSION_UNKNOWN);
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = util_to_dx12_srv_format(dxFormat, false);
        ASSERTMSG(srvDesc.Format != DXGI_FORMAT_UNKNOWN,
                  "Attempted to create a depth buffer SRV with a format that doesn't support a depth component");
        AddSrv(pRenderer, NULL, pTexture->mDx.pResource, &srvDesc, &pTexture->mDx.mDescriptors);
        ++pTexture->mDx.mUavStartIndex;

        // Create Stencil texture SRV
        bool shouldCreateStencilDesc = TinyImageFormat_HasStencil(pDesc->mFormat);
        if (shouldCreateStencilDesc)
        {
            pTexture->mDx.mStencilDescriptor = consume_descriptor_handles(pHeap, 1);
            // Stencil resides in plane 1 for all DX12 formats, meaning that we'd need to read the green channel by default.
            // However, it's easier to map the stencil component to the red channel instead, so that we don't need to
            // modify the code on other APIs
            srvDesc.Format = util_to_dx12_srv_format(dxFormat, true);
            ASSERTMSG(srvDesc.Format != DXGI_FORMAT_UNKNOWN,
                      "Attempted to create a stencil buffer SRV with a format that doesn't support a stencil component");
            srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1, 1, 1, 1);
            srvDesc.Texture2DArray.PlaneSlice = 1;
            srvDesc.Texture2D.PlaneSlice = 1;
            ++pTexture->mDx.mUavStartIndex;
            AddSrv(pRenderer, NULL, pTexture->mDx.pResource, &srvDesc, &pTexture->mDx.mStencilDescriptor);
        }
    }

    if (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
    {
        uavDesc.Format = util_to_dx12_uav_format(dxFormat);
        for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
        {
            DxDescriptorID handle = pTexture->mDx.mDescriptors + i + pTexture->mDx.mUavStartIndex;

            uavDesc.Texture1DArray.MipSlice = i;
            if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
                uavDesc.Texture3D.WSize = desc.DepthOrArraySize / (UINT)pow(2.0, (int)i);
            AddUav(pRenderer, NULL, pTexture->mDx.pResource, NULL, &uavDesc, &handle);
        }
    }

    SetObjectName((ID3D12Object*)pTexture->mDx.pResource, pDesc->pName); // -V1027

    pTexture->mNodeIndex = pDesc->mNodeIndex;
    pTexture->mDx.mHandleCount = handleCount;
    pTexture->mMipLevels = pDesc->mMipLevels;
    pTexture->mWidth = pDesc->mWidth;
    pTexture->mHeight = pDesc->mHeight;
    pTexture->mDepth = pDesc->mDepth;
    pTexture->mUav = pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE;
    pTexture->mFormat = pDesc->mFormat;
    pTexture->mArraySizeMinusOne = pDesc->mArraySize - 1;
    pTexture->mSampleCount = pDesc->mSampleCount;

    *ppTexture = pTexture;
}

void removeTexture(Renderer* pRenderer, Texture* pTexture)
{
    ASSERT(pRenderer);
    ASSERT(pTexture);

    // return texture descriptors
    if (pTexture->mDx.mDescriptors != D3D12_DESCRIPTOR_ID_NONE)
    {
        return_descriptor_handles(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], pTexture->mDx.mDescriptors,
                                  pTexture->mDx.mHandleCount);
    }

    if (pTexture->mDx.mStencilDescriptor != D3D12_DESCRIPTOR_ID_NONE)
    {
        return_descriptor_handles(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
                                  pTexture->mDx.mStencilDescriptor, 1);
    }

    if (pTexture->mOwnsImage)
    {
        D3D12MA_ReleaseAllocation(pTexture->mDx.pAllocation);
        SAFE_RELEASE(pTexture->mDx.pResource);
    }

    SAFE_FREE(pTexture);
}

void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppRenderTarget);
    ASSERT(pRenderer->mGpuMode != GPU_MODE_UNLINKED || pDesc->mNodeIndex == pRenderer->mUnlinkedRendererIndex);

    const bool isDepth = TinyImageFormat_HasDepth(pDesc->mFormat);
    ASSERT(!((isDepth) && ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE) == DESCRIPTOR_TYPE_RW_TEXTURE)) &&
           "Cannot use depth stencil as UAV");

    ((RenderTargetDesc*)pDesc)->mMipLevels = max(1U, pDesc->mMipLevels);

    RenderTarget* pRenderTarget = (RenderTarget*)tf_calloc_memalign(1, ALIGN_RenderTarget, sizeof(RenderTarget));
    ASSERT(pRenderTarget);

    // add to gpu
    DXGI_FORMAT dxFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mFormat);
    ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

    TextureDesc textureDesc = {
        .mArraySize = pDesc->mArraySize,
        .mClearValue = pDesc->mClearValue,
        .mDepth = pDesc->mDepth,
        .mFlags = pDesc->mFlags,
        .mFormat = pDesc->mFormat,
        .mWidth = pDesc->mWidth,
        .mHeight = pDesc->mHeight,
        .mMipLevels = pDesc->mMipLevels,
        .mSampleCount = pDesc->mSampleCount,
        .mSampleQuality = pDesc->mSampleQuality,
        .mStartState = pDesc->mStartState,
        .pNativeHandle = pDesc->pNativeHandle,
        .pName = pDesc->pName,
        .mNodeIndex = pDesc->mNodeIndex,
        .pSharedNodeIndices = pDesc->pSharedNodeIndices,
        .mSharedNodeIndexCount = pDesc->mSharedNodeIndexCount,
        .mDescriptors = pDesc->mDescriptors,
        .pPlacement = pDesc->pPlacement,
    };
    if (!isDepth)
        textureDesc.mStartState |= RESOURCE_STATE_RENDER_TARGET;
    else
        textureDesc.mStartState |= RESOURCE_STATE_DEPTH_WRITE;

    if (!(pDesc->mFlags & TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET))
    {
        // Create SRV by default for a render target
        textureDesc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE;
    }
    addTexture(pRenderer, &textureDesc, &pRenderTarget->pTexture);

    D3D12_RESOURCE_DESC desc = { 0 };
    COM_CALL(GetDesc, pRenderTarget->pTexture->mDx.pResource, &desc);

    uint32_t handleCount = desc.MipLevels;
    if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
        (pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
        handleCount *= desc.DepthOrArraySize;
    handleCount += 1;

    DescriptorHeap* pHeap = isDepth ? pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]
                                    : pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
    pRenderTarget->mDx.mDescriptors = consume_descriptor_handles(pHeap, handleCount);

    if (isDepth)
        AddDsv(pRenderer, NULL, pRenderTarget->pTexture->mDx.pResource, dxFormat, 0, (uint32_t)-1, &pRenderTarget->mDx.mDescriptors);
    else
        AddRtv(pRenderer, NULL, pRenderTarget->pTexture->mDx.pResource, dxFormat, 0, (uint32_t)-1, &pRenderTarget->mDx.mDescriptors);

    for (uint32_t i = 0; i < desc.MipLevels; ++i)
    {
        if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
            (pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
        {
            for (uint32_t j = 0; j < desc.DepthOrArraySize; ++j)
            {
                DxDescriptorID handle = pRenderTarget->mDx.mDescriptors + (1 + i * desc.DepthOrArraySize + j);

                if (isDepth)
                    AddDsv(pRenderer, NULL, pRenderTarget->pTexture->mDx.pResource, dxFormat, i, j, &handle);
                else
                    AddRtv(pRenderer, NULL, pRenderTarget->pTexture->mDx.pResource, dxFormat, i, j, &handle);
            }
        }
        else
        {
            DxDescriptorID handle = pRenderTarget->mDx.mDescriptors + 1 + i;

            if (isDepth)
                AddDsv(pRenderer, NULL, pRenderTarget->pTexture->mDx.pResource, dxFormat, i, (uint32_t)-1, &handle);
            else
                AddRtv(pRenderer, NULL, pRenderTarget->pTexture->mDx.pResource, dxFormat, i, (uint32_t)-1, &handle);
        }
    }

    pRenderTarget->mWidth = pDesc->mWidth;
    pRenderTarget->mHeight = pDesc->mHeight;
    pRenderTarget->mArraySize = pDesc->mArraySize;
    pRenderTarget->mDepth = pDesc->mDepth;
    pRenderTarget->mMipLevels = pDesc->mMipLevels;
    pRenderTarget->mSampleCount = pDesc->mSampleCount;
    pRenderTarget->mSampleQuality = pDesc->mSampleQuality;
    pRenderTarget->mFormat = pDesc->mFormat;
    pRenderTarget->mClearValue = pDesc->mClearValue;
    pRenderTarget->mDescriptors = pDesc->mDescriptors;

    *ppRenderTarget = pRenderTarget;
}

void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
    bool const isDepth = TinyImageFormat_HasDepth(pRenderTarget->mFormat);

    removeTexture(pRenderer, pRenderTarget->pTexture);

    const uint32_t depthOrArraySize = (uint32_t)(pRenderTarget->mArraySize * pRenderTarget->mDepth);
    uint32_t       handleCount = pRenderTarget->mMipLevels;
    if ((pRenderTarget->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
        (pRenderTarget->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
        handleCount *= depthOrArraySize;
    handleCount += 1;

    !isDepth ? return_descriptor_handles(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV],
                                         pRenderTarget->mDx.mDescriptors, handleCount)
             : return_descriptor_handles(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV],
                                         pRenderTarget->mDx.mDescriptors, handleCount);

    SAFE_FREE(pRenderTarget);
}

void addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
    ASSERT(pRenderer);
    ASSERT(pRenderer->mDx.pDevice);
    ASSERT(ppSampler);
    ASSERT(pDesc->mCompareFunc < MAX_COMPARE_MODES);

    Sampler* pSampler = (Sampler*)tf_calloc_memalign(1, ALIGN_Sampler, sizeof(Sampler));
    pSampler->mDx.mDescriptor = D3D12_DESCRIPTOR_ID_NONE;
    ASSERT(pSampler);

    // default sampler lod values
    // used if not overriden by mSetLodRange or not Linear mipmaps
    float minSamplerLod = 0;
    float maxSamplerLod = pDesc->mMipMapMode == MIPMAP_MODE_LINEAR ? D3D12_FLOAT32_MAX : 0;
    // user provided lods
    if (pDesc->mSetLodRange)
    {
        minSamplerLod = pDesc->mMinLod;
        maxSamplerLod = pDesc->mMaxLod;
    }

    D3D12_SAMPLER_DESC desc = {
        .Filter = util_to_dx12_filter(pDesc->mMinFilter, pDesc->mMagFilter, pDesc->mMipMapMode, pDesc->mMaxAnisotropy > 0.0f,
                                      (pDesc->mCompareFunc != CMP_NEVER ? true : false)),
        .AddressU = util_to_dx12_texture_address_mode(pDesc->mAddressU),
        .AddressV = util_to_dx12_texture_address_mode(pDesc->mAddressV),
        .AddressW = util_to_dx12_texture_address_mode(pDesc->mAddressW),
        .MipLODBias = pDesc->mMipLodBias,
        .MaxAnisotropy = max((UINT)pDesc->mMaxAnisotropy, 1U),
        .ComparisonFunc = gDx12ComparisonFuncTranslator[pDesc->mCompareFunc],
        .BorderColor = { 0.0f, 0.0f, 0.0f, 0.0f },
        .MinLOD = minSamplerLod,
        .MaxLOD = maxSamplerLod,
    };
    pSampler->mDx.mDesc = desc;
    AddSampler(pRenderer, NULL, &pSampler->mDx.mDesc, &pSampler->mDx.mDescriptor);

    *ppSampler = pSampler;
}

void removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
    ASSERT(pRenderer);
    ASSERT(pSampler);

    // Nop op
    return_descriptor_handles(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER], pSampler->mDx.mDescriptor, 1);

    SAFE_FREE(pSampler);
}
/************************************************************************/
// Shader Functions
/************************************************************************/
void addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
    ASSERT(pRenderer);
    ASSERT(pDesc && pDesc->mStages);
    ASSERT(ppShaderProgram);

    Shader* pShaderProgram = (Shader*)tf_calloc(1, sizeof(Shader));
    ASSERT(pShaderProgram);
    pShaderProgram->mStages = pDesc->mStages;
    for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
    {
        ShaderStage                  stage_mask = (ShaderStage)(1 << i);
        const BinaryShaderStageDesc* pStage = NULL;
        if (stage_mask == (pShaderProgram->mStages & stage_mask))
        {
            struct IDxcBlobEncoding** ppBlob = NULL;

            switch (stage_mask)
            {
            case SHADER_STAGE_VERT:
                pStage = &pDesc->mVert;
                ppBlob = &pShaderProgram->mDx.pVSBlob;
                break;
            case SHADER_STAGE_HULL:
                pStage = &pDesc->mHull;
                ppBlob = &pShaderProgram->mDx.pHSBlob;
                break;
            case SHADER_STAGE_DOMN:
                pStage = &pDesc->mDomain;
                ppBlob = &pShaderProgram->mDx.pDSBlob;
                break;
            case SHADER_STAGE_GEOM:
                pStage = &pDesc->mGeom;
                ppBlob = &pShaderProgram->mDx.pGSBlob;
                break;
            case SHADER_STAGE_FRAG:
                pStage = &pDesc->mFrag;
                ppBlob = &pShaderProgram->mDx.pPSBlob;
                break;
            case SHADER_STAGE_COMP:
#if defined(ENABLE_WORKGRAPH)
            case SHADER_STAGE_WORKGRAPH:
#endif
                pStage = &pDesc->mComp;
                ppBlob = &pShaderProgram->mDx.pCSBlob;
                break;
            default:
                LOGF(eERROR, "Unknown shader stage %i", stage_mask);
                continue;
            }

            IDxcUtils_CreateBlob(pStage->pByteCode, pStage->mByteCodeSize, ppBlob);
        }
    }

    *ppShaderProgram = pShaderProgram;
}

void removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
    UNREF_PARAM(pRenderer);

    // remove given shader
    if (pShaderProgram->mDx.pVSBlob)
    {
        IDxcBlobEncoding_Release(pShaderProgram->mDx.pVSBlob);
    }
    if (pShaderProgram->mDx.pHSBlob)
    {
        IDxcBlobEncoding_Release(pShaderProgram->mDx.pHSBlob);
    }
    if (pShaderProgram->mDx.pDSBlob)
    {
        IDxcBlobEncoding_Release(pShaderProgram->mDx.pDSBlob);
    }
    if (pShaderProgram->mDx.pGSBlob)
    {
        IDxcBlobEncoding_Release(pShaderProgram->mDx.pGSBlob);
    }
    if (pShaderProgram->mDx.pPSBlob)
    {
        IDxcBlobEncoding_Release(pShaderProgram->mDx.pPSBlob);
    }
    if (pShaderProgram->mDx.pCSBlob)
    {
        IDxcBlobEncoding_Release(pShaderProgram->mDx.pCSBlob);
    }

    SAFE_FREE(pShaderProgram);
}

/************************************************************************/
// Root Signature Functions
/************************************************************************/
void initRootSignatureImpl(Renderer* pRenderer, const void* pData, uint32_t dataSize, ID3D12RootSignature** ppOutRootSignature)
{
    ASSERT(pRenderer);
    ASSERT(pData);
    ASSERT(dataSize);
    ASSERT(ppOutRootSignature);

    const uint32_t nodeMask = util_calculate_shared_node_mask(pRenderer);
    CHECK_HRESULT(COM_CALL(CreateRootSignature, pRenderer->mDx.pDevice, nodeMask, pData, dataSize,
                           IID_ARGS(ID3D12RootSignature, ppOutRootSignature)));
}

void exitRootSignatureImpl(Renderer* pRenderer, ID3D12RootSignature* pRootSignature)
{
    ASSERT(pRenderer);
    ASSERT(pRootSignature);
    SAFE_RELEASE(pRootSignature);
}
/************************************************************************/
// Descriptor Set Functions
/************************************************************************/

void addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppDescriptorSet);

    DescriptorSet* pDescriptorSet = (DescriptorSet*)tf_calloc_memalign(1, ALIGN_DescriptorSet, sizeof(DescriptorSet));
    ASSERT(pDescriptorSet);

    const Descriptor* highestViewDesc = NULL;
    const Descriptor* highestSamplerDesc = NULL;
    for (uint32_t descIndex = 0; descIndex < pDesc->mDescriptorCount; ++descIndex)
    {
        const Descriptor* desc = &pDesc->pDescriptors[descIndex];
        if (DESCRIPTOR_TYPE_SAMPLER == desc->mType)
        {
            if (!highestSamplerDesc)
            {
                highestSamplerDesc = desc;
            }
            if (highestSamplerDesc->mOffset < desc->mOffset)
            {
                highestSamplerDesc = desc;
            }
        }
        else
        {
            if (!highestViewDesc)
            {
                highestViewDesc = desc;
            }
            if (highestViewDesc->mOffset < desc->mOffset)
            {
                highestViewDesc = desc;
            }
        }
    }

    const uint32_t rootParamIndex = pDesc->mIndex;
    const uint32_t nodeIndex = pRenderer->mGpuMode == GPU_MODE_LINKED ? pDesc->mNodeIndex : 0;

    pDescriptorSet->mDx.mCbvSrvUavRootIndex = rootParamIndex;
    pDescriptorSet->mDx.mSamplerRootIndex = highestViewDesc ? rootParamIndex + 1 : rootParamIndex;
    pDescriptorSet->mDx.mNodeIndex = nodeIndex;
    pDescriptorSet->mDx.pDescriptors = pDesc->pDescriptors;
    pDescriptorSet->mDx.mMaxSets = pDesc->mMaxSets;
    pDescriptorSet->mDx.mCbvSrvUavHandle = D3D12_DESCRIPTOR_ID_NONE;
    pDescriptorSet->mDx.mSamplerHandle = D3D12_DESCRIPTOR_ID_NONE;

    if (highestViewDesc || highestSamplerDesc)
    {
        if (highestViewDesc)
        {
            DescriptorHeap* pSrcHeap = pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
            DescriptorHeap* pHeap = pRenderer->mDx.pCbvSrvUavHeaps[nodeIndex];
            // #NOTE: Descriptor count in the table is the highest register + the count of the resource with highest descriptor
            const uint32_t  viewDescCount = highestViewDesc->mOffset + highestViewDesc->mCount;
            pDescriptorSet->mDx.mCbvSrvUavHandle = consume_descriptor_handles(pHeap, viewDescCount * pDesc->mMaxSets);
            pDescriptorSet->mDx.mCbvSrvUavStride = viewDescCount;

            for (uint32_t i = 0; i < pDesc->mDescriptorCount; ++i)
            {
                const Descriptor* pDescInfo = &pDesc->pDescriptors[i];
                {
                    DescriptorType type = (DescriptorType)pDescInfo->mType;
                    DxDescriptorID srcHandle = D3D12_DESCRIPTOR_ID_NONE;
                    switch (type)
                    {
                    case DESCRIPTOR_TYPE_TEXTURE:
                        // #TODO: desc->mDim?
                        srcHandle = pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_2D];
                        break;
                    case DESCRIPTOR_TYPE_BUFFER:
                        srcHandle = pRenderer->pNullDescriptors->mNullBufferSRV;
                        break;
                    case DESCRIPTOR_TYPE_RW_TEXTURE:
                        // #TODO: desc->mDim?
                        srcHandle = pRenderer->pNullDescriptors->mNullTextureUAV[TEXTURE_DIM_2D];
                        break;
                    case DESCRIPTOR_TYPE_RW_BUFFER:
                        srcHandle = pRenderer->pNullDescriptors->mNullBufferUAV;
                        break;
                    case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                        srcHandle = pRenderer->pNullDescriptors->mNullBufferCBV;
                        break;
                    default:
                        break;
                    }

                    if (srcHandle != D3D12_DESCRIPTOR_ID_NONE)
                    {
                        for (uint32_t s = 0; s < pDesc->mMaxSets; ++s)
                            for (uint32_t j = 0; j < pDescInfo->mCount; ++j)
                                copy_descriptor_handle(pSrcHeap, srcHandle, pHeap,
                                                       pDescriptorSet->mDx.mCbvSrvUavHandle + s * pDescriptorSet->mDx.mCbvSrvUavStride +
                                                           pDescInfo->mOffset + j);
                    }
                }
            }
        }
        if (highestSamplerDesc)
        {
            DescriptorHeap* pSrcHeap = pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
            DescriptorHeap* pHeap = pRenderer->mDx.pSamplerHeaps[nodeIndex];
            const uint32_t  samplerDescCount = highestSamplerDesc->mOffset + highestSamplerDesc->mCount;
            pDescriptorSet->mDx.mSamplerHandle = consume_descriptor_handles(pHeap, samplerDescCount * pDesc->mMaxSets);
            pDescriptorSet->mDx.mSamplerStride = samplerDescCount;
            for (uint32_t i = 0; i < pDesc->mMaxSets; ++i)
            {
                for (uint32_t j = 0; j < samplerDescCount; ++j)
                    copy_descriptor_handle(pSrcHeap, pRenderer->pNullDescriptors->mNullSampler, pHeap,
                                           pDescriptorSet->mDx.mSamplerHandle + i * pDescriptorSet->mDx.mSamplerStride + j);
            }
        }
    }

    *ppDescriptorSet = pDescriptorSet;
}

void removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
{
    ASSERT(pRenderer);
    ASSERT(pDescriptorSet);

    if (pDescriptorSet->mDx.mCbvSrvUavHandle != D3D12_DESCRIPTOR_ID_NONE)
    {
        return_descriptor_handles(pRenderer->mDx.pCbvSrvUavHeaps[pDescriptorSet->mDx.mNodeIndex], pDescriptorSet->mDx.mCbvSrvUavHandle,
                                  pDescriptorSet->mDx.mCbvSrvUavStride * pDescriptorSet->mDx.mMaxSets);
    }

    if (pDescriptorSet->mDx.mSamplerHandle != D3D12_DESCRIPTOR_ID_NONE)
    {
        return_descriptor_handles(pRenderer->mDx.pSamplerHeaps[pDescriptorSet->mDx.mNodeIndex], pDescriptorSet->mDx.mSamplerHandle,
                                  pDescriptorSet->mDx.mSamplerStride * pDescriptorSet->mDx.mMaxSets);
    }

    pDescriptorSet->mDx.mCbvSrvUavHandle = D3D12_DESCRIPTOR_ID_NONE;
    pDescriptorSet->mDx.mSamplerHandle = D3D12_DESCRIPTOR_ID_NONE;

    SAFE_FREE(pDescriptorSet);
}

void updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
    ASSERT(pRenderer);
    ASSERT(pDescriptorSet);
    ASSERT(index < pDescriptorSet->mDx.mMaxSets);
    const uint32_t nodeIndex = pDescriptorSet->mDx.mNodeIndex;

    for (uint32_t i = 0; i < count; ++i)
    {
        const DescriptorData* pParam = pParams + i;
        const Descriptor*     pDesc = (pDescriptorSet->mDx.pDescriptors + pParam->mIndex);
        const DescriptorType  type = (DescriptorType)pDesc->mType; //-V522
        const uint32_t        arrayStart = pParam->mArrayOffset;
        const uint32_t        arrayCount = max(1U, pParam->mCount);

        IF_VALIDATE_DESCRIPTOR(const uint32_t rootParamIndex = DESCRIPTOR_TYPE_SAMPLER == type ? pDescriptorSet->mDx.mSamplerRootIndex
                                                                                               : pDescriptorSet->mDx.mCbvSrvUavRootIndex);
        VALIDATE_DESCRIPTOR(pDesc->mSetIndex == rootParamIndex, "Descriptor (%s) - Mismatching descriptor set index (%u/%u)", pDesc->pName,
                            pDesc->mSetIndex, rootParamIndex);

        switch (type)
        {
        case DESCRIPTOR_TYPE_SAMPLER:
        {
            VALIDATE_DESCRIPTOR(pParam->ppSamplers, "NULL Sampler (%s)", pDesc->pName);

            for (uint32_t arr = 0; arr < arrayCount; ++arr)
            {
                VALIDATE_DESCRIPTOR(pParam->ppSamplers[arr], "NULL Sampler (%s [%u] )", pDesc->pName, arr);

                copy_descriptor_handle(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER],
                                       pParam->ppSamplers[arr]->mDx.mDescriptor, pRenderer->mDx.pSamplerHeaps[nodeIndex],
                                       pDescriptorSet->mDx.mSamplerHandle + index * pDescriptorSet->mDx.mSamplerStride + pDesc->mOffset +
                                           arrayStart + arr);
            }
            break;
        }
        case DESCRIPTOR_TYPE_TEXTURE:
        {
            VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)", pDesc->pName);

            for (uint32_t arr = 0; arr < arrayCount; ++arr)
            {
                VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);
                DxDescriptorID targetDescriptor = pParam->mBindStencilResource ? pParam->ppTextures[arr]->mDx.mStencilDescriptor
                                                                               : pParam->ppTextures[arr]->mDx.mDescriptors;
                copy_descriptor_handle(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], targetDescriptor,
                                       pRenderer->mDx.pCbvSrvUavHeaps[nodeIndex],
                                       pDescriptorSet->mDx.mCbvSrvUavHandle + index * pDescriptorSet->mDx.mCbvSrvUavStride +
                                           pDesc->mOffset + arrayStart + arr);
            }
            break;
        }
        case DESCRIPTOR_TYPE_RW_TEXTURE:
        {
            VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL RW Texture (%s)", pDesc->pName);

            if (pParam->mBindMipChain)
            {
                VALIDATE_DESCRIPTOR(pParam->ppTextures[0], "NULL RW Texture (%s)", pDesc->pName);
                for (uint32_t arr = 0; arr < pParam->ppTextures[0]->mMipLevels; ++arr)
                {
                    DxDescriptorID srcId = pParam->ppTextures[0]->mDx.mDescriptors + arr + pParam->ppTextures[0]->mDx.mUavStartIndex;

                    copy_descriptor_handle(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
                                           pRenderer->mDx.pCbvSrvUavHeaps[nodeIndex],
                                           pDescriptorSet->mDx.mCbvSrvUavHandle + index * pDescriptorSet->mDx.mCbvSrvUavStride +
                                               pDesc->mOffset + arrayStart + arr);
                }
            }
            else
            {
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL RW Texture (%s [%u] )", pDesc->pName, arr);

                    DxDescriptorID srcId =
                        pParam->ppTextures[arr]->mDx.mDescriptors + pParam->mUAVMipSlice + pParam->ppTextures[arr]->mDx.mUavStartIndex;

                    copy_descriptor_handle(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
                                           pRenderer->mDx.pCbvSrvUavHeaps[nodeIndex],
                                           pDescriptorSet->mDx.mCbvSrvUavHandle + index * pDescriptorSet->mDx.mCbvSrvUavStride +
                                               pDesc->mOffset + arrayStart + arr);
                }
            }
            break;
        }
        case DESCRIPTOR_TYPE_BUFFER:
        case DESCRIPTOR_TYPE_BUFFER_RAW:
        {
            VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Buffer (%s)", pDesc->pName);

            if (pParam->pRanges)
            {
                const bool raw = DESCRIPTOR_TYPE_BUFFER_RAW == type;
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    DescriptorDataRange range = pParam->pRanges[arr];
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Buffer (%s [%u] )", pDesc->pName, arr);
                    VALIDATE_DESCRIPTOR(range.mSize > 0, "Descriptor (%s) - pRanges[%u].mSize is zero", pDesc->pName, arr);
                    if (!raw)
                    {
                        VALIDATE_DESCRIPTOR(range.mStructStride > 0, "Descriptor (%s) - pRanges[%u].mStructStride is zero", pDesc->pName,
                                            arr);
                    }
                    const uint32_t setStart = index * pDescriptorSet->mDx.mCbvSrvUavStride;
                    const uint32_t stride = raw ? sizeof(uint32_t) : range.mStructStride;
                    DxDescriptorID srv = pDescriptorSet->mDx.mCbvSrvUavHandle + setStart + (pDesc->mOffset + arrayStart + arr);
                    AddBufferSrv(pRenderer, pRenderer->mDx.pCbvSrvUavHeaps[nodeIndex], pParam->ppBuffers[arr]->mDx.pResource, raw,
                                 range.mOffset / stride, range.mSize / stride, stride, &srv);
                }
            }
            else
            {
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Buffer (%s [%u] )", pDesc->pName, arr);

                    DxDescriptorID srcId = pParam->ppBuffers[arr]->mDx.mDescriptors + pParam->ppBuffers[arr]->mDx.mSrvDescriptorOffset;

                    copy_descriptor_handle(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
                                           pRenderer->mDx.pCbvSrvUavHeaps[nodeIndex],
                                           pDescriptorSet->mDx.mCbvSrvUavHandle + index * pDescriptorSet->mDx.mCbvSrvUavStride +
                                               pDesc->mOffset + arrayStart + arr);
                }
            }
            break;
        }
        case DESCRIPTOR_TYPE_RW_BUFFER:
        case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
        {
            VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL RW Buffer (%s)", pDesc->pName);

            if (pParam->pRanges)
            {
                const bool raw = DESCRIPTOR_TYPE_RW_BUFFER_RAW == type;
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    DescriptorDataRange range = pParam->pRanges[arr];
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL RW Buffer (%s [%u] )", pDesc->pName, arr);
                    VALIDATE_DESCRIPTOR(range.mSize > 0, "Descriptor (%s) - pRanges[%u].mSize is zero", pDesc->pName, arr);
                    if (!raw)
                    {
                        VALIDATE_DESCRIPTOR(range.mStructStride > 0, "Descriptor (%s) - pRanges[%u].mStructStride is zero", pDesc->pName,
                                            arr);
                    }
                    const uint32_t setStart = index * pDescriptorSet->mDx.mCbvSrvUavStride;
                    const uint32_t stride = raw ? sizeof(uint32_t) : range.mStructStride;
                    DxDescriptorID uav = pDescriptorSet->mDx.mCbvSrvUavHandle + setStart + (pDesc->mOffset + arrayStart + arr);
                    AddBufferUav(pRenderer, pRenderer->mDx.pCbvSrvUavHeaps[nodeIndex], pParam->ppBuffers[arr]->mDx.pResource, NULL, 0, raw,
                                 range.mOffset / stride, range.mSize / stride, stride, &uav);
                }
            }
            else
            {
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL RW Buffer (%s [%u] )", pDesc->pName, arr);

                    DxDescriptorID srcId = pParam->ppBuffers[arr]->mDx.mDescriptors + pParam->ppBuffers[arr]->mDx.mUavDescriptorOffset;

                    copy_descriptor_handle(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
                                           pRenderer->mDx.pCbvSrvUavHeaps[nodeIndex],
                                           pDescriptorSet->mDx.mCbvSrvUavHandle + index * pDescriptorSet->mDx.mCbvSrvUavStride +
                                               pDesc->mOffset + arrayStart + arr);
                }
            }
            break;
        }
        case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        {
            VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Uniform Buffer (%s)", pDesc->pName);

            if (pParam->pRanges)
            {
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    DescriptorDataRange range = pParam->pRanges[arr];
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Uniform Buffer (%s [%u] )", pDesc->pName, arr);
                    VALIDATE_DESCRIPTOR(range.mSize > 0, "Descriptor (%s) - pRanges[%u].mSize is zero", pDesc->pName, arr);
                    VALIDATE_DESCRIPTOR(range.mSize <= D3D12_REQ_CONSTANT_BUFFER_SIZE,
                                        "Descriptor (%s) - pRanges[%u].mSize is %u which exceeds max size %u", pDesc->pName, arr,
                                        range.mSize, D3D12_REQ_CONSTANT_BUFFER_SIZE);

                    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { 0 };
                    cbvDesc.BufferLocation = pParam->ppBuffers[arr]->mDx.mGpuAddress + range.mOffset;
                    cbvDesc.SizeInBytes = round_up(range.mSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
                    uint32_t       setStart = index * pDescriptorSet->mDx.mCbvSrvUavStride;
                    DxDescriptorID cbv = pDescriptorSet->mDx.mCbvSrvUavHandle + setStart + (pDesc->mOffset + arrayStart + arr);
                    AddCbv(pRenderer, pRenderer->mDx.pCbvSrvUavHeaps[nodeIndex], &cbvDesc, &cbv);
                }
            }
            else
            {
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Uniform Buffer (%s [%u] )", pDesc->pName, arr);
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr]->mSize <= D3D12_REQ_CONSTANT_BUFFER_SIZE,
                                        "Descriptor (%s) - pParam->ppBuffers[%u]->mSize is %llu which exceeds max size %u", pDesc->pName,
                                        arr, pParam->ppBuffers[arr]->mSize, D3D12_REQ_CONSTANT_BUFFER_SIZE);

                    copy_descriptor_handle(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
                                           pParam->ppBuffers[arr]->mDx.mDescriptors, pRenderer->mDx.pCbvSrvUavHeaps[nodeIndex],
                                           pDescriptorSet->mDx.mCbvSrvUavHandle + index * pDescriptorSet->mDx.mCbvSrvUavStride +
                                               pDesc->mOffset + arrayStart + arr);
                }
            }
            break;
        }
#ifdef D3D12_RAYTRACING_AVAILABLE
        case DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE:
        {
            VALIDATE_DESCRIPTOR(pParam->ppAccelerationStructures, "NULL Acceleration Structure (%s)", pDesc->pName);

            for (uint32_t arr = 0; arr < arrayCount; ++arr)
            {
                VALIDATE_DESCRIPTOR(pParam->ppAccelerationStructures[arr], "Acceleration Structure (%s [%u] )", pDesc->pName, arr);

                DxDescriptorID handle = D3D12_DESCRIPTOR_ID_NONE;
                fillRaytracingDescriptorHandle(pParam->ppAccelerationStructures[arr], &handle);

                VALIDATE_DESCRIPTOR(handle != D3D12_DESCRIPTOR_ID_NONE, "Invalid Acceleration Structure (%s [%u] )", pDesc->pName, arr);

                copy_descriptor_handle(pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], handle,
                                       pRenderer->mDx.pCbvSrvUavHeaps[nodeIndex],
                                       pDescriptorSet->mDx.mCbvSrvUavHandle + index * pDescriptorSet->mDx.mCbvSrvUavStride +
                                           pDesc->mOffset + arrayStart + arr);
            }
            break;
        }
#endif
        default:
            break;
        }
    }
}

void cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
{
    ASSERT(pCmd);
    ASSERT(pDescriptorSet);
    ASSERT(index < pDescriptorSet->mDx.mMaxSets);

    // Bind the descriptor tables associated with this DescriptorSet
    const uint32_t pipeline = pCmd->mDx.mPipelineType == PIPELINE_TYPE_GRAPHICS ? 0 : 1;

    if (pDescriptorSet->mDx.mCbvSrvUavHandle != D3D12_DESCRIPTOR_ID_NONE)
    {
        const D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptor_id_to_gpu_handle(
            pCmd->mDx.pBoundHeaps[0], pDescriptorSet->mDx.mCbvSrvUavHandle + index * pDescriptorSet->mDx.mCbvSrvUavStride);
        if (handle.ptr != pCmd->mDx.mBoundDescriptorSets[pipeline][pDescriptorSet->mDx.mCbvSrvUavRootIndex].ptr)
        {
            if (0 == pipeline)
            {
                hook_SetGraphicsRootDescriptorTable(pCmd->mDx.pCmdList, pDescriptorSet->mDx.mCbvSrvUavRootIndex, handle);
            }
            else
            {
                hook_SetComputeRootDescriptorTable(pCmd->mDx.pCmdList, pDescriptorSet->mDx.mCbvSrvUavRootIndex, handle);
            }
            pCmd->mDx.mBoundDescriptorSets[pipeline][pDescriptorSet->mDx.mCbvSrvUavRootIndex] = handle;
        }
    }
    if (pDescriptorSet->mDx.mSamplerHandle != D3D12_DESCRIPTOR_ID_NONE)
    {
        const D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptor_id_to_gpu_handle(
            pCmd->mDx.pBoundHeaps[1], pDescriptorSet->mDx.mSamplerHandle + index * pDescriptorSet->mDx.mSamplerStride);
        if (handle.ptr != pCmd->mDx.mBoundDescriptorSets[pipeline][pDescriptorSet->mDx.mSamplerRootIndex].ptr)
        {
            if (0 == pipeline)
            {
                hook_SetGraphicsRootDescriptorTable(pCmd->mDx.pCmdList, pDescriptorSet->mDx.mSamplerRootIndex, handle);
            }
            else
            {
                hook_SetComputeRootDescriptorTable(pCmd->mDx.pCmdList, pDescriptorSet->mDx.mSamplerRootIndex, handle);
            }
            pCmd->mDx.mBoundDescriptorSets[pipeline][pDescriptorSet->mDx.mSamplerRootIndex] = handle;
        }

        if (0 == pipeline)
        {
            hook_SetGraphicsRootDescriptorTable(
                pCmd->mDx.pCmdList, pDescriptorSet->mDx.mSamplerRootIndex,
                descriptor_id_to_gpu_handle(pCmd->mDx.pBoundHeaps[1],
                                            pDescriptorSet->mDx.mSamplerHandle + index * pDescriptorSet->mDx.mSamplerStride));
        }
    }
}
/************************************************************************/
// Pipeline State Functions
/************************************************************************/
static void addGraphicsPipeline(Renderer* pRenderer, const PipelineDesc* pMainDesc, Pipeline** ppPipeline)
{
    ASSERT(pRenderer);
    ASSERT(ppPipeline);
    ASSERT(pMainDesc);

    const GraphicsPipelineDesc* pDesc = &pMainDesc->mGraphicsDesc;

    ASSERT(pDesc->pShaderProgram);
    ASSERT(pRenderer->mDx.pGraphicsRootSignature);

    Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, ALIGN_Pipeline, sizeof(Pipeline));
    ASSERT(pPipeline);

    const Shader*       pShaderProgram = pDesc->pShaderProgram;
    const VertexLayout* pVertexLayout = pDesc->pVertexLayout;

#ifndef DISABLE_PIPELINE_LIBRARY
    ID3D12PipelineLibrary* psoCache = pMainDesc->pCache ? pMainDesc->pCache->mDx.pLibrary : NULL;

    size_t psoShaderHash = 0;
    size_t psoRenderHash = 0;
#endif

    pPipeline->mDx.mType = PIPELINE_TYPE_GRAPHICS;

    // add to gpu
    D3D12_SHADER_BYTECODE VS = { 0 };
    D3D12_SHADER_BYTECODE PS = { 0 };
    D3D12_SHADER_BYTECODE DS = { 0 };
    D3D12_SHADER_BYTECODE HS = { 0 };
    D3D12_SHADER_BYTECODE GS = { 0 };
    if (pShaderProgram->mStages & SHADER_STAGE_VERT)
    {
        VS.BytecodeLength = IDxcBlobEncoding_GetBufferSize(pShaderProgram->mDx.pVSBlob);
        VS.pShaderBytecode = IDxcBlobEncoding_GetBufferPointer(pShaderProgram->mDx.pVSBlob);
    }
    if (pShaderProgram->mStages & SHADER_STAGE_FRAG)
    {
        PS.BytecodeLength = IDxcBlobEncoding_GetBufferSize(pShaderProgram->mDx.pPSBlob);
        PS.pShaderBytecode = IDxcBlobEncoding_GetBufferPointer(pShaderProgram->mDx.pPSBlob);
    }
    if (pShaderProgram->mStages & SHADER_STAGE_HULL)
    {
        HS.BytecodeLength = IDxcBlobEncoding_GetBufferSize(pShaderProgram->mDx.pHSBlob);
        HS.pShaderBytecode = IDxcBlobEncoding_GetBufferPointer(pShaderProgram->mDx.pHSBlob);
    }
    if (pShaderProgram->mStages & SHADER_STAGE_DOMN)
    {
        DS.BytecodeLength = IDxcBlobEncoding_GetBufferSize(pShaderProgram->mDx.pDSBlob);
        DS.pShaderBytecode = IDxcBlobEncoding_GetBufferPointer(pShaderProgram->mDx.pDSBlob);
    }
    if (pShaderProgram->mStages & SHADER_STAGE_GEOM)
    {
        GS.BytecodeLength = IDxcBlobEncoding_GetBufferSize(pShaderProgram->mDx.pGSBlob);
        GS.pShaderBytecode = IDxcBlobEncoding_GetBufferPointer(pShaderProgram->mDx.pGSBlob);
    }

    D3D12_STREAM_OUTPUT_DESC stream_output_desc = {
        .pSODeclaration = NULL,
        .NumEntries = 0,
        .pBufferStrides = NULL,
        .NumStrides = 0,
        .RasterizedStream = 0,
    };
    uint32_t                 input_elementCount = 0;
    D3D12_INPUT_ELEMENT_DESC input_elements[MAX_VERTEX_ATTRIBS] = { 0 };

    char semantic_names[MAX_VERTEX_ATTRIBS][MAX_SEMANTIC_NAME_LENGTH] = { 0 };
    // Make sure there's attributes
    if (pVertexLayout != NULL)
    {
        ASSERT(pVertexLayout->mAttribCount && pVertexLayout->mBindingCount);

        for (uint32_t attrib_index = 0; attrib_index < pVertexLayout->mAttribCount; ++attrib_index)
        {
            const VertexAttrib* attrib = &(pVertexLayout->mAttribs[attrib_index]);

            ASSERT(SEMANTIC_UNDEFINED != attrib->mSemantic);
            ASSERT(attrib->mBinding < pVertexLayout->mBindingCount);

            if (attrib->mSemanticNameLength > 0)
            {
                uint32_t name_length = min((uint32_t)MAX_SEMANTIC_NAME_LENGTH, attrib->mSemanticNameLength);
                strncpy_s(semantic_names[attrib_index], MAX_SEMANTIC_NAME_LENGTH, attrib->mSemanticName, name_length);
            }
            else
            {
                switch (attrib->mSemantic)
                {
                case SEMANTIC_POSITION:
                    strcpy_s(semantic_names[attrib_index], MAX_SEMANTIC_NAME_LENGTH, "POSITION");
                    break;
                case SEMANTIC_NORMAL:
                    strcpy_s(semantic_names[attrib_index], MAX_SEMANTIC_NAME_LENGTH, "NORMAL");
                    break;
                case SEMANTIC_COLOR:
                    strcpy_s(semantic_names[attrib_index], MAX_SEMANTIC_NAME_LENGTH, "COLOR");
                    break;
                case SEMANTIC_TANGENT:
                    strcpy_s(semantic_names[attrib_index], MAX_SEMANTIC_NAME_LENGTH, "TANGENT");
                    break;
                case SEMANTIC_BITANGENT:
                    strcpy_s(semantic_names[attrib_index], MAX_SEMANTIC_NAME_LENGTH, "BITANGENT");
                    break;
                case SEMANTIC_JOINTS:
                    strcpy_s(semantic_names[attrib_index], MAX_SEMANTIC_NAME_LENGTH, "JOINTS");
                    break;
                case SEMANTIC_WEIGHTS:
                    strcpy_s(semantic_names[attrib_index], MAX_SEMANTIC_NAME_LENGTH, "WEIGHTS");
                    break;
                case SEMANTIC_CUSTOM:
                    strcpy_s(semantic_names[attrib_index], MAX_SEMANTIC_NAME_LENGTH, "CUSTOM");
                    break;
                case SEMANTIC_TEXCOORD0:
                case SEMANTIC_TEXCOORD1:
                case SEMANTIC_TEXCOORD2:
                case SEMANTIC_TEXCOORD3:
                case SEMANTIC_TEXCOORD4:
                case SEMANTIC_TEXCOORD5:
                case SEMANTIC_TEXCOORD6:
                case SEMANTIC_TEXCOORD7:
                case SEMANTIC_TEXCOORD8:
                case SEMANTIC_TEXCOORD9:
                    strcpy_s(semantic_names[attrib_index], MAX_SEMANTIC_NAME_LENGTH, "TEXCOORD");
                    break;
                default:
                    ASSERT(false);
                    break;
                }
            }

            UINT semantic_index = 0;
            switch (attrib->mSemantic)
            {
            case SEMANTIC_TEXCOORD0:
                semantic_index = 0;
                break;
            case SEMANTIC_TEXCOORD1:
                semantic_index = 1;
                break;
            case SEMANTIC_TEXCOORD2:
                semantic_index = 2;
                break;
            case SEMANTIC_TEXCOORD3:
                semantic_index = 3;
                break;
            case SEMANTIC_TEXCOORD4:
                semantic_index = 4;
                break;
            case SEMANTIC_TEXCOORD5:
                semantic_index = 5;
                break;
            case SEMANTIC_TEXCOORD6:
                semantic_index = 6;
                break;
            case SEMANTIC_TEXCOORD7:
                semantic_index = 7;
                break;
            case SEMANTIC_TEXCOORD8:
                semantic_index = 8;
                break;
            case SEMANTIC_TEXCOORD9:
                semantic_index = 9;
                break;
            default:
                break;
            }

            input_elements[input_elementCount].SemanticName = semantic_names[attrib_index];
            input_elements[input_elementCount].SemanticIndex = semantic_index;

            input_elements[input_elementCount].Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(attrib->mFormat);
            input_elements[input_elementCount].InputSlot = attrib->mBinding;
            input_elements[input_elementCount].AlignedByteOffset = attrib->mOffset;
            if (pVertexLayout->mBindings[attrib->mBinding].mRate == VERTEX_BINDING_RATE_INSTANCE)
            {
                input_elements[input_elementCount].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
                input_elements[input_elementCount].InstanceDataStepRate = 1;
            }
            else
            {
                input_elements[input_elementCount].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                input_elements[input_elementCount].InstanceDataStepRate = 0;
            }

#ifndef DISABLE_PIPELINE_LIBRARY
            if (psoCache)
            {
                psoRenderHash = tf_mem_hash_uint8_t((uint8_t*)&attrib->mSemantic, sizeof(ShaderSemantic), psoRenderHash);
                psoRenderHash = tf_mem_hash_uint8_t((uint8_t*)&attrib->mFormat, sizeof(TinyImageFormat), psoRenderHash);
                psoRenderHash = tf_mem_hash_uint8_t((uint8_t*)&attrib->mBinding, sizeof(uint32_t), psoRenderHash);
                psoRenderHash = tf_mem_hash_uint8_t((uint8_t*)&attrib->mLocation, sizeof(uint32_t), psoRenderHash);
                psoRenderHash = tf_mem_hash_uint8_t((uint8_t*)&attrib->mOffset, sizeof(uint32_t), psoRenderHash);
                psoRenderHash = tf_mem_hash_uint8_t((uint8_t*)&pVertexLayout->mBindings[attrib->mBinding].mRate, sizeof(VertexBindingRate),
                                                    psoRenderHash);
            }
#endif

            ++input_elementCount;
        }
    }

    D3D12_INPUT_LAYOUT_DESC input_layout_desc = {
        .pInputElementDescs = input_elementCount ? input_elements : NULL,
        .NumElements = input_elementCount,
    };

    uint32_t render_target_count = min(pDesc->mRenderTargetCount, (uint32_t)MAX_RENDER_TARGET_ATTACHMENTS);
    render_target_count = min(render_target_count, (uint32_t)D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

    DXGI_SAMPLE_DESC sample_desc = {
        .Count = (UINT)(pDesc->mSampleCount),
        .Quality = (UINT)(pDesc->mSampleQuality),
    };

    D3D12_CACHED_PIPELINE_STATE        cached_pso_desc = { 0 };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc = {
        .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
        .pRootSignature = pRenderer->mDx.pGraphicsRootSignature,
        .VS = VS,
        .PS = PS,
        .DS = DS,
        .HS = HS,
        .GS = GS,
        .StreamOutput = stream_output_desc,
        .SampleMask = UINT_MAX,
        .InputLayout = input_layout_desc,
        .BlendState = pDesc->pBlendState ? util_to_blend_desc(pDesc->pBlendState) : gDefaultBlendDesc,
        .RasterizerState = pDesc->pRasterizerState ? util_to_rasterizer_desc(pDesc->pRasterizerState) : gDefaultRasterizerDesc,
        .DepthStencilState = pDesc->pDepthState ? util_to_depth_desc(pDesc->pDepthState) : gDefaultDepthDesc,
        .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
        .PrimitiveTopologyType = util_to_dx12_primitive_topology_type(pDesc->mPrimitiveTopo),
        .DSVFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mDepthStencilFormat),
        .NumRenderTargets = render_target_count,
        .SampleDesc = sample_desc,
        .CachedPSO = cached_pso_desc,
    };

    for (uint32_t attrib_index = 0; attrib_index < render_target_count; ++attrib_index)
    {
        pipeline_state_desc.RTVFormats[attrib_index] = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->pColorFormats[attrib_index]);
    }

    // If running Linked Mode (SLI) create pipeline for all nodes
    // #NOTE : In non SLI mode, mNodeCount will be 0 which sets nodeMask to default value
    pipeline_state_desc.NodeMask = util_calculate_shared_node_mask(pRenderer);

    HRESULT result = E_FAIL;

#ifndef DISABLE_PIPELINE_LIBRARY
    wchar_t pipelineName[MAX_DEBUG_NAME_LENGTH + 32] = { 0 };
    if (psoCache)
    {
        psoShaderHash = tf_mem_hash_uint8_t((uint8_t*)VS.pShaderBytecode, VS.BytecodeLength, psoShaderHash);
        psoShaderHash = tf_mem_hash_uint8_t((uint8_t*)PS.pShaderBytecode, PS.BytecodeLength, psoShaderHash);
        psoShaderHash = tf_mem_hash_uint8_t((uint8_t*)HS.pShaderBytecode, HS.BytecodeLength, psoShaderHash);
        psoShaderHash = tf_mem_hash_uint8_t((uint8_t*)DS.pShaderBytecode, DS.BytecodeLength, psoShaderHash);
        psoShaderHash = tf_mem_hash_uint8_t((uint8_t*)GS.pShaderBytecode, GS.BytecodeLength, psoShaderHash);

        psoRenderHash = tf_mem_hash_uint8_t((uint8_t*)&pipeline_state_desc.BlendState, sizeof(D3D12_BLEND_DESC), psoRenderHash);
        psoRenderHash =
            tf_mem_hash_uint8_t((uint8_t*)&pipeline_state_desc.DepthStencilState, sizeof(D3D12_DEPTH_STENCIL_DESC), psoRenderHash);
        psoRenderHash = tf_mem_hash_uint8_t((uint8_t*)&pipeline_state_desc.RasterizerState, sizeof(D3D12_RASTERIZER_DESC), psoRenderHash);

        psoRenderHash =
            tf_mem_hash_uint8_t((uint8_t*)pipeline_state_desc.RTVFormats, render_target_count * sizeof(DXGI_FORMAT), psoRenderHash);
        psoRenderHash = tf_mem_hash_uint8_t((uint8_t*)&pipeline_state_desc.DSVFormat, sizeof(DXGI_FORMAT), psoRenderHash);
        psoRenderHash =
            tf_mem_hash_uint8_t((uint8_t*)&pipeline_state_desc.PrimitiveTopologyType, sizeof(D3D12_PRIMITIVE_TOPOLOGY_TYPE), psoRenderHash);
        psoRenderHash = tf_mem_hash_uint8_t((uint8_t*)&pipeline_state_desc.SampleDesc, sizeof(DXGI_SAMPLE_DESC), psoRenderHash);
        psoRenderHash = tf_mem_hash_uint8_t((uint8_t*)&pipeline_state_desc.NodeMask, sizeof(UINT), psoRenderHash);

        swprintf(pipelineName, MAX_DEBUG_NAME_LENGTH + 32, L"%S_S%zuR%zu", (pMainDesc->pName ? pMainDesc->pName : "GRAPHICSPSO"),
                 psoShaderHash, psoRenderHash);
        result = COM_CALL(LoadGraphicsPipeline, psoCache, pipelineName, &pipeline_state_desc,
                          IID_ARGS(ID3D12PipelineState, &pPipeline->mDx.pPipelineState));
    }
#endif

    if (!SUCCEEDED(result))
    {
        CHECK_HRESULT(hook_create_graphics_pipeline_state(pRenderer->mDx.pDevice, &pipeline_state_desc, pMainDesc->pPipelineExtensions,
                                                          pMainDesc->mExtensionCount, &pPipeline->mDx.pPipelineState));

#ifndef DISABLE_PIPELINE_LIBRARY
        if (psoCache)
        {
            CHECK_HRESULT(COM_CALL(StorePipeline, psoCache, pipelineName, pPipeline->mDx.pPipelineState));
        }
#endif
    }

    D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    switch (pDesc->mPrimitiveTopo)
    {
    case PRIMITIVE_TOPO_POINT_LIST:
        topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        break;
    case PRIMITIVE_TOPO_LINE_LIST:
        topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        break;
    case PRIMITIVE_TOPO_LINE_STRIP:
        topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        break;
    case PRIMITIVE_TOPO_TRI_LIST:
        topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        break;
    case PRIMITIVE_TOPO_TRI_STRIP:
        topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        break;
    case PRIMITIVE_TOPO_PATCH_LIST:
    {
        uint32_t controlPoint = pShaderProgram->mNumControlPoints;
        topology = (D3D_PRIMITIVE_TOPOLOGY)(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (controlPoint - 1));
    }
    break;

    default:
        break;
    }

    ASSERT(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED != topology);
    pPipeline->mDx.mPrimitiveTopology = topology;

    *ppPipeline = pPipeline;
}

static void addComputePipeline(Renderer* pRenderer, const PipelineDesc* pMainDesc, Pipeline** ppPipeline)
{
    ASSERT(pRenderer);
    ASSERT(ppPipeline);
    ASSERT(pMainDesc);

    const ComputePipelineDesc* pDesc = &pMainDesc->mComputeDesc;

    ASSERT(pDesc->pShaderProgram);
    ASSERT(pRenderer->mDx.pComputeRootSignature);
    ASSERT(pDesc->pShaderProgram->mDx.pCSBlob);

    Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, ALIGN_Pipeline, sizeof(Pipeline));
    ASSERT(pPipeline);

    pPipeline->mDx.mType = PIPELINE_TYPE_COMPUTE;

    // add pipeline specifying its for compute purposes
    D3D12_SHADER_BYTECODE CS = {
        .BytecodeLength = IDxcBlobEncoding_GetBufferSize(pDesc->pShaderProgram->mDx.pCSBlob),
        .pShaderBytecode = IDxcBlobEncoding_GetBufferPointer(pDesc->pShaderProgram->mDx.pCSBlob),
    };

    D3D12_CACHED_PIPELINE_STATE       cached_pso_desc = { 0 };
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_state_desc = {
        .pRootSignature = pRenderer->mDx.pComputeRootSignature,
        .CS = CS,
        .CachedPSO = cached_pso_desc,
        // If running Linked Mode (SLI) create pipeline for all nodes
        // #NOTE : In non SLI mode, mNodeCount will be 0 which sets nodeMask to default value
        .NodeMask = util_calculate_shared_node_mask(pRenderer),
    };

#if !defined(XBOX)
    pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
#endif

    HRESULT result = E_FAIL;
#ifndef DISABLE_PIPELINE_LIBRARY
    ID3D12PipelineLibrary* psoCache = pMainDesc->pCache ? pMainDesc->pCache->mDx.pLibrary : NULL;
    wchar_t                pipelineName[MAX_DEBUG_NAME_LENGTH + 32] = { 0 };

    if (psoCache)
    {
        size_t psoShaderHash = 0;
        psoShaderHash = tf_mem_hash_uint8_t((uint8_t*)CS.pShaderBytecode, CS.BytecodeLength, psoShaderHash);

        swprintf(pipelineName, MAX_DEBUG_NAME_LENGTH + 32, L"%S_S%zu", (pMainDesc->pName ? pMainDesc->pName : "COMPUTEPSO"), psoShaderHash);
        result = COM_CALL(LoadComputePipeline, psoCache, pipelineName, &pipeline_state_desc,
                          IID_ARGS(ID3D12PipelineState, &pPipeline->mDx.pPipelineState));
    }
#endif

    if (!SUCCEEDED(result))
    {
        CHECK_HRESULT(hook_create_compute_pipeline_state(pRenderer->mDx.pDevice, &pipeline_state_desc, pMainDesc->pPipelineExtensions,
                                                         pMainDesc->mExtensionCount, &pPipeline->mDx.pPipelineState));

#ifndef DISABLE_PIPELINE_LIBRARY
        if (psoCache)
        {
            CHECK_HRESULT(COM_CALL(StorePipeline, psoCache, pipelineName, pPipeline->mDx.pPipelineState));
        }
#endif
    }

    *ppPipeline = pPipeline;
}

#if defined(ENABLE_WORKGRAPH)
static void addWorkgraphPipeline(Renderer* pRenderer, const PipelineDesc* pMainDesc, Pipeline** ppPipeline);
#endif

void addPipeline(Renderer* pRenderer, const PipelineDesc* pDesc, Pipeline** ppPipeline)
{
    switch (pDesc->mType)
    {
    case PIPELINE_TYPE_COMPUTE:
    {
        addComputePipeline(pRenderer, pDesc, ppPipeline);
        break;
    }
    case PIPELINE_TYPE_GRAPHICS:
    {
        addGraphicsPipeline(pRenderer, pDesc, ppPipeline);
        break;
    }
#if defined(ENABLE_WORKGRAPH)
    case PIPELINE_TYPE_WORKGRAPH:
    {
        addWorkgraphPipeline(pRenderer, pDesc, ppPipeline);
        break;
    }
#endif
    default:
    {
        ASSERTFAIL("Unknown pipeline type %i", pDesc->mType);
        *ppPipeline = NULL;
        break;
    }
    }
}

void removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
    ASSERT(pRenderer);
    ASSERT(pPipeline);

    // delete pipeline from device
    hook_remove_pipeline(pPipeline);

    SAFE_FREE(pPipeline);
}

void addPipelineCache(Renderer* pRenderer, const PipelineCacheDesc* pDesc, PipelineCache** ppPipelineCache)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pDesc);
    UNREF_PARAM(ppPipelineCache);
#ifndef DISABLE_PIPELINE_LIBRARY
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppPipelineCache);

    PipelineCache* pPipelineCache = (PipelineCache*)tf_calloc(1, sizeof(PipelineCache));
    ASSERT(pPipelineCache);

    if (pDesc->mSize)
    {
        // D3D12 does not copy pipeline cache data. We have to keep it around until the cache is alive
        pPipelineCache->mDx.pData = tf_malloc(pDesc->mSize);
        memcpy(pPipelineCache->mDx.pData, pDesc->pData, pDesc->mSize);
    }

    D3D12_FEATURE_DATA_SHADER_CACHE feature = { 0 };
    HRESULT result = COM_CALL(CheckFeatureSupport, pRenderer->mDx.pDevice, D3D12_FEATURE_SHADER_CACHE, &feature, sizeof(feature));
    if (SUCCEEDED(result))
    {
        result = E_NOTIMPL;
        if (feature.SupportFlags & D3D12_SHADER_CACHE_SUPPORT_LIBRARY)
        {
            ID3D12Device1* device1 = NULL;
            result = COM_CALL(QueryInterface, pRenderer->mDx.pDevice, IID_REF(ID3D12Device1_Copy), &device1);
            if (SUCCEEDED(result))
            {
                result = hook_CreatePipelineLibrary(device1, pPipelineCache->mDx.pData, pDesc->mSize,
                                                    IID_ARGS(ID3D12PipelineLibrary, &pPipelineCache->mDx.pLibrary));
            }
            SAFE_RELEASE(device1);
        }
    }

    if (!SUCCEEDED(result))
    {
        LOGF(eWARNING, "Pipeline Cache Library feature is not present. Pipeline Cache will be disabled");
    }

    *ppPipelineCache = pPipelineCache;
#endif
}

void removePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pPipelineCache);
#ifndef DISABLE_PIPELINE_LIBRARY
    ASSERT(pRenderer);
    ASSERT(pPipelineCache);

    SAFE_RELEASE(pPipelineCache->mDx.pLibrary);
    SAFE_FREE(pPipelineCache->mDx.pData);
    SAFE_FREE(pPipelineCache);
#endif
}

void getPipelineCacheData(Renderer* pRenderer, PipelineCache* pPipelineCache, size_t* pSize, void* pData)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pPipelineCache);
    UNREF_PARAM(pData);
    ASSERT(pSize);

    *pSize = 0;

#ifndef DISABLE_PIPELINE_LIBRARY
    ASSERT(pRenderer);
    ASSERT(pPipelineCache);

    if (pPipelineCache->mDx.pLibrary)
    {
        *pSize = COM_CALL(GetSerializedSize, pPipelineCache->mDx.pLibrary);
        if (pData)
        {
            CHECK_HRESULT(COM_CALL(Serialize, pPipelineCache->mDx.pLibrary, pData, *pSize));
        }
    }
#endif
}

/************************************************************************/
// Command buffer Functions
/************************************************************************/
void resetCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
    ASSERT(pRenderer);
    ASSERT(pCmdPool);

    CHECK_HRESULT(COM_CALL(Reset, pCmdPool->pCmdAlloc));
}

void beginCmd(Cmd* pCmd)
{
    ASSERT(pCmd);
    ASSERT(pCmd->mDx.pCmdList);

    CHECK_HRESULT(COM_CALL(Reset, pCmd->mDx.pCmdList, pCmd->mDx.pCmdPool->pCmdAlloc, NULL));

    if (pCmd->mDx.mType != QUEUE_TYPE_TRANSFER)
    {
        ID3D12DescriptorHeap* heaps[] = {
            pCmd->mDx.pBoundHeaps[0]->pHeap,
            pCmd->mDx.pBoundHeaps[1]->pHeap,
        };
        hook_SetDescriptorHeaps(pCmd->mDx.pCmdList, 2, heaps);

        COM_CALL_RETURN_NO_ARGS(GetGPUDescriptorHandleForHeapStart, pCmd->mDx.pBoundHeaps[0]->pHeap, pCmd->mDx.mBoundHeapStartHandles[0]);
        COM_CALL_RETURN_NO_ARGS(GetGPUDescriptorHandleForHeapStart, pCmd->mDx.pBoundHeaps[1]->pHeap, pCmd->mDx.mBoundHeapStartHandles[1]);

        if (pCmd->mDx.mType == QUEUE_TYPE_GRAPHICS)
        {
            hook_SetGraphicsRootSignature(pCmd->mDx.pCmdList, pCmd->pRenderer->mDx.pGraphicsRootSignature);
        }
        hook_SetComputeRootSignature(pCmd->mDx.pCmdList, pCmd->pRenderer->mDx.pComputeRootSignature);
    }

    // Reset CPU side data
    memset(pCmd->mDx.mBoundDescriptorSets, 0, sizeof(pCmd->mDx.mBoundDescriptorSets));

#if defined(XBOX)
    pCmd->mDx.mSampleCount = 0;
#endif
}

void endCmd(Cmd* pCmd)
{
    ASSERT(pCmd);
    ASSERT(pCmd->mDx.pCmdList);

    CHECK_HRESULT(COM_CALL(Close, pCmd->mDx.pCmdList));
}

void cmdBindRenderTargets(Cmd* pCmd, const BindRenderTargetsDesc* pDesc)
{
    ASSERT(pCmd);
    ASSERT(pCmd->mDx.pCmdList);

    if (!pDesc)
    {
        return;
    }

    if (!pDesc->mRenderTargetCount && !pDesc->mDepthStencil.pDepthStencil)
    {
        hook_OMSetRenderTargets(pCmd->mDx.pCmdList, 0, NULL, FALSE, NULL);
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = { 0 };
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[MAX_RENDER_TARGET_ATTACHMENTS] = { 0 };
    const bool                  hasDepth = pDesc->mDepthStencil.pDepthStencil;
    SampleCount                 sampleCount = SAMPLE_COUNT_1;

    for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i)
    {
        const BindRenderTargetDesc* desc = &pDesc->mRenderTargets[i];
        sampleCount = desc->pRenderTarget->mSampleCount;
#if defined(XBOX)
        pCmd->mDx.mSampleCount = desc->pRenderTarget->mSampleCount;
#endif
        if (!desc->mUseMipSlice && !desc->mUseArraySlice)
        {
            rtvs[i] = descriptor_id_to_cpu_handle(pCmd->pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV],
                                                  desc->pRenderTarget->mDx.mDescriptors);
        }
        else
        {
            uint32_t handle = 0;
            if (desc->mUseMipSlice)
            {
                if (desc->mUseArraySlice)
                {
                    handle = 1 + desc->mMipSlice * (uint32_t)desc->pRenderTarget->mArraySize + desc->mArraySlice;
                }
                else
                {
                    handle = 1 + desc->mMipSlice;
                }
            }
            else if (desc->mUseArraySlice)
            {
                handle = 1 + desc->mArraySlice;
            }

            rtvs[i] = descriptor_id_to_cpu_handle(pCmd->pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV],
                                                  desc->pRenderTarget->mDx.mDescriptors + handle);
        }

        if (desc->mLoadAction == LOAD_ACTION_CLEAR)
        {
            const float* clearValue = desc->mOverrideClearValue ? &desc->mClearValue.r : &desc->pRenderTarget->mClearValue.r;
            hook_ClearRenderTargetView(pCmd->mDx.pCmdList, rtvs[i], clearValue, 0, NULL);
        }
    }

    // SetSamplePositions() needs to be called before potential depth clear. Read / write sample positions needs to match with clear.
    const SampleLocationDesc* pSampleLocation = &pDesc->mSampleLocation;
    uint32_t                  numPixels = pSampleLocation->mGridSizeX * pSampleLocation->mGridSizeY;
    uint32_t                  sampleLocationsCount = sampleCount * numPixels;
#if defined(XBOX)
    pCmd->mDx.mNumPixel = numPixels;
#endif
    if (sampleLocationsCount)
    {
        ASSERT(sampleLocationsCount <= MAX_SAMPLE_LOCATIONS);
        ASSERT(pSampleLocation->pLocations);

        D3D12_SAMPLE_POSITION* pLocs;
#if defined(XBOX)
        pLocs = pCmd->mDx.mSampleLocations;
#elif defined(_WINDOWS)
        D3D12_SAMPLE_POSITION sampleLocations[MAX_SAMPLE_LOCATIONS];
        pLocs = sampleLocations;
#endif
        for (uint32_t i = 0; i < sampleLocationsCount; ++i)
            pLocs[i] = (D3D12_SAMPLE_POSITION){ pDesc->mSampleLocation.pLocations[i].mX, pDesc->mSampleLocation.pLocations[i].mY };

        hook_SetSamplePositions(pCmd->mDx.pCmdList, sampleCount, numPixels, pLocs);
    }

    if (hasDepth)
    {
        const BindDepthTargetDesc* desc = &pDesc->mDepthStencil;
#if defined(XBOX)
        pCmd->mDx.mSampleCount = desc->pDepthStencil->mSampleCount;
#endif

        if (!desc->mUseMipSlice && !desc->mUseArraySlice)
        {
            dsv = descriptor_id_to_cpu_handle(pCmd->pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV],
                                              desc->pDepthStencil->mDx.mDescriptors);
        }
        else
        {
            uint32_t handle = 0;
            if (desc->mUseMipSlice)
            {
                if (desc->mUseArraySlice)
                {
                    handle = 1 + desc->mMipSlice * (uint32_t)desc->pDepthStencil->mArraySize + desc->mArraySlice;
                }
                else
                {
                    handle = 1 + desc->mMipSlice;
                }
            }
            else if (desc->mUseArraySlice)
            {
                handle = 1 + desc->mArraySlice;
            }

            dsv = descriptor_id_to_cpu_handle(pCmd->pRenderer->mDx.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV],
                                              desc->pDepthStencil->mDx.mDescriptors + handle);
        }

        ASSERT(dsv.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL);
        if (desc->mLoadAction == LOAD_ACTION_CLEAR || desc->mLoadActionStencil == LOAD_ACTION_CLEAR)
        {
            D3D12_CLEAR_FLAGS flags = (D3D12_CLEAR_FLAGS)0;
            if (desc->mLoadAction == LOAD_ACTION_CLEAR)
            {
                flags |= D3D12_CLEAR_FLAG_DEPTH;
            }
            if (desc->mLoadActionStencil == LOAD_ACTION_CLEAR)
            {
                flags |= D3D12_CLEAR_FLAG_STENCIL;
                ASSERT(TinyImageFormat_HasStencil(desc->pDepthStencil->mFormat));
            }
            ASSERT(flags > 0);
            const ClearValue* clearValue = desc->mOverrideClearValue ? &desc->mClearValue : &desc->pDepthStencil->mClearValue;
            hook_ClearDepthStencilView(pCmd->mDx.pCmdList, dsv, flags, clearValue->depth, (UINT8)clearValue->stencil, 0, NULL);
        }
    }

    hook_OMSetRenderTargets(pCmd->mDx.pCmdList, pDesc->mRenderTargetCount, rtvs, FALSE,
                            dsv.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL ? &dsv : NULL);
}

void cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
    ASSERT(pCmd);

    // set new viewport
    ASSERT(pCmd->mDx.pCmdList);

    D3D12_VIEWPORT viewport = {
        .TopLeftX = x,
        .TopLeftY = y,
        .Width = width,
        .Height = height,
        .MinDepth = minDepth,
        .MaxDepth = maxDepth,
    };

    hook_RSSetViewports(pCmd->mDx.pCmdList, 1, &viewport);
}

void cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    ASSERT(pCmd);

    // set new scissor values
    ASSERT(pCmd->mDx.pCmdList);

    D3D12_RECT scissor;
    scissor.left = x;
    scissor.top = y;
    scissor.right = x + width;
    scissor.bottom = y + height;

    hook_RSSetScissorRects(pCmd->mDx.pCmdList, 1, &scissor);
}

void cmdSetStencilReferenceValue(Cmd* pCmd, uint32_t val)
{
    ASSERT(pCmd);
    ASSERT(pCmd->mDx.pCmdList);

    hook_OMSetStencilRef(pCmd->mDx.pCmdList, val);
}

void cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
    ASSERT(pCmd);
    ASSERT(pPipeline);

    // bind given pipeline
    ASSERT(pCmd->mDx.pCmdList);

    if (pPipeline->mDx.mType == PIPELINE_TYPE_GRAPHICS)
    {
        ASSERT(pPipeline->mDx.pPipelineState);
        hook_IASetPrimitiveTopology(pCmd->mDx.pCmdList, pPipeline->mDx.mPrimitiveTopology);
        hook_SetPipelineState(pCmd->mDx.pCmdList, pPipeline->mDx.pPipelineState);
        pCmd->mDx.mPipelineType = PIPELINE_TYPE_GRAPHICS;
    }
    else
    {
        ASSERT(pPipeline->mDx.pPipelineState);
        hook_SetPipelineState(pCmd->mDx.pCmdList, pPipeline->mDx.pPipelineState);
        pCmd->mDx.mPipelineType = PIPELINE_TYPE_COMPUTE;
    }

#if defined(XBOX)
    if (pCmd->mDx.mNumPixel * pCmd->mDx.mSampleCount)
    {
        hook_SetSamplePositions(pCmd->mDx.pCmdList, pCmd->mDx.mSampleCount, pCmd->mDx.mNumPixel, pCmd->mDx.mSampleLocations);
    }

#if defined(XBOXONE)
    memset(pCmd->mDx.mBoundDescriptorSets, 0, sizeof(pCmd->mDx.mBoundDescriptorSets));
#endif

#endif
}

void cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
    ASSERT(pCmd);
    ASSERT(pBuffer);
    ASSERT(pCmd->mDx.pCmdList);
    ASSERT(D3D12_GPU_VIRTUAL_ADDRESS_NULL != pBuffer->mDx.mGpuAddress);

    D3D12_INDEX_BUFFER_VIEW ibView = {
        .BufferLocation = pBuffer->mDx.mGpuAddress + offset,
        .Format = (INDEX_TYPE_UINT16 == indexType) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
        .SizeInBytes = (UINT)(pBuffer->mSize - offset),
    };
    hook_IASetIndexBuffer(pCmd->mDx.pCmdList, &ibView);
}

void cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
{
    ASSERT(pCmd);
    ASSERT(0 != bufferCount);
    ASSERT(ppBuffers);
    ASSERT(pCmd->mDx.pCmdList);
    // bind given vertex buffer

    D3D12_VERTEX_BUFFER_VIEW views[MAX_VERTEX_ATTRIBS] = { 0 };
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        ASSERT(D3D12_GPU_VIRTUAL_ADDRESS_NULL != ppBuffers[i]->mDx.mGpuAddress);
        views[i] = (D3D12_VERTEX_BUFFER_VIEW){
            .BufferLocation = (ppBuffers[i]->mDx.mGpuAddress + (pOffsets ? pOffsets[i] : 0)),
            .SizeInBytes = (UINT)(ppBuffers[i]->mSize - (pOffsets ? pOffsets[i] : 0)),
            .StrideInBytes = (UINT)pStrides[i],
        };
    }

    hook_IASetVertexBuffers(pCmd->mDx.pCmdList, 0, bufferCount, views);
}

void cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
    ASSERT(pCmd);

    // draw given vertices
    ASSERT(pCmd->mDx.pCmdList);

    COM_CALL(DrawInstanced, pCmd->mDx.pCmdList, (UINT)vertexCount, (UINT)1, (UINT)firstVertex, (UINT)0);
}

void cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
    ASSERT(pCmd);

    // draw given vertices
    ASSERT(pCmd->mDx.pCmdList);

    COM_CALL(DrawInstanced, pCmd->mDx.pCmdList, (UINT)vertexCount, (UINT)instanceCount, (UINT)firstVertex, (UINT)firstInstance);
}

void cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
    ASSERT(pCmd);

    // draw indexed mesh
    ASSERT(pCmd->mDx.pCmdList);

    COM_CALL(DrawIndexedInstanced, pCmd->mDx.pCmdList, (UINT)indexCount, (UINT)1, (UINT)firstIndex, (UINT)firstVertex, (UINT)0);
}

void cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex,
                             uint32_t firstInstance)
{
    ASSERT(pCmd);

    // draw indexed mesh
    ASSERT(pCmd->mDx.pCmdList);

    COM_CALL(DrawIndexedInstanced, pCmd->mDx.pCmdList, (UINT)indexCount, (UINT)instanceCount, (UINT)firstIndex, (UINT)firstVertex,
             (UINT)firstInstance);
}

void cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    ASSERT(pCmd);

    // dispatch given command
    ASSERT(pCmd->mDx.pCmdList != NULL);

#if defined(_WINDOWS) && defined(D3D12_RAYTRACING_AVAILABLE) && defined(ENABLE_GRAPHICS_VALIDATION)
    // Bug in validation when using acceleration structure in compute or graphics pipeline
    // D3D12 ERROR: ID3D12CommandList::Dispatch: Static Descriptor SRV resource dimensions (UNKNOWN (11)) differs from that expected by
    // shader (D3D12_SRV_DIMENSION_BUFFER) UNKNOWN (11) is D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE
    // // #TODO
    // if (pCmd->pRenderer->mDx.pDebugValidation && pCmd->mDx.pBoundRootSignature->mDx.mHasRayQueryAccelerationStructure)
    //{
    //    D3D12_MESSAGE_ID        hide[] = { D3D12_MESSAGE_ID_COMMAND_LIST_STATIC_DESCRIPTOR_RESOURCE_DIMENSION_MISMATCH };
    //    D3D12_INFO_QUEUE_FILTER filter = {};
    //    filter.DenyList.NumIDs = 1;
    //    filter.DenyList.pIDList = hide;
    //    pCmd->pRenderer->mDx.pDebugValidation->PushStorageFilter(&filter);
    //}
#endif

    hook_dispatch(pCmd, groupCountX, groupCountY, groupCountZ);

#if defined(_WINDOWS) && defined(D3D12_RAYTRACING_AVAILABLE) && defined(ENABLE_GRAPHICS_VALIDATION)
    // // #TODO
    // if (pCmd->pRenderer->mDx.pDebugValidation && pCmd->mDx.pBoundRootSignature->mDx.mHasRayQueryAccelerationStructure)
    //{
    //    pCmd->pRenderer->mDx.pDebugValidation->PopStorageFilter();
    //}
#endif
}

void cmdResourceBarrier(Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers,
                        TextureBarrier* pTextureBarriers, uint32_t numRtBarriers, RenderTargetBarrier* pRtBarriers)
{
    D3D12_RESOURCE_BARRIER* barriers =
        (D3D12_RESOURCE_BARRIER*)_alloca((numBufferBarriers + numTextureBarriers + numRtBarriers) * sizeof(D3D12_RESOURCE_BARRIER));
    uint32_t transitionCount = 0;

#if defined(ENABLE_GRAPHICS_VALIDATION) && defined(_WINDOWS)
    ID3D12DebugCommandList* debugCmd = pCmd->mDx.pDebugCmdList;
#endif

    for (uint32_t i = 0; i < numBufferBarriers; ++i)
    {
        BufferBarrier*          pTransBarrier = &pBufferBarriers[i];
        D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
        Buffer*                 pBuffer = pTransBarrier->pBuffer;

        // Only transition GPU visible resources.
        // Note: General CPU_TO_GPU resources have to stay in generic read state. They are created in upload heap.
        // There is one corner case: CPU_TO_GPU resources with UAV usage can have state transition. And they are created in custom heap.
        if (pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY || pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU ||
            (pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU && (pBuffer->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER)))
        {
            // if (!(pBuffer->mCurrentState & pTransBarrier->mNewState) && pBuffer->mCurrentState != pTransBarrier->mNewState)
            if (RESOURCE_STATE_UNORDERED_ACCESS == pTransBarrier->mCurrentState &&
                RESOURCE_STATE_UNORDERED_ACCESS == pTransBarrier->mNewState)
            {
                pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                pBarrier->UAV.pResource = pBuffer->mDx.pResource;
                ++transitionCount;
            }
#ifdef D3D12_RAYTRACING_AVAILABLE
            else if ((RESOURCE_STATE_ACCELERATION_STRUCTURE_WRITE & pTransBarrier->mCurrentState) &&
                     (RESOURCE_STATE_ACCELERATION_STRUCTURE_READ & pTransBarrier->mNewState))
            {
                pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                pBarrier->UAV.pResource = pBuffer->mDx.pResource;
                ++transitionCount;
            }
#endif
            else
            {
                pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                if (pTransBarrier->mBeginOnly)
                {
                    pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
                }
                else if (pTransBarrier->mEndOnly)
                {
                    pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
                }
                pBarrier->Transition.pResource = pBuffer->mDx.pResource;
                pBarrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                pBarrier->Transition.StateBefore = util_to_dx12_resource_state(pTransBarrier->mCurrentState);
                pBarrier->Transition.StateAfter = util_to_dx12_resource_state(pTransBarrier->mNewState);

                ++transitionCount;

#if defined(ENABLE_GRAPHICS_VALIDATION) && defined(_WINDOWS)
                if (debugCmd)
                {
                    COM_CALL(AssertResourceState, debugCmd, pBarrier->Transition.pResource, pBarrier->Transition.Subresource,
                             pBarrier->Transition.StateBefore);
                }
#endif
            }
        }
    }

    for (uint32_t i = 0; i < numTextureBarriers; ++i)
    {
        TextureBarrier*         pTrans = &pTextureBarriers[i];
        D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
        Texture*                pTexture = pTrans->pTexture;

        if (RESOURCE_STATE_UNORDERED_ACCESS == pTrans->mCurrentState && RESOURCE_STATE_UNORDERED_ACCESS == pTrans->mNewState)
        {
            pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            pBarrier->UAV.pResource = pTexture->mDx.pResource;
            ++transitionCount;
        }
        else
        {
            pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            if (pTrans->mBeginOnly)
            {
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
            }
            else if (pTrans->mEndOnly)
            {
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
            }
            pBarrier->Transition.pResource = pTexture->mDx.pResource;
            pBarrier->Transition.Subresource = pTrans->mSubresourceBarrier
                                                   ? CALC_SUBRESOURCE_INDEX(pTrans->mMipLevel, pTrans->mArrayLayer, 0, pTexture->mMipLevels,
                                                                            pTexture->mArraySizeMinusOne + 1)
                                                   : D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            if (pTrans->mAcquire)
                pBarrier->Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            else
                pBarrier->Transition.StateBefore = util_to_dx12_resource_state(pTrans->mCurrentState);

            if (pTrans->mRelease)
                pBarrier->Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
            else
                pBarrier->Transition.StateAfter = util_to_dx12_resource_state(pTrans->mNewState);

            ++transitionCount;

#if defined(ENABLE_GRAPHICS_VALIDATION) && defined(_WINDOWS)
            if (debugCmd)
            {
                COM_CALL(AssertResourceState, debugCmd, pBarrier->Transition.pResource, pBarrier->Transition.Subresource,
                         pBarrier->Transition.StateBefore);
            }
#endif
        }
    }

    for (uint32_t i = 0; i < numRtBarriers; ++i)
    {
        RenderTargetBarrier*    pTrans = &pRtBarriers[i];
        D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
        Texture*                pTexture = pTrans->pRenderTarget->pTexture;

        if (RESOURCE_STATE_UNORDERED_ACCESS == pTrans->mCurrentState && RESOURCE_STATE_UNORDERED_ACCESS == pTrans->mNewState)
        {
            pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            pBarrier->UAV.pResource = pTexture->mDx.pResource;
            ++transitionCount;
        }
        else
        {
            pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            if (pTrans->mBeginOnly)
            {
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
            }
            else if (pTrans->mEndOnly)
            {
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
            }
            pBarrier->Transition.pResource = pTexture->mDx.pResource;
            pBarrier->Transition.Subresource = pTrans->mSubresourceBarrier
                                                   ? CALC_SUBRESOURCE_INDEX(pTrans->mMipLevel, pTrans->mArrayLayer, 0, pTexture->mMipLevels,
                                                                            pTexture->mArraySizeMinusOne + 1)
                                                   : D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            if (pTrans->mAcquire)
                pBarrier->Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            else
                pBarrier->Transition.StateBefore = util_to_dx12_resource_state(pTrans->mCurrentState);

            if (pTrans->mRelease)
                pBarrier->Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
            else
                pBarrier->Transition.StateAfter = util_to_dx12_resource_state(pTrans->mNewState);

            ++transitionCount;

#if defined(ENABLE_GRAPHICS_VALIDATION) && defined(_WINDOWS)
            if (debugCmd)
            {
                COM_CALL(AssertResourceState, debugCmd, pBarrier->Transition.pResource, pBarrier->Transition.Subresource,
                         pBarrier->Transition.StateBefore);
            }
#endif
        }
    }

    if (transitionCount)
    {
        hook_resource_barrier(pCmd, transitionCount, barriers);
    }
}

void cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
    ASSERT(pCmd);
    ASSERT(pSrcBuffer);
    ASSERT(pSrcBuffer->mDx.pResource);
    ASSERT(pBuffer);
    ASSERT(pBuffer->mDx.pResource);

    hook_copy_buffer_region(pCmd, pBuffer->mDx.pResource, dstOffset, pSrcBuffer->mDx.pResource, srcOffset, size);
}

void cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pDesc)
{
    uint32_t subresource =
        CALC_SUBRESOURCE_INDEX(pDesc->mMipLevel, pDesc->mArrayLayer, 0, pTexture->mMipLevels, pTexture->mArraySizeMinusOne + 1);
    D3D12_RESOURCE_DESC resourceDesc;
    COM_CALL(GetDesc, pTexture->mDx.pResource, &resourceDesc);

    D3D12_TEXTURE_COPY_LOCATION src = {
        .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
        .pResource = pSrcBuffer->mDx.pResource,
    };
    COM_CALL(GetCopyableFootprints, pCmd->pRenderer->mDx.pDevice, &resourceDesc, subresource, 1, pDesc->mSrcOffset, &src.PlacedFootprint,
             NULL, NULL, NULL);
    src.PlacedFootprint.Offset = pDesc->mSrcOffset;

    D3D12_TEXTURE_COPY_LOCATION dst = {
        .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .pResource = pTexture->mDx.pResource,
        .SubresourceIndex = subresource,
    };
    hook_copy_texture_region(pCmd, &dst, 0, 0, 0, &src, NULL);
}

void cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const SubresourceDataDesc* pDesc)
{
    uint32_t subresource =
        CALC_SUBRESOURCE_INDEX(pDesc->mMipLevel, pDesc->mArrayLayer, 0, pTexture->mMipLevels, pTexture->mArraySizeMinusOne + 1);
    D3D12_RESOURCE_DESC resourceDesc;
    COM_CALL(GetDesc, pTexture->mDx.pResource, &resourceDesc);

    D3D12_TEXTURE_COPY_LOCATION src = {
        .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .pResource = pTexture->mDx.pResource,
        .SubresourceIndex = subresource,
    };
    D3D12_TEXTURE_COPY_LOCATION dst = {
        .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
        .pResource = pDstBuffer->mDx.pResource,
    };
    COM_CALL(GetCopyableFootprints, pCmd->pRenderer->mDx.pDevice, &resourceDesc, subresource, 1, pDesc->mSrcOffset, &dst.PlacedFootprint,
             NULL, NULL, NULL);
    dst.PlacedFootprint.Offset = pDesc->mSrcOffset;

    hook_copy_texture_region(pCmd, &dst, 0, 0, 0, &src, NULL);
}

/************************************************************************/
// Queue Fence Semaphore Functions
/************************************************************************/
void acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence,
                      uint32_t* pSwapChainImageIndex)
{
    UNREF_PARAM(pSignalSemaphore);
    UNREF_PARAM(pFence);
    ASSERT(pRenderer);
    ASSERT(pSwapChainImageIndex);

    // get latest backbuffer image
    HRESULT hr = hook_acquire_next_image(pRenderer->mDx.pDevice, pSwapChain);
    if (FAILED(hr))
    {
        LOGF(eERROR, "Failed to acquire next image");
        *pSwapChainImageIndex = UINT32_MAX;
        return;
    }

    *pSwapChainImageIndex = hook_get_swapchain_image_index(pSwapChain);
}

void queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc)
{
    ASSERT(pDesc);

    uint32_t    cmdCount = pDesc->mCmdCount;
    Cmd**       pCmds = pDesc->ppCmds;
    Fence*      pFence = pDesc->pSignalFence;
    uint32_t    waitSemaphoreCount = pDesc->mWaitSemaphoreCount;
    Semaphore** ppWaitSemaphores = pDesc->ppWaitSemaphores;
    uint32_t    signalSemaphoreCount = pDesc->mSignalSemaphoreCount;
    Semaphore** ppSignalSemaphores = pDesc->ppSignalSemaphores;

    // ASSERT that given cmd list and given params are valid
    ASSERT(pQueue);
    ASSERT(cmdCount > 0);
    ASSERT(pCmds);
    if (waitSemaphoreCount > 0)
    {
        ASSERT(ppWaitSemaphores);
    }
    if (signalSemaphoreCount > 0)
    {
        ASSERT(ppSignalSemaphores);
    }

    // execute given command list
    ASSERT(pQueue->mDx.pQueue);

    ID3D12CommandList** cmds = (ID3D12CommandList**)_alloca(cmdCount * sizeof(ID3D12CommandList*));
    for (uint32_t i = 0; i < cmdCount; ++i)
    {
        cmds[i] = (ID3D12CommandList*)pCmds[i]->mDx.pCmdList; // -V1027
    }

    for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
    {
        CHECK_HRESULT(hook_wait(pQueue, ppWaitSemaphores[i]->mDx.pFence, ppWaitSemaphores[i]->mDx.mFenceValue));
    }

    COM_CALL(ExecuteCommandLists, pQueue->mDx.pQueue, cmdCount, cmds);

    if (pFence)
    {
        CHECK_HRESULT(hook_signal(pQueue, pFence->mDx.pFence, ++pFence->mDx.mFenceValue));
    }

    for (uint32_t i = 0; i < signalSemaphoreCount; ++i)
    {
        CHECK_HRESULT(hook_signal(pQueue, ppSignalSemaphores[i]->mDx.pFence, ++ppSignalSemaphores[i]->mDx.mFenceValue));
    }
}

void queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc)
{
    if (!pDesc->pSwapChain)
    {
        return;
    }

#if defined(_WINDOWS) && defined(ENABLE_GRAPHICS_VALIDATION)
    Renderer* pRenderer = pQueue->mDx.pRenderer;
    if (pRenderer->mDx.pDebugValidation && pRenderer->mDx.mSuppressMismatchingCommandListDuringPresent)
    {
        D3D12_MESSAGE_ID        hide[] = { D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE };
        D3D12_INFO_QUEUE_FILTER filter = { 0 };
        filter.DenyList.NumIDs = 1;
        filter.DenyList.pIDList = hide;
        COM_CALL(PushStorageFilter, pRenderer->mDx.pDebugValidation, &filter);
    }
#endif

    SwapChain* pSwapChain = pDesc->pSwapChain;
#if defined(AUTOMATED_TESTING)
    if (isScreenshotCaptureRequested())
    {
        ScreenshotDesc desc = {
            .pRenderTarget = pSwapChain->ppRenderTargets[pDesc->mIndex],
            .ppWaitSemaphores = pDesc->ppWaitSemaphores,
            .mWaitSemaphoresCount = pDesc->mWaitSemaphoreCount,
            .mColorSpace = pSwapChain->mColorSpace,
            .discardAlpha = true,
            .flipRedBlue = false,
        };
        captureScreenshot(&desc);
    }
#endif

    HRESULT hr = hook_queue_present(pQueue, pSwapChain, pDesc->mIndex);

#if defined(_WINDOWS) && defined(ENABLE_GRAPHICS_VALIDATION)
    if (pRenderer->mDx.pDebugValidation && pRenderer->mDx.mSuppressMismatchingCommandListDuringPresent)
    {
        COM_CALL(PopStorageFilter, pRenderer->mDx.pDebugValidation);
    }
#endif

    if (FAILED(hr))
    {
#if defined(_WINDOWS)
        ID3D12Device* device = NULL;
        COM_CALL(GetDevice, pSwapChain->mDx.pSwapChain, IID_ARGS(ID3D12Device, &device));
        HRESULT removeHr = COM_CALL(GetDeviceRemovedReason, device);

        if (!VERIFY(SUCCEEDED(removeHr)))
        {
            threadSleep(5000); // Wait for a few seconds to allow the driver to come back online before doing a reset.
            ResetDesc resetDesc;
            resetDesc.mType = RESET_TYPE_DEVICE_LOST;
            requestReset(&resetDesc);
        }

#if defined(USE_DRED)
        ID3D12DeviceRemovedExtendedData* pDread;
        if (SUCCEEDED(COM_CALL(QueryInterface, device, IID_ARGS(ID3D12DeviceRemovedExtendedData, &pDread))))
        {
            D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs;
            if (SUCCEEDED(hook_GetAutoBreadcrumbsOutput(pDread, &breadcrumbs)))
            {
                LOGF(eINFO, "Gathered auto-breadcrumbs output.");
            }

            D3D12_DRED_PAGE_FAULT_OUTPUT pageFault;
            if (SUCCEEDED(hook_GetPageFaultAllocationOutput(pDread, &pageFault)))
            {
                LOGF(eINFO, "Gathered page fault allocation output.");
            }
        }
        COM_CALL(Release, pDread);
#endif
        COM_CALL(Release, device);
#endif
        LOGF(eERROR, "Failed to present swapchain render target");
    }
}

static inline void GetFenceStatus(Fence* pFence, FenceStatus* pFenceStatus)
{
    if (!pFence->mDx.mFenceValue)
    {
        *pFenceStatus = FENCE_STATUS_NOTSUBMITTED;
    }
    else if (COM_CALL(GetCompletedValue, pFence->mDx.pFence) < pFence->mDx.mFenceValue)
    {
        *pFenceStatus = FENCE_STATUS_INCOMPLETE;
    }
    else
    {
        *pFenceStatus = FENCE_STATUS_COMPLETE;
    }
}

static void WaitForFences(uint32_t fenceCount, Fence** ppFences)
{
    // Wait for fence completion
    for (uint32_t i = 0; i < fenceCount; ++i)
    {
        FenceStatus fenceStatus;
        GetFenceStatus(ppFences[i], &fenceStatus);
        uint64_t fenceValue = ppFences[i]->mDx.mFenceValue;
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
        {
            COM_CALL(SetEventOnCompletion, ppFences[i]->mDx.pFence, fenceValue, ppFences[i]->mDx.pWaitIdleFenceEvent);
            WaitForSingleObject(ppFences[i]->mDx.pWaitIdleFenceEvent, INFINITE);
        }
    }
}

void getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
    UNREF_PARAM(pRenderer);
    GetFenceStatus(pFence, pFenceStatus);
}

void waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
{
    UNREF_PARAM(pRenderer);
    WaitForFences(fenceCount, ppFences);
}

void waitQueueIdle(Queue* pQueue)
{
    hook_signal_flush(pQueue, pQueue->mDx.pFence->mDx.pFence, ++pQueue->mDx.pFence->mDx.mFenceValue);
    WaitForFences(1, &pQueue->mDx.pFence);
}
/************************************************************************/
// Utility functions
/************************************************************************/
TinyImageFormat getSupportedSwapchainFormat(Renderer* pRenderer, const SwapChainDesc* pDesc, ColorSpace colorSpace)
{
    return hook_get_recommended_swapchain_format(pRenderer, pDesc, colorSpace);
}

uint32_t getRecommendedSwapchainImageCount(Renderer* pRenderer, const WindowHandle* hwnd)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(hwnd);
#if defined(XBOX)
    return 2;
#else
    return 3;
#endif
}
/************************************************************************/
// Execute Indirect Implementation
/************************************************************************/
void cmdExecuteIndirect(Cmd* pCmd, IndirectArgumentType type, uint32_t maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
                        Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
    ASSERT(pIndirectBuffer);

#if defined(_WINDOWS) && defined(D3D12_RAYTRACING_AVAILABLE) && defined(ENABLE_GRAPHICS_VALIDATION)
    // #TODO
    // if (pCmd->pRenderer->mDx.pDebugValidation && pCmd->mDx.pBoundRootSignature->mDx.mHasRayQueryAccelerationStructure)
    //{
    //    D3D12_MESSAGE_ID        hide[] = { D3D12_MESSAGE_ID_COMMAND_LIST_STATIC_DESCRIPTOR_RESOURCE_DIMENSION_MISMATCH };
    //    D3D12_INFO_QUEUE_FILTER filter = {};
    //    filter.DenyList.NumIDs = 1;
    //    filter.DenyList.pIDList = hide;
    //    pCmd->pRenderer->mDx.pDebugValidation->PushStorageFilter(&filter);
    //}
#endif

    ID3D12CommandSignature* cmdSignature = NULL;
    const uint32_t          nodeIndex = pCmd->mDx.mNodeIndex;
    switch (type)
    {
    case INDIRECT_DRAW:
        cmdSignature = pCmd->pRenderer->pNullDescriptors->pDrawCommandSignature[nodeIndex];
        break;
    case INDIRECT_DRAW_INDEX:
        cmdSignature = pCmd->pRenderer->pNullDescriptors->pDrawIndexCommandSignature[nodeIndex];
        break;
    case INDIRECT_DISPATCH:
#if defined(XBOX)
        if (QUEUE_TYPE_COMPUTE == pCmd->mDx.mType)
        {
            cmdSignature = pCmd->pRenderer->pNullDescriptors->pAsyncDispatchCommandSignature[nodeIndex];
        }
        else
#endif
        {
            cmdSignature = pCmd->pRenderer->pNullDescriptors->pDispatchCommandSignature[nodeIndex];
        }
        break;
    default:
        ASSERTFAIL("Invalid IndirectArgumentType %u", (uint32_t)type);
        break;
    }

    if (!pCounterBuffer)
    {
        hook_ExecuteIndirect(pCmd->mDx.pCmdList, cmdSignature, maxCommandCount, pIndirectBuffer->mDx.pResource, bufferOffset, NULL, 0);
    }
    else
    {
        hook_ExecuteIndirect(pCmd->mDx.pCmdList, cmdSignature, maxCommandCount, pIndirectBuffer->mDx.pResource, bufferOffset,
                             pCounterBuffer->mDx.pResource, counterBufferOffset);
    }

#if defined(_WINDOWS) && defined(D3D12_RAYTRACING_AVAILABLE) && defined(ENABLE_GRAPHICS_VALIDATION)
    // #TODO
    // if (pCmd->pRenderer->mDx.pDebugValidation && pCmd->mDx.pBoundRootSignature->mDx.mHasRayQueryAccelerationStructure)
    //{
    //    pCmd->pRenderer->mDx.pDebugValidation->PopStorageFilter();
    //}
#endif
}
/************************************************************************/
// Workgraph Implementation
/************************************************************************/
#if defined(ENABLE_WORKGRAPH)
static void addWorkgraphPipeline(Renderer* pRenderer, const PipelineDesc* pMainDesc, Pipeline** ppPipeline)
{
    ASSERT(pRenderer);
    ASSERT(pMainDesc);
    ASSERT(ppPipeline);

    const WorkgraphPipelineDesc* desc = &pMainDesc->mWorkgraphDesc;

    size_t    extSize = sizeof(WCHAR) * (strlen(desc->pWorkgraphName) + 1);
    Pipeline* pipeline = (Pipeline*)tf_calloc_memalign(1, ALIGN_Pipeline, sizeof(Pipeline) + extSize);
    ASSERT(pipeline);

    pipeline->mDx.mType = PIPELINE_TYPE_WORKGRAPH;
    pipeline->mDx.pWorkgraphName = (WCHAR*)(pipeline + 1);

    D3D12_STATE_SUBOBJECT subobjects[3] = { 0 };
    uint32_t              subobjectCount = 0;
    subobjects[subobjectCount++] =
        (D3D12_STATE_SUBOBJECT){ D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &pRenderer->mDx.pComputeRootSignature };

    D3D12_DXIL_LIBRARY_DESC dxilDesc = { 0 };
    dxilDesc.DXILLibrary.BytecodeLength = IDxcBlobEncoding_GetBufferSize(desc->pShaderProgram->mDx.pCSBlob);
    dxilDesc.DXILLibrary.pShaderBytecode = IDxcBlobEncoding_GetBufferPointer(desc->pShaderProgram->mDx.pCSBlob);
    subobjects[subobjectCount++] = (D3D12_STATE_SUBOBJECT){ D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &dxilDesc };

    mbstowcs(pipeline->mDx.pWorkgraphName, desc->pWorkgraphName, strlen(desc->pWorkgraphName));
    D3D12_WORK_GRAPH_DESC workgraphDesc = {
        .Flags = D3D12_WORK_GRAPH_FLAG_INCLUDE_ALL_AVAILABLE_NODES,
        .ProgramName = pipeline->mDx.pWorkgraphName,
    };
    subobjects[subobjectCount++] = (D3D12_STATE_SUBOBJECT){ D3D12_STATE_SUBOBJECT_TYPE_WORK_GRAPH, &workgraphDesc };

    ID3D12Device9* device = NULL;
    HRESULT        hres = COM_CALL(QueryInterface, pRenderer->mDx.pDevice, IID_REF(ID3D12Device9_Copy), &device);
    if (!SUCCEEDED(hres))
    {
        ASSERTFAIL("ID3D12Device9 query failed");
        return;
    }

    ASSERT(subobjectCount <= TF_ARRAY_COUNT(subobjects));

    D3D12_STATE_OBJECT_DESC stateDesc = {
        .Type = D3D12_STATE_OBJECT_TYPE_EXECUTABLE,
        .NumSubobjects = subobjectCount,
        .pSubobjects = subobjects,
    };
    hres = hook_CreateStateObject((ID3D12Device5*)device, &stateDesc, IID_ARGS(ID3D12StateObject, &pipeline->mDx.pStateObject)); // -V1027
    if (!SUCCEEDED(hres))
    {
        ASSERTFAIL("CreateStateObject failed");
        return;
    }

    SAFE_RELEASE(device);

    *ppPipeline = pipeline;
}

void addWorkgraph(Renderer* pRenderer, const WorkgraphDesc* pDesc, Workgraph** ppWorkgraph)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppWorkgraph);

    Workgraph* workgraph = (Workgraph*)tf_calloc_memalign(1, sizeof(void*), sizeof(Workgraph));
    ASSERT(workgraph);

    workgraph->pPipeline = pDesc->pPipeline;

    ID3D12StateObjectProperties1* stateObjProps = NULL;
    ID3D12WorkGraphProperties*    workgraphProps = NULL;
    CHECK_HRESULT(COM_CALL(QueryInterface, pDesc->pPipeline->mDx.pStateObject, IID_ARGS(ID3D12StateObjectProperties1, &stateObjProps)));
    CHECK_HRESULT(COM_CALL(QueryInterface, pDesc->pPipeline->mDx.pStateObject, IID_ARGS(ID3D12WorkGraphProperties, &workgraphProps)));

    const WCHAR*             workgraphName = pDesc->pPipeline->mDx.pWorkgraphName;
    D3D12_PROGRAM_IDENTIFIER id;
    COM_CALL_RETURN_DST_FIRST(GetProgramIdentifier, stateObjProps, id, workgraphName);
    uint32_t                             workgraphIndex = COM_CALL(GetWorkGraphIndex, workgraphProps, workgraphName);
    D3D12_WORK_GRAPH_MEMORY_REQUIREMENTS memReqs = { 0 };
    COM_CALL(GetWorkGraphMemoryRequirements, workgraphProps, workgraphIndex, &memReqs);
    ASSERT(memReqs.MaxSizeInBytes);

    if (memReqs.MaxSizeInBytes != 0) //-V547 - MaxSizeInBytes can be zero. PVS false positive
    {
        BufferDesc backingDesc = {
            .mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
            .mFlags = BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION,
            .mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER,
            .mStartState = RESOURCE_STATE_COMMON,
            .mSize = memReqs.MaxSizeInBytes,
        };
        addBuffer(pRenderer, &backingDesc, &workgraph->pBackingBuffer);
    }

    workgraph->mId = id;

    SAFE_RELEASE(stateObjProps);
    SAFE_RELEASE(workgraphProps);

    *ppWorkgraph = workgraph;
}

void removeWorkgraph(Renderer* pRenderer, Workgraph* pWorkgraph)
{
    ASSERT(pRenderer);
    ASSERT(pWorkgraph);

    if (pWorkgraph->pBackingBuffer)
    {
        removeBuffer(pRenderer, pWorkgraph->pBackingBuffer);
    }

    SAFE_FREE(pWorkgraph);
}

void cmdDispatchWorkgraph(Cmd* pCmd, const DispatchGraphDesc* pDesc)
{
    ID3D12GraphicsCommandList10* cmd = NULL;
    HRESULT                      hres = COM_CALL(QueryInterface, pCmd->mDx.pCmdList, IID_ARGS(ID3D12GraphicsCommandList10, &cmd));
    if (!SUCCEEDED(hres))
    {
        ASSERTFAIL("ID3D12GraphicsCommandList10 query failed");
        return;
    }

    D3D12_SET_WORK_GRAPH_DESC workgraphDesc = {
        .BackingMemory = (D3D12_GPU_VIRTUAL_ADDRESS_RANGE){ pDesc->pWorkgraph->pBackingBuffer->mDx.mGpuAddress,
                                                            pDesc->pWorkgraph->pBackingBuffer->mSize },
        .Flags = pDesc->mInitialize ? D3D12_SET_WORK_GRAPH_FLAG_INITIALIZE : D3D12_SET_WORK_GRAPH_FLAG_NONE,
        .ProgramIdentifier = pDesc->pWorkgraph->mId,
    };
    D3D12_SET_PROGRAM_DESC programDesc = {
        .Type = D3D12_PROGRAM_TYPE_WORK_GRAPH,
        .WorkGraph = workgraphDesc,
    };
    COM_CALL(SetProgram, cmd, &programDesc);

    D3D12_DISPATCH_GRAPH_DESC    dispatchDesc = { 0 };
    const DispatchGraphInputType input = pDesc->mInputType;
    switch (input)
    {
    case DISPATCH_GRAPH_INPUT_CPU:
    {
        dispatchDesc.Mode = D3D12_DISPATCH_MODE_NODE_CPU_INPUT;
        dispatchDesc.NodeCPUInput.NumRecords = 1;
        dispatchDesc.NodeCPUInput.pRecords = pDesc->pInput;
        dispatchDesc.NodeCPUInput.RecordStrideInBytes = pDesc->mInputStride;
        break;
    }
    case DISPATCH_GRAPH_INPUT_GPU:
    {
        dispatchDesc.Mode = D3D12_DISPATCH_MODE_NODE_GPU_INPUT;
        dispatchDesc.NodeGPUInput = pDesc->pInputBuffer->mDx.mGpuAddress + pDesc->mInputBufferOffset;
        break;
    }
    default:
        ASSERTFAIL("Invalid DispatchGraphInputType");
        break;
    }

    COM_CALL(DispatchGraph, cmd, &dispatchDesc);

    SAFE_RELEASE(cmd);
}
#endif
/************************************************************************/
// Query Heap Implementation
/************************************************************************/
void getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
    ASSERT(pQueue);
    ASSERT(pFrequency);

    UINT64 freq = 0;
    CHECK_HRESULT(hook_GetTimestampFrequency(pQueue->mDx.pQueue, &freq));
    *pFrequency = (double)freq;
}

void initQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppQueryPool);

    QueryPool* pQueryPool = (QueryPool*)tf_calloc(1, sizeof(QueryPool));
    ASSERT(pQueryPool);

    const uint32_t queryCount = pDesc->mQueryCount * (QUERY_TYPE_TIMESTAMP == pDesc->mType ? 2 : 1);

    pQueryPool->mDx.mType = ToDX12QueryType(pDesc->mType);
    pQueryPool->mCount = queryCount;
    pQueryPool->mStride = ToQueryWidth(pDesc->mType);

    D3D12_QUERY_HEAP_DESC desc = {
        .Count = queryCount,
        .NodeMask = util_calculate_node_mask(pRenderer, pDesc->mNodeIndex),
        .Type = ToDX12QueryHeapType(pDesc->mType),
    };
    COM_CALL(CreateQueryHeap, pRenderer->mDx.pDevice, &desc, IID_REF(ID3D12QueryHeap_Copy), &pQueryPool->mDx.pQueryHeap);
    SetObjectName((ID3D12Object*)pQueryPool->mDx.pQueryHeap, pDesc->pName); // -V1027

    BufferDesc bufDesc = {
        .mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
        .mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT,
        .mElementCount = queryCount,
        .mSize = queryCount * pQueryPool->mStride,
        .mStructStride = pQueryPool->mStride,
        .pName = pDesc->pName,
        .mNodeIndex = pDesc->mNodeIndex,
        .mStartState = RESOURCE_STATE_COPY_DEST,
    };
    addBuffer(pRenderer, &bufDesc, &pQueryPool->mDx.pReadbackBuffer);

    *ppQueryPool = pQueryPool;
}

void exitQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
    UNREF_PARAM(pRenderer);
    SAFE_RELEASE(pQueryPool->mDx.pQueryHeap);
    removeBuffer(pRenderer, pQueryPool->mDx.pReadbackBuffer);

    SAFE_FREE(pQueryPool);
}

void cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    const D3D12_QUERY_TYPE type = pQueryPool->mDx.mType;
    switch (type)
    {
    case D3D12_QUERY_TYPE_TIMESTAMP:
    {
        const uint32_t index = pQuery->mIndex * 2;
        hook_EndQuery(pCmd->mDx.pCmdList, pQueryPool->mDx.pQueryHeap, type, index);
        break;
    }
    case D3D12_QUERY_TYPE_OCCLUSION:
    {
#if defined(XBOX)
        ASSERT(pCmd->mDx.mSampleCount != 0);
        hook_fill_occlusion_query(pCmd, pCmd->mDx.mSampleCount);
#endif
        const uint32_t index = pQuery->mIndex;
        hook_BeginQuery(pCmd->mDx.pCmdList, pQueryPool->mDx.pQueryHeap, type, index);
        break;
    }
    case D3D12_QUERY_TYPE_PIPELINE_STATISTICS:
    {
        const uint32_t index = pQuery->mIndex;
        hook_BeginQuery(pCmd->mDx.pCmdList, pQueryPool->mDx.pQueryHeap, type, index);
        break;
    }
    default:
        ASSERT(false && "Not implemented");
    }
}

void cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    const D3D12_QUERY_TYPE type = pQueryPool->mDx.mType;
    uint32_t               index = pQuery->mIndex * 2 + 1;
    switch (type)
    {
    case D3D12_QUERY_TYPE_TIMESTAMP:
    {
        index = pQuery->mIndex * 2 + 1;
        hook_EndQuery(pCmd->mDx.pCmdList, pQueryPool->mDx.pQueryHeap, type, index);
        break;
    }
    case D3D12_QUERY_TYPE_OCCLUSION:
    {
        hook_fill_occlusion_query(pCmd, 0);

        index = pQuery->mIndex;
        hook_EndQuery(pCmd->mDx.pCmdList, pQueryPool->mDx.pQueryHeap, type, index);
        break;
    }
    case D3D12_QUERY_TYPE_PIPELINE_STATISTICS:
    {
        index = pQuery->mIndex;
        hook_EndQuery(pCmd->mDx.pCmdList, pQueryPool->mDx.pQueryHeap, type, index);
        break;
    }
    default:
        ASSERT(false && "Not implemented");
    }
}

void cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
    ASSERT(pCmd);
    ASSERT(pQueryPool);
    ASSERT(queryCount);

    hook_pre_resolve_query(pCmd);

    const uint32_t internalQueryCount = (D3D12_QUERY_TYPE_TIMESTAMP == pQueryPool->mDx.mType ? 2 : 1);

    hook_ResolveQueryData(pCmd->mDx.pCmdList, pQueryPool->mDx.pQueryHeap, pQueryPool->mDx.mType, startQuery * internalQueryCount,
                          queryCount * internalQueryCount, pQueryPool->mDx.pReadbackBuffer->mDx.pResource,
                          (uint64_t)startQuery * internalQueryCount * pQueryPool->mStride);
}

void cmdResetQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(pQueryPool);
    UNREF_PARAM(startQuery);
    UNREF_PARAM(queryCount);
}

void getQueryData(Renderer* pRenderer, QueryPool* pQueryPool, uint32_t queryIndex, QueryData* pOutData)
{
    ASSERT(pRenderer);
    ASSERT(pQueryPool);
    ASSERT(pOutData);

    const D3D12_QUERY_TYPE type = pQueryPool->mDx.mType;
    *pOutData = (QueryData){ 0 };
    pOutData->mValid = true;

    const uint32_t queryCount = (D3D12_QUERY_TYPE_TIMESTAMP == pQueryPool->mDx.mType ? 2 : 1);
    ReadRange      range = {
        .mOffset = queryIndex * queryCount * pQueryPool->mStride,
        .mSize = queryCount * pQueryPool->mStride,
    };
    mapBuffer(pRenderer, pQueryPool->mDx.pReadbackBuffer, &range);
    uint64_t* queries = (uint64_t*)((uint8_t*)pQueryPool->mDx.pReadbackBuffer->pCpuMappedAddress + range.mOffset);

    switch (type)
    {
    case D3D12_QUERY_TYPE_TIMESTAMP:
    {
        pOutData->mBeginTimestamp = queries[0];
        pOutData->mEndTimestamp = queries[1];
        break;
    }
    case D3D12_QUERY_TYPE_OCCLUSION:
    {
        pOutData->mOcclusionCounts = queries[0];
        break;
    }
    case D3D12_QUERY_TYPE_PIPELINE_STATISTICS:
    {
        COMPILE_ASSERT(sizeof(pOutData->mPipelineStats) == sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS));
        memcpy(&pOutData->mPipelineStats, queries, sizeof(pOutData->mPipelineStats));
        break;
    }
    default:
        ASSERT(false && "Not implemented");
    }

    unmapBuffer(pRenderer, pQueryPool->mDx.pReadbackBuffer);
}
/************************************************************************/
// Memory Stats Implementation
/************************************************************************/
void logMemoryStats(Renderer* pRenderer) { D3D12MA_BuildStatsString(pRenderer->mDx.pResourceAllocator, TRUE); }

void calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
{
    D3D12MA_CalculateMemoryUse(pRenderer->mDx.pResourceAllocator, usedBytes, totalAllocatedBytes);
}
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    PIX_BeginEvent(pCmd->mDx.pCmdList, r, g, b, pName);
#else
    UNREF_PARAM(pCmd);
    UNREF_PARAM(r);
    UNREF_PARAM(g);
    UNREF_PARAM(b);
    UNREF_PARAM(pName);
#endif // ENABLE_GRAPHICS_DEBUG_ANNOTATION
}

void cmdEndDebugMarker(Cmd* pCmd)
{
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    PIX_EndEvent(pCmd->mDx.pCmdList);
#else
    UNREF_PARAM(pCmd);
#endif // ENABLE_GRAPHICS_DEBUG_ANNOTATION
}

void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    PIX_SetMarker(pCmd->mDx.pCmdList, r, g, b, pName);
#else
    UNREF_PARAM(pCmd);
    UNREF_PARAM(r);
    UNREF_PARAM(g);
    UNREF_PARAM(b);
    UNREF_PARAM(pName);
#endif // ENABLE_GRAPHICS_DEBUG_ANNOTATION
}

void cmdWriteMarker(Cmd* pCmd, const MarkerDesc* pDesc)
{
    ASSERT(pCmd);
    ASSERT(pDesc);
    ASSERT(pDesc->pBuffer);

#if defined(XBOX)
    hook_cmd_write_marker(pCmd, pDesc);
#else
    D3D12_GPU_VIRTUAL_ADDRESS       gpuAddress = COM_CALL(GetGPUVirtualAddress, pDesc->pBuffer->mDx.pResource) + pDesc->mOffset;
    D3D12_WRITEBUFFERIMMEDIATE_MODE wbMode = D3D12_WRITEBUFFERIMMEDIATE_MODE_DEFAULT;
    if (pDesc->mFlags & MARKER_FLAG_WAIT_FOR_WRITE)
    {
        wbMode = D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_OUT;
    }
    D3D12_WRITEBUFFERIMMEDIATE_PARAMETER wbParam = {
        .Dest = gpuAddress,
        .Value = pDesc->mValue,
    };
    ID3D12GraphicsCommandList2* pCmdList2 = (ID3D12GraphicsCommandList2*)pCmd->mDx.pCmdList; // -V1027
    hook_WriteBufferImmediate(pCmdList2, 1, &wbParam, &wbMode);
#endif
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pBuffer);
    UNREF_PARAM(pName);
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    ASSERT(pRenderer);
    ASSERT(pBuffer);
    ASSERT(pName);
    SetObjectName((ID3D12Object*)pBuffer->mDx.pResource, pName); // -V1027
#endif
}

void setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pTexture);
    UNREF_PARAM(pName);
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    ASSERT(pRenderer);
    ASSERT(pTexture);
    ASSERT(pName);
    SetObjectName((ID3D12Object*)pTexture->mDx.pResource, pName); // -V1027
#endif
}

void setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName)
{
    setTextureName(pRenderer, pRenderTarget->pTexture, pName);
}

void setPipelineName(Renderer* pRenderer, Pipeline* pPipeline, const char* pName)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pPipeline);
    UNREF_PARAM(pName);
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    ASSERT(pRenderer);
    ASSERT(pPipeline);
    ASSERT(pName);
    SetObjectName((ID3D12Object*)pPipeline->mDx.pPipelineState, pName); // -V1027
#endif
}

#endif

#endif

/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#if defined(XBOX)
#include "../../../Xbox/Common_3/Graphics/Direct3D12/Direct3D12X.h"
#else
#define IID_ARGS IID_PPV_ARGS
#endif

// Pull in minimal Windows headers
#include <Windows.h>

#include "../Interfaces/IGraphics.h"

#define D3D12MA_IMPLEMENTATION
#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#if defined(XBOX)
#define D3D12MA_DXGI_1_4 0
#endif
#include "../ThirdParty/OpenSource/D3D12MemoryAllocator/Direct3D12MemoryAllocator.h"

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#if defined(XBOX)
#include <pix3.h>
#else
#include "../../OS/ThirdParty/OpenSource/winpixeventruntime/Include/WinPixEventRuntime/pix3.h"
#endif

#include "../ThirdParty/OpenSource/renderdoc/renderdoc_app.h"

#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/RingBuffer.h"
#include "../GPUConfig.h"
#include "../../Utilities/Math/AlgorithmsImpl.h"

#include "Direct3D12CapBuilder.h"
#include "Direct3D12Hooks.h"

#include "../ThirdParty/OpenSource/ags/AgsHelper.h"
#include "../ThirdParty/OpenSource/nvapi/NvApiHelper.h"

#if !defined(_WINDOWS) && !defined(XBOX)
#error "Windows is needed!"
#endif

//
// C++ is the only language supported by D3D12:
//   https://msdn.microsoft.com/en-us/library/windows/desktop/dn899120(v=vs.85).aspx
//
#if !defined(__cplusplus)
#error "D3D12 requires C++! Sorry!"
#endif

#include "../../Utilities/Interfaces/IMemory.h"

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN ((D3D12_GPU_VIRTUAL_ADDRESS)-1)
#define D3D12_REQ_CONSTANT_BUFFER_SIZE (D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16u)
#define D3D12_DESCRIPTOR_ID_NONE ((int32_t)-1)

#define MAX_COMPILE_ARGS 64

extern void
	d3d12_createShaderReflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection);

//stubs for durango because Direct3D12Raytracing.cpp is not used on XBOX
#if defined(D3D12_RAYTRACING_AVAILABLE)
extern void addRaytracingPipeline(const RaytracingPipelineDesc* pDesc, Pipeline** ppPipeline);
extern void fillRaytracingDescriptorHandle(AccelerationStructure* pAccelerationStructure, DxDescriptorID* pOutId);
extern void cmdBindRaytracingPipeline(Cmd* pCmd, Pipeline* pPipeline);
#endif
//Enabling DRED
#if defined(_WIN32) && defined(_DEBUG) && defined(DRED)
#define USE_DRED 1
#endif

DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(void, mapBuffer, Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
DECLARE_RENDERER_FUNCTION(void, unmapBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(
	void, cmdUpdateBuffer, Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
DECLARE_RENDERER_FUNCTION(
	void, cmdUpdateSubresource, Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_RENDERER_FUNCTION(
	void, cmdCopySubresource, Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_RENDERER_FUNCTION(void, addTexture, Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
DECLARE_RENDERER_FUNCTION(void, removeTexture, Renderer* pRenderer, Texture* pTexture)
DECLARE_RENDERER_FUNCTION(void, addVirtualTexture, Cmd* pCmd, const TextureDesc* pDesc, Texture** ppTexture, void* pImageData)
DECLARE_RENDERER_FUNCTION(void, removeVirtualTexture, Renderer* pRenderer, VirtualTexture* pTexture)

// clang-format off
D3D12_BLEND_OP gDx12BlendOpTranslator[BlendMode::MAX_BLEND_MODES] =
{
	D3D12_BLEND_OP_ADD,
	D3D12_BLEND_OP_SUBTRACT,
	D3D12_BLEND_OP_REV_SUBTRACT,
	D3D12_BLEND_OP_MIN,
	D3D12_BLEND_OP_MAX,
};

D3D12_BLEND gDx12BlendConstantTranslator[BlendConstant::MAX_BLEND_CONSTANTS] =
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

D3D12_COMPARISON_FUNC gDx12ComparisonFuncTranslator[CompareMode::MAX_COMPARE_MODES] =
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

D3D12_STENCIL_OP gDx12StencilOpTranslator[StencilOp::MAX_STENCIL_OPS] =
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

const D3D12_COMMAND_QUEUE_PRIORITY gDx12QueuePriorityTranslator[QueuePriority::MAX_QUEUE_PRIORITY]
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
#include "../ThirdParty/OpenSource/DirectXShaderCompiler/inc/dxcapi.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
#endif

#define SAFE_FREE(p_var)         \
	if ((p_var))                 \
	{                            \
		tf_free((void*)(p_var)); \
		p_var = NULL;            \
	}

#if defined(__cplusplus)
#define DECLARE_ZERO(type, var) type var = {};
#else
#define DECLARE_ZERO(type, var) type var = { 0 };
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p_var) \
	if (p_var)              \
	{                       \
		p_var->Release();   \
		p_var = NULL;       \
	}
#endif

#define CALC_SUBRESOURCE_INDEX(MipSlice, ArraySlice, PlaneSlice, MipLevels, ArraySize) \
	((MipSlice) + ((ArraySlice) * (MipLevels)) + ((PlaneSlice) * (MipLevels) * (ArraySize)))

// Internal utility functions (may become external one day)
uint64_t                    util_dx12_determine_storage_counter_offset(uint64_t buffer_size);
DXGI_FORMAT                 util_to_dx12_uav_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx12_dsv_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx12_srv_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx12_stencil_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx12_swapchain_format(TinyImageFormat format);
D3D12_SHADER_VISIBILITY     util_to_dx12_shader_visibility(ShaderStage stages);
D3D12_DESCRIPTOR_RANGE_TYPE util_to_dx12_descriptor_range(DescriptorType type);
D3D12_RESOURCE_STATES       util_to_dx12_resource_state(ResourceState state);
D3D12_FILTER
							  util_to_dx12_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool aniso, bool comparisonFilterEnabled);
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

DescriptorHeapProperties gCpuDescriptorHeapProperties[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] =
{
	{ 1024 * 256, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },    // CBV SRV UAV
	{ 2048, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },          // Sampler
	{ 512, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },           // RTV
	{ 512, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },           // DSV
};

typedef struct NullDescriptors
{
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
	ID3D12DescriptorHeap*        pHeap;
	/// Lock for multi-threaded descriptor allocations
	Mutex                        mMutex;
	ID3D12Device*                pDevice;
	/// Start position in the heap
	D3D12_CPU_DESCRIPTOR_HANDLE  mStartCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE  mStartGpuHandle;
	// Bitmask to track free regions (set bit means occupied)
	uint32_t*                    pFlags;
	/// Description
	D3D12_DESCRIPTOR_HEAP_TYPE   mType;
	uint32_t                     mNumDescriptors;
	/// Descriptor Increment Size
	uint32_t                     mDescriptorSize;
	// Usage
	uint32_t                     mUsedDescriptors;
} DescriptorHeap;

typedef struct DescriptorIndexMap
{
	char* key;
	uint32_t value;
}DescriptorIndexMap;

/************************************************************************/
// Static Descriptor Heap Implementation
/************************************************************************/
static void add_descriptor_heap(ID3D12Device* pDevice, const D3D12_DESCRIPTOR_HEAP_DESC* pDesc, DescriptorHeap** ppDescHeap)
{
	uint32_t numDescriptors = pDesc->NumDescriptors;
	hook_modify_descriptor_heap_size(pDesc->Type, &numDescriptors);

	// Keep 32 aligned for easy remove
	numDescriptors = round_up(numDescriptors, 32);

	const size_t sizeInBytes = (numDescriptors / 32) * sizeof(uint32_t);

	DescriptorHeap* pHeap = (DescriptorHeap*)tf_calloc(1, sizeof(*pHeap) + sizeInBytes);
	pHeap->pFlags = (uint32_t*)(pHeap + 1);
	pHeap->pDevice = pDevice;

	initMutex(&pHeap->mMutex);

	D3D12_DESCRIPTOR_HEAP_DESC desc = *pDesc;
	desc.NumDescriptors = numDescriptors;

	CHECK_HRESULT(pDevice->CreateDescriptorHeap(&desc, IID_ARGS(&pHeap->pHeap)));

	pHeap->mStartCpuHandle = pHeap->pHeap->GetCPUDescriptorHandleForHeapStart();
	if (desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
	{
		pHeap->mStartGpuHandle = pHeap->pHeap->GetGPUDescriptorHandleForHeapStart();
	}
	pHeap->mNumDescriptors = desc.NumDescriptors;
	pHeap->mType = desc.Type;
	pHeap->mDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(pHeap->mType);

	*ppDescHeap = pHeap;
}

static void reset_descriptor_heap(DescriptorHeap* pHeap)
{
	memset(pHeap->pFlags, 0, (pHeap->mNumDescriptors / 32) * sizeof(uint32_t));
	pHeap->mUsedDescriptors = 0;
}

static void remove_descriptor_heap(DescriptorHeap* pHeap)
{
	SAFE_RELEASE(pHeap->pHeap);
	destroyMutex(&pHeap->mMutex);
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
		const uint32_t i = id / 32;
		const uint32_t mask = ~(1 << (id % 32));
		pHeap->pFlags[i] &= mask;
	}

	pHeap->mUsedDescriptors -= count;
}

void return_descriptor_handles(DescriptorHeap* pHeap, DxDescriptorID handle, uint32_t count)
{
	MutexLock lock(pHeap->mMutex);
	return_descriptor_handles_unlocked(pHeap, handle, count);
}

static DxDescriptorID consume_descriptor_handles(DescriptorHeap* pHeap, uint32_t descriptorCount)
{
	if (!descriptorCount)
	{
		return D3D12_DESCRIPTOR_ID_NONE;
	}

	MutexLock lock(pHeap->mMutex);

	DxDescriptorID result = D3D12_DESCRIPTOR_ID_NONE;
	DxDescriptorID firstResult = D3D12_DESCRIPTOR_ID_NONE;
	uint32_t foundCount = 0;

	for (uint32_t i = 0; i < pHeap->mNumDescriptors / 32; ++i)
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

		for (int32_t j = 0, mask = 1; j < 32; ++j, mask <<= 1)
		{
			if (!(flag & mask))
			{
				pHeap->pFlags[i] |= mask;
				result = i * 32 + j;

				ASSERT(result != D3D12_DESCRIPTOR_ID_NONE && "Out of descriptors");

				if (D3D12_DESCRIPTOR_ID_NONE == firstResult)
				{
					firstResult = result;
				}

				++foundCount;
				++pHeap->mUsedDescriptors;

				if (foundCount == descriptorCount)
				{
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

	ASSERT(result != D3D12_DESCRIPTOR_ID_NONE && "Out of descriptors");
	return firstResult;
}

static inline FORGE_CONSTEXPR D3D12_CPU_DESCRIPTOR_HANDLE descriptor_id_to_cpu_handle(DescriptorHeap* pHeap, DxDescriptorID id)
{
	return { pHeap->mStartCpuHandle.ptr + id * pHeap->mDescriptorSize };
}

static inline FORGE_CONSTEXPR D3D12_GPU_DESCRIPTOR_HANDLE descriptor_id_to_gpu_handle(DescriptorHeap* pHeap, DxDescriptorID id)
{
	return { pHeap->mStartGpuHandle.ptr + id * pHeap->mDescriptorSize };
}

static void copy_descriptor_handle(DescriptorHeap* pSrcHeap, DxDescriptorID srcId, DescriptorHeap* pDstHeap, DxDescriptorID dstId)
{
	ASSERT(pSrcHeap->mType == pDstHeap->mType);
	D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = descriptor_id_to_cpu_handle(pSrcHeap, srcId);
	D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = descriptor_id_to_cpu_handle(pDstHeap, dstId);
	pSrcHeap->pDevice->CopyDescriptorsSimple(1, dstHandle, srcHandle, pSrcHeap->mType);
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
constexpr D3D12_DEPTH_STENCIL_DESC util_to_depth_desc(const DepthStateDesc* pDesc)
{
	ASSERT(pDesc->mDepthFunc < CompareMode::MAX_COMPARE_MODES);
	ASSERT(pDesc->mStencilFrontFunc < CompareMode::MAX_COMPARE_MODES);
	ASSERT(pDesc->mStencilFrontFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mDepthFrontFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mStencilFrontPass < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mStencilBackFunc < CompareMode::MAX_COMPARE_MODES);
	ASSERT(pDesc->mStencilBackFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mDepthBackFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mStencilBackPass < StencilOp::MAX_STENCIL_OPS);

	D3D12_DEPTH_STENCIL_DESC ret = {};
	ret.DepthEnable = (BOOL)pDesc->mDepthTest;
	ret.DepthWriteMask = pDesc->mDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	ret.DepthFunc = gDx12ComparisonFuncTranslator[pDesc->mDepthFunc];
	ret.StencilEnable = (BOOL)pDesc->mStencilTest;
	ret.StencilReadMask = pDesc->mStencilReadMask;
	ret.StencilWriteMask = pDesc->mStencilWriteMask;
	ret.BackFace.StencilFunc = gDx12ComparisonFuncTranslator[pDesc->mStencilBackFunc];
	ret.FrontFace.StencilFunc = gDx12ComparisonFuncTranslator[pDesc->mStencilFrontFunc];
	ret.BackFace.StencilDepthFailOp = gDx12StencilOpTranslator[pDesc->mDepthBackFail];
	ret.FrontFace.StencilDepthFailOp = gDx12StencilOpTranslator[pDesc->mDepthFrontFail];
	ret.BackFace.StencilFailOp = gDx12StencilOpTranslator[pDesc->mStencilBackFail];
	ret.FrontFace.StencilFailOp = gDx12StencilOpTranslator[pDesc->mStencilFrontFail];
	ret.BackFace.StencilPassOp = gDx12StencilOpTranslator[pDesc->mStencilBackPass];
	ret.FrontFace.StencilPassOp = gDx12StencilOpTranslator[pDesc->mStencilFrontPass];

	return ret;
}

constexpr D3D12_BLEND_DESC util_to_blend_desc(const BlendStateDesc* pDesc)
{
	int blendDescIndex = 0;
#if defined(ENABLE_GRAPHICS_DEBUG)

	for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
	{
		if (pDesc->mRenderTargetMask & (1 << i))
		{
			ASSERT(pDesc->mSrcFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mDstFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mSrcAlphaFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mDstAlphaFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mBlendModes[blendDescIndex] < BlendMode::MAX_BLEND_MODES);
			ASSERT(pDesc->mBlendAlphaModes[blendDescIndex] < BlendMode::MAX_BLEND_MODES);
		}

		if (pDesc->mIndependentBlend)
			++blendDescIndex;
	}

	blendDescIndex = 0;
#endif

	D3D12_BLEND_DESC ret = {};
	ret.AlphaToCoverageEnable = (BOOL)pDesc->mAlphaToCoverage;
	ret.IndependentBlendEnable = TRUE;
	for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; i++)
	{
		if (pDesc->mRenderTargetMask & (1 << i))
		{
			BOOL blendEnable =
				(gDx12BlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]] != D3D12_BLEND_ONE ||
				 gDx12BlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]] != D3D12_BLEND_ZERO ||
				 gDx12BlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]] != D3D12_BLEND_ONE ||
				 gDx12BlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]] != D3D12_BLEND_ZERO);

			ret.RenderTarget[i].BlendEnable = blendEnable;
			ret.RenderTarget[i].RenderTargetWriteMask = (UINT8)pDesc->mMasks[blendDescIndex];
			ret.RenderTarget[i].BlendOp = gDx12BlendOpTranslator[pDesc->mBlendModes[blendDescIndex]];
			ret.RenderTarget[i].SrcBlend = gDx12BlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]];
			ret.RenderTarget[i].DestBlend = gDx12BlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]];
			ret.RenderTarget[i].BlendOpAlpha = gDx12BlendOpTranslator[pDesc->mBlendAlphaModes[blendDescIndex]];
			ret.RenderTarget[i].SrcBlendAlpha = gDx12BlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]];
			ret.RenderTarget[i].DestBlendAlpha = gDx12BlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]];
		}

		if (pDesc->mIndependentBlend)
			++blendDescIndex;
	}

	return ret;
}

constexpr D3D12_RASTERIZER_DESC util_to_rasterizer_desc(const RasterizerStateDesc* pDesc)
{
	ASSERT(pDesc->mFillMode < FillMode::MAX_FILL_MODES);
	ASSERT(pDesc->mCullMode < CullMode::MAX_CULL_MODES);
	ASSERT(pDesc->mFrontFace == FRONT_FACE_CCW || pDesc->mFrontFace == FRONT_FACE_CW);

	D3D12_RASTERIZER_DESC ret = {};
	ret.FillMode = gDx12FillModeTranslator[pDesc->mFillMode];
	ret.CullMode = gDx12CullModeTranslator[pDesc->mCullMode];
	ret.FrontCounterClockwise = pDesc->mFrontFace == FRONT_FACE_CCW;
	ret.DepthBias = pDesc->mDepthBias;
	ret.DepthBiasClamp = 0.0f;
	ret.SlopeScaledDepthBias = pDesc->mSlopeScaledDepthBias;
	ret.DepthClipEnable = !pDesc->mDepthClampEnable;
	ret.MultisampleEnable = pDesc->mMultiSample ? TRUE : FALSE;
	ret.AntialiasedLineEnable = FALSE;
	ret.ForcedSampleCount = 0;
	ret.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	return ret;
}
/************************************************************************/
/************************************************************************/

const DescriptorInfo* d3d12_get_descriptor(const RootSignature* pRootSignature, const char* pResName)
{
	const DescriptorIndexMap* pNode = shgetp_null(pRootSignature->pDescriptorNameToIndexMap, pResName);

	if (pNode)
	{
		return &pRootSignature->pDescriptors[pNode->value];
	}
	else
	{
		LOGF(LogLevel::eERROR, "Invalid descriptor param (%s)", pResName);
		return NULL;
	}
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
// Proxy log callback
static void internal_log(LogLevel level, const char* msg, const char* component)
{
	LOGF(level, "%s ( %s )", component, msg);
}

void add_srv(
	Renderer* pRenderer, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc, DxDescriptorID* pInOutId)
{
	DescriptorHeap* heap = pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
	if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
	{
		*pInOutId = consume_descriptor_handles(heap, 1);
	}
	pRenderer->mD3D12.pDxDevice->CreateShaderResourceView(pResource, pSrvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void add_uav(
	Renderer* pRenderer, ID3D12Resource* pResource, ID3D12Resource* pCounterResource,
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc, DxDescriptorID* pInOutId)
{
	DescriptorHeap* heap = pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
	if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
	{
		*pInOutId = consume_descriptor_handles(heap, 1);
	}
	pRenderer->mD3D12.pDxDevice->CreateUnorderedAccessView(pResource, pCounterResource, pUavDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void add_cbv(Renderer* pRenderer, const D3D12_CONSTANT_BUFFER_VIEW_DESC* pCbvDesc, DxDescriptorID* pInOutId)
{
	DescriptorHeap* heap = pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
	if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
	{
		*pInOutId = consume_descriptor_handles(heap, 1);
	}
	pRenderer->mD3D12.pDxDevice->CreateConstantBufferView(pCbvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void add_rtv(
	Renderer* pRenderer, ID3D12Resource* pResource, DXGI_FORMAT format, uint32_t mipSlice, uint32_t arraySlice,
	DxDescriptorID* pInOutId)
{
	DescriptorHeap* heap = pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
	if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
	{
		*pInOutId = consume_descriptor_handles(heap, 1);
	}
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	D3D12_RESOURCE_DESC           desc = pResource->GetDesc();
	D3D12_RESOURCE_DIMENSION      type = desc.Dimension;

	rtvDesc.Format = format;

	switch (type)
	{
		case D3D12_RESOURCE_DIMENSION_BUFFER: break;
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
		default: break;
	}

	pRenderer->mD3D12.pDxDevice->CreateRenderTargetView(pResource, &rtvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void add_dsv(
	Renderer* pRenderer, ID3D12Resource* pResource, DXGI_FORMAT format, uint32_t mipSlice, uint32_t arraySlice,
	DxDescriptorID* pInOutId)
{
	DescriptorHeap* heap = pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
	if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
	{
		*pInOutId = consume_descriptor_handles(heap, 1);
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	D3D12_RESOURCE_DESC           desc = pResource->GetDesc();
	D3D12_RESOURCE_DIMENSION      type = desc.Dimension;

	dsvDesc.Format = format;

	switch (type)
	{
		case D3D12_RESOURCE_DIMENSION_BUFFER: break;
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
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D: ASSERT(false && "Cannot create 3D Depth Stencil"); break;
		default: break;
	}

	pRenderer->mD3D12.pDxDevice->CreateDepthStencilView(pResource, &dsvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void add_sampler(Renderer* pRenderer, const D3D12_SAMPLER_DESC* pSamplerDesc, DxDescriptorID* pInOutId)
{
	DescriptorHeap* heap = pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
	if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
	{
		*pInOutId = consume_descriptor_handles(heap, 1);
	}
	pRenderer->mD3D12.pDxDevice->CreateSampler(pSamplerDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

D3D12_DEPTH_STENCIL_DESC gDefaultDepthDesc = {};
D3D12_BLEND_DESC         gDefaultBlendDesc = {};
D3D12_RASTERIZER_DESC    gDefaultRasterizerDesc = {};

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
	D3D12_SAMPLER_DESC samplerDesc = {};
	samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	add_sampler(pRenderer, &samplerDesc, &pRenderer->pNullDescriptors->mNullSampler);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8_UINT;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R8_UINT;

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
	add_srv(pRenderer, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_1D]);
	add_uav(pRenderer, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->mNullTextureUAV[TEXTURE_DIM_1D]);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	add_srv(pRenderer, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_2D]);
	add_uav(pRenderer, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->mNullTextureUAV[TEXTURE_DIM_2D]);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
	add_srv(pRenderer, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_2DMS]);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	add_srv(pRenderer, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_3D]);
	add_uav(pRenderer, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->mNullTextureUAV[TEXTURE_DIM_3D]);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
	add_srv(pRenderer, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_1D_ARRAY]);
	add_uav(pRenderer, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->mNullTextureUAV[TEXTURE_DIM_1D_ARRAY]);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	add_srv(pRenderer, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_2D_ARRAY]);
	add_uav(pRenderer, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->mNullTextureUAV[TEXTURE_DIM_2D_ARRAY]);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
	add_srv(pRenderer, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_2DMS_ARRAY]);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	add_srv(pRenderer, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_CUBE]);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
	add_srv(pRenderer, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullTextureSRV[TEXTURE_DIM_CUBE_ARRAY]);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	add_srv(pRenderer, NULL, &srvDesc, &pRenderer->pNullDescriptors->mNullBufferSRV);
	add_uav(pRenderer, NULL, NULL, &uavDesc, &pRenderer->pNullDescriptors->mNullBufferUAV);
	add_cbv(pRenderer, NULL, &pRenderer->pNullDescriptors->mNullBufferCBV);

	BlendStateDesc blendStateDesc = {};
	blendStateDesc.mDstAlphaFactors[0] = BC_ZERO;
	blendStateDesc.mDstFactors[0] = BC_ZERO;
	blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
	blendStateDesc.mSrcFactors[0] = BC_ONE;
	blendStateDesc.mMasks[0] = ALL;
	blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
	blendStateDesc.mIndependentBlend = false;
	gDefaultBlendDesc = util_to_blend_desc(&blendStateDesc);

	DepthStateDesc depthStateDesc = {};
	depthStateDesc.mDepthFunc = CMP_LEQUAL;
	depthStateDesc.mDepthTest = false;
	depthStateDesc.mDepthWrite = false;
	depthStateDesc.mStencilBackFunc = CMP_ALWAYS;
	depthStateDesc.mStencilFrontFunc = CMP_ALWAYS;
	depthStateDesc.mStencilReadMask = 0xFF;
	depthStateDesc.mStencilWriteMask = 0xFF;
	gDefaultDepthDesc = util_to_depth_desc(&depthStateDesc);

	RasterizerStateDesc rasterizerStateDesc = {};
	rasterizerStateDesc.mCullMode = CULL_MODE_BACK;
	gDefaultRasterizerDesc = util_to_rasterizer_desc(&rasterizerStateDesc);
}

static void remove_default_resources(Renderer* pRenderer)
{
	return_descriptor_handles(pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER], pRenderer->pNullDescriptors->mNullSampler, 1);

	DescriptorHeap* heap = pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
	for (uint32_t i = 0; i < TEXTURE_DIM_COUNT; ++i)
	{
		return_descriptor_handles(heap, pRenderer->pNullDescriptors->mNullTextureSRV[i], 1);
		return_descriptor_handles(heap, pRenderer->pNullDescriptors->mNullTextureUAV[i], 1);
	}
	return_descriptor_handles(heap, pRenderer->pNullDescriptors->mNullBufferSRV, 1);
	return_descriptor_handles(heap, pRenderer->pNullDescriptors->mNullBufferUAV, 1);
	return_descriptor_handles(heap, pRenderer->pNullDescriptors->mNullBufferCBV, 1);

	SAFE_FREE(pRenderer->pNullDescriptors);
}

typedef enum GpuVendor
{
	GPU_VENDOR_NVIDIA,
	GPU_VENDOR_AMD,
	GPU_VENDOR_INTEL,
	GPU_VENDOR_UNKNOWN,
	GPU_VENDOR_COUNT,
} GpuVendor;

static uint32_t gRootSignatureDWORDS[GpuVendor::GPU_VENDOR_COUNT] = {
	64U,
	13U,
	64U,
	64U,
};

#define VENDOR_ID_NVIDIA 0x10DE
#define VENDOR_ID_AMD 0x1002
#define VENDOR_ID_AMD_1 0x1022
#define VENDOR_ID_INTEL 0x163C
#define VENDOR_ID_INTEL_1 0x8086
#define VENDOR_ID_INTEL_2 0x8087

static GpuVendor util_to_internal_gpu_vendor(UINT vendorId)
{
	if (vendorId == VENDOR_ID_NVIDIA)
		return GPU_VENDOR_NVIDIA;
	else if (vendorId == VENDOR_ID_AMD || vendorId == VENDOR_ID_AMD_1)
		return GPU_VENDOR_AMD;
	else if (vendorId == VENDOR_ID_INTEL || vendorId == VENDOR_ID_INTEL_1 || vendorId == VENDOR_ID_INTEL_2)
		return GPU_VENDOR_INTEL;
	else
		return GPU_VENDOR_UNKNOWN;
}
/************************************************************************/
// Internal Root Signature Functions
/************************************************************************/
typedef struct RootParameter
{
	ShaderResource mShaderResource;
	DescriptorInfo* pDescriptorInfo;
}RootParameter;

// For sort
// sort table by type (CBV/SRV/UAV) by register by space
static bool lessRootParameter(const RootParameter* pLhs, const RootParameter* pRhs)
{
	// swap operands to achieve descending order
	int results[3] = {
		(int)((int64_t)pRhs->pDescriptorInfo->mType - (int64_t)pLhs->pDescriptorInfo->mType),
		(int)((int64_t)pRhs->mShaderResource.set    - (int64_t)pLhs->mShaderResource.set),
		(int)((int64_t)pRhs->mShaderResource.reg - (int64_t)pLhs->mShaderResource.reg),
	};

	for (int i = 0; i < 3; ++i)
	{
		if (results[i])
			return results[i] < 0;
	}
	return false;
}

DEFINE_SORT_ALGORITHMS_FOR_TYPE(static, RootParameter, lessRootParameter)


#undef CREATE_TEMP_ROOT_PARAM
#undef DESTROY_TEMP_ROOT_PARAM
#undef COPY_ROOT_PARAM


typedef struct DescriptorInfoIndexNode
{
	DescriptorInfo*	key;
	uint32_t		value;
}DescriptorInfoIndexNode;
typedef struct d3d12_UpdateFrequencyLayoutInfo
{
	// stb_ds array
	RootParameter*				mCbvSrvUavTable;
	// stb_ds array
	RootParameter*				mSamplerTable;
	// stb_ds array
	RootParameter*				mRootDescriptorParams;
	// stb_ds array
	RootParameter*				mRootConstants;
	// stb_ds hash map
	DescriptorInfoIndexNode*	mDescriptorIndexMap;
} UpdateFrequencyLayoutInfo;

/// Calculates the total size of the root signature (in DWORDS) from the input layouts
uint32_t calculate_root_signature_size(UpdateFrequencyLayoutInfo* pLayouts, uint32_t numLayouts)
{
	uint32_t size = 0;
	for (uint32_t i = 0; i < numLayouts; ++i)
	{
		if (arrlen(pLayouts[i].mCbvSrvUavTable))
			size += gDescriptorTableDWORDS;
		if (arrlen(pLayouts[i].mSamplerTable))
			size += gDescriptorTableDWORDS;

		for (ptrdiff_t c = 0; c < arrlen(pLayouts[i].mRootDescriptorParams); ++c)
		{
			size += gRootDescriptorDWORDS;
		}
		for (ptrdiff_t c = 0; c < arrlen(pLayouts[i].mRootConstants); ++c)
		{
			DescriptorInfo* pDesc = pLayouts[i].mRootConstants[c].pDescriptorInfo;
			size += pDesc->mSize;
		}
	}

	return size;
}

/// Creates a root descriptor table parameter from the input table layout for root signature version 1_1
void create_descriptor_table(
	uint32_t numDescriptors, RootParameter* tableRef, D3D12_DESCRIPTOR_RANGE1* pRange, D3D12_ROOT_PARAMETER1* pRootParam)
{
	pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	ShaderStage stageCount = SHADER_STAGE_NONE;
	for (uint32_t i = 0; i < numDescriptors; ++i)
	{
		const ShaderResource* res = &tableRef[i].mShaderResource;
		const DescriptorInfo* desc = tableRef[i].pDescriptorInfo;
		pRange[i].BaseShaderRegister = res->reg;
		pRange[i].RegisterSpace = res->set;
		pRange[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
		pRange[i].NumDescriptors = desc->mSize;
		pRange[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		pRange[i].RangeType = util_to_dx12_descriptor_range((DescriptorType)desc->mType);
		stageCount |= res->used_stages;
	}
	pRootParam->ShaderVisibility = util_to_dx12_shader_visibility(stageCount);
	pRootParam->DescriptorTable.NumDescriptorRanges = numDescriptors;
	pRootParam->DescriptorTable.pDescriptorRanges = pRange;
}

/// Creates a root descriptor / root constant parameter for root signature version 1_1
void create_root_descriptor(const RootParameter* pDesc, D3D12_ROOT_PARAMETER1* pRootParam)
{
	pRootParam->ShaderVisibility = util_to_dx12_shader_visibility(pDesc->mShaderResource.used_stages);
	pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pRootParam->Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
	pRootParam->Descriptor.ShaderRegister = pDesc->mShaderResource.reg;
	pRootParam->Descriptor.RegisterSpace = pDesc->mShaderResource.set;
}

void create_root_constant(const RootParameter* pDesc, D3D12_ROOT_PARAMETER1* pRootParam)
{
	pRootParam->ShaderVisibility = util_to_dx12_shader_visibility(pDesc->mShaderResource.used_stages);
	pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pRootParam->Constants.Num32BitValues = pDesc->pDescriptorInfo->mSize;
	pRootParam->Constants.ShaderRegister = pDesc->mShaderResource.reg;
	pRootParam->Constants.RegisterSpace = pDesc->mShaderResource.set;
}
/************************************************************************/
// Internal utility functions
/************************************************************************/
D3D12_FILTER
	util_to_dx12_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool aniso, bool comparisonFilterEnabled)
{
	if (aniso)
		return (comparisonFilterEnabled ? D3D12_FILTER_COMPARISON_ANISOTROPIC : D3D12_FILTER_ANISOTROPIC);

	// control bit : minFilter  magFilter   mipMapMode
	//   point   :   00	  00	   00
	//   linear  :   01	  01	   01
	// ex : trilinear == 010101
	int filter = (minFilter << 4) | (magFilter << 2) | mipMapMode;
	int baseFilter = comparisonFilterEnabled ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_POINT;
	return (D3D12_FILTER)(baseFilter + filter);
}

D3D12_TEXTURE_ADDRESS_MODE util_to_dx12_texture_address_mode(AddressMode addressMode)
{
	switch (addressMode)
	{
		case ADDRESS_MODE_MIRROR: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		case ADDRESS_MODE_REPEAT: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		case ADDRESS_MODE_CLAMP_TO_EDGE: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		case ADDRESS_MODE_CLAMP_TO_BORDER: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE util_to_dx12_primitive_topology_type(PrimitiveTopology topology)
{
	switch (topology)
	{
		case PRIMITIVE_TOPO_POINT_LIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		case PRIMITIVE_TOPO_LINE_LIST:
		case PRIMITIVE_TOPO_LINE_STRIP: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case PRIMITIVE_TOPO_TRI_LIST:
		case PRIMITIVE_TOPO_TRI_STRIP: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		case PRIMITIVE_TOPO_PATCH_LIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		default: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
	}
	return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
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
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;

		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;

		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8X8_UNORM;

		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;

#if defined(ENABLE_GRAPHICS_DEBUG)
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		case DXGI_FORMAT_D16_UNORM: LOGF(LogLevel::eERROR, "Requested a UAV format for a depth stencil format");
#endif

		default: return defaultFormat;
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
		case DXGI_FORMAT_R16_UNORM: return DXGI_FORMAT_D16_UNORM;

		default: return defaultFormat;
	}
}

DXGI_FORMAT util_to_dx12_srv_format(DXGI_FORMAT defaultFormat)
{
	switch (defaultFormat)
	{
			// 32-bit Z w/ Stencil
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

			// No Stencil
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
			return DXGI_FORMAT_R32_FLOAT;

			// 24-bit Z
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

			// 16-bit Z w/o Stencil
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM: return DXGI_FORMAT_R16_UNORM;

		case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;

		default: return defaultFormat;
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
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT: return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

		default: return DXGI_FORMAT_UNKNOWN;
	}
}

DXGI_FORMAT util_to_dx12_swapchain_format(TinyImageFormat const format)
{
	DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;

	// FLIP_DISCARD and FLIP_SEQEUNTIAL swapchain buffers only support these formats
	switch (format)
	{
		case TinyImageFormat_R16G16B16A16_SFLOAT: result = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
		case TinyImageFormat_B8G8R8A8_UNORM:
		case TinyImageFormat_B8G8R8A8_SRGB: result = DXGI_FORMAT_B8G8R8A8_UNORM; break;
		case TinyImageFormat_R8G8B8A8_UNORM:
		case TinyImageFormat_R8G8B8A8_SRGB: result = DXGI_FORMAT_R8G8B8A8_UNORM; break;
		case TinyImageFormat_R10G10B10A2_UNORM: result = DXGI_FORMAT_R10G10B10A2_UNORM; break;
		default: break;
	}

	if (result == DXGI_FORMAT_UNKNOWN)
	{
		LOGF(LogLevel::eERROR, "Image Format (%u) not supported for creating swapchain buffer", (uint32_t)format);
	}

	return result;
}

D3D12_SHADER_VISIBILITY util_to_dx12_shader_visibility(ShaderStage stages)
{
	D3D12_SHADER_VISIBILITY res = D3D12_SHADER_VISIBILITY_ALL;
	uint32_t                stageCount = 0;

	if (stages == SHADER_STAGE_COMP)
	{
		return D3D12_SHADER_VISIBILITY_ALL;
	}
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
#ifdef D3D12_RAYTRACING_AVAILABLE
	if (stages == SHADER_STAGE_RAYTRACING)
	{
		return D3D12_SHADER_VISIBILITY_ALL;
	}
#endif
	ASSERT(stageCount > 0);
	return stageCount > 1 ? D3D12_SHADER_VISIBILITY_ALL : res;
}

D3D12_DESCRIPTOR_RANGE_TYPE util_to_dx12_descriptor_range(DescriptorType type)
{
	switch (type)
	{
		case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case DESCRIPTOR_TYPE_ROOT_CONSTANT: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		case DESCRIPTOR_TYPE_RW_BUFFER:
		case DESCRIPTOR_TYPE_RW_TEXTURE: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case DESCRIPTOR_TYPE_SAMPLER: return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
#ifdef D3D12_RAYTRACING_AVAILABLE
		case DESCRIPTOR_TYPE_RAY_TRACING:
#endif
		case DESCRIPTOR_TYPE_TEXTURE:
		case DESCRIPTOR_TYPE_BUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

		default: ASSERT(false && "Invalid DescriptorInfo Type"); return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	}
}

D3D12_RESOURCE_STATES util_to_dx12_resource_state(ResourceState state)
{
	D3D12_RESOURCE_STATES ret = D3D12_RESOURCE_STATE_COMMON;

	// These states cannot be combined with other states so we just do an == check
	if (state == RESOURCE_STATE_GENERIC_READ)
		return D3D12_RESOURCE_STATE_GENERIC_READ;
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
	if (state & RESOURCE_STATE_DEPTH_READ)
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
	if (state & RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
		ret |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
#endif

#ifdef ENABLE_VRS
	if (state & RESOURCE_STATE_SHADING_RATE_SOURCE)
		ret |= D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE;
#endif

	return ret;
}

D3D12_QUERY_HEAP_TYPE util_to_dx12_query_heap_type(QueryType type)
{
	switch (type)
	{
		case QUERY_TYPE_TIMESTAMP: return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		case QUERY_TYPE_PIPELINE_STATISTICS: return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
		case QUERY_TYPE_OCCLUSION: return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
		default: ASSERT(false && "Invalid query heap type"); return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	}
}

D3D12_QUERY_TYPE util_to_dx12_query_type(QueryType type)
{
	switch (type)
	{
		case QUERY_TYPE_TIMESTAMP: return D3D12_QUERY_TYPE_TIMESTAMP;
		case QUERY_TYPE_PIPELINE_STATISTICS: return D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
		case QUERY_TYPE_OCCLUSION: return D3D12_QUERY_TYPE_OCCLUSION;
		default: ASSERT(false && "Invalid query heap type"); return D3D12_QUERY_TYPE_OCCLUSION;
	}
}

#ifdef ENABLE_VRS
D3D12_SHADING_RATE_COMBINER util_to_dx_shading_rate_combiner(ShadingRateCombiner combiner)
{
	switch (combiner)
	{
		case SHADING_RATE_COMBINER_PASSTHROUGH: return D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
		case SHADING_RATE_COMBINER_OVERRIDE: return D3D12_SHADING_RATE_COMBINER_OVERRIDE;
		case SHADING_RATE_COMBINER_MAX: return D3D12_SHADING_RATE_COMBINER_MAX;
		case SHADING_RATE_COMBINER_SUM: return D3D12_SHADING_RATE_COMBINER_SUM;
		default: ASSERT(false && "Invalid shading rate combiner type"); return D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
	}
}

D3D12_SHADING_RATE util_to_dx_shading_rate(ShadingRate shadingRate)
{
	switch (shadingRate)
	{
		case SHADING_RATE_FULL: return D3D12_SHADING_RATE_1X1;
		case SHADING_RATE_1X2: return D3D12_SHADING_RATE_1X2;
		case SHADING_RATE_2X1: return D3D12_SHADING_RATE_2X1;
		case SHADING_RATE_HALF: return D3D12_SHADING_RATE_2X2;
		case SHADING_RATE_2X4: return D3D12_SHADING_RATE_2X4;
		case SHADING_RATE_4X2: return D3D12_SHADING_RATE_4X2;
		case SHADING_RATE_QUARTER: return D3D12_SHADING_RATE_4X4;
		default: ASSERT(false && "Invalid shading rate"); return D3D12_SHADING_RATE_1X1;
	}
}
#endif

#if !defined(XBOX)
void util_enumerate_gpus(IDXGIFactory6* dxgiFactory, uint32_t* pGpuCount, GpuDesc* gpuDesc, bool* pFoundSoftwareAdapter)
{
	D3D_FEATURE_LEVEL feature_levels[4] =
	{
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
	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapterByGpuPreference(
		i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_ARGS(&adapter));
		++i)
	{
		if (gpuCount >= MAX_MULTIPLE_GPUS)
		{
			break;
		}
		
		DECLARE_ZERO(DXGI_ADAPTER_DESC3, desc);
		adapter->GetDesc3(&desc);

		// Ignore Microsoft Driver
		if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
		{
			for (uint32_t level = 0; level < sizeof(feature_levels) / sizeof(feature_levels[0]); ++level)
			{
				// Make sure the adapter can support a D3D12 device
				if (SUCCEEDED(D3D12CreateDevice(adapter, feature_levels[level], __uuidof(ID3D12Device), NULL)))
				{
					GpuDesc gpuDescTmp = {};
					GpuDesc* pGpuDesc = gpuDesc ? &gpuDesc[gpuCount] : &gpuDescTmp;
					HRESULT hres = adapter->QueryInterface(IID_ARGS(&pGpuDesc->pGpu));
					if (SUCCEEDED(hres))
					{
						if (gpuDesc)
						{
							Renderer renderer = {};
							D3D12CreateDevice(adapter, feature_levels[level], IID_PPV_ARGS(&renderer.mD3D12.pDxDevice));
							hook_fill_gpu_desc(&renderer, feature_levels[level], pGpuDesc);
							//get preset for current gpu description
							pGpuDesc->mPreset =
								getGPUPresetLevel(pGpuDesc->mVendorId, pGpuDesc->mDeviceId, pGpuDesc->mRevisionId);
							SAFE_RELEASE(renderer.mD3D12.pDxDevice);
						}
						else
						{
							SAFE_RELEASE(pGpuDesc->pGpu);
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

		adapter->Release();
	}

	if (pGpuCount)
		*pGpuCount = gpuCount;

	if (pFoundSoftwareAdapter)
		*pFoundSoftwareAdapter = foundSoftwareAdapter;
}
#endif

GPUSettings util_to_GpuSettings(const GpuDesc* pGpuDesc)
{
	GPUSettings gpuSettings = {};
	gpuSettings.mUniformBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	gpuSettings.mUploadBufferTextureAlignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
	gpuSettings.mUploadBufferTextureRowAlignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
	gpuSettings.mMultiDrawIndirect = true;
	gpuSettings.mMaxVertexInputBindings = 32U;

	//assign device ID
	strncpy(gpuSettings.mGpuVendorPreset.mModelId, pGpuDesc->mDeviceId, MAX_GPU_VENDOR_STRING_LENGTH);
	//assign vendor ID
	strncpy(gpuSettings.mGpuVendorPreset.mVendorId, pGpuDesc->mVendorId, MAX_GPU_VENDOR_STRING_LENGTH);
	//assign Revision ID
	strncpy(gpuSettings.mGpuVendorPreset.mRevisionId, pGpuDesc->mRevisionId, MAX_GPU_VENDOR_STRING_LENGTH);
	//get name from api
	strncpy(gpuSettings.mGpuVendorPreset.mGpuName, pGpuDesc->mName, MAX_GPU_VENDOR_STRING_LENGTH);
	//get preset
	gpuSettings.mGpuVendorPreset.mPresetLevel = pGpuDesc->mPreset;
	//get VRAM
	gpuSettings.mVRAM = pGpuDesc->mDedicatedVideoMemory;
	//get wave lane count
	gpuSettings.mWaveLaneCount = pGpuDesc->mFeatureDataOptions1.WaveLaneCountMin;
	gpuSettings.mROVsSupported = pGpuDesc->mFeatureDataOptions.ROVsSupported ? true : false;
	gpuSettings.mTessellationSupported = gpuSettings.mGeometryShaderSupported = true;
	gpuSettings.mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_ALL;
	gpuSettings.mGpuBreadcrumbs = true;
	gpuSettings.mHDRSupported = true;
	gpuSettings.mIndirectRootConstant = true;
	gpuSettings.mBuiltinDrawID = false;

	// Determine root signature size for this gpu driver
	DXGI_ADAPTER_DESC adapterDesc;
	pGpuDesc->pGpu->GetDesc(&adapterDesc);
	gpuSettings.mMaxRootSignatureDWORDS = gRootSignatureDWORDS[util_to_internal_gpu_vendor(adapterDesc.VendorId)];

	//fill in driver info
	switch ( util_to_internal_gpu_vendor( adapterDesc.VendorId ) )
	{
	case GPU_VENDOR_NVIDIA:
#if defined(NVAPI)
		if ( NvAPI_Status::NVAPI_OK == gNvStatus ) {
			sprintf( gpuSettings.mGpuVendorPreset.mGpuDriverVersion, "%lu.%lu", gNvGpuInfo.driverVersion / 100, gNvGpuInfo.driverVersion % 100);
		}
#endif
		break;
	case GPU_VENDOR_AMD:
#if defined(AMDAGS)
		//populating driver info
		if ( AGSReturnCode::AGS_SUCCESS == gAgsStatus ) {
			sprintf( gpuSettings.mGpuVendorPreset.mGpuDriverVersion, "%s", gAgsGpuInfo.driverVersion );
		}
#endif
		break;
	default:
		break;
	}

	return gpuSettings;
}

void util_query_variable_shading_rate_tier(Renderer* pRenderer, GPUSettings* pGpuSettings)
{
	pGpuSettings->mShadingRates = SHADING_RATE_NOT_SUPPORTED;
	pGpuSettings->mShadingRateCaps = SHADING_RATE_CAPS_NOT_SUPPORTED;
	pGpuSettings->mShadingRateTexelWidth = pGpuSettings->mShadingRateTexelHeight = 0;

#ifdef ENABLE_VRS
	D3D12_FEATURE_DATA_D3D12_OPTIONS6 options;
	HRESULT hres = pRenderer->mD3D12.pDxDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &options, sizeof(options));

	if (SUCCEEDED(hres))
	{
		if (options.VariableShadingRateTier == D3D12_VARIABLE_SHADING_RATE_TIER_1)
		{
			pGpuSettings->mShadingRateCaps |= SHADING_RATE_CAPS_PER_DRAW;
		}

		if (options.VariableShadingRateTier == D3D12_VARIABLE_SHADING_RATE_TIER_2)
		{
			pGpuSettings->mShadingRateCaps |= SHADING_RATE_CAPS_PER_DRAW;
			pGpuSettings->mShadingRateCaps |= SHADING_RATE_CAPS_PER_TILE;
		}

		if (pGpuSettings->mShadingRateCaps)
		{
			pGpuSettings->mShadingRates |= SHADING_RATE_FULL;
			pGpuSettings->mShadingRates |= SHADING_RATE_HALF;
			pGpuSettings->mShadingRates |= SHADING_RATE_1X2;
			pGpuSettings->mShadingRates |= SHADING_RATE_2X1;

			if (options.AdditionalShadingRatesSupported)
			{
				pGpuSettings->mShadingRates |= SHADING_RATE_2X4;
				pGpuSettings->mShadingRates |= SHADING_RATE_4X2;
				pGpuSettings->mShadingRates |= SHADING_RATE_QUARTER;
			}

			pGpuSettings->mShadingRateTexelWidth = pGpuSettings->mShadingRateTexelHeight =
				options.ShadingRateImageTileSize;
		}
	}
#endif
}

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

	return D3D12EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModelsID, NULL, NULL);
}
#endif

#if defined(XBOX)
static UINT HANGBEGINCALLBACK(UINT64 Flags)
{
	LOGF(LogLevel::eINFO, "( %d )", Flags);
	return (UINT)Flags;
}

static void HANGPRINTCALLBACK(const CHAR* strLine)
{
	LOGF(LogLevel::eINFO, "( %s )", strLine);
	return;
}

static void HANGDUMPCALLBACK(const WCHAR* strFileName) { return; }
#endif

static bool SelectBestGpu(Renderer* pRenderer, D3D_FEATURE_LEVEL* pFeatureLevel)
{
	// The D3D debug layer (as well as Microsoft PIX and other graphics debugger
	// tools using an injection library) is not compatible with Nsight Aftermath.
	// If Aftermath detects that any of these tools are present it will fail initialization.
#if defined(ENABLE_GRAPHICS_DEBUG) && !defined(ENABLE_NSIGHT_AFTERMATH)
	//add debug layer if in debug mode
	if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(pRenderer->mD3D12.pDXDebug), (void**)&(pRenderer->mD3D12.pDXDebug))))
	{
		hook_enable_debug_layer(pRenderer);
	}
#endif

#if defined(ENABLE_NSIGHT_AFTERMATH)
	// Enable Nsight Aftermath GPU crash dump creation.
	// This needs to be done before the Vulkan device is created.
	CreateAftermathTracker(pRenderer->pName, &pRenderer->mAftermathTracker);
#endif

#if defined(USE_DRED)
	SUCCEEDED(D3D12GetDebugInterface(__uuidof(pRenderer->pDredSettings), (void**)&(pRenderer->pDredSettings)));

	// Turn on AutoBreadcrumbs and Page Fault reporting
	pRenderer->pDredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	pRenderer->pDredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
#endif

#if defined(XBOX)
	// Create the DX12 API device object.
	CHECK_HRESULT(hook_create_device(NULL, D3D_FEATURE_LEVEL_12_1, &pRenderer->mD3D12.pDxDevice));

#if defined(ENABLE_GRAPHICS_DEBUG)
	//Sets the callback functions to invoke when the GPU hangs
	//pRenderer->mD3D12.pDxDevice->SetHangCallbacksX(HANGBEGINCALLBACK, HANGPRINTCALLBACK, NULL);
#endif

	// First, retrieve the underlying DXGI device from the D3D device.
	IDXGIDevice1* dxgiDevice;
	CHECK_HRESULT(pRenderer->mD3D12.pDxDevice->QueryInterface(IID_ARGS(&dxgiDevice)));

	// Identify the physical adapter (GPU or card) this device is running on.
	IDXGIAdapter* dxgiAdapter;
	CHECK_HRESULT(dxgiDevice->GetAdapter(&dxgiAdapter));

	// And obtain the factory object that created it.
	CHECK_HRESULT(dxgiAdapter->GetParent(IID_ARGS(&pRenderer->mD3D12.pDXGIFactory)));

	uint32_t gpuCount = 1;
	GpuDesc  gpuDesc[1] = {};
	dxgiAdapter->QueryInterface(IID_ARGS(&gpuDesc[0].pGpu));

	dxgiAdapter->Release();
	typedef bool (*DeviceBetterFn)(GpuDesc * gpuDesc, uint32_t testIndex, uint32_t refIndex);
	DeviceBetterFn isDeviceBetter = [](GpuDesc* gpuDesc, uint32_t testIndex, uint32_t refIndex) -> bool { return false; };

	hook_fill_gpu_desc(pRenderer, D3D_FEATURE_LEVEL_12_1, &gpuDesc[0]);

#else
	UINT flags = 0;
#if defined(ENABLE_GRAPHICS_DEBUG)
	flags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	CHECK_HRESULT(CreateDXGIFactory2(flags, IID_ARGS(&pRenderer->mD3D12.pDXGIFactory)));

	uint32_t       gpuCount = 0;
	bool           foundSoftwareAdapter = false;

	// Find number of usable GPUs
	util_enumerate_gpus(pRenderer->mD3D12.pDXGIFactory, &gpuCount, NULL, &foundSoftwareAdapter);

	// If the only adapter we found is a software adapter, log error message for QA
	if (!gpuCount && foundSoftwareAdapter)
	{
		LOGF(eERROR, "The only available GPU has DXGI_ADAPTER_FLAG_SOFTWARE. Early exiting");
		ASSERT(false);
		return false;
	}

	ASSERT(gpuCount);
	GpuDesc gpuDesc[MAX_MULTIPLE_GPUS] = {};
	gpuCount = 0;

	util_enumerate_gpus(pRenderer->mD3D12.pDXGIFactory, &gpuCount, gpuDesc, NULL);
	ASSERT(gpuCount > 0);

	typedef bool (*DeviceBetterFn)(GpuDesc * gpuDesc, uint32_t testIndex, uint32_t refIndex);
	DeviceBetterFn isDeviceBetter = [](GpuDesc* gpuDesc, uint32_t testIndex, uint32_t refIndex) -> bool {
		const GpuDesc& gpu1 = gpuDesc[testIndex];
		const GpuDesc& gpu2 = gpuDesc[refIndex];

		// force to an Intel, useful sometimes for debugging
		//		if(stricmp(gpu1.mVendorId, "0x8086") == 0 )
		//			return true;

		// If shader model 6.0 or higher is requested, prefer the GPU which supports it
		if (gpu1.pRenderer->mShaderTarget >= shader_target_6_0)
		{
			if (gpu1.mFeatureDataOptions1.WaveOps != gpu2.mFeatureDataOptions1.WaveOps)
				return gpu1.mFeatureDataOptions1.WaveOps;
		}

		// Next check for higher preset
		if ((int)gpu1.mPreset != (int)gpu2.mPreset)
		{
			return gpu1.mPreset > gpu2.mPreset;
		}

		// Check feature level first, sort the greatest feature level gpu to the front
		if ((int)gpu1.mMaxSupportedFeatureLevel != (int)gpu2.mMaxSupportedFeatureLevel)
		{
			return gpu1.mMaxSupportedFeatureLevel > gpu2.mMaxSupportedFeatureLevel;
		}

		return gpu1.mDedicatedVideoMemory > gpu2.mDedicatedVideoMemory;
	};

#endif

	uint32_t     gpuIndex = UINT32_MAX;
	GPUSettings* gpuSettings = (GPUSettings*)alloca(gpuCount * sizeof(GPUSettings));

	for (uint32_t i = 0; i < gpuCount; ++i)
	{
		gpuSettings[i] = util_to_GpuSettings(&gpuDesc[i]);
		LOGF(
			LogLevel::eINFO, "GPU[%i] detected. Vendor ID: %s, Model ID: %s, Revision ID: %s, Preset: %s, GPU Name: %S", i,
			gpuSettings[i].mGpuVendorPreset.mVendorId, gpuSettings[i].mGpuVendorPreset.mModelId, gpuSettings[i].mGpuVendorPreset.mRevisionId,
			presetLevelToString(gpuSettings[i].mGpuVendorPreset.mPresetLevel), gpuSettings[i].mGpuVendorPreset.mGpuName);

		// Check that gpu supports at least graphics
		if (gpuIndex == UINT32_MAX || isDeviceBetter(gpuDesc, i, gpuIndex))
		{
			gpuIndex = i;
		}
	}
	// Get the latest and greatest feature level gpu
	CHECK_HRESULT(gpuDesc[gpuIndex].pGpu->QueryInterface(IID_ARGS(&pRenderer->mD3D12.pDxActiveGPU)));
	ASSERT(pRenderer->mD3D12.pDxActiveGPU != NULL);
	pRenderer->pActiveGpuSettings = (GPUSettings*)tf_malloc(sizeof(GPUSettings));
	*pRenderer->pActiveGpuSettings = gpuSettings[gpuIndex];

	for (uint32_t i = 0; i < gpuCount; ++i)
	{
		SAFE_RELEASE(gpuDesc[i].pGpu);
	}

	// Print selected GPU information
	LOGF(LogLevel::eINFO, "GPU[%d] is selected as default GPU", gpuIndex);
	LOGF(LogLevel::eINFO, "Name of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mGpuName);
	LOGF(LogLevel::eINFO, "Vendor id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mVendorId);
	LOGF(LogLevel::eINFO, "Model id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mModelId);
	LOGF(LogLevel::eINFO, "Revision id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mRevisionId);
	LOGF(LogLevel::eINFO, "Preset of selected gpu: %s", presetLevelToString(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel));

	if (pFeatureLevel)
		*pFeatureLevel = gpuDesc[gpuIndex].mMaxSupportedFeatureLevel;

	return true;
}

static bool AddDevice(const RendererDesc* pDesc, Renderer* pRenderer)
{
#if !defined(XBOX)
	if (pDesc->pContext && pDesc->pContext->mD3D12.pDXDebug)
	{
		pDesc->pContext->mD3D12.pDXDebug->QueryInterface(IID_ARGS(&pRenderer->mD3D12.pDXDebug));
	}
#endif

#if defined(USE_NSIGHT_AFTERMATH)
	// Enable Nsight Aftermath GPU crash dump creation.
	// This needs to be done before the Vulkan device is created.
	CreateAftermathTracker(pRenderer->pName, &pRenderer->mAftermathTracker);
#endif

#if defined(USE_DRED)
	SUCCEEDED(D3D12GetDebugInterface(__uuidof(pRenderer->pDredSettings), (void**)&(pRenderer->pDredSettings)));

	// Turn on AutoBreadcrumbs and Page Fault reporting
	pRenderer->pDredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	pRenderer->pDredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
#endif

	D3D_FEATURE_LEVEL supportedFeatureLevel = (D3D_FEATURE_LEVEL)0;
#if !defined(XBOX)
	if (pDesc->pContext)
	{
		ASSERT(pDesc->mGpuIndex < pDesc->pContext->mGpuCount);

		CHECK_HRESULT(pDesc->pContext->mD3D12.pDXGIFactory->QueryInterface(IID_ARGS(&pRenderer->mD3D12.pDXGIFactory)));
		CHECK_HRESULT(pDesc->pContext->mGpus[pDesc->mGpuIndex].mD3D12.pDxGPU->QueryInterface(IID_ARGS(&pRenderer->mD3D12.pDxActiveGPU)));
		pRenderer->pActiveGpuSettings = (GPUSettings*)tf_malloc(sizeof(GPUSettings));
		*pRenderer->pActiveGpuSettings = pDesc->pContext->mGpus[pDesc->mGpuIndex].mSettings;
		supportedFeatureLevel = pDesc->pContext->mGpus[pDesc->mGpuIndex].mD3D12.mMaxSupportedFeatureLevel;
	}
	else
#endif
	{
		if (!SelectBestGpu(pRenderer, &supportedFeatureLevel))
			return false;
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

#if !defined(XBOX)
	CHECK_HRESULT(D3D12CreateDevice(
		pRenderer->mD3D12.pDxActiveGPU, supportedFeatureLevel, IID_ARGS(&pRenderer->mD3D12.pDxDevice)));
#endif

#if defined(ENABLE_NSIGHT_AFTERMATH)
	SetAftermathDevice(pRenderer->mD3D12.pDxDevice);
#endif

#if defined(_WINDOWS) && defined(FORGE_DEBUG)
	HRESULT hr = pRenderer->mD3D12.pDxDevice->QueryInterface(IID_ARGS(&pRenderer->mD3D12.pDxDebugValidation));
	if (SUCCEEDED(hr))
	{
		pRenderer->mD3D12.pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		pRenderer->mD3D12.pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		// D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND breaks even when it is disabled
		pRenderer->mD3D12.pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
		pRenderer->mD3D12.pDxDebugValidation->SetBreakOnID(D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND, false);

		// On Windows 11 there's a bug in the DXGI debug layer that triggers a false-positive on hybrid GPU
		// laptops during Present. The problem appears to be a race condition, so it may or may not happen.
		// Suppressing D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE avoids this problem.
		uint32_t gpuCount = 0;
		util_enumerate_gpus(pRenderer->mD3D12.pDXGIFactory, &gpuCount, NULL, NULL);
		// If we have >2 GPU's (eg. Laptop with integrated and dedicated GPU).
		if (gpuCount >= 2)
		{
			D3D12_MESSAGE_ID hide[] = { D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE };
			D3D12_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.NumIDs = _countof(hide);
			filter.DenyList.pIDList = hide;
			pRenderer->mD3D12.pDxDebugValidation->AddStorageFilterEntries(&filter);
		}
	}
#endif

	// Query variable rate shading tier (for unlinked mode, this was already done during enumeration).
	if (!pDesc->pContext)
	{
		util_query_variable_shading_rate_tier(pRenderer, pRenderer->pActiveGpuSettings);
	}

	return true;
}

static void RemoveDevice(Renderer* pRenderer)
{
	SAFE_RELEASE(pRenderer->mD3D12.pDXGIFactory);
	SAFE_RELEASE(pRenderer->mD3D12.pDxActiveGPU);
#if defined(_WINDOWS)
	if (pRenderer->mD3D12.pDxDebugValidation)
	{
		pRenderer->mD3D12.pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, false);
		pRenderer->mD3D12.pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, false);
		pRenderer->mD3D12.pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
		SAFE_RELEASE(pRenderer->mD3D12.pDxDebugValidation);
	}
#endif

#if defined(XBOX)
	SAFE_RELEASE(pRenderer->mD3D12.pDxDevice);
#elif defined(ENABLE_GRAPHICS_DEBUG)
	ID3D12DebugDevice* pDebugDevice = NULL;
	pRenderer->mD3D12.pDxDevice->QueryInterface(&pDebugDevice);

	SAFE_RELEASE(pRenderer->mD3D12.pDXDebug);
	SAFE_RELEASE(pRenderer->mD3D12.pDxDevice);

	if (pDebugDevice)
	{
		// Debug device is released first so report live objects don't show its ref as a warning.
		pDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
		pDebugDevice->Release();
	}
#else
	SAFE_RELEASE(pRenderer->mD3D12.pDxDevice);
#endif

#if defined(ENABLE_NSIGHT_AFTERMATH)
	DestroyAftermathTracker(&pRenderer->mAftermathTracker);
#endif

#if defined(USE_DRED)
	SAFE_RELEASE(pRenderer->pDredSettings);
#endif
}

void InitCommon(const RendererDesc* pDesc, Renderer* pRenderer)
{
	AGSReturnCode agsRet = agsInit();
	if (AGSReturnCode::AGS_SUCCESS == agsRet)
	{
		agsPrintDriverInfo();
	}

	NvAPI_Status nvStatus = nvapiInit();
	if (NvAPI_Status::NVAPI_OK == nvStatus)
	{
		nvapiPrintDriverInfo();
	}

	// The D3D debug layer (as well as Microsoft PIX and other graphics debugger
	// tools using an injection library) is not compatible with Nsight Aftermath.
	// If Aftermath detects that any of these tools are present it will fail initialization.
#if defined(ENABLE_GRAPHICS_DEBUG) && !defined(USE_NSIGHT_AFTERMATH)
	//add debug layer if in debug mode
	if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(pRenderer->mD3D12.pDXDebug), (void**)&(pRenderer->mD3D12.pDXDebug))))
	{
		hook_enable_debug_layer(pRenderer);
	}
#endif

#if !defined(XBOX)
	UINT flags = 0;
#if defined(ENABLE_GRAPHICS_DEBUG)
	flags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	CHECK_HRESULT(CreateDXGIFactory2(flags, IID_ARGS(&pRenderer->mD3D12.pDXGIFactory)));
#endif

	pRenderer->mUnlinkedRendererIndex = 0;
}

void ExitCommon(Renderer* pRenderer)
{
	SAFE_RELEASE(pRenderer->mD3D12.pDXGIFactory);

	nvapiExit();
	agsExit();
}

/************************************************************************/
// Renderer Context Init Exit (multi GPU)
/************************************************************************/
static uint32_t gRendererCount = 0;

void d3d12_initRendererContext(const char* appName, const RendererContextDesc* pDesc, RendererContext** ppContext)
{
#if !defined(XBOX)
	ASSERT(appName);
	ASSERT(pDesc);
	ASSERT(ppContext);
	ASSERT(gRendererCount == 0);

	RendererDesc fakeDesc = {};
	fakeDesc.mDxFeatureLevel = pDesc->mDxFeatureLevel;
	fakeDesc.mEnableGPUBasedValidation = pDesc->mEnableGPUBasedValidation;

	Renderer fakeRenderer = {};

	InitCommon(&fakeDesc, &fakeRenderer);

	RendererContext* pContext = (RendererContext*)tf_calloc_memalign(1, alignof(RendererContext), sizeof(RendererContext));
	pContext->mD3D12.pDXGIFactory = fakeRenderer.mD3D12.pDXGIFactory;
	pContext->mD3D12.pDXDebug = fakeRenderer.mD3D12.pDXDebug;

	bool           foundSoftwareAdapter = false;

	// Find number of usable GPUs
	util_enumerate_gpus(pContext->mD3D12.pDXGIFactory, &pContext->mGpuCount, NULL, &foundSoftwareAdapter);

	// If the only adapter we found is a software adapter, log error message for QA
	if (!pContext->mGpuCount && foundSoftwareAdapter)
	{
		LOGF(eERROR, "The only available GPU has DXGI_ADAPTER_FLAG_SOFTWARE. Early exiting");
		ASSERT(false);
		return;
	}

	ASSERT(pContext->mGpuCount);
	GpuDesc gpuDesc[MAX_MULTIPLE_GPUS] = {};

	util_enumerate_gpus(pContext->mD3D12.pDXGIFactory, &pContext->mGpuCount, gpuDesc, NULL);
	ASSERT(pContext->mGpuCount > 0);
	for (uint32_t i = 0; i < pContext->mGpuCount; ++i)
	{
		pContext->mGpus[i].mSettings = util_to_GpuSettings(&gpuDesc[i]);
		pContext->mGpus[i].mD3D12.pDxGPU = gpuDesc[i].pGpu;
		pContext->mGpus[i].mD3D12.mMaxSupportedFeatureLevel = gpuDesc[i].mMaxSupportedFeatureLevel;

		// Create device to query additional properties.
		D3D12CreateDevice(gpuDesc[i].pGpu, gpuDesc[i].mMaxSupportedFeatureLevel, IID_PPV_ARGS(&fakeRenderer.mD3D12.pDxDevice));

		util_query_variable_shading_rate_tier(&fakeRenderer, &pContext->mGpus[i].mSettings);

		SAFE_RELEASE(fakeRenderer.mD3D12.pDxDevice);
	}

	*ppContext = pContext;
#endif
}

void d3d12_exitRendererContext(RendererContext* pContext)
{
#if !defined(XBOX)
	ASSERT(pContext);

	SAFE_RELEASE(pContext->mD3D12.pDXDebug);

	Renderer fakeRenderer = {};

	fakeRenderer.mD3D12.pDXGIFactory = pContext->mD3D12.pDXGIFactory;
	ExitCommon(&fakeRenderer);

	for (uint32_t i = 0; i < pContext->mGpuCount; ++i)
	{
		SAFE_RELEASE(pContext->mGpus[i].mD3D12.pDxGPU);
	}
	SAFE_FREE(pContext);
#endif
}
/************************************************************************/
// Renderer Init Remove
/************************************************************************/
void d3d12_initRenderer(const char* appName, const RendererDesc* pDesc, Renderer** ppRenderer)
{
	ASSERT(appName);
	ASSERT(pDesc);
	ASSERT(ppRenderer);

	Renderer* pRenderer = (Renderer*)tf_calloc_memalign(1, alignof(Renderer), sizeof(Renderer));
	ASSERT(pRenderer);

	pRenderer->mGpuMode = pDesc->mGpuMode;
	pRenderer->mShaderTarget = pDesc->mShaderTarget;
	pRenderer->mEnableGpuBasedValidation = pDesc->mEnableGPUBasedValidation;

	pRenderer->pName = (char*)tf_calloc(strlen(appName) + 1, sizeof(char));
	strcpy(pRenderer->pName, appName);

	// Initialize the D3D12 bits
	{
		if (!pDesc->pContext)
		{
			InitCommon(pDesc, pRenderer);
		}
		else
		{
			pRenderer->mUnlinkedRendererIndex = gRendererCount;
		}

		if (!AddDevice(pDesc, pRenderer))
		{
			*ppRenderer = NULL;
			return;
		}

#if !defined(XBOX)
		//anything below LOW preset is not supported and we will exit
		if (pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel < GPU_PRESET_LOW)
		{
			//have the condition in the assert as well so its cleared when the assert message box appears

			ASSERT(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel >= GPU_PRESET_LOW);    //-V547

			SAFE_FREE(pRenderer->pName);

			//remove device and any memory we allocated in just above as this is the first function called
			//when initializing the forge
			RemoveDevice(pRenderer);
			SAFE_FREE(pRenderer);
			LOGF(LogLevel::eERROR, "Selected GPU has an Office Preset in gpu.cfg.");
			LOGF(LogLevel::eERROR, "Office preset is not supported by The Forge.");

			//return NULL pRenderer so that client can gracefully handle exit
			//This is better than exiting from here in case client has allocated memory or has fallbacks
			*ppRenderer = NULL;
			return;
		}

		d3d12_utils_caps_builder(pRenderer);

		if (pRenderer->mShaderTarget >= shader_target_6_0)
		{
			// Query the level of support of Shader Model.
			D3D12_FEATURE_DATA_SHADER_MODEL   shaderModelSupport = { D3D_SHADER_MODEL_6_0 };
			D3D12_FEATURE_DATA_D3D12_OPTIONS1 waveIntrinsicsSupport = {};
			if (!SUCCEEDED(pRenderer->mD3D12.pDxDevice->CheckFeatureSupport(
					(D3D12_FEATURE)D3D12_FEATURE_SHADER_MODEL, &shaderModelSupport, sizeof(shaderModelSupport))))
			{
				return;
			}
			// Query the level of support of Wave Intrinsics.
			if (!SUCCEEDED(pRenderer->mD3D12.pDxDevice->CheckFeatureSupport(
					(D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &waveIntrinsicsSupport, sizeof(waveIntrinsicsSupport))))
			{
				return;
			}

			// If the device doesn't support SM6 or Wave Intrinsics, try enabling the experimental feature for Shader Model 6 and creating the device again.
			if (shaderModelSupport.HighestShaderModel != D3D_SHADER_MODEL_6_0 || waveIntrinsicsSupport.WaveOps == FALSE)
			{
				RENDERDOC_API_1_1_2* rdoc_api = NULL;
				// At init, on windows
				if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
				{
					pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
					RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);
				}

				// If RenderDoc is connected shader model 6 is not detected but it still works
				if (!rdoc_api || !rdoc_api->IsTargetControlConnected())
				{
					// If the device still doesn't support SM6 or Wave Intrinsics after enabling the experimental feature, you could set up your application to use the highest supported shader model.
					// For simplicity we just exit the application here.
					if (shaderModelSupport.HighestShaderModel < D3D_SHADER_MODEL_6_0 ||
						(waveIntrinsicsSupport.WaveOps == FALSE && !SUCCEEDED(EnableExperimentalShaderModels())))
					{
						RemoveDevice(pRenderer);
						LOGF(LogLevel::eERROR, "Hardware does not support Shader Model 6.0");
						return;
					}
				}
				else
				{
					LOGF(
						LogLevel::eWARNING,
						"\nRenderDoc does not support SM 6.0 or higher. Application might work but you won't be able to debug the SM 6.0+ "
						"shaders or view their bytecode.");
				}
			}
		}
#endif

		/************************************************************************/
		// Multi GPU - SLI Node Count
		/************************************************************************/
		uint32_t gpuCount = pRenderer->mD3D12.pDxDevice->GetNodeCount();
		pRenderer->mLinkedNodeCount = (pRenderer->mGpuMode == GPU_MODE_LINKED) ? gpuCount : 1;
		if (pRenderer->mGpuMode == GPU_MODE_LINKED && pRenderer->mLinkedNodeCount < 2)
			pRenderer->mGpuMode = GPU_MODE_SINGLE;
		/************************************************************************/
		// Descriptor heaps
		/************************************************************************/
		pRenderer->mD3D12.pCPUDescriptorHeaps = (DescriptorHeap**)tf_malloc(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES * sizeof(DescriptorHeap*));
		pRenderer->mD3D12.pCbvSrvUavHeaps = (DescriptorHeap**)tf_malloc(pRenderer->mLinkedNodeCount * sizeof(DescriptorHeap*));
		pRenderer->mD3D12.pSamplerHeaps = (DescriptorHeap**)tf_malloc(pRenderer->mLinkedNodeCount * sizeof(DescriptorHeap*));

		for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Flags = gCpuDescriptorHeapProperties[i].mFlags;
			desc.NodeMask = 0;    // CPU Descriptor Heap - Node mask is irrelevant
			desc.NumDescriptors = gCpuDescriptorHeapProperties[i].mMaxDescriptors;
			desc.Type = (D3D12_DESCRIPTOR_HEAP_TYPE)i;
			add_descriptor_heap(pRenderer->mD3D12.pDxDevice, &desc, &pRenderer->mD3D12.pCPUDescriptorHeaps[i]);
		}

		// One shader visible heap for each linked node
		for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			desc.NodeMask = util_calculate_node_mask(pRenderer, i);

			desc.NumDescriptors = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			add_descriptor_heap(pRenderer->mD3D12.pDxDevice, &desc, &pRenderer->mD3D12.pCbvSrvUavHeaps[i]);

			// Max sampler descriptor count
			desc.NumDescriptors = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			add_descriptor_heap(pRenderer->mD3D12.pDxDevice, &desc, &pRenderer->mD3D12.pSamplerHeaps[i]);
		}
		/************************************************************************/
		// Memory allocator
		/************************************************************************/
		D3D12MA::ALLOCATOR_DESC desc = {};
		desc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
		desc.pDevice = pRenderer->mD3D12.pDxDevice;
		desc.pAdapter = pRenderer->mD3D12.pDxActiveGPU;

		D3D12MA::ALLOCATION_CALLBACKS allocationCallbacks = {};
		allocationCallbacks.pAllocate = [](size_t size, size_t alignment, void*) { return tf_memalign(alignment, size); };
		allocationCallbacks.pFree = [](void* ptr, void*) { tf_free(ptr); };
		desc.pAllocationCallbacks = &allocationCallbacks;
		CHECK_HRESULT(D3D12MA::CreateAllocator(&desc, &pRenderer->mD3D12.pResourceAllocator));
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

void d3d12_exitRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);
	--gRendererCount;

	remove_default_resources(pRenderer);

	// Destroy the Direct3D12 bits
	for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		remove_descriptor_heap(pRenderer->mD3D12.pCPUDescriptorHeaps[i]);
	}

	for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
	{
		remove_descriptor_heap(pRenderer->mD3D12.pCbvSrvUavHeaps[i]);
		remove_descriptor_heap(pRenderer->mD3D12.pSamplerHeaps[i]);
	}

	SAFE_RELEASE(pRenderer->mD3D12.pResourceAllocator);

	RemoveDevice(pRenderer);

	hook_post_remove_renderer(pRenderer);

	if (pRenderer->mGpuMode != GPU_MODE_UNLINKED)
		ExitCommon(pRenderer);

	// Free all the renderer components
	SAFE_FREE(pRenderer->mD3D12.pCPUDescriptorHeaps);
	SAFE_FREE(pRenderer->mD3D12.pCbvSrvUavHeaps);
	SAFE_FREE(pRenderer->mD3D12.pSamplerHeaps);
	SAFE_FREE(pRenderer->pCapBits);
	SAFE_FREE(pRenderer->pActiveGpuSettings);
	SAFE_FREE(pRenderer->pName);
	SAFE_FREE(pRenderer);
}
/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void d3d12_addFence(Renderer* pRenderer, Fence** ppFence)
{
	//ASSERT that renderer is valid
	ASSERT(pRenderer);
	ASSERT(ppFence);

	//create a Fence and ASSERT that it is valid
	Fence* pFence = (Fence*)tf_calloc(1, sizeof(Fence));
	ASSERT(pFence);

	CHECK_HRESULT(pRenderer->mD3D12.pDxDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_ARGS(&pFence->mD3D12.pDxFence)));
	pFence->mD3D12.mFenceValue = 1;

	pFence->mD3D12.pDxWaitIdleFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	*ppFence = pFence;
}

void d3d12_removeFence(Renderer* pRenderer, Fence* pFence)
{
	//ASSERT that renderer is valid
	ASSERT(pRenderer);
	//ASSERT that given fence to remove is valid
	ASSERT(pFence);

	SAFE_RELEASE(pFence->mD3D12.pDxFence);
	CloseHandle(pFence->mD3D12.pDxWaitIdleFenceEvent);

	SAFE_FREE(pFence);
}

void d3d12_addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
	addFence(pRenderer, (Fence**)ppSemaphore);    //-V1027
}

void d3d12_removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
	//ASSERT that renderer and given semaphore are valid
	ASSERT(pRenderer);
	ASSERT(pSemaphore);

	removeFence(pRenderer, (Fence*)pSemaphore);    //-V1027
}

void d3d12_addQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
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

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	if (pDesc->mFlag & QUEUE_FLAG_DISABLE_GPU_TIMEOUT)
		queueDesc.Flags |= D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
	queueDesc.Type = gDx12CmdTypeTranslator[pDesc->mType];
	queueDesc.Priority = gDx12QueuePriorityTranslator[pDesc->mPriority];
	queueDesc.NodeMask = util_calculate_node_mask(pRenderer, nodeIndex);

	CHECK_HRESULT(hook_create_command_queue(pRenderer->mD3D12.pDxDevice, &queueDesc, &pQueue->mD3D12.pDxQueue));

	wchar_t  queueTypeBuffer[32] = {};
	const wchar_t* queueType = NULL;
	switch (queueDesc.Type)
	{
		case D3D12_COMMAND_LIST_TYPE_DIRECT: queueType = L"GRAPHICS QUEUE"; break;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE: queueType = L"COMPUTE QUEUE"; break;
		case D3D12_COMMAND_LIST_TYPE_COPY: queueType = L"COPY QUEUE"; break;
		default: queueType = L"UNKNOWN QUEUE"; break;
	}

	swprintf(queueTypeBuffer, L"%ls %u", queueType, pDesc->mNodeIndex);
	pQueue->mD3D12.pDxQueue->SetName(queueTypeBuffer);

	pQueue->mType = pDesc->mType;
	pQueue->mNodeIndex = pDesc->mNodeIndex;

	// override node index
	if (pRenderer->mGpuMode == GPU_MODE_UNLINKED)
		pQueue->mNodeIndex = pRenderer->mUnlinkedRendererIndex;

	// Add queue fence. This fence will make sure we finish all GPU works before releasing the queue
	addFence(pRenderer, &pQueue->mD3D12.pFence);

	*ppQueue = pQueue;
}

void d3d12_removeQueue(Renderer* pRenderer, Queue* pQueue)
{
	ASSERT(pQueue);

	// Make sure we finished all GPU works before we remove the queue
	waitQueueIdle(pQueue);

	removeFence(pRenderer, pQueue->mD3D12.pFence);

	SAFE_RELEASE(pQueue->mD3D12.pDxQueue);

	SAFE_FREE(pQueue);
}

void d3d12_addCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
{
	//ASSERT that renderer is valid
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppCmdPool);

	//create one new CmdPool and add to renderer
	CmdPool* pCmdPool = (CmdPool*)tf_calloc(1, sizeof(CmdPool));
	ASSERT(pCmdPool);

	CHECK_HRESULT(pRenderer->mD3D12.pDxDevice->CreateCommandAllocator(
		gDx12CmdTypeTranslator[pDesc->pQueue->mType], IID_ARGS(&pCmdPool->pDxCmdAlloc)));

	pCmdPool->pQueue = pDesc->pQueue;

	*ppCmdPool = pCmdPool;
}

void d3d12_removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	//check validity of given renderer and command pool
	ASSERT(pRenderer);
	ASSERT(pCmdPool);

	SAFE_RELEASE(pCmdPool->pDxCmdAlloc);
	SAFE_FREE(pCmdPool);
}

void d3d12_addCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
{
	//verify that given pool is valid
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppCmd);

	// initialize to zero
	Cmd* pCmd = (Cmd*)tf_calloc_memalign(1, alignof(Cmd), sizeof(Cmd));
	ASSERT(pCmd);

	//set command pool of new command
	pCmd->mD3D12.mNodeIndex = pRenderer->mGpuMode == GPU_MODE_LINKED ? pDesc->pPool->pQueue->mNodeIndex : 0;
	pCmd->mD3D12.mType = pDesc->pPool->pQueue->mType;
	pCmd->pQueue = pDesc->pPool->pQueue;
	pCmd->pRenderer = pRenderer;

	pCmd->mD3D12.pBoundHeaps[0] = pRenderer->mD3D12.pCbvSrvUavHeaps[pCmd->mD3D12.mNodeIndex];
	pCmd->mD3D12.pBoundHeaps[1] = pRenderer->mD3D12.pSamplerHeaps[pCmd->mD3D12.mNodeIndex];

	pCmd->mD3D12.pCmdPool = pDesc->pPool;

	uint32_t nodeMask = util_calculate_node_mask(pRenderer, pCmd->mD3D12.mNodeIndex);

	if (QUEUE_TYPE_TRANSFER == pDesc->pPool->pQueue->mType)
	{
		CHECK_HRESULT(hook_create_copy_cmd(pRenderer->mD3D12.pDxDevice, nodeMask, pDesc->pPool->pDxCmdAlloc, pCmd));
	}
	else
	{
		ID3D12PipelineState* initialState = NULL;
		CHECK_HRESULT(pRenderer->mD3D12.pDxDevice->CreateCommandList(
			nodeMask, gDx12CmdTypeTranslator[pCmd->mD3D12.mType], pDesc->pPool->pDxCmdAlloc, initialState,
			__uuidof(pCmd->mD3D12.pDxCmdList), (void**)&(pCmd->mD3D12.pDxCmdList)));
	}

	// Command lists are addd in the recording state, but there is nothing
	// to record yet. The main loop expects it to be closed, so close it now.
	CHECK_HRESULT(pCmd->mD3D12.pDxCmdList->Close());

	*ppCmd = pCmd;
}

void d3d12_removeCmd(Renderer* pRenderer, Cmd* pCmd)
{
	//verify that given command and pool are valid
	ASSERT(pRenderer);
	ASSERT(pCmd);

	if (QUEUE_TYPE_TRANSFER == pCmd->mD3D12.mType)
	{
		hook_remove_copy_cmd(pCmd);
	}
	else
	{
		SAFE_RELEASE(pCmd->mD3D12.pDxCmdList);
	}

	SAFE_FREE(pCmd);
}

void d3d12_addCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmd)
{
	//verify that ***cmd is valid
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(cmdCount);
	ASSERT(pppCmd);

	Cmd** ppCmds = (Cmd**)tf_calloc(cmdCount, sizeof(Cmd*));
	ASSERT(ppCmds);

	//add n new cmds to given pool
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		::addCmd(pRenderer, pDesc, &ppCmds[i]);
	}

	*pppCmd = ppCmds;
}

void d3d12_removeCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
{
	//verify that given command list is valid
	ASSERT(ppCmds);

	//remove every given cmd in array
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		removeCmd(pRenderer, ppCmds[i]);
	}

	SAFE_FREE(ppCmds);
}

void d3d12_toggleVSync(Renderer* pRenderer, SwapChain** ppSwapChain)
{
	UNREF_PARAM(pRenderer);
	ASSERT(ppSwapChain);

	SwapChain* pSwapChain = *ppSwapChain;
	//set descriptor vsync boolean
	pSwapChain->mEnableVsync = !pSwapChain->mEnableVsync;
#if !defined(XBOX)
	if (!pSwapChain->mEnableVsync)
	{
		pSwapChain->mD3D12.mFlags |= DXGI_PRESENT_ALLOW_TEARING;
	}
	else
	{
		pSwapChain->mD3D12.mFlags &= ~DXGI_PRESENT_ALLOW_TEARING;
	}
#endif

	//toggle vsync present flag (this can go up to 4 but we don't need to refresh on nth vertical sync)
	pSwapChain->mD3D12.mDxSyncInterval = (pSwapChain->mD3D12.mDxSyncInterval + 1) % 2;
}

void d3d12_addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppSwapChain);
	ASSERT(pDesc->mImageCount <= MAX_SWAPCHAIN_IMAGES);

	SwapChain* pSwapChain = (SwapChain*)tf_calloc(1, sizeof(SwapChain) + pDesc->mImageCount * sizeof(RenderTarget*));
	ASSERT(pSwapChain);
	pSwapChain->ppRenderTargets = (RenderTarget**)(pSwapChain + 1);
	ASSERT(pSwapChain->ppRenderTargets);

#if !defined(XBOX)
	pSwapChain->mD3D12.mDxSyncInterval = pDesc->mEnableVsync ? 1 : 0;

	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width = pDesc->mWidth;
	desc.Height = pDesc->mHeight;
	desc.Format = util_to_dx12_swapchain_format(pDesc->mColorFormat);
	desc.Stereo = false;
	desc.SampleDesc.Count = 1;    // If multisampling is needed, we'll resolve it later
	desc.SampleDesc.Quality = 0;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = pDesc->mImageCount;
	desc.Scaling = DXGI_SCALING_STRETCH;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	desc.Flags = 0;

	BOOL allowTearing = FALSE;
	pRenderer->mD3D12.pDXGIFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
	desc.Flags |= allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	pSwapChain->mD3D12.mFlags |= (!pDesc->mEnableVsync && allowTearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;

	IDXGISwapChain1* swapchain;

	HWND hwnd = (HWND)pDesc->mWindowHandle.window;

	CHECK_HRESULT(pRenderer->mD3D12.pDXGIFactory->CreateSwapChainForHwnd(
		pDesc->ppPresentQueues[0]->mD3D12.pDxQueue, hwnd, &desc, NULL, NULL, &swapchain));

	CHECK_HRESULT(pRenderer->mD3D12.pDXGIFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

	CHECK_HRESULT(swapchain->QueryInterface(IID_ARGS(&pSwapChain->mD3D12.pDxSwapChain)));
	swapchain->Release();

	// Allowing multiple command queues to present for applications like Alternate Frame Rendering
	if (pRenderer->mGpuMode == GPU_MODE_LINKED && pDesc->mPresentQueueCount > 1)
	{
		IUnknown** ppQueues = (IUnknown**)alloca(pDesc->mPresentQueueCount * sizeof(IUnknown*));
		UINT*      pCreationMasks = (UINT*)alloca(pDesc->mPresentQueueCount * sizeof(UINT));
		for (uint32_t i = 0; i < pDesc->mPresentQueueCount; ++i)
		{
			ppQueues[i] = pDesc->ppPresentQueues[i]->mD3D12.pDxQueue;
			pCreationMasks[i] = (1 << pDesc->ppPresentQueues[i]->mNodeIndex);
		}

		pSwapChain->mD3D12.pDxSwapChain->ResizeBuffers1(
			desc.BufferCount, desc.Width, desc.Height, desc.Format, desc.Flags, pCreationMasks, ppQueues);
	}

	ID3D12Resource** buffers = (ID3D12Resource**)alloca(pDesc->mImageCount * sizeof(ID3D12Resource*));

	// Create rendertargets from swapchain
	for (uint32_t i = 0; i < pDesc->mImageCount; ++i)
	{
		CHECK_HRESULT(pSwapChain->mD3D12.pDxSwapChain->GetBuffer(i, IID_ARGS(&buffers[i])));
	}

#endif

	RenderTargetDesc descColor = {};
	descColor.mWidth = pDesc->mWidth;
	descColor.mHeight = pDesc->mHeight;
	descColor.mDepth = 1;
	descColor.mArraySize = 1;
	descColor.mFormat = pDesc->mColorFormat;
	descColor.mClearValue = pDesc->mColorClearValue;
	descColor.mSampleCount = SAMPLE_COUNT_1;
	descColor.mSampleQuality = 0;
	descColor.pNativeHandle = NULL;
	descColor.mFlags = TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET;
	descColor.mStartState = RESOURCE_STATE_PRESENT;
#if defined(XBOX)
	descColor.mFlags |= TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
	pSwapChain->mD3D12.pPresentQueue = pDesc->mPresentQueueCount ? pDesc->ppPresentQueues[0] : NULL;
#endif

	for (uint32_t i = 0; i < pDesc->mImageCount; ++i)
	{
#if !defined(XBOX)
		descColor.pNativeHandle = (void*)buffers[i];
#endif
		::addRenderTarget(pRenderer, &descColor, &pSwapChain->ppRenderTargets[i]);
	}

	pSwapChain->mImageCount = pDesc->mImageCount;
	pSwapChain->mEnableVsync = pDesc->mEnableVsync;

	*ppSwapChain = pSwapChain;
}

void d3d12_removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
#if defined(XBOX)
	hook_queue_present(pSwapChain->mD3D12.pPresentQueue, NULL, 0);
#endif

	for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
	{
		ID3D12Resource* resource = pSwapChain->ppRenderTargets[i]->pTexture->mD3D12.pDxResource;
		removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[i]);
#if !defined(XBOX)
		SAFE_RELEASE(resource);
#endif
	}

#if !defined(XBOX)
	SAFE_RELEASE(pSwapChain->mD3D12.pDxSwapChain);
#endif
	SAFE_FREE(pSwapChain);
}

void d3d12_addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
	//verify renderer validity
	ASSERT(pRenderer);
	//verify adding at least 1 buffer
	ASSERT(pDesc);
	ASSERT(ppBuffer);
	ASSERT(pDesc->mSize > 0);
	ASSERT(pRenderer->mGpuMode != GPU_MODE_UNLINKED || pDesc->mNodeIndex == pRenderer->mUnlinkedRendererIndex);

	// initialize to zero
	Buffer* pBuffer = (Buffer*)tf_calloc_memalign(1, alignof(Buffer), sizeof(Buffer));
	pBuffer->mD3D12.mDescriptors = D3D12_DESCRIPTOR_ID_NONE;
	ASSERT(ppBuffer);

	//add to renderer

	uint64_t allocationSize = pDesc->mSize;
	// Align the buffer size to multiples of 256
	if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER))
	{
		allocationSize = round_up_64(allocationSize, pRenderer->pActiveGpuSettings->mUniformBufferAlignment);
	}

	DECLARE_ZERO(D3D12_RESOURCE_DESC, desc);
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	//Alignment must be 64KB (D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) or 0, which is effectively 64KB.
	//https://msdn.microsoft.com/en-us/library/windows/desktop/dn903813(v=vs.85).aspx
	desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	desc.Width = allocationSize;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hook_modify_buffer_resource_desc(pDesc, &desc);

	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER)
	{
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	// Adjust for padding
	UINT64 padded_size = 0;
	pRenderer->mD3D12.pDxDevice->GetCopyableFootprints(&desc, 0, 1, 0, NULL, NULL, NULL, &padded_size);
	allocationSize = (uint64_t)padded_size;
	desc.Width = padded_size;

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

	D3D12MA::ALLOCATION_DESC alloc_desc = {};

	if (RESOURCE_MEMORY_USAGE_CPU_ONLY == pDesc->mMemoryUsage || RESOURCE_MEMORY_USAGE_CPU_TO_GPU == pDesc->mMemoryUsage)
		alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
	else if (RESOURCE_MEMORY_USAGE_GPU_TO_CPU == pDesc->mMemoryUsage)
	{
		alloc_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
		desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}
	else
		alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	if (pDesc->mFlags & BUFFER_CREATION_FLAG_OWN_MEMORY_BIT)
		alloc_desc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;

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

#if defined(ENABLE_GRAPHICS_DEBUG)
	wchar_t debugName[MAX_DEBUG_NAME_LENGTH] = {};
	if (pDesc->pName)
	{
		mbstowcs(debugName, pDesc->pName, MAX_DEBUG_NAME_LENGTH);
	}
#endif

	// Special heap flags
	hook_modify_heap_flags(pDesc->mDescriptors, &alloc_desc.ExtraHeapFlags);

	// Create resource
	if (SUCCEEDED(hook_create_special_resource(pRenderer, &desc, NULL, res_states, pDesc->mFlags, &pBuffer->mD3D12.pDxResource)))
	{
		LOGF(LogLevel::eINFO, "Allocated memory in special platform-specific RAM");
	}
	// #TODO: This is not at all good but seems like virtual textures are using this
	// Remove as soon as possible
	else if (D3D12_HEAP_TYPE_DEFAULT != alloc_desc.HeapType && (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
	{
		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_CUSTOM;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		heapProps.VisibleNodeMask = alloc_desc.VisibleNodeMask;
		heapProps.CreationNodeMask = alloc_desc.CreationNodeMask;
		CHECK_HRESULT(pRenderer->mD3D12.pDxDevice->CreateCommittedResource(
			&heapProps, alloc_desc.ExtraHeapFlags, &desc, res_states, NULL, IID_ARGS(&pBuffer->mD3D12.pDxResource)));
	}
	else
	{
		CHECK_HRESULT(pRenderer->mD3D12.pResourceAllocator->CreateResource(
			&alloc_desc, &desc, res_states, NULL, &pBuffer->mD3D12.pDxAllocation, IID_ARGS(&pBuffer->mD3D12.pDxResource)));

		// Set name
#if defined(ENABLE_GRAPHICS_DEBUG)
		pBuffer->mD3D12.pDxAllocation->SetName(debugName);
#endif
	}

	if (pDesc->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && pDesc->mFlags & BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT)
		pBuffer->mD3D12.pDxResource->Map(0, NULL, &pBuffer->pCpuMappedAddress);

	pBuffer->mD3D12.mDxGpuAddress = pBuffer->mD3D12.pDxResource->GetGPUVirtualAddress();
#if defined(XBOX)
	pBuffer->pCpuMappedAddress = (void*)pBuffer->mD3D12.mDxGpuAddress;
#endif

	if (!(pDesc->mFlags & BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION))
	{
		DescriptorHeap* pHeap = pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
		uint32_t        handleCount = ((pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER) ? 1 : 0) +
							   ((pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER) ? 1 : 0) +
							   ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER) ? 1 : 0);
		pBuffer->mD3D12.mDescriptors = consume_descriptor_handles(pHeap, handleCount);

		if (pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		{
			pBuffer->mD3D12.mSrvDescriptorOffset = 1;

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = pBuffer->mD3D12.mDxGpuAddress;
			cbvDesc.SizeInBytes = (UINT)allocationSize;
			add_cbv(pRenderer, &cbvDesc, &pBuffer->mD3D12.mDescriptors);
		}

		if (pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER)
		{
			DxDescriptorID srv = pBuffer->mD3D12.mDescriptors + pBuffer->mD3D12.mSrvDescriptorOffset;
			pBuffer->mD3D12.mUavDescriptorOffset = pBuffer->mD3D12.mSrvDescriptorOffset + 1;

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = pDesc->mFirstElement;
			srvDesc.Buffer.NumElements = (UINT)(pDesc->mElementCount);
			srvDesc.Buffer.StructureByteStride = (UINT)(pDesc->mStructStride);
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			srvDesc.Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mFormat);
			if (DESCRIPTOR_TYPE_BUFFER_RAW == (pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER_RAW))
			{
				if (pDesc->mFormat != TinyImageFormat_UNDEFINED)
					LOGF(LogLevel::eWARNING, "Raw buffers use R32 typeless format. Format will be ignored");
				srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				srvDesc.Buffer.Flags |= D3D12_BUFFER_SRV_FLAG_RAW;
			}
			// Cannot create a typed StructuredBuffer
			if (srvDesc.Format != DXGI_FORMAT_UNKNOWN)
			{
				srvDesc.Buffer.StructureByteStride = 0;
			}

			add_srv(pRenderer, pBuffer->mD3D12.pDxResource, &srvDesc, &srv);
		}

		if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER)
		{
			DxDescriptorID uav = pBuffer->mD3D12.mDescriptors + pBuffer->mD3D12.mUavDescriptorOffset;

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = pDesc->mFirstElement;
			uavDesc.Buffer.NumElements = (UINT)(pDesc->mElementCount);
			uavDesc.Buffer.StructureByteStride = (UINT)(pDesc->mStructStride);
			uavDesc.Buffer.CounterOffsetInBytes = 0;
			uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
			if (DESCRIPTOR_TYPE_RW_BUFFER_RAW == (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER_RAW))
			{
				if (pDesc->mFormat != TinyImageFormat_UNDEFINED)
					LOGF(LogLevel::eWARNING, "Raw buffers use R32 typeless format. Format will be ignored");
				uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				uavDesc.Buffer.Flags |= D3D12_BUFFER_UAV_FLAG_RAW;
			}
			else if (pDesc->mFormat != TinyImageFormat_UNDEFINED)
			{
				uavDesc.Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mFormat);
				D3D12_FEATURE_DATA_FORMAT_SUPPORT FormatSupport = { uavDesc.Format, D3D12_FORMAT_SUPPORT1_NONE,
																	D3D12_FORMAT_SUPPORT2_NONE };
				HRESULT                           hr =
					pRenderer->mD3D12.pDxDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &FormatSupport, sizeof(FormatSupport));
				if (!SUCCEEDED(hr) || !(FormatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) ||
					!(FormatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
				{
					// Format does not support UAV Typed Load
					LOGF(LogLevel::eWARNING, "Cannot use Typed UAV for buffer format %u", (uint32_t)pDesc->mFormat);
					uavDesc.Format = DXGI_FORMAT_UNKNOWN;
				}
			}
			// Cannot create a typed RWStructuredBuffer
			if (uavDesc.Format != DXGI_FORMAT_UNKNOWN)
			{
				uavDesc.Buffer.StructureByteStride = 0;
			}

			ID3D12Resource* pCounterResource = pDesc->pCounterBuffer ? pDesc->pCounterBuffer->mD3D12.pDxResource : NULL;
			add_uav(pRenderer, pBuffer->mD3D12.pDxResource, pCounterResource, &uavDesc, &uav);
		}
	}

#if defined(ENABLE_GRAPHICS_DEBUG)
	if (pDesc->pName)
	{
		// Set name
		pBuffer->mD3D12.pDxResource->SetName(debugName);
	}
#endif

	pBuffer->mSize = (uint32_t)pDesc->mSize;
	pBuffer->mMemoryUsage = pDesc->mMemoryUsage;
	pBuffer->mNodeIndex = pDesc->mNodeIndex;
	pBuffer->mDescriptors = pDesc->mDescriptors;

	*ppBuffer = pBuffer;
}

void d3d12_removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	UNREF_PARAM(pRenderer);
	ASSERT(pRenderer);
	ASSERT(pBuffer);

	if (pBuffer->mD3D12.mDescriptors != D3D12_DESCRIPTOR_ID_NONE)
	{
		uint32_t handleCount = ((pBuffer->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER) ? 1 : 0) +
							   ((pBuffer->mDescriptors & DESCRIPTOR_TYPE_BUFFER) ? 1 : 0) +
							   ((pBuffer->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER) ? 1 : 0);
		return_descriptor_handles(
			pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], pBuffer->mD3D12.mDescriptors,
			handleCount);
	}

	SAFE_RELEASE(pBuffer->mD3D12.pDxAllocation);
	SAFE_RELEASE(pBuffer->mD3D12.pDxResource);

	SAFE_FREE(pBuffer);
}

void d3d12_mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
	UNREF_PARAM(pRenderer);
	ASSERT(pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to map non-cpu accessible resource");

	D3D12_RANGE range = { 0, pBuffer->mSize };
	if (pRange)
	{
		range.Begin += pRange->mOffset;
		range.End = range.Begin + pRange->mSize;
	}

	CHECK_HRESULT(pBuffer->mD3D12.pDxResource->Map(0, &range, &pBuffer->pCpuMappedAddress));
}

void d3d12_unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	UNREF_PARAM(pRenderer);
	ASSERT(pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to unmap non-cpu accessible resource");

	pBuffer->mD3D12.pDxResource->Unmap(0, NULL);
	pBuffer->pCpuMappedAddress = NULL;
}

void d3d12_addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mWidth && pDesc->mHeight && (pDesc->mDepth || pDesc->mArraySize));
	ASSERT(pRenderer->mGpuMode != GPU_MODE_UNLINKED || pDesc->mNodeIndex == pRenderer->mUnlinkedRendererIndex);
	if (pDesc->mSampleCount > SAMPLE_COUNT_1 && pDesc->mMipLevels > 1)
	{
		LOGF(LogLevel::eERROR, "Multi-Sampled textures cannot have mip maps");
		ASSERT(false);
		return;
	}

	//allocate new texture
	Texture* pTexture = (Texture*)tf_calloc_memalign(1, alignof(Texture), sizeof(Texture));
	pTexture->mD3D12.mDescriptors = D3D12_DESCRIPTOR_ID_NONE;
	ASSERT(pTexture);

	if (pDesc->pNativeHandle)
	{
		pTexture->mOwnsImage = false;
		pTexture->mD3D12.pDxResource = (ID3D12Resource*)pDesc->pNativeHandle;
	}
	else
	{
		pTexture->mOwnsImage = true;
	}

	//add to gpu
	D3D12_RESOURCE_DESC desc = {};

	DXGI_FORMAT dxFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mFormat);

	DescriptorType descriptors = pDesc->mDescriptors;

#if defined(ENABLE_GRAPHICS_DEBUG)
	wchar_t debugName[MAX_DEBUG_NAME_LENGTH] = {};
	if (pDesc->pName)
	{
		mbstowcs(debugName, pDesc->pName, MAX_DEBUG_NAME_LENGTH);
	}
#endif

	ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

	if (NULL == pTexture->mD3D12.pDxResource)
	{
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

		desc.Dimension = res_dim;
		//On PC, If Alignment is set to 0, the runtime will use 4MB for MSAA textures and 64KB for everything else.
		//On XBox, We have to explicitlly assign D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT if MSAA is used
		desc.Alignment = (UINT)pDesc->mSampleCount > 1 ? D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT : 0;
		desc.Width = pDesc->mWidth;
		desc.Height = pDesc->mHeight;
		desc.DepthOrArraySize = (UINT16)(pDesc->mArraySize != 1 ? pDesc->mArraySize : pDesc->mDepth);
		desc.MipLevels = (UINT16)pDesc->mMipLevels;
		desc.Format = (DXGI_FORMAT)TinyImageFormat_DXGI_FORMATToTypeless((TinyImageFormat_DXGI_FORMAT)dxFormat);
		desc.SampleDesc.Count = (UINT)pDesc->mSampleCount;
		desc.SampleDesc.Quality = (UINT)pDesc->mSampleQuality;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		// VRS enforces the format
		if (pDesc->mStartState == RESOURCE_STATE_SHADING_RATE_SOURCE)
		{
			desc.Format = dxFormat;
		}

		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS data;
		data.Format = desc.Format;
		data.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
		data.SampleCount = desc.SampleDesc.Count;
		pRenderer->mD3D12.pDxDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(data));
		while (data.NumQualityLevels == 0 && data.SampleCount > 0)
		{
			LOGF(
				LogLevel::eWARNING, "Sample Count (%u) not supported. Trying a lower sample count (%u)", data.SampleCount,
				data.SampleCount / 2);
			data.SampleCount = desc.SampleDesc.Count / 2;
			pRenderer->mD3D12.pDxDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(data));
		}
		desc.SampleDesc.Count = data.SampleCount;

		ResourceState actualStartState = pDesc->mStartState;

		// Decide UAV flags
		if (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}

		// Decide render target flags
		if (pDesc->mStartState & RESOURCE_STATE_RENDER_TARGET)
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			actualStartState = (pDesc->mStartState > RESOURCE_STATE_RENDER_TARGET)
								   ? (pDesc->mStartState & (ResourceState)~RESOURCE_STATE_RENDER_TARGET)
								   : RESOURCE_STATE_RENDER_TARGET;
		}
		else if (pDesc->mStartState & RESOURCE_STATE_DEPTH_WRITE)
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			actualStartState = (pDesc->mStartState > RESOURCE_STATE_DEPTH_WRITE)
								   ? (pDesc->mStartState & (ResourceState)~RESOURCE_STATE_DEPTH_WRITE)
								   : RESOURCE_STATE_DEPTH_WRITE;
		}

		// Decide sharing flags
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT)
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		}

		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET)
		{
			actualStartState = RESOURCE_STATE_PRESENT;
		}

		hook_modify_texture_resource_flags(pDesc->mFlags, &desc.Flags);

		DECLARE_ZERO(D3D12_CLEAR_VALUE, clearValue);
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

		D3D12MA::ALLOCATION_DESC alloc_desc = {};
		alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT)
			alloc_desc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;

#if defined(XBOX)
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET)
		{
			alloc_desc.ExtraHeapFlags |= D3D12_HEAP_FLAG_ALLOW_DISPLAY;
			desc.Format = dxFormat;
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
		if (SUCCEEDED(
				hook_create_special_resource(pRenderer, &desc, pClearValue, res_states, pDesc->mFlags, &pTexture->mD3D12.pDxResource)))
		{
			LOGF(LogLevel::eINFO, "Allocated memory in special platform-specific RAM");
		}
		else
		{
			CHECK_HRESULT(pRenderer->mD3D12.pResourceAllocator->CreateResource(
				&alloc_desc, &desc, res_states, pClearValue, &pTexture->mD3D12.pDxAllocation, IID_ARGS(&pTexture->mD3D12.pDxResource)));
#if defined(ENABLE_GRAPHICS_DEBUG)
			// Set name
			pTexture->mD3D12.pDxAllocation->SetName(debugName);
#endif
		}
	}
	else
	{
		desc = pTexture->mD3D12.pDxResource->GetDesc();
		dxFormat = desc.Format;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC  srvDesc = {};
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

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
		default: break;
	}

	DescriptorHeap* pHeap = pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
	uint32_t        handleCount = (descriptors & DESCRIPTOR_TYPE_TEXTURE) ? 1 : 0;
	handleCount += (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE) ? pDesc->mMipLevels : 0;
	pTexture->mD3D12.mDescriptors = consume_descriptor_handles(pHeap, handleCount);

	if (descriptors & DESCRIPTOR_TYPE_TEXTURE)
	{
		ASSERT(srvDesc.ViewDimension != D3D12_SRV_DIMENSION_UNKNOWN);

		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = util_to_dx12_srv_format(dxFormat);
		add_srv(pRenderer, pTexture->mD3D12.pDxResource, &srvDesc, &pTexture->mD3D12.mDescriptors);
		++pTexture->mD3D12.mUavStartIndex;
	}

	if (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
	{
		uavDesc.Format = util_to_dx12_uav_format(dxFormat);
		for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
		{
			DxDescriptorID handle = pTexture->mD3D12.mDescriptors + i + pTexture->mD3D12.mUavStartIndex;

			uavDesc.Texture1DArray.MipSlice = i;
			if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
				uavDesc.Texture3D.WSize = desc.DepthOrArraySize / (UINT)pow(2.0, int(i));
			add_uav(pRenderer, pTexture->mD3D12.pDxResource, NULL, &uavDesc, &handle);
		}
	}

#if defined(ENABLE_GRAPHICS_DEBUG)
	if (pDesc->pName)
	{
		// Set name
		pTexture->mD3D12.pDxResource->SetName(debugName);
	}
#endif

	pTexture->mNodeIndex = pDesc->mNodeIndex;
	pTexture->mD3D12.mHandleCount = handleCount;
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

void d3d12_removeTexture(Renderer* pRenderer, Texture* pTexture)
{
	ASSERT(pRenderer);
	ASSERT(pTexture);

	// return texture descriptors
	if (pTexture->mD3D12.mDescriptors != D3D12_DESCRIPTOR_ID_NONE)
	{
		return_descriptor_handles(
			pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], pTexture->mD3D12.mDescriptors,
			pTexture->mD3D12.mHandleCount);
	}

	if (pTexture->mOwnsImage)
	{
		SAFE_RELEASE(pTexture->mD3D12.pDxAllocation);
		SAFE_RELEASE(pTexture->mD3D12.pDxResource);
	}

	if (pTexture->pSvt)
	{
		removeVirtualTexture(pRenderer, pTexture->pSvt);
	}

	SAFE_FREE(pTexture);
}

void d3d12_addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppRenderTarget);
	ASSERT(pRenderer->mGpuMode != GPU_MODE_UNLINKED || pDesc->mNodeIndex == pRenderer->mUnlinkedRendererIndex);

	const bool isDepth = TinyImageFormat_HasDepth(pDesc->mFormat);
	ASSERT(!((isDepth) && (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)) && "Cannot use depth stencil as UAV");

	((RenderTargetDesc*)pDesc)->mMipLevels = max(1U, pDesc->mMipLevels);

	RenderTarget* pRenderTarget = (RenderTarget*)tf_calloc_memalign(1, alignof(RenderTarget), sizeof(RenderTarget));
	ASSERT(pRenderTarget);

	//add to gpu
	DXGI_FORMAT dxFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mFormat);
	ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

	TextureDesc textureDesc = {};
	textureDesc.mArraySize = pDesc->mArraySize;
	textureDesc.mClearValue = pDesc->mClearValue;
	textureDesc.mDepth = pDesc->mDepth;
	textureDesc.mFlags = pDesc->mFlags;
	textureDesc.mFormat = pDesc->mFormat;
	textureDesc.mHeight = pDesc->mHeight;
	textureDesc.mMipLevels = pDesc->mMipLevels;
	textureDesc.mSampleCount = pDesc->mSampleCount;
	textureDesc.mSampleQuality = pDesc->mSampleQuality;
	textureDesc.mStartState = pDesc->mStartState;

	if (!isDepth)
		textureDesc.mStartState |= RESOURCE_STATE_RENDER_TARGET;
	else
		textureDesc.mStartState |= RESOURCE_STATE_DEPTH_WRITE;

	textureDesc.mWidth = pDesc->mWidth;
	textureDesc.pNativeHandle = pDesc->pNativeHandle;
	textureDesc.pName = pDesc->pName;
	textureDesc.mNodeIndex = pDesc->mNodeIndex;
	textureDesc.pSharedNodeIndices = pDesc->pSharedNodeIndices;
	textureDesc.mSharedNodeIndexCount = pDesc->mSharedNodeIndexCount;
	textureDesc.mDescriptors = pDesc->mDescriptors;
	if (!(pDesc->mFlags & TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET))
	{
		// Create SRV by default for a render target
		textureDesc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE;
	}

	addTexture(pRenderer, &textureDesc, &pRenderTarget->pTexture);

	D3D12_RESOURCE_DESC desc = pRenderTarget->pTexture->mD3D12.pDxResource->GetDesc();

	uint32_t handleCount = desc.MipLevels;
	if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		handleCount *= desc.DepthOrArraySize;
	handleCount += 1;

	DescriptorHeap* pHeap = isDepth ? pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]
									: pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
	pRenderTarget->mD3D12.mDescriptors = consume_descriptor_handles(pHeap, handleCount);

	if (isDepth)
		add_dsv(pRenderer, pRenderTarget->pTexture->mD3D12.pDxResource, dxFormat, 0, -1, &pRenderTarget->mD3D12.mDescriptors);
	else
		add_rtv(pRenderer, pRenderTarget->pTexture->mD3D12.pDxResource, dxFormat, 0, -1, &pRenderTarget->mD3D12.mDescriptors);

	for (uint32_t i = 0; i < desc.MipLevels; ++i)
	{
		if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
			(pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		{
			for (uint32_t j = 0; j < desc.DepthOrArraySize; ++j)
			{
				DxDescriptorID handle = pRenderTarget->mD3D12.mDescriptors + (1 + i * desc.DepthOrArraySize + j);

				if (isDepth)
					add_dsv(pRenderer, pRenderTarget->pTexture->mD3D12.pDxResource, dxFormat, i, j, &handle);
				else
					add_rtv(pRenderer, pRenderTarget->pTexture->mD3D12.pDxResource, dxFormat, i, j, &handle);
			}
		}
		else
		{
			DxDescriptorID handle = pRenderTarget->mD3D12.mDescriptors + 1 + i;

			if (isDepth)
				add_dsv(pRenderer, pRenderTarget->pTexture->mD3D12.pDxResource, dxFormat, i, -1, &handle);
			else
				add_rtv(pRenderer, pRenderTarget->pTexture->mD3D12.pDxResource, dxFormat, i, -1, &handle);
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

	*ppRenderTarget = pRenderTarget;
}

void d3d12_removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
	bool const isDepth = TinyImageFormat_HasDepth(pRenderTarget->mFormat);

	removeTexture(pRenderer, pRenderTarget->pTexture);

	const uint32_t depthOrArraySize = (uint32_t)(pRenderTarget->mArraySize * pRenderTarget->mDepth);
	uint32_t       handleCount = pRenderTarget->mMipLevels;
	if ((pRenderTarget->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pRenderTarget->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		handleCount *= depthOrArraySize;
	handleCount += 1;

	!isDepth
		? return_descriptor_handles(
			  pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV], pRenderTarget->mD3D12.mDescriptors, handleCount)
		: return_descriptor_handles(
			  pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV], pRenderTarget->mD3D12.mDescriptors, handleCount);

	SAFE_FREE(pRenderTarget);
}

void d3d12_addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->mD3D12.pDxDevice);
	ASSERT(ppSampler);
	ASSERT(pDesc->mCompareFunc < MAX_COMPARE_MODES);

	// initialize to zero
	Sampler* pSampler = (Sampler*)tf_calloc_memalign(1, alignof(Sampler), sizeof(Sampler));
	pSampler->mD3D12.mDescriptor = D3D12_DESCRIPTOR_ID_NONE;
	ASSERT(pSampler);
	
	//default sampler lod values
	//used if not overriden by mSetLodRange or not Linear mipmaps
	float minSamplerLod = 0;
	float maxSamplerLod = pDesc->mMipMapMode == MIPMAP_MODE_LINEAR ? D3D12_FLOAT32_MAX : 0;
	//user provided lods
	if(pDesc->mSetLodRange)
	{
		minSamplerLod = pDesc->mMinLod;
		maxSamplerLod = pDesc->mMaxLod;
	}

	D3D12_SAMPLER_DESC desc = {};
	//add sampler to gpu
	desc.Filter = util_to_dx12_filter(
		pDesc->mMinFilter, pDesc->mMagFilter, pDesc->mMipMapMode, pDesc->mMaxAnisotropy > 0.0f,
		(pDesc->mCompareFunc != CMP_NEVER ? true : false));
	desc.AddressU = util_to_dx12_texture_address_mode(pDesc->mAddressU);
	desc.AddressV = util_to_dx12_texture_address_mode(pDesc->mAddressV);
	desc.AddressW = util_to_dx12_texture_address_mode(pDesc->mAddressW);
	desc.MipLODBias = pDesc->mMipLodBias;
	desc.MaxAnisotropy = max((UINT)pDesc->mMaxAnisotropy, 1U);
	desc.ComparisonFunc = gDx12ComparisonFuncTranslator[pDesc->mCompareFunc];
	desc.BorderColor[0] = 0.0f;
	desc.BorderColor[1] = 0.0f;
	desc.BorderColor[2] = 0.0f;
	desc.BorderColor[3] = 0.0f;
	desc.MinLOD = minSamplerLod;
	desc.MaxLOD = maxSamplerLod;

	pSampler->mD3D12.mDesc = desc;
	add_sampler(pRenderer, &pSampler->mD3D12.mDesc, &pSampler->mD3D12.mDescriptor);

	*ppSampler = pSampler;
}

void d3d12_removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
	ASSERT(pRenderer);
	ASSERT(pSampler);

	// Nop op
	return_descriptor_handles(pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER], pSampler->mD3D12.mDescriptor, 1);

	SAFE_FREE(pSampler);
}
/************************************************************************/
// Shader Functions
/************************************************************************/
void d3d12_compileShader(
	Renderer* pRenderer, ShaderTarget shaderTarget, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code,
	bool enablePrimitiveId, uint32_t macroCount, ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint)
{
	if ((uint32_t)shaderTarget > pRenderer->mShaderTarget)
	{
		LOGF(
			LogLevel::eERROR,
			"Requested shader target (%u) is higher than the shader target that the renderer supports (%u). Shader wont be compiled",
			(uint32_t)shaderTarget, (uint32_t)pRenderer->mShaderTarget);
		return;
	}
	{
		IDxcLibrary*  pLibrary;
		IDxcCompiler* pCompiler;
		CHECK_HRESULT(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler)));
		CHECK_HRESULT(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&pLibrary)));

		/************************************************************************/
		// Determine shader target
		/************************************************************************/
		int major;
		int minor;
		switch (shaderTarget)
		{
			default:
			case shader_target_6_0:
			{
				major = 6;
				minor = 0;
				break;
			}
			case shader_target_6_1:
			{
				major = 6;
				minor = 1;
				break;
			}
			case shader_target_6_2:
			{
				major = 6;
				minor = 2;
				break;
			}
			case shader_target_6_3:
			{
				major = 6;
				minor = 3;
				break;
			}
			case shader_target_6_4:
			{
				major = 6;
				minor = 4;
				break;
			}
		}
		wchar_t target[16] = {};
		switch (stage)
		{
			case SHADER_STAGE_VERT: swprintf(target, L"vs_%d_%d", major, minor); break;
			case SHADER_STAGE_TESC: swprintf(target, L"hs_%d_%d", major, minor); break;
			case SHADER_STAGE_TESE: swprintf(target, L"ds_%d_%d", major, minor); break;
			case SHADER_STAGE_GEOM: swprintf(target, L"gs_%d_%d", major, minor); break;
			case SHADER_STAGE_FRAG: swprintf(target, L"ps_%d_%d", major, minor); break;
			case SHADER_STAGE_COMP: swprintf(target, L"cs_%d_%d", major, minor); break;
#ifdef D3D12_RAYTRACING_AVAILABLE
			case SHADER_STAGE_RAYTRACING:
			{
				swprintf(target, L"lib_%d_%d", major, minor);
				ASSERT(shaderTarget >= shader_target_6_3);
				break;
			}
#else
				return;
#endif
			default: break;
		}
		/************************************************************************/
		// Collect macros
		/************************************************************************/
		uint32_t namePoolSize = 0;
		for (uint32_t i = 0; i < macroCount; ++i)
		{
			namePoolSize += (uint32_t)strlen(pMacros[i].definition) + 1;
			namePoolSize += (uint32_t)strlen(pMacros[i].value) + 1;
		}
		WCHAR* namePool = NULL;
		if (namePoolSize)
			namePool = (WCHAR*)alloca(namePoolSize * sizeof(WCHAR));

		// Extract shader macro definitions into D3D_SHADER_MACRO scruct
		// Allocate Size+2 structs: one for D3D12 1 definition and one for null termination
		DxcDefine* macros = (DxcDefine*)alloca((macroCount + 1) * sizeof(DxcDefine));
		macros[0] = { L"D3D12", L"1" };
		WCHAR* pCurrent = namePool;
		for (uint32_t j = 0; j < macroCount; ++j)
		{
			uint32_t len = (uint32_t)strlen(pMacros[j].definition);
			mbstowcs(pCurrent, pMacros[j].definition, len);
			pCurrent[len] = L'\0';    //-V522
			macros[j + 1].Name = pCurrent;
			pCurrent += (len + 1);    //-V769

			len = (uint32_t)strlen(pMacros[j].value);
			mbstowcs(pCurrent, pMacros[j].value, len);
			pCurrent[len] = L'\0';
			macros[j + 1].Value = pCurrent;
			pCurrent += (len + 1);
		}
		/************************************************************************/
		// Create blob from the string
		/************************************************************************/
		IDxcBlobEncoding* pTextBlob;
		CHECK_HRESULT(pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)code, (UINT32)codeSize, 0, &pTextBlob));
		IDxcOperationResult* pResult;
		WCHAR                filename[FS_MAX_PATH] = {};
		char                 pathStr[FS_MAX_PATH] = { 0 };
		fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_SOURCES), fileName, pathStr);
		mbstowcs(filename, fileName, strlen(fileName));
		IDxcIncludeHandler* pInclude = NULL;
		pLibrary->CreateIncludeHandler(&pInclude);

		WCHAR* entryName = L"main";
		if (pEntryPoint != NULL)
		{
			entryName = (WCHAR*)tf_calloc(strlen(pEntryPoint) + 1, sizeof(WCHAR));
			mbstowcs(entryName, pEntryPoint, strlen(pEntryPoint));
		}

		/************************************************************************/
		// Compiler args
		/************************************************************************/

		uint32_t numCompileArgs = 0;
		char     directoryStr[FS_MAX_PATH] = { 0 };
		fsGetParentPath(pathStr, directoryStr);

		const wchar_t* compileArgs[MAX_COMPILE_ARGS];

		hook_modify_shader_compile_flags(stage, enablePrimitiveId, NULL, &numCompileArgs);
		// 4 is the number of manually added flags
		ASSERT(numCompileArgs < MAX_COMPILE_ARGS);
		hook_modify_shader_compile_flags(stage, enablePrimitiveId, compileArgs, &numCompileArgs);

		compileArgs[numCompileArgs++] = L"-Zi";
		ASSERT(numCompileArgs < MAX_COMPILE_ARGS);
		compileArgs[numCompileArgs++] = L"-all_resources_bound";
		ASSERT(numCompileArgs < MAX_COMPILE_ARGS);
#if defined(ENABLE_GRAPHICS_DEBUG)
		compileArgs[numCompileArgs++] = L"-Od";
#else
		compileArgs[numCompileArgs++] = L"-O3";
#endif

		// specify the parent directory as include path
		wchar_t directory[FS_MAX_PATH + 2] = L"-I";
		mbstowcs(directory + 2, directoryStr, strlen(directoryStr));
		ASSERT(numCompileArgs < MAX_COMPILE_ARGS);
		compileArgs[numCompileArgs++] = directory;

		CHECK_HRESULT(pCompiler->Compile(
			pTextBlob, filename, entryName, target, compileArgs, numCompileArgs, macros, macroCount + 1, pInclude,
			&pResult));

		if (pEntryPoint != NULL)
		{
			tf_free(entryName);
			entryName = NULL;
		}

		pInclude->Release();
		pLibrary->Release();
		pCompiler->Release();
		/************************************************************************/
		// Verify the result
		/************************************************************************/
		HRESULT resultCode;
		CHECK_HRESULT(pResult->GetStatus(&resultCode));
		if (FAILED(resultCode))
		{
			IDxcBlobEncoding* pError;
			CHECK_HRESULT(pResult->GetErrorBuffer(&pError));
			LOGF(LogLevel::eERROR, "%s", (char*)pError->GetBufferPointer());
			pError->Release();
			return;
		}
		/************************************************************************/
		// Collect blob
		/************************************************************************/
		IDxcBlob* pBlob;
		CHECK_HRESULT(pResult->GetResult(&pBlob));

		char* pByteCode = (char*)tf_malloc(pBlob->GetBufferSize());
		memcpy(pByteCode, pBlob->GetBufferPointer(), pBlob->GetBufferSize());
		pOut->mByteCodeSize = (uint32_t)pBlob->GetBufferSize();
		pOut->pByteCode = pByteCode;

		pBlob->Release();
		/************************************************************************/
		/************************************************************************/
	}
}

void d3d12_addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mStages);
	ASSERT(ppShaderProgram);

	size_t totalSize = sizeof(Shader);
	totalSize += sizeof(PipelineReflection);

	uint32_t reflectionCount = 0;
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		ShaderStage                  stage_mask = (ShaderStage)(1 << i);
		const BinaryShaderStageDesc* pStage = NULL;
		if (stage_mask == (pDesc->mStages & stage_mask))
		{
			switch (stage_mask)
			{
				case SHADER_STAGE_VERT: pStage = &pDesc->mVert; break;
				case SHADER_STAGE_HULL: pStage = &pDesc->mHull; break;
				case SHADER_STAGE_DOMN: pStage = &pDesc->mDomain; break;
				case SHADER_STAGE_GEOM: pStage = &pDesc->mGeom; break;
				case SHADER_STAGE_FRAG: pStage = &pDesc->mFrag; break;
#ifdef D3D12_RAYTRACING_AVAILABLE
				case SHADER_STAGE_RAYTRACING:
#endif
				case SHADER_STAGE_COMP: pStage = &pDesc->mComp; break;
				default: LOGF(LogLevel::eERROR, "Unknown shader stage %i", stage_mask); break;
			}

			totalSize += sizeof(ID3DBlob*);
			totalSize += sizeof(LPCWSTR);
			totalSize += (strlen(pStage->pEntryPoint) + 1) * sizeof(WCHAR);    //-V522
			++reflectionCount;
		}
	}

	Shader* pShaderProgram = (Shader*)tf_calloc(1, totalSize);
	ASSERT(pShaderProgram);

	pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1);    //-V1027
	pShaderProgram->mD3D12.pShaderBlobs = (IDxcBlobEncoding**)(pShaderProgram->pReflection + 1);
	pShaderProgram->mD3D12.pEntryNames = (LPCWSTR*)(pShaderProgram->mD3D12.pShaderBlobs + reflectionCount);
	pShaderProgram->mStages = pDesc->mStages;

	uint8_t* mem = (uint8_t*)(pShaderProgram->mD3D12.pEntryNames + reflectionCount);

	reflectionCount = 0;

	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		ShaderStage                  stage_mask = (ShaderStage)(1 << i);
		const BinaryShaderStageDesc* pStage = NULL;
		if (stage_mask == (pShaderProgram->mStages & stage_mask))
		{
			switch (stage_mask)
			{
				case SHADER_STAGE_VERT: pStage = &pDesc->mVert; break;
				case SHADER_STAGE_HULL: pStage = &pDesc->mHull; break;
				case SHADER_STAGE_DOMN: pStage = &pDesc->mDomain; break;
				case SHADER_STAGE_GEOM: pStage = &pDesc->mGeom; break;
				case SHADER_STAGE_FRAG: pStage = &pDesc->mFrag; break;

#ifdef D3D12_RAYTRACING_AVAILABLE
				case SHADER_STAGE_RAYTRACING:
#endif
				case SHADER_STAGE_COMP: pStage = &pDesc->mComp; break;

				default: LOGF(LogLevel::eERROR, "Unknown shader stage %i", stage_mask); break;
			}

			IDxcUtils* pUtils;
			CHECK_HRESULT(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));
			pUtils->CreateBlob(
				pStage->pByteCode, pStage->mByteCodeSize, DXC_CP_ACP, &pShaderProgram->mD3D12.pShaderBlobs[reflectionCount]);    //-V522
			pUtils->Release();

			d3d12_createShaderReflection(
				(uint8_t*)(pShaderProgram->mD3D12.pShaderBlobs[reflectionCount]->GetBufferPointer()),
				(uint32_t)pShaderProgram->mD3D12.pShaderBlobs[reflectionCount]->GetBufferSize(), stage_mask,
				&pShaderProgram->pReflection->mStageReflections[reflectionCount]);

			WCHAR* entryPointName = (WCHAR*)mem;
			mbstowcs((WCHAR*)entryPointName, pStage->pEntryPoint, strlen(pStage->pEntryPoint));
			pShaderProgram->mD3D12.pEntryNames[reflectionCount] = entryPointName;
			mem += (strlen(pStage->pEntryPoint) + 1) * sizeof(WCHAR);

			reflectionCount++;
		}
	}

	createPipelineReflection(pShaderProgram->pReflection->mStageReflections, reflectionCount, pShaderProgram->pReflection);

	addShaderDependencies(pShaderProgram, pDesc);

	*ppShaderProgram = pShaderProgram;
}

void d3d12_removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
	removeShaderDependencies(pShaderProgram);

	UNREF_PARAM(pRenderer);

	//remove given shader
	for (uint32_t i = 0; i < pShaderProgram->pReflection->mStageReflectionCount; ++i)
	{
		SAFE_RELEASE(pShaderProgram->mD3D12.pShaderBlobs[i]);
	}
	destroyPipelineReflection(pShaderProgram->pReflection);

	SAFE_FREE(pShaderProgram);
}
/************************************************************************/
// Root Signature Functions
/************************************************************************/
void d3d12_addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
{
	ASSERT(pRenderer->pActiveGpuSettings->mMaxRootSignatureDWORDS > 0);
	ASSERT(ppRootSignature);

	typedef struct StaticSampler
	{
		ShaderResource*	pShaderResource;
		Sampler*		pSampler;
	}StaticSampler;

	typedef struct StaticSamplerNode
	{
		char* key;
		Sampler* value;
	}StaticSamplerNode;

	static constexpr uint32_t	kMaxLayoutCount = DESCRIPTOR_UPDATE_FREQ_COUNT;
	UpdateFrequencyLayoutInfo	layouts[kMaxLayoutCount] = {};
	ShaderResource*             shaderResources = NULL;
	uint32_t*                   constantSizes = NULL;
	StaticSampler*				staticSamplers = NULL;
	ShaderStage					shaderStages = SHADER_STAGE_NONE;
	bool						useInputLayout = false;
	StaticSamplerNode*			staticSamplerMap = NULL;
	PipelineType				pipelineType = PIPELINE_TYPE_UNDEFINED;
	DescriptorIndexMap*			indexMap = NULL;
	sh_new_arena(staticSamplerMap);
	sh_new_arena(indexMap);

	for (uint32_t i = 0; i < pRootSignatureDesc->mStaticSamplerCount; ++i)
	{
		shput(staticSamplerMap, pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i]);
	}

	// Collect all unique shader resources in the given shaders
	// Resources are parsed by name (two resources named "XYZ" in two shaders will be considered the same resource)
	for (uint32_t sh = 0; sh < pRootSignatureDesc->mShaderCount; ++sh)
	{
		PipelineReflection const* pReflection = pRootSignatureDesc->ppShaders[sh]->pReflection;

		// Keep track of the used pipeline stages
		shaderStages |= pReflection->mShaderStages;

		if (pReflection->mShaderStages & SHADER_STAGE_COMP)
			pipelineType = PIPELINE_TYPE_COMPUTE;
#ifdef D3D12_RAYTRACING_AVAILABLE
		// All raytracing shader bindings use the SetComputeXXX methods to bind descriptors
		else if (pReflection->mShaderStages == SHADER_STAGE_RAYTRACING)
			pipelineType = PIPELINE_TYPE_COMPUTE;
#endif
		else
			pipelineType = PIPELINE_TYPE_GRAPHICS;

		if (pReflection->mShaderStages & SHADER_STAGE_VERT)
		{
			if (pReflection->mStageReflections[pReflection->mVertexStageIndex].mVertexInputsCount)
			{
				useInputLayout = true;
			}
		}
		for (uint32_t i = 0; i < pReflection->mShaderResourceCount; ++i)
		{
			ShaderResource const* pRes = &pReflection->pShaderResources[i];

			DescriptorIndexMap* pNode = shgetp_null(indexMap, pRes->name);

			// Find all unique resources
			if (pNode == NULL)
			{
				ShaderResource* pFound = NULL;
				for (ptrdiff_t i = 0; i < arrlen(shaderResources); ++i)
				{
					ShaderResource* pCurrent = &shaderResources[i];
					if  (pCurrent->type == pRes->type && 
						(pCurrent->used_stages == pRes->used_stages) &&
						(((pCurrent->reg ^ pRes->reg) | (pCurrent->set ^ pRes->set)) == 0))
					{
						pFound = pCurrent;
						break;
					}

				}
				if (!pFound)
				{
					shput(indexMap, pRes->name, (uint32_t)arrlenu(shaderResources));

					arrpush(shaderResources, *pRes);

					uint32_t constantSize = 0;

					if (pRes->type == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
					{
						for (uint32_t v = 0; v < pReflection->mVariableCount; ++v)
						{
							if (pReflection->pVariables[v].parent_index == i)
								constantSize += pReflection->pVariables[v].size;
						}
					}

					//shaderStages |= pRes->used_stages;
					arrpush(constantSizes, constantSize);
				}
				else
				{
					ASSERT(pRes->type == pFound->type);
					if (pRes->type != pFound->type)
					{
						LOGF(
							LogLevel::eERROR,
							"\nFailed to create root signature\n"
							"Shared shader resources %s and %s have mismatching types (%u) and (%u). All shader resources "
							"sharing the same register and space addRootSignature "
							"must have the same type",
							pRes->name, pFound->name, (uint32_t)pRes->type, (uint32_t)pFound->type);
						return;
					}

					uint32_t foundIndex = shget(indexMap, pFound->name);
					shput(indexMap, pRes->name, foundIndex);

					pFound->used_stages |= pRes->used_stages;
				}
			}
			// If the resource was already collected, just update the shader stage mask in case it is used in a different
			// shader stage in this case
			else
			{
				if (shaderResources[pNode->value].reg != pRes->reg) //-V::522, 595
				{
					LOGF(
						LogLevel::eERROR,
						"\nFailed to create root signature\n"
						"Shared shader resource %s has mismatching register. All shader resources "
						"shared by multiple shaders specified in addRootSignature "
						"have the same register and space",
						pRes->name);
					return;
				}
				if (shaderResources[pNode->value].set != pRes->set) //-V::522, 595
				{
					LOGF(
						LogLevel::eERROR,
						"\nFailed to create root signature\n"
						"Shared shader resource %s has mismatching space. All shader resources "
						"shared by multiple shaders specified in addRootSignature "
						"have the same register and space",
						pRes->name);
					return;
				}

				for (ptrdiff_t i = 0; i < arrlen(shaderResources); ++i)
				{
					if (strcmp(shaderResources[i].name, pNode->key) == 0)
					{
						shaderResources[i].used_stages |= pRes->used_stages;
						break;
					}
				}
			}
		}
	}

	size_t totalSize = sizeof(RootSignature);
	totalSize += arrlenu(shaderResources) * sizeof(DescriptorInfo);

	RootSignature* pRootSignature = (RootSignature*)tf_calloc_memalign(1, alignof(RootSignature), totalSize);
	ASSERT(pRootSignature);

	if ((uint32_t)arrlenu(shaderResources))
	{
		pRootSignature->mDescriptorCount = (uint32_t)arrlenu(shaderResources);
	}

	pRootSignature->pDescriptors = (DescriptorInfo*)(pRootSignature + 1);    //-V1027
	pRootSignature->pDescriptorNameToIndexMap = indexMap;
	ASSERT(pRootSignature->pDescriptorNameToIndexMap);

	pRootSignature->mPipelineType = pipelineType;

	// Fill the descriptor array to be stored in the root signature
	for (uint32_t i = 0; i < (uint32_t)arrlenu(shaderResources); ++i)
	{
		DescriptorInfo* pDesc = &pRootSignature->pDescriptors[i];
		ShaderResource* pRes = &shaderResources[i];
		uint32_t        setIndex = pRes->set;
		if (pRes->size == 0 || setIndex >= DESCRIPTOR_UPDATE_FREQ_COUNT)
			setIndex = 0;

		DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)setIndex;

		pDesc->mSize = pRes->size;
		pDesc->mType = pRes->type;
		pDesc->mDim = pRes->dim;
		pDesc->pName = pRes->name;
		pDesc->mUpdateFrequency = updateFreq;

		if (pDesc->mSize == 0 && pDesc->mType == DESCRIPTOR_TYPE_TEXTURE)
		{
			pDesc->mSize = pRootSignatureDesc->mMaxBindlessTextures;
		}

		// Find the D3D12 type of the descriptors
		if (pDesc->mType == DESCRIPTOR_TYPE_SAMPLER)
		{
			// If the sampler is a static sampler, no need to put it in the descriptor table
			StaticSamplerNode* pNode = shgetp_null(staticSamplerMap, pDesc->pName);

			if (pNode)
			{
				LOGF(LogLevel::eINFO, "Descriptor (%s) : User specified Static Sampler", pDesc->pName);
				// Set the index to invalid value so we can use this later for error checking if user tries to update a static sampler
				pDesc->mStaticSampler = true;
				StaticSampler sampler = { pRes, pNode->value };
				arrpush(staticSamplers, sampler);
			}
			else
			{
				// In D3D12, sampler descriptors cannot be placed in a table containing view descriptors
				RootParameter param = { *pRes, pDesc };
				arrpush(layouts[setIndex].mSamplerTable, param);
			}
		}
		// No support for arrays of constant buffers to be used as root descriptors as this might bloat the root signature size
		else if (pDesc->mType == DESCRIPTOR_TYPE_UNIFORM_BUFFER && pDesc->mSize == 1)
		{
			// D3D12 has no special syntax to declare root constants like Vulkan
			// So we assume that all constant buffers with the word "rootconstant", "pushconstant" (case insensitive) are root constants
			if (isDescriptorRootConstant(pRes->name))
			{
				// Make the root param a 32 bit constant if the user explicitly specifies it in the shader
				pDesc->mRootDescriptor = 1;
				pDesc->mType = DESCRIPTOR_TYPE_ROOT_CONSTANT;
				RootParameter param = { *pRes, pDesc };
				arrpush(layouts[setIndex].mRootConstants, param);

				pDesc->mSize = constantSizes[i] / sizeof(uint32_t);
			}
			// If a user specified a uniform buffer to be used directly in the root signature change its type to D3D12_ROOT_PARAMETER_TYPE_CBV
			// Also log a message for debugging purpose
			else if (isDescriptorRootCbv(pRes->name))
			{
				RootParameter param = { *pRes, pDesc };
				arrpush(layouts[setIndex].mRootDescriptorParams, param);
				pDesc->mRootDescriptor = 1;

				LOGF(LogLevel::eINFO, "Descriptor (%s) : User specified D3D12_ROOT_PARAMETER_TYPE_CBV", pDesc->pName);
			}
			else
			{
				RootParameter param = { *pRes, pDesc };
				arrpush(layouts[setIndex].mCbvSrvUavTable, param);
			}
		}
		else
		{
			RootParameter param = { *pRes, pDesc };
			arrpush(layouts[setIndex].mCbvSrvUavTable, param);
		}

		hmput(layouts[setIndex].mDescriptorIndexMap, pDesc, i);
	}

	// We should never reach inside this if statement. If we do, something got messed up
	if (pRenderer->pActiveGpuSettings->mMaxRootSignatureDWORDS < calculate_root_signature_size(layouts, kMaxLayoutCount))
	{
		LOGF(LogLevel::eWARNING, "Root Signature size greater than the specified max size");
		ASSERT(false);
	}

	// D3D12 currently has two versions of root signatures (1_0, 1_1)
	// So we fill the structs of both versions and in the end use the structs compatible with the supported version
	constexpr uint32_t         kMaxResourceTableSize = 32;
	D3D12_DESCRIPTOR_RANGE1    cbvSrvUavRange[kMaxLayoutCount][kMaxResourceTableSize] = {};
	D3D12_DESCRIPTOR_RANGE1    samplerRange[kMaxLayoutCount][kMaxResourceTableSize] = {};
	D3D12_ROOT_PARAMETER1      rootParams[D3D12_MAX_ROOT_COST] = {};
	uint32_t                   rootParamCount = 0;
	D3D12_STATIC_SAMPLER_DESC* staticSamplerDescs = NULL;
	uint32_t                   staticSamplerCount = (uint32_t)arrlenu(staticSamplers);

	if (staticSamplerCount)
	{
		staticSamplerDescs = (D3D12_STATIC_SAMPLER_DESC*)alloca(staticSamplerCount * sizeof(D3D12_STATIC_SAMPLER_DESC));

		for (uint32_t i = 0; i < staticSamplerCount; ++i)
		{
			D3D12_SAMPLER_DESC& desc = staticSamplers[i].pSampler->mD3D12.mDesc;
			staticSamplerDescs[i].Filter = desc.Filter;
			staticSamplerDescs[i].AddressU = desc.AddressU;
			staticSamplerDescs[i].AddressV = desc.AddressV;
			staticSamplerDescs[i].AddressW = desc.AddressW;
			staticSamplerDescs[i].MipLODBias = desc.MipLODBias;
			staticSamplerDescs[i].MaxAnisotropy = desc.MaxAnisotropy;
			staticSamplerDescs[i].ComparisonFunc = desc.ComparisonFunc;
			staticSamplerDescs[i].MinLOD = desc.MinLOD;
			staticSamplerDescs[i].MaxLOD = desc.MaxLOD;
			staticSamplerDescs[i].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;

			ShaderResource* samplerResource = staticSamplers[i].pShaderResource;
			staticSamplerDescs[i].RegisterSpace = samplerResource->set;
			staticSamplerDescs[i].ShaderRegister = samplerResource->reg;
			staticSamplerDescs[i].ShaderVisibility = util_to_dx12_shader_visibility(samplerResource->used_stages);
		}
	}

	for (uint32_t i = 0; i < kMaxLayoutCount; ++i)
	{
		if (arrlen(layouts[i].mCbvSrvUavTable))
		{
			ASSERT(arrlenu(layouts[i].mCbvSrvUavTable) <= kMaxResourceTableSize);
			++rootParamCount;
		}
		if (arrlen(layouts[i].mSamplerTable))
		{
			ASSERT(arrlenu(layouts[i].mSamplerTable) <= kMaxResourceTableSize);
			++rootParamCount;
		}
	}

	pRootSignature->mDescriptorCount = (uint32_t)arrlenu(shaderResources);

	for (uint32_t i = 0; i < kMaxLayoutCount; ++i)
	{
		rootParamCount += (uint32_t)arrlenu(layouts[i].mRootConstants);
		rootParamCount += (uint32_t)arrlenu(layouts[i].mRootDescriptorParams);
	}

	rootParamCount = 0;

	// Start collecting root parameters
	// Start with root descriptors since they will be the most frequently updated descriptors
	// This also makes sure that if we spill, the root descriptors in the front of the root signature will most likely still remain in the root
	// Collect all root descriptors
	// Put most frequently changed params first
	for (uint32_t i = kMaxLayoutCount; i-- > 0U;)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[i];
		if (arrlen(layout.mRootDescriptorParams))
		{
			ASSERT(1 == arrlen(layout.mRootDescriptorParams));

			uint32_t rootDescriptorIndex = 0;

			for (ptrdiff_t descIndex = 0; descIndex < arrlen(layout.mRootDescriptorParams); ++descIndex)
			{
				RootParameter* pDesc = &layout.mRootDescriptorParams[descIndex];
				pDesc->pDescriptorInfo->mHandleIndex = rootParamCount;

				D3D12_ROOT_PARAMETER1 rootParam;
				create_root_descriptor(pDesc, &rootParam);

				rootParams[rootParamCount++] = rootParam;

				++rootDescriptorIndex;
			}
		}
	}

	uint32_t rootConstantIndex = 0;

	// Collect all root constants
	for (uint32_t setIndex = 0; setIndex < kMaxLayoutCount; ++setIndex)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[setIndex];

		if (!arrlen(layout.mRootConstants))
			continue;

		for (ptrdiff_t i = 0; i < arrlen(layouts[setIndex].mRootConstants); ++i)
		{
			RootParameter* pDesc = &layout.mRootConstants[i];
			pDesc->pDescriptorInfo->mHandleIndex = rootParamCount;

			D3D12_ROOT_PARAMETER1 rootParam;
			create_root_constant(pDesc, &rootParam);

			rootParams[rootParamCount++] = rootParam;

			if (pDesc->pDescriptorInfo->mSize > gMaxRootConstantsPerRootParam)
			{
				//64 DWORDS for NVIDIA, 16 for AMD but 3 are used by driver so we get 13 SGPR
				//DirectX12
				//Root descriptors - 2
				//Root constants - Number of 32 bit constants
				//Descriptor tables - 1
				//Static samplers - 0
				LOGF(
					LogLevel::eINFO,
					"Root constant (%s) has (%u) 32 bit values. It is recommended to have root constant number <= %u",
					pDesc->pDescriptorInfo->pName, pDesc->pDescriptorInfo->mSize, gMaxRootConstantsPerRootParam);
			}

			++rootConstantIndex;
		}
	}

	// Collect descriptor table parameters
	// Put most frequently changed descriptor tables in the front of the root signature
	for (uint32_t i = kMaxLayoutCount; i-- > 0U;)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[i];

		// Fill the descriptor table layout for the view descriptor table of this update frequency
		if (arrlen(layout.mCbvSrvUavTable))
		{
			// sort table by type (CBV/SRV/UAV) by register by space
			sortRootParameter(layout.mCbvSrvUavTable, arrlenu(layout.mCbvSrvUavTable));

			D3D12_ROOT_PARAMETER1 rootParam;
			create_descriptor_table((uint32_t)arrlenu(layout.mCbvSrvUavTable), layout.mCbvSrvUavTable, cbvSrvUavRange[i], &rootParam);

			// Store some of the binding info which will be required later when binding the descriptor table
			// We need the root index when calling SetRootDescriptorTable
			pRootSignature->mD3D12.mDxViewDescriptorTableRootIndices[i] = rootParamCount;
			pRootSignature->mD3D12.mDxViewDescriptorCounts[i] = (uint32_t)arrlenu(layout.mCbvSrvUavTable);

			for (ptrdiff_t descIndex = 0; descIndex < arrlen(layout.mCbvSrvUavTable); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mCbvSrvUavTable[descIndex].pDescriptorInfo;

				// Store the d3d12 related info in the descriptor to avoid constantly calling the util_to_dx mapping functions
				pDesc->mRootDescriptor = 0;
				pDesc->mHandleIndex = pRootSignature->mD3D12.mDxCumulativeViewDescriptorCounts[i];

				// Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
				// This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays) in the descriptor table
				pRootSignature->mD3D12.mDxCumulativeViewDescriptorCounts[i] += pDesc->mSize;
			}

			rootParams[rootParamCount++] = rootParam;
		}

		// Fill the descriptor table layout for the sampler descriptor table of this update frequency
		if (arrlen(layout.mSamplerTable))
		{
			D3D12_ROOT_PARAMETER1 rootParam;
			create_descriptor_table((uint32_t)arrlenu(layout.mSamplerTable), layout.mSamplerTable, samplerRange[i], &rootParam);

			// Store some of the binding info which will be required later when binding the descriptor table
			// We need the root index when calling SetRootDescriptorTable
			pRootSignature->mD3D12.mDxSamplerDescriptorTableRootIndices[i] = rootParamCount;
			pRootSignature->mD3D12.mDxSamplerDescriptorCounts[i] = (uint32_t)arrlenu(layout.mSamplerTable);
			//table.pDescriptorIndices = (uint32_t*)tf_calloc(table.mDescriptorCount, sizeof(uint32_t));

			for (ptrdiff_t descIndex = 0; descIndex < arrlen(layout.mSamplerTable); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mSamplerTable[descIndex].pDescriptorInfo;

				// Store the d3d12 related info in the descriptor to avoid constantly calling the util_to_dx mapping functions
				pDesc->mRootDescriptor = 0;
				pDesc->mHandleIndex = pRootSignature->mD3D12.mDxCumulativeSamplerDescriptorCounts[i];

				// Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
				// This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays) in the descriptor table
				pRootSignature->mD3D12.mDxCumulativeSamplerDescriptorCounts[i] += pDesc->mSize;
			}

			rootParams[rootParamCount++] = rootParam;
		}
	}

	// Specify the deny flags to avoid unnecessary shader stages being notified about descriptor modifications
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	if (useInputLayout)
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	if (!(shaderStages & SHADER_STAGE_VERT))
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
	if (!(shaderStages & SHADER_STAGE_HULL))
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
	if (!(shaderStages & SHADER_STAGE_DOMN))
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
	if (!(shaderStages & SHADER_STAGE_GEOM))
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
	if (!(shaderStages & SHADER_STAGE_FRAG))
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
#ifdef D3D12_RAYTRACING_AVAILABLE
	if (pRootSignatureDesc->mFlags & ROOT_SIGNATURE_FLAG_LOCAL_BIT)
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
#endif

	hook_modify_rootsignature_flags(shaderStages, &rootSignatureFlags);

	ID3DBlob* error = NULL;
	ID3DBlob* rootSignatureString = NULL;
	DECLARE_ZERO(D3D12_VERSIONED_ROOT_SIGNATURE_DESC, desc);
	desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	desc.Desc_1_1.NumParameters = rootParamCount;
	desc.Desc_1_1.pParameters = rootParams;
	desc.Desc_1_1.NumStaticSamplers = staticSamplerCount;
	desc.Desc_1_1.pStaticSamplers = staticSamplerDescs;
	desc.Desc_1_1.Flags = rootSignatureFlags;

	HRESULT hres = D3D12SerializeVersionedRootSignature(&desc, &rootSignatureString, &error);

	if (!SUCCEEDED(hres))
	{
		LOGF(LogLevel::eERROR, "Failed to serialize root signature with error (%s)", (char*)error->GetBufferPointer());
	}

	// If running Linked Mode (SLI) create root signature for all nodes
	// #NOTE : In non SLI mode, mNodeCount will be 0 which sets nodeMask to default value
	CHECK_HRESULT(pRenderer->mD3D12.pDxDevice->CreateRootSignature(
		util_calculate_shared_node_mask(pRenderer), rootSignatureString->GetBufferPointer(), rootSignatureString->GetBufferSize(),
		IID_ARGS(&pRootSignature->mD3D12.pDxRootSignature)));

	SAFE_RELEASE(error);
	SAFE_RELEASE(rootSignatureString);
	for (uint32_t i = 0; i < kMaxLayoutCount; ++i)
	{
		UpdateFrequencyLayoutInfo* pLayout = &layouts[i];
		arrfree(pLayout->mCbvSrvUavTable);
		arrfree(pLayout->mSamplerTable);
		arrfree(pLayout->mRootDescriptorParams);
		arrfree(pLayout->mRootConstants);
		hmfree(pLayout->mDescriptorIndexMap);
	}

	arrfree(shaderResources);
	arrfree(constantSizes);
	arrfree(staticSamplers);
	shfree(staticSamplerMap);

	addRootSignatureDependencies(pRootSignature, pRootSignatureDesc);

	*ppRootSignature = pRootSignature;
}

void d3d12_removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
	removeRootSignatureDependencies(pRootSignature);

	shfree(pRootSignature->pDescriptorNameToIndexMap);
	SAFE_RELEASE(pRootSignature->mD3D12.pDxRootSignature);

	SAFE_FREE(pRootSignature);
}
/************************************************************************/
// Descriptor Set Functions
/************************************************************************/
void d3d12_addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppDescriptorSet);

	const RootSignature*            pRootSignature = pDesc->pRootSignature;
	const DescriptorUpdateFrequency updateFreq = pDesc->mUpdateFrequency;
	const uint32_t                  nodeIndex = pRenderer->mGpuMode == GPU_MODE_LINKED ? pDesc->mNodeIndex : 0;
	const uint32_t                  cbvSrvUavDescCount = pRootSignature->mD3D12.mDxCumulativeViewDescriptorCounts[updateFreq];
	const uint32_t                  samplerDescCount = pRootSignature->mD3D12.mDxCumulativeSamplerDescriptorCounts[updateFreq];

	DescriptorSet* pDescriptorSet = (DescriptorSet*)tf_calloc_memalign(1, alignof(DescriptorSet), sizeof(DescriptorSet));
	ASSERT(pDescriptorSet);

	pDescriptorSet->mD3D12.pRootSignature = pRootSignature;
	pDescriptorSet->mD3D12.mUpdateFrequency = updateFreq;
	pDescriptorSet->mD3D12.mNodeIndex = nodeIndex;
	pDescriptorSet->mD3D12.mMaxSets = pDesc->mMaxSets;
	pDescriptorSet->mD3D12.mCbvSrvUavRootIndex = pRootSignature->mD3D12.mDxViewDescriptorTableRootIndices[updateFreq];
	pDescriptorSet->mD3D12.mSamplerRootIndex = pRootSignature->mD3D12.mDxSamplerDescriptorTableRootIndices[updateFreq];
	pDescriptorSet->mD3D12.mCbvSrvUavHandle = D3D12_DESCRIPTOR_ID_NONE;
	pDescriptorSet->mD3D12.mSamplerHandle = D3D12_DESCRIPTOR_ID_NONE;
	pDescriptorSet->mD3D12.mPipelineType = pRootSignature->mPipelineType;

	if (cbvSrvUavDescCount || samplerDescCount)
	{
		if (cbvSrvUavDescCount)
		{
			DescriptorHeap*                  pSrcHeap = pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
			DescriptorHeap*                  pHeap = pRenderer->mD3D12.pCbvSrvUavHeaps[nodeIndex];
			pDescriptorSet->mD3D12.mCbvSrvUavHandle = consume_descriptor_handles(pHeap, cbvSrvUavDescCount * pDesc->mMaxSets);
			pDescriptorSet->mD3D12.mCbvSrvUavStride = cbvSrvUavDescCount;

			for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
			{
				const DescriptorInfo* pDescInfo = &pRootSignature->pDescriptors[i];
				if (!pDescInfo->mRootDescriptor && pDescInfo->mType != DESCRIPTOR_TYPE_SAMPLER && pDescInfo->mUpdateFrequency == updateFreq)
				{
					DescriptorType              type = (DescriptorType)pDescInfo->mType;
					DxDescriptorID srcHandle = D3D12_DESCRIPTOR_ID_NONE;
					switch (type)
					{
						case DESCRIPTOR_TYPE_TEXTURE: srcHandle = pRenderer->pNullDescriptors->mNullTextureSRV[pDescInfo->mDim]; break;
						case DESCRIPTOR_TYPE_BUFFER: srcHandle = pRenderer->pNullDescriptors->mNullBufferSRV; break;
						case DESCRIPTOR_TYPE_RW_TEXTURE: srcHandle = pRenderer->pNullDescriptors->mNullTextureUAV[pDescInfo->mDim]; break;
						case DESCRIPTOR_TYPE_RW_BUFFER: srcHandle = pRenderer->pNullDescriptors->mNullBufferUAV; break;
						case DESCRIPTOR_TYPE_UNIFORM_BUFFER: srcHandle = pRenderer->pNullDescriptors->mNullBufferCBV; break;
						default: break;
					}

#ifdef D3D12_RAYTRACING_AVAILABLE
					if (pDescInfo->mType != DESCRIPTOR_TYPE_RAY_TRACING)
#endif
					{
						ASSERT(srcHandle != D3D12_DESCRIPTOR_ID_NONE);

						for (uint32_t s = 0; s < pDesc->mMaxSets; ++s)
							for (uint32_t j = 0; j < pDescInfo->mSize; ++j)
								copy_descriptor_handle(
									pSrcHeap, srcHandle,
									pHeap, pDescriptorSet->mD3D12.mCbvSrvUavHandle + s * pDescriptorSet->mD3D12.mCbvSrvUavStride + pDescInfo->mHandleIndex + j);
					}
				}
			}
		}
		if (samplerDescCount)
		{
			DescriptorHeap*                  pSrcHeap = pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
			DescriptorHeap*                  pHeap = pRenderer->mD3D12.pSamplerHeaps[nodeIndex];
			pDescriptorSet->mD3D12.mSamplerHandle = consume_descriptor_handles(pHeap, samplerDescCount * pDesc->mMaxSets);
			pDescriptorSet->mD3D12.mSamplerStride = samplerDescCount;
			for (uint32_t i = 0; i < pDesc->mMaxSets; ++i)
			{
				for (uint32_t j = 0; j < samplerDescCount; ++j)
					copy_descriptor_handle(
						pSrcHeap, pRenderer->pNullDescriptors->mNullSampler,
						pHeap, pDescriptorSet->mD3D12.mSamplerHandle + i * pDescriptorSet->mD3D12.mSamplerStride + j);
			}
		}
	}

	*ppDescriptorSet = pDescriptorSet;
}

void d3d12_removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
{
	ASSERT(pRenderer);
	ASSERT(pDescriptorSet);

	if (pDescriptorSet->mD3D12.mCbvSrvUavHandle != D3D12_DESCRIPTOR_ID_NONE)
	{
		return_descriptor_handles(pRenderer->mD3D12.pCbvSrvUavHeaps[pDescriptorSet->mD3D12.mNodeIndex],
			pDescriptorSet->mD3D12.mCbvSrvUavHandle, pDescriptorSet->mD3D12.mCbvSrvUavStride * pDescriptorSet->mD3D12.mMaxSets);
	}

	if (pDescriptorSet->mD3D12.mSamplerHandle != D3D12_DESCRIPTOR_ID_NONE)
	{
		return_descriptor_handles(pRenderer->mD3D12.pSamplerHeaps[pDescriptorSet->mD3D12.mNodeIndex],
			pDescriptorSet->mD3D12.mSamplerHandle, pDescriptorSet->mD3D12.mSamplerStride * pDescriptorSet->mD3D12.mMaxSets);
	}

	pDescriptorSet->mD3D12.mCbvSrvUavHandle = D3D12_DESCRIPTOR_ID_NONE;
	pDescriptorSet->mD3D12.mSamplerHandle = D3D12_DESCRIPTOR_ID_NONE;

	SAFE_FREE(pDescriptorSet);
}

#if defined(ENABLE_GRAPHICS_DEBUG)
#define VALIDATE_DESCRIPTOR(descriptor, ...)                                                            \
	if (!(descriptor))                                                                                  \
	{                                                                                                   \
		LOGF(LogLevel::eERROR,  __FUNCTION__ " : " __VA_ARGS__);                                        \
		_FailedAssert(__FILE__, __LINE__, __FUNCTION__);												\
		continue;                                                                                       \
	}
#else
#define VALIDATE_DESCRIPTOR(descriptor, ...)
#endif

void d3d12_updateDescriptorSet(
	Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
	ASSERT(pRenderer);
	ASSERT(pDescriptorSet);
	ASSERT(index < pDescriptorSet->mD3D12.mMaxSets);

	const RootSignature*            pRootSignature = pDescriptorSet->mD3D12.pRootSignature;
	const DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)pDescriptorSet->mD3D12.mUpdateFrequency;
	const uint32_t                  nodeIndex = pDescriptorSet->mD3D12.mNodeIndex;

	for (uint32_t i = 0; i < count; ++i)
	{
		const DescriptorData* pParam = pParams + i;
		uint32_t              paramIndex = pParam->mBindByIndex ? pParam->mIndex : UINT32_MAX;

		VALIDATE_DESCRIPTOR(pParam->pName || (paramIndex != UINT32_MAX), "DescriptorData has NULL name and invalid index");

		const DescriptorInfo* pDesc =
			(paramIndex != UINT32_MAX) ? (pRootSignature->pDescriptors + paramIndex) : d3d12_get_descriptor(pRootSignature, pParam->pName);
		if (paramIndex != UINT32_MAX)
		{
			VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param index (%u)", paramIndex);
		}
		else
		{
			VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param name (%s)", pParam->pName);
		}

		const DescriptorType type = (DescriptorType)pDesc->mType;    //-V522
		const uint32_t       arrayStart = pParam->mArrayOffset;
		const uint32_t       arrayCount = max(1U, pParam->mCount);

		VALIDATE_DESCRIPTOR(
			pDesc->mUpdateFrequency == updateFreq, "Descriptor (%s) - Mismatching update frequency and register space", pDesc->pName);

		if (pDesc->mRootDescriptor)
		{
			VALIDATE_DESCRIPTOR(
				false,
				"Descriptor (%s) - Trying to update a root cbv through updateDescriptorSet. All root cbvs must be updated through cmdBindDescriptorSetWithRootCbvs",
				pDesc->pName);
		}
		else if (type == DESCRIPTOR_TYPE_SAMPLER)
		{
			// Index is invalid when descriptor is a static sampler
			VALIDATE_DESCRIPTOR(
				!pDesc->mStaticSampler,
				"Trying to update a static sampler (%s). All static samplers must be set in addRootSignature and cannot be updated later",
				pDesc->pName);

			VALIDATE_DESCRIPTOR(pParam->ppSamplers, "NULL Sampler (%s)", pDesc->pName);

			for (uint32_t arr = 0; arr < arrayCount; ++arr)
			{
				VALIDATE_DESCRIPTOR(
					pParam->ppSamplers[arr] != D3D12_GPU_VIRTUAL_ADDRESS_NULL, "NULL Sampler (%s [%u] )", pDesc->pName, arr);

				copy_descriptor_handle(
					pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER], pParam->ppSamplers[arr]->mD3D12.mDescriptor,
					pRenderer->mD3D12.pSamplerHeaps[nodeIndex],
					pDescriptorSet->mD3D12.mSamplerHandle + index * pDescriptorSet->mD3D12.mSamplerStride + pDesc->mHandleIndex + arrayStart + arr);
			}
		}
		else
		{
			switch (type)
			{
				case DESCRIPTOR_TYPE_TEXTURE:
				{
					VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)", pDesc->pName);

					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);

						copy_descriptor_handle(
							pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], pParam->ppTextures[arr]->mD3D12.mDescriptors,
							pRenderer->mD3D12.pCbvSrvUavHeaps[nodeIndex],
							pDescriptorSet->mD3D12.mCbvSrvUavHandle + index * pDescriptorSet->mD3D12.mCbvSrvUavStride +
							pDesc->mHandleIndex + arrayStart + arr);
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
							DxDescriptorID srcId = pParam->ppTextures[0]->mD3D12.mDescriptors + arr + pParam->ppTextures[0]->mD3D12.mUavStartIndex;

							copy_descriptor_handle(
								pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
								pRenderer->mD3D12.pCbvSrvUavHeaps[nodeIndex],
								pDescriptorSet->mD3D12.mCbvSrvUavHandle + index * pDescriptorSet->mD3D12.mCbvSrvUavStride +
								pDesc->mHandleIndex + arrayStart + arr);
						}
					}
					else
					{
						for (uint32_t arr = 0; arr < arrayCount; ++arr)
						{
							VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL RW Texture (%s [%u] )", pDesc->pName, arr);

							DxDescriptorID srcId = pParam->ppTextures[arr]->mD3D12.mDescriptors +
								pParam->mUAVMipSlice + pParam->ppTextures[arr]->mD3D12.mUavStartIndex;

							copy_descriptor_handle(
								pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
								pRenderer->mD3D12.pCbvSrvUavHeaps[nodeIndex],
								pDescriptorSet->mD3D12.mCbvSrvUavHandle + index * pDescriptorSet->mD3D12.mCbvSrvUavStride +
								pDesc->mHandleIndex + arrayStart + arr);
						}
					}
					break;
				}
				case DESCRIPTOR_TYPE_BUFFER:
				case DESCRIPTOR_TYPE_BUFFER_RAW:
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Buffer (%s)", pDesc->pName);

					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Buffer (%s [%u] )", pDesc->pName, arr);

						DxDescriptorID srcId = pParam->ppBuffers[arr]->mD3D12.mDescriptors + pParam->ppBuffers[arr]->mD3D12.mSrvDescriptorOffset;

						copy_descriptor_handle(
							pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
							pRenderer->mD3D12.pCbvSrvUavHeaps[nodeIndex],
							pDescriptorSet->mD3D12.mCbvSrvUavHandle + index * pDescriptorSet->mD3D12.mCbvSrvUavStride +
							pDesc->mHandleIndex + arrayStart + arr);
					}
					break;
				}
				case DESCRIPTOR_TYPE_RW_BUFFER:
				case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL RW Buffer (%s)", pDesc->pName);

					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL RW Buffer (%s [%u] )", pDesc->pName, arr);

						DxDescriptorID srcId = pParam->ppBuffers[arr]->mD3D12.mDescriptors + pParam->ppBuffers[arr]->mD3D12.mUavDescriptorOffset;

						copy_descriptor_handle(
							pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
							pRenderer->mD3D12.pCbvSrvUavHeaps[nodeIndex],
							pDescriptorSet->mD3D12.mCbvSrvUavHandle + index * pDescriptorSet->mD3D12.mCbvSrvUavStride +
							pDesc->mHandleIndex + arrayStart + arr);
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
							VALIDATE_DESCRIPTOR(
								range.mSize <= D3D12_REQ_CONSTANT_BUFFER_SIZE, "Descriptor (%s) - pRanges[%u].mSize is %u which exceeds max size %u",
								pDesc->pName, arr, range.mSize, D3D12_REQ_CONSTANT_BUFFER_SIZE);

							D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
							cbvDesc.BufferLocation = pParam->ppBuffers[arr]->mD3D12.mDxGpuAddress + range.mOffset;
							cbvDesc.SizeInBytes = range.mSize;
							uint32_t setStart = index * pDescriptorSet->mD3D12.mCbvSrvUavStride;
							DxDescriptorID cbv = pDescriptorSet->mD3D12.mCbvSrvUavHandle + setStart + (pDesc->mHandleIndex + arrayStart + arr);
							pRenderer->mD3D12.pDxDevice->CreateConstantBufferView(
								&cbvDesc, descriptor_id_to_cpu_handle(pRenderer->mD3D12.pCbvSrvUavHeaps[nodeIndex], cbv));
						}
					}
					else
					{
						for (uint32_t arr = 0; arr < arrayCount; ++arr)
						{
							VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Uniform Buffer (%s [%u] )", pDesc->pName, arr);
							VALIDATE_DESCRIPTOR(
								pParam->ppBuffers[arr]->mSize <= D3D12_REQ_CONSTANT_BUFFER_SIZE, "Descriptor (%s) - pParam->ppBuffers[%u]->mSize is %u which exceeds max size %u", pDesc->pName,
								arr, pParam->ppBuffers[arr]->mSize, D3D12_REQ_CONSTANT_BUFFER_SIZE);

							copy_descriptor_handle(
								pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], pParam->ppBuffers[arr]->mD3D12.mDescriptors,
								pRenderer->mD3D12.pCbvSrvUavHeaps[nodeIndex],
								pDescriptorSet->mD3D12.mCbvSrvUavHandle + index * pDescriptorSet->mD3D12.mCbvSrvUavStride +
								pDesc->mHandleIndex + arrayStart + arr);
						}
					}
					break;
				}
#ifdef D3D12_RAYTRACING_AVAILABLE
				case DESCRIPTOR_TYPE_RAY_TRACING:
				{
					VALIDATE_DESCRIPTOR(pParam->ppAccelerationStructures, "NULL Acceleration Structure (%s)", pDesc->pName);

					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppAccelerationStructures[arr], "Acceleration Structure (%s [%u] )", pDesc->pName, arr);

						DxDescriptorID handle = D3D12_DESCRIPTOR_ID_NONE;
						fillRaytracingDescriptorHandle(pParam->ppAccelerationStructures[arr], &handle);

						VALIDATE_DESCRIPTOR(
							handle != D3D12_DESCRIPTOR_ID_NONE, "Invalid Acceleration Structure (%s [%u] )", pDesc->pName,
							arr);

						copy_descriptor_handle(
							pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], handle,
							pRenderer->mD3D12.pCbvSrvUavHeaps[nodeIndex],
							pDescriptorSet->mD3D12.mCbvSrvUavHandle + index * pDescriptorSet->mD3D12.mCbvSrvUavStride +
							pDesc->mHandleIndex + arrayStart + arr);
					}
					break;
				}
#endif
				default: break;
			}
		}
	}
}

bool reset_root_signature(Cmd* pCmd, PipelineType type, ID3D12RootSignature* pRootSignature)
{
	// Set root signature if the current one differs from pRootSignature
	if (pCmd->mD3D12.pBoundRootSignature != pRootSignature)
	{
		pCmd->mD3D12.pBoundRootSignature = pRootSignature;
		if (type == PIPELINE_TYPE_GRAPHICS)
			pCmd->mD3D12.pDxCmdList->SetGraphicsRootSignature(pRootSignature);
		else
			pCmd->mD3D12.pDxCmdList->SetComputeRootSignature(pRootSignature);

		for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
		{
			pCmd->mD3D12.pBoundDescriptorSets[i] = NULL;
			pCmd->mD3D12.mBoundDescriptorSetIndices[i] = -1;
		}
	}

	return false;
}

void d3d12_cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
{
	ASSERT(pCmd);
	ASSERT(pDescriptorSet);
	ASSERT(index < pDescriptorSet->mD3D12.mMaxSets);

	const DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)pDescriptorSet->mD3D12.mUpdateFrequency;

	// Set root signature if the current one differs from pRootSignature
	reset_root_signature(pCmd, (PipelineType)pDescriptorSet->mD3D12.mPipelineType, pDescriptorSet->mD3D12.pRootSignature->mD3D12.pDxRootSignature);

	if (pCmd->mD3D12.mBoundDescriptorSetIndices[pDescriptorSet->mD3D12.mUpdateFrequency] != index ||
		pCmd->mD3D12.pBoundDescriptorSets[pDescriptorSet->mD3D12.mUpdateFrequency] != pDescriptorSet)
	{
		pCmd->mD3D12.pBoundDescriptorSets[pDescriptorSet->mD3D12.mUpdateFrequency] = pDescriptorSet;
		pCmd->mD3D12.mBoundDescriptorSetIndices[pDescriptorSet->mD3D12.mUpdateFrequency] = index;

		// Bind the descriptor tables associated with this DescriptorSet
		if (pDescriptorSet->mD3D12.mPipelineType == PIPELINE_TYPE_GRAPHICS)
		{
			if (pDescriptorSet->mD3D12.mCbvSrvUavHandle != D3D12_DESCRIPTOR_ID_NONE)
			{
				pCmd->mD3D12.pDxCmdList->SetGraphicsRootDescriptorTable(
					pDescriptorSet->mD3D12.mCbvSrvUavRootIndex,
					descriptor_id_to_gpu_handle(pCmd->mD3D12.pBoundHeaps[0],
						pDescriptorSet->mD3D12.mCbvSrvUavHandle + index * pDescriptorSet->mD3D12.mCbvSrvUavStride));
			}

			if (pDescriptorSet->mD3D12.mSamplerHandle != D3D12_DESCRIPTOR_ID_NONE)
			{
				pCmd->mD3D12.pDxCmdList->SetGraphicsRootDescriptorTable(
					pDescriptorSet->mD3D12.mSamplerRootIndex,
					descriptor_id_to_gpu_handle(pCmd->mD3D12.pBoundHeaps[1],
						pDescriptorSet->mD3D12.mSamplerHandle + index * pDescriptorSet->mD3D12.mSamplerStride));
			}
		}
		else
		{
			if (pDescriptorSet->mD3D12.mCbvSrvUavHandle != D3D12_DESCRIPTOR_ID_NONE)
			{
				pCmd->mD3D12.pDxCmdList->SetComputeRootDescriptorTable(
					pDescriptorSet->mD3D12.mCbvSrvUavRootIndex,
					descriptor_id_to_gpu_handle(pCmd->mD3D12.pBoundHeaps[0],
						pDescriptorSet->mD3D12.mCbvSrvUavHandle + index * pDescriptorSet->mD3D12.mCbvSrvUavStride));
			}

			if (pDescriptorSet->mD3D12.mSamplerHandle != D3D12_DESCRIPTOR_ID_NONE)
			{
				pCmd->mD3D12.pDxCmdList->SetComputeRootDescriptorTable(
					pDescriptorSet->mD3D12.mSamplerRootIndex,
					descriptor_id_to_gpu_handle(pCmd->mD3D12.pBoundHeaps[1],
						pDescriptorSet->mD3D12.mSamplerHandle + index * pDescriptorSet->mD3D12.mSamplerStride));
			}
		}
	}
}

void d3d12_cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
{
	ASSERT(pCmd);
	ASSERT(pConstants);
	ASSERT(pRootSignature);
	ASSERT(paramIndex >= 0 && paramIndex < pRootSignature->mDescriptorCount);

	// Set root signature if the current one differs from pRootSignature
	reset_root_signature(pCmd, pRootSignature->mPipelineType, pRootSignature->mD3D12.pDxRootSignature);

	const DescriptorInfo* pDesc = pRootSignature->pDescriptors + paramIndex;
	ASSERT(pDesc);
	ASSERT(DESCRIPTOR_TYPE_ROOT_CONSTANT == pDesc->mType);

	if (pRootSignature->mPipelineType == PIPELINE_TYPE_GRAPHICS)
		pCmd->mD3D12.pDxCmdList->SetGraphicsRoot32BitConstants(pDesc->mHandleIndex, pDesc->mSize, pConstants, 0);
	else
		pCmd->mD3D12.pDxCmdList->SetComputeRoot32BitConstants(pDesc->mHandleIndex, pDesc->mSize, pConstants, 0);
}

void d3d12_cmdBindDescriptorSetWithRootCbvs(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
	ASSERT(pCmd);
	ASSERT(pDescriptorSet);
	ASSERT(pParams);

	d3d12_cmdBindDescriptorSet(pCmd, index, pDescriptorSet);

	const RootSignature* pRootSignature = pDescriptorSet->mD3D12.pRootSignature;

	for (uint32_t i = 0; i < count; ++i)
	{
		const DescriptorData* pParam = pParams + i;
		uint32_t              paramIndex = pParam->mBindByIndex ? pParam->mIndex : UINT32_MAX;

		const DescriptorInfo* pDesc =
			(paramIndex != UINT32_MAX) ? (pRootSignature->pDescriptors + paramIndex) : d3d12_get_descriptor(pRootSignature, pParam->pName);
		if (paramIndex != UINT32_MAX)
		{
			VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param index (%u)", paramIndex);
		}
		else
		{
			VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param name (%s)", pParam->pName);
		}

		VALIDATE_DESCRIPTOR(pDesc->mRootDescriptor, "Descriptor (%s) - must be a root cbv", pDesc->pName);
		VALIDATE_DESCRIPTOR(pParam->mCount <= 1, "Descriptor (%s) - cmdBindDescriptorSetWithRootCbvs does not support arrays", pDesc->pName);
		VALIDATE_DESCRIPTOR(pParam->pRanges, "Descriptor (%s) - pRanges must be provided for cmdBindDescriptorSetWithRootCbvs", pDesc->pName);

		DescriptorDataRange range = pParam->pRanges[0];
		D3D12_GPU_VIRTUAL_ADDRESS address = pParam->ppBuffers[0]->mD3D12.mDxGpuAddress + range.mOffset;

		VALIDATE_DESCRIPTOR(range.mSize > 0, "Descriptor (%s) - pRanges->mSize is zero", pDesc->pName);
		VALIDATE_DESCRIPTOR(range.mSize <= D3D12_REQ_CONSTANT_BUFFER_SIZE, "Descriptor (%s) - pRanges->mSize is %u which exceeds max %u",
			pDesc->pName, range.mSize, D3D12_REQ_CONSTANT_BUFFER_SIZE);

		if (pRootSignature->mPipelineType == PIPELINE_TYPE_GRAPHICS)
		{
			pCmd->mD3D12.pDxCmdList->SetGraphicsRootConstantBufferView(pDesc->mHandleIndex, address);    //-V522
		}
		else
		{
			pCmd->mD3D12.pDxCmdList->SetComputeRootConstantBufferView(pDesc->mHandleIndex, address);    //-V522
		}
	}
}
/************************************************************************/
// Pipeline State Functions
/************************************************************************/
void addGraphicsPipeline(Renderer* pRenderer, const PipelineDesc* pMainDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(ppPipeline);
	ASSERT(pMainDesc);

	const GraphicsPipelineDesc* pDesc = &pMainDesc->mGraphicsDesc;

	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);

	//allocate new pipeline
	Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
	ASSERT(pPipeline);

	const Shader*       pShaderProgram = pDesc->pShaderProgram;
	const VertexLayout* pVertexLayout = pDesc->pVertexLayout;

#ifndef DISABLE_PIPELINE_LIBRARY
	ID3D12PipelineLibrary* psoCache = pMainDesc->pCache ? pMainDesc->pCache->mD3D12.pLibrary : NULL;
#endif

	size_t psoShaderHash = 0;
	size_t psoRenderHash = 0;

	pPipeline->mD3D12.mType = PIPELINE_TYPE_GRAPHICS;
	pPipeline->mD3D12.pRootSignature = pDesc->pRootSignature->mD3D12.pDxRootSignature;

	//add to gpu
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, VS);
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, PS);
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, DS);
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, HS);
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, GS);
	if (pShaderProgram->mStages & SHADER_STAGE_VERT)
	{
		VS.BytecodeLength = pShaderProgram->mD3D12.pShaderBlobs[pShaderProgram->pReflection->mVertexStageIndex]->GetBufferSize();
		VS.pShaderBytecode = pShaderProgram->mD3D12.pShaderBlobs[pShaderProgram->pReflection->mVertexStageIndex]->GetBufferPointer();
	}
	if (pShaderProgram->mStages & SHADER_STAGE_FRAG)
	{
		PS.BytecodeLength = pShaderProgram->mD3D12.pShaderBlobs[pShaderProgram->pReflection->mPixelStageIndex]->GetBufferSize();
		PS.pShaderBytecode = pShaderProgram->mD3D12.pShaderBlobs[pShaderProgram->pReflection->mPixelStageIndex]->GetBufferPointer();
	}
	if (pShaderProgram->mStages & SHADER_STAGE_HULL)
	{
		HS.BytecodeLength = pShaderProgram->mD3D12.pShaderBlobs[pShaderProgram->pReflection->mHullStageIndex]->GetBufferSize();
		HS.pShaderBytecode = pShaderProgram->mD3D12.pShaderBlobs[pShaderProgram->pReflection->mHullStageIndex]->GetBufferPointer();
	}
	if (pShaderProgram->mStages & SHADER_STAGE_DOMN)
	{
		DS.BytecodeLength = pShaderProgram->mD3D12.pShaderBlobs[pShaderProgram->pReflection->mDomainStageIndex]->GetBufferSize();
		DS.pShaderBytecode = pShaderProgram->mD3D12.pShaderBlobs[pShaderProgram->pReflection->mDomainStageIndex]->GetBufferPointer();
	}
	if (pShaderProgram->mStages & SHADER_STAGE_GEOM)
	{
		GS.BytecodeLength = pShaderProgram->mD3D12.pShaderBlobs[pShaderProgram->pReflection->mGeometryStageIndex]->GetBufferSize();
		GS.pShaderBytecode = pShaderProgram->mD3D12.pShaderBlobs[pShaderProgram->pReflection->mGeometryStageIndex]->GetBufferPointer();
	}

	DECLARE_ZERO(D3D12_STREAM_OUTPUT_DESC, stream_output_desc);
	stream_output_desc.pSODeclaration = NULL;
	stream_output_desc.NumEntries = 0;
	stream_output_desc.pBufferStrides = NULL;
	stream_output_desc.NumStrides = 0;
	stream_output_desc.RasterizedStream = 0;

	DECLARE_ZERO(D3D12_DEPTH_STENCILOP_DESC, depth_stencilop_desc);
	depth_stencilop_desc.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depth_stencilop_desc.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depth_stencilop_desc.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depth_stencilop_desc.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	uint32_t input_elementCount = 0;
	DECLARE_ZERO(D3D12_INPUT_ELEMENT_DESC, input_elements[MAX_VERTEX_ATTRIBS]);

	DECLARE_ZERO(char, semantic_names[MAX_VERTEX_ATTRIBS][MAX_SEMANTIC_NAME_LENGTH]);
	// Make sure there's attributes
	if (pVertexLayout != NULL)
	{
		//uint32_t attrib_count = min(pVertexLayout->mAttribCount, MAX_VERTEX_ATTRIBS);  //Not used
		for (uint32_t attrib_index = 0; attrib_index < pVertexLayout->mAttribCount; ++attrib_index)
		{
			const VertexAttrib* attrib = &(pVertexLayout->mAttribs[attrib_index]);

			ASSERT(SEMANTIC_UNDEFINED != attrib->mSemantic);

			if (attrib->mSemanticNameLength > 0)
			{
				uint32_t name_length = min((uint32_t)MAX_SEMANTIC_NAME_LENGTH, attrib->mSemanticNameLength);
				strncpy_s(semantic_names[attrib_index], attrib->mSemanticName, name_length);
			}
			else
			{
				switch (attrib->mSemantic)
				{
					case SEMANTIC_POSITION: strcpy_s(semantic_names[attrib_index], "POSITION"); break;
					case SEMANTIC_NORMAL: strcpy_s(semantic_names[attrib_index], "NORMAL"); break;
					case SEMANTIC_COLOR: strcpy_s(semantic_names[attrib_index], "COLOR"); break;
					case SEMANTIC_TANGENT: strcpy_s(semantic_names[attrib_index], "TANGENT"); break;
					case SEMANTIC_BITANGENT: strcpy_s(semantic_names[attrib_index], "BITANGENT"); break;
					case SEMANTIC_JOINTS: strcpy_s(semantic_names[attrib_index], "JOINTS"); break;
					case SEMANTIC_WEIGHTS: strcpy_s(semantic_names[attrib_index], "WEIGHTS"); break;
					case SEMANTIC_TEXCOORD0:
					case SEMANTIC_TEXCOORD1:
					case SEMANTIC_TEXCOORD2:
					case SEMANTIC_TEXCOORD3:
					case SEMANTIC_TEXCOORD4:
					case SEMANTIC_TEXCOORD5:
					case SEMANTIC_TEXCOORD6:
					case SEMANTIC_TEXCOORD7:
					case SEMANTIC_TEXCOORD8:
					case SEMANTIC_TEXCOORD9: strcpy_s(semantic_names[attrib_index], "TEXCOORD"); break;
					case SEMANTIC_SHADING_RATE: strcpy_s(semantic_names[attrib_index], "SV_ShadingRate"); break;
					default: ASSERT(false); break;
				}
			}

			UINT semantic_index = 0;
			switch (attrib->mSemantic)
			{
				case SEMANTIC_TEXCOORD0: semantic_index = 0; break;
				case SEMANTIC_TEXCOORD1: semantic_index = 1; break;
				case SEMANTIC_TEXCOORD2: semantic_index = 2; break;
				case SEMANTIC_TEXCOORD3: semantic_index = 3; break;
				case SEMANTIC_TEXCOORD4: semantic_index = 4; break;
				case SEMANTIC_TEXCOORD5: semantic_index = 5; break;
				case SEMANTIC_TEXCOORD6: semantic_index = 6; break;
				case SEMANTIC_TEXCOORD7: semantic_index = 7; break;
				case SEMANTIC_TEXCOORD8: semantic_index = 8; break;
				case SEMANTIC_TEXCOORD9: semantic_index = 9; break;
				default: break;
			}

			input_elements[input_elementCount].SemanticName = semantic_names[attrib_index];
			input_elements[input_elementCount].SemanticIndex = semantic_index;

			input_elements[input_elementCount].Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(attrib->mFormat);
			input_elements[input_elementCount].InputSlot = attrib->mBinding;
			input_elements[input_elementCount].AlignedByteOffset = attrib->mOffset;
			if (attrib->mRate == VERTEX_ATTRIB_RATE_INSTANCE)
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
				psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&attrib->mSemantic, sizeof(ShaderSemantic), psoRenderHash);
				psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&attrib->mFormat, sizeof(TinyImageFormat), psoRenderHash);
				psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&attrib->mBinding, sizeof(uint32_t), psoRenderHash);
				psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&attrib->mLocation, sizeof(uint32_t), psoRenderHash);
				psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&attrib->mOffset, sizeof(uint32_t), psoRenderHash);
				psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&attrib->mRate, sizeof(VertexAttribRate), psoRenderHash);
			}
#endif

			++input_elementCount;
		}
	}

	DECLARE_ZERO(D3D12_INPUT_LAYOUT_DESC, input_layout_desc);
	input_layout_desc.pInputElementDescs = input_elementCount ? input_elements : NULL;
	input_layout_desc.NumElements = input_elementCount;

	uint32_t render_target_count = min(pDesc->mRenderTargetCount, (uint32_t)MAX_RENDER_TARGET_ATTACHMENTS);
	render_target_count = min(render_target_count, (uint32_t)D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

	DECLARE_ZERO(DXGI_SAMPLE_DESC, sample_desc);
	sample_desc.Count = (UINT)(pDesc->mSampleCount);
	sample_desc.Quality = (UINT)(pDesc->mSampleQuality);

	DECLARE_ZERO(D3D12_CACHED_PIPELINE_STATE, cached_pso_desc);
	cached_pso_desc.pCachedBlob = NULL;
	cached_pso_desc.CachedBlobSizeInBytes = 0;

	DECLARE_ZERO(D3D12_GRAPHICS_PIPELINE_STATE_DESC, pipeline_state_desc);
	pipeline_state_desc.pRootSignature = pDesc->pRootSignature->mD3D12.pDxRootSignature;
	pipeline_state_desc.VS = VS;
	pipeline_state_desc.PS = PS;
	pipeline_state_desc.DS = DS;
	pipeline_state_desc.HS = HS;
	pipeline_state_desc.GS = GS;
	pipeline_state_desc.StreamOutput = stream_output_desc;
	pipeline_state_desc.BlendState = pDesc->pBlendState ? util_to_blend_desc(pDesc->pBlendState) : gDefaultBlendDesc;
	pipeline_state_desc.SampleMask = UINT_MAX;
	pipeline_state_desc.RasterizerState =
		pDesc->pRasterizerState ? util_to_rasterizer_desc(pDesc->pRasterizerState) : gDefaultRasterizerDesc;
	pipeline_state_desc.DepthStencilState = pDesc->pDepthState ? util_to_depth_desc(pDesc->pDepthState) : gDefaultDepthDesc;

	pipeline_state_desc.InputLayout = input_layout_desc;
	pipeline_state_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	pipeline_state_desc.PrimitiveTopologyType = util_to_dx12_primitive_topology_type(pDesc->mPrimitiveTopo);
	pipeline_state_desc.NumRenderTargets = render_target_count;
	pipeline_state_desc.DSVFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mDepthStencilFormat);

	pipeline_state_desc.SampleDesc = sample_desc;
	pipeline_state_desc.CachedPSO = cached_pso_desc;
	pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	for (uint32_t attrib_index = 0; attrib_index < render_target_count; ++attrib_index)
	{
		pipeline_state_desc.RTVFormats[attrib_index] = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->pColorFormats[attrib_index]);
	}

	// If running Linked Mode (SLI) create pipeline for all nodes
	// #NOTE : In non SLI mode, mNodeCount will be 0 which sets nodeMask to default value
	pipeline_state_desc.NodeMask = util_calculate_shared_node_mask(pRenderer);

	HRESULT result = E_FAIL;
	wchar_t pipelineName[MAX_DEBUG_NAME_LENGTH + 32] = {};

#ifndef DISABLE_PIPELINE_LIBRARY
	if (psoCache)
	{
		psoShaderHash = tf_mem_hash<uint8_t>((uint8_t*)VS.pShaderBytecode, VS.BytecodeLength, psoShaderHash);
		psoShaderHash = tf_mem_hash<uint8_t>((uint8_t*)PS.pShaderBytecode, PS.BytecodeLength, psoShaderHash);
		psoShaderHash = tf_mem_hash<uint8_t>((uint8_t*)HS.pShaderBytecode, HS.BytecodeLength, psoShaderHash);
		psoShaderHash = tf_mem_hash<uint8_t>((uint8_t*)DS.pShaderBytecode, DS.BytecodeLength, psoShaderHash);
		psoShaderHash = tf_mem_hash<uint8_t>((uint8_t*)GS.pShaderBytecode, GS.BytecodeLength, psoShaderHash);

		psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.BlendState, sizeof(D3D12_BLEND_DESC), psoRenderHash);
		psoRenderHash =
			tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.DepthStencilState, sizeof(D3D12_DEPTH_STENCIL_DESC), psoRenderHash);
		psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.RasterizerState, sizeof(D3D12_RASTERIZER_DESC), psoRenderHash);

		psoRenderHash =
			tf_mem_hash<uint8_t>((uint8_t*)pipeline_state_desc.RTVFormats, render_target_count * sizeof(DXGI_FORMAT), psoRenderHash);
		psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.DSVFormat, sizeof(DXGI_FORMAT), psoRenderHash);
		psoRenderHash = tf_mem_hash<uint8_t>(
			(uint8_t*)&pipeline_state_desc.PrimitiveTopologyType, sizeof(D3D12_PRIMITIVE_TOPOLOGY_TYPE), psoRenderHash);
		psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.SampleDesc, sizeof(DXGI_SAMPLE_DESC), psoRenderHash);
		psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.NodeMask, sizeof(UINT), psoRenderHash);

		swprintf(pipelineName, L"%S_S%zuR%zu", (pMainDesc->pName ? pMainDesc->pName : "GRAPHICSPSO"), psoShaderHash, psoRenderHash);
		result = psoCache->LoadGraphicsPipeline(pipelineName, &pipeline_state_desc, IID_ARGS(&pPipeline->mD3D12.pDxPipelineState));
	}
#endif

	if (!SUCCEEDED(result))
	{
		CHECK_HRESULT(hook_create_graphics_pipeline_state(
			pRenderer->mD3D12.pDxDevice, &pipeline_state_desc, pMainDesc->pPipelineExtensions, pMainDesc->mExtensionCount,
			&pPipeline->mD3D12.pDxPipelineState));

#ifndef DISABLE_PIPELINE_LIBRARY
		if (psoCache)
		{
			CHECK_HRESULT(psoCache->StorePipeline(pipelineName, pPipeline->mD3D12.pDxPipelineState));
		}
#endif
	}

	D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	switch (pDesc->mPrimitiveTopo)
	{
		case PRIMITIVE_TOPO_POINT_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
		case PRIMITIVE_TOPO_LINE_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
		case PRIMITIVE_TOPO_LINE_STRIP: topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
		case PRIMITIVE_TOPO_TRI_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
		case PRIMITIVE_TOPO_TRI_STRIP: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
		case PRIMITIVE_TOPO_PATCH_LIST:
		{
			const PipelineReflection* pReflection = pDesc->pShaderProgram->pReflection;
			uint32_t                  controlPoint = pReflection->mStageReflections[pReflection->mHullStageIndex].mNumControlPoint;
			topology = (D3D_PRIMITIVE_TOPOLOGY)(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (controlPoint - 1));
		}
		break;

		default: break;
	}

	ASSERT(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED != topology);
	pPipeline->mD3D12.mDxPrimitiveTopology = topology;

	*ppPipeline = pPipeline;
}

void addComputePipeline(Renderer* pRenderer, const PipelineDesc* pMainDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(ppPipeline);
	ASSERT(pMainDesc);

	const ComputePipelineDesc* pDesc = &pMainDesc->mComputeDesc;

	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);
	ASSERT(pDesc->pShaderProgram->mD3D12.pShaderBlobs[0]);

	//allocate new pipeline
	Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
	ASSERT(pPipeline);

	pPipeline->mD3D12.mType = PIPELINE_TYPE_COMPUTE;
	pPipeline->mD3D12.pRootSignature = pDesc->pRootSignature->mD3D12.pDxRootSignature;

	//add pipeline specifying its for compute purposes
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, CS);
	CS.BytecodeLength = pDesc->pShaderProgram->mD3D12.pShaderBlobs[0]->GetBufferSize();
	CS.pShaderBytecode = pDesc->pShaderProgram->mD3D12.pShaderBlobs[0]->GetBufferPointer();

	DECLARE_ZERO(D3D12_CACHED_PIPELINE_STATE, cached_pso_desc);
	cached_pso_desc.pCachedBlob = NULL;
	cached_pso_desc.CachedBlobSizeInBytes = 0;

	DECLARE_ZERO(D3D12_COMPUTE_PIPELINE_STATE_DESC, pipeline_state_desc);
	pipeline_state_desc.pRootSignature = pDesc->pRootSignature->mD3D12.pDxRootSignature;
	pipeline_state_desc.CS = CS;
	pipeline_state_desc.CachedPSO = cached_pso_desc;
#if !defined(XBOX)
	pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
#endif

	// If running Linked Mode (SLI) create pipeline for all nodes
	// #NOTE : In non SLI mode, mNodeCount will be 0 which sets nodeMask to default value
	pipeline_state_desc.NodeMask = util_calculate_shared_node_mask(pRenderer);

	HRESULT result = E_FAIL;
#ifndef DISABLE_PIPELINE_LIBRARY
	ID3D12PipelineLibrary* psoCache = pMainDesc->pCache ? pMainDesc->pCache->mD3D12.pLibrary : NULL;
	wchar_t                pipelineName[MAX_DEBUG_NAME_LENGTH + 32] = {};

	if (psoCache)
	{
		size_t psoShaderHash = 0;
		psoShaderHash = tf_mem_hash<uint8_t>((uint8_t*)CS.pShaderBytecode, CS.BytecodeLength, psoShaderHash);

		swprintf(pipelineName, L"%S_S%zu", (pMainDesc->pName ? pMainDesc->pName : "COMPUTEPSO"), psoShaderHash);
		result = psoCache->LoadComputePipeline(pipelineName, &pipeline_state_desc, IID_ARGS(&pPipeline->mD3D12.pDxPipelineState));
	}
#endif

	if (!SUCCEEDED(result))
	{
		CHECK_HRESULT(hook_create_compute_pipeline_state(
			pRenderer->mD3D12.pDxDevice, &pipeline_state_desc, pMainDesc->pPipelineExtensions, pMainDesc->mExtensionCount,
			&pPipeline->mD3D12.pDxPipelineState));

#ifndef DISABLE_PIPELINE_LIBRARY
		if (psoCache)
		{
			CHECK_HRESULT(psoCache->StorePipeline(pipelineName, pPipeline->mD3D12.pDxPipelineState));
		}
#endif
	}

	*ppPipeline = pPipeline;
}

void d3d12_addPipeline(Renderer* pRenderer, const PipelineDesc* pDesc, Pipeline** ppPipeline)
{
	switch (pDesc->mType)
	{
		case (PIPELINE_TYPE_COMPUTE):
		{
			addComputePipeline(pRenderer, pDesc, ppPipeline);
			break;
		}
		case (PIPELINE_TYPE_GRAPHICS):
		{
			addGraphicsPipeline(pRenderer, pDesc, ppPipeline);
			break;
		}
#ifdef D3D12_RAYTRACING_AVAILABLE
		case (PIPELINE_TYPE_RAYTRACING):
		{
			addRaytracingPipeline(&pDesc->mRaytracingDesc, ppPipeline);
			break;
		}
#endif
		default:
		{
			ASSERT(false);
			*ppPipeline = {};
			break;
		}
	}

	addPipelineDependencies(*ppPipeline, pDesc);
}

void d3d12_removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
	removePipelineDependencies(pPipeline);

	ASSERT(pRenderer);
	ASSERT(pPipeline);

	//delete pipeline from device
	SAFE_RELEASE(pPipeline->mD3D12.pDxPipelineState);
#ifdef D3D12_RAYTRACING_AVAILABLE
	SAFE_RELEASE(pPipeline->mD3D12.pDxrPipeline);
#endif

	SAFE_FREE(pPipeline);
}

void d3d12_addPipelineCache(Renderer* pRenderer, const PipelineCacheDesc* pDesc, PipelineCache** ppPipelineCache)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppPipelineCache);

	PipelineCache* pPipelineCache = (PipelineCache*)tf_calloc(1, sizeof(PipelineCache));
	ASSERT(pPipelineCache);

	if (pDesc->mSize)
	{
		// D3D12 does not copy pipeline cache data. We have to keep it around until the cache is alive
		pPipelineCache->mD3D12.pData = tf_malloc(pDesc->mSize);
		memcpy(pPipelineCache->mD3D12.pData, pDesc->pData, pDesc->mSize);
	}

#ifndef DISABLE_PIPELINE_LIBRARY
	D3D12_FEATURE_DATA_SHADER_CACHE feature = {};
	HRESULT result = pRenderer->mD3D12.pDxDevice->CheckFeatureSupport(D3D12_FEATURE_SHADER_CACHE, &feature, sizeof(feature));
	if (SUCCEEDED(result))
	{
		result = E_NOTIMPL;
		if (feature.SupportFlags & D3D12_SHADER_CACHE_SUPPORT_LIBRARY)
		{
			ID3D12Device1* device1 = NULL;
			result = pRenderer->mD3D12.pDxDevice->QueryInterface(IID_ARGS(&device1));
			if (SUCCEEDED(result))
			{
				result =
					device1->CreatePipelineLibrary(pPipelineCache->mD3D12.pData, pDesc->mSize, IID_ARGS(&pPipelineCache->mD3D12.pLibrary));
			}
			SAFE_RELEASE(device1);
		}
	}

	if (!SUCCEEDED(result))
	{
		LOGF(eWARNING, "Pipeline Cache Library feature is not present. Pipeline Cache will be disabled");
	}
#endif

	*ppPipelineCache = pPipelineCache;
}

void d3d12_removePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache)
{
	ASSERT(pRenderer);
	ASSERT(pPipelineCache);

#ifndef DISABLE_PIPELINE_LIBRARY
	SAFE_RELEASE(pPipelineCache->mD3D12.pLibrary);
#endif
	SAFE_FREE(pPipelineCache->mD3D12.pData);
	SAFE_FREE(pPipelineCache);
}

void d3d12_getPipelineCacheData(Renderer* pRenderer, PipelineCache* pPipelineCache, size_t* pSize, void* pData)
{
	ASSERT(pRenderer);
	ASSERT(pPipelineCache);
	ASSERT(pSize);

#ifndef DISABLE_PIPELINE_LIBRARY
	*pSize = 0;

	if (pPipelineCache->mD3D12.pLibrary)
	{
		*pSize = pPipelineCache->mD3D12.pLibrary->GetSerializedSize();
		if (pData)
		{
			CHECK_HRESULT(pPipelineCache->mD3D12.pLibrary->Serialize(pData, *pSize));
		}
	}
#endif
}
/************************************************************************/
// Command buffer Functions
/************************************************************************/
void d3d12_resetCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	ASSERT(pRenderer);
	ASSERT(pCmdPool);

	CHECK_HRESULT(pCmdPool->pDxCmdAlloc->Reset());
}

void d3d12_beginCmd(Cmd* pCmd)
{
	ASSERT(pCmd);
	ASSERT(pCmd->mD3D12.pDxCmdList);

	CHECK_HRESULT(pCmd->mD3D12.pDxCmdList->Reset(pCmd->mD3D12.pCmdPool->pDxCmdAlloc, NULL));

	if (pCmd->mD3D12.mType != QUEUE_TYPE_TRANSFER)
	{
		ID3D12DescriptorHeap* heaps[] =
		{
			pCmd->mD3D12.pBoundHeaps[0]->pHeap,
			pCmd->mD3D12.pBoundHeaps[1]->pHeap,
		};
		pCmd->mD3D12.pDxCmdList->SetDescriptorHeaps(2, heaps);

		pCmd->mD3D12.mBoundHeapStartHandles[0] = pCmd->mD3D12.pBoundHeaps[0]->pHeap->GetGPUDescriptorHandleForHeapStart();
		pCmd->mD3D12.mBoundHeapStartHandles[1] = pCmd->mD3D12.pBoundHeaps[1]->pHeap->GetGPUDescriptorHandleForHeapStart();
	}

	// Reset CPU side data
	pCmd->mD3D12.pBoundRootSignature = NULL;
	for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
	{
		pCmd->mD3D12.pBoundDescriptorSets[i] = NULL;
		pCmd->mD3D12.mBoundDescriptorSetIndices[i] = -1;
	}
}

void d3d12_endCmd(Cmd* pCmd)
{
	ASSERT(pCmd);
	ASSERT(pCmd->mD3D12.pDxCmdList);

	CHECK_HRESULT(pCmd->mD3D12.pDxCmdList->Close());
}

void d3d12_cmdBindRenderTargets(
	Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** pRenderTargets, RenderTarget* pDepthStencil,
	const LoadActionsDesc* pLoadActions /* = NULL*/, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice,
	uint32_t depthMipSlice)
{
	ASSERT(pCmd);
	ASSERT(pCmd->mD3D12.pDxCmdList);

	if (!renderTargetCount && !pDepthStencil)
		return;

	D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
	D3D12_CPU_DESCRIPTOR_HANDLE rtvs[MAX_RENDER_TARGET_ATTACHMENTS] = {};

	for (uint32_t i = 0; i < renderTargetCount; ++i)
	{
		if (!pColorMipSlices && !pColorArraySlices)
		{
			rtvs[i] = descriptor_id_to_cpu_handle(
				pCmd->pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV],
				pRenderTargets[i]->mD3D12.mDescriptors);
		}
		else
		{
			uint32_t handle = 0;
			if (pColorMipSlices)
			{
				if (pColorArraySlices)
					handle = 1 + pColorMipSlices[i] * (uint32_t)pRenderTargets[i]->mArraySize + pColorArraySlices[i];
				else
					handle = 1 + pColorMipSlices[i];
			}
			else if (pColorArraySlices)
			{
				handle = 1 + pColorArraySlices[i];
			}

			rtvs[i] = descriptor_id_to_cpu_handle(
				pCmd->pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV],
				pRenderTargets[i]->mD3D12.mDescriptors + handle);
		}
	}

	if (pDepthStencil)
	{
		if (-1 == depthMipSlice && -1 == depthArraySlice)
		{
			dsv = descriptor_id_to_cpu_handle(
				pCmd->pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV],
				pDepthStencil->mD3D12.mDescriptors);
		}
		else
		{
			uint32_t handle = 0;
			if (depthMipSlice != -1)
			{
				if (depthArraySlice != -1)
					handle = 1 + depthMipSlice * (uint32_t)pDepthStencil->mArraySize + depthArraySlice;
				else
					handle = 1 + depthMipSlice;
			}
			else if (depthArraySlice != -1)
			{
				handle = 1 + depthArraySlice;
			}

			dsv = descriptor_id_to_cpu_handle(
				pCmd->pRenderer->mD3D12.pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV],
				pDepthStencil->mD3D12.mDescriptors + handle);
		}
	}

	pCmd->mD3D12.pDxCmdList->OMSetRenderTargets(
		renderTargetCount, rtvs, FALSE, dsv.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL ? &dsv : NULL);

	// process clear actions (clear color/depth)
	if (pLoadActions)
	{
		for (uint32_t i = 0; i < renderTargetCount; ++i)
		{
			if (pLoadActions->mLoadActionsColor[i] == LOAD_ACTION_CLEAR)
			{
				float color_rgba[4] =
				{
					pLoadActions->mClearColorValues[i].r,
					pLoadActions->mClearColorValues[i].g,
					pLoadActions->mClearColorValues[i].b,
					pLoadActions->mClearColorValues[i].a,
				};

				pCmd->mD3D12.pDxCmdList->ClearRenderTargetView(rtvs[i], color_rgba, 0, NULL);
			}
		}
		if (pLoadActions->mLoadActionDepth == LOAD_ACTION_CLEAR || pLoadActions->mLoadActionStencil == LOAD_ACTION_CLEAR)
		{
			ASSERT(dsv.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL);

			D3D12_CLEAR_FLAGS flags = (D3D12_CLEAR_FLAGS)0;
			if (pLoadActions->mLoadActionDepth == LOAD_ACTION_CLEAR)
				flags |= D3D12_CLEAR_FLAG_DEPTH;
			if (pLoadActions->mLoadActionStencil == LOAD_ACTION_CLEAR)
				flags |= D3D12_CLEAR_FLAG_STENCIL;
			ASSERT(flags > 0);
			pCmd->mD3D12.pDxCmdList->ClearDepthStencilView(
				dsv, flags, pLoadActions->mClearDepth.depth, (UINT8)pLoadActions->mClearDepth.stencil, 0, NULL);
		}
	}
}

void d3d12_cmdSetShadingRate(
	Cmd* pCmd, ShadingRate shadingRate, Texture* pTexture, ShadingRateCombiner postRasterizerRate, ShadingRateCombiner finalRate)
{
#ifdef ENABLE_VRS
	ASSERT(pCmd->pRenderer->pActiveGpuSettings->mShadingRateCaps);
	ASSERT(pCmd);
	ASSERT(pCmd->mD3D12.pDxCmdList);

	if (pCmd->pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_DRAW)
	{
		D3D12_SHADING_RATE_COMBINER combiners[2] = { util_to_dx_shading_rate_combiner(postRasterizerRate),
													 util_to_dx_shading_rate_combiner(finalRate) };
		((ID3D12GraphicsCommandList5*)pCmd->mD3D12.pDxCmdList)->RSSetShadingRate(util_to_dx_shading_rate(shadingRate), combiners);
	}

	if (pCmd->pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE)
	{
		ID3D12Resource* resource = pTexture ? pTexture->mD3D12.pDxResource : NULL;
		((ID3D12GraphicsCommandList5*)pCmd->mD3D12.pDxCmdList)->RSSetShadingRateImage(resource);
	}
#endif
}

void d3d12_cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
	ASSERT(pCmd);

	//set new viewport
	ASSERT(pCmd->mD3D12.pDxCmdList);

	D3D12_VIEWPORT viewport;
	viewport.TopLeftX = x;
	viewport.TopLeftY = y;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinDepth = minDepth;
	viewport.MaxDepth = maxDepth;

	pCmd->mD3D12.pDxCmdList->RSSetViewports(1, &viewport);
}

void d3d12_cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	ASSERT(pCmd);

	//set new scissor values
	ASSERT(pCmd->mD3D12.pDxCmdList);

	D3D12_RECT scissor;
	scissor.left = x;
	scissor.top = y;
	scissor.right = x + width;
	scissor.bottom = y + height;

	pCmd->mD3D12.pDxCmdList->RSSetScissorRects(1, &scissor);
}

void d3d12_cmdSetStencilReferenceValue(Cmd* pCmd, uint32_t val)
{
	ASSERT(pCmd);
	ASSERT(pCmd->mD3D12.pDxCmdList);

	pCmd->mD3D12.pDxCmdList->OMSetStencilRef(val);
}

void d3d12_cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
	ASSERT(pCmd);
	ASSERT(pPipeline);

	//bind given pipeline
	ASSERT(pCmd->mD3D12.pDxCmdList);

	if (pPipeline->mD3D12.mType == PIPELINE_TYPE_GRAPHICS)
	{
		ASSERT(pPipeline->mD3D12.pDxPipelineState);
		reset_root_signature(pCmd, pPipeline->mD3D12.mType, pPipeline->mD3D12.pRootSignature);
		pCmd->mD3D12.pDxCmdList->IASetPrimitiveTopology(pPipeline->mD3D12.mDxPrimitiveTopology);
		pCmd->mD3D12.pDxCmdList->SetPipelineState(pPipeline->mD3D12.pDxPipelineState);
	}
#ifdef D3D12_RAYTRACING_AVAILABLE
	else if (pPipeline->mD3D12.mType == PIPELINE_TYPE_RAYTRACING)
	{
		reset_root_signature(pCmd, pPipeline->mD3D12.mType, pPipeline->mD3D12.pRootSignature);
		cmdBindRaytracingPipeline(pCmd, pPipeline);
	}
#endif
	else
	{
		ASSERT(pPipeline->mD3D12.pDxPipelineState);
		reset_root_signature(pCmd, pPipeline->mD3D12.mType, pPipeline->mD3D12.pRootSignature);
		pCmd->mD3D12.pDxCmdList->SetPipelineState(pPipeline->mD3D12.pDxPipelineState);
	}
}

void d3d12_cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
	ASSERT(pCmd);
	ASSERT(pBuffer);
	ASSERT(pCmd->mD3D12.pDxCmdList);
	ASSERT(D3D12_GPU_VIRTUAL_ADDRESS_NULL != pBuffer->mD3D12.mDxGpuAddress);

	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = pBuffer->mD3D12.mDxGpuAddress + offset;
	ibView.Format = (INDEX_TYPE_UINT16 == indexType) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	ibView.SizeInBytes = (UINT)(pBuffer->mSize - offset);

	//bind given index buffer
	pCmd->mD3D12.pDxCmdList->IASetIndexBuffer(&ibView);
}

void d3d12_cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
{
	ASSERT(pCmd);
	ASSERT(0 != bufferCount);
	ASSERT(ppBuffers);
	ASSERT(pCmd->mD3D12.pDxCmdList);
	//bind given vertex buffer

	DECLARE_ZERO(D3D12_VERTEX_BUFFER_VIEW, views[MAX_VERTEX_ATTRIBS]);
	for (uint32_t i = 0; i < bufferCount; ++i)
	{
		ASSERT(D3D12_GPU_VIRTUAL_ADDRESS_NULL != ppBuffers[i]->mD3D12.mDxGpuAddress);

		views[i].BufferLocation = (ppBuffers[i]->mD3D12.mDxGpuAddress + (pOffsets ? pOffsets[i] : 0));
		views[i].SizeInBytes = (UINT)(ppBuffers[i]->mSize - (pOffsets ? pOffsets[i] : 0));
		views[i].StrideInBytes = (UINT)pStrides[i];
	}

	pCmd->mD3D12.pDxCmdList->IASetVertexBuffers(0, bufferCount, views);
}

void d3d12_cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
	ASSERT(pCmd);

	//draw given vertices
	ASSERT(pCmd->mD3D12.pDxCmdList);

	pCmd->mD3D12.pDxCmdList->DrawInstanced((UINT)vertexCount, (UINT)1, (UINT)firstVertex, (UINT)0);
}

void d3d12_cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
	ASSERT(pCmd);

	//draw given vertices
	ASSERT(pCmd->mD3D12.pDxCmdList);

	pCmd->mD3D12.pDxCmdList->DrawInstanced((UINT)vertexCount, (UINT)instanceCount, (UINT)firstVertex, (UINT)firstInstance);
}

void d3d12_cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
	ASSERT(pCmd);

	//draw indexed mesh
	ASSERT(pCmd->mD3D12.pDxCmdList);

	pCmd->mD3D12.pDxCmdList->DrawIndexedInstanced((UINT)indexCount, (UINT)1, (UINT)firstIndex, (UINT)firstVertex, (UINT)0);
}

void d3d12_cmdDrawIndexedInstanced(
	Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstInstance, uint32_t firstVertex)
{
	ASSERT(pCmd);

	//draw indexed mesh
	ASSERT(pCmd->mD3D12.pDxCmdList);

	pCmd->mD3D12.pDxCmdList->DrawIndexedInstanced(
		(UINT)indexCount, (UINT)instanceCount, (UINT)firstIndex, (UINT)firstVertex, (UINT)firstInstance);
}

void d3d12_cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	ASSERT(pCmd);

	//dispatch given command
	ASSERT(pCmd->mD3D12.pDxCmdList != NULL);

	hook_dispatch(pCmd, groupCountX, groupCountY, groupCountZ);
}

void d3d12_cmdResourceBarrier(
	Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers,
	uint32_t numRtBarriers, RenderTargetBarrier* pRtBarriers)
{
	D3D12_RESOURCE_BARRIER* barriers =
		(D3D12_RESOURCE_BARRIER*)alloca((numBufferBarriers + numTextureBarriers + numRtBarriers) * sizeof(D3D12_RESOURCE_BARRIER));
	uint32_t transitionCount = 0;

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
			//if (!(pBuffer->mCurrentState & pTransBarrier->mNewState) && pBuffer->mCurrentState != pTransBarrier->mNewState)
			if (RESOURCE_STATE_UNORDERED_ACCESS == pTransBarrier->mCurrentState &&
				RESOURCE_STATE_UNORDERED_ACCESS == pTransBarrier->mNewState)
			{
				pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				pBarrier->UAV.pResource = pBuffer->mD3D12.pDxResource;
				++transitionCount;
			}
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
				pBarrier->Transition.pResource = pBuffer->mD3D12.pDxResource;
				pBarrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				pBarrier->Transition.StateBefore = util_to_dx12_resource_state(pTransBarrier->mCurrentState);
				pBarrier->Transition.StateAfter = util_to_dx12_resource_state(pTransBarrier->mNewState);

				++transitionCount;
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
			pBarrier->UAV.pResource = pTexture->mD3D12.pDxResource;
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
			pBarrier->Transition.pResource = pTexture->mD3D12.pDxResource;
			pBarrier->Transition.Subresource = pTrans->mSubresourceBarrier ? CALC_SUBRESOURCE_INDEX(
																				 pTrans->mMipLevel, pTrans->mArrayLayer, 0,
																				 pTexture->mMipLevels, pTexture->mArraySizeMinusOne + 1)
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
			pBarrier->UAV.pResource = pTexture->mD3D12.pDxResource;
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
			pBarrier->Transition.pResource = pTexture->mD3D12.pDxResource;
			pBarrier->Transition.Subresource = pTrans->mSubresourceBarrier ? CALC_SUBRESOURCE_INDEX(
																				 pTrans->mMipLevel, pTrans->mArrayLayer, 0,
																				 pTexture->mMipLevels, pTexture->mArraySizeMinusOne + 1)
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
		}
	}

	if (transitionCount)
	{
#if defined(XBOX)
		if (pCmd->mD3D12.mDma.pDxCmdList)
		{
			pCmd->mD3D12.mDma.pDxCmdList->ResourceBarrier(transitionCount, barriers);
		}
		else
		{
			pCmd->mD3D12.pDxCmdList->ResourceBarrier(transitionCount, barriers);
		}
#else
		pCmd->mD3D12.pDxCmdList->ResourceBarrier(transitionCount, barriers);
#endif
	}
}

void d3d12_cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
	ASSERT(pCmd);
	ASSERT(pSrcBuffer);
	ASSERT(pSrcBuffer->mD3D12.pDxResource);
	ASSERT(pBuffer);
	ASSERT(pBuffer->mD3D12.pDxResource);

#if defined(XBOX)
	pCmd->mD3D12.mDma.pDxCmdList->CopyBufferRegion(pBuffer->mD3D12.pDxResource, dstOffset, pSrcBuffer->mD3D12.pDxResource, srcOffset, size);
#else
	pCmd->mD3D12.pDxCmdList->CopyBufferRegion(pBuffer->mD3D12.pDxResource, dstOffset, pSrcBuffer->mD3D12.pDxResource, srcOffset, size);
#endif
}

typedef struct SubresourceDataDesc
{
	uint64_t mSrcOffset;
	uint32_t mMipLevel;
	uint32_t mArrayLayer;
} SubresourceDataDesc;

void d3d12_cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pDesc)
{
	uint32_t subresource =
		CALC_SUBRESOURCE_INDEX(pDesc->mMipLevel, pDesc->mArrayLayer, 0, pTexture->mMipLevels, pTexture->mArraySizeMinusOne + 1);
	D3D12_RESOURCE_DESC resourceDesc = pTexture->mD3D12.pDxResource->GetDesc();

	D3D12_TEXTURE_COPY_LOCATION src = {};
	D3D12_TEXTURE_COPY_LOCATION dst = {};
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.pResource = pSrcBuffer->mD3D12.pDxResource;
	pCmd->pRenderer->mD3D12.pDxDevice->GetCopyableFootprints(
		&resourceDesc, subresource, 1, pDesc->mSrcOffset, &src.PlacedFootprint, NULL, NULL, NULL);
	src.PlacedFootprint.Offset = pDesc->mSrcOffset;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.pResource = pTexture->mD3D12.pDxResource;
	dst.SubresourceIndex = subresource;
#if defined(XBOX)
	pCmd->mD3D12.mDma.pDxCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
#else
	pCmd->mD3D12.pDxCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
#endif
}

void d3d12_cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const SubresourceDataDesc* pDesc)
{
	uint32_t subresource =
		CALC_SUBRESOURCE_INDEX(pDesc->mMipLevel, pDesc->mArrayLayer, 0, pTexture->mMipLevels, pTexture->mArraySizeMinusOne + 1);
	D3D12_RESOURCE_DESC resourceDesc = pTexture->mD3D12.pDxResource->GetDesc();

	D3D12_TEXTURE_COPY_LOCATION src = {};
	D3D12_TEXTURE_COPY_LOCATION dst = {};
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src.pResource = pTexture->mD3D12.pDxResource;
	src.SubresourceIndex = subresource;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst.pResource = pDstBuffer->mD3D12.pDxResource;
	pCmd->pRenderer->mD3D12.pDxDevice->GetCopyableFootprints(
		&resourceDesc, subresource, 1, pDesc->mSrcOffset, &dst.PlacedFootprint, NULL, NULL, NULL);
	dst.PlacedFootprint.Offset = pDesc->mSrcOffset;
#if defined(XBOX)
	pCmd->mD3D12.mDma.pDxCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
#else
	pCmd->mD3D12.pDxCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
#endif
}

/************************************************************************/
// Queue Fence Semaphore Functions
/************************************************************************/
void d3d12_acquireNextImage(
	Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pSwapChainImageIndex)
{
	UNREF_PARAM(pSignalSemaphore);
	UNREF_PARAM(pFence);
	ASSERT(pRenderer);
	ASSERT(pSwapChainImageIndex);

	//get latest backbuffer image
	HRESULT hr = hook_acquire_next_image(pRenderer->mD3D12.pDxDevice, pSwapChain);
	if (FAILED(hr))
	{
		LOGF(LogLevel::eERROR, "Failed to acquire next image");
		*pSwapChainImageIndex = UINT32_MAX;
		return;
	}

	*pSwapChainImageIndex = hook_get_swapchain_image_index(pSwapChain);
}

void d3d12_queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc)
{
	ASSERT(pDesc);

	uint32_t    cmdCount = pDesc->mCmdCount;
	Cmd**       pCmds = pDesc->ppCmds;
	Fence*      pFence = pDesc->pSignalFence;
	uint32_t    waitSemaphoreCount = pDesc->mWaitSemaphoreCount;
	Semaphore** ppWaitSemaphores = pDesc->ppWaitSemaphores;
	uint32_t    signalSemaphoreCount = pDesc->mSignalSemaphoreCount;
	Semaphore** ppSignalSemaphores = pDesc->ppSignalSemaphores;

	//ASSERT that given cmd list and given params are valid
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

	//execute given command list
	ASSERT(pQueue->mD3D12.pDxQueue);

	ID3D12CommandList** cmds = (ID3D12CommandList**)alloca(cmdCount * sizeof(ID3D12CommandList*));
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		cmds[i] = pCmds[i]->mD3D12.pDxCmdList;
	}

	for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
		pQueue->mD3D12.pDxQueue->Wait(ppWaitSemaphores[i]->mD3D12.pDxFence, ppWaitSemaphores[i]->mD3D12.mFenceValue - 1);

	pQueue->mD3D12.pDxQueue->ExecuteCommandLists(cmdCount, cmds);

	if (pFence)
		hook_signal(pQueue, pFence->mD3D12.pDxFence, pFence->mD3D12.mFenceValue++);

	for (uint32_t i = 0; i < signalSemaphoreCount; ++i)
		hook_signal(pQueue, ppSignalSemaphores[i]->mD3D12.pDxFence, ppSignalSemaphores[i]->mD3D12.mFenceValue++);
}

void d3d12_queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc)
{
	if (pDesc->pSwapChain)
	{
		SwapChain* pSwapChain = pDesc->pSwapChain;
		HRESULT    hr = hook_queue_present(pQueue, pSwapChain, pDesc->mIndex);
		if (FAILED(hr))
		{
#if defined(_WINDOWS)
			ID3D12Device* device = NULL;
			pSwapChain->mD3D12.pDxSwapChain->GetDevice(IID_ARGS(&device));
			HRESULT removeHr = device->GetDeviceRemovedReason();

			if (FAILED(removeHr))
			{
				threadSleep(5000);    // Wait for a few seconds to allow the driver to come back online before doing a reset.
				ResetDesc resetDesc;
				resetDesc.mType = RESET_TYPE_DEVICE_LOST;
				requestReset(&resetDesc);
			}

#if defined(ENABLE_NSIGHT_AFTERMATH)
			// DXGI_ERROR error notification is asynchronous to the NVIDIA display
			// driver's GPU crash handling. Give the Nsight Aftermath GPU crash dump
			// thread some time to do its work before terminating the process.
			sleep(3000);
#endif

#if defined(USE_DRED)
			ID3D12DeviceRemovedExtendedData* pDread;
			if (SUCCEEDED(device->QueryInterface(IID_ARGS(&pDread))))
			{
				D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs;
				if (SUCCEEDED(pDread->GetAutoBreadcrumbsOutput(&breadcrumbs)))
				{
					LOGF(LogLevel::eINFO, "Gathered auto-breadcrumbs output.");
				}

				D3D12_DRED_PAGE_FAULT_OUTPUT pageFault;
				if (SUCCEEDED(pDread->GetPageFaultAllocationOutput(&pageFault)))
				{
					LOGF(LogLevel::eINFO, "Gathered page fault allocation output.");
				}
			}
			pDread->Release();
#endif
			device->Release();
#endif
			LOGF(LogLevel::eERROR, "Failed to present swapchain render target");
		}
	}
}

void d3d12_waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
{
	// Wait for fence completion
	for (uint32_t i = 0; i < fenceCount; ++i)
	{
		FenceStatus fenceStatus;
		::getFenceStatus(pRenderer, ppFences[i], &fenceStatus);
		uint64_t fenceValue = ppFences[i]->mD3D12.mFenceValue - 1;
		//if (completedValue < fenceValue)
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
		{
			ppFences[i]->mD3D12.pDxFence->SetEventOnCompletion(fenceValue, ppFences[i]->mD3D12.pDxWaitIdleFenceEvent);
			WaitForSingleObject(ppFences[i]->mD3D12.pDxWaitIdleFenceEvent, INFINITE);
		}
	}
}

void d3d12_waitQueueIdle(Queue* pQueue)
{
	hook_signal(pQueue, pQueue->mD3D12.pFence->mD3D12.pDxFence, pQueue->mD3D12.pFence->mD3D12.mFenceValue++);

	uint64_t fenceValue = pQueue->mD3D12.pFence->mD3D12.mFenceValue - 1;
	if (pQueue->mD3D12.pFence->mD3D12.pDxFence->GetCompletedValue() < pQueue->mD3D12.pFence->mD3D12.mFenceValue - 1)
	{
		pQueue->mD3D12.pFence->mD3D12.pDxFence->SetEventOnCompletion(fenceValue, pQueue->mD3D12.pFence->mD3D12.pDxWaitIdleFenceEvent);
		WaitForSingleObject(pQueue->mD3D12.pFence->mD3D12.pDxWaitIdleFenceEvent, INFINITE);
	}
}

void d3d12_getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
	UNREF_PARAM(pRenderer);

	if (pFence->mD3D12.pDxFence->GetCompletedValue() < pFence->mD3D12.mFenceValue - 1)
		*pFenceStatus = FENCE_STATUS_INCOMPLETE;
	else
		*pFenceStatus = FENCE_STATUS_COMPLETE;
}
/************************************************************************/
// Utility functions
/************************************************************************/
TinyImageFormat d3d12_getRecommendedSwapchainFormat(bool hintHDR, bool hintSRGB) 
{ 
	return hook_get_recommended_swapchain_format(hintHDR, hintSRGB); 
}

/************************************************************************/
// Execute Indirect Implementation
/************************************************************************/
void d3d12_addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(pDesc->pArgDescs);
	ASSERT(ppCommandSignature);

	CommandSignature* pCommandSignature = (CommandSignature*)tf_calloc(1, sizeof(CommandSignature));
	ASSERT(pCommandSignature);

	bool needRootSignature = false;
	// calculate size through arguement types
	uint32_t commandStride = 0;
	IndirectArgumentType drawType = INDIRECT_ARG_INVALID;

	D3D12_INDIRECT_ARGUMENT_DESC* argumentDescs =
		(D3D12_INDIRECT_ARGUMENT_DESC*)alloca((pDesc->mIndirectArgCount + 1) * sizeof(D3D12_INDIRECT_ARGUMENT_DESC));

	for (uint32_t i = 0; i < pDesc->mIndirectArgCount; ++i)
	{
		const DescriptorInfo* desc = NULL;
		if (pDesc->pArgDescs[i].mType > INDIRECT_DISPATCH)
		{
			ASSERT(pDesc->pArgDescs[i].mIndex < pDesc->pRootSignature->mDescriptorCount);

			desc = &pDesc->pRootSignature->pDescriptors[pDesc->pArgDescs[i].mIndex];
			ASSERT(desc);
		}

		switch (pDesc->pArgDescs[i].mType)
		{
			case INDIRECT_CONSTANT:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
				argumentDescs[i].Constant.RootParameterIndex = desc->mHandleIndex;    //-V522
				argumentDescs[i].Constant.DestOffsetIn32BitValues = 0;
				argumentDescs[i].Constant.Num32BitValuesToSet = desc->mSize;
				commandStride += sizeof(UINT) * argumentDescs[i].Constant.Num32BitValuesToSet;
				needRootSignature = true;
				break;
			case INDIRECT_UNORDERED_ACCESS_VIEW:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW;
				argumentDescs[i].UnorderedAccessView.RootParameterIndex = desc->mHandleIndex;
				commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
				needRootSignature = true;
				break;
			case INDIRECT_SHADER_RESOURCE_VIEW:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW;
				argumentDescs[i].ShaderResourceView.RootParameterIndex = desc->mHandleIndex;
				commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
				needRootSignature = true;
				break;
			case INDIRECT_CONSTANT_BUFFER_VIEW:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
				argumentDescs[i].ConstantBufferView.RootParameterIndex = desc->mHandleIndex;
				commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
				needRootSignature = true;
				break;
			case INDIRECT_VERTEX_BUFFER:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
				argumentDescs[i].VertexBuffer.Slot = desc->mHandleIndex;
				commandStride += sizeof(D3D12_VERTEX_BUFFER_VIEW);
				needRootSignature = true;
				break;
			case INDIRECT_INDEX_BUFFER:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
				argumentDescs[i].VertexBuffer.Slot = desc->mHandleIndex;
				commandStride += sizeof(D3D12_INDEX_BUFFER_VIEW);
				needRootSignature = true;
				break;
			case INDIRECT_DRAW:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
				commandStride += sizeof(IndirectDrawArguments);
				// Only one draw command allowed. So make sure no other draw command args in the list
				ASSERT(INDIRECT_ARG_INVALID == drawType);
				drawType = INDIRECT_DRAW;
				break;
			case INDIRECT_DRAW_INDEX:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
				commandStride += sizeof(IndirectDrawIndexArguments);
				ASSERT(INDIRECT_ARG_INVALID == drawType);
				drawType = INDIRECT_DRAW_INDEX;
				break;
			case INDIRECT_DISPATCH:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
				commandStride += sizeof(IndirectDispatchArguments);
				ASSERT(INDIRECT_ARG_INVALID == drawType);
				drawType = INDIRECT_DISPATCH;
				break;
			default:
				ASSERT(false);
				break;
		}
	}

	if (needRootSignature)
	{
		ASSERT(pDesc->pRootSignature);
	}

	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = pDesc->mIndirectArgCount;
	commandSignatureDesc.ByteStride = commandStride;
	// If running Linked Mode (SLI) create command signature for all nodes
	// #NOTE : In non SLI mode, mNodeCount will be 0 which sets nodeMask to default value
	commandSignatureDesc.NodeMask = util_calculate_shared_node_mask(pRenderer);

	uint32_t alignedStride = round_up(commandStride, 16);
	if (!pDesc->mPacked && alignedStride != commandStride)
	{
		hook_modify_command_signature_desc(&commandSignatureDesc, alignedStride - commandStride);
	}

	CHECK_HRESULT(pRenderer->mD3D12.pDxDevice->CreateCommandSignature(
		&commandSignatureDesc, needRootSignature ? pDesc->pRootSignature->mD3D12.pDxRootSignature : NULL, IID_ARGS(&pCommandSignature->pDxHandle)));
	pCommandSignature->mStride = commandSignatureDesc.ByteStride;
	pCommandSignature->mDrawType = drawType;

	*ppCommandSignature = pCommandSignature;
}

void d3d12_removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
{
	UNREF_PARAM(pRenderer);
	SAFE_RELEASE(pCommandSignature->pDxHandle);
	SAFE_FREE(pCommandSignature);
}

void d3d12_cmdExecuteIndirect(
	Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
	Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
	ASSERT(pCommandSignature);
	ASSERT(pIndirectBuffer);

	if (!pCounterBuffer)
		pCmd->mD3D12.pDxCmdList->ExecuteIndirect(
			pCommandSignature->pDxHandle, maxCommandCount, pIndirectBuffer->mD3D12.pDxResource, bufferOffset, NULL, 0);
	else
		pCmd->mD3D12.pDxCmdList->ExecuteIndirect(
			pCommandSignature->pDxHandle, maxCommandCount, pIndirectBuffer->mD3D12.pDxResource, bufferOffset,
			pCounterBuffer->mD3D12.pDxResource, counterBufferOffset);
}
/************************************************************************/
// Query Heap Implementation
/************************************************************************/
void d3d12_getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
	ASSERT(pQueue);
	ASSERT(pFrequency);

	UINT64 freq = 0;
	pQueue->mD3D12.pDxQueue->GetTimestampFrequency(&freq);
	*pFrequency = (double)freq;
}

void d3d12_addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppQueryPool);

	QueryPool* pQueryPool = (QueryPool*)tf_calloc(1, sizeof(QueryPool));
	ASSERT(pQueryPool);

	pQueryPool->mD3D12.mType = util_to_dx12_query_type(pDesc->mType);
	pQueryPool->mCount = pDesc->mQueryCount;

	D3D12_QUERY_HEAP_DESC desc = {};
	desc.Count = pDesc->mQueryCount;
	desc.NodeMask = util_calculate_node_mask(pRenderer, pDesc->mNodeIndex);
	desc.Type = util_to_dx12_query_heap_type(pDesc->mType);
	pRenderer->mD3D12.pDxDevice->CreateQueryHeap(&desc, IID_ARGS(&pQueryPool->mD3D12.pDxQueryHeap));

	*ppQueryPool = pQueryPool;
}

void d3d12_removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
	UNREF_PARAM(pRenderer);
	SAFE_RELEASE(pQueryPool->mD3D12.pDxQueryHeap);

	SAFE_FREE(pQueryPool);
}

void d3d12_cmdResetQueryPool(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
	UNREF_PARAM(pCmd);
	UNREF_PARAM(pQueryPool);
	UNREF_PARAM(startQuery);
	UNREF_PARAM(queryCount);
}

void d3d12_cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
	D3D12_QUERY_TYPE type = pQueryPool->mD3D12.mType;
	switch (type)
	{
		case D3D12_QUERY_TYPE_OCCLUSION: break;
		case D3D12_QUERY_TYPE_BINARY_OCCLUSION: break;
		case D3D12_QUERY_TYPE_TIMESTAMP: pCmd->mD3D12.pDxCmdList->EndQuery(pQueryPool->mD3D12.pDxQueryHeap, type, pQuery->mIndex); break;
		case D3D12_QUERY_TYPE_PIPELINE_STATISTICS: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM1: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM2: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM3: break;
		default: break;
	}
}

void d3d12_cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery) { cmdBeginQuery(pCmd, pQueryPool, pQuery); }

void d3d12_cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
{
	pCmd->mD3D12.pDxCmdList->ResolveQueryData(
		pQueryPool->mD3D12.pDxQueryHeap, pQueryPool->mD3D12.mType, startQuery, queryCount, pReadbackBuffer->mD3D12.pDxResource,
		startQuery * 8);
}
/************************************************************************/
// Memory Stats Implementation
/************************************************************************/
void d3d12_calculateMemoryStats(Renderer* pRenderer, char** stats)
{
	WCHAR* wstats = NULL;
	pRenderer->mD3D12.pResourceAllocator->BuildStatsString(&wstats, TRUE);
	*stats = (char*)tf_malloc(wcslen(wstats) * sizeof(char));
	wcstombs(*stats, wstats, wcslen(wstats));
	pRenderer->mD3D12.pResourceAllocator->FreeStatsString(wstats);
}

void d3d12_calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
{
	D3D12MA::TotalStatistics stats;
	pRenderer->mD3D12.pResourceAllocator->CalculateStatistics(&stats);
	*usedBytes = stats.Total.Stats.BlockBytes;
	*totalAllocatedBytes = stats.Total.Stats.AllocationBytes;
}

void d3d12_freeMemoryStats(Renderer* pRenderer, char* stats) { tf_free(stats); }
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void d3d12_cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	// note: USE_PIX isn't the ideal test because we might be doing a debug build where pix
	// is not installed, or a variety of other reasons. It should be a separate #ifdef flag?
#if defined(USE_PIX)
	//color is in B8G8R8X8 format where X is padding
	PIXBeginEvent(pCmd->mD3D12.pDxCmdList, PIX_COLOR((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255)), pName);
#endif
}

void d3d12_cmdEndDebugMarker(Cmd* pCmd)
{
#if defined(USE_PIX)
	PIXEndEvent(pCmd->mD3D12.pDxCmdList);
#endif
}

void d3d12_cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if defined(USE_PIX)
	//color is in B8G8R8X8 format where X is padding
	PIXSetMarker(pCmd->mD3D12.pDxCmdList, PIX_COLOR((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255)), pName);
#endif
#if defined(ENABLE_NSIGHT_AFTERMATH)
	SetAftermathMarker(&pCmd->pRenderer->mAftermathTracker, pCmd->mD3D12.pDxCmdList, pName);
#endif
}

uint32_t d3d12_cmdWriteMarker(Cmd* pCmd, MarkerType markerType, uint32_t markerValue, Buffer* pBuffer, size_t offset, bool useAutoFlags)
{
	D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = pBuffer->mD3D12.mDxGpuAddress + offset * sizeof(uint32_t);

	uint                                 count = MARKER_TYPE_IN_OUT == markerType ? 2 : 1;
	D3D12_WRITEBUFFERIMMEDIATE_PARAMETER wbParam[2];
	D3D12_WRITEBUFFERIMMEDIATE_MODE      wbMode[2];

	if (count > 1)
	{
		wbParam[0].Dest = wbParam[1].Dest = gpuAddress;
		wbParam[0].Value = markerValue | (useAutoFlags ? (MARKER_TYPE_IN << 30) : 0);
		wbParam[1].Value = markerValue | (useAutoFlags ? (MARKER_TYPE_OUT << 30) : 0);

		wbMode[0] = (D3D12_WRITEBUFFERIMMEDIATE_MODE)MARKER_TYPE_IN;
		wbMode[1] = (D3D12_WRITEBUFFERIMMEDIATE_MODE)MARKER_TYPE_OUT;
	}
	else
	{
		wbParam[0].Dest = gpuAddress;
		wbParam[0].Value = markerValue | (useAutoFlags ? (markerType << 30) : 0);
		wbMode[0] = (D3D12_WRITEBUFFERIMMEDIATE_MODE)markerType;
	}

	((ID3D12GraphicsCommandList2*)pCmd->mD3D12.pDxCmdList)->WriteBufferImmediate(count, wbParam, wbMode);

	return markerValue;
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void d3d12_setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	ASSERT(pRenderer);
	ASSERT(pBuffer);
	ASSERT(pName);

	wchar_t wName[MAX_DEBUG_NAME_LENGTH] = {};
	size_t  numConverted = 0;
	mbstowcs_s(&numConverted, wName, pName, MAX_DEBUG_NAME_LENGTH);
	pBuffer->mD3D12.pDxResource->SetName(wName);
#endif
}

void d3d12_setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	ASSERT(pRenderer);
	ASSERT(pTexture);
	ASSERT(pName);

	wchar_t wName[MAX_DEBUG_NAME_LENGTH] = {};
	size_t  numConverted = 0;
	mbstowcs_s(&numConverted, wName, pName, MAX_DEBUG_NAME_LENGTH);
	pTexture->mD3D12.pDxResource->SetName(wName);
#endif
}

void d3d12_setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName)
{
	setTextureName(pRenderer, pRenderTarget->pTexture, pName);
}

void d3d12_setPipelineName(Renderer* pRenderer, Pipeline* pPipeline, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	ASSERT(pRenderer);
	ASSERT(pPipeline);
	ASSERT(pName);

	wchar_t wName[MAX_DEBUG_NAME_LENGTH] = {};
	size_t  numConverted = 0;
	mbstowcs_s(&numConverted, wName, pName, MAX_DEBUG_NAME_LENGTH);
	pPipeline->mD3D12.pDxPipelineState->SetName(wName);
#endif
}
/************************************************************************/
// Virtual Texture
/************************************************************************/
static void alignedDivision(
	const D3D12_TILED_RESOURCE_COORDINATE& extent, const D3D12_TILED_RESOURCE_COORDINATE& granularity, D3D12_TILED_RESOURCE_COORDINATE* out)
{
	out->X = (extent.X / granularity.X + ((extent.X % granularity.X) ? 1u : 0u));
	out->Y = (extent.Y / granularity.Y + ((extent.Y % granularity.Y) ? 1u : 0u));
	out->Z = (extent.Z / granularity.Z + ((extent.Z % granularity.Z) ? 1u : 0u));
}

// Allocate memory for the virtual page
static bool allocateVirtualPage(Renderer* pRenderer, Texture* pTexture, VirtualTexturePage& virtualPage, Buffer** ppIntermediateBuffer)
{
	if (virtualPage.mD3D12.pAllocation != NULL)
	{
		//already filled
		return false;
	};

	BufferDesc desc = {};
	desc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
	desc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	desc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
	//desc.mFormat = pTexture->mDesc.mFormat;
	desc.mStartState = RESOURCE_STATE_COPY_SOURCE;

	desc.mFirstElement = 0;
	desc.mElementCount = pTexture->pSvt->mSparseVirtualTexturePageWidth * pTexture->pSvt->mSparseVirtualTexturePageHeight;
	desc.mStructStride = sizeof(uint32_t);
	desc.mSize = desc.mElementCount * desc.mStructStride;
#if defined(ENABLE_GRAPHICS_DEBUG)
	wchar_t texNameBuf[MAX_DEBUG_NAME_LENGTH]{};
#if defined(_WINDOWS)
	UINT texNameBufSize = sizeof(texNameBuf);
	CHECK_HRESULT(pTexture->mD3D12.pDxResource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &texNameBufSize, texNameBuf));
#else
	// WKPDID_D3DDebugObjectNameW not available on xbox
	swprintf(texNameBuf, MAX_DEBUG_NAME_LENGTH, L"(tex %p)", pTexture);
#endif
	char debugNameBuffer[MAX_DEBUG_NAME_LENGTH]{};
	snprintf(debugNameBuffer, MAX_DEBUG_NAME_LENGTH, "%ls - VT page #%u intermediate buffer", texNameBuf, virtualPage.index);
	desc.pName = debugNameBuffer;
#endif
	addBuffer(pRenderer, &desc, ppIntermediateBuffer);

	D3D12MA::ALLOCATION_DESC allocDesc = {};
	allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)pTexture->mFormat);
	resourceDesc.Width = (UINT)pTexture->pSvt->mSparseVirtualTexturePageWidth;
	resourceDesc.Height = (UINT)pTexture->pSvt->mSparseVirtualTexturePageHeight;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;

	CHECK_HRESULT(pRenderer->mD3D12.pResourceAllocator->CreateResource(
		&allocDesc,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		NULL,
		&virtualPage.mD3D12.pAllocation,
		__uuidof(ID3D12Resource), NULL));

	ASSERT(virtualPage.mD3D12.pAllocation->GetHeap());

	// Offsets used by VT are in pages, not in bytes, so the allocation needs to be aligned properly
	ASSERT((virtualPage.mD3D12.pAllocation->GetOffset() % virtualPage.mD3D12.pAllocation->GetSize()) == 0);

#if defined(ENABLE_GRAPHICS_DEBUG)
	wchar_t debugNameBufferW[MAX_DEBUG_NAME_LENGTH]{};
	swprintf(debugNameBufferW, MAX_DEBUG_NAME_LENGTH, L"%ls - VT page #%u", texNameBuf, virtualPage.index);
	virtualPage.mD3D12.pAllocation->SetName(debugNameBufferW);

	CHECK_HRESULT(virtualPage.mD3D12.pAllocation->GetResource()->SetName(debugNameBufferW));
#endif

	++pTexture->pSvt->mVirtualPageAliveCount;

	return true;
}

struct D3D12VTPendingPageDeletion
{
	D3D12MA::Allocation** pAllocations;
	uint32_t* pAllocationsCount;

	Buffer** pIntermediateBuffers;
	uint32_t* pIntermediateBuffersCount;
};

// When updating virtual textures, this data needs to be stored per-heap
struct D3D12VTHeapPageData
{
	uint32_t* pRangeStartOffsets = NULL;
	D3D12_TILED_RESOURCE_COORDINATE* pSparseCoordinates = NULL;

	uint32_t mRangeStartOffsetsCount = 0;
	uint32_t mSparseCoordinatesCount = 0;
};

VirtualTexturePage* addPage(Renderer* pRenderer, Texture* pTexture, const D3D12_TILED_RESOURCE_COORDINATE& offset, const D3D12_TILED_RESOURCE_COORDINATE& extent,
	const uint32_t size, const uint32_t mipLevel, uint32_t layer, uint32_t pageIndex)
{
	VirtualTexturePage& newPage = pTexture->pSvt->pPages[pageIndex];

	newPage.mD3D12.offset = offset;
	newPage.mD3D12.extent = extent;
	newPage.mD3D12.size = size;
	newPage.mipLevel = mipLevel;
	newPage.layer = layer;
	newPage.index = pageIndex;

	return &newPage;
}

struct VTReadbackBufOffsets
{
	uint* pAlivePageCount;
	uint* pRemovePageCount;
	uint* pAlivePages;
	uint* pRemovePages;
	uint mTotalSize;
};
static VTReadbackBufOffsets vtGetReadbackBufOffsets(uint32_t* buffer, uint32_t readbackBufSize, uint32_t pageCount, uint32_t currentImage)
{
	ASSERT(!!buffer == !!readbackBufSize);  // If you already know the readback buf size, why is buffer null?

	VTReadbackBufOffsets offsets;
	offsets.pAlivePageCount = buffer + ((readbackBufSize / sizeof(uint32_t)) * currentImage);
	offsets.pRemovePageCount = offsets.pAlivePageCount + 1;
	offsets.pAlivePages = offsets.pRemovePageCount + 1;
	offsets.pRemovePages = offsets.pAlivePages + pageCount;

	offsets.mTotalSize = (uint)((offsets.pRemovePages - offsets.pAlivePageCount) + pageCount) * sizeof(uint);
	return offsets;
}
static uint32_t vtGetReadbackBufSize(uint32_t pageCount, uint32_t imageCount)
{
	VTReadbackBufOffsets offsets = vtGetReadbackBufOffsets(NULL, 0, pageCount, 0);
	return offsets.mTotalSize;
}
static D3D12VTPendingPageDeletion vtGetPendingPageDeletion(VirtualTexture* pSvt, uint32_t currentImage)
{
	if (pSvt->mPendingDeletionCount <= currentImage)
	{
		// Grow arrays
		const uint32_t oldDeletionCount = pSvt->mPendingDeletionCount;
		pSvt->mPendingDeletionCount = currentImage + 1;
		pSvt->mD3D12.pPendingDeletedAllocations = (D3D12MA::Allocation**)tf_realloc(pSvt->mD3D12.pPendingDeletedAllocations,
			pSvt->mPendingDeletionCount * pSvt->mVirtualPageTotalCount * sizeof(pSvt->mD3D12.pPendingDeletedAllocations[0]));

		pSvt->pPendingDeletedAllocationsCount = (uint32_t*)tf_realloc(pSvt->pPendingDeletedAllocationsCount,
			pSvt->mPendingDeletionCount * sizeof(pSvt->pPendingDeletedAllocationsCount[0]));

		pSvt->pPendingDeletedBuffers = (Buffer**)tf_realloc(pSvt->pPendingDeletedBuffers,
			pSvt->mPendingDeletionCount * pSvt->mVirtualPageTotalCount * sizeof(pSvt->pPendingDeletedBuffers[0]));

		pSvt->pPendingDeletedBuffersCount = (uint32_t*)tf_realloc(pSvt->pPendingDeletedBuffersCount,
			pSvt->mPendingDeletionCount * sizeof(pSvt->pPendingDeletedBuffersCount[0]));

		// Zero the new counts
		for (uint32_t i = oldDeletionCount; i < pSvt->mPendingDeletionCount; i++)
		{
			pSvt->pPendingDeletedAllocationsCount[i] = 0;
			pSvt->pPendingDeletedBuffersCount[i] = 0;
		}
	}

	D3D12VTPendingPageDeletion pendingDeletion;
	pendingDeletion.pAllocations = &pSvt->mD3D12.pPendingDeletedAllocations[currentImage * pSvt->mVirtualPageTotalCount];
	pendingDeletion.pAllocationsCount = &pSvt->pPendingDeletedAllocationsCount[currentImage];
	pendingDeletion.pIntermediateBuffers = &pSvt->pPendingDeletedBuffers[currentImage * pSvt->mVirtualPageTotalCount];
	pendingDeletion.pIntermediateBuffersCount = &pSvt->pPendingDeletedBuffersCount[currentImage];
	return pendingDeletion;
}

void d3d12_releasePage(Cmd* pCmd, Texture* pTexture, uint32_t currentImage)
{
	Renderer* pRenderer = pCmd->pRenderer;

	VTReadbackBufOffsets readbackOffsets = vtGetReadbackBufOffsets(
		(uint*)pTexture->pSvt->pReadbackBuffer->pCpuMappedAddress,
		pTexture->pSvt->mReadbackBufferSize,
		pTexture->pSvt->mVirtualPageTotalCount,
		currentImage);

	const uint removePageCount = *readbackOffsets.pRemovePageCount;

	uint32_t* RemovePageTable = readbackOffsets.pRemovePages;

	const D3D12VTPendingPageDeletion pendingDeletion = vtGetPendingPageDeletion(pTexture->pSvt, currentImage);

	// Release pending pages
	{
		for (size_t i = 0; i < *pendingDeletion.pAllocationsCount; i++)
			pendingDeletion.pAllocations[i]->Release();
		for (size_t i = 0; i < *pendingDeletion.pIntermediateBuffersCount; i++)
			removeBuffer(pRenderer, pendingDeletion.pIntermediateBuffers[i]);

		*pendingDeletion.pAllocationsCount = 0;
		*pendingDeletion.pIntermediateBuffersCount = 0;
	}

	// Schedule release of newly unneeded pages and unmap the associated tiles
	uint removePageIndex = 0;
	while (removePageIndex < removePageCount)
 	{
		const uint MAX_UNBINDS = 1024;
		D3D12_TILED_RESOURCE_COORDINATE pageUnbinds[MAX_UNBINDS];
		uint pageUnbindCount = 0;

		// Schedule release of newly unneeded pages
		for ( ; removePageIndex < (int)removePageCount && pageUnbindCount < MAX_UNBINDS; ++removePageIndex)
		{
			uint32_t RemoveIndex = RemovePageTable[removePageIndex];
			VirtualTexturePage& removePage = pTexture->pSvt->pPages[RemoveIndex];

			// Never remove the lowest mip level
			if ((int)removePage.mipLevel >= (pTexture->pSvt->mTiledMipLevelCount - 1))
				continue;

			if (removePage.mD3D12.pAllocation)
			{
				pendingDeletion.pAllocations[(*pendingDeletion.pAllocationsCount)++] = removePage.mD3D12.pAllocation;
				removePage.mD3D12.pAllocation = NULL;

				D3D12_TILED_RESOURCE_COORDINATE coord = removePage.mD3D12.offset;
				coord.X /= (UINT)pTexture->pSvt->mSparseVirtualTexturePageWidth;
				coord.Y /= (UINT)pTexture->pSvt->mSparseVirtualTexturePageHeight;
				pageUnbinds[pageUnbindCount++] = coord;
				--pTexture->pSvt->mVirtualPageAliveCount;
			}
		}

		if (pageUnbindCount > 0)
		{
			// Unmap tiles
			D3D12_TILE_RANGE_FLAGS nullFlags = D3D12_TILE_RANGE_FLAG_NULL;
			pCmd->pQueue->mD3D12.pDxQueue->UpdateTileMappings(pTexture->mD3D12.pDxResource,
				pageUnbindCount,
				pageUnbinds,
				NULL, // null for resource region sizes means all sizes are a single tile
				NULL, // null ID3D12Heap* since we're unbinding
				1, &nullFlags, NULL, NULL, D3D12_TILE_MAPPING_FLAG_NONE);
		}
 	}
}

typedef struct HeapNode
{
	ID3D12Heap* key;
	D3D12VTHeapPageData value;
}HeapNode;

void d3d12_uploadVirtualTexturePage(Cmd* pCmd, Texture* pTexture, VirtualTexturePage* pPage,
	HeapNode** ppHeapPageData, uint32_t currentImage)
{
	Renderer* pRenderer = pCmd->pRenderer;
	Buffer* pIntermediateBuffer = NULL;
	if (allocateVirtualPage(pRenderer, pTexture, *pPage, &pIntermediateBuffer))
	{
		HeapNode* pHeapPageData = *ppHeapPageData;

		uint32_t heapPageIndex = (uint32_t)(pPage->mD3D12.pAllocation->GetOffset() / pPage->mD3D12.size); // Page index into the current heap

		void* pData = (void*)((unsigned char*)pTexture->pSvt->pVirtualImageData + (pPage->index * (uint32_t)pPage->mD3D12.size));

		const bool intermediateMap = !pIntermediateBuffer->pCpuMappedAddress;
		if (intermediateMap)
		{
			mapBuffer(pRenderer, pIntermediateBuffer, NULL);
		}

		memcpy(pIntermediateBuffer->pCpuMappedAddress, pData, pPage->mD3D12.size);

		if (intermediateMap)
		{
			unmapBuffer(pRenderer, pIntermediateBuffer);
		}

		D3D12_TILED_RESOURCE_COORDINATE startCoord;
		startCoord.X = pPage->mD3D12.offset.X / (uint)pTexture->pSvt->mSparseVirtualTexturePageWidth;
		startCoord.Y = pPage->mD3D12.offset.Y / (uint)pTexture->pSvt->mSparseVirtualTexturePageHeight;
		startCoord.Z = pPage->mD3D12.offset.Z;
		startCoord.Subresource = pPage->mD3D12.offset.Subresource;
		
		ID3D12Heap* pHeap = pPage->mD3D12.pAllocation->GetHeap();
		HeapNode* pNode = hmgetp_null(pHeapPageData, pHeap);
		D3D12VTHeapPageData* pHeapData = pNode ? &pNode->value : NULL;

		if (!pHeapData)
			pHeapData = &hmput(pHeapPageData, pHeap, pHeapPageData[-1].value);

		ASSERT(!!pHeapData->pSparseCoordinates == !!pHeapData->pRangeStartOffsets);
		if (!pHeapData->pSparseCoordinates)
		{
			const D3D12_HEAP_DESC heapDesc = pPage->mD3D12.pAllocation->GetHeap()->GetDesc();
			const uint32_t maxCount = min((uint32_t)(heapDesc.SizeInBytes / pPage->mD3D12.size), pTexture->pSvt->mVirtualPageTotalCount);
			pHeapData->pSparseCoordinates = (D3D12_TILED_RESOURCE_COORDINATE*)tf_calloc(maxCount, sizeof(D3D12_TILED_RESOURCE_COORDINATE) + sizeof(uint32_t));
			pHeapData->pRangeStartOffsets = (uint32_t*)&pHeapData->pSparseCoordinates[maxCount];
			pHeapData->mSparseCoordinatesCount = 0;
			pHeapData->mRangeStartOffsetsCount = 0;
		}
		pHeapData->pSparseCoordinates[pHeapData->mSparseCoordinatesCount++] = startCoord;
		pHeapData->pRangeStartOffsets[pHeapData->mRangeStartOffsetsCount++] = heapPageIndex;

		D3D12_RESOURCE_DESC         Desc = pTexture->mD3D12.pDxResource->GetDesc();
		D3D12_TEXTURE_COPY_LOCATION Dst = {};
		Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		Dst.pResource = pPage->mD3D12.pAllocation->GetResource();
		Dst.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION Src = {};
		Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		Src.pResource = pIntermediateBuffer->mD3D12.pDxResource;
		Src.PlacedFootprint =
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT{ 0,
								{ Desc.Format,
									(UINT)pTexture->pSvt->mSparseVirtualTexturePageWidth, (UINT)pTexture->pSvt->mSparseVirtualTexturePageHeight, 1, (UINT)(pTexture->pSvt->mSparseVirtualTexturePageWidth * sizeof(uint32_t)) } };

		pCmd->mD3D12.pDxCmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, NULL);

		// Schedule deletion of this intermediate buffer
		const D3D12VTPendingPageDeletion pendingDeletion = vtGetPendingPageDeletion(pTexture->pSvt, currentImage);
		pendingDeletion.pIntermediateBuffers[(*pendingDeletion.pIntermediateBuffersCount)++] = pIntermediateBuffer;
		
		// Assign hash map, because it can be reallocated
		*ppHeapPageData = pHeapPageData;
	}
}

void d3d12_updateVirtualTextureHeap(Cmd* pCmd, Texture* pTexture, HeapNode* pHeapPageData)
{
	// Update sparse bind info
	for (ptrdiff_t i = 0; i < hmlen(pHeapPageData); ++i)
	{
		D3D12VTHeapPageData* pHeapData = &pHeapPageData[i].value;

		pCmd->pQueue->mD3D12.pDxQueue->UpdateTileMappings(
			pTexture->mD3D12.pDxResource,
			pHeapData->mSparseCoordinatesCount,
			pHeapData->pSparseCoordinates,
			NULL,  // All regions are single tiles
			pHeapPageData[i].key, // ID3D12Heap*
			pHeapData->mRangeStartOffsetsCount,
			NULL,  // All ranges are sequential tiles in the heap
			pHeapData->pRangeStartOffsets,
			(const UINT*)pTexture->pSvt->mD3D12.pCachedTileCounts,
			D3D12_TILE_MAPPING_FLAG_NONE);

		// Free the combined allocation
		tf_free(pHeapData->pSparseCoordinates);
		pHeapData->pRangeStartOffsets = NULL;
		pHeapData->pSparseCoordinates = NULL;
	}
}

// Fill a complete mip level
// Need to get visibility info first then fill them
void d3d12_fillVirtualTexture(Cmd* pCmd, Texture* pTexture, Fence* pFence, uint32_t currentImage)
{
	HeapNode* heapPageData = NULL;
	D3D12VTHeapPageData defaultHeapData = {};
	hmdefault(heapPageData, defaultHeapData);

	VTReadbackBufOffsets readbackOffsets = vtGetReadbackBufOffsets(
		(uint*)pTexture->pSvt->pReadbackBuffer->pCpuMappedAddress,
		pTexture->pSvt->mReadbackBufferSize,
		pTexture->pSvt->mVirtualPageTotalCount,
		currentImage);

	const uint alivePageCount = *readbackOffsets.pAlivePageCount;

	uint32_t* VisibilityData = readbackOffsets.pAlivePages;

	for (int i = 0; i < (int)alivePageCount; ++i)
	{
		uint32_t globalPageIndex = VisibilityData[i]; // Page index into the SVT
		ASSERT(globalPageIndex < pTexture->pSvt->mVirtualPageTotalCount);
		VirtualTexturePage* pPage = &pTexture->pSvt->pPages[globalPageIndex];
		ASSERT(globalPageIndex == pPage->index);

		d3d12_uploadVirtualTexturePage(pCmd, pTexture, pPage, &heapPageData, currentImage);
	}

	d3d12_updateVirtualTextureHeap(pCmd, pTexture, heapPageData);
	hmfree(heapPageData);
}

// Fill smallest (non-tail) mip map level
void d3d12_fillVirtualTextureLevel(Cmd* pCmd, Texture* pTexture, uint32_t mipLevel, uint32_t currentImage)
{
	HeapNode* heapPageData = NULL;
	D3D12VTHeapPageData defaultHeapData = {};
	hmdefault(heapPageData, defaultHeapData);

	for (int i = 0; i < (int)pTexture->pSvt->mVirtualPageTotalCount; i++)
	{
		uint32_t globalPageIndex = i; // Page index into the SVT
		VirtualTexturePage* pPage = &pTexture->pSvt->pPages[globalPageIndex];
		ASSERT(globalPageIndex == pPage->index);

		if (pPage->mipLevel == mipLevel)
		{
			d3d12_uploadVirtualTexturePage(pCmd, pTexture, pPage, &heapPageData, currentImage);
		}
	}

	d3d12_updateVirtualTextureHeap(pCmd, pTexture, heapPageData);
	hmfree(heapPageData);
}

void d3d12_addVirtualTexture(Cmd* pCmd, const TextureDesc* pDesc, Texture** ppTexture, void* pImageData)
{
	ASSERT(pCmd);
	Texture* pTexture = (Texture*)tf_calloc_memalign(1, alignof(Texture), sizeof(*pTexture) + sizeof(VirtualTexture));
	ASSERT(pTexture);
	pTexture->mD3D12.mDescriptors = D3D12_DESCRIPTOR_ID_NONE;

	Renderer* pRenderer = pCmd->pRenderer;

	pTexture->pSvt = (VirtualTexture*)(pTexture + 1);

	uint32_t imageSize = 0;
	uint32_t mipSize = pDesc->mWidth * pDesc->mHeight * pDesc->mDepth;

	while (mipSize > 0)
	{
		imageSize += mipSize;
		mipSize /= 4;
	}

	pTexture->pSvt->pVirtualImageData = pImageData;
	pTexture->mFormat = pDesc->mFormat;
	ASSERT(pTexture->mFormat == pDesc->mFormat);

	//add to gpu
	D3D12_RESOURCE_DESC desc = {};
	DXGI_FORMAT         dxFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)pTexture->mFormat);

	ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

	desc.Width = pDesc->mWidth;
	desc.Height = pDesc->mHeight;
	desc.MipLevels = pDesc->mMipLevels;
	desc.Format = dxFormat;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	desc.DepthOrArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;

	D3D12_RESOURCE_STATES res_states = util_to_dx12_resource_state(pDesc->mStartState);

	CHECK_HRESULT(pRenderer->mD3D12.pDxDevice->CreateReservedResource(&desc, res_states, NULL, IID_ARGS(&pTexture->mD3D12.pDxResource)));

#if defined(ENABLE_GRAPHICS_DEBUG)
	if (pDesc->pName && pDesc->pName[0])
	{
		wchar_t buf[MAX_DEBUG_NAME_LENGTH]{};
		mbstowcs(buf, pDesc->pName, MAX_DEBUG_NAME_LENGTH - 1);
		pTexture->mD3D12.pDxResource->SetName(buf);
	}
#endif

	D3D12_SHADER_RESOURCE_VIEW_DESC  srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = dxFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	add_srv(pRenderer, pTexture->mD3D12.pDxResource, &srvDesc, &pTexture->mD3D12.mDescriptors);

	UINT numTiles = 0;
	D3D12_PACKED_MIP_INFO packedMipInfo;
	D3D12_TILE_SHAPE tileShape = {};
	UINT subresourceCount = pDesc->mMipLevels;
	D3D12_SUBRESOURCE_TILING* tilings = (D3D12_SUBRESOURCE_TILING*)tf_calloc(subresourceCount, sizeof(D3D12_SUBRESOURCE_TILING));
	pRenderer->mD3D12.pDxDevice->GetResourceTiling(pTexture->mD3D12.pDxResource, &numTiles, &packedMipInfo, &tileShape, &subresourceCount, 0, &tilings[0]);
	tf_free(tilings);
	tilings = NULL;

	pTexture->pSvt->mSparseVirtualTexturePageWidth = tileShape.WidthInTexels;
	pTexture->pSvt->mSparseVirtualTexturePageHeight = tileShape.HeightInTexels;
	pTexture->pSvt->mVirtualPageTotalCount = imageSize / (uint32_t)(pTexture->pSvt->mSparseVirtualTexturePageWidth * pTexture->pSvt->mSparseVirtualTexturePageHeight);
	pTexture->pSvt->mReadbackBufferSize = vtGetReadbackBufSize(pTexture->pSvt->mVirtualPageTotalCount, 1);
	pTexture->pSvt->mPageVisibilityBufferSize = pTexture->pSvt->mVirtualPageTotalCount * 2 * sizeof(uint);
	pTexture->pSvt->mD3D12.pCachedTileCounts = (uint32_t*)tf_calloc(pTexture->pSvt->mVirtualPageTotalCount, sizeof(uint32_t));
	pTexture->pSvt->pPages = (VirtualTexturePage*)tf_calloc(pTexture->pSvt->mVirtualPageTotalCount, sizeof(VirtualTexturePage));

	for (uint32_t i = 0; i < numTiles; i++)
	{
		uint32_t* cachedTileCounts = (uint32_t*)pTexture->pSvt->mD3D12.pCachedTileCounts;
		cachedTileCounts[i] = 1;
	}

	uint32_t TiledMiplevel = pDesc->mMipLevels - (uint32_t)log2(float(min(pTexture->pSvt->mSparseVirtualTexturePageWidth, pTexture->pSvt->mSparseVirtualTexturePageHeight)));
	pTexture->pSvt->mTiledMipLevelCount = (uint8_t)TiledMiplevel;

	uint32_t currentPageIndex = 0;
	// Sparse bindings for each mip level of all layers outside of the mip tail
	for (uint32_t layer = 0; layer < 1; layer++)
	{
		// sparseMemoryReq.imageMipTailFirstLod is the first mip level that's stored inside the mip tail
		for (uint32_t mipLevel = 0; mipLevel < TiledMiplevel; mipLevel++)
		{
			D3D12_TILED_RESOURCE_COORDINATE extent;
			extent.X = max(pDesc->mWidth >> mipLevel, 1u);
			extent.Y = max(pDesc->mHeight >> mipLevel, 1u);
			extent.Z = max(pDesc->mDepth >> mipLevel, 1u);

			// Aligned sizes by image granularity
			D3D12_TILED_RESOURCE_COORDINATE imageGranularity;
			imageGranularity.X = tileShape.WidthInTexels;
			imageGranularity.Y = tileShape.HeightInTexels;
			imageGranularity.Z = tileShape.DepthInTexels;

			D3D12_TILED_RESOURCE_COORDINATE sparseBindCounts = {};
			D3D12_TILED_RESOURCE_COORDINATE lastBlockExtent = {};
			alignedDivision(extent, imageGranularity, &sparseBindCounts);
			lastBlockExtent.X = ((extent.X % imageGranularity.X) ? extent.X % imageGranularity.X : imageGranularity.X);
			lastBlockExtent.Y = ((extent.Y % imageGranularity.Y) ? extent.Y % imageGranularity.Y : imageGranularity.Y);
			lastBlockExtent.Z = ((extent.Z % imageGranularity.Z) ? extent.Z % imageGranularity.Z : imageGranularity.Z);

			// Alllocate memory for some blocks
			for (uint32_t z = 0; z < sparseBindCounts.Z; z++)
			{
				for (uint32_t y = 0; y < sparseBindCounts.Y; y++)
				{
					for (uint32_t x = 0; x < sparseBindCounts.X; x++)
					{
						// Offset
						D3D12_TILED_RESOURCE_COORDINATE offset;
						offset.X = x * imageGranularity.X;
						offset.Y = y * imageGranularity.Y;
						offset.Z = z * imageGranularity.Z;
						offset.Subresource = mipLevel;
						// Size of the page
						D3D12_TILED_RESOURCE_COORDINATE extent;
						extent.X = (x == sparseBindCounts.X - 1) ? lastBlockExtent.X : imageGranularity.X;
						extent.Y = (y == sparseBindCounts.Y - 1) ? lastBlockExtent.Y : imageGranularity.Y;
						extent.Z = (z == sparseBindCounts.Z - 1) ? lastBlockExtent.Z : imageGranularity.Z;
						extent.Subresource = mipLevel;

						// Add new virtual page
						addPage(pRenderer, pTexture, offset, extent, (uint32_t)pTexture->pSvt->mSparseVirtualTexturePageWidth * (uint32_t)pTexture->pSvt->mSparseVirtualTexturePageHeight * sizeof(uint), mipLevel, layer, currentPageIndex);
						currentPageIndex++;
					}
				}
			}
		}
	}

	ASSERT(currentPageIndex == pTexture->pSvt->mVirtualPageTotalCount);

	LOGF(LogLevel::eINFO, "Virtual Texture info: Dim %d x %d Pages %d", pDesc->mWidth, pDesc->mHeight, pTexture->pSvt->mVirtualPageTotalCount);

	d3d12_fillVirtualTextureLevel(pCmd, pTexture, TiledMiplevel - 1, 0);

	pTexture->mOwnsImage = true;
	pTexture->mNodeIndex = pDesc->mNodeIndex;
	pTexture->mMipLevels = pDesc->mMipLevels;
	pTexture->mWidth = pDesc->mWidth;
	pTexture->mHeight = pDesc->mHeight;
	pTexture->mDepth = pDesc->mDepth;

	////save tetxure in given pointer
	*ppTexture = pTexture;
}

void d3d12_removeVirtualTexture(Renderer* pRenderer, VirtualTexture* pSvt)
{
	ASSERT(!!pSvt->mD3D12.pPendingDeletedAllocations == !!pSvt->pPendingDeletedAllocationsCount);
	ASSERT(!!pSvt->pPendingDeletedBuffers == !!pSvt->pPendingDeletedBuffersCount);

	for (uint32_t deletionIndex = 0; deletionIndex < pSvt->mPendingDeletionCount; deletionIndex++)
	{
		const D3D12VTPendingPageDeletion pendingDeletion = vtGetPendingPageDeletion(pSvt, deletionIndex);

		for (uint32_t i = 0; i < *pendingDeletion.pAllocationsCount; i++)
			pendingDeletion.pAllocations[i]->Release();
		for (uint32_t i = 0; i < *pendingDeletion.pIntermediateBuffersCount; i++)
			removeBuffer(pRenderer, pendingDeletion.pIntermediateBuffers[i]);
	}
	tf_free(pSvt->mD3D12.pPendingDeletedAllocations);
	tf_free(pSvt->pPendingDeletedAllocationsCount);
	tf_free(pSvt->pPendingDeletedBuffers);
	tf_free(pSvt->pPendingDeletedBuffersCount);

	for (int i = 0; i < (int)pSvt->mVirtualPageTotalCount; i++)
	{
		VirtualTexturePage& virtualPage = pSvt->pPages[i];
		SAFE_RELEASE(virtualPage.mD3D12.pAllocation);
	}
	tf_free(pSvt->pPages);

	tf_free(pSvt->mD3D12.pCachedTileCounts);

	tf_free(pSvt->pVirtualImageData);
}

void d3d12_cmdUpdateVirtualTexture(Cmd* cmd, Texture* pTexture, uint32_t currentImage)
{
	ASSERT(pTexture->pSvt->pReadbackBuffer);

	const bool map = !pTexture->pSvt->pReadbackBuffer->pCpuMappedAddress;
	if (map)
	{
		mapBuffer(cmd->pRenderer, pTexture->pSvt->pReadbackBuffer, NULL);
	}

	d3d12_releasePage(cmd, pTexture, currentImage);
	d3d12_fillVirtualTexture(cmd, pTexture, NULL, currentImage);

	if (map)
	{
		unmapBuffer(cmd->pRenderer, pTexture->pSvt->pReadbackBuffer);
	}
}

#endif

void initD3D12Renderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer)
{
	// API functions
	addFence = d3d12_addFence;
	removeFence = d3d12_removeFence;
	addSemaphore = d3d12_addSemaphore;
	removeSemaphore = d3d12_removeSemaphore;
	addQueue = d3d12_addQueue;
	removeQueue = d3d12_removeQueue;
	addSwapChain = d3d12_addSwapChain;
	removeSwapChain = d3d12_removeSwapChain;

	// command pool functions
	addCmdPool = d3d12_addCmdPool;
	removeCmdPool = d3d12_removeCmdPool;
	addCmd = d3d12_addCmd;
	removeCmd = d3d12_removeCmd;
	addCmd_n = d3d12_addCmd_n;
	removeCmd_n = d3d12_removeCmd_n;

	addRenderTarget = d3d12_addRenderTarget;
	removeRenderTarget = d3d12_removeRenderTarget;
	addSampler = d3d12_addSampler;
	removeSampler = d3d12_removeSampler;

	// Resource Load functions
	addBuffer = d3d12_addBuffer;
	removeBuffer = d3d12_removeBuffer;
	mapBuffer = d3d12_mapBuffer;
	unmapBuffer = d3d12_unmapBuffer;
	cmdUpdateBuffer = d3d12_cmdUpdateBuffer;
	cmdUpdateSubresource = d3d12_cmdUpdateSubresource;
	cmdCopySubresource = d3d12_cmdCopySubresource;
	addTexture = d3d12_addTexture;
	removeTexture = d3d12_removeTexture;
	addVirtualTexture = d3d12_addVirtualTexture;
	removeVirtualTexture = d3d12_removeVirtualTexture;

	// shader functions
	addShaderBinary = d3d12_addShaderBinary;
	removeShader = d3d12_removeShader;

	addRootSignature = d3d12_addRootSignature;
	removeRootSignature = d3d12_removeRootSignature;

	// pipeline functions
	addPipeline = d3d12_addPipeline;
	removePipeline = d3d12_removePipeline;
	addPipelineCache = d3d12_addPipelineCache;
	getPipelineCacheData = d3d12_getPipelineCacheData;
	removePipelineCache = d3d12_removePipelineCache;

	// Descriptor Set functions
	addDescriptorSet = d3d12_addDescriptorSet;
	removeDescriptorSet = d3d12_removeDescriptorSet;
	updateDescriptorSet = d3d12_updateDescriptorSet;

	// command buffer functions
	resetCmdPool = d3d12_resetCmdPool;
	beginCmd = d3d12_beginCmd;
	endCmd = d3d12_endCmd;
	cmdBindRenderTargets = d3d12_cmdBindRenderTargets;
	cmdSetShadingRate = d3d12_cmdSetShadingRate;
	cmdSetViewport = d3d12_cmdSetViewport;
	cmdSetScissor = d3d12_cmdSetScissor;
	cmdSetStencilReferenceValue = d3d12_cmdSetStencilReferenceValue;
	cmdBindPipeline = d3d12_cmdBindPipeline;
	cmdBindDescriptorSet = d3d12_cmdBindDescriptorSet;
	cmdBindPushConstants = d3d12_cmdBindPushConstants;
	cmdBindDescriptorSetWithRootCbvs = d3d12_cmdBindDescriptorSetWithRootCbvs;
	cmdBindIndexBuffer = d3d12_cmdBindIndexBuffer;
	cmdBindVertexBuffer = d3d12_cmdBindVertexBuffer;
	cmdDraw = d3d12_cmdDraw;
	cmdDrawInstanced = d3d12_cmdDrawInstanced;
	cmdDrawIndexed = d3d12_cmdDrawIndexed;
	cmdDrawIndexedInstanced = d3d12_cmdDrawIndexedInstanced;
	cmdDispatch = d3d12_cmdDispatch;

	// Transition Commands
	cmdResourceBarrier = d3d12_cmdResourceBarrier;
	// Virtual Textures
	cmdUpdateVirtualTexture = d3d12_cmdUpdateVirtualTexture;

	// queue/fence/swapchain functions
	acquireNextImage = d3d12_acquireNextImage;
	queueSubmit = d3d12_queueSubmit;
	queuePresent = d3d12_queuePresent;
	waitQueueIdle = d3d12_waitQueueIdle;
	getFenceStatus = d3d12_getFenceStatus;
	waitForFences = d3d12_waitForFences;
	toggleVSync = d3d12_toggleVSync;

	getRecommendedSwapchainFormat = d3d12_getRecommendedSwapchainFormat;

	//indirect Draw functions
	addIndirectCommandSignature = d3d12_addIndirectCommandSignature;
	removeIndirectCommandSignature = d3d12_removeIndirectCommandSignature;
	cmdExecuteIndirect = d3d12_cmdExecuteIndirect;

	/************************************************************************/
	// GPU Query Interface
	/************************************************************************/
	getTimestampFrequency = d3d12_getTimestampFrequency;
	addQueryPool = d3d12_addQueryPool;
	removeQueryPool = d3d12_removeQueryPool;
	cmdResetQueryPool = d3d12_cmdResetQueryPool;
	cmdBeginQuery = d3d12_cmdBeginQuery;
	cmdEndQuery = d3d12_cmdEndQuery;
	cmdResolveQuery = d3d12_cmdResolveQuery;
	/************************************************************************/
	// Stats Info Interface
	/************************************************************************/
	calculateMemoryStats = d3d12_calculateMemoryStats;
	calculateMemoryUse = d3d12_calculateMemoryUse;
	freeMemoryStats = d3d12_freeMemoryStats;
	/************************************************************************/
	// Debug Marker Interface
	/************************************************************************/
	cmdBeginDebugMarker = d3d12_cmdBeginDebugMarker;
	cmdEndDebugMarker = d3d12_cmdEndDebugMarker;
	cmdAddDebugMarker = d3d12_cmdAddDebugMarker;
	cmdWriteMarker = d3d12_cmdWriteMarker;
	/************************************************************************/
	// Resource Debug Naming Interface
	/************************************************************************/
	setBufferName = d3d12_setBufferName;
	setTextureName = d3d12_setTextureName;
	setRenderTargetName = d3d12_setRenderTargetName;
	setPipelineName = d3d12_setPipelineName;

	d3d12_initRenderer(appName, pSettings, ppRenderer);
}

void exitD3D12Renderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);

	d3d12_exitRenderer(pRenderer);
}

void initD3D12RendererContext(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext)
{
	// No need to initialize API function pointers, initRenderer MUST be called before using anything else anyway.
	d3d12_initRendererContext(appName, pSettings, ppContext);
}

void exitD3D12RendererContext(RendererContext* pContext)
{
	ASSERT(pContext);

	d3d12_exitRendererContext(pContext);
}
#endif

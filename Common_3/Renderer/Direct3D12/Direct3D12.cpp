/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#ifdef DIRECT3D12

#define RENDERER_IMPLEMENTATION

#if defined(XBOX)
#include "../../../Xbox/Common_3/Renderer/Direct3D12/Direct3D12X.h"
#else
#define IID_ARGS IID_PPV_ARGS
#endif

// Pull in minimal Windows headers
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "../IRenderer.h"

#define D3D12MA_IMPLEMENTATION
#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#if defined(XBOX)
#define D3D12MA_DXGI_1_4 0
#endif
#include "../../ThirdParty/OpenSource/D3D12MemoryAllocator/Direct3D12MemoryAllocator.h"

#include "../../ThirdParty/OpenSource/EASTL/sort.h"
#include "../../ThirdParty/OpenSource/EASTL/string.h"
#include "../../ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../../ThirdParty/OpenSource/EASTL/string_hash_map.h"

#if defined(XBOX)
#include <pix3.h>
#else
#include "../../ThirdParty/OpenSource/winpixeventruntime/Include/WinPixEventRuntime/pix3.h"
#endif

#include "../../ThirdParty/OpenSource/renderdoc/renderdoc_app.h"

#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

#include "../../OS/Interfaces/ILog.h"
#include "../../OS/Core/RingBuffer.h"
#include "../../OS/Core/GPUConfig.h"

#include "Direct3D12CapBuilder.h"
#include "Direct3D12Hooks.h"

#include "../../ThirdParty/OpenSource/ags/AgsHelper.h"
#include "../../ThirdParty/OpenSource/nvapi/NvApiHelper.h"

#if !defined(_WIN32)
#error "Windows is needed!"
#endif

//
// C++ is the only language supported by D3D12:
//   https://msdn.microsoft.com/en-us/library/windows/desktop/dn899120(v=vs.85).aspx
//
#if !defined(__cplusplus)
#error "D3D12 requires C++! Sorry!"
#endif

#include "../../OS/Interfaces/IMemory.h"

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN ((D3D12_GPU_VIRTUAL_ADDRESS)-1)

extern void d3d12_createShaderReflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection);

//stubs for durango because Direct3D12Raytracing.cpp is not used on XBOX
#if defined(ENABLE_RAYTRACING)
extern void addRaytracingPipeline(const RaytracingPipelineDesc* pDesc, Pipeline** ppPipeline);
extern void fillRaytracingDescriptorHandle(AccelerationStructure* pAccelerationStructure, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle);
extern void cmdBindRaytracingPipeline(Cmd* pCmd, Pipeline* pPipeline);
#endif

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
#include "../../ThirdParty/OpenSource/DirectXShaderCompiler/inc/dxcapi.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
#endif

#define SAFE_FREE(p_var)           \
	if ((p_var))                   \
	{                              \
		tf_free((void*)(p_var)); \
		p_var = NULL;              \
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

void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget);
void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget);

// Internal utility functions (may become external one day)
uint64_t                    util_dx_determine_storage_counter_offset(uint64_t buffer_size);
DXGI_FORMAT                 util_to_dx_uav_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx_dsv_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx_srv_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx_stencil_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx_swapchain_format(TinyImageFormat format);
D3D12_SHADER_VISIBILITY     util_to_dx_shader_visibility(ShaderStage stages);
D3D12_DESCRIPTOR_RANGE_TYPE util_to_dx_descriptor_range(DescriptorType type);
D3D12_RESOURCE_STATES       util_to_dx_resource_state(ResourceState state);
D3D12_FILTER util_to_dx_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool aniso, bool comparisonFilterEnabled);
D3D12_TEXTURE_ADDRESS_MODE    util_to_dx_texture_address_mode(AddressMode addressMode);
D3D12_PRIMITIVE_TOPOLOGY_TYPE util_to_dx_primitive_topology_type(PrimitiveTopology topology);

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
	D3D12_CPU_DESCRIPTOR_HANDLE mNullTextureSRV[TEXTURE_DIM_COUNT];
	D3D12_CPU_DESCRIPTOR_HANDLE mNullTextureUAV[TEXTURE_DIM_COUNT];
	D3D12_CPU_DESCRIPTOR_HANDLE mNullBufferSRV;
	D3D12_CPU_DESCRIPTOR_HANDLE mNullBufferUAV;
	D3D12_CPU_DESCRIPTOR_HANDLE mNullBufferCBV;
	D3D12_CPU_DESCRIPTOR_HANDLE mNullSampler;
} NullDescriptors;
/************************************************************************/
// Descriptor Heap Structures
/************************************************************************/
/// CPU Visible Heap to store all the resources needing CPU read / write operations - Textures/Buffers/RTV
typedef struct DescriptorHeap
{
	typedef struct DescriptorHandle
	{
		D3D12_CPU_DESCRIPTOR_HANDLE mCpu;
		D3D12_GPU_DESCRIPTOR_HANDLE mGpu;
	} DescriptorHandle;

	/// DX Heap
	ID3D12DescriptorHeap*           pCurrentHeap;
	/// Lock for multi-threaded descriptor allocations
	Mutex*                          pMutex;
	ID3D12Device*                   pDevice;
	D3D12_CPU_DESCRIPTOR_HANDLE*    pHandles;
	/// Start position in the heap
	DescriptorHandle                mStartHandle;
	/// Free List used for CPU only descriptor heaps
	eastl::vector<DescriptorHandle> mFreeList;
	/// Description
	D3D12_DESCRIPTOR_HEAP_DESC      mDesc;
	/// DescriptorInfo Increment Size
	uint32_t                        mDescriptorSize;
	/// Used
	tfrg_atomic32_t                 mUsedDescriptors;
} DescriptorHeap;

typedef struct DescriptorIndexMap
{
	eastl::string_hash_map<uint32_t> mMap;
} DescriptorIndexMap;
/************************************************************************/
// Static Descriptor Heap Implementation
/************************************************************************/
static void add_descriptor_heap(ID3D12Device* pDevice, const D3D12_DESCRIPTOR_HEAP_DESC* pDesc, DescriptorHeap** ppDescHeap)
{
	uint32_t numDescriptors = pDesc->NumDescriptors;
	hook_modify_descriptor_heap_size(pDesc->Type, &numDescriptors);

	DescriptorHeap* pHeap = (DescriptorHeap*)tf_calloc(1, sizeof(*pHeap));

	pHeap->pMutex = (Mutex*)tf_calloc(1, sizeof(Mutex));
	pHeap->pMutex->Init();
	pHeap->pDevice = pDevice;

	// Keep 32 aligned for easy remove
	numDescriptors = round_up(numDescriptors, 32);

	D3D12_DESCRIPTOR_HEAP_DESC Desc = *pDesc;
	Desc.NumDescriptors = numDescriptors;

	pHeap->mDesc = Desc;

	CHECK_HRESULT(pDevice->CreateDescriptorHeap(&Desc, IID_ARGS(&pHeap->pCurrentHeap)));

	pHeap->mStartHandle.mCpu = pHeap->pCurrentHeap->GetCPUDescriptorHandleForHeapStart();
	pHeap->mStartHandle.mGpu = pHeap->pCurrentHeap->GetGPUDescriptorHandleForHeapStart();
	pHeap->mDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(pHeap->mDesc.Type);
	if (Desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
		pHeap->pHandles = (D3D12_CPU_DESCRIPTOR_HANDLE*)tf_calloc(Desc.NumDescriptors, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));

	*ppDescHeap = pHeap;
}

/// Resets the CPU Handle to start of heap and clears all stored resource ids
static void reset_descriptor_heap(DescriptorHeap* pHeap)
{
	pHeap->mUsedDescriptors = 0;
	pHeap->mFreeList.clear();
}

static void remove_descriptor_heap(DescriptorHeap* pHeap)
{
	SAFE_RELEASE(pHeap->pCurrentHeap);

	// Need delete since object frees allocated memory in destructor
	pHeap->pMutex->Destroy();
	tf_free(pHeap->pMutex);

	pHeap->mFreeList.~vector();

	SAFE_FREE(pHeap->pHandles);
	SAFE_FREE(pHeap);
}

static DescriptorHeap::DescriptorHandle consume_descriptor_handles(DescriptorHeap* pHeap, uint32_t descriptorCount)
{
	if (pHeap->mUsedDescriptors + descriptorCount > pHeap->mDesc.NumDescriptors)
	{
		MutexLock lock(*pHeap->pMutex);

		if ((pHeap->mDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE))
		{
			uint32_t currentOffset = pHeap->mUsedDescriptors;
			D3D12_DESCRIPTOR_HEAP_DESC desc = pHeap->mDesc;
			while(pHeap->mUsedDescriptors + descriptorCount > desc.NumDescriptors)
				desc.NumDescriptors <<= 1;
			ID3D12Device* pDevice = pHeap->pDevice;
			SAFE_RELEASE(pHeap->pCurrentHeap);
			pDevice->CreateDescriptorHeap(&desc, IID_ARGS(&pHeap->pCurrentHeap));
			pHeap->mDesc = desc;
			pHeap->mStartHandle.mCpu = pHeap->pCurrentHeap->GetCPUDescriptorHandleForHeapStart();
			pHeap->mStartHandle.mGpu = pHeap->pCurrentHeap->GetGPUDescriptorHandleForHeapStart();

			uint32_t* rangeSizes = (uint32_t*)alloca(pHeap->mUsedDescriptors * sizeof(uint32_t));
			uint32_t usedDescriptors = tfrg_atomic32_load_relaxed(&pHeap->mUsedDescriptors);
			for (uint32_t i = 0; i < pHeap->mUsedDescriptors; ++i)
				rangeSizes[i] = 1;
			pDevice->CopyDescriptors(1, &pHeap->mStartHandle.mCpu, &usedDescriptors,
				pHeap->mUsedDescriptors, pHeap->pHandles, rangeSizes,
				pHeap->mDesc.Type);
			D3D12_CPU_DESCRIPTOR_HANDLE* pNewHandles = (D3D12_CPU_DESCRIPTOR_HANDLE*)tf_calloc(pHeap->mDesc.NumDescriptors, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));
			memcpy(pNewHandles, pHeap->pHandles, pHeap->mUsedDescriptors * sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));
			SAFE_FREE(pHeap->pHandles);
			pHeap->pHandles = pNewHandles;
		}
		else if (descriptorCount == 1 && pHeap->mFreeList.size())
		{
			DescriptorHeap::DescriptorHandle ret = pHeap->mFreeList.back();
			pHeap->mFreeList.pop_back();
			return ret;
		}
	}

	uint32_t usedDescriptors = tfrg_atomic32_add_relaxed(&pHeap->mUsedDescriptors, descriptorCount);
	DescriptorHeap::DescriptorHandle ret =
	{
		{ pHeap->mStartHandle.mCpu.ptr + usedDescriptors * pHeap->mDescriptorSize },
		{ pHeap->mStartHandle.mGpu.ptr + usedDescriptors * pHeap->mDescriptorSize },
	};


	return ret;
}

void return_cpu_descriptor_handles(DescriptorHeap* pHeap, D3D12_CPU_DESCRIPTOR_HANDLE handle, uint32_t count)
{
	ASSERT((pHeap->mDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) == 0);
	for (uint32_t i = 0; i < count; ++i)
		pHeap->mFreeList.push_back({
		{ handle.ptr * pHeap->mDescriptorSize * i },
		D3D12_GPU_VIRTUAL_ADDRESS_NULL });
}

static void copy_descriptor_handle(DescriptorHeap* pHeap, const D3D12_CPU_DESCRIPTOR_HANDLE& srcHandle, const uint64_t& dstHandle, uint32_t index)
{
	pHeap->pHandles[(dstHandle / pHeap->mDescriptorSize) + index] = srcHandle;
	pHeap->pDevice->CopyDescriptorsSimple(
		1,
		{ pHeap->mStartHandle.mCpu.ptr + dstHandle + (index * pHeap->mDescriptorSize) },
		srcHandle,
		pHeap->mDesc.Type);
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
			ret.RenderTarget[i].DestBlendAlpha =
				gDx12BlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]];
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
using DescriptorNameToIndexMap = eastl::string_hash_map<uint32_t>;

const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName)
{
	DescriptorNameToIndexMap::const_iterator it = pRootSignature->pDescriptorNameToIndexMap->mMap.find(pResName);
	if (it != pRootSignature->pDescriptorNameToIndexMap->mMap.end())
	{
		return &pRootSignature->pDescriptors[it->second];
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
static void internal_log(LogType type, const char* msg, const char* component)
{
	switch (type)
	{
		case LOG_TYPE_INFO: LOGF(LogLevel::eINFO, "%s ( %s )", component, msg); break;
		case LOG_TYPE_WARN: LOGF(LogLevel::eWARNING, "%s ( %s )", component, msg); break;
		case LOG_TYPE_DEBUG: LOGF(LogLevel::eDEBUG, "%s ( %s )", component, msg); break;
		case LOG_TYPE_ERROR: LOGF(LogLevel::eERROR, "%s ( %s )", component, msg); break;
		default: break;
	}
}

static void add_srv(
	Renderer* pRenderer, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	if (D3D12_GPU_VIRTUAL_ADDRESS_NULL == pHandle->ptr)
		*pHandle = consume_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], 1).mCpu;
	pRenderer->pDxDevice->CreateShaderResourceView(pResource, pSrvDesc, *pHandle);
}

static void add_uav(
	Renderer* pRenderer, ID3D12Resource* pResource, ID3D12Resource* pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc,
	D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	if (D3D12_GPU_VIRTUAL_ADDRESS_NULL == pHandle->ptr)
		*pHandle = consume_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], 1).mCpu;
	pRenderer->pDxDevice->CreateUnorderedAccessView(pResource, pCounterResource, pUavDesc, *pHandle);
}

static void add_cbv(Renderer* pRenderer, const D3D12_CONSTANT_BUFFER_VIEW_DESC* pCbvDesc, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	if (D3D12_GPU_VIRTUAL_ADDRESS_NULL == pHandle->ptr)
		*pHandle = consume_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], 1).mCpu;
	pRenderer->pDxDevice->CreateConstantBufferView(pCbvDesc, *pHandle);
}

static void add_rtv(
	Renderer* pRenderer, ID3D12Resource* pResource, DXGI_FORMAT format, uint32_t mipSlice, uint32_t arraySlice,
	D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	if (D3D12_GPU_VIRTUAL_ADDRESS_NULL == pHandle->ptr)
		*pHandle = consume_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV], 1).mCpu;
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

	pRenderer->pDxDevice->CreateRenderTargetView(pResource, &rtvDesc, *pHandle);
}

static void add_dsv(
	Renderer* pRenderer, ID3D12Resource* pResource, DXGI_FORMAT format, uint32_t mipSlice, uint32_t arraySlice,
	D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	*pHandle = consume_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV], 1).mCpu;
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

	pRenderer->pDxDevice->CreateDepthStencilView(pResource, &dsvDesc, *pHandle);
}

static void add_sampler(Renderer* pRenderer, const D3D12_SAMPLER_DESC* pSamplerDesc, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	*pHandle =
		consume_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER], 1).mCpu;
	pRenderer->pDxDevice->CreateSampler(pSamplerDesc, *pHandle);
}

D3D12_DEPTH_STENCIL_DESC gDefaultDepthDesc = {};
D3D12_BLEND_DESC gDefaultBlendDesc = {};
D3D12_RASTERIZER_DESC gDefaultRasterizerDesc = {};

static void add_default_resources(Renderer* pRenderer)
{
	pRenderer->pNullDescriptors = (NullDescriptors*)tf_calloc(1, sizeof(NullDescriptors));
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

static uint32_t gRootSignatureDWORDS[GpuVendor::GPU_VENDOR_COUNT] =
{
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
typedef eastl::pair<ShaderResource, DescriptorInfo*> RootParameter;
typedef struct UpdateFrequencyLayoutInfo
{
	eastl::vector<RootParameter>                    mCbvSrvUavTable;
	eastl::vector<RootParameter>                    mSamplerTable;
	eastl::vector<RootParameter>                    mRootDescriptorParams;
	eastl::vector<RootParameter>                    mRootConstants;
	eastl::unordered_map<DescriptorInfo*, uint32_t> mDescriptorIndexMap;
} UpdateFrequencyLayoutInfo;

/// Calculates the total size of the root signature (in DWORDS) from the input layouts
uint32_t calculate_root_signature_size(UpdateFrequencyLayoutInfo* pLayouts, uint32_t numLayouts)
{
	uint32_t size = 0;
	for (uint32_t i = 0; i < numLayouts; ++i)
	{
		if ((uint32_t)pLayouts[i].mCbvSrvUavTable.size())
			size += gDescriptorTableDWORDS;
		if ((uint32_t)pLayouts[i].mSamplerTable.size())
			size += gDescriptorTableDWORDS;

		for (uint32_t c = 0; c < (uint32_t)pLayouts[i].mRootDescriptorParams.size(); ++c)
		{
			size += gRootDescriptorDWORDS;
		}
		for (uint32_t c = 0; c < (uint32_t)pLayouts[i].mRootConstants.size(); ++c)
		{
			DescriptorInfo* pDesc = pLayouts[i].mRootConstants[c].second;
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
		const ShaderResource* res = &tableRef[i].first;
		const DescriptorInfo* desc = tableRef[i].second;
		pRange[i].BaseShaderRegister = res->reg;
		pRange[i].RegisterSpace = res->set;
		pRange[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
		pRange[i].NumDescriptors = desc->mSize;
		pRange[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		pRange[i].RangeType = util_to_dx_descriptor_range((DescriptorType)desc->mType);
		stageCount |= res->used_stages;
	}
	pRootParam->ShaderVisibility = util_to_dx_shader_visibility(stageCount);
	pRootParam->DescriptorTable.NumDescriptorRanges = numDescriptors;
	pRootParam->DescriptorTable.pDescriptorRanges = pRange;
}

/// Creates a root descriptor / root constant parameter for root signature version 1_1
void create_root_descriptor(const RootParameter* pDesc, D3D12_ROOT_PARAMETER1* pRootParam)
{
	pRootParam->ShaderVisibility = util_to_dx_shader_visibility(pDesc->first.used_stages);
	pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pRootParam->Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
	pRootParam->Descriptor.ShaderRegister = pDesc->first.reg;
	pRootParam->Descriptor.RegisterSpace = pDesc->first.set;
}

void create_root_constant(const RootParameter* pDesc, D3D12_ROOT_PARAMETER1* pRootParam)
{
	pRootParam->ShaderVisibility = util_to_dx_shader_visibility(pDesc->first.used_stages);
	pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pRootParam->Constants.Num32BitValues = pDesc->second->mSize;
	pRootParam->Constants.ShaderRegister = pDesc->first.reg;
	pRootParam->Constants.RegisterSpace = pDesc->first.set;
}
/************************************************************************/
// Internal utility functions
/************************************************************************/
D3D12_FILTER util_to_dx_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool aniso, bool comparisonFilterEnabled)
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

D3D12_TEXTURE_ADDRESS_MODE util_to_dx_texture_address_mode(AddressMode addressMode)
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

D3D12_PRIMITIVE_TOPOLOGY_TYPE util_to_dx_primitive_topology_type(PrimitiveTopology topology)
{
	switch (topology)
	{
		case PRIMITIVE_TOPO_POINT_LIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		case PRIMITIVE_TOPO_LINE_LIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case PRIMITIVE_TOPO_LINE_STRIP: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case PRIMITIVE_TOPO_TRI_LIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		case PRIMITIVE_TOPO_TRI_STRIP: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		case PRIMITIVE_TOPO_PATCH_LIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	}
	return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
}

uint64_t util_dx_determine_storage_counter_offset(uint64_t buffer_size)
{
	uint64_t alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
	uint64_t result = (buffer_size + (alignment - 1)) & ~(alignment - 1);
	return result;
}

DXGI_FORMAT util_to_dx_uav_format(DXGI_FORMAT defaultFormat)
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
		case DXGI_FORMAT_D16_UNORM: LOGF( LogLevel::eERROR, "Requested a UAV format for a depth stencil format");
#endif

		default: return defaultFormat;
	}
}

DXGI_FORMAT util_to_dx_dsv_format(DXGI_FORMAT defaultFormat)
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

DXGI_FORMAT util_to_dx_srv_format(DXGI_FORMAT defaultFormat)
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

DXGI_FORMAT util_to_dx_stencil_format(DXGI_FORMAT defaultFormat)
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

DXGI_FORMAT util_to_dx_swapchain_format(TinyImageFormat const format)
{
	DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;

	// FLIP_DISCARD and FLIP_SEQEUNTIAL swapchain buffers only support these formats
	switch (format)
	{
		case TinyImageFormat_R16G16B16A16_SFLOAT: result = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
		case TinyImageFormat_B8G8R8A8_UNORM: result = DXGI_FORMAT_B8G8R8A8_UNORM; break;
		case TinyImageFormat_R8G8B8A8_UNORM: result = DXGI_FORMAT_R8G8B8A8_UNORM; break;
		case TinyImageFormat_B8G8R8A8_SRGB: result = DXGI_FORMAT_B8G8R8A8_UNORM; break;
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


D3D12_SHADER_VISIBILITY util_to_dx_shader_visibility(ShaderStage stages)
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
#ifdef ENABLE_RAYTRACING
	if (stages == SHADER_STAGE_RAYTRACING)
	{
		return D3D12_SHADER_VISIBILITY_ALL;
	}
#endif
	ASSERT(stageCount > 0);
	return stageCount > 1 ? D3D12_SHADER_VISIBILITY_ALL : res;
}

D3D12_DESCRIPTOR_RANGE_TYPE util_to_dx_descriptor_range(DescriptorType type)
{
	switch (type)
	{
		case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case DESCRIPTOR_TYPE_ROOT_CONSTANT: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		case DESCRIPTOR_TYPE_TEXTURE:
		case DESCRIPTOR_TYPE_BUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case DESCRIPTOR_TYPE_RW_BUFFER:
		case DESCRIPTOR_TYPE_RW_TEXTURE: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case DESCRIPTOR_TYPE_SAMPLER: return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
#ifdef ENABLE_RAYTRACING
		case DESCRIPTOR_TYPE_RAY_TRACING: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
#endif
		default: ASSERT("Invalid DescriptorInfo Type"); return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	}
}

D3D12_RESOURCE_STATES util_to_dx_resource_state(ResourceState state)
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
#ifdef ENABLE_RAYTRACING
	if (state & RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
		ret |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
#endif

	return ret;
}

D3D12_QUERY_HEAP_TYPE util_to_dx_query_heap_type(QueryType type)
{
	switch (type)
	{
		case QUERY_TYPE_TIMESTAMP: return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		case QUERY_TYPE_PIPELINE_STATISTICS: return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
		case QUERY_TYPE_OCCLUSION: return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
		default: ASSERT(false && "Invalid query heap type"); return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	}
}

D3D12_QUERY_TYPE util_to_dx_query_type(QueryType type)
{
	switch (type)
	{
		case QUERY_TYPE_TIMESTAMP: return D3D12_QUERY_TYPE_TIMESTAMP;
		case QUERY_TYPE_PIPELINE_STATISTICS: return D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
		case QUERY_TYPE_OCCLUSION: return D3D12_QUERY_TYPE_OCCLUSION;
		default: ASSERT(false && "Invalid query heap type"); return D3D12_QUERY_TYPE_OCCLUSION;
	}
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

static bool AddDevice(Renderer* pRenderer)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	//add debug layer if in debug mode
	if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(pRenderer->pDXDebug), (void**)&(pRenderer->pDXDebug))))
	{
		hook_enable_debug_layer(pRenderer);
	}
#endif

	D3D_FEATURE_LEVEL feature_levels[4] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

#if defined(XBOX)
	// Create the DX12 API device object.
	CHECK_HRESULT(hook_create_device(NULL, feature_levels[0], &pRenderer->pDxDevice));

#if defined(ENABLE_GRAPHICS_DEBUG)
	//Sets the callback functions to invoke when the GPU hangs
	//pRenderer->pDxDevice->SetHangCallbacksX(HANGBEGINCALLBACK, HANGPRINTCALLBACK, NULL);
#endif

	// First, retrieve the underlying DXGI device from the D3D device.
	IDXGIDevice1* dxgiDevice;
	CHECK_HRESULT(pRenderer->pDxDevice->QueryInterface(IID_ARGS(&dxgiDevice)));

	// Identify the physical adapter (GPU or card) this device is running on.
	IDXGIAdapter* dxgiAdapter;
	CHECK_HRESULT(dxgiDevice->GetAdapter(&dxgiAdapter));

	// And obtain the factory object that created it.
	CHECK_HRESULT(dxgiAdapter->GetParent(IID_ARGS(&pRenderer->pDXGIFactory)));

	uint32_t gpuCount = 1;
	GpuDesc gpuDesc[1] = {};
	dxgiAdapter->QueryInterface(IID_ARGS(&gpuDesc[0].pGpu));

	dxgiAdapter->Release();
	typedef bool(*DeviceBetterFn)(GpuDesc* gpuDesc, uint32_t testIndex, uint32_t refIndex);
	DeviceBetterFn isDeviceBetter = [](GpuDesc* gpuDesc, uint32_t testIndex, uint32_t refIndex) -> bool
	{
		return false;
	};

	hook_fill_gpu_desc(pRenderer, feature_levels[0], &gpuDesc[0]);

#else
	UINT flags = 0;
#if defined(ENABLE_GRAPHICS_DEBUG)
	flags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	CHECK_HRESULT(CreateDXGIFactory2(flags, IID_ARGS(&pRenderer->pDXGIFactory)));

	uint32_t gpuCount = 0;
	IDXGIAdapter4* adapter = NULL;
	bool foundSoftwareAdapter = false;

	// Find number of usable GPUs
	// Use DXGI6 interface which lets us specify gpu preference so we dont need to use NVOptimus or AMDPowerExpress exports
	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != pRenderer->pDXGIFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
		IID_ARGS(&adapter)); ++i)
	{
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
					GpuDesc gpuDesc = {};
					HRESULT hres = adapter->QueryInterface(IID_ARGS(&gpuDesc.pGpu));
					if (SUCCEEDED(hres))
					{
						SAFE_RELEASE(gpuDesc.pGpu);
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

	// If the only adapter we found is a software adapter, log error message for QA
	if (!gpuCount && foundSoftwareAdapter)
	{
		LOGF(eERROR, "The only available GPU has DXGI_ADAPTER_FLAG_SOFTWARE. Early exiting");
		ASSERT(false);
		return false;
	}

	ASSERT(gpuCount);
	GpuDesc* gpuDesc = (GpuDesc*)alloca(gpuCount * sizeof(GpuDesc));
	memset(gpuDesc, 0, gpuCount * sizeof(GpuDesc));
	gpuCount = 0;

	// Use DXGI6 interface which lets us specify gpu preference so we dont need to use NVOptimus or AMDPowerExpress exports
	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != pRenderer->pDXGIFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
		IID_ARGS(&adapter)); ++i)
	{
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
					HRESULT hres = adapter->QueryInterface(IID_ARGS(&gpuDesc[gpuCount].pGpu));
					if (SUCCEEDED(hres))
					{
						D3D12CreateDevice(adapter, feature_levels[level], IID_PPV_ARGS(&pRenderer->pDxDevice));
						hook_fill_gpu_desc(pRenderer, feature_levels[level], &gpuDesc[gpuCount]);
						//get preset for current gpu description
						gpuDesc[gpuCount].mPreset = getGPUPresetLevel(
							gpuDesc[gpuCount].mVendorId, gpuDesc[gpuCount].mDeviceId,
							gpuDesc[gpuCount].mRevisionId);

						++gpuCount;
						SAFE_RELEASE(pRenderer->pDxDevice);
						break;
					}
				}
			}
		}

		adapter->Release();
	}

	ASSERT(gpuCount > 0);

	typedef bool (*DeviceBetterFn)(GpuDesc* gpuDesc, uint32_t testIndex, uint32_t refIndex);
	DeviceBetterFn isDeviceBetter = [](GpuDesc* gpuDesc, uint32_t testIndex, uint32_t refIndex) -> bool
	{
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

	uint32_t gpuIndex = UINT32_MAX;
	GPUSettings* gpuSettings = (GPUSettings*)alloca(gpuCount * sizeof(GPUSettings));

	for (uint32_t i = 0; i < gpuCount; ++i)
	{
		gpuSettings[i] = {};
		gpuSettings[i].mUniformBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
		gpuSettings[i].mUploadBufferTextureAlignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
		gpuSettings[i].mUploadBufferTextureRowAlignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
		gpuSettings[i].mMultiDrawIndirect = true;
		gpuSettings[i].mMaxVertexInputBindings = 32U;

		//assign device ID
		strncpy(gpuSettings[i].mGpuVendorPreset.mModelId, gpuDesc[i].mDeviceId, MAX_GPU_VENDOR_STRING_LENGTH);
		//assign vendor ID
		strncpy(gpuSettings[i].mGpuVendorPreset.mVendorId, gpuDesc[i].mVendorId, MAX_GPU_VENDOR_STRING_LENGTH);
		//assign Revision ID
		strncpy(gpuSettings[i].mGpuVendorPreset.mRevisionId, gpuDesc[i].mRevisionId, MAX_GPU_VENDOR_STRING_LENGTH);
		//get name from api
		strncpy(gpuSettings[i].mGpuVendorPreset.mGpuName, gpuDesc[i].mName, MAX_GPU_VENDOR_STRING_LENGTH);
		//get preset
		gpuSettings[i].mGpuVendorPreset.mPresetLevel = gpuDesc[i].mPreset;
		//get wave lane count
		gpuSettings[i].mWaveLaneCount = gpuDesc[i].mFeatureDataOptions1.WaveLaneCountMin;
		gpuSettings[i].mROVsSupported = gpuDesc[i].mFeatureDataOptions.ROVsSupported ? true : false;
		gpuSettings[i].mTessellationSupported = gpuSettings[i].mGeometryShaderSupported = true;
		gpuSettings[i].mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_ALL;

		// Determine root signature size for this gpu driver
		DXGI_ADAPTER_DESC adapterDesc;
		gpuDesc[i].pGpu->GetDesc(&adapterDesc);
		gpuSettings[i].mMaxRootSignatureDWORDS = gRootSignatureDWORDS[util_to_internal_gpu_vendor(adapterDesc.VendorId)];
		LOGF(LogLevel::eINFO, "GPU[%i] detected. Vendor ID: %x, Model ID: %x, Revision ID: %x, Preset: %s, GPU Name: %S", i,
			adapterDesc.VendorId,
			adapterDesc.DeviceId,
			adapterDesc.Revision,
			presetLevelToString(gpuSettings[i].mGpuVendorPreset.mPresetLevel),
			adapterDesc.Description);

		// Check that gpu supports at least graphics
		if (gpuIndex == UINT32_MAX || isDeviceBetter(gpuDesc, i, gpuIndex))
		{
			gpuIndex = i;
		}
	}

#if defined(ACTIVE_TESTING_GPU) && !defined(DURANGO) && defined(AUTOMATED_TESTING)
	selectActiveGpu(gpuSettings, &gpuIndex, gpuCount);
#endif

	// Get the latest and greatest feature level gpu
	CHECK_HRESULT(gpuDesc[gpuIndex].pGpu->QueryInterface(IID_ARGS(&pRenderer->pDxActiveGPU)));
	ASSERT(pRenderer->pDxActiveGPU != NULL);
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
	CHECK_HRESULT(D3D12CreateDevice(pRenderer->pDxActiveGPU, gpuDesc[gpuIndex].mMaxSupportedFeatureLevel, IID_ARGS(&pRenderer->pDxDevice)));
#endif

#if defined(_WINDOWS) && defined(FORGE_DEBUG)
	HRESULT hr = pRenderer->pDxDevice->QueryInterface(IID_ARGS(&pRenderer->pDxDebugValidation));
	if (SUCCEEDED(hr))
	{
		pRenderer->pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		pRenderer->pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		// D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND breaks even when it is disabled
		pRenderer->pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
		hr = pRenderer->pDxDebugValidation->SetBreakOnID(D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND, false);
	}
#endif

	return true;
}

static void RemoveDevice(Renderer* pRenderer)
{
	SAFE_RELEASE(pRenderer->pDXGIFactory);
	SAFE_RELEASE(pRenderer->pDxActiveGPU);
#if defined(_WINDOWS)
	if (pRenderer->pDxDebugValidation)
	{
		pRenderer->pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, false);
		pRenderer->pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, false);
		pRenderer->pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
		SAFE_RELEASE(pRenderer->pDxDebugValidation);
	}
#endif

#if defined(XBOX)
	SAFE_RELEASE(pRenderer->pDxDevice);
#elif defined(ENABLE_GRAPHICS_DEBUG)
	ID3D12DebugDevice* pDebugDevice = NULL;
	pRenderer->pDxDevice->QueryInterface(&pDebugDevice);

	SAFE_RELEASE(pRenderer->pDXDebug);
	SAFE_RELEASE(pRenderer->pDxDevice);

	if (pDebugDevice)
	{
		// Debug device is released first so report live objects don't show its ref as a warning.
		pDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
		pDebugDevice->Release();
	}
#else
	SAFE_RELEASE(pRenderer->pDxDevice);
#endif
}
/************************************************************************/
// Renderer Init Remove
/************************************************************************/
void initRenderer(const char* appName, const RendererDesc* pDesc, Renderer** ppRenderer)
{
	ASSERT(appName);
	ASSERT(pDesc);
	ASSERT(ppRenderer);

	Renderer* pRenderer = (Renderer*)tf_calloc_memalign(1, alignof(Renderer), sizeof(Renderer));
	ASSERT(pRenderer);

	pRenderer->mGpuMode = pDesc->mGpuMode;
	pRenderer->mShaderTarget = pDesc->mShaderTarget;
	pRenderer->mEnableGpuBasedValidation = pDesc->mEnableGPUBasedValidation;
#if defined(XBOX)
	pRenderer->mApi = RENDERER_API_XBOX_D3D12;
#else
	pRenderer->mApi = RENDERER_API_D3D12;
#endif

	pRenderer->pName = (char*)tf_calloc(strlen(appName) + 1, sizeof(char));
	strcpy(pRenderer->pName, appName);

	// Initialize the D3D12 bits
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

		if (!AddDevice(pRenderer))
		{
			*ppRenderer = NULL;
			return;
		}

#if !defined(XBOX)
		//anything below LOW preset is not supported and we will exit
		if (pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel < GPU_PRESET_LOW)
		{
			//have the condition in the assert as well so its cleared when the assert message box appears

			ASSERT(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel >= GPU_PRESET_LOW);

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

		utils_caps_builder(pRenderer);

		if (pRenderer->mShaderTarget >= shader_target_6_0)
		{
			// Query the level of support of Shader Model.
			D3D12_FEATURE_DATA_SHADER_MODEL   shaderModelSupport = { D3D_SHADER_MODEL_6_0 };
			D3D12_FEATURE_DATA_D3D12_OPTIONS1 waveIntrinsicsSupport = {};
			if (!SUCCEEDED(pRenderer->pDxDevice->CheckFeatureSupport(
					(D3D12_FEATURE)D3D12_FEATURE_SHADER_MODEL, &shaderModelSupport, sizeof(shaderModelSupport))))
			{
				return;
			}
			// Query the level of support of Wave Intrinsics.
			if (!SUCCEEDED(pRenderer->pDxDevice->CheckFeatureSupport(
					(D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &waveIntrinsicsSupport, sizeof(waveIntrinsicsSupport))))
			{
				return;
			}

			// If the device doesn't support SM6 or Wave Intrinsics, try enabling the experimental feature for Shader Model 6 and creating the device again.
			if (shaderModelSupport.HighestShaderModel != D3D_SHADER_MODEL_6_0 || waveIntrinsicsSupport.WaveOps != TRUE)
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
						(waveIntrinsicsSupport.WaveOps != TRUE && !SUCCEEDED(EnableExperimentalShaderModels())))
					{
						RemoveDevice(pRenderer);
						LOGF(LogLevel::eERROR, "Hardware does not support Shader Model 6.0");
						return;
					}
				}
				else
				{
					LOGF( LogLevel::eWARNING,
						"\nRenderDoc does not support SM 6.0 or higher. Application might work but you won't be able to debug the SM 6.0+ "
						"shaders or view their bytecode.");
				}
			}
		}
#endif
		/************************************************************************/
		// Multi GPU - SLI Node Count
		/************************************************************************/
		uint32_t gpuCount = pRenderer->pDxDevice->GetNodeCount();
		pRenderer->mLinkedNodeCount = gpuCount;
		if (pRenderer->mLinkedNodeCount < 2)
			pRenderer->mGpuMode = GPU_MODE_SINGLE;
		/************************************************************************/
		// Descriptor heaps
		/************************************************************************/
		pRenderer->pCPUDescriptorHeaps = (DescriptorHeap**)tf_malloc(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES * sizeof(DescriptorHeap*));
		pRenderer->pCbvSrvUavHeaps = (DescriptorHeap**)tf_malloc(pRenderer->mLinkedNodeCount * sizeof(DescriptorHeap*));
		pRenderer->pSamplerHeaps = (DescriptorHeap**)tf_malloc(pRenderer->mLinkedNodeCount * sizeof(DescriptorHeap*));

		for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Flags = gCpuDescriptorHeapProperties[i].mFlags;
			desc.NodeMask = 0; // CPU Descriptor Heap - Node mask is irrelevant
			desc.NumDescriptors = gCpuDescriptorHeapProperties[i].mMaxDescriptors;
			desc.Type = (D3D12_DESCRIPTOR_HEAP_TYPE)i;
			add_descriptor_heap(pRenderer->pDxDevice, &desc, &pRenderer->pCPUDescriptorHeaps[i]);
		}

		// One shader visible heap for each linked node
		for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			desc.NodeMask = util_calculate_node_mask(pRenderer, i);

			desc.NumDescriptors = 1 << 16;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			add_descriptor_heap(pRenderer->pDxDevice, &desc, &pRenderer->pCbvSrvUavHeaps[i]);

			desc.NumDescriptors = 1 << 11;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			add_descriptor_heap(pRenderer->pDxDevice, &desc, &pRenderer->pSamplerHeaps[i]);
		}
		/************************************************************************/
		// Memory allocator
		/************************************************************************/
		D3D12MA::ALLOCATOR_DESC desc = {};
		desc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
		desc.pDevice = pRenderer->pDxDevice;
		desc.pAdapter = pRenderer->pDxActiveGPU;

		D3D12MA::ALLOCATION_CALLBACKS allocationCallbacks = {};
		allocationCallbacks.pAllocate = [](size_t size, size_t alignment, void*)
		{
			return tf_memalign(alignment, size);
		};
		allocationCallbacks.pFree = [](void* ptr, void*)
		{
			tf_free(ptr);
		};
		desc.pAllocationCallbacks = &allocationCallbacks;
		CHECK_HRESULT(D3D12MA::CreateAllocator(&desc, &pRenderer->pResourceAllocator));
	}
	/************************************************************************/
	/************************************************************************/
	add_default_resources(pRenderer);

	hook_post_init_renderer(pRenderer);

	// Set shader macro based on runtime information
	ShaderMacro rendererShaderDefines[] =
	{
		// Descriptor set indices
		{ "UPDATE_FREQ_NONE",      "space0" },
		{ "UPDATE_FREQ_PER_FRAME", "space1" },
		{ "UPDATE_FREQ_PER_BATCH", "space2" },
		{ "UPDATE_FREQ_PER_DRAW",  "space3" },
	};
	pRenderer->mBuiltinShaderDefinesCount = sizeof(rendererShaderDefines) / sizeof(rendererShaderDefines[0]);
	pRenderer->pBuiltinShaderDefines = (ShaderMacro*)tf_calloc(pRenderer->mBuiltinShaderDefinesCount, sizeof(ShaderMacro));
	for (uint32_t i = 0; i < pRenderer->mBuiltinShaderDefinesCount; ++i)
	{
		pRenderer->pBuiltinShaderDefines[i] = rendererShaderDefines[i];
	}

	// Renderer is good!
	*ppRenderer = pRenderer;
}

void removeRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);

	for (uint32_t i = 0; i < pRenderer->mBuiltinShaderDefinesCount; ++i)
		pRenderer->pBuiltinShaderDefines[i].~ShaderMacro();
	SAFE_FREE(pRenderer->pBuiltinShaderDefines);

	remove_default_resources(pRenderer);

	// Destroy the Direct3D12 bits
	for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		remove_descriptor_heap(pRenderer->pCPUDescriptorHeaps[i]);
	}

	for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
	{
		remove_descriptor_heap(pRenderer->pCbvSrvUavHeaps[i]);
		remove_descriptor_heap(pRenderer->pSamplerHeaps[i]);
	}

	SAFE_RELEASE(pRenderer->pResourceAllocator);

	RemoveDevice(pRenderer);

	hook_post_remove_renderer(pRenderer);

	nvapiExit();
	agsExit();

	// Free all the renderer components
	SAFE_FREE(pRenderer->pCPUDescriptorHeaps);
	SAFE_FREE(pRenderer->pCbvSrvUavHeaps);
	SAFE_FREE(pRenderer->pSamplerHeaps);
	SAFE_FREE(pRenderer->pCapBits);
	SAFE_FREE(pRenderer->pActiveGpuSettings);
	SAFE_FREE(pRenderer->pName);
	SAFE_FREE(pRenderer);
}
/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void addFence(Renderer* pRenderer, Fence** ppFence)
{
	//ASSERT that renderer is valid
	ASSERT(pRenderer);
	ASSERT(ppFence);

	//create a Fence and ASSERT that it is valid
	Fence* pFence = (Fence*)tf_calloc(1, sizeof(Fence));
	ASSERT(pFence);

	CHECK_HRESULT(pRenderer->pDxDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_ARGS(&pFence->pDxFence)));
	pFence->mFenceValue = 1;

	pFence->pDxWaitIdleFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	*ppFence = pFence;
}

void removeFence(Renderer* pRenderer, Fence* pFence)
{
	//ASSERT that renderer is valid
	ASSERT(pRenderer);
	//ASSERT that given fence to remove is valid
	ASSERT(pFence);

	SAFE_RELEASE(pFence->pDxFence);
	CloseHandle(pFence->pDxWaitIdleFenceEvent);

	SAFE_FREE(pFence);
}

void addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
	addFence(pRenderer, (Fence**)ppSemaphore);
}

void removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
	//ASSERT that renderer and given semaphore are valid
	ASSERT(pRenderer);
	ASSERT(pSemaphore);

	removeFence(pRenderer, (Fence*)pSemaphore);
}

void addQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppQueue);

	Queue* pQueue = (Queue*)tf_calloc(1, sizeof(Queue));
	ASSERT(pQueue);

	if (pDesc->mNodeIndex)
	{
		ASSERT(pRenderer->mGpuMode == GPU_MODE_LINKED && "Node Masking can only be used with Linked Multi GPU");
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	if (pDesc->mFlag & QUEUE_FLAG_DISABLE_GPU_TIMEOUT)
		queueDesc.Flags |= D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
	queueDesc.Type = gDx12CmdTypeTranslator[pDesc->mType];
	queueDesc.Priority = gDx12QueuePriorityTranslator[pDesc->mPriority];
	queueDesc.NodeMask = util_calculate_node_mask(pRenderer, pDesc->mNodeIndex);

	CHECK_HRESULT(hook_create_command_queue(pRenderer->pDxDevice, &queueDesc, &pQueue->pDxQueue));

	wchar_t queueTypeBuffer[32] = {};
	wchar_t* queueType = NULL;
	switch (queueDesc.Type)
	{
		case D3D12_COMMAND_LIST_TYPE_DIRECT: queueType = L"GRAPHICS QUEUE"; break;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE: queueType = L"COMPUTE QUEUE"; break;
		case D3D12_COMMAND_LIST_TYPE_COPY: queueType = L"COPY QUEUE"; break;
		default: break;
	}

	swprintf(queueTypeBuffer, L"%ls %u", queueType, pDesc->mNodeIndex);
	pQueue->pDxQueue->SetName(queueTypeBuffer);

	pQueue->mType = pDesc->mType;
	pQueue->mNodeIndex = pDesc->mNodeIndex;

	// Add queue fence. This fence will make sure we finish all GPU works before releasing the queue
	addFence(pRenderer, &pQueue->pFence);

	*ppQueue = pQueue;
}

void removeQueue(Renderer* pRenderer, Queue* pQueue)
{
	ASSERT(pQueue);

	// Make sure we finished all GPU works before we remove the queue
	waitQueueIdle(pQueue);

	removeFence(pRenderer, pQueue->pFence);
	
	SAFE_RELEASE(pQueue->pDxQueue);

	SAFE_FREE(pQueue);
}

void addCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
{
	//ASSERT that renderer is valid
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppCmdPool);

	//create one new CmdPool and add to renderer
	CmdPool* pCmdPool = (CmdPool*)tf_calloc(1, sizeof(CmdPool));
	ASSERT(pCmdPool);

	CHECK_HRESULT(pRenderer->pDxDevice->CreateCommandAllocator(
		gDx12CmdTypeTranslator[pDesc->pQueue->mType], IID_ARGS(&pCmdPool->pDxCmdAlloc)));

	pCmdPool->pQueue = pDesc->pQueue;

	*ppCmdPool = pCmdPool;
}

void removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	//check validity of given renderer and command pool
	ASSERT(pRenderer);
	ASSERT(pCmdPool);

	SAFE_RELEASE(pCmdPool->pDxCmdAlloc);
	SAFE_FREE(pCmdPool);
}

void addCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
{
	//verify that given pool is valid
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppCmd);

	// initialize to zero
	Cmd* pCmd = (Cmd*)tf_calloc_memalign(1, alignof(Cmd), sizeof(Cmd));
	ASSERT(pCmd);

	//set command pool of new command
	pCmd->mNodeIndex = pDesc->pPool->pQueue->mNodeIndex;
	pCmd->mType = pDesc->pPool->pQueue->mType;
	pCmd->pQueue = pDesc->pPool->pQueue;
	pCmd->pRenderer = pRenderer;

	pCmd->pBoundHeaps[0] = pRenderer->pCbvSrvUavHeaps[pCmd->mNodeIndex];
	pCmd->pBoundHeaps[1] = pRenderer->pSamplerHeaps[pCmd->mNodeIndex];

	pCmd->pCmdPool = pDesc->pPool;

	uint32_t nodeMask = util_calculate_node_mask(pRenderer, pCmd->mNodeIndex);

	if (QUEUE_TYPE_TRANSFER == pDesc->pPool->pQueue->mType)
	{
		CHECK_HRESULT(hook_create_copy_cmd(pRenderer->pDxDevice, nodeMask, pDesc->pPool->pDxCmdAlloc, pCmd));
	}
	else
	{
		ID3D12PipelineState* initialState = NULL;
		CHECK_HRESULT(pRenderer->pDxDevice->CreateCommandList(
			nodeMask, gDx12CmdTypeTranslator[pCmd->mType], pDesc->pPool->pDxCmdAlloc,
			initialState, __uuidof(pCmd->pDxCmdList), (void**)&(pCmd->pDxCmdList)));
	}

	// Command lists are addd in the recording state, but there is nothing
	// to record yet. The main loop expects it to be closed, so close it now.
	CHECK_HRESULT(pCmd->pDxCmdList->Close());

	*ppCmd = pCmd;
}

void removeCmd(Renderer* pRenderer, Cmd* pCmd)
{
	//verify that given command and pool are valid
	ASSERT(pRenderer);
	ASSERT(pCmd);

	if (QUEUE_TYPE_TRANSFER == pCmd->mType)
	{
		hook_remove_copy_cmd(pCmd);
	}
	else
	{
		SAFE_RELEASE(pCmd->pDxCmdList);
	}

	SAFE_FREE(pCmd);
}

void addCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmd)
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

void removeCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
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

void toggleVSync(Renderer* pRenderer, SwapChain** ppSwapChain)
{
	UNREF_PARAM(pRenderer);
	ASSERT(ppSwapChain);

	SwapChain* pSwapChain = *ppSwapChain;
	//set descriptor vsync boolean
	pSwapChain->mEnableVsync = !pSwapChain->mEnableVsync;
#if !defined(XBOX)
	if (!pSwapChain->mEnableVsync)
	{
		pSwapChain->mFlags |= DXGI_PRESENT_ALLOW_TEARING;
	}
	else
	{
		pSwapChain->mFlags &= ~DXGI_PRESENT_ALLOW_TEARING;
	}
#endif

	//toggle vsync present flag (this can go up to 4 but we don't need to refresh on nth vertical sync)
	pSwapChain->mDxSyncInterval = (pSwapChain->mDxSyncInterval + 1) % 2;
}

void addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
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
	pSwapChain->mDxSyncInterval = pDesc->mEnableVsync ? 1 : 0;

	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width = pDesc->mWidth;
	desc.Height = pDesc->mHeight;
	desc.Format = util_to_dx_swapchain_format(pDesc->mColorFormat);
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
	pRenderer->pDXGIFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
	desc.Flags |= allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	pSwapChain->mFlags |= (!pDesc->mEnableVsync && allowTearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;

	IDXGISwapChain1* swapchain;

	HWND hwnd = (HWND)pDesc->mWindowHandle.window;

	CHECK_HRESULT(pRenderer->pDXGIFactory->CreateSwapChainForHwnd(pDesc->ppPresentQueues[0]->pDxQueue, hwnd, &desc, NULL, NULL, &swapchain));

	CHECK_HRESULT(pRenderer->pDXGIFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

	CHECK_HRESULT(swapchain->QueryInterface(IID_ARGS(&pSwapChain->pDxSwapChain)));
	swapchain->Release();

	// Allowing multiple command queues to present for applications like Alternate Frame Rendering
	if (pRenderer->mGpuMode == GPU_MODE_LINKED && pDesc->mPresentQueueCount > 1)
	{
		IUnknown** ppQueues = (IUnknown**)alloca(pDesc->mPresentQueueCount * sizeof(IUnknown*));
		UINT*      pCreationMasks = (UINT*)alloca(pDesc->mPresentQueueCount * sizeof(UINT));
		for (uint32_t i = 0; i < pDesc->mPresentQueueCount; ++i)
		{
			ppQueues[i] = pDesc->ppPresentQueues[i]->pDxQueue;
			pCreationMasks[i] = (1 << pDesc->ppPresentQueues[i]->mNodeIndex);
		}

		pSwapChain->pDxSwapChain->ResizeBuffers1(
			desc.BufferCount, desc.Width, desc.Height, desc.Format, desc.Flags, pCreationMasks, ppQueues);
	}

	ID3D12Resource** buffers = (ID3D12Resource**)alloca(pDesc->mImageCount * sizeof(ID3D12Resource*));

	// Create rendertargets from swapchain
	for (uint32_t i = 0; i < pDesc->mImageCount; ++i)
	{
		CHECK_HRESULT(pSwapChain->pDxSwapChain->GetBuffer(i, IID_ARGS(&buffers[i])));
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
#if defined(XBOX)
	descColor.mFlags |= TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
	pSwapChain->pPresentQueue = pDesc->mPresentQueueCount ? pDesc->ppPresentQueues[0] : NULL;
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

void removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
#if defined(XBOX)
	hook_queue_present(pSwapChain->pPresentQueue, NULL, 0);
#endif

	for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
	{
		ID3D12Resource* resource = pSwapChain->ppRenderTargets[i]->pTexture->pDxResource;
		removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[i]);
#if !defined(XBOX)
		SAFE_RELEASE(resource);
#endif
	}

#if !defined(XBOX)
	SAFE_RELEASE(pSwapChain->pDxSwapChain);
#endif
	SAFE_FREE(pSwapChain);
}

void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
	//verify renderer validity
	ASSERT(pRenderer);
	//verify adding at least 1 buffer
	ASSERT(pDesc);
	ASSERT(ppBuffer);
	ASSERT(pDesc->mSize > 0);

	// initialize to zero
	Buffer* pBuffer = (Buffer*)tf_calloc_memalign(1, alignof(Buffer), sizeof(Buffer));
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
	pRenderer->pDxDevice->GetCopyableFootprints(&desc, 0, 1, 0, NULL, NULL, NULL, &padded_size);
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

	D3D12_RESOURCE_STATES res_states = util_to_dx_resource_state(start_state);

	D3D12MA::ALLOCATION_DESC alloc_desc = {};

	if (RESOURCE_MEMORY_USAGE_CPU_ONLY == pDesc->mMemoryUsage || RESOURCE_MEMORY_USAGE_CPU_TO_GPU == pDesc->mMemoryUsage)
		alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
	else if (RESOURCE_MEMORY_USAGE_GPU_TO_CPU == pDesc->mMemoryUsage)
		alloc_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
	else
		alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	if (pDesc->mFlags & BUFFER_CREATION_FLAG_OWN_MEMORY_BIT)
		alloc_desc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;

	// Multi GPU
	alloc_desc.CreationNodeMask = (1 << pDesc->mNodeIndex);
	alloc_desc.VisibleNodeMask = alloc_desc.CreationNodeMask;
	for (uint32_t i = 0; i < pDesc->mSharedNodeIndexCount; ++i)
		alloc_desc.VisibleNodeMask |= (1 << pDesc->pSharedNodeIndices[i]);

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
	if (SUCCEEDED(hook_create_special_resource(pRenderer, &desc, NULL, res_states, pDesc->mFlags, &pBuffer->pDxResource)))
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
		CHECK_HRESULT(pRenderer->pDxDevice->CreateCommittedResource(&heapProps, alloc_desc.ExtraHeapFlags, &desc, res_states, NULL, IID_ARGS(&pBuffer->pDxResource)));
	}
	else
	{
		CHECK_HRESULT(pRenderer->pResourceAllocator->CreateResource(&alloc_desc, &desc, res_states, NULL,
			&pBuffer->pDxAllocation, IID_ARGS(&pBuffer->pDxResource)));

		// Set name
#if defined(ENABLE_GRAPHICS_DEBUG)
		pBuffer->pDxAllocation->SetName(debugName);
#endif
	}

	if (pDesc->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && pDesc->mFlags & BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT)
		pBuffer->pDxResource->Map(0, NULL, &pBuffer->pCpuMappedAddress);

	pBuffer->mDxGpuAddress = pBuffer->pDxResource->GetGPUVirtualAddress();
#if defined(XBOX)
	pBuffer->pCpuMappedAddress = (void*)pBuffer->mDxGpuAddress;
#endif

	if (!(pDesc->mFlags & BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION))
	{
		DescriptorHeap* pHeap = pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
		uint32_t handleCount =
			((pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER) ? 1 : 0) +
			((pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER) ? 1 : 0) +
			((pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER) ? 1 : 0);
		pBuffer->mDxDescriptorHandles = consume_descriptor_handles(
			pHeap,
			handleCount).mCpu;

		if (pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE cbv = { pBuffer->mDxDescriptorHandles.ptr };
			pBuffer->mDxSrvOffset = pHeap->mDescriptorSize * 1;

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = pBuffer->mDxGpuAddress;
			cbvDesc.SizeInBytes = (UINT)allocationSize;
			add_cbv(pRenderer, &cbvDesc, &cbv);
		}

		if (pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE srv = { pBuffer->mDxDescriptorHandles.ptr + pBuffer->mDxSrvOffset };
			pBuffer->mDxUavOffset = pBuffer->mDxSrvOffset + pHeap->mDescriptorSize * 1;

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

			add_srv(pRenderer, pBuffer->pDxResource, &srvDesc, &srv);
		}

		if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE uav = { pBuffer->mDxDescriptorHandles.ptr + pBuffer->mDxUavOffset };

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
				D3D12_FEATURE_DATA_FORMAT_SUPPORT FormatSupport = { uavDesc.Format, D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE };
				HRESULT hr = pRenderer->pDxDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &FormatSupport, sizeof(FormatSupport));
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

			ID3D12Resource* pCounterResource = pDesc->pCounterBuffer ? pDesc->pCounterBuffer->pDxResource : NULL;
			add_uav(pRenderer, pBuffer->pDxResource, pCounterResource, &uavDesc, &uav);
		}
	}

#if defined(ENABLE_GRAPHICS_DEBUG)
	if (pDesc->pName)
	{
		// Set name
		pBuffer->pDxResource->SetName(debugName);
	}
#endif

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

	if (pBuffer->mDxDescriptorHandles.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
	{
		uint32_t handleCount =
			((pBuffer->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER) ? 1 : 0) +
			((pBuffer->mDescriptors & DESCRIPTOR_TYPE_BUFFER) ? 1 : 0) +
			((pBuffer->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER) ? 1 : 0);
		return_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
			pBuffer->mDxDescriptorHandles, handleCount);
	}

	SAFE_RELEASE(pBuffer->pDxAllocation);
	SAFE_RELEASE(pBuffer->pDxResource);

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

	CHECK_HRESULT(pBuffer->pDxResource->Map(0, &range, &pBuffer->pCpuMappedAddress));
}

void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	UNREF_PARAM(pRenderer);
	ASSERT(pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to unmap non-cpu accessible resource");

	pBuffer->pDxResource->Unmap(0, NULL);
	pBuffer->pCpuMappedAddress = NULL;
}

void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mWidth && pDesc->mHeight && (pDesc->mDepth || pDesc->mArraySize));
	if (pDesc->mSampleCount > SAMPLE_COUNT_1 && pDesc->mMipLevels > 1)
	{
		LOGF(LogLevel::eERROR, "Multi-Sampled textures cannot have mip maps");
		ASSERT(false);
		return;
	}

	//allocate new texture
	Texture* pTexture = (Texture*)tf_calloc_memalign(1, alignof(Texture), sizeof(Texture));
	ASSERT(pTexture);

	if (pDesc->pNativeHandle)
	{
		pTexture->mOwnsImage = false;
		pTexture->pDxResource = (ID3D12Resource*)pDesc->pNativeHandle;
	}
	else
	{
		pTexture->mOwnsImage = true;
	}


	//add to gpu
	D3D12_RESOURCE_DESC desc = {};

	DXGI_FORMAT dxFormat = (DXGI_FORMAT) TinyImageFormat_ToDXGI_FORMAT(pDesc->mFormat);

	DescriptorType      descriptors = pDesc->mDescriptors;

#if defined(ENABLE_GRAPHICS_DEBUG)
	wchar_t debugName[MAX_DEBUG_NAME_LENGTH] = {};
	if (pDesc->pName)
	{
		mbstowcs(debugName, pDesc->pName, MAX_DEBUG_NAME_LENGTH);
	}
#endif

	ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

	if (NULL == pTexture->pDxResource)
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
		desc.Format = (DXGI_FORMAT) TinyImageFormat_DXGI_FORMATToTypeless((TinyImageFormat_DXGI_FORMAT)dxFormat);
		desc.SampleDesc.Count = (UINT)pDesc->mSampleCount;
		desc.SampleDesc.Quality = (UINT)pDesc->mSampleQuality;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS data;
		data.Format = desc.Format;
		data.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
		data.SampleCount = desc.SampleDesc.Count;
		pRenderer->pDxDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(data));
		while (data.NumQualityLevels == 0 && data.SampleCount > 0)
		{
			LOGF(
				LogLevel::eWARNING, "Sample Count (%u) not supported. Trying a lower sample count (%u)", data.SampleCount,
				data.SampleCount / 2);
			data.SampleCount = desc.SampleDesc.Count / 2;
			pRenderer->pDxDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(data));
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
			actualStartState = (pDesc->mStartState > RESOURCE_STATE_RENDER_TARGET) ?
				(pDesc->mStartState & (ResourceState)~RESOURCE_STATE_RENDER_TARGET) :
				RESOURCE_STATE_RENDER_TARGET;

		}
		else if (pDesc->mStartState & RESOURCE_STATE_DEPTH_WRITE)
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			actualStartState = (pDesc->mStartState > RESOURCE_STATE_DEPTH_WRITE) ?
				(pDesc->mStartState & (ResourceState)~RESOURCE_STATE_DEPTH_WRITE) :
				RESOURCE_STATE_DEPTH_WRITE;
		}

		// Decide sharing flags
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT)
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
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
		D3D12_RESOURCE_STATES res_states = util_to_dx_resource_state(actualStartState);

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
		alloc_desc.CreationNodeMask = (1 << pDesc->mNodeIndex);
		alloc_desc.VisibleNodeMask = alloc_desc.CreationNodeMask;
		for (uint32_t i = 0; i < pDesc->mSharedNodeIndexCount; ++i)
			alloc_desc.VisibleNodeMask |= (1 << pDesc->pSharedNodeIndices[i]);

		// Create resource
		if (SUCCEEDED(hook_create_special_resource(pRenderer, &desc, pClearValue, res_states, pDesc->mFlags, &pTexture->pDxResource)))
		{
			LOGF(LogLevel::eINFO, "Allocated memory in special platform-specific RAM");
		}
		else
		{
			CHECK_HRESULT(pRenderer->pResourceAllocator->CreateResource(&alloc_desc, &desc, res_states, pClearValue,
				&pTexture->pDxAllocation, IID_ARGS(&pTexture->pDxResource)));
#if defined(ENABLE_GRAPHICS_DEBUG)
			// Set name
			pTexture->pDxAllocation->SetName(debugName);
#endif
		}
	}
	else
	{
		desc = pTexture->pDxResource->GetDesc();
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

	DescriptorHeap* pHeap = pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
	uint32_t handleCount = (descriptors & DESCRIPTOR_TYPE_TEXTURE) ? 1 : 0;
	handleCount += (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE) ? pDesc->mMipLevels : 0;
	pTexture->mDxDescriptorHandles = consume_descriptor_handles(pHeap, handleCount).mCpu;

	if (descriptors & DESCRIPTOR_TYPE_TEXTURE)
	{
		ASSERT(srvDesc.ViewDimension != D3D12_SRV_DIMENSION_UNKNOWN);

		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = util_to_dx_srv_format(dxFormat);
		add_srv(pRenderer, pTexture->pDxResource, &srvDesc, &pTexture->mDxDescriptorHandles);
		++pTexture->mUavStartIndex;
	}

	if (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
	{
		pTexture->mDescriptorSize = pHeap->mDescriptorSize;
		uavDesc.Format = util_to_dx_uav_format(dxFormat);
		for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE handle = { pTexture->mDxDescriptorHandles.ptr + (i + pTexture->mUavStartIndex) * pTexture->mDescriptorSize };

			uavDesc.Texture1DArray.MipSlice = i;
			if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
				uavDesc.Texture3D.WSize = desc.DepthOrArraySize / (UINT)pow(2, i);
			add_uav(pRenderer, pTexture->pDxResource, NULL, &uavDesc, &handle);
		}
	}

#if defined(ENABLE_GRAPHICS_DEBUG)
	if (pDesc->pName)
	{
		// Set name
		pTexture->pDxResource->SetName(debugName);
	}
#endif

	pTexture->mNodeIndex = pDesc->mNodeIndex;
	pTexture->mHandleCount = handleCount;
	pTexture->mMipLevels = pDesc->mMipLevels;
	pTexture->mWidth = pDesc->mWidth;
	pTexture->mHeight = pDesc->mHeight;
	pTexture->mDepth = pDesc->mDepth;
	pTexture->mUav = pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE;
	pTexture->mFormat = pDesc->mFormat;
	pTexture->mArraySizeMinusOne = pDesc->mArraySize - 1;

	// TODO: Handle host visible textures in a better way
	if (pDesc->mHostVisible)
	{
		internal_log(
			LOG_TYPE_WARN,
			"D3D12 does not support host visible textures, memory of resulting texture will not be mapped for CPU visibility",
			"addTexture");
	}

	*ppTexture = pTexture;
}

void removeTexture(Renderer* pRenderer, Texture* pTexture)
{
	ASSERT(pRenderer);
	ASSERT(pTexture);

	//delete texture descriptors
	if (pTexture->mDxDescriptorHandles.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		return_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
			pTexture->mDxDescriptorHandles, pTexture->mHandleCount);

	if (pTexture->mOwnsImage)
	{
		SAFE_RELEASE(pTexture->pDxAllocation);
		SAFE_RELEASE(pTexture->pDxResource);
	}

	if (pTexture->pSvt)
	{
		void removeVirtualTexture(Renderer* pRenderer, VirtualTexture* pSvt);
		removeVirtualTexture(pRenderer, pTexture->pSvt);
	}

	SAFE_FREE(pTexture);
}

void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppRenderTarget);

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

	D3D12_RESOURCE_DESC desc = pRenderTarget->pTexture->pDxResource->GetDesc();

	uint32_t handleCount = desc.MipLevels;
	if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		handleCount *= desc.DepthOrArraySize;

	DescriptorHeap* pHeap = isDepth ?
		pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] :
		pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
	pRenderTarget->mDxDescriptors = consume_descriptor_handles(pHeap, 1 + handleCount).mCpu;
	pRenderTarget->mDxDescriptorSize = pHeap->mDescriptorSize;

	if(isDepth)
		add_dsv(pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, 0, -1, &pRenderTarget->mDxDescriptors);
	else
		add_rtv(pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, 0, -1, &pRenderTarget->mDxDescriptors);

	for (uint32_t i = 0; i < desc.MipLevels; ++i)
	{
		if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
			(pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		{
			for (uint32_t j = 0; j < desc.DepthOrArraySize; ++j)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE handle = {
					pRenderTarget->mDxDescriptors.ptr +
					(1 + i * desc.DepthOrArraySize + j) * pRenderTarget->mDxDescriptorSize };

				if(isDepth)
					add_dsv(pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, i, j, &handle);
				else
					add_rtv(pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, i, j, &handle);
			}
		}
		else
		{
			D3D12_CPU_DESCRIPTOR_HANDLE handle = { pRenderTarget->mDxDescriptors.ptr + (1 + i) * pRenderTarget->mDxDescriptorSize };

			if(isDepth)
				add_dsv(pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, i, -1, &handle);
			else
				add_rtv(pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, i, -1, &handle);
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

void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
	bool const isDepth = TinyImageFormat_HasDepth(pRenderTarget->mFormat);

	removeTexture(pRenderer, pRenderTarget->pTexture);

	const uint32_t depthOrArraySize = (uint32_t)(pRenderTarget->mArraySize * pRenderTarget->mDepth);
	uint32_t handleCount = pRenderTarget->mMipLevels;
	if ((pRenderTarget->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pRenderTarget->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		handleCount *= depthOrArraySize;

	!isDepth ?
		return_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV], pRenderTarget->mDxDescriptors, handleCount) :
		return_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV], pRenderTarget->mDxDescriptors, handleCount);

	SAFE_FREE(pRenderTarget);
}

void addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDxDevice);
	ASSERT(ppSampler);
	ASSERT(pDesc->mCompareFunc < MAX_COMPARE_MODES);

	// initialize to zero
	Sampler* pSampler = (Sampler*)tf_calloc_memalign(1, alignof(Sampler), sizeof(Sampler));
	ASSERT(pSampler);

	D3D12_SAMPLER_DESC desc = {};
	//add sampler to gpu
	desc.Filter = util_to_dx_filter(
		pDesc->mMinFilter, pDesc->mMagFilter, pDesc->mMipMapMode, pDesc->mMaxAnisotropy > 0.0f,
		(pDesc->mCompareFunc != CMP_NEVER ? true : false));
	desc.AddressU = util_to_dx_texture_address_mode(pDesc->mAddressU);
	desc.AddressV = util_to_dx_texture_address_mode(pDesc->mAddressV);
	desc.AddressW = util_to_dx_texture_address_mode(pDesc->mAddressW);
	desc.MipLODBias = pDesc->mMipLodBias;
	desc.MaxAnisotropy = max((UINT)pDesc->mMaxAnisotropy, 1U);
	desc.ComparisonFunc = gDx12ComparisonFuncTranslator[pDesc->mCompareFunc];
	desc.BorderColor[0] = 0.0f;
	desc.BorderColor[1] = 0.0f;
	desc.BorderColor[2] = 0.0f;
	desc.BorderColor[3] = 0.0f;
	desc.MinLOD = 0.0f;
	desc.MaxLOD = ((pDesc->mMipMapMode == MIPMAP_MODE_LINEAR) ? D3D12_FLOAT32_MAX : 0.0f);;
	pSampler->mDxDesc = desc;
	add_sampler(pRenderer, &pSampler->mDxDesc, &pSampler->mDxHandle);

	*ppSampler = pSampler;
}

void removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
	ASSERT(pRenderer);
	ASSERT(pSampler);

	//remove_sampler(pRenderer, &pSampler->mDxSamplerHandle);

	// Nop op
	return_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER], pSampler->mDxHandle, 1);

	SAFE_FREE(pSampler);
}
/************************************************************************/
// Shader Functions
/************************************************************************/
void compileShader(
	Renderer* pRenderer,
	ShaderTarget shaderTarget, ShaderStage stage, const char* fileName,
	uint32_t codeSize, const char* code,
	bool enablePrimitiveId,
	uint32_t macroCount, ShaderMacro* pMacros,
	BinaryShaderStageDesc* pOut, const char* pEntryPoint)
{
	if ((uint32_t)shaderTarget > pRenderer->mShaderTarget)
	{
		LOGF( LogLevel::eERROR,
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
#ifdef ENABLE_RAYTRACING
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
			pCurrent[len] = L'\0';
			macros[j + 1].Name = pCurrent;
			pCurrent += (len + 1);

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
		char				 pathStr[FS_MAX_PATH] = { 0 };
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

		uint32_t numCompileFlags = 0;
		char directoryStr[FS_MAX_PATH] = { 0 };
		fsGetParentPath(pathStr, directoryStr);
		eastl::vector<const wchar_t*> compilerArgs;
		hook_modify_shader_compile_flags(stage, enablePrimitiveId, NULL, &numCompileFlags);
		compilerArgs.resize(compilerArgs.size() + numCompileFlags);
		hook_modify_shader_compile_flags(stage, enablePrimitiveId, compilerArgs.end() - numCompileFlags, &numCompileFlags);

		compilerArgs.push_back(L"-Zi");
		compilerArgs.push_back(L"-all_resources_bound");
#if defined(ENABLE_GRAPHICS_DEBUG)
		compilerArgs.push_back(L"-Od");
#else
		compilerArgs.push_back(L"-O3");
#endif

		// specify the parent directory as include path
		wchar_t directory[FS_MAX_PATH + 2] = L"-I";
		mbstowcs(directory + 2, directoryStr, strlen(directoryStr));
		compilerArgs.push_back(directory);

		CHECK_HRESULT(pCompiler->Compile(
			pTextBlob, filename, entryName, target, compilerArgs.data(), (UINT32)compilerArgs.size(), macros, macroCount + 1, pInclude,
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

void addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
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
			case SHADER_STAGE_COMP: pStage = &pDesc->mComp; break;
#ifdef ENABLE_RAYTRACING
			case SHADER_STAGE_RAYTRACING: pStage = &pDesc->mComp; break;
#endif
			}

			totalSize += sizeof(ID3DBlob*);
			totalSize += sizeof(LPCWSTR);
			totalSize += (strlen(pStage->pEntryPoint) + 1) * sizeof(WCHAR);
			++reflectionCount;
		}
	}

	Shader* pShaderProgram = (Shader*)tf_calloc(1, totalSize);
	ASSERT(pShaderProgram);

	pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1);
	pShaderProgram->pShaderBlobs = (IDxcBlobEncoding**)(pShaderProgram->pReflection + 1);
	pShaderProgram->pEntryNames = (LPCWSTR*)(pShaderProgram->pShaderBlobs + reflectionCount);
	pShaderProgram->mStages = pDesc->mStages;

	uint8_t* mem = (uint8_t*)(pShaderProgram->pEntryNames + reflectionCount);

	reflectionCount = 0;

	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		ShaderStage                  stage_mask = (ShaderStage)(1 << i);
		const BinaryShaderStageDesc* pStage = NULL;
		if (stage_mask == (pShaderProgram->mStages & stage_mask))
		{
			switch (stage_mask)
			{
				case SHADER_STAGE_VERT: { pStage = &pDesc->mVert;
				}
				break;
				case SHADER_STAGE_HULL: { pStage = &pDesc->mHull;
				}
				break;
				case SHADER_STAGE_DOMN: { pStage = &pDesc->mDomain;
				}
				break;
				case SHADER_STAGE_GEOM: { pStage = &pDesc->mGeom;
				}
				break;
				case SHADER_STAGE_FRAG: { pStage = &pDesc->mFrag;
				}
				break;
				case SHADER_STAGE_COMP: { pStage = &pDesc->mComp;
#ifdef ENABLE_RAYTRACING
				case SHADER_STAGE_RAYTRACING: { pStage = &pDesc->mComp;
				}
				break;
#endif
				}
			}

			IDxcUtils*  pUtils;
			CHECK_HRESULT(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));
			pUtils->CreateBlob(pStage->pByteCode, pStage->mByteCodeSize, DXC_CP_ACP, &pShaderProgram->pShaderBlobs[reflectionCount]);
			pUtils->Release();

			d3d12_createShaderReflection(
				(uint8_t*)(pShaderProgram->pShaderBlobs[reflectionCount]->GetBufferPointer()),
				(uint32_t)pShaderProgram->pShaderBlobs[reflectionCount]->GetBufferSize(), stage_mask,
				&pShaderProgram->pReflection->mStageReflections[reflectionCount]);

			WCHAR* entryPointName = (WCHAR*)mem;
			mbstowcs((WCHAR*)entryPointName, pStage->pEntryPoint, strlen(pStage->pEntryPoint));
			pShaderProgram->pEntryNames[reflectionCount] = entryPointName;
			mem += (strlen(pStage->pEntryPoint) + 1) * sizeof(WCHAR);

			reflectionCount++;
		}
	}

	createPipelineReflection(pShaderProgram->pReflection->mStageReflections, reflectionCount, pShaderProgram->pReflection);

	*ppShaderProgram = pShaderProgram;
}

void removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
	UNREF_PARAM(pRenderer);

	//remove given shader
	for (uint32_t i = 0; i < pShaderProgram->pReflection->mStageReflectionCount; ++i)
	{
		SAFE_RELEASE(pShaderProgram->pShaderBlobs[i]);
	}
	destroyPipelineReflection(pShaderProgram->pReflection);

	SAFE_FREE(pShaderProgram);
}
/************************************************************************/
// Root Signature Functions
/************************************************************************/
void addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
{
	ASSERT(pRenderer->pActiveGpuSettings->mMaxRootSignatureDWORDS > 0);
	ASSERT(ppRootSignature);

	static constexpr uint32_t                              kMaxLayoutCount = DESCRIPTOR_UPDATE_FREQ_COUNT;
	UpdateFrequencyLayoutInfo                              layouts[kMaxLayoutCount] = {};
	eastl::vector<ShaderResource>                          shaderResources;
	eastl::vector<uint32_t>                                constantSizes;
	eastl::vector<eastl::pair<ShaderResource*, Sampler*> > staticSamplers;
	ShaderStage                                            shaderStages = SHADER_STAGE_NONE;
	bool                                                   useInputLayout = false;
	eastl::string_hash_map<Sampler*>                       staticSamplerMap;
	PipelineType                                           pipelineType = PIPELINE_TYPE_UNDEFINED;
	DescriptorIndexMap                                     indexMap;

	for (uint32_t i = 0; i < pRootSignatureDesc->mStaticSamplerCount; ++i)
	{
		staticSamplerMap.insert(pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i]);
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
#ifdef ENABLE_RAYTRACING
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
			uint32_t              setIndex = pRes->set;

			// If the size of the resource is zero, assume its a bindless resource
			// All bindless resources will go in the static descriptor table
			if (pRes->size == 0)
				setIndex = 0;

			// Find all unique resources
			decltype(indexMap.mMap)::iterator it =
				indexMap.mMap.find(pRes->name);
			if (it == indexMap.mMap.end())
			{
				decltype(shaderResources)::iterator it = eastl::find(shaderResources.begin(), shaderResources.end(), *pRes,
					[](const ShaderResource& a, const ShaderResource& b)
				{
					// HLSL - Every type has different register type unlike Vulkan where all registers are shared by all types
					return (a.type == b.type) && (a.used_stages == b.used_stages) && (((a.reg ^ b.reg) | (a.set ^ b.set)) == 0);
				});
				if (it == shaderResources.end())
				{
					indexMap.mMap.insert(pRes->name, (uint32_t)shaderResources.size());

					shaderResources.push_back(*pRes);

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
					constantSizes.push_back(constantSize);
				}
				else
				{
					ASSERT(pRes->type == it->type);
					if (pRes->type != it->type)
					{
						LOGF(LogLevel::eERROR,
							"\nFailed to create root signature\n"
							"Shared shader resources %s and %s have mismatching types (%u) and (%u). All shader resources "
							"sharing the same register and space addRootSignature "
							"must have the same type",
							pRes->name, it->name, (uint32_t)pRes->type, (uint32_t)it->type);
						return;
					}

					indexMap.mMap.insert(pRes->name,
						indexMap.mMap[it->name]);

					it->used_stages |= pRes->used_stages;
				}
			}
			// If the resource was already collected, just update the shader stage mask in case it is used in a different
			// shader stage in this case
			else
			{
				if (shaderResources[it->second].reg != pRes->reg)
				{
					LOGF( LogLevel::eERROR,
						"\nFailed to create root signature\n"
						"Shared shader resource %s has mismatching register. All shader resources "
						"shared by multiple shaders specified in addRootSignature "
						"have the same register and space",
						pRes->name);
					return;
				}
				if (shaderResources[it->second].set != pRes->set)
				{
					LOGF( LogLevel::eERROR,
						"\nFailed to create root signature\n"
						"Shared shader resource %s has mismatching space. All shader resources "
						"shared by multiple shaders specified in addRootSignature "
						"have the same register and space",
						pRes->name);
					return;
				}

				for (ShaderResource& res : shaderResources)
				{
					if (strcmp(res.name, it->first) == 0)
					{
						res.used_stages |= pRes->used_stages;
						break;
					}
				}
			}
		}
	}

	size_t totalSize = sizeof(RootSignature);
	totalSize += shaderResources.size() * sizeof(DescriptorInfo);
	totalSize += sizeof(DescriptorIndexMap);

	RootSignature* pRootSignature = (RootSignature*)tf_calloc_memalign(1, alignof(RootSignature), totalSize);
	ASSERT(pRootSignature);

	if ((uint32_t)shaderResources.size())
	{
		pRootSignature->mDescriptorCount = (uint32_t)shaderResources.size();
	}

	pRootSignature->pDescriptors = (DescriptorInfo*)(pRootSignature + 1);
	pRootSignature->pDescriptorNameToIndexMap = (DescriptorIndexMap*)(pRootSignature->pDescriptors + pRootSignature->mDescriptorCount);
	ASSERT(pRootSignature->pDescriptorNameToIndexMap);
	tf_placement_new<DescriptorIndexMap>(pRootSignature->pDescriptorNameToIndexMap);

	pRootSignature->mPipelineType = pipelineType;
	pRootSignature->pDescriptorNameToIndexMap->mMap = indexMap.mMap;

	// Fill the descriptor array to be stored in the root signature
	for (uint32_t i = 0; i < (uint32_t)shaderResources.size(); ++i)
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
			const decltype(staticSamplerMap)::iterator pNode = staticSamplerMap.find(pDesc->pName);

			if (pNode != staticSamplerMap.end())
			{
				LOGF(LogLevel::eINFO, "Descriptor (%s) : User specified Static Sampler", pDesc->pName);
				// Set the index to invalid value so we can use this later for error checking if user tries to update a static sampler
				pDesc->mIndexInParent = ~0u;
				staticSamplers.push_back({ pRes, pNode->second });
			}
			else
			{
				// In D3D12, sampler descriptors cannot be placed in a table containing view descriptors
				layouts[setIndex].mSamplerTable.push_back({ *pRes, pDesc });
			}
		}
		// No support for arrays of constant buffers to be used as root descriptors as this might bloat the root signature size
		else if (pDesc->mType == DESCRIPTOR_TYPE_UNIFORM_BUFFER && pDesc->mSize == 1)
		{
			// D3D12 has no special syntax to declare root constants like Vulkan
			// So we assume that all constant buffers with the word "rootconstant" (case insensitive) are root constants
			eastl::string name = pRes->name;
			name.make_lower();
			if (name.find("rootconstant", 0) != eastl::string::npos)
			{
				// Make the root param a 32 bit constant if the user explicitly specifies it in the shader
				pDesc->mRootDescriptor = 1;
				pDesc->mType = DESCRIPTOR_TYPE_ROOT_CONSTANT;
				layouts[setIndex].mRootConstants.push_back({ *pRes, pDesc });

				pDesc->mSize = constantSizes[i] / sizeof(uint32_t);
			}
			// If a user specified a uniform buffer to be used directly in the root signature change its type to D3D12_ROOT_PARAMETER_TYPE_CBV
			// Also log a message for debugging purpose
			else if (name.find("rootcbv", 0) != eastl::string::npos)
			{
				layouts[setIndex].mRootDescriptorParams.push_back({ *pRes, pDesc });
				pDesc->mRootDescriptor = 1;

				LOGF(LogLevel::eINFO, "Descriptor (%s) : User specified D3D12_ROOT_PARAMETER_TYPE_CBV", pDesc->pName);
			}
			else
			{
				layouts[setIndex].mCbvSrvUavTable.push_back({ *pRes, pDesc });
			}
		}
		else
		{
			layouts[setIndex].mCbvSrvUavTable.push_back({ *pRes, pDesc });
		}

		layouts[setIndex].mDescriptorIndexMap[pDesc] = i;
	}

	// We should never reach inside this if statement. If we do, something got messed up
	if (pRenderer->pActiveGpuSettings->mMaxRootSignatureDWORDS < calculate_root_signature_size(layouts, kMaxLayoutCount))
	{
		LOGF(LogLevel::eWARNING, "Root Signature size greater than the specified max size");
		ASSERT(false);
	}

	// D3D12 currently has two versions of root signatures (1_0, 1_1)
	// So we fill the structs of both versions and in the end use the structs compatible with the supported version
	D3D12_DESCRIPTOR_RANGE1*   cbvSrvUavRange[DESCRIPTOR_UPDATE_FREQ_COUNT] = {};
	D3D12_DESCRIPTOR_RANGE1*   samplerRange[DESCRIPTOR_UPDATE_FREQ_COUNT] = {};
	D3D12_ROOT_PARAMETER1*     rootParams = NULL;
	uint32_t                   rootParamCount = 0;
	D3D12_STATIC_SAMPLER_DESC* staticSamplerDescs = NULL;
	uint32_t                   staticSamplerCount = (uint32_t)staticSamplers.size();

	if (staticSamplerCount)
	{
		staticSamplerDescs = (D3D12_STATIC_SAMPLER_DESC*)alloca(staticSamplerCount * sizeof(D3D12_STATIC_SAMPLER_DESC));

		for (uint32_t i = 0; i < staticSamplerCount; ++i)
		{
			D3D12_SAMPLER_DESC& desc = staticSamplers[i].second->mDxDesc;
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
			staticSamplerDescs[i].RegisterSpace = staticSamplers[i].first->set;
			staticSamplerDescs[i].ShaderRegister = staticSamplers[i].first->reg;
			staticSamplerDescs[i].ShaderVisibility = util_to_dx_shader_visibility(staticSamplers[i].first->used_stages);
		}
	}

	for (uint32_t i = 0; i < kMaxLayoutCount; ++i)
	{
		if (layouts[i].mCbvSrvUavTable.size())
		{
			cbvSrvUavRange[i] = (D3D12_DESCRIPTOR_RANGE1*)alloca(layouts[i].mCbvSrvUavTable.size() * sizeof(D3D12_DESCRIPTOR_RANGE1));
			++rootParamCount;
		}
		if (layouts[i].mSamplerTable.size())
		{
			samplerRange[i] = (D3D12_DESCRIPTOR_RANGE1*)alloca(layouts[i].mSamplerTable.size() * sizeof(D3D12_DESCRIPTOR_RANGE1));
			++rootParamCount;
		}
	}

	pRootSignature->mDescriptorCount = (uint32_t)shaderResources.size();

	for (uint32_t i = 0; i < kMaxLayoutCount; ++i)
	{
		pRootSignature->mDxRootConstantCount += (uint32_t)layouts[i].mRootConstants.size();
		pRootSignature->mDxRootDescriptorCounts[i] += (uint32_t)layouts[i].mRootDescriptorParams.size();
		rootParamCount += pRootSignature->mDxRootConstantCount;
		rootParamCount += (uint32_t)layouts[i].mRootDescriptorParams.size();
	}

	rootParams = (D3D12_ROOT_PARAMETER1*)alloca(rootParamCount * sizeof(D3D12_ROOT_PARAMETER1));
	rootParamCount = 0;

	// Start collecting root parameters
	// Start with root descriptors since they will be the most frequently updated descriptors
	// This also makes sure that if we spill, the root descriptors in the front of the root signature will most likely still remain in the root
	// Collect all root descriptors
	// Put most frequently changed params first
	for (uint32_t i = kMaxLayoutCount; i-- > 0U;)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[i];
		if (layout.mRootDescriptorParams.size())
		{
			ASSERT(1 == layout.mRootDescriptorParams.size());

			uint32_t rootDescriptorIndex = 0;

			for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mRootDescriptorParams.size(); ++descIndex)
			{
				RootParameter* pDesc = &layout.mRootDescriptorParams[descIndex];
				pDesc->second->mIndexInParent = rootDescriptorIndex;
				pRootSignature->mDxRootDescriptorRootIndices[i] = rootParamCount;

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

		if (!layout.mRootConstants.size())
			continue;

		for (uint32_t i = 0; i < (uint32_t)layouts[setIndex].mRootConstants.size(); ++i)
		{
			RootParameter* pDesc = &layout.mRootConstants[i];
			pDesc->second->mIndexInParent = rootConstantIndex;
			pRootSignature->mDxRootConstantRootIndices[pDesc->second->mIndexInParent] = rootParamCount;

			D3D12_ROOT_PARAMETER1 rootParam;
			create_root_constant(pDesc, &rootParam);

			rootParams[rootParamCount++] = rootParam;

			if (pDesc->second->mSize > gMaxRootConstantsPerRootParam)
			{
				//64 DWORDS for NVIDIA, 16 for AMD but 3 are used by driver so we get 13 SGPR
				//DirectX12
				//Root descriptors - 2
				//Root constants - Number of 32 bit constants
				//Descriptor tables - 1
				//Static samplers - 0
				LOGF(
					LogLevel::eINFO,
					"Root constant (%s) has (%u) 32 bit values. It is recommended to have root constant number less or equal than 13",
					pDesc->second->pName, pDesc->second->mSize);
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
		if (layout.mCbvSrvUavTable.size())
		{
			// sort table by type (CBV/SRV/UAV) by register by space
			eastl::stable_sort(
				layout.mCbvSrvUavTable.begin(), layout.mCbvSrvUavTable.end(),
				[](RootParameter const& lhs, RootParameter const& rhs) { return lhs.first.reg > rhs.first.reg; });
			eastl::stable_sort(
				layout.mCbvSrvUavTable.begin(), layout.mCbvSrvUavTable.end(),
				[](RootParameter const& lhs, RootParameter const& rhs) { return lhs.first.set > rhs.first.set; });
			eastl::stable_sort(
				layout.mCbvSrvUavTable.begin(), layout.mCbvSrvUavTable.end(),
				[](RootParameter const& lhs, RootParameter const& rhs) { return lhs.second->mType > rhs.second->mType; });

			D3D12_ROOT_PARAMETER1 rootParam;
			create_descriptor_table(
				(uint32_t)layout.mCbvSrvUavTable.size(), layout.mCbvSrvUavTable.data(), cbvSrvUavRange[i], &rootParam);

			// Store some of the binding info which will be required later when binding the descriptor table
			// We need the root index when calling SetRootDescriptorTable
			pRootSignature->mDxViewDescriptorTableRootIndices[i] = rootParamCount;
			pRootSignature->mDxViewDescriptorCounts[i] = (uint32_t)layout.mCbvSrvUavTable.size();

			for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mCbvSrvUavTable.size(); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mCbvSrvUavTable[descIndex].second;
				pDesc->mIndexInParent = descIndex;

				// Store the d3d12 related info in the descriptor to avoid constantly calling the util_to_dx mapping functions
				pDesc->mRootDescriptor = 0;
				pDesc->mHandleIndex = pRootSignature->mDxCumulativeViewDescriptorCounts[i];

				// Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
				// This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays) in the descriptor table
				pRootSignature->mDxCumulativeViewDescriptorCounts[i] += pDesc->mSize;
			}

			rootParams[rootParamCount++] = rootParam;
		}

		// Fill the descriptor table layout for the sampler descriptor table of this update frequency
		if (layout.mSamplerTable.size())
		{
			D3D12_ROOT_PARAMETER1 rootParam;
			create_descriptor_table((uint32_t)layout.mSamplerTable.size(), layout.mSamplerTable.data(), samplerRange[i], &rootParam);

			// Store some of the binding info which will be required later when binding the descriptor table
			// We need the root index when calling SetRootDescriptorTable
			pRootSignature->mDxSamplerDescriptorTableRootIndices[i] = rootParamCount;
			pRootSignature->mDxSamplerDescriptorCounts[i] = (uint32_t)layout.mSamplerTable.size();
			//table.pDescriptorIndices = (uint32_t*)tf_calloc(table.mDescriptorCount, sizeof(uint32_t));

			for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mSamplerTable.size(); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mSamplerTable[descIndex].second;
				pDesc->mIndexInParent = descIndex;

				// Store the d3d12 related info in the descriptor to avoid constantly calling the util_to_dx mapping functions
				pDesc->mRootDescriptor = 0;
				pDesc->mHandleIndex = pRootSignature->mDxCumulativeSamplerDescriptorCounts[i];

				// Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
				// This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays) in the descriptor table
				pRootSignature->mDxCumulativeSamplerDescriptorCounts[i] += pDesc->mSize;
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
#ifdef ENABLE_RAYTRACING
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
	CHECK_HRESULT(pRenderer->pDxDevice->CreateRootSignature(
		util_calculate_shared_node_mask(pRenderer), rootSignatureString->GetBufferPointer(),
		rootSignatureString->GetBufferSize(), IID_ARGS(&pRootSignature->pDxRootSignature)));

	SAFE_RELEASE(error);
	SAFE_RELEASE(rootSignatureString);

	*ppRootSignature = pRootSignature;
}

void removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
	pRootSignature->pDescriptorNameToIndexMap->mMap.clear(true);
	SAFE_RELEASE(pRootSignature->pDxRootSignature);

	SAFE_FREE(pRootSignature);
}
/************************************************************************/
// Descriptor Set Functions
/************************************************************************/
void addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppDescriptorSet);

	const RootSignature* pRootSignature = pDesc->pRootSignature;
	const DescriptorUpdateFrequency updateFreq = pDesc->mUpdateFrequency;
	const uint32_t nodeIndex = pDesc->mNodeIndex;
	const uint32_t cbvSrvUavDescCount = pRootSignature->mDxCumulativeViewDescriptorCounts[updateFreq];
	const uint32_t samplerDescCount = pRootSignature->mDxCumulativeSamplerDescriptorCounts[updateFreq];
	const uint32_t rootDescCount = pRootSignature->mDxRootDescriptorCounts[updateFreq];

	size_t totalSize = sizeof(DescriptorSet);
	totalSize += rootDescCount ? (pDesc->mMaxSets * sizeof(D3D12_GPU_VIRTUAL_ADDRESS)) : 0;
	DescriptorSet* pDescriptorSet = (DescriptorSet*)tf_calloc_memalign(1, alignof(DescriptorSet), totalSize);
	ASSERT(pDescriptorSet);

	pDescriptorSet->pRootSignature = pRootSignature;
	pDescriptorSet->mUpdateFrequency = updateFreq;
	pDescriptorSet->mNodeIndex = nodeIndex;
	pDescriptorSet->mMaxSets = pDesc->mMaxSets;
	pDescriptorSet->mCbvSrvUavRootIndex = pRootSignature->mDxViewDescriptorTableRootIndices[updateFreq];
	pDescriptorSet->mSamplerRootIndex = pRootSignature->mDxSamplerDescriptorTableRootIndices[updateFreq];
	pDescriptorSet->mRootAddressCount = rootDescCount;
	pDescriptorSet->mCbvSrvUavHandle = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
	pDescriptorSet->mSamplerHandle = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
	pDescriptorSet->mPipelineType = pRootSignature->mPipelineType;
	pDescriptorSet->pRootSignatureHandle = pRootSignature->pDxRootSignature;

	if (cbvSrvUavDescCount || samplerDescCount)
	{
		if (cbvSrvUavDescCount)
		{
			DescriptorHeap* pHeap = pRenderer->pCbvSrvUavHeaps[nodeIndex];
			DescriptorHeap::DescriptorHandle startHandle = consume_descriptor_handles(pHeap, cbvSrvUavDescCount * pDesc->mMaxSets);
			pDescriptorSet->mCbvSrvUavHandle = startHandle.mGpu.ptr - pHeap->mStartHandle.mGpu.ptr;
			pDescriptorSet->mCbvSrvUavStride = cbvSrvUavDescCount * pHeap->mDescriptorSize;

			for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
			{
				const DescriptorInfo* pDescInfo = &pRootSignature->pDescriptors[i];
				if (!pDescInfo->mRootDescriptor && pDescInfo->mType != DESCRIPTOR_TYPE_SAMPLER && pDescInfo->mUpdateFrequency == updateFreq)
				{
					DescriptorType        type = (DescriptorType)pDescInfo->mType;
					D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = { D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN };
					switch (type)
					{
					case DESCRIPTOR_TYPE_TEXTURE:        srcHandle = pRenderer->pNullDescriptors->mNullTextureSRV[pDescInfo->mDim]; break;
					case DESCRIPTOR_TYPE_BUFFER:         srcHandle = pRenderer->pNullDescriptors->mNullBufferSRV; break;
					case DESCRIPTOR_TYPE_RW_TEXTURE:     srcHandle = pRenderer->pNullDescriptors->mNullTextureUAV[pDescInfo->mDim]; break;
					case DESCRIPTOR_TYPE_RW_BUFFER:      srcHandle = pRenderer->pNullDescriptors->mNullBufferUAV; break;
					case DESCRIPTOR_TYPE_UNIFORM_BUFFER: srcHandle = pRenderer->pNullDescriptors->mNullBufferCBV; break;
					default: break;
					}

#ifdef ENABLE_RAYTRACING
					if (pDescInfo->mType != DESCRIPTOR_TYPE_RAY_TRACING)
#endif
					{
						ASSERT(srcHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

						for (uint32_t s = 0; s < pDesc->mMaxSets; ++s)
							for (uint32_t j = 0; j < pDescInfo->mSize; ++j)
								copy_descriptor_handle(pHeap,
									srcHandle, pDescriptorSet->mCbvSrvUavHandle + s * pDescriptorSet->mCbvSrvUavStride,
									pDescInfo->mHandleIndex + j);
					}
				}
			}
		}
		if (samplerDescCount)
		{
			DescriptorHeap* pHeap = pRenderer->pSamplerHeaps[nodeIndex];
			DescriptorHeap::DescriptorHandle startHandle = consume_descriptor_handles(pHeap, samplerDescCount * pDesc->mMaxSets);
			pDescriptorSet->mSamplerHandle = startHandle.mGpu.ptr - pHeap->mStartHandle.mGpu.ptr;
			pDescriptorSet->mSamplerStride = samplerDescCount * pHeap->mDescriptorSize;
			for (uint32_t i = 0; i < pDesc->mMaxSets; ++i)
			{
				for (uint32_t j = 0; j < samplerDescCount; ++j)
					copy_descriptor_handle(pHeap, pRenderer->pNullDescriptors->mNullSampler,
						pDescriptorSet->mSamplerHandle + i * pDescriptorSet->mSamplerStride, j);
			}
		}
	}

	if (pDescriptorSet->mRootAddressCount)
	{
		ASSERT(1 == pDescriptorSet->mRootAddressCount);
		pDescriptorSet->pRootAddresses = (D3D12_GPU_VIRTUAL_ADDRESS*)(pDescriptorSet + 1);
		pDescriptorSet->mRootDescriptorRootIndex = pRootSignature->mDxRootDescriptorRootIndices[updateFreq];
	}

	*ppDescriptorSet = pDescriptorSet;
}

void removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
{
	ASSERT(pRenderer);
	ASSERT(pDescriptorSet);
	SAFE_FREE(pDescriptorSet);
}

void updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
#define VALIDATE_DESCRIPTOR(descriptor,...)																\
	if (!(descriptor))																					\
	{																									\
		eastl::string msg = __FUNCTION__ + eastl::string(" : ") + eastl::string().sprintf(__VA_ARGS__);	\
		LOGF(LogLevel::eERROR, msg.c_str());															\
		_FailedAssert(__FILE__, __LINE__, msg.c_str());													\
		continue;																						\
	}
#else
#define VALIDATE_DESCRIPTOR(descriptor,...)
#endif

	ASSERT(pRenderer);
	ASSERT(pDescriptorSet);
	ASSERT(index < pDescriptorSet->mMaxSets);

	const RootSignature* pRootSignature = pDescriptorSet->pRootSignature;
	const DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)pDescriptorSet->mUpdateFrequency;
	const uint32_t nodeIndex = pDescriptorSet->mNodeIndex;
	bool update = false;

	for (uint32_t i = 0; i < count; ++i)
	{
		const DescriptorData* pParam = pParams + i;
		uint32_t paramIndex = pParam->mIndex;

		VALIDATE_DESCRIPTOR(pParam->pName || (paramIndex != -1), "DescriptorData has NULL name and invalid index");

		const DescriptorInfo* pDesc = (paramIndex != -1) ? (pRootSignature->pDescriptors + paramIndex) : get_descriptor(pRootSignature, pParam->pName);
		if (paramIndex != -1)
		{
			VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param index (%u)", paramIndex);
		}
		else
		{
			VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param name (%s)", pParam->pName);
		}

		const DescriptorType type = (DescriptorType)pDesc->mType;
		const uint32_t arrayCount = max(1U, pParam->mCount);

		VALIDATE_DESCRIPTOR(pDesc->mUpdateFrequency == updateFreq,
			"Descriptor (%s) - Mismatching update frequency and register space", pDesc->pName);

		if (pDesc->mRootDescriptor)
		{
			VALIDATE_DESCRIPTOR(arrayCount == 1, "Descriptor (%s) : D3D12_ROOT_PARAMETER_TYPE_CBV does not support arrays", pDesc->pName);
			// We have this validation to stay consistent with Vulkan
			VALIDATE_DESCRIPTOR(pParam->pSizes, "Descriptor (%s) : Must provide pSizes for D3D12_ROOT_PARAMETER_TYPE_CBV", pDesc->pName);

			pDescriptorSet->pRootAddresses[index] = pParam->ppBuffers[0]->mDxGpuAddress +
				(pParam->pOffsets ? (uint32_t)pParam->pOffsets[0] : 0);
		}
		else if (type == DESCRIPTOR_TYPE_SAMPLER)
		{
			// Index is invalid when descriptor is a static sampler
			VALIDATE_DESCRIPTOR(pDesc->mIndexInParent != -1,
				"Trying to update a static sampler (%s). All static samplers must be set in addRootSignature and cannot be updated later",
				pDesc->pName);

			VALIDATE_DESCRIPTOR(pParam->ppSamplers, "NULL Sampler (%s)", pDesc->pName);

			for (uint32_t arr = 0; arr < arrayCount; ++arr)
			{
				VALIDATE_DESCRIPTOR(pParam->ppSamplers[arr] != D3D12_GPU_VIRTUAL_ADDRESS_NULL, "NULL Sampler (%s [%u] )", pDesc->pName, arr);

				copy_descriptor_handle(pRenderer->pSamplerHeaps[nodeIndex],
					pParam->ppSamplers[arr]->mDxHandle,
					pDescriptorSet->mSamplerHandle + index * pDescriptorSet->mSamplerStride, pDesc->mHandleIndex + arr);
			}

			update = true;
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

					copy_descriptor_handle(pRenderer->pCbvSrvUavHeaps[nodeIndex],
						pParam->ppTextures[arr]->mDxDescriptorHandles,
						pDescriptorSet->mCbvSrvUavHandle + index * pDescriptorSet->mCbvSrvUavStride, pDesc->mHandleIndex + arr);
				}
				break;
			}
			case DESCRIPTOR_TYPE_RW_TEXTURE:
			{
				VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL RW Texture (%s)", pDesc->pName);

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL RW Texture (%s [%u] )", pDesc->pName, arr);

					copy_descriptor_handle(pRenderer->pCbvSrvUavHeaps[nodeIndex],
						{ pParam->ppTextures[arr]->mDxDescriptorHandles.ptr +
						pParam->ppTextures[arr]->mDescriptorSize * (pParam->mUAVMipSlice + pParam->ppTextures[arr]->mUavStartIndex) },
						pDescriptorSet->mCbvSrvUavHandle + index * pDescriptorSet->mCbvSrvUavStride, pDesc->mHandleIndex + arr);
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

					copy_descriptor_handle(pRenderer->pCbvSrvUavHeaps[nodeIndex],
						{ pParam->ppBuffers[arr]->mDxDescriptorHandles.ptr + pParam->ppBuffers[arr]->mDxSrvOffset },
						pDescriptorSet->mCbvSrvUavHandle + index * pDescriptorSet->mCbvSrvUavStride, pDesc->mHandleIndex + arr);
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

					copy_descriptor_handle(pRenderer->pCbvSrvUavHeaps[nodeIndex],
						{ pParam->ppBuffers[arr]->mDxDescriptorHandles.ptr + pParam->ppBuffers[arr]->mDxUavOffset },
						pDescriptorSet->mCbvSrvUavHandle + index * pDescriptorSet->mCbvSrvUavStride, pDesc->mHandleIndex + arr);
				}
				break;
			}
			case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			{
				VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Uniform Buffer (%s)", pDesc->pName);

				if (pParam->pOffsets)
				{
					VALIDATE_DESCRIPTOR(pParam->pSizes, "Descriptor (%s) - pSizes must be provided with pOffsets", pDesc->pName);

					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Uniform Buffer (%s [%u] )", pDesc->pName, arr);
						VALIDATE_DESCRIPTOR(pParam->pSizes[arr] > 0, "Descriptor (%s) - pSizes[%u] is zero", pDesc->pName, arr);
						VALIDATE_DESCRIPTOR(pParam->pSizes[arr] <= 65536, "Descriptor (%s) - pSizes[%u] is %ull which exceeds max size %u", pDesc->pName, arr,
							pParam->pSizes[arr], 65536U);

						D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
						cbvDesc.BufferLocation = pParam->ppBuffers[arr]->mDxGpuAddress + pParam->pOffsets[arr];
						cbvDesc.SizeInBytes = (UINT)pParam->pSizes[arr];
						pRenderer->pDxDevice->CreateConstantBufferView(&cbvDesc,
							{ pRenderer->pCbvSrvUavHeaps[nodeIndex]->mStartHandle.mCpu.ptr +
							(pDescriptorSet->mCbvSrvUavHandle + index * pDescriptorSet->mCbvSrvUavStride) });
					}
				}
				else
				{
					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Uniform Buffer (%s [%u] )", pDesc->pName, arr);
						VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr]->mSize <= 65536, "Descriptor (%s) - pSizes[%u] is exceeds max size", pDesc->pName, arr);

						copy_descriptor_handle(pRenderer->pCbvSrvUavHeaps[nodeIndex],
							{ pParam->ppBuffers[arr]->mDxDescriptorHandles.ptr },
							pDescriptorSet->mCbvSrvUavHandle + index * pDescriptorSet->mCbvSrvUavStride, pDesc->mHandleIndex + arr);
					}
				}
				break;
			}
#ifdef ENABLE_RAYTRACING
			case DESCRIPTOR_TYPE_RAY_TRACING:
			{
				VALIDATE_DESCRIPTOR(pParam->ppAccelerationStructures, "NULL Acceleration Structure (%s)", pDesc->pName);

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppAccelerationStructures[arr], "Acceleration Structure (%s [%u] )", pDesc->pName, arr);

					D3D12_CPU_DESCRIPTOR_HANDLE handle = { D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN };
					fillRaytracingDescriptorHandle(pParam->ppAccelerationStructures[arr], &handle);

					VALIDATE_DESCRIPTOR(handle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN, "Invalid Acceleration Structure (%s [%u] )", pDesc->pName, arr);

					copy_descriptor_handle(pRenderer->pCbvSrvUavHeaps[nodeIndex],
						handle,
						pDescriptorSet->mCbvSrvUavHandle + index * pDescriptorSet->mCbvSrvUavStride, pDesc->mHandleIndex + arr);
				}
				break;
			}
#endif
			default:
				break;
			}

			update = true;
		}
	}
}

bool reset_root_signature(Cmd* pCmd, PipelineType type, ID3D12RootSignature* pRootSignature)
{
	// Set root signature if the current one differs from pRootSignature
	if (pCmd->pBoundRootSignature != pRootSignature)
	{
		pCmd->pBoundRootSignature = pRootSignature;
		if (type == PIPELINE_TYPE_GRAPHICS)
			pCmd->pDxCmdList->SetGraphicsRootSignature(pRootSignature);
		else
			pCmd->pDxCmdList->SetComputeRootSignature(pRootSignature);

		for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
		{
			pCmd->pBoundDescriptorSets[i] = NULL;
			pCmd->mBoundDescriptorSetIndices[i] = -1;
		}
	}

	return false;
}

void cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
{
	ASSERT(pCmd);
	ASSERT(pDescriptorSet);
	ASSERT(index < pDescriptorSet->mMaxSets);

	const DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)pDescriptorSet->mUpdateFrequency;

	// Set root signature if the current one differs from pRootSignature
	reset_root_signature(pCmd, (PipelineType)pDescriptorSet->mPipelineType, pDescriptorSet->pRootSignatureHandle);

	// Bind all required root descriptors
	for (uint32_t i = 0; i < pDescriptorSet->mRootAddressCount; ++i)
	{
		if (pDescriptorSet->mPipelineType == PIPELINE_TYPE_GRAPHICS)
			pCmd->pDxCmdList->SetGraphicsRootConstantBufferView(pDescriptorSet->mRootDescriptorRootIndex,
				pDescriptorSet->pRootAddresses[index]);
		else
			pCmd->pDxCmdList->SetComputeRootConstantBufferView(pDescriptorSet->mRootDescriptorRootIndex,
				pDescriptorSet->pRootAddresses[index]);
	}

	if (pCmd->mBoundDescriptorSetIndices[pDescriptorSet->mUpdateFrequency] != index || pCmd->pBoundDescriptorSets[pDescriptorSet->mUpdateFrequency] != pDescriptorSet)
	{
		pCmd->pBoundDescriptorSets[pDescriptorSet->mUpdateFrequency] = pDescriptorSet;
		pCmd->mBoundDescriptorSetIndices[pDescriptorSet->mUpdateFrequency] = index;

		// Bind the descriptor tables associated with this DescriptorSet
		if (pDescriptorSet->mPipelineType == PIPELINE_TYPE_GRAPHICS)
		{
			if (pDescriptorSet->mCbvSrvUavHandle != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
				pCmd->pDxCmdList->SetGraphicsRootDescriptorTable(pDescriptorSet->mCbvSrvUavRootIndex,
					{ pCmd->mBoundHeapStartHandles[0].ptr +
					pDescriptorSet->mCbvSrvUavHandle + index * pDescriptorSet->mCbvSrvUavStride });
			if (pDescriptorSet->mSamplerHandle != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
				pCmd->pDxCmdList->SetGraphicsRootDescriptorTable(pDescriptorSet->mSamplerRootIndex,
					{ pCmd->mBoundHeapStartHandles[1].ptr +
					pDescriptorSet->mSamplerHandle + index * pDescriptorSet->mSamplerStride });
		}
		else
		{
			if (pDescriptorSet->mCbvSrvUavHandle != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
				pCmd->pDxCmdList->SetComputeRootDescriptorTable(pDescriptorSet->mCbvSrvUavRootIndex,
					{ pCmd->mBoundHeapStartHandles[0].ptr +
					pDescriptorSet->mCbvSrvUavHandle + index * pDescriptorSet->mCbvSrvUavStride });
			if (pDescriptorSet->mSamplerHandle != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
				pCmd->pDxCmdList->SetComputeRootDescriptorTable(pDescriptorSet->mSamplerRootIndex,
					{ pCmd->mBoundHeapStartHandles[1].ptr +
					pDescriptorSet->mSamplerHandle + index * pDescriptorSet->mSamplerStride });
		}
	}
}

void cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, const char* pName, const void* pConstants)
{
	ASSERT(pCmd);
	ASSERT(pConstants);
	ASSERT(pRootSignature);
	ASSERT(pName);
	
	// Set root signature if the current one differs from pRootSignature
	reset_root_signature(pCmd, pRootSignature->mPipelineType, pRootSignature->pDxRootSignature);

	const DescriptorInfo* pDesc = get_descriptor(pRootSignature, pName);
	ASSERT(pDesc);
	ASSERT(DESCRIPTOR_TYPE_ROOT_CONSTANT == pDesc->mType);
	
	if (pRootSignature->mPipelineType == PIPELINE_TYPE_GRAPHICS)
		pCmd->pDxCmdList->SetGraphicsRoot32BitConstants(pRootSignature->mDxRootConstantRootIndices[pDesc->mIndexInParent], pDesc->mSize, pConstants, 0);
	else
		pCmd->pDxCmdList->SetComputeRoot32BitConstants(pRootSignature->mDxRootConstantRootIndices[pDesc->mIndexInParent], pDesc->mSize, pConstants, 0);
}

void cmdBindPushConstantsByIndex(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
{
	ASSERT(pCmd);
	ASSERT(pConstants);
	ASSERT(pRootSignature);
	ASSERT(paramIndex >= 0 && paramIndex < pRootSignature->mDescriptorCount);
	
	// Set root signature if the current one differs from pRootSignature
	reset_root_signature(pCmd, pRootSignature->mPipelineType, pRootSignature->pDxRootSignature);

	const DescriptorInfo* pDesc = pRootSignature->pDescriptors + paramIndex;
	ASSERT(pDesc);
	ASSERT(DESCRIPTOR_TYPE_ROOT_CONSTANT == pDesc->mType);

	if (pRootSignature->mPipelineType == PIPELINE_TYPE_GRAPHICS)
		pCmd->pDxCmdList->SetGraphicsRoot32BitConstants(pRootSignature->mDxRootConstantRootIndices[pDesc->mIndexInParent], pDesc->mSize, pConstants, 0);
	else
		pCmd->pDxCmdList->SetComputeRoot32BitConstants(pRootSignature->mDxRootConstantRootIndices[pDesc->mIndexInParent], pDesc->mSize, pConstants, 0);
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
	ID3D12PipelineLibrary* psoCache = pMainDesc->pCache ? pMainDesc->pCache->pLibrary : NULL;
#endif
	
	size_t psoShaderHash = 0;
	size_t psoRenderHash = 0;

	pPipeline->mType = PIPELINE_TYPE_GRAPHICS;
	pPipeline->pRootSignature = pDesc->pRootSignature->pDxRootSignature;

	//add to gpu
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, VS);
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, PS);
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, DS);
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, HS);
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, GS);
	if (pShaderProgram->mStages & SHADER_STAGE_VERT)
	{
		VS.BytecodeLength = pShaderProgram->pShaderBlobs[pShaderProgram->pReflection->mVertexStageIndex]->GetBufferSize();
		VS.pShaderBytecode = pShaderProgram->pShaderBlobs[pShaderProgram->pReflection->mVertexStageIndex]->GetBufferPointer();
	}
	if (pShaderProgram->mStages & SHADER_STAGE_FRAG)
	{
		PS.BytecodeLength = pShaderProgram->pShaderBlobs[pShaderProgram->pReflection->mPixelStageIndex]->GetBufferSize();
		PS.pShaderBytecode = pShaderProgram->pShaderBlobs[pShaderProgram->pReflection->mPixelStageIndex]->GetBufferPointer();
	}
	if (pShaderProgram->mStages & SHADER_STAGE_HULL)
	{
		HS.BytecodeLength = pShaderProgram->pShaderBlobs[pShaderProgram->pReflection->mHullStageIndex]->GetBufferSize();
		HS.pShaderBytecode = pShaderProgram->pShaderBlobs[pShaderProgram->pReflection->mHullStageIndex]->GetBufferPointer();
	}
	if (pShaderProgram->mStages & SHADER_STAGE_DOMN)
	{
		DS.BytecodeLength = pShaderProgram->pShaderBlobs[pShaderProgram->pReflection->mDomainStageIndex]->GetBufferSize();
		DS.pShaderBytecode = pShaderProgram->pShaderBlobs[pShaderProgram->pReflection->mDomainStageIndex]->GetBufferPointer();
	}
	if (pShaderProgram->mStages & SHADER_STAGE_GEOM)
	{
		GS.BytecodeLength = pShaderProgram->pShaderBlobs[pShaderProgram->pReflection->mGeometryStageIndex]->GetBufferSize();
		GS.pShaderBytecode = pShaderProgram->pShaderBlobs[pShaderProgram->pReflection->mGeometryStageIndex]->GetBufferPointer();
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

			input_elements[input_elementCount].Format = (DXGI_FORMAT) TinyImageFormat_ToDXGI_FORMAT(attrib->mFormat);
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
	pipeline_state_desc.pRootSignature = pDesc->pRootSignature->pDxRootSignature;
	pipeline_state_desc.VS = VS;
	pipeline_state_desc.PS = PS;
	pipeline_state_desc.DS = DS;
	pipeline_state_desc.HS = HS;
	pipeline_state_desc.GS = GS;
	pipeline_state_desc.StreamOutput = stream_output_desc;
	pipeline_state_desc.BlendState = pDesc->pBlendState ? util_to_blend_desc(pDesc->pBlendState) : gDefaultBlendDesc;
	pipeline_state_desc.SampleMask = UINT_MAX;
	pipeline_state_desc.RasterizerState = pDesc->pRasterizerState ? util_to_rasterizer_desc(pDesc->pRasterizerState) : gDefaultRasterizerDesc;
	pipeline_state_desc.DepthStencilState = pDesc->pDepthState ? util_to_depth_desc(pDesc->pDepthState) : gDefaultDepthDesc;

	pipeline_state_desc.InputLayout = input_layout_desc;
	pipeline_state_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	pipeline_state_desc.PrimitiveTopologyType = util_to_dx_primitive_topology_type(pDesc->mPrimitiveTopo);
	pipeline_state_desc.NumRenderTargets = render_target_count;
	pipeline_state_desc.DSVFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mDepthStencilFormat);

	pipeline_state_desc.SampleDesc = sample_desc;
	pipeline_state_desc.CachedPSO = cached_pso_desc;
	pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	for (uint32_t attrib_index = 0; attrib_index < render_target_count; ++attrib_index)
	{
		pipeline_state_desc.RTVFormats[attrib_index] = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(
				pDesc->pColorFormats[attrib_index]);
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
		psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.DepthStencilState, sizeof(D3D12_DEPTH_STENCIL_DESC), psoRenderHash);
		psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.RasterizerState, sizeof(D3D12_RASTERIZER_DESC), psoRenderHash);

		psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)pipeline_state_desc.RTVFormats, render_target_count * sizeof(DXGI_FORMAT), psoRenderHash);
		psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.DSVFormat, sizeof(DXGI_FORMAT), psoRenderHash);
		psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.PrimitiveTopologyType, sizeof(D3D12_PRIMITIVE_TOPOLOGY_TYPE), psoRenderHash);
		psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.SampleDesc, sizeof(DXGI_SAMPLE_DESC), psoRenderHash);
		psoRenderHash = tf_mem_hash<uint8_t>((uint8_t*)&pipeline_state_desc.NodeMask, sizeof(UINT), psoRenderHash);

		swprintf(pipelineName, L"%S_S%zuR%zu", (pMainDesc->pName ? pMainDesc->pName : "GRAPHICSPSO"), psoShaderHash, psoRenderHash);
		result = psoCache->LoadGraphicsPipeline(pipelineName, &pipeline_state_desc, IID_ARGS(&pPipeline->pDxPipelineState));
	}
#endif

	if (!SUCCEEDED(result))
	{
		CHECK_HRESULT(hook_create_graphics_pipeline_state(pRenderer->pDxDevice, &pipeline_state_desc,
			pMainDesc->pPipelineExtensions, pMainDesc->mExtensionCount, &pPipeline->pDxPipelineState));

#ifndef DISABLE_PIPELINE_LIBRARY
		if (psoCache)
		{
			CHECK_HRESULT(psoCache->StorePipeline(pipelineName, pPipeline->pDxPipelineState));
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
	pPipeline->mDxPrimitiveTopology = topology;

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
	ASSERT(pDesc->pShaderProgram->pShaderBlobs[0]);

	//allocate new pipeline
	Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
	ASSERT(pPipeline);

	pPipeline->mType = PIPELINE_TYPE_COMPUTE;
	pPipeline->pRootSignature = pDesc->pRootSignature->pDxRootSignature;

	//add pipeline specifying its for compute purposes
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, CS);
	CS.BytecodeLength = pDesc->pShaderProgram->pShaderBlobs[0]->GetBufferSize();
	CS.pShaderBytecode = pDesc->pShaderProgram->pShaderBlobs[0]->GetBufferPointer();

	DECLARE_ZERO(D3D12_CACHED_PIPELINE_STATE, cached_pso_desc);
	cached_pso_desc.pCachedBlob = NULL;
	cached_pso_desc.CachedBlobSizeInBytes = 0;

	DECLARE_ZERO(D3D12_COMPUTE_PIPELINE_STATE_DESC, pipeline_state_desc);
	pipeline_state_desc.pRootSignature = pDesc->pRootSignature->pDxRootSignature;
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
	ID3D12PipelineLibrary* psoCache = pMainDesc->pCache ? pMainDesc->pCache->pLibrary : NULL;
	wchar_t pipelineName[MAX_DEBUG_NAME_LENGTH + 32] = {};

	if (psoCache)
	{
		size_t psoShaderHash = 0;
		psoShaderHash = tf_mem_hash<uint8_t>((uint8_t*)CS.pShaderBytecode, CS.BytecodeLength, psoShaderHash);

		swprintf(pipelineName, L"%S_S%zu", (pMainDesc->pName ? pMainDesc->pName : "COMPUTEPSO"), psoShaderHash);
		result = psoCache->LoadComputePipeline(pipelineName, &pipeline_state_desc, IID_ARGS(&pPipeline->pDxPipelineState));
	}
#endif

	if (!SUCCEEDED(result))
	{
		CHECK_HRESULT(hook_create_compute_pipeline_state(pRenderer->pDxDevice, &pipeline_state_desc,
			pMainDesc->pPipelineExtensions, pMainDesc->mExtensionCount, &pPipeline->pDxPipelineState));

#ifndef DISABLE_PIPELINE_LIBRARY
		if (psoCache)
		{
			CHECK_HRESULT(psoCache->StorePipeline(pipelineName, pPipeline->pDxPipelineState));
		}
#endif
	}

	*ppPipeline = pPipeline;
}

void addPipeline(Renderer* pRenderer, const PipelineDesc* pDesc, Pipeline** ppPipeline)
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
#ifdef ENABLE_RAYTRACING
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
}

void removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pPipeline);

	//delete pipeline from device
	SAFE_RELEASE(pPipeline->pDxPipelineState);
#ifdef ENABLE_RAYTRACING
	SAFE_RELEASE(pPipeline->pDxrPipeline);
#endif

	SAFE_FREE(pPipeline);
}

void addPipelineCache(Renderer* pRenderer, const PipelineCacheDesc* pDesc, PipelineCache** ppPipelineCache)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppPipelineCache);

	PipelineCache* pPipelineCache = (PipelineCache*)tf_calloc(1, sizeof(PipelineCache));
	ASSERT(pPipelineCache);

	if (pDesc->mSize)
	{
		// D3D12 does not copy pipeline cache data. We have to keep it around until the cache is alive
		pPipelineCache->pData = tf_malloc(pDesc->mSize);
		memcpy(pPipelineCache->pData, pDesc->pData, pDesc->mSize);
	}

#ifndef DISABLE_PIPELINE_LIBRARY
	D3D12_FEATURE_DATA_SHADER_CACHE feature = {};
	HRESULT result = pRenderer->pDxDevice->CheckFeatureSupport(D3D12_FEATURE_SHADER_CACHE, &feature, sizeof(feature));
	if (SUCCEEDED(result))
	{
		result = E_NOTIMPL;
		if (feature.SupportFlags & D3D12_SHADER_CACHE_SUPPORT_LIBRARY)
		{
			ID3D12Device1* device1 = NULL;
			result = pRenderer->pDxDevice->QueryInterface(IID_ARGS(&device1));
			if (SUCCEEDED(result))
			{
				result = device1->CreatePipelineLibrary(pPipelineCache->pData, pDesc->mSize, IID_ARGS(&pPipelineCache->pLibrary));
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

void removePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache)
{
	ASSERT(pRenderer);
	ASSERT(pPipelineCache);

#ifndef DISABLE_PIPELINE_LIBRARY
	SAFE_RELEASE(pPipelineCache->pLibrary);
#endif
	SAFE_FREE(pPipelineCache->pData);
	SAFE_FREE(pPipelineCache);
}

void getPipelineCacheData(Renderer* pRenderer, PipelineCache* pPipelineCache, size_t* pSize, void* pData)
{
	ASSERT(pRenderer);
	ASSERT(pPipelineCache);
	ASSERT(pSize);

#ifndef DISABLE_PIPELINE_LIBRARY
	*pSize = 0;

	if (pPipelineCache->pLibrary)
	{
		*pSize = pPipelineCache->pLibrary->GetSerializedSize();
		if (pData)
		{
			CHECK_HRESULT(pPipelineCache->pLibrary->Serialize(pData, *pSize));
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

	CHECK_HRESULT(pCmdPool->pDxCmdAlloc->Reset());
}

void beginCmd(Cmd* pCmd)
{
	ASSERT(pCmd);
	ASSERT(pCmd->pDxCmdList);

	CHECK_HRESULT(pCmd->pDxCmdList->Reset(pCmd->pCmdPool->pDxCmdAlloc, NULL));

	if (pCmd->mType != QUEUE_TYPE_TRANSFER)
	{
		ID3D12DescriptorHeap* heaps[] =
		{
			pCmd->pBoundHeaps[0]->pCurrentHeap,
			pCmd->pBoundHeaps[1]->pCurrentHeap,
		};
		pCmd->pDxCmdList->SetDescriptorHeaps(2, heaps);

		pCmd->mBoundHeapStartHandles[0] = pCmd->pBoundHeaps[0]->pCurrentHeap->GetGPUDescriptorHandleForHeapStart();
		pCmd->mBoundHeapStartHandles[1] = pCmd->pBoundHeaps[1]->pCurrentHeap->GetGPUDescriptorHandleForHeapStart();
	}

	// Reset CPU side data
	pCmd->pBoundRootSignature = NULL;
	for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
	{
		pCmd->pBoundDescriptorSets[i] = NULL;
		pCmd->mBoundDescriptorSetIndices[i] = -1;
	}
}

void endCmd(Cmd* pCmd)
{
	ASSERT(pCmd);
	ASSERT(pCmd->pDxCmdList);

	CHECK_HRESULT(pCmd->pDxCmdList->Close());
}

void cmdBindRenderTargets(
	Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** pRenderTargets, RenderTarget* pDepthStencil,
	const LoadActionsDesc* pLoadActions /* = NULL*/, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice,
	uint32_t depthMipSlice)
{
	ASSERT(pCmd);
	ASSERT(pCmd->pDxCmdList);

	if (!renderTargetCount && !pDepthStencil)
		return;

	D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
	D3D12_CPU_DESCRIPTOR_HANDLE* p_rtv_handles =
		renderTargetCount ? (D3D12_CPU_DESCRIPTOR_HANDLE*)alloca(renderTargetCount * sizeof(D3D12_CPU_DESCRIPTOR_HANDLE)) : NULL;
	for (uint32_t i = 0; i < renderTargetCount; ++i)
	{
		if (!pColorMipSlices && !pColorArraySlices)
		{
			p_rtv_handles[i] = pRenderTargets[i]->mDxDescriptors;
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

			p_rtv_handles[i] = { pRenderTargets[i]->mDxDescriptors.ptr + handle * pRenderTargets[i]->mDxDescriptorSize };
		}
	}

	if (pDepthStencil)
	{
		if (-1 == depthMipSlice && -1 == depthArraySlice)
		{
			dsv = pDepthStencil->mDxDescriptors;
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

			dsv = { pDepthStencil->mDxDescriptors.ptr + handle * pDepthStencil->mDxDescriptorSize };
		}
	}

	pCmd->pDxCmdList->OMSetRenderTargets(renderTargetCount, p_rtv_handles, FALSE, dsv.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL ? &dsv : NULL);

	//process clear actions (clear color/depth)
	if (pLoadActions)
	{
		for (uint32_t i = 0; i < renderTargetCount; ++i)
		{
			if (pLoadActions->mLoadActionsColor[i] == LOAD_ACTION_CLEAR)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE handle = p_rtv_handles[i];

				DECLARE_ZERO(FLOAT, color_rgba[4]);
				color_rgba[0] = pLoadActions->mClearColorValues[i].r;
				color_rgba[1] = pLoadActions->mClearColorValues[i].g;
				color_rgba[2] = pLoadActions->mClearColorValues[i].b;
				color_rgba[3] = pLoadActions->mClearColorValues[i].a;

				pCmd->pDxCmdList->ClearRenderTargetView(handle, color_rgba, 0, NULL);
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
			pCmd->pDxCmdList->ClearDepthStencilView(dsv, flags, pLoadActions->mClearDepth.depth, (UINT8)pLoadActions->mClearDepth.stencil, 0, NULL);
		}
	}
}

void cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
	ASSERT(pCmd);

	//set new viewport
	ASSERT(pCmd->pDxCmdList);

	D3D12_VIEWPORT viewport;
	viewport.TopLeftX = x;
	viewport.TopLeftY = y;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinDepth = minDepth;
	viewport.MaxDepth = maxDepth;

	pCmd->pDxCmdList->RSSetViewports(1, &viewport);
}

void cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	ASSERT(pCmd);

	//set new scissor values
	ASSERT(pCmd->pDxCmdList);

	D3D12_RECT scissor;
	scissor.left = x;
	scissor.top = y;
	scissor.right = x + width;
	scissor.bottom = y + height;

	pCmd->pDxCmdList->RSSetScissorRects(1, &scissor);
}

void cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
	ASSERT(pCmd);
	ASSERT(pPipeline);

	//bind given pipeline
	ASSERT(pCmd->pDxCmdList);

	if (pPipeline->mType == PIPELINE_TYPE_GRAPHICS)
	{
		ASSERT(pPipeline->pDxPipelineState);
		reset_root_signature(pCmd, pPipeline->mType, pPipeline->pRootSignature);
		pCmd->pDxCmdList->IASetPrimitiveTopology(pPipeline->mDxPrimitiveTopology);
		pCmd->pDxCmdList->SetPipelineState(pPipeline->pDxPipelineState);
	}
#ifdef ENABLE_RAYTRACING
	else if (pPipeline->mType == PIPELINE_TYPE_RAYTRACING)
	{
		reset_root_signature(pCmd, pPipeline->mType, pPipeline->pRootSignature);
		cmdBindRaytracingPipeline(pCmd, pPipeline);
	}
#endif
	else
	{
		ASSERT(pPipeline->pDxPipelineState);
		reset_root_signature(pCmd, pPipeline->mType, pPipeline->pRootSignature);
		pCmd->pDxCmdList->SetPipelineState(pPipeline->pDxPipelineState);
	}
}

void cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
	ASSERT(pCmd);
	ASSERT(pBuffer);
	ASSERT(pCmd->pDxCmdList);
	ASSERT(D3D12_GPU_VIRTUAL_ADDRESS_NULL != pBuffer->mDxGpuAddress);

	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = pBuffer->mDxGpuAddress + offset;
	ibView.Format = (INDEX_TYPE_UINT16 == indexType) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	ibView.SizeInBytes = (UINT)(pBuffer->mSize - offset);

	//bind given index buffer
	pCmd->pDxCmdList->IASetIndexBuffer(&ibView);
}

void cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
{
	ASSERT(pCmd);
	ASSERT(0 != bufferCount);
	ASSERT(ppBuffers);
	ASSERT(pCmd->pDxCmdList);
	//bind given vertex buffer

	DECLARE_ZERO(D3D12_VERTEX_BUFFER_VIEW, views[MAX_VERTEX_ATTRIBS]);
	for (uint32_t i = 0; i < bufferCount; ++i)
	{
		ASSERT(D3D12_GPU_VIRTUAL_ADDRESS_NULL != ppBuffers[i]->mDxGpuAddress);

		views[i].BufferLocation = (ppBuffers[i]->mDxGpuAddress + (pOffsets ? pOffsets[i] : 0));
		views[i].SizeInBytes = (UINT)(ppBuffers[i]->mSize - (pOffsets ? pOffsets[i] : 0));
		views[i].StrideInBytes = (UINT)pStrides[i];
	}

	pCmd->pDxCmdList->IASetVertexBuffers(0, bufferCount, views);
}

void cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
	ASSERT(pCmd);

	//draw given vertices
	ASSERT(pCmd->pDxCmdList);

	pCmd->pDxCmdList->DrawInstanced((UINT)vertexCount, (UINT)1, (UINT)firstVertex, (UINT)0);
}

void cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
	ASSERT(pCmd);

	//draw given vertices
	ASSERT(pCmd->pDxCmdList);

	pCmd->pDxCmdList->DrawInstanced((UINT)vertexCount, (UINT)instanceCount, (UINT)firstVertex, (UINT)firstInstance);
}

void cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
	ASSERT(pCmd);

	//draw indexed mesh
	ASSERT(pCmd->pDxCmdList);

	pCmd->pDxCmdList->DrawIndexedInstanced((UINT)indexCount, (UINT)1, (UINT)firstIndex, (UINT)firstVertex, (UINT)0);
}

void cmdDrawIndexedInstanced(
	Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstInstance, uint32_t firstVertex)
{
	ASSERT(pCmd);

	//draw indexed mesh
	ASSERT(pCmd->pDxCmdList);

	pCmd->pDxCmdList->DrawIndexedInstanced((UINT)indexCount, (UINT)instanceCount, (UINT)firstIndex, (UINT)firstVertex, (UINT)firstInstance);
}

void cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	ASSERT(pCmd);

	//dispatch given command
	ASSERT(pCmd->pDxCmdList != NULL);

	hook_dispatch(pCmd, groupCountX, groupCountY, groupCountZ);

}

void cmdResourceBarrier(Cmd* pCmd,
	uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers,
	uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers,
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
		if (pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY ||
			pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU ||
			(pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU && (pBuffer->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER)))
		{
			//if (!(pBuffer->mCurrentState & pTransBarrier->mNewState) && pBuffer->mCurrentState != pTransBarrier->mNewState)
			if (RESOURCE_STATE_UNORDERED_ACCESS == pTransBarrier->mCurrentState &&
				RESOURCE_STATE_UNORDERED_ACCESS == pTransBarrier->mNewState)
			{
				pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				pBarrier->UAV.pResource = pBuffer->pDxResource;
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
					pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
				}
				pBarrier->Transition.pResource = pBuffer->pDxResource;
				pBarrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				pBarrier->Transition.StateBefore = util_to_dx_resource_state(pTransBarrier->mCurrentState);
				pBarrier->Transition.StateAfter = util_to_dx_resource_state(pTransBarrier->mNewState);

				++transitionCount;
			}
		}
	}

	for (uint32_t i = 0; i < numTextureBarriers; ++i)
	{
		TextureBarrier*         pTransBarrier = &pTextureBarriers[i];
		D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
		Texture*                pTexture = pTransBarrier->pTexture;

		if (RESOURCE_STATE_UNORDERED_ACCESS == pTransBarrier->mCurrentState &&
			RESOURCE_STATE_UNORDERED_ACCESS == pTransBarrier->mNewState)
		{
			pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			pBarrier->UAV.pResource = pTexture->pDxResource;
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
				pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
			}
			pBarrier->Transition.pResource = pTexture->pDxResource;
			pBarrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			pBarrier->Transition.StateBefore = util_to_dx_resource_state(pTransBarrier->mCurrentState);
			pBarrier->Transition.StateAfter = util_to_dx_resource_state(pTransBarrier->mNewState);

			++transitionCount;
		}
	}

	for (uint32_t i = 0; i < numRtBarriers; ++i)
	{
		RenderTargetBarrier*    pTransBarrier = &pRtBarriers[i];
		D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
		Texture*                pTexture = pTransBarrier->pRenderTarget->pTexture;

		if (RESOURCE_STATE_UNORDERED_ACCESS == pTransBarrier->mCurrentState &&
			RESOURCE_STATE_UNORDERED_ACCESS == pTransBarrier->mNewState)
		{
			pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			pBarrier->UAV.pResource = pTexture->pDxResource;
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
				pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
			}
			pBarrier->Transition.pResource = pTexture->pDxResource;
			pBarrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			pBarrier->Transition.StateBefore = util_to_dx_resource_state(pTransBarrier->mCurrentState);
			pBarrier->Transition.StateAfter = util_to_dx_resource_state(pTransBarrier->mNewState);

			++transitionCount;
		}
	}

	if (transitionCount)
		pCmd->pDxCmdList->ResourceBarrier(transitionCount, barriers);
}

void cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
	ASSERT(pCmd);
	ASSERT(pSrcBuffer);
	ASSERT(pSrcBuffer->pDxResource);
	ASSERT(pBuffer);
	ASSERT(pBuffer->pDxResource);

#if defined(XBOX)
	pCmd->mDma.pDxCmdList->CopyBufferRegion(
		pBuffer->pDxResource, dstOffset, pSrcBuffer->pDxResource, srcOffset, size);
#else
	pCmd->pDxCmdList->CopyBufferRegion(
		pBuffer->pDxResource, dstOffset, pSrcBuffer->pDxResource, srcOffset, size);
#endif
}

typedef struct SubresourceDataDesc
{
	uint64_t                           mSrcOffset;
	uint32_t                           mMipLevel;
	uint32_t                           mArrayLayer;
} SubresourceDataDesc;

void cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pDesc)
{
#define SUBRESOURCE_INDEX(MipSlice, ArraySlice, PlaneSlice, MipLevels, ArraySize) \
((MipSlice) + ((ArraySlice) * (MipLevels)) + ((PlaneSlice) * (MipLevels) * (ArraySize)))

	uint32_t subresource = SUBRESOURCE_INDEX(pDesc->mMipLevel, pDesc->mArrayLayer, 0, pTexture->mMipLevels, pTexture->mArraySizeMinusOne + 1);
	D3D12_RESOURCE_DESC resourceDesc = pTexture->pDxResource->GetDesc();

	D3D12_TEXTURE_COPY_LOCATION src = {};
	D3D12_TEXTURE_COPY_LOCATION dst = {};
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.pResource = pSrcBuffer->pDxResource;
	pCmd->pRenderer->pDxDevice->GetCopyableFootprints(&resourceDesc, subresource, 1, pDesc->mSrcOffset, &src.PlacedFootprint, NULL, NULL, NULL);
	src.PlacedFootprint.Offset = pDesc->mSrcOffset;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.pResource = pTexture->pDxResource;
	dst.SubresourceIndex = subresource;
#if defined(XBOX)
	pCmd->mDma.pDxCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
#else
	pCmd->pDxCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
#endif
}
/************************************************************************/
// Queue Fence Semaphore Functions
/************************************************************************/
void acquireNextImage(
	Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pSwapChainImageIndex)
{
	UNREF_PARAM(pSignalSemaphore);
	UNREF_PARAM(pFence);
	ASSERT(pRenderer);
	ASSERT(pSwapChainImageIndex);

	//get latest backbuffer image
	HRESULT hr = hook_acquire_next_image(pRenderer->pDxDevice, pSwapChain);
	if (FAILED(hr))
	{
		LOGF(LogLevel::eERROR, "Failed to acquire next image");
		*pSwapChainImageIndex = UINT32_MAX;
		return;
	}

	*pSwapChainImageIndex = hook_get_swapchain_image_index(pSwapChain);
}

void queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc)
{
	ASSERT(pDesc);

	uint32_t cmdCount = pDesc->mCmdCount;
	Cmd** pCmds = pDesc->ppCmds;
	Fence* pFence = pDesc->pSignalFence;
	uint32_t waitSemaphoreCount = pDesc->mWaitSemaphoreCount;
	Semaphore** ppWaitSemaphores = pDesc->ppWaitSemaphores;
	uint32_t signalSemaphoreCount = pDesc->mSignalSemaphoreCount;
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
	ASSERT(pQueue->pDxQueue);

	ID3D12CommandList** cmds = (ID3D12CommandList**)alloca(cmdCount * sizeof(ID3D12CommandList*));
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		cmds[i] = pCmds[i]->pDxCmdList;
	}

	for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
		pQueue->pDxQueue->Wait(ppWaitSemaphores[i]->pDxFence, ppWaitSemaphores[i]->mFenceValue - 1);

	pQueue->pDxQueue->ExecuteCommandLists(cmdCount, cmds);

	if (pFence)
		hook_signal(pQueue, pFence->pDxFence, pFence->mFenceValue++);

	for (uint32_t i = 0; i < signalSemaphoreCount; ++i)
		hook_signal(pQueue, ppSignalSemaphores[i]->pDxFence, ppSignalSemaphores[i]->mFenceValue++);
}

void queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc)
{
	if (pDesc->pSwapChain)
	{
		SwapChain* pSwapChain = pDesc->pSwapChain;
		HRESULT hr = hook_queue_present(pQueue, pSwapChain, pDesc->mIndex);
		if (FAILED(hr))
		{
#if !defined(XBOX)
			ID3D12Device* device = NULL;
			pSwapChain->pDxSwapChain->GetDevice(IID_ARGS(&device));
			HRESULT removeHr =  device->GetDeviceRemovedReason();
			if (FAILED(removeHr))
				ASSERT(false);    //TODO: let's do something with the error
#else
			// Xbox One apps do not need to handle DXGI_ERROR_DEVICE_REMOVED or DXGI_ERROR_DEVICE_RESET.
#endif
			LOGF(LogLevel::eERROR, "Failed to present swapchain render target");
		}
	}
}

void waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
{
	// Wait for fence completion
	for (uint32_t i = 0; i < fenceCount; ++i)
	{
		FenceStatus fenceStatus;
		::getFenceStatus(pRenderer, ppFences[i], &fenceStatus);
		uint64_t fenceValue = ppFences[i]->mFenceValue - 1;
		//if (completedValue < fenceValue)
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
		{
			ppFences[i]->pDxFence->SetEventOnCompletion(fenceValue, ppFences[i]->pDxWaitIdleFenceEvent);
			WaitForSingleObject(ppFences[i]->pDxWaitIdleFenceEvent, INFINITE);
		}
	}
}

void waitQueueIdle(Queue* pQueue)
{
	hook_signal(pQueue, pQueue->pFence->pDxFence, pQueue->pFence->mFenceValue++);

	uint64_t fenceValue = pQueue->pFence->mFenceValue - 1;
	if (pQueue->pFence->pDxFence->GetCompletedValue() < pQueue->pFence->mFenceValue - 1)
	{
		pQueue->pFence->pDxFence->SetEventOnCompletion(fenceValue, pQueue->pFence->pDxWaitIdleFenceEvent);
		WaitForSingleObject(pQueue->pFence->pDxWaitIdleFenceEvent, INFINITE);
	}
}

void getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
	UNREF_PARAM(pRenderer);

	if (pFence->pDxFence->GetCompletedValue() < pFence->mFenceValue - 1)
		*pFenceStatus = FENCE_STATUS_INCOMPLETE;
	else
		*pFenceStatus = FENCE_STATUS_COMPLETE;
}
/************************************************************************/
// Utility functions
/************************************************************************/
TinyImageFormat getRecommendedSwapchainFormat(bool hintHDR)
{
	return hook_get_recommended_swapchain_format(hintHDR);
}

/************************************************************************/
// Execute Indirect Implementation
/************************************************************************/
void addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(pDesc->pArgDescs);
	ASSERT(ppCommandSignature);

	CommandSignature* pCommandSignature = (CommandSignature*)tf_calloc(1, sizeof(CommandSignature));
	ASSERT(pCommandSignature);

	bool change = false;
	// calculate size through arguement types
	uint32_t commandStride = 0;

	D3D12_INDIRECT_ARGUMENT_DESC* argumentDescs =
		(D3D12_INDIRECT_ARGUMENT_DESC*)alloca((pDesc->mIndirectArgCount + 1) * sizeof(D3D12_INDIRECT_ARGUMENT_DESC));

	for (uint32_t i = 0; i < pDesc->mIndirectArgCount; ++i)
	{
		if (pDesc->pArgDescs[i].mType == INDIRECT_DESCRIPTOR_TABLE || pDesc->pArgDescs[i].mType == INDIRECT_PIPELINE)
		{
			LOGF(LogLevel::eERROR, "Dx12 Doesn't support DescriptorTable or Pipeline in Indirect Command");
		}

		const DescriptorInfo* desc = NULL;
		if (pDesc->pArgDescs[i].mType > INDIRECT_DISPATCH)
		{
			desc = (pDesc->pArgDescs[i].pName) ?
				get_descriptor(pDesc->pRootSignature, pDesc->pArgDescs[i].pName) :
				&pDesc->pRootSignature->pDescriptors[pDesc->pArgDescs[i].mIndex];
		}

		switch (pDesc->pArgDescs[i].mType)
		{
			case INDIRECT_CONSTANT:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
				argumentDescs[i].Constant.RootParameterIndex = desc->mIndexInParent;
				argumentDescs[i].Constant.DestOffsetIn32BitValues = 0;
				argumentDescs[i].Constant.Num32BitValuesToSet = desc->mSize;
				commandStride += sizeof(UINT) * argumentDescs[i].Constant.Num32BitValuesToSet;
				change = true;
				break;
			case INDIRECT_UNORDERED_ACCESS_VIEW:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW;
				argumentDescs[i].UnorderedAccessView.RootParameterIndex = desc->mIndexInParent;
				commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
				change = true;
				break;
			case INDIRECT_SHADER_RESOURCE_VIEW:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW;
				argumentDescs[i].ShaderResourceView.RootParameterIndex = desc->mIndexInParent;
				commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
				change = true;
				break;
			case INDIRECT_CONSTANT_BUFFER_VIEW:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
				argumentDescs[i].ConstantBufferView.RootParameterIndex = desc->mIndexInParent;
				commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
				change = true;
				break;
			case INDIRECT_VERTEX_BUFFER:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
				argumentDescs[i].VertexBuffer.Slot = desc->mIndexInParent;
				commandStride += sizeof(D3D12_VERTEX_BUFFER_VIEW);
				change = true;
				break;
			case INDIRECT_INDEX_BUFFER:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
				argumentDescs[i].VertexBuffer.Slot = desc->mIndexInParent;
				commandStride += sizeof(D3D12_INDEX_BUFFER_VIEW);
				change = true;
				break;
			case INDIRECT_DRAW:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
				commandStride += sizeof(IndirectDrawArguments);
				break;
			case INDIRECT_DRAW_INDEX:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
				commandStride += sizeof(IndirectDrawIndexArguments);
				break;
			case INDIRECT_DISPATCH:
				argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
				commandStride += sizeof(IndirectDispatchArguments);
				break;
		}
	}

	if (change)
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

	CHECK_HRESULT(pRenderer->pDxDevice->CreateCommandSignature(
		&commandSignatureDesc, change ? pDesc->pRootSignature->pDxRootSignature : NULL, IID_ARGS(&pCommandSignature->pDxHandle)));

	*ppCommandSignature = pCommandSignature;
}

void removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
{
	UNREF_PARAM(pRenderer);
	SAFE_RELEASE(pCommandSignature->pDxHandle);
	SAFE_FREE(pCommandSignature);
}

void cmdExecuteIndirect(
	Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
	Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
	ASSERT(pCommandSignature);
	ASSERT(pIndirectBuffer);

	if (!pCounterBuffer)
		pCmd->pDxCmdList->ExecuteIndirect(
			pCommandSignature->pDxHandle, maxCommandCount, pIndirectBuffer->pDxResource, bufferOffset, NULL, 0);
	else
		pCmd->pDxCmdList->ExecuteIndirect(
			pCommandSignature->pDxHandle, maxCommandCount, pIndirectBuffer->pDxResource, bufferOffset,
			pCounterBuffer->pDxResource, counterBufferOffset);
}
/************************************************************************/
// Query Heap Implementation
/************************************************************************/
void getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
	ASSERT(pQueue);
	ASSERT(pFrequency);

	UINT64 freq = 0;
	pQueue->pDxQueue->GetTimestampFrequency(&freq);
	*pFrequency = (double)freq;
}

void addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppQueryPool);

	QueryPool* pQueryPool = (QueryPool*)tf_calloc(1, sizeof(QueryPool));
	ASSERT(pQueryPool);

	pQueryPool->mType = util_to_dx_query_type(pDesc->mType);
	pQueryPool->mCount = pDesc->mQueryCount;

	D3D12_QUERY_HEAP_DESC desc = {};
	desc.Count = pDesc->mQueryCount;
	desc.NodeMask = util_calculate_node_mask(pRenderer, pDesc->mNodeIndex);
	desc.Type = util_to_dx_query_heap_type(pDesc->mType);
	pRenderer->pDxDevice->CreateQueryHeap(&desc, IID_ARGS(&pQueryPool->pDxQueryHeap));

	*ppQueryPool = pQueryPool;
}

void removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
	UNREF_PARAM(pRenderer);
	SAFE_RELEASE(pQueryPool->pDxQueryHeap);

	SAFE_FREE(pQueryPool);
}

void cmdResetQueryPool(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
	UNREF_PARAM(pCmd);
	UNREF_PARAM(pQueryPool);
	UNREF_PARAM(startQuery);
	UNREF_PARAM(queryCount);
}

void cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
	D3D12_QUERY_TYPE type = pQueryPool->mType;
	switch (type)
	{
		case D3D12_QUERY_TYPE_OCCLUSION: break;
		case D3D12_QUERY_TYPE_BINARY_OCCLUSION: break;
		case D3D12_QUERY_TYPE_TIMESTAMP: pCmd->pDxCmdList->EndQuery(pQueryPool->pDxQueryHeap, type, pQuery->mIndex); break;
		case D3D12_QUERY_TYPE_PIPELINE_STATISTICS: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM1: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM2: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM3: break;
		default: break;
	}
}

void cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
	cmdBeginQuery(pCmd, pQueryPool, pQuery);
}

void cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
{
	pCmd->pDxCmdList->ResolveQueryData(
		pQueryPool->pDxQueryHeap, pQueryPool->mType, startQuery, queryCount, pReadbackBuffer->pDxResource,
		startQuery * 8);
}
/************************************************************************/
// Memory Stats Implementation
/************************************************************************/
void calculateMemoryStats(Renderer* pRenderer, char** stats)
{
	WCHAR* wstats = NULL;
	pRenderer->pResourceAllocator->BuildStatsString(&wstats, TRUE);
	size_t converted = 0;
	*stats = (char*)tf_malloc(wcslen(wstats) * sizeof(char));
	wcstombs(*stats, wstats, wcslen(wstats));
	pRenderer->pResourceAllocator->FreeStatsString(wstats);
}

void calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
{
	D3D12MA::Stats stats;
	pRenderer->pResourceAllocator->CalculateStats(&stats);
	*usedBytes = stats.Total.UsedBytes;
	*totalAllocatedBytes = *usedBytes + stats.Total.UnusedBytes;
}

void freeMemoryStats(Renderer* pRenderer, char* stats) { tf_free(stats); }
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	// note: USE_PIX isn't the ideal test because we might be doing a debug build where pix
	// is not installed, or a variety of other reasons. It should be a separate #ifdef flag?
#if defined(USE_PIX)
	//color is in B8G8R8X8 format where X is padding
	PIXBeginEvent(pCmd->pDxCmdList, PIX_COLOR((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255)), pName);
#endif
}

void cmdEndDebugMarker(Cmd* pCmd)
{
#if defined(USE_PIX)
	PIXEndEvent(pCmd->pDxCmdList);
#endif
}

void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if defined(USE_PIX)
	//color is in B8G8R8X8 format where X is padding
	PIXSetMarker(pCmd->pDxCmdList, PIX_COLOR((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255)), pName);
#endif
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	ASSERT(pRenderer);
	ASSERT(pBuffer);
	ASSERT(pName);

	wchar_t wName[MAX_DEBUG_NAME_LENGTH] = {};
	size_t numConverted = 0;
	mbstowcs_s(&numConverted, wName, pName, MAX_DEBUG_NAME_LENGTH);
	pBuffer->pDxResource->SetName(wName);
#endif
}


void setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	ASSERT(pRenderer);
	ASSERT(pTexture);
	ASSERT(pName);

	wchar_t wName[MAX_DEBUG_NAME_LENGTH] = {};
	size_t numConverted = 0;
	mbstowcs_s(&numConverted, wName, pName, MAX_DEBUG_NAME_LENGTH);
	pTexture->pDxResource->SetName(wName);
#endif
}

void setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName)
{
	setTextureName(pRenderer, pRenderTarget->pTexture, pName);
}

void setPipelineName(Renderer* pRenderer, Pipeline* pPipeline, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	ASSERT(pRenderer);
	ASSERT(pPipeline);
	ASSERT(pName);

	wchar_t wName[MAX_DEBUG_NAME_LENGTH] = {};
	size_t numConverted = 0;
	mbstowcs_s(&numConverted, wName, pName, MAX_DEBUG_NAME_LENGTH);
	pPipeline->pDxPipelineState->SetName(wName);
#endif
}
/************************************************************************/
// Virtual Texture
/************************************************************************/
void alignedDivision(const D3D12_TILED_RESOURCE_COORDINATE& extent, const D3D12_TILED_RESOURCE_COORDINATE& granularity, D3D12_TILED_RESOURCE_COORDINATE* out)
{
	out->X = (extent.X / granularity.X + ((extent.X  % granularity.X) ? 1u : 0u));
	out->Y = (extent.Y / granularity.Y + ((extent.Y % granularity.Y) ? 1u : 0u));
	out->Z = (extent.Z / granularity.Z + ((extent.Z  % granularity.Z) ? 1u : 0u));
}

// Allocate memory for the virtual page
bool allocateVirtualPage(Renderer* pRenderer, Texture* pTexture, VirtualTexturePage &virtualPage)
{
	if (virtualPage.pIntermediateBuffer != NULL)
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
	addBuffer(pRenderer, &desc, &virtualPage.pIntermediateBuffer);
	return true;
}


// Release memory allocated for this page
void releaseVirtualPage(Renderer* pRenderer, VirtualTexturePage &virtualPage, bool removeMemoryBind)
{
	if (virtualPage.pIntermediateBuffer)
	{
		removeBuffer(pRenderer, virtualPage.pIntermediateBuffer);
		virtualPage.pIntermediateBuffer = NULL;
	}
}

VirtualTexturePage* addPage(Renderer* pRenderer, Texture* pTexture, D3D12_TILED_RESOURCE_COORDINATE offset, D3D12_TILED_RESOURCE_COORDINATE extent,
	const uint32_t size, const uint32_t mipLevel, uint32_t layer)
{
	eastl::vector<VirtualTexturePage>* pPageTable = (eastl::vector<VirtualTexturePage>*)pTexture->pSvt->pPages;

	VirtualTexturePage newPage = {};
	newPage.offset = offset;
	newPage.extent = extent;
	newPage.size = size;
	newPage.mipLevel = mipLevel;
	newPage.layer = layer;
	newPage.index = static_cast<uint32_t>(pPageTable->size());

	pPageTable->push_back(newPage);

	return &pPageTable->back();
}

struct PageCounts
{
	uint mAlivePageCount;
	uint mRemovePageCount;
};

void releasePage(Cmd* pCmd, Texture* pTexture)
{
	Renderer* pRenderer = pCmd->pRenderer;
	eastl::vector<VirtualTexturePage>* pPageTable = (eastl::vector<VirtualTexturePage>*)pTexture->pSvt->pPages;

	bool map = !pTexture->pSvt->mPageCounts->pCpuMappedAddress;
	if (map)
	{
		mapBuffer(pRenderer, pTexture->pSvt->mPageCounts, NULL);
	}

	uint removePageCount = ((const PageCounts*)pTexture->pSvt->mPageCounts->pCpuMappedAddress)->mRemovePageCount;

	if (map)
	{
		unmapBuffer(pRenderer, pTexture->pSvt->mPageCounts);
	}

	if (removePageCount == 0)
		return;

	eastl::vector<uint32_t> RemovePageTable;
	RemovePageTable.resize(removePageCount);

	map = !pTexture->pSvt->mRemovePage->pCpuMappedAddress;
	if (map)
	{
		mapBuffer(pRenderer, pTexture->pSvt->mRemovePage, NULL);
	}
	memcpy(RemovePageTable.data(), pTexture->pSvt->mRemovePage->pCpuMappedAddress, sizeof(uint));
	if (map)
	{
		unmapBuffer(pRenderer, pTexture->pSvt->mRemovePage);
	}

	for (int i = 0; i < (int)removePageCount; ++i)
	{
		uint32_t RemoveIndex = RemovePageTable[i];
		releaseVirtualPage(pRenderer, (*pPageTable)[RemoveIndex], false);
	}
}

// Fill a complete mip level
// Need to get visibility info first then fill them
void fillVirtualTexture(Cmd* pCmd, Texture* pTexture, Fence* pFence)
{
	Renderer* pRenderer = pCmd->pRenderer;
	eastl::vector<VirtualTexturePage>* pPageTable = (eastl::vector<VirtualTexturePage>*)pTexture->pSvt->pPages;
	eastl::vector<D3D12_TILED_RESOURCE_COORDINATE>* pSparseCoordinates = (eastl::vector<D3D12_TILED_RESOURCE_COORDINATE>*)pTexture->pSvt->pSparseCoordinates;
	eastl::vector<uint32_t>* pHeapRangeStartOffsets = (eastl::vector<uint32_t>*)pTexture->pSvt->pHeapRangeStartOffsets;

	eastl::vector<uint32_t> tileCounts;
	eastl::vector<D3D12_TILE_REGION_SIZE> regionSizes;
	eastl::vector<D3D12_TILE_RANGE_FLAGS> rangeFlags;
	//eastl::vector<uint32_t> rangeStartOffsets;

	pSparseCoordinates->set_capacity(0);
	pHeapRangeStartOffsets->set_capacity(0);

	bool map = !pTexture->pSvt->mPageCounts->pCpuMappedAddress;
	if (map)
	{
		mapBuffer(pRenderer, pTexture->pSvt->mPageCounts, NULL);
	}

	uint alivePageCount = ((const PageCounts*)pTexture->pSvt->mPageCounts->pCpuMappedAddress)->mAlivePageCount;

	if (map)
	{
		unmapBuffer(pRenderer, pTexture->pSvt->mPageCounts);
	}

	map = !pTexture->pSvt->mAlivePage->pCpuMappedAddress;
	if (map)
	{
		mapBuffer(pRenderer, pTexture->pSvt->mAlivePage, NULL);
	}

	eastl::vector<uint> VisibilityData;
	VisibilityData.resize(alivePageCount);
	memcpy(VisibilityData.data(), pTexture->pSvt->mAlivePage->pCpuMappedAddress, VisibilityData.size() * sizeof(uint));

	if (map)
	{
		unmapBuffer(pRenderer, pTexture->pSvt->mAlivePage);
	}

	D3D12_TILE_REGION_SIZE regionSize = { 1, true, 1, 1, 1 };
	D3D12_TILE_RANGE_FLAGS rangeFlag = D3D12_TILE_RANGE_FLAG_NONE;

	for (int i = 0; i < (int)VisibilityData.size(); ++i)
	{
		uint pageIndex = VisibilityData[i];
		VirtualTexturePage* pPage = &(*pPageTable)[pageIndex];

		if (allocateVirtualPage(pRenderer, pTexture, *pPage))
		{
			void* pData = (void*)((unsigned char*)pTexture->pSvt->mVirtualImageData + (pageIndex * pPage->size));

			map = !pPage->pIntermediateBuffer->pCpuMappedAddress;
			if (map)
			{
				mapBuffer(pRenderer, pPage->pIntermediateBuffer, NULL);
			}

			memcpy(pPage->pIntermediateBuffer->pCpuMappedAddress, pData, pPage->size);


			D3D12_TILED_RESOURCE_COORDINATE startCoord;
			startCoord.X = pPage->offset.X / (uint)pTexture->pSvt->mSparseVirtualTexturePageWidth;
			startCoord.Y = pPage->offset.Y / (uint)pTexture->pSvt->mSparseVirtualTexturePageHeight;
			startCoord.Z = pPage->offset.Z;
			startCoord.Subresource = pPage->offset.Subresource;

			pSparseCoordinates->push_back(startCoord);
			tileCounts.push_back(1);
			regionSizes.push_back(regionSize);
			rangeFlags.push_back(rangeFlag);
			pHeapRangeStartOffsets->push_back(pageIndex);

			D3D12_RESOURCE_DESC         Desc = pTexture->pDxResource->GetDesc();
			D3D12_TEXTURE_COPY_LOCATION Dst = {};
			Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			Dst.pResource = pTexture->pDxResource;
			Dst.SubresourceIndex = startCoord.Subresource;

			D3D12_TEXTURE_COPY_LOCATION Src = {};
			Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			Src.pResource = pPage->pIntermediateBuffer->pDxResource;
			Src.PlacedFootprint =
				D3D12_PLACED_SUBRESOURCE_FOOTPRINT{ 0,
									{ Desc.Format,
										(UINT)pTexture->pSvt->mSparseVirtualTexturePageWidth, (UINT)pTexture->pSvt->mSparseVirtualTexturePageHeight, 1, (UINT)pTexture->pSvt->mSparseVirtualTexturePageWidth * sizeof(uint32_t) } };

			pCmd->pDxCmdList->CopyTextureRegion(&Dst, startCoord.X * (UINT)pTexture->pSvt->mSparseVirtualTexturePageWidth, startCoord.Y * (UINT)pTexture->pSvt->mSparseVirtualTexturePageHeight, 0, &Src, NULL);

			if (map)
			{
				unmapBuffer(pRenderer, pPage->pIntermediateBuffer);
			}
		}
	}

	// Update sparse bind info
	if (pSparseCoordinates->size() > 0)
	{
		pCmd->pQueue->pDxQueue->UpdateTileMappings(pTexture->pDxResource,
			(UINT)pSparseCoordinates->size(),
			pSparseCoordinates->data(),
			regionSizes.data(),
			pTexture->pSvt->pSparseImageMemory,
			(UINT)pSparseCoordinates->size(),
			rangeFlags.data(),
			pHeapRangeStartOffsets->data(),
			tileCounts.data(),
			D3D12_TILE_MAPPING_FLAG_NONE);
	}

	regionSizes.set_capacity(0);
	rangeFlags.set_capacity(0);
	tileCounts.set_capacity(0);
}

// Fill smallest (non-tail) mip map level
void fillVirtualTextureLevel(Cmd* pCmd, Texture* pTexture, uint32_t mipLevel)
{
	Renderer* renderer = pCmd->pRenderer;

	eastl::vector<VirtualTexturePage>* pPageTable = (eastl::vector<VirtualTexturePage>*)pTexture->pSvt->pPages;
	eastl::vector<D3D12_TILED_RESOURCE_COORDINATE>* pSparseCoordinates = (eastl::vector<D3D12_TILED_RESOURCE_COORDINATE>*)pTexture->pSvt->pSparseCoordinates;

	eastl::vector<uint32_t> tileCounts;
	eastl::vector<D3D12_TILE_REGION_SIZE> regionSizes;
	eastl::vector<D3D12_TILE_RANGE_FLAGS> rangeFlags;
	eastl::vector<uint32_t> rangeStartOffsets;

	pSparseCoordinates->set_capacity(0);

	D3D12_TILE_REGION_SIZE regionSize = { 1, true, 1, 1, 1 };
	D3D12_TILE_RANGE_FLAGS rangeFlag = D3D12_TILE_RANGE_FLAG_NONE;

	for (int i = 0; i < (int)pTexture->pSvt->mVirtualPageTotalCount; i++)
	{
		VirtualTexturePage* pPage = &(*pPageTable)[i];
		uint32_t pageIndex = pPage->index;

		int globalOffset = 0;

		if ((pPage->mipLevel == mipLevel) && (pPage->pIntermediateBuffer == NULL))
		{
			if (allocateVirtualPage(renderer, pTexture, *pPage))
			{
				void* pData = (void*)((unsigned char*)pTexture->pSvt->mVirtualImageData + (pageIndex * (uint32_t)pPage->size));

				//CPU to GPU
				bool map = !pPage->pIntermediateBuffer->pCpuMappedAddress;
				if (map)
				{
					mapBuffer(renderer, pPage->pIntermediateBuffer, NULL);
				}

				memcpy(pPage->pIntermediateBuffer->pCpuMappedAddress, pData, pPage->size);

				D3D12_TILED_RESOURCE_COORDINATE startCoord;
				startCoord.X = pPage->offset.X / (uint)pTexture->pSvt->mSparseVirtualTexturePageWidth;
				startCoord.Y = pPage->offset.Y / (uint)pTexture->pSvt->mSparseVirtualTexturePageHeight;
				startCoord.Z = pPage->offset.Z;
				startCoord.Subresource = pPage->offset.Subresource;

				pSparseCoordinates->push_back(startCoord);
				tileCounts.push_back(1);
				regionSizes.push_back(regionSize);
				rangeFlags.push_back(rangeFlag);
				rangeStartOffsets.push_back(pageIndex);

				D3D12_RESOURCE_DESC         Desc = pTexture->pDxResource->GetDesc();
				D3D12_TEXTURE_COPY_LOCATION Dst = {};
				Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				Dst.pResource = pTexture->pDxResource;
				Dst.SubresourceIndex = mipLevel;


				D3D12_TEXTURE_COPY_LOCATION Src = {};
				Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				Src.pResource = pPage->pIntermediateBuffer->pDxResource;
				Src.PlacedFootprint =
					D3D12_PLACED_SUBRESOURCE_FOOTPRINT{ 0,
										{ Desc.Format,
											(UINT)pTexture->pSvt->mSparseVirtualTexturePageWidth, (UINT)pTexture->pSvt->mSparseVirtualTexturePageHeight, 1, (UINT)pTexture->pSvt->mSparseVirtualTexturePageWidth * sizeof(uint32_t) } };

				pCmd->pDxCmdList->CopyTextureRegion(
					&Dst, startCoord.X * (UINT)pTexture->pSvt->mSparseVirtualTexturePageWidth, startCoord.Y * (UINT)pTexture->pSvt->mSparseVirtualTexturePageHeight, 0, &Src, NULL);

				if (map)
				{
					unmapBuffer(renderer, pPage->pIntermediateBuffer);
				}
			}
		}
	}

	// Update sparse bind info
	if (pSparseCoordinates->size() > 0)
	{
		pCmd->pQueue->pDxQueue->UpdateTileMappings(
			pTexture->pDxResource,
			(UINT)pSparseCoordinates->size(),
			pSparseCoordinates->data(),
			regionSizes.data(),
			pTexture->pSvt->pSparseImageMemory,
			(UINT)pSparseCoordinates->size(),
			rangeFlags.data(),
			rangeStartOffsets.data(),
			tileCounts.data(),
			D3D12_TILE_MAPPING_FLAG_NONE);
	}

	regionSizes.set_capacity(0);
	rangeFlags.set_capacity(0);
	rangeStartOffsets.set_capacity(0);
	tileCounts.set_capacity(0);
}

void addVirtualTexture(Cmd* pCmd, const TextureDesc * pDesc, Texture ** ppTexture, void* pImageData)
{
	ASSERT(pCmd);
	Texture* pTexture = (Texture*)tf_calloc_memalign(1, alignof(Texture), sizeof(*pTexture) + sizeof(VirtualTexture));
	ASSERT(pTexture);

	Renderer* pRenderer = pCmd->pRenderer;

	pTexture->pSvt = (VirtualTexture*)(pTexture + 1);

	uint32_t imageSize = 0;
	uint32_t mipSize = pDesc->mWidth * pDesc->mHeight * pDesc->mDepth;

	while (mipSize > 0)
	{
		imageSize += mipSize;
		mipSize /= 4;
	}

	pTexture->pSvt->mVirtualImageData = pImageData;

	//add to gpu
	D3D12_RESOURCE_DESC desc = {};
	DXGI_FORMAT         dxFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DescriptorType      descriptors = pDesc->mDescriptors;

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

	D3D12_RESOURCE_STATES res_states = util_to_dx_resource_state(pDesc->mStartState);

	CHECK_HRESULT(pRenderer->pDxDevice->CreateReservedResource(&desc, res_states, NULL, IID_ARGS(&pTexture->pDxResource)));

	D3D12_SHADER_RESOURCE_VIEW_DESC  srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = dxFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	add_srv(pRenderer, pTexture->pDxResource, &srvDesc, &pTexture->mDxDescriptorHandles);

	UINT numTiles = 0;
	D3D12_PACKED_MIP_INFO packedMipInfo;
	D3D12_TILE_SHAPE tileShape = {};
	UINT subresourceCount = pDesc->mMipLevels;
	eastl::vector<D3D12_SUBRESOURCE_TILING> tilings(subresourceCount);
	pRenderer->pDxDevice->GetResourceTiling(pTexture->pDxResource, &numTiles, &packedMipInfo, &tileShape, &subresourceCount, 0, &tilings[0]);

	pTexture->pSvt->mSparseVirtualTexturePageWidth = tileShape.WidthInTexels;
	pTexture->pSvt->mSparseVirtualTexturePageHeight = tileShape.HeightInTexels;
	pTexture->pSvt->mVirtualPageTotalCount = imageSize / (uint32_t)(pTexture->pSvt->mSparseVirtualTexturePageWidth * pTexture->pSvt->mSparseVirtualTexturePageHeight);
	pTexture->pSvt->pPages = (eastl::vector<VirtualTexturePage>*)tf_calloc(1, sizeof(eastl::vector<VirtualTexturePage>));
	pTexture->pSvt->pSparseCoordinates = (eastl::vector<D3D12_TILED_RESOURCE_COORDINATE>*)tf_calloc(1, sizeof(eastl::vector<D3D12_TILED_RESOURCE_COORDINATE>));
	pTexture->pSvt->pHeapRangeStartOffsets = (eastl::vector<uint32_t>*)tf_calloc(1, sizeof(eastl::vector<uint32_t>));

	tf_placement_new<decltype(pTexture->pSvt->pPages)>(pTexture->pSvt->pPages);
	tf_placement_new<decltype(pTexture->pSvt->pSparseCoordinates)>(pTexture->pSvt->pSparseCoordinates);
	tf_placement_new<decltype(pTexture->pSvt->pHeapRangeStartOffsets)>(pTexture->pSvt->pHeapRangeStartOffsets);

	uint32_t TiledMiplevel = pDesc->mMipLevels - (uint32_t)log2(min((uint32_t)pTexture->pSvt->mSparseVirtualTexturePageWidth, (uint32_t)pTexture->pSvt->mSparseVirtualTexturePageHeight));

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
						VirtualTexturePage *newPage = addPage(pRenderer, pTexture, offset, extent, (uint32_t)pTexture->pSvt->mSparseVirtualTexturePageWidth * (uint32_t)pTexture->pSvt->mSparseVirtualTexturePageHeight * sizeof(uint), mipLevel, layer);
					}
				}
			}
		}
	}

	eastl::vector<VirtualTexturePage>* pPageTable = (eastl::vector<VirtualTexturePage>*)pTexture->pSvt->pPages;

	// Create Memeory
	UINT heapSize = UINT(pTexture->pSvt->mSparseVirtualTexturePageWidth * pTexture->pSvt->mSparseVirtualTexturePageHeight * sizeof(uint32_t) * (uint32_t)pPageTable->size());

	D3D12_HEAP_DESC desc_heap = {};
	desc_heap.Alignment = 0;
	desc_heap.Flags = D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
	desc_heap.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
	desc_heap.SizeInBytes = heapSize;

	CHECK_HRESULT(pRenderer->pDxDevice->CreateHeap(&desc_heap, __uuidof(pTexture->pSvt->pSparseImageMemory), (void**)&pTexture->pSvt->pSparseImageMemory));

	LOGF(LogLevel::eINFO, "Virtual Texture info: Dim %d x %d Pages %d", pDesc->mWidth, pDesc->mHeight, (uint32_t)(((eastl::vector<VirtualTexturePage>*)pTexture->pSvt->pPages)->size()));

	fillVirtualTextureLevel(pCmd, pTexture, TiledMiplevel - 1);

	pTexture->mOwnsImage = true;
	pTexture->mNodeIndex = pDesc->mNodeIndex;
	pTexture->mMipLevels = pDesc->mMipLevels;
	pTexture->mWidth = pDesc->mWidth;
	pTexture->mHeight = pDesc->mHeight;
	pTexture->mDepth = pDesc->mDepth;

	////save tetxure in given pointer
	*ppTexture = pTexture;
}

void removeVirtualTexture(Renderer* pRenderer, VirtualTexture* pSvt)
{
	eastl::vector<VirtualTexturePage>* pPageTable = (eastl::vector<VirtualTexturePage>*)pSvt->pPages;

	if (pPageTable)
	{
		for (int i = 0; i < (int)pPageTable->size(); i++)
		{
			VirtualTexturePage& virtualPage = (*pPageTable)[i];
			if (virtualPage.pIntermediateBuffer)
			{
				removeBuffer(pRenderer, virtualPage.pIntermediateBuffer);
				virtualPage.pIntermediateBuffer = NULL;
			}
		}

		pPageTable->set_capacity(0);
		SAFE_FREE(pSvt->pPages);
	}

	eastl::vector<D3D12_TILED_RESOURCE_COORDINATE>* pSparseCoordinates = (eastl::vector<D3D12_TILED_RESOURCE_COORDINATE>*)pSvt->pSparseCoordinates;

	if (pSparseCoordinates)
	{
		pSparseCoordinates->set_capacity(0);
		SAFE_FREE(pSvt->pSparseCoordinates);
	}

	eastl::vector<uint32_t>* pHeapRangeStartOffsets = (eastl::vector<uint32_t>*)pSvt->pHeapRangeStartOffsets;

	if (pHeapRangeStartOffsets)
	{
		pHeapRangeStartOffsets->set_capacity(0);
		SAFE_FREE(pSvt->pHeapRangeStartOffsets);
	}

	if (pSvt->pSparseImageMemory)
	{
		pSvt->pSparseImageMemory->Release();
	}

	if (pSvt->mVisibility)
		removeBuffer(pRenderer, pSvt->mVisibility);

	if (pSvt->mPrevVisibility)
		removeBuffer(pRenderer, pSvt->mPrevVisibility);

	if (pSvt->mAlivePage)
		removeBuffer(pRenderer, pSvt->mAlivePage);

	if (pSvt->mRemovePage)
		removeBuffer(pRenderer, pSvt->mRemovePage);

	if (pSvt->mPageCounts)
		removeBuffer(pRenderer, pSvt->mPageCounts);

	if (pSvt->mVirtualImageData)
		tf_free(pSvt->mVirtualImageData);
}

void cmdUpdateVirtualTexture(Cmd* cmd, Texture* pTexture)
{
	if (pTexture->pSvt->mVisibility)
	{
		releasePage(cmd, pTexture);
		fillVirtualTexture(cmd, pTexture, NULL);
	}
}
#endif
#endif

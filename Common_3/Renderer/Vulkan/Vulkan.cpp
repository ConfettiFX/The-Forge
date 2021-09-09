/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

#ifdef VULKAN

#define RENDERER_IMPLEMENTATION
#define VMA_IMPLEMENTATION
/************************************************************************/
// Debugging Macros
/************************************************************************/
// Uncomment this to enable render doc capture support
//#define USE_RENDER_DOC

// Debug Utils Extension not present on many Android devices
#if !defined(__ANDROID__)
#define USE_DEBUG_UTILS_EXTENSION
#endif
/************************************************************************/
/************************************************************************/
#if defined(_WINDOWS)
// Pull in minimal Windows headers
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#if defined(__linux__)
#define stricmp(a, b) strcasecmp(a, b)
#define vsprintf_s vsnprintf
#define strncpy_s strncpy
#endif

#if defined(NX64)
#include "../../../Switch/Common_3/Renderer/Vulkan/NX/NXVulkan.h"
#endif

#include "../IRenderer.h"

#include "../../ThirdParty/OpenSource/EASTL/vector.h"
#include "../../ThirdParty/OpenSource/EASTL/functional.h"
#include "../../ThirdParty/OpenSource/EASTL/sort.h"
#include "../../ThirdParty/OpenSource/EASTL/string_hash_map.h"

#include "../../OS/Interfaces/ILog.h"

#include "../../OS/Math/MathTypes.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif
#include "../../ThirdParty/OpenSource/VulkanMemoryAllocator/VulkanMemoryAllocator.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "../../OS/Core/Atomics.h"
#include "../../OS/Core/GPUConfig.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "VulkanCapsBuilder.h"

#if defined(VK_USE_DISPATCH_TABLES) && !defined(NX64)
#include "../../../Common_3/ThirdParty/OpenSource/volk/volkForgeExt.h"
#endif

#include "../../ThirdParty/OpenSource/ags/AgsHelper.h"
#include "../../ThirdParty/OpenSource/nvapi/NvApiHelper.h"

#if defined(QUEST_VR)
#include "../../../Quest/Common_3/Renderer/VR/VrApiHooks.h"
#endif

#include "../../OS/Interfaces/IMemory.h"

#define CHECK_VKRESULT(exp)                                                      \
	{                                                                            \
		VkResult vkres = (exp);                                                  \
		if (VK_SUCCESS != vkres)                                                 \
		{                                                                        \
			LOGF(eERROR, "%s: FAILED with VkResult: %i", #exp, (int)vkres); \
			ASSERT(false);                                                       \
		}                                                                        \
	}

extern void
	vk_createShaderReflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection);

#ifdef ENABLE_RAYTRACING
extern void vk_addRaytracingPipeline(const PipelineDesc*, Pipeline**);
extern void vk_FillRaytracingDescriptorData(const AccelerationStructure* pAccelerationStructure, void* pWriteNV);
#endif

// clang-format off
VkBlendOp gVkBlendOpTranslator[BlendMode::MAX_BLEND_MODES] =
{
	VK_BLEND_OP_ADD,
	VK_BLEND_OP_SUBTRACT,
	VK_BLEND_OP_REVERSE_SUBTRACT,
	VK_BLEND_OP_MIN,
	VK_BLEND_OP_MAX,
};

VkBlendFactor gVkBlendConstantTranslator[BlendConstant::MAX_BLEND_CONSTANTS] =
{
	VK_BLEND_FACTOR_ZERO,
	VK_BLEND_FACTOR_ONE,
	VK_BLEND_FACTOR_SRC_COLOR,
	VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
	VK_BLEND_FACTOR_DST_COLOR,
	VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
	VK_BLEND_FACTOR_SRC_ALPHA,
	VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	VK_BLEND_FACTOR_DST_ALPHA,
	VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
	VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
	VK_BLEND_FACTOR_CONSTANT_COLOR,
	VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
};

VkCompareOp gVkComparisonFuncTranslator[CompareMode::MAX_COMPARE_MODES] =
{
	VK_COMPARE_OP_NEVER,
	VK_COMPARE_OP_LESS,
	VK_COMPARE_OP_EQUAL,
	VK_COMPARE_OP_LESS_OR_EQUAL,
	VK_COMPARE_OP_GREATER,
	VK_COMPARE_OP_NOT_EQUAL,
	VK_COMPARE_OP_GREATER_OR_EQUAL,
	VK_COMPARE_OP_ALWAYS,
};

VkStencilOp gVkStencilOpTranslator[StencilOp::MAX_STENCIL_OPS] =
{
	VK_STENCIL_OP_KEEP,
	VK_STENCIL_OP_ZERO,
	VK_STENCIL_OP_REPLACE,
	VK_STENCIL_OP_INVERT,
	VK_STENCIL_OP_INCREMENT_AND_WRAP,
	VK_STENCIL_OP_DECREMENT_AND_WRAP,
	VK_STENCIL_OP_INCREMENT_AND_CLAMP,
	VK_STENCIL_OP_DECREMENT_AND_CLAMP,
};

VkCullModeFlagBits gVkCullModeTranslator[CullMode::MAX_CULL_MODES] =
{
	VK_CULL_MODE_NONE,
	VK_CULL_MODE_BACK_BIT,
	VK_CULL_MODE_FRONT_BIT
};

VkPolygonMode gVkFillModeTranslator[FillMode::MAX_FILL_MODES] =
{
	VK_POLYGON_MODE_FILL,
	VK_POLYGON_MODE_LINE
};

VkFrontFace gVkFrontFaceTranslator[] =
{
	VK_FRONT_FACE_COUNTER_CLOCKWISE,
	VK_FRONT_FACE_CLOCKWISE
};

VkAttachmentLoadOp gVkAttachmentLoadOpTranslator[LoadActionType::MAX_LOAD_ACTION] = 
{
	VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	VK_ATTACHMENT_LOAD_OP_LOAD,
	VK_ATTACHMENT_LOAD_OP_CLEAR,
};

const char* gVkWantedInstanceExtensions[] =
{
	VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
	VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_GGP)
	VK_GGP_STREAM_DESCRIPTOR_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_VI_NN)
	VK_NN_VI_SURFACE_EXTENSION_NAME,
#endif
	// Debug utils not supported on all devices yet
#ifdef USE_DEBUG_UTILS_EXTENSION
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#else
	VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
	VK_NV_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
	// To legally use HDR formats
	VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
	/************************************************************************/
	// VR Extensions
	/************************************************************************/
	VK_KHR_DISPLAY_EXTENSION_NAME,
	VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
	/************************************************************************/
	// Multi GPU Extensions
	/************************************************************************/
#if VK_KHR_device_group_creation
	  VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME,
#endif
#ifndef NX64 
	  /************************************************************************/
	// Property querying extensions
	/************************************************************************/
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	/************************************************************************/
	/************************************************************************/
#endif
};

const char* gVkWantedDeviceExtensions[] =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_MAINTENANCE1_EXTENSION_NAME,
	VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
	VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME,
	VK_EXT_SHADER_SUBGROUP_VOTE_EXTENSION_NAME,
	VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
	VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
#ifdef USE_EXTERNAL_MEMORY_EXTENSIONS
	VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
	VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
	VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
	VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
	VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME,
#endif
#endif
	// Debug marker extension in case debug utils is not supported
#ifndef USE_DEBUG_UTILS_EXTENSION
	VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
#endif
#if defined(VK_USE_PLATFORM_GGP)
	VK_GGP_FRAME_TOKEN_EXTENSION_NAME,
#endif

#if VK_KHR_draw_indirect_count
	VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
#endif
	// Fragment shader interlock extension to be used for ROV type functionality in Vulkan
#if VK_EXT_fragment_shader_interlock
	VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME,
#endif
	/************************************************************************/
	// NVIDIA Specific Extensions
	/************************************************************************/
#ifdef USE_NV_EXTENSIONS
	VK_NVX_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME,
#endif
	/************************************************************************/
	// AMD Specific Extensions
	/************************************************************************/
	VK_AMD_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
	VK_AMD_SHADER_BALLOT_EXTENSION_NAME,
	VK_AMD_GCN_SHADER_EXTENSION_NAME,
	/************************************************************************/
	// Multi GPU Extensions
	/************************************************************************/
#if VK_KHR_device_group
	VK_KHR_DEVICE_GROUP_EXTENSION_NAME,
#endif
	/************************************************************************/
	// Bindless & None Uniform access Extensions
	/************************************************************************/
	VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
#if VK_KHR_maintenance3 // descriptor indexing depends on this
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
#endif
	/************************************************************************/
	// Descriptor Update Template Extension for efficient descriptor set updates
	/************************************************************************/
	VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
	/************************************************************************/
	// Raytracing
	/************************************************************************/
#ifdef ENABLE_RAYTRACING
	VK_NV_RAY_TRACING_EXTENSION_NAME,
#endif
	/************************************************************************/
	// YCbCr format support
	/************************************************************************/
#if VK_KHR_bind_memory2
	// Requirement for VK_KHR_sampler_ycbcr_conversion
	VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
#endif
#if VK_KHR_sampler_ycbcr_conversion
	VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
    #if VK_KHR_bind_memory2 // ycbcr conversion depends on this
        VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
    #endif
#endif
    /************************************************************************/
	// Nsight Aftermath
	/************************************************************************/
#ifdef USE_NSIGHT_AFTERMATH
	VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME,
	VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME,
#endif
	/************************************************************************/
	/************************************************************************/
};
// clang-format on

#ifdef USE_DEBUG_UTILS_EXTENSION
static bool gDebugUtilsExtension = false;
#endif
static bool gRenderDocLayerEnabled = false;
static bool gDedicatedAllocationExtension = false;
static bool gExternalMemoryExtension = false;
#ifndef NX64
static bool gDrawIndirectCountExtension = false;
#endif
static bool gDeviceGroupCreationExtension = false;
static bool gDescriptorIndexingExtension = false;
static bool gAMDDrawIndirectCountExtension = false;
static bool gAMDGCNShaderExtension = false;
static bool gNVRayTracingExtension = false;
static bool gYCbCrExtension = false;
static bool gDebugMarkerSupport = false;

static void* VKAPI_PTR gVkAllocation(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
	return tf_memalign(alignment, size);
}

static void* VKAPI_PTR
			 gVkReallocation(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
	return tf_realloc(pOriginal, size);
}

static void VKAPI_PTR gVkFree(void* pUserData, void* pMemory) { tf_free(pMemory); }

static void VKAPI_PTR
			gVkInternalAllocation(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
}

static void VKAPI_PTR
			gVkInternalFree(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
}

VkAllocationCallbacks gVkAllocationCallbacks = {
	// pUserData
	NULL,
	// pfnAllocation
	gVkAllocation,
	// pfnReallocation
	gVkReallocation,
	// pfnFree
	gVkFree,
	// pfnInternalAllocation
	gVkInternalAllocation,
	// pfnInternalFree
	gVkInternalFree
};

#if defined(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME)
PFN_vkCmdDrawIndirectCountKHR        pfnVkCmdDrawIndirectCountKHR = NULL;
PFN_vkCmdDrawIndexedIndirectCountKHR pfnVkCmdDrawIndexedIndirectCountKHR = NULL;
#else
PFN_vkCmdDrawIndirectCountAMD        pfnVkCmdDrawIndirectCountKHR = NULL;
PFN_vkCmdDrawIndexedIndirectCountAMD pfnVkCmdDrawIndexedIndirectCountKHR = NULL;
#endif
/************************************************************************/
// IMPLEMENTATION
/************************************************************************/
#if defined(RENDERER_IMPLEMENTATION)

#if !defined(VK_USE_DISPATCH_TABLES)
#ifdef _MSC_VER
#pragma comment(lib, "vulkan-1.lib")
#endif
#endif

#define SAFE_FREE(p_var)       \
	if (p_var)                 \
	{                          \
		tf_free((void*)p_var); \
	}

//-V:SAFE_FREE:547 - The TF memory manager might not appreciate having null passed to tf_free.
// Don't trigger PVS warnings about always-true/false on this macro

#if defined(__cplusplus)
#define DECLARE_ZERO(type, var) type var = {};
#else
#define DECLARE_ZERO(type, var) type var = { 0 };
#endif

// Internal utility functions (may become external one day)
VkSampleCountFlagBits util_to_vk_sample_count(SampleCount sampleCount);

DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(void, mapBuffer, Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
DECLARE_RENDERER_FUNCTION(void, unmapBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(
	void, cmdUpdateBuffer, Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
DECLARE_RENDERER_FUNCTION(
	void, cmdUpdateSubresource, Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_RENDERER_FUNCTION(void, addTexture, Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
DECLARE_RENDERER_FUNCTION(void, removeTexture, Renderer* pRenderer, Texture* pTexture)
DECLARE_RENDERER_FUNCTION(void, addVirtualTexture, Cmd* pCmd, const TextureDesc* pDesc, Texture** ppTexture, void* pImageData)
DECLARE_RENDERER_FUNCTION(void, removeVirtualTexture, Renderer* pRenderer, VirtualTexture* pTexture)

//+1 for Acceleration Structure
#define FORGE_DESCRIPTOR_TYPE_RANGE_SIZE (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT + 2)
static uint32_t gDescriptorTypeRangeSize = (FORGE_DESCRIPTOR_TYPE_RANGE_SIZE - 1);

/************************************************************************/
// DescriptorInfo Heap Structures
/************************************************************************/
/// CPU Visible Heap to store all the resources needing CPU read / write operations - Textures/Buffers/RTV
typedef struct DescriptorPool
{
	VkDevice                        pDevice;
	VkDescriptorPool                pCurrentPool;
	VkDescriptorPoolSize*           pPoolSizes;
	eastl::vector<VkDescriptorPool> mDescriptorPools;
	uint32_t                        mPoolSizeCount;
	uint32_t                        mNumDescriptorSets;
	uint32_t                        mUsedDescriptorSetCount;
	VkDescriptorPoolCreateFlags     mFlags;
	Mutex*                          pMutex;
} DescriptorPool;

/************************************************************************/
// Static DescriptorInfo Heap Implementation
/************************************************************************/
static void add_descriptor_pool(
	Renderer* pRenderer, uint32_t numDescriptorSets, VkDescriptorPoolCreateFlags flags, VkDescriptorPoolSize* pPoolSizes,
	uint32_t numPoolSizes, DescriptorPool** ppPool)
{
	DescriptorPool* pPool = (DescriptorPool*)tf_calloc(1, sizeof(*pPool));
	pPool->mFlags = flags;
	pPool->mNumDescriptorSets = numDescriptorSets;
	pPool->mUsedDescriptorSetCount = 0;
	pPool->pDevice = pRenderer->mVulkan.pVkDevice;
	pPool->pMutex = (Mutex*)tf_calloc(1, sizeof(Mutex));
	initMutex(pPool->pMutex);

	pPool->mPoolSizeCount = numPoolSizes;
	pPool->pPoolSizes = (VkDescriptorPoolSize*)tf_calloc(numPoolSizes, sizeof(VkDescriptorPoolSize));
	for (uint32_t i = 0; i < numPoolSizes; ++i)
		pPool->pPoolSizes[i] = pPoolSizes[i];

	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.pNext = NULL;
	poolCreateInfo.poolSizeCount = numPoolSizes;
	poolCreateInfo.pPoolSizes = pPoolSizes;
	poolCreateInfo.flags = flags;
	poolCreateInfo.maxSets = numDescriptorSets;

	CHECK_VKRESULT(vkCreateDescriptorPool(pPool->pDevice, &poolCreateInfo, &gVkAllocationCallbacks, &pPool->pCurrentPool));

	pPool->mDescriptorPools.emplace_back(pPool->pCurrentPool);

	*ppPool = pPool;
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif
static void reset_descriptor_pool(DescriptorPool* pPool)
{
	CHECK_VKRESULT(vkResetDescriptorPool(pPool->pDevice, pPool->pCurrentPool, pPool->mFlags));
	pPool->mUsedDescriptorSetCount = 0;
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

static void remove_descriptor_pool(Renderer* pRenderer, DescriptorPool* pPool)
{
	for (uint32_t i = 0; i < (uint32_t)pPool->mDescriptorPools.size(); ++i)
		vkDestroyDescriptorPool(pRenderer->mVulkan.pVkDevice, pPool->mDescriptorPools[i], &gVkAllocationCallbacks);

	pPool->mDescriptorPools.~vector();

	destroyMutex(pPool->pMutex);
	tf_free(pPool->pMutex);
	SAFE_FREE(pPool->pPoolSizes);
	SAFE_FREE(pPool);
}

static void consume_descriptor_sets(
	DescriptorPool* pPool, const VkDescriptorSetLayout* pLayouts, VkDescriptorSet** pSets, uint32_t numDescriptorSets)
{
	// Need a lock since vkAllocateDescriptorSets needs to be externally synchronized
	// This is fine since this will only happen during Init time
	MutexLock lock(*pPool->pMutex);

	DECLARE_ZERO(VkDescriptorSetAllocateInfo, alloc_info);
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.descriptorPool = pPool->pCurrentPool;
	alloc_info.descriptorSetCount = numDescriptorSets;
	alloc_info.pSetLayouts = pLayouts;

	VkResult vk_res = vkAllocateDescriptorSets(pPool->pDevice, &alloc_info, *pSets);
	if (VK_SUCCESS != vk_res)
	{
		VkDescriptorPool pDescriptorPool = VK_NULL_HANDLE;

		VkDescriptorPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCreateInfo.pNext = NULL;
		poolCreateInfo.poolSizeCount = pPool->mPoolSizeCount;
		poolCreateInfo.pPoolSizes = pPool->pPoolSizes;
		poolCreateInfo.flags = pPool->mFlags;
		poolCreateInfo.maxSets = pPool->mNumDescriptorSets;

		VkResult res = vkCreateDescriptorPool(pPool->pDevice, &poolCreateInfo, &gVkAllocationCallbacks, &pDescriptorPool);
		ASSERT(VK_SUCCESS == res);

		pPool->mDescriptorPools.emplace_back(pDescriptorPool);

		pPool->pCurrentPool = pDescriptorPool;
		pPool->mUsedDescriptorSetCount = 0;

		alloc_info.descriptorPool = pPool->pCurrentPool;
		vk_res = vkAllocateDescriptorSets(pPool->pDevice, &alloc_info, *pSets);
	}

	ASSERT(VK_SUCCESS == vk_res);

	pPool->mUsedDescriptorSetCount += numDescriptorSets;
}

/************************************************************************/
/************************************************************************/
VkPipelineBindPoint gPipelineBindPoint[PIPELINE_TYPE_COUNT] = { VK_PIPELINE_BIND_POINT_MAX_ENUM, VK_PIPELINE_BIND_POINT_COMPUTE,
																VK_PIPELINE_BIND_POINT_GRAPHICS,
#ifdef ENABLE_RAYTRACING
																VK_PIPELINE_BIND_POINT_RAY_TRACING_NV
#endif
};

using DescriptorNameToIndexMap = eastl::string_hash_map<uint32_t>;

union DescriptorUpdateData
{
	VkDescriptorImageInfo  mImageInfo;
	VkDescriptorBufferInfo mBufferInfo;
	VkBufferView           mBuferView;
};

struct SizeOffset
{
	uint32_t mSize;
	uint32_t mOffset;
};
/************************************************************************/
// Descriptor Set Structure
/************************************************************************/
typedef struct DescriptorIndexMap
{
	eastl::string_hash_map<uint32_t> mMap;
} DescriptorIndexMap;

static const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName)
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
// Render Pass Implementation
/************************************************************************/
typedef struct RenderPassDesc
{
	TinyImageFormat*      pColorFormats;
	const LoadActionType* pLoadActionsColor;
	bool*                 pSrgbValues;
	uint32_t              mRenderTargetCount;
	SampleCount           mSampleCount;
	TinyImageFormat       mDepthStencilFormat;
	LoadActionType        mLoadActionDepth;
	LoadActionType        mLoadActionStencil;
    bool                  mVRMultiview;
    bool                  mVRFoveatedRendering;
} RenderPassDesc;

typedef struct RenderPass
{
	VkRenderPass   pRenderPass;
	RenderPassDesc mDesc;
} RenderPass;

typedef struct FrameBufferDesc
{
	RenderPass*    pRenderPass;
	RenderTarget** ppRenderTargets;
	RenderTarget*  pDepthStencil;
	uint32_t*      pColorArraySlices;
	uint32_t*      pColorMipSlices;
	uint32_t       mDepthArraySlice;
	uint32_t       mDepthMipSlice;
	uint32_t       mRenderTargetCount;
    bool           mVRFoveatedRendering;
} FrameBufferDesc;

typedef struct FrameBuffer
{
	VkFramebuffer pFramebuffer;
	uint32_t      mWidth;
	uint32_t      mHeight;
	uint32_t      mArraySize;
} FrameBuffer;

static void add_render_pass(Renderer* pRenderer, const RenderPassDesc* pDesc, RenderPass** ppRenderPass)
{
	RenderPass* pRenderPass = (RenderPass*)tf_calloc(1, sizeof(*pRenderPass));
	pRenderPass->mDesc = *pDesc;
	/************************************************************************/
	// Add render pass
	/************************************************************************/
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);

	uint32_t colorAttachmentCount = pDesc->mRenderTargetCount;
	uint32_t depthAttachmentCount = (pDesc->mDepthStencilFormat != TinyImageFormat_UNDEFINED) ? 1 : 0;

	VkAttachmentDescription* attachments = NULL;
	VkAttachmentReference*   color_attachment_refs = NULL;
	VkAttachmentReference*   depth_stencil_attachment_ref = NULL;

	VkSampleCountFlagBits sample_count = util_to_vk_sample_count(pDesc->mSampleCount);

	// Fill out attachment descriptions and references
	{
		attachments = (VkAttachmentDescription*)tf_calloc(colorAttachmentCount + depthAttachmentCount + (int)pDesc->mVRFoveatedRendering, sizeof(*attachments));
		ASSERT(attachments);

		if (colorAttachmentCount > 0)
		{
			color_attachment_refs = (VkAttachmentReference*)tf_calloc(colorAttachmentCount, sizeof(*color_attachment_refs));
			ASSERT(color_attachment_refs);
		}
		if (depthAttachmentCount > 0)
		{
			depth_stencil_attachment_ref = (VkAttachmentReference*)tf_calloc(1, sizeof(*depth_stencil_attachment_ref));
			ASSERT(depth_stencil_attachment_ref);
		}

		// Color
		for (uint32_t i = 0; i < colorAttachmentCount; ++i)
		{
			const uint32_t ssidx = i;

			// descriptions
			attachments[ssidx].flags = 0;
			attachments[ssidx].format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->pColorFormats[i]);
			attachments[ssidx].samples = sample_count;
			attachments[ssidx].loadOp =
				pDesc->pLoadActionsColor ? gVkAttachmentLoadOpTranslator[pDesc->pLoadActionsColor[i]] : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[ssidx].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[ssidx].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[ssidx].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[ssidx].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments[ssidx].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			// references
			color_attachment_refs[i].attachment = ssidx;    //-V522
			color_attachment_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
	}

	// Depth stencil
	if (depthAttachmentCount > 0)
	{
		uint32_t idx = colorAttachmentCount;
		attachments[idx].flags = 0;
		attachments[idx].format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mDepthStencilFormat);
		attachments[idx].samples = sample_count;
		attachments[idx].loadOp = gVkAttachmentLoadOpTranslator[pDesc->mLoadActionDepth];
		attachments[idx].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[idx].stencilLoadOp = gVkAttachmentLoadOpTranslator[pDesc->mLoadActionStencil];
		attachments[idx].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[idx].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[idx].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depth_stencil_attachment_ref[0].attachment = idx;    //-V522
		depth_stencil_attachment_ref[0].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

    uint32_t attachment_count = colorAttachmentCount;
    attachment_count += depthAttachmentCount;

    void* render_pass_next = NULL;
#if defined(QUEST_VR)
    DECLARE_ZERO(VkRenderPassFragmentDensityMapCreateInfoEXT, frag_density_create_info);
    if (pDesc->mVRFoveatedRendering && isFFRFragmentDensityMaskAvailable())
    {
        uint32_t idx = colorAttachmentCount + depthAttachmentCount;
        attachments[idx].flags = 0;
        attachments[idx].format = VK_FORMAT_R8G8_UNORM;
        attachments[idx].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[idx].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[idx].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[idx].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[idx].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[idx].initialLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
        attachments[idx].finalLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;

        frag_density_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT;
        frag_density_create_info.fragmentDensityMapAttachment.attachment = colorAttachmentCount + depthAttachmentCount;
        frag_density_create_info.fragmentDensityMapAttachment.layout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;

        render_pass_next = &frag_density_create_info;
        ++attachment_count;
    }

#endif

	DECLARE_ZERO(VkSubpassDescription, subpass);
	subpass.flags = 0;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = NULL;
	subpass.colorAttachmentCount = colorAttachmentCount;
	subpass.pColorAttachments = color_attachment_refs;
	subpass.pResolveAttachments = NULL;
	subpass.pDepthStencilAttachment = depth_stencil_attachment_ref;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = NULL;

	DECLARE_ZERO(VkRenderPassCreateInfo, create_info);
	create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	create_info.pNext = render_pass_next;
	create_info.flags = 0;
	create_info.attachmentCount = attachment_count;
	create_info.pAttachments = attachments;
	create_info.subpassCount = 1;
	create_info.pSubpasses = &subpass;
	create_info.dependencyCount = 0;
	create_info.pDependencies = NULL;

#if defined(QUEST_VR)
    const uint viewMask = 0b11;
    DECLARE_ZERO(VkRenderPassMultiviewCreateInfo, multiview_create_info);

    if (pDesc->mVRMultiview)
    {
        multiview_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
        multiview_create_info.pNext = create_info.pNext;
        multiview_create_info.subpassCount = 1;
        multiview_create_info.pViewMasks = &viewMask;
        multiview_create_info.dependencyCount = 0;
        multiview_create_info.correlationMaskCount = 1;
        multiview_create_info.pCorrelationMasks = &viewMask;

        create_info.pNext = &multiview_create_info;
    }
#endif

	CHECK_VKRESULT(vkCreateRenderPass(pRenderer->mVulkan.pVkDevice, &create_info, &gVkAllocationCallbacks, &(pRenderPass->pRenderPass)));

	SAFE_FREE(attachments);
	SAFE_FREE(color_attachment_refs);
	SAFE_FREE(depth_stencil_attachment_ref);

	*ppRenderPass = pRenderPass;
}

static void remove_render_pass(Renderer* pRenderer, RenderPass* pRenderPass)
{
	vkDestroyRenderPass(pRenderer->mVulkan.pVkDevice, pRenderPass->pRenderPass, &gVkAllocationCallbacks);
	SAFE_FREE(pRenderPass);
}

static void add_framebuffer(Renderer* pRenderer, const FrameBufferDesc* pDesc, FrameBuffer** ppFrameBuffer)
{
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);

	FrameBuffer* pFrameBuffer = (FrameBuffer*)tf_calloc(1, sizeof(*pFrameBuffer));
	ASSERT(pFrameBuffer);

	uint32_t colorAttachmentCount = pDesc->mRenderTargetCount;
	uint32_t depthAttachmentCount = (pDesc->pDepthStencil) ? 1 : 0;

	if (colorAttachmentCount)
	{
		pFrameBuffer->mWidth = pDesc->ppRenderTargets[0]->mWidth;
		pFrameBuffer->mHeight = pDesc->ppRenderTargets[0]->mHeight;
		if (pDesc->pColorArraySlices)
			pFrameBuffer->mArraySize = 1;
		else
			pFrameBuffer->mArraySize = pDesc->ppRenderTargets[0]->mVRMultiview ? 1 : pDesc->ppRenderTargets[0]->mArraySize;
	}
	else if (depthAttachmentCount)
	{
		pFrameBuffer->mWidth = pDesc->pDepthStencil->mWidth;
		pFrameBuffer->mHeight = pDesc->pDepthStencil->mHeight;
		if (pDesc->mDepthArraySlice != -1)
			pFrameBuffer->mArraySize = 1;
		else
			pFrameBuffer->mArraySize = pDesc->pDepthStencil->mVRMultiview ? 1 : pDesc->pDepthStencil->mArraySize;
	}
	else
	{
		ASSERT(0 && "No color or depth attachments");
	}

	if (colorAttachmentCount && pDesc->ppRenderTargets[0]->mDepth > 1)
	{
		pFrameBuffer->mArraySize = pDesc->ppRenderTargets[0]->mDepth;
	}

	/************************************************************************/
	// Add frame buffer
	/************************************************************************/
	uint32_t attachment_count = colorAttachmentCount;
	attachment_count += depthAttachmentCount;

#if defined(QUEST_VR)
    if (pDesc->mVRFoveatedRendering && isFFRFragmentDensityMaskAvailable())
        ++attachment_count;
#endif

	VkImageView* pImageViews = (VkImageView*)tf_calloc(attachment_count, sizeof(*pImageViews));
	ASSERT(pImageViews);

	VkImageView* iter_attachments = pImageViews;
	// Color
	for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i)
	{
		if (!pDesc->pColorMipSlices && !pDesc->pColorArraySlices)
		{
			*iter_attachments = pDesc->ppRenderTargets[i]->mVulkan.pVkDescriptor;
			++iter_attachments;
		}
		else
		{
			uint32_t handle = 0;
			if (pDesc->pColorMipSlices)
			{
				if (pDesc->pColorArraySlices)
					handle = pDesc->pColorMipSlices[i] * pDesc->ppRenderTargets[i]->mArraySize + pDesc->pColorArraySlices[i];
				else
					handle = pDesc->pColorMipSlices[i];
			}
			else if (pDesc->pColorArraySlices)
			{
				handle = pDesc->pColorArraySlices[i];
			}
			*iter_attachments = pDesc->ppRenderTargets[i]->mVulkan.pVkSliceDescriptors[handle];
			++iter_attachments;
		}
	}
	// Depth/stencil
	if (pDesc->pDepthStencil)
	{
		if (-1 == pDesc->mDepthMipSlice && -1 == pDesc->mDepthArraySlice)
		{
			*iter_attachments = pDesc->pDepthStencil->mVulkan.pVkDescriptor;
			++iter_attachments;
		}
		else
		{
			uint32_t handle = 0;
			if (pDesc->mDepthMipSlice != -1)
			{
				if (pDesc->mDepthArraySlice != -1)
					handle = pDesc->mDepthMipSlice * pDesc->pDepthStencil->mArraySize + pDesc->mDepthArraySlice;
				else
					handle = pDesc->mDepthMipSlice;
			}
			else if (pDesc->mDepthArraySlice != -1)
			{
				handle = pDesc->mDepthArraySlice;
			}
			*iter_attachments = pDesc->pDepthStencil->mVulkan.pVkSliceDescriptors[handle];
			++iter_attachments;
		}
	}

#if defined(QUEST_VR)
    if (pDesc->mVRFoveatedRendering && isFFRFragmentDensityMaskAvailable())
    {
        *iter_attachments = getFFRFragmentDensityMask();
        ++iter_attachments;
    }
#endif

	DECLARE_ZERO(VkFramebufferCreateInfo, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.renderPass = pDesc->pRenderPass->pRenderPass;
	add_info.attachmentCount = attachment_count;
	add_info.pAttachments = pImageViews;
	add_info.width = pFrameBuffer->mWidth;
	add_info.height = pFrameBuffer->mHeight;
	add_info.layers = pFrameBuffer->mArraySize;
	CHECK_VKRESULT(vkCreateFramebuffer(pRenderer->mVulkan.pVkDevice, &add_info, &gVkAllocationCallbacks, &(pFrameBuffer->pFramebuffer)));
	SAFE_FREE(pImageViews);
	/************************************************************************/
	/************************************************************************/

	*ppFrameBuffer = pFrameBuffer;
}

static void remove_framebuffer(Renderer* pRenderer, FrameBuffer* pFrameBuffer)
{
	ASSERT(pRenderer);
	ASSERT(pFrameBuffer);

	vkDestroyFramebuffer(pRenderer->mVulkan.pVkDevice, pFrameBuffer->pFramebuffer, &gVkAllocationCallbacks);
	SAFE_FREE(pFrameBuffer);
}
/************************************************************************/
// Per Thread Render Pass synchronization logic
/************************************************************************/
/// Render-passes are not exposed to the app code since they are not available on all apis
/// This map takes care of hashing a render pass based on the render targets passed to cmdBeginRender
using RenderPassMap = eastl::hash_map<uint64_t, struct RenderPass*>;
using RenderPassMapNode = RenderPassMap::value_type;
using RenderPassMapIt = RenderPassMap::iterator;
using FrameBufferMap = eastl::hash_map<uint64_t, struct FrameBuffer*>;
using FrameBufferMapNode = FrameBufferMap::value_type;
using FrameBufferMapIt = FrameBufferMap::iterator;

// RenderPass map per thread (this will make lookups lock free and we only need a lock when inserting a RenderPass Map for the first time)
eastl::hash_map<ThreadID, RenderPassMap>* gRenderPassMap;
// FrameBuffer map per thread (this will make lookups lock free and we only need a lock when inserting a FrameBuffer map for the first time)
eastl::hash_map<ThreadID, FrameBufferMap>* gFrameBufferMap;
Mutex*                                     pRenderPassMutex;

static RenderPassMap& get_render_pass_map()
{
	// Only need a lock when creating a new renderpass map for this thread
	MutexLock                                          lock(*pRenderPassMutex);
	eastl::hash_map<ThreadID, RenderPassMap>::iterator it = gRenderPassMap->find(getCurrentThreadID());
	if (it == gRenderPassMap->end())
	{
		return gRenderPassMap->insert(getCurrentThreadID()).first->second;
	}
	else
	{
		return it->second;
	}
}

static FrameBufferMap& get_frame_buffer_map()
{
	// Only need a lock when creating a new framebuffer map for this thread
	MutexLock                                           lock(*pRenderPassMutex);
	eastl::hash_map<ThreadID, FrameBufferMap>::iterator it = gFrameBufferMap->find(getCurrentThreadID());
	if (it == gFrameBufferMap->end())
	{
		return gFrameBufferMap->insert(getCurrentThreadID()).first->second;
	}
	else
	{
		return it->second;
	}
}
/************************************************************************/
// Logging, Validation layer implementation
/************************************************************************/
// Proxy log callback
static void internal_log(LogLevel level, const char* msg, const char* component)
{
#ifndef NX64
	LOGF(level, "%s ( %s )", component, msg);
#endif
}

#ifdef USE_DEBUG_UTILS_EXTENSION
// Debug callback for Vulkan layers
static VkBool32 VKAPI_PTR internal_debug_report_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	const char* pLayerPrefix = pCallbackData->pMessageIdName;
	const char* pMessage = pCallbackData->pMessage;
	int32_t     messageCode = pCallbackData->messageIdNumber;
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
	{
		LOGF(LogLevel::eINFO, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		LOGF(LogLevel::eWARNING, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		LOGF(LogLevel::eERROR, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
		ASSERT(false);
	}

	return VK_FALSE;
}
#else
static VKAPI_ATTR VkBool32 VKAPI_CALL internal_debug_report_callback(
	VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode,
	const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
	{
		LOGF(LogLevel::eINFO, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
	}
	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
	{
		LOGF(LogLevel::eWARNING, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
	}
	else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
	{
		LOGF(LogLevel::eWARNING, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
	}
	else if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
	{
		LOGF(LogLevel::eERROR, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
		ASSERT(false);
	}

	return VK_FALSE;
}
#endif
/************************************************************************/
/************************************************************************/
static inline VkPipelineColorBlendStateCreateInfo
	util_to_blend_desc(const BlendStateDesc* pDesc, VkPipelineColorBlendAttachmentState* pAttachments)
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

	for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
	{
		if (pDesc->mRenderTargetMask & (1 << i))
		{
			VkBool32 blendEnable =
				(gVkBlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]] != VK_BLEND_FACTOR_ONE ||
				 gVkBlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]] != VK_BLEND_FACTOR_ZERO ||
				 gVkBlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]] != VK_BLEND_FACTOR_ONE ||
				 gVkBlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]] != VK_BLEND_FACTOR_ZERO);

			pAttachments[i].blendEnable = blendEnable;
			pAttachments[i].colorWriteMask = pDesc->mMasks[blendDescIndex];
			pAttachments[i].srcColorBlendFactor = gVkBlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]];
			pAttachments[i].dstColorBlendFactor = gVkBlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]];
			pAttachments[i].colorBlendOp = gVkBlendOpTranslator[pDesc->mBlendModes[blendDescIndex]];
			pAttachments[i].srcAlphaBlendFactor = gVkBlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]];
			pAttachments[i].dstAlphaBlendFactor = gVkBlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]];
			pAttachments[i].alphaBlendOp = gVkBlendOpTranslator[pDesc->mBlendAlphaModes[blendDescIndex]];
		}

		if (pDesc->mIndependentBlend)
			++blendDescIndex;
	}

	VkPipelineColorBlendStateCreateInfo cb = {};
	cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cb.pNext = NULL;
	cb.flags = 0;
	cb.logicOpEnable = VK_FALSE;
	cb.logicOp = VK_LOGIC_OP_CLEAR;
	cb.pAttachments = pAttachments;
	cb.blendConstants[0] = 0.0f;
	cb.blendConstants[1] = 0.0f;
	cb.blendConstants[2] = 0.0f;
	cb.blendConstants[3] = 0.0f;

	return cb;
}

static inline VkPipelineDepthStencilStateCreateInfo util_to_depth_desc(const DepthStateDesc* pDesc)
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

	VkPipelineDepthStencilStateCreateInfo ds = {};
	ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	ds.pNext = NULL;
	ds.flags = 0;
	ds.depthTestEnable = pDesc->mDepthTest ? VK_TRUE : VK_FALSE;
	ds.depthWriteEnable = pDesc->mDepthWrite ? VK_TRUE : VK_FALSE;
	ds.depthCompareOp = gVkComparisonFuncTranslator[pDesc->mDepthFunc];
	ds.depthBoundsTestEnable = VK_FALSE;
	ds.stencilTestEnable = pDesc->mStencilTest ? VK_TRUE : VK_FALSE;

	ds.front.failOp = gVkStencilOpTranslator[pDesc->mStencilFrontFail];
	ds.front.passOp = gVkStencilOpTranslator[pDesc->mStencilFrontPass];
	ds.front.depthFailOp = gVkStencilOpTranslator[pDesc->mDepthFrontFail];
	ds.front.compareOp = VkCompareOp(pDesc->mStencilFrontFunc);
	ds.front.compareMask = pDesc->mStencilReadMask;
	ds.front.writeMask = pDesc->mStencilWriteMask;
	ds.front.reference = 0;

	ds.back.failOp = gVkStencilOpTranslator[pDesc->mStencilBackFail];
	ds.back.passOp = gVkStencilOpTranslator[pDesc->mStencilBackPass];
	ds.back.depthFailOp = gVkStencilOpTranslator[pDesc->mDepthBackFail];
	ds.back.compareOp = gVkComparisonFuncTranslator[pDesc->mStencilBackFunc];
	ds.back.compareMask = pDesc->mStencilReadMask;
	ds.back.writeMask = pDesc->mStencilWriteMask;    // devsh fixed
	ds.back.reference = 0;

	ds.minDepthBounds = 0;
	ds.maxDepthBounds = 1;

	return ds;
}

static inline VkPipelineRasterizationStateCreateInfo util_to_rasterizer_desc(const RasterizerStateDesc* pDesc)
{
	ASSERT(pDesc->mFillMode < FillMode::MAX_FILL_MODES);
	ASSERT(pDesc->mCullMode < CullMode::MAX_CULL_MODES);
	ASSERT(pDesc->mFrontFace == FRONT_FACE_CCW || pDesc->mFrontFace == FRONT_FACE_CW);

	VkPipelineRasterizationStateCreateInfo rs = {};
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.pNext = NULL;
	rs.flags = 0;
	rs.depthClampEnable = pDesc->mDepthClampEnable ? VK_TRUE : VK_FALSE;
	rs.rasterizerDiscardEnable = VK_FALSE;
	rs.polygonMode = gVkFillModeTranslator[pDesc->mFillMode];
	rs.cullMode = gVkCullModeTranslator[pDesc->mCullMode];
	rs.frontFace = gVkFrontFaceTranslator[pDesc->mFrontFace];
	rs.depthBiasEnable = (pDesc->mDepthBias != 0) ? VK_TRUE : VK_FALSE;
	rs.depthBiasConstantFactor = float(pDesc->mDepthBias);
	rs.depthBiasClamp = 0.0f;
	rs.depthBiasSlopeFactor = pDesc->mSlopeScaledDepthBias;
	rs.lineWidth = 1;

	return rs;
}
/************************************************************************/
// Create default resources to be used a null descriptors in case user does not specify some descriptors
/************************************************************************/
static VkPipelineRasterizationStateCreateInfo gDefaultRasterizerDesc = {};
static VkPipelineDepthStencilStateCreateInfo  gDefaultDepthDesc = {};
static VkPipelineColorBlendStateCreateInfo    gDefaultBlendDesc = {};
static VkPipelineColorBlendAttachmentState    gDefaultBlendAttachments[MAX_RENDER_TARGET_ATTACHMENTS] = {};

typedef struct NullDescriptors
{
	Texture* pDefaultTextureSRV[MAX_LINKED_GPUS][TEXTURE_DIM_COUNT];
	Texture* pDefaultTextureUAV[MAX_LINKED_GPUS][TEXTURE_DIM_COUNT];
	Buffer*  pDefaultBufferSRV[MAX_LINKED_GPUS];
	Buffer*  pDefaultBufferUAV[MAX_LINKED_GPUS];
	Sampler* pDefaultSampler;
	Mutex    mSubmitMutex;

	// #TODO - Remove after we have a better way to specify initial resource state
	// Unlike DX12, Vulkan textures start in undefined layout.
	// With this, we transition them to the specified layout so app code doesn't have to worry about this
	Mutex    mInitialTransitionMutex;
	Queue*   pInitialTransitionQueue;
	CmdPool* pInitialTransitionCmdPool;
	Cmd*     pInitialTransitionCmd;
	Fence*   pInitialTransitionFence;
} NullDescriptors;

static void util_initial_transition(Renderer* pRenderer, Texture* pTexture, ResourceState startState)
{
	acquireMutex(&pRenderer->pNullDescriptors->mInitialTransitionMutex);
	Cmd* cmd = pRenderer->pNullDescriptors->pInitialTransitionCmd;
	resetCmdPool(pRenderer, pRenderer->pNullDescriptors->pInitialTransitionCmdPool);
	beginCmd(cmd);
	TextureBarrier barrier = { pTexture, RESOURCE_STATE_UNDEFINED, startState };
	cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
	endCmd(cmd);
	QueueSubmitDesc submitDesc = {};
	submitDesc.mCmdCount = 1;
	submitDesc.ppCmds = &cmd;
	submitDesc.pSignalFence = pRenderer->pNullDescriptors->pInitialTransitionFence;
	queueSubmit(pRenderer->pNullDescriptors->pInitialTransitionQueue, &submitDesc);
	waitForFences(pRenderer, 1, &pRenderer->pNullDescriptors->pInitialTransitionFence);
	releaseMutex(&pRenderer->pNullDescriptors->mInitialTransitionMutex);
}

static void add_default_resources(Renderer* pRenderer)
{
	pRenderer->pNullDescriptors = (NullDescriptors*)tf_calloc(1, sizeof(NullDescriptors));
	initMutex(&pRenderer->pNullDescriptors->mSubmitMutex);

	for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
	{
		// 1D texture
		TextureDesc textureDesc = {};
		textureDesc.mNodeIndex = i;
		textureDesc.mArraySize = 1;
		textureDesc.mDepth = 1;
		textureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
		textureDesc.mHeight = 1;
		textureDesc.mMipLevels = 1;
		textureDesc.mSampleCount = SAMPLE_COUNT_1;
		textureDesc.mStartState = RESOURCE_STATE_COMMON;
		textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		textureDesc.mWidth = 1;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_1D]);
		textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][TEXTURE_DIM_1D]);

		// 1D texture array
		textureDesc.mArraySize = 2;
		textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_1D_ARRAY]);
		textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][TEXTURE_DIM_1D_ARRAY]);

		// 2D texture
		textureDesc.mWidth = 2;
		textureDesc.mHeight = 2;
		textureDesc.mArraySize = 1;
		textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_2D]);
		textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][TEXTURE_DIM_2D]);

		// 2D MS texture
		textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		textureDesc.mSampleCount = SAMPLE_COUNT_2;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_2DMS]);
		textureDesc.mSampleCount = SAMPLE_COUNT_1;

		// 2D texture array
		textureDesc.mArraySize = 2;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_2D_ARRAY]);
		textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][TEXTURE_DIM_2D_ARRAY]);

		// 2D MS texture array
		textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		textureDesc.mSampleCount = SAMPLE_COUNT_2;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_2DMS_ARRAY]);
		textureDesc.mSampleCount = SAMPLE_COUNT_1;

		// 3D texture
		textureDesc.mDepth = 2;
		textureDesc.mArraySize = 1;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_3D]);
		textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][TEXTURE_DIM_3D]);

		// Cube texture
		textureDesc.mDepth = 1;
		textureDesc.mArraySize = 6;
		textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_CUBE]);
		textureDesc.mArraySize = 6 * 2;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_CUBE_ARRAY]);

		BufferDesc bufferDesc = {};
		bufferDesc.mNodeIndex = i;
		bufferDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufferDesc.mStartState = RESOURCE_STATE_COMMON;
		bufferDesc.mSize = sizeof(uint32_t);
		bufferDesc.mFirstElement = 0;
		bufferDesc.mElementCount = 1;
		bufferDesc.mStructStride = sizeof(uint32_t);
		bufferDesc.mFormat = TinyImageFormat_R32_UINT;
		addBuffer(pRenderer, &bufferDesc, &pRenderer->pNullDescriptors->pDefaultBufferSRV[i]);
		bufferDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
		addBuffer(pRenderer, &bufferDesc, &pRenderer->pNullDescriptors->pDefaultBufferUAV[i]);
	}

	SamplerDesc samplerDesc = {};
	samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_BORDER;
	addSampler(pRenderer, &samplerDesc, &pRenderer->pNullDescriptors->pDefaultSampler);

	BlendStateDesc blendStateDesc = {};
	blendStateDesc.mDstAlphaFactors[0] = BC_ZERO;
	blendStateDesc.mDstFactors[0] = BC_ZERO;
	blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
	blendStateDesc.mSrcFactors[0] = BC_ONE;
	blendStateDesc.mMasks[0] = ALL;
	blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
	blendStateDesc.mIndependentBlend = false;
	gDefaultBlendDesc = util_to_blend_desc(&blendStateDesc, gDefaultBlendAttachments);

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

	// Create command buffer to transition resources to the correct state
	Queue*   graphicsQueue = NULL;
	CmdPool* cmdPool = NULL;
	Cmd*     cmd = NULL;
	Fence*   fence = NULL;

	QueueDesc queueDesc = {};
	queueDesc.mType = QUEUE_TYPE_GRAPHICS;
	addQueue(pRenderer, &queueDesc, &graphicsQueue);

	CmdPoolDesc cmdPoolDesc = {};
	cmdPoolDesc.pQueue = graphicsQueue;
	cmdPoolDesc.mTransient = true;
	addCmdPool(pRenderer, &cmdPoolDesc, &cmdPool);
	CmdDesc cmdDesc = {};
	cmdDesc.pPool = cmdPool;
	addCmd(pRenderer, &cmdDesc, &cmd);

	addFence(pRenderer, &fence);

	pRenderer->pNullDescriptors->pInitialTransitionQueue = graphicsQueue;
	pRenderer->pNullDescriptors->pInitialTransitionCmdPool = cmdPool;
	pRenderer->pNullDescriptors->pInitialTransitionCmd = cmd;
	pRenderer->pNullDescriptors->pInitialTransitionFence = fence;
	initMutex(&pRenderer->pNullDescriptors->mInitialTransitionMutex);

	// Transition resources
	for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
	{
		for (uint32_t dim = 0; dim < TEXTURE_DIM_COUNT; ++dim)
		{
			if (pRenderer->pNullDescriptors->pDefaultTextureSRV[i][dim])
				util_initial_transition(pRenderer, pRenderer->pNullDescriptors->pDefaultTextureSRV[i][dim], RESOURCE_STATE_SHADER_RESOURCE);

			if (pRenderer->pNullDescriptors->pDefaultTextureUAV[i][dim])
				util_initial_transition(
					pRenderer, pRenderer->pNullDescriptors->pDefaultTextureUAV[i][dim], RESOURCE_STATE_UNORDERED_ACCESS);
		}
	}
}

static void remove_default_resources(Renderer* pRenderer)
{
	for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
	{
		for (uint32_t dim = 0; dim < TEXTURE_DIM_COUNT; ++dim)
		{
			if (pRenderer->pNullDescriptors->pDefaultTextureSRV[i][dim])
				removeTexture(pRenderer, pRenderer->pNullDescriptors->pDefaultTextureSRV[i][dim]);

			if (pRenderer->pNullDescriptors->pDefaultTextureUAV[i][dim])
				removeTexture(pRenderer, pRenderer->pNullDescriptors->pDefaultTextureUAV[i][dim]);
		}

		removeBuffer(pRenderer, pRenderer->pNullDescriptors->pDefaultBufferSRV[i]);
		removeBuffer(pRenderer, pRenderer->pNullDescriptors->pDefaultBufferUAV[i]);
	}

	removeSampler(pRenderer, pRenderer->pNullDescriptors->pDefaultSampler);

	removeFence(pRenderer, pRenderer->pNullDescriptors->pInitialTransitionFence);
	removeCmd(pRenderer, pRenderer->pNullDescriptors->pInitialTransitionCmd);
	removeCmdPool(pRenderer, pRenderer->pNullDescriptors->pInitialTransitionCmdPool);
	removeQueue(pRenderer, pRenderer->pNullDescriptors->pInitialTransitionQueue);
	destroyMutex(&pRenderer->pNullDescriptors->mInitialTransitionMutex);

	destroyMutex(&pRenderer->pNullDescriptors->mSubmitMutex);
	SAFE_FREE(pRenderer->pNullDescriptors);
}
/************************************************************************/
// Globals
/************************************************************************/
static tfrg_atomic32_t gRenderTargetIds = 1;
/************************************************************************/
// Internal utility functions
/************************************************************************/
VkFilter util_to_vk_filter(FilterType filter)
{
	switch (filter)
	{
		case FILTER_NEAREST: return VK_FILTER_NEAREST;
		case FILTER_LINEAR: return VK_FILTER_LINEAR;
		default: return VK_FILTER_LINEAR;
	}
}

VkSamplerMipmapMode util_to_vk_mip_map_mode(MipMapMode mipMapMode)
{
	switch (mipMapMode)
	{
		case MIPMAP_MODE_NEAREST: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case MIPMAP_MODE_LINEAR: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		default: ASSERT(false && "Invalid Mip Map Mode"); return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
	}
}

VkSamplerAddressMode util_to_vk_address_mode(AddressMode addressMode)
{
	switch (addressMode)
	{
		case ADDRESS_MODE_MIRROR: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case ADDRESS_MODE_REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case ADDRESS_MODE_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case ADDRESS_MODE_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
}

VkShaderStageFlags util_to_vk_shader_stages(ShaderStage shader_stages)
{
	VkShaderStageFlags result = 0;
	if (SHADER_STAGE_ALL_GRAPHICS == (shader_stages & SHADER_STAGE_ALL_GRAPHICS))
	{
		result = VK_SHADER_STAGE_ALL_GRAPHICS;
	}
	else
	{
		if (SHADER_STAGE_VERT == (shader_stages & SHADER_STAGE_VERT))
		{
			result |= VK_SHADER_STAGE_VERTEX_BIT;
		}
		if (SHADER_STAGE_TESC == (shader_stages & SHADER_STAGE_TESC))
		{
			result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		}
		if (SHADER_STAGE_TESE == (shader_stages & SHADER_STAGE_TESE))
		{
			result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		}
		if (SHADER_STAGE_GEOM == (shader_stages & SHADER_STAGE_GEOM))
		{
			result |= VK_SHADER_STAGE_GEOMETRY_BIT;
		}
		if (SHADER_STAGE_FRAG == (shader_stages & SHADER_STAGE_FRAG))
		{
			result |= VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		if (SHADER_STAGE_COMP == (shader_stages & SHADER_STAGE_COMP))
		{
			result |= VK_SHADER_STAGE_COMPUTE_BIT;
		}
	}
	return result;
}

VkSampleCountFlagBits util_to_vk_sample_count(SampleCount sampleCount)
{
	VkSampleCountFlagBits result = VK_SAMPLE_COUNT_1_BIT;
	switch (sampleCount)
	{
		case SAMPLE_COUNT_1: result = VK_SAMPLE_COUNT_1_BIT; break;
		case SAMPLE_COUNT_2: result = VK_SAMPLE_COUNT_2_BIT; break;
		case SAMPLE_COUNT_4: result = VK_SAMPLE_COUNT_4_BIT; break;
		case SAMPLE_COUNT_8: result = VK_SAMPLE_COUNT_8_BIT; break;
		case SAMPLE_COUNT_16: result = VK_SAMPLE_COUNT_16_BIT; break;
	}
	return result;
}

VkBufferUsageFlags util_to_vk_buffer_usage(DescriptorType usage, bool typed)
{
	VkBufferUsageFlags result = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if (usage & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
	{
		result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}
	if (usage & DESCRIPTOR_TYPE_RW_BUFFER)
	{
		result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (typed)
			result |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
	}
	if (usage & DESCRIPTOR_TYPE_BUFFER)
	{
		result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (typed)
			result |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
	}
	if (usage & DESCRIPTOR_TYPE_INDEX_BUFFER)
	{
		result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}
	if (usage & DESCRIPTOR_TYPE_VERTEX_BUFFER)
	{
		result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}
	if (usage & DESCRIPTOR_TYPE_INDIRECT_BUFFER)
	{
		result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	}
#ifdef ENABLE_RAYTRACING
	if (usage & DESCRIPTOR_TYPE_RAY_TRACING)
	{
		result |= VK_BUFFER_USAGE_RAY_TRACING_BIT_NV;
	}
#endif
	return result;
}

VkImageUsageFlags util_to_vk_image_usage(DescriptorType usage)
{
	VkImageUsageFlags result = 0;
	if (DESCRIPTOR_TYPE_TEXTURE == (usage & DESCRIPTOR_TYPE_TEXTURE))
		result |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if (DESCRIPTOR_TYPE_RW_TEXTURE == (usage & DESCRIPTOR_TYPE_RW_TEXTURE))
		result |= VK_IMAGE_USAGE_STORAGE_BIT;
	return result;
}

VkAccessFlags util_to_vk_access_flags(ResourceState state)
{
	VkAccessFlags ret = 0;
	if (state & RESOURCE_STATE_COPY_SOURCE)
	{
		ret |= VK_ACCESS_TRANSFER_READ_BIT;
	}
	if (state & RESOURCE_STATE_COPY_DEST)
	{
		ret |= VK_ACCESS_TRANSFER_WRITE_BIT;
	}
	if (state & RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
	{
		ret |= VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	}
	if (state & RESOURCE_STATE_INDEX_BUFFER)
	{
		ret |= VK_ACCESS_INDEX_READ_BIT;
	}
	if (state & RESOURCE_STATE_UNORDERED_ACCESS)
	{
		ret |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	}
	if (state & RESOURCE_STATE_INDIRECT_ARGUMENT)
	{
		ret |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	}
	if (state & RESOURCE_STATE_RENDER_TARGET)
	{
		ret |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	}
	if (state & RESOURCE_STATE_DEPTH_WRITE)
	{
		ret |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}
	if (state & RESOURCE_STATE_SHADER_RESOURCE)
	{
		ret |= VK_ACCESS_SHADER_READ_BIT;
	}
	if (state & RESOURCE_STATE_PRESENT)
	{
		ret |= VK_ACCESS_MEMORY_READ_BIT;
	}
#if defined(QUEST_VR)
    if (state & RESOURCE_STATE_SHADING_RATE_SOURCE)
    {
        ret |= VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
    }
#endif

#ifdef ENABLE_RAYTRACING
	if (state & RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
	{
		ret |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV;
	}
#endif

	return ret;
}

VkImageLayout util_to_vk_image_layout(ResourceState usage)
{
	if (usage & RESOURCE_STATE_COPY_SOURCE)
		return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	if (usage & RESOURCE_STATE_COPY_DEST)
		return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	if (usage & RESOURCE_STATE_RENDER_TARGET)
		return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	if (usage & RESOURCE_STATE_DEPTH_WRITE)
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	if (usage & RESOURCE_STATE_UNORDERED_ACCESS)
		return VK_IMAGE_LAYOUT_GENERAL;

	if (usage & RESOURCE_STATE_SHADER_RESOURCE)
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	if (usage & RESOURCE_STATE_PRESENT)
		return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	if (usage == RESOURCE_STATE_COMMON)
		return VK_IMAGE_LAYOUT_GENERAL;

#if defined(QUEST_VR)
    if (usage == RESOURCE_STATE_SHADING_RATE_SOURCE)
        return VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
#endif

	return VK_IMAGE_LAYOUT_UNDEFINED;
}

void util_get_planar_vk_image_memory_requirement(
	VkDevice device, VkImage image, uint32_t planesCount, VkMemoryRequirements* outVkMemReq, uint64_t* outPlanesOffsets)
{
	outVkMemReq->size = 0;
	outVkMemReq->alignment = 0;
	outVkMemReq->memoryTypeBits = 0;

	VkImagePlaneMemoryRequirementsInfo imagePlaneMemReqInfo = { VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO, NULL };

	VkImageMemoryRequirementsInfo2 imagePlaneMemReqInfo2 = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2 };
	imagePlaneMemReqInfo2.pNext = &imagePlaneMemReqInfo;
	imagePlaneMemReqInfo2.image = image;

	VkMemoryDedicatedRequirements memDedicatedReq = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, NULL };
	VkMemoryRequirements2         memReq2 = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
	memReq2.pNext = &memDedicatedReq;

	for (uint32_t i = 0; i < planesCount; ++i)
	{
		imagePlaneMemReqInfo.planeAspect = (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_PLANE_0_BIT << i);
		vkGetImageMemoryRequirements2(device, &imagePlaneMemReqInfo2, &memReq2);

		outPlanesOffsets[i] += outVkMemReq->size;
		outVkMemReq->alignment = max(memReq2.memoryRequirements.alignment, outVkMemReq->alignment);
		outVkMemReq->size += round_up_64(memReq2.memoryRequirements.size, memReq2.memoryRequirements.alignment);
		outVkMemReq->memoryTypeBits |= memReq2.memoryRequirements.memoryTypeBits;
	}
}

uint32_t util_get_memory_type(
	uint32_t typeBits, const VkPhysicalDeviceMemoryProperties& memoryProperties, const VkMemoryPropertyFlags& properties,
	VkBool32* memTypeFound = nullptr)
{
	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		if ((typeBits & 1) == 1)
		{
			if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				if (memTypeFound)
				{
					*memTypeFound = true;
				}
				return i;
			}
		}
		typeBits >>= 1;
	}

	if (memTypeFound)
	{
		*memTypeFound = false;
		return 0;
	}
	else
	{
		LOGF(LogLevel::eERROR, "Could not find a matching memory type");
		ASSERT(0);
		return 0;
	}
}

// Determines pipeline stages involved for given accesses
VkPipelineStageFlags util_determine_pipeline_stage_flags(Renderer* pRenderer, VkAccessFlags accessFlags, QueueType queueType)
{
	VkPipelineStageFlags flags = 0;

	switch (queueType)
	{
		case QUEUE_TYPE_GRAPHICS:
		{
			if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

			if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
			{
				flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
				flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				if (pRenderer->pActiveGpuSettings->mGeometryShaderSupported)
				{
					flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
				}
				if (pRenderer->pActiveGpuSettings->mTessellationSupported)
				{
					flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
					flags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
				}
				flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
#ifdef ENABLE_RAYTRACING
				if (pRenderer->mVulkan.mRaytracingExtension)
				{
					flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV;
				}
#endif
			}

			if ((accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0)
				flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

			if ((accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			if ((accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

#if defined(QUEST_VR)
            if ((accessFlags & VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT) != 0)
                flags |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
#endif
			break;
		}
		case QUEUE_TYPE_COMPUTE:
		{
			if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0 ||
				(accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0 ||
				(accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0 ||
				(accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
				return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

			if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			break;
		}
		case QUEUE_TYPE_TRANSFER: return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		default: break;
	}

	// Compatible with both compute and graphics queues
	if ((accessFlags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0)
		flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

	if ((accessFlags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
		flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;

	if ((accessFlags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT)) != 0)
		flags |= VK_PIPELINE_STAGE_HOST_BIT;

	if (flags == 0)
		flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	return flags;
}

VkImageAspectFlags util_vk_determine_aspect_mask(VkFormat format, bool includeStencilBit)
{
	VkImageAspectFlags result = 0;
	switch (format)
	{
			// Depth
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
		case VK_FORMAT_D32_SFLOAT:
			result = VK_IMAGE_ASPECT_DEPTH_BIT;
			break;
			// Stencil
		case VK_FORMAT_S8_UINT:
			result = VK_IMAGE_ASPECT_STENCIL_BIT;
			break;
			// Depth/stencil
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			result = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (includeStencilBit)
				result |= VK_IMAGE_ASPECT_STENCIL_BIT;
			break;
			// Assume everything else is Color
		default: result = VK_IMAGE_ASPECT_COLOR_BIT; break;
	}
	return result;
}

VkFormatFeatureFlags util_vk_image_usage_to_format_features(VkImageUsageFlags usage)
{
	VkFormatFeatureFlags result = (VkFormatFeatureFlags)0;
	if (VK_IMAGE_USAGE_SAMPLED_BIT == (usage & VK_IMAGE_USAGE_SAMPLED_BIT))
	{
		result |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
	}
	if (VK_IMAGE_USAGE_STORAGE_BIT == (usage & VK_IMAGE_USAGE_STORAGE_BIT))
	{
		result |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
	}
	if (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT == (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
	{
		result |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
	}
	if (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT == (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
	{
		result |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
	return result;
}

VkQueueFlags util_to_vk_queue_flags(QueueType queueType)
{
	switch (queueType)
	{
		case QUEUE_TYPE_GRAPHICS: return VK_QUEUE_GRAPHICS_BIT;
		case QUEUE_TYPE_TRANSFER: return VK_QUEUE_TRANSFER_BIT;
		case QUEUE_TYPE_COMPUTE: return VK_QUEUE_COMPUTE_BIT;
		default: ASSERT(false && "Invalid Queue Type"); return VK_QUEUE_FLAG_BITS_MAX_ENUM;
	}
}

VkDescriptorType util_to_vk_descriptor_type(DescriptorType type)
{
	switch (type)
	{
		case DESCRIPTOR_TYPE_UNDEFINED: ASSERT("Invalid DescriptorInfo Type"); return VK_DESCRIPTOR_TYPE_MAX_ENUM;
		case DESCRIPTOR_TYPE_SAMPLER: return VK_DESCRIPTOR_TYPE_SAMPLER;
		case DESCRIPTOR_TYPE_TEXTURE: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		case DESCRIPTOR_TYPE_UNIFORM_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case DESCRIPTOR_TYPE_RW_TEXTURE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		case DESCRIPTOR_TYPE_BUFFER:
		case DESCRIPTOR_TYPE_RW_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		case DESCRIPTOR_TYPE_TEXEL_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		case DESCRIPTOR_TYPE_RW_TEXEL_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
#ifdef ENABLE_RAYTRACING
		case DESCRIPTOR_TYPE_RAY_TRACING: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
#endif
		default:
			ASSERT("Invalid DescriptorInfo Type");
			return VK_DESCRIPTOR_TYPE_MAX_ENUM;
			break;
	}
}

VkShaderStageFlags util_to_vk_shader_stage_flags(ShaderStage stages)
{
	VkShaderStageFlags res = 0;
	if (stages & SHADER_STAGE_ALL_GRAPHICS)
		return VK_SHADER_STAGE_ALL_GRAPHICS;

	if (stages & SHADER_STAGE_VERT)
		res |= VK_SHADER_STAGE_VERTEX_BIT;
	if (stages & SHADER_STAGE_GEOM)
		res |= VK_SHADER_STAGE_GEOMETRY_BIT;
	if (stages & SHADER_STAGE_TESE)
		res |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	if (stages & SHADER_STAGE_TESC)
		res |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	if (stages & SHADER_STAGE_COMP)
		res |= VK_SHADER_STAGE_COMPUTE_BIT;
#ifdef ENABLE_RAYTRACING
	if (stages & SHADER_STAGE_RAYTRACING)
		res |=
			(VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
			 VK_SHADER_STAGE_MISS_BIT_NV | VK_SHADER_STAGE_INTERSECTION_BIT_NV | VK_SHADER_STAGE_CALLABLE_BIT_NV);
#endif

	ASSERT(res != 0);
	return res;
}

void util_find_queue_family_index(
	const Renderer* pRenderer, uint32_t nodeIndex, QueueType queueType, VkQueueFamilyProperties* pOutProps, uint8_t* pOutFamilyIndex,
	uint8_t* pOutQueueIndex)
{
	uint32_t     queueFamilyIndex = UINT32_MAX;
	uint32_t     queueIndex = UINT32_MAX;
	VkQueueFlags requiredFlags = util_to_vk_queue_flags(queueType);
	bool         found = false;

	// Get queue family properties
	uint32_t                 queueFamilyPropertyCount = 0;
	VkQueueFamilyProperties* queueFamilyProperties = NULL;
	vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->mVulkan.pVkActiveGPU, &queueFamilyPropertyCount, NULL);
	queueFamilyProperties = (VkQueueFamilyProperties*)alloca(queueFamilyPropertyCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->mVulkan.pVkActiveGPU, &queueFamilyPropertyCount, queueFamilyProperties);

	uint32_t minQueueFlag = UINT32_MAX;

	// Try to find a dedicated queue of this type
	for (uint32_t index = 0; index < queueFamilyPropertyCount; ++index)
	{
		VkQueueFlags queueFlags = queueFamilyProperties[index].queueFlags;
		bool         graphicsQueue = (queueFlags & VK_QUEUE_GRAPHICS_BIT) ? true : false;
		uint32_t     flagAnd = (queueFlags & requiredFlags);
		if (queueType == QUEUE_TYPE_GRAPHICS && graphicsQueue)
		{
			found = true;
			queueFamilyIndex = index;
			queueIndex = 0;
			break;
		}
		if ((queueFlags & requiredFlags) && ((queueFlags & ~requiredFlags) == 0) &&
			pRenderer->mVulkan.pUsedQueueCount[nodeIndex][queueFlags] < pRenderer->mVulkan.pAvailableQueueCount[nodeIndex][queueFlags])
		{
			found = true;
			queueFamilyIndex = index;
			queueIndex = pRenderer->mVulkan.pUsedQueueCount[nodeIndex][queueFlags];
			break;
		}
		if (flagAnd && ((queueFlags - flagAnd) < minQueueFlag) && !graphicsQueue &&
			pRenderer->mVulkan.pUsedQueueCount[nodeIndex][queueFlags] < pRenderer->mVulkan.pAvailableQueueCount[nodeIndex][queueFlags])
		{
			found = true;
			minQueueFlag = (queueFlags - flagAnd);
			queueFamilyIndex = index;
			queueIndex = pRenderer->mVulkan.pUsedQueueCount[nodeIndex][queueFlags];
			break;
		}
	}

	// If hardware doesn't provide a dedicated queue try to find a non-dedicated one
	if (!found)
	{
		for (uint32_t index = 0; index < queueFamilyPropertyCount; ++index)
		{
			VkQueueFlags queueFlags = queueFamilyProperties[index].queueFlags;
			if ((queueFlags & requiredFlags) &&
				pRenderer->mVulkan.pUsedQueueCount[nodeIndex][queueFlags] < pRenderer->mVulkan.pAvailableQueueCount[nodeIndex][queueFlags])
			{
				found = true;
				queueFamilyIndex = index;
				queueIndex = pRenderer->mVulkan.pUsedQueueCount[nodeIndex][queueFlags];
				break;
			}
		}
	}

	if (!found)
	{
		found = true;
		queueFamilyIndex = 0;
		queueIndex = 0;

		LOGF(LogLevel::eWARNING, "Could not find queue of type %u. Using default queue", (uint32_t)queueType);
	}

	if (pOutProps)
		*pOutProps = queueFamilyProperties[queueFamilyIndex];
	if (pOutFamilyIndex)
		*pOutFamilyIndex = (uint8_t)queueFamilyIndex;
	if (pOutQueueIndex)
		*pOutQueueIndex = (uint8_t)queueIndex;
}

static VkPipelineCacheCreateFlags util_to_pipeline_cache_flags(PipelineCacheFlags flags)
{
	VkPipelineCacheCreateFlags ret = 0;
#if VK_EXT_pipeline_creation_cache_control
	if (flags & PIPELINE_CACHE_FLAG_EXTERNALLY_SYNCHRONIZED)
	{
		ret |= VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT_EXT;
	}
#endif

	return ret;
}
/************************************************************************/
// Multi GPU Helper Functions
/************************************************************************/
uint32_t util_calculate_shared_device_mask(uint32_t gpuCount) { return (1 << gpuCount) - 1; }

void util_calculate_device_indices(
	Renderer* pRenderer, uint32_t nodeIndex, uint32_t* pSharedNodeIndices, uint32_t sharedNodeIndexCount, uint32_t* pIndices)
{
	for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
		pIndices[i] = i;

	pIndices[nodeIndex] = nodeIndex;
	/************************************************************************/
	// Set the node indices which need sharing access to the creation node
	// Example: Texture created on GPU0 but GPU1 will need to access it, GPU2 does not care
	//		  pIndices = { 0, 0, 2 }
	/************************************************************************/
	for (uint32_t i = 0; i < sharedNodeIndexCount; ++i)
		pIndices[pSharedNodeIndices[i]] = nodeIndex;
}
/************************************************************************/
// Internal init functions
/************************************************************************/
void CreateInstance(
	const char* app_name, const RendererDesc* pDesc, uint32_t userDefinedInstanceLayerCount, const char** userDefinedInstanceLayers,
	Renderer* pRenderer)
{
	// These are the extensions that we have loaded
	const char* instanceExtensionCache[MAX_INSTANCE_EXTENSIONS] = {};

	uint32_t layerCount = 0;
	uint32_t extCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, NULL);
	vkEnumerateInstanceExtensionProperties(NULL, &extCount, NULL);

	VkLayerProperties* layers = (VkLayerProperties*)alloca(sizeof(VkLayerProperties) * layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, layers);

	VkExtensionProperties* exts = (VkExtensionProperties*)alloca(sizeof(VkExtensionProperties) * extCount);
	vkEnumerateInstanceExtensionProperties(NULL, &extCount, exts);

	for (uint32_t i = 0; i < layerCount; ++i)
	{
		internal_log(eINFO, layers[i].layerName, "vkinstance-layer");
	}

	for (uint32_t i = 0; i < extCount; ++i)
	{
		internal_log(eINFO, exts[i].extensionName, "vkinstance-ext");
	}

	DECLARE_ZERO(VkApplicationInfo, app_info);
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pNext = NULL;
	app_info.pApplicationName = app_name;
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "TheForge";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
#if defined(ANDROID) && !defined(QUEST_VR)
	app_info.apiVersion = VK_API_VERSION_1_0;
#else
	app_info.apiVersion = VK_API_VERSION_1_1;
#endif

	eastl::vector<const char*> layerTemp = eastl::vector<const char*>(userDefinedInstanceLayerCount);
	memcpy(layerTemp.data(), userDefinedInstanceLayers, layerTemp.size() * sizeof(char*));

	// Instance
	{
		// check to see if the layers are present
		for (uint32_t i = 0; i < (uint32_t)layerTemp.size(); ++i)
		{
			bool layerFound = false;
			for (uint32_t j = 0; j < layerCount; ++j)
			{
				if (strcmp(userDefinedInstanceLayers[i], layers[j].layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}
			if (layerFound == false)
			{
				internal_log(eWARNING, userDefinedInstanceLayers[i], "vkinstance-layer-missing");
				// delete layer and get new index
				i = (uint32_t)(layerTemp.erase(layerTemp.begin() + i) - layerTemp.begin());
			}
		}

		uint32_t                   extension_count = 0;
		const uint32_t             initialCount = sizeof(gVkWantedInstanceExtensions) / sizeof(gVkWantedInstanceExtensions[0]);
		const uint32_t             userRequestedCount = (uint32_t)pDesc->mVulkan.mInstanceExtensionCount;
		eastl::vector<const char*> wantedInstanceExtensions(initialCount + userRequestedCount);
		for (uint32_t i = 0; i < initialCount; ++i)
		{
			wantedInstanceExtensions[i] = gVkWantedInstanceExtensions[i];
		}
		for (uint32_t i = 0; i < userRequestedCount; ++i)
		{
			wantedInstanceExtensions[initialCount + i] = pDesc->mVulkan.ppInstanceExtensions[i];
		}
		const uint32_t wanted_extension_count = (uint32_t)wantedInstanceExtensions.size();
		// Layer extensions
		for (size_t i = 0; i < layerTemp.size(); ++i)
		{
			const char* layer_name = layerTemp[i];
			uint32_t    count = 0;
			vkEnumerateInstanceExtensionProperties(layer_name, &count, NULL);
			VkExtensionProperties* properties = count ? (VkExtensionProperties*)tf_calloc(count, sizeof(*properties)) : NULL;
			ASSERT(properties != NULL || count == 0);
			vkEnumerateInstanceExtensionProperties(layer_name, &count, properties);
			for (uint32_t j = 0; j < count; ++j)
			{
				for (uint32_t k = 0; k < wanted_extension_count; ++k)
				{
					if (strcmp(wantedInstanceExtensions[k], properties[j].extensionName) == 0)    //-V522
					{
						if (strcmp(wantedInstanceExtensions[k], VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME) == 0)
							gDeviceGroupCreationExtension = true;
#ifdef USE_DEBUG_UTILS_EXTENSION
						if (strcmp(wantedInstanceExtensions[k], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
							gDebugUtilsExtension = true;
#endif
						instanceExtensionCache[extension_count++] = wantedInstanceExtensions[k];
						// clear wanted extension so we dont load it more then once
						wantedInstanceExtensions[k] = "";
						break;
					}
				}
			}
			SAFE_FREE((void*)properties);
		}
		// Standalone extensions
		{
			const char* layer_name = NULL;
			uint32_t    count = 0;
			vkEnumerateInstanceExtensionProperties(layer_name, &count, NULL);
			if (count > 0)
			{
				VkExtensionProperties* properties = (VkExtensionProperties*)tf_calloc(count, sizeof(*properties));
				ASSERT(properties != NULL);
				vkEnumerateInstanceExtensionProperties(layer_name, &count, properties);
				for (uint32_t j = 0; j < count; ++j)
				{
					for (uint32_t k = 0; k < wanted_extension_count; ++k)
					{
						if (strcmp(wantedInstanceExtensions[k], properties[j].extensionName) == 0)
						{
							instanceExtensionCache[extension_count++] = wantedInstanceExtensions[k];
							// clear wanted extension so we dont load it more then once
							//gVkWantedInstanceExtensions[k] = "";
							if (strcmp(wantedInstanceExtensions[k], VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME) == 0)
								gDeviceGroupCreationExtension = true;
#ifdef USE_DEBUG_UTILS_EXTENSION
							if (strcmp(wantedInstanceExtensions[k], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
								gDebugUtilsExtension = true;
#endif
							break;
						}
					}
				}
				SAFE_FREE((void*)properties);
			}
		}

#if defined(QUEST_VR)
        char oculusVRInstanceExtensionBuffer[4096];
        hook_add_vk_instance_extensions(instanceExtensionCache, &extension_count, MAX_INSTANCE_EXTENSIONS, oculusVRInstanceExtensionBuffer, sizeof(oculusVRInstanceExtensionBuffer));
#endif

#if VK_HEADER_VERSION >= 108
		VkValidationFeaturesEXT      validationFeaturesExt = { VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
		VkValidationFeatureEnableEXT enabledValidationFeatures[] = {
			VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
		};

		if (pDesc->mEnableGPUBasedValidation)
		{
			validationFeaturesExt.enabledValidationFeatureCount = 1;
			validationFeaturesExt.pEnabledValidationFeatures = enabledValidationFeatures;
		}
#endif

		// Add more extensions here
		DECLARE_ZERO(VkInstanceCreateInfo, create_info);
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
#if VK_HEADER_VERSION >= 108
		create_info.pNext = &validationFeaturesExt;
#endif
		create_info.flags = 0;
		create_info.pApplicationInfo = &app_info;
		create_info.enabledLayerCount = (uint32_t)layerTemp.size();
		create_info.ppEnabledLayerNames = layerTemp.data();
		create_info.enabledExtensionCount = extension_count;
		create_info.ppEnabledExtensionNames = instanceExtensionCache;
		CHECK_VKRESULT(vkCreateInstance(&create_info, &gVkAllocationCallbacks, &(pRenderer->mVulkan.pVkInstance)));
	}

#if defined(NX64)
	loadExtensionsNX(pRenderer->mVulkan.pVkInstance);
#else
	// Load Vulkan instance functions
	volkLoadInstance(pRenderer->mVulkan.pVkInstance);
#endif

	// Debug
	{
#ifdef USE_DEBUG_UTILS_EXTENSION
		if (gDebugUtilsExtension)
		{
			VkDebugUtilsMessengerCreateInfoEXT create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			create_info.pfnUserCallback = internal_debug_report_callback;
			create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
									  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			create_info.flags = 0;
			create_info.pUserData = NULL;
			VkResult res = vkCreateDebugUtilsMessengerEXT(
				pRenderer->mVulkan.pVkInstance, &create_info, &gVkAllocationCallbacks, &(pRenderer->mVulkan.pVkDebugUtilsMessenger));
			if (VK_SUCCESS != res)
			{
				internal_log(
					eERROR, "vkCreateDebugUtilsMessengerEXT failed - disabling Vulkan debug callbacks",
					"internal_vk_init_instance");
			}
		}
#else
#if defined(__ANDROID__)
		if (vkCreateDebugReportCallbackEXT)
#endif
		{
			DECLARE_ZERO(VkDebugReportCallbackCreateInfoEXT, create_info);
			create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
			create_info.pNext = NULL;
			create_info.pfnCallback = internal_debug_report_callback;
			create_info.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
#if defined(NX64) || defined(__ANDROID__)
								VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |    // Performance warnings are not very vaild on desktop
#endif
								VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT /* | VK_DEBUG_REPORT_INFORMATION_BIT_EXT*/;
			VkResult res = vkCreateDebugReportCallbackEXT(
				pRenderer->mVulkan.pVkInstance, &create_info, &gVkAllocationCallbacks, &(pRenderer->mVulkan.pVkDebugReport));
			if (VK_SUCCESS != res)
			{
				internal_log(
					eERROR, "vkCreateDebugReportCallbackEXT failed - disabling Vulkan debug callbacks",
					"internal_vk_init_instance");
			}
		}
#endif
	}
}

static void RemoveInstance(Renderer* pRenderer)
{
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkInstance);

#ifdef USE_DEBUG_UTILS_EXTENSION
	if (pRenderer->mVulkan.pVkDebugUtilsMessenger)
	{
		vkDestroyDebugUtilsMessengerEXT(pRenderer->mVulkan.pVkInstance, pRenderer->mVulkan.pVkDebugUtilsMessenger, &gVkAllocationCallbacks);
		pRenderer->mVulkan.pVkDebugUtilsMessenger = NULL;
	}
#else
	if (pRenderer->mVulkan.pVkDebugReport)
	{
		vkDestroyDebugReportCallbackEXT(pRenderer->mVulkan.pVkInstance, pRenderer->mVulkan.pVkDebugReport, &gVkAllocationCallbacks);
		pRenderer->mVulkan.pVkDebugReport = NULL;
	}
#endif

	vkDestroyInstance(pRenderer->mVulkan.pVkInstance, &gVkAllocationCallbacks);
}

static bool AddDevice(const RendererDesc* pDesc, Renderer* pRenderer)
{
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkInstance);

	// These are the extensions that we have loaded
	const char* deviceExtensionCache[MAX_DEVICE_EXTENSIONS] = {};
	VkResult    vk_res = VK_RESULT_MAX_ENUM;

#if VK_KHR_device_group_creation
	VkDeviceGroupDeviceCreateInfoKHR   deviceGroupInfo = { VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO_KHR };
	VkPhysicalDeviceGroupPropertiesKHR props[MAX_LINKED_GPUS] = {};

	pRenderer->mLinkedNodeCount = 1;
	if (pRenderer->mGpuMode == GPU_MODE_LINKED && gDeviceGroupCreationExtension)
	{
		// (not shown) fill out devCreateInfo as usual.
		uint32_t deviceGroupCount = 0;

		// Query the number of device groups
		vkEnumeratePhysicalDeviceGroupsKHR(pRenderer->mVulkan.pVkInstance, &deviceGroupCount, NULL);

		// Allocate and initialize structures to query the device groups
		for (uint32_t i = 0; i < deviceGroupCount; ++i)
		{
			props[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES_KHR;
			props[i].pNext = NULL;
		}
		vk_res = vkEnumeratePhysicalDeviceGroupsKHR(pRenderer->mVulkan.pVkInstance, &deviceGroupCount, props);
		ASSERT(VK_SUCCESS == vk_res);

		// If the first device group has more than one physical device. create
		// a logical device using all of the physical devices.
		for (uint32_t i = 0; i < deviceGroupCount; ++i)
		{
			if (props[i].physicalDeviceCount > 1)
			{
				deviceGroupInfo.physicalDeviceCount = props[i].physicalDeviceCount;
				deviceGroupInfo.pPhysicalDevices = props[i].physicalDevices;
				pRenderer->mLinkedNodeCount = deviceGroupInfo.physicalDeviceCount;
				break;
			}
		}
	}
#endif

	if (pRenderer->mLinkedNodeCount < 2)
	{
		pRenderer->mGpuMode = GPU_MODE_SINGLE;
	}

	uint32_t gpuCount = 0;

	vk_res = vkEnumeratePhysicalDevices(pRenderer->mVulkan.pVkInstance, &gpuCount, NULL);
	ASSERT(VK_SUCCESS == vk_res);

	if (gpuCount < 1)
	{
		LOGF(LogLevel::eERROR, "Failed to enumerate any physical Vulkan devices");
		ASSERT(gpuCount);
		return false;
	}

	VkPhysicalDevice*                 gpus = (VkPhysicalDevice*)alloca(gpuCount * sizeof(VkPhysicalDevice));
	VkPhysicalDeviceProperties2*      gpuProperties = (VkPhysicalDeviceProperties2*)alloca(gpuCount * sizeof(VkPhysicalDeviceProperties2));
	VkPhysicalDeviceMemoryProperties* gpuMemoryProperties =
		(VkPhysicalDeviceMemoryProperties*)alloca(gpuCount * sizeof(VkPhysicalDeviceMemoryProperties));
	VkPhysicalDeviceFeatures2KHR* gpuFeatures = (VkPhysicalDeviceFeatures2KHR*)alloca(gpuCount * sizeof(VkPhysicalDeviceFeatures2KHR));
	VkQueueFamilyProperties**     queueFamilyProperties = (VkQueueFamilyProperties**)alloca(gpuCount * sizeof(VkQueueFamilyProperties*));
	uint32_t*                     queueFamilyPropertyCount = (uint32_t*)alloca(gpuCount * sizeof(uint32_t));

	vk_res = vkEnumeratePhysicalDevices(pRenderer->mVulkan.pVkInstance, &gpuCount, gpus);
	ASSERT(VK_SUCCESS == vk_res);
	/************************************************************************/
	// Select discrete gpus first
	// If we have multiple discrete gpus prefer with bigger VRAM size
	// To find VRAM in Vulkan, loop through all the heaps and find if the
	// heap has the DEVICE_LOCAL_BIT flag set
	/************************************************************************/
	typedef bool (*DeviceBetterFunc)(
		uint32_t, uint32_t, const GPUSettings*, const VkPhysicalDeviceProperties2*, const VkPhysicalDeviceMemoryProperties*);
	DeviceBetterFunc isDeviceBetter = [](uint32_t testIndex, uint32_t refIndex, const GPUSettings* gpuSettings,
										 const VkPhysicalDeviceProperties2*      gpuProperties,
										 const VkPhysicalDeviceMemoryProperties* gpuMemoryProperties) {
		const GPUSettings& testSettings = gpuSettings[testIndex];
		const GPUSettings& refSettings = gpuSettings[refIndex];

		// First test the preset level
		if (testSettings.mGpuVendorPreset.mPresetLevel != refSettings.mGpuVendorPreset.mPresetLevel)
		{
			return testSettings.mGpuVendorPreset.mPresetLevel > refSettings.mGpuVendorPreset.mPresetLevel;
		}

		// Next test discrete vs integrated/software
		const VkPhysicalDeviceProperties& testProps = gpuProperties[testIndex].properties;
		const VkPhysicalDeviceProperties& refProps = gpuProperties[refIndex].properties;

		// If first is a discrete gpu and second is not discrete (integrated, software, ...), always prefer first
		if (testProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && refProps.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			return true;
		}

		// If first is not a discrete gpu (integrated, software, ...) and second is a discrete gpu, always prefer second
		if (testProps.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && refProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			return false;
		}

		// Compare by VRAM if both gpu's are of same type (integrated vs discrete)
		if (testProps.vendorID == refProps.vendorID && testProps.deviceID == refProps.deviceID)
		{
			const VkPhysicalDeviceMemoryProperties& testMemoryProps = gpuMemoryProperties[testIndex];
			const VkPhysicalDeviceMemoryProperties& refMemoryProps = gpuMemoryProperties[refIndex];
			//if presets are the same then sort by vram size
			VkDeviceSize totalTestVram = 0;
			VkDeviceSize totalRefVram = 0;
			for (uint32_t i = 0; i < testMemoryProps.memoryHeapCount; ++i)
			{
				if (VK_MEMORY_HEAP_DEVICE_LOCAL_BIT & testMemoryProps.memoryHeaps[i].flags)
					totalTestVram += testMemoryProps.memoryHeaps[i].size;
			}
			for (uint32_t i = 0; i < refMemoryProps.memoryHeapCount; ++i)
			{
				if (VK_MEMORY_HEAP_DEVICE_LOCAL_BIT & refMemoryProps.memoryHeaps[i].flags)
					totalRefVram += refMemoryProps.memoryHeaps[i].size;
			}

			return totalTestVram >= totalRefVram;
		}

		return false;
	};

	uint32_t     gpuIndex = UINT32_MAX;
	GPUSettings* gpuSettings = (GPUSettings*)alloca(gpuCount * sizeof(GPUSettings));

	for (uint32_t i = 0; i < gpuCount; ++i)
	{
		gpuProperties[i] = {};
		gpuMemoryProperties[i] = {};
		gpuFeatures[i] = {};
		queueFamilyProperties[i] = NULL;
		queueFamilyPropertyCount[i] = 0;

		// Get memory properties
		vkGetPhysicalDeviceMemoryProperties(gpus[i], &gpuMemoryProperties[i]);

		// Get features
		gpuFeatures[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;

#if VK_EXT_fragment_shader_interlock
		VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragmentShaderInterlockFeatures = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT
		};
		gpuFeatures[i].pNext = &fragmentShaderInterlockFeatures;
#endif
		vkGetPhysicalDeviceFeatures2KHR(gpus[i], &gpuFeatures[i]);

		// Get device properties
		VkPhysicalDeviceSubgroupProperties subgroupProperties = {};
		subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
		subgroupProperties.pNext = NULL;
		gpuProperties[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
		subgroupProperties.pNext = gpuProperties[i].pNext;
		gpuProperties[i].pNext = &subgroupProperties;
#ifdef ANDROID
		vkGetPhysicalDeviceProperties2KHR(gpus[i], &gpuProperties[i]);
#else
		vkGetPhysicalDeviceProperties2(gpus[i], &gpuProperties[i]);
#endif

		// Get queue family properties
		vkGetPhysicalDeviceQueueFamilyProperties(gpus[i], &queueFamilyPropertyCount[i], NULL);
		queueFamilyProperties[i] = (VkQueueFamilyProperties*)tf_calloc(queueFamilyPropertyCount[i], sizeof(VkQueueFamilyProperties));
		vkGetPhysicalDeviceQueueFamilyProperties(gpus[i], &queueFamilyPropertyCount[i], queueFamilyProperties[i]);

		gpuSettings[i] = {};
		gpuSettings[i].mUniformBufferAlignment = (uint32_t)gpuProperties[i].properties.limits.minUniformBufferOffsetAlignment;
		gpuSettings[i].mUploadBufferTextureAlignment = (uint32_t)gpuProperties[i].properties.limits.optimalBufferCopyOffsetAlignment;
		gpuSettings[i].mUploadBufferTextureRowAlignment = (uint32_t)gpuProperties[i].properties.limits.optimalBufferCopyRowPitchAlignment;
		gpuSettings[i].mMaxVertexInputBindings = gpuProperties[i].properties.limits.maxVertexInputBindings;
		gpuSettings[i].mMultiDrawIndirect = gpuProperties[i].properties.limits.maxDrawIndirectCount > 1;

		gpuSettings[i].mWaveLaneCount = subgroupProperties.subgroupSize;
		gpuSettings[i].mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_NONE;
		if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT)
			gpuSettings[i].mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_BASIC_BIT;
		if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_VOTE_BIT)
			gpuSettings[i].mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_VOTE_BIT;
		if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT)
			gpuSettings[i].mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_ARITHMETIC_BIT;
		if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT)
			gpuSettings[i].mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_BALLOT_BIT;
		if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT)
			gpuSettings[i].mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_SHUFFLE_BIT;
		if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT)
			gpuSettings[i].mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_SHUFFLE_RELATIVE_BIT;
		if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_CLUSTERED_BIT)
			gpuSettings[i].mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_CLUSTERED_BIT;
		if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_QUAD_BIT)
			gpuSettings[i].mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_QUAD_BIT;
		if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_PARTITIONED_BIT_NV)
			gpuSettings[i].mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_PARTITIONED_BIT_NV;

#if VK_EXT_fragment_shader_interlock
		gpuSettings[i].mROVsSupported = (bool)fragmentShaderInterlockFeatures.fragmentShaderPixelInterlock;
#endif
		gpuSettings[i].mTessellationSupported = gpuFeatures[i].features.tessellationShader;
		gpuSettings[i].mGeometryShaderSupported = gpuFeatures[i].features.geometryShader;

		//save vendor and model Id as string
		sprintf(gpuSettings[i].mGpuVendorPreset.mModelId, "%#x", gpuProperties[i].properties.deviceID);
		sprintf(gpuSettings[i].mGpuVendorPreset.mVendorId, "%#x", gpuProperties[i].properties.vendorID);
		strncpy(gpuSettings[i].mGpuVendorPreset.mGpuName, gpuProperties[i].properties.deviceName, MAX_GPU_VENDOR_STRING_LENGTH);

		//TODO: Fix once vulkan adds support for revision ID
		strncpy(gpuSettings[i].mGpuVendorPreset.mRevisionId, "0x00", MAX_GPU_VENDOR_STRING_LENGTH);
		gpuSettings[i].mGpuVendorPreset.mPresetLevel = getGPUPresetLevel(
			gpuSettings[i].mGpuVendorPreset.mVendorId, gpuSettings[i].mGpuVendorPreset.mModelId,
			gpuSettings[i].mGpuVendorPreset.mRevisionId);

		LOGF(
			LogLevel::eINFO, "GPU[%i] detected. Vendor ID: %x, Model ID: %x, Preset: %s, GPU Name: %s", i,
			gpuSettings[i].mGpuVendorPreset.mVendorId, gpuSettings[i].mGpuVendorPreset.mModelId,
			presetLevelToString(gpuSettings[i].mGpuVendorPreset.mPresetLevel), gpuSettings[i].mGpuVendorPreset.mGpuName);

		// Check that gpu supports at least graphics
		if (gpuIndex == UINT32_MAX || isDeviceBetter(i, gpuIndex, gpuSettings, gpuProperties, gpuMemoryProperties))
		{
			uint32_t                 count = queueFamilyPropertyCount[i];
			VkQueueFamilyProperties* properties = queueFamilyProperties[i];

			//select if graphics queue is available
			for (uint32_t j = 0; j < count; j++)
			{
				//get graphics queue family
				if (properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					gpuIndex = i;
					break;
				}
			}
		}
	}

#if defined(AUTOMATED_TESTING) && defined(ACTIVE_TESTING_GPU) && !defined(__ANDROID__) && !defined(NX64)
	selectActiveGpu(gpuSettings, &gpuIndex, gpuCount);
#endif

	// If we don't own the instance or device, then we need to set the gpuIndex to the correct physical device
#if defined(VK_USE_DISPATCH_TABLES)
	gpuIndex = UINT32_MAX;
	for (uint32_t i = 0; i < gpuCount; i++)
	{
		if (gpus[i] == pRenderer->mVulkan.pVkActiveGPU)
		{
			gpuIndex = i;
		}
	}
#endif

	if (VK_PHYSICAL_DEVICE_TYPE_CPU == gpuProperties[gpuIndex].properties.deviceType)
	{
		LOGF(eERROR, "The only available GPU is of type VK_PHYSICAL_DEVICE_TYPE_CPU. Early exiting");
		ASSERT(false);
		return false;
	}

	ASSERT(gpuIndex != UINT32_MAX);
	pRenderer->mVulkan.pVkActiveGPU = gpus[gpuIndex];
	pRenderer->mVulkan.pVkActiveGPUProperties = (VkPhysicalDeviceProperties2*)tf_malloc(sizeof(VkPhysicalDeviceProperties2));
	pRenderer->pActiveGpuSettings = (GPUSettings*)tf_malloc(sizeof(GPUSettings));
	*pRenderer->mVulkan.pVkActiveGPUProperties = gpuProperties[gpuIndex];
	*pRenderer->pActiveGpuSettings = gpuSettings[gpuIndex];
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkActiveGPU);

	LOGF(LogLevel::eINFO, "GPU[%d] is selected as default GPU", gpuIndex);
	LOGF(LogLevel::eINFO, "Name of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mGpuName);
	LOGF(LogLevel::eINFO, "Vendor id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mVendorId);
	LOGF(LogLevel::eINFO, "Model id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mModelId);
	LOGF(LogLevel::eINFO, "Preset of selected gpu: %s", presetLevelToString(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel));

	uint32_t layerCount = 0;
	uint32_t extCount = 0;
	vkEnumerateDeviceLayerProperties(pRenderer->mVulkan.pVkActiveGPU, &layerCount, NULL);
	vkEnumerateDeviceExtensionProperties(pRenderer->mVulkan.pVkActiveGPU, NULL, &extCount, NULL);

	VkLayerProperties* layers = (VkLayerProperties*)alloca(sizeof(VkLayerProperties) * layerCount);
	vkEnumerateDeviceLayerProperties(pRenderer->mVulkan.pVkActiveGPU, &layerCount, layers);

	VkExtensionProperties* exts = (VkExtensionProperties*)alloca(sizeof(VkExtensionProperties) * extCount);
	vkEnumerateDeviceExtensionProperties(pRenderer->mVulkan.pVkActiveGPU, NULL, &extCount, exts);

	for (uint32_t i = 0; i < layerCount; ++i)
	{
		internal_log(eINFO, layers[i].layerName, "vkdevice-layer");
		if (strcmp(layers[i].layerName, "VK_LAYER_RENDERDOC_Capture") == 0)
			gRenderDocLayerEnabled = true;
	}

	for (uint32_t i = 0; i < extCount; ++i)
	{
		internal_log(eINFO, exts[i].extensionName, "vkdevice-ext");
	}

	uint32_t extension_count = 0;
	bool     dedicatedAllocationExtension = false;
	bool     memoryReq2Extension = false;
	bool     externalMemoryExtension = false;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	bool externalMemoryWin32Extension = false;
#endif
	// Standalone extensions
	{
		const char*                layer_name = NULL;
		uint32_t                   initialCount = sizeof(gVkWantedDeviceExtensions) / sizeof(gVkWantedDeviceExtensions[0]);
		const uint32_t             userRequestedCount = (uint32_t)pDesc->mVulkan.mDeviceExtensionCount;
		eastl::vector<const char*> wantedDeviceExtensions(initialCount + userRequestedCount);
		for (uint32_t i = 0; i < initialCount; ++i)
		{
			wantedDeviceExtensions[i] = gVkWantedDeviceExtensions[i];
		}
		for (uint32_t i = 0; i < userRequestedCount; ++i)
		{
			wantedDeviceExtensions[initialCount + i] = pDesc->mVulkan.ppDeviceExtensions[i];
		}
		const uint32_t wanted_extension_count = (uint32_t)wantedDeviceExtensions.size();
		uint32_t       count = 0;
		vkEnumerateDeviceExtensionProperties(pRenderer->mVulkan.pVkActiveGPU, layer_name, &count, NULL);
		if (count > 0)
		{
			VkExtensionProperties* properties = (VkExtensionProperties*)tf_calloc(count, sizeof(*properties));
			ASSERT(properties != NULL);
			vkEnumerateDeviceExtensionProperties(pRenderer->mVulkan.pVkActiveGPU, layer_name, &count, properties);
			for (uint32_t j = 0; j < count; ++j)
			{
				for (uint32_t k = 0; k < wanted_extension_count; ++k)
				{
					if (strcmp(wantedDeviceExtensions[k], properties[j].extensionName) == 0)
					{
						deviceExtensionCache[extension_count++] = wantedDeviceExtensions[k];

#ifndef USE_DEBUG_UTILS_EXTENSION
						if (strcmp(wantedDeviceExtensions[k], VK_EXT_DEBUG_MARKER_EXTENSION_NAME) == 0)
							gDebugMarkerSupport = true;
#endif
						if (strcmp(wantedDeviceExtensions[k], VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) == 0)
							dedicatedAllocationExtension = true;
						if (strcmp(wantedDeviceExtensions[k], VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 0)
							memoryReq2Extension = true;
						if (strcmp(wantedDeviceExtensions[k], VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME) == 0)
							externalMemoryExtension = true;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
						if (strcmp(wantedDeviceExtensions[k], VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME) == 0)
							externalMemoryWin32Extension = true;
#endif
#ifdef VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME
						if (strcmp(wantedDeviceExtensions[k], VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME) == 0)
							gDrawIndirectCountExtension = true;
#endif
						if (strcmp(wantedDeviceExtensions[k], VK_AMD_DRAW_INDIRECT_COUNT_EXTENSION_NAME) == 0)
							gAMDDrawIndirectCountExtension = true;
						if (strcmp(wantedDeviceExtensions[k], VK_AMD_GCN_SHADER_EXTENSION_NAME) == 0)
							gAMDGCNShaderExtension = true;
						if (strcmp(wantedDeviceExtensions[k], VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) == 0)
							gDescriptorIndexingExtension = true;
#ifdef VK_NV_RAY_TRACING_SPEC_VERSION
						if (strcmp(wantedDeviceExtensions[k], VK_NV_RAY_TRACING_EXTENSION_NAME) == 0)
						{
							pRenderer->mVulkan.mRaytracingExtension = 1;
							gNVRayTracingExtension = true;
							gDescriptorTypeRangeSize = FORGE_DESCRIPTOR_TYPE_RANGE_SIZE;
						}
#endif
#ifdef VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME
						if (strcmp(wantedDeviceExtensions[k], VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME) == 0)
						{
							gYCbCrExtension = true;
						}
#endif
#ifdef USE_NSIGHT_AFTERMATH
						if (strcmp(wantedDeviceExtensions[k], VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME) == 0)
						{
							pRenderer->mDiagnosticCheckPointsSupport = true;
						}
						if (strcmp(wantedDeviceExtensions[k], VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME) == 0)
						{
							pRenderer->mDiagnosticsConfigSupport = true;
						}
#endif
						break;
					}
				}
			}
			SAFE_FREE((void*)properties);
		}
	}

#if !defined(VK_USE_DISPATCH_TABLES)
	// Add more extensions here
#if VK_EXT_fragment_shader_interlock
	VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragmentShaderInterlockFeatures = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT
	};
	VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexingFeatures = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT, &fragmentShaderInterlockFeatures
	};
#else
	VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexingFeatures = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT
	};
#endif    // VK_EXT_fragment_shader_interlock

	VkPhysicalDeviceFeatures2KHR gpuFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
	gpuFeatures2.pNext = &descriptorIndexingFeatures;

#ifdef VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME
	VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcr_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES };
	if (gYCbCrExtension)
	{
		ycbcr_features.pNext = gpuFeatures2.pNext;
		gpuFeatures2.pNext = &ycbcr_features;
	}
#endif

#ifndef NX64
	vkGetPhysicalDeviceFeatures2KHR(pRenderer->mVulkan.pVkActiveGPU, &gpuFeatures2);
#else
	vkGetPhysicalDeviceFeatures2(pRenderer->mVulkan.pVkActiveGPU, &gpuFeatures2);
#endif

	// need a queue_priority for each queue in the queue family we create
	uint32_t                 queueFamiliesCount = queueFamilyPropertyCount[gpuIndex];
	VkQueueFamilyProperties* queueFamiliesProperties = queueFamilyProperties[gpuIndex];
	constexpr uint32_t       kMaxQueueFamilies = 16;
	constexpr uint32_t       kMaxQueueCount = 64;
	float                    queueFamilyPriorities[kMaxQueueFamilies][kMaxQueueCount] = {};
	uint32_t                 queue_create_infos_count = 0;
	VkDeviceQueueCreateInfo* queue_create_infos = (VkDeviceQueueCreateInfo*)alloca(queueFamiliesCount * sizeof(VkDeviceQueueCreateInfo));

	const uint32_t maxQueueFlag =
		VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT | VK_QUEUE_PROTECTED_BIT;
	pRenderer->mVulkan.pAvailableQueueCount = (uint32_t**)tf_malloc(pRenderer->mLinkedNodeCount * sizeof(uint32_t*));
	pRenderer->mVulkan.pUsedQueueCount = (uint32_t**)tf_malloc(pRenderer->mLinkedNodeCount * sizeof(uint32_t*));
	for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
	{
		pRenderer->mVulkan.pAvailableQueueCount[i] = (uint32_t*)tf_calloc(maxQueueFlag, sizeof(uint32_t));
		pRenderer->mVulkan.pUsedQueueCount[i] = (uint32_t*)tf_calloc(maxQueueFlag, sizeof(uint32_t));
	}

	for (uint32_t i = 0; i < queueFamiliesCount; i++)
	{
		uint32_t queueCount = queueFamiliesProperties[i].queueCount;
		if (queueCount > 0)
		{
			// Request only one queue of each type if mRequestAllAvailableQueues is not set to true
			if (queueCount > 1 && !pDesc->mVulkan.mRequestAllAvailableQueues)
			{
				queueCount = 1;
			}

			ASSERT(queueCount <= kMaxQueueCount);
			queueCount = min(queueCount, kMaxQueueCount);

			queue_create_infos[queue_create_infos_count] = {};
			queue_create_infos[queue_create_infos_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_create_infos[queue_create_infos_count].pNext = NULL;
			queue_create_infos[queue_create_infos_count].flags = 0;
			queue_create_infos[queue_create_infos_count].queueFamilyIndex = i;
			queue_create_infos[queue_create_infos_count].queueCount = queueCount;
			queue_create_infos[queue_create_infos_count].pQueuePriorities = queueFamilyPriorities[i];
			queue_create_infos_count++;

			for (uint32_t n = 0; n < pRenderer->mLinkedNodeCount; ++n)
			{
				pRenderer->mVulkan.pAvailableQueueCount[n][queueFamiliesProperties[i].queueFlags] = queueCount;
			}
		}
	}

#if defined(QUEST_VR)
    char oculusVRDeviceExtensionBuffer[4096];
    hook_add_vk_device_extensions(deviceExtensionCache, &extension_count, MAX_DEVICE_EXTENSIONS, oculusVRDeviceExtensionBuffer, sizeof(oculusVRDeviceExtensionBuffer));
#endif

	DECLARE_ZERO(VkDeviceCreateInfo, create_info);
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pNext = &gpuFeatures2;
	create_info.flags = 0;
	create_info.queueCreateInfoCount = queue_create_infos_count;
	create_info.pQueueCreateInfos = queue_create_infos;
	create_info.enabledLayerCount = 0;
	create_info.ppEnabledLayerNames = NULL;
	create_info.enabledExtensionCount = extension_count;
	create_info.ppEnabledExtensionNames = deviceExtensionCache;
	create_info.pEnabledFeatures = NULL;

#if defined(USE_NSIGHT_AFTERMATH)
	if (pRenderer->mDiagnosticCheckPointsSupport && pRenderer->mDiagnosticsConfigSupport)
	{
		pRenderer->mAftermathSupport = true;
		LOGF(LogLevel::eINFO, "Successfully loaded Aftermath extensions");
	}

	if (pRenderer->mAftermathSupport)
	{
		DECLARE_ZERO(VkDeviceDiagnosticsConfigCreateInfoNV, diagnostics_create_info);
		diagnostics_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV;
		diagnostics_create_info.flags = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV |
										VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV |
										VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV;
		diagnostics_create_info.pNext = gpuFeatures2.pNext;
		gpuFeatures2.pNext = &diagnostics_create_info;
		// Enable Nsight Aftermath GPU crash dump creation.
		// This needs to be done before the Vulkan device is created.
		CreateAftermathTracker(pRenderer->pName, &pRenderer->mAftermathTracker);
	}
#endif

	/************************************************************************/
	// Add Device Group Extension if requested and available
	/************************************************************************/
#if VK_KHR_device_group_creation
	if (pRenderer->mGpuMode == GPU_MODE_LINKED)
	{
		create_info.pNext = &deviceGroupInfo;
	}
#endif
	CHECK_VKRESULT(vkCreateDevice(pRenderer->mVulkan.pVkActiveGPU, &create_info, &gVkAllocationCallbacks, &pRenderer->mVulkan.pVkDevice));

#if !defined(NX64)
	// Load Vulkan device functions to bypass loader
	volkLoadDevice(pRenderer->mVulkan.pVkDevice);
#endif
#endif

	gDedicatedAllocationExtension = dedicatedAllocationExtension && memoryReq2Extension;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
	gExternalMemoryExtension = externalMemoryExtension && externalMemoryWin32Extension;
#endif

	if (gDedicatedAllocationExtension)
	{
		LOGF(LogLevel::eINFO, "Successfully loaded Dedicated Allocation extension");
	}

	if (gExternalMemoryExtension)
	{
		LOGF(LogLevel::eINFO, "Successfully loaded External Memory extension");
	}

#ifdef VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME
	if (gDrawIndirectCountExtension)
	{
		pfnVkCmdDrawIndirectCountKHR = vkCmdDrawIndirectCountKHR;
		pfnVkCmdDrawIndexedIndirectCountKHR = vkCmdDrawIndexedIndirectCountKHR;
		LOGF(LogLevel::eINFO, "Successfully loaded Draw Indirect extension");
	}
	else if (gAMDDrawIndirectCountExtension)
#endif
	{
		pfnVkCmdDrawIndirectCountKHR = vkCmdDrawIndirectCountAMD;
		pfnVkCmdDrawIndexedIndirectCountKHR = vkCmdDrawIndexedIndirectCountAMD;
		LOGF(LogLevel::eINFO, "Successfully loaded AMD Draw Indirect extension");
	}

	if (gAMDGCNShaderExtension)
	{
		LOGF(LogLevel::eINFO, "Successfully loaded AMD GCN Shader extension");
	}

	if (gDescriptorIndexingExtension)
	{
		LOGF(LogLevel::eINFO, "Successfully loaded Descriptor Indexing extension");
	}

	if (gNVRayTracingExtension)
	{
		LOGF(LogLevel::eINFO, "Successfully loaded Nvidia Ray Tracing extension");
	}

#ifdef USE_DEBUG_UTILS_EXTENSION
	gDebugMarkerSupport = (&vkCmdBeginDebugUtilsLabelEXT) && (&vkCmdEndDebugUtilsLabelEXT) && (&vkCmdInsertDebugUtilsLabelEXT) &&
						  (&vkSetDebugUtilsObjectNameEXT);
#endif

	for (uint32_t i = 0; i < gpuCount; ++i)
		SAFE_FREE(queueFamilyProperties[i]);

	vk_utils_caps_builder(pRenderer);

	return true;
}

static void RemoveDevice(Renderer* pRenderer)
{
	vkDestroyDevice(pRenderer->mVulkan.pVkDevice, &gVkAllocationCallbacks);
	SAFE_FREE(pRenderer->pActiveGpuSettings);
	SAFE_FREE(pRenderer->mVulkan.pVkActiveGPUProperties);

#if defined(USE_NSIGHT_AFTERMATH)
	if (pRenderer->mAftermathSupport)
	{
		DestroyAftermathTracker(&pRenderer->mAftermathTracker);
	}
#endif
}

VkDeviceMemory get_vk_device_memory(Renderer* pRenderer, Buffer* pBuffer)
{
	VmaAllocationInfo allocInfo = {};
	vmaGetAllocationInfo(pRenderer->mVulkan.pVmaAllocator, pBuffer->mVulkan.pVkAllocation, &allocInfo);
	return allocInfo.deviceMemory;
}

uint64_t get_vk_device_memory_offset(Renderer* pRenderer, Buffer* pBuffer)
{
	VmaAllocationInfo allocInfo = {};
	vmaGetAllocationInfo(pRenderer->mVulkan.pVmaAllocator, pBuffer->mVulkan.pVkAllocation, &allocInfo);
	return (uint64_t)allocInfo.offset;
}
/************************************************************************/
// Renderer Init Remove
/************************************************************************/
void vk_initRenderer(const char* appName, const RendererDesc* pDesc, Renderer** ppRenderer)
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

	// Initialize the Vulkan internal bits
	{
		AGSReturnCode agsRet = agsInit();
		if (AGSReturnCode::AGS_SUCCESS == agsRet)
		{
			agsPrintDriverInfo();
		}

		// Display NVIDIA driver version using nvapi
		NvAPI_Status nvStatus = nvapiInit();
		if (NvAPI_Status::NVAPI_OK == nvStatus)
		{
			nvapiPrintDriverInfo();
		}

#if defined(VK_USE_DISPATCH_TABLES)
		VkResult vkRes = volkInitializeWithDispatchTables(pRenderer);
		if (vkRes != VK_SUCCESS)
		{
			LOGF(LogLevel::eERROR, "Failed to initialize Vulkan");
			return;
		}
#else
		const char** instanceLayers = (const char**)alloca((2 + pDesc->mVulkan.mInstanceLayerCount) * sizeof(char*));
		uint32_t     instanceLayerCount = 0;

#if defined(ENABLE_GRAPHICS_DEBUG)
		// this turns on all validation layers
		instanceLayers[instanceLayerCount++] = "VK_LAYER_KHRONOS_validation";
#endif

		// this turns on render doc layer for gpu capture
#ifdef USE_RENDER_DOC
		instanceLayers[instanceLayerCount++] = "VK_LAYER_RENDERDOC_Capture";
#endif

		// Add user specified instance layers for instance creation
		for (uint32_t i = 0; i < (uint32_t)pDesc->mVulkan.mInstanceLayerCount; ++i)
			instanceLayers[instanceLayerCount++] = pDesc->mVulkan.ppInstanceLayers[i];

#if !defined(NX64)
		VkResult vkRes = volkInitialize();
		if (vkRes != VK_SUCCESS)
		{
			LOGF(LogLevel::eERROR, "Failed to initialize Vulkan");
			SAFE_FREE(pRenderer->pName);
			SAFE_FREE(pRenderer);
			return;
		}
#endif

		CreateInstance(appName, pDesc, instanceLayerCount, instanceLayers, pRenderer);
#endif
		if (!AddDevice(pDesc, pRenderer))
		{
			SAFE_FREE(pRenderer->pName);
			RemoveInstance(pRenderer);
			SAFE_FREE(pRenderer);
			return;
		}

		//anything below LOW preset is not supported and we will exit
		if (pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel < GPU_PRESET_LOW)
		{
			//have the condition in the assert as well so its cleared when the assert message box appears

			ASSERT(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel >= GPU_PRESET_LOW);    //-V547

			SAFE_FREE(pRenderer->pName);

			//remove device and any memory we allocated in just above as this is the first function called
			//when initializing the forge
#if !defined(VK_USE_DISPATCH_TABLES)
			RemoveDevice(pRenderer);
			RemoveInstance(pRenderer);
			SAFE_FREE(pRenderer);
			LOGF(LogLevel::eERROR, "Selected GPU has an Office Preset in gpu.cfg.");
			LOGF(LogLevel::eERROR, "Office preset is not supported by The Forge.");
#endif

			//return NULL pRenderer so that client can gracefully handle exit
			//This is better than exiting from here in case client has allocated memory or has fallbacks
			*ppRenderer = NULL;
			return;
		}
		/************************************************************************/
		// Memory allocator
		/************************************************************************/
		VmaAllocatorCreateInfo createInfo = { 0 };
		createInfo.device = pRenderer->mVulkan.pVkDevice;
		createInfo.physicalDevice = pRenderer->mVulkan.pVkActiveGPU;
		createInfo.instance = pRenderer->mVulkan.pVkInstance;

		// Render Doc Capture currently does not support use of this extension
		if (gDedicatedAllocationExtension && !gRenderDocLayerEnabled)
		{
			createInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
		}

		VmaVulkanFunctions vulkanFunctions = {};
		vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
		vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
		vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
		vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
		vulkanFunctions.vkCreateImage = vkCreateImage;
		vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
		vulkanFunctions.vkDestroyImage = vkDestroyImage;
		vulkanFunctions.vkFreeMemory = vkFreeMemory;
		vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
		vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
		vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
		vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
		vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
		vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
		vulkanFunctions.vkMapMemory = vkMapMemory;
		vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
		vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
		vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
		vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;

		createInfo.pVulkanFunctions = &vulkanFunctions;
		createInfo.pAllocationCallbacks = &gVkAllocationCallbacks;
		vmaCreateAllocator(&createInfo, &pRenderer->mVulkan.pVmaAllocator);
	}

	VkDescriptorPoolSize descriptorPoolSizes[FORGE_DESCRIPTOR_TYPE_RANGE_SIZE] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1024 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8192 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1024 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1024 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8192 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 },
	};
#ifdef VK_NV_RAY_TRACING_SPEC_VERSION
	if (gNVRayTracingExtension)
	{
		descriptorPoolSizes[FORGE_DESCRIPTOR_TYPE_RANGE_SIZE - 1] = { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1024 };
	}
#endif
	add_descriptor_pool(
		pRenderer, 8192, (VkDescriptorPoolCreateFlags)0, descriptorPoolSizes, gDescriptorTypeRangeSize,
		&pRenderer->mVulkan.pDescriptorPool);
	pRenderPassMutex = (Mutex*)tf_calloc(1, sizeof(Mutex));
	initMutex(pRenderPassMutex);
	gRenderPassMap = tf_placement_new<eastl::hash_map<ThreadID, RenderPassMap> >(tf_malloc(sizeof(*gRenderPassMap)));
	gFrameBufferMap = tf_placement_new<eastl::hash_map<ThreadID, FrameBufferMap> >(tf_malloc(sizeof(*gFrameBufferMap)));

	VkPhysicalDeviceFeatures2KHR gpuFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
	vkGetPhysicalDeviceFeatures2KHR(pRenderer->mVulkan.pVkActiveGPU, &gpuFeatures);

	// Set shader macro based on runtime information
	static char descriptorIndexingMacroBuffer[2] = {};
	static char textureArrayDynamicIndexingMacroBuffer[2] = {};
	sprintf(descriptorIndexingMacroBuffer, "%u", (uint32_t)(gDescriptorIndexingExtension));
	sprintf(textureArrayDynamicIndexingMacroBuffer, "%u", (uint32_t)(gpuFeatures.features.shaderSampledImageArrayDynamicIndexing));
	static ShaderMacro rendererShaderDefines[] = {
		{ "VK_EXT_DESCRIPTOR_INDEXING_ENABLED", descriptorIndexingMacroBuffer },
		{ "VK_FEATURE_TEXTURE_ARRAY_DYNAMIC_INDEXING_ENABLED", textureArrayDynamicIndexingMacroBuffer },
		// Descriptor set indices
		{ "UPDATE_FREQ_NONE", "set = 0" },
		{ "UPDATE_FREQ_PER_FRAME", "set = 1" },
		{ "UPDATE_FREQ_PER_BATCH", "set = 2" },
		{ "UPDATE_FREQ_PER_DRAW", "set = 3" },
	};
	pRenderer->mBuiltinShaderDefinesCount = sizeof(rendererShaderDefines) / sizeof(rendererShaderDefines[0]);
	pRenderer->pBuiltinShaderDefines = rendererShaderDefines;

	util_find_queue_family_index(pRenderer, 0, QUEUE_TYPE_GRAPHICS, NULL, &pRenderer->mVulkan.mGraphicsQueueFamilyIndex, NULL);
	util_find_queue_family_index(pRenderer, 0, QUEUE_TYPE_COMPUTE, NULL, &pRenderer->mVulkan.mComputeQueueFamilyIndex, NULL);
	util_find_queue_family_index(pRenderer, 0, QUEUE_TYPE_TRANSFER, NULL, &pRenderer->mVulkan.mTransferQueueFamilyIndex, NULL);

	add_default_resources(pRenderer);

#if defined(QUEST_VR)
    if (!hook_post_init_renderer(pRenderer->mVulkan.pVkInstance,
        pRenderer->mVulkan.pVkActiveGPU,
        pRenderer->mVulkan.pVkDevice))
    {
        vmaDestroyAllocator(pRenderer->mVulkan.pVmaAllocator);
        SAFE_FREE(pRenderer->pName);
#if !defined(VK_USE_DISPATCH_TABLES)
        RemoveDevice(pRenderer);
        RemoveInstance(pRenderer);
        SAFE_FREE(pRenderer);
        LOGF(LogLevel::eERROR, "Failed to initialize VrApi Vulkan systems.");
#endif
        *ppRenderer = NULL;
        return;
    }
#endif

	// Renderer is good!
	*ppRenderer = pRenderer;
}

void vk_exitRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);

	remove_default_resources(pRenderer);

	remove_descriptor_pool(pRenderer, pRenderer->mVulkan.pDescriptorPool);

	// Remove the renderpasses
	for (eastl::hash_map<ThreadID, RenderPassMap>::value_type& t : *gRenderPassMap)
		for (RenderPassMapNode& it : t.second)
			remove_render_pass(pRenderer, it.second);

	for (eastl::hash_map<ThreadID, FrameBufferMap>::value_type& t : *gFrameBufferMap)
		for (FrameBufferMapNode& it : t.second)
			remove_framebuffer(pRenderer, it.second);

#if defined(QUEST_VR)
    hook_pre_remove_renderer();
#endif

	// Destroy the Vulkan bits
	vmaDestroyAllocator(pRenderer->mVulkan.pVmaAllocator);

#if defined(VK_USE_DISPATCH_TABLES)
#else
	RemoveDevice(pRenderer);
	RemoveInstance(pRenderer);
#endif

	nvapiExit();
	agsExit();

	destroyMutex(pRenderPassMutex);
	gRenderPassMap->clear(true);
	gFrameBufferMap->clear(true);

	SAFE_FREE(pRenderPassMutex);
	SAFE_FREE(gRenderPassMap);
	SAFE_FREE(gFrameBufferMap);

	for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
	{
		SAFE_FREE(pRenderer->mVulkan.pAvailableQueueCount[i]);
		SAFE_FREE(pRenderer->mVulkan.pUsedQueueCount[i]);
	}

	// Free all the renderer components!
	SAFE_FREE(pRenderer->mVulkan.pAvailableQueueCount);
	SAFE_FREE(pRenderer->mVulkan.pUsedQueueCount);
	SAFE_FREE(pRenderer->pCapBits);
	SAFE_FREE(pRenderer->pName);
	SAFE_FREE(pRenderer);
}
/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void vk_addFence(Renderer* pRenderer, Fence** ppFence)
{
	ASSERT(pRenderer);
	ASSERT(ppFence);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);

	Fence* pFence = (Fence*)tf_calloc(1, sizeof(Fence));
	ASSERT(pFence);

	DECLARE_ZERO(VkFenceCreateInfo, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	add_info.pNext = NULL;
	add_info.flags = 0;
	CHECK_VKRESULT(vkCreateFence(pRenderer->mVulkan.pVkDevice, &add_info, &gVkAllocationCallbacks, &pFence->mVulkan.pVkFence));

	pFence->mVulkan.mSubmitted = false;

	*ppFence = pFence;
}

void vk_removeFence(Renderer* pRenderer, Fence* pFence)
{
	ASSERT(pRenderer);
	ASSERT(pFence);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);
	ASSERT(VK_NULL_HANDLE != pFence->mVulkan.pVkFence);

	vkDestroyFence(pRenderer->mVulkan.pVkDevice, pFence->mVulkan.pVkFence, &gVkAllocationCallbacks);

	SAFE_FREE(pFence);
}

void vk_addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
	ASSERT(pRenderer);
	ASSERT(ppSemaphore);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);

	Semaphore* pSemaphore = (Semaphore*)tf_calloc(1, sizeof(Semaphore));
	ASSERT(pSemaphore);

	DECLARE_ZERO(VkSemaphoreCreateInfo, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	add_info.pNext = NULL;
	add_info.flags = 0;
	CHECK_VKRESULT(
		vkCreateSemaphore(pRenderer->mVulkan.pVkDevice, &add_info, &gVkAllocationCallbacks, &(pSemaphore->mVulkan.pVkSemaphore)));
	// Set signal initial state.
	pSemaphore->mVulkan.mSignaled = false;

	*ppSemaphore = pSemaphore;
}

void vk_removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
	ASSERT(pRenderer);
	ASSERT(pSemaphore);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);
	ASSERT(VK_NULL_HANDLE != pSemaphore->mVulkan.pVkSemaphore);

	vkDestroySemaphore(pRenderer->mVulkan.pVkDevice, pSemaphore->mVulkan.pVkSemaphore, &gVkAllocationCallbacks);

	SAFE_FREE(pSemaphore);
}

void vk_addQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
{
	ASSERT(pDesc != NULL);

	const uint32_t          nodeIndex = pDesc->mNodeIndex;
	VkQueueFamilyProperties queueProps = {};
	uint8_t                 queueFamilyIndex = UINT8_MAX;
	uint8_t                 queueIndex = UINT8_MAX;

	util_find_queue_family_index(pRenderer, nodeIndex, pDesc->mType, &queueProps, &queueFamilyIndex, &queueIndex);
	++pRenderer->mVulkan.pUsedQueueCount[nodeIndex][queueProps.queueFlags];

	Queue* pQueue = (Queue*)tf_calloc(1, sizeof(Queue));
	ASSERT(pQueue);

	pQueue->mVulkan.mVkQueueFamilyIndex = queueFamilyIndex;
	pQueue->mNodeIndex = pDesc->mNodeIndex;
	pQueue->mType = pDesc->mType;
	pQueue->mVulkan.mVkQueueIndex = queueIndex;
	pQueue->mVulkan.mGpuMode = pRenderer->mGpuMode;
	pQueue->mVulkan.mTimestampPeriod = pRenderer->mVulkan.pVkActiveGPUProperties->properties.limits.timestampPeriod;
	pQueue->mVulkan.mFlags = queueProps.queueFlags;
	pQueue->mVulkan.pSubmitMutex = &pRenderer->pNullDescriptors->mSubmitMutex;

	// Get queue handle
	vkGetDeviceQueue(
		pRenderer->mVulkan.pVkDevice, pQueue->mVulkan.mVkQueueFamilyIndex, pQueue->mVulkan.mVkQueueIndex, &pQueue->mVulkan.pVkQueue);
	ASSERT(VK_NULL_HANDLE != pQueue->mVulkan.pVkQueue);

	*ppQueue = pQueue;

#if defined(QUEST_VR)
    extern Queue* pSynchronisationQueue;
    if(pDesc->mType == QUEUE_TYPE_GRAPHICS)
        pSynchronisationQueue = pQueue;
#endif
}

void vk_removeQueue(Renderer* pRenderer, Queue* pQueue)
{
#if defined(QUEST_VR)
    extern Queue* pSynchronisationQueue;
    if (pQueue == pSynchronisationQueue)
        pSynchronisationQueue = NULL;
#endif

	ASSERT(pRenderer);
	ASSERT(pQueue);

	const uint32_t     nodeIndex = pQueue->mNodeIndex;
	const VkQueueFlags queueFlags = pQueue->mVulkan.mFlags;
	--pRenderer->mVulkan.pUsedQueueCount[nodeIndex][queueFlags];

	SAFE_FREE(pQueue);
}

void vk_addCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
{
	ASSERT(pRenderer);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);
	ASSERT(ppCmdPool);

	CmdPool* pCmdPool = (CmdPool*)tf_calloc(1, sizeof(CmdPool));
	ASSERT(pCmdPool);

	pCmdPool->pQueue = pDesc->pQueue;

	DECLARE_ZERO(VkCommandPoolCreateInfo, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.queueFamilyIndex = pDesc->pQueue->mVulkan.mVkQueueFamilyIndex;
	if (pDesc->mTransient)
	{
		add_info.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	}

	CHECK_VKRESULT(vkCreateCommandPool(pRenderer->mVulkan.pVkDevice, &add_info, &gVkAllocationCallbacks, &(pCmdPool->pVkCmdPool)));

	*ppCmdPool = pCmdPool;
}

void vk_removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	ASSERT(pRenderer);
	ASSERT(pCmdPool);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);
	ASSERT(VK_NULL_HANDLE != pCmdPool->pVkCmdPool);

	vkDestroyCommandPool(pRenderer->mVulkan.pVkDevice, pCmdPool->pVkCmdPool, &gVkAllocationCallbacks);

	SAFE_FREE(pCmdPool);
}

void vk_addCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
{
	ASSERT(pRenderer);
	ASSERT(VK_NULL_HANDLE != pDesc->pPool);
	ASSERT(ppCmd);

	Cmd* pCmd = (Cmd*)tf_calloc_memalign(1, alignof(Cmd), sizeof(Cmd));
	ASSERT(pCmd);

	pCmd->pRenderer = pRenderer;
	pCmd->pQueue = pDesc->pPool->pQueue;
	pCmd->mVulkan.pCmdPool = pDesc->pPool;
	pCmd->mVulkan.mType = pDesc->pPool->pQueue->mType;
	pCmd->mVulkan.mNodeIndex = pDesc->pPool->pQueue->mNodeIndex;

	DECLARE_ZERO(VkCommandBufferAllocateInfo, alloc_info);
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.commandPool = pDesc->pPool->pVkCmdPool;
	alloc_info.level = pDesc->mSecondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	CHECK_VKRESULT(vkAllocateCommandBuffers(pRenderer->mVulkan.pVkDevice, &alloc_info, &(pCmd->mVulkan.pVkCmdBuf)));

	*ppCmd = pCmd;
}

void vk_removeCmd(Renderer* pRenderer, Cmd* pCmd)
{
	ASSERT(pRenderer);
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pRenderer->mVulkan.pVkDevice);
	ASSERT(VK_NULL_HANDLE != pCmd->mVulkan.pVkCmdBuf);

	vkFreeCommandBuffers(pRenderer->mVulkan.pVkDevice, pCmd->mVulkan.pCmdPool->pVkCmdPool, 1, &(pCmd->mVulkan.pVkCmdBuf));

	SAFE_FREE(pCmd);
}

void vk_addCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmd)
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

void vk_removeCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
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

void vk_toggleVSync(Renderer* pRenderer, SwapChain** ppSwapChain)
{
	SwapChain* pSwapChain = *ppSwapChain;

	Queue queue = {};
	queue.mVulkan.mVkQueueFamilyIndex = pSwapChain->mVulkan.mPresentQueueFamilyIndex;
	Queue* queues[] = { &queue };

	SwapChainDesc desc = *pSwapChain->mVulkan.pDesc;
	desc.mEnableVsync = !desc.mEnableVsync;
	desc.mPresentQueueCount = 1;
	desc.ppPresentQueues = queues;
	//toggle vsync on or off
	//for Vulkan we need to remove the SwapChain and recreate it with correct vsync option
	removeSwapChain(pRenderer, pSwapChain);
	addSwapChain(pRenderer, &desc, ppSwapChain);
}

void vk_addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppSwapChain);
	ASSERT(pDesc->mImageCount <= MAX_SWAPCHAIN_IMAGES);

#if defined(QUEST_VR)
    hook_add_swap_chain(pRenderer, pDesc, ppSwapChain);
    return;
#endif

	/************************************************************************/
	// Create surface
	/************************************************************************/
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkInstance);
	VkSurfaceKHR vkSurface;
	// Create a WSI surface for the window:
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	DECLARE_ZERO(VkWin32SurfaceCreateInfoKHR, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.hinstance = ::GetModuleHandle(NULL);
	add_info.hwnd = (HWND)pDesc->mWindowHandle.window;
	CHECK_VKRESULT(vkCreateWin32SurfaceKHR(pRenderer->mVulkan.pVkInstance, &add_info, &gVkAllocationCallbacks, &vkSurface));
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	DECLARE_ZERO(VkXlibSurfaceCreateInfoKHR, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.dpy = pDesc->mWindowHandle.display;      //TODO
	add_info.window = pDesc->mWindowHandle.window;    //TODO
	CHECK_VKRESULT(vkCreateXlibSurfaceKHR(pRenderer->mVulkan.pVkInstance, &add_info, &gVkAllocationCallbacks, &vkSurface));
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	DECLARE_ZERO(VkXcbSurfaceCreateInfoKHR, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.connection = pDesc->mWindowHandle.connection;    //TODO
	add_info.window = pDesc->mWindowHandle.window;            //TODO
	CHECK_VKRESULT(vkCreateXcbSurfaceKHR(pRenderer->pVkInstance, &add_info, &gVkAllocationCallbacks, &vkSurface));
#elif defined(VK_USE_PLATFORM_IOS_MVK)
	// Add IOS support here
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
	// Add MacOS support here
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
	DECLARE_ZERO(VkAndroidSurfaceCreateInfoKHR, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.window = pDesc->mWindowHandle.window;
	CHECK_VKRESULT(vkCreateAndroidSurfaceKHR(pRenderer->mVulkan.pVkInstance, &add_info, &gVkAllocationCallbacks, &vkSurface));
#elif defined(VK_USE_PLATFORM_GGP)
	extern VkResult ggpCreateSurface(VkInstance, VkSurfaceKHR * surface);
	CHECK_VKRESULT(ggpCreateSurface(pRenderer->pVkInstance, &vkSurface));
#elif defined(VK_USE_PLATFORM_VI_NN)
	extern VkResult nxCreateSurface(VkInstance, VkSurfaceKHR * surface);
	CHECK_VKRESULT(nxCreateSurface(pRenderer->mVulkan.pVkInstance, &vkSurface));
#else
#error PLATFORM NOT SUPPORTED
#endif
	/************************************************************************/
	// Create swap chain
	/************************************************************************/
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkActiveGPU);

	// Image count
	if (0 == pDesc->mImageCount)
	{
		((SwapChainDesc*)pDesc)->mImageCount = 2;
	}

	DECLARE_ZERO(VkSurfaceCapabilitiesKHR, caps);
	CHECK_VKRESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pRenderer->mVulkan.pVkActiveGPU, vkSurface, &caps));

	if ((caps.maxImageCount > 0) && (pDesc->mImageCount > caps.maxImageCount))
	{
		LOGF(
			LogLevel::eWARNING, "Changed requested SwapChain images {%u} to maximum allowed SwapChain images {%u}", pDesc->mImageCount,
			caps.maxImageCount);
		((SwapChainDesc*)pDesc)->mImageCount = caps.maxImageCount;
	}
	if (pDesc->mImageCount < caps.minImageCount)
	{
		LOGF(
			LogLevel::eWARNING, "Changed requested SwapChain images {%u} to minimum required SwapChain images {%u}", pDesc->mImageCount,
			caps.minImageCount);
		((SwapChainDesc*)pDesc)->mImageCount = caps.minImageCount;
	}

	// Surface format
	// Select a surface format, depending on whether HDR is available.

	DECLARE_ZERO(VkSurfaceFormatKHR, surface_format);
	surface_format.format = VK_FORMAT_UNDEFINED;
	uint32_t            surfaceFormatCount = 0;
	VkSurfaceFormatKHR* formats = NULL;

	// Get surface formats count
	CHECK_VKRESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->mVulkan.pVkActiveGPU, vkSurface, &surfaceFormatCount, NULL));

	// Allocate and get surface formats
	formats = (VkSurfaceFormatKHR*)tf_calloc(surfaceFormatCount, sizeof(*formats));
	CHECK_VKRESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->mVulkan.pVkActiveGPU, vkSurface, &surfaceFormatCount, formats));

	if ((1 == surfaceFormatCount) && (VK_FORMAT_UNDEFINED == formats[0].format))
	{
		surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
		surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	}
	else
	{
		VkSurfaceFormatKHR hdrSurfaceFormat = { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT };
		VkFormat           requested_format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mColorFormat);
		VkColorSpaceKHR    requested_color_space =
			requested_format == hdrSurfaceFormat.format ? hdrSurfaceFormat.colorSpace : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		for (uint32_t i = 0; i < surfaceFormatCount; ++i)
		{
			if ((requested_format == formats[i].format) && (requested_color_space == formats[i].colorSpace))
			{
				surface_format.format = requested_format;
				surface_format.colorSpace = requested_color_space;
				break;
			}
		}

		// Default to VK_FORMAT_B8G8R8A8_UNORM if requested format isn't found
		if (VK_FORMAT_UNDEFINED == surface_format.format)
		{
			surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
			surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		}
	}

	// Free formats
	SAFE_FREE(formats);

	// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
	// This mode waits for the vertical blank ("v-sync")
	VkPresentModeKHR  present_mode = VK_PRESENT_MODE_FIFO_KHR;
	uint32_t          swapChainImageCount = 0;
	VkPresentModeKHR* modes = NULL;
	// Get present mode count
	CHECK_VKRESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(pRenderer->mVulkan.pVkActiveGPU, vkSurface, &swapChainImageCount, NULL));

	// Allocate and get present modes
	modes = (VkPresentModeKHR*)alloca(swapChainImageCount * sizeof(*modes));
	CHECK_VKRESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(pRenderer->mVulkan.pVkActiveGPU, vkSurface, &swapChainImageCount, modes));

	const uint32_t   preferredModeCount = 4;
	VkPresentModeKHR preferredModeList[preferredModeCount] = { VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR,
															   VK_PRESENT_MODE_FIFO_RELAXED_KHR, VK_PRESENT_MODE_FIFO_KHR };
	uint32_t         preferredModeStartIndex = pDesc->mEnableVsync ? 2 : 0;

	for (uint32_t j = preferredModeStartIndex; j < preferredModeCount; ++j)
	{
		VkPresentModeKHR mode = preferredModeList[j];
		uint32_t         i = 0;
		for (; i < swapChainImageCount; ++i)
		{
			if (modes[i] == mode)
			{
				break;
			}
		}
		if (i < swapChainImageCount)
		{
			present_mode = mode;
			break;
		}
	}

	// Swapchain
	VkExtent2D extent = { 0 };
	extent.width = clamp(pDesc->mWidth, caps.minImageExtent.width, caps.maxImageExtent.width);
	extent.height = clamp(pDesc->mHeight, caps.minImageExtent.height, caps.maxImageExtent.height);

	VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
	uint32_t      queue_family_index_count = 0;
	uint32_t      queue_family_indices[2] = { pDesc->ppPresentQueues[0]->mVulkan.mVkQueueFamilyIndex, 0 };
	uint32_t      presentQueueFamilyIndex = -1;

	// Get queue family properties
	uint32_t                 queueFamilyPropertyCount = 0;
	VkQueueFamilyProperties* queueFamilyProperties = NULL;
	vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->mVulkan.pVkActiveGPU, &queueFamilyPropertyCount, NULL);
	queueFamilyProperties = (VkQueueFamilyProperties*)alloca(queueFamilyPropertyCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->mVulkan.pVkActiveGPU, &queueFamilyPropertyCount, queueFamilyProperties);

	// Check if hardware provides dedicated present queue
	if (queueFamilyPropertyCount)
	{
		for (uint32_t index = 0; index < queueFamilyPropertyCount; ++index)
		{
			VkBool32 supports_present = VK_FALSE;
			VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(pRenderer->mVulkan.pVkActiveGPU, index, vkSurface, &supports_present);
			if ((VK_SUCCESS == res) && (VK_TRUE == supports_present) && pDesc->ppPresentQueues[0]->mVulkan.mVkQueueFamilyIndex != index)
			{
				presentQueueFamilyIndex = index;
				break;
			}
		}

		// If there is no dedicated present queue, just find the first available queue which supports present
		if (presentQueueFamilyIndex == -1)
		{
			for (uint32_t index = 0; index < queueFamilyPropertyCount; ++index)
			{
				VkBool32 supports_present = VK_FALSE;
				VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(pRenderer->mVulkan.pVkActiveGPU, index, vkSurface, &supports_present);
				if ((VK_SUCCESS == res) && (VK_TRUE == supports_present))
				{
					presentQueueFamilyIndex = index;
					break;
				}
				else
				{
					// No present queue family available. Something goes wrong.
					ASSERT(0);
				}
			}
		}
	}

	// Find if gpu has a dedicated present queue
	VkQueue  presentQueue;
	uint32_t finalPresentQueueFamilyIndex;
	if (presentQueueFamilyIndex != -1 && queue_family_indices[0] != presentQueueFamilyIndex)
	{
		queue_family_indices[0] = presentQueueFamilyIndex;
		vkGetDeviceQueue(pRenderer->mVulkan.pVkDevice, queue_family_indices[0], 0, &presentQueue);
		queue_family_index_count = 1;
		finalPresentQueueFamilyIndex = presentQueueFamilyIndex;
	}
	else
	{
		finalPresentQueueFamilyIndex = queue_family_indices[0];
		presentQueue = VK_NULL_HANDLE;
	}

	VkSurfaceTransformFlagBitsKHR pre_transform;
	// #TODO: Add more if necessary but identity should be enough for now
	if (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		pre_transform = caps.currentTransform;
	}

	VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[] = {
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
	};
	VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR;
	for (VkCompositeAlphaFlagBitsKHR flag : compositeAlphaFlags)
	{
		if (caps.supportedCompositeAlpha & flag)
		{
			composite_alpha = flag;
			break;
		}
	}

	ASSERT(composite_alpha != VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR);

	VkSwapchainKHR vkSwapchain;
	DECLARE_ZERO(VkSwapchainCreateInfoKHR, swapChainCreateInfo);
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.pNext = NULL;
	swapChainCreateInfo.flags = 0;
	swapChainCreateInfo.surface = vkSurface;
	swapChainCreateInfo.minImageCount = pDesc->mImageCount;
	swapChainCreateInfo.imageFormat = surface_format.format;
	swapChainCreateInfo.imageColorSpace = surface_format.colorSpace;
	swapChainCreateInfo.imageExtent = extent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	swapChainCreateInfo.imageSharingMode = sharing_mode;
	swapChainCreateInfo.queueFamilyIndexCount = queue_family_index_count;
	swapChainCreateInfo.pQueueFamilyIndices = queue_family_indices;
	swapChainCreateInfo.preTransform = pre_transform;
	swapChainCreateInfo.compositeAlpha = composite_alpha;
	swapChainCreateInfo.presentMode = present_mode;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = 0;
	CHECK_VKRESULT(vkCreateSwapchainKHR(pRenderer->mVulkan.pVkDevice, &swapChainCreateInfo, &gVkAllocationCallbacks, &vkSwapchain));

	((SwapChainDesc*)pDesc)->mColorFormat = TinyImageFormat_FromVkFormat((TinyImageFormat_VkFormat)surface_format.format);

	// Create rendertargets from swapchain
	uint32_t imageCount = 0;
	CHECK_VKRESULT(vkGetSwapchainImagesKHR(pRenderer->mVulkan.pVkDevice, vkSwapchain, &imageCount, NULL));

	ASSERT(imageCount >= pDesc->mImageCount);

	VkImage* images = (VkImage*)alloca(imageCount * sizeof(VkImage));

	CHECK_VKRESULT(vkGetSwapchainImagesKHR(pRenderer->mVulkan.pVkDevice, vkSwapchain, &imageCount, images));

	SwapChain* pSwapChain = (SwapChain*)tf_calloc(1, sizeof(SwapChain) + imageCount * sizeof(RenderTarget*) + sizeof(SwapChainDesc));
	pSwapChain->ppRenderTargets = (RenderTarget**)(pSwapChain + 1);
	pSwapChain->mVulkan.pDesc = (SwapChainDesc*)(pSwapChain->ppRenderTargets + imageCount);
	ASSERT(pSwapChain);

	RenderTargetDesc descColor = {};
	descColor.mWidth = pDesc->mWidth;
	descColor.mHeight = pDesc->mHeight;
	descColor.mDepth = 1;
	descColor.mArraySize = 1;
	descColor.mFormat = pDesc->mColorFormat;
	descColor.mClearValue = pDesc->mColorClearValue;
	descColor.mSampleCount = SAMPLE_COUNT_1;
	descColor.mSampleQuality = 0;
	descColor.mStartState = RESOURCE_STATE_PRESENT;

	// Populate the vk_image field and add the Vulkan texture objects
	for (uint32_t i = 0; i < imageCount; ++i)
	{
		descColor.pNativeHandle = (void*)images[i];
		addRenderTarget(pRenderer, &descColor, &pSwapChain->ppRenderTargets[i]);
	}
	/************************************************************************/
	/************************************************************************/
	*pSwapChain->mVulkan.pDesc = *pDesc;
	pSwapChain->mEnableVsync = pDesc->mEnableVsync;
	pSwapChain->mImageCount = imageCount;
	pSwapChain->mVulkan.pVkSurface = vkSurface;
	pSwapChain->mVulkan.mPresentQueueFamilyIndex = finalPresentQueueFamilyIndex;
	pSwapChain->mVulkan.pPresentQueue = presentQueue;
	pSwapChain->mVulkan.pSwapChain = vkSwapchain;

	*ppSwapChain = pSwapChain;
}

void vk_removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pSwapChain);

#if defined(QUEST_VR)
    hook_remove_swap_chain(pRenderer, pSwapChain);
    return;
#endif

	for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
	{
		removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[i]);
	}

	vkDestroySwapchainKHR(pRenderer->mVulkan.pVkDevice, pSwapChain->mVulkan.pSwapChain, &gVkAllocationCallbacks);
	vkDestroySurfaceKHR(pRenderer->mVulkan.pVkInstance, pSwapChain->mVulkan.pVkSurface, &gVkAllocationCallbacks);

	SAFE_FREE(pSwapChain);
}

void vk_addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(pDesc->mSize > 0);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);

	Buffer* pBuffer = (Buffer*)tf_calloc_memalign(1, alignof(Buffer), sizeof(Buffer));
	ASSERT(ppBuffer);

	uint64_t allocationSize = pDesc->mSize;
	// Align the buffer size to multiples of the dynamic uniform buffer minimum size
	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
	{
		uint64_t minAlignment = pRenderer->pActiveGpuSettings->mUniformBufferAlignment;
		allocationSize = round_up_64(allocationSize, minAlignment);
	}

	DECLARE_ZERO(VkBufferCreateInfo, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.size = allocationSize;
	add_info.usage = util_to_vk_buffer_usage(pDesc->mDescriptors, pDesc->mFormat != TinyImageFormat_UNDEFINED);
	add_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	add_info.queueFamilyIndexCount = 0;
	add_info.pQueueFamilyIndices = NULL;

	// Buffer can be used as dest in a transfer command (Uploading data to a storage buffer, Readback query data)
	if (pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY || pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
		add_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	const bool linkedMultiGpu = (pRenderer->mGpuMode == GPU_MODE_LINKED && (pDesc->pSharedNodeIndices || pDesc->mNodeIndex));

	VmaAllocationCreateInfo vma_mem_reqs = {};
	vma_mem_reqs.usage = (VmaMemoryUsage)pDesc->mMemoryUsage;
	if (pDesc->mFlags & BUFFER_CREATION_FLAG_OWN_MEMORY_BIT)
		vma_mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	if (pDesc->mFlags & BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT)
		vma_mem_reqs.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
	if (linkedMultiGpu)
		vma_mem_reqs.flags |= VMA_ALLOCATION_CREATE_DONT_BIND_BIT;

	VmaAllocationInfo alloc_info = {};
	CHECK_VKRESULT(vmaCreateBuffer(
		pRenderer->mVulkan.pVmaAllocator, &add_info, &vma_mem_reqs, &pBuffer->mVulkan.pVkBuffer, &pBuffer->mVulkan.pVkAllocation,
		&alloc_info));

	pBuffer->pCpuMappedAddress = alloc_info.pMappedData;
	/************************************************************************/
	// Buffer to be used on multiple GPUs
	/************************************************************************/
	if (linkedMultiGpu)
	{
		VmaAllocationInfo allocInfo = {};
		vmaGetAllocationInfo(pRenderer->mVulkan.pVmaAllocator, pBuffer->mVulkan.pVkAllocation, &allocInfo);
		/************************************************************************/
		// Set all the device indices to the index of the device where we will create the buffer
		/************************************************************************/
		uint32_t* pIndices = (uint32_t*)alloca(pRenderer->mLinkedNodeCount * sizeof(uint32_t));
		util_calculate_device_indices(pRenderer, pDesc->mNodeIndex, pDesc->pSharedNodeIndices, pDesc->mSharedNodeIndexCount, pIndices);
		/************************************************************************/
		// #TODO : Move this to the Vulkan memory allocator
		/************************************************************************/
		VkBindBufferMemoryInfoKHR            bindInfo = { VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO_KHR };
		VkBindBufferMemoryDeviceGroupInfoKHR bindDeviceGroup = { VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO_KHR };
		bindDeviceGroup.deviceIndexCount = pRenderer->mLinkedNodeCount;
		bindDeviceGroup.pDeviceIndices = pIndices;
		bindInfo.buffer = pBuffer->mVulkan.pVkBuffer;
		bindInfo.memory = allocInfo.deviceMemory;
		bindInfo.memoryOffset = allocInfo.offset;
		bindInfo.pNext = &bindDeviceGroup;
		CHECK_VKRESULT(vkBindBufferMemory2KHR(pRenderer->mVulkan.pVkDevice, 1, &bindInfo));
		/************************************************************************/
		/************************************************************************/
	}
	/************************************************************************/
	// Set descriptor data
	/************************************************************************/
	if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER) || (pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER) ||
		(pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER))
	{
		if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER) || (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER))
		{
			pBuffer->mVulkan.mOffset = pDesc->mStructStride * pDesc->mFirstElement;
		}
	}

	if (add_info.usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT)
	{
		VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, NULL };
		viewInfo.buffer = pBuffer->mVulkan.pVkBuffer;
		viewInfo.flags = 0;
		viewInfo.format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mFormat);
		viewInfo.offset = pDesc->mFirstElement * pDesc->mStructStride;
		viewInfo.range = pDesc->mElementCount * pDesc->mStructStride;
		VkFormatProperties formatProps = {};
		vkGetPhysicalDeviceFormatProperties(pRenderer->mVulkan.pVkActiveGPU, viewInfo.format, &formatProps);
		if (!(formatProps.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT))
		{
			LOGF(LogLevel::eWARNING, "Failed to create uniform texel buffer view for format %u", (uint32_t)pDesc->mFormat);
		}
		else
		{
			CHECK_VKRESULT(vkCreateBufferView(
				pRenderer->mVulkan.pVkDevice, &viewInfo, &gVkAllocationCallbacks, &pBuffer->mVulkan.pVkUniformTexelView));
		}
	}
	if (add_info.usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)
	{
		VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, NULL };
		viewInfo.buffer = pBuffer->mVulkan.pVkBuffer;
		viewInfo.flags = 0;
		viewInfo.format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mFormat);
		viewInfo.offset = pDesc->mFirstElement * pDesc->mStructStride;
		viewInfo.range = pDesc->mElementCount * pDesc->mStructStride;
		VkFormatProperties formatProps = {};
		vkGetPhysicalDeviceFormatProperties(pRenderer->mVulkan.pVkActiveGPU, viewInfo.format, &formatProps);
		if (!(formatProps.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
		{
			LOGF(LogLevel::eWARNING, "Failed to create storage texel buffer view for format %u", (uint32_t)pDesc->mFormat);
		}
		else
		{
			CHECK_VKRESULT(vkCreateBufferView(
				pRenderer->mVulkan.pVkDevice, &viewInfo, &gVkAllocationCallbacks, &pBuffer->mVulkan.pVkStorageTexelView));
		}
	}

#if defined(ENABLE_GRAPHICS_DEBUG)
	if (pDesc->pName)
	{
		setBufferName(pRenderer, pBuffer, pDesc->pName);
	}
#endif

	/************************************************************************/
	/************************************************************************/
	pBuffer->mSize = (uint32_t)pDesc->mSize;
	pBuffer->mMemoryUsage = pDesc->mMemoryUsage;
	pBuffer->mNodeIndex = pDesc->mNodeIndex;
	pBuffer->mDescriptors = pDesc->mDescriptors;

	*ppBuffer = pBuffer;
}

void vk_removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	ASSERT(pRenderer);
	ASSERT(pBuffer);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);
	ASSERT(VK_NULL_HANDLE != pBuffer->mVulkan.pVkBuffer);

	if (pBuffer->mVulkan.pVkUniformTexelView)
	{
		vkDestroyBufferView(pRenderer->mVulkan.pVkDevice, pBuffer->mVulkan.pVkUniformTexelView, &gVkAllocationCallbacks);
		pBuffer->mVulkan.pVkUniformTexelView = VK_NULL_HANDLE;
	}
	if (pBuffer->mVulkan.pVkStorageTexelView)
	{
		vkDestroyBufferView(pRenderer->mVulkan.pVkDevice, pBuffer->mVulkan.pVkStorageTexelView, &gVkAllocationCallbacks);
		pBuffer->mVulkan.pVkStorageTexelView = VK_NULL_HANDLE;
	}

	vmaDestroyBuffer(pRenderer->mVulkan.pVmaAllocator, pBuffer->mVulkan.pVkBuffer, pBuffer->mVulkan.pVkAllocation);

	SAFE_FREE(pBuffer);
}

void vk_addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mWidth && pDesc->mHeight && (pDesc->mDepth || pDesc->mArraySize));
	if (pDesc->mSampleCount > SAMPLE_COUNT_1 && pDesc->mMipLevels > 1)
	{
		LOGF(LogLevel::eERROR, "Multi-Sampled textures cannot have mip maps");
		ASSERT(false);
		return;
	}

	size_t totalSize = sizeof(Texture);
	totalSize += (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE ? (pDesc->mMipLevels * sizeof(VkImageView)) : 0);
	Texture* pTexture = (Texture*)tf_calloc_memalign(1, alignof(Texture), totalSize);
	ASSERT(pTexture);

	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
		pTexture->mVulkan.pVkUAVDescriptors = (VkImageView*)(pTexture + 1);

	if (pDesc->pNativeHandle && !(pDesc->mFlags & TEXTURE_CREATION_FLAG_IMPORT_BIT))
	{
		pTexture->mOwnsImage = false;
		pTexture->mVulkan.pVkImage = (VkImage)pDesc->pNativeHandle;
	}
	else
	{
		pTexture->mOwnsImage = true;
	}

	VkImageUsageFlags additionalFlags = 0;
	if (pDesc->mStartState & RESOURCE_STATE_RENDER_TARGET)
		additionalFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	else if (pDesc->mStartState & RESOURCE_STATE_DEPTH_WRITE)
		additionalFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    uint arraySize = pDesc->mArraySize;
#if defined(QUEST_VR)
    if (additionalFlags == 0 && // If not a render target
        !!(pDesc->mFlags & TEXTURE_CREATION_FLAG_VR_MULTIVIEW))
    {
        // Double the array size
        arraySize *= 2;
    }
#endif

	VkImageType image_type = VK_IMAGE_TYPE_MAX_ENUM;
	if (pDesc->mFlags & TEXTURE_CREATION_FLAG_FORCE_2D)
	{
		ASSERT(pDesc->mDepth == 1);
		image_type = VK_IMAGE_TYPE_2D;
	}
	else if (pDesc->mFlags & TEXTURE_CREATION_FLAG_FORCE_3D)
	{
		image_type = VK_IMAGE_TYPE_3D;
	}
	else
	{
		if (pDesc->mDepth > 1)
			image_type = VK_IMAGE_TYPE_3D;
		else if (pDesc->mHeight > 1)
			image_type = VK_IMAGE_TYPE_2D;
		else
			image_type = VK_IMAGE_TYPE_1D;
	}

	DescriptorType descriptors = pDesc->mDescriptors;
	bool           cubemapRequired = (DESCRIPTOR_TYPE_TEXTURE_CUBE == (descriptors & DESCRIPTOR_TYPE_TEXTURE_CUBE));
	bool           arrayRequired = false;

	const bool     isPlanarFormat = TinyImageFormat_IsPlanar(pDesc->mFormat);
	const uint32_t numOfPlanes = TinyImageFormat_NumOfPlanes(pDesc->mFormat);
	const bool     isSinglePlane = TinyImageFormat_IsSinglePlane(pDesc->mFormat);
	ASSERT(
		((isSinglePlane && numOfPlanes == 1) || (!isSinglePlane && numOfPlanes > 1 && numOfPlanes <= MAX_PLANE_COUNT)) &&
		"Number of planes for multi-planar formats must be 2 or 3 and for single-planar formats it must be 1.");

	if (image_type == VK_IMAGE_TYPE_3D)
		arrayRequired = true;

	if (VK_NULL_HANDLE == pTexture->mVulkan.pVkImage)
	{
		DECLARE_ZERO(VkImageCreateInfo, add_info);
		add_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		add_info.pNext = NULL;
		add_info.flags = 0;
		add_info.imageType = image_type;
		add_info.format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mFormat);
		add_info.extent.width = pDesc->mWidth;
		add_info.extent.height = pDesc->mHeight;
		add_info.extent.depth = pDesc->mDepth;
		add_info.mipLevels = pDesc->mMipLevels;
		add_info.arrayLayers = arraySize;
		add_info.samples = util_to_vk_sample_count(pDesc->mSampleCount);
		add_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		add_info.usage = util_to_vk_image_usage(descriptors);
		add_info.usage |= additionalFlags;
		add_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		add_info.queueFamilyIndexCount = 0;
		add_info.pQueueFamilyIndices = NULL;
		add_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if (cubemapRequired)
			add_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		if (arrayRequired)
			add_info.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR;

		DECLARE_ZERO(VkFormatProperties, format_props);
		vkGetPhysicalDeviceFormatProperties(pRenderer->mVulkan.pVkActiveGPU, add_info.format, &format_props);
		if (isPlanarFormat)    // multi-planar formats must have each plane separately bound to memory, rather than having a single memory binding for the whole image
		{
			ASSERT(format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DISJOINT_BIT);
			add_info.flags |= VK_IMAGE_CREATE_DISJOINT_BIT;
		}

		if ((VK_IMAGE_USAGE_SAMPLED_BIT & add_info.usage) || (VK_IMAGE_USAGE_STORAGE_BIT & add_info.usage))
		{
			// Make it easy to copy to and from textures
			add_info.usage |= (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		}

		ASSERT(pRenderer->pCapBits->canShaderReadFrom[pDesc->mFormat] && "GPU shader can't' read from this format");

		// Verify that GPU supports this format
		VkFormatFeatureFlags format_features = util_vk_image_usage_to_format_features(add_info.usage);

		VkFormatFeatureFlags flags = format_props.optimalTilingFeatures & format_features;
		ASSERT((0 != flags) && "Format is not supported for GPU local images (i.e. not host visible images)");

		const bool linkedMultiGpu = (pRenderer->mGpuMode == GPU_MODE_LINKED) && (pDesc->pSharedNodeIndices || pDesc->mNodeIndex);

		VmaAllocationCreateInfo mem_reqs = { 0 };
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT)
			mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		if (linkedMultiGpu)
			mem_reqs.flags |= VMA_ALLOCATION_CREATE_DONT_BIND_BIT;
		mem_reqs.usage = (VmaMemoryUsage)VMA_MEMORY_USAGE_GPU_ONLY;

		VkExternalMemoryImageCreateInfoKHR externalInfo = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR, NULL };

#if defined(VK_USE_PLATFORM_WIN32_KHR)
		VkImportMemoryWin32HandleInfoKHR importInfo = { VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR, NULL };
#endif
		VkExportMemoryAllocateInfoKHR exportMemoryInfo = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR, NULL };

		if (gExternalMemoryExtension && pDesc->mFlags & TEXTURE_CREATION_FLAG_IMPORT_BIT)
		{
			add_info.pNext = &externalInfo;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
			struct ImportHandleInfo
			{
				void*                                 pHandle;
				VkExternalMemoryHandleTypeFlagBitsKHR mHandleType;
			};

			ImportHandleInfo* pHandleInfo = (ImportHandleInfo*)pDesc->pNativeHandle;
			importInfo.handle = pHandleInfo->pHandle;
			importInfo.handleType = pHandleInfo->mHandleType;

			externalInfo.handleTypes = pHandleInfo->mHandleType;

			mem_reqs.pUserData = &importInfo;
			// Allocate external (importable / exportable) memory as dedicated memory to avoid adding unnecessary complexity to the Vulkan Memory Allocator
			mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
#endif
		}
		else if (gExternalMemoryExtension && pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_BIT)
		{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
			exportMemoryInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
#endif

			mem_reqs.pUserData = &exportMemoryInfo;
			// Allocate external (importable / exportable) memory as dedicated memory to avoid adding unnecessary complexity to the Vulkan Memory Allocator
			mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		}

		VmaAllocationInfo alloc_info = {};
		if (isSinglePlane)
		{
			CHECK_VKRESULT(vmaCreateImage(
				pRenderer->mVulkan.pVmaAllocator, &add_info, &mem_reqs, &pTexture->mVulkan.pVkImage, &pTexture->mVulkan.pVkAllocation,
				&alloc_info));
		}
		else    // Multi-planar formats
		{
			// Create info requires the mutable format flag set for multi planar images
			// Also pass the format list for mutable formats as per recommendation from the spec
			// Might help to keep DCC enabled if we ever use this as a output format
			// DCC gets disabled when we pass mutable format bit to the create info. Passing the format list helps the driver to enable it
			VkFormat                       planarFormat = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mFormat);
			VkImageFormatListCreateInfoKHR formatList = {};
			formatList.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR;
			formatList.pNext = NULL;
			formatList.pViewFormats = &planarFormat;
			formatList.viewFormatCount = 1;

			add_info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
			add_info.pNext = &formatList;    //-V506

			// Create Image
			CHECK_VKRESULT(vkCreateImage(pRenderer->mVulkan.pVkDevice, &add_info, &gVkAllocationCallbacks, &pTexture->mVulkan.pVkImage));

			VkMemoryRequirements vkMemReq = {};
			uint64_t             planesOffsets[MAX_PLANE_COUNT] = { 0 };
			util_get_planar_vk_image_memory_requirement(
				pRenderer->mVulkan.pVkDevice, pTexture->mVulkan.pVkImage, numOfPlanes, &vkMemReq, planesOffsets);

			// Allocate image memory
			VkMemoryAllocateInfo mem_alloc_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			mem_alloc_info.allocationSize = vkMemReq.size;
			VkPhysicalDeviceMemoryProperties memProps = {};
			vkGetPhysicalDeviceMemoryProperties(pRenderer->mVulkan.pVkActiveGPU, &memProps);
			mem_alloc_info.memoryTypeIndex = util_get_memory_type(vkMemReq.memoryTypeBits, memProps, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			CHECK_VKRESULT(vkAllocateMemory(
				pRenderer->mVulkan.pVkDevice, &mem_alloc_info, &gVkAllocationCallbacks, &pTexture->mVulkan.pVkDeviceMemory));

			// Bind planes to their memories
			VkBindImageMemoryInfo      bindImagesMemoryInfo[MAX_PLANE_COUNT];
			VkBindImagePlaneMemoryInfo bindImagePlanesMemoryInfo[MAX_PLANE_COUNT];
			for (uint32_t i = 0; i < numOfPlanes; ++i)
			{
				VkBindImagePlaneMemoryInfo& bindImagePlaneMemoryInfo = bindImagePlanesMemoryInfo[i];
				bindImagePlaneMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO;
				bindImagePlaneMemoryInfo.pNext = NULL;
				bindImagePlaneMemoryInfo.planeAspect = (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_PLANE_0_BIT << i);

				VkBindImageMemoryInfo& bindImageMemoryInfo = bindImagesMemoryInfo[i];
				bindImageMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
				bindImageMemoryInfo.pNext = &bindImagePlaneMemoryInfo;
				bindImageMemoryInfo.image = pTexture->mVulkan.pVkImage;
				bindImageMemoryInfo.memory = pTexture->mVulkan.pVkDeviceMemory;
				bindImageMemoryInfo.memoryOffset = planesOffsets[i];
			}

			CHECK_VKRESULT(vkBindImageMemory2(pRenderer->mVulkan.pVkDevice, numOfPlanes, bindImagesMemoryInfo));
		}

		/************************************************************************/
		// Texture to be used on multiple GPUs
		/************************************************************************/
		if (linkedMultiGpu)
		{
			VmaAllocationInfo allocInfo = {};
			vmaGetAllocationInfo(pRenderer->mVulkan.pVmaAllocator, pTexture->mVulkan.pVkAllocation, &allocInfo);
			/************************************************************************/
			// Set all the device indices to the index of the device where we will create the texture
			/************************************************************************/
			uint32_t* pIndices = (uint32_t*)alloca(pRenderer->mLinkedNodeCount * sizeof(uint32_t));
			util_calculate_device_indices(pRenderer, pDesc->mNodeIndex, pDesc->pSharedNodeIndices, pDesc->mSharedNodeIndexCount, pIndices);
			/************************************************************************/
			// #TODO : Move this to the Vulkan memory allocator
			/************************************************************************/
			VkBindImageMemoryInfoKHR            bindInfo = { VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR };
			VkBindImageMemoryDeviceGroupInfoKHR bindDeviceGroup = { VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO_KHR };
			bindDeviceGroup.deviceIndexCount = pRenderer->mLinkedNodeCount;
			bindDeviceGroup.pDeviceIndices = pIndices;
			bindInfo.image = pTexture->mVulkan.pVkImage;
			bindInfo.memory = allocInfo.deviceMemory;
			bindInfo.memoryOffset = allocInfo.offset;
			bindInfo.pNext = &bindDeviceGroup;
			CHECK_VKRESULT(vkBindImageMemory2KHR(pRenderer->mVulkan.pVkDevice, 1, &bindInfo));
			/************************************************************************/
			/************************************************************************/
		}
	}
	/************************************************************************/
	// Create image view
	/************************************************************************/
	VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
	switch (image_type)
	{
		case VK_IMAGE_TYPE_1D: view_type = arraySize > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D; break;
		case VK_IMAGE_TYPE_2D:
			if (cubemapRequired)
				view_type = (arraySize > 6) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
			else
				view_type = arraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
			break;
		case VK_IMAGE_TYPE_3D:
			if (arraySize > 1)
			{
				LOGF(LogLevel::eERROR, "Cannot support 3D Texture Array in Vulkan");
				ASSERT(false);
			}
			view_type = VK_IMAGE_VIEW_TYPE_3D;
			break;
		default: ASSERT(false && "Image Format not supported!"); break;
	}

	ASSERT(view_type != VK_IMAGE_VIEW_TYPE_MAX_ENUM && "Invalid Image View");

	VkImageViewCreateInfo srvDesc = {};
	// SRV
	srvDesc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	srvDesc.pNext = NULL;
	srvDesc.flags = 0;
	srvDesc.image = pTexture->mVulkan.pVkImage;
	srvDesc.viewType = view_type;
	srvDesc.format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mFormat);
	srvDesc.components.r = VK_COMPONENT_SWIZZLE_R;
	srvDesc.components.g = VK_COMPONENT_SWIZZLE_G;
	srvDesc.components.b = VK_COMPONENT_SWIZZLE_B;
	srvDesc.components.a = VK_COMPONENT_SWIZZLE_A;
	srvDesc.subresourceRange.aspectMask = util_vk_determine_aspect_mask(srvDesc.format, false);
	srvDesc.subresourceRange.baseMipLevel = 0;
	srvDesc.subresourceRange.levelCount = pDesc->mMipLevels;
	srvDesc.subresourceRange.baseArrayLayer = 0;
	srvDesc.subresourceRange.layerCount = arraySize;
	pTexture->mAspectMask = util_vk_determine_aspect_mask(srvDesc.format, true);

	if (pDesc->pVkSamplerYcbcrConversionInfo)
	{
		srvDesc.pNext = pDesc->pVkSamplerYcbcrConversionInfo;
	}

	if (descriptors & DESCRIPTOR_TYPE_TEXTURE)
	{
		CHECK_VKRESULT(
			vkCreateImageView(pRenderer->mVulkan.pVkDevice, &srvDesc, &gVkAllocationCallbacks, &pTexture->mVulkan.pVkSRVDescriptor));
	}

	// SRV stencil
	if ((TinyImageFormat_HasStencil(pDesc->mFormat)) && (descriptors & DESCRIPTOR_TYPE_TEXTURE))
	{
		srvDesc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
		CHECK_VKRESULT(
			vkCreateImageView(pRenderer->mVulkan.pVkDevice, &srvDesc, &gVkAllocationCallbacks, &pTexture->mVulkan.pVkSRVStencilDescriptor));
	}

	// UAV
	if (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
	{
		VkImageViewCreateInfo uavDesc = srvDesc;
		// #NOTE : We dont support imageCube, imageCubeArray for consistency with other APIs
		// All cubemaps will be used as image2DArray for Image Load / Store ops
		if (uavDesc.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY || uavDesc.viewType == VK_IMAGE_VIEW_TYPE_CUBE)
			uavDesc.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		uavDesc.subresourceRange.levelCount = 1;
		for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
		{
			uavDesc.subresourceRange.baseMipLevel = i;
			CHECK_VKRESULT(vkCreateImageView(
				pRenderer->mVulkan.pVkDevice, &uavDesc, &gVkAllocationCallbacks, &pTexture->mVulkan.pVkUAVDescriptors[i]));
		}
	}
	/************************************************************************/
	/************************************************************************/
	pTexture->mNodeIndex = pDesc->mNodeIndex;
	pTexture->mWidth = pDesc->mWidth;
	pTexture->mHeight = pDesc->mHeight;
	pTexture->mDepth = pDesc->mDepth;
	pTexture->mMipLevels = pDesc->mMipLevels;
	pTexture->mUav = pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE;
	pTexture->mArraySizeMinusOne = arraySize - 1;
	pTexture->mFormat = pDesc->mFormat;

#if defined(ENABLE_GRAPHICS_DEBUG)
	if (pDesc->pName)
	{
		setTextureName(pRenderer, pTexture, pDesc->pName);
	}
#endif

	*ppTexture = pTexture;
}

void vk_removeTexture(Renderer* pRenderer, Texture* pTexture)
{
	ASSERT(pRenderer);
	ASSERT(pTexture);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);
	ASSERT(VK_NULL_HANDLE != pTexture->mVulkan.pVkImage);

	if (pTexture->mOwnsImage)
	{
		const TinyImageFormat fmt = (TinyImageFormat)pTexture->mFormat;
		const bool            isSinglePlane = TinyImageFormat_IsSinglePlane(fmt);
		if (isSinglePlane)
		{
			vmaDestroyImage(pRenderer->mVulkan.pVmaAllocator, pTexture->mVulkan.pVkImage, pTexture->mVulkan.pVkAllocation);
		}
		else
		{
			vkDestroyImage(pRenderer->mVulkan.pVkDevice, pTexture->mVulkan.pVkImage, &gVkAllocationCallbacks);
			vkFreeMemory(pRenderer->mVulkan.pVkDevice, pTexture->mVulkan.pVkDeviceMemory, &gVkAllocationCallbacks);
		}
	}

	if (VK_NULL_HANDLE != pTexture->mVulkan.pVkSRVDescriptor)
		vkDestroyImageView(pRenderer->mVulkan.pVkDevice, pTexture->mVulkan.pVkSRVDescriptor, &gVkAllocationCallbacks);

	if (VK_NULL_HANDLE != pTexture->mVulkan.pVkSRVStencilDescriptor)
		vkDestroyImageView(pRenderer->mVulkan.pVkDevice, pTexture->mVulkan.pVkSRVStencilDescriptor, &gVkAllocationCallbacks);

	if (pTexture->mVulkan.pVkUAVDescriptors)
	{
		for (uint32_t i = 0; i < pTexture->mMipLevels; ++i)
		{
			vkDestroyImageView(pRenderer->mVulkan.pVkDevice, pTexture->mVulkan.pVkUAVDescriptors[i], &gVkAllocationCallbacks);
		}
	}

	if (pTexture->pSvt)
	{
		removeVirtualTexture(pRenderer, pTexture->pSvt);
	}

	SAFE_FREE(pTexture);
}

void vk_addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppRenderTarget);

	bool const isDepth = TinyImageFormat_IsDepthOnly(pDesc->mFormat) || TinyImageFormat_IsDepthAndStencil(pDesc->mFormat);

	ASSERT(!((isDepth) && (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)) && "Cannot use depth stencil as UAV");

	((RenderTargetDesc*)pDesc)->mMipLevels = max(1U, pDesc->mMipLevels);

    uint arraySize = pDesc->mArraySize;
#if defined(QUEST_VR)
    if (pDesc->mFlags & TEXTURE_CREATION_FLAG_VR_MULTIVIEW)
    {
        ASSERT(arraySize == 1 && pDesc->mDepth == 1);
        arraySize = 2; // TODO: Support non multiview rendering
    }
#endif

	uint32_t depthOrArraySize = arraySize * pDesc->mDepth;
	uint32_t numRTVs = pDesc->mMipLevels;
	if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		numRTVs *= depthOrArraySize;
	size_t totalSize = sizeof(RenderTarget);
	totalSize += numRTVs * sizeof(VkImageView);
	RenderTarget* pRenderTarget = (RenderTarget*)tf_calloc_memalign(1, alignof(RenderTarget), totalSize);
	ASSERT(pRenderTarget);

	pRenderTarget->mVulkan.pVkSliceDescriptors = (VkImageView*)(pRenderTarget + 1);

	// Monotonically increasing thread safe id generation
	pRenderTarget->mVulkan.mId = tfrg_atomic32_add_relaxed(&gRenderTargetIds, 1);

	TextureDesc textureDesc = {};
	textureDesc.mArraySize = arraySize;
	textureDesc.mClearValue = pDesc->mClearValue;
	textureDesc.mDepth = pDesc->mDepth;
	textureDesc.mFlags = pDesc->mFlags;
	textureDesc.mFormat = pDesc->mFormat;
	textureDesc.mHeight = pDesc->mHeight;
	textureDesc.mMipLevels = pDesc->mMipLevels;
	textureDesc.mSampleCount = pDesc->mSampleCount;
	textureDesc.mSampleQuality = pDesc->mSampleQuality;
	textureDesc.mWidth = pDesc->mWidth;
	textureDesc.pNativeHandle = pDesc->pNativeHandle;
	textureDesc.mNodeIndex = pDesc->mNodeIndex;
	textureDesc.pSharedNodeIndices = pDesc->pSharedNodeIndices;
	textureDesc.mSharedNodeIndexCount = pDesc->mSharedNodeIndexCount;

	if (!isDepth)
		textureDesc.mStartState |= RESOURCE_STATE_RENDER_TARGET;
	else
		textureDesc.mStartState |= RESOURCE_STATE_DEPTH_WRITE;

	// Set this by default to be able to sample the rendertarget in shader
	textureDesc.mDescriptors = pDesc->mDescriptors;
	// Create SRV by default for a render target
	textureDesc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE;

	if (isDepth)
	{
		// Make sure depth/stencil format is supported - fall back to VK_FORMAT_D16_UNORM if not
		VkFormat vk_depth_stencil_format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mFormat);
		if (VK_FORMAT_UNDEFINED != vk_depth_stencil_format)
		{
			DECLARE_ZERO(VkImageFormatProperties, properties);
			VkResult vk_res = vkGetPhysicalDeviceImageFormatProperties(
				pRenderer->mVulkan.pVkActiveGPU, vk_depth_stencil_format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &properties);
			// Fall back to something that's guaranteed to work
			if (VK_SUCCESS != vk_res)
			{
				textureDesc.mFormat = TinyImageFormat_D16_UNORM;
				LOGF(LogLevel::eWARNING, "Depth stencil format (%u) not supported. Falling back to D16 format", pDesc->mFormat);
			}
		}
	}

	textureDesc.pName = pDesc->pName;

	addTexture(pRenderer, &textureDesc, &pRenderTarget->pTexture);

	VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
	if (pDesc->mHeight > 1)
		viewType = depthOrArraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
	else
		viewType = depthOrArraySize > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;

	VkImageViewCreateInfo rtvDesc = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL };
	rtvDesc.flags = 0;
	rtvDesc.image = pRenderTarget->pTexture->mVulkan.pVkImage;
	rtvDesc.viewType = viewType;
	rtvDesc.format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mFormat);
	rtvDesc.components.r = VK_COMPONENT_SWIZZLE_R;
	rtvDesc.components.g = VK_COMPONENT_SWIZZLE_G;
	rtvDesc.components.b = VK_COMPONENT_SWIZZLE_B;
	rtvDesc.components.a = VK_COMPONENT_SWIZZLE_A;
	rtvDesc.subresourceRange.aspectMask = util_vk_determine_aspect_mask(rtvDesc.format, true);
	rtvDesc.subresourceRange.baseMipLevel = 0;
	rtvDesc.subresourceRange.levelCount = 1;
	rtvDesc.subresourceRange.baseArrayLayer = 0;
	rtvDesc.subresourceRange.layerCount = depthOrArraySize;

	CHECK_VKRESULT(
		vkCreateImageView(pRenderer->mVulkan.pVkDevice, &rtvDesc, &gVkAllocationCallbacks, &pRenderTarget->mVulkan.pVkDescriptor));

	for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
	{
		rtvDesc.subresourceRange.baseMipLevel = i;
		if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
			(pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		{
			for (uint32_t j = 0; j < depthOrArraySize; ++j)
			{
				rtvDesc.subresourceRange.layerCount = 1;
				rtvDesc.subresourceRange.baseArrayLayer = j;
				CHECK_VKRESULT(vkCreateImageView(
					pRenderer->mVulkan.pVkDevice, &rtvDesc, &gVkAllocationCallbacks,
					&pRenderTarget->mVulkan.pVkSliceDescriptors[i * depthOrArraySize + j]));
			}
		}
		else
		{
			CHECK_VKRESULT(vkCreateImageView(
				pRenderer->mVulkan.pVkDevice, &rtvDesc, &gVkAllocationCallbacks, &pRenderTarget->mVulkan.pVkSliceDescriptors[i]));
		}
	}

	pRenderTarget->mWidth = pDesc->mWidth;
	pRenderTarget->mHeight = pDesc->mHeight;
	pRenderTarget->mArraySize = arraySize;
	pRenderTarget->mDepth = pDesc->mDepth;
	pRenderTarget->mMipLevels = pDesc->mMipLevels;
	pRenderTarget->mSampleCount = pDesc->mSampleCount;
	pRenderTarget->mSampleQuality = pDesc->mSampleQuality;
	pRenderTarget->mFormat = pDesc->mFormat;
	pRenderTarget->mClearValue = pDesc->mClearValue;
    pRenderTarget->mVRMultiview = (pDesc->mFlags & TEXTURE_CREATION_FLAG_VR_MULTIVIEW) != 0;
    pRenderTarget->mVRFoveatedRendering = (pDesc->mFlags & TEXTURE_CREATION_FLAG_VR_FOVEATED_RENDERING) != 0;

	// Unlike DX12, Vulkan textures start in undefined layout.
	// To keep in line with DX12, we transition them to the specified layout manually so app code doesn't have to worry about this
	// Render targets wont be created during runtime so this overhead will be minimal
	util_initial_transition(pRenderer, pRenderTarget->pTexture, pDesc->mStartState);

	*ppRenderTarget = pRenderTarget;
}

void vk_removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
	::removeTexture(pRenderer, pRenderTarget->pTexture);

	vkDestroyImageView(pRenderer->mVulkan.pVkDevice, pRenderTarget->mVulkan.pVkDescriptor, &gVkAllocationCallbacks);

	const uint32_t depthOrArraySize = pRenderTarget->mArraySize * pRenderTarget->mDepth;
	if ((pRenderTarget->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pRenderTarget->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
	{
		for (uint32_t i = 0; i < pRenderTarget->mMipLevels; ++i)
			for (uint32_t j = 0; j < depthOrArraySize; ++j)
				vkDestroyImageView(
					pRenderer->mVulkan.pVkDevice, pRenderTarget->mVulkan.pVkSliceDescriptors[i * depthOrArraySize + j],
					&gVkAllocationCallbacks);
	}
	else
	{
		for (uint32_t i = 0; i < pRenderTarget->mMipLevels; ++i)
			vkDestroyImageView(pRenderer->mVulkan.pVkDevice, pRenderTarget->mVulkan.pVkSliceDescriptors[i], &gVkAllocationCallbacks);
	}

	SAFE_FREE(pRenderTarget);
}

void vk_addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
	ASSERT(pRenderer);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);
	ASSERT(pDesc->mCompareFunc < MAX_COMPARE_MODES);
	ASSERT(ppSampler);

	Sampler* pSampler = (Sampler*)tf_calloc_memalign(1, alignof(Sampler), sizeof(Sampler));
	ASSERT(pSampler);

	DECLARE_ZERO(VkSamplerCreateInfo, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.magFilter = util_to_vk_filter(pDesc->mMagFilter);
	add_info.minFilter = util_to_vk_filter(pDesc->mMinFilter);
	add_info.mipmapMode = util_to_vk_mip_map_mode(pDesc->mMipMapMode);
	add_info.addressModeU = util_to_vk_address_mode(pDesc->mAddressU);
	add_info.addressModeV = util_to_vk_address_mode(pDesc->mAddressV);
	add_info.addressModeW = util_to_vk_address_mode(pDesc->mAddressW);
	add_info.mipLodBias = pDesc->mMipLodBias;
	add_info.anisotropyEnable = (pDesc->mMaxAnisotropy > 0.0f) ? VK_TRUE : VK_FALSE;
	add_info.maxAnisotropy = pDesc->mMaxAnisotropy;
	add_info.compareEnable = (gVkComparisonFuncTranslator[pDesc->mCompareFunc] != VK_COMPARE_OP_NEVER) ? VK_TRUE : VK_FALSE;
	add_info.compareOp = gVkComparisonFuncTranslator[pDesc->mCompareFunc];
	add_info.minLod = 0.0f;
	add_info.maxLod = ((pDesc->mMipMapMode == MIPMAP_MODE_LINEAR) ? FLT_MAX : 0.0f);
	add_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	add_info.unnormalizedCoordinates = VK_FALSE;

	if (TinyImageFormat_IsPlanar(pDesc->mSamplerConversionDesc.mFormat))
	{
		auto&    conversionDesc = pDesc->mSamplerConversionDesc;
		VkFormat format = (VkFormat)TinyImageFormat_ToVkFormat(conversionDesc.mFormat);

		// Check format props
		{
			ASSERT(gYCbCrExtension);

			DECLARE_ZERO(VkFormatProperties, format_props);
			vkGetPhysicalDeviceFormatProperties(pRenderer->mVulkan.pVkActiveGPU, format, &format_props);
			if (conversionDesc.mChromaOffsetX == SAMPLE_LOCATION_MIDPOINT)
			{
				ASSERT(format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT);
			}
			else if (conversionDesc.mChromaOffsetX == SAMPLE_LOCATION_COSITED)
			{
				ASSERT(format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT);
			}
		}

		DECLARE_ZERO(VkSamplerYcbcrConversionCreateInfo, conversion_info);
		conversion_info.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
		conversion_info.pNext = NULL;
		conversion_info.format = format;
		conversion_info.ycbcrModel = (VkSamplerYcbcrModelConversion)conversionDesc.mModel;
		conversion_info.ycbcrRange = (VkSamplerYcbcrRange)conversionDesc.mRange;
		conversion_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
									   VK_COMPONENT_SWIZZLE_IDENTITY };
		conversion_info.xChromaOffset = (VkChromaLocation)conversionDesc.mChromaOffsetX;
		conversion_info.yChromaOffset = (VkChromaLocation)conversionDesc.mChromaOffsetY;
		conversion_info.chromaFilter = util_to_vk_filter(conversionDesc.mChromaFilter);
		conversion_info.forceExplicitReconstruction = conversionDesc.mForceExplicitReconstruction ? VK_TRUE : VK_FALSE;
		CHECK_VKRESULT(vkCreateSamplerYcbcrConversion(
			pRenderer->mVulkan.pVkDevice, &conversion_info, &gVkAllocationCallbacks, &pSampler->mVulkan.pVkSamplerYcbcrConversion));

		pSampler->mVulkan.mVkSamplerYcbcrConversionInfo = {};
		pSampler->mVulkan.mVkSamplerYcbcrConversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO;
		pSampler->mVulkan.mVkSamplerYcbcrConversionInfo.pNext = NULL;
		pSampler->mVulkan.mVkSamplerYcbcrConversionInfo.conversion = pSampler->mVulkan.pVkSamplerYcbcrConversion;
		add_info.pNext = &pSampler->mVulkan.mVkSamplerYcbcrConversionInfo;
	}

	CHECK_VKRESULT(vkCreateSampler(pRenderer->mVulkan.pVkDevice, &add_info, &gVkAllocationCallbacks, &(pSampler->mVulkan.pVkSampler)));

	*ppSampler = pSampler;
}

void vk_removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
	ASSERT(pRenderer);
	ASSERT(pSampler);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);
	ASSERT(VK_NULL_HANDLE != pSampler->mVulkan.pVkSampler);

	vkDestroySampler(pRenderer->mVulkan.pVkDevice, pSampler->mVulkan.pVkSampler, &gVkAllocationCallbacks);

	if (NULL != pSampler->mVulkan.pVkSamplerYcbcrConversion)
	{
		vkDestroySamplerYcbcrConversion(pRenderer->mVulkan.pVkDevice, pSampler->mVulkan.pVkSamplerYcbcrConversion, &gVkAllocationCallbacks);
	}

	SAFE_FREE(pSampler);
}

/************************************************************************/
// Buffer Functions
/************************************************************************/
void vk_mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
	ASSERT(pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to map non-cpu accessible resource");

	VkResult vk_res = vmaMapMemory(pRenderer->mVulkan.pVmaAllocator, pBuffer->mVulkan.pVkAllocation, &pBuffer->pCpuMappedAddress);
	ASSERT(vk_res == VK_SUCCESS);

	if (pRange)
	{
		pBuffer->pCpuMappedAddress = ((uint8_t*)pBuffer->pCpuMappedAddress + pRange->mOffset);
	}
}

void vk_unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	ASSERT(pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to unmap non-cpu accessible resource");

	vmaUnmapMemory(pRenderer->mVulkan.pVmaAllocator, pBuffer->mVulkan.pVkAllocation);
	pBuffer->pCpuMappedAddress = NULL;
}
/************************************************************************/
// Descriptor Set Functions
/************************************************************************/
void vk_addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppDescriptorSet);

	const RootSignature*            pRootSignature = pDesc->pRootSignature;
	const DescriptorUpdateFrequency updateFreq = pDesc->mUpdateFrequency;
	const uint32_t                  nodeIndex = pDesc->mNodeIndex;
	const uint32_t                  descriptorCount = pRootSignature->mVulkan.mVkCumulativeDescriptorCounts[updateFreq];
	const uint32_t                  dynamicOffsetCount = pRootSignature->mVulkan.mVkDynamicDescriptorCounts[updateFreq];

	uint32_t totalSize = sizeof(DescriptorSet);
	if (VK_NULL_HANDLE != pRootSignature->mVulkan.mVkDescriptorSetLayouts[updateFreq])
	{
		totalSize += pDesc->mMaxSets * sizeof(VkDescriptorSet);
		totalSize += pDesc->mMaxSets * sizeof(DescriptorUpdateData*);
		totalSize += pDesc->mMaxSets * descriptorCount * sizeof(DescriptorUpdateData);
	}
	if (dynamicOffsetCount)
	{
		ASSERT(1 == dynamicOffsetCount);
		totalSize += pDesc->mMaxSets * sizeof(SizeOffset);
	}

	DescriptorSet* pDescriptorSet = (DescriptorSet*)tf_calloc_memalign(1, alignof(DescriptorSet), totalSize);

	pDescriptorSet->mVulkan.pRootSignature = pRootSignature;
	pDescriptorSet->mVulkan.mUpdateFrequency = updateFreq;
	pDescriptorSet->mVulkan.mDynamicOffsetCount = dynamicOffsetCount;
	pDescriptorSet->mVulkan.mNodeIndex = nodeIndex;
	pDescriptorSet->mVulkan.mMaxSets = pDesc->mMaxSets;

	uint8_t* pMem = (uint8_t*)(pDescriptorSet + 1);
	pDescriptorSet->mVulkan.pHandles = (VkDescriptorSet*)pMem;

	if (VK_NULL_HANDLE != pRootSignature->mVulkan.mVkDescriptorSetLayouts[updateFreq])
	{
		pMem += pDesc->mMaxSets * sizeof(VkDescriptorSet);

		pDescriptorSet->mVulkan.ppUpdateData = (DescriptorUpdateData**)pMem;
		pMem += pDesc->mMaxSets * sizeof(DescriptorUpdateData*);

		VkDescriptorSetLayout* pLayouts = (VkDescriptorSetLayout*)alloca(pDesc->mMaxSets * sizeof(VkDescriptorSetLayout));
		VkDescriptorSet**      pHandles = (VkDescriptorSet**)alloca(pDesc->mMaxSets * sizeof(VkDescriptorSet*));

		for (uint32_t i = 0; i < pDesc->mMaxSets; ++i)
		{
			pLayouts[i] = pRootSignature->mVulkan.mVkDescriptorSetLayouts[updateFreq];
			pHandles[i] = &pDescriptorSet->mVulkan.pHandles[i];

			pDescriptorSet->mVulkan.ppUpdateData[i] = (DescriptorUpdateData*)pMem;
			pMem += descriptorCount * sizeof(DescriptorUpdateData);
			memcpy(
				pDescriptorSet->mVulkan.ppUpdateData[i],
				pRootSignature->mVulkan.pUpdateTemplateData[updateFreq][pDescriptorSet->mVulkan.mNodeIndex],
				descriptorCount * sizeof(DescriptorUpdateData));
		}

		consume_descriptor_sets(pRenderer->mVulkan.pDescriptorPool, pLayouts, pHandles, pDesc->mMaxSets);
	}
	else
	{
		LOGF(LogLevel::eERROR, "NULL Descriptor Set Layout for update frequency %u. Cannot allocate descriptor set", (uint32_t)updateFreq);
		ASSERT(false && "NULL Descriptor Set Layout for update frequency. Cannot allocate descriptor set");
	}

	if (pDescriptorSet->mVulkan.mDynamicOffsetCount)
	{
		ASSERT(1 == pDescriptorSet->mVulkan.mDynamicOffsetCount);
		pDescriptorSet->mVulkan.pDynamicSizeOffsets = (SizeOffset*)pMem;
		pMem += pDescriptorSet->mVulkan.mMaxSets * sizeof(SizeOffset);
	}

	*ppDescriptorSet = pDescriptorSet;
}

void vk_removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
{
	ASSERT(pRenderer);
	ASSERT(pDescriptorSet);

	SAFE_FREE(pDescriptorSet);
}

void vk_updateDescriptorSet(
	Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
#define VALIDATE_DESCRIPTOR(descriptor, ...)                                                            \
	if (!(descriptor))                                                                                  \
	{                                                                                                   \
		eastl::string msg = __FUNCTION__ + eastl::string(" : ") + eastl::string().sprintf(__VA_ARGS__); \
		LOGF(LogLevel::eERROR, msg.c_str());                                                            \
		_FailedAssert(__FILE__, __LINE__, msg.c_str());                                                 \
		continue;                                                                                       \
	}
#else
#define VALIDATE_DESCRIPTOR(descriptor, ...)
#endif

	ASSERT(pRenderer);
	ASSERT(pDescriptorSet);
	ASSERT(pDescriptorSet->mVulkan.pHandles);
	ASSERT(index < pDescriptorSet->mVulkan.mMaxSets);

	const RootSignature*      pRootSignature = pDescriptorSet->mVulkan.pRootSignature;
	DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)pDescriptorSet->mVulkan.mUpdateFrequency;
	DescriptorUpdateData*     pUpdateData = pDescriptorSet->mVulkan.ppUpdateData[index];
	bool                      update = false;

#ifdef ENABLE_RAYTRACING
	VkWriteDescriptorSet*                        raytracingWrites = NULL;
	VkWriteDescriptorSetAccelerationStructureNV* raytracingWritesNV = NULL;
	uint32_t                                     raytracingWriteCount = 0;

	if (pRootSignature->mVulkan.mVkRaytracingDescriptorCounts[updateFreq])
	{
		raytracingWrites =
			(VkWriteDescriptorSet*)alloca(pRootSignature->mVulkan.mVkRaytracingDescriptorCounts[updateFreq] * sizeof(VkWriteDescriptorSet));
		raytracingWritesNV = (VkWriteDescriptorSetAccelerationStructureNV*)alloca(
			pRootSignature->mVulkan.mVkRaytracingDescriptorCounts[updateFreq] * sizeof(VkWriteDescriptorSetAccelerationStructureNV));
	}
#endif

	for (uint32_t i = 0; i < count; ++i)
	{
		const DescriptorData* pParam = pParams + i;
		uint32_t              paramIndex = pParam->mIndex;

		VALIDATE_DESCRIPTOR(pParam->pName || (paramIndex != -1), "DescriptorData has NULL name and invalid index");

		const DescriptorInfo* pDesc =
			(paramIndex != -1) ? (pRootSignature->pDescriptors + paramIndex) : get_descriptor(pRootSignature, pParam->pName);
		if (paramIndex != -1)
		{
			VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param index (%u)", paramIndex);
		}
		else
		{
			VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param name (%s)", pParam->pName);
		}

		const DescriptorType type = (DescriptorType)pDesc->mType;    //-V522
		const uint32_t       arrayCount = max(1U, pParam->mCount);

		VALIDATE_DESCRIPTOR(
			pDesc->mUpdateFrequency == updateFreq, "Descriptor (%s) - Mismatching update frequency and set index", pDesc->pName);

		switch (type)
		{
			case DESCRIPTOR_TYPE_SAMPLER:
			{
				// Index is invalid when descriptor is a static sampler
				VALIDATE_DESCRIPTOR(
					pDesc->mIndexInParent != -1,
					"Trying to update a static sampler (%s). All static samplers must be set in addRootSignature and cannot be updated "
					"later",
					pDesc->pName);

				VALIDATE_DESCRIPTOR(pParam->ppSamplers, "NULL Sampler (%s)", pDesc->pName);

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppSamplers[arr], "NULL Sampler (%s [%u] )", pDesc->pName, arr);

					pUpdateData[pDesc->mHandleIndex + arr].mImageInfo = { pParam->ppSamplers[arr]->mVulkan.pVkSampler, VK_NULL_HANDLE };
					update = true;
				}
				break;
			}
			case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			{
				VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)", pDesc->pName);

				DescriptorNameToIndexMap::const_iterator it = pRootSignature->pDescriptorNameToIndexMap->mMap.find(pDesc->pName);
				if (it == pRootSignature->pDescriptorNameToIndexMap->mMap.end())
				{
					LOGF(LogLevel::eERROR, "No Static Sampler called (%s)", pDesc->pName);
					ASSERT(false);
				}

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);

					pUpdateData[pDesc->mHandleIndex + arr].mImageInfo = {
						NULL,                                                 // Sampler
						pParam->ppTextures[arr]->mVulkan.pVkSRVDescriptor,    // Image View
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL              // Image Layout
					};

					update = true;
				}
				break;
			}
			case DESCRIPTOR_TYPE_TEXTURE:
			{
				VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)", pDesc->pName);

				if (!pParam->mBindStencilResource)
				{
					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);

						pUpdateData[pDesc->mHandleIndex + arr].mImageInfo = {
							VK_NULL_HANDLE,                                       // Sampler
							pParam->ppTextures[arr]->mVulkan.pVkSRVDescriptor,    // Image View
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL              // Image Layout
						};

						update = true;
					}
				}
				else
				{
					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);

						pUpdateData[pDesc->mHandleIndex + arr].mImageInfo = {
							VK_NULL_HANDLE,                                              // Sampler
							pParam->ppTextures[arr]->mVulkan.pVkSRVStencilDescriptor,    // Image View
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL                     // Image Layout
						};

						update = true;
					}
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
						pUpdateData[pDesc->mHandleIndex + arr].mImageInfo = {
							VK_NULL_HANDLE,                                           // Sampler
							pParam->ppTextures[0]->mVulkan.pVkUAVDescriptors[arr],    // Image View
							VK_IMAGE_LAYOUT_GENERAL                                   // Image Layout
						};

						update = true;
					}
				}
				else
				{
					const uint32_t mipSlice = pParam->mUAVMipSlice;

					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL RW Texture (%s [%u] )", pDesc->pName, arr);
						VALIDATE_DESCRIPTOR(
							mipSlice < pParam->ppTextures[arr]->mMipLevels,
							"Descriptor : (%s [%u] ) Mip Slice (%u) exceeds mip levels (%u)", pDesc->pName, arr, mipSlice,
							pParam->ppTextures[arr]->mMipLevels);

						pUpdateData[pDesc->mHandleIndex + arr].mImageInfo = {
							VK_NULL_HANDLE,                                                  // Sampler
							pParam->ppTextures[arr]->mVulkan.pVkUAVDescriptors[mipSlice],    // Image View
							VK_IMAGE_LAYOUT_GENERAL                                          // Image Layout
						};

						update = true;
					}
				}
				break;
			}
			case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			{
				if (pDesc->mVulkan.mVkType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Uniform Buffer (%s)", pDesc->pName);
					VALIDATE_DESCRIPTOR(pParam->ppBuffers[0], "NULL Uniform Buffer (%s [%u] )", pDesc->pName, 0);
					VALIDATE_DESCRIPTOR(
						arrayCount == 1, "Descriptor (%s) : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC does not support arrays",
						pDesc->pName);
					VALIDATE_DESCRIPTOR(
						pParam->pSizes, "Descriptor (%s) : Must provide pSizes for VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC",
						pDesc->pName);
					VALIDATE_DESCRIPTOR(pParam->pSizes[0] > 0, "Descriptor (%s) - pSizes[%u] is zero", pDesc->pName, 0);
					VALIDATE_DESCRIPTOR(
						pParam->pSizes[0] <= pRenderer->mVulkan.pVkActiveGPUProperties->properties.limits.maxUniformBufferRange,
						"Descriptor (%s) - pSizes[%u] is %ull which exceeds max size %u", pDesc->pName, 0, pParam->pSizes[0],
						pRenderer->mVulkan.pVkActiveGPUProperties->properties.limits.maxUniformBufferRange);

					pDescriptorSet->mVulkan.pDynamicSizeOffsets[index].mOffset = pParam->pOffsets ? (uint32_t)pParam->pOffsets[0] : 0;
					pUpdateData[pDesc->mHandleIndex + 0].mBufferInfo = { pParam->ppBuffers[0]->mVulkan.pVkBuffer,
																		 pParam->ppBuffers[0]->mVulkan.mOffset, pParam->pSizes[0] };

					// If this is a different size we have to update the VkDescriptorBufferInfo::range so a call to vkUpdateDescriptorSet is necessary
					if (pParam->pSizes[0] != (uint32_t)pDescriptorSet->mVulkan.pDynamicSizeOffsets[index].mSize)
					{
						pDescriptorSet->mVulkan.pDynamicSizeOffsets[index].mSize = (uint32_t)pParam->pSizes[0];
						update = true;
					}

					break;
				}
				case DESCRIPTOR_TYPE_BUFFER:
				case DESCRIPTOR_TYPE_BUFFER_RAW:
				case DESCRIPTOR_TYPE_RW_BUFFER:
				case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Buffer (%s)", pDesc->pName);

					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Buffer (%s [%u] )", pDesc->pName, arr);

						pUpdateData[pDesc->mHandleIndex + arr].mBufferInfo = { pParam->ppBuffers[arr]->mVulkan.pVkBuffer,
																			   pParam->ppBuffers[arr]->mVulkan.mOffset, VK_WHOLE_SIZE };
						if (pParam->pOffsets)
						{
							VALIDATE_DESCRIPTOR(pParam->pSizes, "Descriptor (%s) - pSizes must be provided with pOffsets", pDesc->pName);
							VALIDATE_DESCRIPTOR(pParam->pSizes[arr] > 0, "Descriptor (%s) - pSizes[%u] is zero", pDesc->pName, arr);
							VALIDATE_DESCRIPTOR(
								pParam->pSizes[arr] <= pRenderer->mVulkan.pVkActiveGPUProperties->properties.limits.maxUniformBufferRange,
								"Descriptor (%s) - pSizes[%u] is %ull which exceeds max size %u", pDesc->pName, arr, pParam->pSizes[arr],
								pRenderer->mVulkan.pVkActiveGPUProperties->properties.limits.maxUniformBufferRange);

							pUpdateData[pDesc->mHandleIndex + arr].mBufferInfo.offset = pParam->pOffsets[arr];
							pUpdateData[pDesc->mHandleIndex + arr].mBufferInfo.range = pParam->pSizes[arr];
						}

						update = true;
					}
				}
				break;
			}
			case DESCRIPTOR_TYPE_TEXEL_BUFFER:
			{
				VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Texel Buffer (%s)", pDesc->pName);

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Texel Buffer (%s [%u] )", pDesc->pName, arr);
					pUpdateData[pDesc->mHandleIndex + arr].mBuferView = pParam->ppBuffers[arr]->mVulkan.pVkUniformTexelView;
					update = true;
				}

				break;
			}
			case DESCRIPTOR_TYPE_RW_TEXEL_BUFFER:
			{
				VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL RW Texel Buffer (%s)", pDesc->pName);

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL RW Texel Buffer (%s [%u] )", pDesc->pName, arr);
					pUpdateData[pDesc->mHandleIndex + arr].mBuferView = pParam->ppBuffers[arr]->mVulkan.pVkStorageTexelView;
					update = true;
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

					ASSERT(raytracingWrites && raytracingWritesNV);
					VkWriteDescriptorSet*                        pWrite = raytracingWrites + raytracingWriteCount;        //-V769
					VkWriteDescriptorSetAccelerationStructureNV* pWriteNV = raytracingWritesNV + raytracingWriteCount;    //-V769

					pWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;    //-V522
					pWrite->pNext = pWriteNV;
					pWrite->dstSet = pDescriptorSet->mVulkan.pHandles[index];
					pWrite->descriptorCount = 1;
					pWrite->descriptorType = (VkDescriptorType)pDesc->mVulkan.mVkType;
					pWrite->dstArrayElement = arr;
					pWrite->dstBinding = pDesc->mVulkan.mReg;

					vk_FillRaytracingDescriptorData(pParam->ppAccelerationStructures[arr], pWriteNV);

					++raytracingWriteCount;
				}
				break;
			}
#endif
			default: break;
		}
	}

	// If this was called to just update a dynamic offset skip the update
	if (update)
		vkUpdateDescriptorSetWithTemplateKHR(
			pRenderer->mVulkan.pVkDevice, pDescriptorSet->mVulkan.pHandles[index], pRootSignature->mVulkan.mUpdateTemplates[updateFreq],
			pUpdateData);

#ifdef ENABLE_RAYTRACING
	// Raytracing Update Descriptor Set since it does not support update template
	if (raytracingWriteCount)
		vkUpdateDescriptorSets(pRenderer->mVulkan.pVkDevice, raytracingWriteCount, raytracingWrites, 0, NULL);
#endif
}

void vk_cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
{
	ASSERT(pCmd);
	ASSERT(pDescriptorSet);
	ASSERT(pDescriptorSet->mVulkan.pHandles);
	ASSERT(index < pDescriptorSet->mVulkan.mMaxSets);

	const RootSignature* pRootSignature = pDescriptorSet->mVulkan.pRootSignature;

	if (pCmd->mVulkan.pBoundPipelineLayout != pRootSignature->mVulkan.pPipelineLayout)
	{
		pCmd->mVulkan.pBoundPipelineLayout = pRootSignature->mVulkan.pPipelineLayout;

		// Vulkan requires to bind all descriptor sets upto the highest set number even if they are empty
		// Example: If shader uses only set 2, we still have to bind empty sets for set=0 and set=1
		for (uint32_t setIndex = 0; setIndex < DESCRIPTOR_UPDATE_FREQ_COUNT; ++setIndex)
		{
			if (pRootSignature->mVulkan.mVkEmptyDescriptorSets[setIndex] != VK_NULL_HANDLE)
			{
				vkCmdBindDescriptorSets(
					pCmd->mVulkan.pVkCmdBuf, gPipelineBindPoint[pRootSignature->mPipelineType], pRootSignature->mVulkan.pPipelineLayout,
					setIndex, 1, &pRootSignature->mVulkan.mVkEmptyDescriptorSets[setIndex], 0, NULL);
			}
		}
	}

	vkCmdBindDescriptorSets(
		pCmd->mVulkan.pVkCmdBuf, gPipelineBindPoint[pRootSignature->mPipelineType], pRootSignature->mVulkan.pPipelineLayout,
		pDescriptorSet->mVulkan.mUpdateFrequency, 1, &pDescriptorSet->mVulkan.pHandles[index], pDescriptorSet->mVulkan.mDynamicOffsetCount,
		pDescriptorSet->mVulkan.mDynamicOffsetCount ? &pDescriptorSet->mVulkan.pDynamicSizeOffsets[index].mOffset : NULL);
}

void vk_cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, const char* pName, const void* pConstants)
{
	ASSERT(pCmd);
	ASSERT(pConstants);
	ASSERT(pRootSignature);
	ASSERT(pName);

	const DescriptorInfo* pDesc = get_descriptor(pRootSignature, pName);
	ASSERT(pDesc);
	ASSERT(DESCRIPTOR_TYPE_ROOT_CONSTANT == pDesc->mType);    //-V522

	vkCmdPushConstants(
		pCmd->mVulkan.pVkCmdBuf, pRootSignature->mVulkan.pPipelineLayout, pDesc->mVulkan.mVkStages, 0, pDesc->mSize, pConstants);
}

void vk_cmdBindPushConstantsByIndex(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
{
	ASSERT(pCmd);
	ASSERT(pConstants);
	ASSERT(pRootSignature);
	ASSERT(paramIndex >= 0 && paramIndex < pRootSignature->mDescriptorCount);

	const DescriptorInfo* pDesc = pRootSignature->pDescriptors + paramIndex;
	ASSERT(pDesc);
	ASSERT(DESCRIPTOR_TYPE_ROOT_CONSTANT == pDesc->mType);

	vkCmdPushConstants(
		pCmd->mVulkan.pVkCmdBuf, pRootSignature->mVulkan.pPipelineLayout, pDesc->mVulkan.mVkStages, 0, pDesc->mSize, pConstants);
}
/************************************************************************/
// Shader Functions
/************************************************************************/
void vk_addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppShaderProgram);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);

	uint32_t counter = 0;

	size_t totalSize = sizeof(Shader);
	totalSize += sizeof(PipelineReflection);

	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		ShaderStage stage_mask = (ShaderStage)(1 << i);
		if (stage_mask == (pDesc->mStages & stage_mask))
		{
			switch (stage_mask)
			{
				case SHADER_STAGE_VERT: totalSize += (strlen(pDesc->mVert.pEntryPoint) + 1) * sizeof(char); break;      //-V814
				case SHADER_STAGE_TESC: totalSize += (strlen(pDesc->mHull.pEntryPoint) + 1) * sizeof(char); break;      //-V814
				case SHADER_STAGE_TESE: totalSize += (strlen(pDesc->mDomain.pEntryPoint) + 1) * sizeof(char); break;    //-V814
				case SHADER_STAGE_GEOM: totalSize += (strlen(pDesc->mGeom.pEntryPoint) + 1) * sizeof(char); break;      //-V814
				case SHADER_STAGE_FRAG: totalSize += (strlen(pDesc->mFrag.pEntryPoint) + 1) * sizeof(char); break;      //-V814
				case SHADER_STAGE_RAYTRACING:
				case SHADER_STAGE_COMP: totalSize += (strlen(pDesc->mComp.pEntryPoint) + 1) * sizeof(char); break;    //-V814
				default: break;
			}
			++counter;
		}
	}

	totalSize += counter * sizeof(VkShaderModule);
	totalSize += counter * sizeof(char*);
	Shader* pShaderProgram = (Shader*)tf_calloc(1, totalSize);
	pShaderProgram->mStages = pDesc->mStages;
	pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1);    //-V1027
	pShaderProgram->mVulkan.pShaderModules = (VkShaderModule*)(pShaderProgram->pReflection + 1);
	pShaderProgram->mVulkan.pEntryNames = (char**)(pShaderProgram->mVulkan.pShaderModules + counter);

	uint8_t* mem = (uint8_t*)(pShaderProgram->mVulkan.pEntryNames + counter);
	counter = 0;
	ShaderReflection stageReflections[SHADER_STAGE_COUNT] = {};

	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		ShaderStage stage_mask = (ShaderStage)(1 << i);
		if (stage_mask == (pShaderProgram->mStages & stage_mask))
		{
			DECLARE_ZERO(VkShaderModuleCreateInfo, create_info);
			create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			create_info.pNext = NULL;
			create_info.flags = 0;

			const BinaryShaderStageDesc* pStageDesc = nullptr;
			switch (stage_mask)
			{
				case SHADER_STAGE_VERT:
				{
					vk_createShaderReflection(
						(const uint8_t*)pDesc->mVert.pByteCode, (uint32_t)pDesc->mVert.mByteCodeSize, stage_mask,
						&stageReflections[counter]);

					create_info.codeSize = pDesc->mVert.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mVert.pByteCode;
					pStageDesc = &pDesc->mVert;
					CHECK_VKRESULT(vkCreateShaderModule(
						pRenderer->mVulkan.pVkDevice, &create_info, &gVkAllocationCallbacks,
						&(pShaderProgram->mVulkan.pShaderModules[counter])));
				}
				break;
				case SHADER_STAGE_TESC:
				{
					vk_createShaderReflection(
						(const uint8_t*)pDesc->mHull.pByteCode, (uint32_t)pDesc->mHull.mByteCodeSize, stage_mask,
						&stageReflections[counter]);

					create_info.codeSize = pDesc->mHull.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mHull.pByteCode;
					pStageDesc = &pDesc->mHull;
					CHECK_VKRESULT(vkCreateShaderModule(
						pRenderer->mVulkan.pVkDevice, &create_info, &gVkAllocationCallbacks,
						&(pShaderProgram->mVulkan.pShaderModules[counter])));
				}
				break;
				case SHADER_STAGE_TESE:
				{
					vk_createShaderReflection(
						(const uint8_t*)pDesc->mDomain.pByteCode, (uint32_t)pDesc->mDomain.mByteCodeSize, stage_mask,
						&stageReflections[counter]);

					create_info.codeSize = pDesc->mDomain.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mDomain.pByteCode;
					pStageDesc = &pDesc->mDomain;
					CHECK_VKRESULT(vkCreateShaderModule(
						pRenderer->mVulkan.pVkDevice, &create_info, &gVkAllocationCallbacks,
						&(pShaderProgram->mVulkan.pShaderModules[counter])));
				}
				break;
				case SHADER_STAGE_GEOM:
				{
					vk_createShaderReflection(
						(const uint8_t*)pDesc->mGeom.pByteCode, (uint32_t)pDesc->mGeom.mByteCodeSize, stage_mask,
						&stageReflections[counter]);

					create_info.codeSize = pDesc->mGeom.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mGeom.pByteCode;
					pStageDesc = &pDesc->mGeom;
					CHECK_VKRESULT(vkCreateShaderModule(
						pRenderer->mVulkan.pVkDevice, &create_info, &gVkAllocationCallbacks,
						&(pShaderProgram->mVulkan.pShaderModules[counter])));
				}
				break;
				case SHADER_STAGE_FRAG:
				{
					vk_createShaderReflection(
						(const uint8_t*)pDesc->mFrag.pByteCode, (uint32_t)pDesc->mFrag.mByteCodeSize, stage_mask,
						&stageReflections[counter]);

					create_info.codeSize = pDesc->mFrag.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mFrag.pByteCode;
					pStageDesc = &pDesc->mFrag;
					CHECK_VKRESULT(vkCreateShaderModule(
						pRenderer->mVulkan.pVkDevice, &create_info, &gVkAllocationCallbacks,
						&(pShaderProgram->mVulkan.pShaderModules[counter])));
				}
				break;
				case SHADER_STAGE_COMP:
#ifdef ENABLE_RAYTRACING
				case SHADER_STAGE_RAYTRACING:
#endif
				{
					vk_createShaderReflection(
						(const uint8_t*)pDesc->mComp.pByteCode, (uint32_t)pDesc->mComp.mByteCodeSize, stage_mask,
						&stageReflections[counter]);

					create_info.codeSize = pDesc->mComp.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mComp.pByteCode;
					pStageDesc = &pDesc->mComp;
					CHECK_VKRESULT(vkCreateShaderModule(
						pRenderer->mVulkan.pVkDevice, &create_info, &gVkAllocationCallbacks,
						&(pShaderProgram->mVulkan.pShaderModules[counter])));
				}
				break;
				default: ASSERT(false && "Shader Stage not supported!"); break;
			}

			pShaderProgram->mVulkan.pEntryNames[counter] = (char*)mem;
			mem += (strlen(pStageDesc->pEntryPoint) + 1) * sizeof(char);    //-V522
			strcpy(pShaderProgram->mVulkan.pEntryNames[counter], pStageDesc->pEntryPoint);
			++counter;
		}
	}

	createPipelineReflection(stageReflections, counter, pShaderProgram->pReflection);

	*ppShaderProgram = pShaderProgram;
}

void vk_removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
	ASSERT(pRenderer);

	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);

	if (pShaderProgram->mStages & SHADER_STAGE_VERT)
	{
		vkDestroyShaderModule(
			pRenderer->mVulkan.pVkDevice, pShaderProgram->mVulkan.pShaderModules[pShaderProgram->pReflection->mVertexStageIndex],
			&gVkAllocationCallbacks);
	}

	if (pShaderProgram->mStages & SHADER_STAGE_TESC)
	{
		vkDestroyShaderModule(
			pRenderer->mVulkan.pVkDevice, pShaderProgram->mVulkan.pShaderModules[pShaderProgram->pReflection->mHullStageIndex],
			&gVkAllocationCallbacks);
	}

	if (pShaderProgram->mStages & SHADER_STAGE_TESE)
	{
		vkDestroyShaderModule(
			pRenderer->mVulkan.pVkDevice, pShaderProgram->mVulkan.pShaderModules[pShaderProgram->pReflection->mDomainStageIndex],
			&gVkAllocationCallbacks);
	}

	if (pShaderProgram->mStages & SHADER_STAGE_GEOM)
	{
		vkDestroyShaderModule(
			pRenderer->mVulkan.pVkDevice, pShaderProgram->mVulkan.pShaderModules[pShaderProgram->pReflection->mGeometryStageIndex],
			&gVkAllocationCallbacks);
	}

	if (pShaderProgram->mStages & SHADER_STAGE_FRAG)
	{
		vkDestroyShaderModule(
			pRenderer->mVulkan.pVkDevice, pShaderProgram->mVulkan.pShaderModules[pShaderProgram->pReflection->mPixelStageIndex],
			&gVkAllocationCallbacks);
	}

	if (pShaderProgram->mStages & SHADER_STAGE_COMP)
	{
		vkDestroyShaderModule(pRenderer->mVulkan.pVkDevice, pShaderProgram->mVulkan.pShaderModules[0], &gVkAllocationCallbacks);
	}
#ifdef ENABLE_RAYTRACING
	if (pShaderProgram->mStages & SHADER_STAGE_RAYTRACING)
	{
		vkDestroyShaderModule(pRenderer->mVulkan.pVkDevice, pShaderProgram->mVulkan.pShaderModules[0], &gVkAllocationCallbacks);
	}
#endif

	destroyPipelineReflection(pShaderProgram->pReflection);
	SAFE_FREE(pShaderProgram);
}
/************************************************************************/
// Root Signature Functions
/************************************************************************/
typedef struct vk_UpdateFrequencyLayoutInfo
{
	/// Array of all bindings in the descriptor set
	eastl::vector<VkDescriptorSetLayoutBinding> mBindings{};
	/// Array of all descriptors in this descriptor set
	eastl::vector<DescriptorInfo*> mDescriptors{};
	/// Array of all descriptors marked as dynamic in this descriptor set (applicable to DESCRIPTOR_TYPE_UNIFORM_BUFFER)
	eastl::vector<DescriptorInfo*> mDynamicDescriptors{};
	/// Hash map to get index of the descriptor in the root signature
	eastl::hash_map<DescriptorInfo*, uint32_t> mDescriptorIndexMap{};
} UpdateFrequencyLayoutInfo;

void vk_addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
{
	ASSERT(pRenderer);
	ASSERT(pRootSignatureDesc);
	ASSERT(ppRootSignature);

	static constexpr uint32_t                kMaxLayoutCount = DESCRIPTOR_UPDATE_FREQ_COUNT;
	UpdateFrequencyLayoutInfo                layouts[kMaxLayoutCount] = {};
	VkPushConstantRange                      pushConstants[SHADER_STAGE_COUNT] = {};
	uint32_t                                 pushConstantCount = 0;
	eastl::vector<ShaderResource>            shaderResources;
	eastl::hash_map<eastl::string, Sampler*> staticSamplerMap;

	for (uint32_t i = 0; i < pRootSignatureDesc->mStaticSamplerCount; ++i)
	{
		ASSERT(pRootSignatureDesc->ppStaticSamplers[i]);
		staticSamplerMap.insert({ { pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i] } });
	}

	PipelineType       pipelineType = PIPELINE_TYPE_UNDEFINED;
	DescriptorIndexMap indexMap;

	// Collect all unique shader resources in the given shaders
	// Resources are parsed by name (two resources named "XYZ" in two shaders will be considered the same resource)
	for (uint32_t sh = 0; sh < pRootSignatureDesc->mShaderCount; ++sh)
	{
		PipelineReflection const* pReflection = pRootSignatureDesc->ppShaders[sh]->pReflection;

		if (pReflection->mShaderStages & SHADER_STAGE_COMP)
			pipelineType = PIPELINE_TYPE_COMPUTE;
#ifdef ENABLE_RAYTRACING
		else if (pReflection->mShaderStages & SHADER_STAGE_RAYTRACING)
			pipelineType = PIPELINE_TYPE_RAYTRACING;
#endif
		else
			pipelineType = PIPELINE_TYPE_GRAPHICS;

		for (uint32_t i = 0; i < pReflection->mShaderResourceCount; ++i)
		{
			ShaderResource const* pRes = &pReflection->pShaderResources[i];
			uint32_t              setIndex = pRes->set;

			if (pRes->type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
				setIndex = 0;

			eastl::string_hash_map<uint32_t>::iterator it = indexMap.mMap.find(pRes->name);
			if (it == indexMap.mMap.end())
			{
				decltype(shaderResources)::iterator it = eastl::find(
					shaderResources.begin(), shaderResources.end(), *pRes, [](const ShaderResource& a, const ShaderResource& b) {
						return (a.type == b.type) && (a.used_stages == b.used_stages) && (((a.reg ^ b.reg) | (a.set ^ b.set)) == 0);
					});
				if (it == shaderResources.end())
				{
					indexMap.mMap.insert(pRes->name, (uint32_t)shaderResources.size());
					shaderResources.push_back(*pRes);
				}
				else
				{
					ASSERT(pRes->type == it->type);
					if (pRes->type != it->type)
					{
						LOGF(
							LogLevel::eERROR,
							"\nFailed to create root signature\n"
							"Shared shader resources %s and %s have mismatching types (%u) and (%u). All shader resources "
							"sharing the same register and space addRootSignature "
							"must have the same type",
							pRes->name, it->name, (uint32_t)pRes->type, (uint32_t)it->type);
						return;
					}

					indexMap.mMap.insert(pRes->name, indexMap.mMap[it->name]);
					it->used_stages |= pRes->used_stages;
				}
			}
			else
			{
				if (shaderResources[it->second].reg != pRes->reg)
				{
					LOGF(
						LogLevel::eERROR,
						"\nFailed to create root signature\n"
						"Shared shader resource %s has mismatching binding. All shader resources "
						"shared by multiple shaders specified in addRootSignature "
						"must have the same binding and set",
						pRes->name);
					return;
				}
				if (shaderResources[it->second].set != pRes->set)
				{
					LOGF(
						LogLevel::eERROR,
						"\nFailed to create root signature\n"
						"Shared shader resource %s has mismatching set. All shader resources "
						"shared by multiple shaders specified in addRootSignature "
						"must have the same binding and set",
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

	pRootSignature->pDescriptors = (DescriptorInfo*)(pRootSignature + 1);                                                        //-V1027
	pRootSignature->pDescriptorNameToIndexMap = (DescriptorIndexMap*)(pRootSignature->pDescriptors + shaderResources.size());    //-V1027
	ASSERT(pRootSignature->pDescriptorNameToIndexMap);
	tf_placement_new<DescriptorIndexMap>(pRootSignature->pDescriptorNameToIndexMap);

	if ((uint32_t)shaderResources.size())
	{
		pRootSignature->mDescriptorCount = (uint32_t)shaderResources.size();
	}

	pRootSignature->mPipelineType = pipelineType;
	pRootSignature->pDescriptorNameToIndexMap->mMap = indexMap.mMap;

	// Fill the descriptor array to be stored in the root signature
	for (uint32_t i = 0; i < (uint32_t)shaderResources.size(); ++i)
	{
		DescriptorInfo*           pDesc = &pRootSignature->pDescriptors[i];
		ShaderResource const*     pRes = &shaderResources[i];
		uint32_t                  setIndex = pRes->set;
		DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)setIndex;

		// Copy the binding information generated from the shader reflection into the descriptor
		pDesc->mVulkan.mReg = pRes->reg;
		pDesc->mSize = pRes->size;
		pDesc->mType = pRes->type;
		pDesc->pName = pRes->name;
		pDesc->mDim = pRes->dim;

		// If descriptor is not a root constant create a new layout binding for this descriptor and add it to the binding array
		if (pDesc->mType != DESCRIPTOR_TYPE_ROOT_CONSTANT)
		{
			VkDescriptorSetLayoutBinding binding = {};
			binding.binding = pRes->reg;
			binding.descriptorCount = pDesc->mSize;
			binding.descriptorType = util_to_vk_descriptor_type((DescriptorType)pDesc->mType);

			eastl::string name = pRes->name;
			name.make_lower();

			// If a user specified a uniform buffer to be used as a dynamic uniform buffer change its type to VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
			// Also log a message for debugging purpose
			if (name.find("rootcbv") != eastl::string::npos)
			{
				if (pDesc->mSize == 1)
				{
					LOGF(LogLevel::eINFO, "Descriptor (%s) : User specified VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC", pDesc->pName);
					binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				}
				else
				{
					LOGF(
						LogLevel::eWARNING, "Descriptor (%s) : Cannot use VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC for arrays",
						pDesc->pName);
				}
			}

			binding.stageFlags = util_to_vk_shader_stage_flags(pRes->used_stages);

			// Store the vulkan related info in the descriptor to avoid constantly calling the util_to_vk mapping functions
			pDesc->mVulkan.mVkType = binding.descriptorType;
			pDesc->mVulkan.mVkStages = binding.stageFlags;
			pDesc->mUpdateFrequency = updateFreq;

			if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
			{
				layouts[setIndex].mDynamicDescriptors.emplace_back(pDesc);
			}

			// Find if the given descriptor is a static sampler
			decltype(staticSamplerMap)::iterator it = staticSamplerMap.find(pDesc->pName);
			bool                                 hasStaticSampler = it != staticSamplerMap.end();
			if (hasStaticSampler)
			{
				LOGF(LogLevel::eINFO, "Descriptor (%s) : User specified Static Sampler", pDesc->pName);
				binding.pImmutableSamplers = &it->second->mVulkan.pVkSampler;
			}

			// Set the index to an invalid value so we can use this later for error checking if user tries to update a static sampler
			// In case of Combined Image Samplers, skip invalidating the index
			// because we do not to introduce new ways to update the descriptor in the Interface
			if (hasStaticSampler && pDesc->mType != DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
			{
				pDesc->mIndexInParent = -1;
			}
			else
			{
				layouts[setIndex].mDescriptors.emplace_back(pDesc);
			}

			layouts[setIndex].mBindings.push_back(binding);
		}
		// If descriptor is a root constant, add it to the root constant array
		else
		{
			LOGF(LogLevel::eINFO, "Descriptor (%s) : User specified Push Constant", pDesc->pName);

			pDesc->mVulkan.mVkStages = util_to_vk_shader_stage_flags(pRes->used_stages);
			setIndex = 0;
			pDesc->mIndexInParent = pushConstantCount++;

			pushConstants[pDesc->mIndexInParent] = {};
			pushConstants[pDesc->mIndexInParent].offset = 0;
			pushConstants[pDesc->mIndexInParent].size = pDesc->mSize;
			pushConstants[pDesc->mIndexInParent].stageFlags = pDesc->mVulkan.mVkStages;
		}

		layouts[setIndex].mDescriptorIndexMap[pDesc] = i;
	}

	pRootSignature->mVulkan.mVkPushConstantCount = pushConstantCount;

	// Create descriptor layouts
	// Put most frequently changed params first
	for (uint32_t i = kMaxLayoutCount; i-- > 0U;)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[i];

		if (layouts[i].mBindings.size())
		{
			// sort table by type (CBV/SRV/UAV) by register
			eastl::stable_sort(
				layout.mBindings.begin(), layout.mBindings.end(),
				[](const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs) { return lhs.binding > rhs.binding; });
			eastl::stable_sort(
				layout.mBindings.begin(), layout.mBindings.end(),
				[](const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs) {
					return lhs.descriptorType > rhs.descriptorType;
				});
		}

		bool createLayout = layout.mBindings.size() > 0;
		// Check if we need to create an empty layout in case there is an empty set between two used sets
		// Example: set = 0 is used, set = 2 is used. In this case, set = 1 needs to exist even if it is empty
		if (!createLayout && i < kMaxLayoutCount - 1)
		{
			createLayout = pRootSignature->mVulkan.mVkDescriptorSetLayouts[i + 1] != VK_NULL_HANDLE;
		}

		if (createLayout)
		{
			VkDescriptorSetLayoutCreateInfo layoutInfo = {};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.pNext = NULL;
			layoutInfo.bindingCount = (uint32_t)layout.mBindings.size();
			layoutInfo.pBindings = layout.mBindings.data();
			layoutInfo.flags = 0;

			CHECK_VKRESULT(vkCreateDescriptorSetLayout(
				pRenderer->mVulkan.pVkDevice, &layoutInfo, &gVkAllocationCallbacks, &pRootSignature->mVulkan.mVkDescriptorSetLayouts[i]));
		}

		if (!layouts[i].mBindings.size())
		{
			continue;
		}

		pRootSignature->mVulkan.mVkDescriptorCounts[i] = (uint32_t)layout.mDescriptors.size();

		// Loop through descriptors belonging to this update frequency and increment the cumulative descriptor count
		for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mDescriptors.size(); ++descIndex)
		{
			DescriptorInfo* pDesc = layout.mDescriptors[descIndex];
			pDesc->mIndexInParent = descIndex;
			pDesc->mHandleIndex = pRootSignature->mVulkan.mVkCumulativeDescriptorCounts[i];
			pRootSignature->mVulkan.mVkCumulativeDescriptorCounts[i] += pDesc->mSize;
		}

		pRootSignature->mVulkan.mVkDynamicDescriptorCounts[i] = (uint32_t)layout.mDynamicDescriptors.size();
		for (uint32_t descIndex = 0; descIndex < pRootSignature->mVulkan.mVkDynamicDescriptorCounts[i]; ++descIndex)
		{
			DescriptorInfo* pDesc = layout.mDynamicDescriptors[descIndex];
			pDesc->mVulkan.mRootDescriptorIndex = descIndex;
		}
	}
	/************************************************************************/
	// Pipeline layout
	/************************************************************************/
	VkDescriptorSetLayout descriptorSetLayouts[kMaxLayoutCount] = {};
	uint32_t              descriptorSetLayoutCount = 0;
	for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
	{
		if (pRootSignature->mVulkan.mVkDescriptorSetLayouts[i])
		{
			descriptorSetLayouts[descriptorSetLayoutCount++] = pRootSignature->mVulkan.mVkDescriptorSetLayouts[i];
		}
	}

	DECLARE_ZERO(VkPipelineLayoutCreateInfo, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.setLayoutCount = descriptorSetLayoutCount;
	add_info.pSetLayouts = descriptorSetLayouts;
	add_info.pushConstantRangeCount = pRootSignature->mVulkan.mVkPushConstantCount;
	add_info.pPushConstantRanges = pushConstants;
	CHECK_VKRESULT(vkCreatePipelineLayout(
		pRenderer->mVulkan.pVkDevice, &add_info, &gVkAllocationCallbacks, &(pRootSignature->mVulkan.pPipelineLayout)));
	/************************************************************************/
	// Update templates
	/************************************************************************/
	for (uint32_t setIndex = 0; setIndex < DESCRIPTOR_UPDATE_FREQ_COUNT; ++setIndex)
	{
		if (pRootSignature->mVulkan.mVkDescriptorCounts[setIndex])
		{
			pRootSignature->mVulkan.pUpdateTemplateData[setIndex] =
				(void**)tf_calloc(pRenderer->mLinkedNodeCount, sizeof(DescriptorUpdateData*));

			const UpdateFrequencyLayoutInfo& layout = layouts[setIndex];
			VkDescriptorUpdateTemplateEntry* pEntries = (VkDescriptorUpdateTemplateEntry*)tf_malloc(
				pRootSignature->mVulkan.mVkDescriptorCounts[setIndex] * sizeof(VkDescriptorUpdateTemplateEntry));
			uint32_t entryCount = 0;

			for (uint32_t nodeIndex = 0; nodeIndex < pRenderer->mLinkedNodeCount; ++nodeIndex)
			{
				pRootSignature->mVulkan.pUpdateTemplateData[setIndex][nodeIndex] =
					tf_calloc(pRootSignature->mVulkan.mVkCumulativeDescriptorCounts[setIndex], sizeof(DescriptorUpdateData));
			}

			// Fill the write descriptors with default values during initialize so the only thing we change in cmdBindDescriptors is the the VkBuffer / VkImageView objects
			for (uint32_t i = 0; i < (uint32_t)layout.mDescriptors.size(); ++i)
			{
				const DescriptorInfo* pDesc = layout.mDescriptors[i];
				const uint64_t        offset = pDesc->mHandleIndex * sizeof(DescriptorUpdateData);

#ifdef VK_NV_RAY_TRACING_SPEC_VERSION
				// Raytracing descriptors dont support update template so we ignore them
				if (pDesc->mType == DESCRIPTOR_TYPE_RAY_TRACING)
				{
					pRootSignature->mVulkan.mVkRaytracingDescriptorCounts[setIndex] += pDesc->mSize;
					continue;
				}
#endif

				pEntries[entryCount].descriptorCount = pDesc->mSize;
				pEntries[entryCount].descriptorType = (VkDescriptorType)pDesc->mVulkan.mVkType;
				pEntries[entryCount].dstArrayElement = 0;
				pEntries[entryCount].dstBinding = pDesc->mVulkan.mReg;
				pEntries[entryCount].offset = offset;
				pEntries[entryCount].stride = sizeof(DescriptorUpdateData);

				for (uint32_t nodeIndex = 0; nodeIndex < pRenderer->mLinkedNodeCount; ++nodeIndex)
				{
					DescriptorUpdateData* pUpdateData =
						(DescriptorUpdateData*)pRootSignature->mVulkan.pUpdateTemplateData[setIndex][nodeIndex];

					const DescriptorType type = (DescriptorType)pDesc->mType;
					const uint32_t       arrayCount = pDesc->mSize;

					switch (type)
					{
						case DESCRIPTOR_TYPE_SAMPLER:
						{
							for (uint32_t arr = 0; arr < arrayCount; ++arr)
								pUpdateData[pDesc->mHandleIndex + arr].mImageInfo = {
									pRenderer->pNullDescriptors->pDefaultSampler->mVulkan.pVkSampler, VK_NULL_HANDLE
								};
							break;
						}
						case DESCRIPTOR_TYPE_TEXTURE:
						{
							for (uint32_t arr = 0; arr < arrayCount; ++arr)
								pUpdateData[pDesc->mHandleIndex + arr].mImageInfo = {
									VK_NULL_HANDLE,
									pRenderer->pNullDescriptors->pDefaultTextureSRV[nodeIndex][pDesc->mDim]->mVulkan.pVkSRVDescriptor,
									VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
								};
							break;
						}
						case DESCRIPTOR_TYPE_RW_TEXTURE:
						{
							for (uint32_t arr = 0; arr < arrayCount; ++arr)
								pUpdateData[pDesc->mHandleIndex + arr].mImageInfo = {
									VK_NULL_HANDLE,
									pRenderer->pNullDescriptors->pDefaultTextureUAV[nodeIndex][pDesc->mDim]->mVulkan.pVkUAVDescriptors[0],
									VK_IMAGE_LAYOUT_GENERAL
								};
							break;
						}
						case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
						case DESCRIPTOR_TYPE_BUFFER:
						case DESCRIPTOR_TYPE_BUFFER_RAW:
						{
							for (uint32_t arr = 0; arr < arrayCount; ++arr)
								pUpdateData[pDesc->mHandleIndex + arr].mBufferInfo = {
									pRenderer->pNullDescriptors->pDefaultBufferSRV[nodeIndex]->mVulkan.pVkBuffer,
									pRenderer->pNullDescriptors->pDefaultBufferSRV[nodeIndex]->mVulkan.mOffset, VK_WHOLE_SIZE
								};
							break;
						}
						case DESCRIPTOR_TYPE_RW_BUFFER:
						case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
						{
							for (uint32_t arr = 0; arr < arrayCount; ++arr)
								pUpdateData[pDesc->mHandleIndex + arr].mBufferInfo = {
									pRenderer->pNullDescriptors->pDefaultBufferUAV[nodeIndex]->mVulkan.pVkBuffer,
									pRenderer->pNullDescriptors->pDefaultBufferUAV[nodeIndex]->mVulkan.mOffset, VK_WHOLE_SIZE
								};
							break;
						}
						case DESCRIPTOR_TYPE_TEXEL_BUFFER:
						{
							for (uint32_t arr = 0; arr < arrayCount; ++arr)
								pUpdateData[pDesc->mHandleIndex + arr].mBuferView =
									pRenderer->pNullDescriptors->pDefaultBufferSRV[nodeIndex]->mVulkan.pVkUniformTexelView;
							break;
						}
						case DESCRIPTOR_TYPE_RW_TEXEL_BUFFER:
						{
							for (uint32_t arr = 0; arr < arrayCount; ++arr)
								pUpdateData[pDesc->mHandleIndex + arr].mBuferView =
									pRenderer->pNullDescriptors->pDefaultBufferUAV[nodeIndex]->mVulkan.pVkStorageTexelView;
							break;
						}
						default: break;
					}
				}

				++entryCount;
			}

			VkDescriptorUpdateTemplateCreateInfoKHR createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR;
			createInfo.pNext = NULL;
			createInfo.descriptorSetLayout = pRootSignature->mVulkan.mVkDescriptorSetLayouts[setIndex];
			createInfo.descriptorUpdateEntryCount = entryCount;
			createInfo.pDescriptorUpdateEntries = pEntries;
			createInfo.pipelineBindPoint = gPipelineBindPoint[pRootSignature->mPipelineType];
			createInfo.pipelineLayout = pRootSignature->mVulkan.pPipelineLayout;
			createInfo.set = setIndex;
			createInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR;
			CHECK_VKRESULT(vkCreateDescriptorUpdateTemplateKHR(
				pRenderer->mVulkan.pVkDevice, &createInfo, &gVkAllocationCallbacks, &pRootSignature->mVulkan.mUpdateTemplates[setIndex]));

			tf_free(pEntries);
		}
		else if (VK_NULL_HANDLE != pRootSignature->mVulkan.mVkDescriptorSetLayouts[setIndex])
		{
			// Consume empty descriptor sets from empty descriptor set pool
			VkDescriptorSet* pSets[] = { &pRootSignature->mVulkan.mVkEmptyDescriptorSets[setIndex] };
			consume_descriptor_sets(
				pRenderer->mVulkan.pDescriptorPool, &pRootSignature->mVulkan.mVkDescriptorSetLayouts[setIndex], pSets, 1);
		}
	}

	*ppRootSignature = pRootSignature;
}

void vk_removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
	for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
	{
		vkDestroyDescriptorSetLayout(
			pRenderer->mVulkan.pVkDevice, pRootSignature->mVulkan.mVkDescriptorSetLayouts[i], &gVkAllocationCallbacks);
		if (VK_NULL_HANDLE != pRootSignature->mVulkan.mUpdateTemplates[i])
			vkDestroyDescriptorUpdateTemplateKHR(
				pRenderer->mVulkan.pVkDevice, pRootSignature->mVulkan.mUpdateTemplates[i], &gVkAllocationCallbacks);

		if (pRootSignature->mVulkan.mVkDescriptorCounts[i])
			for (uint32_t nodeIndex = 0; nodeIndex < pRenderer->mLinkedNodeCount; ++nodeIndex)
				SAFE_FREE(pRootSignature->mVulkan.pUpdateTemplateData[i][nodeIndex]);

		SAFE_FREE(pRootSignature->mVulkan.pUpdateTemplateData[i]);
	}

	// Need delete since the destructor frees allocated memory
	pRootSignature->pDescriptorNameToIndexMap->mMap.clear(true);

	vkDestroyPipelineLayout(pRenderer->mVulkan.pVkDevice, pRootSignature->mVulkan.pPipelineLayout, &gVkAllocationCallbacks);

	SAFE_FREE(pRootSignature);
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
	VkPipelineCache             psoCache = pMainDesc->pCache ? pMainDesc->pCache->mVulkan.pCache : VK_NULL_HANDLE;

	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);

	Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
	ASSERT(pPipeline);

	const Shader*       pShaderProgram = pDesc->pShaderProgram;
	const VertexLayout* pVertexLayout = pDesc->pVertexLayout;

	pPipeline->mVulkan.mType = PIPELINE_TYPE_GRAPHICS;

	// Create tempporary renderpass for pipeline creation
	RenderPassDesc renderPassDesc = { 0 };
	RenderPass*    pRenderPass = NULL;
	renderPassDesc.mRenderTargetCount = pDesc->mRenderTargetCount;
	renderPassDesc.pColorFormats = pDesc->pColorFormats;
	renderPassDesc.mSampleCount = pDesc->mSampleCount;
	renderPassDesc.mDepthStencilFormat = pDesc->mDepthStencilFormat;
    renderPassDesc.mVRMultiview = pDesc->pShaderProgram->mIsMultiviewVR;
    renderPassDesc.mVRFoveatedRendering = pDesc->mVRFoveatedRendering;
	add_render_pass(pRenderer, &renderPassDesc, &pRenderPass);

	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);
	for (uint32_t i = 0; i < pShaderProgram->pReflection->mStageReflectionCount; ++i)
		ASSERT(VK_NULL_HANDLE != pShaderProgram->mVulkan.pShaderModules[i]);

	// Pipeline
	{
		uint32_t stage_count = 0;
		DECLARE_ZERO(VkPipelineShaderStageCreateInfo, stages[5]);
		for (uint32_t i = 0; i < 5; ++i)
		{
			ShaderStage stage_mask = (ShaderStage)(1 << i);
			if (stage_mask == (pShaderProgram->mStages & stage_mask))
			{
				stages[stage_count].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				stages[stage_count].pNext = NULL;
				stages[stage_count].flags = 0;
				stages[stage_count].pSpecializationInfo = NULL;
				switch (stage_mask)
				{
					case SHADER_STAGE_VERT:
					{
						stages[stage_count].pName =
							pShaderProgram->pReflection->mStageReflections[pShaderProgram->pReflection->mVertexStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_VERTEX_BIT;
						stages[stage_count].module = pShaderProgram->mVulkan.pShaderModules[pShaderProgram->pReflection->mVertexStageIndex];
					}
					break;
					case SHADER_STAGE_TESC:
					{
						stages[stage_count].pName =
							pShaderProgram->pReflection->mStageReflections[pShaderProgram->pReflection->mHullStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
						stages[stage_count].module = pShaderProgram->mVulkan.pShaderModules[pShaderProgram->pReflection->mHullStageIndex];
					}
					break;
					case SHADER_STAGE_TESE:
					{
						stages[stage_count].pName =
							pShaderProgram->pReflection->mStageReflections[pShaderProgram->pReflection->mDomainStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
						stages[stage_count].module = pShaderProgram->mVulkan.pShaderModules[pShaderProgram->pReflection->mDomainStageIndex];
					}
					break;
					case SHADER_STAGE_GEOM:
					{
						stages[stage_count].pName =
							pShaderProgram->pReflection->mStageReflections[pShaderProgram->pReflection->mGeometryStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
						stages[stage_count].module =
							pShaderProgram->mVulkan.pShaderModules[pShaderProgram->pReflection->mGeometryStageIndex];
					}
					break;
					case SHADER_STAGE_FRAG:
					{
						stages[stage_count].pName =
							pShaderProgram->pReflection->mStageReflections[pShaderProgram->pReflection->mPixelStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
						stages[stage_count].module = pShaderProgram->mVulkan.pShaderModules[pShaderProgram->pReflection->mPixelStageIndex];
					}
					break;
					default: ASSERT(false && "Shader Stage not supported!"); break;
				}
				++stage_count;
			}
		}

		// Make sure there's a shader
		ASSERT(0 != stage_count);

		uint32_t                          input_binding_count = 0;
		VkVertexInputBindingDescription   input_bindings[MAX_VERTEX_BINDINGS] = { { 0 } };
		uint32_t                          input_attribute_count = 0;
		VkVertexInputAttributeDescription input_attributes[MAX_VERTEX_ATTRIBS] = { { 0 } };

		// Make sure there's attributes
		if (pVertexLayout != NULL)
		{
			// Ignore everything that's beyond max_vertex_attribs
			uint32_t attrib_count = pVertexLayout->mAttribCount > MAX_VERTEX_ATTRIBS ? MAX_VERTEX_ATTRIBS : pVertexLayout->mAttribCount;
			uint32_t binding_value = UINT32_MAX;

			// Initial values
			for (uint32_t i = 0; i < attrib_count; ++i)
			{
				const VertexAttrib* attrib = &(pVertexLayout->mAttribs[i]);

				if (binding_value != attrib->mBinding)
				{
					binding_value = attrib->mBinding;
					++input_binding_count;
				}

				input_bindings[input_binding_count - 1].binding = binding_value;
				if (attrib->mRate == VERTEX_ATTRIB_RATE_INSTANCE)
				{
					input_bindings[input_binding_count - 1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
				}
				else
				{
					input_bindings[input_binding_count - 1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
				}
				input_bindings[input_binding_count - 1].stride += TinyImageFormat_BitSizeOfBlock(attrib->mFormat) / 8;

				input_attributes[input_attribute_count].location = attrib->mLocation;
				input_attributes[input_attribute_count].binding = attrib->mBinding;
				input_attributes[input_attribute_count].format = (VkFormat)TinyImageFormat_ToVkFormat(attrib->mFormat);
				input_attributes[input_attribute_count].offset = attrib->mOffset;
				++input_attribute_count;
			}
		}

		DECLARE_ZERO(VkPipelineVertexInputStateCreateInfo, vi);
		vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vi.pNext = NULL;
		vi.flags = 0;
		vi.vertexBindingDescriptionCount = input_binding_count;
		vi.pVertexBindingDescriptions = input_bindings;
		vi.vertexAttributeDescriptionCount = input_attribute_count;
		vi.pVertexAttributeDescriptions = input_attributes;

		VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		switch (pDesc->mPrimitiveTopo)
		{
			case PRIMITIVE_TOPO_POINT_LIST: topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
			case PRIMITIVE_TOPO_LINE_LIST: topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
			case PRIMITIVE_TOPO_LINE_STRIP: topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
			case PRIMITIVE_TOPO_TRI_STRIP: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
			case PRIMITIVE_TOPO_PATCH_LIST: topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; break;
			case PRIMITIVE_TOPO_TRI_LIST: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
			default: ASSERT(false && "Primitive Topo not supported!"); break;
		}
		DECLARE_ZERO(VkPipelineInputAssemblyStateCreateInfo, ia);
		ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		ia.pNext = NULL;
		ia.flags = 0;
		ia.topology = topology;
		ia.primitiveRestartEnable = VK_FALSE;

		DECLARE_ZERO(VkPipelineTessellationStateCreateInfo, ts);
		if ((pShaderProgram->mStages & SHADER_STAGE_TESC) && (pShaderProgram->mStages & SHADER_STAGE_TESE))
		{
			ts.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
			ts.pNext = NULL;
			ts.flags = 0;
			ts.patchControlPoints =
				pShaderProgram->pReflection->mStageReflections[pShaderProgram->pReflection->mHullStageIndex].mNumControlPoint;
		}

		DECLARE_ZERO(VkPipelineViewportStateCreateInfo, vs);
		vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		vs.pNext = NULL;
		vs.flags = 0;
		// we are using dynamic viewports but we must set the count to 1
		vs.viewportCount = 1;
		vs.pViewports = NULL;
		vs.scissorCount = 1;
		vs.pScissors = NULL;

		DECLARE_ZERO(VkPipelineMultisampleStateCreateInfo, ms);
		ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		ms.pNext = NULL;
		ms.flags = 0;
		ms.rasterizationSamples = util_to_vk_sample_count(pDesc->mSampleCount);
		ms.sampleShadingEnable = VK_FALSE;
		ms.minSampleShading = 0.0f;
		ms.pSampleMask = 0;
		ms.alphaToCoverageEnable = VK_FALSE;
		ms.alphaToOneEnable = VK_FALSE;

		DECLARE_ZERO(VkPipelineRasterizationStateCreateInfo, rs);
		rs = pDesc->pRasterizerState ? util_to_rasterizer_desc(pDesc->pRasterizerState) : gDefaultRasterizerDesc;

		/// TODO: Dont create depth state if no depth stencil bound
		DECLARE_ZERO(VkPipelineDepthStencilStateCreateInfo, ds);
		ds = pDesc->pDepthState ? util_to_depth_desc(pDesc->pDepthState) : gDefaultDepthDesc;

		DECLARE_ZERO(VkPipelineColorBlendStateCreateInfo, cb);
		DECLARE_ZERO(VkPipelineColorBlendAttachmentState, cbAtt[MAX_RENDER_TARGET_ATTACHMENTS]);
		cb = pDesc->pBlendState ? util_to_blend_desc(pDesc->pBlendState, cbAtt) : gDefaultBlendDesc;
		cb.attachmentCount = pDesc->mRenderTargetCount;

		VkDynamicState dyn_states[] =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_BLEND_CONSTANTS,
			VK_DYNAMIC_STATE_DEPTH_BOUNDS,
			VK_DYNAMIC_STATE_STENCIL_REFERENCE,
		};
		DECLARE_ZERO(VkPipelineDynamicStateCreateInfo, dy);
		dy.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dy.pNext = NULL;
		dy.flags = 0;
		dy.dynamicStateCount = sizeof(dyn_states) / sizeof(dyn_states[0]);
		dy.pDynamicStates = dyn_states;

		DECLARE_ZERO(VkGraphicsPipelineCreateInfo, add_info);
		add_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		add_info.pNext = NULL;
		add_info.flags = 0;
		add_info.stageCount = stage_count;
		add_info.pStages = stages;
		add_info.pVertexInputState = &vi;
		add_info.pInputAssemblyState = &ia;

		if ((pShaderProgram->mStages & SHADER_STAGE_TESC) && (pShaderProgram->mStages & SHADER_STAGE_TESE))
			add_info.pTessellationState = &ts;
		else
			add_info.pTessellationState = NULL;    // set tessellation state to null if we have no tessellation

		add_info.pViewportState = &vs;
		add_info.pRasterizationState = &rs;
		add_info.pMultisampleState = &ms;
		add_info.pDepthStencilState = &ds;
		add_info.pColorBlendState = &cb;
		add_info.pDynamicState = &dy;
		add_info.layout = pDesc->pRootSignature->mVulkan.pPipelineLayout;
		add_info.renderPass = pRenderPass->pRenderPass;
		add_info.subpass = 0;
		add_info.basePipelineHandle = VK_NULL_HANDLE;
		add_info.basePipelineIndex = -1;
		CHECK_VKRESULT(vkCreateGraphicsPipelines(
			pRenderer->mVulkan.pVkDevice, psoCache, 1, &add_info, &gVkAllocationCallbacks, &(pPipeline->mVulkan.pVkPipeline)));

		remove_render_pass(pRenderer, pRenderPass);
	}

	*ppPipeline = pPipeline;
}

static void addComputePipeline(Renderer* pRenderer, const PipelineDesc* pMainDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(ppPipeline);
	ASSERT(pMainDesc);

	const ComputePipelineDesc* pDesc = &pMainDesc->mComputeDesc;
	VkPipelineCache            psoCache = pMainDesc->pCache ? pMainDesc->pCache->mVulkan.pCache : VK_NULL_HANDLE;

	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);
	ASSERT(pRenderer->mVulkan.pVkDevice != VK_NULL_HANDLE);
	ASSERT(pDesc->pShaderProgram->mVulkan.pShaderModules[0] != VK_NULL_HANDLE);

	Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
	ASSERT(pPipeline);
	pPipeline->mVulkan.mType = PIPELINE_TYPE_COMPUTE;

	// Pipeline
	{
		DECLARE_ZERO(VkPipelineShaderStageCreateInfo, stage);
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage.pNext = NULL;
		stage.flags = 0;
		stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		stage.module = pDesc->pShaderProgram->mVulkan.pShaderModules[0];
		stage.pName = pDesc->pShaderProgram->pReflection->mStageReflections[0].pEntryPoint;
		stage.pSpecializationInfo = NULL;

		DECLARE_ZERO(VkComputePipelineCreateInfo, create_info);
		create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		create_info.pNext = NULL;
		create_info.flags = 0;
		create_info.stage = stage;
		create_info.layout = pDesc->pRootSignature->mVulkan.pPipelineLayout;
		create_info.basePipelineHandle = 0;
		create_info.basePipelineIndex = 0;
		CHECK_VKRESULT(vkCreateComputePipelines(
			pRenderer->mVulkan.pVkDevice, psoCache, 1, &create_info, &gVkAllocationCallbacks, &(pPipeline->mVulkan.pVkPipeline)));
	}

	*ppPipeline = pPipeline;
}

void vk_addPipeline(Renderer* pRenderer, const PipelineDesc* pDesc, Pipeline** ppPipeline)
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
			vk_addRaytracingPipeline(pDesc, ppPipeline);
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

	if (*ppPipeline && pDesc->pName)
	{
		setPipelineName(pRenderer, *ppPipeline, pDesc->pName);
	}
}

void vk_removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pPipeline);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);
	ASSERT(VK_NULL_HANDLE != pPipeline->mVulkan.pVkPipeline);

#ifdef ENABLE_RAYTRACING
	SAFE_FREE(pPipeline->mVulkan.ppShaderStageNames);
#endif

	vkDestroyPipeline(pRenderer->mVulkan.pVkDevice, pPipeline->mVulkan.pVkPipeline, &gVkAllocationCallbacks);

	SAFE_FREE(pPipeline);
}

void vk_addPipelineCache(Renderer* pRenderer, const PipelineCacheDesc* pDesc, PipelineCache** ppPipelineCache)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppPipelineCache);

	PipelineCache* pPipelineCache = (PipelineCache*)tf_calloc(1, sizeof(PipelineCache));
	ASSERT(pPipelineCache);

	VkPipelineCacheCreateInfo psoCacheCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
	psoCacheCreateInfo.initialDataSize = pDesc->mSize;
	psoCacheCreateInfo.pInitialData = pDesc->pData;
	psoCacheCreateInfo.flags = util_to_pipeline_cache_flags(pDesc->mFlags);
	CHECK_VKRESULT(
		vkCreatePipelineCache(pRenderer->mVulkan.pVkDevice, &psoCacheCreateInfo, &gVkAllocationCallbacks, &pPipelineCache->mVulkan.pCache));

	*ppPipelineCache = pPipelineCache;
}

void vk_removePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache)
{
	ASSERT(pRenderer);
	ASSERT(pPipelineCache);

	if (pPipelineCache->mVulkan.pCache)
	{
		vkDestroyPipelineCache(pRenderer->mVulkan.pVkDevice, pPipelineCache->mVulkan.pCache, &gVkAllocationCallbacks);
	}

	SAFE_FREE(pPipelineCache);
}

void vk_getPipelineCacheData(Renderer* pRenderer, PipelineCache* pPipelineCache, size_t* pSize, void* pData)
{
	ASSERT(pRenderer);
	ASSERT(pPipelineCache);
	ASSERT(pSize);

	if (pPipelineCache->mVulkan.pCache)
	{
		CHECK_VKRESULT(vkGetPipelineCacheData(pRenderer->mVulkan.pVkDevice, pPipelineCache->mVulkan.pCache, pSize, pData));
	}
}
/************************************************************************/
// Command buffer functions
/************************************************************************/
void vk_resetCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	ASSERT(pRenderer);
	ASSERT(pCmdPool);

	VkResult vk_res = vkResetCommandPool(pRenderer->mVulkan.pVkDevice, pCmdPool->pVkCmdPool, 0);
	ASSERT(VK_SUCCESS == vk_res);
}

void vk_beginCmd(Cmd* pCmd)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->mVulkan.pVkCmdBuf);

	DECLARE_ZERO(VkCommandBufferBeginInfo, begin_info);
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VkDeviceGroupCommandBufferBeginInfoKHR deviceGroupBeginInfo = { VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO_KHR };
	deviceGroupBeginInfo.pNext = NULL;
	if (pCmd->pRenderer->mGpuMode == GPU_MODE_LINKED)
	{
		deviceGroupBeginInfo.deviceMask = (1 << pCmd->mVulkan.mNodeIndex);
		begin_info.pNext = &deviceGroupBeginInfo;
	}

	VkResult vk_res = vkBeginCommandBuffer(pCmd->mVulkan.pVkCmdBuf, &begin_info);
	ASSERT(VK_SUCCESS == vk_res);

	// Reset CPU side data
	pCmd->mVulkan.pBoundPipelineLayout = NULL;
}

void vk_endCmd(Cmd* pCmd)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->mVulkan.pVkCmdBuf);

	if (pCmd->mVulkan.pVkActiveRenderPass)
	{
		vkCmdEndRenderPass(pCmd->mVulkan.pVkCmdBuf);
	}

	pCmd->mVulkan.pVkActiveRenderPass = VK_NULL_HANDLE;

	VkResult vk_res = vkEndCommandBuffer(pCmd->mVulkan.pVkCmdBuf);
	ASSERT(VK_SUCCESS == vk_res);
}

void vk_cmdBindRenderTargets(
	Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil,
	const LoadActionsDesc* pLoadActions /* = NULL*/, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice,
	uint32_t depthMipSlice)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->mVulkan.pVkCmdBuf);

	if (pCmd->mVulkan.pVkActiveRenderPass)
	{
		vkCmdEndRenderPass(pCmd->mVulkan.pVkCmdBuf);
		pCmd->mVulkan.pVkActiveRenderPass = VK_NULL_HANDLE;
	}

	if (!renderTargetCount && !pDepthStencil)
		return;

	size_t renderPassHash = 0;
	size_t frameBufferHash = 0;
    bool vrFoveatedRendering = false;

	// Generate hash for render pass and frame buffer
	// NOTE:
	// Render pass does not care about underlying VkImageView. It only cares about the format and sample count of the attachments.
	// We hash those two values to generate render pass hash
	// Frame buffer is the actual array of all the VkImageViews
	// We hash the texture id associated with the render target to generate frame buffer hash
	for (uint32_t i = 0; i < renderTargetCount; ++i)
	{
		uint32_t hashValues[] = {
			(uint32_t)ppRenderTargets[i]->mFormat,
			(uint32_t)ppRenderTargets[i]->mSampleCount,
			pLoadActions ? (uint32_t)pLoadActions->mLoadActionsColor[i] : 0,
		};
		renderPassHash = tf_mem_hash<uint32_t>(hashValues, 3, renderPassHash);
		frameBufferHash = tf_mem_hash<uint32_t>(&ppRenderTargets[i]->mVulkan.mId, 1, frameBufferHash);
        vrFoveatedRendering |= ppRenderTargets[i]->mVRFoveatedRendering;
	}
	if (pDepthStencil)
	{
		uint32_t hashValues[] = {
			(uint32_t)pDepthStencil->mFormat,
			(uint32_t)pDepthStencil->mSampleCount,
			pLoadActions ? (uint32_t)pLoadActions->mLoadActionDepth : 0,
			pLoadActions ? (uint32_t)pLoadActions->mLoadActionStencil : 0,
		};
		renderPassHash = tf_mem_hash<uint32_t>(hashValues, 4, renderPassHash);
		frameBufferHash = tf_mem_hash<uint32_t>(&pDepthStencil->mVulkan.mId, 1, frameBufferHash);
        vrFoveatedRendering |= pDepthStencil->mVRFoveatedRendering;
	}
	if (pColorArraySlices)
		frameBufferHash = tf_mem_hash<uint32_t>(pColorArraySlices, renderTargetCount, frameBufferHash);
	if (pColorMipSlices)
		frameBufferHash = tf_mem_hash<uint32_t>(pColorMipSlices, renderTargetCount, frameBufferHash);
	if (depthArraySlice != -1)
		frameBufferHash = tf_mem_hash<uint32_t>(&depthArraySlice, 1, frameBufferHash);
	if (depthMipSlice != -1)
		frameBufferHash = tf_mem_hash<uint32_t>(&depthMipSlice, 1, frameBufferHash);

	SampleCount sampleCount = SAMPLE_COUNT_1;

	RenderPassMap&  renderPassMap = get_render_pass_map();
	FrameBufferMap& frameBufferMap = get_frame_buffer_map();

	const RenderPassMapIt  pNode = renderPassMap.find(renderPassHash);
	const FrameBufferMapIt pFrameBufferNode = frameBufferMap.find(frameBufferHash);

	RenderPass*  pRenderPass = NULL;
	FrameBuffer* pFrameBuffer = NULL;

	// If a render pass of this combination already exists just use it or create a new one
	if (pNode != renderPassMap.end())
	{
		pRenderPass = pNode->second;
	}
	else
	{
		TinyImageFormat colorFormats[MAX_RENDER_TARGET_ATTACHMENTS] = {};
		TinyImageFormat depthStencilFormat = TinyImageFormat_UNDEFINED;
        bool vrMultiview = false;
		for (uint32_t i = 0; i < renderTargetCount; ++i)
		{
			colorFormats[i] = ppRenderTargets[i]->mFormat;
            vrMultiview |= ppRenderTargets[i]->mVRMultiview;
		}
		if (pDepthStencil)
		{
			depthStencilFormat = pDepthStencil->mFormat;
			sampleCount = pDepthStencil->mSampleCount;
            vrMultiview |= pDepthStencil->mVRMultiview;
		}
		else if (renderTargetCount)
		{
			sampleCount = ppRenderTargets[0]->mSampleCount;
		}

		RenderPassDesc renderPassDesc = {};
		renderPassDesc.mRenderTargetCount = renderTargetCount;
		renderPassDesc.mSampleCount = sampleCount;
		renderPassDesc.pColorFormats = colorFormats;
		renderPassDesc.mDepthStencilFormat = depthStencilFormat;
		renderPassDesc.pLoadActionsColor = pLoadActions ? pLoadActions->mLoadActionsColor : NULL;
		renderPassDesc.mLoadActionDepth = pLoadActions ? pLoadActions->mLoadActionDepth : LOAD_ACTION_DONTCARE;
		renderPassDesc.mLoadActionStencil = pLoadActions ? pLoadActions->mLoadActionStencil : LOAD_ACTION_DONTCARE;
        renderPassDesc.mVRMultiview = vrMultiview;
        renderPassDesc.mVRFoveatedRendering = vrFoveatedRendering;
		add_render_pass(pCmd->pRenderer, &renderPassDesc, &pRenderPass);

		// No need of a lock here since this map is per thread
		renderPassMap.insert({ { renderPassHash, pRenderPass } });
	}

	// If a frame buffer of this combination already exists just use it or create a new one
	if (pFrameBufferNode != frameBufferMap.end())
	{
		pFrameBuffer = pFrameBufferNode->second;
	}
	else
	{
		FrameBufferDesc desc = { 0 };
		desc.mRenderTargetCount = renderTargetCount;
		desc.pDepthStencil = pDepthStencil;
		desc.ppRenderTargets = ppRenderTargets;
		desc.pRenderPass = pRenderPass;
		desc.pColorArraySlices = pColorArraySlices;
		desc.pColorMipSlices = pColorMipSlices;
		desc.mDepthArraySlice = depthArraySlice;
		desc.mDepthMipSlice = depthMipSlice;
        desc.mVRFoveatedRendering = vrFoveatedRendering;
		add_framebuffer(pCmd->pRenderer, &desc, &pFrameBuffer);

		// No need of a lock here since this map is per thread
		frameBufferMap.insert({ { frameBufferHash, pFrameBuffer } });
	}

	DECLARE_ZERO(VkRect2D, render_area);
	render_area.offset.x = 0;
	render_area.offset.y = 0;
	render_area.extent.width = pFrameBuffer->mWidth;
	render_area.extent.height = pFrameBuffer->mHeight;

	uint32_t     clearValueCount = renderTargetCount;
	VkClearValue clearValues[MAX_RENDER_TARGET_ATTACHMENTS + 1] = {};
	if (pLoadActions)
	{
		for (uint32_t i = 0; i < renderTargetCount; ++i)
		{
			ClearValue clearValue = pLoadActions->mClearColorValues[i];
			clearValues[i].color = { { clearValue.r, clearValue.g, clearValue.b, clearValue.a } };
		}
		if (pDepthStencil)
		{
			clearValues[renderTargetCount].depthStencil = { pLoadActions->mClearDepth.depth, pLoadActions->mClearDepth.stencil };
			++clearValueCount;
		}
	}

	DECLARE_ZERO(VkRenderPassBeginInfo, begin_info);
	begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.renderPass = pRenderPass->pRenderPass;
	begin_info.framebuffer = pFrameBuffer->pFramebuffer;
	begin_info.renderArea = render_area;
	begin_info.clearValueCount = clearValueCount;
	begin_info.pClearValues = clearValues;

	vkCmdBeginRenderPass(pCmd->mVulkan.pVkCmdBuf, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
	pCmd->mVulkan.pVkActiveRenderPass = pRenderPass->pRenderPass;
}

void vk_cmdSetShadingRate(
	Cmd* pCmd, ShadingRate shadingRate, Texture* pTexture, ShadingRateCombiner postRasterizerRate, ShadingRateCombiner finalRate)
{
}
void vk_cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->mVulkan.pVkCmdBuf);

	DECLARE_ZERO(VkViewport, viewport);
	viewport.x = x;
    viewport.y = y + height;
	viewport.width = width;
    viewport.height = -height;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;
	vkCmdSetViewport(pCmd->mVulkan.pVkCmdBuf, 0, 1, &viewport);
}

void vk_cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->mVulkan.pVkCmdBuf);

	DECLARE_ZERO(VkRect2D, rect);
	rect.offset.x = x;
	rect.offset.y = y;
	rect.extent.width = width;
	rect.extent.height = height;
	vkCmdSetScissor(pCmd->mVulkan.pVkCmdBuf, 0, 1, &rect);
}

void vk_cmdSetStencilReferenceValue(Cmd* pCmd, uint32_t val)
{
	ASSERT(pCmd);

	vkCmdSetStencilReference(pCmd->mVulkan.pVkCmdBuf, VK_STENCIL_FRONT_AND_BACK, val);
}

void vk_cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
	ASSERT(pCmd);
	ASSERT(pPipeline);
	ASSERT(pCmd->mVulkan.pVkCmdBuf != VK_NULL_HANDLE);

	VkPipelineBindPoint pipeline_bind_point = gPipelineBindPoint[pPipeline->mVulkan.mType];
	vkCmdBindPipeline(pCmd->mVulkan.pVkCmdBuf, pipeline_bind_point, pPipeline->mVulkan.pVkPipeline);
}

void vk_cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
	ASSERT(pCmd);
	ASSERT(pBuffer);
	ASSERT(VK_NULL_HANDLE != pCmd->mVulkan.pVkCmdBuf);

	VkIndexType vk_index_type = (INDEX_TYPE_UINT16 == indexType) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
	vkCmdBindIndexBuffer(pCmd->mVulkan.pVkCmdBuf, pBuffer->mVulkan.pVkBuffer, offset, vk_index_type);
}

void vk_cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
{
	UNREF_PARAM(pStrides);

	ASSERT(pCmd);
	ASSERT(0 != bufferCount);
	ASSERT(ppBuffers);
	ASSERT(VK_NULL_HANDLE != pCmd->mVulkan.pVkCmdBuf);
	ASSERT(pStrides);

	const uint32_t max_buffers = pCmd->pRenderer->mVulkan.pVkActiveGPUProperties->properties.limits.maxVertexInputBindings;
	uint32_t       capped_buffer_count = bufferCount > max_buffers ? max_buffers : bufferCount;

	// No upper bound for this, so use 64 for now
	ASSERT(capped_buffer_count < 64);

	DECLARE_ZERO(VkBuffer, buffers[64]);
	DECLARE_ZERO(VkDeviceSize, offsets[64]);

	for (uint32_t i = 0; i < capped_buffer_count; ++i)
	{
		buffers[i] = ppBuffers[i]->mVulkan.pVkBuffer;
		offsets[i] = (pOffsets ? pOffsets[i] : 0);
	}

	vkCmdBindVertexBuffers(pCmd->mVulkan.pVkCmdBuf, 0, capped_buffer_count, buffers, offsets);
}

void vk_cmdDraw(Cmd* pCmd, uint32_t vertex_count, uint32_t first_vertex)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->mVulkan.pVkCmdBuf);

	vkCmdDraw(pCmd->mVulkan.pVkCmdBuf, vertex_count, 1, first_vertex, 0);
}

void vk_cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->mVulkan.pVkCmdBuf);

	vkCmdDraw(pCmd->mVulkan.pVkCmdBuf, vertexCount, instanceCount, firstVertex, firstInstance);
}

void vk_cmdDrawIndexed(Cmd* pCmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->mVulkan.pVkCmdBuf);

	vkCmdDrawIndexed(pCmd->mVulkan.pVkCmdBuf, index_count, 1, first_index, first_vertex, 0);
}

void vk_cmdDrawIndexedInstanced(
	Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstInstance, uint32_t firstVertex)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->mVulkan.pVkCmdBuf);

	vkCmdDrawIndexed(pCmd->mVulkan.pVkCmdBuf, indexCount, instanceCount, firstIndex, firstVertex, firstInstance);
}

void vk_cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	ASSERT(pCmd);
	ASSERT(pCmd->mVulkan.pVkCmdBuf != VK_NULL_HANDLE);

	vkCmdDispatch(pCmd->mVulkan.pVkCmdBuf, groupCountX, groupCountY, groupCountZ);
}

void vk_cmdResourceBarrier(
	Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers,
	uint32_t numRtBarriers, RenderTargetBarrier* pRtBarriers)
{
	VkImageMemoryBarrier* imageBarriers =
		(numTextureBarriers + numRtBarriers)
			? (VkImageMemoryBarrier*)alloca((numTextureBarriers + numRtBarriers) * sizeof(VkImageMemoryBarrier))
			: NULL;
	uint32_t imageBarrierCount = 0;

	VkBufferMemoryBarrier* bufferBarriers =
		numBufferBarriers ? (VkBufferMemoryBarrier*)alloca(numBufferBarriers * sizeof(VkBufferMemoryBarrier)) : NULL;
	uint32_t bufferBarrierCount = 0;

	VkAccessFlags srcAccessFlags = 0;
	VkAccessFlags dstAccessFlags = 0;

	for (uint32_t i = 0; i < numBufferBarriers; ++i)
	{
		BufferBarrier*         pTrans = &pBufferBarriers[i];
		Buffer*                pBuffer = pTrans->pBuffer;
		VkBufferMemoryBarrier* pBufferBarrier = NULL;

		if (RESOURCE_STATE_UNORDERED_ACCESS == pTrans->mCurrentState && RESOURCE_STATE_UNORDERED_ACCESS == pTrans->mNewState)
		{
			pBufferBarrier = &bufferBarriers[bufferBarrierCount++];             //-V522
			pBufferBarrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;    //-V522
			pBufferBarrier->pNext = NULL;

			pBufferBarrier->srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			pBufferBarrier->dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
		}
		else
		{
			pBufferBarrier = &bufferBarriers[bufferBarrierCount++];
			pBufferBarrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			pBufferBarrier->pNext = NULL;

			pBufferBarrier->srcAccessMask = util_to_vk_access_flags(pTrans->mCurrentState);
			pBufferBarrier->dstAccessMask = util_to_vk_access_flags(pTrans->mNewState);
		}

		if (pBufferBarrier)
		{
			pBufferBarrier->buffer = pBuffer->mVulkan.pVkBuffer;
			pBufferBarrier->size = VK_WHOLE_SIZE;
			pBufferBarrier->offset = 0;

			if (pTrans->mAcquire)
			{
				pBufferBarrier->srcQueueFamilyIndex = pCmd->pRenderer->mVulkan.mQueueFamilyIndices[pTrans->mQueueType];
				pBufferBarrier->dstQueueFamilyIndex = pCmd->pQueue->mVulkan.mVkQueueFamilyIndex;
			}
			else if (pTrans->mRelease)
			{
				pBufferBarrier->srcQueueFamilyIndex = pCmd->pQueue->mVulkan.mVkQueueFamilyIndex;
				pBufferBarrier->dstQueueFamilyIndex = pCmd->pRenderer->mVulkan.mQueueFamilyIndices[pTrans->mQueueType];
			}
			else
			{
				pBufferBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				pBufferBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			}

			srcAccessFlags |= pBufferBarrier->srcAccessMask;
			dstAccessFlags |= pBufferBarrier->dstAccessMask;
		}
	}

	for (uint32_t i = 0; i < numTextureBarriers; ++i)
	{
		TextureBarrier*       pTrans = &pTextureBarriers[i];
		Texture*              pTexture = pTrans->pTexture;
		VkImageMemoryBarrier* pImageBarrier = NULL;

		if (RESOURCE_STATE_UNORDERED_ACCESS == pTrans->mCurrentState && RESOURCE_STATE_UNORDERED_ACCESS == pTrans->mNewState)
		{
			pImageBarrier = &imageBarriers[imageBarrierCount++];              //-V522
			pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;    //-V522
			pImageBarrier->pNext = NULL;

			pImageBarrier->srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			pImageBarrier->dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			pImageBarrier->oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			pImageBarrier->newLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		else
		{
			pImageBarrier = &imageBarriers[imageBarrierCount++];
			pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			pImageBarrier->pNext = NULL;

			pImageBarrier->srcAccessMask = util_to_vk_access_flags(pTrans->mCurrentState);
			pImageBarrier->dstAccessMask = util_to_vk_access_flags(pTrans->mNewState);
			pImageBarrier->oldLayout = util_to_vk_image_layout(pTrans->mCurrentState);
			pImageBarrier->newLayout = util_to_vk_image_layout(pTrans->mNewState);
		}

		if (pImageBarrier)
		{
			pImageBarrier->image = pTexture->mVulkan.pVkImage;
			pImageBarrier->subresourceRange.aspectMask = (VkImageAspectFlags)pTexture->mAspectMask;
			pImageBarrier->subresourceRange.baseMipLevel = pTrans->mSubresourceBarrier ? pTrans->mMipLevel : 0;
			pImageBarrier->subresourceRange.levelCount = pTrans->mSubresourceBarrier ? 1 : VK_REMAINING_MIP_LEVELS;
			pImageBarrier->subresourceRange.baseArrayLayer = pTrans->mSubresourceBarrier ? pTrans->mArrayLayer : 0;
			pImageBarrier->subresourceRange.layerCount = pTrans->mSubresourceBarrier ? 1 : VK_REMAINING_ARRAY_LAYERS;

			if (pTrans->mAcquire)
			{
				pImageBarrier->srcQueueFamilyIndex = pCmd->pRenderer->mVulkan.mQueueFamilyIndices[pTrans->mQueueType];
				pImageBarrier->dstQueueFamilyIndex = pCmd->pQueue->mVulkan.mVkQueueFamilyIndex;
			}
			else if (pTrans->mRelease)
			{
				pImageBarrier->srcQueueFamilyIndex = pCmd->pQueue->mVulkan.mVkQueueFamilyIndex;
				pImageBarrier->dstQueueFamilyIndex = pCmd->pRenderer->mVulkan.mQueueFamilyIndices[pTrans->mQueueType];
			}
			else
			{
				pImageBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				pImageBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			}

			srcAccessFlags |= pImageBarrier->srcAccessMask;
			dstAccessFlags |= pImageBarrier->dstAccessMask;
		}
	}

	for (uint32_t i = 0; i < numRtBarriers; ++i)
	{
		RenderTargetBarrier*  pTrans = &pRtBarriers[i];
		Texture*              pTexture = pTrans->pRenderTarget->pTexture;
		VkImageMemoryBarrier* pImageBarrier = NULL;

		if (RESOURCE_STATE_UNORDERED_ACCESS == pTrans->mCurrentState && RESOURCE_STATE_UNORDERED_ACCESS == pTrans->mNewState)
		{
			pImageBarrier = &imageBarriers[imageBarrierCount++];
			pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			pImageBarrier->pNext = NULL;

			pImageBarrier->srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			pImageBarrier->dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			pImageBarrier->oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			pImageBarrier->newLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		else
		{
			pImageBarrier = &imageBarriers[imageBarrierCount++];
			pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			pImageBarrier->pNext = NULL;

			pImageBarrier->srcAccessMask = util_to_vk_access_flags(pTrans->mCurrentState);
			pImageBarrier->dstAccessMask = util_to_vk_access_flags(pTrans->mNewState);
			pImageBarrier->oldLayout = util_to_vk_image_layout(pTrans->mCurrentState);
			pImageBarrier->newLayout = util_to_vk_image_layout(pTrans->mNewState);
		}

		if (pImageBarrier)
		{
			pImageBarrier->image = pTexture->mVulkan.pVkImage;
			pImageBarrier->subresourceRange.aspectMask = (VkImageAspectFlags)pTexture->mAspectMask;
			pImageBarrier->subresourceRange.baseMipLevel = pTrans->mSubresourceBarrier ? pTrans->mMipLevel : 0;
			pImageBarrier->subresourceRange.levelCount = pTrans->mSubresourceBarrier ? 1 : VK_REMAINING_MIP_LEVELS;
			pImageBarrier->subresourceRange.baseArrayLayer = pTrans->mSubresourceBarrier ? pTrans->mArrayLayer : 0;
			pImageBarrier->subresourceRange.layerCount = pTrans->mSubresourceBarrier ? 1 : VK_REMAINING_ARRAY_LAYERS;

			if (pTrans->mAcquire)
			{
				pImageBarrier->srcQueueFamilyIndex = pCmd->pRenderer->mVulkan.mQueueFamilyIndices[pTrans->mQueueType];
				pImageBarrier->dstQueueFamilyIndex = pCmd->pQueue->mVulkan.mVkQueueFamilyIndex;
			}
			else if (pTrans->mRelease)
			{
				pImageBarrier->srcQueueFamilyIndex = pCmd->pQueue->mVulkan.mVkQueueFamilyIndex;
				pImageBarrier->dstQueueFamilyIndex = pCmd->pRenderer->mVulkan.mQueueFamilyIndices[pTrans->mQueueType];
			}
			else
			{
				pImageBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				pImageBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			}

			srcAccessFlags |= pImageBarrier->srcAccessMask;
			dstAccessFlags |= pImageBarrier->dstAccessMask;
		}
	}

	VkPipelineStageFlags srcStageMask =
		util_determine_pipeline_stage_flags(pCmd->pRenderer, srcAccessFlags, (QueueType)pCmd->mVulkan.mType);
	VkPipelineStageFlags dstStageMask =
		util_determine_pipeline_stage_flags(pCmd->pRenderer, dstAccessFlags, (QueueType)pCmd->mVulkan.mType);

	if (bufferBarrierCount || imageBarrierCount)
	{
		vkCmdPipelineBarrier(
			pCmd->mVulkan.pVkCmdBuf, srcStageMask, dstStageMask, 0, 0, NULL, bufferBarrierCount, bufferBarriers, imageBarrierCount,
			imageBarriers);
	}
}

void vk_cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
	ASSERT(pCmd);
	ASSERT(pSrcBuffer);
	ASSERT(pSrcBuffer->mVulkan.pVkBuffer);
	ASSERT(pBuffer);
	ASSERT(pBuffer->mVulkan.pVkBuffer);
	ASSERT(srcOffset + size <= pSrcBuffer->mSize);
	ASSERT(dstOffset + size <= pBuffer->mSize);

	DECLARE_ZERO(VkBufferCopy, region);
	region.srcOffset = srcOffset;
	region.dstOffset = dstOffset;
	region.size = (VkDeviceSize)size;
	vkCmdCopyBuffer(pCmd->mVulkan.pVkCmdBuf, pSrcBuffer->mVulkan.pVkBuffer, pBuffer->mVulkan.pVkBuffer, 1, &region);
}

typedef struct SubresourceDataDesc
{
	uint64_t mSrcOffset;
	uint32_t mMipLevel;
	uint32_t mArrayLayer;
	uint32_t mRowPitch;
	uint32_t mSlicePitch;
} SubresourceDataDesc;

void vk_cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pSubresourceDesc)
{
	const TinyImageFormat fmt = (TinyImageFormat)pTexture->mFormat;
	const bool            isSinglePlane = TinyImageFormat_IsSinglePlane(fmt);

	if (isSinglePlane)
	{
		const uint32_t width = max<uint32_t>(1, pTexture->mWidth >> pSubresourceDesc->mMipLevel);
		const uint32_t height = max<uint32_t>(1, pTexture->mHeight >> pSubresourceDesc->mMipLevel);
		const uint32_t depth = max<uint32_t>(1, pTexture->mDepth >> pSubresourceDesc->mMipLevel);
		const uint32_t numBlocksWide = pSubresourceDesc->mRowPitch / (TinyImageFormat_BitSizeOfBlock(fmt) >> 3);
		const uint32_t numBlocksHigh = (pSubresourceDesc->mSlicePitch / pSubresourceDesc->mRowPitch);

		VkBufferImageCopy copy = {};
		copy.bufferOffset = pSubresourceDesc->mSrcOffset;
		copy.bufferRowLength = numBlocksWide * TinyImageFormat_WidthOfBlock(fmt);
		copy.bufferImageHeight = numBlocksHigh * TinyImageFormat_HeightOfBlock(fmt);
		copy.imageSubresource.aspectMask = (VkImageAspectFlags)pTexture->mAspectMask;
		copy.imageSubresource.mipLevel = pSubresourceDesc->mMipLevel;
		copy.imageSubresource.baseArrayLayer = pSubresourceDesc->mArrayLayer;
		copy.imageSubresource.layerCount = 1;
		copy.imageOffset.x = 0;
		copy.imageOffset.y = 0;
		copy.imageOffset.z = 0;
		copy.imageExtent.width = width;
		copy.imageExtent.height = height;
		copy.imageExtent.depth = depth;

		vkCmdCopyBufferToImage(
			pCmd->mVulkan.pVkCmdBuf, pSrcBuffer->mVulkan.pVkBuffer, pTexture->mVulkan.pVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
			&copy);
	}
	else
	{
		const uint32_t width = pTexture->mWidth;
		const uint32_t height = pTexture->mHeight;
		const uint32_t depth = pTexture->mDepth;
		const uint32_t numOfPlanes = TinyImageFormat_NumOfPlanes(fmt);

		uint64_t          offset = pSubresourceDesc->mSrcOffset;
		VkBufferImageCopy bufferImagesCopy[MAX_PLANE_COUNT];

		for (uint32_t i = 0; i < numOfPlanes; ++i)
		{
			VkBufferImageCopy& copy = bufferImagesCopy[i];
			copy.bufferOffset = offset;
			copy.bufferRowLength = 0;
			copy.bufferImageHeight = 0;
			copy.imageSubresource.aspectMask = (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_PLANE_0_BIT << i);
			copy.imageSubresource.mipLevel = pSubresourceDesc->mMipLevel;
			copy.imageSubresource.baseArrayLayer = pSubresourceDesc->mArrayLayer;
			copy.imageSubresource.layerCount = 1;
			copy.imageOffset.x = 0;
			copy.imageOffset.y = 0;
			copy.imageOffset.z = 0;
			copy.imageExtent.width = TinyImageFormat_PlaneWidth(fmt, i, width);
			copy.imageExtent.height = TinyImageFormat_PlaneHeight(fmt, i, height);
			copy.imageExtent.depth = depth;
			offset += copy.imageExtent.width * copy.imageExtent.height * TinyImageFormat_PlaneSizeOfBlock(fmt, i);
		}

		vkCmdCopyBufferToImage(
			pCmd->mVulkan.pVkCmdBuf, pSrcBuffer->mVulkan.pVkBuffer, pTexture->mVulkan.pVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			numOfPlanes, bufferImagesCopy);
	}
}

/************************************************************************/
// Queue Fence Semaphore Functions
/************************************************************************/
void vk_acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex)
{
	ASSERT(pRenderer);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);
	ASSERT(pSignalSemaphore || pFence);

#if defined(QUEST_VR)
    ASSERT(VK_NULL_HANDLE != pSwapChain->mVR.pSwapChain);
    hook_acquire_next_image(pSwapChain, pImageIndex);
    return;
#else
    ASSERT(VK_NULL_HANDLE != pSwapChain->mVulkan.pSwapChain);
#endif

	VkResult vk_res = {};

	if (pFence != NULL)
	{
		vk_res = vkAcquireNextImageKHR(
			pRenderer->mVulkan.pVkDevice, pSwapChain->mVulkan.pSwapChain, UINT64_MAX, VK_NULL_HANDLE, pFence->mVulkan.pVkFence,
			pImageIndex);

		// If swapchain is out of date, let caller know by setting image index to -1
		if (vk_res == VK_ERROR_OUT_OF_DATE_KHR)
		{
			*pImageIndex = -1;
			vkResetFences(pRenderer->mVulkan.pVkDevice, 1, &pFence->mVulkan.pVkFence);
			pFence->mVulkan.mSubmitted = false;
			return;
		}

		pFence->mVulkan.mSubmitted = true;
	}
	else
	{
		vk_res = vkAcquireNextImageKHR(
			pRenderer->mVulkan.pVkDevice, pSwapChain->mVulkan.pSwapChain, UINT64_MAX, pSignalSemaphore->mVulkan.pVkSemaphore,
			VK_NULL_HANDLE, pImageIndex);    //-V522

		// If swapchain is out of date, let caller know by setting image index to -1
		if (vk_res == VK_ERROR_OUT_OF_DATE_KHR)
		{
			*pImageIndex = -1;
			pSignalSemaphore->mVulkan.mSignaled = false;
			return;
		}

		ASSERT(VK_SUCCESS == vk_res);
		pSignalSemaphore->mVulkan.mSignaled = true;
	}
}

void vk_queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc)
{
	ASSERT(pQueue);
	ASSERT(pDesc);

	uint32_t    cmdCount = pDesc->mCmdCount;
	Cmd**       ppCmds = pDesc->ppCmds;
	Fence*      pFence = pDesc->pSignalFence;
	uint32_t    waitSemaphoreCount = pDesc->mWaitSemaphoreCount;
	Semaphore** ppWaitSemaphores = pDesc->ppWaitSemaphores;
	uint32_t    signalSemaphoreCount = pDesc->mSignalSemaphoreCount;
	Semaphore** ppSignalSemaphores = pDesc->ppSignalSemaphores;

	ASSERT(cmdCount > 0);
	ASSERT(ppCmds);
	if (waitSemaphoreCount > 0)
	{
		ASSERT(ppWaitSemaphores);
	}
	if (signalSemaphoreCount > 0)
	{
		ASSERT(ppSignalSemaphores);
	}

	ASSERT(VK_NULL_HANDLE != pQueue->mVulkan.pVkQueue);

	VkCommandBuffer* cmds = (VkCommandBuffer*)alloca(cmdCount * sizeof(VkCommandBuffer));
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		cmds[i] = ppCmds[i]->mVulkan.pVkCmdBuf;
	}

	VkSemaphore*          wait_semaphores = waitSemaphoreCount ? (VkSemaphore*)alloca(waitSemaphoreCount * sizeof(VkSemaphore)) : NULL;
	VkPipelineStageFlags* wait_masks = (VkPipelineStageFlags*)alloca(waitSemaphoreCount * sizeof(VkPipelineStageFlags));
	uint32_t              waitCount = 0;
	for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
	{
		if (ppWaitSemaphores[i]->mVulkan.mSignaled)
		{
			wait_semaphores[waitCount] = ppWaitSemaphores[i]->mVulkan.pVkSemaphore;    //-V522
			wait_masks[waitCount] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			++waitCount;

			ppWaitSemaphores[i]->mVulkan.mSignaled = false;
		}
	}

	VkSemaphore* signal_semaphores = signalSemaphoreCount ? (VkSemaphore*)alloca(signalSemaphoreCount * sizeof(VkSemaphore)) : NULL;
	uint32_t     signalCount = 0;
	for (uint32_t i = 0; i < signalSemaphoreCount; ++i)
	{
		if (!ppSignalSemaphores[i]->mVulkan.mSignaled)
		{
			signal_semaphores[signalCount] = ppSignalSemaphores[i]->mVulkan.pVkSemaphore;    //-V522
			ppSignalSemaphores[i]->mVulkan.mCurrentNodeIndex = pQueue->mNodeIndex;
			ppSignalSemaphores[i]->mVulkan.mSignaled = true;
			++signalCount;
		}
	}

	DECLARE_ZERO(VkSubmitInfo, submit_info);
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.waitSemaphoreCount = waitCount;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_masks;
	submit_info.commandBufferCount = cmdCount;
	submit_info.pCommandBuffers = cmds;
	submit_info.signalSemaphoreCount = signalCount;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkDeviceGroupSubmitInfo deviceGroupSubmitInfo = { VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO_KHR };
	if (pQueue->mVulkan.mGpuMode == GPU_MODE_LINKED)
	{
		uint32_t* pVkDeviceMasks = NULL;
		uint32_t* pSignalIndices = NULL;
		uint32_t* pWaitIndices = NULL;
		deviceGroupSubmitInfo.pNext = NULL;
		deviceGroupSubmitInfo.commandBufferCount = submit_info.commandBufferCount;
		deviceGroupSubmitInfo.signalSemaphoreCount = submit_info.signalSemaphoreCount;
		deviceGroupSubmitInfo.waitSemaphoreCount = submit_info.waitSemaphoreCount;

		pVkDeviceMasks = (uint32_t*)alloca(deviceGroupSubmitInfo.commandBufferCount * sizeof(uint32_t));
		pSignalIndices = (uint32_t*)alloca(deviceGroupSubmitInfo.signalSemaphoreCount * sizeof(uint32_t));
		pWaitIndices = (uint32_t*)alloca(deviceGroupSubmitInfo.waitSemaphoreCount * sizeof(uint32_t));

		for (uint32_t i = 0; i < deviceGroupSubmitInfo.commandBufferCount; ++i)
		{
			pVkDeviceMasks[i] = (1 << ppCmds[i]->mVulkan.mNodeIndex);
		}
		for (uint32_t i = 0; i < deviceGroupSubmitInfo.signalSemaphoreCount; ++i)
		{
			pSignalIndices[i] = pQueue->mNodeIndex;
		}
		for (uint32_t i = 0; i < deviceGroupSubmitInfo.waitSemaphoreCount; ++i)
		{
			pWaitIndices[i] = ppWaitSemaphores[i]->mVulkan.mCurrentNodeIndex;
		}

		deviceGroupSubmitInfo.pCommandBufferDeviceMasks = pVkDeviceMasks;
		deviceGroupSubmitInfo.pSignalSemaphoreDeviceIndices = pSignalIndices;
		deviceGroupSubmitInfo.pWaitSemaphoreDeviceIndices = pWaitIndices;
		submit_info.pNext = &deviceGroupSubmitInfo;
	}

	// Lightweight lock to make sure multiple threads dont use the same queue simultaneously
	// Many setups have just one queue family and one queue. In this case, async compute, async transfer doesn't exist and we end up using
	// the same queue for all three operations
	MutexLock lock(*pQueue->mVulkan.pSubmitMutex);
	VkResult  vk_res = vkQueueSubmit(pQueue->mVulkan.pVkQueue, 1, &submit_info, pFence ? pFence->mVulkan.pVkFence : VK_NULL_HANDLE);
	ASSERT(VK_SUCCESS == vk_res);

	if (pFence)
		pFence->mVulkan.mSubmitted = true;
}

void vk_queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc)
{
	ASSERT(pQueue);
	ASSERT(pDesc);

#if defined(QUEST_VR)
    hook_queue_present(pDesc);
    return;
#endif

	uint32_t    waitSemaphoreCount = pDesc->mWaitSemaphoreCount;
	Semaphore** ppWaitSemaphores = pDesc->ppWaitSemaphores;
	if (pDesc->pSwapChain)
	{
		SwapChain* pSwapChain = pDesc->pSwapChain;

		ASSERT(pQueue);
		if (waitSemaphoreCount > 0)
		{
			ASSERT(ppWaitSemaphores);
		}

		ASSERT(VK_NULL_HANDLE != pQueue->mVulkan.pVkQueue);

		VkSemaphore* wait_semaphores = waitSemaphoreCount ? (VkSemaphore*)alloca(waitSemaphoreCount * sizeof(VkSemaphore)) : NULL;
		uint32_t     waitCount = 0;
		for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
		{
			if (ppWaitSemaphores[i]->mVulkan.mSignaled)
			{
				wait_semaphores[waitCount] = ppWaitSemaphores[i]->mVulkan.pVkSemaphore;    //-V522
				ppWaitSemaphores[i]->mVulkan.mSignaled = false;
				++waitCount;
			}
		}

		uint32_t presentIndex = pDesc->mIndex;

		DECLARE_ZERO(VkPresentInfoKHR, present_info);
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.pNext = NULL;
		present_info.waitSemaphoreCount = waitCount;
		present_info.pWaitSemaphores = wait_semaphores;
		present_info.swapchainCount = 1;
		present_info.pSwapchains = &(pSwapChain->mVulkan.pSwapChain);
		present_info.pImageIndices = &(presentIndex);
		present_info.pResults = NULL;

		// Lightweight lock to make sure multiple threads dont use the same queue simultaneously
		MutexLock lock(*pQueue->mVulkan.pSubmitMutex);
		VkResult  vk_res = vkQueuePresentKHR(
            pSwapChain->mVulkan.pPresentQueue ? pSwapChain->mVulkan.pPresentQueue : pQueue->mVulkan.pVkQueue, &present_info);

		if (vk_res == VK_ERROR_DEVICE_LOST)
		{
			// Will crash normally on Android.
#if defined(_WINDOWS)
			threadSleep(5000);    // Wait for a few seconds to allow the driver to come back online before doing a reset.
			onDeviceLost();
#endif
		}
		else if (vk_res == VK_ERROR_OUT_OF_DATE_KHR)
		{
			// TODO : Fix bug where we get this error if window is closed before able to present queue.
		}
		else if (vk_res != VK_SUCCESS && vk_res != VK_SUBOPTIMAL_KHR)
		{
			ASSERT(0);
		}
	}
}

void vk_waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
{
	ASSERT(pRenderer);
	ASSERT(fenceCount);
	ASSERT(ppFences);

	VkFence* fences = (VkFence*)alloca(fenceCount * sizeof(VkFence));
	uint32_t numValidFences = 0;
	for (uint32_t i = 0; i < fenceCount; ++i)
	{
		if (ppFences[i]->mVulkan.mSubmitted)
			fences[numValidFences++] = ppFences[i]->mVulkan.pVkFence;
	}

	if (numValidFences)
	{
#if defined(USE_NSIGHT_AFTERMATH)
		VkResult result = vkWaitForFences(pRenderer->mVulkan.pVkDevice, numValidFences, fences, VK_TRUE, UINT64_MAX);
		if (pRenderer->mAftermathSupport)
		{
			if (VK_ERROR_DEVICE_LOST == result)
			{
				// Device lost notification is asynchronous to the NVIDIA display
				// driver's GPU crash handling. Give the Nsight Aftermath GPU crash dump
				// thread some time to do its work before terminating the process.
				sleep(3000);
			}
		}
#else
		vkWaitForFences(pRenderer->mVulkan.pVkDevice, numValidFences, fences, VK_TRUE, UINT64_MAX);
#endif
		vkResetFences(pRenderer->mVulkan.pVkDevice, numValidFences, fences);
	}

	for (uint32_t i = 0; i < fenceCount; ++i)
		ppFences[i]->mVulkan.mSubmitted = false;
}

void vk_waitQueueIdle(Queue* pQueue) { vkQueueWaitIdle(pQueue->mVulkan.pVkQueue); }

void vk_getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
	*pFenceStatus = FENCE_STATUS_COMPLETE;

	if (pFence->mVulkan.mSubmitted)
	{
		VkResult vkRes = vkGetFenceStatus(pRenderer->mVulkan.pVkDevice, pFence->mVulkan.pVkFence);
		if (vkRes == VK_SUCCESS)
		{
			vkResetFences(pRenderer->mVulkan.pVkDevice, 1, &pFence->mVulkan.pVkFence);
			pFence->mVulkan.mSubmitted = false;
		}

		*pFenceStatus = vkRes == VK_SUCCESS ? FENCE_STATUS_COMPLETE : FENCE_STATUS_INCOMPLETE;
	}
	else
	{
		*pFenceStatus = FENCE_STATUS_NOTSUBMITTED;
	}
}
/************************************************************************/
// Utility functions
/************************************************************************/
TinyImageFormat vk_getRecommendedSwapchainFormat(bool hintHDR)
{
	//TODO: figure out this properly. BGRA not supported on android
#if !defined(VK_USE_PLATFORM_ANDROID_KHR) && !defined(VK_USE_PLATFORM_VI_NN)
	return TinyImageFormat_B8G8R8A8_UNORM;
#else
	return TinyImageFormat_R8G8B8A8_UNORM;
#endif
}

/************************************************************************/
// Indirect draw functions
/************************************************************************/
void vk_addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppCommandSignature);

	CommandSignature* pCommandSignature =
		(CommandSignature*)tf_calloc(1, sizeof(CommandSignature) + sizeof(IndirectArgument) * pDesc->mIndirectArgCount);
	ASSERT(pCommandSignature);

	pCommandSignature->pIndirectArguments = (IndirectArgument*)(pCommandSignature + 1);    //-V1027
	pCommandSignature->mIndirectArgumentCount = pDesc->mIndirectArgCount;
	uint32_t offset = 0;

	for (uint32_t i = 0; i < pDesc->mIndirectArgCount; ++i)    // counting for all types;
	{
		pCommandSignature->pIndirectArguments[i].mType = pDesc->pArgDescs[i].mType;
		pCommandSignature->pIndirectArguments[i].mOffset = offset;

		switch (pDesc->pArgDescs[i].mType)
		{
			case INDIRECT_DRAW:
				pCommandSignature->mStride += sizeof(IndirectDrawArguments);
				offset += sizeof(IndirectDrawArguments);
				break;
			case INDIRECT_DRAW_INDEX:
				pCommandSignature->mStride += sizeof(IndirectDrawIndexArguments);
				offset += sizeof(IndirectDrawIndexArguments);
				break;
			case INDIRECT_DISPATCH:
				pCommandSignature->mStride += sizeof(IndirectDispatchArguments);
				offset += sizeof(IndirectDispatchArguments);
				break;
			default:
				pCommandSignature->mStride += pDesc->pArgDescs[i].mByteSize;
				offset += pDesc->pArgDescs[i].mByteSize;
				LOGF(LogLevel::eWARNING, "Vulkan runtime only supports IndirectDraw, IndirectDrawIndex and IndirectDispatch at this point");
				break;
		}
	}

	if (!pDesc->mPacked)
	{
		pCommandSignature->mStride = round_up(pCommandSignature->mStride, 16);
	}

	*ppCommandSignature = pCommandSignature;
}

void vk_removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
{
	pCommandSignature->mStride = 0;
	SAFE_FREE(pCommandSignature);
}

void vk_cmdExecuteIndirect(
	Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
	Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
	for (uint32_t i = 0; i < pCommandSignature->mIndirectArgumentCount; ++i)    // execute for all types;
	{
		IndirectArgument* pIndirectArgument = &pCommandSignature->pIndirectArguments[i];
		uint64_t          offset = bufferOffset + pIndirectArgument->mOffset;

		if (pIndirectArgument->mType == INDIRECT_DRAW)
		{
#ifndef NX64
			if (pCounterBuffer && pfnVkCmdDrawIndirectCountKHR)
				pfnVkCmdDrawIndirectCountKHR(
					pCmd->mVulkan.pVkCmdBuf, pIndirectBuffer->mVulkan.pVkBuffer, offset, pCounterBuffer->mVulkan.pVkBuffer,
					counterBufferOffset, maxCommandCount, pCommandSignature->mStride);
			else
#endif
				vkCmdDrawIndirect(
					pCmd->mVulkan.pVkCmdBuf, pIndirectBuffer->mVulkan.pVkBuffer, offset, maxCommandCount, pCommandSignature->mStride);
		}
		else if (pIndirectArgument->mType == INDIRECT_DRAW_INDEX)
		{
#ifndef NX64
			if (pCounterBuffer && pfnVkCmdDrawIndexedIndirectCountKHR)
				pfnVkCmdDrawIndexedIndirectCountKHR(
					pCmd->mVulkan.pVkCmdBuf, pIndirectBuffer->mVulkan.pVkBuffer, offset, pCounterBuffer->mVulkan.pVkBuffer,
					counterBufferOffset, maxCommandCount, pCommandSignature->mStride);
			else
#endif
				vkCmdDrawIndexedIndirect(
					pCmd->mVulkan.pVkCmdBuf, pIndirectBuffer->mVulkan.pVkBuffer, offset, maxCommandCount, pCommandSignature->mStride);
		}
		else if (pIndirectArgument->mType == INDIRECT_DISPATCH)
		{
			vkCmdDispatchIndirect(pCmd->mVulkan.pVkCmdBuf, pIndirectBuffer->mVulkan.pVkBuffer, offset);
		}
	}
}
/************************************************************************/
// Query Heap Implementation
/************************************************************************/
VkQueryType util_to_vk_query_type(QueryType type)
{
	switch (type)
	{
		case QUERY_TYPE_TIMESTAMP: return VK_QUERY_TYPE_TIMESTAMP;
		case QUERY_TYPE_PIPELINE_STATISTICS: return VK_QUERY_TYPE_PIPELINE_STATISTICS;
		case QUERY_TYPE_OCCLUSION: return VK_QUERY_TYPE_OCCLUSION;
		default: ASSERT(false && "Invalid query heap type"); return VK_QUERY_TYPE_MAX_ENUM;
	}
}

void vk_getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
	ASSERT(pQueue);
	ASSERT(pFrequency);

	// The framework is using ticks per sec as frequency. Vulkan is nano sec per tick.
	// Handle the conversion logic here.
	*pFrequency =
		1.0f /
		((double)pQueue->mVulkan.mTimestampPeriod /*ns/tick number of nanoseconds required for a timestamp query to be incremented by 1*/
		 * 1e-9);                                 // convert to ticks/sec (DX12 standard)
}

void vk_addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppQueryPool);

	QueryPool* pQueryPool = (QueryPool*)tf_calloc(1, sizeof(QueryPool));
	ASSERT(ppQueryPool);

	pQueryPool->mVulkan.mType = util_to_vk_query_type(pDesc->mType);
	pQueryPool->mCount = pDesc->mQueryCount;

	VkQueryPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	createInfo.pNext = NULL;
	createInfo.queryCount = pDesc->mQueryCount;
	createInfo.queryType = util_to_vk_query_type(pDesc->mType);
	createInfo.flags = 0;
	createInfo.pipelineStatistics = 0;
	CHECK_VKRESULT(
		vkCreateQueryPool(pRenderer->mVulkan.pVkDevice, &createInfo, &gVkAllocationCallbacks, &pQueryPool->mVulkan.pVkQueryPool));

	*ppQueryPool = pQueryPool;
}

void vk_removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
	ASSERT(pRenderer);
	ASSERT(pQueryPool);
	vkDestroyQueryPool(pRenderer->mVulkan.pVkDevice, pQueryPool->mVulkan.pVkQueryPool, &gVkAllocationCallbacks);

	SAFE_FREE(pQueryPool);
}

void vk_cmdResetQueryPool(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
	vkCmdResetQueryPool(pCmd->mVulkan.pVkCmdBuf, pQueryPool->mVulkan.pVkQueryPool, startQuery, queryCount);
}

void vk_cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
	VkQueryType type = pQueryPool->mVulkan.mType;
	switch (type)
	{
		case VK_QUERY_TYPE_TIMESTAMP:
			vkCmdWriteTimestamp(
				pCmd->mVulkan.pVkCmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pQueryPool->mVulkan.pVkQueryPool, pQuery->mIndex);
			break;
		case VK_QUERY_TYPE_PIPELINE_STATISTICS: break;
		case VK_QUERY_TYPE_OCCLUSION: break;
		default: break;
	}
}

void vk_cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery) { cmdBeginQuery(pCmd, pQueryPool, pQuery); }

void vk_cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
{
	VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT;
#ifdef ANDROID
	flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
#else
	flags |= VK_QUERY_RESULT_WAIT_BIT;
#endif
	vkCmdCopyQueryPoolResults(
		pCmd->mVulkan.pVkCmdBuf, pQueryPool->mVulkan.pVkQueryPool, startQuery, queryCount, pReadbackBuffer->mVulkan.pVkBuffer, 0,
		sizeof(uint64_t), flags);
}
/************************************************************************/
// Memory Stats Implementation
/************************************************************************/
void vk_calculateMemoryStats(Renderer* pRenderer, char** stats) { vmaBuildStatsString(pRenderer->mVulkan.pVmaAllocator, stats, VK_TRUE); }

void vk_calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
{
	VmaStats stats;
	pRenderer->mVulkan.pVmaAllocator->CalculateStats(&stats);
	*usedBytes = stats.total.usedBytes;
	*totalAllocatedBytes = *usedBytes + stats.total.unusedBytes;
}

void vk_freeMemoryStats(Renderer* pRenderer, char* stats) { vmaFreeStatsString(pRenderer->mVulkan.pVmaAllocator, stats); }
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void vk_cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	if (gDebugMarkerSupport)
	{
#ifdef USE_DEBUG_UTILS_EXTENSION
		VkDebugUtilsLabelEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		markerInfo.color[0] = r;
		markerInfo.color[1] = g;
		markerInfo.color[2] = b;
		markerInfo.color[3] = 1.0f;
		markerInfo.pLabelName = pName;
		vkCmdBeginDebugUtilsLabelEXT(pCmd->mVulkan.pVkCmdBuf, &markerInfo);
#elif !defined(NX64) || !defined(USE_RENDER_DOC)
		VkDebugMarkerMarkerInfoEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		markerInfo.color[0] = r;
		markerInfo.color[1] = g;
		markerInfo.color[2] = b;
		markerInfo.color[3] = 1.0f;
		markerInfo.pMarkerName = pName;
		vkCmdDebugMarkerBeginEXT(pCmd->mVulkan.pVkCmdBuf, &markerInfo);
#endif
	}
}

void vk_cmdEndDebugMarker(Cmd* pCmd)
{
	if (gDebugMarkerSupport)
	{
#ifdef USE_DEBUG_UTILS_EXTENSION
		vkCmdEndDebugUtilsLabelEXT(pCmd->mVulkan.pVkCmdBuf);
#elif !defined(NX64) || !defined(USE_RENDER_DOC)
		vkCmdDebugMarkerEndEXT(pCmd->mVulkan.pVkCmdBuf);
#endif
	}
}

void vk_cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	if (gDebugMarkerSupport)
	{
#ifdef USE_DEBUG_UTILS_EXTENSION
		VkDebugUtilsLabelEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		markerInfo.color[0] = r;
		markerInfo.color[1] = g;
		markerInfo.color[2] = b;
		markerInfo.color[3] = 1.0f;
		markerInfo.pLabelName = pName;
		vkCmdInsertDebugUtilsLabelEXT(pCmd->mVulkan.pVkCmdBuf, &markerInfo);
#else
		VkDebugMarkerMarkerInfoEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		markerInfo.color[0] = r;
		markerInfo.color[1] = g;
		markerInfo.color[2] = b;
		markerInfo.color[3] = 1.0f;
		markerInfo.pMarkerName = pName;
		vkCmdDebugMarkerInsertEXT(pCmd->mVulkan.pVkCmdBuf, &markerInfo);
#endif
	}

#if defined(USE_NSIGHT_AFTERMATH)
	if (pCmd->pRenderer->mAftermathSupport)
	{
		vkCmdSetCheckpointNV(pCmd->pVkCmdBuf, pName);
	}
#endif
}

uint32_t vk_cmdWriteMarker(Cmd* pCmd, MarkerType markerType, uint32_t markerValue, Buffer* pBuffer, size_t offset, bool useAutoFlags)
{
	return 0;
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
#ifdef USE_DEBUG_UTILS_EXTENSION
void util_set_object_name(VkDevice pDevice, uint64_t handle, VkObjectType type, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	if (gDebugMarkerSupport)
	{
		VkDebugUtilsObjectNameInfoEXT nameInfo = {};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = type;
		nameInfo.objectHandle = handle;
		nameInfo.pObjectName = pName;
		vkSetDebugUtilsObjectNameEXT(pDevice, &nameInfo);
	}
#endif
}
#else
void util_set_object_name(VkDevice pDevice, uint64_t handle, VkDebugReportObjectTypeEXT type, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	if (gDebugMarkerSupport)
	{
		VkDebugMarkerObjectNameInfoEXT nameInfo = {};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = type;
		nameInfo.object = (uint64_t)handle;
		nameInfo.pObjectName = pName;
		vkDebugMarkerSetObjectNameEXT(pDevice, &nameInfo);
	}
#endif
}
#endif

void vk_setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
	ASSERT(pRenderer);
	ASSERT(pBuffer);
	ASSERT(pName);

#ifdef USE_DEBUG_UTILS_EXTENSION
	util_set_object_name(pRenderer->mVulkan.pVkDevice, (uint64_t)pBuffer->mVulkan.pVkBuffer, VK_OBJECT_TYPE_BUFFER, pName);
#else
	util_set_object_name(pRenderer->mVulkan.pVkDevice, (uint64_t)pBuffer->mVulkan.pVkBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pName);
#endif
}

void vk_setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
	ASSERT(pRenderer);
	ASSERT(pTexture);
	ASSERT(pName);

#ifdef USE_DEBUG_UTILS_EXTENSION
	util_set_object_name(pRenderer->mVulkan.pVkDevice, (uint64_t)pTexture->mVulkan.pVkImage, VK_OBJECT_TYPE_IMAGE, pName);
#else
	util_set_object_name(pRenderer->mVulkan.pVkDevice, (uint64_t)pTexture->mVulkan.pVkImage, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, pName);
#endif
}

void vk_setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName)
{
	setTextureName(pRenderer, pRenderTarget->pTexture, pName);
}

void vk_setPipelineName(Renderer* pRenderer, Pipeline* pPipeline, const char* pName)
{
	ASSERT(pRenderer);
	ASSERT(pPipeline);
	ASSERT(pName);

#ifdef USE_DEBUG_UTILS_EXTENSION
	util_set_object_name(pRenderer->mVulkan.pVkDevice, (uint64_t)pPipeline->mVulkan.pVkPipeline, VK_OBJECT_TYPE_PIPELINE, pName);
#else
	util_set_object_name(
		pRenderer->mVulkan.pVkDevice, (uint64_t)pPipeline->mVulkan.pVkPipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, pName);
#endif
}
/************************************************************************/
// Virtual Texture
/************************************************************************/
void alignedDivision(const VkExtent3D& extent, const VkExtent3D& granularity, VkExtent3D* out)
{
	out->width = (extent.width / granularity.width + ((extent.width % granularity.width) ? 1u : 0u));
	out->height = (extent.height / granularity.height + ((extent.height % granularity.height) ? 1u : 0u));
	out->depth = (extent.depth / granularity.depth + ((extent.depth % granularity.depth) ? 1u : 0u));
}

struct VkVTPendingPageDeletion
{
	VmaAllocation* pAllocations;
	uint32_t* pAllocationsCount;

	Buffer** pIntermediateBuffers;
	uint32_t* pIntermediateBuffersCount;
};

// Allocate Vulkan memory for the virtual page
static bool allocateVirtualPage(Renderer* pRenderer, Texture* pTexture, VirtualTexturePage& virtualPage, Buffer** ppIntermediateBuffer)
{
	if (virtualPage.mVulkan.imageMemoryBind.memory != VK_NULL_HANDLE)
	{
		//already filled
		return false;
	};

	BufferDesc desc = {};
	desc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
	desc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;

	desc.mFirstElement = 0;
	desc.mElementCount = pTexture->pSvt->mSparseVirtualTexturePageWidth * pTexture->pSvt->mSparseVirtualTexturePageHeight;
	desc.mStructStride = sizeof(uint32_t);
	desc.mSize = desc.mElementCount * desc.mStructStride;
#if defined(ENABLE_GRAPHICS_DEBUG)
	char debugNameBuffer[MAX_DEBUG_NAME_LENGTH]{};
	snprintf(debugNameBuffer, MAX_DEBUG_NAME_LENGTH, "(tex %p) VT page #%u intermediate buffer", pTexture, virtualPage.index);
	desc.pName = debugNameBuffer;
#endif
	addBuffer(pRenderer, &desc, ppIntermediateBuffer);

	VkMemoryRequirements memReqs = {};
	memReqs.size = virtualPage.mVulkan.size;
	memReqs.memoryTypeBits = pTexture->pSvt->mVulkan.mSparseMemoryTypeBits;
	memReqs.alignment = memReqs.size;

	VmaAllocationCreateInfo vmaAllocInfo = {};
	vmaAllocInfo.pool = (VmaPool)pTexture->pSvt->mVulkan.pPool;

	VmaAllocation allocation;
	VmaAllocationInfo allocationInfo;
	CHECK_VKRESULT(vmaAllocateMemory(pRenderer->mVulkan.pVmaAllocator, &memReqs, &vmaAllocInfo, &allocation, &allocationInfo));
	ASSERT(allocation->GetAlignment() == memReqs.size || allocation->GetAlignment() == 0);

	virtualPage.mVulkan.pAllocation = allocation;
	virtualPage.mVulkan.imageMemoryBind.memory = allocation->GetMemory();
	virtualPage.mVulkan.imageMemoryBind.memoryOffset = allocation->GetOffset();

	// Sparse image memory binding
	virtualPage.mVulkan.imageMemoryBind.subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	virtualPage.mVulkan.imageMemoryBind.subresource.mipLevel = virtualPage.mipLevel;
	virtualPage.mVulkan.imageMemoryBind.subresource.arrayLayer = virtualPage.layer;

	++pTexture->pSvt->mVirtualPageAliveCount;

	return true;
}

VirtualTexturePage* addPage(Renderer* pRenderer, Texture* pTexture, const VkOffset3D& offset, const VkExtent3D& extent, const VkDeviceSize size, const uint32_t mipLevel, uint32_t layer, uint32_t pageIndex)
{
	VirtualTexturePage& newPage = pTexture->pSvt->pPages[pageIndex];

	newPage.mVulkan.imageMemoryBind.offset = offset;
	newPage.mVulkan.imageMemoryBind.extent = extent;
	newPage.mVulkan.size = size;
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
static VkVTPendingPageDeletion vtGetPendingPageDeletion(VirtualTexture* pSvt, uint32_t currentImage)
{
	if (pSvt->mPendingDeletionCount <= currentImage)
	{
		// Grow arrays
		const uint32_t oldDeletionCount = pSvt->mPendingDeletionCount;
		pSvt->mPendingDeletionCount = currentImage + 1;
		pSvt->mVulkan.pPendingDeletedAllocations = (void**)tf_realloc(pSvt->mVulkan.pPendingDeletedAllocations,
			pSvt->mPendingDeletionCount * pSvt->mVirtualPageTotalCount * sizeof(pSvt->mVulkan.pPendingDeletedAllocations[0]));

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

	VkVTPendingPageDeletion pendingDeletion;
	pendingDeletion.pAllocations = (VmaAllocation*)&pSvt->mVulkan.pPendingDeletedAllocations[currentImage * pSvt->mVirtualPageTotalCount];
	pendingDeletion.pAllocationsCount = &pSvt->pPendingDeletedAllocationsCount[currentImage];
	pendingDeletion.pIntermediateBuffers = &pSvt->pPendingDeletedBuffers[currentImage * pSvt->mVirtualPageTotalCount];
	pendingDeletion.pIntermediateBuffersCount = &pSvt->pPendingDeletedBuffersCount[currentImage];
	return pendingDeletion;
}

void vk_updateVirtualTextureMemory(Cmd* pCmd, Texture* pTexture, uint32_t imageMemoryCount)
{
	// Update sparse bind info
	if (imageMemoryCount > 0)
	{
		VkBindSparseInfo bindSparseInfo = {};
		bindSparseInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;

		// Image memory binds
		VkSparseImageMemoryBindInfo imageMemoryBindInfo = {};
		imageMemoryBindInfo.image = pTexture->mVulkan.pVkImage;
		imageMemoryBindInfo.bindCount = imageMemoryCount;
		imageMemoryBindInfo.pBinds = pTexture->pSvt->mVulkan.pSparseImageMemoryBinds;
		bindSparseInfo.imageBindCount = 1;
		bindSparseInfo.pImageBinds = &imageMemoryBindInfo;

		// Opaque image memory binds (mip tail)
		VkSparseImageOpaqueMemoryBindInfo opaqueMemoryBindInfo = {};
		opaqueMemoryBindInfo.image = pTexture->mVulkan.pVkImage;
		opaqueMemoryBindInfo.bindCount = pTexture->pSvt->mVulkan.mOpaqueMemoryBindsCount;
		opaqueMemoryBindInfo.pBinds = pTexture->pSvt->mVulkan.pOpaqueMemoryBinds;
		bindSparseInfo.imageOpaqueBindCount = (opaqueMemoryBindInfo.bindCount > 0) ? 1 : 0;
		bindSparseInfo.pImageOpaqueBinds = &opaqueMemoryBindInfo;

		CHECK_VKRESULT(vkQueueBindSparse(pCmd->pQueue->mVulkan.pVkQueue, (uint32_t)1, &bindSparseInfo, VK_NULL_HANDLE));
	}
}

void vk_releasePage(Cmd* pCmd, Texture* pTexture, uint32_t currentImage)
{
	Renderer* pRenderer = pCmd->pRenderer;

	VirtualTexturePage* pPageTable = pTexture->pSvt->pPages;

	VTReadbackBufOffsets offsets = vtGetReadbackBufOffsets(
		(uint32_t*)pTexture->pSvt->pReadbackBuffer->pCpuMappedAddress,
		pTexture->pSvt->mReadbackBufferSize,
		pTexture->pSvt->mVirtualPageTotalCount,
		currentImage);

	const uint removePageCount = *offsets.pRemovePageCount;
	const uint32_t* RemovePageTable = offsets.pRemovePages;

	const VkVTPendingPageDeletion pendingDeletion = vtGetPendingPageDeletion(pTexture->pSvt, currentImage);

	// Release pending intermediate buffers
	{
		for (size_t i = 0; i < *pendingDeletion.pIntermediateBuffersCount; i++)
			removeBuffer(pRenderer, pendingDeletion.pIntermediateBuffers[i]);

		for (size_t i = 0; i < *pendingDeletion.pAllocationsCount; i++)
			vmaFreeMemory(pRenderer->mVulkan.pVmaAllocator, pendingDeletion.pAllocations[i]);

		*pendingDeletion.pIntermediateBuffersCount = 0;
		*pendingDeletion.pAllocationsCount = 0;
	}

	// Schedule release of newly unneeded pages
	uint pageUnbindCount = 0;
	for (uint removePageIndex = 0; removePageIndex < (int)removePageCount; ++removePageIndex)
	{
		uint32_t RemoveIndex = RemovePageTable[removePageIndex];
		VirtualTexturePage& removePage = pPageTable[RemoveIndex];

		// Never remove the lowest mip level
		if ((int)removePage.mipLevel >= (pTexture->pSvt->mTiledMipLevelCount - 1))
			continue;

		ASSERT(!!removePage.mVulkan.pAllocation == !!removePage.mVulkan.imageMemoryBind.memory);
		if (removePage.mVulkan.pAllocation)
		{
			ASSERT(((VmaAllocation)removePage.mVulkan.pAllocation)->GetMemory() == removePage.mVulkan.imageMemoryBind.memory);
			pendingDeletion.pAllocations[(*pendingDeletion.pAllocationsCount)++] = (VmaAllocation)removePage.mVulkan.pAllocation;
			removePage.mVulkan.pAllocation = VK_NULL_HANDLE;
			removePage.mVulkan.imageMemoryBind.memory = VK_NULL_HANDLE;

			VkSparseImageMemoryBind& unbind = pTexture->pSvt->mVulkan.pSparseImageMemoryBinds[pageUnbindCount++];
			unbind = {};
			unbind.offset = removePage.mVulkan.imageMemoryBind.offset;
			unbind.extent = removePage.mVulkan.imageMemoryBind.extent;
			unbind.subresource = removePage.mVulkan.imageMemoryBind.subresource;

			--pTexture->pSvt->mVirtualPageAliveCount;
		}
	}

	// Unmap tiles
	vk_updateVirtualTextureMemory(pCmd, pTexture, pageUnbindCount);
}

void vk_uploadVirtualTexturePage(Cmd* pCmd, Texture* pTexture, VirtualTexturePage* pPage, uint32_t* imageMemoryCount, uint32_t currentImage)
{
	Buffer* pIntermediateBuffer = NULL;
	if (allocateVirtualPage(pCmd->pRenderer, pTexture, *pPage, &pIntermediateBuffer))
	{
		void* pData = (void*)((unsigned char*)pTexture->pSvt->pVirtualImageData + (pPage->index * pPage->mVulkan.size));

		const bool intermediateMap = !pIntermediateBuffer->pCpuMappedAddress;
		if (intermediateMap)
		{
			mapBuffer(pCmd->pRenderer, pIntermediateBuffer, NULL);
		}

		//CPU to GPU
		memcpy(pIntermediateBuffer->pCpuMappedAddress, pData, pPage->mVulkan.size);

		if (intermediateMap)
		{
			unmapBuffer(pCmd->pRenderer, pIntermediateBuffer);
		}

		//Copy image to VkImage
		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.mipLevel = pPage->mipLevel;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = pPage->mVulkan.imageMemoryBind.offset;
		region.imageOffset.z = 0;
		region.imageExtent = { (uint32_t)pTexture->pSvt->mSparseVirtualTexturePageWidth, (uint32_t)pTexture->pSvt->mSparseVirtualTexturePageHeight, 1 };

		vkCmdCopyBufferToImage(
			pCmd->mVulkan.pVkCmdBuf,
			pIntermediateBuffer->mVulkan.pVkBuffer,
			pTexture->mVulkan.pVkImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region);

		// Update list of memory-backed sparse image memory binds
		pTexture->pSvt->mVulkan.pSparseImageMemoryBinds[(*imageMemoryCount)++] = pPage->mVulkan.imageMemoryBind;

		// Schedule deletion of this intermediate buffer
		const VkVTPendingPageDeletion pendingDeletion = vtGetPendingPageDeletion(pTexture->pSvt, currentImage);
		pendingDeletion.pIntermediateBuffers[(*pendingDeletion.pIntermediateBuffersCount)++] = pIntermediateBuffer;
	}
}

// Fill a complete mip level
// Need to get visibility info first then fill them
void vk_fillVirtualTexture(Cmd* pCmd, Texture* pTexture, Fence* pFence, uint32_t currentImage)
{
	uint32_t imageMemoryCount = 0;

	VTReadbackBufOffsets readbackOffsets = vtGetReadbackBufOffsets(
		(uint*)pTexture->pSvt->pReadbackBuffer->pCpuMappedAddress,
		pTexture->pSvt->mReadbackBufferSize,
		pTexture->pSvt->mVirtualPageTotalCount,
		currentImage);

	const uint alivePageCount = *readbackOffsets.pAlivePageCount;
	uint32_t* VisibilityData = readbackOffsets.pAlivePages;

	for (int i = 0; i < (int)alivePageCount; ++i)
	{
		uint pageIndex = VisibilityData[i];
		ASSERT(pageIndex < pTexture->pSvt->mVirtualPageTotalCount);
		VirtualTexturePage* pPage = &pTexture->pSvt->pPages[pageIndex];
		ASSERT(pageIndex == pPage->index);

		vk_uploadVirtualTexturePage(pCmd, pTexture, pPage, &imageMemoryCount, currentImage);
	}

	vk_updateVirtualTextureMemory(pCmd, pTexture, imageMemoryCount);
}

// Fill specific mipLevel
void vk_fillVirtualTextureLevel(Cmd* pCmd, Texture* pTexture, uint32_t mipLevel, uint32_t currentImage)
{
	//Bind data
	uint32_t imageMemoryCount = 0;

	for (int i = 0; i < (int)pTexture->pSvt->mVirtualPageTotalCount; i++)
	{
		VirtualTexturePage* pPage = &pTexture->pSvt->pPages[i];
		ASSERT(pPage->index == i);

		if (pPage->mipLevel == mipLevel)
		{
			vk_uploadVirtualTexturePage(pCmd, pTexture, pPage, &imageMemoryCount, currentImage);
		}
	}

	vk_updateVirtualTextureMemory(pCmd, pTexture, imageMemoryCount);
}

void vk_addVirtualTexture(Cmd* pCmd, const TextureDesc* pDesc, Texture** ppTexture, void* pImageData)
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

	pTexture->pSvt->pVirtualImageData = pImageData;
	pTexture->mFormat = pDesc->mFormat;
	ASSERT(pTexture->mFormat == pDesc->mFormat);

	VkFormat format = (VkFormat)TinyImageFormat_ToVkFormat((TinyImageFormat)pTexture->mFormat);
	pTexture->mOwnsImage = true;

	VkImageCreateInfo add_info = {};
	add_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	add_info.flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
	add_info.imageType = VK_IMAGE_TYPE_2D;
	add_info.format = format;
	add_info.extent.width = pDesc->mWidth;
	add_info.extent.height = pDesc->mHeight;
	add_info.extent.depth = pDesc->mDepth;
	add_info.mipLevels = pDesc->mMipLevels;
	add_info.arrayLayers = 1;
	add_info.samples = VK_SAMPLE_COUNT_1_BIT;
	add_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	add_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	add_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	add_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	CHECK_VKRESULT(vkCreateImage(pRenderer->mVulkan.pVkDevice, &add_info, &gVkAllocationCallbacks, &pTexture->mVulkan.pVkImage));

	// Get memory requirements
	VkMemoryRequirements sparseImageMemoryReqs;
	// Sparse image memory requirement counts
	vkGetImageMemoryRequirements(pRenderer->mVulkan.pVkDevice, pTexture->mVulkan.pVkImage, &sparseImageMemoryReqs);

	// Check requested image size against hardware sparse limit
	if (sparseImageMemoryReqs.size > pRenderer->mVulkan.pVkActiveGPUProperties->properties.limits.sparseAddressSpaceSize)
	{
		LOGF(LogLevel::eERROR, "Requested sparse image size exceeds supported sparse address space size!");
		return;
	}

	// Get sparse memory requirements
	// Count
	uint32_t sparseMemoryReqsCount;
	vkGetImageSparseMemoryRequirements(pRenderer->mVulkan.pVkDevice, pTexture->mVulkan.pVkImage, &sparseMemoryReqsCount, NULL);  // Get count
	VkSparseImageMemoryRequirements* sparseMemoryReqs = NULL;

	if (sparseMemoryReqsCount == 0)
	{
		LOGF(LogLevel::eERROR, "No memory requirements for the sparse image!");
		return;
	}
	else
	{
		sparseMemoryReqs = (VkSparseImageMemoryRequirements*)tf_calloc(sparseMemoryReqsCount, sizeof(VkSparseImageMemoryRequirements));
		vkGetImageSparseMemoryRequirements(pRenderer->mVulkan.pVkDevice, pTexture->mVulkan.pVkImage, &sparseMemoryReqsCount, sparseMemoryReqs);  // Get reqs
	}

	ASSERT(sparseMemoryReqsCount == 1 && "Multiple sparse image memory requirements not currently implemented");

	pTexture->pSvt->mSparseVirtualTexturePageWidth = sparseMemoryReqs[0].formatProperties.imageGranularity.width;
	pTexture->pSvt->mSparseVirtualTexturePageHeight = sparseMemoryReqs[0].formatProperties.imageGranularity.height;
	pTexture->pSvt->mVirtualPageTotalCount = imageSize / (uint32_t)(pTexture->pSvt->mSparseVirtualTexturePageWidth * pTexture->pSvt->mSparseVirtualTexturePageHeight);
	pTexture->pSvt->mReadbackBufferSize = vtGetReadbackBufSize(pTexture->pSvt->mVirtualPageTotalCount, 1);
	pTexture->pSvt->mPageVisibilityBufferSize = pTexture->pSvt->mVirtualPageTotalCount * 2 * sizeof(uint);

	uint32_t TiledMiplevel = pDesc->mMipLevels - (uint32_t)log2(min((uint32_t)pTexture->pSvt->mSparseVirtualTexturePageWidth, (uint32_t)pTexture->pSvt->mSparseVirtualTexturePageHeight));
	pTexture->pSvt->mTiledMipLevelCount = (uint8_t)TiledMiplevel;

	LOGF(LogLevel::eINFO, "Sparse image memory requirements: %d", sparseMemoryReqsCount);

	// Get sparse image requirements for the color aspect
	VkSparseImageMemoryRequirements sparseMemoryReq = {};
	bool colorAspectFound = false;
	for (int i = 0; i < (int)sparseMemoryReqsCount; ++i)
	{
		VkSparseImageMemoryRequirements reqs = sparseMemoryReqs[i];

		if (reqs.formatProperties.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
		{
			sparseMemoryReq = reqs;
			colorAspectFound = true;
			break;
		}
	}

	SAFE_FREE(sparseMemoryReqs);
	sparseMemoryReqs = NULL;

	if (!colorAspectFound)
	{
		LOGF(LogLevel::eERROR, "Could not find sparse image memory requirements for color aspect bit!");
		return;
	}

	VkPhysicalDeviceMemoryProperties memProps = {};
	vkGetPhysicalDeviceMemoryProperties(pRenderer->mVulkan.pVkActiveGPU, &memProps);

	// todo:
	// Calculate number of required sparse memory bindings by alignment
	assert((sparseImageMemoryReqs.size % sparseImageMemoryReqs.alignment) == 0);
	pTexture->pSvt->mVulkan.mSparseMemoryTypeBits = sparseImageMemoryReqs.memoryTypeBits;

	// Check if the format has a single mip tail for all layers or one mip tail for each layer
	// The mip tail contains all mip levels > sparseMemoryReq.imageMipTailFirstLod
	bool singleMipTail = sparseMemoryReq.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT;

	pTexture->pSvt->pPages = (VirtualTexturePage*)tf_calloc(pTexture->pSvt->mVirtualPageTotalCount, sizeof(VirtualTexturePage));
	pTexture->pSvt->mVulkan.pSparseImageMemoryBinds = (VkSparseImageMemoryBind*)tf_calloc(pTexture->pSvt->mVirtualPageTotalCount, sizeof(VkSparseImageMemoryBind));

	pTexture->pSvt->mVulkan.pOpaqueMemoryBindAllocations = (void**)tf_calloc(pTexture->pSvt->mVirtualPageTotalCount, sizeof(VmaAllocation));
	pTexture->pSvt->mVulkan.pOpaqueMemoryBinds = (VkSparseMemoryBind*)tf_calloc(pTexture->pSvt->mVirtualPageTotalCount, sizeof(VkSparseMemoryBind));

	VmaPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.memoryTypeIndex = util_get_memory_type(sparseImageMemoryReqs.memoryTypeBits, memProps, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CHECK_VKRESULT(vmaCreatePool(pRenderer->mVulkan.pVmaAllocator, &poolCreateInfo, (VmaPool*)&pTexture->pSvt->mVulkan.pPool));

	uint32_t currentPageIndex = 0;

	// Sparse bindings for each mip level of all layers outside of the mip tail
	for (uint32_t layer = 0; layer < 1; layer++)
	{
		// sparseMemoryReq.imageMipTailFirstLod is the first mip level that's stored inside the mip tail
		for (uint32_t mipLevel = 0; mipLevel < TiledMiplevel; mipLevel++)
		{
			VkExtent3D extent;
			extent.width = max(add_info.extent.width >> mipLevel, 1u);
			extent.height = max(add_info.extent.height >> mipLevel, 1u);
			extent.depth = max(add_info.extent.depth >> mipLevel, 1u);

			VkImageSubresource subResource{};
			subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subResource.mipLevel = mipLevel;
			subResource.arrayLayer = layer;

			// Aligned sizes by image granularity
			VkExtent3D imageGranularity = sparseMemoryReq.formatProperties.imageGranularity;
			VkExtent3D sparseBindCounts = {};
			VkExtent3D lastBlockExtent = {};
			alignedDivision(extent, imageGranularity, &sparseBindCounts);
			lastBlockExtent.width =
				((extent.width % imageGranularity.width) ? extent.width % imageGranularity.width : imageGranularity.width);
			lastBlockExtent.height =
				((extent.height % imageGranularity.height) ? extent.height % imageGranularity.height : imageGranularity.height);
			lastBlockExtent.depth =
				((extent.depth % imageGranularity.depth) ? extent.depth % imageGranularity.depth : imageGranularity.depth);

			// Allocate memory for some blocks
			uint32_t index = 0;
			for (uint32_t z = 0; z < sparseBindCounts.depth; z++)
			{
				for (uint32_t y = 0; y < sparseBindCounts.height; y++)
				{
					for (uint32_t x = 0; x < sparseBindCounts.width; x++)
					{
						// Offset
						VkOffset3D offset;
						offset.x = x * imageGranularity.width;
						offset.y = y * imageGranularity.height;
						offset.z = z * imageGranularity.depth;
						// Size of the page
						VkExtent3D extent;
						extent.width = (x == sparseBindCounts.width - 1) ? lastBlockExtent.width : imageGranularity.width;
						extent.height = (y == sparseBindCounts.height - 1) ? lastBlockExtent.height : imageGranularity.height;
						extent.depth = (z == sparseBindCounts.depth - 1) ? lastBlockExtent.depth : imageGranularity.depth;

						// Add new virtual page
						VirtualTexturePage *newPage = addPage(pRenderer, pTexture, offset, extent, pTexture->pSvt->mSparseVirtualTexturePageWidth * pTexture->pSvt->mSparseVirtualTexturePageHeight * sizeof(uint), mipLevel, layer, currentPageIndex);
						currentPageIndex++;
						newPage->mVulkan.imageMemoryBind.subresource = subResource;

						index++;
					}
				}
			}
		}

		// Check if format has one mip tail per layer
		if ((!singleMipTail) && (sparseMemoryReq.imageMipTailFirstLod < pDesc->mMipLevels))
		{
			// Allocate memory for the mip tail
			VkMemoryRequirements memReqs = {};
			memReqs.size = sparseMemoryReq.imageMipTailSize;
			memReqs.memoryTypeBits = pTexture->pSvt->mVulkan.mSparseMemoryTypeBits;
			memReqs.alignment = memReqs.size;

			VmaAllocationCreateInfo allocCreateInfo = {};
			allocCreateInfo.memoryTypeBits = memReqs.memoryTypeBits;
			allocCreateInfo.pool = (VmaPool)pTexture->pSvt->mVulkan.pPool;

			VmaAllocation allocation;
			VmaAllocationInfo allocationInfo;
			CHECK_VKRESULT(vmaAllocateMemory(pRenderer->mVulkan.pVmaAllocator, &memReqs, &allocCreateInfo, &allocation, &allocationInfo));

			// (Opaque) sparse memory binding
			VkSparseMemoryBind sparseMemoryBind{};
			sparseMemoryBind.resourceOffset = sparseMemoryReq.imageMipTailOffset + layer * sparseMemoryReq.imageMipTailStride;
			sparseMemoryBind.size = sparseMemoryReq.imageMipTailSize;
			sparseMemoryBind.memory = allocation->GetMemory();
			sparseMemoryBind.memoryOffset = allocation->GetOffset();

			pTexture->pSvt->mVulkan.pOpaqueMemoryBindAllocations[pTexture->pSvt->mVulkan.mOpaqueMemoryBindsCount] = allocation;
			pTexture->pSvt->mVulkan.pOpaqueMemoryBinds[pTexture->pSvt->mVulkan.mOpaqueMemoryBindsCount] = sparseMemoryBind;
			pTexture->pSvt->mVulkan.mOpaqueMemoryBindsCount++;
		}
	}    // end layers and mips

	LOGF(LogLevel::eINFO, "Virtual Texture info: Dim %d x %d Pages %d", pDesc->mWidth, pDesc->mHeight, pTexture->pSvt->mVirtualPageTotalCount);

	// Check if format has one mip tail for all layers
	if ((sparseMemoryReq.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) &&
		(sparseMemoryReq.imageMipTailFirstLod < pDesc->mMipLevels))
	{
		// Allocate memory for the mip tail
		VkMemoryRequirements memReqs = {};
		memReqs.size = sparseMemoryReq.imageMipTailSize;
		memReqs.memoryTypeBits = pTexture->pSvt->mVulkan.mSparseMemoryTypeBits;
		memReqs.alignment = memReqs.size;

		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.memoryTypeBits = memReqs.memoryTypeBits;
		allocCreateInfo.pool = (VmaPool)pTexture->pSvt->mVulkan.pPool;

		VmaAllocation allocation;
		VmaAllocationInfo allocationInfo;
		CHECK_VKRESULT(vmaAllocateMemory(pRenderer->mVulkan.pVmaAllocator, &memReqs, &allocCreateInfo, &allocation, &allocationInfo));

		// (Opaque) sparse memory binding
		VkSparseMemoryBind sparseMemoryBind{};
		sparseMemoryBind.resourceOffset = sparseMemoryReq.imageMipTailOffset;
		sparseMemoryBind.size = sparseMemoryReq.imageMipTailSize;
		sparseMemoryBind.memory = allocation->GetMemory();
		sparseMemoryBind.memoryOffset = allocation->GetOffset();

		pTexture->pSvt->mVulkan.pOpaqueMemoryBindAllocations[pTexture->pSvt->mVulkan.mOpaqueMemoryBindsCount] = allocation;
		pTexture->pSvt->mVulkan.pOpaqueMemoryBinds[pTexture->pSvt->mVulkan.mOpaqueMemoryBindsCount] = sparseMemoryBind;
		pTexture->pSvt->mVulkan.mOpaqueMemoryBindsCount++;
	}

	/************************************************************************/
	// Create image view
	/************************************************************************/
	VkImageViewCreateInfo view = {};
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = format;
	view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view.subresourceRange.baseMipLevel = 0;
	view.subresourceRange.baseArrayLayer = 0;
	view.subresourceRange.layerCount = 1;
	view.subresourceRange.levelCount = pDesc->mMipLevels;
	view.image = pTexture->mVulkan.pVkImage;
	pTexture->mAspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	CHECK_VKRESULT(vkCreateImageView(pRenderer->mVulkan.pVkDevice, &view, &gVkAllocationCallbacks, &pTexture->mVulkan.pVkSRVDescriptor));

	TextureBarrier textureBarriers[] = { { pTexture, RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_COPY_DEST } };
	cmdResourceBarrier(pCmd, 0, NULL, 1, textureBarriers, 0, NULL);

	// Fill smallest (non-tail) mip map level
	vk_fillVirtualTextureLevel(pCmd, pTexture, TiledMiplevel - 1, 0);

	pTexture->mOwnsImage = true;
	pTexture->mNodeIndex = pDesc->mNodeIndex;
	pTexture->mMipLevels = pDesc->mMipLevels;
	pTexture->mWidth = pDesc->mWidth;
	pTexture->mHeight = pDesc->mHeight;
	pTexture->mDepth = pDesc->mDepth;

	*ppTexture = pTexture;
}

void vk_removeVirtualTexture(Renderer* pRenderer, VirtualTexture* pSvt)
{
	for (int i = 0; i < (int)pSvt->mVirtualPageTotalCount; i++)
	{
		VirtualTexturePage& page = pSvt->pPages[i];
		if (page.mVulkan.pAllocation)
			vmaFreeMemory(pRenderer->mVulkan.pVmaAllocator, (VmaAllocation)page.mVulkan.pAllocation);
	}
	tf_free(pSvt->pPages);

	for (int i = 0; i < (int)pSvt->mVulkan.mOpaqueMemoryBindsCount; i++)
	{
		vmaFreeMemory(pRenderer->mVulkan.pVmaAllocator, (VmaAllocation)pSvt->mVulkan.pOpaqueMemoryBindAllocations[i]);
	}
	tf_free(pSvt->mVulkan.pOpaqueMemoryBinds);
	tf_free(pSvt->mVulkan.pOpaqueMemoryBindAllocations);
	tf_free(pSvt->mVulkan.pSparseImageMemoryBinds);

	for (uint32_t deletionIndex = 0; deletionIndex < pSvt->mPendingDeletionCount; deletionIndex++)
	{
		const VkVTPendingPageDeletion pendingDeletion = vtGetPendingPageDeletion(pSvt, deletionIndex);

		for (uint32_t i = 0; i < *pendingDeletion.pAllocationsCount; i++)
			vmaFreeMemory(pRenderer->mVulkan.pVmaAllocator, pendingDeletion.pAllocations[i]);
		for (uint32_t i = 0; i < *pendingDeletion.pIntermediateBuffersCount; i++)
			removeBuffer(pRenderer, pendingDeletion.pIntermediateBuffers[i]);
	}
	tf_free(pSvt->mVulkan.pPendingDeletedAllocations);
	tf_free(pSvt->pPendingDeletedAllocationsCount);
	tf_free(pSvt->pPendingDeletedBuffers);
	tf_free(pSvt->pPendingDeletedBuffersCount);

	tf_free(pSvt->pVirtualImageData);

	vmaDestroyPool(pRenderer->mVulkan.pVmaAllocator, (VmaPool)pSvt->mVulkan.pPool);
}

void vk_cmdUpdateVirtualTexture(Cmd* cmd, Texture* pTexture, uint32_t currentImage)
{
	ASSERT(pTexture->pSvt->pReadbackBuffer);

	const bool map = !pTexture->pSvt->pReadbackBuffer->pCpuMappedAddress;
	if (map)
	{
		mapBuffer(cmd->pRenderer, pTexture->pSvt->pReadbackBuffer, NULL);
	}

	vk_releasePage(cmd, pTexture, currentImage);
	vk_fillVirtualTexture(cmd, pTexture, NULL, currentImage);

	if (map)
	{
		unmapBuffer(cmd->pRenderer, pTexture->pSvt->pReadbackBuffer);
	}
}

#endif

void initVulkanRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer)
{
	// API functions
	addFence = vk_addFence;
	removeFence = vk_removeFence;
	addSemaphore = vk_addSemaphore;
	removeSemaphore = vk_removeSemaphore;
	addQueue = vk_addQueue;
	removeQueue = vk_removeQueue;
	addSwapChain = vk_addSwapChain;
	removeSwapChain = vk_removeSwapChain;

	// command pool functions
	addCmdPool = vk_addCmdPool;
	removeCmdPool = vk_removeCmdPool;
	addCmd = vk_addCmd;
	removeCmd = vk_removeCmd;
	addCmd_n = vk_addCmd_n;
	removeCmd_n = vk_removeCmd_n;

	addRenderTarget = vk_addRenderTarget;
	removeRenderTarget = vk_removeRenderTarget;
	addSampler = vk_addSampler;
	removeSampler = vk_removeSampler;

	// Resource Load functions
	addBuffer = vk_addBuffer;
	removeBuffer = vk_removeBuffer;
	mapBuffer = vk_mapBuffer;
	unmapBuffer = vk_unmapBuffer;
	cmdUpdateBuffer = vk_cmdUpdateBuffer;
	cmdUpdateSubresource = vk_cmdUpdateSubresource;
	addTexture = vk_addTexture;
	removeTexture = vk_removeTexture;
	addVirtualTexture = vk_addVirtualTexture;
	removeVirtualTexture = vk_removeVirtualTexture;

	// shader functions
	addShaderBinary = vk_addShaderBinary;
	removeShader = vk_removeShader;

	addRootSignature = vk_addRootSignature;
	removeRootSignature = vk_removeRootSignature;

	// pipeline functions
	addPipeline = vk_addPipeline;
	removePipeline = vk_removePipeline;
	addPipelineCache = vk_addPipelineCache;
	getPipelineCacheData = vk_getPipelineCacheData;
	removePipelineCache = vk_removePipelineCache;

	// Descriptor Set functions
	addDescriptorSet = vk_addDescriptorSet;
	removeDescriptorSet = vk_removeDescriptorSet;
	updateDescriptorSet = vk_updateDescriptorSet;

	// command buffer functions
	resetCmdPool = vk_resetCmdPool;
	beginCmd = vk_beginCmd;
	endCmd = vk_endCmd;
	cmdBindRenderTargets = vk_cmdBindRenderTargets;
	cmdSetShadingRate = vk_cmdSetShadingRate;
	cmdSetViewport = vk_cmdSetViewport;
	cmdSetScissor = vk_cmdSetScissor;
	cmdSetStencilReferenceValue = vk_cmdSetStencilReferenceValue;
	cmdBindPipeline = vk_cmdBindPipeline;
	cmdBindDescriptorSet = vk_cmdBindDescriptorSet;
	cmdBindPushConstants = vk_cmdBindPushConstants;
	cmdBindPushConstantsByIndex = vk_cmdBindPushConstantsByIndex;
	cmdBindIndexBuffer = vk_cmdBindIndexBuffer;
	cmdBindVertexBuffer = vk_cmdBindVertexBuffer;
	cmdDraw = vk_cmdDraw;
	cmdDrawInstanced = vk_cmdDrawInstanced;
	cmdDrawIndexed = vk_cmdDrawIndexed;
	cmdDrawIndexedInstanced = vk_cmdDrawIndexedInstanced;
	cmdDispatch = vk_cmdDispatch;

	// Transition Commands
	cmdResourceBarrier = vk_cmdResourceBarrier;
	// Virtual Textures
	cmdUpdateVirtualTexture = vk_cmdUpdateVirtualTexture;

	// queue/fence/swapchain functions
	acquireNextImage = vk_acquireNextImage;
	queueSubmit = vk_queueSubmit;
	queuePresent = vk_queuePresent;
	waitQueueIdle = vk_waitQueueIdle;
	getFenceStatus = vk_getFenceStatus;
	waitForFences = vk_waitForFences;
	toggleVSync = vk_toggleVSync;

	getRecommendedSwapchainFormat = vk_getRecommendedSwapchainFormat;

	//indirect Draw functions
	addIndirectCommandSignature = vk_addIndirectCommandSignature;
	removeIndirectCommandSignature = vk_removeIndirectCommandSignature;
	cmdExecuteIndirect = vk_cmdExecuteIndirect;

	/************************************************************************/
	// GPU Query Interface
	/************************************************************************/
	getTimestampFrequency = vk_getTimestampFrequency;
	addQueryPool = vk_addQueryPool;
	removeQueryPool = vk_removeQueryPool;
	cmdResetQueryPool = vk_cmdResetQueryPool;
	cmdBeginQuery = vk_cmdBeginQuery;
	cmdEndQuery = vk_cmdEndQuery;
	cmdResolveQuery = vk_cmdResolveQuery;
	/************************************************************************/
	// Stats Info Interface
	/************************************************************************/
	calculateMemoryStats = vk_calculateMemoryStats;
	calculateMemoryUse = vk_calculateMemoryUse;
	freeMemoryStats = vk_freeMemoryStats;
	/************************************************************************/
	// Debug Marker Interface
	/************************************************************************/
	cmdBeginDebugMarker = vk_cmdBeginDebugMarker;
	cmdEndDebugMarker = vk_cmdEndDebugMarker;
	cmdAddDebugMarker = vk_cmdAddDebugMarker;
	cmdWriteMarker = vk_cmdWriteMarker;
	/************************************************************************/
	// Resource Debug Naming Interface
	/************************************************************************/
	setBufferName = vk_setBufferName;
	setTextureName = vk_setTextureName;
	setRenderTargetName = vk_setRenderTargetName;
	setPipelineName = vk_setPipelineName;

	vk_initRenderer(appName, pSettings, ppRenderer);
}

void exitVulkanRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);

	vk_exitRenderer(pRenderer);
}

#if !defined(NX64)
#include "../../../Common_3/ThirdParty/OpenSource/volk/volk.c"
#if defined(VK_USE_DISPATCH_TABLES)
#include "../../../Common_3/ThirdParty/OpenSource/volk/volkForgeExt.c"
#endif
#endif
#endif

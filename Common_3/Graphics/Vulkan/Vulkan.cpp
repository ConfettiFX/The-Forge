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

#ifdef VULKAN

#define RENDERER_IMPLEMENTATION
#define VMA_IMPLEMENTATION

/************************************************************************/
/************************************************************************/
#if defined(_WINDOWS)
#include <Windows.h>
#endif

#if defined(__linux__)
#define stricmp(a, b) strcasecmp(a, b)
#define vsprintf_s vsnprintf
#define strncpy_s strncpy
#endif

#if defined(NX64)
#include "../../../Switch/Common_3/Graphics/Vulkan/NXVulkan.h"
#include "../GPUConfig.h"
#endif

#include "../Interfaces/IGraphics.h"

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#include "../../Utilities/Interfaces/ILog.h"

#include "../../Utilities/Math/MathTypes.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wswitch"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wswitch"
#endif

#if defined(NX64)
// #NOTE: NX64 VK header does not have this type
typedef struct VkBaseInStructure
{
	VkStructureType                    sType;
	const struct VkBaseInStructure*    pNext;
} VkBaseInStructure;

typedef struct VkBaseOutStructure
{
	VkStructureType               sType;
	struct VkBaseOutStructure*    pNext;
} VkBaseOutStructure;
#endif

#include "../ThirdParty/OpenSource/VulkanMemoryAllocator/VulkanMemoryAllocator.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "../../Utilities/Threading/Atomics.h"
#include "../GPUConfig.h"
#include "../../Utilities/Math/AlgorithmsImpl.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "VulkanCapsBuilder.h"

#if defined(VK_USE_DISPATCH_TABLES) && !defined(NX64)
#include "../../../Common_3/Graphics/ThirdParty/OpenSource/volk/volkForgeExt.h"
#endif

#if defined(QUEST_VR)
#include "../../OS/Quest/VrApi.h"
#include "../Quest/VrApiHooks.h"

extern RenderTarget* pFragmentDensityMask;
#endif

#include "../../Utilities/Interfaces/IMemory.h"


extern void
	vk_createShaderReflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection);

#ifdef VK_RAYTRACING_AVAILABLE
extern void vk_addRaytracingPipeline(const PipelineDesc*, Pipeline**);
extern void vk_FillRaytracingDescriptorData(AccelerationStructure* ppAccelerationStructures, VkAccelerationStructureKHR* pOutHandle);
#endif

#define VENDOR_ID_NVIDIA 0x10DE
#define VENDOR_ID_AMD 0x1002
#define VENDOR_ID_AMD_1 0x1022
#define VENDOR_ID_INTEL 0x163C
#define VENDOR_ID_INTEL_1 0x8086
#define VENDOR_ID_INTEL_2 0x8087

typedef enum GpuVendor
{
	GPU_VENDOR_NVIDIA,
	GPU_VENDOR_AMD,
	GPU_VENDOR_INTEL,
	GPU_VENDOR_UNKNOWN,
	GPU_VENDOR_COUNT,
} GpuVendor;

#define VK_FORMAT_VERSION( version, outVersionString )                     \
	ASSERT( VK_MAX_DESCRIPTION_SIZE == TF_ARRAY_COUNT( outVersionString ) ); \
	sprintf( outVersionString, "%u.%u.%u", VK_VERSION_MAJOR( version ), VK_VERSION_MINOR( version ), VK_VERSION_PATCH( version ) );

static GpuVendor util_to_internal_gpu_vendor( uint32_t vendorId )
{
	if ( vendorId == VENDOR_ID_NVIDIA )
		return GPU_VENDOR_NVIDIA;
	else if ( vendorId == VENDOR_ID_AMD || vendorId == VENDOR_ID_AMD_1 )
		return GPU_VENDOR_AMD;
	else if ( vendorId == VENDOR_ID_INTEL || vendorId == VENDOR_ID_INTEL_1 || vendorId == VENDOR_ID_INTEL_2 )
		return GPU_VENDOR_INTEL;
	else
		return GPU_VENDOR_UNKNOWN;
}

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

VkAttachmentStoreOp gVkAttachmentStoreOpTranslator[StoreActionType::MAX_STORE_ACTION] =
{
	VK_ATTACHMENT_STORE_OP_STORE,
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
	// Dont care is treated as store op none in most drivers
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
	// Resolve + Store = Store Resolve attachment + Store MSAA attachment
	VK_ATTACHMENT_STORE_OP_STORE,
	// Resolve + Dont Care = Store Resolve attachment + Dont Care MSAA attachment
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
#endif
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
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
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
#ifndef ENABLE_DEBUG_UTILS_EXTENSION
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
	// Raytracing
	/************************************************************************/
#ifdef VK_RAYTRACING_AVAILABLE
	VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
	VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
	VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, 

	VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
	VK_KHR_SPIRV_1_4_EXTENSION_NAME,
	VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,

	VK_KHR_RAY_QUERY_EXTENSION_NAME,
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
	VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
	VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
#endif
#if VK_KHR_image_format_list
	VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME ,
#endif
	/************************************************************************/
	// Multiview support
	/************************************************************************/
#ifdef QUEST_VR
	VK_KHR_MULTIVIEW_EXTENSION_NAME,
#endif
    /************************************************************************/
	// Nsight Aftermath
	/************************************************************************/
#ifdef ENABLE_NSIGHT_AFTERMATH
	VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME,
	VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME,
#endif
	/************************************************************************/
	/************************************************************************/
};
// clang-format on

#ifdef ENABLE_DEBUG_UTILS_EXTENSION
static bool gDebugUtilsExtension = false;
#endif
static bool gRenderDocLayerEnabled = false;
static bool gDeviceGroupCreationExtension = false;

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

VkAllocationCallbacks gVkAllocationCallbacks =
{
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

#if VK_KHR_draw_indirect_count
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
DECLARE_RENDERER_FUNCTION(
	void, cmdCopySubresource, Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_RENDERER_FUNCTION(void, addTexture, Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
DECLARE_RENDERER_FUNCTION(void, removeTexture, Renderer* pRenderer, Texture* pTexture)
DECLARE_RENDERER_FUNCTION(void, addVirtualTexture, Cmd* pCmd, const TextureDesc* pDesc, Texture** ppTexture, void* pImageData)
DECLARE_RENDERER_FUNCTION(void, removeVirtualTexture, Renderer* pRenderer, VirtualTexture* pTexture)
/************************************************************************/
// Descriptor Pool Functions
/************************************************************************/
static void add_descriptor_pool(
	Renderer* pRenderer, uint32_t numDescriptorSets, VkDescriptorPoolCreateFlags flags,
	const VkDescriptorPoolSize* pPoolSizes, uint32_t numPoolSizes,
	VkDescriptorPool* pPool)
{
	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.pNext = NULL;
	poolCreateInfo.poolSizeCount = numPoolSizes;
	poolCreateInfo.pPoolSizes = pPoolSizes;
	poolCreateInfo.flags = flags;
	poolCreateInfo.maxSets = numDescriptorSets;

	CHECK_VKRESULT(vkCreateDescriptorPool(pRenderer->mVulkan.pVkDevice, &poolCreateInfo, &gVkAllocationCallbacks, pPool));
}

static void consume_descriptor_sets(VkDevice pDevice, VkDescriptorPool pPool, const VkDescriptorSetLayout* pLayouts, uint32_t numDescriptorSets, VkDescriptorSet** pSets)
{
	DECLARE_ZERO(VkDescriptorSetAllocateInfo, alloc_info);
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.descriptorPool = pPool;
	alloc_info.descriptorSetCount = numDescriptorSets;
	alloc_info.pSetLayouts = pLayouts;
	CHECK_VKRESULT(vkAllocateDescriptorSets(pDevice, &alloc_info, *pSets));
}

/************************************************************************/
/************************************************************************/
VkPipelineBindPoint gPipelineBindPoint[PIPELINE_TYPE_COUNT] = { VK_PIPELINE_BIND_POINT_MAX_ENUM, VK_PIPELINE_BIND_POINT_COMPUTE,
																VK_PIPELINE_BIND_POINT_GRAPHICS,
#ifdef VK_RAYTRACING_AVAILABLE
																VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR
#endif
};

struct DynamicUniformData
{
	VkBuffer pBuffer;
	uint32_t mOffset;
	uint32_t mSize;
};
/************************************************************************/
// Descriptor Set Structure
/************************************************************************/
typedef struct DescriptorIndexMap
{
	char*		key;
	uint32_t	value;
} DescriptorIndexMap;

static const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName)
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
// Render Pass Implementation
/************************************************************************/
static const LoadActionType gDefaultLoadActions[MAX_RENDER_TARGET_ATTACHMENTS] = {};
static const StoreActionType gDefaultStoreActions[MAX_RENDER_TARGET_ATTACHMENTS] = {};

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
static inline FORGE_CONSTEXPR bool IsStoreActionResolve(StoreActionType action)
{
	return STORE_ACTION_RESOLVE_DONTCARE == action || STORE_ACTION_RESOLVE_STORE == action;
}
#endif

typedef struct RenderPassDesc
{
	TinyImageFormat*       pColorFormats;
	const LoadActionType*  pLoadActionsColor;
	const StoreActionType* pStoreActionsColor;
	bool*                  pSrgbValues;
	uint32_t               mRenderTargetCount;
	SampleCount            mSampleCount;
	TinyImageFormat        mDepthStencilFormat;
	LoadActionType         mLoadActionDepth;
	LoadActionType         mLoadActionStencil;
	StoreActionType        mStoreActionDepth;
	StoreActionType        mStoreActionStencil;
    bool                   mVRMultiview;
    bool                   mVRFoveatedRendering;
} RenderPassDesc;

typedef struct RenderPass
{
	VkRenderPass   pRenderPass;
	RenderPassDesc mDesc;
} RenderPass;

typedef struct FrameBufferDesc
{
	RenderPass*      pRenderPass;
	RenderTarget**   ppRenderTargets;
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
	StoreActionType* pRenderTargetResolveActions;
#endif
	RenderTarget*    pDepthStencil;
	uint32_t*        pColorArraySlices;
	uint32_t*        pColorMipSlices;
	uint32_t         mDepthArraySlice;
	uint32_t         mDepthMipSlice;
	uint32_t         mRenderTargetCount;
	bool             mVRFoveatedRendering;
} FrameBufferDesc;

typedef struct FrameBuffer
{
	VkFramebuffer pFramebuffer;
	uint32_t      mWidth;
	uint32_t      mHeight;
	uint32_t      mArraySize;
} FrameBuffer;

#define VK_MAX_ATTACHMENT_ARRAY_COUNT ((MAX_RENDER_TARGET_ATTACHMENTS + 2) * 2)

static void add_render_pass(Renderer* pRenderer, const RenderPassDesc* pDesc, RenderPass* pRenderPass)
{
	ASSERT(pRenderPass);
	*pRenderPass = {};
	pRenderPass->mDesc = *pDesc;
	/************************************************************************/
	// Add render pass
	/************************************************************************/
	bool hasDepthAttachmentCount = (pDesc->mDepthStencilFormat != TinyImageFormat_UNDEFINED);

	VkAttachmentDescription attachments[VK_MAX_ATTACHMENT_ARRAY_COUNT] = {};
	VkAttachmentReference   colorAttachmentRefs[MAX_RENDER_TARGET_ATTACHMENTS] = {};
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
	VkAttachmentReference   resolveAttachmentRefs[MAX_RENDER_TARGET_ATTACHMENTS] = {};
#endif
	VkAttachmentReference   dsAttachmentRef = {};
	uint32_t attachmentCount = 0;

	VkSampleCountFlagBits sampleCount = util_to_vk_sample_count(pDesc->mSampleCount);

	// Fill out attachment descriptions and references
	{
		// Color
		for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i, ++attachmentCount)
		{
			// descriptions
			VkAttachmentDescription* attachment = &attachments[attachmentCount];
			attachment->flags = 0;
			attachment->format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->pColorFormats[i]);
			attachment->samples = sampleCount;
			attachment->loadOp = gVkAttachmentLoadOpTranslator[pDesc->pLoadActionsColor[i]];
			attachment->storeOp = gVkAttachmentStoreOpTranslator[pDesc->pStoreActionsColor[i]];
			attachment->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachment->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachment->initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachment->finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			// references
			VkAttachmentReference* attachmentRef = &colorAttachmentRefs[i];
			attachmentRef->attachment = attachmentCount;    //-V522
			attachmentRef->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
			VkAttachmentReference* resolveAttachmentRef = &resolveAttachmentRefs[i];
			*resolveAttachmentRef = *attachmentRef;

			if (IsStoreActionResolve(pDesc->pStoreActionsColor[i]))
			{
				++attachmentCount;
				VkAttachmentDescription* resolveAttachment = &attachments[attachmentCount];
				*resolveAttachment = *attachment;
				resolveAttachment->samples = VK_SAMPLE_COUNT_1_BIT;
				resolveAttachment->loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				resolveAttachment->storeOp = VK_ATTACHMENT_STORE_OP_STORE;

				resolveAttachmentRef->attachment = attachmentCount;
			}
			else
			{
				resolveAttachmentRef->attachment = VK_ATTACHMENT_UNUSED;
			}
#endif
		}
	}

	// Depth stencil
	if (hasDepthAttachmentCount)
	{
		uint32_t idx = attachmentCount++;
		attachments[idx].flags = 0;
		attachments[idx].format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mDepthStencilFormat);
		attachments[idx].samples = sampleCount;
		attachments[idx].loadOp = gVkAttachmentLoadOpTranslator[pDesc->mLoadActionDepth];
		attachments[idx].storeOp = gVkAttachmentStoreOpTranslator[pDesc->mStoreActionDepth];
		attachments[idx].stencilLoadOp = gVkAttachmentLoadOpTranslator[pDesc->mLoadActionStencil];
		attachments[idx].stencilStoreOp = gVkAttachmentStoreOpTranslator[pDesc->mStoreActionStencil];
		attachments[idx].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[idx].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		dsAttachmentRef.attachment = idx;    //-V522
		dsAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	void* renderPassNext = NULL;
#if defined(QUEST_VR)
	DECLARE_ZERO(VkRenderPassFragmentDensityMapCreateInfoEXT, fragDensityCreateInfo);
	if (pDesc->mVRFoveatedRendering && pQuest->mFoveatedRenderingEnabled)
	{
		uint32_t idx = attachmentCount++;
		attachments[idx].flags = 0;
		attachments[idx].format = VK_FORMAT_R8G8_UNORM;
		attachments[idx].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[idx].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[idx].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[idx].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[idx].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[idx].initialLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
		attachments[idx].finalLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;

		fragDensityCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT;
		fragDensityCreateInfo.fragmentDensityMapAttachment.attachment = idx;
		fragDensityCreateInfo.fragmentDensityMapAttachment.layout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;

		renderPassNext = &fragDensityCreateInfo;
	}
#endif

	DECLARE_ZERO(VkSubpassDescription, subpass);
	subpass.flags = 0;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = NULL;
	subpass.colorAttachmentCount = pDesc->mRenderTargetCount;
	subpass.pColorAttachments = pDesc->mRenderTargetCount ? colorAttachmentRefs : NULL;
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
	subpass.pResolveAttachments = pDesc->mRenderTargetCount ? resolveAttachmentRefs : NULL;
#else
	subpass.pResolveAttachments = NULL;
#endif
	subpass.pDepthStencilAttachment = hasDepthAttachmentCount ? &dsAttachmentRef : NULL;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = NULL;

	DECLARE_ZERO(VkRenderPassCreateInfo, createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.pNext = renderPassNext;
	createInfo.flags = 0;
	createInfo.attachmentCount = attachmentCount;
	createInfo.pAttachments = attachments;
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpass;
	createInfo.dependencyCount = 0;
	createInfo.pDependencies = NULL;

#if defined(QUEST_VR)
	const uint viewMask = 0b11;
	DECLARE_ZERO(VkRenderPassMultiviewCreateInfo, multiviewCreateInfo);

	if (pDesc->mVRMultiview)
	{
		multiviewCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
		multiviewCreateInfo.pNext = createInfo.pNext;
		multiviewCreateInfo.subpassCount = 1;
		multiviewCreateInfo.pViewMasks = &viewMask;
		multiviewCreateInfo.dependencyCount = 0;
		multiviewCreateInfo.correlationMaskCount = 1;
		multiviewCreateInfo.pCorrelationMasks = &viewMask;

		createInfo.pNext = &multiviewCreateInfo;
	}
#endif

	CHECK_VKRESULT(vkCreateRenderPass(pRenderer->mVulkan.pVkDevice, &createInfo, &gVkAllocationCallbacks, &(pRenderPass->pRenderPass)));
}

static void remove_render_pass(Renderer* pRenderer, RenderPass* pRenderPass)
{
	vkDestroyRenderPass(pRenderer->mVulkan.pVkDevice, pRenderPass->pRenderPass, &gVkAllocationCallbacks);
}

static void add_framebuffer(Renderer* pRenderer, const FrameBufferDesc* pDesc, FrameBuffer* pFrameBuffer)
{
	ASSERT(pFrameBuffer);
	*pFrameBuffer = {};

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
		if (pDesc->mDepthArraySlice != UINT32_MAX)
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
	VkImageView imageViews[VK_MAX_ATTACHMENT_ARRAY_COUNT] = {};
	VkImageView* iterViews = imageViews;

	// Color
	for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i)
	{
		if (!pDesc->pColorMipSlices && !pDesc->pColorArraySlices)
		{
			*iterViews = pDesc->ppRenderTargets[i]->mVulkan.pVkDescriptor;
			++iterViews;

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
			if (IsStoreActionResolve(pDesc->pRenderTargetResolveActions[i]))
			{
				*iterViews = pDesc->ppRenderTargets[i]->pResolveAttachment->mVulkan.pVkDescriptor;
				++iterViews;
			}
#endif
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
			*iterViews = pDesc->ppRenderTargets[i]->mVulkan.pVkSliceDescriptors[handle];
			++iterViews;

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
			if (IsStoreActionResolve(pDesc->pRenderTargetResolveActions[i]))
			{
				*iterViews = pDesc->ppRenderTargets[i]->pResolveAttachment->mVulkan.pVkSliceDescriptors[handle];
				++iterViews;
			}
#endif
		}
	}
	// Depth/stencil
	if (pDesc->pDepthStencil)
	{
		if (UINT32_MAX == pDesc->mDepthMipSlice && UINT32_MAX == pDesc->mDepthArraySlice)
		{
			*iterViews = pDesc->pDepthStencil->mVulkan.pVkDescriptor;
			++iterViews;
		}
		else
		{
			uint32_t handle = 0;
			if (pDesc->mDepthMipSlice != UINT32_MAX)
			{
				if (pDesc->mDepthArraySlice != UINT32_MAX)
					handle = pDesc->mDepthMipSlice * pDesc->pDepthStencil->mArraySize + pDesc->mDepthArraySlice;
				else
					handle = pDesc->mDepthMipSlice;
			}
			else if (pDesc->mDepthArraySlice != UINT32_MAX)
			{
				handle = pDesc->mDepthArraySlice;
			}
			*iterViews = pDesc->pDepthStencil->mVulkan.pVkSliceDescriptors[handle];
			++iterViews;
		}
	}

#if defined(QUEST_VR)
	if (pDesc->mVRFoveatedRendering && pQuest->mFoveatedRenderingEnabled)
	{
		*iterViews = pFragmentDensityMask->mVulkan.pVkDescriptor;
		++iterViews;
	}
#endif

	DECLARE_ZERO(VkFramebufferCreateInfo, createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.pNext = NULL;
	createInfo.flags = 0;
	createInfo.renderPass = pDesc->pRenderPass->pRenderPass;
	createInfo.attachmentCount = (uint32_t)(iterViews - imageViews);
	createInfo.pAttachments = imageViews;
	createInfo.width = pFrameBuffer->mWidth;
	createInfo.height = pFrameBuffer->mHeight;
	createInfo.layers = pFrameBuffer->mArraySize;
	CHECK_VKRESULT(vkCreateFramebuffer(pRenderer->mVulkan.pVkDevice, &createInfo, &gVkAllocationCallbacks, &(pFrameBuffer->pFramebuffer)));
	/************************************************************************/
	/************************************************************************/
}

static void remove_framebuffer(Renderer* pRenderer, FrameBuffer* pFrameBuffer)
{
	ASSERT(pRenderer);
	ASSERT(pFrameBuffer);

	vkDestroyFramebuffer(pRenderer->mVulkan.pVkDevice, pFrameBuffer->pFramebuffer, &gVkAllocationCallbacks);
}
/************************************************************************/
// Per Thread Render Pass synchronization logic
/************************************************************************/
/// Render-passes are not exposed to the app code since they are not available on all apis
/// This map takes care of hashing a render pass based on the render targets passed to cmdBeginRender
typedef struct RenderPassNode
{
	uint64_t   key;
	RenderPass value;
}RenderPassNode;

typedef struct FrameBufferNode
{
	uint64_t    key;
	FrameBuffer value;
}FrameBufferNode;

typedef struct ThreadRenderPassNode
{
	ThreadID			key;
	RenderPassNode**	value; // pointer to stb_ds map
}ThreadRenderPassNode;

typedef struct ThreadFrameBufferNode
{
	ThreadID			key;
	FrameBufferNode**	value; // pointer to stb_ds map
}ThreadFrameBufferNode;

// RenderPass map per thread (this will make lookups lock free and we only need a lock when inserting a RenderPass Map for the first time)
static ThreadRenderPassNode*	gRenderPassMap[MAX_UNLINKED_GPUS] = { NULL };
// FrameBuffer map per thread (this will make lookups lock free and we only need a lock when inserting a FrameBuffer map for the first time)
static ThreadFrameBufferNode*	gFrameBufferMap[MAX_UNLINKED_GPUS] = { NULL };
Mutex							gRenderPassMutex[MAX_UNLINKED_GPUS];

// Returns pointer map
static RenderPassNode** get_render_pass_map(uint32_t rendererID)
{
	// Only need a lock when creating a new renderpass map for this thread
	MutexLock	lock(gRenderPassMutex[rendererID]);
	ThreadRenderPassNode* threadMap = gRenderPassMap[rendererID];
	ThreadID              threadId = getCurrentThreadID();
	ThreadRenderPassNode* pNode = hmgetp_null(threadMap, threadId);
	if (!pNode)
	{
		// We need pointer to map, so that thread map can be reallocated without causing data races
		RenderPassNode** result = (RenderPassNode**)tf_calloc(1, sizeof(RenderPassNode*));
		return hmput(gRenderPassMap[rendererID], threadId, result);
	}
	else
	{
		return pNode->value;
	}
}

// Returns pointer to map
static FrameBufferNode** get_frame_buffer_map(uint32_t rendererID)
{
	// Only need a lock when creating a new framebuffer map for this thread
	MutexLock	lock(gRenderPassMutex[rendererID]);
	ThreadID	threadId = getCurrentThreadID();
	ThreadFrameBufferNode* pNode = hmgetp_null(gFrameBufferMap[rendererID], threadId);
	if (!pNode)
	{
		FrameBufferNode** result = (FrameBufferNode**)tf_calloc(1, sizeof(FrameBufferNode*));
		return hmput(gFrameBufferMap[rendererID], threadId, result);
	}
	else
	{
		return pNode->value;
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

#ifdef ENABLE_DEBUG_UTILS_EXTENSION
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
// gAssertOnVkValidationError is used to work around a bug in the ovr mobile sdk.
// There is a fence creation struct that is not initialized in the sdk.
bool gAssertOnVkValidationError = true;

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
		if (gAssertOnVkValidationError)
		{
			ASSERT(false);
		}
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

	for (uint32_t i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
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

	for (uint32_t i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
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
	initMutex(&pRenderer->pNullDescriptors->mSubmitMutex);

	for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
	{
		uint32_t nodeIndex = pRenderer->mGpuMode == GPU_MODE_UNLINKED ? pRenderer->mUnlinkedRendererIndex : i;
		// 1D texture
		TextureDesc textureDesc = {};
		textureDesc.mNodeIndex = nodeIndex;
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
		textureDesc.mSampleCount = SAMPLE_COUNT_4;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_2DMS]);
		textureDesc.mSampleCount = SAMPLE_COUNT_1;

		// 2D texture array
		textureDesc.mArraySize = 2;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_2D_ARRAY]);
		textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
		addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][TEXTURE_DIM_2D_ARRAY]);

		// 2D MS texture array
		textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		textureDesc.mSampleCount = SAMPLE_COUNT_4;
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
		bufferDesc.mNodeIndex = nodeIndex;
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
		default: ASSERT(false); break;
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
#ifdef VK_RAYTRACING_AVAILABLE
	if (usage & DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE)
	{
		result |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
	}
	if (usage & DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT)
	{
		result |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
	}
	if (usage & DESCRIPTOR_TYPE_SHADER_DEVICE_ADDRESS)
	{
		result |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	}
	if (usage & DESCRIPTOR_TYPE_SHADER_BINDING_TABLE)
	{
		result |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
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

#ifdef VK_RAYTRACING_AVAILABLE
	if (state & RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
	{
		ret |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
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
#ifdef VK_RAYTRACING_AVAILABLE
				if (pRenderer->mVulkan.mRaytracingSupported)
				{
					flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
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
		case DESCRIPTOR_TYPE_UNDEFINED: ASSERT(false && "Invalid DescriptorInfo Type"); return VK_DESCRIPTOR_TYPE_MAX_ENUM;
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
#ifdef VK_RAYTRACING_AVAILABLE
		case DESCRIPTOR_TYPE_RAY_TRACING: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
#endif
		default:
			ASSERT(false && "Invalid DescriptorInfo Type");
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
#ifdef VK_RAYTRACING_AVAILABLE
	if (stages & SHADER_STAGE_RAYTRACING)
		res |=
			(VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
			 VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR);
#endif

	ASSERT(res != 0);
	return res;
}

void util_find_queue_family_index(
	const Renderer* pRenderer, uint32_t nodeIndex, QueueType queueType, VkQueueFamilyProperties* pOutProps, uint8_t* pOutFamilyIndex,
	uint8_t* pOutQueueIndex)
{
	if (pRenderer->mGpuMode != GPU_MODE_LINKED)
		nodeIndex = 0;

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

void util_query_gpu_settings(VkPhysicalDevice gpu, VkPhysicalDeviceProperties2* gpuProperties, VkPhysicalDeviceMemoryProperties* gpuMemoryProperties,
	VkPhysicalDeviceFeatures2KHR* gpuFeatures, VkQueueFamilyProperties** queueFamilyProperties, uint32_t* queueFamilyPropertyCount, GPUSettings* gpuSettings)
{
	*gpuProperties = {};
	*gpuMemoryProperties = {};
	*gpuFeatures = {};
	*queueFamilyProperties = NULL;
	*queueFamilyPropertyCount = 0;

	// Get memory properties
	vkGetPhysicalDeviceMemoryProperties(gpu, gpuMemoryProperties);

	// Get features
	gpuFeatures->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;

#if VK_EXT_fragment_shader_interlock
	VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragmentShaderInterlockFeatures =
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT
	};
	gpuFeatures->pNext = &fragmentShaderInterlockFeatures;
#endif
#ifndef NX64
	vkGetPhysicalDeviceFeatures2KHR(gpu, gpuFeatures);
#else
	vkGetPhysicalDeviceFeatures2(gpu, gpuFeatures);
#endif

	// Get device properties
	VkPhysicalDeviceSubgroupProperties subgroupProperties = {};
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
	subgroupProperties.pNext = NULL;
	gpuProperties->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
	subgroupProperties.pNext = gpuProperties->pNext;
	gpuProperties->pNext = &subgroupProperties;
#if defined(NX64)
	vkGetPhysicalDeviceProperties2(gpu, gpuProperties);
#else
	vkGetPhysicalDeviceProperties2KHR(gpu, gpuProperties);
#endif

	// Get queue family properties
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, queueFamilyPropertyCount, NULL);
	*queueFamilyProperties = (VkQueueFamilyProperties*)tf_calloc(*queueFamilyPropertyCount, sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, queueFamilyPropertyCount, *queueFamilyProperties);

	*gpuSettings = {};
	gpuSettings->mUniformBufferAlignment = (uint32_t)gpuProperties->properties.limits.minUniformBufferOffsetAlignment;
	gpuSettings->mUploadBufferTextureAlignment = (uint32_t)gpuProperties->properties.limits.optimalBufferCopyOffsetAlignment;
	gpuSettings->mUploadBufferTextureRowAlignment = (uint32_t)gpuProperties->properties.limits.optimalBufferCopyRowPitchAlignment;
	gpuSettings->mMaxVertexInputBindings = gpuProperties->properties.limits.maxVertexInputBindings;
	gpuSettings->mMultiDrawIndirect = gpuFeatures->features.multiDrawIndirect;
	gpuSettings->mIndirectRootConstant = false;
	gpuSettings->mBuiltinDrawID = true;

	gpuSettings->mWaveLaneCount = subgroupProperties.subgroupSize;
	gpuSettings->mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_NONE;
	if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT)
		gpuSettings->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_BASIC_BIT;
	if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_VOTE_BIT)
		gpuSettings->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_VOTE_BIT;
	if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT)
		gpuSettings->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_ARITHMETIC_BIT;
	if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT)
		gpuSettings->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_BALLOT_BIT;
	if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT)
		gpuSettings->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_SHUFFLE_BIT;
	if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT)
		gpuSettings->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_SHUFFLE_RELATIVE_BIT;
	if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_CLUSTERED_BIT)
		gpuSettings->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_CLUSTERED_BIT;
	if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_QUAD_BIT)
		gpuSettings->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_QUAD_BIT;
	if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_PARTITIONED_BIT_NV)
		gpuSettings->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_PARTITIONED_BIT_NV;

#if VK_EXT_fragment_shader_interlock
	gpuSettings->mROVsSupported = (bool)fragmentShaderInterlockFeatures.fragmentShaderPixelInterlock;
#endif
	gpuSettings->mTessellationSupported = gpuFeatures->features.tessellationShader;
	gpuSettings->mGeometryShaderSupported = gpuFeatures->features.geometryShader;
	gpuSettings->mSamplerAnisotropySupported = gpuFeatures->features.samplerAnisotropy;

	//save vendor and model Id as string
	sprintf(gpuSettings->mGpuVendorPreset.mModelId, "%#x", gpuProperties->properties.deviceID);
	sprintf(gpuSettings->mGpuVendorPreset.mVendorId, "%#x", gpuProperties->properties.vendorID);
	strncpy(gpuSettings->mGpuVendorPreset.mGpuName, gpuProperties->properties.deviceName, MAX_GPU_VENDOR_STRING_LENGTH);

	//TODO: Fix once vulkan adds support for revision ID
	strncpy(gpuSettings->mGpuVendorPreset.mRevisionId, "0x00", MAX_GPU_VENDOR_STRING_LENGTH);
	gpuSettings->mGpuVendorPreset.mPresetLevel = getGPUPresetLevel(
		gpuSettings->mGpuVendorPreset.mVendorId, gpuSettings->mGpuVendorPreset.mModelId,
		gpuSettings->mGpuVendorPreset.mRevisionId);

	//fill in driver info
    uint32_t major, minor, secondaryBranch, tertiaryBranch;
	switch ( util_to_internal_gpu_vendor( gpuProperties->properties.vendorID ) )
	{
	case GPU_VENDOR_NVIDIA:
        major = (gpuProperties->properties.driverVersion >> 22) & 0x3ff;
        minor = (gpuProperties->properties.driverVersion >> 14) & 0x0ff;
        secondaryBranch = (gpuProperties->properties.driverVersion >> 6) & 0x0ff;
        tertiaryBranch = (gpuProperties->properties.driverVersion) & 0x003f;
        
        sprintf( gpuSettings->mGpuVendorPreset.mGpuDriverVersion, "%u.%u.%u.%u", major,minor,secondaryBranch,tertiaryBranch);
		break;
	default:
		VK_FORMAT_VERSION(gpuProperties->properties.driverVersion, gpuSettings->mGpuVendorPreset.mGpuDriverVersion);
		break;
	}

	gpuFeatures->pNext = NULL;
	gpuProperties->pNext = NULL;
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

#if VK_DEBUG_LOG_EXTENSIONS
	for (uint32_t i = 0; i < layerCount; ++i)
	{
		internal_log(eINFO, layers[i].layerName, "vkinstance-layer");
	}

	for (uint32_t i = 0; i < extCount; ++i)
	{
		internal_log(eINFO, exts[i].extensionName, "vkinstance-ext");
	}
#endif

	DECLARE_ZERO(VkApplicationInfo, app_info);
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pNext = NULL;
	app_info.pApplicationName = app_name;
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "TheForge";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = TARGET_VULKAN_API_VERSION;

	const char** layerTemp = NULL;
	arrsetcap(layerTemp, userDefinedInstanceLayerCount);

	// Instance
	{
		// check to see if the layers are present
		for (uint32_t i = 0; i < userDefinedInstanceLayerCount; ++i)
		{
			bool layerFound = false;
			for (uint32_t j = 0; j < layerCount; ++j)
			{
				if (strcmp(userDefinedInstanceLayers[i], layers[j].layerName) == 0)
				{
					layerFound = true;
					arrpush(layerTemp, userDefinedInstanceLayers[i]);
					break;
				}
			}
			if (layerFound == false)
			{
				internal_log(eWARNING, userDefinedInstanceLayers[i], "vkinstance-layer-missing");
			}
		}

		uint32_t                   extension_count = 0;
		const uint32_t             initialCount = sizeof(gVkWantedInstanceExtensions) / sizeof(gVkWantedInstanceExtensions[0]);
		const uint32_t             userRequestedCount = (uint32_t)pDesc->mVulkan.mInstanceExtensionCount;
		const char** wantedInstanceExtensions = NULL;
		arrsetlen(wantedInstanceExtensions, initialCount + userRequestedCount);
		for (uint32_t i = 0; i < initialCount; ++i)
		{
			wantedInstanceExtensions[i] = gVkWantedInstanceExtensions[i];
		}
		for (uint32_t i = 0; i < userRequestedCount; ++i)
		{
			wantedInstanceExtensions[initialCount + i] = pDesc->mVulkan.ppInstanceExtensions[i];
		}
		const uint32_t wanted_extension_count = (uint32_t)arrlen(wantedInstanceExtensions);
		// Layer extensions
		for (ptrdiff_t i = 0; i < arrlen(layerTemp); ++i)
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
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
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
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
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
		create_info.enabledLayerCount = (uint32_t)arrlen(layerTemp);
		create_info.ppEnabledLayerNames = layerTemp;
		create_info.enabledExtensionCount = extension_count;
		create_info.ppEnabledExtensionNames = instanceExtensionCache;

		LOGF(eINFO, "Creating VkInstance with %i enabled instance layers:", arrlen(layerTemp));
		for (int i = 0; i < arrlen(layerTemp); i++)
			LOGF(eINFO, "\tLayer %i: %s", i, layerTemp[i]);

		CHECK_VKRESULT(vkCreateInstance(&create_info, &gVkAllocationCallbacks, &(pRenderer->mVulkan.pVkInstance)));
		arrfree(layerTemp);
		arrfree(wantedInstanceExtensions);
	}

#if defined(NX64)
	loadExtensionsNX(pRenderer->mVulkan.pVkInstance);
#else
	// Load Vulkan instance functions
	volkLoadInstance(pRenderer->mVulkan.pVkInstance);
#endif

	// Debug
	{
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
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

#ifdef ENABLE_DEBUG_UTILS_EXTENSION
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

static bool initCommon(const char* appName, const RendererDesc* pDesc, Renderer* pRenderer)
{
#if defined(VK_USE_DISPATCH_TABLES)
	VkResult vkRes = volkInitializeWithDispatchTables(pRenderer);
	if (vkRes != VK_SUCCESS)
	{
		LOGF(LogLevel::eERROR, "Failed to initialize Vulkan");
		nvapiExit();
		agsExit();
		return false;
	}
#else
		const char** instanceLayers = (const char**)alloca((2 + pDesc->mVulkan.mInstanceLayerCount) * sizeof(char*));
		uint32_t     instanceLayerCount = 0;

#if defined(ENABLE_GRAPHICS_DEBUG)
		// this turns on all validation layers
		instanceLayers[instanceLayerCount++] = "VK_LAYER_KHRONOS_validation";
#endif

		// this turns on render doc layer for gpu capture
#ifdef ENABLE_RENDER_DOC
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
		return false;
	}
#endif

	CreateInstance(appName, pDesc, instanceLayerCount, instanceLayers, pRenderer);
#endif

	pRenderer->mUnlinkedRendererIndex = 0;
	pRenderer->mVulkan.mOwnInstance = true;
	return true;
}

static void exitCommon(Renderer* pRenderer)
{
#if defined(VK_USE_DISPATCH_TABLES)
#else
	RemoveInstance(pRenderer);
#endif
}

static bool SelectBestGpu(Renderer* pRenderer)
{
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkInstance);

	uint32_t gpuCount = 0;

	CHECK_VKRESULT(vkEnumeratePhysicalDevices(pRenderer->mVulkan.pVkInstance, &gpuCount, NULL));

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

	CHECK_VKRESULT(vkEnumeratePhysicalDevices(pRenderer->mVulkan.pVkInstance, &gpuCount, gpus));
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
		util_query_gpu_settings(gpus[i], &gpuProperties[i], &gpuMemoryProperties[i], &gpuFeatures[i], &queueFamilyProperties[i], &queueFamilyPropertyCount[i], &gpuSettings[i]);

		LOGF(
			LogLevel::eINFO, "GPU[%i] detected. Vendor ID: %s, Model ID: %s, Preset: %s, GPU Name: %s", i,
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
	pRenderer->mVulkan.pVkActiveGPUProperties->pNext = NULL;
	*pRenderer->pActiveGpuSettings = gpuSettings[gpuIndex];
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkActiveGPU);

	LOGF(LogLevel::eINFO, "GPU[%d] is selected as default GPU", gpuIndex);
	LOGF(LogLevel::eINFO, "Name of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mGpuName);
	LOGF(LogLevel::eINFO, "Vendor id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mVendorId);
	LOGF(LogLevel::eINFO, "Model id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mModelId);
	LOGF(LogLevel::eINFO, "Preset of selected gpu: %s", presetLevelToString(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel));

	for (uint32_t i = 0; i < gpuCount; ++i)
		SAFE_FREE(queueFamilyProperties[i]);

	return true;
}

static bool AddDevice(const RendererDesc* pDesc, Renderer* pRenderer)
{
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkInstance);

	// These are the extensions that we have loaded
	const char* deviceExtensionCache[MAX_DEVICE_EXTENSIONS] = {};

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
		CHECK_VKRESULT(vkEnumeratePhysicalDeviceGroupsKHR(pRenderer->mVulkan.pVkInstance, &deviceGroupCount, props));

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

	if (pRenderer->mLinkedNodeCount < 2 && pRenderer->mGpuMode == GPU_MODE_LINKED)
	{
		pRenderer->mGpuMode = GPU_MODE_SINGLE;
	}

	if (!pDesc->pContext)
	{
		if (!SelectBestGpu(pRenderer))
			return false;
	}
	else
	{
		ASSERT(pDesc->mGpuIndex < pDesc->pContext->mGpuCount);

		pRenderer->mVulkan.pVkActiveGPU = pDesc->pContext->mGpus[pDesc->mGpuIndex].mVulkan.pGPU;
		pRenderer->mVulkan.pVkActiveGPUProperties = (VkPhysicalDeviceProperties2*)tf_malloc(sizeof(VkPhysicalDeviceProperties2));
		pRenderer->pActiveGpuSettings = (GPUSettings*)tf_malloc(sizeof(GPUSettings));
		*pRenderer->mVulkan.pVkActiveGPUProperties = pDesc->pContext->mGpus[pDesc->mGpuIndex].mVulkan.mGPUProperties;
		pRenderer->mVulkan.pVkActiveGPUProperties->pNext = NULL;
		*pRenderer->pActiveGpuSettings = pDesc->pContext->mGpus[pDesc->mGpuIndex].mSettings;
	}


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
		if (strcmp(layers[i].layerName, "VK_LAYER_RENDERDOC_Capture") == 0)
			gRenderDocLayerEnabled = true;
	}

#if VK_DEBUG_LOG_EXTENSIONS
	for (uint32_t i = 0; i < layerCount; ++i)
	{
		internal_log(eINFO, layers[i].layerName, "vkdevice-layer");
	}

	for (uint32_t i = 0; i < extCount; ++i)
	{
		internal_log(eINFO, exts[i].extensionName, "vkdevice-ext");
	}
#endif

	uint32_t extension_count = 0;
	bool     dedicatedAllocationExtension = false;
	bool     memoryReq2Extension = false;
#if VK_EXT_fragment_shader_interlock
	bool     fragmentShaderInterlockExtension = false;
#endif
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	bool     externalMemoryExtension = false;
	bool     externalMemoryWin32Extension = false;
#endif
	// Standalone extensions
	{
		const char*		layer_name = NULL;
		uint32_t		initialCount = sizeof(gVkWantedDeviceExtensions) / sizeof(gVkWantedDeviceExtensions[0]);
		const uint32_t	userRequestedCount = (uint32_t)pDesc->mVulkan.mDeviceExtensionCount;
		const char**	wantedDeviceExtensions = NULL;
		arrsetlen(wantedDeviceExtensions, initialCount + userRequestedCount);
		for (uint32_t i = 0; i < initialCount; ++i)
		{
			wantedDeviceExtensions[i] = gVkWantedDeviceExtensions[i];
		}
		for (uint32_t i = 0; i < userRequestedCount; ++i)
		{
			wantedDeviceExtensions[initialCount + i] = pDesc->mVulkan.ppDeviceExtensions[i];
		}
		const uint32_t wanted_extension_count = (uint32_t)arrlen(wantedDeviceExtensions);
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

#ifndef ENABLE_DEBUG_UTILS_EXTENSION
						if (strcmp(wantedDeviceExtensions[k], VK_EXT_DEBUG_MARKER_EXTENSION_NAME) == 0)
							pRenderer->mVulkan.mDebugMarkerSupport = true;
#endif
						if (strcmp(wantedDeviceExtensions[k], VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) == 0)
							dedicatedAllocationExtension = true;
						if (strcmp(wantedDeviceExtensions[k], VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 0)
							memoryReq2Extension = true;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
						if (strcmp(wantedDeviceExtensions[k], VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME) == 0)
							externalMemoryExtension = true;
						if (strcmp(wantedDeviceExtensions[k], VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME) == 0)
							externalMemoryWin32Extension = true;
#endif
#if VK_KHR_draw_indirect_count
						if (strcmp(wantedDeviceExtensions[k], VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME) == 0)
							pRenderer->mVulkan.mDrawIndirectCountExtension = true;
#endif
						if (strcmp(wantedDeviceExtensions[k], VK_AMD_DRAW_INDIRECT_COUNT_EXTENSION_NAME) == 0)
							pRenderer->mVulkan.mAMDDrawIndirectCountExtension = true;
						if (strcmp(wantedDeviceExtensions[k], VK_AMD_GCN_SHADER_EXTENSION_NAME) == 0)
							pRenderer->mVulkan.mAMDGCNShaderExtension = true;
#if VK_EXT_descriptor_indexing
						if (strcmp(wantedDeviceExtensions[k], VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) == 0)
							pRenderer->mVulkan.mDescriptorIndexingExtension = true;
#endif
#ifdef VK_RAYTRACING_AVAILABLE
						// KHRONOS VULKAN RAY TRACING
						uint32_t khrRaytracingSupported = 1; 

						if (strcmp(wantedDeviceExtensions[k], VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME) == 0)
							pRenderer->mVulkan.mShaderFloatControlsExtension = 1;
						khrRaytracingSupported &= pRenderer->mVulkan.mShaderFloatControlsExtension;

						if (strcmp(wantedDeviceExtensions[k], VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) == 0)
							pRenderer->mVulkan.mBufferDeviceAddressExtension = 1;
						khrRaytracingSupported &= pRenderer->mVulkan.mBufferDeviceAddressExtension;

						if (strcmp(wantedDeviceExtensions[k], VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) == 0)
							pRenderer->mVulkan.mDeferredHostOperationsExtension = 1;
						khrRaytracingSupported &= pRenderer->mVulkan.mDeferredHostOperationsExtension;

						if (strcmp(wantedDeviceExtensions[k], VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0)
							pRenderer->mVulkan.mKHRAccelerationStructureExtension = 1;
						khrRaytracingSupported &= pRenderer->mVulkan.mKHRAccelerationStructureExtension;

						if (strcmp(wantedDeviceExtensions[k], VK_KHR_SPIRV_1_4_EXTENSION_NAME) == 0)
							pRenderer->mVulkan.mKHRSpirv14Extension = 1;
						khrRaytracingSupported &= pRenderer->mVulkan.mKHRSpirv14Extension;

						if (strcmp(wantedDeviceExtensions[k], VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0)
							pRenderer->mVulkan.mKHRRayTracingPipelineExtension = 1;
						khrRaytracingSupported &= pRenderer->mVulkan.mKHRRayTracingPipelineExtension;

						if (khrRaytracingSupported)
							pRenderer->mVulkan.mRaytracingSupported = 1;

						if (strcmp(wantedDeviceExtensions[k], VK_KHR_RAY_QUERY_EXTENSION_NAME) == 0)
							pRenderer->mVulkan.mKHRRayQueryExtension = 1;
#endif
#if VK_KHR_sampler_ycbcr_conversion
						if (strcmp(wantedDeviceExtensions[k], VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME) == 0)
						{
							pRenderer->mVulkan.mYCbCrExtension = true;
						}
#endif
#if VK_EXT_fragment_shader_interlock
						if (strcmp(wantedDeviceExtensions[k], VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME) == 0)
						{
							fragmentShaderInterlockExtension = true;
						}
#endif
#if defined(QUEST_VR)
						if (strcmp(wantedDeviceExtensions[k], VK_KHR_MULTIVIEW_EXTENSION_NAME) == 0)
						{
							pRenderer->mVulkan.mMultiviewExtension = true;
						}
#endif
#ifdef ENABLE_NSIGHT_AFTERMATH
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
		arrfree(wantedDeviceExtensions);
	}

#if !defined(VK_USE_DISPATCH_TABLES)
	//-V:ADD_TO_NEXT_CHAIN:506, 1027
#define ADD_TO_NEXT_CHAIN(condition, next)        \
	if ((condition))                              \
	{                                             \
		base->pNext = (VkBaseOutStructure*)&next; \
		base = (VkBaseOutStructure*)base->pNext;  \
	}

	VkPhysicalDeviceFeatures2KHR gpuFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
	VkBaseOutStructure* base = (VkBaseOutStructure*)&gpuFeatures2; //-V1027

	// Add more extensions here
#if VK_EXT_fragment_shader_interlock
	VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragmentShaderInterlockFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT };
	ADD_TO_NEXT_CHAIN(fragmentShaderInterlockExtension, fragmentShaderInterlockFeatures);
#endif
#if VK_EXT_descriptor_indexing
	VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexingFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT };
	ADD_TO_NEXT_CHAIN(pRenderer->mVulkan.mDescriptorIndexingExtension, descriptorIndexingFeatures);
#endif
#if VK_KHR_sampler_ycbcr_conversion
	VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcrFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES };
	ADD_TO_NEXT_CHAIN(pRenderer->mVulkan.mYCbCrExtension, ycbcrFeatures);
#endif
#if defined(QUEST_VR)
	VkPhysicalDeviceMultiviewFeatures multiviewFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES };
	ADD_TO_NEXT_CHAIN(pRenderer->mVulkan.mMultiviewExtension, multiviewFeatures);
#endif


#if VK_KHR_buffer_device_address
	VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDeviceAddressFeatures = {};
	enabledBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	enabledBufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
	ADD_TO_NEXT_CHAIN(pRenderer->mVulkan.mBufferDeviceAddressExtension, enabledBufferDeviceAddressFeatures);
#endif
#if VK_KHR_ray_tracing_pipeline
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures = {};
	enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
	ADD_TO_NEXT_CHAIN(pRenderer->mVulkan.mKHRRayTracingPipelineExtension, enabledRayTracingPipelineFeatures); 
#endif
#if VK_KHR_acceleration_structure
	VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures = {};
	enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
	ADD_TO_NEXT_CHAIN(pRenderer->mVulkan.mKHRAccelerationStructureExtension, enabledAccelerationStructureFeatures);
#endif
#if VK_KHR_ray_query
	VkPhysicalDeviceRayQueryFeaturesKHR enabledRayQueryFeatures = {};
	enabledRayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
	enabledRayQueryFeatures.rayQuery = VK_TRUE; 
	ADD_TO_NEXT_CHAIN(pRenderer->mVulkan.mKHRRayQueryExtension, enabledRayQueryFeatures); 
#endif

#ifdef NX64
	vkGetPhysicalDeviceFeatures2(pRenderer->mVulkan.pVkActiveGPU, &gpuFeatures2);
#else
	vkGetPhysicalDeviceFeatures2KHR(pRenderer->mVulkan.pVkActiveGPU, &gpuFeatures2);
#endif

	// Get queue family properties
	uint32_t                 queueFamiliesCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->mVulkan.pVkActiveGPU, &queueFamiliesCount, NULL);
	VkQueueFamilyProperties* queueFamiliesProperties = (VkQueueFamilyProperties*)alloca(queueFamiliesCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->mVulkan.pVkActiveGPU, &queueFamiliesCount, queueFamiliesProperties);

	// need a queue_priority for each queue in the queue family we create
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

	VkDeviceCreateInfo create_info = {};
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

#if defined(ENABLE_NSIGHT_AFTERMATH)
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
	ADD_TO_NEXT_CHAIN(pRenderer->mGpuMode == GPU_MODE_LINKED, deviceGroupInfo);
#endif
	CHECK_VKRESULT(vkCreateDevice(pRenderer->mVulkan.pVkActiveGPU, &create_info, &gVkAllocationCallbacks, &pRenderer->mVulkan.pVkDevice));

#if !defined(NX64)
	// Load Vulkan device functions to bypass loader
	if (pRenderer->mGpuMode != GPU_MODE_UNLINKED)
		volkLoadDevice(pRenderer->mVulkan.pVkDevice);
#endif
#endif

	pRenderer->mVulkan.mDedicatedAllocationExtension = dedicatedAllocationExtension && memoryReq2Extension;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
	pRenderer->mVulkan.mExternalMemoryExtension = externalMemoryExtension && externalMemoryWin32Extension;
#endif

	if (pRenderer->mVulkan.mDedicatedAllocationExtension)
	{
		LOGF(LogLevel::eINFO, "Successfully loaded Dedicated Allocation extension");
	}

	if (pRenderer->mVulkan.mExternalMemoryExtension)
	{
		LOGF(LogLevel::eINFO, "Successfully loaded External Memory extension");
	}

#if VK_KHR_draw_indirect_count
	if (pRenderer->mVulkan.mDrawIndirectCountExtension)
	{
		pfnVkCmdDrawIndirectCountKHR = vkCmdDrawIndirectCountKHR;
		pfnVkCmdDrawIndexedIndirectCountKHR = vkCmdDrawIndexedIndirectCountKHR;
		LOGF(LogLevel::eINFO, "Successfully loaded Draw Indirect extension");
	}
	else if (pRenderer->mVulkan.mAMDDrawIndirectCountExtension)
#endif
	{
		pfnVkCmdDrawIndirectCountKHR = vkCmdDrawIndirectCountAMD;
		pfnVkCmdDrawIndexedIndirectCountKHR = vkCmdDrawIndexedIndirectCountAMD;
		LOGF(LogLevel::eINFO, "Successfully loaded AMD Draw Indirect extension");
	}

	if (pRenderer->mVulkan.mAMDGCNShaderExtension)
	{
		LOGF(LogLevel::eINFO, "Successfully loaded AMD GCN Shader extension");
	}

	if (pRenderer->mVulkan.mDescriptorIndexingExtension)
	{
		LOGF(LogLevel::eINFO, "Successfully loaded Descriptor Indexing extension");
	}

	if (pRenderer->mVulkan.mRaytracingSupported)
	{
		LOGF(LogLevel::eINFO, "Successfully loaded Khronos Ray Tracing extensions");
	}

#ifdef _ENABLE_DEBUG_UTILS_EXTENSION

	pRenderer->mVulkan.mDebugMarkerSupport = (vkCmdBeginDebugUtilsLabelEXT) && (vkCmdEndDebugUtilsLabelEXT) && (vkCmdInsertDebugUtilsLabelEXT) &&
						  (vkSetDebugUtilsObjectNameEXT);
#endif

	vk_utils_caps_builder(pRenderer);

	return true;
}

static void RemoveDevice(Renderer* pRenderer)
{
	vkDestroyDescriptorSetLayout(pRenderer->mVulkan.pVkDevice, pRenderer->mVulkan.pEmptyDescriptorSetLayout, &gVkAllocationCallbacks);
	vkDestroyDescriptorPool(pRenderer->mVulkan.pVkDevice, pRenderer->mVulkan.pEmptyDescriptorPool, &gVkAllocationCallbacks);
	vkDestroyDevice(pRenderer->mVulkan.pVkDevice, &gVkAllocationCallbacks);
	SAFE_FREE(pRenderer->pActiveGpuSettings);
	SAFE_FREE(pRenderer->mVulkan.pVkActiveGPUProperties);

#if defined(ENABLE_NSIGHT_AFTERMATH)
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
// Renderer Context Init Exit (multi GPU)
/************************************************************************/
static uint32_t gRendererCount = 0;

void vk_initRendererContext(const char* appName, const RendererContextDesc* pDesc, RendererContext** ppContext)
{
	ASSERT(appName);
	ASSERT(pDesc);
	ASSERT(ppContext);
	ASSERT(gRendererCount == 0);

	RendererDesc fakeDesc = {};
	fakeDesc.mVulkan.mInstanceExtensionCount = pDesc->mVulkan.mInstanceExtensionCount;
	fakeDesc.mVulkan.mInstanceLayerCount = pDesc->mVulkan.mInstanceLayerCount;
	fakeDesc.mVulkan.ppInstanceExtensions = pDesc->mVulkan.ppInstanceExtensions;
	fakeDesc.mVulkan.ppInstanceLayers = pDesc->mVulkan.ppInstanceLayers;
	fakeDesc.mEnableGPUBasedValidation = pDesc->mEnableGPUBasedValidation;

	Renderer fakeRenderer = {};

	if (!initCommon(appName, &fakeDesc, &fakeRenderer))
		return;

	RendererContext* pContext = (RendererContext*)tf_calloc_memalign(1, alignof(RendererContext), sizeof(RendererContext));

	pContext->mVulkan.pVkInstance = fakeRenderer.mVulkan.pVkInstance;
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
	pContext->mVulkan.pVkDebugUtilsMessenger = fakeRenderer.mVulkan.pVkDebugUtilsMessenger;
#else
	pContext->mVulkan.pVkDebugReport = fakeRenderer.mVulkan.pVkDebugReport;
#endif

	uint32_t gpuCount = 0;
	CHECK_VKRESULT(vkEnumeratePhysicalDevices(pContext->mVulkan.pVkInstance, &gpuCount, NULL));
	gpuCount = min((uint32_t)MAX_MULTIPLE_GPUS, gpuCount);

	VkPhysicalDevice gpus[MAX_MULTIPLE_GPUS] = {};
	VkPhysicalDeviceProperties2 gpuProperties[MAX_MULTIPLE_GPUS] = {};
	VkPhysicalDeviceMemoryProperties gpuMemoryProperties[MAX_MULTIPLE_GPUS] = {};
	VkPhysicalDeviceFeatures2KHR gpuFeatures[MAX_MULTIPLE_GPUS] = {};

	CHECK_VKRESULT(vkEnumeratePhysicalDevices(pContext->mVulkan.pVkInstance, &gpuCount, gpus));

	GPUSettings gpuSettings[MAX_MULTIPLE_GPUS] = {};
	bool gpuValid[MAX_MULTIPLE_GPUS] = {};

	uint32_t realGpuCount = 0;

	for (uint32_t i = 0; i < gpuCount; ++i)
	{
		uint32_t queueFamilyPropertyCount = 0;
		VkQueueFamilyProperties* queueFamilyProperties = NULL;
		util_query_gpu_settings(gpus[i], &gpuProperties[i], &gpuMemoryProperties[i], &gpuFeatures[i],
			&queueFamilyProperties, &queueFamilyPropertyCount, &gpuSettings[i]);

		// Filter GPUs that don't meet requirements
		bool supportGraphics = false;
		for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j)
		{
			if (queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				supportGraphics = true;
				break;
			}
		}
		gpuValid[i] = supportGraphics && (VK_PHYSICAL_DEVICE_TYPE_CPU != gpuProperties[i].properties.deviceType);
		if (gpuValid[i])
		{
			++realGpuCount;
		}

		SAFE_FREE(queueFamilyProperties);
	}

	pContext->mGpuCount = realGpuCount;

	for (uint32_t i = 0, realGpu = 0; i < gpuCount; ++i)
	{
		if (!gpuValid[i])
			continue;

		pContext->mGpus[realGpu].mSettings = gpuSettings[i];
		pContext->mGpus[realGpu].mVulkan.pGPU = gpus[i];
		pContext->mGpus[realGpu].mVulkan.mGPUProperties = gpuProperties[i];
		pContext->mGpus[realGpu].mVulkan.mGPUProperties.pNext = NULL;

		LOGF(LogLevel::eINFO, "GPU[%i] detected. Vendor ID: %s, Model ID: %s, Preset: %s, GPU Name: %s", realGpu,
			gpuSettings[i].mGpuVendorPreset.mVendorId,
			gpuSettings[i].mGpuVendorPreset.mModelId,
			presetLevelToString(gpuSettings[i].mGpuVendorPreset.mPresetLevel),
			gpuSettings[i].mGpuVendorPreset.mGpuName);

		++realGpu;
	}
	*ppContext = pContext;
}

void vk_exitRendererContext(RendererContext* pContext)
{
	ASSERT(gRendererCount == 0);

	Renderer fakeRenderer = {};
	fakeRenderer.mVulkan.pVkInstance = pContext->mVulkan.pVkInstance;
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
	fakeRenderer.mVulkan.pVkDebugUtilsMessenger = pContext->mVulkan.pVkDebugUtilsMessenger;
#else
	fakeRenderer.mVulkan.pVkDebugReport = pContext->mVulkan.pVkDebugReport;
#endif
	exitCommon(&fakeRenderer);

	SAFE_FREE(pContext);
}


/************************************************************************/
// Renderer Init Remove
/************************************************************************/
void vk_initRenderer(const char* appName, const RendererDesc* pDesc, Renderer** ppRenderer)
{
	ASSERT(appName);
	ASSERT(pDesc);
	ASSERT(ppRenderer);

	uint8_t* mem = (uint8_t*)tf_calloc_memalign(1, alignof(Renderer), sizeof(Renderer) + sizeof(NullDescriptors));
	ASSERT(mem);

	Renderer* pRenderer = (Renderer*)mem;
	pRenderer->mGpuMode = pDesc->mGpuMode;
	pRenderer->mShaderTarget = pDesc->mShaderTarget;
	pRenderer->mEnableGpuBasedValidation = pDesc->mEnableGPUBasedValidation;
	pRenderer->pNullDescriptors = (NullDescriptors*)(mem + sizeof(Renderer));

	pRenderer->pName = (char*)tf_calloc(strlen(appName) + 1, sizeof(char));
	strcpy(pRenderer->pName, appName);

	// Initialize the Vulkan internal bits
	{
		ASSERT(pDesc->mGpuMode != GPU_MODE_UNLINKED || pDesc->pContext); // context required in unlinked mode
		if (pDesc->pContext)
		{
			ASSERT(pDesc->mGpuIndex < pDesc->pContext->mGpuCount);
			pRenderer->mVulkan.pVkInstance = pDesc->pContext->mVulkan.pVkInstance;
			pRenderer->mVulkan.mOwnInstance = false;
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
			pRenderer->mVulkan.pVkDebugUtilsMessenger = pDesc->pContext->mVulkan.pVkDebugUtilsMessenger;
#else
			pRenderer->mVulkan.pVkDebugReport = pDesc->pContext->mVulkan.pVkDebugReport;
#endif
			pRenderer->mUnlinkedRendererIndex = gRendererCount;
		}
		else if (!initCommon(appName, pDesc, pRenderer))
		{
			SAFE_FREE(pRenderer->pName);
			SAFE_FREE(pRenderer);
			return;
		}

		if (!AddDevice(pDesc, pRenderer))
		{
			if (pRenderer->mVulkan.mOwnInstance)
				exitCommon(pRenderer);
			SAFE_FREE(pRenderer->pName);
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
			RemoveDevice(pRenderer);
			if (pRenderer->mVulkan.mOwnInstance)
				exitCommon(pRenderer);
			SAFE_FREE(pRenderer);
			LOGF(LogLevel::eERROR, "Selected GPU has an Office Preset in gpu.cfg.");
			LOGF(LogLevel::eERROR, "Office preset is not supported by The Forge.");

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

		if (pRenderer->mVulkan.mDedicatedAllocationExtension)
		{
			createInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
		}

		if (pRenderer->mVulkan.mBufferDeviceAddressExtension)
		{
			createInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		}

		VmaVulkanFunctions vulkanFunctions = {};
		vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
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
#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
		/// Fetch "vkBindBufferMemory2" on Vulkan >= 1.1, fetch "vkBindBufferMemory2KHR" when using VK_KHR_bind_memory2 extension.
		vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
		/// Fetch "vkBindImageMemory2" on Vulkan >= 1.1, fetch "vkBindImageMemory2KHR" when using VK_KHR_bind_memory2 extension.
		vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
#endif
#if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
#ifdef NX64
		vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;
#else
		vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR;
#endif
#endif
#if VMA_VULKAN_VERSION >= 1003000
		/// Fetch from "vkGetDeviceBufferMemoryRequirements" on Vulkan >= 1.3, but you can also fetch it from "vkGetDeviceBufferMemoryRequirementsKHR" if you enabled extension VK_KHR_maintenance4.
		vulkanFunctions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
		/// Fetch from "vkGetDeviceImageMemoryRequirements" on Vulkan >= 1.3, but you can also fetch it from "vkGetDeviceImageMemoryRequirementsKHR" if you enabled extension VK_KHR_maintenance4.
		vulkanFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;
#endif

		createInfo.pVulkanFunctions = &vulkanFunctions;
		createInfo.pAllocationCallbacks = &gVkAllocationCallbacks;
		vmaCreateAllocator(&createInfo, &pRenderer->mVulkan.pVmaAllocator);
	}

	// Empty descriptor set for filling in gaps when example: set 1 is used but set 0 is not used in the shader.
	// We still need to bind empty descriptor set here to keep some drivers happy
	VkDescriptorPoolSize descriptorPoolSizes[1] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1 } };
	add_descriptor_pool(pRenderer, 1, 0, descriptorPoolSizes, 1, &pRenderer->mVulkan.pEmptyDescriptorPool);
	VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
	VkDescriptorSet* emptySets[] = { &pRenderer->mVulkan.pEmptyDescriptorSet };
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	CHECK_VKRESULT(vkCreateDescriptorSetLayout(pRenderer->mVulkan.pVkDevice, &layoutCreateInfo, &gVkAllocationCallbacks, &pRenderer->mVulkan.pEmptyDescriptorSetLayout));
	consume_descriptor_sets(pRenderer->mVulkan.pVkDevice, pRenderer->mVulkan.pEmptyDescriptorPool, &pRenderer->mVulkan.pEmptyDescriptorSetLayout, 1, emptySets);

	initMutex(&gRenderPassMutex[pRenderer->mUnlinkedRendererIndex]);
	gRenderPassMap[pRenderer->mUnlinkedRendererIndex] = NULL;
	hmdefault(gRenderPassMap[pRenderer->mUnlinkedRendererIndex], NULL);
	gFrameBufferMap[pRenderer->mUnlinkedRendererIndex] = NULL;
	hmdefault(gFrameBufferMap[pRenderer->mUnlinkedRendererIndex], NULL);

	VkPhysicalDeviceFeatures2KHR gpuFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
#ifdef NX64
	vkGetPhysicalDeviceFeatures2(pRenderer->mVulkan.pVkActiveGPU, &gpuFeatures);
#else
	vkGetPhysicalDeviceFeatures2KHR(pRenderer->mVulkan.pVkActiveGPU, &gpuFeatures);
#endif
	
	pRenderer->mVulkan.mShaderSampledImageArrayDynamicIndexingSupported = (uint32_t)(gpuFeatures.features.shaderSampledImageArrayDynamicIndexing);
	if (pRenderer->mVulkan.mShaderSampledImageArrayDynamicIndexingSupported)
	{
		LOGF(LogLevel::eINFO, "GPU supports texture array dynamic indexing");
	}

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
		if (pDesc->mGpuMode != GPU_MODE_UNLINKED)
			exitCommon(pRenderer);
        SAFE_FREE(pRenderer);
        LOGF(LogLevel::eERROR, "Failed to initialize VrApi Vulkan systems.");
#endif
        *ppRenderer = NULL;
        return;
    }
#endif

	++gRendererCount;
	ASSERT(gRendererCount <= MAX_UNLINKED_GPUS);

	// Renderer is good!
	*ppRenderer = pRenderer;
}

void vk_exitRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);
	--gRendererCount;

	remove_default_resources(pRenderer);

	// Remove the renderpasses
	for (ptrdiff_t i = 0; i < hmlen(gRenderPassMap[pRenderer->mUnlinkedRendererIndex]); ++i)
	{
		RenderPassNode** pMap = gRenderPassMap[pRenderer->mUnlinkedRendererIndex][i].value;
		RenderPassNode* map = *pMap;
		for (ptrdiff_t j = 0; j < hmlen(map); ++j)
			remove_render_pass(pRenderer, &map[j].value);
		hmfree(map);
		tf_free(pMap);
	}
	hmfree(gRenderPassMap[pRenderer->mUnlinkedRendererIndex]);

	for (ptrdiff_t i = 0; i < hmlen(gFrameBufferMap[pRenderer->mUnlinkedRendererIndex]); ++i)
	{
		FrameBufferNode** pMap = gFrameBufferMap[pRenderer->mUnlinkedRendererIndex][i].value;
		FrameBufferNode* map = *pMap;
		for (ptrdiff_t j = 0; j < hmlen(map); ++j)
			remove_framebuffer(pRenderer, &map[j].value);
		hmfree(map);
		tf_free(pMap);
	}
	hmfree(gFrameBufferMap[pRenderer->mUnlinkedRendererIndex]);

#if defined(QUEST_VR)
    hook_pre_remove_renderer();
#endif

	// Destroy the Vulkan bits
	vmaDestroyAllocator(pRenderer->mVulkan.pVmaAllocator);

#if defined(VK_USE_DISPATCH_TABLES)
#else
	RemoveDevice(pRenderer);
#endif
	if (pRenderer->mVulkan.mOwnInstance)
		exitCommon(pRenderer);

	destroyMutex(&gRenderPassMutex[pRenderer->mUnlinkedRendererIndex]);

	SAFE_FREE(gRenderPassMap[pRenderer->mUnlinkedRendererIndex]);
	SAFE_FREE(gFrameBufferMap[pRenderer->mUnlinkedRendererIndex]);

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

	const uint32_t          nodeIndex = (pRenderer->mGpuMode == GPU_MODE_LINKED) ? pDesc->mNodeIndex : 0;
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

	// override node index
	if (pRenderer->mGpuMode == GPU_MODE_UNLINKED)
		pQueue->mNodeIndex = pRenderer->mUnlinkedRendererIndex;

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

	const uint32_t     nodeIndex = pRenderer->mGpuMode == GPU_MODE_LINKED ? pQueue->mNodeIndex : 0;
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
		if (presentQueueFamilyIndex == UINT32_MAX)
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
	if (presentQueueFamilyIndex != UINT32_MAX && queue_family_indices[0] != presentQueueFamilyIndex)
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
	descColor.mNodeIndex = pRenderer->mUnlinkedRendererIndex;

	char buffer[32] = {};
	// Populate the vk_image field and add the Vulkan texture objects
	for (uint32_t i = 0; i < imageCount; ++i)
	{
		sprintf(buffer, "Swapchain RT[%u]", i);
		descColor.pName = buffer;
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
	ASSERT(pRenderer->mGpuMode != GPU_MODE_UNLINKED || pDesc->mNodeIndex == pRenderer->mUnlinkedRendererIndex);

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
	if (pDesc->mFlags & BUFFER_CREATION_FLAG_HOST_VISIBLE)
		vma_mem_reqs.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	if (pDesc->mFlags & BUFFER_CREATION_FLAG_HOST_COHERENT)
		vma_mem_reqs.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

#if defined(ANDROID) || defined(NX64)
	// UMA for Android and NX64 devices
	if (vma_mem_reqs.usage != VMA_MEMORY_USAGE_GPU_TO_CPU)
	{
		vma_mem_reqs.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		vma_mem_reqs.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
	}
#endif

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
	ASSERT(pRenderer->mGpuMode != GPU_MODE_UNLINKED || pDesc->mNodeIndex == pRenderer->mUnlinkedRendererIndex);
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

		if (pRenderer->mVulkan.mExternalMemoryExtension && pDesc->mFlags & TEXTURE_CREATION_FLAG_IMPORT_BIT)
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
		else if (pRenderer->mVulkan.mExternalMemoryExtension && pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_BIT)
		{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
			exportMemoryInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
#endif

			mem_reqs.pUserData = &exportMemoryInfo;
			// Allocate external (importable / exportable) memory as dedicated memory to avoid adding unnecessary complexity to the Vulkan Memory Allocator
			mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		}


		// If lazy allocation is requested, check that the hardware supports it
		bool lazyAllocation = pDesc->mFlags & TEXTURE_CREATION_FLAG_ON_TILE;
		if (lazyAllocation)
		{
			uint32_t memoryTypeIndex = 0;
			VmaAllocationCreateInfo lazyMemReqs = mem_reqs;
			lazyMemReqs.usage = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
			VkResult result = vmaFindMemoryTypeIndex(pRenderer->mVulkan.pVmaAllocator, UINT32_MAX, &lazyMemReqs, &memoryTypeIndex);
			if (VK_SUCCESS == result)
			{
				mem_reqs = lazyMemReqs;
				add_info.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
				// The Vulkan spec states: If usage includes VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
				// then bits other than VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				// and VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT must not be set
				add_info.usage &= (VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
				pTexture->mLazilyAllocated = true;
			}
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
	pTexture->mSampleCount = pDesc->mSampleCount;

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
	ASSERT(pRenderer->mGpuMode != GPU_MODE_UNLINKED || pDesc->mNodeIndex == pRenderer->mUnlinkedRendererIndex);

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
	// Create SRV by default for a render target unless this is on tile texture where SRV is not supported
	if (!(pDesc->mFlags & TEXTURE_CREATION_FLAG_ON_TILE))
	{
		textureDesc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE;
	}
	else
	{
		if ((textureDesc.mDescriptors & DESCRIPTOR_TYPE_TEXTURE) || (textureDesc.mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE))
		{
			LOGF(eWARNING, "On tile textures do not support DESCRIPTOR_TYPE_TEXTURE or DESCRIPTOR_TYPE_RW_TEXTURE");
		}
		// On tile textures do not support SRV/UAV as there is no backing memory
		// You can only read these textures as input attachments inside same render pass
		textureDesc.mDescriptors &= (DescriptorType)(~(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE));
	}

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
	rtvDesc.format = (VkFormat)TinyImageFormat_ToVkFormat(textureDesc.mFormat);
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
	pRenderTarget->mFormat = textureDesc.mFormat;
	pRenderTarget->mClearValue = pDesc->mClearValue;
    pRenderTarget->mVRMultiview = (pDesc->mFlags & TEXTURE_CREATION_FLAG_VR_MULTIVIEW) != 0;
    pRenderTarget->mVRFoveatedRendering = (pDesc->mFlags & TEXTURE_CREATION_FLAG_VR_FOVEATED_RENDERING) != 0;

	// Unlike DX12, Vulkan textures start in undefined layout.
	// To keep in line with DX12, we transition them to the specified layout manually so app code doesn't have to worry about this
	// Render targets wont be created during runtime so this overhead will be minimal
	util_initial_transition(pRenderer, pRenderTarget->pTexture, pDesc->mStartState);

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
	if (pDesc->mFlags & TEXTURE_CREATION_FLAG_CREATE_RESOLVE_ATTACHMENT)
	{
		RenderTargetDesc resolveRTDesc = *pDesc;
		resolveRTDesc.mFlags &= ~(TEXTURE_CREATION_FLAG_CREATE_RESOLVE_ATTACHMENT | TEXTURE_CREATION_FLAG_ON_TILE);
		resolveRTDesc.mSampleCount = SAMPLE_COUNT_1;
		addRenderTarget(pRenderer, &resolveRTDesc, &pRenderTarget->pResolveAttachment);
	}
#endif

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

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
	if (pRenderTarget->pResolveAttachment)
	{
		removeRenderTarget(pRenderer, pRenderTarget->pResolveAttachment);
	}
#endif

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

	//default sampler lod values
	//used if not overriden by mSetLodRange or not Linear mipmaps
	float minSamplerLod = 0;
	float maxSamplerLod = pDesc->mMipMapMode == MIPMAP_MODE_LINEAR ? VK_LOD_CLAMP_NONE : 0;
	//user provided lods
	if(pDesc->mSetLodRange)
	{
		minSamplerLod = pDesc->mMinLod;
		maxSamplerLod = pDesc->mMaxLod;
	}

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
	add_info.anisotropyEnable = pRenderer->pActiveGpuSettings->mSamplerAnisotropySupported && (pDesc->mMaxAnisotropy > 0.0f) ? VK_TRUE : VK_FALSE;
	add_info.maxAnisotropy = pDesc->mMaxAnisotropy;
	add_info.compareEnable = (gVkComparisonFuncTranslator[pDesc->mCompareFunc] != VK_COMPARE_OP_NEVER) ? VK_TRUE : VK_FALSE;
	add_info.compareOp = gVkComparisonFuncTranslator[pDesc->mCompareFunc];
	add_info.minLod = minSamplerLod;
	add_info.maxLod = maxSamplerLod;
	add_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	add_info.unnormalizedCoordinates = VK_FALSE;

	if (TinyImageFormat_IsPlanar(pDesc->mSamplerConversionDesc.mFormat))
	{
		auto&    conversionDesc = pDesc->mSamplerConversionDesc;
		VkFormat format = (VkFormat)TinyImageFormat_ToVkFormat(conversionDesc.mFormat);

		// Check format props
		{
			ASSERT((uint32_t)pRenderer->mVulkan.mYCbCrExtension);

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

	CHECK_VKRESULT(vmaMapMemory(pRenderer->mVulkan.pVmaAllocator, pBuffer->mVulkan.pVkAllocation, &pBuffer->pCpuMappedAddress));

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
	const uint32_t                  nodeIndex = pRenderer->mGpuMode == GPU_MODE_LINKED ? pDesc->mNodeIndex : 0;
	const uint32_t                  dynamicOffsetCount = pRootSignature->mVulkan.mVkDynamicDescriptorCounts[updateFreq];

	uint32_t totalSize = sizeof(DescriptorSet);

	if (VK_NULL_HANDLE != pRootSignature->mVulkan.mVkDescriptorSetLayouts[updateFreq])
	{
		totalSize += pDesc->mMaxSets * sizeof(VkDescriptorSet);
	}

	totalSize += pDesc->mMaxSets * dynamicOffsetCount * sizeof(DynamicUniformData);

	DescriptorSet* pDescriptorSet = (DescriptorSet*)tf_calloc_memalign(1, alignof(DescriptorSet), totalSize);

	pDescriptorSet->mVulkan.pRootSignature = pRootSignature;
	pDescriptorSet->mVulkan.mUpdateFrequency = updateFreq;
	pDescriptorSet->mVulkan.mDynamicOffsetCount = dynamicOffsetCount;
	pDescriptorSet->mVulkan.mNodeIndex = nodeIndex;
	pDescriptorSet->mVulkan.mMaxSets = pDesc->mMaxSets;

	uint8_t* pMem = (uint8_t*)(pDescriptorSet + 1);
	pDescriptorSet->mVulkan.pHandles = (VkDescriptorSet*)pMem;
	pMem += pDesc->mMaxSets * sizeof(VkDescriptorSet);

	if (VK_NULL_HANDLE != pRootSignature->mVulkan.mVkDescriptorSetLayouts[updateFreq])
	{
		VkDescriptorSetLayout* pLayouts = (VkDescriptorSetLayout*)alloca(pDesc->mMaxSets * sizeof(VkDescriptorSetLayout));
		VkDescriptorSet**      pHandles = (VkDescriptorSet**)alloca(pDesc->mMaxSets * sizeof(VkDescriptorSet*));

		for (uint32_t i = 0; i < pDesc->mMaxSets; ++i)
		{
			pLayouts[i] = pRootSignature->mVulkan.mVkDescriptorSetLayouts[updateFreq];
			pHandles[i] = &pDescriptorSet->mVulkan.pHandles[i];
		}

		VkDescriptorPoolSize poolSizes[MAX_DESCRIPTOR_POOL_SIZE_ARRAY_COUNT] = {};
		for (uint32_t i = 0; i < pRootSignature->mVulkan.mPoolSizeCount[updateFreq]; ++i)
		{
			poolSizes[i] = pRootSignature->mVulkan.mPoolSizes[updateFreq][i];
			poolSizes[i].descriptorCount *= pDesc->mMaxSets;
		}
		add_descriptor_pool(pRenderer, pDesc->mMaxSets, 0,
			poolSizes, pRootSignature->mVulkan.mPoolSizeCount[updateFreq],
			&pDescriptorSet->mVulkan.pDescriptorPool);
		consume_descriptor_sets(pRenderer->mVulkan.pVkDevice, pDescriptorSet->mVulkan.pDescriptorPool, pLayouts, pDesc->mMaxSets, pHandles);

		for (uint32_t descIndex = 0; descIndex < pRootSignature->mDescriptorCount; ++descIndex)
		{
			const DescriptorInfo* descInfo = &pRootSignature->pDescriptors[descIndex];
			if (descInfo->mUpdateFrequency != updateFreq || descInfo->mRootDescriptor || descInfo->mStaticSampler)
			{
				continue;
			}

			DescriptorType type = (DescriptorType)descInfo->mType;

			VkWriteDescriptorSet writeSet = {};
			writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeSet.pNext = NULL;
			writeSet.descriptorCount = 1;
			writeSet.descriptorType = (VkDescriptorType)descInfo->mVulkan.mVkType;
			writeSet.dstArrayElement = 0;
			writeSet.dstBinding = descInfo->mVulkan.mReg;

			for (uint32_t index = 0; index < pDesc->mMaxSets; ++index)
			{
				writeSet.dstSet = pDescriptorSet->mVulkan.pHandles[index];

				switch (type)
				{
				case DESCRIPTOR_TYPE_SAMPLER:
				{
					VkDescriptorImageInfo updateData = { pRenderer->pNullDescriptors->pDefaultSampler->mVulkan.pVkSampler, VK_NULL_HANDLE };
					writeSet.pImageInfo = &updateData;
					for (uint32_t arr = 0; arr < descInfo->mSize; ++arr)
					{
						writeSet.dstArrayElement = arr;
						vkUpdateDescriptorSets(pRenderer->mVulkan.pVkDevice, 1, &writeSet, 0, NULL);
					}
					writeSet.pImageInfo = NULL;
					break;
				}
				case DESCRIPTOR_TYPE_TEXTURE:
				case DESCRIPTOR_TYPE_RW_TEXTURE:
				{
					VkImageView srcView = (type == DESCRIPTOR_TYPE_RW_TEXTURE) ?
						pRenderer->pNullDescriptors->pDefaultTextureUAV[nodeIndex][descInfo->mDim]->mVulkan.pVkUAVDescriptors[0] :
						pRenderer->pNullDescriptors->pDefaultTextureSRV[nodeIndex][descInfo->mDim]->mVulkan.pVkSRVDescriptor;
					VkImageLayout layout = (type == DESCRIPTOR_TYPE_RW_TEXTURE) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					VkDescriptorImageInfo updateData = { VK_NULL_HANDLE, srcView, layout };
					writeSet.pImageInfo = &updateData;
					for (uint32_t arr = 0; arr < descInfo->mSize; ++arr)
					{
						writeSet.dstArrayElement = arr;
						vkUpdateDescriptorSets(pRenderer->mVulkan.pVkDevice, 1, &writeSet, 0, NULL);
					}
					writeSet.pImageInfo = NULL;
					break;
				}
				case DESCRIPTOR_TYPE_BUFFER:
				case DESCRIPTOR_TYPE_BUFFER_RAW:
				case DESCRIPTOR_TYPE_RW_BUFFER:
				case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
				case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				{
					VkDescriptorBufferInfo updateData = { pRenderer->pNullDescriptors->pDefaultBufferSRV[nodeIndex]->mVulkan.pVkBuffer, 0, VK_WHOLE_SIZE };
					writeSet.pBufferInfo = &updateData;
					for (uint32_t arr = 0; arr < descInfo->mSize; ++arr)
					{
						writeSet.dstArrayElement = arr;
						vkUpdateDescriptorSets(pRenderer->mVulkan.pVkDevice, 1, &writeSet, 0, NULL);
					}
					writeSet.pBufferInfo = NULL;
					break;
				}
				case DESCRIPTOR_TYPE_TEXEL_BUFFER:
				case DESCRIPTOR_TYPE_RW_TEXEL_BUFFER:
				{
					VkBufferView updateData = (type == DESCRIPTOR_TYPE_RW_TEXEL_BUFFER) ?
						pRenderer->pNullDescriptors->pDefaultBufferUAV[nodeIndex]->mVulkan.pVkStorageTexelView :
						pRenderer->pNullDescriptors->pDefaultBufferSRV[nodeIndex]->mVulkan.pVkUniformTexelView;
					writeSet.pTexelBufferView = &updateData;
					for (uint32_t arr = 0; arr < descInfo->mSize; ++arr)
					{
						writeSet.dstArrayElement = arr;
						vkUpdateDescriptorSets(pRenderer->mVulkan.pVkDevice, 1, &writeSet, 0, NULL);
					}
					writeSet.pTexelBufferView = NULL;
					break;
				}
				default:
					break;
				}
			}
		}
	}
	else
	{
		LOGF(LogLevel::eERROR, "NULL Descriptor Set Layout for update frequency %u. Cannot allocate descriptor set", (uint32_t)updateFreq);
		ASSERT(false && "NULL Descriptor Set Layout for update frequency. Cannot allocate descriptor set");
	}

	if (pDescriptorSet->mVulkan.mDynamicOffsetCount)
	{
		pDescriptorSet->mVulkan.pDynamicUniformData = (DynamicUniformData*)pMem;
		pMem += pDescriptorSet->mVulkan.mMaxSets * pDescriptorSet->mVulkan.mDynamicOffsetCount * sizeof(DynamicUniformData);
	}

	*ppDescriptorSet = pDescriptorSet;
}

void vk_removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
{
	ASSERT(pRenderer);
	ASSERT(pDescriptorSet);

	vkDestroyDescriptorPool(pRenderer->mVulkan.pVkDevice, pDescriptorSet->mVulkan.pDescriptorPool, &gVkAllocationCallbacks);
	SAFE_FREE(pDescriptorSet);
}

#if defined(ENABLE_GRAPHICS_DEBUG)
#define VALIDATE_DESCRIPTOR(descriptor, ...)                       \
	if (!(descriptor))                                             \
	{                                                              \
		char messageBuf[256];                                      \
		sprintf(messageBuf, __VA_ARGS__);                          \
		LOGF(LogLevel::eERROR, "%s", messageBuf);                  \
		_FailedAssert(__FILE__, __LINE__, __FUNCTION__);           \
		continue;                                                  \
	}
#else
#define VALIDATE_DESCRIPTOR(descriptor, ...)
#endif

void vk_updateDescriptorSet(
	Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
	ASSERT(pRenderer);
	ASSERT(pDescriptorSet);
	ASSERT(pDescriptorSet->mVulkan.pHandles);
	ASSERT(index < pDescriptorSet->mVulkan.mMaxSets);

	const uint32_t maxWriteSets = 256;
	// #NOTE - Should be good enough to avoid splitting most of the update calls other than edge cases having huge update sizes
	const uint32_t maxDescriptorInfoByteSize = sizeof(VkDescriptorImageInfo) * 1024;
	const RootSignature*      pRootSignature = pDescriptorSet->mVulkan.pRootSignature;
	VkWriteDescriptorSet      writeSetArray[maxWriteSets] = {};
	uint8_t                   descriptorUpdateDataStart[maxDescriptorInfoByteSize] = {};
	const uint8_t*            descriptorUpdateDataEnd = &descriptorUpdateDataStart[maxDescriptorInfoByteSize - 1];
	uint32_t                  writeSetCount = 0;

	uint8_t* descriptorUpdateData = descriptorUpdateDataStart;

#define FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(type, pInfo, count)                                         \
	if (descriptorUpdateData + sizeof(type) >= descriptorUpdateDataEnd)                               \
	{                                                                                                 \
		writeSet->descriptorCount = arr - lastArrayIndexStart;                                        \
		vkUpdateDescriptorSets(pRenderer->mVulkan.pVkDevice, writeSetCount, writeSetArray, 0, NULL);  \
		/* All previous write sets flushed. Start from zero */                                        \
		writeSetCount = 1;                                                                            \
		writeSetArray[0] = *writeSet;                                                                 \
		writeSet = &writeSetArray[0];                                                                 \
		lastArrayIndexStart = arr;                                                                    \
		writeSet->dstArrayElement += writeSet->descriptorCount;                                       \
		/* Set descriptor count to the remaining count */                                             \
		writeSet->descriptorCount = count - writeSet->dstArrayElement;                                \
		descriptorUpdateData = descriptorUpdateDataStart;                                             \
		writeSet->pInfo = (type*)descriptorUpdateData;                                                \
	}                                                                                                 \
	type* currUpdateData = (type*)descriptorUpdateData;                                               \
	descriptorUpdateData += sizeof(type);

	for (uint32_t i = 0; i < count; ++i)
	{
		const DescriptorData* pParam = pParams + i;
		uint32_t              paramIndex = pParam->mBindByIndex ? pParam->mIndex : UINT32_MAX;

		VALIDATE_DESCRIPTOR(pParam->pName || (paramIndex != UINT32_MAX), "DescriptorData has NULL name and invalid index");

		const DescriptorInfo* pDesc =
			(paramIndex != UINT32_MAX) ? (pRootSignature->pDescriptors + paramIndex) : get_descriptor(pRootSignature, pParam->pName);
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

		// #NOTE - Flush the update if we go above the max write set limit
		if (writeSetCount >= maxWriteSets)
		{
			vkUpdateDescriptorSets(pRenderer->mVulkan.pVkDevice, writeSetCount, writeSetArray, 0, NULL);
			writeSetCount = 0;
			descriptorUpdateData = descriptorUpdateDataStart;
		}

		VkWriteDescriptorSet* writeSet = &writeSetArray[writeSetCount++];
		writeSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet->pNext = NULL;
		writeSet->descriptorCount = arrayCount;
		writeSet->descriptorType = (VkDescriptorType)pDesc->mVulkan.mVkType;
		writeSet->dstArrayElement = arrayStart;
		writeSet->dstBinding = pDesc->mVulkan.mReg;
		writeSet->dstSet = pDescriptorSet->mVulkan.pHandles[index];

		VALIDATE_DESCRIPTOR(
			pDesc->mUpdateFrequency == pDescriptorSet->mVulkan.mUpdateFrequency, "Descriptor (%s) - Mismatching update frequency and set index", pDesc->pName);

		uint32_t lastArrayIndexStart = 0;

		switch (type)
		{
			case DESCRIPTOR_TYPE_SAMPLER:
			{
				// Index is invalid when descriptor is a static sampler
				VALIDATE_DESCRIPTOR(
					!pDesc->mStaticSampler,
					"Trying to update a static sampler (%s). All static samplers must be set in addRootSignature and cannot be updated "
					"later",
					pDesc->pName);

				VALIDATE_DESCRIPTOR(pParam->ppSamplers, "NULL Sampler (%s)", pDesc->pName);

				writeSet->pImageInfo = (VkDescriptorImageInfo*)descriptorUpdateData;

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppSamplers[arr], "NULL Sampler (%s [%u] )", pDesc->pName, arr);
					FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkDescriptorImageInfo, pImageInfo, arrayCount) //-V1032
					*currUpdateData = { pParam->ppSamplers[arr]->mVulkan.pVkSampler, VK_NULL_HANDLE };
				}
				break;
			}
			case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			{
				VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)", pDesc->pName);

#if defined(ENABLE_GRAPHICS_DEBUG)
				DescriptorIndexMap* pNode = shgetp_null(pRootSignature->pDescriptorNameToIndexMap, pDesc->pName);
				if (!pNode)
				{
					LOGF(LogLevel::eERROR, "No Static Sampler called (%s)", pDesc->pName);
					ASSERT(false);
				}
#endif

				writeSet->pImageInfo = (VkDescriptorImageInfo*)descriptorUpdateData;

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);
					FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkDescriptorImageInfo, pImageInfo, arrayCount)
					*currUpdateData =
					{
						NULL,                                                 // Sampler
						pParam->ppTextures[arr]->mVulkan.pVkSRVDescriptor,    // Image View
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL              // Image Layout
					};
				}
				break;
			}
			case DESCRIPTOR_TYPE_TEXTURE:
			{
				VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)", pDesc->pName);

				writeSet->pImageInfo = (VkDescriptorImageInfo*)descriptorUpdateData;

				if (!pParam->mBindStencilResource)
				{
					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);
						FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkDescriptorImageInfo, pImageInfo, arrayCount)
						*currUpdateData =
						{
							VK_NULL_HANDLE,                                       // Sampler
							pParam->ppTextures[arr]->mVulkan.pVkSRVDescriptor,    // Image View
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL              // Image Layout
						};
					}
				}
				else
				{
					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);
						FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkDescriptorImageInfo, pImageInfo, arrayCount)
						*currUpdateData =
						{
							VK_NULL_HANDLE,                                              // Sampler
							pParam->ppTextures[arr]->mVulkan.pVkSRVStencilDescriptor,    // Image View
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL                     // Image Layout
						};
					}
				}
				break;
			}
			case DESCRIPTOR_TYPE_RW_TEXTURE:
			{
				VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL RW Texture (%s)", pDesc->pName);

				writeSet->pImageInfo = (VkDescriptorImageInfo*)descriptorUpdateData;

				if (pParam->mBindMipChain)
				{
					VALIDATE_DESCRIPTOR(pParam->ppTextures[0], "NULL RW Texture (%s)", pDesc->pName);
					VALIDATE_DESCRIPTOR((!arrayStart), "Descriptor (%s) - mBindMipChain supports only updating the whole mip-chain. No partial updates supported", pParam->pName);
					const uint32_t mipCount = pParam->ppTextures[0]->mMipLevels;
					writeSet->descriptorCount = mipCount;

					for (uint32_t arr = 0; arr < mipCount; ++arr)
					{
						FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkDescriptorImageInfo, pImageInfo, mipCount)
						*currUpdateData =
						{
							VK_NULL_HANDLE,                                           // Sampler
							pParam->ppTextures[0]->mVulkan.pVkUAVDescriptors[arr],    // Image View
							VK_IMAGE_LAYOUT_GENERAL                                   // Image Layout
						};
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

						FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkDescriptorImageInfo, pImageInfo, arrayCount)
						*currUpdateData =
						{
							VK_NULL_HANDLE,                                                  // Sampler
							pParam->ppTextures[arr]->mVulkan.pVkUAVDescriptors[mipSlice],    // Image View
							VK_IMAGE_LAYOUT_GENERAL                                          // Image Layout
						};
					}
				}
				break;
			}
			case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			{
				if (pDesc->mRootDescriptor)
				{
					VALIDATE_DESCRIPTOR(
						false,
						"Descriptor (%s) - Trying to update a root cbv through updateDescriptorSet. All root cbvs must be updated through cmdBindDescriptorSetWithRootCbvs",
						pDesc->pName);

					break;
				}
				case DESCRIPTOR_TYPE_BUFFER:
				case DESCRIPTOR_TYPE_BUFFER_RAW:
				case DESCRIPTOR_TYPE_RW_BUFFER:
				case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Buffer (%s)", pDesc->pName);

					writeSet->pBufferInfo = (VkDescriptorBufferInfo*)descriptorUpdateData;

					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Buffer (%s [%u] )", pDesc->pName, arr);
						FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkDescriptorBufferInfo, pBufferInfo, arrayCount)
						*currUpdateData =
						{
							pParam->ppBuffers[arr]->mVulkan.pVkBuffer,
							pParam->ppBuffers[arr]->mVulkan.mOffset,
							VK_WHOLE_SIZE
						};

						if (pParam->pRanges)
						{
							DescriptorDataRange range = pParam->pRanges[arr];
#if defined(ENABLE_GRAPHICS_DEBUG)
							uint32_t maxRange = DESCRIPTOR_TYPE_UNIFORM_BUFFER == type ?
								pRenderer->mVulkan.pVkActiveGPUProperties->properties.limits.maxUniformBufferRange :
								pRenderer->mVulkan.pVkActiveGPUProperties->properties.limits.maxStorageBufferRange;
#endif

							VALIDATE_DESCRIPTOR(range.mSize, "Descriptor (%s) - pRanges[%u].mSize is zero", pDesc->pName, arr);
							VALIDATE_DESCRIPTOR(
								range.mSize <= maxRange,
								"Descriptor (%s) - pRanges[%u].mSize is %ull which exceeds max size %u", pDesc->pName, arr, range.mSize,
								maxRange);

							currUpdateData->offset = range.mOffset;
							currUpdateData->range = range.mSize;
						}
					}
				}
				break;
			}
			case DESCRIPTOR_TYPE_TEXEL_BUFFER:
			case DESCRIPTOR_TYPE_RW_TEXEL_BUFFER:
			{
				VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Texel Buffer (%s)", pDesc->pName);

				writeSet->pTexelBufferView = (VkBufferView*)descriptorUpdateData;

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Texel Buffer (%s [%u] )", pDesc->pName, arr);
					FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkBufferView, pTexelBufferView, arrayCount)
					*currUpdateData = DESCRIPTOR_TYPE_TEXEL_BUFFER == type ?
						pParam->ppBuffers[arr]->mVulkan.pVkUniformTexelView :
						pParam->ppBuffers[arr]->mVulkan.pVkStorageTexelView;
				}

				break;
			}
#ifdef VK_RAYTRACING_AVAILABLE
			case DESCRIPTOR_TYPE_RAY_TRACING:
			{
				VALIDATE_DESCRIPTOR(pParam->ppAccelerationStructures, "NULL Acceleration Structure (%s)", pDesc->pName);

				VkWriteDescriptorSetAccelerationStructureKHR writeSetKHR = {};
				VkAccelerationStructureKHR currUpdateData = {};
				writeSet->pNext = &writeSetKHR;
				writeSet->descriptorCount = 1;
				writeSetKHR.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
				writeSetKHR.pNext = NULL;
				writeSetKHR.accelerationStructureCount = 1;
				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					vk_FillRaytracingDescriptorData(pParam->ppAccelerationStructures[arr], &currUpdateData);
					writeSetKHR.pAccelerationStructures = &currUpdateData;
					vkUpdateDescriptorSets(pRenderer->mVulkan.pVkDevice, 1, writeSet, 0, NULL);
					++writeSet->dstArrayElement;
				}

				// Update done - Dont need this write set anymore. Return it to the array
				writeSet->pNext = NULL;
				--writeSetCount;

				break;
			}
#endif
			default: break;
		}
	}

	vkUpdateDescriptorSets(pRenderer->mVulkan.pVkDevice, writeSetCount, writeSetArray, 0, NULL);
}

static const uint32_t VK_MAX_ROOT_DESCRIPTORS = 32;

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
			if (pRootSignature->mVulkan.pEmptyDescriptorSet[setIndex] != VK_NULL_HANDLE)
			{
				vkCmdBindDescriptorSets(
					pCmd->mVulkan.pVkCmdBuf, gPipelineBindPoint[pRootSignature->mPipelineType], pRootSignature->mVulkan.pPipelineLayout,
					setIndex, 1, &pRootSignature->mVulkan.pEmptyDescriptorSet[setIndex], 0, NULL);
			}
		}
	}

	static uint32_t offsets[VK_MAX_ROOT_DESCRIPTORS] = {};

	vkCmdBindDescriptorSets(
		pCmd->mVulkan.pVkCmdBuf, gPipelineBindPoint[pRootSignature->mPipelineType], pRootSignature->mVulkan.pPipelineLayout,
		pDescriptorSet->mVulkan.mUpdateFrequency, 1, &pDescriptorSet->mVulkan.pHandles[index],
		pDescriptorSet->mVulkan.mDynamicOffsetCount, offsets);
}

void vk_cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
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

void vk_cmdBindDescriptorSetWithRootCbvs(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
	ASSERT(pCmd);
	ASSERT(pDescriptorSet);
	ASSERT(pParams);

	const RootSignature* pRootSignature = pDescriptorSet->mVulkan.pRootSignature;
	uint32_t offsets[VK_MAX_ROOT_DESCRIPTORS] = {};

	for (uint32_t i = 0; i < count; ++i)
	{
		const DescriptorData* pParam = pParams + i;
		uint32_t              paramIndex = pParam->mBindByIndex ? pParam->mIndex : UINT32_MAX;

		const DescriptorInfo* pDesc =
			(paramIndex != UINT32_MAX) ? (pRootSignature->pDescriptors + paramIndex) : get_descriptor(pRootSignature, pParam->pName);
		if (paramIndex != UINT32_MAX)
		{
			VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param index (%u)", paramIndex);
		}
		else
		{
			VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param name (%s)", pParam->pName);
		}

#if defined(ENABLE_GRAPHICS_DEBUG)
		const uint32_t maxRange = DESCRIPTOR_TYPE_UNIFORM_BUFFER == pDesc->mType ?    //-V522
			pCmd->pRenderer->mVulkan.pVkActiveGPUProperties->properties.limits.maxUniformBufferRange :
			pCmd->pRenderer->mVulkan.pVkActiveGPUProperties->properties.limits.maxStorageBufferRange;
#endif

		VALIDATE_DESCRIPTOR(pDesc->mRootDescriptor, "Descriptor (%s) - must be a root cbv", pDesc->pName);
		VALIDATE_DESCRIPTOR(pParam->mCount <= 1, "Descriptor (%s) - cmdBindDescriptorSetWithRootCbvs does not support arrays", pDesc->pName);
		VALIDATE_DESCRIPTOR(pParam->pRanges, "Descriptor (%s) - pRanges must be provided for cmdBindDescriptorSetWithRootCbvs", pDesc->pName);

		DescriptorDataRange range = pParam->pRanges[0];
		VALIDATE_DESCRIPTOR(range.mSize, "Descriptor (%s) - pRanges->mSize is zero", pDesc->pName);
		VALIDATE_DESCRIPTOR(
			range.mSize <= maxRange,
			"Descriptor (%s) - pRanges->mSize is %ull which exceeds max size %u", pDesc->pName, range.mSize,
			maxRange);

		offsets[pDesc->mHandleIndex] = range.mOffset; //-V522
		DynamicUniformData* pData = &pDescriptorSet->mVulkan.pDynamicUniformData[index * pDescriptorSet->mVulkan.mDynamicOffsetCount + pDesc->mHandleIndex];
		if (pData->pBuffer != pParam->ppBuffers[0]->mVulkan.pVkBuffer || range.mSize != pData->mSize)
		{
			*pData = { pParam->ppBuffers[0]->mVulkan.pVkBuffer, 0, range.mSize };

			VkDescriptorBufferInfo bufferInfo = { pData->pBuffer, 0, (VkDeviceSize)pData->mSize };
			VkWriteDescriptorSet writeSet = {};
			writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeSet.pNext = NULL;
			writeSet.descriptorCount = 1;
			writeSet.descriptorType = (VkDescriptorType)pDesc->mVulkan.mVkType;
			writeSet.dstArrayElement = 0;
			writeSet.dstBinding = pDesc->mVulkan.mReg;
			writeSet.dstSet = pDescriptorSet->mVulkan.pHandles[index];
			writeSet.pBufferInfo = &bufferInfo;
			vkUpdateDescriptorSets(pCmd->pRenderer->mVulkan.pVkDevice, 1, &writeSet, 0, NULL);
		}
	}

	vkCmdBindDescriptorSets(
		pCmd->mVulkan.pVkCmdBuf, gPipelineBindPoint[pRootSignature->mPipelineType], pRootSignature->mVulkan.pPipelineLayout,
		pDescriptorSet->mVulkan.mUpdateFrequency, 1, &pDescriptorSet->mVulkan.pHandles[index],
		pDescriptorSet->mVulkan.mDynamicOffsetCount, offsets);
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

	if (pDesc->mConstantCount)
	{
		totalSize += sizeof(VkSpecializationInfo);
		totalSize += sizeof(VkSpecializationMapEntry) * pDesc->mConstantCount;
		for (uint32_t i = 0; i < pDesc->mConstantCount; ++i)
		{
			const ShaderConstant* constant = &pDesc->pConstants[i];
			totalSize += (constant->mSize == sizeof(bool)) ? sizeof(VkBool32) : constant->mSize;
		}
	}

	totalSize += counter * sizeof(VkShaderModule);
	totalSize += counter * sizeof(char*);
	Shader* pShaderProgram = (Shader*)tf_calloc(1, totalSize);
	pShaderProgram->mStages = pDesc->mStages;
	pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1);    //-V1027
	pShaderProgram->mVulkan.pShaderModules = (VkShaderModule*)(pShaderProgram->pReflection + 1);
	pShaderProgram->mVulkan.pEntryNames = (char**)(pShaderProgram->mVulkan.pShaderModules + counter);
	pShaderProgram->mVulkan.pSpecializationInfo = NULL;

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
#ifdef VK_RAYTRACING_AVAILABLE
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

	// Fill specialization constant entries
	if (pDesc->mConstantCount)
	{
		pShaderProgram->mVulkan.pSpecializationInfo = (VkSpecializationInfo*)mem;
		mem += sizeof(VkSpecializationInfo);

		VkSpecializationMapEntry* mapEntries = (VkSpecializationMapEntry*)mem;
		mem += pDesc->mConstantCount * sizeof(VkSpecializationMapEntry);

		uint8_t* data = mem;
		uint32_t offset = 0;
		for (uint32_t i = 0; i < pDesc->mConstantCount; ++i)
		{
			const ShaderConstant* constant = &pDesc->pConstants[i];
			const bool boolType = constant->mSize == sizeof(bool);
			const uint32_t size = boolType ? sizeof(VkBool32) : constant->mSize;

			VkSpecializationMapEntry* entry = &mapEntries[i];
			entry->constantID = constant->mIndex;
			entry->offset = offset;
			entry->size = size;

			if (boolType)
			{
				*(VkBool32*)(data + offset) = *(const bool*)constant->pValue;
			}
			else
			{
				memcpy(data + offset, constant->pValue, constant->mSize);
			}
			offset += size;
		}

		VkSpecializationInfo* specializationInfo = pShaderProgram->mVulkan.pSpecializationInfo;
		specializationInfo->dataSize = offset;
		specializationInfo->mapEntryCount = pDesc->mConstantCount;
		specializationInfo->pData = data;
		specializationInfo->pMapEntries = mapEntries;
	}

	createPipelineReflection(stageReflections, counter, pShaderProgram->pReflection);

	addShaderDependencies(pShaderProgram, pDesc);

	*ppShaderProgram = pShaderProgram;
}

void vk_removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
	removeShaderDependencies(pShaderProgram);

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
#ifdef VK_RAYTRACING_AVAILABLE
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
	VkDescriptorSetLayoutBinding* mBindings = NULL;
	/// Array of all descriptors in this descriptor set
	DescriptorInfo** mDescriptors = NULL;
	/// Array of all descriptors marked as dynamic in this descriptor set (applicable to DESCRIPTOR_TYPE_UNIFORM_BUFFER)
	DescriptorInfo** mDynamicDescriptors = NULL;
} UpdateFrequencyLayoutInfo;

static bool compareDescriptorSetLayoutBinding(const VkDescriptorSetLayoutBinding* pLhs, const VkDescriptorSetLayoutBinding* pRhs)
{
	if (pRhs->descriptorType < pLhs->descriptorType)
		return true;
	else if (pRhs->descriptorType == pLhs->descriptorType && pRhs->binding < pLhs->binding)
		return true;

	return false;
}

typedef DescriptorInfo* PDescriptorInfo;
static bool comparePDescriptorInfo(const PDescriptorInfo* pLhs, const PDescriptorInfo* pRhs)
{
	return (*pLhs)->mVulkan.mReg < (*pRhs)->mVulkan.mReg;
}

DEFINE_SIMPLE_SORT_FUNCTION(static, simpleSortVkDescriptorSetLayoutBinding, VkDescriptorSetLayoutBinding, compareDescriptorSetLayoutBinding)
DEFINE_INSERTION_SORT_FUNCTION(static, stableSortVkDescriptorSetLayoutBinding, VkDescriptorSetLayoutBinding, compareDescriptorSetLayoutBinding, simpleSortVkDescriptorSetLayoutBinding)
DEFINE_PARTITION_IMPL_FUNCTION(static, partitionImplVkDescriptorSetLayoutBinding, VkDescriptorSetLayoutBinding, compareDescriptorSetLayoutBinding)
DEFINE_QUICK_SORT_IMPL_FUNCTION(static, quickSortImplVkDescriptorSetLayoutBinding, VkDescriptorSetLayoutBinding, compareDescriptorSetLayoutBinding, stableSortVkDescriptorSetLayoutBinding, partitionImplVkDescriptorSetLayoutBinding)
DEFINE_QUICK_SORT_FUNCTION(static, sortVkDescriptorSetLayoutBinding, VkDescriptorSetLayoutBinding, quickSortImplVkDescriptorSetLayoutBinding)

DEFINE_SIMPLE_SORT_FUNCTION(static, simpleSortPDescriptorInfo, PDescriptorInfo, comparePDescriptorInfo)
DEFINE_INSERTION_SORT_FUNCTION(static, stableSortPDescriptorInfo, PDescriptorInfo, comparePDescriptorInfo, simpleSortPDescriptorInfo)

void vk_addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
{
	ASSERT(pRenderer);
	ASSERT(pRootSignatureDesc);
	ASSERT(ppRootSignature);

	typedef struct StaticSamplerNode
	{
		char*		key;
		Sampler*	value;
	}StaticSamplerNode;

	static constexpr uint32_t	kMaxLayoutCount = DESCRIPTOR_UPDATE_FREQ_COUNT;
	UpdateFrequencyLayoutInfo	layouts[kMaxLayoutCount] = {};
	VkPushConstantRange			pushConstants[SHADER_STAGE_COUNT] = {};
	uint32_t					pushConstantCount = 0;
	ShaderResource*				shaderResources = NULL;
	StaticSamplerNode*			staticSamplerMap = NULL;
	sh_new_arena(staticSamplerMap);

	for (uint32_t i = 0; i < pRootSignatureDesc->mStaticSamplerCount; ++i)
	{
		ASSERT(pRootSignatureDesc->ppStaticSamplers[i]);
		shput(staticSamplerMap, pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i]);
	}

	PipelineType		pipelineType = PIPELINE_TYPE_UNDEFINED;
	DescriptorIndexMap*	indexMap = NULL;
	sh_new_arena(indexMap);

	// Collect all unique shader resources in the given shaders
	// Resources are parsed by name (two resources named "XYZ" in two shaders will be considered the same resource)
	for (uint32_t sh = 0; sh < pRootSignatureDesc->mShaderCount; ++sh)
	{
		PipelineReflection const* pReflection = pRootSignatureDesc->ppShaders[sh]->pReflection;

		if (pReflection->mShaderStages & SHADER_STAGE_COMP)
			pipelineType = PIPELINE_TYPE_COMPUTE;
#ifdef VK_RAYTRACING_AVAILABLE
		else if (pReflection->mShaderStages & SHADER_STAGE_RAYTRACING)
			pipelineType = PIPELINE_TYPE_RAYTRACING;
#endif
		else
			pipelineType = PIPELINE_TYPE_GRAPHICS;

		for (uint32_t i = 0; i < pReflection->mShaderResourceCount; ++i)
		{
			ShaderResource const* pRes = &pReflection->pShaderResources[i];
			// uint32_t              setIndex = pRes->set;

			// if (pRes->type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
			// 	setIndex = 0;

			DescriptorIndexMap* pNode = shgetp_null(indexMap, pRes->name);
			if (!pNode)
			{
				ShaderResource* pResource = NULL;
				for (ptrdiff_t i = 0; i < arrlen(shaderResources); ++i)
				{
					ShaderResource* pCurrent = &shaderResources[i];
					if (pCurrent->type == pRes->type &&
						(pCurrent->used_stages == pRes->used_stages) &&
						(((pCurrent->reg ^ pRes->reg) | (pCurrent->set ^ pRes->set)) == 0))
					{
						pResource = pCurrent;
						break;
					}

				}
				if (!pResource)
				{
					shput(indexMap, pRes->name, (uint32_t)arrlen(shaderResources));
					arrpush(shaderResources, *pRes);
				}
				else
				{
					ASSERT(pRes->type == pResource->type);
					if (pRes->type != pResource->type)
					{
						LOGF(
							LogLevel::eERROR,
							"\nFailed to create root signature\n"
							"Shared shader resources %s and %s have mismatching types (%u) and (%u). All shader resources "
							"sharing the same register and space addRootSignature "
							"must have the same type",
							pRes->name, pResource->name, (uint32_t)pRes->type, (uint32_t)pResource->type);
						return;
					}

					uint32_t value = shget(indexMap, pResource->name);
					shput(indexMap, pRes->name, value);
					pResource->used_stages |= pRes->used_stages;
				}
			}
			else
			{
				if (shaderResources[pNode->value].reg != pRes->reg) //-V::522, 595
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
				if (shaderResources[pNode->value].set != pRes->set) //-V::522, 595
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

	pRootSignature->pDescriptors = (DescriptorInfo*)(pRootSignature + 1);                                                        //-V1027
	pRootSignature->pDescriptorNameToIndexMap = indexMap;

	if (arrlen(shaderResources))
	{
		pRootSignature->mDescriptorCount = (uint32_t)arrlen(shaderResources);
	}

	pRootSignature->mPipelineType = pipelineType;

	// Fill the descriptor array to be stored in the root signature
	for (ptrdiff_t i = 0; i < arrlen(shaderResources); ++i)
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

			// If a user specified a uniform buffer to be used as a dynamic uniform buffer change its type to VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
			// Also log a message for debugging purpose
			if (isDescriptorRootCbv(pRes->name))
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

			// Find if the given descriptor is a static sampler
			StaticSamplerNode* it = shgetp_null(staticSamplerMap, pDesc->pName);
			if (it)
			{
				LOGF(LogLevel::eINFO, "Descriptor (%s) : User specified Static Sampler", pDesc->pName);
				binding.pImmutableSamplers = &it->value->mVulkan.pVkSampler;
			}

			// Set the index to an invalid value so we can use this later for error checking if user tries to update a static sampler
			// In case of Combined Image Samplers, skip invalidating the index
			// because we do not to introduce new ways to update the descriptor in the Interface
			if (it && pDesc->mType != DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
			{
				pDesc->mStaticSampler = true;
			}
			else
			{
				arrpush(layouts[setIndex].mDescriptors, pDesc);
			}

			if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
			{
				arrpush(layouts[setIndex].mDynamicDescriptors, pDesc);
				pDesc->mRootDescriptor = true;
			}

			arrpush(layouts[setIndex].mBindings, binding);

			// Update descriptor pool size for this descriptor type
			VkDescriptorPoolSize* poolSize = NULL;
			for (uint32_t i = 0; i < MAX_DESCRIPTOR_POOL_SIZE_ARRAY_COUNT; ++i)
			{
				if (binding.descriptorType == pRootSignature->mVulkan.mPoolSizes[setIndex][i].type && pRootSignature->mVulkan.mPoolSizes[setIndex][i].descriptorCount)
				{
					poolSize = &pRootSignature->mVulkan.mPoolSizes[setIndex][i];
					break;
				}
			}
			if (!poolSize)
			{
				poolSize = &pRootSignature->mVulkan.mPoolSizes[setIndex][pRootSignature->mVulkan.mPoolSizeCount[setIndex]++];
				poolSize->type = binding.descriptorType;
			}

			poolSize->descriptorCount += binding.descriptorCount;
		}
		// If descriptor is a root constant, add it to the root constant array
		else
		{
			LOGF(LogLevel::eINFO, "Descriptor (%s) : User specified Push Constant", pDesc->pName);

			pDesc->mRootDescriptor = true;
			pDesc->mVulkan.mVkStages = util_to_vk_shader_stage_flags(pRes->used_stages);
			setIndex = 0;
			pDesc->mHandleIndex = pushConstantCount++;

			pushConstants[pDesc->mHandleIndex] = {};
			pushConstants[pDesc->mHandleIndex].offset = 0;
			pushConstants[pDesc->mHandleIndex].size = pDesc->mSize;
			pushConstants[pDesc->mHandleIndex].stageFlags = pDesc->mVulkan.mVkStages;
		}
	}

	// Create descriptor layouts
	// Put least frequently changed params first
	for (uint32_t layoutIndex = kMaxLayoutCount; layoutIndex-- > 0U;)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[layoutIndex];

		if (arrlen(layouts[layoutIndex].mBindings))
		{
			// sort table by type (CBV/SRV/UAV) by register
			sortVkDescriptorSetLayoutBinding(layout.mBindings, arrlenu(layout.mBindings));
		}

		bool createLayout = arrlen(layout.mBindings) != 0;
		// Check if we need to create an empty layout in case there is an empty set between two used sets
		// Example: set = 0 is used, set = 2 is used. In this case, set = 1 needs to exist even if it is empty
		if (!createLayout && layoutIndex < kMaxLayoutCount - 1)
		{
			createLayout = pRootSignature->mVulkan.mVkDescriptorSetLayouts[layoutIndex + 1] != VK_NULL_HANDLE;
		}

		if (createLayout)
		{
			if (arrlen(layout.mBindings))
			{
				VkDescriptorSetLayoutCreateInfo layoutInfo = {};
				layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				layoutInfo.pNext = NULL;
				layoutInfo.bindingCount = (uint32_t)arrlen(layout.mBindings);
				layoutInfo.pBindings = layout.mBindings;
				layoutInfo.flags = 0;

				CHECK_VKRESULT(vkCreateDescriptorSetLayout(
					pRenderer->mVulkan.pVkDevice, &layoutInfo, &gVkAllocationCallbacks, &pRootSignature->mVulkan.mVkDescriptorSetLayouts[layoutIndex]));
			}
			else
			{
				pRootSignature->mVulkan.mVkDescriptorSetLayouts[layoutIndex] = pRenderer->mVulkan.pEmptyDescriptorSetLayout;
			}
		}

		if (!arrlen(layout.mBindings))
		{
			continue;
		}

		uint32_t cumulativeDescriptorCount = 0;

		for (ptrdiff_t descIndex = 0; descIndex < arrlen(layout.mDescriptors); ++descIndex)
		{
			DescriptorInfo* pDesc = layout.mDescriptors[descIndex];
			if (!pDesc->mRootDescriptor)
			{
				pDesc->mHandleIndex = cumulativeDescriptorCount;
				cumulativeDescriptorCount += pDesc->mSize;
			}
		}

		if (arrlen(layout.mDynamicDescriptors))
		{
			// vkCmdBindDescriptorSets - pDynamicOffsets - entries are ordered by the binding numbers in the descriptor set layouts
			stableSortPDescriptorInfo(layout.mDynamicDescriptors, arrlenu(layout.mDynamicDescriptors));

			pRootSignature->mVulkan.mVkDynamicDescriptorCounts[layoutIndex] = (uint32_t)arrlen(layout.mDynamicDescriptors);
			for (uint32_t descIndex = 0; descIndex < (uint32_t)arrlen(layout.mDynamicDescriptors); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mDynamicDescriptors[descIndex];
				pDesc->mHandleIndex = descIndex;
			}
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
	add_info.pushConstantRangeCount = pushConstantCount;
	add_info.pPushConstantRanges = pushConstants;
	CHECK_VKRESULT(vkCreatePipelineLayout(
		pRenderer->mVulkan.pVkDevice, &add_info, &gVkAllocationCallbacks, &(pRootSignature->mVulkan.pPipelineLayout)));
	/************************************************************************/
	// Update templates
	/************************************************************************/
	for (uint32_t setIndex = 0; setIndex < DESCRIPTOR_UPDATE_FREQ_COUNT; ++setIndex)
	{
		const UpdateFrequencyLayoutInfo& layout = layouts[setIndex];

		if (!arrlen(layout.mDescriptors) && pRootSignature->mVulkan.mVkDescriptorSetLayouts[setIndex] != VK_NULL_HANDLE)
		{
			pRootSignature->mVulkan.pEmptyDescriptorSet[setIndex] = pRenderer->mVulkan.pEmptyDescriptorSet;
			if (pRootSignature->mVulkan.mVkDescriptorSetLayouts[setIndex] != pRenderer->mVulkan.pEmptyDescriptorSetLayout)
			{
				add_descriptor_pool(pRenderer, 1, 0,
					pRootSignature->mVulkan.mPoolSizes[setIndex], pRootSignature->mVulkan.mPoolSizeCount[setIndex],
					&pRootSignature->mVulkan.pEmptyDescriptorPool[setIndex]);
				VkDescriptorSet* emptySet[] = { &pRootSignature->mVulkan.pEmptyDescriptorSet[setIndex] };
				consume_descriptor_sets(pRenderer->mVulkan.pVkDevice, pRootSignature->mVulkan.pEmptyDescriptorPool[setIndex],
					&pRootSignature->mVulkan.mVkDescriptorSetLayouts[setIndex],
					1, emptySet);
			}
		}
	}

	addRootSignatureDependencies(pRootSignature, pRootSignatureDesc);

	*ppRootSignature = pRootSignature;
	for (uint32_t i = 0; i < kMaxLayoutCount; ++i)
	{
		arrfree(layouts[i].mBindings);
		arrfree(layouts[i].mDescriptors);
		arrfree(layouts[i].mDynamicDescriptors);
	}
	arrfree(shaderResources);
	shfree(staticSamplerMap);
}

void vk_removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
	removeRootSignatureDependencies(pRootSignature);

	for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
	{
		if (pRootSignature->mVulkan.mVkDescriptorSetLayouts[i] != pRenderer->mVulkan.pEmptyDescriptorSetLayout)
		{
			vkDestroyDescriptorSetLayout(
				pRenderer->mVulkan.pVkDevice, pRootSignature->mVulkan.mVkDescriptorSetLayouts[i], &gVkAllocationCallbacks);
		}
		if (VK_NULL_HANDLE != pRootSignature->mVulkan.pEmptyDescriptorPool[i])
		{
			vkDestroyDescriptorPool(
				pRenderer->mVulkan.pVkDevice, pRootSignature->mVulkan.pEmptyDescriptorPool[i], &gVkAllocationCallbacks);
		}
	}

	shfree(pRootSignature->pDescriptorNameToIndexMap);

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
	RenderPassDesc renderPassDesc = {};
	RenderPass     renderPass = {};
	renderPassDesc.mRenderTargetCount = pDesc->mRenderTargetCount;
	renderPassDesc.pColorFormats = pDesc->pColorFormats;
	renderPassDesc.mSampleCount = pDesc->mSampleCount;
	renderPassDesc.mDepthStencilFormat = pDesc->mDepthStencilFormat;
    renderPassDesc.mVRMultiview = pDesc->pShaderProgram->mIsMultiviewVR;
    renderPassDesc.mVRFoveatedRendering = pDesc->mVRFoveatedRendering;
	renderPassDesc.pLoadActionsColor = gDefaultLoadActions;
	renderPassDesc.mLoadActionDepth = gDefaultLoadActions[0];
	renderPassDesc.mLoadActionStencil = gDefaultLoadActions[1];
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
	renderPassDesc.pStoreActionsColor = pDesc->pColorResolveActions ? pDesc->pColorResolveActions : gDefaultStoreActions;
#else
	renderPassDesc.pStoreActionsColor = gDefaultStoreActions;
#endif
	renderPassDesc.mStoreActionDepth = gDefaultStoreActions[0];
	renderPassDesc.mStoreActionStencil = gDefaultStoreActions[1];
	add_render_pass(pRenderer, &renderPassDesc, &renderPass);

	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);
	for (uint32_t i = 0; i < pShaderProgram->pReflection->mStageReflectionCount; ++i)
		ASSERT(VK_NULL_HANDLE != pShaderProgram->mVulkan.pShaderModules[i]);

	const VkSpecializationInfo* specializationInfo = pShaderProgram->mVulkan.pSpecializationInfo;

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
				stages[stage_count].pSpecializationInfo = specializationInfo;
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
		add_info.renderPass = renderPass.pRenderPass;
		add_info.subpass = 0;
		add_info.basePipelineHandle = VK_NULL_HANDLE;
		add_info.basePipelineIndex = -1;
		CHECK_VKRESULT(vkCreateGraphicsPipelines(
			pRenderer->mVulkan.pVkDevice, psoCache, 1, &add_info, &gVkAllocationCallbacks, &(pPipeline->mVulkan.pVkPipeline)));

		remove_render_pass(pRenderer, &renderPass);
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
		stage.pSpecializationInfo = pDesc->pShaderProgram->mVulkan.pSpecializationInfo;

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
#ifdef VK_RAYTRACING_AVAILABLE
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

	addPipelineDependencies(*ppPipeline, pDesc);

	if (*ppPipeline && pDesc->pName)
	{
		setPipelineName(pRenderer, *ppPipeline, pDesc->pName);
	}
}

void vk_removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
	removePipelineDependencies(pPipeline);

	ASSERT(pRenderer);
	ASSERT(pPipeline);
	ASSERT(VK_NULL_HANDLE != pRenderer->mVulkan.pVkDevice);
	ASSERT(VK_NULL_HANDLE != pPipeline->mVulkan.pVkPipeline);

#ifdef VK_RAYTRACING_AVAILABLE
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

	CHECK_VKRESULT(vkResetCommandPool(pRenderer->mVulkan.pVkDevice, pCmdPool->pVkCmdPool, 0));
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

	CHECK_VKRESULT(vkBeginCommandBuffer(pCmd->mVulkan.pVkCmdBuf, &begin_info));

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

	CHECK_VKRESULT(vkEndCommandBuffer(pCmd->mVulkan.pVkCmdBuf));
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
	StoreActionType colorStoreAction[MAX_RENDER_TARGET_ATTACHMENTS] = {};
	StoreActionType depthStoreAction = {};
	StoreActionType stencilStoreAction = {};
	uint32_t frameBufferRenderTargetCount = 0;

	// Generate hash for render pass and frame buffer
	// NOTE:
	// Render pass does not care about underlying VkImageView. It only cares about the format and sample count of the attachments.
	// We hash those two values to generate render pass hash
	// Frame buffer is the actual array of all the VkImageViews
	// We hash the texture id associated with the render target to generate frame buffer hash
	for (uint32_t i = 0; i < renderTargetCount; ++i)
	{
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
		bool resolveAttachment = pLoadActions && IsStoreActionResolve(pLoadActions->mStoreActionsColor[i]);
		if (resolveAttachment)
		{
			colorStoreAction[i] = ppRenderTargets[i]->pTexture->mLazilyAllocated ? STORE_ACTION_RESOLVE_DONTCARE : STORE_ACTION_DONTCARE;
		}
		else
#endif
		{
			colorStoreAction[i] = ppRenderTargets[i]->pTexture->mLazilyAllocated ? STORE_ACTION_DONTCARE :
				(pLoadActions ? pLoadActions->mStoreActionsColor[i] : gDefaultStoreActions[i]);
		}

		uint32_t renderPassHashValues[] =
		{
			(uint32_t)ppRenderTargets[i]->mFormat,
			(uint32_t)ppRenderTargets[i]->mSampleCount,
			pLoadActions ? (uint32_t)pLoadActions->mLoadActionsColor[i] : gDefaultLoadActions[i],
			(uint32_t)colorStoreAction[i]
		};
		uint32_t frameBufferHashValues[] =
		{
			ppRenderTargets[i]->mVulkan.mId,
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
			resolveAttachment ? ppRenderTargets[i]->pResolveAttachment->mVulkan.mId : 0
#endif
		};
		renderPassHash = tf_mem_hash<uint32_t>(renderPassHashValues, TF_ARRAY_COUNT(renderPassHashValues), renderPassHash);
		frameBufferHash = tf_mem_hash<uint32_t>(frameBufferHashValues, TF_ARRAY_COUNT(frameBufferHashValues), frameBufferHash);
		vrFoveatedRendering |= ppRenderTargets[i]->mVRFoveatedRendering;

		++frameBufferRenderTargetCount;
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
		frameBufferRenderTargetCount += resolveAttachment ? 1 : 0;
#endif
	}
	if (pDepthStencil)
	{
		depthStoreAction = pDepthStencil->pTexture->mLazilyAllocated ? STORE_ACTION_DONTCARE :
			(pLoadActions ? pLoadActions->mStoreActionDepth : gDefaultStoreActions[0]);
		stencilStoreAction = pDepthStencil->pTexture->mLazilyAllocated ? STORE_ACTION_DONTCARE :
			(pLoadActions ? pLoadActions->mStoreActionStencil : gDefaultStoreActions[0]);

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
		// Dont support depth stencil auto resolve
		ASSERT(!(IsStoreActionResolve(depthStoreAction) || IsStoreActionResolve(stencilStoreAction)));
#endif

		uint32_t hashValues[] =
		{
			(uint32_t)pDepthStencil->mFormat,
			(uint32_t)pDepthStencil->mSampleCount,
			pLoadActions ? (uint32_t)pLoadActions->mLoadActionDepth : gDefaultLoadActions[0],
			pLoadActions ? (uint32_t)pLoadActions->mLoadActionStencil : gDefaultLoadActions[0],
			(uint32_t)depthStoreAction,
			(uint32_t)stencilStoreAction,
		};
		renderPassHash = tf_mem_hash<uint32_t>(hashValues, 6, renderPassHash);
		frameBufferHash = tf_mem_hash<uint32_t>(&pDepthStencil->mVulkan.mId, 1, frameBufferHash);
		vrFoveatedRendering |= pDepthStencil->mVRFoveatedRendering;
	}
	if (pColorArraySlices)
		frameBufferHash = tf_mem_hash<uint32_t>(pColorArraySlices, renderTargetCount, frameBufferHash);
	if (pColorMipSlices)
		frameBufferHash = tf_mem_hash<uint32_t>(pColorMipSlices, renderTargetCount, frameBufferHash);
	if (depthArraySlice != UINT32_MAX)
		frameBufferHash = tf_mem_hash<uint32_t>(&depthArraySlice, 1, frameBufferHash);
	if (depthMipSlice != UINT32_MAX)
		frameBufferHash = tf_mem_hash<uint32_t>(&depthMipSlice, 1, frameBufferHash);

	SampleCount sampleCount = SAMPLE_COUNT_1;

	// Need pointer to pointer in order to reassign hash map when it is resized
	RenderPassNode** pRenderPassMap = get_render_pass_map(pCmd->pRenderer->mUnlinkedRendererIndex);
	FrameBufferNode** pFrameBufferMap = get_frame_buffer_map(pCmd->pRenderer->mUnlinkedRendererIndex);

	RenderPassNode*	 pNode = hmgetp_null(*pRenderPassMap, renderPassHash);
	FrameBufferNode* pFrameBufferNode = hmgetp_null(*pFrameBufferMap, frameBufferHash);

	// If a render pass of this combination already exists just use it or create a new one
	if (!pNode)
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

		RenderPass renderPass = {};
		RenderPassDesc renderPassDesc = {};
		renderPassDesc.mRenderTargetCount = renderTargetCount;
		renderPassDesc.mSampleCount = sampleCount;
		renderPassDesc.pColorFormats = colorFormats;
		renderPassDesc.mDepthStencilFormat = depthStencilFormat;
		renderPassDesc.pLoadActionsColor = pLoadActions ? pLoadActions->mLoadActionsColor : gDefaultLoadActions;
		renderPassDesc.mLoadActionDepth = pLoadActions ? pLoadActions->mLoadActionDepth : gDefaultLoadActions[0];
		renderPassDesc.mLoadActionStencil = pLoadActions ? pLoadActions->mLoadActionStencil : gDefaultLoadActions[0];
		renderPassDesc.pStoreActionsColor = colorStoreAction;
		renderPassDesc.mStoreActionDepth = depthStoreAction;
		renderPassDesc.mStoreActionStencil = stencilStoreAction;
		renderPassDesc.mVRMultiview = vrMultiview;
		renderPassDesc.mVRFoveatedRendering = vrFoveatedRendering;
		add_render_pass(pCmd->pRenderer, &renderPassDesc, &renderPass);

		// No need of a lock here since this map is per thread
		hmput(*pRenderPassMap, renderPassHash, renderPass);

		pNode = hmgetp_null(*pRenderPassMap, renderPassHash);
	}

	RenderPass* pRenderPass = &pNode->value;

	// If a frame buffer of this combination already exists just use it or create a new one
	if (!pFrameBufferNode)
	{
		FrameBuffer frameBuffer = {};
		FrameBufferDesc desc = {};
		desc.mRenderTargetCount = renderTargetCount;
		desc.pDepthStencil = pDepthStencil;
		desc.ppRenderTargets = ppRenderTargets;
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
		desc.pRenderTargetResolveActions = colorStoreAction;
#endif
		desc.pRenderPass = pRenderPass;
		desc.pColorArraySlices = pColorArraySlices;
		desc.pColorMipSlices = pColorMipSlices;
		desc.mDepthArraySlice = depthArraySlice;
		desc.mDepthMipSlice = depthMipSlice;
		desc.mVRFoveatedRendering = vrFoveatedRendering;
		add_framebuffer(pCmd->pRenderer, &desc, &frameBuffer);

		// No need of a lock here since this map is per thread
		hmput(*pFrameBufferMap, frameBufferHash, frameBuffer);

		pFrameBufferNode = hmgetp_null(*pFrameBufferMap, frameBufferHash);
	}

	FrameBuffer* pFrameBuffer = &pFrameBufferNode->value;

	DECLARE_ZERO(VkRect2D, render_area);
	render_area.offset.x = 0;
	render_area.offset.y = 0;
	render_area.extent.width = pFrameBuffer->mWidth;
	render_area.extent.height = pFrameBuffer->mHeight;

	uint32_t     clearValueCount = frameBufferRenderTargetCount;
	VkClearValue clearValues[VK_MAX_ATTACHMENT_ARRAY_COUNT] = {};
	if (pLoadActions)
	{
		for (uint32_t i = 0; i < renderTargetCount; ++i)
		{
			ClearValue clearValue = pLoadActions->mClearColorValues[i];
			clearValues[i].color = { { clearValue.r, clearValue.g, clearValue.b, clearValue.a } };
		}
		if (pDepthStencil)
		{
			clearValues[frameBufferRenderTargetCount].depthStencil = { pLoadActions->mClearDepth.depth, pLoadActions->mClearDepth.stencil };
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

			if (pTrans->mAcquire && pTrans->mCurrentState != RESOURCE_STATE_UNDEFINED)
			{
				pImageBarrier->srcQueueFamilyIndex = pCmd->pRenderer->mVulkan.mQueueFamilyIndices[pTrans->mQueueType];
				pImageBarrier->dstQueueFamilyIndex = pCmd->pQueue->mVulkan.mVkQueueFamilyIndex;
			}
			else if (pTrans->mRelease && pTrans->mCurrentState != RESOURCE_STATE_UNDEFINED)
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

			if (pTrans->mAcquire && pTrans->mCurrentState != RESOURCE_STATE_UNDEFINED)
			{
				pImageBarrier->srcQueueFamilyIndex = pCmd->pRenderer->mVulkan.mQueueFamilyIndices[pTrans->mQueueType];
				pImageBarrier->dstQueueFamilyIndex = pCmd->pQueue->mVulkan.mVkQueueFamilyIndex;
			}
			else if (pTrans->mRelease && pTrans->mCurrentState != RESOURCE_STATE_UNDEFINED)
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

void vk_cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const SubresourceDataDesc* pSubresourceDesc)
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

		vkCmdCopyImageToBuffer(
			pCmd->mVulkan.pVkCmdBuf, pTexture->mVulkan.pVkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pDstBuffer->mVulkan.pVkBuffer, 1,
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

		vkCmdCopyImageToBuffer(
			pCmd->mVulkan.pVkCmdBuf, pTexture->mVulkan.pVkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pDstBuffer->mVulkan.pVkBuffer,
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

		// Commonly returned immediately following swapchain resize. 
		// Vulkan spec states that this return value constitutes a successful call to vkAcquireNextImageKHR
		// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkAcquireNextImageKHR.html
		if (vk_res == VK_SUBOPTIMAL_KHR)
		{
			LOGF(LogLevel::eINFO, "vkAcquireNextImageKHR returned VK_SUBOPTIMAL_KHR. If window was just resized, ignore this message."); 
			pSignalSemaphore->mVulkan.mSignaled = true;
			return; 
		}

		CHECK_VKRESULT(vk_res);
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
	CHECK_VKRESULT(vkQueueSubmit(pQueue->mVulkan.pVkQueue, 1, &submit_info, pFence ? pFence->mVulkan.pVkFence : VK_NULL_HANDLE));

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
			ResetDesc resetDesc;
			resetDesc.mType = RESET_TYPE_DEVICE_LOST;
			requestReset(&resetDesc);
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
#if defined(ENABLE_NSIGHT_AFTERMATH)
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
		CHECK_VKRESULT(vkWaitForFences(pRenderer->mVulkan.pVkDevice, numValidFences, fences, VK_TRUE, UINT64_MAX));
#endif
		CHECK_VKRESULT(vkResetFences(pRenderer->mVulkan.pVkDevice, numValidFences, fences));
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
TinyImageFormat vk_getRecommendedSwapchainFormat(bool hintHDR, bool hintSRGB)
{
	//TODO: figure out this properly. BGRA not supported on android
#if !defined(VK_USE_PLATFORM_ANDROID_KHR) && !defined(VK_USE_PLATFORM_VI_NN)
	if (hintSRGB)
		return TinyImageFormat_B8G8R8A8_SRGB;
	else
		return TinyImageFormat_B8G8R8A8_UNORM;
#else
	if (hintSRGB)
		return TinyImageFormat_R8G8B8A8_SRGB;
	else
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
	ASSERT(pDesc->mIndirectArgCount == 1);
	
	CommandSignature* pCommandSignature =
		(CommandSignature*)tf_calloc(1, sizeof(CommandSignature) + sizeof(IndirectArgument) * pDesc->mIndirectArgCount);
	ASSERT(pCommandSignature);
	
	pCommandSignature->mDrawType = pDesc->pArgDescs[0].mType;
	switch (pDesc->pArgDescs[0].mType)
	{
		case INDIRECT_DRAW:
			pCommandSignature->mStride += sizeof(IndirectDrawArguments);
			break;
		case INDIRECT_DRAW_INDEX:
			pCommandSignature->mStride += sizeof(IndirectDrawIndexArguments);
			break;
		case INDIRECT_DISPATCH:
			pCommandSignature->mStride += sizeof(IndirectDispatchArguments);
			break;
		default:
			ASSERT(false);
			break;
	}

	if (!pDesc->mPacked)
	{
		pCommandSignature->mStride = round_up(pCommandSignature->mStride, 16);
	}

	*ppCommandSignature = pCommandSignature;
}

void vk_removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
{
	ASSERT(pRenderer);
	SAFE_FREE(pCommandSignature);
}

void vk_cmdExecuteIndirect(
	Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
	Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
	PFN_vkCmdDrawIndirect drawIndirect = (pCommandSignature->mDrawType == INDIRECT_DRAW) ? vkCmdDrawIndirect : vkCmdDrawIndexedIndirect;
#ifndef NX64
	decltype(pfnVkCmdDrawIndirectCountKHR) drawIndirectCount = (pCommandSignature->mDrawType == INDIRECT_DRAW) ? pfnVkCmdDrawIndirectCountKHR : pfnVkCmdDrawIndexedIndirectCountKHR;
#endif

	if (pCommandSignature->mDrawType == INDIRECT_DRAW || pCommandSignature->mDrawType == INDIRECT_DRAW_INDEX)
	{
		if (pCmd->pRenderer->pActiveGpuSettings->mMultiDrawIndirect)
		{
#ifndef NX64
			if (pCounterBuffer && drawIndirectCount)
			{
				drawIndirectCount(
					pCmd->mVulkan.pVkCmdBuf, pIndirectBuffer->mVulkan.pVkBuffer, bufferOffset, pCounterBuffer->mVulkan.pVkBuffer,
					counterBufferOffset, maxCommandCount, pCommandSignature->mStride);
			}
			else
#endif
			{
				drawIndirect(
					pCmd->mVulkan.pVkCmdBuf, pIndirectBuffer->mVulkan.pVkBuffer, bufferOffset, maxCommandCount, pCommandSignature->mStride);
			}
		}
		else
		{
			// Cannot use counter buffer when MDI is not supported. We will blindly loop through maxCommandCount
			ASSERT(!pCounterBuffer);

			for (uint32_t cmd = 0; cmd < maxCommandCount; ++cmd)
			{
				drawIndirect(
					pCmd->mVulkan.pVkCmdBuf, pIndirectBuffer->mVulkan.pVkBuffer, bufferOffset + cmd * pCommandSignature->mStride, 1, pCommandSignature->mStride);
			}
		}
	}
	else if (pCommandSignature->mDrawType == INDIRECT_DISPATCH)
	{
		for (uint32_t i = 0; i < maxCommandCount; ++i)
		{
			vkCmdDispatchIndirect(pCmd->mVulkan.pVkCmdBuf, pIndirectBuffer->mVulkan.pVkBuffer, bufferOffset + i * pCommandSignature->mStride);
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
				pCmd->mVulkan.pVkCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pQueryPool->mVulkan.pVkQueryPool, pQuery->mIndex);
			break;
		case VK_QUERY_TYPE_PIPELINE_STATISTICS: break;
		case VK_QUERY_TYPE_OCCLUSION: break;
		default: break;
	}
}

void vk_cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
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

void vk_cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
{
	VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT;
	vkCmdCopyQueryPoolResults(
		pCmd->mVulkan.pVkCmdBuf, pQueryPool->mVulkan.pVkQueryPool, startQuery, queryCount, pReadbackBuffer->mVulkan.pVkBuffer, 0,
		sizeof(uint64_t), flags);
}
/************************************************************************/
// Memory Stats Implementation
/************************************************************************/
void vk_calculateMemoryStats(Renderer* pRenderer, char** stats)
{
	vmaBuildStatsString(pRenderer->mVulkan.pVmaAllocator, stats, VK_TRUE);
}

void vk_freeMemoryStats(Renderer* pRenderer, char* stats)
{
	vmaFreeStatsString(pRenderer->mVulkan.pVmaAllocator, stats);
}

void vk_calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
{
	VmaTotalStatistics stats = {};
	vmaCalculateStatistics(pRenderer->mVulkan.pVmaAllocator, &stats);
	*usedBytes = stats.total.statistics.allocationBytes;
	*totalAllocatedBytes = stats.total.statistics.blockBytes;
}
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void vk_cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	if (pCmd->pRenderer->mVulkan.mDebugMarkerSupport)
	{
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
		VkDebugUtilsLabelEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		markerInfo.color[0] = r;
		markerInfo.color[1] = g;
		markerInfo.color[2] = b;
		markerInfo.color[3] = 1.0f;
		markerInfo.pLabelName = pName;
		vkCmdBeginDebugUtilsLabelEXT(pCmd->mVulkan.pVkCmdBuf, &markerInfo);
#elif !defined(NX64) || !defined(ENABLE_RENDER_DOC)
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
	if (pCmd->pRenderer->mVulkan.mDebugMarkerSupport)
	{
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
		vkCmdEndDebugUtilsLabelEXT(pCmd->mVulkan.pVkCmdBuf);
#elif !defined(NX64) || !defined(ENABLE_RENDER_DOC)
		vkCmdDebugMarkerEndEXT(pCmd->mVulkan.pVkCmdBuf);
#endif
	}
}

void vk_cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	if (pCmd->pRenderer->mVulkan.mDebugMarkerSupport)
	{
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
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

#if defined(ENABLE_NSIGHT_AFTERMATH)
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
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
void util_set_object_name(VkDevice pDevice, uint64_t handle, VkObjectType type, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	VkDebugUtilsObjectNameInfoEXT nameInfo = {};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = type;
	nameInfo.objectHandle = handle;
	nameInfo.pObjectName = pName;
	vkSetDebugUtilsObjectNameEXT(pDevice, &nameInfo);
#endif
}
#else
void util_set_object_name(VkDevice pDevice, uint64_t handle, VkDebugReportObjectTypeEXT type, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	VkDebugMarkerObjectNameInfoEXT nameInfo = {};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = type;
	nameInfo.object = (uint64_t)handle;
	nameInfo.pObjectName = pName;
	vkDebugMarkerSetObjectNameEXT(pDevice, &nameInfo);
#endif
}
#endif

void vk_setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
	ASSERT(pRenderer);
	ASSERT(pBuffer);
	ASSERT(pName);

	if (pRenderer->mVulkan.mDebugMarkerSupport)
	{
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
		util_set_object_name(pRenderer->mVulkan.pVkDevice, (uint64_t)pBuffer->mVulkan.pVkBuffer, VK_OBJECT_TYPE_BUFFER, pName);
#else
		util_set_object_name(pRenderer->mVulkan.pVkDevice, (uint64_t)pBuffer->mVulkan.pVkBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pName);
#endif
	}
}

void vk_setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
	ASSERT(pRenderer);
	ASSERT(pTexture);
	ASSERT(pName);

	if (pRenderer->mVulkan.mDebugMarkerSupport)
	{
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
		util_set_object_name(pRenderer->mVulkan.pVkDevice, (uint64_t)pTexture->mVulkan.pVkImage, VK_OBJECT_TYPE_IMAGE, pName);
#else
		util_set_object_name(pRenderer->mVulkan.pVkDevice, (uint64_t)pTexture->mVulkan.pVkImage, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, pName);
#endif
	}
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

	if (pRenderer->mVulkan.mDebugMarkerSupport)
	{
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
		util_set_object_name(pRenderer->mVulkan.pVkDevice, (uint64_t)pPipeline->mVulkan.pVkPipeline, VK_OBJECT_TYPE_PIPELINE, pName);
#else
		util_set_object_name(
			pRenderer->mVulkan.pVkDevice, (uint64_t)pPipeline->mVulkan.pVkPipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, pName);
#endif
	}
}
/************************************************************************/
// Virtual Texture
/************************************************************************/
static void alignedDivision(const VkExtent3D& extent, const VkExtent3D& granularity, VkExtent3D* out)
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
	for (uint removePageIndex = 0; removePageIndex < removePageCount; ++removePageIndex)
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

	for (uint32_t i = 0; i < pTexture->pSvt->mVirtualPageTotalCount; i++)
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
	ASSERT(VK_FORMAT_UNDEFINED != format);
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

#if defined(ENABLE_GRAPHICS_DEBUG)
	if (pDesc->pName)
	{
		setTextureName(pRenderer, pTexture, pDesc->pName);
	}
#endif

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
	cmdCopySubresource = vk_cmdCopySubresource;
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
	cmdBindDescriptorSetWithRootCbvs = vk_cmdBindDescriptorSetWithRootCbvs;
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


void initVulkanRendererContext(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext)
{
	// No need to initialize API function pointers, initRenderer MUST be called before using anything else anyway.
	vk_initRendererContext(appName, pSettings, ppContext);
}

void exitVulkanRendererContext(RendererContext* pContext)
{
	ASSERT(pContext);

	vk_exitRendererContext(pContext);
}

#if !defined(NX64)
#include "../ThirdParty/OpenSource/volk/volk.c"
#if defined(VK_USE_DISPATCH_TABLES)
#include "../ThirdParty/OpenSource/volk/volkForgeExt.c"
#endif
#endif
#endif

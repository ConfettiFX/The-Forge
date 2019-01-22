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

#ifdef VULKAN

#define RENDERER_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#define MAX_FRAMES_IN_FLIGHT 3U
/************************************************************************/
// Debugging Macros
/************************************************************************/
// Uncomment this to enable render doc capture support
//#define USE_RENDER_DOC

// Debug Utils Extension is still WIP and does not work with all Validation Layers
// Disable this to use the old debug report and debug marker extensions
// Debug Utils requires the Nightly Build of RenderDoc
#define USE_DEBUG_UTILS_EXTENSION
/************************************************************************/
/************************************************************************/
#if defined(_WIN32)
// Pull in minimal Windows headers
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#ifdef FORGE_JHABLE_EDITS_V01
// These are placed separately in Vulkan.cpp and Direct3D12.cpp, but they should be in one place
// with an option for the user to append custom attributes. However, it should be initialized once
// so that we can parse them during shader reflection, not during pipeline binding.
// clang-format off
static const char * g_hackSemanticList[] =
{
	"POSITION",
	"NORMAL",
	"UV",
	"COLOR",
	"TANGENT",
	"BINORMAL",
	"TANGENT_TW",
	"TEXCOORD",
};
// clang-format on
#endif

#if defined(__linux__)
#define stricmp(a, b) strcasecmp(a, b)
#define vsprintf_s vsnprintf
#define strncpy_s strncpy
#define vsnprintf_s vsnprintf
#endif

#include "../IRenderer.h"
#include "../../ThirdParty/OpenSource/TinySTL/hash.h"
#include "../../OS/Interfaces/ILogManager.h"
#include "../../ThirdParty/OpenSource/VulkanMemoryAllocator/VulkanMemoryAllocator.h"
#include "../../OS/Core/GPUConfig.h"

#include "../../OS/Interfaces/IMemoryManager.h"

extern void
			vk_createShaderReflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection);
extern long vk_createBuffer(
	MemoryAllocator* pAllocator, const BufferCreateInfo* pCreateInfo, const AllocatorMemoryRequirements* pMemoryRequirements,
	Buffer* pBuffer);
extern void vk_destroyBuffer(MemoryAllocator* pAllocator, struct Buffer* pBuffer);
extern long vk_createTexture(
	MemoryAllocator* pAllocator, const TextureCreateInfo* pCreateInfo, const AllocatorMemoryRequirements* pMemoryRequirements,
	Texture* pTexture);
extern void vk_destroyTexture(MemoryAllocator* pAllocator, struct Texture* pTexture);

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

static const VkFormat gVkFormatTranslator[] = {
	VK_FORMAT_UNDEFINED,

	VK_FORMAT_R8_UNORM,
	VK_FORMAT_R8G8_UNORM,
	VK_FORMAT_UNDEFINED,  // RGB8 not directly supported
	VK_FORMAT_R8G8B8A8_UNORM,

	VK_FORMAT_R16_UNORM,
	VK_FORMAT_R16G16_UNORM,
	VK_FORMAT_UNDEFINED,  // RGB16 not directly supported
	VK_FORMAT_R16G16B16A16_UNORM,

	VK_FORMAT_R8_SNORM,
	VK_FORMAT_R8G8_SNORM,
	VK_FORMAT_UNDEFINED,  // RGB8S not directly supported
	VK_FORMAT_R8G8B8A8_SNORM,

	VK_FORMAT_R16_SNORM,
	VK_FORMAT_R16G16_SNORM,
	VK_FORMAT_UNDEFINED,  // RGB16S not directly supported
	VK_FORMAT_R16G16B16A16_SNORM,

	VK_FORMAT_R16_SFLOAT,
	VK_FORMAT_R16G16_SFLOAT,
	VK_FORMAT_UNDEFINED,  // RGB16F not directly supported
	VK_FORMAT_R16G16B16A16_SFLOAT,

	VK_FORMAT_R32_SFLOAT,
	VK_FORMAT_R32G32_SFLOAT,
	VK_FORMAT_R32G32B32_SFLOAT,
	VK_FORMAT_R32G32B32A32_SFLOAT,

	VK_FORMAT_R16_SINT,
	VK_FORMAT_R16G16_SINT,
	VK_FORMAT_UNDEFINED,  // RGB16I not directly supported
	VK_FORMAT_R16G16B16A16_SINT,

	VK_FORMAT_R32_SINT,
	VK_FORMAT_R32G32_SINT,
	VK_FORMAT_R32G32B32_SINT,
	VK_FORMAT_R32G32B32A32_SINT,

	VK_FORMAT_R16_UINT,
	VK_FORMAT_R16G16_UINT,
	VK_FORMAT_UNDEFINED,  // RGB16UI not directly supported
	VK_FORMAT_R16G16B16A16_UINT,

	VK_FORMAT_R32_UINT,
	VK_FORMAT_R32G32_UINT,
	VK_FORMAT_R32G32B32_UINT,
	VK_FORMAT_R32G32B32A32_UINT,

	VK_FORMAT_UNDEFINED,  // RGBE8 not directly supported
	VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
	VK_FORMAT_B10G11R11_UFLOAT_PACK32, // order switched in vulkan compared to DX11/12
	VK_FORMAT_B5G6R5_UNORM_PACK16,
	VK_FORMAT_UNDEFINED,  // RGBA4 not directly supported
	VK_FORMAT_A2R10G10B10_UNORM_PACK32,// order switched in vulkan compared to DX11/12

	VK_FORMAT_D16_UNORM,
	VK_FORMAT_D24_UNORM_S8_UINT,
	VK_FORMAT_D24_UNORM_S8_UINT,
	VK_FORMAT_D32_SFLOAT,

	VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
	VK_FORMAT_BC2_UNORM_BLOCK,
	VK_FORMAT_BC3_UNORM_BLOCK,
	VK_FORMAT_BC4_UNORM_BLOCK,
	VK_FORMAT_BC5_UNORM_BLOCK,
	// PVR formats
	VK_FORMAT_UNDEFINED, // PVR_2BPP = 56,
	VK_FORMAT_UNDEFINED, // PVR_2BPPA = 57,
	VK_FORMAT_UNDEFINED, // PVR_4BPP = 58,
	VK_FORMAT_UNDEFINED, // PVR_4BPPA = 59,
	VK_FORMAT_UNDEFINED, // INTZ = 60,  //  NVidia hack. Supported on all DX10+ HW
	//  XBox 360 specific fron buffer formats. NOt listed in other renderers. Please, add them when extend this structure.
	VK_FORMAT_UNDEFINED, // LE_XRGB8 = 61,
	VK_FORMAT_UNDEFINED, // LE_ARGB8 = 62,
	VK_FORMAT_UNDEFINED, // LE_X2RGB10 = 63,
	VK_FORMAT_UNDEFINED, // LE_A2RGB10 = 64,
	// compressed mobile forms
	VK_FORMAT_UNDEFINED, // ETC1 = 65,  //  RGB
	VK_FORMAT_UNDEFINED, // ATC = 66,   //  RGB
	VK_FORMAT_UNDEFINED, // ATCA = 67,  //  RGBA, explicit alpha
	VK_FORMAT_UNDEFINED, // ATCI = 68,  //  RGBA, interpolated alpha
	VK_FORMAT_UNDEFINED, // RAWZ = 69, //depth only, Nvidia (requires recombination of data) //FIX IT: PS3 as well?
	VK_FORMAT_UNDEFINED, // DF16 = 70, //depth only, Intel/AMD
	VK_FORMAT_UNDEFINED, // STENCILONLY = 71, // stencil ony usage
	VK_FORMAT_UNDEFINED, // GNF_BC1 = 72,
	VK_FORMAT_UNDEFINED, // GNF_BC2 = 73,
	VK_FORMAT_UNDEFINED, // GNF_BC3 = 74,
	VK_FORMAT_UNDEFINED, // GNF_BC4 = 75,
	VK_FORMAT_UNDEFINED, // GNF_BC5 = 76,
#ifdef FORGE_JHABLE_EDITS_V01
							// this is incorrect, because we need separate enums for signed float and unsigned float. for now, just pretend it's a signed float
	VK_FORMAT_BC6H_SFLOAT_BLOCK, // GNF_BC6 = 77,
	VK_FORMAT_BC7_UNORM_BLOCK, // GNF_BC7 = 78,
#else
	VK_FORMAT_UNDEFINED, // GNF_BC6 = 77,
	VK_FORMAT_UNDEFINED, // GNF_BC7 = 78,
#endif
	// Reveser Form
	VK_FORMAT_B8G8R8A8_UNORM, // BGRA8 = 79,
	// Extend for DXGI
	VK_FORMAT_UNDEFINED, // X8D24PAX32 = 80,
	VK_FORMAT_UNDEFINED, // S8 = 81,
	VK_FORMAT_UNDEFINED, // D16S8 = 82,
	VK_FORMAT_UNDEFINED, // D32S8 = 83,
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
#endif
	// Render-Doc does not support the new debug utils extension yet
#ifdef USE_DEBUG_UTILS_EXTENSION
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#else
	VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
	VK_NV_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
	/************************************************************************/
	// VR Extensions
	/************************************************************************/
	VK_KHR_DISPLAY_EXTENSION_NAME,
	VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
	/************************************************************************/
	// Multi GPU Extensions
	/************************************************************************/
#if !defined(USE_RENDER_DOC)
	  VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME,
#endif
	/************************************************************************/
	// Property querying extensions
	/************************************************************************/
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	/************************************************************************/
	/************************************************************************/
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
	VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
	VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
	VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
	// Render-Doc does not support the new debug utils extension yet so we need to use the debug marker extension
#ifndef USE_DEBUG_UTILS_EXTENSION
	VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
#endif
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
	VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
	VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME,
#endif
#ifdef VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME
	VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
#endif
	/************************************************************************/
	// NVIDIA Specific Extensions
	/************************************************************************/
	VK_NVX_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME,
	/************************************************************************/
	// AMD Specific Extensions
	/************************************************************************/
	VK_AMD_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
	VK_AMD_SHADER_BALLOT_EXTENSION_NAME,
	VK_AMD_GCN_SHADER_EXTENSION_NAME,
	/************************************************************************/
	// Multi GPU Extensions
	/************************************************************************/
	VK_KHR_DEVICE_GROUP_EXTENSION_NAME,
	/************************************************************************/
	// Bindless & None Uniform access Extensions
	/************************************************************************/
	VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
	/************************************************************************/
	// Descriptor Update Template Extension for efficient descriptor set updates
	/************************************************************************/
	VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
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
static bool gDrawIndirectCountExtension = false;
static bool gDeviceGroupCreationExtension = false;
static bool gDescriptorIndexingExtension = false;
static bool gAMDDrawIndirectCountExtension = false;
static bool gAMDGCNShaderExtension = false;

static bool gDebugMarkerSupport = false;

PFN_vkCmdDrawIndirectCountKHR        pfnVkCmdDrawIndirectCountKHR = NULL;
PFN_vkCmdDrawIndexedIndirectCountKHR pfnVkCmdDrawIndexedIndirectCountKHR = NULL;
/************************************************************************/
// IMPLEMENTATION
/************************************************************************/
#if defined(RENDERER_IMPLEMENTATION)

#ifdef _MSC_VER
#pragma comment(lib, "vulkan-1.lib")
#endif

#define SAFE_FREE(p_var)         \
	if (p_var)                   \
	{                            \
		conf_free((void*)p_var); \
	}

#if defined(__cplusplus)
#define DECLARE_ZERO(type, var) type var = {};
#else
#define DECLARE_ZERO(type, var) type var = { 0 };
#endif

// Internal utility functions (may become external one day)
VkSampleCountFlagBits util_to_vk_sample_count(SampleCount sampleCount);
VkFormat              util_to_vk_image_format(ImageFormat::Enum format, bool srgb);
#if !defined(ENABLE_RENDERER_RUNTIME_SWITCH) && !defined(ENABLE_RENDERER_RUNTIME_SWITCH)
API_INTERFACE void CALLTYPE addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
API_INTERFACE void CALLTYPE removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
API_INTERFACE void CALLTYPE addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture);
API_INTERFACE void CALLTYPE removeTexture(Renderer* pRenderer, Texture* pTexture);
API_INTERFACE void CALLTYPE mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
API_INTERFACE void CALLTYPE unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
API_INTERFACE void          CALLTYPE
							cmdUpdateBuffer(Cmd* pCmd, uint64_t srcOffset, uint64_t dstOffset, uint64_t size, Buffer* pSrcBuffer, Buffer* pBuffer);
API_INTERFACE void CALLTYPE cmdUpdateSubresources(
	Cmd* pCmd, uint32_t startSubresource, uint32_t numSubresources, SubresourceDataDesc* pSubresources, Buffer* pIntermediate,
	uint64_t intermediateOffset, Texture* pTexture);
API_INTERFACE const RendererShaderDefinesDesc CALLTYPE get_renderer_shaderdefines(Renderer* pRenderer);
#endif
/************************************************************************/
// DescriptorInfo Heap Defines
/************************************************************************/
static const uint32_t gDefaultDescriptorPoolSize = 4096;
static const uint32_t gDefaultDescriptorSets = gDefaultDescriptorPoolSize * VK_DESCRIPTOR_TYPE_RANGE_SIZE;

VkDescriptorPoolSize gDescriptorHeapPoolSizes[VK_DESCRIPTOR_TYPE_RANGE_SIZE] = {
	{ VK_DESCRIPTOR_TYPE_SAMPLER, gDefaultDescriptorPoolSize },
	// Not used in framework
	{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
	{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, gDefaultDescriptorPoolSize * 4 },
	{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, gDefaultDescriptorPoolSize * 4 },
	// Not used in framework
	{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1 },
	// Not used in framework
	{
		VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
		1,
	},
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, gDefaultDescriptorPoolSize * 4 },
	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, gDefaultDescriptorPoolSize * 4 },
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, gDefaultDescriptorPoolSize },
	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, gDefaultDescriptorPoolSize },
	// Not used in framework
	{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 },
};

static const uint32_t gDefaultDynamicDescriptorPoolSize = gDefaultDescriptorPoolSize / 4;
static const uint32_t gDefaultDynamicDescriptorSets = gDefaultDynamicDescriptorPoolSize * VK_DESCRIPTOR_TYPE_RANGE_SIZE;

VkDescriptorPoolSize gDynamicDescriptorHeapPoolSizes[VK_DESCRIPTOR_TYPE_RANGE_SIZE] = {
	{ VK_DESCRIPTOR_TYPE_SAMPLER, gDefaultDynamicDescriptorPoolSize },
	// Not used in framework
	{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
	{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, gDefaultDynamicDescriptorPoolSize },
	{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, gDefaultDynamicDescriptorPoolSize },
	// Not used in framework
	{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1 },
	// Not used in framework
	{
		VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
		1,
	},
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, gDefaultDynamicDescriptorPoolSize },
	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, gDefaultDynamicDescriptorPoolSize },
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, gDefaultDynamicDescriptorPoolSize },
	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, gDefaultDynamicDescriptorPoolSize },
	// Not used in framework
	{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 },
};
/************************************************************************/
// DescriptorInfo Heap Structures
/************************************************************************/
/// CPU Visible Heap to store all the resources needing CPU read / write operations - Textures/Buffers/RTV
typedef struct DescriptorStoreHeap
{
	uint32_t mNumDescriptorSets;
	/// Lock for multi-threaded descriptor allocations
	Mutex*   pAllocationMutex;
	uint32_t mUsedDescriptorSetCount;
	/// VK Heap
	VkDescriptorPool            pCurrentHeap;
	VkDescriptorPoolCreateFlags mFlags;
} DescriptorStoreHeap;
/************************************************************************/
// Static DescriptorInfo Heap Implementation
/************************************************************************/
static void add_descriptor_heap(
	Renderer* pRenderer, uint32_t numDescriptorSets, VkDescriptorPoolCreateFlags flags, VkDescriptorPoolSize* pPoolSizes,
	uint32_t numPoolSizes, DescriptorStoreHeap** ppHeap)
{
	DescriptorStoreHeap* pHeap = (DescriptorStoreHeap*)conf_calloc(1, sizeof(*pHeap));
	pHeap->pAllocationMutex = conf_placement_new<Mutex>(conf_calloc(1, sizeof(Mutex)));
	pHeap->mFlags = flags;
	pHeap->mNumDescriptorSets = numDescriptorSets;
	pHeap->mUsedDescriptorSetCount = 0;

	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.pNext = NULL;
	poolCreateInfo.poolSizeCount = numPoolSizes;
	poolCreateInfo.pPoolSizes = pPoolSizes;
	poolCreateInfo.flags = flags;
	poolCreateInfo.maxSets = numDescriptorSets;

	VkResult res = vkCreateDescriptorPool(pRenderer->pVkDevice, &poolCreateInfo, NULL, &pHeap->pCurrentHeap);
	ASSERT(VK_SUCCESS == res);

	*ppHeap = pHeap;
}

static void reset_descriptor_heap(Renderer* pRenderer, DescriptorStoreHeap* pHeap)
{
	VkResult res = vkResetDescriptorPool(pRenderer->pVkDevice, pHeap->pCurrentHeap, pHeap->mFlags);
	pHeap->mUsedDescriptorSetCount = 0;
	ASSERT(VK_SUCCESS == res);
}

static void remove_descriptor_heap(Renderer* pRenderer, DescriptorStoreHeap* pHeap)
{
	pHeap->pAllocationMutex->~Mutex();
	conf_free(pHeap->pAllocationMutex);
	vkDestroyDescriptorPool(pRenderer->pVkDevice, pHeap->pCurrentHeap, NULL);
	SAFE_FREE(pHeap);
}

static void consume_descriptor_sets_lock_free(
	Renderer* pRenderer, const VkDescriptorSetLayout* pLayouts, VkDescriptorSet** pSets, uint32_t numDescriptorSets,
	DescriptorStoreHeap* pHeap)
{
	DECLARE_ZERO(VkDescriptorSetAllocateInfo, alloc_info);
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.descriptorPool = pHeap->pCurrentHeap;
	alloc_info.descriptorSetCount = numDescriptorSets;
	alloc_info.pSetLayouts = pLayouts;

	VkResult vk_res = vkAllocateDescriptorSets(pRenderer->pVkDevice, &alloc_info, *pSets);
	ASSERT(VK_SUCCESS == vk_res);

	pHeap->mUsedDescriptorSetCount += numDescriptorSets;
}

static void consume_descriptor_sets(
	Renderer* pRenderer, const VkDescriptorSetLayout* pLayouts, VkDescriptorSet** pSets, uint32_t numDescriptorSets,
	DescriptorStoreHeap* pHeap)
{
	MutexLock lock(*pHeap->pAllocationMutex);
	consume_descriptor_sets_lock_free(pRenderer, pLayouts, pSets, numDescriptorSets, pHeap);
}
/************************************************************************/
/************************************************************************/
VkPipelineBindPoint gPipelineBindPoint[PIPELINE_TYPE_COUNT] = {
	VK_PIPELINE_BIND_POINT_MAX_ENUM,
	VK_PIPELINE_BIND_POINT_COMPUTE,
	VK_PIPELINE_BIND_POINT_GRAPHICS,
};
/************************************************************************/
// Descriptor Manager Implementation
/************************************************************************/
using DescriptorSetMap = tinystl::unordered_map<uint64_t, VkDescriptorSet>;
using ConstDescriptorSetMapIterator = tinystl::unordered_map<uint64_t, VkDescriptorSet>::const_iterator;
using DescriptorSetMapNode = tinystl::unordered_hash_node<uint64_t, VkDescriptorSet>;
using DescriptorNameToIndexMap = tinystl::unordered_map<uint32_t, uint32_t>;

union DescriptorUpdateData
{
	VkDescriptorImageInfo  mImageInfo;
	VkDescriptorBufferInfo mBufferInfo;
	VkBufferView           mBuferView;
};

typedef struct DescriptorManager
{
	/// The root signature associated with this descriptor manager
	RootSignature* pRootSignature;
	/// Array of Dynamic offsets per update frequency to pass the vkCmdBindDescriptorSet for binding dynamic uniform or storage buffers
	uint32_t* pDynamicOffsets[DESCRIPTOR_UPDATE_FREQ_COUNT];
	/// Array of flags to check whether a descriptor set of the update frequency is already bound to avoid unnecessary rebinding of descriptor sets
	bool                       mBoundSets[DESCRIPTOR_UPDATE_FREQ_COUNT];
	VkDescriptorUpdateTemplate mUpdateTemplates[DESCRIPTOR_UPDATE_FREQ_COUNT];
	/// Compact Descriptor data which will be filled up and passed to vkUpdateDescriptorSetWithTemplate
	DescriptorUpdateData* pUpdateData[DESCRIPTOR_UPDATE_FREQ_COUNT];
	/// Triple buffered Hash map to check if a descriptor table with a descriptor hash already exists to avoid redundant copy descriptors operations
	DescriptorSetMap mStaticDescriptorSetMap[MAX_FRAMES_IN_FLIGHT];
	VkDescriptorSet  pEmptyDescriptorSets[DESCRIPTOR_UPDATE_FREQ_COUNT];
	/// Triple buffered array of number of descriptor tables allocated per update frequency
	/// Only used for recording stats
	uint32_t mDescriptorSetCount[MAX_FRAMES_IN_FLIGHT][DESCRIPTOR_UPDATE_FREQ_COUNT];
	Cmd*     pCurrentCmd;
	uint32_t mFrameIdx;
} DescriptorManager;

static Mutex gDescriptorMutex;

static void add_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature, DescriptorManager** ppManager)
{
	DescriptorManager* pManager = (DescriptorManager*)conf_calloc(1, sizeof(*pManager));
	pManager->pRootSignature = pRootSignature;
	pManager->mFrameIdx = -1;

	const uint32_t setCount = DESCRIPTOR_UPDATE_FREQ_COUNT;

	for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
	{
		// Allocate dynamic offsets array if the descriptor set of this update frequency uses descriptors of type uniform / storage buffer dynamic
		if (pRootSignature->mVkDynamicDescriptorCounts[setIndex])
		{
			pManager->pDynamicOffsets[setIndex] =
				(uint32_t*)conf_calloc(pRootSignature->mVkDynamicDescriptorCounts[setIndex], sizeof(uint32_t));
		}

		if (pRootSignature->mVkDescriptorCounts[setIndex])
		{
			VkDescriptorUpdateTemplateEntry* pEntries = (VkDescriptorUpdateTemplateEntry*)alloca(
				pRootSignature->mVkDescriptorCounts[setIndex] * sizeof(VkDescriptorUpdateTemplateEntry));

			pManager->pUpdateData[setIndex] =
				(DescriptorUpdateData*)conf_calloc(pRootSignature->mVkCumulativeDescriptorCounts[setIndex], sizeof(DescriptorUpdateData));

			// Fill the write descriptors with default values during initialize so the only thing we change in cmdBindDescriptors is the the VkBuffer / VkImageView objects
			for (uint32_t i = 0; i < pRootSignature->mVkDescriptorCounts[setIndex]; ++i)
			{
				const DescriptorInfo* pDesc = &pRootSignature->pDescriptors[pRootSignature->pVkDescriptorIndices[setIndex][i]];
				const uint64_t        offset = pDesc->mHandleIndex * sizeof(DescriptorUpdateData);

				pEntries[i].descriptorCount = pDesc->mDesc.size;
				pEntries[i].descriptorType = pDesc->mVkType;
				pEntries[i].dstArrayElement = 0;
				pEntries[i].dstBinding = pDesc->mDesc.reg;
				pEntries[i].offset = offset;
				pEntries[i].stride = sizeof(DescriptorUpdateData);

				Texture* srvTexture = NULL;
				Texture* uavTexture = NULL;
				switch (pDesc->mDesc.textureDim)
				{
					case TEXTURE_DIM_1D:
						srvTexture = pRenderer->pDefaultTexture1DSRV;
						uavTexture = pRenderer->pDefaultTexture1DUAV;
						break;
					case TEXTURE_DIM_2D:
						srvTexture = pRenderer->pDefaultTexture2DSRV;
						uavTexture = pRenderer->pDefaultTexture2DUAV;
						break;
					case TEXTURE_DIM_3D:
						srvTexture = pRenderer->pDefaultTexture3DSRV;
						uavTexture = pRenderer->pDefaultTexture3DUAV;
						break;
					case TEXTURE_DIM_1D_ARRAY:
						srvTexture = pRenderer->pDefaultTexture1DArraySRV;
						uavTexture = pRenderer->pDefaultTexture1DArrayUAV;
						break;
					case TEXTURE_DIM_2D_ARRAY:
						srvTexture = pRenderer->pDefaultTexture2DArraySRV;
						uavTexture = pRenderer->pDefaultTexture2DArrayUAV;
						break;
					case TEXTURE_DIM_CUBE: srvTexture = pRenderer->pDefaultTextureCubeSRV; break;
					default: break;
				}

				if (pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER)
				{
					for (uint32_t arr = 0; arr < pDesc->mDesc.size; ++arr)
						pManager->pUpdateData[setIndex][pDesc->mHandleIndex + arr].mImageInfo = pRenderer->pDefaultSampler->mVkSamplerView;
				}
				else if (pDesc->mDesc.type == DESCRIPTOR_TYPE_TEXTURE)
				{
					for (uint32_t arr = 0; arr < pDesc->mDesc.size; ++arr)
						pManager->pUpdateData[setIndex][pDesc->mHandleIndex + arr].mImageInfo = {
							VK_NULL_HANDLE, srvTexture->pVkSRVDescriptor, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
						};
				}
				else if (pDesc->mDesc.type == DESCRIPTOR_TYPE_RW_TEXTURE)
				{
					for (uint32_t arr = 0; arr < pDesc->mDesc.size; ++arr)
						pManager->pUpdateData[setIndex][pDesc->mHandleIndex + arr].mImageInfo = { VK_NULL_HANDLE,
																								  uavTexture->pVkUAVDescriptors[0],
																								  VK_IMAGE_LAYOUT_GENERAL };
				}
				else if (pDesc->mDesc.type == DESCRIPTOR_TYPE_TEXEL_BUFFER)
				{
					for (uint32_t arr = 0; arr < pDesc->mDesc.size; ++arr)
						pManager->pUpdateData[setIndex][pDesc->mHandleIndex + arr].mBuferView =
							pRenderer->pDefaultBuffer->pVkUniformTexelView;
				}
				else if (pDesc->mDesc.type == DESCRIPTOR_TYPE_RW_TEXEL_BUFFER)
				{
					for (uint32_t arr = 0; arr < pDesc->mDesc.size; ++arr)
						pManager->pUpdateData[setIndex][pDesc->mHandleIndex + arr].mBuferView =
							pRenderer->pDefaultBuffer->pVkStorageTexelView;
				}
				else
				{
					for (uint32_t arr = 0; arr < pDesc->mDesc.size; ++arr)
						pManager->pUpdateData[setIndex][pDesc->mHandleIndex + arr].mBufferInfo = pRenderer->pDefaultBuffer->mVkBufferInfo;
				}
			}

			VkDescriptorUpdateTemplateCreateInfoKHR createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR;
			createInfo.pNext = NULL;
			createInfo.descriptorSetLayout = pRootSignature->mVkDescriptorSetLayouts[setIndex];
			createInfo.descriptorUpdateEntryCount = pRootSignature->mVkDescriptorCounts[setIndex];
			createInfo.pDescriptorUpdateEntries = pEntries;
			createInfo.pipelineBindPoint = gPipelineBindPoint[pRootSignature->mPipelineType];
			createInfo.pipelineLayout = pRootSignature->pPipelineLayout;
			createInfo.set = setIndex;
			createInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR;
			VkResult vkRes =
				vkCreateDescriptorUpdateTemplateKHR(pRenderer->pVkDevice, &createInfo, NULL, &pManager->mUpdateTemplates[setIndex]);
			ASSERT(VK_SUCCESS == vkRes);
		}
		else
		{
			VkDescriptorSet* pSets[] = { &pManager->pEmptyDescriptorSets[setIndex] };
			consume_descriptor_sets(pRenderer, &pRootSignature->mVkDescriptorSetLayouts[setIndex], pSets, 1, pRenderer->pDescriptorPool);
		}
	}

	*ppManager = pManager;
}

static void remove_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature, DescriptorManager* pManager)
{
	const uint32_t setCount = DESCRIPTOR_UPDATE_FREQ_COUNT;
	const uint32_t frameCount = MAX_FRAMES_IN_FLIGHT;

	// Clean up all allocated descriptor sets
	for (uint32_t frameIdx = 0; frameIdx < frameCount; ++frameIdx)
	{
		pManager->mStaticDescriptorSetMap[frameIdx].~DescriptorSetMap();
	}

	for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
	{
		SAFE_FREE(pManager->pDynamicOffsets[setIndex]);
		SAFE_FREE(pManager->pUpdateData[setIndex]);

		if (VK_NULL_HANDLE != pManager->mUpdateTemplates[setIndex])
			vkDestroyDescriptorUpdateTemplateKHR(pRenderer->pVkDevice, pManager->mUpdateTemplates[setIndex], NULL);
	}

	SAFE_FREE(pManager);
}

// This function returns the descriptor manager belonging to this thread
// If a descriptor manager does not exist for this thread, a new one is created
// With this approach we make sure that descriptor binding is thread safe and lock conf_free at the same time
static DescriptorManager* get_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature)
{
	tinystl::unordered_hash_node<ThreadID, DescriptorManager*>* pNode =
		pRootSignature->pDescriptorManagerMap.find(Thread::GetCurrentThreadID()).node;
	if (pNode == NULL)
	{
		// Only need a lock when creating a new descriptor manager for this thread
		MutexLock          lock(gDescriptorMutex);
		DescriptorManager* pManager = NULL;
		add_descriptor_manager(pRenderer, pRootSignature, &pManager);
		pRootSignature->pDescriptorManagerMap.insert({ Thread::GetCurrentThreadID(), pManager });
		return pManager;
	}
	else
	{
		return pNode->second;
	}
}

static const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName, uint32_t* pIndex)
{
	DescriptorNameToIndexMap::const_iterator it = pRootSignature->pDescriptorNameToIndexMap.find(tinystl::hash(pResName));
	if (it.node)
	{
		*pIndex = it.node->second;
		return &pRootSignature->pDescriptors[it.node->second];
	}
	else
	{
		LOGERRORF("Invalid descriptor param (%s)", pResName);
		return NULL;
	}
}
/************************************************************************/
// Render Pass Implementation
/************************************************************************/
typedef struct RenderPassDesc
{
	ImageFormat::Enum*    pColorFormats;
	const LoadActionType* pLoadActionsColor;
	bool*                 pSrgbValues;
	uint32_t              mRenderTargetCount;
	SampleCount           mSampleCount;
	ImageFormat::Enum     mDepthStencilFormat;
	LoadActionType        mLoadActionDepth;
	LoadActionType        mLoadActionStencil;
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
	RenderPass* pRenderPass = (RenderPass*)conf_calloc(1, sizeof(*pRenderPass));
	pRenderPass->mDesc = *pDesc;
	/************************************************************************/
	// Add render pass
	/************************************************************************/
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

	uint32_t colorAttachmentCount = pDesc->mRenderTargetCount;
	uint32_t depthAttachmentCount = (pDesc->mDepthStencilFormat != ImageFormat::NONE) ? 1 : 0;

	VkAttachmentDescription* attachments = NULL;
	VkAttachmentReference*   color_attachment_refs = NULL;
	VkAttachmentReference*   depth_stencil_attachment_ref = NULL;

	VkSampleCountFlagBits sample_count = util_to_vk_sample_count(pDesc->mSampleCount);

	// Fill out attachment descriptions and references
	{
		attachments = (VkAttachmentDescription*)conf_calloc(colorAttachmentCount + depthAttachmentCount, sizeof(*attachments));
		ASSERT(attachments);

		if (colorAttachmentCount > 0)
		{
			color_attachment_refs = (VkAttachmentReference*)conf_calloc(colorAttachmentCount, sizeof(*color_attachment_refs));
			ASSERT(color_attachment_refs);
		}
		if (depthAttachmentCount > 0)
		{
			depth_stencil_attachment_ref = (VkAttachmentReference*)conf_calloc(1, sizeof(*depth_stencil_attachment_ref));
			ASSERT(depth_stencil_attachment_ref);
		}

		// Color
		for (uint32_t i = 0; i < colorAttachmentCount; ++i)
		{
			const uint32_t ssidx = i;

			// descriptions
			attachments[ssidx].flags = 0;
			attachments[ssidx].format = util_to_vk_image_format(pDesc->pColorFormats[i], pDesc->pSrgbValues[i]);
			attachments[ssidx].samples = sample_count;
			attachments[ssidx].loadOp =
				pDesc->pLoadActionsColor ? gVkAttachmentLoadOpTranslator[pDesc->pLoadActionsColor[i]] : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[ssidx].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[ssidx].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[ssidx].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[ssidx].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachments[ssidx].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			// references
			color_attachment_refs[i].attachment = ssidx;
			color_attachment_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
	}

	// Depth stencil
	if (depthAttachmentCount > 0)
	{
		uint32_t idx = colorAttachmentCount;
		attachments[idx].flags = 0;
		attachments[idx].format = util_to_vk_image_format(pDesc->mDepthStencilFormat, false);
		attachments[idx].samples = sample_count;
		attachments[idx].loadOp = gVkAttachmentLoadOpTranslator[pDesc->mLoadActionDepth];
		attachments[idx].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[idx].stencilLoadOp = gVkAttachmentLoadOpTranslator[pDesc->mLoadActionStencil];
		attachments[idx].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[idx].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[idx].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depth_stencil_attachment_ref[0].attachment = idx;
		depth_stencil_attachment_ref[0].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

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

	uint32_t attachment_count = colorAttachmentCount;
	attachment_count += depthAttachmentCount;

	DECLARE_ZERO(VkRenderPassCreateInfo, create_info);
	create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.attachmentCount = attachment_count;
	create_info.pAttachments = attachments;
	create_info.subpassCount = 1;
	create_info.pSubpasses = &subpass;
	create_info.dependencyCount = 0;
	create_info.pDependencies = NULL;

	VkResult vk_res = vkCreateRenderPass(pRenderer->pVkDevice, &create_info, NULL, &(pRenderPass->pRenderPass));
	ASSERT(VK_SUCCESS == vk_res);

	SAFE_FREE(attachments);
	SAFE_FREE(color_attachment_refs);
	SAFE_FREE(depth_stencil_attachment_ref);

	*ppRenderPass = pRenderPass;
}

static void remove_render_pass(Renderer* pRenderer, RenderPass* pRenderPass)
{
	vkDestroyRenderPass(pRenderer->pVkDevice, pRenderPass->pRenderPass, NULL);
	SAFE_FREE(pRenderPass);
}

static void add_framebuffer(Renderer* pRenderer, const FrameBufferDesc* pDesc, FrameBuffer** ppFrameBuffer)
{
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

	FrameBuffer* pFrameBuffer = (FrameBuffer*)conf_calloc(1, sizeof(*pFrameBuffer));
	ASSERT(pFrameBuffer);

	uint32_t colorAttachmentCount = pDesc->mRenderTargetCount;
	uint32_t depthAttachmentCount = (pDesc->pDepthStencil) ? 1 : 0;

	if (colorAttachmentCount)
	{
		pFrameBuffer->mWidth = pDesc->ppRenderTargets[0]->mDesc.mWidth;
		pFrameBuffer->mHeight = pDesc->ppRenderTargets[0]->mDesc.mHeight;
		if (pDesc->pColorArraySlices)
			pFrameBuffer->mArraySize = 1;
		else
			pFrameBuffer->mArraySize = pDesc->ppRenderTargets[0]->mDesc.mArraySize;
	}
	else
	{
		pFrameBuffer->mWidth = pDesc->pDepthStencil->mDesc.mWidth;
		pFrameBuffer->mHeight = pDesc->pDepthStencil->mDesc.mHeight;
		if (pDesc->mDepthArraySlice != -1)
			pFrameBuffer->mArraySize = 1;
		else
			pFrameBuffer->mArraySize = pDesc->pDepthStencil->mDesc.mArraySize;
	}

	/************************************************************************/
	// Add frame buffer
	/************************************************************************/
	uint32_t attachment_count = colorAttachmentCount;
	attachment_count += depthAttachmentCount;

	VkImageView* pImageViews = (VkImageView*)conf_calloc(attachment_count, sizeof(*pImageViews));
	ASSERT(pImageViews);

	VkImageView* iter_attachments = pImageViews;
	// Color
	for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i)
	{
		uint32_t handle = 0;
		if (pDesc->pColorMipSlices)
		{
			if (pDesc->pColorArraySlices)
				handle = 1 + pDesc->pColorMipSlices[i] * pDesc->ppRenderTargets[i]->mDesc.mArraySize + pDesc->pColorArraySlices[i];
			else
				handle = 1 + pDesc->pColorMipSlices[i];
		}
		else if (pDesc->pColorArraySlices)
		{
			handle = 1 + pDesc->pColorArraySlices[i];
		}
		*iter_attachments = pDesc->ppRenderTargets[i]->pVkDescriptors[handle];
		++iter_attachments;
	}
	// Depth/stencil
	if (pDesc->pDepthStencil)
	{
		uint32_t handle = 0;
		if (pDesc->mDepthMipSlice != -1)
		{
			if (pDesc->mDepthArraySlice != -1)
				handle = 1 + pDesc->mDepthMipSlice * pDesc->pDepthStencil->mDesc.mArraySize + pDesc->mDepthArraySlice;
			else
				handle = 1 + pDesc->mDepthMipSlice;
		}
		else if (pDesc->mDepthArraySlice != -1)
		{
			handle = 1 + pDesc->mDepthArraySlice;
		}
		*iter_attachments = pDesc->pDepthStencil->pVkDescriptors[handle];
		++iter_attachments;
	}

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
	VkResult vk_res = vkCreateFramebuffer(pRenderer->pVkDevice, &add_info, NULL, &(pFrameBuffer->pFramebuffer));
	ASSERT(VK_SUCCESS == vk_res);
	SAFE_FREE(pImageViews);
	/************************************************************************/
	/************************************************************************/

	*ppFrameBuffer = pFrameBuffer;
}

static void remove_framebuffer(Renderer* pRenderer, FrameBuffer* pFrameBuffer)
{
	ASSERT(pRenderer);
	ASSERT(pFrameBuffer);

	vkDestroyFramebuffer(pRenderer->pVkDevice, pFrameBuffer->pFramebuffer, NULL);
	SAFE_FREE(pFrameBuffer);
}
/************************************************************************/
// Per Thread Render Pass synchronization logic
/************************************************************************/
/// Render-passes are not exposed to the app code since they are not available on all apis
/// This map takes care of hashing a render pass based on the render targets passed to cmdBeginRender
using RenderPassMap = tinystl::unordered_map<uint64_t, struct RenderPass*>;
using RenderPassMapNode = tinystl::unordered_hash_node<uint64_t, struct RenderPass*>;
using FrameBufferMap = tinystl::unordered_map<uint64_t, struct FrameBuffer*>;
using FrameBufferMapNode = tinystl::unordered_hash_node<uint64_t, struct FrameBuffer*>;

// RenderPass map per thread (this will make lookups lock free and we only need a lock when inserting a RenderPass Map for the first time)
tinystl::unordered_map<ThreadID, RenderPassMap> gRenderPassMap;
// FrameBuffer map per thread (this will make lookups lock free and we only need a lock when inserting a FrameBuffer map for the first time)
tinystl::unordered_map<ThreadID, FrameBufferMap> gFrameBufferMap;
Mutex                                            gRenderPassMutex;

static RenderPassMap& get_render_pass_map()
{
	tinystl::unordered_hash_node<ThreadID, RenderPassMap>* pNode = gRenderPassMap.find(Thread::GetCurrentThreadID()).node;
	if (pNode == NULL)
	{
		// Only need a lock when creating a new renderpass map for this thread
		MutexLock lock(gRenderPassMutex);
		return gRenderPassMap.insert({ Thread::GetCurrentThreadID(), {} }).first->second;
	}
	else
	{
		return pNode->second;
	}
}

static FrameBufferMap& get_frame_buffer_map()
{
	tinystl::unordered_hash_node<ThreadID, FrameBufferMap>* pNode = gFrameBufferMap.find(Thread::GetCurrentThreadID()).node;
	if (pNode == NULL)
	{
		// Only need a lock when creating a new framebuffer map for this thread
		MutexLock lock(gRenderPassMutex);
		return gFrameBufferMap.insert({ Thread::GetCurrentThreadID(), {} }).first->second;
	}
	else
	{
		return pNode->second;
	}
}
/************************************************************************/
// Logging, Validation layer implementation
/************************************************************************/
// Proxy log callback
static void internal_log(LogType type, const char* msg, const char* component)
{
	switch (type)
	{
		case LOG_TYPE_INFO: LOGINFOF("%s ( %s )", component, msg); break;
		case LOG_TYPE_WARN: LOGWARNINGF("%s ( %s )", component, msg); break;
		case LOG_TYPE_DEBUG: LOGDEBUGF("%s ( %s )", component, msg); break;
		case LOG_TYPE_ERROR: LOGERRORF("%s ( %s )", component, msg); break;
		default: break;
	}
}

// Proxy debug callback for Vulkan layers
#ifdef USE_DEBUG_UTILS_EXTENSION
static VkBool32 VKAPI_PTR internal_debug_report_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	const char* pLayerPrefix = pCallbackData->pMessageIdName;
	const char* pMessage = pCallbackData->pMessage;
	int32_t     messageCode = pCallbackData->messageIdNumber;
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
	{
		LOGINFOF("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		// Code 64 is vkCmdClearAttachments issued before any draws
		// We ignore this since we dont use load store actions
		// Instead we clear the attachments in the DirectX way
		if (messageCode == 64)
			return VK_FALSE;

		LOGWARNINGF("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		LOGERRORF("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
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
		LOGINFOF("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
	}
	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
	{
		// Vulkan SDK 1.0.68 fixes the Dedicated memory binding validation error bugs
#if VK_HEADER_VERSION < 68
		// Disable warnings for bind memory for dedicated allocations extension
		if (gDedicatedAllocationExtension && messageCode != 11 && messageCode != 12)
#endif
			LOGWARNINGF("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
	}
	else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
	{
		LOGWARNINGF("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
	}
	else if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
	{
		LOGERRORF("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
	}

	return VK_FALSE;
}
#endif
/************************************************************************/
// Create default resources to be used a null descriptors in case user does not specify some descriptors
/************************************************************************/
static void create_default_resources(Renderer* pRenderer)
{
	// 1D texture
	TextureDesc textureDesc = {};
	textureDesc.mArraySize = 1;
	textureDesc.mDepth = 1;
	textureDesc.mFormat = ImageFormat::R8;
	textureDesc.mHeight = 1;
	textureDesc.mMipLevels = 1;
	textureDesc.mSampleCount = SAMPLE_COUNT_1;
	textureDesc.mStartState = RESOURCE_STATE_COMMON;
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	textureDesc.mWidth = 1;
	addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTexture1DSRV);
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
	addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTexture1DUAV);

	// 1D texture array
	textureDesc.mArraySize = 2;
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTexture1DArraySRV);
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
	addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTexture1DArrayUAV);

	// 2D texture
	textureDesc.mWidth = 2;
	textureDesc.mHeight = 2;
	textureDesc.mArraySize = 1;
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTexture2DSRV);
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
	addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTexture2DUAV);

	// 2D texture array
	textureDesc.mArraySize = 2;
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTexture2DArraySRV);
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
	addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTexture2DArrayUAV);

	// 3D texture
	textureDesc.mDepth = 2;
	textureDesc.mArraySize = 1;
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTexture3DSRV);
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
	addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTexture3DUAV);

	// Cube texture
	textureDesc.mWidth = 2;
	textureDesc.mHeight = 2;
	textureDesc.mDepth = 1;
	textureDesc.mArraySize = 6;
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE;
	addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureCubeSRV);

	BufferDesc bufferDesc = {};
	bufferDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	bufferDesc.mStartState = RESOURCE_STATE_COMMON;
	bufferDesc.mSize = sizeof(uint32_t);
	bufferDesc.mFirstElement = 0;
	bufferDesc.mElementCount = 1;
	bufferDesc.mStructStride = sizeof(uint32_t);
	addBuffer(pRenderer, &bufferDesc, &pRenderer->pDefaultBuffer);

	SamplerDesc samplerDesc = {};
	samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_BORDER;
	addSampler(pRenderer, &samplerDesc, &pRenderer->pDefaultSampler);

	BlendStateDesc blendStateDesc = {};
	blendStateDesc.mDstAlphaFactors[0] = BC_ZERO;
	blendStateDesc.mDstFactors[0] = BC_ZERO;
	blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
	blendStateDesc.mSrcFactors[0] = BC_ONE;
	blendStateDesc.mMasks[0] = ALL;
	blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
	blendStateDesc.mIndependentBlend = false;
	addBlendState(pRenderer, &blendStateDesc, &pRenderer->pDefaultBlendState);

	DepthStateDesc depthStateDesc = {};
	depthStateDesc.mDepthFunc = CMP_LEQUAL;
	depthStateDesc.mDepthTest = false;
	depthStateDesc.mDepthWrite = false;
	depthStateDesc.mStencilBackFunc = CMP_ALWAYS;
	depthStateDesc.mStencilFrontFunc = CMP_ALWAYS;
	depthStateDesc.mStencilReadMask = 0xFF;
	depthStateDesc.mStencilWriteMask = 0xFF;
	addDepthState(pRenderer, &depthStateDesc, &pRenderer->pDefaultDepthState);

	RasterizerStateDesc rasterizerStateDesc = {};
	rasterizerStateDesc.mCullMode = CULL_MODE_BACK;
	addRasterizerState(pRenderer, &rasterizerStateDesc, &pRenderer->pDefaultRasterizerState);

	// Create command buffer to transition resources to the correct state
	Queue*   graphicsQueue = NULL;
	CmdPool* cmdPool = NULL;
	Cmd*     cmd = NULL;
	Fence*   fence = NULL;

	QueueDesc queueDesc = {};
	queueDesc.mType = CMD_POOL_DIRECT;
	addQueue(pRenderer, &queueDesc, &graphicsQueue);

	addCmdPool(pRenderer, graphicsQueue, false, &cmdPool);
	addCmd(cmdPool, false, &cmd);
	addFence(pRenderer, &fence);

	// Transition resources
	beginCmd(cmd);
	BufferBarrier bufferBarriers[] = {
		{ pRenderer->pDefaultBuffer, RESOURCE_STATE_SHADER_RESOURCE, false },
	};
	uint bufferBarrierCount = sizeof(bufferBarriers) / sizeof(bufferBarriers[0]);

	TextureBarrier textureBarriers[] = {
		{ pRenderer->pDefaultTexture1DSRV, RESOURCE_STATE_SHADER_RESOURCE, false },
		{ pRenderer->pDefaultTexture1DUAV, RESOURCE_STATE_UNORDERED_ACCESS, false },
		{ pRenderer->pDefaultTexture1DArraySRV, RESOURCE_STATE_SHADER_RESOURCE, false },
		{ pRenderer->pDefaultTexture1DArrayUAV, RESOURCE_STATE_UNORDERED_ACCESS, false },
		{ pRenderer->pDefaultTexture2DSRV, RESOURCE_STATE_SHADER_RESOURCE, false },
		{ pRenderer->pDefaultTexture2DUAV, RESOURCE_STATE_UNORDERED_ACCESS, false },
		{ pRenderer->pDefaultTexture2DArraySRV, RESOURCE_STATE_SHADER_RESOURCE, false },
		{ pRenderer->pDefaultTexture2DArrayUAV, RESOURCE_STATE_UNORDERED_ACCESS, false },
		{ pRenderer->pDefaultTexture3DSRV, RESOURCE_STATE_SHADER_RESOURCE, false },
		{ pRenderer->pDefaultTexture3DUAV, RESOURCE_STATE_UNORDERED_ACCESS, false },
		{ pRenderer->pDefaultTextureCubeSRV, RESOURCE_STATE_SHADER_RESOURCE, false },
	};
	uint textureBarrierCount = sizeof(textureBarriers) / sizeof(textureBarriers[0]);
	cmdResourceBarrier(cmd, bufferBarrierCount, bufferBarriers, textureBarrierCount, textureBarriers, false);
	endCmd(cmd);

	queueSubmit(graphicsQueue, 1, &cmd, fence, 0, NULL, 0, NULL);
	FenceStatus fenceStatus = {};
	getFenceStatus(pRenderer, fence, &fenceStatus);
	if (fenceStatus != FENCE_STATUS_COMPLETE)
		waitForFences(graphicsQueue, 1, &fence, false);

	// Delete command buffer
	removeFence(pRenderer, fence);
	removeCmd(cmdPool, cmd);
	removeCmdPool(pRenderer, cmdPool);
	removeQueue(graphicsQueue);
}

static void destroy_default_resources(Renderer* pRenderer)
{
	removeTexture(pRenderer, pRenderer->pDefaultTexture1DSRV);
	removeTexture(pRenderer, pRenderer->pDefaultTexture1DUAV);
	removeTexture(pRenderer, pRenderer->pDefaultTexture1DArraySRV);
	removeTexture(pRenderer, pRenderer->pDefaultTexture1DArrayUAV);
	removeTexture(pRenderer, pRenderer->pDefaultTexture2DSRV);
	removeTexture(pRenderer, pRenderer->pDefaultTexture2DUAV);
	removeTexture(pRenderer, pRenderer->pDefaultTexture2DArraySRV);
	removeTexture(pRenderer, pRenderer->pDefaultTexture2DArrayUAV);
	removeTexture(pRenderer, pRenderer->pDefaultTexture3DSRV);
	removeTexture(pRenderer, pRenderer->pDefaultTexture3DUAV);
	removeTexture(pRenderer, pRenderer->pDefaultTextureCubeSRV);
	removeBuffer(pRenderer, pRenderer->pDefaultBuffer);
	removeSampler(pRenderer, pRenderer->pDefaultSampler);

	removeBlendState(pRenderer->pDefaultBlendState);
	removeDepthState(pRenderer->pDefaultDepthState);
	removeRasterizerState(pRenderer->pDefaultRasterizerState);
}
/************************************************************************/
// Globals
/************************************************************************/
static volatile uint64_t gBufferIds = 0;
static volatile uint64_t gTextureIds = 0;
static volatile uint64_t gSamplerIds = 0;
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

VkFormat util_to_vk_image_format(ImageFormat::Enum format, bool srgb)
{
	VkFormat result = VK_FORMAT_UNDEFINED;

	if (format >= sizeof(gVkFormatTranslator) / sizeof(gVkFormatTranslator[0]))
	{
		LOGERROR("Failed to Map from ConfettilFileFromat to DXGI format, should add map method in gVulkanFormatTranslator");
	}
	else
	{
		result = gVkFormatTranslator[format];
		if (srgb)
		{
			if (result == VK_FORMAT_R8G8B8A8_UNORM)
				result = VK_FORMAT_R8G8B8A8_SRGB;
			else if (result == VK_FORMAT_B8G8R8A8_UNORM)
				result = VK_FORMAT_B8G8R8A8_SRGB;
			else if (result == VK_FORMAT_B8G8R8_UNORM)
				result = VK_FORMAT_B8G8R8_SRGB;
			else if (result == VK_FORMAT_BC1_RGBA_UNORM_BLOCK)
				result = VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
			else if (result == VK_FORMAT_BC2_UNORM_BLOCK)
				result = VK_FORMAT_BC2_SRGB_BLOCK;
			else if (result == VK_FORMAT_BC3_UNORM_BLOCK)
				result = VK_FORMAT_BC3_SRGB_BLOCK;
			else if (result == VK_FORMAT_BC7_UNORM_BLOCK)
				result = VK_FORMAT_BC7_SRGB_BLOCK;
		}
	}

	return result;
}

ImageFormat::Enum util_to_internal_image_format(VkFormat format)
{
	ImageFormat::Enum result = ImageFormat::NONE;
	switch (format)
	{
			// 1 channel
		case VK_FORMAT_R8_UNORM: result = ImageFormat::R8; break;
		case VK_FORMAT_R16_UNORM: result = ImageFormat::R16; break;
		case VK_FORMAT_R16_SFLOAT: result = ImageFormat::R16F; break;
		case VK_FORMAT_R32_UINT: result = ImageFormat::R32UI; break;
		case VK_FORMAT_R32_SFLOAT:
			result = ImageFormat::R32F;
			break;
			// 2 channel
		case VK_FORMAT_R8G8_UNORM: result = ImageFormat::RG8; break;
		case VK_FORMAT_R16G16_UNORM: result = ImageFormat::RG16; break;
		case VK_FORMAT_R16G16_SFLOAT: result = ImageFormat::RG16F; break;
		case VK_FORMAT_R32G32_UINT: result = ImageFormat::RG32UI; break;
		case VK_FORMAT_R32G32_SFLOAT:
			result = ImageFormat::RG32F;
			break;
			// 3 channel
		case VK_FORMAT_R8G8B8_UNORM:
		case VK_FORMAT_R8G8B8_SRGB: result = ImageFormat::RGB8; break;
		case VK_FORMAT_R16G16B16_UNORM: result = ImageFormat::RGB16; break;
		case VK_FORMAT_R16G16B16_SFLOAT: result = ImageFormat::RGB16F; break;
		case VK_FORMAT_R32G32B32_UINT: result = ImageFormat::RGB32UI; break;
		case VK_FORMAT_R32G32B32_SFLOAT:
			result = ImageFormat::RGB32F;
			break;
			// 4 channel
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SRGB: result = ImageFormat::BGRA8; break;
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SRGB: result = ImageFormat::RGBA8; break;
		case VK_FORMAT_R16G16B16A16_UNORM: result = ImageFormat::RGBA16; break;
		case VK_FORMAT_R16G16B16A16_SFLOAT: result = ImageFormat::RGBA16F; break;
		case VK_FORMAT_R32G32B32A32_UINT: result = ImageFormat::RGBA32UI; break;
		case VK_FORMAT_R32G32B32A32_SFLOAT:
			result = ImageFormat::RGBA32F;
			break;
			// Depth/stencil
		case VK_FORMAT_D16_UNORM: result = ImageFormat::D16; break;
		case VK_FORMAT_X8_D24_UNORM_PACK32: result = ImageFormat::X8D24PAX32; break;
		case VK_FORMAT_D32_SFLOAT: result = ImageFormat::D32F; break;
		case VK_FORMAT_S8_UINT: result = ImageFormat::S8; break;
		case VK_FORMAT_D16_UNORM_S8_UINT: result = ImageFormat::D16S8; break;
		case VK_FORMAT_D24_UNORM_S8_UINT: result = ImageFormat::D24S8; break;
		case VK_FORMAT_D32_SFLOAT_S8_UINT: result = ImageFormat::D32S8; break;
		default:
		{
			result = ImageFormat::NONE;
			ASSERT(false && "Image Format not supported!");
			break;
		}
	}
	return result;
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

uint32_t util_vk_determine_image_channel_count(ImageFormat::Enum format)
{
	uint32_t result = 0;
	switch (format)
	{
			// 1 channel
		case ImageFormat::R8: result = 1; break;
		case ImageFormat::R16: result = 1; break;
		case ImageFormat::R16F: result = 1; break;
		case ImageFormat::R32UI: result = 1; break;
		case ImageFormat::R32F:
			result = 1;
			break;
			// 2 channel
		case ImageFormat::RG8: result = 2; break;
		case ImageFormat::RG16: result = 2; break;
		case ImageFormat::RG16F: result = 2; break;
		case ImageFormat::RG32UI: result = 2; break;
		case ImageFormat::RG32F:
			result = 2;
			break;
			// 3 channel
		case ImageFormat::RGB8: result = 3; break;
		case ImageFormat::RGB16: result = 3; break;
		case ImageFormat::RGB16F: result = 3; break;
		case ImageFormat::RGB32UI: result = 3; break;
		case ImageFormat::RGB32F:
			result = 3;
			break;
			// 4 channel
		case ImageFormat::BGRA8: result = 4; break;
		case ImageFormat::RGBA8: result = 4; break;
		case ImageFormat::RGBA16: result = 4; break;
		case ImageFormat::RGBA16F: result = 4; break;
		case ImageFormat::RGBA32UI: result = 4; break;
		case ImageFormat::RGBA32F:
			result = 4;
			break;
			// Depth/stencil
		case ImageFormat::D16: result = 0; break;
		case ImageFormat::X8D24PAX32: result = 0; break;
		case ImageFormat::D32F: result = 0; break;
		case ImageFormat::S8: result = 0; break;
		case ImageFormat::D16S8: result = 0; break;
		case ImageFormat::D24S8: result = 0; break;
		case ImageFormat::D32S8: result = 0; break;
		default:
		{
			result = ImageFormat::NONE;
			ASSERT(false && "Image Format not supported!");
			break;
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
	if ((state & RESOURCE_STATE_SHADER_RESOURCE) || (state & RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
	{
		ret |= VK_ACCESS_SHADER_READ_BIT;
	}
	if (state & RESOURCE_STATE_PRESENT)
	{
		ret |= VK_ACCESS_MEMORY_READ_BIT;
	}

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

	if ((usage & RESOURCE_STATE_SHADER_RESOURCE) || (usage & RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	if (usage & RESOURCE_STATE_PRESENT)
		return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	if (usage == RESOURCE_STATE_COMMON)
		return VK_IMAGE_LAYOUT_GENERAL;

	return VK_IMAGE_LAYOUT_UNDEFINED;
}

VkImageAspectFlags util_vk_determine_aspect_mask(VkFormat format)
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
			result = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
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

VkQueueFlags util_to_vk_queue_flags(CmdPoolType cmdPoolType)
{
	switch (cmdPoolType)
	{
		case CMD_POOL_DIRECT: return VK_QUEUE_GRAPHICS_BIT;
		case CMD_POOL_COPY: return VK_QUEUE_TRANSFER_BIT;
		case CMD_POOL_COMPUTE: return VK_QUEUE_COMPUTE_BIT;
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

	ASSERT(res != 0);
	return res;
}
/************************************************************************/
// Multi GPU Helper Functions
/************************************************************************/
uint32_t util_calculate_shared_device_mask(uint32_t gpuCount) { return (1 << gpuCount) - 1; }

void util_calculate_device_indices(
	Renderer* pRenderer, uint32_t nodeIndex, uint32_t* pSharedNodeIndices, uint32_t sharedNodeIndexCount, uint32_t* pIndices)
{
	for (uint32_t i = 0; i < pRenderer->mVkLinkedNodeCount; ++i)
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
void CreateInstance(const char* app_name, Renderer* pRenderer)
{
	uint32_t              layerCount = 0;
	uint32_t              count = 0;
	VkLayerProperties     layers[100];
	VkExtensionProperties exts[100];
	vkEnumerateInstanceLayerProperties(&layerCount, NULL);
	vkEnumerateInstanceLayerProperties(&layerCount, layers);
	for (uint32_t i = 0; i < layerCount; ++i)
	{
		internal_log(LOG_TYPE_INFO, layers[i].layerName, "vkinstance-layer");
	}
	vkEnumerateInstanceExtensionProperties(NULL, &count, NULL);
	vkEnumerateInstanceExtensionProperties(NULL, &count, exts);
	for (uint32_t i = 0; i < count; ++i)
	{
		internal_log(LOG_TYPE_INFO, exts[i].extensionName, "vkinstance-ext");
	}

	DECLARE_ZERO(VkApplicationInfo, app_info);
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pNext = NULL;
	app_info.pApplicationName = app_name;
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "TheForge";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_1;

	VkResult vk_res = VK_RESULT_MAX_ENUM;

	// Instance
	{
		// check to see if the layers are present
		for (uint32_t i = 0; i < (uint32_t)pRenderer->mInstanceLayers.size(); ++i)
		{
			bool layerFound = false;
			for (uint32_t j = 0; j < layerCount; ++j)
			{
				if (strcmp(pRenderer->mInstanceLayers[i], layers[j].layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}
			if (layerFound == false)
			{
				internal_log(LOG_TYPE_WARN, pRenderer->mInstanceLayers[i], "vkinstance-layer-missing");
				// deleate layer and get new index
				i = (uint32_t)(
					pRenderer->mInstanceLayers.erase_unordered(pRenderer->mInstanceLayers.data() + i) - pRenderer->mInstanceLayers.data());
			}
		}

		uint32_t                     extension_count = 0;
		const uint32_t               initialCount = sizeof(gVkWantedInstanceExtensions) / sizeof(gVkWantedInstanceExtensions[0]);
		const uint32_t               userRequestedCount = (uint32_t)pRenderer->mSettings.mInstanceExtensions.size();
		tinystl::vector<const char*> wantedInstanceExtensions(initialCount + userRequestedCount);
		for (uint32_t i = 0; i < initialCount; ++i)
		{
			wantedInstanceExtensions[i] = gVkWantedInstanceExtensions[i];
		}
		for (uint32_t i = 0; i < userRequestedCount; ++i)
		{
			wantedInstanceExtensions[initialCount + i] = pRenderer->mSettings.mInstanceExtensions[i];
		}
		const uint32_t wanted_extension_count = (uint32_t)wantedInstanceExtensions.size();
		// Layer extensions
		for (uint32_t i = -1; i < pRenderer->mInstanceLayers.size(); ++i)
		{
			const char* layer_name = pRenderer->mInstanceLayers[i];
			uint32_t    count = 0;
			vkEnumerateInstanceExtensionProperties(layer_name, &count, NULL);
			VkExtensionProperties* properties = (VkExtensionProperties*)conf_calloc(count, sizeof(*properties));
			ASSERT(properties != NULL);
			vkEnumerateInstanceExtensionProperties(layer_name, &count, properties);
			for (uint32_t j = 0; j < count; ++j)
			{
				for (uint32_t k = 0; k < wanted_extension_count; ++k)
				{
					if (strcmp(wantedInstanceExtensions[k], properties[j].extensionName) == 0)
					{
						pRenderer->gVkInstanceExtensions[extension_count++] = wantedInstanceExtensions[k];
						// clear wanted extenstion so we dont load it more then once
						//gVkWantedInstanceExtensions[k] = "";
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
				VkExtensionProperties* properties = (VkExtensionProperties*)conf_calloc(count, sizeof(*properties));
				ASSERT(properties != NULL);
				vkEnumerateInstanceExtensionProperties(layer_name, &count, properties);
				for (uint32_t j = 0; j < count; ++j)
				{
					for (uint32_t k = 0; k < wanted_extension_count; ++k)
					{
						if (strcmp(wantedInstanceExtensions[k], properties[j].extensionName) == 0)
						{
							pRenderer->gVkInstanceExtensions[extension_count++] = wantedInstanceExtensions[k];
							// clear wanted extenstion so we dont load it more then once
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

		// Add more extensions here
		DECLARE_ZERO(VkInstanceCreateInfo, create_info);
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pNext = NULL;
		create_info.flags = 0;
		create_info.pApplicationInfo = &app_info;
		create_info.enabledLayerCount = (uint32_t)pRenderer->mInstanceLayers.size();
		create_info.ppEnabledLayerNames = pRenderer->mInstanceLayers.data();
		create_info.enabledExtensionCount = extension_count;
		create_info.ppEnabledExtensionNames = pRenderer->gVkInstanceExtensions;
		vk_res = vkCreateInstance(&create_info, NULL, &(pRenderer->pVkInstance));
		ASSERT(VK_SUCCESS == vk_res);
	}

	// Load Vulkan instance functions
	volkLoadInstance(pRenderer->pVkInstance);

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
			VkResult res = vkCreateDebugUtilsMessengerEXT(pRenderer->pVkInstance, &create_info, NULL, &(pRenderer->pVkDebugUtilsMessenger));
			if (VK_SUCCESS != res)
			{
				internal_log(
					LOG_TYPE_ERROR, "vkCreateDebugUtilsMessengerEXT failed - disabling Vulkan debug callbacks",
					"internal_vk_init_instance");
			}
		}
#else
		DECLARE_ZERO(VkDebugReportCallbackCreateInfoEXT, create_info);
		create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
		create_info.pNext = NULL;
		create_info.pfnCallback = internal_debug_report_callback;
		create_info.pUserData = NULL;
		create_info.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
							// VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | Performance warnings are not very vaild on desktop
							VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
		VkResult res = vkCreateDebugReportCallbackEXT(pRenderer->pVkInstance, &create_info, NULL, &(pRenderer->pVkDebugReport));
		if (VK_SUCCESS != res)
		{
			internal_log(
				LOG_TYPE_ERROR, "vkCreateDebugReportCallbackEXT failed - disabling Vulkan debug callbacks", "internal_vk_init_instance");
		}
#endif
	}
}

static void RemoveInstance(Renderer* pRenderer)
{
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkInstance);

#ifdef USE_DEBUG_UTILS_EXTENSION
	if (pRenderer->pVkDebugUtilsMessenger)
	{
		vkDestroyDebugUtilsMessengerEXT(pRenderer->pVkInstance, pRenderer->pVkDebugUtilsMessenger, NULL);
	}
#else
	if (pRenderer->pVkDebugReport)
	{
		vkDestroyDebugReportCallbackEXT(pRenderer->pVkInstance, pRenderer->pVkDebugReport, NULL);
	}
#endif

	vkDestroyInstance(pRenderer->pVkInstance, NULL);
}

static void AddDevice(Renderer* pRenderer)
{
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkInstance);

	VkDeviceGroupDeviceCreateInfoKHR                    deviceGroupInfo = { VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO_KHR };
	tinystl::vector<VkPhysicalDeviceGroupPropertiesKHR> props;
	VkResult                                            vk_res = VK_RESULT_MAX_ENUM;

	if (pRenderer->mSettings.mGpuMode == GPU_MODE_LINKED && gDeviceGroupCreationExtension)
	{
		// (not shown) fill out devCreateInfo as usual.
		uint32_t deviceGroupCount = 0;

		// Query the number of device groups
		vkEnumeratePhysicalDeviceGroupsKHR(pRenderer->pVkInstance, &deviceGroupCount, NULL);

		// Allocate and initialize structures to query the device groups
		props.resize(deviceGroupCount);
		for (uint32_t i = 0; i < deviceGroupCount; ++i)
		{
			props[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES_KHR;
			props[i].pNext = NULL;
		}
		vk_res = vkEnumeratePhysicalDeviceGroupsKHR(pRenderer->pVkInstance, &deviceGroupCount, props.data());
		ASSERT(VK_SUCCESS == vk_res);

		// If the first device group has more than one physical device. create
		// a logical device using all of the physical devices.
		for (uint32_t i = 0; i < deviceGroupCount; ++i)
		{
			if (props[i].physicalDeviceCount > 1)
			{
				deviceGroupInfo.physicalDeviceCount = props[i].physicalDeviceCount;
				deviceGroupInfo.pPhysicalDevices = props[i].physicalDevices;
				pRenderer->mVkLinkedNodeCount = deviceGroupInfo.physicalDeviceCount;
				break;
			}
		}
	}

	if (!pRenderer->mVkLinkedNodeCount)
	{
		pRenderer->mSettings.mGpuMode = GPU_MODE_SINGLE;
	}

	vk_res = vkEnumeratePhysicalDevices(pRenderer->pVkInstance, &(pRenderer->mNumOfGPUs), NULL);
	ASSERT(VK_SUCCESS == vk_res);
	ASSERT(pRenderer->mNumOfGPUs > 0);

	if (pRenderer->mNumOfGPUs > MAX_GPUS)
	{
		pRenderer->mNumOfGPUs = MAX_GPUS;
	}

	vk_res = vkEnumeratePhysicalDevices(pRenderer->pVkInstance, &(pRenderer->mNumOfGPUs), pRenderer->pVkGPUs);
	ASSERT(VK_SUCCESS == vk_res);

	/************************************************************************/
	// Sort gpus to get discrete gpus first
	// If we have multiple discrete gpus sort them by VRAM size
	// To find VRAM in Vulkan, loop through all the heaps and find if the
	// heap has the DEVICE_LOCAL_BIT flag set
	/************************************************************************/
	qsort(pRenderer->pVkGPUs, pRenderer->mNumOfGPUs, sizeof(pRenderer->pVkGPUs[0]), [](const void* lhs, const void* rhs) {
		VkPhysicalDeviceProperties       lhsProps = {};
		VkPhysicalDeviceMemoryProperties lhsMemoryProps = {};
		VkPhysicalDevice                 lhsGpu = *(VkPhysicalDevice*)lhs;
		VkPhysicalDevice                 rhsGpu = *(VkPhysicalDevice*)rhs;
		VkPhysicalDeviceProperties       rhsProps = {};
		VkPhysicalDeviceMemoryProperties rhsMemoryProps = {};
		vkGetPhysicalDeviceProperties(lhsGpu, &lhsProps);
		vkGetPhysicalDeviceMemoryProperties(lhsGpu, &lhsMemoryProps);
		vkGetPhysicalDeviceProperties(rhsGpu, &rhsProps);
		vkGetPhysicalDeviceMemoryProperties(rhsGpu, &rhsMemoryProps);

		char lhsDevId[MAX_GPU_VENDOR_STRING_LENGTH];
		sprintf(lhsDevId, "%#x", lhsProps.deviceID);

		char lhsVendId[MAX_GPU_VENDOR_STRING_LENGTH];
		sprintf(lhsVendId, "%#x", lhsProps.vendorID);

		char rhsDevId[MAX_GPU_VENDOR_STRING_LENGTH];
		sprintf(rhsDevId, "%#x", rhsProps.deviceID);

		char rhsVendId[MAX_GPU_VENDOR_STRING_LENGTH];
		sprintf(rhsVendId, "%#x", rhsProps.vendorID);

		//TODO: Fix once vulkan adds support for revision ID
		GPUPresetLevel lhsPreset = getGPUPresetLevel(lhsVendId, lhsDevId, "0x00");
		GPUPresetLevel rhsPreset = getGPUPresetLevel(rhsVendId, rhsDevId, "0x00");

#if defined(AUTOMATED_TESTING) && defined(ACTIVE_TESTING_GPU)
		//todo : ADD Revision
		GPUVendorPreset activeTestingPreset;
		bool            activeTestingGpu = getActiveGpuConfig(activeTestingPreset);
		if (activeTestingGpu)
		{
			if (lhsVendId == activeTestingPreset.mVendorId && lhsDevId == activeTestingPreset.mModelId)
			{
				return -1;
			}
			else if (rhsVendId == activeTestingPreset.mVendorId && rhsDevId == activeTestingPreset.mModelId)
			{
				return 1;
			}
		}
#endif

		if (lhsProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && rhsProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
		{
			return -1;
		}
		else if (
			lhsProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && rhsProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			return 1;
		}
		else
		{
			//compare by preset if both gpu's are of same type (integrated vs discrete)
			if (lhsPreset != rhsPreset)
			{
				return lhsPreset > rhsPreset ? -1 : 1;
			}
			else
			{
				//if presets are the same then sort by vram size
				VkDeviceSize totalLhsVram = 0;
				VkDeviceSize totalRhsVram = 0;
				for (uint32_t i = 0; i < lhsMemoryProps.memoryHeapCount; ++i)
				{
					if (VK_MEMORY_HEAP_DEVICE_LOCAL_BIT & lhsMemoryProps.memoryHeaps[i].flags)
						totalLhsVram += lhsMemoryProps.memoryHeaps[i].size;
				}
				for (uint32_t i = 0; i < rhsMemoryProps.memoryHeapCount; ++i)
				{
					if (VK_MEMORY_HEAP_DEVICE_LOCAL_BIT & rhsMemoryProps.memoryHeaps[i].flags)
						totalRhsVram += rhsMemoryProps.memoryHeaps[i].size;
				}

				return totalLhsVram > totalRhsVram ? -1 : 1;
			}
		}
	});

	// Find gpu that supports atleast graphics
	pRenderer->mActiveGPUIndex = UINT32_MAX;

	typedef struct VkQueueFamily
	{
		uint32_t     mQueueCount;
		uint32_t     mTotalQueues;
		VkQueueFlags mQueueFlags;
	} VkQueueFamily;
	tinystl::vector<VkQueueFamily> queueFamilies;

	for (uint32_t gpu_index = 0; gpu_index < pRenderer->mNumOfGPUs; ++gpu_index)
	{
		VkPhysicalDevice gpu = pRenderer->pVkGPUs[gpu_index];

		uint32_t count = 0;
		//get count of queue families
		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, NULL);

		queueFamilies.resize(count);

		VkQueueFamilyProperties* properties = (VkQueueFamilyProperties*)conf_calloc(count, sizeof(*properties));
		ASSERT(properties);
		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, properties);

		//get count of queues and their flags
		for (uint i = 0; i < count; i++)
		{
			queueFamilies[i].mQueueFlags = properties[i].queueFlags;
			queueFamilies[i].mTotalQueues = properties[i].queueCount;
			queueFamilies[i].mQueueCount = 0;
		}
		SAFE_FREE(properties);

		uint32_t graphics_queue_family_index = UINT32_MAX;
		uint32_t graphics_queue_index = 0;

		//for each of the queue families for this gpu
		//add one graphics and one present queue
		//save queue families for later queue creation
		for (int i = 0; i < queueFamilies.size(); i++)
		{
			//get graphics queue family
			if (queueFamilies[i].mTotalQueues - queueFamilies[i].mQueueCount > 0 &&
				(queueFamilies[i].mQueueFlags & VK_QUEUE_GRAPHICS_BIT) && graphics_queue_family_index == UINT32_MAX)
			{
				graphics_queue_family_index = i;
				graphics_queue_index = queueFamilies[i].mQueueCount;
				queueFamilies[i].mQueueCount++;
			}
		}

		//if no graphics queues then go to next gpu
		if (graphics_queue_family_index == UINT32_MAX)
			continue;

		if ((UINT32_MAX != graphics_queue_family_index))
		{
			pRenderer->pVkActiveGPU = gpu;
			pRenderer->mActiveGPUIndex = gpu_index;
			break;
		}
	}
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkActiveGPU);

	uint32_t              count = 0;
	VkLayerProperties     layers[100];
	VkExtensionProperties exts[100];
	vkEnumerateDeviceLayerProperties(pRenderer->pVkActiveGPU, &count, NULL);
	vkEnumerateDeviceLayerProperties(pRenderer->pVkActiveGPU, &count, layers);
	for (uint32_t i = 0; i < count; ++i)
	{
		internal_log(LOG_TYPE_INFO, layers[i].layerName, "vkdevice-layer");
		if (strcmp(layers[i].layerName, "VK_LAYER_RENDERDOC_Capture") == 0)
			gRenderDocLayerEnabled = true;
	}
	vkEnumerateDeviceExtensionProperties(pRenderer->pVkActiveGPU, NULL, &count, NULL);
	vkEnumerateDeviceExtensionProperties(pRenderer->pVkActiveGPU, NULL, &count, exts);
	for (uint32_t i = 0; i < count; ++i)
	{
		internal_log(LOG_TYPE_INFO, exts[i].extensionName, "vkdevice-ext");
	}

	for (uint32_t i = 0; i < pRenderer->mNumOfGPUs; ++i)
	{
		// Get memory properties
		vkGetPhysicalDeviceMemoryProperties(pRenderer->pVkGPUs[i], &(pRenderer->mVkGpuMemoryProperties[i]));

		// Get features
		vkGetPhysicalDeviceFeatures(pRenderer->pVkGPUs[i], &pRenderer->mVkGpuFeatures[i]);

		// Get device properties
		VkPhysicalDeviceSubgroupProperties subgroupProperties;
		subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
		subgroupProperties.pNext = NULL;

		pRenderer->mVkGpuProperties[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
		pRenderer->mVkGpuProperties[i].pNext = &subgroupProperties;
		vkGetPhysicalDeviceProperties2(pRenderer->pVkGPUs[i], &(pRenderer->mVkGpuProperties[i]));

		// Get queue family properties
		vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->pVkGPUs[i], &(pRenderer->mVkQueueFamilyPropertyCount[i]), NULL);
		pRenderer->mVkQueueFamilyProperties[i] =
			(VkQueueFamilyProperties*)conf_calloc(pRenderer->mVkQueueFamilyPropertyCount[i], sizeof(VkQueueFamilyProperties));
		vkGetPhysicalDeviceQueueFamilyProperties(
			pRenderer->pVkGPUs[i], &(pRenderer->mVkQueueFamilyPropertyCount[i]), pRenderer->mVkQueueFamilyProperties[i]);

		pRenderer->mGpuSettings[i].mUniformBufferAlignment =
			pRenderer->mVkGpuProperties[i].properties.limits.minUniformBufferOffsetAlignment;
		pRenderer->mGpuSettings[i].mMaxVertexInputBindings = pRenderer->mVkGpuProperties[i].properties.limits.maxVertexInputBindings;
		pRenderer->mGpuSettings[i].mMultiDrawIndirect = pRenderer->mVkGpuProperties[i].properties.limits.maxDrawIndirectCount > 1;
		pRenderer->mGpuSettings[i].mWaveLaneCount = subgroupProperties.subgroupSize;

		//save vendor and model Id as string
		sprintf(pRenderer->mGpuSettings[i].mGpuVendorPreset.mModelId, "%#x", pRenderer->mVkGpuProperties[i].properties.deviceID);
		sprintf(pRenderer->mGpuSettings[i].mGpuVendorPreset.mVendorId, "%#x", pRenderer->mVkGpuProperties[i].properties.vendorID);
		strncpy(
			pRenderer->mGpuSettings[i].mGpuVendorPreset.mGpuName, pRenderer->mVkGpuProperties[i].properties.deviceName,
			MAX_GPU_VENDOR_STRING_LENGTH);

		//TODO: Fix once vulkan adds support for revision ID
		strncpy(pRenderer->mGpuSettings[i].mGpuVendorPreset.mRevisionId, "0x00", MAX_GPU_VENDOR_STRING_LENGTH);
		pRenderer->mGpuSettings[i].mGpuVendorPreset.mPresetLevel = getGPUPresetLevel(
			pRenderer->mGpuSettings[i].mGpuVendorPreset.mVendorId, pRenderer->mGpuSettings[i].mGpuVendorPreset.mModelId,
			pRenderer->mGpuSettings[i].mGpuVendorPreset.mRevisionId);
	}

	pRenderer->pVkActiveGPUProperties = &pRenderer->mVkGpuProperties[pRenderer->mActiveGPUIndex];
	pRenderer->pVkActiveGpuMemoryProperties = &pRenderer->mVkGpuMemoryProperties[pRenderer->mActiveGPUIndex];
	pRenderer->pActiveGpuSettings = &pRenderer->mGpuSettings[pRenderer->mActiveGPUIndex];

	LOGINFOF("Vendor id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mVendorId);
	LOGINFOF("Model id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mModelId);
	LOGINFOF("Name of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mGpuName);

	// need a queue_priorite for each queue in the queue family we create
	tinystl::vector<tinystl::vector<float> > queue_priorities(queueFamilies.size());

	uint32_t queue_create_infos_count = 0;
	DECLARE_ZERO(VkDeviceQueueCreateInfo, queue_create_infos[4]);

	//create all queue families with maximum amount of queues
	for (int i = 0; i < queueFamilies.size(); i++)
	{
		if (queueFamilies[i].mTotalQueues > 0)
		{
			queue_create_infos[queue_create_infos_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_create_infos[queue_create_infos_count].pNext = NULL;
			queue_create_infos[queue_create_infos_count].flags = 0;
			queue_create_infos[queue_create_infos_count].queueFamilyIndex = i;
			queue_create_infos[queue_create_infos_count].queueCount = queueFamilies[i].mTotalQueues;
			queue_priorities[i].resize(queueFamilies[i].mTotalQueues);
			memset(queue_priorities[i].data(), 1, queue_priorities[i].size() * sizeof(float));
			queue_create_infos[queue_create_infos_count].pQueuePriorities = queue_priorities[i].data();
			queue_create_infos_count++;
		}
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
		const char*                  layer_name = NULL;
		uint32_t                     initialCount = sizeof(gVkWantedDeviceExtensions) / sizeof(gVkWantedDeviceExtensions[0]);
		const uint32_t               userRequestedCount = (uint32_t)pRenderer->mSettings.mDeviceExtensions.size();
		tinystl::vector<const char*> wantedDeviceExtensions(initialCount + userRequestedCount);
		for (uint32_t i = 0; i < initialCount; ++i)
		{
			wantedDeviceExtensions[i] = gVkWantedDeviceExtensions[i];
		}
		for (uint32_t i = 0; i < userRequestedCount; ++i)
		{
			wantedDeviceExtensions[initialCount + i] = pRenderer->mSettings.mDeviceExtensions[i];
		}
		const uint32_t wanted_extension_count = (uint32_t)wantedDeviceExtensions.size();
		uint32_t       count = 0;
		vkEnumerateDeviceExtensionProperties(pRenderer->pVkActiveGPU, layer_name, &count, NULL);
		if (count > 0)
		{
			VkExtensionProperties* properties = (VkExtensionProperties*)conf_calloc(count, sizeof(*properties));
			ASSERT(properties != NULL);
			vkEnumerateDeviceExtensionProperties(pRenderer->pVkActiveGPU, layer_name, &count, properties);
			for (uint32_t j = 0; j < count; ++j)
			{
				for (uint32_t k = 0; k < wanted_extension_count; ++k)
				{
					if (strcmp(wantedDeviceExtensions[k], properties[j].extensionName) == 0)
					{
						pRenderer->gVkDeviceExtensions[extension_count++] = wantedDeviceExtensions[k];
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
						if (strcmp(wantedDeviceExtensions[k], VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME) == 0)
						{
							//ballot supported
							int i = 0;
							i++;
						}
						if (strcmp(wantedDeviceExtensions[k], VK_EXT_SHADER_SUBGROUP_VOTE_EXTENSION_NAME) == 0)
						{
							//ballot supported
							int i = 0;
							i++;
						}
						break;
					}
				}
			}
			SAFE_FREE((void*)properties);
		}
	}

	// Add more extensions here
	VkPhysicalDeviceFeatures gpu_features = { 0 };
	vkGetPhysicalDeviceFeatures(pRenderer->pVkActiveGPU, &gpu_features);

	DECLARE_ZERO(VkDeviceCreateInfo, create_info);
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.queueCreateInfoCount = queue_create_infos_count;
	create_info.pQueueCreateInfos = queue_create_infos;
	create_info.enabledLayerCount = 0;
	create_info.ppEnabledLayerNames = NULL;
	create_info.enabledExtensionCount = extension_count;
	create_info.ppEnabledExtensionNames = pRenderer->gVkDeviceExtensions;
	create_info.pEnabledFeatures = &gpu_features;
	/************************************************************************/
	// Add Device Group Extension if requested and available
	/************************************************************************/
	if (pRenderer->mSettings.mGpuMode == GPU_MODE_LINKED)
	{
		create_info.pNext = &deviceGroupInfo;
	}
	vk_res = vkCreateDevice(pRenderer->pVkActiveGPU, &create_info, NULL, &(pRenderer->pVkDevice));
	ASSERT(VK_SUCCESS == vk_res);

	// Load Vulkan device functions to bypass loader
	volkLoadDevice(pRenderer->pVkDevice);

	queue_priorities.clear();

	gDedicatedAllocationExtension = dedicatedAllocationExtension && memoryReq2Extension;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
	gExternalMemoryExtension = externalMemoryExtension && externalMemoryWin32Extension;
#endif

	if (gDedicatedAllocationExtension)
	{
		LOGINFOF("Successfully loaded Dedicated Allocation extension");
	}

	if (gExternalMemoryExtension)
	{
		LOGINFOF("Successfully loaded External Memory extension");
	}

#ifdef VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME
	if (gDrawIndirectCountExtension)
	{
		pfnVkCmdDrawIndirectCountKHR = vkCmdDrawIndirectCountKHR;
		pfnVkCmdDrawIndexedIndirectCountKHR = vkCmdDrawIndexedIndirectCountKHR;
		LOGINFOF("Successfully loaded Draw Indirect extension");
	}
	else if (gAMDDrawIndirectCountExtension)
#endif
	{
		pfnVkCmdDrawIndirectCountKHR = vkCmdDrawIndirectCountAMD;
		pfnVkCmdDrawIndexedIndirectCountKHR = vkCmdDrawIndexedIndirectCountAMD;
		LOGINFOF("Successfully loaded AMD Draw Indirect extension");
	}

	if (gAMDGCNShaderExtension)
	{
		LOGINFOF("Successfully loaded AMD GCN Shader extension");
	}

	if (gDescriptorIndexingExtension)
	{
		LOGINFOF("Successfully loaded Descriptor Indexing extension");
	}

#ifdef USE_DEBUG_UTILS_EXTENSION
	gDebugMarkerSupport =
		vkCmdBeginDebugUtilsLabelEXT && vkCmdEndDebugUtilsLabelEXT && vkCmdInsertDebugUtilsLabelEXT && vkSetDebugUtilsObjectNameEXT;
#endif
}

static void RemoveDevice(Renderer* pRenderer)
{
	for (uint32_t i = 0; i < pRenderer->mNumOfGPUs; ++i)
	{
		SAFE_FREE(pRenderer->mVkQueueFamilyProperties[i]);
	}

	vkDestroyDevice(pRenderer->pVkDevice, NULL);
}

#if defined(__cplusplus) && defined(ENABLE_RENDERER_RUNTIME_SWITCH)
namespace vk {
#endif
/************************************************************************/
// Renderer Init Remove
/************************************************************************/
void initRenderer(const char* app_name, const RendererDesc* settings, Renderer** ppRenderer)
{
	Renderer* pRenderer = (Renderer*)conf_calloc(1, sizeof(*pRenderer));
	ASSERT(pRenderer);

	pRenderer->pName = (char*)conf_calloc(strlen(app_name) + 1, sizeof(char));
	memcpy(pRenderer->pName, app_name, strlen(app_name));

	// Copy settings
	memcpy(&(pRenderer->mSettings), settings, sizeof(*settings));
	pRenderer->mSettings.mApi = RENDERER_API_VULKAN;

	// Initialize the Vulkan internal bits
	{
#if defined(_DEBUG)
		// this turns on all validation layers
		pRenderer->mInstanceLayers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

		// this turns on render doc layer for gpu capture
#ifdef USE_RENDER_DOC
		pRenderer->mInstanceLayers.push_back("VK_LAYER_RENDERDOC_Capture");
#endif

		// Add user specified instance layers for instance creation
		for (uint32_t i = 0; i < (uint32_t)settings->mInstanceLayers.size(); ++i)
			pRenderer->mInstanceLayers.push_back(settings->mInstanceLayers[i]);

		VkResult vkRes = volkInitialize();
		if (vkRes != VK_SUCCESS)
		{
			LOGERROR("Failed to initialize Vulkan");
			return;
		}

		CreateInstance(app_name, pRenderer);
		AddDevice(pRenderer);
		//anything below LOW preset is not supported and we will exit
		if (pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel < GPU_PRESET_LOW)
		{
			//have the condition in the assert as well so its cleared when the assert message box appears

			ASSERT(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel >= GPU_PRESET_LOW);

			SAFE_FREE(pRenderer->pName);

			//remove device and any memory we allocated in just above as this is the first function called
			//when initializing the forge
			RemoveDevice(pRenderer);
			RemoveInstance(pRenderer);
			SAFE_FREE(pRenderer);
			LOGERROR("Selected GPU has an Office Preset in gpu.cfg.");
			LOGERROR("Office preset is not supported by The Forge.");

			//return NULL pRenderer so that client can gracefully handle exit
			//This is better than exiting from here in case client has allocated memory or has fallbacks
			ppRenderer = NULL;
			return;
		}
		/************************************************************************/
		/************************************************************************/
		VmaAllocatorCreateInfo createInfo = { 0 };
		createInfo.device = pRenderer->pVkDevice;
		createInfo.physicalDevice = pRenderer->pVkActiveGPU;

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

		createInfo.pVulkanFunctions = &vulkanFunctions;

		vmaCreateAllocator(&createInfo, &pRenderer->pVmaAllocator);

		add_descriptor_heap(
			pRenderer, gDefaultDescriptorSets, 0, gDescriptorHeapPoolSizes, VK_DESCRIPTOR_TYPE_RANGE_SIZE, &pRenderer->pDescriptorPool);
	}

	create_default_resources(pRenderer);

	// Renderer is good! Assign it to result!
	*(ppRenderer) = pRenderer;
}

void removeRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);

	SAFE_FREE(pRenderer->pName);

	destroy_default_resources(pRenderer);

	// Remove the renderpasses
	for (tinystl::unordered_hash_node<ThreadID, RenderPassMap>& t : gRenderPassMap)
		for (RenderPassMapNode& it : t.second)
			remove_render_pass(pRenderer, it.second);

	for (tinystl::unordered_hash_node<ThreadID, FrameBufferMap>& t : gFrameBufferMap)
		for (FrameBufferMapNode& it : t.second)
			remove_framebuffer(pRenderer, it.second);

	gRenderPassMap.clear();
	gFrameBufferMap.clear();

	// Destroy the Vulkan bits
	remove_descriptor_heap(pRenderer, pRenderer->pDescriptorPool);
	vmaDestroyAllocator(pRenderer->pVmaAllocator);

	RemoveDevice(pRenderer);
	RemoveInstance(pRenderer);

	pRenderer->mInstanceLayers.~vector();

	// Free all the renderer components!
	SAFE_FREE(pRenderer);
}
/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void addFence(Renderer* pRenderer, Fence** ppFence)
{
	ASSERT(pRenderer);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

	Fence* pFence = (Fence*)conf_calloc(1, sizeof(*pFence));
	ASSERT(pFence);

	DECLARE_ZERO(VkFenceCreateInfo, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	add_info.pNext = NULL;
	add_info.flags = 0;
	VkResult vk_res = vkCreateFence(pRenderer->pVkDevice, &add_info, NULL, &(pFence->pVkFence));
	ASSERT(VK_SUCCESS == vk_res);

	pFence->mSubmitted = false;

	*ppFence = pFence;
}

void removeFence(Renderer* pRenderer, Fence* pFence)
{
	ASSERT(pRenderer);
	ASSERT(pFence);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pFence->pVkFence);

	vkDestroyFence(pRenderer->pVkDevice, pFence->pVkFence, NULL);

	SAFE_FREE(pFence);
}

void addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
	ASSERT(pRenderer);

	Semaphore* pSemaphore = (Semaphore*)conf_calloc(1, sizeof(*pSemaphore));
	ASSERT(pSemaphore);

	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

	DECLARE_ZERO(VkSemaphoreCreateInfo, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	add_info.pNext = NULL;
	add_info.flags = 0;
	VkResult vk_res = vkCreateSemaphore(pRenderer->pVkDevice, &add_info, NULL, &(pSemaphore->pVkSemaphore));
	ASSERT(VK_SUCCESS == vk_res);
	// Set signal inital state.
	pSemaphore->mSignaled = false;

	*ppSemaphore = pSemaphore;
}

void removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
	ASSERT(pRenderer);
	ASSERT(pSemaphore);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pSemaphore->pVkSemaphore);

	vkDestroySemaphore(pRenderer->pVkDevice, pSemaphore->pVkSemaphore, NULL);

	SAFE_FREE(pSemaphore);
}

void addQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
{
	ASSERT(pDesc != NULL);

	uint32_t       queueFamilyIndex = -1;
	VkQueueFlags   requiredFlags = util_to_vk_queue_flags(pDesc->mType);
	uint32_t       queueIndex = -1;
	bool           found = false;
	const uint32_t nodeIndex = pDesc->mNodeIndex;

	// Try to find a dedicated queue of this type
	for (uint32_t index = 0; index < pRenderer->mVkQueueFamilyPropertyCount[nodeIndex]; ++index)
	{
		VkQueueFlags queueFlags = pRenderer->mVkQueueFamilyProperties[nodeIndex][index].queueFlags;
		if ((queueFlags & requiredFlags) && ((queueFlags & ~requiredFlags) == 0) &&
			pRenderer->mVkUsedQueueCount[nodeIndex][queueFlags] < pRenderer->mVkQueueFamilyProperties[nodeIndex][index].queueCount)
		{
			found = true;
			queueFamilyIndex = index;
			queueIndex = pRenderer->mVkUsedQueueCount[nodeIndex][queueFlags];
			break;
		}
	}

	// If hardware doesn't provide a dedicated queue try to find a non-dedicated one
	if (!found)
	{
		for (uint32_t index = 0; index < pRenderer->mVkQueueFamilyPropertyCount[nodeIndex]; ++index)
		{
			VkQueueFlags queueFlags = pRenderer->mVkQueueFamilyProperties[nodeIndex][index].queueFlags;
			if ((queueFlags & requiredFlags) &&
				pRenderer->mVkUsedQueueCount[nodeIndex][queueFlags] < pRenderer->mVkQueueFamilyProperties[nodeIndex][index].queueCount)
			{
				found = true;
				queueFamilyIndex = index;
				queueIndex = pRenderer->mVkUsedQueueCount[nodeIndex][queueFlags];
				break;
			}
		}
	}

	if (!found)
	{
		found = true;
		queueFamilyIndex = 0;
		queueIndex = 0;

		LOGWARNINGF("Could not find queue of type %u. Using default queue", (uint32_t)pDesc->mType);
	}

	if (found)
	{
		VkQueueFlags queueFlags = pRenderer->mVkQueueFamilyProperties[nodeIndex][queueFamilyIndex].queueFlags;
		Queue*       pQueueToCreate = (Queue*)conf_calloc(1, sizeof(*pQueueToCreate));
		pQueueToCreate->mVkQueueFamilyIndex = queueFamilyIndex;
		pQueueToCreate->pRenderer = pRenderer;
		pQueueToCreate->mQueueDesc = *pDesc;
		pQueueToCreate->mVkQueueIndex = queueIndex;
		//get queue handle
		vkGetDeviceQueue(
			pRenderer->pVkDevice, pQueueToCreate->mVkQueueFamilyIndex, pQueueToCreate->mVkQueueIndex, &(pQueueToCreate->pVkQueue));
		ASSERT(VK_NULL_HANDLE != pQueueToCreate->pVkQueue);
		*ppQueue = pQueueToCreate;

		++pRenderer->mVkUsedQueueCount[nodeIndex][queueFlags];
	}
	else
	{
		LOGERRORF("Cannot create queue of type (%u)", pDesc->mType);
	}
}

void removeQueue(Queue* pQueue)
{
	ASSERT(pQueue != NULL);
	const uint32_t nodeIndex = pQueue->mQueueDesc.mNodeIndex;
	VkQueueFlags   queueFlags = pQueue->pRenderer->mVkQueueFamilyProperties[nodeIndex][pQueue->mVkQueueFamilyIndex].queueFlags;
	--pQueue->pRenderer->mVkUsedQueueCount[nodeIndex][queueFlags];
	SAFE_FREE(pQueue);
}

void addCmdPool(Renderer* pRenderer, Queue* pQueue, bool transient, CmdPool** ppCmdPool)
{
	ASSERT(pRenderer);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

	CmdPool* pCmdPool = (CmdPool*)conf_calloc(1, sizeof(*pCmdPool));
	ASSERT(pCmdPool);

	pCmdPool->mCmdPoolDesc = { pQueue->mQueueDesc.mType };
	pCmdPool->pQueue = pQueue;

	DECLARE_ZERO(VkCommandPoolCreateInfo, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	add_info.pNext = NULL;
	add_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	add_info.queueFamilyIndex = pQueue->mVkQueueFamilyIndex;
	if (transient)
	{
		add_info.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	}
	VkResult vk_res = vkCreateCommandPool(pRenderer->pVkDevice, &add_info, NULL, &(pCmdPool->pVkCmdPool));
	ASSERT(VK_SUCCESS == vk_res);

	*ppCmdPool = pCmdPool;
}

void removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	ASSERT(pRenderer);
	ASSERT(pCmdPool);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pCmdPool->pVkCmdPool);

	vkDestroyCommandPool(pRenderer->pVkDevice, pCmdPool->pVkCmdPool, NULL);

	SAFE_FREE(pCmdPool);
}

void addCmd(CmdPool* pCmdPool, bool secondary, Cmd** ppCmd)
{
	ASSERT(pCmdPool);
	ASSERT(VK_NULL_HANDLE != pCmdPool->pQueue->pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pCmdPool->pVkCmdPool);

	Cmd* pCmd = (Cmd*)conf_calloc(1, sizeof(*pCmd));
	ASSERT(pCmd);

	pCmd->pRenderer = pCmdPool->pQueue->pRenderer;
	pCmd->pCmdPool = pCmdPool;
	pCmd->mNodeIndex = pCmdPool->pQueue->mQueueDesc.mNodeIndex;

	DECLARE_ZERO(VkCommandBufferAllocateInfo, alloc_info);
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.commandPool = pCmdPool->pVkCmdPool;
	alloc_info.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	VkResult vk_res = vkAllocateCommandBuffers(pCmd->pRenderer->pVkDevice, &alloc_info, &(pCmd->pVkCmdBuf));
	ASSERT(VK_SUCCESS == vk_res);

	if (pCmdPool->mCmdPoolDesc.mCmdPoolType == CMD_POOL_DIRECT)
	{
		pCmd->pBoundColorFormats = (uint32_t*)conf_calloc(MAX_RENDER_TARGET_ATTACHMENTS, sizeof(uint32_t));
		pCmd->pBoundSrgbValues = (bool*)conf_calloc(MAX_RENDER_TARGET_ATTACHMENTS, sizeof(bool));
	}

	*ppCmd = pCmd;
}

void removeCmd(CmdPool* pCmdPool, Cmd* pCmd)
{
	ASSERT(pCmdPool);
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pCmdPool->pVkCmdPool);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	if (pCmd->pDescriptorPool)
		remove_descriptor_heap(pCmd->pRenderer, pCmd->pDescriptorPool);

	if (pCmd->pBoundColorFormats)
		SAFE_FREE(pCmd->pBoundColorFormats);

	if (pCmd->pBoundSrgbValues)
		SAFE_FREE(pCmd->pBoundSrgbValues);

	vkFreeCommandBuffers(pCmd->pRenderer->pVkDevice, pCmdPool->pVkCmdPool, 1, &(pCmd->pVkCmdBuf));

	SAFE_FREE(pCmd);
}

void addCmd_n(CmdPool* pCmdPool, bool secondary, uint32_t cmdCount, Cmd*** pppCmd)
{
	ASSERT(pppCmd);

	Cmd** ppCmd = (Cmd**)conf_calloc(cmdCount, sizeof(*ppCmd));
	ASSERT(ppCmd);

	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		::addCmd(pCmdPool, secondary, &(ppCmd[i]));
	}

	*pppCmd = ppCmd;
}

void removeCmd_n(CmdPool* pCmdPool, uint32_t cmdCount, Cmd** ppCmd)
{
	ASSERT(ppCmd);

	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		::removeCmd(pCmdPool, ppCmd[i]);
	}

	SAFE_FREE(ppCmd);
}

void toggleVSync(Renderer* pRenderer, SwapChain** ppSwapChain)
{
	//toggle vsync on or off
	//for Vulkan we need to remove the SwapChain and recreate it with correct vsync option
	(*ppSwapChain)->mDesc.mEnableVsync = !(*ppSwapChain)->mDesc.mEnableVsync;
	SwapChainDesc desc = (*ppSwapChain)->mDesc;
	removeSwapChain(pRenderer, *ppSwapChain);
	addSwapChain(pRenderer, &desc, ppSwapChain);
}

void addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppSwapChain);

	SwapChain* pSwapChain = (SwapChain*)conf_calloc(1, sizeof(*pSwapChain));
	pSwapChain->mDesc = *pDesc;

	/************************************************************************/
	// Create surface
	/************************************************************************/
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkInstance);
	VkResult vk_res;
	// Create a WSI surface for the window:
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	DECLARE_ZERO(VkWin32SurfaceCreateInfoKHR, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.hinstance = ::GetModuleHandle(NULL);
	add_info.hwnd = (HWND)pDesc->pWindow->handle;
	vk_res = vkCreateWin32SurfaceKHR(pRenderer->pVkInstance, &add_info, NULL, &pSwapChain->pVkSurface);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	DECLARE_ZERO(VkXlibSurfaceCreateInfoKHR, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.dpy = pDesc->pWindow->display;           //TODO
	add_info.window = pDesc->pWindow->xlib_window;    //TODO

	vk_res = vkCreateXlibSurfaceKHR(pRenderer->pVkInstance, &add_info, NULL, &pSwapChain->pVkSurface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	DECLARE_ZERO(VkXcbSurfaceCreateInfoKHR, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.connection = pDesc->pWindow->connection;    //TODO
	add_info.window = pDesc->pWindow->xcb_window;        //TODO

	vk_res = vkCreateXcbSurfaceKHR(pRenderer->pVkInstance, &add_info, NULL, &pSwapChain->pVkSurface);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
	// Add IOS support here
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
	// Add MacOS support here
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
	DECLARE_ZERO(VkAndroidSurfaceCreateInfoKHR, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	add_info.pNext = nullptr;
	add_info.flags = 0;
	add_info.window = (ANativeWindow*)pDesc->pWindow->handle;
	vk_res = vkCreateAndroidSurfaceKHR(pRenderer->pVkInstance, &add_info, nullptr, &pSwapChain->pVkSurface);
#else
#error PLATFORM NOT SUPPORTED
#endif
	ASSERT(VK_SUCCESS == vk_res);
	/************************************************************************/
	// Create swap chain
	/************************************************************************/
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkActiveGPU);

	// Most GPUs will not go beyond VK_SAMPLE_COUNT_8_BIT
	ASSERT(0 != (pRenderer->pVkActiveGPUProperties->properties.limits.framebufferColorSampleCounts & pSwapChain->mDesc.mSampleCount));

	// Image count
	if (0 == pSwapChain->mDesc.mImageCount)
	{
		pSwapChain->mDesc.mImageCount = 2;
	}

	DECLARE_ZERO(VkSurfaceCapabilitiesKHR, caps);
	vk_res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pRenderer->pVkActiveGPU, pSwapChain->pVkSurface, &caps);
	ASSERT(VK_SUCCESS == vk_res);

	if ((caps.maxImageCount > 0) && (pSwapChain->mDesc.mImageCount > caps.maxImageCount))
	{
		pSwapChain->mDesc.mImageCount = caps.maxImageCount;
	}

	// Surface format
	DECLARE_ZERO(VkSurfaceFormatKHR, surface_format);
	surface_format.format = VK_FORMAT_UNDEFINED;
	uint32_t            surfaceFormatCount = 0;
	VkSurfaceFormatKHR* formats = NULL;

	// Get surface formats count
	vk_res = vkGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->pVkActiveGPU, pSwapChain->pVkSurface, &surfaceFormatCount, NULL);
	ASSERT(VK_SUCCESS == vk_res);

	// Allocate and get surface formats
	formats = (VkSurfaceFormatKHR*)conf_calloc(surfaceFormatCount, sizeof(*formats));
	vk_res = vkGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->pVkActiveGPU, pSwapChain->pVkSurface, &surfaceFormatCount, formats);
	ASSERT(VK_SUCCESS == vk_res);

	if ((1 == surfaceFormatCount) && (VK_FORMAT_UNDEFINED == formats[0].format))
	{
		surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
		surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	}
	else
	{
		VkFormat requested_format = util_to_vk_image_format(pSwapChain->mDesc.mColorFormat, pSwapChain->mDesc.mSrgb);
		for (uint32_t i = 0; i < surfaceFormatCount; ++i)
		{
			if ((requested_format == formats[i].format) && (VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == formats[i].colorSpace))
			{
				surface_format.format = requested_format;
				surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
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

	ASSERT(VK_FORMAT_UNDEFINED != surface_format.format);

	// Free formats
	SAFE_FREE(formats);

	// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
	// This mode waits for the vertical blank ("v-sync")
	VkPresentModeKHR  present_mode = VK_PRESENT_MODE_FIFO_KHR;
	uint32_t          swapChainImageCount = 0;
	VkPresentModeKHR* modes = NULL;
	// Get present mode count
	vk_res = vkGetPhysicalDeviceSurfacePresentModesKHR(pRenderer->pVkActiveGPU, pSwapChain->pVkSurface, &swapChainImageCount, NULL);
	ASSERT(VK_SUCCESS == vk_res);

	// Allocate and get present modes
	modes = (VkPresentModeKHR*)conf_calloc(swapChainImageCount, sizeof(*modes));
	vk_res = vkGetPhysicalDeviceSurfacePresentModesKHR(pRenderer->pVkActiveGPU, pSwapChain->pVkSurface, &swapChainImageCount, modes);
	ASSERT(VK_SUCCESS == vk_res);

	// If v-sync is not requested, try to find a mailbox mode
	// It's the lowest latency non-tearing present mode available
	if (!pSwapChain->mDesc.mEnableVsync)
	{
		for (uint32_t i = 0; i < swapChainImageCount; ++i)
		{
			if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
			if ((present_mode != VK_PRESENT_MODE_MAILBOX_KHR) && (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
			{
				present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
		}
	}

	// Free modes
	SAFE_FREE(modes);

	// Swapchain
	VkExtent2D extent = { 0 };
	extent.width = pSwapChain->mDesc.mWidth;
	extent.height = pSwapChain->mDesc.mHeight;

	VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
	uint32_t      queue_family_index_count = 0;
	uint32_t      queue_family_indices[2] = { pDesc->ppPresentQueues[0]->mVkQueueFamilyIndex, 0 };
	uint32_t      presentQueueFamilyIndex = -1;
	uint32_t      nodeIndex = 0;

	// Check if hardware provides dedicated present queue
	if (0 != pRenderer->mVkQueueFamilyPropertyCount[nodeIndex])
	{
		for (uint32_t index = 0; index < pRenderer->mVkQueueFamilyPropertyCount[nodeIndex]; ++index)
		{
			VkBool32 supports_present = VK_FALSE;
			VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(pRenderer->pVkActiveGPU, index, pSwapChain->pVkSurface, &supports_present);
			if ((VK_SUCCESS == res) && (VK_TRUE == supports_present) && pSwapChain->mDesc.ppPresentQueues[0]->mVkQueueFamilyIndex != index)
			{
				presentQueueFamilyIndex = index;
				break;
			}
		}

		// If there is no dedicated present queue, just find the first available queue which supports present
		if (presentQueueFamilyIndex == -1)
		{
			for (uint32_t index = 0; index < pRenderer->mVkQueueFamilyPropertyCount[nodeIndex]; ++index)
			{
				VkBool32 supports_present = VK_FALSE;
				VkResult res =
					vkGetPhysicalDeviceSurfaceSupportKHR(pRenderer->pVkActiveGPU, index, pSwapChain->pVkSurface, &supports_present);
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
	if (presentQueueFamilyIndex != -1 && queue_family_indices[0] != presentQueueFamilyIndex)
	{
		queue_family_indices[1] = presentQueueFamilyIndex;

		vkGetDeviceQueue(pRenderer->pVkDevice, queue_family_indices[1], 0, &pSwapChain->pPresentQueue);
		sharing_mode = VK_SHARING_MODE_CONCURRENT;
		queue_family_index_count = 2;
	}
	else
	{
		pSwapChain->pPresentQueue = VK_NULL_HANDLE;
	}

	DECLARE_ZERO(VkSwapchainCreateInfoKHR, swapChainCreateInfo);
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.pNext = NULL;
	swapChainCreateInfo.flags = 0;
	swapChainCreateInfo.surface = pSwapChain->pVkSurface;
#ifndef VK_USE_PLATFORM_ANDROID_KHR
	swapChainCreateInfo.minImageCount = pSwapChain->mDesc.mImageCount;
#else
	//TODO: thomas Fixme hack
	swapChainCreateInfo.minImageCount = caps.minImageCount;
#endif
	swapChainCreateInfo.imageFormat = surface_format.format;
	swapChainCreateInfo.imageColorSpace = surface_format.colorSpace;
	swapChainCreateInfo.imageExtent = extent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	swapChainCreateInfo.imageSharingMode = sharing_mode;
	swapChainCreateInfo.queueFamilyIndexCount = queue_family_index_count;
	swapChainCreateInfo.pQueueFamilyIndices = queue_family_indices;
	swapChainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.presentMode = present_mode;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = 0;
	vk_res = vkCreateSwapchainKHR(pRenderer->pVkDevice, &swapChainCreateInfo, NULL, &(pSwapChain->pSwapChain));
	ASSERT(VK_SUCCESS == vk_res);

	pSwapChain->mDesc.mColorFormat = util_to_internal_image_format(surface_format.format);

	// Create rendertargets from swapchain
	uint32_t image_count = 0;
	vk_res = vkGetSwapchainImagesKHR(pRenderer->pVkDevice, pSwapChain->pSwapChain, &image_count, NULL);
	ASSERT(VK_SUCCESS == vk_res);

	ASSERT(image_count == pSwapChain->mDesc.mImageCount);

	pSwapChain->ppVkSwapChainImages = (VkImage*)conf_calloc(image_count, sizeof(*pSwapChain->ppVkSwapChainImages));
	ASSERT(pSwapChain->ppVkSwapChainImages);

	vk_res = vkGetSwapchainImagesKHR(pRenderer->pVkDevice, pSwapChain->pSwapChain, &image_count, pSwapChain->ppVkSwapChainImages);
	ASSERT(VK_SUCCESS == vk_res);

	RenderTargetDesc descColor = {};
	descColor.mWidth = pSwapChain->mDesc.mWidth;
	descColor.mHeight = pSwapChain->mDesc.mHeight;
	descColor.mDepth = 1;
	descColor.mArraySize = 1;
	descColor.mFormat = pSwapChain->mDesc.mColorFormat;
	descColor.mSrgb = pSwapChain->mDesc.mSrgb;
	descColor.mClearValue = pSwapChain->mDesc.mColorClearValue;
	descColor.mSampleCount = SAMPLE_COUNT_1;
	descColor.mSampleQuality = 0;

	pSwapChain->ppSwapchainRenderTargets =
		(RenderTarget**)conf_calloc(pSwapChain->mDesc.mImageCount, sizeof(*pSwapChain->ppSwapchainRenderTargets));

	// Populate the vk_image field and add the Vulkan texture objects
	for (uint32_t i = 0; i < pSwapChain->mDesc.mImageCount; ++i)
	{
		descColor.pNativeHandle = (void*)pSwapChain->ppVkSwapChainImages[i];
		addRenderTarget(pRenderer, &descColor, &pSwapChain->ppSwapchainRenderTargets[i]);
	}
	/************************************************************************/
	/************************************************************************/

	*ppSwapChain = pSwapChain;
}

void removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pSwapChain);

	for (uint32_t i = 0; i < pSwapChain->mDesc.mImageCount; ++i)
	{
		removeRenderTarget(pRenderer, pSwapChain->ppSwapchainRenderTargets[i]);
	}

	vkDestroySwapchainKHR(pRenderer->pVkDevice, pSwapChain->pSwapChain, NULL);
	vkDestroySurfaceKHR(pRenderer->pVkInstance, pSwapChain->pVkSurface, NULL);

	SAFE_FREE(pSwapChain->ppSwapchainRenderTargets);
	SAFE_FREE(pSwapChain->ppVkSwapChainImages);
	SAFE_FREE(pSwapChain);
}

void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(pDesc->mSize > 0);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

	Buffer* pBuffer = (Buffer*)conf_calloc(1, sizeof(*pBuffer));
	ASSERT(pBuffer);

	pBuffer->mDesc = *pDesc;

	// Align the buffer size to multiples of the dynamic uniform buffer minimum size
	if (pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
	{
		uint64_t minAlignment = pRenderer->pActiveGpuSettings->mUniformBufferAlignment;
		pBuffer->mDesc.mSize = round_up_64(pBuffer->mDesc.mSize, minAlignment);
	}

	DECLARE_ZERO(VkBufferCreateInfo, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.size = pBuffer->mDesc.mSize;
	add_info.usage = util_to_vk_buffer_usage(pBuffer->mDesc.mDescriptors, pDesc->mFormat != ImageFormat::NONE);
	add_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	add_info.queueFamilyIndexCount = 0;
	add_info.pQueueFamilyIndices = NULL;

	// Buffer can be used as dest in a transfer command (Uploading data to a storage buffer, Readback query data)
	if (pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY || pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
		add_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	AllocatorMemoryRequirements vma_mem_reqs = { 0 };
	vma_mem_reqs.usage = (VmaMemoryUsage)pBuffer->mDesc.mMemoryUsage;
	vma_mem_reqs.flags = 0;
	if (pBuffer->mDesc.mFlags & BUFFER_CREATION_FLAG_OWN_MEMORY_BIT)
		vma_mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	if (pBuffer->mDesc.mFlags & BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT)
		vma_mem_reqs.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;

	BufferCreateInfo alloc_info = { &add_info };
	VkResult         vk_res = (VkResult)vk_createBuffer(pRenderer->pVmaAllocator, &alloc_info, &vma_mem_reqs, pBuffer);
	ASSERT(VK_SUCCESS == vk_res);
	/************************************************************************/
	// Buffer to be used on multiple GPUs
	/************************************************************************/
	if (pRenderer->mSettings.mGpuMode == GPU_MODE_LINKED && (pDesc->pSharedNodeIndices || pDesc->mNodeIndex))
	{
		/************************************************************************/
		// Set all the device indices to the index of the device where we will create the buffer
		/************************************************************************/
		uint32_t* pIndices = (uint32_t*)alloca(pRenderer->mVkLinkedNodeCount * sizeof(uint32_t));
		util_calculate_device_indices(pRenderer, pDesc->mNodeIndex, pDesc->pSharedNodeIndices, pDesc->mSharedNodeIndexCount, pIndices);
		/************************************************************************/
		// #TODO : Move this to the Vulkan memory allocator
		/************************************************************************/
		VkBindBufferMemoryInfoKHR            bindInfo = { VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO_KHR };
		VkBindBufferMemoryDeviceGroupInfoKHR bindDeviceGroup = { VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO_KHR };
		bindDeviceGroup.deviceIndexCount = pRenderer->mVkLinkedNodeCount;
		bindDeviceGroup.pDeviceIndices = pIndices;
		bindInfo.buffer = pBuffer->pVkBuffer;
		bindInfo.memory = pBuffer->pVkAllocation->GetMemory();
		bindInfo.memoryOffset = pBuffer->pVkAllocation->GetOffset();
		bindInfo.pNext = &bindDeviceGroup;
		vkBindBufferMemory2KHR(pRenderer->pVkDevice, 1, &bindInfo);
		/************************************************************************/
		/************************************************************************/
	}

	pBuffer->mCurrentState = RESOURCE_STATE_UNDEFINED;
	/************************************************************************/
	// Set descriptor data
	/************************************************************************/
	if ((pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER) || (pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_BUFFER) ||
		(pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER))
	{
		pBuffer->mVkBufferInfo.buffer = pBuffer->pVkBuffer;
		pBuffer->mVkBufferInfo.range = VK_WHOLE_SIZE;

		if ((pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_BUFFER) || (pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER))
		{
			pBuffer->mVkBufferInfo.offset = pBuffer->mDesc.mStructStride * pBuffer->mDesc.mFirstElement;
		}
		else
		{
			pBuffer->mVkBufferInfo.offset = 0;
		}
	}

	if (add_info.usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT)
	{
		VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, NULL };
		viewInfo.buffer = pBuffer->pVkBuffer;
		viewInfo.flags = 0;
		viewInfo.format = util_to_vk_image_format(pDesc->mFormat, false);
		viewInfo.offset = pDesc->mFirstElement * pDesc->mStructStride;
		viewInfo.range = pDesc->mElementCount * pDesc->mStructStride;
		VkFormatProperties formatProps = {};
		vkGetPhysicalDeviceFormatProperties(pRenderer->pVkActiveGPU, viewInfo.format, &formatProps);
		if (!(formatProps.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT))
		{
			LOGWARNINGF("Failed to create uniform texel buffer view for format %u", (uint32_t)pDesc->mFormat);
		}
		else
		{
			vkCreateBufferView(pRenderer->pVkDevice, &viewInfo, NULL, &pBuffer->pVkUniformTexelView);
		}
	}
	if (add_info.usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)
	{
		VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, NULL };
		viewInfo.buffer = pBuffer->pVkBuffer;
		viewInfo.flags = 0;
		viewInfo.format = util_to_vk_image_format(pDesc->mFormat, false);
		viewInfo.offset = pDesc->mFirstElement * pDesc->mStructStride;
		viewInfo.range = pDesc->mElementCount * pDesc->mStructStride;
		VkFormatProperties formatProps = {};
		vkGetPhysicalDeviceFormatProperties(pRenderer->pVkActiveGPU, viewInfo.format, &formatProps);
		if (!(formatProps.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
		{
			LOGWARNINGF("Failed to create storage texel buffer view for format %u", (uint32_t)pDesc->mFormat);
		}
		else
		{
			vkCreateBufferView(pRenderer->pVkDevice, &viewInfo, NULL, &pBuffer->pVkStorageTexelView);
		}
	}
	/************************************************************************/
	/************************************************************************/
	pBuffer->mBufferId = (++gBufferIds << 8U) + Thread::GetCurrentThreadID();

	*pp_buffer = pBuffer;
}

void removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	ASSERT(pRenderer);
	ASSERT(pBuffer);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pBuffer->pVkBuffer);

	if (pBuffer->pVkUniformTexelView)
	{
		vkDestroyBufferView(pRenderer->pVkDevice, pBuffer->pVkUniformTexelView, NULL);
		pBuffer->pVkUniformTexelView = VK_NULL_HANDLE;
	}
	if (pBuffer->pVkStorageTexelView)
	{
		vkDestroyBufferView(pRenderer->pVkDevice, pBuffer->pVkStorageTexelView, NULL);
		pBuffer->pVkStorageTexelView = VK_NULL_HANDLE;
	}

	vk_destroyBuffer(pRenderer->pVmaAllocator, pBuffer);

	SAFE_FREE(pBuffer);
}

void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mWidth && pDesc->mHeight && (pDesc->mDepth || pDesc->mArraySize));
	if (pDesc->mSampleCount > SAMPLE_COUNT_1 && pDesc->mMipLevels > 1)
	{
		LOGERROR("Multi-Sampled textures cannot have mip maps");
		ASSERT(false);
		return;
	}

	Texture* pTexture = (Texture*)conf_calloc(1, sizeof(*pTexture));
	ASSERT(pTexture);

	pTexture->mDesc = *pDesc;
	// Monotonically increasing thread safe id generation
	pTexture->mTextureId = (++gTextureIds << 8U) + Thread::GetCurrentThreadID();

	if (pDesc->pNativeHandle && !(pDesc->mFlags & TEXTURE_CREATION_FLAG_IMPORT_BIT))
	{
		pTexture->mOwnsImage = false;
		pTexture->pVkImage = (VkImage)pDesc->pNativeHandle;
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

	VkImageType image_type = VK_IMAGE_TYPE_MAX_ENUM;
	if (pDesc->mDepth > 1)
		image_type = VK_IMAGE_TYPE_3D;
	else if (pDesc->mHeight > 1)
		image_type = VK_IMAGE_TYPE_2D;
	else
		image_type = VK_IMAGE_TYPE_1D;

	DescriptorType descriptors = pDesc->mDescriptors;
	bool           cubemapRequired = (DESCRIPTOR_TYPE_TEXTURE_CUBE == (descriptors & DESCRIPTOR_TYPE_TEXTURE_CUBE));
	bool           arrayRequired = false;

	if (VK_NULL_HANDLE == pTexture->pVkImage)
	{
		DECLARE_ZERO(VkImageCreateInfo, add_info);
		add_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		add_info.pNext = NULL;
		add_info.flags = 0;
		add_info.imageType = image_type;
		add_info.format = util_to_vk_image_format(pDesc->mFormat, pDesc->mSrgb);
		add_info.extent.width = pDesc->mWidth;
		add_info.extent.height = pDesc->mHeight;
		add_info.extent.depth = pDesc->mDepth;
		add_info.mipLevels = pDesc->mMipLevels;
		add_info.arrayLayers = pDesc->mArraySize;
		add_info.samples = util_to_vk_sample_count(pDesc->mSampleCount);
		add_info.tiling = (0 != pDesc->mHostVisible) ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
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

		if (VK_IMAGE_USAGE_SAMPLED_BIT & add_info.usage)
		{
			// Make it easy to copy to and from textures
			add_info.usage |= (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		}

		// Verify that GPU supports this format
		DECLARE_ZERO(VkFormatProperties, format_props);
		vkGetPhysicalDeviceFormatProperties(pRenderer->pVkActiveGPU, add_info.format, &format_props);
		VkFormatFeatureFlags format_features = util_vk_image_usage_to_format_features(add_info.usage);
		if (pDesc->mHostVisible)
		{
			VkFormatFeatureFlags flags = format_props.linearTilingFeatures & format_features;
			ASSERT((0 != flags) && "Format is not supported for host visible images");
		}
		else
		{
			VkFormatFeatureFlags flags = format_props.optimalTilingFeatures & format_features;
			ASSERT((0 != flags) && "Format is not supported for GPU local images (i.e. not host visible images)");
		}

		AllocatorMemoryRequirements mem_reqs = { 0 };
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT)
			mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
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

		TextureCreateInfo alloc_info = { pDesc, &add_info };
		VkResult          vk_res = (VkResult)vk_createTexture(pRenderer->pVmaAllocator, &alloc_info, &mem_reqs, pTexture);
		ASSERT(VK_SUCCESS == vk_res);

		pTexture->pVkMemory = pTexture->pVkAllocation->GetMemory();
		/************************************************************************/
		// Texture to be used on multiple GPUs
		/************************************************************************/
		if (pRenderer->mSettings.mGpuMode == GPU_MODE_LINKED && (pDesc->pSharedNodeIndices || pDesc->mNodeIndex))
		{
			/************************************************************************/
			// Set all the device indices to the index of the device where we will create the texture
			/************************************************************************/
			uint32_t* pIndices = (uint32_t*)alloca(pRenderer->mVkLinkedNodeCount * sizeof(uint32_t));
			util_calculate_device_indices(pRenderer, pDesc->mNodeIndex, pDesc->pSharedNodeIndices, pDesc->mSharedNodeIndexCount, pIndices);
			/************************************************************************/
			// #TODO : Move this to the Vulkan memory allocator
			/************************************************************************/
			VkBindImageMemoryInfoKHR            bindInfo = { VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR };
			VkBindImageMemoryDeviceGroupInfoKHR bindDeviceGroup = { VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO_KHR };
			bindDeviceGroup.deviceIndexCount = pRenderer->mVkLinkedNodeCount;
			bindDeviceGroup.pDeviceIndices = pIndices;
			bindInfo.image = pTexture->pVkImage;
			bindInfo.memory = pTexture->pVkAllocation->GetMemory();
			bindInfo.memoryOffset = pTexture->pVkAllocation->GetOffset();
			bindInfo.pNext = &bindDeviceGroup;
			vkBindImageMemory2KHR(pRenderer->pVkDevice, 1, &bindInfo);
			/************************************************************************/
			/************************************************************************/
		}

		pTexture->mCurrentState = RESOURCE_STATE_UNDEFINED;
	}
	/************************************************************************/
	// Create image view
	/************************************************************************/
	VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
	switch (image_type)
	{
		case VK_IMAGE_TYPE_1D: view_type = pDesc->mArraySize > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D; break;
		case VK_IMAGE_TYPE_2D:
			if (cubemapRequired)
				view_type = (pDesc->mArraySize > 6) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
			else
				view_type = pDesc->mArraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
			break;
		case VK_IMAGE_TYPE_3D:
			if (pDesc->mArraySize > 1)
			{
				LOGERROR("Cannot support 3D Texture Array in Vulkan");
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
	srvDesc.image = pTexture->pVkImage;
	srvDesc.viewType = view_type;
	srvDesc.format = util_to_vk_image_format(pDesc->mFormat, pDesc->mSrgb);
	srvDesc.components.r = VK_COMPONENT_SWIZZLE_R;
	srvDesc.components.g = VK_COMPONENT_SWIZZLE_G;
	srvDesc.components.b = VK_COMPONENT_SWIZZLE_B;
	srvDesc.components.a = VK_COMPONENT_SWIZZLE_A;
	srvDesc.subresourceRange.aspectMask = util_vk_determine_aspect_mask(srvDesc.format);
	srvDesc.subresourceRange.baseMipLevel = 0;
	srvDesc.subresourceRange.levelCount = pDesc->mMipLevels;
	srvDesc.subresourceRange.baseArrayLayer = 0;
	srvDesc.subresourceRange.layerCount = pDesc->mArraySize;
	pTexture->mVkAspectMask = srvDesc.subresourceRange.aspectMask;
	if (descriptors & DESCRIPTOR_TYPE_TEXTURE)
	{
		VkResult vk_res = vkCreateImageView(pRenderer->pVkDevice, &srvDesc, NULL, &pTexture->pVkSRVDescriptor);
		ASSERT(VK_SUCCESS == vk_res);
	}

	// UAV
	if (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
	{
		pTexture->pVkUAVDescriptors = (VkImageView*)conf_calloc(pDesc->mMipLevels, sizeof(VkImageView));
		VkImageViewCreateInfo uavDesc = srvDesc;
		// #NOTE : We dont support imageCube, imageCubeArray for consistency with other APIs
		// All cubemaps will be used as image2DArray for Image Load / Store ops
		if (uavDesc.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY || uavDesc.viewType == VK_IMAGE_VIEW_TYPE_CUBE)
			uavDesc.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		uavDesc.subresourceRange.levelCount = 1;
		for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
		{
			uavDesc.subresourceRange.baseMipLevel = i;
			VkResult vk_res = vkCreateImageView(pRenderer->pVkDevice, &uavDesc, NULL, &pTexture->pVkUAVDescriptors[i]);
			ASSERT(VK_SUCCESS == vk_res);
		}
	}
	/************************************************************************/
	/************************************************************************/
	// Get memory requirements that covers all mip levels
	DECLARE_ZERO(VkMemoryRequirements, vk_mem_reqs);
	vkGetImageMemoryRequirements(pRenderer->pVkDevice, pTexture->pVkImage, &vk_mem_reqs);
	pTexture->mTextureSize = vk_mem_reqs.size;

	*ppTexture = pTexture;
}

void removeTexture(Renderer* pRenderer, Texture* pTexture)
{
	ASSERT(pRenderer);
	ASSERT(pTexture);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pTexture->pVkImage);

	if (pTexture->mOwnsImage)
		vk_destroyTexture(pRenderer->pVmaAllocator, pTexture);

	if (VK_NULL_HANDLE != pTexture->pVkSRVDescriptor)
		vkDestroyImageView(pRenderer->pVkDevice, pTexture->pVkSRVDescriptor, NULL);

	if (pTexture->pVkUAVDescriptors)
	{
		for (uint32_t i = 0; i < pTexture->mDesc.mMipLevels; ++i)
		{
			vkDestroyImageView(pRenderer->pVkDevice, pTexture->pVkUAVDescriptors[i], NULL);
		}
	}

	SAFE_FREE(pTexture->pVkUAVDescriptors);
	SAFE_FREE(pTexture);
}

void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppRenderTarget);

	bool isDepth = ImageFormat::IsDepthFormat(pDesc->mFormat);

	ASSERT(!((isDepth) && (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)) && "Cannot use depth stencil as UAV");

	((RenderTargetDesc*)pDesc)->mMipLevels = max(1U, pDesc->mMipLevels);

	RenderTarget* pRenderTarget = (RenderTarget*)conf_calloc(1, sizeof(*pRenderTarget));
	pRenderTarget->mDesc = *pDesc;
	TextureDesc textureDesc = {};
	textureDesc.mArraySize = pDesc->mArraySize;
	textureDesc.mClearValue = pDesc->mClearValue;
	textureDesc.mDepth = pDesc->mDepth;
	textureDesc.mFlags = pDesc->mFlags;
	textureDesc.mFormat = pDesc->mFormat;
	textureDesc.mHeight = pDesc->mHeight;
	textureDesc.mHostVisible = false;
	textureDesc.mMipLevels = pDesc->mMipLevels;
	textureDesc.mSampleCount = pDesc->mSampleCount;
	textureDesc.mSampleQuality = pDesc->mSampleQuality;
	textureDesc.mWidth = pDesc->mWidth;
	textureDesc.pNativeHandle = pDesc->pNativeHandle;
	textureDesc.mSrgb = pDesc->mSrgb;
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
		VkFormat vk_depth_stencil_format = util_to_vk_image_format(pDesc->mFormat, false);
		if (VK_FORMAT_UNDEFINED != vk_depth_stencil_format)
		{
			DECLARE_ZERO(VkImageFormatProperties, properties);
			VkResult vk_res = vkGetPhysicalDeviceImageFormatProperties(
				pRenderer->pVkActiveGPU, vk_depth_stencil_format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &properties);
			// Fall back to something that's guaranteed to work
			if (VK_SUCCESS != vk_res)
			{
				textureDesc.mFormat = ImageFormat::D16;
				LOGWARNINGF("Depth stencil format (%u) not supported. Falling back to D16 format", pDesc->mFormat);
			}
		}
	}

	::addTexture(pRenderer, &textureDesc, &pRenderTarget->pTexture);

	VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
	if (pDesc->mDepth > 1)
		viewType = VK_IMAGE_VIEW_TYPE_3D;
	else if (pDesc->mHeight > 1)
		viewType = pDesc->mArraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
	else
		viewType = pDesc->mArraySize > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;

	uint32_t depthOrArraySize = pDesc->mArraySize * pDesc->mDepth;

	VkImageViewCreateInfo rtvDesc = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL };
	rtvDesc.flags = 0;
	rtvDesc.image = pRenderTarget->pTexture->pVkImage;
	rtvDesc.viewType = viewType;
	rtvDesc.format = util_to_vk_image_format(pRenderTarget->pTexture->mDesc.mFormat, pRenderTarget->pTexture->mDesc.mSrgb);
	rtvDesc.components.r = VK_COMPONENT_SWIZZLE_R;
	rtvDesc.components.g = VK_COMPONENT_SWIZZLE_G;
	rtvDesc.components.b = VK_COMPONENT_SWIZZLE_B;
	rtvDesc.components.a = VK_COMPONENT_SWIZZLE_A;
	rtvDesc.subresourceRange.aspectMask = util_vk_determine_aspect_mask(rtvDesc.format);
	rtvDesc.subresourceRange.baseMipLevel = 0;
	rtvDesc.subresourceRange.levelCount = 1;
	rtvDesc.subresourceRange.baseArrayLayer = 0;
	rtvDesc.subresourceRange.layerCount = depthOrArraySize;

	uint32_t numRTVs = pDesc->mMipLevels;
	if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		numRTVs *= depthOrArraySize;

	pRenderTarget->pVkDescriptors = (VkImageView*)conf_calloc(numRTVs + 1, sizeof(VkImageView));
	vkCreateImageView(pRenderer->pVkDevice, &rtvDesc, NULL, &pRenderTarget->pVkDescriptors[0]);

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
				VkResult vkRes =
					vkCreateImageView(pRenderer->pVkDevice, &rtvDesc, NULL, &pRenderTarget->pVkDescriptors[1 + i * depthOrArraySize + j]);
				ASSERT(VK_SUCCESS == vkRes);
			}
		}
		else
		{
			VkResult vkRes = vkCreateImageView(pRenderer->pVkDevice, &rtvDesc, NULL, &pRenderTarget->pVkDescriptors[1 + i]);
			ASSERT(VK_SUCCESS == vkRes);
		}
	}

	*ppRenderTarget = pRenderTarget;
}

void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
	::removeTexture(pRenderer, pRenderTarget->pTexture);

	vkDestroyImageView(pRenderer->pVkDevice, pRenderTarget->pVkDescriptors[0], NULL);

	const uint32_t depthOrArraySize = pRenderTarget->mDesc.mArraySize * pRenderTarget->mDesc.mDepth;
	if ((pRenderTarget->mDesc.mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pRenderTarget->mDesc.mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
	{
		for (uint32_t i = 0; i < pRenderTarget->mDesc.mMipLevels; ++i)
			for (uint32_t j = 0; j < depthOrArraySize; ++j)
				vkDestroyImageView(pRenderer->pVkDevice, pRenderTarget->pVkDescriptors[1 + i * depthOrArraySize + j], NULL);
	}
	else
	{
		for (uint32_t i = 0; i < pRenderTarget->mDesc.mMipLevels; ++i)
			vkDestroyImageView(pRenderer->pVkDevice, pRenderTarget->pVkDescriptors[1 + i], NULL);
	}

	SAFE_FREE(pRenderTarget->pVkDescriptors);
	SAFE_FREE(pRenderTarget);
}

void addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** pp_sampler)
{
	ASSERT(pRenderer);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(pDesc->mCompareFunc < MAX_COMPARE_MODES);

	Sampler* pSampler = (Sampler*)conf_calloc(1, sizeof(*pSampler));
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
	add_info.mipLodBias = pDesc->mMipLosBias;
	add_info.anisotropyEnable = VK_FALSE;
	add_info.maxAnisotropy = pDesc->mMaxAnisotropy;
	add_info.compareEnable = (gVkComparisonFuncTranslator[pDesc->mCompareFunc] != VK_COMPARE_OP_NEVER) ? VK_TRUE : VK_FALSE;
	add_info.compareOp = gVkComparisonFuncTranslator[pDesc->mCompareFunc];
	add_info.minLod = 0.0f;
	add_info.maxLod = pDesc->mMagFilter >= FILTER_LINEAR ? FLT_MAX : 0.0f;
	add_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	add_info.unnormalizedCoordinates = VK_FALSE;

	VkResult vk_res = vkCreateSampler(pRenderer->pVkDevice, &add_info, NULL, &(pSampler->pVkSampler));
	ASSERT(VK_SUCCESS == vk_res);

	pSampler->mSamplerId = (++gSamplerIds << 8U) + Thread::GetCurrentThreadID();

	pSampler->mVkSamplerView.sampler = pSampler->pVkSampler;

	*pp_sampler = pSampler;
}

void removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
	ASSERT(pRenderer);
	ASSERT(pSampler);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pSampler->pVkSampler);

	vkDestroySampler(pRenderer->pVkDevice, pSampler->pVkSampler, NULL);

	SAFE_FREE(pSampler);
}
/************************************************************************/
// Buffer Functions
/************************************************************************/
void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
	ASSERT(pBuffer->mDesc.mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to map non-cpu accessible resource");

	uint64_t offset = pBuffer->pVkAllocation->GetOffset();
	uint64_t size = pBuffer->pVkAllocation->GetSize();
	if (pRange)
	{
		offset += pRange->mOffset;
		size = pRange->mSize;
	}

	VkResult vk_res = vkMapMemory(pRenderer->pVkDevice, pBuffer->pVkAllocation->GetMemory(), offset, size, 0, &pBuffer->pCpuMappedAddress);
	ASSERT(vk_res == VK_SUCCESS && pBuffer->pCpuMappedAddress);
}

void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	ASSERT(pBuffer->mDesc.mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to unmap non-cpu accessible resource");

	vkUnmapMemory(pRenderer->pVkDevice, pBuffer->pVkAllocation->GetMemory());
	pBuffer->pCpuMappedAddress = NULL;
}
/************************************************************************/
// Shader Functions
/************************************************************************/
// renderer shader macros allocated on stack
ShaderMacro                     _rendererShaderDefines[2];
const RendererShaderDefinesDesc get_renderer_shaderdefines(Renderer* pRenderer)
{
	// Set shader macro based on runtime information
	_rendererShaderDefines[0].definition = "VK_EXT_DESCRIPTOR_INDEXING_ENABLED";
	_rendererShaderDefines[0].value = tinystl::string::format("%d", static_cast<int>(gDescriptorIndexingExtension));
	_rendererShaderDefines[1].definition = "VK_FEATURE_TEXTURE_ARRAY_DYNAMIC_INDEXING_ENABLED";
	_rendererShaderDefines[1].value =
		tinystl::string::format("%d", static_cast<int>(pRenderer->mVkGpuFeatures[0].shaderSampledImageArrayDynamicIndexing));

	RendererShaderDefinesDesc defineDesc = { _rendererShaderDefines, 2 };
	return defineDesc;
}

void addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
	Shader* pShaderProgram = (Shader*)conf_calloc(1, sizeof(*pShaderProgram));
	pShaderProgram->mStages = pDesc->mStages;

	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

	uint32_t                        counter = 0;
	ShaderReflection                stageReflections[SHADER_STAGE_COUNT];
	tinystl::vector<VkShaderModule> modules(SHADER_STAGE_COUNT);

	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		ShaderStage stage_mask = (ShaderStage)(1 << i);
		if (stage_mask == (pShaderProgram->mStages & stage_mask))
		{
			DECLARE_ZERO(VkShaderModuleCreateInfo, create_info);
			create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			create_info.pNext = NULL;
			create_info.flags = 0;
			switch (stage_mask)
			{
				case SHADER_STAGE_VERT:
				{
					vk_createShaderReflection(
						(const uint8_t*)pDesc->mVert.pByteCode, (uint32_t)pDesc->mVert.mByteCodeSize, SHADER_STAGE_VERT,
						&stageReflections[counter]);

					create_info.codeSize = pDesc->mVert.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mVert.pByteCode;
					VkResult vk_res = vkCreateShaderModule(pRenderer->pVkDevice, &create_info, NULL, &(modules[counter]));
					ASSERT(VK_SUCCESS == vk_res);
				}
				break;
				case SHADER_STAGE_TESC:
				{
					vk_createShaderReflection(
						(const uint8_t*)pDesc->mHull.pByteCode, (uint32_t)pDesc->mHull.mByteCodeSize, SHADER_STAGE_TESC,
						&stageReflections[counter]);

					create_info.codeSize = pDesc->mHull.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mHull.pByteCode;
					VkResult vk_res = vkCreateShaderModule(pRenderer->pVkDevice, &create_info, NULL, &(modules[counter]));
					ASSERT(VK_SUCCESS == vk_res);
				}
				break;
				case SHADER_STAGE_TESE:
				{
					vk_createShaderReflection(
						(const uint8_t*)pDesc->mDomain.pByteCode, (uint32_t)pDesc->mDomain.mByteCodeSize, SHADER_STAGE_TESE,
						&stageReflections[counter]);

					create_info.codeSize = pDesc->mDomain.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mDomain.pByteCode;
					VkResult vk_res = vkCreateShaderModule(pRenderer->pVkDevice, &create_info, NULL, &(modules[counter]));
					ASSERT(VK_SUCCESS == vk_res);
				}
				break;
				case SHADER_STAGE_GEOM:
				{
					vk_createShaderReflection(
						(const uint8_t*)pDesc->mGeom.pByteCode, (uint32_t)pDesc->mGeom.mByteCodeSize, SHADER_STAGE_GEOM,
						&stageReflections[counter]);

					create_info.codeSize = pDesc->mGeom.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mGeom.pByteCode;
					VkResult vk_res = vkCreateShaderModule(pRenderer->pVkDevice, &create_info, NULL, &(modules[counter]));
					ASSERT(VK_SUCCESS == vk_res);
				}
				break;
				case SHADER_STAGE_FRAG:
				{
					vk_createShaderReflection(
						(const uint8_t*)pDesc->mFrag.pByteCode, (uint32_t)pDesc->mFrag.mByteCodeSize, SHADER_STAGE_FRAG,
						&stageReflections[counter]);

					create_info.codeSize = pDesc->mFrag.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mFrag.pByteCode;
					VkResult vk_res = vkCreateShaderModule(pRenderer->pVkDevice, &create_info, NULL, &(modules[counter]));
					ASSERT(VK_SUCCESS == vk_res);
				}
				break;
				case SHADER_STAGE_COMP:
				{
					vk_createShaderReflection(
						(const uint8_t*)pDesc->mComp.pByteCode, (uint32_t)pDesc->mComp.mByteCodeSize, SHADER_STAGE_COMP,
						&stageReflections[counter]);

					create_info.codeSize = pDesc->mComp.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mComp.pByteCode;
					VkResult vk_res = vkCreateShaderModule(pRenderer->pVkDevice, &create_info, NULL, &(modules[counter]));
					ASSERT(VK_SUCCESS == vk_res);
				}
				break;
				default: ASSERT(false && "Shader Stage not supported!"); break;
			}

			++counter;
		}
	}

	pShaderProgram->pShaderModules = (VkShaderModule*)conf_calloc(counter, sizeof(VkShaderModule));
	memcpy(pShaderProgram->pShaderModules, modules.data(), counter * sizeof(VkShaderModule));

	createPipelineReflection(stageReflections, counter, &pShaderProgram->mReflection);

	*ppShaderProgram = pShaderProgram;
}

void removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
	ASSERT(pRenderer);

	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

	if (pShaderProgram->mStages & SHADER_STAGE_VERT)
	{
		vkDestroyShaderModule(pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->mReflection.mVertexStageIndex], NULL);
	}

	if (pShaderProgram->mStages & SHADER_STAGE_TESC)
	{
		vkDestroyShaderModule(pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->mReflection.mHullStageIndex], NULL);
	}

	if (pShaderProgram->mStages & SHADER_STAGE_TESE)
	{
		vkDestroyShaderModule(pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->mReflection.mDomainStageIndex], NULL);
	}

	if (pShaderProgram->mStages & SHADER_STAGE_GEOM)
	{
		vkDestroyShaderModule(pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->mReflection.mGeometryStageIndex], NULL);
	}

	if (pShaderProgram->mStages & SHADER_STAGE_FRAG)
	{
		vkDestroyShaderModule(pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->mReflection.mPixelStageIndex], NULL);
	}

	if (pShaderProgram->mStages & SHADER_STAGE_COMP)
	{
		vkDestroyShaderModule(pRenderer->pVkDevice, pShaderProgram->pShaderModules[0], NULL);
	}

	destroyPipelineReflection(&pShaderProgram->mReflection);

	SAFE_FREE(pShaderProgram->pShaderModules);
	SAFE_FREE(pShaderProgram);
}
/************************************************************************/
// Root Signature Functions
/************************************************************************/
typedef struct UpdateFrequencyLayoutInfo
{
	/// Array of all bindings in the descriptor set
	tinystl::vector<VkDescriptorSetLayoutBinding> mBindings;
	/// Array of all descriptors in this descriptor set
	tinystl::vector<DescriptorInfo*> mDescriptors;
	/// Array of all descriptors marked as dynamic in this descriptor set (applicable to DESCRIPTOR_TYPE_UNIFORM_BUFFER)
	tinystl::vector<DescriptorInfo*> mDynamicDescriptors;
	/// Hash map to get index of the descriptor in the root signature
	tinystl::unordered_map<DescriptorInfo*, uint32_t> mDescriptorIndexMap;
} UpdateFrequencyLayoutInfo;

void addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
{
	RootSignature* pRootSignature = (RootSignature*)conf_calloc(1, sizeof(*pRootSignature));

	tinystl::vector<UpdateFrequencyLayoutInfo> layouts(DESCRIPTOR_UPDATE_FREQ_COUNT);
	tinystl::vector<DescriptorInfo*>           pushConstantDescriptors;
	tinystl::vector<ShaderResource const*>     shaderResources;

	tinystl::unordered_map<tinystl::string, Sampler*> staticSamplerMap;
	tinystl::vector<tinystl::string>                  dynamicUniformBuffers;

	for (uint32_t i = 0; i < pRootSignatureDesc->mStaticSamplerCount; ++i)
		staticSamplerMap.insert({ pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i] });
	for (uint32_t i = 0; i < pRootSignatureDesc->mDynamicUniformBufferCount; ++i)
		dynamicUniformBuffers.push_back(pRootSignatureDesc->ppDynamicUniformBufferNames[i]);

	conf_placement_new<tinystl::unordered_map<uint32_t, uint32_t> >(&pRootSignature->pDescriptorNameToIndexMap);

	// Collect all unique shader resources in the given shaders
	// Resources are parsed by name (two resources named "XYZ" in two shaders will be considered the same resource)
	for (uint32_t sh = 0; sh < pRootSignatureDesc->mShaderCount; ++sh)
	{
		PipelineReflection const* pReflection = &pRootSignatureDesc->ppShaders[sh]->mReflection;

		if (pReflection->mShaderStages & SHADER_STAGE_COMP)
			pRootSignature->mPipelineType = PIPELINE_TYPE_COMPUTE;
		else
			pRootSignature->mPipelineType = PIPELINE_TYPE_GRAPHICS;

		for (uint32_t i = 0; i < pReflection->mShaderResourceCount; ++i)
		{
			ShaderResource const* pRes = &pReflection->pShaderResources[i];
			uint32_t              setIndex = pRes->set;

			if (pRes->type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
				setIndex = 0;

			tinystl::unordered_hash_node<uint32_t, uint32_t>* pNode =
				pRootSignature->pDescriptorNameToIndexMap.find(tinystl::hash(pRes->name)).node;
			if (!pNode)
			{
				pRootSignature->pDescriptorNameToIndexMap.insert({ tinystl::hash(pRes->name), (uint32_t)shaderResources.size() });
				shaderResources.emplace_back(pRes);
			}
			else
			{
				if (shaderResources[pNode->second]->reg != pRes->reg)
				{
					ErrorMsg(
						"\nFailed to create root signature\n"
						"Shared shader resource %s has mismatching binding. All shader resources "
						"shared by multiple shaders specified in addRootSignature "
						"must have the same binding and set",
						pRes->name);
					return;
				}
				if (shaderResources[pNode->second]->set != pRes->set)
				{
					ErrorMsg(
						"\nFailed to create root signature\n"
						"Shared shader resource %s has mismatching set. All shader resources "
						"shared by multiple shaders specified in addRootSignature "
						"must have the same binding and set",
						pRes->name);
					return;
				}
			}
		}
	}

	if ((uint32_t)shaderResources.size())
	{
		pRootSignature->mDescriptorCount = (uint32_t)shaderResources.size();
		pRootSignature->pDescriptors = (DescriptorInfo*)conf_calloc(pRootSignature->mDescriptorCount, sizeof(DescriptorInfo));
	}

	// Fill the descriptor array to be stored in the root signature
	for (uint32_t i = 0; i < (uint32_t)shaderResources.size(); ++i)
	{
		DescriptorInfo*           pDesc = &pRootSignature->pDescriptors[i];
		ShaderResource const*     pRes = shaderResources[i];
		uint32_t                  setIndex = pRes->set;
		DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)setIndex;

		// Copy the binding information generated from the shader reflection into the descriptor
		pDesc->mDesc.reg = pRes->reg;
		pDesc->mDesc.set = pRes->set;
		pDesc->mDesc.size = pRes->size;
		pDesc->mDesc.type = pRes->type;
		pDesc->mDesc.used_stages = pRes->used_stages;
		pDesc->mDesc.name_size = pRes->name_size;
		pDesc->mDesc.name = (const char*)conf_calloc(pDesc->mDesc.name_size + 1, sizeof(char));
		pDesc->mDesc.textureDim = pRes->textureDim;
		memcpy((char*)pDesc->mDesc.name, pRes->name, pRes->name_size);

		// If descriptor is not a root constant create a new layout binding for this descriptor and add it to the binding array
		if (pDesc->mDesc.type != DESCRIPTOR_TYPE_ROOT_CONSTANT)
		{
			VkDescriptorSetLayoutBinding binding = {};
			binding.binding = pDesc->mDesc.reg;
			binding.descriptorCount = pDesc->mDesc.size;

			binding.descriptorType = util_to_vk_descriptor_type(pDesc->mDesc.type);

			// If a user specified a uniform buffer to be used as a dynamic uniform buffer change its type to VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
			// Also log a message for debugging purpose
			if (dynamicUniformBuffers.find(pDesc->mDesc.name) != dynamicUniformBuffers.end())
			{
				if (pDesc->mDesc.size == 1)
				{
					LOGINFOF("Descriptor (%s) : User specified VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC", pDesc->mDesc.name);
					binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				}
				else
				{
					LOGWARNINGF("Descriptor (%s) : Cannot use VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC for arrays", pDesc->mDesc.name);
				}
			}

			binding.stageFlags = util_to_vk_shader_stage_flags(pDesc->mDesc.used_stages);

			// Store the vulkan related info in the descriptor to avoid constantly calling the util_to_vk mapping functions
			pDesc->mVkType = binding.descriptorType;
			pDesc->mVkStages = binding.stageFlags;
			pDesc->mUpdateFrquency = updateFreq;

			if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
			{
				layouts[setIndex].mDynamicDescriptors.emplace_back(pDesc);
			}

			// Find if the given descriptor is a static sampler
			const tinystl::unordered_hash_node<tinystl::string, Sampler*>* pNode = staticSamplerMap.find(pDesc->mDesc.name).node;
			if (pNode)
			{
				LOGINFOF("Descriptor (%s) : User specified Static Sampler", pDesc->mDesc.name);

				// Set the index to an invalid value so we can use this later for error checking if user tries to update a static sampler
				pDesc->mIndexInParent = -1;
				binding.pImmutableSamplers = &pNode->second->pVkSampler;
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
			pDesc->mDesc.set = 0;
			pDesc->mVkStages = util_to_vk_shader_stage_flags(pDesc->mDesc.used_stages);
			setIndex = 0;
			pushConstantDescriptors.emplace_back(pDesc);
		}

		layouts[setIndex].mDescriptorIndexMap[pDesc] = i;
	}

	pRootSignature->mVkPushConstantCount = (uint32_t)pushConstantDescriptors.size();
	if (pRootSignature->mVkPushConstantCount)
		pRootSignature->pVkPushConstantRanges =
			(VkPushConstantRange*)conf_calloc(pRootSignature->mVkPushConstantCount, sizeof(*pRootSignature->pVkPushConstantRanges));

	// Create push constant ranges
	for (uint32_t i = 0; i < pRootSignature->mVkPushConstantCount; ++i)
	{
		VkPushConstantRange* pConst = &pRootSignature->pVkPushConstantRanges[i];
		DescriptorInfo*      pDesc = pushConstantDescriptors[i];
		pDesc->mIndexInParent = i;
		pConst->offset = 0;
		pConst->size = pDesc->mDesc.size;
		pConst->stageFlags = util_to_vk_shader_stage_flags(pDesc->mDesc.used_stages);
	}

	// Create descriptor layouts
	// Put most frequently changed params first
	for (uint32_t i = (uint32_t)layouts.size(); i-- > 0U;)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[i];

		if (layouts[i].mBindings.size())
		{
			// sort table by type (CBV/SRV/UAV) by register
			layout.mBindings.sort([](const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs) {
				return (int)(lhs.binding - rhs.binding);
			});
			layout.mBindings.sort([](const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs) {
				return (int)(lhs.descriptorType - rhs.descriptorType);
			});
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.pNext = NULL;
		layoutInfo.bindingCount = (uint32_t)layout.mBindings.size();
		layoutInfo.pBindings = layout.mBindings.data();
		layoutInfo.flags = 0;

		vkCreateDescriptorSetLayout(pRenderer->pVkDevice, &layoutInfo, NULL, &pRootSignature->mVkDescriptorSetLayouts[i]);

		if (!layouts[i].mBindings.size())
			continue;

		pRootSignature->mVkDescriptorCounts[i] = (uint32_t)layout.mDescriptors.size();
		pRootSignature->pVkDescriptorIndices[i] = (uint32_t*)conf_calloc(layout.mDescriptors.size(), sizeof(uint32_t));

		// Loop through descriptors belonging to this update frequency and increment the cumulative descriptor count
		for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mDescriptors.size(); ++descIndex)
		{
			DescriptorInfo* pDesc = layout.mDescriptors[descIndex];
			pDesc->mIndexInParent = descIndex;
			pDesc->mHandleIndex = pRootSignature->mVkCumulativeDescriptorCounts[i];
			pRootSignature->pVkDescriptorIndices[i][descIndex] = layout.mDescriptorIndexMap[pDesc];
			pRootSignature->mVkCumulativeDescriptorCounts[i] += pDesc->mDesc.size;
		}

		layout.mDynamicDescriptors.sort(
			[](DescriptorInfo* const& lhs, DescriptorInfo* const& rhs) { return (int)(lhs->mDesc.reg - rhs->mDesc.reg); });

		pRootSignature->mVkDynamicDescriptorCounts[i] = (uint32_t)layout.mDynamicDescriptors.size();
		for (uint32_t descIndex = 0; descIndex < pRootSignature->mVkDynamicDescriptorCounts[i]; ++descIndex)
		{
			DescriptorInfo* pDesc = layout.mDynamicDescriptors[descIndex];
			pDesc->mDynamicUniformIndex = descIndex;
		}
	}
	/************************************************************************/
	// Pipeline layout
	/************************************************************************/
	tinystl::vector<VkDescriptorSetLayout> descriptorSetLayouts(DESCRIPTOR_UPDATE_FREQ_COUNT);
	tinystl::vector<VkPushConstantRange>   pushConstants(pRootSignature->mVkPushConstantCount);
	for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
		descriptorSetLayouts[i] = pRootSignature->mVkDescriptorSetLayouts[i];
	for (uint32_t i = 0; i < pRootSignature->mVkPushConstantCount; ++i)
		pushConstants[i] = pRootSignature->pVkPushConstantRanges[i];

	DECLARE_ZERO(VkPipelineLayoutCreateInfo, add_info);
	add_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
	add_info.pSetLayouts = descriptorSetLayouts.data();
	add_info.pushConstantRangeCount = pRootSignature->mVkPushConstantCount;
	add_info.pPushConstantRanges = pushConstants.data();
	VkResult vk_res = vkCreatePipelineLayout(pRenderer->pVkDevice, &add_info, NULL, &(pRootSignature->pPipelineLayout));
	ASSERT(VK_SUCCESS == vk_res);
	/************************************************************************/
	/************************************************************************/

	conf_placement_new<RootSignature::ThreadLocalDescriptorManager>(&pRootSignature->pDescriptorManagerMap);
	// Create descriptor manager for this thread
	DescriptorManager* pManager = NULL;
	add_descriptor_manager(pRenderer, pRootSignature, &pManager);
	pRootSignature->pDescriptorManagerMap.insert({ Thread::GetCurrentThreadID(), pManager });

	*ppRootSignature = pRootSignature;
}

void removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
	for (tinystl::unordered_hash_node<ThreadID, DescriptorManager*>& it : pRootSignature->pDescriptorManagerMap)
	{
		remove_descriptor_manager(pRenderer, pRootSignature, it.second);
	}

	for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
	{
		vkDestroyDescriptorSetLayout(pRenderer->pVkDevice, pRootSignature->mVkDescriptorSetLayouts[i], NULL);

		SAFE_FREE(pRootSignature->pVkDescriptorIndices[i]);
	}

	for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
	{
		SAFE_FREE(pRootSignature->pDescriptors[i].mDesc.name);
	}

	pRootSignature->pDescriptorManagerMap.~unordered_map();
	// Need delete since the destructor frees allocated memory
	pRootSignature->pDescriptorNameToIndexMap.~unordered_map();

	SAFE_FREE(pRootSignature->pDescriptors);
	SAFE_FREE(pRootSignature->pVkPushConstantRanges);

	vkDestroyPipelineLayout(pRenderer->pVkDevice, pRootSignature->pPipelineLayout, NULL);

	SAFE_FREE(pRootSignature);
}

#ifdef FORGE_JHABLE_EDITS_V01
static bool convertVertInputToSemantic(int& semanticType, int& semanticIndex, const char attrName[], int attrLen)
{
	semanticType = -1;
	semanticIndex = -1;

	int baseOffset = 0;

	{
		// glslangvalidator and glslc prepend vertex attributes with input_
		int cmp0 = strncmp(attrName, "input_", 6);
		if (cmp0 == 0)
		{
			baseOffset = 6;
		}

		// special case for the shaders in TextShaders.h which prepend with in_var_
		int cmp1 = strncmp(attrName, "in_var_", 7);
		if (cmp1 == 0)
		{
			baseOffset = 7;
		}
	}

	// figure out the semantic index, just go backwards until we find a non-number
	int startIndex = attrLen;
	while (startIndex - 1 >= baseOffset && '0' <= attrName[startIndex - 1] && attrName[startIndex - 1] < '9')
	{
		startIndex--;
	}

	if (startIndex != attrLen)
	{
		int numDigits = attrLen - startIndex;
		int sum = 0;
		for (int digitIter = 0; digitIter < numDigits; digitIter++)
		{
			sum *= 10;
			sum += int(attrName[startIndex + digitIter] - '0');
		}
		semanticIndex = sum;
	}
	else
	{
		// if no digit, then semantic is 0
		semanticIndex = 0;
	}

	int semanticNum = sizeof(g_hackSemanticList) / sizeof(g_hackSemanticList[0]);

	for (int i = 0; i < semanticNum; i++)
	{
		int len = strlen(g_hackSemanticList[i]);

		if (attrLen >= baseOffset + len)
		{
			bool ok = true;
			for (int j = 0; j < len; j++)
			{
				int lhs = tolower(g_hackSemanticList[i][j]);
				int rhs = tolower(attrName[baseOffset + j]);
				if (lhs != rhs)
				{
					ok = false;
					break;
				}
			}

			if (ok)
			{
				semanticType = i;
				break;
			}
		}
	}

	return semanticType >= 0;
}

#endif

/************************************************************************/
// Pipeline State Functions
/************************************************************************/
void addPipeline(Renderer* pRenderer, const GraphicsPipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);

	Pipeline* pPipeline = (Pipeline*)conf_calloc(1, sizeof(*pPipeline));
	ASSERT(pPipeline);

	const Shader*       pShaderProgram = pDesc->pShaderProgram;
	const VertexLayout* pVertexLayout = pDesc->pVertexLayout;

	memcpy(&(pPipeline->mGraphics), pDesc, sizeof(*pDesc));
	pPipeline->mType = PIPELINE_TYPE_GRAPHICS;

	// Create tempporary renderpass for pipeline creation
	RenderPassDesc renderPassDesc = { 0 };
	RenderPass*    pRenderPass = NULL;
	renderPassDesc.mRenderTargetCount = pDesc->mRenderTargetCount;
	renderPassDesc.pColorFormats = pDesc->pColorFormats;
	renderPassDesc.mSampleCount = pDesc->mSampleCount;
	renderPassDesc.pSrgbValues = pDesc->pSrgbValues;
	renderPassDesc.mDepthStencilFormat = pDesc->mDepthStencilFormat;
	add_render_pass(pRenderer, &renderPassDesc, &pRenderPass);

	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	for (uint32_t i = 0; i < pShaderProgram->mReflection.mStageReflectionCount; ++i)
		ASSERT(VK_NULL_HANDLE != pShaderProgram->pShaderModules[i]);

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
							pShaderProgram->mReflection.mStageReflections[pShaderProgram->mReflection.mVertexStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_VERTEX_BIT;
						stages[stage_count].module = pShaderProgram->pShaderModules[pShaderProgram->mReflection.mVertexStageIndex];
					}
					break;
					case SHADER_STAGE_TESC:
					{
						stages[stage_count].pName =
							pShaderProgram->mReflection.mStageReflections[pShaderProgram->mReflection.mHullStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
						stages[stage_count].module = pShaderProgram->pShaderModules[pShaderProgram->mReflection.mHullStageIndex];
					}
					break;
					case SHADER_STAGE_TESE:
					{
						stages[stage_count].pName =
							pShaderProgram->mReflection.mStageReflections[pShaderProgram->mReflection.mDomainStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
						stages[stage_count].module = pShaderProgram->pShaderModules[pShaderProgram->mReflection.mDomainStageIndex];
					}
					break;
					case SHADER_STAGE_GEOM:
					{
						stages[stage_count].pName =
							pShaderProgram->mReflection.mStageReflections[pShaderProgram->mReflection.mGeometryStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
						stages[stage_count].module = pShaderProgram->pShaderModules[pShaderProgram->mReflection.mGeometryStageIndex];
					}
					break;
					case SHADER_STAGE_FRAG:
					{
						stages[stage_count].pName =
							pShaderProgram->mReflection.mStageReflections[pShaderProgram->mReflection.mPixelStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
						stages[stage_count].module = pShaderProgram->pShaderModules[pShaderProgram->mReflection.mPixelStageIndex];
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

#ifdef FORGE_JHABLE_EDITS_V01
			ASSERT(pShaderProgram->mReflection.mVertexStageIndex >= 0);
			ASSERT(attrib_count <= MAX_VERTEX_BINDINGS);

			int vertAttrSemanticType[MAX_VERTEX_BINDINGS];
			int vertAttrSemanticIndex[MAX_VERTEX_BINDINGS];
			int vertLocation[MAX_VERTEX_BINDINGS];
			for (int i = 0; i < MAX_VERTEX_BINDINGS; i++)
			{
				vertAttrSemanticType[i] = -1;
				vertAttrSemanticIndex[i] = -1;
				vertLocation[i] = -1;
			}

			const ShaderReflection* pVertReflection =
				&pShaderProgram->mReflection.mStageReflections[pShaderProgram->mReflection.mVertexStageIndex];
			int numVertexInputs = 0;
			if (pVertReflection != nullptr)
			{
				numVertexInputs = pVertReflection->mVertexInputsCount;
				for (uint32_t i = 0; i < numVertexInputs; ++i)
				{
					int  semanticType = -1;
					int  semanticIndex = -1;
					bool isFound = convertVertInputToSemantic(
						semanticType, semanticIndex, pVertReflection->pVertexInputs[i].name, pVertReflection->pVertexInputs[i].name_size);
					if (isFound)
					{
						vertAttrSemanticType[i] = semanticType;
						vertAttrSemanticIndex[i] = semanticIndex;
					}
					else
					{
						printf("Faild to find vertex input: %s\n", pVertReflection->pVertexInputs[i].name);
						int temp = 0;
						temp++;
					}
				}
			}

			// for each attrib, find the matching input in the reflection
			for (uint32_t i = 0; i < attrib_count; ++i)
			{
				const VertexAttrib* attrib = &(pVertexLayout->mAttribs[i]);
				int                 foundIndex = -1;
				int                 expectedType = attrib->mSemanticType;
				int                 expectedIndex = attrib->mSemanticIndex;

				vertLocation[i] = -1;
				for (int j = 0; j < numVertexInputs; j++)
				{
					if (vertAttrSemanticType[j] == expectedType && vertAttrSemanticIndex[j] == expectedIndex)
					{
						vertLocation[i] = j;
					}
				}
			}
#endif

#ifdef FORGE_JHABLE_EDITS_V01
			// Initial values
			for (uint32_t i = 0; i < attrib_count; ++i)
			{
				const VertexAttrib* attrib = &(pVertexLayout->mAttribs[i]);

				if (binding_value != attrib->mBinding)
				{
					binding_value = attrib->mBinding;
					++input_binding_count;
				}

				input_bindings[input_binding_count - 1].stride += calculateImageFormatStride(attrib->mFormat);

				if (vertLocation[i] >= 0)
				{
					input_bindings[input_binding_count - 1].binding = binding_value;
					if (attrib->mRate == VERTEX_ATTRIB_RATE_INSTANCE)
					{
						input_bindings[input_binding_count - 1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
					}
					else
					{
						input_bindings[input_binding_count - 1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
					}

					input_attributes[input_attribute_count].location = vertLocation[i];    //attrib->mLocation;
					input_attributes[input_attribute_count].binding = attrib->mBinding;
					input_attributes[input_attribute_count].format = util_to_vk_image_format(attrib->mFormat, false);
					input_attributes[input_attribute_count].offset = attrib->mOffset;
					++input_attribute_count;
				}
			}
#else
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
				input_bindings[input_binding_count - 1].stride += calculateImageFormatStride(attrib->mFormat);

				input_attributes[input_attribute_count].location = attrib->mLocation;
				input_attributes[input_attribute_count].binding = attrib->mBinding;
				input_attributes[input_attribute_count].format = util_to_vk_image_format(attrib->mFormat, false);
				input_attributes[input_attribute_count].offset = attrib->mOffset;
				++input_attribute_count;
			}
#endif
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
				pShaderProgram->mReflection.mStageReflections[pShaderProgram->mReflection.mHullStageIndex].mNumControlPoint;
		}

		DECLARE_ZERO(VkPipelineViewportStateCreateInfo, vs);
		vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		vs.pNext = NULL;
		vs.flags = 0;
		// we are using dynimic viewports but we must set the count to 1
		vs.viewportCount = 1;
		vs.pViewports = NULL;
		vs.scissorCount = 1;
		vs.pScissors = NULL;

		BlendState*      pBlendState = pDesc->pBlendState != NULL ? pDesc->pBlendState : pRenderer->pDefaultBlendState;
		DepthState*      pDepthState = pDesc->pDepthState != NULL ? pDesc->pDepthState : pRenderer->pDefaultDepthState;
		RasterizerState* pRasterizerState = pDesc->pRasterizerState != NULL ? pDesc->pRasterizerState : pRenderer->pDefaultRasterizerState;

		DECLARE_ZERO(VkPipelineRasterizationStateCreateInfo, rs);
		rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rs.pNext = NULL;
		rs.flags = 0;
		rs.depthClampEnable = pRasterizerState->DepthClampEnable;
		rs.rasterizerDiscardEnable = VK_FALSE;
		rs.polygonMode = pRasterizerState->PolygonMode;
		rs.cullMode = pRasterizerState->CullMode;
		rs.frontFace = pRasterizerState->FrontFace;
		rs.depthBiasEnable = pRasterizerState->DepthBiasEnable;
		rs.depthBiasConstantFactor = pRasterizerState->DepthBiasConstantFactor;
		rs.depthBiasClamp = pRasterizerState->DepthBiasClamp;
		rs.depthBiasSlopeFactor = pRasterizerState->DepthBiasSlopeFactor;
		rs.lineWidth = pRasterizerState->LineWidth;

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

		/// TODO: Dont create depth state if no depth stencil bound
		DECLARE_ZERO(VkPipelineDepthStencilStateCreateInfo, ds);
		ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		ds.pNext = NULL;
		ds.flags = 0;
		ds.depthTestEnable = pDepthState->DepthTestEnable;
		ds.depthWriteEnable = pDepthState->DepthWriteEnable;
		ds.depthCompareOp = pDepthState->DepthCompareOp;
		ds.depthBoundsTestEnable = pDepthState->DepthBoundsTestEnable;
		ds.stencilTestEnable = pDepthState->StencilTestEnable;
		ds.front = pDepthState->Front;
		ds.back = pDepthState->Back;
		ds.minDepthBounds = pDepthState->MinDepthBounds;
		ds.maxDepthBounds = pDepthState->MaxDepthBounds;

		DECLARE_ZERO(VkPipelineColorBlendStateCreateInfo, cb);
		cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		cb.pNext = NULL;
		cb.flags = 0;
		cb.logicOpEnable = pBlendState->LogicOpEnable;
		cb.logicOp = pBlendState->LogicOp;
		cb.attachmentCount = pDesc->mRenderTargetCount;
		cb.pAttachments = pBlendState->RTBlendStates;
		cb.blendConstants[0] = 0.0f;
		cb.blendConstants[1] = 0.0f;
		cb.blendConstants[2] = 0.0f;
		cb.blendConstants[3] = 0.0f;

		DECLARE_ZERO(VkDynamicState, dyn_states[5]);
		dyn_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dyn_states[1] = VK_DYNAMIC_STATE_SCISSOR;
		dyn_states[2] = VK_DYNAMIC_STATE_DEPTH_BIAS;
		dyn_states[3] = VK_DYNAMIC_STATE_BLEND_CONSTANTS;
		dyn_states[4] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;
		DECLARE_ZERO(VkPipelineDynamicStateCreateInfo, dy);
		dy.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dy.pNext = NULL;
		dy.flags = 0;
		dy.dynamicStateCount = 5;
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
		add_info.layout = pDesc->pRootSignature->pPipelineLayout;
		add_info.renderPass = pRenderPass->pRenderPass;
		add_info.subpass = 0;
		add_info.basePipelineHandle = VK_NULL_HANDLE;
		add_info.basePipelineIndex = -1;
		VkResult vk_res = vkCreateGraphicsPipelines(pRenderer->pVkDevice, VK_NULL_HANDLE, 1, &add_info, NULL, &(pPipeline->pVkPipeline));
		ASSERT(VK_SUCCESS == vk_res);

		remove_render_pass(pRenderer, pRenderPass);
	}

	*ppPipeline = pPipeline;
}

void addComputePipeline(Renderer* pRenderer, const ComputePipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);
	ASSERT(pRenderer->pVkDevice != VK_NULL_HANDLE);
	ASSERT(pDesc->pShaderProgram->pShaderModules[0] != VK_NULL_HANDLE);

	Pipeline* pPipeline = (Pipeline*)conf_calloc(1, sizeof(*pPipeline));
	ASSERT(pPipeline);

	memcpy(&(pPipeline->mCompute), pDesc, sizeof(*pDesc));
	pPipeline->mType = PIPELINE_TYPE_COMPUTE;

	// Pipeline
	{
		DECLARE_ZERO(VkPipelineShaderStageCreateInfo, stage);
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage.pNext = NULL;
		stage.flags = 0;
		stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		stage.module = pDesc->pShaderProgram->pShaderModules[0];
		stage.pName = pDesc->pShaderProgram->mReflection.mStageReflections[0].pEntryPoint;
		stage.pSpecializationInfo = NULL;

		DECLARE_ZERO(VkComputePipelineCreateInfo, create_info);
		create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		create_info.pNext = NULL;
		create_info.flags = 0;
		create_info.stage = stage;
		create_info.layout = pDesc->pRootSignature->pPipelineLayout;
		create_info.basePipelineHandle = 0;
		create_info.basePipelineIndex = 0;
		VkResult vk_res = vkCreateComputePipelines(pRenderer->pVkDevice, VK_NULL_HANDLE, 1, &create_info, NULL, &(pPipeline->pVkPipeline));
		ASSERT(VK_SUCCESS == vk_res);
	}

	*ppPipeline = pPipeline;
}

void removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pPipeline);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pPipeline->pVkPipeline);

	vkDestroyPipeline(pRenderer->pVkDevice, pPipeline->pVkPipeline, NULL);

	SAFE_FREE(pPipeline);
}

void addBlendState(Renderer* pRenderer, const BlendStateDesc* pDesc, BlendState** ppBlendState)
{
	int blendDescIndex = 0;
#ifdef _DEBUG

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

	BlendState blendState = {};

	memset(blendState.RTBlendStates, 0, sizeof(blendState.RTBlendStates));
	for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
	{
		if (pDesc->mRenderTargetMask & (1 << i))
		{
			VkBool32 blendEnable =
				(gVkBlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]] != VK_BLEND_FACTOR_ONE ||
				 gVkBlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]] != VK_BLEND_FACTOR_ZERO ||
				 gVkBlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]] != VK_BLEND_FACTOR_ONE ||
				 gVkBlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]] != VK_BLEND_FACTOR_ZERO);

			blendState.RTBlendStates[i].blendEnable = blendEnable;
			blendState.RTBlendStates[i].colorWriteMask = pDesc->mMasks[blendDescIndex];
			blendState.RTBlendStates[i].srcColorBlendFactor = gVkBlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]];
			blendState.RTBlendStates[i].dstColorBlendFactor = gVkBlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]];
			blendState.RTBlendStates[i].colorBlendOp = gVkBlendOpTranslator[pDesc->mBlendModes[blendDescIndex]];
			blendState.RTBlendStates[i].srcAlphaBlendFactor = gVkBlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]];
			blendState.RTBlendStates[i].dstAlphaBlendFactor = gVkBlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]];
			blendState.RTBlendStates[i].alphaBlendOp = gVkBlendOpTranslator[pDesc->mBlendAlphaModes[blendDescIndex]];
		}

		if (pDesc->mIndependentBlend)
			++blendDescIndex;
	}

	blendState.LogicOpEnable = false;
	blendState.LogicOp = VK_LOGIC_OP_CLEAR;

	*ppBlendState = (BlendState*)conf_malloc(sizeof(blendState));
	memcpy(*ppBlendState, &blendState, sizeof(blendState));
}

void removeBlendState(BlendState* pBlendState) { SAFE_FREE(pBlendState); }

void addDepthState(Renderer* pRenderer, const DepthStateDesc* pDesc, DepthState** ppDepthState)
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

	DepthState depthState = {};
	depthState.DepthTestEnable = pDesc->mDepthTest;
	depthState.DepthWriteEnable = pDesc->mDepthWrite;
	depthState.DepthCompareOp = gVkComparisonFuncTranslator[pDesc->mDepthFunc];
	depthState.StencilTestEnable = pDesc->mStencilTest;

	depthState.Front.failOp = gVkStencilOpTranslator[pDesc->mStencilFrontFail];
	depthState.Front.passOp = gVkStencilOpTranslator[pDesc->mStencilFrontPass];
	depthState.Front.depthFailOp = gVkStencilOpTranslator[pDesc->mDepthFrontFail];
	depthState.Front.compareOp = VkCompareOp(pDesc->mStencilFrontFunc);
	depthState.Front.compareMask = pDesc->mStencilReadMask;
	depthState.Front.writeMask = pDesc->mStencilWriteMask;
	depthState.Front.reference = 0;

	depthState.Back.failOp = gVkStencilOpTranslator[pDesc->mStencilBackFail];
	depthState.Back.passOp = gVkStencilOpTranslator[pDesc->mStencilBackPass];
	depthState.Back.depthFailOp = gVkStencilOpTranslator[pDesc->mDepthBackFail];
	depthState.Back.compareOp = gVkComparisonFuncTranslator[pDesc->mStencilBackFunc];
	depthState.Back.compareMask = pDesc->mStencilReadMask;
	depthState.Back.writeMask = pDesc->mStencilFrontFunc;
	depthState.Back.reference = 0;

	depthState.DepthBoundsTestEnable = false;
	depthState.MinDepthBounds = 0;
	depthState.MaxDepthBounds = 1;

	*ppDepthState = (DepthState*)conf_malloc(sizeof(depthState));
	memcpy(*ppDepthState, &depthState, sizeof(depthState));
}

void removeDepthState(DepthState* pDepthState) { SAFE_FREE(pDepthState); }

void addRasterizerState(Renderer* pRenderer, const RasterizerStateDesc* pDesc, RasterizerState** ppRasterizerState)
{
	ASSERT(pDesc->mFillMode < FillMode::MAX_FILL_MODES);
	ASSERT(pDesc->mCullMode < CullMode::MAX_CULL_MODES);
	ASSERT(pDesc->mFrontFace == FRONT_FACE_CCW || pDesc->mFrontFace == FRONT_FACE_CW);

	RasterizerState rasterizerState = {};

	rasterizerState.DepthClampEnable = VK_TRUE;
	rasterizerState.PolygonMode = gVkFillModeTranslator[pDesc->mFillMode];
	rasterizerState.CullMode = gVkCullModeTranslator[pDesc->mCullMode];
	rasterizerState.FrontFace = gVkFrontFaceTranslator[pDesc->mFrontFace];
	rasterizerState.DepthBiasEnable = (pDesc->mDepthBias != 0) ? VK_TRUE : VK_FALSE;
	rasterizerState.DepthBiasConstantFactor = float(pDesc->mDepthBias);
	rasterizerState.DepthBiasClamp = 0.f;
	rasterizerState.DepthBiasSlopeFactor = pDesc->mSlopeScaledDepthBias;
	rasterizerState.LineWidth = 1;

	*ppRasterizerState = (RasterizerState*)conf_malloc(sizeof(rasterizerState));
	memcpy(*ppRasterizerState, &rasterizerState, sizeof(rasterizerState));
}

void removeRasterizerState(RasterizerState* pRasterizerState) { SAFE_FREE(pRasterizerState); }
/************************************************************************/
// Command buffer functions
/************************************************************************/
void beginCmd(Cmd* pCmd)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	// reset buffer to conf_free memory
	vkResetCommandBuffer(pCmd->pVkCmdBuf, 0);

	DECLARE_ZERO(VkCommandBufferBeginInfo, begin_info);
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	begin_info.pInheritanceInfo = NULL;

	VkDeviceGroupCommandBufferBeginInfoKHR deviceGroupBeginInfo = { VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO_KHR };
	deviceGroupBeginInfo.pNext = NULL;
	if (pCmd->pRenderer->mSettings.mGpuMode == GPU_MODE_LINKED)
	{
		deviceGroupBeginInfo.deviceMask = (1 << pCmd->mNodeIndex);
		begin_info.pNext = &deviceGroupBeginInfo;
	}

	VkResult vk_res = vkBeginCommandBuffer(pCmd->pVkCmdBuf, &begin_info);
	ASSERT(VK_SUCCESS == vk_res);

	if (pCmd->pDescriptorPool)
		reset_descriptor_heap(pCmd->pRenderer, pCmd->pDescriptorPool);
}

void endCmd(Cmd* pCmd)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	if (pCmd->pVkActiveRenderPass)
	{
		vkCmdEndRenderPass(pCmd->pVkCmdBuf);
	}

	pCmd->pVkActiveRenderPass = VK_NULL_HANDLE;
	pCmd->mBoundDepthStencilFormat = ImageFormat::NONE;
	pCmd->mBoundRenderTargetCount = 0;

	cmdFlushBarriers(pCmd);

	VkResult vk_res = vkEndCommandBuffer(pCmd->pVkCmdBuf);
	ASSERT(VK_SUCCESS == vk_res);
}

void cmdBindRenderTargets(
	Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil,
	const LoadActionsDesc* pLoadActions /* = NULL*/, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice,
	uint32_t depthMipSlice)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	if (pCmd->pVkActiveRenderPass)
	{
		vkCmdEndRenderPass(pCmd->pVkCmdBuf);
		pCmd->pVkActiveRenderPass = VK_NULL_HANDLE;
		pCmd->mBoundDepthStencilFormat = ImageFormat::NONE;
		pCmd->mBoundRenderTargetCount = 0;
	}

	if (!renderTargetCount && !pDepthStencil)
		return;

	uint64_t renderPassHash = 0;
	uint64_t frameBufferHash = 0;

	// Generate hash for render pass and frame buffer
	// NOTE:
	// Render pass does not care about underlying VkImageView. It only cares about the format and sample count of the attachments.
	// We hash those two values to generate render pass hash
	// Frame buffer is the actual array of all the VkImageViews
	// We hash the texture id associated with the render target to generate frame buffer hash
	for (uint32_t i = 0; i < renderTargetCount; ++i)
	{
		uint32_t hashValues[] = {
			(uint32_t)ppRenderTargets[i]->mDesc.mFormat,
			(uint32_t)ppRenderTargets[i]->mDesc.mSampleCount,
			(uint32_t)ppRenderTargets[i]->mDesc.mSrgb,
			pLoadActions ? (uint32_t)pLoadActions->mLoadActionsColor[i] : 0,
		};
		renderPassHash = tinystl::hash_state(hashValues, 4, renderPassHash);
		frameBufferHash = tinystl::hash_state(&ppRenderTargets[i]->pTexture->mTextureId, 1, frameBufferHash);

		pCmd->pBoundColorFormats[i] = ppRenderTargets[i]->mDesc.mFormat;
		pCmd->pBoundSrgbValues[i] = ppRenderTargets[i]->mDesc.mSrgb;
	}
	if (pDepthStencil)
	{
		uint32_t hashValues[] = {
			(uint32_t)pDepthStencil->mDesc.mFormat,
			(uint32_t)pDepthStencil->mDesc.mSampleCount,
			(uint32_t)pDepthStencil->mDesc.mSrgb,
			pLoadActions ? (uint32_t)pLoadActions->mLoadActionDepth : 0,
			pLoadActions ? (uint32_t)pLoadActions->mLoadActionStencil : 0,
		};
		renderPassHash = tinystl::hash_state(hashValues, 5, renderPassHash);
		frameBufferHash = tinystl::hash_state(&pDepthStencil->pTexture->mTextureId, 1, frameBufferHash);

		pCmd->mBoundDepthStencilFormat = pDepthStencil->mDesc.mFormat;
	}
	if (pColorArraySlices)
		frameBufferHash = tinystl::hash_state(pColorArraySlices, renderTargetCount, frameBufferHash);
	if (pColorMipSlices)
		frameBufferHash = tinystl::hash_state(pColorMipSlices, renderTargetCount, frameBufferHash);
	if (depthArraySlice != -1)
		frameBufferHash = tinystl::hash_state(&depthArraySlice, 1, frameBufferHash);
	if (depthMipSlice != -1)
		frameBufferHash = tinystl::hash_state(&depthMipSlice, 1, frameBufferHash);

	SampleCount sampleCount = renderTargetCount ? ppRenderTargets[0]->mDesc.mSampleCount : pDepthStencil->mDesc.mSampleCount;
	pCmd->mBoundSampleCount = sampleCount;
	pCmd->mBoundRenderTargetCount = renderTargetCount;
	pCmd->mRenderPassHash = renderPassHash;

	RenderPassMap&  renderPassMap = get_render_pass_map();
	FrameBufferMap& frameBufferMap = get_frame_buffer_map();

	const RenderPassMapNode*  pNode = renderPassMap.find(renderPassHash).node;
	const FrameBufferMapNode* pFrameBufferNode = frameBufferMap.find(frameBufferHash).node;

	RenderPass*  pRenderPass = NULL;
	FrameBuffer* pFrameBuffer = NULL;

	// If a render pass of this combination already exists just use it or create a new one
	if (pNode)
	{
		pRenderPass = pNode->second;
	}
	else
	{
		ImageFormat::Enum colorFormats[MAX_RENDER_TARGET_ATTACHMENTS] = {};
		bool              srgbValues[MAX_RENDER_TARGET_ATTACHMENTS] = {};
		ImageFormat::Enum depthStencilFormat = ImageFormat::NONE;
		for (uint32_t i = 0; i < renderTargetCount; ++i)
		{
			colorFormats[i] = ppRenderTargets[i]->mDesc.mFormat;
			srgbValues[i] = ppRenderTargets[i]->mDesc.mSrgb;
		}
		if (pDepthStencil)
		{
			depthStencilFormat = pDepthStencil->mDesc.mFormat;
		}

		RenderPassDesc renderPassDesc = {};
		renderPassDesc.mRenderTargetCount = renderTargetCount;
		renderPassDesc.mSampleCount = sampleCount;
		renderPassDesc.pColorFormats = colorFormats;
		renderPassDesc.pSrgbValues = srgbValues;
		renderPassDesc.mDepthStencilFormat = depthStencilFormat;
		renderPassDesc.pLoadActionsColor = pLoadActions ? pLoadActions->mLoadActionsColor : NULL;
		renderPassDesc.mLoadActionDepth = pLoadActions ? pLoadActions->mLoadActionDepth : LOAD_ACTION_DONTCARE;
		renderPassDesc.mLoadActionStencil = pLoadActions ? pLoadActions->mLoadActionStencil : LOAD_ACTION_DONTCARE;
		add_render_pass(pCmd->pRenderer, &renderPassDesc, &pRenderPass);

		// No need of a lock here since this map is per thread
		renderPassMap.insert({ renderPassHash, pRenderPass });
	}

	// If a frame buffer of this combination already exists just use it or create a new one
	if (pFrameBufferNode)
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
		add_framebuffer(pCmd->pRenderer, &desc, &pFrameBuffer);

		// No need of a lock here since this map is per thread
		frameBufferMap.insert({ frameBufferHash, pFrameBuffer });
	}

	DECLARE_ZERO(VkRect2D, render_area);
	render_area.offset.x = 0;
	render_area.offset.y = 0;
	render_area.extent.width = pFrameBuffer->mWidth;
	render_area.extent.height = pFrameBuffer->mHeight;

	pCmd->mBoundWidth = pFrameBuffer->mWidth;
	pCmd->mBoundHeight = pFrameBuffer->mHeight;

	uint32_t     clearValueCount = renderTargetCount;
	VkClearValue clearValues[MAX_RENDER_TARGET_ATTACHMENTS + 1] = {};
	if (pLoadActions)
	{
		for (uint32_t i = 0; i < renderTargetCount; ++i)
		{
			ClearValue clearValue = pLoadActions->mClearColorValues[i];
			clearValues[i].color = { clearValue.r, clearValue.g, clearValue.b, clearValue.a };
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

	vkCmdBeginRenderPass(pCmd->pVkCmdBuf, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
	pCmd->pVkActiveRenderPass = pRenderPass->pRenderPass;
}

void cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	DECLARE_ZERO(VkViewport, viewport);
	viewport.x = x;
	viewport.y = y + height;
	viewport.width = width;
	viewport.height = -height;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;
	vkCmdSetViewport(pCmd->pVkCmdBuf, 0, 1, &viewport);
}

void cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	DECLARE_ZERO(VkRect2D, rect);
	rect.offset.x = x;
	rect.offset.y = y;
	rect.extent.width = width;
	rect.extent.height = height;
	vkCmdSetScissor(pCmd->pVkCmdBuf, 0, 1, &rect);
}

void cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
	ASSERT(pCmd);
	ASSERT(pPipeline);
	ASSERT(pCmd->pVkCmdBuf != VK_NULL_HANDLE);

	VkPipelineBindPoint pipeline_bind_point =
		(pPipeline->mType == PIPELINE_TYPE_COMPUTE) ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

	vkCmdBindPipeline(pCmd->pVkCmdBuf, pipeline_bind_point, pPipeline->pVkPipeline);
}

void cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t offset)
{
	ASSERT(pCmd);
	ASSERT(pBuffer);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	VkIndexType vk_index_type = (INDEX_TYPE_UINT16 == pBuffer->mDesc.mIndexType) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
	vkCmdBindIndexBuffer(pCmd->pVkCmdBuf, pBuffer->pVkBuffer, pBuffer->mPositionInHeap + offset, vk_index_type);
}

void cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, uint64_t* pOffsets)
{
	ASSERT(pCmd);
	ASSERT(0 != bufferCount);
	ASSERT(ppBuffers);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	const uint32_t max_buffers = pCmd->pRenderer->pVkActiveGPUProperties->properties.limits.maxVertexInputBindings;
	uint32_t       capped_buffer_count = bufferCount > max_buffers ? max_buffers : bufferCount;

	// No upper bound for this, so use 64 for now
	ASSERT(capped_buffer_count < 64);

	DECLARE_ZERO(VkBuffer, buffers[64]);
	DECLARE_ZERO(VkDeviceSize, offsets[64]);

	for (uint32_t i = 0; i < capped_buffer_count; ++i)
	{
		buffers[i] = ppBuffers[i]->pVkBuffer;
		offsets[i] = (ppBuffers[i]->mPositionInHeap + (pOffsets ? pOffsets[i] : 0));
	}

	vkCmdBindVertexBuffers(pCmd->pVkCmdBuf, 0, capped_buffer_count, buffers, offsets);
}

void cmdDraw(Cmd* pCmd, uint32_t vertex_count, uint32_t first_vertex)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	vkCmdDraw(pCmd->pVkCmdBuf, vertex_count, 1, first_vertex, 0);
}

void cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	vkCmdDraw(pCmd->pVkCmdBuf, vertexCount, instanceCount, firstVertex, firstInstance);
}

void cmdDrawIndexed(Cmd* pCmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	vkCmdDrawIndexed(pCmd->pVkCmdBuf, index_count, 1, first_index, first_vertex, 0);
}

void cmdDrawIndexedInstanced(
	Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstInstance, uint32_t firstVertex)
{
	ASSERT(pCmd);
	ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

	vkCmdDrawIndexed(pCmd->pVkCmdBuf, indexCount, instanceCount, firstIndex, firstVertex, firstInstance);
}

void cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	ASSERT(pCmd);
	ASSERT(pCmd->pVkCmdBuf != VK_NULL_HANDLE);

	vkCmdDispatch(pCmd->pVkCmdBuf, groupCountX, groupCountY, groupCountZ);
}

void cmdBindDescriptors(Cmd* pCmd, RootSignature* pRootSignature, uint32_t numDescriptors, DescriptorData* pDescParams)
{
	Renderer*          pRenderer = pCmd->pRenderer;
	const uint32_t     setCount = DESCRIPTOR_UPDATE_FREQ_COUNT;
	DescriptorManager* pm = get_descriptor_manager(pRenderer, pRootSignature);

	// Logic to detect beginning of a new frame so we dont run this code everytime user calls cmdBindDescriptors
	for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
	{
		// Reset other data
		pm->mBoundSets[setIndex] = true;
	}

	if (pm->pCurrentCmd != pCmd)
	{
		pm->pCurrentCmd = pCmd;
		pm->mFrameIdx = (pm->mFrameIdx + 1) % MAX_FRAMES_IN_FLIGHT;

		// Vulkan requires to bind all descriptor sets upto the highest set number even if they are empty
		// Example: If shader uses only set 1, we still have to bind an empty set 0
		for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
		{
			if (pm->pEmptyDescriptorSets[setIndex] != VK_NULL_HANDLE)
			{
				vkCmdBindDescriptorSets(
					pCmd->pVkCmdBuf, gPipelineBindPoint[pRootSignature->mPipelineType], pRootSignature->pPipelineLayout, setIndex, 1,
					&pm->pEmptyDescriptorSets[setIndex], 0, NULL);    // Reset other data
			}
			pm->mBoundSets[setIndex] = true;
		}
	}

	// 64 bit hash value for hashing the mTextureId / mBufferId / mSamplerId of the input descriptors
	// This value will be later used as look up to find if a descriptor set with the given hash already exists
	// This way we will call updateDescriptorSet for a particular set of descriptors only once
	// Then we only need to do a look up into the mDescriptorSetMap with pHash[setIndex] as the key and retrieve the DescriptorSet* value
	uint64_t* pHash = (uint64_t*)alloca(setCount * sizeof(uint64_t));
	// Initialize this memory to zero
	memset(pHash, 0, setCount * sizeof(uint64_t));

	// Loop through input params to check for new data
	for (uint32_t i = 0; i < numDescriptors; ++i)
	{
		const DescriptorData* pParam = &pDescParams[i];
		ASSERT(pParam);
		if (!pParam->pName)
		{
			LOGERRORF("Name of Descriptor at index (%u) is NULL", i);
			return;
		}

		uint32_t              descIndex = -1;
		const DescriptorInfo* pDesc = get_descriptor(pRootSignature, pParam->pName, &descIndex);
		if (!pDesc)
			continue;

		// Find the update frequency of the descriptor
		// This is also the set index to be used in vkCmdBindDescriptorSets
		const DescriptorUpdateFrequency setIndex = pDesc->mUpdateFrquency;
		const uint32_t                  arrayCount = max(1U, pParam->mCount);

		// If input param is a root constant no need to do any further checks
		if (pDesc->mDesc.type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
		{
			vkCmdPushConstants(
				pCmd->pVkCmdBuf, pRootSignature->pPipelineLayout, pDesc->mVkStages, 0, pDesc->mDesc.size, pParam->pRootConstant);
			continue;
		}

		// Generate hash of all the resource ids
		if (pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER)
		{
			if (pDesc->mIndexInParent == -1)
			{
				LOGERRORF(
					"Trying to bind a static sampler (%s). All static samplers must be bound in addRootSignature through "
					"RootSignatureDesc::mStaticSamplers",
					pParam->pName);
				continue;
			}
			if (!pParam->ppSamplers)
			{
				LOGERRORF("Sampler descriptor (%s) is NULL", pParam->pName);
				return;
			}
			for (uint32_t i = 0; i < arrayCount; ++i)
			{
				if (!pParam->ppSamplers[i])
				{
					LOGERRORF("Sampler descriptor (%s) at array index (%u) is NULL", pParam->pName, i);
					return;
				}
				pHash[setIndex] = tinystl::hash_state(&pParam->ppSamplers[i]->mSamplerId, 1, pHash[setIndex]);
				pm->pUpdateData[setIndex][pDesc->mHandleIndex + i].mImageInfo = pParam->ppSamplers[i]->mVkSamplerView;
			}
		}
		else if (pDesc->mDesc.type == DESCRIPTOR_TYPE_TEXTURE)
		{
			if (!pParam->ppTextures)
			{
				LOGERRORF("Texture descriptor (%s) is NULL", pParam->pName);
				return;
			}

			for (uint32_t i = 0; i < arrayCount; ++i)
			{
				if (!pParam->ppTextures[i])
				{
					LOGERRORF("Texture descriptor (%s) at array index (%u) is NULL", pParam->pName, i);
					return;
				}

				pHash[setIndex] = tinystl::hash_state(&pParam->ppTextures[i]->mTextureId, 1, pHash[setIndex]);

				// Store the new descriptor so we can use it in vkUpdateDescriptorSet later
				pm->pUpdateData[setIndex][pDesc->mHandleIndex + i].mImageInfo.imageView = pParam->ppTextures[i]->pVkSRVDescriptor;
				// SRVs need to be in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				pm->pUpdateData[setIndex][pDesc->mHandleIndex + i].mImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
		}
		else if (pDesc->mDesc.type == DESCRIPTOR_TYPE_RW_TEXTURE)
		{
			if (!pParam->ppTextures)
			{
				LOGERRORF("RW Texture descriptor (%s) is NULL", pParam->pName);
				return;
			}

			if (pParam->mUAVMipSlice)
				pHash[setIndex] = tinystl::hash_state(&pParam->mUAVMipSlice, 1, pHash[setIndex]);

			for (uint32_t i = 0; i < arrayCount; ++i)
			{
				if (!pParam->ppTextures[i])
				{
					LOGERRORF("RW Texture descriptor (%s) at array index (%u) is NULL", pParam->pName, i);
					return;
				}

				pHash[setIndex] = tinystl::hash_state(&pParam->ppTextures[i]->mTextureId, 1, pHash[setIndex]);

				// Store the new descriptor so we can use it in vkUpdateDescriptorSet later
				pm->pUpdateData[setIndex][pDesc->mHandleIndex + i].mImageInfo.imageView =
					pParam->ppTextures[i]->pVkUAVDescriptors[pParam->mUAVMipSlice];
				// UAVs need to be in VK_IMAGE_LAYOUT_GENERAL
				pm->pUpdateData[setIndex][pDesc->mHandleIndex + i].mImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			}
		}
		else
		{
			if (!pParam->ppBuffers)
			{
				LOGERRORF("Buffer descriptor (%s) is NULL", pParam->pName);
				return;
			}
			for (uint32_t i = 0; i < arrayCount; ++i)
			{
				if (!pParam->ppBuffers[i])
				{
					LOGERRORF("Buffer descriptor (%s) at array index (%u) is NULL", pParam->pName, i);
					return;
				}
				pHash[setIndex] = tinystl::hash_state(&pParam->ppBuffers[i]->mBufferId, 1, pHash[setIndex]);

				if (pDesc->mVkType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
					pm->pUpdateData[setIndex][pDesc->mHandleIndex + i].mBuferView = pParam->ppBuffers[i]->pVkUniformTexelView;
				else if (pDesc->mVkType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
					pm->pUpdateData[setIndex][pDesc->mHandleIndex + i].mBuferView = pParam->ppBuffers[i]->pVkStorageTexelView;
				else
				{
					// Store the new descriptor so we can use it in vkUpdateDescriptorSet later
					pm->pUpdateData[setIndex][pDesc->mHandleIndex + i].mBufferInfo = pParam->ppBuffers[i]->mVkBufferInfo;

					// Only store the offset provided in pParam if the descriptor is not dynamic
					// For dynamic descriptors the offsets are bound in vkCmdBindDescriptorSets
					if (pDesc->mVkType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
					{
						if (pParam->pOffsets)
							pm->pUpdateData[setIndex][pDesc->mHandleIndex + i].mBufferInfo.offset = pParam->pOffsets[i];
						if (pParam->pSizes)
							pm->pUpdateData[setIndex][pDesc->mHandleIndex + i].mBufferInfo.range = pParam->pSizes[i];
					}
				}
			}

			if (pDesc->mDesc.type == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			{
				// If descriptor is of type uniform buffer dynamic, we dont need to hash the offset
				// Dynamic uniform buffer descriptors using the same VkBuffer object can be bound at different offsets without the need for vkUpdateDescriptorSets
				if (pDesc->mVkType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
				{
					// Dynamic uniform buffers cannot form arrays so jut use element 0
					pm->pDynamicOffsets[setIndex][pDesc->mDynamicUniformIndex] = (uint32_t)pParam->pOffsets[0];
				}
				// If descriptor is not of type uniform buffer dynamic, hash the offset value
				// Non dynamic uniform buffer descriptors using the same VkBuffer object but different offset values are considered as different descriptors
				else
				{
					if (pParam->pOffsets)
						pHash[setIndex] = tinystl::hash_state(&pParam->pOffsets[i], 1, pHash[setIndex]);
					if (pParam->pSizes)
						pHash[setIndex] = tinystl::hash_state(&pParam->pSizes[i], 1, pHash[setIndex]);
				}
			}
		}

		// Unbind current descriptor set so we can bind a new one
		pm->mBoundSets[setIndex] = false;
	}

	for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
	{
		uint32_t descCount = pRootSignature->mVkDescriptorCounts[setIndex];
		uint32_t rootDescCount = pRootSignature->mVkDynamicDescriptorCounts[setIndex];

		if (descCount && !pm->mBoundSets[setIndex])
		{
			VkDescriptorSet pDescriptorSet = VK_NULL_HANDLE;
			// Search for the generated hash of descriptors in the descriptor set map
			// If the hash already exists, it means we have already created a descriptor set with the input descriptors
			// Now we just bind that descriptor set and no other op is required
			if (setIndex == DESCRIPTOR_UPDATE_FREQ_NONE)
			{
				ConstDescriptorSetMapIterator it = pm->mStaticDescriptorSetMap[pm->mFrameIdx].find(pHash[setIndex]);
				if (it.node)
				{
					pDescriptorSet = it.node->second;
				}
				// If the given hash does not exist, we create a new descriptor set and insert it into the descriptor set map
				else
				{
					VkDescriptorSet* pSets[] = { &pDescriptorSet };
					consume_descriptor_sets(
						pRenderer, &pRootSignature->mVkDescriptorSetLayouts[setIndex], pSets, 1, pRenderer->pDescriptorPool);
					vkUpdateDescriptorSetWithTemplateKHR(
						pRenderer->pVkDevice, pDescriptorSet, pm->mUpdateTemplates[setIndex], pm->pUpdateData[setIndex]);
					++pm->mDescriptorSetCount[pm->mFrameIdx][setIndex];
					pm->mStaticDescriptorSetMap[pm->mFrameIdx].insert({ pHash[setIndex], pDescriptorSet });
				}
			}
			// Dynamic descriptors
			else
			{
				if (!pCmd->pDescriptorPool)
					add_descriptor_heap(
						pRenderer, gDefaultDynamicDescriptorSets, 0, gDynamicDescriptorHeapPoolSizes, VK_DESCRIPTOR_TYPE_RANGE_SIZE,
						&pCmd->pDescriptorPool);

				VkDescriptorSet* pSets[] = { &pDescriptorSet };
				consume_descriptor_sets_lock_free(
					pRenderer, &pRootSignature->mVkDescriptorSetLayouts[setIndex], pSets, 1, pCmd->pDescriptorPool);
				vkUpdateDescriptorSetWithTemplateKHR(
					pRenderer->pVkDevice, pDescriptorSet, pm->mUpdateTemplates[setIndex], pm->pUpdateData[setIndex]);
				++pm->mDescriptorSetCount[pm->mFrameIdx][setIndex];
			}

			vkCmdBindDescriptorSets(
				pCmd->pVkCmdBuf, gPipelineBindPoint[pRootSignature->mPipelineType], pRootSignature->pPipelineLayout, setIndex, 1,
				&pDescriptorSet, rootDescCount, pm->pDynamicOffsets[setIndex]);

			// Set the bound flag for the descriptor set of this update frequency
			// This way in the future if user tries to bind the same descriptor set, we can avoid unnecessary rebinds
			pm->mBoundSets[setIndex] = true;
		}
	}
}

void cmdResourceBarrier(
	Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers,
	bool batch)
{
	VkImageMemoryBarrier* imageBarriers =
		numTextureBarriers ? (VkImageMemoryBarrier*)alloca(numTextureBarriers * sizeof(VkImageMemoryBarrier)) : NULL;
	uint32_t imageBarrierCount = 0;

	VkBufferMemoryBarrier* bufferBarriers =
		numBufferBarriers ? (VkBufferMemoryBarrier*)alloca(numBufferBarriers * sizeof(VkBufferMemoryBarrier)) : NULL;
	uint32_t bufferBarrierCount = 0;

	for (uint32_t i = 0; i < numBufferBarriers; ++i)
	{
		BufferBarrier* pTrans = &pBufferBarriers[i];
		Buffer*        pBuffer = pTrans->pBuffer;
		if (!(pTrans->mNewState & pBuffer->mCurrentState) || pBuffer->mCurrentState == RESOURCE_STATE_UNORDERED_ACCESS)
		{
			VkBufferMemoryBarrier* pBufferBarrier = &bufferBarriers[bufferBarrierCount++];
			pBufferBarrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			pBufferBarrier->pNext = NULL;

			pBufferBarrier->buffer = pBuffer->pVkBuffer;
			pBufferBarrier->size = VK_WHOLE_SIZE;
			pBufferBarrier->offset = 0;

			pBufferBarrier->srcAccessMask = util_to_vk_access_flags(pBuffer->mCurrentState);
			pBufferBarrier->dstAccessMask = util_to_vk_access_flags(pTrans->mNewState);

			pBufferBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			pBufferBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			pBuffer->mCurrentState = pTrans->mNewState;
		}
	}
	for (uint32_t i = 0; i < numTextureBarriers; ++i)
	{
		TextureBarrier* pTrans = &pTextureBarriers[i];
		Texture*        pTexture = pTrans->pTexture;
		if (!(pTrans->mNewState & pTexture->mCurrentState) || pTexture->mCurrentState == RESOURCE_STATE_UNORDERED_ACCESS)
		{
			VkImageMemoryBarrier* pImageBarrier = &imageBarriers[imageBarrierCount++];
			pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			pImageBarrier->pNext = NULL;

			pImageBarrier->image = pTexture->pVkImage;
			pImageBarrier->subresourceRange.aspectMask = pTexture->mVkAspectMask;
			pImageBarrier->subresourceRange.baseMipLevel = 0;
			pImageBarrier->subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
			pImageBarrier->subresourceRange.baseArrayLayer = 0;
			pImageBarrier->subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

			pImageBarrier->srcAccessMask = util_to_vk_access_flags(pTexture->mCurrentState);
			pImageBarrier->dstAccessMask = util_to_vk_access_flags(pTrans->mNewState);
			pImageBarrier->oldLayout = util_to_vk_image_layout(pTexture->mCurrentState);
			pImageBarrier->newLayout = util_to_vk_image_layout(pTrans->mNewState);

			pImageBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			pImageBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			pTexture->mCurrentState = pTrans->mNewState;
		}
	}

	if (bufferBarrierCount || imageBarrierCount)
	{
		uint32_t bufferBarrierEmptySlots = MAX_BATCH_BARRIERS - pCmd->mBatchBufferMemoryBarrierCount;
		uint32_t imageBarrierEmptySlots = MAX_BATCH_BARRIERS - pCmd->mBatchImageMemoryBarrierCount;

		if (batch && bufferBarrierEmptySlots >= bufferBarrierCount && imageBarrierEmptySlots >= imageBarrierCount)
		{
			memcpy(
				pCmd->pBatchBufferMemoryBarriers + pCmd->mBatchBufferMemoryBarrierCount, bufferBarriers,
				bufferBarrierCount * sizeof(VkBufferMemoryBarrier));
			pCmd->mBatchBufferMemoryBarrierCount += bufferBarrierCount;

			memcpy(
				pCmd->pBatchImageMemoryBarriers + pCmd->mBatchImageMemoryBarrierCount, imageBarriers,
				imageBarrierCount * sizeof(VkImageMemoryBarrier));
			pCmd->mBatchImageMemoryBarrierCount += imageBarrierCount;
		}
		else
		{
			VkPipelineStageFlags srcPipelineFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			VkPipelineStageFlags dstPipelineFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			vkCmdPipelineBarrier(
				pCmd->pVkCmdBuf, srcPipelineFlags, dstPipelineFlags, 0, 0, NULL, bufferBarrierCount, bufferBarriers, imageBarrierCount,
				imageBarriers);
		}
	}
}

void cmdSynchronizeResources(Cmd* pCmd, uint32_t numBuffers, Buffer** ppBuffers, uint32_t numTextures, Texture** ppTextures, bool batch)
{
	VkImageMemoryBarrier* imageBarriers = numTextures ? (VkImageMemoryBarrier*)alloca(numTextures * sizeof(VkImageMemoryBarrier)) : NULL;
	uint32_t              imageBarrierCount = 0;

	VkBufferMemoryBarrier* bufferBarriers = numBuffers ? (VkBufferMemoryBarrier*)alloca(numBuffers * sizeof(VkBufferMemoryBarrier)) : NULL;
	uint32_t               bufferBarrierCount = 0;

	for (uint32_t i = 0; i < numBuffers; ++i)
	{
		VkBufferMemoryBarrier* pBufferBarrier = &bufferBarriers[bufferBarrierCount++];
		pBufferBarrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		pBufferBarrier->pNext = NULL;

		pBufferBarrier->buffer = ppBuffers[i]->pVkBuffer;
		pBufferBarrier->size = VK_WHOLE_SIZE;
		pBufferBarrier->offset = 0;

		pBufferBarrier->srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		pBufferBarrier->dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

		pBufferBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		pBufferBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}
	for (uint32_t i = 0; i < numTextures; ++i)
	{
		VkImageMemoryBarrier* pImageBarrier = &imageBarriers[imageBarrierCount++];
		pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		pImageBarrier->pNext = NULL;

		pImageBarrier->image = ppTextures[i]->pVkImage;
		pImageBarrier->subresourceRange.aspectMask = ppTextures[i]->mVkAspectMask;
		pImageBarrier->subresourceRange.baseMipLevel = 0;
		pImageBarrier->subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		pImageBarrier->subresourceRange.baseArrayLayer = 0;
		pImageBarrier->subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

		pImageBarrier->srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		pImageBarrier->dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		pImageBarrier->oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		pImageBarrier->newLayout = VK_IMAGE_LAYOUT_GENERAL;

		pImageBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		pImageBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	if (bufferBarrierCount || imageBarrierCount)
	{
		uint32_t bufferBarrierEmptySlots = MAX_BATCH_BARRIERS - pCmd->mBatchBufferMemoryBarrierCount;
		uint32_t imageBarrierEmptySlots = MAX_BATCH_BARRIERS - pCmd->mBatchImageMemoryBarrierCount;

		if (batch && bufferBarrierEmptySlots >= bufferBarrierCount && imageBarrierEmptySlots >= imageBarrierCount)
		{
			memcpy(
				pCmd->pBatchBufferMemoryBarriers + pCmd->mBatchBufferMemoryBarrierCount, bufferBarriers,
				bufferBarrierCount * sizeof(VkBufferMemoryBarrier));
			pCmd->mBatchBufferMemoryBarrierCount += bufferBarrierCount;

			memcpy(
				pCmd->pBatchImageMemoryBarriers + pCmd->mBatchImageMemoryBarrierCount, imageBarriers,
				imageBarrierCount * sizeof(VkImageMemoryBarrier));
			pCmd->mBatchImageMemoryBarrierCount += imageBarrierCount;
		}
		else
		{
			VkPipelineStageFlags srcPipelineFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			VkPipelineStageFlags dstPipelineFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			vkCmdPipelineBarrier(
				pCmd->pVkCmdBuf, srcPipelineFlags, dstPipelineFlags, 0, 0, NULL, bufferBarrierCount, bufferBarriers, imageBarrierCount,
				imageBarriers);
		}
	}
}

void cmdFlushBarriers(Cmd* pCmd)
{
	if (pCmd->mBatchBufferMemoryBarrierCount || pCmd->mBatchImageMemoryBarrierCount)
	{
		VkPipelineStageFlags srcPipelineFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		VkPipelineStageFlags dstPipelineFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		vkCmdPipelineBarrier(
			pCmd->pVkCmdBuf, srcPipelineFlags, dstPipelineFlags, 0, 0, NULL, pCmd->mBatchBufferMemoryBarrierCount,
			pCmd->pBatchBufferMemoryBarriers, pCmd->mBatchImageMemoryBarrierCount, pCmd->pBatchImageMemoryBarriers);

		pCmd->mBatchBufferMemoryBarrierCount = 0;
		pCmd->mBatchImageMemoryBarrierCount = 0;
	}
}

void cmdUpdateBuffer(Cmd* pCmd, uint64_t srcOffset, uint64_t dstOffset, uint64_t size, Buffer* pSrcBuffer, Buffer* pBuffer)
{
	ASSERT(pCmd);
	ASSERT(pSrcBuffer);
	ASSERT(pSrcBuffer->pVkBuffer);
	ASSERT(pBuffer);
	ASSERT(pBuffer->pVkBuffer);
	ASSERT(srcOffset + size <= pSrcBuffer->mDesc.mSize);
	ASSERT(dstOffset + size <= pBuffer->mDesc.mSize);

	DECLARE_ZERO(VkBufferCopy, region);
	region.srcOffset = srcOffset;
	region.dstOffset = dstOffset;
	region.size = (VkDeviceSize)size;
	vkCmdCopyBuffer(pCmd->pVkCmdBuf, pSrcBuffer->pVkBuffer, pBuffer->pVkBuffer, 1, &region);
}

void cmdUpdateSubresources(
	Cmd* pCmd, uint32_t startSubresource, uint32_t numSubresources, SubresourceDataDesc* pSubresources, Buffer* pIntermediate,
	uint64_t intermediateOffset, Texture* pTexture)
{
	VkBufferImageCopy* pCopyRegions = (VkBufferImageCopy*)alloca(numSubresources * sizeof(VkBufferImageCopy));
	for (uint32_t i = startSubresource; i < startSubresource + numSubresources; ++i)
	{
		VkBufferImageCopy*   pCopy = &pCopyRegions[i];
		SubresourceDataDesc* pRes = &pSubresources[i];

		pCopy->bufferOffset = pRes->mBufferOffset;
		pCopy->bufferRowLength = 0;
		pCopy->bufferImageHeight = 0;
		pCopy->imageSubresource.aspectMask = pTexture->mVkAspectMask;
		pCopy->imageSubresource.mipLevel = pRes->mMipLevel;
		pCopy->imageSubresource.baseArrayLayer = pRes->mArrayLayer;
		pCopy->imageSubresource.layerCount = pRes->mArraySize;
		pCopy->imageOffset.x = 0;
		pCopy->imageOffset.y = 0;
		pCopy->imageOffset.z = 0;
		pCopy->imageExtent.width = pRes->mWidth;
		pCopy->imageExtent.height = pRes->mHeight;
		pCopy->imageExtent.depth = pRes->mDepth;
	}

	vkCmdCopyBufferToImage(
		pCmd->pVkCmdBuf, pIntermediate->pVkBuffer, pTexture->pVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, numSubresources, pCopyRegions);
}
/************************************************************************/
// Queue Fence Semaphore Functions
/************************************************************************/
void acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex)
{
	ASSERT(pRenderer);
	ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
	ASSERT(VK_NULL_HANDLE != pSwapChain->pSwapChain);
	ASSERT(pSignalSemaphore || pFence);

	VkResult vk_res = {};

	if (pFence != NULL)
	{
		vk_res =
			vkAcquireNextImageKHR(pRenderer->pVkDevice, pSwapChain->pSwapChain, UINT64_MAX, VK_NULL_HANDLE, pFence->pVkFence, pImageIndex);

		// If swapchain is out of date, let caller know by setting image index to -1
		if (vk_res == VK_ERROR_OUT_OF_DATE_KHR)
		{
			*pImageIndex = -1;
			vkResetFences(pRenderer->pVkDevice, 1, &pFence->pVkFence);
			pFence->mSubmitted = false;
			return;
		}

		pFence->mSubmitted = true;
	}
	else
	{
		vk_res = vkAcquireNextImageKHR(
			pRenderer->pVkDevice, pSwapChain->pSwapChain, UINT64_MAX, pSignalSemaphore->pVkSemaphore, VK_NULL_HANDLE, pImageIndex);

		// If swapchain is out of date, let caller know by setting image index to -1
		if (vk_res == VK_ERROR_OUT_OF_DATE_KHR)
		{
			*pImageIndex = -1;
			pSignalSemaphore->mSignaled = false;
			return;
		}

		ASSERT(VK_SUCCESS == vk_res);
		pSignalSemaphore->mSignaled = true;
	}
}

void queueSubmit(
	Queue* pQueue, uint32_t cmdCount, Cmd** ppCmds, Fence* pFence, uint32_t waitSemaphoreCount, Semaphore** ppWaitSemaphores,
	uint32_t signalSemaphoreCount, Semaphore** ppSignalSemaphores)
{
	ASSERT(pQueue);
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

	ASSERT(VK_NULL_HANDLE != pQueue->pVkQueue);

	cmdCount = cmdCount > MAX_SUBMIT_CMDS ? MAX_SUBMIT_CMDS : cmdCount;
	waitSemaphoreCount = waitSemaphoreCount > MAX_SUBMIT_WAIT_SEMAPHORES ? MAX_SUBMIT_WAIT_SEMAPHORES : waitSemaphoreCount;
	signalSemaphoreCount = signalSemaphoreCount > MAX_SUBMIT_SIGNAL_SEMAPHORES ? MAX_SUBMIT_SIGNAL_SEMAPHORES : signalSemaphoreCount;

	VkCommandBuffer* cmds = (VkCommandBuffer*)alloca(cmdCount * sizeof(VkCommandBuffer));
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		cmds[i] = ppCmds[i]->pVkCmdBuf;
	}

	VkSemaphore*          wait_semaphores = waitSemaphoreCount ? (VkSemaphore*)alloca(waitSemaphoreCount * sizeof(VkSemaphore)) : NULL;
	VkPipelineStageFlags* wait_masks = (VkPipelineStageFlags*)alloca(waitSemaphoreCount * sizeof(VkPipelineStageFlags));
	uint32_t              waitCount = 0;
	for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
	{
		if (ppWaitSemaphores[i]->mSignaled)
		{
			wait_semaphores[waitCount] = ppWaitSemaphores[i]->pVkSemaphore;
			wait_masks[waitCount] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			++waitCount;

			ppWaitSemaphores[i]->mSignaled = false;
		}
	}

	VkSemaphore* signal_semaphores = signalSemaphoreCount ? (VkSemaphore*)alloca(signalSemaphoreCount * sizeof(VkSemaphore)) : NULL;
	uint32_t     signalCount = 0;
	for (uint32_t i = 0; i < signalSemaphoreCount; ++i)
	{
		if (!ppSignalSemaphores[i]->mSignaled)
		{
			signal_semaphores[signalCount] = ppSignalSemaphores[i]->pVkSemaphore;
			ppSignalSemaphores[i]->mCurrentNodeIndex = pQueue->mQueueDesc.mNodeIndex;
			ppSignalSemaphores[signalCount]->mSignaled = true;
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
	if (pQueue->pRenderer->mSettings.mGpuMode == GPU_MODE_LINKED)
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
			pVkDeviceMasks[i] = (1 << ppCmds[i]->mNodeIndex);
		}
		for (uint32_t i = 0; i < deviceGroupSubmitInfo.signalSemaphoreCount; ++i)
		{
			pSignalIndices[i] = pQueue->mQueueDesc.mNodeIndex;
		}
		for (uint32_t i = 0; i < deviceGroupSubmitInfo.waitSemaphoreCount; ++i)
		{
			pWaitIndices[i] = ppWaitSemaphores[i]->mCurrentNodeIndex;
		}

		deviceGroupSubmitInfo.pCommandBufferDeviceMasks = pVkDeviceMasks;
		deviceGroupSubmitInfo.pSignalSemaphoreDeviceIndices = pSignalIndices;
		deviceGroupSubmitInfo.pWaitSemaphoreDeviceIndices = pWaitIndices;
		submit_info.pNext = &deviceGroupSubmitInfo;
	}

	VkResult vk_res = vkQueueSubmit(pQueue->pVkQueue, 1, &submit_info, pFence->pVkFence);
	ASSERT(VK_SUCCESS == vk_res);

	pFence->mSubmitted = true;
}

void queuePresent(
	Queue* pQueue, SwapChain* pSwapChain, uint32_t swapChainImageIndex, uint32_t waitSemaphoreCount, Semaphore** ppWaitSemaphores)
{
	ASSERT(pQueue);
	if (waitSemaphoreCount > 0)
	{
		ASSERT(ppWaitSemaphores);
	}

	ASSERT(VK_NULL_HANDLE != pQueue->pVkQueue);

	VkSemaphore* wait_semaphores = waitSemaphoreCount ? (VkSemaphore*)alloca(waitSemaphoreCount * sizeof(VkSemaphore)) : NULL;
	waitSemaphoreCount = waitSemaphoreCount > MAX_PRESENT_WAIT_SEMAPHORES ? MAX_PRESENT_WAIT_SEMAPHORES : waitSemaphoreCount;
	uint32_t waitCount = 0;
	for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
	{
		if (ppWaitSemaphores[i]->mSignaled)
		{
			wait_semaphores[waitCount] = ppWaitSemaphores[i]->pVkSemaphore;
			ppWaitSemaphores[i]->mSignaled = false;
			++waitCount;
		}
	}

	DECLARE_ZERO(VkPresentInfoKHR, present_info);
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = NULL;
	present_info.waitSemaphoreCount = waitCount;
	present_info.pWaitSemaphores = wait_semaphores;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &(pSwapChain->pSwapChain);
	present_info.pImageIndices = &(swapChainImageIndex);
	present_info.pResults = NULL;

	VkResult vk_res = vkQueuePresentKHR(pSwapChain->pPresentQueue ? pSwapChain->pPresentQueue : pQueue->pVkQueue, &present_info);
	if (vk_res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		// TODO : Fix bug where we get this error if window is closed before able to present queue.
	}
	else
		ASSERT(VK_SUCCESS == vk_res);
}

void waitForFences(Queue* pQueue, uint32_t fenceCount, Fence** ppFences, bool signal)
{
	ASSERT(pQueue);
	ASSERT(fenceCount);
	ASSERT(ppFences);

	if (signal)
		vkQueueWaitIdle(pQueue->pVkQueue);

	VkFence* pFences = (VkFence*)alloca(fenceCount * sizeof(VkFence));
	uint32_t numValidFences = 0;
	for (uint32_t i = 0; i < fenceCount; ++i)
	{
		if (ppFences[i]->mSubmitted)
			pFences[numValidFences++] = ppFences[i]->pVkFence;
	}

	if (numValidFences)
	{
		vkWaitForFences(pQueue->pRenderer->pVkDevice, numValidFences, pFences, VK_TRUE, UINT64_MAX);
		vkResetFences(pQueue->pRenderer->pVkDevice, numValidFences, pFences);
	}

	for (uint32_t i = 0; i < fenceCount; ++i)
		ppFences[i]->mSubmitted = false;
}

void getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
	*pFenceStatus = FENCE_STATUS_COMPLETE;

	if (pFence->mSubmitted)
	{
		VkResult vkRes = vkGetFenceStatus(pRenderer->pVkDevice, pFence->pVkFence);
		if (vkRes == VK_SUCCESS)
		{
			vkResetFences(pRenderer->pVkDevice, 1, &pFence->pVkFence);
			pFence->mSubmitted = false;
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
ImageFormat::Enum getRecommendedSwapchainFormat(bool hintHDR)
{
	//TODO: figure out this properly. BGRA not supported on android
#ifndef VK_USE_PLATFORM_ANDROID_KHR
	return ImageFormat::BGRA8;
#else
	return ImageFormat::RGBA8;
#endif
}

bool isImageFormatSupported(ImageFormat::Enum format) { return gVkFormatTranslator[format] != VK_FORMAT_UNDEFINED; }
/************************************************************************/
// Indirect draw functions
/************************************************************************/
void addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);

	CommandSignature* pCommandSignature = (CommandSignature*)conf_calloc(1, sizeof(CommandSignature));
	pCommandSignature->mDesc = *pDesc;

	for (uint32_t i = 0; i < pDesc->mIndirectArgCount; ++i)    // counting for all types;
	{
		switch (pDesc->pArgDescs[i].mType)
		{
			case INDIRECT_DRAW:
				pCommandSignature->mDrawType = INDIRECT_DRAW;
				pCommandSignature->mDrawCommandStride += sizeof(IndirectDrawArguments);
				break;
			case INDIRECT_DRAW_INDEX:
				pCommandSignature->mDrawType = INDIRECT_DRAW_INDEX;
				pCommandSignature->mDrawCommandStride += sizeof(IndirectDrawIndexArguments);
				break;
			case INDIRECT_DISPATCH:
				pCommandSignature->mDrawType = INDIRECT_DISPATCH;
				pCommandSignature->mDrawCommandStride += sizeof(IndirectDispatchArguments);
				break;
			default: LOGERROR("Vulkan runtime only supports IndirectDraw, IndirectDrawIndex and IndirectDispatch at this point"); break;
		}
	}

	pCommandSignature->mDrawCommandStride = round_up(pCommandSignature->mDrawCommandStride, 16);

	*ppCommandSignature = pCommandSignature;
}

void removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature) { SAFE_FREE(pCommandSignature); }

void cmdExecuteIndirect(
	Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
	Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
	if (pCommandSignature->mDrawType == INDIRECT_DRAW)
	{
		if (pCounterBuffer && pfnVkCmdDrawIndirectCountKHR)
			pfnVkCmdDrawIndirectCountKHR(
				pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset, pCounterBuffer->pVkBuffer, counterBufferOffset, maxCommandCount,
				pCommandSignature->mDrawCommandStride);
		else
			vkCmdDrawIndirect(
				pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset, maxCommandCount, pCommandSignature->mDrawCommandStride);
	}
	else if (pCommandSignature->mDrawType == INDIRECT_DRAW_INDEX)
	{
		if (pCounterBuffer && pfnVkCmdDrawIndexedIndirectCountKHR)
			pfnVkCmdDrawIndexedIndirectCountKHR(
				pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset, pCounterBuffer->pVkBuffer, counterBufferOffset, maxCommandCount,
				pCommandSignature->mDrawCommandStride);
		else
			vkCmdDrawIndexedIndirect(
				pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset, maxCommandCount, pCommandSignature->mDrawCommandStride);
	}
	else if (pCommandSignature->mDrawType == INDIRECT_DISPATCH)
	{
		vkCmdDispatchIndirect(pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset);
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

void getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
	ASSERT(pQueue);
	ASSERT(pFrequency);

	// The framework is using ticks per sec as frequency. Vulkan is nano sec per tick.
	// Handle the conversion logic here.
	*pFrequency = 1.0f / ((double)pQueue->pRenderer->pVkActiveGPUProperties->properties.limits
							  .timestampPeriod /*ns/tick number of nanoseconds required for a timestamp query to be incremented by 1*/
						  * 1e-9);             // convert to ticks/sec (DX12 standard)
}

void addQueryHeap(Renderer* pRenderer, const QueryHeapDesc* pDesc, QueryHeap** ppQueryHeap)
{
	QueryHeap* pQueryHeap = (QueryHeap*)conf_calloc(1, sizeof(*pQueryHeap));
	pQueryHeap->mDesc = *pDesc;

	VkQueryPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	createInfo.pNext = NULL;
	createInfo.queryCount = pDesc->mQueryCount;
	createInfo.queryType = util_to_vk_query_type(pDesc->mType);
	createInfo.flags = 0;
	createInfo.pipelineStatistics = 0;
	vkCreateQueryPool(pRenderer->pVkDevice, &createInfo, NULL, &pQueryHeap->pVkQueryPool);

	*ppQueryHeap = pQueryHeap;
}

void removeQueryHeap(Renderer* pRenderer, QueryHeap* pQueryHeap)
{
	vkDestroyQueryPool(pRenderer->pVkDevice, pQueryHeap->pVkQueryPool, NULL);
	SAFE_FREE(pQueryHeap);
}

void cmdBeginQuery(Cmd* pCmd, QueryHeap* pQueryHeap, QueryDesc* pQuery)
{
	QueryType type = pQueryHeap->mDesc.mType;
	switch (type)
	{
		case QUERY_TYPE_TIMESTAMP:
			vkCmdWriteTimestamp(pCmd->pVkCmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pQueryHeap->pVkQueryPool, pQuery->mIndex);
			break;
		case QUERY_TYPE_PIPELINE_STATISTICS: break;
		case QUERY_TYPE_OCCLUSION: break;
		default: break;
	}
}

void cmdEndQuery(Cmd* pCmd, QueryHeap* pQueryHeap, QueryDesc* pQuery)
{
	QueryType type = pQueryHeap->mDesc.mType;
	switch (type)
	{
		case QUERY_TYPE_TIMESTAMP:
			vkCmdWriteTimestamp(pCmd->pVkCmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pQueryHeap->pVkQueryPool, pQuery->mIndex);
			break;
		case QUERY_TYPE_PIPELINE_STATISTICS: break;
		case QUERY_TYPE_OCCLUSION: break;
		default: break;
	}
}

void cmdResolveQuery(Cmd* pCmd, QueryHeap* pQueryHeap, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
{
	vkCmdCopyQueryPoolResults(
		pCmd->pVkCmdBuf, pQueryHeap->pVkQueryPool, startQuery, queryCount, pReadbackBuffer->pVkBuffer, 0, sizeof(uint64_t),
		VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
}
/************************************************************************/
// Memory Stats Implementation
/************************************************************************/
void calculateMemoryStats(Renderer* pRenderer, char** stats) { vmaBuildStatsString(pRenderer->pVmaAllocator, stats, 0); }

void freeMemoryStats(Renderer* pRenderer, char* stats) { vmaFreeStatsString(pRenderer->pVmaAllocator, stats); }
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
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
		vkCmdBeginDebugUtilsLabelEXT(pCmd->pVkCmdBuf, &markerInfo);
#else
		VkDebugMarkerMarkerInfoEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		markerInfo.color[0] = r;
		markerInfo.color[1] = g;
		markerInfo.color[2] = b;
		markerInfo.color[3] = 1.0f;
		markerInfo.pMarkerName = pName;
		vkCmdDebugMarkerBeginEXT(pCmd->pVkCmdBuf, &markerInfo);
#endif
	}
}

void cmdEndDebugMarker(Cmd* pCmd)
{
	if (gDebugMarkerSupport)
	{
#ifdef USE_DEBUG_UTILS_EXTENSION
		vkCmdEndDebugUtilsLabelEXT(pCmd->pVkCmdBuf);
#else
		vkCmdDebugMarkerEndEXT(pCmd->pVkCmdBuf);
#endif
	}
}

void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
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
		vkCmdInsertDebugUtilsLabelEXT(pCmd->pVkCmdBuf, &markerInfo);
#else
		VkDebugMarkerMarkerInfoEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		markerInfo.color[0] = r;
		markerInfo.color[1] = g;
		markerInfo.color[2] = b;
		markerInfo.color[3] = 1.0f;
		markerInfo.pMarkerName = pName;
		vkCmdDebugMarkerInsertEXT(pCmd->pVkCmdBuf, &markerInfo);

#endif
	}
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
	ASSERT(pRenderer);
	ASSERT(pBuffer);
	ASSERT(pName);

	if (gDebugMarkerSupport)
	{
#ifdef USE_DEBUG_UTILS_EXTENSION
		VkDebugUtilsObjectNameInfoEXT nameInfo = {};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
		nameInfo.objectHandle = (uint64_t)pBuffer->pVkBuffer;
		nameInfo.pObjectName = pName;
		vkSetDebugUtilsObjectNameEXT(pRenderer->pVkDevice, &nameInfo);
#else
		VkDebugMarkerObjectNameInfoEXT nameInfo = {};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT;
		nameInfo.object = (uint64_t)pBuffer->pVkBuffer;
		nameInfo.pObjectName = pName;
		vkDebugMarkerSetObjectNameEXT(pRenderer->pVkDevice, &nameInfo);
#endif
	}
}

void setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
	ASSERT(pRenderer);
	ASSERT(pTexture);
	ASSERT(pName);

	if (gDebugMarkerSupport)
	{
#ifdef USE_DEBUG_UTILS_EXTENSION
		VkDebugUtilsObjectNameInfoEXT nameInfo = {};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
		nameInfo.objectHandle = (uint64_t)pTexture->pVkImage;
		nameInfo.pObjectName = pName;
		vkSetDebugUtilsObjectNameEXT(pRenderer->pVkDevice, &nameInfo);
#else
		VkDebugMarkerObjectNameInfoEXT nameInfo = {};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
		nameInfo.object = (uint64_t)pTexture->pVkImage;
		nameInfo.pObjectName = pName;
		vkDebugMarkerSetObjectNameEXT(pRenderer->pVkDevice, &nameInfo);

#endif
	}
}
/************************************************************************/
/************************************************************************/
#endif    // RENDERER_IMPLEMENTATION

#if defined(__cplusplus) && defined(ENABLE_RENDERER_RUNTIME_SWITCH)
}    // namespace RENDERER_CPP_NAMESPACE
#endif
#include "../../../Common_3/ThirdParty/OpenSource/volk/volk.c"
#endif

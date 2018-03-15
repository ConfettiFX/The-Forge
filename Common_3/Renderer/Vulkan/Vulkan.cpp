/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

#if defined(_WIN32)
// Pull in minimal Windows headers
#if ! defined(NOMINMAX)
#define NOMINMAX
#endif
#if ! defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#if defined(__cplusplus) && defined(RENDERER_CPP_NAMESPACE)
namespace RENDERER_CPP_NAMESPACE {
#endif

#include "../IRenderer.h"
#include "../../ThirdParty/OpenSource/TinySTL/hash.h"
#include "../../OS/Interfaces/ILogManager.h"
#include "../IMemoryAllocator.h"
#include "../../OS/Interfaces/IMemoryManager.h"

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
		VK_STENCIL_OP_DECREMENT_AND_WRAP,
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
		VK_FORMAT_UNDEFINED, // INTZ = 60,	//	NVidia hack. Supported on all DX10+ HW
		//	XBox 360 specific fron buffer formats. NOt listed in other renderers. Please, add them when extend this structure.
		VK_FORMAT_UNDEFINED, // LE_XRGB8 = 61,
		VK_FORMAT_UNDEFINED, // LE_ARGB8 = 62,
		VK_FORMAT_UNDEFINED, // LE_X2RGB10 = 63,
		VK_FORMAT_UNDEFINED, // LE_A2RGB10 = 64,
		// compressed mobile forms
		VK_FORMAT_UNDEFINED, // ETC1 = 65,	//	RGB
		VK_FORMAT_UNDEFINED, // ATC = 66,	//	RGB
		VK_FORMAT_UNDEFINED, // ATCA = 67,	//	RGBA, explicit alpha
		VK_FORMAT_UNDEFINED, // ATCI = 68,	//	RGBA, interpolated alpha
		VK_FORMAT_UNDEFINED, // RAWZ = 69, //depth only, Nvidia (requires recombination of data) //FIX IT: PS3 as well?
		VK_FORMAT_UNDEFINED, // DF16 = 70, //depth only, Intel/AMD
		VK_FORMAT_UNDEFINED, // STENCILONLY = 71, // stencil ony usage
		VK_FORMAT_UNDEFINED, // GNF_BC1 = 72,
		VK_FORMAT_UNDEFINED, // GNF_BC2 = 73,
		VK_FORMAT_UNDEFINED, // GNF_BC3 = 74,
		VK_FORMAT_UNDEFINED, // GNF_BC4 = 75,
		VK_FORMAT_UNDEFINED, // GNF_BC5 = 76,
		VK_FORMAT_UNDEFINED, // GNF_BC6 = 77,
		VK_FORMAT_UNDEFINED, // GNF_BC7 = 78,
		// Reveser Form
		VK_FORMAT_B8G8R8A8_UNORM, // BGRA8 = 79,
		// Extend for DXGI
		VK_FORMAT_UNDEFINED, // X8D24PAX32 = 80,
		VK_FORMAT_UNDEFINED, // S8 = 81,
		VK_FORMAT_UNDEFINED, // D16S8 = 82,
		VK_FORMAT_UNDEFINED, // D32S8 = 83,
	};

   const char* gVkWantedInstanceExtensions[] =
   {
      VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
      VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
      VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
	  VK_NV_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
   };

   const char* gVkWantedDeviceExtensions[] =
   {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_KHR_MAINTENANCE1_EXTENSION_NAME,
	  VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
	  VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME,
	  VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
	  VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
	  VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
	  VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	  VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
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
	  // VR Extensions
	  /************************************************************************/
	  VK_KHR_DISPLAY_EXTENSION_NAME,
	  VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
	  /************************************************************************/
	  /************************************************************************/
   };

	static bool	gDebugMarkerExtension = false;
	static bool	gRenderDocLayerEnabled = false;
	static bool	gDedicatedAllocationExtension = false;
	static bool	gExternalMemoryExtension = false;
	static bool	gDrawIndirectCountAMDExtension = false;
	// =================================================================================================
	// IMPLEMENTATION
	// =================================================================================================

#if defined(RENDERER_IMPLEMENTATION)

#pragma comment(lib, "vulkan-1.lib")

#define SAFE_FREE(p_var)		\
    if(p_var) {               \
       conf_free((void*)p_var);      \
    }

#if defined(__cplusplus)  
#define DECLARE_ZERO(type, var) \
            type var = {};
#else
#define DECLARE_ZERO(type, var) \
            type var = {0};
#endif

	// Internal utility functions (may become external one day)
	VkSampleCountFlagBits	util_to_vk_sample_count(SampleCount sampleCount);
	VkBufferUsageFlags		util_to_vk_buffer_usage(BufferUsage usage);
	VkImageUsageFlags		util_to_vk_image_usage(TextureUsage usage);
	VkImageLayout			util_to_vk_image_layout(ResourceState usage);
	VkAccessFlags			util_to_vk_access_flags(ResourceState state);
	VkImageAspectFlags		util_vk_determine_aspect_mask(VkFormat format);
	VkFormatFeatureFlags	util_vk_image_usage_to_format_features(VkImageUsageFlags usage);
	VkFilter				util_to_vk_filter(FilterType filter);
	VkSamplerMipmapMode		util_to_vk_mip_map_mode(MipMapMode mipMapMode);
	VkSamplerAddressMode	util_to_vk_address_mode(AddressMode addressMode);
	VkFormat				util_to_vk_image_format(ImageFormat::Enum format, bool srgb);
	ImageFormat::Enum		util_to_internal_image_format(VkFormat format);
	VkQueueFlags			util_to_vk_queue_flags(CmdPoolType cmddPoolType);

	// Internal init functions
	void CreateInstance(const char* app_name, Renderer* pRenderer);
	void RemoveInstance(Renderer* pRenderer);
	void AddDevice(Renderer* pRenderer);
	void RemoveDevice(Renderer* pRenderer);
  /************************************************************************/
  // Memory Manager Data Structures
  /************************************************************************/
  // Memory allocation #2 after VmaAllocator_T definition

  typedef struct DynamicMemoryAllocator
  {
	  /// Size of mapped resources to be created
	  uint64_t mSize;
	  /// Current offset in the used page
	  uint64_t mCurrentPos;
	  /// Buffer alignment
	  uint64_t mAlignment;
	  Buffer* pBuffer;

	  Mutex* pAllocationMutex;
  } DynamicMemoryAllocator;

  void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
  void removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
  void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture);
  void removeTexture(Renderer* pRenderer, Texture* pTexture);

  void add_dynamic_memory_allocator(Renderer* pRenderer, uint64_t size, DynamicMemoryAllocator** ppAllocator)
  {
	  ASSERT(pRenderer);

	  DynamicMemoryAllocator* pAllocator = (DynamicMemoryAllocator*)conf_calloc(1, sizeof(*pAllocator));
	  pAllocator->mCurrentPos = 0;
	  pAllocator->mSize = size;
	  pAllocator->pAllocationMutex = conf_placement_new<Mutex>(conf_calloc(1, sizeof(Mutex)));

	  BufferDesc desc = {};
	  desc.mUsage = (BufferUsage)(BUFFER_USAGE_INDEX | BUFFER_USAGE_VERTEX | BUFFER_USAGE_UNIFORM);
	  desc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	  desc.mSize = pAllocator->mSize;
	  desc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	  addBuffer(pRenderer, &desc, &pAllocator->pBuffer);

	  pAllocator->mAlignment = pRenderer->pActiveGpuSettings->mUniformBufferAlignment;

	  *ppAllocator = pAllocator;
  }

  void remove_dynamic_memory_allocator(Renderer* pRenderer, DynamicMemoryAllocator* pAllocator)
  {
	  ASSERT(pAllocator);

	  removeBuffer(pRenderer, pAllocator->pBuffer);

	  pAllocator->pAllocationMutex->~Mutex();
	  conf_free(pAllocator->pAllocationMutex);

	  SAFE_FREE(pAllocator);
  }

  void reset_dynamic_memory_allocator(DynamicMemoryAllocator* pAllocator)
  {
	  ASSERT(pAllocator);
	  pAllocator->mCurrentPos = 0;
  }

  void consume_dynamic_memory_allocator(DynamicMemoryAllocator* p_linear_allocator, uint64_t size, void** ppCpuAddress, uint64_t* pOffset, VkBuffer* ppVkBuffer = NULL)
  {
	  MutexLock lock(*p_linear_allocator->pAllocationMutex);

	  if (p_linear_allocator->mCurrentPos + size > p_linear_allocator->mSize)
		  reset_dynamic_memory_allocator(p_linear_allocator);

	  *ppCpuAddress = (uint8_t*)p_linear_allocator->pBuffer->pCpuMappedAddress + p_linear_allocator->mCurrentPos;
	  *pOffset = p_linear_allocator->mCurrentPos;
	  if (ppVkBuffer)
		  *ppVkBuffer = p_linear_allocator->pBuffer->pVkBuffer;

	  // Increment position by multiple of 256 to use CBVs in same heap as other buffers
	  p_linear_allocator->mCurrentPos += round_up_64(size, p_linear_allocator->mAlignment);
  }

  void consume_dynamic_memory_allocator_lock_free(DynamicMemoryAllocator* p_linear_allocator, uint64_t size, void** ppCpuAddress, uint64_t* pOffset, VkBuffer* ppVkBuffer = NULL)
  {
	  if (p_linear_allocator->mCurrentPos + size > p_linear_allocator->mSize)
		  reset_dynamic_memory_allocator(p_linear_allocator);

	  *ppCpuAddress = (uint8_t*)p_linear_allocator->pBuffer->pCpuMappedAddress + p_linear_allocator->mCurrentPos;
	  *pOffset = p_linear_allocator->mCurrentPos;
	  if (ppVkBuffer)
		  *ppVkBuffer = p_linear_allocator->pBuffer->pVkBuffer;

	  // Increment position by multiple of 256 to use CBVs in same heap as other buffers
	  p_linear_allocator->mCurrentPos += round_up_64(size, p_linear_allocator->mAlignment);
  }
  /************************************************************************/
  // DescriptorInfo Heap Defines
  /************************************************************************/
  static const uint32_t gDefaultDescriptorPoolSize = 4096;
  static const uint32_t gDefaultDescriptorSets = gDefaultDescriptorPoolSize * VK_DESCRIPTOR_TYPE_RANGE_SIZE;

  VkDescriptorPoolSize gDescriptorHeapPoolSizes[VK_DESCRIPTOR_TYPE_RANGE_SIZE] =
  {
	  { VK_DESCRIPTOR_TYPE_SAMPLER, gDefaultDescriptorPoolSize },
	  // Not used in framework
	  { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
	  { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, gDefaultDescriptorPoolSize * 4 },
	  { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, gDefaultDescriptorPoolSize * 4 },
	  // Not used in framework
	  { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1 },
	  // Not used in framework
	  { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, },
	  { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, gDefaultDescriptorPoolSize * 4 },
	  { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, gDefaultDescriptorPoolSize * 4 },
	  { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, gDefaultDescriptorPoolSize },
	  { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, gDefaultDescriptorPoolSize },
	  // Not used in framework
	  { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 },
  };

  static const uint32_t gDefaultDynamicDescriptorPoolSize = gDefaultDescriptorPoolSize / 16;
  static const uint32_t gDefaultDynamicDescriptorSets = gDefaultDynamicDescriptorPoolSize * VK_DESCRIPTOR_TYPE_RANGE_SIZE;

  VkDescriptorPoolSize gDynamicDescriptorHeapPoolSizes[VK_DESCRIPTOR_TYPE_RANGE_SIZE] =
  {
	  { VK_DESCRIPTOR_TYPE_SAMPLER, gDefaultDynamicDescriptorPoolSize },
	  // Not used in framework
	  { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
	  { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, gDefaultDynamicDescriptorPoolSize },
	  { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, gDefaultDynamicDescriptorPoolSize },
	  // Not used in framework
	  { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1 },
	  // Not used in framework
	  { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, },
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
	  Mutex* pAllocationMutex;
	  uint32_t mUsedDescriptorSetCount;
	  /// VK Heap
	  VkDescriptorPool pCurrentHeap;
	  VkDescriptorPoolCreateFlags mFlags;
  } DescriptorStoreHeap;
  /************************************************************************/
  // Static DescriptorInfo Heap Implementation
  /************************************************************************/
  void add_descriptor_heap(Renderer* pRenderer, uint32_t numDescriptorSets, VkDescriptorPoolCreateFlags flags, VkDescriptorPoolSize* pPoolSizes, uint32_t numPoolSizes, DescriptorStoreHeap** ppHeap)
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

	  VkResult res = vkCreateDescriptorPool(pRenderer->pDevice, &poolCreateInfo, NULL, &pHeap->pCurrentHeap);
	  ASSERT(VK_SUCCESS == res);

	  *ppHeap = pHeap;
  }

  void reset_descriptor_heap(Renderer* pRenderer, DescriptorStoreHeap* pHeap)
  {
	  VkResult res = vkResetDescriptorPool(pRenderer->pDevice, pHeap->pCurrentHeap, pHeap->mFlags);
	  pHeap->mUsedDescriptorSetCount = 0;
	  ASSERT(VK_SUCCESS == res);
  }

  void remove_descriptor_heap(Renderer* pRenderer, DescriptorStoreHeap* pHeap)
  {
	  pHeap->pAllocationMutex->~Mutex();
	  conf_free(pHeap->pAllocationMutex);
	  vkDestroyDescriptorPool(pRenderer->pDevice, pHeap->pCurrentHeap, NULL);
	  SAFE_FREE(pHeap);
  }

  void consume_descriptor_sets_lock_free(Renderer* pRenderer, const VkDescriptorSetLayout* pLayouts, VkDescriptorSet** pSets, uint32_t numDescriptorSets, DescriptorStoreHeap* pHeap)
  {
	  DECLARE_ZERO(VkDescriptorSetAllocateInfo, alloc_info);
	  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	  alloc_info.pNext = NULL;
	  alloc_info.descriptorPool = pHeap->pCurrentHeap;
	  alloc_info.descriptorSetCount = numDescriptorSets;
	  alloc_info.pSetLayouts = pLayouts;

	  VkResult vk_res = vkAllocateDescriptorSets(pRenderer->pDevice, &alloc_info, *pSets);
	  ASSERT(VK_SUCCESS == vk_res);

	  pHeap->mUsedDescriptorSetCount += numDescriptorSets;
  }

  void consume_descriptor_sets(Renderer* pRenderer, const VkDescriptorSetLayout* pLayouts, VkDescriptorSet** pSets, uint32_t numDescriptorSets, DescriptorStoreHeap* pHeap)
  {
	  MutexLock lock(*pHeap->pAllocationMutex);
	  consume_descriptor_sets_lock_free(pRenderer, pLayouts, pSets, numDescriptorSets, pHeap);
  }
  /************************************************************************/
  // Descriptor Manager Implementation
  /************************************************************************/
  using DescriptorSetMap = tinystl::unordered_map<uint64_t, VkDescriptorSet>;
  using ConstDescriptorSetMapIterator = tinystl::unordered_map<uint64_t, VkDescriptorSet>::const_iterator;
  using DescriptorSetMapNode = tinystl::unordered_hash_node<uint64_t, VkDescriptorSet>;
  using DescriptorNameToIndexMap = tinystl::unordered_map<uint32_t, uint32_t>;

  typedef struct DescriptorManager
  {
	  /// The root signature associated with this descriptor manager
	  RootSignature*			pRootSignature;
	  /// Array of Dynamic offsets per update frequency to pass the vkCmdBindDescriptorSet for binding dynamic uniform or storage buffers
	  uint32_t*					pDynamicOffsets[DESCRIPTOR_UPDATE_FREQ_COUNT];
	  /// Array of flags to check whether a descriptor set of the update frequency is already bound to avoid unnecessary rebinding of descriptor sets
	  bool						mBoundSets[DESCRIPTOR_UPDATE_FREQ_COUNT];
	  /// Array of Write descriptor sets per update frequency used to update descriptor set in vkUpdateDescriptorSets
	  VkWriteDescriptorSet*		pWriteSets[DESCRIPTOR_UPDATE_FREQ_COUNT];
	  /// Array of buffer descriptors per update frequency.
	  VkDescriptorBufferInfo*	pBufferInfo[DESCRIPTOR_UPDATE_FREQ_COUNT];
	  /// Array of image descriptors per update frequency
	  VkDescriptorImageInfo*	pImageInfo[DESCRIPTOR_UPDATE_FREQ_COUNT];
	  /// Triple buffered Hash map to check if a descriptor table with a descriptor hash already exists to avoid redundant copy descriptors operations
	  /// 
	  DescriptorSetMap			mStaticDescriptorSetMap[MAX_FRAMES_IN_FLIGHT];
	  /// Triple buffered array of number of descriptor tables allocated per update frequency
	  /// Only used for recording stats
	  uint32_t					mDescriptorSetCount[MAX_FRAMES_IN_FLIGHT][DESCRIPTOR_UPDATE_FREQ_COUNT];

	  Cmd*						pCurrentCmd;
	  uint32_t					mFrameIdx;
  } DescriptorManager;

  static Mutex gDescriptorMutex;

  void add_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature, DescriptorManager** ppManager)
  {
	  DescriptorManager* pManager = (DescriptorManager*)conf_calloc(1, sizeof(*pManager));
	  pManager->pRootSignature = pRootSignature;
	  pManager->mFrameIdx = -1;

	  const uint32_t setCount = DESCRIPTOR_UPDATE_FREQ_COUNT;

	  for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
	  {
		  const DescriptorSetLayout* pLayout = &pRootSignature->pDescriptorSetLayouts[setIndex];
		  const RootDescriptorLayout* pRootLayout = &pRootSignature->pRootDescriptorLayouts[setIndex];
		  const uint32_t descCount = pLayout->mDescriptorCount;

		  // Allocate dynamic offsets array if the descriptor set of this update frequency uses descriptors of type uniform / storage buffer dynamic
		  if (pRootLayout->mRootDescriptorCount)
		  {
			  pManager->pDynamicOffsets[setIndex] = (uint32_t*)conf_calloc(pRootLayout->mRootDescriptorCount, sizeof(uint32_t));
		  }

		  if (descCount)
		  {
			  pManager->pWriteSets[setIndex] = (VkWriteDescriptorSet*)conf_calloc(descCount, sizeof(VkWriteDescriptorSet));
			  if (pLayout->mCumulativeBufferDescriptorCount)
				  pManager->pBufferInfo[setIndex] = (VkDescriptorBufferInfo*)conf_calloc(pLayout->mCumulativeBufferDescriptorCount, sizeof(VkDescriptorBufferInfo));
			  if (pLayout->mCumulativeImageDescriptorCount)
				  pManager->pImageInfo[setIndex] = (VkDescriptorImageInfo*)conf_calloc(pLayout->mCumulativeImageDescriptorCount, sizeof(VkDescriptorImageInfo));

			  // Fill the write descriptors with default values during initialize so the only thing we change in cmdBindDescriptors is the the VkBuffer / VkImageView objects
			  for (uint32_t i = 0; i < descCount; ++i)
			  {
				  const DescriptorInfo* pDesc = &pRootSignature->pDescriptors[pLayout->pDescriptorIndices[i]];

				  pManager->pWriteSets[setIndex][i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				  pManager->pWriteSets[setIndex][i].pNext = NULL;
				  pManager->pWriteSets[setIndex][i].descriptorCount = pDesc->mDesc.size;
				  pManager->pWriteSets[setIndex][i].descriptorType = pDesc->mVkType;
				  pManager->pWriteSets[setIndex][i].dstArrayElement = 0;
				  pManager->pWriteSets[setIndex][i].dstBinding = pDesc->mDesc.reg;

				  if (pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER)
				  {
					  for (uint32_t arr = 0; arr < pDesc->mDesc.size; ++arr)
						  pManager->pImageInfo[setIndex][pDesc->mHandleIndex + arr] = pRenderer->pDefaultSampler->mVkSamplerView;
				  }
				  else if (pDesc->mDesc.type == DESCRIPTOR_TYPE_TEXTURE || pDesc->mDesc.type == DESCRIPTOR_TYPE_RW_TEXTURE)
				  {
					  for (uint32_t arr = 0; arr < pDesc->mDesc.size; ++arr)
						  pManager->pImageInfo[setIndex][pDesc->mHandleIndex + arr] = pRenderer->pDefaultTexture->mVkTextureView;
				  }
				  else
				  {
					  for (uint32_t arr = 0; arr < pDesc->mDesc.size; ++arr)
						  pManager->pBufferInfo[setIndex][pDesc->mHandleIndex + arr] = pRenderer->pDefaultBuffer->mVkBufferInfo;
				  }

				  // Assign the buffer descriptor range associated with this write descriptor set
				  // The offset of the range is the offset of the descriptor from start of the set (mHandleIndex) + the array index
				  if (pLayout->mCumulativeBufferDescriptorCount)
					  pManager->pWriteSets[setIndex][i].pBufferInfo = &pManager->pBufferInfo[setIndex][pDesc->mHandleIndex];
				  if (pLayout->mCumulativeImageDescriptorCount)
					  pManager->pWriteSets[setIndex][i].pImageInfo = &pManager->pImageInfo[setIndex][pDesc->mHandleIndex];
			  }
		  }
	  }

	  *ppManager = pManager;
  }

  void remove_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature, DescriptorManager* pManager)
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
		  const DescriptorSetLayout* pLayout = &pRootSignature->pDescriptorSetLayouts[setIndex];
		  const RootDescriptorLayout* pRootLayout = &pRootSignature->pRootDescriptorLayouts[setIndex];
		  const uint32_t descCount = pLayout->mDescriptorCount;

		  if (pRootLayout->mRootDescriptorCount)
		  {
			  SAFE_FREE(pManager->pDynamicOffsets[setIndex]);
		  }

		  if (descCount)
		  {
			  SAFE_FREE(pManager->pWriteSets[setIndex]);
			  SAFE_FREE(pManager->pBufferInfo[setIndex]);
			  SAFE_FREE(pManager->pImageInfo[setIndex]);
		  }
	  }

	  SAFE_FREE(pManager);
  }

  // This function returns the descriptor manager belonging to this thread
  // If a descriptor manager does not exist for this thread, a new one is created
  // With this approach we make sure that descriptor binding is thread safe and lock conf_free at the same time
  DescriptorManager* get_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature)
  {
	  tinystl::unordered_hash_node<ThreadID, DescriptorManager*>* pNode = pRootSignature->pDescriptorManagerMap.find(Thread::GetCurrentThreadID()).node;
	  if (pNode == NULL)
	  {
		  // Only need a lock when creating a new descriptor manager for this thread
		  MutexLock lock(gDescriptorMutex);
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

  const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName, uint32_t* pIndex)
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

  VkPipelineBindPoint gPipelineBindPoint[PIPELINE_TYPE_COUNT] =
  {
	  VK_PIPELINE_BIND_POINT_MAX_ENUM,
	  VK_PIPELINE_BIND_POINT_COMPUTE,
	  VK_PIPELINE_BIND_POINT_GRAPHICS,
  };

  void cmdBindDescriptors(Cmd* pCmd, RootSignature* pRootSignature, uint32_t numDescriptors, DescriptorData* pDescParams)
  {
	  Renderer* pRenderer = pCmd->pCmdPool->pRenderer;
	  const uint32_t setCount = DESCRIPTOR_UPDATE_FREQ_COUNT;
	  DescriptorManager* pm = get_descriptor_manager(pRenderer, pRootSignature);

	  // Logic to detect beginning of a new frame so we dont run this code everytime user calls cmdBindDescriptors
	  for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
	  {
		  // Reset other data
		  pm->mBoundSets[setIndex] = true;
	  }

	  if (pm->pCurrentCmd != pCmd)
	  {
		  pm->mFrameIdx = (pm->mFrameIdx + 1) % MAX_FRAMES_IN_FLIGHT;
	  }

	  // 64 bit hash value for hashing the mTextureId / mBufferId / mSamplerId of the input descriptors
	  // This value will be later used as look up to find if a descriptor set with the given hash already exists
	  // This way we will call updateDescriptorSet for a particular set of descriptors only once
	  // Then we only need to do a look up into the mDescriptorSetMap with pHash[setIndex] as the key and retrieve the DescriptorSet* value
	  uint64_t* pHash = (uint64_t*)alloca(setCount * sizeof(uint64_t));

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

		  uint32_t descIndex = -1;
		  const DescriptorInfo* pDesc = get_descriptor(pRootSignature, pParam->pName, &descIndex);
		  if (!pDesc)
			  continue;

		  // Find the update frequency of the descriptor
		  // This is also the set index to be used in vkCmdBindDescriptorSets
		  const DescriptorUpdateFrequency setIndex = pDesc->mUpdateFrquency;

		  // If input param is a root constant no need to do any further checks
		  if (pDesc->mDesc.type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
		  {
			  vkCmdPushConstants(pCmd->pVkCmdBuf, pRootSignature->pPipelineLayout, pDesc->mVkStages, 0, pDesc->mDesc.size, pParam->pRootConstant);
			  continue;
		  }

		  // Generate hash of all the resource ids
		  if (pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER)
		  {
			  if (pDesc->mIndexInParent == -1)
			  {
				  LOGERRORF("Trying to bind a static sampler (%s). All static samplers must be bound in addRootSignature through RootSignatureDesc::mStaticSamplers", pParam->pName);
				  continue;
			  }
			  if (!pParam->ppSamplers) {
				  LOGERRORF("Sampler descriptor (%s) is NULL", pParam->pName);
				  return;
			  }
			  for (uint32_t i = 0; i < pParam->mCount; ++i)
			  {
				  if (!pParam->ppSamplers[i]) {
					  LOGERRORF("Sampler descriptor (%s) at array index (%u) is NULL", pParam->pName, i);
					  return;
				  }
				  pHash[setIndex] = tinystl::hash_state(&pParam->ppSamplers[i]->mSamplerId, 1, pHash[setIndex]);
				  pm->pImageInfo[setIndex][pDesc->mHandleIndex + i] = pParam->ppSamplers[i]->mVkSamplerView;
			  }
		  }
		  else if (pDesc->mDesc.type == DESCRIPTOR_TYPE_TEXTURE || pDesc->mDesc.type == DESCRIPTOR_TYPE_RW_TEXTURE)
		  {
			  if (!pParam->ppTextures) {
				  LOGERRORF("Texture descriptor (%s) is NULL", pParam->pName);
				  return;
			  }
			  for (uint32_t i = 0; i < pParam->mCount; ++i)
			  {
				  if (!pParam->ppTextures[i]) {
					  LOGERRORF("Texture descriptor (%s) at array index (%u) is NULL", pParam->pName, i);
					  return;
				  }

				  pHash[setIndex] = tinystl::hash_state(&pParam->ppTextures[i]->mTextureId, 1, pHash[setIndex]);

				  // Store the new descriptor so we can use it in vkUpdateDescriptorSet later
				  pm->pImageInfo[setIndex][pDesc->mHandleIndex + i] = pParam->ppTextures[i]->mVkTextureView;
				  // SRVs need to be in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL and UAVs need to be in VK_IMAGE_LAYOUT_GENERAL
				  if (pDesc->mDesc.type == DESCRIPTOR_TYPE_TEXTURE)
					  pm->pImageInfo[setIndex][pDesc->mHandleIndex + i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				  else
					  pm->pImageInfo[setIndex][pDesc->mHandleIndex + i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			  }
		  }
		  else
		  {
			  if (!pParam->ppBuffers) {
				  LOGERRORF("Buffer descriptor (%s) is NULL", pParam->pName);
				  return;
			  }
			  for (uint32_t i = 0; i < pParam->mCount; ++i)
			  {
				  if (!pParam->ppBuffers[i]) {
					  LOGERRORF("Buffer descriptor (%s) at array index (%u) is NULL", pParam->pName, i);
					  return;
				  }
				  pHash[setIndex] = tinystl::hash_state(&pParam->ppBuffers[i]->mBufferId, 1, pHash[setIndex]);

				  // Store the new descriptor so we can use it in vkUpdateDescriptorSet later
				  pm->pBufferInfo[setIndex][pDesc->mHandleIndex + i] = pParam->ppBuffers[i]->mVkBufferInfo;
				  // Only store the offset provided in pParam if the descriptor is not dynamic
				  // For dynamic descriptors the offsets are bound in vkCmdBindDescriptorSets
				  if (pDesc->mVkType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
					pm->pBufferInfo[setIndex][pDesc->mHandleIndex + i].offset = pParam->mOffset;
			  }

			  if (pDesc->mDesc.type == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			  {
				  // If descriptor is of type uniform buffer dynamic, we dont need to hash the offset
				  // Dynamic uniform buffer descriptors using the same VkBuffer object can be bound at different offsets without the need for vkUpdateDescriptorSets
				  if (pDesc->mVkType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
				  {
					  pm->pDynamicOffsets[setIndex][pDesc->mDynamicUniformIndex] = (uint32_t)pParam->mOffset;
				  }
				  // If descriptor is not of type uniform buffer dynamic, hash the offset value
				  // Non dynamic uniform buffer descriptors using the same VkBuffer object but different offset values are considered as different descriptors
				  else
				  {
					  pHash[setIndex] = tinystl::hash_state(&pParam->mOffset, 1, pHash[setIndex]);
				  }
			  }
		  }

		  // Unbind current descriptor set so we can bind a new one
		  pm->mBoundSets[setIndex] = false;
	  }

	  for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
	  {
		  const DescriptorSetLayout* pLayout = &pRootSignature->pDescriptorSetLayouts[setIndex];
		  uint32_t descCount = pRootSignature->pDescriptorSetLayouts[setIndex].mDescriptorCount;
		  uint32_t rootDescCount = pRootSignature->pRootDescriptorLayouts[setIndex].mRootDescriptorCount;

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
					  consume_descriptor_sets(pRenderer, &pLayout->pVkSetLayout, pSets, 1, pRenderer->pDescriptorPool);
					  for (uint32_t i = 0; i < descCount; ++i)
						  pm->pWriteSets[setIndex][i].dstSet = pDescriptorSet;

					  vkUpdateDescriptorSets(pRenderer->pDevice, descCount, pm->pWriteSets[setIndex], 0, NULL);
					  ++pm->mDescriptorSetCount[pm->mFrameIdx][setIndex];
					  pm->mStaticDescriptorSetMap[pm->mFrameIdx].insert({ pHash[setIndex], pDescriptorSet });
				  }
			  }
			  // Dynamic descriptors
			  else
			  {
				  if (!pCmd->pDescriptorPool)
					  add_descriptor_heap(pRenderer, gDefaultDynamicDescriptorSets, 0, gDynamicDescriptorHeapPoolSizes, VK_DESCRIPTOR_TYPE_RANGE_SIZE, &pCmd->pDescriptorPool);

				  VkDescriptorSet* pSets[] = { &pDescriptorSet };
				  consume_descriptor_sets_lock_free(pRenderer, &pLayout->pVkSetLayout, pSets, 1, pCmd->pDescriptorPool);
				  for (uint32_t i = 0; i < descCount; ++i)
					  pm->pWriteSets[setIndex][i].dstSet = pDescriptorSet;

				  vkUpdateDescriptorSets(pRenderer->pDevice, descCount, pm->pWriteSets[setIndex], 0, NULL);
				  ++pm->mDescriptorSetCount[pm->mFrameIdx][setIndex];
			  }

			  vkCmdBindDescriptorSets(pCmd->pVkCmdBuf, gPipelineBindPoint[pRootSignature->mPipelineType], pRootSignature->pPipelineLayout,
				  setIndex, 1,
				  &pDescriptorSet,
				  rootDescCount, pm->pDynamicOffsets[setIndex]);

			  // Set the bound flag for the descriptor set of this update frequency
			  // This way in the future if user tries to bind the same descriptor set, we can avoid unnecessary rebinds
			  pm->mBoundSets[setIndex] = true;
		  }
	  }
  }
  /************************************************************************/
  // Render Pass Implementation
  /************************************************************************/
  typedef struct RenderPassDesc
  {
	  ImageFormat::Enum*	pColorFormats;
	  bool*					pSrgbValues;
	  uint32_t				mRenderTargetCount;
	  SampleCount			mSampleCount;
	  ImageFormat::Enum		mDepthStencilFormat;
  } RenderPassDesc;

  typedef struct RenderPass
  {
	  VkRenderPass		pRenderPass;
	  RenderPassDesc	mDesc;
  } RenderPass;

  typedef struct FrameBufferDesc
  {
	  RenderPass*		pRenderPass;
	  RenderTarget**	ppRenderTargets;
	  RenderTarget*		pDepthStencil;
	  uint32_t			mRenderTargetCount;
  } FrameBufferDesc;

  typedef struct FrameBuffer
  {
	  VkFramebuffer		pFramebuffer;
	  uint32_t			mWidth;
	  uint32_t			mHeight;
	  uint32_t			mArraySize;
  } FrameBuffer;

  void addRenderPass(Renderer* pRenderer, const RenderPassDesc* pDesc, RenderPass** ppRenderPass)
  {
	  RenderPass* pRenderPass = (RenderPass*)conf_calloc(1, sizeof(*pRenderPass));
	  pRenderPass->mDesc = *pDesc;
	  /************************************************************************/
	  // Add render pass
	  /************************************************************************/
	  ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);

	  uint32_t colorAttachmentCount = pDesc->mRenderTargetCount;
	  uint32_t depthAttachmentCount = (pDesc->mDepthStencilFormat != ImageFormat::None) ? 1 : 0;

	  VkAttachmentDescription* attachments = NULL;
	  VkAttachmentReference* color_attachment_refs = NULL;
	  VkAttachmentReference* depth_stencil_attachment_ref = NULL;

	  VkSampleCountFlagBits sample_count = util_to_vk_sample_count(pDesc->mSampleCount);

	  // Fill out attachment descriptions and references
	  {
		  attachments = (VkAttachmentDescription*)conf_calloc(colorAttachmentCount + depthAttachmentCount, sizeof(*attachments));
		  ASSERT(attachments);

		  if (colorAttachmentCount > 0) {
			  color_attachment_refs = (VkAttachmentReference*)conf_calloc(colorAttachmentCount, sizeof(*color_attachment_refs));
			  ASSERT(color_attachment_refs);
		  }
		  if (depthAttachmentCount > 0) {
			  depth_stencil_attachment_ref = (VkAttachmentReference*)conf_calloc(1, sizeof(*depth_stencil_attachment_ref));
			  ASSERT(depth_stencil_attachment_ref);
		  }

		  // Color
		  for (uint32_t i = 0; i < colorAttachmentCount; ++i) {
			  const uint32_t ssidx = i;

			  // descriptions
			  attachments[ssidx].flags = 0;
			  attachments[ssidx].format = util_to_vk_image_format(pDesc->pColorFormats[i], pDesc->pSrgbValues[i]);
			  attachments[ssidx].samples = sample_count;
			  attachments[ssidx].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			  attachments[ssidx].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			  attachments[ssidx].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			  attachments[ssidx].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			  attachments[ssidx].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			  attachments[ssidx].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			  // references
			  color_attachment_refs[i].attachment = ssidx;
			  color_attachment_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		  }
	  }

	  // Depth stencil
	  if (depthAttachmentCount > 0) {
		  uint32_t idx = colorAttachmentCount;
		  attachments[idx].flags = 0;
		  attachments[idx].format = util_to_vk_image_format(pDesc->mDepthStencilFormat, false);
		  attachments[idx].samples = sample_count;
		  attachments[idx].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		  attachments[idx].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		  attachments[idx].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
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

	  VkResult vk_res = vkCreateRenderPass(pRenderer->pDevice, &create_info, NULL, &(pRenderPass->pRenderPass));
	  ASSERT(VK_SUCCESS == vk_res);

	  SAFE_FREE(attachments);
	  SAFE_FREE(color_attachment_refs);
	  SAFE_FREE(depth_stencil_attachment_ref);

	  *ppRenderPass = pRenderPass;
  }

  void removeRenderPass(Renderer* pRenderer, RenderPass* pRenderPass)
  {
	  vkDestroyRenderPass(pRenderer->pDevice, pRenderPass->pRenderPass, NULL);
	  SAFE_FREE(pRenderPass);
  }

  void addFrameBuffer(Renderer* pRenderer, const FrameBufferDesc* pDesc, FrameBuffer** ppFrameBuffer)
  {
	  ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);

	  FrameBuffer* pFrameBuffer = (FrameBuffer*)conf_calloc(1, sizeof(*pFrameBuffer));
	  ASSERT(pFrameBuffer);

	  uint32_t colorAttachmentCount = pDesc->mRenderTargetCount;
	  uint32_t depthAttachmentCount = (pDesc->pDepthStencil) ? 1 : 0;

	  if (colorAttachmentCount)
	  {
		  pFrameBuffer->mWidth = pDesc->ppRenderTargets[0]->mDesc.mWidth;
		  pFrameBuffer->mHeight = pDesc->ppRenderTargets[0]->mDesc.mHeight;
		  pFrameBuffer->mArraySize = pDesc->ppRenderTargets[0]->mDesc.mArraySize;
	  }
	  else
	  {
		  pFrameBuffer->mWidth = pDesc->pDepthStencil->mDesc.mWidth;
		  pFrameBuffer->mHeight = pDesc->pDepthStencil->mDesc.mHeight;
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
	  for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i) {
		  *iter_attachments = pDesc->ppRenderTargets[i]->pTexture->pVkImageView;
		  ++iter_attachments;
	  }
	  // Depth/stencil
	  if (pDesc->pDepthStencil) {
		  *iter_attachments = pDesc->pDepthStencil->pTexture->pVkImageView;
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
	  VkResult vk_res = vkCreateFramebuffer(pRenderer->pDevice, &add_info, NULL, &(pFrameBuffer->pFramebuffer));
	  ASSERT(VK_SUCCESS == vk_res);
	  SAFE_FREE(pImageViews);
	  /************************************************************************/
	  /************************************************************************/

	  *ppFrameBuffer = pFrameBuffer;
  }

  void removeFrameBuffer(Renderer* pRenderer, FrameBuffer* pFrameBuffer)
  {
	  ASSERT(pRenderer);
	  ASSERT(pFrameBuffer);

	  vkDestroyFramebuffer(pRenderer->pDevice, pFrameBuffer->pFramebuffer, NULL);
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
  tinystl::unordered_map<ThreadID, RenderPassMap >	mRenderPassMap;
  // FrameBuffer map per thread (this will make lookups lock free and we only need a lock when inserting a FrameBuffer map for the first time)
  tinystl::unordered_map<ThreadID, FrameBufferMap >	mFrameBufferMap;
  Mutex												gRenderPassMutex;

  RenderPassMap& get_render_pass_map()
  {
	  tinystl::unordered_hash_node<ThreadID, RenderPassMap>* pNode = mRenderPassMap.find(Thread::GetCurrentThreadID()).node;
	  if (pNode == NULL)
	  {
		  // Only need a lock when creating a new renderpass map for this thread
		  MutexLock lock(gRenderPassMutex);
		  return mRenderPassMap.insert({ Thread::GetCurrentThreadID(),{} }).first->second;
	  }
	  else
	  {
		  return pNode->second;
	  }
  }

  FrameBufferMap& get_frame_buffer_map()
  {
	  tinystl::unordered_hash_node<ThreadID, FrameBufferMap>* pNode = mFrameBufferMap.find(Thread::GetCurrentThreadID()).node;
	  if (pNode == NULL)
	  {
		  // Only need a lock when creating a new framebuffer map for this thread
		  MutexLock lock(gRenderPassMutex);
		  return mFrameBufferMap.insert({ Thread::GetCurrentThreadID(),{} }).first->second;
	  }
	  else
	  {
		  return pNode->second;
	  }
  }
  /************************************************************************/
  // Query Heap Implementation
  /************************************************************************/
  VkQueryType util_to_vk_query_type(QueryType type)
  {
	  switch (type)
	  {
	  case QUERY_TYPE_TIMESTAMP:
		  return VK_QUERY_TYPE_TIMESTAMP;
	  case QUERY_TYPE_PIPELINE_STATISTICS:
		  return VK_QUERY_TYPE_PIPELINE_STATISTICS;
	  case QUERY_TYPE_OCCLUSION:
		  return VK_QUERY_TYPE_OCCLUSION;
	  default:
		  ASSERT(false && "Invalid query heap type");
		  return VK_QUERY_TYPE_MAX_ENUM;
	  }
  }

  void getTimestampFrequency(Queue* pQueue, double* pFrequency)
  {
	  ASSERT(pQueue);
	  ASSERT(pFrequency);

	  *pFrequency = (double)pQueue->pRenderer->pVkActiveGPUProperties->limits.timestampPeriod;
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
	  vkCreateQueryPool(pRenderer->pDevice, &createInfo, NULL, &pQueryHeap->pVkQueryPool);

	  *ppQueryHeap = pQueryHeap;
  }

  void removeQueryHeap(Renderer* pRenderer, QueryHeap* pQueryHeap)
  {
	  vkDestroyQueryPool(pRenderer->pDevice, pQueryHeap->pVkQueryPool, NULL);
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
	  case QUERY_TYPE_PIPELINE_STATISTICS:
		  break;
	  case QUERY_TYPE_OCCLUSION:
		  break;
	  case QUERY_TYPE_COUNT:
		  break;
	  default:
		  break;
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
	  case QUERY_TYPE_PIPELINE_STATISTICS:
		  break;
	  case QUERY_TYPE_OCCLUSION:
		  break;
	  case QUERY_TYPE_COUNT:
		  break;
	  default:
		  break;
	  }
  }

  void cmdResolveQuery(Cmd* pCmd, QueryHeap* pQueryHeap, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
  {
	  vkCmdCopyQueryPoolResults(pCmd->pVkCmdBuf, pQueryHeap->pVkQueryPool, startQuery, queryCount, pReadbackBuffer->pVkBuffer, 0, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  }
  /************************************************************************/
  // Logging, Validation layer implementation
  /************************************************************************/
	// Proxy log callback
	static void internal_log(LogType type, const char* msg, const char* component)
	{
		switch (type)
		{
		case LOG_TYPE_INFO:
			LOGINFOF("%s ( %s )", component, msg);
			break;
		case LOG_TYPE_WARN:
			LOGWARNINGF("%s ( %s )", component, msg);
			break;
		case LOG_TYPE_DEBUG:
			LOGDEBUGF("%s ( %s )", component, msg);
			break;
		case LOG_TYPE_ERROR:
			LOGERRORF("%s ( %s )", component, msg);
			break;
		default:
			break;
		}
	}

	// Proxy debug callback for Vulkan layers
	static VKAPI_ATTR VkBool32 VKAPI_CALL internal_debug_report_callback(
		VkDebugReportFlagsEXT      flags,
		VkDebugReportObjectTypeEXT objectType,
		uint64_t                   object,
		size_t                     location,
		int32_t                    messageCode,
		const char*                pLayerPrefix,
		const char*                pMessage,
		void*                      pUserData
	)
	{
		if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
			LOGINFOF("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
		}
		else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
			// Vulkan SDK 1.0.68 fixes the Dedicated memory binding validation error bugs
#if VK_HEADER_VERSION < 68
			// Disable warnings for bind memory for dedicated allocations extension
			if(gDedicatedAllocationExtension && messageCode != 11 && messageCode != 12)
#endif
				LOGWARNINGF("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
		}
		else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
			LOGWARNINGF("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
		}
		else if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
			LOGERRORF("[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
		}

		return VK_FALSE;
	}
	/************************************************************************/
	// Create default resources to be used a null descriptors in case user does not specify some descriptors
	/************************************************************************/
	void create_default_resources(Renderer* pRenderer)
	{
		TextureDesc textureDesc = {};
		textureDesc.mArraySize = 1;
		textureDesc.mDepth = 1;
		textureDesc.mFormat = ImageFormat::R8;
		textureDesc.mHeight = 2;
		textureDesc.mMipLevels = 1;
		textureDesc.mSampleCount = SAMPLE_COUNT_1;
		textureDesc.mStartState = RESOURCE_STATE_COMMON;
		textureDesc.mType = TEXTURE_TYPE_2D;
		textureDesc.mUsage = TEXTURE_USAGE_SAMPLED_IMAGE | TEXTURE_USAGE_UNORDERED_ACCESS;
		textureDesc.mWidth = 2;
		addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTexture);

		BufferDesc bufferDesc = {};
		bufferDesc.mUsage = BUFFER_USAGE_STORAGE_SRV | BUFFER_USAGE_STORAGE_UAV | BUFFER_USAGE_UNIFORM;
		bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufferDesc.mStartState = RESOURCE_STATE_COMMON;
		bufferDesc.mSize = sizeof(uint32_t);
		bufferDesc.mFirstElement = 0;
		bufferDesc.mElementCount = 1;
		bufferDesc.mStructStride = sizeof(uint32_t);
		addBuffer(pRenderer, &bufferDesc, &pRenderer->pDefaultBuffer);

		addSampler(pRenderer, &pRenderer->pDefaultSampler);

		addBlendState(&pRenderer->pDefaultBlendState, BC_ONE, BC_ZERO, BC_ONE, BC_ZERO);
		addDepthState(pRenderer, &pRenderer->pDefaultDepthState, false, true);
		addRasterizerState(&pRenderer->pDefaultRasterizerState, CullMode::CULL_MODE_BACK);
	}

	void destroy_default_resources(Renderer* pRenderer)
	{
		removeTexture(pRenderer, pRenderer->pDefaultTexture);
		removeBuffer(pRenderer, pRenderer->pDefaultBuffer);
		removeSampler(pRenderer, pRenderer->pDefaultSampler);

		removeBlendState(pRenderer->pDefaultBlendState);
		removeDepthState(pRenderer->pDefaultDepthState);
		removeRasterizerState(pRenderer->pDefaultRasterizerState);
	}

	ImageFormat::Enum getRecommendedSwapchainFormat(bool hintHDR)
	{
		return ImageFormat::BGRA8;
	}
	/************************************************************************/
	// Globals
	/************************************************************************/
	static volatile uint64_t gBufferIds = 0;
	static volatile uint64_t gTextureIds = 0;
	static volatile uint64_t gSamplerIds = 0;
	// -------------------------------------------------------------------------------------------------
	// API functions
	// -------------------------------------------------------------------------------------------------
	void initRenderer(const char* app_name, const RendererDesc * settings, Renderer** ppRenderer)
	{
		Renderer* pRenderer = (Renderer*)conf_calloc (1, sizeof (*pRenderer));
		ASSERT(pRenderer);

		pRenderer->pName = (char*)conf_calloc(strlen(app_name) + 1, sizeof(char));
		memcpy(pRenderer->pName, app_name, strlen(app_name));

		// Copy settings
		memcpy (&(pRenderer->mSettings), settings, sizeof (*settings));

		// Initialize the Vulkan internal bits
		{
#if defined(_DEBUG)
			// this turns on all validation layers
			pRenderer->mInstanceLayers.push_back("VK_LAYER_LUNARG_standard_validation");
			// this turns on render doc layer for gpu capture
#ifdef USE_RENDER_DOC
			pRenderer->mInstanceLayers.push_back("VK_LAYER_RENDERDOC_Capture");
#endif
#endif

			VkResult vkRes = volkInitialize();
			if (vkRes != VK_SUCCESS)
			{
				LOGERROR("Failed to initialize Vulkan");
				return;
			}

			CreateInstance(app_name, pRenderer);
			AddDevice(pRenderer);

			VmaAllocatorCreateInfo createInfo = { 0 };
			createInfo.device = pRenderer->pDevice;
			createInfo.physicalDevice = pRenderer->pActiveGPU;

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

			createInfo.pVulkanFunctions = &vulkanFunctions;

			vmaCreateAllocator(&createInfo, &pRenderer->pVmaAllocator);

			add_descriptor_heap(pRenderer, gDefaultDescriptorSets, 0, gDescriptorHeapPoolSizes, VK_DESCRIPTOR_TYPE_RANGE_SIZE, &pRenderer->pDescriptorPool);
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
		for (tinystl::unordered_hash_node<ThreadID, RenderPassMap>& t : mRenderPassMap)
			for (RenderPassMapNode& it : t.second)
				removeRenderPass(pRenderer, it.second);

		for (tinystl::unordered_hash_node<ThreadID, FrameBufferMap>& t : mFrameBufferMap)
			for (FrameBufferMapNode& it : t.second)
				removeFrameBuffer(pRenderer, it.second);

		// Destroy the Vulkan bits
		remove_descriptor_heap(pRenderer, pRenderer->pDescriptorPool);
		vmaDestroyAllocator(pRenderer->pVmaAllocator);

		RemoveDevice(pRenderer);
		RemoveInstance(pRenderer);

		pRenderer->mInstanceLayers.~vector();

		// Free all the renderer components!
		SAFE_FREE(pRenderer);
	}

	void addFence(Renderer* pRenderer, Fence** ppFence, uint64 mFenceValue)
	{
		ASSERT(pRenderer);
		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);

		Fence* pFence = (Fence*)conf_calloc(1, sizeof(*pFence));
		ASSERT(pFence);

		DECLARE_ZERO(VkFenceCreateInfo, add_info);
		add_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		add_info.pNext = NULL;
		add_info.flags = 0;
		VkResult vk_res = vkCreateFence(pRenderer->pDevice, &add_info, NULL, &(pFence->pVkFence));
		ASSERT(VK_SUCCESS == vk_res);

		pFence->pRenderer = pRenderer;
		pFence->mSubmitted = false;

		*ppFence = pFence;
	}

	void removeFence(Renderer *pRenderer, Fence* pFence)
	{
		ASSERT(pRenderer);
		ASSERT(pFence);
		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);
		ASSERT(VK_NULL_HANDLE != pFence->pVkFence);

		vkDestroyFence(pRenderer->pDevice, pFence->pVkFence, NULL);

		SAFE_FREE(pFence);
	}

	void addSemaphore(Renderer *pRenderer, Semaphore** ppSemaphore)
	{
		ASSERT(pRenderer);

		Semaphore* pSemaphore = (Semaphore*)conf_calloc(1, sizeof(*pSemaphore));
		ASSERT(pSemaphore);

		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);

		DECLARE_ZERO(VkSemaphoreCreateInfo, add_info);
		add_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		add_info.pNext = NULL;
		add_info.flags = 0;
		VkResult vk_res = vkCreateSemaphore(pRenderer->pDevice, &add_info, NULL, &(pSemaphore->pVkSemaphore));
		ASSERT(VK_SUCCESS == vk_res);

		*ppSemaphore = pSemaphore;
	}

	void removeSemaphore(Renderer *pRenderer, Semaphore* pSemaphore)
	{
		ASSERT(pRenderer);
		ASSERT(pSemaphore);
		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);
		ASSERT(VK_NULL_HANDLE != pSemaphore->pVkSemaphore);

		vkDestroySemaphore(pRenderer->pDevice, pSemaphore->pVkSemaphore, NULL);

		SAFE_FREE(pSemaphore);
	}

	VkDescriptorType util_to_vk_descriptor_type(DescriptorType type)
	{
		switch (type)
		{
		case DESCRIPTOR_TYPE_UNDEFINED:
			ASSERT("Invalid DescriptorInfo Type");
			return VK_DESCRIPTOR_TYPE_MAX_ENUM;
		case DESCRIPTOR_TYPE_SAMPLER:
			return VK_DESCRIPTOR_TYPE_SAMPLER;
		case DESCRIPTOR_TYPE_TEXTURE:
			return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case DESCRIPTOR_TYPE_RW_TEXTURE:
			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		case DESCRIPTOR_TYPE_BUFFER:
		case DESCRIPTOR_TYPE_RW_BUFFER:
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
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

	void addQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
	{
		ASSERT(pDesc != NULL);

		uint32_t queueFamilyIndex = -1;
		VkQueueFlags requiredFlags = util_to_vk_queue_flags(pDesc->mType);
		uint32_t queueIndex = -1;
		bool found = false;

		// Try to find a dedicated queue of this type
		for (uint32_t index = 0; index < pRenderer->mVkActiveQueueFamilyPropertyCount; ++index) {
			VkQueueFlags queueFlags = pRenderer->pVkActiveQueueFamilyProperties[index].queueFlags;
			if ((queueFlags & requiredFlags) &&
				((queueFlags & ~requiredFlags) == 0) &&
				pRenderer->mUsedQueueCount[pRenderer->mActiveGPUIndex][queueFlags] < pRenderer->pVkActiveQueueFamilyProperties[index].queueCount) {
				found = true;
				queueFamilyIndex = index;
				queueIndex = pRenderer->mUsedQueueCount[pRenderer->mActiveGPUIndex][queueFlags];
				break;
			}
		}

		// If hardware doesn't provide a dedicated queue try to find a non-dedicated one
		if (!found)
		{
			for (uint32_t index = 0; index < pRenderer->mVkActiveQueueFamilyPropertyCount; ++index) {
				VkQueueFlags queueFlags = pRenderer->pVkActiveQueueFamilyProperties[index].queueFlags;
				if ((queueFlags & requiredFlags) &&
					pRenderer->mUsedQueueCount[pRenderer->mActiveGPUIndex][queueFlags] < pRenderer->pVkActiveQueueFamilyProperties[index].queueCount) {
					found = true;
					queueFamilyIndex = index;
					queueIndex = pRenderer->mUsedQueueCount[pRenderer->mActiveGPUIndex][queueFlags];
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
			VkQueueFlags queueFlags = pRenderer->pVkActiveQueueFamilyProperties[queueFamilyIndex].queueFlags;
			Queue* pQueueToCreate = (Queue*)conf_calloc(1, sizeof(*pQueueToCreate));
			pQueueToCreate->mVkQueueFamilyIndex = queueFamilyIndex;
			pQueueToCreate->pRenderer = pRenderer;
			pQueueToCreate->mQueueDesc = *pDesc;
			pQueueToCreate->mVkQueueIndex = queueIndex;
			//get queue handle
			vkGetDeviceQueue(pRenderer->pDevice, pQueueToCreate->mVkQueueFamilyIndex, pQueueToCreate->mVkQueueIndex, &(pQueueToCreate->pVkQueue));
			ASSERT(VK_NULL_HANDLE != pQueueToCreate->pVkQueue);
			*ppQueue = pQueueToCreate;

			++pRenderer->mUsedQueueCount[pRenderer->mActiveGPUIndex][queueFlags];
		}
		else
		{
			LOGERRORF("Cannot create queue of type (%u)", pDesc->mType);
		}
	}
	
	void removeQueue(Queue* pQueue)
	{
		ASSERT(pQueue != NULL);
		VkQueueFlags queueFlags = pQueue->pRenderer->pVkActiveQueueFamilyProperties[pQueue->mVkQueueFamilyIndex].queueFlags;
		--pQueue->pRenderer->mUsedQueueCount[pQueue->pRenderer->mActiveGPUIndex][queueFlags];
		SAFE_FREE(pQueue);
	}

	void addCmdPool(Renderer *pRenderer, Queue* pQueue, bool transient, CmdPool** ppCmdPool, CmdPoolDesc * pCmdPoolDesc)
	{
		ASSERT(pRenderer);
		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);

		CmdPool* pCmdPool = (CmdPool*)conf_calloc(1, sizeof(*pCmdPool));
		ASSERT(pCmdPool);

		if (pCmdPoolDesc == NULL)
		{
			pCmdPool->mCmdPoolDesc = { pQueue->mQueueDesc.mType };
		}
		else
		{
			pCmdPool->mCmdPoolDesc = *pCmdPoolDesc;
		}

		pCmdPool->pRenderer = pRenderer;

		DECLARE_ZERO(VkCommandPoolCreateInfo, add_info);
		add_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		add_info.pNext = NULL;
		add_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		add_info.queueFamilyIndex = pQueue->mVkQueueFamilyIndex;
		if (transient) {
			add_info.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		}
		VkResult vk_res = vkCreateCommandPool(pRenderer->pDevice, &add_info, NULL, &(pCmdPool->pVkCmdPool));
		ASSERT(VK_SUCCESS == vk_res);

		*ppCmdPool = pCmdPool;
	}

	void removeCmdPool(Renderer *pRenderer, CmdPool* pCmdPool)
	{
		ASSERT(pRenderer);
		ASSERT(pCmdPool);
		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);
		ASSERT(VK_NULL_HANDLE != pCmdPool->pVkCmdPool);

		vkDestroyCommandPool(pRenderer->pDevice, pCmdPool->pVkCmdPool, NULL);

		SAFE_FREE(pCmdPool);
	}

	void addCmd(CmdPool* pCmdPool, bool secondary, Cmd** ppCmd)
	{
		ASSERT(pCmdPool);
		ASSERT(VK_NULL_HANDLE != pCmdPool->pRenderer->pDevice);
		ASSERT(VK_NULL_HANDLE != pCmdPool->pVkCmdPool);

		Cmd* pCmd = (Cmd*)conf_calloc(1, sizeof(*pCmd));
		ASSERT(pCmd);

		pCmd->pCmdPool = pCmdPool;

		DECLARE_ZERO(VkCommandBufferAllocateInfo, alloc_info);
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.commandPool = pCmdPool->pVkCmdPool;
		alloc_info.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		VkResult vk_res = vkAllocateCommandBuffers(pCmdPool->pRenderer->pDevice, &alloc_info, &(pCmd->pVkCmdBuf));
		ASSERT(VK_SUCCESS == vk_res);

		*ppCmd = pCmd;
	}

	void removeCmd(CmdPool* pCmdPool, Cmd* pCmd)
	{
		ASSERT(pCmdPool);
		ASSERT(pCmd);
		ASSERT(VK_NULL_HANDLE != pCmdPool->pRenderer->pDevice);
		ASSERT(VK_NULL_HANDLE != pCmdPool->pVkCmdPool);
		ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

		if (pCmd->pDescriptorPool)
			remove_descriptor_heap(pCmdPool->pRenderer, pCmd->pDescriptorPool);

		vkFreeCommandBuffers(pCmdPool->pRenderer->pDevice, pCmdPool->pVkCmdPool, 1, &(pCmd->pVkCmdBuf));

		SAFE_FREE(pCmd);
	}

	void addCmd_n(CmdPool *pCmdPool, bool secondary, uint32_t cmdCount, Cmd*** pppCmd)
	{
		ASSERT(pppCmd);

		Cmd** ppCmd = (Cmd**)conf_calloc(cmdCount, sizeof(*ppCmd));
		ASSERT(ppCmd);

		for (uint32_t i = 0; i < cmdCount; ++i) {
			addCmd(pCmdPool, secondary, &(ppCmd[i]));
		}

		*pppCmd = ppCmd;
	}

	void removeCmd_n(CmdPool *pCmdPool, uint32_t cmdCount, Cmd** ppCmd)
	{
		ASSERT(ppCmd);

		for (uint32_t i = 0; i < cmdCount; ++i) {
			removeCmd(pCmdPool, ppCmd[i]);
		}

		SAFE_FREE(ppCmd);
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
		ASSERT(VK_NULL_HANDLE != pRenderer->pVKInstance);
		DECLARE_ZERO(VkWin32SurfaceCreateInfoKHR, add_info);
		add_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		add_info.pNext = NULL;
		add_info.flags = 0;
		add_info.hinstance = ::GetModuleHandle(NULL);
		add_info.hwnd = (HWND)pDesc->pWindow->handle;
		VkResult vk_res = vkCreateWin32SurfaceKHR(pRenderer->pVKInstance, &add_info, NULL, &pSwapChain->pVkSurface);
		ASSERT(VK_SUCCESS == vk_res);
		/************************************************************************/
		// Create swap chain
		/************************************************************************/
		ASSERT(VK_NULL_HANDLE != pRenderer->pActiveGPU);

		// Most GPUs will not go beyond VK_SAMPLE_COUNT_8_BIT
		ASSERT(0 != (pRenderer->pVkActiveGPUProperties->limits.framebufferColorSampleCounts & pSwapChain->mDesc.mSampleCount));

		// Image count
		if (0 == pSwapChain->mDesc.mImageCount) {
			pSwapChain->mDesc.mImageCount = 2;
		}

		DECLARE_ZERO(VkSurfaceCapabilitiesKHR, caps);
		vk_res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pRenderer->pActiveGPU, pSwapChain->pVkSurface, &caps);
		ASSERT(VK_SUCCESS == vk_res);

		if ((caps.maxImageCount > 0) && (pSwapChain->mDesc.mImageCount > caps.maxImageCount)) {
			pSwapChain->mDesc.mImageCount = caps.maxImageCount;
		}

		// Surface format
		DECLARE_ZERO(VkSurfaceFormatKHR, surface_format);
		surface_format.format = VK_FORMAT_UNDEFINED;
		uint32_t surfaceFormatCount = 0;
		VkSurfaceFormatKHR* formats = NULL;

		// Get surface formats count
		vk_res = vkGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->pActiveGPU, pSwapChain->pVkSurface, &surfaceFormatCount, NULL);
		ASSERT(VK_SUCCESS == vk_res);

		// Allocate and get surface formats
		formats = (VkSurfaceFormatKHR*)conf_calloc(surfaceFormatCount, sizeof(*formats));
		vk_res = vkGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->pActiveGPU, pSwapChain->pVkSurface, &surfaceFormatCount, formats);
		ASSERT(VK_SUCCESS == vk_res);

		if ((1 == surfaceFormatCount) && (VK_FORMAT_UNDEFINED == formats[0].format)) {
			surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
			surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		}
		else {
			VkFormat requested_format = util_to_vk_image_format(pSwapChain->mDesc.mColorFormat, pSwapChain->mDesc.mSrgb);
			for (uint32_t i = 0; i < surfaceFormatCount; ++i) {
				if ((requested_format == formats[i].format) && (VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == formats[i].colorSpace)) {
					surface_format.format = requested_format;
					surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
					break;
				}
			}

			// Default to VK_FORMAT_B8G8R8A8_UNORM if requested format isn't found
			if (VK_FORMAT_UNDEFINED == surface_format.format) {
				surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
				surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			}
		}

		ASSERT(VK_FORMAT_UNDEFINED != surface_format.format);

		// Free formats
		SAFE_FREE(formats);

		// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
		// This mode waits for the vertical blank ("v-sync")
		VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
		uint32_t swapChainImageCount = 0;
		VkPresentModeKHR* modes = NULL;
		// Get present mode count
		vk_res = vkGetPhysicalDeviceSurfacePresentModesKHR(pRenderer->pActiveGPU, pSwapChain->pVkSurface, &swapChainImageCount, NULL);
		ASSERT(VK_SUCCESS == vk_res);

		// Allocate and get present modes
		modes = (VkPresentModeKHR*)conf_calloc(swapChainImageCount, sizeof(*modes));
		vk_res = vkGetPhysicalDeviceSurfacePresentModesKHR(pRenderer->pActiveGPU, pSwapChain->pVkSurface, &swapChainImageCount, modes);
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
		uint32_t queue_family_index_count = 0;
		uint32_t queue_family_indices[2] = { pDesc->pQueue->mVkQueueFamilyIndex, 0 };
		uint32_t presentQueueFamilyIndex = -1;

		// Check if hardware provides dedicated present queue
		if (0 != pRenderer->mVkActiveQueueFamilyPropertyCount)
		{
			for (uint32_t index = 0; index < pRenderer->mVkActiveQueueFamilyPropertyCount; ++index) {
				VkBool32 supports_present = VK_FALSE;
				VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(pRenderer->pActiveGPU, index, pSwapChain->pVkSurface, &supports_present);
				if ((VK_SUCCESS == res) && (VK_TRUE == supports_present) &&
					pSwapChain->mDesc.pQueue->mVkQueueFamilyIndex != index) {
					presentQueueFamilyIndex = index;
					break;
				}
			}

			// If there is no dedicated present queue, just find the first available queue which supports present
			if (presentQueueFamilyIndex == -1)
			{
				for (uint32_t index = 0; index < pRenderer->mVkActiveQueueFamilyPropertyCount; ++index) {
					VkBool32 supports_present = VK_FALSE;
					VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(pRenderer->pActiveGPU, index, pSwapChain->pVkSurface, &supports_present);
					if ((VK_SUCCESS == res) && (VK_TRUE == supports_present)) {
						presentQueueFamilyIndex = index;
						break;
					}
				}
			}
		}

		// Find if gpu has a dedicated present queue
		if (presentQueueFamilyIndex != -1 && queue_family_indices[0] != presentQueueFamilyIndex)
		{
			queue_family_indices[1] = presentQueueFamilyIndex;

			vkGetDeviceQueue(pRenderer->pDevice, queue_family_indices[1], 0, &pSwapChain->pPresentQueue);
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
		swapChainCreateInfo.minImageCount = pSwapChain->mDesc.mImageCount;
		swapChainCreateInfo.imageFormat = surface_format.format;
		swapChainCreateInfo.imageColorSpace = surface_format.colorSpace;
		swapChainCreateInfo.imageExtent = extent;
		swapChainCreateInfo.imageArrayLayers = 1;
		swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapChainCreateInfo.imageSharingMode = sharing_mode;
		swapChainCreateInfo.queueFamilyIndexCount = queue_family_index_count;
		swapChainCreateInfo.pQueueFamilyIndices = queue_family_indices;
		swapChainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapChainCreateInfo.presentMode = present_mode;
		swapChainCreateInfo.clipped = VK_TRUE;
		swapChainCreateInfo.oldSwapchain = NULL;
		vk_res = vkCreateSwapchainKHR(pRenderer->pDevice, &swapChainCreateInfo, NULL, &(pSwapChain->pSwapChain));
		ASSERT(VK_SUCCESS == vk_res);

		pSwapChain->mDesc.mColorFormat = util_to_internal_image_format(surface_format.format);

		// Create rendertargets from swapchain
		uint32_t image_count = 0;
		vk_res = vkGetSwapchainImagesKHR(pRenderer->pDevice, pSwapChain->pSwapChain, &image_count, NULL);
		ASSERT(VK_SUCCESS == vk_res);

		ASSERT(image_count == pSwapChain->mDesc.mImageCount);

		pSwapChain->ppVkSwapChainImages = (VkImage*)conf_calloc(image_count, sizeof(*pSwapChain->ppVkSwapChainImages));
		ASSERT(pSwapChain->ppVkSwapChainImages);

		vk_res = vkGetSwapchainImagesKHR(pRenderer->pDevice, pSwapChain->pSwapChain, &image_count, pSwapChain->ppVkSwapChainImages);
		ASSERT(VK_SUCCESS == vk_res);

		RenderTargetDesc descColor = {};
		descColor.mType = RENDER_TARGET_TYPE_2D;
		descColor.mUsage = RENDER_TARGET_USAGE_COLOR;
		descColor.mWidth = pSwapChain->mDesc.mWidth;
		descColor.mHeight = pSwapChain->mDesc.mHeight;
		descColor.mDepth = 1;
		descColor.mArraySize = 1;
		descColor.mFormat = pSwapChain->mDesc.mColorFormat;
		descColor.mSrgb = pSwapChain->mDesc.mSrgb;
		descColor.mClearValue = pSwapChain->mDesc.mColorClearValue;
		descColor.mSampleCount = SAMPLE_COUNT_1;
		descColor.mSampleQuality = 0;

		pSwapChain->ppSwapchainRenderTargets = (RenderTarget**)conf_calloc(pSwapChain->mDesc.mImageCount, sizeof(*pSwapChain->ppSwapchainRenderTargets));

		// Populate the vk_image field and add the Vulkan texture objects
		for (uint32_t i = 0; i < pSwapChain->mDesc.mImageCount; ++i) {
			addRenderTarget(pRenderer, &descColor, &pSwapChain->ppSwapchainRenderTargets[i], (void*)pSwapChain->ppVkSwapChainImages[i]);
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

		vkDestroySwapchainKHR(pRenderer->pDevice, pSwapChain->pSwapChain, NULL);
		vkDestroySurfaceKHR(pRenderer->pVKInstance, pSwapChain->pVkSurface, NULL);

		SAFE_FREE(pSwapChain->ppSwapchainRenderTargets);
		SAFE_FREE(pSwapChain->ppVkSwapChainImages);
		SAFE_FREE(pSwapChain);
	}

	void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc);
		ASSERT(pDesc->mSize > 0);
		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);

		Buffer* pBuffer = (Buffer*)conf_calloc(1, sizeof(*pBuffer));
		ASSERT(pBuffer);

		pBuffer->pRenderer = pRenderer;
		pBuffer->mDesc = *pDesc;

		// Align the buffer size to multiples of the dynamic uniform buffer minimum size
		if (pBuffer->mDesc.mUsage & BUFFER_USAGE_UNIFORM)
		{
			uint64_t minAlignment = pRenderer->pActiveGpuSettings->mUniformBufferAlignment;
			pBuffer->mDesc.mSize = round_up_64(pBuffer->mDesc.mSize, minAlignment);
		}

		DECLARE_ZERO(VkBufferCreateInfo, add_info);
		add_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		add_info.pNext = NULL;
		add_info.flags = 0;
		add_info.size = pBuffer->mDesc.mSize;
		add_info.usage = util_to_vk_buffer_usage(pBuffer->mDesc.mUsage);
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
		VkResult vk_res = (VkResult)createBuffer(pRenderer->pVmaAllocator, &alloc_info, &vma_mem_reqs, pBuffer);
		ASSERT(VK_SUCCESS == vk_res);
		pBuffer->mCurrentState = RESOURCE_STATE_UNDEFINED;

		if ((pBuffer->mDesc.mUsage & BUFFER_USAGE_UNIFORM)
			|| (pBuffer->mDesc.mUsage & BUFFER_USAGE_STORAGE_SRV)
			|| (pBuffer->mDesc.mUsage & BUFFER_USAGE_STORAGE_UAV))
		{
			pBuffer->mVkBufferInfo.buffer = pBuffer->pVkBuffer;
			pBuffer->mVkBufferInfo.range = VK_WHOLE_SIZE;

			if ((pBuffer->mDesc.mUsage & BUFFER_USAGE_STORAGE_SRV)
				|| (pBuffer->mDesc.mUsage & BUFFER_USAGE_STORAGE_UAV))
			{
				pBuffer->mVkBufferInfo.offset = pBuffer->mDesc.mStructStride * pBuffer->mDesc.mFirstElement;
			}
			else
			{
				pBuffer->mVkBufferInfo.offset = 0;
			}
		}

		pBuffer->mBufferId = (++gBufferIds << 8U) + Thread::GetCurrentThreadID();

		*pp_buffer = pBuffer;
	}

	void removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
	{
		ASSERT(pRenderer);
		ASSERT(pBuffer);
		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);
		ASSERT(VK_NULL_HANDLE != pBuffer->pVkBuffer);

		destroyBuffer(pRenderer->pVmaAllocator, pBuffer);

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

		pTexture->pRenderer = pRenderer;
		pTexture->mDesc = *pDesc;
		pTexture->pCpuMappedAddress = NULL;
		// Monotonically increasing thread safe id generation
		pTexture->mTextureId = (++gTextureIds << 8U) + Thread::GetCurrentThreadID();
		pTexture->pRenderer = pRenderer;

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

		if (VK_NULL_HANDLE == pTexture->pVkImage)
		{
			VkImageType image_type = VK_IMAGE_TYPE_2D;
			switch (pTexture->mDesc.mType) {
			case TEXTURE_TYPE_1D: image_type = VK_IMAGE_TYPE_1D; break;
			case TEXTURE_TYPE_2D: image_type = VK_IMAGE_TYPE_2D; break;
			case TEXTURE_TYPE_3D: image_type = VK_IMAGE_TYPE_3D; break;
			case TEXTURE_TYPE_CUBE: image_type = VK_IMAGE_TYPE_2D; break;
			}

			VkExternalMemoryImageCreateInfoKHR externalInfo = {};
			externalInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR;
			externalInfo.pNext = NULL;
			externalInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;

			DECLARE_ZERO(VkImageCreateInfo, add_info);
			add_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			add_info.pNext = NULL;
			add_info.flags = 0;
			add_info.imageType = image_type;
			add_info.format = util_to_vk_image_format(pTexture->mDesc.mFormat, pTexture->mDesc.mSrgb);
			add_info.extent.width = pTexture->mDesc.mWidth;
			add_info.extent.height = pTexture->mDesc.mHeight;
			add_info.extent.depth = pTexture->mDesc.mType == TEXTURE_TYPE_3D ? pTexture->mDesc.mDepth : 1;
			add_info.mipLevels = pTexture->mDesc.mMipLevels;
			add_info.arrayLayers = pTexture->mDesc.mArraySize * (pTexture->mDesc.mType == TEXTURE_TYPE_CUBE ? 6 : 1);
			add_info.samples = util_to_vk_sample_count(pTexture->mDesc.mSampleCount);
			add_info.tiling = (0 != pTexture->mDesc.mHostVisible) ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
			add_info.usage = util_to_vk_image_usage(pTexture->mDesc.mUsage);
			add_info.usage |= additionalFlags;
			add_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			add_info.queueFamilyIndexCount = 0;
			add_info.pQueueFamilyIndices = NULL;
			add_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			if (pTexture->mDesc.mType == TEXTURE_TYPE_CUBE)
				add_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

			if (VK_IMAGE_USAGE_SAMPLED_BIT & add_info.usage)
			{
				// Make it easy to copy to and from textures
				add_info.usage |= (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
			}

			// Verify that GPU supports this format
			DECLARE_ZERO(VkFormatProperties, format_props);
			vkGetPhysicalDeviceFormatProperties(pRenderer->pActiveGPU, add_info.format, &format_props);
			VkFormatFeatureFlags format_features = util_vk_image_usage_to_format_features(add_info.usage);
			if (pTexture->mDesc.mHostVisible) {
				VkFormatFeatureFlags flags = format_props.linearTilingFeatures & format_features;
				ASSERT((0 != flags) && "Format is not supported for host visible images");
			}
			else {
				VkFormatFeatureFlags flags = format_props.optimalTilingFeatures & format_features;
				ASSERT((0 != flags) && "Format is not supported for GPU local images (i.e. not host visible images)");
			}

			AllocatorMemoryRequirements mem_reqs = { 0 };
			if (pDesc->mFlags & TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT)
				mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
			mem_reqs.usage = (VmaMemoryUsage)VMA_MEMORY_USAGE_GPU_ONLY;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
			VkImportMemoryWin32HandleInfoKHR importInfo = { VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR, NULL };
#endif
			VkExportMemoryAllocateInfoKHR exportMemoryInfo = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR, NULL };

			if (gExternalMemoryExtension && pDesc->mFlags & TEXTURE_CREATION_FLAG_IMPORT_BIT)
			{
				add_info.pNext = &externalInfo;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
				importInfo.handle = (HANDLE)pDesc->pNativeHandle;
				importInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
#endif

				mem_reqs.pUserData = &importInfo;
				// Allocate external (importable / exportable) memory as dedicated memory to avoid adding unnecessary complexity to the Vulkan Memory Allocator
				mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
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

			TextureCreateInfo alloc_info = { &add_info };
			VkResult vk_res = (VkResult)createTexture(pRenderer->pVmaAllocator, &alloc_info, &mem_reqs, pTexture);
			ASSERT(VK_SUCCESS == vk_res);

			pTexture->mCurrentState = RESOURCE_STATE_UNDEFINED;
		}

		// Create image view
		{
			VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
			switch (pTexture->mDesc.mType)
			{
			case TEXTURE_TYPE_1D:
				view_type = pTexture->mDesc.mArraySize > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
				break;
			case TEXTURE_TYPE_2D:
				view_type = pTexture->mDesc.mArraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
				break;
			case TEXTURE_TYPE_3D:
				if (pTexture->mDesc.mArraySize > 1)
				{
					LOGERROR("Cannot support 3D Texture Array in Vulkan");
					ASSERT(false);
				}
				view_type = VK_IMAGE_VIEW_TYPE_3D;
				break;
			case TEXTURE_TYPE_CUBE:
				view_type = pTexture->mDesc.mArraySize > 1 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
				break;
			}

			ASSERT(view_type != VK_IMAGE_VIEW_TYPE_MAX_ENUM && "Invalid Image View");

			DECLARE_ZERO(VkImageViewCreateInfo, add_info);
			add_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			add_info.pNext = NULL;
			add_info.flags = 0;
			add_info.image = pTexture->pVkImage;
			add_info.viewType = view_type;
			add_info.format = util_to_vk_image_format(pTexture->mDesc.mFormat, pTexture->mDesc.mSrgb);
			add_info.components.r = VK_COMPONENT_SWIZZLE_R;
			add_info.components.g = VK_COMPONENT_SWIZZLE_G;
			add_info.components.b = VK_COMPONENT_SWIZZLE_B;
			add_info.components.a = VK_COMPONENT_SWIZZLE_A;
			add_info.subresourceRange.aspectMask = util_vk_determine_aspect_mask(add_info.format);
			add_info.subresourceRange.baseMipLevel = pTexture->mDesc.mBaseMipLevel;
			add_info.subresourceRange.levelCount = pTexture->mDesc.mMipLevels;
			add_info.subresourceRange.baseArrayLayer = pTexture->mDesc.mBaseArrayLayer;
			add_info.subresourceRange.layerCount = pTexture->mDesc.mArraySize * (pTexture->mDesc.mType == TEXTURE_TYPE_CUBE ? 6 : 1);
			VkResult vk_res = vkCreateImageView(pRenderer->pDevice, &add_info, NULL, &(pTexture->pVkImageView));
			ASSERT(VK_SUCCESS == vk_res);

			pTexture->mVkAspectMask = add_info.subresourceRange.aspectMask;
		}

		pTexture->mVkTextureView.imageView = pTexture->pVkImageView;

		if (additionalFlags != 0)
		{
			if (additionalFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
				pTexture->mVkTextureView.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			else if (additionalFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
				pTexture->mVkTextureView.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			if (pTexture->mDesc.mUsage & TEXTURE_USAGE_UNORDERED_ACCESS)
				pTexture->mVkTextureView.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			else if (pTexture->mDesc.mUsage & TEXTURE_USAGE_SAMPLED_IMAGE)
				pTexture->mVkTextureView.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		// Get memory requirements that covers all mip levels
		DECLARE_ZERO(VkMemoryRequirements, vk_mem_reqs);
		vkGetImageMemoryRequirements(pTexture->pRenderer->pDevice, pTexture->pVkImage, &vk_mem_reqs);
		pTexture->mTextureSize = vk_mem_reqs.size;

		*ppTexture = pTexture;
	}

	void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget, void* pNativeHandle /* = NULL */)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc);
		ASSERT(ppRenderTarget);

		RenderTarget* pRenderTarget = (RenderTarget*)conf_calloc(1, sizeof(*pRenderTarget));
		pRenderTarget->mDesc = *pDesc;

		TextureDesc textureDesc = {};
		textureDesc.mBaseArrayLayer = pDesc->mBaseArrayLayer;
		textureDesc.mArraySize = pDesc->mArraySize;
		textureDesc.mClearValue = pDesc->mClearValue;
		textureDesc.mDepth = pDesc->mDepth;
		textureDesc.mFlags = pDesc->mFlags;
		textureDesc.mFormat = pDesc->mFormat;
		textureDesc.mHeight = pDesc->mHeight;
		textureDesc.mHostVisible = false;
		textureDesc.mBaseMipLevel = pDesc->mBaseMipLevel;
		textureDesc.mMipLevels = 1;
		textureDesc.mSampleCount = pDesc->mSampleCount;
		textureDesc.mSampleQuality = pDesc->mSampleQuality;
		textureDesc.mStartState = (pDesc->mUsage & RENDER_TARGET_USAGE_COLOR) ? RESOURCE_STATE_RENDER_TARGET : RESOURCE_STATE_DEPTH_WRITE;
		// Set this by default to be able to sample the rendertarget in shader
		textureDesc.mUsage = TEXTURE_USAGE_SAMPLED_IMAGE;
		textureDesc.mWidth = pDesc->mWidth;
		textureDesc.pNativeHandle = pNativeHandle;
		textureDesc.mSrgb = pDesc->mSrgb;

		switch (pDesc->mType)
		{
		case RENDER_TARGET_TYPE_1D:
			textureDesc.mType = TEXTURE_TYPE_1D;
			break;
		case RENDER_TARGET_TYPE_2D:
			textureDesc.mType = TEXTURE_TYPE_2D;
			break;
		case RENDER_TARGET_TYPE_3D:
			textureDesc.mType = TEXTURE_TYPE_3D;
			break;
		default:
			break;
		}

		if (pDesc->mUsage == RENDER_TARGET_USAGE_DEPTH_STENCIL)
		{
			// Make sure depth/stencil format is supported - fall back to VK_FORMAT_D16_UNORM if not
			VkFormat vk_depth_stencil_format = util_to_vk_image_format(pDesc->mFormat, false);
			if (VK_FORMAT_UNDEFINED != vk_depth_stencil_format) {
				DECLARE_ZERO(VkImageFormatProperties, properties);
				VkResult vk_res = vkGetPhysicalDeviceImageFormatProperties(pRenderer->pActiveGPU,
					vk_depth_stencil_format,
					VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
					VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
					0,
					&properties);
				// Fall back to something that's guaranteed to work
				if (VK_SUCCESS != vk_res) {
					textureDesc.mFormat = ImageFormat::D16;
					LOGWARNINGF("Depth stencil format (%u) not supported. Falling back to D16 format", pDesc->mFormat);
				}
			}
		}

		addTexture(pRenderer, &textureDesc, &pRenderTarget->pTexture);

		*ppRenderTarget = pRenderTarget;
	}

	void removeTexture(Renderer* pRenderer, Texture* pTexture)
	{
		ASSERT(pRenderer);
		ASSERT(pTexture);
		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);
		ASSERT(VK_NULL_HANDLE != pTexture->pVkImage);

		if (pTexture->mOwnsImage)
			destroyTexture(pRenderer->pVmaAllocator, pTexture);

		if (VK_NULL_HANDLE != pTexture->pVkImageView) {
			vkDestroyImageView(pRenderer->pDevice, pTexture->pVkImageView, NULL);
		}

		SAFE_FREE(pTexture);
	}

	void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
	{
		removeTexture(pRenderer, pRenderTarget->pTexture);
		SAFE_FREE(pRenderTarget);
	}

	void addSampler(Renderer* pRenderer, Sampler** pp_sampler, FilterType minFilter, FilterType magFilter, MipMapMode  mipMapMode, AddressMode addressU, AddressMode addressV, AddressMode addressW, float mipLosBias, float maxAnisotropy)
	{
		ASSERT(pRenderer);
		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);

		Sampler* pSampler = (Sampler*)conf_calloc(1, sizeof(*pSampler));
		ASSERT(pSampler);
		pSampler->pRenderer = pRenderer;

		DECLARE_ZERO(VkSamplerCreateInfo, add_info);

		add_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		add_info.pNext = NULL;
		add_info.flags = 0;
		add_info.magFilter = util_to_vk_filter(magFilter);
		add_info.minFilter = util_to_vk_filter(minFilter);
		add_info.mipmapMode = util_to_vk_mip_map_mode(mipMapMode);
		add_info.addressModeU = util_to_vk_address_mode(addressU);
		add_info.addressModeV = util_to_vk_address_mode(addressV);
		add_info.addressModeW = util_to_vk_address_mode(addressW);
		add_info.mipLodBias = mipLosBias;
		add_info.anisotropyEnable = VK_FALSE;
		add_info.maxAnisotropy = maxAnisotropy;
		add_info.compareEnable = VK_FALSE;
		add_info.compareOp = VK_COMPARE_OP_NEVER;
		add_info.minLod = 0.0f;
		add_info.maxLod = magFilter >= FILTER_LINEAR ? FLT_MAX : 0.0f;
		add_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		add_info.unnormalizedCoordinates = VK_FALSE;

		VkResult vk_res = vkCreateSampler(pRenderer->pDevice, &add_info, NULL, &(pSampler->pVkSampler));
		ASSERT(VK_SUCCESS == vk_res);

		pSampler->mSamplerId = (++gSamplerIds << 8U) + Thread::GetCurrentThreadID();

		pSampler->mVkSamplerView.sampler = pSampler->pVkSampler;

		*pp_sampler = pSampler;
	}

	void removeSampler(Renderer* pRenderer, Sampler* pSampler)
	{
		ASSERT(pRenderer);
		ASSERT(pSampler);
		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);
		ASSERT(VK_NULL_HANDLE != pSampler->pVkSampler);

		vkDestroySampler(pRenderer->pDevice, pSampler->pVkSampler, NULL);

		SAFE_FREE(pSampler);
	}

	void addShader(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
	{
		Shader* pShaderProgram = (Shader*)conf_calloc(1, sizeof(*pShaderProgram));
		pShaderProgram->mStages = pDesc->mStages;

		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);

		uint32_t counter = 0;
		ShaderReflection stageReflections[SHADER_STAGE_COUNT];

		for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i) {
			ShaderStage stage_mask = (ShaderStage)(1 << i);
			if (stage_mask == (pShaderProgram->mStages & stage_mask)) {
				DECLARE_ZERO(VkShaderModuleCreateInfo, create_info);
				create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				create_info.pNext = NULL;
				create_info.flags = 0;
				switch (stage_mask) {
				case SHADER_STAGE_VERT: {
					createShaderReflection((const uint8_t*)pDesc->mVert.pByteCode, (uint32_t)pDesc->mVert.mByteCodeSize, SHADER_STAGE_VERT, &stageReflections[counter++]);

					create_info.codeSize = pDesc->mVert.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mVert.pByteCode;
					VkResult vk_res = vkCreateShaderModule(pRenderer->pDevice, &create_info, NULL, &(pShaderProgram->pVkVert));
					ASSERT(VK_SUCCESS == vk_res);
				} break;
				case SHADER_STAGE_TESC: {
					createShaderReflection((const uint8_t*)pDesc->mHull.pByteCode, (uint32_t)pDesc->mHull.mByteCodeSize, SHADER_STAGE_TESC, &stageReflections[counter++]);

					memcpy(&pShaderProgram->mNumControlPoint, &stageReflections[counter - 1].mNumControlPoint, sizeof(pShaderProgram->mNumControlPoint));

					create_info.codeSize = pDesc->mHull.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mHull.pByteCode;
					VkResult vk_res = vkCreateShaderModule(pRenderer->pDevice, &create_info, NULL, &(pShaderProgram->pVkTesc));
					ASSERT(VK_SUCCESS == vk_res);
				} break;
				case SHADER_STAGE_TESE: {
					createShaderReflection((const uint8_t*)pDesc->mDomain.pByteCode, (uint32_t)pDesc->mDomain.mByteCodeSize, SHADER_STAGE_TESE, &stageReflections[counter++]);

					create_info.codeSize = pDesc->mDomain.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mDomain.pByteCode;
					VkResult vk_res = vkCreateShaderModule(pRenderer->pDevice, &create_info, NULL, &(pShaderProgram->pVkTese));
					ASSERT(VK_SUCCESS == vk_res);
				} break;
				case SHADER_STAGE_GEOM: {
					createShaderReflection((const uint8_t*)pDesc->mGeom.pByteCode, (uint32_t)pDesc->mGeom.mByteCodeSize, SHADER_STAGE_GEOM, &stageReflections[counter++]);

					create_info.codeSize = pDesc->mGeom.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mGeom.pByteCode;
					VkResult vk_res = vkCreateShaderModule(pRenderer->pDevice, &create_info, NULL, &(pShaderProgram->pVkGeom));
					ASSERT(VK_SUCCESS == vk_res);
				} break;
				case SHADER_STAGE_FRAG: {
					createShaderReflection((const uint8_t*)pDesc->mFrag.pByteCode, (uint32_t)pDesc->mFrag.mByteCodeSize, SHADER_STAGE_FRAG, &stageReflections[counter++]);

					create_info.codeSize = pDesc->mFrag.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mFrag.pByteCode;
					VkResult vk_res = vkCreateShaderModule(pRenderer->pDevice, &create_info, NULL, &(pShaderProgram->pVkFrag));
					ASSERT(VK_SUCCESS == vk_res);
				} break;
				case SHADER_STAGE_COMP: {
					createShaderReflection((const uint8_t*)pDesc->mComp.pByteCode, (uint32_t)pDesc->mComp.mByteCodeSize, SHADER_STAGE_COMP, &stageReflections[counter++]);

					memcpy(pShaderProgram->mNumThreadsPerGroup, stageReflections[counter - 1].mNumThreadsPerGroup, sizeof(pShaderProgram->mNumThreadsPerGroup));

					create_info.codeSize = pDesc->mComp.mByteCodeSize;
					create_info.pCode = (const uint32_t*)pDesc->mComp.pByteCode;
					VkResult vk_res = vkCreateShaderModule(pRenderer->pDevice, &create_info, NULL, &(pShaderProgram->pVkComp));
					ASSERT(VK_SUCCESS == vk_res);
				} break;
				}
			}
		}

		createPipelineReflection(stageReflections, counter, &pShaderProgram->mReflection);

		*ppShaderProgram = pShaderProgram;
	}

	void removeShader(Renderer* pRenderer, Shader* pShaderProgram)
	{
		ASSERT(pRenderer);

		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);

		if (VK_NULL_HANDLE != pShaderProgram->pVkVert) {
			vkDestroyShaderModule(pRenderer->pDevice, pShaderProgram->pVkVert, NULL);
		}

		if (VK_NULL_HANDLE != pShaderProgram->pVkTesc) {
			vkDestroyShaderModule(pRenderer->pDevice, pShaderProgram->pVkTesc, NULL);
		}

		if (VK_NULL_HANDLE != pShaderProgram->pVkTese) {
			vkDestroyShaderModule(pRenderer->pDevice, pShaderProgram->pVkTese, NULL);
		}

		if (VK_NULL_HANDLE != pShaderProgram->pVkGeom) {
			vkDestroyShaderModule(pRenderer->pDevice, pShaderProgram->pVkGeom, NULL);
		}

		if (VK_NULL_HANDLE != pShaderProgram->pVkFrag) {
			vkDestroyShaderModule(pRenderer->pDevice, pShaderProgram->pVkFrag, NULL);
		}

		if (VK_NULL_HANDLE != pShaderProgram->pVkComp) {
			vkDestroyShaderModule(pRenderer->pDevice, pShaderProgram->pVkComp, NULL);
		}

		destroyPipelineReflection(&pShaderProgram->mReflection);

		SAFE_FREE(pShaderProgram);
	}

	typedef struct UpdateFrequencyLayoutInfo
	{
		/// Array of all bindings in the descriptor set
		tinystl::vector <VkDescriptorSetLayoutBinding> mBindings;
		/// Array of all descriptors in this descriptor set
		tinystl::vector <DescriptorInfo*> mDescriptors;
		/// Array of all descriptors marked as dynamic in this descriptor set (applicable to DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		tinystl::vector <DescriptorInfo*> mDynamicDescriptors;
		/// Hash map to get index of the descriptor in the root signature
		tinystl::unordered_map<DescriptorInfo*, uint32_t> mDescriptorIndexMap;
	} UpdateFrequencyLayoutInfo;

	/// Default root signature description - Used when no description is provided in addRootSignature
	RootSignatureDesc gDefaultRootSignatureDesc = {};

	void addRootSignature(Renderer* pRenderer, uint32_t numShaders, Shader* const* ppShaders, RootSignature** ppRootSignature, const RootSignatureDesc* pRootDesc)
	{
		RootSignature* pRootSignature = (RootSignature*)conf_calloc(1, sizeof(*pRootSignature));

		tinystl::vector<UpdateFrequencyLayoutInfo> layouts(DESCRIPTOR_UPDATE_FREQ_COUNT);
		tinystl::vector<DescriptorInfo*> pushConstantDescriptors;
		tinystl::vector<ShaderResource const*> shaderResources;
		const RootSignatureDesc* pRootSignatureDesc = pRootDesc ? pRootDesc : &gDefaultRootSignatureDesc;

		conf_placement_new<tinystl::unordered_map<uint32_t,uint32_t> >(&pRootSignature->pDescriptorNameToIndexMap);

		// Collect all unique shader resources in the given shaders
		// Resources are parsed by name (two resources named "XYZ" in two shaders will be considered the same resource)
		for (uint32_t sh = 0; sh < numShaders; ++sh)
		{
			PipelineReflection const* pReflection = &ppShaders[sh]->mReflection;

			if (pReflection->mShaderStages & SHADER_STAGE_COMP)
				pRootSignature->mPipelineType = PIPELINE_TYPE_COMPUTE;
			else
				pRootSignature->mPipelineType = PIPELINE_TYPE_GRAPHICS;

			for (uint32_t i = 0; i < pReflection->mShaderResourceCount; ++i)
			{
				ShaderResource const* pRes = &pReflection->pShaderResources[i];
				uint32_t setIndex = pRes->set;

				if (pRes->type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
					setIndex = 0;

				if (pRootSignature->pDescriptorNameToIndexMap.find(tinystl::hash(pRes->name)).node == 0)
				{
					pRootSignature->pDescriptorNameToIndexMap.insert({ tinystl::hash(pRes->name), (uint32_t)shaderResources.size() });
					shaderResources.emplace_back(pRes);
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
			DescriptorInfo* pDesc = &pRootSignature->pDescriptors[i];
			ShaderResource const* pRes = shaderResources[i];
			uint32_t setIndex = pRes->set;
			DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)setIndex;

			// Copy the binding information generated from the shader reflection into the descriptor
			pDesc->mDesc.reg = pRes->reg;
			pDesc->mDesc.set = pRes->set;
			pDesc->mDesc.size = pRes->size;
			pDesc->mDesc.type = pRes->type;
			pDesc->mDesc.used_stages = pRes->used_stages;
			pDesc->mDesc.name_size = pRes->name_size;
			pDesc->mDesc.name = (const char*)conf_calloc(pDesc->mDesc.name_size + 1, sizeof(char));
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
				if (pRootSignatureDesc->mDynamicUniformBuffers.find(pDesc->mDesc.name) != pRootSignatureDesc->mDynamicUniformBuffers.end())
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
				const tinystl::unordered_hash_node<tinystl::string, Sampler*>* pNode = pRootSignatureDesc->mStaticSamplers.find(pDesc->mDesc.name).node;
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

		pRootSignature->pDescriptorSetLayouts = (DescriptorSetLayout*)conf_calloc((uint32_t)layouts.size(), sizeof(*pRootSignature->pDescriptorSetLayouts));
		pRootSignature->pRootDescriptorLayouts = (RootDescriptorLayout*)conf_calloc((uint32_t)layouts.size(), sizeof(*pRootSignature->pRootDescriptorLayouts));

		pRootSignature->mRootConstantCount = (uint32_t)pushConstantDescriptors.size();
		if (pRootSignature->mRootConstantCount)
			pRootSignature->pRootConstantLayouts = (RootConstantLayout*)conf_calloc(pRootSignature->mRootConstantCount, sizeof(*pRootSignature->pRootConstantLayouts));

		// Create push constant ranges
		for (uint32_t i = 0; i < pRootSignature->mRootConstantCount; ++i)
		{
			RootConstantLayout* pConst = &pRootSignature->pRootConstantLayouts[i];
			DescriptorInfo* pDesc = pushConstantDescriptors[i];
			pDesc->mIndexInParent = i;
			pConst->mDescriptorIndex = layouts[0].mDescriptorIndexMap[pDesc];
			pConst->mVkPushConstantRange.offset = 0;
			pConst->mVkPushConstantRange.size = pDesc->mDesc.size;
			pConst->mVkPushConstantRange.stageFlags = util_to_vk_shader_stage_flags(pDesc->mDesc.used_stages);
		}

		// Create descriptor layouts
		// Put most frequently changed params first
		for (uint32_t i = (uint32_t)layouts.size(); i-- > 0U;)
		{
			UpdateFrequencyLayoutInfo& layout = layouts[i];
			DescriptorSetLayout& table = pRootSignature->pDescriptorSetLayouts[i];
			RootDescriptorLayout& root = pRootSignature->pRootDescriptorLayouts[i];

			if (layouts[i].mBindings.size())
			{
				// sort table by type (CBV/SRV/UAV) by register
				layout.mBindings.sort([](const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs)
				{
					return (int)(lhs.binding - rhs.binding);
				});
				layout.mBindings.sort([](const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs)
				{
					return (int)(lhs.descriptorType - rhs.descriptorType);
				});
			}

			VkDescriptorSetLayoutCreateInfo layoutInfo = {};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.pNext = NULL;
			layoutInfo.bindingCount = (uint32_t)layout.mBindings.size();
			layoutInfo.pBindings = layout.mBindings.data();
			layoutInfo.flags = 0;

			vkCreateDescriptorSetLayout(pRenderer->pDevice, &layoutInfo, NULL, &table.pVkSetLayout);

			if (!layouts[i].mBindings.size())
				continue;

			table.mDescriptorCount = (uint32_t)layout.mDescriptors.size();
			table.pDescriptorIndices = (uint32_t*)conf_calloc(table.mDescriptorCount, sizeof(uint32_t));

			// Loop through descriptors belonging to this update frequency and increment the cumulative descriptor count
			for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mDescriptors.size(); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mDescriptors[descIndex];
				pDesc->mIndexInParent = descIndex;
				table.mCumulativeDescriptorCount += pDesc->mDesc.size;
				if (pDesc->mDesc.type == DESCRIPTOR_TYPE_BUFFER || pDesc->mDesc.type == DESCRIPTOR_TYPE_RW_BUFFER || pDesc->mDesc.type == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
				{
					pDesc->mHandleIndex = table.mCumulativeBufferDescriptorCount;
					table.mCumulativeBufferDescriptorCount += pDesc->mDesc.size;
				}
				else
				{
					pDesc->mHandleIndex = table.mCumulativeImageDescriptorCount;
					table.mCumulativeImageDescriptorCount += pDesc->mDesc.size;
				}
				table.pDescriptorIndices[descIndex] = layout.mDescriptorIndexMap[pDesc];
			}

			root.mRootDescriptorCount = (uint32_t)layout.mDynamicDescriptors.size();
			if (root.mRootDescriptorCount)
				root.pDescriptorIndices = (uint32_t*)conf_calloc(root.mRootDescriptorCount, sizeof(uint32_t));
			for (uint32_t descIndex = 0; descIndex < root.mRootDescriptorCount; ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mDynamicDescriptors[descIndex];
				pDesc->mDynamicUniformIndex = descIndex;
				root.pDescriptorIndices[descIndex] = layout.mDescriptorIndexMap[pDesc];
			}
		}

		/************************************************************************/
		// Pipeline layout
		/************************************************************************/
		tinystl::vector<VkDescriptorSetLayout> descriptorSetLayouts(DESCRIPTOR_UPDATE_FREQ_COUNT);
		tinystl::vector<VkPushConstantRange> pushConstants(pRootSignature->mRootConstantCount);
		for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
		{
			descriptorSetLayouts[i] = pRootSignature->pDescriptorSetLayouts[i].pVkSetLayout;
		}
		for (uint32_t i = 0; i < pRootSignature->mRootConstantCount; ++i)
			pushConstants[i] = pRootSignature->pRootConstantLayouts[i].mVkPushConstantRange;

		DECLARE_ZERO(VkPipelineLayoutCreateInfo, add_info);
		add_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		add_info.pNext = NULL;
		add_info.flags = 0;
		add_info.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
		add_info.pSetLayouts = descriptorSetLayouts.data();
		add_info.pushConstantRangeCount = pRootSignature->mRootConstantCount;
		add_info.pPushConstantRanges = pushConstants.data();
		VkResult vk_res = vkCreatePipelineLayout(pRenderer->pDevice, &add_info, NULL, &(pRootSignature->pPipelineLayout));
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

		pRootSignature->pDescriptorManagerMap.~unordered_map();

		vkDestroyPipelineLayout(pRenderer->pDevice, pRootSignature->pPipelineLayout, NULL);

		for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
		{
			vkDestroyDescriptorSetLayout(pRenderer->pDevice, pRootSignature->pDescriptorSetLayouts[i].pVkSetLayout, NULL);

			SAFE_FREE(pRootSignature->pDescriptorSetLayouts[i].pDescriptorIndices);
			SAFE_FREE(pRootSignature->pRootDescriptorLayouts[i].pDescriptorIndices);
		}

		for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
		{
			SAFE_FREE(pRootSignature->pDescriptors[i].mDesc.name);
		}

		SAFE_FREE(pRootSignature->pRootConstantLayouts);
		SAFE_FREE(pRootSignature->pDescriptors);
		SAFE_FREE(pRootSignature->pDescriptorSetLayouts);
		SAFE_FREE(pRootSignature->pRootDescriptorLayouts);

		// Need delete since the destructor frees allocated memory
		pRootSignature->pDescriptorNameToIndexMap.~unordered_map();

		SAFE_FREE(pRootSignature);
	}

	void addPipeline(Renderer* pRenderer, const GraphicsPipelineDesc* pDesc, Pipeline** ppPipeline)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc);
		ASSERT(pDesc->pShaderProgram);
		ASSERT(pDesc->pRootSignature);

		Pipeline* pPipeline = (Pipeline*)conf_calloc(1, sizeof(*pPipeline));
		ASSERT(pPipeline);

		const Shader* pShaderProgram = pDesc->pShaderProgram;
		const VertexLayout* pVertexLayout = pDesc->pVertexLayout;

		memcpy(&(pPipeline->mGraphics), pDesc, sizeof(*pDesc));
		pPipeline->mType = PIPELINE_TYPE_GRAPHICS;

		// Create tempporary renderpass for pipeline creation
		RenderPassDesc renderPassDesc = { 0 };
		RenderPass* pRenderPass = NULL;
		renderPassDesc.mRenderTargetCount = pDesc->mRenderTargetCount;
		renderPassDesc.pColorFormats = pDesc->pColorFormats;
		renderPassDesc.mSampleCount = pDesc->mSampleCount;
		renderPassDesc.pSrgbValues = pDesc->pSrgbValues;
		renderPassDesc.mDepthStencilFormat = pDesc->mDepthStencilFormat;
		addRenderPass(pRenderer, &renderPassDesc, &pRenderPass);

		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);
		ASSERT((VK_NULL_HANDLE != pShaderProgram->pVkVert) || (VK_NULL_HANDLE != pShaderProgram->pVkTesc) || (VK_NULL_HANDLE != pShaderProgram->pVkTese) || (VK_NULL_HANDLE != pShaderProgram->pVkGeom) || (VK_NULL_HANDLE != pShaderProgram->pVkFrag));

		// Pipeline
		{
			uint32_t stage_count = 0;
			DECLARE_ZERO(VkPipelineShaderStageCreateInfo, stages[5]);
			for (uint32_t i = 0; i < 5; ++i) {
				ShaderStage stage_mask = (ShaderStage)(1 << i);
				if (stage_mask == (pShaderProgram->mStages & stage_mask)) {
					stages[stage_count].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
					stages[stage_count].pNext = NULL;
					stages[stage_count].flags = 0;
					stages[stage_count].pSpecializationInfo = NULL;
					switch (stage_mask) {
					case SHADER_STAGE_VERT: {
						stages[stage_count].pName = pShaderProgram->mReflection.mStageReflections[pShaderProgram->mReflection.mVertexStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_VERTEX_BIT;
						stages[stage_count].module = pShaderProgram->pVkVert;
					} break;
					case SHADER_STAGE_TESC: {
						stages[stage_count].pName = pShaderProgram->mReflection.mStageReflections[pShaderProgram->mReflection.mHullStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
						stages[stage_count].module = pShaderProgram->pVkTesc;
					} break;
					case SHADER_STAGE_TESE: {
						stages[stage_count].pName = pShaderProgram->mReflection.mStageReflections[pShaderProgram->mReflection.mDomainStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
						stages[stage_count].module = pShaderProgram->pVkTese;
					} break;
					case SHADER_STAGE_GEOM: {
						stages[stage_count].pName = pShaderProgram->mReflection.mStageReflections[pShaderProgram->mReflection.mGeometryStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
						stages[stage_count].module = pShaderProgram->pVkGeom;
					} break;
					case SHADER_STAGE_FRAG: {
						stages[stage_count].pName = pShaderProgram->mReflection.mStageReflections[pShaderProgram->mReflection.mPixelStageIndex].pEntryPoint;
						stages[stage_count].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
						stages[stage_count].module = pShaderProgram->pVkFrag;
					} break;
					}
					++stage_count;
				}
			}

			// Make sure there's a shader
			ASSERT(0 != stage_count);

			uint32_t input_binding_count = 0;
			VkVertexInputBindingDescription input_bindings[MAX_VERTEX_BINDINGS] = { 0 };
			uint32_t input_attribute_count = 0;
			VkVertexInputAttributeDescription input_attributes[MAX_VERTEX_ATTRIBS] = { 0 };

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
					input_bindings[input_binding_count - 1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
					input_bindings[input_binding_count - 1].stride += calculateImageFormatStride(attrib->mFormat);

					input_attributes[input_attribute_count].location = attrib->mLocation;
					input_attributes[input_attribute_count].binding = attrib->mBinding;
					input_attributes[input_attribute_count].format = util_to_vk_image_format(attrib->mFormat, false);
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
			switch (pDesc->mPrimitiveTopo) {
			case PRIMITIVE_TOPO_POINT_LIST: topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
			case PRIMITIVE_TOPO_LINE_LIST: topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
			case PRIMITIVE_TOPO_LINE_STRIP: topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
			case PRIMITIVE_TOPO_TRI_STRIP: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
			case PRIMITIVE_TOPO_PATCH_LIST: topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; break;
			}
			DECLARE_ZERO(VkPipelineInputAssemblyStateCreateInfo, ia);
			ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			ia.pNext = NULL;
			ia.flags = 0;
			ia.topology = topology;
			ia.primitiveRestartEnable = VK_FALSE;

			DECLARE_ZERO(VkPipelineTessellationStateCreateInfo, ts);
			ts.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
			ts.pNext = NULL;
			ts.flags = 0;
			ts.patchControlPoints = pShaderProgram->mNumControlPoint;

			DECLARE_ZERO(VkPipelineViewportStateCreateInfo, vs);
			vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			vs.pNext = NULL;
			vs.flags = 0;
			// we are using dynimic viewports but we must set the count to 1
			vs.viewportCount = 1;
			vs.pViewports = NULL;
			vs.scissorCount = 1;
			vs.pScissors = NULL;

			BlendState* pBlendState = pDesc->pBlendState != NULL ? pDesc->pBlendState : pRenderer->pDefaultBlendState;
			DepthState* pDepthState = pDesc->pDepthState != NULL ? pDesc->pDepthState : pRenderer->pDefaultDepthState;
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

			if (pShaderProgram->pVkTesc != NULL && pShaderProgram->pVkTese != NULL)
				add_info.pTessellationState = &ts;
			else
				add_info.pTessellationState = NULL; // set tessellation state to null if we have no tessellation

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
			VkResult vk_res = vkCreateGraphicsPipelines(pRenderer->pDevice, VK_NULL_HANDLE, 1, &add_info, NULL, &(pPipeline->pVkPipeline));
			ASSERT(VK_SUCCESS == vk_res);

			removeRenderPass(pRenderer, pRenderPass);
		}

		*ppPipeline = pPipeline;
	}

	void addComputePipeline(Renderer* pRenderer, const ComputePipelineDesc* pDesc, Pipeline** ppPipeline)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc);
		ASSERT(pDesc->pShaderProgram);
		ASSERT(pDesc->pRootSignature);
		ASSERT(pRenderer->pDevice != VK_NULL_HANDLE);
		ASSERT(pDesc->pShaderProgram->pVkComp != VK_NULL_HANDLE);

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
			stage.module = pDesc->pShaderProgram->pVkComp;
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
			VkResult vk_res = vkCreateComputePipelines(pRenderer->pDevice, VK_NULL_HANDLE, 1, &create_info, NULL, &(pPipeline->pVkPipeline));
			ASSERT(VK_SUCCESS == vk_res);
		}

		*ppPipeline = pPipeline;
	}

	void removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
	{
		ASSERT(pRenderer);
		ASSERT(pPipeline);
		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);
		ASSERT(VK_NULL_HANDLE != pPipeline->pVkPipeline);

		vkDestroyPipeline(pRenderer->pDevice, pPipeline->pVkPipeline, NULL);

		SAFE_FREE(pPipeline);
	}

	void addBlendState(BlendState** ppBlendState,
		BlendConstant srcFactor, BlendConstant destFactor,
		BlendConstant srcAlphaFactor, BlendConstant destAlphaFactor,
		BlendMode blendMode /*= BlendMode::BM_REPLACE*/, BlendMode blendAlphaMode /*= BlendMode::BM_REPLACE*/,
		const int mask /*= ALL*/, const int MRTRenderTargetNumber /*= eBlendStateMRTRenderTarget0*/, const bool alphaToCoverage /*= false*/)
	{
		ASSERT(srcFactor < BlendConstant::MAX_BLEND_CONSTANTS);
		ASSERT(destFactor < BlendConstant::MAX_BLEND_CONSTANTS);
		ASSERT(srcAlphaFactor < BlendConstant::MAX_BLEND_CONSTANTS);
		ASSERT(destAlphaFactor < BlendConstant::MAX_BLEND_CONSTANTS);
		ASSERT(blendMode < BlendMode::MAX_BLEND_MODES);
		ASSERT(blendAlphaMode < BlendMode::MAX_BLEND_MODES);

		VkBool32 blendEnable = (gVkBlendConstantTranslator[srcFactor] != VK_BLEND_FACTOR_ONE || gVkBlendConstantTranslator[destFactor] != VK_BLEND_FACTOR_ZERO ||
			gVkBlendConstantTranslator[srcAlphaFactor] != VK_BLEND_FACTOR_ONE || gVkBlendConstantTranslator[destAlphaFactor] != VK_BLEND_FACTOR_ZERO);

		BlendState blendState = {};

		memset(blendState.RTBlendStates, 0, sizeof(blendState.RTBlendStates));
		for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
		{
			blendState.RTBlendStates[i].colorWriteMask = mask;
			if (MRTRenderTargetNumber & (1 << i))
			{
				blendState.RTBlendStates[i].blendEnable = blendEnable;
				blendState.RTBlendStates[i].srcColorBlendFactor = gVkBlendConstantTranslator[srcFactor];
				blendState.RTBlendStates[i].dstColorBlendFactor = gVkBlendConstantTranslator[destFactor];
				blendState.RTBlendStates[i].colorBlendOp = gVkBlendOpTranslator[blendMode];
				blendState.RTBlendStates[i].srcAlphaBlendFactor = gVkBlendConstantTranslator[srcAlphaFactor];
				blendState.RTBlendStates[i].dstAlphaBlendFactor = gVkBlendConstantTranslator[destAlphaFactor];
				blendState.RTBlendStates[i].alphaBlendOp = gVkBlendOpTranslator[blendAlphaMode];
			}
		}

		blendState.LogicOpEnable = false;
		blendState.LogicOp = VK_LOGIC_OP_CLEAR;

		*ppBlendState = (BlendState*)conf_malloc(sizeof(blendState));
		memcpy(*ppBlendState, &blendState, sizeof(blendState));
	}

	void removeBlendState(BlendState* pBlendState)
	{
		SAFE_FREE(pBlendState);
	}

	void addDepthState(Renderer* pRenderer, DepthState** ppDepthState, const bool depthTest, const bool depthWrite,
		const CompareMode depthFunc /*= CompareMode::CMP_LEQUAL*/,
		const bool stencilTest /*= false*/,
		const uint8 stencilReadMask /*= 0xFF*/,
		const uint8 stencilWriteMask /*= 0xFF*/,
		const CompareMode stencilFrontFunc /*= CompareMode::CMP_ALWAYS*/,
		const StencilOp stencilFrontFail /*= StencilOp::STENCIL_OP_KEEP*/,
		const StencilOp depthFrontFail /*= StencilOp::STENCIL_OP_KEEP*/,
		const StencilOp stencilFrontPass /*= StencilOp::STENCIL_OP_KEEP*/,
		const CompareMode stencilBackFunc /*= CompareMode::CMP_ALWAYS*/,
		const StencilOp stencilBackFail /*= StencilOp::STENCIL_OP_KEEP*/,
		const StencilOp depthBackFail /*= StencilOp::STENCIL_OP_KEEP*/,
		const StencilOp stencilBackPass /*= StencilOp::STENCIL_OP_KEEP*/)
	{
		ASSERT(depthFunc < CompareMode::MAX_COMPARE_MODES);
		ASSERT(stencilFrontFunc < CompareMode::MAX_COMPARE_MODES);
		ASSERT(stencilFrontFail < StencilOp::MAX_STENCIL_OPS);
		ASSERT(depthFrontFail < StencilOp::MAX_STENCIL_OPS);
		ASSERT(stencilFrontPass < StencilOp::MAX_STENCIL_OPS);
		ASSERT(stencilBackFunc < CompareMode::MAX_COMPARE_MODES);
		ASSERT(stencilBackFail < StencilOp::MAX_STENCIL_OPS);
		ASSERT(depthBackFail < StencilOp::MAX_STENCIL_OPS);
		ASSERT(stencilBackPass < StencilOp::MAX_STENCIL_OPS);

		DepthState depthState = {};
		depthState.DepthTestEnable = depthTest;
		depthState.DepthWriteEnable = depthWrite;
		depthState.DepthCompareOp = gVkComparisonFuncTranslator[depthFunc];
		depthState.StencilTestEnable = stencilTest;

		depthState.Front.failOp = gVkStencilOpTranslator[stencilFrontFail];
		depthState.Front.passOp = gVkStencilOpTranslator[stencilFrontPass];
		depthState.Front.depthFailOp = gVkStencilOpTranslator[depthFrontFail];
		depthState.Front.compareOp = VkCompareOp(stencilFrontFunc);
		depthState.Front.compareMask = stencilReadMask;
		depthState.Front.writeMask = stencilWriteMask;
		depthState.Front.reference = 0;

		depthState.Back.failOp = gVkStencilOpTranslator[stencilBackFail];
		depthState.Back.passOp = gVkStencilOpTranslator[stencilBackPass];
		depthState.Back.depthFailOp = gVkStencilOpTranslator[depthBackFail];
		depthState.Back.compareOp = gVkComparisonFuncTranslator[stencilBackFunc];
		depthState.Back.compareMask = stencilReadMask;
		depthState.Back.writeMask = stencilFrontFunc;
		depthState.Back.reference = 0;

		depthState.DepthBoundsTestEnable = false;
		depthState.MinDepthBounds = 0;
		depthState.MaxDepthBounds = 1;

		*ppDepthState = (DepthState*)conf_malloc(sizeof(depthState));
		memcpy(*ppDepthState, &depthState, sizeof(depthState));
	}

	void removeDepthState(DepthState* pDepthState)
	{
		SAFE_FREE(pDepthState);
	}

	void addRasterizerState(RasterizerState** ppRasterizerState,
		const CullMode cullMode,
		const int depthBias /*= 0*/,
		const float slopeScaledDepthBias /*= 0*/,
		const FillMode fillMode /*= FillMode::FILL_MODE_SOLID*/,
		const bool multiSample /*= false*/,
		const bool scissor /*= false*/)
	{
		ASSERT(fillMode < FillMode::MAX_FILL_MODES);
		ASSERT(cullMode < CullMode::MAX_CULL_MODES);

		RasterizerState rasterizerState = {};

		rasterizerState.DepthClampEnable = VK_TRUE;
		rasterizerState.PolygonMode = gVkFillModeTranslator[fillMode];
		rasterizerState.CullMode = gVkCullModeTranslator[cullMode];
		rasterizerState.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Change this when negative viewport is in
		rasterizerState.DepthBiasEnable = (depthBias != 0) ? VK_TRUE : VK_FALSE;
		rasterizerState.DepthBiasConstantFactor = float(depthBias);
		rasterizerState.DepthBiasClamp = 0.f;
		rasterizerState.DepthBiasSlopeFactor = slopeScaledDepthBias;
		rasterizerState.LineWidth = 1;

		*ppRasterizerState = (RasterizerState*)conf_malloc(sizeof(rasterizerState));
		memcpy(*ppRasterizerState, &rasterizerState, sizeof(rasterizerState));
	}

	void removeRasterizerState(RasterizerState* pRasterizerState)
	{
		SAFE_FREE(pRasterizerState);
	}
	// -------------------------------------------------------------------------------------------------
	// Buffer functions
	// -------------------------------------------------------------------------------------------------
	void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange = NULL)
	{
		ASSERT(pBuffer->mDesc.mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to map non-cpu accessible resource");

		uint64_t offset = pBuffer->pVkMemory->GetOffset();
		uint64_t size = pBuffer->pVkMemory->GetSize();
		if (pRange)
		{
			offset += pRange->mOffset;
			size = pRange->mSize;
		}

		VkResult vk_res = vkMapMemory(pRenderer->pDevice, pBuffer->pVkMemory->GetMemory(), offset, size, 0, &pBuffer->pCpuMappedAddress);
		ASSERT(vk_res == VK_SUCCESS && pBuffer->pCpuMappedAddress);
	}

	void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
	{
		ASSERT(pBuffer->mDesc.mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to unmap non-cpu accessible resource");

		vkUnmapMemory(pRenderer->pDevice, pBuffer->pVkMemory->GetMemory());
		pBuffer->pCpuMappedAddress = NULL;
	}
	// -------------------------------------------------------------------------------------------------
	// Command buffer functions
	// -------------------------------------------------------------------------------------------------
	void beginCmd(Cmd* pCmd)
	{
		ASSERT(pCmd);
		ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

		// reset buffer to conf_free memory
		vkResetCommandBuffer(pCmd->pVkCmdBuf, 0);

		DECLARE_ZERO(VkCommandBufferBeginInfo, begin_info);
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;;
		begin_info.pNext = NULL;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin_info.pInheritanceInfo = NULL;
		VkResult vk_res = vkBeginCommandBuffer(pCmd->pVkCmdBuf, &begin_info);
		ASSERT(VK_SUCCESS == vk_res);

		if (pCmd->pDescriptorPool)
			reset_descriptor_heap(pCmd->pCmdPool->pRenderer, pCmd->pDescriptorPool);
	}

	void endCmd(Cmd* pCmd)
	{
		ASSERT(pCmd);
		ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

		cmdFlushBarriers(pCmd);

		VkResult vk_res = vkEndCommandBuffer(pCmd->pVkCmdBuf);
		ASSERT(VK_SUCCESS == vk_res);
	}

	void cmdBeginRender(Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil, const LoadActionsDesc* pLoadActions/* = NULL*/)
	{
		ASSERT(pCmd);
		ASSERT(ppRenderTargets || pDepthStencil);
		ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

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
			};
			renderPassHash = tinystl::hash_state(hashValues, 3, renderPassHash);
			frameBufferHash = tinystl::hash_state(&ppRenderTargets[i]->pTexture->mTextureId, 1, frameBufferHash);
		}
		if (pDepthStencil)
		{
			uint32_t hashValues[] = {
				(uint32_t)pDepthStencil->mDesc.mFormat,
				(uint32_t)pDepthStencil->mDesc.mSampleCount,
				(uint32_t)pDepthStencil->mDesc.mSrgb,
			};
			renderPassHash = tinystl::hash_state(hashValues, 3, renderPassHash);
			frameBufferHash = tinystl::hash_state(&pDepthStencil->pTexture->mTextureId, 1, frameBufferHash);
		}

		RenderPassMap& renderPassMap = get_render_pass_map();
		FrameBufferMap& frameBufferMap = get_frame_buffer_map();

		const RenderPassMapNode* pNode = renderPassMap.find(renderPassHash).node;
		const FrameBufferMapNode* pFrameBufferNode = frameBufferMap.find(frameBufferHash).node;

		RenderPass* pRenderPass = NULL;
		FrameBuffer* pFrameBuffer = NULL;

		// If a render pass of this combination already exists just use it or create a new one
		if (pNode)
		{
			pRenderPass = pNode->second;
		}
		else
		{
			ImageFormat::Enum colorFormats[MAX_RENDER_TARGET_ATTACHMENTS] = {};
			bool srgbValues[MAX_RENDER_TARGET_ATTACHMENTS] = {};
			ImageFormat::Enum depthStencilFormat = ImageFormat::None;
			SampleCount sampleCount = renderTargetCount ? ppRenderTargets[0]->mDesc.mSampleCount : pDepthStencil->mDesc.mSampleCount;
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
			addRenderPass(pCmd->pCmdPool->pRenderer, &renderPassDesc, &pRenderPass);

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
			addFrameBuffer(pCmd->pCmdPool->pRenderer, &desc, &pFrameBuffer);

			// No need of a lock here since this map is per thread
			frameBufferMap.insert({ frameBufferHash, pFrameBuffer });
		}

		DECLARE_ZERO(VkRect2D, render_area);
		render_area.offset.x = 0;
		render_area.offset.y = 0;
		render_area.extent.width = pFrameBuffer->mWidth;
		render_area.extent.height = pFrameBuffer->mHeight;

		DECLARE_ZERO(VkRenderPassBeginInfo, begin_info);
		begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		begin_info.pNext = NULL;
		begin_info.renderPass = pRenderPass->pRenderPass;
		begin_info.framebuffer = pFrameBuffer->pFramebuffer;
		begin_info.renderArea = render_area;
		begin_info.clearValueCount = 0; // we will clear on our own
		begin_info.pClearValues = NULL;

		vkCmdBeginRenderPass(pCmd->pVkCmdBuf, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

		// Process clear actions
		if(pLoadActions)
		{
			DECLARE_ZERO(VkClearRect, rect);
			rect.baseArrayLayer = 0;
			rect.layerCount = pFrameBuffer->mArraySize;
			rect.rect.offset.x = 0;
			rect.rect.offset.y = 0;
			rect.rect.extent.width = pFrameBuffer->mWidth;
			rect.rect.extent.height = pFrameBuffer->mHeight;

			for (uint32_t i = 0; i < renderTargetCount; ++i)
			{
				if (pLoadActions->mLoadActionsColor[i] == LOAD_ACTION_CLEAR)
				{
					DECLARE_ZERO(VkClearAttachment, attachment);
					attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					attachment.colorAttachment = i;
					attachment.clearValue.color.float32[0] = pLoadActions->mClearColorValues[i].r;
					attachment.clearValue.color.float32[1] = pLoadActions->mClearColorValues[i].g;
					attachment.clearValue.color.float32[2] = pLoadActions->mClearColorValues[i].b;
					attachment.clearValue.color.float32[3] = pLoadActions->mClearColorValues[i].a;
					vkCmdClearAttachments(pCmd->pVkCmdBuf, 1, &attachment, 1, &rect);
				}
			}
			if (pLoadActions->mLoadActionDepth == LOAD_ACTION_CLEAR || pLoadActions->mLoadActionStencil == LOAD_ACTION_CLEAR)
			{
				VkImageAspectFlags flag = 0;
				if (pLoadActions->mLoadActionDepth == LOAD_ACTION_CLEAR)
					flag |= VK_IMAGE_ASPECT_DEPTH_BIT;
				if (pLoadActions->mLoadActionStencil == LOAD_ACTION_CLEAR)
					flag |= VK_IMAGE_ASPECT_STENCIL_BIT;

				DECLARE_ZERO(VkClearAttachment, attachment);
				attachment.aspectMask = flag;
				attachment.clearValue.depthStencil.depth = pLoadActions->mClearDepth.depth;
				attachment.clearValue.depthStencil.stencil = pLoadActions->mClearDepth.stencil;

				vkCmdClearAttachments(pCmd->pVkCmdBuf, 1, &attachment, 1, &rect);
			}
		}
	}

	void cmdEndRender(Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil)
	{
		ASSERT(pCmd);
		ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

		vkCmdEndRenderPass(pCmd->pVkCmdBuf);
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

		VkPipelineBindPoint pipeline_bind_point
			= (pPipeline->mType == PIPELINE_TYPE_COMPUTE) ? VK_PIPELINE_BIND_POINT_COMPUTE
			: VK_PIPELINE_BIND_POINT_GRAPHICS;

		vkCmdBindPipeline(pCmd->pVkCmdBuf, pipeline_bind_point, pPipeline->pVkPipeline);
	}

	void cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer)
	{
		ASSERT(pCmd);
		ASSERT(pBuffer);
		ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

		VkIndexType vk_index_type = (INDEX_TYPE_UINT16 == pBuffer->mDesc.mIndexType) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
		vkCmdBindIndexBuffer(pCmd->pVkCmdBuf, pBuffer->pVkBuffer, pBuffer->mPositionInHeap, vk_index_type);
	}

	void cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers)
	{
		ASSERT(pCmd);
		ASSERT(0 != bufferCount);
		ASSERT(ppBuffers);
		ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

		const uint32_t max_buffers = pCmd->pCmdPool->pRenderer->pVkActiveGPUProperties->limits.maxVertexInputBindings;
		uint32_t capped_buffer_count = bufferCount > max_buffers ? max_buffers : bufferCount;

		// No upper bound for this, so use 64 for now
		ASSERT(capped_buffer_count < 64);

		DECLARE_ZERO(VkBuffer, buffers[64]);
		DECLARE_ZERO(VkDeviceSize, offsets[64]);

		for (uint32_t i = 0; i < capped_buffer_count; ++i) {
			buffers[i] = ppBuffers[i]->pVkBuffer;
			offsets[i] = ppBuffers[i]->mPositionInHeap;
		}

		vkCmdBindVertexBuffers(pCmd->pVkCmdBuf, 0, capped_buffer_count, buffers, offsets);
	}

	void cmdDraw(Cmd* pCmd, uint32_t vertex_count, uint32_t first_vertex)
	{
		ASSERT(pCmd);
		ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

		vkCmdDraw(pCmd->pVkCmdBuf, vertex_count, 1, first_vertex, 0);
	}

	void cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount)
	{
		ASSERT(pCmd);
		ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

		vkCmdDraw(pCmd->pVkCmdBuf, vertexCount, instanceCount, firstVertex, 0);
	}

	void cmdDrawIndexed(Cmd* pCmd, uint32_t index_count, uint32_t first_index)
	{
		ASSERT(pCmd);
		ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

		vkCmdDrawIndexed(pCmd->pVkCmdBuf, index_count, 1, first_index, 0, 0);
	}

	void cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount)
	{
		ASSERT(pCmd);
		ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

		vkCmdDrawIndexed(pCmd->pVkCmdBuf, indexCount, instanceCount, firstIndex, 0, 0);
	}

	void cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
		ASSERT(pCmd);
		ASSERT(pCmd->pVkCmdBuf != VK_NULL_HANDLE);

		vkCmdDispatch(pCmd->pVkCmdBuf, groupCountX, groupCountY, groupCountZ);
	}

	void cmdResourceBarrier(Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers, bool batch)
	{
		VkImageMemoryBarrier* imageBarriers = numTextureBarriers ?
			(VkImageMemoryBarrier*)alloca(numTextureBarriers * sizeof(VkImageMemoryBarrier)) : NULL;
		uint32_t imageBarrierCount = 0;

		VkBufferMemoryBarrier* bufferBarriers = numBufferBarriers ?
			(VkBufferMemoryBarrier*)alloca(numBufferBarriers * sizeof(VkBufferMemoryBarrier)) : NULL;
		uint32_t bufferBarrierCount = 0;

		for (uint32_t i = 0; i < numBufferBarriers; ++i)
		{
			BufferBarrier* pTrans = &pBufferBarriers[i];
			Buffer* pBuffer = pTrans->pBuffer;
			if (!(pTrans->mNewState & pBuffer->mCurrentState))
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
			Texture* pTexture = pTrans->pTexture;
			if (!(pTrans->mNewState & pTexture->mCurrentState))
			{
				VkImageMemoryBarrier* pImageBarrier = &imageBarriers[imageBarrierCount++];
				pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				pImageBarrier->pNext = NULL;

				pImageBarrier->image = pTexture->pVkImage;
				pImageBarrier->subresourceRange.aspectMask = pTexture->mVkAspectMask;
				pImageBarrier->subresourceRange.baseMipLevel = 0;
				pImageBarrier->subresourceRange.levelCount = pTexture->mDesc.mMipLevels;
				pImageBarrier->subresourceRange.baseArrayLayer = 0;
				pImageBarrier->subresourceRange.layerCount = pTexture->mDesc.mArraySize * (pTexture->mDesc.mType == TEXTURE_TYPE_CUBE ? 6 : 1);

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
				memcpy(pCmd->pBatchBufferMemoryBarriers + pCmd->mBatchBufferMemoryBarrierCount, bufferBarriers, bufferBarrierCount * sizeof(VkBufferMemoryBarrier));
				pCmd->mBatchBufferMemoryBarrierCount += bufferBarrierCount;

				memcpy(pCmd->pBatchImageMemoryBarriers + pCmd->mBatchImageMemoryBarrierCount, imageBarriers, imageBarrierCount * sizeof(VkImageMemoryBarrier));
				pCmd->mBatchImageMemoryBarrierCount += imageBarrierCount;
			}
			else
			{
				VkPipelineStageFlags srcPipelineFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
				VkPipelineStageFlags dstPipelineFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
				vkCmdPipelineBarrier(pCmd->pVkCmdBuf, srcPipelineFlags, dstPipelineFlags, 0, 0, NULL, bufferBarrierCount, bufferBarriers, imageBarrierCount, imageBarriers);
			}
		}
	}

	void cmdSynchronizeResources(Cmd* pCmd, uint32_t numBuffers, Buffer** ppBuffers, uint32_t numTextures, Texture** ppTextures, bool batch)
	{
		VkImageMemoryBarrier* imageBarriers = numTextures ?
			(VkImageMemoryBarrier*)alloca(numTextures * sizeof(VkImageMemoryBarrier)) : NULL;
		uint32_t imageBarrierCount = 0;

		VkBufferMemoryBarrier* bufferBarriers = numBuffers ?
			(VkBufferMemoryBarrier*)alloca(numBuffers * sizeof(VkBufferMemoryBarrier)) : NULL;
		uint32_t bufferBarrierCount = 0;

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
			pImageBarrier->subresourceRange.levelCount = ppTextures[i]->mDesc.mMipLevels;
			pImageBarrier->subresourceRange.baseArrayLayer = 0;
			pImageBarrier->subresourceRange.layerCount = 1;

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
				memcpy(pCmd->pBatchBufferMemoryBarriers + pCmd->mBatchBufferMemoryBarrierCount, bufferBarriers, bufferBarrierCount * sizeof(VkBufferMemoryBarrier));
				pCmd->mBatchBufferMemoryBarrierCount += bufferBarrierCount;

				memcpy(pCmd->pBatchImageMemoryBarriers + pCmd->mBatchImageMemoryBarrierCount, imageBarriers, imageBarrierCount * sizeof(VkImageMemoryBarrier));
				pCmd->mBatchImageMemoryBarrierCount += imageBarrierCount;
			}
			else
			{
				VkPipelineStageFlags srcPipelineFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
				VkPipelineStageFlags dstPipelineFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
				vkCmdPipelineBarrier(pCmd->pVkCmdBuf, srcPipelineFlags, dstPipelineFlags, 0, 0, NULL, bufferBarrierCount, bufferBarriers, imageBarrierCount, imageBarriers);
			}
		}
	}

	void cmdFlushBarriers(Cmd* pCmd)
	{
		if (pCmd->mBatchBufferMemoryBarrierCount || pCmd->mBatchImageMemoryBarrierCount)
		{
			VkPipelineStageFlags srcPipelineFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			VkPipelineStageFlags dstPipelineFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			vkCmdPipelineBarrier(pCmd->pVkCmdBuf, srcPipelineFlags, dstPipelineFlags, 0, 0, NULL,
				pCmd->mBatchBufferMemoryBarrierCount, pCmd->pBatchBufferMemoryBarriers,
				pCmd->mBatchImageMemoryBarrierCount, pCmd->pBatchImageMemoryBarriers);

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

	void cmdUpdateSubresources(Cmd* pCmd, uint32_t startSubresource, uint32_t numSubresources, SubresourceDataDesc* pSubresources, Buffer* pIntermediate, uint64_t intermediateOffset, Texture* pTexture)
	{
		VkBufferImageCopy* pCopyRegions = (VkBufferImageCopy*)alloca(numSubresources * sizeof(VkBufferImageCopy));
		for (uint32_t i = startSubresource; i < startSubresource + numSubresources; ++i)
		{
			VkBufferImageCopy* pCopy = &pCopyRegions[i];
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

		vkCmdCopyBufferToImage(pCmd->pVkCmdBuf, pIntermediate->pVkBuffer, pTexture->pVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, numSubresources, pCopyRegions);
	}

	void cmdCopyBufferToTexture2d(Cmd* pCmd, uint32_t width, uint32_t height, uint32_t rowPitch, uint64_t bufferOffset, uint32_t mipLevel, Buffer* pBuffer, Texture* pTexture)
	{
		ASSERT(pCmd != NULL);
		ASSERT(pBuffer != NULL);
		ASSERT(pTexture != NULL);
		ASSERT(pCmd->pVkCmdBuf != VK_NULL_HANDLE);

		VkBufferImageCopy regions = { 0 };
		regions.bufferOffset = bufferOffset;
		regions.imageSubresource.aspectMask = pTexture->mVkAspectMask;
		regions.imageSubresource.mipLevel = mipLevel;
		regions.imageSubresource.baseArrayLayer = 0;
		regions.imageSubresource.layerCount = 1;
		regions.imageOffset.x = 0;
		regions.imageOffset.y = 0;
		regions.imageOffset.z = 0;
		regions.imageExtent.width = width;
		regions.imageExtent.height = height;
		regions.imageExtent.depth = 1;

		vkCmdCopyBufferToImage(pCmd->pVkCmdBuf, pBuffer->pVkBuffer, pTexture->pVkImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regions);
	}

	void acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex)
	{
		ASSERT(pRenderer);
		ASSERT(VK_NULL_HANDLE != pRenderer->pDevice);
		ASSERT(VK_NULL_HANDLE != pSwapChain->pSwapChain);
		ASSERT(pSignalSemaphore || pFence);

		VkResult vk_res = {};

		if (pFence != NULL)
		{
			vk_res = vkAcquireNextImageKHR(pRenderer->pDevice,
				pSwapChain->pSwapChain,
				UINT64_MAX,
				VK_NULL_HANDLE,
				pFence->pVkFence,
				pImageIndex);
			ASSERT(VK_SUCCESS == vk_res);

			vk_res = vkWaitForFences(pRenderer->pDevice, 1, &pFence->pVkFence, VK_TRUE, UINT64_MAX);
			ASSERT(VK_SUCCESS == vk_res);
		}
		else
		{
			vk_res = vkAcquireNextImageKHR(pRenderer->pDevice,
				pSwapChain->pSwapChain,
				UINT64_MAX,
				pSignalSemaphore->pVkSemaphore,
				VK_NULL_HANDLE,
				pImageIndex);
			ASSERT(VK_SUCCESS == vk_res);

			pSignalSemaphore->mSignaled = true;
		}
	}

	void queueSubmit(
		Queue*      pQueue,
		uint32_t       cmdCount,
		Cmd**       ppCmds,
		Fence* pFence,
		uint32_t       waitSemaphoreCount,
		Semaphore** ppWaitSemaphores,
		uint32_t       signalSemaphoreCount,
		Semaphore** ppSignalSemaphores
	)
	{
		ASSERT(pQueue);
		ASSERT(cmdCount > 0);
		ASSERT(ppCmds);
		if (waitSemaphoreCount > 0) {
			ASSERT(ppWaitSemaphores);
		}
		if (signalSemaphoreCount > 0) {
			ASSERT(ppSignalSemaphores);
		}

		//// Performance optimization for Windows
#if _WIN32_WINNT  < _WIN32_WINNT_WIN10
		vmaUnmapPersistentlyMappedMemory(pVmaAllocator);
#endif

		ASSERT(VK_NULL_HANDLE != pQueue->pVkQueue);

		cmdCount = cmdCount > MAX_SUBMIT_CMDS ? MAX_SUBMIT_CMDS : cmdCount;
		waitSemaphoreCount = waitSemaphoreCount > MAX_SUBMIT_WAIT_SEMAPHORES ? MAX_SUBMIT_WAIT_SEMAPHORES : waitSemaphoreCount;
		signalSemaphoreCount = signalSemaphoreCount > MAX_SUBMIT_SIGNAL_SEMAPHORES ? MAX_SUBMIT_SIGNAL_SEMAPHORES : signalSemaphoreCount;

		VkCommandBuffer* cmds = (VkCommandBuffer*)alloca(cmdCount * sizeof(VkCommandBuffer));
		for (uint32_t i = 0; i < cmdCount; ++i) {
			cmds[i] = ppCmds[i]->pVkCmdBuf;
		}

		VkSemaphore* wait_semaphores = waitSemaphoreCount ? (VkSemaphore*)alloca(waitSemaphoreCount * sizeof(VkSemaphore)) : NULL;
		VkPipelineStageFlags* wait_masks = (VkPipelineStageFlags*)alloca(waitSemaphoreCount * sizeof(VkPipelineStageFlags));
		uint32_t waitCount = 0;
		for (uint32_t i = 0; i < waitSemaphoreCount; ++i) {
			if (ppWaitSemaphores[i]->mSignaled)
			{
				wait_semaphores[waitCount] = ppWaitSemaphores[i]->pVkSemaphore;
				wait_masks[waitCount] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
				++waitCount;

				ppWaitSemaphores[i]->mSignaled = false;
			}
		}

		VkSemaphore* signal_semaphores = signalSemaphoreCount ? (VkSemaphore*)alloca(signalSemaphoreCount * sizeof(VkSemaphore)) : NULL;
		uint32_t signalCount = 0;
		for (uint32_t i = 0; i < signalSemaphoreCount; ++i) {
			if (!ppSignalSemaphores[i]->mSignaled)
			{
				signal_semaphores[signalCount] = ppSignalSemaphores[i]->pVkSemaphore;
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
		VkResult vk_res = vkQueueSubmit(pQueue->pVkQueue, 1, &submit_info, pFence->pVkFence);
		ASSERT(VK_SUCCESS == vk_res);

		pFence->mSubmitted = true;

#if _WIN32_WINNT  < _WIN32_WINNT_WIN10
		vmaMapPersistentlyMappedMemory(pVmaAllocator);
#endif
	}

	void queuePresent(Queue* pQueue, SwapChain* pSwapChain, uint32_t swapChainImageIndex, uint32_t waitSemaphoreCount, Semaphore** ppWaitSemaphores)
	{
		ASSERT(pQueue);
		if (waitSemaphoreCount > 0) {
			ASSERT(ppWaitSemaphores);
		}

		ASSERT(VK_NULL_HANDLE != pQueue->pVkQueue);

		Renderer* renderer = pQueue->pRenderer;

		VkSemaphore* wait_semaphores = waitSemaphoreCount ? (VkSemaphore*)alloca(waitSemaphoreCount * sizeof(VkSemaphore)) : NULL;
		waitSemaphoreCount = waitSemaphoreCount > MAX_PRESENT_WAIT_SEMAPHORES ? MAX_PRESENT_WAIT_SEMAPHORES : waitSemaphoreCount;
		uint32_t waitCount = 0;
		for (uint32_t i = 0; i < waitSemaphoreCount; ++i) {
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

	void waitForFences(Queue* pQueue, uint32_t fenceCount, Fence** ppFences)
	{
		ASSERT(pQueue);
		ASSERT(fenceCount);
		ASSERT(ppFences);

		VkFence* pFences = (VkFence*)alloca(fenceCount * sizeof(VkFence));
		uint32_t numValidFences = 0;
		for (uint32_t i = 0; i < fenceCount; ++i)
		{
			if (ppFences[i]->mSubmitted)
				pFences[numValidFences++] = ppFences[i]->pVkFence;
		}

		if (numValidFences)
		{
			vkWaitForFences(pQueue->pRenderer->pDevice, numValidFences, pFences, VK_TRUE, UINT64_MAX);
			vkResetFences(pQueue->pRenderer->pDevice, numValidFences, pFences);
		}

		for (uint32_t i = 0; i < fenceCount; ++i)
			ppFences[i]->mSubmitted = false;
	}

	void getFenceStatus(Fence* pFence, FenceStatus* pFenceStatus)
	{
		*pFenceStatus = FENCE_STATUS_COMPLETE;

		if (pFence->mSubmitted)
		{
			VkResult vkRes = vkGetFenceStatus(pFence->pRenderer->pDevice, pFence->pVkFence);
			if (vkRes == VK_SUCCESS)
			{
				vkResetFences(pFence->pRenderer->pDevice, 1, &pFence->pVkFence);
				pFence->mSubmitted = false;
			}

			*pFenceStatus = vkRes == VK_SUCCESS ? FENCE_STATUS_COMPLETE : FENCE_STATUS_INCOMPLETE;
		}
	}


	// -------------------------------------------------------------------------------------------------
	// Utility functions
	// -------------------------------------------------------------------------------------------------
	bool isImageFormatSupported(ImageFormat::Enum format)
	{
		bool result = false;
		switch (format) {
			// 1 channel
		case ImageFormat::R8: result = true; break;
		case ImageFormat::R16: result = true; break;
		case ImageFormat::R16F: result = true; break;
		case ImageFormat::R32UI: result = true; break;
		case ImageFormat::R32F: result = true; break;
			// 2 channel
		case ImageFormat::RG8: result = true; break;
		case ImageFormat::RG16: result = true; break;
		case ImageFormat::RG16F: result = true; break;
		case ImageFormat::RG32UI: result = true; break;
		case ImageFormat::RG32F: result = true; break;
			// 3 channel
		case ImageFormat::RGB8: result = true; break;
		case ImageFormat::RGB16: result = true; break;
		case ImageFormat::RGB16F: result = true; break;
		case ImageFormat::RGB32UI: result = true; break;
		case ImageFormat::RGB32F: result = true; break;
			// 4 channel
		case ImageFormat::BGRA8: result = true; break;
		case ImageFormat::RGBA16: result = true; break;
		case ImageFormat::RGBA16F: result = true; break;
		case ImageFormat::RGBA32UI: result = true; break;
		case ImageFormat::RGBA32F: result = true; break;
		}
		return result;
	}

	uint32_t calculateVertexLayoutStride(const VertexLayout* pVertexLayout)
	{
		ASSERT(pVertexLayout);

		uint32_t result = 0;
		for (uint32_t i = 0; i < pVertexLayout->mAttribCount; ++i) {
			result += calculateImageFormatStride(pVertexLayout->mAttribs[i].mFormat);
		}
		return result;
	}
	// -------------------------------------------------------------------------------------------------
	// Internal utility functions
	// -------------------------------------------------------------------------------------------------
	VkFilter util_to_vk_filter(FilterType filter)
	{
		switch (filter)
		{
		case FILTER_NEAREST:
			return VK_FILTER_NEAREST;
		case FILTER_LINEAR:
			return VK_FILTER_LINEAR;
		default:
			return VK_FILTER_LINEAR;
		}
	}

	VkSamplerMipmapMode util_to_vk_mip_map_mode(MipMapMode mipMapMode)
	{
		switch (mipMapMode)
		{
		case MIPMAP_MODE_NEAREST:
			return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case MIPMAP_MODE_LINEAR:
			return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		default:
			ASSERT(false && "Invalid Mip Map Mode");
			return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
		}
	}

	VkSamplerAddressMode util_to_vk_address_mode(AddressMode addressMode)
	{
		switch (addressMode)
		{
		case ADDRESS_MODE_MIRROR:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case ADDRESS_MODE_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case ADDRESS_MODE_CLAMP_TO_EDGE:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case ADDRESS_MODE_CLAMP_TO_BORDER:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		default:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}
	}

	// This function always returns zero for Vulkan for now because
	// of how the counter are configured.
	VkDeviceSize util_vk_determine_storage_counter_offset(VkDeviceSize buffer_size)
	{
		VkDeviceSize result = 0;
		return result;
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
				if (result == VK_FORMAT_R8G8B8A8_UNORM) result = VK_FORMAT_R8G8B8A8_SRGB;
				else if (result == VK_FORMAT_B8G8R8A8_UNORM) result = VK_FORMAT_B8G8R8A8_SRGB;
				else if (result == VK_FORMAT_B8G8R8_UNORM) result = VK_FORMAT_B8G8R8_SRGB;
				else if (result == VK_FORMAT_BC1_RGBA_UNORM_BLOCK) result = VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
				else if (result == VK_FORMAT_BC2_UNORM_BLOCK) result = VK_FORMAT_BC2_SRGB_BLOCK;
				else if (result == VK_FORMAT_BC3_UNORM_BLOCK) result = VK_FORMAT_BC3_SRGB_BLOCK;
				else if (result == VK_FORMAT_BC7_UNORM_BLOCK) result = VK_FORMAT_BC7_SRGB_BLOCK;
			}
		}

		return result;
	}

	ImageFormat::Enum util_to_internal_image_format(VkFormat format)
	{
		ImageFormat::Enum result = ImageFormat::None;
		switch (format) {
			// 1 channel
		case VK_FORMAT_R8_UNORM: result = ImageFormat::R8; break;
		case VK_FORMAT_R16_UNORM: result = ImageFormat::R16; break;
		case VK_FORMAT_R16_SFLOAT: result = ImageFormat::R16F; break;
		case VK_FORMAT_R32_UINT: result = ImageFormat::R32UI; break;
		case VK_FORMAT_R32_SFLOAT: result = ImageFormat::R32F; break;
			// 2 channel
		case VK_FORMAT_R8G8_UNORM: result = ImageFormat::RG8; break;
		case VK_FORMAT_R16G16_UNORM: result = ImageFormat::RG16; break;
		case VK_FORMAT_R16G16_SFLOAT: result = ImageFormat::RG16F; break;
		case VK_FORMAT_R32G32_UINT: result = ImageFormat::RG32UI; break;
		case VK_FORMAT_R32G32_SFLOAT: result = ImageFormat::RG32F; break;
			// 3 channel
		case VK_FORMAT_R8G8B8_UNORM:
		case VK_FORMAT_R8G8B8_SRGB:
			result = ImageFormat::RGB8; break;
		case VK_FORMAT_R16G16B16_UNORM: result = ImageFormat::RGB16; break;
		case VK_FORMAT_R16G16B16_SFLOAT: result = ImageFormat::RGB16F; break;
		case VK_FORMAT_R32G32B32_UINT: result = ImageFormat::RGB32UI; break;
		case VK_FORMAT_R32G32B32_SFLOAT: result = ImageFormat::RGB32F; break;
			// 4 channel
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SRGB:
			result = ImageFormat::BGRA8; break;
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SRGB:
			result = ImageFormat::RGBA8; break;
		case VK_FORMAT_R16G16B16A16_UNORM: result = ImageFormat::RGBA16; break;
		case VK_FORMAT_R16G16B16A16_SFLOAT: result = ImageFormat::RGBA16F; break;
		case VK_FORMAT_R32G32B32A32_UINT: result = ImageFormat::RGBA32UI; break;
		case VK_FORMAT_R32G32B32A32_SFLOAT: result = ImageFormat::RGBA32F; break;
			// Depth/stencil
		case VK_FORMAT_D16_UNORM: result = ImageFormat::D16; break;
		case VK_FORMAT_X8_D24_UNORM_PACK32: result = ImageFormat::X8D24PAX32; break;
		case VK_FORMAT_D32_SFLOAT: result = ImageFormat::D32F; break;
		case VK_FORMAT_S8_UINT: result = ImageFormat::S8; break;
		case VK_FORMAT_D16_UNORM_S8_UINT: result = ImageFormat::D16S8; break;
		case VK_FORMAT_D24_UNORM_S8_UINT: result = ImageFormat::D24S8; break;
		case VK_FORMAT_D32_SFLOAT_S8_UINT: result = ImageFormat::D32S8; break;
		}
		return result;
	}

	VkShaderStageFlags util_to_vk_shader_stages(ShaderStage shader_stages)
	{
		VkShaderStageFlags result = 0;
		if (SHADER_STAGE_ALL_GRAPHICS == (shader_stages & SHADER_STAGE_ALL_GRAPHICS)) {
			result = VK_SHADER_STAGE_ALL_GRAPHICS;
		}
		else {
			if (SHADER_STAGE_VERT == (shader_stages & SHADER_STAGE_VERT)) {
				result |= VK_SHADER_STAGE_VERTEX_BIT;
			}
			if (SHADER_STAGE_TESC == (shader_stages & SHADER_STAGE_TESC)) {
				result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
			}
			if (SHADER_STAGE_TESE == (shader_stages & SHADER_STAGE_TESE)) {
				result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
			}
			if (SHADER_STAGE_GEOM == (shader_stages & SHADER_STAGE_GEOM)) {
				result |= VK_SHADER_STAGE_GEOMETRY_BIT;
			}
			if (SHADER_STAGE_FRAG == (shader_stages & SHADER_STAGE_FRAG)) {
				result |= VK_SHADER_STAGE_FRAGMENT_BIT;
			}
			if (SHADER_STAGE_COMP == (shader_stages & SHADER_STAGE_COMP)) {
				result |= VK_SHADER_STAGE_COMPUTE_BIT;
			}
		}
		return result;
	}

	uint32_t util_vk_determine_image_channel_count(ImageFormat::Enum format)
	{
		uint32_t result = 0;
		switch (format) {
			// 1 channel
		case ImageFormat::R8: result = 1; break;
		case ImageFormat::R16: result = 1; break;
		case ImageFormat::R16F: result = 1; break;
		case ImageFormat::R32UI: result = 1; break;
		case ImageFormat::R32F: result = 1; break;
			// 2 channel
		case ImageFormat::RG8: result = 2; break;
		case ImageFormat::RG16: result = 2; break;
		case ImageFormat::RG16F: result = 2; break;
		case ImageFormat::RG32UI: result = 2; break;
		case ImageFormat::RG32F: result = 2; break;
			// 3 channel
		case ImageFormat::RGB8: result = 3; break;
		case ImageFormat::RGB16: result = 3; break;
		case ImageFormat::RGB16F: result = 3; break;
		case ImageFormat::RGB32UI: result = 3; break;
		case ImageFormat::RGB32F: result = 3; break;
			// 4 channel
		case ImageFormat::BGRA8: result = 4; break;
		case ImageFormat::RGBA8: result = 4; break;
		case ImageFormat::RGBA16: result = 4; break;
		case ImageFormat::RGBA16F: result = 4; break;
		case ImageFormat::RGBA32UI: result = 4; break;
		case ImageFormat::RGBA32F: result = 4; break;
			// Depth/stencil
		case ImageFormat::D16: result = 0; break;
		case ImageFormat::X8D24PAX32: result = 0; break;
		case ImageFormat::D32F: result = 0; break;
		case ImageFormat::S8: result = 0; break;
		case ImageFormat::D16S8: result = 0; break;
		case ImageFormat::D24S8: result = 0; break;
		case ImageFormat::D32S8: result = 0; break;
		}
		return result;
	}

	VkSampleCountFlagBits util_to_vk_sample_count(SampleCount sampleCount)
	{
		VkSampleCountFlagBits result = VK_SAMPLE_COUNT_1_BIT;
		switch (sampleCount) {
		case SAMPLE_COUNT_1: result = VK_SAMPLE_COUNT_1_BIT;  break;
		case SAMPLE_COUNT_2: result = VK_SAMPLE_COUNT_2_BIT;  break;
		case SAMPLE_COUNT_4: result = VK_SAMPLE_COUNT_4_BIT;  break;
		case SAMPLE_COUNT_8: result = VK_SAMPLE_COUNT_8_BIT;  break;
		case SAMPLE_COUNT_16: result = VK_SAMPLE_COUNT_16_BIT; break;
		}
		return result;
	}

	VkBufferUsageFlags util_to_vk_buffer_usage(BufferUsage usage)
	{
		VkBufferUsageFlags result = 0;
		if (usage & BUFFER_USAGE_UPLOAD) {
			result |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		}
		if (usage & BUFFER_USAGE_UNIFORM) {
			result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		}
		if (usage & BUFFER_USAGE_STORAGE_UAV) {
			result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		}
		if (usage & BUFFER_USAGE_STORAGE_SRV) {
			result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		}
		if (usage & BUFFER_USAGE_INDEX) {
			result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		}
		if (usage & BUFFER_USAGE_VERTEX) {
			result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		}
		if (usage & BUFFER_USAGE_INDIRECT) {
			result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
		}
		return result;
	}

	VkImageUsageFlags util_to_vk_image_usage(TextureUsage usage)
	{
		VkImageUsageFlags result = 0;
		if (TEXTURE_USAGE_SAMPLED_IMAGE == (usage & TEXTURE_USAGE_SAMPLED_IMAGE)) {
			result |= VK_IMAGE_USAGE_SAMPLED_BIT;
		}
		if (usage & TEXTURE_USAGE_UNORDERED_ACCESS) {
			result |= VK_IMAGE_USAGE_STORAGE_BIT;
		}
		return result;
	}

	VkAccessFlags util_to_vk_access_flags(ResourceState state)
	{
		VkAccessFlags ret = 0;
		if (state & RESOURCE_STATE_COPY_SOURCE) {
			ret |= VK_ACCESS_TRANSFER_READ_BIT;
		}
		if (state & RESOURCE_STATE_COPY_DEST) {
			ret |= VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		if (state & RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER) {
			ret |= VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		}
		if (state & RESOURCE_STATE_INDEX_BUFFER) {
			ret |= VK_ACCESS_INDEX_READ_BIT;
		}
		if (state & RESOURCE_STATE_UNORDERED_ACCESS) {
			ret |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		}
		if (state & RESOURCE_STATE_INDIRECT_ARGUMENT) {
			ret |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		}
		if (state & RESOURCE_STATE_RENDER_TARGET) {
			ret |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}
		if (state & RESOURCE_STATE_DEPTH_WRITE) {
			ret |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
		if ((state & RESOURCE_STATE_SHADER_RESOURCE) || (state & RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) {
			ret |= VK_ACCESS_SHADER_READ_BIT;
		}
		if (state & RESOURCE_STATE_PRESENT) {
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
		switch (format) {
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
		default:
			result = VK_IMAGE_ASPECT_COLOR_BIT;
			break;
		}
		return result;
	}

	VkFormatFeatureFlags util_vk_image_usage_to_format_features(VkImageUsageFlags usage)
	{
		VkFormatFeatureFlags result = (VkFormatFeatureFlags)0;
		if (VK_IMAGE_USAGE_SAMPLED_BIT == (usage & VK_IMAGE_USAGE_SAMPLED_BIT)) {
			result |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
		}
		if (VK_IMAGE_USAGE_STORAGE_BIT == (usage & VK_IMAGE_USAGE_STORAGE_BIT)) {
			result |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
		}
		if (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT == (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) {
			result |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
		}
		if (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT == (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
			result |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		return result;
	}

	VkQueueFlags util_to_vk_queue_flags(CmdPoolType cmdPoolType)
	{
		switch (cmdPoolType)
		{
		case CMD_POOL_DIRECT:
			return VK_QUEUE_GRAPHICS_BIT;
		case CMD_POOL_COPY:
			return VK_QUEUE_TRANSFER_BIT;
		case CMD_POOL_COMPUTE:
			return VK_QUEUE_COMPUTE_BIT;
		default:
			ASSERT(false && "Invalid Queue Type");
			return VK_QUEUE_FLAG_BITS_MAX_ENUM;
		}
	}
	// -------------------------------------------------------------------------------------------------
	// Internal init functions
	// -------------------------------------------------------------------------------------------------
	void CreateInstance(const char* app_name, Renderer* pRenderer)
	{
        uint32_t layerCount = 0;
        uint32_t count = 0;
		VkLayerProperties layers[100];
		VkExtensionProperties exts[100];
		vkEnumerateInstanceLayerProperties(&layerCount, NULL);
		vkEnumerateInstanceLayerProperties(&layerCount, layers);
		for (uint32_t i = 0; i < layerCount; ++i) {
			internal_log(LOG_TYPE_INFO, layers[i].layerName, "vkinstance-layer");
		}
		vkEnumerateInstanceExtensionProperties(NULL, &count, NULL);
		vkEnumerateInstanceExtensionProperties(NULL, &count, exts);
		for (uint32_t i = 0; i < count; ++i) {
			internal_log(LOG_TYPE_INFO, exts[i].extensionName, "vkinstance-ext");
		}

		DECLARE_ZERO(VkApplicationInfo, app_info);
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pNext = NULL;
		app_info.pApplicationName = app_name;
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName = "TheForge";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_MAKE_VERSION(1, 0, 51);

		// Instance
		{
           // check to see if the layers are present
           for(uint32_t i = 0; i < (uint32_t)pRenderer->mInstanceLayers.size(); ++i) {
                bool layerFound = false;
                for (uint32_t j = 0; j < layerCount; ++j) {
                    if(strcmp(pRenderer->mInstanceLayers[i], layers[j].layerName) == 0) {
                        layerFound = true;
                        break;
                    }
				}
                if(layerFound == false) {
                    internal_log(LOG_TYPE_WARN, pRenderer->mInstanceLayers[i], "vkinstance-layer-missing");
                    // deleate layer and get new index
                    i = (uint32_t)(pRenderer->mInstanceLayers.erase_unordered(pRenderer->mInstanceLayers.data() + i) - pRenderer->mInstanceLayers.data());
                }
            }

			uint32_t extension_count = 0;
            const int wanted_extenstion_count = sizeof(gVkWantedInstanceExtensions) / sizeof(gVkWantedInstanceExtensions[0]);
			// Layer extensions
			for (uint32_t i = 0; i < pRenderer->mInstanceLayers.size(); ++i) {
				const char* layer_name = pRenderer->mInstanceLayers[i];
				uint32_t count = 0;
				vkEnumerateInstanceExtensionProperties(layer_name, &count, NULL);
				VkExtensionProperties* properties = (VkExtensionProperties*)conf_calloc(count, sizeof(*properties));
				ASSERT(properties != NULL);
				vkEnumerateInstanceExtensionProperties(layer_name, &count, properties);
				for (uint32_t j = 0; j < count; ++j) {
                    for(uint32_t k = 0; k < wanted_extenstion_count; ++k) {
                        if(strcmp(gVkWantedInstanceExtensions[k], properties[j].extensionName) == 0) {
							pRenderer->gVkInstanceExtensions[extension_count++] = gVkWantedInstanceExtensions[k];
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
				uint32_t count = 0;
				vkEnumerateInstanceExtensionProperties(layer_name, &count, NULL);
				if (count > 0) {
					VkExtensionProperties* properties = (VkExtensionProperties*)conf_calloc(count, sizeof(*properties));
					ASSERT(properties != NULL);
					vkEnumerateInstanceExtensionProperties(layer_name, &count, properties);
					for (uint32_t j = 0; j < count; ++j) {
                       for(uint32_t k = 0; k < wanted_extenstion_count; ++k) {
                          if(strcmp(gVkWantedInstanceExtensions[k], properties[j].extensionName) == 0) {
                              pRenderer->gVkInstanceExtensions[extension_count++] = gVkWantedInstanceExtensions[k];
                              // clear wanted extenstion so we dont load it more then once
                              //gVkWantedInstanceExtensions[k] = "";
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
			VkResult vk_res = vkCreateInstance(&create_info, NULL, &(pRenderer->pVKInstance));
			ASSERT(VK_SUCCESS == vk_res);
		}

		// Load Vulkan instance functions
		volkLoadInstance(pRenderer->pVKInstance);

		// Debug
		{
			if ((vkCreateDebugReportCallbackEXT) && (vkDestroyDebugReportCallbackEXT) && (vkDebugReportMessageEXT)) {
				DECLARE_ZERO(VkDebugReportCallbackCreateInfoEXT, create_info);
				create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
				create_info.pNext = NULL;
				create_info.pfnCallback = internal_debug_report_callback;
				create_info.pUserData = NULL;
				create_info.flags = 
					VK_DEBUG_REPORT_WARNING_BIT_EXT |
					// VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | Performance warnings are not very vaild on desktop
					VK_DEBUG_REPORT_ERROR_BIT_EXT |
					VK_DEBUG_REPORT_DEBUG_BIT_EXT;
				VkResult res = vkCreateDebugReportCallbackEXT(pRenderer->pVKInstance, &create_info, NULL, &(pRenderer->pDebugReport));
				if (VK_SUCCESS != res) {
					internal_log(LOG_TYPE_ERROR, "vkCreateDebugReportCallbackEXT failed - disabling Vulkan debug callbacks", "internal_vk_init_instance");
					vkCreateDebugReportCallbackEXT = NULL;
					vkDestroyDebugReportCallbackEXT = NULL;
					vkDebugReportMessageEXT = NULL;
				}
			}
		}
	}

	void RemoveInstance(Renderer* pRenderer)
	{
		ASSERT(VK_NULL_HANDLE != pRenderer->pVKInstance);

		if ((vkDestroyDebugReportCallbackEXT) && (VK_NULL_HANDLE != pRenderer->pDebugReport)) {
			vkDestroyDebugReportCallbackEXT(pRenderer->pVKInstance, pRenderer->pDebugReport, NULL);
		}
		vkDestroyInstance(pRenderer->pVKInstance, NULL);
	}

	void AddDevice(Renderer* pRenderer)
	{
		ASSERT(VK_NULL_HANDLE != pRenderer->pVKInstance);

		VkResult vk_res = vkEnumeratePhysicalDevices(pRenderer->pVKInstance, &(pRenderer->mNumOfGPUs), NULL);
		ASSERT(VK_SUCCESS == vk_res);
		ASSERT(pRenderer->mNumOfGPUs > 0);

		if (pRenderer->mNumOfGPUs > MAX_GPUS) {
			pRenderer->mNumOfGPUs = MAX_GPUS;
		}

		vk_res = vkEnumeratePhysicalDevices(pRenderer->pVKInstance, &(pRenderer->mNumOfGPUs), pRenderer->pGPUs);
		ASSERT(VK_SUCCESS == vk_res);

		typedef struct VkQueueFamily
		{
			uint32_t		mQueueCount;
			uint32_t		mTotalQueues;
			VkQueueFlags	mQueueFlags;
		}VkQueueFamily;
		tinystl::vector<VkQueueFamily> queueFamilies;

		// Find gpu that supports atleast graphics
		pRenderer->mActiveGPUIndex = UINT32_MAX;
		DECLARE_ZERO(VkQueueFamilyProperties, mQueueProperties);
		for (uint32_t gpu_index = 0; gpu_index < pRenderer->mNumOfGPUs; ++gpu_index) {
			VkPhysicalDevice gpu = pRenderer->pGPUs[gpu_index];
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
				DECLARE_ZERO(VkQueueFamily, queueFamily);
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
				if (queueFamilies[i].mTotalQueues - queueFamilies[i].mQueueCount > 0 && (queueFamilies[i].mQueueFlags & VK_QUEUE_GRAPHICS_BIT) && graphics_queue_family_index == UINT32_MAX)
				{
					graphics_queue_family_index = i;
					graphics_queue_index = queueFamilies[i].mQueueCount;
					queueFamilies[i].mQueueCount++;
				}
			}

			//if no graphics queues then go to next gpu
			if (graphics_queue_family_index == UINT32_MAX)
				continue;


			if ((UINT32_MAX != graphics_queue_family_index)) {
				pRenderer->pActiveGPU = gpu;
				pRenderer->mActiveGPUIndex = gpu_index;
				break;
			}
		}

		ASSERT(VK_NULL_HANDLE != pRenderer->pActiveGPU);

		uint32_t count = 0;
		VkLayerProperties layers[100];
		VkExtensionProperties exts[100];
		vkEnumerateDeviceLayerProperties(pRenderer->pActiveGPU, &count, NULL);
		vkEnumerateDeviceLayerProperties(pRenderer->pActiveGPU, &count, layers);
		for (uint32_t i = 0; i < count; ++i) {
			internal_log(LOG_TYPE_INFO, layers[i].layerName, "vkdevice-layer");
			if (strcmp(layers[i].layerName, "VK_LAYER_RENDERDOC_Capture") == 0)
				gRenderDocLayerEnabled = true;
		}
		vkEnumerateDeviceExtensionProperties(pRenderer->pActiveGPU, NULL, &count, NULL);
		vkEnumerateDeviceExtensionProperties(pRenderer->pActiveGPU, NULL, &count, exts);
		for (uint32_t i = 0; i < count; ++i) {
			internal_log(LOG_TYPE_INFO, exts[i].extensionName, "vkdevice-ext");
		}

		for (uint32_t i = 0; i < pRenderer->mNumOfGPUs; ++i)
		{
			// Get memory properties
			vkGetPhysicalDeviceMemoryProperties(pRenderer->pGPUs[i], &(pRenderer->mVkGpuMemoryProperties[i]));

			// Get device properties
			vkGetPhysicalDeviceProperties(pRenderer->pGPUs[i], &(pRenderer->mVkGpuProperties[i]));

			// Get queue family properties
			vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->pGPUs[i], &(pRenderer->mVkQueueFamilyPropertyCount[i]), NULL);
			pRenderer->mVkQueueFamilyProperties[i] =
				(VkQueueFamilyProperties*)conf_calloc(pRenderer->mVkQueueFamilyPropertyCount[i], sizeof(VkQueueFamilyProperties));
			vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->pGPUs[i], &(pRenderer->mVkQueueFamilyPropertyCount[i]), pRenderer->mVkQueueFamilyProperties[i]);

			pRenderer->mGpuSettings[i].mUniformBufferAlignment = pRenderer->mVkGpuProperties[i].limits.minUniformBufferOffsetAlignment;
			pRenderer->mGpuSettings[i].mMaxVertexInputBindings = pRenderer->mVkGpuProperties[i].limits.maxVertexInputBindings;
			pRenderer->mGpuSettings[i].mMultiDrawIndirect = pRenderer->mVkGpuProperties[i].limits.maxDrawIndirectCount > 1;
		}

		pRenderer->pVkActiveGPUProperties = &pRenderer->mVkGpuProperties[pRenderer->mActiveGPUIndex];
		pRenderer->pVkActiveGpuMemoryProperties = &pRenderer->mVkGpuMemoryProperties[pRenderer->mActiveGPUIndex];
		pRenderer->pActiveGpuSettings = &pRenderer->mGpuSettings[pRenderer->mActiveGPUIndex];
		pRenderer->mVkActiveQueueFamilyPropertyCount = pRenderer->mVkQueueFamilyPropertyCount[pRenderer->mActiveGPUIndex];
		pRenderer->pVkActiveQueueFamilyProperties = pRenderer->mVkQueueFamilyProperties[pRenderer->mActiveGPUIndex];

		uint32_t queueCount = 1;

		// need a queue_priorite for each queue in the queue family we create
		tinystl::vector <tinystl::vector <float> > queue_priorities(queueFamilies.size());

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
		bool debugMarkerExtension = false;
		bool dedicatedAllocationExtension = false;
		bool memoryReq2Extension = false;
		bool externalMemoryExtension = false;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		bool externalMemoryWin32Extension = false;
#endif
		// Standalone extensions
		{
			const char* layer_name = NULL;
			uint32_t count = 0;
			const int wanted_extenstion_count = sizeof(gVkWantedDeviceExtensions) / sizeof(gVkWantedDeviceExtensions[0]);
			vkEnumerateDeviceExtensionProperties(pRenderer->pActiveGPU, layer_name, &count, NULL);
			if (count > 0) {
				VkExtensionProperties* properties = (VkExtensionProperties*)conf_calloc(count, sizeof(*properties));
				ASSERT(properties != NULL);
				vkEnumerateDeviceExtensionProperties(pRenderer->pActiveGPU, layer_name, &count, properties);
				for (uint32_t j = 0; j < count; ++j) {
					for (uint32_t k = 0; k < wanted_extenstion_count; ++k) {
						if (strcmp(gVkWantedDeviceExtensions[k], properties[j].extensionName) == 0) {
							pRenderer->gVkDeviceExtensions[extension_count++] = gVkWantedDeviceExtensions[k];
							if (strcmp(gVkWantedDeviceExtensions[k], VK_EXT_DEBUG_MARKER_EXTENSION_NAME) == 0)
								gDebugMarkerExtension = true;
							if (strcmp(gVkWantedDeviceExtensions[k], VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) == 0)
								dedicatedAllocationExtension = true;
							if (strcmp(gVkWantedDeviceExtensions[k], VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 0)
								memoryReq2Extension = true;
							if (strcmp(gVkWantedDeviceExtensions[k], VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME) == 0)
								externalMemoryExtension = true;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
							if (strcmp(gVkWantedDeviceExtensions[k], VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME) == 0)
								externalMemoryWin32Extension = true;
#endif
							if (strcmp(gVkWantedDeviceExtensions[k], VK_AMD_DRAW_INDIRECT_COUNT_EXTENSION_NAME) == 0)
								gDrawIndirectCountAMDExtension = true;
							break;
						}
					}
				}
				SAFE_FREE((void*)properties);
			}
		}

		// Add more extensions here
		VkPhysicalDeviceFeatures gpu_features = { 0 };
		vkGetPhysicalDeviceFeatures(pRenderer->pActiveGPU, &gpu_features);

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
		vk_res = vkCreateDevice(pRenderer->pActiveGPU, &create_info, NULL, &(pRenderer->pDevice));
		ASSERT(VK_SUCCESS == vk_res);

		// Load Vulkan device functions to bypass loader
		volkLoadDevice(pRenderer->pDevice);

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

		if (gDrawIndirectCountAMDExtension)
		{
			LOGINFOF("Successfully loaded AMD Draw Indirect extension");
		}
	}

	void RemoveDevice(Renderer* pRenderer)
	{
		for (uint32_t i = 0; i < pRenderer->mNumOfGPUs; ++i)
		{
			SAFE_FREE(pRenderer->mVkQueueFamilyProperties[i]);
		}

		vkDestroyDevice(pRenderer->pDevice, NULL);
	}
	// -------------------------------------------------------------------------------------------------
	// Indirect draw functions
	// -------------------------------------------------------------------------------------------------
	void addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc);

		CommandSignature* pCommandSignature = (CommandSignature*)conf_calloc(1, sizeof (CommandSignature));
		pCommandSignature->mDesc = *pDesc;

		for (uint32_t i = 0; i < pDesc->mIndirectArgCount; ++i)  // counting for all types;
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
			default:
				LOGERROR("Vulkan runtime only supports IndirectDraw, IndirectDrawIndex and IndirectDispatch at this point");
				break;
			}
		}

		pCommandSignature->mDrawCommandStride = round_up(pCommandSignature->mDrawCommandStride, 16);

		*ppCommandSignature = pCommandSignature;
	}

	void removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
	{
		SAFE_FREE(pCommandSignature);
	}

	void cmdExecuteIndirect(Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset)
	{
		if (pCommandSignature->mDrawType == INDIRECT_DRAW)
		{
			if (pCounterBuffer && vkCmdDrawIndirectCountAMD)
				vkCmdDrawIndirectCountAMD(pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset, pCounterBuffer->pVkBuffer, counterBufferOffset, maxCommandCount, pCommandSignature->mDrawCommandStride);
			else
				vkCmdDrawIndirect(pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset, maxCommandCount, pCommandSignature->mDrawCommandStride);
		}
		else if (pCommandSignature->mDrawType == INDIRECT_DRAW_INDEX)
		{
			if (pCounterBuffer && vkCmdDrawIndexedIndirectCountAMD)
				vkCmdDrawIndexedIndirectCountAMD(pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset, pCounterBuffer->pVkBuffer, counterBufferOffset, maxCommandCount, pCommandSignature->mDrawCommandStride);
			else
				vkCmdDrawIndexedIndirect(pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset, maxCommandCount, pCommandSignature->mDrawCommandStride);
		}
		else if (pCommandSignature->mDrawType == INDIRECT_DISPATCH)
		{
			vkCmdDispatchIndirect(pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset);
		}
	}
	/************************************************************************/
	// Debug Marker Implementation
	/************************************************************************/
	void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
	{
		if (gDebugMarkerExtension)
		{
			VkDebugMarkerMarkerInfoEXT markerInfo = {};
			markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
			markerInfo.color[0] = r;
			markerInfo.color[1] = g;
			markerInfo.color[2] = b;
			markerInfo.color[3] = 1.0f;
			markerInfo.pMarkerName = pName;
			vkCmdDebugMarkerBeginEXT(pCmd->pVkCmdBuf, &markerInfo);
		}
	}
	void cmdBeginDebugMarkerf(Cmd* pCmd, float r, float g, float b, const char* pFormat, ...)
	{
		va_list argptr;
		va_start(argptr, pFormat);
		char buffer[65536];
		vsnprintf_s(buffer, sizeof(buffer), pFormat, argptr);
		va_end(argptr);
		cmdBeginDebugMarker(pCmd, r, g, b, buffer);
	}

	void cmdEndDebugMarker(Cmd* pCmd)
	{
		if (gDebugMarkerExtension)
		{
			vkCmdDebugMarkerEndEXT(pCmd->pVkCmdBuf);
		}
	}

	void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
	{
		if (gDebugMarkerExtension)
		{
			VkDebugMarkerMarkerInfoEXT markerInfo = {};
			markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
			markerInfo.color[0] = r;
			markerInfo.color[1] = g;
			markerInfo.color[2] = b;
			markerInfo.color[3] = 1.0f;
			markerInfo.pMarkerName = pName;
			vkCmdDebugMarkerInsertEXT(pCmd->pVkCmdBuf, &markerInfo);
		}
	}
	/************************************************************************/
	// Resource Debug Naming Interface
	/************************************************************************/
	void setName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
	{
		ASSERT(pRenderer);
		ASSERT(pBuffer);
		ASSERT(pName);

		if (gDebugMarkerExtension)
		{
			VkDebugMarkerObjectNameInfoEXT nameInfo = {};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT;
			nameInfo.object = (uint64_t)pBuffer->pVkBuffer;
			nameInfo.pObjectName = pName;
			vkDebugMarkerSetObjectNameEXT(pRenderer->pDevice, &nameInfo);
		}
	}

	void setName(Renderer* pRenderer, Texture* pTexture, const char* pName)
	{
		ASSERT(pRenderer);
		ASSERT(pTexture);
		ASSERT(pName);

		if (gDebugMarkerExtension)
		{
			VkDebugMarkerObjectNameInfoEXT nameInfo = {};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
			nameInfo.object = (uint64_t)pTexture->pVkImage;
			nameInfo.pObjectName = pName;
			vkDebugMarkerSetObjectNameEXT(pRenderer->pDevice, &nameInfo);
		}
	}
	/************************************************************************/
	/************************************************************************/
#endif // RENDERER_IMPLEMENTATION

#if defined(__cplusplus) && defined(RENDERER_CPP_NAMESPACE)
} // namespace RENDERER_CPP_NAMESPACE
#endif

#include "../../../Common_3/ThirdParty/OpenSource/volk/volk.c"
#endif

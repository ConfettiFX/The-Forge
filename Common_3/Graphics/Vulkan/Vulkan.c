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

#ifdef VULKAN

#define MIN(a, b)        (((a) < (b)) ? (a) : (b))
#define MAX(a, b)        (((a) > (b)) ? (a) : (b))
#define CLAMP(v, mn, mx) MIN(MAX(v, mn), mx)
uint32_t round_up(uint32_t a, uint32_t b) { return (a + b - 1) & ~(b - 1); }
uint64_t round_up_64(uint64_t a, uint64_t b) { return (a + b - 1) & ~(b - 1); }

static inline uint32_t count_bits(uint32_t v)
{
    uint32_t c = v - ((v >> 1) & 0x55555555);
    c = ((c >> 2) & 0x33333333) + (c & 0x33333333);
    c = ((c >> 4) + c) & 0x0F0F0F0F;
    c = ((c >> 8) + c) & 0x00FF00FF;
    c = ((c >> 16) + c) & 0x0000FFFF;
    return c;
}

static inline size_t tf_mem_hash_uint8_t(const uint8_t* mem, size_t size, size_t prev)
{
    uint32_t result = (uint32_t)prev;
    while (size--)
        result = (result * 16777619) ^ *mem++;
    return (size_t)result;
}

/************************************************************************/
/************************************************************************/
#if defined(_WINDOWS)
#include <Windows.h>
#endif

#if defined(__linux__)
#define stricmp(a, b) strcasecmp(a, b)
#define vsprintf_s    vsnprintf
#define strncpy_s     strncpy
#endif

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#include "../../Utilities/Interfaces/ILog.h"

#include "../Interfaces/IGraphics.h"

//#include "../../Utilities/Math/MathTypes.h"

#if defined(__clang__)
// This warning is not consistent across platforms
#pragma clang diagnostic ignored "-Wmissing-braces"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-value"
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-const-variable"
#pragma clang diagnostic ignored "-Wswitch"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wswitch"
#endif

#include "../ThirdParty/OpenSource/VulkanMemoryAllocator/VulkanMemoryAllocator.h"
#include "../../../Common_3/Graphics/ThirdParty/OpenSource/ags/AgsHelper.h"
#include "../../../Common_3/Graphics/ThirdParty/OpenSource/nvapi/NvApiHelper.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

#include "../../Utilities/Math/AlgorithmsImpl.h"
#include "../../Utilities/Threading/Atomics.h"

#include "VulkanCapsBuilder.h"

#if defined(VK_USE_DISPATCH_TABLES)
#include "../../../Common_3/Graphics/ThirdParty/OpenSource/volk/volkForgeExt.h"
#endif

#if defined(GFX_ENABLE_SWAPPY)
#include "swappy/swappyVk.h"
#endif

#include "../../Utilities/Interfaces/IMemory.h"

#if defined(AUTOMATED_TESTING)
#include "../../Application/Interfaces/IScreenshot.h"
#endif

extern void vk_addRaytracingPipeline(const PipelineDesc*, Pipeline**);
extern void vk_FillRaytracingDescriptorData(AccelerationStructure* ppAccelerationStructures, VkAccelerationStructureKHR* pOutHandle);
static void SetVkObjectName(Renderer* pRenderer, uint64_t handle, VkObjectType type, VkDebugReportObjectTypeEXT typeExt, const char* pName);

#if VMA_STATS_STRING_ENABLED
extern void vmaInitBufferUsage(VmaAllocation alloc, const VkBufferCreateInfo* pCreateInfo, bool useKhrMaintenance5);
extern void vmaInitImageUsage(VmaAllocation alloc, const VkImageCreateInfo* pCreateInfo);
#endif

#define VK_FORMAT_VERSION(version, outVersionString)                     \
    ASSERT(VK_MAX_DESCRIPTION_SIZE == TF_ARRAY_COUNT(outVersionString)); \
    snprintf(outVersionString, 256, "%u.%u.%u", VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version), VK_VERSION_PATCH(version));

// clang-format off
VkBlendOp gVkBlendOpTranslator[MAX_BLEND_MODES] =
{
	VK_BLEND_OP_ADD,
	VK_BLEND_OP_SUBTRACT,
	VK_BLEND_OP_REVERSE_SUBTRACT,
	VK_BLEND_OP_MIN,
	VK_BLEND_OP_MAX,
};

VkBlendFactor gVkBlendConstantTranslator[MAX_BLEND_CONSTANTS] =
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

VkCompareOp gVkComparisonFuncTranslator[MAX_COMPARE_MODES] =
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

VkStencilOp gVkStencilOpTranslator[MAX_STENCIL_OPS] =
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

VkCullModeFlagBits gVkCullModeTranslator[MAX_CULL_MODES] =
{
	VK_CULL_MODE_NONE,
	VK_CULL_MODE_BACK_BIT,
	VK_CULL_MODE_FRONT_BIT
};

VkPolygonMode gVkFillModeTranslator[MAX_FILL_MODES] =
{
	VK_POLYGON_MODE_FILL,
	VK_POLYGON_MODE_LINE
};

VkFrontFace gVkFrontFaceTranslator[] =
{
	VK_FRONT_FACE_COUNTER_CLOCKWISE,
	VK_FRONT_FACE_CLOCKWISE
};

VkAttachmentLoadOp gVkAttachmentLoadOpTranslator[MAX_LOAD_ACTION] =
{
	VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	VK_ATTACHMENT_LOAD_OP_LOAD,
	VK_ATTACHMENT_LOAD_OP_CLEAR,
};

VkAttachmentStoreOp gVkAttachmentStoreOpTranslator[MAX_STORE_ACTION] =
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
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
	VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
#if defined(VK_USE_PLATFORM_XCB_KHR)
	VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
	VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#endif
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#endif
#if defined(VK_USE_PLATFORM_GGP)
	VK_GGP_STREAM_DESCRIPTOR_SURFACE_EXTENSION_NAME,
#endif
#if defined(VK_USE_PLATFORM_VI_NN)
	VK_NN_VI_SURFACE_EXTENSION_NAME,
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
	VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME,
	/************************************************************************/
	// Property querying extensions
	/************************************************************************/
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	/************************************************************************/
	/************************************************************************/
	// Debug utils not supported on all devices yet
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
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
#if defined(VK_USE_PLATFORM_GGP)
	VK_GGP_FRAME_TOKEN_EXTENSION_NAME,
#endif

	VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
	VK_EXT_DEVICE_FAULT_EXTENSION_NAME,
	// Fragment shader interlock extension to be used for ROV type functionality in Vulkan
	VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME,
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
    VK_AMD_BUFFER_MARKER_EXTENSION_NAME,
    VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME,
	/************************************************************************/
	// Multi GPU Extensions
	/************************************************************************/
	VK_KHR_DEVICE_GROUP_EXTENSION_NAME,
	/************************************************************************/
	// Bindless & Non Uniform access Extensions
	/************************************************************************/
	VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
	VK_KHR_MAINTENANCE3_EXTENSION_NAME,
	// Required by raytracing and the new bindless descriptor API if we use it in future
	VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    /************************************************************************/
    // Shader Atomic Int 64 Extension
    /************************************************************************/
    VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME,
	/************************************************************************/
	// Raytracing
	/************************************************************************/
	VK_KHR_RAY_QUERY_EXTENSION_NAME,
	VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
	// Required by VK_KHR_ray_tracing_pipeline
	VK_KHR_SPIRV_1_4_EXTENSION_NAME,
	// Required by VK_KHR_spirv_1_4
	VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,

	VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
	// Required by VK_KHR_acceleration_structure
	VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
	/************************************************************************/
	// YCbCr format support
	/************************************************************************/
	// Requirement for VK_KHR_sampler_ycbcr_conversion
	VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
	VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
	VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
	VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
	VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
	VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME,
	/************************************************************************/
	// Dynamic rendering
	/************************************************************************/
	VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
	VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,  // Required by VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
	VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,	  // Required by VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME
	VK_KHR_MULTIVIEW_EXTENSION_NAME,			  // Required by VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME

	VK_EXT_ASTC_DECODE_MODE_EXTENSION_NAME,
#if defined(GFX_DEVICE_MEMORY_TRACKING)
    VK_EXT_DEVICE_MEMORY_REPORT_EXTENSION_NAME,
#endif
	/************************************************************************/
	/************************************************************************/
	// Debug marker extension in case debug utils is not supported. Should be last element in the array
	VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
};
// clang-format on

// #NOTE: Keep the door open to disable the extension on buggy drivers as it is still new
bool gEnableDynamicRenderingExtension = true;

//-V:SAFE_FREE:779
#define SAFE_FREE(p_var)       \
    if (p_var)                 \
    {                          \
        tf_free((void*)p_var); \
        p_var = NULL;          \
    }

//-V:SAFE_FREE:547 - The TF memory manager might not appreciate having null passed to tf_free.
// Don't trigger PVS warnings about always-true/false on this macro

#if defined(GFX_DRIVER_MEMORY_TRACKING) || defined(GFX_DEVICE_MEMORY_TRACKING)

typedef enum VkTrackedObjectType
{
    VK_TRACKED_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_KHR = VK_OBJECT_TYPE_COMMAND_POOL + 1,
    VK_TRACKED_OBJECT_TYPE_SURFACE_KHR,
    VK_TRACKED_OBJECT_TYPE_SWAPCHAIN_KHR,
    VK_TRACKED_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT,
    VK_TRACKED_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT,
    VK_TRACKED_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR,
    VK_TRACKED_OBJECT_TYPE_COUNT
} VkTrackedObjectType;

uint32_t GetTrackedObjectTypeCount()
{
    COMPILE_ASSERT(VK_TRACKED_OBJECT_TYPE_COUNT <= TRACKED_OBJECT_TYPE_COUNT_MAX);
    return VK_TRACKED_OBJECT_TYPE_COUNT;
}

const char* GetTrackedObjectName(uint32_t typeIndex)
{
    static const char* vkTrackedObjectTypeNames[] = { "UNKNOWN",
                                                      "INSTANCE",
                                                      "PHYSICAL_DEVICE",
                                                      "DEVICE",
                                                      "QUEUE",
                                                      "SEMAPHORE",
                                                      "COMMAND_BUFFER",
                                                      "FENCE",
                                                      "DEVICE_MEMORY",
                                                      "BUFFER",
                                                      "IMAGE",
                                                      "EVENT",
                                                      "QUERY_POOL",
                                                      "BUFFER_VIEW",
                                                      "IMAGE_VIEW",
                                                      "SHADER_MODULE",
                                                      "PIPELINE_CACHE",
                                                      "PIPELINE_LAYOUT",
                                                      "RENDER_PASS",
                                                      "PIPELINE",
                                                      "DESCRIPTOR_SET_LAYOUT",
                                                      "SAMPLER",
                                                      "DESCRIPTOR_POOL",
                                                      "DESCRIPTOR_SET",
                                                      "FRAMEBUFFER",
                                                      "COMMAND_POOL",
                                                      "DESCRIPTOR_UPDATE_TEMPLATE",
                                                      "SURFACE_KHR",
                                                      "SWAPCHAIN_KHR",
                                                      "DEBUG_UTILS_MESSENGER",
                                                      "DEBUG_REPORT_CALLBACK",
                                                      "ACCELERATION_STRUCTURE" };
    COMPILE_ASSERT(TF_ARRAY_COUNT(vkTrackedObjectTypeNames) == VK_TRACKED_OBJECT_TYPE_COUNT);
    return vkTrackedObjectTypeNames[typeIndex];
}

VkTrackedObjectType GetTrackedObjectType(VkObjectType type)
{
    if (type > VK_OBJECT_TYPE_COMMAND_POOL)
    {
        switch (type)
        {
        case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_KHR:
            return VK_TRACKED_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_KHR;
        case VK_OBJECT_TYPE_SURFACE_KHR:
            return VK_TRACKED_OBJECT_TYPE_SURFACE_KHR;
        case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
            return VK_TRACKED_OBJECT_TYPE_SWAPCHAIN_KHR;
        case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:
            return VK_TRACKED_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT;
        case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT:
            return VK_TRACKED_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT;
        case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR:
            return VK_TRACKED_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
        default:
            ASSERTMSG(false,
                      "Unknown VkObjectType enum value %d. Please add it to VkTrackedObjectType, switch statement in "
                      "util_to_tracked_object_type.",
                      (int)type);
            return (VkTrackedObjectType)VK_OBJECT_TYPE_UNKNOWN;
        }
    }

    return (VkTrackedObjectType)type;
}

#endif

#if defined(GFX_DRIVER_MEMORY_TRACKING)

#define GFX_DRIVER_MEMORY_TRACKING_ALIGNMENT 32

typedef enum VkTrackedSystemAllocationScope
{
    VK_TRACKED_SYSTEM_ALLOCATION_SCOPE_COUNT = VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE + 1
} VkTrackedSystemAllocationScope;

bool gEnableDriverMemoryTracking = false;

typedef struct DriverMemoryStats
{
    // Total driver memory and allocation amount
    tfrg_atomic64_t mMemoryTotal;
    tfrg_atomic64_t mMemoryAllocCount;
    // Amount of driver memory for every object type
    tfrg_atomic64_t mMemoryPerObject[VK_TRACKED_OBJECT_TYPE_COUNT][VK_TRACKED_SYSTEM_ALLOCATION_SCOPE_COUNT];
    // Amount of allocations for every object type
    tfrg_atomic64_t mMemoryAllocsPerObject[VK_TRACKED_OBJECT_TYPE_COUNT][VK_TRACKED_SYSTEM_ALLOCATION_SCOPE_COUNT];
} DriverMemoryStats;

typedef struct VkMemoryHeader
{
    size_t                  size;
    VkSystemAllocationScope allocationScope;
    VkObjectType            type;
} VkMemoryHeader;

static DriverMemoryStats gDriverMemStats = { 0 };

uint64_t GetDriverAllocationsCount() { return gDriverMemStats.mMemoryAllocCount; }

uint64_t GetDriverMemoryAmount() { return gDriverMemStats.mMemoryTotal; }

uint64_t GetDriverAllocationsPerObject(uint32_t obj)
{
    uint64_t ret = 0;
    for (uint32_t i = 0; i < VK_TRACKED_SYSTEM_ALLOCATION_SCOPE_COUNT; i++)
    {
        ret += gDriverMemStats.mMemoryAllocsPerObject[obj][i];
    }
    return ret;
}

uint64_t GetDriverMemoryPerObject(uint32_t obj)
{
    uint64_t ret = 0;
    for (uint32_t i = 0; i < VK_TRACKED_SYSTEM_ALLOCATION_SCOPE_COUNT; i++)
    {
        ret += gDriverMemStats.mMemoryPerObject[obj][i];
    }
    return ret;
}

void* gVkAllocation(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
void* gVkReallocation(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
void  gVkFree(void* pUserData, void* pMemory);

#endif

VkAllocationCallbacks* GetAllocationCallbacks(VkObjectType type)
{
    UNREF_PARAM(type);

#if defined(GFX_DRIVER_MEMORY_TRACKING)
    if (!gEnableDriverMemoryTracking)
    {
        return NULL;
    }

    static VkAllocationCallbacks trackingCallbacks = {
        NULL,
        gVkAllocation,
        gVkReallocation,
        gVkFree,
    };

    static VkAllocationCallbacks objectCallbacks[VK_TRACKED_OBJECT_TYPE_COUNT];
    static uint32_t              objectUserData[VK_TRACKED_OBJECT_TYPE_COUNT] = { 0 };

    // Assign callbacks for each type of object, pass the type as the user data
    if (!objectCallbacks[0].pfnAllocation)
    {
        for (uint32_t c = 0; c < VK_TRACKED_OBJECT_TYPE_COUNT; ++c)
        {
            objectCallbacks[c] = trackingCallbacks;
            objectUserData[c] = c;
            objectCallbacks[c].pUserData = &objectUserData[c];
        }
    }

    uint32_t typeIndex = GetTrackedObjectType(type);
    return &objectCallbacks[typeIndex];
#else
    return NULL;
#endif
}

#if defined(GFX_DRIVER_MEMORY_TRACKING)
void* gVkAllocation(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    VkObjectType type = (VkObjectType)(*(uint32_t*)pUserData);

    tfrg_atomic64_add_relaxed(&gDriverMemStats.mMemoryPerObject[type][allocationScope], size);
    tfrg_atomic64_add_relaxed(&gDriverMemStats.mMemoryTotal, size);

    tfrg_atomic64_add_relaxed(&gDriverMemStats.mMemoryAllocsPerObject[type][allocationScope], 1);
    tfrg_atomic64_add_relaxed(&gDriverMemStats.mMemoryAllocCount, 1);

    alignment = MAX(alignment, GFX_DRIVER_MEMORY_TRACKING_ALIGNMENT);
    uint8_t*        ret = (uint8_t*)tf_memalign(alignment, size + alignment);
    VkMemoryHeader* header = (VkMemoryHeader*)ret;
    header->size = size;
    header->allocationScope = allocationScope;
    header->type = type;

    *(size_t*)(ret + alignment - sizeof(size_t)) = alignment;
    return ret + alignment;
}

void* gVkReallocation(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    if (!pOriginal)
    {
        VkObjectType type = (VkObjectType)(*(uint32_t*)(pUserData));
        return GetAllocationCallbacks(type)->pfnAllocation(pUserData, size, alignment, allocationScope);
    }
    uint8_t* mem = (uint8_t*)pOriginal;
    alignment = *(size_t*)(mem - sizeof(size_t));
    VkMemoryHeader* header = (VkMemoryHeader*)(mem - alignment);

    // Subtract previous size
    tfrg_atomic64_add_relaxed(&gDriverMemStats.mMemoryPerObject[header->type][header->allocationScope], -(int64_t)header->size);
    tfrg_atomic64_add_relaxed(&gDriverMemStats.mMemoryTotal, -(int64_t)header->size);

    // Add new size
    tfrg_atomic64_add_relaxed(&gDriverMemStats.mMemoryPerObject[header->type][header->allocationScope], size);
    tfrg_atomic64_add_relaxed(&gDriverMemStats.mMemoryTotal, size);

    uint8_t* ret = (uint8_t*)tf_realloc(header, size + alignment);
    header = (VkMemoryHeader*)ret;
    header->size = size;
    return ret + alignment;
}

void gVkFree(void* pUserData, void* pMemory)
{
    UNREF_PARAM(pUserData);
    if (!pMemory)
    {
        return;
    }

    uint8_t*        mem = (uint8_t*)pMemory;
    size_t          alignment = *(size_t*)(mem - sizeof(size_t));
    VkMemoryHeader* header = (VkMemoryHeader*)(mem - alignment);

    tfrg_atomic64_add_relaxed(&gDriverMemStats.mMemoryPerObject[header->type][header->allocationScope], -(int64_t)(header->size));
    tfrg_atomic64_add_relaxed(&gDriverMemStats.mMemoryAllocsPerObject[header->type][header->allocationScope], -(int64_t)1);
    tfrg_atomic64_add_relaxed(&gDriverMemStats.mMemoryTotal, -(int64_t)(header->size));
    tfrg_atomic64_add_relaxed(&gDriverMemStats.mMemoryAllocCount, -1);

    tf_free(header);
}
#endif

#if defined(GFX_DEVICE_MEMORY_TRACKING)

typedef struct VkObjectMemoryNode
{
    uint64_t key;
    uint64_t value;
} VkObjectMemoryNode;

bool gEnableDeviceMemoryTracking = true;

typedef struct DeviceMemoryStats
{
    // Total device memory and allocation amount
    tfrg_atomic64_t     mMemoryTotal;
    tfrg_atomic64_t     mMemoryAllocCount;
    // Amount of device memory for every object type
    tfrg_atomic64_t     mMemoryPerObject[VK_TRACKED_OBJECT_TYPE_COUNT];
    // Amount of device allocations for every object type
    tfrg_atomic64_t     mMemoryAllocsPerObject[VK_TRACKED_OBJECT_TYPE_COUNT];
    // Amount of memory per object id (stb hashmap, must be freed on exit)
    VkObjectMemoryNode* mMemoryMap;
    // Mutex for memorymap
    Mutex               mMemoryMapMutex;
} DeviceMemoryStats;

static DeviceMemoryStats gDeviceMemStats = { 0 };

uint64_t GetDeviceAllocationsCount() { return gDeviceMemStats.mMemoryAllocCount; }

uint64_t GetDeviceMemoryAmount() { return gDeviceMemStats.mMemoryTotal; }

uint64_t GetDeviceAllocationsPerObject(uint32_t obj) { return gDeviceMemStats.mMemoryAllocsPerObject[obj]; }

uint64_t GetDeviceMemoryPerObject(uint32_t obj) { return gDeviceMemStats.mMemoryPerObject[obj]; }

static void MemoryReportCallback(const VkDeviceMemoryReportCallbackDataEXT* pCallbackData, void* pUserData)
{
    UNREF_PARAM(pUserData);
    if (!pCallbackData)
    {
        return;
    }

    // Only process allocations and deallocations
    switch (pCallbackData->type)
    {
    case VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_ALLOCATE_EXT:
    case VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_FREE_EXT:
        break;
    default:
        return;
    }

    const VkTrackedObjectType objType = GetTrackedObjectType(pCallbackData->objectType);
    acquireMutex(&gDeviceMemStats.mMemoryMapMutex);
    int currObjIndex = (int)hmgeti(gDeviceMemStats.mMemoryMap, pCallbackData->memoryObjectId);

    if (pCallbackData->type == VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_ALLOCATE_EXT)
    {
        if (currObjIndex != -1)
        {
            // Realloc, subtract previous size, add new size
            const size_t oldSize = gDeviceMemStats.mMemoryMap[currObjIndex].value;
            tfrg_atomic64_add_relaxed(&gDeviceMemStats.mMemoryTotal, -(int64_t)oldSize);
            tfrg_atomic64_add_relaxed(&gDeviceMemStats.mMemoryPerObject[objType], -(int64_t)oldSize);

            tfrg_atomic64_add_relaxed(&gDeviceMemStats.mMemoryTotal, pCallbackData->size);
            tfrg_atomic64_add_relaxed(&gDeviceMemStats.mMemoryPerObject[objType], pCallbackData->size);

            gDeviceMemStats.mMemoryMap[currObjIndex].value = pCallbackData->size;
        }
        else
        {
            // New allocation
            tfrg_atomic64_add_relaxed(&gDeviceMemStats.mMemoryTotal, pCallbackData->size);
            tfrg_atomic64_add_relaxed(&gDeviceMemStats.mMemoryAllocCount, 1);
            tfrg_atomic64_add_relaxed(&gDeviceMemStats.mMemoryPerObject[objType], pCallbackData->size);
            tfrg_atomic64_add_relaxed(&gDeviceMemStats.mMemoryAllocsPerObject[objType], 1);

            hmput(gDeviceMemStats.mMemoryMap, pCallbackData->memoryObjectId, pCallbackData->size);
        }
    }

    // Deallocation
    if (pCallbackData->type == VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_FREE_EXT)
    {
        int objIndex = (int)hmgeti(gDeviceMemStats.mMemoryMap, pCallbackData->memoryObjectId);

        if (objIndex != -1)
        {
            uint64_t size = gDeviceMemStats.mMemoryMap[objIndex].value;

            tfrg_atomic64_add_relaxed(&gDeviceMemStats.mMemoryTotal, -(int64_t)size);
            tfrg_atomic64_add_relaxed(&gDeviceMemStats.mMemoryAllocCount, -(int64_t)1);
            tfrg_atomic64_add_relaxed(&gDeviceMemStats.mMemoryPerObject[objType], -(int64_t)size);
            tfrg_atomic64_add_relaxed(&gDeviceMemStats.mMemoryAllocsPerObject[objType], -(int64_t)1);

            hmdel(gDeviceMemStats.mMemoryMap, pCallbackData->memoryObjectId);
        }
    }
    releaseMutex(&gDeviceMemStats.mMemoryMapMutex);
}

#endif

#if VK_KHR_draw_indirect_count
PFN_vkCmdDrawIndirectCountKHR        pfnVkCmdDrawIndirectCountKHR = NULL;
PFN_vkCmdDrawIndexedIndirectCountKHR pfnVkCmdDrawIndexedIndirectCountKHR = NULL;
#else
PFN_vkCmdDrawIndirectCountAMD pfnVkCmdDrawIndirectCountKHR = NULL;
PFN_vkCmdDrawIndexedIndirectCountAMD pfnVkCmdDrawIndexedIndirectCountKHR = NULL;
#endif

// OpenXR functions declarations
#ifdef QUEST_VR

#define MAX_OPENXR_LAYERS 16
#define MAX_OPENXR_VIEWS  2
#define LEFT_EYE_VIEW     0
#define RIGHT_EYE_VIEW    1

typedef struct OpenXRSwapChainDesc
{
    XrSwapchain mSwapchainHandle;
    uint32_t    mWidth;
    uint32_t    mHeight;
} OpenXRSwapChainDesc;

typedef struct OpenXRPresentDesc
{
    OpenXRSwapChainDesc mMainView;
    OpenXRSwapChainDesc m2DLayer;
    VR2DLayerDesc       m2DLayerSettings;
} OpenXRPresentDesc;

// OpenXR functions shared with other system/platforms.
// Only tested on the Meta Quest platform.
extern XrInstance                    GetCurrentXRInstance();
extern XrSession                     GetCurrentXRSession();
extern XrSystemId                    GetXRSystemId();
extern XrFrameState*                 GetCurrentXRFrameState();
extern XrViewConfigurationProperties GetXRViewConfig();
extern uint32_t                      GetXRViewCount();
extern XrView*                       GetXRViews();
extern XrSpace                       GetXRLocalSpace();
extern XrSpace                       GetXRViewSpace();
extern XrResult                      VerifyOXRResult(XrResult res, const char* func, const char* cmd, bool assertOnErr);
extern void                          InitXRSession(Renderer* pRenderer);

// OpenXR specific debugging macro to check function results (similar to CHECK_VKRESULT)
#define CHECK_OXRRESULT(func) VerifyOXRResult(func, __FUNCTION__, #func, true)

// Functions to manage XRSwapchains
bool VerifySwapchainFormat(int64_t format);
void AddOpenXRSwapchain(uint32_t width, uint32_t height, bool multiView, int64_t format, uint32_t sampleCount,
                        SwapChainCreationFlags swapchainFlags, XrSwapchain* outSwapchain, uint32_t* outSwapchainImagesCount);
void RemoveOpenXRSwapchain(const XrSwapchain swapchainHandle);
void GetOpenXRSwapchainImages(const XrSwapchain swapchainHandle, uint32_t swapchainImagesCount,
                              XrSwapchainImageBaseHeader* outSwapchainImages);

bool DidRequestFoveation(SwapChainCreationFlags swapchainFlags);
void SetOpenXRFoveation(XrFoveationLevelFB level, float verticalOffset, XrFoveationDynamicFB dynamic, const XrSwapchain swapchainHandle);

void AcquireOpenXRSwapchainImage(const XrSwapchain swapchainHandle, uint32_t* outSwapchainImageIndex);
void ReleaseOpenXRSwapchainImage(const XrSwapchain swapchainHandle);

void BeginOpenXRFrame();
void EndOpenXRFrame(OpenXRPresentDesc* desc);

bool OpenXRAddVKInstanceExt(const char** instanceExtensionCache, uint* extensionCount, uint maxExtensionCount, char* pBuffer,
                            uint bufferSize);
bool OpenXRAddVKDeviceExt(const char** deviceExtensionCache, uint* extensionCount, uint maxExtensionCount, char* pBuffer, uint bufferSize);
bool OpenXRPostInitVKRenderer(Renderer* pRenderer, VkInstance instance, VkDevice device, uint8_t queueFamilyIndex);
void OpenXRAddVKSwapchain(Renderer* pRenderer, const SwapChainDesc* pDesc, bool multiView, SwapChain** ppSwapChain);
void OpenXRRemoveVKSwapchain(Renderer* pRenderer, SwapChain* pSwapChain);
void OpenXRVKQueuePresent(const QueuePresentDesc* pQueuePresentDesc);

#endif

/************************************************************************/
// IMPLEMENTATION
/************************************************************************/

void OnVkDeviceLost(Renderer* pRenderer)
{
    ASSERT(pRenderer);
    ASSERT(pRenderer->mVk.pDevice);

    if (!pRenderer->pGpu->mDeviceFaultSupported)
    {
        return;
    }

    VkDeviceFaultCountsEXT faultCounts = { VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT };
    VkResult               vkres = vkGetDeviceFaultInfoEXT(pRenderer->mVk.pDevice, &faultCounts, NULL);
    if (vkres != VK_SUCCESS)
    {
        LOGF(eERROR, "vkGetDeviceFaultInfoEXT returned %d when getting fault count, skipping VK_EXT_device_fault report...", vkres);
        return;
    }

    VkDeviceFaultInfoEXT faultInfo = { VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT };
    faultInfo.pVendorInfos = faultCounts.vendorInfoCount
                                 ? (VkDeviceFaultVendorInfoEXT*)tf_malloc(faultCounts.vendorInfoCount * sizeof(VkDeviceFaultVendorInfoEXT))
                                 : NULL;
    faultInfo.pAddressInfos =
        faultCounts.addressInfoCount
            ? (VkDeviceFaultAddressInfoEXT*)tf_malloc(faultCounts.addressInfoCount * sizeof(VkDeviceFaultAddressInfoEXT))
            : NULL;
    faultCounts.vendorBinarySize = 0;
    vkres = vkGetDeviceFaultInfoEXT(pRenderer->mVk.pDevice, &faultCounts, &faultInfo);
    if (vkres != VK_SUCCESS)
    {
        LOGF(eERROR, "vkGetDeviceFaultInfoEXT returned %d when getting fault info, skipping VK_EXT_device_fault report...", vkres);
    }
    else
    {
        LOGF(eINFO, "** Report from VK_EXT_device_fault **");
        LOGF(eINFO, "Description: %s", faultInfo.description);
        LOGF(eINFO, "Vendor infos");
        for (uint32_t vd = 0; vd < faultCounts.vendorInfoCount; ++vd)
        {
            const VkDeviceFaultVendorInfoEXT* vendorInfo = &faultInfo.pVendorInfos[vd];
            LOGF(eINFO, "Info[%u]", vd);
            LOGF(eINFO, "   Description: %s", vendorInfo->description);
            LOGF(eINFO, "   Fault code : %zu", (size_t)vendorInfo->vendorFaultCode);
            LOGF(eINFO, "   Fault data : %zu", (size_t)vendorInfo->vendorFaultData);
        }

        static const char* addressTypeNames[] = {
            "NONE",
            "READ_INVALID",
            "WRITE_INVALID",
            "EXECUTE_INVALID",
            "INSTRUCTION_POINTER_UNKNOWN",
            "INSTRUCTION_POINTER_INVALID",
            "INSTRUCTION_POINTER_FAULT",
        };
        LOGF(eINFO, "Address infos");
        for (uint32_t ad = 0; ad < faultCounts.addressInfoCount; ++ad)
        {
            const VkDeviceFaultAddressInfoEXT* addrInfo = &faultInfo.pAddressInfos[ad];
            // From https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkDeviceFaultAddressInfoEXT.html
            const VkDeviceAddress              lower = (addrInfo->reportedAddress & ~(addrInfo->addressPrecision - 1));
            const VkDeviceAddress              upper = (addrInfo->reportedAddress | (addrInfo->addressPrecision - 1));
            LOGF(eINFO, "Info[%u]", ad);
            LOGF(eINFO, "   Type            : %s", addressTypeNames[addrInfo->addressType]);
            LOGF(eINFO, "   Reported address: %zu", (size_t)addrInfo->reportedAddress);
            LOGF(eINFO, "   Lower address   : %zu", (size_t)lower);
            LOGF(eINFO, "   Upper address   : %zu", (size_t)upper);
            LOGF(eINFO, "   Precision       : %zu", (size_t)addrInfo->addressPrecision);
        }
    }

    SAFE_FREE(faultInfo.pVendorInfos);
    SAFE_FREE(faultInfo.pAddressInfos);
}

// Internal utility functions (may become external one day)
VkSampleCountFlagBits util_to_vk_sample_count(Renderer* pRenderer, SampleCount sampleCount);

VkColorSpaceKHR util_to_vk_colorspace(ColorSpace colorSpace);
ColorSpace      util_from_vk_colorspace(VkColorSpaceKHR colorSpace);

typedef struct SubresourceDataDesc SubresourceDataDesc;

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

/************************************************************************/
// Descriptor Pool Functions
/************************************************************************/
static void AddDescriptorPool(Renderer* pRenderer, uint32_t numDescriptorSets, VkDescriptorPoolCreateFlags flags,
                              const VkDescriptorPoolSize* pPoolSizes, uint32_t numPoolSizes, VkDescriptorPool* pPool)
{
    VkDescriptorPoolCreateInfo poolCreateInfo = { 0 };
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.pNext = NULL;
    poolCreateInfo.poolSizeCount = numPoolSizes;
    poolCreateInfo.pPoolSizes = pPoolSizes;
    poolCreateInfo.flags = flags;
    poolCreateInfo.maxSets = numDescriptorSets;
    CHECK_VKRESULT(
        vkCreateDescriptorPool(pRenderer->mVk.pDevice, &poolCreateInfo, GetAllocationCallbacks(VK_OBJECT_TYPE_DESCRIPTOR_POOL), pPool));
}

static void AllocateDescriptorSets(Renderer* pRenderer, VkDescriptorPool pPool, const VkDescriptorSetLayout* pLayouts,
                                   uint32_t numDescriptorSets, VkDescriptorSet** pSets)
{
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = pPool,
        .descriptorSetCount = numDescriptorSets,
        .pSetLayouts = pLayouts,
    };
    CHECK_VKRESULT(vkAllocateDescriptorSets(pRenderer->mVk.pDevice, &alloc_info, *pSets));
}

/************************************************************************/
/************************************************************************/
VkPipelineBindPoint gPipelineBindPoint[PIPELINE_TYPE_COUNT] = {
    VK_PIPELINE_BIND_POINT_MAX_ENUM,
    VK_PIPELINE_BIND_POINT_COMPUTE,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
};

struct DynamicUniformData
{
    VkBuffer pBuffer;
    uint32_t mOffset;
    uint32_t mSize;
};
/************************************************************************/
// Render Pass Implementation
/************************************************************************/
static const LoadActionType  gDefaultLoadActions[MAX_RENDER_TARGET_ATTACHMENTS] = { 0 };
static const StoreActionType gDefaultStoreActions[MAX_RENDER_TARGET_ATTACHMENTS] = { 0 };

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
    bool                   mDepthLazilyAllocated;
} RenderPassDesc;

typedef struct RenderPass
{
    VkRenderPass pRenderPass;
} RenderPass;

typedef struct FrameBuffer
{
    VkFramebuffer pFramebuffer;
    uint32_t      mWidth;
    uint32_t      mHeight;
    uint32_t      mArraySize;
} FrameBuffer;

#define VK_MAX_ATTACHMENT_ARRAY_COUNT ((MAX_RENDER_TARGET_ATTACHMENTS + 2) * 2)

static void AddRenderPass(Renderer* pRenderer, const RenderPassDesc* pDesc, RenderPass* pRenderPass)
{
    ASSERT(pRenderPass);
    memset(pRenderPass, 0, sizeof(RenderPass));
    /************************************************************************/
    // Add render pass
    /************************************************************************/
    bool hasDepthAttachmentCount = (pDesc->mDepthStencilFormat != TinyImageFormat_UNDEFINED);

    VkAttachmentDescription attachments[VK_MAX_ATTACHMENT_ARRAY_COUNT] = { { 0 } };
    VkAttachmentReference   colorAttachmentRefs[MAX_RENDER_TARGET_ATTACHMENTS] = { { 0 } };
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
    VkAttachmentReference resolveAttachmentRefs[MAX_RENDER_TARGET_ATTACHMENTS] = { { 0 } };
#endif
    VkAttachmentReference dsAttachmentRef = { 0 };
    uint32_t              attachmentCount = 0;

    VkSampleCountFlagBits sampleCount = util_to_vk_sample_count(pRenderer, pDesc->mSampleCount);

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
            attachmentRef->attachment = attachmentCount; //-V522
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
        // If we're only loading depth/stencil (for instance, z-prepass) the RenderPass layout should be READ_ONLY and not
        // ATTACHMENT_OPTIMAL)
        const bool noDepthWrites = pDesc->mStoreActionDepth == STORE_ACTION_NONE || pDesc->mStoreActionDepth == STORE_ACTION_DONTCARE;
        const bool noStencilWrites = pDesc->mStoreActionStencil == STORE_ACTION_NONE || pDesc->mStoreActionStencil == STORE_ACTION_DONTCARE;
        const bool isLoadingDepth = pDesc->mLoadActionDepth == LOAD_ACTION_LOAD;
        const bool isLoadingStencil = pDesc->mLoadActionStencil == LOAD_ACTION_LOAD;
        const bool isDepthLazilyAllocated = pDesc->mDepthLazilyAllocated;

        VkImageLayout imageLayout = !isDepthLazilyAllocated && noDepthWrites && noStencilWrites && (isLoadingDepth || isLoadingStencil)
                                        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                        : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        uint32_t      idx = attachmentCount++;
        attachments[idx].flags = 0;
        attachments[idx].format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mDepthStencilFormat);
        attachments[idx].samples = sampleCount;
        attachments[idx].loadOp = gVkAttachmentLoadOpTranslator[pDesc->mLoadActionDepth];
        attachments[idx].storeOp = gVkAttachmentStoreOpTranslator[pDesc->mStoreActionDepth];
        attachments[idx].stencilLoadOp = gVkAttachmentLoadOpTranslator[pDesc->mLoadActionStencil];
        attachments[idx].stencilStoreOp = gVkAttachmentStoreOpTranslator[pDesc->mStoreActionStencil];
        attachments[idx].initialLayout = imageLayout;
        attachments[idx].finalLayout = imageLayout;
        dsAttachmentRef.attachment = idx; //-V522
        dsAttachmentRef.layout = pDesc->mStoreActionDepth == STORE_ACTION_NONE && pDesc->mStoreActionStencil == STORE_ACTION_NONE
                                     ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                     : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    void* renderPassNext = NULL;
#ifdef QUEST_VR
    VkRenderPassFragmentDensityMapCreateInfoEXT fragDensityCreateInfo = { 0 };
    // Foveation / Fragment Density Maps
    if (pDesc->mVRFoveatedRendering && pRenderer->mVR.mIsFoveationEnabled)
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

    VkSubpassDescription subpass = {        
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .colorAttachmentCount = pDesc->mRenderTargetCount,
        .pColorAttachments = pDesc->mRenderTargetCount ? colorAttachmentRefs : NULL,
        .pDepthStencilAttachment = hasDepthAttachmentCount ? &dsAttachmentRef : NULL,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = NULL,
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
        .pResolveAttachments = pDesc->mRenderTargetCount ? resolveAttachmentRefs : NULL,
#else
        .pResolveAttachments = NULL,
#endif
    };

    VkRenderPassCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = renderPassNext,
        .flags = 0,
        .attachmentCount = attachmentCount,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 0,
        .pDependencies = NULL,
    };

#if defined(QUEST_VR)
    const uint                      viewMask = 0b11;
    VkRenderPassMultiviewCreateInfo multiviewCreateInfo = { 0 };

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

    CHECK_VKRESULT(vkCreateRenderPass(pRenderer->mVk.pDevice, &createInfo, GetAllocationCallbacks(VK_OBJECT_TYPE_RENDER_PASS),
                                      &(pRenderPass->pRenderPass)));
}

static void RemoveRenderPass(Renderer* pRenderer, RenderPass* pRenderPass)
{
    vkDestroyRenderPass(pRenderer->mVk.pDevice, pRenderPass->pRenderPass, GetAllocationCallbacks(VK_OBJECT_TYPE_RENDER_PASS));
}

static void AddFramebuffer(Renderer* pRenderer, VkRenderPass renderPass, const BindRenderTargetsDesc* pDesc, FrameBuffer* pFrameBuffer)
{
    ASSERT(pFrameBuffer);
    memset(pFrameBuffer, 0, sizeof(FrameBuffer));

    uint32_t colorAttachmentCount = pDesc->mRenderTargetCount;
    uint32_t depthAttachmentCount = pDesc->mDepthStencil.pDepthStencil ? 1 : 0;

    if (colorAttachmentCount)
    {
        const BindRenderTargetDesc* desc = &pDesc->mRenderTargets[0];
        pFrameBuffer->mWidth = desc->mUseMipSlice ? (desc->pRenderTarget->mWidth >> desc->mMipSlice) : desc->pRenderTarget->mWidth;
        pFrameBuffer->mHeight = desc->mUseMipSlice ? (desc->pRenderTarget->mHeight >> desc->mMipSlice) : desc->pRenderTarget->mHeight;
        if (desc->mUseArraySlice)
        {
            pFrameBuffer->mArraySize = 1;
        }
        else
        {
            pFrameBuffer->mArraySize = desc->pRenderTarget->mVRMultiview ? 1 : desc->pRenderTarget->mArraySize;
        }
    }
    else if (depthAttachmentCount)
    {
        const BindDepthTargetDesc* desc = &pDesc->mDepthStencil;
        pFrameBuffer->mWidth = desc->mUseMipSlice ? (desc->pDepthStencil->mWidth >> desc->mMipSlice) : desc->pDepthStencil->mWidth;
        pFrameBuffer->mHeight = desc->mUseMipSlice ? (desc->pDepthStencil->mHeight >> desc->mMipSlice) : desc->pDepthStencil->mHeight;
        if (desc->mUseArraySlice)
        {
            pFrameBuffer->mArraySize = 1;
        }
        else
        {
            pFrameBuffer->mArraySize = desc->pDepthStencil->mVRMultiview ? 1 : desc->pDepthStencil->mArraySize;
        }
    }
    else
    {
        ASSERT(pDesc->mExtent[0] && pDesc->mExtent[1]);
        pFrameBuffer->mWidth = pDesc->mExtent[0];
        pFrameBuffer->mHeight = pDesc->mExtent[1];
        pFrameBuffer->mArraySize = 1;
    }

    if (pDesc->mRenderTargetCount && pDesc->mRenderTargets[0].pRenderTarget->mDepth > 1)
    {
        pFrameBuffer->mArraySize = pDesc->mRenderTargets[0].pRenderTarget->mDepth;
    }
    else if (depthAttachmentCount && pDesc->mDepthStencil.pDepthStencil->mDepth > 1)
    {
        pFrameBuffer->mArraySize = pDesc->mDepthStencil.pDepthStencil->mDepth;
    }
    /************************************************************************/
    // Add frame buffer
    /************************************************************************/
    VkImageView  imageViews[VK_MAX_ATTACHMENT_ARRAY_COUNT] = { 0 };
    VkImageView* iterViews = imageViews;
    bool         vrFoveatedRendering = false;
    UNREF_PARAM(vrFoveatedRendering);

    // Color
    for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i)
    {
        const BindRenderTargetDesc* desc = &pDesc->mRenderTargets[i];
        vrFoveatedRendering |= desc->pRenderTarget->mVRFoveatedRendering;

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
        bool resolveAttachment = desc->pRenderTarget->pTexture->mLazilyAllocated && IsStoreActionResolve(desc->mStoreAction);
#endif
        if (!desc->mUseArraySlice && !desc->mUseMipSlice)
        {
            *iterViews = desc->pRenderTarget->mVk.pDescriptor;
            ++iterViews;

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
            if (resolveAttachment)
            {
                ASSERT(desc->pRenderTarget->pResolveAttachment);
                *iterViews = desc->pRenderTarget->pResolveAttachment->mVk.pDescriptor;
                ++iterViews;
            }
#endif
        }
        else
        {
            uint32_t handle = 0;
            if (desc->mUseMipSlice)
            {
                if (desc->mUseArraySlice)
                {
                    handle = desc->mMipSlice * desc->pRenderTarget->mArraySize + desc->mArraySlice;
                }
                else
                {
                    handle = desc->mMipSlice;
                }
            }
            else if (desc->mUseArraySlice)
            {
                handle = desc->mArraySlice;
            }
            *iterViews = desc->pRenderTarget->mVk.pSliceDescriptors[handle];
            ++iterViews;

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
            if (resolveAttachment)
            {
                ASSERT(desc->pRenderTarget->pResolveAttachment);
                *iterViews = desc->pRenderTarget->pResolveAttachment->mVk.pSliceDescriptors[handle];
                ++iterViews;
            }
#endif
        }
    }
    // Depth/stencil
    if (depthAttachmentCount)
    {
        const BindDepthTargetDesc* desc = &pDesc->mDepthStencil;
        vrFoveatedRendering |= desc->pDepthStencil->mVRFoveatedRendering;

        if (!desc->mUseArraySlice && !desc->mUseMipSlice)
        {
            *iterViews = desc->pDepthStencil->mVk.pDescriptor;
            ++iterViews;
        }
        else
        {
            uint32_t handle = 0;
            if (desc->mUseMipSlice)
            {
                if (desc->mUseArraySlice)
                {
                    handle = desc->mMipSlice * desc->pDepthStencil->mArraySize + desc->mArraySlice;
                }
                else
                {
                    handle = desc->mMipSlice;
                }
            }
            else if (desc->mUseArraySlice)
            {
                handle = desc->mArraySlice;
            }
            *iterViews = desc->pDepthStencil->mVk.pSliceDescriptors[handle];
            ++iterViews;
        }
    }

#if defined(QUEST_VR)
    if (vrFoveatedRendering && pRenderer->mVR.mIsFoveationEnabled)
    {
        RenderTarget* pFoveationFDM = pRenderer->mVR.pCurrentFDM;
        ASSERT(pFoveationFDM != NULL);
        *iterViews = pFoveationFDM->mVk.pDescriptor;
        ++iterViews;
    }
#endif

    VkFramebufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = renderPass,
        .attachmentCount = (uint32_t)(iterViews - imageViews),
        .pAttachments = imageViews,
        .width = pFrameBuffer->mWidth,
        .height = pFrameBuffer->mHeight,
        .layers = pFrameBuffer->mArraySize,
    };
    CHECK_VKRESULT(vkCreateFramebuffer(pRenderer->mVk.pDevice, &createInfo, GetAllocationCallbacks(VK_OBJECT_TYPE_FRAMEBUFFER),
                                       &(pFrameBuffer->pFramebuffer)));
    /************************************************************************/
    /************************************************************************/
}

static void RemoveFramebuffer(Renderer* pRenderer, FrameBuffer* pFrameBuffer)
{
    ASSERT(pRenderer);
    ASSERT(pFrameBuffer);

    vkDestroyFramebuffer(pRenderer->mVk.pDevice, pFrameBuffer->pFramebuffer, GetAllocationCallbacks(VK_OBJECT_TYPE_FRAMEBUFFER));
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
} RenderPassNode;

typedef struct FrameBufferNode
{
    uint64_t    key;
    FrameBuffer value;
} FrameBufferNode;

typedef struct ThreadRenderPassNode
{
    ThreadID         key;
    RenderPassNode** value; // pointer to stb_ds map
} ThreadRenderPassNode;

typedef struct ThreadFrameBufferNode
{
    ThreadID          key;
    FrameBufferNode** value; // pointer to stb_ds map
} ThreadFrameBufferNode;

typedef struct DescriptorLayoutNode
{
    uint64_t              key;
    VkDescriptorSetLayout value;
} DescriptorLayoutNode;

typedef struct ThreadDescriptorLayoutNode
{
    ThreadID               key;
    DescriptorLayoutNode** value; // pointer to stb_ds map
} ThreadDescriptorLayoutNode;

typedef struct PipelineLayout
{
    VkPipelineLayout pLayout;
    uint32_t         mLayoutCount;
    uint32_t         mEmptyLayouts : 31;
    uint32_t         mEmptyStaticSamplerLayout : 1;
    size_t           mDescSetsHashes[MAX_DESCRIPTOR_SETS];
} PipelineLayout;

typedef struct PipelineLayoutNode
{
    uint64_t        key;
    PipelineLayout* value;
} PipelineLayoutNode;

typedef struct ThreadPipelineLayoutNode
{
    ThreadID             key;
    PipelineLayoutNode** value; // pointer to stb_ds map
} ThreadPipelineLayoutNode;

typedef struct StaticSamplerNode
{
    uint64_t key;
    Sampler* value;
} StaticSamplerNode;

typedef struct ThreadStaticSamplerNode
{
    ThreadID            key;
    StaticSamplerNode** value; // pointer to stb_ds map
} ThreadStaticSamplerNode;

// RenderPass map per thread (this will make lookups lock free and we only need a lock when inserting a RenderPass Map for the first time)
static ThreadRenderPassNode*  gRenderPassMap[MAX_UNLINKED_GPUS] = { NULL };
// FrameBuffer map per thread (this will make lookups lock free and we only need a lock when inserting a FrameBuffer map for the first time)
static ThreadFrameBufferNode* gFrameBufferMap[MAX_UNLINKED_GPUS] = { NULL };
Mutex                         gRenderPassMutex[MAX_UNLINKED_GPUS];

static ThreadDescriptorLayoutNode* gDescriptorLayoutMap[MAX_UNLINKED_GPUS] = { NULL };
static ThreadPipelineLayoutNode*   gPipelineLayoutMap[MAX_UNLINKED_GPUS] = { NULL };
static ThreadStaticSamplerNode*    gStaticSamplerMap[MAX_UNLINKED_GPUS] = { NULL };
Mutex                              gDescriptorLayoutMutex[MAX_UNLINKED_GPUS];

// Returns pointer map
static RenderPassNode** get_render_pass_map(uint32_t rendererID)
{
    // Only need a lock when creating a new renderpass map for this thread
    acquireMutex(&gRenderPassMutex[rendererID]);
    ThreadRenderPassNode* threadMap = gRenderPassMap[rendererID];
    ThreadID              threadId = getCurrentThreadID();
    ThreadRenderPassNode* pNode = hmgetp_null(threadMap, threadId);
    if (!pNode)
    {
        // We need pointer to map, so that thread map can be reallocated without causing data races
        RenderPassNode** result = (RenderPassNode**)tf_calloc(1, sizeof(RenderPassNode*));
        RenderPassNode** ppResult = hmput(gRenderPassMap[rendererID], threadId, result);
        releaseMutex(&gRenderPassMutex[rendererID]);
        return ppResult;
    }
    else
    {
        releaseMutex(&gRenderPassMutex[rendererID]);
        return pNode->value;
    }
}

// Returns pointer to map
static FrameBufferNode** get_frame_buffer_map(uint32_t rendererID)
{
    // Only need a lock when creating a new framebuffer map for this thread
    acquireMutex(&gRenderPassMutex[rendererID]);
    ThreadID               threadId = getCurrentThreadID();
    ThreadFrameBufferNode* pNode = hmgetp_null(gFrameBufferMap[rendererID], threadId);
    if (!pNode)
    {
        FrameBufferNode** result = (FrameBufferNode**)tf_calloc(1, sizeof(FrameBufferNode*));
        FrameBufferNode** ppResult = hmput(gFrameBufferMap[rendererID], threadId, result);
        releaseMutex(&gRenderPassMutex[rendererID]);
        return ppResult;
    }
    else
    {
        releaseMutex(&gRenderPassMutex[rendererID]);
        return pNode->value;
    }
}

static DescriptorLayoutNode** GetDescriptorLayoutMap(uint32_t rendererID)
{
    // Only need a lock when creating a new framebuffer map for this thread
    acquireMutex(&gDescriptorLayoutMutex[rendererID]);
    ThreadID                    threadId = getCurrentThreadID();
    ThreadDescriptorLayoutNode* pNode = hmgetp_null(gDescriptorLayoutMap[rendererID], threadId);
    if (!pNode)
    {
        DescriptorLayoutNode** result = (DescriptorLayoutNode**)tf_calloc(1, sizeof(DescriptorLayoutNode*));
        DescriptorLayoutNode** ppResult = hmput(gDescriptorLayoutMap[rendererID], threadId, result);
        releaseMutex(&gDescriptorLayoutMutex[rendererID]);
        return ppResult;
    }
    else
    {
        releaseMutex(&gDescriptorLayoutMutex[rendererID]);
        return pNode->value;
    }
}

static PipelineLayoutNode** GetPipelineLayoutMap(uint32_t rendererID)
{
    // Only need a lock when creating a new framebuffer map for this thread
    acquireMutex(&gDescriptorLayoutMutex[rendererID]);
    ThreadID                  threadId = getCurrentThreadID();
    ThreadPipelineLayoutNode* pNode = hmgetp_null(gPipelineLayoutMap[rendererID], threadId);
    if (!pNode)
    {
        PipelineLayoutNode** result = (PipelineLayoutNode**)tf_calloc(1, sizeof(DescriptorLayoutNode*));
        PipelineLayoutNode** ppResult = hmput(gPipelineLayoutMap[rendererID], threadId, result);
        releaseMutex(&gDescriptorLayoutMutex[rendererID]);
        return ppResult;
    }
    else
    {
        releaseMutex(&gDescriptorLayoutMutex[rendererID]);
        return pNode->value;
    }
}

static StaticSamplerNode** GetStaticSamplerMap(uint32_t rendererID)
{
    // Only need a lock when creating a new framebuffer map for this thread
    acquireMutex(&gDescriptorLayoutMutex[rendererID]);
    ThreadID                 threadId = getCurrentThreadID();
    ThreadStaticSamplerNode* pNode = hmgetp_null(gStaticSamplerMap[rendererID], threadId);
    if (!pNode)
    {
        StaticSamplerNode** result = (StaticSamplerNode**)tf_calloc(1, sizeof(StaticSamplerNode*));
        StaticSamplerNode** ppResult = hmput(gStaticSamplerMap[rendererID], threadId, result);
        releaseMutex(&gDescriptorLayoutMutex[rendererID]);
        return ppResult;
    }
    else
    {
        releaseMutex(&gDescriptorLayoutMutex[rendererID]);
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

#if defined(ENABLE_GRAPHICS_VALIDATION)
// gAssertOnVkValidationError is used to work around a bug in the ovr mobile sdk.
// There is a fence creation struct that is not initialized in the sdk.
bool gAssertOnVkValidationError = true;

// Debug callback for Vulkan layers
static VkBool32 VKAPI_PTR DebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                                             VkDebugUtilsMessageTypeFlagsEXT             messageType,
                                             const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    UNREF_PARAM(messageType);
    UNREF_PARAM(pUserData);
    const char* pLayerPrefix = pCallbackData->pMessageIdName;
    const char* pMessage = pCallbackData->pMessage;
    int32_t     messageCode = pCallbackData->messageIdNumber;
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        LOGF(eINFO, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        LOGF(eWARNING, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        static const char* messageDataToIgnore[] = {
            // Bug in validation layer: https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/2521
            // #TODO: Remove once we upgrade to SDK with the fix
            "UNASSIGNED-CoreValidation-DrawState-QueryNotReset",
            // Per stage limits can be ignored as long as we are under the descriptor set limitss
            "maxPerStageDescriptor"
        };
        static const int32_t messageIdNumbersToIgnore[] = {
            // This ID is for validation of VkSwapChainCreateInfoKHR-imageExtent-01274
            // It is impossible to avoid this error with resizable window on X11
            // https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/1340
            2094043421,
            // The only way to ensure that image acquire semaphore is safe to delete is
            // to use VK_EXT_swapchain_maintenance1 which is not widely support at the time.
            // More information on issue can be found here:
            // https://github.com/KhronosGroup/Vulkan-Docs/issues/152
            // https://www.khronos.org/blog/resolving-longstanding-issues-with-wsi
            -1588160456,

        // These validation errors only happen in debug builds and are caused by the function vrapi_SubmitFrame2(pQuest->pOvr, &frameDesc)
        // in VrApiHooks.cpp when trying to present a frame.
        // Ignorning them because they have to do with the implementation of the function
        // and with VrApi being deprecated there won't be an update to fix it.
#if defined(QUEST_VR)
            369360862,
            448332540,
            1755645774
#endif
        };

        bool ignoreError = false;
        for (uint32_t m = 0; m < TF_ARRAY_COUNT(messageDataToIgnore); ++m)
        {
            if (strstr(pMessage, messageDataToIgnore[m]))
            {
                ignoreError = true;
                break;
            }
        }
        for (uint32_t m = 0; m < TF_ARRAY_COUNT(messageIdNumbersToIgnore); ++m)
        {
            if (messageIdNumbersToIgnore[m] == messageCode)
            {
                ignoreError = true;
                break;
            }
        }
        if (!ignoreError)
        {
            LOGF(eERROR, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
            if (gAssertOnVkValidationError)
            {
                ASSERT(false);
            }
        }
    }

    return VK_FALSE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
                                                          uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix,
                                                          const char* pMessage, void* pUserData)
{
    UNREF_PARAM(object);
    UNREF_PARAM(objectType);
    UNREF_PARAM(location);
    UNREF_PARAM(pUserData);
    if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    {
        LOGF(eINFO, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    {
        LOGF(eWARNING, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    {
        LOGF(eWARNING, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    {
        LOGF(eERROR, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
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
static inline FORGE_CONSTEXPR VkColorComponentFlags ToColorComponentFlags(ColorMask mask)
{
    VkColorComponentFlags ret = 0;
    if (mask & COLOR_MASK_RED)
    {
        ret |= VK_COLOR_COMPONENT_R_BIT;
    }
    if (mask & COLOR_MASK_GREEN)
    {
        ret |= VK_COLOR_COMPONENT_G_BIT;
    }
    if (mask & COLOR_MASK_BLUE)
    {
        ret |= VK_COLOR_COMPONENT_B_BIT;
    }
    if (mask & COLOR_MASK_ALPHA)
    {
        ret |= VK_COLOR_COMPONENT_A_BIT;
    }

    return ret;
}

static inline VkPipelineColorBlendStateCreateInfo util_to_blend_desc(const BlendStateDesc*                pDesc,
                                                                     VkPipelineColorBlendAttachmentState* pAttachments)
{
    int blendDescIndex = 0;
#if defined(ENABLE_GRAPHICS_RUNTIME_CHECK)

    for (uint32_t i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
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

    for (uint32_t i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
    {
        if (pDesc->mRenderTargetMask & (1 << i))
        {
            VkBool32 blendEnable = (gVkBlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]] != VK_BLEND_FACTOR_ONE ||
                                    gVkBlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]] != VK_BLEND_FACTOR_ZERO ||
                                    gVkBlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]] != VK_BLEND_FACTOR_ONE ||
                                    gVkBlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]] != VK_BLEND_FACTOR_ZERO);

            pAttachments[i].blendEnable = blendEnable;
            pAttachments[i].colorWriteMask = ToColorComponentFlags(pDesc->mColorWriteMasks[blendDescIndex]);
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

    VkPipelineColorBlendStateCreateInfo cb = { 0 };
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
    ASSERT(pDesc->mDepthFunc < MAX_COMPARE_MODES);
    ASSERT(pDesc->mStencilFrontFunc < MAX_COMPARE_MODES);
    ASSERT(pDesc->mStencilFrontFail < MAX_STENCIL_OPS);
    ASSERT(pDesc->mDepthFrontFail < MAX_STENCIL_OPS);
    ASSERT(pDesc->mStencilFrontPass < MAX_STENCIL_OPS);
    ASSERT(pDesc->mStencilBackFunc < MAX_COMPARE_MODES);
    ASSERT(pDesc->mStencilBackFail < MAX_STENCIL_OPS);
    ASSERT(pDesc->mDepthBackFail < MAX_STENCIL_OPS);
    ASSERT(pDesc->mStencilBackPass < MAX_STENCIL_OPS);

    VkPipelineDepthStencilStateCreateInfo ds = { 0 };
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
    ds.front.compareOp = (VkCompareOp)pDesc->mStencilFrontFunc;
    ds.front.compareMask = pDesc->mStencilReadMask;
    ds.front.writeMask = pDesc->mStencilWriteMask;
    ds.front.reference = 0;

    ds.back.failOp = gVkStencilOpTranslator[pDesc->mStencilBackFail];
    ds.back.passOp = gVkStencilOpTranslator[pDesc->mStencilBackPass];
    ds.back.depthFailOp = gVkStencilOpTranslator[pDesc->mDepthBackFail];
    ds.back.compareOp = gVkComparisonFuncTranslator[pDesc->mStencilBackFunc];
    ds.back.compareMask = pDesc->mStencilReadMask;
    ds.back.writeMask = pDesc->mStencilWriteMask; // devsh fixed
    ds.back.reference = 0;

    ds.minDepthBounds = 0;
    ds.maxDepthBounds = 1;

    return ds;
}

static inline VkPipelineRasterizationStateCreateInfo util_to_rasterizer_desc(Renderer* pRenderer, const RasterizerStateDesc* pDesc)
{
    ASSERT(pDesc->mFillMode < MAX_FILL_MODES);
    ASSERT(pDesc->mCullMode < MAX_CULL_MODES);
    ASSERT(pDesc->mFrontFace == FRONT_FACE_CCW || pDesc->mFrontFace == FRONT_FACE_CW);

    VkPipelineRasterizationStateCreateInfo rs = { 0 };
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.pNext = NULL;
    rs.flags = 0;
    rs.depthClampEnable = pDesc->mDepthClampEnable ? VK_TRUE : VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode = gVkFillModeTranslator[pDesc->mFillMode];
    if (!pRenderer->pGpu->mFillModeNonSolid && VK_POLYGON_MODE_FILL != rs.polygonMode)
    {
        LOGF(eWARNING,
             "VkPolygonMode %i not supported on this device. VK_POLYGON_MODE_FILL is the only supported polygon mode. Falling back to that",
             rs.polygonMode);
        rs.polygonMode = VK_POLYGON_MODE_FILL;
    }
    rs.cullMode = gVkCullModeTranslator[pDesc->mCullMode];
    rs.frontFace = gVkFrontFaceTranslator[pDesc->mFrontFace];
    rs.depthBiasEnable = (pDesc->mDepthBias != 0) ? VK_TRUE : VK_FALSE;
    rs.depthBiasConstantFactor = (float)pDesc->mDepthBias;
    rs.depthBiasClamp = 0.0f;
    rs.depthBiasSlopeFactor = pDesc->mSlopeScaledDepthBias;
    rs.lineWidth = 1;

    return rs;
}
/************************************************************************/
// Create default resources to be used a null descriptors in case user does not specify some descriptors
/************************************************************************/
static VkPipelineRasterizationStateCreateInfo gDefaultRasterizerDesc = { 0 };
static VkPipelineDepthStencilStateCreateInfo  gDefaultDepthDesc = { 0 };
static VkPipelineColorBlendStateCreateInfo    gDefaultBlendDesc = { 0 };
static VkPipelineColorBlendAttachmentState    gDefaultBlendAttachments[MAX_RENDER_TARGET_ATTACHMENTS] = { { 0 } };

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
    Queue*   pInitialTransitionQueue[MAX_LINKED_GPUS];
    CmdPool* pInitialTransitionCmdPool[MAX_LINKED_GPUS];
    Cmd*     pInitialTransitionCmd[MAX_LINKED_GPUS];
    Fence*   pInitialTransitionFence[MAX_LINKED_GPUS];
} NullDescriptors;

static void TransitionInitialLayout(Renderer* pRenderer, Texture* pTexture, ResourceState startState)
{
    const uint32_t nodeIndex = GPU_MODE_LINKED == pRenderer->mGpuMode ? pTexture->mNodeIndex : 0;
    acquireMutex(&pRenderer->pNullDescriptors->mInitialTransitionMutex);
    Cmd* cmd = pRenderer->pNullDescriptors->pInitialTransitionCmd[nodeIndex];
    resetCmdPool(pRenderer, pRenderer->pNullDescriptors->pInitialTransitionCmdPool[nodeIndex]);
    beginCmd(cmd);
    TextureBarrier barrier = { pTexture, RESOURCE_STATE_UNDEFINED, startState };
    cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
    endCmd(cmd);
    QueueSubmitDesc submitDesc = { 0 };
    submitDesc.mCmdCount = 1;
    submitDesc.ppCmds = &cmd;
    submitDesc.pSignalFence = pRenderer->pNullDescriptors->pInitialTransitionFence[nodeIndex];
    queueSubmit(pRenderer->pNullDescriptors->pInitialTransitionQueue[nodeIndex], &submitDesc);
    waitForFences(pRenderer, 1, &pRenderer->pNullDescriptors->pInitialTransitionFence[nodeIndex]);
    releaseMutex(&pRenderer->pNullDescriptors->mInitialTransitionMutex);
}

static void ResetQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
    const uint32_t nodeIndex = GPU_MODE_LINKED == pRenderer->mGpuMode ? pQueryPool->mVk.mNodeIndex : 0;
    acquireMutex(&pRenderer->pNullDescriptors->mInitialTransitionMutex);
    Cmd* cmd = pRenderer->pNullDescriptors->pInitialTransitionCmd[nodeIndex];
    resetCmdPool(pRenderer, pRenderer->pNullDescriptors->pInitialTransitionCmdPool[nodeIndex]);
    beginCmd(cmd);
    vkCmdResetQueryPool(cmd->mVk.pCmdBuf, pQueryPool->mVk.pQueryPool, 0, pQueryPool->mCount);
    endCmd(cmd);
    QueueSubmitDesc submitDesc = { 0 };
    submitDesc.mCmdCount = 1;
    submitDesc.ppCmds = &cmd;
    submitDesc.pSignalFence = pRenderer->pNullDescriptors->pInitialTransitionFence[nodeIndex];
    queueSubmit(pRenderer->pNullDescriptors->pInitialTransitionQueue[nodeIndex], &submitDesc);
    waitForFences(pRenderer, 1, &pRenderer->pNullDescriptors->pInitialTransitionFence[nodeIndex]);
    releaseMutex(&pRenderer->pNullDescriptors->mInitialTransitionMutex);
}

static void add_default_resources(Renderer* pRenderer)
{
    initMutex(&pRenderer->pNullDescriptors->mSubmitMutex);

    for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
    {
        uint32_t    nodeIndex = pRenderer->mGpuMode == GPU_MODE_UNLINKED ? pRenderer->mUnlinkedRendererIndex : i;
        // 1D texture
        TextureDesc textureDesc = { 0 };
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
        textureDesc.pName = "DefaultTextureSRV_1D";
        addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_1D]);
        textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
        textureDesc.pName = "DefaultTextureUAV_1D";
        addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][TEXTURE_DIM_1D]);

        // 1D texture array
        textureDesc.mArraySize = 2;
        textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        textureDesc.pName = "DefaultTextureSRV_1D_ARRAY";
        addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_1D_ARRAY]);
        textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
        textureDesc.pName = "DefaultTextureUAV_1D_ARRAY";
        addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][TEXTURE_DIM_1D_ARRAY]);

        // 2D texture
        textureDesc.mWidth = 2;
        textureDesc.mHeight = 2;
        textureDesc.mArraySize = 1;
        textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        textureDesc.pName = "DefaultTextureSRV_2D";
        addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_2D]);
        textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
        textureDesc.pName = "DefaultTextureUAV_2D";
        addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][TEXTURE_DIM_2D]);

        // 2D MS texture
        textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        textureDesc.mSampleCount = SAMPLE_COUNT_4;
        textureDesc.pName = "DefaultTextureSRV_2DMS";
        addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_2DMS]);
        textureDesc.mSampleCount = SAMPLE_COUNT_1;

        // 2D texture array
        textureDesc.mArraySize = 2;
        textureDesc.pName = "DefaultTextureSRV_2D_ARRAY";
        addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_2D_ARRAY]);
        textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
        textureDesc.pName = "DefaultTextureUAV_2D_ARRAY";
        addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][TEXTURE_DIM_2D_ARRAY]);

        // 2D MS texture array
        textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        textureDesc.mSampleCount = SAMPLE_COUNT_4;
        textureDesc.pName = "DefaultTextureSRV_2DMS_ARRAY";
        addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_2DMS_ARRAY]);
        textureDesc.mSampleCount = SAMPLE_COUNT_1;

        // 3D texture
        textureDesc.mDepth = 2;
        textureDesc.mArraySize = 1;
        textureDesc.pName = "DefaultTextureSRV_3D";
        addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_3D]);
        textureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
        textureDesc.pName = "DefaultTextureUAV_3D";
        addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureUAV[i][TEXTURE_DIM_3D]);

        // Cube texture
        textureDesc.mDepth = 1;
        textureDesc.mArraySize = 6;
        textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE;
        textureDesc.pName = "DefaultTextureSRV_CUBE";
        addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_CUBE]);
        textureDesc.mArraySize = 6 * 2;
        textureDesc.pName = "DefaultTextureSRV_CUBE_ARRAY";
        addTexture(pRenderer, &textureDesc, &pRenderer->pNullDescriptors->pDefaultTextureSRV[i][TEXTURE_DIM_CUBE_ARRAY]);

        BufferDesc bufferDesc = { 0 };
        bufferDesc.mNodeIndex = nodeIndex;
        bufferDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        bufferDesc.mStartState = RESOURCE_STATE_COMMON;
        bufferDesc.mSize = sizeof(uint32_t);
        bufferDesc.mFirstElement = 0;
        bufferDesc.mElementCount = 1;
        bufferDesc.mStructStride = sizeof(uint32_t);
        bufferDesc.mFormat = TinyImageFormat_R32_UINT;
        bufferDesc.pName = "DefaultBufferSRV";
        addBuffer(pRenderer, &bufferDesc, &pRenderer->pNullDescriptors->pDefaultBufferSRV[i]);
        bufferDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
        bufferDesc.pName = "DefaultBufferUAV";
        addBuffer(pRenderer, &bufferDesc, &pRenderer->pNullDescriptors->pDefaultBufferUAV[i]);
    }

    SamplerDesc samplerDesc = { 0 };
    samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_BORDER;
    addSampler(pRenderer, &samplerDesc, &pRenderer->pNullDescriptors->pDefaultSampler);

    BlendStateDesc blendStateDesc = { { 0 } };
    blendStateDesc.mDstAlphaFactors[0] = BC_ZERO;
    blendStateDesc.mDstFactors[0] = BC_ZERO;
    blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
    blendStateDesc.mSrcFactors[0] = BC_ONE;
    blendStateDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
    blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
    blendStateDesc.mIndependentBlend = false;
    gDefaultBlendDesc = util_to_blend_desc(&blendStateDesc, gDefaultBlendAttachments);

    DepthStateDesc depthStateDesc = { 0 };
    depthStateDesc.mDepthFunc = CMP_LEQUAL;
    depthStateDesc.mDepthTest = false;
    depthStateDesc.mDepthWrite = false;
    depthStateDesc.mStencilBackFunc = CMP_ALWAYS;
    depthStateDesc.mStencilFrontFunc = CMP_ALWAYS;
    depthStateDesc.mStencilReadMask = 0xFF;
    depthStateDesc.mStencilWriteMask = 0xFF;
    gDefaultDepthDesc = util_to_depth_desc(&depthStateDesc);

    RasterizerStateDesc rasterizerStateDesc = { 0 };
    rasterizerStateDesc.mCullMode = CULL_MODE_BACK;
    gDefaultRasterizerDesc = util_to_rasterizer_desc(pRenderer, &rasterizerStateDesc);

    // Create command buffer to transition resources to the correct state
    initMutex(&pRenderer->pNullDescriptors->mInitialTransitionMutex);

    // Transition resources
    for (uint32_t nodeIndex = 0; nodeIndex < pRenderer->mLinkedNodeCount; ++nodeIndex)
    {
        QueueDesc queueDesc = { 0 };
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        initQueue(pRenderer, &queueDesc, &pRenderer->pNullDescriptors->pInitialTransitionQueue[nodeIndex]);
        CmdPoolDesc cmdPoolDesc = { 0 };
        cmdPoolDesc.pQueue = pRenderer->pNullDescriptors->pInitialTransitionQueue[nodeIndex];
        cmdPoolDesc.mTransient = true;
        initCmdPool(pRenderer, &cmdPoolDesc, &pRenderer->pNullDescriptors->pInitialTransitionCmdPool[nodeIndex]);
        CmdDesc cmdDesc = { 0 };
        cmdDesc.pPool = pRenderer->pNullDescriptors->pInitialTransitionCmdPool[nodeIndex];
        initCmd(pRenderer, &cmdDesc, &pRenderer->pNullDescriptors->pInitialTransitionCmd[nodeIndex]);
        initFence(pRenderer, &pRenderer->pNullDescriptors->pInitialTransitionFence[nodeIndex]);

        Cmd*           cmd = pRenderer->pNullDescriptors->pInitialTransitionCmd[nodeIndex];
        TextureBarrier barriers[TEXTURE_DIM_COUNT * 2] = { { 0 } };
        uint32_t       barrierCount = 0;
        resetCmdPool(pRenderer, pRenderer->pNullDescriptors->pInitialTransitionCmdPool[nodeIndex]);
        beginCmd(cmd);

        for (uint32_t dim = 0; dim < TEXTURE_DIM_COUNT; ++dim)
        {
            if (pRenderer->pNullDescriptors->pDefaultTextureSRV[nodeIndex][dim])
            {
                barriers[barrierCount++] = (TextureBarrier){ pRenderer->pNullDescriptors->pDefaultTextureSRV[nodeIndex][dim],
                                                             RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_SHADER_RESOURCE };
            }
            if (pRenderer->pNullDescriptors->pDefaultTextureUAV[nodeIndex][dim])
            {
                barriers[barrierCount++] = (TextureBarrier){ pRenderer->pNullDescriptors->pDefaultTextureUAV[nodeIndex][dim],
                                                             RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_UNORDERED_ACCESS };
            }
        }

        cmdResourceBarrier(cmd, 0, NULL, barrierCount, barriers, 0, NULL);
        endCmd(cmd);
        QueueSubmitDesc submitDesc = { 0 };
        submitDesc.mCmdCount = 1;
        submitDesc.ppCmds = &cmd;
        submitDesc.pSignalFence = pRenderer->pNullDescriptors->pInitialTransitionFence[nodeIndex];
        queueSubmit(pRenderer->pNullDescriptors->pInitialTransitionQueue[nodeIndex], &submitDesc);
        waitForFences(pRenderer, 1, &pRenderer->pNullDescriptors->pInitialTransitionFence[nodeIndex]);
    }
}

static void remove_default_resources(Renderer* pRenderer)
{
    for (uint32_t nodeIndex = 0; nodeIndex < pRenderer->mLinkedNodeCount; ++nodeIndex)
    {
        for (uint32_t dim = 0; dim < TEXTURE_DIM_COUNT; ++dim)
        {
            if (pRenderer->pNullDescriptors->pDefaultTextureSRV[nodeIndex][dim])
                removeTexture(pRenderer, pRenderer->pNullDescriptors->pDefaultTextureSRV[nodeIndex][dim]);

            if (pRenderer->pNullDescriptors->pDefaultTextureUAV[nodeIndex][dim])
                removeTexture(pRenderer, pRenderer->pNullDescriptors->pDefaultTextureUAV[nodeIndex][dim]);
        }

        removeBuffer(pRenderer, pRenderer->pNullDescriptors->pDefaultBufferSRV[nodeIndex]);
        removeBuffer(pRenderer, pRenderer->pNullDescriptors->pDefaultBufferUAV[nodeIndex]);

        exitFence(pRenderer, pRenderer->pNullDescriptors->pInitialTransitionFence[nodeIndex]);
        exitCmd(pRenderer, pRenderer->pNullDescriptors->pInitialTransitionCmd[nodeIndex]);
        exitCmdPool(pRenderer, pRenderer->pNullDescriptors->pInitialTransitionCmdPool[nodeIndex]);
        exitQueue(pRenderer, pRenderer->pNullDescriptors->pInitialTransitionQueue[nodeIndex]);
    }

    removeSampler(pRenderer, pRenderer->pNullDescriptors->pDefaultSampler);

    exitMutex(&pRenderer->pNullDescriptors->mInitialTransitionMutex);
    exitMutex(&pRenderer->pNullDescriptors->mSubmitMutex);
}
/************************************************************************/
// Globals
/************************************************************************/
static tfrg_atomic32_t gRenderTargetIds = 1;
/************************************************************************/
// Internal utility functions
/************************************************************************/
VkFilter               util_to_vk_filter(FilterType filter)
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

VkSampleCountFlagBits util_to_vk_sample_count(Renderer* pRenderer, SampleCount sampleCount)
{
    VkSampleCountFlagBits result = VK_SAMPLE_COUNT_1_BIT;

    while (sampleCount > 0)
    {
        if ((pRenderer->pGpu->mFrameBufferSamplesCount & sampleCount) == 0)
        {
            LOGF(eWARNING, "Sample Count (%u) not supported. Trying a lower sample count (%u)", sampleCount, sampleCount / 2);
            sampleCount = (SampleCount)(sampleCount / 2);
        }
        else
        {
            break;
        }
    }

    switch (sampleCount)
    {
    case SAMPLE_COUNT_1:
        result = VK_SAMPLE_COUNT_1_BIT;
        break;
    case SAMPLE_COUNT_2:
        result = VK_SAMPLE_COUNT_2_BIT;
        break;
    case SAMPLE_COUNT_4:
        result = VK_SAMPLE_COUNT_4_BIT;
        break;
    case SAMPLE_COUNT_8:
        result = VK_SAMPLE_COUNT_8_BIT;
        break;
    case SAMPLE_COUNT_16:
        result = VK_SAMPLE_COUNT_16_BIT;
        break;
    default:
        ASSERT(false);
        break;
    }
    return result;
}

VkBufferUsageFlags util_to_vk_buffer_usage(BufferCreationFlags flags, DescriptorType usage, bool typed)
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
    if (usage & DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE)
    {
        result |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    }
    if (flags & BUFFER_CREATION_FLAG_ACCELERATION_STRUCTURE_BUILD_INPUT)
    {
        result |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }
    if (flags & BUFFER_CREATION_FLAG_SHADER_DEVICE_ADDRESS)
    {
        result |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }
    if (flags & BUFFER_CREATION_FLAG_SHADER_BINDING_TABLE)
    {
        result |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
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

VkImageType util_to_vk_image_type(const TextureDesc* pDesc)
{
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

    return image_type;
}

VkImageUsageFlags util_to_vk_image_usage_flags(ResourceState startState)
{
    VkImageUsageFlags usageFlags = 0;
    if (startState & RESOURCE_STATE_RENDER_TARGET)
        usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    else if (startState & RESOURCE_STATE_DEPTH_WRITE)
        usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    return usageFlags;
}

uint32_t util_vk_image_array_size(const TextureDesc* pDesc, VkImageUsageFlags usageFlags)
{
    UNREF_PARAM(usageFlags);
    uint32_t arraySize = pDesc->mArraySize;
#if defined(QUEST_VR)
    const VkImageUsageFlags renderTargetUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if ((usageFlags & renderTargetUsageFlags) == 0 && // If not a render target
        !!(pDesc->mFlags & TEXTURE_CREATION_FLAG_VR_MULTIVIEW))
    {
        // Double the array size
        arraySize *= 2;
    }
#endif

    return arraySize;
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
    if (state & (RESOURCE_STATE_UNORDERED_ACCESS | RESOURCE_STATE_UNORDERED_ACCESS_PIXEL))
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
    if (state & RESOURCE_STATE_DEPTH_READ)
    {
        ret |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
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

    if (state & RESOURCE_STATE_ACCELERATION_STRUCTURE_READ)
    {
        ret |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    }
    if (state & RESOURCE_STATE_ACCELERATION_STRUCTURE_WRITE)
    {
        ret |= VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
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
    else if (usage & RESOURCE_STATE_DEPTH_READ)
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    if (usage & (RESOURCE_STATE_UNORDERED_ACCESS | RESOURCE_STATE_UNORDERED_ACCESS_PIXEL))
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

uint32_t util_get_memory_type(uint32_t typeBits, const VkPhysicalDeviceMemoryProperties* pMemoryProperties,
                              VkMemoryPropertyFlags properties, VkBool32* memTypeFound)
{
    for (uint32_t i = 0; i < pMemoryProperties->memoryTypeCount; i++)
    {
        if ((typeBits & 1) == 1)
        {
            if ((pMemoryProperties->memoryTypes[i].propertyFlags & properties) == properties)
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
        LOGF(eERROR, "Could not find a matching memory type");
        ASSERT(0);
        return 0;
    }
}

// Determines pipeline stages involved for given accesses
static inline FORGE_CONSTEXPR VkPipelineStageFlags ToPipelineStageFlags(Renderer* pRenderer, ResourceState state, QueueType queueType)
{
    VkPipelineStageFlags flags = 0;

    if (pRenderer->pGpu->mRaytracingSupported)
    {
        if (state & (RESOURCE_STATE_ACCELERATION_STRUCTURE_READ))
        {
            flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        }
        if (state & (RESOURCE_STATE_ACCELERATION_STRUCTURE_WRITE))
        {
            flags |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        }
    }

    switch (queueType)
    {
    case QUEUE_TYPE_GRAPHICS:
    {
        if (state & RESOURCE_STATE_INDEX_BUFFER)
        {
            flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        }
        if (state & RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
        {
            flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        }
        if (state & RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        {
            flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        if (state & RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
        {
            flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            if (pRenderer->pGpu->mGeometryShaderSupported)
            {
                flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
            }
            if (pRenderer->pGpu->mTessellationSupported)
            {
                flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
                flags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
            }
            if (pRenderer->pGpu->mRayPipelineSupported)
            {
                flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
            }
        }
        if (state & RESOURCE_STATE_UNORDERED_ACCESS)
        {
            flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        if (state & RESOURCE_STATE_UNORDERED_ACCESS_PIXEL)
        {
            flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        if (state & RESOURCE_STATE_RENDER_TARGET)
        {
            flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        if (state & (RESOURCE_STATE_DEPTH_READ | RESOURCE_STATE_DEPTH_WRITE))
        {
            flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        }
#if defined(QUEST_VR)
        if (state & RESOURCE_STATE_SHADING_RATE_SOURCE)
        {
            flags |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        }
#endif
        break;
    }
    case QUEUE_TYPE_COMPUTE:
    {
        flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    case QUEUE_TYPE_TRANSFER:
    {
        return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
    default:
        break;
    }

    // Compatible with both compute and graphics queues
    if (state & RESOURCE_STATE_INDIRECT_ARGUMENT)
    {
        flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    }
    if (state & (RESOURCE_STATE_COPY_DEST | RESOURCE_STATE_COPY_SOURCE))
    {
        flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    if (flags == 0)
    {
        flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }

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
    default:
        result = VK_IMAGE_ASPECT_COLOR_BIT;
        break;
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

VkSampleLocationEXT util_to_vk_locations(SampleLocations location)
{
    VkSampleLocationEXT result = { location.mX / 16.f + 0.5f, location.mY / 16.f + 0.5f };
    return result;
}

VkQueueFlags util_to_vk_queue_flags(QueueType queueType)
{
    switch (queueType)
    {
    case QUEUE_TYPE_GRAPHICS:
        return VK_QUEUE_GRAPHICS_BIT;
    case QUEUE_TYPE_TRANSFER:
        return VK_QUEUE_TRANSFER_BIT;
    case QUEUE_TYPE_COMPUTE:
        return VK_QUEUE_COMPUTE_BIT;
    default:
        ASSERT(false && "Invalid Queue Type");
        return VK_QUEUE_FLAG_BITS_MAX_ENUM;
    }
}

static inline VkDescriptorType ToVkDescriptorType(DescriptorType type)
{
    switch (type)
    {
    case DESCRIPTOR_TYPE_UNDEFINED:
        ASSERT(false && "Invalid DescriptorInfo Type");
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
    case DESCRIPTOR_TYPE_TEXEL_BUFFER:
        return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    case DESCRIPTOR_TYPE_RW_TEXEL_BUFFER:
        return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE:
        return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    default:
        ASSERTFAIL("Invalid DescriptorInfo Type %i", type);
        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        break;
    }
}

VkShaderStageFlags util_to_vk_shader_stage_flags(ShaderStage stages)
{
    VkShaderStageFlags res = 0;
    if (SHADER_STAGE_ALL_GRAPHICS == (stages & SHADER_STAGE_ALL_GRAPHICS))
        return VK_SHADER_STAGE_ALL_GRAPHICS;

    if (stages & SHADER_STAGE_VERT)
        res |= VK_SHADER_STAGE_VERTEX_BIT;
    if (stages & SHADER_STAGE_GEOM)
        res |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (stages & SHADER_STAGE_TESE)
        res |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    if (stages & SHADER_STAGE_TESC)
        res |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (stages & SHADER_STAGE_FRAG)
        res |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stages & SHADER_STAGE_COMP)
        res |= VK_SHADER_STAGE_COMPUTE_BIT;

    ASSERT(res != 0);
    return res;
}

void util_find_queue_family_index(const Renderer* pRenderer, uint32_t nodeIndex, QueueType queueType, VkQueueFamilyProperties* pOutProps,
                                  uint8_t* pOutFamilyIndex, uint8_t* pOutQueueIndex)
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
    vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->pGpu->mVk.pGpu, &queueFamilyPropertyCount, NULL);
    queueFamilyProperties = (VkQueueFamilyProperties*)alloca(queueFamilyPropertyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->pGpu->mVk.pGpu, &queueFamilyPropertyCount, queueFamilyProperties);

    uint32_t minQueueFlag = UINT32_MAX;

    // Try to find a dedicated queue of this type
    for (uint32_t index = 0; index < queueFamilyPropertyCount; ++index)
    {
        VkQueueFlags queueFlags = queueFamilyProperties[index].queueFlags;
        bool         graphicsQueue = (queueFlags & VK_QUEUE_GRAPHICS_BIT) ? true : false;
        uint32_t     matchingQueueFlags = (queueFlags & requiredFlags);
        // Graphics queue - Usually only one graphics queue so choose first one with graphics bit
        if (queueType == QUEUE_TYPE_GRAPHICS && graphicsQueue)
        {
            found = true;
            queueFamilyIndex = index;
            queueIndex = 0;
            break;
        }
#if defined(__ANDROID__)
        // Note: Xclipse 920 GPU has issues with the transfer queue from the non-graphics queue family in the recent driver, Android OS U
        // But since Vulkan properties always provide 2.0.0 driver version for all Xclipse GPUs
        // We have to apply this workaround for Xclipse 920 in general
        // Expected to be fixed in Android OS V update
        if (pRenderer->pGpu->mXclipseTransferQueueWorkaround)
        {
            if (queueType == QUEUE_TYPE_TRANSFER)
            {
                if (graphicsQueue)
                {
                    found = true;
                    queueFamilyIndex = index;
                    queueIndex = (pRenderer->mVk.pUsedQueueCount[nodeIndex][index] < pRenderer->mVk.pAvailableQueueCount[nodeIndex][index])
                                     ? pRenderer->mVk.pUsedQueueCount[nodeIndex][index]
                                     : 0;
                    break;
                }
                else
                {
                    continue;
                }
            }
        }
#endif
        // Only flag set on this queue family is the one required - Most optimal choice
        // Example: Required flag is VK_QUEUE_TRANSFER_BIT and the queue family has only VK_QUEUE_TRANSFER_BIT set
        if (matchingQueueFlags && ((queueFlags & ~requiredFlags) == 0) &&
            pRenderer->mVk.pUsedQueueCount[nodeIndex][index] < pRenderer->mVk.pAvailableQueueCount[nodeIndex][index])
        {
            found = true;
            queueFamilyIndex = index;
            queueIndex = pRenderer->mVk.pUsedQueueCount[nodeIndex][index];
            break;
        }
        // Queue family has flags other than required flags - Choose the family with least number of other flags
        // Example: Required flag is VK_QUEUE_TRANSFER_BIT
        // Two queue families considered
        // Queue family 1 has VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT
        // Queue family 2 has VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_SPARSE_BINDING_BIT
        // Since 1 has less flags, we choose queue family 1
        if (matchingQueueFlags && ((queueFlags - matchingQueueFlags) < minQueueFlag) && !graphicsQueue &&
            pRenderer->mVk.pUsedQueueCount[nodeIndex][index] < pRenderer->mVk.pAvailableQueueCount[nodeIndex][index])
        {
            found = true;
            minQueueFlag = (queueFlags - matchingQueueFlags);
            queueFamilyIndex = index;
            queueIndex = pRenderer->mVk.pUsedQueueCount[nodeIndex][index];
        }
    }

    // If hardware doesn't provide a dedicated queue try to find a non-dedicated one
    if (!found)
    {
        for (uint32_t index = 0; index < queueFamilyPropertyCount; ++index)
        {
            VkQueueFlags queueFlags = queueFamilyProperties[index].queueFlags;
            if ((queueFlags & requiredFlags) &&
                pRenderer->mVk.pUsedQueueCount[nodeIndex][index] < pRenderer->mVk.pAvailableQueueCount[nodeIndex][index])
            {
                found = true;
                queueFamilyIndex = index;
                queueIndex = pRenderer->mVk.pUsedQueueCount[nodeIndex][index];
                break;
            }
        }
    }

    if (!found)
    {
        found = true;
        queueFamilyIndex = 0;
        queueIndex = 0;

        LOGF(eWARNING, "Could not find queue of type %u. Using default queue", (uint32_t)queueType);
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
    if (flags & PIPELINE_CACHE_FLAG_EXTERNALLY_SYNCHRONIZED)
    {
        ret |= VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT_EXT;
    }

    return ret;
}
/************************************************************************/
// Multi GPU Helper Functions
/************************************************************************/
uint32_t util_calculate_shared_device_mask(uint32_t gpuCount) { return (1 << gpuCount) - 1; }

void util_calculate_device_indices(Renderer* pRenderer, uint32_t nodeIndex, uint32_t* pSharedNodeIndices, uint32_t sharedNodeIndexCount,
                                   uint32_t* pIndices)
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

static bool QueryGpuInfo(const RendererContextDesc* pDesc, RendererContext* pContext, GpuDesc* pGpuDesc)
{
    ASSERT(VK_NULL_HANDLE != pContext->mVk.pInstance);

    uint32_t layerCount = 0;
    uint32_t extCount = 0;
    vkEnumerateDeviceLayerProperties(pGpuDesc->mVk.pGpu, &layerCount, NULL);
    vkEnumerateDeviceExtensionProperties(pGpuDesc->mVk.pGpu, NULL, &extCount, NULL);

    VkLayerProperties* layers = (VkLayerProperties*)alloca(sizeof(VkLayerProperties) * layerCount);
    vkEnumerateDeviceLayerProperties(pGpuDesc->mVk.pGpu, &layerCount, layers);

    VkExtensionProperties* exts = (VkExtensionProperties*)alloca(sizeof(VkExtensionProperties) * extCount);
    vkEnumerateDeviceExtensionProperties(pGpuDesc->mVk.pGpu, NULL, &extCount, exts);

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

    // Standalone extensions
    {
        const char* layer_name = NULL;
        uint32_t    initialCount = TF_ARRAY_COUNT(gVkWantedDeviceExtensions);
        // Dont need debug marker if we have debug utils
        initialCount -= pContext->mVk.mDebugUtilsExtension ? 1 : 0;
        const uint32_t userRequestedCount = (uint32_t)pDesc->mVk.mDeviceExtensionCount;
        const char**   wantedDeviceExtensions = NULL;
        arrsetlen(wantedDeviceExtensions, initialCount + userRequestedCount + 1);
        uint32_t wantedExtensionCount = 0;
        for (uint32_t i = 0; i < initialCount; ++i, ++wantedExtensionCount)
        {
            wantedDeviceExtensions[i] = gVkWantedDeviceExtensions[i];
        }
        for (uint32_t i = 0; i < userRequestedCount; ++i, ++wantedExtensionCount)
        {
            wantedDeviceExtensions[initialCount + i] = pDesc->mVk.ppDeviceExtensions[i];
        }
#if defined(SHADER_STATS_AVAILABLE)
        if (pDesc->mEnableShaderStats)
        {
            wantedDeviceExtensions[wantedExtensionCount++] = VK_AMD_SHADER_INFO_EXTENSION_NAME;
        }
#endif
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(pGpuDesc->mVk.pGpu, layer_name, &count, NULL);
        if (count > 0)
        {
            VkExtensionProperties* properties = (VkExtensionProperties*)tf_calloc(count, sizeof(*properties));
            ASSERT(properties != NULL);
            vkEnumerateDeviceExtensionProperties(pGpuDesc->mVk.pGpu, layer_name, &count, properties);
            for (uint32_t j = 0; j < count; ++j)
            {
                for (uint32_t k = 0; k < wantedExtensionCount; ++k)
                {
                    if (strcmp(wantedDeviceExtensions[k], properties[j].extensionName) == 0)
                    {
                        if (strcmp(wantedDeviceExtensions[k], VK_AMD_BUFFER_MARKER_EXTENSION_NAME) == 0)
                            pGpuDesc->mAMDBufferMarkerExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME) == 0)
                            pGpuDesc->mAMDDeviceCoherentMemoryExtension = true;
#if defined(GFX_DEVICE_MEMORY_TRACKING)
                        if (strcmp(wantedDeviceExtensions[k], VK_EXT_DEVICE_MEMORY_REPORT_EXTENSION_NAME) == 0)
                            pGpuDesc->mDeviceMemoryReportExtension = gEnableDeviceMemoryTracking;
#endif
                        if (strcmp(wantedDeviceExtensions[k], VK_EXT_ASTC_DECODE_MODE_EXTENSION_NAME) == 0)
                            pGpuDesc->mASTCDecodeModeExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_EXT_DEBUG_MARKER_EXTENSION_NAME) == 0)
                            pGpuDesc->mDebugMarkerExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) == 0)
                            pGpuDesc->mDedicatedAllocationExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 0)
                            pGpuDesc->mMemoryReq2Extension = true;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME) == 0)
                            pGpuDesc->mExternalMemoryExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME) == 0)
                            pGpuDesc->mExternalMemoryWin32Extension = true;
#endif
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME) == 0)
                            pGpuDesc->mDrawIndirectCountExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_AMD_DRAW_INDIRECT_COUNT_EXTENSION_NAME) == 0)
                            pGpuDesc->mAMDDrawIndirectCountExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_AMD_GCN_SHADER_EXTENSION_NAME) == 0)
                            pGpuDesc->mAMDGCNShaderExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_AMD_SHADER_INFO_EXTENSION_NAME) == 0)
                            pGpuDesc->mAMDShaderInfoExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) == 0)
                            pGpuDesc->mDescriptorIndexingExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) == 0)
                            pGpuDesc->mBufferDeviceAddressExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME) == 0)
                            pGpuDesc->mShaderFloatControlsExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) == 0)
                            pGpuDesc->mDeferredHostOperationsExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0)
                            pGpuDesc->mAccelerationStructureExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_SPIRV_1_4_EXTENSION_NAME) == 0)
                            pGpuDesc->mSpirv14Extension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME) == 0)
                            pGpuDesc->mShaderAtomicInt64Extension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0)
                            pGpuDesc->mRayTracingPipelineExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_RAY_QUERY_EXTENSION_NAME) == 0)
                            pGpuDesc->mRayQueryExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME) == 0)
                            pGpuDesc->mYCbCrExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME) == 0)
                            pGpuDesc->mFragmentShaderInterlockExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0)
                            pGpuDesc->mDynamicRenderingExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME) == 0)
                            pGpuDesc->mSoftwareVRSSupported = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_EXT_DEVICE_FAULT_EXTENSION_NAME) == 0) //-V595
                            pGpuDesc->mDeviceFaultExtension = true;
#if defined(QUEST_VR)
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_MULTIVIEW_EXTENSION_NAME) == 0)
                            pGpuDesc->mMultiviewExtension = true;
#endif
                        break;
                    }
                }
            }
            SAFE_FREE(properties);
        }
        arrfree(wantedDeviceExtensions);
    }

    //-V:ADD_TO_NEXT_CHAIN:506, 1027
#define ADD_TO_NEXT_CHAIN(condition, next)        \
    if ((condition))                              \
    {                                             \
        base->pNext = (VkBaseOutStructure*)&next; \
        base = (VkBaseOutStructure*)base->pNext;  \
    }

    VkPhysicalDeviceFeatures2KHR gpuFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
    VkBaseOutStructure*          base = (VkBaseOutStructure*)&gpuFeatures2; //-V1027

    // Add more extensions here
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragmentShaderInterlockFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT
    };
    ADD_TO_NEXT_CHAIN(pGpuDesc->mFragmentShaderInterlockExtension, fragmentShaderInterlockFeatures);
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexingFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT
    };
    ADD_TO_NEXT_CHAIN(pGpuDesc->mDescriptorIndexingExtension, descriptorIndexingFeatures);
    VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcrFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES };
    ADD_TO_NEXT_CHAIN(pGpuDesc->mYCbCrExtension, ycbcrFeatures);
#if defined(QUEST_VR)
    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES };
    ADD_TO_NEXT_CHAIN(pGpuDesc->mMultiviewExtension, multiviewFeatures);
#endif

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR
    };
    ADD_TO_NEXT_CHAIN(pGpuDesc->mDynamicRenderingExtension, dynamicRenderingFeatures);

    VkPhysicalDeviceShaderAtomicInt64Features shaderAtomicInt64Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR
    };
    ADD_TO_NEXT_CHAIN(pGpuDesc->mShaderAtomicInt64Extension, shaderAtomicInt64Features);

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES
    };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
    };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
    };
    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
    ADD_TO_NEXT_CHAIN(pGpuDesc->mBufferDeviceAddressExtension, bufferDeviceAddressFeatures);
    ADD_TO_NEXT_CHAIN(pGpuDesc->mRayTracingPipelineExtension, rayTracingPipelineFeatures);
    ADD_TO_NEXT_CHAIN(pGpuDesc->mAccelerationStructureExtension, accelerationStructureFeatures);
    ADD_TO_NEXT_CHAIN(pGpuDesc->mRayQueryExtension, rayQueryFeatures);

    VkPhysicalDeviceFaultFeaturesEXT deviceFaultFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT };
    ADD_TO_NEXT_CHAIN(pGpuDesc->mDeviceFaultExtension, deviceFaultFeatures);

    VkPhysicalDeviceDeviceMemoryReportFeaturesEXT memoryReportFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_MEMORY_REPORT_FEATURES_EXT
    };
    ADD_TO_NEXT_CHAIN(pGpuDesc->mDeviceMemoryReportExtension, memoryReportFeatures);

    VkPhysicalDeviceCoherentMemoryFeaturesAMD deviceCoherentFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD };
    ADD_TO_NEXT_CHAIN(pGpuDesc->mAMDDeviceCoherentMemoryExtension, deviceCoherentFeatures);

    vkGetPhysicalDeviceFeatures2KHR(pGpuDesc->mVk.pGpu, &gpuFeatures2);

    // Get memory properties
    VkPhysicalDeviceMemoryProperties gpuMemoryProperties = { 0 };
    vkGetPhysicalDeviceMemoryProperties(pGpuDesc->mVk.pGpu, &gpuMemoryProperties);

    // Get device properties
    VkPhysicalDeviceSubgroupProperties subgroupProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };
    VkPhysicalDeviceProperties2KHR     gpuProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR, &subgroupProperties };
    vkGetPhysicalDeviceProperties2KHR(pGpuDesc->mVk.pGpu, &gpuProperties);

    pGpuDesc->mDynamicRenderingSupported = dynamicRenderingFeatures.dynamicRendering;

    const bool bufferDeviceAddressFeature = bufferDeviceAddressFeatures.bufferDeviceAddress;
    pGpuDesc->mBufferDeviceAddressSupported = bufferDeviceAddressFeature;

    pGpuDesc->m64BitAtomicsSupported = shaderAtomicInt64Features.shaderBufferInt64Atomics;

    const bool rayTracingPipelineFeature = rayTracingPipelineFeatures.rayTracingPipeline;
    const bool accelerationStructureFeature = accelerationStructureFeatures.accelerationStructure;
    const bool rayQueryFeature = rayQueryFeatures.rayQuery;

    pGpuDesc->mRayPipelineSupported = bufferDeviceAddressFeature && rayTracingPipelineFeature && pGpuDesc->mShaderFloatControlsExtension &&
                                      pGpuDesc->mSpirv14Extension && accelerationStructureFeature &&
                                      pGpuDesc->mDeferredHostOperationsExtension;
    pGpuDesc->mRayQuerySupported = rayQueryFeature && accelerationStructureFeature && pGpuDesc->mDeferredHostOperationsExtension;
    pGpuDesc->mRaytracingSupported = pGpuDesc->mRayPipelineSupported || pGpuDesc->mRayQuerySupported;

    pGpuDesc->mDedicatedAllocationExtension = pGpuDesc->mDedicatedAllocationExtension && pGpuDesc->mMemoryReq2Extension;

    const bool deviceFaultSupported = deviceFaultFeatures.deviceFault;
    pGpuDesc->mDeviceFaultSupported = deviceFaultSupported;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    pGpuDesc->mExternalMemoryExtension = pGpuDesc->mExternalMemoryExtension && pGpuDesc->mExternalMemoryWin32Extension;
#endif

    pGpuDesc->mUniformBufferAlignment = (uint32_t)gpuProperties.properties.limits.minUniformBufferOffsetAlignment;
    pGpuDesc->mUploadBufferAlignment = 1;
    pGpuDesc->mUploadBufferTextureAlignment = (uint32_t)gpuProperties.properties.limits.optimalBufferCopyOffsetAlignment;
    pGpuDesc->mUploadBufferTextureRowAlignment = (uint32_t)gpuProperties.properties.limits.optimalBufferCopyRowPitchAlignment;
    pGpuDesc->mMaxVertexInputBindings = gpuProperties.properties.limits.maxVertexInputBindings;
    pGpuDesc->mMultiDrawIndirect = gpuFeatures2.features.multiDrawIndirect;
    pGpuDesc->mMultiDrawIndirectCount = pGpuDesc->mDrawIndirectCountExtension || pGpuDesc->mAMDDrawIndirectCountExtension;
    pGpuDesc->mMaxBoundTextures = gpuProperties.properties.limits.maxPerStageDescriptorSampledImages;
    pGpuDesc->mMaxStorageBufferSize = gpuProperties.properties.limits.maxStorageBufferRange;
    pGpuDesc->mMaxTotalComputeThreads = gpuProperties.properties.limits.maxComputeWorkGroupInvocations;
    pGpuDesc->mPrimitiveIdPsSupported = true;
    COMPILE_ASSERT(sizeof(pGpuDesc->mMaxComputeThreads) == sizeof(gpuProperties.properties.limits.maxComputeWorkGroupSize));
    memcpy(pGpuDesc->mMaxComputeThreads, gpuProperties.properties.limits.maxComputeWorkGroupSize,
           sizeof(gpuProperties.properties.limits.maxComputeWorkGroupSize));
    pGpuDesc->mRootConstant = true;
    pGpuDesc->mIndirectRootConstant = false;
    pGpuDesc->mBuiltinDrawID = true;
    pGpuDesc->mTimestampQueries =
        gpuProperties.properties.limits.timestampPeriod > 0 && gpuProperties.properties.limits.timestampComputeAndGraphics;
    pGpuDesc->mOcclusionQueries = true;
    pGpuDesc->mPipelineStatsQueries = gpuFeatures2.features.pipelineStatisticsQuery;
#if !defined(NX64) && !defined(__ANDROID__)
    pGpuDesc->mHDRSupported = true;
#endif
    pGpuDesc->mGpuMarkers = true;
    pGpuDesc->mAMDDeviceCoherentMemorySupported = deviceCoherentFeatures.deviceCoherentMemory;
    /************************************************************************/
    // To find VRAM in Vulkan, loop through all the heaps and find if the
    // heap has the DEVICE_LOCAL_BIT flag set
    /************************************************************************/
    uint64_t totalTestVram = 0;
    for (uint32_t i = 0; i < gpuMemoryProperties.memoryHeapCount; ++i)
    {
        if (VK_MEMORY_HEAP_DEVICE_LOCAL_BIT & gpuMemoryProperties.memoryHeaps[i].flags)
            totalTestVram += gpuMemoryProperties.memoryHeaps[i].size;
    }
    pGpuDesc->mVRAM = totalTestVram;
    pGpuDesc->mAllowBufferTextureInSameHeap = true;

    pGpuDesc->mWaveLaneCount = subgroupProperties.subgroupSize;
    pGpuDesc->mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_NONE;
    if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT)
        pGpuDesc->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_BASIC_BIT;
    if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_VOTE_BIT)
        pGpuDesc->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_VOTE_BIT;
    if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT)
        pGpuDesc->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_ARITHMETIC_BIT;
    if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT)
        pGpuDesc->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_BALLOT_BIT;
    if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT)
        pGpuDesc->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_SHUFFLE_BIT;
    if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT)
        pGpuDesc->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_SHUFFLE_RELATIVE_BIT;
    if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_CLUSTERED_BIT)
        pGpuDesc->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_CLUSTERED_BIT;
    if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_QUAD_BIT)
        pGpuDesc->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_QUAD_BIT;
    if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_PARTITIONED_BIT_NV)
        pGpuDesc->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_PARTITIONED_BIT_NV;

    pGpuDesc->mWaveOpsSupportedStageFlags = SHADER_STAGE_NONE;
    if (subgroupProperties.supportedStages & VK_SHADER_STAGE_VERTEX_BIT)
        pGpuDesc->mWaveOpsSupportedStageFlags |= SHADER_STAGE_VERT;
    if (subgroupProperties.supportedStages & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
        pGpuDesc->mWaveOpsSupportedStageFlags |= SHADER_STAGE_TESC;
    if (subgroupProperties.supportedStages & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        pGpuDesc->mWaveOpsSupportedStageFlags |= SHADER_STAGE_TESE;
    if (subgroupProperties.supportedStages & VK_SHADER_STAGE_GEOMETRY_BIT)
        pGpuDesc->mWaveOpsSupportedStageFlags |= SHADER_STAGE_GEOM;
    if (subgroupProperties.supportedStages & VK_SHADER_STAGE_FRAGMENT_BIT)
        pGpuDesc->mWaveOpsSupportedStageFlags |= SHADER_STAGE_FRAG;
    if (subgroupProperties.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT)
        pGpuDesc->mWaveOpsSupportedStageFlags |= SHADER_STAGE_COMP;

    pGpuDesc->mROVsSupported = (bool)fragmentShaderInterlockFeatures.fragmentShaderPixelInterlock;
    pGpuDesc->mTessellationSupported = gpuFeatures2.features.tessellationShader;
    pGpuDesc->mGeometryShaderSupported = gpuFeatures2.features.geometryShader;
    pGpuDesc->mSamplerAnisotropySupported = gpuFeatures2.features.samplerAnisotropy;
    pGpuDesc->mNonUniformResourceIndexSupported = descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing;

    pGpuDesc->mFillModeNonSolid = gpuFeatures2.features.fillModeNonSolid;
#if defined(AMDAGS)
    pGpuDesc->mAmdAsicFamily = agsGetAsicFamily(gpuProperties.properties.deviceID);
#endif
    pGpuDesc->mFrameBufferSamplesCount = (uint32_t)gpuProperties.properties.limits.framebufferColorSampleCounts;
#if defined(ANDROID) || defined(NX64)
    pGpuDesc->mUnifiedMemorySupport = UMA_SUPPORT_READ_WRITE;
#endif
    // save vendor and model Id as string
    pGpuDesc->mGpuVendorPreset.mModelId = gpuProperties.properties.deviceID;
    pGpuDesc->mGpuVendorPreset.mVendorId = gpuProperties.properties.vendorID;
    strncpy(pGpuDesc->mGpuVendorPreset.mGpuName, gpuProperties.properties.deviceName, MAX_GPU_VENDOR_STRING_LENGTH);

    // Disable memory device report on Xclipse GPUs since it can cause crashes
    if (gpuVendorEquals(pGpuDesc->mGpuVendorPreset.mVendorId, "samsung"))
    {
        pGpuDesc->mDeviceMemoryReportExtension = 0;
    }

    // TODO: Fix once vulkan adds support for revision ID
    GPUVendorPreset* preset = &pGpuDesc->mGpuVendorPreset;
    preset->mRevisionId = 0;
    preset->mPresetLevel = getGPUPresetLevel(preset->mVendorId, preset->mModelId, preset->mVendorName, preset->mGpuName);

    // set default driver to be very high to not trigger driver rejection rules if NVAPI or AMDAGS fails
    snprintf(pGpuDesc->mGpuVendorPreset.mGpuDriverVersion, MAX_GPU_VENDOR_STRING_LENGTH, "%u.%u", 999999, 99);
    if (gpuVendorEquals(pGpuDesc->mGpuVendorPreset.mVendorId, "nvidia"))
    {
#if defined(NVAPI)
        if (NVAPI_OK == gNvStatus)
        {
            snprintf(pGpuDesc->mGpuVendorPreset.mGpuDriverVersion, MAX_GPU_VENDOR_STRING_LENGTH, "%lu.%lu", gNvGpuInfo.driverVersion / 100,
                     gNvGpuInfo.driverVersion % 100);
        }
#else
        uint32_t major, minor, secondaryBranch, tertiaryBranch;
        major = (gpuProperties.properties.driverVersion >> 22) & 0x3ff;
        minor = (gpuProperties.properties.driverVersion >> 14) & 0x0ff;
        secondaryBranch = (gpuProperties.properties.driverVersion >> 6) & 0x0ff;
        tertiaryBranch = (gpuProperties.properties.driverVersion) & 0x003f;
        snprintf(pGpuDesc->mGpuVendorPreset.mGpuDriverVersion, MAX_GPU_VENDOR_STRING_LENGTH, "%u.%u.%u.%u", major, minor, secondaryBranch,
                 tertiaryBranch);
#endif
    }
    else if (gpuVendorEquals(pGpuDesc->mGpuVendorPreset.mVendorId, "amd"))
    {
#if defined(AMDAGS)
        if (AGSReturnCode::AGS_SUCCESS == gAgsStatus)
        {
            snprintf(pGpuDesc->mGpuVendorPreset.mGpuDriverVersion, MAX_GPU_VENDOR_STRING_LENGTH, "%s", gAgsGpuInfo.driverVersion);
        }
#else
        VK_FORMAT_VERSION(gpuProperties.properties.driverVersion, pGpuDesc->mGpuVendorPreset.mGpuDriverVersion);
        LOGF(eWARNING, "Parsing amd driver version without ags lib is unreliable, setting to default value: %s",
             pGpuDesc->mGpuVendorPreset.mGpuDriverVersion);
#endif
    }
    else if (gpuVendorEquals(pGpuDesc->mGpuVendorPreset.mVendorId, "intel"))
    {
        VK_FORMAT_VERSION(gpuProperties.properties.driverVersion, pGpuDesc->mGpuVendorPreset.mGpuDriverVersion);
        uint32_t major = gpuProperties.properties.driverVersion >> 14;
        uint32_t minor = gpuProperties.properties.driverVersion & 0x3fff;
        snprintf(pGpuDesc->mGpuVendorPreset.mGpuDriverVersion, MAX_GPU_VENDOR_STRING_LENGTH, "%u.%u", major, minor);
    }
    else
    {
        VK_FORMAT_VERSION(gpuProperties.properties.driverVersion, pGpuDesc->mGpuVendorPreset.mGpuDriverVersion);
    }

    gpuProperties.pNext = NULL;
    pGpuDesc->mVk.mGpuProperties = gpuProperties;

    return true;
}

#if defined(GFX_ENABLE_SWAPPY)
bool     gSwappyEnabled = true;
uint64_t gSwapIntervalNS = SWAPPY_SWAP_60FPS;
#endif

void InitializeBufferCreateInfo(Renderer* pRenderer, const BufferDesc* pDesc, VkBufferCreateInfo* pOutInfo)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(pOutInfo);

    uint64_t allocationSize = pDesc->mSize;
    // Align the buffer size to multiples of the dynamic uniform buffer minimum size
    if (pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    {
        uint64_t minAlignment = pRenderer->pGpu->mUniformBufferAlignment;
        allocationSize = round_up_64(allocationSize, minAlignment);
    }

    VkBufferCreateInfo add_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = allocationSize,
        .usage = util_to_vk_buffer_usage(pDesc->mFlags, pDesc->mDescriptors, pDesc->mFormat != TinyImageFormat_UNDEFINED),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
    };

    // Buffer can be used as dest in a transfer command (Uploading data to a storage buffer, Readback query data)
    if (pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY || pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU ||
        (pDesc->mStartState & RESOURCE_STATE_COPY_DEST))
    {
        add_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    *pOutInfo = add_info;
}

// Holds variables that might be pointed by VkImageCreateInfo::pNext and additional output from InitializeImageCreateInfo
typedef struct ImageCreateInfoExtras
{
    VmaAllocationCreateInfo mMemReq;

    // VkImageCreateInfo::pNext might point to these, make sure this structure lives as long as VkImageCreateInfo
    VkExternalMemoryImageCreateInfoKHR mExternalInfo;
    VkImageFormatListCreateInfoKHR     mFormatListInfo;
    VkExportMemoryAllocateInfoKHR      mExportMemoryInfo;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    VkImportMemoryWin32HandleInfoKHR mImportMemoryInfo;
#endif
    VkFormat mPlanarFormat;

    bool mLazilyAllocated;
} ImageCreateInfoExtras;

void InitializeImageCreateInfo(Renderer* pRenderer, const TextureDesc* pDesc, VkImageCreateInfo* pOutInfo, ImageCreateInfoExtras* pExtras)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(pOutInfo);
    ASSERT(pExtras);

    const DescriptorType descriptors = pDesc->mDescriptors;

    const VkImageUsageFlags additionalFlags = util_to_vk_image_usage_flags(pDesc->mStartState);
    const uint32_t          arraySize = util_vk_image_array_size(pDesc, additionalFlags);
    const VkImageType       image_type = util_to_vk_image_type(pDesc);

    const uint32_t numOfPlanes = TinyImageFormat_NumOfPlanes(pDesc->mFormat);
    const bool     isSinglePlane = TinyImageFormat_IsSinglePlane(pDesc->mFormat);
    ASSERT(((isSinglePlane && numOfPlanes == 1) || (!isSinglePlane && numOfPlanes > 1 && numOfPlanes <= MAX_PLANE_COUNT)) &&
           "Number of planes for multi-planar formats must be 2 or 3 and for single-planar formats it must be 1.");

    bool arrayRequired = false;
    if (image_type == VK_IMAGE_TYPE_3D)
        arrayRequired = true;

    VkImageCreateInfo add_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = pDesc->mFlags & TEXTURE_CREATION_FLAG_SAMPLE_LOCATIONS_COMPATIBLE
                     ? VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT
                     : 0,
        .imageType = image_type,
        .format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mFormat),
        .extent.width = pDesc->mWidth,
        .extent.height = pDesc->mHeight,
        .extent.depth = pDesc->mDepth,
        .mipLevels = pDesc->mMipLevels,
        .arrayLayers = arraySize,
        .samples = util_to_vk_sample_count(pRenderer, pDesc->mSampleCount),
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = util_to_vk_image_usage(descriptors) | additionalFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    if ((DESCRIPTOR_TYPE_TEXTURE_CUBE == (descriptors & DESCRIPTOR_TYPE_TEXTURE_CUBE)))
        add_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    if (arrayRequired)
        add_info.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR;

    if ((VK_IMAGE_USAGE_SAMPLED_BIT & add_info.usage) || (VK_IMAGE_USAGE_STORAGE_BIT & add_info.usage))
    {
        // Make it easy to copy to and from textures
        add_info.usage |= (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    }

    ASSERT((pRenderer->pGpu->mFormatCaps[pDesc->mFormat] & FORMAT_CAP_READ) && "GPU shader can't' read from this format");

    VkFormatProperties format_props = { 0 };
    vkGetPhysicalDeviceFormatProperties(pRenderer->pGpu->mVk.pGpu, add_info.format, &format_props);

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

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    if (pRenderer->pGpu->mExternalMemoryExtension && pDesc->mFlags & TEXTURE_CREATION_FLAG_IMPORT_BIT)
    {
        struct ImportHandleInfo
        {
            void*                                 pHandle;
            VkExternalMemoryHandleTypeFlagBitsKHR mHandleType;
        };
        struct ImportHandleInfo* pHandleInfo = (struct ImportHandleInfo*)pDesc->pNativeHandle;

        pExtras->mExternalInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR;
        pExtras->mExternalInfo.pNext = NULL;
        pExtras->mExternalInfo.handleTypes = pHandleInfo->mHandleType;
        add_info.pNext = &pExtras->mExternalInfo;

        pExtras->mImportMemoryInfo.handle = pHandleInfo->pHandle;
        pExtras->mImportMemoryInfo.handleType = pHandleInfo->mHandleType;
        mem_reqs.pUserData = &pExtras->mImportMemoryInfo;
        // Allocate external (importable / exportable) memory as dedicated memory to avoid adding unnecessary complexity to the Vulkan
        // Memory Allocator
        mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }
    else if (pRenderer->pGpu->mExternalMemoryExtension && pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_BIT)
    {
        pExtras->mExportMemoryInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR;
        pExtras->mExportMemoryInfo.pNext = NULL;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
        pExtras->mExportMemoryInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
#endif

        mem_reqs.pUserData = &pExtras->mExportMemoryInfo;
        // Allocate external (importable / exportable) memory as dedicated memory to avoid adding unnecessary complexity to the Vulkan
        // Memory Allocator
        mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }
#endif

    // If lazy allocation is requested, check that the hardware supports it
    bool lazyAllocation = pDesc->mFlags & TEXTURE_CREATION_FLAG_ON_TILE;
    if (lazyAllocation)
    {
        uint32_t                memoryTypeIndex = 0;
        VmaAllocationCreateInfo lazyMemReqs = mem_reqs;
        lazyMemReqs.usage = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
        VkResult result = vmaFindMemoryTypeIndex(pRenderer->mVk.pVmaAllocator, UINT32_MAX, &lazyMemReqs, &memoryTypeIndex);
        if (VK_SUCCESS == result)
        {
            mem_reqs = lazyMemReqs;
            add_info.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
            // The Vulkan spec states: If usage includes VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
            // then bits other than VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            // and VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT must not be set
            add_info.usage &= (VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

            pExtras->mLazilyAllocated = true;
        }
    }

    if (!isSinglePlane)
    {
        // Create info requires the mutable format flag set for multi planar images
        // Also pass the format list for mutable formats as per recommendation from the spec
        // Might help to keep DCC enabled if we ever use this as a output format
        // DCC gets disabled when we pass mutable format bit to the create info. Passing the format list helps the driver to enable it
        pExtras->mPlanarFormat = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mFormat);
        pExtras->mFormatListInfo.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR;
        pExtras->mFormatListInfo.pNext = NULL;
        pExtras->mFormatListInfo.pViewFormats = &pExtras->mPlanarFormat;
        pExtras->mFormatListInfo.viewFormatCount = 1;

        add_info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        add_info.pNext = &pExtras->mFormatListInfo; //-V506
    }

    *pOutInfo = add_info;
    pExtras->mMemReq = mem_reqs;
}

/************************************************************************/
// Internal init functions
/************************************************************************/
void CreateInstance(const char* app_name, const RendererContextDesc* pDesc, uint32_t userDefinedInstanceLayerCount,
                    const char** userDefinedInstanceLayers, RendererContext* pContext)
{
    // These are the extensions that we have loaded
    const char* instanceExtensionCache[MAX_INSTANCE_EXTENSIONS] = { 0 };

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

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = app_name,
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "TheForge",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = TARGET_VULKAN_API_VERSION,
    };

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

        uint32_t       extension_count = 0;
        const uint32_t initialCount = sizeof(gVkWantedInstanceExtensions) / sizeof(gVkWantedInstanceExtensions[0]);
        const uint32_t userRequestedCount = (uint32_t)pDesc->mVk.mInstanceExtensionCount;
        const char**   wantedInstanceExtensions = NULL;
        arrsetlen(wantedInstanceExtensions, initialCount + userRequestedCount);
        for (uint32_t i = 0; i < initialCount; ++i)
        {
            wantedInstanceExtensions[i] = gVkWantedInstanceExtensions[i]; //-V595
        }
        for (uint32_t i = 0; i < userRequestedCount; ++i)
        {
            wantedInstanceExtensions[initialCount + i] = pDesc->mVk.ppInstanceExtensions[i];
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
                    if (strcmp(wantedInstanceExtensions[k], properties[j].extensionName) == 0) //-V::522
                    {
                        if (strcmp(wantedInstanceExtensions[k], VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME) == 0)
                            pContext->mVk.mDeviceGroupCreationExtension = true;
                        if (strcmp(wantedInstanceExtensions[k], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
                            pContext->mVk.mDebugUtilsExtension = true;
                        if (strcmp(wantedInstanceExtensions[k], VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0)
                            pContext->mVk.mDebugReportExtension = true;

                        instanceExtensionCache[extension_count++] = wantedInstanceExtensions[k];
                        // clear wanted extension so we dont load it more then once
                        wantedInstanceExtensions[k] = "";
                        break;
                    }
                }
            }
            SAFE_FREE(properties);
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
                            // gVkWantedInstanceExtensions[k] = "";
                            if (strcmp(wantedInstanceExtensions[k], VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME) == 0)
                                pContext->mVk.mDeviceGroupCreationExtension = true;
                            if (strcmp(wantedInstanceExtensions[k], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
                                pContext->mVk.mDebugUtilsExtension = true;
                            if (strcmp(wantedInstanceExtensions[k], VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0)
                                pContext->mVk.mDebugReportExtension = true;

                            break;
                        }
                    }
                }
                SAFE_FREE(properties);
            }
        }

#if defined(QUEST_VR)
        char oculusVRInstanceExtensionBuffer[4096];
        OpenXRAddVKInstanceExt(instanceExtensionCache, &extension_count, MAX_INSTANCE_EXTENSIONS, oculusVRInstanceExtensionBuffer,
                               sizeof(oculusVRInstanceExtensionBuffer));
#endif

#if VK_HEADER_VERSION >= 108
        VkValidationFeaturesEXT      validationFeaturesExt = { VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
        VkValidationFeatureEnableEXT enabledValidationFeatures[] = {
            VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
        };

        if (pDesc->mEnableGpuBasedValidation)
        {
            validationFeaturesExt.enabledValidationFeatureCount = 1;
            validationFeaturesExt.pEnabledValidationFeatures = enabledValidationFeatures;
        }
#endif

        // Add more extensions here
        VkInstanceCreateInfo create_info = {            
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .flags = 0,
            .pApplicationInfo = &app_info,
            .enabledLayerCount = (uint32_t)arrlen(layerTemp),
            .ppEnabledLayerNames = layerTemp,
            .enabledExtensionCount = extension_count,
            .ppEnabledExtensionNames = instanceExtensionCache,
#if VK_HEADER_VERSION >= 108
            .pNext = &validationFeaturesExt,
#endif
        };

        LOGF(eINFO, "Creating VkInstance with %ti enabled instance layers:", arrlen(layerTemp));
        for (int i = 0; i < arrlen(layerTemp); i++)
            LOGF(eINFO, "\tLayer %i: %s", i, layerTemp[i]);

        CHECK_VKRESULT_INSTANCE(
            vkCreateInstance(&create_info, GetAllocationCallbacks(VK_OBJECT_TYPE_INSTANCE), &(pContext->mVk.pInstance)));
        arrfree(layerTemp);
        arrfree(wantedInstanceExtensions);
    }

    // Load Vulkan instance functions
    volkLoadInstance(pContext->mVk.pInstance);

    // Debug
#if defined(ENABLE_GRAPHICS_VALIDATION)
    VkResult debugCallbackRes = VK_ERROR_UNKNOWN;
    if (pContext->mVk.mDebugUtilsExtension)
    {
        VkDebugUtilsMessengerCreateInfoEXT create_info = { 0 };
        create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.pfnUserCallback = DebugUtilsCallback;
        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType =
#if defined(NX64) || defined(__ANDROID__)
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
#endif
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        create_info.flags = 0;
        create_info.pUserData = NULL;
        debugCallbackRes = vkCreateDebugUtilsMessengerEXT(pContext->mVk.pInstance, &create_info,
                                                          GetAllocationCallbacks(VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT),
                                                          &(pContext->mVk.pDebugUtilsMessenger));
        if (VK_SUCCESS != debugCallbackRes)
        {
            pContext->mVk.pDebugUtilsMessenger = NULL;
            internal_log(eERROR, "vkCreateDebugUtilsMessengerEXT failed - Attempting to use debug report callback instead",
                         "internal_vk_init_instance");
        }
    }
    if (debugCallbackRes != VK_SUCCESS && pContext->mVk.mDebugReportExtension)
    {
        VkDebugReportCallbackCreateInfoEXT create_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT,
            .pNext = NULL,
            .pfnCallback = DebugReportCallback,
            .flags = VK_DEBUG_REPORT_WARNING_BIT_EXT 
                | VK_DEBUG_REPORT_ERROR_BIT_EXT 
                | VK_DEBUG_REPORT_DEBUG_BIT_EXT
        /* | VK_DEBUG_REPORT_INFORMATION_BIT_EXT*/
#if defined(NX64) || defined(__ANDROID__)
                | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT // Performance warnings are not very vaild on desktop
#endif        
        };
        VkResult res =
            vkCreateDebugReportCallbackEXT(pContext->mVk.pInstance, &create_info,
                                           GetAllocationCallbacks(VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT), &(pContext->mVk.pDebugReport));
        if (VK_SUCCESS != res)
        {
            internal_log(eERROR, "vkCreateDebugReportCallbackEXT failed - disabling Vulkan debug callbacks", "internal_vk_init_instance");
        }
    }
#endif
}

static void RemoveInstance(RendererContext* pContext)
{
    ASSERT(VK_NULL_HANDLE != pContext->mVk.pInstance);

    if (pContext->mVk.pDebugUtilsMessenger)
    {
        vkDestroyDebugUtilsMessengerEXT(pContext->mVk.pInstance, pContext->mVk.pDebugUtilsMessenger,
                                        GetAllocationCallbacks(VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT));
        pContext->mVk.pDebugUtilsMessenger = NULL;
    }
    if (pContext->mVk.pDebugReport)
    {
        vkDestroyDebugReportCallbackEXT(pContext->mVk.pInstance, pContext->mVk.pDebugReport,
                                        GetAllocationCallbacks(VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT));
        pContext->mVk.pDebugReport = NULL;
    }

    vkDestroyInstance(pContext->mVk.pInstance, GetAllocationCallbacks(VK_OBJECT_TYPE_INSTANCE));
}

static bool SelectBestGpu(Renderer* pRenderer)
{
    ASSERT(VK_NULL_HANDLE != pRenderer->pContext->mVk.pInstance);

    uint32_t gpuCount = 0;

    CHECK_VKRESULT_INSTANCE(vkEnumeratePhysicalDevices(pRenderer->pContext->mVk.pInstance, &gpuCount, NULL));

    if (gpuCount < 1)
    {
        LOGF(eERROR, "Failed to enumerate any physical Vulkan devices");
        ASSERT(gpuCount);
        return false;
    }

    // update renderer gpu settings

    GpuDesc gpuDesc[MAX_MULTIPLE_GPUS] = { 0 };

    for (uint32_t i = 0; i < pRenderer->pContext->mGpuCount; ++i)
    {
        gpuDesc[i] = pRenderer->pContext->mGpus[i];
    }
    uint32_t gpuIndex = util_select_best_gpu(gpuDesc, pRenderer->pContext->mGpuCount);
    bool     gpuSupported = util_check_is_gpu_supported(&gpuDesc[gpuIndex]);
    if (!gpuSupported)
    {
        LOGF(eERROR, "Failed to Init Renderer: %s", getUnsupportedGPUMsg());
        return false;
    }

    //  driver rejection rules from gpu.cfg
    bool driverValid = checkDriverRejectionSettings(&gpuDesc[gpuIndex]);
    if (!driverValid)
    {
        return false;
    }

    // If we don't own the instance or device, then we need to set the gpuIndex to the correct physical device
#if defined(VK_USE_DISPATCH_TABLES)
    gpuIndex = UINT32_MAX;
    for (uint32_t i = 0; i < gpuCount; i++)
    {
        if (gpus[i] == pRenderer->pGpu->mVk.pGpu)
        {
            gpuIndex = i;
        }
    }
#endif
    ASSERT(gpuIndex < pRenderer->pContext->mGpuCount);
    ASSERT(gpuIndex != UINT32_MAX);
    pRenderer->pGpu = &pRenderer->pContext->mGpus[gpuIndex];
    ASSERT(pRenderer->pGpu);

    if (VK_PHYSICAL_DEVICE_TYPE_CPU == pRenderer->pGpu->mVk.mGpuProperties.properties.deviceType)
    {
        LOGF(eERROR, "The only available GPU is of type VK_PHYSICAL_DEVICE_TYPE_CPU. Early exiting");
        ASSERT(false);
        return false;
    }

    LOGF(eINFO, "GPU[%u] is selected as default GPU", gpuIndex);
    LOGF(eINFO, "Name of selected gpu: %s", pRenderer->pGpu->mGpuVendorPreset.mGpuName);
    LOGF(eINFO, "Vendor id of selected gpu: %#x", pRenderer->pGpu->mGpuVendorPreset.mVendorId);
    LOGF(eINFO, "Model id of selected gpu: %#x", pRenderer->pGpu->mGpuVendorPreset.mModelId);
    LOGF(eINFO, "Preset of selected gpu: %s", presetLevelToString(pRenderer->pGpu->mGpuVendorPreset.mPresetLevel));

    return true;
}

#define VULKAN_MAX_QUEUE_FAMILY_COUNT 16
#define VULKAN_MAX_QUEUE_COUNT        64

static bool AddDevice(const RendererDesc* pDesc, Renderer* pRenderer)
{
    ASSERT(VK_NULL_HANDLE != pRenderer->pContext->mVk.pInstance);

    // These are the extensions that we have loaded
    const char* deviceExtensionCache[MAX_DEVICE_EXTENSIONS] = { 0 };
    uint32_t    extension_count = 0;

    VkDeviceGroupDeviceCreateInfoKHR   deviceGroupInfo = { VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO_KHR };
    VkPhysicalDeviceGroupPropertiesKHR props[MAX_LINKED_GPUS] = { 0 };

    pRenderer->mLinkedNodeCount = 1;
    if (pRenderer->mGpuMode == GPU_MODE_LINKED && pRenderer->pContext->mVk.mDeviceGroupCreationExtension)
    {
        // (not shown) fill out devCreateInfo as usual.
        uint32_t deviceGroupCount = 0;

        // Query the number of device groups
        vkEnumeratePhysicalDeviceGroupsKHR(pRenderer->pContext->mVk.pInstance, &deviceGroupCount, NULL);

        // Allocate and initialize structures to query the device groups
        for (uint32_t i = 0; i < deviceGroupCount; ++i)
        {
            props[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES_KHR;
            props[i].pNext = NULL;
        }
        CHECK_VKRESULT_INSTANCE(vkEnumeratePhysicalDeviceGroupsKHR(pRenderer->pContext->mVk.pInstance, &deviceGroupCount, props));

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

    if (pRenderer->mLinkedNodeCount < 2 && pRenderer->mGpuMode == GPU_MODE_LINKED)
    {
        pRenderer->mGpuMode = GPU_MODE_SINGLE;
    }

    if (!pDesc->pContext)
    {
        if (!SelectBestGpu(pRenderer))
        {
            return false;
        }
    }
    else
    {
        ASSERT(pDesc->mGpuIndex < pDesc->pContext->mGpuCount);
        pRenderer->pGpu = &pDesc->pContext->mGpus[pDesc->mGpuIndex];
    }

    const GpuDesc* pGpu = pRenderer->pGpu;

    uint32_t layerCount = 0;
    uint32_t extCount = 0;
    vkEnumerateDeviceLayerProperties(pGpu->mVk.pGpu, &layerCount, NULL);
    vkEnumerateDeviceExtensionProperties(pGpu->mVk.pGpu, NULL, &extCount, NULL);

    VkLayerProperties* layers = (VkLayerProperties*)alloca(sizeof(VkLayerProperties) * layerCount);
    vkEnumerateDeviceLayerProperties(pGpu->mVk.pGpu, &layerCount, layers);

    VkExtensionProperties* exts = (VkExtensionProperties*)alloca(sizeof(VkExtensionProperties) * extCount);
    vkEnumerateDeviceExtensionProperties(pGpu->mVk.pGpu, NULL, &extCount, exts);

#if defined(GFX_ENABLE_SWAPPY)
    // Swappy could require some extensions to work, so find and enable them
    uint32_t swappyExtensionCount = 0;
    char**   pSwappyExtensions = NULL;
    char*    pSwappyExtensionsData = NULL;

    if (gSwappyEnabled)
    {
        // Determine the number of required extensions.
        SwappyVk_determineDeviceExtensions(pGpu->mVk.pGpu, extCount, exts, &swappyExtensionCount, NULL);

        // Determine the required extensions.
        pSwappyExtensions = (char**)tf_malloc(swappyExtensionCount * sizeof(char*));
        pSwappyExtensionsData = (char*)tf_malloc(swappyExtensionCount * (VK_MAX_EXTENSION_NAME_SIZE + 1));
        for (uint32_t i = 0; i < swappyExtensionCount; i++)
        {
            pSwappyExtensions[i] = &pSwappyExtensionsData[i * (VK_MAX_EXTENSION_NAME_SIZE + 1)];
        }
        SwappyVk_determineDeviceExtensions(pGpu->mVk.pGpu, extCount, exts, &swappyExtensionCount, pSwappyExtensions);

        for (uint32_t i = 0; i < swappyExtensionCount; i++)
        {
            deviceExtensionCache[extension_count++] = pSwappyExtensions[i];
        }
    }
#endif

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

    // Standalone extensions
    {
        const char* layer_name = NULL;
        uint32_t    initialCount = TF_ARRAY_COUNT(gVkWantedDeviceExtensions);
        // Dont need debug marker if we have debug utils
        initialCount -= pRenderer->pContext->mVk.mDebugUtilsExtension ? 1 : 0;
        const uint32_t userRequestedCount = (uint32_t)pDesc->mVk.mDeviceExtensionCount;
        const char**   wantedDeviceExtensions = NULL;
        arrsetlen(wantedDeviceExtensions, initialCount + userRequestedCount + 1);
        uint32_t wantedExtensionCount = 0;
        for (uint32_t i = 0; i < initialCount; ++i, ++wantedExtensionCount)
        {
            wantedDeviceExtensions[i] = gVkWantedDeviceExtensions[i];
        }
        for (uint32_t i = 0; i < userRequestedCount; ++i, ++wantedExtensionCount)
        {
            wantedDeviceExtensions[initialCount + i] = pDesc->mVk.ppDeviceExtensions[i]; //-V595
        }
#if defined(SHADER_STATS_AVAILABLE)
        if (pDesc->mEnableShaderStats)
        {
            wantedDeviceExtensions[wantedExtensionCount++] = VK_AMD_SHADER_INFO_EXTENSION_NAME;
        }
#endif
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(pRenderer->pGpu->mVk.pGpu, layer_name, &count, NULL);
        if (count > 0)
        {
            VkExtensionProperties* properties = (VkExtensionProperties*)tf_calloc(count, sizeof(*properties));
            ASSERT(properties != NULL);
            vkEnumerateDeviceExtensionProperties(pRenderer->pGpu->mVk.pGpu, layer_name, &count, properties);
            for (uint32_t j = 0; j < count; ++j)
            {
                for (uint32_t k = 0; k < wantedExtensionCount; ++k)
                {
                    if (strcmp(wantedDeviceExtensions[k], properties[j].extensionName) == 0)
                    {
                        deviceExtensionCache[extension_count++] = wantedDeviceExtensions[k];
                        break;
                    }
                }
            }
            SAFE_FREE(properties);
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
    VkBaseOutStructure*          base = (VkBaseOutStructure*)&gpuFeatures2; //-V1027

    // Add more extensions here
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragmentShaderInterlockFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT
    };
    ADD_TO_NEXT_CHAIN(pGpu->mFragmentShaderInterlockExtension, fragmentShaderInterlockFeatures);
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexingFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT
    };
    ADD_TO_NEXT_CHAIN(pGpu->mDescriptorIndexingExtension, descriptorIndexingFeatures);
    VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcrFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES };
    ADD_TO_NEXT_CHAIN(pGpu->mYCbCrExtension, ycbcrFeatures);
#if defined(QUEST_VR)
    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES };
    ADD_TO_NEXT_CHAIN(pGpu->mMultiviewExtension, multiviewFeatures);
#endif
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR
    };
    ADD_TO_NEXT_CHAIN(pGpu->mDynamicRenderingExtension, dynamicRenderingFeatures);

    VkPhysicalDeviceShaderAtomicInt64Features shaderAtomicInt64Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR
    };
    ADD_TO_NEXT_CHAIN(pGpu->mShaderAtomicInt64Extension, shaderAtomicInt64Features);

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES
    };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
    };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
    };
    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };

    ADD_TO_NEXT_CHAIN(pGpu->mBufferDeviceAddressExtension, bufferDeviceAddressFeatures);
    ADD_TO_NEXT_CHAIN(pGpu->mRayTracingPipelineExtension, rayTracingPipelineFeatures);
    ADD_TO_NEXT_CHAIN(pGpu->mAccelerationStructureExtension, accelerationStructureFeatures);
    ADD_TO_NEXT_CHAIN(pGpu->mRayQueryExtension, rayQueryFeatures);

    VkPhysicalDeviceFaultFeaturesEXT deviceFaultFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT };
    ADD_TO_NEXT_CHAIN(pGpu->mDeviceFaultExtension, deviceFaultFeatures);

    VkPhysicalDeviceDeviceMemoryReportFeaturesEXT memoryReportFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_MEMORY_REPORT_FEATURES_EXT
    };
    ADD_TO_NEXT_CHAIN(pGpu->mDeviceMemoryReportExtension, memoryReportFeatures);

    VkPhysicalDeviceCoherentMemoryFeaturesAMD deviceCoherentFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD };
    ADD_TO_NEXT_CHAIN(pGpu->mAMDDeviceCoherentMemoryExtension, deviceCoherentFeatures);

    vkGetPhysicalDeviceFeatures2KHR(pRenderer->pGpu->mVk.pGpu, &gpuFeatures2);

#if defined(GFX_DEVICE_MEMORY_TRACKING)
    // Note: on some devices this extention causes crash on swapchain creation even though it is reported as supported
    gEnableDeviceMemoryTracking &= !pRenderer->pGpu->mDeviceMemoryReportCrashWorkaround;

    VkDeviceDeviceMemoryReportCreateInfoEXT memoryReport = { 0 };
    memoryReport.flags = 0;
    memoryReport.pfnUserCallback = MemoryReportCallback;
    memoryReport.pUserData = (void*)&gDeviceMemStats.mMemoryAllocCount;
    memoryReport.sType = VK_STRUCTURE_TYPE_DEVICE_DEVICE_MEMORY_REPORT_CREATE_INFO_EXT;
    ADD_TO_NEXT_CHAIN(gEnableDeviceMemoryTracking, memoryReport);
#endif

    // Get queue family properties
    uint32_t queueFamiliesCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->pGpu->mVk.pGpu, &queueFamiliesCount, NULL);
    VkQueueFamilyProperties* queueFamiliesProperties =
        (VkQueueFamilyProperties*)alloca(queueFamiliesCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->pGpu->mVk.pGpu, &queueFamiliesCount, queueFamiliesProperties);

    // need a queue_priority for each queue in the queue family we create
    float                    queueFamilyPriorities[VULKAN_MAX_QUEUE_FAMILY_COUNT][VULKAN_MAX_QUEUE_COUNT] = { 0 };
    uint32_t                 queue_create_infos_count = 0;
    VkDeviceQueueCreateInfo* queue_create_infos = (VkDeviceQueueCreateInfo*)alloca(queueFamiliesCount * sizeof(VkDeviceQueueCreateInfo));

    pRenderer->mVk.pAvailableQueueCount = (uint32_t**)tf_malloc(pRenderer->mLinkedNodeCount * sizeof(uint32_t*));
    pRenderer->mVk.pUsedQueueCount = (uint32_t**)tf_malloc(pRenderer->mLinkedNodeCount * sizeof(uint32_t*));
    for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
    {
        pRenderer->mVk.pAvailableQueueCount[i] = (uint32_t*)tf_calloc(queueFamiliesCount, sizeof(uint32_t));
        pRenderer->mVk.pUsedQueueCount[i] = (uint32_t*)tf_calloc(queueFamiliesCount, sizeof(uint32_t));
    }

    for (uint32_t i = 0; i < queueFamiliesCount; i++)
    {
        uint32_t queueCount = queueFamiliesProperties[i].queueCount;
        if (queueCount > 0)
        {
            // Request only one queue of each type if mRequestAllAvailableQueues is not set to true
            if (queueCount > 1 && !pDesc->mVk.mRequestAllAvailableQueues)
            {
                queueCount = 1;
            }

            ASSERT(queueCount <= VULKAN_MAX_QUEUE_COUNT);
            queueCount = MIN(queueCount, VULKAN_MAX_QUEUE_COUNT);

            queue_create_infos[queue_create_infos_count++] = (VkDeviceQueueCreateInfo){
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .queueFamilyIndex = i,
                .queueCount = queueCount,
                .pQueuePriorities = queueFamilyPriorities[i],
            };

            for (uint32_t n = 0; n < pRenderer->mLinkedNodeCount; ++n)
            {
                pRenderer->mVk.pAvailableQueueCount[n][i] = queueCount;
            }
        }
    }

#if defined(QUEST_VR)
    char oculusVRDeviceExtensionBuffer[4096];
    OpenXRAddVKDeviceExt(deviceExtensionCache, &extension_count, MAX_DEVICE_EXTENSIONS, oculusVRDeviceExtensionBuffer,
                         sizeof(oculusVRDeviceExtensionBuffer));
#endif

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &gpuFeatures2,
        .flags = 0,
        .queueCreateInfoCount = queue_create_infos_count,
        .pQueueCreateInfos = queue_create_infos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = deviceExtensionCache,
        .pEnabledFeatures = NULL,
    };

    /************************************************************************/
    // Add Device Group Extension if requested and available
    /************************************************************************/
    ADD_TO_NEXT_CHAIN(pRenderer->mGpuMode == GPU_MODE_LINKED, deviceGroupInfo);
    CHECK_VKRESULT_INSTANCE(
        vkCreateDevice(pGpu->mVk.pGpu, &create_info, GetAllocationCallbacks(VK_OBJECT_TYPE_DEVICE), &pRenderer->mVk.pDevice));

    // Load Vulkan device functions to bypass loader
    if (pRenderer->mGpuMode != GPU_MODE_UNLINKED)
    {
        volkLoadDevice(pRenderer->mVk.pDevice);
    }
#endif

    if (pGpu->mDedicatedAllocationExtension)
    {
        LOGF(eINFO, "Successfully loaded Dedicated Allocation extension");
    }

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    if (pGpu->mExternalMemoryExtension)
    {
        LOGF(eINFO, "Successfully loaded External Memory extension");
    }
#endif

    if (pGpu->mDrawIndirectCountExtension)
    {
        pfnVkCmdDrawIndirectCountKHR = vkCmdDrawIndirectCountKHR;
        pfnVkCmdDrawIndexedIndirectCountKHR = vkCmdDrawIndexedIndirectCountKHR;
        LOGF(eINFO, "Successfully loaded Draw Indirect extension");
    }
    else if (pGpu->mAMDDrawIndirectCountExtension)
    {
        pfnVkCmdDrawIndirectCountKHR = vkCmdDrawIndirectCountAMD;
        pfnVkCmdDrawIndexedIndirectCountKHR = vkCmdDrawIndexedIndirectCountAMD;
        LOGF(eINFO, "Successfully loaded AMD Draw Indirect extension");
    }

    if (pGpu->mAMDGCNShaderExtension)
    {
        LOGF(eINFO, "Successfully loaded AMD GCN Shader extension");
    }

    if (pGpu->mAMDShaderInfoExtension)
    {
        LOGF(eINFO, "Successfully loaded AMD Shader Info extension");
    }

    if (pGpu->mDescriptorIndexingExtension)
    {
        LOGF(eINFO, "Successfully loaded Descriptor Indexing extension");
    }

    if (pGpu->mRayPipelineSupported)
    {
        LOGF(eINFO, "Successfully loaded Khronos Ray Pipeline extensions");
    }

    if (pGpu->mRayQuerySupported)
    {
        LOGF(eINFO, "Successfully loaded Khronos Ray Query extensions");
    }

    if (pGpu->m64BitAtomicsSupported)
    {
        LOGF(eINFO, "Successfully loaded 64 bit Shader Atomic extension");
    }

    if (pGpu->mDynamicRenderingSupported)
    {
        LOGF(eINFO, "Successfully loaded Dynamic Rendering extension");
    }

    if (pGpu->mDeviceFaultExtension)
    {
        LOGF(eINFO, "Successfully loaded Device Fault extension");
    }

#if defined(GFX_ENABLE_SWAPPY)
    if (gSwappyEnabled)
    {
        tf_free(pSwappyExtensions);
        tf_free(pSwappyExtensionsData);
    }
#endif

    return true;
}

static void RemoveDevice(Renderer* pRenderer)
{
    vkDestroyDescriptorSetLayout(pRenderer->mVk.pDevice, pRenderer->mVk.pEmptyDescriptorSetLayout,
                                 GetAllocationCallbacks(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT));
    vkDestroyDescriptorPool(pRenderer->mVk.pDevice, pRenderer->mVk.pEmptyDescriptorPool,
                            GetAllocationCallbacks(VK_OBJECT_TYPE_DESCRIPTOR_POOL));
    vkDestroyDevice(pRenderer->mVk.pDevice, GetAllocationCallbacks(VK_OBJECT_TYPE_DEVICE));
}

VkDeviceMemory get_vk_device_memory(Renderer* pRenderer, Buffer* pBuffer)
{
    VmaAllocationInfo allocInfo = { 0 };
    vmaGetAllocationInfo(pRenderer->mVk.pVmaAllocator, pBuffer->mVk.pAllocation, &allocInfo);
    return allocInfo.deviceMemory;
}

uint64_t get_vk_device_memory_offset(Renderer* pRenderer, Buffer* pBuffer)
{
    VmaAllocationInfo allocInfo = { 0 };
    vmaGetAllocationInfo(pRenderer->mVk.pVmaAllocator, pBuffer->mVk.pAllocation, &allocInfo);
    return (uint64_t)allocInfo.offset;
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

    RendererContext* pContext = (RendererContext*)tf_calloc_memalign(1, 64, sizeof(RendererContext));

    for (uint32_t i = 0; i < TF_ARRAY_COUNT(pContext->mGpus); ++i)
    {
        setDefaultGPUProperties(&pContext->mGpus[i]);
    }

    AGSReturnCode agsRet = (AGSReturnCode)agsInit();
    if (AGS_SUCCESS == agsRet)
    {
        agsPrintDriverInfo();
    }

    NvAPI_Status nvStatus = nvapiInit();
    if (NVAPI_OK == nvStatus)
    {
        nvapiPrintDriverInfo();
    }

#if defined(VK_USE_DISPATCH_TABLES)
    VkResult vkRes = volkInitializeWithDispatchTables(pRenderer);
    if (vkRes != VK_SUCCESS)
    {
        LOGF(eERROR, "Failed to initialize Vulkan");
        return false;
    }
#else
    const char** instanceLayers = (const char**)alloca((2 + pDesc->mVk.mInstanceLayerCount) * sizeof(char*));
    uint32_t instanceLayerCount = 0;

#if defined(GFX_DEVICE_MEMORY_TRACKING)
    initMutex(&gDeviceMemStats.mMemoryMapMutex);
#endif

#if defined(ENABLE_GRAPHICS_VALIDATION)
    // this turns on all validation layers
    instanceLayers[instanceLayerCount++] = "VK_LAYER_KHRONOS_validation";
#endif

#if defined(NX64) && defined(FORGE_DEBUG)
    instanceLayers[instanceLayerCount++] = "VK_LAYER_RENDERDOC_Capture";
#endif

    // Add user specified instance layers for instance creation
    for (uint32_t i = 0; i < (uint32_t)pDesc->mVk.mInstanceLayerCount; ++i)
        instanceLayers[instanceLayerCount++] = pDesc->mVk.ppInstanceLayers[i];

    VkResult vkRes = volkInitialize();
    if (vkRes != VK_SUCCESS)
    {
        LOGF(eERROR, "Failed to initialize Vulkan");
        return;
    }

    CreateInstance(appName, pDesc, instanceLayerCount, instanceLayers, pContext);
#endif

    uint32_t gpuCount = 0;
    CHECK_VKRESULT_INSTANCE(vkEnumeratePhysicalDevices(pContext->mVk.pInstance, &gpuCount, NULL));
    gpuCount = MIN((uint32_t)MAX_MULTIPLE_GPUS, gpuCount);

    VkPhysicalDevice gpus[MAX_MULTIPLE_GPUS] = { 0 };
    CHECK_VKRESULT_INSTANCE(vkEnumeratePhysicalDevices(pContext->mVk.pInstance, &gpuCount, gpus));

    bool     gpuValid[MAX_MULTIPLE_GPUS] = { 0 };
    uint32_t realGpuCount = 0;

    for (uint32_t i = 0; i < gpuCount; ++i)
    {
        // Get queue family properties
        uint32_t queueFamilyPropertyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpus[i], &queueFamilyPropertyCount, NULL);
        VkQueueFamilyProperties* queueFamilyProperties =
            (VkQueueFamilyProperties*)tf_calloc(queueFamilyPropertyCount, sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(gpus[i], &queueFamilyPropertyCount, queueFamilyProperties);

        // Filter GPUs that don't meet requirements
        bool isGraphicsQueueAvailable = false;
        for (uint32_t j = 0; j < queueFamilyPropertyCount; j++)
        {
            // get graphics queue family
            if (queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                isGraphicsQueueAvailable = true;
                break;
            }
        }

        VkPhysicalDeviceProperties gpuProperties = { 0 };
        vkGetPhysicalDeviceProperties(gpus[i], &gpuProperties);
        gpuValid[i] = isGraphicsQueueAvailable && (VK_PHYSICAL_DEVICE_TYPE_CPU != gpuProperties.deviceType);
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
        {
            continue;
        }

        pContext->mGpus[realGpu].mVk.pGpu = gpus[i];
        QueryGpuInfo(pDesc, pContext, &pContext->mGpus[realGpu]);
        vkCapsBuilder(&pContext->mGpus[realGpu]);
        applyGPUConfigurationRules(&pContext->mGpus[i]);

        // ----- update vulkan features based on gpu.cfg
        pContext->mGpus[realGpu].mDynamicRenderingSupported &= gEnableDynamicRenderingExtension;

        LOGF(eINFO, "GPU[%u] detected. Vendor ID: %#x, Model ID: %#x, Preset: %s, GPU Name: %s", realGpu,
             pContext->mGpus[realGpu].mGpuVendorPreset.mVendorId, pContext->mGpus[realGpu].mGpuVendorPreset.mModelId,
             presetLevelToString(pContext->mGpus[realGpu].mGpuVendorPreset.mPresetLevel),
             pContext->mGpus[realGpu].mGpuVendorPreset.mGpuName);

        ++realGpu;
    }

    *ppContext = pContext;
}

void exitRendererContext(RendererContext* pContext)
{
    ASSERT(gRendererCount == 0);

    agsExit();
    nvapiExit();

#if !defined(VK_USE_DISPATCH_TABLES)
    RemoveInstance(pContext);
#endif

#if defined(GFX_DRIVER_MEMORY_TRACKING)
    memset(&gDriverMemStats, 0, sizeof(DriverMemoryStats));
#endif
#if defined(GFX_DEVICE_MEMORY_TRACKING)
    hmfree(gDeviceMemStats.mMemoryMap);
    exitMutex(&gDeviceMemStats.mMemoryMapMutex);
    memset(&gDeviceMemStats, 0, sizeof(DeviceMemoryStats));
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

    uint8_t* mem = (uint8_t*)tf_calloc_memalign(1, ALIGN_Renderer, sizeof(Renderer) + sizeof(NullDescriptors));
    ASSERT(mem);

    Renderer* pRenderer = (Renderer*)mem;
    pRenderer->mRendererApi = RENDERER_API_VULKAN;
    pRenderer->mGpuMode = pDesc->mGpuMode;
    pRenderer->mShaderTarget = pDesc->mShaderTarget;
    pRenderer->pNullDescriptors = (NullDescriptors*)(mem + sizeof(Renderer));
    pRenderer->pName = appName;

    // Initialize the Vulkan internal bits
    {
        ASSERT(pDesc->mGpuMode != GPU_MODE_UNLINKED || pDesc->pContext); // context required in unlinked mode
        if (pDesc->pContext)
        {
            ASSERT(pDesc->mGpuIndex < pDesc->pContext->mGpuCount);
            pRenderer->mOwnsContext = false;
            pRenderer->pContext = pDesc->pContext;
            pRenderer->mUnlinkedRendererIndex = gRendererCount;

            if (pDesc->pContext->mGpuCount > 1 && pDesc->mGpuMode == GPU_MODE_UNLINKED)
            {
                bool           dynamicRenderingSupportMismatch = false;
                const GpuDesc* firstGpuInfo = &pDesc->pContext->mGpus[0];
                for (uint32_t gpuIndex = 1; gpuIndex < pDesc->pContext->mGpuCount; ++gpuIndex)
                {
                    const GpuDesc* gpuDesc = &pDesc->pContext->mGpus[gpuIndex];
                    if (gpuDesc->mDynamicRenderingSupported != firstGpuInfo->mDynamicRenderingSupported)
                    {
                        dynamicRenderingSupportMismatch = true;
                        break;
                    }
                }

                if (dynamicRenderingSupportMismatch)
                {
                    for (uint32_t gpuIndex = 0; gpuIndex < pDesc->pContext->mGpuCount; ++gpuIndex)
                    {
                        GpuDesc* gpuDesc = &pDesc->pContext->mGpus[gpuIndex];
                        if (gpuDesc->mDynamicRenderingSupported)
                        {
                            gpuDesc->mDynamicRenderingSupported = 0;
                            LOGF(eWARNING, "Dynamic rendering feature was disabled for gpu: %u: %s.", gpuIndex,
                                 gpuDesc->mVk.mGpuProperties.properties.deviceName);
                        }
                    }
                }
            }
        }
        else
        {
            RendererContextDesc contextDesc = { 0 };
            contextDesc.mEnableGpuBasedValidation = pDesc->mEnableGpuBasedValidation;
#if defined(SHADER_STATS_AVAILABLE)
            contextDesc.mEnableShaderStats = pDesc->mEnableShaderStats;
#endif
            COMPILE_ASSERT(sizeof(contextDesc.mVk) == sizeof(pDesc->mVk));
            memcpy(&contextDesc.mVk, &pDesc->mVk, sizeof(pDesc->mVk));
            initRendererContext(appName, &contextDesc, &pRenderer->pContext);
            pRenderer->mOwnsContext = true;
            if (!pRenderer->pContext)
            {
                SAFE_FREE(pRenderer);
                return;
            }
        }

        if (!AddDevice(pDesc, pRenderer))
        {
            if (pRenderer->mOwnsContext)
            {
                exitRendererContext(pRenderer->pContext);
            }
            SAFE_FREE(pRenderer);
            return;
        }

        // anything below LOW preset is not supported and we will exit
        if (pRenderer->pGpu->mGpuVendorPreset.mPresetLevel < GPU_PRESET_VERYLOW)
        {
            // remove device and any memory we allocated in just above as this is the first function called
            // when initializing the forge
            RemoveDevice(pRenderer);
            if (pRenderer->mOwnsContext)
            {
                exitRendererContext(pRenderer->pContext);
            }
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
        /************************************************************************/
        // Memory allocator
        /************************************************************************/
        VmaAllocatorCreateInfo createInfo = { 0 };
        createInfo.device = pRenderer->mVk.pDevice;
        createInfo.physicalDevice = pRenderer->pGpu->mVk.pGpu;
        createInfo.instance = pRenderer->pContext->mVk.pInstance;

        if (pRenderer->pGpu->mDedicatedAllocationExtension)
        {
            createInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
        }

        if (pRenderer->pGpu->mBufferDeviceAddressSupported)
        {
            createInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        }

        if (pRenderer->pGpu->mAMDDeviceCoherentMemorySupported)
        {
            createInfo.flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT;
        }

        VmaVulkanFunctions vulkanFunctions = { 0 };
        vulkanFunctions.vkGetInstanceProcAddr = _vkGetInstanceProcAddr;
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
        vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR =
            vkGetPhysicalDeviceMemoryProperties2KHR ? vkGetPhysicalDeviceMemoryProperties2KHR : vkGetPhysicalDeviceMemoryProperties2;
#endif
#if VMA_VULKAN_VERSION >= 1003000
        /// Fetch from "vkGetDeviceBufferMemoryRequirements" on Vulkan >= 1.3, but you can also fetch it from
        /// "vkGetDeviceBufferMemoryRequirementsKHR" if you enabled extension VK_KHR_maintenance4.
        vulkanFunctions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
        /// Fetch from "vkGetDeviceImageMemoryRequirements" on Vulkan >= 1.3, but you can also fetch it from
        /// "vkGetDeviceImageMemoryRequirementsKHR" if you enabled extension VK_KHR_maintenance4.
        vulkanFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;
#endif

        createInfo.pVulkanFunctions = &vulkanFunctions;
        createInfo.pAllocationCallbacks = GetAllocationCallbacks(VK_OBJECT_TYPE_DEVICE_MEMORY);
        vmaCreateAllocator(&createInfo, &pRenderer->mVk.pVmaAllocator);
    }

    // Empty descriptor set for filling in gaps when example: set 1 is used but set 0 is not used in the shader.
    // We still need to bind empty descriptor set here to keep some drivers happy
    VkDescriptorPoolSize descriptorPoolSizes[1] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1 } };
    AddDescriptorPool(pRenderer, 1, 0, descriptorPoolSizes, 1, &pRenderer->mVk.pEmptyDescriptorPool);
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = { 0 };
    VkDescriptorSet*                emptySets[] = { &pRenderer->mVk.pEmptyDescriptorSet };
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    CHECK_VKRESULT(vkCreateDescriptorSetLayout(pRenderer->mVk.pDevice, &layoutCreateInfo,
                                               GetAllocationCallbacks(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT),
                                               &pRenderer->mVk.pEmptyDescriptorSetLayout));
    AllocateDescriptorSets(pRenderer, pRenderer->mVk.pEmptyDescriptorPool, &pRenderer->mVk.pEmptyDescriptorSetLayout, 1, emptySets);

    if (!pRenderer->pGpu->mDynamicRenderingSupported)
    {
        initMutex(&gRenderPassMutex[pRenderer->mUnlinkedRendererIndex]);

        gRenderPassMap[pRenderer->mUnlinkedRendererIndex] = NULL;
        hmdefault(gRenderPassMap[pRenderer->mUnlinkedRendererIndex], NULL);
        gFrameBufferMap[pRenderer->mUnlinkedRendererIndex] = NULL;
        hmdefault(gFrameBufferMap[pRenderer->mUnlinkedRendererIndex], NULL);
    }

    initMutex(&gDescriptorLayoutMutex[pRenderer->mUnlinkedRendererIndex]);
    gDescriptorLayoutMap[pRenderer->mUnlinkedRendererIndex] = NULL;
    hmdefault(gDescriptorLayoutMap[pRenderer->mUnlinkedRendererIndex], NULL);
    gPipelineLayoutMap[pRenderer->mUnlinkedRendererIndex] = NULL;
    hmdefault(gPipelineLayoutMap[pRenderer->mUnlinkedRendererIndex], NULL);
    gStaticSamplerMap[pRenderer->mUnlinkedRendererIndex] = NULL;
    hmdefault(gStaticSamplerMap[pRenderer->mUnlinkedRendererIndex], NULL);

    VkPhysicalDeviceFeatures2KHR gpuFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
    vkGetPhysicalDeviceFeatures2KHR(pRenderer->pGpu->mVk.pGpu, &gpuFeatures);

    if (pRenderer->pGpu->mNonUniformResourceIndexSupported)
    {
        LOGF(eINFO, "GPU supports `nonuniformEXT`");
    }

    util_find_queue_family_index(pRenderer, 0, QUEUE_TYPE_GRAPHICS, NULL, &pRenderer->mVk.mGraphicsQueueFamilyIndex, NULL);
    util_find_queue_family_index(pRenderer, 0, QUEUE_TYPE_COMPUTE, NULL, &pRenderer->mVk.mComputeQueueFamilyIndex, NULL);
    util_find_queue_family_index(pRenderer, 0, QUEUE_TYPE_TRANSFER, NULL, &pRenderer->mVk.mTransferQueueFamilyIndex, NULL);

    add_default_resources(pRenderer);

#if defined(QUEST_VR)
    if (!OpenXRPostInitVKRenderer(pRenderer, pRenderer->pContext->mVk.pInstance, pRenderer->mVk.pDevice,
                                  pRenderer->mVk.mGraphicsQueueFamilyIndex))
    {
        vmaDestroyAllocator(pRenderer->mVk.pVmaAllocator);
        SAFE_FREE(pRenderer->pName);
#if !defined(VK_USE_DISPATCH_TABLES)
        RemoveDevice(pRenderer);
        if (pDesc->mGpuMode != GPU_MODE_UNLINKED)
            exitRendererContext(pRenderer->pContext);
        SAFE_FREE(pRenderer);
        LOGF(eERROR, "Failed to initialize VrApi Vulkan systems.");
#endif
        *ppRenderer = NULL;
        return;
    }
#endif // QUEST_VR

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

    if (!pRenderer->pGpu->mDynamicRenderingSupported)
    {
        exitMutex(&gRenderPassMutex[pRenderer->mUnlinkedRendererIndex]);

        // Remove the renderpasses
        for (ptrdiff_t i = 0; i < hmlen(gRenderPassMap[pRenderer->mUnlinkedRendererIndex]); ++i)
        {
            RenderPassNode** pMap = gRenderPassMap[pRenderer->mUnlinkedRendererIndex][i].value;
            RenderPassNode*  map = *pMap;
            for (ptrdiff_t j = 0; j < hmlen(map); ++j)
            {
                RemoveRenderPass(pRenderer, &map[j].value);
            }
            hmfree(map);
            tf_free(pMap);
        }
        hmfree(gRenderPassMap[pRenderer->mUnlinkedRendererIndex]);

        for (ptrdiff_t i = 0; i < hmlen(gFrameBufferMap[pRenderer->mUnlinkedRendererIndex]); ++i)
        {
            FrameBufferNode** pMap = gFrameBufferMap[pRenderer->mUnlinkedRendererIndex][i].value;
            FrameBufferNode*  map = *pMap;
            for (ptrdiff_t j = 0; j < hmlen(map); ++j)
            {
                RemoveFramebuffer(pRenderer, &map[j].value);
            }
            hmfree(map);
            tf_free(pMap);
        }
        hmfree(gFrameBufferMap[pRenderer->mUnlinkedRendererIndex]);

        SAFE_FREE(gRenderPassMap[pRenderer->mUnlinkedRendererIndex]);
        SAFE_FREE(gFrameBufferMap[pRenderer->mUnlinkedRendererIndex]);
    }

    exitMutex(&gDescriptorLayoutMutex[pRenderer->mUnlinkedRendererIndex]);
    for (ptrdiff_t i = 0; i < hmlen(gDescriptorLayoutMap[pRenderer->mUnlinkedRendererIndex]); ++i)
    {
        DescriptorLayoutNode** pMap = gDescriptorLayoutMap[pRenderer->mUnlinkedRendererIndex][i].value;
        DescriptorLayoutNode*  map = *pMap;
        for (ptrdiff_t j = 0; j < hmlen(map); ++j)
        {
            vkDestroyDescriptorSetLayout(pRenderer->mVk.pDevice, map[j].value,
                                         GetAllocationCallbacks(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT));
        }
        hmfree(map);
        tf_free(pMap);
    }
    hmfree(gDescriptorLayoutMap[pRenderer->mUnlinkedRendererIndex]);

    for (ptrdiff_t i = 0; i < hmlen(gPipelineLayoutMap[pRenderer->mUnlinkedRendererIndex]); ++i)
    {
        PipelineLayoutNode** pMap = gPipelineLayoutMap[pRenderer->mUnlinkedRendererIndex][i].value;
        PipelineLayoutNode*  map = *pMap;
        for (ptrdiff_t j = 0; j < hmlen(map); ++j)
        {
            vkDestroyPipelineLayout(pRenderer->mVk.pDevice, map[j].value->pLayout, GetAllocationCallbacks(VK_OBJECT_TYPE_PIPELINE_LAYOUT));
            SAFE_FREE(map[j].value);
        }
        hmfree(map);
        tf_free(pMap);
    }
    hmfree(gPipelineLayoutMap[pRenderer->mUnlinkedRendererIndex]);

    for (ptrdiff_t i = 0; i < hmlen(gStaticSamplerMap[pRenderer->mUnlinkedRendererIndex]); ++i)
    {
        StaticSamplerNode** pMap = gStaticSamplerMap[pRenderer->mUnlinkedRendererIndex][i].value;
        StaticSamplerNode*  map = *pMap;
        for (ptrdiff_t j = 0; j < hmlen(map); ++j)
        {
            removeSampler(pRenderer, map[j].value);
        }
        hmfree(map);
        tf_free(pMap);
    }
    hmfree(gStaticSamplerMap[pRenderer->mUnlinkedRendererIndex]);

    SAFE_FREE(gDescriptorLayoutMap[pRenderer->mUnlinkedRendererIndex]);
    SAFE_FREE(gPipelineLayoutMap[pRenderer->mUnlinkedRendererIndex]);
    SAFE_FREE(gStaticSamplerMap[pRenderer->mUnlinkedRendererIndex]);

    // Destroy the Vulkan bits
    vmaDestroyAllocator(pRenderer->mVk.pVmaAllocator);

#if defined(VK_USE_DISPATCH_TABLES)
#else
    RemoveDevice(pRenderer);
#endif
    if (pRenderer->mOwnsContext)
    {
        exitRendererContext(pRenderer->pContext);
    }

    for (uint32_t i = 0; i < pRenderer->mLinkedNodeCount; ++i)
    {
        SAFE_FREE(pRenderer->mVk.pAvailableQueueCount[i]);
        SAFE_FREE(pRenderer->mVk.pUsedQueueCount[i]);
    }

    // Free all the renderer components!
    SAFE_FREE(pRenderer->mVk.pAvailableQueueCount);
    SAFE_FREE(pRenderer->mVk.pUsedQueueCount);
    SAFE_FREE(pRenderer);
}

/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void initFence(Renderer* pRenderer, Fence** ppFence)
{
    ASSERT(pRenderer);
    ASSERT(ppFence);
    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);

    Fence* pFence = (Fence*)tf_calloc(1, sizeof(Fence));
    ASSERT(pFence);

    VkFenceCreateInfo add_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    CHECK_VKRESULT(vkCreateFence(pRenderer->mVk.pDevice, &add_info, GetAllocationCallbacks(VK_OBJECT_TYPE_FENCE), &pFence->mVk.pFence));

    *ppFence = pFence;
}

void exitFence(Renderer* pRenderer, Fence* pFence)
{
    ASSERT(pRenderer);
    ASSERT(pFence);
    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);
    ASSERT(VK_NULL_HANDLE != pFence->mVk.pFence);

    vkDestroyFence(pRenderer->mVk.pDevice, pFence->mVk.pFence, GetAllocationCallbacks(VK_OBJECT_TYPE_FENCE));

    SAFE_FREE(pFence);
}

void initSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
    ASSERT(pRenderer);
    ASSERT(ppSemaphore);
    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);

    Semaphore* pSemaphore = (Semaphore*)tf_calloc(1, sizeof(Semaphore));
    ASSERT(pSemaphore);

    VkSemaphoreCreateInfo add_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    CHECK_VKRESULT(vkCreateSemaphore(pRenderer->mVk.pDevice, &add_info, GetAllocationCallbacks(VK_OBJECT_TYPE_SEMAPHORE),
                                     &(pSemaphore->mVk.pSemaphore)));
    // Set signal initial state.
    pSemaphore->mVk.mSignaled = false;

    *ppSemaphore = pSemaphore;
}

void exitSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
    ASSERT(pRenderer);
    ASSERT(pSemaphore);
    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);
    ASSERT(VK_NULL_HANDLE != pSemaphore->mVk.pSemaphore);

    vkDestroySemaphore(pRenderer->mVk.pDevice, pSemaphore->mVk.pSemaphore, GetAllocationCallbacks(VK_OBJECT_TYPE_SEMAPHORE));

    SAFE_FREE(pSemaphore);
}

void initQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
{
    ASSERT(pDesc != NULL);

    const uint32_t          nodeIndex = (pRenderer->mGpuMode == GPU_MODE_LINKED) ? pDesc->mNodeIndex : 0;
    VkQueueFamilyProperties queueProps = { 0 };
    uint8_t                 queueFamilyIndex = UINT8_MAX;
    uint8_t                 queueIndex = UINT8_MAX;

    util_find_queue_family_index(pRenderer, nodeIndex, pDesc->mType, &queueProps, &queueFamilyIndex, &queueIndex);
    ++pRenderer->mVk.pUsedQueueCount[nodeIndex][queueFamilyIndex];

    Queue* pQueue = (Queue*)tf_calloc(1, sizeof(Queue));
    ASSERT(pQueue);

    pQueue->mVk.mQueueFamilyIndex = queueFamilyIndex;
    pQueue->mNodeIndex = pDesc->mNodeIndex;
    pQueue->mType = pDesc->mType;
    pQueue->mVk.mQueueIndex = queueIndex;
    pQueue->mVk.mGpuMode = pRenderer->mGpuMode;
    pQueue->mVk.mTimestampPeriod = pRenderer->pGpu->mVk.mGpuProperties.properties.limits.timestampPeriod;
    pQueue->mVk.pRenderer = pRenderer;
    pQueue->mVk.pSubmitMutex = &pRenderer->pNullDescriptors->mSubmitMutex;
    // override node index
    if (pRenderer->mGpuMode == GPU_MODE_UNLINKED)
    {
        pQueue->mNodeIndex = pRenderer->mUnlinkedRendererIndex;
    }

    // Get queue handle
    vkGetDeviceQueue(pRenderer->mVk.pDevice, pQueue->mVk.mQueueFamilyIndex, pQueue->mVk.mQueueIndex, &pQueue->mVk.pQueue);
    ASSERT(VK_NULL_HANDLE != pQueue->mVk.pQueue);

    const bool firstQueue = 1 == pRenderer->mVk.pUsedQueueCount[nodeIndex][queueFamilyIndex];
    if (firstQueue)
    {
        const char* queueNames[] = {
            "GRAPHICS QUEUE",
            "COMPUTE QUEUE",
            "COPY QUEUE",
        };
        COMPILE_ASSERT(TF_ARRAY_COUNT(queueNames) == MAX_QUEUE_TYPE);

        SetVkObjectName(pRenderer, (uint64_t)pQueue->mVk.pQueue, VK_OBJECT_TYPE_QUEUE, VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT,
                        pDesc->pName ? pDesc->pName : queueNames[pDesc->mType]);
    }

    *ppQueue = pQueue;

#if defined(GFX_ENABLE_SWAPPY)
    if (pQueue->mType == QUEUE_TYPE_GRAPHICS && gSwappyEnabled)
    {
        SwappyVk_setQueueFamilyIndex(pRenderer->mVk.pDevice, pQueue->mVk.pQueue, queueFamilyIndex);
    }
#endif
}

void exitQueue(Renderer* pRenderer, Queue* pQueue)
{
    ASSERT(pRenderer);
    ASSERT(pQueue);

    const uint32_t nodeIndex = pRenderer->mGpuMode == GPU_MODE_LINKED ? pQueue->mNodeIndex : 0;
    --pRenderer->mVk.pUsedQueueCount[nodeIndex][pQueue->mVk.mQueueFamilyIndex];

    SAFE_FREE(pQueue);
}

void initCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
{
    ASSERT(pRenderer);
    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);
    ASSERT(ppCmdPool);

    CmdPool* pCmdPool = (CmdPool*)tf_calloc(1, sizeof(CmdPool));
    ASSERT(pCmdPool);

    pCmdPool->pQueue = pDesc->pQueue;

    VkCommandPoolCreateInfo add_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = pDesc->pQueue->mVk.mQueueFamilyIndex,
    };
    if (pDesc->mTransient)
    {
        add_info.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    }

    CHECK_VKRESULT(
        vkCreateCommandPool(pRenderer->mVk.pDevice, &add_info, GetAllocationCallbacks(VK_OBJECT_TYPE_COMMAND_POOL), &(pCmdPool->pCmdPool)));

    *ppCmdPool = pCmdPool;
}

void exitCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
    ASSERT(pRenderer);
    ASSERT(pCmdPool);
    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);
    ASSERT(VK_NULL_HANDLE != pCmdPool->pCmdPool);

    vkDestroyCommandPool(pRenderer->mVk.pDevice, pCmdPool->pCmdPool, GetAllocationCallbacks(VK_OBJECT_TYPE_COMMAND_POOL));

    SAFE_FREE(pCmdPool);
}

void initCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
{
    ASSERT(pRenderer);
    ASSERT(VK_NULL_HANDLE != pDesc->pPool);
    ASSERT(ppCmd);

    Cmd* pCmd = (Cmd*)tf_calloc_memalign(1, ALIGN_Cmd, sizeof(Cmd));
    ASSERT(pCmd);

    pCmd->pRenderer = pRenderer;
    pCmd->pQueue = pDesc->pPool->pQueue;
    pCmd->mVk.pCmdPool = pDesc->pPool;
    pCmd->mVk.mType = pDesc->pPool->pQueue->mType;
    pCmd->mVk.mNodeIndex = pDesc->pPool->pQueue->mNodeIndex;

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pDesc->pPool->pCmdPool,
        .level = pDesc->mSecondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    CHECK_VKRESULT(vkAllocateCommandBuffers(pRenderer->mVk.pDevice, &alloc_info, &(pCmd->mVk.pCmdBuf)));

    *ppCmd = pCmd;
}

void exitCmd(Renderer* pRenderer, Cmd* pCmd)
{
    ASSERT(pRenderer);
    ASSERT(pCmd);
    ASSERT(VK_NULL_HANDLE != pCmd->pRenderer->mVk.pDevice);
    ASSERT(VK_NULL_HANDLE != pCmd->mVk.pCmdBuf);

    vkFreeCommandBuffers(pRenderer->mVk.pDevice, pCmd->mVk.pCmdPool->pCmdPool, 1, &(pCmd->mVk.pCmdBuf));

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
    SwapChain* pSwapChain = *ppSwapChain;

    Queue queue = { 0 };
    queue.mVk.mQueueFamilyIndex = pSwapChain->mVk.mPresentQueueFamilyIndex;
    Queue* queues[] = { &queue };

    SwapChainDesc desc = *pSwapChain->mVk.pDesc;
    desc.mEnableVsync = !desc.mEnableVsync;
    desc.mPresentQueueCount = 1;
    desc.ppPresentQueues = queues;
    // toggle vsync on or off
    // for Vulkan we need to remove the SwapChain and recreate it with correct vsync option
    removeSwapChain(pRenderer, pSwapChain);
    addSwapChain(pRenderer, &desc, ppSwapChain);
}

static void CreateSurface(Renderer* pRenderer, WindowHandle hwnd, VkSurfaceKHR* outSurface)
{
    // Create a WSI surface for the window:
    switch (hwnd.type)
    {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    case WINDOW_HANDLE_TYPE_WIN32:
    {
        VkWin32SurfaceCreateInfoKHR add_info = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
        add_info.pNext = NULL;
        add_info.flags = 0;
        add_info.hinstance = GetModuleHandle(NULL);
        add_info.hwnd = (HWND)hwnd.window;
        CHECK_VKRESULT_INSTANCE(vkCreateWin32SurfaceKHR(pRenderer->pContext->mVk.pInstance, &add_info,
                                                        GetAllocationCallbacks(VK_OBJECT_TYPE_SURFACE_KHR), outSurface));
        break;
    }
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    case WINDOW_HANDLE_TYPE_XLIB:
    {
        VkXlibSurfaceCreateInfoKHR add_info = { VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR };
        add_info.pNext = NULL;
        add_info.flags = 0;
        add_info.dpy = hwnd.display;   // TODO
        add_info.window = hwnd.window; // TODO
        CHECK_VKRESULT_INSTANCE(vkCreateXlibSurfaceKHR(pRenderer->pContext->mVk.pInstance, &add_info,
                                                       GetAllocationCallbacks(VK_OBJECT_TYPE_SURFACE_KHR), outSurface));
        break;
    }
#endif
#if defined(VK_USE_PLATFORM_XCB_KHR)
    case WINDOW_HANDLE_TYPE_XCB:
    {
        VkXcbSurfaceCreateInfoKHR add_info = { VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR };
        add_info.pNext = NULL;
        add_info.flags = 0;
        add_info.connection = hwnd.connection; // TODO
        add_info.window = hwnd.window;         // TODO
        CHECK_VKRESULT_INSTANCE(vkCreateXcbSurfaceKHR(pRenderer->pContext->mVk.pInstance, &add_info,
                                                      GetAllocationCallbacks(VK_OBJECT_TYPE_SURFACE_KHR), outSurface));
        break;
    }
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    case WINDOW_HANDLE_TYPE_WAYLAND:
    {
        VkWaylandSurfaceCreateInfoKHR add_info = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
        add_info.pNext = NULL;
        add_info.flags = 0;
        add_info.display = hwnd.wl_display;
        add_info.surface = hwnd.wl_surface;
        CHECK_VKRESULT_INSTANCE(vkCreateWaylandSurfaceKHR(pRenderer->pContext->mVk.pInstance, &add_info,
                                                          GetAllocationCallbacks(VK_OBJECT_TYPE_SURFACE_KHR), outSurface));
        break;
    }
#endif
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    case WINDOW_HANDLE_TYPE_ANDROID:
    {
        VkAndroidSurfaceCreateInfoKHR add_info = { VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR };
        add_info.pNext = NULL;
        add_info.flags = 0;
        add_info.window = hwnd.window;
        CHECK_VKRESULT_INSTANCE(vkCreateAndroidSurfaceKHR(pRenderer->pContext->mVk.pInstance, &add_info,
                                                          GetAllocationCallbacks(VK_OBJECT_TYPE_SURFACE_KHR), outSurface));
        break;
    }
#endif
#if defined(VK_USE_PLATFORM_VI_NN)
    case WINDOW_HANDLE_TYPE_VI_NN:
    {
        VkViSurfaceCreateInfoNN add_info = { VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN };
        add_info.pNext = NULL;
        add_info.flags = 0;
        add_info.window = hwnd.window;
        CHECK_VKRESULT_INSTANCE(vkCreateViSurfaceNN(pRenderer->pContext->mVk.pInstance, &add_info,
                                                    GetAllocationCallbacks(VK_OBJECT_TYPE_SURFACE_KHR), outSurface));
        break;
    }
#endif
    default:
        LOGF(eERROR, "Unsupported window handle type %d", (int)hwnd.type);
        ASSERT(false);
    }
}

#if defined(GFX_ENABLE_SWAPPY)
extern jint AndroidAttachToCurrentThread(WindowDesc* pWindow, JNIEnv** ppEnv);
#endif

void addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppSwapChain);

    LOGF(eINFO, "Adding Vulkan swapchain @ %ux%u", pDesc->mWidth, pDesc->mHeight);

#if defined(QUEST_VR)
    OpenXRAddVKSwapchain(pRenderer, pDesc, true, ppSwapChain);

    bool shouldAdd2DLayer = pDesc->mFlags & SWAP_CHAIN_CREATION_FLAG_ENABLE_2D_VR_LAYER;
    if (shouldAdd2DLayer)
    {
        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mPresentQueueCount = 0;
        swapChainDesc.ppPresentQueues = NULL;
        swapChainDesc.mWidth = pDesc->mWidth;
        swapChainDesc.mHeight = pDesc->mHeight;
        swapChainDesc.mImageCount = pDesc->mImageCount;
        swapChainDesc.mColorFormat = (TinyImageFormat)pDesc->mColorFormat;
        swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
        OpenXRAddVKSwapchain(pRenderer, &swapChainDesc, false, &(*ppSwapChain)->mVR.m2DLayer.pSwapchain);
        SwapChain* p2DSwapChain = (*ppSwapChain)->mVR.m2DLayer.pSwapchain;
        p2DSwapChain->mVR.m2DLayer.mCurrentSwapChainIndex = 0;
        p2DSwapChain->mVR.m2DLayer.mDesc = pDesc->mVR.m2DLayer;
    }
    return;
#endif

    ASSERT(pDesc->ppPresentQueues);
    /************************************************************************/
    // Create surface
    /************************************************************************/
    ASSERT(VK_NULL_HANDLE != pRenderer->pContext->mVk.pInstance);
    VkSurfaceKHR vkSurface;
    CreateSurface(pRenderer, pDesc->mWindowHandle, &vkSurface);
    /************************************************************************/
    // Create swap chain
    /************************************************************************/
    ASSERT(VK_NULL_HANDLE != pRenderer->pGpu->mVk.pGpu);

    // Image count
    if (0 == pDesc->mImageCount)
    {
        ((SwapChainDesc*)pDesc)->mImageCount = 2;
    }

    VkSurfaceCapabilitiesKHR caps = { 0 };
    CHECK_VKRESULT_INSTANCE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pRenderer->pGpu->mVk.pGpu, vkSurface, &caps));

    if ((caps.maxImageCount > 0) && (pDesc->mImageCount > caps.maxImageCount))
    {
        LOGF(eWARNING, "Changed requested SwapChain images {%u} to maximum allowed SwapChain images {%u}", pDesc->mImageCount,
             caps.maxImageCount);
        ((SwapChainDesc*)pDesc)->mImageCount = caps.maxImageCount;
    }
    if (pDesc->mImageCount < caps.minImageCount)
    {
        LOGF(eWARNING, "Changed requested SwapChain images {%u} to minimum required SwapChain images {%u}", pDesc->mImageCount,
             caps.minImageCount);
        ((SwapChainDesc*)pDesc)->mImageCount = caps.minImageCount;
    }

    // Surface format
    // Select a surface format, depending on whether HDR is available.

    VkSurfaceFormatKHR  surfaceFormat = { .format = VK_FORMAT_UNDEFINED };
    uint32_t            surfaceFormatCount = 0;
    VkSurfaceFormatKHR* formats = NULL;

    // Get surface formats count
    CHECK_VKRESULT_INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->pGpu->mVk.pGpu, vkSurface, &surfaceFormatCount, NULL));

    // Allocate and get surface formats
    formats = (VkSurfaceFormatKHR*)tf_calloc(surfaceFormatCount, sizeof(*formats));
    CHECK_VKRESULT_INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->pGpu->mVk.pGpu, vkSurface, &surfaceFormatCount, formats));

    if ((1 == surfaceFormatCount) && (VK_FORMAT_UNDEFINED == formats[0].format))
    {
        surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
        surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }
    else
    {
        VkFormat        requestedFormat = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mColorFormat);
        VkColorSpaceKHR requestedColorSpace = util_to_vk_colorspace(pDesc->mColorSpace);
        for (uint32_t i = 0; i < surfaceFormatCount; ++i)
        {
            if ((requestedFormat == formats[i].format) && (requestedColorSpace == formats[i].colorSpace))
            {
                surfaceFormat.format = requestedFormat;
                surfaceFormat.colorSpace = requestedColorSpace;
                break;
            }
        }

        // Default to VK_FORMAT_B8G8R8A8_UNORM if requested format isn't found
        if (VK_FORMAT_UNDEFINED == surfaceFormat.format)
        {
#if !defined(VK_USE_PLATFORM_ANDROID_KHR) && !defined(VK_USE_PLATFORM_VI_NN)
            surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
#else
            surfaceFormat.format = VK_FORMAT_R8G8B8A8_UNORM;
#endif
            surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        }
    }

    // Free formats
    SAFE_FREE(formats);

    // The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
    // This mode waits for the vertical blank ("v-sync")
    VkPresentModeKHR  presentMode = VK_PRESENT_MODE_FIFO_KHR;
    uint32_t          presentModeCount = 0;
    VkPresentModeKHR* modes = NULL;
    // Get present mode count
    CHECK_VKRESULT_INSTANCE(vkGetPhysicalDeviceSurfacePresentModesKHR(pRenderer->pGpu->mVk.pGpu, vkSurface, &presentModeCount, NULL));

    // Allocate and get present modes
    modes = (VkPresentModeKHR*)alloca(presentModeCount * sizeof(*modes));
    CHECK_VKRESULT_INSTANCE(vkGetPhysicalDeviceSurfacePresentModesKHR(pRenderer->pGpu->mVk.pGpu, vkSurface, &presentModeCount, modes));

    VkPresentModeKHR preferredModeList[] = { VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR,
                                             VK_PRESENT_MODE_FIFO_KHR };

    const uint32_t preferredModeCount = TF_ARRAY_COUNT(preferredModeList);
    const uint32_t preferredModeStartIndex = pDesc->mEnableVsync ? (preferredModeCount - 2) : 0;

    for (uint32_t j = preferredModeStartIndex; j < preferredModeCount; ++j)
    {
        VkPresentModeKHR mode = preferredModeList[j];
        uint32_t         i = 0;
        for (; i < presentModeCount; ++i)
        {
            if (modes[i] == mode)
            {
                break;
            }
        }
        if (i < presentModeCount)
        {
            presentMode = mode;
            break;
        }
    }

    // Swapchain
    VkExtent2D extent = { 0 };
    extent.width = CLAMP(pDesc->mWidth, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = CLAMP(pDesc->mHeight, caps.minImageExtent.height, caps.maxImageExtent.height);

    // Get queue family properties
    uint32_t                 queueFamilyPropertyCount = 0;
    VkQueueFamilyProperties* queueFamilyProperties = NULL;
    vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->pGpu->mVk.pGpu, &queueFamilyPropertyCount, NULL);
    queueFamilyProperties = (VkQueueFamilyProperties*)alloca(queueFamilyPropertyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->pGpu->mVk.pGpu, &queueFamilyPropertyCount, queueFamilyProperties);

    uint32_t presentQueueFamilyIndex = UINT32_MAX;
    uint32_t presentQueueIndex = UINT32_MAX;

    for (uint32_t index = 0; index < pDesc->mPresentQueueCount; ++index)
    {
        const uint32_t queueFamilyIndex = pDesc->ppPresentQueues[index]->mVk.mQueueFamilyIndex;

        VkBool32 supportsPresent = VK_FALSE;
        VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(pRenderer->pGpu->mVk.pGpu, queueFamilyIndex, vkSurface, &supportsPresent);
        if ((VK_SUCCESS == res) && supportsPresent)
        {
            presentQueueFamilyIndex = queueFamilyIndex;
            presentQueueIndex = pDesc->ppPresentQueues[index]->mVk.mQueueIndex;
            break;
        }
    }

    // If none of the user provided queues support present, find first available queue which supports present
    if (presentQueueFamilyIndex == UINT32_MAX)
    {
        for (uint32_t index = 0; index < queueFamilyPropertyCount; ++index)
        {
            VkBool32 supportsPresent = VK_FALSE;
            VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(pRenderer->pGpu->mVk.pGpu, index, vkSurface, &supportsPresent);
            if ((VK_SUCCESS == res) && supportsPresent)
            {
                presentQueueFamilyIndex = index;
                presentQueueIndex = 0;
                break;
            }
        }
    }

    if (presentQueueFamilyIndex == UINT32_MAX)
    {
        ASSERT(false && "No present queue found");
    }

    VkQueue presentQueue = VK_NULL_HANDLE;
    vkGetDeviceQueue(pRenderer->mVk.pDevice, presentQueueFamilyIndex, presentQueueIndex, &presentQueue);
    ASSERT(presentQueue != VK_NULL_HANDLE);

    VkSurfaceTransformFlagBitsKHR surfacePreTransform;
    // #TODO: Add more if necessary but identity should be enough for now
    if (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        surfacePreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        surfacePreTransform = caps.currentTransform;
    }

    VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[] = {
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
    };

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR;
    for (int i = 0; i < TF_ARRAY_COUNT(compositeAlphaFlags); ++i)
    {
        VkCompositeAlphaFlagBitsKHR flag = compositeAlphaFlags[i];
        if (caps.supportedCompositeAlpha & flag)
        {
            compositeAlpha = flag;
            break;
        }
    }

    ASSERT(compositeAlpha != VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR);

    VkSwapchainKHR           vkSwapchain;
    VkSwapchainCreateInfoKHR swapChainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .surface = vkSurface,
        .minImageCount = pDesc->mImageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .preTransform = surfacePreTransform,
        .compositeAlpha = compositeAlpha,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = 0,
    };
#ifdef AUTOMATED_TESTING
    swapChainCreateInfo.imageUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
#endif
    CHECK_VKRESULT(vkCreateSwapchainKHR(pRenderer->mVk.pDevice, &swapChainCreateInfo, GetAllocationCallbacks(VK_OBJECT_TYPE_SWAPCHAIN_KHR),
                                        &vkSwapchain));
    ((SwapChainDesc*)pDesc)->mColorFormat = TinyImageFormat_FromVkFormat((TinyImageFormat_VkFormat)surfaceFormat.format);

#if defined(GFX_ENABLE_SWAPPY)
    if (gSwappyEnabled)
    {
        // Setup Swappy
        // AndroidWindow.cpp, used to retrieve the Java activity
        extern WindowDesc gWindow;
        uint64_t          refreshDuration;

        JNIEnv* pJavaEnv = NULL;
        AndroidAttachToCurrentThread(&gWindow, &pJavaEnv);

        SwappyVk_initAndGetRefreshCycleDuration(pJavaEnv, gWindow.handle.activity->clazz, pRenderer->pGpu->mVk.pGpu, pRenderer->mVk.pDevice,
                                                vkSwapchain, &refreshDuration);
        SwappyVk_setAutoSwapInterval(false);
        // Don't swap faster than the monitor refresh rate
        SwappyVk_setSwapIntervalNS(pRenderer->mVk.pDevice, vkSwapchain, MAX(refreshDuration, gSwapIntervalNS));
    }
#endif
    // Create rendertargets from swapchain
    uint32_t imageCount = 0;
    CHECK_VKRESULT(vkGetSwapchainImagesKHR(pRenderer->mVk.pDevice, vkSwapchain, &imageCount, NULL));

    ASSERT(imageCount >= pDesc->mImageCount);

    VkImage* images = (VkImage*)alloca(imageCount * sizeof(VkImage));

    CHECK_VKRESULT(vkGetSwapchainImagesKHR(pRenderer->mVk.pDevice, vkSwapchain, &imageCount, images));

    SwapChain* pSwapChain = (SwapChain*)tf_calloc(1, sizeof(SwapChain) + imageCount * sizeof(RenderTarget*) + sizeof(SwapChainDesc));
    pSwapChain->ppRenderTargets = (RenderTarget**)(pSwapChain + 1);
    pSwapChain->mVk.pDesc = (SwapChainDesc*)(pSwapChain->ppRenderTargets + imageCount);
    ASSERT(pSwapChain);

    RenderTargetDesc descColor = { 0 };
    descColor.mWidth = extent.width;
    descColor.mHeight = extent.height;
    descColor.mDepth = 1;
    descColor.mArraySize = 1;
    descColor.mFormat = pDesc->mColorFormat;
    descColor.mClearValue = pDesc->mColorClearValue;
    descColor.mSampleCount = SAMPLE_COUNT_1;
    descColor.mSampleQuality = 0;
    descColor.mStartState = RESOURCE_STATE_PRESENT;
    descColor.mNodeIndex = pRenderer->mUnlinkedRendererIndex;

    char buffer[32] = { 0 };
    // Populate the vk_image field and add the Vulkan texture objects
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        snprintf(buffer, 32, "Swapchain RT[%u]", i);
        descColor.pName = buffer;
        descColor.pNativeHandle = (void*)images[i];
        addRenderTarget(pRenderer, &descColor, &pSwapChain->ppRenderTargets[i]);
    }
    /************************************************************************/
    /************************************************************************/
    *pSwapChain->mVk.pDesc = *pDesc;
    pSwapChain->mEnableVsync = pDesc->mEnableVsync;
    pSwapChain->mImageCount = imageCount;
    pSwapChain->mColorSpace = util_from_vk_colorspace(surfaceFormat.colorSpace);
    pSwapChain->mFormat = TinyImageFormat_FromVkFormat((TinyImageFormat_VkFormat)surfaceFormat.format);
    pSwapChain->mVk.pSurface = vkSurface;
    pSwapChain->mVk.mPresentQueueFamilyIndex = presentQueueFamilyIndex;
    pSwapChain->mVk.pPresentQueue = presentQueue;
    pSwapChain->mVk.pSwapChain = vkSwapchain;

    *ppSwapChain = pSwapChain;
}

void removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
    ASSERT(pRenderer);
    ASSERT(pSwapChain);

#if defined(GFX_ENABLE_SWAPPY)
    if (gSwappyEnabled)
    {
        SwappyVk_destroySwapchain(pRenderer->mVk.pDevice, pSwapChain->mVk.pSwapChain);
    }
#endif

#if defined(QUEST_VR)
    if (pSwapChain->mVR.m2DLayer.pSwapchain)
    {
        OpenXRRemoveVKSwapchain(pRenderer, pSwapChain->mVR.m2DLayer.pSwapchain);
    }
    OpenXRRemoveVKSwapchain(pRenderer, pSwapChain);
    return;
#endif

    for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
    {
        removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[i]);
    }

    vkDestroySwapchainKHR(pRenderer->mVk.pDevice, pSwapChain->mVk.pSwapChain, GetAllocationCallbacks(VK_OBJECT_TYPE_SWAPCHAIN_KHR));
    vkDestroySurfaceKHR(pRenderer->pContext->mVk.pInstance, pSwapChain->mVk.pSurface, GetAllocationCallbacks(VK_OBJECT_TYPE_SURFACE_KHR));

    SAFE_FREE(pSwapChain);
}

void addResourceHeap(Renderer* pRenderer, const ResourceHeapDesc* pDesc, ResourceHeap** ppHeap)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppHeap);
    ASSERT(pDesc->mAlignment != 0);

    VmaAllocationCreateInfo vma_mem_reqs = { 0 };
    vma_mem_reqs.usage = (VmaMemoryUsage)pDesc->mMemoryUsage;
    vma_mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VkMemoryRequirements vkMemReq = { 0 };
    vkMemReq.size = pDesc->mSize;
    vkMemReq.alignment = pDesc->mAlignment;
    vkMemReq.memoryTypeBits = UINT32_MAX;

    VmaAllocationInfo alloc_info = { 0 };
    VmaAllocation     alloc = { 0 };
    CHECK_VKRESULT(vmaAllocateMemory(pRenderer->mVk.pVmaAllocator, &vkMemReq, &vma_mem_reqs, &alloc, &alloc_info));

    ASSERT(alloc_info.size >= pDesc->mSize);
    ASSERT(alloc_info.offset == 0);

    ResourceHeap* pHeap = (ResourceHeap*)tf_calloc(1, sizeof(ResourceHeap));
    pHeap->mVk.pAllocation = alloc;
    pHeap->mVk.pMemory = alloc_info.deviceMemory;
    pHeap->mVk.pCpuMappedAddress = alloc_info.pMappedData;
    pHeap->mSize = pDesc->mSize;
    *ppHeap = pHeap;
}

void removeResourceHeap(Renderer* pRenderer, ResourceHeap* pHeap)
{
    vmaFreeMemory(pRenderer->mVk.pVmaAllocator, pHeap->mVk.pAllocation);

    tf_free(pHeap);
}

void getBufferSizeAlign(Renderer* pRenderer, const BufferDesc* pDesc, ResourceSizeAlign* pOut)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(pOut);

    VkBufferCreateInfo bufferInfo = { 0 };
    InitializeBufferCreateInfo(pRenderer, pDesc, &bufferInfo);

    VkBuffer buffer = VK_NULL_HANDLE;
    CHECK_VKRESULT(vkCreateBuffer(pRenderer->mVk.pDevice, &bufferInfo, GetAllocationCallbacks(VK_OBJECT_TYPE_BUFFER), &buffer));

    VkMemoryRequirements vkMemReq = { 0 };
    vkGetBufferMemoryRequirements(pRenderer->mVk.pDevice, buffer, &vkMemReq);

    vkDestroyBuffer(pRenderer->mVk.pDevice, buffer, GetAllocationCallbacks(VK_OBJECT_TYPE_BUFFER));

    pOut->mSize = vkMemReq.size;
    pOut->mAlignment = vkMemReq.alignment;
}

void getTextureSizeAlign(Renderer* pRenderer, const TextureDesc* pDesc, ResourceSizeAlign* pOut)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(pOut);

    ImageCreateInfoExtras imageCreateInfoExtras = { 0 };
    VkImageCreateInfo     add_info = { 0 };
    InitializeImageCreateInfo(pRenderer, pDesc, &add_info, &imageCreateInfoExtras);

    VkImage image = VK_NULL_HANDLE;
    vkCreateImage(pRenderer->mVk.pDevice, &add_info, GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE), &image);

    VkMemoryRequirements vkMemReq = { 0 };
    vkGetImageMemoryRequirements(pRenderer->mVk.pDevice, image, &vkMemReq);

    vkDestroyImage(pRenderer->mVk.pDevice, image, GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE));

    pOut->mSize = vkMemReq.size;
    pOut->mAlignment = vkMemReq.alignment;
}

void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(pDesc->mSize > 0);
    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);
    ASSERT(pRenderer->mGpuMode != GPU_MODE_UNLINKED || pDesc->mNodeIndex == pRenderer->mUnlinkedRendererIndex);

    Buffer* pBuffer = (Buffer*)tf_calloc_memalign(1, ALIGN_Buffer, sizeof(Buffer));
    ASSERT(ppBuffer);

    VkBufferCreateInfo add_info = { 0 };
    InitializeBufferCreateInfo(pRenderer, pDesc, &add_info);

    const bool linkedMultiGpu = (pRenderer->mGpuMode == GPU_MODE_LINKED && (pDesc->pSharedNodeIndices || pDesc->mNodeIndex));

    VmaAllocationCreateInfo vma_mem_reqs = { 0 };
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
    if (pRenderer->pGpu->mAMDDeviceCoherentMemorySupported && (pDesc->mFlags & BUFFER_CREATION_FLAG_MARKER))
        vma_mem_reqs.preferredFlags |= VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD | VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD;
    if (!pRenderer->pGpu->mAMDDeviceCoherentMemorySupported && (pDesc->mFlags & BUFFER_CREATION_FLAG_MARKER))
        vma_mem_reqs.preferredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

#if defined(ANDROID) || defined(NX64)
    // UMA for Android and NX64 devices
    if (vma_mem_reqs.usage != VMA_MEMORY_USAGE_GPU_TO_CPU)
    {
        vma_mem_reqs.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vma_mem_reqs.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
#endif

    VmaAllocationInfo alloc_info = { 0 };
    if (pDesc->pPlacement)
    {
        ResourceHeap*  pHeap = pDesc->pPlacement->pHeap;
        const uint64_t placementOffset = pDesc->pPlacement->mOffset;
        ASSERT(pHeap);

        CHECK_VKRESULT(
            vkCreateBuffer(pRenderer->mVk.pDevice, &add_info, GetAllocationCallbacks(VK_OBJECT_TYPE_BUFFER), &pBuffer->mVk.pBuffer));
        CHECK_VKRESULT(vkBindBufferMemory(pRenderer->mVk.pDevice, pBuffer->mVk.pBuffer, pHeap->mVk.pMemory, placementOffset));
        alloc_info.deviceMemory = pHeap->mVk.pMemory;
        alloc_info.pMappedData = pHeap->mVk.pCpuMappedAddress ? ((uint8_t*)pHeap->mVk.pCpuMappedAddress + placementOffset) : NULL;
    }
    else
    {
        CHECK_VKRESULT(
            vkCreateBuffer(pRenderer->mVk.pDevice, &add_info, GetAllocationCallbacks(VK_OBJECT_TYPE_BUFFER), &pBuffer->mVk.pBuffer));
        CHECK_VKRESULT(vmaAllocateMemoryForBuffer(pRenderer->mVk.pVmaAllocator, pBuffer->mVk.pBuffer, &vma_mem_reqs,
                                                  &pBuffer->mVk.pAllocation, &alloc_info));
        CHECK_VKRESULT(vmaBindBufferMemory2(pRenderer->mVk.pVmaAllocator, pBuffer->mVk.pAllocation, 0, pBuffer->mVk.pBuffer, NULL));
#if VMA_STATS_STRING_ENABLED
        vmaInitBufferUsage(pBuffer->mVk.pAllocation, &add_info, false);
#endif
    }

    pBuffer->pCpuMappedAddress = alloc_info.pMappedData;
    /************************************************************************/
    // Buffer to be used on multiple GPUs
    /************************************************************************/
    if (linkedMultiGpu)
    {
        VmaAllocationInfo allocInfo = { 0 };
        vmaGetAllocationInfo(pRenderer->mVk.pVmaAllocator, pBuffer->mVk.pAllocation, &allocInfo);
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
        bindInfo.buffer = pBuffer->mVk.pBuffer;
        bindInfo.memory = allocInfo.deviceMemory;
        bindInfo.memoryOffset = allocInfo.offset;
        bindInfo.pNext = &bindDeviceGroup;
        CHECK_VKRESULT(vkBindBufferMemory2KHR(pRenderer->mVk.pDevice, 1, &bindInfo));
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
            pBuffer->mVk.mOffset = pDesc->mStructStride * pDesc->mFirstElement;
        }
    }

    if (add_info.usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT)
    {
        VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, NULL };
        viewInfo.buffer = pBuffer->mVk.pBuffer;
        viewInfo.flags = 0;
        viewInfo.format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mFormat);
        viewInfo.offset = pDesc->mFirstElement * pDesc->mStructStride;
        viewInfo.range = pDesc->mElementCount * pDesc->mStructStride;
        VkFormatProperties formatProps = { 0 };
        vkGetPhysicalDeviceFormatProperties(pRenderer->pGpu->mVk.pGpu, viewInfo.format, &formatProps);
        if (!(formatProps.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT))
        {
            LOGF(eWARNING, "Failed to create uniform texel buffer view for format %u", (uint32_t)pDesc->mFormat);
        }
        else
        {
            CHECK_VKRESULT(vkCreateBufferView(pRenderer->mVk.pDevice, &viewInfo, GetAllocationCallbacks(VK_OBJECT_TYPE_BUFFER_VIEW),
                                              &pBuffer->mVk.pUniformTexelView));
        }
    }
    if (add_info.usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)
    {
        VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, NULL };
        viewInfo.buffer = pBuffer->mVk.pBuffer;
        viewInfo.flags = 0;
        viewInfo.format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mFormat);
        viewInfo.offset = pDesc->mFirstElement * pDesc->mStructStride;
        viewInfo.range = pDesc->mElementCount * pDesc->mStructStride;
        VkFormatProperties formatProps = { 0 };
        vkGetPhysicalDeviceFormatProperties(pRenderer->pGpu->mVk.pGpu, viewInfo.format, &formatProps);
        if (!(formatProps.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
        {
            LOGF(eWARNING, "Failed to create storage texel buffer view for format %u", (uint32_t)pDesc->mFormat);
        }
        else
        {
            CHECK_VKRESULT(vkCreateBufferView(pRenderer->mVk.pDevice, &viewInfo, GetAllocationCallbacks(VK_OBJECT_TYPE_BUFFER_VIEW),
                                              &pBuffer->mVk.pStorageTexelView));
        }
    }

#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
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

void removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
    ASSERT(pRenderer);
    ASSERT(pBuffer);
    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);
    ASSERT(VK_NULL_HANDLE != pBuffer->mVk.pBuffer);

    if (pBuffer->mVk.pUniformTexelView)
    {
        vkDestroyBufferView(pRenderer->mVk.pDevice, pBuffer->mVk.pUniformTexelView, GetAllocationCallbacks(VK_OBJECT_TYPE_BUFFER_VIEW));
        pBuffer->mVk.pUniformTexelView = VK_NULL_HANDLE;
    }
    if (pBuffer->mVk.pStorageTexelView)
    {
        vkDestroyBufferView(pRenderer->mVk.pDevice, pBuffer->mVk.pStorageTexelView, GetAllocationCallbacks(VK_OBJECT_TYPE_BUFFER_VIEW));
        pBuffer->mVk.pStorageTexelView = VK_NULL_HANDLE;
    }

    vkDestroyBuffer(pRenderer->mVk.pDevice, pBuffer->mVk.pBuffer, GetAllocationCallbacks(VK_OBJECT_TYPE_BUFFER));
    pBuffer->mVk.pBuffer = VK_NULL_HANDLE;
    if (pBuffer->mVk.pAllocation)
    {
        vmaFreeMemory(pRenderer->mVk.pVmaAllocator, pBuffer->mVk.pAllocation);
        pBuffer->mVk.pAllocation = NULL;
    }

    SAFE_FREE(pBuffer);
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

    size_t totalSize = sizeof(Texture);
    totalSize += (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE ? (pDesc->mMipLevels * sizeof(VkImageView)) : 0);
    Texture* pTexture = (Texture*)tf_calloc_memalign(1, ALIGN_Texture, totalSize);
    ASSERT(pTexture);

    if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
        pTexture->mVk.pUAVDescriptors = (VkImageView*)(pTexture + 1);

    if (pDesc->pNativeHandle && !(pDesc->mFlags & TEXTURE_CREATION_FLAG_IMPORT_BIT))
    {
        pTexture->mOwnsImage = false;
        pTexture->mVk.pImage = (VkImage)pDesc->pNativeHandle;
    }
    else
    {
        pTexture->mOwnsImage = true;
    }

    const VkImageUsageFlags additionalFlags = util_to_vk_image_usage_flags(pDesc->mStartState);
    const uint32_t          arraySize = util_vk_image_array_size(pDesc, additionalFlags);
    const VkImageType       image_type = util_to_vk_image_type(pDesc);

    const DescriptorType descriptors = pDesc->mDescriptors;
    const bool           cubemapRequired = (DESCRIPTOR_TYPE_TEXTURE_CUBE == (descriptors & DESCRIPTOR_TYPE_TEXTURE_CUBE));

    // planar texture do not need special allocator via VK_FORMAT_FEATURE_DISJOINT_BIT
    // this flag is only usefull for custom allocator or if the texture are already on the gpu
    const uint32_t numOfPlanes = TinyImageFormat_NumOfPlanes(pDesc->mFormat);
    const bool     isSinglePlane = TinyImageFormat_IsSinglePlane(pDesc->mFormat);
    ASSERT(((isSinglePlane && numOfPlanes == 1) || (!isSinglePlane && numOfPlanes > 1 && numOfPlanes <= MAX_PLANE_COUNT)) &&
           "Number of planes for multi-planar formats must be 2 or 3 and for single-planar formats it must be 1.");

    const bool linkedMultiGpu = (pRenderer->mGpuMode == GPU_MODE_LINKED) && (pDesc->pSharedNodeIndices || pDesc->mNodeIndex);

    if (VK_NULL_HANDLE == pTexture->mVk.pImage)
    {
        ImageCreateInfoExtras imageCreateInfoExtras = { 0 };
        VkImageCreateInfo     add_info = { 0 };
        InitializeImageCreateInfo(pRenderer, pDesc, &add_info, &imageCreateInfoExtras);
        pTexture->mLazilyAllocated = imageCreateInfoExtras.mLazilyAllocated;

        const VmaAllocationCreateInfo mem_reqs = imageCreateInfoExtras.mMemReq;

        if (pDesc->pPlacement)
        {
            ResourceHeap* pHeap = pDesc->pPlacement->pHeap;
            ASSERT(pHeap);
            CHECK_VKRESULT(
                vkCreateImage(pRenderer->mVk.pDevice, &add_info, GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE), &pTexture->mVk.pImage));
            CHECK_VKRESULT(vkBindImageMemory(pRenderer->mVk.pDevice, pTexture->mVk.pImage, pHeap->mVk.pMemory, pDesc->pPlacement->mOffset));
        }
        else
        {
            CHECK_VKRESULT(
                vkCreateImage(pRenderer->mVk.pDevice, &add_info, GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE), &pTexture->mVk.pImage));
            CHECK_VKRESULT(
                vmaAllocateMemoryForImage(pRenderer->mVk.pVmaAllocator, pTexture->mVk.pImage, &mem_reqs, &pTexture->mVk.pAllocation, NULL));
            CHECK_VKRESULT(vmaBindImageMemory2(pRenderer->mVk.pVmaAllocator, pTexture->mVk.pAllocation, 0, pTexture->mVk.pImage, NULL));
#if VMA_STATS_STRING_ENABLED
            vmaInitImageUsage(pTexture->mVk.pAllocation, &add_info);
#endif
        }

        /************************************************************************/
        // Texture to be used on multiple GPUs
        /************************************************************************/
        if (linkedMultiGpu)
        {
            VmaAllocationInfo allocInfo = { 0 };
            vmaGetAllocationInfo(pRenderer->mVk.pVmaAllocator, pTexture->mVk.pAllocation, &allocInfo);
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
            bindInfo.image = pTexture->mVk.pImage;
            bindInfo.memory = allocInfo.deviceMemory;
            bindInfo.memoryOffset = allocInfo.offset;
            bindInfo.pNext = &bindDeviceGroup;
            CHECK_VKRESULT(vkBindImageMemory2KHR(pRenderer->mVk.pDevice, 1, &bindInfo));
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
    case VK_IMAGE_TYPE_1D:
        view_type = arraySize > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        break;
    case VK_IMAGE_TYPE_2D:
        if (cubemapRequired)
            view_type = (arraySize > 6) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
        else
            view_type = arraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        break;
    case VK_IMAGE_TYPE_3D:
        if (arraySize > 1)
        {
            LOGF(eERROR, "Cannot support 3D Texture Array in Vulkan");
            ASSERT(false);
        }
        view_type = VK_IMAGE_VIEW_TYPE_3D;
        break;
    default:
        ASSERT(false && "Image Format not supported!");
        break;
    }

    ASSERT(view_type != VK_IMAGE_VIEW_TYPE_MAX_ENUM && "Invalid Image View");

    VkImageViewCreateInfo srvDesc = { 0 };
    // SRV
    srvDesc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    srvDesc.pNext = NULL;
    srvDesc.flags = 0;
    srvDesc.image = pTexture->mVk.pImage;
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

    if (pDesc->pSamplerYcbcrConversionInfo)
    {
        srvDesc.pNext = pDesc->pSamplerYcbcrConversionInfo;
    }

    if (descriptors & DESCRIPTOR_TYPE_TEXTURE)
    {
        CHECK_VKRESULT(vkCreateImageView(pRenderer->mVk.pDevice, &srvDesc, GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE_VIEW),
                                         &pTexture->mVk.pSRVDescriptor));
    }

    // SRV stencil
    if ((TinyImageFormat_HasStencil(pDesc->mFormat)) && (descriptors & DESCRIPTOR_TYPE_TEXTURE))
    {
        srvDesc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        CHECK_VKRESULT(vkCreateImageView(pRenderer->mVk.pDevice, &srvDesc, GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE_VIEW),
                                         &pTexture->mVk.pSRVStencilDescriptor));
    }

    // ASTC decode mode extension
    VkImageViewASTCDecodeModeEXT astcDecodeMode = { VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT };
    astcDecodeMode.decodeMode = VK_FORMAT_R8G8B8A8_UNORM;
    if (pRenderer->pGpu->mASTCDecodeModeExtension && TinyImageFormat_IsCompressedASTC(pDesc->mFormat))
    {
        srvDesc.pNext = &astcDecodeMode;
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
            CHECK_VKRESULT(vkCreateImageView(pRenderer->mVk.pDevice, &uavDesc, GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE_VIEW),
                                             &pTexture->mVk.pUAVDescriptors[i]));
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

#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    if (pDesc->pName)
    {
        setTextureName(pRenderer, pTexture, pDesc->pName);
    }
#endif

    *ppTexture = pTexture;
}

void removeTexture(Renderer* pRenderer, Texture* pTexture)
{
    ASSERT(pRenderer);
    ASSERT(pTexture);
    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);
    ASSERT(VK_NULL_HANDLE != pTexture->mVk.pImage);

    if (VK_NULL_HANDLE != pTexture->mVk.pSRVDescriptor)
    {
        vkDestroyImageView(pRenderer->mVk.pDevice, pTexture->mVk.pSRVDescriptor, GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE_VIEW));
    }

    if (VK_NULL_HANDLE != pTexture->mVk.pSRVStencilDescriptor)
    {
        vkDestroyImageView(pRenderer->mVk.pDevice, pTexture->mVk.pSRVStencilDescriptor, GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE_VIEW));
    }

    if (pTexture->mVk.pUAVDescriptors)
    {
        for (uint32_t i = 0; i < pTexture->mMipLevels; ++i)
        {
            vkDestroyImageView(pRenderer->mVk.pDevice, pTexture->mVk.pUAVDescriptors[i], GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE_VIEW));
        }
    }

    if (pTexture->mOwnsImage)
    {
        vkDestroyImage(pRenderer->mVk.pDevice, pTexture->mVk.pImage, GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE));
        if (pTexture->mVk.pAllocation)
        {
            vmaFreeMemory(pRenderer->mVk.pVmaAllocator, pTexture->mVk.pAllocation);
            pTexture->mVk.pAllocation = NULL;
        }
    }

    pTexture->mVk.pImage = VK_NULL_HANDLE;
    SAFE_FREE(pTexture);
}

void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppRenderTarget);
    ASSERT(pRenderer->mGpuMode != GPU_MODE_UNLINKED || pDesc->mNodeIndex == pRenderer->mUnlinkedRendererIndex);

    bool const isDepth = TinyImageFormat_HasDepthOrStencil(pDesc->mFormat);

    ASSERT(!((isDepth) && (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)) && "Cannot use depth stencil as UAV");

    ((RenderTargetDesc*)pDesc)->mMipLevels = MAX(1U, pDesc->mMipLevels);

    uint32_t arraySize = pDesc->mArraySize;
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
    RenderTarget* pRenderTarget = (RenderTarget*)tf_calloc_memalign(1, ALIGN_RenderTarget, totalSize);
    ASSERT(pRenderTarget);

    pRenderTarget->mVk.pSliceDescriptors = (VkImageView*)(pRenderTarget + 1);

    // Monotonically increasing thread safe id generation
    pRenderTarget->mVk.mId = tfrg_atomic32_add_relaxed(&gRenderTargetIds, 1);

    TextureDesc textureDesc = { 0 };
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
            VkImageFormatProperties properties = { 0 };
            VkResult vk_res = vkGetPhysicalDeviceImageFormatProperties(pRenderer->pGpu->mVk.pGpu, vk_depth_stencil_format, VK_IMAGE_TYPE_2D,
                                                                       VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                                       0, &properties);
            // Fall back to something that's guaranteed to work
            if (VK_SUCCESS != vk_res)
            {
                textureDesc.mFormat = TinyImageFormat_D16_UNORM;
                LOGF(eWARNING, "Depth stencil format (%i) not supported. Falling back to D16 format", pDesc->mFormat);
            }
        }
    }

    textureDesc.pName = pDesc->pName;
    textureDesc.pPlacement = pDesc->pPlacement;

    addTexture(pRenderer, &textureDesc, &pRenderTarget->pTexture);

    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    if (pDesc->mHeight > 1)
    {
        viewType = depthOrArraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    }
    else
    {
        viewType = depthOrArraySize > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
    }

    VkImageViewCreateInfo rtvDesc = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL };
    rtvDesc.flags = 0;
    rtvDesc.image = pRenderTarget->pTexture->mVk.pImage;
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

    CHECK_VKRESULT(vkCreateImageView(pRenderer->mVk.pDevice, &rtvDesc, GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE_VIEW),
                                     &pRenderTarget->mVk.pDescriptor));

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
                CHECK_VKRESULT(vkCreateImageView(pRenderer->mVk.pDevice, &rtvDesc, GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE_VIEW),
                                                 &pRenderTarget->mVk.pSliceDescriptors[i * depthOrArraySize + j]));
            }
        }
        else
        {
            CHECK_VKRESULT(vkCreateImageView(pRenderer->mVk.pDevice, &rtvDesc, GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE_VIEW),
                                             &pRenderTarget->mVk.pSliceDescriptors[i]));
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
    pRenderTarget->mDescriptors = pDesc->mDescriptors;

    // Unlike DX12, Vulkan textures start in undefined layout.
    // To keep in line with DX12, we transition them to the specified layout manually so app code doesn't have to worry about this
    // Render targets wont be created during runtime so this overhead will be minimal
    TransitionInitialLayout(pRenderer, pRenderTarget->pTexture, pDesc->mStartState);

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
    if (pDesc->mFlags & TEXTURE_CREATION_FLAG_CREATE_RESOLVE_ATTACHMENT)
    {
        RenderTargetDesc resolveRTDesc = *pDesc;
        resolveRTDesc.mFlags &= ~(TEXTURE_CREATION_FLAG_CREATE_RESOLVE_ATTACHMENT | TEXTURE_CREATION_FLAG_ON_TILE);
        resolveRTDesc.mSampleCount = SAMPLE_COUNT_1;
        addRenderTarget(pRenderer, &resolveRTDesc, &pRenderTarget->pResolveAttachment);
    }
#endif

#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    if (pDesc->pName)
    {
        setRenderTargetName(pRenderer, pRenderTarget, pDesc->pName);
    }
#endif

    *ppRenderTarget = pRenderTarget;
}

void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
    removeTexture(pRenderer, pRenderTarget->pTexture);

    vkDestroyImageView(pRenderer->mVk.pDevice, pRenderTarget->mVk.pDescriptor, GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE_VIEW));

    const uint32_t depthOrArraySize = pRenderTarget->mArraySize * pRenderTarget->mDepth;
    if ((pRenderTarget->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
        (pRenderTarget->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
    {
        for (uint32_t i = 0; i < pRenderTarget->mMipLevels; ++i)
            for (uint32_t j = 0; j < depthOrArraySize; ++j)
                vkDestroyImageView(pRenderer->mVk.pDevice, pRenderTarget->mVk.pSliceDescriptors[i * depthOrArraySize + j],
                                   GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE_VIEW));
    }
    else
    {
        for (uint32_t i = 0; i < pRenderTarget->mMipLevels; ++i)
            vkDestroyImageView(pRenderer->mVk.pDevice, pRenderTarget->mVk.pSliceDescriptors[i],
                               GetAllocationCallbacks(VK_OBJECT_TYPE_IMAGE_VIEW));
    }

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
    if (pRenderTarget->pResolveAttachment)
    {
        removeRenderTarget(pRenderer, pRenderTarget->pResolveAttachment);
    }
#endif

    SAFE_FREE(pRenderTarget);
}

void addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
    ASSERT(pRenderer);
    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);
    ASSERT(pDesc->mCompareFunc < MAX_COMPARE_MODES);
    ASSERT(ppSampler);

    Sampler* pSampler = (Sampler*)tf_calloc_memalign(1, ALIGN_Sampler, sizeof(Sampler));
    ASSERT(pSampler);

    // default sampler lod values
    // used if not overriden by mSetLodRange or not Linear mipmaps
    float minSamplerLod = 0;
    float maxSamplerLod = pDesc->mMipMapMode == MIPMAP_MODE_LINEAR ? VK_LOD_CLAMP_NONE : 0;
    // user provided lods
    if (pDesc->mSetLodRange)
    {
        minSamplerLod = pDesc->mMinLod;
        maxSamplerLod = pDesc->mMaxLod;
    }

    VkSamplerCreateInfo add_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .magFilter = util_to_vk_filter(pDesc->mMagFilter),
        .minFilter = util_to_vk_filter(pDesc->mMinFilter),
        .mipmapMode = util_to_vk_mip_map_mode(pDesc->mMipMapMode),
        .addressModeU = util_to_vk_address_mode(pDesc->mAddressU),
        .addressModeV = util_to_vk_address_mode(pDesc->mAddressV),
        .addressModeW = util_to_vk_address_mode(pDesc->mAddressW),
        .mipLodBias = pDesc->mMipLodBias,
        .anisotropyEnable = pRenderer->pGpu->mSamplerAnisotropySupported && (pDesc->mMaxAnisotropy > 0.0f) ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = pDesc->mMaxAnisotropy,
        .compareEnable = (gVkComparisonFuncTranslator[pDesc->mCompareFunc] != VK_COMPARE_OP_NEVER) ? VK_TRUE : VK_FALSE,
        .compareOp = gVkComparisonFuncTranslator[pDesc->mCompareFunc],
        .minLod = minSamplerLod,
        .maxLod = maxSamplerLod,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    if (TinyImageFormat_IsPlanar(pDesc->mSamplerConversionDesc.mFormat))
    {
        VkFormat format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mSamplerConversionDesc.mFormat);

        // Check format props
        {
            ASSERT((uint32_t)pRenderer->pGpu->mYCbCrExtension);

            VkFormatProperties format_props = { 0 };
            vkGetPhysicalDeviceFormatProperties(pRenderer->pGpu->mVk.pGpu, format, &format_props);
            if (pDesc->mSamplerConversionDesc.mChromaOffsetX == SAMPLE_LOCATION_MIDPOINT)
            {
                ASSERT(format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT);
            }
            else if (pDesc->mSamplerConversionDesc.mChromaOffsetX == SAMPLE_LOCATION_COSITED)
            {
                ASSERT(format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT);
            }
        }

        VkSamplerYcbcrConversionCreateInfo conversion_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
            .pNext = NULL,
            .format = format,
            .ycbcrModel = (VkSamplerYcbcrModelConversion)pDesc->mSamplerConversionDesc.mModel,
            .ycbcrRange = (VkSamplerYcbcrRange)pDesc->mSamplerConversionDesc.mRange,
            .components = (VkComponentMapping){ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                VK_COMPONENT_SWIZZLE_IDENTITY },
            .xChromaOffset = (VkChromaLocation)pDesc->mSamplerConversionDesc.mChromaOffsetX,
            .yChromaOffset = (VkChromaLocation)pDesc->mSamplerConversionDesc.mChromaOffsetY,
            .chromaFilter = util_to_vk_filter(pDesc->mSamplerConversionDesc.mChromaFilter),
            .forceExplicitReconstruction = pDesc->mSamplerConversionDesc.mForceExplicitReconstruction ? VK_TRUE : VK_FALSE,
        };
        CHECK_VKRESULT(vkCreateSamplerYcbcrConversion(pRenderer->mVk.pDevice, &conversion_info,
                                                      GetAllocationCallbacks(VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION),
                                                      &pSampler->mVk.pSamplerYcbcrConversion));

        pSampler->mVk.mSamplerYcbcrConversionInfo = (VkSamplerYcbcrConversionInfo){
            .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
            .pNext = NULL,
            .conversion = pSampler->mVk.pSamplerYcbcrConversion,
        };
        add_info.pNext = &pSampler->mVk.mSamplerYcbcrConversionInfo;
    }

    CHECK_VKRESULT(
        vkCreateSampler(pRenderer->mVk.pDevice, &add_info, GetAllocationCallbacks(VK_OBJECT_TYPE_SAMPLER), &(pSampler->mVk.pSampler)));

    *ppSampler = pSampler;
}

void removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
    ASSERT(pRenderer);
    ASSERT(pSampler);
    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);
    ASSERT(VK_NULL_HANDLE != pSampler->mVk.pSampler);

    vkDestroySampler(pRenderer->mVk.pDevice, pSampler->mVk.pSampler, GetAllocationCallbacks(VK_OBJECT_TYPE_SAMPLER));

    if (NULL != pSampler->mVk.pSamplerYcbcrConversion)
    {
        vkDestroySamplerYcbcrConversion(pRenderer->mVk.pDevice, pSampler->mVk.pSamplerYcbcrConversion,
                                        GetAllocationCallbacks(VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION));
    }

    SAFE_FREE(pSampler);
}

/************************************************************************/
// Buffer Functions
/************************************************************************/
void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
    ASSERT(pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to map non-cpu accessible resource");

    CHECK_VKRESULT(vmaMapMemory(pRenderer->mVk.pVmaAllocator, pBuffer->mVk.pAllocation, &pBuffer->pCpuMappedAddress));

    if (pRange)
    {
        pBuffer->pCpuMappedAddress = ((uint8_t*)pBuffer->pCpuMappedAddress + pRange->mOffset);
    }
}

void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
    ASSERT(pBuffer->mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to unmap non-cpu accessible resource");

    vmaUnmapMemory(pRenderer->mVk.pVmaAllocator, pBuffer->mVk.pAllocation);
    pBuffer->pCpuMappedAddress = NULL;
}
/************************************************************************/
// Descriptor Set Functions
/************************************************************************/
static void GetOrAddSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppOutSampler)
{
    size_t samplerHash = tf_mem_hash_uint8_t((uint8_t*)pDesc, sizeof(SamplerDesc), 0);

    // Need pointer to pointer in order to reassign hash map when it is resized
    StaticSamplerNode** samplerMap = GetStaticSamplerMap(pRenderer->mUnlinkedRendererIndex);
    StaticSamplerNode*  node = hmgetp_null(*samplerMap, samplerHash);
    Sampler*            sampler = node ? node->value : NULL;
    if (!node)
    {
        addSampler(pRenderer, pDesc, &sampler);
        // No need of a lock here since this map is per thread
        hmput(*samplerMap, samplerHash, sampler);
    }

    *ppOutSampler = sampler;
}

static void AddDescriptorSetLayout(Renderer* pRenderer, const DescriptorSetLayoutDesc* pDesc, VkDescriptorSetLayout* ppOutLayout)
{
    VkDescriptorSetLayoutBinding* bindings = (VkDescriptorSetLayoutBinding*)tf_calloc(pDesc->mDescriptorCount + pDesc->mStaticSamplerCount,
                                                                                      sizeof(VkDescriptorSetLayoutBinding));
    const uint32_t                bindingCount = pDesc->mDescriptorCount + pDesc->mStaticSamplerCount;
    ASSERT(bindings);

    for (uint32_t descIndex = 0; descIndex < pDesc->mDescriptorCount; ++descIndex)
    {
        VkDescriptorSetLayoutBinding* binding = &bindings[descIndex];
        const Descriptor*             desc = &pDesc->pDescriptors[descIndex];
        binding->binding = desc->mOffset;
        binding->descriptorCount = desc->mCount;
        binding->descriptorType = ToVkDescriptorType(desc->mType);
        binding->pImmutableSamplers = NULL;
        binding->stageFlags = VK_SHADER_STAGE_ALL;
    }

    for (uint32_t descIndex = 0; descIndex < pDesc->mStaticSamplerCount; ++descIndex)
    {
        VkDescriptorSetLayoutBinding* binding = &bindings[pDesc->mDescriptorCount + descIndex];
        const StaticSamplerDesc*      desc = &pDesc->pStaticSamplers[descIndex];
        Sampler*                      sampler = NULL;
        GetOrAddSampler(pRenderer, &desc->mDesc, &sampler);
        binding->binding = desc->mBinding;
        binding->descriptorCount = 1;
        binding->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        binding->pImmutableSamplers = &sampler->mVk.pSampler;
        binding->stageFlags = VK_SHADER_STAGE_ALL;
    }

    VkDescriptorSetLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL };
    createInfo.bindingCount = bindingCount;
    createInfo.flags = 0;
    createInfo.pBindings = bindings;
    CHECK_VKRESULT(vkCreateDescriptorSetLayout(pRenderer->mVk.pDevice, &createInfo,
                                               GetAllocationCallbacks(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT), ppOutLayout));

    SAFE_FREE(bindings);
}

static size_t GetOrAddDescriptorSetLayout(Renderer* pRenderer, const DescriptorSetLayoutDesc* pDesc, VkDescriptorSetLayout* pOutLayout)
{
    size_t layoutHash = tf_mem_hash_uint8_t((const uint8_t*)&pDesc->mDescriptorCount, sizeof(uint32_t), 0);
    layoutHash = tf_mem_hash_uint8_t((const uint8_t*)&pDesc->mStaticSamplerCount, sizeof(uint32_t), layoutHash);
    for (uint32_t descIndex = 0; descIndex < pDesc->mDescriptorCount; ++descIndex)
    {
        layoutHash = tf_mem_hash_uint8_t((const uint8_t*)&pDesc->pDescriptors[descIndex], sizeof(pDesc->pDescriptors[0]), layoutHash);
    }
    for (uint32_t descIndex = 0; descIndex < pDesc->mStaticSamplerCount; ++descIndex)
    {
        layoutHash = tf_mem_hash_uint8_t((const uint8_t*)&pDesc->pStaticSamplers[descIndex], sizeof(SamplerDesc), layoutHash);
    }

    // Need pointer to pointer in order to reassign hash map when it is resized
    DescriptorLayoutNode** descriptorMap = GetDescriptorLayoutMap(pRenderer->mUnlinkedRendererIndex);
    DescriptorLayoutNode*  node = hmgetp_null(*descriptorMap, layoutHash);
    VkDescriptorSetLayout  setLayout = node ? node->value : VK_NULL_HANDLE;
    if (!node)
    {
        AddDescriptorSetLayout(pRenderer, pDesc, &setLayout);
        // No need of a lock here since this map is per thread
        hmput(*descriptorMap, layoutHash, setLayout);
    }

    *pOutLayout = setLayout;
    return layoutHash;
}

static void GetOrAddPipelineLayout(Renderer* pRenderer, const DescriptorSetLayoutDesc** pLayouts, uint32_t layoutCount,
                                   PipelineType pipelineType, PipelineLayout** ppOutLayout)
{
    VkDescriptorSetLayout* setLayouts = (VkDescriptorSetLayout*)alloca(layoutCount * sizeof(VkDescriptorSetLayout));
    ASSERT(setLayouts);

    size_t   pipelineLayoutHash = 0;
    uint32_t emptyLayouts = 0;
    bool     emptyStaticSamplerLayout = false;
    size_t   descSetsHashes[MAX_DESCRIPTOR_SETS] = { 0 };

    for (uint32_t layoutIndex = 0; layoutIndex < layoutCount; ++layoutIndex)
    {
        if (!pLayouts[layoutIndex])
        {
            setLayouts[layoutIndex] = pRenderer->mVk.pEmptyDescriptorSetLayout;
            emptyLayouts |= (1 << layoutIndex);
            pipelineLayoutHash = tf_mem_hash_uint8_t((const uint8_t*)&layoutIndex, sizeof(uint32_t), pipelineLayoutHash);
            descSetsHashes[layoutIndex] = 0;
            continue;
        }

        size_t layoutHash = GetOrAddDescriptorSetLayout(pRenderer, pLayouts[layoutIndex], &setLayouts[layoutIndex]);
        descSetsHashes[layoutIndex] = layoutHash;
        pipelineLayoutHash = tf_mem_hash_uint8_t((const uint8_t*)&layoutHash, sizeof(size_t), pipelineLayoutHash);

        if (pLayouts[layoutIndex]->mStaticSamplerCount && !pLayouts[layoutIndex]->mDescriptorCount)
        {
            emptyStaticSamplerLayout = true;
            ASSERTMSG(layoutIndex == 0, "Empty Static Sampler Layout is only supported for setIndex 0");
        }
    }

    // Need pointer to pointer in order to reassign hash map when it is resized
    PipelineLayoutNode** descriptorMap = GetPipelineLayoutMap(pRenderer->mUnlinkedRendererIndex);
    PipelineLayoutNode*  node = hmgetp_null(*descriptorMap, pipelineLayoutHash);
    PipelineLayout*      pipelineLayout = node ? node->value : NULL;
    if (!node)
    {
        VkPipelineLayout           layout = VK_NULL_HANDLE;
        VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL };
        createInfo.flags = 0;
        createInfo.pPushConstantRanges = NULL;
        createInfo.pSetLayouts = setLayouts;
        createInfo.pushConstantRangeCount = 1;
        createInfo.setLayoutCount = layoutCount;

        VkPushConstantRange pushConstantRange = { 0 };
        if (pipelineType == PIPELINE_TYPE_GRAPHICS)
        {
            pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        else
        {
            pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        pushConstantRange.size = sizeof(float) * MAX_PUSH_CONSTANTS_32BIT_COUNT;
        pushConstantRange.offset = 0;

        createInfo.pPushConstantRanges = &pushConstantRange;

        CHECK_VKRESULT(
            vkCreatePipelineLayout(pRenderer->mVk.pDevice, &createInfo, GetAllocationCallbacks(VK_OBJECT_TYPE_PIPELINE_LAYOUT), &layout));
        pipelineLayout = (PipelineLayout*)tf_calloc(1, sizeof(PipelineLayout));
        hmput(*descriptorMap, pipelineLayoutHash, pipelineLayout);
        pipelineLayout->pLayout = layout;
        pipelineLayout->mLayoutCount = layoutCount;
        pipelineLayout->mEmptyLayouts = emptyLayouts;
        pipelineLayout->mEmptyStaticSamplerLayout = emptyStaticSamplerLayout;

        memcpy(pipelineLayout->mDescSetsHashes, descSetsHashes, sizeof(descSetsHashes));
        // No need of a lock here since this map is per thread
    }

    *ppOutLayout = pipelineLayout;
}

static void CalculateDescriptorPoolSizes(const DescriptorSetDesc* pDesc,
                                         VkDescriptorPoolSize     outPoolSizes[MAX_DESCRIPTOR_POOL_SIZE_ARRAY_COUNT],
                                         uint32_t*                outPoolSizeCount)
{
    for (uint32_t b = 0; b < MAX_DESCRIPTOR_POOL_SIZE_ARRAY_COUNT; ++b)
    {
        outPoolSizes[b].descriptorCount = 0;
    }

    uint32_t poolSizeCount = 0;
    uint32_t poolSizeIndex[MAX_DESCRIPTOR_POOL_SIZE_ARRAY_COUNT] = { 0 };
    // Fill the descriptor pool sizes for this layout so it can be used later by the set allocator
    for (uint32_t b = 0; b < pDesc->mDescriptorCount; ++b)
    {
        const Descriptor* desc = &pDesc->pDescriptors[b];
        if (desc->mType != DESCRIPTOR_TYPE_UNDEFINED)
        {
            const VkDescriptorType type =
                desc->mType == DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : ToVkDescriptorType(desc->mType);

            if (!poolSizeIndex[type]) //-V557
            {
                poolSizeIndex[type] = ++poolSizeCount; //-V557
            }
            outPoolSizes[poolSizeIndex[type] - 1].type = type;                     //-V557
            outPoolSizes[poolSizeIndex[type] - 1].descriptorCount += desc->mCount; //-V557
        }
    }
    for (uint32_t b = 0; b < pDesc->mStaticSamplerCount; ++b)
    {
        const VkDescriptorType type = VK_DESCRIPTOR_TYPE_SAMPLER;
        if (!poolSizeIndex[type])
        {
            poolSizeIndex[type] = ++poolSizeCount;
        }
        outPoolSizes[poolSizeIndex[type] - 1].type = type;
        outPoolSizes[poolSizeIndex[type] - 1].descriptorCount += 1;
    }

    for (uint32_t b = 0; b < poolSizeCount; ++b)
    {
        outPoolSizes[b].descriptorCount *= pDesc->mMaxSets;
    }

    *outPoolSizeCount = poolSizeCount;
}

void addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppDescriptorSet);

    const uint8_t nodeIndex = (uint8_t)pDesc->mNodeIndex;

    uint32_t totalSize = sizeof(DescriptorSet);
    totalSize += pDesc->mMaxSets * sizeof(VkDescriptorSet);
    DescriptorSet* pDescriptorSet = (DescriptorSet*)tf_calloc_memalign(1, ALIGN_DescriptorSet, totalSize);
    ASSERT(pDescriptorSet);

    pDescriptorSet->mVk.mNodeIndex = nodeIndex;
    pDescriptorSet->mVk.mMaxSets = pDesc->mMaxSets;
    pDescriptorSet->mVk.pDescriptors = pDesc->pDescriptors;
    pDescriptorSet->mVk.mSetIndex = pDesc->mIndex;

    uint8_t* pMem = (uint8_t*)(pDescriptorSet + 1);
    pDescriptorSet->mVk.pHandles = (VkDescriptorSet*)pMem;
    pMem += pDesc->mMaxSets * sizeof(VkDescriptorSet);

    VkDescriptorPoolSize poolSizes[MAX_DESCRIPTOR_POOL_SIZE_ARRAY_COUNT] = { 0 };
    uint32_t             poolSizeCount = 0;
    CalculateDescriptorPoolSizes(pDesc, poolSizes, &poolSizeCount);

    AddDescriptorPool(pRenderer, pDesc->mMaxSets, 0, poolSizes, poolSizeCount, &pDescriptorSet->mVk.pDescriptorPool);
    VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(pDesc->mMaxSets * sizeof(VkDescriptorSetLayout));
    VkDescriptorSet**      sets = (VkDescriptorSet**)alloca(pDesc->mMaxSets * sizeof(VkDescriptorSet*));

    VkDescriptorSetLayout   setLayout = VK_NULL_HANDLE;
    DescriptorSetLayoutDesc layoutDesc = { 0 };
    layoutDesc.mDescriptorCount = pDesc->mDescriptorCount;
    layoutDesc.mStaticSamplerCount = pDesc->mStaticSamplerCount;
    layoutDesc.pDescriptors = pDesc->pDescriptors;
    layoutDesc.pStaticSamplers = pDesc->pStaticSamplers;
    size_t descSetHash = GetOrAddDescriptorSetLayout(pRenderer, &layoutDesc, &setLayout);

    for (uint32_t i = 0; i < pDesc->mMaxSets; ++i)
    {
        layouts[i] = setLayout;
        sets[i] = &pDescriptorSet->mVk.pHandles[i];
    }
    AllocateDescriptorSets(pRenderer, pDescriptorSet->mVk.pDescriptorPool, layouts, pDesc->mMaxSets, sets);

    for (uint32_t descIndex = 0; descIndex < pDesc->mDescriptorCount; ++descIndex)
    {
        const Descriptor* descInfo = &pDesc->pDescriptors[descIndex];
        DescriptorType    type = (DescriptorType)descInfo->mType;

        VkWriteDescriptorSet writeSet = { 0 };
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.pNext = NULL;
        writeSet.descriptorCount = 1;
        writeSet.dstArrayElement = 0;
        writeSet.dstBinding = descInfo->mOffset;

        for (uint32_t index = 0; index < pDesc->mMaxSets; ++index)
        {
            writeSet.dstSet = pDescriptorSet->mVk.pHandles[index];

            switch (type)
            {
            case DESCRIPTOR_TYPE_SAMPLER:
            {
                VkDescriptorImageInfo updateData = { pRenderer->pNullDescriptors->pDefaultSampler->mVk.pSampler, VK_NULL_HANDLE };
                writeSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                writeSet.pImageInfo = &updateData;
                for (uint32_t arr = 0; arr < descInfo->mCount; ++arr)
                {
                    writeSet.dstArrayElement = arr;
                    vkUpdateDescriptorSets(pRenderer->mVk.pDevice, 1, &writeSet, 0, NULL);
                }
                writeSet.pImageInfo = NULL;
                break;
            }
            case DESCRIPTOR_TYPE_TEXTURE:
            case DESCRIPTOR_TYPE_RW_TEXTURE:
            {
                VkImageView srcView =
                    (type == DESCRIPTOR_TYPE_RW_TEXTURE)
                        // #TODO
                        ? pRenderer->pNullDescriptors->pDefaultTextureUAV[nodeIndex][TEXTURE_DIM_2D]->mVk.pUAVDescriptors[0]
                        : pRenderer->pNullDescriptors->pDefaultTextureSRV[nodeIndex][TEXTURE_DIM_2D]->mVk.pSRVDescriptor;
                VkImageLayout layout =
                    (type == DESCRIPTOR_TYPE_RW_TEXTURE) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkDescriptorImageInfo updateData = { VK_NULL_HANDLE, srcView, layout };
                writeSet.descriptorType =
                    (type == DESCRIPTOR_TYPE_RW_TEXTURE) ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                writeSet.pImageInfo = &updateData;
                for (uint32_t arr = 0; arr < descInfo->mCount; ++arr)
                {
                    writeSet.dstArrayElement = arr;
                    vkUpdateDescriptorSets(pRenderer->mVk.pDevice, 1, &writeSet, 0, NULL);
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
                VkDescriptorBufferInfo updateData = { pRenderer->pNullDescriptors->pDefaultBufferSRV[nodeIndex]->mVk.pBuffer, 0,
                                                      VK_WHOLE_SIZE };
                writeSet.pBufferInfo = &updateData;
                if (DESCRIPTOR_TYPE_UNIFORM_BUFFER == type)
                {
                    writeSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                }
                else
                {
                    writeSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                }
                for (uint32_t arr = 0; arr < descInfo->mCount; ++arr)
                {
                    writeSet.dstArrayElement = arr;
                    vkUpdateDescriptorSets(pRenderer->mVk.pDevice, 1, &writeSet, 0, NULL);
                }
                writeSet.pBufferInfo = NULL;
                break;
            }
            case DESCRIPTOR_TYPE_TEXEL_BUFFER:
            case DESCRIPTOR_TYPE_RW_TEXEL_BUFFER:
            {
                VkBufferView updateData = (type == DESCRIPTOR_TYPE_RW_TEXEL_BUFFER)
                                              ? pRenderer->pNullDescriptors->pDefaultBufferUAV[nodeIndex]->mVk.pStorageTexelView
                                              : pRenderer->pNullDescriptors->pDefaultBufferSRV[nodeIndex]->mVk.pUniformTexelView;
                writeSet.pTexelBufferView = &updateData;
                writeSet.descriptorType = (type == DESCRIPTOR_TYPE_TEXEL_BUFFER) ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
                                                                                 : VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                for (uint32_t arr = 0; arr < descInfo->mCount; ++arr)
                {
                    writeSet.dstArrayElement = arr;
                    vkUpdateDescriptorSets(pRenderer->mVk.pDevice, 1, &writeSet, 0, NULL);
                }
                writeSet.pTexelBufferView = NULL;
                break;
            }
            default:
                break;
            }
        }
    }

    pDescriptorSet->mVk.mHash = descSetHash;
    *ppDescriptorSet = pDescriptorSet;
}

void removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
{
    ASSERT(pRenderer);
    ASSERT(pDescriptorSet);

    vkDestroyDescriptorPool(pRenderer->mVk.pDevice, pDescriptorSet->mVk.pDescriptorPool,
                            GetAllocationCallbacks(VK_OBJECT_TYPE_DESCRIPTOR_POOL));
    SAFE_FREE(pDescriptorSet);
}

// #NOTE - Should be good enough to avoid splitting most of the update calls other than edge cases having huge update sizes
#define VULKAN_MAX_DESCRIPTOR_SET_WRITES     256
#define VULKAN_MAX_DESCRIPTOR_INFO_BYTE_SIZE (1024 * sizeof(VkDescriptorImageInfo))

void updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
    ASSERT(pRenderer);
    ASSERT(pDescriptorSet);
    ASSERT(pDescriptorSet->mVk.pHandles);
    ASSERT(index < pDescriptorSet->mVk.mMaxSets);

    VkWriteDescriptorSet writeSetArray[VULKAN_MAX_DESCRIPTOR_SET_WRITES] = { 0 };
    uint8_t              descriptorUpdateDataStart[VULKAN_MAX_DESCRIPTOR_INFO_BYTE_SIZE] = { 0 };
    const uint8_t*       descriptorUpdateDataEnd = &descriptorUpdateDataStart[VULKAN_MAX_DESCRIPTOR_INFO_BYTE_SIZE - 1];
    uint32_t             writeSetCount = 0;

    uint8_t* descriptorUpdateData = descriptorUpdateDataStart;

#define FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(type, pInfo, count)                                  \
    if (descriptorUpdateData + sizeof(type) >= descriptorUpdateDataEnd)                        \
    {                                                                                          \
        writeSet->descriptorCount = arr - lastArrayIndexStart;                                 \
        vkUpdateDescriptorSets(pRenderer->mVk.pDevice, writeSetCount, writeSetArray, 0, NULL); \
        /* All previous write sets flushed. Start from zero */                                 \
        writeSetCount = 1;                                                                     \
        writeSetArray[0] = *writeSet;                                                          \
        writeSet = &writeSetArray[0];                                                          \
        lastArrayIndexStart = arr;                                                             \
        writeSet->dstArrayElement += writeSet->descriptorCount;                                \
        /* Set descriptor count to the remaining count */                                      \
        writeSet->descriptorCount = count - writeSet->dstArrayElement;                         \
        descriptorUpdateData = descriptorUpdateDataStart;                                      \
        writeSet->pInfo = (type*)descriptorUpdateData;                                         \
    }                                                                                          \
    type* currUpdateData = (type*)descriptorUpdateData;                                        \
    descriptorUpdateData += sizeof(type);

    for (uint32_t i = 0; i < count; ++i)
    {
        const DescriptorData* pParam = pParams + i;
        uint32_t              paramIndex = pParam->mIndex;

        VALIDATE_DESCRIPTOR((paramIndex != UINT32_MAX), "DescriptorData has NULL name and invalid index");

        const Descriptor* pDesc = pDescriptorSet->mVk.pDescriptors + paramIndex;

        const DescriptorType type = pDesc->mType;
        const uint32_t       arrayStart = pParam->mArrayOffset;
        const uint32_t       arrayCount = MAX(1U, pParam->mCount);

        // #NOTE - Flush the update if we go above the max write set limit
        if (writeSetCount >= VULKAN_MAX_DESCRIPTOR_SET_WRITES)
        {
            vkUpdateDescriptorSets(pRenderer->mVk.pDevice, writeSetCount, writeSetArray, 0, NULL);
            writeSetCount = 0;
            descriptorUpdateData = descriptorUpdateDataStart;
        }

        VkWriteDescriptorSet* writeSet = &writeSetArray[writeSetCount++];
        writeSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet->pNext = NULL;
        writeSet->descriptorCount = arrayCount;
        writeSet->dstArrayElement = arrayStart;
        writeSet->dstBinding = pDesc->mOffset;
        writeSet->dstSet = pDescriptorSet->mVk.pHandles[index];

        VALIDATE_DESCRIPTOR(pDesc->mSetIndex == pDescriptorSet->mVk.mSetIndex,
                            "Descriptor (%s) - Mismatching update frequency and set index", pDesc->pName);

        uint32_t lastArrayIndexStart = 0;

        switch (type)
        {
        case DESCRIPTOR_TYPE_SAMPLER:
        {
            VALIDATE_DESCRIPTOR(pParam->ppSamplers, "NULL Sampler (%s)", pDesc->pName);
            writeSet->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            writeSet->pImageInfo = (VkDescriptorImageInfo*)descriptorUpdateData;

            for (uint32_t arr = 0; arr < arrayCount; ++arr)
            {
                VALIDATE_DESCRIPTOR(pParam->ppSamplers[arr], "NULL Sampler (%s [%u] )", pDesc->pName, arr);
                FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkDescriptorImageInfo, pImageInfo, arrayCount) //-V1032
                *currUpdateData = (VkDescriptorImageInfo){ pParam->ppSamplers[arr]->mVk.pSampler, VK_NULL_HANDLE };
            }
            break;
        }
        case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        {
            VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)", pDesc->pName);
            writeSet->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeSet->pImageInfo = (VkDescriptorImageInfo*)descriptorUpdateData;

            for (uint32_t arr = 0; arr < arrayCount; ++arr)
            {
                VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);
                FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkDescriptorImageInfo, pImageInfo, arrayCount)
                *currUpdateData = (VkDescriptorImageInfo){
                    NULL,                                        // Sampler
                    pParam->ppTextures[arr]->mVk.pSRVDescriptor, // Image View
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL     // Image Layout
                };
            }
            break;
        }
        case DESCRIPTOR_TYPE_TEXTURE:
        {
            VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)", pDesc->pName);
            writeSet->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writeSet->pImageInfo = (VkDescriptorImageInfo*)descriptorUpdateData;

            if (!pParam->mBindStencilResource)
            {
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);
                    FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkDescriptorImageInfo, pImageInfo, arrayCount)
                    *currUpdateData = (VkDescriptorImageInfo){
                        VK_NULL_HANDLE,                              // Sampler
                        pParam->ppTextures[arr]->mVk.pSRVDescriptor, // Image View
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL     // Image Layout
                    };
                }
            }
            else
            {
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);
                    FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkDescriptorImageInfo, pImageInfo, arrayCount)
                    *currUpdateData = (VkDescriptorImageInfo){
                        VK_NULL_HANDLE,                                     // Sampler
                        pParam->ppTextures[arr]->mVk.pSRVStencilDescriptor, // Image View
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL            // Image Layout
                    };
                }
            }
            break;
        }
        case DESCRIPTOR_TYPE_RW_TEXTURE:
        {
            VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL RW Texture (%s)", pDesc->pName);

            writeSet->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writeSet->pImageInfo = (VkDescriptorImageInfo*)descriptorUpdateData;

            if (pParam->mBindMipChain)
            {
                VALIDATE_DESCRIPTOR(pParam->ppTextures[0], "NULL RW Texture (%s)", pDesc->pName);
                VALIDATE_DESCRIPTOR(
                    (!arrayStart),
                    "Descriptor (%s) - mBindMipChain supports only updating the whole mip-chain. No partial updates supported",
                    pDesc->pName);
                const uint32_t mipCount = pParam->ppTextures[0]->mMipLevels;
                writeSet->descriptorCount = mipCount;

                for (uint32_t arr = 0; arr < mipCount; ++arr)
                {
                    FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkDescriptorImageInfo, pImageInfo, mipCount)
                    *currUpdateData = (VkDescriptorImageInfo){
                        VK_NULL_HANDLE,                                  // Sampler
                        pParam->ppTextures[0]->mVk.pUAVDescriptors[arr], // Image View
                        VK_IMAGE_LAYOUT_GENERAL                          // Image Layout
                    };
                }
            }
            else
            {
                const uint32_t mipSlice = pParam->mUAVMipSlice;

                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL RW Texture (%s [%u] )", pDesc->pName, arr);
                    VALIDATE_DESCRIPTOR(mipSlice < pParam->ppTextures[arr]->mMipLevels,
                                        "Descriptor : (%s [%u] ) Mip Slice (%u) exceeds mip levels (%u)", pDesc->pName, arr, mipSlice,
                                        pParam->ppTextures[arr]->mMipLevels);

                    FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkDescriptorImageInfo, pImageInfo, arrayCount)
                    *currUpdateData = (VkDescriptorImageInfo){
                        VK_NULL_HANDLE,                                         // Sampler
                        pParam->ppTextures[arr]->mVk.pUAVDescriptors[mipSlice], // Image View
                        VK_IMAGE_LAYOUT_GENERAL                                 // Image Layout
                    };
                }
            }
            break;
        }
        case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        {
            // #TODO
            // if (pDesc->mRootDescriptor)
            //{
            //        VALIDATE_DESCRIPTOR(false,
            //                            "Descriptor (%s) - Trying to update a root cbv through updateDescriptorSet. All root cbvs must be
            //                            " "updated through cmdBindDescriptorSetWithRootCbvs", pDesc->pName);

            //        break;
            //}
        case DESCRIPTOR_TYPE_BUFFER:
        case DESCRIPTOR_TYPE_BUFFER_RAW:
        case DESCRIPTOR_TYPE_RW_BUFFER:
        case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
        {
            VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Buffer (%s)", pDesc->pName);
            writeSet->descriptorType =
                (type == DESCRIPTOR_TYPE_UNIFORM_BUFFER) ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeSet->pBufferInfo = (VkDescriptorBufferInfo*)descriptorUpdateData;

            for (uint32_t arr = 0; arr < arrayCount; ++arr)
            {
                VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Buffer (%s [%u] )", pDesc->pName, arr);
                FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkDescriptorBufferInfo, pBufferInfo, arrayCount)
                *currUpdateData =
                    (VkDescriptorBufferInfo){ pParam->ppBuffers[arr]->mVk.pBuffer, pParam->ppBuffers[arr]->mVk.mOffset, VK_WHOLE_SIZE };

                if (pParam->pRanges)
                {
                    DescriptorDataRange range = pParam->pRanges[arr];
#if defined(ENABLE_GRAPHICS_VALIDATION)
                    uint32_t maxRange = DESCRIPTOR_TYPE_UNIFORM_BUFFER == type
                                            ? pRenderer->pGpu->mVk.mGpuProperties.properties.limits.maxUniformBufferRange
                                            : pRenderer->pGpu->mVk.mGpuProperties.properties.limits.maxStorageBufferRange;

                    VALIDATE_DESCRIPTOR(range.mSize, "Descriptor (%s) - pRanges[%u].mSize is zero", pDesc->pName, arr);
                    VALIDATE_DESCRIPTOR(range.mSize <= maxRange, "Descriptor (%s) - pRanges[%u].mSize is %ull which exceeds max size %u",
                                        pDesc->pName, arr, range.mSize, maxRange);
#endif
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
            writeSet->descriptorType =
                (DESCRIPTOR_TYPE_TEXEL_BUFFER == type) ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
            writeSet->pTexelBufferView = (VkBufferView*)descriptorUpdateData;

            for (uint32_t arr = 0; arr < arrayCount; ++arr)
            {
                VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Texel Buffer (%s [%u] )", pDesc->pName, arr);
                FLUSH_OVERFLOW_DESCRIPTOR_UPDATES(VkBufferView, pTexelBufferView, arrayCount)
                *currUpdateData = DESCRIPTOR_TYPE_TEXEL_BUFFER == type ? pParam->ppBuffers[arr]->mVk.pUniformTexelView
                                                                       : pParam->ppBuffers[arr]->mVk.pStorageTexelView;
            }

            break;
        }
        case DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE:
        {
            VALIDATE_DESCRIPTOR(pParam->ppAccelerationStructures, "NULL Acceleration Structure (%s)", pDesc->pName);

            VkWriteDescriptorSetAccelerationStructureKHR writeSetKHR = { 0 };
            VkAccelerationStructureKHR                   currUpdateData = { 0 };
            writeSet->descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            writeSet->pNext = &writeSetKHR;
            writeSet->descriptorCount = 1;
            writeSetKHR.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            writeSetKHR.pNext = NULL;
            writeSetKHR.accelerationStructureCount = 1;
            for (uint32_t arr = 0; arr < arrayCount; ++arr)
            {
                vk_FillRaytracingDescriptorData(pParam->ppAccelerationStructures[arr], &currUpdateData);
                writeSetKHR.pAccelerationStructures = &currUpdateData;
                vkUpdateDescriptorSets(pRenderer->mVk.pDevice, 1, writeSet, 0, NULL);
                ++writeSet->dstArrayElement;
            }

            // Update done - Dont need this write set anymore. Return it to the array
            writeSet->pNext = NULL;
            --writeSetCount;

            break;
        }
        default:
            break;
        }
    }

    vkUpdateDescriptorSets(pRenderer->mVk.pDevice, writeSetCount, writeSetArray, 0, NULL);
}

//#todo
// static const uint32_t VK_MAX_ROOT_DESCRIPTORS = 32;

void cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
{
    ASSERT(pCmd);
    ASSERT(pDescriptorSet);
    ASSERT(pDescriptorSet->mVk.pHandles);
    ASSERT(index < pDescriptorSet->mVk.mMaxSets);

    pCmd->mVk.mBoundDescriptorSets[pDescriptorSet->mVk.mSetIndex] = pDescriptorSet;
    pCmd->mVk.mBoundDescriptorSetIndices[pDescriptorSet->mVk.mSetIndex] = index;
    pCmd->mVk.mBoundDescriptorSetHashes[pDescriptorSet->mVk.mSetIndex] = pDescriptorSet->mVk.mHash;

    // if no pipeline layout is bound yet, mask this descriptor set to be bound when cmdBindPipeline is called
    if (pCmd->mVk.pBoundPipelineLayout == NULL)
    {
        pCmd->mVk.mShouldRebindDescriptorSetsMask |= 0x01 << pDescriptorSet->mVk.mSetIndex;
        return;
    }

    vkCmdBindDescriptorSets(pCmd->mVk.pCmdBuf, gPipelineBindPoint[pCmd->mVk.mPipelineType], pCmd->mVk.pBoundPipelineLayout,
                            pDescriptorSet->mVk.mSetIndex, 1, &pDescriptorSet->mVk.pHandles[index], 0, NULL);
}
/************************************************************************/
// Shader Functions
/************************************************************************/
void addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppShaderProgram);
    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);

    size_t totalSize = sizeof(Shader);

    for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
    {
        ShaderStage stage_mask = (ShaderStage)(1 << i);
        if (stage_mask == (pDesc->mStages & stage_mask))
        {
            switch (stage_mask)
            {
            case SHADER_STAGE_VERT:
                totalSize += (strlen(pDesc->mVert.pEntryPoint) + 1) * sizeof(char); //-V814
                break;
            case SHADER_STAGE_TESC:
                totalSize += (strlen(pDesc->mHull.pEntryPoint) + 1) * sizeof(char); //-V814
                break;
            case SHADER_STAGE_TESE:
                totalSize += (strlen(pDesc->mDomain.pEntryPoint) + 1) * sizeof(char); //-V814
                break;
            case SHADER_STAGE_GEOM:
                totalSize += (strlen(pDesc->mGeom.pEntryPoint) + 1) * sizeof(char); //-V814
                break;
            case SHADER_STAGE_FRAG:
                totalSize += (strlen(pDesc->mFrag.pEntryPoint) + 1) * sizeof(char); //-V814
                break;
            case SHADER_STAGE_COMP:
                totalSize += (strlen(pDesc->mComp.pEntryPoint) + 1) * sizeof(char); //-V814
                break;
            default:
                break;
            }
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
    Shader* pShaderProgram = (Shader*)tf_calloc(1, totalSize);
    pShaderProgram->mStages = pDesc->mStages;
    pShaderProgram->mVk.pSpecializationInfo = NULL;

    uint8_t* mem = (uint8_t*)(pShaderProgram + 1);

    for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
    {
        ShaderStage stage_mask = (ShaderStage)(1 << i);
        if (stage_mask == (pShaderProgram->mStages & stage_mask))
        {
            VkShaderModuleCreateInfo create_info = { 0 };
            create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            create_info.pNext = NULL;
            create_info.flags = 0;
            switch (stage_mask)
            {
            case SHADER_STAGE_VERT:
            {
                create_info.codeSize = pDesc->mVert.mByteCodeSize;
                create_info.pCode = (const uint32_t*)pDesc->mVert.pByteCode;
                CHECK_VKRESULT(vkCreateShaderModule(pRenderer->mVk.pDevice, &create_info,
                                                    GetAllocationCallbacks(VK_OBJECT_TYPE_SHADER_MODULE), &(pShaderProgram->mVk.pVS)));

                SetVkObjectName(pRenderer, (uint64_t)pShaderProgram->mVk.pVS, VK_OBJECT_TYPE_SHADER_MODULE,
                                VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, pDesc->mVert.pName);
            }
            break;
            case SHADER_STAGE_TESC:
            {
                create_info.codeSize = pDesc->mHull.mByteCodeSize;
                create_info.pCode = (const uint32_t*)pDesc->mHull.pByteCode;
                CHECK_VKRESULT(vkCreateShaderModule(pRenderer->mVk.pDevice, &create_info,
                                                    GetAllocationCallbacks(VK_OBJECT_TYPE_SHADER_MODULE), &(pShaderProgram->mVk.pHS)));

                SetVkObjectName(pRenderer, (uint64_t)pShaderProgram->mVk.pHS, VK_OBJECT_TYPE_SHADER_MODULE,
                                VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, pDesc->mHull.pName);
            }
            break;
            case SHADER_STAGE_TESE:
            {
                create_info.codeSize = pDesc->mDomain.mByteCodeSize;
                create_info.pCode = (const uint32_t*)pDesc->mDomain.pByteCode;
                CHECK_VKRESULT(vkCreateShaderModule(pRenderer->mVk.pDevice, &create_info,
                                                    GetAllocationCallbacks(VK_OBJECT_TYPE_SHADER_MODULE), &(pShaderProgram->mVk.pDS)));

                SetVkObjectName(pRenderer, (uint64_t)pShaderProgram->mVk.pDS, VK_OBJECT_TYPE_SHADER_MODULE,
                                VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, pDesc->mDomain.pName);
            }
            break;
            case SHADER_STAGE_GEOM:
            {
                create_info.codeSize = pDesc->mGeom.mByteCodeSize;
                create_info.pCode = (const uint32_t*)pDesc->mGeom.pByteCode;
                CHECK_VKRESULT(vkCreateShaderModule(pRenderer->mVk.pDevice, &create_info,
                                                    GetAllocationCallbacks(VK_OBJECT_TYPE_SHADER_MODULE), &(pShaderProgram->mVk.pGS)));

                SetVkObjectName(pRenderer, (uint64_t)pShaderProgram->mVk.pGS, VK_OBJECT_TYPE_SHADER_MODULE,
                                VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, pDesc->mGeom.pName);
            }
            break;
            case SHADER_STAGE_FRAG:
            {
                create_info.codeSize = pDesc->mFrag.mByteCodeSize;
                create_info.pCode = (const uint32_t*)pDesc->mFrag.pByteCode;
                CHECK_VKRESULT(vkCreateShaderModule(pRenderer->mVk.pDevice, &create_info,
                                                    GetAllocationCallbacks(VK_OBJECT_TYPE_SHADER_MODULE), &(pShaderProgram->mVk.pPS)));

                SetVkObjectName(pRenderer, (uint64_t)pShaderProgram->mVk.pPS, VK_OBJECT_TYPE_SHADER_MODULE,
                                VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, pDesc->mFrag.pName);
            }
            break;
            case SHADER_STAGE_COMP:
            {
                create_info.codeSize = pDesc->mComp.mByteCodeSize;
                create_info.pCode = (const uint32_t*)pDesc->mComp.pByteCode;
                CHECK_VKRESULT(vkCreateShaderModule(pRenderer->mVk.pDevice, &create_info,
                                                    GetAllocationCallbacks(VK_OBJECT_TYPE_SHADER_MODULE), &(pShaderProgram->mVk.pCS)));

                SetVkObjectName(pRenderer, (uint64_t)pShaderProgram->mVk.pCS, VK_OBJECT_TYPE_SHADER_MODULE,
                                VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, pDesc->mComp.pName);
            }
            break;
            default:
                ASSERT(false && "Shader Stage not supported!");
                break;
            }
        }
    }

    // Fill specialization constant entries
    if (pDesc->mConstantCount)
    {
        pShaderProgram->mVk.pSpecializationInfo = (VkSpecializationInfo*)mem;
        mem += sizeof(VkSpecializationInfo);

        VkSpecializationMapEntry* mapEntries = (VkSpecializationMapEntry*)mem;
        mem += pDesc->mConstantCount * sizeof(VkSpecializationMapEntry);

        uint8_t* data = mem;
        uint32_t offset = 0;
        for (uint32_t i = 0; i < pDesc->mConstantCount; ++i)
        {
            const ShaderConstant* constant = &pDesc->pConstants[i];
            const bool            boolType = constant->mSize == sizeof(bool);
            const uint32_t        size = boolType ? sizeof(VkBool32) : constant->mSize;

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

        VkSpecializationInfo* specializationInfo = pShaderProgram->mVk.pSpecializationInfo;
        specializationInfo->dataSize = offset;
        specializationInfo->mapEntryCount = pDesc->mConstantCount;
        specializationInfo->pData = data;
        specializationInfo->pMapEntries = mapEntries;
    }

#if defined(QUEST_VR)
    pShaderProgram->mIsMultiviewVR = pDesc->mIsMultiviewVR;
#endif

    *ppShaderProgram = pShaderProgram;
}

void removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
    ASSERT(pRenderer);

    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);

    if (pShaderProgram->mStages & SHADER_STAGE_VERT)
    {
        vkDestroyShaderModule(pRenderer->mVk.pDevice, pShaderProgram->mVk.pVS, GetAllocationCallbacks(VK_OBJECT_TYPE_SHADER_MODULE));
    }

    if (pShaderProgram->mStages & SHADER_STAGE_TESC)
    {
        vkDestroyShaderModule(pRenderer->mVk.pDevice, pShaderProgram->mVk.pHS, GetAllocationCallbacks(VK_OBJECT_TYPE_SHADER_MODULE));
    }

    if (pShaderProgram->mStages & SHADER_STAGE_TESE)
    {
        vkDestroyShaderModule(pRenderer->mVk.pDevice, pShaderProgram->mVk.pDS, GetAllocationCallbacks(VK_OBJECT_TYPE_SHADER_MODULE));
    }

    if (pShaderProgram->mStages & SHADER_STAGE_GEOM)
    {
        vkDestroyShaderModule(pRenderer->mVk.pDevice, pShaderProgram->mVk.pGS, GetAllocationCallbacks(VK_OBJECT_TYPE_SHADER_MODULE));
    }

    if (pShaderProgram->mStages & SHADER_STAGE_FRAG)
    {
        vkDestroyShaderModule(pRenderer->mVk.pDevice, pShaderProgram->mVk.pPS, GetAllocationCallbacks(VK_OBJECT_TYPE_SHADER_MODULE));
    }

    if (pShaderProgram->mStages & SHADER_STAGE_COMP)
    {
        vkDestroyShaderModule(pRenderer->mVk.pDevice, pShaderProgram->mVk.pCS, GetAllocationCallbacks(VK_OBJECT_TYPE_SHADER_MODULE));
    }

    SAFE_FREE(pShaderProgram);
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
    VkPipelineCache             psoCache = pMainDesc->pCache ? pMainDesc->pCache->mVk.pCache : VK_NULL_HANDLE;

    ASSERT(pDesc->pShaderProgram);

    Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, ALIGN_Pipeline, sizeof(Pipeline));
    ASSERT(pPipeline);

    const Shader*       pShaderProgram = pDesc->pShaderProgram;
    const VertexLayout* pVertexLayout = pDesc->pVertexLayout;

    pPipeline->mVk.mType = PIPELINE_TYPE_GRAPHICS;

    // Create tempporary renderpass for pipeline creation
    RenderPassDesc renderPassDesc = { 0 };
    RenderPass     renderPass = { 0 };

    VkPipelineRenderingCreateInfoKHR rc = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR };
    VkFormat                         rcColorFormats[MAX_RENDER_TARGET_ATTACHMENTS] = { 0 };

    if (pRenderer->pGpu->mDynamicRenderingSupported)
    {
        for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i)
        {
            rcColorFormats[i] = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->pColorFormats[i]);
        }
        rc.colorAttachmentCount = pDesc->mRenderTargetCount;
        rc.pColorAttachmentFormats = rcColorFormats;
        rc.depthAttachmentFormat = TinyImageFormat_HasDepth(pDesc->mDepthStencilFormat)
                                       ? (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mDepthStencilFormat)
                                       : VK_FORMAT_UNDEFINED;
        rc.stencilAttachmentFormat = TinyImageFormat_HasStencil(pDesc->mDepthStencilFormat)
                                         ? (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mDepthStencilFormat)
                                         : VK_FORMAT_UNDEFINED;
    }
    else
    {
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
        renderPassDesc.mDepthLazilyAllocated = false;
        AddRenderPass(pRenderer, &renderPassDesc, &renderPass);
    }

    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);

    const VkSpecializationInfo* specializationInfo = pShaderProgram->mVk.pSpecializationInfo;
    static const char*          defaultEntryName = "main";
    // Pipeline
    {
        uint32_t                        stage_count = 0;
        VkPipelineShaderStageCreateInfo stages[5] = { 0 };
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
                    stages[stage_count].pName = defaultEntryName;
                    stages[stage_count].stage = VK_SHADER_STAGE_VERTEX_BIT;
                    stages[stage_count].module = pShaderProgram->mVk.pVS;
                }
                break;
                case SHADER_STAGE_TESC:
                {
                    stages[stage_count].pName = defaultEntryName;
                    stages[stage_count].stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
                    stages[stage_count].module = pShaderProgram->mVk.pHS;
                }
                break;
                case SHADER_STAGE_TESE:
                {
                    stages[stage_count].pName = defaultEntryName;
                    stages[stage_count].stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
                    stages[stage_count].module = pShaderProgram->mVk.pDS;
                }
                break;
                case SHADER_STAGE_GEOM:
                {
                    stages[stage_count].pName = defaultEntryName;
                    stages[stage_count].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
                    stages[stage_count].module = pShaderProgram->mVk.pGS;
                }
                break;
                case SHADER_STAGE_FRAG:
                {
                    stages[stage_count].pName = defaultEntryName;
                    stages[stage_count].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                    stages[stage_count].module = pShaderProgram->mVk.pPS;
                }
                break;
                default:
                    ASSERT(false && "Shader Stage not supported!");
                    break;
                }
                ++stage_count;
            }
        }

        // Make sure there's a shader
        ASSERT(0 != stage_count);

        VkVertexInputBindingDescription   inputBindings[MAX_VERTEX_BINDINGS] = { { 0 } };
        VkVertexInputAttributeDescription inputAttributes[MAX_VERTEX_ATTRIBS] = { { 0 } };

        VkPipelineVertexInputStateCreateInfo vi = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pVertexBindingDescriptions = inputBindings,
            .pVertexAttributeDescriptions = inputAttributes,
        };

        // Make sure there's attributes
        if (pVertexLayout != NULL)
        {
            // Ignore everything that's beyond max_vertex_attribs
            uint32_t attribCount = pVertexLayout->mAttribCount > MAX_VERTEX_ATTRIBS ? MAX_VERTEX_ATTRIBS : pVertexLayout->mAttribCount;
            uint32_t bindingCount = pVertexLayout->mBindingCount > MAX_VERTEX_BINDINGS ? MAX_VERTEX_BINDINGS : pVertexLayout->mBindingCount;

            ASSERT(attribCount && bindingCount);

            // Bindings
            for (uint32_t b = 0; b < bindingCount; ++b)
            {
                const VertexBinding*             vertexBinding = &pVertexLayout->mBindings[b];
                VkVertexInputBindingDescription* inputBinding = &inputBindings[b];

                // init binding
                inputBinding->binding = b;
                if (vertexBinding->mRate == VERTEX_BINDING_RATE_INSTANCE)
                {
                    inputBinding->inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
                }
                else
                {
                    inputBinding->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                }

                inputBinding->stride = vertexBinding->mStride;
            }

            // Attributes
            for (uint32_t i = 0; i < attribCount; ++i)
            {
                const VertexAttrib*  attrib = &pVertexLayout->mAttribs[i];
                const VertexBinding* vertexBinding = &pVertexLayout->mBindings[attrib->mBinding];
                inputAttributes[i].location = attrib->mLocation;
                inputAttributes[i].binding = attrib->mBinding;
                inputAttributes[i].format = (VkFormat)TinyImageFormat_ToVkFormat(attrib->mFormat);
                inputAttributes[i].offset = attrib->mOffset;

                // update binding stride if necessary
                if (!vertexBinding->mStride)
                {
                    // guessing stride using attribute offset in case there are several attributes at the same binding
                    VkVertexInputBindingDescription* inputBinding = &inputBindings[attrib->mBinding];
                    inputBinding->stride = MAX(attrib->mOffset + TinyImageFormat_BitSizeOfBlock(attrib->mFormat) / 8, inputBinding->stride);
                }
            }

            vi.vertexBindingDescriptionCount = pVertexLayout->mBindingCount;
            vi.vertexAttributeDescriptionCount = pVertexLayout->mAttribCount;
        }

        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        switch (pDesc->mPrimitiveTopo)
        {
        case PRIMITIVE_TOPO_POINT_LIST:
            topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            break;
        case PRIMITIVE_TOPO_LINE_LIST:
            topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            break;
        case PRIMITIVE_TOPO_LINE_STRIP:
            topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            break;
        case PRIMITIVE_TOPO_TRI_STRIP:
            topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            break;
        case PRIMITIVE_TOPO_PATCH_LIST:
            topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
            break;
        case PRIMITIVE_TOPO_TRI_LIST:
            topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            break;
        default:
            ASSERT(false && "Primitive Topo not supported!");
            break;
        }
        VkPipelineInputAssemblyStateCreateInfo ia = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = topology,
            .primitiveRestartEnable = VK_FALSE,
        };

        VkPipelineTessellationStateCreateInfo ts = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
        if ((pShaderProgram->mStages & SHADER_STAGE_TESC) && (pShaderProgram->mStages & SHADER_STAGE_TESE))
        {
            ts.patchControlPoints = pShaderProgram->mNumControlPoints;
        }

        VkPipelineViewportStateCreateInfo vs = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            // we are using dynamic viewports but we must set the count to 1
            .viewportCount = 1,
            .pViewports = NULL,
            .scissorCount = 1,
            .pScissors = NULL,
        };

        VkPipelineMultisampleStateCreateInfo ms = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .rasterizationSamples = util_to_vk_sample_count(pRenderer, pDesc->mSampleCount),
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 0.0f,
            .pSampleMask = 0,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        };

        VkPipelineSampleLocationsStateCreateInfoEXT sl = { 0 };
        if (pDesc->mUseCustomSampleLocations)
        {
            sl.sType = VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT;
            sl.sampleLocationsEnable = VK_TRUE;
            sl.sampleLocationsInfo = (VkSampleLocationsInfoEXT){ VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT };
            ms.pNext = &sl;
        }

        VkPipelineRasterizationStateCreateInfo rs =
            pDesc->pRasterizerState ? util_to_rasterizer_desc(pRenderer, pDesc->pRasterizerState) : gDefaultRasterizerDesc;

        /// TODO: Dont create depth state if no depth stencil bound
        VkPipelineDepthStencilStateCreateInfo ds = pDesc->pDepthState ? util_to_depth_desc(pDesc->pDepthState) : gDefaultDepthDesc;

        VkPipelineColorBlendStateCreateInfo cb = { 0 };
        VkPipelineColorBlendAttachmentState cbAtt[MAX_RENDER_TARGET_ATTACHMENTS] = { 0 };
        cb = pDesc->pBlendState ? util_to_blend_desc(pDesc->pBlendState, cbAtt) : gDefaultBlendDesc;
        cb.attachmentCount = pDesc->mRenderTargetCount;

        VkDynamicState dyn_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_BLEND_CONSTANTS,
            VK_DYNAMIC_STATE_DEPTH_BOUNDS,
            VK_DYNAMIC_STATE_STENCIL_REFERENCE,
            // Should be last in the list. Causes validation error if the state is enabled, but not used
            VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT,
        };
        VkPipelineDynamicStateCreateInfo dy = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = pDesc->mUseCustomSampleLocations ? sizeof(dyn_states) / sizeof(dyn_states[0])
                                                                  : sizeof(dyn_states) / sizeof(dyn_states[0]) - 1,
            .pDynamicStates = dyn_states,
        };

        PipelineLayout* pipelineLayout = NULL;
        GetOrAddPipelineLayout(pRenderer, pMainDesc->pLayouts, pMainDesc->mLayoutCount, PIPELINE_TYPE_GRAPHICS, &pipelineLayout);

        VkGraphicsPipelineCreateInfo add_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = stage_count,
            .pStages = stages,
            .pVertexInputState = &vi,
            .pInputAssemblyState = &ia,
            .pViewportState = &vs,
            .pRasterizationState = &rs,
            .pMultisampleState = &ms,
            .pDepthStencilState = &ds,
            .pColorBlendState = &cb,
            .pDynamicState = &dy,
            .layout = pipelineLayout->pLayout,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1,
        };

        if ((pShaderProgram->mStages & SHADER_STAGE_TESC) && (pShaderProgram->mStages & SHADER_STAGE_TESE))
            add_info.pTessellationState = &ts;
        else
            add_info.pTessellationState = NULL; // set tessellation state to null if we have no tessellation

        if (pRenderer->pGpu->mDynamicRenderingSupported)
        {
            add_info.pNext = &rc;
            add_info.renderPass = VK_NULL_HANDLE;
        }
        else
        {
            add_info.renderPass = renderPass.pRenderPass;
        }
        CHECK_VKRESULT(vkCreateGraphicsPipelines(pRenderer->mVk.pDevice, psoCache, 1, &add_info,
                                                 GetAllocationCallbacks(VK_OBJECT_TYPE_PIPELINE), &(pPipeline->mVk.pPipeline)));

        if (renderPass.pRenderPass)
        {
            RemoveRenderPass(pRenderer, &renderPass);
        }

        pPipeline->mVk.pPipelineLayout = pipelineLayout;
    }

    *ppPipeline = pPipeline;
}

static void addComputePipeline(Renderer* pRenderer, const PipelineDesc* pMainDesc, Pipeline** ppPipeline)
{
    ASSERT(pRenderer);
    ASSERT(ppPipeline);
    ASSERT(pMainDesc);

    const ComputePipelineDesc* pDesc = &pMainDesc->mComputeDesc;
    VkPipelineCache            psoCache = pMainDesc->pCache ? pMainDesc->pCache->mVk.pCache : VK_NULL_HANDLE;

    ASSERT(pDesc->pShaderProgram);
    ASSERT(pRenderer->mVk.pDevice != VK_NULL_HANDLE);
    ASSERT(pDesc->pShaderProgram->mVk.pCS != VK_NULL_HANDLE);

    Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, ALIGN_Pipeline, sizeof(Pipeline));
    ASSERT(pPipeline);
    pPipeline->mVk.mType = PIPELINE_TYPE_COMPUTE;

    // Pipeline
    {
        VkPipelineShaderStageCreateInfo stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = pDesc->pShaderProgram->mVk.pCS,
            .pName = "main",
            .pSpecializationInfo = pDesc->pShaderProgram->mVk.pSpecializationInfo,
        };

        PipelineLayout* pipelineLayout = NULL;
        GetOrAddPipelineLayout(pRenderer, pMainDesc->pLayouts, pMainDesc->mLayoutCount, PIPELINE_TYPE_COMPUTE, &pipelineLayout);

        VkComputePipelineCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = stage,
            .layout = pipelineLayout->pLayout,
        };
        CHECK_VKRESULT(vkCreateComputePipelines(pRenderer->mVk.pDevice, psoCache, 1, &create_info,
                                                GetAllocationCallbacks(VK_OBJECT_TYPE_PIPELINE), &(pPipeline->mVk.pPipeline)));

        pPipeline->mVk.pPipelineLayout = pipelineLayout;
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
    default:
    {
        ASSERTFAIL("Unknown pipeline type %i", pDesc->mType);
        *ppPipeline = NULL;
        break;
    }
    }

#if defined(SHADER_STATS_AVAILABLE)
    // Both members in union have pShaderProgram at same offset
    (*ppPipeline)->mVk.mShaderStages = pDesc->mComputeDesc.pShaderProgram->mStages; //-V595
#endif
    if (*ppPipeline && pDesc->pName)
    {
        setPipelineName(pRenderer, *ppPipeline, pDesc->pName);
    }
}

void removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
    ASSERT(pRenderer);
    ASSERT(pPipeline);
    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);
    ASSERT(VK_NULL_HANDLE != pPipeline->mVk.pPipeline);

    vkDestroyPipeline(pRenderer->mVk.pDevice, pPipeline->mVk.pPipeline, GetAllocationCallbacks(VK_OBJECT_TYPE_PIPELINE));

    SAFE_FREE(pPipeline);
}

void addPipelineCache(Renderer* pRenderer, const PipelineCacheDesc* pDesc, PipelineCache** ppPipelineCache)
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
    CHECK_VKRESULT(vkCreatePipelineCache(pRenderer->mVk.pDevice, &psoCacheCreateInfo, GetAllocationCallbacks(VK_OBJECT_TYPE_PIPELINE_CACHE),
                                         &pPipelineCache->mVk.pCache));

    *ppPipelineCache = pPipelineCache;
}

void removePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache)
{
    ASSERT(pRenderer);
    ASSERT(pPipelineCache);

    if (pPipelineCache->mVk.pCache)
    {
        vkDestroyPipelineCache(pRenderer->mVk.pDevice, pPipelineCache->mVk.pCache, GetAllocationCallbacks(VK_OBJECT_TYPE_PIPELINE_CACHE));
    }

    SAFE_FREE(pPipelineCache);
}

void getPipelineCacheData(Renderer* pRenderer, PipelineCache* pPipelineCache, size_t* pSize, void* pData)
{
    ASSERT(pRenderer);
    ASSERT(pPipelineCache);
    ASSERT(pSize);

    if (pPipelineCache->mVk.pCache)
    {
        CHECK_VKRESULT(vkGetPipelineCacheData(pRenderer->mVk.pDevice, pPipelineCache->mVk.pCache, pSize, pData));
    }
}

#if defined(SHADER_STATS_AVAILABLE)
void addPipelineStats(Renderer* pRenderer, Pipeline* pPipeline, bool generateDisassembly, PipelineStats* pOutStats)
{
    memset(pOutStats, 0, sizeof(PipelineStats));
    if (!pRenderer->pGpu->mAMDShaderInfoExtension)
    {
        return;
    }

    for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
    {
        if (!(pPipeline->mVk.mShaderStages & (1 << i)))
        {
            continue;
        }
        VkShaderStageFlagBits     stage = (VkShaderStageFlagBits)util_to_vk_shader_stage_flags((ShaderStage)(1 << i));
        VkShaderStatisticsInfoAMD statsAMD = { 0 };
        size_t                    dataSize = sizeof(statsAMD);
        VkResult res = vkGetShaderInfoAMD(pRenderer->mVk.pDevice, pPipeline->mVk.pPipeline, stage, VK_SHADER_INFO_TYPE_STATISTICS_AMD,
                                          &dataSize, &statsAMD);
        if (VK_SUCCESS != res)
        {
            continue;
        }
        ShaderStats* stats = NULL;
        switch (stage)
        {
        case VK_SHADER_STAGE_VERTEX_BIT:
            stats = &pOutStats->mVert;
            break;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            stats = &pOutStats->mHull;
            break;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            stats = &pOutStats->mDomain;
            break;
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            stats = &pOutStats->mGeom;
            break;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            stats = &pOutStats->mFrag;
            break;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            stats = &pOutStats->mComp;
            break;
        default:
            break;
        }

        stats->mValid = true;
        stats->mUsedVgprs = statsAMD.resourceUsage.numUsedVgprs;
        stats->mUsedSgprs = statsAMD.resourceUsage.numUsedSgprs;
        stats->mLdsSizePerLocalWorkGroup = statsAMD.resourceUsage.ldsSizePerLocalWorkGroup;
        stats->mLdsUsageSizeInBytes = (uint32_t)statsAMD.resourceUsage.ldsUsageSizeInBytes;
        stats->mScratchMemUsageInBytes = (uint32_t)statsAMD.resourceUsage.scratchMemUsageInBytes;
        stats->mPhysicalVgprs = statsAMD.numPhysicalVgprs;
        stats->mPhysicalSgprs = statsAMD.numPhysicalSgprs;
        stats->mAvailableVgprs = statsAMD.numAvailableVgprs;
        stats->mAvailableSgprs = statsAMD.numAvailableSgprs;
        COMPILE_ASSERT(sizeof(stats->mComputeWorkGroupSize) == sizeof(statsAMD.computeWorkGroupSize));
        memcpy(stats->mComputeWorkGroupSize, statsAMD.computeWorkGroupSize, sizeof(statsAMD.computeWorkGroupSize));

        if (generateDisassembly)
        {
            dataSize = 0;
            res = vkGetShaderInfoAMD(pRenderer->mVk.pDevice, pPipeline->mVk.pPipeline, stage, VK_SHADER_INFO_TYPE_DISASSEMBLY_AMD,
                                     &dataSize, NULL);
            if (VK_SUCCESS != res)
            {
                continue;
            }
            stats->mVk.mDisassemblySize = (uint32_t)dataSize;
            stats->mVk.pDisassemblyAMD = tf_malloc(dataSize);
            vkGetShaderInfoAMD(pRenderer->mVk.pDevice, pPipeline->mVk.pPipeline, stage, VK_SHADER_INFO_TYPE_DISASSEMBLY_AMD, &dataSize,
                               stats->mVk.pDisassemblyAMD);
        }
    }
}

void removePipelineStats(Renderer* pRenderer, PipelineStats* pStats)
{
    if (!pRenderer->pGpu->mAMDShaderInfoExtension)
    {
        return;
    }

    ShaderStats* stats[] = {
        &pStats->mVert, &pStats->mHull, &pStats->mDomain, &pStats->mGeom, &pStats->mFrag, &pStats->mComp,
    };
    for (uint32_t i = 0; i < TF_ARRAY_COUNT(stats); ++i)
    {
        SAFE_FREE(stats[i]->mVk.pDisassemblyAMD);
    }
}
#endif
/************************************************************************/
// Command buffer functions
/************************************************************************/
void util_cache_sample_location_state(Cmd* pCmd, const SampleLocationDesc* pDesc, SampleCount sampleCount)
{
    uint32_t sampleLocationsCount = pDesc->mGridSizeX * pDesc->mGridSizeY * sampleCount;
    ASSERT(sampleLocationsCount <= MAX_SAMPLE_LOCATIONS);
    pCmd->mVk.mGridSizeX = pDesc->mGridSizeX;
    pCmd->mVk.mGridSizeY = pDesc->mGridSizeY;
    pCmd->mVk.mSampleCount = sampleCount;
    for (uint32_t i = 0; i < sampleLocationsCount; i++)
    {
        pCmd->mVk.mSampleLocations[i] = pDesc->pLocations[i];
    }
}

void resetCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
    ASSERT(pRenderer);
    ASSERT(pCmdPool);

    CHECK_VKRESULT(vkResetCommandPool(pRenderer->mVk.pDevice, pCmdPool->pCmdPool, 0));
}

void beginCmd(Cmd* pCmd)
{
    ASSERT(pCmd);
    ASSERT(VK_NULL_HANDLE != pCmd->mVk.pCmdBuf);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    VkDeviceGroupCommandBufferBeginInfoKHR deviceGroupBeginInfo = { VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO_KHR };
    deviceGroupBeginInfo.pNext = NULL;
    if (pCmd->pRenderer->mGpuMode == GPU_MODE_LINKED)
    {
        deviceGroupBeginInfo.deviceMask = (1 << pCmd->mVk.mNodeIndex);
        begin_info.pNext = &deviceGroupBeginInfo;
    }

    Renderer* pRenderer = pCmd->pRenderer;
    CHECK_VKRESULT(vkBeginCommandBuffer(pCmd->mVk.pCmdBuf, &begin_info));

    // Reset CPU side data
    pCmd->mVk.pBoundPipelineLayout = NULL;
    pCmd->mVk.mShouldRebindDescriptorSetsMask = 0;
    for (uint32_t i = 0; i < MAX_DESCRIPTOR_SETS; i++)
    {
        pCmd->mVk.mBoundDescriptorSetHashes[i] = 0;
    }
}

void endCmd(Cmd* pCmd)
{
    ASSERT(pCmd);
    ASSERT(VK_NULL_HANDLE != pCmd->mVk.pCmdBuf);

    if (pCmd->mVk.mIsRendering)
    {
        vkCmdEndRenderingKHR(pCmd->mVk.pCmdBuf);
        pCmd->mVk.mIsRendering = false;
    }
    else if (pCmd->mVk.pActiveRenderPass)
    {
        vkCmdEndRenderPass(pCmd->mVk.pCmdBuf);
        pCmd->mVk.pActiveRenderPass = VK_NULL_HANDLE;
    }

    Renderer* pRenderer = pCmd->pRenderer;
    CHECK_VKRESULT(vkEndCommandBuffer(pCmd->mVk.pCmdBuf));
}

void cmdBindRenderTargetsDynamic(Cmd* pCmd, const BindRenderTargetsDesc* pDesc)
{
    ASSERT(pCmd);
    ASSERT(VK_NULL_HANDLE != pCmd->mVk.pCmdBuf);

    if (pCmd->mVk.mIsRendering)
    {
        vkCmdEndRenderingKHR(pCmd->mVk.pCmdBuf);
        pCmd->mVk.mIsRendering = false;
    }

    if (!pDesc)
    {
        return;
    }

    SampleCount sampleCount = SAMPLE_COUNT_1;
    bool        vrFoveatedRendering = false;
    UNREF_PARAM(vrFoveatedRendering);
    VkRenderingAttachmentInfo colorAttachments[MAX_RENDER_TARGET_ATTACHMENTS] = { 0 };
    VkRenderingAttachmentInfo depthAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR };
    VkRenderingAttachmentInfo stencilAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR };
    const bool hasDepth = pDesc->mDepthStencil.pDepthStencil && TinyImageFormat_HasDepth(pDesc->mDepthStencil.pDepthStencil->mFormat);
    const bool hasStencil = pDesc->mDepthStencil.pDepthStencil && TinyImageFormat_HasStencil(pDesc->mDepthStencil.pDepthStencil->mFormat);
    const bool hasDepthOrStencil = hasDepth || hasStencil;
    VkMemoryBarrier      barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    VkPipelineStageFlags srcStageMask = { 0 };
    VkPipelineStageFlags dstStageMask = { 0 };

    for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i)
    {
        const BindRenderTargetDesc* desc = &pDesc->mRenderTargets[i];
        vrFoveatedRendering |= desc->pRenderTarget->mVRFoveatedRendering;
        const LoadActionType loadAction = desc->mLoadAction;
        sampleCount = desc->pRenderTarget->mSampleCount;
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
        StoreActionType storeAction;
        bool            resolveAttachment = IsStoreActionResolve(desc->mStoreAction);
        if (resolveAttachment)
        {
            storeAction = desc->pRenderTarget->pTexture->mLazilyAllocated ? STORE_ACTION_RESOLVE_DONTCARE : STORE_ACTION_DONTCARE;

            colorAttachments[i].resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachments[i].resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        }
        else
        {
            storeAction = desc->pRenderTarget->pTexture->mLazilyAllocated ? STORE_ACTION_DONTCARE : desc->mStoreAction;
        }
#else
        const StoreActionType storeAction = desc->pRenderTarget->pTexture->mLazilyAllocated ? STORE_ACTION_DONTCARE : desc->mStoreAction;
#endif

        colorAttachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        colorAttachments[i].pNext = NULL;
        colorAttachments[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachments[i].loadOp = gVkAttachmentLoadOpTranslator[loadAction];
        colorAttachments[i].storeOp = gVkAttachmentStoreOpTranslator[storeAction];

        if (!desc->mUseMipSlice && !desc->mUseArraySlice)
        {
            colorAttachments[i].imageView = desc->pRenderTarget->mVk.pDescriptor;

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
            if (resolveAttachment)
            {
                colorAttachments[i].resolveImageView = desc->pRenderTarget->pResolveAttachment->mVk.pDescriptor;
            }
#endif
        }
        else
        {
            uint32_t handle = 0;
            if (desc->mUseMipSlice)
            {
                if (desc->mUseArraySlice)
                {
                    handle = desc->mMipSlice * desc->pRenderTarget->mArraySize + desc->mArraySlice;
                }
                else
                {
                    handle = desc->mMipSlice;
                }
            }
            else if (desc->mUseArraySlice)
            {
                handle = desc->mArraySlice;
            }
            colorAttachments[i].imageView = desc->pRenderTarget->mVk.pSliceDescriptors[handle];

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
            if (resolveAttachment)
            {
                colorAttachments[i].resolveImageView = desc->pRenderTarget->pResolveAttachment->mVk.pSliceDescriptors[handle];
            }
#endif
        }
        const ClearValue* clearValue = desc->mOverrideClearValue ? &desc->mClearValue : &desc->pRenderTarget->mClearValue;
        colorAttachments[i].clearValue.color = (VkClearColorValue){ { clearValue->r, clearValue->g, clearValue->b, clearValue->a } };

        if (LOAD_ACTION_LOAD == loadAction)
        {
            barrier.srcAccessMask |= util_to_vk_access_flags(RESOURCE_STATE_RENDER_TARGET);
            barrier.dstAccessMask |= util_to_vk_access_flags(RESOURCE_STATE_RENDER_TARGET);
            srcStageMask |= ToPipelineStageFlags(pCmd->pRenderer, RESOURCE_STATE_RENDER_TARGET, QUEUE_TYPE_GRAPHICS);
            dstStageMask |= ToPipelineStageFlags(pCmd->pRenderer, RESOURCE_STATE_RENDER_TARGET, QUEUE_TYPE_GRAPHICS);
        }
    }
    if (hasDepthOrStencil)
    {
        const BindDepthTargetDesc* desc = &pDesc->mDepthStencil;
        sampleCount = desc->pDepthStencil->mSampleCount;
        vrFoveatedRendering |= desc->pDepthStencil->mVRFoveatedRendering;
        const LoadActionType depthLoadAction = desc->mLoadAction;
        const LoadActionType stencilLoadAction = desc->mLoadActionStencil;

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
        // Dynamic rendering allows depth stencil auto resolve
        StoreActionType depthStoreAction;
        StoreActionType stencilStoreAction;
        bool            resolveAttachment = IsStoreActionResolve(desc->mStoreAction) || IsStoreActionResolve(desc->mStoreActionStencil);
        if (resolveAttachment)
        {
            depthStoreAction = desc->pDepthStencil->pTexture->mLazilyAllocated ? STORE_ACTION_RESOLVE_DONTCARE : STORE_ACTION_DONTCARE;
            stencilStoreAction = desc->pDepthStencil->pTexture->mLazilyAllocated ? STORE_ACTION_RESOLVE_DONTCARE : STORE_ACTION_DONTCARE;
            depthAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
            stencilAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            stencilAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        }
        else
        {
            depthStoreAction = desc->pDepthStencil->pTexture->mLazilyAllocated ? STORE_ACTION_DONTCARE : desc->mStoreAction;
            stencilStoreAction = desc->pDepthStencil->pTexture->mLazilyAllocated ? STORE_ACTION_DONTCARE : desc->mStoreActionStencil;
        }
#else
        const StoreActionType depthStoreAction =
            desc->pDepthStencil->pTexture->mLazilyAllocated ? STORE_ACTION_DONTCARE : desc->mStoreAction;
        const StoreActionType stencilStoreAction =
            desc->pDepthStencil->pTexture->mLazilyAllocated ? STORE_ACTION_DONTCARE : desc->mStoreActionStencil;
#endif

        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = gVkAttachmentLoadOpTranslator[depthLoadAction];
        depthAttachment.storeOp = gVkAttachmentStoreOpTranslator[depthStoreAction];

        stencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        stencilAttachment.loadOp = gVkAttachmentLoadOpTranslator[stencilLoadAction];
        stencilAttachment.storeOp = gVkAttachmentStoreOpTranslator[stencilStoreAction];

        if (!desc->mUseMipSlice && !desc->mUseArraySlice)
        {
            depthAttachment.imageView = desc->pDepthStencil->mVk.pDescriptor;

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
            if (resolveAttachment)
            {
                depthAttachment.resolveImageView = desc->pDepthStencil->pResolveAttachment->mVk.pDescriptor;
            }
#endif
        }
        else
        {
            uint32_t handle = 0;
            if (desc->mUseMipSlice)
            {
                if (desc->mUseArraySlice)
                {
                    handle = desc->mMipSlice * desc->pDepthStencil->mArraySize + desc->mArraySlice;
                }
                else
                {
                    handle = desc->mMipSlice;
                }
            }
            else if (desc->mUseArraySlice)
            {
                handle = desc->mArraySlice;
            }
            depthAttachment.imageView = desc->pDepthStencil->mVk.pSliceDescriptors[handle];

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
            if (resolveAttachment)
            {
                depthAttachment.resolveImageView = desc->pDepthStencil->pResolveAttachment->mVk.pSliceDescriptors[handle];
            }
#endif
        }

        stencilAttachment.imageView = depthAttachment.imageView;
        stencilAttachment.resolveImageView = depthAttachment.resolveImageView;

        const ClearValue* clearValue = desc->mOverrideClearValue ? &desc->mClearValue : &desc->pDepthStencil->mClearValue;
        depthAttachment.clearValue.depthStencil = (VkClearDepthStencilValue){ clearValue->depth, clearValue->stencil };
        stencilAttachment.clearValue.depthStencil = (VkClearDepthStencilValue){ clearValue->depth, clearValue->stencil };

        if (LOAD_ACTION_LOAD == depthLoadAction || LOAD_ACTION_LOAD == stencilLoadAction)
        {
            barrier.srcAccessMask |= util_to_vk_access_flags(RESOURCE_STATE_DEPTH_WRITE);
            barrier.dstAccessMask |= util_to_vk_access_flags(RESOURCE_STATE_DEPTH_WRITE);
            srcStageMask |= ToPipelineStageFlags(pCmd->pRenderer, RESOURCE_STATE_DEPTH_WRITE, QUEUE_TYPE_GRAPHICS);
            dstStageMask |= ToPipelineStageFlags(pCmd->pRenderer, RESOURCE_STATE_DEPTH_WRITE, QUEUE_TYPE_GRAPHICS);
        }
    }

    if (srcStageMask || dstStageMask)
    {
        vkCmdPipelineBarrier(pCmd->mVk.pCmdBuf, srcStageMask, dstStageMask, 0, 1, &barrier, 0, NULL, 0, NULL);
    }

    VkRect2D renderArea = { 0 };
    renderArea.offset.x = 0;
    renderArea.offset.y = 0;

    uint32_t layerCount = 0;
    if (pDesc->mRenderTargetCount)
    {
        const BindRenderTargetDesc* desc = &pDesc->mRenderTargets[0];
        renderArea.extent.width = desc->pRenderTarget->mWidth >> (desc->mUseMipSlice ? desc->mMipSlice : 0);
        renderArea.extent.height = desc->pRenderTarget->mHeight >> (desc->mUseMipSlice ? desc->mMipSlice : 0);

        if (desc->mUseArraySlice)
        {
            layerCount = 1;
        }
        else
        {
            layerCount = desc->pRenderTarget->mVRMultiview ? 1 : desc->pRenderTarget->mArraySize;
        }
    }
    else if (hasDepthOrStencil)
    {
        const BindDepthTargetDesc* desc = &pDesc->mDepthStencil;
        renderArea.extent.width = desc->pDepthStencil->mWidth >> (desc->mUseMipSlice ? desc->mMipSlice : 0);
        renderArea.extent.height = desc->pDepthStencil->mHeight >> (desc->mUseMipSlice ? desc->mMipSlice : 0);

        if (desc->mUseArraySlice)
        {
            layerCount = 1;
        }
        else
        {
            layerCount = desc->pDepthStencil->mVRMultiview ? 1 : desc->pDepthStencil->mArraySize;
        }
    }
    else
    {
        // Empty render pass - Need to set layerCount to 1
        ASSERT(pDesc->mExtent[0] && pDesc->mExtent[1]);
        renderArea.extent = (VkExtent2D){ pDesc->mExtent[0], pDesc->mExtent[1] };
        renderArea.offset = (VkOffset2D){ 0 };
        layerCount = 1;
    }

    if (pDesc->mRenderTargetCount && pDesc->mRenderTargets[0].pRenderTarget->mDepth > 1)
    {
        layerCount = pDesc->mRenderTargets[0].pRenderTarget->mDepth;
    }
    else if (hasDepth && pDesc->mDepthStencil.pDepthStencil->mDepth > 1)
    {
        layerCount = pDesc->mDepthStencil.pDepthStencil->mDepth;
    }

    VkRenderingInfoKHR renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO_KHR };
    renderingInfo.pColorAttachments = colorAttachments;
    renderingInfo.colorAttachmentCount = pDesc->mRenderTargetCount;
    renderingInfo.pDepthAttachment = hasDepth ? &depthAttachment : NULL;
    renderingInfo.pStencilAttachment = hasStencil ? &stencilAttachment : NULL;
    renderingInfo.renderArea = renderArea;
    renderingInfo.layerCount = layerCount;

    vkCmdBeginRenderingKHR(pCmd->mVk.pCmdBuf, &renderingInfo);
    pCmd->mVk.mIsRendering = true;

    util_cache_sample_location_state(pCmd, &pDesc->mSampleLocation, sampleCount);
}

void cmdBindRenderTargets(Cmd* pCmd, const BindRenderTargetsDesc* pDesc)
{
    if (pCmd->pRenderer && pCmd->pRenderer->pGpu->mDynamicRenderingSupported)
    {
        cmdBindRenderTargetsDynamic(pCmd, pDesc);
        return;
    }
    ASSERT(pCmd);
    ASSERT(VK_NULL_HANDLE != pCmd->mVk.pCmdBuf);

    if (pCmd->mVk.pActiveRenderPass)
    {
        vkCmdEndRenderPass(pCmd->mVk.pCmdBuf);
        pCmd->mVk.pActiveRenderPass = VK_NULL_HANDLE;
    }

    if (!pDesc)
    {
        return;
    }

    const bool hasDepth = pDesc->mDepthStencil.pDepthStencil;

    size_t               renderPassHash = 0;
    size_t               frameBufferHash = 0;
    bool                 vrFoveatedRendering = false;
    StoreActionType      colorStoreAction[MAX_RENDER_TARGET_ATTACHMENTS] = { 0 };
    StoreActionType      depthStoreAction = { 0 };
    StoreActionType      stencilStoreAction = { 0 };
    uint32_t             frameBufferRenderTargetCount = 0;
    VkMemoryBarrier      barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    VkPipelineStageFlags srcStageMask = { 0 };
    VkPipelineStageFlags dstStageMask = { 0 };

    bool isDepthLazilyAllocated = false;

    // Generate hash for render pass and frame buffer
    // NOTE:
    // Render pass does not care about underlying VkImageView. It only cares about the format and sample count of the attachments.
    // We hash those two values to generate render pass hash
    // Frame buffer is the actual array of all the VkImageViews
    // We hash the texture id associated with the render target to generate frame buffer hash
    for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i)
    {
        const BindRenderTargetDesc* desc = &pDesc->mRenderTargets[i];
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
        bool resolveAttachment = IsStoreActionResolve(desc->mStoreAction);
        if (resolveAttachment)
        {
            colorStoreAction[i] = desc->pRenderTarget->pTexture->mLazilyAllocated ? STORE_ACTION_RESOLVE_DONTCARE : STORE_ACTION_DONTCARE;
        }
        else
#endif
        {
            colorStoreAction[i] = desc->pRenderTarget->pTexture->mLazilyAllocated ? STORE_ACTION_DONTCARE : desc->mStoreAction;
        }

        uint32_t renderPassHashValues[] = { (uint32_t)desc->pRenderTarget->mFormat,
                                            (uint32_t)desc->pRenderTarget->mSampleCount,
                                            (uint32_t)desc->mLoadAction,
                                            (uint32_t)colorStoreAction[i],
                                            (uint32_t)desc->pRenderTarget->pTexture->mArraySizeMinusOne,
                                            (uint32_t)desc->pRenderTarget->mVRFoveatedRendering };
        uint32_t frameBufferHashValues[] = {
            desc->pRenderTarget->mVk.mId,
            ((uint32_t)desc->mUseArraySlice << 1) | ((uint32_t)desc->mUseMipSlice),
            (uint32_t)desc->mArraySlice,
            (uint32_t)desc->mMipSlice,
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
            resolveAttachment ? desc->pRenderTarget->pResolveAttachment->mVk.mId : 0
#endif
        };
        renderPassHash = tf_mem_hash_uint8_t((const uint8_t*)renderPassHashValues, sizeof(renderPassHashValues), renderPassHash);
        frameBufferHash = tf_mem_hash_uint8_t((const uint8_t*)frameBufferHashValues, sizeof(frameBufferHashValues), frameBufferHash);
        vrFoveatedRendering |= desc->pRenderTarget->mVRFoveatedRendering;

        ++frameBufferRenderTargetCount;
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
        frameBufferRenderTargetCount += resolveAttachment ? 1 : 0;
#endif
        if (LOAD_ACTION_LOAD == desc->mLoadAction)
        {
            barrier.srcAccessMask |= util_to_vk_access_flags(RESOURCE_STATE_RENDER_TARGET);
            barrier.dstAccessMask |= util_to_vk_access_flags(RESOURCE_STATE_RENDER_TARGET);
            srcStageMask |= ToPipelineStageFlags(pCmd->pRenderer, RESOURCE_STATE_RENDER_TARGET, QUEUE_TYPE_GRAPHICS);
            dstStageMask |= ToPipelineStageFlags(pCmd->pRenderer, RESOURCE_STATE_RENDER_TARGET, QUEUE_TYPE_GRAPHICS);
        }
    }
    if (hasDepth)
    {
        const BindDepthTargetDesc* desc = &pDesc->mDepthStencil;
        isDepthLazilyAllocated = desc->pDepthStencil->pTexture->mLazilyAllocated;
        depthStoreAction = desc->pDepthStencil->pTexture->mLazilyAllocated ? STORE_ACTION_DONTCARE : desc->mStoreAction;
        stencilStoreAction = desc->pDepthStencil->pTexture->mLazilyAllocated ? STORE_ACTION_DONTCARE : desc->mStoreActionStencil;

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
        // Dont support depth stencil auto resolve
        ASSERT(!(IsStoreActionResolve(depthStoreAction) || IsStoreActionResolve(stencilStoreAction)));
#endif

        uint32_t renderPassHashValues[] = {
            (uint32_t)desc->pDepthStencil->mFormat,
            (uint32_t)desc->pDepthStencil->mSampleCount,
            (uint32_t)desc->mLoadAction,
            (uint32_t)desc->mLoadActionStencil,
            (uint32_t)depthStoreAction,
            (uint32_t)stencilStoreAction,
            (uint32_t)desc->pDepthStencil->mVRFoveatedRendering,
        };
        uint32_t frameBufferHashValues[] = {
            desc->pDepthStencil->mVk.mId,
            ((uint32_t)desc->mUseArraySlice << 1) | ((uint32_t)desc->mUseMipSlice),
            (uint32_t)desc->mArraySlice,
            (uint32_t)desc->mMipSlice,
        };
        renderPassHash = tf_mem_hash_uint8_t((const uint8_t*)renderPassHashValues, sizeof(renderPassHashValues), renderPassHash);
        frameBufferHash = tf_mem_hash_uint8_t((const uint8_t*)frameBufferHashValues, sizeof(frameBufferHashValues), frameBufferHash);
        vrFoveatedRendering |= desc->pDepthStencil->mVRFoveatedRendering;
        if (LOAD_ACTION_LOAD == desc->mLoadAction || LOAD_ACTION_LOAD == desc->mLoadActionStencil)
        {
            barrier.srcAccessMask |= util_to_vk_access_flags(RESOURCE_STATE_DEPTH_WRITE);
            barrier.dstAccessMask |= util_to_vk_access_flags(RESOURCE_STATE_DEPTH_WRITE);
            srcStageMask |= ToPipelineStageFlags(pCmd->pRenderer, RESOURCE_STATE_DEPTH_WRITE, QUEUE_TYPE_GRAPHICS);
            dstStageMask |= ToPipelineStageFlags(pCmd->pRenderer, RESOURCE_STATE_DEPTH_WRITE, QUEUE_TYPE_GRAPHICS);
        }
    }

    // NOTE: Some drivers can rearrange render passes if no dependency is added
    // Add output->output barrier to avoid this
    if (srcStageMask || dstStageMask)
    {
        vkCmdPipelineBarrier(pCmd->mVk.pCmdBuf, srcStageMask, dstStageMask, 0, 1, &barrier, 0, NULL, 0, NULL);
    }

    SampleCount sampleCount =
        hasDepth ? pDesc->mDepthStencil.pDepthStencil->mSampleCount : pDesc->mRenderTargets[0].pRenderTarget->mSampleCount;

    // Need pointer to pointer in order to reassign hash map when it is resized
    RenderPassNode**  pRenderPassMap = get_render_pass_map(pCmd->pRenderer->mUnlinkedRendererIndex);
    FrameBufferNode** pFrameBufferMap = get_frame_buffer_map(pCmd->pRenderer->mUnlinkedRendererIndex);

    RenderPassNode*  pNode = hmgetp_null(*pRenderPassMap, renderPassHash);
    FrameBufferNode* pFrameBufferNode = hmgetp_null(*pFrameBufferMap, frameBufferHash);

    // If a render pass of this combination already exists just use it or create a new one
    if (!pNode)
    {
        TinyImageFormat colorFormats[MAX_RENDER_TARGET_ATTACHMENTS] = { 0 };
        LoadActionType  colorLoadActions[MAX_RENDER_TARGET_ATTACHMENTS] = { 0 };
        TinyImageFormat depthStencilFormat = TinyImageFormat_UNDEFINED;
        bool            vrMultiview = false;
        for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i)
        {
            const BindRenderTargetDesc* desc = &pDesc->mRenderTargets[i];
            colorFormats[i] = desc->pRenderTarget->mFormat;
            colorLoadActions[i] = desc->mLoadAction;
            vrMultiview |= desc->pRenderTarget->mVRMultiview;
            sampleCount = desc->pRenderTarget->mSampleCount;
        }
        if (hasDepth)
        {
            const BindDepthTargetDesc* desc = &pDesc->mDepthStencil;
            depthStencilFormat = desc->pDepthStencil->mFormat;
            sampleCount = desc->pDepthStencil->mSampleCount;
            vrMultiview |= desc->pDepthStencil->mVRMultiview;
        }

        RenderPass     renderPass = { 0 };
        RenderPassDesc renderPassDesc = { 0 };
        renderPassDesc.mRenderTargetCount = pDesc->mRenderTargetCount;
        renderPassDesc.mSampleCount = sampleCount;
        renderPassDesc.pColorFormats = colorFormats;
        renderPassDesc.mDepthStencilFormat = depthStencilFormat;
        renderPassDesc.pLoadActionsColor = colorLoadActions;
        renderPassDesc.mLoadActionDepth = pDesc->mDepthStencil.mLoadAction;
        renderPassDesc.mLoadActionStencil = pDesc->mDepthStencil.mLoadActionStencil;
        renderPassDesc.pStoreActionsColor = colorStoreAction;
        renderPassDesc.mStoreActionDepth = depthStoreAction;
        renderPassDesc.mStoreActionStencil = stencilStoreAction;
        renderPassDesc.mVRMultiview = vrMultiview;
        renderPassDesc.mVRFoveatedRendering = vrFoveatedRendering;
        renderPassDesc.mDepthLazilyAllocated = isDepthLazilyAllocated;
        AddRenderPass(pCmd->pRenderer, &renderPassDesc, &renderPass);

        // No need of a lock here since this map is per thread
        hmput(*pRenderPassMap, renderPassHash, renderPass);

        pNode = hmgetp_null(*pRenderPassMap, renderPassHash);
    }

    RenderPass* pRenderPass = &pNode->value;

    // If a frame buffer of this combination already exists just use it or create a new one
    if (!pFrameBufferNode)
    {
        FrameBuffer frameBuffer = { 0 };
        AddFramebuffer(pCmd->pRenderer, pRenderPass->pRenderPass, pDesc, &frameBuffer);

        // No need of a lock here since this map is per thread
        hmput(*pFrameBufferMap, frameBufferHash, frameBuffer);

        pFrameBufferNode = hmgetp_null(*pFrameBufferMap, frameBufferHash);
    }

    FrameBuffer* pFrameBuffer = &pFrameBufferNode->value;

    VkRect2D renderArea = { 0 };
    renderArea.offset.x = 0;
    renderArea.offset.y = 0;
    renderArea.extent.width = pFrameBuffer->mWidth;
    renderArea.extent.height = pFrameBuffer->mHeight;

    uint32_t     clearValueCount = frameBufferRenderTargetCount;
    VkClearValue clearValues[VK_MAX_ATTACHMENT_ARRAY_COUNT] = { 0 };

    for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i)
    {
        const BindRenderTargetDesc* desc = &pDesc->mRenderTargets[i];
        const ClearValue*           clearValue = desc->mOverrideClearValue ? &desc->mClearValue : &desc->pRenderTarget->mClearValue;
        clearValues[i].color = (VkClearColorValue){ { clearValue->r, clearValue->g, clearValue->b, clearValue->a } };
    }
    if (hasDepth)
    {
        const BindDepthTargetDesc* desc = &pDesc->mDepthStencil;
        const ClearValue*          clearValue = desc->mOverrideClearValue ? &desc->mClearValue : &desc->pDepthStencil->mClearValue;
        clearValues[frameBufferRenderTargetCount].depthStencil = (VkClearDepthStencilValue){ clearValue->depth, clearValue->stencil };
        ++clearValueCount;
    }

    VkRenderPassBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = pRenderPass->pRenderPass,
        .framebuffer = pFrameBuffer->pFramebuffer,
        .renderArea = renderArea,
        .clearValueCount = clearValueCount,
        .pClearValues = clearValues,
    };

    vkCmdBeginRenderPass(pCmd->mVk.pCmdBuf, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    pCmd->mVk.pActiveRenderPass = pRenderPass->pRenderPass;

    util_cache_sample_location_state(pCmd, &pDesc->mSampleLocation, sampleCount);

#if defined(QUEST_VR)
    // changing render targets invalidates the desc sets on quest ?
    pCmd->mVk.mShouldRebindDescriptorSetsMask = MAX_DESCRIPTOR_SETS_MASK;
#endif
}

void cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
    ASSERT(pCmd);
    ASSERT(VK_NULL_HANDLE != pCmd->mVk.pCmdBuf);

    VkViewport viewport = {
        .x = x,
        .y = y + height,
        .width = width,
        .height = -height,
        .minDepth = minDepth,
        .maxDepth = maxDepth,
    };
    vkCmdSetViewport(pCmd->mVk.pCmdBuf, 0, 1, &viewport);
}

void cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    ASSERT(pCmd);
    ASSERT(VK_NULL_HANDLE != pCmd->mVk.pCmdBuf);

    VkRect2D rect = {
        .offset = (VkOffset2D){ x, y },
        .extent = (VkExtent2D){ width, height },
    };
    vkCmdSetScissor(pCmd->mVk.pCmdBuf, 0, 1, &rect);
}

void cmdSetStencilReferenceValue(Cmd* pCmd, uint32_t val)
{
    ASSERT(pCmd);

    vkCmdSetStencilReference(pCmd->mVk.pCmdBuf, VK_STENCIL_FRONT_AND_BACK, val);
}

void cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
    ASSERT(pCmd);
    ASSERT(pPipeline);
    ASSERT(pPipeline->mVk.pPipelineLayout);
    ASSERT(pCmd->mVk.pCmdBuf != VK_NULL_HANDLE);

    // if compute then draw have same layout, we need to rebind all sets.
    if ((uint32_t)pPipeline->mVk.mType != pCmd->mVk.mPipelineType)
    {
        pCmd->mVk.pBoundPipelineLayout = NULL;
    }
    pCmd->mVk.mPipelineType = pPipeline->mVk.mType;

    VkPipelineBindPoint bindPoint = gPipelineBindPoint[pPipeline->mVk.mType];
    vkCmdBindPipeline(pCmd->mVk.pCmdBuf, bindPoint, pPipeline->mVk.pPipeline);
    bool   isNewlayout = pCmd->mVk.pBoundPipelineLayout != pPipeline->mVk.pPipelineLayout->pLayout;
    /// layout changed, desc sets need to rebind
    size_t shouldRebindMask = isNewlayout ? 0xff : pCmd->mVk.mShouldRebindDescriptorSetsMask;

    if (pCmd->mVk.pBoundPipeline != pPipeline->mVk.pPipeline || isNewlayout)
    {
        pCmd->mVk.pBoundPipelineLayout = pPipeline->mVk.pPipelineLayout->pLayout;
        const PipelineLayout* layout = pPipeline->mVk.pPipelineLayout;

        // Vulkan requires to bind all descriptor sets upto the highest set number even if they are empty
        // Example: If shader uses only set 2, we still have to bind empty sets for set=0 and set=1
        for (uint32_t setIndex = 0; setIndex < layout->mLayoutCount; ++setIndex)
        {
            uint32_t currSetMask = 0x01 << setIndex;
            if (layout->mEmptyStaticSamplerLayout && setIndex == 0)
            {
                ASSERT(pCmd->pRenderer->mVk.pEmptyStaticSamplerDescriptorSet);
                vkCmdBindDescriptorSets(pCmd->mVk.pCmdBuf, bindPoint, layout->pLayout, setIndex, 1,
                                        &pCmd->pRenderer->mVk.pEmptyStaticSamplerDescriptorSet->mVk.pHandles[0], 0, NULL);
            }
            else if (layout->mEmptyLayouts & currSetMask)
            {
                vkCmdBindDescriptorSets(pCmd->mVk.pCmdBuf, bindPoint, layout->pLayout, setIndex, 1,
                                        &pCmd->pRenderer->mVk.pEmptyDescriptorSet, 0, NULL);
            }
            else if ((shouldRebindMask & currSetMask) &&
                     (pCmd->mVk.mBoundDescriptorSetHashes[setIndex] == pPipeline->mVk.pPipelineLayout->mDescSetsHashes[setIndex]))
            {
                cmdBindDescriptorSet(pCmd, pCmd->mVk.mBoundDescriptorSetIndices[setIndex], pCmd->mVk.mBoundDescriptorSets[setIndex]);
                pCmd->mVk.mShouldRebindDescriptorSetsMask ^= currSetMask;
            }
        }
    }

    pCmd->mVk.pBoundPipeline = pPipeline->mVk.pPipeline;

    uint32_t sampleLocationsCount = pCmd->mVk.mGridSizeX * pCmd->mVk.mGridSizeY * pCmd->mVk.mSampleCount;
    if (sampleLocationsCount)
    {
        VkSampleLocationEXT sampleLocations[MAX_SAMPLE_LOCATIONS] = { 0 };
        for (uint32_t i = 0; i < sampleLocationsCount; i++)
        {
            sampleLocations[i] = util_to_vk_locations(pCmd->mVk.mSampleLocations[i]);
        }
        VkSampleLocationsInfoEXT sampleLocationInfo = { 0 };
        sampleLocationInfo.sType = VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT;
        sampleLocationInfo.pSampleLocations = sampleLocations;
        sampleLocationInfo.sampleLocationsPerPixel = util_to_vk_sample_count(pCmd->pRenderer, (SampleCount)pCmd->mVk.mSampleCount);
        sampleLocationInfo.sampleLocationGridSize = (VkExtent2D){ pCmd->mVk.mGridSizeX, pCmd->mVk.mGridSizeY };
        sampleLocationInfo.sampleLocationsCount = sampleLocationsCount;
        vkCmdSetSampleLocationsEXT(pCmd->mVk.pCmdBuf, &sampleLocationInfo);
    }
}

void cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
    ASSERT(pCmd);
    ASSERT(pBuffer);
    ASSERT(VK_NULL_HANDLE != pCmd->mVk.pCmdBuf);

    VkIndexType vk_index_type = (INDEX_TYPE_UINT16 == indexType) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    vkCmdBindIndexBuffer(pCmd->mVk.pCmdBuf, pBuffer->mVk.pBuffer, offset, vk_index_type);
}

void cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
{
    UNREF_PARAM(pStrides);

    ASSERT(pCmd);
    ASSERT(0 != bufferCount);
    ASSERT(ppBuffers);
    ASSERT(VK_NULL_HANDLE != pCmd->mVk.pCmdBuf);
    ASSERT(pStrides);

    const uint32_t max_buffers = pCmd->pRenderer->pGpu->mVk.mGpuProperties.properties.limits.maxVertexInputBindings;
    uint32_t       capped_buffer_count = bufferCount > max_buffers ? max_buffers : bufferCount;

    // No upper bound for this, so use 64 for now
    ASSERT(capped_buffer_count < 64);

    VkBuffer     buffers[64] = { 0 };
    VkDeviceSize offsets[64] = { 0 };
    for (uint32_t i = 0; i < capped_buffer_count; ++i)
    {
        buffers[i] = ppBuffers[i]->mVk.pBuffer;
        offsets[i] = (pOffsets ? pOffsets[i] : 0);
    }

    vkCmdBindVertexBuffers(pCmd->mVk.pCmdBuf, 0, capped_buffer_count, buffers, offsets);
}

void cmdDraw(Cmd* pCmd, uint32_t vertex_count, uint32_t first_vertex)
{
    ASSERT(pCmd);
    ASSERT(VK_NULL_HANDLE != pCmd->mVk.pCmdBuf);

    vkCmdDraw(pCmd->mVk.pCmdBuf, vertex_count, 1, first_vertex, 0);
}

void cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
    ASSERT(pCmd);
    ASSERT(VK_NULL_HANDLE != pCmd->mVk.pCmdBuf);

    vkCmdDraw(pCmd->mVk.pCmdBuf, vertexCount, instanceCount, firstVertex, firstInstance);
}

void cmdDrawIndexed(Cmd* pCmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex)
{
    ASSERT(pCmd);
    ASSERT(VK_NULL_HANDLE != pCmd->mVk.pCmdBuf);

    vkCmdDrawIndexed(pCmd->mVk.pCmdBuf, index_count, 1, first_index, first_vertex, 0);
}

void cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex,
                             uint32_t firstInstance)
{
    ASSERT(pCmd);
    ASSERT(VK_NULL_HANDLE != pCmd->mVk.pCmdBuf);

    vkCmdDrawIndexed(pCmd->mVk.pCmdBuf, indexCount, instanceCount, firstIndex, firstVertex, firstInstance);
}

void cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    ASSERT(pCmd);
    ASSERT(pCmd->mVk.pCmdBuf != VK_NULL_HANDLE);

    vkCmdDispatch(pCmd->mVk.pCmdBuf, groupCountX, groupCountY, groupCountZ);
}

void cmdResourceBarrier(Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers,
                        TextureBarrier* pTextureBarriers, uint32_t numRtBarriers, RenderTargetBarrier* pRtBarriers)
{
    VkImageMemoryBarrier* imageBarriers =
        (numTextureBarriers + numRtBarriers)
            ? (VkImageMemoryBarrier*)alloca((numTextureBarriers + numRtBarriers) * sizeof(VkImageMemoryBarrier))
            : NULL;
    uint32_t imageBarrierCount = 0;

    VkMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };

    VkAccessFlags srcAccessFlags = 0;
    VkAccessFlags dstAccessFlags = 0;

    VkPipelineStageFlags srcStageMask = 0;
    VkPipelineStageFlags dstStageMask = 0;

    for (uint32_t i = 0; i < numBufferBarriers; ++i)
    {
        BufferBarrier* pTrans = &pBufferBarriers[i];

        if (RESOURCE_STATE_UNORDERED_ACCESS == pTrans->mCurrentState && RESOURCE_STATE_UNORDERED_ACCESS == pTrans->mNewState)
        {
            memoryBarrier.srcAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
            memoryBarrier.dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        }
        else
        {
            memoryBarrier.srcAccessMask |= util_to_vk_access_flags(pTrans->mCurrentState);
            memoryBarrier.dstAccessMask |= util_to_vk_access_flags(pTrans->mNewState);
        }

        srcAccessFlags |= memoryBarrier.srcAccessMask;
        dstAccessFlags |= memoryBarrier.dstAccessMask;
        srcStageMask |= ToPipelineStageFlags(pCmd->pRenderer, pTrans->mCurrentState, (QueueType)pCmd->mVk.mType);
        dstStageMask |= ToPipelineStageFlags(pCmd->pRenderer, pTrans->mNewState, (QueueType)pCmd->mVk.mType);
    }

    for (uint32_t i = 0; i < numTextureBarriers; ++i)
    {
        TextureBarrier*       pTrans = &pTextureBarriers[i];
        Texture*              pTexture = pTrans->pTexture;
        VkImageMemoryBarrier* pImageBarrier = NULL;

        if (RESOURCE_STATE_UNORDERED_ACCESS == pTrans->mCurrentState && RESOURCE_STATE_UNORDERED_ACCESS == pTrans->mNewState)
        {
            pImageBarrier = &imageBarriers[imageBarrierCount++];           //-V522
            pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; //-V522
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
            ASSERT(pImageBarrier->newLayout != VK_IMAGE_LAYOUT_UNDEFINED);
        }

        if (pImageBarrier)
        {
            pImageBarrier->image = pTexture->mVk.pImage;
            pImageBarrier->subresourceRange.aspectMask = (VkImageAspectFlags)pTexture->mAspectMask;
            pImageBarrier->subresourceRange.baseMipLevel = pTrans->mSubresourceBarrier ? pTrans->mMipLevel : 0;
            pImageBarrier->subresourceRange.levelCount = pTrans->mSubresourceBarrier ? 1 : VK_REMAINING_MIP_LEVELS;
            pImageBarrier->subresourceRange.baseArrayLayer = pTrans->mSubresourceBarrier ? pTrans->mArrayLayer : 0;
            pImageBarrier->subresourceRange.layerCount = pTrans->mSubresourceBarrier ? 1 : VK_REMAINING_ARRAY_LAYERS;

            if (pTrans->mAcquire && pTrans->mCurrentState != RESOURCE_STATE_UNDEFINED)
            {
                pImageBarrier->srcQueueFamilyIndex = pCmd->pRenderer->mVk.mQueueFamilyIndices[pTrans->mQueueType];
                pImageBarrier->dstQueueFamilyIndex = pCmd->pQueue->mVk.mQueueFamilyIndex;
            }
            else if (pTrans->mRelease && pTrans->mCurrentState != RESOURCE_STATE_UNDEFINED)
            {
                pImageBarrier->srcQueueFamilyIndex = pCmd->pQueue->mVk.mQueueFamilyIndex;
                pImageBarrier->dstQueueFamilyIndex = pCmd->pRenderer->mVk.mQueueFamilyIndices[pTrans->mQueueType];
            }
            else
            {
                pImageBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                pImageBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            }

            srcAccessFlags |= pImageBarrier->srcAccessMask;
            dstAccessFlags |= pImageBarrier->dstAccessMask;
            srcStageMask |= ToPipelineStageFlags(pCmd->pRenderer, pTrans->mCurrentState, (QueueType)pCmd->mVk.mType);
            dstStageMask |= ToPipelineStageFlags(pCmd->pRenderer, pTrans->mNewState, (QueueType)pCmd->mVk.mType);
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
            ASSERT(pImageBarrier->newLayout != VK_IMAGE_LAYOUT_UNDEFINED);
        }

        if (pImageBarrier)
        {
            pImageBarrier->image = pTexture->mVk.pImage;
            pImageBarrier->subresourceRange.aspectMask = (VkImageAspectFlags)pTexture->mAspectMask;
            pImageBarrier->subresourceRange.baseMipLevel = pTrans->mSubresourceBarrier ? pTrans->mMipLevel : 0;
            pImageBarrier->subresourceRange.levelCount = pTrans->mSubresourceBarrier ? 1 : VK_REMAINING_MIP_LEVELS;
            pImageBarrier->subresourceRange.baseArrayLayer = pTrans->mSubresourceBarrier ? pTrans->mArrayLayer : 0;
            pImageBarrier->subresourceRange.layerCount = pTrans->mSubresourceBarrier ? 1 : VK_REMAINING_ARRAY_LAYERS;

            if (pTrans->mAcquire && pTrans->mCurrentState != RESOURCE_STATE_UNDEFINED)
            {
                pImageBarrier->srcQueueFamilyIndex = pCmd->pRenderer->mVk.mQueueFamilyIndices[pTrans->mQueueType];
                pImageBarrier->dstQueueFamilyIndex = pCmd->pQueue->mVk.mQueueFamilyIndex;
            }
            else if (pTrans->mRelease && pTrans->mCurrentState != RESOURCE_STATE_UNDEFINED)
            {
                pImageBarrier->srcQueueFamilyIndex = pCmd->pQueue->mVk.mQueueFamilyIndex;
                pImageBarrier->dstQueueFamilyIndex = pCmd->pRenderer->mVk.mQueueFamilyIndices[pTrans->mQueueType];
            }
            else
            {
                pImageBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                pImageBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            }

            srcAccessFlags |= pImageBarrier->srcAccessMask;
            dstAccessFlags |= pImageBarrier->dstAccessMask;
            srcStageMask |= ToPipelineStageFlags(pCmd->pRenderer, pTrans->mCurrentState, (QueueType)pCmd->mVk.mType);
            dstStageMask |= ToPipelineStageFlags(pCmd->pRenderer, pTrans->mNewState, (QueueType)pCmd->mVk.mType);
        }
    }

    // NOTE: Some drivers can rearrange rendering if no dependency is added
    // Add output->output barrier to avoid this
    if (srcAccessFlags || dstAccessFlags)
    {
        vkCmdPipelineBarrier(pCmd->mVk.pCmdBuf, srcStageMask, dstStageMask, 0, memoryBarrier.srcAccessMask ? 1 : 0,
                             memoryBarrier.srcAccessMask ? &memoryBarrier : NULL, 0, NULL, imageBarrierCount, imageBarriers);
    }
}

void cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
    ASSERT(pCmd);
    ASSERT(pSrcBuffer);
    ASSERT(pSrcBuffer->mVk.pBuffer);
    ASSERT(pBuffer);
    ASSERT(pBuffer->mVk.pBuffer);
    ASSERT(srcOffset + size <= pSrcBuffer->mSize);
    ASSERT(dstOffset + size <= pBuffer->mSize);

    VkBufferCopy region = {
        .srcOffset = srcOffset,
        .dstOffset = dstOffset,
        .size = (VkDeviceSize)size,
    };
    vkCmdCopyBuffer(pCmd->mVk.pCmdBuf, pSrcBuffer->mVk.pBuffer, pBuffer->mVk.pBuffer, 1, &region);
}

typedef struct SubresourceDataDesc
{
    uint64_t mSrcOffset;
    uint32_t mMipLevel;
    uint32_t mArrayLayer;
    uint32_t mRowPitch;
    uint32_t mSlicePitch;
} SubresourceDataDesc;

void cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pSubresourceDesc)
{
    const TinyImageFormat fmt = (TinyImageFormat)pTexture->mFormat;
    const bool            isSinglePlane = TinyImageFormat_IsSinglePlane(fmt);

    if (isSinglePlane)
    {
        const uint32_t width = MAX(1, pTexture->mWidth >> pSubresourceDesc->mMipLevel);
        const uint32_t height = MAX(1, pTexture->mHeight >> pSubresourceDesc->mMipLevel);
        const uint32_t depth = MAX(1, pTexture->mDepth >> pSubresourceDesc->mMipLevel);
        const uint32_t numBlocksWide = pSubresourceDesc->mRowPitch / (TinyImageFormat_BitSizeOfBlock(fmt) >> 3);
        const uint32_t numBlocksHigh = (pSubresourceDesc->mSlicePitch / pSubresourceDesc->mRowPitch);

        VkBufferImageCopy copy = { 0 };
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

        vkCmdCopyBufferToImage(pCmd->mVk.pCmdBuf, pSrcBuffer->mVk.pBuffer, pTexture->mVk.pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
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
            VkBufferImageCopy* pCopy = &bufferImagesCopy[i];
            pCopy->bufferOffset = offset;
            pCopy->bufferRowLength = 0;
            pCopy->bufferImageHeight = 0;
            pCopy->imageSubresource.aspectMask = (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_PLANE_0_BIT << i);
            pCopy->imageSubresource.mipLevel = pSubresourceDesc->mMipLevel;
            pCopy->imageSubresource.baseArrayLayer = pSubresourceDesc->mArrayLayer;
            pCopy->imageSubresource.layerCount = 1;
            pCopy->imageOffset.x = 0;
            pCopy->imageOffset.y = 0;
            pCopy->imageOffset.z = 0;
            pCopy->imageExtent.width = TinyImageFormat_PlaneWidth(fmt, i, width);
            pCopy->imageExtent.height = TinyImageFormat_PlaneHeight(fmt, i, height);
            pCopy->imageExtent.depth = depth;
            offset += pCopy->imageExtent.width * pCopy->imageExtent.height * TinyImageFormat_PlaneSizeOfBlock(fmt, i);
        }

        vkCmdCopyBufferToImage(pCmd->mVk.pCmdBuf, pSrcBuffer->mVk.pBuffer, pTexture->mVk.pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               numOfPlanes, bufferImagesCopy);
    }
}

void cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const SubresourceDataDesc* pSubresourceDesc)
{
    const TinyImageFormat fmt = (TinyImageFormat)pTexture->mFormat;
    const bool            isSinglePlane = TinyImageFormat_IsSinglePlane(fmt);

    if (isSinglePlane)
    {
        const uint32_t width = MAX(1, pTexture->mWidth >> pSubresourceDesc->mMipLevel);
        const uint32_t height = MAX(1, pTexture->mHeight >> pSubresourceDesc->mMipLevel);
        const uint32_t depth = MAX(1, pTexture->mDepth >> pSubresourceDesc->mMipLevel);
        const uint32_t numBlocksWide = pSubresourceDesc->mRowPitch / (TinyImageFormat_BitSizeOfBlock(fmt) >> 3);
        const uint32_t numBlocksHigh = (pSubresourceDesc->mSlicePitch / pSubresourceDesc->mRowPitch);

        VkBufferImageCopy copy = { 0 };
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

        vkCmdCopyImageToBuffer(pCmd->mVk.pCmdBuf, pTexture->mVk.pImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pDstBuffer->mVk.pBuffer, 1,
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
            VkBufferImageCopy* pCopy = &bufferImagesCopy[i];
            pCopy->bufferOffset = offset;
            pCopy->bufferRowLength = 0;
            pCopy->bufferImageHeight = 0;
            pCopy->imageSubresource.aspectMask = (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_PLANE_0_BIT << i);
            pCopy->imageSubresource.mipLevel = pSubresourceDesc->mMipLevel;
            pCopy->imageSubresource.baseArrayLayer = pSubresourceDesc->mArrayLayer;
            pCopy->imageSubresource.layerCount = 1;
            pCopy->imageOffset.x = 0;
            pCopy->imageOffset.y = 0;
            pCopy->imageOffset.z = 0;
            pCopy->imageExtent.width = TinyImageFormat_PlaneWidth(fmt, i, width);
            pCopy->imageExtent.height = TinyImageFormat_PlaneHeight(fmt, i, height);
            pCopy->imageExtent.depth = depth;
            offset += pCopy->imageExtent.width * pCopy->imageExtent.height * TinyImageFormat_PlaneSizeOfBlock(fmt, i);
        }

        vkCmdCopyImageToBuffer(pCmd->mVk.pCmdBuf, pTexture->mVk.pImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pDstBuffer->mVk.pBuffer,
                               numOfPlanes, bufferImagesCopy);
    }
}
/************************************************************************/
// Queue Fence Semaphore Functions
/************************************************************************/
void acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex)
{
    ASSERT(pRenderer);
    ASSERT(VK_NULL_HANDLE != pRenderer->mVk.pDevice);

#if defined(QUEST_VR)
    BeginOpenXRFrame();
    AcquireOpenXRSwapchainImage(pSwapChain->mVR.pSwapchain, pImageIndex);
    pRenderer->mVR.pCurrentFDM = pSwapChain->mVR.ppFoveationFragmentDensityMaps[*pImageIndex];
    SwapChain* p2DLayerSwapchain = pSwapChain->mVR.m2DLayer.pSwapchain;
    if (p2DLayerSwapchain != NULL)
    {
        AcquireOpenXRSwapchainImage(p2DLayerSwapchain->mVR.pSwapchain, &pSwapChain->mVR.m2DLayer.mCurrentSwapChainIndex);
    }
    return;
#else
    ASSERT(VK_NULL_HANDLE != pSwapChain->mVk.pSwapChain);
    ASSERT(pSignalSemaphore || pFence);
#endif

    VkResult vk_res = VK_SUCCESS;

    if (pFence != NULL)
    {
        vk_res = vkAcquireNextImageKHR(pRenderer->mVk.pDevice, pSwapChain->mVk.pSwapChain, UINT64_MAX, VK_NULL_HANDLE, pFence->mVk.pFence,
                                       pImageIndex);

        // If swapchain is out of date, let caller know by setting image index to -1
        if (vk_res == VK_ERROR_OUT_OF_DATE_KHR)
        {
            *pImageIndex = (uint32_t)-1;
            vkResetFences(pRenderer->mVk.pDevice, 1, &pFence->mVk.pFence);
            return;
        }

        if (vk_res == VK_ERROR_DEVICE_LOST)
        {
            OnVkDeviceLost(pRenderer);
        }
    }
    else
    {
        vk_res = vkAcquireNextImageKHR(pRenderer->mVk.pDevice, pSwapChain->mVk.pSwapChain, UINT64_MAX, pSignalSemaphore->mVk.pSemaphore,
                                       VK_NULL_HANDLE, pImageIndex); //-V522

        // If swapchain is out of date, let caller know by setting image index to -1
        if (vk_res == VK_ERROR_OUT_OF_DATE_KHR)
        {
            *pImageIndex = (uint32_t)-1;
            pSignalSemaphore->mVk.mSignaled = false;
            return;
        }

        // Commonly returned immediately following swapchain resize.
        // Vulkan spec states that this return value constitutes a successful call to vkAcquireNextImageKHR
        // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkAcquireNextImageKHR.html
        if (vk_res == VK_SUBOPTIMAL_KHR)
        {
            LOGF(eINFO, "vkAcquireNextImageKHR returned VK_SUBOPTIMAL_KHR. If window was just resized, ignore this message.");
            pSignalSemaphore->mVk.mSignaled = true;
            return;
        }

        CHECK_VKRESULT(vk_res);
        pSignalSemaphore->mVk.mSignaled = true;
    }
}

void queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc)
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

    ASSERT(VK_NULL_HANDLE != pQueue->mVk.pQueue);

    VkCommandBuffer* cmds = (VkCommandBuffer*)alloca(cmdCount * sizeof(VkCommandBuffer));
    for (uint32_t i = 0; i < cmdCount; ++i)
    {
        cmds[i] = ppCmds[i]->mVk.pCmdBuf;
    }

    VkSemaphore*          wait_semaphores = waitSemaphoreCount ? (VkSemaphore*)alloca(waitSemaphoreCount * sizeof(VkSemaphore)) : NULL;
    VkPipelineStageFlags* wait_masks = (VkPipelineStageFlags*)alloca(waitSemaphoreCount * sizeof(VkPipelineStageFlags));
    uint32_t              waitCount = 0;
    for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
    {
        if (ppWaitSemaphores[i]->mVk.mSignaled)
        {
            wait_semaphores[waitCount] = ppWaitSemaphores[i]->mVk.pSemaphore; //-V522
            wait_masks[waitCount] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            ++waitCount;

            ppWaitSemaphores[i]->mVk.mSignaled = false;
        }
    }

    VkSemaphore* signal_semaphores = signalSemaphoreCount ? (VkSemaphore*)alloca(signalSemaphoreCount * sizeof(VkSemaphore)) : NULL;
    uint32_t     signalCount = 0;
    for (uint32_t i = 0; i < signalSemaphoreCount; ++i)
    {
        if (!ppSignalSemaphores[i]->mVk.mSignaled)
        {
            signal_semaphores[signalCount] = ppSignalSemaphores[i]->mVk.pSemaphore; //-V522
            ppSignalSemaphores[i]->mVk.mCurrentNodeIndex = pQueue->mNodeIndex;
            ppSignalSemaphores[i]->mVk.mSignaled = true;
            ++signalCount;
        }
    }

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = waitCount,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_masks,
        .commandBufferCount = cmdCount,
        .pCommandBuffers = cmds,
        .signalSemaphoreCount = signalCount,
        .pSignalSemaphores = signal_semaphores,
    };

    VkDeviceGroupSubmitInfo deviceGroupSubmitInfo = { VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO_KHR };
    if (pQueue->mVk.mGpuMode == GPU_MODE_LINKED)
    {
        uint32_t* pDeviceMasks = NULL;
        uint32_t* pSignalIndices = NULL;
        uint32_t* pWaitIndices = NULL;
        deviceGroupSubmitInfo.pNext = NULL;
        deviceGroupSubmitInfo.commandBufferCount = submit_info.commandBufferCount;
        deviceGroupSubmitInfo.signalSemaphoreCount = submit_info.signalSemaphoreCount;
        deviceGroupSubmitInfo.waitSemaphoreCount = submit_info.waitSemaphoreCount;

        pDeviceMasks = (uint32_t*)alloca(deviceGroupSubmitInfo.commandBufferCount * sizeof(uint32_t));
        pSignalIndices = (uint32_t*)alloca(deviceGroupSubmitInfo.signalSemaphoreCount * sizeof(uint32_t));
        pWaitIndices = (uint32_t*)alloca(deviceGroupSubmitInfo.waitSemaphoreCount * sizeof(uint32_t));

        for (uint32_t i = 0; i < deviceGroupSubmitInfo.commandBufferCount; ++i)
        {
            pDeviceMasks[i] = (1 << ppCmds[i]->mVk.mNodeIndex);
        }
        for (uint32_t i = 0; i < deviceGroupSubmitInfo.signalSemaphoreCount; ++i)
        {
            pSignalIndices[i] = pQueue->mNodeIndex;
        }
        for (uint32_t i = 0; i < deviceGroupSubmitInfo.waitSemaphoreCount; ++i)
        {
            pWaitIndices[i] = ppWaitSemaphores[i]->mVk.mCurrentNodeIndex;
        }

        deviceGroupSubmitInfo.pCommandBufferDeviceMasks = pDeviceMasks;
        deviceGroupSubmitInfo.pSignalSemaphoreDeviceIndices = pSignalIndices;
        deviceGroupSubmitInfo.pWaitSemaphoreDeviceIndices = pWaitIndices;
        submit_info.pNext = &deviceGroupSubmitInfo;
    }

    // Lock to make sure multiple threads dont use the same queue simultaneously
    // Many setups have just one queue family and one queue. In this case, async compute, async transfer doesn't exist and we end up using
    // the same queue for all three operations
    acquireMutex(pQueue->mVk.pSubmitMutex);
    Renderer* pRenderer = pQueue->mVk.pRenderer;
    if (pFence)
    {
        // Validate that fence is not pending before we submit it
        CHECK_VKRESULT(vkGetFenceStatus(pRenderer->mVk.pDevice, pFence->mVk.pFence));
        CHECK_VKRESULT(vkResetFences(pRenderer->mVk.pDevice, 1, &pFence->mVk.pFence));
    }

    CHECK_VKRESULT(vkQueueSubmit(pQueue->mVk.pQueue, 1, &submit_info, pFence ? pFence->mVk.pFence : VK_NULL_HANDLE));
    releaseMutex(pQueue->mVk.pSubmitMutex);
}

void queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc)
{
    ASSERT(pDesc);

    uint32_t    waitSemaphoreCount = pDesc->mWaitSemaphoreCount;
    Semaphore** ppWaitSemaphores = pDesc->ppWaitSemaphores;
    if (pDesc->pSwapChain)
    {
        SwapChain* pSwapChain = pDesc->pSwapChain;
#if defined(AUTOMATED_TESTING)
        if (isScreenshotCaptureRequested())
        {
            ScreenshotDesc desc = { 0 };
            desc.pRenderTarget = pSwapChain->ppRenderTargets[pDesc->mIndex];
            desc.ppWaitSemaphores = pDesc->ppWaitSemaphores;
            desc.mWaitSemaphoresCount = pDesc->mWaitSemaphoreCount;
            desc.mColorSpace = pSwapChain->mColorSpace;
            desc.discardAlpha = true;
            desc.flipRedBlue = false;
            captureScreenshot(&desc);

#if defined(QUEST_VR)
            // When taking automated screenshots on Quest, we need to include a separate one for the 2D compositor layer.
            SwapChain* p2DLayerSwapChain = pSwapChain->mVR.m2DLayer.pSwapchain;
            if (p2DLayerSwapChain != NULL)
            {
                char* pScreenshotName = getScreenshotName();
                strncat(pScreenshotName, "_UI", FS_MAX_PATH);
                setScreenshotName(pScreenshotName);

                desc.pRenderTarget = p2DLayerSwapChain->ppRenderTargets[pSwapChain->mVR.m2DLayer.mCurrentSwapChainIndex];
                desc.ppWaitSemaphores = pDesc->ppWaitSemaphores;
                desc.mWaitSemaphoresCount = pDesc->mWaitSemaphoreCount;
                desc.mColorSpace = p2DLayerSwapChain->mColorSpace;
                desc.discardAlpha = true;
                desc.flipRedBlue = false;
                captureScreenshot(&desc);
            }
#endif
        }
#endif

#if defined(QUEST_VR)
        OpenXRVKQueuePresent(pDesc);
        return;
#endif // QUEST_VR

        ASSERT(pQueue);
        if (waitSemaphoreCount > 0)
        {
            ASSERT(ppWaitSemaphores);
        }

        ASSERT(VK_NULL_HANDLE != pQueue->mVk.pQueue);

        VkSemaphore* wait_semaphores = waitSemaphoreCount ? (VkSemaphore*)alloca(waitSemaphoreCount * sizeof(VkSemaphore)) : NULL;
        uint32_t     waitCount = 0;
        for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
        {
            if (ppWaitSemaphores[i]->mVk.mSignaled)
            {
                wait_semaphores[waitCount] = ppWaitSemaphores[i]->mVk.pSemaphore; //-V522
                ppWaitSemaphores[i]->mVk.mSignaled = false;
                ++waitCount;
            }
        }

        uint32_t presentIndex = pDesc->mIndex;

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = waitCount,
            .pWaitSemaphores = wait_semaphores,
            .swapchainCount = 1,
            .pSwapchains = &(pSwapChain->mVk.pSwapChain),
            .pImageIndices = &(presentIndex),
        };

        // Lightweight lock to make sure multiple threads dont use the same queue simultaneously
        acquireMutex(pQueue->mVk.pSubmitMutex);

        VkResult vk_res;
#if defined(GFX_ENABLE_SWAPPY)
        if (gSwappyEnabled)
        {
            vk_res =
                SwappyVk_queuePresent(pSwapChain->mVk.pPresentQueue ? pSwapChain->mVk.pPresentQueue : pQueue->mVk.pQueue, &present_info);
        }
        else
#endif
        {
            vk_res = vkQueuePresentKHR(pSwapChain->mVk.pPresentQueue ? pSwapChain->mVk.pPresentQueue : pQueue->mVk.pQueue, &present_info);
        }

        if (vk_res == VK_ERROR_DEVICE_LOST)
        {
            OnVkDeviceLost(pQueue->mVk.pRenderer);
            // Will crash normally on Android.
#if defined(_WINDOWS)
            threadSleep(5000); // Wait for a few seconds to allow the driver to come back online before doing a reset.
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

        releaseMutex(pQueue->mVk.pSubmitMutex);
    }
}

void waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
{
    ASSERT(pRenderer);
    ASSERT(fenceCount);
    ASSERT(ppFences);

    VkFence* fences = (VkFence*)alloca(fenceCount * sizeof(VkFence));
    uint32_t numValidFences = 0;
    for (uint32_t i = 0; i < fenceCount; ++i)
    {
        fences[numValidFences++] = ppFences[i]->mVk.pFence;
    }

    if (numValidFences)
    {
        CHECK_VKRESULT(vkWaitForFences(pRenderer->mVk.pDevice, numValidFences, fences, VK_TRUE, UINT64_MAX));
    }
}

void waitQueueIdle(Queue* pQueue)
{
    acquireMutex(pQueue->mVk.pSubmitMutex);

    Renderer* pRenderer = pQueue->mVk.pRenderer;
    CHECK_VKRESULT(vkQueueWaitIdle(pQueue->mVk.pQueue));
#if defined(_WINDOWS)
    // #NOTE: In unlinked mode, vkQueueWaitIdle is not enough to ensure GPU idle.
    // vkDeviceWaitIdle call is needed otherwise validation complains about objects in flight being destroyed
    if (GPU_MODE_UNLINKED == pQueue->mVk.mGpuMode)
    {
        CHECK_VKRESULT(vkDeviceWaitIdle(pRenderer->mVk.pDevice));
    }
#endif
    releaseMutex(pQueue->mVk.pSubmitMutex);
}

void getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
    VkResult vkRes = vkGetFenceStatus(pRenderer->mVk.pDevice, pFence->mVk.pFence);
    *pFenceStatus = vkRes == VK_SUCCESS ? FENCE_STATUS_COMPLETE : FENCE_STATUS_INCOMPLETE;
}

/************************************************************************/
// Utility functions
/************************************************************************/

VkColorSpaceKHR util_to_vk_colorspace(ColorSpace colorSpace)
{
    switch (colorSpace)
    {
    case COLOR_SPACE_SDR_LINEAR:
    case COLOR_SPACE_SDR_SRGB:
        return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    case COLOR_SPACE_P2020:
        return VK_COLOR_SPACE_HDR10_ST2084_EXT;
    case COLOR_SPACE_EXTENDED_SRGB:
        return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
    default:
        break;
    }
    LOGF(eERROR, "Color Space (%u) not supported for creating swapchain buffer", (uint32_t)colorSpace);

    return VK_COLOR_SPACE_MAX_ENUM_KHR;
}

ColorSpace util_from_vk_colorspace(VkColorSpaceKHR colorSpace)
{
    switch (colorSpace)
    {
    case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
        return COLOR_SPACE_SDR_SRGB;
    case VK_COLOR_SPACE_HDR10_ST2084_EXT:
        return COLOR_SPACE_P2020;
    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
        return COLOR_SPACE_EXTENDED_SRGB;
    default:
        break;
    }
    return COLOR_SPACE_SDR_LINEAR;
}

TinyImageFormat getSupportedSwapchainFormat(Renderer* pRenderer, const SwapChainDesc* pDesc, ColorSpace colorSpace)
{
    // TODO: figure out this properly. BGRA not supported on android
#if !defined(VK_USE_PLATFORM_ANDROID_KHR) && !defined(VK_USE_PLATFORM_VI_NN)
    if (COLOR_SPACE_SDR_LINEAR == colorSpace)
        return TinyImageFormat_B8G8R8A8_UNORM;
    else if (COLOR_SPACE_SDR_SRGB == colorSpace)
        return TinyImageFormat_B8G8R8A8_SRGB;
#else
    if (COLOR_SPACE_SDR_LINEAR == colorSpace)
        return TinyImageFormat_R8G8B8A8_UNORM;
    else if (COLOR_SPACE_SDR_SRGB == colorSpace)
        return TinyImageFormat_R8G8B8A8_SRGB;
#endif

    VkSurfaceKHR vkSurface;
    CreateSurface(pRenderer, pDesc->mWindowHandle, &vkSurface);

    // Get surface formats
    VkSurfaceFormatKHR* formats = NULL;
    uint32_t            surfaceFormatCount = 0;
    CHECK_VKRESULT_INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->pGpu->mVk.pGpu, vkSurface, &surfaceFormatCount, NULL));
    formats = (VkSurfaceFormatKHR*)alloca(surfaceFormatCount * sizeof(*formats));
    CHECK_VKRESULT_INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->pGpu->mVk.pGpu, vkSurface, &surfaceFormatCount, formats));

    VkColorSpaceKHR requested_color_space = util_to_vk_colorspace(colorSpace);
    TinyImageFormat format = TinyImageFormat_UNDEFINED;

    for (uint32_t i = 0; i < surfaceFormatCount; ++i)
    {
        if (requested_color_space == formats[i].colorSpace)
        {
            format = TinyImageFormat_FromVkFormat((TinyImageFormat_VkFormat)formats[i].format);
            break;
        }
    }

    vkDestroySurfaceKHR(pRenderer->pContext->mVk.pInstance, vkSurface, GetAllocationCallbacks(VK_OBJECT_TYPE_SURFACE_KHR));

    return format;
}

uint32_t getRecommendedSwapchainImageCount(Renderer* pRenderer, const WindowHandle* hwnd)
{
#if defined(NX64) || defined(ANDROID)
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(hwnd);
    return 3;
#else
    VkSurfaceKHR surface;
    CreateSurface(pRenderer, *hwnd, &surface);
    VkSurfaceCapabilitiesKHR surfaceCaps = { 0 };
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pRenderer->pGpu->mVk.pGpu, surface, &surfaceCaps);
    uint32_t imageCount = surfaceCaps.minImageCount + 1;
    imageCount = MIN(surfaceCaps.maxImageCount ? surfaceCaps.maxImageCount : UINT32_MAX, imageCount);
    vkDestroySurfaceKHR(pRenderer->pContext->mVk.pInstance, surface, GetAllocationCallbacks(VK_OBJECT_TYPE_SURFACE_KHR));
    return imageCount;
#endif
}
/************************************************************************/
// Indirect draw functions
/************************************************************************/
void cmdExecuteIndirect(Cmd* pCmd, IndirectArgumentType type, uint32_t maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
                        Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
    static const uint32_t cmdStrides[3] = {
        sizeof(IndirectDrawArguments),
        sizeof(IndirectDrawIndexArguments),
        sizeof(IndirectDispatchArguments),
    };
    const uint32_t                      stride = cmdStrides[type];
    const PFN_vkCmdDrawIndirect         drawIndirect = (type == INDIRECT_DRAW) ? vkCmdDrawIndirect : vkCmdDrawIndexedIndirect;
    const PFN_vkCmdDrawIndirectCountKHR drawIndirectCount =
        (type == INDIRECT_DRAW) ? pfnVkCmdDrawIndirectCountKHR : pfnVkCmdDrawIndexedIndirectCountKHR;

    if (type == INDIRECT_DRAW || type == INDIRECT_DRAW_INDEX)
    {
        if (pCmd->pRenderer->pGpu->mMultiDrawIndirect)
        {
            if (pCounterBuffer && drawIndirectCount)
            {
                drawIndirectCount(pCmd->mVk.pCmdBuf, pIndirectBuffer->mVk.pBuffer, bufferOffset, pCounterBuffer->mVk.pBuffer,
                                  counterBufferOffset, maxCommandCount, stride);
            }
            else
            {
                drawIndirect(pCmd->mVk.pCmdBuf, pIndirectBuffer->mVk.pBuffer, bufferOffset, maxCommandCount, stride);
            }
        }
        else
        {
            // Cannot use counter buffer when MDI is not supported. We will blindly loop through maxCommandCount
            for (uint32_t cmd = 0; cmd < maxCommandCount; ++cmd)
            {
                drawIndirect(pCmd->mVk.pCmdBuf, pIndirectBuffer->mVk.pBuffer, bufferOffset + cmd * stride, 1, cmdStrides[type]);
            }
        }
    }
    else if (type == INDIRECT_DISPATCH)
    {
        for (uint32_t i = 0; i < maxCommandCount; ++i)
        {
            vkCmdDispatchIndirect(pCmd->mVk.pCmdBuf, pIndirectBuffer->mVk.pBuffer, bufferOffset + i * stride);
        }
    }
}
/************************************************************************/
// Query Heap Implementation
/************************************************************************/
static inline FORGE_CONSTEXPR VkQueryType ToVkQueryType(QueryType type)
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
        return VK_QUERY_TYPE_MAX_ENUM;
    }
}

// All flags used by DX12 (11 fields)
static const FORGE_CONSTEXPR VkQueryPipelineStatisticFlags gPipelineStatsFlags =
    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT | VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
    VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
    VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
    VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT | VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
    VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
    VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

static inline FORGE_CONSTEXPR uint32_t ToQueryWidth(QueryType type)
{
    switch (type)
    {
    case QUERY_TYPE_PIPELINE_STATISTICS:
        return count_bits(gPipelineStatsFlags) * sizeof(uint64_t);
    default:
        return sizeof(uint64_t);
    }
}

void getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
    ASSERT(pQueue);
    ASSERT(pFrequency);

    // The framework is using ticks per sec as frequency. Vulkan is nano sec per tick.
    // Handle the conversion logic here.
    *pFrequency =
        1.0f / ((double)pQueue->mVk.mTimestampPeriod /*ns/tick number of nanoseconds required for a timestamp query to be incremented by 1*/
                * 1e-9);                             // convert to ticks/sec (DX12 standard)
}

void initQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppQueryPool);

    QueryPool* pQueryPool = (QueryPool*)tf_calloc(1, sizeof(QueryPool));
    ASSERT(ppQueryPool);

    const uint32_t queryCount = pDesc->mQueryCount * (QUERY_TYPE_TIMESTAMP == pDesc->mType ? 2 : 1);

    pQueryPool->mVk.mType = ToVkQueryType(pDesc->mType);
    pQueryPool->mCount = queryCount;
    pQueryPool->mStride = ToQueryWidth(pDesc->mType);
    pQueryPool->mVk.mNodeIndex = pDesc->mNodeIndex;

    VkQueryPipelineStatisticFlags pipelineStatsFlags = gPipelineStatsFlags;
    if (QUERY_TYPE_PIPELINE_STATISTICS == pDesc->mType && !pRenderer->pGpu->mTessellationSupported)
    {
        pipelineStatsFlags &= ~VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT;
        pipelineStatsFlags &= ~VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT;
        pQueryPool->mStride = count_bits(pipelineStatsFlags) * sizeof(uint64_t);
    }

    VkQueryPoolCreateInfo createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.queryCount = queryCount;
    createInfo.queryType = pQueryPool->mVk.mType;
    createInfo.flags = 0;
    createInfo.pipelineStatistics = VK_QUERY_TYPE_PIPELINE_STATISTICS == pQueryPool->mVk.mType ? pipelineStatsFlags : 0;
    CHECK_VKRESULT(vkCreateQueryPool(pRenderer->mVk.pDevice, &createInfo, GetAllocationCallbacks(VK_OBJECT_TYPE_QUERY_POOL),
                                     &pQueryPool->mVk.pQueryPool));
    SetVkObjectName(pRenderer, (uint64_t)pQueryPool->mVk.pQueryPool, VK_OBJECT_TYPE_QUERY_POOL, VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT,
                    pDesc->pName);
    ResetQueryPool(pRenderer, pQueryPool);

    *ppQueryPool = pQueryPool;
}

void exitQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
    ASSERT(pRenderer);
    ASSERT(pQueryPool);
    vkDestroyQueryPool(pRenderer->mVk.pDevice, pQueryPool->mVk.pQueryPool, GetAllocationCallbacks(VK_OBJECT_TYPE_QUERY_POOL));

    SAFE_FREE(pQueryPool);
}

void cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    const VkQueryType type = pQueryPool->mVk.mType;
    switch (type)
    {
    case VK_QUERY_TYPE_TIMESTAMP:
    {
        const uint32_t index = pQuery->mIndex * 2;
        vkCmdWriteTimestamp(pCmd->mVk.pCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pQueryPool->mVk.pQueryPool, index);
        break;
    }
    case VK_QUERY_TYPE_OCCLUSION:
    case VK_QUERY_TYPE_PIPELINE_STATISTICS:
    {
        const uint32_t index = pQuery->mIndex;
        vkCmdBeginQuery(pCmd->mVk.pCmdBuf, pQueryPool->mVk.pQueryPool, index, 0);
        break;
    }
    default:
        ASSERT(false && "Not implemented");
    }
}

void cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    const VkQueryType type = pQueryPool->mVk.mType;
    switch (type)
    {
    case VK_QUERY_TYPE_TIMESTAMP:
    {
        const uint32_t index = pQuery->mIndex * 2 + 1;
        vkCmdWriteTimestamp(pCmd->mVk.pCmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pQueryPool->mVk.pQueryPool, index);
        break;
    }
    case VK_QUERY_TYPE_OCCLUSION:
    case VK_QUERY_TYPE_PIPELINE_STATISTICS:
    {
        const uint32_t index = pQuery->mIndex;
        vkCmdEndQuery(pCmd->mVk.pCmdBuf, pQueryPool->mVk.pQueryPool, index);
        break;
    }
    default:
        ASSERT(false && "Not implemented");
    }
}

void cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(pQueryPool);
    UNREF_PARAM(startQuery);
    UNREF_PARAM(queryCount);
}

void cmdResetQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
    ASSERT(pCmd);
    ASSERT(pQueryPool);
    ASSERT(queryCount);

    const uint32_t pairQueryCount = (VK_QUERY_TYPE_TIMESTAMP == pQueryPool->mVk.mType ? 2 : 1);
    vkCmdResetQueryPool(pCmd->mVk.pCmdBuf, pQueryPool->mVk.pQueryPool, startQuery * pairQueryCount, queryCount * pairQueryCount);
}

void getQueryData(Renderer* pRenderer, QueryPool* pQueryPool, uint32_t queryIndex, QueryData* pOutData)
{
    ASSERT(pRenderer);
    ASSERT(pQueryPool);
    ASSERT(pOutData);

    const VkQueryType type = pQueryPool->mVk.mType;
    memset(pOutData, 0, sizeof(QueryData));
    pOutData->mValid = true;

    switch (type)
    {
    case VK_QUERY_TYPE_TIMESTAMP:
    {
        vkGetQueryPoolResults(pRenderer->mVk.pDevice, pQueryPool->mVk.pQueryPool, queryIndex * 2, 2, pQueryPool->mStride * 2,
                              &pOutData->mBeginTimestamp, pQueryPool->mStride, VK_QUERY_RESULT_64_BIT);
        break;
    }
    case VK_QUERY_TYPE_OCCLUSION:
    {
        vkGetQueryPoolResults(pRenderer->mVk.pDevice, pQueryPool->mVk.pQueryPool, queryIndex, 1, pQueryPool->mStride,
                              &pOutData->mOcclusionCounts, pQueryPool->mStride, VK_QUERY_RESULT_64_BIT);
        break;
    }
    case VK_QUERY_TYPE_PIPELINE_STATISTICS:
    {
        uint64_t data[sizeof(PipelineStatisticsQueryData) / sizeof(uint64_t)] = { 0 };
        vkGetQueryPoolResults(pRenderer->mVk.pDevice, pQueryPool->mVk.pQueryPool, queryIndex, 1, pQueryPool->mStride, data,
                              pQueryPool->mStride, VK_QUERY_RESULT_64_BIT);
        memcpy(&pOutData->mPipelineStats, data, pQueryPool->mStride);
        if (!pRenderer->pGpu->mTessellationSupported)
        {
            COMPILE_ASSERT(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT == 0x00000080);
            COMPILE_ASSERT(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT == 0x00000100);
            // Final entry depends on whether tessellation is supported
            pOutData->mPipelineStats.mCSInvocations = data[8];
        }
        break;
    }
    default:
        ASSERT(false && "Not implemented");
    }
}
/************************************************************************/
// Memory Stats Implementation
/************************************************************************/
void logMemoryStats(Renderer* pRenderer) { vmaBuildStatsString(pRenderer->mVk.pVmaAllocator, VK_TRUE); }

void calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
{
    VmaTotalStatistics stats = { 0 };
    vmaCalculateStatistics(pRenderer->mVk.pVmaAllocator, &stats);
    *usedBytes = stats.total.statistics.allocationBytes;
    *totalAllocatedBytes = stats.total.statistics.blockBytes;
}
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    if (pCmd->pRenderer->pContext->mVk.mDebugUtilsExtension)
    {
        VkDebugUtilsLabelEXT markerInfo = { 0 };
        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        markerInfo.color[0] = r;
        markerInfo.color[1] = g;
        markerInfo.color[2] = b;
        markerInfo.color[3] = 1.0f;
        markerInfo.pLabelName = pName;
        vkCmdBeginDebugUtilsLabelEXT(pCmd->mVk.pCmdBuf, &markerInfo);
    }
    else if (pCmd->pRenderer->pGpu->mDebugMarkerExtension)
    {
        VkDebugMarkerMarkerInfoEXT markerInfo = { 0 };
        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
        markerInfo.color[0] = r;
        markerInfo.color[1] = g;
        markerInfo.color[2] = b;
        markerInfo.color[3] = 1.0f;
        markerInfo.pMarkerName = pName;
        vkCmdDebugMarkerBeginEXT(pCmd->mVk.pCmdBuf, &markerInfo);
    }
#endif
}

void cmdEndDebugMarker(Cmd* pCmd)
{
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    if (pCmd->pRenderer->pContext->mVk.mDebugUtilsExtension)
    {
        vkCmdEndDebugUtilsLabelEXT(pCmd->mVk.pCmdBuf);
    }
    else if (pCmd->pRenderer->pGpu->mDebugMarkerExtension)
    {
        vkCmdDebugMarkerEndEXT(pCmd->mVk.pCmdBuf);
    }
#endif
}

void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    if (pCmd->pRenderer->pContext->mVk.mDebugUtilsExtension)
    {
        VkDebugUtilsLabelEXT markerInfo = { 0 };
        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        markerInfo.color[0] = r;
        markerInfo.color[1] = g;
        markerInfo.color[2] = b;
        markerInfo.color[3] = 1.0f;
        markerInfo.pLabelName = pName;
        vkCmdInsertDebugUtilsLabelEXT(pCmd->mVk.pCmdBuf, &markerInfo);
    }
    else if (pCmd->pRenderer->pGpu->mDebugMarkerExtension)
    {
        VkDebugMarkerMarkerInfoEXT markerInfo = { 0 };
        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
        markerInfo.color[0] = r;
        markerInfo.color[1] = g;
        markerInfo.color[2] = b;
        markerInfo.color[3] = 1.0f;
        markerInfo.pMarkerName = pName;
        vkCmdDebugMarkerInsertEXT(pCmd->mVk.pCmdBuf, &markerInfo);
    }
#endif // ENABLE_GRAPHICS_DEBUG_ANNOTATION
}

void cmdWriteMarker(Cmd* pCmd, const MarkerDesc* pDesc)
{
    ASSERT(pCmd);
    ASSERT(pDesc);
    ASSERT(pDesc->pBuffer);

    if (pCmd->pRenderer->pGpu->mAMDBufferMarkerExtension)
    {
        VkPipelineStageFlagBits pipeStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        if (pDesc->mFlags & MARKER_FLAG_WAIT_FOR_WRITE)
        {
            pipeStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }
        vkCmdWriteBufferMarkerAMD(pCmd->mVk.pCmdBuf, pipeStage, pDesc->pBuffer->mVk.pBuffer, pDesc->mOffset, pDesc->mValue);
    }
    else
    {
        vkCmdFillBuffer(pCmd->mVk.pCmdBuf, pDesc->pBuffer->mVk.pBuffer, pDesc->mOffset, sizeof(uint32_t), pDesc->mValue);
        if (pDesc->mFlags & MARKER_FLAG_WAIT_FOR_WRITE)
        {
            VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
            barrier.srcAccessMask = util_to_vk_access_flags((ResourceState)UINT32_MAX); //-V1016
            barrier.dstAccessMask = barrier.srcAccessMask;
            vkCmdPipelineBarrier(pCmd->mVk.pCmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier,
                                 0, NULL, 0, NULL);
        }
    }
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
static void SetVkObjectName(Renderer* pRenderer, uint64_t handle, VkObjectType type, VkDebugReportObjectTypeEXT typeExt, const char* pName)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(handle);
    UNREF_PARAM(type);
    UNREF_PARAM(typeExt);
    UNREF_PARAM(pName);
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    // #NOTE: Some drivers dont like empty names - VK_ERROR_OUT_OF_HOST_MEMORY
    if (!pName || !strcmp(pName, ""))
    {
        return;
    }
    if (pRenderer->pContext->mVk.mDebugUtilsExtension)
    {
        VkDebugUtilsObjectNameInfoEXT nameInfo = { 0 };
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = type;
        nameInfo.objectHandle = handle;
        nameInfo.pObjectName = pName;
        vkSetDebugUtilsObjectNameEXT(pRenderer->mVk.pDevice, &nameInfo);
    }
    else if (pRenderer->pGpu->mDebugMarkerExtension)
    {
        VkDebugMarkerObjectNameInfoEXT nameInfo = { 0 };
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = typeExt;
        nameInfo.object = handle;
        nameInfo.pObjectName = pName;
        vkDebugMarkerSetObjectNameEXT(pRenderer->mVk.pDevice, &nameInfo);
    }
#endif
}

void setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
    ASSERT(pRenderer);
    ASSERT(pBuffer);
    ASSERT(pName);
    SetVkObjectName(pRenderer, (uint64_t)pBuffer->mVk.pBuffer, VK_OBJECT_TYPE_BUFFER, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, pName);
}

void setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
    ASSERT(pRenderer);
    ASSERT(pTexture);
    ASSERT(pName);
    SetVkObjectName(pRenderer, (uint64_t)pTexture->mVk.pImage, VK_OBJECT_TYPE_IMAGE, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, pName);
    if (pTexture->mVk.pSRVDescriptor)
    {
        SetVkObjectName(pRenderer, (uint64_t)pTexture->mVk.pSRVDescriptor, VK_OBJECT_TYPE_IMAGE_VIEW,
                        VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, pName);
    }
    if (pTexture->mVk.pSRVStencilDescriptor)
    {
        SetVkObjectName(pRenderer, (uint64_t)pTexture->mVk.pSRVStencilDescriptor, VK_OBJECT_TYPE_IMAGE_VIEW,
                        VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, pName);
    }
    if (pTexture->mVk.pUAVDescriptors)
    {
        for (uint32_t i = 0; i < pTexture->mMipLevels; i++)
        {
            SetVkObjectName(pRenderer, (uint64_t)pTexture->mVk.pUAVDescriptors[i], VK_OBJECT_TYPE_IMAGE_VIEW,
                            VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, pName);
        }
    }
}

void setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName)
{
    ASSERT(pRenderer);
    ASSERT(pRenderTarget);
    ASSERT(pName);
    setTextureName(pRenderer, pRenderTarget->pTexture, pName);
    SetVkObjectName(pRenderer, (uint64_t)pRenderTarget->mVk.pDescriptor, VK_OBJECT_TYPE_IMAGE_VIEW,
                    VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, pName);
    uint32_t arraySize = pRenderTarget->mArraySize;
    uint32_t depthOrArraySize = arraySize * pRenderTarget->mDepth;
    uint32_t numRTVs = pRenderTarget->mMipLevels;
    if ((pRenderTarget->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
        (pRenderTarget->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
        numRTVs *= depthOrArraySize;
    for (uint32_t i = 0; i < numRTVs; i++)
    {
        SetVkObjectName(pRenderer, (uint64_t)pRenderTarget->mVk.pSliceDescriptors[i], VK_OBJECT_TYPE_IMAGE_VIEW,
                        VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, pName);
    }
}

void setPipelineName(Renderer* pRenderer, Pipeline* pPipeline, const char* pName)
{
    ASSERT(pRenderer);
    ASSERT(pPipeline);
    ASSERT(pName);
    SetVkObjectName(pRenderer, (uint64_t)pPipeline->mVk.pPipeline, VK_OBJECT_TYPE_PIPELINE, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT,
                    pName);
}

// OpenXR Vulkan functionality and bindings
#ifdef QUEST_VR

bool VerifySwapchainFormat(int64_t format)
{
    uint32_t  swapchainFormatCount;
    XrSession xrSession = GetCurrentXRSession();
    CHECK_OXRRESULT(xrEnumerateSwapchainFormats(xrSession, 0, &swapchainFormatCount, NULL));
    int64_t* swapchainFormats = (int64_t*)tf_malloc(swapchainFormatCount * sizeof(int64_t));
    CHECK_OXRRESULT(xrEnumerateSwapchainFormats(xrSession, swapchainFormatCount, &swapchainFormatCount, swapchainFormats));
    bool verified = false;
    for (uint32_t formatIndex = 0; formatIndex < swapchainFormatCount; formatIndex++)
    {
        if (format == swapchainFormats[formatIndex])
        {
            verified = true;
            break;
        }
    }
    tf_free(swapchainFormats);

    return verified;
}

bool DidRequestFoveation(SwapChainCreationFlags swapchainFlags)
{
    return (bool)(swapchainFlags & SWAP_CHAIN_CREATION_FLAG_ENABLE_FOVEATED_RENDERING_VR);
}

void AddOpenXRSwapchain(uint32_t width, uint32_t height, bool multiView, int64_t format, uint32_t sampleCount,
                        SwapChainCreationFlags swapchainFlags, XrSwapchain* outSwapchain, uint32_t* outSwapchainImagesCount)
{
    ASSERT(GetXRViewCount() == MAX_OPENXR_VIEWS);

    LOGF(eINFO, "Creating OpenXR Swapchains with dimensions %d X %d and SampleCount %d", width, height, sampleCount);

    XrSwapchainCreateInfo swapchainCreateInfo = { .type = XR_TYPE_SWAPCHAIN_CREATE_INFO };
    swapchainCreateInfo.arraySize = multiView ? GetXRViewCount() : 1;
    swapchainCreateInfo.format = format;
    swapchainCreateInfo.width = width;
    swapchainCreateInfo.height = height;
    swapchainCreateInfo.mipCount = 1;
    swapchainCreateInfo.faceCount = 1;
    swapchainCreateInfo.sampleCount = sampleCount;
    swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

    XrSwapchainCreateInfoFoveationFB swapchainFoveationCreateInfo = { XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB };
    if (DidRequestFoveation(swapchainFlags))
    {
        swapchainFoveationCreateInfo.flags = XR_SWAPCHAIN_CREATE_FOVEATION_FRAGMENT_DENSITY_MAP_BIT_FB;
        swapchainCreateInfo.next = &swapchainFoveationCreateInfo;
    }

    CHECK_OXRRESULT(xrCreateSwapchain(GetCurrentXRSession(), &swapchainCreateInfo, outSwapchain));

    CHECK_OXRRESULT(xrEnumerateSwapchainImages(*outSwapchain, 0, outSwapchainImagesCount, NULL));
}

void RemoveOpenXRSwapchain(const XrSwapchain swapchainHandle)
{
    ASSERT(swapchainHandle != XR_NULL_HANDLE);
    CHECK_OXRRESULT(xrDestroySwapchain(swapchainHandle));
}

void GetOpenXRSwapchainImages(const XrSwapchain swapchainHandle, uint32_t swapchainImagesCount,
                              XrSwapchainImageBaseHeader* outSwapchainImages)
{
    ASSERT(swapchainHandle != XR_NULL_HANDLE);

    CHECK_OXRRESULT(xrEnumerateSwapchainImages(swapchainHandle, swapchainImagesCount, &swapchainImagesCount, outSwapchainImages));
}

void AcquireOpenXRSwapchainImage(const XrSwapchain swapchainHandle, uint32_t* outSwapchainImageIndex)
{
    XrSwapchainImageAcquireInfo acquireInfo = { .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    CHECK_OXRRESULT(xrAcquireSwapchainImage(swapchainHandle, &acquireInfo, outSwapchainImageIndex));

    XrSwapchainImageWaitInfo waitInfo = { .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = XR_INFINITE_DURATION;
    CHECK_OXRRESULT(xrWaitSwapchainImage(swapchainHandle, &waitInfo));
}

void ReleaseOpenXRSwapchainImage(const XrSwapchain swapchainHandle)
{
    XrSwapchainImageReleaseInfo releaseInfo = { .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    CHECK_OXRRESULT(xrReleaseSwapchainImage(swapchainHandle, &releaseInfo));
}

void SetOpenXRFoveation(XrFoveationLevelFB level, float verticalOffset, XrFoveationDynamicFB dynamic, const XrSwapchain swapchainHandle)
{
    XrInstance                     xrInstance = GetCurrentXRInstance();
    PFN_xrCreateFoveationProfileFB pfnCreateFoveationProfileFB;
    CHECK_OXRRESULT(xrGetInstanceProcAddr(xrInstance, "xrCreateFoveationProfileFB", (PFN_xrVoidFunction*)(&pfnCreateFoveationProfileFB)));

    PFN_xrDestroyFoveationProfileFB pfnDestroyFoveationProfileFB;
    CHECK_OXRRESULT(xrGetInstanceProcAddr(xrInstance, "xrDestroyFoveationProfileFB", (PFN_xrVoidFunction*)(&pfnDestroyFoveationProfileFB)));

    PFN_xrUpdateSwapchainFB pfnUpdateSwapchainFB;
    CHECK_OXRRESULT(xrGetInstanceProcAddr(xrInstance, "xrUpdateSwapchainFB", (PFN_xrVoidFunction*)(&pfnUpdateSwapchainFB)));

    XrFoveationLevelProfileCreateInfoFB levelProfileCreateInfo = { .type = XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB };
    levelProfileCreateInfo.level = level;
    levelProfileCreateInfo.verticalOffset = verticalOffset;
    levelProfileCreateInfo.dynamic = dynamic;

    XrFoveationProfileCreateInfoFB profileCreateInfo = { .type = XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB };
    profileCreateInfo.next = &levelProfileCreateInfo;

    XrFoveationProfileFB foveationProfile;

    pfnCreateFoveationProfileFB(GetCurrentXRSession(), &profileCreateInfo, &foveationProfile);

    XrSwapchainStateFoveationFB foveationUpdateState = { .type = XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB };
    foveationUpdateState.profile = foveationProfile;

    pfnUpdateSwapchainFB(swapchainHandle, (XrSwapchainStateBaseHeaderFB*)(&foveationUpdateState));

    pfnDestroyFoveationProfileFB(foveationProfile);
}

void BeginOpenXRFrame()
{
    XrSession xrSession = GetCurrentXRSession();
    ASSERT(xrSession != XR_NULL_HANDLE);

    XrFrameState*   pXrFrameState = GetCurrentXRFrameState();
    XrFrameWaitInfo frameWaitInfo = { .type = XR_TYPE_FRAME_WAIT_INFO };
    memset(pXrFrameState, 0, sizeof(XrFrameState));
    pXrFrameState->type = XR_TYPE_FRAME_STATE;
    CHECK_OXRRESULT(xrWaitFrame(xrSession, &frameWaitInfo, pXrFrameState));

    XrFrameBeginInfo frameBeginInfo = { .type = XR_TYPE_FRAME_BEGIN_INFO };
    CHECK_OXRRESULT(xrBeginFrame(xrSession, &frameBeginInfo));

    XrViewState viewState;
    viewState.type = XR_TYPE_VIEW_STATE;
    uint32_t viewCapacityInput = (uint32_t)MAX_OPENXR_VIEWS;
    uint32_t viewCountOutput;

    XrViewLocateInfo viewLocateInfo = { .type = XR_TYPE_VIEW_LOCATE_INFO };
    viewLocateInfo.viewConfigurationType = GetXRViewConfig().viewConfigurationType;
    viewLocateInfo.displayTime = pXrFrameState->predictedDisplayTime;
    viewLocateInfo.space = GetXRLocalSpace();

    CHECK_OXRRESULT(xrLocateViews(xrSession, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, GetXRViews()));
    ASSERTMSG(viewCountOutput == GetXRViewCount(), "Mismatch in Viewcount from xrLocateViews");

    XrView mViewSpaceViews[MAX_OPENXR_VIEWS];
    mViewSpaceViews[0].type = XR_TYPE_VIEW;
    mViewSpaceViews[1].type = XR_TYPE_VIEW;

    viewLocateInfo.space = GetXRViewSpace();
    CHECK_OXRRESULT(xrLocateViews(xrSession, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, mViewSpaceViews));
}

void EndOpenXRFrame(OpenXRPresentDesc* desc)
{
    XrSession xrSession = GetCurrentXRSession();
    ASSERT(xrSession != XR_NULL_HANDLE);

    XrCompositionLayerProjectionView    projectionLayerViews[MAX_OPENXR_VIEWS] = { [LEFT_EYE_VIEW] = { 0 }, [RIGHT_EYE_VIEW] = { 0 } };
    const XrCompositionLayerBaseHeader* layerPtrs[MAX_OPENXR_LAYERS] = { 0 };
    uint32_t                            layerCount = 0;
    uint32_t                            viewCount = GetXRViewCount();
    XrView*                             pXRViews = GetXRViews();

    for (uint32_t layerViewIndex = 0; layerViewIndex < viewCount; layerViewIndex++)
    {
        projectionLayerViews[layerViewIndex].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        projectionLayerViews[layerViewIndex].pose = pXRViews[layerViewIndex].pose;
        projectionLayerViews[layerViewIndex].fov = pXRViews[layerViewIndex].fov;
        projectionLayerViews[layerViewIndex].subImage.swapchain = desc->mMainView.mSwapchainHandle;
        projectionLayerViews[layerViewIndex].subImage.imageRect.offset.x = 0;
        projectionLayerViews[layerViewIndex].subImage.imageRect.offset.y = 0;
        projectionLayerViews[layerViewIndex].subImage.imageRect.extent.width = desc->mMainView.mWidth;
        projectionLayerViews[layerViewIndex].subImage.imageRect.extent.height = desc->mMainView.mHeight;
        projectionLayerViews[layerViewIndex].subImage.imageArrayIndex = layerViewIndex;
    }

    XrCompositionLayerProjection layer = { .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    layer.space = GetXRLocalSpace();
    layer.layerFlags = 0;
    layer.viewCount = viewCount;
    layer.views = projectionLayerViews;
    layerPtrs[layerCount] = (XrCompositionLayerBaseHeader*)&layer;
    ++layerCount;

    XrCompositionLayerQuad quadLayer = { .type = XR_TYPE_COMPOSITION_LAYER_QUAD };
    if (desc->m2DLayer.mSwapchainHandle != XR_NULL_HANDLE)
    {
        quadLayer.next = NULL;
        quadLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        quadLayer.space = GetXRViewSpace();
        quadLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        quadLayer.subImage.swapchain = desc->m2DLayer.mSwapchainHandle;
        quadLayer.subImage.imageRect.offset.x = 0;
        quadLayer.subImage.imageRect.offset.y = 0;
        quadLayer.subImage.imageRect.extent.width = desc->m2DLayer.mWidth;
        quadLayer.subImage.imageRect.extent.height = desc->m2DLayer.mHeight;

        XrPosef pose;
        XrPosef_CreateIdentity(&pose);
        pose.position.x = desc->m2DLayerSettings.m2DLayerPosition.x;
        pose.position.y = desc->m2DLayerSettings.m2DLayerPosition.y;
        pose.position.z = desc->m2DLayerSettings.m2DLayerPosition.z;
        pose.orientation.x = desc->m2DLayerSettings.m2DLayerRotQuat.x;
        pose.orientation.y = desc->m2DLayerSettings.m2DLayerRotQuat.y;
        pose.orientation.z = desc->m2DLayerSettings.m2DLayerRotQuat.z;
        pose.orientation.w = desc->m2DLayerSettings.m2DLayerRotQuat.w;
        quadLayer.pose = pose;

        float effectiveScale = desc->m2DLayerSettings.m2DLayerScale;
        effectiveScale = effectiveScale <= 0.0f ? 1.0f : effectiveScale;
        float aspectRatio = (float)desc->m2DLayer.mHeight / (float)desc->m2DLayer.mWidth;
        quadLayer.size.width = effectiveScale;
        quadLayer.size.height = effectiveScale * aspectRatio;

        layerPtrs[layerCount] = (XrCompositionLayerBaseHeader*)&quadLayer;
        ++layerCount;
    }

    XrFrameState* pCurrentFrameState = GetCurrentXRFrameState();
    ASSERT(pCurrentFrameState->type == XR_TYPE_FRAME_STATE);
    XrFrameEndInfo frameEndInfo = { .type = XR_TYPE_FRAME_END_INFO };
    frameEndInfo.displayTime = pCurrentFrameState->predictedDisplayTime;
    frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE; // TODO: Add support for additional blend modes?
    frameEndInfo.layerCount = layerCount;
    frameEndInfo.layers = layerPtrs;
    CHECK_OXRRESULT(xrEndFrame(xrSession, &frameEndInfo));
}

XrResult GetVulkanGraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsVulkanKHR* graphicsRequirements)
{
    PFN_xrGetVulkanGraphicsRequirementsKHR pfnGetVulkanGraphicsRequirementsKHR = NULL;
    xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&pfnGetVulkanGraphicsRequirementsKHR);

    pfnGetVulkanGraphicsRequirementsKHR(instance, systemId, graphicsRequirements);

    return XR_SUCCESS;
}

XrResult GetVulkanGraphicsDeviceKHR(XrInstance instance, const XrSystemId systemId, VkInstance vkInstance, VkPhysicalDevice* physicalDevice)
{
    PFN_xrGetVulkanGraphicsDeviceKHR pfnGetVulkanGraphicsDeviceKHR = NULL;
    xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction*)&pfnGetVulkanGraphicsDeviceKHR);

    return pfnGetVulkanGraphicsDeviceKHR(instance, systemId, vkInstance, physicalDevice);
}

bool OpenXRAddVKInstanceExt(const char** instanceExtensionCache, uint* extensionCount, uint maxExtensionCount, char* pExtensionsBuffer,
                            uint extensionsBufferSize)
{
    XrInstance                           xrInstance = GetCurrentXRInstance();
    XrSystemId                           xrSystemId = GetXRSystemId();
    PFN_xrGetVulkanInstanceExtensionsKHR pfnGetVulkanInstanceExtensionsKHR = NULL;
    xrGetInstanceProcAddr(xrInstance, "xrGetVulkanInstanceExtensionsKHR", (PFN_xrVoidFunction*)&pfnGetVulkanInstanceExtensionsKHR);

    pfnGetVulkanInstanceExtensionsKHR(xrInstance, xrSystemId, 0, &extensionsBufferSize, NULL);
    pfnGetVulkanInstanceExtensionsKHR(xrInstance, xrSystemId, extensionsBufferSize, &extensionsBufferSize, pExtensionsBuffer);

    const char* vrExtensions[32];
    vrExtensions[0] = pExtensionsBuffer;
    int vrExtensionCount = 1;
    for (int i = 0; i < extensionsBufferSize; ++i)
    {
        if (pExtensionsBuffer[i] == ' ')
        {
            vrExtensions[vrExtensionCount] = &pExtensionsBuffer[i + 1];
            pExtensionsBuffer[i] = '\0';
            ++vrExtensionCount;
        }
        else if (pExtensionsBuffer[i] == '\0')
            break;
    }

    for (int i = 0; i < vrExtensionCount; ++i)
    {
        bool alreadyInCache = false;
        for (int j = 0; j < (*extensionCount); ++j)
        {
            if (strcmp(vrExtensions[i], instanceExtensionCache[j]) == 0)
            {
                alreadyInCache = true;
                break;
            }
        }

        if (!alreadyInCache)
        {
            if ((*extensionCount) == maxExtensionCount)
            {
                LOGF(eERROR, "Reached maximum instance extension count");
                ASSERT(false);
                return false;
            }
            instanceExtensionCache[*extensionCount] = vrExtensions[i];
            LOGF(eINFO, "Adding %s Vk Instance Extensions requested by the OpenXR runtime", vrExtensions[i]);
            ++(*extensionCount);
        }
    }

    return true;
}

bool OpenXRAddVKDeviceExt(const char** deviceExtensionCache, uint* extensionCount, uint maxExtensionCount, char* pExtensionsBuffer,
                          uint extensionsBufferSize)
{
    XrInstance                         xrInstance = GetCurrentXRInstance();
    PFN_xrGetVulkanDeviceExtensionsKHR pfnGetVulkanDeviceExtensionsKHR = NULL;
    xrGetInstanceProcAddr(xrInstance, "xrGetVulkanDeviceExtensionsKHR", (PFN_xrVoidFunction*)(&pfnGetVulkanDeviceExtensionsKHR));

    extensionsBufferSize = 0;
    XrSystemId xrSystemId = GetXRSystemId();
    pfnGetVulkanDeviceExtensionsKHR(xrInstance, xrSystemId, 0, &extensionsBufferSize, NULL);
    if (extensionsBufferSize > 0)
    {
        pfnGetVulkanDeviceExtensionsKHR(xrInstance, xrSystemId, extensionsBufferSize, &extensionsBufferSize, pExtensionsBuffer);
    }

    const char* vrExtensions[32];
    vrExtensions[0] = pExtensionsBuffer;
    int vrExtensionCount = 1;
    for (int i = 0; i < extensionsBufferSize; ++i)
    {
        if (pExtensionsBuffer[i] == ' ')
        {
            vrExtensions[vrExtensionCount] = &pExtensionsBuffer[i + 1];
            pExtensionsBuffer[i] = '\0';
            ++vrExtensionCount;
        }
        else if (pExtensionsBuffer[i] == '\0')
            break;
    }

    for (int i = 0; i < vrExtensionCount; ++i)
    {
        bool alreadyInCache = false;
        for (int j = 0; j < (*extensionCount); ++j)
        {
            if (strcmp(vrExtensions[i], deviceExtensionCache[j]) == 0)
            {
                alreadyInCache = true;
                break;
            }
        }

        if (!alreadyInCache)
        {
            if ((*extensionCount) == maxExtensionCount)
            {
                LOGF(eERROR, "Reached maximum device extension count");
                ASSERT(false);
                return false;
            }
            deviceExtensionCache[*extensionCount] = vrExtensions[i];
            LOGF(eINFO, "Adding %s Vk Device Extensions requested by the OpenXR runtime", vrExtensions[i]);
            ++(*extensionCount);
        }
    }

    return true;
}

bool OpenXRPostInitVKRenderer(Renderer* pRenderer, VkInstance instance, VkDevice device, uint8_t queueFamilyIndex)
{
    // Create the Vulkan device for the adapter associated with the system.
    // Extension function must be loaded by name
    XrInstance                      xrInstance = GetCurrentXRInstance();
    XrGraphicsRequirementsVulkanKHR graphicsRequirements = { .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };
    GetVulkanGraphicsRequirementsKHR(xrInstance, GetXRSystemId(), &graphicsRequirements);

    VkPhysicalDevice vkPhysicalDevice;
    GetVulkanGraphicsDeviceKHR(xrInstance, GetXRSystemId(), instance, &vkPhysicalDevice);

    XrGraphicsBindingVulkanKHR* pGraphicsBinding = &pRenderer->pContext->mVR.mXRGraphicsBinding;
    memset(pGraphicsBinding, 0, sizeof(XrGraphicsBindingVulkanKHR));
    pGraphicsBinding->type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
    pGraphicsBinding->instance = instance;
    pGraphicsBinding->physicalDevice = vkPhysicalDevice;
    pGraphicsBinding->device = device;
    pGraphicsBinding->queueFamilyIndex = queueFamilyIndex;
    pGraphicsBinding->queueIndex = 0;
    pRenderer->mVR.pCurrentFDM = NULL;
    pRenderer->mVR.mIsFoveationEnabled = false;

    InitXRSession(pRenderer);

    return true;
}

void OpenXRAddVKSwapchain(Renderer* pRenderer, const SwapChainDesc* pDesc, bool multiView, SwapChain** ppSwapChain)
{
    VkFormat vkFormat = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mColorFormat);
    ASSERTMSG(VerifySwapchainFormat(vkFormat), "Requested swapchain format is not a supported OpenXR swapchain format");

    bool foveationEnabled = DidRequestFoveation(pDesc->mFlags);
    pRenderer->mVR.mIsFoveationEnabled |= foveationEnabled;

    XrSwapchain swapchain;
    uint32_t    imageCount;
    AddOpenXRSwapchain(pDesc->mWidth, pDesc->mHeight, multiView, vkFormat, VK_SAMPLE_COUNT_1_BIT, pDesc->mFlags, &swapchain, &imageCount);
    ASSERT(imageCount >= pDesc->mImageCount);

    SwapChain* pSwapChain = (SwapChain*)tf_calloc(1, sizeof(SwapChain) + 2 * imageCount * sizeof(RenderTarget*) +
                                                         imageCount * sizeof(XrSwapchainImageVulkanKHR) +
                                                         imageCount * sizeof(XrSwapchainImageFoveationVulkanFB) + sizeof(SwapChainDesc));
    ASSERT(pSwapChain);

    RenderTarget**                     ppRenderTargets = (RenderTarget**)(pSwapChain + 1);
    RenderTarget**                     ppFragmentDensityMapRTs = ppRenderTargets + imageCount;
    XrSwapchainImageVulkanKHR*         pSwapchainImages = (XrSwapchainImageVulkanKHR*)(ppFragmentDensityMapRTs + imageCount);
    XrSwapchainImageFoveationVulkanFB* pFoveationImages = (XrSwapchainImageFoveationVulkanFB*)(pSwapchainImages + imageCount);
    SwapChainDesc*                     pSwapchainDesc = (SwapChainDesc*)(pFoveationImages + imageCount);

    pSwapChain->ppRenderTargets = ppRenderTargets;
    pSwapChain->mVR.ppFoveationFragmentDensityMaps = ppFragmentDensityMapRTs;
    pSwapChain->mVR.pSwapchainImages = (XrSwapchainImageBaseHeader*)pSwapchainImages;
    pSwapChain->mVk.pDesc = pSwapchainDesc;

    pSwapChain->mVR.pSwapchain = swapchain;

    if (foveationEnabled)
    {
        XrFoveationDynamicFB dynamicFoveationLevel = (pDesc->mVR.mFoveationLevel == FOVEATION_LEVEL_DYNAMIC)
                                                         ? XR_FOVEATION_DYNAMIC_LEVEL_ENABLED_FB
                                                         : XR_FOVEATION_DYNAMIC_DISABLED_FB;
        XrFoveationLevelFB   foveationLevel;

        switch (pDesc->mFlags)
        {
        case FOVEATION_LEVEL_LOW:
            foveationLevel = XR_FOVEATION_LEVEL_LOW_FB;
            break;
        case FOVEATION_LEVEL_MEDIUM:
            foveationLevel = XR_FOVEATION_LEVEL_MEDIUM_FB;
            break;
        case FOVEATION_LEVEL_HIGH:
        default:
            foveationLevel = XR_FOVEATION_LEVEL_HIGH_FB;
            break;
        }

        SetOpenXRFoveation(foveationLevel, 0, dynamicFoveationLevel, pSwapChain->mVR.pSwapchain);
    }

    for (uint32_t imageIndex = 0; imageIndex < imageCount; imageIndex++)
    {
        XrSwapchainImageVulkanKHR* pSwapchainImage = pSwapchainImages + imageIndex;
        pSwapchainImage->type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
        pSwapchainImage->image = VK_NULL_HANDLE;

        if (foveationEnabled)
        {
            XrSwapchainImageFoveationVulkanFB* pFoveationImage = pFoveationImages + imageIndex;
            pFoveationImage->type = XR_TYPE_SWAPCHAIN_IMAGE_FOVEATION_VULKAN_FB;
            pFoveationImage->next = NULL;

            pSwapchainImage->next = pFoveationImage;
        }
    }
    GetOpenXRSwapchainImages(pSwapChain->mVR.pSwapchain, imageCount, pSwapChain->mVR.pSwapchainImages);

    RenderTargetDesc rtDesc = {};
    rtDesc.mWidth = pDesc->mWidth;
    rtDesc.mHeight = pDesc->mHeight;
    rtDesc.mDepth = 1;
    rtDesc.mArraySize = 1; // TODO: Support non multiview rendering
    rtDesc.mFormat = pDesc->mColorFormat;
    rtDesc.mClearValue = pDesc->mColorClearValue;
    rtDesc.mSampleCount = SAMPLE_COUNT_1;
    rtDesc.mSampleQuality = 0;
    rtDesc.mStartState = RESOURCE_STATE_PRESENT;
    if (multiView)
    {
        rtDesc.mFlags |= TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
    }
    if (foveationEnabled)
    {
        rtDesc.mFlags |= TEXTURE_CREATION_FLAG_VR_FOVEATED_RENDERING;
    }

    RenderTargetDesc fragmentDensityMapDesc = {};
    fragmentDensityMapDesc.mDepth = 1;
    fragmentDensityMapDesc.mArraySize = 1;
    fragmentDensityMapDesc.mFormat = TinyImageFormat_R8G8_UNORM;
    fragmentDensityMapDesc.mSampleCount = SAMPLE_COUNT_1;
    fragmentDensityMapDesc.mSampleQuality = 0;
    fragmentDensityMapDesc.mStartState = RESOURCE_STATE_SHADING_RATE_SOURCE;
    fragmentDensityMapDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;

    for (uint32_t rtIndex = 0; rtIndex < imageCount; rtIndex++)
    {
        XrSwapchainImageVulkanKHR* pSwapchainImage = &((XrSwapchainImageVulkanKHR*)pSwapChain->mVR.pSwapchainImages)[rtIndex];
        rtDesc.pNativeHandle = (void*)pSwapchainImage->image;
        addRenderTarget(pRenderer, &rtDesc, &pSwapChain->ppRenderTargets[rtIndex]);

        if (foveationEnabled)
        {
            XrSwapchainImageFoveationVulkanFB* pFoveationImage = (XrSwapchainImageFoveationVulkanFB*)pSwapchainImage->next;
            fragmentDensityMapDesc.mWidth = pFoveationImage->width;
            fragmentDensityMapDesc.mHeight = pFoveationImage->height;
            fragmentDensityMapDesc.pNativeHandle = (void*)pFoveationImage->image;
            addRenderTarget(pRenderer, &fragmentDensityMapDesc, &pSwapChain->mVR.ppFoveationFragmentDensityMaps[rtIndex]);
        }
    }

    *pSwapChain->mVk.pDesc = *pDesc;
    pSwapChain->mEnableVsync = pDesc->mEnableVsync;
    pSwapChain->mImageCount = imageCount;
    pSwapChain->mVk.pSurface = NULL;
    pSwapChain->mVk.mPresentQueueFamilyIndex = 0;
    pSwapChain->mVk.pPresentQueue = NULL;
    pSwapChain->mVk.pSwapChain = NULL;
    pSwapChain->mFormat = pDesc->mColorFormat;

    *ppSwapChain = pSwapChain;
}

void OpenXRRemoveVKSwapchain(Renderer* pRenderer, SwapChain* pSwapChain)
{
    for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
    {
        removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[i]);
    }

    pRenderer->mVR.pCurrentFDM = NULL;
    pRenderer->mVR.mIsFoveationEnabled = false;
    for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
    {
        RenderTarget* pFDM = pSwapChain->mVR.ppFoveationFragmentDensityMaps[i];
        if (pFDM != NULL)
        {
            removeRenderTarget(pRenderer, pFDM);
        }
    }

    RemoveOpenXRSwapchain(pSwapChain->mVR.pSwapchain);

    if (pSwapChain)
        tf_free(pSwapChain);
}

void OpenXRVKQueuePresent(const QueuePresentDesc* pQueuePresentDesc)
{
    SwapChain* pSwapchain = pQueuePresentDesc->pSwapChain;
    SwapChain* p2DLayerSwapChain = pSwapchain->mVR.m2DLayer.pSwapchain;

    OpenXRPresentDesc presentDesc;
    presentDesc.mMainView.mSwapchainHandle = pSwapchain->mVR.pSwapchain;
    presentDesc.mMainView.mWidth = pSwapchain->mVk.pDesc->mWidth;
    presentDesc.mMainView.mHeight = pSwapchain->mVk.pDesc->mHeight;
    presentDesc.m2DLayer.mSwapchainHandle = XR_NULL_HANDLE;

    ReleaseOpenXRSwapchainImage(pSwapchain->mVR.pSwapchain);
    if (p2DLayerSwapChain != NULL)
    {
        ReleaseOpenXRSwapchainImage(p2DLayerSwapChain->mVR.pSwapchain);

        presentDesc.m2DLayer.mWidth = p2DLayerSwapChain->mVk.pDesc->mWidth;
        presentDesc.m2DLayer.mHeight = p2DLayerSwapChain->mVk.pDesc->mHeight;
        presentDesc.m2DLayer.mSwapchainHandle = p2DLayerSwapChain->mVR.pSwapchain;
        presentDesc.m2DLayerSettings = p2DLayerSwapChain->mVR.m2DLayer.mDesc;
    }

    EndOpenXRFrame(&presentDesc);
}

#endif

#include "../ThirdParty/OpenSource/volk/volk.c"
#if defined(VK_USE_DISPATCH_TABLES)
#include "../ThirdParty/OpenSource/volk/volkForgeExt.c"
#endif

#endif

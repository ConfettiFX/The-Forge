/************************************************************************************

Filename	:	Framework_Vulkan.h
Content		:	Vulkan Framework
Created		:	October, 2017
Authors		:	J.M.P. van Waveren

Copyright	:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

/*
================================
Platform headers / declarations
================================
*/

#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

#define VULKAN_LOADER "libvulkan.so"

#include <android/log.h>

#include <VrApi_Types.h> // for vector and matrix types

/*
================================
Common headers
================================
*/

#include <assert.h>
#include <dlfcn.h> // for dlopen
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for memset

/*
================================
Common defines
================================
*/

#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))
#define OFFSETOF_MEMBER(type, member) (size_t) & ((type*)0)->member
#define SIZEOF_MEMBER(type, member) sizeof(((type*)0)->member)
#define MAX(x, y) ((x > y) ? (x) : (y))

#define VK_ALLOCATOR NULL

#define USE_API_DUMP \
    0 // place vk_layer_settings.txt in the executable folder and change APIDumpFile = TRUE

#define ICD_SPV_MAGIC 0x07230203
#define PROGRAM(name) name##SPIRV

#if !defined(ALOGE)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Framework_Vulkan", __VA_ARGS__)
#endif

#if !defined(ALOGV)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "Framework_Vulkan", __VA_ARGS__)
#endif

/*
================================================================================================================================

ovrScreenRect

================================================================================================================================
*/

typedef struct {
    int x;
    int y;
    int width;
    int height;
} ovrScreenRect;

/*
================================================================================================================================

Vulkan error checking.

================================================================================================================================
*/

#define VK(func) VkCheckErrors(func, #func);
#define VC(func) func;

static const char* VkErrorString(VkResult result) {
    switch (result) {
        case VK_SUCCESS:
            return "VK_SUCCESS";
        case VK_NOT_READY:
            return "VK_NOT_READY";
        case VK_TIMEOUT:
            return "VK_TIMEOUT";
        case VK_EVENT_SET:
            return "VK_EVENT_SET";
        case VK_EVENT_RESET:
            return "VK_EVENT_RESET";
        case VK_INCOMPLETE:
            return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:
            return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_SUBOPTIMAL_KHR:
            return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "VK_ERROR_VALIDATION_FAILED_EXT";
        default: {
            if (result == VK_ERROR_INVALID_SHADER_NV) {
                return "VK_ERROR_INVALID_SHADER_NV";
            }
            return "unknown";
        }
    }
}

static void VkCheckErrors(VkResult result, const char* function) {
    if (result != VK_SUCCESS) {
        ALOGE("Vulkan error: %s: %s\n", function, VkErrorString(result));
    }
}

/*
================================================================================================================================

Vulkan Instance.

================================================================================================================================
*/

typedef struct {
    void* loader;
    VkInstance instance;
    VkBool32 validate;

    // Global functions.
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
    PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
    PFN_vkCreateInstance vkCreateInstance;

    // Instance functions.
    PFN_vkDestroyInstance vkDestroyInstance;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
    PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
    PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties;
    PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
    PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
    PFN_vkCreateDevice vkCreateDevice;
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;

    // Debug callback.
    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
    VkDebugReportCallbackEXT debugReportCallback;

} ovrVkInstance;

// Expects 'requiredExtensionNames' as list of strings delimited by a single space.
bool ovrVkInstance_Create(
    ovrVkInstance* instance,
    const char* requiredExtensionNames,
    uint32_t requiredExtensionNamesSize);
void ovrVkInstance_Destroy(ovrVkInstance* instance);

/*
================================================================================================================================

Vulkan Device.

================================================================================================================================
*/

typedef struct {
    ovrVkInstance* instance;
    uint32_t enabledExtensionCount;
    const char* enabledExtensionNames[32];
    uint32_t enabledLayerCount;
    const char* enabledLayerNames[32];
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceFeatures2 physicalDeviceFeatures;
    VkPhysicalDeviceProperties2 physicalDeviceProperties;
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
    uint32_t queueFamilyCount;
    VkQueueFamilyProperties* queueFamilyProperties;
    uint32_t* queueFamilyUsedQueues;
    pthread_mutex_t queueFamilyMutex;
    int workQueueFamilyIndex;
    int presentQueueFamilyIndex;
    bool supportsMultiview;
    bool supportsFragmentDensity;
    bool supportsLazyAllocate;

    // The logical device.
    VkDevice device;

    // Device functions
    PFN_vkDestroyDevice vkDestroyDevice;
    PFN_vkGetDeviceQueue vkGetDeviceQueue;
    PFN_vkQueueSubmit vkQueueSubmit;
    PFN_vkQueueWaitIdle vkQueueWaitIdle;
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
    PFN_vkAllocateMemory vkAllocateMemory;
    PFN_vkFreeMemory vkFreeMemory;
    PFN_vkMapMemory vkMapMemory;
    PFN_vkUnmapMemory vkUnmapMemory;
    PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
    PFN_vkBindBufferMemory vkBindBufferMemory;
    PFN_vkBindImageMemory vkBindImageMemory;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
    PFN_vkCreateFence vkCreateFence;
    PFN_vkDestroyFence vkDestroyFence;
    PFN_vkResetFences vkResetFences;
    PFN_vkGetFenceStatus vkGetFenceStatus;
    PFN_vkWaitForFences vkWaitForFences;
    PFN_vkCreateBuffer vkCreateBuffer;
    PFN_vkDestroyBuffer vkDestroyBuffer;
    PFN_vkCreateImage vkCreateImage;
    PFN_vkDestroyImage vkDestroyImage;
    PFN_vkCreateImageView vkCreateImageView;
    PFN_vkDestroyImageView vkDestroyImageView;
    PFN_vkCreateShaderModule vkCreateShaderModule;
    PFN_vkDestroyShaderModule vkDestroyShaderModule;
    PFN_vkCreatePipelineCache vkCreatePipelineCache;
    PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
    PFN_vkDestroyPipeline vkDestroyPipeline;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
    PFN_vkCreateSampler vkCreateSampler;
    PFN_vkDestroySampler vkDestroySampler;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
    PFN_vkResetDescriptorPool vkResetDescriptorPool;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
    PFN_vkCreateFramebuffer vkCreateFramebuffer;
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
    PFN_vkCreateRenderPass vkCreateRenderPass;
    PFN_vkDestroyRenderPass vkDestroyRenderPass;
    PFN_vkCreateCommandPool vkCreateCommandPool;
    PFN_vkDestroyCommandPool vkDestroyCommandPool;
    PFN_vkResetCommandPool vkResetCommandPool;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
    PFN_vkEndCommandBuffer vkEndCommandBuffer;
    PFN_vkResetCommandBuffer vkResetCommandBuffer;
    PFN_vkCmdBindPipeline vkCmdBindPipeline;
    PFN_vkCmdSetViewport vkCmdSetViewport;
    PFN_vkCmdSetScissor vkCmdSetScissor;
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
    PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
    PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
    PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
    PFN_vkCmdResolveImage vkCmdResolveImage;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
    PFN_vkCmdPushConstants vkCmdPushConstants;
    PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
    PFN_vkCmdEndRenderPass vkCmdEndRenderPass;

} ovrVkDevice;

// Expects 'requiredExtensionNames' as list of strings delimited by a single space.
bool ovrVkDevice_SelectPhysicalDevice(
    ovrVkDevice* device,
    ovrVkInstance* instance,
    const char* requiredExtensionNames,
    uint32_t requiredExtensionNamesSize);

bool ovrVkDevice_Create(ovrVkDevice* device, ovrVkInstance* instance);

void ovrVkDevice_Destroy(ovrVkDevice* device);

/*
================================================================================================================================

Vulkan context.

A context encapsulates a queue that is used to submit command buffers.
A context can only be used by a single thread.
For optimal performance a context should only be created at load time, not at runtime.

================================================================================================================================
*/

typedef enum {
    OVR_SURFACE_COLOR_FORMAT_R5G6B5,
    OVR_SURFACE_COLOR_FORMAT_B5G6R5,
    OVR_SURFACE_COLOR_FORMAT_R8G8B8A8,
    OVR_SURFACE_COLOR_FORMAT_B8G8R8A8,
    OVR_SURFACE_COLOR_FORMAT_MAX
} ovrSurfaceColorFormat;

typedef enum {
    OVR_SURFACE_DEPTH_FORMAT_NONE,
    OVR_SURFACE_DEPTH_FORMAT_D16,
    OVR_SURFACE_DEPTH_FORMAT_D24,
    OVR_SURFACE_DEPTH_FORMAT_MAX
} ovrSurfaceDepthFormat;

typedef enum {
    OVR_SAMPLE_COUNT_1 = VK_SAMPLE_COUNT_1_BIT,
    OVR_SAMPLE_COUNT_2 = VK_SAMPLE_COUNT_2_BIT,
    OVR_SAMPLE_COUNT_4 = VK_SAMPLE_COUNT_4_BIT,
    OVR_SAMPLE_COUNT_8 = VK_SAMPLE_COUNT_8_BIT,
} ovrSampleCount;

typedef struct {
    ovrVkDevice* device;
    uint32_t queueFamilyIndex;
    uint32_t queueIndex;
    VkQueue queue;
    VkCommandPool commandPool;
    VkPipelineCache pipelineCache;
    VkCommandBuffer setupCommandBuffer;
} ovrVkContext;

bool ovrVkContext_Create(ovrVkContext* context, ovrVkDevice* device, const int queueIndex);

void ovrVkContext_Destroy(ovrVkContext* context);

void ovrVkContext_WaitIdle(ovrVkContext* context);

void ovrVkContext_CreateSetupCmdBuffer(ovrVkContext* context);

void ovrVkContext_FlushSetupCmdBuffer(ovrVkContext* context);

/*
================================================================================================================================

Vulkan depth buffer.

For optimal performance a depth buffer should only be created at load time, not at runtime.

================================================================================================================================
*/

typedef struct {
    ovrSurfaceDepthFormat format;
    VkFormat internalFormat;
    VkImageLayout imageLayout;
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
} ovrVkDepthBuffer;

void ovrVkDepthBuffer_Create(
    ovrVkContext* context,
    ovrVkDepthBuffer* depthBuffer,
    const ovrSurfaceDepthFormat depthFormat,
    const ovrSampleCount sampleCount,
    const int width,
    const int height,
    const int numLayers);

void ovrVkDepthBuffer_Destroy(ovrVkContext* context, ovrVkDepthBuffer* depthBuffer);

/*
================================================================================================================================

Vulkan buffer.

A buffer maintains a block of memory for a specific use by GPU programs (vertex, index, uniform,
storage). For optimal performance a buffer should only be created at load time, not at runtime. The
best performance is typically achieved when the buffer is not host visible.

================================================================================================================================
*/

typedef enum {
    OVR_BUFFER_TYPE_VERTEX,
    OVR_BUFFER_TYPE_INDEX,
    OVR_BUFFER_TYPE_UNIFORM
} ovrVkBufferType;

typedef struct ovrVkBuffer_s {
    struct ovrVkBuffer_s* next;
    int unusedCount;
    ovrVkBufferType type;
    size_t size;
    VkMemoryPropertyFlags flags;
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* mapped;
    bool owner;
} ovrVkBuffer;

bool ovrVkBuffer_Create(
    ovrVkContext* context,
    ovrVkBuffer* buffer,
    const ovrVkBufferType type,
    const size_t dataSize,
    const void* data,
    const bool hostVisible);

void ovrVkBuffer_Destroy(ovrVkDevice* device, ovrVkBuffer* buffer);

/*
================================================================================================================================

Vulkan texture.

For optimal performance a texture should only be created or modified at load time, not at runtime.
Note that the geometry code assumes the texture origin 0,0 = left-top as opposed to left-bottom.
In other words, textures are expected to be stored top-down as opposed to bottom-up.

================================================================================================================================
*/

// Note that the channel listed first in the name shall occupy the least significant bit.
typedef enum {
    //
    // 8 bits per component
    //
    OVR_TEXTURE_FORMAT_R8_UNORM = VK_FORMAT_R8_UNORM, // 1-component, 8-bit unsigned normalized
    OVR_TEXTURE_FORMAT_R8G8_UNORM = VK_FORMAT_R8G8_UNORM, // 2-component, 8-bit unsigned normalized
    OVR_TEXTURE_FORMAT_R8G8B8A8_UNORM =
        VK_FORMAT_R8G8B8A8_UNORM, // 4-component, 8-bit unsigned normalized

    OVR_TEXTURE_FORMAT_R8_SNORM = VK_FORMAT_R8_SNORM, // 1-component, 8-bit signed normalized
    OVR_TEXTURE_FORMAT_R8G8_SNORM = VK_FORMAT_R8G8_SNORM, // 2-component, 8-bit signed normalized
    OVR_TEXTURE_FORMAT_R8G8B8A8_SNORM =
        VK_FORMAT_R8G8B8A8_SNORM, // 4-component, 8-bit signed normalized

    OVR_TEXTURE_FORMAT_R8_UINT = VK_FORMAT_R8_UINT, // 1-component, 8-bit unsigned integer
    OVR_TEXTURE_FORMAT_R8G8_UINT = VK_FORMAT_R8G8_UINT, // 2-component, 8-bit unsigned integer
    OVR_TEXTURE_FORMAT_R8G8B8A8_UINT =
        VK_FORMAT_R8G8B8A8_UINT, // 4-component, 8-bit unsigned integer

    OVR_TEXTURE_FORMAT_R8_SINT = VK_FORMAT_R8_SINT, // 1-component, 8-bit signed integer
    OVR_TEXTURE_FORMAT_R8G8_SINT = VK_FORMAT_R8G8_SINT, // 2-component, 8-bit signed integer
    OVR_TEXTURE_FORMAT_R8G8B8A8_SINT = VK_FORMAT_R8G8B8A8_SINT, // 4-component, 8-bit signed integer

    OVR_TEXTURE_FORMAT_R8_SRGB = VK_FORMAT_R8_SRGB, // 1-component, 8-bit sRGB
    OVR_TEXTURE_FORMAT_R8G8_SRGB = VK_FORMAT_R8G8_SRGB, // 2-component, 8-bit sRGB
    OVR_TEXTURE_FORMAT_R8G8B8A8_SRGB = VK_FORMAT_R8G8B8A8_SRGB, // 4-component, 8-bit sRGB

    //
    // 16 bits per component
    //
    OVR_TEXTURE_FORMAT_R16_UNORM = VK_FORMAT_R16_UNORM, // 1-component, 16-bit unsigned normalized
    OVR_TEXTURE_FORMAT_R16G16_UNORM =
        VK_FORMAT_R16G16_UNORM, // 2-component, 16-bit unsigned normalized
    OVR_TEXTURE_FORMAT_R16G16B16A16_UNORM =
        VK_FORMAT_R16G16B16A16_UNORM, // 4-component, 16-bit unsigned normalized

    OVR_TEXTURE_FORMAT_R16_SNORM = VK_FORMAT_R16_SNORM, // 1-component, 16-bit signed normalized
    OVR_TEXTURE_FORMAT_R16G16_SNORM =
        VK_FORMAT_R16G16_SNORM, // 2-component, 16-bit signed normalized
    OVR_TEXTURE_FORMAT_R16G16B16A16_SNORM =
        VK_FORMAT_R16G16B16A16_SNORM, // 4-component, 16-bit signed normalized

    OVR_TEXTURE_FORMAT_R16_UINT = VK_FORMAT_R16_UINT, // 1-component, 16-bit unsigned integer
    OVR_TEXTURE_FORMAT_R16G16_UINT = VK_FORMAT_R16G16_UINT, // 2-component, 16-bit unsigned integer
    OVR_TEXTURE_FORMAT_R16G16B16A16_UINT =
        VK_FORMAT_R16G16B16A16_UINT, // 4-component, 16-bit unsigned integer

    OVR_TEXTURE_FORMAT_R16_SINT = VK_FORMAT_R16_SINT, // 1-component, 16-bit signed integer
    OVR_TEXTURE_FORMAT_R16G16_SINT = VK_FORMAT_R16G16_SINT, // 2-component, 16-bit signed integer
    OVR_TEXTURE_FORMAT_R16G16B16A16_SINT =
        VK_FORMAT_R16G16B16A16_SINT, // 4-component, 16-bit signed integer

    OVR_TEXTURE_FORMAT_R16_SFLOAT = VK_FORMAT_R16_SFLOAT, // 1-component, 16-bit floating-point
    OVR_TEXTURE_FORMAT_R16G16_SFLOAT =
        VK_FORMAT_R16G16_SFLOAT, // 2-component, 16-bit floating-point
    OVR_TEXTURE_FORMAT_R16G16B16A16_SFLOAT =
        VK_FORMAT_R16G16B16A16_SFLOAT, // 4-component, 16-bit floating-point

    //
    // 32 bits per component
    //
    OVR_TEXTURE_FORMAT_R32_UINT = VK_FORMAT_R32_UINT, // 1-component, 32-bit unsigned integer
    OVR_TEXTURE_FORMAT_R32G32_UINT = VK_FORMAT_R32G32_UINT, // 2-component, 32-bit unsigned integer
    OVR_TEXTURE_FORMAT_R32G32B32A32_UINT =
        VK_FORMAT_R32G32B32A32_UINT, // 4-component, 32-bit unsigned integer

    OVR_TEXTURE_FORMAT_R32_SINT = VK_FORMAT_R32_SINT, // 1-component, 32-bit signed integer
    OVR_TEXTURE_FORMAT_R32G32_SINT = VK_FORMAT_R32G32_SINT, // 2-component, 32-bit signed integer
    OVR_TEXTURE_FORMAT_R32G32B32A32_SINT =
        VK_FORMAT_R32G32B32A32_SINT, // 4-component, 32-bit signed integer

    OVR_TEXTURE_FORMAT_R32_SFLOAT = VK_FORMAT_R32_SFLOAT, // 1-component, 32-bit floating-point
    OVR_TEXTURE_FORMAT_R32G32_SFLOAT =
        VK_FORMAT_R32G32_SFLOAT, // 2-component, 32-bit floating-point
    OVR_TEXTURE_FORMAT_R32G32B32A32_SFLOAT =
        VK_FORMAT_R32G32B32A32_SFLOAT, // 4-component, 32-bit floating-point

    //
    // S3TC/DXT/BC
    //
    OVR_TEXTURE_FORMAT_BC1_R8G8B8_UNORM =
        VK_FORMAT_BC1_RGB_UNORM_BLOCK, // 3-component, line through 3D space, unsigned normalized
    OVR_TEXTURE_FORMAT_BC1_R8G8B8A1_UNORM =
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK, // 4-component, line through 3D space plus 1-bit alpha,
                                        // unsigned normalized
    OVR_TEXTURE_FORMAT_BC2_R8G8B8A8_UNORM =
        VK_FORMAT_BC2_UNORM_BLOCK, // 4-component, line through 3D space plus line through 1D space,
                                   // unsigned normalized
    OVR_TEXTURE_FORMAT_BC3_R8G8B8A4_UNORM =
        VK_FORMAT_BC3_UNORM_BLOCK, // 4-component, line through 3D space plus 4-bit alpha, unsigned
                                   // normalized

    OVR_TEXTURE_FORMAT_BC1_R8G8B8_SRGB =
        VK_FORMAT_BC1_RGB_SRGB_BLOCK, // 3-component, line through 3D space, sRGB
    OVR_TEXTURE_FORMAT_BC1_R8G8B8A1_SRGB =
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK, // 4-component, line through 3D space plus 1-bit alpha, sRGB
    OVR_TEXTURE_FORMAT_BC2_R8G8B8A8_SRGB =
        VK_FORMAT_BC2_SRGB_BLOCK, // 4-component, line through 3D space plus line through 1D space,
                                  // sRGB
    OVR_TEXTURE_FORMAT_BC3_R8G8B8A4_SRGB =
        VK_FORMAT_BC3_SRGB_BLOCK, // 4-component, line through 3D space plus 4-bit alpha, sRGB

    OVR_TEXTURE_FORMAT_BC4_R8_UNORM =
        VK_FORMAT_BC4_UNORM_BLOCK, // 1-component, line through 1D space, unsigned normalized
    OVR_TEXTURE_FORMAT_BC5_R8G8_UNORM =
        VK_FORMAT_BC5_UNORM_BLOCK, // 2-component, two lines through 1D space, unsigned normalized

    OVR_TEXTURE_FORMAT_BC4_R8_SNORM =
        VK_FORMAT_BC4_SNORM_BLOCK, // 1-component, line through 1D space, signed normalized
    OVR_TEXTURE_FORMAT_BC5_R8G8_SNORM =
        VK_FORMAT_BC5_SNORM_BLOCK, // 2-component, two lines through 1D space, signed normalized

    //
    // ETC
    //
    OVR_TEXTURE_FORMAT_ETC2_R8G8B8_UNORM =
        VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, // 3-component ETC2, unsigned normalized
    OVR_TEXTURE_FORMAT_ETC2_R8G8B8A1_UNORM =
        VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, // 3-component with 1-bit alpha ETC2, unsigned
                                             // normalized
    OVR_TEXTURE_FORMAT_ETC2_R8G8B8A8_UNORM =
        VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, // 4-component ETC2, unsigned normalized

    OVR_TEXTURE_FORMAT_ETC2_R8G8B8_SRGB =
        VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK, // 3-component ETC2, sRGB
    OVR_TEXTURE_FORMAT_ETC2_R8G8B8A1_SRGB =
        VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK, // 3-component with 1-bit alpha ETC2, sRGB
    OVR_TEXTURE_FORMAT_ETC2_R8G8B8A8_SRGB =
        VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK, // 4-component ETC2, sRGB

    OVR_TEXTURE_FORMAT_EAC_R11_UNORM =
        VK_FORMAT_EAC_R11_UNORM_BLOCK, // 1-component ETC, line through 1D space, unsigned
                                       // normalized
    OVR_TEXTURE_FORMAT_EAC_R11G11_UNORM =
        VK_FORMAT_EAC_R11G11_UNORM_BLOCK, // 2-component ETC, two lines through 1D space, unsigned
                                          // normalized

    OVR_TEXTURE_FORMAT_EAC_R11_SNORM =
        VK_FORMAT_EAC_R11_SNORM_BLOCK, // 1-component ETC, line through 1D space, signed normalized
    OVR_TEXTURE_FORMAT_EAC_R11G11_SNORM =
        VK_FORMAT_EAC_R11G11_SNORM_BLOCK, // 2-component ETC, two lines through 1D space, signed
                                          // normalized

    //
    // ASTC
    //
    OVR_TEXTURE_FORMAT_ASTC_4x4_UNORM =
        VK_FORMAT_ASTC_4x4_UNORM_BLOCK, // 4-component ASTC, 4x4 blocks, unsigned normalized
    OVR_TEXTURE_FORMAT_ASTC_5x4_UNORM =
        VK_FORMAT_ASTC_5x4_UNORM_BLOCK, // 4-component ASTC, 5x4 blocks, unsigned normalized
    OVR_TEXTURE_FORMAT_ASTC_5x5_UNORM =
        VK_FORMAT_ASTC_5x5_UNORM_BLOCK, // 4-component ASTC, 5x5 blocks, unsigned normalized
    OVR_TEXTURE_FORMAT_ASTC_6x5_UNORM =
        VK_FORMAT_ASTC_6x5_UNORM_BLOCK, // 4-component ASTC, 6x5 blocks, unsigned normalized
    OVR_TEXTURE_FORMAT_ASTC_6x6_UNORM =
        VK_FORMAT_ASTC_6x6_UNORM_BLOCK, // 4-component ASTC, 6x6 blocks, unsigned normalized
    OVR_TEXTURE_FORMAT_ASTC_8x5_UNORM =
        VK_FORMAT_ASTC_8x5_UNORM_BLOCK, // 4-component ASTC, 8x5 blocks, unsigned normalized
    OVR_TEXTURE_FORMAT_ASTC_8x6_UNORM =
        VK_FORMAT_ASTC_8x6_UNORM_BLOCK, // 4-component ASTC, 8x6 blocks, unsigned normalized
    OVR_TEXTURE_FORMAT_ASTC_8x8_UNORM =
        VK_FORMAT_ASTC_8x8_UNORM_BLOCK, // 4-component ASTC, 8x8 blocks, unsigned normalized
    OVR_TEXTURE_FORMAT_ASTC_10x5_UNORM =
        VK_FORMAT_ASTC_10x5_UNORM_BLOCK, // 4-component ASTC, 10x5 blocks, unsigned normalized
    OVR_TEXTURE_FORMAT_ASTC_10x6_UNORM =
        VK_FORMAT_ASTC_10x6_UNORM_BLOCK, // 4-component ASTC, 10x6 blocks, unsigned normalized
    OVR_TEXTURE_FORMAT_ASTC_10x8_UNORM =
        VK_FORMAT_ASTC_10x8_UNORM_BLOCK, // 4-component ASTC, 10x8 blocks, unsigned normalized
    OVR_TEXTURE_FORMAT_ASTC_10x10_UNORM =
        VK_FORMAT_ASTC_10x10_UNORM_BLOCK, // 4-component ASTC, 10x10 blocks, unsigned normalized
    OVR_TEXTURE_FORMAT_ASTC_12x10_UNORM =
        VK_FORMAT_ASTC_12x10_UNORM_BLOCK, // 4-component ASTC, 12x10 blocks, unsigned normalized
    OVR_TEXTURE_FORMAT_ASTC_12x12_UNORM =
        VK_FORMAT_ASTC_12x12_UNORM_BLOCK, // 4-component ASTC, 12x12 blocks, unsigned normalized

    OVR_TEXTURE_FORMAT_ASTC_4x4_SRGB =
        VK_FORMAT_ASTC_4x4_SRGB_BLOCK, // 4-component ASTC, 4x4 blocks, sRGB
    OVR_TEXTURE_FORMAT_ASTC_5x4_SRGB =
        VK_FORMAT_ASTC_5x4_SRGB_BLOCK, // 4-component ASTC, 5x4 blocks, sRGB
    OVR_TEXTURE_FORMAT_ASTC_5x5_SRGB =
        VK_FORMAT_ASTC_5x5_SRGB_BLOCK, // 4-component ASTC, 5x5 blocks, sRGB
    OVR_TEXTURE_FORMAT_ASTC_6x5_SRGB =
        VK_FORMAT_ASTC_6x5_SRGB_BLOCK, // 4-component ASTC, 6x5 blocks, sRGB
    OVR_TEXTURE_FORMAT_ASTC_6x6_SRGB =
        VK_FORMAT_ASTC_6x6_SRGB_BLOCK, // 4-component ASTC, 6x6 blocks, sRGB
    OVR_TEXTURE_FORMAT_ASTC_8x5_SRGB =
        VK_FORMAT_ASTC_8x5_SRGB_BLOCK, // 4-component ASTC, 8x5 blocks, sRGB
    OVR_TEXTURE_FORMAT_ASTC_8x6_SRGB =
        VK_FORMAT_ASTC_8x6_SRGB_BLOCK, // 4-component ASTC, 8x6 blocks, sRGB
    OVR_TEXTURE_FORMAT_ASTC_8x8_SRGB =
        VK_FORMAT_ASTC_8x8_SRGB_BLOCK, // 4-component ASTC, 8x8 blocks, sRGB
    OVR_TEXTURE_FORMAT_ASTC_10x5_SRGB =
        VK_FORMAT_ASTC_10x5_SRGB_BLOCK, // 4-component ASTC, 10x5 blocks, sRGB
    OVR_TEXTURE_FORMAT_ASTC_10x6_SRGB =
        VK_FORMAT_ASTC_10x6_SRGB_BLOCK, // 4-component ASTC, 10x6 blocks, sRGB
    OVR_TEXTURE_FORMAT_ASTC_10x8_SRGB =
        VK_FORMAT_ASTC_10x8_SRGB_BLOCK, // 4-component ASTC, 10x8 blocks, sRGB
    OVR_TEXTURE_FORMAT_ASTC_10x10_SRGB =
        VK_FORMAT_ASTC_10x10_SRGB_BLOCK, // 4-component ASTC, 10x10 blocks, sRGB
    OVR_TEXTURE_FORMAT_ASTC_12x10_SRGB =
        VK_FORMAT_ASTC_12x10_SRGB_BLOCK, // 4-component ASTC, 12x10 blocks, sRGB
    OVR_TEXTURE_FORMAT_ASTC_12x12_SRGB =
        VK_FORMAT_ASTC_12x12_SRGB_BLOCK, // 4-component ASTC, 12x12 blocks, sRGB
} ovrVkTextureFormat;

typedef enum {
    OVR_TEXTURE_USAGE_UNDEFINED = 1 << 0,
    OVR_TEXTURE_USAGE_GENERAL = 1 << 1,
    OVR_TEXTURE_USAGE_TRANSFER_SRC = 1 << 2,
    OVR_TEXTURE_USAGE_TRANSFER_DST = 1 << 3,
    OVR_TEXTURE_USAGE_SAMPLED = 1 << 4,
    OVR_TEXTURE_USAGE_STORAGE = 1 << 5,
    OVR_TEXTURE_USAGE_COLOR_ATTACHMENT = 1 << 6,
    OVR_TEXTURE_USAGE_PRESENTATION = 1 << 7,
    OVR_TEXTURE_USAGE_FRAG_DENSITY = 1 << 8,
} ovrVkTextureUsage;

typedef unsigned int ovrVkTextureUsageFlags;

typedef enum {
    OVR_TEXTURE_WRAP_MODE_REPEAT,
    OVR_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE,
    OVR_TEXTURE_WRAP_MODE_CLAMP_TO_BORDER
} ovrVkTextureWrapMode;

typedef enum {
    OVR_TEXTURE_FILTER_NEAREST,
    OVR_TEXTURE_FILTER_LINEAR,
    OVR_TEXTURE_FILTER_BILINEAR
} ovrVkTextureFilter;

typedef struct {
    int width;
    int height;
    int depth;
    int layerCount;
    int mipCount;
    ovrSampleCount sampleCount;
    ovrVkTextureUsage usage;
    ovrVkTextureUsageFlags usageFlags;
    ovrVkTextureWrapMode wrapMode;
    ovrVkTextureFilter filter;
    float maxAnisotropy;
    VkFormat format;
    VkImageLayout imageLayout;
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkSampler sampler;
} ovrVkTexture;

void ovrVkTexture_UpdateSampler(ovrVkContext* context, ovrVkTexture* texture);

void ovrVkTexture_ChangeUsage(
    ovrVkContext* context,
    VkCommandBuffer cmdBuffer,
    ovrVkTexture* texture,
    const ovrVkTextureUsage usage);

bool ovrVkTexture_Create2D(
    ovrVkContext* context,
    ovrVkTexture* texture,
    const ovrVkTextureFormat format,
    const ovrSampleCount sampleCount,
    const int width,
    const int height,
    const int mipCount,
    const int numLayers,
    const ovrVkTextureUsageFlags usageFlags);

void ovrVkTexture_Destroy(ovrVkContext* context, ovrVkTexture* texture);

/*
================================================================================================================================

Indices and vertex attributes.

================================================================================================================================
*/

typedef unsigned short ovrTriangleIndex;

typedef struct {
    const ovrVkBuffer* buffer;
    ovrTriangleIndex* indexArray;
    int indexCount;
} ovrVkTriangleIndexArray;

typedef struct {
    int attributeFlag; // OVR_VERTEX_ATTRIBUTE_FLAG_
    size_t attributeOffset; // Offset in bytes to the pointer in ovrVkVertexAttributeArrays
    size_t attributeSize; // Size in bytes of a single attribute
    VkFormat attributeFormat; // Format of the attribute
    int locationCount; // Number of attribute locations
    const char* name; // Name in vertex program
} ovrVkVertexAttribute;

typedef struct {
    const ovrVkBuffer* buffer;
    const ovrVkVertexAttribute* layout;
    void* data;
    size_t dataSize;
    int vertexCount;
    int attribsFlags;
} ovrVkVertexAttributeArrays;

void ovrVkTriangleIndexArray_Alloc(
    ovrVkTriangleIndexArray* indices,
    const int indexCount,
    const unsigned short* data);

void ovrVkTriangleIndexArray_Free(ovrVkTriangleIndexArray* indices);

void ovrVkVertexAttributeArrays_Map(
    ovrVkVertexAttributeArrays* attribs,
    void* data,
    const size_t dataSize,
    const int vertexCount,
    const int attribsFlags);

void ovrVkVertexAttributeArrays_Alloc(
    ovrVkVertexAttributeArrays* attribs,
    const ovrVkVertexAttribute* layout,
    const int vertexCount,
    const int attribsFlags);

void ovrVkVertexAttributeArrays_Free(ovrVkVertexAttributeArrays* attribs);

/*
================================================================================================================================

Default vertex attribute layout.

================================================================================================================================
*/

typedef enum {
    OVR_VERTEX_ATTRIBUTE_FLAG_POSITION = 1 << 0, // vec3 vertexPosition
    OVR_VERTEX_ATTRIBUTE_FLAG_NORMAL = 1 << 1, // vec3 vertexNormal
    OVR_VERTEX_ATTRIBUTE_FLAG_TANGENT = 1 << 2, // vec3 vertexTangent
    OVR_VERTEX_ATTRIBUTE_FLAG_BINORMAL = 1 << 3, // vec3 vertexBinormal
    OVR_VERTEX_ATTRIBUTE_FLAG_COLOR = 1 << 4, // vec4 vertexColor
    OVR_VERTEX_ATTRIBUTE_FLAG_UV0 = 1 << 5, // vec2 vertexUv0
    OVR_VERTEX_ATTRIBUTE_FLAG_UV1 = 1 << 6, // vec2 vertexUv1
    OVR_VERTEX_ATTRIBUTE_FLAG_UV2 = 1 << 7, // vec2 vertexUv2
    OVR_VERTEX_ATTRIBUTE_FLAG_JOINT_INDICES = 1 << 8, // vec4 jointIndices
    OVR_VERTEX_ATTRIBUTE_FLAG_JOINT_WEIGHTS = 1 << 9, // vec4 jointWeights
    OVR_VERTEX_ATTRIBUTE_FLAG_TRANSFORM = 1
        << 10 // mat4 vertexTransform (NOTE this mat4 takes up 4 attribute locations)
} ovrDefaultVertexAttributeFlags;

typedef struct {
    ovrVkVertexAttributeArrays base;
    ovrVector3f* position;
    ovrVector3f* normal;
    ovrVector3f* tangent;
    ovrVector3f* binormal;
    ovrVector4f* color;
    ovrVector2f* uv0;
    ovrVector2f* uv1;
    ovrVector2f* uv2;
    ovrVector4f* jointIndices;
    ovrVector4f* jointWeights;
    ovrMatrix4f* transform;
} ovrDefaultVertexAttributeArrays;

static const ovrVkVertexAttribute DefaultVertexAttributeLayout[] = {
    {OVR_VERTEX_ATTRIBUTE_FLAG_POSITION,
     OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, position),
     SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, position[0]),
     VK_FORMAT_R32G32B32_SFLOAT,
     1,
     "vertexPosition"},
    {OVR_VERTEX_ATTRIBUTE_FLAG_NORMAL,
     OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, normal),
     SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, normal[0]),
     VK_FORMAT_R32G32B32_SFLOAT,
     1,
     "vertexNormal"},
    {OVR_VERTEX_ATTRIBUTE_FLAG_TANGENT,
     OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, tangent),
     SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, tangent[0]),
     VK_FORMAT_R32G32B32_SFLOAT,
     1,
     "vertexTangent"},
    {OVR_VERTEX_ATTRIBUTE_FLAG_BINORMAL,
     OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, binormal),
     SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, binormal[0]),
     VK_FORMAT_R32G32B32_SFLOAT,
     1,
     "vertexBinormal"},
    {OVR_VERTEX_ATTRIBUTE_FLAG_COLOR,
     OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, color),
     SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, color[0]),
     VK_FORMAT_R32G32B32A32_SFLOAT,
     1,
     "vertexColor"},
    {OVR_VERTEX_ATTRIBUTE_FLAG_UV0,
     OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, uv0),
     SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, uv0[0]),
     VK_FORMAT_R32G32_SFLOAT,
     1,
     "vertexUv0"},
    {OVR_VERTEX_ATTRIBUTE_FLAG_UV1,
     OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, uv1),
     SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, uv1[0]),
     VK_FORMAT_R32G32_SFLOAT,
     1,
     "vertexUv1"},
    {OVR_VERTEX_ATTRIBUTE_FLAG_UV2,
     OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, uv2),
     SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, uv2[0]),
     VK_FORMAT_R32G32_SFLOAT,
     1,
     "vertexUv2"},
    {OVR_VERTEX_ATTRIBUTE_FLAG_JOINT_INDICES,
     OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, jointIndices),
     SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, jointIndices[0]),
     VK_FORMAT_R32G32B32A32_SFLOAT,
     1,
     "vertexJointIndices"},
    {OVR_VERTEX_ATTRIBUTE_FLAG_JOINT_WEIGHTS,
     OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, jointWeights),
     SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, jointWeights[0]),
     VK_FORMAT_R32G32B32A32_SFLOAT,
     1,
     "vertexJointWeights"},
    {OVR_VERTEX_ATTRIBUTE_FLAG_TRANSFORM,
     OFFSETOF_MEMBER(ovrDefaultVertexAttributeArrays, transform),
     SIZEOF_MEMBER(ovrDefaultVertexAttributeArrays, transform[0]),
     VK_FORMAT_R32G32B32A32_SFLOAT,
     4,
     "vertexTransform"},
    {0, 0, 0, (VkFormat)0, 0, ""}};

/*
================================================================================================================================

Geometry.

For optimal performance geometry should only be created at load time, not at runtime.
The vertex, index and instance buffers are placed in device memory for optimal performance.
The vertex attributes are not packed. Each attribute is stored in a separate array for
optimal binning on tiling GPUs that only transform the vertex position for the binning pass.
Storing each attribute in a saparate array is preferred even on immediate-mode GPUs to avoid
wasting cache space for attributes that are not used by a particular vertex shader.

================================================================================================================================
*/

typedef struct {
    const ovrVkVertexAttribute* layout;
    int vertexAttribsFlags;
    int instanceAttribsFlags;
    int vertexCount;
    int instanceCount;
    int indexCount;
    ovrVkBuffer vertexBuffer;
    ovrVkBuffer instanceBuffer;
    ovrVkBuffer indexBuffer;
} ovrVkGeometry;

void ovrVkGeometry_Create(
    ovrVkContext* context,
    ovrVkGeometry* geometry,
    const ovrVkVertexAttributeArrays* attribs,
    const ovrVkTriangleIndexArray* indices);

void ovrVkGeometry_Destroy(ovrVkDevice* device, ovrVkGeometry* geometry);

void ovrVkGeometry_AddInstanceAttributes(
    ovrVkContext* context,
    ovrVkGeometry* geometry,
    const int numInstances,
    const int instanceAttribsFlags);

/*
================================================================================================================================

Vulkan render pass.

A render pass encapsulates a sequence of graphics commands that can be executed in a single tiling
pass. For optimal performance a render pass should only be created at load time, not at runtime.
Render passes cannot overlap and cannot be nested.

================================================================================================================================
*/

#define EXPLICIT_RESOLVE 0

typedef enum {
    OVR_RENDERPASS_TYPE_INLINE,
    OVR_RENDERPASS_TYPE_SECONDARY_COMMAND_BUFFERS
} ovrVkRenderPassType;

typedef enum {
    OVR_RENDERPASS_FLAG_CLEAR_COLOR_BUFFER = 1 << 0,
    OVR_RENDERPASS_FLAG_CLEAR_DEPTH_BUFFER = 1 << 1,
    OVR_RENDERPASS_FLAG_INCLUDE_FRAG_DENSITY = 1 << 2,
} ovrVkRenderPassFlags;

typedef struct {
    ovrVkRenderPassType type;
    int flags;
    ovrSurfaceColorFormat colorFormat;
    ovrSurfaceDepthFormat depthFormat;
    ovrSampleCount sampleCount;
    VkFormat internalColorFormat;
    VkFormat internalDepthFormat;
    VkFormat internalFragmentDensityFormat;
    VkRenderPass renderPass;
    ovrVector4f clearColor;
} ovrVkRenderPass;

bool ovrVkRenderPass_Create(
    ovrVkContext* context,
    ovrVkRenderPass* renderPass,
    const ovrSurfaceColorFormat colorFormat,
    const ovrSurfaceDepthFormat depthFormat,
    const ovrSampleCount sampleCount,
    const ovrVkRenderPassType type,
    const int flags,
    const ovrVector4f* clearColor,
    bool isMultiview);

void ovrVkRenderPass_Destroy(ovrVkContext* context, ovrVkRenderPass* renderPass);

/*
================================================================================================================================

Vulkan framebuffer.

A framebuffer encapsulates a buffered set of textures.

For optimal performance a framebuffer should only be created at load time, not at runtime.

================================================================================================================================
*/

typedef struct {
    ovrVkTexture* colorTextures;
    ovrVkTexture* fragmentDensityTextures;
    ovrVkTexture renderTexture;
    ovrVkDepthBuffer depthBuffer;
    VkFramebuffer* framebuffers;
    ovrVkRenderPass* renderPass;
    int width;
    int height;
    int numLayers;
    int numBuffers;
    int currentBuffer;
    int currentLayer;
} ovrVkFramebuffer;

ovrScreenRect ovrVkFramebuffer_GetRect(const ovrVkFramebuffer* framebuffer);

int ovrVkFramebuffer_GetBufferCount(const ovrVkFramebuffer* framebuffer);

ovrVkTexture* ovrVkFramebuffer_GetColorTexture(const ovrVkFramebuffer* framebuffer);

/*
================================================================================================================================

Vulkan program parms and layout.

================================================================================================================================
*/

#define MAX_PROGRAM_PARMS 16

typedef enum {
    OVR_PROGRAM_STAGE_FLAG_VERTEX = 1 << 0,
    OVR_PROGRAM_STAGE_FLAG_FRAGMENT = 1 << 1,
    OVR_PROGRAM_STAGE_MAX = 2
} ovrVkProgramStageFlags;

typedef enum {
    OVR_PROGRAM_PARM_TYPE_TEXTURE_SAMPLED, // texture plus sampler bound together		(GLSL:
                                           // sampler*, isampler*, usampler*)
    OVR_PROGRAM_PARM_TYPE_TEXTURE_STORAGE, // not sampled, direct read-write storage	(GLSL:
                                           // image*, iimage*, uimage*)
    OVR_PROGRAM_PARM_TYPE_BUFFER_UNIFORM, // read-only uniform buffer					(GLSL:
                                          // uniform)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_INT, // int										(GLSL: int)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_INT_VECTOR2, // int[2] (GLSL: ivec2)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_INT_VECTOR3, // int[3] (GLSL: ivec3)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_INT_VECTOR4, // int[4] (GLSL: ivec4)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT, // float									(GLSL:
                                               // float)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_VECTOR2, // float[2] (GLSL: vec2)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_VECTOR3, // float[3] (GLSL: vec3)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_VECTOR4, // float[4] (GLSL: vec4)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX2X2, // float[2][2]
                                                         // (GLSL: mat2x2 or mat2)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX2X3, // float[2][3] (GLSL: mat2x3)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX2X4, // float[2][4] (GLSL: mat2x4)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX3X2, // float[3][2] (GLSL: mat3x2)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX3X3, // float[3][3]
                                                         // (GLSL: mat3x3 or mat3)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX3X4, // float[3][4] (GLSL: mat3x4)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX4X2, // float[4][2] (GLSL: mat4x2)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX4X3, // float[4][3] (GLSL: mat4x3)
    OVR_PROGRAM_PARM_TYPE_PUSH_CONSTANT_FLOAT_MATRIX4X4, // float[4][4]
                                                         // (GLSL: mat4x4 or mat4)
    OVR_PROGRAM_PARM_TYPE_MAX
} ovrVkProgramParmType;

typedef enum {
    OVR_PROGRAM_PARM_ACCESS_READ_ONLY,
    OVR_PROGRAM_PARM_ACCESS_WRITE_ONLY,
    OVR_PROGRAM_PARM_ACCESS_READ_WRITE
} ovrVkProgramParmAccess;

typedef struct {
    int stageFlags; // vertex, fragment
    ovrVkProgramParmType type; // texture, buffer or push constant
    ovrVkProgramParmAccess access; // read and/or write
    int index; // index into ovrVkProgramParmState::parms
    const char* name; // GLSL name
    int binding; // Vulkan texture/buffer binding, or push constant offset
                 // Note that all Vulkan bindings must be unique per descriptor set across all
                 // stages of the pipeline. Note that all Vulkan push constant ranges must be unique
                 // across all stages of the pipeline.
} ovrVkProgramParm;

typedef struct {
    int numParms;
    const ovrVkProgramParm* parms;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    int offsetForIndex[MAX_PROGRAM_PARMS]; // push constant offsets into ovrVkProgramParmState::data
                                           // based on ovrVkProgramParm::index
    const ovrVkProgramParm* bindings[MAX_PROGRAM_PARMS]; // descriptor bindings
    const ovrVkProgramParm* pushConstants[MAX_PROGRAM_PARMS]; // push constants
    int numBindings;
    int numPushConstants;
    unsigned int hash;
} ovrVkProgramParmLayout;

int ovrVkProgramParm_GetPushConstantSize(ovrVkProgramParmType type);

void ovrVkProgramParmLayout_Create(
    ovrVkContext* context,
    ovrVkProgramParmLayout* layout,
    const ovrVkProgramParm* parms,
    const int numParms);

void ovrVkProgramParmLayout_Destroy(ovrVkContext* context, ovrVkProgramParmLayout* layout);

/*
================================================================================================================================

Vulkan graphics program.

A graphics program encapsulates a vertex and fragment program that are used to render geometry.
For optimal performance a graphics program should only be created at load time, not at runtime.


================================================================================================================================
*/

typedef struct {
    VkShaderModule vertexShaderModule;
    VkShaderModule fragmentShaderModule;
    VkPipelineShaderStageCreateInfo pipelineStages[2];
    ovrVkProgramParmLayout parmLayout;
    int vertexAttribsFlags;
} ovrVkGraphicsProgram;

bool ovrVkGraphicsProgram_Create(
    ovrVkContext* context,
    ovrVkGraphicsProgram* program,
    const void* vertexSourceData,
    const size_t vertexSourceSize,
    const void* fragmentSourceData,
    const size_t fragmentSourceSize,
    const ovrVkProgramParm* parms,
    const int numParms,
    const ovrVkVertexAttribute* vertexLayout,
    const int vertexAttribsFlags);

void ovrVkGraphicsProgram_Destroy(ovrVkContext* context, ovrVkGraphicsProgram* program);

/*
================================================================================================================================

Vulkan graphics pipeline.

A graphics pipeline encapsulates the geometry, program and ROP state that is used to render.
For optimal performance a graphics pipeline should only be created at load time, not at runtime.
The vertex attribute locations are assigned here, when both the geometry and program are known,
to avoid binding vertex attributes that are not used by the vertex shader, and to avoid binding
to a discontinuous set of vertex attribute locations.

================================================================================================================================
*/

typedef enum {
    OVR_FRONT_FACE_COUNTER_CLOCKWISE = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    OVR_FRONT_FACE_CLOCKWISE = VK_FRONT_FACE_CLOCKWISE
} ovrVkFrontFace;

typedef enum {
    OVR_CULL_MODE_NONE = 0,
    OVR_CULL_MODE_FRONT = VK_CULL_MODE_FRONT_BIT,
    OVR_CULL_MODE_BACK = VK_CULL_MODE_BACK_BIT
} ovrVkCullMode;

typedef enum {
    OVR_COMPARE_OP_NEVER = VK_COMPARE_OP_NEVER,
    OVR_COMPARE_OP_LESS = VK_COMPARE_OP_LESS,
    OVR_COMPARE_OP_EQUAL = VK_COMPARE_OP_EQUAL,
    OVR_COMPARE_OP_LESS_OR_EQUAL = VK_COMPARE_OP_LESS_OR_EQUAL,
    OVR_COMPARE_OP_GREATER = VK_COMPARE_OP_GREATER,
    OVR_COMPARE_OP_NOT_EQUAL = VK_COMPARE_OP_NOT_EQUAL,
    OVR_COMPARE_OP_GREATER_OR_EQUAL = VK_COMPARE_OP_GREATER_OR_EQUAL,
    OVR_COMPARE_OP_ALWAYS = VK_COMPARE_OP_ALWAYS
} ovrVkCompareOp;

typedef enum {
    OVR_BLEND_OP_ADD = VK_BLEND_OP_ADD,
    OVR_BLEND_OP_SUBTRACT = VK_BLEND_OP_SUBTRACT,
    OVR_BLEND_OP_REVERSE_SUBTRACT = VK_BLEND_OP_REVERSE_SUBTRACT,
    OVR_BLEND_OP_MIN = VK_BLEND_OP_MIN,
    OVR_BLEND_OP_MAX = VK_BLEND_OP_MAX
} ovrVkBlendOp;

typedef enum {
    OVR_BLEND_FACTOR_ZERO = VK_BLEND_FACTOR_ZERO,
    OVR_BLEND_FACTOR_ONE = VK_BLEND_FACTOR_ONE,
    OVR_BLEND_FACTOR_SRC_COLOR = VK_BLEND_FACTOR_SRC_COLOR,
    OVR_BLEND_FACTOR_ONE_MINUS_SRC_COLOR = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    OVR_BLEND_FACTOR_DST_COLOR = VK_BLEND_FACTOR_DST_COLOR,
    OVR_BLEND_FACTOR_ONE_MINUS_DST_COLOR = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    OVR_BLEND_FACTOR_SRC_ALPHA = VK_BLEND_FACTOR_SRC_ALPHA,
    OVR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    OVR_BLEND_FACTOR_DST_ALPHA = VK_BLEND_FACTOR_DST_ALPHA,
    OVR_BLEND_FACTOR_ONE_MINUS_DST_ALPHA = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    OVR_BLEND_FACTOR_CONSTANT_COLOR = VK_BLEND_FACTOR_CONSTANT_COLOR,
    OVR_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    OVR_BLEND_FACTOR_CONSTANT_ALPHA = VK_BLEND_FACTOR_CONSTANT_ALPHA,
    OVR_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
    OVR_BLEND_FACTOR_SRC_ALPHA_SATURATE = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE
} ovrVkBlendFactor;

typedef struct {
    bool blendEnable;
    bool redWriteEnable;
    bool blueWriteEnable;
    bool greenWriteEnable;
    bool alphaWriteEnable;
    bool depthTestEnable;
    bool depthWriteEnable;
    ovrVkFrontFace frontFace;
    ovrVkCullMode cullMode;
    ovrVkCompareOp depthCompare;
    ovrVector4f blendColor;
    ovrVkBlendOp blendOpColor;
    ovrVkBlendFactor blendSrcColor;
    ovrVkBlendFactor blendDstColor;
    ovrVkBlendOp blendOpAlpha;
    ovrVkBlendFactor blendSrcAlpha;
    ovrVkBlendFactor blendDstAlpha;
} ovrVkRasterOperations;

typedef struct {
    ovrVkRasterOperations rop;
    const ovrVkRenderPass* renderPass;
    const ovrVkGraphicsProgram* program;
    const ovrVkGeometry* geometry;
} ovrVkGraphicsPipelineParms;

#define MAX_VERTEX_ATTRIBUTES 16

typedef struct {
    ovrVkRasterOperations rop;
    const ovrVkGraphicsProgram* program;
    const ovrVkGeometry* geometry;
    int vertexAttributeCount;
    int vertexBindingCount;
    int firstInstanceBinding;
    VkVertexInputAttributeDescription vertexAttributes[MAX_VERTEX_ATTRIBUTES];
    VkVertexInputBindingDescription vertexBindings[MAX_VERTEX_ATTRIBUTES];
    VkDeviceSize vertexBindingOffsets[MAX_VERTEX_ATTRIBUTES];
    VkPipelineVertexInputStateCreateInfo vertexInputState;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
    VkPipeline pipeline;
} ovrVkGraphicsPipeline;

void ovrVkGraphicsPipelineParms_Init(ovrVkGraphicsPipelineParms* parms);

bool ovrVkGraphicsPipeline_Create(
    ovrVkContext* context,
    ovrVkGraphicsPipeline* pipeline,
    const ovrVkGraphicsPipelineParms* parms);

void ovrVkGraphicsPipeline_Destroy(ovrVkContext* context, ovrVkGraphicsPipeline* pipeline);

/*
================================================================================================================================

Vulkan fence.

A fence is used to notify completion of a command buffer.
For optimal performance a fence should only be created at load time, not at runtime.

================================================================================================================================
*/

typedef struct {
    VkFence fence;
    bool submitted;
} ovrVkFence;

void ovrVkFence_Create(ovrVkContext* context, ovrVkFence* fence);

void ovrVkFence_Destroy(ovrVkContext* context, ovrVkFence* fence);

void ovrVkFence_Submit(ovrVkContext* context, ovrVkFence* fence);

bool ovrVkFence_IsSignalled(ovrVkContext* context, ovrVkFence* fence);

/*
================================================================================================================================

Vulkan program parm state.

================================================================================================================================
*/

#define MAX_SAVED_PUSH_CONSTANT_BYTES 512
typedef struct {
    const void* parms[MAX_PROGRAM_PARMS];
    unsigned char data[MAX_SAVED_PUSH_CONSTANT_BYTES];
} ovrVkProgramParmState;

void ovrVkProgramParmState_SetParm(
    ovrVkProgramParmState* parmState,
    const ovrVkProgramParmLayout* parmLayout,
    const int index,
    const ovrVkProgramParmType parmType,
    const void* pointer);

const void* ovrVkProgramParmState_NewPushConstantData(
    const ovrVkProgramParmLayout* newLayout,
    const int newPushConstantIndex,
    const ovrVkProgramParmState* newParmState,
    const ovrVkProgramParmLayout* oldLayout,
    const int oldPushConstantIndex,
    const ovrVkProgramParmState* oldParmState,
    const bool force);

bool ovrVkProgramParmState_DescriptorsMatch(
    const ovrVkProgramParmLayout* layout1,
    const ovrVkProgramParmState* parmState1,
    const ovrVkProgramParmLayout* layout2,
    const ovrVkProgramParmState* parmState2);

/*
================================================================================================================================

Vulkan graphics commands.

A graphics command encapsulates all GPU state associated with a single draw call.
The pointers passed in as parameters are expected to point to unique objects that persist
at least past the submission of the command buffer into which the graphics command is
submitted. Because pointers are maintained as state, DO NOT use pointers to local
variables that will go out of scope before the command buffer is submitted.

================================================================================================================================
*/

typedef struct {
    const ovrVkGraphicsPipeline* pipeline;
    const ovrVkBuffer*
        vertexBuffer; // vertex buffer returned by ovrVkCommandBuffer_MapVertexAttributes
    const ovrVkBuffer*
        instanceBuffer; // instance buffer returned by ovrVkCommandBuffer_MapInstanceAttributes
    ovrVkProgramParmState parmState;
    int numInstances;
} ovrVkGraphicsCommand;

void ovrVkGraphicsCommand_Init(ovrVkGraphicsCommand* command);

void ovrVkGraphicsCommand_SetPipeline(
    ovrVkGraphicsCommand* command,
    const ovrVkGraphicsPipeline* pipeline);

void ovrVkGraphicsCommand_SetParmBufferUniform(
    ovrVkGraphicsCommand* command,
    const int index,
    const ovrVkBuffer* buffer);

void ovrVkGraphicsCommand_SetNumInstances(ovrVkGraphicsCommand* command, const int numInstances);

/*
================================================================================================================================

Vulkan pipeline resources.

Resources, like texture and uniform buffer descriptions, that are used by a graphics or compute
pipeline.

================================================================================================================================
*/

typedef struct ovrVkPipelineResources_s {
    struct ovrVkPipelineResources_s* next;
    int unusedCount; // Number of frames these resources have not been used.
    const ovrVkProgramParmLayout* parmLayout;
    ovrVkProgramParmState parms;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
} ovrVkPipelineResources;

void ovrVkPipelineResources_Create(
    ovrVkContext* context,
    ovrVkPipelineResources* resources,
    const ovrVkProgramParmLayout* parmLayout,
    const ovrVkProgramParmState* parms);

void ovrVkPipelineResources_Destroy(ovrVkContext* context, ovrVkPipelineResources* resources);

/*
================================================================================================================================

Vulkan command buffer.

A command buffer is used to record graphics and compute commands.
For optimal performance a command buffer should only be created at load time, not at runtime.
When a command is submitted, the state of the command is compared with the currently saved state,
and only the state that has changed translates into graphics API function calls.

================================================================================================================================
*/

typedef enum {
    OVR_BUFFER_UNMAP_TYPE_USE_ALLOCATED, // use the newly allocated (host visible) buffer
    OVR_BUFFER_UNMAP_TYPE_COPY_BACK // copy back to the original buffer
} ovrVkBufferUnmapType;

typedef enum {
    OVR_COMMAND_BUFFER_TYPE_PRIMARY,
    OVR_COMMAND_BUFFER_TYPE_SECONDARY,
    OVR_COMMAND_BUFFER_TYPE_SECONDARY_CONTINUE_RENDER_PASS
} ovrVkCommandBufferType;

typedef struct {
    ovrVkCommandBufferType type;
    int numBuffers;
    int currentBuffer;
    VkCommandBuffer* cmdBuffers;
    ovrVkContext* context;
    ovrVkFence* fences;
    ovrVkBuffer** mappedBuffers;
    ovrVkBuffer** oldMappedBuffers;
    ovrVkPipelineResources** pipelineResources;
    ovrVkGraphicsCommand currentGraphicsState;
    ovrVkFramebuffer* currentFramebuffer;
    ovrVkRenderPass* currentRenderPass;
} ovrVkCommandBuffer;

#define MAX_VERTEX_BUFFER_UNUSED_COUNT 16
#define MAX_PIPELINE_RESOURCES_UNUSED_COUNT 16

void ovrVkCommandBuffer_Create(
    ovrVkContext* context,
    ovrVkCommandBuffer* commandBuffer,
    const ovrVkCommandBufferType type,
    const int numBuffers);

void ovrVkCommandBuffer_Destroy(ovrVkContext* context, ovrVkCommandBuffer* commandBuffer);

void ovrVkCommandBuffer_ManageBuffers(ovrVkCommandBuffer* commandBuffer);

void ovrVkCommandBuffer_BeginPrimary(ovrVkCommandBuffer* commandBuffer);

void ovrVkCommandBuffer_EndPrimary(ovrVkCommandBuffer* commandBuffer);

ovrVkFence* ovrVkCommandBuffer_SubmitPrimary(ovrVkCommandBuffer* commandBuffer);

void ovrVkCommandBuffer_ChangeTextureUsage(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkTexture* texture,
    const ovrVkTextureUsage usage);

void ovrVkCommandBuffer_BeginFramebuffer(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkFramebuffer* framebuffer,
    const int arrayLayer,
    const ovrVkTextureUsage usage);

void ovrVkCommandBuffer_EndFramebuffer(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkFramebuffer* framebuffer,
    const int arrayLayer,
    const ovrVkTextureUsage usage);

void ovrVkCommandBuffer_BeginRenderPass(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkRenderPass* renderPass,
    ovrVkFramebuffer* framebuffer,
    const ovrScreenRect* rect);

void ovrVkCommandBuffer_EndRenderPass(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkRenderPass* renderPass);

void ovrVkCommandBuffer_SetViewport(ovrVkCommandBuffer* commandBuffer, const ovrScreenRect* rect);

void ovrVkCommandBuffer_SetScissor(ovrVkCommandBuffer* commandBuffer, const ovrScreenRect* rect);

void ovrVkCommandBuffer_UpdateProgramParms(
    ovrVkCommandBuffer* commandBuffer,
    const ovrVkProgramParmLayout* newLayout,
    const ovrVkProgramParmLayout* oldLayout,
    const ovrVkProgramParmState* newParmState,
    const ovrVkProgramParmState* oldParmState,
    VkPipelineBindPoint bindPoint);

void ovrVkCommandBuffer_SubmitGraphicsCommand(
    ovrVkCommandBuffer* commandBuffer,
    const ovrVkGraphicsCommand* command);

ovrVkBuffer*
ovrVkCommandBuffer_MapBuffer(ovrVkCommandBuffer* commandBuffer, ovrVkBuffer* buffer, void** data);

void ovrVkCommandBuffer_UnmapBuffer(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkBuffer* buffer,
    ovrVkBuffer* mappedBuffer,
    const ovrVkBufferUnmapType type);

ovrVkBuffer* ovrVkCommandBuffer_MapInstanceAttributes(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkGeometry* geometry,
    ovrVkVertexAttributeArrays* attribs);

void ovrVkCommandBuffer_UnmapInstanceAttributes(
    ovrVkCommandBuffer* commandBuffer,
    ovrVkGeometry* geometry,
    ovrVkBuffer* mappedInstanceBuffer,
    const ovrVkBufferUnmapType type);

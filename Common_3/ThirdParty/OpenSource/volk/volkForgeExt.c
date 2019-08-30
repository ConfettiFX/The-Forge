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
#include "volk.h"
#include <vulkan/vk_layer.h>
#include "volkForgeExt.h"

#if defined(VK_USE_DISPATCH_TABLES)
extern VkLayerInstanceDispatchTable* pExternInstanceDispatchTable;
extern VkLayerDispatchTable* pExternDeviceDispatchTable;
extern VkInstance pExternVkInstance;
extern VkPhysicalDevice pExternVkGPU;
extern VkDevice pExternVkDevice;

PFN_vkSetDeviceLoaderData pfnDevInit = NULL;

#ifdef __cplusplus
extern "C" {
#endif

VkResult volkInitializeWithDispatchTables(Renderer* pRenderer)
{
	// Validate data
	if (!pRenderer ||
		!pExternInstanceDispatchTable ||
		!pExternDeviceDispatchTable ||
		!pExternVkInstance ||
		!pExternVkGPU ||
		!pExternVkDevice)
		return VK_ERROR_INITIALIZATION_FAILED;

	// Ensure pRenderer doesn't already have a VK instance/device
	if (pRenderer->pVkInstance ||
		pRenderer->pVkDevice ||
		pRenderer->pVkActiveGPU)
		return VK_ERROR_INITIALIZATION_FAILED;

	// Fill out pRenderer
	pRenderer->pVkInstance = pExternVkInstance;
	pRenderer->pVkDevice = pExternVkDevice;
	pRenderer->pVkActiveGPU = pExternVkGPU;

	// Fill out VK funcs
#if defined(VK_VERSION_1_0)
// ---- Core 1_0 commands
	vkCreateInstance = pExternInstanceDispatchTable->CreateInstance;
	vkDestroyInstance = pExternInstanceDispatchTable->DestroyInstance;
	vkEnumeratePhysicalDevices = pExternInstanceDispatchTable->EnumeratePhysicalDevices;
	vkGetPhysicalDeviceFeatures = pExternInstanceDispatchTable->GetPhysicalDeviceFeatures;
	vkGetPhysicalDeviceFormatProperties = pExternInstanceDispatchTable->GetPhysicalDeviceFormatProperties;
	vkGetPhysicalDeviceImageFormatProperties = pExternInstanceDispatchTable->GetPhysicalDeviceImageFormatProperties;
	vkGetPhysicalDeviceProperties = pExternInstanceDispatchTable->GetPhysicalDeviceProperties;
	vkGetPhysicalDeviceQueueFamilyProperties = pExternInstanceDispatchTable->GetPhysicalDeviceQueueFamilyProperties;
	vkGetPhysicalDeviceMemoryProperties = pExternInstanceDispatchTable->GetPhysicalDeviceMemoryProperties;
	vkGetInstanceProcAddr = pExternInstanceDispatchTable->GetInstanceProcAddr;
	vkCreateDevice = pExternInstanceDispatchTable->CreateDevice;
	vkEnumerateInstanceExtensionProperties = pExternInstanceDispatchTable->EnumerateInstanceExtensionProperties;
	vkEnumerateDeviceExtensionProperties = pExternInstanceDispatchTable->EnumerateDeviceExtensionProperties;
	vkEnumerateInstanceLayerProperties = pExternInstanceDispatchTable->EnumerateInstanceLayerProperties;
	vkEnumerateDeviceLayerProperties = pExternInstanceDispatchTable->EnumerateDeviceLayerProperties;
	vkGetPhysicalDeviceSparseImageFormatProperties = pExternInstanceDispatchTable->GetPhysicalDeviceSparseImageFormatProperties;
#endif

#if defined(VK_VERSION_1_1)
	// ---- Core 1_1 commands
	vkEnumerateInstanceVersion = pExternInstanceDispatchTable->EnumerateInstanceVersion;
	vkEnumeratePhysicalDeviceGroups = pExternInstanceDispatchTable->EnumeratePhysicalDeviceGroups;
	vkGetPhysicalDeviceFeatures2 = pExternInstanceDispatchTable->GetPhysicalDeviceFeatures2;
	vkGetPhysicalDeviceProperties2 = pExternInstanceDispatchTable->GetPhysicalDeviceProperties2;
	vkGetPhysicalDeviceFormatProperties2 = pExternInstanceDispatchTable->GetPhysicalDeviceFormatProperties2;
	vkGetPhysicalDeviceImageFormatProperties2 = pExternInstanceDispatchTable->GetPhysicalDeviceImageFormatProperties2;
	vkGetPhysicalDeviceQueueFamilyProperties2 = pExternInstanceDispatchTable->GetPhysicalDeviceQueueFamilyProperties2;
	vkGetPhysicalDeviceMemoryProperties2 = pExternInstanceDispatchTable->GetPhysicalDeviceMemoryProperties2;
	vkGetPhysicalDeviceSparseImageFormatProperties2 = pExternInstanceDispatchTable->GetPhysicalDeviceSparseImageFormatProperties2;
	vkGetPhysicalDeviceExternalBufferProperties = pExternInstanceDispatchTable->GetPhysicalDeviceExternalBufferProperties;
	vkGetPhysicalDeviceExternalFenceProperties = pExternInstanceDispatchTable->GetPhysicalDeviceExternalFenceProperties;
	vkGetPhysicalDeviceExternalSemaphoreProperties = pExternInstanceDispatchTable->GetPhysicalDeviceExternalSemaphoreProperties;
#endif

#if defined(VK_KHR_surface)
	// ---- VK_KHR_surface extension commands
	vkDestroySurfaceKHR = pExternInstanceDispatchTable->DestroySurfaceKHR;
	vkGetPhysicalDeviceSurfaceSupportKHR = pExternInstanceDispatchTable->GetPhysicalDeviceSurfaceSupportKHR;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR = pExternInstanceDispatchTable->GetPhysicalDeviceSurfaceCapabilitiesKHR;
	vkGetPhysicalDeviceSurfaceFormatsKHR = pExternInstanceDispatchTable->GetPhysicalDeviceSurfaceFormatsKHR;
	vkGetPhysicalDeviceSurfacePresentModesKHR = pExternInstanceDispatchTable->GetPhysicalDeviceSurfacePresentModesKHR;
#endif

#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
	// ---- VK_KHR_swapchain extension commands
	vkGetPhysicalDevicePresentRectanglesKHR = pExternInstanceDispatchTable->GetPhysicalDevicePresentRectanglesKHR;
#endif

#if defined(VK_KHR_display)
	// ---- VK_KHR_display extension commands
	vkGetPhysicalDeviceDisplayPropertiesKHR = pExternInstanceDispatchTable->GetPhysicalDeviceDisplayPropertiesKHR;
	vkGetPhysicalDeviceDisplayPlanePropertiesKHR = pExternInstanceDispatchTable->GetPhysicalDeviceDisplayPlanePropertiesKHR;
	vkGetDisplayPlaneSupportedDisplaysKHR = pExternInstanceDispatchTable->GetDisplayPlaneSupportedDisplaysKHR;
	vkGetDisplayModePropertiesKHR = pExternInstanceDispatchTable->GetDisplayModePropertiesKHR;
	vkCreateDisplayModeKHR = pExternInstanceDispatchTable->CreateDisplayModeKHR;
	vkGetDisplayPlaneCapabilitiesKHR = pExternInstanceDispatchTable->GetDisplayPlaneCapabilitiesKHR;
	vkCreateDisplayPlaneSurfaceKHR = pExternInstanceDispatchTable->CreateDisplayPlaneSurfaceKHR;
#endif

#if defined(VK_KHR_xlib_surface)
	// ---- VK_KHR_xlib_surface extension commands
#ifdef VK_USE_PLATFORM_XLIB_KHR
	vkCreateXlibSurfaceKHR = pExternInstanceDispatchTable->CreateXlibSurfaceKHR;
#endif // VK_USE_PLATFORM_XLIB_KHR
#ifdef VK_USE_PLATFORM_XLIB_KHR
	vkGetPhysicalDeviceXlibPresentationSupportKHR = pExternInstanceDispatchTable->GetPhysicalDeviceXlibPresentationSupportKHR;
#endif // VK_USE_PLATFORM_XLIB_KHR
#endif

#if defined(VK_KHR_xcb_surface)
	// ---- VK_KHR_xcb_surface extension commands
#ifdef VK_USE_PLATFORM_XCB_KHR
	vkCreateXcbSurfaceKHR = pExternInstanceDispatchTable->CreateXcbSurfaceKHR;
#endif // VK_USE_PLATFORM_XCB_KHR
#ifdef VK_USE_PLATFORM_XCB_KHR
	vkGetPhysicalDeviceXcbPresentationSupportKHR = pExternInstanceDispatchTable->GetPhysicalDeviceXcbPresentationSupportKHR;
#endif // VK_USE_PLATFORM_XCB_KHR
#endif

#if defined(VK_KHR_wayland_surface)
	// ---- VK_KHR_wayland_surface extension commands
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
	vkCreateWaylandSurfaceKHR = pExternInstanceDispatchTable->CreateWaylandSurfaceKHR;
#endif // VK_USE_PLATFORM_WAYLAND_KHR
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
	vkGetPhysicalDeviceWaylandPresentationSupportKHR = pExternInstanceDispatchTable->GetPhysicalDeviceWaylandPresentationSupportKHR;
#endif // VK_USE_PLATFORM_WAYLAND_KHR
#endif

#if defined(VK_KHR_android_surface)
	// ---- VK_KHR_android_surface extension commands
#ifdef VK_USE_PLATFORM_ANDROID_KHR
	vkCreateAndroidSurfaceKHR = pExternInstanceDispatchTable->CreateAndroidSurfaceKHR;
#endif // VK_USE_PLATFORM_ANDROID_KHR
#endif

#if defined(VK_KHR_win32_surface)
	// ---- VK_KHR_win32_surface extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkCreateWin32SurfaceKHR = pExternInstanceDispatchTable->CreateWin32SurfaceKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkGetPhysicalDeviceWin32PresentationSupportKHR = pExternInstanceDispatchTable->GetPhysicalDeviceWin32PresentationSupportKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#endif

#if defined(VK_KHR_get_physical_device_properties2)
	// ---- VK_KHR_get_physical_device_properties2 extension commands
	vkGetPhysicalDeviceFeatures2KHR = pExternInstanceDispatchTable->GetPhysicalDeviceFeatures2KHR;
	vkGetPhysicalDeviceProperties2KHR = pExternInstanceDispatchTable->GetPhysicalDeviceProperties2KHR;
	vkGetPhysicalDeviceFormatProperties2KHR = pExternInstanceDispatchTable->GetPhysicalDeviceFormatProperties2KHR;
	vkGetPhysicalDeviceImageFormatProperties2KHR = pExternInstanceDispatchTable->GetPhysicalDeviceImageFormatProperties2KHR;
	vkGetPhysicalDeviceQueueFamilyProperties2KHR = pExternInstanceDispatchTable->GetPhysicalDeviceQueueFamilyProperties2KHR;
	vkGetPhysicalDeviceMemoryProperties2KHR = pExternInstanceDispatchTable->GetPhysicalDeviceMemoryProperties2KHR;
	vkGetPhysicalDeviceSparseImageFormatProperties2KHR = pExternInstanceDispatchTable->GetPhysicalDeviceSparseImageFormatProperties2KHR;
#endif

#if defined(VK_KHR_device_group_creation)
	// ---- VK_KHR_device_group_creation extension commands
	vkEnumeratePhysicalDeviceGroupsKHR = pExternInstanceDispatchTable->EnumeratePhysicalDeviceGroupsKHR;
#endif

#if defined(VK_KHR_external_memory_capabilities)
	// ---- VK_KHR_external_memory_capabilities extension commands
	vkGetPhysicalDeviceExternalBufferPropertiesKHR = pExternInstanceDispatchTable->GetPhysicalDeviceExternalBufferPropertiesKHR;
#endif

#if defined(VK_KHR_external_semaphore_capabilities)
	// ---- VK_KHR_external_semaphore_capabilities extension commands
	vkGetPhysicalDeviceExternalSemaphorePropertiesKHR = pExternInstanceDispatchTable->GetPhysicalDeviceExternalSemaphorePropertiesKHR;
#endif

#if defined(VK_KHR_external_fence_capabilities)
	// ---- VK_KHR_external_fence_capabilities extension commands
	vkGetPhysicalDeviceExternalFencePropertiesKHR = pExternInstanceDispatchTable->GetPhysicalDeviceExternalFencePropertiesKHR;
#endif

#if defined(VK_KHR_get_surface_capabilities2)
	// ---- VK_KHR_get_surface_capabilities2 extension commands
	vkGetPhysicalDeviceSurfaceCapabilities2KHR = pExternInstanceDispatchTable->GetPhysicalDeviceSurfaceCapabilities2KHR;
	vkGetPhysicalDeviceSurfaceFormats2KHR = pExternInstanceDispatchTable->GetPhysicalDeviceSurfaceFormats2KHR;
#endif

#if defined(VK_KHR_get_display_properties2)
	// ---- VK_KHR_get_display_properties2 extension commands
	vkGetPhysicalDeviceDisplayProperties2KHR = pExternInstanceDispatchTable->GetPhysicalDeviceDisplayProperties2KHR;
	vkGetPhysicalDeviceDisplayPlaneProperties2KHR = pExternInstanceDispatchTable->GetPhysicalDeviceDisplayPlaneProperties2KHR;
	vkGetDisplayModeProperties2KHR = pExternInstanceDispatchTable->GetDisplayModeProperties2KHR;
	vkGetDisplayPlaneCapabilities2KHR = pExternInstanceDispatchTable->GetDisplayPlaneCapabilities2KHR;
#endif

#if defined(VK_EXT_debug_report)
	// ---- VK_EXT_debug_report extension commands
	vkCreateDebugReportCallbackEXT = pExternInstanceDispatchTable->CreateDebugReportCallbackEXT;
	vkDestroyDebugReportCallbackEXT = pExternInstanceDispatchTable->DestroyDebugReportCallbackEXT;
	vkDebugReportMessageEXT = pExternInstanceDispatchTable->DebugReportMessageEXT;
#endif

#if defined(VK_NV_external_memory_capabilities)
	// ---- VK_NV_external_memory_capabilities extension commands
	vkGetPhysicalDeviceExternalImageFormatPropertiesNV = pExternInstanceDispatchTable->GetPhysicalDeviceExternalImageFormatPropertiesNV;
#endif

#if defined(VK_NN_vi_surface)
	// ---- VK_NN_vi_surface extension commands
#ifdef VK_USE_PLATFORM_VI_NN
	vkCreateViSurfaceNN = pExternInstanceDispatchTable->CreateViSurfaceNN;
#endif // VK_USE_PLATFORM_VI_NN
#endif

#if defined(VK_NVX_device_generated_commands)
	// ---- VK_NVX_device_generated_commands extension commands
	vkGetPhysicalDeviceGeneratedCommandsPropertiesNVX = pExternInstanceDispatchTable->GetPhysicalDeviceGeneratedCommandsPropertiesNVX;
#endif

#if defined(VK_EXT_direct_mode_display)
	// ---- VK_EXT_direct_mode_display extension commands
	vkReleaseDisplayEXT = pExternInstanceDispatchTable->ReleaseDisplayEXT;
#endif

#if defined(VK_EXT_acquire_xlib_display)
	// ---- VK_EXT_acquire_xlib_display extension commands
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
	vkAcquireXlibDisplayEXT = pExternInstanceDispatchTable->AcquireXlibDisplayEXT;
#endif // VK_USE_PLATFORM_XLIB_XRANDR_EXT
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
	vkGetRandROutputDisplayEXT = pExternInstanceDispatchTable->GetRandROutputDisplayEXT;
#endif // VK_USE_PLATFORM_XLIB_XRANDR_EXT
#endif

#if defined(VK_EXT_display_surface_counter)
	// ---- VK_EXT_display_surface_counter extension commands
	vkGetPhysicalDeviceSurfaceCapabilities2EXT = pExternInstanceDispatchTable->GetPhysicalDeviceSurfaceCapabilities2EXT;
#endif

#if defined(VK_MVK_ios_surface)
	// ---- VK_MVK_ios_surface extension commands
#ifdef VK_USE_PLATFORM_IOS_MVK
	vkCreateIOSSurfaceMVK = pExternInstanceDispatchTable->CreateIOSSurfaceMVK;
#endif // VK_USE_PLATFORM_IOS_MVK
#endif

#if defined(VK_MVK_macos_surface)
	// ---- VK_MVK_macos_surface extension commands
#ifdef VK_USE_PLATFORM_MACOS_MVK
	vkCreateMacOSSurfaceMVK = pExternInstanceDispatchTable->CreateMacOSSurfaceMVK;
#endif // VK_USE_PLATFORM_MACOS_MVK
#endif

#if defined(VK_EXT_debug_utils)
	// ---- VK_EXT_debug_utils extension commands
	vkCreateDebugUtilsMessengerEXT = pExternInstanceDispatchTable->CreateDebugUtilsMessengerEXT;
	vkDestroyDebugUtilsMessengerEXT = pExternInstanceDispatchTable->DestroyDebugUtilsMessengerEXT;
	vkSubmitDebugUtilsMessageEXT = pExternInstanceDispatchTable->SubmitDebugUtilsMessageEXT;
#endif

#if defined(VK_EXT_sample_locations)
	// ---- VK_EXT_sample_locations extension commands
	vkGetPhysicalDeviceMultisamplePropertiesEXT = pExternInstanceDispatchTable->GetPhysicalDeviceMultisamplePropertiesEXT;
#endif

#if defined(VK_EXT_calibrated_timestamps)
	// ---- VK_EXT_calibrated_timestamps extension commands
	vkGetPhysicalDeviceCalibrateableTimeDomainsEXT = pExternInstanceDispatchTable->GetPhysicalDeviceCalibrateableTimeDomainsEXT;
#endif

#if defined(VK_FUCHSIA_imagepipe_surface)
	// ---- VK_FUCHSIA_imagepipe_surface extension commands
#ifdef VK_USE_PLATFORM_FUCHSIA
	vkCreateImagePipeSurfaceFUCHSIA = pExternInstanceDispatchTable->CreateImagePipeSurfaceFUCHSIA;
#endif // VK_USE_PLATFORM_FUCHSIA
#endif

#if defined(VK_NV_cooperative_matrix)
	// ---- VK_NV_cooperative_matrix extension commands
	vkGetPhysicalDeviceCooperativeMatrixPropertiesNV = pExternInstanceDispatchTable->GetPhysicalDeviceCooperativeMatrixPropertiesNV;
#endif



#if defined(VK_VERSION_1_0)
	// ---- Core 1_0 commands
	vkGetDeviceProcAddr = pExternDeviceDispatchTable->GetDeviceProcAddr;
	vkDestroyDevice = pExternDeviceDispatchTable->DestroyDevice;
	vkGetDeviceQueue = pExternDeviceDispatchTable->GetDeviceQueue;
	vkQueueSubmit = pExternDeviceDispatchTable->QueueSubmit;
	vkQueueWaitIdle = pExternDeviceDispatchTable->QueueWaitIdle;
	vkDeviceWaitIdle = pExternDeviceDispatchTable->DeviceWaitIdle;
	vkAllocateMemory = pExternDeviceDispatchTable->AllocateMemory;
	vkFreeMemory = pExternDeviceDispatchTable->FreeMemory;
	vkMapMemory = pExternDeviceDispatchTable->MapMemory;
	vkUnmapMemory = pExternDeviceDispatchTable->UnmapMemory;
	vkFlushMappedMemoryRanges = pExternDeviceDispatchTable->FlushMappedMemoryRanges;
	vkInvalidateMappedMemoryRanges = pExternDeviceDispatchTable->InvalidateMappedMemoryRanges;
	vkGetDeviceMemoryCommitment = pExternDeviceDispatchTable->GetDeviceMemoryCommitment;
	vkBindBufferMemory = pExternDeviceDispatchTable->BindBufferMemory;
	vkBindImageMemory = pExternDeviceDispatchTable->BindImageMemory;
	vkGetBufferMemoryRequirements = pExternDeviceDispatchTable->GetBufferMemoryRequirements;
	vkGetImageMemoryRequirements = pExternDeviceDispatchTable->GetImageMemoryRequirements;
	vkGetImageSparseMemoryRequirements = pExternDeviceDispatchTable->GetImageSparseMemoryRequirements;
	vkQueueBindSparse = pExternDeviceDispatchTable->QueueBindSparse;
	vkCreateFence = pExternDeviceDispatchTable->CreateFence;
	vkDestroyFence = pExternDeviceDispatchTable->DestroyFence;
	vkResetFences = pExternDeviceDispatchTable->ResetFences;
	vkGetFenceStatus = pExternDeviceDispatchTable->GetFenceStatus;
	vkWaitForFences = pExternDeviceDispatchTable->WaitForFences;
	vkCreateSemaphore = pExternDeviceDispatchTable->CreateSemaphore;
	vkDestroySemaphore = pExternDeviceDispatchTable->DestroySemaphore;
	vkCreateEvent = pExternDeviceDispatchTable->CreateEvent;
	vkDestroyEvent = pExternDeviceDispatchTable->DestroyEvent;
	vkGetEventStatus = pExternDeviceDispatchTable->GetEventStatus;
	vkSetEvent = pExternDeviceDispatchTable->SetEvent;
	vkResetEvent = pExternDeviceDispatchTable->ResetEvent;
	vkCreateQueryPool = pExternDeviceDispatchTable->CreateQueryPool;
	vkDestroyQueryPool = pExternDeviceDispatchTable->DestroyQueryPool;
	vkGetQueryPoolResults = pExternDeviceDispatchTable->GetQueryPoolResults;
	vkCreateBuffer = pExternDeviceDispatchTable->CreateBuffer;
	vkDestroyBuffer = pExternDeviceDispatchTable->DestroyBuffer;
	vkCreateBufferView = pExternDeviceDispatchTable->CreateBufferView;
	vkDestroyBufferView = pExternDeviceDispatchTable->DestroyBufferView;
	vkCreateImage = pExternDeviceDispatchTable->CreateImage;
	vkDestroyImage = pExternDeviceDispatchTable->DestroyImage;
	vkGetImageSubresourceLayout = pExternDeviceDispatchTable->GetImageSubresourceLayout;
	vkCreateImageView = pExternDeviceDispatchTable->CreateImageView;
	vkDestroyImageView = pExternDeviceDispatchTable->DestroyImageView;
	vkCreateShaderModule = pExternDeviceDispatchTable->CreateShaderModule;
	vkDestroyShaderModule = pExternDeviceDispatchTable->DestroyShaderModule;
	vkCreatePipelineCache = pExternDeviceDispatchTable->CreatePipelineCache;
	vkDestroyPipelineCache = pExternDeviceDispatchTable->DestroyPipelineCache;
	vkGetPipelineCacheData = pExternDeviceDispatchTable->GetPipelineCacheData;
	vkMergePipelineCaches = pExternDeviceDispatchTable->MergePipelineCaches;
	vkCreateGraphicsPipelines = pExternDeviceDispatchTable->CreateGraphicsPipelines;
	vkCreateComputePipelines = pExternDeviceDispatchTable->CreateComputePipelines;
	vkDestroyPipeline = pExternDeviceDispatchTable->DestroyPipeline;
	vkCreatePipelineLayout = pExternDeviceDispatchTable->CreatePipelineLayout;
	vkDestroyPipelineLayout = pExternDeviceDispatchTable->DestroyPipelineLayout;
	vkCreateSampler = pExternDeviceDispatchTable->CreateSampler;
	vkDestroySampler = pExternDeviceDispatchTable->DestroySampler;
	vkCreateDescriptorSetLayout = pExternDeviceDispatchTable->CreateDescriptorSetLayout;
	vkDestroyDescriptorSetLayout = pExternDeviceDispatchTable->DestroyDescriptorSetLayout;
	vkCreateDescriptorPool = pExternDeviceDispatchTable->CreateDescriptorPool;
	vkDestroyDescriptorPool = pExternDeviceDispatchTable->DestroyDescriptorPool;
	vkResetDescriptorPool = pExternDeviceDispatchTable->ResetDescriptorPool;
	vkAllocateDescriptorSets = pExternDeviceDispatchTable->AllocateDescriptorSets;
	vkFreeDescriptorSets = pExternDeviceDispatchTable->FreeDescriptorSets;
	vkUpdateDescriptorSets = pExternDeviceDispatchTable->UpdateDescriptorSets;
	vkCreateFramebuffer = pExternDeviceDispatchTable->CreateFramebuffer;
	vkDestroyFramebuffer = pExternDeviceDispatchTable->DestroyFramebuffer;
	vkCreateRenderPass = pExternDeviceDispatchTable->CreateRenderPass;
	vkDestroyRenderPass = pExternDeviceDispatchTable->DestroyRenderPass;
	vkGetRenderAreaGranularity = pExternDeviceDispatchTable->GetRenderAreaGranularity;
	vkCreateCommandPool = pExternDeviceDispatchTable->CreateCommandPool;
	vkDestroyCommandPool = pExternDeviceDispatchTable->DestroyCommandPool;
	vkResetCommandPool = pExternDeviceDispatchTable->ResetCommandPool;
	vkAllocateCommandBuffers = pExternDeviceDispatchTable->AllocateCommandBuffers;
	vkFreeCommandBuffers = pExternDeviceDispatchTable->FreeCommandBuffers;
	vkBeginCommandBuffer = pExternDeviceDispatchTable->BeginCommandBuffer;
	vkEndCommandBuffer = pExternDeviceDispatchTable->EndCommandBuffer;
	vkResetCommandBuffer = pExternDeviceDispatchTable->ResetCommandBuffer;
	vkCmdBindPipeline = pExternDeviceDispatchTable->CmdBindPipeline;
	vkCmdSetViewport = pExternDeviceDispatchTable->CmdSetViewport;
	vkCmdSetScissor = pExternDeviceDispatchTable->CmdSetScissor;
	vkCmdSetLineWidth = pExternDeviceDispatchTable->CmdSetLineWidth;
	vkCmdSetDepthBias = pExternDeviceDispatchTable->CmdSetDepthBias;
	vkCmdSetBlendConstants = pExternDeviceDispatchTable->CmdSetBlendConstants;
	vkCmdSetDepthBounds = pExternDeviceDispatchTable->CmdSetDepthBounds;
	vkCmdSetStencilCompareMask = pExternDeviceDispatchTable->CmdSetStencilCompareMask;
	vkCmdSetStencilWriteMask = pExternDeviceDispatchTable->CmdSetStencilWriteMask;
	vkCmdSetStencilReference = pExternDeviceDispatchTable->CmdSetStencilReference;
	vkCmdBindDescriptorSets = pExternDeviceDispatchTable->CmdBindDescriptorSets;
	vkCmdBindIndexBuffer = pExternDeviceDispatchTable->CmdBindIndexBuffer;
	vkCmdBindVertexBuffers = pExternDeviceDispatchTable->CmdBindVertexBuffers;
	vkCmdDraw = pExternDeviceDispatchTable->CmdDraw;
	vkCmdDrawIndexed = pExternDeviceDispatchTable->CmdDrawIndexed;
	vkCmdDrawIndirect = pExternDeviceDispatchTable->CmdDrawIndirect;
	vkCmdDrawIndexedIndirect = pExternDeviceDispatchTable->CmdDrawIndexedIndirect;
	vkCmdDispatch = pExternDeviceDispatchTable->CmdDispatch;
	vkCmdDispatchIndirect = pExternDeviceDispatchTable->CmdDispatchIndirect;
	vkCmdCopyBuffer = pExternDeviceDispatchTable->CmdCopyBuffer;
	vkCmdCopyImage = pExternDeviceDispatchTable->CmdCopyImage;
	vkCmdBlitImage = pExternDeviceDispatchTable->CmdBlitImage;
	vkCmdCopyBufferToImage = pExternDeviceDispatchTable->CmdCopyBufferToImage;
	vkCmdCopyImageToBuffer = pExternDeviceDispatchTable->CmdCopyImageToBuffer;
	vkCmdUpdateBuffer = pExternDeviceDispatchTable->CmdUpdateBuffer;
	vkCmdFillBuffer = pExternDeviceDispatchTable->CmdFillBuffer;
	vkCmdClearColorImage = pExternDeviceDispatchTable->CmdClearColorImage;
	vkCmdClearDepthStencilImage = pExternDeviceDispatchTable->CmdClearDepthStencilImage;
	vkCmdClearAttachments = pExternDeviceDispatchTable->CmdClearAttachments;
	vkCmdResolveImage = pExternDeviceDispatchTable->CmdResolveImage;
	vkCmdSetEvent = pExternDeviceDispatchTable->CmdSetEvent;
	vkCmdResetEvent = pExternDeviceDispatchTable->CmdResetEvent;
	vkCmdWaitEvents = pExternDeviceDispatchTable->CmdWaitEvents;
	vkCmdPipelineBarrier = pExternDeviceDispatchTable->CmdPipelineBarrier;
	vkCmdBeginQuery = pExternDeviceDispatchTable->CmdBeginQuery;
	vkCmdEndQuery = pExternDeviceDispatchTable->CmdEndQuery;
	vkCmdResetQueryPool = pExternDeviceDispatchTable->CmdResetQueryPool;
	vkCmdWriteTimestamp = pExternDeviceDispatchTable->CmdWriteTimestamp;
	vkCmdCopyQueryPoolResults = pExternDeviceDispatchTable->CmdCopyQueryPoolResults;
	vkCmdPushConstants = pExternDeviceDispatchTable->CmdPushConstants;
	vkCmdBeginRenderPass = pExternDeviceDispatchTable->CmdBeginRenderPass;
	vkCmdNextSubpass = pExternDeviceDispatchTable->CmdNextSubpass;
	vkCmdEndRenderPass = pExternDeviceDispatchTable->CmdEndRenderPass;
	vkCmdExecuteCommands = pExternDeviceDispatchTable->CmdExecuteCommands;
#endif

#if defined(VK_VERSION_1_1)
	// ---- Core 1_1 commands
	vkBindBufferMemory2 = pExternDeviceDispatchTable->BindBufferMemory2;
	vkBindImageMemory2 = pExternDeviceDispatchTable->BindImageMemory2;
	vkGetDeviceGroupPeerMemoryFeatures = pExternDeviceDispatchTable->GetDeviceGroupPeerMemoryFeatures;
	vkCmdSetDeviceMask = pExternDeviceDispatchTable->CmdSetDeviceMask;
	vkCmdDispatchBase = pExternDeviceDispatchTable->CmdDispatchBase;
	vkGetImageMemoryRequirements2 = pExternDeviceDispatchTable->GetImageMemoryRequirements2;
	vkGetBufferMemoryRequirements2 = pExternDeviceDispatchTable->GetBufferMemoryRequirements2;
	vkGetImageSparseMemoryRequirements2 = pExternDeviceDispatchTable->GetImageSparseMemoryRequirements2;
	vkTrimCommandPool = pExternDeviceDispatchTable->TrimCommandPool;
	vkGetDeviceQueue2 = pExternDeviceDispatchTable->GetDeviceQueue2;
	vkCreateSamplerYcbcrConversion = pExternDeviceDispatchTable->CreateSamplerYcbcrConversion;
	vkDestroySamplerYcbcrConversion = pExternDeviceDispatchTable->DestroySamplerYcbcrConversion;
	vkCreateDescriptorUpdateTemplate = pExternDeviceDispatchTable->CreateDescriptorUpdateTemplate;
	vkDestroyDescriptorUpdateTemplate = pExternDeviceDispatchTable->DestroyDescriptorUpdateTemplate;
	vkUpdateDescriptorSetWithTemplate = pExternDeviceDispatchTable->UpdateDescriptorSetWithTemplate;
	vkGetDescriptorSetLayoutSupport = pExternDeviceDispatchTable->GetDescriptorSetLayoutSupport;
#endif

#if defined(VK_KHR_swapchain)
	// ---- VK_KHR_swapchain extension commands
	vkCreateSwapchainKHR = pExternDeviceDispatchTable->CreateSwapchainKHR;
	vkDestroySwapchainKHR = pExternDeviceDispatchTable->DestroySwapchainKHR;
	vkGetSwapchainImagesKHR = pExternDeviceDispatchTable->GetSwapchainImagesKHR;
	vkAcquireNextImageKHR = pExternDeviceDispatchTable->AcquireNextImageKHR;
	vkQueuePresentKHR = pExternDeviceDispatchTable->QueuePresentKHR;
	vkGetDeviceGroupPresentCapabilitiesKHR = pExternDeviceDispatchTable->GetDeviceGroupPresentCapabilitiesKHR;
	vkGetDeviceGroupSurfacePresentModesKHR = pExternDeviceDispatchTable->GetDeviceGroupSurfacePresentModesKHR;
	vkAcquireNextImage2KHR = pExternDeviceDispatchTable->AcquireNextImage2KHR;
#endif

#if defined(VK_KHR_display_swapchain)
	// ---- VK_KHR_display_swapchain extension commands
	vkCreateSharedSwapchainsKHR = pExternDeviceDispatchTable->CreateSharedSwapchainsKHR;
#endif

#if defined(VK_KHR_device_group)
	// ---- VK_KHR_device_group extension commands
	vkGetDeviceGroupPeerMemoryFeaturesKHR = pExternDeviceDispatchTable->GetDeviceGroupPeerMemoryFeaturesKHR;
	vkCmdSetDeviceMaskKHR = pExternDeviceDispatchTable->CmdSetDeviceMaskKHR;
	vkCmdDispatchBaseKHR = pExternDeviceDispatchTable->CmdDispatchBaseKHR;
#endif

#if defined(VK_KHR_maintenance1)
	// ---- VK_KHR_maintenance1 extension commands
	vkTrimCommandPoolKHR = pExternDeviceDispatchTable->TrimCommandPoolKHR;
#endif

#if defined(VK_KHR_external_memory_win32)
	// ---- VK_KHR_external_memory_win32 extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkGetMemoryWin32HandleKHR = pExternDeviceDispatchTable->GetMemoryWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkGetMemoryWin32HandlePropertiesKHR = pExternDeviceDispatchTable->GetMemoryWin32HandlePropertiesKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#endif

#if defined(VK_KHR_external_memory_fd)
	// ---- VK_KHR_external_memory_fd extension commands
	vkGetMemoryFdKHR = pExternDeviceDispatchTable->GetMemoryFdKHR;
	vkGetMemoryFdPropertiesKHR = pExternDeviceDispatchTable->GetMemoryFdPropertiesKHR;
#endif

#if defined(VK_KHR_external_semaphore_win32)
	// ---- VK_KHR_external_semaphore_win32 extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkImportSemaphoreWin32HandleKHR = pExternDeviceDispatchTable->ImportSemaphoreWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkGetSemaphoreWin32HandleKHR = pExternDeviceDispatchTable->GetSemaphoreWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#endif

#if defined(VK_KHR_external_semaphore_fd)
	// ---- VK_KHR_external_semaphore_fd extension commands
	vkImportSemaphoreFdKHR = pExternDeviceDispatchTable->ImportSemaphoreFdKHR;
	vkGetSemaphoreFdKHR = pExternDeviceDispatchTable->GetSemaphoreFdKHR;
#endif

#if defined(VK_KHR_push_descriptor)
	// ---- VK_KHR_push_descriptor extension commands
	vkCmdPushDescriptorSetKHR = pExternDeviceDispatchTable->CmdPushDescriptorSetKHR;
	vkCmdPushDescriptorSetWithTemplateKHR = pExternDeviceDispatchTable->CmdPushDescriptorSetWithTemplateKHR;
#endif

#if defined(VK_KHR_descriptor_update_template)
	// ---- VK_KHR_descriptor_update_template extension commands
	vkCreateDescriptorUpdateTemplateKHR = pExternDeviceDispatchTable->CreateDescriptorUpdateTemplateKHR;
	vkDestroyDescriptorUpdateTemplateKHR = pExternDeviceDispatchTable->DestroyDescriptorUpdateTemplateKHR;
	vkUpdateDescriptorSetWithTemplateKHR = pExternDeviceDispatchTable->UpdateDescriptorSetWithTemplateKHR;
#endif

#if defined(VK_KHR_create_renderpass2)
	// ---- VK_KHR_create_renderpass2 extension commands
	vkCreateRenderPass2KHR = pExternDeviceDispatchTable->CreateRenderPass2KHR;
	vkCmdBeginRenderPass2KHR = pExternDeviceDispatchTable->CmdBeginRenderPass2KHR;
	vkCmdNextSubpass2KHR = pExternDeviceDispatchTable->CmdNextSubpass2KHR;
	vkCmdEndRenderPass2KHR = pExternDeviceDispatchTable->CmdEndRenderPass2KHR;
#endif

#if defined(VK_KHR_shared_presentable_image)
	// ---- VK_KHR_shared_presentable_image extension commands
	vkGetSwapchainStatusKHR = pExternDeviceDispatchTable->GetSwapchainStatusKHR;
#endif

#if defined(VK_KHR_external_fence_win32)
	// ---- VK_KHR_external_fence_win32 extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkImportFenceWin32HandleKHR = pExternDeviceDispatchTable->ImportFenceWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkGetFenceWin32HandleKHR = pExternDeviceDispatchTable->GetFenceWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#endif

#if defined(VK_KHR_external_fence_fd)
	// ---- VK_KHR_external_fence_fd extension commands
	vkImportFenceFdKHR = pExternDeviceDispatchTable->ImportFenceFdKHR;
	vkGetFenceFdKHR = pExternDeviceDispatchTable->GetFenceFdKHR;
#endif

#if defined(VK_KHR_get_memory_requirements2)
	// ---- VK_KHR_get_memory_requirements2 extension commands
	vkGetImageMemoryRequirements2KHR = pExternDeviceDispatchTable->GetImageMemoryRequirements2KHR;
	vkGetBufferMemoryRequirements2KHR = pExternDeviceDispatchTable->GetBufferMemoryRequirements2KHR;
	vkGetImageSparseMemoryRequirements2KHR = pExternDeviceDispatchTable->GetImageSparseMemoryRequirements2KHR;
#endif

#if defined(VK_KHR_sampler_ycbcr_conversion)
	// ---- VK_KHR_sampler_ycbcr_conversion extension commands
	vkCreateSamplerYcbcrConversionKHR = pExternDeviceDispatchTable->CreateSamplerYcbcrConversionKHR;
	vkDestroySamplerYcbcrConversionKHR = pExternDeviceDispatchTable->DestroySamplerYcbcrConversionKHR;
#endif

#if defined(VK_KHR_bind_memory2)
	// ---- VK_KHR_bind_memory2 extension commands
	vkBindBufferMemory2KHR = pExternDeviceDispatchTable->BindBufferMemory2KHR;
	vkBindImageMemory2KHR = pExternDeviceDispatchTable->BindImageMemory2KHR;
#endif

#if defined(VK_KHR_maintenance3)
	// ---- VK_KHR_maintenance3 extension commands
	vkGetDescriptorSetLayoutSupportKHR = pExternDeviceDispatchTable->GetDescriptorSetLayoutSupportKHR;
#endif

#if defined(VK_KHR_draw_indirect_count)
	// ---- VK_KHR_draw_indirect_count extension commands
	vkCmdDrawIndirectCountKHR = pExternDeviceDispatchTable->CmdDrawIndirectCountKHR;
	vkCmdDrawIndexedIndirectCountKHR = pExternDeviceDispatchTable->CmdDrawIndexedIndirectCountKHR;
#endif

#if defined(VK_EXT_debug_marker)
	// ---- VK_EXT_debug_marker extension commands
	vkDebugMarkerSetObjectTagEXT = pExternDeviceDispatchTable->DebugMarkerSetObjectTagEXT;
	vkDebugMarkerSetObjectNameEXT = pExternDeviceDispatchTable->DebugMarkerSetObjectNameEXT;
	vkCmdDebugMarkerBeginEXT = pExternDeviceDispatchTable->CmdDebugMarkerBeginEXT;
	vkCmdDebugMarkerEndEXT = pExternDeviceDispatchTable->CmdDebugMarkerEndEXT;
	vkCmdDebugMarkerInsertEXT = pExternDeviceDispatchTable->CmdDebugMarkerInsertEXT;
#endif

#if defined(VK_EXT_transform_feedback)
	// ---- VK_EXT_transform_feedback extension commands
	vkCmdBindTransformFeedbackBuffersEXT = pExternDeviceDispatchTable->CmdBindTransformFeedbackBuffersEXT;
	vkCmdBeginTransformFeedbackEXT = pExternDeviceDispatchTable->CmdBeginTransformFeedbackEXT;
	vkCmdEndTransformFeedbackEXT = pExternDeviceDispatchTable->CmdEndTransformFeedbackEXT;
	vkCmdBeginQueryIndexedEXT = pExternDeviceDispatchTable->CmdBeginQueryIndexedEXT;
	vkCmdEndQueryIndexedEXT = pExternDeviceDispatchTable->CmdEndQueryIndexedEXT;
	vkCmdDrawIndirectByteCountEXT = pExternDeviceDispatchTable->CmdDrawIndirectByteCountEXT;
#endif

#if defined(VK_AMD_draw_indirect_count)
	// ---- VK_AMD_draw_indirect_count extension commands
	vkCmdDrawIndirectCountAMD = pExternDeviceDispatchTable->CmdDrawIndirectCountAMD;
	vkCmdDrawIndexedIndirectCountAMD = pExternDeviceDispatchTable->CmdDrawIndexedIndirectCountAMD;
#endif

#if defined(VK_AMD_shader_info)
	// ---- VK_AMD_shader_info extension commands
	vkGetShaderInfoAMD = pExternDeviceDispatchTable->GetShaderInfoAMD;
#endif

#if defined(VK_NV_external_memory_win32)
	// ---- VK_NV_external_memory_win32 extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkGetMemoryWin32HandleNV = pExternDeviceDispatchTable->GetMemoryWin32HandleNV;
#endif // VK_USE_PLATFORM_WIN32_KHR
#endif

#if defined(VK_EXT_conditional_rendering)
	// ---- VK_EXT_conditional_rendering extension commands
	vkCmdBeginConditionalRenderingEXT = pExternDeviceDispatchTable->CmdBeginConditionalRenderingEXT;
	vkCmdEndConditionalRenderingEXT = pExternDeviceDispatchTable->CmdEndConditionalRenderingEXT;
#endif

#if defined(VK_NVX_device_generated_commands)
	// ---- VK_NVX_device_generated_commands extension commands
	vkCmdProcessCommandsNVX = pExternDeviceDispatchTable->CmdProcessCommandsNVX;
	vkCmdReserveSpaceForCommandsNVX = pExternDeviceDispatchTable->CmdReserveSpaceForCommandsNVX;
	vkCreateIndirectCommandsLayoutNVX = pExternDeviceDispatchTable->CreateIndirectCommandsLayoutNVX;
	vkDestroyIndirectCommandsLayoutNVX = pExternDeviceDispatchTable->DestroyIndirectCommandsLayoutNVX;
	vkCreateObjectTableNVX = pExternDeviceDispatchTable->CreateObjectTableNVX;
	vkDestroyObjectTableNVX = pExternDeviceDispatchTable->DestroyObjectTableNVX;
	vkRegisterObjectsNVX = pExternDeviceDispatchTable->RegisterObjectsNVX;
	vkUnregisterObjectsNVX = pExternDeviceDispatchTable->UnregisterObjectsNVX;
#endif

#if defined(VK_NV_clip_space_w_scaling)
	// ---- VK_NV_clip_space_w_scaling extension commands
	vkCmdSetViewportWScalingNV = pExternDeviceDispatchTable->CmdSetViewportWScalingNV;
#endif


#if defined(VK_EXT_display_control)
	// ---- VK_EXT_display_control extension commands
	vkDisplayPowerControlEXT = pExternDeviceDispatchTable->DisplayPowerControlEXT;
	vkRegisterDeviceEventEXT = pExternDeviceDispatchTable->RegisterDeviceEventEXT;
	vkRegisterDisplayEventEXT = pExternDeviceDispatchTable->RegisterDisplayEventEXT;
	vkGetSwapchainCounterEXT = pExternDeviceDispatchTable->GetSwapchainCounterEXT;
#endif

#if defined(VK_GOOGLE_display_timing)
	// ---- VK_GOOGLE_display_timing extension commands
	vkGetRefreshCycleDurationGOOGLE = pExternDeviceDispatchTable->GetRefreshCycleDurationGOOGLE;
	vkGetPastPresentationTimingGOOGLE = pExternDeviceDispatchTable->GetPastPresentationTimingGOOGLE;
#endif

#if defined(VK_EXT_discard_rectangles)
	// ---- VK_EXT_discard_rectangles extension commands
	vkCmdSetDiscardRectangleEXT = pExternDeviceDispatchTable->CmdSetDiscardRectangleEXT;
#endif

#if defined(VK_EXT_hdr_metadata)
	// ---- VK_EXT_hdr_metadata extension commands
	vkSetHdrMetadataEXT = pExternDeviceDispatchTable->SetHdrMetadataEXT;
#endif

#if defined(VK_EXT_debug_utils)
	// ---- VK_EXT_debug_utils extension commands
	vkSetDebugUtilsObjectNameEXT = pExternDeviceDispatchTable->SetDebugUtilsObjectNameEXT;
	vkSetDebugUtilsObjectTagEXT = pExternDeviceDispatchTable->SetDebugUtilsObjectTagEXT;
	vkQueueBeginDebugUtilsLabelEXT = pExternDeviceDispatchTable->QueueBeginDebugUtilsLabelEXT;
	vkQueueEndDebugUtilsLabelEXT = pExternDeviceDispatchTable->QueueEndDebugUtilsLabelEXT;
	vkQueueInsertDebugUtilsLabelEXT = pExternDeviceDispatchTable->QueueInsertDebugUtilsLabelEXT;
	vkCmdBeginDebugUtilsLabelEXT = pExternDeviceDispatchTable->CmdBeginDebugUtilsLabelEXT;
	vkCmdEndDebugUtilsLabelEXT = pExternDeviceDispatchTable->CmdEndDebugUtilsLabelEXT;
	vkCmdInsertDebugUtilsLabelEXT = pExternDeviceDispatchTable->CmdInsertDebugUtilsLabelEXT;
#endif

#if defined(VK_ANDROID_external_memory_android_hardware_buffer)
	// ---- VK_ANDROID_external_memory_android_hardware_buffer extension commands
#ifdef VK_USE_PLATFORM_ANDROID_KHR
	vkGetAndroidHardwareBufferPropertiesANDROID = pExternDeviceDispatchTable->GetAndroidHardwareBufferPropertiesANDROID;
#endif // VK_USE_PLATFORM_ANDROID_KHR
#ifdef VK_USE_PLATFORM_ANDROID_KHR
	vkGetMemoryAndroidHardwareBufferANDROID = pExternDeviceDispatchTable->GetMemoryAndroidHardwareBufferANDROID;
#endif // VK_USE_PLATFORM_ANDROID_KHR
#endif

#if defined(VK_EXT_sample_locations)
	// ---- VK_EXT_sample_locations extension commands
	vkCmdSetSampleLocationsEXT = pExternDeviceDispatchTable->CmdSetSampleLocationsEXT;
#endif

#if defined(VK_EXT_image_drm_format_modifier)
	// ---- VK_EXT_image_drm_format_modifier extension commands
	vkGetImageDrmFormatModifierPropertiesEXT = pExternDeviceDispatchTable->GetImageDrmFormatModifierPropertiesEXT;
#endif

#if defined(VK_EXT_validation_cache)
	// ---- VK_EXT_validation_cache extension commands
	vkCreateValidationCacheEXT = pExternDeviceDispatchTable->CreateValidationCacheEXT;
	vkDestroyValidationCacheEXT = pExternDeviceDispatchTable->DestroyValidationCacheEXT;
	vkMergeValidationCachesEXT = pExternDeviceDispatchTable->MergeValidationCachesEXT;
	vkGetValidationCacheDataEXT = pExternDeviceDispatchTable->GetValidationCacheDataEXT;
#endif

#if defined(VK_NV_shading_rate_image)
	// ---- VK_NV_shading_rate_image extension commands
	vkCmdBindShadingRateImageNV = pExternDeviceDispatchTable->CmdBindShadingRateImageNV;
	vkCmdSetViewportShadingRatePaletteNV = pExternDeviceDispatchTable->CmdSetViewportShadingRatePaletteNV;
	vkCmdSetCoarseSampleOrderNV = pExternDeviceDispatchTable->CmdSetCoarseSampleOrderNV;
#endif

#if defined(VK_NV_ray_tracing)
	// ---- VK_NV_ray_tracing extension commands
	vkCreateAccelerationStructureNV = pExternDeviceDispatchTable->CreateAccelerationStructureNV;
	vkDestroyAccelerationStructureNV = pExternDeviceDispatchTable->DestroyAccelerationStructureNV;
	vkGetAccelerationStructureMemoryRequirementsNV = pExternDeviceDispatchTable->GetAccelerationStructureMemoryRequirementsNV;
	vkBindAccelerationStructureMemoryNV = pExternDeviceDispatchTable->BindAccelerationStructureMemoryNV;
	vkCmdBuildAccelerationStructureNV = pExternDeviceDispatchTable->CmdBuildAccelerationStructureNV;
	vkCmdCopyAccelerationStructureNV = pExternDeviceDispatchTable->CmdCopyAccelerationStructureNV;
	vkCmdTraceRaysNV = pExternDeviceDispatchTable->CmdTraceRaysNV;
	vkCreateRayTracingPipelinesNV = pExternDeviceDispatchTable->CreateRayTracingPipelinesNV;
	vkGetRayTracingShaderGroupHandlesNV = pExternDeviceDispatchTable->GetRayTracingShaderGroupHandlesNV;
	vkGetAccelerationStructureHandleNV = pExternDeviceDispatchTable->GetAccelerationStructureHandleNV;
	vkCmdWriteAccelerationStructuresPropertiesNV = pExternDeviceDispatchTable->CmdWriteAccelerationStructuresPropertiesNV;
	vkCompileDeferredNV = pExternDeviceDispatchTable->CompileDeferredNV;
#endif

#if defined(VK_EXT_external_memory_host)
	// ---- VK_EXT_external_memory_host extension commands
	vkGetMemoryHostPointerPropertiesEXT = pExternDeviceDispatchTable->GetMemoryHostPointerPropertiesEXT;
#endif

#if defined(VK_AMD_buffer_marker)
	// ---- VK_AMD_buffer_marker extension commands
	vkCmdWriteBufferMarkerAMD = pExternDeviceDispatchTable->CmdWriteBufferMarkerAMD;
#endif

#if defined(VK_EXT_calibrated_timestamps)
	// ---- VK_EXT_calibrated_timestamps extension commands
	vkGetCalibratedTimestampsEXT = pExternDeviceDispatchTable->GetCalibratedTimestampsEXT;
#endif

#if defined(VK_NV_mesh_shader)
	// ---- VK_NV_mesh_shader extension commands
	vkCmdDrawMeshTasksNV = pExternDeviceDispatchTable->CmdDrawMeshTasksNV;
	vkCmdDrawMeshTasksIndirectNV = pExternDeviceDispatchTable->CmdDrawMeshTasksIndirectNV;
	vkCmdDrawMeshTasksIndirectCountNV = pExternDeviceDispatchTable->CmdDrawMeshTasksIndirectCountNV;
#endif

#if defined(VK_NV_scissor_exclusive)
	// ---- VK_NV_scissor_exclusive extension commands
	vkCmdSetExclusiveScissorNV = pExternDeviceDispatchTable->CmdSetExclusiveScissorNV;
#endif

#if defined(VK_NV_device_diagnostic_checkpoints)
	// ---- VK_NV_device_diagnostic_checkpoints extension commands
	vkCmdSetCheckpointNV = pExternDeviceDispatchTable->CmdSetCheckpointNV;
	vkGetQueueCheckpointDataNV = pExternDeviceDispatchTable->GetQueueCheckpointDataNV;
#endif

#if defined(VK_EXT_buffer_device_address)
	// ---- VK_EXT_buffer_device_address extension commands
	vkGetBufferDeviceAddressEXT = pExternDeviceDispatchTable->GetBufferDeviceAddressEXT;
#endif

	return VK_SUCCESS;
}

VkResult wrapDispatchableVkObject(const void** pObj)
{
	// Validate data
	if (!pExternInstanceDispatchTable ||
		!pExternDeviceDispatchTable ||
		!pExternVkInstance ||
		!pExternVkGPU ||
		!pExternVkDevice)
		return VK_ERROR_INITIALIZATION_FAILED;

	VkResult ret = VK_SUCCESS;

	if (!pfnDevInit) 
	{
		*(pObj) = *(void**)pExternVkDevice;
	}
	else 
	{
		ret = pfnDevInit(pExternVkDevice, (void*)pObj);
	}
	
	return ret;
}

#ifdef __cplusplus
}
#endif

#endif

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
#include "volk.h"
#include "volkForgeExt.h"

#if defined(VK_USE_DISPATCH_TABLES)
extern VkLayerInstanceDispatchTable gExternInstanceDispatchTable;
extern VkLayerDispatchTable gExternDeviceDispatchTable;
extern VkInstance pExternVkInstance;
extern VkPhysicalDevice pExternVkGPU;
extern VkDevice pExternVkDevice;

#ifdef __cplusplus
extern "C" {
#endif

VkResult volkInitializeWithDispatchTables(Renderer* pRenderer)
{
	// Validate data
	if (!pRenderer ||
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
	vkCreateInstance = gExternInstanceDispatchTable.CreateInstance;
	vkDestroyInstance = gExternInstanceDispatchTable.DestroyInstance;
	vkEnumeratePhysicalDevices = gExternInstanceDispatchTable.EnumeratePhysicalDevices;
	vkGetPhysicalDeviceFeatures = gExternInstanceDispatchTable.GetPhysicalDeviceFeatures;
	vkGetPhysicalDeviceFormatProperties = gExternInstanceDispatchTable.GetPhysicalDeviceFormatProperties;
	vkGetPhysicalDeviceImageFormatProperties = gExternInstanceDispatchTable.GetPhysicalDeviceImageFormatProperties;
	vkGetPhysicalDeviceProperties = gExternInstanceDispatchTable.GetPhysicalDeviceProperties;
	vkGetPhysicalDeviceQueueFamilyProperties = gExternInstanceDispatchTable.GetPhysicalDeviceQueueFamilyProperties;
	vkGetPhysicalDeviceMemoryProperties = gExternInstanceDispatchTable.GetPhysicalDeviceMemoryProperties;
	vkGetInstanceProcAddr = gExternInstanceDispatchTable.GetInstanceProcAddr;
	vkCreateDevice = gExternInstanceDispatchTable.CreateDevice;
	vkEnumerateInstanceExtensionProperties = gExternInstanceDispatchTable.EnumerateInstanceExtensionProperties;
	vkEnumerateDeviceExtensionProperties = gExternInstanceDispatchTable.EnumerateDeviceExtensionProperties;
	vkEnumerateInstanceLayerProperties = gExternInstanceDispatchTable.EnumerateInstanceLayerProperties;
	vkEnumerateDeviceLayerProperties = gExternInstanceDispatchTable.EnumerateDeviceLayerProperties;
	vkGetPhysicalDeviceSparseImageFormatProperties = gExternInstanceDispatchTable.GetPhysicalDeviceSparseImageFormatProperties;
#endif

#if defined(VK_VERSION_1_1)
	// ---- Core 1_1 commands
	vkEnumerateInstanceVersion = gExternInstanceDispatchTable.EnumerateInstanceVersion;
	vkEnumeratePhysicalDeviceGroups = gExternInstanceDispatchTable.EnumeratePhysicalDeviceGroups;
	vkGetPhysicalDeviceFeatures2 = gExternInstanceDispatchTable.GetPhysicalDeviceFeatures2;
	vkGetPhysicalDeviceProperties2 = gExternInstanceDispatchTable.GetPhysicalDeviceProperties2;
	vkGetPhysicalDeviceFormatProperties2 = gExternInstanceDispatchTable.GetPhysicalDeviceFormatProperties2;
	vkGetPhysicalDeviceImageFormatProperties2 = gExternInstanceDispatchTable.GetPhysicalDeviceImageFormatProperties2;
	vkGetPhysicalDeviceQueueFamilyProperties2 = gExternInstanceDispatchTable.GetPhysicalDeviceQueueFamilyProperties2;
	vkGetPhysicalDeviceMemoryProperties2 = gExternInstanceDispatchTable.GetPhysicalDeviceMemoryProperties2;
	vkGetPhysicalDeviceSparseImageFormatProperties2 = gExternInstanceDispatchTable.GetPhysicalDeviceSparseImageFormatProperties2;
	vkGetPhysicalDeviceExternalBufferProperties = gExternInstanceDispatchTable.GetPhysicalDeviceExternalBufferProperties;
	vkGetPhysicalDeviceExternalFenceProperties = gExternInstanceDispatchTable.GetPhysicalDeviceExternalFenceProperties;
	vkGetPhysicalDeviceExternalSemaphoreProperties = gExternInstanceDispatchTable.GetPhysicalDeviceExternalSemaphoreProperties;
#endif

#if defined(VK_KHR_surface)
	// ---- VK_KHR_surface extension commands
	vkDestroySurfaceKHR = gExternInstanceDispatchTable.DestroySurfaceKHR;
	vkGetPhysicalDeviceSurfaceSupportKHR = gExternInstanceDispatchTable.GetPhysicalDeviceSurfaceSupportKHR;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR = gExternInstanceDispatchTable.GetPhysicalDeviceSurfaceCapabilitiesKHR;
	vkGetPhysicalDeviceSurfaceFormatsKHR = gExternInstanceDispatchTable.GetPhysicalDeviceSurfaceFormatsKHR;
	vkGetPhysicalDeviceSurfacePresentModesKHR = gExternInstanceDispatchTable.GetPhysicalDeviceSurfacePresentModesKHR;
#endif

#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
	// ---- VK_KHR_swapchain extension commands
	vkGetPhysicalDevicePresentRectanglesKHR = gExternInstanceDispatchTable.GetPhysicalDevicePresentRectanglesKHR;
#endif

#if defined(VK_KHR_display)
	// ---- VK_KHR_display extension commands
	vkGetPhysicalDeviceDisplayPropertiesKHR = gExternInstanceDispatchTable.GetPhysicalDeviceDisplayPropertiesKHR;
	vkGetPhysicalDeviceDisplayPlanePropertiesKHR = gExternInstanceDispatchTable.GetPhysicalDeviceDisplayPlanePropertiesKHR;
	vkGetDisplayPlaneSupportedDisplaysKHR = gExternInstanceDispatchTable.GetDisplayPlaneSupportedDisplaysKHR;
	vkGetDisplayModePropertiesKHR = gExternInstanceDispatchTable.GetDisplayModePropertiesKHR;
	vkCreateDisplayModeKHR = gExternInstanceDispatchTable.CreateDisplayModeKHR;
	vkGetDisplayPlaneCapabilitiesKHR = gExternInstanceDispatchTable.GetDisplayPlaneCapabilitiesKHR;
	vkCreateDisplayPlaneSurfaceKHR = gExternInstanceDispatchTable.CreateDisplayPlaneSurfaceKHR;
#endif

#if defined(VK_KHR_xlib_surface)
	// ---- VK_KHR_xlib_surface extension commands
#ifdef VK_USE_PLATFORM_XLIB_KHR
	vkCreateXlibSurfaceKHR = gExternInstanceDispatchTable.CreateXlibSurfaceKHR;
#endif // VK_USE_PLATFORM_XLIB_KHR
#ifdef VK_USE_PLATFORM_XLIB_KHR
	vkGetPhysicalDeviceXlibPresentationSupportKHR = gExternInstanceDispatchTable.GetPhysicalDeviceXlibPresentationSupportKHR;
#endif // VK_USE_PLATFORM_XLIB_KHR
#endif

#if defined(VK_KHR_xcb_surface)
	// ---- VK_KHR_xcb_surface extension commands
#ifdef VK_USE_PLATFORM_XCB_KHR
	vkCreateXcbSurfaceKHR = gExternInstanceDispatchTable.CreateXcbSurfaceKHR;
#endif // VK_USE_PLATFORM_XCB_KHR
#ifdef VK_USE_PLATFORM_XCB_KHR
	vkGetPhysicalDeviceXcbPresentationSupportKHR = gExternInstanceDispatchTable.GetPhysicalDeviceXcbPresentationSupportKHR;
#endif // VK_USE_PLATFORM_XCB_KHR
#endif

#if defined(VK_KHR_wayland_surface)
	// ---- VK_KHR_wayland_surface extension commands
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
	vkCreateWaylandSurfaceKHR = gExternInstanceDispatchTable.CreateWaylandSurfaceKHR;
#endif // VK_USE_PLATFORM_WAYLAND_KHR
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
	vkGetPhysicalDeviceWaylandPresentationSupportKHR = gExternInstanceDispatchTable.GetPhysicalDeviceWaylandPresentationSupportKHR;
#endif // VK_USE_PLATFORM_WAYLAND_KHR
#endif

#if defined(VK_KHR_android_surface)
	// ---- VK_KHR_android_surface extension commands
#ifdef VK_USE_PLATFORM_ANDROID_KHR
	vkCreateAndroidSurfaceKHR = gExternInstanceDispatchTable.CreateAndroidSurfaceKHR;
#endif // VK_USE_PLATFORM_ANDROID_KHR
#endif

#if defined(VK_KHR_win32_surface)
	// ---- VK_KHR_win32_surface extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkCreateWin32SurfaceKHR = gExternInstanceDispatchTable.CreateWin32SurfaceKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkGetPhysicalDeviceWin32PresentationSupportKHR = gExternInstanceDispatchTable.GetPhysicalDeviceWin32PresentationSupportKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#endif

#if defined(VK_KHR_get_physical_device_properties2)
	// ---- VK_KHR_get_physical_device_properties2 extension commands
	vkGetPhysicalDeviceFeatures2KHR = gExternInstanceDispatchTable.GetPhysicalDeviceFeatures2KHR;
	vkGetPhysicalDeviceProperties2KHR = gExternInstanceDispatchTable.GetPhysicalDeviceProperties2KHR;
	vkGetPhysicalDeviceFormatProperties2KHR = gExternInstanceDispatchTable.GetPhysicalDeviceFormatProperties2KHR;
	vkGetPhysicalDeviceImageFormatProperties2KHR = gExternInstanceDispatchTable.GetPhysicalDeviceImageFormatProperties2KHR;
	vkGetPhysicalDeviceQueueFamilyProperties2KHR = gExternInstanceDispatchTable.GetPhysicalDeviceQueueFamilyProperties2KHR;
	vkGetPhysicalDeviceMemoryProperties2KHR = gExternInstanceDispatchTable.GetPhysicalDeviceMemoryProperties2KHR;
	vkGetPhysicalDeviceSparseImageFormatProperties2KHR = gExternInstanceDispatchTable.GetPhysicalDeviceSparseImageFormatProperties2KHR;
#endif

#if defined(VK_KHR_device_group_creation)
	// ---- VK_KHR_device_group_creation extension commands
	vkEnumeratePhysicalDeviceGroupsKHR = gExternInstanceDispatchTable.EnumeratePhysicalDeviceGroupsKHR;
#endif

#if defined(VK_KHR_external_memory_capabilities)
	// ---- VK_KHR_external_memory_capabilities extension commands
	vkGetPhysicalDeviceExternalBufferPropertiesKHR = gExternInstanceDispatchTable.GetPhysicalDeviceExternalBufferPropertiesKHR;
#endif

#if defined(VK_KHR_external_semaphore_capabilities)
	// ---- VK_KHR_external_semaphore_capabilities extension commands
	vkGetPhysicalDeviceExternalSemaphorePropertiesKHR = gExternInstanceDispatchTable.GetPhysicalDeviceExternalSemaphorePropertiesKHR;
#endif

#if defined(VK_KHR_external_fence_capabilities)
	// ---- VK_KHR_external_fence_capabilities extension commands
	vkGetPhysicalDeviceExternalFencePropertiesKHR = gExternInstanceDispatchTable.GetPhysicalDeviceExternalFencePropertiesKHR;
#endif

#if defined(VK_KHR_get_surface_capabilities2)
	// ---- VK_KHR_get_surface_capabilities2 extension commands
	vkGetPhysicalDeviceSurfaceCapabilities2KHR = gExternInstanceDispatchTable.GetPhysicalDeviceSurfaceCapabilities2KHR;
	vkGetPhysicalDeviceSurfaceFormats2KHR = gExternInstanceDispatchTable.GetPhysicalDeviceSurfaceFormats2KHR;
#endif

#if defined(VK_KHR_get_display_properties2)
	// ---- VK_KHR_get_display_properties2 extension commands
	vkGetPhysicalDeviceDisplayProperties2KHR = gExternInstanceDispatchTable.GetPhysicalDeviceDisplayProperties2KHR;
	vkGetPhysicalDeviceDisplayPlaneProperties2KHR = gExternInstanceDispatchTable.GetPhysicalDeviceDisplayPlaneProperties2KHR;
	vkGetDisplayModeProperties2KHR = gExternInstanceDispatchTable.GetDisplayModeProperties2KHR;
	vkGetDisplayPlaneCapabilities2KHR = gExternInstanceDispatchTable.GetDisplayPlaneCapabilities2KHR;
#endif

#if defined(VK_EXT_debug_report)
	// ---- VK_EXT_debug_report extension commands
	vkCreateDebugReportCallbackEXT = gExternInstanceDispatchTable.CreateDebugReportCallbackEXT;
	vkDestroyDebugReportCallbackEXT = gExternInstanceDispatchTable.DestroyDebugReportCallbackEXT;
	vkDebugReportMessageEXT = gExternInstanceDispatchTable.DebugReportMessageEXT;
#endif

#if defined(VK_NV_external_memory_capabilities)
	// ---- VK_NV_external_memory_capabilities extension commands
	vkGetPhysicalDeviceExternalImageFormatPropertiesNV = gExternInstanceDispatchTable.GetPhysicalDeviceExternalImageFormatPropertiesNV;
#endif

#if defined(VK_NN_vi_surface)
	// ---- VK_NN_vi_surface extension commands
#ifdef VK_USE_PLATFORM_VI_NN
	vkCreateViSurfaceNN = gExternInstanceDispatchTable.CreateViSurfaceNN;
#endif // VK_USE_PLATFORM_VI_NN
#endif

#if defined(VK_NVX_device_generated_commands)
	// ---- VK_NVX_device_generated_commands extension commands
	vkGetPhysicalDeviceGeneratedCommandsPropertiesNVX = gExternInstanceDispatchTable.GetPhysicalDeviceGeneratedCommandsPropertiesNVX;
#endif

#if defined(VK_EXT_direct_mode_display)
	// ---- VK_EXT_direct_mode_display extension commands
	vkReleaseDisplayEXT = gExternInstanceDispatchTable.ReleaseDisplayEXT;
#endif

#if defined(VK_EXT_acquire_xlib_display)
	// ---- VK_EXT_acquire_xlib_display extension commands
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
	vkAcquireXlibDisplayEXT = gExternInstanceDispatchTable.AcquireXlibDisplayEXT;
#endif // VK_USE_PLATFORM_XLIB_XRANDR_EXT
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
	vkGetRandROutputDisplayEXT = gExternInstanceDispatchTable.GetRandROutputDisplayEXT;
#endif // VK_USE_PLATFORM_XLIB_XRANDR_EXT
#endif

#if defined(VK_EXT_display_surface_counter)
	// ---- VK_EXT_display_surface_counter extension commands
	vkGetPhysicalDeviceSurfaceCapabilities2EXT = gExternInstanceDispatchTable.GetPhysicalDeviceSurfaceCapabilities2EXT;
#endif

#if defined(VK_MVK_ios_surface)
	// ---- VK_MVK_ios_surface extension commands
#ifdef VK_USE_PLATFORM_IOS_MVK
	vkCreateIOSSurfaceMVK = gExternInstanceDispatchTable.CreateIOSSurfaceMVK;
#endif // VK_USE_PLATFORM_IOS_MVK
#endif

#if defined(VK_MVK_macos_surface)
	// ---- VK_MVK_macos_surface extension commands
#ifdef VK_USE_PLATFORM_MACOS_MVK
	vkCreateMacOSSurfaceMVK = gExternInstanceDispatchTable.CreateMacOSSurfaceMVK;
#endif // VK_USE_PLATFORM_MACOS_MVK
#endif

#if defined(VK_EXT_debug_utils)
	// ---- VK_EXT_debug_utils extension commands
	vkCreateDebugUtilsMessengerEXT = gExternInstanceDispatchTable.CreateDebugUtilsMessengerEXT;
	vkDestroyDebugUtilsMessengerEXT = gExternInstanceDispatchTable.DestroyDebugUtilsMessengerEXT;
	vkSubmitDebugUtilsMessageEXT = gExternInstanceDispatchTable.SubmitDebugUtilsMessageEXT;
#endif

#if defined(VK_EXT_sample_locations)
	// ---- VK_EXT_sample_locations extension commands
	vkGetPhysicalDeviceMultisamplePropertiesEXT = gExternInstanceDispatchTable.GetPhysicalDeviceMultisamplePropertiesEXT;
#endif

#if defined(VK_EXT_calibrated_timestamps)
	// ---- VK_EXT_calibrated_timestamps extension commands
	vkGetPhysicalDeviceCalibrateableTimeDomainsEXT = gExternInstanceDispatchTable.GetPhysicalDeviceCalibrateableTimeDomainsEXT;
#endif

#if defined(VK_FUCHSIA_imagepipe_surface)
	// ---- VK_FUCHSIA_imagepipe_surface extension commands
#ifdef VK_USE_PLATFORM_FUCHSIA
	vkCreateImagePipeSurfaceFUCHSIA = gExternInstanceDispatchTable.CreateImagePipeSurfaceFUCHSIA;
#endif // VK_USE_PLATFORM_FUCHSIA
#endif

#if defined(VK_NV_cooperative_matrix)
	// ---- VK_NV_cooperative_matrix extension commands
	vkGetPhysicalDeviceCooperativeMatrixPropertiesNV = gExternInstanceDispatchTable.GetPhysicalDeviceCooperativeMatrixPropertiesNV;
#endif



#if defined(VK_VERSION_1_0)
	// ---- Core 1_0 commands
	vkGetDeviceProcAddr = gExternDeviceDispatchTable.GetDeviceProcAddr;
	vkDestroyDevice = gExternDeviceDispatchTable.DestroyDevice;
	vkGetDeviceQueue = gExternDeviceDispatchTable.GetDeviceQueue;
	vkQueueSubmit = gExternDeviceDispatchTable.QueueSubmit;
	vkQueueWaitIdle = gExternDeviceDispatchTable.QueueWaitIdle;
	vkDeviceWaitIdle = gExternDeviceDispatchTable.DeviceWaitIdle;
	vkAllocateMemory = gExternDeviceDispatchTable.AllocateMemory;
	vkFreeMemory = gExternDeviceDispatchTable.FreeMemory;
	vkMapMemory = gExternDeviceDispatchTable.MapMemory;
	vkUnmapMemory = gExternDeviceDispatchTable.UnmapMemory;
	vkFlushMappedMemoryRanges = gExternDeviceDispatchTable.FlushMappedMemoryRanges;
	vkInvalidateMappedMemoryRanges = gExternDeviceDispatchTable.InvalidateMappedMemoryRanges;
	vkGetDeviceMemoryCommitment = gExternDeviceDispatchTable.GetDeviceMemoryCommitment;
	vkBindBufferMemory = gExternDeviceDispatchTable.BindBufferMemory;
	vkBindImageMemory = gExternDeviceDispatchTable.BindImageMemory;
	vkGetBufferMemoryRequirements = gExternDeviceDispatchTable.GetBufferMemoryRequirements;
	vkGetImageMemoryRequirements = gExternDeviceDispatchTable.GetImageMemoryRequirements;
	vkGetImageSparseMemoryRequirements = gExternDeviceDispatchTable.GetImageSparseMemoryRequirements;
	vkQueueBindSparse = gExternDeviceDispatchTable.QueueBindSparse;
	vkCreateFence = gExternDeviceDispatchTable.CreateFence;
	vkDestroyFence = gExternDeviceDispatchTable.DestroyFence;
	vkResetFences = gExternDeviceDispatchTable.ResetFences;
	vkGetFenceStatus = gExternDeviceDispatchTable.GetFenceStatus;
	vkWaitForFences = gExternDeviceDispatchTable.WaitForFences;
	vkCreateSemaphore = gExternDeviceDispatchTable.CreateSemaphore;
	vkDestroySemaphore = gExternDeviceDispatchTable.DestroySemaphore;
	vkCreateEvent = gExternDeviceDispatchTable.CreateEvent;
	vkDestroyEvent = gExternDeviceDispatchTable.DestroyEvent;
	vkGetEventStatus = gExternDeviceDispatchTable.GetEventStatus;
	vkSetEvent = gExternDeviceDispatchTable.SetEvent;
	vkResetEvent = gExternDeviceDispatchTable.ResetEvent;
	vkCreateQueryPool = gExternDeviceDispatchTable.CreateQueryPool;
	vkDestroyQueryPool = gExternDeviceDispatchTable.DestroyQueryPool;
	vkGetQueryPoolResults = gExternDeviceDispatchTable.GetQueryPoolResults;
	vkCreateBuffer = gExternDeviceDispatchTable.CreateBuffer;
	vkDestroyBuffer = gExternDeviceDispatchTable.DestroyBuffer;
	vkCreateBufferView = gExternDeviceDispatchTable.CreateBufferView;
	vkDestroyBufferView = gExternDeviceDispatchTable.DestroyBufferView;
	vkCreateImage = gExternDeviceDispatchTable.CreateImage;
	vkDestroyImage = gExternDeviceDispatchTable.DestroyImage;
	vkGetImageSubresourceLayout = gExternDeviceDispatchTable.GetImageSubresourceLayout;
	vkCreateImageView = gExternDeviceDispatchTable.CreateImageView;
	vkDestroyImageView = gExternDeviceDispatchTable.DestroyImageView;
	vkCreateShaderModule = gExternDeviceDispatchTable.CreateShaderModule;
	vkDestroyShaderModule = gExternDeviceDispatchTable.DestroyShaderModule;
	vkCreatePipelineCache = gExternDeviceDispatchTable.CreatePipelineCache;
	vkDestroyPipelineCache = gExternDeviceDispatchTable.DestroyPipelineCache;
	vkGetPipelineCacheData = gExternDeviceDispatchTable.GetPipelineCacheData;
	vkMergePipelineCaches = gExternDeviceDispatchTable.MergePipelineCaches;
	vkCreateGraphicsPipelines = gExternDeviceDispatchTable.CreateGraphicsPipelines;
	vkCreateComputePipelines = gExternDeviceDispatchTable.CreateComputePipelines;
	vkDestroyPipeline = gExternDeviceDispatchTable.DestroyPipeline;
	vkCreatePipelineLayout = gExternDeviceDispatchTable.CreatePipelineLayout;
	vkDestroyPipelineLayout = gExternDeviceDispatchTable.DestroyPipelineLayout;
	vkCreateSampler = gExternDeviceDispatchTable.CreateSampler;
	vkDestroySampler = gExternDeviceDispatchTable.DestroySampler;
	vkCreateDescriptorSetLayout = gExternDeviceDispatchTable.CreateDescriptorSetLayout;
	vkDestroyDescriptorSetLayout = gExternDeviceDispatchTable.DestroyDescriptorSetLayout;
	vkCreateDescriptorPool = gExternDeviceDispatchTable.CreateDescriptorPool;
	vkDestroyDescriptorPool = gExternDeviceDispatchTable.DestroyDescriptorPool;
	vkResetDescriptorPool = gExternDeviceDispatchTable.ResetDescriptorPool;
	vkAllocateDescriptorSets = gExternDeviceDispatchTable.AllocateDescriptorSets;
	vkFreeDescriptorSets = gExternDeviceDispatchTable.FreeDescriptorSets;
	vkUpdateDescriptorSets = gExternDeviceDispatchTable.UpdateDescriptorSets;
	vkCreateFramebuffer = gExternDeviceDispatchTable.CreateFramebuffer;
	vkDestroyFramebuffer = gExternDeviceDispatchTable.DestroyFramebuffer;
	vkCreateRenderPass = gExternDeviceDispatchTable.CreateRenderPass;
	vkDestroyRenderPass = gExternDeviceDispatchTable.DestroyRenderPass;
	vkGetRenderAreaGranularity = gExternDeviceDispatchTable.GetRenderAreaGranularity;
	vkCreateCommandPool = gExternDeviceDispatchTable.CreateCommandPool;
	vkDestroyCommandPool = gExternDeviceDispatchTable.DestroyCommandPool;
	vkResetCommandPool = gExternDeviceDispatchTable.ResetCommandPool;
	vkAllocateCommandBuffers = gExternDeviceDispatchTable.AllocateCommandBuffers;
	vkFreeCommandBuffers = gExternDeviceDispatchTable.FreeCommandBuffers;
	vkBeginCommandBuffer = gExternDeviceDispatchTable.BeginCommandBuffer;
	vkEndCommandBuffer = gExternDeviceDispatchTable.EndCommandBuffer;
	vkResetCommandBuffer = gExternDeviceDispatchTable.ResetCommandBuffer;
	vkCmdBindPipeline = gExternDeviceDispatchTable.CmdBindPipeline;
	vkCmdSetViewport = gExternDeviceDispatchTable.CmdSetViewport;
	vkCmdSetScissor = gExternDeviceDispatchTable.CmdSetScissor;
	vkCmdSetLineWidth = gExternDeviceDispatchTable.CmdSetLineWidth;
	vkCmdSetDepthBias = gExternDeviceDispatchTable.CmdSetDepthBias;
	vkCmdSetBlendConstants = gExternDeviceDispatchTable.CmdSetBlendConstants;
	vkCmdSetDepthBounds = gExternDeviceDispatchTable.CmdSetDepthBounds;
	vkCmdSetStencilCompareMask = gExternDeviceDispatchTable.CmdSetStencilCompareMask;
	vkCmdSetStencilWriteMask = gExternDeviceDispatchTable.CmdSetStencilWriteMask;
	vkCmdSetStencilReference = gExternDeviceDispatchTable.CmdSetStencilReference;
	vkCmdBindDescriptorSets = gExternDeviceDispatchTable.CmdBindDescriptorSets;
	vkCmdBindIndexBuffer = gExternDeviceDispatchTable.CmdBindIndexBuffer;
	vkCmdBindVertexBuffers = gExternDeviceDispatchTable.CmdBindVertexBuffers;
	vkCmdDraw = gExternDeviceDispatchTable.CmdDraw;
	vkCmdDrawIndexed = gExternDeviceDispatchTable.CmdDrawIndexed;
	vkCmdDrawIndirect = gExternDeviceDispatchTable.CmdDrawIndirect;
	vkCmdDrawIndexedIndirect = gExternDeviceDispatchTable.CmdDrawIndexedIndirect;
	vkCmdDispatch = gExternDeviceDispatchTable.CmdDispatch;
	vkCmdDispatchIndirect = gExternDeviceDispatchTable.CmdDispatchIndirect;
	vkCmdCopyBuffer = gExternDeviceDispatchTable.CmdCopyBuffer;
	vkCmdCopyImage = gExternDeviceDispatchTable.CmdCopyImage;
	vkCmdBlitImage = gExternDeviceDispatchTable.CmdBlitImage;
	vkCmdCopyBufferToImage = gExternDeviceDispatchTable.CmdCopyBufferToImage;
	vkCmdCopyImageToBuffer = gExternDeviceDispatchTable.CmdCopyImageToBuffer;
	vkCmdUpdateBuffer = gExternDeviceDispatchTable.CmdUpdateBuffer;
	vkCmdFillBuffer = gExternDeviceDispatchTable.CmdFillBuffer;
	vkCmdClearColorImage = gExternDeviceDispatchTable.CmdClearColorImage;
	vkCmdClearDepthStencilImage = gExternDeviceDispatchTable.CmdClearDepthStencilImage;
	vkCmdClearAttachments = gExternDeviceDispatchTable.CmdClearAttachments;
	vkCmdResolveImage = gExternDeviceDispatchTable.CmdResolveImage;
	vkCmdSetEvent = gExternDeviceDispatchTable.CmdSetEvent;
	vkCmdResetEvent = gExternDeviceDispatchTable.CmdResetEvent;
	vkCmdWaitEvents = gExternDeviceDispatchTable.CmdWaitEvents;
	vkCmdPipelineBarrier = gExternDeviceDispatchTable.CmdPipelineBarrier;
	vkCmdBeginQuery = gExternDeviceDispatchTable.CmdBeginQuery;
	vkCmdEndQuery = gExternDeviceDispatchTable.CmdEndQuery;
	vkCmdResetQueryPool = gExternDeviceDispatchTable.CmdResetQueryPool;
	vkCmdWriteTimestamp = gExternDeviceDispatchTable.CmdWriteTimestamp;
	vkCmdCopyQueryPoolResults = gExternDeviceDispatchTable.CmdCopyQueryPoolResults;
	vkCmdPushConstants = gExternDeviceDispatchTable.CmdPushConstants;
	vkCmdBeginRenderPass = gExternDeviceDispatchTable.CmdBeginRenderPass;
	vkCmdNextSubpass = gExternDeviceDispatchTable.CmdNextSubpass;
	vkCmdEndRenderPass = gExternDeviceDispatchTable.CmdEndRenderPass;
	vkCmdExecuteCommands = gExternDeviceDispatchTable.CmdExecuteCommands;
#endif

#if defined(VK_VERSION_1_1)
	// ---- Core 1_1 commands
	vkBindBufferMemory2 = gExternDeviceDispatchTable.BindBufferMemory2;
	vkBindImageMemory2 = gExternDeviceDispatchTable.BindImageMemory2;
	vkGetDeviceGroupPeerMemoryFeatures = gExternDeviceDispatchTable.GetDeviceGroupPeerMemoryFeatures;
	vkCmdSetDeviceMask = gExternDeviceDispatchTable.CmdSetDeviceMask;
	vkCmdDispatchBase = gExternDeviceDispatchTable.CmdDispatchBase;
	vkGetImageMemoryRequirements2 = gExternDeviceDispatchTable.GetImageMemoryRequirements2;
	vkGetBufferMemoryRequirements2 = gExternDeviceDispatchTable.GetBufferMemoryRequirements2;
	vkGetImageSparseMemoryRequirements2 = gExternDeviceDispatchTable.GetImageSparseMemoryRequirements2;
	vkTrimCommandPool = gExternDeviceDispatchTable.TrimCommandPool;
	vkGetDeviceQueue2 = gExternDeviceDispatchTable.GetDeviceQueue2;
	vkCreateSamplerYcbcrConversion = gExternDeviceDispatchTable.CreateSamplerYcbcrConversion;
	vkDestroySamplerYcbcrConversion = gExternDeviceDispatchTable.DestroySamplerYcbcrConversion;
	vkCreateDescriptorUpdateTemplate = gExternDeviceDispatchTable.CreateDescriptorUpdateTemplate;
	vkDestroyDescriptorUpdateTemplate = gExternDeviceDispatchTable.DestroyDescriptorUpdateTemplate;
	vkUpdateDescriptorSetWithTemplate = gExternDeviceDispatchTable.UpdateDescriptorSetWithTemplate;
	vkGetDescriptorSetLayoutSupport = gExternDeviceDispatchTable.GetDescriptorSetLayoutSupport;
#endif

#if defined(VK_KHR_swapchain)
	// ---- VK_KHR_swapchain extension commands
	vkCreateSwapchainKHR = gExternDeviceDispatchTable.CreateSwapchainKHR;
	vkDestroySwapchainKHR = gExternDeviceDispatchTable.DestroySwapchainKHR;
	vkGetSwapchainImagesKHR = gExternDeviceDispatchTable.GetSwapchainImagesKHR;
	vkAcquireNextImageKHR = gExternDeviceDispatchTable.AcquireNextImageKHR;
	vkQueuePresentKHR = gExternDeviceDispatchTable.QueuePresentKHR;
	vkGetDeviceGroupPresentCapabilitiesKHR = gExternDeviceDispatchTable.GetDeviceGroupPresentCapabilitiesKHR;
	vkGetDeviceGroupSurfacePresentModesKHR = gExternDeviceDispatchTable.GetDeviceGroupSurfacePresentModesKHR;
	vkAcquireNextImage2KHR = gExternDeviceDispatchTable.AcquireNextImage2KHR;
#endif

#if defined(VK_KHR_display_swapchain)
	// ---- VK_KHR_display_swapchain extension commands
	vkCreateSharedSwapchainsKHR = gExternDeviceDispatchTable.CreateSharedSwapchainsKHR;
#endif

#if defined(VK_KHR_device_group)
	// ---- VK_KHR_device_group extension commands
	vkGetDeviceGroupPeerMemoryFeaturesKHR = gExternDeviceDispatchTable.GetDeviceGroupPeerMemoryFeaturesKHR;
	vkCmdSetDeviceMaskKHR = gExternDeviceDispatchTable.CmdSetDeviceMaskKHR;
	vkCmdDispatchBaseKHR = gExternDeviceDispatchTable.CmdDispatchBaseKHR;
#endif

#if defined(VK_KHR_maintenance1)
	// ---- VK_KHR_maintenance1 extension commands
	vkTrimCommandPoolKHR = gExternDeviceDispatchTable.TrimCommandPoolKHR;
#endif

#if defined(VK_KHR_external_memory_win32)
	// ---- VK_KHR_external_memory_win32 extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkGetMemoryWin32HandleKHR = gExternDeviceDispatchTable.GetMemoryWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkGetMemoryWin32HandlePropertiesKHR = gExternDeviceDispatchTable.GetMemoryWin32HandlePropertiesKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#endif

#if defined(VK_KHR_external_memory_fd)
	// ---- VK_KHR_external_memory_fd extension commands
	vkGetMemoryFdKHR = gExternDeviceDispatchTable.GetMemoryFdKHR;
	vkGetMemoryFdPropertiesKHR = gExternDeviceDispatchTable.GetMemoryFdPropertiesKHR;
#endif

#if defined(VK_KHR_external_semaphore_win32)
	// ---- VK_KHR_external_semaphore_win32 extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkImportSemaphoreWin32HandleKHR = gExternDeviceDispatchTable.ImportSemaphoreWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkGetSemaphoreWin32HandleKHR = gExternDeviceDispatchTable.GetSemaphoreWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#endif

#if defined(VK_KHR_external_semaphore_fd)
	// ---- VK_KHR_external_semaphore_fd extension commands
	vkImportSemaphoreFdKHR = gExternDeviceDispatchTable.ImportSemaphoreFdKHR;
	vkGetSemaphoreFdKHR = gExternDeviceDispatchTable.GetSemaphoreFdKHR;
#endif

#if defined(VK_KHR_push_descriptor)
	// ---- VK_KHR_push_descriptor extension commands
	vkCmdPushDescriptorSetKHR = gExternDeviceDispatchTable.CmdPushDescriptorSetKHR;
	vkCmdPushDescriptorSetWithTemplateKHR = gExternDeviceDispatchTable.CmdPushDescriptorSetWithTemplateKHR;
#endif

#if defined(VK_KHR_descriptor_update_template)
	// ---- VK_KHR_descriptor_update_template extension commands
	vkCreateDescriptorUpdateTemplateKHR = gExternDeviceDispatchTable.CreateDescriptorUpdateTemplateKHR;
	vkDestroyDescriptorUpdateTemplateKHR = gExternDeviceDispatchTable.DestroyDescriptorUpdateTemplateKHR;
	vkUpdateDescriptorSetWithTemplateKHR = gExternDeviceDispatchTable.UpdateDescriptorSetWithTemplateKHR;
#endif

#if defined(VK_KHR_create_renderpass2)
	// ---- VK_KHR_create_renderpass2 extension commands
	vkCreateRenderPass2KHR = gExternDeviceDispatchTable.CreateRenderPass2KHR;
	vkCmdBeginRenderPass2KHR = gExternDeviceDispatchTable.CmdBeginRenderPass2KHR;
	vkCmdNextSubpass2KHR = gExternDeviceDispatchTable.CmdNextSubpass2KHR;
	vkCmdEndRenderPass2KHR = gExternDeviceDispatchTable.CmdEndRenderPass2KHR;
#endif

#if defined(VK_KHR_shared_presentable_image)
	// ---- VK_KHR_shared_presentable_image extension commands
	vkGetSwapchainStatusKHR = gExternDeviceDispatchTable.GetSwapchainStatusKHR;
#endif

#if defined(VK_KHR_external_fence_win32)
	// ---- VK_KHR_external_fence_win32 extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkImportFenceWin32HandleKHR = gExternDeviceDispatchTable.ImportFenceWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkGetFenceWin32HandleKHR = gExternDeviceDispatchTable.GetFenceWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#endif

#if defined(VK_KHR_external_fence_fd)
	// ---- VK_KHR_external_fence_fd extension commands
	vkImportFenceFdKHR = gExternDeviceDispatchTable.ImportFenceFdKHR;
	vkGetFenceFdKHR = gExternDeviceDispatchTable.GetFenceFdKHR;
#endif

#if defined(VK_KHR_get_memory_requirements2)
	// ---- VK_KHR_get_memory_requirements2 extension commands
	vkGetImageMemoryRequirements2KHR = gExternDeviceDispatchTable.GetImageMemoryRequirements2KHR;
	vkGetBufferMemoryRequirements2KHR = gExternDeviceDispatchTable.GetBufferMemoryRequirements2KHR;
	vkGetImageSparseMemoryRequirements2KHR = gExternDeviceDispatchTable.GetImageSparseMemoryRequirements2KHR;
#endif

#if defined(VK_KHR_sampler_ycbcr_conversion)
	// ---- VK_KHR_sampler_ycbcr_conversion extension commands
	vkCreateSamplerYcbcrConversionKHR = gExternDeviceDispatchTable.CreateSamplerYcbcrConversionKHR;
	vkDestroySamplerYcbcrConversionKHR = gExternDeviceDispatchTable.DestroySamplerYcbcrConversionKHR;
#endif

#if defined(VK_KHR_bind_memory2)
	// ---- VK_KHR_bind_memory2 extension commands
	vkBindBufferMemory2KHR = gExternDeviceDispatchTable.BindBufferMemory2KHR;
	vkBindImageMemory2KHR = gExternDeviceDispatchTable.BindImageMemory2KHR;
#endif

#if defined(VK_KHR_maintenance3)
	// ---- VK_KHR_maintenance3 extension commands
	vkGetDescriptorSetLayoutSupportKHR = gExternDeviceDispatchTable.GetDescriptorSetLayoutSupportKHR;
#endif

#if defined(VK_KHR_draw_indirect_count)
	// ---- VK_KHR_draw_indirect_count extension commands
	vkCmdDrawIndirectCountKHR = gExternDeviceDispatchTable.CmdDrawIndirectCountKHR;
	vkCmdDrawIndexedIndirectCountKHR = gExternDeviceDispatchTable.CmdDrawIndexedIndirectCountKHR;
#endif

#if defined(VK_EXT_debug_marker)
	// ---- VK_EXT_debug_marker extension commands
	vkDebugMarkerSetObjectTagEXT = gExternDeviceDispatchTable.DebugMarkerSetObjectTagEXT;
	vkDebugMarkerSetObjectNameEXT = gExternDeviceDispatchTable.DebugMarkerSetObjectNameEXT;
	vkCmdDebugMarkerBeginEXT = gExternDeviceDispatchTable.CmdDebugMarkerBeginEXT;
	vkCmdDebugMarkerEndEXT = gExternDeviceDispatchTable.CmdDebugMarkerEndEXT;
	vkCmdDebugMarkerInsertEXT = gExternDeviceDispatchTable.CmdDebugMarkerInsertEXT;
#endif

#if defined(VK_EXT_transform_feedback)
	// ---- VK_EXT_transform_feedback extension commands
	vkCmdBindTransformFeedbackBuffersEXT = gExternDeviceDispatchTable.CmdBindTransformFeedbackBuffersEXT;
	vkCmdBeginTransformFeedbackEXT = gExternDeviceDispatchTable.CmdBeginTransformFeedbackEXT;
	vkCmdEndTransformFeedbackEXT = gExternDeviceDispatchTable.CmdEndTransformFeedbackEXT;
	vkCmdBeginQueryIndexedEXT = gExternDeviceDispatchTable.CmdBeginQueryIndexedEXT;
	vkCmdEndQueryIndexedEXT = gExternDeviceDispatchTable.CmdEndQueryIndexedEXT;
	vkCmdDrawIndirectByteCountEXT = gExternDeviceDispatchTable.CmdDrawIndirectByteCountEXT;
#endif

#if defined(VK_AMD_draw_indirect_count)
	// ---- VK_AMD_draw_indirect_count extension commands
	vkCmdDrawIndirectCountAMD = gExternDeviceDispatchTable.CmdDrawIndirectCountAMD;
	vkCmdDrawIndexedIndirectCountAMD = gExternDeviceDispatchTable.CmdDrawIndexedIndirectCountAMD;
#endif

#if defined(VK_AMD_shader_info)
	// ---- VK_AMD_shader_info extension commands
	vkGetShaderInfoAMD = gExternDeviceDispatchTable.GetShaderInfoAMD;
#endif

#if defined(VK_NV_external_memory_win32)
	// ---- VK_NV_external_memory_win32 extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
	vkGetMemoryWin32HandleNV = gExternDeviceDispatchTable.GetMemoryWin32HandleNV;
#endif // VK_USE_PLATFORM_WIN32_KHR
#endif

#if defined(VK_EXT_conditional_rendering)
	// ---- VK_EXT_conditional_rendering extension commands
	vkCmdBeginConditionalRenderingEXT = gExternDeviceDispatchTable.CmdBeginConditionalRenderingEXT;
	vkCmdEndConditionalRenderingEXT = gExternDeviceDispatchTable.CmdEndConditionalRenderingEXT;
#endif

#if defined(VK_NVX_device_generated_commands)
	// ---- VK_NVX_device_generated_commands extension commands
	vkCmdProcessCommandsNVX = gExternDeviceDispatchTable.CmdProcessCommandsNVX;
	vkCmdReserveSpaceForCommandsNVX = gExternDeviceDispatchTable.CmdReserveSpaceForCommandsNVX;
	vkCreateIndirectCommandsLayoutNVX = gExternDeviceDispatchTable.CreateIndirectCommandsLayoutNVX;
	vkDestroyIndirectCommandsLayoutNVX = gExternDeviceDispatchTable.DestroyIndirectCommandsLayoutNVX;
	vkCreateObjectTableNVX = gExternDeviceDispatchTable.CreateObjectTableNVX;
	vkDestroyObjectTableNVX = gExternDeviceDispatchTable.DestroyObjectTableNVX;
	vkRegisterObjectsNVX = gExternDeviceDispatchTable.RegisterObjectsNVX;
	vkUnregisterObjectsNVX = gExternDeviceDispatchTable.UnregisterObjectsNVX;
#endif

#if defined(VK_NV_clip_space_w_scaling)
	// ---- VK_NV_clip_space_w_scaling extension commands
	vkCmdSetViewportWScalingNV = gExternDeviceDispatchTable.CmdSetViewportWScalingNV;
#endif


#if defined(VK_EXT_display_control)
	// ---- VK_EXT_display_control extension commands
	vkDisplayPowerControlEXT = gExternDeviceDispatchTable.DisplayPowerControlEXT;
	vkRegisterDeviceEventEXT = gExternDeviceDispatchTable.RegisterDeviceEventEXT;
	vkRegisterDisplayEventEXT = gExternDeviceDispatchTable.RegisterDisplayEventEXT;
	vkGetSwapchainCounterEXT = gExternDeviceDispatchTable.GetSwapchainCounterEXT;
#endif

#if defined(VK_GOOGLE_display_timing)
	// ---- VK_GOOGLE_display_timing extension commands
	vkGetRefreshCycleDurationGOOGLE = gExternDeviceDispatchTable.GetRefreshCycleDurationGOOGLE;
	vkGetPastPresentationTimingGOOGLE = gExternDeviceDispatchTable.GetPastPresentationTimingGOOGLE;
#endif

#if defined(VK_EXT_discard_rectangles)
	// ---- VK_EXT_discard_rectangles extension commands
	vkCmdSetDiscardRectangleEXT = gExternDeviceDispatchTable.CmdSetDiscardRectangleEXT;
#endif

#if defined(VK_EXT_hdr_metadata)
	// ---- VK_EXT_hdr_metadata extension commands
	vkSetHdrMetadataEXT = gExternDeviceDispatchTable.SetHdrMetadataEXT;
#endif

#if defined(VK_EXT_debug_utils)
	// ---- VK_EXT_debug_utils extension commands
	vkSetDebugUtilsObjectNameEXT = gExternDeviceDispatchTable.SetDebugUtilsObjectNameEXT;
	vkSetDebugUtilsObjectTagEXT = gExternDeviceDispatchTable.SetDebugUtilsObjectTagEXT;
	vkQueueBeginDebugUtilsLabelEXT = gExternDeviceDispatchTable.QueueBeginDebugUtilsLabelEXT;
	vkQueueEndDebugUtilsLabelEXT = gExternDeviceDispatchTable.QueueEndDebugUtilsLabelEXT;
	vkQueueInsertDebugUtilsLabelEXT = gExternDeviceDispatchTable.QueueInsertDebugUtilsLabelEXT;
	vkCmdBeginDebugUtilsLabelEXT = gExternDeviceDispatchTable.CmdBeginDebugUtilsLabelEXT;
	vkCmdEndDebugUtilsLabelEXT = gExternDeviceDispatchTable.CmdEndDebugUtilsLabelEXT;
	vkCmdInsertDebugUtilsLabelEXT = gExternDeviceDispatchTable.CmdInsertDebugUtilsLabelEXT;
#endif

#if defined(VK_ANDROID_external_memory_android_hardware_buffer)
	// ---- VK_ANDROID_external_memory_android_hardware_buffer extension commands
#ifdef VK_USE_PLATFORM_ANDROID_KHR
	vkGetAndroidHardwareBufferPropertiesANDROID = gExternDeviceDispatchTable.GetAndroidHardwareBufferPropertiesANDROID;
#endif // VK_USE_PLATFORM_ANDROID_KHR
#ifdef VK_USE_PLATFORM_ANDROID_KHR
	vkGetMemoryAndroidHardwareBufferANDROID = gExternDeviceDispatchTable.GetMemoryAndroidHardwareBufferANDROID;
#endif // VK_USE_PLATFORM_ANDROID_KHR
#endif

#if defined(VK_EXT_sample_locations)
	// ---- VK_EXT_sample_locations extension commands
	vkCmdSetSampleLocationsEXT = gExternDeviceDispatchTable.CmdSetSampleLocationsEXT;
#endif

#if defined(VK_EXT_image_drm_format_modifier)
	// ---- VK_EXT_image_drm_format_modifier extension commands
	vkGetImageDrmFormatModifierPropertiesEXT = gExternDeviceDispatchTable.GetImageDrmFormatModifierPropertiesEXT;
#endif

#if defined(VK_EXT_validation_cache)
	// ---- VK_EXT_validation_cache extension commands
	vkCreateValidationCacheEXT = gExternDeviceDispatchTable.CreateValidationCacheEXT;
	vkDestroyValidationCacheEXT = gExternDeviceDispatchTable.DestroyValidationCacheEXT;
	vkMergeValidationCachesEXT = gExternDeviceDispatchTable.MergeValidationCachesEXT;
	vkGetValidationCacheDataEXT = gExternDeviceDispatchTable.GetValidationCacheDataEXT;
#endif

#if defined(VK_NV_shading_rate_image)
	// ---- VK_NV_shading_rate_image extension commands
	vkCmdBindShadingRateImageNV = gExternDeviceDispatchTable.CmdBindShadingRateImageNV;
	vkCmdSetViewportShadingRatePaletteNV = gExternDeviceDispatchTable.CmdSetViewportShadingRatePaletteNV;
	vkCmdSetCoarseSampleOrderNV = gExternDeviceDispatchTable.CmdSetCoarseSampleOrderNV;
#endif

#if defined(VK_NV_ray_tracing)
	// ---- VK_NV_ray_tracing extension commands
	vkCreateAccelerationStructureNV = gExternDeviceDispatchTable.CreateAccelerationStructureNV;
	vkDestroyAccelerationStructureNV = gExternDeviceDispatchTable.DestroyAccelerationStructureNV;
	vkGetAccelerationStructureMemoryRequirementsNV = gExternDeviceDispatchTable.GetAccelerationStructureMemoryRequirementsNV;
	vkBindAccelerationStructureMemoryNV = gExternDeviceDispatchTable.BindAccelerationStructureMemoryNV;
	vkCmdBuildAccelerationStructureNV = gExternDeviceDispatchTable.CmdBuildAccelerationStructureNV;
	vkCmdCopyAccelerationStructureNV = gExternDeviceDispatchTable.CmdCopyAccelerationStructureNV;
	vkCmdTraceRaysNV = gExternDeviceDispatchTable.CmdTraceRaysNV;
	vkCreateRayTracingPipelinesNV = gExternDeviceDispatchTable.CreateRayTracingPipelinesNV;
	vkGetRayTracingShaderGroupHandlesNV = gExternDeviceDispatchTable.GetRayTracingShaderGroupHandlesNV;
	vkGetAccelerationStructureHandleNV = gExternDeviceDispatchTable.GetAccelerationStructureHandleNV;
	vkCmdWriteAccelerationStructuresPropertiesNV = gExternDeviceDispatchTable.CmdWriteAccelerationStructuresPropertiesNV;
	vkCompileDeferredNV = gExternDeviceDispatchTable.CompileDeferredNV;
#endif

#if defined(VK_EXT_external_memory_host)
	// ---- VK_EXT_external_memory_host extension commands
	vkGetMemoryHostPointerPropertiesEXT = gExternDeviceDispatchTable.GetMemoryHostPointerPropertiesEXT;
#endif

#if defined(VK_AMD_buffer_marker)
	// ---- VK_AMD_buffer_marker extension commands
	vkCmdWriteBufferMarkerAMD = gExternDeviceDispatchTable.CmdWriteBufferMarkerAMD;
#endif

#if defined(VK_EXT_calibrated_timestamps)
	// ---- VK_EXT_calibrated_timestamps extension commands
	vkGetCalibratedTimestampsEXT = gExternDeviceDispatchTable.GetCalibratedTimestampsEXT;
#endif

#if defined(VK_NV_mesh_shader)
	// ---- VK_NV_mesh_shader extension commands
	vkCmdDrawMeshTasksNV = gExternDeviceDispatchTable.CmdDrawMeshTasksNV;
	vkCmdDrawMeshTasksIndirectNV = gExternDeviceDispatchTable.CmdDrawMeshTasksIndirectNV;
	vkCmdDrawMeshTasksIndirectCountNV = gExternDeviceDispatchTable.CmdDrawMeshTasksIndirectCountNV;
#endif

#if defined(VK_NV_scissor_exclusive)
	// ---- VK_NV_scissor_exclusive extension commands
	vkCmdSetExclusiveScissorNV = gExternDeviceDispatchTable.CmdSetExclusiveScissorNV;
#endif

#if defined(VK_NV_device_diagnostic_checkpoints)
	// ---- VK_NV_device_diagnostic_checkpoints extension commands
	vkCmdSetCheckpointNV = gExternDeviceDispatchTable.CmdSetCheckpointNV;
	vkGetQueueCheckpointDataNV = gExternDeviceDispatchTable.GetQueueCheckpointDataNV;
#endif

#if defined(VK_EXT_buffer_device_address)
	// ---- VK_EXT_buffer_device_address extension commands
	vkGetBufferDeviceAddressEXT = gExternDeviceDispatchTable.GetBufferDeviceAddressEXT;
#endif

	return VK_SUCCESS;
}

#ifdef __cplusplus
}
#endif

#endif

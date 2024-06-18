/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#pragma once

#ifndef FORGE_RENDERER_CONFIG_H
#error "VulkanConfig should be included from RendererConfig only"
#endif

#define VULKAN

#if defined(_WINDOWS) || defined(XBOX)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__ANDROID__)
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#elif defined(__linux__) && !defined(VK_USE_PLATFORM_GGP)
// TODO: Separate vulkan ext from choosing xlib vs xcb
#define VK_USE_PLATFORM_XLIB_KHR // Use Xlib or Xcb as display server, defaults to Xlib
#define VK_USE_PLATFORM_WAYLAND_KHR
#elif defined(NX64)
#define VK_USE_PLATFORM_VI_NN
#else
#endif

#define VOLK_VULKAN_H_PATH "../VulkanSDK/include/vulkan/vulkan.h"
#include "../ThirdParty/OpenSource/volk/volk.h"

// #define USE_EXTERNAL_MEMORY_EXTENSIONS
#ifdef USE_EXTERNAL_MEMORY_EXTENSIONS
#define VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME    "Memory ext name"
#define VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME "Semaphore ext name"
#define VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME     "Fence ext name"
#endif

#define TARGET_VULKAN_API_VERSION VK_API_VERSION_1_1

/************************************************************************/
// Debugging Macros
/************************************************************************/
//////////////////////////////////////////////
//// Availability macros
//////////////////////////////////////////////

#if defined(VK_KHR_ray_tracing_pipeline) && defined(VK_KHR_acceleration_structure)
#define VK_RAYTRACING_AVAILABLE
#endif

#if defined(_WINDOWS) || defined(__linux__)
#define NSIGHT_AFTERMATH_AVAILABLE
#endif

#define SHADER_STATS_AVAILABLE

#ifndef VK_OVERRIDE_LAYER_PATH
#define VK_OVERRIDE_LAYER_PATH 1
#endif

extern void OnVkDeviceLost(struct Renderer*);

// Validate VkResult for functions that take a VkDevice or object created using VkDevice
// Needs Renderer* pRenderer to be declared in scope
#define CHECK_VKRESULT(exp)                                            \
    {                                                                  \
        VkResult vkres = (exp);                                        \
        if (vkres == VK_ERROR_DEVICE_LOST)                             \
        {                                                              \
            OnVkDeviceLost(pRenderer);                                 \
        }                                                              \
        if (VK_SUCCESS != vkres)                                       \
        {                                                              \
            LOGF(eERROR, "%s: FAILED with VkResult: %d", #exp, vkres); \
            ASSERT(false);                                             \
        }                                                              \
    }

// Validate VkResult for functions that take a VkInstance or object created using VkInstance
#define CHECK_VKRESULT_INSTANCE(exp)                                   \
    {                                                                  \
        VkResult vkres = (exp);                                        \
        if (VK_SUCCESS != vkres)                                       \
        {                                                              \
            LOGF(eERROR, "%s: FAILED with VkResult: %d", #exp, vkres); \
            ASSERT(false);                                             \
        }                                                              \
    }

#if defined(ANDROID) && !defined(QUEST_VR)
#define GFX_ENABLE_SWAPPY
#endif

// #NOTE: Keep the door open to disable the extension on buggy drivers as it is still new
extern bool gEnableDynamicRenderingExtension;

// Amount of memory tracked objects
#define TRACKED_OBJECT_TYPE_COUNT_MAX 40

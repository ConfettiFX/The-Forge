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

#ifndef FORGE_RENDERER_CONFIG_H
#define FORGE_RENDERER_CONFIG_H

//Support external config file override
#if defined(EXTERNAL_RENDERER_CONFIG_FILEPATH)
    #include EXTERNAL_RENDERER_CONFIG_FILEPATH
#else

#include "../Application/Config.h"


// Comment/uncomment includes to disable/enable rendering APIs
#if defined(_WINDOWS)
#ifndef _WINDOWS7
#include "Direct3D12/Direct3D12Config.h"
#endif
#include "Direct3D11/Direct3D11Config.h"
#include "Vulkan/VulkanConfig.h"
#elif defined(XBOX)
#include "Direct3D12/Direct3D12Config.h"
#elif defined(__APPLE__)
#include "Metal/MetalConfig.h"
#elif defined(__ANDROID__)
#ifndef QUEST_VR
#include "OpenGLES/GLESConfig.h"
#endif
#ifdef ARCH_ARM64
#include "Vulkan/VulkanConfig.h"
#endif
#elif defined(NX64)
#include "Vulkan/VulkanConfig.h"
#elif defined(__linux__)
#include "Vulkan/VulkanConfig.h"
#endif

// Uncomment this macro to define custom rendering max options
// #define RENDERER_CUSTOM_MAX
#ifdef RENDERER_CUSTOM_MAX
enum
{
	MAX_INSTANCE_EXTENSIONS = 64,
	MAX_DEVICE_EXTENSIONS = 64,
	/// Max number of GPUs in SLI or Cross-Fire
	MAX_LINKED_GPUS = 4,
	MAX_RENDER_TARGET_ATTACHMENTS = 8,
	MAX_VERTEX_BINDINGS = 15,
	MAX_VERTEX_ATTRIBS = 15,
	MAX_SEMANTIC_NAME_LENGTH = 128,
	MAX_DEBUG_NAME_LENGTH = 128,
	MAX_MIP_LEVELS = 0xFFFFFFFF,
	MAX_SWAPCHAIN_IMAGES = 3,
	MAX_GPU_VENDOR_STRING_LENGTH = 64,    //max size for GPUVendorPreset strings
#if defined(VULKAN)
	MAX_PLANE_COUNT = 3,
#endif
};
#endif


// Enable raytracing if available
// Possible renderers: D3D12, Vulkan, Metal
#if defined(D3D12_RAYTRACING_AVAILABLE) || defined(VK_RAYTRACING_AVAILABLE) || defined(MTL_RAYTRACING_AVAILABLE)
#define ENABLE_RAYTRACING
#endif

// Enable variable rate shading if available
// Possible renderers: D3D12
#ifdef VRS_AVAILABLE
#define ENABLE_VRS
#endif

#ifdef ENABLE_PROFILER
#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11) || defined(METAL) || defined(ORBIS) || defined(PROSPERO) || defined(GLES)
#define ENABLE_GPU_PROFILER
#endif
#endif

// Enable graphics debug if general debug is turned on
#ifdef FORGE_DEBUG
#define ENABLE_GRAPHICS_DEBUG
#endif

#ifdef NSIGHT_AFTERMATH_AVAILABLE
#ifdef FORGE_DEBUG
//#define ENABLE_NSIGHT_AFTERMATH
#endif
#endif

#if (defined(DIRECT3D12) + defined(DIRECT3D11) + defined(VULKAN) + defined(GLES) + defined(METAL) + defined(ORBIS) + defined(PROSPERO) + defined(NX64)) == 0
#error "No rendering API defined"
#elif (defined(DIRECT3D12) + defined(DIRECT3D11) + defined(VULKAN) + defined(GLES) + defined(METAL) + defined(ORBIS) + defined(PROSPERO) + defined(NX64)) > 1
#define USE_MULTIPLE_RENDER_APIS
#endif

#if defined(ANDROID) || defined(SWITCH) || defined(TARGET_APPLE_ARM64)
#define USE_MSAA_RESOLVE_ATTACHMENTS
#endif

#ifdef FORGE_DEBUG
#define ENABLE_DEPENDENCY_TRACKER
#endif

#endif
#endif

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
#define FORGE_RENDERER_CONFIG_H

// Support external config file override
#if defined(EXTERNAL_RENDERER_CONFIG_FILEPATH)
#include EXTERNAL_RENDERER_CONFIG_FILEPATH
#elif defined(EXTERNAL_RENDERER_CONFIG_FILEPATH_NO_STRING)
// When invoking clanng from FastBuild the EXTERNAL_CONFIG_FILEPATH define doesn't get expanded to a string,
// quotes are removed, that's why we add this variation of the macro that turns the define back into a valid string
#define TF_EXTERNAL_CONFIG_STRINGIFY2(x) #x
#define TF_EXTERNAL_CONFIG_STRINGIFY(x)  TF_EXTERNAL_CONFIG_STRINGIFY2(x)

#include TF_EXTERNAL_CONFIG_STRINGIFY(EXTERNAL_RENDERER_CONFIG_FILEPATH_NO_STRING)

#undef TF_EXTERNAL_CONFIG_STRINGIFY
#undef TF_EXTERNAL_CONFIG_STRINGIFY2
#else

#include "../Application/Config.h"

// ------------------------------- renderer configuration ------------------------------- //

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
    MAX_GPU_VENDOR_STRING_LENGTH = 64, // max size for GPUVendorPreset strings
#if defined(VULKAN)
    MAX_PLANE_COUNT = 3,
#endif
};
#endif

// Enable raytracing if available
// Possible renderers: D3D12, Vulkan, Metal
#if defined(D3D12_RAYTRACING_AVAILABLE) || defined(VK_RAYTRACING_AVAILABLE) || defined(MTL_RAYTRACING_AVAILABLE) || defined(PROSPERO)
#define ENABLE_RAYTRACING
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
// #define ENABLE_NSIGHT_AFTERMATH
#endif
#endif

#if (defined(DIRECT3D12) + defined(DIRECT3D11) + defined(VULKAN) + defined(GLES) + defined(METAL) + defined(ORBIS) + defined(PROSPERO) + \
     defined(NX64)) == 0
#error "No rendering API defined"
#elif (defined(DIRECT3D12) + defined(DIRECT3D11) + defined(VULKAN) + defined(GLES) + defined(METAL) + defined(ORBIS) + defined(PROSPERO) + \
       defined(NX64)) > 1
#define USE_MULTIPLE_RENDER_APIS
#endif

#if defined(ANDROID) || defined(SWITCH) || defined(TARGET_APPLE_ARM64)
#define USE_MSAA_RESOLVE_ATTACHMENTS
#endif

#ifdef FORGE_DEBUG
#define ENABLE_DEPENDENCY_TRACKER
#endif

#if defined(FORGE_DEBUG) && defined(VULKAN)
#define GFX_DRIVER_MEMORY_TRACKING
#define GFX_DEVICE_MEMORY_TRACKING
#endif

#if defined(_WIN32) && !defined(XBOX)
#define FORGE_D3D11_DYNAMIC_LOADING
#define FORGE_D3D12_DYNAMIC_LOADING
#endif

#endif
#endif

// ------------------------------- gpu configuration rules ------------------------------- //

struct GPUSettings;
struct GPUCapBits;

typedef struct ExtendedSettings
{
    uint32_t     mNumSettings;
    uint32_t*    pSettings;
    const char** ppSettingNames;
} ExtendedSettings;

typedef enum GPUPresetLevel
{
    GPU_PRESET_NONE = 0,
    GPU_PRESET_OFFICE,  // This means unsupported
    GPU_PRESET_VERYLOW, // Mostly for mobile GPU
    GPU_PRESET_LOW,
    GPU_PRESET_MEDIUM,
    GPU_PRESET_HIGH,
    GPU_PRESET_ULTRA,
    GPU_PRESET_COUNT
} GPUPresetLevel;

// read gpu.cfg and store all its content in specific structures
FORGE_API void addGPUConfigurationRules(ExtendedSettings* pExtendedSettings);

// free all specific gpu.cfg structures
FORGE_API void removeGPUConfigurationRules();

// set default value, samplerAnisotropySupported, graphicsQueueSupported, primitiveID
FORGE_API void setDefaultGPUSettings(struct GPUSettings* pGpuSettings);

// selects best gpu depending on the gpu comparison rules stored in gpu.cfg
FORGE_API uint32_t util_select_best_gpu(struct GPUSettings* availableSettings, uint32_t gpuCount);

// reads the gpu data and sets the preset level of all available gpu's
FORGE_API GPUPresetLevel getDefaultPresetLevel();
FORGE_API GPUPresetLevel getGPUPresetLevel(uint32_t vendorId, uint32_t modelId, const char* vendorName, const char* modelName);

// apply the configuration rules stored in gpu.cfg to to a single GPUSettings
FORGE_API void applyGPUConfigurationRules(struct GPUSettings* pGpuSettings, struct GPUCapBits* pCapBits);

// apply the user extended configuration rules stored in gpu.cfg to the ExtendedSetting structure
FORGE_API void setupExtendedSettings(ExtendedSettings* pExtendedSettings, const struct GPUSettings* pGpuSettings);

// return if the the GPUSettings validate the current driver rejection rules
FORGE_API bool checkDriverRejectionSettings(const struct GPUSettings* pGpuSettings);

// ------ utilities ------
FORGE_API const char*    presetLevelToString(GPUPresetLevel preset);
FORGE_API GPUPresetLevel stringToPresetLevel(const char* presetLevel);
FORGE_API bool           gpuVendorEquals(uint32_t vendorId, const char* vendorName);
FORGE_API const char*    getGPUVendorName(uint32_t modelId);
FORGE_API uint32_t       getGPUVendorID(const char*);

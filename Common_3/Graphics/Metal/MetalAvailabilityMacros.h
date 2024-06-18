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

#ifdef TARGET_IOS

// iOS SDK 14.1 have GPU families up to MTLGPUFamilyApple7
// but MacOS SDK 15 have GPU families only up to MTLGPUFamilyApple5.

#define ENABLE_GPU_FAMILY_6
#define ENABLE_GPU_FAMILY_7

// iOS

#if __IPHONE_OS_VERSION_MAX_ALLOWED < 141000
#error "Please only compile with ios SDK version 14.1 or higher"
#endif

#ifdef __IPHONE_17_0
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_17_0
#define ENABLE_REFLECTION_BINDING_API
#define ARGUMENT_ACCESS_DEPRECATED
#define ENABLE_GPU_FAMILY_9
#define HIGHEST_GPU_FAMILY MTLGPUFamilyApple9
#endif // __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_17_0
#endif // __IPHONE_17_0

#ifdef __IPHONE_16_0
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_0
#define ENABLE_ACCELERATION_STRUCTURE_VERTEX_FORMAT
#endif // __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_0
#endif // __IPHONE_16_0

#ifdef __IPHONE_15_0
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_15_0
#define ENABLE_GPU_FAMILY_8
#ifndef HIGHEST_GPU_FAMILY
#define HIGHEST_GPU_FAMILY MTLGPUFamilyApple8
#endif // HIGHEST_GPU_FAMILY
#define ENABLE_PREFERRED_FRAME_RATE_RANGE
#define ENABLE_GFX_CMD_SET_ACCELERATION_STRUCTURE
#define MTL_RAYTRACING_AVAILABLE
#endif // __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_15_0
#endif // __IPHONE_15_0

#ifndef HIGHEST_GPU_FAMILY
#define HIGHEST_GPU_FAMILY MTLGPUFamilyApple7
#endif // HIGHEST_GPU_FAMILY

#ifdef __IPHONE_14_5
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_5
#define ENABLE_32_BIT_FLOAT_FILTERING
#endif // __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
#endif // __IPHONE_14_5

#define ENABLE_OS_PROC_MEMORY
#define ENABLE_MEMORYLESS_TEXTURES

#ifdef TARGET_IOS_SIMULATOR
#define ENABLE_DRAW_INDEX_BASE_VERTEX_FALLBACK
#endif // TARGET_IOS_SIMULATOR

#define MTL_HDR_SUPPORTED IOS17_RUNTIME

#else

// macOS

#if MAC_OS_X_VERSION_MAX_ALLOWED < 110000
#error("Please only compile with macOS SDK version 11 or higher")
#endif

#ifdef MAC_OS_VERSION_14_0
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_VERSION_14_0
#define ENABLE_REFLECTION_BINDING_API
#define ARGUMENT_ACCESS_DEPRECATED
#define ENABLE_GPU_FAMILY_9
#define HIGHEST_GPU_FAMILY MTLGPUFamilyApple9
#endif // MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_14_0
#endif // MAC_OS_X_VERSION_14_0

#ifdef MAC_OS_VERSION_13_0
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_VERSION_13_0
#define ENABLE_ACCELERATION_STRUCTURE_VERTEX_FORMAT
#define ENABLE_GPU_FAMILY_8
#ifndef HIGHEST_GPU_FAMILY
#define HIGHEST_GPU_FAMILY MTLGPUFamilyApple8
#endif // HIGHEST_GPU_FAMILY
#endif // MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_13_0
#endif // MAC_OS_X_VERSION_13_0

#ifdef MAC_OS_VERSION_12_0
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_VERSION_12_0
#define ENABLE_GFX_CMD_SET_ACCELERATION_STRUCTURE
#define MTL_RAYTRACING_AVAILABLE
#endif // MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_12_0
#endif // MAC_OS_X_VERSION_12_0

#ifdef MAC_OS_VERSION_11_0
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_VERSION_11_0
#define ENABLE_GPU_FAMILY_6
#define ENABLE_GPU_FAMILY_7
#ifndef HIGHEST_GPU_FAMILY
#define HIGHEST_GPU_FAMILY MTLGPUFamilyApple7
#endif // HIGHEST_GPU_FAMILY
#define ENABLE_32_BIT_FLOAT_FILTERING
#endif // MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_11_0
#endif // MAC_OS_X_VERSION_11_0

#ifndef HIGHEST_GPU_FAMILY
#define HIGHEST_GPU_FAMILY MTLGPUFamilyApple5
#endif // HIGHEST_GPU_FAMILY

#if !defined(TARGET_APPLE_ARM64)
#define ENABLE_MEMORY_BARRIERS_GRAPHICS
#endif
#define ENABLE_DISPLAY_SYNC_TOGGLE
#define ENABLE_SAMPLER_CLAMP_TO_BORDER

#define MTL_HDR_SUPPORTED IOS14_RUNTIME

#endif // TARGET_IOS

#define MTL_RAYTRACING_SUPPORTED IOS17_RUNTIME

#ifdef ARGUMENT_ACCESS_DEPRECATED
#define MTL_ACCESS_TYPE    MTLBindingAccess
#define MTL_ACCESS_ENUM(X) MTLBindingAccess##X
#else
#define MTL_ACCESS_TYPE    MTLArgumentAccess
#define MTL_ACCESS_ENUM(X) MTLArgumentAccess##X
#endif

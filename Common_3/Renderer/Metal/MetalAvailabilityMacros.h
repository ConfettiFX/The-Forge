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

#pragma once

#ifdef TARGET_IOS

// iOS
#ifdef __IPHONE_13_0
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0
#ifndef TARGET_IOS_SIMULATOR
#define ENABLE_HEAP_PLACEMENT
#define MTL_RAYTRACING_AVAILABLE
#define ENABLE_INDIRECT_COMMAND_BUFFER_INHERIT_PIPELINE
#endif // TARGET_IOS_SIMULATOR
#define ENABLE_HEAP_RESOURCE_OPTIONS
#define ENABLE_ARGUMENT_BUFFER_USE_STAGES
#define ENABLE_OS_PROC_MEMORY
#define ENABLE_GPU_FAMILY
#endif // __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0
#endif // __IPHONE_13_0

#ifdef __IPHONE_12_0
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_12_0
#ifndef TARGET_IOS_SIMULATOR
#define ENABLE_EVENT_SEMAPHORE
#define ENABLE_MEMORY_BARRIERS_COMPUTE
#define ENABLE_INDIRECT_COMMAND_BUFFERS
#endif // TARGET_IOS_SIMULATOR
#endif // __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_12_0
#endif // __IPHONE_12_0

#ifdef __IPHONE_11_0
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0
#ifndef TARGET_IOS_SIMULATOR
#define ENABLE_GPU_FAMILY_4
#define ENABLE_DEPTH_CLIP_MODE
#endif // TARGET_IOS_SIMULATOR
#define ENABLE_ARGUMENT_BUFFERS
#define ENABLE_COMMAND_BUFFER_DEBUG_MARKERS
#define ENABLE_TEXTURE_CUBE_ARRAYS
#define ENABLE_ROVS
#define ENABLE_TEXTURE_READ_WRITE
#endif // __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0
#endif // __IPHONE_11_0

#ifdef __IPHONE_10_3
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_3
#define ENABLE_GPU_TIMESTAMPS
#endif // __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_3
#endif // __IPHONE_10_3

#ifdef __IPHONE_10_0
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_0
#ifndef TARGET_IOS_SIMULATOR
#define ENABLE_HEAPS
#define ENABLE_FENCES
#define ENABLE_MEMORYLESS_TEXTURES
#define ENABLE_TESSELLATION
#endif // TARGET_IOS_SIMULATOR
#endif // __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_0
#endif // __IPHONE_10_0

#ifdef __IPHONE_9_0
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0
#ifndef TARGET_IOS_SIMULATOR
#define ENABLE_GPU_FAMILY_3
#endif // TARGET_IOS_SIMULATOR
#endif // __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0
#endif // __IPHONE_9_0

// Supported on all iOS
#define ENABLE_GPU_FAMILY_2
#define ENABLE_GPU_FAMILY_1

#ifdef TARGET_IOS_SIMULATOR
#define ENABLE_DRAW_INDEX_BASE_VERTEX_FALLBACK
#endif // TARGET_IOS_SIMULATOR

#else

// macOS
#ifdef MAC_OS_X_VERSION_10_15
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_15
#define ENABLE_HEAP_PLACEMENT
#define ENABLE_HEAP_RESOURCE_OPTIONS
#if !defined(TARGET_APPLE_ARM64)
#define ENABLE_ARGUMENT_BUFFER_USE_STAGES
#endif
#define ENABLE_GPU_TIMESTAMPS
#define MTL_RAYTRACING_AVAILABLE
#define ENABLE_GPU_FAMILY
#endif // MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_15
#endif // MAC_OS_X_VERSION_10_15

#ifdef MAC_OS_X_VERSION_10_14
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_14
#define ENABLE_GPU_FAMILY_1_V4
#define ENABLE_INDIRECT_COMMAND_BUFFERS
#define ENABLE_EVENT_SEMAPHORE
#if !defined(TARGET_APPLE_ARM64)
#define ENABLE_MEMORY_BARRIERS_GRAPHICS
#endif
#define ENABLE_MEMORY_BARRIERS_COMPUTE
#define ENABLE_INDIRECT_COMMAND_BUFFER_INHERIT_PIPELINE
#endif // MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_14
#endif // MAC_OS_X_VERSION_10_14

#ifdef MAC_OS_X_VERSION_10_13
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_13
#define ENABLE_GPU_FAMILY_1_V3
#define ENABLE_ARGUMENT_BUFFERS
#define ENABLE_HEAPS
#define ENABLE_FENCES
#define ENABLE_DISPLAY_SYNC_TOGGLE
#define ENABLE_COMMAND_BUFFER_DEBUG_MARKERS
#define ENABLE_ROVS
#define ENABLE_TEXTURE_READ_WRITE
#endif // MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_13
#endif // MAC_OS_X_VERSION_10_13

#ifdef MAC_OS_X_VERSION_10_12
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_12
#define ENABLE_GPU_FAMILY_1_V2
#define ENABLE_SAMPLER_CLAMP_TO_BORDER
#define ENABLE_TESSELLATION
#endif // MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_12
#endif // MAC_OS_X_VERSION_10_12

// Supported for all macOS
#define ENABLE_TEXTURE_CUBE_ARRAYS
#define ENABLE_DEPTH_CLIP_MODE

#endif // TARGET_IOS

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

#pragma once

#if defined(__cplusplus)

//  macros to allow sharing structs between code and shaders
#define STRUCT(T)              struct T
#define DATA(type, name, freq) type name
#define STATIC                 static
#define CBUFFER(NAME)          struct NAME
#define float4x4               mat4
#define uint                   uint32_t

#endif

#if defined(DIRECT3D12)
#include "d3d12_srt.h"
#endif
#if defined(VULKAN)
#include "vulkan_srt.h"
#endif
#if defined(METAL)
#include "metal_srt.h"
#endif
#if defined(ORBIS)
#include "../../../PS4/Common_3/Graphics/FSL/pssl_srt.h"
#endif
#if defined(PROSPERO)
#include "../../../Prospero/Common_3/Graphics/FSL/pssl_srt.h"
#endif

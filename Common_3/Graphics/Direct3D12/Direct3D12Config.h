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

#ifndef FORGE_RENDERER_CONFIG_H
#error "Direct3D12Config should be included from RendererConfig only"
#endif

#define DIRECT3D12

#ifdef XBOX
#include "../../../Xbox/Common_3/Graphics/Direct3D12/Direct3D12X.h"
#else
#include <d3d12.h>
#include "../ThirdParty/OpenSource/DirectXShaderCompiler/inc/dxcapi.h"
#include <dxgi1_6.h>
#include <dxgidebug.h>
#endif


//////////////////////////////////////////////
//// Availability macros
//// Do not modify
//////////////////////////////////////////////

#ifdef D3D12_RAYTRACING_AABB_BYTE_ALIGNMENT
#define D3D12_RAYTRACING_AVAILABLE
#endif

#ifdef D3D12_RS_SET_SHADING_RATE_COMBINER_COUNT
#define VRS_AVAILABLE
#endif

#if defined(_WINDOWS)
#define NSIGHT_AFTERMATH_AVAILABLE
#endif
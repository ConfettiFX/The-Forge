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

#include "../../Application/Config.h"

#include "../../Utilities/Math/MathTypes.h"

typedef struct Texture                    Texture;
typedef struct Cmd                        Cmd;
typedef struct Buffer                     Buffer;
typedef struct Renderer                   Renderer;
typedef struct ParticleConstantBufferData ParticleConstantBufferData;

typedef struct ParticleSystemInitDesc
{
    Renderer* pRenderer = NULL;

    uint32_t mSwapWidth = 0;
    uint32_t mSwapHeight = 0;
    uint32_t mFramesInFlight = 0;

    uint32_t mSwapColorFormat = 0;
    uint32_t mDepthFormat = 0;
    uint32_t mColorSampleQuality = 0;

    Texture*  pColorBuffer = NULL;
    Texture*  pDepthBuffer = NULL;
    Texture** ppParticleTextures = NULL;

    Buffer* pParticlesBuffer = NULL;
    Buffer* pBitfieldBuffer = NULL;
    Buffer* pTransparencyListBuffer = NULL;
    Buffer* pTransparencyListHeadsBuffer = NULL;

    Buffer** ppParticleConstantBuffer = NULL;
} ParticleSystemInitDesc;

FORGE_RENDERER_API bool particleSystemInit(const ParticleSystemInitDesc* pDesc);
FORGE_RENDERER_API void particleSystemExit();

FORGE_RENDERER_API void particleSystemUpdateConstantBuffers(uint32_t frameIndex, ParticleConstantBufferData* cameraConstantBufferData);

FORGE_RENDERER_API void particleSystemCmdBegin(Cmd* pCmd, uint32_t frameIndex);
FORGE_RENDERER_API void particleSystemCmdSimulate(Cmd* pCmd, uint32_t frameIndex);
FORGE_RENDERER_API void particleSystemCmdRender(Cmd* pCmd, uint32_t frameIndex);
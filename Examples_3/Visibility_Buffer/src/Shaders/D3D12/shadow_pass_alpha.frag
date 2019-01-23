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

#include "packing.h"
#include "shader_defs.h"

#ifndef CONF_EARLY_DEPTH_STENCIL
#define CONF_EARLY_DEPTH_STENCIL
#endif

struct PsInAlphaTested
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD0;
};

ConstantBuffer<RootConstant> indirectRootConstant : register(b1);

StructuredBuffer<uint> indirectMaterialBuffer : register(t0);
Texture2D diffuseMaps[] : register(t1);
SamplerState textureFilter : register(s0);

#define __XBOX_FORCE_PS_ZORDER_EARLY_Z_THEN_RE_Z

CONF_EARLY_DEPTH_STENCIL
void main(PsInAlphaTested In)
{
    uint matBaseSlot = BaseMaterialBuffer(true, 0); //1 is camera view, 0 is shadow map view
    uint materialID = indirectMaterialBuffer[matBaseSlot + indirectRootConstant.drawId]; //In.meshID; //IndirectMaterialBuffer[In.drawID];
    float4 texColor = diffuseMaps[NonUniformResourceIndex(materialID)].SampleLevel(textureFilter, In.texCoord, 0);
    clip(texColor.a < 0.5 ? -1 : 1);
}

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

#include "Shader_Defs.h"
#include "Packing.h"


struct PsIn
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

ConstantBuffer<RootConstant> indirectRootConstant : register(b2);

ByteAddressBuffer indirectMaterialBuffer : register(t0, UPDATE_FREQ_PER_FRAME);
Texture2D diffuseMaps[] : register(t1);

SamplerState nearClampSampler : register(s0);



void  main(PsIn In)
{
	uint matBaseSlot = BaseMaterialBuffer(true, VIEW_SHADOW); //1 is camera view, 0 is shadow map view
    uint materialID = indirectMaterialBuffer.Load((matBaseSlot + indirectRootConstant.drawId) << 2);
    float4 texColor = diffuseMaps[NonUniformResourceIndex(materialID)].SampleLevel(nearClampSampler, In.TexCoord, 0);
	clip(texColor.a < 0.5f ? -1 : 1);
}

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


cbuffer uniformBlock : register(b0)
{
	float4x4 mvp;
};

cbuffer particleRootConstant : register(b1)
{
    float paletteFactor;
    uint data;
    uint textureIndex;
};

struct VSOutput {
	float4 Position : SV_POSITION;
    float TexCoord : TEXCOORD;
};

SamplerState uSampler0 : register(s3);
Texture1D uTex0[5] : register(t11);

float4 main(VSOutput input) : SV_TARGET
{
	float4 ca = uTex0[textureIndex].Sample(uSampler0, input.TexCoord);
    float4 cb = uTex0[(textureIndex + 1) % 5].Sample(uSampler0, input.TexCoord);

    return 0.05*lerp(ca, cb, paletteFactor);
}
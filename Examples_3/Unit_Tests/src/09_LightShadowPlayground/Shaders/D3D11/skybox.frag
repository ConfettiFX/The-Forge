/*
* Copyright (c) 2018 Confetti Interactive Inc.
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

struct VSOutput {
    float4 Position : SV_POSITION;
};

cbuffer cameraUniformBlock
{
    row_major float4x4 View : packoffset(c0);
    row_major float4x4 Project : packoffset(c4);
    row_major float4x4 ViewProject : packoffset(c8);
    row_major float4x4 ViewInverse : packoffset(c12);
    row_major float4x4 ProjectInverse : packoffset(c16);
};

cbuffer renderSettingUniformBlock
{
    float4 WindowDimension : packoffset(c0);
    int ShadowType : packoffset(c1);
};

TextureCube<float4> Skybox;
SamplerState skySampler;

[earlydepthstencil]
float4 main(VSOutput input) : SV_TARGET
{
    float3 uvw = mul(normalize(mul(float4(((float2(input.Position.xy) * float2(2.0f, -2.0f)) / WindowDimension.xy) + float2(-1.0f, 1.0f), 1.0f, 1.0f), ProjectInverse).xyz), transpose(float3x3(View[0].xyz, View[1].xyz, View[2].xyz)));
    return Skybox.Sample(skySampler, uvw);
}

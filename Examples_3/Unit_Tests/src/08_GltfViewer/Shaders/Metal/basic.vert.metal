	/*
 * Copyright (c) 2018 Kostas Anagnostou (https://twitter.com/KostasAAA).
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

#include <metal_stdlib>
#include <metal_atomic>
using namespace metal;

inline float3x3 matrix_ctor(float4x4 m) {
        return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}

struct VsIn
{
    uint4 position          [[attribute(0)]];
    int4  normal            [[attribute(1)]];
    uint2 texCoord          [[attribute(2)]];
    uint4 baseColor         [[attribute(3)]];
    uint2 metallicRoughness [[attribute(4)]];
    uint2 alphaSettings     [[attribute(5)]];
};

struct Uniforms_cbPerPass {
    float4x4 projView;
    float4   camPos;
    float4   lightColor[4];
	float4   lightDirection[3];
    int4     quantizationParams;
};

struct Uniforms_cbPerProp {
    float4x4 world;
    float4x4 InvTranspose;
    int unlit;
    int hasAlbedoMap;
    int hasNormalMap;
    int hasMetallicRoughnessMap;
    int hasAOMap;
    int hasEmissiveMap;
    float4 centerOffset;
    float4 posOffset;
    float2 uvOffset;
    float2 uvScale;
    float posScale;   
    float padding0;   
};

struct Uniforms_ShadowUniformBuffer
{
    float4x4 LightViewProj;
};

struct PsIn
{
    float4 position [[position]];
    float4 baseColor;
    float3 normal;
    float3 pos;
    float2 uv;
    float2 metallicRoughness;
    float2 alphaSettings;
};

struct PerFrame
{
	constant Uniforms_cbPerPass & cbPerPass [[id(0)]];
	constant Uniforms_ShadowUniformBuffer & ShadowUniformBuffer       [[id(1)]];
};

struct PerDraw
{
	constant Uniforms_cbPerProp & cbPerProp [[id(0)]];
};

vertex PsIn stageMain(VsIn     In                             [[stage_in]],
                      constant PerFrame& argBufferPerFrame [[buffer(UPDATE_FREQ_PER_FRAME)]],
                      constant PerDraw& argBufferPerDraw [[buffer(UPDATE_FREQ_PER_DRAW)]])
{
    PsIn Out;
    float unormPositionScale = float(1 << argBufferPerFrame.cbPerPass.quantizationParams[0]) - 1.0f;
    float unormTexScale = float(1 << argBufferPerFrame.cbPerPass.quantizationParams[1]) - 1.0f;
    float snormNormalScale = float(1 << (argBufferPerFrame.cbPerPass.quantizationParams[2] - 1)) - 1.0f;
    float unorm16Scale = float(1 << 16) - 1.0f;
    float unorm8Scale = float(1 << 8) - 1.0f;
    
    float4 inPos = float4((float3(In.position.xyz) / (float3)unormPositionScale) * (float3)argBufferPerDraw.cbPerProp.posScale, 1.0f) + argBufferPerDraw.cbPerProp.posOffset;
    inPos.xyz += argBufferPerDraw.cbPerProp.centerOffset.xyz;
    float3 inNormal = float3(In.normal.xyz) / (float3)snormNormalScale;
    
    float4 worldPosition = argBufferPerDraw.cbPerProp.world * float4(inPos.xyz, 1.0f);
    worldPosition.xyz /= (float3)argBufferPerDraw.cbPerProp.posScale;

    Out.position = argBufferPerFrame.cbPerPass.projView * worldPosition;
    Out.normal = normalize((argBufferPerDraw.cbPerProp.InvTranspose * float4(inNormal, 0.0f)).rgb);
    Out.pos = worldPosition.xyz;
    
    Out.uv = float2(In.texCoord.xy) / unormTexScale * argBufferPerDraw.cbPerProp.uvScale + argBufferPerDraw.cbPerProp.uvOffset;
    
    Out.baseColor = float4(In.baseColor.rgba) / unorm8Scale;
    
    Out.metallicRoughness = float2(In.metallicRoughness) / unorm16Scale;
    Out.alphaSettings = float2(In.alphaSettings);
    Out.alphaSettings[0] = Out.alphaSettings[0] + 0.5f;
    Out.alphaSettings[1] = Out.alphaSettings[1] / unorm16Scale;
    
    return Out;
}

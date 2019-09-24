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
using namespace metal;

struct Vertex_Shader
{
    struct Uniforms_ShadowUniformBuffer
    {
        float4x4 ViewProjMat;
    };
    constant Uniforms_ShadowUniformBuffer & ShadowUniformBuffer;
    struct Uniforms_cbPerProp
    {
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
    constant Uniforms_cbPerProp & cbPerProp;
    struct VsIn
    {
        uint4 position          [[attribute(0)]];
        int4  normal            [[attribute(1)]];
        uint2 texCoord          [[attribute(2)]];
        uint4 baseColor         [[attribute(3)]];
        uint2 metallicRoughness [[attribute(4)]];
        uint2 alphaSettings     [[attribute(5)]];
    };
    struct PsIn
    {
        float4 Position [[position]];
    };
    PsIn main(VsIn input)
    {
        PsIn output;
        float unormPositionScale = (float((float)(1 << 16)) - 1.0);
        float4 inPos = (float4(((float3((float3)((input).position).xyz) / (float3)(unormPositionScale)) * (float3)(cbPerProp.posScale)), 1.0) + cbPerProp.posOffset);
		inPos.xyz += cbPerProp.centerOffset.xyz;
        float4 worldPosition = (cbPerProp.world)*(inPos);
        ((worldPosition).xyz /= (float3)(cbPerProp.posScale));
        ((output).Position = ((ShadowUniformBuffer.ViewProjMat)*(worldPosition)));
        return output;
    };

    Vertex_Shader(
constant Uniforms_ShadowUniformBuffer & ShadowUniformBuffer,constant Uniforms_cbPerProp & cbPerProp) :
ShadowUniformBuffer(ShadowUniformBuffer),cbPerProp(cbPerProp) {}
};


struct PerFrame
{
	constant Vertex_Shader::Uniforms_ShadowUniformBuffer& ShadowUniformBuffer [[id(0)]];
};

struct PerDraw
{
	constant Vertex_Shader::Uniforms_cbPerProp& cbPerProp [[id(0)]];
};

vertex Vertex_Shader::PsIn stageMain(
    Vertex_Shader::VsIn input [[stage_in]],
    constant PerFrame& argBufferPerFrame [[buffer(UPDATE_FREQ_PER_FRAME)]],
    constant PerDraw& argBufferPerDraw [[buffer(UPDATE_FREQ_PER_DRAW)]])
{
    Vertex_Shader::VsIn input0;
    input0.position = input.position;
    input0.normal = input.normal;
    input0.texCoord = input.texCoord;
    input0.baseColor = input.baseColor;
    input0.metallicRoughness = input.metallicRoughness;
    input0.alphaSettings = input.alphaSettings;
    Vertex_Shader main(
    argBufferPerFrame.ShadowUniformBuffer,
    argBufferPerDraw.cbPerProp);
    return main.main(input0);
}

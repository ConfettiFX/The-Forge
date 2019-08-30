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
    struct VSInput
    {
        float3 Position [[attribute(0)]];
        float2 TexCoord [[attribute(1)]];
    };
    struct PsIn
    {
        float4 Position [[position]];
    };
    PsIn main(VSInput input)
    {
        PsIn output;
        float4 inPos = float4(float3(((input).Position).xyz), 1.0);
        ((output).Position = ((ShadowUniformBuffer.ViewProjMat)*(((cbPerProp.world)*(inPos)))));
        return output;
    };

    Vertex_Shader(
constant Uniforms_ShadowUniformBuffer & ShadowUniformBuffer,constant Uniforms_cbPerProp & cbPerProp) :
ShadowUniformBuffer(ShadowUniformBuffer),cbPerProp(cbPerProp) {}
};


vertex Vertex_Shader::PsIn stageMain(
    Vertex_Shader::VSInput input [[stage_in]],
    constant Vertex_Shader::Uniforms_ShadowUniformBuffer & ShadowUniformBuffer [[buffer(8)]],
    constant Vertex_Shader::Uniforms_cbPerProp & cbPerProp [[buffer(7)]])
{
    Vertex_Shader::VSInput input0;
    input0.Position = input.Position;
    input0.TexCoord = input.TexCoord;
    Vertex_Shader main(
    ShadowUniformBuffer,
    cbPerProp);
    return main.main(input0);
}

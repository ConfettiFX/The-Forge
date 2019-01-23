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
#include <metal_stdlib>
using namespace metal;
#ifndef MAX_NUM_OBJECTS
#define MAX_NUM_OBJECTS 64
#endif

struct Vertex_Shader
{

    struct ObjectInfo
    {
        float4x4 toWorld;
        float4x4 normalMat;
        uint matID;
    };
    struct Uniforms_ObjectUniformBlock
    {
        ObjectInfo objectInfo[MAX_NUM_OBJECTS];
    };
    constant Uniforms_ObjectUniformBlock & ObjectUniformBlock;
    struct Uniforms_DrawInfoRootConstant
    {
        uint baseInstance = 0;
    };
    constant Uniforms_DrawInfoRootConstant & DrawInfoRootConstant;
    struct Uniforms_CameraUniform
    {
        float4x4 camViewProj;
        float4x4 camViewMat;
        float4 camClipInfo;
        float4 camPosition;
    };
    constant Uniforms_CameraUniform & CameraUniform;
    struct VSInput
    {
        float4 Position [[attribute(0)]];
        float3 Normal [[attribute(1)]];
        float2 UV [[attribute(2)]];
    };
    struct VSOutput
    {
        float4 Position [[position]];
        float4 WorldPosition;
        float4 Normal;
        float4 UV;
        uint MatID;
#if (PT_USE_DIFFUSION + PT_USE_REFRACTION) != 0
        float4 CSPosition;
#endif
#if PT_USE_REFRACTION != 0
        float4 CSNormal;
#endif
    };
    VSOutput main(VSInput input, uint InstanceID)
    {
        VSOutput output;
        uint instanceID = (InstanceID + DrawInfoRootConstant.baseInstance);
        (output.UV = input.UV.xyyy);
        (output.Normal = normalize(((ObjectUniformBlock.objectInfo[instanceID].normalMat)*(float4(input.Normal.xyz, 0)))));
        (output.WorldPosition = ((ObjectUniformBlock.objectInfo[instanceID].toWorld)*(input.Position)));
        (output.Position = ((CameraUniform.camViewProj)*(output.WorldPosition)));
        (output.MatID = ObjectUniformBlock.objectInfo[instanceID].matID);
#if (PT_USE_REFRACTION+PT_USE_DIFFUSION)!=0
        (output.CSPosition = ((CameraUniform.camViewMat)*(output.WorldPosition)));
#endif

#if PT_USE_REFRACTION!=0
        (output.CSNormal = ((CameraUniform.camViewMat)*(float4(output.Normal.xyz, 0))));
        (output.CSNormal.xyz = normalize(output.CSNormal.xyz));
#endif

        return output;
    };

    Vertex_Shader(
constant Uniforms_ObjectUniformBlock & ObjectUniformBlock,constant Uniforms_DrawInfoRootConstant & DrawInfoRootConstant,constant Uniforms_CameraUniform & CameraUniform) :
ObjectUniformBlock(ObjectUniformBlock),DrawInfoRootConstant(DrawInfoRootConstant),CameraUniform(CameraUniform) {}
};


vertex Vertex_Shader::VSOutput stageMain(
    Vertex_Shader::VSInput input [[stage_in]],
uint InstanceID [[instance_id]],
    constant Vertex_Shader::Uniforms_ObjectUniformBlock & ObjectUniformBlock [[buffer(1)]],
    constant Vertex_Shader::Uniforms_DrawInfoRootConstant & DrawInfoRootConstant [[buffer(2)]],
    constant Vertex_Shader::Uniforms_CameraUniform & CameraUniform [[buffer(11)]])
{
    Vertex_Shader::VSInput input0;
    input0.Position = input.Position;
    input0.Normal = input.Normal;
    input0.UV = input.UV;
    uint InstanceID0;
    InstanceID0 = InstanceID;
    Vertex_Shader main(
    ObjectUniformBlock,
    DrawInfoRootConstant,
    CameraUniform);
    return main.main(input0, InstanceID0);
}

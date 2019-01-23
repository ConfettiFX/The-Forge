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
    };
    VSOutput main(VSInput input, uint InstanceID)
    {
        VSOutput output;
        uint instanceID = (InstanceID + DrawInfoRootConstant.baseInstance);
        float4 pos = ((ObjectUniformBlock.objectInfo[instanceID].toWorld)*(input.Position));
        (output.Position = ((CameraUniform.camViewProj)*(pos)));
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
    constant Vertex_Shader::Uniforms_CameraUniform & CameraUniform [[buffer(3)]])
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

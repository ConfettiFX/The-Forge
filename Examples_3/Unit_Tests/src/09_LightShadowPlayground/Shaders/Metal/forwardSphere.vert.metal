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
#include <metal_stdlib>
using namespace metal;

inline float3x3 matrix_ctor(float4x4 m)
{
        return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}
struct Vertex_Shader
{
    struct Uniforms_objectUniformBlock
    {
        float4x4 WorldViewProjMat[26];
        float4x4 WorldMat[26];
    };
    constant Uniforms_objectUniformBlock & objectUniformBlock;
    struct VsIn
    {
        float3 PositionIn [[attribute(0)]];
        float3 NormalIn [[attribute(1)]];
    };
	uint InstanceIndex;
    struct PsIn
    {
        float3 WorldPositionIn;
        float3 ColorIn;
        float3 NormalIn;
        int IfPlaneIn[[flat]];
        float4 Position [[position]];
    };
    PsIn main(VsIn input)
    {
        PsIn output;
        output.Position = objectUniformBlock.WorldViewProjMat[InstanceIndex] * float4(input.PositionIn, 1.0f);
		output.WorldPositionIn = (objectUniformBlock.WorldMat[InstanceIndex] * float4(input.PositionIn, 1.0f)).xyz;
        if (InstanceIndex == (uint)25)
        {
            ((output).NormalIn = float3(0.0, 1.0, 0.0));
            ((output).IfPlaneIn = 1);
        }
        else
        {
            output.NormalIn = float3x3((objectUniformBlock.WorldMat[InstanceIndex][0]).xyz, (objectUniformBlock.WorldMat[InstanceIndex][1]).xyz, (objectUniformBlock.WorldMat[InstanceIndex][2]).xyz) * normalize(input.NormalIn.xyz);
            output.IfPlaneIn = 0;
        }
        output.ColorIn = float3(1.0);
        return output;
    };

    Vertex_Shader(uint InstanceIndex,
constant Uniforms_objectUniformBlock & objectUniformBlock) :
	InstanceIndex(InstanceIndex),
objectUniformBlock(objectUniformBlock) {}
};


vertex Vertex_Shader::PsIn stageMain(
    Vertex_Shader::VsIn input [[stage_in]],
	uint InstanceIndex [[instance_id]],
    constant Vertex_Shader::Uniforms_objectUniformBlock & objectUniformBlock [[buffer(1)]])
{
    Vertex_Shader::VsIn input0;
    input0.PositionIn = input.PositionIn;
    input0.NormalIn = input.NormalIn;
    Vertex_Shader main(
	InstanceIndex,
    objectUniformBlock);
    return main.main(input0);
}

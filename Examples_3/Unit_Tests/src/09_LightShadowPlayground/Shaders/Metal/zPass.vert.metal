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

struct VsIn
{
    float3 Position [[attribute(0)]];
};

struct PsIn
{
    float4 Position [[position]];
};
	
struct Uniforms_objectUniformBlock
{
    float4x4 WorldViewProjMat[26];
};

vertex PsIn stageMain(
    VsIn input [[stage_in]],
	uint InstanceIndex [[instance_id]],
    constant Uniforms_objectUniformBlock & objectUniformBlock [[buffer(1)]])
{
    PsIn output;
    output.Position = objectUniformBlock.WorldViewProjMat[InstanceIndex] * float4(input.Position, 1.0f);
    return output;
}

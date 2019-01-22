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
#include <metal_atomic>
using namespace metal;

struct Vertex_Shader
{
    struct Uniforms_uniformBlock
    {
        float4x4 mvp;
        float4x4 toWorld;
    };
    constant Uniforms_uniformBlock & uniformBlock;
    struct VSInput
    {
        float4 Position [[attribute(0)]];
        float2 TexCoord [[attribute(1)]];
    };
    struct VSOutput
    {
        float4 Position [[position]];
        float2 TexCoord;
    };
    VSOutput main(VSInput input)
    {
        VSOutput result;
        float4x4 tempMat = ((uniformBlock.mvp)*(uniformBlock.toWorld));
        (result.Position = ((tempMat)*(input.Position)));
        (result.TexCoord = input.TexCoord);
        return result;
    };

    Vertex_Shader(
constant Uniforms_uniformBlock & uniformBlock) :
uniformBlock(uniformBlock) {}
};


vertex Vertex_Shader::VSOutput stageMain(
    Vertex_Shader::VSInput input [[stage_in]],
    constant Vertex_Shader::Uniforms_uniformBlock & uniformBlock [[buffer(1)]])
{
    Vertex_Shader::VSInput input0;
    input0.Position = input.Position;
    input0.TexCoord = input.TexCoord;
    Vertex_Shader main(

    uniformBlock);
    return main.main(input0);
}
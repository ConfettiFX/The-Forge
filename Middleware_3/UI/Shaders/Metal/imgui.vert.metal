/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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
    struct Uniforms_uniformBlockVS
    {
        float4x4 ProjectionMatrix;
    };
    constant Uniforms_uniformBlockVS & uniformBlockVS;
    struct VS_INPUT
    {
        float2 pos [[attribute(0)]];
        float2 uv [[attribute(1)]];
        float4 col [[attribute(2)]];
    };
    struct PS_INPUT
    {
        float4 pos [[position]];
        float4 col;
        float2 uv;
    };
    PS_INPUT main(VS_INPUT input)
    {
        PS_INPUT output;
        ((output).pos = ((uniformBlockVS.ProjectionMatrix)*(float4(((input).pos).xy, 0.0, 1.0))));
        ((output).col = (input).col);
        ((output).uv = (input).uv);
        return output;
    };

    Vertex_Shader(
constant Uniforms_uniformBlockVS & uniformBlockVS) :
uniformBlockVS(uniformBlockVS) {}
};

vertex Vertex_Shader::PS_INPUT stageMain(
    Vertex_Shader::VS_INPUT input  [[stage_in]],
    constant Vertex_Shader::Uniforms_uniformBlockVS& uniformBlockVS [[buffer(0)]]
)
{
    Vertex_Shader::VS_INPUT input0;
    input0.pos = input.pos;
    input0.uv = input.uv;
    input0.col = input.col;
    Vertex_Shader main(uniformBlockVS);
    return main.main(input0);
}

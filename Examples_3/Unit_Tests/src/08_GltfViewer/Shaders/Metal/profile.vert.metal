/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

// Shader for simple shading with a point light
// for planets in Unit Test 01 - Transformations

#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    struct VSInput
    {
        float2 Position;
        float4 Color;
    };
    struct VSOutput
    {
        float4 Position;
        float4 Color;
    };
    VSOutput main(VSInput input)
    {
        VSOutput result;
        ((result).Position = float4(((input).Position).x, ((input).Position).y, 0.0, 1.0));
        ((result).Color = (input).Color);
        return result;
    };

    Vertex_Shader()
    {}
};

struct main_input
{
    float2 POSITION [[attribute(0)]];
    float4 COLOR [[attribute(1)]];
};

struct main_output
{
    float4 SV_POSITION [[position]];
    float4 COLOR;
};

vertex main_output stageMain(
	main_input inputData [[stage_in]])
{
    Vertex_Shader::VSInput input0;
    input0.Position = inputData.POSITION;
    input0.Color = inputData.COLOR;
    Vertex_Shader main;
    Vertex_Shader::VSOutput result = main.main(input0);
    main_output output;
    output.SV_POSITION = result.Position;
    output.COLOR = result.Color;
    return output;
}

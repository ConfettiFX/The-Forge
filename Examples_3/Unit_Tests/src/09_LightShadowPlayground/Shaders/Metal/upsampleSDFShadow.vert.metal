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

/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    struct VsIn
    {
        float4 Position [[attribute(0)]];
        float2 TexCoord [[attribute(1)]];
    };
    struct PsIn
    {
        float4 Position [[position]];
        float2 TexCoord;
    };
    PsIn main(VsIn input)
    {
        PsIn output;
        ((output).Position = float4(((input).Position).xy, 0.0, 1.0));
        ((output).TexCoord = (input).TexCoord);
        return output;
    };

    Vertex_Shader(
) {}
};


vertex Vertex_Shader::PsIn stageMain(
    Vertex_Shader::VsIn input [[stage_in]])
{
    Vertex_Shader::VsIn input0;
    input0.Position = input.Position;
    input0.TexCoord = input.TexCoord;
    Vertex_Shader main;
    return main.main(input0);
}

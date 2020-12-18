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
    struct VsIn
    {
        float2 position [[attribute(0)]];
        float2 texcoord [[attribute(1)]];
    };
    struct VsOut
    {
        float4 position [[position]];
        float2 texcoord;
    };
    struct Uniforms_uRootConstants
    {
        packed_float4 color;
        packed_float2 scaleBias;
    };
    constant Uniforms_uRootConstants & uRootConstants;
    VsOut main(VsIn input)
    {
		VsOut output;
        ((output).position = float4(((((input).position).xy * (uRootConstants.scaleBias).xy) + float2((-1.0), 1.0)), 0.0, 1.0));
        ((output).texcoord = (input).texcoord);
        return output;
    };

    Vertex_Shader(constant Uniforms_uRootConstants & uRootConstants)
    : uRootConstants(uRootConstants)
    {
    }
};


vertex Vertex_Shader::VsOut stageMain(
    Vertex_Shader::VsIn input                                        [[stage_in]],
    constant Vertex_Shader::Uniforms_uRootConstants& uRootConstants  [[buffer(0)]])
{
    Vertex_Shader::VsIn input0;
    input0.position = input.position;
    input0.texcoord = input.texcoord;
    Vertex_Shader main(uRootConstants);
    return main.main(input0);
}

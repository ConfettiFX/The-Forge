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
        float2 texCoord [[attribute(1)]];
    };
    struct PsIn
    {
        float4 position [[position]];
        float2 texCoord;
    };
    struct Uniforms_uRootConstants
    {
        packed_float4 color;
        float2 scaleBias;
    };
    constant Uniforms_uRootConstants & uRootConstants;
    PsIn main(VsIn In)
    {
        PsIn Out;
        ((Out).position = float4((In).position, 0.0, 1.0));
        (((Out).position).xy = ((((Out).position).xy * (uRootConstants.scaleBias).xy) + float2((-1.0), 1.0)));
        ((Out).texCoord = (In).texCoord);
        return Out;
    };

    Vertex_Shader(constant Uniforms_uRootConstants & uRootConstants)
        : uRootConstants(uRootConstants)
    {
    }
};

vertex Vertex_Shader::PsIn stageMain(
                                     Vertex_Shader::VsIn In                                           [[stage_in]],
                                     constant Vertex_Shader::Uniforms_uRootConstants& uRootConstants [[buffer(0)]]
)
{
    Vertex_Shader::VsIn In0;
    In0.position = In.position;
    In0.texCoord = In.texCoord;
    Vertex_Shader main(uRootConstants);
    return main.main(In0);
}

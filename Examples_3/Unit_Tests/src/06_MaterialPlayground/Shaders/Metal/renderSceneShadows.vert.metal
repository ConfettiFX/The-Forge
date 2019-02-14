/*
 * Copyright (c) 2019 Confetti Interactive Inc.
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
    struct Uniforms_cbCamera
    {
        float4x4 projView;
    };
    constant Uniforms_cbCamera & cbCamera;
    struct Uniforms_cbObject
    {
        float4x4 worldMat;
    };
    constant Uniforms_cbObject & cbObject;
    struct VSInput
    {
        float3 Position [[attribute(0)]];
    };
    struct VSOutput
    {
        float4 Position [[position]];
    };
    VSOutput main(VSInput input)
    {
        VSOutput result;
        float4x4 mvp = ((cbCamera.projView)*(cbObject.worldMat));
        ((result).Position = ((mvp)*(float4(((input).Position).xyz, 1.0))));
        return result;
    };

    Vertex_Shader(
constant Uniforms_cbCamera & cbCamera,constant Uniforms_cbObject & cbObject) :
cbCamera(cbCamera),cbObject(cbObject) {}
};


vertex Vertex_Shader::VSOutput stageMain(
    Vertex_Shader::VSInput input [[stage_in]],
    constant Vertex_Shader::Uniforms_cbCamera & cbCamera [[buffer(2)]],
    constant Vertex_Shader::Uniforms_cbObject & cbObject [[buffer(3)]])
{
    Vertex_Shader::VSInput input0;
    input0.Position = input.Position;
    Vertex_Shader main(
    cbCamera,
    cbObject);
    return main.main(input0);
}

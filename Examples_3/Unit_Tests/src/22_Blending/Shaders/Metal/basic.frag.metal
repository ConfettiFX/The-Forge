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
#include <metal_atomic>
using namespace metal;

inline float3x3 matrix_ctor(float4x4 m) {
        return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}
struct Fragment_Shader
{
#define MAX_JOINTS 815

	struct Uniforms_uniformBlock {
		
		float4x4 mvp;
		float4x4 toWorld[MAX_JOINTS];
		float4 color[MAX_JOINTS];
		
		float3 lightPosition;
		float3 lightColor;
	};
    struct VSOutput
    {
        float4 Position [[position]];
        float4 Color;
    };

    float4 main(VSOutput input)
    {
        return input.Color;
    };

    Fragment_Shader() {}
};

fragment float4 stageMain(Fragment_Shader::VSOutput input [[stage_in]])
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.Color = input.Color;
    Fragment_Shader main;
    return main.main(input0);
}

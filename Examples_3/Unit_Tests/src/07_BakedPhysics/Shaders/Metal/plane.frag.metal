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

struct Fragment_Shader
{
    struct VSOutput
    {
        float4 Position [[position]];
        float2 TexCoord;
    };
	
    float4 main(VSOutput input)
    {
        float tol = 0.0025000000;
        float res = 0.05;
        float4 backgroundColor = float4(0.49f, 0.64f, 0.68f, 1.0f);
        float4 lineColor = float4(0.39, 0.41, 0.37, 1.0);
        float4 originColor = float4(0.0, 0.0, 0.0, 1.0);
        if (((abs((input.TexCoord.x - 0.5)) <= tol) && (abs((input.TexCoord.y - 0.5)) <= tol)))
        {
            return originColor;
        }
        else if (((((fmod(input.TexCoord.x, res) >= (res - tol)) || (fmod(input.TexCoord.x, res) < tol)) || (fmod(input.TexCoord.y, res) >= (res - tol))) || (fmod(input.TexCoord.y, res) < tol)))
        {
            return lineColor;
        }
        else
        {
            return backgroundColor;
        }
    };

	Fragment_Shader() {}
};


fragment float4 stageMain(
    Fragment_Shader::VSOutput input [[stage_in]],
    sampler uSampler0 [[sampler(0)]],
    texture2d<float> Texture [[texture(0)]])
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.TexCoord = input.TexCoord;
    Fragment_Shader main;
    return main.main(input0);
}

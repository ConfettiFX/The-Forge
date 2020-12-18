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
    struct Uniforms_uniformBlock
    {
        float4x4 ProjectionViewMat;
        float4x4 ModelMat[2];
    };
    constant Uniforms_uniformBlock & uniformBlock;
    struct VSInput
    {
        float4 Position;
        float4 Normal;
    };
    struct VSOutput
    {
        float4 Position;
        float4 Color;
    };
    VSOutput main(VSInput input)
    {
        VSOutput result;
        float3 lightPos = float3(15.0, 0.0, 0.0);
        float3 lightCol = float3(1.0, 1.0, 1.0);
        float4 objColor = float4((float)0.8, (float)0.5, (float)0.144, (float)1.0);
        float4x4 tempMat = ((uniformBlock.ProjectionViewMat)*(uniformBlock.ModelMat[0]));
        ((result).Position = ((tempMat)*((input).Position)));
        float4 normal = normalize(((uniformBlock.ModelMat[0])*(float4(((input).Normal).xyz, 0.0))));
        float4 pos = ((uniformBlock.ModelMat[0])*(float4(((input).Position).xyz, 1.0)));
        float lightIntensity = 1.0;
        float ambientCoeff = (float)(0.4);
        float3 lightDir;
        (lightDir = normalize((lightPos - (pos).xyz)));
        float3 baseColor = (objColor).xyz;
        float3 blendedColor = (((lightCol * baseColor))*(lightIntensity));
        float3 diffuse = ((blendedColor)*(max((float)dot((normal).xyz, lightDir),(float)0.0)));
        float3 ambient = ((baseColor)*(ambientCoeff));
        ((result).Color = float4((diffuse + ambient), (float)1.0));
        return result;
    };

    Vertex_Shader(
constant Uniforms_uniformBlock & uniformBlock) :
uniformBlock(uniformBlock) {}
};


struct main_input
{
    float4 POSITION [[attribute(0)]];
    float4 NORMAL [[attribute(1)]];
};

struct main_output
{
    float4 SV_POSITION [[position]];
    float4 COLOR;
};

vertex main_output stageMain(
	main_input inputData [[stage_in]],
    constant Vertex_Shader::Uniforms_uniformBlock& uniformBlock [[buffer(0)]]
)
{
    Vertex_Shader::VSInput input0;
    input0.Position = inputData.POSITION;
    input0.Normal = inputData.NORMAL;
    Vertex_Shader main(
    uniformBlock);
    Vertex_Shader::VSOutput result = main.main(input0);
    main_output output;
    output.SV_POSITION = result.Position;
    output.COLOR = result.Color;
    return output;
}

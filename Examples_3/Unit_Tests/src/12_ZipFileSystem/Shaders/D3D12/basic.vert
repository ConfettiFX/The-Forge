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

cbuffer uniformBlock : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4x4 ProjectionViewMat;
    float4x4 ModelMatrixCapsule;
    float4x4 ModelMatrixCube;
};

struct VSInput
{
    float4 Position : POSITION;
    float4 Normal : NORMAL;
};

struct VSOutput {
	float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput result;
	
	float3 lightPos = float3(15.0f,0.0f,0.0f);
	float3 lightCol = float3(1.0f,1.0f,1.0f);
	float4 objColor = float4(0.8,0.5,0.144,1.0);
	

    float4x4 tempMat = mul(ProjectionViewMat, ModelMatrixCapsule);
    result.Position = mul(tempMat, input.Position);

    float4 normal = normalize(mul(ModelMatrixCapsule, float4(input.Normal.xyz, 0.0f))); // Assume uniform scaling
    float4 pos = mul(ModelMatrixCapsule, float4(input.Position.xyz, 1.0f));

    float lightIntensity = 1.0f;
    float quadraticCoeff = 1.2;
    float ambientCoeff = 0.4;

    float3 lightDir;
	   
    lightDir = normalize(lightPos - pos.xyz);

    float distance = length(lightDir);
    float attenuation = 1.0 / (quadraticCoeff * distance * distance);
    float intensity = lightIntensity * attenuation;

    float3 baseColor = objColor.xyz;
    float3 blendedColor = mul(lightCol * baseColor, lightIntensity);
    float3 diffuse = mul(blendedColor, max(dot(normal.xyz, lightDir), 0.0));
    float3 ambient = mul(baseColor, ambientCoeff);
    result.Color = float4(diffuse + ambient, 1.0);

    return result;
}

/*
* Copyright (c) 2018 Confetti Interactive Inc.
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

#define SPECULAR_EXP 10.0f

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 WorldPosition : POSITION;
	float4 Color : COLOR;
	float4 Normal : NORMAL;
};

cbuffer LightUniformBlock : register(b0)
{
	float4x4 lightViewProj;
	float4 lightDirection;
	float4 lightColor;
};

cbuffer CameraUniform : register(b3)
{
	float4 CameraPosition;
};

float4 main(VSOutput input) : SV_Target
{    
	float3 normal = normalize(input.Normal.xyz);
	float3 lightVec = -normalize(lightDirection.xyz);
	float3 viewVec = normalize(input.WorldPosition.xyz - CameraPosition.xyz);
	float dotP = dot(normal, lightVec.xyz);
	if (dotP < 0.05f)
		dotP = 0.05f;//set as ambient color
	float3 diffuse = lightColor.xyz * input.Color.xyz * dotP;
	float3 specular = lightColor.xyz * pow(saturate(dot(reflect(lightVec, normal), viewVec)), SPECULAR_EXP);
	float3 finalColor = saturate(diffuse+ specular*0.5f);
	return float4((finalColor).xyz, input.Color.a);
}

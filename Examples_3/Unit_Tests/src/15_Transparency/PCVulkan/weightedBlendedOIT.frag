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

#version 450 core

#define SPECULAR_EXP 10.0f

layout (location = 0) in vec4 WorldPosition;
layout (location = 1) in vec4 Color;
layout (location = 2) in vec4 NormalOut;

layout(location = 0) out vec4 Accumulation;
layout(location = 1) out float Revealage;

layout(set = 0, binding = 9) uniform LightUniformBlock
{
	mat4 lightViewProj;
	vec4 lightDirection;
	vec4 lightColor;
};

layout(set = 0, binding = 12) uniform CameraUniform
{
	vec4 CameraPosition;
};

layout(set = 0, binding = 13) uniform WBOITSettings
{
	float colorResistance;	// Increase if low-coverage foreground transparents are affecting background transparent color.
	float rangeAdjustment;	// Change to avoid saturating at the clamp bounds.
	float depthRange;		// Decrease if high-opacity surfaces seem “too transparent”, increase if distant transparents are blending together too much.
	float orderingStrength;	// Increase if background is showing through foreground too much.
	float underflowLimit;	// Increase to reduce underflow artifacts.
	float overflowLimit;	// Decrease to reduce overflow artifacts.
};

float WeightFunction(float alpha, float depth)
{
	return pow(alpha, colorResistance) * clamp(0.3f / (1e-5f + pow(depth / depthRange, orderingStrength)), underflowLimit, overflowLimit);
}

void main()
{
	vec3 normal = normalize(NormalOut.xyz);
	vec3 lightVec = -normalize(lightDirection.xyz);
	vec3 viewVec = normalize(WorldPosition.xyz - CameraPosition.xyz);
	float dotP = dot(normal, lightVec);
	if(dotP < 0.05f)
		dotP = 0.05f;
	vec3 diffuse = lightColor.xyz * Color.rgb * dotP;
	vec3 specular = lightColor.xyz * pow(clamp(dot(reflect(lightVec, normal), viewVec), 0, 1), SPECULAR_EXP);
	vec4 finalColor = vec4(clamp(diffuse + specular * 0.5f, 0, 1), Color.a);

	float d = gl_FragCoord.z / gl_FragCoord.w;
	vec4 premultipliedColor = vec4(finalColor.rgb * finalColor.a, finalColor.a);

	float w = WeightFunction(premultipliedColor.a, d);
	Accumulation = premultipliedColor * w;
	Revealage = premultipliedColor.a;
}
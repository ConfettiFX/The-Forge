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

layout(location = 0) out vec4 FinalColor;

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
	FinalColor = vec4(clamp(diffuse + specular * 0.5f, 0, 1), Color.a);
}
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

#version 450 core
#extension GL_GOOGLE_include_directive : require

#include "shading.glsl"

// source: https://www.shadertoy.com/view/4sfGzS
float hash(vec3 p)
{
	p = fract(p * 0.3183099f + 0.1f);
	p *= 17.0f;
	return fract(p.x*p.y*p.z*(p.x + p.y + p.z));
}

float noise(vec3 x)
{
	vec3 p = floor(x);
	vec3 f = fract(x);
	f = f * f*(3.0 - 2.0*f);

	return mix(mix(mix(hash(p + vec3(0, 0, 0)),
		hash(p + vec3(1, 0, 0)), f.x),
		mix(hash(p + vec3(0, 1, 0)),
		hash(p + vec3(1, 1, 0)), f.x), f.y),
		mix(mix(hash(p + vec3(0, 0, 1)),
		hash(p + vec3(1, 0, 1)), f.x),
		mix(hash(p + vec3(0, 1, 1)),
		hash(p + vec3(1, 1, 1)), f.x), f.y), f.z);
}

layout(location = 0) in vec4 WorldPosition;
layout(location = 1) in vec4 Normal;
layout(location = 2) in vec4 UV;
layout(location = 3) flat in uint MatID;

layout(location = 0) out vec2 RedVarianceShadowMap;
layout(location = 1) out vec2 GreenVarianceShadowMap;
layout(location = 2) out vec2 BlueVarianceShadowMap;

void main()
{ 
	Material mat = Materials[MatID];
	vec4 matColor = bool(mat.TextureFlags & 1) ? texture(sampler2D(MaterialTextures[mat.AlbedoTexID], LinearSampler), UV.xy) : mat.Color;
	vec3 p = (1.0f - mat.Transmission.xyz) * matColor.a;
	float e = noise(WorldPosition.xyz * 10000.0f);

	vec3 normal = normalize(Normal.xyz);

	vec3 ld = vec3(camViewMat[2][0], camViewMat[2][1], camViewMat[2][2]);
	float s = clamp((mat.RefractionRatio - 1.0f) * 0.5f, 0.0f, 1.0f);
	float g = 2.0f * clamp(1.0f - pow(dot(normalize(normal), -ld.xyz), 128.0f * s*s), 0, 1) - 1.0f;
	p = min(vec3(1.0f), (1.0f + g * pow(s, 0.2f)) * p);

	vec2 moments = ComputeMoments(gl_FragCoord.z);
	RedVarianceShadowMap = max(moments, vec2(e > p.r));
	GreenVarianceShadowMap = max(moments, vec2(e > p.g));
	BlueVarianceShadowMap = max(moments, vec2(e > p.b));
}

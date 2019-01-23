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

layout(location = 0) in vec4 WorldPosition;
layout(location = 1) in vec4 Normal;
layout(location = 2) in vec4 UV;
layout(location = 3) flat in uint MatID;
#if (PT_USE_REFRACTION + PT_USE_DIFFUSION) != 0
layout(location = 4) in vec4 CSPosition;
#endif
#if PT_USE_REFRACTION != 0
layout(location = 5) in vec4 CSNormal;
#endif

layout(location = 0) out vec4 Accumulation;
layout(location = 1) out vec4 Modulation;
#if PT_USE_REFRACTION != 0
layout(location = 2) out vec2 Refraction;
#endif

#if PT_USE_DIFFUSION != 0
struct ObjectInfo
{
	mat4 toWorld;
	mat4 normalMat;
	uint matID;
};

layout(set = 0, binding = 0) uniform ObjectUniformBlock
{
	ObjectInfo	objectInfo[MAX_NUM_OBJECTS];
};

layout(set = 0, binding = 1) uniform texture2D DepthTexture;
layout(set = 0, binding = 2) uniform sampler PointSampler;
#endif

float WeightFunction(float alpha, float depth)
{
	float tmp = 1.0f - depth * 0.99f;
	tmp *= tmp * tmp * 1e4f;
	return clamp(alpha * tmp, 1e-3f, 1.5e2);
}

vec2 ComputeRefractionOffset(vec3 csNormal, vec3 csPosition, float eta)
{
	const vec2 backSizeInMeters = vec2(1000.0f * (1.0f / ((1.0f - eta) * 100.0f)));	// HACK: Background size is supposed to be calculated per object. This is a quick work around that looks good to us.

	const float backgroundZ = csPosition.z - 4.0f;

	vec3 dir = normalize(csPosition);
	vec3 refracted = refract(dir, csNormal, eta);

	bool totalInternalRefraction = bool(dot(refracted, refracted) < 0.01f);
	if (totalInternalRefraction)
		return vec2(0.0f);
	else
	{
		vec3 plane = csPosition;
		plane.z -= backgroundZ;

		vec2 hit = (plane.xy - refracted.xy * plane.z / refracted.z);
		vec2 backCoord = (hit / backSizeInMeters) + 0.5f;
		vec2 startCoord = (csPosition.xy / backSizeInMeters) + 0.5f;
		return backCoord - startCoord;
	}
}

void main()
{
	vec3 transmission = Materials[MatID].Transmission.xyz;
	float collimation = Materials[MatID].Collimation;
	vec4 finalColor = Shade(MatID, UV.xy, WorldPosition.xyz, normalize(Normal.xyz));

	float d = 1.0f - (gl_FragCoord.z / gl_FragCoord.w);
	vec4 premultipliedColor = vec4(finalColor.rgb * finalColor.a, finalColor.a);
	float coverage = finalColor.a;

	Modulation.rgb = coverage * (1.0f - transmission);
	coverage *= 1.0f - (transmission.r + transmission.g + transmission.b) * (1.0f / 3.0f);

	float w = WeightFunction(coverage, d);
	Accumulation = vec4(premultipliedColor.rgb, coverage) * w;

#if PT_USE_DIFFUSION != 0
	if (collimation < 1.0f)
	{
		float backgroundDepth = texelFetch(sampler2D(DepthTexture, PointSampler), ivec2(gl_FragCoord.xy), 0).r;
		backgroundDepth = camClipInfo[0] / (camClipInfo[1] * backgroundDepth + camClipInfo[2]);

		const float scaling = 8.0f;
		const float focusRate = 0.1f;

		Modulation.a = scaling * coverage * (1.0f - collimation) * (1.0f - focusRate / (focusRate + CSPosition.z - backgroundDepth)) / max(abs(CSPosition.z), 1e-5f);
		Modulation.a *= Modulation.a;
		Modulation.a = max(Modulation.a, 1.0f / 256.0f);
	}
	else
		Modulation.a = 0.0f;
#else
	Modulation.a = 0.0f;
#endif


#if PT_USE_REFRACTION != 0
	float eta = 1.0f / Materials[MatID].RefractionRatio;
	vec2 refractionOffset = vec2(0.0f);
	if (eta != 1.0f)
		refractionOffset = ComputeRefractionOffset(normalize(CSNormal.xyz), CSPosition.xyz, eta);
	Refraction = refractionOffset * coverage * 8.0f;
#endif
}

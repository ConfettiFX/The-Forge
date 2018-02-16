#version 450 core
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

// USERMACRO: SAMPLE_COUNT [1,2,4]
// USERMACRO: USE_AMBIENT_OCCLUSION [0,1]

#extension GL_GOOGLE_include_directive : enable

#include "shader_defs.h"
#include "shading.h"

#if(SAMPLE_COUNT > 1)
layout(set = 0, binding=0) uniform texture2DMS gBufferColor;
layout(set = 0, binding=1) uniform texture2DMS gBufferNormal;
layout(set = 0, binding=2) uniform texture2DMS gBufferSpecular;
layout(set = 0, binding=3) uniform texture2DMS gBufferDepth;
layout(set = 0, binding = 4) uniform texture2DMS gBufferSimulation;
#else
layout(set = 0, binding=0) uniform texture2D gBufferColor;
layout(set = 0, binding=1) uniform texture2D gBufferNormal;
layout(set = 0, binding=2) uniform texture2D gBufferSpecular;
layout(set = 0, binding=3) uniform texture2D gBufferSimulation;
layout(set = 0, binding=4) uniform texture2D gBufferDepth;
#endif
layout(set = 0, binding = 5) uniform texture2D shadowMap;
#if USE_AMBIENT_OCCLUSION
layout(set = 0, binding = 6) uniform texture2D aoTex;
#endif
layout(set = 0, binding = 7) uniform sampler depthSampler;
layout(set = 0, binding = 8) uniform uniforms
{
	PerFrameConstants uniformsData;
};

layout(location = 0) in vec2 iScreenPos;

layout(location = 0) out vec4 oColor;

// Pixel shader
void main()
{
	// Load gBuffer data from render target
#if(SAMPLE_COUNT > 1)
    vec4 normalData = texelFetch(sampler2DMS(gBufferNormal, depthSampler),ivec2(gl_FragCoord.xy), gl_SampleID);
#else
    vec4 normalData = texelFetch(sampler2D(gBufferNormal, depthSampler), ivec2(gl_FragCoord.xy), 0);
#endif

	if (normalData.x == 0 && normalData.y == 0 && normalData.z == 0)
	{
		discard;
		return;
	}

#if(SAMPLE_COUNT > 1)
	vec3 colorData = texelFetch(sampler2DMS(gBufferColor, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID).rgb;
    vec4 specularData = texelFetch(sampler2DMS(gBufferSpecular, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID);
	vec4 simulation = texelFetch(sampler2DMS(gBufferSimulation, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID);
    float  depth = texelFetch(sampler2DMS(gBufferDepth, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID).r;
#else
	vec3 colorData = texelFetch(sampler2D(gBufferColor, depthSampler), ivec2(gl_FragCoord.xy), 0).rgb;
    vec4 specularData = texelFetch(sampler2D(gBufferSpecular, depthSampler), ivec2(gl_FragCoord.xy), 0);
	vec4 simulation = texelFetch(sampler2D(gBufferSimulation, depthSampler), ivec2(gl_FragCoord.xy), 0);
    float depth = texelFetch(sampler2D(gBufferDepth, depthSampler), ivec2(gl_FragCoord.xy), 0).r;
#endif

#if USE_AMBIENT_OCCLUSION
	float ao = texelFetch(sampler2D(aoTex, depthSampler), ivec2(gl_FragCoord.xy), 0).r;
#else
	float ao = 1.0f;
#endif
	bool twoSided = (normalData.w > 0.5f);

	vec3 normal = normalData.xyz * 2.0f - 1.0f;

	vec4 position = uniformsData.transform[VIEW_CAMERA].invVP * vec4(iScreenPos, depth, 1);
	vec3 posWS = position.xyz / position.w;

	vec4 posLS = uniformsData.transform[VIEW_SHADOW].vp * position + simulation;;
	vec3 color = calculateIllumination(normal, uniformsData.camPos.xyz, uniformsData.esmControl, uniformsData.lightDir.xyz, twoSided, posLS, posWS, shadowMap, colorData.xyz, specularData.xyz, ao, depthSampler);

	oColor = vec4(color, 1);
}

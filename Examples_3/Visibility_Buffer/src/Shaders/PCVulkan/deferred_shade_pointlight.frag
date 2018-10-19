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

#extension GL_GOOGLE_include_directive : enable

#include "shading.h"

layout(std140, set = 0, binding = 0) uniform uniforms
{
	PerFrameConstants uniformsData;
};

#if(SAMPLE_COUNT > 1)
layout(set = 0, binding=2) uniform texture2DMS gBufferNormal;
layout(set = 0, binding=3) uniform texture2DMS gBufferSpecular;
layout(set = 0, binding=4) uniform texture2DMS gBufferSimulation;
layout(set = 0, binding=5) uniform texture2DMS gBufferDepth;
#else
layout(set = 0, binding=2) uniform texture2D gBufferNormal;
layout(set = 0, binding=3) uniform texture2D gBufferSpecular;
layout(set = 0, binding=4) uniform texture2D gBufferSimulation;
layout(set = 0, binding=5) uniform texture2D gBufferDepth;
#endif

layout(set = 0, binding = 6) uniform sampler depthSampler;
layout(location = 0) in vec3 iLightColor;
layout(location = 1) in vec3 iLightPos;

layout(location = 0) out vec4 oColor;

void main()
{
#if(SAMPLE_COUNT > 1)
    vec4 normalData = texelFetch(sampler2DMS(gBufferNormal, depthSampler),ivec2(gl_FragCoord.xy), gl_SampleID);
    vec4 specularData = texelFetch(sampler2DMS(gBufferSpecular, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID);
	vec4 simulation = texelFetch(sampler2DMS(gBufferSimulation, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID);
    float  depth = texelFetch(sampler2DMS(gBufferDepth, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID).r;
#else
    vec4 normalData = texelFetch(sampler2D(gBufferNormal, depthSampler), ivec2(gl_FragCoord.xy), 0);
    vec4 specularData = texelFetch(sampler2D(gBufferSpecular, depthSampler), ivec2(gl_FragCoord.xy), 0);
	vec4 simulation = texelFetch(sampler2D(gBufferSimulation, depthSampler), ivec2(gl_FragCoord.xy), 0);
    float depth = texelFetch(sampler2D(gBufferDepth, depthSampler), ivec2(gl_FragCoord.xy), 0).r;
#endif

	vec2 screenPos = ((gl_FragCoord.xy / uniformsData.cullingViewports[VIEW_CAMERA].windowSize) * 2.0f - 1.0f);
	screenPos.y = -screenPos.y;
	vec3 normal = normalData.xyz * 2.0f - 1.0f;
	vec4 position = uniformsData.transform[VIEW_CAMERA].invVP * vec4(screenPos, depth, 1) + simulation;
	vec3 posWS = position.xyz / position.w;
	bool twoSided = (normalData.w > 0.5f);
	
	vec3 color = pointLightShade(iLightPos, iLightColor, uniformsData.camPos.xyz, posWS, normal, specularData.xyz, twoSided);
	oColor = vec4(color, 1);
}

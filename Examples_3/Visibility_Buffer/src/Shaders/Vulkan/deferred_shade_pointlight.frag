#version 450 core
#if !defined(WINDOWS) && !defined(ANDROID) && !defined(LINUX)
#define WINDOWS 	// Assume windows if no platform define has been added to the shader
#endif


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

// USERMACRO: SAMPLE_COUNT [1,2,4]

#extension GL_GOOGLE_include_directive : enable

#include "shading.h"

layout(std140, set = 0, binding = 0) uniform uniforms
{
	PerFrameConstants uniformsData;
};

layout(push_constant) uniform RootConstantDrawScene_Block
{
    vec4 lightColor;
	uint lightingMode;
	uint outputMode;
	vec4 CameraPlane; //x : near, y : far
}RootConstantDrawScene;


#if(SAMPLE_COUNT > 1)
layout(set = 0, binding=2) uniform texture2DMS gBufferColor;
layout(set = 0, binding=3) uniform texture2DMS gBufferNormal;
layout(set = 0, binding=4) uniform texture2DMS gBufferSpecular;
layout(set = 0, binding=5) uniform texture2DMS gBufferSimulation;
layout(set = 0, binding=6) uniform texture2DMS gBufferDepth;
#else
layout(set = 0, binding=2) uniform texture2D gBufferColor;
layout(set = 0, binding=3) uniform texture2D gBufferNormal;
layout(set = 0, binding=4) uniform texture2D gBufferSpecular;
layout(set = 0, binding=5) uniform texture2D gBufferSimulation;
layout(set = 0, binding=6) uniform texture2D gBufferDepth;
#endif

layout(set = 0, binding = 7) uniform sampler depthSampler;
layout(location = 0) in vec3 iLightColor;
layout(location = 1) in vec3 iLightPos;

layout(location = 0) out vec4 oColor;

void main()
{
#if(SAMPLE_COUNT > 1)
	vec4 albedoData = texelFetch(sampler2DMS(gBufferColor, depthSampler),ivec2(gl_FragCoord.xy), gl_SampleID);
    vec4 normalData = texelFetch(sampler2DMS(gBufferNormal, depthSampler),ivec2(gl_FragCoord.xy), gl_SampleID);
    vec4 specularData = texelFetch(sampler2DMS(gBufferSpecular, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID);
	vec4 simulation = texelFetch(sampler2DMS(gBufferSimulation, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID);
    float  depth = texelFetch(sampler2DMS(gBufferDepth, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID).r;
#else
	vec4 albedoData = texelFetch(sampler2D(gBufferColor, depthSampler),ivec2(gl_FragCoord.xy), 0);
    vec4 normalData = texelFetch(sampler2D(gBufferNormal, depthSampler), ivec2(gl_FragCoord.xy), 0);
    vec4 specularData = texelFetch(sampler2D(gBufferSpecular, depthSampler), ivec2(gl_FragCoord.xy), 0);
	vec4 simulation = texelFetch(sampler2D(gBufferSimulation, depthSampler), ivec2(gl_FragCoord.xy), 0);
    float depth = texelFetch(sampler2D(gBufferDepth, depthSampler), ivec2(gl_FragCoord.xy), 0).r;
#endif

    float fLightingMode = clamp(float(RootConstantDrawScene.lightingMode), 0.0, 1.0);

	vec2 screenPos = ((gl_FragCoord.xy / uniformsData.cullingViewports[VIEW_CAMERA].windowSize) * 2.0f - 1.0f);
	screenPos.y = -screenPos.y;
	vec3 normal = normalData.xyz * 2.0f - 1.0f;
	vec4 position = uniformsData.transform[VIEW_CAMERA].invVP * vec4(screenPos, depth, 1) + simulation;
	vec3 posWS = position.xyz / position.w;
	
    bool isTwoSided = (albedoData.a > 0.5);
	bool isBackFace = false;

	vec3 ViewVec = normalize(uniformsData.camPos.xyz - posWS.xyz);
	
	//if it is backface
	//this should be < 0 but our mesh's edge normals are smoothed, badly
	
	if(isTwoSided && dot(normal, ViewVec) < 0.0)
	{
		//flip normal
		normal = -normal;
		isBackFace = true;
	}

	vec3 HalfVec = normalize(ViewVec - uniformsData.lightDir.xyz);
	vec3 ReflectVec = reflect(-ViewVec, normal);
	float NoV = clamp(dot(normal, ViewVec), 0.0, 1.0);

	float NoL = dot(normal, -uniformsData.lightDir.xyz);	

	// Deal with two faced materials
	NoL = (isTwoSided ? abs(NoL) : clamp(NoL, 0.0, 1.0));

	vec3 F0 = specularData.xyz;
	vec3 DiffuseColor = albedoData.xyz;

	float Roughness = specularData.w;
	float Metallic = normalData.w;

	vec3 color = pointLightShade(
	normal,
	ViewVec,
	HalfVec,
	ReflectVec,
	NoL,
	NoV,
	iLightPos,
    iLightColor.rgb,
	uniformsData.camPos.xyz + simulation.xyz,
	uniformsData.lightDir.xyz,
	position,
	posWS,
	DiffuseColor,
	F0,
	Roughness,
	Metallic,		
	isBackFace,
	fLightingMode);
    
	oColor = vec4(color, 1.0);
}

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

layout(push_constant) uniform RootConstantDrawScene_Block
{
    vec4 lightColor;
	uint lightingMode;
	uint outputMode;
	vec4 CameraPlane; //x : near, y : far
}RootConstantDrawScene;

layout(location = 0) in vec2 iScreenPos;

layout(location = 0) out vec4 oColor;

// Pixel shader
void main()
{
	// Load gBuffer data from render target
#if(SAMPLE_COUNT > 1)
	float  depth = texelFetch(sampler2DMS(gBufferDepth, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID).r;
#else
    float depth = texelFetch(sampler2D(gBufferDepth, depthSampler), ivec2(gl_FragCoord.xy), 0).r;
#endif
	// Since we don't have a skydome, this code is added here to avoid shading the sky.
  	if (depth == 1.0f)
    {
		discard;
    }
#if(SAMPLE_COUNT > 1)
	vec4 colorData = texelFetch(sampler2DMS(gBufferColor, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID);
    vec4 specularData = texelFetch(sampler2DMS(gBufferSpecular, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID);
	vec4 simulation = texelFetch(sampler2DMS(gBufferSimulation, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID);
    vec4 normalData = texelFetch(sampler2DMS(gBufferNormal, depthSampler),ivec2(gl_FragCoord.xy), gl_SampleID);
#else
	vec4 colorData = texelFetch(sampler2D(gBufferColor, depthSampler), ivec2(gl_FragCoord.xy), 0);
    vec4 specularData = texelFetch(sampler2D(gBufferSpecular, depthSampler), ivec2(gl_FragCoord.xy), 0);
	vec4 simulation = texelFetch(sampler2D(gBufferSimulation, depthSampler), ivec2(gl_FragCoord.xy), 0);
    vec4 normalData = texelFetch(sampler2D(gBufferNormal, depthSampler), ivec2(gl_FragCoord.xy), 0);
#endif

#if USE_AMBIENT_OCCLUSION
	float ao = texelFetch(sampler2D(aoTex, depthSampler), ivec2(gl_FragCoord.xy), 0).r;
#else
	float ao = 1.0f;
#endif

	vec3 normal = normalize(normalData.xyz * 2.0f - 1.0f);
	vec4 position = uniformsData.transform[VIEW_CAMERA].invVP * vec4(iScreenPos, depth, 1);
	vec3 posWS = position.xyz / position.w;

	bool isTwoSided = colorData.a > 0.5f ? true : false;
	bool isBackFace = false;

	vec3 ViewVec = normalize(uniformsData.camPos.xyz - posWS);
	
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

	vec4 posLS = uniformsData.transform[VIEW_SHADOW].vp * position + simulation;

	vec3 DiffuseColor = colorData.xyz;
	
	float shadowFactor = 1.0f;

	float fLightingMode = clamp(float(RootConstantDrawScene.lightingMode), 0.0, 1.0);

	float Roughness = clamp(specularData.a, 0.05f, 0.99f);
	float Metallic = specularData.b;

	vec3 shadedColor = calculateIllumination(
		    normal,
		    ViewVec,
			HalfVec,
			ReflectVec,
			NoL,
			NoV,
			uniformsData.camPos.xyz,
			uniformsData.esmControl,
			uniformsData.lightDir.xyz,
			posLS,
			posWS,  
			shadowMap,
			DiffuseColor,
			DiffuseColor,
			Roughness,
			Metallic,			
			depthSampler,
			isBackFace,
			fLightingMode,
			shadowFactor);


	shadedColor = shadedColor * RootConstantDrawScene.lightColor.rgb * RootConstantDrawScene.lightColor.a * NoL * ao;

	float ambientIntencity = 0.2f;
	vec3 ambient = colorData.xyz * ambientIntencity;

	vec3 FinalColor = shadedColor + ambient;
		
	oColor = vec4(FinalColor, 1.0);
}

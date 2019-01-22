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

// This shader loads gBuffer data and shades the pixel.

#include <metal_stdlib>
using namespace metal;

#include "shader_defs.h"
#include "shading.h"

struct VSOutput {
	float4 position [[position]];
    float2 screenPos;
    uint triangleId;
};

struct RootConstantDrawSceneData
{
	float4 lightColor;
	uint lightingMode;
	uint outputMode;
	float4 CameraPlane; //x : near, y : far
};

// Pixel shader
fragment float4 stageMain(VSOutput input                                       [[stage_in]],
                          uint32_t sampleID                                    [[sample_id]],
#if SAMPLE_COUNT > 1
                          texture2d_ms<float,access::read> gBufferColor        [[texture(0)]],
                          texture2d_ms<float,access::read> gBufferNormal       [[texture(1)]],
                          texture2d_ms<float,access::read> gBufferSpecular     [[texture(2)]],
                          texture2d_ms<float,access::read> gBufferSimulation   [[texture(3)]],
                          depth2d_ms<float,access::read> gBufferDepth          [[texture(4)]],
#else
                          texture2d<float,access::read> gBufferColor           [[texture(0)]],
                          texture2d<float,access::read> gBufferNormal          [[texture(1)]],
                          texture2d<float,access::read> gBufferSpecular        [[texture(2)]],
                          texture2d<float,access::read> gBufferSimulation      [[texture(3)]],
                          depth2d<float,access::read> gBufferDepth             [[texture(4)]],
#endif
                          texture2d<float,access::sample> aoTex                [[texture(5)]],
                          depth2d<float,access::sample> shadowMap              [[texture(6)]],
                          constant PerFrameConstants& uniforms                 [[buffer(7)]],
                          constant LightData* lights                           [[buffer(8)]],
						  constant RootConstantDrawSceneData& RootConstantDrawScene          [[buffer(9)]],
                          sampler depthSampler                                 [[sampler(0)]])
{
    // Load gBuffer data from render target
    float depth = gBufferDepth.read(uint2(input.position.xy), sampleID);
    if (depth == 1.0f) discard_fragment();
    
    float4 colorData = gBufferColor.read(uint2(input.position.xy), sampleID);
    float4 normalData = gBufferNormal.read(uint2(input.position.xy), sampleID);
    float4 specularData = gBufferSpecular.read(uint2(input.position.xy), sampleID);
    float4 simulation = gBufferSimulation.read(uint2(input.position.xy), sampleID);
    
#if USE_AMBIENT_OCCLUSION
    float ao = aoTex.read(uint2(input.position.xy)).x;
#else
    float ao = 1.0f;
#endif
	
    float3 normal = normalData.xyz * 2.0f - 1.0f;
    float4 position = uniforms.transform[VIEW_CAMERA].invVP * float4(input.screenPos, depth, 1.0f);
    float3 posWS = position.xyz / position.w;
    
	bool isTwoSided = colorData.a > 0.5f ? true : false;
	bool isBackFace = false;
	
	float3 ViewVec = normalize(uniforms.camPos.xyz - posWS);
	
	//if it is backface
	//this should be < 0 but our mesh's edge normals are smoothed, badly
	
	if(isTwoSided && dot(normal, ViewVec) < 0.0)
	{
		//flip normal
		normal = -normal;
		isBackFace = true;
	}
	
	float3 HalfVec = normalize(ViewVec - uniforms.lightDir.xyz);
	float3 ReflectVec = reflect(-ViewVec, normal);
	float NoV = saturate(dot(normal, ViewVec));
	
	float NoL = dot(normal, -uniforms.lightDir.xyz);
	
	// Deal with two faced materials
	NoL = (isTwoSided ? abs(NoL) : saturate(NoL));
	
	float4 posLS = uniforms.transform[VIEW_SHADOW].vp * float4(posWS, 1.0) + simulation;
	
	float3 DiffuseColor = colorData.xyz;
	
	float shadowFactor = 1.0f;
	
	float fLightingMode = saturate(float(RootConstantDrawScene.lightingMode));
	
	float Roughness = clamp(specularData.a, 0.05f, 0.99f);
	float Metallic = specularData.b;
	
	float3 shadedColor = calculateIllumination(
											   normal,
											   ViewVec,
											   HalfVec,
											   ReflectVec,
											   NoL,
											   NoV,
											   uniforms.camPos.xyz,
											   uniforms.esmControl,
											   uniforms.lightDir.xyz,
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
	float3 ambient = colorData.xyz * ambientIntencity;
	
	float3 FinalColor = shadedColor + ambient;
	
	return float4(FinalColor, 1.0);
}


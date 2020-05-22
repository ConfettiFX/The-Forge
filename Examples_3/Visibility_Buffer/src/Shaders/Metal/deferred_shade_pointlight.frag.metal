/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#include <metal_stdlib>
using namespace metal;

#include "shader_defs.h"
#include "shading.h"
#include "deferred_shade_pointlight.h"

struct VSInput
{
    float4 position [[attribute(0)]];
};

struct VSOutput
{
    float4 position [[position]];
    float3 color;
    float3 lightPos;
};

fragment float4 stageMain(VSOutput input                                        [[stage_in]],
                          uint32_t sampleID                                     [[sample_id]],
constant ArgData& fsData [[buffer(UPDATE_FREQ_NONE)]],
constant ArgDataPerFrame& fsDataPerFrame [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
	float4 albedoData = fsData.gBufferColor.read(uint2(input.position.xy), sampleID);
    float4 normalData = fsData.gBufferNormal.read(uint2(input.position.xy), sampleID);
    if (normalData.x == 0 && normalData.y == 0 && normalData.z == 0) return float4(0);

    float depth = fsData.gBufferDepth.read(uint2(input.position.xy), sampleID);
    float4 specularData = fsData.gBufferSpecular.read(uint2(input.position.xy), sampleID);
    float4 simulation = fsData.gBufferSimulation.read(uint2(input.position.xy), sampleID);
	
    float3 normal = normalData.xyz * 2.0 - 1.0;
    float2 screenPos = ((input.position.xy / fsDataPerFrame.uniforms.cullingViewports[VIEW_CAMERA].windowSize) * 2.0 - 1.0);
    screenPos.y = -screenPos.y;
    float4 position = fsDataPerFrame.uniforms.transform[VIEW_CAMERA].invVP * float4(screenPos,depth,1) + simulation;
    float3 posWS = position.xyz / position.w;
	
	bool isTwoSided = (albedoData.a > 0.5);
	bool isBackFace = false;
	
	float3 ViewVec = normalize(fsDataPerFrame.uniforms.camPos.xyz - posWS.xyz);
	
	//if it is backface
	//this should be < 0 but our mesh's edge normals are smoothed, badly
	
	if(isTwoSided && dot(normal, ViewVec) < 0.0)
	{
		//flip normal
		normal = -normal;
		isBackFace = true;
	}
	
	float3 HalfVec = normalize(ViewVec - fsDataPerFrame.uniforms.lightDir.xyz);
	float3 ReflectVec = reflect(-ViewVec, normal);
	float NoV = saturate(dot(normal, ViewVec));
	
	float NoL = dot(normal, -fsDataPerFrame.uniforms.lightDir.xyz);
	
	// Deal with two faced materials
	NoL = (isTwoSided ? abs(NoL) : saturate(NoL));
	
	float3 F0 = specularData.xyz;
	float3 DiffuseColor = albedoData.xyz;
	
	float Roughness = specularData.w;
	float Metallic = normalData.w;
	
//	float fLightingMode = clamp(float(RootConstantDrawScene.lightingMode), 0.0, 1.0);
    float fLightingMode = clamp(float(fsDataPerFrame.uniforms.lightingMode), 0.0, 1.0);
    
	float3 color = pointLightShade(
								   normal,
								   ViewVec,
								   HalfVec,
								   ReflectVec,
								   NoL,
								   NoV,
								   input.lightPos,
								   input.color,
								   float4(fsDataPerFrame.uniforms.camPos).xyz + simulation.xyz,
								   float4(fsDataPerFrame.uniforms.lightDir).xyz,
								   position,
								   posWS,
								   DiffuseColor,
								   F0,
								   Roughness,
								   Metallic,
								   isBackFace,
								   fLightingMode);
	
	return float4(color, 1.0);
}

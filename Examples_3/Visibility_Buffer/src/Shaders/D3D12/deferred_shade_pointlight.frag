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

#include "shading.h"

struct VSOutput
{
    float4 position : SV_Position;
    float3 color : TEXCOORD0;
    float3 lightPos : TEXCOORD1;
};

ConstantBuffer<PerFrameConstants> uniforms : register(b0);

cbuffer RootConstantDrawScene : register(b1)
{
	float4 lightColor;
	uint lightingMode;
	uint outputMode;
	float4 CameraPlane; //x : near, y : far
	
};

StructuredBuffer<LightData> lights : register(t1);
#if (SAMPLE_COUNT > 1)
Texture2DMS<float4, SAMPLE_COUNT> gBufferColor : register(t2);
Texture2DMS<float4, SAMPLE_COUNT> gBufferNormal : register(t3);
Texture2DMS<float4, SAMPLE_COUNT> gBufferDepth : register(t4);
Texture2DMS<float4, SAMPLE_COUNT> gBufferSpecular : register(t5);
Texture2DMS<float4, SAMPLE_COUNT> gBufferSimulation : register(t6);
#else
Texture2D gBufferColor : register(t2);
Texture2D gBufferNormal : register(t3);
Texture2D gBufferDepth : register(t4);
Texture2D gBufferSpecular : register(t5);
Texture2D gBufferSimulation : register(t6);
#endif

float4 main(VSOutput input, uint i : SV_SampleIndex) : SV_Target
{
#if (SAMPLE_COUNT > 1)
    float4 normalData = gBufferNormal.Load(uint2(input.position.xy), i);
#else
	float4 normalData = gBufferNormal.Load(uint3(input.position.xy, 0));
#endif

#if (SAMPLE_COUNT > 1)
    float4 albedoData = gBufferColor.Load(uint2(input.position.xy), i);
#else
	float4 albedoData = gBufferColor.Load(uint3(input.position.xy, 0));
#endif

	if (normalData.x == 0 && normalData.y == 0 && normalData.z == 0)
	{
		discard;
	}

#if (SAMPLE_COUNT > 1)
	float depth = gBufferDepth.Load(uint2(input.position.xy), i).r;
	float4 specularData = gBufferSpecular.Load(uint2(input.position.xy), i);
	float4 simulation = gBufferSimulation.Load(uint2(input.position.xy), i);
#else
    float depth = gBufferDepth.Load(uint3(input.position.xy, 0)).r;
    float4 specularData = gBufferSpecular.Load(uint3(input.position.xy, 0));
	float4 simulation = gBufferSimulation.Load(uint3(input.position.xy, 0));
#endif

	float fLightingMode = saturate(float(lightingMode));
    
    float2 screenPos = ((input.position.xy / uniforms.cullingViewports[VIEW_CAMERA].windowSize) * 2.0 - 1.0);
    screenPos.y = -screenPos.y;
    float3 normal = normalize(normalData.xyz*2.0 - 1.0);
    float4 position = mul(uniforms.transform[VIEW_CAMERA].invVP, float4(screenPos,depth,1));
    float3 posWS = position.xyz/position.w;

    bool isTwoSided = (albedoData.a > 0.5);
	bool isBackFace = false;

	float3 ViewVec = normalize(uniforms.camPos.xyz - posWS.xyz);
	
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

	float3 F0 = specularData.xyz;
	float3 DiffuseColor = albedoData.xyz;

	float Roughness = specularData.w;
	float Metallic = normalData.w;

	float3 color = pointLightShade(
	normal,
	ViewVec,
	HalfVec,
	ReflectVec,
	NoL,
	NoV,
	input.lightPos,
	input.color,
	uniforms.camPos.xyz + simulation,
	uniforms.lightDir.xyz,
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

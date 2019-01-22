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
#include "shader_defs.h"

// This shader loads gBuffer data and shades the pixel.

struct VSOutput
{
    float4 position : SV_Position;
    float2 screenPos : TEXCOORD0;
    uint triangleId : TEXCOORD1;
};

#if(SAMPLE_COUNT > 1)
Texture2DMS<float4, SAMPLE_COUNT> gBufferColor : register(t0);
Texture2DMS<float4, SAMPLE_COUNT> gBufferNormal : register(t1);
Texture2DMS<float4, SAMPLE_COUNT> gBufferDepth : register(t2);
Texture2DMS<float4, SAMPLE_COUNT> gBufferSpecular : register(t3);
Texture2DMS<float4, SAMPLE_COUNT> gBufferSimulation : register(t4);
#else
Texture2D gBufferColor : register(t0);
Texture2D gBufferNormal : register(t1);
Texture2D gBufferDepth : register(t2);
Texture2D gBufferSpecular : register(t3);
Texture2D gBufferSimulation : register(t4);
#endif

#if USE_AMBIENT_OCCLUSION
Texture2D<float> aoTex : register(t100);
#endif

Texture2D shadowMap : register(t101);

SamplerState depthSampler : register(s0);
ConstantBuffer<PerFrameConstants> uniforms : register(b0);

cbuffer RootConstantDrawScene : register(b1)
{
	float4 lightColor;
	uint lightingMode;
	uint outputMode;
	float4 CameraPlane; //x : near, y : far
	
};

// Pixel shader
float4 main(VSOutput input, uint i : SV_SampleIndex) : SV_Target
{
    // Load gBuffer data from render target
#if(SAMPLE_COUNT > 1)
	float depth = gBufferDepth.Load(uint2(input.position.xy), i).r;
#else
	float depth = gBufferDepth.Load(uint3(input.position.xy, 0)).r;
#endif
	// Since we don't have a skydome, this code is added here to avoid shading the sky.
  	if (depth >= 0.999f)
    {
		discard;
    }
#if(SAMPLE_COUNT > 1)
	float4 colorData = gBufferColor.Load(uint2(input.position.xy), i);
	float4 specularData = gBufferSpecular.Load(uint2(input.position.xy), i);
	float4 simulation = gBufferSimulation.Load(uint2(input.position.xy), i);
    float4 normalData = gBufferNormal.Load(uint2(input.position.xy), i);
#else
	float4 colorData = gBufferColor.Load(uint3(input.position.xy, 0));
	float4 specularData = gBufferSpecular.Load(uint3(input.position.xy, 0));
	float4 simulation = gBufferSimulation.Load(uint3(input.position.xy, 0));
    float4 normalData = gBufferNormal.Load(uint3(input.position.xy, 0));
#endif

#if USE_AMBIENT_OCCLUSION
	float ao = aoTex.Load(uint3(input.position.xy, 0));
#else
	float ao = 1.0f;
#endif
	
	float3 normal = normalize(normalData.xyz * 2.0f - 1.0f);

	float4 position = mul(uniforms.transform[VIEW_CAMERA].invVP, float4(input.screenPos, depth, 1));
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

	float4 posLS = mul(uniforms.transform[VIEW_SHADOW].vp, float4(posWS, 1)) + simulation;
		
	float3 DiffuseColor = colorData.xyz;
	
	float shadowFactor = 1.0f;

	float fLightingMode = saturate(float(lightingMode));

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


	shadedColor = shadedColor * lightColor.rgb * lightColor.a * NoL * ao;

	float ambientIntencity = 0.2f;
	float3 ambient = colorData.xyz * ambientIntencity;

	float3 FinalColor = shadedColor + ambient;
		
	return float4(FinalColor, 1.0);
}

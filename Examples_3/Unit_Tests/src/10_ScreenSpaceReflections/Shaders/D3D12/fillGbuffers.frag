/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

cbuffer cbCamera : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4x4 projView;
	float4x4 prevProjView;
	float3 camPos;
}

cbuffer cbObject : register(b1, UPDATE_FREQ_PER_DRAW)
{
	float4x4 worldMat;
	float4x4 prevWorldMat;
	float roughness;
	float metalness;
	int pbrMaterials;
}

cbuffer cbTextureRootConstants : register(b2)
{
	uint textureMapIds;
}

#define albedoMap     ((textureMapIds >> 0) & 0xFF)
#define normalMap     ((textureMapIds >> 8) & 0xFF)
#define metallicMap   ((textureMapIds >> 16) & 0xFF)
#define roughnessMap  ((textureMapIds >> 24) & 0xFF)
#define aoMap         (5)

SamplerState defaultSampler : register(s2);

// material parameters
Texture2D textureMaps[] : register(t3, space10);

float3 reconstructNormal(in float4 sampleNormal)
{
	float3 tangentNormal;
	tangentNormal.xy = sampleNormal.rg * 2 - 1;
	tangentNormal.z = sqrt(1 - saturate(dot(tangentNormal.xy, tangentNormal.xy)));
	return tangentNormal;
}

float3 getNormalFromMap(float3 pos, float3 normal, float2 uv)
{
	float3 tangentNormal = reconstructNormal(textureMaps[normalMap].Sample(defaultSampler, uv));

    float3 Q1  = ddx(pos);
    float3 Q2  = ddy(pos);
    float2 st1 = ddx(uv);
    float2 st2 = ddy(uv);

    float3 N   = normalize(normal);
    float3 T  = normalize(Q1*st2.y - Q2*st1.y);
    float3 B  = -normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);

    float3 res = mul(tangentNormal,TBN);
    return res;
}

struct PsIn
{
    float4 position : SV_Position;
    
    float3 normal : TEXCOORD0;
	float3 pos	  : TEXCOORD1;
	float2 uv : TEXCOORD2;
	float4 curPosition: TEXCOORD3;
	float4 prevPosition: TEXTCOORD4;
};

struct PSOut
{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
    float4 specular : SV_Target2;
    float2 motion : SV_Target3;
};


PSOut main(PsIn input) : SV_TARGET
{	
	PSOut Out;

	//cut off
	float alpha = textureMaps[albedoMap].Sample(defaultSampler,input.uv).a;

	if(alpha < 0.5)
		discard;

	// default albedo 
	float3 albedo = float3(0.5f, 0.0f, 0.0f);

	float _roughness = roughness;
	float _metalness = metalness;
	float ao = 1.0f;

	float3 N = normalize(input.normal);
	

	//this means pbr materials is set for these so sample from textures
	if(pbrMaterials!=-1) {

		 N = getNormalFromMap(input.pos, input.normal, input.uv);
		albedo = pow(textureMaps[albedoMap].Sample(defaultSampler,input.uv).rgb, float3(2.2, 2.2, 2.2));
		_metalness   = textureMaps[metallicMap].Sample(defaultSampler,input.uv).r;
		_roughness = textureMaps[roughnessMap].Sample(defaultSampler,input.uv).r;
		ao = textureMaps[aoMap].SampleLevel(defaultSampler,input.uv, 0).r;
	} 

	if(pbrMaterials==2) {
		
		//N = normalize(normal);
		albedo  = float3(0.7f, 0.7f, 0.7f);
		_roughness = roughness;
		_metalness = metalness;
		ao = 1.0f;
	}
	if(roughnessMap == 6)
	{
		_roughness = 0.05f;
	}

	Out.albedo = float4(albedo, alpha);
	Out.normal = float4(N, _metalness);
	Out.specular = float4(_roughness, ao, input.uv);
	Out.motion  = input.curPosition.xy / input.curPosition.w - input.prevPosition.xy / input.prevPosition.w;

	return Out;
}
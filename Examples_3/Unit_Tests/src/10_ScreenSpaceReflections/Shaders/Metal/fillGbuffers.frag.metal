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

#include <metal_stdlib>
using namespace metal;

struct CameraData
{
	float4x4 projView;
	float4x4 prevProjView;
	float3 camPos;
};

struct ObjectData
{
	float4x4 worldMat;
	float4x4 prevWorldMat;
	float roughness;
	float metalness;
	int pbrMaterials;
};

struct TextureIndices {
	uint textureMapIds;
};

#define albedoMap     ((cbTextureRootConstants.textureMapIds >> 0) & 0xFF)
#define normalMap     ((cbTextureRootConstants.textureMapIds >> 8) & 0xFF)
#define metallicMap   ((cbTextureRootConstants.textureMapIds >> 16) & 0xFF)
#define roughnessMap  ((cbTextureRootConstants.textureMapIds >> 24) & 0xFF)
#define aoMap         (5)

float3 reconstructNormal(float4 sampleNormal)
{
	float3 tangentNormal;

#ifdef TARGET_IOS
	// ASTC texture stores normal map components in red and alpha channels
	// Once MTLTextureSwizzle becomes part of final API, we can do this in the renderer side so the shader can treat ASTC normal_psnr like a BC5 texture
	tangentNormal.xy = (sampleNormal.ra * 2 - 1);
#else
	tangentNormal.xy = (sampleNormal.rg * 2 - 1);
#endif
	tangentNormal.z = sqrt(1 - saturate(dot(tangentNormal.xy, tangentNormal.xy)));
	return tangentNormal;
}

float3 getNormalFromMap( texture2d<float> normalMapTex, sampler defaultSampler, float2 uv, float3 pos, float3 normal)
{
    float3 tangentNormal = reconstructNormal(normalMapTex.sample(defaultSampler,uv));

    float3 Q1 = dfdx(pos);
    float3 Q2 = dfdy(pos);
    float2 st1 = dfdx(uv);
    float2 st2 = dfdy(uv);


    float3 N = normalize(normal);
    float3 T = normalize(Q1*st2.g - Q2*st1.g);
    float3 B = -normalize(cross(N,T));
    float3x3 TBN = float3x3(T,B,N);
    float3 res = TBN * tangentNormal ;
    return res;
};

struct PsIn
{
    float4 position [[position]];
    
    float3 normal;
	float3 pos;
	float2 uv;
	float4 curPosition;
	float4 prevPosition;
};

struct PSOut
{
    float4 albedo   [[color(0)]];
    float4 normal   [[color(1)]];
    float4 specular [[color(2)]];
    float2 motion   [[color(3)]];
};

struct FSData
{
    sampler defaultSampler                          [[id(0)]];
    const array<texture2d<float>, 84> textureMaps;
};

struct FSDataPerFrame
{
    constant CameraData& cbCamera                   [[id(0)]];
};

struct FSDataPerDraw
{
    constant ObjectData& cbObject                   [[id(0)]];
};

fragment PSOut stageMain(
    PsIn In                                             [[stage_in]],
    constant FSData& fsData                             [[buffer(UPDATE_FREQ_NONE)]],
    constant FSDataPerFrame& fsDataPerFrame             [[buffer(UPDATE_FREQ_PER_FRAME)]],
    constant FSDataPerDraw& fsDataPerDraw               [[buffer(UPDATE_FREQ_PER_DRAW)]],
    constant TextureIndices& cbTextureRootConstants     [[buffer(UPDATE_FREQ_USER)]]
)
{
	PSOut Out;

	//cut off	
	float alpha = fsData.textureMaps[albedoMap].sample(fsData.defaultSampler, In.uv, 0.0).a;

	if(alpha <= 0.5)
		discard_fragment();

	// default albedo 
	float3 albedo = float3(0.5f, 0.0f, 0.0f);

	float _roughness = fsDataPerDraw.cbObject.roughness;
	float _metalness = fsDataPerDraw.cbObject.metalness;
	float ao = 1.0f;

	float3 N = normalize(In.normal);
	

	//this means pbr materials is set for these so sample from textures
	if(fsDataPerDraw.cbObject.pbrMaterials!=-1) {
        N = getNormalFromMap(fsData.textureMaps[normalMap], fsData.defaultSampler, In.uv, In.pos, N);
        float3 val = fsData.textureMaps[albedoMap].sample(fsData.defaultSampler, In.uv,0.0).rgb;
        albedo = float3(pow(val.x,2.2f),pow(val.y,2.2f),pow(val.z,2.2f));
        _metalness   = fsData.textureMaps[metallicMap].sample(fsData.defaultSampler, In.uv).r;
        _roughness = fsData.textureMaps[roughnessMap].sample(fsData.defaultSampler, In.uv).r;
        ao =   fsData.textureMaps[aoMap].sample(fsData.defaultSampler, In.uv).r ;
	}

	if(fsDataPerDraw.cbObject.pbrMaterials==2) {
		albedo  = float3(0.7f, 0.7f, 0.7f);
		ao = 1.0f;
	}
	if(roughnessMap == 6)
	{
		_roughness = 0.05f;
	}

	Out.albedo = float4(albedo, alpha);
	Out.normal = float4(N, _metalness);
	Out.specular = float4(_roughness, ao, In.uv);
	Out.motion = In.curPosition.xy / In.curPosition.w - In.prevPosition.xy / In.prevPosition.w;

	return Out;
}

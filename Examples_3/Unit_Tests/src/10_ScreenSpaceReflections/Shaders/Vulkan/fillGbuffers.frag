#version 450 core

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

layout (std140, UPDATE_FREQ_PER_FRAME, binding=0) uniform cbCamera
{
	uniform mat4 projView;
	uniform mat4 prevProjView;
	uniform vec3 camPos;
};

layout (std140, UPDATE_FREQ_PER_DRAW, binding=0) uniform cbObject
{
	uniform mat4 worldMat;
	uniform mat4 prevWorldMat;
	float roughness;
	float metalness;
	int pbrMaterials;
};

layout (push_constant) uniform cbTextureRootConstantsData
{
	uint textureMapIds;
} cbTextureRootConstants;

#define albedoMap     ((cbTextureRootConstants.textureMapIds >> 0) & 0xFF)
#define normalMap     ((cbTextureRootConstants.textureMapIds >> 8) & 0xFF)
#define metallicMap   ((cbTextureRootConstants.textureMapIds >> 16) & 0xFF)
#define roughnessMap  ((cbTextureRootConstants.textureMapIds >> 24) & 0xFF)
#define aoMap         (5)

layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 pos;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 curPosition;
layout(location = 4) in vec4 prevPosition;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal; // a : metallic
layout(location = 2) out vec4 outRoughness;
layout(location = 3) out vec2 outMotion;


// material parameters
layout(UPDATE_FREQ_NONE, binding = 6) uniform texture2D textureMaps[TOTAL_IMGS];
layout(UPDATE_FREQ_NONE, binding = 7) uniform sampler defaultSampler;

vec3 reconstructNormal(in vec4 sampleNormal)
{
	vec3 tangentNormal;
	tangentNormal.xy = sampleNormal.rg * 2 - 1;
	tangentNormal.z = sqrt(1.0f - clamp(dot(tangentNormal.xy, tangentNormal.xy), 0.0f, 1.0f));
	return tangentNormal;
}

vec3 getNormalFromMap()
{
    vec3 tangentNormal = reconstructNormal(texture(sampler2D(textureMaps[normalMap], defaultSampler),uv));

    vec3 Q1  = dFdx(pos);
    vec3 Q2  = dFdy(pos);
    vec2 st1 = dFdx(uv);
    vec2 st2 = dFdy(uv);

    vec3 N   = normalize(normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main()
{	
	//cut off
	float alpha = texture(sampler2D(textureMaps[albedoMap], defaultSampler),uv).a;

	if(alpha < 0.5)
		discard;

	// default albedo 
	vec3 albedo = vec3(0.5f, 0.0f, 0.0f);

	float _roughness = roughness;
	float _metalness = metalness;
	float ao = 1.0f;

	vec3 N = normalize(normal);
	

	//this means pbr materials is set for these so sample from textures
	if(pbrMaterials!=-1) {

		 N = getNormalFromMap();
		albedo = pow(texture(sampler2D(textureMaps[albedoMap], defaultSampler),uv).rgb,vec3(2.2)) ;
		_metalness   = texture(sampler2D(textureMaps[metallicMap], defaultSampler), uv).r;
		_roughness = texture(sampler2D(textureMaps[roughnessMap], defaultSampler), uv).r;
		ao = texture(sampler2D(textureMaps[aoMap], defaultSampler), uv).r;
	} 

	if(pbrMaterials==2) {
		
		//N = normalize(normal);
		albedo  = vec3(0.7f, 0.7f, 0.7f);
		_roughness = roughness;
		_metalness = metalness;
		ao = 1.0f;
	}
	if(roughnessMap==6)
	{
		_roughness = 0.05f;
	}
	
	outColor = vec4(albedo, alpha);
	outNormal = vec4(N, _metalness);
	outRoughness = vec4(_roughness, 1.0, uv);
	outMotion  = curPosition.xy / curPosition.w - prevPosition.xy / prevPosition.w;
}
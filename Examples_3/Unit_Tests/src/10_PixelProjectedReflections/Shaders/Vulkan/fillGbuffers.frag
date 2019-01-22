#version 450 core

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

layout (std140, set=0, binding=0) uniform cbCamera {
	uniform mat4 projView;
	uniform vec3 camPos;
};

layout (std140, set=1, binding=0) uniform cbObject {
	uniform mat4 worldMat;
	float roughness;
	float metalness;
	int pbrMaterials;
};

layout (push_constant) uniform cbTextureRootConstantsData
{
	uint albedoMap;
	uint normalMap;
	uint metallicMap;
	uint roughnessMap;
	uint aoMap;
} cbTextureRootConstants;

layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 pos;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal; // a : metallic
layout(location = 2) out vec4 outRoughness;


// material parameters
layout(set = 0, binding = 6) uniform texture2D textureMaps[TOTAL_IMGS];
layout(set = 0, binding = 7) uniform sampler defaultSampler;

vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(sampler2D(textureMaps[cbTextureRootConstants.normalMap], defaultSampler),uv).xyz * 2.0 - 1.0;

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
	float alpha = texture(sampler2D(textureMaps[cbTextureRootConstants.albedoMap], defaultSampler),uv).a;

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
		albedo = pow(texture(sampler2D(textureMaps[cbTextureRootConstants.albedoMap], defaultSampler),uv).rgb,vec3(2.2)) ;
		_metalness   = texture(sampler2D(textureMaps[cbTextureRootConstants.metallicMap], defaultSampler), uv).r;
		_roughness = texture(sampler2D(textureMaps[cbTextureRootConstants.roughnessMap], defaultSampler), uv).r;
		ao = texture(sampler2D(textureMaps[cbTextureRootConstants.aoMap], defaultSampler), uv).r;
	} 

	if(pbrMaterials==2) {
		
		//N = normalize(normal);
		albedo  = vec3(0.7f, 0.7f, 0.7f);
		_roughness = roughness;
		_metalness = metalness;
		ao = 1.0f;
	}

	
	outColor = vec4(albedo, alpha);
	outNormal = vec4(N, _metalness);
	outRoughness = vec4(_roughness, 1.0, uv);
}
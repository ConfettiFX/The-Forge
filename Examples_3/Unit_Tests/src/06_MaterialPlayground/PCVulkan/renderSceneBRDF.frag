#version 450 core

/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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


struct Light
{
	vec4 pos;
	vec4 col;
	float radius;
	float intensity;
	float _pad0;
	float _pad1;
};

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

layout (std140, set=2, binding=0) uniform cbLights {
	Light lights[16];
	int currAmountOfLights;
};

const float PI = 3.14159265359;


vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0f - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness*roughness;
	float a2 = a*a;
	float NdotH = max(dot(N,H), 0.0);
	float NdotH2 = NdotH*NdotH;
	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0f);
	float k = (r*r) / 8.0f;
	
	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom/denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}


layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 pos;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 outColor;


layout(set = 0, binding = 3) uniform texture2D brdfIntegrationMap;
layout(set = 0, binding = 4) uniform textureCube irradianceMap;
layout(set = 0, binding = 5) uniform textureCube specularMap;
layout(set = 0, binding = 6) uniform sampler envSampler;

// material parameters
layout(set = 0, binding = 7)  uniform texture2D albedoMap;
layout(set = 0, binding = 8)  uniform texture2D normalMap;
layout(set = 0, binding = 9)  uniform texture2D metallicMap;
layout(set = 0, binding = 10) uniform texture2D roughnessMap;
layout(set = 0, binding = 11) uniform texture2D aoMap;

layout(set = 0, binding = 12) uniform sampler defaultSampler;



vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(sampler2D(normalMap, defaultSampler),uv).xyz * 2.0 - 1.0;

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
	// default albedo 
	vec3 albedo = vec3(0.5f, 0.0f, 0.0f);

	float _roughness = roughness;
	float _metalness = metalness;
	float ao = 1.0f;

	vec3 N = normalize(normal);

	//this means pbr materials is set for these so sample from textures
	if(pbrMaterials!=-1) {

		 N = getNormalFromMap();
		albedo = pow(texture(sampler2D(albedoMap, defaultSampler),uv).rgb,vec3(2.2)) ;
		_metalness   = texture(sampler2D(metallicMap, defaultSampler), uv).r;
		_roughness = texture(sampler2D(roughnessMap, defaultSampler), uv).r;
		ao = texture(sampler2D(aoMap, defaultSampler), uv).r;
	} 

	if(pbrMaterials==2) {
		
		//N = normalize(normal);
		albedo  = vec3(0.7f, 0.7f, 0.7f);
		//_roughness = roughness;
		//_metalness = metalness;
		ao = 1.0f;//texture(sampler2D(aoMap, defaultSampler), uv).r;
	}

	vec3 V = normalize(camPos - pos);
	vec3 R = reflect(-V, N);
	
	// 0.04 is the index of refraction for metal
	vec3 F0 = vec3(0.04f);
	F0 = mix(F0, albedo, _metalness);

	vec3 Lo = vec3(0.0);
	for(int i = 0; i < currAmountOfLights; ++i)
	{
		 // Vec from world pos to light pos
		vec3 L =  normalize(vec3(lights[i].pos) - pos);

		// halfway vec
		vec3 H = normalize(V + L);

		// get distance
		float distance = length(vec3(lights[i].pos) - pos);

		// Distance attenuation from Epic Games' paper 
		float distanceByRadius = 1.0f - pow((distance / lights[i].radius), 4);
		float clamped = pow(clamp(distanceByRadius, 0.0f, 1.0f), 2.0f);
		float attenuation = clamped/(distance * distance + 1.0f);

		//float attenuation = 1.0f;
		// Radiance is color mul with attenuation mul intensity 
		vec3 radiance = vec3(lights[i].col) * attenuation * lights[i].intensity;

		float NDF = distributionGGX(N, H, _roughness);
		float G = GeometrySmith(N, V, L, _roughness);
		vec3 F = fresnelSchlick(max(dot(N,H), 0.0), F0);
		
		vec3 nominator = NDF * G * F;
		float denominator = 4.0f * max(dot(N,V), 0.0) * max(dot(N, L), 0.0) + 0.001;
		vec3 specular = nominator / denominator;

		vec3 kS = F;

		vec3 kD = vec3(1.0) - kS;

		kD *= 1.0f - _metalness;

		float NdotL = max(dot(N, L), 0.0);

		if(NdotL>0.0f) {

			Lo +=  (kD * albedo / PI + specular) * radiance * NdotL;
		}else {


			Lo+= vec3(0.0f);
		}
		
	}

	vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, _roughness);
	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - _metalness;

	vec3 irradiance = texture(samplerCube(irradianceMap, envSampler), N).rgb;
	vec3 diffuse = kD * irradiance * albedo;

	vec3 specularColor = textureLod(samplerCube(specularMap, envSampler), R, _roughness * 4).rgb;
	vec2 brdf = texture(sampler2D(brdfIntegrationMap, defaultSampler), vec2(max(dot(N,V), _roughness))).rg;
	vec3 specular = specularColor * (F * brdf.x + brdf.y);
	vec3 ambient = vec3(diffuse + specular) * vec3(ao);
	
	vec3 color = Lo + ambient;
	
	// Tonemap and gamma correct
	color = color/(color+vec3(1.0));
	color = pow(color, vec3(1.0/2.2));

	outColor = vec4(color,1.0f);
}

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

#define PI 3.1415926535897932384626422832795028841971
#define TwoPi 6.28318530717958647692
#define InvPi 0.31830988618379067154
#define Inv2Pi 0.15915494309189533577
#define Inv4Pi 0.07957747154594766788

struct Light
{
	vec4 pos;
	vec4 col;
	float radius;
	float intensity;
};

layout (std140, set=0, binding=0) uniform cbCamera {
	uniform mat4 projView;
	uniform vec3 camPos;
	uniform float pad_0;
};

layout (std140, set=1, binding=0) uniform cbObject {
	uniform mat4 worldMat;
	uniform mat4 invWorldMat;

	uniform vec4 u_OceanColor;
	uniform vec4 u_ShorelineColor;
	uniform vec4 u_FoliageColor;
	uniform vec4 u_MountainsColor;

	uniform vec4 u_SnowColor;
	uniform vec4 u_PolarCapsColor;
	uniform vec4 u_AtmosphereColor;
	uniform vec4 u_HeightsInfo; // x : Ocean, y : Shore, z : Snow, w : Polar

	uniform vec4 u_TimeInfo; //time, controls.Noise4D, controls.TerrainExp, controls.TerrainSeed * 39.0
};

layout (std140, set=2, binding=0) uniform cbLights {
	int currAmountOfLights;
	int pad0;
	int pad1;
	int pad2;
	Light lights[16];
};

layout (set=3, binding=0) uniform texture2D  uEnvTex0;
layout (set=3, binding=1) uniform sampler   uSampler0;




vec2 LightingFunGGX_FV(float dotLH, float roughness)
{
	float alpha = roughness*roughness;

	//F
	float F_a, F_b;
	float dotLH5 = pow(clamp(1.0f - dotLH, 0.0f, 1.0f), 5.0f);
	F_a = 1.0f;
	F_b = dotLH5;

	//V
	float vis;
	float k = alpha * 0.5f;
	float k2 = k*k;
	float invK2 = 1.0f - k2;
	vis = 1.0f/(dotLH*dotLH*invK2 + k2);

	return vec2((F_a - F_b)*vis, F_b*vis);
}

float LightingFuncGGX_D(float dotNH, float roughness)
{
	float alpha = roughness*roughness;
	float alphaSqr = alpha*alpha;
	float denom = dotNH * dotNH * (alphaSqr - 1.0f) + 1.0f;

	return alphaSqr / (PI*denom*denom);
}

vec3 GGX_Spec(vec3 Normal, vec3 HalfVec, float Roughness, vec3 BaseColor, vec3 SpecularColor, vec2 paraFV)
{
	float NoH = clamp(dot(Normal, HalfVec), 0.0, 1.0);

	float D = LightingFuncGGX_D(NoH * NoH * NoH * NoH, Roughness);
	vec2 FV_helper = paraFV;

	vec3 F0 = SpecularColor;
	vec3 FV = F0*FV_helper.x + vec3(FV_helper.y, FV_helper.y, FV_helper.y);
	
	return D * FV;
}

float hash(float n)
{    
    //4D
    if(u_TimeInfo.y > 0.0 )
    {
        return fract(sin(n) *cos(u_TimeInfo.x * 0.00001) * 1e4);
    }
    else
    {
        return fract(sin(n) * cos( u_TimeInfo.w * 0.00001) * 1e4);
    }
   
}

float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }
float noise(float x) { float i = floor(x); float f = fract(x); float u = f * f * (3.0 - 2.0 * f); return mix(hash(i), hash(i + 1.0), u); }

float noise(vec3 x)
{   
    vec3 step = vec3(110, 241, 171);
    vec3 i = floor(x); 
    vec3 f = fract(x);
    float n = dot(i, step);
    vec3 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix( hash(n + dot(step, vec3(0, 0, 0))), hash(n + dot(step, vec3(1, 0, 0))), u.x),
     mix( hash(n + dot(step, vec3(0, 1, 0))), hash(n + dot(step, vec3(1, 1, 0))), u.x), u.y),
     mix(mix( hash(n + dot(step, vec3(0, 0, 1))), hash(n + dot(step, vec3(1, 0, 1))), u.x),
     mix( hash(n + dot(step, vec3(0, 1, 1))), hash(n + dot(step, vec3(1, 1, 1))), u.x), u.y), u.z);
}

#define Epsilon 0.0001

float fbm(vec3 x, int LOD)
{
  float v = 0.0;
  float a = 0.5;
  vec3 shift = vec3(100.0);

  for (int i = 0; i < LOD; ++i)
  {
   v += a * noise(x);
   x = x * 2.0 + shift;
   a *= 0.5;
  }  
  return v;
}

float SphericalTheta(vec3 v)
{
	return acos(clamp(v.y, -1.0f, 1.0f));
}

float SphericalPhi(vec3 v)
{
	float p = atan(v.z , v.x);
	return (p < 0.0f) ? (p + TwoPi) : p;
}

float OceanNoise(vec3 vertexPos, float oceneHeight, float noiseResult, float blendFactor)
{
    float relativeWaterDepth = min(1.0, (oceneHeight - noiseResult) * 15.0);

    float oceanTime = u_TimeInfo.x * 0.03;

    float shallowWaveRefraction = 4.0;
    float waveMagnitude = 0.0002;
    float waveLength = mix(0.007, 0.0064, blendFactor);

    float shallowWavePhase = (vertexPos.y - noiseResult * shallowWaveRefraction) * (1.0 / waveLength);
    float deepWavePhase    = (atan(vertexPos.z, vertexPos.x) + noise(vertexPos.xyz * 15.0) * 0.075) * (1.5 / waveLength);
    return (cos(shallowWavePhase + oceanTime  * 1.5) * sqrt(1.0 - relativeWaterDepth) + cos(deepWavePhase + oceanTime  * 2.0) * 2.5 * (1.0 - abs(vertexPos.y)) * (relativeWaterDepth * relativeWaterDepth)) * waveMagnitude;
}

layout(location = 0) in vec4 fs_Pos;
layout(location = 1) in vec4 fs_Nor;            // The array of normals that has been transformed by u_ModelInvTr. This is implicitly passed to the fragment shader.
layout(location = 2) in vec4 fs_Col;            // The color of each vertex. This is implicitly passed to the fragment shader.
layout(location = 3) in vec4 fs_TerrainInfo;    // x: oceanFlag  , y:   , z:,   w: roughness 
layout(location = 4) in vec4 fs_transedPos;

layout(location = 0) out vec4 outColor;




void main()
{
	// Material base color (before shading)
    vec3 normalVec = normalize(fs_Nor.xyz);

    vec4 diffuseColor = fs_Col;

    float Roughness = fs_TerrainInfo.w;
	float energyConservation = 1.0f - Roughness;

    vec3 specularTerm = vec3(0.0);
    vec3 SpecularColor = vec3(1.0, 1.0, 1.0);

    vec3 localNormal = normalize(fs_Pos.xyz);	

	mat3 invMat = mat3(invWorldMat);

	vec4 fs_ViewVec = vec4(invMat * normalize(camPos - fs_transedPos.xyz) ,  0.0);
	vec4 fs_LightVec = vec4( invMat * normalize(vec3(lights[0].pos) - fs_Pos.xyz), 0.0);

	fs_ViewVec.w = length(camPos.xyz);

    //Terrain-atmosphere Color Interpolation
    float a = 1.0 - clamp(dot(fs_ViewVec.xyz, localNormal), 0.0, 1.0);

     a = pow(a, 5.0);

	 float u_resolution = 4.0;
     float constant = 10.0;
     float sm = (1.0 - smoothstep(0.0, 7.0, log(fs_ViewVec.w)));
     int LOD = int(constant * pow(sm, 1.7));	

	 float noise = fbm(fs_Pos.xyz*u_resolution, LOD) * 2.0;                  
     noise = pow(noise, u_TimeInfo.z);
	

    //terrain
    if(fs_TerrainInfo.x > 0.0 && fs_TerrainInfo.x < 0.2 )
    {       
		vec4 vertexPos = fs_Pos;
		vertexPos.xyz += localNormal * noise;	
        //detail normal
        normalVec = normalize(cross( dFdx(vertexPos.xyz), dFdy(vertexPos.xyz)));  
        
        float NolN= clamp(dot(localNormal, normalVec), 0.0, 1.0);
        diffuseColor = mix(u_MountainsColor, diffuseColor, NolN*NolN*NolN);       
    }
    else
    {
		vec4 vertexPos = fs_Pos;		

		float oceneHeight = length(vertexPos.xyz) + u_HeightsInfo.x;

		float height = length(vertexPos.xyz);

		float gap = clamp((1.0 - (oceneHeight - height)), 0.0, 1.0);
		float gap5 = pow(gap, 3.0);

		float gap10 = pow(pow(gap, 100.0), 0.8);

        float wave = OceanNoise(vertexPos.xyz, oceneHeight, noise, gap10);  
        vertexPos.xyz = (oceneHeight + wave) * localNormal;
        

        //detail normal
        normalVec = normalize(cross( dFdx(vertexPos.xyz), dFdy(vertexPos.xyz)));  
    }
   

    float diffuseTerm = clamp(dot(normalVec, normalize(fs_LightVec.xyz)), 0.0, 1.0);

    vec3 halfVec = fs_ViewVec.xyz + fs_LightVec.xyz;
    halfVec = normalize(halfVec);
    float LoH = clamp(dot( fs_LightVec.xyz, halfVec ), 0.0, 1.0);

    specularTerm = GGX_Spec(normalVec, halfVec, Roughness, diffuseColor.xyz, SpecularColor, LightingFunGGX_FV(LoH, Roughness)) *energyConservation;
    

    float ambientTerm = 0.0;

    float lightIntensity = diffuseTerm + ambientTerm; 

    vec3 reflecVec = reflect(-fs_ViewVec.xyz, normalVec);
        
    //Envmap
    vec2 st = vec2(SphericalPhi(reflecVec.xyz) * Inv2Pi, SphericalTheta(reflecVec.xyz) * InvPi);
    vec4 envColor =  texture( sampler2D(uEnvTex0, uSampler0), st) * energyConservation * 0.5;

    // Compute final shaded color
    vec4 planetColor = vec4( ( diffuseColor.rgb + specularTerm + envColor.xyz) * lightIntensity, 1.0);

    outColor = vec4(mix(planetColor.xyz ,u_AtmosphereColor.xyz, a), 1.0);
}
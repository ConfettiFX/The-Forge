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


struct Light
{
	float4 pos;
	float4 col;
	float radius;
	float intensity;
};

static const float PI = 3.14159265359;

cbuffer cbCamera : register(b0) {
	float4x4 projView;
	float3 camPos;
	float pad_0;
}

cbuffer cbObject : register(b1) {
	float4x4 worldMat;
	float4x4 invWorldMat;

	float4 u_OceanColor;
	float4 u_ShorelineColor;
	float4 u_FoliageColor;
	float4 u_MountainsColor;

	float4 u_SnowColor;
	float4 u_PolarCapsColor;
	float4 u_AtmosphereColor;
	float4 u_HeightsInfo; // x : Ocean, y : Shore, z : Snow, w : Polar

	float4 u_TimeInfo; //time, controls.Noise4D, controls.TerrainExp, controls.TerrainSeed * 39.0
}

cbuffer cbLights : register(b2) {
	int currAmountOflights;
	int pad0;
	int pad1;
	int pad2;
	Light lights[16];
}


cbuffer cbScreen : register(b3) {
	float4 u_screenSize;
}

SamplerState uSampler0 : register(s7);
Texture2D uEnvTex0 : register(t1);


struct VSInput
{
    float4 vs_Pos : POSITION;
    float4 vs_Nor : NORMAL;
};

struct VSOutput {
	float4 Position : SV_POSITION;
	float4 fs_Pos : POSITION;
	float4 fs_Nor : NORMAL;

	float4 fs_Col : TEXTURE0;
	float4 fs_TerrainInfo : TEXTURE1;
	float4 fs_transedPos : TEXTURE2;
};

struct VSBGInput
{
	float4 vs_Pos : POSITION;
	float4 vs_Nor : NORMAL;
};

struct VSBGOutput {
	float4 Position : SV_POSITION;
	float2 fs_UV : TEXTURE0;
};



float2 LightingFunGGX_FV(float dotLH, float roughness)
{
	float alpha = roughness * roughness;

	//F
	float F_a, F_b;
	float dotLH5 = pow(clamp(1.0f - dotLH, 0.0f, 1.0f), 5.0f);
	F_a = 1.0f;
	F_b = dotLH5;

	//V
	float vis;
	float k = alpha * 0.5f;
	float k2 = k * k;
	float invK2 = 1.0f - k2;
	vis = 1.0f / (dotLH*dotLH*invK2 + k2);

	return float2((F_a - F_b)*vis, F_b*vis);
}

float LightingFuncGGX_D(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alphaSqr = alpha * alpha;
	float denom = dotNH * dotNH * (alphaSqr - 1.0f) + 1.0f;

	return alphaSqr / (PI*denom*denom);
}

float3 GGX_Spec(float3 Normal, float3 HalfVec, float Roughness, float3 BaseColor, float3 SpecularColor, float2 paraFV)
{
	float NoH = clamp(dot(Normal, HalfVec), 0.0, 1.0);

	float D = LightingFuncGGX_D(NoH * NoH * NoH * NoH, Roughness);
	float2 FV_helper = paraFV;

	float3 F0 = SpecularColor;
	float3 FV = F0 * FV_helper.x + float3(FV_helper.y, FV_helper.y, FV_helper.y);

	return D * FV;
}

float hash(float n)
{
	//4D
	if (u_TimeInfo.y > 0.0)
	{
		return frac(sin(n) *cos(u_TimeInfo.x * 0.00001) * 1e4);
	}
	else
	{
		return frac(sin(n) * cos(u_TimeInfo.w * 0.00001) * 1e4);
	}

}

float hash(float2 p) { return frac(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }
float noise(float x) { float i = floor(x); float f = frac(x); float u = f * f * (3.0 - 2.0 * f); return lerp(hash(i), hash(i + 1.0), u); }
float noise(float2 x) { float2 i = floor(x); float2 f = frac(x); float a = hash(i); float b = hash(i + float2(1.0, 0.0)); float c = hash(i + float2(0.0, 1.0)); float d = hash(i + float2(1.0, 1.0)); float2 u = f * f * (3.0 - 2.0 * f); return lerp(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y; }

float rand(float2 co) {
	return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
}

float noise(float3 x)
{
	float3 step = float3(110, 241, 171);
	float3 i = floor(x);
	float3 f = frac(x);
	float n = dot(i, step);
	float3 u = f * f * (3.0 - 2.0 * f);
	return lerp(lerp(lerp(hash(n + dot(step, float3(0, 0, 0))), hash(n + dot(step, float3(1, 0, 0))), u.x),
		lerp(hash(n + dot(step, float3(0, 1, 0))), hash(n + dot(step, float3(1, 1, 0))), u.x), u.y),
		lerp(lerp(hash(n + dot(step, float3(0, 0, 1))), hash(n + dot(step, float3(1, 0, 1))), u.x),
			lerp(hash(n + dot(step, float3(0, 1, 1))), hash(n + dot(step, float3(1, 1, 1))), u.x), u.y), u.z);
}

#define PI 3.1415926535897932384626422832795028841971
#define TwoPi 6.28318530717958647692
#define InvPi 0.31830988618379067154
#define Inv2Pi 0.15915494309189533577
#define Inv4Pi 0.07957747154594766788

#define Epsilon 0.0001

float fbm(float3 x, int LOD)
{
	float v = 0.0;
	float a = 0.5;
	float3 shift = float3(100.0, 100.0, 100.0);

	for (int i = 0; i < LOD; ++i)
	{
		v += a * noise(x);
		x = x * 2.0 + shift;
		a *= 0.5;
	}
	return v;
}

float SphericalTheta(float3 v)
{
	return acos(clamp(v.y, -1.0f, 1.0f));
}

float SphericalPhi(float3 v)
{
	float p = atan2(v.x, v.z);
	return (p < 0.0f) ? (p + TwoPi) : p;
}


float OceanNoise(float3 vertexPos, float oceneHeight, float noiseResult, float blendFactor)
{
	float relativeWaterDepth = min(1.0, (oceneHeight - noiseResult) * 15.0);

	float oceanTime = u_TimeInfo.x * 0.03;

	float shallowWaveRefraction = 4.0;
	float waveMagnitude = 0.0002;
	float waveLength = lerp(0.007, 0.0064, blendFactor);

	float shallowWavePhase = (vertexPos.y - noiseResult * shallowWaveRefraction) * (1.0 / waveLength);
	float deepWavePhase = (atan2(vertexPos.x, vertexPos.z) + noise(vertexPos.xyz * 15.0) * 0.075) * (1.5 / waveLength);
	return (cos(shallowWavePhase + oceanTime * 1.5) * sqrt(1.0 - relativeWaterDepth) + cos(deepWavePhase + oceanTime * 2.0) * 2.5 * (1.0 - abs(vertexPos.y)) * (relativeWaterDepth * relativeWaterDepth)) * waveMagnitude;
}

float4 main(VSOutput input) : SV_TARGET
{
	// Material base color (before shading)
    float3 normalVec = normalize(input.fs_Nor.xyz);

    float4 diffuseColor = input.fs_Col;

    float Roughness = input.fs_TerrainInfo.w;
	float energyConservation = 1.0f - Roughness;

    float3 specularTerm = float3(0.0, 0.0, 0.0);
    float3 SpecularColor = float3(1.0, 1.0, 1.0);

    float3 localNormal = normalize(input.fs_Pos.xyz);

	float3x3 invMat = (float3x3)invWorldMat;

	float4 fs_ViewVec = float4(mul(invMat, normalize(camPos - input.fs_transedPos.xyz)) ,  0.0);
	float4 fs_LightVec = float4( mul(invMat,  normalize(lights[0].pos.xyz - input.fs_Pos.xyz)), 0.0);

	fs_ViewVec.w = length(camPos.xyz);

    //Terrain-atmosphere Color Interpolation
    float a = 1.0 - clamp(dot(fs_ViewVec.xyz, localNormal), 0.0, 1.0);

    a = pow(a, 5.0);

	float u_resolution = 4.0;
	float constant = 10.0;
	float sm = (1.0 - smoothstep(0.0, 7.0, log(fs_ViewVec.w)));
	int LOD = int(constant * pow(sm, 1.7));

	float noise = fbm(input.fs_Pos.xyz*u_resolution, LOD) * 2.0;
	noise = pow(noise, u_TimeInfo.z);

    //terrain
    if(input.fs_TerrainInfo.x > 0.0 && input.fs_TerrainInfo.x < 0.2 )
    {
        float4 vertexPos = input.fs_Pos;
        vertexPos.xyz += localNormal * noise;

        //detail normal
        normalVec = normalize(cross( ddx(vertexPos.xyz), ddy(vertexPos.xyz)));  
        
        float NolN= clamp(dot(localNormal, normalVec), 0.0, 1.0);
        diffuseColor = lerp(u_MountainsColor, diffuseColor, NolN*NolN*NolN);       
    }
    else
    {
        float4 vertexPos = input.fs_Pos;
		float oceneHeight = length(vertexPos.xyz) + u_HeightsInfo.x;
		float height = length(vertexPos.xyz);

		float gap = saturate((1.0 - (oceneHeight - height)));
		float gap5 = pow(gap, 3.0);

		float gap10 = pow(pow(gap, 100.0), 0.8);

		float wave = OceanNoise(vertexPos.xyz, oceneHeight, noise, gap10);
		vertexPos.xyz = (oceneHeight + wave) * localNormal;

        //detail normal
        normalVec = normalize(cross(ddx(vertexPos.xyz), ddy(vertexPos.xyz)));
    }
   

    float diffuseTerm = clamp(dot(normalVec, normalize(fs_LightVec.xyz)), 0.0, 1.0);

    float3 halfVec = fs_ViewVec.xyz + fs_LightVec.xyz;
    halfVec = normalize(halfVec);
    float LoH = clamp(dot( fs_LightVec.xyz, halfVec ), 0.0, 1.0);

    specularTerm = GGX_Spec(normalVec, halfVec, Roughness, diffuseColor.xyz, SpecularColor, LightingFunGGX_FV(LoH, Roughness)) *energyConservation;
    

    float ambientTerm = 0.0;

    float lightIntensity = diffuseTerm + ambientTerm; 

    float3 reflecVec = reflect(-fs_ViewVec.xyz, normalVec);
        
    //Envmap
    float2 st = float2(SphericalPhi(reflecVec.xyz) * Inv2Pi, SphericalTheta(reflecVec.xyz) * InvPi);
    float4 envColor = uEnvTex0.Sample(uSampler0, st) * energyConservation * 0.5;

    // Compute final shaded color
    float4 planetColor = float4( ( diffuseColor.rgb + specularTerm + envColor.xyz ) * lightIntensity, 1.0);

    return float4(lerp(planetColor.xyz ,u_AtmosphereColor.xyz, a), 1.0);
}

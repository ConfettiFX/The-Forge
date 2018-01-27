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

#include <metal_stdlib>
using namespace metal;

struct Light
{
	float4 pos;
	float4 col;
	float radius;
	float intensity;
};

static constant float PI = 3.14159265359;

struct CameraData
{
    float4x4 projView;
    float3 camPos;
    float pad_0;
};

struct ObjectData
{
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
};

struct LightData
{
    int currAmountOflights;
    int pad0;
    int pad1;
    int pad2;
    array<Light, 16> lights;
};

struct ScreenData
{
    float4 u_screenSize;
};

struct VSInput
{
    float4 vs_Pos [[attribute(0)]];
    float4 vs_Nor [[attribute(1)]];
};

struct VSOutput {
	float4 Position [[position]];
	float4 fs_Pos;
	float4 fs_Nor;

	float4 fs_Col;
	float4 fs_TerrainInfo;
	float4 fs_transedPos;
};

struct VSBGInput
{
    float4 vs_Pos [[attribute(0)]];
    float4 vs_Nor [[attribute(1)]];
};

struct VSBGOutput {
    float4 Position [[position]];
    float2 fs_UV;
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

float hash(float n, constant ObjectData& uniforms)
{
	//4D
	if (uniforms.u_TimeInfo.y > 0.0)
	{
		return fract(sin(n) *cos(uniforms.u_TimeInfo.x * 0.00001) * 1e4);
	}
	else
	{
		return fract(sin(n) * cos(uniforms.u_TimeInfo.w * 0.00001) * 1e4);
	}

}
float hash(float2 p)
{
    return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x))));
}

float noise(float x, constant ObjectData& uniforms)
{
    float i = floor(x);
    float f = fract(x);
    float u = f * f * (3.0 - 2.0 * f);
    return mix(hash(i, uniforms), hash(i + 1.0, uniforms), u);
    
}
float noise(float2 x)
{
    float2 i = floor(x);
    float2 f = fract(x);
    float a = hash(i);
    float b = hash(i + float2(1.0, 0.0));
    float c = hash(i + float2(0.0, 1.0));
    float d = hash(i + float2(1.0, 1.0));
    float2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float rand(float2 co) {
	return fract(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
}

float noise(float3 x)
{
	float3 step = float3(110, 241, 171);
	float3 i = floor(x);
	float3 f = fract(x);
	float n = dot(i, step);
	float3 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash(n + dot(step, float3(0, 0, 0))), hash(n + dot(step, float3(1, 0, 0))), u.x),
		mix(hash(n + dot(step, float3(0, 1, 0))), hash(n + dot(step, float3(1, 1, 0))), u.x), u.y),
		mix(mix(hash(n + dot(step, float3(0, 0, 1))), hash(n + dot(step, float3(1, 0, 1))), u.x),
			mix(hash(n + dot(step, float3(0, 1, 1))), hash(n + dot(step, float3(1, 1, 1))), u.x), u.y), u.z);
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


float OceanNoise(float3 vertexPos, float oceneHeight, float noiseResult, float blendFactor, constant ObjectData& uniforms)
{
	float relativeWaterDepth = min(1.0, (oceneHeight - noiseResult) * 15.0);

	float oceanTime = uniforms.u_TimeInfo.x * 0.03;

	float shallowWaveRefraction = 4.0;
	float waveMagnitude = 0.0002;
	float waveLength = mix(0.007, 0.0064, blendFactor);

	float shallowWavePhase = (vertexPos.y - noiseResult * shallowWaveRefraction) * (1.0 / waveLength);
	float deepWavePhase = (atan2(vertexPos.x, vertexPos.z) + noise(vertexPos.xyz * 15.0) * 0.075) * (1.5 / waveLength);
	return (cos(shallowWavePhase + oceanTime * 1.5) * sqrt(1.0 - relativeWaterDepth) + cos(deepWavePhase + oceanTime * 2.0) * 2.5 * (1.0 - abs(vertexPos.y)) * (relativeWaterDepth * relativeWaterDepth)) * waveMagnitude;
}

//refer to Morgan McGuire's Earth-like Tiny Planet
float3 addStars(float2 screenSize, float2 fs_UV, constant ObjectData& uniforms)
{
	float time = uniforms.u_TimeInfo.x;

	// Background starfield
	float galaxyClump = (pow(noise(fs_UV.xy * (30.0 * screenSize.x)), 3.0) * 0.5 + pow(noise(100.0 + fs_UV.xy * (15.0 * screenSize.x)), 5.0)) / 3.5;

	float color = galaxyClump * pow(hash(fs_UV.xy), 1500.0) * 80.0;
	float3 starColor = float3(color, color, color);

	starColor.x *= sqrt(noise(fs_UV.xy) * 1.2);
	starColor.y *= sqrt(noise(fs_UV.xy * 4.0));

	float2 delta = (fs_UV.xy - screenSize.xy * 0.5) * screenSize.y * 1.2;
	float radialNoise = mix(1.0, noise(normalize(delta) * 20.0 + time * 0.5), 0.12);

	float att = 0.057 * pow(max(0.0, 1.0 - (length(delta) - 0.9) / 0.9), 8.0);

	starColor += radialNoise * uniforms.u_AtmosphereColor.xyz * min(1.0, att);

	float randSeed = rand(fs_UV);

	return starColor * ((sin(randSeed + randSeed * time* 0.05) + 1.0)* 0.4 + 0.2);
}

vertex VSOutput VSMain(VSInput input                    [[stage_in]],
                       uint InstanceID                  [[instance_id]],
                       constant CameraData& cbCamera    [[buffer(0)]],
                       constant ObjectData& cbObject    [[buffer(1)]])
{
	VSOutput output;

	output.fs_TerrainInfo = float4(0.0, 0.0, 0.0, 0.0);

	float4 vertexPos = float4(input.vs_Pos.xyz, 1.0);
	output.fs_Pos = vertexPos;

	float oceneHeight = length(vertexPos.xyz) + cbObject.u_HeightsInfo.x;
	float3 localNormal = normalize(vertexPos.xyz);

	float u_resolution = 4.0;

	float noiseResult = fbm(vertexPos.xyz*u_resolution, 6) * 2.0;

	noiseResult = pow(noiseResult, cbObject.u_TimeInfo.z);

	vertexPos.xyz += localNormal * noiseResult;

	float height = length(vertexPos.xyz);

	float gap = clamp((1.0 - (oceneHeight - height)), 0.0, 1.0);
	float gap5 = pow(gap, 3.0);



	float4 ocenColor = cbObject.u_OceanColor * gap5;

	float oceneRougness = 0.15;
	float iceRougness = 0.15;
	float foliageRougness = 0.8;
	float snowRougness = 0.8;
	float shoreRougness = 0.9;

	//ocean
	if (height < oceneHeight)
	{
		float gap10 = pow(pow(gap, 100.0), 0.8);

		float wave = OceanNoise(vertexPos.xyz, oceneHeight, noiseResult, gap10, cbObject);
		vertexPos.xyz = (oceneHeight + wave) * localNormal;

		output.fs_Pos = vertexPos;
		output.fs_TerrainInfo.w = oceneRougness;
		output.fs_Col = ocenColor;
	}
	//shore
	else
	{
		output.fs_TerrainInfo.x = 0.05;

		float appliedAttitude;

		if (abs(vertexPos.y) > cbObject.u_HeightsInfo.w)
			appliedAttitude = clamp((abs(vertexPos.y) - cbObject.u_HeightsInfo.w) * 3.0, 0.0, 1.0);
		else
			appliedAttitude = 0.0;

		float4 terrainColor = mix(cbObject.u_FoliageColor, cbObject.u_PolarCapsColor, appliedAttitude);
		float terrainRoughness = mix(foliageRougness, iceRougness, appliedAttitude);

		vertexPos.xyz = height * localNormal;

		float oceneLine = oceneHeight + cbObject.u_HeightsInfo.y;
		float snowLine = 1.0 + cbObject.u_HeightsInfo.z;

		if (height < oceneLine)
		{
			output.fs_Col = cbObject.u_ShorelineColor;
			output.fs_TerrainInfo.w = shoreRougness;
		}
		else if (height >= snowLine)
		{
			output.fs_TerrainInfo.x = 0.15;

			float alpha = clamp((height - snowLine) / 0.03, 0.0, 1.0);
			output.fs_Col = mix(terrainColor, cbObject.u_SnowColor, alpha);

			output.fs_TerrainInfo.w = mix(terrainRoughness, snowRougness, alpha);
		}
		else
		{
			float alpha = clamp((height - oceneLine) / cbObject.u_HeightsInfo.y, 0.0, 1.0);
			output.fs_Col = mix(cbObject.u_ShorelineColor, terrainColor, alpha);

			output.fs_TerrainInfo.w = mix(shoreRougness, terrainRoughness, alpha);
		}
	}

	float4 modelposition = cbObject.worldMat * vertexPos;

	output.fs_transedPos = modelposition;
	output.Position = cbCamera.projView * modelposition;

	return output;
}

fragment float4 PSMain(VSOutput input                   [[stage_in]],
                       constant CameraData& cbCamera    [[buffer(0)]],
                       constant ObjectData& cbObject    [[buffer(1)]],
                       constant LightData& cbLights     [[buffer(2)]],
                       sampler uSampler0                [[sampler(3)]],
                       texture2d<float> uEnvTex0        [[texture(4)]])
{
	// Material base color (before shading)
    float3 normalVec = normalize(input.fs_Nor.xyz);

    float4 diffuseColor = input.fs_Col;

    float Roughness = input.fs_TerrainInfo.w;
	float energyConservation = 1.0f - Roughness;

    float3 specularTerm = float3(0.0, 0.0, 0.0);
    float3 SpecularColor = float3(1.0, 1.0, 1.0);

    float3 localNormal = normalize(input.fs_Pos.xyz);

	float3x3 invMat = float3x3(cbObject.invWorldMat[0].xyz, cbObject.invWorldMat[1].xyz, cbObject.invWorldMat[2].xyz);

	float4 fs_ViewVec = float4(invMat * normalize(cbCamera.camPos - input.fs_transedPos.xyz) ,  0.0);
	float4 fs_LightVec = float4(invMat *  normalize(cbLights.lights[0].pos.xyz - input.fs_Pos.xyz), 0.0);

	fs_ViewVec.w = length(cbCamera.camPos.xyz);

    //Terrain-atmosphere Color Interpolation
    float a = 1.0 - clamp(dot(fs_ViewVec.xyz, localNormal), 0.0, 1.0);

    a = pow(a, 5.0);

    //terrain
    if(input.fs_TerrainInfo.x > 0.0 && input.fs_TerrainInfo.x < 0.2 )
    {
        float u_resolution = 4.0;

        float constantVal = 10.0;

        float sm = (1.0 - smoothstep(0.0, 6.0, log(fs_ViewVec.w)));

        int LOD = int(constantVal * pow(sm, 1.7));

        float noise = fbm(input.fs_Pos.xyz*u_resolution, LOD) * 2.0;
                  
        noise = pow(noise, cbObject.u_TimeInfo.z);

        float4 vertexPos = input.fs_Pos;
        vertexPos.xyz += localNormal * noise;

        //detail normal
        normalVec = normalize(cross(dfdx(vertexPos.xyz), dfdy(vertexPos.xyz)));
        
        float NolN= clamp(dot(localNormal, normalVec), 0.0, 1.0);
        diffuseColor = mix(cbObject.u_MountainsColor, diffuseColor, NolN*NolN*NolN);
    }
    else
    {
        float4 vertexPos = input.fs_Pos;

        //detail normal
        normalVec = normalize(cross(dfdx(vertexPos.xyz), dfdy(vertexPos.xyz)));
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
    float4 envColor = uEnvTex0.sample(uSampler0, st) * energyConservation * 0.5;

    // Compute final shaded color
    float4 planetColor = float4( ( diffuseColor.rgb + specularTerm + envColor.xyz ) * lightIntensity, 1.0);

    return float4(mix(planetColor.xyz ,cbObject.u_AtmosphereColor.xyz, a), 1.0);
}


vertex VSBGOutput VSBGMain(VSBGInput input  [[stage_in]],
                           uint VertexID    [[vertex_id]],
                           uint InstanceID  [[instance_id]])
{
	VSBGOutput output;

	if (VertexID == 0)
	{
		output.Position = float4(-1.0, -1.0, 0.999998, 1.0);
		output.fs_UV = float2(0.0, 1.0);
	}
	else if (VertexID == 2)
	{
		output.Position = float4(1.0, -1.0, 0.999998, 1.0);
		output.fs_UV = float2(1.0, 1.0);
	}
	else if (VertexID == 1)
	{
		output.Position = float4(1.0, 1.0, 0.999998, 1.0);
		output.fs_UV = float2(1.0, 0.0);
	}
	else if (VertexID == 3)
	{
		output.Position = float4(-1.0, -1.0, 0.999998, 1.0);
		output.fs_UV = float2(0.0, 1.0);
	}
	else if (VertexID == 5)
	{
		output.Position = float4(1.0, 1.0, 0.999998, 1.0);
		output.fs_UV = float2(1.0, 0.0);
	}
	else if (VertexID == 4)
	{
		output.Position = float4(-1.0, 1.0, 0.999998, 1.0);
		output.fs_UV = float2(0.0, 0.0);
	}

	return output;
}

fragment float4 PSBGMain(VSBGOutput input               [[stage_in]],
                         constant ScreenData& cbScreen  [[buffer(0)]],
                         constant ObjectData& cbObject  [[buffer(1)]])
{
	float4 out_Col = float4(0.0, 0.0, 0.0, 1.0);

	float2 screenSize = cbScreen.u_screenSize.xy;

	// Background stars
	out_Col.xyz += addStars(screenSize, input.fs_UV, cbObject);
	return out_Col;
}

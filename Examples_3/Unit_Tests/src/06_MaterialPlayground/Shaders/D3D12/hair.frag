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

/*
*Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
*
*Permission is hereby granted, free of charge, to any person obtaining a copy
*of this software and associated documentation files (the "Software"), to deal
*in the Software without restriction, including without limitation the rights
*to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*copies of the Software, and to permit persons to whom the Software is
*furnished to do so, subject to the following conditions:
*
*The above copyright notice and this permission notice shall be included in
*all copies or substantial portions of the Software.
*
*THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
*AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
*THE SOFTWARE.
*/

#define SHORT_CUT_MIN_ALPHA 0.02f
#define PI 3.1415926
#define e 2.71828183
#define EPSILON 1e-7f

struct PointLight
{
	float3 pos;
	float radius;
	float3 col;
	float intensity;
};

struct DirectionalLight
{
	float3 direction;
	int shadowMap;
	float3 color;
	float intensity;
	float shadowRange;
	float _pad0;
	float _pad1;
	float _pad2;
};

struct Camera
{
	float4x4 VPMatrix;
	float4x4 InvVPMatrix;
	float3 Pos;
	float __dumm;
	int bUseEnvironmentLight;
};

cbuffer cbCamera : register(b0)
{
	Camera Cam;
}

cbuffer cbHair : register(b2)
{
	float4x4 Transform;
	uint RootColor;
	uint StrandColor;
	float ColorBias;
	float Kd;
	float Ks1;
	float Ex1;
	float Ks2;
	float Ex2;
	float FiberRadius;
	float FiberSpacing;
	uint NumVerticesPerStrand;
}

cbuffer cbPointLights : register(b3)
{
	PointLight PointLights[MAX_NUM_POINT_LIGHTS];
	uint NumPointLights;
}

cbuffer cbDirectionalLights : register(b4)
{
	DirectionalLight DirectionalLights[MAX_NUM_DIRECTIONAL_LIGHTS];
	uint NumDirectionalLights;
}

cbuffer cbDirectionalLightShadowCameras : register(b6)
{
	Camera ShadowCameras[MAX_NUM_DIRECTIONAL_LIGHTS];
}

Texture2D<float> DirectionalLightShadowMaps[MAX_NUM_DIRECTIONAL_LIGHTS] : register(t3);
SamplerState PointSampler : register(s1);

cbuffer cbHairGlobal : register(b5)
{
	float4 Viewport;
	float4 Gravity;
	float4 Wind;
	float TimeStep;
}

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 Tangent : TANGENT;
	float4 P0P1 : POINT;
	float4 Color : COLOR;
	float2 W0W1 : POINT1;
};

struct VSOutputFullscreen
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD;
};

float3 ScreenPosToNDC(float3 screenPos, float4 viewport)
{
	float2 xy = screenPos.xy;

	// add viewport offset.
	xy += viewport.xy;

	// scale by viewport to put in 0 to 1
	xy /= viewport.zw;

	// shift and scale to put in -1 to 1. y is also being flipped.
	xy.x = (2 * xy.x) - 1;
	xy.y = 1 - (2 * xy.y);

	return float3(xy, screenPos.z);
}

float ComputeCoverage(float2 p0, float2 p1, float2 pixelLoc, float2 winSize)
{
	// p0, p1, pixelLoc are in d3d clip space (-1 to 1)x(-1 to 1)

	// Scale positions so 1.f = half pixel width
	p0 *= winSize;
	p1 *= winSize;
	pixelLoc *= winSize;

	float p0dist = length(p0 - pixelLoc);
	float p1dist = length(p1 - pixelLoc);
	float hairWidth = length(p0 - p1);

	// will be 1.f if pixel outside hair, 0.f if pixel inside hair
	float outside = any(float2(step(hairWidth, p0dist), step(hairWidth, p1dist)));

	// if outside, set sign to -1, else set sign to 1
	float sign = outside > 0.f ? -1.f : 1.f;

	// signed distance (positive if inside hair, negative if outside hair)
	float relDist = sign * saturate(min(p0dist, p1dist));

	// returns coverage based on the relative distance
	// 0, if completely outside hair edge
	// 1, if completely inside hair edge
	return (relDist + 1.f) * 0.5f;
}

float3 ComputeDiffuseSpecularFactors(float3 eyeDir, float3 lightDir, float3 tangentDir)
{
	float coneAngleRadians = 10.0f * PI / 180.0f;

	// Kajiya's model
	float cosTL = dot(tangentDir, lightDir);
	float sinTL = sqrt(1.0f - cosTL * cosTL);
	float diffuse = sinTL;

	float cosTRL = -cosTL;
	float sinTRL = sinTL;
	float cosTE = dot(tangentDir, eyeDir);
	float sinTE = sqrt(1.0f - cosTE * cosTE);

	// Primary highlight
	float primaryCosTRL = cosTRL * cos(2.0f * coneAngleRadians) - sinTRL * sin(2.0f * coneAngleRadians);
	float primarySinTRL = sqrt(1.0f - primaryCosTRL * primaryCosTRL);
	float primarySpecular = max(0.0f, primaryCosTRL * cosTE + primarySinTRL * sinTE);

	// Secundary highlight
	float secundaryCosTRL = cosTRL * cos(-3.0f * coneAngleRadians) - sinTRL * sin(-3.0f * coneAngleRadians);
	float secundarySinTRL = sqrt(1.0f - secundaryCosTRL * secundaryCosTRL);
	float secundarySpecular = max(0.0f, secundaryCosTRL * cosTE + secundarySinTRL * sinTE);

	return float3(Kd * diffuse, Ks1 * pow(primarySpecular, Ex1), Ks2 * pow(secundarySpecular, Ex2));
}

float3 CalculateDirectionalLightContribution(uint lightIndex, float3 worldPosition, float3 tangent, float3 viewDirection, float3 baseColor)
{
	const DirectionalLight light = DirectionalLights[lightIndex];
	float3 radiance = light.color * light.intensity;
	float3 reflection = ComputeDiffuseSpecularFactors(viewDirection, light.direction, tangent);
	float3 reflectedLight = reflection.x * radiance * baseColor;	// diffuse
	reflectedLight += reflection.y * radiance;	// 1st specular highlight
	reflectedLight += reflection.z * radiance;	// 2nd specular highlight

	if (light.shadowMap >= 0 && light.shadowMap < MAX_NUM_DIRECTIONAL_LIGHTS)
	{
		float4 shadowPos = mul(ShadowCameras[lightIndex].VPMatrix, float4(worldPosition, 1.0f));
		shadowPos.y = -shadowPos.y;
		shadowPos.xy = (shadowPos.xy + 1.0f) * 0.5f;

		float totalWeight = 0.0f;
		float shadowFactor = 0.0f;

		const int kernelSize = 5;
		const float size = 2.4f;
		const float sigma = (kernelSize / 2.0f) / size;
		const float hairAlpha = 0.99998f;

		[unroll] for (int x = (1 - kernelSize) / 2; x <= kernelSize / 2; ++x)
		{
			[unroll] for (int y = (1 - kernelSize) / 2; y <= kernelSize / 2; ++y)
			{
				float exp = 1.0f - (x * x + y * y) / (2.0f * sigma * sigma);
				float weight = 1.0f / (2.0f * PI * sigma * sigma) * pow(e, exp);

				float shadowMapDepth = DirectionalLightShadowMaps[light.shadowMap].SampleLevel(PointSampler, shadowPos.xy, 0, int2(x, y)).r;
				float shadowRange = max(0.0f, light.shadowRange * (shadowPos.z - shadowMapDepth));
				float numFibers = shadowRange / (FiberSpacing * FiberRadius);

				[flatten] if (shadowRange > EPSILON)
					numFibers += 1;

				shadowFactor += pow(hairAlpha, numFibers) * weight;
				totalWeight += weight;
			}
		}

		shadowFactor /= totalWeight;

		reflectedLight *= shadowFactor;
	}
	
	return max(0.0f, reflectedLight);
}

float3 CalculatePointLightContribution(uint lightIndex, float3 worldPosition, float3 tangent, float3 viewDirection, float3 baseColor)
{
	const PointLight light = PointLights[lightIndex];
	float3 L = normalize(light.pos - worldPosition);

	float distance = length(light.pos - worldPosition);
	float distanceByRadius = 1.0f - pow((distance / light.radius), 4);
	float clamped = pow(saturate(distanceByRadius), 2.0f);
	float attenuation = clamped / (distance * distance + 1.0f);
	float3 radiance = light.col.rgb * attenuation * light.intensity;

	float3 reflection = ComputeDiffuseSpecularFactors(viewDirection, L, tangent);
	float3 reflectedLight = reflection.x * radiance * baseColor;	// diffuse
	reflectedLight += reflection.y * radiance;	// 1st specular highlight
	reflectedLight += reflection.z * radiance;	// 2nd specular highlight
	return max(0.0f, reflectedLight);
}

float3 HairShading(float3 worldPos, float3 eyeDir, float3 tangent, float3 baseColor)
{
	float3 color = 0.0f;

	uint i;
	for (i = 0; i < NumPointLights; ++i)
		color += CalculatePointLightContribution(i, worldPos, tangent, eyeDir, baseColor);

	for (i = 0; i < NumDirectionalLights; ++i)
		color += CalculateDirectionalLightContribution(i, worldPos, tangent, eyeDir, baseColor);
		
	return color;
}

#ifdef SHORT_CUT_CLEAR
RWTexture2DArray<uint> DepthsTexture : register(u0);

void main(VSOutputFullscreen input)
{
	DepthsTexture[uint3(input.Position.xy, 0)] = asuint(1.0f);
	DepthsTexture[uint3(input.Position.xy, 1)] = asuint(1.0f);
	DepthsTexture[uint3(input.Position.xy, 2)] = asuint(1.0f);
}

#elif defined(SHORT_CUT_DEPTH_PEELING)
RWTexture2DArray<uint> DepthsTexture : register(u0);

[earlydepthstencil]
float main(VSOutput input) : SV_TARGET
{
	float3 NDC = ScreenPosToNDC(input.Position.xyz, Viewport);
	float2 p0 = input.P0P1.xy / input.W0W1.x;
	float2 p1 = input.P0P1.zw / input.W0W1.y;
	float coverage = ComputeCoverage(p0, p1, NDC.xy, Viewport.zw);
	clip(coverage);
	float alpha = coverage * input.Color.a;
	clip(alpha - (1.0f / 255.0f));

	if (alpha < SHORT_CUT_MIN_ALPHA)
		return 1.0f;

	uint depth = asuint(input.Position.z);
	uint prevDepths[3];

	InterlockedMin(DepthsTexture[uint3(input.Position.xy, 0)], depth, prevDepths[0]);
	depth = (alpha > 0.98f) ? depth : max(depth, prevDepths[0]);
	InterlockedMin(DepthsTexture[uint3(input.Position.xy, 1)], depth, prevDepths[1]);
	depth = (alpha > 0.98f) ? depth : max(depth, prevDepths[1]);
	InterlockedMin(DepthsTexture[uint3(input.Position.xy, 2)], depth, prevDepths[2]);

	return 1.0f - alpha;
}

#elif defined(SHORT_CUT_RESOLVE_DEPTH)
Texture2DArray<uint> DepthsTexture : register(t4);

float main(VSOutputFullscreen input) : SV_DEPTH
{
	return asfloat(DepthsTexture[uint3(input.Position.xy, 2)]);
}

#elif defined(SHORT_CUT_FILL_COLOR)
[earlydepthstencil]
float4 main(VSOutput input) : SV_TARGET
{
	float3 NDC = ScreenPosToNDC(input.Position.xyz, Viewport);
	float2 p0 = input.P0P1.xy / input.W0W1.x;
	float2 p1 = input.P0P1.zw / input.W0W1.y;
	float coverage = ComputeCoverage(p0, p1, NDC.xy, Viewport.zw);
	clip(coverage);
	float alpha = coverage * input.Color.a;
	clip(alpha - (1.0f / 255.0f));

	if (alpha < SHORT_CUT_MIN_ALPHA)
		return 0.0f;

	float4 worldPos = mul(Cam.InvVPMatrix, float4(NDC, 1.0f));
	worldPos.xyz /= worldPos.w;
	float3 eyeDir = normalize(worldPos.xyz - Cam.Pos);

	float3 color = HairShading(worldPos.xyz, -eyeDir, normalize(input.Tangent.xyz), input.Color.rgb);

	return float4(color.rgb * alpha, alpha);
}

#elif defined(SHORT_CUT_RESOLVE_COLOR)
Texture2D<float4> ColorsTexture : register(t5);
Texture2D<float> InvAlphaTexture : register(t6);

[earlydepthstencil]
float4 main(VSOutputFullscreen input) : SV_TARGET
{
	float invAlpha = InvAlphaTexture[uint2(input.Position.xy)];
	float alpha = 1.0f - invAlpha;

	if (alpha < SHORT_CUT_MIN_ALPHA)
		return float4(0, 0, 0, 1);

	float4 color = ColorsTexture[uint2(input.Position.xy)];
	color.xyz /= color.w;
	color.xyz *= alpha;
	color.w = invAlpha;

	return color;
}

#elif defined(HAIR_SHADOW)
[earlydepthstencil]
void main()
{}

#else
float4 main(VSOutput input) : SV_TARGET
{
	float3 NDC = ScreenPosToNDC(input.Position.xyz, Viewport);
	float3 worldPos = mul(Cam.InvVPMatrix, float4(NDC, 1.0f));

	float coverage = ComputeCoverage(input.P0P1.xy, input.P0P1.zw, NDC.xy, Viewport.zw);
	clip(coverage);
	float alpha = coverage * input.Color.a;
	clip(alpha - (1.0f / 255.0f));

	return float4(input.Color.xyz, alpha);
}
#endif
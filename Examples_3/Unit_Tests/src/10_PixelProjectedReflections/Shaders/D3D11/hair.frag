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

#define SHORT_CUT_MIN_ALPHA 0.02f
#define PI 3.1415926

struct Light
{
	float4 Position;
	float4 Color;
	float Radius;
	float Intensity;
	float Padding;
	float Padding2;
};

cbuffer cbCamera : register(b0)
{
	float4x4 CamVPMatrix;
	float4x4 CamInvVPMatrix;
	float3 CamPos;
}

cbuffer cbHair : register(b2)
{
	float4 Viewport;
	uint BaseColor;
	float Kd;
	float Ks1;
	float Ex1;
	float Ks2;
	float Ex2;
	float FiberRadius;
	uint NumVerticesPerStrand;
}

cbuffer cbLights : register(b3)
{
	Light Lights[16];
	int NumLights;
}

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 Tangent : TANGENT;
	float4 P0P1 : POINT;
	float4 StrandColor : COLOR;
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

float3 HairShading(float3 worldPos, float3 eyeDir, float3 tangent, float3 baseColor)
{
	float3 color = 0.0f;
	
	for (int i = 0; i < NumLights; ++i)
	{
		Light l = Lights[i];

		float3 lightVec = worldPos - l.Position.xyz;
		float3 lightDir = normalize(lightVec);
		float distance = length(lightVec);

		float distanceByRadius = 1.0f - pow((distance / l.Radius), 4);
		float clamped = pow(saturate(distanceByRadius), 2.0f);
		float attenuation = clamped / (distance * distance + 1.0f);
		float intensity = l.Intensity * attenuation;
		float3 attenulatedColor = intensity * l.Color.rgb;

		float3 reflection = ComputeDiffuseSpecularFactors(eyeDir, lightDir, tangent);

		float3 reflectedLight = reflection.x * attenulatedColor * baseColor;
		reflectedLight += reflection.y * attenulatedColor;
		reflectedLight += reflection.z * attenulatedColor;

		color += max(0.0f, reflectedLight);
	}

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
	float coverage = ComputeCoverage(input.P0P1.xy, input.P0P1.zw, NDC.xy, Viewport.zw);
	clip(coverage);
	float alpha = coverage * input.StrandColor.a;
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
	float coverage = ComputeCoverage(input.P0P1.xy, input.P0P1.zw, NDC.xy, Viewport.zw);
	clip(coverage);
	float alpha = coverage * input.StrandColor.a;
	clip(alpha - (1.0f / 255.0f));

	if (alpha < SHORT_CUT_MIN_ALPHA)
		return 0.0f;

	float4 worldPos = mul(CamInvVPMatrix, float4(NDC, 1.0f));
	worldPos.xyz /= worldPos.w;
	float3 eyeDir = normalize(worldPos - CamPos);

	float3 color = HairShading(worldPos, -eyeDir, normalize(input.Tangent.xyz), input.StrandColor.rgb);

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

#else
float4 main(VSOutput input) : SV_TARGET
{
	float3 NDC = ScreenPosToNDC(input.Position.xyz, Viewport);
	float3 worldPos = mul(CamInvVPMatrix, float4(NDC, 1.0f));

	float coverage = ComputeCoverage(input.P0P1.xy, input.P0P1.zw, NDC.xy, Viewport.zw);
	clip(coverage);
	float alpha = coverage * input.StrandColor.a;
	clip(alpha - (1.0f / 255.0f));

	return float4(input.StrandColor.xyz, alpha);
}
#endif
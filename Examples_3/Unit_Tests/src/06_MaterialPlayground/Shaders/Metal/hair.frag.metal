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

#include <metal_stdlib>
using namespace metal;

#define SHORT_CUT_MIN_ALPHA 0.02f
#define PI 3.1415926
#define e 2.71828183
#define EPSILON 1e-7f

struct PointLight
{
    float4 positionAndRadius;
    float4 colorAndIntensity;
};

struct DirectionalLight
{
    packed_float3 direction;
	int shadowMap;
	packed_float3 color;
	float intensity;
    float shadowRange;
	float _pad0;
	float _pad1;
	float _pad2;
};

struct CameraData
{
	float4x4 CamVPMatrix;
	float4x4 CamInvVPMatrix;
	float3 CamPos;
	int bUseEnvironmentLight;
};

struct HairData
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
};

struct PointLightData
{
	PointLight PointLights[MAX_NUM_POINT_LIGHTS];
	uint NumPointLights;
};

struct DirectionalLightData
{
	DirectionalLight DirectionalLights[MAX_NUM_DIRECTIONAL_LIGHTS];
	uint NumDirectionalLights;
};

struct DirectionalLightCameraData
{
    CameraData Cam[MAX_NUM_DIRECTIONAL_LIGHTS];
};

#define DIRECTIONAL_LIGHT_SHADOW_MAPS_NAME DirectionalLightShadowMaps

#if MAX_NUM_DIRECTIONAL_LIGHTS > 1
struct DirectionalLightShadowMapsBuffer
{
    array<texture2d<float, access::sample>, MAX_NUM_DIRECTIONAL_LIGHTS> Textures;
};

#define DIRECTIONAL_LIGHT_SHADOW_MAPS_ATTRIBUTE constant DirectionalLightShadowMapsBuffer& DIRECTIONAL_LIGHT_SHADOW_MAPS_NAME [[buffer(9)]]
#define DIRECTIONAL_LIGHT_SHADOW_MAPS_PARAMETER constant DirectionalLightShadowMapsBuffer& DIRECTIONAL_LIGHT_SHADOW_MAPS_NAME
#define GET_DIRECTIONAL_LIGHT_SHADOW_MAP(i) DIRECTIONAL_LIGHT_SHADOW_MAPS_NAME.Textures[i]
#else
#define DIRECTIONAL_LIGHT_SHADOW_MAPS_ATTRIBUTE texture2d<float, access::sample> DIRECTIONAL_LIGHT_SHADOW_MAPS_NAME [[texture(9)]]
#define DIRECTIONAL_LIGHT_SHADOW_MAPS_PARAMETER texture2d<float, access::sample> DIRECTIONAL_LIGHT_SHADOW_MAPS_NAME
#define GET_DIRECTIONAL_LIGHT_SHADOW_MAP(i) DIRECTIONAL_LIGHT_SHADOW_MAPS_NAME
#endif

struct GlobalHairData
{
	float4 Viewport;
	float4 Gravity;
	float4 Wind;
	float TimeStep;
};

struct VSOutput
{
	float4 Position[[position]];
	float4 Tangent;
	float4 P0P1;
	float4 Color;
	float2 W0W1;
};

struct VSOutputFullscreen
{
	float4 Position[[position]];
	float2 UV;
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
    float2 stepResult = float2(step(hairWidth, p0dist), step(hairWidth, p1dist));
	float outside = min(stepResult.x + stepResult.y, 1.0f);

	// if outside, set sign to -1, else set sign to 1
	float sign = outside > 0.f ? -1.f : 1.f;

	// signed distance (positive if inside hair, negative if outside hair)
	float relDist = sign * saturate(min(p0dist, p1dist));

	// returns coverage based on the relative distance
	// 0, if completely outside hair edge
	// 1, if completely inside hair edge
	return (relDist + 1.f) * 0.5f;
}

float3 ComputeDiffuseSpecularFactors(float3 eyeDir, float3 lightDir, float3 tangentDir, constant HairData& hair)
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

	return float3(hair.Kd * diffuse, hair.Ks1 * pow(primarySpecular, hair.Ex1), hair.Ks2 * pow(secundarySpecular, hair.Ex2));
}

float3 CalculateDirectionalLightContribution(uint lightIndex, float3 worldPosition, float3 tangent, float3 viewDirection, float3 baseColor, constant DirectionalLightData& lights, constant DirectionalLightCameraData& shadowCameras, DIRECTIONAL_LIGHT_SHADOW_MAPS_PARAMETER, sampler pointSampler, constant HairData& hair)
{
	const DirectionalLight light = lights.DirectionalLights[lightIndex];
	float3 radiance = light.color * light.intensity;
	float3 reflection = ComputeDiffuseSpecularFactors(viewDirection, light.direction, tangent, hair);
	float3 reflectedLight = reflection.x * radiance * baseColor;	// diffuse
	reflectedLight += reflection.y * radiance;	// 1st specular highlight
	reflectedLight += reflection.z * radiance;	// 2nd specular highlight

	if (light.shadowMap >= 0 && light.shadowMap < MAX_NUM_DIRECTIONAL_LIGHTS)
	{
		float4 shadowPos = shadowCameras.Cam[lightIndex].CamVPMatrix * float4(worldPosition, 1.0f);
		shadowPos.y = -shadowPos.y;
		shadowPos.xy = (shadowPos.xy + 1.0f) * 0.5f;

		float totalWeight = 0.0f;
		float shadowFactor = 0.0f;

		const int kernelSize = 5;
		const float size = 2.4f;
		const float sigma = (kernelSize / 2.0f) / size;
		const float hairAlpha = 0.99998f;

#define LOOP_BODY(x, y) \
{	\
	float exp = 1.0f - (x * x + y * y) / (2.0f * sigma * sigma);	\
	float weight = 1.0f / (2.0f * PI * sigma * sigma) * pow(e, exp);	\
	float shadowMapDepth = GET_DIRECTIONAL_LIGHT_SHADOW_MAP(light.shadowMap).sample(pointSampler, shadowPos.xy, int2(x, y)).r;	\
	float shadowRange = max(0.0f, light.shadowRange * (shadowPos.z - shadowMapDepth));	\
	float numFibers = shadowRange / (hair.FiberSpacing * hair.FiberRadius);	\
	if (shadowRange > EPSILON)	numFibers += 1;	\
	shadowFactor += pow(hairAlpha, numFibers) * weight;	\
	totalWeight += weight;	\
}

		LOOP_BODY(-2, -2)
		LOOP_BODY(-2, -1)
		LOOP_BODY(-2, 0)
		LOOP_BODY(-2, 1)
		LOOP_BODY(-2, 2)
		LOOP_BODY(-1, -2)
		LOOP_BODY(-1, -1)
		LOOP_BODY(-1, 0)
		LOOP_BODY(-1, 1)
		LOOP_BODY(-1, 2)
		LOOP_BODY(0, -2)
		LOOP_BODY(0, -1)
		LOOP_BODY(0, 0)
		LOOP_BODY(0, 1)
		LOOP_BODY(0, 2)
		LOOP_BODY(1, -2)
		LOOP_BODY(1, -1)
		LOOP_BODY(1, 0)
		LOOP_BODY(1, 1)
		LOOP_BODY(1, 2)
		LOOP_BODY(2, -2)
		LOOP_BODY(2, -1)
		LOOP_BODY(2, 0)
		LOOP_BODY(2, 1)
		LOOP_BODY(2, 2)

		shadowFactor /= totalWeight;
		reflectedLight *= shadowFactor;
	}
	
	return max(0.0f, reflectedLight);
}

float3 CalculatePointLightContribution(uint lightIndex, float3 worldPosition, float3 tangent, float3 viewDirection, float3 baseColor, constant PointLightData& lights, constant HairData& hair)
{
	const PointLight light = lights.PointLights[lightIndex];
	float3 L = normalize(light.positionAndRadius.xyz - worldPosition);

	float distance = length(light.positionAndRadius.xyz - worldPosition);
	float distanceByRadius = 1.0f - pow((distance / light.positionAndRadius.w), 4);
	float clamped = pow(saturate(distanceByRadius), 2.0f);
	float attenuation = clamped / (distance * distance + 1.0f);
	float3 radiance = light.colorAndIntensity.rgb * attenuation * light.colorAndIntensity.w;

	float3 reflection = ComputeDiffuseSpecularFactors(viewDirection, L, tangent, hair);
	float3 reflectedLight = reflection.x * radiance * baseColor;	// diffuse
	reflectedLight += reflection.y * radiance;	// 1st specular highlight
	reflectedLight += reflection.z * radiance;	// 2nd specular highlight
	return max(0.0f, reflectedLight);
}

float3 HairShading(float3 worldPos, float3 eyeDir, float3 tangent, float3 baseColor, constant PointLightData& pointLights, constant DirectionalLightData& directionalLights, constant DirectionalLightCameraData& shadowCameras, DIRECTIONAL_LIGHT_SHADOW_MAPS_PARAMETER, sampler pointSampler, constant HairData& hair)
{
	float3 color = 0.0f;

	uint i;
	for (i = 0; i < pointLights.NumPointLights; ++i)
		color += CalculatePointLightContribution(i, worldPos, tangent, eyeDir, baseColor, pointLights, hair);

	for (i = 0; i < directionalLights.NumDirectionalLights; ++i)
		color += CalculateDirectionalLightContribution(i, worldPos, tangent, eyeDir, baseColor, directionalLights, shadowCameras, DIRECTIONAL_LIGHT_SHADOW_MAPS_NAME, pointSampler, hair);
		
	return color;
}

#ifdef SHORT_CUT_CLEAR

fragment void stageMain(VSOutputFullscreen input[[stage_in]],
    device uint* DepthsTexture[[buffer(6)]],
    constant GlobalHairData& cbHairGlobal[[buffer(5)]])
{
    uint2 p = uint2(input.Position.xy);
    uint2 viewport = uint2(cbHairGlobal.Viewport.zw);
    DepthsTexture[p.x + p.y * viewport.x] = as_type<uint>(1.0f);
    DepthsTexture[p.x + p.y * viewport.x + viewport.x * viewport.y] = as_type<uint>(1.0f);
    DepthsTexture[p.x + p.y * viewport.x + viewport.x * viewport.y * 2] = as_type<uint>(1.0f);
}

#elif defined(SHORT_CUT_DEPTH_PEELING)

[[early_fragment_tests]]
fragment float4 stageMain(VSOutput input[[stage_in]],
    device atomic_uint* DepthsTexture[[buffer(6)]],
    constant GlobalHairData& cbHairGlobal[[buffer(5)]])
{
	float3 NDC = ScreenPosToNDC(input.Position.xyz, cbHairGlobal.Viewport);
	float2 p0 = input.P0P1.xy / input.W0W1.x;
	float2 p1 = input.P0P1.zw / input.W0W1.y;
    float coverage = ComputeCoverage(p0, p1, NDC.xy, cbHairGlobal.Viewport.zw);
	if(coverage < 0.0f)
        discard_fragment();
    float alpha = coverage * input.Color.a;
    if((alpha - (1.0f / 255.0f)) < 0.0f)
        discard_fragment();

	if (alpha < SHORT_CUT_MIN_ALPHA)
		return 1.0f;

	uint depth = as_type<uint>(input.Position.z);
	uint prevDepths[3];
    uint2 viewport = uint2(cbHairGlobal.Viewport.zw);

    uint2 p = uint2(input.Position.xy);
    prevDepths[0] = atomic_fetch_min_explicit(&DepthsTexture[p.x + p.y * viewport.x], depth, memory_order_relaxed);
	depth = (alpha > 0.98f) ? depth : max(depth, prevDepths[0]);
    prevDepths[1] = atomic_fetch_min_explicit(&DepthsTexture[p.x + p.y * viewport.x + viewport.x * viewport.y], depth, memory_order_relaxed);
	depth = (alpha > 0.98f) ? depth : max(depth, prevDepths[1]);
    prevDepths[2] = atomic_fetch_min_explicit(&DepthsTexture[p.x + p.y * viewport.x + viewport.x * viewport.y * 2], depth, memory_order_relaxed);

	return float4(1.0f - alpha, 0, 0, 0);
}

#elif defined(SHORT_CUT_RESOLVE_DEPTH)

struct FSOutput
{
	float depth [[depth(any)]];
};

fragment FSOutput stageMain(VSOutputFullscreen input[[stage_in]],
    device uint* DepthsTexture[[buffer(6)]],
    constant GlobalHairData& cbHairGlobal[[buffer(5)]])
{
	FSOutput output;
    uint2 p = uint2(input.Position.xy);
    uint2 viewport = uint2(cbHairGlobal.Viewport.zw);
	output.depth = as_type<float>(DepthsTexture[p.x + p.y * viewport.x + viewport.x * viewport.y * 2]);
	return output;
}

#elif defined(SHORT_CUT_FILL_COLOR)

[[early_fragment_tests]]
fragment float4 stageMain(VSOutput input[[stage_in]],
    constant CameraData& cbCamera[[buffer(3)]],
    constant HairData& cbHair[[buffer(4)]],
    constant GlobalHairData& cbHairGlobal[[buffer(5)]],
    constant PointLightData& cbPointLights[[buffer(7)]],
    constant DirectionalLightData& cbDirectionalLights[[buffer(8)]],
    constant DirectionalLightCameraData& cbDirectionalLightShadowCameras[[buffer(11)]],
    DIRECTIONAL_LIGHT_SHADOW_MAPS_ATTRIBUTE,
    sampler PointSampler[[sampler(10)]])
{
	float3 NDC = ScreenPosToNDC(input.Position.xyz, cbHairGlobal.Viewport);
	float2 p0 = input.P0P1.xy / input.W0W1.x;
	float2 p1 = input.P0P1.zw / input.W0W1.y;
	float coverage = ComputeCoverage(p0, p1, NDC.xy, cbHairGlobal.Viewport.zw);
    if(coverage < 0.0f)
        discard_fragment();
	float alpha = coverage * input.Color.a;
	if((alpha - (1.0f / 255.0f)) < 0.0f)
        discard_fragment();

	if (alpha < SHORT_CUT_MIN_ALPHA)
		return 0.0f;

	float4 worldPos = cbCamera.CamInvVPMatrix * float4(NDC, 1.0f);
	worldPos.xyz /= worldPos.w;
	float3 eyeDir = normalize(worldPos.xyz - cbCamera.CamPos);

	float3 color = HairShading(worldPos.xyz, -eyeDir, normalize(input.Tangent.xyz), input.Color.rgb, cbPointLights, cbDirectionalLights, cbDirectionalLightShadowCameras, DIRECTIONAL_LIGHT_SHADOW_MAPS_NAME, PointSampler, cbHair);

	return float4(color.rgb * alpha, alpha);
}

#elif defined(SHORT_CUT_RESOLVE_COLOR)

[[early_fragment_tests]]
fragment float4 stageMain(VSOutputFullscreen input[[stage_in]],
    texture2d<float, access::read> ColorsTexture[[texture(5)]],
    texture2d<float, access::read> InvAlphaTexture[[texture(6)]])
{
	float invAlpha = InvAlphaTexture.read(uint2(input.Position.xy)).r;
	float alpha = 1.0f - invAlpha;

	if (alpha < SHORT_CUT_MIN_ALPHA)
		return float4(0, 0, 0, 1);

	float4 color = ColorsTexture.read(uint2(input.Position.xy));
	color.xyz /= color.w;
	color.xyz *= alpha;
	color.w = invAlpha;

	return color;
}

#elif defined(HAIR_SHADOW)

[[early_fragment_tests]]
fragment void stageMain()
{}

#else
fragment float4 stageMain(VSOutput input[[stage_in]],
    constant CameraData& cbCamera[[buffer(3)]],
    constant HairData& cbHair[[buffer(4)]],
    constant GlobalHairData& cbHairGlobal[[buffer(5)]])
{
	float3 NDC = ScreenPosToNDC(input.Position.xyz, cbHairGlobal.Viewport);
	float3 worldPos = cbCamera.CamInvVPMatrix * float4(NDC, 1.0f);

	float coverage = ComputeCoverage(input.P0P1.xy, input.P0P1.zw, NDC.xy, cbHairGlobal.Viewport.zw);
    if(coverage < 0.0f)
        discard_fragment();
	float alpha = coverage * input.Color.a;
	if((alpha - (1.0f / 255.0f)) < 0.0f)
        discard_fragment();

	return float4(input.Color.xyz, alpha);
}
#endif

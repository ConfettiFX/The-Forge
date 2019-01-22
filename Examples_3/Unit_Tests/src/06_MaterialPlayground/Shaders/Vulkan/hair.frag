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

#version 460 core

#define SHORT_CUT_MIN_ALPHA 0.02f
#define PI 3.1415926
#define e 2.71828183
#define EPSILON 1e-7f

struct PointLight
{
	vec3 pos;
	float radius;
	vec3 col;
	float intensity;
};

struct DirectionalLight
{
	vec3 direction;
	int shadowMap;
	vec3 color;
	float intensity;
	float shadowRange;
	float _pad0;
	float _pad1;
	float _pad2;
};

struct Camera
{
	mat4 VPMatrix;
	mat4 InvVPMatrix;
	vec3 Pos;
};

layout(set = 0, binding = 0) uniform cbCamera
{
	Camera Cam;
};

layout(set = 0, binding = 1) uniform cbHair
{
	mat4 Transform;
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

layout(set = 0, binding = 2) uniform cbHairGlobal
{
	vec4 Viewport;
	vec4 Gravity;
	vec4 Wind;
	float TimeStep;
};

layout(set = 0, binding = 6) uniform cbPointLights
{
	PointLight PointLights[MAX_NUM_POINT_LIGHTS];
	uint NumPointLights;
};

layout(set = 0, binding = 7) uniform cbDirectionalLights
{
	DirectionalLight DirectionalLights[MAX_NUM_DIRECTIONAL_LIGHTS];
	uint NumDirectionalLights;
};

layout(set = 0, binding = 10) uniform cbDirectionalLightShadowCameras
{
	Camera ShadowCameras[MAX_NUM_DIRECTIONAL_LIGHTS];
};

layout(set = 0, binding = 8) uniform texture2D DirectionalLightShadowMaps[MAX_NUM_DIRECTIONAL_LIGHTS];
layout(set = 0, binding = 9) uniform sampler PointSampler;

vec3 ScreenPosToNDC(vec3 screenPos, vec4 viewport)
{
	vec2 xy = screenPos.xy;

	// add viewport offset.
	xy += viewport.xy;

	// scale by viewport to put in 0 to 1
	xy /= viewport.zw;

	// shift and scale to put in -1 to 1. y is also being flipped.
	xy.x = (2 * xy.x) - 1;
	xy.y = 1 - (2 * xy.y);

	return vec3(xy, screenPos.z);
}

float ComputeCoverage(vec2 p0, vec2 p1, vec2 pixelLoc, vec2 winSize)
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
	vec2 pDist = vec2(step(hairWidth, p0dist), step(hairWidth, p1dist));
	float outside = clamp(sign(pDist.x + pDist.y), 0.0f, 1.0f);

	// if outside, set sign to -1, else set sign to 1
	float sign = outside > 0.f ? -1.f : 1.f;

	// signed distance (positive if inside hair, negative if outside hair)
	float relDist = sign * clamp(min(p0dist, p1dist), 0.0f, 1.0f);

	// returns coverage based on the relative distance
	// 0, if completely outside hair edge
	// 1, if completely inside hair edge
	return (relDist + 1.f) * 0.5f;
}

vec3 ComputeDiffuseSpecularFactors(vec3 eyeDir, vec3 lightDir, vec3 tangentDir)
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

	return vec3(Kd * diffuse, Ks1 * pow(primarySpecular, Ex1), Ks2 * pow(secundarySpecular, Ex2));
}

vec3 CalculateDirectionalLightContribution(uint lightIndex, vec3 worldPosition, vec3 tangent, vec3 viewDirection, vec3 baseColor)
{
	const DirectionalLight light = DirectionalLights[lightIndex];
	vec3 radiance = light.color * light.intensity;
	vec3 reflection = ComputeDiffuseSpecularFactors(viewDirection, light.direction, tangent);
	vec3 reflectedLight = reflection.x * radiance * baseColor;	// diffuse
	reflectedLight += reflection.y * radiance;	// 1st specular highlight
	reflectedLight += reflection.z * radiance;	// 2nd specular highlight

	if (light.shadowMap >= 0 && light.shadowMap < MAX_NUM_DIRECTIONAL_LIGHTS)
	{
#define shadowSampler sampler2D(DirectionalLightShadowMaps[light.shadowMap], PointSampler)
		vec4 shadowPos = ShadowCameras[lightIndex].VPMatrix * vec4(worldPosition, 1.0f);
		shadowPos.y = -shadowPos.y;
		shadowPos.xy = (shadowPos.xy + 1.0f) * 0.5f;

		float totalWeight = 0.0f;
		float shadowFactor = 0.0f;

		const int kernelSize = 5;
		const float size = 2.4f;
		const float sigma = (kernelSize / 2.0f) / size;
		const float hairAlpha = 0.99998f;

		// Had to manually unroll loop. textureLodOffset requires offset parameter to be a compile time constant.
#define LOOP_BODY(x, y)																				\
		{																							\
			float exp = 1.0f - (x * x + y * y) / (2.0f * sigma * sigma);							\
			float weight = 1.0f / (2.0f * PI * sigma * sigma) * pow(e, exp);						\
			float shadowMapDepth = textureLodOffset(shadowSampler, shadowPos.xy, 0, ivec2(x, y)).r;	\
			float shadowRange = max(0.0f, light.shadowRange * (shadowPos.z - shadowMapDepth));		\
			float numFibers = shadowRange / (FiberSpacing * FiberRadius);							\
			if (shadowRange > EPSILON) numFibers += 1;												\
			shadowFactor += pow(hairAlpha, numFibers) * weight;										\
			totalWeight += weight;																	\
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

#undef shadowSampler

		shadowFactor /= totalWeight;
		reflectedLight *= shadowFactor;
	}

	return max(vec3(0.0f), reflectedLight);
}

vec3 CalculatePointLightContribution(uint lightIndex, vec3 worldPosition, vec3 tangent, vec3 viewDirection, vec3 baseColor)
{
	const PointLight light = PointLights[lightIndex];
	vec3 L = normalize(light.pos - worldPosition);

	float distance = length(light.pos - worldPosition);
	float distanceByRadius = 1.0f - pow((distance / light.radius), 4);
	float clamped = pow(clamp(distanceByRadius, 0.0f, 1.0f), 2.0f);
	float attenuation = clamped / (distance * distance + 1.0f);
	vec3 radiance = light.col.rgb * attenuation * light.intensity;

	vec3 reflection = ComputeDiffuseSpecularFactors(viewDirection, L, tangent);
	vec3 reflectedLight = reflection.x * radiance * baseColor;	// diffuse
	reflectedLight += reflection.y * radiance;	// 1st specular highlight
	reflectedLight += reflection.z * radiance;	// 2nd specular highlight
	return max(vec3(0.0f), reflectedLight);
}

vec3 HairShading(vec3 worldPos, vec3 eyeDir, vec3 tangent, vec3 baseColor)
{
	vec3 color = vec3(0.0f);

	uint i;
	for (i = 0; i < NumPointLights; ++i)
		color += CalculatePointLightContribution(i, worldPos, tangent, eyeDir, baseColor);

	for (i = 0; i < NumDirectionalLights; ++i)
		color += CalculateDirectionalLightContribution(i, worldPos, tangent, eyeDir, baseColor);

	return color;
}

#ifdef SHORT_CUT_CLEAR
layout(set = 0, binding = 6, r32ui) uniform uimage2DArray DepthsTexture;

layout(location = 0) in vec2 UV;

void main()
{
	imageStore(DepthsTexture, ivec3(gl_FragCoord.xy, 0), uvec4(floatBitsToUint(1.0f), 0, 0, 0));
	imageStore(DepthsTexture, ivec3(gl_FragCoord.xy, 1), uvec4(floatBitsToUint(1.0f), 0, 0, 0));
	imageStore(DepthsTexture, ivec3(gl_FragCoord.xy, 2), uvec4(floatBitsToUint(1.0f), 0, 0, 0));
}

#elif defined(SHORT_CUT_DEPTH_PEELING)
layout(set = 0, binding = 6, r32ui) uniform uimage2DArray DepthsTexture;

layout(location = 0) in vec4 Tangent;
layout(location = 1) in vec4 P0P1;
layout(location = 2) in vec4 Color;
layout(location = 3) in vec2 W0W1;

layout(location = 0) out float Coverage;

layout(early_fragment_tests) in;
void main()
{
	vec3 NDC = ScreenPosToNDC(gl_FragCoord.xyz, Viewport);
	vec2 p0 = P0P1.xy / W0W1.x;
	vec2 p1 = P0P1.zw / W0W1.y;
	float coverage = ComputeCoverage(p0, p1, NDC.xy, Viewport.zw);
	if (coverage < 0.0f)
		discard;
	float alpha = coverage * Color.a;
	if ((alpha - (1.0f / 255.0f)) < 0.0f)
		discard;

	if (alpha < SHORT_CUT_MIN_ALPHA)
	{
		Coverage = 1.0f;
		return;
	}

	uint depth = floatBitsToUint(gl_FragCoord.z);
	uint prevDepths[3];

	prevDepths[0] = imageAtomicMin(DepthsTexture, ivec3(gl_FragCoord.xy, 0), depth);
	depth = (alpha > 0.98f) ? depth : max(depth, prevDepths[0]);
	prevDepths[1] = imageAtomicMin(DepthsTexture, ivec3(gl_FragCoord.xy, 1), depth);
	depth = (alpha > 0.98f) ? depth : max(depth, prevDepths[1]);
	prevDepths[2] = imageAtomicMin(DepthsTexture, ivec3(gl_FragCoord.xy, 2), depth);

	Coverage = 1.0f - alpha;
}

#elif defined(SHORT_CUT_RESOLVE_DEPTH)
#extension GL_EXT_samplerless_texture_functions : enable
layout(set = 0, binding = 6) uniform utexture2DArray DepthsTexture;

layout(location = 0) in vec2 UV;

void main()
{
	gl_FragDepth = uintBitsToFloat(texelFetch(DepthsTexture, ivec3(gl_FragCoord.xy, 2), 0).r);
}

#elif defined(SHORT_CUT_FILL_COLOR)
layout(location = 0) in vec4 Tangent;
layout(location = 1) in vec4 P0P1;
layout(location = 2) in vec4 Color;
layout(location = 3) in vec2 W0W1;

layout(location = 0) out vec4 FragColor;

layout(early_fragment_tests) in;
void main()
{
	vec3 NDC = ScreenPosToNDC(gl_FragCoord.xyz, Viewport);
	vec2 p0 = P0P1.xy / W0W1.x;
	vec2 p1 = P0P1.zw / W0W1.y;
	float coverage = ComputeCoverage(p0, p1, NDC.xy, Viewport.zw);
	if (coverage < 0.0f)
		discard;
	float alpha = coverage * Color.a;
	if ((alpha - (1.0f / 255.0f)) < 0.0f)
		discard;

	if (alpha < SHORT_CUT_MIN_ALPHA)
	{
		FragColor = vec4(0.0f);
		return;
	}

	vec4 worldPos = Cam.InvVPMatrix * vec4(NDC, 1.0f);
	worldPos.xyz /= worldPos.w;
	vec3 eyeDir = normalize(worldPos.xyz - Cam.Pos);

	vec3 color = HairShading(worldPos.xyz, -eyeDir, normalize(Tangent.xyz), Color.rgb);

	FragColor = vec4(color.rgb * alpha, alpha);
}

#elif defined(SHORT_CUT_RESOLVE_COLOR)
#extension GL_EXT_samplerless_texture_functions : enable
layout(set = 0, binding = 6) uniform texture2D ColorsTexture;
layout(set = 0, binding = 7) uniform texture2D InvAlphaTexture;

layout(location = 0) in vec2 UV;

layout(location = 0) out vec4 FragColor;

layout(early_fragment_tests) in;
void main()
{
	float invAlpha = texelFetch(InvAlphaTexture, ivec2(gl_FragCoord.xy), 0).r;
	float alpha = 1.0f - invAlpha;

	if (alpha < SHORT_CUT_MIN_ALPHA)
	{
		FragColor = vec4(0, 0, 0, 1);
		return;
	}

	vec4 color = texelFetch(ColorsTexture, ivec2(gl_FragCoord.xy), 0);
	color.xyz /= color.w;
	color.xyz *= alpha;
	color.w = invAlpha;

	FragColor = color;
}

#elif defined(HAIR_SHADOW)

layout(early_fragment_tests) in;
void main()
{}

#else
layout(location = 0) in vec4 Tangent;
layout(location = 1) in vec4 P0P1;
layout(location = 2) in vec4 Color;

layout(location = 0) out vec4 FragColor;

void main()
{
	vec3 NDC = ScreenPosToNDC(gl_FragCoord.xyz, Viewport);
	vec3 worldPos = Cam.InvVPMatrix * vec4(NDC, 1.0f);

	float coverage = ComputeCoverage(P0P1.xy, P0P1.zw, NDC.xy, Viewport.zw);
	if (coverage < 0.0f)
		discard;
	float alpha = coverage * Color.a;
	if ((alpha - (1.0f / 255.0f)) < 0.0f)
		discard;

	FragColor = vec4(Color.xyz, alpha);
}
#endif
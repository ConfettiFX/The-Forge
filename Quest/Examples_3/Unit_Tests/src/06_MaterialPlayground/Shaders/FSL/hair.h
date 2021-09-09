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

#ifndef hair_h
#define hair_h

#define SHORT_CUT_MIN_ALPHA 0.02f
#define PI 3.1415926
#define e 2.71828183
#define EPSILON 1e-7f

#ifndef MAX_NUM_POINT_LIGHTS
	#define MAX_NUM_POINT_LIGHTS 8
#endif
#ifndef MAX_NUM_DIRECTIONAL_LIGHTS
	#define MAX_NUM_DIRECTIONAL_LIGHTS 1
#endif

struct PointLight
{
    float4 positionAndRadius;
    float4 colorAndIntensity;
};

struct DirectionalLight
{
	float4 directionAndShadowMap;
	float4 colorAndIntensity;
	float shadowRange;
	float _pad0;
	float _pad1;
	float _pad2;
	float4x4 viewProj;
};

struct Camera
{
	float4x4 VPMatrix[VR_MULTIVIEW_COUNT];
	float4x4 InvVPMatrix[VR_MULTIVIEW_COUNT];
	float3 Pos;
#ifndef METAL
	float __dumm;
#endif
	
	float fAmbientLightIntensity;
	int bUseEnvironmentLight;
	float fEnvironmentLightIntensity;
	float fAOIntensity;

	int renderMode;
	float fNormalMapIntensity;
}; 

struct CameraShadow
{
    float4x4 VPMatrix;
    float4x4 InvVPMatrix;
    float3 Pos;
#ifndef METAL
    float __dumm;
#endif
};

// UPDATE_FREQ_NONE
#ifndef HAIR_SHADOW
CBUFFER(cbHairGlobal, UPDATE_FREQ_NONE, b5, binding = 2)
{
	DATA(float4, Viewport, None);
	DATA(float4, Gravity, None);
	DATA(float4, Wind, None);
	DATA(float, TimeStep, None);
};

CBUFFER(cbPointLights, UPDATE_FREQ_NONE, b3, binding = 6)
{
	DATA(PointLight, PointLights[MAX_NUM_POINT_LIGHTS], None);
	DATA(uint, NumPointLights, None);
};

CBUFFER(cbDirectionalLights, UPDATE_FREQ_NONE, b4, binding = 7)
{
	DATA(DirectionalLight, DirectionalLights[MAX_NUM_DIRECTIONAL_LIGHTS], None);
	DATA(uint, NumDirectionalLights, None);
};

// TODO: fix this
#ifdef METAL
	RES(RWBuffer(uint), DepthsTexture,  UPDATE_FREQ_NONE, u0, binding = 6);
#else
	#ifdef SHORT_CUT_RESOLVE_DEPTH
		RES(Tex2DArray(uint), DepthsTexture,  UPDATE_FREQ_NONE, t4, binding = 6);
	#elif defined(SHORT_CUT_DEPTH_PEELING) || defined(SHORT_CUT_CLEAR)
		RES(RWTex2DArray(uint), DepthsTexture,  UPDATE_FREQ_NONE, u1, binding = 6);
	#endif
#endif

RES(SamplerState, PointSampler,  UPDATE_FREQ_NONE, s1, binding = 9);

RES(Tex2DArray(float4), ColorsTexture, UPDATE_FREQ_NONE, t5, binding = 6);
RES(Tex2DArray(float), InvAlphaTexture, UPDATE_FREQ_NONE, t6, binding = 7);
#endif

// UPDATE_FREQ_PER_FRAME
#if !defined(HAIR_SHADOW)
CBUFFER(cbCamera, UPDATE_FREQ_PER_FRAME, b0, binding = 0)
{
	DATA(Camera, Cam, None);
};
// UPDATE_FREQ_PER_BATCH
#else
CBUFFER(cbCamera, UPDATE_FREQ_PER_BATCH, b0, binding = 0)
{
	DATA(CameraShadow, Cam, None);
};
#endif

#ifdef SHORT_CUT_FILL_COLOR
CBUFFER(cbDirectionalLightShadowCameras, UPDATE_FREQ_PER_BATCH, b6, binding = 10)
{
	DATA(CameraShadow, ShadowCameras[MAX_NUM_DIRECTIONAL_LIGHTS], None);
};

RES(Tex2D(float), DirectionalLightShadowMaps[MAX_NUM_DIRECTIONAL_LIGHTS],  UPDATE_FREQ_PER_BATCH, t3, binding = 8);
#endif

// UPDATE_FREQ_PER_DRAW
CBUFFER(cbHair, UPDATE_FREQ_PER_DRAW, b2, binding = 1)
{
	DATA(float4x4, Transform, None);
	DATA(uint, RootColor, None);
	DATA(uint, StrandColor, None);
	DATA(float, ColorBias, None);
	DATA(float, Kd, None);
	DATA(float, Ks1, None);
	DATA(float, Ex1, None);
	DATA(float, Ks2, None);
	DATA(float, Ex2, None);
	DATA(float, FiberRadius, None);
	DATA(float, FiberSpacing, None);
	DATA(uint, NumVerticesPerStrand, None);
};
RES(Buffer(float4), GuideHairVertexPositions,  UPDATE_FREQ_PER_DRAW, t0, binding = 3);
RES(Buffer(float4), GuideHairVertexTangents,   UPDATE_FREQ_PER_DRAW, t1, binding = 4);
RES(Buffer(float),  HairThicknessCoefficients, UPDATE_FREQ_PER_DRAW, t2, binding = 5);

// Common functions
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
	float outside = float(any(float2(step(hairWidth, p0dist), step(hairWidth, p1dist))));

	// if outside, set sign to -1, else set sign to 1
	float sign = outside > 0.f ? -1.f : 1.f;

	// signed distance (positive if inside hair, negative if outside hair)
	float relDist = sign * saturate(min(p0dist, p1dist));

	// returns coverage based on the relative distance
	// 0, if completely outside hair edge
	// 1, if completely inside hair edge
	return (relDist + 1.f) * 0.5f;
}

// Common shader I/O
STRUCT(VSOutput)
{
	DATA(float4, Position, SV_Position);
	DATA(float4, Tangent, TANGENT);
	DATA(float4, P0P1, POINT);
	DATA(float4, Color, COLOR);
	DATA(float2, W0W1, POINT1);
    DATA(FLAT(uint), ViewID, TEXCOORD3);
};

STRUCT(VSOutputFullscreen)
{
	DATA(float4, Position, SV_Position);
	DATA(float2, UV, TEXCOORD);
    DATA(FLAT(uint), ViewID, TEXCOORD3);
};

#endif /* hair_h */

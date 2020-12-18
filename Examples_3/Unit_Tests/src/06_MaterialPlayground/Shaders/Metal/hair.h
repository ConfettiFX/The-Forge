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

struct CameraData
{
	float4x4 CamVPMatrix;
	float4x4 CamInvVPMatrix;
	float3 CamPos;
	float fAmbientLightIntensity;
	int bUseEnvironmentLight;
	float fEnvironmentLightIntensity;
	float fAOIntensity;

	int renderMode;
	float fNormalMapIntensity;
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

struct GlobalHairData
{
	float4 Viewport;
	float4 Gravity;
	float4 Wind;
	float TimeStep;
};

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
	int shadowMapDimensions;
	float4x4 viewProj;
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

struct VSData
{
    constant GlobalHairData& cbHairGlobal;
    
    constant PointLightData& cbPointLights;
    constant DirectionalLightData& cbDirectionalLights;
    
    device uint* DepthsTexture;
    
    sampler PointSampler;
	
    texture2d<float, access::read> ColorsTexture;
    texture2d<float, access::read> InvAlphaTexture;
};

struct VSDataPerFrame
{
#if !defined(HAIR_SHADOW)
    constant CameraData& cbCamera;
#endif
};

struct VSDataPerBatch
{
#if defined(HAIR_SHADOW)
        constant CameraData& cbCamera;
#endif
    
    constant DirectionalLightCameraData& cbDirectionalLightShadowCameras;
    
    array<texture2d<float, access::sample>, MAX_NUM_DIRECTIONAL_LIGHTS> DirectionalLightShadowMaps;
};

struct VSDataPerDraw
{
    constant HairData& cbHair;

    constant float4* GuideHairVertexPositions;
    constant float4* GuideHairVertexTangents;
    constant float* HairThicknessCoefficients;
};

#endif /* hair_h */

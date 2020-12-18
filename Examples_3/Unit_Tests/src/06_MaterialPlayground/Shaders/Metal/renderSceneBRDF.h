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

#ifndef renderSceneBRDF_h
#define renderSceneBRDF_h

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
	int shadowMapDimensions;
	float4x4 viewProj;
};

struct CameraData
{
    float4x4 projView;
    float4x4 invProjView;
    float3 camPos;

	float fAmbientLightIntensity;
	int bUseEnvironmentLight;
	float fEnvironmentLightIntensity;
	float fAOIntensity;

	int renderMode;
	float fNormalMapIntensity;
};

struct ObjectData
{
	float4x4 worldMat;
	float4 albedoAndRoughness;
	float2 tiling;
	float metalness;
	int textureConfig;
};

struct PointLightData
{
	PointLight PointLights[MAX_NUM_POINT_LIGHTS];
	int NumPointLights;
};

struct DirectionalLightData
{
	DirectionalLight DirectionalLights[MAX_NUM_DIRECTIONAL_LIGHTS];
	int NumDirectionalLights;
};

struct VSData
{
    constant PointLightData& cbPointLights              [[id(0)]];
    constant DirectionalLightData& cbDirectionalLights  [[id(1)]];
    
    texture2d<float, access::sample> brdfIntegrationMap [[id(2)]];
    texturecube<float, access::sample> irradianceMap    [[id(3)]];
    texturecube<float, access::sample> specularMap      [[id(4)]];
    texture2d<float, access::sample> shadowMap          [[id(5)]];

    sampler bilinearSampler                             [[id(6)]];
    sampler bilinearClampedSampler                      [[id(7)]];
};

struct VSDataPerFrame
{
    constant CameraData& cbCamera                       [[id(0)]];
};

struct VSDataPerDraw
{
    constant ObjectData& cbObject                       [[id(0)]];
    
    texture2d<float> albedoMap                          [[id(1)]];
    texture2d<float, access::sample> normalMap          [[id(2)]];
    texture2d<float, access::sample> metallicMap        [[id(3)]];
    texture2d<float, access::sample> roughnessMap       [[id(4)]];
    texture2d<float, access::sample> aoMap              [[id(5)]];
};

#endif /* renderSceneBRDF_h */

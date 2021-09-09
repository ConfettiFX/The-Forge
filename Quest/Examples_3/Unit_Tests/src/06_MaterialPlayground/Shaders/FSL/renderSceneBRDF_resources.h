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

#ifndef MAX_NUM_POINT_LIGHTS
	#define MAX_NUM_POINT_LIGHTS 8
#endif
#ifndef MAX_NUM_DIRECTIONAL_LIGHTS
	#define MAX_NUM_DIRECTIONAL_LIGHTS 1
#endif

STRUCT(PointLight)
{
	DATA(float4, positionAndRadius, None);
	DATA(float4, colorAndIntensity, None);
};

STRUCT(DirectionalLight)
{
	DATA(float4, directionAndShadowMap, None);
	DATA(float4, colorAndIntensity, None);
	DATA(float, shadowRange, None);
	DATA(float, _pad0, None);
	DATA(float, _pad1, None);
	DATA(int, shadowMapDimensions, None);
	DATA(float4x4, viewProj, None);
};

// UPDATE_FREQ_NONE
CBUFFER(cbPointLights, UPDATE_FREQ_NONE, b2, binding = 2)
{
	DATA(PointLight, PointLights[MAX_NUM_POINT_LIGHTS], None);
	DATA(uint, NumPointLights, None);
};

CBUFFER(cbDirectionalLights, UPDATE_FREQ_NONE, b3, binding = 3)
{
	DATA(DirectionalLight, DirectionalLights[MAX_NUM_DIRECTIONAL_LIGHTS], None);
	DATA(uint, NumDirectionalLights, None);
};

RES(Tex2D(float2), brdfIntegrationMap, UPDATE_FREQ_NONE, t3, binding = 4);
RES(TexCube(float4), irradianceMap, UPDATE_FREQ_NONE, t4, binding = 5);
RES(TexCube(float4), specularMap, UPDATE_FREQ_NONE, t5, binding = 6);
RES(Tex2D(float4), shadowMap, UPDATE_FREQ_NONE, t12, binding = 12);

RES(SamplerState, bilinearSampler, UPDATE_FREQ_NONE, s12, binding = 13);
RES(SamplerState, bilinearClampedSampler, UPDATE_FREQ_NONE, s6, binding = 14);

// UPDATE_FREQ_PER_FRAME
CBUFFER(cbCamera, UPDATE_FREQ_PER_FRAME, b0, binding = 0)
{
	DATA(float4x4, projView[VR_MULTIVIEW_COUNT], None);
	DATA(float4x4, invProjView[VR_MULTIVIEW_COUNT], None);

	DATA(float4, camPos, None);

	DATA(float, fAmbientLightIntensity, None);

	DATA(int, bUseEnvironmentLight, None);
	DATA(float, fEnvironmentLightIntensity, None);
	DATA(float, fAOIntensity, None);
	DATA(int, renderMode, None);
	DATA(float, fNormalMapIntensity, None);
};

// UPDATE_FREQ_PER_DRAW
CBUFFER(cbObject, UPDATE_FREQ_PER_DRAW, b1, binding = 1)
{
	DATA(float4x4, worldMat, None);
	DATA(float4, albedoAndRoughness, None);
	DATA(float2, tiling, None);
	DATA(float, baseMetalness, None);

	// specifies which texture maps are to be used 
	// instead of the constant buffer values above.
	DATA(int, textureConfig, None);
};
RES(Tex2D(float4), albedoMap, UPDATE_FREQ_PER_DRAW, t7, binding = 7);
RES(Tex2D(float4), normalMap, UPDATE_FREQ_PER_DRAW, t8, binding = 8);
RES(Tex2D(float4), metallicMap, UPDATE_FREQ_PER_DRAW, t9, binding = 9);
RES(Tex2D(float4), roughnessMap, UPDATE_FREQ_PER_DRAW, t10, binding = 10);
RES(Tex2D(float4), aoMap, UPDATE_FREQ_PER_DRAW, t11, binding = 11);

#endif /* renderSceneBRDF_h */

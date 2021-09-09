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

#ifndef RESOURCES_H
#define RESOURCES_H

// basic + plane
#if defined(BASIC) || defined(FLOOR)

CBUFFER(cbPerPass, UPDATE_FREQ_PER_FRAME, b0, binding = 0)
{
	DATA(float4x4, projView[VR_MULTIVIEW_COUNT], None);
	DATA(float4x4, shadowLightViewProj, None);
	DATA(float4, camPos, None);
	DATA(float4, lightColor[4], None);
	DATA(float4, lightDirection[3], None);
};

STRUCT(GLTFTextureProperties)
{
	DATA(uint, mTextureSamplerIndex, None);
	DATA(int, mUVStreamIndex, None);
	DATA(float, mRotation, None);
	DATA(float, mValueScale, None);
	DATA(float2, mOffset, None);
	DATA(float2, mScale, None);
};

STRUCT(GLTFMaterialData)
{
	DATA(uint, mAlphaMode, None);
	DATA(float, mAlphaCutoff, None);
	DATA(float2, mEmissiveGBScale, None);

	DATA(float4, mBaseColorFactor, None);
	DATA(float4, mMetallicRoughnessFactors, None);

	DATA(GLTFTextureProperties, mBaseColorProperties, None);
	DATA(GLTFTextureProperties, mMetallicRoughnessProperties, None);

	DATA(GLTFTextureProperties, mNormalTextureProperties, None);
	DATA(GLTFTextureProperties, mOcclusionTextureProperties, None);
	DATA(GLTFTextureProperties, mEmissiveTextureProperties, None);
};

CBUFFER(cbMaterialData, UPDATE_FREQ_PER_DRAW, b0, binding = 0)
{
	DATA(GLTFMaterialData, materialData, None);
};
#if defined(BASIC)
PUSH_CONSTANT(cbRootConstants, b0)
{
	DATA(uint, nodeIndex, None);
};
#endif
RES(Buffer(float4x4), modelToWorldMatrices, UPDATE_FREQ_NONE, t0, binding = 0);

RES(Tex2D(float4), baseColorMap, UPDATE_FREQ_PER_DRAW, t0, binding = 1);
RES(Tex2D(float4), normalMap, UPDATE_FREQ_PER_DRAW, t1, binding = 2);
RES(Tex2D(float4), metallicRoughnessMap, UPDATE_FREQ_PER_DRAW, t2, binding = 3);
RES(Tex2D(float4), occlusionMap, UPDATE_FREQ_PER_DRAW, t3, binding = 4);
RES(Tex2D(float4), emissiveMap, UPDATE_FREQ_PER_DRAW, t4, binding = 5);
RES(Tex2D(float4), ShadowTexture, UPDATE_FREQ_NONE, t14, binding = 14);

RES(SamplerState, baseColorSampler, UPDATE_FREQ_PER_DRAW, s0, binding = 6);
RES(SamplerState, normalMapSampler, UPDATE_FREQ_PER_DRAW, s1, binding = 7);
RES(SamplerState, metallicRoughnessSampler, UPDATE_FREQ_PER_DRAW, s2, binding = 8);
RES(SamplerState, occlusionMapSampler, UPDATE_FREQ_PER_DRAW, s3, binding = 9);
RES(SamplerState, emissiveMapSampler, UPDATE_FREQ_PER_DRAW, s4, binding = 10);
RES(SamplerState, clampMiplessLinearSampler, UPDATE_FREQ_NONE, s7, binding = 7);

// FFXA + watermark + vignette
#else

RES(Tex2DArray(float4), sceneTexture, UPDATE_FREQ_NONE, t6, binding = 6);
RES(SamplerState, clampMiplessLinearSampler, UPDATE_FREQ_NONE, s7, binding = 7);

PUSH_CONSTANT(FXAARootConstant, b0)
{
	DATA(float2, ScreenSize, None);
	DATA(uint, Use, None);
	DATA(uint, padding00, None);
};

#endif

DECLARE_RESOURCES()

#endif
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

#ifndef _SHADER_DEFS_H
#define _SHADER_DEFS_H

#ifndef MAX_NUM_OBJECTS
	#define MAX_NUM_OBJECTS 128
#endif

#ifndef MAX_NUM_TEXTURES
	#define MAX_NUM_TEXTURES 7
#endif

#ifndef USE_SHADOWS
	#define USE_SHADOWS 1
#endif

#ifndef PT_USE_DIFFUSION
	#define PT_USE_DIFFUSION 1
#endif

#ifndef PT_USE_REFRACTION
	#define PT_USE_REFRACTION 1
#endif

#define UNIT_CBV_ID         register(b0, UPDATE_FREQ_NONE)
#define UNIT_CBV_OBJECT     register(b0, UPDATE_FREQ_PER_FRAME)
#define UNIT_CBV_CAMERA     register(b1, UPDATE_FREQ_PER_FRAME)
#define UNIT_CBV_MATERIAL   register(b2, UPDATE_FREQ_PER_FRAME)
#define UNIT_CBV_LIGHT      register(b3, UPDATE_FREQ_PER_FRAME)
#define UNIT_CBV_WBOIT      register(b4, UPDATE_FREQ_PER_FRAME)

#define UNIT_SRV_TEXTURES   register(t100, UPDATE_FREQ_NONE)
#define UNIT_SRV_DEPTH      register(t0, UPDATE_FREQ_NONE)
#define UNIT_SRV_VSM        register(t1, UPDATE_FREQ_NONE)
#define UNIT_SRV_VSM_R      register(t2, UPDATE_FREQ_NONE)
#define UNIT_SRV_VSM_G      register(t3, UPDATE_FREQ_NONE)
#define UNIT_SRV_VSM_B      register(t4, UPDATE_FREQ_NONE)

#define UNIT_SAMPLER_LINEAR register(s0, UPDATE_FREQ_NONE)
#define UNIT_SAMPLER_POINT  register(s1, UPDATE_FREQ_NONE)
#define UNIT_SAMPLER_VSM    register(s2, UPDATE_FREQ_NONE)

STRUCT(Material)
{
	DATA(float4, Color, None);
	DATA(float4, Transmission, None);
	DATA(float, RefractionRatio, None);
	DATA(float, Collimation, None);
	DATA(float2, Padding, None);
	DATA(uint, TextureFlags, None);
	DATA(uint, AlbedoTexID, None);
	DATA(uint, MetallicTexID, None);
	DATA(uint, RoughnessTexID, None);
	DATA(uint, EmissiveTexID, None);
#if defined(DIRECT3D12) || defined(ORBIS) || defined(PROSPERO)
	DATA(uint3, Padding2, None);
#endif
};

STRUCT(ObjectInfo)
{
	DATA(float4x4, toWorld, None);
	DATA(float4x4, normalMat, None);
	DATA(uint, matID, None);
#if defined(ORBIS) || defined(PROSPERO)
	DATA(float3, Padding, None);
#endif
};

// UPDATE_FREQ_NONE

RES(Tex2D(float4), VSM, UPDATE_FREQ_NONE, t1, binding = 2);
#if PT_USE_DIFFUSION != 0
RES(Tex2DArray(float4), DepthTexture, UPDATE_FREQ_NONE, t0, binding = 1);
#endif
#if PT_USE_CAUSTICS != 0
RES(Tex2D(float4), VSMRed, UPDATE_FREQ_NONE, t2, binding = 3);
RES(Tex2D(float4), VSMGreen, UPDATE_FREQ_NONE, t3, binding = 4);
RES(Tex2D(float4), VSMBlue, UPDATE_FREQ_NONE, t4, binding = 5);
#endif
RES(Tex2D(float4), MaterialTextures[MAX_NUM_TEXTURES], UPDATE_FREQ_NONE, t100, binding = 0);

#if PT_USE_DIFFUSION != 0
RES(SamplerState,  PointSampler, UPDATE_FREQ_NONE, s1, binding = 7);
#endif
RES(SamplerState,  LinearSampler, UPDATE_FREQ_NONE, s0, binding = 6);
RES(SamplerState,  VSMSampler, UPDATE_FREQ_NONE, s2, binding = 8);

// UPDATE_FREQ_PER_FRAME
CBUFFER(ObjectUniformBlock, UPDATE_FREQ_PER_FRAME, b0, binding = 0)
{
	DATA(ObjectInfo, objectInfo[MAX_NUM_OBJECTS], None);
};

CBUFFER(LightUniformBlock, UPDATE_FREQ_PER_FRAME, b3, binding = 3)
{
	DATA(float4x4, lightViewProj, None);
	DATA(float4, lightDirection, None);
	DATA(float4, lightColor, None);
};

CBUFFER(CameraUniform, UPDATE_FREQ_PER_FRAME, b1, binding = 1)
{
	DATA(float4x4, camViewProj[VR_MULTIVIEW_COUNT], None);
	DATA(float4x4, camViewMat, None);
	DATA(float4, camClipInfo, None);
	DATA(float4, camPosition, None);
};

CBUFFER(MaterialUniform, UPDATE_FREQ_PER_FRAME, b2, binding = 2)
{
	DATA(Material, Materials[MAX_NUM_OBJECTS], None);
};

CBUFFER(WBOITSettings, UPDATE_FREQ_PER_FRAME, b4, binding = 4)
{
 #ifdef VOLITION
	DATA(float, opacitySensitivity, None); // Should be greater than 1, so that we only downweight nearly transparent things. Otherwise, everything at the same depth should get equal weight. Can be artist controlled
	DATA(float, weightBias, None); //Must be greater than zero. Weight bias helps prevent distant things from getting hugely lower weight than near things, as well as preventing floating point underflow
	DATA(float, precisionScalar, None); //adjusts where the weights fall in the floating point range, used to balance precision to combat both underflow and overflow
	DATA(float, maximumWeight, None); //Don't weight near things more than a certain amount to both combat overflow and reduce the "overpower" effect of very near vs. very far things
	DATA(float, maximumColorValue, None);
	DATA(float, additiveSensitivity, None); //how much we amplify the emissive when deciding whether to consider this additively blended
	DATA(float, emissiveSensitivityValue, None); //artist controlled value between 0.01 and 1
 #else
 	DATA(float, colorResistance, None);	// Increase if low-coverage foreground transparents are affecting background transparent color.
 	DATA(float, rangeAdjustment, None);	// Change to avoid saturating at the clamp bounds.
 	DATA(float, depthRange, None);		// Decrease if high-opacity surfaces seem "too transparent", increase if distant transparents are blending together too much.
 	DATA(float, orderingStrength, None);	// Increase if background is showing through foreground too much.
 	DATA(float, underflowLimit, None);	// Increase to reduce underflow artifacts.
 	DATA(float, overflowLimit, None);	// Decrease to reduce overflow artifacts.
 #endif
};

// PUSH_CONSTANTS
PUSH_CONSTANT(DrawInfoRootConstant, b0)
{
	DATA(uint, baseInstance, None);
};

#endif

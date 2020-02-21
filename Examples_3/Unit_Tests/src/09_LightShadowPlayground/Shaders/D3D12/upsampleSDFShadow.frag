/*
* Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#include "SDF_Constant.h"


Texture2D<float2> SDFShadowTexture;
Texture2D<float> DepthTexture;
SamplerState clampMiplessLinearSampler : register(s0);
SamplerState clampMiplessNearSampler : register(s1);

cbuffer cameraUniformBlock : register(b0, UPDATE_FREQ_PER_FRAME)
{
    float4x4 View;
    float4x4 Project;
    float4x4 ViewProject;
    row_major float4x4 InvView;
	float4x4 InvProj;
	float4x4 InvViewProject;
	float4  mCameraPos;
	float mNear;
	float mFarNearDiff;
	float mFarNear;
	float paddingForAlignment0;
	float2 mTwoOverRes;
	float _pad1;
	float _pad2;
	float2 mWindowSize;
	float _pad3;
	float _pad4;
	float4 mDeviceZToWorldZ;
};

struct PsIn
{
	float4 Position  : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

struct PsOut
{
	float FinalColor : SV_Target0;
};


float ConvertFromDeviceZ(float deviceZ)
{
	return deviceZ * mDeviceZToWorldZ[0] + mDeviceZToWorldZ[1] + 1.0f / (deviceZ * mDeviceZToWorldZ[2] - mDeviceZToWorldZ[3]);
}

PsOut main(PsIn input)
{
	PsOut output;
	float2 UV = input.TexCoord.xy;
	
	float depthVal = DepthTexture.Sample(clampMiplessNearSampler, UV);
	float worldDepth = ConvertFromDeviceZ(depthVal);
	
	float2 downSampledTextureSize = floor(mWindowSize.xy / SDF_SHADOW_DOWNSAMPLE_VALUE);
	float2 downSampledTexelSize = 1.0f / downSampledTextureSize.xy;

	//get the top left corner of this UV coord
	float2 cornerUV = floor(UV.xy * downSampledTextureSize - 0.5f) / 
		downSampledTextureSize.xy + 0.5f * downSampledTexelSize;
	
	float2 billinearWeights = (UV.xy - cornerUV.xy) * downSampledTextureSize;

	float2 textureValues_00 = SDFShadowTexture.
		Sample(clampMiplessLinearSampler, cornerUV).xy;
	float2 textureValues_10 = SDFShadowTexture.
		Sample(clampMiplessLinearSampler, cornerUV + float2(downSampledTexelSize.x, 0.f)).xy;
	float2 textureValues_01 = SDFShadowTexture.
		Sample(clampMiplessLinearSampler, cornerUV + float2(0.f, downSampledTexelSize.y)).xy;
	float2 textureValues_11 = SDFShadowTexture.
		Sample(clampMiplessLinearSampler, cornerUV + downSampledTexelSize).xy;
	

	float4 cornerWeights = float4(  ( (1.f - billinearWeights.y) * (1.f - billinearWeights.x) ),
		(1.f - billinearWeights.y) * billinearWeights.x,
		(1.f - billinearWeights.x) * billinearWeights.y,
		(billinearWeights.x * billinearWeights.y) 
	);

	float epilson = pow(10.f, -4.f);

	float4 cornerDepths = abs(float4(textureValues_00.y, 
		textureValues_10.y, textureValues_01.y, textureValues_11.y));

	float4 depthWeights = 1.0f / ((abs(cornerDepths - worldDepth.xxxx) + epilson));
	float4 finalWeights = cornerWeights * depthWeights;

	float interpolatedResult = (
		finalWeights.x * textureValues_00.x + 
		finalWeights.y * textureValues_10.x + 
		finalWeights.z * textureValues_01.x + 
		finalWeights.w * textureValues_11.x);

	interpolatedResult /= dot(finalWeights, 1);

	float outputVal = interpolatedResult;
	
	output.FinalColor = outputVal;


	return output;
}
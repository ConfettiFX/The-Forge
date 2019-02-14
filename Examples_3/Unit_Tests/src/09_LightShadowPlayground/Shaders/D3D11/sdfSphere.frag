/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

cbuffer sdfUniformBlock
{
    float mShadowHardness : packoffset(c0);
    float mShadowHardnessRcp : packoffset(c0.y);
    uint  mMaxIteration : packoffset(c0.z);
};

cbuffer cameraUniformBlock : register(b0)
{
    row_major float4x4 View : packoffset(c0);
    row_major float4x4 Project : packoffset(c4);
    row_major float4x4 ViewProject : packoffset(c8);
    row_major float4x4 InvView : packoffset(c12);
    row_major float4x4 InvProj : packoffset(c16);
    row_major float4x4 InvViewProject : packoffset(c20);
    float4 NDCConversionConstants[2] : packoffset(c24);
    float Near : packoffset(c26);
    float FarNearDiff : packoffset(c26.y);
    float FarNear : packoffset(c26.z);
};

cbuffer renderSettingUniformBlock
{
    float4 WindowDimension : packoffset(c0);
};

cbuffer lightUniformBlock : register(b1)
{
    row_major float4x4 lightViewProj : packoffset(c0);
    float4 lightPosition : packoffset(c4);
    float4 lightColor : packoffset(c5);
    float4 lightUpVec : packoffset(c6);
    float  lightRange : packoffset(c7);
};

Texture2D<float> DepthBufferCopy;
SamplerState clampMiplessSampler;

struct PsIn
{
    nointerpolation float4 WorldObjectCenterAndRadius : TEXCOORD0;
    float4 Position : SV_Position;
    bool FrontFacing : SV_IsFrontFace;
};

struct PsOut
{
    float ShadowFactorOut : SV_Target0;
};

static float4 WorldObjectCenterAndRadius;

float linearizeZValue(float nonLinearValue)
{
    // Warning, reverse-Z is used!
     return FarNear/(Near+nonLinearValue*FarNearDiff);
}

float3 reconstructPosition(float3 windowCoords)
{
    float linearZ = linearizeZValue(windowCoords.z);
    float4 quasiPostProj = (NDCConversionConstants[0] + float4(windowCoords.xy * float2(2.0f, -2.0f), 0.0f, 0.0f)) * linearZ;
    return mul(quasiPostProj, InvViewProject).xyz + NDCConversionConstants[1].xyz;
}

float sdSphereLocal(float3 pos, float radius)
{
    return length(pos) - radius;
}

static const float stepEpsilon = 0.004;

// TODO: upgrade to deal with non-uniformly scaled spheres
float closestDistLocal(float3 pos)
{
    return sdSphereLocal(pos.xyz - WorldObjectCenterAndRadius.xyz, WorldObjectCenterAndRadius.w);
}

float calcSdfShadowFactor(float3 rayStart, float3 rayDir, float tMin, float tMax)
{
    float minDist = 9999999999.0;
	float t = tMin;
	for (uint i = 0; i < mMaxIteration; i++)
    {
		float h = closestDistLocal(rayStart + rayDir*t);
        if (h<minDist)
            minDist = h;
		t += h;
		if (minDist < stepEpsilon || t>tMax)
			break;
	}
	return max((minDist-stepEpsilon)*mShadowHardness,0.0);
}

[earlydepthstencil]
PsOut main(PsIn input)
{
    if (input.FrontFacing)
        discard;
	
    WorldObjectCenterAndRadius = input.WorldObjectCenterAndRadius;
	
    int2 i2uv = int2(input.Position.xy);
    float depth = DepthBufferCopy[i2uv];

    float2 f2uv = float2(i2uv) / WindowDimension.xy;
    // TODO: inverse transform these by object's world matrix and trace in model-space
    float3 position = reconstructPosition(float3(f2uv, depth));
    float3 lightVec = normalize(lightPosition.xyz - position);

    float dif = calcSdfShadowFactor(position, lightVec, mShadowHardnessRcp, lightRange);
	
    PsOut output;
    output.ShadowFactorOut = (4.0/3.0)*dif;
    return output;
}

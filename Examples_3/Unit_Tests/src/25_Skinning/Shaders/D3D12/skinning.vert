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

cbuffer uniformBlock : register(b0)
{
	float4x4 vpMatrix;
	float4x4 modelMatrix;
};

cbuffer boneMatrices : register(b1)
{
	float4x4 boneMatrix[MAX_NUM_BONES];
};

cbuffer boneOffsetMatrices : register(b2)
{
	float4x4 boneOffsetMatrix[MAX_NUM_BONES];
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
	float2 UV : TEXCOORD0;
	float4 BoneWeights : TEXCOORD1;
	uint4 BoneIndices : TEXCOORD2;
};

struct VSOutput {
	float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
	float2 UV : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput result;
	
	float4x4 boneTransform = mul(boneMatrix[input.BoneIndices[0]], boneOffsetMatrix[input.BoneIndices[0]]) * input.BoneWeights[0];
	boneTransform += mul(boneMatrix[input.BoneIndices[1]], boneOffsetMatrix[input.BoneIndices[1]]) * input.BoneWeights[1];
	boneTransform += mul(boneMatrix[input.BoneIndices[2]], boneOffsetMatrix[input.BoneIndices[2]]) * input.BoneWeights[2];
	boneTransform += mul(boneMatrix[input.BoneIndices[3]], boneOffsetMatrix[input.BoneIndices[3]]) * input.BoneWeights[3];
	
	result.Position = mul(boneTransform, float4(input.Position, 1.0f));
	result.Position = mul(modelMatrix, result.Position);
	result.Position = mul(vpMatrix, result.Position);
	result.Normal = normalize(mul(modelMatrix, float4(input.Normal, 0.0f)).xyz);
	result.UV = input.UV;

    return result;
}

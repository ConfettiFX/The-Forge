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

struct VSOutput
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
    float4 MiscData : TEXCOORD1;
};

SamplerState clampToEdgeNearSampler : register(s0);
Texture2D DepthPassTexture : register(t1);

float main(VSOutput input) : SV_TARGET
{
	const float2 samplerOffsets[16] =
    {
        float2( 1.0,  0.0), float2( 0.0,  1.0), float2( 1.0,  1.0), float2(-1.0, -1.0),
        float2( 0.0, -1.0), float2( 1.0, -1.0), float2(-1.0,  0.0), float2(-1.0,  1.0),

		float2( 0.5,  0.0), float2( 0.0,  0.5), float2( 0.5,  0.5), float2(-0.5, -0.5),
        float2( 0.0, -0.5), float2( 0.5, -0.5), float2(-0.5,  0.0), float2(-0.5,  0.5),
    };

	float maxZ = DepthPassTexture.SampleLevel(
		clampToEdgeNearSampler, input.UV, 0.0);

	
	for(int i = 0; i < 16; ++i)
	{
		maxZ = max(maxZ, DepthPassTexture.SampleLevel(clampToEdgeNearSampler, 
			input.UV + samplerOffsets[i] * input.MiscData.xy, 0));
	}
	
	//WARNING: MISC DATA IS SUPPOSE TO BE 0
	return maxZ - input.MiscData.z;
}
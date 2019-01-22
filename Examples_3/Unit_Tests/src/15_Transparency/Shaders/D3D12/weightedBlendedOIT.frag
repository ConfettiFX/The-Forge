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

#include "shading.hlsl"

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 WorldPosition : POSITION;
	float4 Normal : NORMAL;
	float4 UV : TEXCOORD0;
	uint MatID : MAT_ID;
};

struct PSOutput
{
	float4 Accumulation : SV_Target0;
	float Revealage : SV_Target1;
};

cbuffer WBOITSettings : register(b0)
{
	float colorResistance;	// Increase if low-coverage foreground transparents are affecting background transparent color.
	float rangeAdjustment;	// Change to avoid saturating at the clamp bounds.
	float depthRange;		// Decrease if high-opacity surfaces seem �too transparent�, increase if distant transparents are blending together too much.
	float orderingStrength;	// Increase if background is showing through foreground too much.
	float underflowLimit;	// Increase to reduce underflow artifacts.
	float overflowLimit;	// Decrease to reduce overflow artifacts.
};

float WeightFunction(float alpha, float depth)
{
	return pow(alpha, colorResistance) * clamp(0.3f / (1e-5f + pow(depth / depthRange, orderingStrength)), underflowLimit, overflowLimit);
}

PSOutput main(VSOutput input)
{    
	PSOutput output;

	float4 finalColor = Shade(input.MatID, input.UV.xy, input.WorldPosition.xyz, normalize(input.Normal.xyz));

	float d = input.Position.z / input.Position.w;
	float4 premultipliedColor = float4(finalColor.rgb * finalColor.a, finalColor.a);

	float w = WeightFunction(premultipliedColor.a, d);
	output.Accumulation = premultipliedColor * w;
	output.Revealage = premultipliedColor.a;

	return output;
}

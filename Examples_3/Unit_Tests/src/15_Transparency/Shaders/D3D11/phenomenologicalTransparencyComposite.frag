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

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 UV : TEXCOORD0;
};

SamplerState PointSampler : register(s0);
SamplerState LinearSampler : register(s1);
Texture2D AccumulationTexture : register(t0);
Texture2D ModulationTexture : register(t1);
Texture2D BackgroundTexture : register(t2);
#if PT_USE_REFRACTION != 0
Texture2D RefractionTexture : register(t3);
#endif

float MaxComponent(float3 v)
{
	return max(max(v.x, v.y), v.z);
}

float MinComponent(float3 v)
{
	return min(min(v.x, v.y), v.z);
}

float4 main(VSOutput input) : SV_Target
{
	float4 modulationAndDiffusion = ModulationTexture.Sample(PointSampler, input.UV.xy);
	float3 modulation = modulationAndDiffusion.rgb;

	if (MinComponent(modulation) == 1.0f)
		return BackgroundTexture.Sample(PointSampler, input.UV.xy);

	float4 accumulation = AccumulationTexture.Sample(PointSampler, input.UV.xy);

	// Handle overflow
	if (isinf(accumulation.a))
		accumulation.a = MaxComponent(accumulation.xyz);
	if (isinf(MaxComponent(accumulation.xyz)))
		accumulation = 1.0f;

	// Fake transmission by blending in the background color
	const float epsilon = 0.001f;
	accumulation.rgb *= 0.5f + max(modulation, epsilon) / (2.0f * max(epsilon, MaxComponent(modulation)));

#if PT_USE_REFRACTION != 0
	float2 delta = 3.0f * RefractionTexture.Sample(PointSampler, input.UV.xy).xy * (1.0f / 8.0f);
#else
	float2 delta = 0.0f;
#endif

	float3 background = 0.0f;

#if PT_USE_DIFFUSION != 0
	const float pixelDiffusion2 = 256.0f;
	float diffusion2 = modulationAndDiffusion.a * pixelDiffusion2;
	if (diffusion2 > 0)
	{
		int2 backgroundSize;
		BackgroundTexture.GetDimensions(backgroundSize.x, backgroundSize.y);

		float kernelRadius = min(sqrt(diffusion2), 32) * 2.0f;
		float mipLevel = max(log(kernelRadius) / log(2), 0.0f);
		if (kernelRadius <= 1)
			mipLevel = 0.0f;

		float2 offset = pow(2.0f, mipLevel) / (float2)backgroundSize;
		background += BackgroundTexture.SampleLevel(LinearSampler, input.UV.xy + delta, mipLevel).rgb * 0.5f;
		background += BackgroundTexture.SampleLevel(LinearSampler, input.UV.xy + delta + offset, mipLevel).rgb * 0.125f;
		background += BackgroundTexture.SampleLevel(LinearSampler, input.UV.xy + delta - offset, mipLevel).rgb * 0.125f;
		background += BackgroundTexture.SampleLevel(LinearSampler, input.UV.xy + delta + float2(offset.x, -offset.y), mipLevel).rgb * 0.125f;
		background += BackgroundTexture.SampleLevel(LinearSampler, input.UV.xy + delta + float2(-offset.x, offset.y), mipLevel).rgb * 0.125f;
	}
	else
	{
#endif
#if PT_USE_REFRACTION != 0
	background = BackgroundTexture.SampleLevel(LinearSampler, clamp(delta + input.UV.xy, 0.001f, 0.999f), 0.0f).rgb;
#else
	background = BackgroundTexture.SampleLevel(PointSampler, input.UV.xy, 0.0f).rgb;
#endif
#if PT_USE_DIFFUSION != 0
	}
#endif

	return float4(background * modulation + (1.0f - modulation) * accumulation.rgb / max(accumulation.a, 0.00001f), 1.0f);
}

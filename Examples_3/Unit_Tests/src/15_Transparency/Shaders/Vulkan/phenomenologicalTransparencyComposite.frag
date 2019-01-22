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

#version 450 core

layout(location = 0) in vec4 UV;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler PointSampler;
layout(set = 0, binding = 1) uniform sampler LinearSampler;
layout(set = 0, binding = 2) uniform texture2D AccumulationTexture;
layout(set = 0, binding = 3) uniform texture2D ModulationTexture;
layout(set = 0, binding = 4) uniform texture2D BackgroundTexture;
#if PT_USE_REFRACTION != 0
layout(set = 0, binding = 5) uniform texture2D RefractionTexture;
#endif

float MaxComponent(vec3 v)
{
	return max(max(v.x, v.y), v.z);
}

float MinComponent(vec3 v)
{
	return min(min(v.x, v.y), v.z);
}

void main()
{
	vec4 modulationAndDiffusion = texture(sampler2D(ModulationTexture, PointSampler), UV.xy);
	vec3 modulation = modulationAndDiffusion.rgb;

	if (MinComponent(modulation) == 1.0f)
	{
		FragColor = texture(sampler2D(BackgroundTexture, PointSampler), UV.xy);
		return;
	}

	vec4 accumulation = texture(sampler2D(AccumulationTexture, PointSampler), UV.xy);

	// Handle overflow
	if (isinf(accumulation.a))
		accumulation.a = MaxComponent(accumulation.xyz);
	if (isinf(MaxComponent(accumulation.xyz)))
		accumulation = vec4(1.0f);

	// Fake transmission by blending in the background color
	const float epsilon = 0.001f;
	accumulation.rgb *= 0.5f + max(modulation, epsilon) / (2.0f * max(epsilon, MaxComponent(modulation)));

#if PT_USE_REFRACTION != 0
	vec2 delta = 3.0f * texture(sampler2D(RefractionTexture, PointSampler), UV.xy).xy * (1.0f / 8.0f);
#else
	vec2 delta = vec2(0.0f);
#endif

	vec3 background = vec3(0.0f);

#if PT_USE_DIFFUSION != 0
	const float pixelDiffusion2 = 256.0f;
	float diffusion2 = modulationAndDiffusion.a * pixelDiffusion2;
	if (diffusion2 > 0)
	{
		ivec2 backgroundSize = textureSize(sampler2D(BackgroundTexture, PointSampler), 0);

		float kernelRadius = min(sqrt(diffusion2), 32) * 2.0f;
		float mipLevel = max(log(kernelRadius) / log(2), 0.0f);
		if (kernelRadius <= 1)
			mipLevel = 0.0f;

		vec2 offset = pow(2.0f, mipLevel) / vec2(backgroundSize);
		background += textureLod(sampler2D(BackgroundTexture, LinearSampler), UV.xy + delta, mipLevel).rgb * 0.5f;
		background += textureLod(sampler2D(BackgroundTexture, LinearSampler), UV.xy + delta + offset, mipLevel).rgb * 0.125f;
		background += textureLod(sampler2D(BackgroundTexture, LinearSampler), UV.xy + delta - offset, mipLevel).rgb * 0.125f;
		background += textureLod(sampler2D(BackgroundTexture, LinearSampler), UV.xy + delta + vec2(offset.x, -offset.y), mipLevel).rgb * 0.125f;
		background += textureLod(sampler2D(BackgroundTexture, LinearSampler), UV.xy + delta + vec2(-offset.x, offset.y), mipLevel).rgb * 0.125f;
	}
	else
	{
#endif
#if PT_USE_REFRACTION != 0
	background = textureLod(sampler2D(BackgroundTexture, LinearSampler), clamp(delta + UV.xy, vec2(0.001f), vec2(0.999f)), 0.0f).rgb;
#else
	background = textureLod(sampler2D(BackgroundTexture, PointSampler), UV.xy, 0.0f).rgb;
#endif
#if PT_USE_DIFFUSION != 0
	}
#endif

	FragColor = vec4(background * modulation + (1.0f - modulation) * accumulation.rgb / max(accumulation.a, 0.00001f), 1.0f);
}

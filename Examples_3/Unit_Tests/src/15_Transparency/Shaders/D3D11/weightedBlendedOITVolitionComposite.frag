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
	float4 UV : Texcoord0;
};

SamplerState PointSampler : register(s0);
Texture2D AccumulationTexture : register(t0);
Texture2D RevealageTexture : register(t1);

float4 main(VSOutput input) : SV_Target
{
	float revealage = 1.0;
	float additiveness = 0.0;
	float4 accum = float4(0.0,0.0,0.0,0.0);

	// high-res alpha
	float4 temp = RevealageTexture.Sample(PointSampler, input.UV.xy);
	revealage = temp.r;
	additiveness = temp.w;
	accum = AccumulationTexture.Sample(PointSampler, input.UV.xy);

	// weighted average (weights were applied during accumulation, and accum.a stores the sum of weights)
	float3 average_color = accum.rgb / max(accum.a, 0.00001);

	// Amplify based on additiveness to try and regain intensity we lost from averaging things that would formerly have been additive.
	// Revealage gives a rough estimate of how much "alpha stuff" there is in the pixel, allowing us to reduce the additive amplification when mixed in with non-additive
	float emissive_amplifier = (additiveness*8.0f); //The constant factor here must match the constant divisor in the material shaders!
	emissive_amplifier = lerp(emissive_amplifier*0.25, emissive_amplifier, revealage); //lessen, but do not completely remove amplification when there's opaque stuff mixed in

	// Also add in the opacity (1-revealage) to account for the fact that additive + non-additive should never be darker than the non-additive by itself
	emissive_amplifier += saturate((1.0-revealage)*2.0); //constant factor here is an adjustable thing to indicate how "sensitive" we should be to the presence of opaque stuff

	average_color *= max(emissive_amplifier,1.0); // NOTE: We max with 1 here so that this can only amplify, never darken, the result

	// Suppress overflow (turns INF into bright white)
	if (any(isinf(accum.rgb))) {
		average_color = 100.0f;
	}

	return float4(average_color, 1.0 - revealage);
}
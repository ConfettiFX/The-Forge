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
	float4 Revealage : SV_Target1;
};

cbuffer WBOITSettings : register(b0)
{
	float opacitySensitivity = 3.0; // Should be greater than 1, so that we only downweight nearly transparent things. Otherwise, everything at the same depth should get equal weight. Can be artist controlled
	float weightBias = 5.0; //Must be greater than zero. Weight bias helps prevent distant things from getting hugely lower weight than near things, as well as preventing floating point underflow
	float precisionScalar = 10000.0; //adjusts where the weights fall in the floating point range, used to balance precision to combat both underflow and overflow
	float maximumWeight = 20.0; //Don't weight near things more than a certain amount to both combat overflow and reduce the "overpower" effect of very near vs. very far things
	float maximumColorValue = 1000.0;
	float additiveSensitivity = 10.0; //how much we amplify the emissive when deciding whether to consider this additively blended
	float emissiveSensitivityValue = 0.5f; //artist controlled value between 0.01 and 1
};

void weighted_oit_process(out float4 accum, out float revealage, out float emissive_weight, float4 premultiplied_alpha_color, float raw_emissive_luminance, float view_depth, float current_camera_exposure)
{
	// Exposure changes relative importance of emissive luminance (whereas it does not for opacity)
	float relative_emissive_luminance = raw_emissive_luminance * current_camera_exposure;

	//Emissive sensitivity is hard to pin down
	//On the one hand, we want a low sensitivity so we don't get dark halos around "feathered" emissive alpha that overlap with eachother
	//On the other hand, we want a high sensitivity so that dim emissive holograms don't get overly downweighted.
	//We expose this to the artist to let them choose what is more important.
	const float emissive_sensitivity = 1.0/emissiveSensitivityValue;

	float clamped_emissive = saturate(relative_emissive_luminance);
	float clamped_alpha = saturate(premultiplied_alpha_color.a);

	// Intermediate terms to be cubed
	// NOTE: This part differs from McGuire's sample code:
	// since we're using premultiplied alpha in the composite, we want to
	// keep emissive values that have low coverage weighted appropriately
	// so, we'll add the emissive luminance to the alpha when computing the alpha portion of the weight
	// NOTE: We also don't add a small value to a, we allow it to go all the way to zero, so that completely invisible portions do not influence the result
	float a = saturate((clamped_alpha*opacitySensitivity) + (clamped_emissive*emissive_sensitivity));

	// NOTE: This differs from McGuire's sample code. In order to avoid having to tune the algorithm separately for different
	// near/far plane values, we produce a "canonical" depth value from the view-depth, using an fixed near plane and a tunable far plane
	const float canonical_near_z = 0.5;
	const float canonical_far_z = 300.0;
	float range = canonical_far_z-canonical_near_z;
	float canonical_depth = saturate(canonical_far_z/range - (canonical_far_z*canonical_near_z)/(view_depth*range));
	float b = 1.0 - canonical_depth;

	// clamp color to combat overflow (weight will be clamped too)
	float3 clamped_color = min(premultiplied_alpha_color.rgb, maximumColorValue);

	float w = precisionScalar * b * b * b; //basic depth based weight
	w += weightBias; //NOTE: This differs from McGuire's code. It is an alternate way to prevent underflow and limits near/far weight ratio
	w = min(w, maximumWeight); //clamp by maximum weight BEFORE multiplying by opacity weight (so that we'll properly reduce near faint stuff in weight)
	w *= a * a * a; //incorporate opacity weight as the last step

	accum = float4(clamped_color*w, w); //NOTE: This differs from McGuire's sample code because we want to be able to handle fully additive alpha (e.g. emissive), which has a coverage of 0 (revealage of 1.0)
	revealage = clamped_alpha; //blend state will invert this to produce actual revealage
	emissive_weight = saturate(relative_emissive_luminance*additiveSensitivity)/8.0f; //we're going to store this into an 8-bit channel, so we divide by the maximum number of additive layers we can support
}

PSOutput main(VSOutput input)
{    
	PSOutput output;

	float4 finalColor = Shade(input.MatID, input.UV.xy, input.WorldPosition.xyz, normalize(input.Normal.xyz));

	float d = input.Position.z / input.Position.w;
	float4 premultipliedColor = float4(finalColor.rgb * finalColor.a, finalColor.a);
	float emissiveLuminance = dot(finalColor.rgb, float3(0.2126f, 0.7152f, 0.0722f));

	output.Revealage = 0.0f;
	weighted_oit_process(output.Accumulation, output.Revealage.x, output.Revealage.w, premultipliedColor, emissiveLuminance, d, 1.0f);

	return output;
}

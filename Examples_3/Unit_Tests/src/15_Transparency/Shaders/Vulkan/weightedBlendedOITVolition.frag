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
#extension GL_GOOGLE_include_directive : require

#include "shading.glsl"

layout(location = 0) in vec4 WorldPosition;
layout(location = 1) in vec4 NormalOut;
layout(location = 2) in vec4 UV;
layout(location = 3) flat in uint MatID;

layout(location = 0) out vec4 Accumulation;
layout(location = 1) out vec4 Revealage;

layout(set = 0, binding = 20) uniform WBOITSettings
{
	float opacitySensitivity;		// Should be greater than 1, so that we only downweight nearly transparent things. Otherwise, everything at the same depth should get equal weight. Can be artist controlled
	float weightBias;				//Must be greater than zero. Weight bias helps prevent distant things from getting hugely lower weight than near things, as well as preventing floating point underflow
	float precisionScalar;			//adjusts where the weights fall in the floating point range, used to balance precision to combat both underflow and overflow
	float maximumWeight;			//Don't weight near things more than a certain amount to both combat overflow and reduce the "overpower" effect of very near vs. very far things
	float maximumColorValue;
	float additiveSensitivity;		//how much we amplify the emissive when deciding whether to consider this additively blended
	float emissiveSensitivityValue; //artist controlled value between 0.01 and 1
};

void weighted_oit_process(out vec4 accum, out float revealage, out float emissive_weight, vec4 premultiplied_alpha_color, float raw_emissive_luminance, float view_depth, float current_camera_exposure)
{
	// Exposure changes relative importance of emissive luminance (whereas it does not for opacity)
	float relative_emissive_luminance = raw_emissive_luminance * current_camera_exposure;

	//Emissive sensitivity is hard to pin down
	//On the one hand, we want a low sensitivity so we don't get dark halos around "feathered" emissive alpha that overlap with eachother
	//On the other hand, we want a high sensitivity so that dim emissive holograms don't get overly downweighted.
	//We expose this to the artist to let them choose what is more important.
	const float emissive_sensitivity = 1.0/emissiveSensitivityValue;

	float clamped_emissive = clamp(relative_emissive_luminance, 0, 1);
	float clamped_alpha = clamp(premultiplied_alpha_color.a, 0, 1);

	// Intermediate terms to be cubed
	// NOTE: This part differs from McGuire's sample code:
	// since we're using premultiplied alpha in the composite, we want to
	// keep emissive values that have low coverage weighted appropriately
	// so, we'll add the emissive luminance to the alpha when computing the alpha portion of the weight
	// NOTE: We also don't add a small value to a, we allow it to go all the way to zero, so that completely invisible portions do not influence the result
	float a = clamp((clamped_alpha*opacitySensitivity) + (clamped_emissive*emissive_sensitivity), 0, 1);

	// NOTE: This differs from McGuire's sample code. In order to avoid having to tune the algorithm separately for different
	// near/far plane values, we produce a "canonical" depth value from the view-depth, using an fixed near plane and a tunable far plane
	const float canonical_near_z = 0.5;
	const float canonical_far_z = 300.0;
	float range = canonical_far_z-canonical_near_z;
	float canonical_depth = clamp(canonical_far_z/range - (canonical_far_z*canonical_near_z)/(view_depth*range), 0, 1);
	float b = 1.0 - canonical_depth;

	// clamp color to combat overflow (weight will be clamped too)
	vec3 clamped_color = min(premultiplied_alpha_color.rgb, maximumColorValue);

	float w = precisionScalar * b * b * b; //basic depth based weight
	w += weightBias; //NOTE: This differs from McGuire's code. It is an alternate way to prevent underflow and limits near/far weight ratio
	w = min(w, maximumWeight); //clamp by maximum weight BEFORE multiplying by opacity weight (so that we'll properly reduce near faint stuff in weight)
	w *= a * a * a; //incorporate opacity weight as the last step

	accum = vec4(clamped_color*w, w); //NOTE: This differs from McGuire's sample code because we want to be able to handle fully additive alpha (e.g. emissive), which has a coverage of 0 (revealage of 1.0)
	revealage = clamped_alpha; //blend state will invert this to produce actual revealage
	emissive_weight = clamp(relative_emissive_luminance*additiveSensitivity, 0, 1)/8.0f; //we're going to store this into an 8-bit channel, so we divide by the maximum number of additive layers we can support
}

void main()
{    
	vec4 finalColor = Shade(MatID, UV.xy, WorldPosition.xyz, normalize(NormalOut.xyz));

	float d = gl_FragCoord.z / gl_FragCoord.w;
	vec4 premultipliedColor = vec4(finalColor.rgb * finalColor.a, finalColor.a);
	float emissiveLuminance = dot(finalColor.rgb, vec3(0.2126f, 0.7152f, 0.0722f));

	Revealage = vec4(0.0f);
	weighted_oit_process(Accumulation, Revealage.x, Revealage.w, premultipliedColor, emissiveLuminance, d, 1.0f);
}

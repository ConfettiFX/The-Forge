/*
 * Copyright (c) 2018 Kostas Anagnostou (https://twitter.com/KostasAAA).
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

precision highp float;
precision highp int; 

#define NUM_SHADOW_SAMPLES 4

const float NUM_SHADOW_SAMPLES_INV = 0.125;
const float shadowSamples[NUM_SHADOW_SAMPLES * 8] =
{
	-0.1746646, -0.7913184,
	0.08863912, -0.898169,
	0.1748409, -0.5252063,
	0.4529319, -0.384986,
	0.3857658, -0.9096935,
	0.768011, -0.4906538,
	0.6946555, 0.1605866,
	0.7986544, 0.5325912,
	0.2847693, 0.2293397,
	-0.4357893, -0.3808875,
	-0.139129, 0.2394065,
	0.4287854, 0.899425,
	-0.6924323, -0.2203967,
	-0.2611724, 0.7359962,
	-0.850104, 0.1263935,
	-0.5380967, 0.6264234
};

layout(location = 0) in vec3 fragInput_WorldPos;
layout(location = 1) in vec2 fragInput_TEXCOORD;
layout(location = 0) out vec4 rast_FragData0; 

layout(UPDATE_FREQ_NONE, binding = 14) uniform texture2D ShadowTexture;
layout(UPDATE_FREQ_NONE, binding = 7) uniform sampler clampMiplessLinearSampler;

layout(std140, UPDATE_FREQ_PER_FRAME, binding = 0) uniform cbPerPass
{
	mat4 projView;
	layout(column_major) mat4 shadowLightViewProj;
};

struct VSOutput
{
    vec4 Position;
	vec3 WorldPos;
    vec2 TexCoord;
};

float random(vec3 seed, vec3 freq)
{
	// project seed on random constant vector
	float dt = dot(floor(seed * freq), vec3(53.1215, 21.1352, 9.1322));
	// return only the fractional part
	return fract(sin(dt) * 2105.2354);
}

float CalcPCFShadowFactor(vec3 worldPos)
{
	vec4 posLS = shadowLightViewProj * vec4(worldPos.xyz, 1.0);
	posLS /= posLS.w;
	posLS.y *= -1;
	posLS.xy = posLS.xy * 0.5 + vec2(0.5, 0.5);


	vec2 HalfGaps = vec2(0.00048828125, 0.00048828125);
	vec2 Gaps = vec2(0.0009765625, 0.0009765625);

	posLS.xy += HalfGaps;

	float shadowFactor = 1.0;

		float shadowFilterSize = 0.0016;
		float angle = random(worldPos, vec3(20.0));
		float s = sin(angle);
		float c = cos(angle);

		for (int i = 0; i < NUM_SHADOW_SAMPLES; i++)
		{
			vec2 offset = vec2(shadowSamples[i * 2], shadowSamples[i * 2 + 1]);
			offset = vec2(offset.x * c + offset.y * s, offset.x * -s + offset.y * c);
			offset *= shadowFilterSize;			

			float shadowMapValue = texture(sampler2D(ShadowTexture, clampMiplessLinearSampler), posLS.xy + offset, 0).r;
			shadowFactor += (shadowMapValue - 0.002f > posLS.z ? 0.0f : 1.0f);
		}
		shadowFactor *= NUM_SHADOW_SAMPLES_INV;
		return shadowFactor;
}

vec4 HLSLmain(VSOutput input1)
{
	vec3 color = vec3(1.0, 1.0, 1.0);

	color *= CalcPCFShadowFactor(input1.WorldPos);

    float i = (float(1.0) - length(abs(((input1).TexCoord).xy)));
    (i = pow(i,float(1.20000004)));
    return vec4(color.r, color.g, color.b, i);
}
void main()
{
    VSOutput input1;
    input1.Position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
	input1.WorldPos = fragInput_WorldPos;
    input1.TexCoord = fragInput_TEXCOORD;
    vec4 result = HLSLmain(input1);
    rast_FragData0 = result;
}
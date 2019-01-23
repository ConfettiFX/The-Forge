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

// Shader for simple shading with a point light
// for planets in Unit Test 01 - Transformations

#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
	struct VSOutput
	{
		float4 pos [[position]];
		float3 color;
		float2 uv;
	};
	texture2d<float> uTexture0;
	sampler uSampler0;
	float4 main(VSOutput input)
	{
		float4 diffuse = uTexture0.sample(uSampler0, (input).uv);
		float lum = dot((diffuse).rgb, 0.333);
		((diffuse).rgb = (float3)(mix((diffuse).rgb, float3(lum).xxx, 0.8)));
		((diffuse).rgb *= ((input).color).rgb);
		return diffuse;
	};
	
	Fragment_Shader(
					texture2d<float> uTexture0,sampler uSampler0) :
	uTexture0(uTexture0),uSampler0(uSampler0) {}
};


fragment float4 stageMain(
						  Fragment_Shader::VSOutput input [[stage_in]],
						  texture2d<float> uTexture0 [[texture(0)]],
						  sampler uSampler0 [[sampler(0)]])
{
	Fragment_Shader::VSOutput input0;
	input0.pos = float4(input.pos.xyz, 1.0 / input.pos.w);
	input0.color = input.color;
	input0.uv = input.uv;
	Fragment_Shader main(
						 uTexture0,
						 uSampler0);
	return main.main(input0);
}

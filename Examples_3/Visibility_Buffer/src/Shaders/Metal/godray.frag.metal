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

#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
	struct PsIn
	{
		float4 position [[position]];
		float2 texCoord;
	};
	texture2d<float> uTex0;
	sampler uSampler0;
	struct Uniforms_RootConstantGodrayInfo
	{
		float exposure;
		float decay;
		float density;
		float weight;
		float2 lightPosInSS;
		uint NUM_SAMPLES;
	};
	constant Uniforms_RootConstantGodrayInfo & RootConstantGodrayInfo;
	float4 main(PsIn input)
	{
		float2 deltaTexCoord = float2((input.texCoord - RootConstantGodrayInfo.lightPosInSS.xy));
		float2 texCoord = input.texCoord;
		(deltaTexCoord *= (float2)((((const float)(1.0) / float(RootConstantGodrayInfo.NUM_SAMPLES)) * RootConstantGodrayInfo.density)));
		float illuminationDecay = 1.0;
		float4 result = float4(0.0, 0.0, 0.0, 0.0);
		for (int i = 0; ((uint)(i) < RootConstantGodrayInfo.NUM_SAMPLES); (i++))
		{
			(texCoord -= deltaTexCoord);
			float4 color = uTex0.sample(uSampler0, texCoord).rgba;
			(color *= (float4)((illuminationDecay * RootConstantGodrayInfo.weight)));
			(result += color);
			(illuminationDecay *= RootConstantGodrayInfo.decay);
		}
		(result *= (float4)(RootConstantGodrayInfo.exposure));
		return result;
	};
	
	Fragment_Shader(
					texture2d<float> uTex0,
					sampler uSampler0,
					constant Uniforms_RootConstantGodrayInfo & RootConstantGodrayInfo) :
					uTex0(uTex0),
					uSampler0(uSampler0),
					RootConstantGodrayInfo(RootConstantGodrayInfo) {}
};


fragment float4 stageMain(Fragment_Shader::PsIn input [[stage_in]],
						  texture2d<float> uTex0 [[texture(0)]],
						  sampler uSampler0 [[sampler(0)]],
						  constant Fragment_Shader::Uniforms_RootConstantGodrayInfo & RootConstantGodrayInfo [[buffer(0)]])
{
	Fragment_Shader::PsIn input0;
	input0.position = float4(input.position.xyz, 1.0 / input.position.w);
	input0.texCoord = input.texCoord;
	Fragment_Shader main(uTex0, uSampler0, RootConstantGodrayInfo);
	return main.main(input0);
}


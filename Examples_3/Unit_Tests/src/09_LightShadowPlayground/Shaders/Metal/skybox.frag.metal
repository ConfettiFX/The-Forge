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
	texturecube<float> skyboxTex;
	sampler skyboxSampler;
	struct VSinput
	{
		float4 Position;
	};
	struct VSOutput
	{
		float4 Position [[position]];
		float3 pos;
	};
	float4 main(VSOutput input)
	{
		float4 result = skyboxTex.sample(skyboxSampler, input.pos);
		return result;
	};
	
	Fragment_Shader(
					texturecube<float> skyboxTex,sampler skyboxSampler) :
	skyboxTex(skyboxTex),skyboxSampler(skyboxSampler) {}
};


fragment float4 stageMain(
						  Fragment_Shader::VSOutput input [[stage_in]],
						  texturecube<float> skyboxTex [[texture(0)]],
						  sampler skyboxSampler [[sampler(0)]])
{
	Fragment_Shader::VSOutput input0;
	input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
	input0.pos = input.pos;
	Fragment_Shader main(skyboxTex,
						 skyboxSampler);
	return main.main(input0);
}


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

cbuffer RootConstant
{
	float axis;
};

Texture2D Source : register(t0);
SamplerState LinearSampler : register(s0);

float4 main(VSOutput input) : SV_Target
{    
	const int StepCount = 2;
	const float Weights[StepCount] = { 0.44908f, 0.05092f };
	const float Offsets[StepCount] = { 0.53805f, 2.06278f };

	uint2 dim;
	Source.GetDimensions(dim[0], dim[1]);
	float2 stepSize = float2((1.0f - axis) / dim[0], axis / dim[1]);

	float4 output = 0.0f;

	[unroll] for (int i = 0; i < StepCount; ++i)
	{
		float2 offset = Offsets[i] * stepSize;
		output += Source.Sample(LinearSampler, input.UV.xy + offset) * Weights[i];
		output += Source.Sample(LinearSampler, input.UV.xy - offset) * Weights[i];
	}

	return output;
}

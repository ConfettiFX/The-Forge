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

layout(push_constant) uniform RootConstant_Block
{
	float axis;
} RootConstant;

layout(set = 0, binding = 0) uniform texture2D Source;
layout(set = 0, binding = 1) uniform sampler LinearSampler;

void main()
{    
	const int StepCount = 2;
	const float Weights[StepCount] = { 0.44908f, 0.05092f };
	const float Offsets[StepCount] = { 0.53805f, 2.06278f };

	ivec2 dim = textureSize(sampler2D(Source, LinearSampler), 0);
	vec2 stepSize = vec2((1.0f - RootConstant.axis) / dim[0], RootConstant.axis / dim[1]);

	FragColor = vec4(0.0f);

	for (int i = 0; i < StepCount; ++i)
	{
		vec2 offset = Offsets[i] * stepSize;
		FragColor += texture(sampler2D(Source, LinearSampler), UV.xy + offset) * Weights[i];
		FragColor += texture(sampler2D(Source, LinearSampler), UV.xy - offset) * Weights[i];
	}
}

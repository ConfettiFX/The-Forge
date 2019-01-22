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

layout (location = 0) in vec4 UV;

layout(location = 0) out vec4 FinalColor;

layout(set = 0, binding = 0) uniform sampler PointSampler;
layout(set = 0, binding = 1) uniform texture2D AccumulationTexture;
layout(set = 0, binding = 2) uniform texture2D RevealageTexture;

float MaxComponent(vec4 v)
{
	return max(max(max(v.x, v.y), v.z), v.w);
}

void main()
{
	float revealage = texture(sampler2D(RevealageTexture, PointSampler), UV.xy).r;
	if(revealage == 1.0f) discard;

	vec4 accumulation = texture(sampler2D(AccumulationTexture, PointSampler), UV.xy);
	if(isinf(MaxComponent(abs(accumulation))))
		accumulation.rgb = vec3(accumulation.a);

	vec3 averageColor = accumulation.rgb / max(accumulation.a, 0.00001f);
	FinalColor = vec4(averageColor, 1.0f - revealage);
}
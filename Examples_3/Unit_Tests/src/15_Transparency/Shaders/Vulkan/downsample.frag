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

layout(set = 0, binding = 0) uniform texture2D Source;
layout(set = 0, binding = 1) uniform sampler PointSampler;

void main()
{    
	vec4 total = vec4(0.0f);
	for (int x = 0; x < 4; ++x)
	{
		for(int y = 0; y < 4; ++y)
			total += texelFetch(sampler2D(Source, PointSampler), ivec2(gl_FragCoord.xy) * 4 + ivec2(x, y), 0);
	}

	FragColor =  total / 16.0f;
}

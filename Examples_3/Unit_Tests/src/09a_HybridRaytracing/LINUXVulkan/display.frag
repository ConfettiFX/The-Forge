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

//from https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;

	vec3 result = (x*(a*x + b)) / (x*(c*x + d) + e);

	return clamp(result, 0.0, 1.0);
}

layout(set = 0, binding = 0) uniform texture2D  inputRT;
layout(set = 0, binding = 1) uniform sampler   samplerPoint;

layout(location = 0) in vec2 texcoord;
layout(location = 0) out vec4 fs_out_color;

void main(void)
{
	vec3 colour = texture(sampler2D(inputRT, samplerPoint), texcoord).rgb;
		
	float exposure = 0.7;

	colour *= exposure;
	colour = ACESFilm(colour);

	colour = pow(colour, vec3(1 / 2.2));

	fs_out_color = vec4(colour,1);
}


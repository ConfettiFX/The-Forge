/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

/* Write your header comments here */
#version 450 core


layout(location = 0) in vec2 fragInput_TEXCOORD0;
layout(location = 0) out vec4 rast_FragData0; 

struct VSOutput
{
    vec4 Position;
    vec2 Tex_Coord;
};
layout(UPDATE_FREQ_NONE, binding = 1) uniform sampler clampNearSampler;
layout(UPDATE_FREQ_NONE, binding = 2) uniform texture2D screenTexture;

vec4 HLSLmain(VSOutput input1)
{
    //float rcolor = float ((texture(sampler2D( screenTexture, clampNearSampler), vec2((input1).Tex_Coord))).x);
	vec3 rcolor = vec3 (
		texture(sampler2D( screenTexture, clampNearSampler), input1.Tex_Coord)
	);
    //vec3 color = vec3(rcolor, rcolor, rcolor);
	vec3 color = vec3(rcolor);
    return vec4(color, 1.0);
}
void main()
{
    VSOutput input1;
    input1.Position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    input1.Tex_Coord = fragInput_TEXCOORD0;
    vec4 result = HLSLmain(input1);
    rast_FragData0 = result;
}

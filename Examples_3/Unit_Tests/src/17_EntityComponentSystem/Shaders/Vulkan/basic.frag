#version 450 core

/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

layout(location = 0) in vec3 fragInput_COLOR0;
layout(location = 1) in vec2 fragInput_TEXCOORD0;
layout(location = 0) out vec4 rast_FragData0; 

struct VSOutput
{
    vec4 pos;
    vec3 color;
    vec2 uv;
};
layout(UPDATE_FREQ_NONE, binding = 2) uniform texture2D uTexture0;
layout(UPDATE_FREQ_NONE, binding = 3) uniform sampler uSampler0;
vec4 HLSLmain(VSOutput input0)
{
    vec4 diffuse = texture(sampler2D( uTexture0, uSampler0), vec2((input0).uv));
    float lum = dot((diffuse).rgb, vec3 (0.333));
    ((diffuse).rgb = vec3 (mix(vec3 ((diffuse).rgb), vec3 ((lum).xxx), vec3 (0.8))));
    ((diffuse).rgb *= ((input0).color).rgb);
    return diffuse;
}
void main()
{
    VSOutput input0;
    input0.pos = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    input0.color = fragInput_COLOR0;
    input0.uv = fragInput_TEXCOORD0;
    vec4 result = HLSLmain(input0);
    rast_FragData0 = result;
}
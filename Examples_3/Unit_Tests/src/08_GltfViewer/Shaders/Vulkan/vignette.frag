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

precision highp float;
precision highp int; 

layout(location = 0) in vec2 fragInput_TEXCOORD;
layout(location = 0) out vec4 rast_FragData0; 

layout(UPDATE_FREQ_NONE, binding = 11) uniform texture2D sceneTexture;
layout(UPDATE_FREQ_NONE, binding = 16) uniform sampler clampMiplessLinearSampler;

layout(row_major, UPDATE_FREQ_PER_FRAME, binding = 15) uniform cbPerFrame
{
    mat4 worldMat;
    mat4 projViewMat;
    vec4 screenSize;
};

struct VSOutput
{
    vec4 Position;
    vec2 TexCoord;
};
vec4 HLSLmain(VSOutput input1)
{
    vec4 src = texture(sampler2D( sceneTexture, clampMiplessLinearSampler), vec2((input1).TexCoord));
    if(((screenSize).a > float(0.5)))
    {
        vec2 uv = (input1).TexCoord;
        vec2 coord = (((uv - vec2(0.5)) * vec2(((screenSize).x / (screenSize).y))) * vec2(2.0));
        float rf = (sqrt(dot(coord, coord)) * float(0.2));
        float rf2_1 = ((rf * rf) + float(1.0));
        float e = (float(1.0) / (rf2_1 * rf2_1));
        return vec4(((src).rgb * vec3(e)), 1.0);
    }
    else
    {
        return vec4((src).rgb, 1.0);
    }
}
void main()
{
    VSOutput input1;
    input1.Position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    input1.TexCoord = fragInput_TEXCOORD;
    vec4 result = HLSLmain(input1);
    rast_FragData0 = result;
}
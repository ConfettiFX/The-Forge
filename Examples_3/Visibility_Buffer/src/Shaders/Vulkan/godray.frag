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
#if !defined(WINDOWS) && !defined(ANDROID) && !defined(LINUX)
#define WINDOWS 	// Assume windows if no platform define has been added to the shader
#endif



layout(location = 0) in vec2 fragInput_TEXCOORD;
layout(location = 0) out vec4 rast_FragData0; 

struct PsIn
{
    vec4 position;
    vec2 texCoord;
};
layout(set = 0, binding = 0) uniform texture2D uTex0;
layout(set = 0, binding = 1) uniform sampler uSampler0;
layout(push_constant) uniform RootConstantGodrayInfo_Block
{
    float exposure;
    float decay;
    float density;
    float weight;
    vec2 lightPosInSS;
    uint NUM_SAMPLES;
}RootConstantGodrayInfo;

vec4 HLSLmain(PsIn input0)
{
    vec2 deltaTexCoord = vec2(((input0).texCoord - (RootConstantGodrayInfo.lightPosInSS).xy));
    vec2 texCoord = (input0).texCoord;
    (deltaTexCoord *= vec2 (((float (1.0) / float(RootConstantGodrayInfo.NUM_SAMPLES)) * RootConstantGodrayInfo.density)));
    float illuminationDecay = float (1.0);
    vec4 result = vec4(0.0, 0.0, 0.0, 0.0);
    for (int i = 0; (uint (i) < RootConstantGodrayInfo.NUM_SAMPLES); (i++))
    {
        (texCoord -= deltaTexCoord);
        vec4 color = (texture(sampler2D( uTex0, uSampler0), vec2(texCoord))).rgba;
        (color *= vec4 ((illuminationDecay * RootConstantGodrayInfo.weight)));
        (result += color);
        (illuminationDecay *= RootConstantGodrayInfo.decay);
    }
    (result *= vec4 (RootConstantGodrayInfo.exposure));
    return result;
}
void main()
{
    PsIn input0;
    input0.position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    input0.texCoord = fragInput_TEXCOORD;
    vec4 result = HLSLmain(input0);
    rast_FragData0 = result;
}

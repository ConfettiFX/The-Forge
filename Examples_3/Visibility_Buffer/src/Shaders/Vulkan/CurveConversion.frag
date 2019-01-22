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
layout(set = 0, binding = 0) uniform texture2D SceneTex;
layout(set = 0, binding = 1) uniform sampler uSampler0;
layout(set = 0, binding = 2) uniform texture2D GodRayTex;
vec4 HLSLmain(PsIn In)
{
    vec4 sceneColor = texture(sampler2D( SceneTex, uSampler0), vec2((In).texCoord));
    ((sceneColor).rgb += vec3 ((texture(sampler2D( GodRayTex, uSampler0), vec2((In).texCoord))).rgb));
    return vec4((sceneColor).rgb, 1.0);
}
void main()
{
    PsIn In;
    In.position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    In.texCoord = fragInput_TEXCOORD;
    vec4 result = HLSLmain(In);
    rast_FragData0 = result;
}

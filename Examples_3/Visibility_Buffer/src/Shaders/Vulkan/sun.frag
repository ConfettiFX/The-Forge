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



layout(location = 0) out vec4 rast_FragData0; 

struct PsIn
{
    vec4 Position;
};

layout(set = 0, binding = 0) uniform SunMatrices_Block
{
	mat4 projView;
	mat4 modelMat;
	vec4 mLightColor;
}UniformBufferSunMatrices;

vec4 HLSLmain(PsIn In)
{
    return vec4(((UniformBufferSunMatrices.mLightColor).rgb * vec3 ((UniformBufferSunMatrices.mLightColor).a)), 1.0);
}
void main()
{
    PsIn In;
    In.Position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    vec4 result = HLSLmain(In);
    rast_FragData0 = result;
}

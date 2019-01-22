#version 450 core
#if !defined(WINDOWS) && !defined(ANDROID) && !defined(LINUX)
#define WINDOWS 	// Assume windows if no platform define has been added to the shader
#endif


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

// USERMACRO: SAMPLE_COUNT [1,2,4]

layout(set=0, binding=0) uniform texture2DMS msaaSource;
layout(set=0, binding=1) uniform sampler dummySampler;

layout(location=0) out vec4 baseOut;
void main()
{
    vec4 value = vec4(0,0,0,0);

    ivec2 texCoord = ivec2(gl_FragCoord.xy);
    for(int i = 0; i < SAMPLE_COUNT; ++i)
        value += texelFetch(sampler2DMS(msaaSource, dummySampler), texCoord, i);
    value /= SAMPLE_COUNT;

    baseOut = value; 
}

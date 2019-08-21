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



#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "Shader_Defs.h"
#include "Packing.h"

layout (set = 1, binding = 0) uniform objectUniformBlock
{
	mat4 WorldViewProjMat;
    mat4 WorldMat;
};

layout(location = 0) in vec3 iPosition;
#ifdef WINDOWS
layout(location = 1) in uint iTexCoord;
#elif defined(LINUX)
layout(location = 1) in vec2 iTexCoord;
#endif

layout(location = 0) out vec2 oTexCoord;
layout(location = 1) out flat uint oDrawId;

void main()
{
	uint drawId = gl_DrawIDARB;
	gl_Position = WorldViewProjMat * vec4(iPosition, 1);
#ifdef WINDOWS
	oTexCoord = unpack2Floats(iTexCoord);
#elif defined(LINUX)
	oTexCoord = iTexCoord;
#endif
	oDrawId = drawId;
}

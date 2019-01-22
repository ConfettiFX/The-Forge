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

#include "shader_defs.h"
#include "packing.h"

layout(set = 0, binding = 0) uniform uniforms
{
	PerFrameConstants uniformsData;
};

layout(location = 0) in vec3 iPosition;
#ifdef WINDOWS
layout(location = 1) in uint iTexCoord;
layout(location = 2) in uint iNormal;
layout(location = 3) in uint iTangent;
#elif defined(LINUX)
layout(location = 1) in vec2 iTexCoord;
layout(location = 2) in vec3 iNormal;
layout(location = 3) in vec3 iTangent;
#endif

layout(location = 0) out vec2 oTexCoord;
layout(location = 1) out vec3 oNormal;
layout(location = 2) out vec3 oTangent;
layout(location = 3) out flat uint oDrawId;

void main()
{
	uint drawId = gl_DrawIDARB;
	gl_Position = uniformsData.transform[VIEW_CAMERA].mvp * vec4(iPosition, 1);
#ifdef WINDOWS
	oTexCoord = unpack2Floats(iTexCoord);
	oNormal = decodeDir(unpackUnorm2x16(iNormal));
	oTangent = decodeDir(unpackUnorm2x16(iTangent));
#elif defined(LINUX)
	oTexCoord = (iTexCoord);
	oNormal = ((iNormal));
	oTangent = ((iTangent));
#endif
	oDrawId = drawId;
}


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

#include "shader_defs.h"

layout(std140, set = 0, binding = 0) uniform uniforms
{
	PerFrameConstants uniformsData;
};

layout(std430, set = 0, binding = 1) restrict readonly buffer lights
{
	LightData lightsBuffer[];
};

layout(location = 0) in vec4 iPos;

layout(location = 0) out vec3 oLightColor;
layout(location = 1) out vec3 oLightPos;

void main()
{
	uint instanceId = gl_InstanceIndex;
	oLightPos = vec3(lightsBuffer[instanceId].position.x, lightsBuffer[instanceId].position.y, lightsBuffer[instanceId].position.z);
	oLightColor = vec3(lightsBuffer[instanceId].color.r, lightsBuffer[instanceId].color.g, lightsBuffer[instanceId].color.b);
	gl_Position = (uniformsData.transform[VIEW_CAMERA].mvp * vec4((iPos.xyz * LIGHT_SIZE) + oLightPos, 1));
}

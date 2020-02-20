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


// Shader for simple shading with a point light
// for planets in Unit Test 12 - Transformations

#define MAX_PLANETS 20
#define PI 3.1415926535897932384626433832795
#define PI_2 6.283185307179586476925286766559

layout(location = 0) in vec4 Position;
layout(location = 1) in vec4 Normal;
layout(location = 2) in vec2 UV;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outPos;

layout (set = 0, binding = 9) uniform SparseTextureInfo
{
	uint Width;
	uint Height;
	uint pageWidth;
	uint pageHeight;

	uint DebugMode;
	uint ID;
	uint pad1;
	uint pad2;
	
	vec4 CameraPos;
};

layout (std140, set = 1, binding=0) uniform uniformBlock {
	uniform mat4 mvp;
  uniform mat4 toWorld[MAX_PLANETS];
  uniform vec4 color[MAX_PLANETS];

  // Point Light Information
  uniform vec3 lightPosition;
  uniform vec3 lightColor;
};

void main ()
{
	mat4 tempMat = mvp * toWorld[ID];
	gl_Position = tempMat * vec4(Position.xyz, 1.0f);
	
	vec4 normal = normalize(toWorld[ID] * vec4(Normal.xyz, 0.0f));
	vec4 pos = toWorld[ID] * vec4(Position.xyz, 1.0f);

  outUV = UV;
  outNormal = normal.xyz;
  outPos = pos.xyz;
}

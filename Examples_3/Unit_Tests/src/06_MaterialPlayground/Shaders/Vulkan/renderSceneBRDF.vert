#version 450 core

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


layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 UV;


layout(set = 0, binding = 0) uniform cbCamera 
{
	mat4 projView;
	mat4 invProjView;
	vec3 camPos;
	float _dumm;
	int bUseEnvironmentLight;
};

layout(set = 0, binding = 1) uniform cbObject
{
	mat4 worldMat;
	vec3 albedo;
	float roughness;
	vec2 tiling;
	float metalness;
	int textureConfig;
};

layout(location = 0) out vec3 normal;
layout(location = 1) out vec3 pos;
layout(location = 2) out vec2 uv;


void main ()
{
	gl_Position =  projView * worldMat * vec4(Position.xyz, 1.0f);
	
	normal = vec3(normalize(worldMat * vec4(Normal.xyz, 0.0f)));
	pos = vec3(worldMat * vec4(Position.xyz, 1.0f));
	uv = UV;
}


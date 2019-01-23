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

//--------------------------------------------------------------------------------------------
//
// Copyright (C) 2009 - 2016 Confetti Interactive Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without expressly written permission
// from Confetti Interactive Inc.
//
//--------------------------------------------------------------------------------------------

#version 450 core

layout (location = 0) in vec4 position;
layout (location = 1) in vec4 normal;

layout (location = 0) out vec4 position_out;
layout (location = 1) out vec3 normal_out;
layout (location = 2) out vec3 posModel;
layout (location = 3) out vec4 albedo;

struct AsteroidDynamic
{
	mat4 transform;
    uint indexStart;
    uint indexEnd;
    uint padding[2];
};

struct AsteroidStatic
{
	vec4 rotationAxis;
	vec4 surfaceColor;
    vec4 deepColor;

    float scale;
	float orbitSpeed;
	float rotationSpeed;

    uint textureID;
    uint vertexStart;
    uint padding[3];
};

layout (std140, set=0, binding=5) uniform uniformBlock
{
    uniform mat4 viewProj;
};

layout (std430, set=0, binding=1) buffer asteroidsStatic
{
    AsteroidStatic asteroidsStaticBuffer[];
};

layout (std430, set=0, binding=2) buffer asteroidsDynamic
{
    AsteroidDynamic asteroidsDynamicBuffer[];
};

void main()
{
    AsteroidStatic asteroidStatic = asteroidsStaticBuffer[gl_InstanceIndex];
    AsteroidDynamic asteroidDynamic = asteroidsDynamicBuffer[gl_InstanceIndex];

    mat4 worldMatrix = asteroidDynamic.transform;
    position_out = viewProj * worldMatrix * vec4(position.xyz, 1.0);
    gl_Position = position_out;

    posModel = position.xyz;
    normal_out = mat3(worldMatrix) * normal.xyz;

    float depth = clamp((length(position.xyz) - 0.5f) / 0.2, 0.0, 1.0);
    albedo.xyz = mix(asteroidStatic.deepColor.xyz, asteroidStatic.surfaceColor.xyz, depth);
    albedo.w = float(asteroidStatic.textureID);
}

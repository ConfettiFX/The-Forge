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
// Copyright (C) 2009 - 2017 Confetti Interactive Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without expressly written permission
// from Confetti Interactive Inc.
//
//--------------------------------------------------------------------------------------------
#version 450 core

struct InstanceData
{
    mat4 mvp;
    mat4 normalMat;
    vec4 surfaceColor;
    vec4 deepColor;
    int textureID;
	uint _pad0[3];
};

layout (std430, set=0, binding=0) buffer instanceBuffer
{
	InstanceData instances[];
};

layout(push_constant) uniform pushConstantBlock
{
	uniform uint instanceIndex;
} rootConstant;

layout (location = 0) in vec4 position;
layout (location = 1) in vec4 normal;

layout (location = 0) out vec4 positionOut;
layout (location = 1) out vec3 normalOut;
layout (location = 2) out vec3 posModel;
layout (location = 3) out vec4 albedo;

float linstep(float minVal, float maxVal, float s)
{
    return clamp((s - minVal) / (maxVal - minVal), 0.0, 1.0);
}

void main()
{
    positionOut = instances[rootConstant.instanceIndex].mvp * vec4(position.xyz, 1);
    gl_Position = positionOut;
    posModel = position.xyz;
    normalOut = normalize((instances[rootConstant.instanceIndex].normalMat * vec4(normal.xyz, 0)).xyz);

    float depth = linstep(0.5f, 0.7f, length(position.xyz));
    albedo.xyz = mix(instances[rootConstant.instanceIndex].deepColor.xyz, instances[rootConstant.instanceIndex].surfaceColor.xyz, depth);
    albedo.w = float(instances[rootConstant.instanceIndex].textureID);
}
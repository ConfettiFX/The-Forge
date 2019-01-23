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

layout (location = 0) in vec4 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 posModel;
layout (location = 3) in vec4 albedo;

layout (location = 0) out vec4 outColor;

layout (set=0, binding=1) uniform texture2DArray uTex0;
layout (set=0, binding=2) uniform sampler uSampler0;

void main()
{
    const vec3 lightDir = -normalize(vec3(2,-6,-1));

    float wrap_diffuse = clamp(dot(lightDir, normalize(normal)), 0.0, 1.0);
    float light = 2.0f * wrap_diffuse + 0.06f;

    vec3 uvw = posModel * 0.5 + 0.5;

    vec3 blendWeights = abs(normalize(posModel));
    blendWeights = clamp((blendWeights - 0.2) * 7, 0.0, 1.0);
    blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z).xxx;

    vec3 coord1 = vec3(uvw.yz, albedo.w * 3 + 0);
    vec3 coord2 = vec3(uvw.zx, albedo.w * 3 + 1);
    vec3 coord3 = vec3(uvw.xy, albedo.w * 3 + 2);

    vec3 texColor = vec3(0,0,0);
    texColor += blendWeights.x * (texture(sampler2DArray(uTex0, uSampler0), coord1)).xyz;
    texColor += blendWeights.y * (texture(sampler2DArray(uTex0, uSampler0), coord2)).xyz;
    texColor += blendWeights.z * (texture(sampler2DArray(uTex0, uSampler0), coord3)).xyz;

	float coverage = clamp(position.z * 4000.0f, 0.0, 1.0);

    vec3 color = albedo.xyz;
    color *= light;
    color *= texColor * 2;
	color *= coverage;

    outColor = vec4(color, 1);
}

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


layout(location = 0) in vec3 iNormal;
layout(location = 1) in vec2 iUV;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 3) uniform texture2D DiffuseTexture;
layout(set = 0, binding = 4) uniform sampler DefaultSampler;

void main ()
{
	float nDotl = clamp((dot(normalize(iNormal), vec3(0, 1, 0)) + 1.0f) * 0.5f, 0.0f, 1.0f);
	vec3 color = texture(sampler2D(DiffuseTexture, DefaultSampler), iUV).rgb;
	FragColor = vec4(color * nDotl, 1.0f);
}
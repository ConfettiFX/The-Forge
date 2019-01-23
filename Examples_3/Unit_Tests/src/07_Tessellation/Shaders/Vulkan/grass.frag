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

#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform GrassUniformBlock {

  	mat4 world;
	mat4 view;	
	mat4 invView;
	mat4 proj;
	mat4 viewProj;

	float deltaTime;
	float totalTime;
	
	int gWindMode;
	int gMaxTessellationLevel;

 	float windSpeed;
 	float windWidth;
 	float windStrength;

};

layout(location = 0) in vec4 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec3 WindDirection;
layout(location = 3) in vec2 UV;

layout(location = 0) out vec4 outColor;

void main() {
   	vec3 upperColor = vec3(0.0,0.9,0.1);
	vec3 lowerColor = vec3(0.0,0.2,0.1);

	vec3 sunDirection = normalize(vec3(-1.0, 5.0, -3.0));

	float NoL = clamp(dot(Normal, sunDirection), 0.1, 1.0);

	vec3 mixedColor = mix(lowerColor, upperColor, UV.y);

     outColor = vec4(mixedColor*NoL, 1.0);
}

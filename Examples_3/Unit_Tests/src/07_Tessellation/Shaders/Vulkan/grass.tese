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

layout(quads, equal_spacing, ccw) in;

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

layout(location = 0) patch in vec4 tese_v1;
layout(location = 1) patch in vec4 tese_v2;
layout(location = 2) patch in vec4 tese_up;
layout(location = 3) patch in vec4 tese_widthDir;

layout(location = 0) out vec4 Position;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec3 WindDirection;
layout(location = 3) out vec2 UV;

void main() {
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

	mat4 viewProj = proj * view;
	
	//6.3 Blade Geometry	
	
	vec3 a = gl_in[0].gl_Position.xyz + v*(tese_v1.xyz -  gl_in[0].gl_Position.xyz);
	vec3 b = tese_v1.xyz + v*(tese_v2.xyz - tese_v1.xyz);
	vec3 c = a + v*(b - a);

	vec3 t1 = tese_widthDir.xyz; //bitangent
	vec3 wt1 = t1 * tese_v2.w * 0.5;

    vec3 c0 = c - wt1;
	vec3 c1 = c + wt1;

    vec3 t0 = normalize(b - a);
	Normal = normalize(cross(t1, t0));

    UV = vec2(u, v);

	//triagnle shape
	float t = u + 0.5*v -u*v;
    Position.xyz = (1.0 - t)*c0 + t*c1;
	Position = viewProj * vec4(Position.xyz, 1.0);

    gl_Position = Position;

	//for Debug
	Position.w = tese_widthDir.w;

	WindDirection = tese_widthDir.xyz;

}

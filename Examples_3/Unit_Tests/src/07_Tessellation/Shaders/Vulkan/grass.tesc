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

layout(vertices = 1) out;

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


layout(location = 0) in vec4 tesc_v1[];
layout(location = 1) in vec4 tesc_v2[];
layout(location = 2) in vec4 tesc_up[];
layout(location = 3) in vec4 tesc_widthDir[];

layout(location = 0) patch out vec4 tese_v1;
layout(location = 1) patch out vec4 tese_v2;
layout(location = 2) patch out vec4 tese_up;
layout(location = 3) patch out vec4 tese_widthDir;


void main() {

	vec4 WorldPosV0 = gl_in[gl_InvocationID].gl_Position;
    gl_out[gl_InvocationID].gl_Position = WorldPosV0;


	tese_v1 = tesc_v1[0];
    tese_v2 = tesc_v2[0];
    tese_up = tesc_up[0];
    tese_widthDir = tesc_widthDir[0];

	float near = 0.1;
	float far = 25.0;

	float depth = -(view * WorldPosV0).z / (far - near);
	depth = clamp(depth, 0.0, 1.0);

	float minLevel = 1.0;

	depth = depth*depth;

	float level = mix(float(gMaxTessellationLevel), minLevel, depth);

	tese_widthDir.w = depth;

    gl_TessLevelInner[0] = 1.0; //horizontal
    gl_TessLevelInner[1] = level; //vertical
    
	gl_TessLevelOuter[0] = level; //vertical
    gl_TessLevelOuter[1] = 1.0; //horizontal
    gl_TessLevelOuter[2] = level; //vertical
    gl_TessLevelOuter[3] = 1.0; //horizontal

	
}
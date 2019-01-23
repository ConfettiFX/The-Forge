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


#version 450
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

layout(location = 0) in vec4 v0;
layout(location = 1) in vec4 v1;
layout(location = 2) in vec4 v2;
layout(location = 3) in vec4 up;

layout(location = 0) out vec4 tesc_v1;
layout(location = 1) out vec4 tesc_v2;
layout(location = 2) out vec4 tesc_up;
layout(location = 3) out vec4 tesc_widthDir;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
	
	vec4 V0 = world * vec4(v0.xyz, 1.0);
	tesc_v1 = vec4((world * vec4(v1.xyz, 1.0)).xyz, v1.w);
	tesc_v2 = vec4((world * vec4(v2.xyz, 1.0)).xyz, v2.w);
	
	tesc_up.xyz = normalize(up.xyz);

	float theta = v0.w;
	float sinTheta = sin(theta);
	float cosTheta = cos(theta);

	vec3 faceDir = normalize(cross(tesc_up.xyz, vec3(sinTheta, 0, cosTheta)));

	tesc_widthDir.xyz = normalize( cross(faceDir, tesc_up.xyz));

	//For debug
	tesc_widthDir.w = v1.w * 0.4;

	gl_Position = V0;
}

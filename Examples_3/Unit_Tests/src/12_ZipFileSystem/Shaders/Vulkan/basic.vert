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

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 UV;

layout(location = 0) out vec4 Color;

layout (std140, set=1, binding=0) uniform uniformBlock {
	uniform mat4 ProjectionViewMat;
    uniform mat4 ModelMatrixCapsule;
    uniform mat4 ModelMatrixCube;
};



void main ()
{

	vec3 lightPos = vec3(15.0f,0.0f,0.0f);
	vec3 lightCol = vec3(1.0f,1.0f,1.0f);
	vec4 objColor = vec4(0.8,0.5,0.144,1.0);
	
	mat4 tempMat = ProjectionViewMat * ModelMatrixCapsule;
	gl_Position = tempMat * vec4(Position.xyz, 1.0f);
	
	vec4 normal = normalize(ModelMatrixCapsule * vec4(Normal.xyz, 0.0f));
	vec4 pos = ModelMatrixCapsule * vec4(Position.xyz, 1.0f);
	
	float lightIntensity = 1.0f;
    float quadraticCoeff = 1.2;
    float ambientCoeff = 0.4;
	
	vec3 lightDir;

    
    lightDir = normalize(lightPos - pos.xyz);
	
    float distance = length(lightDir);
    float attenuation = 1.0 / (quadraticCoeff * distance * distance);
    float intensity = lightIntensity * attenuation;

    vec3 baseColor = objColor.xyz;
    vec3 blendedColor = lightCol * baseColor * lightIntensity;
    vec3 diffuse = blendedColor * max(dot(normal.xyz, lightDir), 0.0);
    vec3 ambient = baseColor * ambientCoeff;
    Color = vec4(diffuse + ambient, 1.0);
	
}

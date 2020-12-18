#version 100
precision mediump float;
precision mediump int;

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

// Shader for simple shading with a point light
// for planets in Unit Test 12 - Transformations

struct UniformBlock
{
	mat4 ProjectionViewMat;
    mat4 ModelMatrixCapsule;
    mat4 ModelMatrixCube;
};

attribute vec3 Position;
attribute vec3 Normal;
attribute vec2 UV;

varying vec4 Color;

uniform UniformBlock uniformBlock;

void main ()
{
	vec3 lightPos = vec3(15.0,0.0,0.0);
	vec3 lightCol = vec3(1.0,1.0,1.0);
	vec4 objColor = vec4(0.8,0.5,0.144,1.0);
	
	mat4 tempMat = uniformBlock.ProjectionViewMat * uniformBlock.ModelMatrixCapsule;
	gl_Position = tempMat * vec4(Position.xyz, 1.0);

	vec4 normal = normalize(uniformBlock.ModelMatrixCapsule * vec4(Normal.xyz, 0.0));
	vec4 pos = uniformBlock.ModelMatrixCapsule * vec4(Position.xyz, 1.0);

	float lightIntensity = 1.0;
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

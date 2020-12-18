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

#define MAX_PLANETS 20
struct UniformBlock
{
    mat4 mvp;
    mat4 toWorld[MAX_PLANETS];
    vec4 color[MAX_PLANETS];

    // Point Light Information
    vec3 lightPosition;
    vec3 lightColor;
};

uniform UniformBlock uniformBlock;

uniform int instanceID;
attribute vec4 Position;
attribute vec4 Normal;

varying vec4 Color;

void main()
{
    mat4 tempMat = uniformBlock.mvp * uniformBlock.toWorld[instanceID];
    gl_Position = tempMat * vec4(Position.xyz, 1.0);
	
    vec4 normal = normalize(uniformBlock.toWorld[instanceID] * vec4(Normal.xyz, 0.0));
    vec4 pos = uniformBlock.toWorld[instanceID] * vec4(Position.xyz, 1.0);
	
    float lightIntensity = 1.0;
    float quadraticCoeff = 1.2;
    float ambientCoeff = 0.4;
	
    vec3 lightDir;

    if (int(uniformBlock.color[instanceID].w) == 0) // Special case for Sun, so that it is lit from its top
    {
        lightDir = vec3(0.0, 1.0, 0.0);
    }
    else
    {
        lightDir = normalize(uniformBlock.lightPosition - pos.xyz);
    }
	
    float distance = length(lightDir);
    float attenuation = 1.0 / (quadraticCoeff * distance * distance);
    float intensity = lightIntensity * attenuation;

    vec3 baseColor = uniformBlock.color[instanceID].xyz;
    vec3 blendedColor = uniformBlock.lightColor * baseColor * lightIntensity;
    vec3 diffuse = blendedColor * max(dot(normal.xyz, lightDir), 0.0);
    vec3 ambient = baseColor * ambientCoeff;
    Color = vec4(diffuse + ambient, 1.0);
}

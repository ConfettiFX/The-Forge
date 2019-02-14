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


// Shader for simple shading with a point light
// for skeletons in Unit Tests Animation

layout(location = 0) in vec3 iPosition;
layout(location = 1) in vec3 iNormal;
layout(location = 2) in vec2 iUV;
layout(location = 3) in vec4 iBoneWeights;
layout(location = 4) in uvec4 iBoneIndices;

layout(location = 0) out vec3 oNormal;
layout(location = 1) out vec2 oUV;

layout (std140, set=0, binding=0) uniform uniformBlock 
{
	mat4 vpMatrix;
	mat4 modelMatrix;
};

layout(std140, set = 0, binding = 1) uniform boneMatrices
{
	 mat4 boneMatrix[MAX_NUM_BONES];
};

layout(std140, set = 0, binding = 2) uniform boneOffsetMatrices
{
	mat4 boneOffsetMatrix[MAX_NUM_BONES];
};

void main ()
{
	mat4 boneTransform = boneMatrix[iBoneIndices[0]] * boneOffsetMatrix[iBoneIndices[0]] * iBoneWeights[0];
	boneTransform += boneMatrix[iBoneIndices[1]] * boneOffsetMatrix[iBoneIndices[1]] * iBoneWeights[1];
	boneTransform += boneMatrix[iBoneIndices[2]] * boneOffsetMatrix[iBoneIndices[2]] * iBoneWeights[2];
	boneTransform += boneMatrix[iBoneIndices[3]] * boneOffsetMatrix[iBoneIndices[3]] * iBoneWeights[3];

	gl_Position = boneTransform * vec4(iPosition, 1.0f);
	gl_Position = modelMatrix * gl_Position;
	gl_Position = vpMatrix * gl_Position;
	oNormal = normalize((modelMatrix * vec4(iNormal, 0.0f)).xyz);
	oUV = iUV;
}

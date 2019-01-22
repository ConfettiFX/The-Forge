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

#version 450 core

struct ObjectInfo
{
	mat4 toWorld;
	mat4 normalMat;
	uint matID;
};

layout(set = 0, binding = 0) uniform ObjectUniformBlock
{
	ObjectInfo	objectInfo[MAX_NUM_OBJECTS];
};

layout(push_constant) uniform DrawInfoRootConstant_Block
{
	uint baseInstance;
} DrawInfoRootConstant;

layout(set = 0, binding = 8) uniform CameraUniform
{
	mat4 camViewProj;
	mat4 camViewMat;
	vec4 camClipInfo;
	vec4 camPosition;
};

layout(location = 0) in vec4 PositionIn;
layout(location = 1) in vec4 NormalIn;
layout(location = 2) in vec4 UVIn;


void main()
{
	uint instanceID = gl_InstanceIndex + DrawInfoRootConstant.baseInstance;
	vec4 pos = objectInfo[instanceID].toWorld * PositionIn;
	gl_Position = camViewProj * pos;
}

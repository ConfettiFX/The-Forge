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

#define SPHERE_EACH_ROW 5
#define SPHERE_EACH_COL 5
#define SPHERE_NUM (SPHERE_EACH_ROW*SPHERE_EACH_COL + 1) // .... +1 plane

layout(location = 0) in vec4 Position;
layout(location = 1) in vec4 Normal;

layout(location = 0) out float WDepth;

layout(set = 0, binding = 0) uniform objectUniformBlock
{
	mat4 viewProj;
  mat4 toWorld[SPHERE_NUM];
};
layout (std140, set=0, binding=1) uniform lightUniformBlock {
  mat4 lightViewProj;
  vec4 LightDirection;//not used
  vec4 LightColor;//not used
};


void main()
{
  mat4 lvp = lightViewProj * toWorld[gl_InstanceIndex];
  gl_Position = lvp * Position;
  WDepth = gl_Position.w;    
}

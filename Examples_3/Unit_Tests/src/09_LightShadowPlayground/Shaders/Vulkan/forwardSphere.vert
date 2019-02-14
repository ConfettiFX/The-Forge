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

layout (location = 0) in vec4 PositionIn;
layout (location = 1) in vec4 NormalIn;

layout (location = 0) out vec3 WorldPosition;
layout (location = 1) out vec3 Color;
layout (location = 2) out vec3 NormalOut;
layout (location = 3) out /*flat*/ int IfPlane;

layout(set = 3/*per-frame update frequency*/, binding = 0) uniform objectUniformBlock
{
    mat4 WorldViewProjMat[SPHERE_NUM];
    mat4 WorldMat[SPHERE_NUM];
};

void main()
{
    gl_Position = WorldViewProjMat[gl_InstanceIndex] * PositionIn;
    WorldPosition = (WorldMat[gl_InstanceIndex] * PositionIn).xyz;
    if (gl_InstanceIndex == SPHERE_NUM - 1) {//this is for the plane
        NormalOut = vec3(0,1,0);
        IfPlane = 1;
    }
    else {
        // TODO: "normal matrix" MUST BE an inverse transpose not just the sub 3x3
        NormalOut = mat3(WorldMat[gl_InstanceIndex]) * normalize(NormalIn.xyz);
        IfPlane = 0;
    }
   Color = vec3(1,1,1);
}

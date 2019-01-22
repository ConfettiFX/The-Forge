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

#define PI 3.1415926289793f
#define PI_2 3.1415926289793f*2.0f

layout (location = 0) in vec4 WorldPositionIn;
layout (location = 1) in vec4 ColorIn;
layout (location = 2) in vec4 NormalIn;
layout (location = 3) in flat int IfPlaneIn;

layout(location = 0) out vec4 albedoOut;
layout(location = 1) out vec4 normalOut;
layout(location = 2) out vec4 positionWorldOut;

layout(set = 0, binding = 1) uniform texture2D SphereTex;
layout(set = 0, binding = 2) uniform texture2D PlaneTex;
layout(set = 0, binding = 3) uniform sampler textureSampler;

vec4 calcSphereTexColor(vec3 worldNormal)
{
  vec4 color = vec4(1,1,1,1);
  vec2 uv = vec2(0,0);
  uv.x = asin(worldNormal.x)/PI+0.5f;
  uv.y = asin(worldNormal.y)/PI+0.5f;  
  color = texture(sampler2D(SphereTex, textureSampler), uv);
  return color;
}
void main()
{
    albedoOut = ColorIn;
    normalOut = (NormalIn + 1) * 0.5f;
    positionWorldOut = WorldPositionIn;
    if (IfPlaneIn == 0){//spheres
        albedoOut *= calcSphereTexColor(NormalIn.xyz);
    }
    else {//plane
        albedoOut *= texture(sampler2D(PlaneTex, textureSampler), WorldPositionIn.xz);
    }
}
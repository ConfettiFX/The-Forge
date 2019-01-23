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

#include <metal_stdlib>
using namespace metal;

#define SPHERE_EACH_ROW 5
#define SPHERE_EACH_COL 5
#define SPHERE_NUM (SPHERE_EACH_ROW*SPHERE_EACH_COL +1 ) // .... +1 plane

struct ObjectUniform
{
  float4x4 viewProj;
  float4x4 toWorld[SPHERE_NUM];
};
struct VSInput
{
  float4 Position [[attribute(0)]];
  float4 Normal   [[attribute(1)]];
};
struct VSOutput
{
  float4 Position [[position]];
  float4 WorldPosition;
  float4 Color;
  float4 Normal;
  int IfPlane;
};

vertex VSOutput stageMain(VSInput input                           [[stage_in]],
                       uint InstanceID                            [[instance_id]],
                       constant ObjectUniform& objectUniformBlock [[buffer(1)]])
{
  VSOutput output;
  if (InstanceID == SPHERE_NUM - 1) {//this is for the plane
    output.Normal = float4(0,1,0,0);
    output.IfPlane = 1;
  }
  else {
    output.Normal = normalize(objectUniformBlock.toWorld[InstanceID] * float4(input.Normal.xyz, 0));
    output.IfPlane = 0;
  }
  float4x4 mvp = objectUniformBlock.viewProj * objectUniformBlock.toWorld[InstanceID];
  output.Position = mvp * input.Position;
  output.WorldPosition = objectUniformBlock.toWorld[InstanceID] * input.Position;
  output.Color = float4(1,1,1,1);
  
  return output;
}

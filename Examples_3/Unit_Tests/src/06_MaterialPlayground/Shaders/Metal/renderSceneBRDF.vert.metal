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

struct CameraData
{
    float4x4 projView;
    float4x4 invProjView;
    float3 camPos;
int bUseEnvironmentLight;
};

struct ObjectData
{
    float4x4 worldMat;
	float4 albedoAndRoughness;
    float metalness;
	int textureConfig;
};

struct VSInput
{
    float3 position [[attribute(0)]];
    float3 normal   [[attribute(1)]];
    float2 uv   [[attribute(2)]];
};

struct VSOutput {
    float4 position [[position]];
    float3 pos;
    float3 normal;
    float2 uv;
};

vertex VSOutput stageMain(VSInput In                    [[stage_in]],
                          constant CameraData& cbCamera [[buffer(1)]],
                          constant ObjectData& cbObject [[buffer(2)]])
{
    VSOutput result;
    float4x4 tempMat = cbCamera.projView * cbObject.worldMat;
    result.position = tempMat * float4(In.position,1.0);
    result.pos = (cbObject.worldMat * float4(In.position,1.0)).xyz;
    result.normal = (cbObject.worldMat * float4(In.normal.xyz, 0.0f)).xyz;
    result.uv= In.uv;
    return result;
}

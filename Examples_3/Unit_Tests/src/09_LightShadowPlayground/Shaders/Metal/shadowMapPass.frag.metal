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
#define SPHERE_NUM (SPHERE_EACH_ROW*SPHERE_EACH_COL + 1) // .... +1 plane

struct ESMInput
{
    float2 ScreenDimension;
    float2 NearFarDist;
    float Exponent;
    uint BlurWidth;
    int IfHorizontalBlur;
    int padding;
};
struct ObjectUniform
{
	float4x4 viewProj;//camera viewProj, not used
	float4x4 toWorld[SPHERE_NUM];
};
struct LightUniform
{
	float4x4 lightViewProj;
	float4   LightDirection;//not used
	float4   LightColor;//not used
};
struct VSOutput 
{
	float4 Position [[position]];
	float wDepth;
};
float map_01(float x, float v0, float v1)
{
    return (x - v0) / (v1 - v0);
}

// Pixel shader
fragment float4 stageMain(VSOutput input                                         [[stage_in]],
								   constant ObjectUniform& objectUniformBlock    [[buffer(1)]],
								   constant LightUniform& lightUniformBlock      [[buffer(2)]],
								   constant ESMInput& ESMInputConstants          [[buffer(3)]])
{
    float mappedDepth = map_01(input.wDepth, ESMInputConstants.NearFarDist.x, ESMInputConstants.NearFarDist.y);
    return exp(ESMInputConstants.Exponent * mappedDepth)*float4(1.0f);
}


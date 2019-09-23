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

#define MAX_LOD_OFFSETS (10)

struct UniformBlock0
{
    float4x4 viewProj;
    float4 camPos;
    float dt;
    uint startIdx;
    uint endIdx;
    int numLODs;
    int indexOffsets[MAX_LOD_OFFSETS];
};

struct AsteroidDynamic
{
	float4x4 transform;
    uint indexStart;
    uint indexEnd;
    uint padding[2];
};

struct AsteroidStatic
{
	float4 rotationAxis;
	float4 surfaceColor;
	float4 deepColor;

	float scale;
	float orbitSpeed;
	float rotationSpeed;

    uint textureID;
    uint vertexStart;
    uint padding[3];
};

struct IndirectDrawCommand
{
    //uint drawID;
    uint indexCount;
    uint instanceCount;
    uint startIndex;
    uint vertexOffset;
    uint startInstance;
 //   uint padding[2];
};

//taken from our math library
float4x4 MakeRotationMatrix(float angle, float3 axis)
{
    float s = sin(angle);
    float c = cos(angle);
    float x,y,z;
    x = axis.x;
    y = axis.y;
    z = axis.z;
    float xy, yz, zx;
    xy = axis.x * axis.y;
    yz = axis.y * axis.z;
    zx = axis.z * axis.x;
    float oneMinusC = 1.0 - c;

    return float4x4(
        x * x * oneMinusC + c , xy * oneMinusC + z * s, zx * oneMinusC - y * s, 0.0,
        xy * oneMinusC - z * s, y * y * oneMinusC + c , yz * oneMinusC + x * s, 0.0,
        zx * oneMinusC + y * s, yz * oneMinusC - x * s, z * z * oneMinusC + c , 0.0,
        0.0,                    0.0,                    0.0,                    1.0);
}

struct CSData {
    device AsteroidStatic* asteroidsStatic     [[id(0)]];
    device AsteroidDynamic* asteroidsDynamic   [[id(1)]];
    device IndirectDrawCommand* drawCmds       [[id(2)]];
};

struct CSDataFrame {
    constant UniformBlock0& uniformBlock      [[id(0)]];
};

//[numthreads(128,1,1)]
kernel void stageMain(uint3 threadID                            [[thread_position_in_grid]],
                      constant CSData& csData                   [[buffer(UPDATE_FREQ_NONE)]],
                      constant CSDataFrame& csDataFrame         [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
    const float minSubdivSizeLog2 = log2(0.0019);
    
    uint asteroidIdx = threadID.x + uint(csDataFrame.uniformBlock.startIdx);

    if (asteroidIdx >= csDataFrame.uniformBlock.endIdx)
        return;
    
    AsteroidStatic asteroidStatic = csData.asteroidsStatic[asteroidIdx];
    AsteroidDynamic asteroidDynamic = csData.asteroidsDynamic[asteroidIdx];

    float4x4 orbit = MakeRotationMatrix(asteroidStatic.orbitSpeed * csDataFrame.uniformBlock.dt, float3(0.0,1.0,0.0));
    float4x4 rotate = MakeRotationMatrix(asteroidStatic.rotationSpeed * csDataFrame.uniformBlock.dt, asteroidStatic.rotationAxis.xyz);

    asteroidDynamic.transform = (orbit * asteroidDynamic.transform) * rotate;

    float3 position = float3(
        asteroidDynamic.transform[3][0],
        asteroidDynamic.transform[3][1],
        asteroidDynamic.transform[3][2]);
    float distToEye = length(position - csDataFrame.uniformBlock.camPos.xyz);

    if (distToEye <= 0)
        return;

    float relativeScreenSizeLog2 = log2(asteroidStatic.scale / distToEye);
    float LODfloat = max(0.0, relativeScreenSizeLog2 - minSubdivSizeLog2);
    uint LOD = min(uint(csDataFrame.uniformBlock.numLODs - 1), uint(LODfloat));

    //setting start offset and index count
    uint startIdx = csDataFrame.uniformBlock.indexOffsets[LOD];
    uint endIdx = csDataFrame.uniformBlock.indexOffsets[LOD + 1];

    //drawCmds[threadID.x].drawID = asteroidIdx;
    csData.drawCmds[threadID.x].startIndex = startIdx;
    csData.drawCmds[threadID.x].indexCount = endIdx - startIdx;
    csData.drawCmds[threadID.x].vertexOffset = int(asteroidStatic.vertexStart);
    csData.drawCmds[threadID.x].startInstance = asteroidIdx;
    csData.drawCmds[threadID.x].instanceCount = 1;

    csData.asteroidsDynamic[asteroidIdx] = asteroidDynamic;
}

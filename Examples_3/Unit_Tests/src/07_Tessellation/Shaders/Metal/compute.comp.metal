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
#include <metal_atomic>
#include <metal_compute>
using namespace metal;

struct UniformData
{
    float4x4 world;
    float4x4 view;
    float4x4 invView;
    float4x4 proj;
    float4x4 viewProj;
    
    float deltaTime;
    float totalTime;
    
    int gWindMode;
    int gMaxTessellationLevel;
    
    float windSpeed;
    float windWidth;
    float windStrength;
};

struct Blade {
	float4 v0;
	float4 v1;
	float4 v2;
	float4 up;
};

struct BladeDrawIndirect
{
	atomic_uint vertexCount;
	uint instanceCount;
	uint firstVertex;
	uint firstInstance;
};

bool inBounds(float value, float bounds)
{
	return (value >= -bounds) && (value <= bounds);
}

float3 interPos(float3 posV0, float3 posV1, float3 posV2, float v)
{
	float3 a = posV0 + v*(posV1 - posV0);
	float3 b = posV1 + v*(posV2 - posV1);
	return a + v*(b - a);
}

//[numthreads(32, 1, 1)]
kernel void stageMain(constant UniformData& GrassUniformBlock  [[buffer(0)]],
                      device Blade* Blades                     [[buffer(1)]],
                      device Blade* CulledBlades               [[buffer(2)]],
                      device BladeDrawIndirect* NumBlades      [[buffer(3)]],
                      uint3 DTid                               [[thread_position_in_grid]])
{
    uint index = DTid.x;
    
    // Reset the number of blades to 0
    if (index == 0) {
        atomic_store_explicit(&NumBlades[0].vertexCount, 0, memory_order_relaxed);
    }
    
    threadgroup_barrier(mem_flags::mem_threadgroup); // Wait till all threads reach this point
    
    Blade blade = Blades[index];
    
    //v0.w holds orientation, v1.w holds height, v2.w holds width, and up.w holds the stiffness coefficient.
    float3 posV0 = blade.v0.xyz;
    float3 posV1 = blade.v1.xyz;
    float3 posV2 = blade.v2.xyz;
    
    float3 upV = blade.up.xyz;
    
    float theta = blade.v0.w;
    float sinTheta = sin(theta);
    float cosTheta = cos(theta);
    
    float h = blade.v1.w;
    
    float3 faceDir = normalize(cross(upV, float3(sinTheta, 0, cosTheta)));
    
    //Gravity
    float4 gravityDirection = float4(0, -1, 0, 9.8); //a : gravity Accelation
    
    float3 gE = normalize(gravityDirection.xyz) * gravityDirection.w;
    float3 gF = faceDir * 0.25 * length(gE);
    float3 gravityForce = gE + gF;
    
    //Recovery
    float3 iv2 = posV0 + upV * h;
    
    //min, max Height : 1.3, 5.5
    float maxCap = 1.8;
    float3 recoveryForce = (iv2 - posV2) * blade.up.w * maxCap / min(h, maxCap);
    
    //Wind
    float3 windDirection = float3(1, 0, 0);
    if (GrassUniformBlock.gWindMode == 0)
        windDirection = normalize(float3(1, 0, 0)); // straight wave
    else if (GrassUniformBlock.gWindMode == 1)
        windDirection = -normalize(posV0.xyz); // helicopter wave
    
    float seed = dot(windDirection, float3(posV0.x, 0.0, posV0.z)) + GrassUniformBlock.totalTime * GrassUniformBlock.windSpeed;
    
    float waveStrength = cos(seed * (1.0 / GrassUniformBlock.windWidth));
    
    float directionalAlignment = 1.0 - abs(dot(windDirection, normalize(posV2 - posV0)));
    float heightratio = dot(posV2 - posV0, upV) / h;
    
    float3 windForce = windDirection * directionalAlignment * heightratio * waveStrength * GrassUniformBlock.windStrength;
    
    //Total force
    float3 tv2 = (gravityForce + recoveryForce + windForce) * GrassUniformBlock.deltaTime;
    float3 v2 = posV2 + tv2;
    
    //a position of v2 above the local plane can be ensured
    v2 = v2 - upV*min(dot(upV, v2 - posV0), 0.0);
    
    float lproj = length(v2 - posV0 - upV * dot(v2 - posV0, upV));
    float lprohOverh = lproj / h;
    float3 v1 = posV0 + upV* h * max(1.0 - lprohOverh, 0.05*max(lprohOverh, 1.0));
    
    float degree = 3.0;
    float L0 = distance(v2, posV0);
    float L1 = distance(v2, v1) + distance(v1, posV0);
    float L = (2.0*L0 + (degree - 1.0)*L1) / (degree + 1.0);
    float r = h / L;
    
    blade.v1.xyz = posV0 + r*(v1 - posV0);
    blade.v2.xyz = blade.v1.xyz + r*(v2 - v1);
    
    Blades[index] = blade;
    
    posV0 = blade.v0.xyz;
    posV1 = blade.v1.xyz;
    posV2 = blade.v2.xyz;
    
    float3 viewVectorWorld = (GrassUniformBlock.invView * float4(0, 0, 1, 0)).xyz;
    
    //Orientation culling
    float tresholdOrientCull = 0.1;
    bool culledByOrientation = abs(dot(faceDir, normalize(float3(viewVectorWorld.x, 0.0, viewVectorWorld.z)))) < tresholdOrientCull;
    
    //View-frustum culling
    bool culledByFrustum;

    float4 projPosV0 = GrassUniformBlock.viewProj * float4(posV0, 1.0);
    projPosV0 /= projPosV0.w;
    
    float4 projPosV2 = GrassUniformBlock.viewProj * float4(posV2, 1.0);
    projPosV2 /= projPosV2.w;
    
    float3 center025Pos = interPos(posV0, posV1, posV2, 0.25);
    
    float3 centerPos = 0.25*posV0 * 0.5*posV1 * 0.25*posV2; //interPos(posV0, posV1, posV2, 0.5);
    
    float3 center075Pos = interPos(posV0, posV1, posV2, 0.75);
    
    float4 projCenter025Pos = GrassUniformBlock.viewProj * float4(center025Pos, 1.0);
    projCenter025Pos /= projCenter025Pos.w;
    
    float4 projCenterPos = GrassUniformBlock.viewProj * float4(centerPos, 1.0);
    projCenterPos /= projCenterPos.w;
    
    float4 projCenter075Pos = GrassUniformBlock.viewProj * float4(center075Pos, 1.0);
    projCenter075Pos /= projCenter075Pos.w;
    
    float clipVal = 1.3;
    
    if (((projPosV0.x >= -clipVal && projPosV0.x <= clipVal) && (projPosV0.y >= -clipVal && projPosV0.y <= clipVal) && (projPosV0.z >= 0.0 && projPosV0.z <= 1.0)) ||
        ((projPosV2.x >= -clipVal && projPosV2.x <= clipVal) && (projPosV2.y >= -clipVal && projPosV2.y <= clipVal) && (projPosV2.z >= 0.0 && projPosV2.z <= 1.0)) ||
        ((projCenter025Pos.x >= -clipVal && projCenter025Pos.x <= clipVal) && (projCenter025Pos.y >= -clipVal && projCenter025Pos.y <= clipVal) && (projCenter025Pos.z >= 0.0 && projCenter025Pos.z <= 1.0)) ||
        ((projCenterPos.x >= -clipVal && projCenterPos.x <= clipVal) && (projCenterPos.y >= -clipVal && projCenterPos.y <= clipVal) && (projCenterPos.z >= 0.0 && projCenterPos.z <= 1.0)) ||
        ((projCenter075Pos.x >= -clipVal && projCenter075Pos.x <= clipVal) && (projCenter075Pos.y >= -clipVal && projCenter075Pos.y <= clipVal) && (projCenter075Pos.z >= 0.0 && projCenter075Pos.z <= 1.0)))
        culledByFrustum = false;
    else
        culledByFrustum = true;
    
    //Distance culling
    float near = 0.1;
    float far = 200.0;
    
    bool culledByDistance = true;
    float linearDepth = (2.0 * near) / (far + near - projPosV0.z * (far - near));
    
    float tresholdDistCull = 0.95;
    
    if (linearDepth <= tresholdDistCull)
    {
        culledByDistance = false;
    }
    
    if (!(culledByOrientation || culledByFrustum || culledByDistance))
    {
        uint NewIndex = atomic_fetch_add_explicit(&NumBlades[0].vertexCount, 1, memory_order_relaxed);
        CulledBlades[NewIndex] = blade;
    }
}

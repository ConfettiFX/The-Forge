/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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
using namespace metal;

#include "shader_defs.h"
#include "light_cull_argument_buffers.h"

// This compute shader determines if a light of index groupId overlaps
// the cluster (thread.x,thread.y). Then the light is added to the cluster.

//[numthreads(8, 8, 1)]
kernel void stageMain(
    uint3 threadInGroupId [[thread_position_in_threadgroup]],
    uint3 groupId [[threadgroup_position_in_grid]],
    constant CSData& csData [[buffer(UPDATE_FREQ_NONE)]],
    constant CSDataPerFrame& csDataPerFrame [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
    const float invClusterWidth = 1.0f / float(LIGHT_CLUSTER_WIDTH);
    const float invClusterHeight = 1.0f / float(LIGHT_CLUSTER_HEIGHT);
    const float2 windowSize = csDataPerFrame.uniforms.cullingViewports[VIEW_CAMERA].windowSize;
    
    const float aspectRatio = windowSize.x / windowSize.y;
    
    LightData lightData = csData.lights[groupId.x];
    
    float4 lightPosWorldSpace = float4(lightData.position, 1);
    float4 lightPosClipSpace = csDataPerFrame.uniforms.transform[VIEW_CAMERA].vp * lightPosWorldSpace;
    float invLightPosW = 1.0 / lightPosClipSpace.w;
    float3 lightPos = lightPosClipSpace.xyz * invLightPosW;
    
    // Early exit light if it's behind the camera
    if (lightPos.z < 0)
        return;
    
    float projRadius = LIGHT_SIZE * invLightPosW / 0.5f;    
    lightPos *= float3(aspectRatio,1,1);
    
    // Cluster coordinates in post perspective clip space
    float clusterLeft = float(threadInGroupId.x) * invClusterWidth;
    float clusterTop =  float(threadInGroupId.y) * invClusterHeight;
    float clusterRight = clusterLeft + invClusterWidth;
    float clusterBottom = clusterTop + invClusterHeight;
    
    // Transform coordinates from range [0..1] to range [-1..1]
    clusterLeft = clusterLeft*2.0f - 1.0f;
    clusterTop = clusterTop*2.0f - 1.0f;
    clusterRight = clusterRight*2.0f - 1.0f;
    clusterBottom = clusterBottom*2.0f - 1.0f;
    
    clusterLeft *= aspectRatio;
    clusterRight *= aspectRatio;
    
    float clusterCenterX = (clusterLeft + clusterRight) * 0.5f;
    float clusterCenterY = (clusterTop + clusterBottom) * 0.5f;
    float clusterRadius = distance(float2(clusterLeft, clusterTop), float2(clusterRight, clusterBottom))*0.5f;
    
    // Check if the light projection overlaps the cluster: add the light bit to this cluster coords
    float distanceToCenter = distance(float2(clusterCenterX, clusterCenterY), lightPos.xy);
    if (distanceToCenter  < projRadius + clusterRadius)
    {
        // Increase light count on this cluster
        uint lightArrayPos = atomic_fetch_add_explicit(&csDataPerFrame.lightClustersCount[LIGHT_CLUSTER_COUNT_POS(threadInGroupId.x,threadInGroupId.y)], 1, memory_order_relaxed);

        // Add light id to cluster
        atomic_store_explicit(&csDataPerFrame.lightClusters[LIGHT_CLUSTER_DATA_POS(lightArrayPos, threadInGroupId.x, threadInGroupId.y)], groupId.x, memory_order_relaxed);
    }
}

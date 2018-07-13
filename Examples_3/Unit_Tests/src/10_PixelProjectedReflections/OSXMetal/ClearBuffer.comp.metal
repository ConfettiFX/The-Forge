/*
* Copyright (c) 2018 Confetti Interactive Inc.
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
#include <metal_compute>
using namespace metal;

struct ExtendCameraData
{
	float4x4 viewMat;
	float4x4 projMat;
	float4x4 viewProjMat;
	float4x4 InvViewProjMat;

	float4 cameraWorldPos;
	float4 viewPortSize;
};

//[numthreads(1,1,1)]
kernel void stageMain(
					  const uint DTid [[ thread_position_in_grid ]],
					  volatile device atomic_uint * IntermediateBuffer	[[buffer(0)]])
{	
	uint indexDX = DTid;
	
	//store max value
   //IntermediateBuffer[indexDX] = UINT_MAX;
	atomic_store_explicit(&IntermediateBuffer[indexDX], UINT_MAX, memory_order_relaxed);
}

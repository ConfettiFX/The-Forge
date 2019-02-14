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

//[numthreads(16, 16, 1)]
	kernel void stageMain(
	uint2 invocationID [[thread_position_in_grid]],
    texture2d<float, access::read> srcImg [[texture(0)]],
    texture2d<float, access::write> dstImg [[texture(1)]],
    sampler clampMiplessSampler [[sampler(0)]])
{
    float4 value = srcImg.read(invocationID);
    dstImg.write(value.x, (invocationID));
}

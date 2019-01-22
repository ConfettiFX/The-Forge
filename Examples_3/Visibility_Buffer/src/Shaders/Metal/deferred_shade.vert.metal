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

// This shader loads gBuffer data and shades the pixel.

#include <metal_stdlib>
using namespace metal;

struct VSOutput {
	float4 position [[position]];
    float2 screenPos;
    uint triangleId;
};

// Vertex shader
vertex VSOutput stageMain(uint vertexId [[vertex_id]])
{
    // Produce a fullscreen triangle using the current vertexId
    // to automatically calculate the vertex porision. This
    // method avoids using vertex/index buffers to generate a
    // fullscreen quad.
    
	VSOutput result;
    result.position.x = (vertexId == 2 ? 3.0 : -1.0);
    result.position.y = (vertexId == 0 ? -3.0 : 1.0);
    result.position.zw = float2(0, 1);
    result.screenPos = result.position.xy;
    result.triangleId = vertexId / 3;
	return result;
}

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

// This shader loads draw / triangle Id per pixel and reconstruct interpolated vertex data.
struct PsIn
{
	float4 Position : SV_Position;
	float2 ScreenPos : TEXCOORD0;
};

// Vertex shader
PsIn main(uint vertexID : SV_VertexID)
{
	// Produce a fullscreen triangle using the current vertexId
	// to automatically calculate the vertex porision. This
	// method avoids using vertex/index buffers to generate a
	// fullscreen quad.
	PsIn result;
	result.Position.x = (vertexID == 2 ? 3.0 : -1.0);
	result.Position.y = (vertexID == 0 ? -3.0 : 1.0);
	result.Position.zw = float2(0, 1);
	result.ScreenPos = result.Position.xy;
	return result;
}
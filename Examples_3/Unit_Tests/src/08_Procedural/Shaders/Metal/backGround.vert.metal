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

struct VSInput
{
    float4 vs_Pos [[attribute(0)]];
    float4 vs_Nor [[attribute(1)]];
};

struct VSOutput {
    float4 Position [[position]];
    float2 fs_UV;
};

vertex VSOutput stageMain(VSInput input     [[stage_in]],
                          uint VertexID     [[vertex_id]],
                          uint InstanceID   [[instance_id]])
{
	VSOutput output;

	if (VertexID == 0)
	{
		output.Position = float4(-1.0, -1.0, 0.999998, 1.0);
		output.fs_UV = float2(0.0, 1.0);
	}
	else if (VertexID == 2)
	{
		output.Position = float4(1.0, -1.0, 0.999998, 1.0);
		output.fs_UV = float2(1.0, 1.0);
	}
	else if (VertexID == 1)
	{
		output.Position = float4(1.0, 1.0, 0.999998, 1.0);
		output.fs_UV = float2(1.0, 0.0);
	}
	else if (VertexID == 3)
	{
		output.Position = float4(-1.0, -1.0, 0.999998, 1.0);
		output.fs_UV = float2(0.0, 1.0);
	}
	else if (VertexID == 5)
	{
		output.Position = float4(1.0, 1.0, 0.999998, 1.0);
		output.fs_UV = float2(1.0, 0.0);
	}
	else if (VertexID == 4)
	{
		output.Position = float4(-1.0, 1.0, 0.999998, 1.0);
		output.fs_UV = float2(0.0, 0.0);
	}

	return output;
}

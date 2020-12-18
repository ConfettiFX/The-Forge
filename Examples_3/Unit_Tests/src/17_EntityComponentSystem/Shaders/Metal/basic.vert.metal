/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
	struct Uniforms_VsParams
	{
		float aspect;
	};
	constant Uniforms_VsParams & VsParams;
	struct InstanceData
	{
		float4 posScale;
		float4 colorIndex;
	};
	constant InstanceData* instanceBuffer;
	struct VSOutput
	{
		float4 pos [[position]];
		float3 color;
		float2 uv;
	};
	VSOutput main(uint vertexId, uint instanceId)
	{
		VSOutput result;
		float x = (vertexId / (uint)(2));
		float y = (vertexId & (uint)(1));
		(((result).pos).x = (((instanceBuffer[instanceId]).posScale).x + ((x - 0.5) * ((instanceBuffer[instanceId]).posScale).z)));
		(((result).pos).y = (((instanceBuffer[instanceId]).posScale).y + (((y - 0.5) * ((instanceBuffer[instanceId]).posScale).z) * VsParams.aspect)));
		(((result).pos).z = 0.0);
		(((result).pos).w = 1.0);
		((result).uv = float2(((x + ((instanceBuffer[instanceId]).colorIndex).w) / (float)(8)), ((float)(1) - y)));
		((result).color = ((instanceBuffer[instanceId]).colorIndex).rgb);
		return result;
	};
	
	Vertex_Shader(
				  constant Uniforms_VsParams & VsParams,constant InstanceData* instanceBuffer) :
	VsParams(VsParams),instanceBuffer(instanceBuffer) {}
};

vertex Vertex_Shader::VSOutput stageMain(
										 uint vertexId [[vertex_id]],
										 uint instanceId [[instance_id]],
                                         constant Vertex_Shader::InstanceData* instanceBuffer [[buffer(0)]],
										 constant Vertex_Shader::Uniforms_VsParams& RootConstant [[buffer(1)]]
)
{
	uint vertexId0;
	vertexId0 = vertexId;
	uint instanceId0;
	instanceId0 = instanceId;
	Vertex_Shader main(RootConstant, instanceBuffer);
	return main.main(vertexId0, instanceId0);
}

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

cbuffer VsParams : register(b0) {
	float aspect;
};

struct InstanceData {
    float4 posScale;
    float4 colorIndex;
};

StructuredBuffer<InstanceData> instanceBuffer : register(t0);

struct VSOutput {
    float4 pos : SV_Position;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
};

VSOutput main(in uint vertexId : SV_VertexID, in uint instanceId : SV_InstanceID)
{
    VSOutput result;
    float x = vertexId / 2;
    float y = vertexId & 1;
    result.pos.x = instanceBuffer[instanceId].posScale.x + (x-0.5f) * instanceBuffer[instanceId].posScale.z;
    result.pos.y = instanceBuffer[instanceId].posScale.y + (y-0.5f) * instanceBuffer[instanceId].posScale.z * aspect;
    result.pos.z = 0.0f;
    result.pos.w = 1.0f;
    result.uv = float2((x + instanceBuffer[instanceId].colorIndex.w)/8,1-y);
    result.color = instanceBuffer[instanceId].colorIndex.rgb;
    return result;
};
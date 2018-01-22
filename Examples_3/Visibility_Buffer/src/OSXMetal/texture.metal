/*
 * Copyright (c) 2018 Confetti Interactive Inc.
 *
 * This file is part of TheForge
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

struct VSInput {
    float4 position [[ attribute(0) ]];
    float2 texCoord [[ attribute(1) ]];
};

struct VSOutput {
	float4 position [[position]];
	float2 texCoord;
};

vertex VSOutput VSMain(VSInput v_in [[stage_in]])
{
	VSOutput result;
    result.position = v_in.position;
	result.texCoord = v_in.texCoord;
	return result;
}

fragment float4 PSMain(VSOutput input [[stage_in]],
                       texture2d<float, access::sample> uTex0 [[texture(0)]],
                       sampler uSampler0 [[sampler(1)]])
{
	return uTex0.sample(uSampler0, input.texCoord);
}

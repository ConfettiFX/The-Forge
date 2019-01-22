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


TextureCube<float4> skyboxTex : register(t1);
SamplerState skyboxSampler : register(s2);

struct VSOutput 
{
	float4 Position : SV_POSITION;
	float3 pos : POSITION;
};

float4 main(VSOutput input) : SV_TARGET
{
	float4 outColor = skyboxTex.Sample(skyboxSampler, input.pos);

	float inverseArg = 1 / 2.2;
	return float4(pow(outColor.r, inverseArg), pow(outColor.g, inverseArg), pow(outColor.b, inverseArg), 1.0f);
}
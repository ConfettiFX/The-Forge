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

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 UV : Texcoord0;
};

SamplerState PointSampler : register(s0);
Texture2D AccumulationTexture : register(t0);
Texture2D RevealageTexture : register(t1);

float MaxComponent(float4 v)
{
	return max(max(max(v.x, v.y), v.z), v.w);
}

float4 main(VSOutput input) : SV_Target
{    
	float revealage = RevealageTexture.Sample(PointSampler, input.UV.xy).r;
	clip(1.0f - revealage - 0.00001f);

	float4 accumulation = AccumulationTexture.Sample(PointSampler, input.UV.xy);
	if(isinf(MaxComponent(abs(accumulation))))
		accumulation.rgb = accumulation.a;

	float3 averageColor = accumulation.rgb / max(accumulation.a, 0.00001f);

	return float4(averageColor, 1.0f - revealage);
}

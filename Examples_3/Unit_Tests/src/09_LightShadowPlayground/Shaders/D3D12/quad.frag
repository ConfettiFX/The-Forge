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





struct VSOutput
{
	float4 Position  : SV_POSITION;
	float2 Tex_Coord : TEXCOORD0;
};

SamplerState clampNearSampler : register(s0, UPDATE_FREQ_NONE);
Texture2D screenTexture : register(t1);

float LinearizeDepth(float depth)
{
	const float nearPlane = 0.1f;
	const float farPlane = 1000.f;

    float z = depth * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

float4 main(VSOutput input) : SV_TARGET
{
	/*float depthValue = screenTexture.Sample(clampNearSampler, input.Tex_Coord).x;

	if(depthValue == 1.0)
	{
		return float4(1.0, 0.0, 0.0, 1.0);
	}

	//depthValue = LinearizeDepth(depthValue) / 1000.f;
	//float4 screen_color = screenTexture.Sample(uSampler0, input.Tex_Coord);
	//return screen_color;
	return float4(depthValue, depthValue, depthValue, 1.0);	*/

	float3 color = screenTexture.Sample(clampNearSampler, input.Tex_Coord).xyz;
	//float rcolor = screenTexture.Sample(clampNearSampler, input.Tex_Coord).x;
	//float3 color = float3(rcolor,rcolor,rcolor);
	return float4(color, 1.0);
}
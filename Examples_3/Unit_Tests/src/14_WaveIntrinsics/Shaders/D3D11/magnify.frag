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

cbuffer SceneConstantBuffer : register(b0)
{
	float4x4 orthProjMatrix;
	float2 mousePosition;
	float2 resolution;
	float time;
	uint renderMode;
	uint laneSize;
	uint padding;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

Texture2D g_texture : register(t1);
SamplerState g_sampler : register(s2);

float4 main(PSInput input) : SV_TARGET
{
	float aspectRatio = resolution.x / resolution.y;
	float magnifiedFactor = 6.0f;
	float magnifiedAreaSize = 0.05f;
	float magnifiedAreaBorder = 0.005f;

	// check the distance between this pixel and mouse location in UV space. 
	float2 normalizedPixelPos = input.uv;
	float2 normalizedMousePos = mousePosition / resolution;         // convert mouse position to uv space.
	float2 diff = abs(normalizedPixelPos - normalizedMousePos);     // distance from this pixel to mouse.

	float4 color = g_texture.Sample(g_sampler, normalizedPixelPos);

	// if the distance from this pixel to mouse is touching the border of the magnified area, color it as cyan.
	if (diff.x < (magnifiedAreaSize + magnifiedAreaBorder) && diff.y < (magnifiedAreaSize + magnifiedAreaBorder)*aspectRatio)
	{
		color = float4(0.0f, 1.0f, 1.0f, 1.0f);
	}

	// if the distance from this pixel to mouse is inside the magnified area, enable the magnify effect.
	if (diff.x < magnifiedAreaSize && diff.y < magnifiedAreaSize *aspectRatio)
	{
		color = g_texture.Sample(g_sampler, normalizedMousePos + (normalizedPixelPos - normalizedMousePos) / magnifiedFactor);
	}

	return color;
}

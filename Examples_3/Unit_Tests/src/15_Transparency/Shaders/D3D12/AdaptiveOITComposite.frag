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

#define AOIT_ORDERED_ACCESS
#include "AOIT.hlsl"

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 UV : Texcoord0;
};

float4 main(VSOutput input) : SV_Target
{
	float4 outColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
	uint2 pixelAddr = uint2(input.Position.xy);

	// Load control surface
	AOITControlSurface ctrlSurface = (AOITControlSurface)0;
	AOITLoadControlSurfaceSRV(pixelAddr, ctrlSurface);

	// Any transparent fragments contributing to this pixel?
	if(!ctrlSurface.clear)
	{
		// Load all nodes for this pixel
		AOITNode nodeArray[AOIT_NODE_COUNT];
		AOITLoadDataSRV(pixelAddr, nodeArray);

		// Accumulate final transparent colors
		float trans = 1.0f;
		float3 color = 0.0f;
		[unroll] for(uint i = 0; i < AOIT_NODE_COUNT; ++i)
		{
			color += trans * UnpackRGB(nodeArray[i].color);
			trans = nodeArray[i].trans / 255;
		}

		outColor = float4(color, nodeArray[AOIT_NODE_COUNT - 1].trans / 255);
	}

	// Blend with background color
	return outColor;
}

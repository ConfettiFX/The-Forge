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

#version 450 core

#extension GL_GOOGLE_include_directive : require

#define AOIT_ORDERED_ACCESS
#include "AOIT.h"

layout (location = 0) in vec4 iUV;

layout(location = 0) out vec4 oColor;

void main()
{
	vec4 outColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	ivec2 pixelAddr = ivec2(gl_FragCoord.xy);

	// Load control surface
	AOITControlSurface ctrlSurface = AOITControlSurface(false, false, 0.0f);
	AOITLoadControlSurfaceSRV(pixelAddr, ctrlSurface);

	// Any transparent fragments contributing to this pixel?
	if(!ctrlSurface.clear)
	{
		// Load all nodes for this pixel
		AOITNode nodeArray[AOIT_NODE_COUNT];
		AOITLoadDataSRV(pixelAddr, nodeArray);

		// Accumulate final transparent colors
		float trans = 1.0f;
		vec3 color = vec3(0.0f);
		for(uint i = 0; i < AOIT_NODE_COUNT; ++i)
		{
			color += trans * UnpackRGB(nodeArray[i].color);
			trans = nodeArray[i].trans / 255;
		}

		outColor = vec4(color, nodeArray[AOIT_NODE_COUNT - 1].trans / 255);
	}

	// Blend with background color
	oColor = outColor;
}

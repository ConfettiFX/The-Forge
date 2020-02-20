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

#extension GL_EXT_samplerless_texture_functions : enable
#ifndef TARGET_SWITCH
#extension GL_ARB_fragment_shader_interlock : enable
#endif
/************************************************************************/
// Defines
/************************************************************************/
#define AOIT_EMPTY_NODE_DEPTH   (3.40282E38)
#define AOIT_TRANS_BIT_COUNT    (8)
#define AOIT_MAX_UNNORM_TRANS   ((1 << AOIT_TRANS_BIT_COUNT) - 1)
#define AOIT_TRANS_MASK         (0xFFFFFFFF - uint(AOIT_MAX_UNNORM_TRANS))

#define AOIT_TILED_ADDRESSING

#ifndef AOIT_NODE_COUNT 
#define AOIT_NODE_COUNT			(4)
#endif

#if AOIT_NODE_COUNT == 2
#define AOIT_RT_COUNT			(1)
#else
#define AOIT_RT_COUNT			(AOIT_NODE_COUNT / 4)
#endif

/************************************************************************/
// Structures
/************************************************************************/
struct AOITNode
{
	float  depth;
	float  trans;
	uint   color;
};

struct AOITControlSurface
{
	bool  clear;
	bool  opaque;
	float depth;
};

struct AOITData
{
	vec4 depth[AOIT_RT_COUNT];
	uvec4 color[AOIT_RT_COUNT];
};

struct AOITDepthData
{
	vec4 depth[AOIT_RT_COUNT];
};

struct AOITColorData
{
	uvec4 color[AOIT_RT_COUNT];
};

/************************************************************************/
// Shader resources
/************************************************************************/
#define UNIT_SRV_AOIT_MASK  layout(binding = 15, UPDATE_FREQ_NONE)
#define UNIT_SRV_AOIT_DEPTH layout(binding = 16, UPDATE_FREQ_NONE) readonly
#define UNIT_SRV_AOIT_COLOR layout(binding = 17, UPDATE_FREQ_NONE) readonly

#define UNIT_UAV_AOIT_MASK  layout(binding = 20, UPDATE_FREQ_NONE, r32ui)
#define UNIT_UAV_AOIT_DEPTH layout(binding = 21, UPDATE_FREQ_NONE)
#define UNIT_UAV_AOIT_COLOR layout(binding = 22, UPDATE_FREQ_NONE)

#ifdef AOIT_UNORDERED_ACCESS
#ifndef TARGET_SWITCH
layout(pixel_interlock_ordered) in;
#endif
UNIT_UAV_AOIT_MASK uniform uimage2D AOITClearMaskUAV;
UNIT_UAV_AOIT_DEPTH buffer AOITDepthDataUAV
{
	AOITDepthData AOITDepthDataUAVData[];
};
UNIT_UAV_AOIT_COLOR buffer AOITColorDataUAV
{
	AOITColorData AOITColorDataUAVData[];
};
#endif

#ifdef AOIT_ORDERED_ACCESS
UNIT_SRV_AOIT_MASK uniform utexture2D AOITClearMaskSRV;
UNIT_SRV_AOIT_DEPTH buffer AOITDepthDataSRV
{
	AOITDepthData AOITDepthDataSRVData[];
};
UNIT_SRV_AOIT_COLOR buffer AOITColorDataSRV
{
	AOITColorData AOITColorDataSRVData[];
};
#endif
/************************************************************************/
// Helper functions
/************************************************************************/
vec3 saturate(vec3 c) { return clamp(c, vec3(0.0f), vec3(1.0f)); }
vec4 saturate(vec4 c) { return clamp(c, vec4(0.0f), vec4(1.0f)); }

uint PackRGB(vec3 color)
{
	uvec3 u = uvec3(saturate(color) * 255 + vec3(0.5));
	uint  packedOutput = (u.z << 16) | (u.y << 8) | u.x;
	return packedOutput;
}

vec3 UnpackRGB(uint packedInput)
{
	vec3 unpackedOutput;
	uvec3 p = uvec3((packedInput & 0xFF),
		(packedInput >> 8) & 0xFF,
		(packedInput >> 16) & 0xFF);

	unpackedOutput = (vec3(p)) / 255;
	return unpackedOutput;
}

uint PackRGBA(vec4 color)
{
	uvec4 u = uvec4(saturate(color) * 255 + vec4(0.5));
	uint packedOutput = (u.w << 24) | (u.z << 16) | (u.y << 8) | u.x;
	return packedOutput;
}

vec4 UnpackRGBA(uint packedInput)
{
	vec4 unpackedOutput;
	uvec4 p = uvec4((packedInput & 0xFF),
		(packedInput >> 8) & 0xFF,
		(packedInput >> 16) & 0xFF,
		(packedInput >> 24));

	unpackedOutput = (vec4(p)) / 255;
	return unpackedOutput;
}

float UnpackUnnormAlpha(uint packedInput)
{
	return float(packedInput >> 24);
}

uint AOITAddrGen(uvec2 addr2D, uint width)
{
#ifdef AOIT_TILED_ADDRESSING
	width = width >> 1U;
	uvec2 tileAddr2D = addr2D >> 1U;
	uint tileAddr1D = (tileAddr2D[0] + width * tileAddr2D[1]) << 2U;
	uvec2 pixelAddr2D = addr2D & 0x1U;
	uint pixelAddr1D = (pixelAddr2D[1] << 1U) + pixelAddr2D[0];
	return tileAddr1D | pixelAddr1D;
#else
	return addr2D[0] + width * addr2D[1];
#endif
}

#ifdef AOIT_UNORDERED_ACCESS
uint AOITAddrGenUAV(uvec2 addr2D)
{
	ivec2 dim = imageSize(AOITClearMaskUAV);
	return AOITAddrGen(addr2D, dim[0]);
}
#endif

#ifdef AOIT_ORDERED_ACCESS
uint AOITAddrGenSRV(uvec2 addr2D)
{
	ivec2 dim = textureSize(AOITClearMaskSRV, 0);
	return AOITAddrGen(addr2D, dim[0]);
}
#endif

void AOITLoadControlSurface(in uint data, inout AOITControlSurface surface)
{
	surface.clear = ((data & 0x1) > 0) ? true : false;
	surface.opaque = ((data & 0x2) > 0) ? true : false;
	surface.depth = uintBitsToFloat((data & 0xFFFFFFFC) | 0x3);
}

/************************************************************************/
// Load/Store/Clear functions
/************************************************************************/
#ifdef AOIT_UNORDERED_ACCESS
void AOITLoadControlSurfaceUAV(in uvec2 pixelAddr, inout AOITControlSurface surface)
{
	uint data = imageLoad(AOITClearMaskUAV, ivec2(pixelAddr)).r;
	AOITLoadControlSurface(data, surface);
}

void AOITStoreControlSurfaceUAV(uvec2 pixelAddr, in AOITControlSurface surface)
{
	uint data;
	data = floatBitsToUint(surface.depth) & 0xFFFFFFFC;
	data |= surface.opaque ? 0x2 : 0x0;
	data |= surface.clear ? 0x1 : 0x0;
	imageStore(AOITClearMaskUAV, ivec2(pixelAddr), uvec4(data));
}

void AOITLoadDataUAV(in uvec2 pixelAddr, out AOITNode nodeArray[AOIT_NODE_COUNT])
{
	AOITData data;
	uint addr = AOITAddrGenUAV(pixelAddr);
	data.color = AOITColorDataUAVData[addr].color;

#if AOIT_NODE_COUNT == 2
	for (uint i = 0; i < 2; ++i)
	{
		AOITNode node = { uintBitsToFloat(data.color[0][i]), UnpackUnnormAlpha(data.color[0][2 + i]), data.color[0][2 + i] & 0xFFFFFF };
		nodeArray[i] = node;
	}
#else
	data.depth = AOITDepthDataUAVData[addr].depth;
	for (uint i = 0; i < AOIT_RT_COUNT; ++i)
	{
		for (uint j = 0; j < 4; ++j)
		{
			AOITNode node = { data.depth[i][j],
								UnpackUnnormAlpha(data.color[i][j]),
								data.color[i][j] & 0xFFFFFF };
			nodeArray[4 * i + j] = node;
		}
	}
#endif
}

void AOITStoreDataUAV(in uvec2 pixelAddr, AOITNode nodeArray[AOIT_NODE_COUNT])
{
	AOITData data;
	uint addr = AOITAddrGenUAV(pixelAddr);

#if AOIT_NODE_COUNT == 2
	for (uint i = 0; i < 2; ++i)
	{
		data.color[0][i] = asuint((asuint(float(nodeArray[i].depth)) & (uint)AOIT_TRANS_MASK) | (uint)(nodeArray[i].trans));
		data.color[0][2 + i] = (nodeArray[i].color & 0xFFFFFF) | (((uint)(nodeArray[i].trans)) << 24);
	}
#else
	for (uint i = 0; i < AOIT_RT_COUNT; ++i)
	{
		for (uint j = 0; j < 4; ++j)
		{
			data.depth[i][j] = nodeArray[4 * i + j].depth;
			data.color[i][j] = (nodeArray[4 * i + j].color & 0xFFFFFF) | ((uint(nodeArray[4 * i + j].trans)) << 24);
		}
	}
	AOITDepthDataUAVData[addr].depth = data.depth;
#endif
	AOITColorDataUAVData[addr].color = data.color;
}
#endif

#ifdef AOIT_ORDERED_ACCESS
void AOITLoadControlSurfaceSRV(in uvec2 pixelAddr, inout AOITControlSurface surface)
{
	uint data = texelFetch(AOITClearMaskSRV, ivec2(pixelAddr), 0).r;
	AOITLoadControlSurface(data, surface);
}

void AOITLoadDataSRV(in uvec2 pixelAddr, out AOITNode nodeArray[AOIT_NODE_COUNT])
{
	AOITData data;
	uint addr = AOITAddrGenSRV(pixelAddr);
	data.color = AOITColorDataSRVData[addr].color;

#if AOIT_NODE_COUNT == 2
	for (uint i = 0; i < 2; ++i)
	{
		AOITNode node = { 0, UnpackUnnormAlpha(data.color[0][2 + i]),
			data.color[0][2 + i] & 0xFFFFFF };
		nodeArray[i] = node;
	}
#else
	for (uint i = 0; i < AOIT_RT_COUNT; ++i)
	{
		for (uint j = 0; j < 4; ++j)
		{
			AOITNode node = { 0, UnpackUnnormAlpha(data.color[i][j]),
				data.color[i][j] & 0xFFFFFF };
			nodeArray[4 * i + j] = node;
		}
	}
#endif
}
#endif

/************************************************************************/
// Other functions
/************************************************************************/
void AOITClearData(inout AOITData data, float depth, vec4 color)
{
	uint packedColor = PackRGBA(vec4(0, 0, 0, 1.0f - color.w));

#if AOIT_NODE_COUNT == 2
	data.depth[0] = 0;
	data.color[0][0] = floatBitsToUint(depth);
	data.color[0][1] = floatBitsToUint(float(AOIT_EMPTY_NODE_DEPTH));
	data.color[0][2] = PackRGBA(vec4(color.www * color.xyz, 1.0f - color.w));
	data.color[0][3] = packedColor;
#else
	for (uint i = 0; i < AOIT_RT_COUNT; ++i)
	{
		data.depth[i] = vec4(AOIT_EMPTY_NODE_DEPTH);
		data.color[i] = uvec4(packedColor);
	}
	data.depth[0][0] = depth;
	data.color[0][0] = PackRGBA(vec4(color.www * color.xyz, 1.0f - color.w));
#endif
}

void AOITInsertFragment(in float fragmentDepth, in float fragmentTrans, in vec3 fragmentColor, inout AOITNode nodeArray[AOIT_NODE_COUNT])
{
	int i;

	float depth[AOIT_NODE_COUNT + 1];
	float trans[AOIT_NODE_COUNT + 1];
	uint color[AOIT_NODE_COUNT + 1];

	// Unpack AOIT data
	for (i = 0; i < AOIT_NODE_COUNT; ++i)
	{
		depth[i] = nodeArray[i].depth;
		trans[i] = nodeArray[i].trans;
		color[i] = nodeArray[i].color;
	}

	// Find insertion index
	int index = 0;
	float prevTrans = 255;
	for (i = 0; i < AOIT_NODE_COUNT; ++i)
	{
		if (fragmentDepth > depth[i])
		{
			++index;
			prevTrans = trans[i];
		}
	}

	// Make room for the new fragment
	for (i = AOIT_NODE_COUNT - 1; i >= 0; --i)
	{
		if (index <= i)
		{
			depth[i + 1] = depth[i];
			trans[i + 1] = trans[i] * fragmentTrans;
			color[i + 1] = color[i];
		}
	}

	// Insert new fragment
	const float newFragTrans = fragmentTrans * prevTrans;
	const uint newFragColor = PackRGB(fragmentColor * (1.0f - fragmentTrans));

	depth[index] = fragmentDepth;
	trans[index] = newFragTrans;
	color[index] = newFragColor;

	float EMPTY_NODE = uintBitsToFloat(floatBitsToUint(float(AOIT_EMPTY_NODE_DEPTH)) & uint(AOIT_TRANS_MASK));

	if (uintBitsToFloat(floatBitsToUint(float(depth[AOIT_NODE_COUNT])) & uint(AOIT_TRANS_MASK)) != EMPTY_NODE)
	{
		vec3 toBeRemovedColor = UnpackRGB(color[AOIT_NODE_COUNT]);
		vec3 toBeAccumulatedColor = UnpackRGB(color[AOIT_NODE_COUNT - 1]);
		color[AOIT_NODE_COUNT - 1] = PackRGB(toBeAccumulatedColor + toBeRemovedColor * trans[AOIT_NODE_COUNT - 1] * (1.0f / trans[AOIT_NODE_COUNT - 2]));
		trans[AOIT_NODE_COUNT - 1] = trans[AOIT_NODE_COUNT];
	}

	// Pack AOIT data
	for (i = 0; i < AOIT_NODE_COUNT; ++i)
	{
		nodeArray[i].depth = depth[i];
		nodeArray[i].trans = trans[i];
		nodeArray[i].color = color[i];
	}
}

#ifdef AOIT_UNORDERED_ACCESS
void WriteNewPixelToAOIT(vec2 position, float depth, vec4 color)
{
	// From now on serialize all UAV accesses (with respect to other fragments shaded in flight which map to the same pixel)
	AOITNode nodeArray[AOIT_NODE_COUNT];
	uvec2 pixelAddr = uvec2(position);

	// Load AOIT control surface
	AOITControlSurface ctrlSurface = AOITControlSurface(false, false, 0.0f);
	AOITLoadControlSurfaceUAV(pixelAddr, ctrlSurface);

	// If we are modifying this pixel for the first time we need to clear the AOIT data
	if (ctrlSurface.clear)
	{
		// Clear AOIT data and initialize it with first transparent layer
		AOITData data;
		AOITClearData(data, depth, color);

		// Store AOIT data
		uint addr = AOITAddrGenUAV(pixelAddr);
#if AOIT_NODE_COUNT != 2
		AOITDepthDataUAVData[addr].depth = data.depth;
#endif
		AOITColorDataUAVData[addr].color = data.color;

		imageStore(AOITClearMaskUAV, ivec2(pixelAddr), uvec4(0));
	}
	else
	{
		// Load AOIT data
		AOITLoadDataUAV(pixelAddr, nodeArray);

		// Update AOIT data
		AOITInsertFragment(depth, 1.0f - color.w, color.xyz, nodeArray);

		// Store AOIT data
		AOITStoreDataUAV(pixelAddr, nodeArray);
	}
}
#endif
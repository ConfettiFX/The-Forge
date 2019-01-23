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

/************************************************************************/
// Defines
/************************************************************************/
#define AOIT_EMPTY_NODE_DEPTH   (3.40282E38)
#define AOIT_TRANS_BIT_COUNT    (8)
#define AOIT_MAX_UNNORM_TRANS   ((1 << AOIT_TRANS_BIT_COUNT) - 1)
#define AOIT_TRANS_MASK         (0xFFFFFFFF - (uint)AOIT_MAX_UNNORM_TRANS)

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
	float4 depth[AOIT_RT_COUNT];
	uint4 color[AOIT_RT_COUNT];
};

struct AOITDepthData
{
	float4 depth[AOIT_RT_COUNT];
};

struct AOITColorData
{
	uint4 color[AOIT_RT_COUNT];
};

/************************************************************************/
// Shader resources
/************************************************************************/
#ifdef AOIT_UNORDERED_ACCESS
RasterizerOrderedTexture2D<uint> AOITClearMaskUAV : register(u10);
RWStructuredBuffer<AOITDepthData> AOITDepthDataUAV : register(u11);
RWStructuredBuffer<AOITColorData> AOITColorDataUAV : register(u12);
#endif

#ifdef AOIT_ORDERED_ACCESS
Texture2D<uint> AOITClearMaskSRV : register(t10);
StructuredBuffer<AOITDepthData> AOITDepthDataSRV : register(t11);
StructuredBuffer<AOITColorData> AOITColorDataSRV : register(t12);
#endif

/************************************************************************/
// Helper functions
/************************************************************************/
uint PackRGB(float3 color)
{
	uint3 u = (uint3)(saturate(color) * 255 + 0.5);
	uint  packedOutput = (u.z << 16UL) | (u.y << 8UL) | u.x;
	return packedOutput;
}

float3 UnpackRGB(uint packedInput)
{
	float3 unpackedOutput;
	uint3 p = uint3((packedInput & 0xFFUL),
		(packedInput >> 8UL) & 0xFFUL,
		(packedInput >> 16UL) & 0xFFUL);

	unpackedOutput = ((float3)p) / 255;
	return unpackedOutput;
}

uint PackRGBA(float4 color)
{
	uint4 u = (uint4)(saturate(color) * 255 + 0.5);
	uint packedOutput = (u.w << 24UL) | (u.z << 16UL) | (u.y << 8UL) | u.x;
	return packedOutput;
}

float4 UnpackRGBA(uint packedInput)
{
	float4 unpackedOutput;
	uint4 p = uint4((packedInput & 0xFFUL),
		(packedInput >> 8UL) & 0xFFUL,
		(packedInput >> 16UL) & 0xFFUL,
		(packedInput >> 24UL));

	unpackedOutput = ((float4)p) / 255;
	return unpackedOutput;
}

float UnpackUnnormAlpha(uint packedInput)
{
	return (float)(packedInput >> 24UL);
}

uint AOITAddrGen(uint2 addr2D, uint width)
{
#ifdef AOIT_TILED_ADDRESSING
	width = width >> 1U;
	uint2 tileAddr2D = addr2D >> 1U;
	uint tileAddr1D = (tileAddr2D[0] + width * tileAddr2D[1]) << 2U;
	uint2 pixelAddr2D = addr2D & 0x1U;
	uint pixelAddr1D = (pixelAddr2D[1] << 1U) + pixelAddr2D[0];
	return tileAddr1D | pixelAddr1D;
#else
	return addr2D[0] + width * addr2D[1];
#endif
}

#ifdef AOIT_UNORDERED_ACCESS
uint AOITAddrGenUAV(uint2 addr2D)
{
	uint2 dim;
	AOITClearMaskUAV.GetDimensions(dim[0], dim[1]);
	return AOITAddrGen(addr2D, dim[0]);
}
#endif

#ifdef AOIT_ORDERED_ACCESS
uint AOITAddrGenSRV(uint2 addr2D)
{
	uint2 dim;
	AOITClearMaskSRV.GetDimensions(dim[0], dim[1]);
	return AOITAddrGen(addr2D, dim[0]);
}
#endif

void AOITLoadControlSurface(in uint data, inout AOITControlSurface surface)
{
	surface.clear = data & 0x1 ? true : false;
	surface.opaque = data & 0x2 ? true : false;
	surface.depth = asfloat((data & 0xFFFFFFFCUL) | 0x3UL);
}

/************************************************************************/
// Load/Store/Clear functions
/************************************************************************/
#ifdef AOIT_UNORDERED_ACCESS
void AOITLoadControlSurfaceUAV(in uint2 pixelAddr, inout AOITControlSurface surface)
{
	uint data = AOITClearMaskUAV[pixelAddr];
	AOITLoadControlSurface(data, surface);
}

void AOITStoreControlSurfaceUAV(uint2 pixelAddr, in AOITControlSurface surface)
{
	uint data;
	data = asuint(surface.depth) & 0xFFFFFFFCUL;
	data |= surface.opaque ? 0x2 : 0x0;
	data |= surface.clear ? 0x1 : 0x0;
	AOITClearMaskUAV[pixelAddr] = data;
}

void AOITLoadDataUAV(in uint2 pixelAddr, out AOITNode nodeArray[AOIT_NODE_COUNT])
{
	AOITData data;
	uint addr = AOITAddrGenUAV(pixelAddr);
	data.color = AOITColorDataUAV[addr];

#if AOIT_NODE_COUNT == 2
	[unroll] for (uint i = 0; i < 2; ++i)
	{
		AOITNode node = { asfloat(data.color[0][i]), UnpackUnnormAlpha(data.color[0][2 + i]), data.color[0][2 + i] & 0xFFFFFFUL };
		nodeArray[i] = node;
	}
#else
	data.depth = AOITDepthDataUAV[addr];
	[unroll] for (uint i = 0; i < AOIT_RT_COUNT; ++i)
	{
		[unroll] for (uint j = 0; j < 4; ++j)
		{
			AOITNode node = { data.depth[i][j],
								UnpackUnnormAlpha(data.color[i][j]),
								data.color[i][j] & 0xFFFFFFUL };
			nodeArray[4 * i + j] = node;
		}
	}
#endif
}

void AOITStoreDataUAV(in uint2 pixelAddr, AOITNode nodeArray[AOIT_NODE_COUNT])
{
	AOITData data;
	uint addr = AOITAddrGenUAV(pixelAddr);

#if AOIT_NODE_COUNT == 2
	[unroll] for (uint i = 0; i < 2; ++i)
	{
		data.color[0][i] = asuint((asuint(float(nodeArray[i].depth)) & (uint)AOIT_TRANS_MASK) | (uint)(nodeArray[i].trans));
		data.color[0][2 + i] = (nodeArray[i].color & 0xFFFFFFUL) | (((uint)(nodeArray[i].trans)) << 24UL);
	}
#else
	[unroll] for (uint i = 0; i < AOIT_RT_COUNT; ++i)
	{
		[unroll] for (uint j = 0; j < 4; ++j)
		{
			data.depth[i][j] = nodeArray[4 * i + j].depth;
			data.color[i][j] = (nodeArray[4 * i + j].color & 0xFFFFFFUL) | (((uint)(nodeArray[4 * i + j].trans)) << 24UL);
		}
	}
	AOITDepthDataUAV[addr] = data.depth;
#endif
	AOITColorDataUAV[addr] = data.color;
}
#endif

#ifdef AOIT_ORDERED_ACCESS
void AOITLoadControlSurfaceSRV(in uint2 pixelAddr, inout AOITControlSurface surface)
{
	uint data = AOITClearMaskSRV[pixelAddr];
	AOITLoadControlSurface(data, surface);
}

void AOITLoadDataSRV(in uint2 pixelAddr, out AOITNode nodeArray[AOIT_NODE_COUNT])
{
	AOITData data;
	uint addr = AOITAddrGenSRV(pixelAddr);
	data.color = AOITColorDataSRV[addr];

#if AOIT_NODE_COUNT == 2
	[unroll]for (uint i = 0; i < 2; ++i)
	{
		AOITNode node = { 0, UnpackUnnormAlpha(data.color[0][2 + i]),
			data.color[0][2 + i] & 0xFFFFFFUL };
		nodeArray[i] = node;
	}
#else
	[unroll]for (uint i = 0; i < AOIT_RT_COUNT; ++i)
	{
		[unroll]for (uint j = 0; j < 4; ++j)
		{
			AOITNode node = { 0, UnpackUnnormAlpha(data.color[i][j]),
				data.color[i][j] & 0xFFFFFFUL };
			nodeArray[4 * i + j] = node;
		}
	}
#endif
}
#endif

/************************************************************************/
// Other functions
/************************************************************************/
void AOITClearData(inout AOITData data, float depth, float4 color)
{
	uint packedColor = PackRGBA(float4(0, 0, 0, 1.0f - color.w));

#if AOIT_NODE_COUNT == 2
	data.depth[0] = 0;
	data.color[0][0] = asuint(depth);
	data.color[0][1] = asuint((float)AOIT_EMPTY_NODE_DEPTH);
	data.color[0][2] = PackRGBA(float4(color.www * color.xyz, 1.0f - color.w));
	data.color[0][3] = packedColor;
#else
	[unroll] for (uint i = 0; i < AOIT_RT_COUNT; ++i)
	{
		data.depth[i] = AOIT_EMPTY_NODE_DEPTH;
		data.color[i] = packedColor;
	}
	data.depth[0][0] = depth;
	data.color[0][0] = PackRGBA(float4(color.www * color.xyz, 1.0f - color.w));
#endif
}

void AOITInsertFragment(in float fragmentDepth, in float fragmentTrans, in float3 fragmentColor, inout AOITNode nodeArray[AOIT_NODE_COUNT])
{
	int i;

	float depth[AOIT_NODE_COUNT + 1];
	float trans[AOIT_NODE_COUNT + 1];
	uint color[AOIT_NODE_COUNT + 1];

	// Unpack AOIT data
	[unroll] for (i = 0; i < AOIT_NODE_COUNT; ++i)
	{
		depth[i] = nodeArray[i].depth;
		trans[i] = nodeArray[i].trans;
		color[i] = nodeArray[i].color;
	}

	// Find insertion index
	int index = 0;
	float prevTrans = 255;
	[unroll] for (i = 0; i < AOIT_NODE_COUNT; ++i)
	{
		if (fragmentDepth > depth[i])
		{
			++index;
			prevTrans = trans[i];
		}
	}

	// Make room for the new fragment
	[unroll] for (i = AOIT_NODE_COUNT - 1; i >= 0; --i)
	{
		[flatten] if (index <= i)
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

	float EMPTY_NODE = asfloat(asuint(float(AOIT_EMPTY_NODE_DEPTH)) & (uint)AOIT_TRANS_MASK);

	[flatten] if (asfloat(asuint(float(depth[AOIT_NODE_COUNT])) & (uint)AOIT_TRANS_MASK) != EMPTY_NODE)
	{
		float3 toBeRemovedColor = UnpackRGB(color[AOIT_NODE_COUNT]);
		float3 toBeAccumulatedColor = UnpackRGB(color[AOIT_NODE_COUNT - 1]);
		color[AOIT_NODE_COUNT - 1] = PackRGB(toBeAccumulatedColor + toBeRemovedColor * trans[AOIT_NODE_COUNT - 1] * rcp(trans[AOIT_NODE_COUNT - 2]));
		trans[AOIT_NODE_COUNT - 1] = trans[AOIT_NODE_COUNT];
	}

	// Pack AOIT data
	[unroll] for (i = 0; i < AOIT_NODE_COUNT; ++i)
	{
		nodeArray[i].depth = depth[i];
		nodeArray[i].trans = trans[i];
		nodeArray[i].color = color[i];
	}
}

#ifdef AOIT_UNORDERED_ACCESS
void WriteNewPixelToAOIT(float2 position, float depth, float4 color)
{
	// From now on serialize all UAV accesses (with respect to other fragments shaded in flight which map to the same pixel)
	AOITNode nodeArray[AOIT_NODE_COUNT];
	uint2 pixelAddr = uint2(position);

	// Load AOIT control surface
	AOITControlSurface ctrlSurface = (AOITControlSurface)0;
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
		AOITDepthDataUAV[addr] = data.depth;
#endif
		AOITColorDataUAV[addr] = data.color;

		AOITClearMaskUAV[pixelAddr] = 0;
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
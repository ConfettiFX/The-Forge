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


/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
	struct Uniforms_PackedAtlasQuads_CB
	{
		float4 mQuadsData[192];
	};
	constant Uniforms_PackedAtlasQuads_CB & PackedAtlasQuads_CB;
	struct AtlasQuads
	{
		float4 mPosData;
		float4 mMiscData;
		float4 mTexCoordData;
	};
	struct VSOutput
	{
		float4 Position [[position]];
		float2 UV;
		float4 MiscData;
	};
	float2 ScaleOffset(float2 a, float4 p)
	{
		return ((a * (p).xy) + (p).zw);
	};
	VSOutput main(uint vertexID)
	{
		VSOutput result;
		const uint verticesPerQuad = (const uint)(6);
		uint quadID = (vertexID / verticesPerQuad);
		uint quadVertexID = (vertexID - (quadID * verticesPerQuad));
		float2 pos = float2((-1.0), 1.0);
		if (quadVertexID == 1)
		{
			(pos = float2((-1.0), (-1.0)));
		}
		if (quadVertexID == 2)
		{
			(pos = float2(1.0, (-1.0)));
		}
		if (quadVertexID == 3)
		{
			(pos = float2(1.0, (-1.0)));
		}
		if (quadVertexID == 4)
		{
			(pos = float2(1.0, 1.0));
		}
		if (quadVertexID == 5)
		{
			(pos = float2((-1.0), 1.0));
		}
		const uint registersPerQuad = (const uint)(3);
		float4 quadData[registersPerQuad];
		for (int i = 0; ((uint)(i) < registersPerQuad); (++i))
		{
			(quadData[i] = PackedAtlasQuads_CB.mQuadsData[((quadID * registersPerQuad) + (uint)(i))]);
		}
		AtlasQuads atlasQuad;
		atlasQuad.mPosData = quadData[0];
		atlasQuad.mMiscData = quadData[1];
		atlasQuad.mTexCoordData = quadData[2];
		
		
		((result).Position = float4(ScaleOffset(pos, (atlasQuad).mPosData), 0.0, 1.0));
		((result).MiscData = (atlasQuad).mMiscData);
		((result).UV = ScaleOffset(((float2(0.5, (-0.5)) * pos) + (float2)(0.5)), (atlasQuad).mTexCoordData));
		return result;
	};
	
	Vertex_Shader(
				  constant Uniforms_PackedAtlasQuads_CB & PackedAtlasQuads_CB) :
	PackedAtlasQuads_CB(PackedAtlasQuads_CB) {}
};

struct VSData {
    constant Vertex_Shader::Uniforms_PackedAtlasQuads_CB & PackedAtlasQuads_CB [[id(0)]];
};

vertex Vertex_Shader::VSOutput stageMain(
    uint vertexID [[vertex_id]],
    constant VSData& vsData     [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
	uint vertexID0;
	vertexID0 = vertexID;
	Vertex_Shader main(vsData.PackedAtlasQuads_CB);
	return main.main(vertexID0);
}

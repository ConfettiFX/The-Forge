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
#define QUADS_ARRAY_REGS 192

cbuffer PackedAtlasQuads_CB : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4 mQuadsData[QUADS_ARRAY_REGS];
};

struct AtlasQuads
{
	float4 mPosData;
	float4 mMiscData;
	float4 mTexCoordData;
};

struct VSOutput
{
    float4 Position : SV_Position;
	float2 UV       : TEXCOORD0;
    float4 MiscData : TEXCOORD1;
};

float2 ScaleOffset(float2 a, float4 p)
{
	return a * p.xy + p.zw;
}

VSOutput main( uint vertexID : SV_VertexID )
{
	VSOutput result;

	const uint verticesPerQuad = 6;
    uint quadID = vertexID / verticesPerQuad;
    uint quadVertexID = vertexID - quadID * verticesPerQuad;

	float2 pos = float2(-1.0,1.0 );
	if( quadVertexID == 1 ) pos = float2(-1.0, -1.0 );
    if( quadVertexID == 2 ) pos = float2( 1.0,-1.0 );
    if( quadVertexID == 3 ) pos = float2( 1.0,-1.0 );
    if( quadVertexID == 4 ) pos = float2(1.0, 1.0 );
    if( quadVertexID == 5 ) pos = float2( -1.0, 1.0 );

	const uint registersPerQuad = 3;
	float4 quadData[ registersPerQuad ];

	for(int i = 0; i < registersPerQuad; ++i)
	{
		quadData[i] = mQuadsData[quadID * registersPerQuad + i];
	}

	AtlasQuads atlasQuad = (AtlasQuads) quadData;

	result.Position = float4(ScaleOffset(pos, atlasQuad.mPosData), 0.0, 1.0);
	result.MiscData = atlasQuad.mMiscData;
	result.UV = ScaleOffset(float2(0.5, -0.5) * pos + 0.5,  atlasQuad.mTexCoordData);

	return result;
}
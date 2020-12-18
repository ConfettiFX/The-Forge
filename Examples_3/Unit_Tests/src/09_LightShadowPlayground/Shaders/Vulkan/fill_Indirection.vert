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
#version 450 core


layout(location = 0) out vec4 vertOutput_TEXCOORD0;

layout(column_major, UPDATE_FREQ_PER_FRAME, binding = 0) uniform PackedAtlasQuads_CB
{
    vec4 mQuadsData[192];
};

struct AtlasQuads
{
    vec4 mPosData;
    vec4 mMiscData;
};
struct VSOutput
{
    vec4 Position;
    vec4 MiscData;
};
vec2 ScaleOffset(vec2 a, vec4 p)
{
    return ((a * (p).xy) + (p).zw);
}
VSOutput HLSLmain(uint vertexID)
{
    VSOutput result;
    const uint verticesPerQuad = uint (6);
    uint quadID = (vertexID / verticesPerQuad);
    uint quadVertexID = (vertexID - (quadID * verticesPerQuad));
    vec2 pos = vec2((-1.0), 1.0);
    if((quadVertexID == uint (1)))
    {
        (pos = vec2((-1.0), (-1.0)));
    }
    if((quadVertexID == uint (2)))
    {
        (pos = vec2(1.0, (-1.0)));
    }
    if((quadVertexID == uint (3)))
    {
        (pos = vec2(1.0, (-1.0)));
    }
    if((quadVertexID == uint (4)))
    {
        (pos = vec2(1.0, 1.0));
    }
    if((quadVertexID == uint (5)))
    {
        (pos = vec2((-1.0), 1.0));
    }
    const uint registersPerQuad = uint (2);
    vec4 quadData[registersPerQuad];
    for (int i = 0; (uint (i) < registersPerQuad); (++i))
    {
        (quadData[i] = mQuadsData[((quadID * registersPerQuad) + uint (i))]);
    }
    //AtlasQuads atlasQuad = AtlasQuads (quadData);
	AtlasQuads atlasQuad;
	atlasQuad.mPosData = quadData[0];
	atlasQuad.mMiscData = quadData[1];

    ((result).Position = vec4(ScaleOffset(pos, (atlasQuad).mPosData), 0.0, 1.0));
    ((result).MiscData = (atlasQuad).mMiscData);
    return result;
}
void main()
{
    uint vertexID;
    vertexID = gl_VertexIndex;
    VSOutput result = HLSLmain(vertexID);
    gl_Position = result.Position;
    vertOutput_TEXCOORD0 = result.MiscData;
}



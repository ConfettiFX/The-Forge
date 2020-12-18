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


layout(location = 0) out vec2 vertOutput_TEXCOORD0;
layout(location = 1) out vec4 vertOutput_TEXCOORD1;

layout(row_major, UPDATE_FREQ_PER_FRAME, binding = 0) uniform AtlasQuads_CB
{
    vec4 mPosData;
    vec4 mMiscData;
    vec4 mTexCoordData;
};

struct VSOutput
{
    vec4 Position;
    vec2 UV;
    vec4 MiscData;
};
vec2 ScaleOffset(vec2 a, vec4 p)
{
    return ((a * (p).xy) + (p).zw);
}
VSOutput HLSLmain(uint vertexID)
{
    VSOutput result;
    vec2 pos = vec2((-1.0), 1.0);
    if((vertexID == uint (1)))
    {
        (pos = vec2((-1.0), (-1.0)));
    }
    if((vertexID == uint (2)))
    {
        (pos = vec2(1.0, (-1.0)));
    }
    if((vertexID == uint (3)))
    {
        (pos = vec2(1.0, (-1.0)));
    }
    if((vertexID == uint (4)))
    {
        (pos = vec2(1.0, 1.0));
    }
    if((vertexID == uint (5)))
    {
        (pos = vec2((-1.0), 1.0));
    }
    ((result).Position = vec4(ScaleOffset(pos, mPosData), 0.0, 1.0));
    ((result).MiscData = mMiscData);
    ((result).UV = ScaleOffset(((vec2(0.5, (-0.5)) * pos) + vec2 (0.5)), mTexCoordData));
    return result;
}
void main()
{
    uint vertexID;
    vertexID = gl_VertexIndex;
    VSOutput result = HLSLmain(vertexID);
    gl_Position = result.Position;
    vertOutput_TEXCOORD0 = result.UV;
    vertOutput_TEXCOORD1 = result.MiscData;
}





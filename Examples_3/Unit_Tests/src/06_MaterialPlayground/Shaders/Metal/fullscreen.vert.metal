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
    struct VSOutput
    {
        float4 Position [[position]];
        float2 UV;
    };
    VSOutput main(    uint VertexID)
    {
        VSOutput output;
        (output.UV = float2(((VertexID << (uint)(1)) & (uint)(2)), (VertexID & (uint)(2))));
        (output.Position = float4(((output.UV.xy * float2(2, (-2))) + float2((-1), 1)), 0, 1));
        return output;
    };

    Vertex_Shader(
) {}
};


vertex Vertex_Shader::VSOutput stageMain(
uint VertexID [[vertex_id]])
{
    uint VertexID0;
    VertexID0 = VertexID;
    Vertex_Shader main;
    return main.main(VertexID0);
}

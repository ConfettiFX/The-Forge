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

struct PsIn
{
    vec4 Position;
    vec2 ScreenPos;
};
PsIn HLSLmain(uint vertexID)
{
    PsIn result;
    (((result).Position).x = float ((((vertexID == uint (2)))?(float (3.0)):(float ((-1.0))))));
    (((result).Position).y = float ((((vertexID == uint (0)))?(float ((-3.0))):(float (1.0)))));
    (((result).Position).zw = vec2(0, 1));
    ((result).ScreenPos = ((result).Position).xy);
    return result;
}
void main()
{
    uint vertexID;
    vertexID = gl_VertexIndex;
    PsIn result = HLSLmain(vertexID);
    gl_Position = result.Position;
    vertOutput_TEXCOORD0 = result.ScreenPos;
}

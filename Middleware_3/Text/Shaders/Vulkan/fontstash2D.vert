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

#version 450 core

layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 TEXCOORD0;
layout(location = 0) out vec2 vertOutput_TEXCOORD0;

struct VsIn
{
    vec2 position;
    vec2 texCoord;
};

struct PsIn
{
    vec4 position;
    vec2 texCoord;
};

layout(push_constant) uniform uRootConstants_Block
{
    vec4 color;
    vec2 scaleBias;
} uRootConstants;

PsIn HLSLmain(VsIn In)
{
    PsIn Out;
    ((Out).position = vec4((In).position, 0.0, 1.0));
    (((Out).position).xy = ((((Out).position).xy * (uRootConstants.scaleBias).xy) + vec2((-1.0), 1.0)));
    ((Out).texCoord = (In).texCoord);
    return Out;
}

void main()
{
    VsIn In;
    In.position = Position;
    In.texCoord = TEXCOORD0;
    PsIn result = HLSLmain(In);
    gl_Position = result.position;
    vertOutput_TEXCOORD0 = result.texCoord;
}

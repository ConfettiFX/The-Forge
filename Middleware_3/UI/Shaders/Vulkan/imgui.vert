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

layout(location = 0) in vec2 POSITION;
layout(location = 1) in vec2 TEXCOORD0;
layout(location = 2) in vec4 COLOR0;
layout(location = 0) out vec4 vertOutput_COLOR0;
layout(location = 1) out vec2 vertOutput_TEXCOORD0;

layout(set = 0, binding = 0) uniform uniformBlockVS
{
    mat4 ProjectionMatrix;
};

struct VS_INPUT
{
    vec2 pos;
    vec2 uv;
    vec4 col;
};

struct PS_INPUT
{
    vec4 pos;
    vec4 col;
    vec2 uv;
};

PS_INPUT HLSLmain(VS_INPUT input0)
{
    PS_INPUT output0;
    ((output0).pos = ((ProjectionMatrix)*(vec4(((input0).pos).xy, 0.0, 1.0))));
    ((output0).col = (input0).col);
    ((output0).uv = (input0).uv);
    return output0;
}

void main()
{
    VS_INPUT input0;
    input0.pos = POSITION;
    input0.uv = TEXCOORD0;
    input0.col = COLOR0;
    PS_INPUT result = HLSLmain(input0);
    gl_Position = result.pos;
    vertOutput_COLOR0 = result.col;
    vertOutput_TEXCOORD0 = result.uv;
}

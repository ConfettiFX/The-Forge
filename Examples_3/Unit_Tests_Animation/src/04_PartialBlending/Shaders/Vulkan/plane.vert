#version 450 core

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

// Shader for ground plane in Unit Tests Animation

layout(location = 0) in vec4 POSITION;
layout(location = 1) in vec2 TEXCOORD0;
layout(location = 0) out vec2 vertOutput_TEXCOORD;

layout(set = 0, binding = 0) uniform uniformBlock
{
    mat4 mvp;
    mat4 toWorld;
};

struct VSInput
{
    vec4 Position;
    vec2 TexCoord;
};
struct VSOutput
{
    vec4 Position;
    vec2 TexCoord;
};
VSOutput HLSLmain(VSInput input0)
{
    VSOutput result;
    mat4 tempMat = ((mvp)*(toWorld));
    ((result).Position = ((tempMat)*((input0).Position)));
    ((result).TexCoord = (input0).TexCoord);
    return result;
}
void main()
{
    VSInput input0;
    input0.Position = POSITION;
    input0.TexCoord = TEXCOORD0;
    VSOutput result = HLSLmain(input0);
    gl_Position = result.Position;
    vertOutput_TEXCOORD = result.TexCoord;
}


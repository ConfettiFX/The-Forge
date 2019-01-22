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
#version 450 core
#if !defined(WINDOWS) && !defined(ANDROID) && !defined(LINUX)
#define WINDOWS 	// Assume windows if no platform define has been added to the shader
#endif



layout(location = 0) in vec4 POSITION;
layout(location = 0) out vec3 vertOutput_POSITION;

layout(push_constant) uniform RootConstantCameraSky_Block
{
    mat4 projView;
    vec3 camPos;
}RootConstantCameraSky;

struct VSInput
{
    vec4 Position;
};
struct VSOutput
{
    vec4 Position;
    vec3 pos;
};
VSOutput HLSLmain(VSInput input0)
{
    VSOutput result;
    ((result).Position = ((RootConstantCameraSky.projView)*((input0).Position)));
    ((result).Position = ((result).Position).xyww);
    ((result).pos = ((input0).Position).xyz);
    return result;
}
void main()
{
    VSInput input0;
    input0.Position = POSITION;
    VSOutput result = HLSLmain(input0);
    gl_Position = result.Position;
    vertOutput_POSITION = result.pos;
}

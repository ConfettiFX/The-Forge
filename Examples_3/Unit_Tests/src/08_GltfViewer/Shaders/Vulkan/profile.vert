#version 450 core

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

layout(location = 0) in vec2 POSITION;
layout(location = 1) in vec4 COLOR;
layout(location = 0) out vec4 vertOutput_COLOR;

struct VSInput
{
    vec2 Position;
    vec4 Color;
};
struct VSOutput
{
    vec4 Position;
    vec4 Color;
};
VSOutput HLSLmain(VSInput input1)
{
    VSOutput result;
    ((result).Position = vec4(((input1).Position).x, ((input1).Position).y, 0.0, 1.0));
    ((result).Color = (input1).Color);
    return result;
}
void main()
{
    VSInput input1;
    input1.Position = POSITION;
    input1.Color = COLOR;
    VSOutput result = HLSLmain(input1);
    gl_Position = result.Position;
    vertOutput_COLOR = result.Color;
}

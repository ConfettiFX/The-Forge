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

// Shader for ground plane in Unit Tests Animation

layout(location = 0) in vec2 fragInput_TEXCOORD;
layout(location = 0) out vec4 rast_FragData0; 

struct VSOutput
{
    vec4 Position;
    vec2 TexCoord;
};

vec4 HLSLmain(VSOutput input0)
{
    float tol = 0.0025000000;
    float res = 0.05;
    vec4 backgroundColor = vec4(0.49f, 0.64f, 0.68f, 1.0f); // blue
    vec4 lineColor = vec4(0.39, 0.41, 0.37, 1.0); // grey
    vec4 originColor = vec4(0.0, 0.0, 0.0, 1.0); // black
    if(((abs((((input0).TexCoord).x - 0.5)) <= tol) && (abs((((input0).TexCoord).y - 0.5)) <= tol)))
    {
        return originColor;
    }
    else if (((((mod(((input0).TexCoord).x, res) >= (res - tol)) || (mod(((input0).TexCoord).x, res) < tol)) || (mod(((input0).TexCoord).y, res) >= (res - tol))) || (mod(((input0).TexCoord).y, res) < tol)))
    {
        return lineColor;
    }
    else
    {
        return backgroundColor;
    }
}
void main()
{
    VSOutput input0;
    input0.Position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    input0.TexCoord = fragInput_TEXCOORD;
    vec4 result = HLSLmain(input0);
    rast_FragData0 = result;
}
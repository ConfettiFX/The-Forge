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

layout(location = 0) in vec4 fragInput_COLOR;
layout(location = 0) out vec4 rast_FragData0; 

struct VSOutput
{
    vec4 Position;
    vec4 Color;
};
vec4 HLSLmain(VSOutput In)
{
    return (In).Color;
}
void main()
{
    VSOutput In;
    In.Position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    In.Color = fragInput_COLOR;
    vec4 result = HLSLmain(In);
    rast_FragData0 = result;
}

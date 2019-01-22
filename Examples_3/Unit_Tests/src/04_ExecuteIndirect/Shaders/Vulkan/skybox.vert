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


layout(location = 0) in vec4 vs_in_position;

layout (std140, set=0, binding=0) uniform uniformBlock
{
    uniform mat4 viewProject;
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out INVOCATION
{
    vec4 texcoord;
    int side;
} vs_out;

void main(void)
{
    vec4 p = vec4(vs_in_position.xyz,1.0);
    mat4 m = viewProject;
    m[3] = vec4(0.0, 0.0, 0.0, 1.0);
    p = m * p;
    gl_Position = p.xyww;
    vs_out.texcoord = vs_in_position;
    vs_out.side = int(vs_in_position.w);
}
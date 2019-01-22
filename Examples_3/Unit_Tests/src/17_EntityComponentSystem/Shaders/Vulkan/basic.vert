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



layout(location = 0) out vec3 vertOutput_COLOR0;
layout(location = 1) out vec2 vertOutput_TEXCOORD0;

layout(std140, set = 0, binding = 0) uniform VsParams
{
    float aspect;
};

struct InstanceData
{
    vec4 posScale;
    vec4 colorIndex;
};
layout(std140, set = 0, binding = 1) buffer instanceBuffer
{
    InstanceData instanceBuffer_Data[];
};

void main()
{
    uint vertexId   = gl_VertexIndex;
    uint instanceId = gl_InstanceIndex;
    float x = float (vertexId / uint (2));
    float y = float (vertexId & uint (1));
    gl_Position.x = ((instanceBuffer_Data[instanceId]).posScale).x + ((x - 0.5) * ((instanceBuffer_Data[instanceId]).posScale).z);
    gl_Position.y = ((instanceBuffer_Data[instanceId]).posScale).y + ((y - 0.5) * ((instanceBuffer_Data[instanceId]).posScale).z) * aspect;
    gl_Position.z = 0.0;
    gl_Position.w = 1.0;
    vertOutput_TEXCOORD0 = vec2(((x + ((instanceBuffer_Data[instanceId]).colorIndex).w) / float (8)), (float (1) - y));
    vertOutput_COLOR0 = ((instanceBuffer_Data[instanceId]).colorIndex).rgb;
}


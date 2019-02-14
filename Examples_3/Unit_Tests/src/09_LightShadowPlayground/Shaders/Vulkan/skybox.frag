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

layout(early_fragment_tests) in;

layout (set=0, binding=1) uniform sampler   skySampler;
layout (set=3, binding=0) uniform textureCube  Skybox;

layout(location = 0) out vec4 fs_out_color;

layout(set = 0, binding = 0) uniform renderSettingUniformBlock
{
    vec4 WindowDimension;
    int ShadowType;
};
layout(set = 1, binding = 0) uniform cameraUniformBlock
{
    mat4 View;
    mat4 Project;
    mat4 ViewProject;
    mat4 ViewInverse;
    mat4 ProjectInverse;
};

void main(void)
{
    vec3 uvw = transpose(mat3(View))*normalize((ProjectInverse*vec4(vec2(gl_FragCoord.xy)*vec2(2.0,-2.0)/WindowDimension.xy+vec2(-1.0,1.0),1.0,1.0)).xyz);
    fs_out_color = texture(samplerCube(Skybox, skySampler), uvw);
}
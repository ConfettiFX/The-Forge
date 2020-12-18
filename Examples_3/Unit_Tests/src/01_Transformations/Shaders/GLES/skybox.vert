#version 100
precision mediump float;
precision mediump int;
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

#define MAX_PLANETS 20

struct UniformBlock
{
	  mat4 mvp;
    mat4 toWorld[MAX_PLANETS];
    vec4 color[MAX_PLANETS];

    // Point Light Information
    vec3 lightPosition;
    vec3 lightColor;
};

attribute vec4 Position;

varying vec4 texcoord;

uniform UniformBlock uniformBlock;

void main()
{
  mat4 m = uniformBlock.mvp;
  vec4 p = vec4(Position.xyz,1.0);
  m[3] = vec4(0.0, 0.0, 0.0, 1.0);
  p = m * p;
  gl_Position = vec4(p.x, p.y, p.w, p.w);
  texcoord = Position;
}

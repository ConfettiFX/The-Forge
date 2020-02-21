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

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoords;

layout (std140, set=1, binding=0) uniform uniformBlock {
	uniform mat4 ProjectionViewMat;
    uniform mat4 ModelMatrixCapsule;
    uniform mat4 ModelMatrixCube;
};


layout(location = 0) out vec4 Color;
layout(location = 1) out vec2 out_texcoords;


void main ()
{
	mat4 mvp = ProjectionViewMat * ModelMatrixCube;
	gl_Position = mvp * vec4(Position.xyz, 1.0f);
	
	//vec4 fragPos = ModelMatrixCube * vec4(Position.xyz, 1.0f);
	
	//vec4 normal = normalize(ModelMatrixCube * vec4(Normal.xyz, 0.0f));
	
	out_texcoords = TexCoords;
	
	Color = vec4(0.0,1.0,0.0,1.0);
}

/*
 * Copyright (c) 2018 Kostas Anagnostou (https://twitter.com/KostasAAA).
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

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InUV;

layout(std140, set = 0, binding = 0) uniform cbPerPass 
{
	uniform mat4		projView;
};

layout(std140, set = 1, binding = 0) uniform cbPerProp 
{
	uniform mat4		world;
	uniform float		roughness;
	uniform float		metallic;
	uniform int			pbrMaterials;
	uniform float		pad;
};

layout(location = 0) out vec3 OutNormal;
layout(location = 1) out vec2 OutUV;

void main()
{
	gl_Position = projView * world * vec4(InPosition.xyz, 1.0f);
	OutNormal = mat3(world) * InNormal.xyz;
	OutUV = InUV;
}

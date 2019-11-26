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

layout(location = 0) in uvec4 InPosition;
layout(location = 1) in ivec4 InNormal;
layout(location = 2) in uvec2 InUV;
layout(location = 3) in uvec4 InBaseColor;
layout(location = 4) in uvec2 InMetallicRoughness;
layout(location = 5) in uvec2 InAlphaSettings;

layout(std140, UPDATE_FREQ_PER_FRAME, binding = 0) uniform cbPerPass 
{
	uniform mat4	projView;
	uniform vec4    camPos;
    uniform vec4    lightColor[4];
    uniform vec4    lightDirection[3];
	uniform ivec4   quantizationParams;
};

layout(std140, UPDATE_FREQ_PER_DRAW, binding = 0) uniform cbPerProp
{
	uniform mat4  world;
	uniform mat4  InvTranspose;
	uniform int   unlit;
	uniform int   hasAlbedoMap;
	uniform int   hasNormalMap;
	uniform int   hasMetallicRoughnessMap;
	uniform int   hasAOMap;
	uniform int   hasEmissiveMap;
	uniform vec4  centerOffset;
	uniform vec4  posOffset;
	uniform vec2  uvOffset;
	uniform vec2  uvScale;
  uniform float posScale;
  uniform uint  textureMapInfo;

	uniform uint  sparseTextureMapInfo;
  uniform float padding00;
  uniform float padding01;
  uniform float padding02;
};

layout(location = 0) out vec3 OutPosition;
layout(location = 1) out vec3 OutNormal;
layout(location = 2) out vec2 OutUV;
layout(location = 3) out vec4 OutBaseColor;
layout(location = 4) out vec2 OutMetallicRoughness;
layout(location = 5) out vec2 OutAlphaSettings;

void main()
{
    float unormPositionScale = float(1 << quantizationParams[0]) - 1.0f;
    float unormTexScale = float(1 << quantizationParams[1]) - 1.0f;
    float snormNormalScale = float(1 << (quantizationParams[2] - 1)) - 1.0f;
    float unorm16Scale = float(1 << 16) - 1.0f;
    float unorm8Scale = float(1 << 8) - 1.0f;

	vec4 inPosition = vec4((vec3(InPosition.xyz) / unormPositionScale) * posScale, 1.0f) + posOffset;
	inPosition += centerOffset;
	vec4 worldPosition = world * inPosition;
	worldPosition.xyz /= posScale;

	vec3 inNormal = vec3(InNormal.xyz) / snormNormalScale;

	gl_Position = projView * worldPosition;
	OutPosition = worldPosition.xyz;
	OutNormal = normalize((InvTranspose * vec4(inNormal.xyz,1.0f)).xyz);

	OutUV = vec2(InUV) / unormTexScale * uvScale + uvOffset;

	OutBaseColor = InBaseColor.rgba / unorm8Scale;
	OutMetallicRoughness = vec2(InMetallicRoughness.rg) / unorm16Scale;
	OutAlphaSettings = vec2(InAlphaSettings);
	OutAlphaSettings[0] = float(OutAlphaSettings[0]) + 0.5f;
	OutAlphaSettings[1] = float(OutAlphaSettings[1]) / unorm16Scale;
}

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

#extension GL_ARB_sparse_texture2 : enable
#extension GL_ARB_sparse_texture_clamp : enable

layout (set = 0, binding=8) uniform texture2D  SparseTexture;

layout (set = 0, binding=7) uniform sampler   uSampler0;

layout (set = 0, binding=9) uniform SparseTextureInfo
{
	uint Width;
	uint Height;
	uint pageWidth;
	uint pageHeight;

	uint DebugMode;
	uint ID;
	uint pad1;
	uint pad2;
	
	vec4 CameraPos;
};

layout(location = 0) in vec2 UV;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec3 Pos;

layout(location = 0) out vec4 outColor;

void main ()
{
  vec4 color = vec4(0.0);

  vec3 viewDir = normalize(CameraPos.xyz - Pos);
  float fresnel = pow(max(dot(viewDir, Normal), 0.0f), 5.0f);
  outColor = (fresnel) * vec4(1.0, 0.9, 0.65, 1.0);
  outColor.rgb *= 3.0f;
  //outColor = mix(vec4(1.0, 0.95, 0.85, 1.0), vec4(1.0, 1.0, 1.0, 1.0), fresnel); 
}
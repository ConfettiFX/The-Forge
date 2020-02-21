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

layout (set = 0, binding=10) uniform MipLevel
{
	uint mipLevel;
  uint pad0;
  uint pad1;
  uint pad2;
};

layout(location = 0) in vec2 UV;

layout(location = 0) out vec4 outColor;

void main ()
{
  sparseTextureARB(sampler2D(SparseTexture, uSampler0), UV, outColor, mipLevel);
  //sparseTextureClampARB(sampler2D(SparseTexture, uSampler0), UV, mipLevel, outColor);
  //outColor = textureLod(sampler2D(SparseTexture, uSampler0), UV, 1.0 );
}
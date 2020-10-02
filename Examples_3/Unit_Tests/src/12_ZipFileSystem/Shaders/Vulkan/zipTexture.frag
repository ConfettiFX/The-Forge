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

layout(location = 0) out vec4 outColor;

layout (set=0, binding=1) uniform texture2D  RightText;
layout (set=0, binding=2) uniform texture2D  LeftText;
layout (set=0, binding=3) uniform texture2D  TopText;
layout (set=0, binding=4) uniform texture2D  BotText;
layout (set=0, binding=5) uniform texture2D  FrontText;
layout (set=0, binding=6) uniform texture2D  BackText;
layout (set=0, binding=7) uniform texture2D  ZipTexture;
layout (set=0, binding=8) uniform sampler   uSampler0;

layout(location = 0) in vec4 Color;
layout(location = 1) in vec2 out_texCoords;

void main (void)
{
	
	outColor =	texture(sampler2D(ZipTexture, uSampler0), out_texCoords);
	
	//outColor = vec4(out_texCoords,0.0,1.0);
}
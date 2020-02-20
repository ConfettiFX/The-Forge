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

struct VSOutput {
	float4 Position : SV_POSITION;
    float2 texcoords:TEXCOORD;
};

SamplerState uSampler0 : register(s8);
Texture2D RightText : register(t1);
Texture2D LeftText : register(t2);
Texture2D TopText : register(t3);
Texture2D BotText : register(t4);
Texture2D FrontText : register(t5);
Texture2D BackText : register(t6);
Texture2D ZipTexture : register(t7);

float4 main (VSOutput input): SV_TARGET
{
	//return float4(input.texcoords.x,input.texcoords.y, 0.0,1.0);
	return ZipTexture.Sample(uSampler0,input.texcoords);
}
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

struct VSOutput {
    float4 pos : SV_Position;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
};

Texture2D uTexture0 : register(t1);
SamplerState uSampler0 : register(s2);

float4 main(VSOutput input) : SV_Target0
{
    float4 diffuse = uTexture0.Sample(uSampler0, input.uv);
    float lum = dot(diffuse.rgb, 0.333);
    diffuse.rgb = lerp(diffuse.rgb, lum.xxx, 0.8);
    diffuse.rgb *= input.color.rgb;
    return diffuse;
}
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
    float4 Position : SV_POSITION;
    float4 TexCoord : TEXCOORD;
};

Texture2D RightText : register(t0);
Texture2D LeftText : register(t1);
Texture2D TopText : register(t2);
Texture2D BotText : register(t3);
Texture2D FrontText : register(t4);
Texture2D BackText : register(t5);

SamplerState SkySampler : register(s0);

float4 main(VSOutput input) : SV_TARGET
{
    float2 newtextcoord;
    float side = round(input.TexCoord.w);

    if (side == 1.0f)
    {
        newtextcoord = (input.TexCoord.zy) / 20 + 0.5;
        newtextcoord = float2(1 - newtextcoord.x, 1 - newtextcoord.y);
        return RightText.Sample(SkySampler, newtextcoord);
    }
    else if (side == 2.0f)
    {
        newtextcoord = (input.TexCoord.zy) / 20 + 0.5;
        newtextcoord = float2(newtextcoord.x, 1 - newtextcoord.y);
        return LeftText.Sample(SkySampler, newtextcoord);
    }
    if (side == 4.0f)
    {
        newtextcoord = (input.TexCoord.xz) / 20 + 0.5;
        newtextcoord = float2(newtextcoord.x, 1 - newtextcoord.y);
        return BotText.Sample(SkySampler, newtextcoord);
    }
    else if (side == 5.0f)
    {
        newtextcoord = (input.TexCoord.xy) / 20 + 0.5;
        newtextcoord = float2(newtextcoord.x, 1 - newtextcoord.y);
        return FrontText.Sample(SkySampler, newtextcoord);
    }
    else if (side == 6.0f)
    {
        newtextcoord = (input.TexCoord.xy) / 20 + 0.5;
        newtextcoord = float2(1 - newtextcoord.x, 1 - newtextcoord.y);
        return BackText.Sample(SkySampler, newtextcoord);
    }
    else
    {
        newtextcoord = (input.TexCoord.xz) / 20 + 0.5;
        newtextcoord = float2(newtextcoord.x, newtextcoord.y);
        return TopText.Sample(SkySampler, newtextcoord);
    }
}

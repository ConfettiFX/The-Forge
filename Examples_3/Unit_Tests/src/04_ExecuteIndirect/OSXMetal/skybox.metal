/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

#include <metal_stdlib>
using namespace metal;

struct UniformBlock0
{
    float4x4 mvp;
};

struct VSInput
{
    float4 Position [[attribute(0)]];
};

struct VSOutput {
	float4 Position [[position]];
    float4 TexCoord;
};

vertex VSOutput VSMain(VSInput input [[stage_in]],
                       constant UniformBlock0& uniformBlock [[buffer(0)]])
{
	VSOutput result;
 
    float4 p = float4(input.Position.x*9, input.Position.y*9, input.Position.z*9, 1.0);
    float4x4 m =  uniformBlock.mvp;
    p = m * p;
    result.Position = p.xyww;
    result.TexCoord = float4(input.Position.x, input.Position.y, input.Position.z, input.Position.w);
	return result;
}

fragment float4 PSMain(VSOutput input [[stage_in]],
                       texture2d<float> RightText [[texture(1)]],
                       texture2d<float> LeftText [[texture(2)]],
                       texture2d<float> TopText [[texture(3)]],
                       texture2d<float> BotText [[texture(4)]],
                       texture2d<float> FrontText [[texture(5)]],
                       texture2d<float> BackText [[texture(6)]],
                       sampler uSampler0 [[sampler(7)]])
{
    float2 newtextcoord;
    float side = round(input.TexCoord.w);

	if(side==1.0f)
    {
        newtextcoord = (input.TexCoord.zy) / 20 + 0.5;
        newtextcoord = float2(1 - newtextcoord.x, 1 - newtextcoord.y);
        return RightText.sample(uSampler0, newtextcoord);
    }
    else if (side == 2.0f)
    {
        newtextcoord = (input.TexCoord.zy) / 20 + 0.5;
        newtextcoord = float2(newtextcoord.x, 1 - newtextcoord.y);
        return LeftText.sample(uSampler0, newtextcoord);
    }
    if (side == 4.0f)
    {
        newtextcoord = (input.TexCoord.xz) / 20 +0.5;
        newtextcoord = float2(newtextcoord.x, 1 - newtextcoord.y);
        return BotText.sample(uSampler0, newtextcoord);
    }
    else if (side == 5.0f)
    {
        newtextcoord = (input.TexCoord.xy) / 20 + 0.5;
        newtextcoord = float2(newtextcoord.x, 1 - newtextcoord.y);
        return FrontText.sample(uSampler0, newtextcoord);
    }
    else if (side == 6.0f)
    {
        newtextcoord = (input.TexCoord.xy) / 20 + 0.5;
        newtextcoord = float2(1-newtextcoord.x, 1 - newtextcoord.y);
        return BackText.sample(uSampler0, newtextcoord);
    }
	else
    {
        newtextcoord = (input.TexCoord.xz) / 20 + 0.5;
        newtextcoord = float2(newtextcoord.x, newtextcoord.y);
        return TopText.sample(uSampler0, newtextcoord);
    }
}

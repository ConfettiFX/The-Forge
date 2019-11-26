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

// Shader for Skybox in Unit Test 01 - Transformations

#include <metal_stdlib>
using namespace metal;

struct VSInput {
    float4 Position [[attribute(0)]];
};

struct VSOutput {
	float4 Position [[position]];
    float4 TexCoord;
};

struct FSData {
    texture2d<float,access::sample> RightText   [[id(0)]];
    texture2d<float,access::sample> LeftText    [[id(1)]];
    texture2d<float,access::sample> TopText     [[id(2)]];
    texture2d<float,access::sample> BotText     [[id(3)]];
    texture2d<float,access::sample> FrontText   [[id(4)]];
    texture2d<float,access::sample> BackText    [[id(5)]];
    sampler uSampler0                           [[id(6)]];
};

fragment float4 stageMain(VSOutput input            [[stage_in]],
                          constant FSData& fsData   [[buffer(UPDATE_FREQ_NONE)]]
)
{
    float2 newtextcoord;
    float side = round(input.TexCoord.w);

	if(side==1.0f)
    {
        newtextcoord = (input.TexCoord.zy) / 20 + 0.5;
        newtextcoord = float2(1 - newtextcoord.x, 1 - newtextcoord.y);
        return fsData.RightText.sample(fsData.uSampler0, newtextcoord);
    }
    else if (side == 2.0f)
    {
        newtextcoord = (input.TexCoord.zy) / 20 + 0.5;
        newtextcoord = float2(newtextcoord.x, 1 - newtextcoord.y);
        return fsData.LeftText.sample(fsData.uSampler0, newtextcoord);
    }
    else if (side == 4.0f)
    {
        newtextcoord = (input.TexCoord.xz) / 20 +0.5;
        newtextcoord = float2(newtextcoord.x, 1 - newtextcoord.y);
        return fsData.BotText.sample(fsData.uSampler0, newtextcoord);
    }
    else if (side == 5.0f)
    {
        newtextcoord = (input.TexCoord.xy) / 20 + 0.5;
        newtextcoord = float2(newtextcoord.x, 1 - newtextcoord.y);
        return fsData.FrontText.sample(fsData.uSampler0, newtextcoord);
    }
    else if (side == 6.0f)
    {
        newtextcoord = (input.TexCoord.xy) / 20 + 0.5;
        newtextcoord = float2(1-newtextcoord.x, 1 - newtextcoord.y);
        return fsData.BackText.sample(fsData.uSampler0, newtextcoord);
    }
	else
    {
        newtextcoord = (input.TexCoord.xz) / 20 + 0.5;
        newtextcoord = float2(newtextcoord.x, newtextcoord.y);
        return fsData.TopText.sample(fsData.uSampler0, newtextcoord);
    }
}

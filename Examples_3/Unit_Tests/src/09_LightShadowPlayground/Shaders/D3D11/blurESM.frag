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

#define MAX_WIDTH 31
#define MAX_GAUSSIAN_WEIGHTS_SIZE 64
#define PI 3.1415926535897932384626433832795
#define PI2 (2*3.1415926535897932384626433832795)

struct PsIn
{
    float4 position : SV_Position;
};

Texture2D shadowExpMap : register(t0);
SamplerState blurSampler;

cbuffer ESMInputConstants : register(b0)
{
    float2 ScreenDimension;
    float2 NearFarDist;
    float Exponent;
    uint BlurWidth;
    int IfHorizontalBlur;
    int padding;
};

cbuffer GaussianWeightsBuffer : register(b1)
{
    float4 GaussianWeights[MAX_GAUSSIAN_WEIGHTS_SIZE];
};



float main(PsIn input) : SV_Target
{
    uint3 u3uv = uint3(input.position.xy, 0);

    float DepthStrip[MAX_GAUSSIAN_WEIGHTS_SIZE];

    const uint width = clamp(BlurWidth, 0, MAX_WIDTH);
    const uint width2 = 2 * width;
    const uint width2p1 = 2 * width + 1;
    const float2 dim = ScreenDimension;

    DepthStrip[width] = shadowExpMap.SampleLevel(blurSampler, float2(u3uv.xy) / dim, 0).r;
    if (IfHorizontalBlur) {
        for (uint i = 0; i < width; ++i) {
            float2 f2uv = float2(u3uv.xy - uint2(width - i, 0))/ dim;
            float depthLeft = shadowExpMap.SampleLevel(blurSampler, f2uv, 0).r;

            f2uv = float2(u3uv.xy + uint2(width - i, 0)) / dim;
            float depthRight = shadowExpMap.SampleLevel(blurSampler, f2uv, 0).r;

            DepthStrip[i] = depthLeft;
            DepthStrip[width2 - i] = depthRight;
        }
    }
    else {
        for (uint i = 0; i < width; ++i) {
            float2 f2uv = float2(u3uv.xy - uint2(0, width - i)) / dim;
            float depthTop = shadowExpMap.SampleLevel(blurSampler, f2uv, 0).r;

            f2uv = float2(u3uv.xy + uint2(0, width - i)) / dim;
            float depthDown = shadowExpMap.SampleLevel(blurSampler, f2uv, 0).r;

            DepthStrip[i] = depthTop;
            DepthStrip[width2 - i] = depthDown;
        }
    }
    float sum = 0;
    for (uint i = 0; i < width2p1; ++i)
    {
        sum += DepthStrip[i] * GaussianWeights[i].x;
    }
    return sum;
}

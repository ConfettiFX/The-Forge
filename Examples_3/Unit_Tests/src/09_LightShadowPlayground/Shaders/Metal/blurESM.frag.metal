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

#include <metal_stdlib>
using namespace metal;

#define MAX_WIDTH 31U
#define MAX_GAUSSIAN_WEIGHTS_SIZE 64
#define PI 3.1415926535897932384626433832795
#define PI2 (2*3.1415926535897932384626433832795)

struct VSOutput {
	float4 position [[position]];
};

// layout (location = 0) out float FilteredPixel;
// layout(set = 0, binding = 1) uniform sampler blurSampler;

struct ESMInput
{
    float2 ScreenDimension;
    float2 NearFarDist;
    float Exponent;
    uint BlurWidth;
    int IfHorizontalBlur;
    int padding;
};

struct GaussianWeightsBlock
{
    float4 GaussianWeights[MAX_GAUSSIAN_WEIGHTS_SIZE];
};
fragment float4 stageMain( VSOutput input                                       [[stage_in]],
                          texture2d<float> shadowExpMap                        [[texture(0)]],
                          
                          constant ESMInput& ESMInputConstants          [[buffer(1)]],
                          constant GaussianWeightsBlock& GaussianWeightsBuffer[[buffer(2)]],
                          sampler blurSampler                                  [[sampler(0)]])
{
    uint2 i2uv = uint2(input.position.xy);

    float DepthStrip[MAX_GAUSSIAN_WEIGHTS_SIZE];

    const uint width = clamp(ESMInputConstants.BlurWidth, 0U, MAX_WIDTH);
    const uint width2 = 2 * width;
    const uint width2p1 = 2 * width + 1;
    const float2 dim = ESMInputConstants.ScreenDimension;

    // DepthStrip[width] = texture(sampler2D(shadowExpMap, blurSampler), float2(i2uv) / dim).r;
    DepthStrip[width] = shadowExpMap.sample(blurSampler, float2(i2uv) / dim).r;
    
    if (ESMInputConstants.IfHorizontalBlur != 0) {
        for (uint i = 0; i < width; ++i) {
            float2 f2uv = float2(i2uv.xy - uint2(width - i, 0))/ dim;
            // float depthLeft = texture(sampler2D(shadowExpMap, blurSampler), f2uv).r;
            float depthLeft = shadowExpMap.sample(blurSampler, f2uv).r;

            f2uv = float2(i2uv.xy + uint2(width - i, 0)) / dim;
            // float depthRight = texture(sampler2D(shadowExpMap, blurSampler), f2uv).r;
            float depthRight = shadowExpMap.sample(blurSampler, f2uv).r;

            DepthStrip[i] = depthLeft;
            DepthStrip[width2 - i] = depthRight;
        }
    }
    else {
        for (uint i = 0; i < width; ++i) {
            float2 f2uv = float2(i2uv.xy - uint2(0, width - i)) / dim;
            // float depthTop = texture(sampler2D(shadowExpMap, blurSampler), f2uv).r;
            float depthTop = shadowExpMap.sample(blurSampler, f2uv).r;

            f2uv = float2(i2uv.xy + uint2(0, width - i)) / dim;
            // float depthDown = texture(sampler2D(shadowExpMap, blurSampler), f2uv).r;
            float depthDown = shadowExpMap.sample(blurSampler, f2uv).r;

            DepthStrip[i] = depthTop;
            DepthStrip[width2 - i] = depthDown;
        }
    }
    float sum = 0;
    for (uint i = 0; i < width2p1; ++i)
    {
        sum += DepthStrip[i] * GaussianWeightsBuffer.GaussianWeights[i].x;
    }
    //FilteredPixel = sum;
    return sum*float4(1,1,1,1);
}

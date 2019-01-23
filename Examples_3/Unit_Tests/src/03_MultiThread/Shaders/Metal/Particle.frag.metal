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

struct VSInput
{
    uint seed [[attribute(0)]];
};

struct VSOutput {
    float4 Position [[position]];
    float PointSize [[point_size]];
    float TexCoord;
};

struct particleRootConstantBlock
{
    float paletteFactor;
    uint data;
    uint textureIndex;
};

struct ParticleTextureData
{
    array<texture1d<float,access::sample>,5> textures;
};

fragment float4 stageMain(VSOutput input                                           [[stage_in]],
                          sampler uSampler0                                        [[sampler(0)]],
                          constant ParticleTextureData& uTex0                      [[buffer(1)]],
                          constant particleRootConstantBlock& particleRootConstant [[buffer(2)]])
{
    float4 ca = uTex0.textures[particleRootConstant.textureIndex].sample(uSampler0, input.TexCoord);
    float4 cb = uTex0.textures[(particleRootConstant.textureIndex + 1) % 5].sample(uSampler0, input.TexCoord);

    return 0.05*mix(ca, cb, particleRootConstant.paletteFactor);


}

/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

#ifndef argument_buffers_h
#define argument_buffers_h

struct IndirectDrawCommand
{
    uint indexCount;
    uint instanceCount;
    uint startIndex;
    uint vertexOffset;
    uint startInstance;
    uint padding[3];
};

struct InstanceData
{
    float4x4 mvp;
    float4x4 normalMat;
    float4 surfaceColor;
    float4 deepColor;
    int textureID;
    uint _pad0[3];
};

struct UniformBlockData
{
    float4x4 viewProj;
};

struct AsteroidDynamic
{
	float4x4 transform;
    uint indexStart;
    uint indexEnd;
    uint padding[2];
};

struct AsteroidStatic
{
	float4 rotationAxis;
	float4 surfaceColor;
	float4 deepColor;

	float scale;
	float orbitSpeed;
	float rotationSpeed;

    uint textureID;
    uint vertexStart;
    uint padding[3];
};


#define ExecuteIndirectArgData \
    constant AsteroidStatic* asteroidsStatic   [[buffer(0)]],  \
    constant AsteroidDynamic* asteroidsDynamic [[buffer(1)]],  \
    texture2d_array<float> uTex0               [[texture(0)]], \
    sampler uSampler0                          [[sampler(0)]]

#define ExecuteIndirectArgDataPerFrame \
    constant UniformBlockData& uniformBlock    [[buffer(2)]]


#endif /* argument_buffers_h */

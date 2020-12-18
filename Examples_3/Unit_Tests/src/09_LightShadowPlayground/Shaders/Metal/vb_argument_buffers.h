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

#ifndef vb_argument_buffers_h
#define vb_argument_buffers_h

struct Uniforms_objectUniformBlock
{
	float4x4 mWorldViewProjMat;
	float4x4 mWorldMat;
};

struct ArgData
{
	sampler textureFilter                        [[id(0)]];
    sampler nearClampSampler                     [[id(1)]];
    array<texture2d<float>,256> diffuseMaps;
};

struct ArgDataPerFrame
{
    constant uint* indirectMaterialBuffer         [[id(0)]];
};

struct ArgDataPerDraw
{
    constant Uniforms_objectUniformBlock & objectUniformBlock   [[id(0)]];
};

#endif /* vb_argument_buffers_h */

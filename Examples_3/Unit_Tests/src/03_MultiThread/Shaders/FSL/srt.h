/*
 * Copyright (c) 2017-2025 The Forge Interactive Inc.
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

#pragma once

// limit push_constant decl to particle shader for metal

STRUCT(ParticleData)
{
    DATA(float, mPaletteFactor, None);
    DATA(uint, mData, None);
    DATA(uint, mTextureIndex, None);
    DATA(uint, mPad0, None);
};



// for low end iOS devices, do not use Argument buffers
BEGIN_SRT_NO_AB(SrtData)
    BEGIN_SRT_SET(Persistent)
        DECL_TEXTURE(Persistent, Tex2D(float4), gRightText)
        DECL_TEXTURE(Persistent, Tex2D(float4), gLeftText)
        DECL_TEXTURE(Persistent, Tex2D(float4), gTopText)
        DECL_TEXTURE(Persistent, Tex2D(float4), gBotText)
        DECL_TEXTURE(Persistent, Tex2D(float4), gFrontText)
        DECL_TEXTURE(Persistent, Tex2D(float4), gBackText)
        DECL_SAMPLER(Persistent, SamplerState, gSampler)
        DECL_ARRAY_TEXTURES(Persistent, Tex1D(float4), gTexture, 5)
END_SRT_SET(Persistent)
	BEGIN_SRT_SET(PerBatch)
		DECL_CBUFFER(PerBatch, CBUFFER(UniformBlock), gUniformBlock)
    END_SRT_SET(PerBatch)
	BEGIN_SRT_SET(PerDraw)
		DECL_CBUFFER(PerDraw, CBUFFER(ParticleData), gParticleConstants)
    END_SRT_SET(PerDraw)
END_SRT(SrtData)

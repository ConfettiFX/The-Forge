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

// for low end iOS devices, do not use Argument buffers
BEGIN_SRT_NO_AB(SrtComputeData)
    BEGIN_SRT_SET(Persistent)
        DECL_TEXTURE(Persistent, TexCube(float4), gSrcTexture)
    END_SRT_SET(Persistent)
    BEGIN_SRT_SET(PerFrame)
        DECL_CBUFFER(PerFrame, CBUFFER(SpecularConfig), gSpecularConfig)
    END_SRT_SET(PerFrame)
    BEGIN_SRT_SET(PerBatch)
        DECL_WTEXTURE(PerBatch, WTex2DArray(float4), gDestTextureSpecular)
        DECL_WTEXTURE(PerBatch, WTex2DArray(float4), gDestTextureIrradiance)
        DECL_WTEXTURE(PerBatch, WTex2D(float2), gDestTextureBRDF)
    END_SRT_SET(PerBatch)
END_SRT(SrtComputeData)



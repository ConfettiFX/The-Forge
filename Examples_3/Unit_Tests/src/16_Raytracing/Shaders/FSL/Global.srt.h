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

BEGIN_SRT(SrtData)
    BEGIN_SRT_SET(Persistent)
	    DECL_RAYTRACING(Persistent, RaytracingAccelerationStructure, gRtScene)
	    DECL_BUFFER(Persistent, ByteBuffer, gIndexDataBuffer)
        DECL_BUFFER(Persistent, ByteBuffer, gVertexPositionBuffer)
        DECL_BUFFER(Persistent, ByteBuffer, gVertexNormalBuffer)
        DECL_BUFFER(Persistent, ByteBuffer, gVertexTexCoordBuffer)
	    DECL_BUFFER(Persistent, ByteBuffer, gIndexOffsets)
        DECL_ARRAY_TEXTURES(Persistent, Tex2D(float4), gMaterialTextures, TOTAL_IMGS)
    END_SRT_SET(Persistent)
    BEGIN_SRT_SET(PerFrame)
        DECL_CBUFFER(PerFrame, CBUFFER(ShadersConfigBlock), gSettings)
        DECL_TEXTURE(PerFrame, Tex2D(float4), gDisplayTexture)
        DECL_TEXTURE(PerFrame, Tex2D(float4), gAlbedoTex)
    END_SRT_SET(PerFrame)
    BEGIN_SRT_SET(PerBatch)
        DECL_WTEXTURE(PerBatch, RTex2D(float4), gAlbedoInput)
#if UAV_RW_FALLBACK
        DECL_WTEXTURE(PerBatch, WTex2D(float4), gOutput)
        DECL_RWTEXTURE(PerBatch, RTex2D(float4), gInput)
        DECL_WTEXTURE(PerBatch, WTex2D(float4), gAlbedoOutput)
#else
        DECL_RWTEXTURE(PerBatch, RWTex2D(float4), gOutput)
        DECL_TEXTURE(PerBatch, Tex2D(float4), gInput)
        DECL_RWTEXTURE(PerBatch, RWTex2D(float4), gAlbedoOutput)
#endif
    END_SRT_SET(PerBatch)
END_SRT(SrtData)


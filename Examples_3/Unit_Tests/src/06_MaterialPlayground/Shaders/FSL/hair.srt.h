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

// for low end iOS devices, do not use Argument buffers
BEGIN_SRT_NO_AB(SrtHairData)
    #include "persistent_set.h"
    #include "perframe_set.h"
    BEGIN_SRT_SET(PerBatch)
        DECL_ARRAY_TEXTURES(PerBatch, Tex2D(float), gHairDirectionalLightShadowMaps, MAX_NUM_DIRECTIONAL_LIGHTS)
        DECL_CBUFFER(PerBatch, CBUFFER(DirectionalLightShadowCameras), gHairDirectionalLightShadowCameras)
    END_SRT_SET(PerBatch)
    BEGIN_SRT_SET(PerDraw)
        DECL_CBUFFER(PerDraw, CBUFFER(HairData), gHair)
        DECL_BUFFER(PerDraw, Buffer(float4), gGuideHairVertexPositions)
        DECL_BUFFER(PerDraw, Buffer(float4), gGuideHairVertexTangents)
        DECL_BUFFER(PerDraw, Buffer(float), gHairThicknessCoefficients)
        DECL_CBUFFER(PerDraw, CBUFFER(Simulation), gHairSimulation)
        DECL_BUFFER(PerDraw, Buffer(float), gHairRestLengths)
        DECL_BUFFER(PerDraw, Buffer(float4), gHairRestPositions)
        DECL_BUFFER(PerDraw, Buffer(float4), gFollowHairRootOffsets)
        DECL_BUFFER(PerDraw, Buffer(float4), gHairGlobalRotations)
        DECL_BUFFER(PerDraw, Buffer(float4), gHairRefsInLocalFrame)
        DECL_CBUFFER(PerDraw, CBUFFER(Camera), gHairShadowCamera)
        DECL_RWBUFFER(PerDraw, RWBuffer(float4), gHairVertexPositions)
        DECL_RWBUFFER(PerDraw, RWBuffer(float4), gHairVertexPositionsPrev)
        DECL_RWBUFFER(PerDraw, RWBuffer(float4), gHairVertexPositionsPrevPrev)
        DECL_RWBUFFER(PerDraw, RWBuffer(float4), gHairVertexTangents)
        DECL_RWBUFFER(PerDraw, RWBuffer(uint), gDepthsBuffer)
        DECL_RWTEXTURE(PerDraw, RWTex2DArray(uint), gDestDepthsTexture)
    END_SRT_SET(PerDraw)
END_SRT(SrtHairData)


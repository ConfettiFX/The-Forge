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

    BEGIN_SRT_SET(Persistent)
        DECL_CBUFFER(Persistent, CBUFFER(VBConstantBufferData), gVBConstantBuffer)
        DECL_CBUFFER(Persistent, CBUFFER(cbLightsData), gCBLights)
        DECL_CBUFFER(Persistent, CBUFFER(cbDLightsData), gCBDLights)
        DECL_BUFFER(Persistent, ByteBuffer, gVertexPositionBuffer)
        DECL_BUFFER(Persistent, ByteBuffer, gVertexTexCoordBuffer)
        DECL_BUFFER(Persistent, ByteBuffer, gVertexNormalBuffer)
        DECL_BUFFER(Persistent, Buffer(MeshConstants), gMeshConstantsBuffer)
        DECL_BUFFER(Persistent, ByteBuffer, gIndexDataBuffer)
        DECL_BUFFER(Persistent, Buffer(uint), gSobolBuffer)
        DECL_BUFFER(Persistent, Buffer(uint), gRankingTileBuffer)
        DECL_BUFFER(Persistent, Buffer(uint), gScramblingTileBuffer)
        DECL_SAMPLER(Persistent, SamplerState, gDefaultSampler)
        DECL_SAMPLER(Persistent, SamplerState, gEnvSampler)
        DECL_SAMPLER(Persistent, SamplerState, gSkyboxSampler)
        DECL_SAMPLER(Persistent, SamplerState, gDepthSampler)
        DECL_SAMPLER(Persistent, SamplerState, gBilinearSampler)
        DECL_TEXTURE(Persistent, Tex2D(float4), gVBPassTexture)
        DECL_TEXTURE(Persistent, Tex2D(float2), gBRDFIntegrationMap)
        DECL_TEXTURE(Persistent, TexCube(float4), gIrradianceMap)
        DECL_TEXTURE(Persistent, TexCube(float4), gSpecularMap)
        DECL_TEXTURE(Persistent, TexCube(float4), gEnvironmentMap)
        DECL_TEXTURE(Persistent, TexCube(float4), gSkyboxTex)
        DECL_TEXTURE(Persistent, Tex2D(float), gSourceDepth)
        DECL_TEXTURE(Persistent, Tex2D(float), gDepthBuffer)
        DECL_TEXTURE(Persistent, Tex2D(float4), gLitScene)
        DECL_TEXTURE(Persistent, Tex2D(float), gDepthBufferHierarchy)
        DECL_TEXTURE(Persistent, Tex2D(float4), gSceneTexture)
        DECL_TEXTURE(Persistent, Tex2D(float4), gDepthTexture)
        DECL_TEXTURE(Persistent, Tex2D(float4), gSSRTexture)
        DECL_ARRAY_TEXTURES(Persistent, Tex2D(float4), gDiffuseMaps, MAX_TEXTURE_UNITS)
        DECL_ARRAY_TEXTURES(Persistent, Tex2D(float4), gNormalMaps, MAX_TEXTURE_UNITS)
        DECL_ARRAY_TEXTURES(Persistent, Tex2D(float4), gSpecularMaps, MAX_TEXTURE_UNITS)
END_SRT_SET(Persistent)



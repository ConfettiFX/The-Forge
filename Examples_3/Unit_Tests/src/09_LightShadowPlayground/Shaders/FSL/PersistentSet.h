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
		DECL_CBUFFER(Persistent, CBUFFER(BlurWeightData), gBlurWeights)
        DECL_BUFFER(Persistent, ByteBuffer, gIndexDataBuffer)
        DECL_BUFFER(Persistent, ByteBuffer, gVertexPositionBuffer)
        DECL_BUFFER(Persistent, ByteBuffer, gVertexTexCoordBuffer)
        DECL_BUFFER(Persistent, ByteBuffer, gVertexNormalBuffer)
        DECL_BUFFER(Persistent, Buffer(MeshConstants), gMeshConstantsBuffer)
        DECL_SAMPLER(Persistent, SamplerComparisonState, gShadowCmpSampler)
        DECL_SAMPLER(Persistent, SamplerState, gClampMiplessLinearSampler)
        DECL_SAMPLER(Persistent, SamplerState, gClampToEdgeNearSampler)
        DECL_SAMPLER(Persistent, SamplerState, gClampBorderNearSampler)
        DECL_SAMPLER(Persistent, SamplerState, gTextureSampler)
		DECL_TEXTURE(Persistent, Depth2D(float), gDepthAtlasTexture)
		DECL_TEXTURE(Persistent, Tex2D(float), gDEMTexture)
		DECL_TEXTURE(Persistent, Tex2D(float), gPrerenderLodClampTexture)
        DECL_TEXTURE(Persistent, Tex2D(float), gESMShadowTexture)
        DECL_TEXTURE(Persistent, Tex2D(float4), gMSMShadowTexture)
#if SAMPLE_COUNT > 1
    #if FT_MULTIVIEW
            DECL_TEXTURE(Persistent, Depth2DArrayMS(float, SAMPLE_COUNT), gDepthTexture)
            DECL_TEXTURE(Persistent, Tex2DArrayMS(float, SAMPLE_COUNT), gDepthTextureUpSample)
            DECL_TEXTURE(Persistent, Tex2DArrayMS(float4, SAMPLE_COUNT), gVBPassTexture)
    #else
            DECL_TEXTURE(Persistent, Depth2DMS(float, SAMPLE_COUNT), gDepthTexture)
            DECL_TEXTURE(Persistent, Tex2DMS(float, SAMPLE_COUNT), gDepthTextureUpSample)
            DECL_TEXTURE(Persistent, Tex2DMS(float4, SAMPLE_COUNT), gVBPassTexture)
    #endif /*FT_MULTIVIEW*/
#else
    #if FT_MULTIVIEW
            DECL_TEXTURE(Persistent, Depth2DArray(float), gDepthTexture)
            DECL_TEXTURE(Persistent, Tex2DArray(float), gDepthTextureUpSample)
            DECL_TEXTURE(Persistent, Tex2DArray(float4), gVBPassTexture)
    #else
            DECL_TEXTURE(Persistent, Depth2D(float), gDepthTexture)
            DECL_TEXTURE(Persistent, Tex2D(float), gDepthTextureUpSample)
            DECL_TEXTURE(Persistent, Tex2D(float4), gVBPassTexture)
    #endif /*FT_MULTIVIEW*/
#endif

#if FT_MULTIVIEW
        DECL_TEXTURE(Persistent, Tex2DArrayMS(float4, SAMPLE_COUNT), gMsaaSource)
        DECL_TEXTURE(Persistent, Tex2DArray(float2), gSDFShadowTexture)
#else
        DECL_TEXTURE(Persistent, Tex2DMS(float4, SAMPLE_COUNT), gMsaaSource)
        DECL_TEXTURE(Persistent, Tex2D(float2), gSDFShadowTexture)
#endif
#ifdef USE_FLOAT4_VSM_RT
		DECL_TEXTURE(Persistent, Tex2D(float4), gVSMShadowTexture)
#else
		DECL_TEXTURE(Persistent, Tex2D(float2), gVSMShadowTexture)
#endif
#if TEXTURE_ATOMIC_SUPPORTED
    #if FT_MULTIVIEW
        DECL_TEXTURE(Persistent, Tex2DArray(uint), gScreenSpaceShadowTexture)
    #else
        DECL_TEXTURE(Persistent, Tex2D(uint), gScreenSpaceShadowTexture)
    #endif /*FT_MULTIVIEW*/
#else
		DECL_BUFFER(Persistent, Buffer(uint), gScreenSpaceShadowTexture)
#endif
		DECL_TEXTURE(Persistent, Tex3D(float), gSDFVolumeTextureAtlas)
		DECL_TEXTURE(Persistent, TexCube(float4), gSkyboxTex)
		DECL_ARRAY_TEXTURES(Persistent, Tex2D(float4), gIndexTextureArray, 10)
        DECL_ARRAY_TEXTURES(Persistent, Tex2D(float4), gDiffuseMaps, MAX_TEXTURE_UNITS)
        DECL_ARRAY_TEXTURES(Persistent, Tex2D(float4), gNormalMaps, MAX_TEXTURE_UNITS)
		DECL_ARRAY_TEXTURES(Persistent, Tex2D(float4), gSpecularMaps, MAX_TEXTURE_UNITS)
	END_SRT_SET(Persistent)

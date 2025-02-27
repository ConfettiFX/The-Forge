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


BEGIN_SRT_SET(PerFrame)
    DECL_CBUFFER(PerFrame, CBUFFER(PerFrameConstantsData), gPerFrameConstants)
    DECL_CBUFFER(PerFrame, CBUFFER(PerFrameConstantsData), gPerFrameConstantsComp)
    DECL_CBUFFER(PerFrame, CBUFFER(outputBufferOffsetsData), gOutputBufferOffsets)
    DECL_CBUFFER(PerFrame, CBUFFER(SkyboxUniformBufferData), gSkyboxUniformBuffer)
    DECL_BUFFER(PerFrame, Buffer(float4x4), gJointMatrixes)
	DECL_BUFFER(PerFrame, ByteBuffer, gFilteredIndexBuffer)
    DECL_BUFFER(PerFrame, Buffer(uint), gIndirectDataBuffer)
	DECL_BUFFER(PerFrame, Buffer(MeshData), gMeshDataBuffer)
	DECL_BUFFER(PerFrame, Buffer(uint), gLightClustersCount)
	DECL_BUFFER(PerFrame, Buffer(uint), gLightClusters)
    DECL_BUFFER(PerFrame, Buffer(FilterDispatchGroupData), gFilterDispatchGroupDataBuffer)
#if (SAMPLE_COUNT > 1)
    DECL_TEXTURE(PerFrame, Tex2DMS(uint, SAMPLE_COUNT), gPrevHistoryTex)
    DECL_TEXTURE(PerFrame, Tex2DMS(uint, SAMPLE_COUNT), gHistoryTex)
    DECL_TEXTURE(PerFrame, Tex2DMS(uint, SAMPLE_COUNT), gHistoryTexVBShade)
    DECL_TEXTURE(PerFrame, Tex2DMS(float4, SAMPLE_COUNT), gMsaaSource)
#else
    DECL_TEXTURE(PerFrame, Tex2D(uint), gPrevHistoryTex)
    DECL_TEXTURE(PerFrame, Tex2D(uint), gHistoryTex)
    DECL_TEXTURE(PerFrame, Tex2D(uint), gHistoryTexVBShade)
    DECL_TEXTURE(PerFrame, Tex2D(float4), gMsaaSource)
#endif
    DECL_TEXTURE(PerFrame, Tex2D(float4), gPrevFrameTex)
END_SRT_SET(PerFrame)



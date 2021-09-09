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

#ifndef cull_resources
#define cull_resources

// UPDATE_FREQ_NONE
RES(ByteBuffer, vertexDataBuffer, UPDATE_FREQ_NONE, t12, binding = 2);
RES(ByteBuffer, indexDataBuffer, UPDATE_FREQ_NONE, t13, binding = 3);
RES(Buffer(MeshConstants), meshConstantsBuffer, UPDATE_FREQ_NONE, t14, binding = 4);
RES(Buffer(uint), materialProps, UPDATE_FREQ_NONE, t11, binding = 1);

// UPDATE_FREQ_PER_FRAME
RES(RWBuffer(atomic_uint),              indirectDrawArgsBufferAlpha[NUM_CULLING_VIEWPORTS],   UPDATE_FREQ_PER_FRAME, u5, binding = 8);
RES(RWBuffer(atomic_uint),              indirectDrawArgsBufferNoAlpha[NUM_CULLING_VIEWPORTS], UPDATE_FREQ_PER_FRAME, u7, binding = 10);
RES(RWByteBuffer,                       filteredIndicesBuffer[NUM_CULLING_VIEWPORTS],         UPDATE_FREQ_PER_FRAME, u1, binding = 12);
RES(RWBuffer(UncompactedDrawArguments), uncompactedDrawArgsRW[NUM_CULLING_VIEWPORTS],         UPDATE_FREQ_PER_FRAME, u3, binding = 14);
RES(Buffer(UncompactedDrawArguments),   uncompactedDrawArgs[NUM_CULLING_VIEWPORTS],           UPDATE_FREQ_PER_FRAME, t0, binding = 16);
RES(RWByteBuffer,                       indirectMaterialBuffer,                               UPDATE_FREQ_PER_FRAME, u0, binding = 7);
CBUFFER(visibilityBufferConstants, UPDATE_FREQ_PER_FRAME, b13, binding = 6)
{
	DATA(float4x4, mWorldViewProjMat[NUM_CULLING_VIEWPORTS], None);
	DATA(CullingViewPort, mCullingViewports[NUM_CULLING_VIEWPORTS], None);
};

CBUFFER(batchData_rootcbv, UPDATE_FREQ_USER, b14, binding = 5)
{
	DATA(SmallBatchData, smallBatchDataBuffer[BATCH_COUNT], None);
};

#endif /* cull_resources */

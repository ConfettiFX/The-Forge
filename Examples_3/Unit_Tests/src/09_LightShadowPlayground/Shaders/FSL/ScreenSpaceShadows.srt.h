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

#ifndef screenspace_shadows_srt_h
#define screenspace_shadows_srt_h

BEGIN_SRT(ScreenSpaceShadowsSrtData)
#include "PersistentSet.h"
#include "PerFrameSet.h"
BEGIN_SRT_SET(PerBatch)
	DECL_CBUFFER(PerBatch, CBUFFER(SSSUniformData), gSSSUniform)
#if TEXTURE_ATOMIC_SUPPORTED
    DECL_RWTEXTURE(PerBatch, RWTex2D(uint), gOutputTexture)
#else
    DECL_RWBUFFER(PerBatch, RWBuffer(uint), gOutputTexture)
#endif
END_SRT_SET(PerBatch)
BEGIN_SRT_SET(PerDraw)
	DECL_CBUFFER(PerDraw, CBUFFER(ScreenSpaceShadowsUniforms), gSSSWaveOffsets)
END_SRT_SET(PerDraw)
END_SRT(ScreenSpaceShadowsSrtData)

#endif /* screenspace_shadows_srt_h */

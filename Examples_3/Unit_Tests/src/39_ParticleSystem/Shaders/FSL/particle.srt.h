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

#ifndef particle_srt_h
#define particle_srt_h

BEGIN_SRT(ParticleSrtData)
#include "PersistentSet.h"
#include "PerFrameSet.h"
	BEGIN_SRT_SET(PerBatch)
		DECL_RWBUFFER(PerBatch, RWBuffer(uint), gParticleSetVisibilityRW)
		DECL_RWBUFFER(PerBatch, RWBuffer(PackedParticleTransparencyNode), gTransparencyListRW)
        DECL_RWBUFFER(PerBatch, RWByteBuffer, gParticlesDataBufferRW)
        DECL_RWBUFFER(PerBatch, RWBuffer(uint), gParticlesToRasterizeRW)
        DECL_RWBUFFER(PerBatch, RWBuffer(uint), gParticleRenderIndirectDataRW)
        DECL_RWBUFFER(PerBatch, RWBuffer(uint), gBitfieldBufferRW)
        DECL_RWBUFFER(PerBatch, RWBuffer(ParticleSystemStats), gParticleStatsBufferRW)
        DECL_RWBUFFER(PerBatch, RWBuffer(uint), gTransparencyListHeadsRW)
	END_SRT_SET(PerBatch)
END_SRT(ParticleSrtData)

#endif /* particle_srt_h */


/**********************************************************************
Copyright (c) 2020 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

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

#ifndef sssr_srt_h
#define sssr_srt_h

BEGIN_SRT(SSSRSrtData)
#include "PersistentSet.h"
#include "PerFrameSet.h"
	BEGIN_SRT_SET(PerBatch)
		DECL_RWBUFFER(PerBatch, RWBuffer(uint), gTileList)
		DECL_RWBUFFER(PerBatch, RWBuffer(uint), gRayList)
		DECL_RWBUFFER(PerBatch, RWCoherentBuffer(uint), gTileCounter)
		DECL_RWBUFFER(PerBatch, RWCoherentBuffer(uint), gRayCounter)
		DECL_RWBUFFER(PerBatch, RWBuffer(uint), gIntersectArgs)
		DECL_RWBUFFER(PerBatch, RWBuffer(uint), gDenoiserArgs)
        DECL_RWTEXTURE(PerBatch, RWTex2D(float), gRayLengths)
        DECL_RWTEXTURE(PerBatch, RWTex2D(float), gHasRay)
        DECL_RWTEXTURE(PerBatch, RWTex2D(float), gTemporalVariance)
        DECL_RWTEXTURE(PerBatch, RWTex2D(float4), gTemporallyDenoisedReflections)
        DECL_RWTEXTURE(PerBatch, RWTex2D(float4), gIntersectionResult)
        DECL_RWTEXTURE(PerBatch, RWTex2D(float4), gSpatiallyDenoisedReflections)
        DECL_RWTEXTURE(PerBatch, RWTex2D(float4), gTemporallyDenoisedReflectionsHistory)
        DECL_RWTEXTURE(PerBatch, RWTex2D(float4), gDenoisedReflections)
	END_SRT_SET(PerBatch)
END_SRT(SSSRSrtData)

#endif /* sssr_srt_h */


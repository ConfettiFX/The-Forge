/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#if defined(NO_FSL_DEFINITIONS)
#define float4x4 mat4
#define packed_float3 float3
#endif

struct ShadersConfigBlock
{
	float4x4 mCameraToWorld;
	float4x4 mWorldToCamera;
	float4x4 mCameraToProjection;
	float4x4 mWorldToProjectionPrevious;
	float4x4 mWorldMatrix;
	float2 mRtInvSize;
	float2 mZ1PlaneSize;
	float mProjNear;
	float mProjFarMinusNear;
	float mRandomSeed;
	uint mFrameIndex;
	packed_float3 mLightDirection;
	uint mFramesSinceCameraMove;
	float2 mSubpixelJitter;
	uint mWidth;
	uint mHeight;
};

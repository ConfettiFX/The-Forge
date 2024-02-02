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
#include "../Application/Config.h"

#include <stdbool.h>

#include "ThirdParty/OpenSource/cpu_features/src/cpu_features_types.h"

typedef enum
{
    SIMD_SSE3,
    SIMD_SSE4_1,
    SIMD_SSE4_2,
    SIMD_AVX,
    SIMD_AVX2,
    SIMD_NEON
} SimdIntrinsic;

typedef struct
{
    char          mName[512];
    SimdIntrinsic mSimd;

    X86Features          mFeaturesX86;
    X86Microarchitecture mArchitectureX86;

    Aarch64Features mFeaturesAarch64;
} CpuInfo;

#if defined(ANDROID)
#include <jni.h>
bool initCpuInfo(CpuInfo* outCpuInfo, JNIEnv* pJavaEnv);
#else
bool initCpuInfo(CpuInfo* outCpuInfo);
#endif

FORGE_API CpuInfo* getCpuInfo(void);
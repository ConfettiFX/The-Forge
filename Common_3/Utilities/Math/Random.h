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

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "../../Utilities/ThirdParty/OpenSource/murmurhash3/MurmurHash3_32.h"

#define TF_RAND_MAX     0x7FFFFFFF
#define TF_INITIAL_SEED 0x9747b28c

inline int32_t getRandomInt(void)
{
    static uint32_t       seed = TF_INITIAL_SEED;
    static const uint32_t messageHash[] = {
        0x8C2100D0, 0xEC843F56, 0xDD467E25, 0xC22461F6, 0xA1368AB0, 0xBBDA7B12, 0xA175F888, 0x6BD9BDA2,
        0x999AC54C, 0x7C043DD3, 0xD502088F, 0x1B5B4D72, 0x94BB5742, 0x2CDA891E, 0x88613640, 0x31A50479
    };

    int32_t result;
    MurmurHash3_x86_32(&messageHash, TF_ARRAY_COUNT(messageHash) * sizeof(uint32_t), seed--, &result);
    return abs(result);
}

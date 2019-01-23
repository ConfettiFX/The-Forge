/*
 * Copyright (c) 2019 Confetti Interactive Inc.
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

#include "../../ThirdParty/OpenSource/EnkiTS/Atomics.h"

typedef volatile BASE_ALIGN(4) uint32_t tfrg_atomic32_t;
typedef volatile BASE_ALIGN(8) uint64_t tfrg_atomic64_t;

#define tfrg_memorybarrier_acquire() BASE_MEMORYBARRIER_ACQUIRE()
#define tfrg_memorybarrier_release() BASE_MEMORYBARRIER_RELEASE()

#define tfrg_atomic32_add_relaxed(pVar, val) enki::AtomicAdd((pVar), (val))

#define tfrg_atomic64_add_relaxed(pVar, val) enki::AtomicAdd((pVar), (val))

#define tfrg_atomic64_max_relaxed(pVar, val) enki::AtomicUpdateMax((pVar), (val))

#define tfrg_atomic32_load_relaxed(pVar) (*pVar)
#define tfrg_atomic32_store_relaxed(pVar, val) ((*pVar) = (val))

#define tfrg_atomic64_load_relaxed(pVar) (*pVar)
#define tfrg_atomic64_store_relaxed(pVar, val) ((*pVar) = (val))

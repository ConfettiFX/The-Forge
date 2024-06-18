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

#include "../../Application/Config.h"

typedef volatile ALIGNAS(4) uint32_t tfrg_atomic32_t;
typedef volatile ALIGNAS(8) uint64_t tfrg_atomic64_t;
typedef volatile ALIGNAS(PTR_SIZE) uintptr_t tfrg_atomicptr_t;

#if defined(_MSC_VER) && !defined(NX64)
#include <intrin.h>
#include <windows.h>

#define tfrg_memorybarrier_acquire()                     _ReadWriteBarrier()
#define tfrg_memorybarrier_release()                     _ReadWriteBarrier()

#define tfrg_atomic32_load_relaxed(pVar)                 (*(pVar))
#define tfrg_atomic32_store_relaxed(dst, val)            (uint32_t) InterlockedExchange((volatile long*)(dst), val)
#define tfrg_atomic32_add_relaxed(dst, val)              (uint32_t) InterlockedExchangeAdd((volatile long*)(dst), (val))
#define tfrg_atomic32_cas_relaxed(dst, cmp_val, new_val) (uint32_t) InterlockedCompareExchange((volatile long*)(dst), (new_val), (cmp_val))

#define tfrg_atomic64_load_relaxed(pVar)                 (*(pVar))
#define tfrg_atomic64_store_relaxed(dst, val)            (uint64_t) InterlockedExchange64((volatile LONG64*)(dst), val)
#define tfrg_atomic64_add_relaxed(dst, val)              (uint64_t) InterlockedExchangeAdd64((volatile LONG64*)(dst), (val))
#define tfrg_atomic64_cas_relaxed(dst, cmp_val, new_val) \
    (uint64_t) InterlockedCompareExchange64((volatile LONG64*)(dst), (new_val), (cmp_val))

#else
#define tfrg_memorybarrier_acquire()                     __asm__ __volatile__("" : : : "memory")
#define tfrg_memorybarrier_release()                     __asm__ __volatile__("" : : : "memory")

#define tfrg_atomic32_load_relaxed(pVar)                 (*(pVar))
#define tfrg_atomic32_store_relaxed(dst, val)            __sync_lock_test_and_set((volatile int32_t*)(dst), val)
#define tfrg_atomic32_add_relaxed(dst, val)              __sync_fetch_and_add((volatile int32_t*)(dst), (val))
#define tfrg_atomic32_cas_relaxed(dst, cmp_val, new_val) __sync_val_compare_and_swap((volatile int32_t*)(dst), (cmp_val), (new_val))

#define tfrg_atomic64_load_relaxed(pVar)                 (*(pVar))
#define tfrg_atomic64_store_relaxed(dst, val)            __sync_lock_test_and_set((volatile int64_t*)(dst), val)
#define tfrg_atomic64_add_relaxed(dst, val)              __sync_fetch_and_add((volatile int64_t*)(dst), (val))
#define tfrg_atomic64_cas_relaxed(dst, cmp_val, new_val) __sync_val_compare_and_swap((volatile int64_t*)(dst), (cmp_val), (new_val))

#endif

static inline uint32_t tfrg_atomic32_load_acquire(tfrg_atomic32_t* pVar)
{
    uint32_t value = tfrg_atomic32_load_relaxed(pVar);
    tfrg_memorybarrier_acquire();
    return value;
}

static inline uint32_t tfrg_atomic32_store_release(tfrg_atomic32_t* pVar, uint32_t val)
{
    tfrg_memorybarrier_release();
    return tfrg_atomic32_store_relaxed(pVar, val);
}

static inline uint32_t tfrg_atomic32_max_relaxed(tfrg_atomic32_t* dst, uint32_t val)
{
    uint32_t prev_val = val;
    do
    {
        prev_val = tfrg_atomic32_cas_relaxed(dst, prev_val, val);
    } while (prev_val < val);
    return prev_val;
}

static inline uint64_t tfrg_atomic64_load_acquire(tfrg_atomic64_t* pVar)
{
    uint64_t value = tfrg_atomic64_load_relaxed(pVar);
    tfrg_memorybarrier_acquire();
    return value;
}

static inline uint64_t tfrg_atomic64_store_release(tfrg_atomic64_t* pVar, uint64_t val)
{
    tfrg_memorybarrier_release();
    return tfrg_atomic64_store_relaxed(pVar, val);
}

static inline uint64_t tfrg_atomic64_max_relaxed(tfrg_atomic64_t* dst, uint64_t val)
{
    uint64_t prev_val = val;
    do
    {
        prev_val = tfrg_atomic64_cas_relaxed(dst, prev_val, val);
    } while (prev_val < val);
    return prev_val;
}

#if PTR_SIZE == 4
#define tfrg_atomicptr_load_relaxed  tfrg_atomic32_load_relaxed
#define tfrg_atomicptr_load_acquire  tfrg_atomic32_load_acquire
#define tfrg_atomicptr_store_relaxed tfrg_atomic32_store_relaxed
#define tfrg_atomicptr_store_release tfrg_atomic32_store_release
#define tfrg_atomicptr_add_relaxed   tfrg_atomic32_add_relaxed
#define tfrg_atomicptr_cas_relaxed   tfrg_atomic32_cas_relaxed
#define tfrg_atomicptr_max_relaxed   tfrg_atomic32_max_relaxed
#elif PTR_SIZE == 8
#define tfrg_atomicptr_load_relaxed  tfrg_atomic64_load_relaxed
#define tfrg_atomicptr_load_acquire  tfrg_atomic64_load_acquire
#define tfrg_atomicptr_store_relaxed tfrg_atomic64_store_relaxed
#define tfrg_atomicptr_store_release tfrg_atomic64_store_release
#define tfrg_atomicptr_add_relaxed   tfrg_atomic64_add_relaxed
#define tfrg_atomicptr_cas_relaxed   tfrg_atomic64_cas_relaxed
#define tfrg_atomicptr_max_relaxed   tfrg_atomic64_max_relaxed
#endif

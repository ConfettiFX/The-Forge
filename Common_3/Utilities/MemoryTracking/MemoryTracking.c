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
#include "../../Application/Config.h"

#include "wchar.h"

#if !defined(TARGET_IOS)
#include "../ThirdParty/OpenSource/rmem/inc/rmem.h"
#endif

#include <memory.h>
#include <stdlib.h>

#include "../ThirdParty/OpenSource/ModifiedSonyMath/vectormath_settings.hpp"

#define MEM_MAX(a, b)             ((a) > (b) ? (a) : (b))

#define ALIGN_TO(size, alignment) (((size) + (alignment)-1) & ~((alignment)-1))

// Taken from EASTL EA_PLATFORM_MIN_MALLOC_ALIGNMENT
#ifndef PLATFORM_MIN_MALLOC_ALIGNMENT
#if defined(__APPLE__)
#define PLATFORM_MIN_MALLOC_ALIGNMENT 16
#elif defined(__ANDROID__) && defined(ARCH_ARM_FAMILY)
#define PLATFORM_MIN_MALLOC_ALIGNMENT 8
#elif defined(NX64) && defined(ARCH_ARM_FAMILY)
#define PLATFORM_MIN_MALLOC_ALIGNMENT 8
#elif defined(__ANDROID__) && defined(ARCH_X86_FAMILY)
#define PLATFORM_MIN_MALLOC_ALIGNMENT 8
#else
#define PLATFORM_MIN_MALLOC_ALIGNMENT (PTR_SIZE * 2)
#endif
#endif

#define MIN_ALLOC_ALIGNMENT MEM_MAX(VECTORMATH_MIN_ALIGN, PLATFORM_MIN_MALLOC_ALIGNMENT)

#ifdef ENABLE_MTUNER
#define MTUNER_ALLOC(_handle, _ptr, _size, _overhead) rmemAlloc((_handle), (_ptr), (uint32_t)(_size), (uint32_t)(_overhead))
#define MTUNER_ALIGNED_ALLOC(_handle, _ptr, _size, _overhead, _align) \
    rmemAllocAligned((_handle), (_ptr), (uint32_t)(_size), (uint32_t)(_overhead), (uint32_t)(_align))
#define MTUNER_REALLOC(_handle, _ptr, _size, _overhead, _prevPtr) \
    rmemRealloc((_handle), (_ptr), (uint32_t)(_size), (uint32_t)(_overhead), (_prevPtr))
#define MTUNER_FREE(_handle, _ptr) rmemFree((_handle), (_ptr))
#else
#define MTUNER_ALLOC(_handle, _ptr, _size, _overhead)
#define MTUNER_ALIGNED_ALLOC(_handle, _ptr, _size, _overhead, _align)
#define MTUNER_REALLOC(_handle, _ptr, _size, _overhead, _prevPtr)
#define MTUNER_FREE(_handle, _ptr)
#endif

#if defined(ENABLE_MEMORY_TRACKING)

#define _CRT_SECURE_NO_WARNINGS 1

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomment" // Do not warn whenever a comment-start sequence ‘/*’ appears in a ‘/*’ comment, or whenever a
                                           // backslash-newline appears in a ‘//’ comment.
#pragma GCC diagnostic ignored "-Wformat-truncation" // Do not warn about calls to formatted input/output functions such as snprintf and
                                                     // vsnprintf that might result in output truncation.
#pragma GCC diagnostic ignored \
    "-Wstringop-truncation" // Do not warn for calls to bounded string manipulation functions such as strncat, strncpy, and stpncpy that may
                            // either truncate the copied string or leave the destination unchanged.
#endif

// Just include the cpp here so we don't have to add it to the all projects
#include "../ThirdParty/OpenSource/FluidStudios/MemoryManager/mmgr.c"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

void* tf_malloc_internal(size_t size, const char* f, int l, const char* sf)
{
    return tf_memalign_internal(MIN_ALLOC_ALIGNMENT, size, f, l, sf);
}

void* tf_calloc_internal(size_t count, size_t size, const char* f, int l, const char* sf)
{
    return tf_calloc_memalign_internal(count, MIN_ALLOC_ALIGNMENT, size, f, l, sf);
}

void* tf_memalign_internal(size_t align, size_t size, const char* f, int l, const char* sf)
{
    void* pMemAlign = mmgrAllocator(f, l, sf, m_alloc_malloc, align, size);

    // If using MTuner, report allocation to rmem.
    MTUNER_ALIGNED_ALLOC(0, pMemAlign, size, 0, align);

    // Return handle to allocated memory.
    return pMemAlign;
}

void* tf_calloc_memalign_internal(size_t count, size_t align, size_t size, const char* f, int l, const char* sf)
{
    size = ALIGN_TO(size, align);

    void* pMemAlign = mmgrAllocator(f, l, sf, m_alloc_calloc, align, size * count);

    // If using MTuner, report allocation to rmem.
    MTUNER_ALIGNED_ALLOC(0, pMemAlign, size, 0, align);

    // Return handle to allocated memory.
    return pMemAlign;
}

void* tf_realloc_internal(void* ptr, size_t size, const char* f, int l, const char* sf)
{
    void* pRealloc = mmgrReallocator(f, l, sf, m_alloc_realloc, size, ptr);

    // If using MTuner, report reallocation to rmem.
    MTUNER_REALLOC(0, pRealloc, size, 0, ptr);

    // Return handle to reallocated memory.
    return pRealloc;
}

void tf_free_internal(void* ptr, const char* f, int l, const char* sf)
{
    // If using MTuner, report free to rmem.
    MTUNER_FREE(0, ptr);

    mmgrDeallocator(f, l, sf, m_alloc_free, ptr);
}

#else // defined(ENABLE_MEMORY_TRACKING) || defined(ENABLE_MTUNER)

#include "stdbool.h"

bool initMemAlloc(const char* appName)
{
    UNREF_PARAM(appName);
    // No op but this is where you would initialize your memory allocator and bookkeeping data in a real world scenario
    return true;
}

void exitMemAlloc(void)
{
    // Return all allocated memory to the OS. Analyze memory usage, dump memory leaks, ...
}

void* tf_malloc(size_t size)
{
#ifdef _MSC_VER
    void* ptr = _aligned_malloc(size, MIN_ALLOC_ALIGNMENT);
    MTUNER_ALIGNED_ALLOC(0, ptr, size, 0, MIN_ALLOC_ALIGNMENT);
#else
    void* ptr = malloc(size);
    MTUNER_ALLOC(0, ptr, size, 0);
#endif

    return ptr;
}

void* tf_calloc(size_t count, size_t size)
{
#ifdef _MSC_VER
    size_t sz = count * size;
    void*  ptr = tf_malloc(sz);
    memset(ptr, 0, sz); //-V575
#else
    void* ptr = calloc(count, size);
    MTUNER_ALLOC(0, ptr, count * size, 0);
#endif

    return ptr;
}

void* tf_memalign(size_t alignment, size_t size)
{
#ifdef _MSC_VER
    void* ptr = _aligned_malloc(size, alignment);
#else
    void* ptr;
    alignment = alignment > sizeof(void*) ? alignment : sizeof(void*);
    if (posix_memalign(&ptr, alignment, size))
    {
        ptr = NULL;
    }
#endif

    MTUNER_ALIGNED_ALLOC(0, ptr, size, 0, alignment);

    return ptr;
}

void* tf_calloc_memalign(size_t count, size_t alignment, size_t size)
{
    size_t alignedArrayElementSize = ALIGN_TO(size, alignment);
    size_t totalBytes = count * alignedArrayElementSize;

    void* ptr = tf_memalign(alignment, totalBytes);

    memset(ptr, 0, totalBytes); //-V575
    return ptr;
}

void* tf_realloc(void* ptr, size_t size)
{
#ifdef _MSC_VER
    void* reallocPtr = _aligned_realloc(ptr, size, MIN_ALLOC_ALIGNMENT);
#else
    void* reallocPtr = realloc(ptr, size);
#endif

    MTUNER_REALLOC(0, reallocPtr, size, 0, ptr);

    return reallocPtr;
}

void tf_free(void* ptr)
{
    MTUNER_FREE(0, ptr);

#ifdef _MSC_VER
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

void* tf_malloc_internal(size_t size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    return tf_malloc(size);
}

void* tf_memalign_internal(size_t align, size_t size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    return tf_memalign(align, size);
}

void* tf_calloc_internal(size_t count, size_t size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    return tf_calloc(count, size);
}

void* tf_calloc_memalign_internal(size_t count, size_t align, size_t size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    return tf_calloc_memalign(count, align, size);
}

void* tf_realloc_internal(void* ptr, size_t size, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    return tf_realloc(ptr, size);
}

void tf_free_internal(void* ptr, const char* f, int l, const char* sf)
{
    UNREF_PARAM(f);
    UNREF_PARAM(l);
    UNREF_PARAM(sf);
    tf_free(ptr);
}

#endif // defined(ENABLE_MEMORY_TRACKING) || defined(ENABLE_MTUNER)

/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

//This file contains abstractions for compiler specific things
#pragma once

#include <stdint.h>

//For getting rid of unreferenced parameter warnings
#ifdef _MSC_VER    //If on Visual Studio
#define UNREF_PARAM(x) (x)
#elif defined(ORBIS) || defined(PROSPERO)
#define UNREF_PARAM(x) ((void)(x))
#elif defined(__APPLE__)
#define UNREF_PARAM(x) ((void)(x))
#else
//Add more compilers and platforms as we need them
#define UNREF_PARAM(x)
#endif

#if   INTPTR_MAX == 0x7FFFFFFFFFFFFFFFLL
# define PTR_SIZE 8
#elif INTPTR_MAX == 0x7FFFFFFF
# define PTR_SIZE 4
#else
#error unsupported platform
#endif

#ifdef _MSC_VER
    #define ALIGNAS(x) __declspec( align( x ) ) 
#else
    #define ALIGNAS(x)  __attribute__ ((aligned( x )))
#endif

#if __cplusplus >= 201103
#define DEFINE_ALIGNED(def, a) alignas(a) def
#else
#if defined(_WIN32)
#define DEFINE_ALIGNED(def, a) __declspec(align(a)) def
#elif defined(__APPLE__)
#define DEFINE_ALIGNED(def, a) def __attribute__((aligned(a)))
#else
//If we haven't specified the platform here, we fallback on the C++11 and C11 keyword for aligning
//Best case -> No platform specific align defined -> use this one that does the same thing
//Worst case -> No platform specific align defined -> this one also doesn't work and fails to compile -> add a platform specific one :)
#define DEFINE_ALIGNED(def, a) alignas(a) def
#endif
#endif

#ifdef __APPLE__
#define NOREFS __unsafe_unretained
#endif

#ifdef _WIN32
#define FORGE_CALLCONV __cdecl
#else
#define FORGE_CALLCONV
#endif

#ifdef __cplusplus
#define FORGE_CONSTEXPR constexpr
#else
#define FORGE_CONSTEXPR
#endif

#if defined(_MSC_VER) && !defined(NX64)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

// Generates a compile error if the expression evaluates to false
#define COMPILE_ASSERT(exp) static_assert((exp), #exp)

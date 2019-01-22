/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

//For getting rid of unreferenced parameter warnings
#ifdef _MSC_VER    //If on Visual Studio
#define UNREF_PARAM(x) (x)
#else
//Add more compilers and platforms as we need them
#define UNREF_PARAM(x)
#endif

#if __cplusplus >= 201103
#define DEFINE_ALIGNED(def, a) alignas(a) def
#else
#if defined(_WIN32)
#define DEFINE_ALIGNED(def, a) __declspec(align(a)) def
#elif defined(__OSX__)
#define DEFINE_ALIGNED(def, a) def __attribute__((aligned(a)))
#else
//If we haven't specified the platform here, we fallback on the C++11 and C11 keyword for aligning
//Best case -> No platform specific align defined -> use this one that does the same thing
//Worst case -> No platform specific align defined -> this one also doesn't work and fails to compile -> add a platform specific one :)
#define DEFINE_ALIGNED(def, a) alignas(a) def
#endif
#endif

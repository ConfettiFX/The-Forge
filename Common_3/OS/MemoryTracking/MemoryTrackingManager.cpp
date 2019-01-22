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

#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IMemoryManager.h"

#ifdef USE_MEMORY_TRACKING
#include "../../ThirdParty/OpenSource/FluidStudios/MemoryManager/nommgr.h"
#define malloc(sz) m_allocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_malloc, sz)
#define calloc(count, size) m_allocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_calloc, ((size) * (count)))
#define realloc(ptr, sz) m_reallocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_realloc, sz, ptr)
#define free(ptr) m_deallocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_free, ptr)

#undef conf_malloc
#undef conf_calloc
#undef conf_realloc
#undef conf_free

void* conf_malloc(size_t size) { return malloc(size); }

void* conf_calloc(size_t count, size_t size) { return calloc(count, size); }

void* conf_realloc(void* ptr, size_t size) { return realloc(ptr, size); }

void conf_free(void* ptr) { free(ptr); }

// Just include the cpp here so we don't have to add it to the all projects
#include "../../ThirdParty/OpenSource/FluidStudios/MemoryManager/mmgr.cpp"
#else
#undef malloc
#undef calloc
#undef realloc
#undef free
#include <cstdlib>

void* m_allocator(size_t size) { return malloc(size); }

void* m_allocator(size_t count, size_t size) { return calloc(count, size); }

void* m_reallocator(void* ptr, size_t size) { return realloc(ptr, size); }

void m_deallocator(void* ptr) { free(ptr); }

#undef conf_malloc
#undef conf_calloc
#undef conf_realloc
#undef conf_free

void* conf_malloc(size_t size) { return malloc(size); }

void* conf_calloc(size_t count, size_t size) { return calloc(count, size); }

void* conf_realloc(void* ptr, size_t size) { return realloc(ptr, size); }

void conf_free(void* ptr) { free(ptr); }
#endif
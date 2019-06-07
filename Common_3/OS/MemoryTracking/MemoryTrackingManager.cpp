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

#ifdef USE_MEMORY_TRACKING

// Just include the cpp here so we don't have to add it to the all projects
#include "../../ThirdParty/OpenSource/FluidStudios/MemoryManager/mmgr.cpp"

void* conf_malloc(size_t size) { return m_allocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_malloc, 0, size); }

void* conf_memalign(size_t alignment, size_t size) { return m_allocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_memalign, alignment, size); }

void* conf_calloc(size_t count, size_t size) { return m_allocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_calloc, 0, size * count); }

void* conf_realloc(void* ptr, size_t size) { return m_reallocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_realloc, size, ptr); }

void conf_free(void* ptr) { m_deallocator(__FILE__, __LINE__, __FUNCTION__, m_alloc_free, ptr); }

#else

#include <stdlib.h>

#ifdef _MSC_VER
#include <memory.h>
#include "../../ThirdParty/OpenSource/EASTL/EABase/eabase.h"
void* conf_malloc(size_t size) { return _aligned_malloc(size, EA_PLATFORM_MIN_MALLOC_ALIGNMENT); }

void* conf_calloc(size_t count, size_t size)
{
	size_t sz = count*size;
	void* ptr = conf_malloc(sz);
	memset(ptr, 0, sz);
	return ptr;
}

void* conf_memalign(size_t alignment, size_t size) { return _aligned_malloc(size, alignment); }

void* conf_realloc(void* ptr, size_t size) { return _aligned_realloc(ptr, size, EA_PLATFORM_MIN_MALLOC_ALIGNMENT); }

void conf_free(void* ptr) { _aligned_free(ptr); }
#else
void* conf_malloc(size_t size) { return malloc(size); }

void* conf_calloc(size_t count, size_t size) { return calloc(count, size); }

void* conf_memalign(size_t alignment, size_t size)
{
	void* result;
	if(posix_memalign(&result, alignment, size)) result = 0;
	return result;
}

void* conf_realloc(void* ptr, size_t size) { return realloc(ptr, size); }

void conf_free(void* ptr) { free(ptr); }
#endif

#endif

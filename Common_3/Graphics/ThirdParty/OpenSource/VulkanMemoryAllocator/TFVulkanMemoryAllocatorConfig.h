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

// This file contains hooks for VulkanMemoryAllocator to use TheForge's API
// These are implemented in a separate file so that it's easier to upgrade the VMA version.

// Make sure to include this file at the top of VulkanMemoryAllocator.h and delete the fallbacks 
// of VMA to ensure TheForge API is used. Look for code like the below one in VulkanMemoryAllocator.h 
// and remove it.
// 
// #ifndef VMA_ASSERT
//   ...
// #endif

#ifdef VMA_IMPLEMENTATION

#define IMEMORY_FROM_HEADER

#include "../../../../Utilities/Interfaces/IMemory.h"
#include "../../../../Utilities/Interfaces/ILog.h"
#include "../../../../Utilities/Interfaces/IThread.h"

#define VMA_ASSERT(expr) ASSERT(expr)

// VMA has this disabled by default, putting it here in case we want to enable it in the future.
// Comment from VMA: Assert that will be called very often, like inside data structures e.g. operator[]. Making it non-empty can make program slow.
#define VMA_HEAVY_ASSERT(expr) //ASSERT(expr)

#define VMA_SYSTEM_ALIGNED_MALLOC(size, alignment) tf_memalign_internal(alignment, size, __FILE__, __LINE__, __FUNCTION__)
#define VMA_SYSTEM_ALIGNED_FREE(ptr) tf_free_internal(ptr, __FILE__, __LINE__, __FUNCTION__)

// Log is very verbose, prints for each allocation
#ifdef ENABLE_VMA_LOG
	#define VMA_DEBUG_LOG(format, ...) LOGF(eDEBUG, "VMA: " format, __VA_ARGS__)
#else
	#define VMA_DEBUG_LOG(format, ...)
#endif

struct TFVmaMutex
{
	TFVmaMutex()
	{
		initMutex(&mMutex);
	}
	~TFVmaMutex()
	{
		exitMutex(&mMutex);
	}
	void Lock() { acquireMutex(&mMutex); }
	void Unlock() { releaseMutex(&mMutex); }
	bool TryLock() { return tryAcquireMutex(&mMutex); }

	Mutex mMutex;
};

#define VMA_MUTEX TFVmaMutex

struct TFVmaRWMutex
{
	TFVmaRWMutex()
	{
		initMutex(&mMutex);
	}
	~TFVmaRWMutex()
	{
		exitMutex(&mMutex);
	}

	// For now TF API doesn't support RW mutexes, we just use a normal mutex
	void LockRead() { acquireMutex(&mMutex); }
	void UnlockRead() { releaseMutex(&mMutex); }
	bool TryLockRead() { return tryAcquireMutex(&mMutex); }
	void LockWrite() { acquireMutex(&mMutex); }
	void UnlockWrite() { releaseMutex(&mMutex); }
	bool TryLockWrite() { return tryAcquireMutex(&mMutex); }
	
	Mutex mMutex;
};

#define VMA_RW_MUTEX TFVmaRWMutex

#endif // VMA_IMPLEMENTATION

/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

/*
 * This file is supposed to be included only from <PlatformName>Thread.c
*/

#include "../Interfaces/IThread.h"
#include "../Interfaces/ILog.h"
#include "Atomics.h"

#include <stdlib.h>
#include <assert.h>

static CallOnceGuard gKeyInitGuard = INIT_CALL_ONCE_GUARD;
static pthread_key_t gThreadIDKey;

static void destroyThreadIDKey() 
{
	pthread_key_delete(gThreadIDKey);
}

static void initThreadIDKey() 
{
	int result = pthread_key_create(&gThreadIDKey, NULL);
	ASSERT(result == 0);
	UNREF_PARAM(result);
	result = atexit(destroyThreadIDKey);
	ASSERT(result == 0);
}

static ThreadID getCurrentPthreadID() 
{
	static tfrg_atomic32_t counter = 1;
	callOnce(&gKeyInitGuard, initThreadIDKey);

	void* ptr = pthread_getspecific(gThreadIDKey);
	uintptr_t ptr_id = (uintptr_t)ptr;
	ASSERT(ptr_id < THREAD_ID_MAX);
	ThreadID id = (ThreadID)ptr_id;

	// thread id wasn't set
	if (id == 0) 
	{
		id = (ThreadID)tfrg_atomic32_add_relaxed(&counter, 1);
		ASSERT(id != 0 && "integer overflow");
		// we store plain integers instead of pointers to data
		ptr_id = (uintptr_t)id;
		ptr = (void*)ptr_id;
		int result = pthread_setspecific(gThreadIDKey, ptr);
		ASSERT(result == 0);
		UNREF_PARAM(result);
	}

	ASSERT(id != 0);
	return id;
}

#pragma once
/*
 * Copyright (c) 2019 The Forge Interactive Inc.
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

typedef void (*TaskFunc)(void* user, uintptr_t arg);

template <class T, void (T::*callback)(size_t)>
static void memberTaskFunc(void* userData, size_t arg)
{
	T* pThis = static_cast<T*>(userData);
	(pThis->*callback)(arg);
}

template <class T, void (T::*callback)()>
static void memberTaskFunc0(void* userData, size_t)
{
	T* pThis = static_cast<T*>(userData);
	(pThis->*callback)();
}

enum
{
	MAX_LOAD_THREADS = 16,
	MAX_SYSTEM_TASKS = 128
};

struct ThreadSystem;

void initThreadSystem(ThreadSystem** ppThreadSystem, uint32_t numRequestedThreads = MAX_LOAD_THREADS, int preferreCore = 0, bool migrateEnabled = true ,const char* threadName = "");

void shutdownThreadSystem(ThreadSystem* pThreadSystem);

void addThreadSystemRangeTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t count);
void addThreadSystemRangeTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t start, uintptr_t end);
void addThreadSystemTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t index = 0);

uint32_t getThreadSystemThreadCount(ThreadSystem* pThreadSystem);

bool assistThreadSystemTasks(ThreadSystem* pThreadSystem, uint32_t* pIds, size_t count);

bool assistThreadSystem(ThreadSystem* pThreadSystem);

bool isThreadSystemIdle(ThreadSystem* pThreadSystem);
void waitThreadSystemIdle(ThreadSystem* pThreadSystem);

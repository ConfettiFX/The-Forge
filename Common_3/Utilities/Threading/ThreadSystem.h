#pragma once
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

#ifdef __cplusplus
extern "C"
{
#else
#include <stdbool.h>
#endif

    // If task is executed outside of ThreadSystem, threadId is UINT64_MAX
    // e.g. when threadSystemAssist() is used
    typedef void (*TaskFunc)(void* user, uint64_t threadId);

    struct ThreadSystemInitDesc
    {
        // same as affinity mask from struct ThreadDesc, but for all threads in pool
        bool     setAffinityMask;
        uint64_t affinityMask[16];

        // limited to getNumCPUCores()
        //
        // If count is equal to 0, thread system is going to work in "dummy" mode.
        // Dummy mode is when tasks are executed by caller of 'threadSystemAddTasks'
        // Usefull to easily toggle singlethreaded mode by setting this value to 0
        // No memory is allocated for dummy mode.
        uint64_t threadCount;

        // Thread namings are "ThreadName 1", "ThreadName 2", ...
        // pointer must be valid until threadSystemExit
        const char* threadName;
    };

    struct ThreadSystemExitDesc
    {
        // skip scheduled tasks
        bool abandonTasks;
        // Avoid waiting for all tasks to be finished.
        // Threads are going to continue working on tasks untill they are done.
        bool detachThreads;
    };

    // It's up to the user to estimate the usefulness of provided information
    struct ThreadSystemInfo
    {
        // number of worker threads.
        //
        // Detailed: value after clamping ThreadSystemDesc::threadCount.
        uint64_t threadCount;
        // number of worker threads executed in total in current time.
        //
        // Detailed: if thread system is just started,
        //           some threads might be in initialization stage.
        uint64_t executedThreadCount;
        // number of worker threads executed still.
        //
        // Detailed: is some thread stops working, this number decrements.
        //
        // Note: in current implementation activeThreadCount equal to threadCount,
        //       because threads are running until threadSystemExit is called
        uint64_t activeThreadCount;

        // Copy of pointer from 'ThreadSystemInitDesc::threadName'
        const char* threadName;
    };

    typedef void* ThreadSystem;

    static const struct ThreadSystemInitDesc gThreadSystemInitDescDefault = {
        0,
        { 0 },
        UINT64_MAX,
        NULL,
    };

    static const struct ThreadSystemExitDesc gThreadSystemExitDescDefault = {
        false,
        false,
    };

    bool threadSystemInit(ThreadSystem* out, const struct ThreadSystemInitDesc* desc);
    void threadSystemExit(ThreadSystem* ts, const struct ThreadSystemExitDesc* desc);

    void threadSystemAddTasks(ThreadSystem ts, TaskFunc func, uint64_t count, uint64_t userSize, void* userArray);

#define threadSystemAddTaskGroup(ts, func, count, userArray) threadSystemAddTasks(ts, func, count, sizeof *userArray, userArray)

    // returns result of expression "task is executed"
    bool threadSystemAssist(ThreadSystem ts);

    // Use threadSystemWaitIdle for infinite timeout
    // returns result of expression "no tasks scheduled and no tasks executed"
    bool threadSystemWaitIdleTimeout(ThreadSystem ts, uint32_t msTimeout);

    void threadSystemGetInfo(ThreadSystem ts, struct ThreadSystemInfo* outInfo);

    static inline void threadSystemAddTask(ThreadSystem ts, TaskFunc func, void* user) { threadSystemAddTasks(ts, func, 1, 0, user); }

    static inline bool threadSystemIsIdle(ThreadSystem ts) { return threadSystemWaitIdleTimeout(ts, 0); }

    static inline void threadSystemWaitIdle(ThreadSystem ts) { threadSystemWaitIdleTimeout(ts, UINT32_MAX); }

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
class ThreadSystemClass
{
    ThreadSystem threadSystem;

public:
    ThreadSystemClass(): threadSystem() {}

    ThreadSystemClass(const ThreadSystemInitDesc* desc): threadSystem() { this->init(desc); }

    ~ThreadSystemClass() { this->exit(&gThreadSystemExitDescDefault); }

    bool init(const ThreadSystemInitDesc* desc)
    {
        if (threadSystem)
            this->exit(&gThreadSystemExitDescDefault);
        return threadSystemInit(&threadSystem, desc);
    }

    void exit(const struct ThreadSystemExitDesc* desc) { threadSystemExit(&threadSystem, desc); }

    void addTask(TaskFunc func, void* data) const { threadSystemAddTask(threadSystem, func, data); }

    template<typename T>
    void addTasks(TaskFunc func, uint64_t count, T* dataArray) const
    {
        threadSystemAddTaskGroup(threadSystem, func, count, dataArray);
    }

    bool assist() const { return threadSystemAssist(threadSystem); }

    void assistUntilDone() const
    {
        while (threadSystemAssist(threadSystem))
            ;
    }

    void waitIdle() const { threadSystemWaitIdle(threadSystem); }

    bool waitIdle(uint32_t msTimeout) const { return threadSystemWaitIdleTimeout(threadSystem, msTimeout); }

    bool isIdle() const { return threadSystemIsIdle(threadSystem); }
};
#endif

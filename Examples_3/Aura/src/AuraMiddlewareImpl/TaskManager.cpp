/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This is a part of Aura.
 *
 * This file(code) is licensed under a
 * Creative Commons Attribution-NonCommercial 4.0 International License
 *
 *   (https://creativecommons.org/licenses/by-nc/4.0/legalcode)
 *
 * Based on a work at https://github.com/ConfettiFX/The-Forge.
 * You may not use the material for commercial purposes.
 *
 */

#include "TaskManager.h"

#ifdef _WINDOWS
#include <strsafe.h>
#endif

#include <stdio.h>

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

using namespace MT;

//
//  INTERNAL
//  The SpinLock class implements a simple spinlock using InterlockedCompareExchange.
//  The lock is primarly used by the successor list for quick locking at the cost of
//  spinning in the cases of contention.
//
class SpinLock
{
public:
    SpinLock(): muLock(Atomic32<uint32_t>(0)) {}

    void Lock()
    {
        while (muLock.CompareAndSwap(0, 1) == 1)
            ;
    }

    void Unlock() { muLock.Store(0); }

private:
    Atomic32<uint32_t> muLock;
};

class GenericTask
{
public:
    MT_DECLARE_TASK(GenericTask, MT::StackRequirements::STANDARD, MT::TaskPriority::NORMAL, MT::Color::Blue);

    char*         mpszSetName;
    TASKSETFUNC   mpFunc;
    void*         mpvArg;
    TaskManager*  pTaskManager;
    uint32_t      muIdx;
    uint32_t      muSize;
    TASKSETHANDLE mhTaskSet;

    void Do(MT::FiberContext& ctx)
    {
        mpFunc(mpvArg, 0, muIdx, muSize);
        pTaskManager->CompleteTaskSet(mhTaskSet, ctx);
    }
};

class TaskSet
{
public:
    TaskSet(TaskManager* taskManager):
        pTaskManager(taskManager), mhTaskset(TASKSETHANDLE_INVALID), mbHasBeenWaitedOn(false), mpFunc(NULL), mpvArg(0), muSize(0)
    {
        mTaskGroup = TaskGroup::Default();
        mszSetName[0] = 0;
        memset(Successors, 0, sizeof(Successors));
    }

    void prepareTasks()
    {
        //  Iterate for each task in the set and spawn a GenericTask
        for (uint32_t uIdx = 0; uIdx < muSize; ++uIdx)
        {
            mTasks[uIdx].mpFunc = mpFunc;
            mTasks[uIdx].mpvArg = mpvArg;
            mTasks[uIdx].muSize = muSize;
            mTasks[uIdx].mhTaskSet = mhTaskset;
            mTasks[uIdx].muIdx = uIdx;
            mTasks[uIdx].pTaskManager = pTaskManager;
        }
    }

    void execute(TaskScheduler* pScheduler)
    {
        prepareTasks();
        pScheduler->RunAsync(mTaskGroup, mTasks, muSize);
    }

    void execute(MT::FiberContext* pContext)
    {
        prepareTasks();
        pContext->RunAsync(mTaskGroup, mTasks, muSize);
    }

    TaskSet*      Successors[MAX_SUCCESSORS];
    TaskManager*  pTaskManager;
    TASKSETHANDLE mhTaskset;
    bool          mbHasBeenWaitedOn;

    TASKSETFUNC mpFunc;
    void*       mpvArg;

    MT::Atomic32<uint32_t> muStartCount;
    MT::Atomic32<uint32_t> muCompletionCount;
    MT::Atomic32<uint32_t> muRefCount;

    uint32_t muSize;
    SpinLock mSuccessorsLock;

    char mszSetName[MAX_TASKSETNAMELENGTH];

    TaskGroup   mTaskGroup;
    GenericTask mTasks[100]; //-V730_NOINIT
};
///////////////////////////////////////////////////////////////////////////////
//
//  Implementation of TaskManager
//
///////////////////////////////////////////////////////////////////////////////
bool TaskManager::Init()
{
    memset(mSets, 0x0, sizeof(mSets));
    for (uint32_t i = 0; i < MAX_TASKSETS; ++i)
        mTaskGroups[i] = mScheduler.CreateGroup();

    return true;
}

void TaskManager::Shutdown()
{
    //
    //  Release any left-over tasksets
    for (uint32_t uSet = 0; uSet < MAX_TASKSETS; ++uSet)
    {
        if (mSets[uSet])
        {
            WaitForSet(uSet);
            tf_free(mSets[uSet]);
            mSets[uSet] = NULL;
        }
    }

    for (uint32_t i = 0; i < MAX_TASKSETS; ++i)
        mScheduler.ReleaseGroup(mTaskGroups[i]);
}

bool TaskManager::CreateTaskSet(uint32_t uGroup, TASKSETFUNC pFunc, void* pArg, uint32_t uTaskCount, TASKSETHANDLE* pInDepends,
                                uint32_t uInDepends, const char* szSetName, TASKSETHANDLE* pOutHandle)
{
    UNREF_PARAM(szSetName);
    UNREF_PARAM(szSetName);
    TASKSETHANDLE  hSet;
    TASKSETHANDLE  hSetParent = TASKSETHANDLE_INVALID;
    TASKSETHANDLE* pDepends = pInDepends;
    uint32_t       uDepends = uInDepends;
    bool           bResult = false;

    //  Validate incomming parameters
    if (0 == uTaskCount || NULL == pFunc)
    {
        return false;
    }

    //
    //  Tasksets are spawned when their parents complete.  If no parent for a
    //  taskset is specified we need to create a fake one.
    //
    if (0 == uDepends)
    {
        hSetParent = AllocateTaskSet();
        mSets[hSetParent]->muCompletionCount.StoreRelaxed(0);
        mSets[hSetParent]->muRefCount.StoreRelaxed(1);
        mSets[hSetParent]->mTaskGroup = mTaskGroups[uGroup];

        //  Implicit starting task never needs to be waited on for TBB since
        //  it is not a real tbb task.
        mSets[hSetParent]->mbHasBeenWaitedOn = true;

        uDepends = 1;
        pDepends = &hSetParent;
    }

    //
    //  Allocate and setup the internal taskset
    //
    hSet = AllocateTaskSet();

    mSets[hSet]->muStartCount.Store(uDepends);

    //  NOTE: one refcount is owned by the tasking system the other
    //  by the caller.
    mSets[hSet]->muRefCount.StoreRelaxed(2);

    mSets[hSet]->mpFunc = pFunc;
    mSets[hSet]->mpvArg = pArg;
    mSets[hSet]->muSize = uTaskCount;
    mSets[hSet]->muCompletionCount.StoreRelaxed(uTaskCount);
    mSets[hSet]->mhTaskset = hSet;
    mSets[hSet]->mTaskGroup = mTaskGroups[uGroup];

    //
    //  Iterate over the dependency list and setup the successor
    //  pointers in each parent to point to this taskset.
    //
    for (uint32_t uDepend = 0; uDepend < uDepends; ++uDepend)
    {
        TASKSETHANDLE hDependsOn = pDepends[uDepend];
        TaskSet*      pDependsOn = mSets[hDependsOn];
        long          lPrevCompletion;

        //
        //  A taskset with a new successor is consider incomplete even if it
        //  already has completed.  This mechanism allows us tasksets that are
        //  already done to appear active and capable of spawning successors.
        //
        lPrevCompletion = pDependsOn->muCompletionCount.AddFetch(1) - 1;

        if (0 == lPrevCompletion && hSetParent != hDependsOn)
        {
            //  The dependency taskset was already completed.  This means we have,
            //  or will soon, release the refcount for the tasking system.  Addref
            //  the taskset since the next Completion will release it.
            //  This does not apply to the system-created parent.
            //
            //  NOTE: There is no race conditon here since the caller must still
            //  hold a reference to the depenent taskset which was passed in.
            pDependsOn->muRefCount.IncFetch();
        }

        pDependsOn->mSuccessorsLock.Lock();

        uint32_t uSuccessor;
        for (uSuccessor = 0; uSuccessor < MAX_SUCCESSORS; ++uSuccessor)
        {
            if (NULL == pDependsOn->Successors[uSuccessor])
            {
                pDependsOn->Successors[uSuccessor] = mSets[hSet];
                break;
            }
        }

        //
        //  If the successor list is full we have a problem.  The app
        //  needs to give us more space by increasing MAX_SUCCESSORS
        //
        if (uSuccessor == MAX_SUCCESSORS)
        {
            printf("Too many successors for this task set.\nIncrease MAX_SUCCESSORS\n");
            pDependsOn->mSuccessorsLock.Unlock();
            goto Cleanup;
        }

        pDependsOn->mSuccessorsLock.Unlock();

        //
        //  Mark the set as completed for the successor adding operation.
        //
        CompleteTaskSet(hDependsOn);
    }

    //  Set output taskset handle
    *pOutHandle = hSet;

    bResult = true;

Cleanup:

    return bResult;
}

void TaskManager::ReleaseHandle(TASKSETHANDLE hSet)
{
    mSets[hSet]->muRefCount.DecFetch();
    //
    //  Release cannot destroy the object since TBB may still be
    //  referencing internal members. Defer destruction until
    //  we need to allocate a slot.
}

void TaskManager::ReleaseHandles(TASKSETHANDLE* phSet, uint32_t uSet)
{
    for (uint32_t uIdx = 0; uIdx < uSet; ++uIdx)
    {
        ReleaseHandle(phSet[uIdx]);
    }
}

void TaskManager::WaitForSet(TASKSETHANDLE hSet)
{
    //
    //  Yield the main thread to TBB to get our taskset done faster!
    //  NOTE: tasks can only be waited on once.  After that they will
    //  deadlock if waited on again.
    if (!mSets[hSet]->mbHasBeenWaitedOn)
    {
        mScheduler.WaitGroup(mSets[hSet]->mTaskGroup, (uint32_t)-1);
        mSets[hSet]->mbHasBeenWaitedOn = true;
    }
}

void TaskManager::WaitAll() { mScheduler.WaitAll((uint32_t)-1); }

TASKSETHANDLE
TaskManager::AllocateTaskSet()
{
    TaskSet* pSet = tf_placement_new<TaskSet>(tf_calloc(1, sizeof(TaskSet)), this);
    uint32_t uSet = muNextFreeSet;

    //
    //  Create a new task set and find a slot in the TaskManager to put it in.
    //
    //  NOTE: if we have too many tasks pending we will spin on the slot.  If
    //  spinning occures, see TaskManager.h and increase MAX_TASKSETS
    //

    //
    //  NOTE: Allocating tasksets is not thread-safe due to allocation of the
    //  slot for the task pointer.  This can be easily made threadsafe with
    //  an interlocked op on the muNextFreeSet variable and a spin on the slot.
    //  It will cost a small amount of performance.
    //
    while (NULL != mSets[uSet] && 0 != mSets[uSet]->muRefCount.LoadRelaxed())
    {
        uSet = (uSet + 1) % MAX_TASKSETS;
    }

    if (NULL != mSets[uSet])
    {
        //  We know the refcount is done, but TBB has an assert that requires
        //  a task be waited on before being deleted.
        WaitForSet(uSet);

        //
        //  Once TaskManager is done with a tbb object we need to forcibly destroy it.
        //  There are some refcount issues with tasks in tbb 3.0 which can be
        //  inconsistent if a task has never been waited for.  TaskManager knows the
        //  correct refcount.
        tf_free(mSets[uSet]);
        mSets[uSet] = NULL;
    }

    mSets[uSet] = pSet;
    muNextFreeSet = (uSet + 1) % MAX_TASKSETS;

    return (TASKSETHANDLE)uSet;
}

void TaskManager::CompleteTaskSet(TASKSETHANDLE hSet)
{
    TaskSet* pSet = mSets[hSet];

    uint32_t uCount = pSet->muCompletionCount.DecFetch();

    if (0 == uCount)
    {
        //
        //  The task set has completed.  We need to look at the successors
        //  and signal them that this dependency of theirs has completed.
        //
        pSet->mSuccessorsLock.Lock();

        for (uint32_t uSuccessor = 0; uSuccessor < MAX_SUCCESSORS; ++uSuccessor)
        {
            TaskSet* pSuccessor = pSet->Successors[uSuccessor];

            //
            //  A signaled successor must be removed from the Successors list
            //  before the mSuccessorsLock can be released.
            //
            pSet->Successors[uSuccessor] = NULL;

            if (NULL != pSuccessor)
            {
                uint32_t uStart;

                uStart = pSuccessor->muStartCount.DecFetch();

                //
                //  If the start count is 0 the successor has had all its
                //  dependencies satisified and can be scheduled.
                //
                if (0 == uStart)
                {
                    pSuccessor->execute(&mScheduler);
                }
            }
        }

        pSet->mSuccessorsLock.Unlock();

        ReleaseHandle(hSet);
    }
}

void TaskManager::CompleteTaskSet(TASKSETHANDLE hSet, MT::FiberContext& context)
{
    TaskSet* pSet = mSets[hSet];
    uint32_t uCount = pSet->muCompletionCount.DecFetch();

    if (0 == uCount)
    {
        //
        //  The task set has completed.  We need to look at the successors
        //  and signal them that this dependency of theirs has completed.
        //
        pSet->mSuccessorsLock.Lock();

        for (uint32_t uSuccessor = 0; uSuccessor < MAX_SUCCESSORS; ++uSuccessor)
        {
            TaskSet* pSuccessor = pSet->Successors[uSuccessor];

            //
            //  A signaled successor must be removed from the Successors list
            //  before the mSuccessorsLock can be released.
            //
            pSet->Successors[uSuccessor] = NULL;

            if (NULL != pSuccessor)
            {
                uint32_t uStart;

                uStart = pSuccessor->muStartCount.DecFetch();

                //
                //  If the start count is 0 the successor has had all its
                //  dependencies satisified and can be scheduled.
                //
                if (0 == uStart)
                {
                    pSuccessor->execute(&context);
                }
            }
        }

        pSet->mSuccessorsLock.Unlock();
        ReleaseHandle(hSet);
    }
}

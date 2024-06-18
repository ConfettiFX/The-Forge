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

#include "../../../../../Custom-Middleware/Aura/Interfaces/IAuraTaskManager.h"

#include "TaskManager.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

namespace aura
{
#ifdef ENABLE_CPU_PROPAGATION
class TaskManagerImpl: public ITaskManager
{
public:
    TaskManagerImpl(void);
    ~TaskManagerImpl(void) override;

    bool createTaskSet(uint32_t group, ITASKSETFUNC pFunc, void* pArg, uint32_t uTaskCount, ITASKSETHANDLE* pDepends, uint32_t nDepends,
                       const char* setName, ITASKSETHANDLE* pOutHandle) override;
    void releaseTask(ITASKSETHANDLE hTaskSet) override;
    void releaseTasks(ITASKSETHANDLE* pHTaskSets, uint32_t nSets) override;
    void waitForTaskSet(ITASKSETHANDLE hTaskSet) override;
    bool isTaskDone(ITASKSETHANDLE hTaskSet) override;
    void waitAll() override;

private:
#ifdef ENABLE_CPU_PROPAGATION
    class TaskManager* m_pTaskMgrTbb;
#endif
};

void initTaskManager(ITaskManager** ppTaskManager)
{
    TaskManagerImpl* pTaskManager = tf_new(TaskManagerImpl);
    *ppTaskManager = pTaskManager;
}

void removeTaskManager(ITaskManager* pTaskManager) { tf_delete(pTaskManager); }
/************************************************************************/
// Task Manager Implementation
/************************************************************************/
TaskManagerImpl::TaskManagerImpl(void)
{
#ifdef ENABLE_CPU_PROPAGATION
    m_pTaskMgrTbb = tf_new(TaskManager);
    m_pTaskMgrTbb->Init();
#endif
}

TaskManagerImpl::~TaskManagerImpl(void)
{
#ifdef ENABLE_CPU_PROPAGATION
    m_pTaskMgrTbb->Shutdown();
    tf_delete(m_pTaskMgrTbb);
#endif
}

bool TaskManagerImpl::createTaskSet(uint32_t group, ITASKSETFUNC pFunc, void* pArg, uint32_t uTaskCount, ITASKSETHANDLE* pDepends,
                                    uint32_t nDepends, const char* setName, ITASKSETHANDLE* pOutHandle)
{
#ifdef ENABLE_CPU_PROPAGATION
    return m_pTaskMgrTbb->CreateTaskSet(group, pFunc, pArg, uTaskCount, pDepends, nDepends, setName, pOutHandle);
#else
    return false;
#endif
}

void TaskManagerImpl::releaseTask(ITASKSETHANDLE hTaskSet)
{
#ifdef ENABLE_CPU_PROPAGATION
    m_pTaskMgrTbb->ReleaseHandle(hTaskSet);
#endif
}

void TaskManagerImpl::releaseTasks(ITASKSETHANDLE* pHTaskSets, uint32_t nSets)
{
#ifdef ENABLE_CPU_PROPAGATION
    m_pTaskMgrTbb->ReleaseHandles(pHTaskSets, nSets);
#endif
}

void TaskManagerImpl::waitForTaskSet(ITASKSETHANDLE hTaskSet)
{
#ifdef ENABLE_CPU_PROPAGATION
    m_pTaskMgrTbb->WaitForSet(hTaskSet);
#endif
}

bool TaskManagerImpl::isTaskDone(ITASKSETHANDLE hTaskSet)
{
    UNREF_PARAM(hTaskSet);
    return true;
}

void TaskManagerImpl::waitAll()
{
#ifdef ENABLE_CPU_PROPAGATION
    m_pTaskMgrTbb->WaitAll();
#endif
}
#else
void initTaskManager(ITaskManager** ppTaskManager) {}

void removeTaskManager(ITaskManager* pTaskManager) {}
#endif
} // namespace aura

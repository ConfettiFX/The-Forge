/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../../../../../Custom-Middleware/Aura/Config/AuraConfig.h"

#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/TaskScheduler/Scheduler/Include/MTScheduler.h"
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/TaskScheduler/Scheduler/Include/MTAtomic.h"

#ifdef PROFILEGPA
#include "../../../../Code/Pix/Pix.h"
#define ProfileBeginTask(name) pixBegin(name)
#define ProfileEndTask() pixEnd()
#define ProfileBeginFrame(name) pixBegin(name)
#define ProfileEndFrame() pixEnd()

#else

#define ProfileBeginTask(name)
#define ProfileEndTask()
#define ProfileBeginFrame(name)
#define ProfileEndFrame()
#endif

#include <stdint.h>

//  Callback type for tasks in the tasking TaskManager system
typedef void (*TASKSETFUNC)(void*, int, uint32_t, uint32_t);

//  Handle to a task set that can be used to express task set
//  dependecies and task set synchronization.
typedef uint32_t TASKSETHANDLE;

//  Value of a TASKSETHANDLE that indicates an invalid handle
#define TASKSETHANDLE_INVALID 0xFFFFFFFF

//
//  Variables to control the memory size and performance of the TaskManager
//  class.  See header comment for details.
//
#define MAX_SUCCESSORS 5
#define MAX_TASKSETS 255
#define MAX_TASKSETNAMELENGTH 512

class TaskSet;
class GenericTask;
class TbbContextId;

namespace MT {
class FiberContext;
}

/*! The TaskManager allows the user to schedule tasksets that run on top of
TBB.  All TaskManager functions are NOT threadsafe.  TaskManager is
designed to be called only from the main thread.  Multi-threading is
achieved by creating TaskSets that execte on threads created internally
by TBB.
*/
class TaskManager
{
	public:
	//  Init will setup the tasking system.  It must be called before
	//  any other functions on the TaskManager interface.
	bool Init();

	//  Shutdown will stop the tasking system. Any outstanding tasks will
	//  be terminated and the threads used by TBB will be released.  It is
	//  up to the application to wait on any outstanding tasks before it
	//  calls shutdown.
	void Shutdown();

	//  Creates a task set and provides a handle to allow the application
	//  CreateTaskSet can fail if, by adding this task to the successor lists
	//  of its dependecies the list exceeds MAX_SUCCESSORS.  To fix, increase
	//  MAX_SUCCESSORS.
	//
	//  NOTE: A tasket of size 1 is valid.  The most common case is to have
	//  tasksets of >> 1 so the default tasking primitive is a taskset rather
	//  than a task.
	bool CreateTaskSet(
		uint32_t group, TASKSETFUNC pFunc, /* Function pointer to the */                               /* Taskset callback function */
		void*          pArg,                                                                           /* App data pointer (can be NULL) */
		uint32_t       uTaskCount,                                                                     /* Number of tasks to create */
		TASKSETHANDLE* pDepends, /* Array of TASKSETHANDLEs that */ /* this taskset depends on. The */ /* taskset will not be scheduled */
		/* until all tasksets in this list */                                                          /* complete. */
		uint32_t       uDepends,                                                                       /* Count of the depends list */
		const char*    szSetName, /* [Optional] name of the taskset */                                 /* the name is used for profiling */
		TASKSETHANDLE* pOutHandle                                                                      /* [Out] Handle to the new taskset */
	);

	//  All TASKSETHANDLE must be released when no longer referenced.
	//  ReleaseHandle will release the Applications reference on the taskset.
	//  It should only be called once per handle returned from CreateTaskSet.
	void ReleaseHandle(TASKSETHANDLE hSet);    //  Taskset handle to release

	//  All TASKSETHANDLE must be released when no longer referenced.
	//  ReleaseHandles will release the Applications reference on the array
	//  of taskset handled specified.  It should only be called once per handle
	//  returned from CreateTaskSet.
	void ReleaseHandles(
		TASKSETHANDLE* phSet,    //  Taskset handle array to release
		uint32_t       uSet      //  count of taskset handle array
	);

	//  WaitForSet will yeild the main thread to the tasking system and return
	//  only when the taskset specified has completed execution.
	void WaitForSet(TASKSETHANDLE hSet);    // Taskset to wait for completion

	void WaitAll();

	private:
	friend class GenericTask;

	//  INTERNAL:
	//  Allocate a free slot in the mSets list
	TASKSETHANDLE
	AllocateTaskSet();

	//  INTERNAL:
	//  Called by the tasking system when a task in a set completes.
	void CompleteTaskSet(TASKSETHANDLE hSet);

	//  INTERNAL:
	//  Called by the tasking system when a task in a set completes.
	void CompleteTaskSet(TASKSETHANDLE hSet, MT::FiberContext& context);

	//  Array containing the tbb task parents.
	TaskSet* mSets[MAX_TASKSETS];

	//  Helper array index of next free task slot.
	uint32_t muNextFreeSet;

	//
	//  Global TBB task mananger instance
	//
	MT::TaskScheduler      mScheduler;
	MT::TaskGroup          mTaskGroups[MAX_TASKSETS];
	MT::Atomic32<uint32_t> gCurrentCount;
};

// The MIT License (MIT)
//
// 	Copyright (c) 2015 Sergey Makeev, Vadim Slyusarev
//
// 	Permission is hereby granted, free of charge, to any person obtaining a copy
// 	of this software and associated documentation files (the "Software"), to deal
// 	in the Software without restriction, including without limitation the rights
// 	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// 	copies of the Software, and to permit persons to whom the Software is
// 	furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
// 	all copies or substantial portions of the Software.
//
// 	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// 	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// 	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// 	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// 	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// 	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// 	THE SOFTWARE.

#pragma once

#ifndef __MT_FIBER__
#define __MT_FIBER__

//#define MT_USE_BOOST_CONTEXT (1)



#if MT_USE_BOOST_CONTEXT

#include <fcontext.h>

//TODO

#else


#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif


#ifndef ORBIS
	#include <ucontext.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
    #define MAP_ANONYMOUS MAP_ANON
#endif

#ifndef MAP_STACK
    #define MAP_STACK (0)
#endif

#include "MTAppInterop.h"
#include "MTAtomic.h"

#endif

#ifdef NX64

#include <nn/os/os_FiberApi.h>
#include <nn/os/os_FiberTypes.h>
#include <nn/os/os_FiberCommon.h>

namespace MT
{
	class Fiber
	{
		void* funcData;
		TThreadEntryPoint func;

		Memory::StackDesc stackDesc;

		nn::os::FiberType fiberContext;
		bool isInitialized;

		//static void FiberFuncInternal(void* pFiber)
		//{
		//	MT_ASSERT(pFiber != nullptr, "Invalid fiber");
		//	Fiber* self = (Fiber*)pFiber;

		//	MT_ASSERT(self->isInitialized == true, "Using non initialized fiber");

		//	MT_ASSERT(self->func != nullptr, "Invalid fiber func");
		//	self->func(self->funcData);
		//}

		void CleanUp()
		{
			if (isInitialized)
			{
				// if func != null than we have stack memory ownership
				if (func != nullptr)
				{
					Memory::FreeStack(stackDesc);
				}

				isInitialized = false;
			}
		}

	public:

		MT_NOCOPYABLE(Fiber);

		Fiber()
			: funcData(nullptr)
			, func(nullptr)
			, isInitialized(false)
		{
			memset(&fiberContext, 0, sizeof(nn::os::FiberType));
		}

		~Fiber()
		{
			CleanUp();
		}


		void CreateFromCurrentThreadAndRun(TThreadEntryPoint entryPoint, void *userData)
		{
			//nn::os::FiberType fiber;
			//nn::os::InitializeFiber()

			MT_ASSERT(!isInitialized, "Already initialized");

			func = nullptr;
			funcData = nullptr;

			// get execution context
			//int res = getcontext(&fiberContext);
			//MT_USED_IN_ASSERT(res);
			//MT_ASSERT(res == 0, "getcontext - failed");

			isInitialized = true;

			entryPoint(userData);


			CleanUp();
		}


		void Create(size_t stackSize, TThreadEntryPoint entryPoint, void *userData)
		{
			MT_ASSERT(!isInitialized, "Already initialized");
			MT_ASSERT(stackSize >= PTHREAD_STACK_MIN, "Stack to small");

			func = entryPoint;
			funcData = userData;

			//int res = getcontext(&fiberContext);
			//MT_USED_IN_ASSERT(res);
			//MT_ASSERT(res == 0, "getcontext - failed");

			stackDesc = Memory::AllocStack(stackSize);

			//fiberContext.uc_link = nullptr;
			//fiberContext.uc_stack.ss_sp = stackDesc.stackBottom;
			//fiberContext.uc_stack.ss_size = stackDesc.GetStackSize();
			//fiberContext.uc_stack.ss_flags = 0;

			nn::os::InitializeFiber(&fiberContext, (nn::os::FiberFunction)entryPoint, userData, stackDesc.stackBottom, stackDesc.GetStackSize(), 0);

			//makecontext(&fiberContext, (void(*)())&FiberFuncInternal, 1, (void *)this);

			isInitialized = true;
		}

#ifdef MT_INSTRUMENTED_BUILD
		void SetName(const char* fiberName)
		{
			MT_UNUSED(fiberName);
		}
#endif

		static void SwitchTo(Fiber & from, Fiber & to)
		{
			HardwareFullMemoryBarrier();

			MT_ASSERT(from.isInitialized, "Invalid from fiber");
			MT_ASSERT(to.isInitialized, "Invalid to fiber");

			nn::os::SwitchToFiber(&to.fiberContext);

			//int res = swapcontext(&from.fiberContext, &to.fiberContext);
			//MT_USED_IN_ASSERT(res);
			//MT_ASSERT(res == 0, "setcontext - failed");

		}



	};


}

#elif ORBIS

#include <sce_fiber.h>

namespace MT
{
	class Fiber
	{
		void* funcData;
		TThreadEntryPoint func;

		Memory::StackDesc stackDesc;

		SceFiber fiberContext;

		//ucontext_t fiberContext;
		bool isInitialized;

		static void FiberFuncInternal(void* pFiber)
		{
			MT_ASSERT(pFiber != nullptr, "Invalid fiber");
			Fiber* self = (Fiber*)pFiber;

			MT_ASSERT(self->isInitialized == true, "Using non initialized fiber");

			MT_ASSERT(self->func != nullptr, "Invalid fiber func");
			self->func(self->funcData);
			sceFiberReturnToThread((uint64_t)self->funcData, nullptr);
		}

		void CleanUp()
		{
			if (isInitialized)
			{
				// if func != null than we have stack memory ownership
				if (func != nullptr)
				{
					Memory::FreeStack(stackDesc);
				}

				isInitialized = false;
			}
		}

	public:

		MT_NOCOPYABLE(Fiber);

		Fiber()
			: funcData(nullptr)
			, func(nullptr)
			, isInitialized(false)
		{
			memset(&fiberContext, 0, sizeof(decltype(fiberContext)));
		}

		~Fiber()
		{
			CleanUp();
		}


		void CreateFromCurrentThreadAndRun(TThreadEntryPoint entryPoint, void *userData)
		{
			MT_ASSERT(!isInitialized, "Already initialized");

			this->func = entryPoint;
			this->funcData = userData;

			isInitialized = true;
			const uint32 MT_STANDART_FIBER_STACK_SIZE = 32768; //32Kb
			stackDesc = Memory::AllocStack(MT_STANDART_FIBER_STACK_SIZE);
			sceFiberInitialize(&fiberContext, "", (SceFiberEntry)FiberFuncInternal, (uint64_t)this, stackDesc.stackBottom, stackDesc.GetStackSize(), nullptr);
			sceFiberRun(&fiberContext, (uint64_t)userData, nullptr);

			CleanUp();
		}

		void Create(size_t stackSize, TThreadEntryPoint entryPoint, void *userData)
		{

			MT_ASSERT(!isInitialized, "Already initialized");
			MT_ASSERT(stackSize >= PTHREAD_STACK_MIN, "Stack to small");

			func = entryPoint;
			funcData = userData;

			stackDesc = Memory::AllocStack(stackSize);

			int32_t res = sceFiberInitialize(&fiberContext, "", (SceFiberEntry)FiberFuncInternal, (uint64_t)this, stackDesc.stackBottom, stackDesc.GetStackSize(), nullptr);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "sceFiberInitialize - failed");
			isInitialized = true;
		}

#ifdef MT_INSTRUMENTED_BUILD
		void SetName(const char* fiberName)
		{
			MT_UNUSED(fiberName);
		}
#endif

		static void SwitchTo(Fiber & from, Fiber & to)
		{
			HardwareFullMemoryBarrier();

			MT_ASSERT(from.isInitialized, "Invalid from fiber");
			MT_ASSERT(to.isInitialized, "Invalid to fiber");

			int32_t res = sceFiberSwitch(&to.fiberContext, (uint64_t)to.funcData, (uint64_t*)&from.funcData);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "sceFiberSwitch - failed");
		}
	};
}

#else

#define IMEMORY_FROM_HEADER
#include "../../../../../../../Interfaces/IMemory.h"
#include "../../../../../../../Interfaces/ILog.h"
#define MCO_ALLOC(size)         tf_calloc(1, size)
#define MCO_DEALLOC(ptr, size)  tf_free(ptr)
#define MCO_ASSERT(ex)          ASSERT(ex)
#define MCO_LOG(msg)            LOGF(eERROR, msg)
#define MCO_MAIN_NO_JUMPOUT
#define MCO_DEFAULT_STORAGE_SIZE 0
// Sanitizers use a lot of extra stack space, so we handle that here
#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer))
#define MCO_DEFAULT_STACK_SIZE (16*56*1024)
#else
#define MCO_DEFAULT_STACK_SIZE (1*56*1024)
#endif
#include "../../../../ThirdParty/minicoro/minicoro.h"

// This is required to prevent warnings (and therefore errors with -Werror) on some platforms
#define MAYBE_USED(x) ((void)x)

extern void mco_switch_to(mco_coro* from, mco_coro* to);
extern void mco_finish_switch_to(mco_coro* to);

namespace MT
{
    class Fiber
    {
        mco_coro* coro;
        TThreadEntryPoint func;
        void *userdata;
        
    public:

        MT_NOCOPYABLE(Fiber);

        Fiber() : coro(nullptr), func(nullptr), userdata(nullptr)
        {
        }

        ~Fiber()
        {
            CleanUp();
        }
        
    #ifdef MT_INSTRUMENTED_BUILD
        void SetName(const char* fiberName)
        {
            MT_UNUSED(fiberName);
        }
    #endif

        void CreateFromCurrentThreadAndRun(TThreadEntryPoint entryPoint, void *userData)
        {
            Create(0, entryPoint, userData);
            mco_switch_to(NULL, coro);
            CleanUp();
        }

        void Create(size_t stackSize, TThreadEntryPoint entryPoint, void *userData)
        {
            mco_desc desc = mco_desc_init(CoroFunc, stackSize);
            desc.user_data = (void*)this;
            func = entryPoint;
            userdata = userData;
            const mco_result r = mco_create(&coro, &desc);
            ASSERT(r == MCO_SUCCESS && "Failed to create coro");
            MAYBE_USED(r);
        }

        static void SwitchTo(Fiber & from, Fiber & to)
		{
			mco_switch_to(from.coro, to.coro);
		}
        
    private:
        void CleanUp()
        {
            if (coro)
            {
                coro->state = MCO_DEAD;
                const mco_result r = mco_destroy(coro);
                ASSERT(r == MCO_SUCCESS && "Failed to create coro");
                MAYBE_USED(r);
                coro = nullptr;
            }
        }

        static void CoroFunc(mco_coro* co)
        {
            Fiber* fiber = (Fiber*)co->user_data;
            fiber->func(fiber->userdata);
            mco_finish_switch_to(co);
        }
    };
}

// NOTE: MTFiber.h is included in multiple places, but `MINICORO_IMPL` is only used once in 
// `Common_3/Utilities/ThirdParty/OpenSource/TaskScheduler/Scheduler/Source/MTFiberContext.cpp`,
// so we can use `#ifdef MINICORO_IMPL` to prevent this function from being defined multiple times
// Since `_mco_context` and `_mco_ctxbuf` are only available when `MINICORO_IMPL` is defined, we need
// to define it anyway in order to implement this function (which is required to support the `Fiber` API).
#ifdef MINICORO_IMPL
void mco_switch_to(mco_coro* from, mco_coro* to)
{
    mco_coro* cur = mco_running();
    mco_coro* fromCoro = from;
    _mco_context* toCtx = (_mco_context*)to->context;
    _mco_context* fromCtx = fromCoro ? (_mco_context*)fromCoro->context : NULL;
    _mco_ctxbuf* fromCtxBuf = fromCtx ? &fromCtx->ctx : &toCtx->back_ctx;
    mco_current_co = to;
    
#ifdef _MCO_USE_ASAN
    const void* bottom_old = NULL;
    size_t size_old = 0;
    if (cur){
        __sanitizer_finish_switch_fiber(cur->asan_prev_stack, (const void**)&bottom_old, &size_old);
        cur->asan_prev_stack = NULL;
    }
    __sanitizer_start_switch_fiber(&to->asan_prev_stack, to->stack_base, to->stack_size);
#endif
    
#ifdef _MCO_USE_TSAN
    to->tsan_prev_fiber = __tsan_get_current_fiber();
    __tsan_switch_to_fiber(to->tsan_fiber, 0);
#endif
    
    _mco_switch(fromCtxBuf, &toCtx->ctx);
    mco_current_co = cur;
    
#ifdef _MCO_USE_ASAN
    __sanitizer_finish_switch_fiber(to->asan_prev_stack, (const void**)&bottom_old, &size_old);
    if (cur) {
        __sanitizer_start_switch_fiber(&cur->asan_prev_stack, cur->stack_base, cur->stack_size);
    }
#endif
}

void mco_finish_switch_to(mco_coro* to)
{
    _mco_context* context = (_mco_context*)to->context;
    _mco_switch(&context->ctx, &context->back_ctx);
}
#endif

#endif
#endif

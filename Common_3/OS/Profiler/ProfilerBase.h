#pragma once

// This is free and unencumbered software released into the public domain.
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
// For more information, please refer to <http://unlicense.org/>
//
// ***********************************************************************
//
//
//
//
// Howto:
// Call these functions from your code:
//  ProfileOnThreadCreate
//  ProfileMouseButton
//  ProfileMousePosition 
//  ProfileModKey
//  ProfileFlip  				<-- Call this once per frame
//  ProfileDraw  				<-- Call this once per frame
//  ProfileToggleDisplayMode 	<-- Bind to a key to toggle profiling
//  ProfileTogglePause			<-- Bind to a key to toggle pause
//
// Use these macros in your code in blocks you want to time:
//
// 	PROFILE_DECLARE
// 	PROFILE_DEFINE
// 	PROFILE_DECLARE_GPU
// 	PROFILE_DEFINE_GPU
// 	PROFILE_SCOPE
// 	PROFILE_SCOPEI
//  PROFILE_META
//  PROFILE_LABEL
//  PROFILE_LABELF
//
//	Usage:
//
//	{
//		PROFILE_SCOPEI("GroupName", "TimerName", nColorRgb):
// 		..Code to be timed..
//  }
//
//	PROFILE_DECLARE / PROFILE_DEFINE allows defining groups in a shared place, to ensure sorting of the timers
//
//  (in global scope)
//  PROFILE_DEFINE(g_ProfileFisk, "Fisk", "Skalle", nSomeColorRgb);
//
//  (in some other file)
//  PROFILE_DECLARE(g_ProfileFisk);
//
//  void foo(){
//  	PROFILE_SCOPE(g_ProfileFisk);
//  }
//
// CONFFX: We do not support profile when dealing with multiple runtime graphic apis
// CONFFX: We do not support xbox yet

#include "../Interfaces/IProfiler.h"

#if 0 == PROFILE_ENABLED

#define PROFILE_DECLARE(var)
#define PROFILE_DEFINE(var, group, name, color)
#define PROFILE_DECLARE_GPU(var)
#define PROFILE_DEFINE_GPU(var, name, color)
#define PROFILE_SCOPE(var) do{}while(0)
#define PROFILE_SCOPE_TOKEN(token) do{}while(0)
#define PROFILE_SCOPEI(group, name, color) do{}while(0)
#define PROFILE_SCOPEGPU(var) do{}while(0)
#define PROFILE_SCOPEGPUI(name, color) do{}while(0)
#define PROFILE_META_CPU(name, count)
#define PROFILE_META_GPU(name, count)
#define PROFILE_LABEL(group, name) do{}while(0)
#define PROFILE_LABELF(group, name, ...) do{}while(0)
#define PROFILE_COUNTER_ADD(name, count) do{} while(0)
#define PROFILE_COUNTER_SUB(name, count) do{} while(0)
#define PROFILE_COUNTER_SET(name, count) do{} while(0)
#define PROFILE_COUNTER_SET_LIMIT(name, count) do{} while(0)
#define PROFILE_COUNTER_CONFIG(name, type, limit, flags) do{} while(0)

#define PROFILE_FORCEENABLECPUGROUP(s) do{} while(0)
#define PROFILE_FORCEDISABLECPUGROUP(s) do{} while(0)
#define PROFILE_FORCEENABLEGPUGROUP(s) do{} while(0)
#define PROFILE_FORCEDISABLEGPUGROUP(s) do{} while(0)

#define ProfileGetTime(group, name) 0.f
#define ProfileOnThreadCreate(foo) do{}while(0)
#define ProfileFlip() do{}while(0)
#define ProfileGetAggregateFrames() 0
#define ProfileGetCurrentAggregateFrames() 0
#define ProfileTogglePause() do{}while(0)
#define ProfileShutdown() do{}while(0)
#define ProfileSetForceEnable(a) do{} while(0)
#define ProfileGetForceEnable() false
#define ProfileSetEnableAllGroups(a) do{} while(0)
#define ProfileEnableCategory(a) do{} while(0)
#define ProfileDisableCategory(a) do{} while(0)
#define ProfileGetEnableAllGroups() false
#define ProfileSetForceMetaCounters(a)
#define ProfileGetForceMetaCounters() 0
#define ProfileEnableMetaCounter(c) do{} while(0)
#define ProfileDisableMetaCounter(c) do{} while(0)
#define ProfileContextSwitchTraceStart() do{} while(0)
#define ProfileContextSwitchTraceStop() do{} while(0)
#define ProfileDumpFile(path,type,frames) do{} while(0)
#define ProfileDumpHtml(cb,handle,frames,host) do{} while(0)
#define ProfileWebServerStart() do{} while(0)
#define ProfileWebServerStop() do{} while(0)
#define ProfileWebServerPort() 0

#define ProfileGpuSetContext(c) do{} while(0)

#else

#include "../Interfaces/IThread.h"
#include "../Core/Atomics.h"
#ifndef PROFILE_API
#define PROFILE_API
#endif

PROFILE_API int64_t ProfileTicksPerSecondCpu();


#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <unistd.h>
#include <libkern/OSAtomic.h>
#include <TargetConditionals.h>

#define P_TICK() mach_absolute_time()
inline int64_t ProfileTicksPerSecondCpu()
{
	static int64_t nTicksPerSecond = 0;
	if (nTicksPerSecond == 0)
	{
		mach_timebase_info_data_t sTimebaseInfo;
		mach_timebase_info(&sTimebaseInfo);
		nTicksPerSecond = 1000000000ll * sTimebaseInfo.denom / sTimebaseInfo.numer;
	}
	return nTicksPerSecond;
}

#define P_BREAK() __builtin_trap()
#if __has_feature(tls)
#define P_THREAD_LOCAL thread_local
#endif
#define P_STRCASECMP strcasecmp

#define P_GETCURRENTPROCESSID() getpid()
typedef uint32_t ProfileProcessIdType;
#elif defined(_WIN32)
int64_t ProfileGetTick();
#define P_TICK() ProfileGetTick()
#define P_BREAK() __debugbreak()
#define P_THREAD_LOCAL __declspec(thread)
#define P_STRCASECMP _stricmp
typedef uint32_t ProfileThreadIdType;
#define P_GETCURRENTPROCESSID() GetCurrentProcessId()
typedef uint32_t ProfileProcessIdType;

#elif defined(__linux__) || defined(ORBIS) || defined(PROSPERO)
#include <unistd.h>
#include <time.h>
inline int64_t ProfileTicksPerSecondCpu()
{
	return 1000000000ll;
}

inline int64_t ProfileGetTick()
{
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return 1000000000ll * ts.tv_sec + ts.tv_nsec;
}
#define strcpy_s(pDest, size, pSrc) strncpy(pDest,pSrc, size)
#define fopen_s(pFile,filename,mode) ((*(pFile))=(FILE*)open_file((filename),(mode)))==NULL

#define P_TICK() ProfileGetTick()
#define P_BREAK() __builtin_trap()
//#ifndef __ANDROID__ // __thread is incompatible with ffunction-sections/fdata-sections
#define P_THREAD_LOCAL thread_local
//#endif
#define P_STRCASECMP strcasecmp
typedef uint64_t ProfileThreadIdType;
#define P_GETCURRENTPROCESSID() getpid()
typedef uint32_t ProfileProcessIdType;

#elif defined(NX64)
#include <time.h>
inline int64_t ProfileGetTick()
{
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return 1000000000ll * ts.tv_sec + ts.tv_nsec;
}

inline int64_t ProfileTicksPerSecondCpu()
{
	return 1000000000ll;
}

#define P_TICK() ProfileGetTick()
#define P_BREAK() __builtin_trap()
#define P_THREAD_LOCAL thread_local
#define P_STRCASECMP strcasecmp
#endif

#ifndef P_GETCURRENTPROCESSID
#define P_GETCURRENTPROCESSID() 0
typedef uint32_t ProfileProcessIdType;
#endif

#ifndef P_ASSERT
#define P_ASSERT(a) do{if(!(a)){P_BREAK();} }while(0)
#endif

typedef uint16_t ProfileGroupId;

#define PROFILE_DECLARE(var) extern ProfileToken g_mp_##var
#define PROFILE_DEFINE(var, group, name, color) ProfileToken g_mp_##var = ProfileGetToken(group, name, color, ProfileTokenTypeCpu)
#define PROFILE_DECLARE_GPU(var) extern ProfileToken g_mp_##var
#define PROFILE_DEFINE_GPU(var, name, color) ProfileToken g_mp_##var = ProfileGetToken("GPU", name, color, ProfileTokenTypeGpu)
#define PROFILE_TOKEN_PASTE0(a, b) a ## b
#define PROFILE_TOKEN_PASTE(a, b)  PROFILE_TOKEN_PASTE0(a,b)
#define PROFILE_SCOPE(var) ProfileScopeHandlerCpu PROFILE_TOKEN_PASTE(foo, __LINE__)(g_mp_##var)
#define PROFILE_SCOPE_TOKEN(token) ProfileScopeHandlerCpu PROFILE_TOKEN_PASTE(foo, __LINE__)(token)
#define PROFILE_SCOPEI(group, name, color) static ProfileToken PROFILE_TOKEN_PASTE(g_mp,__LINE__) = ProfileGetToken(group, name, color, ProfileTokenTypeCpu); ProfileScopeHandlerCpu PROFILE_TOKEN_PASTE(foo,__LINE__)( PROFILE_TOKEN_PASTE(g_mp,__LINE__))
#define PROFILE_META_CPU(name, count) do { static ProfileToken PROFILE_TOKEN_PASTE(g_mp_meta,__LINE__) = ProfileGetMetaToken(name); ProfileMetaUpdate(PROFILE_TOKEN_PASTE(g_mp_meta,__LINE__), count, ProfileTokenTypeCpu); } while(0)
#define PROFILE_META_GPU(name, count) do { static ProfileToken PROFILE_TOKEN_PASTE(g_mp_meta,__LINE__) = ProfileGetMetaToken(name); ProfileMetaUpdate(PROFILE_TOKEN_PASTE(g_mp_meta,__LINE__), count, ProfileTokenTypeGpu); } while(0)
#define PROFILE_LABEL(group, name) do { static ProfileToken PROFILE_TOKEN_PASTE(g_mp,__LINE__) = ProfileGetLabelToken(group); ProfileLabel(PROFILE_TOKEN_PASTE(g_mp,__LINE__), name); } while(0)
#define PROFILE_LABELF(group, name, ...) do { static ProfileToken PROFILE_TOKEN_PASTE(g_mp,__LINE__) = ProfileGetLabelToken(group); ProfileLabelFormat(PROFILE_TOKEN_PASTE(g_mp,__LINE__), name, ## __VA_ARGS__); } while(0)
#define PROFILE_COUNTER_ADD(name, count) do { static ProfileToken PROFILE_TOKEN_PASTE(g_mp_counter,__LINE__) = ProfileGetCounterToken(name); ProfileCounterAdd(PROFILE_TOKEN_PASTE(g_mp_counter,__LINE__), count); } while(0)
#define PROFILE_COUNTER_SUB(name, count) do { static ProfileToken PROFILE_TOKEN_PASTE(g_mp_counter,__LINE__) = ProfileGetCounterToken(name); ProfileCounterAdd(PROFILE_TOKEN_PASTE(g_mp_counter,__LINE__), -(int64_t)count); } while(0)
#define PROFILE_COUNTER_SET(name, count) do { static ProfileToken PROFILE_TOKEN_PASTE(g_mp_counter,__LINE__) = ProfileGetCounterToken(name); ProfileCounterSet(PROFILE_TOKEN_PASTE(g_mp_counter,__LINE__), count); } while(0)
#define PROFILE_COUNTER_SET_LIMIT(name, count) do { static ProfileToken PROFILE_TOKEN_PASTE(g_mp_counter,__LINE__) = ProfileGetCounterToken(name); ProfileCounterSetLimit(PROFILE_TOKEN_PASTE(g_mp_counter,__LINE__), count); } while(0)
#define PROFILE_COUNTER_CONFIG(name, type, limit, flags) ProfileCounterConfig(name, type, limit, flags)

#ifndef PROFILE_PER_THREAD_BUFFER_SIZE
#define PROFILE_PER_THREAD_BUFFER_SIZE (2048<<10)
#endif

#ifndef PROFILE_PER_THREAD_GPU_BUFFER_SIZE
#define PROFILE_PER_THREAD_GPU_BUFFER_SIZE (1024<<10)
#endif

#ifndef PROFILE_MAX_FRAME_HISTORY
#define PROFILE_MAX_FRAME_HISTORY 512
#endif

#ifndef PROFILE_PRINTF
#define PROFILE_PRINTF printf
#endif

#ifndef PROFILE_META_MAX
#define PROFILE_META_MAX 8
#endif

#ifndef PROFILE_WEBSERVER_PORT
#define PROFILE_WEBSERVER_PORT 1338
#endif

#ifndef PROFILE_WEBSERVER_FRAMES
#define PROFILE_WEBSERVER_FRAMES 30
#endif

#ifndef PROFILE_WEBSERVER_SOCKET_BUFFER_SIZE
#define PROFILE_WEBSERVER_SOCKET_BUFFER_SIZE (16<<10)
#endif

#ifndef PROFILE_LABEL_BUFFER_SIZE
#define PROFILE_LABEL_BUFFER_SIZE (1024<<10)
#endif

#ifndef PROFILE_GPU_MAX_QUERIES
#define PROFILE_GPU_MAX_QUERIES (8<<10)
#endif

#ifndef PROFILE_GPU_FRAME_DELAY
#define PROFILE_GPU_FRAME_DELAY 3
#endif

#ifndef PROFILE_NAME_MAX_LEN
#define PROFILE_NAME_MAX_LEN 64
#endif

#ifndef PROFILE_LABEL_MAX_LEN
#define PROFILE_LABEL_MAX_LEN 256
#endif

#ifndef PROFILE_EMBED_HTML
#define PROFILE_EMBED_HTML 1
#endif

#ifndef PROFILE_GPU_TIMERS_MULTITHREADED
#define PROFILE_GPU_TIMERS_MULTITHREADED 0
#endif

#define PROFILE_FORCEENABLECPUGROUP(s) ProfileForceEnableGroup(s, ProfileTokenTypeCpu)
#define PROFILE_FORCEDISABLECPUGROUP(s) ProfileForceDisableGroup(s, ProfileTokenTypeCpu)
#define PROFILE_FORCEENABLEGPUGROUP(s) ProfileForceEnableGroup(s, ProfileTokenTypeGpu)
#define PROFILE_FORCEDISABLEGPUGROUP(s) ProfileForceDisableGroup(s, ProfileTokenTypeGpu)


#define PROFILE_INVALID_TICK ((uint64_t)-1)
#define PROFILE_GROUP_MASK_ALL 0xffffffffffff



enum ProfileTokenType
{
	ProfileTokenTypeCpu,
	ProfileTokenTypeGpu,
};

enum ProfileDumpType
{
	ProfileDumpTypeHtml,
	ProfileDumpTypeCsv
};

#ifdef __GNUC__
#define PROFILE_FORMAT(a, b) __attribute__((format(printf, a, b)))
#else
#define PROFILE_FORMAT(a, b)
#endif

struct Profile;

PROFILE_API ProfileToken ProfileFindToken(const char* sGroup, const char* sName, ThreadID* pThread = NULL);
PROFILE_API ProfileToken ProfileGetToken(const char* sGroup, const char* sName, uint32_t nColor, ProfileTokenType Token = ProfileTokenTypeCpu);
PROFILE_API ProfileToken ProfileGetLabelToken(const char* sGroup, ProfileTokenType Token = ProfileTokenTypeCpu);
PROFILE_API const char* ProfileGetLabel(uint32_t eType, uint64_t nLabel);
PROFILE_API ProfileToken ProfileGetMetaToken(const char* pName);
PROFILE_API ProfileToken ProfileGetCounterToken(const char* pName);
PROFILE_API void ProfileMetaUpdate(ProfileToken, int nCount, ProfileTokenType eTokenType);
PROFILE_API void ProfileCounterAdd(ProfileToken nToken, int64_t nCount);
PROFILE_API void ProfileCounterSet(ProfileToken nToken, int64_t nCount);
PROFILE_API void ProfileCounterSetLimit(ProfileToken nToken, int64_t nCount);
PROFILE_API void ProfileCounterConfig(const char* pCounterName, uint32_t nFormat, int64_t nLimit, uint32_t nFlags);
PROFILE_API void ProfileLabel(ProfileToken nToken, const char* pName);
PROFILE_FORMAT(2, 3) PROFILE_API void ProfileLabelFormat(ProfileToken nToken, const char* pName, ...);
PROFILE_API void ProfileLabelFormatV(ProfileToken nToken, const char* pName, va_list args);
PROFILE_API void ProfileLabelLiteral(ProfileToken nToken, const char* pName);
inline uint16_t ProfileGetTimerIndex(ProfileToken t) { return (t & 0xffff); }
inline uint64_t ProfileGetGroupMask(ProfileToken t) { return ((t >> 16)&PROFILE_GROUP_MASK_ALL); }
inline ProfileToken ProfileMakeToken(uint64_t nGroupMask, uint16_t nTimer) { return (nGroupMask << 16) | nTimer; }

PROFILE_API void ProfileTogglePause();
PROFILE_API void ProfileForceEnableGroup(const char* pGroup, ProfileTokenType Type);
PROFILE_API void ProfileForceDisableGroup(const char* pGroup, ProfileTokenType Type);

PROFILE_API void ProfileOnThreadCreate(const char* pThreadName); //should be called from newly created threads
PROFILE_API void ProfileOnThreadExit(); //call on exit to reuse log
PROFILE_API void ProfileSetForceEnable(bool bForceEnable);
PROFILE_API bool ProfileGetForceEnable();
PROFILE_API void ProfileSetEnableAllGroups(bool bEnable);
PROFILE_API void ProfileEnableCategory(const char* pCategory);
PROFILE_API void ProfileDisableCategory(const char* pCategory);
PROFILE_API bool ProfileGetEnableAllGroups();
PROFILE_API void ProfileSetForceMetaCounters(bool bEnable);
PROFILE_API bool ProfileGetForceMetaCounters();
PROFILE_API void ProfileEnableMetaCounter(const char* pMet);
PROFILE_API void ProfileDisableMetaCounter(const char* pMet);
PROFILE_API int ProfileGetAggregateFrames();
PROFILE_API int ProfileGetCurrentAggregateFrames();
PROFILE_API Profile* ProfileGet();
PROFILE_API void ProfileGetRange(uint32_t nPut, uint32_t nGet, uint32_t nRange[2][2]);
PROFILE_API Mutex& ProfileGetMutex();
PROFILE_API struct ProfileThreadLog* ProfileCreateThreadLog(const char* pName);
PROFILE_API void ProfileRemoveThreadLog(struct ProfileThreadLog * pLog);

PROFILE_API void ProfileContextSwitchTraceStart();
PROFILE_API void ProfileContextSwitchTraceStop();

struct ProfileThreadInfo
{
	ProfileProcessIdType nProcessId;
	ThreadID nThreadId;
};

PROFILE_API void ProfileContextSwitchSearch(uint32_t* pContextSwitchStart, uint32_t* pContextSwitchEnd, uint64_t nBaseTicksCpu, uint64_t nBaseTicksEndCpu);
PROFILE_API uint32_t ProfileContextSwitchGatherThreads(uint32_t nContextSwitchStart, uint32_t nContextSwitchEnd, ProfileThreadInfo* Threads, uint32_t* nNumThreadsBase);

PROFILE_API const char* ProfileGetProcessName(ProfileProcessIdType nId, char* Buffer, uint32_t nSize);

PROFILE_API void ProfileDumpFile(const char* pPath, ProfileDumpType eType, uint32_t nFrames);

typedef void ProfileWriteCallback(void* Handle, size_t size, const char* pData);
PROFILE_API void ProfileDumpHtml(ProfileWriteCallback CB, void* Handle, int nMaxFrames, const char* pHost, Renderer* pRenderer);

PROFILE_API int ProfileFormatCounter(int eFormat, int64_t nCounter, char* pOut, uint32_t nBufferSize);

PROFILE_API void ProfileWebServerStart();
PROFILE_API void ProfileWebServerStop();
PROFILE_API uint32_t ProfileWebServerPort();

PROFILE_API uint64_t ProfileEnterGpu(ProfileToken nToken, uint64_t nTick, ProfileThreadLog* pLog);
PROFILE_API void ProfileLeaveGpu(ProfileToken nToken, uint64_t nTick, ProfileThreadLog* pLog);

PROFILE_API const char* ProfileGetThreadName();

#if !defined(PROFILE_THREAD_NAME_FROM_ID)
#define PROFILE_THREAD_NAME_FROM_ID(a) ""
#endif

struct ProfileScopeHandlerCpu
{
	ProfileToken nToken;
	uint64_t nTick;
	ProfileScopeHandlerCpu(ProfileToken Token) :nToken(Token)
	{
		nTick = cpuProfileEnter(nToken);
	}
	~ProfileScopeHandlerCpu()
	{
        cpuProfileLeave(nToken, nTick);
	}
};

#define PROFILE_MAX_COUNTERS 512
#define PROFILE_MAX_COUNTER_NAME_CHARS (PROFILE_MAX_COUNTERS*16)

#define PROFILE_MAX_GROUPS 48 //dont bump! no. of bits used it bitmask
#define PROFILE_MAX_CATEGORIES 16
#define PROFILE_MAX_GRAPHS 5
#define PROFILE_GRAPH_HISTORY 128
#define PROFILE_BUFFER_SIZE ((PROFILE_PER_THREAD_BUFFER_SIZE)/sizeof(ProfileLogEntry))
#define PROFILE_GPU_BUFFER_SIZE ((PROFILE_PER_THREAD_GPU_BUFFER_SIZE)/sizeof(ProfileLogEntry))
#define PROFILE_GPU_FRAMES ((PROFILE_GPU_FRAME_DELAY)+1)
#define PROFILE_MAX_CONTEXT_SWITCH_THREADS 256
#define PROFILE_STACK_MAX 32
//#define PROFILE_MAX_PRESETS 5
#define PROFILE_ANIM_DELAY_PRC 0.5f
#define PROFILE_GAP_TIME 50 //extra ms to fetch to close timers from earlier frames


#ifndef PROFILE_MAX_TIMERS
#define PROFILE_MAX_TIMERS 1024
#endif

#ifndef PROFILE_MAX_THREADS
#define PROFILE_MAX_THREADS 256
#endif 

#ifndef PROFILE_UNPACK_RED
#define PROFILE_UNPACK_RED(c) ((c)>>16)
#endif

#ifndef PROFILE_UNPACK_GREEN
#define PROFILE_UNPACK_GREEN(c) ((c)>>8)
#endif

#ifndef PROFILE_UNPACK_BLUE
#define PROFILE_UNPACK_BLUE(c) ((c))
#endif

#ifndef PROFILE_DEFAULT_PRESET
#define PROFILE_DEFAULT_PRESET "Default"
#endif

// We disable context switch trace because it's unable to open the file needed, and because
// no documentation was found on how to use this
#ifndef PROFILE_CONTEXT_SWITCH_TRACE
#if defined(_WIN32) 
#define PROFILE_CONTEXT_SWITCH_TRACE 0
#elif defined(__APPLE__) && !TARGET_OS_IPHONE
#define PROFILE_CONTEXT_SWITCH_TRACE 0
#else
#define PROFILE_CONTEXT_SWITCH_TRACE 0
#endif
#endif


#if PROFILE_CONTEXT_SWITCH_TRACE
#define PROFILE_CONTEXT_SWITCH_BUFFER_SIZE (128*1024) //2mb with 16 byte entry size
#else
#define PROFILE_CONTEXT_SWITCH_BUFFER_SIZE (1)
#endif

#ifndef PROFILE_MINIZ
#define PROFILE_MINIZ 0
#endif

#ifndef PROFILE_COUNTER_HISTORY
#define PROFILE_COUNTER_HISTORY 1
#endif

#ifdef _WIN32
#include <basetsd.h>
typedef UINT_PTR MpSocket;
#else
typedef int MpSocket;
#endif

typedef ThreadHandle * ProfileThread;

enum ProfileDrawMask
{
	P_DRAW_OFF = 0x0,
	P_DRAW_BARS = 0x1,
	P_DRAW_DETAILED = 0x2,
	P_DRAW_COUNTERS = 0x3,
	P_DRAW_FRAME = 0x4,
	P_DRAW_HIDDEN = 0x5,
	P_DRAW_SIZE = 0x6,
};

enum ProfileDrawBarsMask
{
	P_DRAW_TIMERS = 0x1,
	P_DRAW_AVERAGE = 0x2,
	P_DRAW_MAX = 0x4,
	P_DRAW_MIN = 0x8,
	P_DRAW_CALL_COUNT = 0x10,
	P_DRAW_TIMERS_EXCLUSIVE = 0x20,
	P_DRAW_AVERAGE_EXCLUSIVE = 0x40,
	P_DRAW_MAX_EXCLUSIVE = 0x80,
	P_DRAW_META_FIRST = 0x100,
	P_DRAW_ALL = 0xffffffff,

};

enum ProfileCounterFormat
{
	PROFILE_COUNTER_FORMAT_DEFAULT,
	PROFILE_COUNTER_FORMAT_BYTES,
};

enum ProfileCounterFlags
{
	PROFILE_COUNTER_FLAG_NONE = 0,
	PROFILE_COUNTER_FLAG_DETAILED = 0x1,
	PROFILE_COUNTER_FLAG_DETAILED_GRAPH = 0x2,
	//internal:
	PROFILE_COUNTER_FLAG_INTERNAL_MASK = ~0x3,
	PROFILE_COUNTER_FLAG_HAS_LIMIT = 0x4,
	PROFILE_COUNTER_FLAG_CLOSED = 0x8,
	PROFILE_COUNTER_FLAG_MANUAL_SWAP = 0x10,
	PROFILE_COUNTER_FLAG_LEAF = 0x20,
};

typedef uint64_t ProfileLogEntry;

struct ProfileTimer
{
	uint64_t nTicks;
	uint32_t nCount;
};

struct ProfileCategory
{
	char pName[PROFILE_NAME_MAX_LEN];
	uint64_t nGroupMask;
};

struct ProfileGroupInfo
{
	char pName[PROFILE_NAME_MAX_LEN];
	uint32_t nNameLen;
	uint32_t nGroupIndex;
	uint32_t nNumTimers;
	uint32_t nMaxTimerNameLen;
	uint32_t nColor;
	uint32_t nCategory;
	ProfileTokenType Type;
	ProfileToken nGpuProfileToken;
};

struct ProfileTimerInfo
{
	ProfileToken nToken;
	uint32_t nTimerIndex;
	uint32_t nGroupIndex;
	char pName[PROFILE_NAME_MAX_LEN];
	uint32_t nNameLen;
	uint32_t nColor;
	ThreadID threadID;
	bool bGraph;
};

struct ProfileCounterInfo
{
	int nParent;
	int nSibling;
	int nFirstChild;
	uint16_t nNameLen;
	uint8_t nLevel;
	char* pName;
	uint32_t nFlags;
	int64_t nLimit;
	ProfileCounterFormat eFormat;
};

struct ProfileCounterHistory
{
	uint32_t nPut;
	uint64_t nHistory[PROFILE_GRAPH_HISTORY];
};

struct ProfileGraphState
{
	int64_t nHistory[PROFILE_GRAPH_HISTORY];
	ProfileToken nToken;
	int32_t nKey;
};

struct ProfileContextSwitch
{
	ThreadID nThreadOut;
	ThreadID nThreadIn;
	ProfileProcessIdType nProcessIn;
	int64_t nCpu : 8;
	int64_t nTicks : 56;
};


struct ProfileFrameState
{
	int64_t nFrameStartCpu;
	int64_t nFrameStartGpu[PROFILE_MAX_THREADS];
	uint32_t nLogStart[PROFILE_MAX_THREADS];
};

struct ProfileThreadLog
{
	ProfileLogEntry*	Log;
    tfrg_atomic32_t     nPut;
    tfrg_atomic32_t	    nGet;

	uint32_t 				nGpu;
	ThreadID 				nThreadId;
	uint32_t 				nLogIndex;
    ProfileToken            nGpuToken;

	uint32_t				nStack[PROFILE_STACK_MAX];
	int64_t					nChildTickStack[PROFILE_STACK_MAX];
	uint32_t				nStackPos;

	uint8_t					nGroupStackPos[PROFILE_MAX_GROUPS];
	int64_t 				nGroupTicks[PROFILE_MAX_GROUPS];
	int64_t 				nAggregateGroupTicks[PROFILE_MAX_GROUPS];
	enum
	{
		THREAD_MAX_LEN = 64,
	};
	char					ThreadName[64];
};


struct ProfileGpu
{
	void(*Shutdown)();
	uint32_t(*Flip)();
	uint64_t(*GetTimeStamp)(uint32_t nIndex);
	uint64_t(*GetTicksPerSecond)();
	bool(*GetTickReference)(int64_t* pOutCpu, int64_t* pOutGpu);
};

struct Profile
{
	uint32_t nTotalTimers;
	uint32_t nGroupCount;
	uint32_t nCategoryCount;
	uint32_t nAggregateClear;
	uint32_t nAggregateFlip;
	uint32_t nAggregateFlipCount;
	uint32_t nAggregateFrames;

	uint64_t nAggregateFlipTick;

	uint32_t nDisplay;
	uint32_t nBars;
	uint64_t nActiveGroup;
	uint32_t nActiveBars;

	uint32_t nForceEnable;
	uint32_t nForceMetaCounters;
	uint64_t nForceEnableGroup;
	uint64_t nForceDisableGroup;

	uint64_t nForceGroupUI;
	uint64_t nActiveGroupWanted;
	uint32_t nAllGroupsWanted;
	uint32_t nAllThreadsWanted;

	uint32_t nOverflow;

	uint64_t nGroupMask;
	uint64_t nGroupMaskGpu;
	uint32_t nRunning;
	uint32_t nToggleRunning;
	uint32_t nMaxGroupSize;
	uint32_t nDumpFileNextFrame;
	uint32_t nAutoClearFrames;
	ProfileDumpType eDumpType;
	uint32_t nDumpFrames;
	const char* DumpFile;

	int64_t nPauseTicks;

	float fReferenceTime;
	float fRcpReferenceTime;

	ProfileCategory	CategoryInfo[PROFILE_MAX_CATEGORIES];
	ProfileGroupInfo 	GroupInfo[PROFILE_MAX_GROUPS];
	ProfileTimerInfo 	TimerInfo[PROFILE_MAX_TIMERS];
	uint8_t					TimerToGroup[PROFILE_MAX_TIMERS];

	ProfileTimer 		AccumTimers[PROFILE_MAX_TIMERS];
	uint64_t				AccumMaxTimers[PROFILE_MAX_TIMERS];
	uint64_t				AccumMinTimers[PROFILE_MAX_TIMERS];
	uint64_t				AccumTimersExclusive[PROFILE_MAX_TIMERS];
	uint64_t				AccumMaxTimersExclusive[PROFILE_MAX_TIMERS];

	ProfileTimer 		Frame[PROFILE_MAX_TIMERS];
	uint64_t				FrameExclusive[PROFILE_MAX_TIMERS];

	ProfileTimer 		Aggregate[PROFILE_MAX_TIMERS];
	uint64_t				AggregateMax[PROFILE_MAX_TIMERS];
	uint64_t				AggregateMin[PROFILE_MAX_TIMERS];
	uint64_t				AggregateExclusive[PROFILE_MAX_TIMERS];
	uint64_t				AggregateMaxExclusive[PROFILE_MAX_TIMERS];


	uint64_t 				FrameGroup[PROFILE_MAX_GROUPS];
	uint64_t 				AccumGroup[PROFILE_MAX_GROUPS];
	uint64_t 				AccumGroupMax[PROFILE_MAX_GROUPS];

	uint64_t 				AggregateGroup[PROFILE_MAX_GROUPS];
	uint64_t 				AggregateGroupMax[PROFILE_MAX_GROUPS];


	struct
	{
		uint64_t nCounters[PROFILE_MAX_TIMERS];

		uint64_t nAccum[PROFILE_MAX_TIMERS];
		uint64_t nAccumMax[PROFILE_MAX_TIMERS];

		uint64_t nAggregate[PROFILE_MAX_TIMERS];
		uint64_t nAggregateMax[PROFILE_MAX_TIMERS];

		uint64_t nSum;
		uint64_t nSumAccum;
		uint64_t nSumAccumMax;
		uint64_t nSumAggregate;
		uint64_t nSumAggregateMax;

		const char* pName;
	} MetaCounters[PROFILE_META_MAX];

	ProfileGraphState	Graph[PROFILE_MAX_GRAPHS];
	uint32_t				nGraphPut;

	uint32_t				nThreadActive[PROFILE_MAX_THREADS];
	ProfileThreadLog* 	Pool[PROFILE_MAX_THREADS];
	uint32_t 				nMemUsage;

	uint32_t 				nFrameCurrent;
	uint32_t 				nFrameCurrentIndex;
	uint32_t 				nFramePut;
	uint64_t				nFramePutIndex;

	ProfileFrameState Frames[PROFILE_MAX_FRAME_HISTORY];

	uint64_t				nFlipTicks;
	uint64_t				nFlipAggregate;
	uint64_t				nFlipMax;
	uint64_t				nFlipMin;
	uint64_t				nFlipAggregateDisplay;
	uint64_t				nFlipMaxDisplay;
	uint64_t				nFlipMinDisplay;

	ProfileThread 			ContextSwitchThread;
	bool  						bContextSwitchRunning;
	bool						bContextSwitchStart;
	bool						bContextSwitchStop;
	bool						bContextSwitchAllThreads;
	bool						bContextSwitchNoBars;
	uint32_t					nContextSwitchUsage;
	uint32_t					nContextSwitchLastPut;

	int64_t						nContextSwitchHoverTickIn;
	int64_t						nContextSwitchHoverTickOut;
	ThreadID					nContextSwitchHoverThread;
	ThreadID					nContextSwitchHoverThreadBefore;
	ThreadID					nContextSwitchHoverThreadAfter;
	uint8_t						nContextSwitchHoverCpu;
	uint8_t						nContextSwitchHoverCpuNext;

	uint32_t					nContextSwitchPut;
	ProfileContextSwitch 	ContextSwitch[PROFILE_CONTEXT_SWITCH_BUFFER_SIZE];

	ProfileThread			WebServerThread;

	MpSocket 					WebServerSocket;
	uint32_t					nWebServerPort;

	char						WebServerBuffer[PROFILE_WEBSERVER_SOCKET_BUFFER_SIZE];
	uint32_t					nWebServerPut;
	uint64_t 					nWebServerDataSent;

    tfrg_atomicptr_t			LabelBuffer;
    tfrg_atomic64_t     		nLabelPut;

	char 						CounterNames[PROFILE_MAX_COUNTER_NAME_CHARS];
	ProfileCounterInfo 	CounterInfo[PROFILE_MAX_COUNTERS];
	uint32_t					nNumCounters;
	uint32_t					nCounterNamePos;
    tfrg_atomic64_t      		Counters[PROFILE_MAX_COUNTERS];

#if PROFILE_COUNTER_HISTORY // uses 1kb per allocated counter. 512kb for default counter count
	uint32_t					nCounterHistoryPut;
	int64_t 					nCounterHistory[PROFILE_GRAPH_HISTORY][PROFILE_MAX_COUNTERS]; //flipped to make swapping cheap, drawing more expensive.
	int64_t 					nCounterMax[PROFILE_MAX_COUNTERS];
	int64_t 					nCounterMin[PROFILE_MAX_COUNTERS];
#endif
};

#define P_LOG_TICK_MASK  0x0000ffffffffffff
#define P_LOG_INDEX_MASK 0x1fff000000000000
#define P_LOG_BEGIN_MASK 0xe000000000000000
#define P_LOG_LABEL_LITERAL 0x5
#define P_LOG_GPU_EXTRA 0x4
#define P_LOG_LABEL 0x3
#define P_LOG_META 0x2
#define P_LOG_ENTER 0x1
#define P_LOG_LEAVE 0x0


inline uint64_t ProfileLogType(ProfileLogEntry Index)
{
	return ((P_LOG_BEGIN_MASK & Index) >> 61) & 0x7;
}

inline uint64_t ProfileLogTimerIndex(ProfileLogEntry Index)
{
	return (P_LOG_INDEX_MASK & Index) >> 48;
}

inline ProfileLogEntry ProfileMakeLogIndex(uint64_t nBegin, ProfileToken nToken, uint64_t nTick)
{
	return (nBegin << 61) | (P_LOG_INDEX_MASK&(nToken << 48)) | (P_LOG_TICK_MASK&nTick);
}

inline int64_t ProfileLogTickDifference(ProfileLogEntry Start, ProfileLogEntry End)
{
	uint64_t nStart = Start;
	uint64_t nEnd = End;
	int64_t nDifference = ((nEnd << 16) - (nStart << 16));
	return nDifference >> 16;
}

inline int64_t ProfileLogGetTick(ProfileLogEntry e)
{
	return P_LOG_TICK_MASK & e;
}

inline int64_t ProfileLogSetTick(ProfileLogEntry e, int64_t nTick)
{
	return (P_LOG_TICK_MASK & nTick) | (e & ~P_LOG_TICK_MASK);
}

template<typename T>
T ProfileMin(T a, T b)
{
	return a < b ? a : b;
}

template<typename T>
T ProfileMax(T a, T b)
{
	return a > b ? a : b;
}
template<typename T>
T ProfileClamp(T a, T min_, T max_)
{
	return ProfileMin(max_, ProfileMax(min_, a));
}

inline int64_t ProfileMsToTick(float fMs, int64_t nTicksPerSecond)
{
	return (int64_t)(fMs*0.001f*nTicksPerSecond);
}

inline float ProfileTickToMsMultiplier(int64_t nTicksPerSecond)
{
	return 1000.f / nTicksPerSecond;
}

inline uint16_t ProfileGetGroupIndex(ProfileToken t)
{
	return (uint16_t)ProfileGet()->TimerToGroup[ProfileGetTimerIndex(t)];
}

#define PROFILE_GPU_STATE_DECL(API) \
	void ProfileGpuInitState##API(); \
	ProfileGpuTimerState##API g_ProfileGPU_##API;

#define PROFILE_GPU_STATE_IMPL(API) \
	void ProfileGpuInitState##API() \
	{ \
		P_ASSERT(!S.GPU.Shutdown); \
		memset(&g_ProfileGPU_##API, 0, sizeof(g_ProfileGPU_##API)); \
		S.GPU.Shutdown = ProfileGpuShutdown##API; \
		S.GPU.GetTimeStamp = ProfileGpuGetTimeStamp##API; \
		S.GPU.GetTicksPerSecond = ProfileTicksPerSecondGpu##API; \
		S.GPU.GetTickReference = ProfileGetGpuTickReference##API; \
	}

#endif

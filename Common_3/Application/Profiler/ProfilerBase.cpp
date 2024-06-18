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

#include "ProfilerBase.h"

#include <inttypes.h>

#include "../../Graphics/GraphicsConfig.h"

#include "GpuProfiler.h"

// RENDERER
#include "../../Graphics/Interfaces/IGraphics.h"

// INTERFACES
#include "../../Application/Interfaces/IFont.h"
#include "../../Application/Interfaces/IProfiler.h"
#include "../../Application/Interfaces/IUI.h"
#include "../../Game/Interfaces/IScripting.h"
#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/IFileSystem.h"

#include "../../Utilities/Math/Algorithms.h"

#include "../../Utilities/Interfaces/IMemory.h"

#ifdef ENABLE_PROFILER
/////////////////////////////////////////////////////////////////////////////
// PROFILER UI DECLARATIONS

#define MAX_DETAILED_TIMERS_DRAW 2048
#define MAX_TIME_STR_LEN         32
#define MAX_TITLE_STR_LEN        256
#define FRAME_HISTORY_LEN        128
#define CRITICAL_COLOR_THRESHOLD 0.5f
#define WARNING_COLOR_THRESHOLD  0.3f
#define MAX_TOOLTIP_STR_LEN      256
#define PROFILER_WINDOW_X        0.f
#define PROFILER_WINDOW_Y        0.f
#define MAX_TEMP_BUFFER_SIZE     30

// Must be initialized with application's ui before drawing.
Renderer*    pRendererRef = NULL;
UIComponent* pWidgetUIComponent = NULL;
UIComponent* pMenuUIComponent = NULL;

float gGuiTransparency = 0.1f;

bool gProfilerWidgetUIEnabled = false;
bool gProfilerWidgetMenuUIEnabled = false;
bool gProfilerDrawingEnabled = true;

#if defined(GFX_DRIVER_MEMORY_TRACKING)
UIComponent* pDriverMemTrackerUIComponent = NULL;
bool         gDriverMemoryWidgetUIEnabled = false;
#endif
#if defined(GFX_DEVICE_MEMORY_TRACKING)
UIComponent* pDeviceMemTrackerUIComponent = NULL;
bool         gDeviceMemoryWidgetUIEnabled = false;
#endif

// UI.cpp

// To re-alloc all dynamic data when screen resize unloads it.
bool   gUnloaded = true;
float2 gCurrWindowSize;

float2 gScreenPos;
char   gCpuProfileText[512];

#ifdef ENABLE_FORGE_FONTS
char gGpuProfileTitleText[1024];
#endif

GpuProfiler* getGpuProfiler(ProfileToken nProfileToken);

typedef struct GpuProfileDrawDesc
{
    float mChildIndent = 25.0f;
    float mHeightOffset = 10.0f;
} GpuProfileDrawDesc;

static GpuProfileDrawDesc gDefaultGpuProfileDrawDesc = {};

struct ProfileDetailedModeTime
{
    uint32_t mTimerInfoIndex;
    float    mStartTime;
    float    mEndTime;
    uint32_t mFrameNum;
    float    mCurrFrameTime;
    char     mThreadName[64];
};

struct ProfileDetailedModeFrame
{
    float                    mFrameTime;
    // ProfileDetailedModeTime[dyn_size]
    ProfileDetailedModeTime* mTimers;
};

struct ProfileDetailedModeTooltip
{
    ProfileDetailedModeTooltip() = default; //-V730
    ProfileDetailedModeTooltip(const ProfileDetailedModeTime& detailedLogTimer, UIWidget* widget): mTimer(detailedLogTimer), mWidget(widget)
    {
    }

    ProfileDetailedModeTime mTimer;
    UIWidget*               mWidget;
};

enum ProfileModes
{
    PROFILE_MODE_TIMER,
    PROFILE_MODE_DETAILED,
    PROFILE_MODE_PLOT,
    PROFILE_MODE_MAX
};

enum ProfileAggregateFrames
{
    PROFILE_AGGREGATE_NUM_INF,
    PROFILE_AGGREGATE_NUM_10,
    PROFILE_AGGREGATE_NUM_20,
    PROFILE_AGGREGATE_NUM_30,
    PROFILE_AGGREGATE_NUM_60,
    PROFILE_AGGREGATE_NUM_120,
    PROFILE_AGGREGATE_FRAMES_MAX
};

enum ProfileDumpFramesFile
{
    PROFILE_DUMPFILE_NUM_32,
    PROFILE_DUMPFILE_NUM_64,
    PROFILE_DUMPFILE_NUM_128,
    PROFILE_DUMPFILE_NUM_256,
    PROFILE_DUMPFILE_NUM_512,
    PROFILE_DUMPFILE_MAX
};

enum ProfileDumpFramesDetailedMode
{
    PROFILE_DUMPFRAME_NUM_2,
    PROFILE_DUMPFRAME_NUM_4,
    PROFILE_DUMPFRAME_NUM_8,
    PROFILE_DUMPFRAME_NUM_16,
    PROFILE_DUMPFRAME_MAX
};

enum ProfileReferenceTimes
{
    PROFILE_REFTIME_MS_1,
    PROFILE_REFTIME_MS_2,
    PROFILE_REFTIME_MS_5,
    PROFILE_REFTIME_MS_10,
    PROFILE_REFTIME_MS_15,
    PROFILE_REFTIME_MS_20,
    PROFILE_REFTIME_MS_33,
    PROFILE_REFTIME_MS_66,
    PROFILE_REFTIME_MS_100,
    PROFILE_REFTIME_MS_250,
    PROFILE_REFTIME_MS_500,
    PROFILE_REFTIME_MS_1000,
    PROFILE_REFTIME_MAX
};

static const char* pProfileModesNames[] = { "Timer", "Detailed", "Plot" };

static const char* pAggregateFramesNames[] = { "Infinite", "10", "20", "30", "60", "120" };

static const char* pReferenceTimesNames[] = { "1ms",  "2ms",  "5ms",   "10ms",  "15ms",  "20ms",
                                              "33ms", "66ms", "100ms", "250ms", "500ms", "1000ms" };

static const char* pDumpFramesToFileNames[] = { "32", "64", "128", "256", "512" };

static const char* pDumpFramesDetailedViewNames[] = {
    "2",
    "4",
    "8",
    "16",
};

// Common top-menu data.
ProfileModes                  gProfileMode = PROFILE_MODE_TIMER;
ProfileModes                  gPrevProfileMode = PROFILE_MODE_TIMER;
ProfileAggregateFrames        gAggregateFrames = PROFILE_AGGREGATE_NUM_60;
ProfileReferenceTimes         gReferenceTime = PROFILE_REFTIME_MS_15;
ProfileDumpFramesFile         gDumpFramesToFile = PROFILE_DUMPFILE_NUM_32;
ProfileDumpFramesDetailedMode gDumpFramesDetailedMode = PROFILE_DUMPFRAME_NUM_4;
bool                          gProfilerPaused = false;
float                         gMinPlotReferenceTime = 0.f;
float                         gFrameTime = 0.f;
float                         gFrameTimeData[FRAME_HISTORY_LEN] = { 0.f };
float                         gGPUFrameTime[PROFILE_MAX_GROUPS];
float                         gGPUFrameTimeData[PROFILE_MAX_GROUPS][FRAME_HISTORY_LEN] = { { 0.0f } };
char                          gFrameTimerTitle[MAX_TITLE_STR_LEN] = "FrameTimer";
char                          gGPUTimerTitle[PROFILE_MAX_GROUPS][MAX_TITLE_STR_LEN]{};
float2                        gHistogramSize;

// Timer mode data.
uint32_t gTotalGroups = 0;
uint32_t gTotalTimers = 0;

static const uint32_t gColumnsCount = 10;
static const uint32_t gTimeColumnsCount = gColumnsCount - 1;
static const uint32_t gTimeGpuColumnsCount = 3;

typedef struct TimerColumnData
{
    float4        mColor;
    bstring       mText;
    unsigned char mTextBuf[MAX_TIME_STR_LEN];
} TimerColumnData;

typedef struct TimerRowData
{
    TimerColumnData mCollumns[gTimeColumnsCount];
} TimerRowData;

// stb_ds array of TimerRowData*
// Note: need to allocate TimerRowData separately, so that
// pointers to text and color don't change during execution
TimerRowData** gTimerData = NULL;

// Timer mode color coding.
float4 gCriticalColor = float4(1.f, 0.f, 0.f, 1.f);
float4 gWarningColor = float4(1.f, 1.f, 0.f, 1.f);
float4 gNormalColor = float4(1.f, 1.f, 1.f, 1.f);
float4 gFernGreenColor = float4(0.31f, 0.87f, 0.26f, 1.0f);
float4 gLilacColor = float4(0.f, 1.f, 1.f, 1.0f);

// Plot mode data.
struct PlotModeData
{
    float2*  mTimeData;
    uint32_t mTimerInfo;
    bool     mEnabled;
};

// stb_ds array
PlotModeData* gPlotModeData = NULL;
// stb_ds array of UIWidget*
UIWidget**    gPlotModeWidgets = NULL;
bool          gUpdatePlotModeGUI = false;

// Detailed mode data.
// stb_ds array
ProfileDetailedModeFrame*   gDetailedModeDump = NULL;
// stb_ds array
ProfileDetailedModeTooltip* gDetailedModeTooltips = NULL;
// stb_ds array of UIWidget*
UIWidget**                  gDetailedModeWidgets = NULL;
bool                        gDumpFramesNow = true;
bool                        gShowTooltip = true;
char                        gTooltipData[MAX_TOOLTIP_STR_LEN] = "0";

/////////////////////////////////////////////////////////////////////////////
// PROFILER DECLARATIONS

void initGpuProfilers();
void exitGpuProfilers();

#if defined(_WINDOWS) || defined(XBOX)
#if !defined(_MSC_VER) || _MSC_VER < 1900 // VS2015 includes proper snprintf
#define snprintf _snprintf
#endif

int64_t ProfileTicksPerSecondCpu()
{
    static int64_t nTicksPerSecond = 0;
    if (nTicksPerSecond == 0)
    {
        QueryPerformanceFrequency((LARGE_INTEGER*)&nTicksPerSecond);
    }
    return nTicksPerSecond;
}
int64_t ProfileGetTick()
{
    int64_t ticks;
    QueryPerformanceCounter((LARGE_INTEGER*)&ticks);
    return ticks;
}

#endif

#ifdef ENABLE_PROFILER_WEBSERVER

#if defined(_WINDOWS) || defined(XBOX)
#if defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
#error WinSock.h has already been included; microprofile requires WinSock2
#endif
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#define P_INVALID_SOCKET(f) ((f) == INVALID_SOCKET)
#endif

#if defined(__APPLE__) || defined(__linux__)
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#define P_INVALID_SOCKET(f) ((f) < 0)
#endif

#endif

#if defined(ENABLE_PROFILER_WEBSERVER) || PROFILE_CONTEXT_SWITCH_TRACE
typedef ThreadFunction ProfileThreadFunc;

inline void ProfileThreadStart(ProfileThread* pThread, ProfileThreadFunc Func)
{
    *pThread = (ThreadHandle*)tf_malloc(sizeof(ThreadHandle));
    ThreadDesc desc = {};
    desc.pFunc = Func;
    desc.pData = *pThread;
    strncpy(desc.mThreadName, "ProfilerWebServer", sizeof(desc.mThreadName));
    initThread(&desc, *pThread);
}
inline void ProfileThreadJoin(ProfileThread* pThread)
{
    joinThread(**pThread);
    tf_free(*pThread);
    *pThread = nullptr;
}
#endif

#ifndef PROFILE_DEBUG
#define PROFILE_DEBUG 0
#endif

Profile g_Profile;

static bool g_bUseLock =
    false; /// This is used because windows does not support using mutexes under dll init(which is where global initialization is handled)
static bool g_bOnce = true;

#ifndef P_THREAD_LOCAL
static pthread_key_t  g_ProfileThreadLogKey;
static pthread_once_t g_ProfileThreadLogKeyOnce = PTHREAD_ONCE_INIT;
static void           ProfileCreateThreadLogKey() { pthread_key_create(&g_ProfileThreadLogKey, NULL); }
#else
P_THREAD_LOCAL ProfileThreadLog* g_ProfileThreadLog = nullptr;

struct ForceProfileThreadExit
{
    ~ForceProfileThreadExit()
    {
        if (g_bUseLock)
            ProfileOnThreadExit();
    }

    // NOTE: The thread_local object needs to be used on Linux or it'll never get created!
    //       According to the standard:
    //       "A variable with thread storage duration shall be initialized before its first odr-use (3.2) and, if constructed, shall be
    //       destroyed on thread exit."
    // So the following is used on thread creation to ensure the object is properly constructed.
    void   EnsureConstruction() { dummy = 0; }
    int8_t dummy;
};

P_THREAD_LOCAL ForceProfileThreadExit g_ForceProfileThreadExit;
#endif

/////////////////////////////////////////////////////////////////////////////
// PROFILER UI FUNCTIONS

// Utility functions.
float profileUtilRoundFloatByPrecision(float number, float precision = 2.f)
{
    return roundf(number * powf(10.f, precision)) / powf(10.f, precision);
}

float profileUtilReferenceTimeFromEnum(ProfileReferenceTimes referenceTime)
{
    switch (referenceTime)
    {
    case PROFILE_REFTIME_MS_1:
        return 1.f;
    case PROFILE_REFTIME_MS_2:
        return 2.f;
    case PROFILE_REFTIME_MS_5:
        return 5.f;
    case PROFILE_REFTIME_MS_10:
        return 10.f;
    case PROFILE_REFTIME_MS_15:
        return 15.f;
    case PROFILE_REFTIME_MS_20:
        return 20.f;
    case PROFILE_REFTIME_MS_33:
        return 33.f;
    case PROFILE_REFTIME_MS_66:
        return 66.f;
    case PROFILE_REFTIME_MS_100:
        return 100.f;
    case PROFILE_REFTIME_MS_250:
        return 250.f;
    case PROFILE_REFTIME_MS_500:
        return 500.f;
    case PROFILE_REFTIME_MS_1000:
        return 1000.f;
    default:
        return 15.f;
    }
}

uint32_t profileUtilAggregateFramesFromEnum(ProfileAggregateFrames aggregateFrames)
{
    switch (aggregateFrames)
    {
    case PROFILE_AGGREGATE_NUM_INF:
        return 0;
    case PROFILE_AGGREGATE_NUM_10:
        return 10;
    case PROFILE_AGGREGATE_NUM_20:
        return 20;
    case PROFILE_AGGREGATE_NUM_30:
        return 30;
    case PROFILE_AGGREGATE_NUM_60:
        return 60;
    case PROFILE_AGGREGATE_NUM_120:
        return 120;
    default:
        return 60;
    }
}

uint32_t profileUtilDumpFramesFromFileEnum(ProfileDumpFramesFile dumpFrames)
{
    switch (dumpFrames)
    {
    case PROFILE_DUMPFILE_NUM_32:
        return 32;
    case PROFILE_DUMPFILE_NUM_64:
        return 64;
    case PROFILE_DUMPFILE_NUM_128:
        return 128;
    case PROFILE_DUMPFILE_NUM_256:
        return 256;
    case PROFILE_DUMPFILE_NUM_512:
        return 512;
    default:
        return 32;
    }
}

uint32_t profileUtilDumpFramesDetailedModeEnum(ProfileDumpFramesDetailedMode dumpFrames)
{
    switch (dumpFrames)
    {
    case PROFILE_DUMPFRAME_NUM_2:
        return 2;
    case PROFILE_DUMPFRAME_NUM_4:
        return 4;
    case PROFILE_DUMPFRAME_NUM_8:
        return 8;
    case PROFILE_DUMPFRAME_NUM_16:
        return 16;
    default:
        return 8;
    }
}

void profileUtilTrimFloatString(const char* numericString, char* result, uint32_t precision = 2u)
{
    const char* delim = strchr(numericString, '.');
    ASSERT(delim);
    size_t delimPos = delim - numericString; //-V769
    strncpy(result, numericString, delimPos + 1 + precision);
}

int profileUtilGroupIndexFromName(Profile& S, const char* groupName)
{
    for (uint32_t i = 0; i < S.nGroupCount; ++i)
    {
        if (strcmp(S.GroupInfo[i].pName, groupName) == 0)
        {
            return S.GroupInfo[i].nGroupIndex;
        }
    }
    return 0;
}

vec2 profileUtilCalcWindowSize(int32_t width, int32_t height) { return vec2((float)(width >> 1), (float)(height >> 1)); }

// Callback functions.
void profileCallbkDumpFramesToFile(void* pUserData)
{
    UNREF_PARAM(pUserData);
    dumpProfileData(pRendererRef->pName, profileUtilDumpFramesFromFileEnum(gDumpFramesToFile));
}

void profileCallbkDumpFrames(void* pUserData)
{
    UNREF_PARAM(pUserData);
    // Dump fresh frames to detailed mode and clear any old data.
    gDumpFramesNow = true;

    for (uint32_t i = 0, end = (uint32_t)arrlen(gDetailedModeDump); i < end; ++i)
        arrfree(gDetailedModeDump[i].mTimers);
    arrsetlen(gDetailedModeDump, 0);
}

void profileCallbkPauseProfiler(void* pUserData)
{
    UNREF_PARAM(pUserData);
    ProfileTogglePause();
}

void ProfileCallbkReferenceTimeUpdated(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if (gProfileMode == PROFILE_MODE_PLOT)
    {
        gUpdatePlotModeGUI = true;
    }
}

// Detailed mode functions.
void profileDrawDetailedModeGrid(float startHeightPixels, float startWidthPixels, float interLineDistance, uint32_t totalLines,
                                 float lineHeight)
{
#ifdef ENABLE_FORGE_UI
    float x = startWidthPixels;

    for (uint32_t i = 0; i < totalLines; ++i)
    {
        // Alternate between dark lines as whole numbers and light lines decimal mid.
        float4 color = float4(0.25f);

        if (i % 2 != 0)
        {
            color = float4(0.125f);
        }

        DrawLineWidget lineWidget;
        lineWidget.mPos1 = float2(x, startHeightPixels);
        lineWidget.mPos2 = float2(x, lineHeight);
        lineWidget.mColor = color;
        lineWidget.mAddItem = false;
        UIWidget* pLineWidget = uiCreateComponentWidget(pWidgetUIComponent, "", &lineWidget, WIDGET_TYPE_DRAW_LINE);
        arrpush(gDetailedModeWidgets, pLineWidget);
        REGISTER_LUA_WIDGET(pLineWidget);
        x += interLineDistance;
    }
#endif
}

void profileGetDetailedModeFrameTimeBetweenTicks(int64_t nTicks, int64_t nTicksEnd, int32_t nLogIndex, uint32_t* nFrameBegin,
                                                 uint32_t* nFrameEnd)
{
    Profile& S = *ProfileGet();
    ASSERT(nLogIndex >= 0 && S.Pool[nLogIndex]);

    bool     bGpu = S.Pool[nLogIndex]->nGpu != 0;
    uint32_t nPut = tfrg_atomic32_load_relaxed(&S.Pool[nLogIndex]->nPut);

    uint32_t nBegin = S.nFrameCurrent;

    for (uint32_t i = 0; i < PROFILE_MAX_FRAME_HISTORY - PROFILE_GPU_FRAME_DELAY; ++i)
    {
        uint32_t nFrame = (S.nFrameCurrent + PROFILE_MAX_FRAME_HISTORY - i) % PROFILE_MAX_FRAME_HISTORY;
        uint32_t nCurrStart = S.Frames[nBegin].nLogStart[nLogIndex];
        uint32_t nPrevStart = S.Frames[nFrame].nLogStart[nLogIndex];
        bool bOverflow = (nPrevStart <= nCurrStart) ? (nPut >= nPrevStart && nPut < nCurrStart) : (nPut < nCurrStart || nPut >= nPrevStart);
        if (bOverflow)
            break;

        nBegin = nFrame;
        if ((bGpu ? S.Frames[nBegin].nFrameStartGpu[nLogIndex] : S.Frames[nBegin].nFrameStartCpu) <= nTicks)
            break;
    }

    uint32_t nEnd = nBegin;

    while (nEnd != S.nFrameCurrent)
    {
        nEnd = (nEnd + 1) % PROFILE_MAX_FRAME_HISTORY;
        if ((bGpu ? S.Frames[nEnd].nFrameStartGpu[nLogIndex] : S.Frames[nEnd].nFrameStartCpu) >= nTicksEnd)
            break;
    }

    *nFrameBegin = nBegin;
    *nFrameEnd = nEnd;
}

/// Get data for detailed mode UI from the microprofiler.
void profileUpdateDetailedModeData(Profile& S)
{
    int64_t nTicksPerSecondCpu = ProfileTicksPerSecondCpu();
    float   fToMsCpu = ProfileTickToMsMultiplier(nTicksPerSecondCpu);
    float   fDetailedRange = profileUtilReferenceTimeFromEnum(gReferenceTime);
    int64_t nBaseTicksCpu = S.Frames[S.nFrameCurrent].nFrameStartCpu;
    int64_t nBaseTicksEndCpu = nBaseTicksCpu + ProfileMsToTick(fDetailedRange, nTicksPerSecondCpu);

    uint64_t                 nActiveGroup = S.nAllGroupsWanted ? S.nGroupMask : S.nActiveGroupWanted;
    ProfileDetailedModeFrame frameLog = { 0 };
    frameLog.mFrameTime = fDetailedRange;

    // Go over each active thread to profile.
    for (uint32_t i = 0; i < PROFILE_MAX_THREADS; ++i)
    {
        ProfileThreadLog* pLog = S.Pool[i];
        if (!pLog)
            continue;

        bool    bGpu = pLog->nGpu != 0;
        float   fToMs = bGpu ? ProfileTickToMsMultiplier(getGpuProfileTicksPerSecond(pLog->nGpuToken)) : fToMsCpu;
        int64_t nBaseTicks = bGpu ? S.Frames[S.nFrameCurrent].nFrameStartGpu[i] : nBaseTicksCpu;
        int64_t nBaseTicksEnd =
            bGpu ? nBaseTicks + ProfileMsToTick(fDetailedRange, (int64_t)getGpuProfileTicksPerSecond(pLog->nGpuToken)) : nBaseTicksEndCpu;
        int64_t  nGapTime = 0;
        uint32_t nLogFrameBegin, nLogFrameEnd;
        profileGetDetailedModeFrameTimeBetweenTicks(nBaseTicks - nGapTime, nBaseTicksEnd + nGapTime, i, &nLogFrameBegin, &nLogFrameEnd);

        uint32_t nGet = S.Frames[nLogFrameBegin].nLogStart[i];
        uint32_t nPut = nLogFrameEnd == S.nFrameCurrent ? tfrg_atomic32_load_relaxed(&pLog->nPut) : S.Frames[nLogFrameEnd].nLogStart[i];
        if (nPut == nGet)
            continue;

        uint32_t nRange[2][2] = { { 0, 0 }, { 0, 0 } };
        ProfileGetRange(nPut, nGet, nRange);

        uint32_t nStack[PROFILE_STACK_MAX];
        uint32_t nStackPos = 0;

        {
            uint32_t nStart = nRange[0][0];
            uint32_t nEnd = nRange[0][1];

            for (uint32_t k = nStart; k < nEnd; ++k)
            {
                ProfileLogEntry* pEntry = pLog->Log + k;
                uint64_t         nType = ProfileLogType(*pEntry);

                if (P_LOG_ENTER == nType)
                {
                    ASSERT(nStackPos < PROFILE_STACK_MAX);
                    nStack[nStackPos++] = k;
                }
                else if (P_LOG_LEAVE == nType)
                {
                    if (0 == nStackPos)
                    {
                        continue;
                    }

                    ProfileLogEntry* pEntryEnter = pLog->Log + nStack[nStackPos - 1];

                    // Make sure the entry timer points belong to the same event index.
                    if (ProfileLogTimerIndex(*pEntryEnter) != ProfileLogTimerIndex(*pEntry))
                    {
                        continue;
                    }

                    int64_t  nTickStart = ProfileLogGetTick(*pEntryEnter);
                    int64_t  nTickEnd = ProfileLogGetTick(*pEntry);
                    uint64_t nTimerIndex = ProfileLogTimerIndex(*pEntry);

                    // Make sure the timer is in the groups being processed.
                    if (!(nActiveGroup & (1ull << S.TimerInfo[nTimerIndex].nGroupIndex)))
                    {
                        nStackPos--;
                        continue;
                    }

                    float fMsStart = fToMs * ProfileLogTickDifference(nBaseTicks, nTickStart);
                    float fMsEnd = fToMs * ProfileLogTickDifference(nBaseTicks, nTickEnd);
                    // Add relevant profile data for the frame to be drawn in UI.
                    if (fMsEnd < fDetailedRange && fMsStart > 0 && abs(fMsStart - fMsEnd) > 0.0001f)
                    {
                        ProfileDetailedModeTime timerLog;
                        timerLog.mTimerInfoIndex = (uint32_t)nTimerIndex;
                        timerLog.mStartTime = fMsStart;
                        timerLog.mEndTime = fMsEnd;
                        strncpy(timerLog.mThreadName, pLog->ThreadName, 64);
                        timerLog.mFrameNum = (uint32_t)arrlen(gDetailedModeDump) + 1;
                        arrpush(frameLog.mTimers, timerLog);
                    }

                    nStackPos--;

                    if (0 == nStackPos && ProfileLogTickDifference(nTickEnd, nBaseTicksEnd) < 0)
                    {
                        break;
                    }
                }
            }
        }
    }

    arrpush(gDetailedModeDump, frameLog);
}

/// Get data to display as a tooltip in detailed mode. To enable tooltop just hover over any text in detailed mode.
void profileUpdateDetailedModeTooltip(Profile& S)
{
    gShowTooltip = false;

    for (ptrdiff_t i = 0; i < arrlen(gDetailedModeTooltips); ++i)
    {
        // Need tooltip.
        if (gDetailedModeTooltips[i].mWidget->mHovered)
        {
            // Update tooltip data for this frame.
            memset(gTooltipData, 0, MAX_TOOLTIP_STR_LEN);
            const ProfileDetailedModeTime& timer = gDetailedModeTooltips[i].mTimer;
            const ProfileTimerInfo&        timerInfo = S.TimerInfo[timer.mTimerInfoIndex];

            strcat(gTooltipData, timerInfo.pName);
            strcat(gTooltipData, "\n------------------------------\n");

            strcat(gTooltipData, "Group Name: ");
            strcat(gTooltipData, S.GroupInfo[timerInfo.nGroupIndex].pName);
            strcat(gTooltipData, "\n");

            strcat(gTooltipData, "Thread: ");
            strcat(gTooltipData, timer.mThreadName);
            strcat(gTooltipData, "\n");

            char buffer[MAX_TEMP_BUFFER_SIZE]{};

            snprintf(buffer, MAX_TEMP_BUFFER_SIZE, "%u", timer.mFrameNum);
            strcat(gTooltipData, "Frame Number: ");
            strcat(gTooltipData, buffer);
            strcat(gTooltipData, "\n");

            snprintf(buffer, MAX_TEMP_BUFFER_SIZE, "%f", timer.mCurrFrameTime + timer.mStartTime);
            strcat(gTooltipData, "Start Time(ms): ");
            strcat(gTooltipData, buffer);
            strcat(gTooltipData, "\n");

            snprintf(buffer, MAX_TEMP_BUFFER_SIZE, "%f", timer.mCurrFrameTime + timer.mEndTime);
            strcat(gTooltipData, "End Time(ms)  : ");
            strcat(gTooltipData, buffer);
            strcat(gTooltipData, "\n");

            snprintf(buffer, MAX_TEMP_BUFFER_SIZE, "%f", timer.mEndTime - timer.mStartTime);
            strcat(gTooltipData, "Total Time(ms): ");
            strcat(gTooltipData, buffer);
            strcat(gTooltipData, "\n");

            gShowTooltip = true;
        }
    }
}

/// Main functionality to draw the horizontal UI for detailed mode.
void profileDrawDetailedMode(Profile& S)
{
#ifdef ENABLE_FORGE_UI
    // Remove any tooltip widgets.
    for (ptrdiff_t i = 0; i < arrlen(gDetailedModeTooltips); ++i)
    {
        uiDestroyComponentWidget(pWidgetUIComponent, gDetailedModeTooltips[i].mWidget);
    }
    arrsetlen(gDetailedModeTooltips, 0);

    // Remove all other widgets.
    for (ptrdiff_t i = 0; i < arrlen(gDetailedModeWidgets); ++i)
    {
        uiDestroyComponentWidget(pWidgetUIComponent, gDetailedModeWidgets[i]);
    }
    arrsetlen(gDetailedModeWidgets, 0);

    const float startHeightPixels = gCurrWindowSize.y * 0.2f;
    const float startWidthPixels = gCurrWindowSize.x * 0.02f;
    const float interTimerHeight = gCurrWindowSize.y * 0.01f;
    const float msToPixels = gCurrWindowSize.y * 0.1f;
    const float timerHeight = gCurrWindowSize.y * 0.025f;
    float       frameTime = 0.0f;
    float       frameHeight = 0.f;
    uint32_t    timerCount = 0;
    // Draw all frames in the dump into a timeline.
    for (ptrdiff_t frameIndex = 0; frameIndex < arrlen(gDetailedModeDump); ++frameIndex)
    {
        ProfileDetailedModeFrame& frameToDraw = gDetailedModeDump[frameIndex];
        if (timerCount > MAX_DETAILED_TIMERS_DRAW)
        {
            LOGF(LogLevel::eWARNING, "Reached maximum amount of drawable detailed timers");
            break;
        }

        // Draw all timers as rectangles on the timeline.
        for (uint32_t i = 0; i < (uint32_t)arrlen(frameToDraw.mTimers); ++i)
        {
            if (timerCount > MAX_DETAILED_TIMERS_DRAW)
                break;
            ProfileDetailedModeTime& timer = frameToDraw.mTimers[i];
            timer.mCurrFrameTime = frameTime;
            ProfileTimerInfo& timerInfo = S.TimerInfo[timer.mTimerInfoIndex];
            // timer.mFrameNum = frameIndex + 1;
            float height = startHeightPixels + gCurrWindowSize.y * 0.035f + (interTimerHeight + timerHeight) * timer.mTimerInfoIndex;

            // Overall start and end times.
            float startTime = frameTime + timer.mStartTime;
            float endTime = frameTime + timer.mEndTime;

            float2 pos = float2(startWidthPixels + startTime * msToPixels, height);
            float2 scale = float2(max((endTime - startTime), 0.05f) * msToPixels, timerHeight);
            float4 color = unpackA8B8G8R8(timerInfo.nColor | 0x7D000000);

            // Draw the timer bar and text.
            FilledRectWidget rectWidget;
            rectWidget.mPos = pos;
            rectWidget.mScale = scale;
            rectWidget.mColor = color;

            UIWidget* pRectWidget = uiCreateComponentWidget(pWidgetUIComponent, timerInfo.pName, &rectWidget, WIDGET_TYPE_FILLED_RECT);
            arrpush(gDetailedModeWidgets, pRectWidget);
            REGISTER_LUA_WIDGET(pRectWidget);

            // Add data to draw a tooltip for this timer.
            DrawTextWidget textWidget;
            textWidget.mPos = pos + float2(scale.x, 0.f);
            textWidget.mColor = float4(1.f);

            UIWidget* pTextWidget = uiCreateComponentWidget(pWidgetUIComponent, timerInfo.pName, &textWidget, WIDGET_TYPE_DRAW_TEXT);
            arrpush(gDetailedModeTooltips, ProfileDetailedModeTooltip(timer, pTextWidget));

            REGISTER_LUA_WIDGET(pTextWidget);
            frameHeight = max(frameHeight, height * 1.2f);
            ++timerCount;
        }
        frameTime += frameToDraw.mFrameTime;
    }

    // Add timeline.
    for (uint32_t i = 0; i < (uint32_t)ceilf(frameTime); ++i)
    {
        char indexStr[11]; // Increased buffer size to 11 to avoid buffer overflow warning (-Wformat-overflow)
        snprintf(indexStr, 11, "%u", i);

        DrawTextWidget textWidget;
        textWidget.mPos = float2(startWidthPixels + i * msToPixels, startHeightPixels - gCurrWindowSize.y * 0.03f);
        textWidget.mColor = float4(1.f);

        UIWidget* pTextWidget = uiCreateComponentWidget(pWidgetUIComponent, indexStr, &textWidget, WIDGET_TYPE_DRAW_TEXT);
        arrpush(gDetailedModeWidgets, pTextWidget);
        REGISTER_LUA_WIDGET(pTextWidget);
    }

    // Backgroud vertical lines.
    profileDrawDetailedModeGrid(startHeightPixels, startWidthPixels, msToPixels / 2.f, (uint32_t)ceilf(frameTime * 2.f), frameHeight);
    // Horizontal Separator lines.

    DrawLineWidget topLine;
    topLine.mPos1 = float2(startWidthPixels, startHeightPixels);
    topLine.mPos2 = float2(ceilf(frameTime) * msToPixels, startHeightPixels);
    topLine.mColor = unpackA8B8G8R8(0x64646464);
    topLine.mAddItem = true;

    UIWidget* pTopLine = uiCreateComponentWidget(pWidgetUIComponent, "TopSeparator", &topLine, WIDGET_TYPE_DRAW_LINE);
    arrpush(gDetailedModeWidgets, pTopLine);
    REGISTER_LUA_WIDGET(pTopLine);

    DrawLineWidget bottomLine;
    bottomLine.mPos1 = float2(startWidthPixels, frameHeight);
    bottomLine.mPos2 = float2(ceilf(frameTime) * msToPixels, frameHeight);
    bottomLine.mColor = unpackA8B8G8R8(0x64646464);
    bottomLine.mAddItem = true;

    UIWidget* pBottomLine = uiCreateComponentWidget(pWidgetUIComponent, "BottomSeparator", &bottomLine, WIDGET_TYPE_DRAW_LINE);
    arrpush(gDetailedModeWidgets, pBottomLine);
    REGISTER_LUA_WIDGET(pBottomLine);
    // Tooltip widget.

    DrawTooltipWidget tooltip;
    tooltip.mShowTooltip = &gShowTooltip;
    tooltip.mText = gTooltipData;

    UIWidget* pTooltip = uiCreateComponentWidget(pWidgetUIComponent, "Tooltips", &tooltip, WIDGET_TYPE_DRAW_TOOLTIP);
    arrpush(gDetailedModeWidgets, pTooltip);
    REGISTER_LUA_WIDGET(pTooltip);
#endif
}

// Plot mode functions.
/// Get data for plot mode UI.
void profileUpdatePlotModeData(Profile& S, int frameNum)
{
    const float timeHeightStart = gCurrWindowSize.y * 0.2f;
    const float timeHeightEnd = timeHeightStart + gCurrWindowSize.y * 0.4f;
    const float timeHeight = timeHeightEnd - timeHeightStart;
    const float widthOffsetLeft = gCurrWindowSize.x * 0.02f;
    ;
    const float widthOffsetRight = 0.f;

    // Go over all timers and store frame's data into plotData if enabled.
    uint32_t timerLocation = 0;
    for (uint32_t groupIndex = 0; groupIndex < S.nGroupCount; ++groupIndex)
    {
        float fToMs = ProfileTickToMsMultiplier(S.GroupInfo[groupIndex].Type == ProfileTokenTypeGpu
                                                    ? getGpuProfileTicksPerSecond(S.GroupInfo[groupIndex].nGpuProfileToken)
                                                    : ProfileTicksPerSecondCpu());
        for (uint32_t timerIndex = 0; timerIndex < S.nTotalTimers; ++timerIndex)
        {
            if (S.TimerInfo[timerIndex].nGroupIndex == groupIndex)
            {
                float referenceTime = profileUtilReferenceTimeFromEnum(gReferenceTime);
                float fTime = profileUtilRoundFloatByPrecision(fToMs * S.Frame[timerIndex].nTicks);
                float percentY = clamp(fTime / referenceTime, 0.f, 1.f);
                float x = (float)frameNum / FRAME_HISTORY_LEN * gCurrWindowSize.x;
                if (gPlotModeData[timerLocation].mEnabled)
                {
                    gPlotModeData[timerLocation].mTimeData[frameNum] =
                        float2(clamp(x, widthOffsetLeft, gCurrWindowSize.x - widthOffsetRight), timeHeightEnd - percentY * timeHeight);
                }
                else
                {
                    // Don't render this value as the timer is not enabled.
                    gPlotModeData[timerLocation].mTimeData[frameNum] = float2(FLT_MAX, FLT_MAX);
                }
                timerLocation++;
            }
        }
    }
}

void profileDrawPlotMode(Profile& S)
{
#ifdef ENABLE_FORGE_UI
    // Remove any previous widgets.
    for (ptrdiff_t i = 0; i < arrlen(gPlotModeWidgets); ++i)
    {
        uiDestroyComponentWidget(pWidgetUIComponent, gPlotModeWidgets[i]);
    }
    arrsetlen(gPlotModeWidgets, 0);

    float       referenceTime = profileUtilReferenceTimeFromEnum(gReferenceTime);
    const float baseWidthOffset = gCurrWindowSize.x * 0.02f;
    const float timelineCount = min(10.f, referenceTime);
    const float timeHeightStart = gCurrWindowSize.y * 0.2f;
    const float timeHeightEnd = timeHeightStart + gCurrWindowSize.y * 0.4f;
    const float timeHeight = timeHeightEnd - timeHeightStart;

    // Draw Reference Separator.
    DrawLineWidget lineSeparator;
    lineSeparator.mPos1 = float2(baseWidthOffset, timeHeightStart);
    lineSeparator.mPos2 = float2(baseWidthOffset, timeHeightEnd);
    lineSeparator.mColor = unpackA8B8G8R8(0x64646464);
    lineSeparator.mAddItem = false;

    UIWidget* pLineSeparator = uiCreateComponentWidget(pWidgetUIComponent, "PlotTimelineSeparator", &lineSeparator, WIDGET_TYPE_DRAW_LINE);
    arrpush(gPlotModeWidgets, pLineSeparator);
    REGISTER_LUA_WIDGET(pLineSeparator);

    lineSeparator.mPos1 = float2(gCurrWindowSize.x - baseWidthOffset, timeHeightStart);
    lineSeparator.mPos2 = float2(gCurrWindowSize.x - baseWidthOffset, timeHeightEnd);
    pLineSeparator = uiCreateComponentWidget(pWidgetUIComponent, "PlotTimelineSeparator2", &lineSeparator, WIDGET_TYPE_DRAW_LINE);
    arrpush(gPlotModeWidgets, pLineSeparator);
    REGISTER_LUA_WIDGET(pLineSeparator);

    // Draw Reference times and reference lines.
    for (uint32_t i = 0; i <= (uint32_t)timelineCount; ++i)
    {
        float       percentHeight = (float)i / timelineCount;
        const float xStart = baseWidthOffset;
        const float xEnd = gCurrWindowSize.x - baseWidthOffset;
        float       y = timeHeightStart + percentHeight * timeHeight;

        char floatStr[MAX_TEMP_BUFFER_SIZE];
        snprintf(floatStr, MAX_TEMP_BUFFER_SIZE, "%f", (1.f - percentHeight) * referenceTime);

        char resultStr[MAX_TEMP_BUFFER_SIZE]{};
        profileUtilTrimFloatString(floatStr, resultStr, 1);
        strcat(resultStr, "ms");

        char yStr[MAX_TEMP_BUFFER_SIZE];
        snprintf(yStr, MAX_TEMP_BUFFER_SIZE, "%f", y);

        lineSeparator.mPos1 = float2(xStart, y);
        lineSeparator.mPos2 = float2(xEnd, y);
        lineSeparator.mColor = unpackA8B8G8R8(0x32323232);
        pLineSeparator = uiCreateComponentWidget(pWidgetUIComponent, yStr, &lineSeparator, WIDGET_TYPE_DRAW_LINE);
        arrpush(gPlotModeWidgets, pLineSeparator);
        REGISTER_LUA_WIDGET(pLineSeparator);

        DrawTextWidget text;
        text.mPos = float2(baseWidthOffset, y);
        text.mColor = float4(1.f);
        UIWidget* pText = uiCreateComponentWidget(pWidgetUIComponent, resultStr, &text, WIDGET_TYPE_DRAW_TEXT);
        arrpush(gPlotModeWidgets, pText);
        REGISTER_LUA_WIDGET(pText);
    }

    // Add some space after the graph drawing.
    CursorLocationWidget cursor;
    cursor.mLocation = float2(baseWidthOffset, timeHeightEnd * 1.05f);
    UIWidget* pCursor = uiCreateComponentWidget(pWidgetUIComponent, "", &cursor, WIDGET_TYPE_CURSOR_LOCATION);
    arrpush(gPlotModeWidgets, pCursor);
    REGISTER_LUA_WIDGET(pCursor);

    // Draw Timer Infos.
    for (ptrdiff_t i = 0; i < arrlen(gPlotModeData); ++i)
    {
        ProfileTimerInfo& timerInfo = S.TimerInfo[gPlotModeData[i].mTimerInfo];
        float4            color = unpackA8B8G8R8(timerInfo.nColor | 0xFF000000);

        OneLineCheckboxWidget oneLineCheckbox;
        oneLineCheckbox.pData = &gPlotModeData[i].mEnabled;
        oneLineCheckbox.mColor = color;
        UIWidget* pOneLineCheckbox =
            uiCreateComponentWidget(pWidgetUIComponent, timerInfo.pName, &oneLineCheckbox, WIDGET_TYPE_ONE_LINE_CHECKBOX);
        arrpush(gPlotModeWidgets, pOneLineCheckbox);
        REGISTER_LUA_WIDGET(pOneLineCheckbox);

        // Only 6 timer names in one line to not overflow horizontally.
        if ((i + 1) % 10 != 0)
        {
            HorizontalSpaceWidget space;
            UIWidget*             pSpace = uiCreateComponentWidget(pWidgetUIComponent, "", &space, WIDGET_TYPE_HORIZONTAL_SPACE);
            arrpush(gPlotModeWidgets, pSpace);
            REGISTER_LUA_WIDGET(pSpace);
        }
    }
#endif
}

// Timer mode functions.
void profileDrawTimerMode(Profile& S)
{
#ifdef ENABLE_FORGE_UI
    const char* headerNames[gColumnsCount] = { "Group/Timer",  "Time",       "Average Time",   "Max Time",          "Min Time",
                                               "Call Average", "Call Count", "Exclusive Time", "Exclusive Average", "Exclusive Max Time" };

    // Create the table header.
    UIWidget*        headerWidgets[gColumnsCount];
    UIWidget         headerWidgetBases[gColumnsCount] = {};
    ColorLabelWidget headerLabelWidgets[gColumnsCount] = {};

    for (uint32_t i = 0; i < gColumnsCount; ++i)
    {
        ColorLabelWidget* pColorLabel = &headerLabelWidgets[i];
        pColorLabel->mColor = gLilacColor;

        UIWidget* pLabel = &headerWidgetBases[i];
        pLabel->mType = WIDGET_TYPE_COLOR_LABEL;
        ASSERT(strlen(headerNames[i]) < sizeof(pLabel->mLabel));
        strcpy(pLabel->mLabel, headerNames[i]);

        pLabel->pWidget = pColorLabel;

        headerWidgets[i] = pLabel;
    }

    // Add the header row
    ColumnWidget headerRow;
    headerRow.pPerColumnWidgets = headerWidgets;
    headerRow.mWidgetsCount = gColumnsCount;
    REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWidgetUIComponent, "Header", &headerRow, WIDGET_TYPE_COLUMN));

    SeparatorWidget separator;
    REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWidgetUIComponent, "", &separator, WIDGET_TYPE_SEPARATOR));

    // Add other coloumn data.
    for (uint32_t groupIndex = 0; groupIndex < S.nGroupCount; ++groupIndex)
    {
        ColorLabelWidget colorLabel;
        colorLabel.mColor = gFernGreenColor;
        REGISTER_LUA_WIDGET(
            uiCreateComponentWidget(pWidgetUIComponent, S.GroupInfo[groupIndex].pName, &colorLabel, WIDGET_TYPE_COLOR_LABEL));
        REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWidgetUIComponent, "", &separator, WIDGET_TYPE_SEPARATOR));

        // Timers are not 1-1 with the groups so search entire list(not very large) every time.
        for (uint32_t timerIndex = 0; timerIndex < S.nTotalTimers; ++timerIndex)
        {
            if (S.TimerInfo[timerIndex].nGroupIndex == groupIndex)
            {
                UIWidget* columnWidgets[gColumnsCount];

                UIWidget labelBase = {};
                labelBase.mType = WIDGET_TYPE_LABEL;
                COMPILE_ASSERT(sizeof(S.TimerInfo[timerIndex].pName) < sizeof(labelBase.mLabel));
                strcpy(labelBase.mLabel, S.TimerInfo[timerIndex].pName);

                LabelWidget labelWidget = {};
                labelBase.pWidget = &labelWidget;

                columnWidgets[0] = &labelBase;

                TimerRowData* pTimerRowData = (TimerRowData*)tf_malloc(sizeof(TimerRowData));
                // Store for dynamic text update.
                arrpush(gTimerData, pTimerRowData);

                UIWidget          textBases[gTimeColumnsCount];
                DynamicTextWidget textWidgets[gTimeColumnsCount];

                for (uint32_t i = 0; i < gTimeColumnsCount; ++i)
                {
                    TimerColumnData* pColumnData = &pTimerRowData->mCollumns[i];
                    pColumnData->mColor = gNormalColor;
                    pColumnData->mText = bemptyfromarr(pColumnData->mTextBuf);
                    bassignliteral(&pColumnData->mText, "-");

                    UIWidget* pTextBase = &textBases[i];
                    *pTextBase = {};
                    pTextBase->mType = WIDGET_TYPE_DYNAMIC_TEXT;

                    DynamicTextWidget* pTextWidget = &textWidgets[i];
                    *pTextWidget = {};
                    pTextWidget->pText = &pColumnData->mText;
                    pTextWidget->pColor = &pColumnData->mColor;

                    pTextBase->pWidget = pTextWidget;

                    columnWidgets[i + 1] = pTextBase;
                }

                // Add the time data to the column.
                ColumnWidget timerColWidget = {};
                timerColWidget.pPerColumnWidgets = columnWidgets;
                timerColWidget.mWidgetsCount = gColumnsCount;

                REGISTER_LUA_WIDGET(
                    uiCreateComponentWidget(pWidgetUIComponent, S.TimerInfo[timerIndex].pName, &timerColWidget, WIDGET_TYPE_COLUMN));
            }
        }

        REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWidgetUIComponent, "", &separator, WIDGET_TYPE_SEPARATOR));
    }
#endif
}

void profileResetTimerModeData(uint32_t tableLocation)
{
    TimerRowData* pRow = gTimerData[tableLocation];
    for (uint32_t i = 0; i < gTimeColumnsCount; ++i)
    {
        bassignliteral(&pRow->mCollumns[i].mText, "-");
        pRow->mCollumns[i].mColor = gNormalColor;
    }
}

/// Get data for timer mode functionality.
void profileUpdateTimerModeData(Profile& S, uint32_t groupIndex, uint32_t timerIndex, uint32_t tableLocation)
{
    if (!S.Aggregate[timerIndex].nCount)
    {
        profileResetTimerModeData(tableLocation);
        return;
    }
    bool  bGpu = S.GroupInfo[groupIndex].Type == ProfileTokenTypeGpu;
    float fToMs = ProfileTickToMsMultiplier(bGpu ? getGpuProfileTicksPerSecond(S.GroupInfo[groupIndex].nGpuProfileToken)
                                                 : ProfileTicksPerSecondCpu());

    uint32_t nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;
    uint32_t nAggregateCount = S.Aggregate[timerIndex].nCount ? S.Aggregate[timerIndex].nCount : 1;

    // 6 because we're only showing that much in the UI.
    FORGE_CONSTEXPR float precision = 6.0f;

    // Data to append to the horizontal list of times.
    float    fTime = profileUtilRoundFloatByPrecision(fToMs * S.Frame[timerIndex].nTicks, precision);
    float    fAverage = profileUtilRoundFloatByPrecision(fToMs * (S.Aggregate[timerIndex].nTicks / nAggregateFrames), precision);
    float    fCallAverage = profileUtilRoundFloatByPrecision(fToMs * (S.Aggregate[timerIndex].nTicks / nAggregateCount), precision);
    float    fMax = profileUtilRoundFloatByPrecision(fToMs * (S.AggregateMax[timerIndex]), precision);
    float    fMin = profileUtilRoundFloatByPrecision(fToMs * (S.AggregateMin[timerIndex]), precision);
    uint32_t fCallCount = S.Frame[timerIndex].nCount;
    float    fFrameMsExclusive = profileUtilRoundFloatByPrecision(fToMs * (S.FrameExclusive[timerIndex]), precision);
    float    fAverageExclusive = profileUtilRoundFloatByPrecision(fToMs * (S.AggregateExclusive[timerIndex] / nAggregateFrames), precision);
    float    fMaxExclusive = profileUtilRoundFloatByPrecision(fToMs * (S.AggregateMaxExclusive[timerIndex]), precision);

    TimerRowData* pRow = gTimerData[tableLocation];

    // Fill the timer data buffers with selected timer's data.

    const float timeValues[gTimeColumnsCount] = {
        fTime, fAverage, fMax, fMin, fCallAverage, (float)fCallCount, fFrameMsExclusive, fAverageExclusive, fMaxExclusive
    };

    uint32_t updateCount = bGpu ? gTimeColumnsCount : (gTimeColumnsCount - gTimeGpuColumnsCount);

    // Also add color coding to the times relative to the current selected reference time.
    float criticalTime = CRITICAL_COLOR_THRESHOLD * profileUtilReferenceTimeFromEnum(gReferenceTime);
    float warnTime = WARNING_COLOR_THRESHOLD * profileUtilReferenceTimeFromEnum(gReferenceTime);

    for (uint32_t i = 0; i < updateCount; ++i)
    {
        TimerColumnData* pCol = &pRow->mCollumns[i];
        int              res = bformat(&pCol->mText, "%f", timeValues[i]);
        ASSERT(res == BSTR_OK);
        ASSERT(!bownsdata(&pCol->mText) && "string became dynamic, increase MAX_TIME_STR_LEN");
        if (pCol->mText.data[0] == '0')
        {
            int pos = bstrchr(&pCol->mText, '.');
            ASSERT(pos > 0);
            bdelete(&pCol->mText, 0, pos);
        }

        if (timeValues[i] >= criticalTime)
            pCol->mColor = gCriticalColor;
        else if (timeValues[i] >= warnTime)
            pCol->mColor = gWarningColor;
        else
            pCol->mColor = gNormalColor;
    }

    // Need to handle count separately from others
    const uint32_t criticalCount = 10;
    const uint32_t warnCount = 5;

    if (fCallCount >= criticalCount)
        pRow->mCollumns[5].mColor = gCriticalColor;
    else if (fCallCount >= warnCount)
        pRow->mCollumns[5].mColor = gWarningColor;
    else
        pRow->mCollumns[5].mColor = gNormalColor;
}

void resetProfilerUI()
{
#ifdef ENABLE_FORGE_UI
    if (pWidgetUIComponent)
    {
        uiDestroyAllComponentWidgets(pWidgetUIComponent);
    }

    // Free any allocated timer data mem.
    for (ptrdiff_t i = 0; i < arrlen(gTimerData); ++i)
    {
        tf_free(gTimerData[i]);
    }
    arrsetlen(gTimerData, 0);

    // Free any allocated graph data mem.
    for (ptrdiff_t i = 0; i < arrlen(gPlotModeData); ++i)
    {
        tf_free(gPlotModeData[i].mTimeData);
    }
    arrsetlen(gPlotModeData, 0);
    arrsetlen(gDetailedModeTooltips, 0);
    arrsetlen(gDetailedModeWidgets, 0);
    for (ptrdiff_t i = 0; i < arrlen(gDetailedModeDump); ++i)
    {
        arrfree(gDetailedModeDump[i].mTimers);
    }
    arrsetlen(gDetailedModeDump, 0);
    arrsetlen(gPlotModeWidgets, 0);
    memset(gFrameTimerTitle, 0, MAX_TITLE_STR_LEN);
#endif
}

void drawGpuProfileRecursive(Cmd* pCmd, const GpuProfiler* pGpuProfiler, const FontDrawDesc* pDrawDesc, float2& origin,
                             const uint32_t index, float2& curTotalTxtSizePx)
{
    GpuTimer* pRoot = &pGpuProfiler->pGpuTimerPool[index];
    ASSERT(pRoot);
    uint32_t nTimerIndex = ProfileGetTimerIndex(pRoot->mMicroProfileToken);
    Profile& S = *ProfileGet();
    if (!S.Aggregate[nTimerIndex].nCount)
        return;

    const GpuProfileDrawDesc* pGpuDrawDesc = &gDefaultGpuProfileDrawDesc;
    float2                    pos(origin.x + pGpuDrawDesc->mChildIndent * pRoot->mDepth, origin.y);
    uint32_t                  nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;
    float                     fsToMs = ProfileTickToMsMultiplier((uint64_t)pGpuProfiler->mGpuTimeStampFrequency);
    float                     fAverage = fsToMs * (float)(S.Aggregate[nTimerIndex].nTicks / nAggregateFrames);

    char printableString[128]{};
    strcpy(printableString, pRoot->mName);
    strcat(printableString, " - ");

    char buffer[MAX_TEMP_BUFFER_SIZE];
    snprintf(buffer, MAX_TEMP_BUFFER_SIZE, "%f", fAverage);
    strcat(printableString, buffer);

    snprintf(buffer, MAX_TEMP_BUFFER_SIZE, " ms");
    strcat(printableString, buffer);

    FontDrawDesc drawDesc = {};
    drawDesc.pText = printableString;
    drawDesc.mFontID = pDrawDesc->mFontID;
    drawDesc.mFontColor = pDrawDesc->mFontColor;
    drawDesc.mFontSize = pDrawDesc->mFontSize;
    drawDesc.mFontSpacing = pDrawDesc->mFontSpacing;
    drawDesc.mFontBlur = pDrawDesc->mFontBlur;
    cmdDrawTextWithFont(pCmd, pos, &drawDesc);

    float2 textSizePx = fntMeasureFontText(printableString, pDrawDesc);
    origin.y += textSizePx.y + pGpuDrawDesc->mHeightOffset;
    textSizePx.x += pGpuDrawDesc->mChildIndent * pRoot->mDepth;
    curTotalTxtSizePx.x = max(textSizePx.x, curTotalTxtSizePx.x);
    curTotalTxtSizePx.y += textSizePx.y + pGpuDrawDesc->mHeightOffset;

    for (uint32_t i = index + 1; i < pGpuProfiler->mCurrentPoolIndex; ++i)
    {
        if (pGpuProfiler->pGpuTimerPool[i].pParent == pRoot)
        {
            drawGpuProfileRecursive(pCmd, pGpuProfiler, pDrawDesc, origin, i, curTotalTxtSizePx);
        }
    }
}

float2 cmdDrawGpuProfile(Cmd* pCmd, float2 screenCoordsInPx, ProfileToken nProfileToken, FontDrawDesc* pDrawDesc)
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(pDrawDesc);

    GpuProfiler* pGpuProfiler = getGpuProfiler(nProfileToken);
    if (!pGpuProfiler || !gProfilerDrawingEnabled)
    {
        return float2(0.f, 0.f);
    }

    gScreenPos = screenCoordsInPx;

    pDrawDesc->pText = gGpuProfileTitleText;
    cmdDrawTextWithFont(pCmd, gScreenPos, pDrawDesc);

    float2 totalTextSizePx = fntMeasureFontText(gGpuProfileTitleText, pDrawDesc);
    gScreenPos.y += totalTextSizePx.y + gDefaultGpuProfileDrawDesc.mHeightOffset;
    drawGpuProfileRecursive(pCmd, pGpuProfiler, pDrawDesc, gScreenPos, 0, totalTextSizePx);

    return totalTextSizePx;
#else
    return float2(0.f, 0.f);
#endif
}

float2 cmdDrawCpuProfile(Cmd* pCmd, float2 screenCoordsInPx, FontDrawDesc* pDrawDesc)
{
    if (!gProfilerDrawingEnabled)
    {
        return float2(0.f, 0.f);
    }

#ifdef ENABLE_FORGE_FONTS
    ASSERT(pDrawDesc);

    pDrawDesc->pText = gCpuProfileText;
    float2 totalTextSizePx = float2(0.0f, 0.0f);
    float2 textSize;
    // print cpu name
#if !defined(NX64)
    snprintf(gCpuProfileText, sizeof(gCpuProfileText), "%s", getCpuInfo()->mName);
    cmdDrawTextWithFont(pCmd, screenCoordsInPx, pDrawDesc);
    textSize = fntMeasureFontText(gCpuProfileText, pDrawDesc);
    totalTextSizePx.x = max(totalTextSizePx.x, textSize.x);
    totalTextSizePx.y += textSize.y + gDefaultGpuProfileDrawDesc.mHeightOffset;
    screenCoordsInPx.y += textSize.y + gDefaultGpuProfileDrawDesc.mHeightOffset;
#endif

    // print cpu time

    snprintf(gCpuProfileText, sizeof(gCpuProfileText), "%08.4f ms (avg %08.4f ms)", getCpuFrameTime(), getCpuAvgFrameTime());
    cmdDrawTextWithFont(pCmd, screenCoordsInPx, pDrawDesc);
    textSize = fntMeasureFontText(gCpuProfileText, pDrawDesc);
    totalTextSizePx.x = max(totalTextSizePx.x, textSize.x);
    totalTextSizePx.y += textSize.y;
    screenCoordsInPx.y += textSize.y + gDefaultGpuProfileDrawDesc.mHeightOffset;

    const uint32_t lightBlue = 0xFFFFFF00;
    const uint32_t green = 0xFF00FF00;
    const uint32_t yellow = 0xFF00FFFF;
    const uint32_t orange = 0xFF0080FF;
    const uint32_t red = 0xFF0000FF;
    const uint32_t pink = 0xFFFF00FF;

    const ThermalStatus thermalStatus = getThermalStatus();
    if (thermalStatus != THERMAL_STATUS_NOT_SUPPORTED)
    {
        FontDrawDesc thermalStatusFontDrawDesc = *pDrawDesc;
        switch (thermalStatus)
        {
        default:
        case THERMAL_STATUS_NONE:
            thermalStatusFontDrawDesc.mFontColor = lightBlue;
            break;
        case THERMAL_STATUS_LIGHT:
            thermalStatusFontDrawDesc.mFontColor = green;
            break;
        case THERMAL_STATUS_MODERATE:
            thermalStatusFontDrawDesc.mFontColor = yellow;
            break;
        case THERMAL_STATUS_SEVERE:
            thermalStatusFontDrawDesc.mFontColor = orange;
            break;
        case THERMAL_STATUS_CRITICAL:
        case THERMAL_STATUS_EMERGENCY:
            thermalStatusFontDrawDesc.mFontColor = red;
            break;
        case THERMAL_STATUS_SHUTDOWN:
            thermalStatusFontDrawDesc.mFontColor = pink;
            break;
        }
        const char* thermalStr = getThermalStatusString(thermalStatus);
        snprintf(gCpuProfileText, sizeof(gCpuProfileText), "Thermal: %s (%d)", thermalStr, thermalStatus);
        cmdDrawTextWithFont(pCmd, screenCoordsInPx, &thermalStatusFontDrawDesc);
        textSize = fntMeasureFontText(gCpuProfileText, pDrawDesc);
        totalTextSizePx.x = max(totalTextSizePx.x, textSize.x);
        totalTextSizePx.y += textSize.y + gDefaultGpuProfileDrawDesc.mHeightOffset;
        screenCoordsInPx.y += textSize.y + gDefaultGpuProfileDrawDesc.mHeightOffset;
    }
#if defined(TF_MEMORY_STATE_API)
    const MemoryState memoryState = getMemoryState();
    if (memoryState != MEMORY_STATE_UNKNOWN)
    {
        const char*  memoryStateString = getMemoryStateString(memoryState);
        FontDrawDesc memoryStateDrawDesc = *pDrawDesc;
        switch (memoryState)
        {
        case MEMORY_STATE_OK:
            memoryStateDrawDesc.mFontColor = green;
            break;
        case MEMORY_STATE_APPROACHING_LIMIT:
            memoryStateDrawDesc.mFontColor = orange;
            break;
        case MEMORY_STATE_CRITICAL:
            memoryStateDrawDesc.mFontColor = red;
            break;
        default:
            break;
        }
        snprintf(gCpuProfileText, sizeof(gCpuProfileText), "Memory State: %s (%d)", memoryStateString, memoryState);
        cmdDrawTextWithFont(pCmd, screenCoordsInPx, &memoryStateDrawDesc);
        textSize = fntMeasureFontText(gCpuProfileText, pDrawDesc);
        totalTextSizePx.x = max(totalTextSizePx.x, textSize.x);
        totalTextSizePx.y += textSize.y + gDefaultGpuProfileDrawDesc.mHeightOffset;
        screenCoordsInPx.y += textSize.y + gDefaultGpuProfileDrawDesc.mHeightOffset;
    }
#endif // ANDROID
    totalTextSizePx.y -= gDefaultGpuProfileDrawDesc.mHeightOffset;
    return totalTextSizePx;
#else
    return float2(0.f, 0.f);
#endif
}
/// Draws the top menu and draws the selected timer mode.
void profileLoadWidgetUI(Profile& S)
{
#ifdef ENABLE_FORGE_UI
    resetProfilerUI();

    // enum for indexing widgets
    enum
    {
        PROFILE_MODE,
        AGGREGATE_FRAMES,
        REFERENCE,
        DUMP,
        PAUSE_CHECKBOX,
        WIDGETS_MAX
    };

    UIWidget* columnWidgets[WIDGETS_MAX];
    UIWidget  columnWidgetBases[WIDGETS_MAX];

    for (uint32_t i = 0; i < WIDGETS_MAX; ++i)
        columnWidgets[i] = &columnWidgetBases[i];

    // Profile mode
    UIWidget* pProfileModeBase = &columnWidgetBases[PROFILE_MODE];
    pProfileModeBase->mType = WIDGET_TYPE_DROPDOWN;
    strcpy(pProfileModeBase->mLabel, "Select Profile Mode");

    DropdownWidget profileMode = {};
    profileMode.pData = (uint32_t*)&gProfileMode;
    profileMode.pNames = pProfileModesNames;
    profileMode.mCount = PROFILE_MODE_MAX;

    pProfileModeBase->pWidget = &profileMode; //-V506

    // Aggregate frames
    UIWidget* pAggregateBase = &columnWidgetBases[AGGREGATE_FRAMES];
    pAggregateBase->mType = WIDGET_TYPE_DROPDOWN;
    strcpy(pAggregateBase->mLabel, "Aggregate Frames");

    DropdownWidget aggregate = {};
    aggregate.pData = (uint32_t*)&gAggregateFrames;
    aggregate.pNames = pAggregateFramesNames;
    aggregate.mCount = PROFILE_AGGREGATE_FRAMES_MAX;

    pAggregateBase->pWidget = &aggregate; //-V506

    // Reference times
    UIWidget* pReferenceBase = &columnWidgetBases[REFERENCE];
    pReferenceBase->mType = WIDGET_TYPE_DROPDOWN;
    strcpy(pReferenceBase->mLabel, "Reference Times");

    DropdownWidget reference = {};
    reference.pData = (uint32_t*)&gReferenceTime;
    reference.pNames = pReferenceTimesNames;
    reference.mCount = PROFILE_REFTIME_MAX;

    pReferenceBase->pWidget = &reference; //-V506

    uiSetWidgetOnEditedCallback(pReferenceBase, nullptr, ProfileCallbkReferenceTimeUpdated);

    DropdownWidget dump = {};

    // Don't dump frames to disk in detailed mode.
    if (gProfileMode == PROFILE_MODE_DETAILED)
    {
        UIWidget* pDetailedBase = &columnWidgetBases[DUMP];
        pDetailedBase->mType = WIDGET_TYPE_DROPDOWN;
        strcpy(pDetailedBase->mLabel, "Dump Frames Detailed View");

        dump.pData = (uint32_t*)&gDumpFramesDetailedMode;

        COMPILE_ASSERT(TF_ARRAY_COUNT(pDumpFramesDetailedViewNames) == PROFILE_DUMPFRAME_MAX);
        dump.pNames = pDumpFramesDetailedViewNames;
        dump.mCount = PROFILE_DUMPFRAME_MAX;

        pDetailedBase->pWidget = &dump; //-V506

        uiSetWidgetOnEditedCallback(pDetailedBase, nullptr, profileCallbkDumpFrames);

        gDumpFramesNow = true;
    }
    else
    {
        UIWidget* pDumpBase = &columnWidgetBases[DUMP];
        pDumpBase->mType = WIDGET_TYPE_DROPDOWN;
        strcpy(pDumpBase->mLabel, "Dump Frames Detailed View");

        dump.pData = (uint32_t*)&gDumpFramesToFile;
        dump.pNames = pDumpFramesToFileNames;
        dump.mCount = PROFILE_DUMPFILE_MAX;

        pDumpBase->pWidget = &dump; //-V506

        uiSetWidgetOnEditedCallback(pDumpBase, nullptr, profileCallbkDumpFramesToFile);
    }

    // Pause checkbox
    UIWidget* pCheckboxBase = &columnWidgetBases[PAUSE_CHECKBOX];
    pCheckboxBase->mType = WIDGET_TYPE_CHECKBOX;
    strcpy(pCheckboxBase->mLabel, "Profiler Paused");

    CheckboxWidget checkbox = {};
    checkbox.pData = &gProfilerPaused;

    pCheckboxBase->pWidget = &checkbox; //-V506
    pCheckboxBase->pOnEdited = profileCallbkPauseProfiler;

    ColumnWidget topMenuRow = {};
    topMenuRow.pPerColumnWidgets = columnWidgets;
    topMenuRow.mWidgetsCount = WIDGETS_MAX;

    REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWidgetUIComponent, "topmenu", &topMenuRow, WIDGET_TYPE_COLUMN));

    SeparatorWidget separator;
    REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWidgetUIComponent, "", &separator, WIDGET_TYPE_SEPARATOR));

    if (gProfileMode == PROFILE_MODE_TIMER)
    {
        PlotLinesWidget plotLines;
        plotLines.mValues = gFrameTimeData;
        plotLines.mNumValues = FRAME_HISTORY_LEN;
        plotLines.mScaleMin = &gMinPlotReferenceTime;
        plotLines.mScaleMax = &S.fReferenceTime;
        plotLines.mPlotScale = &gHistogramSize;
        plotLines.mTitle = gFrameTimerTitle;

        REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWidgetUIComponent, "", &plotLines, WIDGET_TYPE_PLOT_LINES));
        for (uint32_t i = 0; i < S.nGroupCount; ++i)
        {
            if (S.GroupInfo[i].Type == ProfileTokenTypeGpu)
            {
                plotLines.mValues = gGPUFrameTimeData[i];
                plotLines.mNumValues = FRAME_HISTORY_LEN;
                plotLines.mScaleMin = &gMinPlotReferenceTime;
                plotLines.mScaleMax = &S.fReferenceTime;
                plotLines.mPlotScale = &gHistogramSize;
                plotLines.mTitle = gGPUTimerTitle[i];

                REGISTER_LUA_WIDGET(uiCreateComponentWidget(pWidgetUIComponent, "", &plotLines, WIDGET_TYPE_PLOT_LINES));
            }
        }

        profileDrawTimerMode(S);
    }
    else if (gProfileMode == PROFILE_MODE_DETAILED)
    {
        profileDrawDetailedMode(S);
    }
    else if (gProfileMode == PROFILE_MODE_PLOT)
    {
        // Allocate plot data for all timers.
        for (uint32_t groupIndex = 0; groupIndex < S.nGroupCount; ++groupIndex)
        {
            for (uint32_t timerIndex = 0; timerIndex < S.nTotalTimers; ++timerIndex)
            {
                if (S.TimerInfo[timerIndex].nGroupIndex == groupIndex)
                {
                    PlotModeData graphTimer;
                    graphTimer.mTimerInfo = timerIndex;
                    graphTimer.mTimeData = (float2*)tf_malloc(FRAME_HISTORY_LEN * sizeof(float2));
                    graphTimer.mEnabled = false;
                    float4 color = unpackA8B8G8R8(S.TimerInfo[timerIndex].nColor | 0x7D000000);

                    DrawCurveWidget curve;
                    curve.mPos = graphTimer.mTimeData;
                    curve.mNumPoints = FRAME_HISTORY_LEN;
                    curve.mThickness = 1.f;
                    curve.mColor = color;
                    UIWidget* pCurve = uiCreateComponentWidget(pWidgetUIComponent, "Curves", &curve, WIDGET_TYPE_DRAW_CURVE);
                    REGISTER_LUA_WIDGET(pCurve);
                    arrpush(gPlotModeData, graphTimer);
                }
            }
        }

        profileDrawPlotMode(S);
    }
#endif
}

/// Checks for a change in profile mode so we don't have to draw UI widget every frame (expensive).
void profileUpdateProfileMode(Profile& S)
{
    if (gPrevProfileMode != gProfileMode)
    {
        profileLoadWidgetUI(S);
        gPrevProfileMode = gProfileMode;
    }
}

/// Get data for the header histograms and updates the plot mode data. Called every frame.
void profileUpdateWidgetUI(Profile& S)
{
#ifdef ENABLE_FORGE_UI
    float    fToMs = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu());
    uint32_t nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;

    // Check windowSize change.
    float2 windowSize = float2(pWidgetUIComponent->mCurrentWindowRect.getZ(), pWidgetUIComponent->mCurrentWindowRect.getW());
    if (windowSize.x != gCurrWindowSize.x || windowSize.y != gCurrWindowSize.y)
    {
        gCurrWindowSize = windowSize;
        gUpdatePlotModeGUI = true;
    }

    gHistogramSize = float2(gCurrWindowSize.x, 0.1f * gCurrWindowSize.y);
    gFrameTime = fToMs * (S.nFlipTicks);

    // Currently take gpu ticks per second from first Gpu group found
    for (uint32_t i = 0; i < S.nGroupCount; ++i)
    {
        if (S.GroupInfo[i].Type == ProfileTokenTypeGpu)
        {
            gGPUFrameTime[i] = getGpuProfileTime(S.GroupInfo[i].nGpuProfileToken);
        }
    }

    S.fReferenceTime = profileUtilReferenceTimeFromEnum(gReferenceTime);
    S.nAggregateFlip = profileUtilAggregateFramesFromEnum(gAggregateFrames);

    static int frameNum = 0;
    frameNum = (frameNum + 1) % FRAME_HISTORY_LEN;

    // Add the data to histogram array.
    if (!gProfilerPaused)
    {
        gFrameTimeData[frameNum] = gFrameTime;

        snprintf(gFrameTimerTitle, MAX_TITLE_STR_LEN, "Frame: Time[");

        char buffer[MAX_TEMP_BUFFER_SIZE];

        snprintf(buffer, MAX_TEMP_BUFFER_SIZE, "%f", gFrameTime);
        strcat(gFrameTimerTitle, buffer);
        strcat(gFrameTimerTitle, "ms] Avg[");

        snprintf(buffer, MAX_TEMP_BUFFER_SIZE, "%f", fToMs * (S.nFlipAggregateDisplay / nAggregateFrames));
        strcat(gFrameTimerTitle, buffer);
        strcat(gFrameTimerTitle, "ms] Min[");

        snprintf(buffer, MAX_TEMP_BUFFER_SIZE, "%f", fToMs * S.nFlipMinDisplay);
        strcat(gFrameTimerTitle, buffer);
        strcat(gFrameTimerTitle, "ms] Max[");

        snprintf(buffer, MAX_TEMP_BUFFER_SIZE, "%f", fToMs * S.nFlipMaxDisplay);
        strcat(gFrameTimerTitle, buffer);
        strcat(gFrameTimerTitle, "ms]");

        for (uint32_t i = 0; i < S.nGroupCount; ++i)
        {
            if (S.GroupInfo[i].Type == ProfileTokenTypeGpu)
            {
                gGPUFrameTimeData[i][frameNum] = gGPUFrameTime[i];

                memset(gGPUTimerTitle[i], 0, MAX_TITLE_STR_LEN);
                strcpy(gGPUTimerTitle[i], S.GroupInfo[i].pName);
                strcat(gGPUTimerTitle[i], ": Time[");

                char buf[MAX_TEMP_BUFFER_SIZE];
                snprintf(buf, MAX_TEMP_BUFFER_SIZE, "%f", gGPUFrameTime[i]);
                strcat(gGPUTimerTitle[i], buf);

                strcat(gGPUTimerTitle[i], "ms]");
            }
        }

        static int frameNumPlot = 0;
        if (gProfileMode == PROFILE_MODE_PLOT)
        {
            profileUpdatePlotModeData(S, frameNumPlot);
            frameNumPlot = (frameNumPlot + 1) % FRAME_HISTORY_LEN;
        }
        else
        {
            frameNumPlot = 0;
        }
    }
#endif
}

void toggleProfilerUI(bool active) { gProfilerWidgetUIEnabled = active; }
void toggleProfilerMenuUI(bool active) { gProfilerWidgetMenuUIEnabled = active; }
void toggleProfilerDrawing(bool active) { gProfilerDrawingEnabled = active; }

void updateProfilerUI()
{
#ifdef ENABLE_FORGE_UI
    if (!pMenuUIComponent || !pWidgetUIComponent)
        return;

    PROFILER_SET_CPU_SCOPE("MicroProfilerUI", "WidgetsUI", 0x737373);

    pMenuUIComponent->mAlpha = 1.0f - gGuiTransparency;

    Profile& S = *ProfileGet();
    uiSetComponentActive(pWidgetUIComponent, gProfilerWidgetUIEnabled);
    uiSetComponentActive(pMenuUIComponent, gProfilerWidgetMenuUIEnabled);

    // Profiler enabled.
    if (gProfilerWidgetUIEnabled)
    {
        // New groups or timers were found. Create the widget table.
        if (S.nGroupCount != gTotalGroups || S.nTotalTimers != gTotalTimers || gUnloaded)
        {
            profileLoadWidgetUI(S);
            gTotalGroups = S.nGroupCount;
            gTotalTimers = S.nTotalTimers;
            gUnloaded = false;
        }

        // Select profile mode to display.
        profileUpdateProfileMode(S);
        // Update frame history, etc.
        profileUpdateWidgetUI(S);

        // Update timer mode buffers.
        if (gProfileMode == PROFILE_MODE_TIMER)
        {
            uint32_t timerTableLocation = 0;
            for (uint32_t groupIndex = 0; groupIndex < S.nGroupCount; ++groupIndex)
            {
                // Timers are not 1-1 with the groups so search entire list every time.
                for (uint32_t timerIndex = 0; timerIndex < S.nTotalTimers; ++timerIndex)
                {
                    if (S.TimerInfo[timerIndex].nGroupIndex == groupIndex)
                    {
                        profileUpdateTimerModeData(S, groupIndex, timerIndex, timerTableLocation++);
                    }
                }
            }
        }

        // Accumulate frames for detailed view.
        if (gProfileMode == PROFILE_MODE_DETAILED)
        {
            if (gDumpFramesNow)
            {
                // Add this frame data.
                profileUpdateDetailedModeData(S);
                // Finish dumping required number of frames and display in GUI.
                if (arrlenu(gDetailedModeDump) >= profileUtilDumpFramesDetailedModeEnum(gDumpFramesDetailedMode))
                {
                    gDumpFramesNow = false;
                    profileDrawDetailedMode(S);
                }
            }

            // Update mouse hover tooltip for detailed mode.
            profileUpdateDetailedModeTooltip(S);
        }

        // Update plot mode GUI.
        if (gProfileMode == PROFILE_MODE_PLOT)
        {
            // Recreate Plot mode GUI if reference settings are changed.
            if (gUpdatePlotModeGUI)
            {
                profileDrawPlotMode(S);
                gUpdatePlotModeGUI = false;
            }
        }

        pWidgetUIComponent->mAlpha = 1.0f - gGuiTransparency;

    } // profiler enabled.
#endif
}

/////////////////////////////////////////////////////////////////////////////
// PROFILER FUNCTIONS

inline Mutex& ProfileMutex()
{
    static Mutex sMutex;
    return sMutex;
}

Mutex& ProfileGetMutex() { return ProfileMutex(); }

PROFILE_API Profile* ProfileGet() { return &g_Profile; }

void ProfileInit()
{
    Mutex&   mutex = ProfileMutex();
    Profile& S = g_Profile;
    bool     bUseLock = g_bUseLock;
    if (bUseLock)
        acquireMutex(&mutex);
    if (g_bOnce)
    {
        g_bOnce = false;
        initMutex(&mutex);

        // We do not want to reset the Pool array!
        // It could be that other threads still have a valid log thread.
        // The corresponding pool elements will get cleaned up once those threads exit.
        // So before memsetting to 0, let's backup the pool and restore it

        ProfileThreadLog* pool[PROFILE_MAX_THREADS];
        for (int i = 0; i < PROFILE_MAX_THREADS; ++i)
            pool[i] = S.Pool[i];

        memset((void*)&S, 0, sizeof(S));

        for (int i = 0; i < PROFILE_MAX_THREADS; ++i)
            S.Pool[i] = pool[i];

        S.nMemUsage = sizeof(S);
        for (int i = 0; i < PROFILE_MAX_GROUPS; ++i)
        {
            S.GroupInfo[i].pName[0] = '\0';
        }
        for (int i = 0; i < PROFILE_MAX_CATEGORIES; ++i)
        {
            S.CategoryInfo[i].pName[0] = '\0';
            S.CategoryInfo[i].nGroupMask = 0;
        }
#if !defined(_WINDOWS) && !defined(XBOX)
        strcpy(&S.CategoryInfo[0].pName[0], "default");
#else
        strcpy_s(&S.CategoryInfo[0].pName[0], PROFILE_NAME_MAX_LEN, "default");
#endif
        S.nCategoryCount = 1;
        for (int i = 0; i < PROFILE_MAX_TIMERS; ++i)
        {
            S.TimerInfo[i].pName[0] = '\0';
        }
        S.nGroupCount = 0;
        S.nAggregateFlipTick = P_TICK();
        S.nBars = P_DRAW_AVERAGE | P_DRAW_MAX | P_DRAW_CALL_COUNT;
        S.nActiveGroup = 0;
        S.nActiveBars = 0;
        S.nForceEnableGroup = 0;
        S.nForceDisableGroup = 0;
        S.nAllGroupsWanted = 0;
        S.nActiveGroupWanted = 0;
        S.nAllThreadsWanted = 1;
        S.nAggregateFlip = 60;
        S.nTotalTimers = 0;
        for (uint32_t i = 0; i < PROFILE_MAX_GRAPHS; ++i)
        {
            S.Graph[i].nToken = PROFILE_INVALID_TOKEN;
        }
        S.nRunning = 1;
        S.fReferenceTime = 33.33f;
        S.fRcpReferenceTime = 1.f / S.fReferenceTime;
        int64_t nTick = P_TICK();
        for (int i = 0; i < PROFILE_MAX_FRAME_HISTORY; ++i)
        {
            S.Frames[i].nFrameStartCpu = nTick;
            memset(&S.Frames[i].nFrameStartGpu[0], 0, PROFILE_MAX_THREADS * sizeof(S.Frames[i].nFrameStartGpu[0]));
        }

#if PROFILE_COUNTER_HISTORY
        S.nCounterHistoryPut = 0;
        for (uint32_t i = 0; i < PROFILE_MAX_COUNTERS; ++i)
        {
            S.nCounterMin[i] = 0x7fffffffffffffff;
            S.nCounterMax[i] = 0x8000000000000000;
        }
#endif
    }
    if (bUseLock)
        releaseMutex(&mutex);
}

void initProfiler(ProfilerDesc* pDesc)
{
    // PROFILER BASE

#ifdef ENABLE_PROFILER
    ProfileInit();
    ProfileSetEnableAllGroups(true);
    ProfileWebServerStart();

#ifdef ENABLE_GPU_PROFILER
    initGpuProfilers();

    if (pDesc->mGpuProfilerCount > 0)
    {
        ASSERT(pDesc->pRenderer != NULL && pDesc->ppQueues != NULL && pDesc->ppProfilerNames != NULL);
        for (uint32_t i = 0; i < pDesc->mGpuProfilerCount; ++i)
        {
            pDesc->pProfileTokens[i] = addGpuProfiler(pDesc->pRenderer, pDesc->ppQueues[i], pDesc->ppProfilerNames[i]);
        }
    }
#endif
    // store active gpu settings
    ProfileGet()->pGpuSettings = &pDesc->pRenderer->pGpu->mSettings;

    // set gpu profiler title text
    if (pDesc->pRenderer->pGpu->mSettings.mGpuVendorPreset.mGpuDriverVersion[0] != '\0')
    {
        snprintf(gGpuProfileTitleText, sizeof(gGpuProfileTitleText), "%s \t\t\t\t Driver: %s",
                 pDesc->pRenderer->pGpu->mSettings.mGpuVendorPreset.mGpuName,
                 pDesc->pRenderer->pGpu->mSettings.mGpuVendorPreset.mGpuDriverVersion);
    }
    else
    {
        snprintf(gGpuProfileTitleText, sizeof(gGpuProfileTitleText), "%s", pDesc->pRenderer->pGpu->mSettings.mGpuVendorPreset.mGpuName);
    }
#endif

    // PROFILER UI

#ifdef ENABLE_FORGE_UI
    // Remove previous GUI component.
    if (pWidgetUIComponent)
    {
        uiDestroyComponent(pWidgetUIComponent);
    }

    pRendererRef = pDesc->pRenderer;

    UIComponentDesc profilerComponentDesc = {};
    profilerComponentDesc.mStartSize = profileUtilCalcWindowSize(pDesc->mWidthUI, pDesc->mHeightUI);
    profilerComponentDesc.mStartPosition = vec2(PROFILER_WINDOW_X, PROFILER_WINDOW_Y);
    uiCreateComponent(" Micro Profiler ", &profilerComponentDesc, &pWidgetUIComponent);

    // Disable auto resize and enable manual re-size for the profiler window with scrollbars.
    pWidgetUIComponent->mFlags |= GUI_COMPONENT_FLAGS_ALWAYS_HORIZONTAL_SCROLLBAR;
    pWidgetUIComponent->mFlags |= GUI_COMPONENT_FLAGS_ALWAYS_VERTICAL_SCROLLBAR;
    pWidgetUIComponent->mFlags &= (~GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE);
    pWidgetUIComponent->mFlags &= (~GUI_COMPONENT_FLAGS_NO_RESIZE);

    gUnloaded = true;

    // We won't be drawing the default profiler UI anymore.
    Profile& S = *ProfileGet();
    S.nDisplay = P_DRAW_OFF;

    UIComponentDesc guiMenuDesc = {};
    float           dpiScale;
    {
        float          dpiScaleArr[2];
        const uint32_t monitorIdx = getActiveMonitorIdx();
        getMonitorDpiScale(monitorIdx, dpiScaleArr);
        dpiScale = dpiScaleArr[0];
    }

    guiMenuDesc.mStartPosition = vec2(pDesc->mWidthUI - 300.0f * dpiScale, pDesc->mHeightUI * 0.8f);

    uiCreateComponent("Micro Profiler", &guiMenuDesc, &pMenuUIComponent);

    CheckboxWidget checkbox;
    checkbox.pData = &gProfilerWidgetUIEnabled;
    REGISTER_LUA_WIDGET(uiCreateComponentWidget(pMenuUIComponent, "Toggle Profiler", &checkbox, WIDGET_TYPE_CHECKBOX));

#if defined(GFX_DRIVER_MEMORY_TRACKING)
    extern uint64_t GetDriverMemoryAmount();
    if (GetDriverMemoryAmount())
    {
        CheckboxWidget driverMemoryCheckbox;
        driverMemoryCheckbox.pData = &gDriverMemoryWidgetUIEnabled;
        UIWidget* widget =
            uiCreateComponentWidget(pMenuUIComponent, "Toggle Driver Memory tracker", &driverMemoryCheckbox, WIDGET_TYPE_CHECKBOX);
        uiSetWidgetOnEditedCallback(widget, NULL,
                                    [](void*) { pDriverMemTrackerUIComponent->mActive = !pDriverMemTrackerUIComponent->mActive; });

        UIComponentDesc guiMenuDescMemTracking = {};
        uiCreateComponent("Driver memory tracking", &guiMenuDescMemTracking, &pDriverMemTrackerUIComponent);
        pDriverMemTrackerUIComponent->mActive = gDriverMemoryWidgetUIEnabled;
        pDriverMemTrackerUIComponent->mFlags &= ~GUI_COMPONENT_FLAGS_START_COLLAPSED;
        extern void  DrawDriverMemoryTrackingUI(void*);
        CustomWidget trackerWidget = {};
        trackerWidget.pCallback = DrawDriverMemoryTrackingUI;
        uiCreateComponentWidget(pDriverMemTrackerUIComponent, "Custom", &trackerWidget, WIDGET_TYPE_CUSTOM);
    }
#endif

#if defined(GFX_DEVICE_MEMORY_TRACKING)
    extern uint64_t GetDeviceMemoryAmount();
    if (GetDeviceMemoryAmount())
    {
        // Vulkan.cpp
        CheckboxWidget deviceMemoryCheckbox;
        deviceMemoryCheckbox.pData = &gDeviceMemoryWidgetUIEnabled;
        UIWidget* widget =
            uiCreateComponentWidget(pMenuUIComponent, "Toggle Device Memory tracker", &deviceMemoryCheckbox, WIDGET_TYPE_CHECKBOX);
        uiSetWidgetOnEditedCallback(widget, NULL,
                                    [](void*) { pDeviceMemTrackerUIComponent->mActive = !pDeviceMemTrackerUIComponent->mActive; });

        UIComponentDesc guiMenuDescMemTracking = {};
        uiCreateComponent("Device memory tracking", &guiMenuDescMemTracking, &pDeviceMemTrackerUIComponent);
        pDeviceMemTrackerUIComponent->mActive = gDeviceMemoryWidgetUIEnabled;
        pDeviceMemTrackerUIComponent->mFlags &= ~GUI_COMPONENT_FLAGS_START_COLLAPSED;
        extern void  DrawDeviceMemoryReportUI(void*);
        CustomWidget trackerWidget = {};
        trackerWidget.pCallback = DrawDeviceMemoryReportUI;
        uiCreateComponentWidget(pDeviceMemTrackerUIComponent, "Custom", &trackerWidget, WIDGET_TYPE_CUSTOM);
    }
#endif

    SeparatorWidget separator;
    REGISTER_LUA_WIDGET(uiCreateComponentWidget(pMenuUIComponent, "", &separator, WIDGET_TYPE_SEPARATOR));

    ButtonWidget dumpButtonWidget;
    UIWidget*    pDumpButton = uiCreateComponentWidget(pMenuUIComponent, "Dump Profile", &dumpButtonWidget, WIDGET_TYPE_BUTTON);
    pDumpButton->pOnEdited = profileCallbkDumpFramesToFile;
    REGISTER_LUA_WIDGET(pDumpButton);

    REGISTER_LUA_WIDGET(uiCreateComponentWidget(pMenuUIComponent, "", &separator, WIDGET_TYPE_SEPARATOR));

    SliderFloatWidget sliderFloat;
    sliderFloat.pData = &gGuiTransparency;
    sliderFloat.mMin = 0.f;
    sliderFloat.mMax = 0.9f;
    sliderFloat.mStep = 0.01f;
    memset(sliderFloat.mFormat, 0, MAX_FORMAT_STR_LENGTH);
    strcpy(sliderFloat.mFormat, "%.2f");
    REGISTER_LUA_WIDGET(uiCreateComponentWidget(pMenuUIComponent, "Transparency", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT));
#endif
}

void exitCpuProfiler()
{
    ProfileOnThreadExit();
    ProfileWebServerStop();
    ProfileContextSwitchTraceStop();

    g_bOnce = true;
    g_bUseLock = false;
}

void exitProfiler()
{
    // PROFILER UI

#ifdef ENABLE_FORGE_UI
    resetProfilerUI();

    arrfree(gTimerData);
    arrfree(gPlotModeData);
    arrfree(gDetailedModeTooltips);
    arrfree(gDetailedModeWidgets);
    arrfree(gDetailedModeDump);
    arrfree(gPlotModeWidgets);
    memset(gFrameTimerTitle, 0, MAX_TITLE_STR_LEN);
    for (uint32_t i = 0; i < PROFILE_MAX_GROUPS; ++i)
    {
        memset(gGPUTimerTitle[i], 0, MAX_TITLE_STR_LEN);
    }
    pWidgetUIComponent = 0;
    pMenuUIComponent = 0;
    gTotalGroups = 0;
    gTotalTimers = 0;
    gUnloaded = true;
#endif

    // PROFILER BASE

#ifdef ENABLE_PROFILER
    exitCpuProfiler();

#ifdef ENABLE_GPU_PROFILER
    exitGpuProfilers();
#endif
#endif
}

#ifndef P_THREAD_LOCAL
inline ProfileThreadLog* ProfileGetThreadLog()
{
    pthread_once(&g_ProfileThreadLogKeyOnce, ProfileCreateThreadLogKey);
    return (ProfileThreadLog*)pthread_getspecific(g_ProfileThreadLogKey);
}

inline void ProfileSetThreadLog(ProfileThreadLog* pLog)
{
    pthread_once(&g_ProfileThreadLogKeyOnce, ProfileCreateThreadLogKey);
    pthread_setspecific(g_ProfileThreadLogKey, pLog);
}
#else
ProfileThreadLog* ProfileGetThreadLog() { return g_ProfileThreadLog; }

void ProfileSetThreadLog(ProfileThreadLog* pLog) { g_ProfileThreadLog = pLog; }
#endif

PROFILE_API void ProfileRemoveThreadLog(ProfileThreadLog* pLog)
{
    MutexLock lock(ProfileMutex());
    Profile&  S = g_Profile;
    if (pLog)
    {
        int32_t nLogIndex = -1;
        for (int i = 0; i < PROFILE_MAX_THREADS; ++i)
        {
            if (pLog == S.Pool[i])
            {
                nLogIndex = i;
                break;
            }
        }
        P_ASSERT(nLogIndex >= 0 && nLogIndex < PROFILE_MAX_THREADS);

        S.Pool[nLogIndex] = 0; //-V557

        for (int i = 0; i < PROFILE_MAX_FRAME_HISTORY; ++i)
        {
            S.Frames[i].nLogStart[nLogIndex] = 0; //-V557
        }

        if (pLog->Log)
        {
            tf_free(pLog->Log);
            S.nMemUsage -= sizeof(ProfileLogEntry) * PROFILE_BUFFER_SIZE;
        }

        pLog->~ProfileThreadLog();
        tf_free(pLog);
        S.nMemUsage -= sizeof(ProfileThreadLog);
    }
}

ProfileThreadLog* ProfileCreateThreadLog(const char* pName)
{
    Profile&          S = g_Profile;
    ProfileThreadLog* pLog = 0;
    uint32_t          nLogIndex = 0;
    for (uint32_t i = 0; i < PROFILE_MAX_THREADS; ++i)
    {
        if (!S.Pool[i])
        {
            nLogIndex = i;
            pLog = static_cast<ProfileThreadLog*>(tf_malloc(sizeof(ProfileThreadLog)));
            memset(pLog, 0, sizeof(ProfileThreadLog));
            S.nMemUsage += sizeof(ProfileThreadLog);
            S.Pool[i] = pLog;
            break;
        }
    }

    if (!pLog)
        return nullptr;

    memset(pLog, 0, sizeof(*pLog));
    pLog->nLogIndex = nLogIndex;
    int len = (int)strlen(pName);
    int maxlen = sizeof(pLog->ThreadName) - 1;
    len = len < maxlen ? len : maxlen;
    memcpy(&pLog->ThreadName[0], pName, len);
    pLog->ThreadName[len] = '\0';
    pLog->nThreadId = getCurrentThreadID();
    return pLog;
}

void ProfileOnThreadCreate(const char* pThreadName)
{
    g_bUseLock = true;
    ProfileInit();
    MutexLock lock(ProfileMutex());
    if (ProfileGetThreadLog() == 0)
    {
        ProfileThreadLog* pLog = ProfileCreateThreadLog(pThreadName ? pThreadName : ProfileGetThreadName());
        P_ASSERT(pLog);
        ProfileSetThreadLog(pLog);
        g_ForceProfileThreadExit.EnsureConstruction();
    }
}

void ProfileOnThreadExit()
{
    MutexLock         lock(ProfileMutex());
    Profile&          S = g_Profile;
    ProfileThreadLog* pLog = ProfileGetThreadLog();
    if (pLog)
    {
        int32_t nLogIndex = -1;
        for (int i = 0; i < PROFILE_MAX_THREADS; ++i)
        {
            if (pLog == S.Pool[i])
            {
                nLogIndex = i;
                break;
            }
        }
        P_ASSERT(nLogIndex >= 0 && nLogIndex < PROFILE_MAX_THREADS);

        S.Pool[nLogIndex] = 0; //-V557

        for (int i = 0; i < PROFILE_MAX_FRAME_HISTORY; ++i)
        {
            S.Frames[i].nLogStart[nLogIndex] = 0; //-V557
        }

        if (pLog->Log)
        {
            tf_free(pLog->Log);
            S.nMemUsage -= sizeof(ProfileLogEntry) * PROFILE_BUFFER_SIZE;
        }

        pLog->~ProfileThreadLog();
        tf_free(pLog);
        S.nMemUsage -= sizeof(ProfileThreadLog);

        ProfileSetThreadLog(0);
    }
}

ProfileThreadLog* ProfileGetOrCreateThreadLog()
{
    ProfileThreadLog* pLog = ProfileGetThreadLog();

    if (!pLog)
    {
        ProfileOnThreadCreate(nullptr);
        pLog = ProfileGetThreadLog();
    }

    return pLog;
}

const char* ProfileGetThreadName()
{
    static char buffer[MAX_THREAD_NAME_LENGTH + 1]; // Plus null character
    getCurrentThreadName(buffer, MAX_THREAD_NAME_LENGTH);
    return buffer;
}

ProfileToken ProfileFindToken(const char* pGroup, const char* pName, ThreadID* pThreadID)
{
    ProfileInit();
    MutexLock lock(ProfileMutex());
    Profile&  S = g_Profile;
    ThreadID  threadID;
    if (!pThreadID)
        threadID = getCurrentThreadID();
    else
        threadID = *pThreadID;

    for (uint32_t i = 0; i < S.nTotalTimers; ++i)
    {
        if (!P_STRCASECMP(pName, S.TimerInfo[i].pName) && !P_STRCASECMP(pGroup, S.GroupInfo[S.TimerToGroup[i]].pName) &&
            threadID == S.TimerInfo[i].threadID)
        {
            return S.TimerInfo[i].nToken;
        }
    }
    return PROFILE_INVALID_TOKEN;
}

uint16_t ProfileGetGroup(const char* pGroup, ProfileTokenType Type)
{
    Profile& S = g_Profile;
    for (uint32_t i = 0; i < S.nGroupCount; ++i)
    {
        if (!P_STRCASECMP(pGroup, S.GroupInfo[i].pName))
        {
            return (uint16_t)i;
        }
    }

    uint16_t nGroupIndex = (uint16_t)S.nGroupCount++;
    P_ASSERT(nGroupIndex < PROFILE_MAX_GROUPS);

    size_t nLen = strlen(pGroup);
    if (nLen > PROFILE_NAME_MAX_LEN - 1)
        nLen = PROFILE_NAME_MAX_LEN - 1;
    memcpy(&S.GroupInfo[nGroupIndex].pName[0], pGroup, nLen);
    S.GroupInfo[nGroupIndex].pName[nLen] = '\0';
    S.GroupInfo[nGroupIndex].nNameLen = (uint32_t)nLen;

    S.GroupInfo[nGroupIndex].nNumTimers = 0;
    S.GroupInfo[nGroupIndex].nGroupIndex = nGroupIndex;
    S.GroupInfo[nGroupIndex].Type = Type;
    S.GroupInfo[nGroupIndex].nMaxTimerNameLen = 0;
    S.GroupInfo[nGroupIndex].nColor = 0x88888888;
    S.GroupInfo[nGroupIndex].nCategory = 0;

    S.CategoryInfo[0].nGroupMask |= 1ll << nGroupIndex;
    S.nGroupMask |= 1ll << nGroupIndex;
    S.nGroupMaskGpu |= uint64_t(Type == ProfileTokenTypeGpu) << nGroupIndex;

    if ((S.nRunning || S.nForceEnable) && S.nAllGroupsWanted)
        S.nActiveGroup |= 1ll << nGroupIndex;

    return nGroupIndex;
}

ProfileToken ProfileGetToken(const char* pGroup, const char* pName, uint32_t nColor, ProfileTokenType Type)
{
    ProfileInit();
    MutexLock    lock(ProfileMutex());
    Profile&     S = g_Profile;
    ProfileToken ret = ProfileFindToken(pGroup, pName);
    if (ret != PROFILE_INVALID_TOKEN)
        return ret;
    if (S.nTotalTimers == PROFILE_MAX_TIMERS)
        return PROFILE_INVALID_TOKEN;
    uint16_t     nGroupIndex = ProfileGetGroup(pGroup, Type);
    uint16_t     nTimerIndex = (uint16_t)(S.nTotalTimers++);
    uint64_t     nGroupMask = 1ll << nGroupIndex;
    ProfileToken nToken = ProfileMakeToken(nGroupMask, nTimerIndex);
    S.GroupInfo[nGroupIndex].nNumTimers++;
    S.GroupInfo[nGroupIndex].nMaxTimerNameLen = ProfileMax(S.GroupInfo[nGroupIndex].nMaxTimerNameLen, (uint32_t)strlen(pName));
    P_ASSERT(S.GroupInfo[nGroupIndex].Type == Type); // dont mix cpu & gpu timers in the same group
    S.nMaxGroupSize = ProfileMax(S.nMaxGroupSize, S.GroupInfo[nGroupIndex].nNumTimers);
    S.TimerInfo[nTimerIndex].nToken = nToken;
    uint32_t nLen = (uint32_t)strlen(pName);
    if (nLen > PROFILE_NAME_MAX_LEN - 1)
        nLen = PROFILE_NAME_MAX_LEN - 1;
    memcpy(&S.TimerInfo[nTimerIndex].pName, pName, nLen);

    if (nColor == 0xffffffff)
    {
        // http://www.two4u.com/color/small-txt.html with some omissions
        static const int kDebugColors[] = {
            0x70DB93, 0xB5A642, 0x5F9F9F, 0xB87333, 0x4F6F4F, 0x9932CD, 0x871F78, 0x855E42, 0x545454, 0x8E2323, 0x238E23,
            0xCD7F32, 0xDBDB70, 0x527F76, 0x9F9F5F, 0x8E236B, 0xFF2F4F, 0xCFB53B, 0xFF7F00, 0xDB70DB, 0x5959AB, 0x8C1717,
            0x238E68, 0x6B4226, 0x8E6B23, 0x007FFF, 0x00FF7F, 0x236B8E, 0x38B0DE, 0xDB9370, 0xCC3299, 0x99CC32,
        };

        // djb2
        unsigned int result = 5381;
        for (const char* i = pGroup; *i; ++i)
            result = result * 33 ^ *i;
        for (const char* i = pName; *i; ++i)
            result = result * 33 ^ *i;

        nColor = kDebugColors[result % (sizeof(kDebugColors) / sizeof(kDebugColors[0]))];
    }

    S.TimerInfo[nTimerIndex].pName[nLen] = '\0';
    S.TimerInfo[nTimerIndex].nNameLen = nLen;
    S.TimerInfo[nTimerIndex].nColor = nColor & 0xffffff;
    S.TimerInfo[nTimerIndex].nGroupIndex = nGroupIndex;
    S.TimerInfo[nTimerIndex].nTimerIndex = nTimerIndex;
    S.TimerInfo[nTimerIndex].threadID = getCurrentThreadID();
    S.TimerToGroup[nTimerIndex] = (uint8_t)nGroupIndex;
    return nToken;
}

ProfileToken getCpuProfileToken(const char* pGroup, const char* pName, uint32_t nColor)
{
#ifdef ENABLE_PROFILER
    return ProfileGetToken(pGroup, pName, nColor);
#else
    return 0;
#endif
}

ProfileToken ProfileGetLabelToken(const char* pGroup, ProfileTokenType Type)
{
    ProfileInit();
    MutexLock lock(ProfileMutex());

    uint16_t     nGroupIndex = ProfileGetGroup(pGroup, Type);
    uint64_t     nGroupMask = 1ll << nGroupIndex;
    ProfileToken nToken = ProfileMakeToken(nGroupMask, 0);

    return nToken;
}

ProfileToken ProfileGetMetaToken(const char* pName)
{
    ProfileInit();
    MutexLock lock(ProfileMutex());
    Profile&  S = g_Profile;
    for (uint32_t i = 0; i < PROFILE_META_MAX; ++i)
    {
        if (!S.MetaCounters[i].pName)
        {
            S.MetaCounters[i].pName = pName;
            return i;
        }
        else if (!P_STRCASECMP(pName, S.MetaCounters[i].pName))
        {
            return i;
        }
    }
    P_ASSERT(0); // out of slots, increase PROFILE_META_MAX
    return (ProfileToken)-1;
}

const char* ProfileNextName(const char* pName, char* pNameOut, uint32_t* nSubNameLen)
{
    uint32_t    nMaxLen = PROFILE_NAME_MAX_LEN - 1;
    const char* pRet = 0;
    bool        bDone = false;
    uint32_t    nChars = 0;
    for (uint32_t i = 0; i < nMaxLen && !bDone; ++i)
    {
        char c = *pName++;
        switch (c)
        {
        case 0:
            bDone = true;
            break;
        case '\\':
        case '/':
            if (nChars)
            {
                bDone = true;
                pRet = pName;
            }
            break;
        default:
            nChars++;
            *pNameOut++ = c;
        }
    }
    *nSubNameLen = nChars;
    *pNameOut = '\0';
    return pRet;
}

const char* ProfileCounterFullName(int nCounter)
{
    Profile&    S = g_Profile;
    static char Buffer[1024];
    int         nNodes[32];
    int         nIndex = 0;
    do
    {
        nNodes[nIndex++] = nCounter;
        nCounter = S.CounterInfo[nCounter].nParent;
    } while (nCounter >= 0);
    int nOffset = 0;
    while (nIndex >= 0 && nOffset < (int)sizeof(Buffer) - 2)
    {
        uint32_t nLen = S.CounterInfo[nNodes[nIndex]].nNameLen + nOffset; // < sizeof(Buffer)-1
        nLen = ProfileMin((uint32_t)(sizeof(Buffer) - 2 - nOffset), nLen);
        memcpy(&Buffer[nOffset], S.CounterInfo[nNodes[nIndex]].pName, nLen);

        nOffset += S.CounterInfo[nNodes[nIndex]].nNameLen + 1;
        if (nIndex)
        {
            Buffer[nOffset++] = '/';
        }
        nIndex--;
    }
    return &Buffer[0];
}

int ProfileGetCounterTokenByParent(int nParent, const char* pName)
{
    Profile& S = g_Profile;
    for (uint32_t i = 0; i < S.nNumCounters; ++i)
    {
        if (nParent == S.CounterInfo[i].nParent && !P_STRCASECMP(S.CounterInfo[i].pName, pName))
        {
            return i;
        }
    }
    ProfileToken nResult = S.nNumCounters++;
    S.CounterInfo[nResult].nParent = nParent;
    S.CounterInfo[nResult].nSibling = -1;
    S.CounterInfo[nResult].nFirstChild = -1;
    S.CounterInfo[nResult].nFlags = 0;
    S.CounterInfo[nResult].eFormat = PROFILE_COUNTER_FORMAT_DEFAULT;
    S.CounterInfo[nResult].nLimit = 0;
    int nLen = (int)strlen(pName) + 1;

    P_ASSERT(nLen + S.nCounterNamePos <= PROFILE_MAX_COUNTER_NAME_CHARS);
    uint32_t nPos = S.nCounterNamePos;
    S.nCounterNamePos += nLen;
    memcpy(&S.CounterNames[nPos], pName, nLen);
    S.CounterInfo[nResult].nNameLen = (uint16_t)(nLen - 1);
    S.CounterInfo[nResult].pName = &S.CounterNames[nPos];
    if (nParent >= 0)
    {
        S.CounterInfo[nResult].nSibling = S.CounterInfo[nParent].nFirstChild;
        S.CounterInfo[nResult].nLevel = S.CounterInfo[nParent].nLevel + 1;
        S.CounterInfo[nParent].nFirstChild = (int)nResult;
    }
    else
    {
        S.CounterInfo[nResult].nLevel = 0;
    }

    return (int)nResult;
}

ProfileToken ProfileGetCounterToken(const char* pName)
{
    ProfileInit();
    MutexLock lock(ProfileMutex());
    Profile&  S = g_Profile;
    char      SubName[PROFILE_NAME_MAX_LEN];
    int       nResult = -1;
    do
    {
        uint32_t nLen = 0;
        pName = ProfileNextName(pName, &SubName[0], &nLen);
        if (0 == nLen)
        {
            break;
        }
        nResult = ProfileGetCounterTokenByParent(nResult, SubName);

    } while (pName != 0);
    S.CounterInfo[nResult].nFlags |= PROFILE_COUNTER_FLAG_LEAF;

    P_ASSERT(nResult >= 0);
    return nResult;
}

inline void ProfileLogPut(ProfileToken nToken_, uint64_t nTick, uint64_t nBegin, ProfileThreadLog* pLog)
{
    P_ASSERT(pLog != 0); // this assert is hit if ProfileOnCreateThread is not called
    Profile& S = g_Profile;
    uint32_t nPos = tfrg_atomic32_load_relaxed(&pLog->nPut);
    uint32_t nNextPos = (nPos + 1) % PROFILE_BUFFER_SIZE;
    if (nNextPos == tfrg_atomic32_load_relaxed(&pLog->nGet))
    {
        S.nOverflow = 100;
    }
    else
    {
        if (!pLog->Log)
        {
            pLog->Log = static_cast<ProfileLogEntry*>(tf_malloc(sizeof(ProfileLogEntry) * PROFILE_BUFFER_SIZE));
            memset(pLog->Log, 0, sizeof(ProfileLogEntry) * PROFILE_BUFFER_SIZE);
            S.nMemUsage += sizeof(ProfileLogEntry) * PROFILE_BUFFER_SIZE;
        }
        pLog->Log[nPos] = ProfileMakeLogIndex(nBegin, nToken_, nTick);
        tfrg_atomic32_store_release(&pLog->nPut, nNextPos);
    }
}

uint64_t cpuProfileEnter(ProfileToken nToken_)
{
    Profile& S = g_Profile;
    uint64_t nGroupMask = ProfileGetGroupMask(nToken_);
    if (nGroupMask & S.nActiveGroup)
    {
        if (ProfileThreadLog* pLog = ProfileGetOrCreateThreadLog())
        {
            uint64_t nTick = P_TICK();
            ProfileLogPut(nToken_, nTick, P_LOG_ENTER, pLog);
            return nTick;
        }
    }
    return PROFILE_INVALID_TICK;
}

PROFILE_API uint64_t ProfileEnterGpu(ProfileToken nToken_, uint64_t nTick, ProfileThreadLog* pLog)
{
    Profile& S = g_Profile;
    uint64_t nGroupMask = ProfileGetGroupMask(nToken_);
    if (nGroupMask & S.nActiveGroup)
    {
        if (nTick != (uint32_t)-1)
        {
            ProfileLogPut(nToken_, nTick, P_LOG_ENTER, pLog);
            return 0;
        }
    }
    return PROFILE_INVALID_TICK;
}

uint64_t ProfileAllocateLabel(const char* pName)
{
    Profile& S = g_Profile;
    char*    pLabelBuffer = (char*)tfrg_atomicptr_load_relaxed(&S.LabelBuffer);
    if (!pLabelBuffer)
    {
        MutexLock lock(ProfileMutex());

        pLabelBuffer = (char*)tfrg_atomicptr_load_relaxed(&S.LabelBuffer);
        if (!pLabelBuffer)
        {
            pLabelBuffer = static_cast<char*>(tf_malloc(PROFILE_LABEL_BUFFER_SIZE + PROFILE_LABEL_MAX_LEN));
            memset(pLabelBuffer, 0, PROFILE_LABEL_BUFFER_SIZE + PROFILE_LABEL_MAX_LEN);
            S.nMemUsage += PROFILE_LABEL_BUFFER_SIZE + PROFILE_LABEL_MAX_LEN;
            tfrg_atomic64_store_release(&S.LabelBuffer, *pLabelBuffer);
        }
    }

    size_t nLen = strlen(pName);

    if (nLen > PROFILE_LABEL_MAX_LEN - 1)
        nLen = PROFILE_LABEL_MAX_LEN - 1;

    uint64_t nLabel = tfrg_atomic64_add_relaxed(&S.nLabelPut, nLen + 1);
    char*    pLabel = &pLabelBuffer[nLabel % PROFILE_LABEL_BUFFER_SIZE];

    memcpy(pLabel, pName, nLen);
    pLabel[nLen] = 0;

    return nLabel;
}

void ProfilePutLabel(ProfileToken nToken_, const char* pName)
{
    // Profile & S = g_Profile;
    if (ProfileThreadLog* pLog = ProfileGetThreadLog())
    {
        uint64_t nLabel = ProfileAllocateLabel(pName);
        // uint64_t nGroupMask = ProfileGetGroupMask(nToken_);

        ProfileLogPut(nToken_, nLabel, P_LOG_LABEL, pLog);
    }
}

void ProfilePutLabelLiteral(ProfileToken nToken_, const char* pName)
{
    if (ProfileThreadLog* pLog = ProfileGetThreadLog())
    {
        // Profile & S = g_Profile;
        uint64_t nLabel = uint64_t(uintptr_t(pName));
        // uint64_t nGroupMask = ProfileGetGroupMask(nToken_);

        // We can only store 48 bits for each label pointer; for 32-bit platforms the entire pointer fits as is
        // For 64-bit platforms, most platforms use 48-bit addressing.
        // In some cases, like x64, the top 16 bits are 1 or 0 based on the bit 47.
        // In some other cases, like AArch64 (I think?), the top 16 bits are all 0 in user space, even if bit 47 is 1.
        // To avoid dealing with these complications, clear out the pointer if it doesn't fit in 48 bits without modifications.
        // This will automatically safely ignore pointers that don't fit.
        nLabel = (nLabel & P_LOG_TICK_MASK) == nLabel ? nLabel : 0;

        ProfileLogPut(nToken_, nLabel, P_LOG_LABEL_LITERAL, pLog);
    }
}

void ProfileCounterAdd(ProfileToken nToken, int64_t nCount)
{
    Profile& S = g_Profile;
    P_ASSERT(nToken < S.nNumCounters);
    tfrg_atomic64_add_relaxed(&S.Counters[nToken], nCount);
}
void ProfileCounterSet(ProfileToken nToken, int64_t nCount)
{
    Profile& S = g_Profile;
    P_ASSERT(nToken < S.nNumCounters);
    tfrg_atomic64_store_relaxed(&S.Counters[nToken], nCount);
}
void ProfileCounterSetLimit(ProfileToken nToken, int64_t nCount)
{
    Profile& S = g_Profile;
    P_ASSERT(nToken < S.nNumCounters);
    S.CounterInfo[nToken].nLimit = nCount;
}

void ProfileCounterConfig(const char* pName, uint32_t eFormat, int64_t nLimit, uint32_t nFlags)
{
    ProfileToken nToken = ProfileGetCounterToken(pName);
    Profile&     S = g_Profile;
    S.CounterInfo[nToken].eFormat = (ProfileCounterFormat)eFormat;
    S.CounterInfo[nToken].nLimit = nLimit;
    S.CounterInfo[nToken].nFlags |= (nFlags & ~PROFILE_COUNTER_FLAG_INTERNAL_MASK);
}

const char* ProfileGetLabel(uint32_t eType, uint64_t nLabel)
{
    P_ASSERT(eType == P_LOG_LABEL || eType == P_LOG_LABEL_LITERAL);
    P_ASSERT((nLabel & P_LOG_TICK_MASK) == nLabel);

    if (eType == P_LOG_LABEL_LITERAL)
        return (const char*)uintptr_t(nLabel);

    Profile& S = g_Profile;
    char*    pLabelBuffer = (char*)tfrg_atomicptr_load_relaxed(&S.LabelBuffer);
    uint64_t nLabelPut = tfrg_atomic64_load_relaxed(&S.nLabelPut);

    P_ASSERT(pLabelBuffer && nLabel < nLabelPut);

    if (nLabelPut - nLabel > PROFILE_LABEL_BUFFER_SIZE)
        return 0;
    else
        return &pLabelBuffer[nLabel % PROFILE_LABEL_BUFFER_SIZE];
}

void ProfileLabel(ProfileToken nToken_, const char* pName)
{
    Profile& S = g_Profile;
    if (ProfileGetGroupMask(nToken_) & S.nActiveGroup)
    {
        ProfilePutLabel(nToken_, pName);
    }
}

void ProfileLabelFormat(ProfileToken nToken_, const char* pName, ...)
{
    va_list args;
    va_start(args, pName);
    ProfileLabelFormatV(nToken_, pName, args);
    va_end(args);
}

void ProfileLabelFormatV(ProfileToken nToken_, const char* pName, va_list args)
{
    Profile& S = g_Profile;
    if (ProfileGetGroupMask(nToken_) & S.nActiveGroup)
    {
        char buffer[PROFILE_LABEL_MAX_LEN];
        vsnprintf(buffer, sizeof(buffer) - 1, pName, args);

        buffer[sizeof(buffer) - 1] = 0;

        ProfilePutLabel(nToken_, buffer);
    }
}

void ProfileLabelLiteral(ProfileToken nToken_, const char* pName)
{
    Profile& S = g_Profile;
    if (ProfileGetGroupMask(nToken_) & S.nActiveGroup)
    {
        ProfilePutLabelLiteral(nToken_, pName);
    }
}

void ProfileMetaUpdate(ProfileToken nToken, int nCount, ProfileTokenType eTokenType)
{
    UNREF_PARAM(eTokenType);
    Profile& S = g_Profile;
    if ((P_DRAW_META_FIRST << nToken) & S.nActiveBars)
    {
        if (ProfileThreadLog* pLog = ProfileGetThreadLog())
        {
            P_ASSERT(nToken < PROFILE_META_MAX);

            ProfileLogPut(nToken, nCount, P_LOG_META, pLog);
        }
    }
}

void cpuProfileLeave(ProfileToken nToken_, uint64_t nTickStart)
{
    if (PROFILE_INVALID_TICK != nTickStart)
    {
        if (ProfileThreadLog* pLog = ProfileGetOrCreateThreadLog())
        {
            uint64_t nTick = P_TICK();
            ProfileLogPut(nToken_, nTick, P_LOG_LEAVE, pLog);
        }
    }
}

PROFILE_API void ProfileLeaveGpu(ProfileToken nToken_, uint64_t nTick, ProfileThreadLog* pLog)
{
    if (PROFILE_INVALID_TOKEN != nToken_)
    {
        ProfileLogPut(nToken_, nTick, P_LOG_LEAVE, pLog);
    }
}

void ProfileContextSwitchPut(ProfileContextSwitch* pContextSwitch)
{
    Profile& S = g_Profile;
    if (S.nRunning || pContextSwitch->nTicks <= S.nPauseTicks)
    {
        uint32_t nPut = S.nContextSwitchPut;
        S.ContextSwitch[nPut] = *pContextSwitch;
        S.nContextSwitchPut = (S.nContextSwitchPut + 1) % PROFILE_CONTEXT_SWITCH_BUFFER_SIZE;
    }
}

void ProfileGetRange(uint32_t nPut, uint32_t nGet, uint32_t nRange[2][2])
{
    if (nPut > nGet)
    {
        nRange[0][0] = nGet;
        nRange[0][1] = nPut;
        nRange[1][0] = nRange[1][1] = 0;
    }
    else if (nPut != nGet)
    {
        P_ASSERT(nGet != PROFILE_BUFFER_SIZE);
        uint32_t nCountEnd = PROFILE_BUFFER_SIZE - nGet;
        nRange[0][0] = nGet;
        nRange[0][1] = nGet + nCountEnd;
        nRange[1][0] = 0;
        nRange[1][1] = nPut;
    }
}

void ProfileDumpToFile(Renderer* pRenderer);

void ProfileFlipCpu()
{
    MutexLock lock(ProfileMutex());

    Profile& S = g_Profile;

    if (S.nToggleRunning)
    {
        S.nRunning = !S.nRunning;
        if (!S.nRunning)
            S.nPauseTicks = P_TICK();
        S.nToggleRunning = 0;
        for (uint32_t i = 0; i < PROFILE_MAX_THREADS; ++i)
        {
            ProfileThreadLog* pLog = S.Pool[i];
            if (pLog)
            {
                pLog->nStackPos = 0;
            }
        }
    }
    uint32_t nAggregateClear = S.nAggregateClear || S.nAutoClearFrames, nAggregateFlip = 0;
    if (S.nDumpFileNextFrame)
    {
        ProfileDumpToFile(nullptr);
        S.nDumpFileNextFrame = 0;
        S.nAutoClearFrames = PROFILE_GPU_FRAME_DELAY + 3; // hide spike from dumping webpage
    }

    if (S.nAutoClearFrames)
    {
        nAggregateClear = 1;
        nAggregateFlip = 1;
        S.nAutoClearFrames -= 1;
    }

    if (S.nRunning || S.nForceEnable)
    {
        S.nFramePutIndex++;
        S.nFramePut = (S.nFramePut + 1) % PROFILE_MAX_FRAME_HISTORY;
        P_ASSERT((S.nFramePutIndex % PROFILE_MAX_FRAME_HISTORY) == S.nFramePut);
        S.nFrameCurrent = (S.nFramePut + PROFILE_MAX_FRAME_HISTORY - PROFILE_GPU_FRAME_DELAY - 1) % PROFILE_MAX_FRAME_HISTORY;
        S.nFrameCurrentIndex++;
        uint32_t nFrameNext = (S.nFrameCurrent + 1) % PROFILE_MAX_FRAME_HISTORY;

        uint32_t nContextSwitchPut = S.nContextSwitchPut;
        if (S.nContextSwitchLastPut < nContextSwitchPut)
        {
            S.nContextSwitchUsage = (nContextSwitchPut - S.nContextSwitchLastPut);
        }
        else
        {
            S.nContextSwitchUsage = PROFILE_CONTEXT_SWITCH_BUFFER_SIZE - S.nContextSwitchLastPut + nContextSwitchPut;
        }
        S.nContextSwitchLastPut = nContextSwitchPut;

        ProfileFrameState* pFramePut = &S.Frames[S.nFramePut];
        ProfileFrameState* pFrameCurrent = &S.Frames[S.nFrameCurrent];
        ProfileFrameState* pFrameNext = &S.Frames[nFrameNext];

        pFramePut->nFrameStartCpu = P_TICK();
        memset(&pFramePut->nFrameStartGpu[0], 0, PROFILE_MAX_THREADS * sizeof(pFramePut->nFrameStartGpu[0]));

        uint64_t nFrameStartCpu = pFrameCurrent->nFrameStartCpu;
        uint64_t nFrameEndCpu = pFrameNext->nFrameStartCpu;

        {
            uint64_t nTick = nFrameEndCpu - nFrameStartCpu;
            S.nFlipTicks = nTick;
            S.nFlipAggregate += nTick;
            S.nFlipMin = ProfileMin(S.nFlipMin, nTick);
            S.nFlipMax = ProfileMax(S.nFlipMax, nTick);
        }

        uint8_t* pTimerToGroup = &S.TimerToGroup[0];
        for (uint32_t i = 0; i < PROFILE_MAX_THREADS; ++i)
        {
            ProfileThreadLog* pLog = S.Pool[i];
            if (!pLog)
            {
                pFramePut->nLogStart[i] = 0;
            }
            else
            {
                uint32_t nPut = tfrg_atomic32_load_acquire(&pLog->nPut);
                pFramePut->nLogStart[i] = nPut;
                P_ASSERT(nPut < PROFILE_BUFFER_SIZE);
                if (pLog->nGpu && pLog->Log && pFramePut->nFrameStartGpu[i] == 0)
                {
                    uint32_t nPreviousPos = (nPut - 1) % PROFILE_BUFFER_SIZE;
                    pFramePut->nFrameStartGpu[i] = ProfileLogGetTick(pLog->Log[nPreviousPos]);
                }
                // need to keep last frame around to close timers. timers more than 1 frame old is ditched.
                tfrg_atomic32_store_relaxed(&pLog->nGet, nPut);
            }
        }

        if (S.nRunning)
        {
            uint64_t* pFrameGroup = &S.FrameGroup[0];
            {
                PROFILER_SET_CPU_SCOPE("Profile", "Clear", 0x3355ee);
                for (uint32_t i = 0; i < S.nTotalTimers; ++i)
                {
                    if (S.GroupInfo[S.TimerInfo[i].nGroupIndex].Type == ProfileTokenTypeGpu)
                    {
                        continue;
                    }
                    S.Frame[i].nTicks = 0;
                    S.Frame[i].nCount = 0;
                    S.FrameExclusive[i] = 0;
                }
                for (uint32_t i = 0; i < PROFILE_MAX_GROUPS; ++i)
                {
                    pFrameGroup[i] = 0;
                }
                for (uint32_t j = 0; j < PROFILE_META_MAX; ++j)
                {
                    if (S.MetaCounters[j].pName && 0 != (S.nActiveBars & (P_DRAW_META_FIRST << j)))
                    {
                        auto& Meta = S.MetaCounters[j];
                        for (uint32_t i = 0; i < S.nTotalTimers; ++i)
                        {
                            Meta.nCounters[i] = 0;
                        }
                    }
                }
            }
            {
                PROFILER_SET_CPU_SCOPE("Profile", "ThreadLoop", 0x3355ee);
                for (uint32_t i = 0; i < PROFILE_MAX_THREADS; ++i)
                {
                    ProfileThreadLog* pLog = S.Pool[i];
                    if (!pLog)
                        continue;

                    uint8_t* pGroupStackPos = &pLog->nGroupStackPos[0];
                    int64_t  nGroupTicks[PROFILE_MAX_GROUPS] = { 0 };

                    uint32_t nPut = pFrameNext->nLogStart[i];
                    uint32_t nGet = pFrameCurrent->nLogStart[i];
                    uint32_t nRange[2][2] = {
                        { 0, 0 },
                        { 0, 0 },
                    };
                    ProfileGetRange(nPut, nGet, nRange);

                    // fetch gpu results.
                    // if (pLog->nGpu)
                    //{
                    //	// Get current log's initial gpu tick
                    //	uint64_t nGPUTick = PROFILE_INVALID_TICK;
                    //	if (pFrameCurrent->nFrameStartGpuTimer != (uint32_t)-1)
                    //		nGPUTick = ProfileGpuGetTimeStamp(pLog, pFrameCurrent->nFrameStartGpuTimer);
                    //	uint64_t nLastTick = (nGPUTick == PROFILE_INVALID_TICK) ? pFrameCurrent->nFrameStartGpu : nGPUTick;
                    //
                    //	for (uint32_t j = 0; j < 2; ++j)
                    //	{
                    //		uint32_t nStart = nRange[j][0];
                    //		uint32_t nEnd = nRange[j][1];
                    //		for (uint32_t k = nStart; k < nEnd; ++k)
                    //		{
                    //			ProfileLogEntry L = pLog->Log[k];
                    //
                    //			uint64_t Type = ProfileLogType(L);
                    //
                    //			if (Type == P_LOG_ENTER || Type == P_LOG_LEAVE)
                    //			{
                    //				uint32_t nTimer = (uint32_t)ProfileLogGetTick(L);
                    //				uint64_t nTick = ProfileGpuGetTimeStamp(pLog, nTimer);
                    //
                    //				if (nTick != PROFILE_INVALID_TICK)
                    //					nLastTick = nTick;
                    //
                    //				pLog->Log[k] = ProfileLogSetTick(L, nLastTick);
                    //			}
                    //		}
                    //	}
                    // }

                    uint32_t* pStack = &pLog->nStack[0];
                    int64_t*  pChildTickStack = &pLog->nChildTickStack[0];
                    uint32_t  nStackPos = pLog->nStackPos;

                    for (uint32_t j = 0; j < 2; ++j)
                    {
                        uint32_t nStart = nRange[j][0];
                        uint32_t nEnd = nRange[j][1];
                        for (uint32_t k = nStart; k < nEnd; ++k)
                        {
                            ProfileLogEntry LE = pLog->Log[k];
                            uint64_t        nType = ProfileLogType(LE);

                            if (P_LOG_ENTER == nType)
                            {
                                uint64_t nTimer = ProfileLogTimerIndex(LE);
                                uint8_t  nGroup = pTimerToGroup[nTimer];
                                P_ASSERT(nStackPos < PROFILE_STACK_MAX);
                                P_ASSERT(nGroup < PROFILE_MAX_GROUPS);
                                pGroupStackPos[nGroup]++;
                                pStack[nStackPos] = k;
                                pChildTickStack[nStackPos] = 0;
                                nStackPos++;
                            }
                            else if (P_LOG_META == nType)
                            {
                                if (nStackPos)
                                {
                                    int64_t nMetaIndex = ProfileLogTimerIndex(LE);
                                    int64_t nMetaCount = ProfileLogGetTick(LE);
                                    P_ASSERT(nMetaIndex < PROFILE_META_MAX);
                                    int64_t nCounter = ProfileLogTimerIndex(pLog->Log[pStack[nStackPos - 1]]);
                                    S.MetaCounters[nMetaIndex].nCounters[nCounter] += nMetaCount;
                                }
                            }
                            else if (P_LOG_LEAVE == nType)
                            {
                                uint64_t nTimer = ProfileLogTimerIndex(LE);
                                uint8_t  nGroup = pTimerToGroup[nTimer];
                                P_ASSERT(nGroup < PROFILE_MAX_GROUPS);
                                if (nStackPos)
                                {
                                    int64_t nTickStart = pLog->Log[pStack[nStackPos - 1]];
                                    int64_t nTicks = ProfileLogTickDifference(nTickStart, LE);
                                    int64_t nChildTicks = pChildTickStack[nStackPos];
                                    nStackPos--;
                                    pChildTickStack[nStackPos] += nTicks;

                                    if (!pLog->nGpu)
                                    {
                                        uint32_t nTimerIndex = (uint32_t)ProfileLogTimerIndex(LE);
                                        S.Frame[nTimerIndex].nTicks += nTicks;
                                        S.FrameExclusive[nTimerIndex] += (nTicks - nChildTicks);
                                        S.Frame[nTimerIndex].nCount += 1;
                                    }
                                    uint8_t nGroupStackPos = pGroupStackPos[nGroup];
                                    if (nGroupStackPos)
                                    {
                                        nGroupStackPos--;
                                        if (0 == nGroupStackPos)
                                        {
                                            nGroupTicks[nGroup] += nTicks;
                                        }
                                        pGroupStackPos[nGroup] = nGroupStackPos;
                                    }
                                }
                            }
                        }
                    }
                    for (uint32_t k = 0; k < PROFILE_MAX_GROUPS; ++k)
                    {
                        pLog->nGroupTicks[k] += nGroupTicks[k];
                        pFrameGroup[k] += nGroupTicks[k];
                    }
                    pLog->nStackPos = nStackPos;
                }
            }
            {
                PROFILER_SET_CPU_SCOPE("Profile", "Accumulate", 0x3355ee);
                for (uint32_t i = 0; i < S.nTotalTimers; ++i)
                {
                    if (S.GroupInfo[S.TimerInfo[i].nGroupIndex].Type == ProfileTokenTypeGpu)
                    {
                        continue;
                    }

                    S.AccumTimers[i].nTicks += S.Frame[i].nTicks;
                    S.AccumTimers[i].nCount += S.Frame[i].nCount;
                    S.AccumMaxTimers[i] = ProfileMax(S.AccumMaxTimers[i], S.Frame[i].nTicks);
                    S.AccumMinTimers[i] = ProfileMin(S.AccumMinTimers[i], S.Frame[i].nTicks);
                    S.AccumTimersExclusive[i] += S.FrameExclusive[i];
                    S.AccumMaxTimersExclusive[i] = ProfileMax(S.AccumMaxTimersExclusive[i], S.FrameExclusive[i]);
                }

                for (uint32_t i = 0; i < PROFILE_MAX_GROUPS; ++i)
                {
                    S.AccumGroup[i] += pFrameGroup[i];
                    S.AccumGroupMax[i] = ProfileMax(S.AccumGroupMax[i], pFrameGroup[i]);
                }

                for (uint32_t j = 0; j < PROFILE_META_MAX; ++j)
                {
                    if (S.MetaCounters[j].pName && 0 != (S.nActiveBars & (P_DRAW_META_FIRST << j)))
                    {
                        auto&    Meta = S.MetaCounters[j];
                        uint64_t nSum = 0;
                        ;
                        for (uint32_t i = 0; i < S.nTotalTimers; ++i)
                        {
                            uint64_t nCounter = Meta.nCounters[i];
                            Meta.nAccumMax[i] = ProfileMax(Meta.nAccumMax[i], nCounter);
                            Meta.nAccum[i] += nCounter;
                            nSum += nCounter;
                        }
                        Meta.nSumAccum += nSum;
                        Meta.nSumAccumMax = ProfileMax(Meta.nSumAccumMax, nSum);
                    }
                }
            }
            for (uint32_t i = 0; i < PROFILE_MAX_GRAPHS; ++i)
            {
                if (S.Graph[i].nToken != PROFILE_INVALID_TOKEN)
                {
                    ProfileToken nToken = S.Graph[i].nToken;
                    S.Graph[i].nHistory[S.nGraphPut] = S.Frame[ProfileGetTimerIndex(nToken)].nTicks;
                }
            }
            S.nGraphPut = (S.nGraphPut + 1) % PROFILE_GRAPH_HISTORY;
        }

        if (S.nRunning && S.nAggregateFlip <= ++S.nAggregateFlipCount)
        {
            nAggregateFlip = 1;
            if (S.nAggregateFlip) // if 0 accumulate indefinitely
            {
                nAggregateClear = 1;
            }
        }
    }
    if (nAggregateFlip)
    {
        memcpy(&S.Aggregate[0], &S.AccumTimers[0], sizeof(S.Aggregate[0]) * S.nTotalTimers);
        memcpy(&S.AggregateMax[0], &S.AccumMaxTimers[0], sizeof(S.AggregateMax[0]) * S.nTotalTimers);
        memcpy(&S.AggregateMin[0], &S.AccumMinTimers[0], sizeof(S.AggregateMin[0]) * S.nTotalTimers);
        memcpy(&S.AggregateExclusive[0], &S.AccumTimersExclusive[0], sizeof(S.AggregateExclusive[0]) * S.nTotalTimers);
        memcpy(&S.AggregateMaxExclusive[0], &S.AccumMaxTimersExclusive[0], sizeof(S.AggregateMaxExclusive[0]) * S.nTotalTimers);

        memcpy(&S.AggregateGroup[0], &S.AccumGroup[0], sizeof(S.AggregateGroup));
        memcpy(&S.AggregateGroupMax[0], &S.AccumGroupMax[0], sizeof(S.AggregateGroup));

        for (uint32_t i = 0; i < PROFILE_MAX_THREADS; ++i)
        {
            ProfileThreadLog* pLog = S.Pool[i];
            if (!pLog)
                continue;

            memcpy(&pLog->nAggregateGroupTicks[0], &pLog->nGroupTicks[0], sizeof(pLog->nAggregateGroupTicks));

            if (nAggregateClear)
            {
                memset(&pLog->nGroupTicks[0], 0, sizeof(pLog->nGroupTicks));
            }
        }

        for (uint32_t j = 0; j < PROFILE_META_MAX; ++j)
        {
            if (S.MetaCounters[j].pName && 0 != (S.nActiveBars & (P_DRAW_META_FIRST << j)))
            {
                auto& Meta = S.MetaCounters[j];
                memcpy(&Meta.nAggregateMax[0], &Meta.nAccumMax[0], sizeof(Meta.nAggregateMax[0]) * S.nTotalTimers);
                memcpy(&Meta.nAggregate[0], &Meta.nAccum[0], sizeof(Meta.nAggregate[0]) * S.nTotalTimers);
                Meta.nSumAggregate = Meta.nSumAccum;
                Meta.nSumAggregateMax = Meta.nSumAccumMax;
                if (nAggregateClear)
                {
                    memset(&Meta.nAccumMax[0], 0, sizeof(Meta.nAccumMax[0]) * S.nTotalTimers);
                    memset(&Meta.nAccum[0], 0, sizeof(Meta.nAccum[0]) * S.nTotalTimers);
                    Meta.nSumAccum = 0;
                    Meta.nSumAccumMax = 0;
                }
            }
        }

        S.nAggregateFrames = S.nAggregateFlipCount;
        S.nFlipAggregateDisplay = S.nFlipAggregate;
        S.nFlipMaxDisplay = S.nFlipMax;
        S.nFlipMinDisplay = S.nFlipMin;
        if (nAggregateClear)
        {
            memset(&S.AccumTimers[0], 0, sizeof(S.Aggregate[0]) * S.nTotalTimers);
            memset(&S.AccumMaxTimers[0], 0, sizeof(S.AccumMaxTimers[0]) * S.nTotalTimers);
            memset(&S.AccumMinTimers[0], 0xFF, sizeof(S.AccumMinTimers[0]) * S.nTotalTimers);
            memset(&S.AccumTimersExclusive[0], 0, sizeof(S.AggregateExclusive[0]) * S.nTotalTimers);
            memset(&S.AccumMaxTimersExclusive[0], 0, sizeof(S.AccumMaxTimersExclusive[0]) * S.nTotalTimers);
            memset(&S.AccumGroup[0], 0, sizeof(S.AggregateGroup));
            memset(&S.AccumGroupMax[0], 0, sizeof(S.AggregateGroup));

            S.nAggregateFlipCount = 0;
            S.nFlipAggregate = 0;
            S.nFlipMax = 0;
            S.nFlipMin = (uint64_t)-1;

            S.nAggregateFlipTick = P_TICK();
        }

#if PROFILE_COUNTER_HISTORY
        int64_t* pDest = &S.nCounterHistory[S.nCounterHistoryPut][0];
        S.nCounterHistoryPut = (S.nCounterHistoryPut + 1) % PROFILE_GRAPH_HISTORY;
        for (uint32_t i = 0; i < S.nNumCounters; ++i)
        {
            if (0 != (S.CounterInfo[i].nFlags & PROFILE_COUNTER_FLAG_DETAILED))
            {
                uint64_t nValue = tfrg_atomic64_load_relaxed(&S.Counters[i]);
                pDest[i] = nValue;
                S.nCounterMin[i] = ProfileMin(S.nCounterMin[i], (int64_t)nValue);
                S.nCounterMax[i] = ProfileMax(S.nCounterMax[i], (int64_t)nValue);
            }
        }
#endif
    }
    S.nAggregateClear = 0;

    uint64_t nNewActiveGroup = 0;
    if (S.nRunning || S.nForceEnable)
        nNewActiveGroup = S.nAllGroupsWanted ? S.nGroupMask : S.nActiveGroupWanted;
    nNewActiveGroup |= S.nForceEnableGroup;
    nNewActiveGroup |= S.nForceGroupUI;
    nNewActiveGroup &= ~S.nForceDisableGroup;
    if (S.nActiveGroup != nNewActiveGroup)
        S.nActiveGroup = nNewActiveGroup;

    uint32_t nNewActiveBars = 0;
    if (S.nRunning || S.nForceEnable)
        nNewActiveBars = S.nBars;
    if (S.nForceMetaCounters)
    {
        for (int i = 0; i < PROFILE_META_MAX; ++i)
        {
            if (S.MetaCounters[i].pName)
            {
                nNewActiveBars |= (P_DRAW_META_FIRST << i);
            }
        }
    }
    if (nNewActiveBars != S.nActiveBars)
        S.nActiveBars = nNewActiveBars;
}

void flipProfiler()
{
    PROFILER_SET_CPU_SCOPE("Profile", "ProfileFlip", 0x3355ee);

    ProfileFlipCpu();
}

void ProfileSetForceEnable(bool bEnable)
{
    Profile& S = g_Profile;
    S.nForceEnable = bEnable ? 1 : 0;
}
bool ProfileGetForceEnable()
{
    Profile& S = g_Profile;
    return S.nForceEnable != 0;
}

void ProfileSetEnableAllGroups(bool bEnableAllGroups)
{
    Profile& S = g_Profile;
    S.nAllGroupsWanted = bEnableAllGroups ? 1 : 0;
}

void ProfileEnableCategory(const char* pCategory, bool bEnabled)
{
    int      nCategoryIndex = -1;
    Profile& S = g_Profile;
    for (uint32_t i = 0; i < S.nCategoryCount; ++i)
    {
        if (!P_STRCASECMP(pCategory, S.CategoryInfo[i].pName))
        {
            nCategoryIndex = (int)i;
            break;
        }
    }
    if (nCategoryIndex >= 0)
    {
        if (bEnabled)
        {
            S.nActiveGroupWanted |= S.CategoryInfo[nCategoryIndex].nGroupMask;
        }
        else
        {
            S.nActiveGroupWanted &= ~S.CategoryInfo[nCategoryIndex].nGroupMask;
        }
    }
}

void ProfileEnableCategory(const char* pCategory) { ProfileEnableCategory(pCategory, true); }
void ProfileDisableCategory(const char* pCategory) { ProfileEnableCategory(pCategory, false); }

bool ProfileGetEnableAllGroups()
{
    Profile& S = g_Profile;
    return 0 != S.nAllGroupsWanted;
}

void ProfileSetForceMetaCounters(bool bForce)
{
    Profile& S = g_Profile;
    S.nForceMetaCounters = bForce ? 1 : 0;
}

bool ProfileGetForceMetaCounters()
{
    Profile& S = g_Profile;
    return 0 != S.nForceMetaCounters;
}

void ProfileEnableMetaCounter(const char* pMeta)
{
    Profile& S = g_Profile;
    for (uint32_t i = 0; i < PROFILE_META_MAX; ++i)
    {
        if (S.MetaCounters[i].pName && 0 == P_STRCASECMP(S.MetaCounters[i].pName, pMeta))
        {
            S.nBars |= (P_DRAW_META_FIRST << i);
            return;
        }
    }
}
void ProfileDisableMetaCounter(const char* pMeta)
{
    Profile& S = g_Profile;
    for (uint32_t i = 0; i < PROFILE_META_MAX; ++i)
    {
        if (S.MetaCounters[i].pName && 0 == P_STRCASECMP(S.MetaCounters[i].pName, pMeta))
        {
            S.nBars &= ~(P_DRAW_META_FIRST << i);
            return;
        }
    }
}

void setAggregateFrames(uint32_t nFrames)
{
    Profile& S = g_Profile;
    S.nAggregateFlip = (uint32_t)nFrames;
    if (0 == nFrames)
    {
        S.nAggregateClear = 1;
    }
}

int ProfileGetAggregateFrames()
{
    Profile& S = g_Profile;
    return S.nAggregateFlip;
}

int ProfileGetCurrentAggregateFrames()
{
    Profile& S = g_Profile;
    return int(S.nAggregateFlip ? S.nAggregateFlip : S.nAggregateFlipCount);
}

void ProfileForceEnableGroup(const char* pGroup, ProfileTokenType Type)
{
    ProfileInit();
    Profile&  S = g_Profile;
    MutexLock lock(ProfileMutex());
    uint16_t  nGroup = ProfileGetGroup(pGroup, Type);
    S.nForceEnableGroup |= (1ll << nGroup);
}

void ProfileForceDisableGroup(const char* pGroup, ProfileTokenType Type)
{
    ProfileInit();
    Profile&  S = g_Profile;
    MutexLock lock(ProfileMutex());
    uint16_t  nGroup = ProfileGetGroup(pGroup, Type);
    S.nForceDisableGroup |= (1ll << nGroup);
}

void ProfileCalcAllTimers(float* pTimers, float* pAverage, float* pMax, float* pMin, float* pCallAverage, float* pExclusive,
                          float* pAverageExclusive, float* pMaxExclusive, float* pTotal, uint32_t nSize)
{
    Profile& S = g_Profile;
    for (uint32_t i = 0; i < S.nTotalTimers && i < nSize; ++i)
    {
        const uint32_t nGroupId = S.TimerInfo[i].nGroupIndex;
        const float    fToMs = ProfileTickToMsMultiplier(S.GroupInfo[nGroupId].Type == ProfileTokenTypeGpu
                                                             ? getGpuProfileTicksPerSecond(S.GroupInfo[nGroupId].nGpuProfileToken)
                                                             : ProfileTicksPerSecondCpu());
        uint32_t       nTimer = i;
        uint32_t       nIdx = i * 2;
        uint32_t       nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;
        uint32_t       nAggregateCount = S.Aggregate[nTimer].nCount ? S.Aggregate[nTimer].nCount : 1;
        float          fToPrc = S.fRcpReferenceTime;
        float          fMs = fToMs * (S.Frame[nTimer].nTicks);
        float          fPrc = ProfileMin(fMs * fToPrc, 1.f);
        float          fAverageMs = fToMs * (float)(S.Aggregate[nTimer].nTicks / nAggregateFrames);
        float          fAveragePrc = ProfileMin(fAverageMs * fToPrc, 1.f);
        float          fMaxMs = fToMs * (S.AggregateMax[nTimer]);
        float          fMaxPrc = ProfileMin(fMaxMs * fToPrc, 1.f);
        float          fMinMs = fToMs * (S.AggregateMin[nTimer] != uint64_t(-1) ? S.AggregateMin[nTimer] : 0);
        float          fMinPrc = ProfileMin(fMinMs * fToPrc, 1.f);
        float          fCallAverageMs = fToMs * (float)(S.Aggregate[nTimer].nTicks / nAggregateCount);
        float          fCallAveragePrc = ProfileMin(fCallAverageMs * fToPrc, 1.f);
        float          fMsExclusive = fToMs * (S.FrameExclusive[nTimer]);
        float          fPrcExclusive = ProfileMin(fMsExclusive * fToPrc, 1.f);
        float          fAverageMsExclusive = fToMs * (float)(S.AggregateExclusive[nTimer] / nAggregateFrames);
        float          fAveragePrcExclusive = ProfileMin(fAverageMsExclusive * fToPrc, 1.f);
        float          fMaxMsExclusive = fToMs * (S.AggregateMaxExclusive[nTimer]);
        float          fMaxPrcExclusive = ProfileMin(fMaxMsExclusive * fToPrc, 1.f);
        float          fTotalMs = fToMs * S.Aggregate[nTimer].nTicks;
        pTimers[nIdx] = fMs;
        pTimers[nIdx + 1] = fPrc;
        pAverage[nIdx] = fAverageMs;
        pAverage[nIdx + 1] = fAveragePrc;
        pMax[nIdx] = fMaxMs;
        pMax[nIdx + 1] = fMaxPrc;
        pMin[nIdx] = fMinMs;
        pMin[nIdx + 1] = fMinPrc;
        pCallAverage[nIdx] = fCallAverageMs;
        pCallAverage[nIdx + 1] = fCallAveragePrc;
        pExclusive[nIdx] = fMsExclusive;
        pExclusive[nIdx + 1] = fPrcExclusive;
        pAverageExclusive[nIdx] = fAverageMsExclusive;
        pAverageExclusive[nIdx + 1] = fAveragePrcExclusive;
        pMaxExclusive[nIdx] = fMaxMsExclusive;
        pMaxExclusive[nIdx + 1] = fMaxPrcExclusive;
        pTotal[nIdx] = fTotalMs;
        pTotal[nIdx + 1] = 0.f;
    }
}

void ProfileTogglePause()
{
    Profile& S = g_Profile;
    S.nToggleRunning = 1;
}

float getCpuProfileMinTime(const char* pGroup, const char* pName, ThreadID* pThreadID)
{
    ProfileToken nToken = ProfileFindToken(pGroup, pName, pThreadID);
    if (nToken == PROFILE_INVALID_TOKEN)
    {
        return 0.f;
    }
    Profile& S = g_Profile;
    uint32_t nTimerIndex = ProfileGetTimerIndex(nToken);
    float    fToMs = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu());
    return fToMs * (S.AggregateMin[nTimerIndex] != uint64_t(-1) ? S.AggregateMin[nTimerIndex] : 0);
}

float getCpuProfileMaxTime(const char* pGroup, const char* pName, ThreadID* pThreadID)
{
    ProfileToken nToken = ProfileFindToken(pGroup, pName, pThreadID);
    if (nToken == PROFILE_INVALID_TOKEN)
    {
        return 0.f;
    }
    Profile& S = g_Profile;
    uint32_t nTimerIndex = ProfileGetTimerIndex(nToken);
    float    fToMs = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu());
    return fToMs * (S.AggregateMax[nTimerIndex]);
}

float getCpuProfileAvgTime(const char* pGroup, const char* pName, ThreadID* pThreadID)
{
    ProfileToken nToken = ProfileFindToken(pGroup, pName, pThreadID);
    if (nToken == PROFILE_INVALID_TOKEN)
    {
        return 0.f;
    }
    Profile& S = g_Profile;
    uint32_t nTimerIndex = ProfileGetTimerIndex(nToken);
    uint32_t nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;
    float    fToMs = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu());
    return fToMs * (float)(S.Aggregate[nTimerIndex].nTicks / nAggregateFrames);
}

float getCpuProfileTime(const char* pGroup, const char* pName, ThreadID* pThreadID)
{
    ProfileToken nToken = ProfileFindToken(pGroup, pName, pThreadID);
    if (nToken == PROFILE_INVALID_TOKEN)
    {
        return 0.f;
    }
    Profile& S = g_Profile;
    uint32_t nTimerIndex = ProfileGetTimerIndex(nToken);
    float    fToMs = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu());
    return S.Frame[nTimerIndex].nTicks * fToMs;
}

float getCpuMinFrameTime()
{
    float    fToMs = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu());
    Profile& S = g_Profile;
    return fToMs * (S.nFlipMinDisplay);
}

float getCpuMaxFrameTime()
{
    float    fToMs = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu());
    Profile& S = g_Profile;
    return fToMs * (S.nFlipMaxDisplay);
}

float getCpuAvgFrameTime()
{
    float    fToMs = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu());
    Profile& S = g_Profile;
    uint32_t nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;
    return fToMs * (float)(S.nFlipAggregateDisplay / nAggregateFrames);
}

float getCpuFrameTime()
{
    float    fToMs = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu());
    Profile& S = g_Profile;
    return fToMs * (S.nFlipTicks);
}

int ProfileFormatCounter(int eFormat, int64_t nCounter, char* pOut, uint32_t nBufferSize)
{
    if (!nCounter)
    {
        pOut[0] = '0';
        pOut[1] = '\0';
        return 1;
    }
    int   nLen = 0;
    char* pBase = pOut;
    char* pTmp = pOut;
    char* pEnd = pOut + nBufferSize;

    switch (eFormat)
    {
    case PROFILE_COUNTER_FORMAT_DEFAULT:
    {
        int nNegative = 0;
        if (nCounter < 0)
        {
            nCounter = -nCounter;
            nNegative = 1;
        }
        int nSeperate = 0;
        while (nCounter)
        {
            if (nSeperate)
            {
                *pTmp++ = ' ';
            }
            nSeperate = 1;
            for (uint32_t i = 0; nCounter && i < 3; ++i)
            {
                int nDigit = nCounter % 10;
                nCounter /= 10;
                *pTmp++ = (char)('0' + nDigit);
            }
        }
        if (nNegative)
        {
            *pTmp++ = '-';
        }
        nLen = int(pTmp - pOut);
        --pTmp;
        P_ASSERT(pTmp <= pEnd);
        while (pTmp > pOut) // reverse string
        {
            char c = *pTmp;
            *pTmp = *pOut;
            *pOut = c;
            pTmp--;
            pOut++;
        }
    }
    break;
    case PROFILE_COUNTER_FORMAT_BYTES:
    {
        const char* pExt[] = { "b", "kb", "mb", "gb", "tb", "pb", "eb", "zb", "yb" };
        size_t      nNumExt = sizeof(pExt) / sizeof(pExt[0]);
        int64_t     nShift = 0;
        int64_t     nDivisor = 1;
        int64_t     nCountShifted = nCounter >> 10;
        while (nCountShifted)
        {
            nDivisor <<= 10;
            nCountShifted >>= 10;
            nShift++;
        }
        P_ASSERT(nShift < (int64_t)nNumExt);
        if (nShift)
        {
            snprintf(pOut, nBufferSize - 1, "%3.2f%s", (double)nCounter / nDivisor, pExt[nShift]);
        }
        else
        {
            snprintf(pOut, nBufferSize - 1, "%lld%s", (long long)nCounter, pExt[nShift]);
        }
        nLen = (int)strlen(pOut);
    }
    break;
    }
    pBase[nLen] = '\0';

    return nLen;
}

void ProfileDumpFile(const char* pDumpFile, ProfileDumpType eType, uint32_t nFrames)
{
    Profile& S = g_Profile;

    S.DumpFile = pDumpFile;
    S.nDumpFileNextFrame = 1;
    S.eDumpType = eType;
    S.nDumpFrames = nFrames;
}

PROFILE_FORMAT(3, 4) void ProfilePrintf(ProfileWriteCallback CB, void* Handle, const char* pFmt, ...)
{
    char    buffer[4096];
    va_list args;
    va_start(args, pFmt);
#if defined(_WINDOWS) || defined(XBOX)
    size_t size = vsprintf_s(buffer, pFmt, args);
#else
    size_t size = vsnprintf(buffer, sizeof(buffer) - 1, pFmt, args);
#endif
    CB(Handle, size, &buffer[0]);
    va_end(args);
}

void ProfilePrintUIntComma(ProfileWriteCallback CB, void* Handle, uint64_t nData)
{
    char Buffer[32];

    uint32_t nOffset = sizeof(Buffer);
    Buffer[--nOffset] = ',';

    if (nData < 10)
    {
        Buffer[--nOffset] = (char)((int)'0' + nData);
    }
    else
    {
        do
        {
            Buffer[--nOffset] = "0123456789abcdef"[nData & 0xf];
            nData >>= 4;
        } while (nData);

        Buffer[--nOffset] = 'x';
        Buffer[--nOffset] = '0';
    }

    CB(Handle, sizeof(Buffer) - nOffset, &Buffer[nOffset]);
}

void ProfilePrintString(ProfileWriteCallback CB, void* Handle, const char* pData) { CB(Handle, strlen(pData), pData); }

void ProfileDumpCsv(ProfileWriteCallback CB, void* Handle, int nMaxFrames)
{
    (void)nMaxFrames;

    Profile& S = g_Profile;
    uint32_t nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;
    float    fToMsCPU = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu());

    ProfilePrintf(CB, Handle, "frames,%d\n", nAggregateFrames);
    ProfilePrintf(CB, Handle, "group,name,average,max,callaverage\n");

    uint32_t nNumTimers = S.nTotalTimers;
    uint32_t nBlockSize = 2 * nNumTimers;
    float*   pTimers = (float*)alloca(nBlockSize * 9 * sizeof(float));
    float*   pAverage = pTimers + nBlockSize;
    float*   pMax = pTimers + 2 * nBlockSize;
    float*   pMin = pTimers + 3 * nBlockSize;
    float*   pCallAverage = pTimers + 4 * nBlockSize;
    float*   pTimersExclusive = pTimers + 5 * nBlockSize;
    float*   pAverageExclusive = pTimers + 6 * nBlockSize;
    float*   pMaxExclusive = pTimers + 7 * nBlockSize;
    float*   pTotal = pTimers + 8 * nBlockSize;

    ProfileCalcAllTimers(pTimers, pAverage, pMax, pMin, pCallAverage, pTimersExclusive, pAverageExclusive, pMaxExclusive, pTotal,
                         nNumTimers);

    for (uint32_t i = 0; i < S.nTotalTimers; ++i)
    {
        uint32_t nIdx = i * 2;
        ProfilePrintf(CB, Handle, "\"%s\",\"%s\",%f,%f,%f\n", S.TimerInfo[i].pName, S.GroupInfo[S.TimerInfo[i].nGroupIndex].pName,
                      pAverage[nIdx], pMax[nIdx], pCallAverage[nIdx]);
    }

    ProfilePrintf(CB, Handle, "\n\n");

    ProfilePrintf(CB, Handle, "group,average,max,total\n");
    for (uint32_t j = 0; j < PROFILE_MAX_GROUPS; ++j)
    {
        const char* pGroupName = S.GroupInfo[j].pName;
        float       fToMs = S.GroupInfo[j].Type == ProfileTokenTypeGpu
                                ? ProfileTickToMsMultiplier(getGpuProfileTicksPerSecond(S.GroupInfo[j].nGpuProfileToken))
                                : fToMsCPU;
        if (pGroupName[0] != '\0')
        {
            ProfilePrintf(CB, Handle, "\"%s\",%.3f,%.3f,%.3f\n", pGroupName, fToMs * S.AggregateGroup[j] / nAggregateFrames,
                          fToMs * S.AggregateGroup[j] / nAggregateFrames, fToMs * S.AggregateGroup[j]);
        }
    }

    ProfilePrintf(CB, Handle, "\n\n");
    ProfilePrintf(CB, Handle, "group,thread,average,total\n");
    for (uint32_t j = 0; j < PROFILE_MAX_GROUPS; ++j)
    {
        for (uint32_t i = 0; i < PROFILE_MAX_THREADS; ++i)
        {
            if (S.Pool[i])
            {
                const char* pThreadName = &S.Pool[i]->ThreadName[0];
                // ProfilePrintf(CB, Handle, "var ThreadGroupTime%d = [", i);
                float       fToMs =
                    S.Pool[i]->nGpu ? ProfileTickToMsMultiplier(getGpuProfileTicksPerSecond(S.GroupInfo[j].nGpuProfileToken)) : fToMsCPU;
                {
                    uint64_t nTicks = S.Pool[i]->nAggregateGroupTicks[j];
                    float    fTime = (float)(nTicks / nAggregateFrames) * fToMs;
                    float    fTimeTotal = nTicks * fToMs;
                    if (fTimeTotal > 0.01f)
                    {
                        const char* pGroupName = S.GroupInfo[j].pName;
                        ProfilePrintf(CB, Handle, "\"%s\",\"%s\",%.3f,%.3f\n", pGroupName, pThreadName, fTime, fTimeTotal);
                    }
                }
            }
        }
    }

    ProfilePrintf(CB, Handle, "\n\n");
    ProfilePrintf(CB, Handle, "frametimecpu\n");

    const uint32_t nCount = PROFILE_MAX_FRAME_HISTORY - PROFILE_GPU_FRAME_DELAY - 3;
    const uint32_t nStart = S.nFrameCurrent;
    for (uint32_t i = nCount; i > 0; i--)
    {
        uint32_t nFrame = (nStart + PROFILE_MAX_FRAME_HISTORY - i) % PROFILE_MAX_FRAME_HISTORY;
        uint32_t nFrameNext = (nStart + PROFILE_MAX_FRAME_HISTORY - i + 1) % PROFILE_MAX_FRAME_HISTORY;
        uint64_t nTicks = S.Frames[nFrameNext].nFrameStartCpu - S.Frames[nFrame].nFrameStartCpu;
        ProfilePrintf(CB, Handle, "%f,", nTicks * fToMsCPU);
    }
    ProfilePrintf(CB, Handle, "\n");

    ProfilePrintf(CB, Handle, "\n\n");
    ProfilePrintf(CB, Handle, "frametimegpu\n");

    for (uint32_t i = nCount; i > 0; i--)
    {
        uint32_t nFrame = (nStart + PROFILE_MAX_FRAME_HISTORY - i) % PROFILE_MAX_FRAME_HISTORY;
        uint32_t nFrameNext = (nStart + PROFILE_MAX_FRAME_HISTORY - i + 1) % PROFILE_MAX_FRAME_HISTORY;
        uint64_t nTicks = S.Frames[nFrameNext].nFrameStartGpu - S.Frames[nFrame].nFrameStartGpu;
        ProfilePrintf(CB, Handle, "%" PRIu64 ",", nTicks);
    }
    ProfilePrintf(CB, Handle, "\n\n");
    ProfilePrintf(CB, Handle, "Meta\n"); // only single frame snapshot
    ProfilePrintf(CB, Handle, "name,average,max,total\n");
    for (int j = 0; j < PROFILE_META_MAX; ++j)
    {
        if (S.MetaCounters[j].pName)
        {
            ProfilePrintf(CB, Handle, "\"%s\",%f,%lld,%lld\n", S.MetaCounters[j].pName,
                          S.MetaCounters[j].nSumAggregate / (float)nAggregateFrames, (long long)S.MetaCounters[j].nSumAggregateMax,
                          (long long)S.MetaCounters[j].nSumAggregate);
        }
    }
}

#if PROFILE_EMBED_HTML
extern const char* g_ProfileHtml_begin[];
extern size_t      g_ProfileHtml_begin_sizes[];
extern size_t      g_ProfileHtml_begin_count;
extern const char* g_ProfileHtml_end[];
extern size_t      g_ProfileHtml_end_sizes[];
extern size_t      g_ProfileHtml_end_count;

static bool utilCompareIndices(const void* pvLhs, const void* pvRhs, void* pvArr)
{
    uint32_t* pArr = (uint32_t*)pvArr;
    uint32_t  lhs = *(uint32_t*)pvLhs;
    uint32_t  rhs = *(uint32_t*)pvRhs;

    const uint32_t len = (uint32_t)arrlen(pArr);
    (void)len;
    ASSERT(lhs < len);
    ASSERT(rhs < len);
    return (int)((int64_t)pArr[lhs] - (int64_t)pArr[rhs]);
}

void ProfileDumpHtml(ProfileWriteCallback CB, void* Handle, int nMaxFrames, const char* pHost)
{
    Profile& S = g_Profile;
    uint32_t nRunning = S.nRunning;
    S.nRunning = 0;

    // stall pushing of timers
    uint64_t nActiveGroup = S.nActiveGroup;
    S.nActiveGroup = 0;
    S.nPauseTicks = P_TICK();

    for (size_t i = 0; i < g_ProfileHtml_begin_count; ++i)
    {
        CB(Handle, g_ProfileHtml_begin_sizes[i] - 1, g_ProfileHtml_begin[i]);
    }

    // dump info
    if (g_Profile.pGpuSettings != NULL)
    {
        ProfilePrintf(CB, Handle, "var GpuName = '%s';\n", g_Profile.pGpuSettings->mGpuVendorPreset.mGpuName);
        ProfilePrintf(CB, Handle, "var VendorID = '%#x';\n", g_Profile.pGpuSettings->mGpuVendorPreset.mVendorId);
        ProfilePrintf(CB, Handle, "var ModelID = '%#x';\n", g_Profile.pGpuSettings->mGpuVendorPreset.mModelId);
    }

    uint64_t nTicks = P_TICK();

    float fToMsCPU = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu());
    float fAggregateMs = fToMsCPU * (nTicks - S.nAggregateFlipTick);
    ProfilePrintf(CB, Handle, "var DumpHost = '%s';\n", pHost ? pHost : "");
    time_t CaptureTime;
    time(&CaptureTime);
    ProfilePrintf(CB, Handle, "var DumpUtcCaptureTime = %ld;\n", CaptureTime);
    ProfilePrintf(CB, Handle, "var AggregateInfo = {'Frames':%d, 'Time':%f};\n", S.nAggregateFrames, fAggregateMs);

    // categories
    ProfilePrintf(CB, Handle, "var CategoryInfo = Array(%d);\n", S.nCategoryCount);
    for (uint32_t i = 0; i < S.nCategoryCount; ++i)
    {
        ProfilePrintf(CB, Handle, "CategoryInfo[%d] = \"%s\";\n", i, S.CategoryInfo[i].pName);
    }

    // groups
    ProfilePrintf(CB, Handle, "var GroupInfo = Array(%d);\n\n", S.nGroupCount);
    uint32_t nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;

    for (uint32_t i = 0; i < S.nGroupCount; ++i)
    {
        P_ASSERT(i == S.GroupInfo[i].nGroupIndex);
        float    fToMs = S.GroupInfo[i].Type == ProfileTokenTypeCpu
                             ? fToMsCPU
                             : ProfileTickToMsMultiplier(getGpuProfileTicksPerSecond(S.GroupInfo[i].nGpuProfileToken));
        uint32_t nColor = S.TimerInfo[i].nColor;
        ProfilePrintf(CB, Handle, "GroupInfo[%d] = MakeGroup(%d, \"%s\", %d, %d, %d, %f, %f, %f, '#%06x');\n", S.GroupInfo[i].nGroupIndex,
                      S.GroupInfo[i].nGroupIndex, S.GroupInfo[i].pName, S.GroupInfo[i].nCategory, S.GroupInfo[i].nNumTimers,
                      S.GroupInfo[i].Type == ProfileTokenTypeGpu ? 1 : 0, fToMs * S.AggregateGroup[i],
                      fToMs * S.AggregateGroup[i] / nAggregateFrames, fToMs * S.AggregateGroupMax[i],
                      ((PROFILE_UNPACK_RED(nColor) & 0xff) << 16) | ((PROFILE_UNPACK_GREEN(nColor) & 0xff) << 8) |
                          (PROFILE_UNPACK_BLUE(nColor) & 0xff));
    }
    // timers

    uint32_t nNumTimers = S.nTotalTimers;
    uint32_t nBlockSize = 2 * nNumTimers;
    float*   pTimers = (float*)tf_calloc(nBlockSize * 9, sizeof(float));
    float*   pAverage = pTimers + nBlockSize;
    float*   pMax = pTimers + 2 * nBlockSize;
    float*   pMin = pTimers + 3 * nBlockSize;
    float*   pCallAverage = pTimers + 4 * nBlockSize;
    float*   pTimersExclusive = pTimers + 5 * nBlockSize;
    float*   pAverageExclusive = pTimers + 6 * nBlockSize;
    float*   pMaxExclusive = pTimers + 7 * nBlockSize;
    float*   pTotal = pTimers + 8 * nBlockSize;

    ProfileCalcAllTimers(pTimers, pAverage, pMax, pMin, pCallAverage, pTimersExclusive, pAverageExclusive, pMaxExclusive, pTotal,
                         nNumTimers);

    ProfilePrintf(CB, Handle, "\nvar TimerInfo = Array(%d);\n\n", S.nTotalTimers);
    for (uint32_t i = 0; i < S.nTotalTimers; ++i)
    {
        uint32_t nIdx = i * 2;
        P_ASSERT(i == S.TimerInfo[i].nTimerIndex);

        uint32_t nColor = S.TimerInfo[i].nColor;
        uint32_t nColorDark = (nColor >> 1) & ~0x80808080;
        ProfilePrintf(CB, Handle, "TimerInfo[%d] = MakeTimer(%d, \"%s\", %d, '#%06x','#%06x', %f, %f, %f, %f, %f, %f, %d, %f,\n",
                      S.TimerInfo[i].nTimerIndex, S.TimerInfo[i].nTimerIndex, S.TimerInfo[i].pName, S.TimerInfo[i].nGroupIndex,
                      ((PROFILE_UNPACK_RED(nColor) & 0xff) << 16) | ((PROFILE_UNPACK_GREEN(nColor) & 0xff) << 8) |
                          (PROFILE_UNPACK_BLUE(nColor) & 0xff),
                      ((PROFILE_UNPACK_RED(nColorDark) & 0xff) << 16) | ((PROFILE_UNPACK_GREEN(nColorDark) & 0xff) << 8) |
                          (PROFILE_UNPACK_BLUE(nColorDark) & 0xff),
                      pAverage[nIdx], pMax[nIdx], pMin[nIdx], pAverageExclusive[nIdx], pMaxExclusive[nIdx], pCallAverage[nIdx],
                      S.Aggregate[i].nCount, pTotal[nIdx]);

        ProfilePrintString(CB, Handle, "\t[");
        for (int j = 0; j < PROFILE_META_MAX; ++j)
        {
            if (S.MetaCounters[j].pName)
            {
                ProfilePrintUIntComma(CB, Handle, S.MetaCounters[j].nCounters[i]);
            }
        }
        ProfilePrintString(CB, Handle, "],[");
        for (int j = 0; j < PROFILE_META_MAX; ++j)
        {
            if (S.MetaCounters[j].pName)
            {
                ProfilePrintUIntComma(CB, Handle, S.MetaCounters[j].nAggregate[i]);
            }
        }
        ProfilePrintString(CB, Handle, "],[");
        for (int j = 0; j < PROFILE_META_MAX; ++j)
        {
            if (S.MetaCounters[j].pName)
            {
                ProfilePrintUIntComma(CB, Handle, S.MetaCounters[j].nAggregateMax[i]);
            }
        }
        ProfilePrintString(CB, Handle, "]);\n");
    }

    ProfilePrintString(CB, Handle, "\nvar ThreadNames = [");
    for (uint32_t i = 0; i < PROFILE_MAX_THREADS; ++i)
    {
        if (!S.Pool[i])
            continue;
        ProfilePrintf(CB, Handle, "'%s',", S.Pool[i]->ThreadName);
    }
    ProfilePrintString(CB, Handle, "];\n\n");

    ProfilePrintString(CB, Handle, "\nvar ThreadIds = [");
    for (uint32_t i = 0; i < PROFILE_MAX_THREADS; ++i)
    {
        if (!S.Pool[i])
            continue;
        ProfilePrintUIntComma(CB, Handle, i);
    }
    ProfilePrintString(CB, Handle, "];\n\n");

    ProfilePrintString(CB, Handle, "\nvar ThreadGpu = [");
    for (uint32_t i = 0; i < PROFILE_MAX_THREADS; ++i)
    {
        if (!S.Pool[i])
            continue;
        ProfilePrintUIntComma(CB, Handle, S.Pool[i]->nGpu);
    }
    ProfilePrintString(CB, Handle, "];\n\n");

    ProfilePrintString(CB, Handle, "\nvar ThreadGroupTimeArray = [\n");
    for (uint32_t i = 0; i < PROFILE_MAX_THREADS; ++i)
    {
        if (!S.Pool[i])
            continue;
        float fToMs = S.Pool[i]->nGpu ? ProfileTickToMsMultiplier(getGpuProfileTicksPerSecond(S.Pool[i]->nGpuToken)) : fToMsCPU;
        ProfilePrintf(CB, Handle, "MakeTimes(%e,[", fToMs);
        for (uint32_t j = 0; j < PROFILE_MAX_GROUPS; ++j)
        {
            ProfilePrintUIntComma(CB, Handle, S.Pool[i]->nAggregateGroupTicks[j]);
        }
        ProfilePrintString(CB, Handle, "]),\n");
    }
    ProfilePrintString(CB, Handle, "];");

    ProfilePrintString(CB, Handle, "\nvar MetaNames = [");
    for (int i = 0; i < PROFILE_META_MAX; ++i)
    {
        if (S.MetaCounters[i].pName)
        {
            ProfilePrintf(CB, Handle, "'%s',", S.MetaCounters[i].pName);
        }
    }
    ProfilePrintString(CB, Handle, "];\n\n");

    ProfilePrintString(CB, Handle, "\nvar CounterInfo = [");
    for (uint32_t i = 0; i < S.nNumCounters; ++i)
    {
        int64_t nCounter = tfrg_atomic64_load_relaxed(&S.Counters[i]);
        int64_t nLimit = S.CounterInfo[i].nLimit;
        float   fCounterPrc = 0.f;
        float   fBoxPrc = 1.f;
        if (nLimit)
        {
            fCounterPrc = (float)nCounter / nLimit;
            if (fCounterPrc > 1.f)
            {
                fBoxPrc = 1.f / fCounterPrc;
                fCounterPrc = 1.f;
            }
        }

        int64_t nCounterMin = 0, nCounterMax = 0;

#if PROFILE_COUNTER_HISTORY
        nCounterMin = S.nCounterMin[i];
        nCounterMax = S.nCounterMax[i];
#endif

        char Formatted[64];
        char FormattedLimit[64];
        ProfileFormatCounter(S.CounterInfo[i].eFormat, nCounter, Formatted, sizeof(Formatted) - 1);
        ProfileFormatCounter(S.CounterInfo[i].eFormat, S.CounterInfo[i].nLimit, FormattedLimit, sizeof(FormattedLimit) - 1);
        ProfilePrintf(CB, Handle, "MakeCounter(%d, %d, %d, %d, %d, '%s', %lld, %lld, %lld, '%s', %lld, '%s', %d, %f, %f, [", i,
                      S.CounterInfo[i].nParent, S.CounterInfo[i].nSibling, S.CounterInfo[i].nFirstChild, S.CounterInfo[i].nLevel,
                      S.CounterInfo[i].pName, (long long)nCounter, (long long)nCounterMin, (long long)nCounterMax, Formatted,
                      (long long)nLimit, FormattedLimit, S.CounterInfo[i].eFormat == PROFILE_COUNTER_FORMAT_BYTES ? 1 : 0, fCounterPrc,
                      fBoxPrc);

#if PROFILE_COUNTER_HISTORY
        if (0 != (S.CounterInfo[i].nFlags & PROFILE_COUNTER_FLAG_DETAILED))
        {
            uint32_t nBaseIndex = S.nCounterHistoryPut;
            for (uint32_t j = 0; j < PROFILE_GRAPH_HISTORY; ++j)
            {
                uint32_t nHistoryIndex = (nBaseIndex + j) % PROFILE_GRAPH_HISTORY;
                int64_t  nValue = ProfileClamp(S.nCounterHistory[nHistoryIndex][i], nCounterMin, nCounterMax);
                ProfilePrintUIntComma(CB, Handle, nValue - nCounterMin);
            }
        }
#endif

        ProfilePrintString(CB, Handle, "]),\n");
    }
    ProfilePrintString(CB, Handle, "];\n\n");

    uint32_t nNumFrames = (PROFILE_MAX_FRAME_HISTORY - PROFILE_GPU_FRAME_DELAY - 3); // leave a few to not overwrite
    nNumFrames = ProfileMin(nNumFrames, (uint32_t)nMaxFrames);

    const uint32_t nFirstFrame = (S.nFrameCurrent + PROFILE_MAX_FRAME_HISTORY - nNumFrames) % PROFILE_MAX_FRAME_HISTORY;
    uint32_t       nLastFrame = (nFirstFrame + nNumFrames) % PROFILE_MAX_FRAME_HISTORY;
    P_ASSERT(nLastFrame == (S.nFrameCurrent % PROFILE_MAX_FRAME_HISTORY));
    const int64_t nTickStart = S.Frames[nFirstFrame].nFrameStartCpu;
    const int64_t nTickEnd = S.Frames[nLastFrame].nFrameStartCpu;
    const int64_t nTickStartGpu = S.Frames[nFirstFrame].nFrameStartGpu[0];

    int64_t nTicksPerSecondCpu = ProfileTicksPerSecondCpu();
    int64_t nTicksPerSecondGpu = 0;

#if PROFILE_DEBUG
    printf("dumping %d frames\n", nNumFrames);
    printf("dumping frame %d to %d\n", nFirstFrame, nLastFrame);
#endif

    uint32_t* nTimerCounter = NULL;
    arrsetlen(nTimerCounter, S.nTotalTimers);
    if (S.nTotalTimers)
        memset(nTimerCounter, 0, sizeof(uint32_t) * S.nTotalTimers);

    ProfilePrintf(CB, Handle, "var Frames = Array(%d);\n", nNumFrames);
    for (uint32_t i = 0; i < nNumFrames; ++i)
    {
        uint32_t nFrameIndex = (nFirstFrame + i) % PROFILE_MAX_FRAME_HISTORY;
        uint32_t nFrameIndexNext = (nFrameIndex + 1) % PROFILE_MAX_FRAME_HISTORY;

        ProfilePrintf(CB, Handle, "var tt%d = [\n", i);
        for (uint32_t j = 0; j < PROFILE_MAX_THREADS; ++j)
        {
            if (!S.Pool[j])
                continue;
            ProfileThreadLog* pLog = S.Pool[j];
            uint32_t          nLogStart = S.Frames[nFrameIndex].nLogStart[j];
            uint32_t          nLogEnd = S.Frames[nFrameIndexNext].nLogStart[j];

            ProfilePrintString(CB, Handle, "[");
            for (uint32_t k = nLogStart; k != nLogEnd; k = (k + 1) % PROFILE_BUFFER_SIZE)
            {
                uint32_t nLogType = (uint32_t)ProfileLogType(pLog->Log[k]);
                if (nLogType == P_LOG_META)
                {
                    // for meta, store the count + 8, which is the tick part
                    nLogType = uint32_t(8 + ProfileLogGetTick(pLog->Log[k]));
                }
                if (nLogType == P_LOG_LABEL_LITERAL)
                {
                    // for label literals, pretend that they are stored as labels; HTML dump doesn't support efficent label literal storage
                    // yet
                    nLogType = P_LOG_LABEL;
                }
                ProfilePrintUIntComma(CB, Handle, nLogType);
            }
            ProfilePrintString(CB, Handle, "],\n");
        }
        ProfilePrintString(CB, Handle, "];\n");

        ProfilePrintf(CB, Handle, "var ts%d = [\n", i);
        for (uint32_t j = 0; j < PROFILE_MAX_THREADS; ++j)
        {
            if (!S.Pool[j])
                continue;
            ProfileThreadLog* pLog = S.Pool[j];
            uint32_t          nLogStart = S.Frames[nFrameIndex].nLogStart[j];
            uint32_t          nLogEnd = S.Frames[nFrameIndexNext].nLogStart[j];

            int64_t nStartTick = pLog->nGpu ? S.Frames[nFirstFrame].nFrameStartGpu[j] : nTickStart;
            float   fToMs = pLog->nGpu ? ProfileTickToMsMultiplier(getGpuProfileTicksPerSecond(pLog->nGpuToken)) : fToMsCPU;

            // if (pLog->nGpu)
            //	ProfilePrintf(CB, Handle, "MakeTimesExtra(%e,%e,tt%d[%d],[", fToMs, fToMsCPU, i, j);
            // else
            ProfilePrintf(CB, Handle, "MakeTimes(%e,[", fToMs);
            for (uint32_t k = nLogStart; k != nLogEnd; k = (k + 1) % PROFILE_BUFFER_SIZE)
            {
                uint32_t nLogType = (uint32_t)ProfileLogType(pLog->Log[k]);
                uint64_t nTick = (nLogType == P_LOG_ENTER || nLogType == P_LOG_LEAVE) ? ProfileLogTickDifference(nStartTick, pLog->Log[k])
                                 : (nLogType == P_LOG_GPU_EXTRA)                      ? ProfileLogTickDifference(nStartTick, pLog->Log[k])
                                                                                      : 0;
                ProfilePrintUIntComma(CB, Handle, nTick);
            }
            ProfilePrintString(CB, Handle, "]),\n");
        }
        ProfilePrintString(CB, Handle, "];\n");

        ProfilePrintf(CB, Handle, "var ti%d = [\n", i);
        for (uint32_t j = 0; j < PROFILE_MAX_THREADS; ++j)
        {
            if (!S.Pool[j])
                continue;
            ProfileThreadLog* pLog = S.Pool[j];
            uint32_t          nLogStart = S.Frames[nFrameIndex].nLogStart[j];
            uint32_t          nLogEnd = S.Frames[nFrameIndexNext].nLogStart[j];

            uint32_t nLabelIndex = 0;
            ProfilePrintString(CB, Handle, "[");
            for (uint32_t k = nLogStart; k != nLogEnd; k = (k + 1) % PROFILE_BUFFER_SIZE)
            {
                uint32_t nLogType = (uint32_t)ProfileLogType(pLog->Log[k]);
                uint32_t nTimerIndex = (uint32_t)ProfileLogTimerIndex(pLog->Log[k]);
                uint32_t nIndex = (nLogType == P_LOG_LABEL || nLogType == P_LOG_LABEL_LITERAL) ? nLabelIndex++ : nTimerIndex;
                ProfilePrintUIntComma(CB, Handle, nIndex);

                if (nLogType == P_LOG_ENTER && VERIFY(nTimerCounter))
                    nTimerCounter[nTimerIndex]++;
            }
            ProfilePrintString(CB, Handle, "],\n");
        }
        ProfilePrintString(CB, Handle, "];\n");

        ProfilePrintf(CB, Handle, "var tl%d = [\n", i);
        for (uint32_t j = 0; j < PROFILE_MAX_THREADS; ++j)
        {
            if (!S.Pool[j])
                continue;
            ProfileThreadLog* pLog = S.Pool[j];
            if (nTicksPerSecondGpu == 0 && pLog->nGpu) // Set nTicksPerSecondsGpu on first gpu profiler found
            {
                nTicksPerSecondGpu = getGpuProfileTicksPerSecond(pLog->nGpuToken);
            }
            uint32_t nLogStart = S.Frames[nFrameIndex].nLogStart[j];
            uint32_t nLogEnd = S.Frames[nFrameIndexNext].nLogStart[j];

            ProfilePrintString(CB, Handle, "[");
            for (uint32_t k = nLogStart; k != nLogEnd; k = (k + 1) % PROFILE_BUFFER_SIZE)
            {
                uint32_t nLogType = (uint32_t)ProfileLogType(pLog->Log[k]);
                if (nLogType == P_LOG_LABEL || nLogType == P_LOG_LABEL_LITERAL)
                {
                    uint64_t    nLabel = ProfileLogGetTick(pLog->Log[k]);
                    const char* pLabelName = ProfileGetLabel(nLogType, nLabel);

                    if (pLabelName)
                    {
                        ProfilePrintString(CB, Handle, "\"");
                        ProfilePrintString(CB, Handle, pLabelName);
                        ProfilePrintString(CB, Handle, "\",");
                    }
                    else
                        ProfilePrintString(CB, Handle, "null,");
                }
            }
            ProfilePrintString(CB, Handle, "],\n");
        }
        ProfilePrintString(CB, Handle, "];\n");

        int64_t nFrameStart = S.Frames[nFrameIndex].nFrameStartCpu;
        int64_t nFrameEnd = S.Frames[nFrameIndexNext].nFrameStartCpu;

        // Get largest frame window over all gpu profilers
        int64_t nFrameStartGpu = nFrameEnd;
        int64_t nFrameEndGpu = nFrameStart;
        for (uint32_t nGpuStart = 0; nGpuStart < PROFILE_MAX_THREADS; ++nGpuStart)
        {
            if (S.Pool[nGpuStart] && S.Pool[nGpuStart]->nGpu)
            {
                nFrameStartGpu = min(nFrameStartGpu, S.Frames[nFrameIndex].nFrameStartGpu[nGpuStart]);
                nFrameEndGpu = max(nFrameEndGpu, S.Frames[nFrameIndexNext].nFrameStartGpu[nGpuStart]);
            }
        }

        float fToMs = ProfileTickToMsMultiplier(nTicksPerSecondCpu);
        float fToMsGPU = ProfileTickToMsMultiplier(nTicksPerSecondGpu);
        float fFrameMs = ProfileLogTickDifference(nTickStart, nFrameStart) * fToMs;
        float fFrameEndMs = ProfileLogTickDifference(nTickStart, nFrameEnd) * fToMs;
        float fFrameGpuMs = ProfileLogTickDifference(nTickStartGpu, nFrameStartGpu) * fToMsGPU;
        float fFrameGpuEndMs = ProfileLogTickDifference(nTickStartGpu, nFrameEndGpu) * fToMsGPU;

        ProfilePrintf(CB, Handle, "Frames[%d] = MakeFrame(%d, %f, %f, %f, %f, ts%d, tt%d, ti%d, tl%d);\n", i, 0, fFrameMs, fFrameEndMs,
                      fFrameGpuMs, fFrameGpuEndMs, i, i, i, i);
    }

    uint32_t nContextSwitchStart = 0;
    uint32_t nContextSwitchEnd = 0;
    ProfileContextSwitchSearch(&nContextSwitchStart, &nContextSwitchEnd, nTickStart, nTickEnd);

    ProfilePrintString(CB, Handle, "var CSwitchThreadInOutCpu = [\n");
    for (uint32_t j = nContextSwitchStart; j != nContextSwitchEnd; j = (j + 1) % PROFILE_CONTEXT_SWITCH_BUFFER_SIZE)
    {
        ProfileContextSwitch CS = S.ContextSwitch[j];
        int                  nCpu = CS.nCpu;
        ProfilePrintUIntComma(CB, Handle, 0);
        ProfilePrintUIntComma(CB, Handle, 0);
        ProfilePrintUIntComma(CB, Handle, nCpu);
    }
    ProfilePrintString(CB, Handle, "];\n");

    ProfilePrintString(CB, Handle, "var CSwitchTime = [\n");
    float fToMsCpu = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu());
    for (uint32_t j = nContextSwitchStart; j != nContextSwitchEnd; j = (j + 1) % PROFILE_CONTEXT_SWITCH_BUFFER_SIZE)
    {
        ProfileContextSwitch CS = S.ContextSwitch[j];
        float                fTime = ProfileLogTickDifference(nTickStart, CS.nTicks) * fToMsCpu;
        ProfilePrintf(CB, Handle, "%f,", fTime);
    }
    ProfilePrintString(CB, Handle, "];\n");

    ProfileThreadInfo Threads[PROFILE_MAX_CONTEXT_SWITCH_THREADS];
    uint32_t          nNumThreadsBase = 0;
    uint32_t          nNumThreads = ProfileContextSwitchGatherThreads(nContextSwitchStart, nContextSwitchEnd, Threads, &nNumThreadsBase);

    ProfilePrintString(CB, Handle, "var CSwitchThreads = {");

    for (uint32_t i = 0; i < nNumThreads; ++i)
    {
        char        Name[256];
        const char* pProcessName = ProfileGetProcessName(Threads[i].nProcessId, Name, sizeof(Name));

        const char* p1 = i < nNumThreadsBase && S.Pool[i] ? S.Pool[i]->ThreadName : "?";
        const char* p2 = pProcessName ? pProcessName : "?";

        ProfilePrintf(CB, Handle, "%lld:{\'tid\':%lld,\'pid\':%lld,\'t\':\'%s\',\'p\':\'%s\'},", (long long)Threads[i].nThreadId,
                      (long long)Threads[i].nThreadId, (long long)Threads[i].nProcessId, p1, p2);
    }

    ProfilePrintString(CB, Handle, "};\n");

    for (size_t i = 0; i < g_ProfileHtml_end_count; ++i)
    {
        CB(Handle, g_ProfileHtml_end_sizes[i] - 1, g_ProfileHtml_end[i]);
    }

    uint32_t* nGroupCounter = NULL;
    arrsetlen(nGroupCounter, S.nGroupCount);
    if (S.nGroupCount)
        memset(nGroupCounter, 0, sizeof(uint32_t) * S.nGroupCount);
    for (uint32_t i = 0; i < S.nTotalTimers; ++i)
    {
        uint32_t nGroupIndex = S.TimerInfo[i].nGroupIndex;
        nGroupCounter[nGroupIndex] += nTimerCounter[i];
    }

    uint32_t* nGroupCounterSort = NULL;
    arrsetlen(nGroupCounterSort, S.nGroupCount);
    uint32_t* nTimerCounterSort = NULL;
    arrsetlen(nTimerCounterSort, S.nTotalTimers);
    for (ptrdiff_t i = 0; i < arrlen(nGroupCounterSort); ++i)
    {
        nGroupCounterSort[i] = (uint32_t)i;
    }
    for (ptrdiff_t i = 0; i < arrlen(nTimerCounterSort); ++i)
    {
        nTimerCounterSort[i] = (uint32_t)i;
    }

    sort(nGroupCounterSort, arrlenu(nGroupCounterSort), sizeof(nGroupCounterSort[0]), utilCompareIndices, nGroupCounter);
    sort(nTimerCounterSort, arrlenu(nTimerCounterSort), sizeof(nTimerCounterSort[0]), utilCompareIndices, nTimerCounter);

    ProfilePrintf(CB, Handle, "\n<!--\nMarker Per Group\n");
    for (uint32_t i = 0; i < S.nGroupCount; ++i)
    {
        uint32_t idx = nGroupCounterSort[i]; //-V595
        ASSERT(idx < arrlen(nGroupCounter));
        ProfilePrintf(CB, Handle, "%8d:%s\n", nGroupCounter[idx], S.GroupInfo[idx].pName); //-V595
    }
    ProfilePrintf(CB, Handle, "Marker Per Timer\n");
    for (uint32_t i = 0; i < S.nTotalTimers; ++i)
    {
        uint32_t idx = nTimerCounterSort[i]; //-V595
        ASSERT(idx < arrlen(nTimerCounter));
        ProfilePrintf(CB, Handle, "%8d:%s(%s)\n", nTimerCounter[idx], S.TimerInfo[idx].pName,
                      S.GroupInfo[S.TimerInfo[idx].nGroupIndex].pName); //-V595
    }
    ProfilePrintf(CB, Handle, "\n-->\n");

    S.nActiveGroup = nActiveGroup;
    S.nRunning = nRunning;

    arrfree(nTimerCounterSort);
    arrfree(nGroupCounterSort);
    arrfree(nGroupCounter);
    arrfree(nTimerCounter);
    tf_free(pTimers);

#if PROFILE_DEBUG
    int64_t nTicksEnd = P_TICK();
    float   fMs = fToMsCpu * (nTicksEnd - S.nPauseTicks);
    printf("html dump took %6.2fms\n", fMs);
#endif
}
#else
void ProfileDumpHtml(ProfileWriteCallback CB, void* Handle, int nMaxFrames, const char* pHost)
{
    ProfilePrintString(CB, Handle, "HTML output is disabled because PROFILE_EMBED_HTML is 0\n");
}
#endif

struct ProfileWriteFileData
{
    bstring    mBuffer;
    FileStream mStream;
};

static void ProfileWriteFileFlush(ProfileWriteFileData* data)
{
    const size_t length = (size_t)blength(&data->mBuffer);
    if (length > 0)
    {
        const char* bufData = bdata(&data->mBuffer);
        VERIFY(fsWriteToStream(&data->mStream, bufData, length) == length);
        btrunc(&data->mBuffer, 0); // Clear the buffer
    }
}

void ProfileWriteFile(void* Handle, size_t nSize, const char* pData)
{
    ProfileWriteFileData* data = (ProfileWriteFileData*)Handle;
    bcatblk(&data->mBuffer, pData, (int)nSize);

    if (blength(&data->mBuffer) > 4096)
    {
        // Buffer is sufficiently big, flush it to disk
        ProfileWriteFileFlush(data);
    }
}

void ProfileDumpToFile(Renderer* pRenderer)
{
    UNREF_PARAM(pRenderer);
    if (!g_Profile.nRunning)
        return;

    MutexLock lock(ProfileMutex());
    Profile&  S = g_Profile;

    ProfileWriteFileData data = {};
    data.mBuffer = bempty();
    if (fsOpenStreamFromPath(RD_LOG, S.DumpFile, FM_WRITE, &data.mStream))
    {
        if (S.eDumpType == ProfileDumpTypeHtml)
            ProfileDumpHtml(ProfileWriteFile, &data, S.nDumpFrames, 0);
        else if (S.eDumpType == ProfileDumpTypeCsv)
            ProfileDumpCsv(ProfileWriteFile, &data, S.nDumpFrames);

        ProfileWriteFileFlush(&data);
        fsCloseStream(&data.mStream);
    }
    bdestroy(&data.mBuffer);
}

void dumpProfileData(const char* appName, uint32_t nMaxFrames)
{
    if (!g_Profile.nRunning)
        return;

    MutexLock lock(ProfileMutex());

    // Dump frames to file.
    time_t t = time(0);

    char   time[64];
    size_t timeLen = strftime(time, sizeof(time), R"(Profile-%Y-%m-%d-%H.%M.%S.html)", localtime(&t));
    ASSERT(timeLen < 64);

    char name[256] = {};
    snprintf(name, sizeof(name), "%s%s", appName, time);
    ProfileWriteFileData data = {};
    data.mBuffer = bempty();
    if (fsOpenStreamFromPath(RD_LOG, name, FM_WRITE, &data.mStream))
    {
        ProfileDumpHtml(ProfileWriteFile, &data, nMaxFrames, 0);
        ProfileWriteFileFlush(&data);
        fsCloseStream(&data.mStream);
    }
    bdestroy(&data.mBuffer);
}

void dumpBenchmarkData(IApp::Settings* pSettings, const char* outFilename, const char* appName)
{
    if (!g_Profile.nRunning)
        return;

    time_t        t = time(0);
    unsigned char nameBuf[256];
    bstring       name = bemptyfromarr(nameBuf);
    bformat(&name, "%s", outFilename);

    char   time[64];
    // bstrlib doesn't have strftime
    size_t timeLen = strftime(time, sizeof(time), R"(Benchmark-%Y-%m-%d-%H.%M.%S.profile)", localtime(&t));
    ASSERT(timeLen < 64);
    bcatblk(&name, time, (int)timeLen);

    FileStream statsFile = {};
    if (fsOpenStreamFromPath(RD_LOG, (const char*)name.data, FM_WRITE, &statsFile))
    {
        bstring output = bempty();
        balloc(&output, 2048);

        bassignliteral(&output, "{\n");

        bformata(&output, "\"Application\": \"%s\", \n", appName);
        bformata(&output, "\"Width\": %d, \n", pSettings->mWidth);
        bformata(&output, "\"Height\": %d, \n\n", pSettings->mHeight);
        bformata(&output, "\"GpuName\": \"%s\", \n", g_Profile.pGpuSettings->mGpuVendorPreset.mGpuName);
        bformata(&output, "\"VendorID\": \"%#x\", \n", g_Profile.pGpuSettings->mGpuVendorPreset.mVendorId);
        bformata(&output, "\"ModelID\": \"%#x\", \n\n", g_Profile.pGpuSettings->mGpuVendorPreset.mModelId);

        const Profile& S = *ProfileGet();
        for (uint32_t groupIndex = 0; groupIndex < S.nGroupCount; ++groupIndex)
        {
            if (S.GroupInfo[groupIndex].Type != ProfileTokenTypeGpu)
                continue;

            for (uint32_t timerIndex = 0; timerIndex < S.nTotalTimers; ++timerIndex)
            {
                if (strcmp(S.TimerInfo[timerIndex].pName, S.GroupInfo[groupIndex].pName) == 0)
                {
                    bformata(&output, "\"%s\": { \n", S.GroupInfo[groupIndex].pName);

                    float    fToMs = ProfileTickToMsMultiplier(getGpuProfileTicksPerSecond(S.GroupInfo[groupIndex].nGpuProfileToken));
                    uint32_t nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;
                    uint32_t nAggregateCount = S.Aggregate[timerIndex].nCount ? S.Aggregate[timerIndex].nCount : 1;
                    float    fAverage = fToMs * (float)(S.Aggregate[timerIndex].nTicks / nAggregateFrames);
                    float    fMax = fToMs * (S.AggregateMax[timerIndex]);
                    float    fMin = fToMs * (S.AggregateMin[timerIndex]);

                    bformata(&output, "\"Average\": %0.4f, \n", fAverage);
                    bformata(&output, "\"Min\": %0.4f, \n", fMin);
                    bformata(&output, "\"Max\": %0.4f, \n", fMax);
                    bformata(&output, "\"Frames\": %d \n", nAggregateCount);
                    bformata(&output, "}, \n\n");
                    break;
                }
            }
        }

        bformata(&output, "\"Cpu\": { \n");
        bformata(&output, "\"Average\": %0.4f, \n", getCpuAvgFrameTime());
        bformata(&output, "\"Min\": %0.4f, \n", getCpuMinFrameTime());
        bformata(&output, "\"Max\": %0.4f, \n", getCpuMaxFrameTime());
        bformata(&output, "\"Frames\": %d \n", S.nAggregateFrames);
        bformata(&output, "} \n");
        bformata(&output, "}");

        fsWriteToStream(&statsFile, (const char*)output.data, (size_t)output.slen * sizeof(char));
        fsCloseStream(&statsFile);
        bdestroy(&output);
    }
}

#ifdef ENABLE_PROFILER_WEBSERVER
uint32_t ProfileWebServerPort()
{
    Profile& S = g_Profile;
    return S.nWebServerPort;
}

void ProfileSendSocket(MpSocket Socket, const char* pData, size_t nSize)
{
#ifdef MSG_NOSIGNAL
    int nFlags = MSG_NOSIGNAL;
#else
    int                          nFlags = 0;
#endif

    send(Socket, pData, (int)nSize, nFlags);
}

void ProfileFlushSocket(MpSocket Socket)
{
    Profile& S = g_Profile;
    if (S.nWebServerPut)
    {
        ProfileSendSocket(Socket, &S.WebServerBuffer[0], S.nWebServerPut);
        S.nWebServerPut = 0;
    }
}

void ProfileWriteSocket(void* Handle, size_t nSize, const char* pData)
{
    Profile& S = g_Profile;
    MpSocket Socket = *(MpSocket*)Handle;
    if (nSize > PROFILE_WEBSERVER_SOCKET_BUFFER_SIZE / 2)
    {
        ProfileFlushSocket(Socket);
        ProfileSendSocket(Socket, pData, nSize);
    }
    else
    {
        memcpy(&S.WebServerBuffer[S.nWebServerPut], pData, nSize);
        S.nWebServerPut += (uint32_t)nSize;
        if (S.nWebServerPut > PROFILE_WEBSERVER_SOCKET_BUFFER_SIZE / 2)
        {
            ProfileFlushSocket(Socket);
        }
    }

    S.nWebServerDataSent += nSize;
}

#if PROFILE_MINIZ
#ifndef PROFILE_COMPRESS_BUFFER_SIZE
#define PROFILE_COMPRESS_BUFFER_SIZE (256 << 10)
#endif

#define PROFILE_COMPRESS_CHUNK (PROFILE_COMPRESS_BUFFER_SIZE / 2)
struct ProfileCompressedSocketState
{
    unsigned char DeflateOut[PROFILE_COMPRESS_CHUNK];
    unsigned char DeflateIn[PROFILE_COMPRESS_CHUNK];
    mz_stream     Stream;
    MpSocket      Socket;
    uint32_t      nSize;
    uint32_t      nCompressedSize;
    uint32_t      nFlushes;
    uint32_t      nMemmoveBytes;
};

void ProfileCompressedSocketFlush(ProfileCompressedSocketState* pState)
{
    mz_stream&     Stream = pState->Stream;
    unsigned char* pSendStart = &pState->DeflateOut[0];
    unsigned char* pSendEnd = &pState->DeflateOut[PROFILE_COMPRESS_CHUNK - Stream.avail_out];
    if (pSendStart != pSendEnd)
    {
        ProfileSendSocket(pState->Socket, (char*)pSendStart, pSendEnd - pSendStart);
        pState->nCompressedSize += pSendEnd - pSendStart;
    }
    Stream.next_out = &pState->DeflateOut[0];
    Stream.avail_out = PROFILE_COMPRESS_CHUNK;
}
void ProfileCompressedSocketStart(ProfileCompressedSocketState* pState, MpSocket Socket)
{
    mz_stream& Stream = pState->Stream;
    memset(&Stream, 0, sizeof(Stream));
    Stream.next_out = &pState->DeflateOut[0];
    Stream.avail_out = PROFILE_COMPRESS_CHUNK;
    Stream.next_in = &pState->DeflateIn[0];
    Stream.avail_in = 0;
    mz_deflateInit(&Stream, MZ_DEFAULT_COMPRESSION);
    pState->Socket = Socket;
    pState->nSize = 0;
    pState->nCompressedSize = 0;
    pState->nFlushes = 0;
    pState->nMemmoveBytes = 0;
}
void ProfileCompressedSocketFinish(ProfileCompressedSocketState* pState)
{
    mz_stream& Stream = pState->Stream;
    ProfileCompressedSocketFlush(pState);
    int r = mz_deflate(&Stream, MZ_FINISH);
    P_ASSERT(r == MZ_STREAM_END);
    ProfileCompressedSocketFlush(pState);
    r = mz_deflateEnd(&Stream);
    P_ASSERT(r == MZ_OK);
}

void ProfileCompressedWriteSocket(void* Handle, size_t nSize, const char* pData)
{
    ProfileCompressedSocketState* pState = (ProfileCompressedSocketState*)Handle;
    mz_stream&                    Stream = pState->Stream;
    const unsigned char*          pDeflateInEnd = Stream.next_in + Stream.avail_in;
    const unsigned char*          pDeflateInStart = &pState->DeflateIn[0];
    const unsigned char*          pDeflateInRealEnd = &pState->DeflateIn[PROFILE_COMPRESS_CHUNK];
    pState->nSize += nSize;
    if (nSize <= pDeflateInRealEnd - pDeflateInEnd)
    {
        memcpy((void*)pDeflateInEnd, pData, nSize);
        Stream.avail_in += nSize;
        P_ASSERT(Stream.next_in + Stream.avail_in <= pDeflateInRealEnd);
        return;
    }
    int Flush = 0;
    while (nSize)
    {
        pDeflateInEnd = Stream.next_in + Stream.avail_in;
        if (Flush)
        {
            pState->nFlushes++;
            ProfileCompressedSocketFlush(pState);
            pDeflateInRealEnd = &pState->DeflateIn[PROFILE_COMPRESS_CHUNK];
            if (pDeflateInEnd == pDeflateInRealEnd)
            {
                if (Stream.avail_in)
                {
                    P_ASSERT(pDeflateInStart != Stream.next_in);
                    memmove((void*)pDeflateInStart, Stream.next_in, Stream.avail_in);
                    pState->nMemmoveBytes += Stream.avail_in;
                }
                Stream.next_in = pDeflateInStart;
                pDeflateInEnd = Stream.next_in + Stream.avail_in;
            }
        }
        size_t nSpace = pDeflateInRealEnd - pDeflateInEnd;
        size_t nBytes = ProfileMin(nSpace, nSize);
        P_ASSERT(nBytes + pDeflateInEnd <= pDeflateInRealEnd);
        memcpy((void*)pDeflateInEnd, pData, nBytes);
        Stream.avail_in += nBytes;
        nSize -= nBytes;
        pData += nBytes;
        int r = mz_deflate(&Stream, MZ_NO_FLUSH);
        Flush = r == MZ_BUF_ERROR || nBytes == 0 || Stream.avail_out == 0 ? 1 : 0;
        P_ASSERT(r == MZ_BUF_ERROR || r == MZ_OK);
        if (r == MZ_BUF_ERROR)
        {
            r = mz_deflate(&Stream, MZ_SYNC_FLUSH);
        }
    }
}
#endif

void ProfileWebServerUpdate(void*);
void ProfileWebServerUpdateStop();

void ProfileWebServerHello(int nPort)
{
    uint32_t nInterfaces = 0;

    // getifaddrs hangs on some versions of Android so disable IP address scanning
#if (defined(__APPLE__) || defined(__linux__)) && !defined(__ANDROID__)
    struct ifaddrs* ifal;
    if (getifaddrs(&ifal) == 0 && ifal)
    {
        for (struct ifaddrs* ifa = ifal; ifa; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)
            {
                void* pAddress = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
                char  Ip[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, pAddress, Ip, sizeof(Ip)))
                {
                    PROFILE_PRINTF("Profile: Web server started on %s:%d\n", Ip, nPort);
                    nInterfaces++;
                }
            }
        }

        freeifaddrs(ifal);
    }
#endif

    if (nInterfaces == 0) //-V547
    {
        PROFILE_PRINTF("Profile: Web server started on port %d\n", nPort);
    }
}

void ProfileWebServerStart()
{
    Profile& S = g_Profile;
    if (!S.WebServerThread)
    {
        ProfileThreadStart(&S.WebServerThread, ProfileWebServerUpdate);
    }
}

void ProfileWebServerStop()
{
    Profile& S = g_Profile;
    if (S.WebServerThread)
    {
        ProfileWebServerUpdateStop();
        ProfileThreadJoin(&S.WebServerThread);
    }
}

const char* ProfileParseHeader(char* pRequest, const char* pPrefix)
{
    size_t nRequestSize = strlen(pRequest);
    size_t nPrefixSize = strlen(pPrefix);

    for (uint32_t i = 0; i < nRequestSize; ++i)
    {
        if ((i == 0 || pRequest[i - 1] == '\n') && strncmp(&pRequest[i], pPrefix, nPrefixSize) == 0)
        {
            char*  pResult = &pRequest[i + nPrefixSize];
            size_t nResultSize = strcspn(pResult, " \r\n");

            pResult[nResultSize] = '\0';
            return pResult;
        }
    }

    return 0;
}

int ProfileParseGet(const char* pGet)
{
    const char* pStart = pGet;
    while (*pGet != '\0')
    {
        if (*pGet < '0' || *pGet > '9')
            return 0;
        pGet++;
    }
    int nFrames = atoi(pStart);
    if (nFrames)
    {
        return nFrames;
    }
    else
    {
        return PROFILE_WEBSERVER_FRAMES;
    }
}

void ProfileWebServerHandleRequest(MpSocket Connection)
{
    Profile& S = g_Profile;
    char     Request[8192];
    long     nReceived = recv(Connection, Request, sizeof(Request) - 1, 0);
    if (nReceived <= 0)
        return;
    Request[nReceived] = 0;

    MutexLock lock(ProfileMutex());

    PROFILE_SCOPEI("Profile", "WebServerUpdate", 0xDD7300);

#if PROFILE_MINIZ
#define PROFILE_HTML_HEADER \
    "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Encoding: deflate\r\nExpires: Tue, 01 Jan 2199 16:00:00 GMT\r\n\r\n"
#else
#define PROFILE_HTML_HEADER "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nExpires: Tue, 01 Jan 2199 16:00:00 GMT\r\n\r\n"
#endif

    const char* pUrl = ProfileParseHeader(Request, "GET /");
    if (!pUrl)
        return;

    int nFrames = ProfileParseGet(pUrl);
    if (nFrames <= 0)
        return;

    const char* pHost = ProfileParseHeader(Request, "Host: ");

    uint64_t nTickStart = P_TICK();
    ProfileSendSocket(Connection, PROFILE_HTML_HEADER, sizeof(PROFILE_HTML_HEADER) - 1);
    uint64_t nDataStart = S.nWebServerDataSent;
    S.nWebServerPut = 0;
#if 0 == PROFILE_MINIZ
	ProfileDumpHtml(ProfileWriteSocket, &Connection, nFrames, pHost);
	uint64_t nDataEnd = S.nWebServerDataSent;
	uint64_t nTickEnd = P_TICK();
	uint64_t nDiff = (nTickEnd - nTickStart);
	float fMs = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu()) * nDiff;
	int nKb = (int)((nDataEnd - nDataStart) >> 10) + 1;
	ProfilePrintf(ProfileWriteSocket, &Connection, "\n<!-- Sent %dkb in %.2fms-->\n\n", nKb, fMs);
	ProfileFlushSocket(Connection);
#else
    ProfileCompressedSocketState CompressState;
    ProfileCompressedSocketStart(&CompressState, Connection);
    ProfileDumpHtml(ProfileCompressedWriteSocket, &CompressState, nFrames, pHost);
    S.nWebServerDataSent += CompressState.nSize;
    uint64_t nDataEnd = S.nWebServerDataSent;
    uint64_t nTickEnd = P_TICK();
    uint64_t nDiff = (nTickEnd - nTickStart);
    float    fMs = ProfileTickToMsMultiplier(ProfileTicksPerSecondCpu()) * nDiff;
    int      nKb = ((nDataEnd - nDataStart) >> 10) + 1;
    int      nCompressedKb = ((CompressState.nCompressedSize) >> 10) + 1;
    ProfilePrintf(ProfileCompressedWriteSocket, &CompressState, "\n<!-- Sent %dkb(compressed %dkb) in %.2fms-->\n\n", nKb, nCompressedKb,
                  fMs);
    ProfileCompressedSocketFinish(&CompressState);
    ProfileFlushSocket(Connection);
#endif
}

void ProfileWebServerCloseSocket(MpSocket Connection)
{
#ifdef _WINDOWS
    closesocket(Connection);
#else
    shutdown(Connection, SHUT_RDWR);
    close(Connection);
#endif
}

void ProfileWebServerUpdate(void*)
{
    Profile& S = g_Profile;
#if defined(_WINDOWS) || defined(XBOX)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa))
        return;
#endif

    S.WebServerSocket = socket(PF_INET, SOCK_STREAM, 6);
    P_ASSERT(!P_INVALID_SOCKET(S.WebServerSocket));

    uint32_t nPortBegin = PROFILE_WEBSERVER_PORT;
    uint32_t nPortEnd = nPortBegin + 20;

    struct sockaddr_in Addr;
    Addr.sin_family = AF_INET;
    Addr.sin_addr.s_addr = INADDR_ANY;
    for (uint32_t nPort = nPortBegin; nPort < nPortEnd; ++nPort)
    {
        Addr.sin_port = htons(nPort);
        if (0 == ::bind(S.WebServerSocket, (sockaddr*)&Addr, sizeof(Addr)))
        {
            S.nWebServerPort = nPort;
            break;
        }
    }

    if (S.nWebServerPort)
    {
        ProfileWebServerHello(S.nWebServerPort);

        listen(S.WebServerSocket, 8);

        for (;;)
        {
            MpSocket Connection = accept(S.WebServerSocket, 0, 0);
            if (P_INVALID_SOCKET(Connection))
                break;

#ifdef SO_NOSIGPIPE
            int nConnectionOption = 1;
            setsockopt(Connection, SOL_SOCKET, SO_NOSIGPIPE, &nConnectionOption, sizeof(nConnectionOption));
#endif

            ProfileWebServerHandleRequest(Connection);

            ProfileWebServerCloseSocket(Connection);
            ProfileOnThreadExit();
        }

        S.nWebServerPort = 0;
    }
    else
    {
        PROFILE_PRINTF("Profile: Web server could not start: no free ports in range [%d..%d)\n", nPortBegin, nPortEnd);
    }

#if defined(_WINDOWS) || defined(XBOX)
    WSACleanup();
#endif

    return;
}

void ProfileWebServerUpdateStop()
{
    Profile& S = g_Profile;
    ProfileWebServerCloseSocket(S.WebServerSocket);
}
#else
void ProfileWebServerStart() {}

void ProfileWebServerStop() {}

uint32_t ProfileWebServerPort() { return 0; }
#endif

#if PROFILE_CONTEXT_SWITCH_TRACE
// functions that need to be implemented per platform.
void ProfileTraceThread(void* unused);

void ProfileContextSwitchTraceStart()
{
    Profile& S = g_Profile;
    if (!S.ContextSwitchThread)
    {
        ProfileThreadStart(&S.ContextSwitchThread, ProfileTraceThread);
    }
}

void ProfileContextSwitchTraceStop()
{
    Profile& S = g_Profile;
    if (S.ContextSwitchThread)
    {
        S.bContextSwitchStop = true;
        ProfileThreadJoin(&S.ContextSwitchThread);
        S.bContextSwitchStop = false;
    }
}

void ProfileContextSwitchSearch(uint32_t* pContextSwitchStart, uint32_t* pContextSwitchEnd, uint64_t nBaseTicksCpu,
                                uint64_t nBaseTicksEndCpu)
{
    PROFILE_SCOPEI("Profile", "ContextSwitchSearch", 0xDD7300);
    Profile& S = g_Profile;
    uint32_t nContextSwitchPut = S.nContextSwitchPut;
    uint64_t nContextSwitchStart, nContextSwitchEnd;
    nContextSwitchStart = nContextSwitchEnd =
        (nContextSwitchPut + PROFILE_CONTEXT_SWITCH_BUFFER_SIZE - 1) % PROFILE_CONTEXT_SWITCH_BUFFER_SIZE;
    int64_t nSearchEnd = nBaseTicksEndCpu + ProfileMsToTick(30.f, ProfileTicksPerSecondCpu());
    int64_t nSearchBegin = nBaseTicksCpu - ProfileMsToTick(30.f, ProfileTicksPerSecondCpu());
    for (uint32_t i = 0; i < PROFILE_CONTEXT_SWITCH_BUFFER_SIZE; ++i)
    {
        uint32_t nIndex = (nContextSwitchPut + PROFILE_CONTEXT_SWITCH_BUFFER_SIZE - (i + 1)) % PROFILE_CONTEXT_SWITCH_BUFFER_SIZE;
        ProfileContextSwitch& CS = S.ContextSwitch[nIndex];
        if (CS.nTicks > nSearchEnd)
        {
            nContextSwitchEnd = nIndex;
        }
        if (CS.nTicks > nSearchBegin)
        {
            nContextSwitchStart = nIndex;
        }
    }
    *pContextSwitchStart = (uint32_t)nContextSwitchStart;
    *pContextSwitchEnd = (uint32_t)nContextSwitchEnd;
}

uint32_t ProfileContextSwitchGatherThreads(uint32_t nContextSwitchStart, uint32_t nContextSwitchEnd, ProfileThreadInfo* Threads,
                                           uint32_t* nNumThreadsBase)
{
    ProfileProcessIdType nCurrentProcessId = P_GETCURRENTPROCESSID();

    uint32_t nNumThreads = 0;
    Profile& S = g_Profile;
    for (uint32_t i = 0; i < PROFILE_MAX_THREADS; ++i)
    {
        if (!S.Pool[i])
            continue;
        Threads[nNumThreads].nProcessId = nCurrentProcessId;
        Threads[nNumThreads].nThreadId = S.Pool[i]->nThreadId;
        nNumThreads++;
    }

    *nNumThreadsBase = nNumThreads;

    for (uint32_t i = nContextSwitchStart; i != nContextSwitchEnd; i = (i + 1) % PROFILE_CONTEXT_SWITCH_BUFFER_SIZE)
    {
        ProfileContextSwitch CS = S.ContextSwitch[i];
        ProfileThreadIdType  nThreadId = CS.nThreadIn;
        if (nThreadId)
        {
            ProfileProcessIdType nProcessId = CS.nProcessIn;

            bool bSeen = false;
            for (uint32_t j = 0; j < nNumThreads; ++j)
            {
                if (Threads[j].nThreadId == nThreadId && Threads[j].nProcessId == nProcessId)
                {
                    bSeen = true;
                    break;
                }
            }
            if (!bSeen)
            {
                Threads[nNumThreads].nProcessId = nProcessId;
                Threads[nNumThreads].nThreadId = nThreadId;
                nNumThreads++;
            }
        }
        if (nNumThreads == PROFILE_MAX_CONTEXT_SWITCH_THREADS)
        {
            break;
        }
    }

    return nNumThreads;
}

#if defined(_WINDOWS) || defined(XBOX)
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

const char* ProfileGetProcessName(ProfileProcessIdType nId, char* Buffer, uint32_t nSize)
{
    if (HANDLE Handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, nId))
    {
        DWORD nResult = GetModuleBaseNameA(Handle, nullptr, Buffer, nSize);
        CloseHandle(Handle);

        return nResult ? Buffer : nullptr;
    }
    return nullptr;
}

void ProfileTraceThread(void* unused)
{
    Profile& S = g_Profile;
    while (!S.bContextSwitchStop)
    {
        PathHandle  path = fsCreatePath(fsGetSystemFileSystem(), "\\\\.\\pipe\\microprofile-contextswitch");
        FileStream* fh = fsOpenStreamFromPath(path, FM_WRITE);

        if (!fh)
        {
            Sleep(1000);
            continue;
        }

        S.bContextSwitchRunning = true;

        ProfileContextSwitch Buffer[1024];
        while (!fsStreamAtEnd(fh) && !S.bContextSwitchStop)
        {
            size_t nCount = fsReadFromStream(Buffer, sizeof(ProfileContextSwitch) * ARRAYSIZE(Buffer));

            for (size_t i = 0; i < nCount; ++i)
                ProfileContextSwitchPut(&Buffer[i]);
        }

        fsCloseStream(fh);

        S.bContextSwitchRunning = false;
    }
}
#elif defined(__APPLE__)
#include <libproc.h>
#include <sys/time.h>

const char* ProfileGetProcessName(ProfileProcessIdType nId, char* Buffer, uint32_t nSize)
{
    char Path[PATH_MAX];
    if (proc_pidpath(nId, Path, sizeof(Path)) == 0)
        return nullptr;

    char* pSlash = strrchr(Path, '/');
    char* pName = pSlash ? pSlash + 1 : Path;

    strncpy(Buffer, pName, nSize - 1);
    Buffer[nSize - 1] = 0;

    return Buffer;
}

void ProfileTraceThread(void*)
{
    Profile& S = g_Profile;
    while (!S.bContextSwitchStop)
    {
        FileSystem* fileSystem = fsGetSystemFileSystem();
        PathHandle path = fsCreatePath(fileSystem, "/tmp/microprofile-contextswitch");
        FileStream* fh = fsOpenStreamFromPath(path, FM_READ);
        if (!fh)
        {
            usleep(1000000);
            continue;
        }

        S.bContextSwitchRunning = true;

        char line[1024] = {};
        size_t cap = 0;
        size_t len = 0;

        ProfileThreadIdType nLastThread[PROFILE_MAX_CONTEXT_SWITCH_THREADS] = { 0 };

        while ((len = fsReadFromStreamLine(fh, line, 1024)) > 0 && !S.bContextSwitchStop)
        {
            if (strncmp(line, "MPTD ", 5) != 0)
                continue;

            char* pos = line + 4;
            long cpu = strtol(pos + 1, &pos, 16);
            long pid = strtol(pos + 1, &pos, 16);
            long tid = strtol(pos + 1, &pos, 16);
            int64_t timestamp = strtoll(pos + 1, &pos, 16);

            if (cpu < PROFILE_MAX_CONTEXT_SWITCH_THREADS)
            {
                ProfileContextSwitch Switch;

                Switch.nThreadOut = nLastThread[cpu];
                Switch.nThreadIn = tid;
                Switch.nProcessIn = (ProfileProcessIdType)pid;
                Switch.nCpu = cpu;
                Switch.nTicks = timestamp;
                ProfileContextSwitchPut(&Switch);

                nLastThread[cpu] = tid;
            }
        }

        fsCloseStream(fh);
        S.bContextSwitchRunning = false;
    }
}
#endif
#else
void     ProfileContextSwitchTraceStart() {}

void ProfileContextSwitchTraceStop() {}

void ProfileContextSwitchSearch(uint32_t* pContextSwitchStart, uint32_t* pContextSwitchEnd, uint64_t nBaseTicksCpu,
                                uint64_t nBaseTicksEndCpu)
{
    (void)nBaseTicksCpu;
    (void)nBaseTicksEndCpu;

    *pContextSwitchStart = 0;
    *pContextSwitchEnd = 0;
}

uint32_t ProfileContextSwitchGatherThreads(uint32_t nContextSwitchStart, uint32_t nContextSwitchEnd, ProfileThreadInfo* Threads,
                                           uint32_t* nNumThreadsBase)
{
    (void)nContextSwitchStart;
    (void)nContextSwitchEnd;
    (void)Threads;

    *nNumThreadsBase = 0;
    return 0;
}

const char* ProfileGetProcessName(ProfileProcessIdType nId, char* Buffer, uint32_t nSize)
{
    (void)nId;
    (void)Buffer;
    (void)nSize;

    return nullptr;
}
#endif

#if PROFILE_EMBED_HTML
#include "ProfilerHTML.h"
#endif // PROFILE_EMBED_HTML

#else

void  initProfiler(ProfilerDesc* pDesc) {}
void  exitProfiler() {}
void  flipProfiler() {}
void  dumpProfileData(const char* appName, uint32_t nMaxFrames) {}
void  dumpBenchmarkData(IApp::Settings* pSettings, const char* outFilename, const char* appName) {}
void  setAggregateFrames(uint32_t nFrames) {}
float getCpuProfileTime(const char* pGroup, const char* pName, ThreadID* pThreadID) { return -1.0f; }
float getCpuProfileAvgTime(const char* pGroup, const char* pName, ThreadID* pThreadID) { return -1.0f; }
float getCpuProfileMinTime(const char* pGroup, const char* pName, ThreadID* pThreadID) { return -1.0f; }
float getCpuProfileMaxTime(const char* pGroup, const char* pName, ThreadID* pThreadID) { return -1.0f; }

float getCpuFrameTime() { return -1.0f; }
float getCpuAvgFrameTime() { return -1.0f; }
float getCpuMinFrameTime() { return -1.0f; }
float getCpuMaxFrameTime() { return -1.0f; }

uint64_t     cpuProfileEnter(ProfileToken nToken) { return 0; }
void         cpuProfileLeave(ProfileToken nToken, uint64_t nTick) {}
ProfileToken getCpuProfileToken(const char* pGroup, const char* pName, uint32_t nColor) { return PROFILE_INVALID_TOKEN; }

float2 cmdDrawGpuProfile(Cmd* pCmd, float2 screenCoordsInPx, ProfileToken nProfileToken, FontDrawDesc* pDrawDesc) { return float2{}; }
float2 cmdDrawCpuProfile(Cmd* pCmd, float2 screenCoordsInPx, FontDrawDesc* pDrawDesc) { return float2{}; }
void   updateProfilerUI() {}
void   toggleProfilerUI(bool active) {}
void   toggleProfilerMenuUI(bool active) {}
void   toggleProfilerDrawing(bool active) {}
#endif

#pragma once

#ifndef PROFILE_ENABLED
#define PROFILE_ENABLED 1
#endif

#if (PROFILE_ENABLED)
#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11) || defined(METAL) || defined(ORBIS) || defined(PROSPERO)
#define GPU_PROFILER_SUPPORTED 1
#endif

#ifndef PROFILE_WEBSERVER
#define PROFILE_WEBSERVER 0	// Enable this if you want to have the profiler through a web browser, see PROFILE_WEBSERVER_PORT for server location
#endif

#endif

#include "ILog.h"
#include "IApp.h"
#include "IOperatingSystem.h"
#include "IThread.h"

typedef uint64_t ProfileToken;
#define PROFILE_INVALID_TOKEN (uint64_t)-1

struct Cmd;
struct Renderer;
struct Queue;
struct TextDrawDesc;
class UIApp;

// Must be called before adding any profiling
void initProfiler(Renderer* pRenderer = NULL, Queue** ppQueue = NULL, const char** ppProfilerNames = NULL, ProfileToken* pProfileTokens = NULL, uint32_t nGpuProfilerCount = 0);

// Call on application exit
void exitProfiler();

// Call once per frame to update profiler
void flipProfiler();

// Set amount of frames before aggregation
void setAggregateFrames(uint32_t nFrames);

// Dump profile data to "profile-(date).html" of recorded frames, until a maximum amount of frames
void dumpProfileData(Renderer* pRenderer, const char* appName = "" , uint32_t nMaxFrames = 64);

// Dump benchmark data to "benchmark-(data).txt" of recorded frames
void dumpBenchmarkData(Renderer* pRenderer, IApp::Settings* pSettings, const char* appName = "");


//------ Profiler UI Widget --------//

// Call on application load to generate the resources needed for UI drawing
void loadProfilerUI(UIApp* uiApp, int32_t width, int32_t height);

// Call on application exit to release resources needed for UI drawing
void unloadProfilerUI();

// Call once per frame to draw UI
void cmdDrawProfilerUI();

// Call once per frame before AppUI.Draw, draw requested Gpu profiler timers
// Returns text dimensions so caller can align other UI elements
float2 cmdDrawGpuProfile(Cmd* pCmd, const float2& screenCoordsInPx, ProfileToken nProfileToken, const TextDrawDesc* pDrawDesc = NULL);

// Call once per frame before AppUI.Draw, draw requested Cpu profile time
// Returns text dimensions so caller can align other UI elements
float2 cmdDrawCpuProfile(Cmd* pCmd, const float2& screenCoordsInPx, const TextDrawDesc* pDrawDesc = NULL);

// Toggle profiler display on/off.
void toggleProfilerUI();

//------ Gpu profiler ------------//

// Call only after initProfiler(), for manually adding Gpu Profilers
ProfileToken addGpuProfiler(Renderer* pRenderer, Queue* pQueue, const char* pProfilerName);

// Call only before exitProfiler(), for manually removing Gpu Profilers
void removeGpuProfiler(ProfileToken nProfileToken);

// Must be called before any call to cmdBeginGpuTimestampQuery
// Preferred time to call this function is right after calling beginCmd
void cmdBeginGpuFrameProfile(Cmd* pCmd, ProfileToken nProfileToken, bool bUseMarker = true);

// Must be called after all gpu profiles are finished.
// This function cannot be called inside a render pass (cmdBeginRender-cmdEndRender)
// Preferred time to call this function is right before calling endCmd
void cmdEndGpuFrameProfile(Cmd* pCmd, ProfileToken nProfileToken);

ProfileToken cmdBeginGpuTimestampQuery(Cmd* pCmd, ProfileToken nProfileToken, const char* pName, bool bUseMarker = true);

void cmdEndGpuTimestampQuery(Cmd* pCmd, ProfileToken nProfileToken);

// Gpu times in milliseconds
float getGpuProfileTime(ProfileToken nProfileToken);
float getGpuProfileAvgTime(ProfileToken nProfileToken);
float getGpuProfileMinTime(ProfileToken nProfileToken);
float getGpuProfileMaxTime(ProfileToken nProfileToken);

uint64_t getGpuProfileTicksPerSecond(ProfileToken nProfileToken);

//------ Cpu profiler ------------//

uint64_t cpuProfileEnter(ProfileToken nToken);

void cpuProfileLeave(ProfileToken nToken, uint64_t nTick);

ProfileToken getCpuProfileToken(const char* pGroup, const char* pName, uint32_t nColor);

struct CpuProfileScopeMarker
{
    ProfileToken nToken;
    uint64_t nTick;
    CpuProfileScopeMarker(const char* pGroup, const char* pName, uint32_t nColor)
    {
        nToken = getCpuProfileToken(pGroup, pName, nColor);
        nTick = cpuProfileEnter(nToken);
    }
    ~CpuProfileScopeMarker()
    {
        cpuProfileLeave(nToken, nTick);
    }
};

#define PROFILER_CONCAT0(a, b) a ## b 
#define PROFILER_CONCAT(a, b) PROFILER_CONCAT0(a, b) 
// Call at the start of a block to profile cpu time between '{' '}' 
#define PROFILER_SET_CPU_SCOPE(group, name, color) CpuProfileScopeMarker PROFILER_CONCAT(marker,__LINE__)(group, name, color)

// Cpu times in milliseconds
float getCpuProfileTime(const char* pGroup, const char* pName, ThreadID* pThreadID = NULL);
float getCpuProfileAvgTime(const char* pGroup, const char* pName, ThreadID* pThreadID = NULL);
float getCpuProfileMinTime(const char* pGroup, const char* pName, ThreadID* pThreadID = NULL);
float getCpuProfileMaxTime(const char* pGroup, const char* pName, ThreadID* pThreadID = NULL);

float getCpuFrameTime();
float getCpuAvgFrameTime();
float getCpuMinFrameTime();
float getCpuMaxFrameTime();
#pragma once

#if defined(ENABLE_RENDERER_RUNTIME_SWITCH)
#ifndef PROFILE_ENABLED
#define PROFILE_ENABLED 0
#endif
#else
#ifndef PROFILE_ENABLED
#define PROFILE_ENABLED 1
#endif
#endif

#if (PROFILE_ENABLED)

#ifndef PROFILE_WEBSERVER
#define PROFILE_WEBSERVER 0	// Enable this if you want to have the profiler through a web browser
#endif

#endif

#include <stdint.h>

struct Cmd;
struct GpuProfiler;
struct Renderer;
struct GpuTimer;
struct SwapChain;
struct RenderTarget;

// Call on application initialize to generate the resources needed for UI drawing
// Must be called before adding any GpuProfiler
void initProfiler(Renderer* pRenderer);

// Must be called to specify render target format to draw into
void loadProfiler(RenderTarget* pRenderTarget);

// Call on application initialize for Profiler's UI input handling
// To modify inputs go to file /Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerInput.cpp
void profileRegisterInput();

// Call on application load
void unloadProfiler();

// Call on application exit to release resources needed for UI drawing
void exitProfiler();

// Call once per frame to update profiler, ideally after presenting the frame so GPU timestamps are as accurate as possible
void flipProfiler();

// Call once per frame to draw UI
void cmdDrawProfiler(Cmd* pCmd);

// Check this file for how to CPU profile
#include "../../ThirdParty/OpenSource/MicroProfile/ProfilerBase.h"

// Check this file for how to GPU profile
#include "../../Renderer/GpuProfiler.h"

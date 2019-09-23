#pragma once

#ifndef PROFILE_ENABLED
#define PROFILE_ENABLED 1
#endif

#if (PROFILE_ENABLED)
#ifndef PROFILE_WEBSERVER
#define PROFILE_WEBSERVER 0	// Enable this if you want to have the profiler through a web browser
#endif

#endif

#include "IOperatingSystem.h"

struct Cmd;
struct GpuProfiler;
struct Renderer;
struct GpuTimer;
struct SwapChain;
struct RenderTarget;
class UIApp;


// Must be called before adding any GpuProfiler
void initProfiler();

// Call on application load to generate the resources needed for UI drawing
void loadProfiler(UIApp* uiApp, int32_t width, int32_t height);

// Call on application load
void unloadProfiler();

// Call on application exit to release resources needed for UI drawing
void exitProfiler();

// Call once per frame to update profiler, ideally after presenting the frame so GPU timestamps are as accurate as possible
void flipProfiler();

// Call once per frame to draw UI
void cmdDrawProfiler();

// Toggle profiler display on/off.
void toggleProfiler();

// To modify inputs go to file /Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerInput.cpp
//bool onProfilerButton(bool press, float2* pPos, bool delta);

// Check this file for how to CPU profile
#include "../../ThirdParty/OpenSource/MicroProfile/ProfilerBase.h"

// Check this file for how to GPU profile
#include "../../Renderer/GpuProfiler.h"

// Check this for widget profiler UI.
#include "../../ThirdParty/OpenSource/MicroProfile/ProfilerWidgetsUI.h"

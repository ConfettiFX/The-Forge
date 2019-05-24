#pragma once

#include "ProfilerEnableMacro.h"

#if (PROFILE_ENABLED)
struct Renderer;
struct SwapChain;

// Creates resources needed to draw and initializes UI
void ProfileInitialize(Renderer * pRenderer, int image_count);
// Creates graphics pipelines
void ProfileLoad(Renderer * pRenderer, SwapChain * pSwapChain);
// Destroys graphics pipelines
void ProfileUnload(Renderer * pRenderer);
// Destroys resources and shutdowns profiler
void ProfileExit(Renderer * pRenderer);

class UIApp;
void ActivateMicroProfile(UIApp * app, bool isActive);

#else
#define ProfileInitialize(renderer, image_count) while(0);
#define ProfileLoad(renderer, swap_chain) while(0);
#define ProfileUnload(renderer) while(0);
#define ProfileExit(renderer) while(0);
#define ActivateMicroProfile(app, is_active) while(0);
#define ProfileBeginDraw(width, height) while(0);
#define ProfileEndDraw(cmd) while(0);
#endif

#include "ProfilerBase.h"
#include "ProfilerUI.h"
#include "ProfilerInput.h"

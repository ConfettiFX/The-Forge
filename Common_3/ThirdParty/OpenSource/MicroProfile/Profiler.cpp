#include "../../../OS/Interfaces/IProfiler.h"

void flipProfiler()
{
#if PROFILE_ENABLED
	ProfileFlip();
#endif
}

void toggleProfiler() 
{
#if PROFILE_ENABLED
  toggleWidgetProfilerUI();
#endif
}

void cmdDrawProfiler()
{
#if  PROFILE_ENABLED
  drawWidgetProfilerUI();
#endif
}

#if PROFILE_ENABLED
extern void ProfileInit();
#endif

// Profiler initialization.
void initProfiler()
{
#if PROFILE_ENABLED
  // Initialize Profiler and Widget UI.
  ProfileInit();
  ProfileSetEnableAllGroups(true);
  ProfileWebServerStart();
#endif
}

void loadProfiler(UIApp* uiApp, int32_t width, int32_t height)
{
  initWidgetProfilerUI(uiApp, width, height);
}

void unloadProfiler()
{
#if  PROFILE_ENABLED
  unloadWidgetProfilerUI();
#endif
}

#if PROFILE_ENABLED
extern void ProfileShutdown();
#endif

void exitProfiler()
{
#if PROFILE_ENABLED
  ProfileShutdown();
#endif
}
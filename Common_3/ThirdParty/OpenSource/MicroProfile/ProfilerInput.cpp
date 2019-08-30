#include "../../../OS/Interfaces/IProfiler.h"

#if (PROFILE_ENABLED)

#include "ProfilerBase.h"
#include "ProfilerUI.h"

bool onProfilerButton(bool press, float2* pPos, bool delta)
{
    if (pPos)
    {
        if (delta)
            ProfileMousePositionDelta(pPos->x, pPos->y);
        else
            ProfileMousePosition(pPos, 0);
    }
    
    ProfileMouseButton(press, false);
	return true;
}
#else
bool onProfilerButton(bool press, float x, float y, bool delta) { return true; }
#endif

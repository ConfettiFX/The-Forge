#include "../../../OS/Interfaces/IProfiler.h"

#include "ProfilerBase.h"
#include "ProfilerUI.h"

void flipProfiler()
{
	ProfileFlip();
}

void cmdDrawProfiler(Cmd * pCmd, uint32_t Width, uint32_t Height)
{
	ProfileDraw(pCmd, Width, Height);
}

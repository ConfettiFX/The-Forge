#include "../../../OS/Interfaces/IProfiler.h"

#include "ProfilerBase.h"
#include "ProfilerUI.h"

void flipProfiler()
{
	ProfileFlip();
}

void cmdDrawProfiler(Cmd * pCmd)
{
	ProfileDraw(pCmd);
}

#pragma once

#include "ProfilerEnableMacro.h"

#if (PROFILE_ENABLED)
void ProfileRegisterInput();
#else
#define ProfileRegisterInput() while(0);
#endif

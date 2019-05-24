#pragma once

#if defined(_DURANGO) || defined(ENABLE_RENDERER_RUNTIME_SWITCH)
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

typedef uint64_t ProfileToken;
typedef uint16_t ProfileGroupId;


#ifndef GAINPUT_LOG_H_
#define GAINPUT_LOG_H_

#include "../../include/gainput/gainput.h"

#if defined(GAINPUT_PLATFORM_LINUX)

#if defined(GAINPUT_DEBUG) || defined(GAINPUT_DEV)
	#include <stdio.h>
	#define GAINPUT_LOG(...) printf(__VA_ARGS__);
#endif

#elif defined(GAINPUT_PLATFORM_WIN) || defined(GAINPUT_PLATFORM_XBOX_ONE)

#if defined(GAINPUT_DEBUG) || defined(GAINPUT_DEV)
	#include <stdio.h>
    #include <Windows.h>
	#define GAINPUT_LOG(...) { char buf[1024]; sprintf(buf, __VA_ARGS__); OutputDebugStringA(buf); }
#endif

#elif defined(GAINPUT_PLATFORM_ANDROID)

#if defined(GAINPUT_DEBUG) || defined(GAINPUT_DEV)
	#include <android/log.h>
	#define GAINPUT_LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, "gainput", __VA_ARGS__))
#endif

#elif defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
	#include <stdio.h>
	#define GAINPUT_LOG(...) printf(__VA_ARGS__);
#endif

#ifndef GAINPUT_LOG
#define GAINPUT_LOG(...)
#endif

#endif


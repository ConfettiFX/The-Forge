/*******************************************************************************

Filename	:   Log.h
Content		:	Macros for debug logging.
Created		:   February 21, 2018
Authors		:   Jonathan Wright
Language	:   C++

Copyright	:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#pragma once

#include <android/log.h>
#include <stdlib.h> // abort

#if defined(__cplusplus)
extern "C" {
#endif

void LogWithFilenameTag(const int priority, const char* filename, const char* fmt, ...);

#define ALOGE(...) \
    { LogWithFilenameTag(ANDROID_LOG_ERROR, __FILE__, __VA_ARGS__); }

#define ALOGE_FAIL(...)                                               \
    {                                                                 \
        LogWithFilenameTag(ANDROID_LOG_ERROR, __FILE__, __VA_ARGS__); \
        abort();                                                      \
    }

#if 1 // DEBUG

#define ALOG(...) \
    { LogWithFilenameTag(ANDROID_LOG_INFO, __FILE__, __VA_ARGS__); }

#define ALOGV(...) \
    { LogWithFilenameTag(ANDROID_LOG_VERBOSE, __FILE__, __VA_ARGS__); }

#define ALOGW(...) \
    { LogWithFilenameTag(ANDROID_LOG_WARN, __FILE__, __VA_ARGS__); }

#else
#define ALOGV(...)
#endif

#if defined(__cplusplus)
} // extern "C"
#endif

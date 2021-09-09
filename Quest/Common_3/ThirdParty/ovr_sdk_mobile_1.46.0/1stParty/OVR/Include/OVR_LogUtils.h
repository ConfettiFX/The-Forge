/************************************************************************************

Filename    :   OVR_LogUtils.h (Previously Log.h)
Content     :   Macros and helpers for Android logging.
Created     :   4/15/2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#if !defined(OVRLib_Log_h)
#define OVRLib_Log_h

#include "OVR_Types.h"
#include "OVR_Std.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#if defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
#if !defined(NOMINMAX)
#define NOMINMAX // stop Windows.h from redefining min and max and breaking std::min / std::max
#endif
#include <windows.h> // OutputDebugString
#endif

#if defined(OVR_OS_ANDROID)
#include <android/log.h>
#include <jni.h>

void LogWithTag(const int prio, const char* tag, const char* fmt, ...)
    __attribute__((__format__(printf, 3, 4))) __attribute__((__nonnull__(3)));

void LogWithFileTag(const int prio, const char* fileTag, const char* fmt, ...)
    __attribute__((__format__(printf, 3, 4))) __attribute__((__nonnull__(3)));

#endif

// Log with an explicit tag
inline void LogWithTag(const int prio, const char* tag, const char* fmt, ...) {
#if defined(OVR_OS_ANDROID)
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(prio, tag, fmt, ap);
    va_end(ap);
#elif defined(OVR_OS_WIN32)
    OVR_UNUSED(tag);
    OVR_UNUSED(prio);

    va_list args;
    va_start(args, fmt);

    char buffer[4096];
    vsnprintf_s(buffer, 4096, _TRUNCATE, fmt, args);
    va_end(args);

    OutputDebugStringA(buffer);
#else
#warning "LogWithTag not implemented for this given OVR_OS_"
#endif
}

// Strips the directory and extension from fileTag to give a concise log tag
inline void FilePathToTag(const char* filePath, char* strippedTag, const int strippedTagSize) {
    if (strippedTag == nullptr || strippedTagSize == 0) {
        return;
    }

    // scan backwards from the end to the first slash
    const int len = static_cast<int>(strlen(filePath));
    int slash;
    for (slash = len - 1; slash > 0 && filePath[slash] != '/' && filePath[slash] != '\\'; slash--) {
    }
    if (filePath[slash] == '/' || filePath[slash] == '\\') {
        slash++;
    }
    // copy forward until a dot or 0
    int i;
    for (i = 0; i < strippedTagSize - 1; i++) {
        const char c = filePath[slash + i];
        if (c == '.' || c == 0) {
            break;
        }
        strippedTag[i] = c;
    }
    strippedTag[i] = 0;
}

// Strips the directory and extension from fileTag to give a concise log tag
inline void LogWithFileTag(const int prio, const char* fileTag, const char* fmt, ...) {
    if (fileTag == nullptr) {
        return;
    }
#if defined(OVR_OS_ANDROID)
    va_list ap, ap2;

    // fileTag will be something like "jni/App.cpp", which we
    // want to strip down to just "App"
    char strippedTag[128];

    FilePathToTag(fileTag, strippedTag, static_cast<int>(sizeof(strippedTag)));

    va_start(ap, fmt);

    // Calculate the length of the log message... if its too long __android_log_vprint() will clip
    // it!
    va_copy(ap2, ap);
    const int requiredLen = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (requiredLen < 0) {
        __android_log_write(prio, strippedTag, "ERROR: failed to calculate log length!");
        return;
    }
    const size_t loglen = static_cast<size_t>(requiredLen);
    if (prio == ANDROID_LOG_ERROR) {
        // For FAIL messages which are longer than 512, truncate at 512.
        // We do not know the max size of abort message that will be taken by SIGABRT. 512 has been
        // verified to work
        char* formattedMsg = (char*)malloc(512);
        if (formattedMsg == nullptr) {
            __android_log_write(prio, strippedTag, "ERROR: out of memory allocating log message!");
        } else {
            va_copy(ap2, ap);
            vsnprintf(formattedMsg, 512U, fmt, ap2);
            va_end(ap2);
            __android_log_assert("FAIL", strippedTag, "%s", formattedMsg);
            free(formattedMsg);
        }
    }
    if (loglen < 512) {
        // For short messages just use android's default formatting path (which has a fixed size
        // buffer on the stack).
        __android_log_vprint(prio, strippedTag, fmt, ap);
    } else {
        // For long messages allocate off the heap to avoid blowing the stack...
        char* formattedMsg = (char*)malloc(loglen + 1);
        if (formattedMsg == nullptr) {
            __android_log_write(prio, strippedTag, "ERROR: out of memory allocating log message!");
        } else {
            vsnprintf(formattedMsg, loglen + 1, fmt, ap);
            __android_log_write(prio, strippedTag, formattedMsg);
            free(formattedMsg);
        }
    }

    va_end(ap);
#elif defined(OVR_OS_WIN32)
    OVR_UNUSED(fileTag);
    OVR_UNUSED(prio);

    va_list args;
    va_start(args, fmt);

    char buffer[4096];
    vsnprintf_s(buffer, 4096, _TRUNCATE, fmt, args);
    va_end(args);

    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
#else
#warning "LogWithFileTag not implemented for this given OVR_OS_"
#endif
}

#if defined(OVR_OS_WIN32) // allow this file to be included in PC projects

#define OVR_LOG(...) LogWithFileTag(0, __FILE__, __VA_ARGS__)
#define OVR_WARN(...) LogWithFileTag(0, __FILE__, __VA_ARGS__)
#define OVR_ERROR(...) \
    { LogWithFileTag(0, __FILE__, __VA_ARGS__); }
#define OVR_FAIL(...)                             \
    {                                             \
        LogWithFileTag(0, __FILE__, __VA_ARGS__); \
        exit(0);                                  \
    }
#define OVR_LOG_WITH_TAG(__tag__, ...) LogWithTag(0, __FILE__, __VA_ARGS__)
#define OVR_ASSERT_WITH_TAG(__expr__, __tag__)

#elif defined(OVR_OS_ANDROID)

// Our standard logging (and far too much of our debugging) involves writing
// to the system log for viewing with logcat.  Previously we defined separate
// LOG() macros in each file to give them file-specific tags for filtering;
// now we use this helper function to use a OVR_LOG_TAG define (via cflags or
// #define OVR_LOG_TAG in source file) when available. Fallback to using a massaged
// __FILE__ macro turning the file base in to a tag -- jni/App.cpp becomes the
// tag "App".
#ifdef OVR_LOG_TAG
#define OVR_LOG(...) ((void)LogWithTag(ANDROID_LOG_INFO, OVR_LOG_TAG, __VA_ARGS__))
#define OVR_WARN(...) ((void)LogWithTag(ANDROID_LOG_WARN, OVR_LOG_TAG, __VA_ARGS__))
#define OVR_ERROR(...) \
    { (void)LogWithTag(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__); }
#define OVR_FAIL(...)                                                  \
    {                                                                  \
        (void)LogWithTag(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__); \
        abort();                                                       \
    }
#else
#define OVR_LOG(...) LogWithFileTag(ANDROID_LOG_INFO, __FILE__, __VA_ARGS__)
#define OVR_WARN(...) LogWithFileTag(ANDROID_LOG_WARN, __FILE__, __VA_ARGS__)
#define OVR_ERROR(...) \
    { LogWithFileTag(ANDROID_LOG_ERROR, __FILE__, __VA_ARGS__); }
#define OVR_FAIL(...)                                             \
    {                                                             \
        LogWithFileTag(ANDROID_LOG_ERROR, __FILE__, __VA_ARGS__); \
        abort();                                                  \
    }
#endif

#define OVR_LOG_WITH_TAG(__tag__, ...) ((void)LogWithTag(ANDROID_LOG_INFO, __tag__, __VA_ARGS__))
#define OVR_WARN_WITH_TAG(__tag__, ...) ((void)LogWithTag(ANDROID_LOG_WARN, __tag__, __VA_ARGS__))
#define OVR_FAIL_WITH_TAG(__tag__, ...)                            \
    {                                                              \
        (void)LogWithTag(ANDROID_LOG_ERROR, __tag__, __VA_ARGS__); \
        abort();                                                   \
    }

// LOG (usually defined on a per-file basis to write to a specific tag) is for logging that can be
// checked in enabled and generally only prints once or infrequently. SPAM is for logging you want
// to see every frame but should never be checked in
#if defined(OVR_BUILD_DEBUG)
// you should always comment this out before checkin
//#define ALLOW_LOG_SPAM
#endif

#if defined(ALLOW_LOG_SPAM)
#define SPAM(...) LogWithTag(ANDROID_LOG_VERBOSE, "Spam", __VA_ARGS__)
#else
#define SPAM(...) \
    {}
#endif

// TODO: we need a define for internal builds that will compile in assertion messages but not debug
// breaks and we need a define for external builds that will do nothing when an assert is hit.
#if !defined(OVR_BUILD_DEBUG)
#define OVR_ASSERT_WITH_TAG(__expr__, __tag__)                             \
    {                                                                      \
        if (!(__expr__)) {                                                 \
            OVR_WARN_WITH_TAG(__tag__, "ASSERTION FAILED: %s", #__expr__); \
        }                                                                  \
    }
#else
#define OVR_ASSERT_WITH_TAG(__expr__, __tag__)                             \
    {                                                                      \
        if (!(__expr__)) {                                                 \
            OVR_WARN_WITH_TAG(__tag__, "ASSERTION FAILED: %s", #__expr__); \
            OVR_DEBUG_BREAK;                                               \
        }                                                                  \
    }
#endif

#elif defined(OVR_OS_MAC)

#define OVR_LOG(...) \
    {}
#define OVR_WARN(...) \
    {}
#define OVR_ERROR(...) \
    {}
#define OVR_FAIL(...) \
    {                 \
        ;             \
        exit(0);      \
    }
#define OVR_LOG_WITH_TAG(__tag__, ...) \
    {}
#define OVR_ASSERT_WITH_TAG(__expr__, __tag__) \
    {}

#else
#error "unknown platform"
#endif

#endif // OVRLib_Log_h

/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#pragma once

#include "../../Application/Config.h"

#include <stdarg.h>

#include "../Log/Log.h"

#ifdef __cplusplus
extern "C"
{
#endif

    FORGE_API void _EnableInteractiveMode(bool isInteractivenMode);
    FORGE_API bool _IsInteractiveMode(void);

    FORGE_API void _FailedAssertImpl(const char* file, int line, const char* statement, const char* message);
    //+V576, function:_OutputDebugString, format_arg:1, ellipsis_arg:2
    FORGE_API void _OutputDebugString(const char* str, ...);
    FORGE_API void _OutputDebugStringV(const char* str, va_list args);

    FORGE_API void _PrintUnicode(const char* str, bool error);

#ifdef __cplusplus
} // extern "C"
#endif

#if defined(_WINDOWS) || defined(XBOX)
#define CHECK_HRESULT(exp)                                                       \
    do                                                                           \
    {                                                                            \
        HRESULT hres = (exp);                                                    \
        if (!SUCCEEDED(hres))                                                    \
        {                                                                        \
            LOGF(eERROR, "%s: FAILED with HRESULT: 0x%X", #exp, (uint32_t)hres); \
            ASSERT(false);                                                       \
        }                                                                        \
    } while (0)
#endif

#if _MSC_VER >= 1400
// To make MSVC 2005 happy
#pragma warning(disable : 4996)
#define assume(x) __assume(x)
#define no_alias  __declspec(noalias)
#else
#define assume(x)
#define no_alias
#endif

// PVS-Studio isn't convinced an assert macro is real unless it branches on the value being asserted.
#if defined(FORGE_DEBUG) || defined(PVS_STUDIO)

// Checks that an expression if true. Becomes a no-op in release.
#define ASSERTMSG(b, msgFmt, ...)                                                                                \
    do                                                                                                           \
    {                                                                                                            \
        (void)sizeof(b); /* This sizeof must compile, otherwise when disabling ASSERTs the code won't compile */ \
        b ? (void)0 : _FailedAssert(__FILE__, __LINE__, #b, "" msgFmt, ##__VA_ARGS__);                           \
    } while (0)

// Version of ASSERT intended to be used in if statements.
// Expression is evaluated in both debug and release. Assertion only happens in debug.
#define VERIFYMSG(b, msgFmt, ...) ((b) || (_FailedAssert(__FILE__, __LINE__, #b, "" msgFmt, ##__VA_ARGS__), false))

#else

//-V:ASSERT:627
//-V:ASSERT:568
#define ASSERTMSG(b, msgFmt, ...) \
    do                            \
    {                             \
        ((void)sizeof(b));        \
    } while (0)
#define VERIFYMSG(b, msgFmt, ...) ((b) || (LOGF(eERROR, "VERIFY(" #b ") failed. " msgFmt, ##__VA_ARGS__), false))

#endif

#define ASSERT(b) ASSERTMSG((b), "")
#define VERIFY(b) VERIFYMSG((b), "")

// ASSERTFAIL() is ASSERT(false) that doesn't cause issues with PVS-Studio
#ifdef PVS_STUDIO
#define ASSERTFAIL(msgFmt, ...) \
    do                          \
    {                           \
    } while (0)
#else
#define ASSERTFAIL(msgFmt, ...) ASSERTMSG(false, msgFmt, ##__VA_ARGS__)
#endif

// Usage: LOGF(LogLevel::eINFO | LogLevel::eDEBUG, "Whatever string %s, this is an int %d", "This is a string", 1)
#define LOGF(log_level, ...) writeLog((log_level), __FILE__, __LINE__, __VA_ARGS__)
// Usage: LOGF_IF(LogLevel::eINFO | LogLevel::eDEBUG, boolean_value && integer_value == 5, "Whatever string %s, this is an int %d", "This is
// a string", 1)
#define LOGF_IF(log_level, condition, ...) \
    ((condition) ? writeLog((log_level), __FILE__, __LINE__, __VA_ARGS__) : (void)sizeof(condition)) //-V568
//
// #define LOGF_SCOPE(log_level, ...) LogLogScope ANONIMOUS_VARIABLE_LOG(scope_log_){ (log_level), __FILE__, __LINE__, __VA_ARGS__ }

// Usage: RAW_LOGF(LogLevel::eINFO | LogLevel::eDEBUG, "Whatever string %s, this is an int %d", "This is a string", 1)
#define RAW_LOGF(log_level, ...) writeRawLog((log_level), false, __VA_ARGS__)
// Usage: RAW_LOGF_IF(LogLevel::eINFO | LogLevel::eDEBUG, boolean_value && integer_value == 5, "Whatever string %s, this is an int %d",
// "This is a string", 1)
#define RAW_LOGF_IF(log_level, condition, ...) \
    ((condition) ? writeRawLog((log_level), false, __VA_ARGS__) : (void)sizeof(condition)) //-V568

#if defined(FORGE_DEBUG)

// Usage: DLOGF(LogLevel::eINFO | LogLevel::eDEBUG, "Whatever string %s, this is an int %d", "This is a string", 1)
#define DLOGF(log_level, ...)                   LOGF(log_level, __VA_ARGS__)
// Usage: DLOGF_IF(LogLevel::eINFO | LogLevel::eDEBUG, boolean_value && integer_value == 5, "Whatever string %s, this is an int %d", "This
// is a string", 1)
#define DLOGF_IF(log_level, condition, ...)     LOGF_IF(log_level, condition, __VA_ARGS__)

// Usage: DRAW_LOGF(LogLevel::eINFO | LogLevel::eDEBUG, "Whatever string %s, this is an int %d", "This is a string", 1)
#define DRAW_LOGF(log_level, ...)               RAW_LOGF(log_level, __VA_ARGS__)
// Usage: DRAW_LOGF_IF(LogLevel::eINFO | LogLevel::eDEBUG, boolean_value && integer_value == 5, "Whatever string %s, this is an int %d",
// "This is a string", 1)
#define DRAW_LOGF_IF(log_level, condition, ...) RAW_LOGF_IF(log_level, condition, __VA_ARGS__)

#else

// Usage: DLOGF(LogLevel::eINFO | LogLevel::eDEBUG, "Whatever string %s, this is an int %d", "This is a string", 1)
#define DLOGF(log_value, ...) \
    do                        \
    {                         \
    } while (0)
// Usage: DLOGF_IF(LogLevel::eINFO | LogLevel::eDEBUG, boolean_value && integer_value == 5, "Whatever string %s, this is an int %d", "This
// is a string", 1)
#define DLOGF_IF(log_value, condition, ...) \
    do                                      \
    {                                       \
        ((void)sizeof(condition));          \
    } while (0)

// Usage: DRAW_LOGF(LogLevel::eINFO | LogLevel::eDEBUG, "Whatever string %s, this is an int %d", "This is a string", 1)
#define DRAW_LOGF(log_level, ...) \
    do                            \
    {                             \
    } while (0)
// Usage: DRAW_LOGF_IF(LogLevel::eINFO | LogLevel::eDEBUG, boolean_value && integer_value == 5, "Whatever string %s, this is an int %d",
// "This is a string", 1)
#define DRAW_LOGF_IF(log_level, condition, ...) \
    do                                          \
    {                                           \
        ((void)sizeof(condition));              \
    } while (0)

#endif

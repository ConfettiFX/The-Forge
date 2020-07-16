/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#include "../../ThirdParty/OpenSource/EASTL/string.h"
#include "../../OS/Logging/Log.h"
#include "ITime.h"

void _FailedAssert(const char* file, int line, const char* statement);
void _OutputDebugString(const char* str, ...);
void _OutputDebugStringV(const char* str, va_list args);

void _PrintUnicode(const char* str, bool error = false);

#ifdef _WIN32
#define CHECK_HRESULT(exp)                                                     \
{                                                                              \
	HRESULT hres = (exp);                                                      \
	if (!SUCCEEDED(hres))                                                      \
	{                                                                          \
		LOGF(eERROR, "%s: FAILED with HRESULT: %u", #exp, (uint32_t)hres);     \
		ASSERT(false);                                                         \
	}                                                                          \
}
#endif

#if _MSC_VER >= 1400
// To make MSVC 2005 happy
#pragma warning(disable : 4996)
#define assume(x) __assume(x)
#define no_alias __declspec(noalias)
#else
#define assume(x)
#define no_alias
#endif

#if defined(FORGE_DEBUG)
#define IFASSERT(x) x

#if defined(_XBOX)
#else
#define ASSERT(b) \
	if (!(b)) _FailedAssert(__FILE__, __LINE__, #b)
#endif
#else

#define ASSERT(b) (void)(b)

#if _MSC_VER >= 1400
#define IFASSERT(x) x
#else
#define IFASSERT(x)
#endif
#endif

// Usage: LOGF(LogLevel::eINFO | LogLevel::eDEBUG, "Whatever string %s, this is an int %d", "This is a string", 1)
#define LOGF(log_level, ...) Log::Write((log_level), __FILE__, __LINE__, __VA_ARGS__)
// Usage: LOGF_IF(LogLevel::eINFO | LogLevel::eDEBUG, boolean_value && integer_value == 5, "Whatever string %s, this is an int %d", "This is a string", 1)
#define LOGF_IF(log_level, condition, ...) ((condition) ? Log::Write((log_level), __FILE__, __LINE__, __VA_ARGS__) : (void)0)
//
#define LOGF_SCOPE(log_level, ...) Log::LogScope ANONIMOUS_VARIABLE_LOG(scope_log_){ (log_level), __FILE__, __LINE__, __VA_ARGS__ }

// Usage: RAW_LOGF(LogLevel::eINFO | LogLevel::eDEBUG, "Whatever string %s, this is an int %d", "This is a string", 1)
#define RAW_LOGF(log_level, ...) Log::WriteRaw((log_level), false, __VA_ARGS__)
// Usage: RAW_LOGF_IF(LogLevel::eINFO | LogLevel::eDEBUG, boolean_value && integer_value == 5, "Whatever string %s, this is an int %d", "This is a string", 1)
#define RAW_LOGF_IF(log_level, condition, ...) ((condition) ? Log::WriteRaw((log_level), false, __VA_ARGS__))

#if defined(FORGE_DEBUG)

// Usage: DLOGF(LogLevel::eINFO | LogLevel::eDEBUG, "Whatever string %s, this is an int %d", "This is a string", 1)
#define DLOGF(log_level, ...) LOGF(log_level, __VA_ARGS__)
// Usage: DLOGF_IF(LogLevel::eINFO | LogLevel::eDEBUG, boolean_value && integer_value == 5, "Whatever string %s, this is an int %d", "This is a string", 1)
#define DLOGF_IF(log_level, condition, ...) LOGF_IF(log_level, condition, __VA_ARGS__)

// Usage: DRAW_LOGF(LogLevel::eINFO | LogLevel::eDEBUG, "Whatever string %s, this is an int %d", "This is a string", 1)
#define DRAW_LOGF(log_level, ...) RAW_LOGF(log_level, __VA_ARGS__)
// Usage: DRAW_LOGF_IF(LogLevel::eINFO | LogLevel::eDEBUG, boolean_value && integer_value == 5, "Whatever string %s, this is an int %d", "This is a string", 1)
#define DRAW_LOGF_IF(log_level, condition, ...) RAW_LOGF_IF(log_level, condition, __VA_ARGS__)

#else

// Usage: DLOGF(LogLevel::eINFO | LogLevel::eDEBUG, "Whatever string %s, this is an int %d", "This is a string", 1)
#define DLOGF(log_value, ...)
// Usage: DLOGF_IF(LogLevel::eINFO | LogLevel::eDEBUG, boolean_value && integer_value == 5, "Whatever string %s, this is an int %d", "This is a string", 1)
#define DLOGF_IF(log_value, condition, ...)

// Usage: DRAW_LOGF(LogLevel::eINFO | LogLevel::eDEBUG, "Whatever string %s, this is an int %d", "This is a string", 1)
#define DRAW_LOGF(log_level, ...)
// Usage: DRAW_LOGF_IF(LogLevel::eINFO | LogLevel::eDEBUG, boolean_value && integer_value == 5, "Whatever string %s, this is an int %d", "This is a string", 1)
#define DRAW_LOGF_IF(log_level, condition, ...)

#endif

//====================
//Taken from Gladiator
//====================

#ifndef USE_LOGGING
#define USE_LOGGING 0
#endif

#ifdef USE_LOGGING
#define LOGDEBUG(message) LogManager::Write(LogLevel::eDEBUG, __FILE__, __LINE__, message)
#define LOGINFO(message) LogManager::Write(LogLevel::eINFO, __FILE__, __LINE__, message)
#define LOGWARNING(message) LogManager::Write(LogLevel::eWARNING, __FILE__, __LINE__, message)
#define LOGERROR(message) LogManager::Write(LogLevel::eERROR, __FILE__, __LINE__, message)
#define LOGRAW(message) LogManager::WriteRaw(LogLevel::eINFO, false, message)
#define LOGDEBUGF(format, ...) LogManager::Write(LogLevel::eDEBUG, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOGINFOF(format, ...) LogManager::Write(LogLevel::eINFO, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOGWARNINGF(format, ...) LogManager::Write(LogLevel::eWARNING, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOGERRORF(format, ...) LogManager::Write(LogLevel::eERROR, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOGRAWF(format, ...) LogManager::WriteRaw(LogLevel::eINFO, false, format, ##__VA_ARGS__)
#else
#define LOGDEBUG(message) ((void)0)
#define LOGINFO(message) ((void)0)
#define LOGWARNING(message) ((void)0)
#define LOGERROR(message) ((void)0)
#define LOGRAW(message) ((void)0)
#define LOGDEBUGF(...) ((void)0)
#define LOGINFOF(...) ((void)0)
#define LOGWARNINGF(...) ((void)0)
#define LOGERRORF(...) ((void)0)
#define LOGRAWF(...) ((void)0)
#endif

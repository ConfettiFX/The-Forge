/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#include "../../ThirdParty/OpenSource/TinySTL/string.h"
#include "../../OS/Logging/LogManager.h"
#include "ITimeManager.h"

#ifndef USE_LOGGING
#define USE_LOGGING 1
#endif

void _ErrorMsg(int line, const char*, const char* string, ...);
void _WarningMsg(int line, const char*, const char* string, ...);
void _InfoMsg(int line, const char*, const char* string, ...);
void _FailedAssert(const char* file, int line, const char* statement);
void _OutputDebugString(const char* str, ...);

void _PrintUnicode(const tinystl::string& str, bool error = false);
void _PrintUnicodeLine(const tinystl::string& str, bool error = false);

#define ErrorMsg(str, ...) _ErrorMsg(__LINE__, __FILE__, str, ##__VA_ARGS__)
#define WarningMsg(str, ...) _WarningMsg(__LINE__, __FILE__, str, ##__VA_ARGS__)
#define InfoMsg(str, ...) _InfoMsg(__LINE__, __FILE__, str, ##__VA_ARGS__)

#if _MSC_VER >= 1400
// To make MSVC 2005 happy
#pragma warning(disable : 4996)
#define assume(x) __assume(x)
#define no_alias __declspec(noalias)
#else
#define assume(x)
#define no_alias
#endif

#ifdef _DEBUG
#define IFASSERT(x) x

#if defined(_XBOX)

#elif defined(ORBIS)
// there is a large amount of stuff included via header files ...
#define ASSERT(cond) SCE_GNM_ASSERT(cond)
#else
#define ASSERT(b) \
	if (b) {}     \
	else          \
		_FailedAssert(__FILE__, __LINE__, #b)
#endif
#else
#define ASSERT(b) assume(b)
#if _MSC_VER >= 1400
#define IFASSERT(x) x
#else
#define IFASSERT(x)
#endif
#endif    // DEBUG
#ifdef USE_LOGGING
#define LOGDEBUG(message) LogManager::Write(LogLevel::LL_Debug, ToString(__FUNCTION__, message, ""))
#define LOGINFO(message) LogManager::Write(LogLevel::LL_Info, ToString(__FUNCTION__, message, ""))
#define LOGWARNING(message) LogManager::Write(LogLevel::LL_Warning, ToString(__FUNCTION__, message, ""))
#define LOGERROR(message) LogManager::Write(LogLevel::LL_Error, ToString(__FUNCTION__, message, ""))
#define LOGRAW(message) LogManager::WriteRaw(ToString(__FUNCTION__, message, ""))
#define LOGDEBUGF(format, ...) LogManager::Write(LogLevel::LL_Debug, ToString(__FUNCTION__, format, ##__VA_ARGS__))
#define LOGINFOF(format, ...) LogManager::Write(LogLevel::LL_Info, ToString(__FUNCTION__, format, ##__VA_ARGS__))
#define LOGWARNINGF(format, ...) LogManager::Write(LogLevel::LL_Warning, ToString(__FUNCTION__, format, ##__VA_ARGS__))
#define LOGERRORF(format, ...) LogManager::Write(LogLevel::LL_Error, ToString(__FUNCTION__, format, ##__VA_ARGS__))
#define LOGRAWF(format, ...) LogManager::WriteRaw(ToString(__FUNCTION__, format, ##__VA_ARGS__))
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

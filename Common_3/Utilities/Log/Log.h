/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../../Utilities/Interfaces/IFileSystem.h"

#include "stdbool.h"

#ifndef FILENAME_NAME_LENGTH_LOG
#define FILENAME_NAME_LENGTH_LOG 23
#endif

#ifndef INDENTATION_SIZE_LOG
#define INDENTATION_SIZE_LOG 4
#endif

#ifndef LEVELS_LOG
#define LEVELS_LOG 6
#endif

#define CONCAT_STR_LOG_IMPL(a, b) a##b
#define CONCAT_STR_LOG(a, b) CONCAT_STR_LOG_IMPL(a, b)

#ifndef ANONIMOUS_VARIABLE_LOG
#define ANONIMOUS_VARIABLE_LOG(str) CONCAT_STR_LOG(str, __LINE__)
#endif

// If you add more levels don't forget to change LOG_LEVELS macro to the actual number of levels
typedef enum LogLevel
{
	eNONE = 0,
	eRAW = 1,
	eDEBUG = 2,
	eINFO = 4,
	eWARNING = 8,
	eERROR = 16,
	eALL = ~0
} LogLevel;

typedef void (*LogCallbackFn)(void* user_data, const char* message);
typedef void (*LogCloseFn)(void* user_data);
typedef void (*LogFlushFn)(void* user_data);

#ifdef __cplusplus
extern "C"
{
#endif
	// Initialization/Exit functions are thread unsafe
	FORGE_API void initLog(const char* appName, LogLevel level);
	FORGE_API void exitLog(void);

	FORGE_API void addLogFile(const char* filename, FileMode file_mode, LogLevel log_level);
	FORGE_API void addLogCallback(const char* id, uint32_t log_level, void* user_data, LogCallbackFn callback, LogCloseFn close, LogFlushFn flush);

	FORGE_API void writeLogVaList(uint32_t level, const char* filename, int line_number, const char* message, va_list args);
	FORGE_API void writeLog(uint32_t level, const char* filename, int line_number, const char* message, ...);
	FORGE_API void writeRawLog(uint32_t level, bool error, const char* message, ...);
#ifdef __cplusplus
}    // extern "C"
#endif
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

#include "../../ThirdParty/OpenSource/EASTL/vector.h"
#include "../../ThirdParty/OpenSource/EASTL/string.h"

#include "../../OS/Interfaces/IThread.h"
#include "../../OS/Interfaces/IFileSystem.h"

#ifndef FILENAME_NAME_LENGTH_LOG
#define FILENAME_NAME_LENGTH_LOG 23
#endif

#ifndef INDENTATION_SIZE_LOG
#define INDENTATION_SIZE_LOG 4
#endif

#ifndef LEVELS_LOG
#define LEVELS_LOG 6
#endif

#define CONCAT_STR_LOG_IMPL(a, b) a ## b
#define CONCAT_STR_LOG(a, b) CONCAT_STR_LOG_IMPL(a, b)

#ifndef ANONIMOUS_VARIABLE_LOG
#define ANONIMOUS_VARIABLE_LOG(str) CONCAT_STR_LOG(str, __LINE__)
#endif

// If you add more levels don't forget to change LOG_LEVELS macro to the actual number of levels
enum LogLevel
{
	eNONE = 0,
	eRAW = 1,
	eDEBUG = 2,
	eINFO = 4,
	eWARNING = 8,
	eERROR = 16,
	eALL = ~0
};


typedef void(*log_callback_t)(void * user_data, const char* message);
typedef void(*log_close_t)(void * user_data);
typedef void(*log_flush_t)(void * user_data);

/// Logging subsystem.
class Log
{
public:
	struct LogScope
	{
		LogScope(uint32_t log_level, const char * file, int line, const char * format, ...);
		~LogScope();

		eastl::string mMessage;
		const char * mFile;
		int mLine;
		uint32_t mLevel;
	};

	Log(const char* appName, LogLevel level = LogLevel::eALL);
	~Log();

	static void Init(const char* appName, LogLevel level = LogLevel::eALL);
	static void Exit();

	static void SetLevel(LogLevel level);
	static void SetQuiet(bool bQuiet);
	static void SetTimeStamp(bool bEnable);
	static void SetRecordingFile(bool bEnable);
	static void SetRecordingThreadName(bool bEnable);
	static void SetConsoleLogging(bool bEnable);

	static uint32_t        GetLevel();
	static eastl::string   GetLastMessage();
	static bool            IsQuiet();
	static bool            IsRecordingTimeStamp();
	static bool            IsRecordingFile();
	static bool            IsRecordingThreadName();

	static void AddFile(const char * filename, FileMode file_mode, LogLevel log_level);
	static void AddCallback(const char * id, uint32_t log_level, void * user_data, log_callback_t callback, log_close_t close = nullptr, log_flush_t flush = nullptr);

	static void Write(uint32_t level, const char * filename, int line_number, const char* message, ...);
	static void WriteRaw(uint32_t level, bool error, const char* message, ...);

private:
	static void AddInitialLogFile(const char* appName);
	static uint32_t WritePreamble(char * buffer, uint32_t buffer_size, const char * file, int line);
	static bool CallbackExists(const char * id);

	// Singleton
	Log(const Log &) = delete;
	Log(Log &&) = delete;
	Log & operator=(const Log &) = delete;
	Log & operator=(Log &&) = delete;

	struct LogCallback
	{
		LogCallback(const eastl::string & id, void * user_data, log_callback_t callback, log_close_t close, log_flush_t flush, uint32_t level)
			: mID(id)
			, mUserData(user_data)
			, mCallback(callback)
			, mClose(close)
			, mFlush(flush)
			, mLevel(level)
		 { }
		
		eastl::string mID;
		void * mUserData;
		log_callback_t mCallback;
		log_close_t mClose = nullptr;
		log_flush_t mFlush = nullptr;
		uint32_t mLevel;
	};

	eastl::vector<LogCallback> mCallbacks;
	/// Mutex for threaded operation.
	Mutex           mLogMutex;
	uint32_t        mLogLevel;
	uint32_t        mIndentation;
	bool            mQuietMode;
	bool            mRecordTimestamp;
	bool            mRecordFile;
	bool            mRecordThreadName;

	enum{MAX_BUFFER=1024};

	static thread_local char Buffer[MAX_BUFFER+2];
	static bool sConsoleLogging;
};

eastl::string ToString(const char* formatString, ...);

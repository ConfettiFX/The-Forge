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

#include "../../ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../ThirdParty/OpenSource/TinySTL/string.h"

#include "../../OS/Interfaces/IThread.h"

enum LogLevel
{
	LL_Raw = -1,
	LL_Debug,
	LL_Info,
	LL_Warning,
	LL_Error,
	LL_None,
};

class File;

/// Logging subsystem.
class LogManager
{
	public:
	LogManager(
		LogLevel level =
#ifdef _DEBUG
			LogLevel::LL_Debug
#else
			LogLevel::LL_Info
#endif
	);
	~LogManager();

	void Open(const tinystl::string& fileName);
	void Close();

	void SetLevel(LogLevel level);
	void SetTimeStamp(bool enable);
	void SetQuiet(bool quiet);

	LogLevel        GetLevel() const { return mLogLevel; }
	bool            GetTimeStamp() const { return mRecordTimestamp; }
	tinystl::string GetLastMessage() const { return mLastMessage; }
	bool            IsQuiet() const { return mQuietMode; }

	virtual void OutputLog(int level, const tinystl::string& message);

	static void Write(int level, const tinystl::string& message);
	static void WriteRaw(const tinystl::string& message, bool error = false);

	private:
	/// Mutex for threaded operation.
	Mutex           mLogMutex;
	File*           pLogFile;
	tinystl::string mLastMessage;
	LogLevel        mLogLevel;
	bool            mRecordTimestamp;
	bool            mInWrite;
	bool            mQuietMode;
};

tinystl::string ToString(const char* formatString, const char* function, ...);

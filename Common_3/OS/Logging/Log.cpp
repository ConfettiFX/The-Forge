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

#include "Log.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../../ThirdParty/OpenSource/EASTL/unordered_map.h"

#include "../Interfaces/IMemory.h"

#define LOG_PREAMBLE_SIZE (56 + MAX_THREAD_NAME_LENGTH + FILENAME_NAME_LENGTH_LOG)

static Log gLogger;

// Fill unordered map for log level prefixes
static eastl::unordered_map<uint32_t, eastl::string> logLevelPrefixes{
	eastl::pair<uint32_t, eastl::string>{ LogLevel::eWARNING, eastl::string{ "WARN" } },
	eastl::pair<uint32_t, eastl::string>{ LogLevel::eINFO, eastl::string{ "INFO" } },
	eastl::pair<uint32_t, eastl::string>{ LogLevel::eDEBUG, eastl::string{ " DBG" } },
	eastl::pair<uint32_t, eastl::string>{ LogLevel::eERROR, eastl::string{ " ERR" } }
};
static bool gOnce = true;

eastl::string GetTimeStamp()
{
	time_t sysTime;
	time(&sysTime);
	eastl::string dateTime = ctime(&sysTime);
	eastl::replace(dateTime.begin(), dateTime.end(), '\n', ' ');
	return dateTime;
}

// Returns the part of the path after the last / or \ (if any).
const char* get_filename(const char* path)
{
	for (auto ptr = path; *ptr; ++ptr) {
		if (*ptr == '/' || *ptr == '\\') {
			path = ptr + 1;
		}
	}
	return path;
}

// Default callback
void log_write(void * user_data, const eastl::string & message)
{
	File * file = static_cast<File *>(user_data);
	file->WriteLine(message);
	file->Flush();
}

// Close callback
void log_close(void * user_data)
{
	File * file = static_cast<File *>(user_data);
	file->Close();
	file->~File();
	conf_free(file);
}

// Flush callback
void log_flush(void * user_data)
{
	File * file = static_cast<File *>(user_data);
	file->Flush();
}

Log::LogScope::LogScope(uint32_t log_level, const char * file, int line, const char * format, ...)
	: mFile(file)
	, mLine(line)
	, mLevel(log_level)
{
	const unsigned BUFFER_SIZE = 4096;
	char           buf[BUFFER_SIZE];
	va_list arglist;
	va_start(arglist, format);
	vsprintf_s(buf, BUFFER_SIZE, format, arglist);
	va_end(arglist);
	mMessage = buf;

	// Write to log and update indentation
	Log::Write(mLevel, "{ " + mMessage, mFile, mLine);
	{
		MutexLock lock{ gLogger.mLogMutex };
		++gLogger.mIndentation;
	}
}

Log::LogScope::~LogScope()
{
	// Update indentation and write to log
	{
		MutexLock lock{ gLogger.mLogMutex };
		--gLogger.mIndentation;
	}
	Log::Write(mLevel, "} " + mMessage, mFile, mLine);
}

// Settors
void Log::SetLevel(LogLevel level)             { gLogger.mLogLevel = level; }
void Log::SetQuiet(bool bQuiet)                { gLogger.mQuietMode = bQuiet; }
void Log::SetTimeStamp(bool bEnable)           { gLogger.mRecordTimestamp = bEnable; }
void Log::SetRecordingFile(bool bEnable)       { gLogger.mRecordFile = bEnable; }
void Log::SetRecordingThreadName(bool bEnable) { gLogger.mRecordThreadName = bEnable; }

// Gettors
uint32_t Log::GetLevel()            { return gLogger.mLogLevel; }
eastl::string Log::GetLastMessage() { return gLogger.mLastMessage; }
bool Log::IsQuiet()                 { return gLogger.mQuietMode; }
bool Log::IsRecordingTimeStamp()    { return gLogger.mRecordTimestamp; }
bool Log::IsRecordingFile()         { return gLogger.mRecordFile; }
bool Log::IsRecordingThreadName()   { return gLogger.mRecordThreadName; }

void Log::AddFile(const char * filename, FileMode file_mode, LogLevel log_level)
{
	if (filename == 0)
		return;
	
	File * file = conf_placement_new<File>(conf_calloc(1, sizeof(File)));
	if (file->Open(filename, file_mode, FSR_Absolute))
	{
		// AddCallback will try to acquire mutex
		AddCallback((FileSystem::GetCurrentDir() + filename).c_str(), log_level, file, log_write, log_close, log_flush);

		{
			MutexLock lock{ gLogger.mLogMutex }; // scope lock as Write will try to acquire mutex

			// Header
			eastl::string header;
			if (gLogger.mRecordTimestamp)
				header += "date       time     ";
			if (gLogger.mRecordThreadName)
				header += "[thread name/id ]";
			if (gLogger.mRecordFile)
				header += "                   file:line  ";
			header += "  v |\n";
			file->Write(header.c_str(), (unsigned)header.size());
			file->Flush();
		}

		Write(LogLevel::eINFO, "Opened log file " + eastl::string{ filename }, __FILE__, __LINE__);
	}
	else
	{
		file->~File();
		conf_free(file);
		Write(LogLevel::eERROR, "Failed to create log file " + eastl::string{ filename }, __FILE__, __LINE__); // will try to acquire mutex
	}
}

void Log::AddCallback(const char * id, uint32_t log_level, void * user_data, log_callback_t callback, log_close_t close, log_flush_t flush)
{
	MutexLock lock{ gLogger.mLogMutex };
	if (!CallbackExists(id))
	{
		gLogger.mCallbacks.emplace_back(LogCallback{ id, user_data, callback, close, flush, log_level });
	}
	else
		close(user_data);
}

void Log::Write(uint32_t level, const eastl::string & message, const char * filename, int line_number)
{
	eastl::string log_level_strings[LEVELS_LOG];
	uint32_t log_levels[LEVELS_LOG];
	uint32_t log_level_count = 0;

	// Check flags
	for (eastl::unordered_map<uint32_t, eastl::string>::iterator it = logLevelPrefixes.begin(); it != logLevelPrefixes.end(); ++it)
	{
		if (it->first & level)
		{
			log_level_strings[log_level_count] = it->second + "| ";
			log_levels[log_level_count] = it->first;
			++log_level_count;
		}
	}
	
	bool do_once = false;
	{
		MutexLock lock{ gLogger.mLogMutex }; // scope lock as stack frames from calling AddInitialLogFile will attempt to lock mutex
		do_once = gOnce;
		gOnce = false;
	}
	if (do_once)
		AddInitialLogFile();

	MutexLock lock{ gLogger.mLogMutex };
	gLogger.mLastMessage = message;

	char preamble[LOG_PREAMBLE_SIZE] = { 0 };
	WritePreamble(preamble, LOG_PREAMBLE_SIZE, filename, line_number);

	// Prepare indentation
	eastl::string indentation;
	indentation.resize(gLogger.mIndentation * INDENTATION_SIZE_LOG);
	memset(indentation.begin(), ' ', indentation.size());

	// Log for each flag
	for (uint32_t i = 0; i < log_level_count; ++i)
	{
        eastl::string formattedMessage = preamble + log_level_strings[i] + indentation + message;

		if (gLogger.mQuietMode)
		{
			if (level & LogLevel::eERROR)
				_PrintUnicodeLine(formattedMessage, true);
		}
		else
		{
			_PrintUnicodeLine(formattedMessage, level & LogLevel::eERROR);
		}
	}
	
	for (LogCallback & callback : gLogger.mCallbacks)
	{
		// Log for each flag
		for (uint32_t i = 0; i < log_level_count; ++i)
		{
			if (callback.mLevel & log_levels[i])
				callback.mCallback(callback.mUserData, preamble + log_level_strings[i] + indentation + message);
		}
	}
}

void Log::WriteRaw(uint32_t level, const eastl::string & message, bool error)
{
	bool do_once = false;
	{
		MutexLock lock{ gLogger.mLogMutex }; // scope lock as stack frames from calling AddInitialLogFile will attempt to lock mutex
		do_once = gOnce;
		gOnce = false;
	}
	if (do_once)
		AddInitialLogFile();
	
	MutexLock lock{ gLogger.mLogMutex };
	
	gLogger.mLastMessage = message;
	
	if (gLogger.mQuietMode)
	{
		if (error)
			_PrintUnicode(message, true);
	}
	else
		_PrintUnicode(message, error);
	
	for (LogCallback & callback : gLogger.mCallbacks)
	{
		if (callback.mLevel & level)
			callback.mCallback(callback.mUserData, message);
	}
}

void Log::AddInitialLogFile()
{
	// Add new file with executable name
	eastl::string exeFileName = FileSystem::GetProgramFileName();
	//Minimum Length check
	if (exeFileName.size() < 2)
		exeFileName = "Log";
	AddFile((exeFileName + ".log").c_str(), FileMode::FM_WriteBinary, LogLevel::eALL);
}

void Log::WritePreamble(char * buffer, uint32_t buffer_size, const char * file, int line)
{
	time_t  t = time(NULL);
	tm time_info;
#ifdef _WIN32
	localtime_s(&time_info, &t);
#else
	localtime_r(&t, &time_info);
#endif

	uint32_t pos = 0;
	// Date and time
	if (gLogger.mRecordTimestamp && pos < buffer_size)
	{
		pos += snprintf(buffer + pos, buffer_size - pos, "%04d-%02d-%02d ",
			1900 + time_info.tm_year, 1 + time_info.tm_mon, time_info.tm_mday);
		pos += snprintf(buffer + pos, buffer_size - pos, "%02d:%02d:%02d ",
			time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
	}

	if (gLogger.mRecordThreadName && pos < buffer_size)
	{
		char thread_name[MAX_THREAD_NAME_LENGTH + 1] = { 0 };
		Thread::GetCurrentThreadName(thread_name, MAX_THREAD_NAME_LENGTH + 1);

		// No thread name
		if (thread_name[0] == 0)
			snprintf(thread_name, MAX_THREAD_NAME_LENGTH + 1, "NoName");

		pos += snprintf(buffer + pos, buffer_size - pos, "[%-15s]", thread_name);
	}

	// File and line
	if (gLogger.mRecordFile && pos < buffer_size)
	{
		file = get_filename(file);

		char shortened_filename[FILENAME_NAME_LENGTH_LOG + 1];
		snprintf(shortened_filename, FILENAME_NAME_LENGTH_LOG + 1, "%s", file);
		pos += snprintf(buffer + pos, buffer_size - pos, " %22s:%-5u ", shortened_filename, line);
	}
}

bool Log::CallbackExists(const char * id)
{
	for (const LogCallback & callback : gLogger.mCallbacks)
	{
		if (callback.mID == id)
			return true;
	}

	return false;
}

Log::Log(LogLevel level)
	: mLogLevel(level)
	, mIndentation(0)
	, mQuietMode(false)
	, mRecordTimestamp(true)
	, mRecordFile(true)
	, mRecordThreadName(true)
{
	Thread::SetMainThread();
	Thread::SetCurrentThreadName("MainThread");
}

Log::~Log()
{
	for (LogCallback & callback : mCallbacks)
	{
		if (callback.mClose)
			callback.mClose(callback.mUserData);
	}
	
	mCallbacks.clear();
}

eastl::string ToString(const char* format, ...)
{
	const unsigned BUFFER_SIZE = 4096;
	char           buf[BUFFER_SIZE];

	va_list arglist;
	va_start(arglist, format);
	vsprintf_s(buf, BUFFER_SIZE, format, arglist);
	va_end(arglist);

	return eastl::string(buf);
}

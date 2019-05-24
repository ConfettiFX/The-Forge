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

#include "LogManager.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../../ThirdParty/OpenSource/TinySTL/unordered_map.h"

#include "../Interfaces/IMemoryManager.h"

#define LOG_PREAMBLE_SIZE (56 + MAX_THREAD_NAME_LENGTH + FILENAME_NAME_LENGTH_LOG)

tinystl::unordered_map<uint32_t, tinystl::string> logLevelPrefixes{};

tinystl::string GetTimeStamp()
{
	time_t sysTime;
	time(&sysTime);
	tinystl::string dateTime = ctime(&sysTime);
	dateTime.replace('\n', ' ');
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
void log_write(void * user_data, const tinystl::string & message)
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

LogManager::LogScope::LogScope(uint32_t log_level, const char * file, int line, const char * format, ...)
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
	LogManager::Write(mLevel, "{ " + mMessage, mFile, mLine);
	{
		LogManager & log = LogManager::Get();
		MutexLock lock{ log.mLogMutex };
		++log.mIndentation;
	}
}

LogManager::LogScope::~LogScope()
{
	// Update indentation and write to log
	{
		LogManager & log = LogManager::Get();
		MutexLock lock{ log.mLogMutex };
		--log.mIndentation;
	}
	LogManager::Write(mLevel, "} " + mMessage, mFile, mLine);
}

void LogManager::AddFile(const char * filename, FileMode file_mode, LogLevel log_level)
{
	AddFile(Get(), filename, file_mode, log_level);
}

void LogManager::AddCallback(const char * id, uint32_t log_level, void * user_data, log_callback_t callback, log_close_t close, log_flush_t flush)
{
	AddCallback(Get(), id, log_level, user_data, callback, close, flush);
}

void LogManager::SetLevel(LogLevel level)
{
	Get().mLogLevel = level;
}

void LogManager::SetQuiet(bool quiet) { Get().mQuietMode = quiet; }

void LogManager::SetTimeStamp(bool enable) { Get().mRecordTimestamp = enable; }

void LogManager::SetRecordingFile(bool enable) { Get().mRecordFile = enable; }

void LogManager::SetRecordingThreadName(bool enable) { Get().mRecordThreadName = enable; }

void LogManager::Write(uint32_t level, const tinystl::string& message, const char * filename, int line_number)
{
	Write(Get(), level, message, filename, line_number);
}

void LogManager::WriteRaw(uint32_t level, const tinystl::string& message, bool error)
{
	WriteRaw(Get(), level, message, error);
}

LogManager & LogManager::Get()
{
	static LogManager logger;
	return logger;
}

void LogManager::AddFile(LogManager & log, const char * filename, FileMode file_mode, LogLevel log_level)
{
	if (filename == 0)
		return;
	
	File * file = conf_placement_new<File>(conf_calloc(1, sizeof(File)));
	if (file->Open(filename, file_mode, FSR_Absolute))
	{
		AddCallback(log, FileSystem::GetCurrentDir() + filename, log_level, file, log_write, log_close, log_flush);

		// Header
		tinystl::string header;
		if (log.mRecordTimestamp)
			header += "date       time     "; 
		if (log.mRecordThreadName)
			header += "[thread name/id ]";
		if (log.mRecordFile)
			header += "                   file:line  ";
		header += "  v |\n";
		file->Write(header.c_str(), (unsigned)header.size());
		file->Flush();

		Write(log, LogLevel::eINFO, "Opened log file " + tinystl::string{ filename }, __FILE__, __LINE__);
	}
	else
	{
		file->~File();
		conf_free(file);
		Write(log, LogLevel::eERROR, "Failed to create log file " + tinystl::string{ filename }, __FILE__, __LINE__);
	}
}

void LogManager::AddCallback(LogManager & log, const char * id, uint32_t log_level, void * user_data, log_callback_t callback, log_close_t close, log_flush_t flush)
{
	MutexLock lock{ log.mLogMutex };
	if (!CallbackExists(log, id))
	{
		log.mCallbacks.emplace_back(LogCallback{ id, user_data, callback, close, flush, log_level });
	}
	else
		close(user_data);
}

void LogManager::Write(LogManager & log, uint32_t level, const tinystl::string & message, const char * filename, int line_number)
{
	tinystl::string log_level_strings[LEVELS_LOG];
	uint32_t log_levels[LEVELS_LOG];
	uint32_t log_level_count = 0;

	// Check flags
	for (tinystl::unordered_map<uint32_t, tinystl::string>::iterator it = logLevelPrefixes.begin(); it != logLevelPrefixes.end(); ++it)
	{
		if (it->first & level)
		{
			log_level_strings[log_level_count] = it->second + "| ";
			log_levels[log_level_count] = it->first;
			++log_level_count;
		}
	}
	
	MutexLock lock{ log.mLogMutex };

	log.mLastMessage = message;

	char preamble[LOG_PREAMBLE_SIZE] = { 0 };
	WritePreamble(log, preamble, LOG_PREAMBLE_SIZE, filename, line_number);

	// Prepare indentation
	tinystl::string indentation;
	indentation.resize(log.mIndentation * INDENTATION_SIZE_LOG);
	memset(indentation.begin(), ' ', indentation.size());

	// Log for each flag
	for (uint32_t i = 0; i < log_level_count; ++i)
	{
        tinystl::string formattedMessage = preamble + log_level_strings[i] + indentation + message;

		if (log.mQuietMode)
		{
			if (level & LogLevel::eERROR)
				_PrintUnicodeLine(formattedMessage, true);
		}
		else
		{
			_PrintUnicodeLine(formattedMessage, level & LogLevel::eERROR);
		}
	}
	
	for (LogCallback & callback : log.mCallbacks)
	{
		// Log for each flag
		for (uint32_t i = 0; i < log_level_count; ++i)
		{
			if (callback.mLevel & log_levels[i])
				callback.mCallback(callback.mUserData, preamble + log_level_strings[i] + indentation + message);
		}
	}
}

void LogManager::WriteRaw(LogManager & log, uint32_t level, const tinystl::string & message, bool error)
{
	MutexLock lock{ log.mLogMutex };
	
	log.mLastMessage = message;
	
	if (log.mQuietMode)
	{
		if (error)
			_PrintUnicode(message, true);
	}
	else
		_PrintUnicode(message, error);
	
	for (LogCallback & callback : log.mCallbacks)
	{
		if (callback.mLevel & level)
			callback.mCallback(callback.mUserData, message);
	}
}

void LogManager::WritePreamble(LogManager & log, char * buffer, uint32_t buffer_size, const char * file, int line)
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
	if (log.mRecordTimestamp && pos < buffer_size)
	{
		pos += snprintf(buffer + pos, buffer_size - pos, "%04d-%02d-%02d ",
			1900 + time_info.tm_year, 1 + time_info.tm_mon, time_info.tm_mday);
		pos += snprintf(buffer + pos, buffer_size - pos, "%02d:%02d:%02d ",
			time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
	}

	if (log.mRecordThreadName && pos < buffer_size)
	{
		char thread_name[MAX_THREAD_NAME_LENGTH + 1] = { 0 };
		Thread::GetCurrentThreadName(thread_name, MAX_THREAD_NAME_LENGTH + 1);

		// No thread name
		if (thread_name[0] == 0)
			snprintf(thread_name, MAX_THREAD_NAME_LENGTH + 1, "NoName");

		pos += snprintf(buffer + pos, buffer_size - pos, "[%-*s]",
			MAX_THREAD_NAME_LENGTH, thread_name);
	}

	// File and line
	if (log.mRecordFile && pos < buffer_size)
	{
		file = get_filename(file);

		char shortened_filename[FILENAME_NAME_LENGTH_LOG + 1];
		snprintf(shortened_filename, FILENAME_NAME_LENGTH_LOG + 1, "%s", file);
		pos += snprintf(buffer + pos, buffer_size - pos, "%*s:%-5u ",
			FILENAME_NAME_LENGTH_LOG, shortened_filename, line);
	}
}

bool LogManager::CallbackExists(const LogManager & log, const char * id)
{
	for (const LogCallback & callback : log.mCallbacks)
	{
		if (callback.mID == id)
			return true;
	}

	return false;
}

LogManager::LogManager(LogLevel level)
	: mLogLevel(level)
	, mIndentation(0)
	, mQuietMode(false)
	, mRecordTimestamp(true)
	, mRecordFile(true)
	, mRecordThreadName(true)
{
	Thread::SetMainThread();
	Thread::SetCurrentThreadName("MainThread");

	// Fill unordered map for log level prefixes
	logLevelPrefixes.insert(tinystl::pair<uint32_t, tinystl::string>{ LogLevel::eWARNING, tinystl::string{ "WARN" } });
	logLevelPrefixes.insert(tinystl::pair<uint32_t, tinystl::string>{ LogLevel::eINFO, tinystl::string{ "INFO" } });
	logLevelPrefixes.insert(tinystl::pair<uint32_t, tinystl::string>{ LogLevel::eDEBUG, tinystl::string{ " DBG" } });
	logLevelPrefixes.insert(tinystl::pair<uint32_t, tinystl::string>{ LogLevel::eERROR, tinystl::string{ " ERR" } });

	tinystl::string exeFileName = FileSystem::GetProgramFileName();
	//Minimum Length check
	if (exeFileName.size() < 2)
		exeFileName = "Log";
	AddFile(*this, exeFileName + ".log", FileMode::FM_WriteBinary, LogLevel::eALL);
}

LogManager::~LogManager()
{
	for (LogCallback & callback : mCallbacks)
	{
		if (callback.mClose)
			callback.mClose(callback.mUserData);
	}
	
	mCallbacks.clear();
	logLevelPrefixes.clear();
}

tinystl::string ToString(const char* format, ...)
{
	const unsigned BUFFER_SIZE = 4096;
	char           buf[BUFFER_SIZE];

	va_list arglist;
	va_start(arglist, format);
	vsprintf_s(buf, BUFFER_SIZE, format, arglist);
	va_end(arglist);

	return tinystl::string(buf);
}

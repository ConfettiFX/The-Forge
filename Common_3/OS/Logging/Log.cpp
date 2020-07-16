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

#include "Log.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../../ThirdParty/OpenSource/EASTL/unordered_map.h"

#include "../Interfaces/IMemory.h"

#define LOG_PREAMBLE_SIZE (56 + MAX_THREAD_NAME_LENGTH + FILENAME_NAME_LENGTH_LOG)
#define LOG_LEVEL_SIZE 6
#define LOG_MESSAGE_OFFSET (LOG_PREAMBLE_SIZE + LOG_LEVEL_SIZE)

static Log* pLogger = NULL;
static bool gOnce = true;

thread_local char Log::Buffer[MAX_BUFFER+2];
bool Log::sConsoleLogging = true;

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
void log_write(void * user_data, const char* message)
{
	FileStream* fh = (FileStream*)user_data;
    ASSERT(fh);
    
    fsWriteToStreamString(fh, message);
    fsFlushStream(fh);
}

// Close callback
void log_close(void * user_data)
{
    FileStream* fh = (FileStream*)user_data;
    ASSERT(fh);
    fsCloseStream(fh);
}

// Flush callback
void log_flush(void * user_data)
{
    FileStream* fh = (FileStream*)user_data;
    ASSERT(fh);
    
    fsFlushStream(fh);
}

void Log::Init(LogLevel level /* = LogLevel::eALL */)
{
	if (!pLogger)
	{
		pLogger = conf_new(Log, level);
		pLogger->mLogMutex.Init();
	}
}

void Log::Exit()
{
	pLogger->mLogMutex.Destroy();
	conf_delete(pLogger);
	pLogger = NULL;
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
	Log::Write(mLevel, mFile, mLine, "{ %s", buf);
	{
		MutexLock lock{ pLogger->mLogMutex };
		++pLogger->mIndentation;
	}
}

Log::LogScope::~LogScope()
{
	// Update indentation and write to log
	{
		MutexLock lock{ pLogger->mLogMutex };
		--pLogger->mIndentation;
	}
	Log::Write(mLevel, mFile, mLine, "} %s", mMessage.c_str());
}

void Log::SetLevel(LogLevel level)             { pLogger->mLogLevel = level; }
void Log::SetQuiet(bool bQuiet)                { pLogger->mQuietMode = bQuiet; }
void Log::SetTimeStamp(bool bEnable)           { pLogger->mRecordTimestamp = bEnable; }
void Log::SetRecordingFile(bool bEnable)       { pLogger->mRecordFile = bEnable; }
void Log::SetRecordingThreadName(bool bEnable) { pLogger->mRecordThreadName = bEnable; }
void Log::SetConsoleLogging(bool bEnable)      { pLogger->sConsoleLogging = bEnable; } // @CONFFX: Change by Koste: Controllable console logging

// Gettors
uint32_t Log::GetLevel()            { return pLogger->mLogLevel; }
bool Log::IsQuiet()                 { return pLogger->mQuietMode; }
bool Log::IsRecordingTimeStamp()    { return pLogger->mRecordTimestamp; }
bool Log::IsRecordingFile()         { return pLogger->mRecordFile; }
bool Log::IsRecordingThreadName()   { return pLogger->mRecordThreadName; }

void Log::AddFile(const char * filename, FileMode file_mode, LogLevel log_level)
{
	if (filename == NULL)
		return;
	
    PathHandle logFileDirectory = fsGetLogFileDirectory();
    PathHandle path = fsAppendPathComponent(logFileDirectory, filename);
    FileStream* fh = fsOpenFile(path, file_mode);
	if (fh)//If the File Exists
	{
	
		// AddCallback will try to acquire mutex
        
		AddCallback(fsGetPathAsNativeString(path), log_level, fh, log_write, log_close, log_flush);

		{
			MutexLock lock{ pLogger->mLogMutex }; // scope lock as Write will try to acquire mutex

			// Header
			eastl::string header;
			if (pLogger->mRecordTimestamp)
				header += "date       time     ";
			if (pLogger->mRecordThreadName)
				header += "[thread name/id ]";
			if (pLogger->mRecordFile)
				header += "                   file:line  ";
			header += "  v |\n";
            fsWriteToStream(fh, header.c_str(), header.size());
            fsFlushStream(fh);
			//file->Write(header.c_str(), (unsigned)header.size());
			//file->Flush();
		}

		Write(LogLevel::eINFO, __FILE__, __LINE__, "Opened log file %s", filename);
	}
	else
	{
		Write(LogLevel::eERROR, __FILE__, __LINE__, "Failed to create log file %s", filename); // will try to acquire mutex
	}
}

void Log::AddCallback(const char * id, uint32_t log_level, void * user_data, log_callback_t callback, log_close_t close, log_flush_t flush)
{
	MutexLock lock{ pLogger->mLogMutex };
	if (!CallbackExists(id))
	{
		pLogger->mCallbacks.emplace_back(LogCallback{ id, user_data, callback, close, flush, log_level });
	}
	else
		close(user_data);
}


typedef char LogStr[LOG_LEVEL_SIZE+1];

void Log::Write(uint32_t level, const char * filename, int line_number, const char* message, ...)
{
	static eastl::pair<uint32_t, const char*> logLevelPrefixes[] =
	{
		eastl::pair<uint32_t, const char*>{ LogLevel::eWARNING, "WARN| " },
		eastl::pair<uint32_t, const char*>{ LogLevel::eINFO, "INFO| " },
		eastl::pair<uint32_t, const char*>{ LogLevel::eDEBUG, " DBG| " },
		eastl::pair<uint32_t, const char*>{ LogLevel::eERROR, " ERR| " }
	};

	uint32_t log_levels[LEVELS_LOG];
	uint32_t log_level_count = 0;

	// Check flags
	for (uint32_t i = 0; i < sizeof(logLevelPrefixes) / sizeof(logLevelPrefixes[0]); ++i)
	{
		eastl::pair<uint32_t, const char*>& it = logLevelPrefixes[i];
		if (it.first & level)
		{
			log_levels[log_level_count] = i;
			++log_level_count;
		}
	}

	bool do_once = false;
	{
		MutexLock lock{ pLogger->mLogMutex }; // scope lock as stack frames from calling AddInitialLogFile will attempt to lock mutex
		do_once = gOnce;
		gOnce = false;
	}
	if (do_once)
		AddInitialLogFile();

	uint32_t preable_end = WritePreamble(Buffer, LOG_PREAMBLE_SIZE, filename, line_number);

	// Prepare indentation
	uint32_t indentation = pLogger->mIndentation * INDENTATION_SIZE_LOG;
	memset(Buffer+preable_end, ' ', indentation);

	uint32_t offset = preable_end + LOG_LEVEL_SIZE + indentation;
	va_list args;
	va_start(args, message);
	offset += vsnprintf(Buffer + offset, MAX_BUFFER - offset, message, args);
	va_end(args);

	offset = (offset > MAX_BUFFER) ? MAX_BUFFER : offset;
	Buffer[offset] = '\n';
	Buffer[offset + 1] = 0;

	// Log for each flag
	for (uint32_t i = 0; i < log_level_count; ++i)
	{
		strncpy(Buffer + preable_end, logLevelPrefixes[log_levels[i]].second, LOG_LEVEL_SIZE);

		if (sConsoleLogging)
		{
			if (pLogger->mQuietMode)
			{
				if (level & LogLevel::eERROR)
					_PrintUnicode(Buffer, true);
			}
			else
			{
				_PrintUnicode(Buffer, level & LogLevel::eERROR);
			}
		}

		MutexLock lock{ pLogger->mLogMutex };
		for (LogCallback & callback : pLogger->mCallbacks)
		{
			if (callback.mLevel & log_levels[i])
				callback.mCallback(callback.mUserData, Buffer);
		}
	}
}

void Log::WriteRaw(uint32_t level, bool error, const char* message, ...)
{
	bool do_once = false;
	{
		MutexLock lock{ pLogger->mLogMutex }; // scope lock as stack frames from calling AddInitialLogFile will attempt to lock mutex
		do_once = gOnce;
		gOnce = false;
	}
	if (do_once)
		AddInitialLogFile();
	
	va_list args;
	va_start(args, message);
	vsnprintf(Buffer, MAX_BUFFER, message, args);
	va_end(args);

	if (sConsoleLogging)
	{
		if (pLogger->mQuietMode)
		{
			if (error)
				_PrintUnicode(Buffer, true);
		}
		else
			_PrintUnicode(Buffer, error);
	}

	MutexLock lock{ pLogger->mLogMutex };
	for (LogCallback & callback : pLogger->mCallbacks)
	{
		if (callback.mLevel & level)
			callback.mCallback(callback.mUserData, Buffer);
	}
}

void Log::AddInitialLogFile()
{

	// Add new file with executable name
    
    const char *extension = ".log";
    const size_t extensionLength = strlen(extension);
    
    char exeFileName[256];
    fsGetExecutableName(exeFileName, 256 - extensionLength);
    
	// Minimum length check
	if (exeFileName[0] == 0 || exeFileName[1] == 0)
    {
        strncpy(exeFileName, "Log", 3);
    }
    strncat(exeFileName, extension, extensionLength);
    
    AddFile(exeFileName, FM_WRITE_BINARY_ALLOW_READ, LogLevel::eALL);
}

uint32_t Log::WritePreamble(char * buffer, uint32_t buffer_size, const char * file, int line)
{
	uint32_t pos = 0;
	// Date and time
	if (pLogger->mRecordTimestamp && pos < buffer_size)
	{
		time_t  t = time(NULL);
		tm time_info;
	#ifdef _WIN32
		localtime_s(&time_info, &t);
	#elif defined(ORBIS) || defined(PROSPERO)
		localtime_s(&t, &time_info);
	#elif defined(NX64)
		t = getTimeSinceStart();
		localtime_r(&t, &time_info);
	#else
		localtime_r(&t, &time_info);
	#endif
		pos += snprintf(buffer + pos, buffer_size - pos, "%04d-%02d-%02d %02d:%02d:%02d ",
			1900 + time_info.tm_year, 1 + time_info.tm_mon, time_info.tm_mday,
			time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
	}

	if (pLogger->mRecordThreadName && pos < buffer_size)
	{
		char thread_name[MAX_THREAD_NAME_LENGTH + 1] = { 0 };
		Thread::GetCurrentThreadName(thread_name, MAX_THREAD_NAME_LENGTH + 1);
		pos += snprintf(buffer + pos, buffer_size - pos, "[%-15s]", thread_name[0] == 0 ? "NoName" : thread_name);
	}

	// File and line
	if (pLogger->mRecordFile && pos < buffer_size)
	{
		file = get_filename(file);
		pos += snprintf(buffer + pos, buffer_size - pos, " %22.*s:%-5u ", FILENAME_NAME_LENGTH_LOG, file, line);
	}

	return pos;
}

bool Log::CallbackExists(const char * id)
{
	for (const LogCallback & callback : pLogger->mCallbacks)
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

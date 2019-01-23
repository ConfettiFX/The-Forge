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
#include "../Interfaces/IMemoryManager.h"

tinystl::string GetTimeStamp()
{
	time_t sysTime;
	time(&sysTime);
	tinystl::string dateTime = ctime(&sysTime);
	dateTime.replace('\n', ' ');
	return dateTime;
}

const char* logLevelPrefixes[] = { "DEBUG", "INFO", "WARNING", "ERROR", 0 };

static LogManager* pLogInstance = 0;
//  static bool threadErrorDisplayed = false;

LogManager::LogManager(LogLevel level /* = LogLevel::LL_Debug */):
	pLogFile(NULL),
	mLogLevel(level),
	mRecordTimestamp(true),
	mInWrite(false),
	mQuietMode(false)
{
	pLogInstance = this;
	tinystl::string exeFileName = FileSystem::GetProgramFileName();
	//Minimum Length check
	if (exeFileName.size() < 2)
		exeFileName = "Log";
	Open(FileSystem::GetCurrentDir() + exeFileName + ".log");

	Thread::SetMainThread();
}

LogManager::~LogManager()
{
	Close();
	pLogInstance = NULL;
}

void LogManager::Open(const tinystl::string& fileName)
{
	if (fileName.size() == 0)
		return;

	if (pLogFile && pLogFile->IsOpen())
	{
		if (pLogFile->GetName() == fileName)
			return;
		else
			Close();
	}

	pLogFile = conf_placement_new<File>(conf_calloc(1, sizeof(File)));
	if (pLogFile->Open(fileName, FileMode::FM_Write, FSR_Absolute))
		Write(LogLevel::LL_Info, "Opened log file " + fileName);
	else
	{
		pLogFile->~File();
		conf_free(pLogFile);
		pLogFile = NULL;
		Write(LogLevel::LL_Error, "Failed to create log file " + fileName);
	}
}

void LogManager::Close()
{
	if (pLogFile && pLogFile->IsOpen())
	{
		pLogFile->Close();
		pLogFile->~File();
		conf_free(pLogFile);
		pLogFile = NULL;
	}
}

void LogManager::SetLevel(LogLevel level)
{
	ASSERT(level >= LogLevel::LL_Debug && level < LogLevel::LL_None);

	mLogLevel = level;
}

void LogManager::SetTimeStamp(bool enable) { mRecordTimestamp = enable; }

void LogManager::SetQuiet(bool quiet) { mQuietMode = quiet; }

void LogManager::Write(int level, const tinystl::string& message)
{
	ASSERT(level >= LogLevel::LL_Debug && level < LogLevel::LL_None);

	tinystl::string formattedMessage = logLevelPrefixes[level];
	formattedMessage += ": " + message;

	if (!pLogInstance || pLogInstance->mLogLevel > level || pLogInstance->mInWrite)
		return;

	if (!Thread::IsMainThread())
		pLogInstance->mLogMutex.Acquire();

	if (pLogInstance)
		pLogInstance->OutputLog(level, formattedMessage);

	pLogInstance->mLastMessage = message;

	if (pLogInstance->mRecordTimestamp)
		formattedMessage = "[ " + ::GetTimeStamp() + "] " + formattedMessage;

	if (pLogInstance->pLogFile)
	{
		pLogInstance->pLogFile->WriteLine(formattedMessage);
		pLogInstance->pLogFile->Flush();
	}

	pLogInstance->mInWrite = true;

	// TODO: Send Log Write Event

	pLogInstance->mInWrite = false;

	if (!Thread::IsMainThread())
		pLogInstance->mLogMutex.Release();
}

void LogManager::WriteRaw(const tinystl::string& message, bool error)
{
	// Avoid infinite recursion
	if (!pLogInstance || pLogInstance->mInWrite)
		return;

	if (!Thread::IsMainThread())
		pLogInstance->mLogMutex.Acquire();

	pLogInstance->mLastMessage = message;

	if (pLogInstance->mQuietMode)
	{
		if (error)
			_PrintUnicode(message, true);
	}
	else
		_PrintUnicode(message, error);

	if (pLogInstance->pLogFile)
	{
		pLogInstance->pLogFile->Write(message.c_str(), (uint32_t)message.size());
		pLogInstance->pLogFile->Flush();
	}

	pLogInstance->mInWrite = true;

	// TODO: Send Log Write Event

	pLogInstance->mInWrite = false;

	if (!Thread::IsMainThread())
		pLogInstance->mLogMutex.Release();
}

void LogManager::OutputLog(int level, const tinystl::string& message)
{
	if (mQuietMode)
	{
		if (level == LogLevel::LL_Error)
			_PrintUnicodeLine(message, true);
	}
	else
	{
		_PrintUnicodeLine(message, level == LogLevel::LL_Error);
	}
}

tinystl::string ToString(const char* function, const char* str, ...)
{
	const unsigned BUFFER_SIZE = 4096;
	char           buf[BUFFER_SIZE];

	va_list arglist;
	va_start(arglist, str);
	vsprintf_s(buf, BUFFER_SIZE, str, arglist);
	va_end(arglist);

	return tinystl::string("[") + tinystl::string(function) + "] " + tinystl::string(buf);
}

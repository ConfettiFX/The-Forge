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

#include <errno.h>

#include "SystemFileStream.h"

#include "../Interfaces/ILog.h"
#include "../Interfaces/IMemory.h"

#if defined(_WIN64)

int fseeko(FILE* stream, ssize_t offset, int origin) { return _fseeki64(stream, offset, origin); }

ssize_t ftello(FILE* stream) { return (ssize_t)_ftelli64(stream); }

#elif defined(_WIN32)
typedef int32_t off_t;

int fseeko(FILE* stream, ssize_t offset, int origin) { return _fseek(stream, offset, origin); }

ssize_t ftello(FILE* stream) { return (ssize_t)_ftell(stream); }

#endif

SystemFileStream::SystemFileStream(FILE* file, FileMode mode): FileStream(FileStreamType_System), pFile(file), mMode(mode)
{
	mFileSize = -1;

	if (fseeko(pFile, 0, SEEK_END) == 0)
	{
		mFileSize = ftello(pFile);
		rewind(pFile);
	}
}

size_t SystemFileStream::Read(void* outputBuffer, size_t bufferSizeInBytes)
{
	size_t bytesRead = fread(outputBuffer, 1, bufferSizeInBytes, pFile);
	if (bytesRead != bufferSizeInBytes)
	{
		if (ferror(pFile) != 0)
		{
			LOGF(LogLevel::eWARNING, "Error reading from system FileStream: %s", strerror(errno));
		}
	}
	return bytesRead;
}

size_t SystemFileStream::Scan(const char *format, va_list args, int *bytesRead)
{
    return vfscanf(pFile, format, args);
}

size_t SystemFileStream::Write(const void* sourceBuffer, size_t byteCount)
{
	if ((mMode & (FM_WRITE | FM_APPEND)) == 0)
	{
		LOGF(LogLevel::eWARNING, "Writing to FileStream with mode %u", mMode);
		return false;
	}

	size_t bytesWritten = fwrite(sourceBuffer, 1, byteCount, pFile);

	if (bytesWritten != byteCount)
	{
		if (ferror(pFile) != 0)
		{
			LOGF(LogLevel::eWARNING, "Error writing to system FileStream: %s", strerror(errno));
		}
	}
	return bytesWritten;
}

size_t SystemFileStream::Print(const char *format, va_list args) {
    return vfprintf(pFile, format, args);
}

bool SystemFileStream::Seek(SeekBaseOffset baseOffset, ssize_t seekOffset)
{
	if ((mMode & FM_BINARY) == 0 && baseOffset != SBO_START_OF_FILE)
	{
		LOGF(LogLevel::eWARNING, "Text-mode FileStreams only support SBO_START_OF_FILE");
		return false;
	}

	int origin = SEEK_SET;
	switch (baseOffset)
	{
		case SBO_START_OF_FILE: origin = SEEK_SET; break;
		case SBO_CURRENT_POSITION: origin = SEEK_CUR; break;
		case SBO_END_OF_FILE: origin = SEEK_END; break;
	}

	return fseeko(pFile, seekOffset, origin) == 0;
}

ssize_t SystemFileStream::GetSeekPosition() const
{
	long int result = ftell(pFile);
	if (result == -1L)
	{
		LOGF(LogLevel::eWARNING, "Error getting seek position in FileStream: %i", errno);
	}
	return result;
}

ssize_t SystemFileStream::GetFileSize() const { return mFileSize; }

void SystemFileStream::Flush()
{
	if (fflush(pFile) == EOF)
	{
		LOGF(LogLevel::eWARNING, "Error flushing system FileStream: %s", strerror(errno));
	}
}

bool SystemFileStream::IsAtEnd() const
{
	return (mFileSize >= 0 && this->GetSeekPosition() == mFileSize) || feof(pFile) != 0;
}

bool SystemFileStream::Close()
{
	if (fclose(pFile) == EOF)
	{
		LOGF(LogLevel::eERROR, "Error closing system FileStream", errno);
		conf_delete(this);
		return false;
	}

	conf_delete(this);
	return true;
}

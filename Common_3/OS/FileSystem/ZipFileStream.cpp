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

#include "../../ThirdParty/OpenSource/zip/zip.h"

#include "ZipFileStream.h"

#include "../Interfaces/ILog.h"
#include "../Interfaces/IMemory.h"

ZipFileStream::ZipFileStream(zip_t* source, FileMode mode, const Path* path):
	FileStream(FileStreamType_Zip, path),
	pSource(source),
	mMode(mode)
{
}

size_t ZipFileStream::Read(void* outputBuffer, size_t bufferSizeInBytes)
{
	ssize_t bytesRead = zip_entry_noallocread(pSource, outputBuffer, (ssize_t)bufferSizeInBytes);
	if (bytesRead == -1)
	{
		LOGF(LogLevel::eERROR, "Error reading from file %s", fsGetPathAsNativeString(pPath));
		return 0;
	}
	return bytesRead;
}

size_t ZipFileStream::Scan(const char* format, va_list args, int* bytesRead)
{
	LOGF(LogLevel::eWARNING, "fsScanFromStream is unimplemented for ZipFileStreams.");
	*bytesRead = 0;
	return 0;
}

size_t ZipFileStream::Write(const void* sourceBuffer, size_t byteCount)
{
	int64_t bytesWritten = zip_entry_write(pSource, sourceBuffer, (ssize_t)byteCount);
	if (bytesWritten == -1)
	{
		LOGF(LogLevel::eERROR, "Error writing to file %s", fsGetPathAsNativeString(pPath));
		return 0;
	}
	return 0;
}

size_t ZipFileStream::Print(const char* format, va_list args)
{
    char buffer[2048];
    size_t bytesNeeded = vsnprintf((char*)buffer, 2048, format, args);
    size_t bytesWritten = min(bytesNeeded, (size_t)2048);
    bytesWritten = min(bytesWritten, Write(buffer, bytesWritten));
    return bytesWritten;
}

bool ZipFileStream::Seek(SeekBaseOffset baseOffset, ssize_t seekOffset)
{
	return false;
}

ssize_t ZipFileStream::GetSeekPosition() const
{
	return 0;
}

ssize_t ZipFileStream::GetFileSize() const
{
	return zip_entry_size(pSource);
}

void* ZipFileStream::GetUnderlyingBuffer() const { return NULL; }

void ZipFileStream::Flush()
{
}

bool ZipFileStream::IsAtEnd() const
{
    return GetSeekPosition() == GetFileSize();
}

bool ZipFileStream::Close()
{
	int status = zip_entry_close(pSource);

	bool success = status == 0;
	if (!success)
	{
		LOGF(LogLevel::eWARNING, "Error %i closing file %s", status, fsGetPathAsNativeString(pPath));
		success = false;
	}

	conf_delete(this);
	return success;
}

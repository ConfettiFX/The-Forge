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

#include "../../ThirdParty/OpenSource/libzip/zip.h"

#include "ZipFileStream.h"

#include "../Interfaces/ILog.h"
#include "../Interfaces/IMemory.h"

// MARK: - ZipFileStream

ZipFileStream::ZipFileStream(zip_file_t* file, FileMode mode, size_t uncompressedSize):
	FileStream(FileStreamType_Zip),
	pFile(file),
    mMode(mode),
	mUncompressedSize(uncompressedSize)
{
}

size_t ZipFileStream::Read(void* outputBuffer, size_t bufferSizeInBytes)
{
	zip_int64_t bytesRead = zip_fread(pFile, outputBuffer, bufferSizeInBytes);
	if (bytesRead == -1)
	{
		zip_error_t* error = zip_file_get_error(pFile);
		LOGF(LogLevel::eERROR, "Error %i reading from file in zip: %s", error->zip_err, error->str);
		return 0;
	}
	return bytesRead;
}

size_t ZipFileStream::Scan(const char* format, va_list args, int* bytesRead)
{
    LOGF(LogLevel::eWARNING, "fsScanFromStream is unsupported for ZipFileStreams.");
    *bytesRead = 0;
    return 0;
}

size_t ZipFileStream::Write(const void* sourceBuffer, size_t byteCount)
{
	LOGF(LogLevel::eERROR, "Error: Cannot write to read-only zip file.");
	return 0;
}

size_t ZipFileStream::Print(const char* format, va_list args)
{
    LOGF(LogLevel::eWARNING, "fsPrintToStream is unsupported for ZipFileStreams.");
    return 0;
}

bool ZipFileStream::Seek(SeekBaseOffset baseOffset, ssize_t seekOffset)
{
	int origin = SEEK_SET;
	
	switch (baseOffset)
	{
		case SBO_START_OF_FILE: origin = SEEK_SET; break;
		case SBO_CURRENT_POSITION: origin = SEEK_CUR; break;
		case SBO_END_OF_FILE: origin = SEEK_END; break;
	}

	if (zip_fseek(pFile, seekOffset, origin) != 0)
	{
		zip_error_t* error = zip_file_get_error(pFile);
		LOGF(LogLevel::eERROR, "Error %i seeking in file in zip: %s", error->zip_err, error->str);
		return false;
	}
	return true;
}

ssize_t ZipFileStream::GetSeekPosition() const
{
	zip_int64_t offset = zip_ftell(pFile);
	if (offset == -1)
	{
		zip_error_t* error = zip_file_get_error(pFile);
		LOGF(LogLevel::eWARNING, "Error %i getting seek position in file in zip: %s", error->zip_err, error->str);
	}

	return offset;
}

ssize_t ZipFileStream::GetFileSize() const { return mUncompressedSize; }

void ZipFileStream::Flush() {}

bool ZipFileStream::IsAtEnd() const { return GetSeekPosition() == mUncompressedSize; }

bool ZipFileStream::Close()
{
	bool success = zip_fclose(pFile) == 0;
	if (!success)
	{
		zip_error_t* error = zip_file_get_error(pFile);
		LOGF(LogLevel::eWARNING, "Error %i closing file in zip: %s", error->zip_err, error->str);
		success = false;
	}

	conf_delete(this);
	return success;
}

// MARK: - Zip Source Stream

ZipSourceStream::ZipSourceStream(zip_source_t* source, FileMode mode):
    FileStream(FileStreamType_Zip),
    pSource(source),
    mMode(mode)
{
}

size_t ZipSourceStream::Read(void* outputBuffer, size_t bufferSizeInBytes)
{
    zip_int64_t bytesRead = zip_source_read(pSource, outputBuffer, (zip_int64_t)bufferSizeInBytes);
    if (bytesRead == -1)
    {
        zip_error_t* error = zip_source_error(pSource);
        LOGF(LogLevel::eERROR, "Error %i reading from file in zip: %s", error->zip_err, error->str);
        return 0;
    }
    return bytesRead;
}

size_t ZipSourceStream::Scan(const char* format, va_list args, int* bytesRead)
{
    LOGF(LogLevel::eWARNING, "fsScanFromStream is unimplemented for ZipSourceStreams.");
    *bytesRead = 0;
    return 0;
}

size_t ZipSourceStream::Write(const void* sourceBuffer, size_t byteCount)
{
    int64_t bytesWritten = zip_source_write(pSource, sourceBuffer, (zip_int64_t)byteCount);
    if (bytesWritten == -1)
    {
        zip_error_t* error = zip_source_error(pSource);
        LOGF(LogLevel::eERROR, "Error %i writing to file in zip: %s", error->zip_err, error->str);
        return 0;
    }
    return 0;
}

size_t ZipSourceStream::Print(const char* format, va_list args)
{
    char buffer[2048];
    size_t bytesNeeded = vsnprintf((char*)buffer, 2048, format, args);
    size_t bytesWritten = min(bytesNeeded, (size_t)2048);
    bytesWritten = min(bytesWritten, Write(buffer, bytesWritten));
    return bytesWritten;
}

bool ZipSourceStream::Seek(SeekBaseOffset baseOffset, ssize_t seekOffset)
{
    int origin = SEEK_SET;
    
    switch (baseOffset)
    {
        case SBO_START_OF_FILE: origin = SEEK_SET; break;
        case SBO_CURRENT_POSITION: origin = SEEK_CUR; break;
        case SBO_END_OF_FILE: origin = SEEK_END; break;
    }

    int status = 0;
    if (mMode & (FM_WRITE | FM_APPEND))
        status = zip_source_seek_write(pSource, seekOffset, origin);
    
    if (mMode & FM_READ)
        status = zip_source_seek(pSource, seekOffset, origin);
    
    if (status != 0)
    {
        zip_error_t* error = zip_source_error(pSource);
        LOGF(LogLevel::eERROR, "Error %i seeking in file in zip: %s", error->zip_err, error->str);
        return false;
    }
    return true;
}

ssize_t ZipSourceStream::GetSeekPosition() const
{
    if (mMode & (FM_WRITE | FM_APPEND))
    {
        return (ssize_t)zip_source_tell_write(pSource);
    }
    
    return (ssize_t)zip_source_tell(pSource);
}

ssize_t ZipSourceStream::GetFileSize() const
{
    zip_stat_t stats;
    zip_stat_init(&stats);
    zip_source_stat(pSource, &stats);
    
    return stats.size;
}

void ZipSourceStream::Flush()
{
    int status = zip_source_commit_write(pSource);
    if (status != 0)
    {
        zip_error_t* error = zip_source_error(pSource);
        LOGF(LogLevel::eWARNING, "Error %i flushing writes to zip file: %s", error->zip_err, error->str);
    }
}

bool ZipSourceStream::IsAtEnd() const
{
    return GetSeekPosition() == GetFileSize();
}

bool ZipSourceStream::Close()
{
    int status = 0;
    if (mMode & (FM_WRITE | FM_APPEND))
        status = zip_source_commit_write(pSource);
    else
        status = zip_source_close(pSource);
    
    bool success = status == 0;
    if (!success)
    {
        zip_error_t* error = zip_source_error(pSource);
        LOGF(LogLevel::eWARNING, "Error %i closing file in zip: %s", error->zip_err, error->str);
        success = false;
    }
    zip_source_free(pSource);
    
    conf_delete(this);
    return success;
}

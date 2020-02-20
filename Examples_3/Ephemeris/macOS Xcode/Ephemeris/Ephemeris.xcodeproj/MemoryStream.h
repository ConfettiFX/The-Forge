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

#include "FileSystemInternal.h"

class MemoryStream: public FileStream
{
	uint8_t* pBuffer;
	size_t   mBufferSize;
	size_t   mCursor;
	bool     mReadOnly;

	public:
	inline MemoryStream(uint8_t* buffer, size_t bufferSize, bool readOnly):
		FileStream(FileStreamType_MemoryStream),
		pBuffer(buffer),
		mBufferSize(bufferSize),
		mCursor(0),
		mReadOnly(readOnly)
	{
	}

	inline size_t AvailableCapacity(size_t requestedCapacity) const
	{
		return min((ssize_t)requestedCapacity, max((ssize_t)mBufferSize - (ssize_t)mCursor, (ssize_t)0));
	}

	size_t  Read(void* outputBuffer, size_t bufferSizeInBytes) override;
    size_t  Scan(const char* format, va_list args, int* bytesRead) override;
	size_t  Write(const void* sourceBuffer, size_t byteCount) override;
    size_t  Print(const char* format, va_list args) override;
	bool    Seek(SeekBaseOffset baseOffset, ssize_t seekOffset) override;
	ssize_t GetSeekPosition() const override;
	ssize_t GetFileSize() const override;
	void    Flush() override;
	bool    IsAtEnd() const override;
	bool    Close() override;
};

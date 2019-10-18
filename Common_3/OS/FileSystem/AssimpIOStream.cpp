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

#include "AssimpIOStream.h"

#include "../Interfaces/ILog.h"
#include "../Interfaces/IMemory.h"

AssimpIOStream::~AssimpIOStream()
{
	if (pFileStream)
	{
		fsCloseStream(pFileStream);
	}
}

size_t AssimpIOStream::Read(void* pvBuffer, size_t pSize, size_t pCount) { return fsReadFromStream(pFileStream, pvBuffer, pSize * pCount); }

size_t AssimpIOStream::Write(const void* pvBuffer, size_t pSize, size_t pCount)
{
	return fsWriteToStream(pFileStream, pvBuffer, pSize * pCount);
}

aiReturn AssimpIOStream::Seek(size_t pOffset, aiOrigin pOrigin)
{
	SeekBaseOffset baseOffset = SBO_START_OF_FILE;
	switch (pOrigin)
	{
		case aiOrigin_SET: baseOffset = SBO_START_OF_FILE; break;
		case aiOrigin_CUR: baseOffset = SBO_CURRENT_POSITION; break;
		case aiOrigin_END: baseOffset = SBO_END_OF_FILE; break;
		default: ASSERT(false && "Invalid seek origin"); break;
	}

	return fsSeekStream(pFileStream, baseOffset, pOffset) ? aiReturn_SUCCESS : aiReturn_FAILURE;
}

size_t AssimpIOStream::Tell() const { return (size_t)fsGetStreamSeekPosition(pFileStream); }

size_t AssimpIOStream::FileSize() const { return (size_t)fsGetStreamFileSize(pFileStream); }

void AssimpIOStream::Flush() { fsFlushStream(pFileStream); }

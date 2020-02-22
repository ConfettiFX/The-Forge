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

#pragma once
#include "Atomics.h" // for tfrg_atomicptr_t

// MARK: - Path

// Paths are always formatted in the native format of their file system.
// They never contain a trailing slash unless they are a root path.
//
// Implementation note: Paths must always be heap-allocated in mutable memory,
// since we cast away their const-ness in fsCopyPath.
// Doing so for a Path that is not allocated in writable heap memory is undefined
// behaviour.
struct Path
{
	struct FileSystem*         pFileSystem;
	tfrg_atomicptr_t    mRefCount;
	size_t              mPathLength;
	char                mPathBufferOffset;
	// ... plus a heap allocated UTF-8 buffer of length pathLength.
};
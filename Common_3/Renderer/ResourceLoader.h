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

// ***************************************************
// NOTE:
// "IRenderer.h" MUST be included before this header!
// ***************************************************

#pragma once

#include "../OS/Core/Atomics.h"

// Resource Loader Interface
#if !defined(TARGET_IOS)
#define DEFAULT_MEMORY_BUDGET (uint64_t)8e+7
#else
#define DEFAULT_MEMORY_BUDGET (uint64_t)6e+7
#endif

typedef struct BufferLoadDesc
{
	Buffer**    ppBuffer;
	const void* pData;
	BufferDesc  mDesc;
	/// Force Reset buffer to NULL
	bool mForceReset;
} BufferLoadDesc;

typedef struct TextureLoadDesc
{
	Texture** ppTexture;
	/// Load texture from image
	Image* pImage;
	/// Load empty texture
	TextureDesc* pDesc;
	/// Load texture from disk
	const char* pFilename;
	FSRoot      mRoot;
	uint32_t    mNodeIndex;
	bool        mUseMipmaps;
	bool        mSrgb;

	// Following is ignored if pDesc != NULL.  pDesc->mFlags will be considered instead.
	TextureCreationFlags mCreationFlag; 
} TextureLoadDesc;

typedef struct BufferUpdateDesc
{
	BufferUpdateDesc(Buffer* buf = NULL, const void* data = NULL, uint64_t srcOff = 0, uint64_t dstOff = 0, uint64_t size = 0):
		pBuffer(buf),
		pData(data),
		mSrcOffset(srcOff),
		mDstOffset(dstOff),
		mSize(size)
	{
	}

	Buffer*     pBuffer;
	const void* pData;
	uint64_t    mSrcOffset;
	uint64_t    mDstOffset;
	uint64_t    mSize;    // If 0, uses size of pBuffer
} BufferUpdateDesc;

typedef struct TextureUpdateDesc
{
	Texture* pTexture;
	Image*   pImage;
	bool     freeImage;
} TextureUpdateDesc;

typedef enum ResourceType
{
	RESOURCE_TYPE_BUFFER = 0,
	RESOURCE_TYPE_TEXTURE,
} ResourceType;

typedef struct ResourceLoadDesc
{
	ResourceLoadDesc(BufferLoadDesc& buffer): mType(RESOURCE_TYPE_BUFFER), buf(buffer) {}
	ResourceLoadDesc(TextureLoadDesc& texture): mType(RESOURCE_TYPE_TEXTURE), tex(texture) {}

	ResourceType mType;
	union
	{
		BufferLoadDesc  buf;
		TextureLoadDesc tex;
	};
} ResourceLoadDesc;

typedef struct ResourceUpdateDesc
{
	ResourceUpdateDesc(BufferUpdateDesc& buffer): mType(RESOURCE_TYPE_BUFFER), buf(buffer) {}
	ResourceUpdateDesc(TextureUpdateDesc& texture): mType(RESOURCE_TYPE_TEXTURE), tex(texture) {}

	ResourceType mType;
	union
	{
		BufferUpdateDesc  buf;
		TextureUpdateDesc tex;
	};
} ResourceUpdateDesc;

typedef struct ShaderStageLoadDesc
{
	tinystl::string mFileName;
	ShaderMacro*    pMacros;
	uint32_t        mMacroCount;
	FSRoot          mRoot;
} ShaderStageLoadDesc;

typedef struct ShaderLoadDesc
{
	ShaderStageLoadDesc mStages[SHADER_STAGE_COUNT];
	ShaderTarget        mTarget;
} ShaderLoadDesc;

typedef tfrg_atomic64_t SyncToken;

void initResourceLoaderInterface(Renderer* pRenderer, uint64_t memoryBudget = DEFAULT_MEMORY_BUDGET, bool useThreads = false);
void removeResourceLoaderInterface(Renderer* pRenderer);

void addResource(BufferLoadDesc* pBuffer, bool batch = false);
void addResource(TextureLoadDesc* pTexture, bool batch = false);
void addResource(BufferLoadDesc* pBufferDesc, SyncToken* token);
void addResource(TextureLoadDesc* pTextureDesc, SyncToken* token);

void updateResource(BufferUpdateDesc* pBuffer, bool batch = false);
void updateResource(TextureUpdateDesc* pTexture, bool batch = false);
void updateResources(uint32_t resourceCount, ResourceUpdateDesc* pResources);
void updateResource(BufferUpdateDesc* pBuffer, SyncToken* token);
void updateResource(TextureUpdateDesc* pTexture, SyncToken* token);
void updateResources(uint32_t resourceCount, ResourceUpdateDesc* pResources, SyncToken* token);

void waitBatchCompleted();
bool isTokenCompleted(SyncToken token);
void waitTokenCompleted(SyncToken token);

void removeResource(Buffer* pBuffer);
void removeResource(Texture* pTexture);

/// Either loads the cached shader bytecode or compiles the shader to create new bytecode depending on whether source is newer than binary
void addShader(Renderer* pRenderer, const ShaderLoadDesc* pDesc, Shader** ppShader);

void flushResourceUpdates();
void finishResourceLoading();

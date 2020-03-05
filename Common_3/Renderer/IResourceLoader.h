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

// ***************************************************
// NOTE:
// "IRenderer.h" MUST be included before this header!
// ***************************************************

#pragma once

#include "../Renderer/IRenderer.h"
#include "../OS/Core/Atomics.h"
#include "../OS/Interfaces/IFileSystem.h"
#include "../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../Renderer/ResourceLoaderInternalTypes.h"

typedef struct BufferUpdateDesc BufferUpdateDesc;
typedef struct BufferLoadDesc BufferLoadDesc;
typedef struct RenderMesh RenderMesh;

// MARK: - Resource Loading

typedef enum LoadPriority
{
	// This load priority is only used for updates that
	// have their data stored in GPU memory (e.g. from an
	// updateResource call).
	LOAD_PRIORITY_UPDATE = 0,
	
	// LoadPriorities High, Normal, and Low are for loads
	// where the data is not already stored in GPU memory.
	LOAD_PRIORITY_HIGH,
	LOAD_PRIORITY_NORMAL,
	LOAD_PRIORITY_LOW,
	
	LOAD_PRIORITY_COUNT
} LoadPriority;

typedef struct BufferLoadDesc
{
	Buffer**    ppBuffer;
	const void* pData;
	BufferDesc  mDesc;
	/// Force Reset buffer to NULL
	bool mForceReset;
	/// Whether to skip uploading any data to the buffer.
	/// Automatically set to true if using addResource (rather than begin/endAddResource)
	/// with pData = NULL and mForceReset = false
	bool mSkipUpload;
	
	BufferUpdateInternalData mInternalData;
} BufferLoadDesc;

typedef struct RawImageData
{
	uint8_t* pRawData;
	TinyImageFormat mFormat;
	uint32_t mWidth, mHeight, mDepth, mArraySize, mMipLevels;
	bool mMipsAfterSlices;
	
	// The stride between subsequent rows.
	// If using a beginUpdateResource/endUpdateResource pair,
	// copies to pRawData should use this stride.
	// A stride of 0 means the data is tightly packed.
	uint32_t mRowStride;
} RawImageData;

typedef struct BinaryImageData
{
    void* pBinaryData;
    size_t mSize;
    const char* pExtension;
} BinaryImageData;

typedef struct TextureLoadDesc
{
	Texture**    ppTexture;
	
	/// Load empty texture
	TextureDesc* pDesc;
    
	/// Load texture from disk
	const Path* pFilePath;
	uint32_t    mNodeIndex;
	/// Load texture from raw data
	RawImageData* pRawImageData = NULL;
	/// Load texture from binary data (with header)
	BinaryImageData* pBinaryImageData = NULL;

	// Following is ignored if pDesc != NULL.  pDesc->mFlags will be considered instead.
	TextureCreationFlags mCreationFlag;

	struct
	{
		MappedMemoryRange mMappedRange;
	} mInternalData;
} TextureLoadDesc;

typedef struct Geometry
{
	struct Hair
	{
		uint32_t                mVertexCountPerStrand;
		uint32_t                mGuideCountPerStrand;
	};

	struct ShadowData
	{
		void*                   pIndices;
		void*                   pAttributes[MAX_VERTEX_ATTRIBS];
	};

	/// Index buffer to bind when drawing this geometry
	Buffer*                     pIndexBuffer;
	/// The array of vertex buffers to bind when drawing this geometry
	Buffer*                     pVertexBuffers[MAX_VERTEX_BINDINGS];
	uint32_t                    mVertexStrides[MAX_VERTEX_BINDINGS];
	/// The array of traditional draw arguments to draw each subset in this geometry
	IndirectDrawIndexArguments* pDrawArgs;
	/// Shadow copy of the geometry vertex and index data if requested through the load flags
	ShadowData*                 pShadow;

	/// The array of joint inverse bind-pose matrices ( object-space )
	mat4*                       pInverseBindPoses;
	/// The array of data to remap skin batch local joint ids to global joint ids
	uint32_t*                   pJointRemaps;
	/// Hair data
	Hair                        mHair;

	/// Number of vertex buffers in this geometry
	uint32_t                    mVertexBufferCount : 8;
	/// Index type (32 or 16 bit)
	uint32_t                    mIndexType : 2;
	/// Number of joints in the skinned geometry
	uint32_t                    mJointCount : 16;
	/// Number of draw args in the geometry
	uint32_t                    mDrawArgCount;
	/// Number of indices in the geometry
	uint32_t                    mIndexCount;
	/// Number of vertices in the geometry
	uint32_t                    mVertexCount;

	uint32_t                    mPadA;
	uint32_t                    mPadB;
} Geometry;
static_assert(sizeof(Geometry) % 16 == 0, "GLTFContainer size must be a multiple of 16");

typedef enum GeometryLoadFlags
{
	/// Keep shadow copy of indices and vertices for CPU
	GEOMETRY_LOAD_FLAG_SHADOWED = 0x1,
	/// Use structured buffers instead of raw buffers
	GEOMETRY_LOAD_FLAG_STRUCTURED_BUFFERS = 0x2,
} GeometryLoadFlags;
MAKE_ENUM_FLAG(uint32_t, GeometryLoadFlags)

typedef struct GeometryLoadDesc
{
	/// Output geometry
	Geometry**        ppGeometry;
	/// Path to geometry container
	const Path*       pFilePath;
	/// Loading flags
	GeometryLoadFlags mFlags;
	/// Linked gpu node
	uint32_t          mNodeIndex;
	/// Specifies how to arrange the vertex data loaded from the file into GPU memory
	VertexLayout*     pVertexLayout;
} GeometryLoadDesc;

typedef struct VirtualTexturePageInfo
{
	uint pageAlive;
	uint TexID;
	uint mipLevel;
	uint padding1;
} VirtualTexturePageInfo;

typedef struct BufferUpdateDesc
{
	Buffer*                  pBuffer;
	uint64_t                 mDstOffset;
	uint64_t                 mSize;

	/// To be filled by the caller
	/// Example:
	/// BufferUpdateDesc update = { pBuffer, bufferDstOffset };
	/// beginUpdateResource(&update);
	/// ParticleVertex* vertices = (ParticleVertex*)update.pMappedData;
	///   for (uint32_t i = 0; i < particleCount; ++i)
	///	    vertices[i] = { rand() };
	/// endUpdateResource(&update, &token);
	void*                    pMappedData;

	// Internal
	BufferUpdateInternalData mInternalData;
} BufferUpdateDesc;

typedef struct TextureUpdateDesc
{
	Texture* 		pTexture;
	RawImageData* 	pRawImageData = NULL;
	
	void* pMappedData;
	struct {
		MappedMemoryRange mMappedRange;
	} mInternalData;
} TextureUpdateDesc;

typedef struct ShaderStageLoadDesc
{
	const char*         pFileName;
	ShaderMacro*        pMacros;
	uint32_t            mMacroCount;
	ResourceDirectory   mRoot;
    const char*         pEntryPointName;
} ShaderStageLoadDesc;

typedef struct ShaderLoadDesc
{
	ShaderStageLoadDesc mStages[SHADER_STAGE_COUNT];
	ShaderTarget        mTarget;
} ShaderLoadDesc;

typedef struct SyncToken
{
	uint64_t mWaitIndex[LOAD_PRIORITY_COUNT];
} SyncToken;

typedef struct ResourceLoaderDesc
{
	uint64_t mBufferSize;
	uint32_t mBufferCount;
} ResourceLoaderDesc;

extern ResourceLoaderDesc gDefaultResourceLoaderDesc;

// MARK: - Resource Loader Functions

void initResourceLoaderInterface(Renderer* pRenderer, ResourceLoaderDesc* pDesc = nullptr);
void exitResourceLoaderInterface(Renderer* pRenderer);

// MARK: addResource and updateResource

/// Adding and updating resources can be done using a addResource or
/// beginUpdateResource/endUpdateResource pair.
/// if addResource(BufferLoadDesc) is called with a data size larger than the ResourceLoader's staging buffer, the ResourceLoader
/// will perform multiple copies/flushes rather than failing the copy.

/// If token is NULL, the resource will be available when allResourceLoadsCompleted() returns true.
/// If token is non NULL, the resource will be available after isTokenCompleted(token) returns true.
void addResource(BufferLoadDesc* pBufferDesc, SyncToken* token, LoadPriority priority);
void addResource(TextureLoadDesc* pTextureDesc, SyncToken* token, LoadPriority priority);
void addResource(GeometryLoadDesc* pGeomDesc, SyncToken* token, LoadPriority priority);

void beginUpdateResource(BufferUpdateDesc* pBufferDesc);
void beginUpdateResource(TextureUpdateDesc* pTextureDesc);
void endUpdateResource(BufferUpdateDesc* pBuffer, SyncToken* token);
void endUpdateResource(TextureUpdateDesc* pTexture, SyncToken* token);

// MARK: removeResource

void removeResource(Buffer* pBuffer);
void removeResource(Texture* pTexture);
void removeResource(Geometry* pGeom);

// MARK: Waiting for Loads

/// Returns whether all submitted resource loads and updates have been completed.
bool allResourceLoadsCompleted();

/// Blocks the calling thread until allResourceLoadsCompleted() returns true.
/// Note that if more resource loads or updates are submitted from a different thread while
/// while the calling thread is blocked, those loads or updates are not guaranteed to have
/// completed when this function returns.
void waitForAllResourceLoads();

/// A SyncToken is an array of monotonically increasing integers.
/// getLastTokenCompleted() returns the last value for which
/// isTokenCompleted(token) is guaranteed to return true.
SyncToken getLastTokenCompleted();
bool isTokenCompleted(const SyncToken* token);
void waitForToken(const SyncToken* token);

/// Either loads the cached shader bytecode or compiles the shader to create new bytecode depending on whether source is newer than binary
void addShader(Renderer* pRenderer, const ShaderLoadDesc* pDesc, Shader** pShader);

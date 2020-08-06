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

#include "../Renderer/IRenderer.h"
#include "../OS/Core/Atomics.h"

typedef struct MappedMemoryRange
{
	uint8_t* pData;
	Buffer*  pBuffer;
	uint64_t mOffset;
	uint64_t mSize;
	uint32_t mFlags;
} MappedMemoryRange;

typedef enum TextureContainerType
{
	/// Use whatever container is designed for that platform
	/// Windows, macOS, Linux - TEXTURE_CONTAINER_DDS
	/// iOS, Android          - TEXTURE_CONTAINER_KTX
	TEXTURE_CONTAINER_DEFAULT = 0,
	/// Explicit container types
	/// .dds
	TEXTURE_CONTAINER_DDS,
	/// .ktx
	TEXTURE_CONTAINER_KTX,
	/// .gnf
	TEXTURE_CONTAINER_GNF,
	/// .basis
	TEXTURE_CONTAINER_BASIS,
	/// .svt
	TEXTURE_CONTAINER_SVT,
} TextureContainerType;

// MARK: - Resource Loading

typedef struct BufferLoadDesc
{
	Buffer**    ppBuffer;
	const void* pData;
	BufferDesc  mDesc;
	/// Force Reset buffer to NULL
	bool        mForceReset;
} BufferLoadDesc;

typedef struct TextureLoadDesc
{
	Texture**            ppTexture;
	/// Load empty texture
	TextureDesc*         pDesc;
	/// Filename without extension. Extension will be determined based on mContainer
	const char*          pFileName;
	/// The index of the GPU in SLI/Cross-Fire that owns this texture
	uint32_t             mNodeIndex;
	/// Following is ignored if pDesc != NULL.  pDesc->mFlags will be considered instead.
	TextureCreationFlags mCreationFlag;
	/// The texture file format (dds/ktx/...)
	TextureContainerType mContainer;
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
#if defined(_WINDOWS) && !defined(_WIN64)
	uint32_t                    mPadC;
#endif
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
	/// Filename of geometry container
	const char*       pFileName;
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

	/// Internal
	struct
	{
		MappedMemoryRange    mMappedRange;
	} mInternal;
} BufferUpdateDesc;

/// #NOTE: Only use for procedural textures which are created on CPU (noise textures, font texture, ...)
typedef struct TextureUpdateDesc
{
	Texture*              pTexture;
	uint32_t              mMipLevel;
	uint32_t              mArrayLayer;

	/// To be filled by the caller
	/// Example:
	/// BufferUpdateDesc update = { pTexture, 2, 1 };
	/// beginUpdateResource(&update);
	/// Row by row copy is required if mDstRowStride > mSrcRowStride. Single memcpy will work if mDstRowStride == mSrcRowStride
	/// 2D
	/// for (uint32_t r = 0; r < update.mRowCount; ++r)
	///     memcpy(update.pMappedData + r * update.mDstRowStride, srcPixels + r * update.mSrcRowStride, update.mSrcRowStride);
	/// 3D
	/// for (uint32_t z = 0; z < depth; ++z)
	/// {
	///     uint8_t* dstData = update.pMappedData + update.mDstSliceStride * z;
	///     uint8_t* srcData = srcPixels + update.mSrcSliceStride * z;
	///     for (uint32_t r = 0; r < update.mRowCount; ++r)
	///         memcpy(dstData + r * update.mDstRowStride, srcData + r * update.mSrcRowStride, update.mSrcRowStride);
	/// }
	/// endUpdateResource(&update, &token);
	uint8_t*              pMappedData;
	/// Size of each row in destination including padding - Needs to be respected otherwise texture data will be corrupted if dst row stride is not the same as src row stride
	uint32_t              mDstRowStride;
	/// Number of rows in this slice of the texture
	uint32_t              mRowCount;
	/// Src row stride for convenience (mRowCount * width * texture format size)
	uint32_t              mSrcRowStride;
	/// Size of each slice in destination including padding - Use for offsetting dst data updating 3D textures
	uint32_t              mDstSliceStride;
	/// Size of each slice in src - Use for offsetting src data when updating 3D textures
	uint32_t              mSrcSliceStride;

	/// Internal
	struct
	{
		MappedMemoryRange mMappedRange;
	} mInternal;
} TextureUpdateDesc;

typedef enum ShaderStageLoadFlags
{
	SHADER_STAGE_LOAD_FLAG_NONE = 0x0,
	/// D3D12 only - Enable passing primitive id to pixel shader
	SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID = 0x1,
} ShaderStageLoadFlags;
MAKE_ENUM_FLAG(uint32_t, ShaderStageLoadFlags);

typedef struct ShaderStageLoadDesc
{
	const char*          pFileName;
	ShaderMacro*         pMacros;
	uint32_t             mMacroCount;
	const char*          pEntryPointName;
	ShaderStageLoadFlags mFlags;
} ShaderStageLoadDesc;

typedef struct ShaderLoadDesc
{
	ShaderStageLoadDesc mStages[SHADER_STAGE_COUNT];
	ShaderTarget        mTarget;
} ShaderLoadDesc;

typedef struct PipelineCacheLoadDesc
{
	const char*         pFileName;
	PipelineCacheFlags  mFlags;
} PipelineCacheLoadDesc;

typedef struct PipelineCacheSaveDesc
{
	const char*         pFileName;
} PipelineCacheSaveDesc;

typedef uint64_t SyncToken;

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
void addResource(BufferLoadDesc* pBufferDesc, SyncToken* token);
void addResource(TextureLoadDesc* pTextureDesc, SyncToken* token);
void addResource(GeometryLoadDesc* pGeomDesc, SyncToken* token);

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

/// Save/Load pipeline cache from disk
void addPipelineCache(Renderer* pRenderer, const PipelineCacheLoadDesc* pDesc, PipelineCache** ppPipelineCache);
void savePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache, PipelineCacheSaveDesc* pDesc);

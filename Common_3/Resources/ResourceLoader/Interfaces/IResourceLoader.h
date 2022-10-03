/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../../../Graphics/GraphicsConfig.h"

#include "../../../Utilities/Math/MathTypes.h"
#include "../../../Utilities/Threading/Atomics.h"
#include "../../../Graphics/Interfaces/IGraphics.h"

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
	Texture** ppTexture;
	/// Load empty texture
	TextureDesc* pDesc;
	/// Filename without extension. Extension will be determined based on mContainer
	const char* pFileName;
	/// Password for loading texture from encrypted files
	const char* pFilePassword;
	/// The index of the GPU in SLI/Cross-Fire that owns this texture, or the Renderer index in unlinked mode.
	uint32_t mNodeIndex;
	/// Following is ignored if pDesc != NULL.  pDesc->mFlags will be considered instead.
	TextureCreationFlags mCreationFlag;
	/// The texture file format (dds/ktx/...)
	TextureContainerType mContainer;
} TextureLoadDesc;


FORGE_CONSTEXPR const char GEOMETRY_FILE_MAGIC_STR[] = { 'G', 'e', 'o', 'm', 'e', 't', 'r', 'y', 'T', 'F' };

typedef struct Geometry
{
	struct Hair
	{
		uint32_t mVertexCountPerStrand;
		uint32_t mGuideCountPerStrand;
	};

	struct ShadowData
	{
		void* pIndices;
		void* pAttributes[MAX_VERTEX_ATTRIBS];
	};

	/// Index buffer to bind when drawing this geometry
	Buffer* pIndexBuffer;
	/// The array of vertex buffers to bind when drawing this geometry
	Buffer* pVertexBuffers[MAX_VERTEX_BINDINGS];
	/// The array of traditional draw arguments to draw each subset in this geometry
	IndirectDrawIndexArguments* pDrawArgs;
	/// Shadow copy of the geometry vertex and index data if requested through the load flags
	ShadowData* pShadow;

	/// The array of joint inverse bind-pose matrices ( object-space )
	mat4* pInverseBindPoses;
	/// The array of data to remap skin batch local joint ids to global joint ids
	uint32_t* pJointRemaps;
	/// The array of vertex buffer strides to bind when drawing this geometry
	uint32_t mVertexStrides[MAX_VERTEX_BINDINGS];
	/// Hair data
	Hair mHair;

	/// Number of vertex buffers in this geometry
	uint32_t mVertexBufferCount : 8;
	/// Index type (32 or 16 bit)
	uint32_t mIndexType : 2;
	/// Number of joints in the skinned geometry
	uint32_t mJointCount : 16;
	/// Number of draw args in the geometry
	uint32_t mDrawArgCount;
	/// Number of indices in the geometry
	uint32_t mIndexCount;
	/// Number of vertices in the geometry
	uint32_t mVertexCount;

	uint32_t mPad[3];
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

typedef enum MeshOptimizerFlags
{
	MESH_OPTIMIZATION_FLAG_OFF = 0x0,
	/// Vertex cache optimization
	MESH_OPTIMIZATION_FLAG_VERTEXCACHE = 0x1,
	/// Overdraw optimization
	MESH_OPTIMIZATION_FLAG_OVERDRAW = 0x2,
	/// Vertex fetch optimization
	MESH_OPTIMIZATION_FLAG_VERTEXFETCH = 0x4,
	/// All
	MESH_OPTIMIZATION_FLAG_ALL = 0x7,
} MeshOptimizerFlags;
MAKE_ENUM_FLAG(uint32_t, MeshOptimizerFlags)

typedef struct GeometryLoadDesc
{
	/// Output geometry
	Geometry** ppGeometry;
	/// Filename of geometry container
	const char* pFileName;
	/// Password for file
	const char* pFilePassword;
	/// Loading flags
	GeometryLoadFlags mFlags;
	/// Optimization flags
	MeshOptimizerFlags mOptimizationFlags;
	/// Linked gpu node / Unlinked Renderer index
	uint32_t mNodeIndex;
	/// Specifies how to arrange the vertex data loaded from the file into GPU memory
	VertexLayout* pVertexLayout;
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
	Buffer*  pBuffer;
	uint64_t mDstOffset;
	uint64_t mSize;

	/// To be filled by the caller
	/// Example:
	/// BufferUpdateDesc update = { pBuffer, bufferDstOffset };
	/// beginUpdateResource(&update);
	/// ParticleVertex* vertices = (ParticleVertex*)update.pMappedData;
	///   for (uint32_t i = 0; i < particleCount; ++i)
	///	    vertices[i] = { rand() };
	/// endUpdateResource(&update, &token);
	void* pMappedData;

	/// Internal
	struct
	{
		MappedMemoryRange mMappedRange;
	} mInternal;
} BufferUpdateDesc;

/// #NOTE: Only use for procedural textures which are created on CPU (noise textures, font texture, ...)
typedef struct TextureUpdateDesc
{
	Texture* pTexture;
	uint32_t mMipLevel;
	uint32_t mArrayLayer;

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
	uint8_t* pMappedData;
	/// Size of each row in destination including padding - Needs to be respected otherwise texture data will be corrupted if dst row stride is not the same as src row stride
	uint32_t mDstRowStride;
	/// Number of rows in this slice of the texture
	uint32_t mRowCount;
	/// Src row stride for convenience (mRowCount * width * texture format size)
	uint32_t mSrcRowStride;
	/// Size of each slice in destination including padding - Use for offsetting dst data updating 3D textures
	uint32_t mDstSliceStride;
	/// Size of each slice in src - Use for offsetting src data when updating 3D textures
	uint32_t mSrcSliceStride;

	/// Internal
	struct
	{
		MappedMemoryRange mMappedRange;
	} mInternal;
} TextureUpdateDesc;

typedef struct TextureCopyDesc
{
	Texture* pTexture;
	Buffer* pBuffer;
	/// Semaphore to synchronize graphics/compute operations that write to the texture with the texture -> buffer copy.
	Semaphore* pWaitSemaphore;
	uint32_t mTextureMipLevel;
	uint32_t mTextureArrayLayer;
	/// Current texture state.
	ResourceState mTextureState;
	/// Queue the texture is copied from.
	QueueType mQueueType;
	uint64_t mBufferOffset;
} TextureCopyDesc;

typedef enum ShaderStageLoadFlags
{
	SHADER_STAGE_LOAD_FLAG_NONE = 0x0,
	/// D3D12 only - Enable passing primitive id to pixel shader
	SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID = 0x1,
    /// Creates VR multisample variant of the shader
    SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW = 0x2,
} ShaderStageLoadFlags;
MAKE_ENUM_FLAG(uint32_t, ShaderStageLoadFlags);

typedef struct ShaderStageLoadDesc
{    //-V802 : Very user-facing struct, and order is highly important to convenience
	const char*          pFileName;
	ShaderMacro*         pMacros;
	uint32_t             mMacroCount;
	const char*          pEntryPointName;
	ShaderStageLoadFlags mFlags;
} ShaderStageLoadDesc;

typedef struct ShaderLoadDesc
{
	ShaderStageLoadDesc   mStages[SHADER_STAGE_COUNT];
	ShaderTarget          mTarget;
	const ShaderConstant* pConstants;
	uint32_t              mConstantCount;
} ShaderLoadDesc;

typedef struct PipelineCacheLoadDesc
{
	const char*        pFileName;
	const char*        pFilePassword;
	PipelineCacheFlags mFlags;
} PipelineCacheLoadDesc;

typedef struct PipelineCacheSaveDesc
{
	const char* pFileName;
	const char* pFilePassword;
} PipelineCacheSaveDesc;

typedef uint64_t SyncToken;

typedef struct ResourceLoaderDesc
{
	uint64_t mBufferSize;
	uint32_t mBufferCount;
	bool     mSingleThreaded;
} ResourceLoaderDesc;

extern ResourceLoaderDesc gDefaultResourceLoaderDesc;

// MARK: - Resource Loader Functions

FORGE_RENDERER_API void initResourceLoaderInterface(Renderer* pRenderer, ResourceLoaderDesc* pDesc = nullptr);
FORGE_RENDERER_API void exitResourceLoaderInterface(Renderer* pRenderer);

/// Multiple Renderer (unlinked GPU) variants. The Resource Loader must be shared between Renderers.
FORGE_RENDERER_API void initResourceLoaderInterface(Renderer** ppRenderers, uint32_t rendererCount, ResourceLoaderDesc* pDesc = nullptr);
FORGE_RENDERER_API void exitResourceLoaderInterface(Renderer** ppRenderers, uint32_t rendererCount);

// MARK: addResource and updateResource

/// Adding and updating resources can be done using a addResource or
/// beginUpdateResource/endUpdateResource pair.
/// if addResource(BufferLoadDesc) is called with a data size larger than the ResourceLoader's staging buffer, the ResourceLoader
/// will perform multiple copies/flushes rather than failing the copy.

/// If token is NULL, the resource will be available when allResourceLoadsCompleted() returns true.
/// If token is non NULL, the resource will be available after isTokenCompleted(token) returns true.
FORGE_RENDERER_API void addResource(BufferLoadDesc* pBufferDesc, SyncToken* token);
FORGE_RENDERER_API void addResource(TextureLoadDesc* pTextureDesc, SyncToken* token);
FORGE_RENDERER_API void addResource(GeometryLoadDesc* pGeomDesc, SyncToken* token);

FORGE_RENDERER_API void beginUpdateResource(BufferUpdateDesc* pBufferDesc);
FORGE_RENDERER_API void beginUpdateResource(TextureUpdateDesc* pTextureDesc);
FORGE_RENDERER_API void endUpdateResource(BufferUpdateDesc* pBuffer, SyncToken* token);
FORGE_RENDERER_API void endUpdateResource(TextureUpdateDesc* pTexture, SyncToken* token);

/// Copies data from GPU to the CPU, typically for transferring it to another GPU in unlinked mode.
/// For optimal use, the amount of data to transfer should be minimized as much as possible and applications should
/// provide additional graphics/compute work that the GPU can execute alongside the copy.
FORGE_RENDERER_API void copyResource(TextureCopyDesc* pTextureDesc, SyncToken* token);

// MARK: removeResource

FORGE_RENDERER_API void removeResource(Buffer* pBuffer);
FORGE_RENDERER_API void removeResource(Texture* pTexture);
FORGE_RENDERER_API void removeResource(Geometry* pGeom);
// Frees pGeom->pShadow in case it was requested with GEOMETRY_LOAD_FLAG_SHADOWED and you are already done with it
FORGE_RENDERER_API void removeGeometryShadowData(Geometry* pGeom);

// MARK: Waiting for Loads

/// Returns whether all submitted resource loads and updates have been completed.
FORGE_RENDERER_API bool allResourceLoadsCompleted();

/// Blocks the calling thread until allResourceLoadsCompleted() returns true.
/// Note that if more resource loads or updates are submitted from a different thread while
/// while the calling thread is blocked, those loads or updates are not guaranteed to have
/// completed when this function returns.
FORGE_RENDERER_API void waitForAllResourceLoads();

/// Returns wheter the resourceloader is single threaded or not
FORGE_RENDERER_API bool isResourceLoaderSingleThreaded();

/// A SyncToken is an array of monotonically increasing integers.
/// getLastTokenCompleted() returns the last value for which
/// isTokenCompleted(token) is guaranteed to return true.
FORGE_RENDERER_API SyncToken getLastTokenCompleted();
FORGE_RENDERER_API bool      isTokenCompleted(const SyncToken* token);
FORGE_RENDERER_API void      waitForToken(const SyncToken* token);

/// Allows clients to synchronize with the submission of copy commands (as opposed to their completion).
/// This can reduce the wait time for clients but requires using the Semaphore from getLastSemaphoreCompleted() in a wait
/// operation in a submit that uses the textures just updated.
FORGE_RENDERER_API SyncToken getLastTokenSubmitted();
FORGE_RENDERER_API bool      isTokenSubmitted(const SyncToken* token);
FORGE_RENDERER_API void      waitForTokenSubmitted(const SyncToken* token);

/// Return the semaphore for the last copy operation of a specific GPU.
/// Could be NULL if no operations have been executed.
FORGE_RENDERER_API Semaphore* getLastSemaphoreCompleted(uint32_t nodeIndex);

/// Either loads the cached shader bytecode or compiles the shader to create new bytecode depending on whether source is newer than binary
FORGE_RENDERER_API void addShader(Renderer* pRenderer, const ShaderLoadDesc* pDesc, Shader** pShader);

/// Save/Load pipeline cache from disk
FORGE_RENDERER_API void loadPipelineCache(Renderer* pRenderer, const PipelineCacheLoadDesc* pDesc, PipelineCache** ppPipelineCache);
FORGE_RENDERER_API void savePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache, PipelineCacheSaveDesc* pDesc);

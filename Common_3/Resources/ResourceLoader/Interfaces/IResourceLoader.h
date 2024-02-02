/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#include "../../../Graphics/Interfaces/IGraphics.h"

#include "../../../Graphics/GraphicsConfig.h"
#include "../../../Utilities/Math/MathTypes.h"
#include "../../../Utilities/Threading/Atomics.h"

static FORGE_CONSTEXPR const ResourceState gVertexBufferState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | RESOURCE_STATE_SHADER_RESOURCE;
static FORGE_CONSTEXPR const ResourceState gIndexBufferState = RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE;

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
} TextureContainerType;

typedef enum RegisterMaterialResult
{

    REGISTER_MATERIAL_SUCCESS = 0x0000,
    REGISTER_MATERIAL_BADFILE = 0x0001,

} RegisterMaterialResult;

// MARK: - Resource Loading

typedef struct BufferLoadDesc
{
    Buffer**    ppBuffer;
    // This must stay valid until the buffer load is not completed
    // Use waitForToken (if a token was passed to addResource) or waitForAllResourceLoads before freeing pData
    const void* pData;
    BufferDesc  mDesc;
    /// MemZero buffer
    bool        mForceReset;

    // Optional (if user provides staging buffer memory)
    Buffer*  pSrcBuffer;
    uint64_t mSrcOffset;
} BufferLoadDesc;

typedef struct TextureLoadDesc
{
    Texture** ppTexture;
    union
    {
        /// Load empty texture
        struct
        {
            TextureDesc* pDesc;
            /// MemZero texture
            bool         mForceReset;
        };
        /// Ycbcr sampler to use when loading ycbcr texture from file
        Sampler* pYcbcrSampler;
    };
    /// Filename without extension. Extension will be determined based on mContainer
    const char*          pFileName;
    /// The index of the GPU in SLI/Cross-Fire that owns this texture, or the Renderer index in unlinked mode.
    uint32_t             mNodeIndex;
    /// Following is ignored if pDesc != NULL.  pDesc->mFlags will be considered instead.
    TextureCreationFlags mCreationFlag;
    /// The texture file format (dds/ktx/...)
    TextureContainerType mContainer;
} TextureLoadDesc;

typedef struct BufferChunk
{
    uint32_t mOffset;
    uint32_t mSize;
} BufferChunk;

// Structure used to sub-allocate chunks on a buffer, keeps track of free memory to handle new requests.
// Interface to add/remove this allocator is currently private, could be made public if needed.
typedef struct BufferChunkAllocator
{
    Buffer*      pBuffer;
    uint32_t     mUsedChunkCount;
    uint32_t     mSize;
    BufferChunk* mUnusedChunks;
} BufferChunkAllocator;

// Stores huge buffers that are then used to sub-allocate memory for each of the loaded meshes.
// GeometryBuffer can be provided to GeometryLoadDesc::pGeometryBuffer when loading a mesh, sub-chunks will be allocated
// by mIndex and mVertex allocators and return the BufferChunk(s) that where used in Geometry::mIndexBufferChunk and
// Geometry::mVertexBufferChunks
typedef struct GeometryBuffer
{
    BufferChunkAllocator mIndex;
    BufferChunkAllocator mVertex[MAX_VERTEX_BINDINGS];
} GeometryBuffer;

FORGE_CONSTEXPR const char GEOMETRY_FILE_MAGIC_STR[] = { 'G', 'e', 'o', 'm', 'e', 't', 'r', 'y', 'T', 'F' };

typedef struct Meshlet
{
    /// Offsets within meshlet_vertices and meshlet_triangles arrays with meshlet data
    uint vertexOffset;
    uint triangleOffset;

    /// Number of vertices and triangles used in the meshlet; data is stored in consecutive range defined by offset and count
    uint vertexCount;
    uint triangleCount;
} Meshlet;

typedef struct MeshletData
{
    float3 center;
    float  radius;

    /// Normal cone, useful for backface culling
    float3 coneApex;
    float3 coneAxis;
    float  coneCutoff; // = cos(angle/2)
} MeshletData;

typedef struct GeometryMeshlets
{
    uint64_t     mMeshletCount;
    Meshlet*     mMeshlets;
    MeshletData* mMeshletsData;

    uint64_t  mVertexCount;
    uint32_t* mVertices;

    uint64_t mTriangleCount;
    uint8_t* mTriangles;
} GeometryMeshlets;

typedef struct Geometry
{
    union
    {
        struct
        {
            /// Index buffer to bind when drawing this geometry
            Buffer* pIndexBuffer;
            /// The array of vertex buffers to bind when drawing this geometry
            Buffer* pVertexBuffers[MAX_VERTEX_BINDINGS];
        };
        struct
        {
            /// Used when Geometry is loaded to unified GeometryBuffer object (when GeometryLoadDesc::pGeometryBuffer is valid)
            BufferChunk mIndexBufferChunk;
            BufferChunk mVertexBufferChunks[MAX_VERTEX_BINDINGS];
        };
    };

    /// The array of traditional draw arguments to draw each subset in this geometry
    IndirectDrawIndexArguments* pDrawArgs;

    /// The array of vertex buffer strides to bind when drawing this geometry
    uint32_t mVertexStrides[MAX_VERTEX_BINDINGS];

    /// Number of vertex buffers in this geometry
    uint32_t mVertexBufferCount : 8;
    /// Index type (32 or 16 bit)
    uint32_t mIndexType : 2;
    /// Number of draw args in the geometry
    uint32_t mDrawArgCount : 22;
    /// Number of indices in the geometry
    uint32_t mIndexCount;
    /// Number of vertices in the geometry
    uint32_t mVertexCount;

    // If present, data is stored in pGeometryBuffer
    GeometryBuffer* pGeometryBuffer;

    GeometryMeshlets meshlets;

    uint32_t mPad[20];
} Geometry;

static_assert(sizeof(Geometry) == 352, "If Geometry size changes we need to rebuild all custom binary meshes");
static_assert(sizeof(Geometry) % 16 == 0, "Geometry size must be a multiple of 16");

// Outputs data that's only needed in the CPU side, OTOH the Geometry object holds GPU related information and buffers
typedef struct GeometryData
{
    struct Hair
    {
        uint32_t mVertexCountPerStrand;
        uint32_t mGuideCountPerStrand;
    };

    struct ShadowData
    {
        void* pIndices;
        void* pAttributes[MAX_SEMANTICS];

        // Strides for the data in pAttributes, this might not match Geometry::mVertexStrides since those are generated based on
        // GeometryLoadDesc::pVertexLayout, e.g. if the normals are packed on the GPU as half2 then:
        //         - Geometry::mVertexStrides will be sizeof(half2)
        //         - ShadowData::mVertexStrides might be sizeof(float3) = 12 (or maybe sizeof(float4) = 16)
        // If the data readed from the file in pAttributes is already packed then ShadowData::mVertexStrides[i] ==
        // Geometry::mVertexStrides[i]
        uint32_t mVertexStrides[MAX_SEMANTICS];

        // We might have a different number of attributes than mVertexCount.
        // This happens for example for Hair
        uint32_t mAttributeCount[MAX_SEMANTICS];

        // TODO: Consider if we want to store here mIndexStride to access ShadowData::pIndices,
        //       right now it depends on the number of vertexes in the mesh (uint16_t if mVertexCount < 64k otherwise uint32_t)
    };

    /// Shadow copy of the geometry vertex and index data if requested through the load flags
    ShadowData* pShadow;

    /// The array of joint inverse bind-pose matrices ( object-space )
    mat4*     pInverseBindPoses;
    /// The array of data to remap skin batch local joint ids to global joint ids
    uint32_t* pJointRemaps;

    /// Number of joints in the skinned geometry
    uint32_t mJointCount;

    /// Hair data
    Hair mHair;

    uint32_t mPad0[1];

    MeshletData* meshlets;

    // Custom data imported by the user in custom AssetPipelines, this can be data that was exported from a custom tool/plugin
    // specific to the engine/game. See AssetPipeline: callbacks in ProcessGLTFParams for more information.
    void*    pUserData;
    uint32_t mUserDataSize;

    uint32_t mPad1[5];
} GeometryData;

static_assert(sizeof(GeometryData) % 16 == 0, "GeometryData size must be a multiple of 16");

typedef enum GeometryLoadFlags
{
    GEOMETRY_LOAD_FLAG_NONE = 0x0,
    /// Keep shadow copy of indices and vertices for CPU
    GEOMETRY_LOAD_FLAG_SHADOWED = 0x1,
    /// Use structured buffers instead of raw buffers
    GEOMETRY_LOAD_FLAG_STRUCTURED_BUFFERS = 0x2,
    /// Geometry buffers can be used as input for ray tracing
    GEOMETRY_LOAD_FLAG_RAYTRACING_INPUT = 0x4,
} GeometryLoadFlags;
MAKE_ENUM_FLAG(uint32_t, GeometryLoadFlags)

typedef struct GeometryBufferLoadDesc
{
    ResourceState mStartState;

    const char* pNameIndexBuffer;
    const char* pNamesVertexBuffers[MAX_VERTEX_BINDINGS];

    uint32_t mIndicesSize;
    uint32_t mVerticesSizes[MAX_VERTEX_BINDINGS];

    ResourcePlacement* pIndicesPlacement;
    ResourcePlacement* pVerticesPlacements[MAX_VERTEX_BINDINGS];

    GeometryBuffer** pOutGeometryBuffer;
} GeometryBufferLoadDesc;

typedef struct GeometryBufferLayoutDesc
{
    IndexType mIndexType;
    uint32_t  mVerticesStrides[MAX_VERTEX_BINDINGS];
    // Vertex buffer/binding idx for each semantic.
    // Used to locate attributes inside specific buffers for loaded Geometry.
    uint32_t  mSemanticBindings[SEMANTIC_TEXCOORD9 + 1];
} GeometryBufferLayoutDesc;

typedef struct GeometryLoadDesc
{
    /// Output geometry
    Geometry**     ppGeometry;
    GeometryData** ppGeometryData;

    /// Filename of geometry container
    const char*         pFileName;
    /// Loading flags
    GeometryLoadFlags   mFlags;
    /// Linked gpu node / Unlinked Renderer index
    uint32_t            mNodeIndex;
    /// Specifies how to arrange the vertex data loaded from the file into GPU memory
    const VertexLayout* pVertexLayout;

    /// Optional preallocated unified buffer for geometry.
    /// When this parameter is specified, Geometry::pDrawArgs values are going
    /// to be shifted according to index/vertex location within BufferChunkAllocator.
    GeometryBuffer* pGeometryBuffer;

    /// Used to convert data to desired state inside GeometryBuffer.
    GeometryBufferLayoutDesc* pGeometryBufferLayoutDesc;
} GeometryLoadDesc;

typedef struct BufferUpdateDesc
{
    Buffer*  pBuffer;
    uint64_t mDstOffset;
    uint64_t mSize;

    /// To be filled by the caller between beginUpdateResource and endUpdateResource calls
    /// Example:
    /// BufferUpdateDesc update = { pBuffer, bufferDstOffset };
    /// beginUpdateResource(&update);
    /// ParticleVertex* vertices = (ParticleVertex*)update.pMappedData;
    ///   for (uint32_t i = 0; i < particleCount; ++i)
    ///	    vertices[i] = { rand() };
    /// endUpdateResource(&update, &token);
    void* pMappedData;

    // Optional (if user provides staging buffer memory)
    Buffer*       pSrcBuffer;
    uint64_t      mSrcOffset;
    ResourceState mCurrentState;

    /// Internal
    struct
    {
        MappedMemoryRange mMappedRange;
    } mInternal;
} BufferUpdateDesc;

typedef struct TextureSubresourceUpdate
{
    /// Filled by ResourceLaoder in beginUpdateResource
    /// Size of each row in destination including padding - Needs to be respected otherwise texture data will be corrupted if dst row stride
    /// is not the same as src row stride
    uint32_t mDstRowStride;
    /// Number of rows in this slice of the texture
    uint32_t mRowCount;
    /// Src row stride for convenience (mRowCount * width * texture format size)
    uint32_t mSrcRowStride;
    /// Size of each slice in destination including padding - Use for offsetting dst data updating 3D textures
    uint32_t mDstSliceStride;
    /// Size of each slice in src - Use for offsetting src data when updating 3D textures
    uint32_t mSrcSliceStride;
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
} TextureSubresourceUpdate;

/// #NOTE: Only use for procedural textures which are created on CPU (noise textures, font texture, ...)
typedef struct TextureUpdateDesc
{
    Texture*      pTexture;
    uint32_t      mBaseMipLevel;
    uint32_t      mMipLevels;
    uint32_t      mBaseArrayLayer;
    uint32_t      mLayerCount;
    ResourceState mCurrentState;
    // Optional - If we want to run the update on user specified command buffer instead
    Cmd*          pCmd;

    FORGE_RENDERER_API TextureSubresourceUpdate getSubresourceUpdateDesc(uint32_t mip, uint32_t layer);

    /// Internal
    struct
    {
        MappedMemoryRange mMappedRange;
        uint32_t          mDstSliceStride;
        bool              mSkipBarrier;
    } mInternal;
} TextureUpdateDesc;

typedef struct TextureCopyDesc
{
    Texture*      pTexture;
    Buffer*       pBuffer;
    /// Semaphore to synchronize graphics/compute operations that write to the texture with the texture -> buffer copy.
    Semaphore*    pWaitSemaphore;
    uint32_t      mTextureMipLevel;
    uint32_t      mTextureArrayLayer;
    /// Current texture state.
    ResourceState mTextureState;
    /// Queue the texture is copied from.
    QueueType     mQueueType;
    uint64_t      mBufferOffset;
} TextureCopyDesc;

typedef struct ShaderStageLoadDesc
{ //-V802 : Very user-facing struct, and order is highly important to convenience
    const char* pFileName;
    const char* pEntryPointName;
} ShaderStageLoadDesc;

typedef struct ShaderLoadDesc
{
    ShaderStageLoadDesc   mStages[SHADER_STAGE_COUNT];
    const ShaderConstant* pConstants;
    uint32_t              mConstantCount;
} ShaderLoadDesc;

typedef struct PipelineCacheLoadDesc
{
    const char*        pFileName;
    PipelineCacheFlags mFlags;
} PipelineCacheLoadDesc;

typedef struct PipelineCacheSaveDesc
{
    const char* pFileName;
} PipelineCacheSaveDesc;

typedef uint64_t SyncToken;

struct Material;

typedef struct ResourceLoaderDesc
{
    uint64_t mBufferSize;
    uint32_t mBufferCount;
    bool     mSingleThreaded;
#ifdef ENABLE_FORGE_MATERIALS
    bool mUseMaterials;
#endif
} ResourceLoaderDesc;

FORGE_RENDERER_API extern ResourceLoaderDesc gDefaultResourceLoaderDesc;

// MARK: - Resource Loader Functions

FORGE_RENDERER_API void initResourceLoaderInterface(Renderer* pRenderer, ResourceLoaderDesc* pDesc = nullptr);
FORGE_RENDERER_API void exitResourceLoaderInterface(Renderer* pRenderer);

/// Multiple Renderer (unlinked GPU) variants. The Resource Loader must be shared between Renderers.
FORGE_RENDERER_API void initResourceLoaderInterface(Renderer** ppRenderers, uint32_t rendererCount, ResourceLoaderDesc* pDesc = nullptr);
FORGE_RENDERER_API void exitResourceLoaderInterface(Renderer** ppRenderers, uint32_t rendererCount);

// MARK: App Material Management
#ifdef ENABLE_FORGE_MATERIALS

// Will load a material and all related shaders/textures (if they are not already loaded, Material shaders/textures are shared across all
// Materials)
FORGE_RENDERER_API uint32_t addMaterial(const char* pMaterialFileName, Material** pMaterial, SyncToken* pSyncToken);
// Will unload all the related shaders/textures (if they are not still used by some other Material)
FORGE_RENDERER_API void     removeMaterial(Material* pMaterial);

// TODO: Functions below are a simple interface to get resources from materials, as we develop materials further this interface will
// probably change.
FORGE_RENDERER_API uint32_t getMaterialSetIndex(Material* pMaterial, const char* name);
FORGE_RENDERER_API void     getMaterialShader(Material* pMaterial, uint32_t materialSetIndex, Shader** ppOutShader);
FORGE_RENDERER_API void     getMaterialTextures(Material* pMaterial, uint32_t materialSetIndex, const char** ppOutTextureBindingNames,
                                                Texture** ppOutTextures, uint32_t outTexturesSize);

#endif // ENABLE_FORGE_MATERIALS

// MARK: addResource and updateResource

FORGE_RENDERER_API void getResourceSizeAlign(const BufferLoadDesc* pDesc, ResourceSizeAlign* pOut);
FORGE_RENDERER_API void getResourceSizeAlign(const TextureLoadDesc* pDesc, ResourceSizeAlign* pOut);

/// Adding and updating resources can be done using a addResource or
/// beginUpdateResource/endUpdateResource pair.
/// if addResource(BufferLoadDesc) is called with a data size larger than the ResourceLoader's staging buffer, the ResourceLoader
/// will perform multiple copies/flushes rather than failing the copy.

/// If token is NULL, the resource will be available when allResourceLoadsCompleted() returns true.
/// If token is non NULL, the resource will be available after isTokenCompleted(token) returns true.
FORGE_RENDERER_API void addResource(BufferLoadDesc* pBufferDesc, SyncToken* token);
FORGE_RENDERER_API void addResource(TextureLoadDesc* pTextureDesc, SyncToken* token);
FORGE_RENDERER_API void addResource(GeometryLoadDesc* pGeomDesc, SyncToken* token);
FORGE_RENDERER_API void addGeometryBuffer(GeometryBufferLoadDesc* pDesc);

FORGE_RENDERER_API void beginUpdateResource(BufferUpdateDesc* pBufferDesc);
FORGE_RENDERER_API void beginUpdateResource(TextureUpdateDesc* pTextureDesc);
FORGE_RENDERER_API void endUpdateResource(BufferUpdateDesc* pBuffer);
FORGE_RENDERER_API void endUpdateResource(TextureUpdateDesc* pTexture);

/// This function is used to acquire geometry buffer location.
/// It can be used on index or vertex buffer
/// When there are no continious chunk with enough size, output chunk contains 0 size.
/// Use releaseGeometryBufferPart to release chunk.
/// Make sure all chunks are released before removeGeometryBuffer.
FORGE_RENDERER_API void addGeometryBufferPart(BufferChunkAllocator* buffer, uint32_t size, uint32_t alignment, BufferChunk* pOut,
                                              BufferChunk* pPreferredChunk = NULL);

/// Release previously claimed chunk to buffer.
/// Buffer must be the one passed to claimGeometryBufferPart for this chunk.
FORGE_RENDERER_API void removeGeometryBufferPart(BufferChunkAllocator* buffer, BufferChunk* chunk);

typedef struct FlushResourceUpdateDesc
{
    uint32_t    mNodeIndex;
    uint32_t    mWaitSemaphoreCount;
    Semaphore** ppWaitSemaphores;
    Fence*      pOutFence;
    Semaphore*  pOutSubmittedSemaphore;
} FlushResourceUpdateDesc;
FORGE_RENDERER_API void flushResourceUpdates(FlushResourceUpdateDesc* pDesc);

/// Copies data from GPU to the CPU, typically for transferring it to another GPU in unlinked mode.
/// For optimal use, the amount of data to transfer should be minimized as much as possible and applications should
/// provide additional graphics/compute work that the GPU can execute alongside the copy.
FORGE_RENDERER_API void copyResource(TextureCopyDesc* pTextureDesc, SyncToken* token);

// MARK: removeResource

FORGE_RENDERER_API void removeResource(Buffer* pBuffer);
FORGE_RENDERER_API void removeResource(Texture* pTexture);
FORGE_RENDERER_API void removeResource(Geometry* pGeom);
FORGE_RENDERER_API void removeResource(GeometryData* pGeom);
FORGE_RENDERER_API void removeGeometryBuffer(GeometryBuffer* pGeomBuffer);
// Frees pGeom->pShadow in case it was requested with GEOMETRY_LOAD_FLAG_SHADOWED and you are already done with it
FORGE_RENDERER_API void removeGeometryShadowData(GeometryData* pGeom);

// MARK: Waiting for Loads

/// Returns whether all submitted resource loads and updates have been completed.
FORGE_RENDERER_API bool allResourceLoadsCompleted();

/// Blocks the calling thread until allResourceLoadsCompleted() returns true.
/// Note that if more resource loads or updates are submitted from a different thread while
/// while the calling thread is blocked, those loads or updates are not guaranteed to have
/// completed when this function returns.
FORGE_RENDERER_API void waitForAllResourceLoads();

/// Wait for the copy queue to finish all work
FORGE_RENDERER_API void waitCopyQueueIdle();

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
FORGE_RENDERER_API Semaphore* getLastSemaphoreSubmitted(uint32_t nodeIndex);

/// Either loads the cached shader bytecode or compiles the shader to create new bytecode depending on whether source is newer than binary
FORGE_RENDERER_API void addShader(Renderer* pRenderer, const ShaderLoadDesc* pDesc, Shader** pShader);

/// Save/Load pipeline cache from disk
FORGE_RENDERER_API void loadPipelineCache(Renderer* pRenderer, const PipelineCacheLoadDesc* pDesc, PipelineCache** ppPipelineCache);
FORGE_RENDERER_API void savePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache, PipelineCacheSaveDesc* pDesc);

/// Determines whether we are using Uniform Memory Architecture or not.
/// Do not assume this variable will be the same, if code was compiled with multiple APIs result of this function might change per API.
FORGE_RENDERER_API bool isUma();

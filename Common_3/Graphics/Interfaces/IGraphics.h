/*
 * Copyright (c) 2017-2025 The Forge Interactive Inc.
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

#include "../GraphicsConfig.h"

#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"

#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"

//
// default capability levels of the renderer
//
#if !defined(RENDERER_CUSTOM_MAX)
enum
{
    MAX_INSTANCE_EXTENSIONS = 64,
    MAX_DEVICE_EXTENSIONS = 64,
    /// Max number of GPUs in SLI or Cross-Fire
    MAX_LINKED_GPUS = 4,
    /// Max number of GPUs in unlinked mode
    MAX_UNLINKED_GPUS = 4,
    /// Max number of GPus for either linked or unlinked mode. must update WindowsBase::setupPlatformUI accordingly
    MAX_MULTIPLE_GPUS = 4,
    MAX_RENDER_TARGET_ATTACHMENTS = 8,
    MAX_VERTEX_BINDINGS = 15,
    MAX_VERTEX_ATTRIBS = 15,
    MAX_RESOURCE_NAME_LENGTH = 256,
    MAX_SEMANTIC_NAME_LENGTH = 128,
    MAX_DEBUG_NAME_LENGTH = 128,
    MAX_MIP_LEVELS = 0xFFFFFFFF,
    MAX_GPU_VENDOR_STRING_LENGTH = 256, // max size for GPUVendorPreset strings
    MAX_SAMPLE_LOCATIONS = 16,
    MAX_PUSH_CONSTANTS_32BIT_COUNT = 16,
#if defined(DIRECT3D12)
    MAX_DESCRIPTOR_TABLES = 8,
#endif
#if defined(VULKAN)
    MAX_PLANE_COUNT = 3,
    MAX_DESCRIPTOR_POOL_SIZE_ARRAY_COUNT = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT + 1,
#endif
#if defined(METAL)
    MAX_DESCRIPTOR_SETS = 8,
#endif
};
#endif

#ifdef DIRECT3D12
typedef int32_t DxDescriptorID;
#endif

#if defined(ORBIS)
#include "../../../PS4/Common_3/Graphics/Gnm/GnmStructs.h"
#endif
#if defined(PROSPERO)
#include "../../../Prospero/Common_3/Graphics/Agc/AgcStructs.h"
#endif

typedef enum RendererApi
{
#if defined(DIRECT3D12)
    RENDERER_API_D3D12,
#endif
#if defined(VULKAN)
    RENDERER_API_VULKAN,
#endif
#if defined(METAL)
    RENDERER_API_METAL,
#endif
#if defined(ORBIS)
    RENDERER_API_ORBIS,
#endif
#if defined(PROSPERO)
    RENDERER_API_PROSPERO,
#endif
    RENDERER_API_COUNT
} RendererApi;

typedef enum QueueType
{
    QUEUE_TYPE_GRAPHICS = 0,
    QUEUE_TYPE_TRANSFER,
    QUEUE_TYPE_COMPUTE,
    MAX_QUEUE_TYPE
} QueueType;

typedef enum QueueFlag
{
    QUEUE_FLAG_NONE = 0x0,
    QUEUE_FLAG_DISABLE_GPU_TIMEOUT = 0x1,
    QUEUE_FLAG_INIT_MICROPROFILE = 0x2,
    MAX_QUEUE_FLAG = 0xFFFFFFFF
} QueueFlag;
MAKE_ENUM_FLAG(uint32_t, QueueFlag)

typedef enum QueuePriority
{
    QUEUE_PRIORITY_NORMAL,
    QUEUE_PRIORITY_HIGH,
    QUEUE_PRIORITY_GLOBAL_REALTIME,
    MAX_QUEUE_PRIORITY
} QueuePriority;

typedef enum LoadActionType
{
    LOAD_ACTION_DONTCARE,
    LOAD_ACTION_LOAD,
    LOAD_ACTION_CLEAR,
    MAX_LOAD_ACTION
} LoadActionType;

typedef enum StoreActionType
{
    // Store is the most common use case so keep that as default
    STORE_ACTION_STORE,
    STORE_ACTION_DONTCARE,
    STORE_ACTION_NONE,
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
    // Resolve into pResolveAttachment and also store the MSAA attachment (rare - maybe used for debug)
    STORE_ACTION_RESOLVE_STORE,
    // Resolve into pResolveAttachment and discard MSAA attachment (most common use case for resolve)
    STORE_ACTION_RESOLVE_DONTCARE,
#endif
    MAX_STORE_ACTION
} StoreActionType;

typedef void (*LogFn)(LogLevel, const char*, const char*);

typedef enum ResourceState
{
    RESOURCE_STATE_UNDEFINED = 0,
    RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER = 0x1,
    RESOURCE_STATE_INDEX_BUFFER = 0x2,
    RESOURCE_STATE_RENDER_TARGET = 0x4,
    RESOURCE_STATE_UNORDERED_ACCESS = 0x8,
    RESOURCE_STATE_DEPTH_WRITE = 0x10,
    RESOURCE_STATE_DEPTH_READ = 0x20,
    RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE = 0x40,
    RESOURCE_STATE_PIXEL_SHADER_RESOURCE = 0x80,
    RESOURCE_STATE_SHADER_RESOURCE = 0x40 | 0x80,
    RESOURCE_STATE_STREAM_OUT = 0x100,
    RESOURCE_STATE_INDIRECT_ARGUMENT = 0x200,
    RESOURCE_STATE_COPY_DEST = 0x400,
    RESOURCE_STATE_COPY_SOURCE = 0x800,
    RESOURCE_STATE_GENERIC_READ = (((((0x1 | 0x2) | 0x40) | 0x80) | 0x200) | 0x800),
    RESOURCE_STATE_PRESENT = 0x1000,
    RESOURCE_STATE_COMMON = 0x2000,
    RESOURCE_STATE_ACCELERATION_STRUCTURE_READ = 0x4000,
    RESOURCE_STATE_ACCELERATION_STRUCTURE_WRITE = 0x8000,
#if defined(VULKAN) || defined(PROSPERO)
    RESOURCE_STATE_UNORDERED_ACCESS_PIXEL = 0x10000,
#else
    RESOURCE_STATE_UNORDERED_ACCESS_PIXEL = RESOURCE_STATE_UNORDERED_ACCESS,
#endif
#if defined(QUEST_VR)
    RESOURCE_STATE_SHADING_RATE_SOURCE = 0x20000,
#endif
} ResourceState;
MAKE_ENUM_FLAG(uint32_t, ResourceState)

/// Choosing Memory Type
typedef enum ResourceMemoryUsage
{
    /// No intended memory usage specified.
    RESOURCE_MEMORY_USAGE_UNKNOWN = 0,
    /// Memory will be used on device only, no need to be mapped on host.
    RESOURCE_MEMORY_USAGE_GPU_ONLY = 1,
    /// Memory will be mapped on host. Could be used for transfer to device.
    RESOURCE_MEMORY_USAGE_CPU_ONLY = 2,
    /// Memory will be used for frequent (dynamic) updates from host and reads on device.
    RESOURCE_MEMORY_USAGE_CPU_TO_GPU = 3,
    /// Memory will be used for writing on device and readback on host.
    RESOURCE_MEMORY_USAGE_GPU_TO_CPU = 4,
    RESOURCE_MEMORY_USAGE_COUNT,
    RESOURCE_MEMORY_USAGE_MAX_ENUM = 0x7FFFFFFF
} ResourceMemoryUsage;

typedef struct GPUSelection
{
    // Available GPU capabilities
    char     ppAvailableGpuNames[MAX_MULTIPLE_GPUS][MAX_GPU_VENDOR_STRING_LENGTH];
    uint32_t pAvailableGpuIds[MAX_MULTIPLE_GPUS];
    uint32_t mAvailableGpuCount;
    uint32_t mSelectedGpuIndex;
    // Could add swap chain size, render target format, ...
    uint32_t mPreferedGpuId;
} GPUSelection;

// Forward declarations
typedef struct RendererContext    RendererContext;
typedef struct Renderer           Renderer;
typedef struct Queue              Queue;
typedef struct Pipeline           Pipeline;
typedef struct Buffer             Buffer;
typedef struct Texture            Texture;
typedef struct RenderTarget       RenderTarget;
typedef struct Shader             Shader;
typedef struct DescriptorSet      DescriptorSet;
typedef struct DescriptorIndexMap DescriptorIndexMap;
typedef struct PipelineCache      PipelineCache;

// Raytracing
typedef struct Raytracing            Raytracing;
typedef struct RaytracingHitGroup    RaytracingHitGroup;
typedef struct AccelerationStructure AccelerationStructure;

typedef struct EsramManager EsramManager;

typedef struct IndirectDrawArguments
{
    uint32_t mVertexCount;
    uint32_t mInstanceCount;
    uint32_t mStartVertex;
    uint32_t mStartInstance;
} IndirectDrawArguments;

typedef struct IndirectDrawIndexArguments
{
    uint32_t mIndexCount;
    uint32_t mInstanceCount;
    uint32_t mStartIndex;
    uint32_t mVertexOffset;
    uint32_t mStartInstance;
} IndirectDrawIndexArguments;

typedef struct IndirectDispatchArguments
{
    uint32_t mGroupCountX;
    uint32_t mGroupCountY;
    uint32_t mGroupCountZ;
} IndirectDispatchArguments;

#define INDIRECT_DRAW_ELEM_INDEX(m)       (offsetof(IndirectDrawArguments, m) / sizeof(uint32_t))
#define INDIRECT_DRAW_INDEX_ELEM_INDEX(m) (offsetof(IndirectDrawIndexArguments, m) / sizeof(uint32_t))
#define INDIRECT_DISPATCH_ELEM_INDEX(m)   (offsetof(IndirectDispatchArguments, m) / sizeof(uint32_t))

typedef enum IndirectArgumentType
{
    INDIRECT_DRAW,
    INDIRECT_DRAW_INDEX,
    INDIRECT_DISPATCH,
    INDIRECT_COMMAND_BUFFER,         // metal ICB
    INDIRECT_COMMAND_BUFFER_RESET,   // metal ICB reset
    INDIRECT_COMMAND_BUFFER_OPTIMIZE // metal ICB optimization
} IndirectArgumentType;
/************************************************/

typedef enum DescriptorType
{
    DESCRIPTOR_TYPE_UNDEFINED = 0,
    DESCRIPTOR_TYPE_SAMPLER = 0x01,
    // SRV Read only texture
    DESCRIPTOR_TYPE_TEXTURE = (DESCRIPTOR_TYPE_SAMPLER << 1),
    /// UAV Texture
    DESCRIPTOR_TYPE_RW_TEXTURE = (DESCRIPTOR_TYPE_TEXTURE << 1),
    // SRV Read only buffer
    DESCRIPTOR_TYPE_BUFFER = (DESCRIPTOR_TYPE_RW_TEXTURE << 1),
    DESCRIPTOR_TYPE_BUFFER_RAW = (DESCRIPTOR_TYPE_BUFFER | (DESCRIPTOR_TYPE_BUFFER << 1)),
    /// UAV Buffer
    DESCRIPTOR_TYPE_RW_BUFFER = (DESCRIPTOR_TYPE_BUFFER << 2),
    DESCRIPTOR_TYPE_RW_BUFFER_RAW = (DESCRIPTOR_TYPE_RW_BUFFER | (DESCRIPTOR_TYPE_RW_BUFFER << 1)),
    /// Uniform buffer
    DESCRIPTOR_TYPE_UNIFORM_BUFFER = (DESCRIPTOR_TYPE_RW_BUFFER << 2),
    /// Push constant / Root constant
    DESCRIPTOR_TYPE_ROOT_CONSTANT = (DESCRIPTOR_TYPE_UNIFORM_BUFFER << 1),
    /// IA
    DESCRIPTOR_TYPE_VERTEX_BUFFER = (DESCRIPTOR_TYPE_ROOT_CONSTANT << 1),
    DESCRIPTOR_TYPE_INDEX_BUFFER = (DESCRIPTOR_TYPE_VERTEX_BUFFER << 1),
    DESCRIPTOR_TYPE_INDIRECT_BUFFER = (DESCRIPTOR_TYPE_INDEX_BUFFER << 1),
    /// Cubemap SRV
    DESCRIPTOR_TYPE_TEXTURE_CUBE = (DESCRIPTOR_TYPE_TEXTURE | (DESCRIPTOR_TYPE_INDIRECT_BUFFER << 1)),
    /// RTV / DSV per mip slice
    DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES = (DESCRIPTOR_TYPE_INDIRECT_BUFFER << 2),
    /// RTV / DSV per array slice
    DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES = (DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES << 1),
    /// RTV / DSV per depth slice
    DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES = (DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES << 1),
    DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER = (DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES << 1),
    /// Raytracing acceleration structure
    DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE = (DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER << 1),

    // Mask for UAV resources
    DESCRIPTOR_TYPE_RW_MASK = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_RW_BUFFER,

#if defined(VULKAN)
    /// Subpass input (descriptor type only available in Vulkan)
    DESCRIPTOR_TYPE_INPUT_ATTACHMENT = (DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE << 1),
    DESCRIPTOR_TYPE_TEXEL_BUFFER = (DESCRIPTOR_TYPE_INPUT_ATTACHMENT << 1),
    DESCRIPTOR_TYPE_RW_TEXEL_BUFFER = (DESCRIPTOR_TYPE_TEXEL_BUFFER << 1),
    DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = (DESCRIPTOR_TYPE_RW_TEXEL_BUFFER << 1),
#endif
} DescriptorType;
MAKE_ENUM_FLAG(uint32_t, DescriptorType)

typedef enum SampleCount
{
    SAMPLE_COUNT_1 = 1,
    SAMPLE_COUNT_2 = 2,
    SAMPLE_COUNT_4 = 4,
    SAMPLE_COUNT_8 = 8,
    SAMPLE_COUNT_16 = 16,
    SAMPLE_COUNT_COUNT = 5,
    SAMPLE_COUNT_ALL_BITS = ((uint32_t)SAMPLE_COUNT_1 | (uint32_t)SAMPLE_COUNT_2 | (uint32_t)SAMPLE_COUNT_4 | (uint32_t)SAMPLE_COUNT_8 |
                             (uint32_t)SAMPLE_COUNT_16),
} SampleCount;

typedef enum ShaderStage
{
    SHADER_STAGE_NONE = 0,
    SHADER_STAGE_VERT = 0x1,
    SHADER_STAGE_FRAG = 0x2,
    SHADER_STAGE_COMP = 0x4,
    SHADER_STAGE_GEOM = 0x8,
    SHADER_STAGE_TESC = 0x10,
    SHADER_STAGE_TESE = 0x20,
    SHADER_STAGE_ALL_GRAPHICS = ((uint32_t)SHADER_STAGE_VERT | (uint32_t)SHADER_STAGE_TESC | (uint32_t)SHADER_STAGE_TESE |
                                 (uint32_t)SHADER_STAGE_GEOM | (uint32_t)SHADER_STAGE_FRAG),
    SHADER_STAGE_HULL = SHADER_STAGE_TESC,
    SHADER_STAGE_DOMN = SHADER_STAGE_TESE,
#if defined(ENABLE_WORKGRAPH)
    SHADER_STAGE_WORKGRAPH = 0x40,
    SHADER_STAGE_COUNT = 7,
#else
    SHADER_STAGE_COUNT = 6,
#endif
} ShaderStage;
MAKE_ENUM_FLAG(uint32_t, ShaderStage)

typedef enum TextureDimension
{
    TEXTURE_DIM_1D,
    TEXTURE_DIM_2D,
    TEXTURE_DIM_2DMS,
    TEXTURE_DIM_3D,
    TEXTURE_DIM_CUBE,
    TEXTURE_DIM_1D_ARRAY,
    TEXTURE_DIM_2D_ARRAY,
    TEXTURE_DIM_2DMS_ARRAY,
    TEXTURE_DIM_CUBE_ARRAY,
    TEXTURE_DIM_COUNT,
    TEXTURE_DIM_UNDEFINED,
} TextureDimension;

typedef enum PrimitiveTopology
{
    PRIMITIVE_TOPO_POINT_LIST = 0,
    PRIMITIVE_TOPO_LINE_LIST,
    PRIMITIVE_TOPO_LINE_STRIP,
    PRIMITIVE_TOPO_TRI_LIST,
    PRIMITIVE_TOPO_TRI_STRIP,
    PRIMITIVE_TOPO_PATCH_LIST,
    PRIMITIVE_TOPO_COUNT,
} PrimitiveTopology;

typedef enum IndexType
{
    INDEX_TYPE_UINT32 = 0,
    INDEX_TYPE_UINT16,
} IndexType;

typedef enum ShaderSemantic
{
    SEMANTIC_UNDEFINED = 0,
    SEMANTIC_POSITION,
    SEMANTIC_NORMAL,
    SEMANTIC_COLOR,
    SEMANTIC_TANGENT,
    SEMANTIC_BITANGENT,
    SEMANTIC_JOINTS,
    SEMANTIC_WEIGHTS,
    SEMANTIC_CUSTOM,
    SEMANTIC_TEXCOORD0,
    SEMANTIC_TEXCOORD1,
    SEMANTIC_TEXCOORD2,
    SEMANTIC_TEXCOORD3,
    SEMANTIC_TEXCOORD4,
    SEMANTIC_TEXCOORD5,
    SEMANTIC_TEXCOORD6,
    SEMANTIC_TEXCOORD7,
    SEMANTIC_TEXCOORD8,
    SEMANTIC_TEXCOORD9,
    MAX_SEMANTICS
} ShaderSemantic;

typedef enum BlendConstant
{
    BC_ZERO = 0,
    BC_ONE,
    BC_SRC_COLOR,
    BC_ONE_MINUS_SRC_COLOR,
    BC_DST_COLOR,
    BC_ONE_MINUS_DST_COLOR,
    BC_SRC_ALPHA,
    BC_ONE_MINUS_SRC_ALPHA,
    BC_DST_ALPHA,
    BC_ONE_MINUS_DST_ALPHA,
    BC_SRC_ALPHA_SATURATE,
    BC_BLEND_FACTOR,
    BC_ONE_MINUS_BLEND_FACTOR,
    MAX_BLEND_CONSTANTS
} BlendConstant;

typedef enum BlendMode
{
    BM_ADD,
    BM_SUBTRACT,
    BM_REVERSE_SUBTRACT,
    BM_MIN,
    BM_MAX,
    MAX_BLEND_MODES,
} BlendMode;

typedef enum CompareMode
{
    CMP_NEVER,
    CMP_LESS,
    CMP_EQUAL,
    CMP_LEQUAL,
    CMP_GREATER,
    CMP_NOTEQUAL,
    CMP_GEQUAL,
    CMP_ALWAYS,
    MAX_COMPARE_MODES,
} CompareMode;

typedef enum StencilOp
{
    STENCIL_OP_KEEP,
    STENCIL_OP_SET_ZERO,
    STENCIL_OP_REPLACE,
    STENCIL_OP_INVERT,
    STENCIL_OP_INCR,
    STENCIL_OP_DECR,
    STENCIL_OP_INCR_SAT,
    STENCIL_OP_DECR_SAT,
    MAX_STENCIL_OPS,
} StencilOp;

typedef enum ColorMask
{
    COLOR_MASK_NONE = 0x0,
    COLOR_MASK_RED = 0x1,
    COLOR_MASK_GREEN = 0x2,
    COLOR_MASK_BLUE = 0x4,
    COLOR_MASK_ALPHA = 0x8,
    COLOR_MASK_ALL = (COLOR_MASK_RED | COLOR_MASK_GREEN | COLOR_MASK_BLUE | COLOR_MASK_ALPHA),
} ColorMask;
MAKE_ENUM_FLAG(uint8_t, ColorMask)

// Blend states are always attached to one of the eight or more render targets that
// are in a MRT
// Mask constants
typedef enum BlendStateTargets
{
    BLEND_STATE_TARGET_0 = 0x1,
    BLEND_STATE_TARGET_1 = 0x2,
    BLEND_STATE_TARGET_2 = 0x4,
    BLEND_STATE_TARGET_3 = 0x8,
    BLEND_STATE_TARGET_4 = 0x10,
    BLEND_STATE_TARGET_5 = 0x20,
    BLEND_STATE_TARGET_6 = 0x40,
    BLEND_STATE_TARGET_7 = 0x80,
    BLEND_STATE_TARGET_ALL = 0xFF,
} BlendStateTargets;
MAKE_ENUM_FLAG(uint32_t, BlendStateTargets)

typedef enum CullMode
{
    CULL_MODE_NONE = 0,
    CULL_MODE_BACK,
    CULL_MODE_FRONT,
    CULL_MODE_BOTH,
    MAX_CULL_MODES
} CullMode;

typedef enum FrontFace
{
    FRONT_FACE_CCW = 0,
    FRONT_FACE_CW
} FrontFace;

typedef enum FillMode
{
    FILL_MODE_SOLID,
    FILL_MODE_WIREFRAME,
    MAX_FILL_MODES
} FillMode;

typedef enum PipelineType
{
    PIPELINE_TYPE_UNDEFINED = 0,
    PIPELINE_TYPE_COMPUTE,
    PIPELINE_TYPE_GRAPHICS,
#if defined(ENABLE_WORKGRAPH)
    PIPELINE_TYPE_WORKGRAPH,
#endif
    PIPELINE_TYPE_COUNT,
} PipelineType;

typedef enum FilterType
{
    FILTER_NEAREST = 0,
    FILTER_LINEAR,
} FilterType;

typedef enum AddressMode
{
    ADDRESS_MODE_MIRROR,
    ADDRESS_MODE_REPEAT,
    ADDRESS_MODE_CLAMP_TO_EDGE,
    ADDRESS_MODE_CLAMP_TO_BORDER
} AddressMode;

typedef enum MipMapMode
{
    MIPMAP_MODE_NEAREST = 0,
    MIPMAP_MODE_LINEAR
} MipMapMode;

typedef union ClearValue
{
    struct
    {
        float r;
        float g;
        float b;
        float a;
    };
    struct
    {
        float    depth;
        uint32_t stencil;
    };
} ClearValue;

typedef enum BufferCreationFlags
{
    /// Default flag (Buffer will use aliased memory, buffer will not be cpu accessible until mapBuffer is called)
    BUFFER_CREATION_FLAG_NONE = 0x0,
    /// Buffer will allocate its own memory (COMMITTED resource)
    BUFFER_CREATION_FLAG_OWN_MEMORY_BIT = 0x1,
    /// Buffer will be persistently mapped
    BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT = 0x2,
    /// Use ESRAM to store this buffer
    BUFFER_CREATION_FLAG_ESRAM = 0x4,
    /// Flag to specify not to allocate descriptors for the resource
    BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION = 0x8,

    BUFFER_CREATION_FLAG_ACCELERATION_STRUCTURE_BUILD_INPUT = 0x10,
    BUFFER_CREATION_FLAG_SHADER_DEVICE_ADDRESS = 0x20,
    BUFFER_CREATION_FLAG_SHADER_BINDING_TABLE = 0x40,
    BUFFER_CREATION_FLAG_MARKER = 0x80,
#ifdef VULKAN
    /* Memory Host Flags */
    BUFFER_CREATION_FLAG_HOST_COHERENT = 0x100,
    BUFFER_CREATION_FLAG_HOST_VISIBLE = 0x200,
#endif

} BufferCreationFlags;
MAKE_ENUM_FLAG(uint32_t, BufferCreationFlags)

typedef enum TextureCreationFlags
{
    /// Default flag (Texture will use default allocation strategy decided by the api specific allocator)
    TEXTURE_CREATION_FLAG_NONE = 0,
    /// Texture will allocate its own memory (COMMITTED resource)
    TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT = 0x01,
    /// Texture will be allocated in memory which can be shared among multiple processes
    TEXTURE_CREATION_FLAG_EXPORT_BIT = 0x02,
    /// Texture will be allocated in memory which can be shared among multiple gpus
    TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT = 0x04,
    /// Texture will be imported from a handle created in another process
    TEXTURE_CREATION_FLAG_IMPORT_BIT = 0x08,
    /// Use ESRAM to store this texture
    TEXTURE_CREATION_FLAG_ESRAM = 0x10,
    /// Use on-tile memory to store this texture
    TEXTURE_CREATION_FLAG_ON_TILE = 0x20,
    /// Prevent compression meta data from generating (XBox)
    TEXTURE_CREATION_FLAG_NO_COMPRESSION = 0x40,
    /// Force 2D instead of automatically determining dimension based on width, height, depth
    TEXTURE_CREATION_FLAG_FORCE_2D = 0x80,
    /// Force 3D instead of automatically determining dimension based on width, height, depth
    TEXTURE_CREATION_FLAG_FORCE_3D = 0x100,
    /// Display target
    TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET = 0x200,
    /// Create an sRGB texture.
    TEXTURE_CREATION_FLAG_SRGB = 0x400,
    /// Create a normal map texture
    TEXTURE_CREATION_FLAG_NORMAL_MAP = 0x800,
    /// Fast clear
    TEXTURE_CREATION_FLAG_FAST_CLEAR = 0x1000,
    /// Fragment mask
    TEXTURE_CREATION_FLAG_FRAG_MASK = 0x2000,
    /// Doubles the amount of array layers of the texture when rendering VR. Also forces the texture to be a 2D Array texture.
    TEXTURE_CREATION_FLAG_VR_MULTIVIEW = 0x4000,
    /// Binds the FFR fragment density if this texture is used as a render target.
    TEXTURE_CREATION_FLAG_VR_FOVEATED_RENDERING = 0x8000,
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
    /// Creates resolve attachment for auto resolve (MSAA on tiled architecture - Resolve can be done on tile through render pass)
    TEXTURE_CREATION_FLAG_CREATE_RESOLVE_ATTACHMENT = 0x10000,
#endif
    TEXTURE_CREATION_FLAG_SAMPLE_LOCATIONS_COMPATIBLE = 0x20000
} TextureCreationFlags;
MAKE_ENUM_FLAG(uint32_t, TextureCreationFlags)

// Used for swapchain
typedef enum ColorSpace
{
    COLOR_SPACE_SDR_LINEAR = 0x0,
    COLOR_SPACE_SDR_SRGB,
    COLOR_SPACE_P2020,         // BT2020 color space with PQ EOTF
    COLOR_SPACE_EXTENDED_SRGB, // Extended sRGB with linear EOTF
} ColorSpace;

// Material Unit test use this enum to index a shader table
COMPILE_ASSERT(GPU_PRESET_COUNT == 7);

typedef struct BufferBarrier
{
    Buffer*       pBuffer;
    ResourceState mCurrentState;
    ResourceState mNewState;
    uint8_t       mBeginOnly : 1;
    uint8_t       mEndOnly : 1;
} BufferBarrier;

typedef struct TextureBarrier
{
    Texture*      pTexture;
    ResourceState mCurrentState;
    ResourceState mNewState;
    uint8_t       mBeginOnly : 1;
    uint8_t       mEndOnly : 1;
    uint8_t       mAcquire : 1;
    uint8_t       mRelease : 1;
    uint8_t       mQueueType : 5;
    /// Specifiy whether following barrier targets particular subresource
    uint8_t       mSubresourceBarrier : 1;
    /// Following values are ignored if mSubresourceBarrier is false
    uint8_t       mMipLevel : 7;
    uint16_t      mArrayLayer;
} TextureBarrier;

typedef struct RenderTargetBarrier
{
    RenderTarget* pRenderTarget;
    ResourceState mCurrentState;
    ResourceState mNewState;
    uint8_t       mBeginOnly : 1;
    uint8_t       mEndOnly : 1;
    uint8_t       mAcquire : 1;
    uint8_t       mRelease : 1;
    uint8_t       mQueueType : 5;
    /// Specifiy whether following barrier targets particular subresource
    uint8_t       mSubresourceBarrier : 1;
    /// Following values are ignored if mSubresourceBarrier is false
    uint8_t       mMipLevel : 7;
    uint16_t      mArrayLayer;
} RenderTargetBarrier;

typedef struct ReadRange
{
    uint64_t mOffset;
    uint64_t mSize;
} ReadRange;

typedef enum QueryType
{
    QUERY_TYPE_TIMESTAMP = 0,
    QUERY_TYPE_OCCLUSION,
    QUERY_TYPE_PIPELINE_STATISTICS,
    QUERY_TYPE_COUNT,
} QueryType;

typedef struct QueryPoolDesc
{
    const char* pName;
    QueryType   mType;
    uint32_t    mQueryCount;
    uint32_t    mNodeIndex;
} QueryPoolDesc;

typedef struct QueryDesc
{
    uint32_t mIndex;
} QueryDesc;

typedef struct QueryPool
{
#if defined(DIRECT3D12)
    struct
    {
        ID3D12QueryHeap* pQueryHeap;
        Buffer*          pReadbackBuffer;
        D3D12_QUERY_TYPE mType;
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        VkQueryPool pQueryPool;
        VkQueryType mType;
        uint32_t    mNodeIndex;
    } mVk;
#endif
#if defined(METAL)
    struct
    {
        // Length: 'mCount'
        // Not dynamic. It is of length n, given when a queryPool is added, look at: mtl_addQueryPool()
        // Take a look into QuerySampleRange in MetalRenderer.mm..
        void*                      pQueries;
        // Sampling done only at encoder level..
        id<MTLCounterSampleBuffer> pSampleBuffer;
        // Offset from the start of their relative origin
        uint32_t                   mRenderSamplesOffset;  // Origin: 0.
        uint32_t                   mComputeSamplesOffset; // Origin: RenderSampleCount * mCount.
        uint32_t                   mType;
    };
#endif
#if defined(ORBIS)
    struct
    {
        OrbisQueryPool mStruct;
        uint32_t       mType;
    };
#endif
#if defined(PROSPERO)
    struct
    {
        ProsperoQueryPool mStruct;
        uint32_t          mType;
    };
#endif
    uint32_t mCount;
    uint32_t mStride;
} QueryPool;

typedef struct PipelineStatisticsQueryData
{
    uint64_t mIAVertices;
    uint64_t mIAPrimitives;
    uint64_t mVSInvocations;
    uint64_t mGSInvocations;
    uint64_t mGSPrimitives;
    uint64_t mCInvocations;
    uint64_t mCPrimitives;
    uint64_t mPSInvocations;
    uint64_t mHSInvocations;
    uint64_t mDSInvocations;
    uint64_t mCSInvocations;
} PipelineStatisticsQueryData;

typedef struct QueryData
{
    union
    {
        struct
        {
            PipelineStatisticsQueryData mPipelineStats;
        };
        struct
        {
            uint64_t mBeginTimestamp;
            uint64_t mEndTimestamp;
        };
        uint64_t mOcclusionCounts;
    };
    bool mValid;
} QueryData;

#if defined(VULKAN)
typedef enum SamplerRange
{
    SAMPLER_RANGE_FULL = 0,
    SAMPLER_RANGE_NARROW = 1,
} SamplerRange;

typedef enum SamplerModelConversion
{
    SAMPLER_MODEL_CONVERSION_RGB_IDENTITY = 0,
    SAMPLER_MODEL_CONVERSION_YCBCR_IDENTITY = 1,
    SAMPLER_MODEL_CONVERSION_YCBCR_709 = 2,
    SAMPLER_MODEL_CONVERSION_YCBCR_601 = 3,
    SAMPLER_MODEL_CONVERSION_YCBCR_2020 = 4,
} SamplerModelConversion;

typedef enum SampleLocation
{
    SAMPLE_LOCATION_COSITED = 0,
    SAMPLE_LOCATION_MIDPOINT = 1,
} SampleLocation;
#endif

typedef enum ResourceHeapCreationFlags
{
    RESOURCE_HEAP_FLAG_NONE = 0,
    RESOURCE_HEAP_FLAG_SHARED = 0x1,
    RESOURCE_HEAP_FLAG_DENY_BUFFERS = 0x2,
    RESOURCE_HEAP_FLAG_ALLOW_DISPLAY = 0x4,
    RESOURCE_HEAP_FLAG_SHARED_CROSS_ADAPTER = 0x8,
    RESOURCE_HEAP_FLAG_DENY_RT_DS_TEXTURES = 0x10,
    RESOURCE_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES = 0x20,
    RESOURCE_HEAP_FLAG_HARDWARE_PROTECTED = 0x40,
    RESOURCE_HEAP_FLAG_ALLOW_WRITE_WATCH = 0x80,
    RESOURCE_HEAP_FLAG_ALLOW_SHADER_ATOMICS = 0x100,

    // These are convenience aliases to manage resource heap tier restrictions. They cannot be bitwise OR'ed together cleanly.
    RESOURCE_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES = 0x200,
    RESOURCE_HEAP_FLAG_ALLOW_ONLY_BUFFERS = RESOURCE_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES | RESOURCE_HEAP_FLAG_DENY_RT_DS_TEXTURES,
    RESOURCE_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES = RESOURCE_HEAP_FLAG_DENY_BUFFERS | RESOURCE_HEAP_FLAG_DENY_RT_DS_TEXTURES,
    RESOURCE_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES = RESOURCE_HEAP_FLAG_DENY_BUFFERS | RESOURCE_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES,
} ResourceHeapCreationFlags;

typedef struct ResourceHeapDesc
{
    uint64_t mSize;
    uint64_t mAlignment;

    ResourceMemoryUsage       mMemoryUsage;
    DescriptorType            mDescriptors;
    ResourceHeapCreationFlags mFlags;

    uint32_t    mNodeIndex;
    uint32_t    mSharedNodeIndexCount;
    uint32_t*   pSharedNodeIndices;
    const char* pName;
} ResourceHeapDesc;

#define ALIGN_ResourceHeap 64
typedef struct DEFINE_ALIGNED(ResourceHeap, ALIGN_ResourceHeap)
{
#if defined(DIRECT3D12)
    struct
    {
        ID3D12Heap* pHeap;

#if defined(XBOX)
        D3D12_GPU_VIRTUAL_ADDRESS mPtr;
#endif
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        struct VmaAllocation_T* pAllocation;
        VkDeviceMemory          pMemory;
        void*                   pCpuMappedAddress;
        uint64_t                mOffset;
    } mVk;
#endif
#if defined(METAL)
    struct
    {
        struct VmaAllocation_T* pAllocation;
        NOREFS id<MTLHeap> pHeap;
    };
#endif
#if defined(ORBIS)
    OrbisResourceHeap mStruct;
#endif
#if defined(PROSPERO)
    ProsperoResourceHeap mStruct;
#endif
    uint64_t mSize;
} ResourceHeap;

typedef struct ResourceSizeAlign
{
    uint64_t mSize;
    uint64_t mAlignment;
} ResourceSizeAlign;

typedef struct ResourcePlacement
{
    ResourceHeap* pHeap;
    uint64_t      mOffset;

#if defined(ORBIS)
    OrbisResourcePlacement mStruct;
#endif
#if defined(PROSPERO)
    ProsperoResourcePlacement mStruct;
#endif
} ResourcePlacement;

/// Data structure holding necessary info to create a Buffer
typedef struct BufferDesc
{
    /// Optional placement (addBuffer will place/bind buffer in this memory instead of allocating space)
    ResourcePlacement*   pPlacement;
    /// Size of the buffer (in bytes)
    uint64_t             mSize;
    /// Set this to specify a counter buffer for this buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
    struct Buffer*       pCounterBuffer;
    /// Index of the first element accessible by the SRV/UAV (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
    uint32_t             mFirstElement;
    /// Number of elements in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
    uint32_t             mElementCount;
    /// Size of each element (in bytes) in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
    uint32_t             mStructStride;
    /// Alignment
    uint32_t             mAlignment;
    /// Debug name used in gpu profile
    const char*          pName;
    uint32_t*            pSharedNodeIndices;
    /// Decides which memory heap buffer will use (default, upload, readback)
    ResourceMemoryUsage  mMemoryUsage;
    /// Creation flags of the buffer
    BufferCreationFlags  mFlags;
    /// What type of queue the buffer is owned by
    QueueType            mQueueType;
    /// What state will the buffer get created in
    ResourceState        mStartState;
    /// ICB draw type
    IndirectArgumentType mICBDrawType;
    /// ICB max commands in indirect command buffer
    uint32_t             mICBMaxCommandCount;
    /// Format of the buffer (applicable to typed storage buffers (Buffer<T>)
    TinyImageFormat      mFormat;
    /// Flags specifying the suitable usage of this buffer (Uniform buffer, Vertex Buffer, Index Buffer,...)
    DescriptorType       mDescriptors;
    /// The index of the GPU in SLI/Cross-Fire that owns this buffer, or the Renderer index in unlinked mode.
    uint32_t             mNodeIndex;
    uint32_t             mSharedNodeIndexCount;
} BufferDesc;

#define ALIGN_Buffer 64
typedef struct DEFINE_ALIGNED(Buffer, ALIGN_Buffer)
{
    /// CPU address of the mapped buffer (applicable to buffers created in CPU accessible heaps (CPU, CPU_TO_GPU, GPU_TO_CPU)
    void* pCpuMappedAddress;
#if defined(DIRECT3D12)
    struct
    {
        /// GPU Address - Cache to avoid calls to ID3D12Resource::GetGpuVirtualAddress
        D3D12_GPU_VIRTUAL_ADDRESS mGpuAddress;
        /// Descriptor handle of the CBV in a CPU visible descriptor heap (applicable to BUFFER_USAGE_UNIFORM)
        DxDescriptorID            mDescriptors;
        /// Offset from mDescriptors for srv descriptor handle
        uint8_t                   mSrvDescriptorOffset;
        /// Offset from mDescriptors for uav descriptor handle
        uint8_t                   mUavDescriptorOffset;
#if !defined(XBOX)
        uint8_t mMarkerBuffer : 1;
#endif
        /// Native handle of the underlying resource
        ID3D12Resource* pResource;
        union
        {
            ID3D12Heap*                pMarkerBufferHeap;
            /// Contains resource allocation info such as parent heap, offset in heap
            struct D3D12MAAllocation_* pAllocation;
        };
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        /// Native handle of the underlying resource
        VkBuffer                pBuffer;
        /// Buffer view
        VkBufferView            pStorageTexelView;
        VkBufferView            pUniformTexelView;
        /// Contains resource allocation info such as parent heap, offset in heap
        struct VmaAllocation_T* pAllocation;
        uint64_t                mOffset;
    } mVk;
#endif
#if defined(METAL)
    struct
    {
        struct VmaAllocation_T*      pAllocation;
        id<MTLBuffer>                pBuffer;
        id<MTLIndirectCommandBuffer> pIndirectCommandBuffer;
        uint64_t                     mOffset;
    };
#endif
#if defined(ORBIS)
    OrbisBuffer mStruct;
#endif
#if defined(PROSPERO)
    ProsperoBuffer mStruct;
#endif
    uint64_t mSize : 32;
    uint64_t mDescriptors : 20;
    uint64_t mMemoryUsage : 3;
    uint64_t mNodeIndex : 4;
} Buffer;
// One cache line
COMPILE_ASSERT(sizeof(Buffer) == 8 * sizeof(uint64_t));

/// Data structure holding necessary info to create a Texture
typedef struct TextureDesc
{
    /// Optional placement (addTexture will place/bind buffer in this memory instead of allocating space)
    ResourcePlacement* pPlacement;
    /// Optimized clear value (recommended to use this same value when clearing the rendertarget)
    ClearValue         mClearValue;
    /// Pointer to native texture handle if the texture does not own underlying resource
    const void*        pNativeHandle;
    /// Debug name used in gpu profile
    const char*        pName;
    /// GPU indices to share this texture
    uint32_t*          pSharedNodeIndices;
#if defined(VULKAN)
    VkSamplerYcbcrConversionInfo* pSamplerYcbcrConversionInfo;
#endif
    /// Texture creation flags (decides memory allocation strategy, sharing access,...)
    TextureCreationFlags mFlags;
    /// Width
    uint32_t             mWidth;
    /// Height
    uint32_t             mHeight;
    /// Depth (Should be 1 if not a mType is not TEXTURE_TYPE_3D)
    uint32_t             mDepth;
    /// Texture array size (Should be 1 if texture is not a texture array or cubemap)
    uint32_t             mArraySize;
    /// Number of mip levels
    uint32_t             mMipLevels;
    /// Number of multisamples per pixel (currently Textures created with mUsage TEXTURE_USAGE_SAMPLED_IMAGE only support SAMPLE_COUNT_1)
    SampleCount          mSampleCount;
    /// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value
    /// appropriate for mSampleCount
    uint32_t             mSampleQuality;
    ///  image format
    TinyImageFormat      mFormat;
    /// What state will the texture get created in
    ResourceState        mStartState;
    /// Descriptor creation
    DescriptorType       mDescriptors;
    /// Number of GPUs to share this texture
    uint32_t             mSharedNodeIndexCount;
    /// GPU which will own this texture
    uint32_t             mNodeIndex;
} TextureDesc;

#define ALIGN_Texture 64
typedef struct DEFINE_ALIGNED(Texture, ALIGN_Texture)
{
#if defined(DIRECT3D12)
    struct
    {
        /// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to TEXTURE_USAGE_SAMPLED_IMAGE)
        DxDescriptorID             mDescriptors;
        /// Native handle of the underlying resource
        ID3D12Resource*            pResource;
        /// Contains resource allocation info such as parent heap, offset in heap
        struct D3D12MAAllocation_* pAllocation;
        uint32_t                   mHandleCount : 24;
        uint32_t                   mUavStartIndex;
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        /// Opaque handle used by shaders for doing read/write operations on the texture
        VkImageView  pSRVDescriptor;
        /// Opaque handle used by shaders for doing read/write operations on the texture
        VkImageView* pUAVDescriptors;
        /// Opaque handle used by shaders for doing read/write operations on the texture
        VkImageView  pSRVStencilDescriptor;
        /// Native handle of the underlying resource
        VkImage      pImage;
        union
        {
            /// Contains resource allocation info such as parent heap, offset in heap
            struct VmaAllocation_T* pAllocation;
            VkDeviceMemory          pDeviceMemory;
        };
    } mVk;
#endif
#if defined(METAL)
    struct
    {
        struct VmaAllocation_T* pAllocation;
        /// Native handle of the underlying resource
        id<MTLTexture>          pTexture;
        union
        {
            id<MTLTexture> __strong* pUAVDescriptors;
            id<MTLTexture>           pStencilTexture;
        };
        id mpsTextureAllocator;
    };
#endif
#if defined(ORBIS)
    OrbisTexture mStruct;
    /// Contains resource allocation info such as parent heap, offset in heap
#endif
#if defined(PROSPERO)
    ProsperoTexture mStruct;
#endif
    /// Current state of the buffer
    uint32_t mWidth : 16;
    uint32_t mHeight : 16;
    uint32_t mDepth : 16;
    uint32_t mMipLevels : 5;
    uint32_t mArraySizeMinusOne : 11;
    uint32_t mFormat : 8;
    /// Flags specifying which aspects (COLOR,DEPTH,STENCIL) are included in the pImageView
    uint32_t mAspectMask : 4;
    uint32_t mNodeIndex : 4;
    uint32_t mSampleCount : 5;
    uint32_t mUav : 1;
    /// This value will be false if the underlying resource is not owned by the texture (swapchain textures,...)
    uint32_t mOwnsImage : 1;
    // Only applies to Vulkan but kept here as adding it inside mVk block increases the size of the struct and triggers assert below
    uint32_t mLazilyAllocated : 1;
} Texture;
// One cache line
COMPILE_ASSERT(sizeof(Texture) == 8 * sizeof(uint64_t));

typedef struct RenderTargetDesc
{
    /// Optional placement (addRenderTarget will place/bind buffer in this memory instead of allocating space)
    ResourcePlacement*   pPlacement;
    /// Texture creation flags (decides memory allocation strategy, sharing access,...)
    TextureCreationFlags mFlags;
    /// Width
    uint32_t             mWidth;
    /// Height
    uint32_t             mHeight;
    /// Depth (Should be 1 if not a mType is not TEXTURE_TYPE_3D)
    uint32_t             mDepth;
    /// Texture array size (Should be 1 if texture is not a texture array or cubemap)
    uint32_t             mArraySize;
    /// Number of mip levels
    uint32_t             mMipLevels;
    /// MSAA
    SampleCount          mSampleCount;
    /// Internal image format
    TinyImageFormat      mFormat;
    /// What state will the texture get created in
    ResourceState        mStartState;
    /// Optimized clear value (recommended to use this same value when clearing the rendertarget)
    ClearValue           mClearValue;
    /// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value
    /// appropriate for mSampleCount
    uint32_t             mSampleQuality;
    /// Descriptor creation
    DescriptorType       mDescriptors;
    const void*          pNativeHandle;
    /// Debug name used in gpu profile
    const char*          pName;
    /// GPU indices to share this texture
    uint32_t*            pSharedNodeIndices;
    /// Number of GPUs to share this texture
    uint32_t             mSharedNodeIndexCount;
    /// GPU which will own this texture
    uint32_t             mNodeIndex;
} RenderTargetDesc;

#define ALIGN_RenderTarget 64
typedef struct DEFINE_ALIGNED(RenderTarget, ALIGN_RenderTarget)
{
    Texture* pTexture;
#if defined(DIRECT3D12)
    struct
    {
        DxDescriptorID mDescriptors;
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        VkImageView  pDescriptor;
        VkImageView* pSliceDescriptors;
        uint32_t     mId;
    } mVk;
#endif
#if defined(ORBIS)
    struct
    {
        OrbisRenderTarget mStruct;
        Texture*          pFragMask;
    };
#endif
#if defined(PROSPERO)
    struct
    {
        ProsperoRenderTarget mStruct;
        Texture*             pFragMask;
    };
#endif
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
    RenderTarget* pResolveAttachment;
#endif
    ClearValue      mClearValue;
    uint32_t        mArraySize : 16;
    uint32_t        mDepth : 16;
    uint32_t        mWidth : 16;
    uint32_t        mHeight : 16;
    uint32_t        mDescriptors : 20;
    uint32_t        mMipLevels : 10;
    uint32_t        mSampleQuality : 5;
    TinyImageFormat mFormat;
    SampleCount     mSampleCount;
    bool            mVRMultiview;
    bool            mVRFoveatedRendering;
} RenderTarget;
COMPILE_ASSERT(sizeof(RenderTarget) <= 32 * sizeof(uint64_t));

typedef struct SampleLocations
{
    int8_t mX;
    int8_t mY;
} SampleLocations;

typedef struct SamplerDesc
{
    FilterType  mMinFilter;
    FilterType  mMagFilter;
    MipMapMode  mMipMapMode;
    AddressMode mAddressU;
    AddressMode mAddressV;
    AddressMode mAddressW;
    float       mMipLodBias;
    bool        mSetLodRange;
    float       mMinLod;
    float       mMaxLod;
    float       mMaxAnisotropy;
    CompareMode mCompareFunc;

#if defined(VULKAN)
    struct
    {
        TinyImageFormat        mFormat;
        SamplerModelConversion mModel;
        SamplerRange           mRange;
        SampleLocation         mChromaOffsetX;
        SampleLocation         mChromaOffsetY;
        FilterType             mChromaFilter;
        bool                   mForceExplicitReconstruction;
    } mSamplerConversionDesc;
#endif
} SamplerDesc;

#define ALIGN_Sampler 16
typedef struct DEFINE_ALIGNED(Sampler, ALIGN_Sampler)
{
#if defined(DIRECT3D12)
    struct
    {
        /// Description for creating the Sampler descriptor for this sampler
        D3D12_SAMPLER_DESC mDesc;
        /// Descriptor handle of the Sampler in a CPU visible descriptor heap
        DxDescriptorID     mDescriptor;
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        /// Native handle of the underlying resource
        VkSampler                    pSampler;
        VkSamplerYcbcrConversion     pSamplerYcbcrConversion;
        VkSamplerYcbcrConversionInfo mSamplerYcbcrConversionInfo;
    } mVk;
#endif
#if defined(METAL)
    struct
    {
        /// Native handle of the underlying resource
        id<MTLSamplerState> pSamplerState;
    };
#endif
#if defined(ORBIS)
    OrbisSampler mStruct;
#endif
#if defined(PROSPERO)
    ProsperoSampler mStruct;
#endif
} Sampler;
#if defined(DIRECT3D12)
COMPILE_ASSERT(sizeof(Sampler) == 8 * sizeof(uint64_t));
#elif defined(VULKAN)
COMPILE_ASSERT(sizeof(Sampler) <= 8 * sizeof(uint64_t));
#else
COMPILE_ASSERT(sizeof(Sampler) == 2 * sizeof(uint64_t));
#endif

typedef struct DescriptorDataRange
{
    uint32_t mOffset;
    uint32_t mSize;
    // Specify different structured buffer stride (ignored for raw buffer - ByteAddressBuffer)
    uint32_t mStructStride;
} DescriptorDataRange;

typedef struct DescriptorData
{
    /// Number of array entries to update (array size of ppTextures/ppBuffers/...)
    uint32_t             mCount : 31;
    // Binds stencil only descriptor instead of color/depth
    uint32_t             mBindStencilResource : 1;
    /// Dst offset into the array descriptor (useful for updating few entries in a large array)
    // Example: to update 6th entry in a bindless texture descriptor, mArrayOffset will be 6 and mCount will be 1)
    uint32_t             mArrayOffset : 20;
    // Index in DescriptorSetDesc::pDescriptors array
    uint32_t             mIndex : 12;
    // Range to bind (buffer offset, size)
    DescriptorDataRange* pRanges;
    union
    {
        struct
        {
            // When binding UAV, control the mip slice to to bind for UAV (example - generating mipmaps in a compute shader)
            uint16_t mUAVMipSlice;
            // Binds entire mip chain as array of UAV
            bool     mBindMipChain;
        };
        struct
        {
            // Bind MTLIndirectCommandBuffer along with the MTLBuffer
            const char* pICBName;
            uint32_t    mICBIndex;
            bool        mBindICB;
        };
    };
    /// Array of resources containing descriptor handles or constant to be used in ring buffer memory - DescriptorRange can hold only one
    /// resource type array
    union
    {
        /// Array of texture descriptors (srv and uav textures)
        Texture**               ppTextures;
        /// Array of sampler descriptors
        Sampler**               ppSamplers;
        /// Array of buffer descriptors (srv, uav and cbv buffers)
        Buffer**                ppBuffers;
        /// Custom binding (raytracing acceleration structure ...)
        AccelerationStructure** ppAccelerationStructures;
    };
} DescriptorData;

#define ALIGN_DescriptorSet 64
typedef struct DEFINE_ALIGNED(DescriptorSet, ALIGN_DescriptorSet)
{
#if defined(DIRECT3D12)
    struct
    {
        /// Start handle to cbv srv uav descriptor table
        DxDescriptorID           mCbvSrvUavHandle;
        /// Start handle to sampler descriptor table
        DxDescriptorID           mSamplerHandle;
        /// Stride of the cbv srv uav descriptor table (number of descriptors * descriptor size)
        uint32_t                 mCbvSrvUavStride;
        /// Stride of the sampler descriptor table (number of descriptors * descriptor size)
        uint32_t                 mSamplerStride;
        const struct Descriptor* pDescriptors;
        uint32_t                 mMaxSets : 16;
        uint32_t                 mNodeIndex : 4;
        uint32_t                 mCbvSrvUavRootIndex : 4;
        uint32_t                 mSamplerRootIndex : 4;
        uint32_t                 mPipelineType : 3;
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        VkDescriptorSet*         pHandles;
        VkDescriptorPool         pDescriptorPool;
        const struct Descriptor* pDescriptors;
        uint32_t                 mMaxSets;
        uint32_t                 mSetIndex;
        uint32_t                 mNodeIndex;
    } mVk;
#endif
#if defined(METAL)
    struct
    {
        id<MTLArgumentEncoder>         mArgumentEncoder;
        Buffer*                        mArgumentBuffer;
        struct UntrackedResourceData** ppUntrackedData;
        const struct Descriptor*       pDescriptors;
        /// Descriptors that are bound without argument buffers
        /// This is necessary since there are argument buffers bugs in some iOS Metal drivers which causes shader compiler crashes or
        /// incorrect shader generation. This makes it necessary to keep fallback descriptor binding path alive
        struct RootDescriptorData*     pRootDescriptorData;
        uint32_t*                      pBindings;
        uint32_t*                      pResourceIndices;
        uint32_t                       mStride;
        uint32_t                       mMaxSets;
        uint32_t                       mRootBufferCount : 10;
        uint32_t                       mRootTextureCount : 10;
        uint32_t                       mRootSamplerCount : 10;
        uint8_t                        mNodeIndex;
        uint8_t                        mStages;
        uint8_t                        mForceArgumentBuffer;
    };
#endif
#if defined(ORBIS)
    OrbisDescriptorSet mStruct;
#endif
#if defined(PROSPERO)
    ProsperoDescriptorSet mStruct;
#endif
} DescriptorSet;

typedef struct CmdPoolDesc
{
    Queue* pQueue;
    bool   mTransient;
} CmdPoolDesc;

typedef struct CmdPool
{
#if defined(DIRECT3D12)
    ID3D12CommandAllocator* pCmdAlloc;
#endif
#if defined(VULKAN)
    VkCommandPool pCmdPool;
#endif
    Queue* pQueue;
} CmdPool;

typedef struct CmdDesc
{
    CmdPool* pPool;
#if defined(ORBIS) || defined(PROSPERO)
    uint32_t mMaxSize;
#endif
    bool mSecondary;
#ifdef ENABLE_GRAPHICS_DEBUG_ANNOTATION
    const char* pName;
#endif // ENABLE_GRAPHICS_DEBUG_ANNOTATION
} CmdDesc;

typedef enum MarkerFlags
{
    /// Default flag
    MARKER_FLAG_NONE = 0,
    MARKER_FLAG_WAIT_FOR_WRITE = 0x1,
} MarkerFlags;
MAKE_ENUM_FLAG(uint8_t, MarkerFlags)

typedef struct MarkerDesc
{
    Buffer*     pBuffer;
    uint32_t    mOffset;
    uint32_t    mValue;
    MarkerFlags mFlags;
} MarkerDesc;

#if !defined(PROSPERO) && !defined(XBOX)
#define GPU_MARKER_SIZE                        sizeof(uint32_t)
#define GPU_MARKER_VALUE(markerBuffer, offset) (*((uint32_t*)markerBuffer->pCpuMappedAddress) + ((offset) / GPU_MARKER_SIZE))
#endif

#if !defined(GFX_ESRAM_ALLOCATIONS)
#define ESRAM_BEGIN_ALLOC(...)
#define ESRAM_CURRENT_OFFSET(renderer, offset) \
    uint32_t offset = 0u;                      \
    UNREF_PARAM(offset);
#define ESRAM_END_ALLOC(...)
#define ESRAM_RESET_ALLOCS(...)
#endif

#define ALIGN_Cmd 64
typedef struct DEFINE_ALIGNED(Cmd, ALIGN_Cmd)
{
#if defined(DIRECT3D12)
    struct
    {
#if defined(XBOX)
        DmaCmd mDma;
#endif
        ID3D12GraphicsCommandList1* pCmdList;
#if defined(ENABLE_GRAPHICS_VALIDATION) && defined(_WINDOWS)
        // For resource state validation
        ID3D12DebugCommandList* pDebugCmdList;
#endif
        // Cached in beginCmd to avoid fetching them during rendering
        struct DescriptorHeap*      pBoundHeaps[2];
        D3D12_GPU_DESCRIPTOR_HANDLE mBoundHeapStartHandles[2];

        // Command buffer state
        D3D12_GPU_DESCRIPTOR_HANDLE mBoundDescriptorSets[2][MAX_DESCRIPTOR_TABLES];
#if defined(XBOX)
        D3D12_SAMPLE_POSITION mSampleLocations[MAX_SAMPLE_LOCATIONS];
#endif
        uint32_t mNodeIndex : 4;
        uint32_t mType : 3;
        uint32_t mPipelineType : 3;
#if defined(XBOX)
        // Required for setting occlusion query control
        uint32_t mSampleCount : 5;
        // Required for setting sample locations
        uint32_t mNumPixel : 3;
#endif
        CmdPool* pCmdPool;
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        VkCommandBuffer  pCmdBuf;
        VkRenderPass     pActiveRenderPass;
        VkPipeline       pBoundPipeline;
        VkPipelineLayout pBoundPipelineLayout;
        // VkSampleLocationEXT is 8 byte each. Choose to store SampleLocation instead.
        SampleLocations  mSampleLocations[MAX_SAMPLE_LOCATIONS];
        CmdPool*         pCmdPool;
        uint32_t         mNodeIndex : 4;
        uint32_t         mType : 3;
        uint32_t         mPipelineType : 3;
        uint32_t         mIsRendering : 1;
        // Required for vkSetSampleLocations
        uint32_t         mGridSizeX : 2;
        uint32_t         mGridSizeY : 2;
        uint32_t         mSampleCount : 5;
    } mVk;
#endif
#if defined(METAL)
    struct
    {
        id<MTLCommandBuffer>         pCommandBuffer;
        id<MTLRenderCommandEncoder>  pRenderEncoder;
        id<MTLComputeCommandEncoder> pComputeEncoder;
        id<MTLBlitCommandEncoder>    pBlitEncoder;
#if defined(MTL_RAYTRACING_AVAILABLE)
        id<MTLAccelerationStructureCommandEncoder> pASEncoder IOS17_API;
#endif

        // Stored in cmdBindPipeline. Used in
        // - cmdDraw functions to check for tessellation and patch control point count
        // - cmdDispatch functions to check for num threads per group (Metal needs to specify numThreadsPerThreadGroup explicitly)
        Pipeline*  pBoundPipeline;
        QueryPool* pCurrentQueryPool;
        int32_t    mCurrentQueryIndex;
        // Stored in cmdBindIndexBuffer and used in cmdDrawIndexed functions (no bindIndexBuffer in Metal)
        NOREFS id<MTLBuffer> mBoundIndexBuffer;
        // Stored in cmdBindIndexBuffer and used in cmdDrawIndexed functions (no bindIndexBuffer in Metal)
        uint32_t             mBoundIndexBufferOffset;
        // Stored in cmdBindIndexBuffer and used in cmdDrawIndexed functions (no bindIndexBuffer in Metal)
        uint32_t             mIndexType : 2;
        // Stored in cmdBindIndexBuffer and used in cmdDrawIndexed functions (no bindIndexBuffer in Metal)
        uint32_t             mIndexStride : 3;
        // Stored in cmdBindPipeline and used in all draw functions (primitive type does not go in PSO but specified in the draw call)
        uint32_t             mSelectedPrimitiveType : 4;
        uint32_t             mPipelineType : 3;
        uint32_t             mShouldRebindPipeline : 1;
        DescriptorSet*       mBoundDescriptorSets[MAX_DESCRIPTOR_SETS];
        uint32_t             mBoundDescriptorSetIndices[MAX_DESCRIPTOR_SETS];
#ifdef ENABLE_DRAW_INDEX_BASE_VERTEX_FALLBACK
        // When first vertex is not supported for indexed draw, we have to offset the
        // vertex buffer manually using setVertexBufferOffset
        // mOffsets, mStrides stored in cmdBindVertexBuffer and used in cmdDrawIndexed functions
        uint32_t mOffsets[MAX_VERTEX_BINDINGS];
        uint32_t mStrides[MAX_VERTEX_BINDINGS];
        uint32_t mFirstVertex;
#endif
#ifdef ENABLE_GRAPHICS_DEBUG_ANNOTATION
        char mDebugMarker[MAX_DEBUG_NAME_LENGTH];
#endif
    };
#endif
#if defined(ORBIS)
    OrbisCmd mStruct;
#endif
#if defined(PROSPERO)
    ProsperoCmd mStruct;
#endif
    Renderer* pRenderer;
    Queue*    pQueue;
} Cmd;
COMPILE_ASSERT(sizeof(Cmd) <= 64 * sizeof(uint64_t));

typedef enum FenceStatus
{
    FENCE_STATUS_COMPLETE = 0,
    FENCE_STATUS_INCOMPLETE,
    FENCE_STATUS_NOTSUBMITTED,
} FenceStatus;

typedef struct Fence
{
#if defined(DIRECT3D12)
    struct
    {
        ID3D12Fence* pFence;
        HANDLE       pWaitIdleFenceEvent;
        uint64_t     mFenceValue;
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        VkFence pFence;
    } mVk;
#endif
#if defined(METAL)
    struct
    {
        dispatch_semaphore_t pSemaphore;
        uint32_t             mSubmitted : 1;
    };
#endif
#if defined(ORBIS)
    OrbisFence mStruct;
#endif
#if defined(PROSPERO)
    ProsperoFence mStruct;
#endif
} Fence;

typedef struct Semaphore
{
#if defined(DIRECT3D12)
    // DirectX12 does not have a concept of semaphores
    // All synchronization is done using fences
    // Simlate semaphore signal and wait using DirectX12 fences

    // Semaphores used in DirectX12 only in queueSubmit
    // queueSubmit -> How the semaphores work in DirectX12

    // pp_wait_semaphores -> queue->Wait is manually called on each fence in this
    // array before calling ExecuteCommandLists to make the fence work like a wait semaphore

    // pp_signal_semaphores -> Manually call queue->Signal on each fence in this array after
    // calling ExecuteCommandLists and increment the underlying fence value

    // queuePresent does not use the wait semaphore since the swapchain Present function
    // already does the synchronization in this case
    struct
    {
        ID3D12Fence* pFence;
        HANDLE       pWaitIdleFenceEvent;
        uint64_t     mFenceValue;
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        VkSemaphore pSemaphore;
        uint32_t    mCurrentNodeIndex : 5;
        uint32_t    mSignaled : 1;
    } mVk;
#endif
#if defined(METAL)
    struct
    {
        id<MTLEvent> pSemaphore;
        uint64_t     mValue : 63;
        uint64_t     mSignaled : 1;
    };
#endif
#if defined(ORBIS)
    OrbisSemaphore mStruct;
#endif
#if defined(PROSPERO)
    ProsperoSemaphore mStruct;
#endif
} Semaphore;

typedef struct QueueDesc
{
    QueueType     mType;
    QueueFlag     mFlag;
    QueuePriority mPriority;
    uint32_t      mNodeIndex;
    const char*   pName;
} QueueDesc;

typedef struct Queue
{
#if defined(DIRECT3D12)
    struct
    {
        ID3D12CommandQueue* pQueue;
        Fence*              pFence;
#if defined(_WINDOWS) && defined(FORGE_DEBUG)
        // To silence mismatching command list on Windows 11 multi GPU
        Renderer* pRenderer;
#endif
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        VkQueue   pQueue;
        Renderer* pRenderer;
        Mutex*    pSubmitMutex;
        float     mTimestampPeriod;
        uint32_t  mQueueFamilyIndex : 5;
        uint32_t  mQueueIndex : 5;
        uint32_t  mGpuMode : 3;
    } mVk;
#endif
#if defined(METAL)
    struct
    {
        id<MTLCommandQueue> pCommandQueue;
        id<MTLFence>        pQueueFence;
        uint32_t            mBarrierFlags;
    };
#endif
#if defined(ORBIS)
    OrbisQueue mStruct;
#endif
#if defined(PROSPERO)
    ProsperoQueue mStruct;
#endif
    uint32_t mType : 3;
    uint32_t mNodeIndex : 4;
} Queue;

/// ShaderConstant only supported by Vulkan and Metal APIs
typedef struct ShaderConstant
{
    const void* pValue;
    uint32_t    mIndex;
    uint32_t    mSize;
} ShaderConstant;

typedef struct BinaryShaderStageDesc
{
    const char* pName;
#if defined(PROSPERO)
    ProsperoBinaryShaderStageDesc mStruct;
#else
    /// Byte code array
    void* pByteCode;
    uint32_t mByteCodeSize;
    const char* pEntryPoint;
#if defined(METAL)
    uint32_t mNumThreadsPerGroup[3];
    uint32_t mOutputRenderTargetTypesMask;
#endif
#endif
} BinaryShaderStageDesc;

typedef struct BinaryShaderDesc
{
    ShaderStage           mStages;
    /// Specify whether shader will own byte code memory
    uint32_t              mOwnByteCode : 1;
    BinaryShaderStageDesc mVert;
    BinaryShaderStageDesc mFrag;
    BinaryShaderStageDesc mGeom;
    BinaryShaderStageDesc mHull;
    BinaryShaderStageDesc mDomain;
    BinaryShaderStageDesc mComp;
    const ShaderConstant* pConstants;
    uint32_t              mConstantCount;
#if defined(QUEST_VR)
    bool mIsMultiviewVR : 1;
#endif
} BinaryShaderDesc;

typedef struct Shader
{
    ShaderStage mStages : 31;
    bool        mIsMultiviewVR : 1;
    uint32_t    mNumThreadsPerGroup[3];
    uint32_t    mOutputRenderTargetTypesMask;
#if defined(DIRECT3D12)
    struct
    {
        LPCWSTR*                 pEntryNames;
        struct IDxcBlobEncoding* pVSBlob;
        struct IDxcBlobEncoding* pHSBlob;
        struct IDxcBlobEncoding* pDSBlob;
        struct IDxcBlobEncoding* pGSBlob;
        struct IDxcBlobEncoding* pPSBlob;
        struct IDxcBlobEncoding* pCSBlob;
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        union
        {
            struct
            {
                VkShaderModule pVS;
                VkShaderModule pDS;
                VkShaderModule pHS;
                VkShaderModule pGS;
                VkShaderModule pPS;
            };
            VkShaderModule pCS;
        };
        VkSpecializationInfo* pSpecializationInfo;
    } mVk;
#endif
#if defined(METAL)
    struct
    {
        id<MTLFunction> pVertexShader;
        id<MTLFunction> pFragmentShader;
        id<MTLFunction> pComputeShader;
        uint32_t        mTessellation : 1;
        uint32_t        mICB : 1;
    };
#endif
#if defined(ORBIS)
    OrbisShader mStruct;
#endif
#if defined(PROSPERO)
    ProsperoShader mStruct;
#endif

    uint32_t mNumControlPoints;

} Shader;

typedef struct BlendStateDesc
{
    /// Source blend factor per render target.
    BlendConstant     mSrcFactors[MAX_RENDER_TARGET_ATTACHMENTS];
    /// Destination blend factor per render target.
    BlendConstant     mDstFactors[MAX_RENDER_TARGET_ATTACHMENTS];
    /// Source alpha blend factor per render target.
    BlendConstant     mSrcAlphaFactors[MAX_RENDER_TARGET_ATTACHMENTS];
    /// Destination alpha blend factor per render target.
    BlendConstant     mDstAlphaFactors[MAX_RENDER_TARGET_ATTACHMENTS];
    /// Blend mode per render target.
    BlendMode         mBlendModes[MAX_RENDER_TARGET_ATTACHMENTS];
    /// Alpha blend mode per render target.
    BlendMode         mBlendAlphaModes[MAX_RENDER_TARGET_ATTACHMENTS];
    /// Write mask per render target.
    ColorMask         mColorWriteMasks[MAX_RENDER_TARGET_ATTACHMENTS];
    /// Mask that identifies the render targets affected by the blend state.
    BlendStateTargets mRenderTargetMask;
    /// Set whether alpha to coverage should be enabled.
    bool              mAlphaToCoverage;
    /// Set whether each render target has an unique blend function. When false the blend function in slot 0 will be used for all render
    /// targets.
    bool              mIndependentBlend;
} BlendStateDesc;

typedef struct DepthStateDesc
{
    bool        mDepthTest;
    bool        mDepthWrite;
    CompareMode mDepthFunc;
    bool        mStencilTest;
    uint8_t     mStencilReadMask;
    uint8_t     mStencilWriteMask;
    CompareMode mStencilFrontFunc;
    StencilOp   mStencilFrontFail;
    StencilOp   mDepthFrontFail;
    StencilOp   mStencilFrontPass;
    CompareMode mStencilBackFunc;
    StencilOp   mStencilBackFail;
    StencilOp   mDepthBackFail;
    StencilOp   mStencilBackPass;
} DepthStateDesc;

typedef struct RasterizerStateDesc
{
    CullMode  mCullMode;
    int32_t   mDepthBias;
    float     mSlopeScaledDepthBias;
    FillMode  mFillMode;
    FrontFace mFrontFace;
    bool      mMultiSample;
    bool      mScissor;
    bool      mDepthClampEnable;
} RasterizerStateDesc;

typedef enum VertexBindingRate
{
    VERTEX_BINDING_RATE_VERTEX = 0,
    VERTEX_BINDING_RATE_INSTANCE = 1,
    VERTEX_BINDING_RATE_COUNT,
} VertexBindingRate;

typedef struct VertexBinding
{
    uint32_t          mStride;
    VertexBindingRate mRate;
} VertexBinding;

typedef struct VertexAttrib
{
    ShaderSemantic  mSemantic;
    uint32_t        mSemanticNameLength;
    char            mSemanticName[MAX_SEMANTIC_NAME_LENGTH];
    TinyImageFormat mFormat;
    uint32_t        mBinding;
    uint32_t        mLocation;
    uint32_t        mOffset;
} VertexAttrib;

typedef struct VertexLayout
{
    VertexBinding mBindings[MAX_VERTEX_BINDINGS];
    VertexAttrib  mAttribs[MAX_VERTEX_ATTRIBS];
    uint32_t      mBindingCount;
    uint32_t      mAttribCount;
} VertexLayout;

#if defined(VULKAN)
typedef struct StaticSamplerDesc
{
    SamplerDesc mDesc;
    uint32_t    mBinding;
} StaticSamplerDesc;
#endif

#if defined(METAL)
// for metal : Buffers 0 to 3 are reserved for Argument buffers
#define METAL_BUFFER_BIND_START_INDEX  4
#define METAL_TEXTURE_BIND_START_INDEX 0
struct MetalDescriptorSet
{
    void*       pUnused;
    uint32_t    mDescriptorCount;
    Descriptor* pDescriptors;
};
#endif

typedef struct Descriptor
{
    IF_VALIDATE_DESCRIPTOR_MEMBER(const char*, pName)
    IF_VALIDATE_DESCRIPTOR_MEMBER(uint32_t, mSetIndex)
    DescriptorType mType;
    uint32_t       mCount;
    uint32_t       mOffset;
#if defined(METAL)
    uint32_t mUseArgumentBuffer;
#endif
} Descriptor;

typedef struct DescriptorSetDesc
{
    uint32_t          mIndex;
    uint32_t          mMaxSets;
    uint32_t          mNodeIndex;
    uint32_t          mDescriptorCount;
    const Descriptor* pDescriptors;
#if defined(METAL)
    uint32_t                  mForceArgumentBuffer;
    const MetalDescriptorSet* pSrtSets;
    uint32_t                  mSrtSetCount;
#endif
#if defined(VULKAN)
    const StaticSamplerDesc* pStaticSamplers;
    uint32_t                 mStaticSamplerCount;
#endif
} DescriptorSetDesc;

#if defined(VULKAN)
typedef struct DescriptorSetLayoutDesc
{
    const Descriptor*        pDescriptors;
    const StaticSamplerDesc* pStaticSamplers;
    uint32_t                 mDescriptorCount;
    uint32_t                 mStaticSamplerCount;
} DescriptorSetLayoutDesc;
#endif

typedef struct GraphicsPipelineDesc
{
    Shader*              pShaderProgram;
    VertexLayout*        pVertexLayout;
    BlendStateDesc*      pBlendState;
    DepthStateDesc*      pDepthState;
    RasterizerStateDesc* pRasterizerState;
    TinyImageFormat*     pColorFormats;
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
    /// Used to specify resolve attachment for render pass
    StoreActionType* pColorResolveActions;
#endif
    uint32_t          mRenderTargetCount;
    SampleCount       mSampleCount;
    uint32_t          mSampleQuality;
    TinyImageFormat   mDepthStencilFormat;
    PrimitiveTopology mPrimitiveTopo;
    bool              mSupportIndirectCommandBuffer;
    bool              mVRFoveatedRendering;
    bool              mUseCustomSampleLocations;
} GraphicsPipelineDesc;

typedef struct ComputePipelineDesc
{
    Shader* pShaderProgram;
} ComputePipelineDesc;

#if defined(ENABLE_WORKGRAPH)
typedef struct WorkgraphPipelineDesc
{
    Shader*     pShaderProgram;
    const char* pWorkgraphName;
} WorkgraphPipelineDesc;
#endif

typedef struct PipelineDesc
{
    union
    {
        ComputePipelineDesc  mComputeDesc;
        GraphicsPipelineDesc mGraphicsDesc;
#if defined(ENABLE_WORKGRAPH)
        WorkgraphPipelineDesc mWorkgraphDesc;
#endif
    };
    PipelineCache* pCache;
    void*          pPipelineExtensions;
    const char*    pName;
    PipelineType   mType;
    uint32_t       mExtensionCount;
#if defined(VULKAN)
    const DescriptorSetLayoutDesc** pLayouts;
    uint32_t                        mLayoutCount;
#endif
} PipelineDesc;

#define ALIGN_Pipeline 64
typedef struct DEFINE_ALIGNED(Pipeline, ALIGN_Pipeline)
{
#if defined(DIRECT3D12)
    struct
    {
        union
        {
            ID3D12PipelineState* pPipelineState;
#if defined(ENABLE_WORKGRAPH)
            struct
            {
                ID3D12StateObject* pStateObject;
                WCHAR*             pWorkgraphName;
            };
#endif
        };
        PipelineType           mType;
        D3D_PRIMITIVE_TOPOLOGY mPrimitiveTopology;
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        VkPipeline             pPipeline;
        struct PipelineLayout* pPipelineLayout;
        PipelineType           mType;
#if defined(SHADER_STATS_AVAILABLE)
        ShaderStage mShaderStages;
#endif
    } mVk;
#endif
#if defined(METAL)
    struct
    {
        id<MTLRenderPipelineState>  pRenderPipelineState;
        id<MTLComputePipelineState> pComputePipelineState;
        id<MTLDepthStencilState>    pDepthStencilState;
        union
        {
            // Graphics
            struct
            {
                uint32_t mCullMode : 3;
                uint32_t mFillMode : 3;
                uint32_t mWinding : 3;
                uint32_t mDepthClipMode : 1;
                uint32_t mPrimitiveType : 4;
                // Between 0-32
                uint32_t mPatchControlPointCount : 6;
                uint32_t mTessellation : 1;
                float    mDepthBias;
                float    mSlopeScale;
            };
            // Compute
            struct
            {
                MTLSize mNumThreadsPerGroup;
            };
        };
        PipelineType mType;
    };
#endif
#if defined(ORBIS)
    OrbisPipeline mStruct;
#endif
#if defined(PROSPERO)
    ProsperoPipeline mStruct;
#endif
} Pipeline;
#if defined(ORBIS)
// Requires more cache lines due to no concept of an encapsulated pipeline state object
COMPILE_ASSERT(sizeof(Pipeline) <= 64 * sizeof(uint64_t));
#elif defined(PROSPERO)
COMPILE_ASSERT(sizeof(Pipeline) == 16 * sizeof(uint64_t));
#elif defined(ENABLE_DEPENDENCY_TRACKER)
// Two cache lines
COMPILE_ASSERT(sizeof(Pipeline) <= 16 * sizeof(uint64_t));
#else
// One cache line
COMPILE_ASSERT(sizeof(Pipeline) == 8 * sizeof(uint64_t));
#endif

typedef enum PipelineCacheFlags
{
    PIPELINE_CACHE_FLAG_NONE = 0x0,
    PIPELINE_CACHE_FLAG_EXTERNALLY_SYNCHRONIZED = 0x1,
} PipelineCacheFlags;
MAKE_ENUM_FLAG(uint32_t, PipelineCacheFlags);

typedef struct PipelineCacheDesc
{
    /// Initial pipeline cache data (can be NULL which means empty pipeline cache)
    void*              pData;
    /// Initial pipeline cache size
    size_t             mSize;
    PipelineCacheFlags mFlags;
} PipelineCacheDesc;

typedef struct PipelineCache
{
#if defined(DIRECT3D12)
    struct
    {
        ID3D12PipelineLibrary* pLibrary;
        void*                  pData;
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        VkPipelineCache pCache;
    } mVk;
#endif
} PipelineCache;

#if defined(SHADER_STATS_AVAILABLE)
typedef struct ShaderStats
{
#if defined(VULKAN)
    struct
    {
        void*    pDisassemblyAMD;
        uint32_t mDisassemblySize;
    } mVk;
#endif
    uint32_t mUsedVgprs;
    uint32_t mUsedSgprs;
    uint32_t mLdsSizePerLocalWorkGroup;
    uint32_t mLdsUsageSizeInBytes;
    uint32_t mScratchMemUsageInBytes;
    uint32_t mPhysicalVgprs;
    uint32_t mPhysicalSgprs;
    uint32_t mAvailableVgprs;
    uint32_t mAvailableSgprs;
    uint32_t mComputeWorkGroupSize[3];
    bool     mValid;
} ShaderStats;

typedef struct PipelineStats
{
    ShaderStats mVert;
    ShaderStats mHull;
    ShaderStats mDomain;
    ShaderStats mGeom;
    ShaderStats mFrag;
    ShaderStats mComp;
} PipelineStats;
#endif

#if defined(ENABLE_WORKGRAPH)
typedef struct WorkgraphDesc
{
    Pipeline* pPipeline;
} WorkgraphDesc;

typedef struct Workgraph
{
    Buffer*   pBackingBuffer;
    Pipeline* pPipeline;
#if defined(DIRECT3D12)
    D3D12_PROGRAM_IDENTIFIER mId;
#endif
} Workgraph;

typedef enum DispatchGraphInputType
{
    DISPATCH_GRAPH_INPUT_CPU = 0,
    DISPATCH_GRAPH_INPUT_GPU,
    DISPATCH_GRAPH_INPUT_COUNT,
} DispatchGraphInputType;

typedef struct DispatchGraphDesc
{
    Workgraph* pWorkgraph;
    union
    {
        struct
        {
            void*    pInput;
            uint32_t mInputStride;
        };
        struct
        {
            Buffer*  pInputBuffer;
            uint32_t mInputBufferOffset;
        };
    };
    DispatchGraphInputType mInputType;
    bool                   mInitialize;
} DispatchGraphDesc;
#endif

typedef enum SwapChainCreationFlags
{
    SWAP_CHAIN_CREATION_FLAG_NONE = 0x0,
    SWAP_CHAIN_CREATION_FLAG_ENABLE_FOVEATED_RENDERING_VR = 0x1,
} SwapChainCreationFlags;
MAKE_ENUM_FLAG(uint32_t, SwapChainCreationFlags);

typedef struct SwapChainDesc
{
    /// Window handle
    WindowHandle           mWindowHandle;
    /// Queues which should be allowed to present
    Queue**                ppPresentQueues;
    /// Number of present queues
    uint32_t               mPresentQueueCount;
    /// Number of backbuffers in this swapchain
    uint32_t               mImageCount;
    /// Width of the swapchain
    uint32_t               mWidth;
    /// Height of the swapchain
    uint32_t               mHeight;
    /// Color format of the swapchain
    TinyImageFormat        mColorFormat;
    /// Clear value
    ClearValue             mColorClearValue;
    /// Swapchain creation flags
    SwapChainCreationFlags mFlags;
    /// Set whether swap chain will be presented using vsync
    bool                   mEnableVsync;
    /// We can toggle to using FLIP model if app desires.
    bool                   mUseFlipSwapEffect;
    /// Optional colorspace for HDR
    ColorSpace             mColorSpace;
} SwapChainDesc;

typedef struct SwapChain
{
    /// Render targets created from the swapchain back buffers
    RenderTarget** ppRenderTargets;
#if defined(DIRECT3D12)
    struct
    {
#if defined(XBOX)
        uint64_t mFramePipelineToken;
        /// Sync interval to specify how interval for vsync
        uint32_t mSyncInterval : 3;
        uint32_t mFlags : 10;
        uint32_t mIndex;
        void*    pWindow;
        Queue*   pPresentQueue;
#else
        /// Use IDXGISwapChain3 for now since IDXGISwapChain4
        /// isn't supported by older devices.
        IDXGISwapChain3*                         pSwapChain;
        /// Sync interval to specify how interval for vsync
        uint32_t                                 mSyncInterval : 3;
        uint32_t                                 mFlags : 10;
#endif
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        /// Present queue if one exists (queuePresent will use this queue if the hardware has a dedicated present queue)
        VkQueue        pPresentQueue;
        VkSwapchainKHR pSwapChain;
        VkSurfaceKHR   pSurface;
        SwapChainDesc* pDesc;
        uint32_t       mPresentQueueFamilyIndex : 5;
    } mVk;
#endif
#if defined(METAL)
    struct
    {
#if defined(TARGET_IOS)
        UIView* pForgeView;
#else
        NSView*                                  pForgeView;
#endif
        id<CAMetalDrawable>  mMTKDrawable;
        id<MTLCommandBuffer> presentCommandBuffer;
        uint32_t             mIndex;
    };
#endif
#if defined(ORBIS)
    OrbisSwapChain mStruct;
#endif
#if defined(PROSPERO)
    ProsperoSwapChain mStruct;
#endif
#if defined(QUEST_VR)
    struct
    {
        struct ovrTextureSwapChain* pSwapChain;
        VkExtent2D*                 pFragmentDensityTextureSizes;
        RenderTarget**              ppFragmentDensityMasks;
    } mVR;
#endif
    uint32_t        mImageCount : 8;
    uint32_t        mEnableVsync : 1;
    ColorSpace      mColorSpace : 4;
    TinyImageFormat mFormat : 8;
} SwapChain;

typedef enum ShaderTarget
{
    // 5.1 is supported on all DX12 hardware
    SHADER_TARGET_5_1,
    SHADER_TARGET_6_0,
    SHADER_TARGET_6_1,
    SHADER_TARGET_6_2,
    SHADER_TARGET_6_3, // required for Raytracing
    SHADER_TARGET_6_4, // required for VRS
} ShaderTarget;

typedef enum GpuMode
{
    GPU_MODE_SINGLE = 0,
    GPU_MODE_LINKED,
    GPU_MODE_UNLINKED,
} GpuMode;

typedef struct RendererDesc
{
#if defined(DIRECT3D12)
    struct
    {
        D3D_FEATURE_LEVEL mFeatureLevel;
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        const char** ppInstanceLayers;
        const char** ppInstanceExtensions;
        const char** ppDeviceExtensions;
        uint32_t     mInstanceLayerCount;
        uint32_t     mInstanceExtensionCount;
        uint32_t     mDeviceExtensionCount;
        /// Flag to specify whether to request all queues from the gpu or just one of each type
        /// This will affect memory usage - Around 200 MB more used if all queues are requested
        bool         mRequestAllAvailableQueues;
    } mVk;
#endif
#if defined(ORBIS)
    OrbisExtendedDesc mExt;
#endif
#if defined(PROSPERO)
    ProsperoExtendedDesc mExt;
#endif

    ShaderTarget mShaderTarget;
    GpuMode      mGpuMode;

    /// Apps may want to query additional state for their applications. That information is transferred through here.
    ExtendedSettings* pExtendedSettings;

    /// Required when creating unlinked multiple renderers. Optional otherwise, can be used for explicit GPU selection.
    RendererContext* pContext;
    uint32_t         mGpuIndex;

    /// This results in new validation not possible during API calls on the CPU, by creating patched shaders that have validation added
    /// directly to the shader. However, it can slow things down a lot, especially for applications with numerous PSOs. Time to see the
    /// first render frame may take several minutes
    bool mEnableGpuBasedValidation;
#if defined(SHADER_STATS_AVAILABLE)
    bool mEnableShaderStats;
#endif

    // to align on PC on 40 bytes
    bool mPaddingA;
    bool mPaddingB;
    bool mPaddingC;
} RendererDesc;

typedef struct GPUVendorPreset
{
    uint32_t       mVendorId;
    uint32_t       mModelId;
    uint32_t       mRevisionId; // Optional as not all gpu's have that. Default is : 0x00
    GPUPresetLevel mPresetLevel;
    char           mVendorName[MAX_GPU_VENDOR_STRING_LENGTH];
    char           mGpuName[MAX_GPU_VENDOR_STRING_LENGTH]; // If GPU Name is missing then value will be empty string
    char           mGpuDriverVersion[MAX_GPU_VENDOR_STRING_LENGTH];
    char           mGpuDriverDate[MAX_GPU_VENDOR_STRING_LENGTH];
    uint32_t       mRTCoresCount;
} GPUVendorPreset;

// if you made change to this structure, please update GraphicsConfig.cpp FORMAT_CAPABILITY_COUNT
typedef enum FormatCapability
{
    FORMAT_CAP_NONE = 0,
    FORMAT_CAP_LINEAR_FILTER = 0x1,
    FORMAT_CAP_READ = 0x2,
    FORMAT_CAP_WRITE = 0x4,
    FORMAT_CAP_READ_WRITE = 0x8,
    FORMAT_CAP_RENDER_TARGET = 0x10,
} FormatCapability;
MAKE_ENUM_FLAG(uint32_t, FormatCapability);

typedef enum WaveOpsSupportFlags
{
    WAVE_OPS_SUPPORT_FLAG_NONE = 0x0,
    WAVE_OPS_SUPPORT_FLAG_BASIC_BIT = 0x00000001,
    WAVE_OPS_SUPPORT_FLAG_VOTE_BIT = 0x00000002,
    WAVE_OPS_SUPPORT_FLAG_ARITHMETIC_BIT = 0x00000004,
    WAVE_OPS_SUPPORT_FLAG_BALLOT_BIT = 0x00000008,
    WAVE_OPS_SUPPORT_FLAG_SHUFFLE_BIT = 0x00000010,
    WAVE_OPS_SUPPORT_FLAG_SHUFFLE_RELATIVE_BIT = 0x00000020,
    WAVE_OPS_SUPPORT_FLAG_CLUSTERED_BIT = 0x00000040,
    WAVE_OPS_SUPPORT_FLAG_QUAD_BIT = 0x00000080,
    WAVE_OPS_SUPPORT_FLAG_PARTITIONED_BIT_NV = 0x00000100,
    WAVE_OPS_SUPPORT_FLAG_ALL = 0x7FFFFFFF
} WaveOpsSupportFlags;
MAKE_ENUM_FLAG(uint32_t, WaveOpsSupportFlags);

typedef enum UMASupportFlags
{
    UMA_SUPPORT_NONE = 0x0,
    UMA_SUPPORT_READ = 0x1,
    UMA_SUPPORT_WRITE = 0x2,
    UMA_SUPPORT_READ_WRITE = UMA_SUPPORT_READ | UMA_SUPPORT_WRITE,
} UMASupportFlags;
MAKE_ENUM_FLAG(uint8_t, UMASupportFlags);

typedef struct GpuDesc
{
#if defined(DIRECT3D12)
    struct
    {
#if defined(XBOX)
        IDXGIAdapter* pGpu;
        ID3D12Device* pDevice;
#elif defined(DIRECT3D12)
        IDXGIAdapter4*                           pGpu;
#endif
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        VkPhysicalDevice            pGpu;
        VkPhysicalDeviceProperties2 mGpuProperties;
    } mVk;
#endif

#if defined(METAL)
    id<MTLDevice>     pGPU;
    id<MTLCounterSet> pCounterSetTimestamp;
    uint32_t          mDrawBoundarySamplingSupported : 1;
    uint32_t          mStageBoundarySamplingSupported : 1;
#endif

    FormatCapability mFormatCaps[TinyImageFormat_Count];

    /*************************************************************************************/
    // GPU Properties
    /*************************************************************************************/
    // update availableGpuProperties, setDefaultGPUProperties in GraphicsConfig.cpp
    // if you made changes to this list
    uint64_t mVRAM;
    uint32_t mUniformBufferAlignment;
    uint32_t mUploadBufferAlignment;
    uint32_t mUploadBufferTextureAlignment;
    uint32_t mUploadBufferTextureRowAlignment;
    uint32_t mMaxVertexInputBindings;
#if defined(DIRECT3D12)
    uint32_t mMaxRootSignatureDWORDS;
#endif
    uint32_t            mWaveLaneCount;
    WaveOpsSupportFlags mWaveOpsSupportFlags;
    GPUVendorPreset     mGpuVendorPreset;
    ShaderStage         mWaveOpsSupportedStageFlags;

    uint32_t mMaxTotalComputeThreads;
    uint32_t mMaxComputeThreads[3];
    uint32_t mMultiDrawIndirect : 1;
    uint32_t mMultiDrawIndirectCount : 1;
    uint32_t mRootConstant : 1;
    uint32_t mIndirectRootConstant : 1;
    uint32_t mBuiltinDrawID : 1;
    uint32_t mIndirectCommandBuffer : 1;
    uint32_t mROVsSupported : 1;
    uint32_t mTessellationSupported : 1;
    uint32_t mGeometryShaderSupported : 1;
    uint32_t mGpuMarkers : 1;
    uint32_t mHDRSupported : 1;
    uint32_t mTimestampQueries : 1;
    uint32_t mOcclusionQueries : 1;
    uint32_t mPipelineStatsQueries : 1;
    uint32_t mAllowBufferTextureInSameHeap : 1;
    uint32_t mRaytracingSupported : 1;
    uint32_t mUnifiedMemorySupport : 2;
    uint32_t mRayPipelineSupported : 1;
    uint32_t mRayQuerySupported : 1;
    uint32_t mWorkgraphSupported : 1;
    uint32_t mSoftwareVRSSupported : 1;
    uint32_t mPrimitiveIdSupported : 1;
    uint32_t mPrimitiveIdPsSupported : 1;
    uint32_t m64BitAtomicsSupported : 1;
#if defined(DIRECT3D12)
    D3D_FEATURE_LEVEL mFeatureLevel;
    uint32_t          mSuppressInvalidSubresourceStateAfterExit : 1;
#endif
#if defined(VULKAN)
    uint32_t mDynamicRenderingSupported : 1;
    uint32_t mXclipseTransferQueueWorkaround : 1;
    uint32_t mDeviceMemoryReportCrashWorkaround : 1;
    uint32_t mYCbCrExtension : 1;
    uint32_t mFillModeNonSolid : 1;
    uint32_t mKHRRayQueryExtension : 1;
    uint32_t mAMDGCNShaderExtension : 1;
    uint32_t mAMDDrawIndirectCountExtension : 1;
    uint32_t mAMDShaderInfoExtension : 1;
    uint32_t mDescriptorIndexingExtension : 1;
    uint32_t mDynamicRenderingExtension : 1;
    uint32_t mNonUniformResourceIndexSupported : 1;
    uint32_t mBufferDeviceAddressSupported : 1;
    uint32_t mDrawIndirectCountExtension : 1;
    uint32_t mDedicatedAllocationExtension : 1;
    uint32_t mDebugMarkerExtension : 1;
    uint32_t mMemoryReq2Extension : 1;
    uint32_t mFragmentShaderInterlockExtension : 1;
    uint32_t mBufferDeviceAddressExtension : 1;
    uint32_t mAccelerationStructureExtension : 1;
    uint32_t mRayTracingPipelineExtension : 1;
    uint32_t mRayQueryExtension : 1;
    uint32_t mShaderAtomicInt64Extension : 1;
    uint32_t mBufferDeviceAddressFeature : 1;
    uint32_t mShaderFloatControlsExtension : 1;
    uint32_t mSpirv14Extension : 1;
    uint32_t mDeferredHostOperationsExtension : 1;
    uint32_t mDeviceFaultExtension : 1;
    uint32_t mDeviceFaultSupported : 1;
    uint32_t mASTCDecodeModeExtension : 1;
    uint32_t mDeviceMemoryReportExtension : 1;
    uint32_t mAMDBufferMarkerExtension : 1;
    uint32_t mAMDDeviceCoherentMemoryExtension : 1;
    uint32_t mAMDDeviceCoherentMemorySupported : 1;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    uint32_t mExternalMemoryExtension : 1;
    uint32_t mExternalMemoryWin32Extension : 1;
#endif
#if defined(QUEST_VR)
    uint32_t mMultiviewExtension : 1;
#endif

#endif
    uint32_t mMaxBoundTextures;
    uint32_t mSamplerAnisotropySupported : 1;
    uint32_t mGraphicsQueueSupported : 1;
#if defined(METAL)
    uint32_t mHeaps : 1;
    uint32_t mPlacementHeaps : 1;
    uint32_t mTessellationIndirectDrawSupported : 1;
    uint32_t mDrawIndexVertexOffsetSupported : 1;
    uint32_t mCubeMapTextureArraySupported : 1;
#if !defined(TARGET_IOS)
    uint32_t mIsHeadLess : 1; // indicates whether a GPU device does not have a connection to a display.
#endif
#endif
    uint32_t mAmdAsicFamily;
    uint32_t mFrameBufferSamplesCount;
    uint32_t mGPUTarget;
} GpuDesc;

#define ALIGN_Renderer 64
typedef struct DEFINE_ALIGNED(Renderer, ALIGN_Renderer)
{
#if defined(DIRECT3D12)
    struct
    {
        // API specific descriptor heap and memory allocator
        struct DescriptorHeap**   pCPUDescriptorHeaps;
        struct DescriptorHeap**   pCbvSrvUavHeaps;
        struct DescriptorHeap**   pSamplerHeaps;
        struct D3D12MAAllocator_* pResourceAllocator;
        // Filled by user - See initGraphicsRootSignature, initComputeRootSignature
        ID3D12RootSignature*      pGraphicsRootSignature;
        ID3D12RootSignature*      pComputeRootSignature;
#if defined(XBOX)
        ID3D12Device* pDevice;
        EsramManager* pESRAMManager;
#elif defined(DIRECT3D12)
        ID3D12Device*                            pDevice;
#endif
#if defined(_WINDOWS) && defined(ENABLE_GRAPHICS_VALIDATION)
        ID3D12InfoQueue1* pDebugValidation;
        DWORD             mCallbackCookie;
        bool              mUseDebugCallback;
        bool              mSuppressMismatchingCommandListDuringPresent;
#endif
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        VkDevice               pDevice;
        uint32_t**             pAvailableQueueCount;
        uint32_t**             pUsedQueueCount;
        VkDescriptorPool       pEmptyDescriptorPool;
        VkDescriptorSetLayout  pEmptyDescriptorSetLayout;
        VkDescriptorSet        pEmptyDescriptorSet;
        DescriptorSet*         pEmptyStaticSamplerDescriptorSet;
        struct VmaAllocator_T* pVmaAllocator;
        union
        {
            uint8_t mGraphicsQueueFamilyIndex;
            uint8_t mTransferQueueFamilyIndex;
            uint8_t mComputeQueueFamilyIndex;
        };
        uint8_t mQueueFamilyIndices[3];
    } mVk;
#endif
#if defined(METAL)
    struct
    {
        id<MTLDevice>               pDevice;
        struct VmaAllocator_T*      pVmaAllocator;
        id<MTLComputePipelineState> pFillBufferPipeline;
        NOREFS id<MTLHeap>* pHeaps;
        uint32_t            mHeapCount;
        uint32_t            mHeapCapacity;
        // To synchronize resource allocation done through automatic heaps
        Mutex*              pHeapMutex;
        double              mGpuToCpuTimestampFactor;
        MTLTimestamp        mPrevCpuTimestamp;
        MTLTimestamp        mPrevGpuTimestamp;
    };
#endif

    struct NullDescriptors* pNullDescriptors;
    struct RendererContext* pContext;
    const struct GpuDesc*   pGpu;
    const char*             pName;
    RendererApi             mRendererApi;
    uint32_t                mLinkedNodeCount : 4;
    uint32_t                mUnlinkedRendererIndex : 4;
    uint32_t                mGpuMode : 3;
    uint32_t                mShaderTarget : 4;
    uint32_t                mOwnsContext : 1;

} Renderer;
// 3 cache lines
COMPILE_ASSERT(sizeof(Renderer) <= 24 * sizeof(uint64_t));

typedef struct RendererContextDesc
{
#if defined(DIRECT3D12)
    struct
    {
        D3D_FEATURE_LEVEL mFeatureLevel;
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        const char** ppInstanceLayers;
        const char** ppInstanceExtensions;
        const char** ppDeviceExtensions;
        uint32_t     mInstanceLayerCount;
        uint32_t     mInstanceExtensionCount;
        uint32_t     mDeviceExtensionCount;
        /// Flag to specify whether to request all queues from the gpu or just one of each type
        /// This will affect memory usage - Around 200 MB more used if all queues are requested
        bool         mRequestAllAvailableQueues;
    } mVk;
#endif

    bool mEnableGpuBasedValidation;
#if defined(SHADER_STATS_AVAILABLE)
    bool mEnableShaderStats;
#endif
} RendererContextDesc;

typedef struct RendererContext
{
#if defined(DIRECT3D12)
    struct
    {
#if defined(XBOX)
        IDXGIFactory2* pDXGIFactory;
#elif defined(DIRECT3D12)
        IDXGIFactory6*                           pDXGIFactory;
        ID3D12Debug*                             pDebug;
#if defined(_WINDOWS) && defined(DRED)
        ID3D12DeviceRemovedExtendedDataSettings* pDredSettings;
#endif
#endif
    } mDx;
#endif
#if defined(VULKAN)
    struct
    {
        VkInstance               pInstance;
        VkDebugUtilsMessengerEXT pDebugUtilsMessenger;
        VkDebugReportCallbackEXT pDebugReport;
        uint32_t                 mDebugUtilsExtension : 1;
        uint32_t                 mDebugReportExtension : 1;
        uint32_t                 mDeviceGroupCreationExtension : 1;
    } mVk;
#endif
#if defined(METAL)
    struct
    {
        uint32_t mExtendedEncoderDebugReport : 1;
    } mMtl;
#endif
    GpuDesc  mGpus[MAX_MULTIPLE_GPUS];
    uint32_t mGpuCount;
} RendererContext;

typedef struct QueueSubmitDesc
{
    Cmd**       ppCmds;
    Fence*      pSignalFence;
    Semaphore** ppWaitSemaphores;
    Semaphore** ppSignalSemaphores;
    uint32_t    mCmdCount;
    uint32_t    mWaitSemaphoreCount;
    uint32_t    mSignalSemaphoreCount;
    bool        mSubmitDone;
} QueueSubmitDesc;

typedef struct QueuePresentDesc
{
    SwapChain*  pSwapChain;
    Semaphore** ppWaitSemaphores;
    uint32_t    mWaitSemaphoreCount;
    uint8_t     mIndex;
    bool        mSubmitDone;
} QueuePresentDesc;

typedef struct BindRenderTargetDesc
{
    RenderTarget*   pRenderTarget;
    LoadActionType  mLoadAction;
    StoreActionType mStoreAction;
    ClearValue      mClearValue;
    LoadActionType  mLoadActionStencil;
    StoreActionType mStoreActionStencil;
    uint32_t        mArraySlice;
    uint32_t        mMipSlice : 10;
    uint32_t        mOverrideClearValue : 1;
    uint32_t        mUseArraySlice : 1;
    uint32_t        mUseMipSlice : 1;
} BindRenderTargetDesc;

typedef struct BindDepthTargetDesc
{
    RenderTarget*   pDepthStencil;
    LoadActionType  mLoadAction;
    LoadActionType  mLoadActionStencil;
    StoreActionType mStoreAction;
    StoreActionType mStoreActionStencil;
    ClearValue      mClearValue;
    uint32_t        mArraySlice;
    uint32_t        mMipSlice : 10;
    uint32_t        mOverrideClearValue : 1;
    uint32_t        mUseArraySlice : 1;
    uint32_t        mUseMipSlice : 1;
} BindDepthTargetDesc;

// Uses render targets' sample count in bindRenderTargetsDesc
typedef struct SampleLocationDesc
{
    SampleLocations* pLocations;
    uint32_t         mGridSizeX;
    uint32_t         mGridSizeY;
} SampleLocationDesc;

typedef struct BindRenderTargetsDesc
{
    uint32_t             mRenderTargetCount;
    BindRenderTargetDesc mRenderTargets[MAX_RENDER_TARGET_ATTACHMENTS];
    BindDepthTargetDesc  mDepthStencil;
    SampleLocationDesc   mSampleLocation;
    // Explicit viewport for empty render pass
    uint32_t             mExtent[2];
} BindRenderTargetsDesc;

// clang-format off

// API functions
#ifdef __cplusplus
extern "C" {
#endif

// Multiple renderer API (optional)
FORGE_RENDERER_API void FORGE_CALLCONV initRendererContext(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext);
FORGE_RENDERER_API void FORGE_CALLCONV exitRendererContext(RendererContext* pContext);

// allocates memory and initializes the renderer -> returns pRenderer
//
FORGE_RENDERER_API void FORGE_CALLCONV initRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
FORGE_RENDERER_API void FORGE_CALLCONV exitRenderer(Renderer* pRenderer);

void initFence(Renderer* pRenderer, Fence** ppFence);
void exitFence(Renderer* pRenderer, Fence* pFence);

void initSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore);
void exitSemaphore(Renderer* pRenderer, Semaphore* pSemaphore);

void initQueue(Renderer* pRenderer, QueueDesc* pQDesc, Queue** ppQueue);
void exitQueue(Renderer* pRenderer, Queue* pQueue);

void addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain);
void removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain);

// memory functions
void addResourceHeap(Renderer* pRenderer, const ResourceHeapDesc* pDesc, ResourceHeap** ppHeap);
void removeResourceHeap(Renderer* pRenderer, ResourceHeap* pHeap);

// command pool functions
void initCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool);
void exitCmdPool(Renderer* pRenderer, CmdPool* pCmdPool);
void initCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd);
void exitCmd(Renderer* pRenderer, Cmd* pCmd);
void initCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmds);
void exitCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds);

//
// All buffer, texture loading handled by resource system -> IResourceLoader.*
//

void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget);
void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget);
void addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler);
void removeSampler(Renderer* pRenderer, Sampler* pSampler);

// shader functions
void addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram);
void removeShader(Renderer* pRenderer, Shader* pShaderProgram);

// pipeline functions
void addPipeline(Renderer* pRenderer, const PipelineDesc* pPipelineSettings, Pipeline** ppPipeline);
void removePipeline(Renderer* pRenderer, Pipeline* pPipeline);
void addPipelineCache(Renderer* pRenderer, const PipelineCacheDesc* pDesc, PipelineCache** ppPipelineCache);
void getPipelineCacheData(Renderer* pRenderer, PipelineCache* pPipelineCache, size_t* pSize, void* pData);
#if defined(SHADER_STATS_AVAILABLE)
void addPipelineStats(Renderer* pRenderer, Pipeline* pPipeline, bool generateDisassembly, PipelineStats* pOutStats);
void removePipelineStats(Renderer* pRenderer, PipelineStats* pStats);
#endif
void removePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache);

// Descriptor Set functions
void addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet);
void removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet);
void updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams);

// command buffer functions
void resetCmdPool(Renderer* pRenderer, CmdPool* pCmdPool);
void beginCmd(Cmd* pCmd);
void endCmd(Cmd* pCmd);
void cmdBindRenderTargets(Cmd* pCmd, const BindRenderTargetsDesc* pDesc);
void cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth);
void cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void cmdSetStencilReferenceValue(Cmd* pCmd, uint32_t val);
void cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline);
void cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet);
void cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset);
void cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets);
void cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex);
void cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance);
void cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex);
void cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
void cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

// Transition Commands
void cmdResourceBarrier(Cmd* pCmd, uint32_t bufferBarrierCount, BufferBarrier* pBufferBarriers, uint32_t textureBarrierCount, TextureBarrier* pTextureBarriers, uint32_t rtBarrierCount, RenderTargetBarrier* pRtBarriers);

// queue/fence/swapchain functions
void acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex);
void queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc);
void queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc);
void waitQueueIdle(Queue* pQueue);
void getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus);
void waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences);
void toggleVSync(Renderer* pRenderer, SwapChain** ppSwapchain);

//Returns the recommended format for the swapchain.
//If true is passed for the hintHDR parameter, it will return an HDR format IF the platform supports it
//If false is passed or the platform does not support HDR a non HDR format is returned.
//If true is passed for the hintSrgb parameter, it will return format that is will do gamma correction automatically
//If false is passed for the hintSrgb parameter the gamma correction should be done as a postprocess step before submitting image to swapchain
TinyImageFormat getSupportedSwapchainFormat(Renderer* pRenderer, const SwapChainDesc* pDesc, ColorSpace colorSpace);
uint32_t getRecommendedSwapchainImageCount(Renderer* pRenderer, const WindowHandle* hwnd);

//indirect Draw functions
void cmdExecuteIndirect(Cmd* pCmd, IndirectArgumentType type, unsigned int maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset);

// Workgraph functions
#if defined(ENABLE_WORKGRAPH)
void addWorkgraph(Renderer* pRenderer, const WorkgraphDesc* pDesc, Workgraph** ppWorkgraph);
void removeWorkgraph(Renderer* pRenderer, Workgraph* pWorkgraph);
void cmdDispatchWorkgraph(Cmd* pCmd, const DispatchGraphDesc* pDesc);
#endif
/************************************************************************/
// GPU Query Interface
/************************************************************************/
void getTimestampFrequency(Queue* pQueue, double* pFrequency);
void initQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool);
void exitQueryPool(Renderer* pRenderer, QueryPool* pQueryPool);
void cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery);
void cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery);
void cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount);
void cmdResetQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount);
void getQueryData(Renderer* pRenderer, QueryPool* pQueryPool, uint32_t queryIndex, QueryData* pOutData);
/************************************************************************/
// Stats Info Interface
/************************************************************************/
void logMemoryStats(Renderer* pRenderer);
void calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes);
/************************************************************************/
// Debug Marker Interface
/************************************************************************/
void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName);
void cmdEndDebugMarker(Cmd* pCmd);
void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName);
void cmdWriteMarker(Cmd* pCmd, const MarkerDesc* pDesc);
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName);
void setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName);
void setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName);
void setPipelineName(Renderer* pRenderer, Pipeline* pPipeline, const char* pName);
/************************************************************************/
/************************************************************************/
// clang-format on
#ifdef __cplusplus
}
#endif
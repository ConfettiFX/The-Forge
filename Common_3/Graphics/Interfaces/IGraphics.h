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

#include "../GraphicsConfig.h"

#ifdef ENABLE_NSIGHT_AFTERMATH
#include "../ThirdParty/PrivateNvidia/NsightAftermath/include/AftermathTracker.h"
#endif

#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"

#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"

#ifdef __cplusplus
#ifndef MAKE_ENUM_FLAG
#define MAKE_ENUM_FLAG(TYPE, ENUM_TYPE)                                                                                      \
    inline FORGE_CONSTEXPR ENUM_TYPE operator|(ENUM_TYPE a, ENUM_TYPE b) { return ENUM_TYPE(((TYPE)a) | ((TYPE)b)); }        \
    inline ENUM_TYPE&                operator|=(ENUM_TYPE& a, ENUM_TYPE b) { return (ENUM_TYPE&)(((TYPE&)a) |= ((TYPE)b)); } \
    inline FORGE_CONSTEXPR ENUM_TYPE operator&(ENUM_TYPE a, ENUM_TYPE b) { return ENUM_TYPE(((TYPE)a) & ((TYPE)b)); }        \
    inline ENUM_TYPE&                operator&=(ENUM_TYPE& a, ENUM_TYPE b) { return (ENUM_TYPE&)(((TYPE&)a) &= ((TYPE)b)); } \
    inline FORGE_CONSTEXPR ENUM_TYPE operator~(ENUM_TYPE a) { return ENUM_TYPE(~((TYPE)a)); }                                \
    inline FORGE_CONSTEXPR ENUM_TYPE operator^(ENUM_TYPE a, ENUM_TYPE b) { return ENUM_TYPE(((TYPE)a) ^ ((TYPE)b)); }        \
    inline ENUM_TYPE&                operator^=(ENUM_TYPE& a, ENUM_TYPE b) { return (ENUM_TYPE&)(((TYPE&)a) ^= ((TYPE)b)); }
#endif
#else
#define MAKE_ENUM_FLAG(TYPE, ENUM_TYPE)
#endif

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
    MAX_SWAPCHAIN_IMAGES = 3,
    MAX_GPU_VENDOR_STRING_LENGTH = 256, // max size for GPUVendorPreset strings
    MAX_SAMPLE_LOCATIONS = 16,
#if defined(VULKAN)
    MAX_PLANE_COUNT = 3,
    MAX_DESCRIPTOR_POOL_SIZE_ARRAY_COUNT = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT + 1,
#endif
};
#endif

#ifdef DIRECT3D12
#ifndef D3D12MAAllocator
// Forward declare opaque memory allocator structs
typedef struct D3D12MAAllocator  D3D12MAAllocator;
typedef struct D3D12MAAllocation D3D12MAAllocation;
#endif
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
#if defined(GLES)
    RENDERER_API_GLES,
#endif
#if defined(DIRECT3D12)
    RENDERER_API_D3D12,
#endif
#if defined(VULKAN)
    RENDERER_API_VULKAN,
#endif
#if defined(DIRECT3D11)
    RENDERER_API_D3D11,
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
#if defined(QUEST_VR)
    RESOURCE_STATE_SHADING_RATE_SOURCE = 0x10000,
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

typedef struct PlatformParameters
{
    // RendererAPI
    RendererApi mSelectedRendererApi;
    // Available GPU capabilities
    char        ppAvailableGpuNames[MAX_MULTIPLE_GPUS][MAX_GPU_VENDOR_STRING_LENGTH];
    uint32_t    pAvailableGpuIds[MAX_MULTIPLE_GPUS];
    uint32_t    mAvailableGpuCount;
    uint32_t    mSelectedGpuIndex;
    // Could add swap chain size, render target format, ...
    uint32_t    mPreferedGpuId;
} PlatformParameters;

// Forward declarations
typedef struct RendererContext    RendererContext;
typedef struct Renderer           Renderer;
typedef struct Queue              Queue;
typedef struct Pipeline           Pipeline;
typedef struct Buffer             Buffer;
typedef struct Texture            Texture;
typedef struct RenderTarget       RenderTarget;
typedef struct Shader             Shader;
typedef struct RootSignature      RootSignature;
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
    INDIRECT_ARG_INVALID,
    INDIRECT_DRAW,
    INDIRECT_DRAW_INDEX,
    INDIRECT_DISPATCH,
    INDIRECT_VERTEX_BUFFER,
    INDIRECT_INDEX_BUFFER,
    INDIRECT_CONSTANT,
    INDIRECT_CONSTANT_BUFFER_VIEW,   // only for dx
    INDIRECT_SHADER_RESOURCE_VIEW,   // only for dx
    INDIRECT_UNORDERED_ACCESS_VIEW,  // only for dx
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
} SampleCount;

#ifdef METAL
typedef enum ShaderStage
{
    SHADER_STAGE_NONE = 0,
    SHADER_STAGE_VERT = 0X00000001,
    SHADER_STAGE_FRAG = 0X00000002,
    SHADER_STAGE_COMP = 0X00000004,
    SHADER_STAGE_ALL_GRAPHICS = ((uint32_t)SHADER_STAGE_VERT | (uint32_t)SHADER_STAGE_FRAG),
    SHADER_STAGE_COUNT = 3,
} ShaderStage;

typedef enum ShaderStageIndex
{
    SHADER_STAGE_INDEX_VERT = 0,
    SHADER_STAGE_INDEX_FRAG,
    SHADER_STAGE_INDEX_COMP,
} ShaderStageIndex;
#else
typedef enum ShaderStage
{
    SHADER_STAGE_NONE = 0,
    SHADER_STAGE_VERT = 0X00000001,
    SHADER_STAGE_TESC = 0X00000002,
    SHADER_STAGE_TESE = 0X00000004,
    SHADER_STAGE_GEOM = 0X00000008,
    SHADER_STAGE_FRAG = 0X00000010,
    SHADER_STAGE_COMP = 0X00000020,
    SHADER_STAGE_ALL_GRAPHICS = ((uint32_t)SHADER_STAGE_VERT | (uint32_t)SHADER_STAGE_TESC | (uint32_t)SHADER_STAGE_TESE |
                                 (uint32_t)SHADER_STAGE_GEOM | (uint32_t)SHADER_STAGE_FRAG),
    SHADER_STAGE_HULL = SHADER_STAGE_TESC,
    SHADER_STAGE_DOMN = SHADER_STAGE_TESE,
    SHADER_STAGE_COUNT = 6,
} ShaderStage;

typedef enum ShaderStageIndex
{
    SHADER_STAGE_INDEX_VERT = 0,
    SHADER_STAGE_INDEX_TESC,
    SHADER_STAGE_INDEX_TESE,
    SHADER_STAGE_INDEX_GEOM,
    SHADER_STAGE_INDEX_FRAG,
    SHADER_STAGE_INDEX_COMP,
    SHADER_STAGE_INDEX_HULL = SHADER_STAGE_INDEX_TESC,
    SHADER_STAGE_INDEX_DOMN = SHADER_STAGE_INDEX_TESE,
} ShaderStageIndex;
#endif
MAKE_ENUM_FLAG(uint32_t, ShaderStage)

// This include is placed here because it uses data types defined previously in this file
// and forward enums are not allowed for some compilers (Xcode).
#include "IShaderReflection.h"

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
#ifdef VULKAN
    /* Memory Host Flags */
    BUFFER_CREATION_FLAG_HOST_VISIBLE = 0x80,
    BUFFER_CREATION_FLAG_HOST_COHERENT = 0x100,
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

typedef enum GPUPresetLevel
{
    GPU_PRESET_NONE = 0,
    GPU_PRESET_OFFICE,  // This means unsupported
    GPU_PRESET_VERYLOW, // Mostly for mobile GPU
    GPU_PRESET_LOW,
    GPU_PRESET_MEDIUM,
    GPU_PRESET_HIGH,
    GPU_PRESET_ULTRA,
    GPU_PRESET_COUNT
} GPUPresetLevel;

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
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
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
            double mGpuTimestampStart;
            double mGpuTimestampEnd;
        };
#endif
#if defined(DIRECT3D11)
        struct
        {
            ID3D11Query** ppQueries;
            D3D11_QUERY   mType;
        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            uint32_t* pQueries;
            uint32_t  mType;
        } mGLES;
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
#if defined(USE_MULTIPLE_RENDER_APIS)
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

typedef struct DEFINE_ALIGNED(ResourceHeap, 64)
{
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
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
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
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

typedef struct DEFINE_ALIGNED(Buffer, 64)
{
    /// CPU address of the mapped buffer (applicable to buffers created in CPU accessible heaps (CPU, CPU_TO_GPU, GPU_TO_CPU)
    void* pCpuMappedAddress;
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
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
            /// Native handle of the underlying resource
            ID3D12Resource*           pResource;
            /// Contains resource allocation info such as parent heap, offset in heap
            D3D12MAAllocation*        pAllocation;
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
#if defined(DIRECT3D11)
        struct
        {
            ID3D11Buffer*              pResource;
            ID3D11ShaderResourceView*  pSrvHandle;
            ID3D11UnorderedAccessView* pUavHandle;
            uint64_t                   mFlags;
            uint64_t                   mPadA;
        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            GLuint mBuffer;
            GLenum mTarget;
            void*  pGLCpuMappedAddress;
        } mGLES;
#endif
#if defined(ORBIS)
        OrbisBuffer mStruct;
#endif
#if defined(PROSPERO)
        ProsperoBuffer mStruct;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
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

typedef struct DEFINE_ALIGNED(Texture, 64)
{
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
#if defined(DIRECT3D12)
        struct
        {
            /// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to TEXTURE_USAGE_SAMPLED_IMAGE)
            DxDescriptorID     mDescriptors;
            /// Native handle of the underlying resource
            ID3D12Resource*    pResource;
            /// Contains resource allocation info such as parent heap, offset in heap
            D3D12MAAllocation* pAllocation;
            uint32_t           mHandleCount : 24;
            uint32_t           mUavStartIndex;
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
            id       mpsTextureAllocator;
            uint32_t pPixelFormat;
            uint32_t mRT : 1;
        };
#endif
#if defined(DIRECT3D11)
        struct
        {
            ID3D11Resource*             pResource;
            ID3D11ShaderResourceView*   pSRVDescriptor;
            ID3D11UnorderedAccessView** pUAVDescriptors;
            uint64_t                    mPadA;
            uint64_t                    mPadB;
        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            GLuint mTexture;
            GLenum mTarget;
            GLenum mGlFormat;
            GLenum mInternalFormat;
            GLenum mType;
            bool   mStateModified;
        } mGLES;
#endif
#if defined(ORBIS)
        OrbisTexture mStruct;
        /// Contains resource allocation info such as parent heap, offset in heap
#endif
#if defined(PROSPERO)
        ProsperoTexture mStruct;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
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

typedef struct DEFINE_ALIGNED(RenderTarget, 64)
{
    Texture* pTexture;
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
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
#if defined(DIRECT3D11)
        struct
        {
            union
            {
                /// Resources
                ID3D11RenderTargetView* pRtvDescriptor;
                ID3D11DepthStencilView* pDsvDescriptor;
            };
            union
            {
                /// Resources
                ID3D11RenderTargetView** pRtvSliceDescriptors;
                ID3D11DepthStencilView** pDsvSliceDescriptors;
            };
        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            GLuint mType;
            GLuint mFramebuffer;
            GLuint mDepthTarget;
            GLuint mStencilTarget;
        } mGLES;
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
#if defined(USE_MULTIPLE_RENDER_APIS)
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

typedef struct DEFINE_ALIGNED(Sampler, 16)
{
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
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
#if defined(DIRECT3D11)
        struct
        {
            /// Native handle of the underlying resource
            ID3D11SamplerState* pSamplerState;
        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            GLenum mMinFilter;
            GLenum mMagFilter;
            GLenum mMipMapMode;
            GLenum mAddressS;
            GLenum mAddressT;
            GLenum mCompareFunc;
        } mGLES;
#endif
#if defined(ORBIS)
        OrbisSampler mStruct;
#endif
#if defined(PROSPERO)
        ProsperoSampler mStruct;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
#endif
} Sampler;
#if defined(DIRECT3D12)
COMPILE_ASSERT(sizeof(Sampler) == 8 * sizeof(uint64_t));
#elif defined(VULKAN)
COMPILE_ASSERT(sizeof(Sampler) <= 8 * sizeof(uint64_t));
#elif defined(GLES)
COMPILE_ASSERT(sizeof(Sampler) == 4 * sizeof(uint64_t));
#else
COMPILE_ASSERT(sizeof(Sampler) == 2 * sizeof(uint64_t));
#endif

typedef enum DescriptorUpdateFrequency
{
    DESCRIPTOR_UPDATE_FREQ_NONE = 0,
    DESCRIPTOR_UPDATE_FREQ_PER_FRAME,
    DESCRIPTOR_UPDATE_FREQ_PER_BATCH,
    DESCRIPTOR_UPDATE_FREQ_PER_DRAW,
    DESCRIPTOR_UPDATE_FREQ_COUNT,
} DescriptorUpdateFrequency;

/// Data structure holding the layout for a descriptor
typedef struct DEFINE_ALIGNED(DescriptorInfo, 16)
{
    const char* pName;
#if defined(ORBIS)
    OrbisDescriptorInfo mStruct;
#elif defined(PROSPERO)
    ProsperoDescriptorInfo mStruct;
#else
    uint32_t mType;
    uint32_t mDim : 4;
    uint32_t mRootDescriptor : 1;
    uint32_t mStaticSampler : 1;
    uint32_t mUpdateFrequency : 3;
    uint32_t mSize;
    uint32_t mHandleIndex;
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
#if defined(DIRECT3D12)
        struct
        {
            uint64_t mPadA;
        } mDx;
#endif
#if defined(VULKAN)
        struct
        {
            uint32_t mType;
            uint32_t mReg : 20;
            uint32_t mStages : 8;
        } mVk;
#endif
#if defined(METAL)
        struct
        {
            id<MTLSamplerState> pStaticSampler;
            uint32_t            mUsedStages : 6;
            uint32_t            mReg : 10;
            uint32_t            mIsArgumentBufferField : 1;
            MTLResourceUsage    mUsage;
            uint64_t            mPadB[2];
        };
#endif
#if defined(DIRECT3D11)
        struct
        {
            uint32_t mUsedStages : 6;
            uint32_t mReg : 20;
            uint32_t mPadA;
        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            union
            {
                uint32_t mGlType;
                uint32_t mUBOSize;
                uint32_t mVariableStart;
            };
        } mGLES;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
#endif
#endif
} DescriptorInfo;
#if defined(METAL)
COMPILE_ASSERT(sizeof(DescriptorInfo) == 8 * sizeof(uint64_t));
#elif defined(ORBIS) || defined(PROSPERO)
COMPILE_ASSERT(sizeof(DescriptorInfo) == 2 * sizeof(uint64_t));
#else
COMPILE_ASSERT(sizeof(DescriptorInfo) == 4 * sizeof(uint64_t));
#endif

typedef enum RootSignatureFlags
{
    /// Default flag
    ROOT_SIGNATURE_FLAG_NONE = 0,
} RootSignatureFlags;
MAKE_ENUM_FLAG(uint32_t, RootSignatureFlags)

typedef struct RootSignatureDesc
{
    Shader**           ppShaders;
    uint32_t           mShaderCount;
    uint32_t           mMaxBindlessTextures;
    const char**       ppStaticSamplerNames;
    Sampler**          ppStaticSamplers;
    uint32_t           mStaticSamplerCount;
    RootSignatureFlags mFlags;
} RootSignatureDesc;

typedef struct DEFINE_ALIGNED(RootSignature, 64)
{
    /// Number of descriptors declared in the root signature layout
    uint32_t            mDescriptorCount;
    /// Graphics or Compute
    PipelineType        mPipelineType;
    /// Array of all descriptors declared in the root signature layout
    DescriptorInfo*     pDescriptors;
    /// Translates hash of descriptor name to descriptor index in pDescriptors array
    DescriptorIndexMap* pDescriptorNameToIndexMap;
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
#if defined(DIRECT3D12)
        struct
        {
            ID3D12RootSignature* pRootSignature;
            uint8_t              mViewDescriptorTableRootIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint8_t              mSamplerDescriptorTableRootIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint32_t             mCumulativeViewDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint32_t             mCumulativeSamplerDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint16_t             mViewDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint16_t             mSamplerDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
#if defined(_WINDOWS) && defined(D3D12_RAYTRACING_AVAILABLE) && defined(FORGE_DEBUG)
            bool mHasRayQueryAccelerationStructure;
#endif
        } mDx;
#endif
#if defined(VULKAN)
        struct
        {
            VkPipelineLayout      pPipelineLayout;
            VkDescriptorSetLayout mDescriptorSetLayouts[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint8_t               mDynamicDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
            VkDescriptorPoolSize  mPoolSizes[DESCRIPTOR_UPDATE_FREQ_COUNT][MAX_DESCRIPTOR_POOL_SIZE_ARRAY_COUNT];
            uint8_t               mPoolSizeCount[DESCRIPTOR_UPDATE_FREQ_COUNT];
            VkDescriptorPool      pEmptyDescriptorPool[DESCRIPTOR_UPDATE_FREQ_COUNT];
            VkDescriptorSet       pEmptyDescriptorSet[DESCRIPTOR_UPDATE_FREQ_COUNT];
        } mVk;
#endif
#if defined(METAL)
        struct
        {
            NSMutableArray<MTLArgumentDescriptor*>* mArgumentDescriptors[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint8_t                                 mRootTextureCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint8_t                                 mRootBufferCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint8_t                                 mRootSamplerCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
        };
#endif
#if defined(DIRECT3D11)
        struct
        {
            ID3D11SamplerState** ppStaticSamplers;
            uint32_t*            pStaticSamplerSlots;
            ShaderStage*         pStaticSamplerStages;
            uint32_t             mStaticSamplerCount;
            uint8_t              mSrvCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint8_t              mUavCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint8_t              mCbvCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint8_t              mSamplerCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint8_t              mDynamicCbvCount[DESCRIPTOR_UPDATE_FREQ_COUNT];

        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            uint32_t           mProgramCount : 6;
            uint32_t           mVariableCount : 10;
            uint32_t*          pProgramTargets;
            int32_t*           pDescriptorGlLocations;
            struct GlVariable* pVariables;
            Sampler*           pSampler;
        } mGLES;
#endif
#if defined(ORBIS)
        OrbisRootSignature mStruct;
#endif
#if defined(PROSPERO)
        ProsperoRootSignature mStruct;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
#endif

} RootSignature;
#if defined(VULKAN)
COMPILE_ASSERT(sizeof(RootSignature) <= 72 * sizeof(uint64_t));
#elif defined(ORBIS) || defined(PROSPERO) || defined(DIRECT3D12) || defined(ENABLE_DEPENDENCY_TRACKER)
// 2 cache lines
COMPILE_ASSERT(sizeof(RootSignature) <= 16 * sizeof(uint64_t));
#else
// 1 cache line
COMPILE_ASSERT(sizeof(RootSignature) == 16 * sizeof(uint64_t));
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
    /// User can either set name of descriptor or index (index in pRootSignature->pDescriptors array)
    /// Name of descriptor
    const char* pName;
    /// Number of array entries to update (array size of ppTextures/ppBuffers/...)
    uint32_t    mCount : 31;
    /// Dst offset into the array descriptor (useful for updating few entries in a large array)
    // Example: to update 6th entry in a bindless texture descriptor, mArrayOffset will be 6 and mCount will be 1)
    uint32_t    mArrayOffset : 20;
    // Index in pRootSignature->pDescriptors array - Cache index using getDescriptorIndexFromName to avoid using string checks at runtime
    uint32_t    mIndex : 10;
    uint32_t    mBindByIndex : 1;

    // Range to bind (buffer offset, size)
    DescriptorDataRange* pRanges;

    // Binds stencil only descriptor instead of color/depth
    bool mBindStencilResource : 1;

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

typedef struct DEFINE_ALIGNED(DescriptorSet, 64)
{
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
#if defined(DIRECT3D12)
        struct
        {
            /// Start handle to cbv srv uav descriptor table
            DxDescriptorID       mCbvSrvUavHandle;
            /// Start handle to sampler descriptor table
            DxDescriptorID       mSamplerHandle;
            /// Stride of the cbv srv uav descriptor table (number of descriptors * descriptor size)
            uint32_t             mCbvSrvUavStride;
            /// Stride of the sampler descriptor table (number of descriptors * descriptor size)
            uint32_t             mSamplerStride;
            const RootSignature* pRootSignature;
            uint32_t             mMaxSets : 16;
            uint32_t             mUpdateFrequency : 3;
            uint32_t             mNodeIndex : 4;
            uint32_t             mCbvSrvUavRootIndex : 4;
            uint32_t             mSamplerRootIndex : 4;
            uint32_t             mPipelineType : 3;
        } mDx;
#endif
#if defined(VULKAN)
        struct
        {
            VkDescriptorSet*           pHandles;
            const RootSignature*       pRootSignature;
            struct DynamicUniformData* pDynamicUniformData;
            VkDescriptorPool           pDescriptorPool;
            uint32_t                   mMaxSets;
            uint8_t                    mDynamicOffsetCount;
            uint8_t                    mUpdateFrequency;
            uint8_t                    mNodeIndex;
        } mVk;
#endif
#if defined(METAL)
        struct
        {
            id<MTLArgumentEncoder>         mArgumentEncoder;
            Buffer*                        mArgumentBuffer;
            struct UntrackedResourceData** ppUntrackedData;
            const RootSignature*           pRootSignature;
            /// Descriptors that are bound without argument buffers
            /// This is necessary since there are argument buffers bugs in some iOS Metal drivers which causes shader compiler crashes or
            /// incorrect shader generation. This makes it necessary to keep fallback descriptor binding path alive
            struct RootDescriptorData*     pRootDescriptorData;
            uint32_t                       mStride;
            uint32_t                       mMaxSets;
            uint32_t                       mRootBufferCount : 10;
            uint32_t                       mRootTextureCount : 10;
            uint32_t                       mRootSamplerCount : 10;
            uint8_t                        mUpdateFrequency;
            uint8_t                        mNodeIndex;
            uint8_t                        mStages;
        };
#endif
#if defined(DIRECT3D11)
        struct
        {
            struct DescriptorDataArray* pHandles;
            const RootSignature*        pRootSignature;
            uint16_t                    mMaxSets;
            uint32_t                    mUpdateFrequency : 3;
        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            struct DescriptorDataArray* pHandles;
            uint8_t                     mUpdateFrequency;
            const RootSignature*        pRootSignature;
            uint16_t                    mMaxSets;
        } mGLES;
#endif
#if defined(ORBIS)
        OrbisDescriptorSet mStruct;
#endif
#if defined(PROSPERO)
        ProsperoDescriptorSet mStruct;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
#endif
} DescriptorSet;

typedef struct CmdPoolDesc
{
    Queue* pQueue;
    bool   mTransient;
} CmdPoolDesc;

typedef struct CmdPool
{
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
#if defined(DIRECT3D12)
        ID3D12CommandAllocator* pCmdAlloc;
#endif
#if defined(VULKAN)
        VkCommandPool pCmdPool;
#endif
#if defined(GLES)
        struct CmdCache* pCmdCache;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
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
#ifdef ENABLE_GRAPHICS_DEBUG
    const char* pName;
#endif // ENABLE_GRAPHICS_DEBUG
} CmdDesc;

typedef enum MarkerType
{
    MARKER_TYPE_DEFAULT = 0x0,
    MARKER_TYPE_IN = 0x1,
    MARKER_TYPE_OUT = 0x2,
    MARKER_TYPE_IN_OUT = 0x3,
} MarkerType;

typedef struct DEFINE_ALIGNED(Cmd, 64)
{
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
#if defined(DIRECT3D12)
        struct
        {
#if defined(XBOX)
            DmaCmd mDma;
#endif
            ID3D12GraphicsCommandList1* pCmdList;
#if defined(ENABLE_GRAPHICS_DEBUG) && defined(_WINDOWS)
            // For resource state validation
            ID3D12DebugCommandList* pDebugCmdList;
#endif
            // Cached in beginCmd to avoid fetching them during rendering
            struct DescriptorHeap*      pBoundHeaps[2];
            D3D12_GPU_DESCRIPTOR_HANDLE mBoundHeapStartHandles[2];

            // Command buffer state
            const RootSignature* pBoundRootSignature;
            DescriptorSet*       pBoundDescriptorSets[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint16_t             mBoundDescriptorSetIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint32_t             mNodeIndex : 4;
            uint32_t             mType : 3;
#if defined(XBOX)
            // Required for setting occlusion query control
            uint32_t mSampleCount : 5;
#endif
            CmdPool* pCmdPool;
        } mDx;
#endif
#if defined(VULKAN)
        struct
        {
            VkCommandBuffer  pCmdBuf;
            VkRenderPass     pActiveRenderPass;
            VkPipelineLayout pBoundPipelineLayout;
            CmdPool*         pCmdPool;
            uint32_t         mNodeIndex : 4;
            uint32_t         mType : 3;
            uint32_t         mIsRendering : 1;
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
            // To store the begin-end timestamp for this command buffer
            QueryPool* pLastFrameQuery;
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
            // To store sample locations provided by cmdSetSampleLocations and used in cmdBindRenderTargets
            uint32_t             mSampleLocationsCount : 5;
            uint32_t             mShouldRebindDescriptorSets: DESCRIPTOR_UPDATE_FREQ_COUNT;
            uint32_t             mShouldRebindPipeline : 1;
            MTLSamplePosition    mSamplePositions[MAX_SAMPLE_LOCATIONS];
            const RootSignature* pUsedRootSignature;
            DescriptorSet*       mBoundDescriptorSets[DESCRIPTOR_UPDATE_FREQ_COUNT];
            uint32_t             mBoundDescriptorSetIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
#ifdef ENABLE_DRAW_INDEX_BASE_VERTEX_FALLBACK
            // When first vertex is not supported for indexed draw, we have to offset the
            // vertex buffer manually using setVertexBufferOffset
            // mOffsets, mStrides stored in cmdBindVertexBuffer and used in cmdDrawIndexed functions
            uint32_t mOffsets[MAX_VERTEX_BINDINGS];
            uint32_t mStrides[MAX_VERTEX_BINDINGS];
            uint32_t mFirstVertex;
#endif
#ifdef ENABLE_GRAPHICS_DEBUG
            char mDebugMarker[MAX_DEBUG_NAME_LENGTH];
#endif
        };
#endif
#if defined(DIRECT3D11)
        struct
        {
            ID3D11Buffer* pRootConstantBuffer;
        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            CmdPool* pCmdPool;
        } mGLES;
#endif
#if defined(ORBIS)
        OrbisCmd mStruct;
#endif
#if defined(PROSPERO)
        ProsperoCmd mStruct;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
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
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
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
#if defined(DIRECT3D11)
        struct
        {
            ID3D11Query* pDX11Query;
            uint32_t     mSubmitted : 1;
        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            uint32_t mSubmitted : 1;
        } mGLES;
#endif
#if defined(ORBIS)
        OrbisFence mStruct;
#endif
#if defined(PROSPERO)
        ProsperoFence mStruct;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
#endif
} Fence;

typedef struct Semaphore
{
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
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
#if defined(GLES)
        struct
        {
            uint32_t mSignaled : 1;
        } mGLES;
#endif
#if defined(ORBIS)
        OrbisSemaphore mStruct;
#endif
#if defined(PROSPERO)
        ProsperoSemaphore mStruct;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
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
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
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
#if defined(GLES)
        struct
        {
            struct CmdCache* pCmdCache;
        } mGLES;
#endif
#if defined(METAL)
        struct
        {
            id<MTLCommandQueue> pCommandQueue;
            id<MTLFence>        pQueueFence;
            uint32_t            mBarrierFlags;
        };
#endif
#if defined(DIRECT3D11)
        struct
        {
            ID3D11Device*        pDevice;
            ID3D11DeviceContext* pContext;
            Fence*               pFence;
        } mDx11;
#endif
#if defined(ORBIS)
        OrbisQueue mStruct;
#endif
#if defined(PROSPERO)
        ProsperoQueue mStruct;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
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
    void*       pByteCode;
    uint32_t    mByteCodeSize;
    const char* pEntryPoint;
#if defined(METAL)
    uint32_t    mNumThreadsPerGroup[3];
    uint32_t    mOutputRenderTargetTypesMask;
#endif
#if defined(GLES)
    GLuint      mShader;
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
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
#if defined(DIRECT3D12)
        struct
        {
            IDxcBlobEncoding** pShaderBlobs;
            LPCWSTR*           pEntryNames;
        } mDx;
#endif
#if defined(VULKAN)
        struct
        {
            VkShaderModule*       pShaderModules;
            char**                pEntryNames;
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
#if defined(DIRECT3D11)
        struct
        {
            union
            {
                struct
                {
                    ID3D11VertexShader*   pVertexShader;
                    ID3D11PixelShader*    pPixelShader;
                    ID3D11GeometryShader* pGeometryShader;
                    ID3D11DomainShader*   pDomainShader;
                    ID3D11HullShader*     pHullShader;
                };
                ID3D11ComputeShader* pComputeShader;
            };
            ID3DBlob* pInputSignature;
        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            GLuint mProgram;
        } mGLES;
#endif
#if defined(ORBIS)
        OrbisShader mStruct;
#endif
#if defined(PROSPERO)
        ProsperoShader mStruct;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
#endif
    PipelineReflection* pReflection;

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

typedef struct GraphicsPipelineDesc
{
    Shader*              pShaderProgram;
    RootSignature*       pRootSignature;
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
    Shader*        pShaderProgram;
    RootSignature* pRootSignature;
} ComputePipelineDesc;

typedef struct PipelineDesc
{
    union
    {
        ComputePipelineDesc  mComputeDesc;
        GraphicsPipelineDesc mGraphicsDesc;
    };
    PipelineCache* pCache;
    void*          pPipelineExtensions;
    const char*    pName;
    PipelineType   mType;
    uint32_t       mExtensionCount;
} PipelineDesc;

typedef struct DEFINE_ALIGNED(Pipeline, 64)
{
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
#if defined(DIRECT3D12)
        struct
        {
            ID3D12PipelineState*   pPipelineState;
            const RootSignature*   pRootSignature;
            PipelineType           mType;
            D3D_PRIMITIVE_TOPOLOGY mPrimitiveTopology;
        } mDx;
#endif
#if defined(VULKAN)
        struct
        {
            VkPipeline   pPipeline;
            PipelineType mType;
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
#if defined(DIRECT3D11)
        struct
        {
            ID3D11VertexShader*      pVertexShader;
            ID3D11PixelShader*       pPixelShader;
            ID3D11GeometryShader*    pGeometryShader;
            ID3D11DomainShader*      pDomainShader;
            ID3D11HullShader*        pHullShader;
            ID3D11ComputeShader*     pComputeShader;
            ID3D11InputLayout*       pInputLayout;
            ID3D11BlendState*        pBlendState;
            ID3D11DepthStencilState* pDepthState;
            ID3D11RasterizerState*   pRasterizerState;
            PipelineType             mType;
            D3D_PRIMITIVE_TOPOLOGY   mPrimitiveTopology;
        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            uint16_t                    mVertexLayoutSize;
            uint16_t                    mRootSignatureIndex;
            uint16_t                    mVAOStateCount;
            uint16_t                    mVAOStateLoop;
            struct GLVAOState*          pVAOState;
            struct GlVertexAttrib*      pVertexLayout;
            struct GLRasterizerState*   pRasterizerState;
            struct GLDepthStencilState* pDepthStencilState;
            struct GLBlendState*        pBlendState;
            RootSignature*              pRootSignature;
            uint32_t                    mType;
            GLenum                      mGlPrimitiveTopology;
        } mGLES;
#endif
#if defined(ORBIS)
        OrbisPipeline mStruct;
#endif
#if defined(PROSPERO)
        ProsperoPipeline mStruct;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
#endif

} Pipeline;
#if defined(DIRECT3D11) || defined(ORBIS)
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
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
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
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
#endif
} PipelineCache;

#if defined(SHADER_STATS_AVAILABLE)
typedef struct ShaderStats
{
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
#if defined(VULKAN)
        struct
        {
            void*    pDisassemblyAMD;
            uint32_t mDisassemblySize;
        } mVk;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
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
    ShaderStats mStats[SHADER_STAGE_COUNT];
} PipelineStats;
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
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
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
#if defined(DIRECT3D11)
        struct
        {
            /// Use IDXGISwapChain3 for now since IDXGISwapChain4
            /// isn't supported by older devices.
            IDXGISwapChain*  pSwapChain;
            /// Sync interval to specify how interval for vsync
            uint32_t         mSyncInterval : 3;
            uint32_t         mFlags : 10;
            uint32_t         mImageIndex : 3;
            DXGI_SWAP_EFFECT mSwapEffect;
        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            GLSurface pSurface;
        } mGLES;
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
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
#endif
    uint32_t        mImageCount : 8;
    uint32_t        mEnableVsync : 1;
    ColorSpace      mColorSpace : 4;
    TinyImageFormat mFormat : 8;
} SwapChain;

typedef enum ShaderTarget
{
// We only need SM 5.0 for supporting D3D11 fallback
#if defined(DIRECT3D11)
    SHADER_TARGET_5_0,
#endif
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

typedef struct ExtendedSettings
{
    uint32_t     mNumSettings;
    uint32_t*    pSettings;
    const char** ppSettingNames;
} ExtendedSettings;

typedef struct RendererDesc
{
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
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
#if defined(DIRECT3D11)
        struct
        {
            /// Set whether to force feature level 10 for compatibility
            bool mUseDx10;
            /// Set whether to pick the first valid GPU or use our GpuConfig
            bool mUseDefaultGpu;
        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            const char** ppDeviceExtensions;
            uint32_t     mDeviceExtensionCount;
        } mGLES;
#endif
#if defined(ORBIS)
        OrbisExtendedDesc mExt;
#endif
#if defined(PROSPERO)
        ProsperoExtendedDesc mExt;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
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

    bool mD3D11Supported;
    bool mGLESSupported;
#if defined(VULKAN) && defined(ANDROID)
    bool mPreferVulkan;
#endif

    // Also, if `ReloadServer` code is interfering with debugging (due to threads/networking), then it can be temporarily disabled via this
    // flag. NOTE: This flag overrides the behaviour specified by the `EnableReloadServer` field.
    bool mDisableReloadServer;
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

typedef struct GPUCapBits
{
    FormatCapability mFormatCaps[TinyImageFormat_Count];
} GPUCapBits;

typedef enum DefaultResourceAlignment
{
    RESOURCE_BUFFER_ALIGNMENT = 4U,
} DefaultResourceAlignment;

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

// update availableGpuProperties in GPUConfig.cpp if you made changes to this list
typedef struct GPUSettings
{
    uint64_t mVRAM; // set to 0 on OpenGLES platform
    uint32_t mUniformBufferAlignment;
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
    uint32_t mIndirectRootConstant : 1;
    uint32_t mBuiltinDrawID : 1;
    uint32_t mIndirectCommandBuffer : 1;
    uint32_t mROVsSupported : 1;
    uint32_t mTessellationSupported : 1;
    uint32_t mGeometryShaderSupported : 1;
    uint32_t mGpuBreadcrumbs : 1;
    uint32_t mHDRSupported : 1;
    uint32_t mTimestampQueries : 1;
    uint32_t mOcclusionQueries : 1;
    uint32_t mPipelineStatsQueries : 1;
    uint32_t mAllowBufferTextureInSameHeap : 1;
    uint32_t mRaytracingSupported : 1;
    uint32_t mRayPipelineSupported : 1;
    uint32_t mRayQuerySupported : 1;
    uint32_t mSoftwareVRSSupported : 1;
    uint32_t mPrimitiveIdSupported : 1;
#if defined(DIRECT3D11) || defined(DIRECT3D12)
    D3D_FEATURE_LEVEL mFeatureLevel;
#endif
#if defined(VULKAN)
    uint32_t mDynamicRenderingSupported : 1;
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
} GPUSettings;

typedef struct DEFINE_ALIGNED(Renderer, 64)
{
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
#if defined(DIRECT3D12)
        struct
        {
            // API specific descriptor heap and memory allocator
            struct DescriptorHeap** pCPUDescriptorHeaps;
            struct DescriptorHeap** pCbvSrvUavHeaps;
            struct DescriptorHeap** pSamplerHeaps;
            D3D12MAAllocator*       pResourceAllocator;
#if defined(XBOX)
            ID3D12Device* pDevice;
            EsramManager* pESRAMManager;
#elif defined(DIRECT3D12)
            ID3D12Device*                            pDevice;
#endif
#if defined(_WINDOWS) && defined(FORGE_DEBUG)
            ID3D12InfoQueue* pDebugValidation;
            bool             mSuppressMismatchingCommandListDuringPresent;
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
            struct VmaAllocator_T* pVmaAllocator;
            union
            {
                struct
                {
                    uint8_t mGraphicsQueueFamilyIndex;
                    uint8_t mTransferQueueFamilyIndex;
                    uint8_t mComputeQueueFamilyIndex;
                };
                uint8_t mQueueFamilyIndices[3];
            };
        } mVk;
#endif
#if defined(METAL)
        struct
        {
            id<MTLDevice>          pDevice;
            struct VmaAllocator_T* pVmaAllocator;
            NOREFS id<MTLHeap>* pHeaps;
            uint32_t            mHeapCount;
            uint32_t            mHeapCapacity;
            // To synchronize resource allocation done through automatic heaps
            Mutex*              pHeapMutex;
        };
#endif
#if defined(DIRECT3D11)
        struct
        {
            ID3D11Device*              pDevice;
            ID3D11DeviceContext*       pContext;
            ID3D11DeviceContext1*      pContext1;
            ID3D11BlendState*          pDefaultBlendState;
            ID3D11DepthStencilState*   pDefaultDepthState;
            ID3D11RasterizerState*     pDefaultRasterizerState;
            ID3DUserDefinedAnnotation* pUserDefinedAnnotation;
        } mDx11;
#endif
#if defined(GLES)
        struct
        {
            GLContext pContext;
            GLConfig  pConfig;
        } mGLES;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
#endif

#if defined(ENABLE_NSIGHT_AFTERMATH)
    // GPU crash dump tracker using Nsight Aftermath instrumentation
    AftermathTracker mAftermathTracker;
#endif
    struct NullDescriptors* pNullDescriptors;
    struct RendererContext* pContext;
    const struct GpuInfo*   pGpu;
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
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
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
#if defined(DIRECT3D11)
        struct
        {
            /// Set whether to force feature level 10 for compatibility
            bool mUseDx10;
            /// Set whether to pick the first valid GPU or use our GpuConfig
            bool mUseDefaultGpu;
        } mDx11;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
#endif
    bool mEnableGpuBasedValidation;
#if defined(SHADER_STATS_AVAILABLE)
    bool mEnableShaderStats;
#endif
    bool mD3D11Supported;
    bool mGLESSupported;
#if defined(VULKAN) && defined(ANDROID)
    bool mPreferVulkan;
#endif
} RendererContextDesc;

typedef struct GpuInfo
{
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
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
            uint32_t                    mYCbCrExtension : 1;
            uint32_t                    mFillModeNonSolid : 1;
            uint32_t                    mKHRRayQueryExtension : 1;
            uint32_t                    mAMDGCNShaderExtension : 1;
            uint32_t                    mAMDDrawIndirectCountExtension : 1;
            uint32_t                    mAMDShaderInfoExtension : 1;
            uint32_t                    mDescriptorIndexingExtension : 1;
            uint32_t                    mDynamicRenderingExtension : 1;
            uint32_t                    mShaderSampledImageArrayDynamicIndexingSupported : 1;
            uint32_t                    mBufferDeviceAddressSupported : 1;
            uint32_t                    mDrawIndirectCountExtension : 1;
            uint32_t                    mDedicatedAllocationExtension : 1;
            uint32_t                    mDebugMarkerExtension : 1;
            uint32_t                    mMemoryReq2Extension : 1;
            uint32_t                    mFragmentShaderInterlockExtension : 1;
            uint32_t                    mBufferDeviceAddressExtension : 1;
            uint32_t                    mAccelerationStructureExtension : 1;
            uint32_t                    mRayTracingPipelineExtension : 1;
            uint32_t                    mRayQueryExtension : 1;
            uint32_t                    mBufferDeviceAddressFeature : 1;
            uint32_t                    mShaderFloatControlsExtension : 1;
            uint32_t                    mSpirv14Extension : 1;
            uint32_t                    mDeferredHostOperationsExtension : 1;
            uint32_t                    mDeviceFaultExtension : 1;
            uint32_t                    mDeviceFaultSupported : 1;
            uint32_t                    mASTCDecodeModeExtension : 1;
            uint32_t                    mDeviceMemoryReportExtension : 1;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
            uint32_t mExternalMemoryExtension : 1;
            uint32_t mExternalMemoryWin32Extension : 1;
#endif
#if defined(QUEST_VR)
            uint32_t mMultiviewExtension : 1;
#endif
#if defined(ENABLE_NSIGHT_AFTERMATH)
            uint32_t mNVDeviceDiagnosticsCheckpointExtension : 1;
            uint32_t mNVDeviceDiagnosticsConfigExtension : 1;
            uint32_t mAftermathSupport : 1;
#endif
        } mVk;
#endif
#if defined(DIRECT3D11)
        struct
        {
            IDXGIAdapter1* pGpu;
            uint32_t       mPartialUpdateConstantBufferSupported : 1;
        } mDx11;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
#endif
#if defined(METAL)
    id<MTLDevice> pGPU;
#endif
    GPUSettings mSettings;
    GPUCapBits  mCapBits;
} GpuInfo;

typedef struct RendererContext
{
#if defined(USE_MULTIPLE_RENDER_APIS)
    union
    {
#endif
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
#if defined(DIRECT3D11)
        struct
        {
            IDXGIFactory1* pDXGIFactory;
        } mDx11;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
    };
#endif
#if defined(METAL)
    struct
    {
        uint32_t mExtendedEncoderDebugReport : 1;
    } mMtl;
#endif
    GpuInfo  mGpus[MAX_MULTIPLE_GPUS];
    uint32_t mGpuCount;
} RendererContext;

// Indirect command structure define
typedef struct IndirectArgument
{
    IndirectArgumentType mType;
    uint32_t             mOffset;
} IndirectArgument;

typedef struct IndirectArgumentDescriptor
{
    IndirectArgumentType mType;
    uint32_t             mIndex;
    uint32_t             mByteSize;
} IndirectArgumentDescriptor;

typedef struct CommandSignatureDesc
{
    RootSignature*              pRootSignature;
    IndirectArgumentDescriptor* pArgDescs;
    uint32_t                    mIndirectArgCount;
    /// Set to true if indirect argument struct should not be aligned to 16 bytes
    bool                        mPacked;
} CommandSignatureDesc;

typedef struct CommandSignature
{
#if defined(DIRECT3D12)
    ID3D12CommandSignature* pHandle;
#endif
    IndirectArgumentType mDrawType;
    uint32_t             mStride;
} CommandSignature;

typedef struct DescriptorSetDesc
{
    RootSignature*            pRootSignature;
    DescriptorUpdateFrequency mUpdateFrequency;
    uint32_t                  mMaxSets;
    uint32_t                  mNodeIndex;
} DescriptorSetDesc;

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

typedef struct BindRenderTargetsDesc
{
    uint32_t             mRenderTargetCount;
    BindRenderTargetDesc mRenderTargets[MAX_RENDER_TARGET_ATTACHMENTS];
    BindDepthTargetDesc  mDepthStencil;
    // Explicit viewport for empty render pass
    uint32_t             mExtent[2];
} BindRenderTargetsDesc;

#ifdef __INTELLISENSE__
// IntelliSense is the code completion engine in Visual Studio. When it parses the source files, __INTELLISENSE__ macro is defined.
// Here we trick IntelliSense into thinking that the renderer functions are not function pointers, but just regular functions.
// What this achieves is filtering out duplicated function names from code completion results and improving the code completion for function
// parameters. This dramatically improves the quality of life for Visual Studio users.
#define DECLARE_RENDERER_FUNCTION(ret, name, ...) ret name(__VA_ARGS__);
#else
#define DECLARE_RENDERER_FUNCTION(ret, name, ...)       \
    typedef ret(FORGE_CALLCONV* name##Fn)(__VA_ARGS__); \
    FORGE_RENDERER_API extern name##Fn name;
#endif

// clang-format off
// Utilities functions
FORGE_RENDERER_API void setRendererInitializationError(const char* reason);
FORGE_RENDERER_API bool hasRendererInitializationError(const char** outReason);

// API functions

// Multiple renderer API (optional)
FORGE_RENDERER_API void FORGE_CALLCONV initRendererContext(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext);
FORGE_RENDERER_API void FORGE_CALLCONV exitRendererContext(RendererContext* pContext);

// allocates memory and initializes the renderer -> returns pRenderer
//
FORGE_RENDERER_API void FORGE_CALLCONV initRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
FORGE_RENDERER_API void FORGE_CALLCONV exitRenderer(Renderer* pRenderer);

DECLARE_RENDERER_FUNCTION(void, addFence, Renderer* pRenderer, Fence** ppFence)
DECLARE_RENDERER_FUNCTION(void, removeFence, Renderer* pRenderer, Fence* pFence)

DECLARE_RENDERER_FUNCTION(void, addSemaphore, Renderer* pRenderer, Semaphore** ppSemaphore)
DECLARE_RENDERER_FUNCTION(void, removeSemaphore, Renderer* pRenderer, Semaphore* pSemaphore)

DECLARE_RENDERER_FUNCTION(void, addQueue, Renderer* pRenderer, QueueDesc* pQDesc, Queue** ppQueue)
DECLARE_RENDERER_FUNCTION(void, removeQueue, Renderer* pRenderer, Queue* pQueue)

DECLARE_RENDERER_FUNCTION(void, addSwapChain, Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
DECLARE_RENDERER_FUNCTION(void, removeSwapChain, Renderer* pRenderer, SwapChain* pSwapChain)

// memory functions
DECLARE_RENDERER_FUNCTION(void, addResourceHeap, Renderer* pRenderer, const ResourceHeapDesc* pDesc, ResourceHeap** ppHeap)
DECLARE_RENDERER_FUNCTION(void, removeResourceHeap, Renderer* pRenderer, ResourceHeap* pHeap)

// command pool functions
DECLARE_RENDERER_FUNCTION(void, addCmdPool, Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
DECLARE_RENDERER_FUNCTION(void, removeCmdPool, Renderer* pRenderer, CmdPool* pCmdPool)
DECLARE_RENDERER_FUNCTION(void, addCmd, Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
DECLARE_RENDERER_FUNCTION(void, removeCmd, Renderer* pRenderer, Cmd* pCmd)
DECLARE_RENDERER_FUNCTION(void, addCmd_n, Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmds)
DECLARE_RENDERER_FUNCTION(void, removeCmd_n, Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)

//
// All buffer, texture loading handled by resource system -> IResourceLoader.*
//

DECLARE_RENDERER_FUNCTION(void, addRenderTarget, Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
DECLARE_RENDERER_FUNCTION(void, removeRenderTarget, Renderer* pRenderer, RenderTarget* pRenderTarget)
DECLARE_RENDERER_FUNCTION(void, addSampler, Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
DECLARE_RENDERER_FUNCTION(void, removeSampler, Renderer* pRenderer, Sampler* pSampler)

// shader functions
DECLARE_RENDERER_FUNCTION(void, addShaderBinary, Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
DECLARE_RENDERER_FUNCTION(void, removeShader, Renderer* pRenderer, Shader* pShaderProgram)

DECLARE_RENDERER_FUNCTION(void, addRootSignature, Renderer* pRenderer, const RootSignatureDesc* pDesc, RootSignature** ppRootSignature)
DECLARE_RENDERER_FUNCTION(void, removeRootSignature, Renderer* pRenderer, RootSignature* pRootSignature)
DECLARE_RENDERER_FUNCTION(uint32_t, getDescriptorIndexFromName, const RootSignature* pRootSignature, const char* pName)

// pipeline functions
DECLARE_RENDERER_FUNCTION(void, addPipeline, Renderer* pRenderer, const PipelineDesc* pPipelineSettings, Pipeline** ppPipeline)
DECLARE_RENDERER_FUNCTION(void, removePipeline, Renderer* pRenderer, Pipeline* pPipeline)
DECLARE_RENDERER_FUNCTION(void, addPipelineCache, Renderer* pRenderer, const PipelineCacheDesc* pDesc, PipelineCache** ppPipelineCache)
DECLARE_RENDERER_FUNCTION(void, getPipelineCacheData, Renderer* pRenderer, PipelineCache* pPipelineCache, size_t* pSize, void* pData)
#if defined(SHADER_STATS_AVAILABLE)
DECLARE_RENDERER_FUNCTION(void, addPipelineStats, Renderer* pRenderer, Pipeline* pPipeline, bool generateDisassembly, PipelineStats* pOutStats);
DECLARE_RENDERER_FUNCTION(void, removePipelineStats, Renderer* pRenderer, PipelineStats* pStats);
#endif
DECLARE_RENDERER_FUNCTION(void, removePipelineCache, Renderer* pRenderer, PipelineCache* pPipelineCache)

// Descriptor Set functions
DECLARE_RENDERER_FUNCTION(void, addDescriptorSet, Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
DECLARE_RENDERER_FUNCTION(void, removeDescriptorSet, Renderer* pRenderer, DescriptorSet* pDescriptorSet)
DECLARE_RENDERER_FUNCTION(void, updateDescriptorSet, Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)

// command buffer functions
DECLARE_RENDERER_FUNCTION(void, resetCmdPool, Renderer* pRenderer, CmdPool* pCmdPool)
DECLARE_RENDERER_FUNCTION(void, beginCmd, Cmd* pCmd)
DECLARE_RENDERER_FUNCTION(void, endCmd, Cmd* pCmd)
DECLARE_RENDERER_FUNCTION(void, cmdBindRenderTargets, Cmd* pCmd, const BindRenderTargetsDesc* pDesc)
DECLARE_RENDERER_FUNCTION(void, cmdSetSampleLocations, Cmd* pCmd, SampleCount samplesCount, uint32_t gridSizeX, uint32_t gridSizeY, SampleLocations* plocations);
DECLARE_RENDERER_FUNCTION(void, cmdSetViewport, Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
DECLARE_RENDERER_FUNCTION(void, cmdSetScissor, Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
DECLARE_RENDERER_FUNCTION(void, cmdSetStencilReferenceValue, Cmd* pCmd, uint32_t val)
DECLARE_RENDERER_FUNCTION(void, cmdBindPipeline, Cmd* pCmd, Pipeline* pPipeline)
DECLARE_RENDERER_FUNCTION(void, cmdBindDescriptorSet, Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
DECLARE_RENDERER_FUNCTION(void, cmdBindPushConstants, Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
DECLARE_RENDERER_FUNCTION(void, cmdBindDescriptorSetWithRootCbvs, Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
DECLARE_RENDERER_FUNCTION(void, cmdBindIndexBuffer, Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
DECLARE_RENDERER_FUNCTION(void, cmdBindVertexBuffer, Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
DECLARE_RENDERER_FUNCTION(void, cmdDraw, Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
DECLARE_RENDERER_FUNCTION(void, cmdDrawInstanced, Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
DECLARE_RENDERER_FUNCTION(void, cmdDrawIndexed, Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
DECLARE_RENDERER_FUNCTION(void, cmdDrawIndexedInstanced, Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
DECLARE_RENDERER_FUNCTION(void, cmdDispatch, Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)

// Transition Commands
DECLARE_RENDERER_FUNCTION(void, cmdResourceBarrier, Cmd* pCmd, uint32_t bufferBarrierCount, BufferBarrier* pBufferBarriers, uint32_t textureBarrierCount, TextureBarrier* pTextureBarriers, uint32_t rtBarrierCount, RenderTargetBarrier* pRtBarriers)

// queue/fence/swapchain functions
DECLARE_RENDERER_FUNCTION(void, acquireNextImage, Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex)
DECLARE_RENDERER_FUNCTION(void, queueSubmit, Queue* pQueue, const QueueSubmitDesc* pDesc)
DECLARE_RENDERER_FUNCTION(void, queuePresent, Queue* pQueue, const QueuePresentDesc* pDesc)
DECLARE_RENDERER_FUNCTION(void, waitQueueIdle, Queue* pQueue)
DECLARE_RENDERER_FUNCTION(void, getFenceStatus, Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
DECLARE_RENDERER_FUNCTION(void, waitForFences, Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
DECLARE_RENDERER_FUNCTION(void, toggleVSync, Renderer* pRenderer, SwapChain** ppSwapchain)

//Returns the recommended format for the swapchain.
//If true is passed for the hintHDR parameter, it will return an HDR format IF the platform supports it
//If false is passed or the platform does not support HDR a non HDR format is returned.
//If true is passed for the hintSrgb parameter, it will return format that is will do gamma correction automatically
//If false is passed for the hintSrgb parameter the gamma correction should be done as a postprocess step before submitting image to swapchain
DECLARE_RENDERER_FUNCTION(TinyImageFormat, getSupportedSwapchainFormat, Renderer *pRenderer, const SwapChainDesc* pDesc, ColorSpace colorSpace)
DECLARE_RENDERER_FUNCTION(uint32_t, getRecommendedSwapchainImageCount, Renderer* pRenderer, const WindowHandle* hwnd)

//indirect Draw functions
DECLARE_RENDERER_FUNCTION(void, addIndirectCommandSignature, Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
DECLARE_RENDERER_FUNCTION(void, removeIndirectCommandSignature, Renderer* pRenderer, CommandSignature* pCommandSignature)
DECLARE_RENDERER_FUNCTION(void, cmdExecuteIndirect, Cmd* pCmd, CommandSignature* pCommandSignature, unsigned int maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset)

/************************************************************************/
// GPU Query Interface
/************************************************************************/
DECLARE_RENDERER_FUNCTION(void, getTimestampFrequency, Queue* pQueue, double* pFrequency)
DECLARE_RENDERER_FUNCTION(void, addQueryPool, Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
DECLARE_RENDERER_FUNCTION(void, removeQueryPool, Renderer* pRenderer, QueryPool* pQueryPool)
DECLARE_RENDERER_FUNCTION(void, cmdBeginQuery, Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
DECLARE_RENDERER_FUNCTION(void, cmdEndQuery, Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
DECLARE_RENDERER_FUNCTION(void, cmdResolveQuery, Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
DECLARE_RENDERER_FUNCTION(void, cmdResetQuery, Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
DECLARE_RENDERER_FUNCTION(void, getQueryData, Renderer* pRenderer, QueryPool* pQueryPool, uint32_t queryIndex, QueryData* pOutData)
/************************************************************************/
// Stats Info Interface
/************************************************************************/
DECLARE_RENDERER_FUNCTION(void, calculateMemoryStats, Renderer* pRenderer, char** ppStats)
DECLARE_RENDERER_FUNCTION(void, calculateMemoryUse, Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
DECLARE_RENDERER_FUNCTION(void, freeMemoryStats, Renderer* pRenderer, char* pStats)
/************************************************************************/
// Debug Marker Interface
/************************************************************************/
DECLARE_RENDERER_FUNCTION(void, cmdBeginDebugMarker, Cmd* pCmd, float r, float g, float b, const char* pName)
DECLARE_RENDERER_FUNCTION(void, cmdEndDebugMarker, Cmd* pCmd)
DECLARE_RENDERER_FUNCTION(void, cmdAddDebugMarker, Cmd* pCmd, float r, float g, float b, const char* pName)
DECLARE_RENDERER_FUNCTION(uint32_t, cmdWriteMarker, Cmd* pCmd, MarkerType markerType, uint32_t markerValue, Buffer* pBuffer, size_t offset, bool useAutoFlags);
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
DECLARE_RENDERER_FUNCTION(void, setBufferName, Renderer* pRenderer, Buffer* pBuffer, const char* pName)
DECLARE_RENDERER_FUNCTION(void, setTextureName, Renderer* pRenderer, Texture* pTexture, const char* pName)
DECLARE_RENDERER_FUNCTION(void, setRenderTargetName, Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName)
DECLARE_RENDERER_FUNCTION(void, setPipelineName, Renderer* pRenderer, Pipeline* pPipeline, const char* pName)
/************************************************************************/
/************************************************************************/
// clang-format on

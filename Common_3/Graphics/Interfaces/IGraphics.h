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

#include "../GraphicsConfig.h"

#ifdef ENABLE_NSIGHT_AFTERMATH
#include "../ThirdParty/PrivateNvidia/NsightAftermath/include/AftermathTracker.h"
#endif

#include "../../Utilities/Interfaces/ILog.h"
#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"

#ifdef __cplusplus
#ifndef MAKE_ENUM_FLAG
#define MAKE_ENUM_FLAG(TYPE, ENUM_TYPE)                                                                              \
inline FORGE_CONSTEXPR ENUM_TYPE operator| (ENUM_TYPE a, ENUM_TYPE b) { return ENUM_TYPE(((TYPE)a) | ((TYPE)b)); }   \
inline ENUM_TYPE& operator|= (ENUM_TYPE& a, ENUM_TYPE b) { return (ENUM_TYPE&)(((TYPE&)a) |= ((TYPE)b)); }           \
inline FORGE_CONSTEXPR ENUM_TYPE operator& (ENUM_TYPE a, ENUM_TYPE b) { return ENUM_TYPE(((TYPE)a) & ((TYPE)b)); }   \
inline ENUM_TYPE& operator&= (ENUM_TYPE& a, ENUM_TYPE b) { return (ENUM_TYPE&)(((TYPE&)a) &= ((TYPE)b)); }           \
inline FORGE_CONSTEXPR ENUM_TYPE operator~ (ENUM_TYPE a) { return ENUM_TYPE(~((TYPE)a)); }                           \
inline FORGE_CONSTEXPR ENUM_TYPE operator^ (ENUM_TYPE a, ENUM_TYPE b) { return ENUM_TYPE(((TYPE)a) ^ ((TYPE)b)); }   \
inline ENUM_TYPE &operator ^= (ENUM_TYPE& a, ENUM_TYPE b) { return (ENUM_TYPE&)(((TYPE&)a) ^= ((TYPE)b)); }
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
	/// Max number of GPus for either linked or unlinked mode.
	MAX_MULTIPLE_GPUS = 4,
	MAX_RENDER_TARGET_ATTACHMENTS = 8,
	MAX_VERTEX_BINDINGS = 15,
	MAX_VERTEX_ATTRIBS = 15,
	MAX_RESOURCE_NAME_LENGTH = 256,
	MAX_SEMANTIC_NAME_LENGTH = 128,
	MAX_DEBUG_NAME_LENGTH = 128,
	MAX_MIP_LEVELS = 0xFFFFFFFF,
	MAX_SWAPCHAIN_IMAGES = 3,
	MAX_GPU_VENDOR_STRING_LENGTH = 256,    //max size for GPUVendorPreset strings
#if defined(VULKAN)
	MAX_PLANE_COUNT = 3,
	MAX_DESCRIPTOR_POOL_SIZE_ARRAY_COUNT = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT + 1,
#endif
};
#endif


#ifdef DIRECT3D12
// Forward declare memory allocator classes
namespace D3D12MA
{
	class Allocator;
	class Allocation;
} // namespace D3D12MA

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
	RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE = 0x4000,
	RESOURCE_STATE_SHADING_RATE_SOURCE = 0x8000,
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

#ifdef ENABLE_DEPENDENCY_TRACKER
typedef struct
{
	Shader* key;
	int value;
} ShaderDependencyNode;

typedef struct
{
	RootSignature* key;
	int value;
} RootSignatureDependencyNode;

typedef struct
{
	Pipeline* key;
	int value;
} PipelineDependencyNode;
#endif

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

#define INDIRECT_DRAW_ELEM_INDEX(m) (offsetof(IndirectDrawArguments, m) / sizeof(uint32_t))
#define INDIRECT_DRAW_INDEX_ELEM_INDEX(m) (offsetof(IndirectDrawIndexArguments, m) / sizeof(uint32_t))
#define INDIRECT_DISPATCH_ELEM_INDEX(m) (offsetof(IndirectDispatchArguments, m) / sizeof(uint32_t))

typedef enum IndirectArgumentType
{
	INDIRECT_ARG_INVALID,
	INDIRECT_DRAW,
	INDIRECT_DRAW_INDEX,
	INDIRECT_DISPATCH,
	INDIRECT_VERTEX_BUFFER,
	INDIRECT_INDEX_BUFFER,
	INDIRECT_CONSTANT,
	INDIRECT_CONSTANT_BUFFER_VIEW,     // only for dx
	INDIRECT_SHADER_RESOURCE_VIEW,     // only for dx
	INDIRECT_UNORDERED_ACCESS_VIEW,    // only for dx
	INDIRECT_COMMAND_BUFFER,            // metal ICB
	INDIRECT_COMMAND_BUFFER_RESET,      // metal ICB reset
	INDIRECT_COMMAND_BUFFER_OPTIMIZE    // metal ICB optimization
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
	DESCRIPTOR_TYPE_RAY_TRACING = (DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES << 1),
	DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER = (DESCRIPTOR_TYPE_RAY_TRACING << 1),
#if defined(VULKAN)
	/// Subpass input (descriptor type only available in Vulkan)
	DESCRIPTOR_TYPE_INPUT_ATTACHMENT = (DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER << 1),
	DESCRIPTOR_TYPE_TEXEL_BUFFER = (DESCRIPTOR_TYPE_INPUT_ATTACHMENT << 1),
	DESCRIPTOR_TYPE_RW_TEXEL_BUFFER = (DESCRIPTOR_TYPE_TEXEL_BUFFER << 1),
	DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = (DESCRIPTOR_TYPE_RW_TEXEL_BUFFER << 1),
    
	/// Khronos extension ray tracing
	DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE = (DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER << 1),
	DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT = (DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE << 1),
	DESCRIPTOR_TYPE_SHADER_DEVICE_ADDRESS = (DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT << 1),
	DESCRIPTOR_TYPE_SHADER_BINDING_TABLE = (DESCRIPTOR_TYPE_SHADER_DEVICE_ADDRESS << 1),
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
	SHADER_STAGE_RAYTRACING = 0X00000040,
	SHADER_STAGE_ALL_GRAPHICS =
		((uint32_t)SHADER_STAGE_VERT | (uint32_t)SHADER_STAGE_TESC | (uint32_t)SHADER_STAGE_TESE | (uint32_t)SHADER_STAGE_GEOM |
		 (uint32_t)SHADER_STAGE_FRAG),
	SHADER_STAGE_HULL = SHADER_STAGE_TESC,
	SHADER_STAGE_DOMN = SHADER_STAGE_TESE,
	SHADER_STAGE_COUNT = 7,
} ShaderStage;
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
	SEMANTIC_SHADING_RATE,
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

static const int RED = 0x1;
static const int GREEN = 0x2;
static const int BLUE = 0x4;
static const int ALPHA = 0x8;
static const int ALL = (RED | GREEN | BLUE | ALPHA);
static const int NONE = 0;

static const int BS_NONE = -1;
static const int DS_NONE = -1;
static const int RS_NONE = -1;

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
	PIPELINE_TYPE_RAYTRACING,
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

typedef enum ShadingRate
{
	SHADING_RATE_NOT_SUPPORTED = 0x0,
	SHADING_RATE_FULL = 0x1,
	SHADING_RATE_HALF = SHADING_RATE_FULL << 1,
	SHADING_RATE_QUARTER = SHADING_RATE_HALF << 1,
	SHADING_RATE_EIGHTH = SHADING_RATE_QUARTER << 1,
	SHADING_RATE_1X2 = SHADING_RATE_EIGHTH << 1,
	SHADING_RATE_2X1 = SHADING_RATE_1X2 << 1,
	SHADING_RATE_2X4 = SHADING_RATE_2X1 << 1,
	SHADING_RATE_4X2 = SHADING_RATE_2X4 << 1,
} ShadingRate;
MAKE_ENUM_FLAG(uint32_t, ShadingRate)

typedef enum ShadingRateCombiner
{
	SHADING_RATE_COMBINER_PASSTHROUGH = 0,
	SHADING_RATE_COMBINER_OVERRIDE = 1,
	SHADING_RATE_COMBINER_MIN = 2,
	SHADING_RATE_COMBINER_MAX = 3,
	SHADING_RATE_COMBINER_SUM = 4,
} ShadingRateCombiner;

typedef enum ShadingRateCaps
{
	SHADING_RATE_CAPS_NOT_SUPPORTED = 0x0,
	SHADING_RATE_CAPS_PER_DRAW = 0x1,
	SHADING_RATE_CAPS_PER_TILE = SHADING_RATE_CAPS_PER_DRAW << 1,
} ShadingRateCaps;
MAKE_ENUM_FLAG(uint32_t, ShadingRateCaps)

typedef enum BufferCreationFlags
{
	/// Default flag (Buffer will use aliased memory, buffer will not be cpu accessible until mapBuffer is called)
	BUFFER_CREATION_FLAG_NONE = 0x01,
	/// Buffer will allocate its own memory (COMMITTED resource)
	BUFFER_CREATION_FLAG_OWN_MEMORY_BIT = 0x02,
	/// Buffer will be persistently mapped
	BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT = 0x04,
	/// Use ESRAM to store this buffer
	BUFFER_CREATION_FLAG_ESRAM = 0x08,
	/// Flag to specify not to allocate descriptors for the resource
	BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION = 0x10,

#ifdef VULKAN
	/* Memory Host Flags */
	BUFFER_CREATION_FLAG_HOST_VISIBLE = 0x100,
	BUFFER_CREATION_FLAG_HOST_COHERENT = 0x200,
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

typedef enum GPUPresetLevel
{
	GPU_PRESET_NONE = 0,
	GPU_PRESET_OFFICE,    //This means unsupported
	GPU_PRESET_LOW,
	GPU_PRESET_MEDIUM,
	GPU_PRESET_HIGH,
	GPU_PRESET_ULTRA,
	GPU_PRESET_COUNT
} GPUPresetLevel;

typedef struct BufferBarrier
{
	Buffer*       pBuffer;
	ResourceState mCurrentState;
	ResourceState mNewState;
	uint8_t       mBeginOnly : 1;
	uint8_t       mEndOnly : 1;
	uint8_t       mAcquire : 1;
	uint8_t       mRelease : 1;
	uint8_t       mQueueType : 5;
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
	uint8_t mSubresourceBarrier : 1;
	/// Following values are ignored if mSubresourceBarrier is false
	uint8_t  mMipLevel : 7;
	uint16_t mArrayLayer;
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
	uint8_t mSubresourceBarrier : 1;
	/// Following values are ignored if mSubresourceBarrier is false
	uint8_t  mMipLevel : 7;
	uint16_t mArrayLayer;
} RenderTargetBarrier;

typedef struct ReadRange
{
	uint64_t mOffset;
	uint64_t mSize;
} ReadRange;

typedef enum QueryType
{
	QUERY_TYPE_TIMESTAMP = 0,
	QUERY_TYPE_PIPELINE_STATISTICS,
	QUERY_TYPE_OCCLUSION,
	QUERY_TYPE_COUNT,
} QueryType;

typedef struct QueryPoolDesc
{
	QueryType mType;
	uint32_t  mQueryCount;
	uint32_t  mNodeIndex;
} QueryPoolDesc;

typedef struct QueryDesc
{
	uint32_t mIndex;
} QueryDesc;

typedef struct QueryPool
{
	union
	{
#if defined(DIRECT3D12)
		struct
		{
			ID3D12QueryHeap* pDxQueryHeap;
			D3D12_QUERY_TYPE mType;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			VkQueryPool pVkQueryPool;
			VkQueryType mType;
		} mVulkan;
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
			ID3D11Query** ppDxQueries;
			D3D11_QUERY   mType;
		} mD3D11;
#endif
#if defined(GLES)
		struct
		{
			uint32_t* pQueries;
			uint32_t  mType;
			int32_t   mDisjointOccurred;
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
	};
	uint32_t mCount;
} QueryPool;

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

/// Data structure holding necessary info to create a Buffer
typedef struct BufferDesc
{
	/// Size of the buffer (in bytes)
	uint64_t mSize;
	/// Set this to specify a counter buffer for this buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	struct Buffer* pCounterBuffer;
	/// Index of the first element accessible by the SRV/UAV (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	uint64_t mFirstElement;
	/// Number of elements in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	uint64_t mElementCount;
	/// Size of each element (in bytes) in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	uint64_t mStructStride;
	/// Debug name used in gpu profile
	const char* pName;
	uint32_t*   pSharedNodeIndices;
	/// Alignment
	uint32_t mAlignment;
	/// Decides which memory heap buffer will use (default, upload, readback)
	ResourceMemoryUsage mMemoryUsage;
	/// Creation flags of the buffer
	BufferCreationFlags mFlags;
	/// What type of queue the buffer is owned by
	QueueType mQueueType;
	/// What state will the buffer get created in
	ResourceState mStartState;
	/// ICB draw type
	IndirectArgumentType mICBDrawType;
	/// ICB max vertex buffers slots count
	uint32_t mICBMaxCommandCount;
	/// Format of the buffer (applicable to typed storage buffers (Buffer<T>)
	TinyImageFormat mFormat;
	/// Flags specifying the suitable usage of this buffer (Uniform buffer, Vertex Buffer, Index Buffer,...)
	DescriptorType mDescriptors;
	/// The index of the GPU in SLI/Cross-Fire that owns this buffer, or the Renderer index in unlinked mode.
	uint32_t       mNodeIndex;
	uint32_t       mSharedNodeIndexCount;
} BufferDesc;

typedef struct DEFINE_ALIGNED(Buffer, 64)
{
	/// CPU address of the mapped buffer (applicable to buffers created in CPU accessible heaps (CPU, CPU_TO_GPU, GPU_TO_CPU)
	void* pCpuMappedAddress;
	union
	{
#if defined(DIRECT3D12)
		struct
		{
			/// GPU Address - Cache to avoid calls to ID3D12Resource::GetGpuVirtualAddress
			D3D12_GPU_VIRTUAL_ADDRESS mDxGpuAddress;
			/// Descriptor handle of the CBV in a CPU visible descriptor heap (applicable to BUFFER_USAGE_UNIFORM)
			DxDescriptorID mDescriptors;
			/// Offset from mDxDescriptors for srv descriptor handle
			uint8_t mSrvDescriptorOffset;
			/// Offset from mDxDescriptors for uav descriptor handle
			uint8_t mUavDescriptorOffset;
			/// Native handle of the underlying resource
			ID3D12Resource* pDxResource;
			/// Contains resource allocation info such as parent heap, offset in heap
			D3D12MA::Allocation* pDxAllocation;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			/// Native handle of the underlying resource
			VkBuffer pVkBuffer;
			/// Buffer view
			VkBufferView pVkStorageTexelView;
			VkBufferView pVkUniformTexelView;
			/// Contains resource allocation info such as parent heap, offset in heap
			struct VmaAllocation_T* pVkAllocation;
			uint64_t                mOffset;
		} mVulkan;
#endif
#if defined(METAL)
		struct
		{
			struct VmaAllocation_T*      pAllocation;
			id<MTLBuffer>                mtlBuffer;
			id<MTLIndirectCommandBuffer> mtlIndirectCommandBuffer API_AVAILABLE(macos(10.14), ios(12.0));
			uint64_t                                              mOffset;
			uint64_t                                              mPadB;
		};
#endif
#if defined(DIRECT3D11)
		struct
		{
			ID3D11Buffer*              pDxResource;
			ID3D11ShaderResourceView*  pDxSrvHandle;
			ID3D11UnorderedAccessView* pDxUavHandle;
			uint64_t                   mFlags;
			uint64_t                   mPadA;
		} mD3D11;
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
	};
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
	/// Optimized clear value (recommended to use this same value when clearing the rendertarget)
	ClearValue mClearValue;
	/// Pointer to native texture handle if the texture does not own underlying resource
	const void* pNativeHandle;
	/// Debug name used in gpu profile
	const char* pName;
	/// GPU indices to share this texture
	uint32_t* pSharedNodeIndices;
#if defined(VULKAN)
	VkSamplerYcbcrConversionInfo* pVkSamplerYcbcrConversionInfo;
#endif
	/// Texture creation flags (decides memory allocation strategy, sharing access,...)
	TextureCreationFlags mFlags;
	/// Width
	uint32_t mWidth;
	/// Height
	uint32_t mHeight;
	/// Depth (Should be 1 if not a mType is not TEXTURE_TYPE_3D)
	uint32_t mDepth;
	/// Texture array size (Should be 1 if texture is not a texture array or cubemap)
	uint32_t mArraySize;
	/// Number of mip levels
	uint32_t mMipLevels;
	/// Number of multisamples per pixel (currently Textures created with mUsage TEXTURE_USAGE_SAMPLED_IMAGE only support SAMPLE_COUNT_1)
	SampleCount mSampleCount;
	/// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value appropriate for mSampleCount
	uint32_t mSampleQuality;
	///  image format
	TinyImageFormat mFormat;
	/// What state will the texture get created in
	ResourceState mStartState;
	/// Descriptor creation
	DescriptorType mDescriptors;
	/// Number of GPUs to share this texture
	uint32_t mSharedNodeIndexCount;
	/// GPU which will own this texture
	uint32_t mNodeIndex;
} TextureDesc;

// Virtual texture page as a part of the partially resident texture
// Contains memory bindings, offsets and status information
struct VirtualTexturePage
{
	/// Miplevel for this page
	uint32_t mipLevel;
	/// Array layer for this page
	uint32_t layer;
	/// Index for this page
	uint32_t index;
#if defined(USE_MULTIPLE_RENDER_APIS)
	union
	{
#endif
#if defined(DIRECT3D12)
		struct
		{
			/// Allocation and resource for this tile
			D3D12MA::Allocation* pAllocation;
			/// Offset for this page
			D3D12_TILED_RESOURCE_COORDINATE offset;
			/// Size for this page
			D3D12_TILED_RESOURCE_COORDINATE extent;
			/// Byte size for this page
			uint32_t size;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			/// Allocation and resource for this tile
			void* pAllocation;
			/// Sparse image memory bind for this page
			VkSparseImageMemoryBind imageMemoryBind;
			/// Byte size for this page
			VkDeviceSize size;
		} mVulkan;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
	};
#endif
};

typedef struct VirtualTexture
{
#if defined(USE_MULTIPLE_RENDER_APIS)
	union
	{
#endif
#if defined(DIRECT3D12)
		struct
		{
			/// Cached allocated array that is required to interact with d3d12
			uint32_t* pCachedTileCounts;
			/// Pending allocation deletions
			D3D12MA::Allocation** pPendingDeletedAllocations;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			/// GPU memory pool for tiles
			void* pPool;
			/// Sparse image memory bindings of all memory-backed virtual tables
			VkSparseImageMemoryBind* pSparseImageMemoryBinds;
			/// Sparse opaque memory bindings for the mip tail (if present)
			VkSparseMemoryBind* pOpaqueMemoryBinds;
			/// GPU allocations for opaque memory binds (mip tail)
			void** pOpaqueMemoryBindAllocations;
			/// Pending allocation deletions
			void** pPendingDeletedAllocations;
			/// Memory type bits for Sparse texture's memory
			uint32_t mSparseMemoryTypeBits;
			/// Number of opaque memory binds
			uint32_t mOpaqueMemoryBindsCount;
		} mVulkan;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
	};
#endif
	/// Virtual Texture members
	VirtualTexturePage* pPages;
	/// Pending intermediate buffer deletions
	Buffer** pPendingDeletedBuffers;
	/// Pending intermediate buffer deletions count
	uint32_t* pPendingDeletedBuffersCount;
	/// Pending allocation deletions count
	uint32_t* pPendingDeletedAllocationsCount;
	/// Readback buffer, must be filled by app. Size = mReadbackBufferSize * imagecount
	Buffer*  pReadbackBuffer;
	/// Original Pixel image data
	void*    pVirtualImageData;
	///  Total pages count
	uint32_t mVirtualPageTotalCount;
	///  Alive pages count
	uint32_t mVirtualPageAliveCount;
	/// Size of the readback buffer per image
	uint32_t mReadbackBufferSize;
	/// Size of the readback buffer per image
	uint32_t mPageVisibilityBufferSize;
	/// Sparse Virtual Texture Width
	uint16_t mSparseVirtualTexturePageWidth;
	/// Sparse Virtual Texture Height
	uint16_t mSparseVirtualTexturePageHeight;
	/// Number of mip levels that are tiled
	uint8_t mTiledMipLevelCount;
	/// Size of the pending deletion arrays in image count (highest currentImage + 1)
	uint8_t mPendingDeletionCount;
} VirtualTexture;

typedef struct DEFINE_ALIGNED(Texture, 64)
{
	union
	{
#if defined(DIRECT3D12)
		struct
		{
			/// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to TEXTURE_USAGE_SAMPLED_IMAGE)
			DxDescriptorID                mDescriptors;
			/// Native handle of the underlying resource
			ID3D12Resource*             pDxResource;
			/// Contains resource allocation info such as parent heap, offset in heap
			D3D12MA::Allocation*        pDxAllocation;
			uint32_t                    mHandleCount : 24;
			uint32_t                    mUavStartIndex;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			/// Opaque handle used by shaders for doing read/write operations on the texture
			VkImageView pVkSRVDescriptor;
			/// Opaque handle used by shaders for doing read/write operations on the texture
			VkImageView* pVkUAVDescriptors;
			/// Opaque handle used by shaders for doing read/write operations on the texture
			VkImageView pVkSRVStencilDescriptor;
			/// Native handle of the underlying resource
			VkImage pVkImage;
			union
			{
				/// Contains resource allocation info such as parent heap, offset in heap
				struct VmaAllocation_T* pVkAllocation;
				VkDeviceMemory          pVkDeviceMemory;
			};
		} mVulkan;
#endif
#if defined(METAL)
		struct
		{
			struct VmaAllocation_T* pAllocation;
			/// Native handle of the underlying resource
			id<MTLTexture> mtlTexture;
			union
			{
				id<MTLTexture> __strong* pMtlUAVDescriptors;
				id<MTLTexture> mtlStencilTexture;
			};
			id mpsTextureAllocator;
			uint32_t                 mtlPixelFormat;
			uint32_t                 mRT : 1;
		};
#endif
#if defined(DIRECT3D11)
		struct
		{
			ID3D11Resource*             pDxResource;
			ID3D11ShaderResourceView*   pDxSRVDescriptor;
			ID3D11UnorderedAccessView** pDxUAVDescriptors;
			uint64_t                    mPadA;
			uint64_t                    mPadB;
		} mD3D11;
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
	};
	VirtualTexture* pSvt;
	/// Current state of the buffer
	uint32_t mWidth : 16;
	uint32_t mHeight : 16;
	uint32_t mDepth : 16;
	uint32_t mMipLevels : 5;
	uint32_t mArraySizeMinusOne : 11;
	uint32_t mFormat : 8;
	/// Flags specifying which aspects (COLOR,DEPTH,STENCIL) are included in the pVkImageView
	uint32_t mAspectMask : 4;
	uint32_t mNodeIndex : 4;
	uint32_t mSampleCount : 5;
	uint32_t mUav : 1;
	/// This value will be false if the underlying resource is not owned by the texture (swapchain textures,...)
	uint32_t mOwnsImage : 1;
	// Only applies to Vulkan but kept here as adding it inside mVulkan block increases the size of the struct and triggers assert below
	uint32_t mLazilyAllocated : 1;
} Texture;
// One cache line
COMPILE_ASSERT(sizeof(Texture) == 8 * sizeof(uint64_t));

typedef struct RenderTargetDesc
{
	/// Texture creation flags (decides memory allocation strategy, sharing access,...)
	TextureCreationFlags mFlags;
	/// Width
	uint32_t mWidth;
	/// Height
	uint32_t mHeight;
	/// Depth (Should be 1 if not a mType is not TEXTURE_TYPE_3D)
	uint32_t mDepth;
	/// Texture array size (Should be 1 if texture is not a texture array or cubemap)
	uint32_t mArraySize;
	/// Number of mip levels
	uint32_t mMipLevels;
	/// MSAA
	SampleCount mSampleCount;
	/// Internal image format
	TinyImageFormat mFormat;
	/// What state will the texture get created in
	ResourceState mStartState;
	/// Optimized clear value (recommended to use this same value when clearing the rendertarget)
	ClearValue mClearValue;
	/// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value appropriate for mSampleCount
	uint32_t mSampleQuality;
	/// Descriptor creation
	DescriptorType mDescriptors;
	const void*    pNativeHandle;
	/// Debug name used in gpu profile
	const char* pName;
	/// GPU indices to share this texture
	uint32_t* pSharedNodeIndices;
	/// Number of GPUs to share this texture
	uint32_t mSharedNodeIndexCount;
	/// GPU which will own this texture
	uint32_t mNodeIndex;
} RenderTargetDesc;

typedef struct DEFINE_ALIGNED(RenderTarget, 64)
{
	Texture* pTexture;
	union
	{
#if defined(DIRECT3D12)
		struct
		{
			DxDescriptorID              mDescriptors;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			VkImageView   pVkDescriptor;
			VkImageView*  pVkSliceDescriptors;
			uint32_t      mId;
		} mVulkan;
#endif
#if defined(METAL)
		struct
		{
			uint64_t mPad;
		};
#endif
#if defined(DIRECT3D11)
		struct
		{
			union
			{
				/// Resources
				ID3D11RenderTargetView* pDxRtvDescriptor;
				ID3D11DepthStencilView* pDxDsvDescriptor;
			};
			union
			{
				/// Resources
				ID3D11RenderTargetView** pDxRtvSliceDescriptors;
				ID3D11DepthStencilView** pDxDsvSliceDescriptors;
			};
			uint64_t mPadA;
		} mD3D11;
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
	};
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
	RenderTarget*   pResolveAttachment;
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

typedef struct LoadActionsDesc
{
	LoadActionType  mLoadActionsColor[MAX_RENDER_TARGET_ATTACHMENTS];
	LoadActionType  mLoadActionDepth;
	LoadActionType  mLoadActionStencil;
	ClearValue      mClearColorValues[MAX_RENDER_TARGET_ATTACHMENTS];
	ClearValue      mClearDepth;
	StoreActionType mStoreActionsColor[MAX_RENDER_TARGET_ATTACHMENTS];
	StoreActionType mStoreActionDepth;
	StoreActionType mStoreActionStencil;
} LoadActionsDesc;

typedef struct SamplerDesc
{
	FilterType  mMinFilter;
	FilterType  mMagFilter;
	MipMapMode  mMipMapMode;
	AddressMode mAddressU;
	AddressMode mAddressV;
	AddressMode mAddressW;
	float       mMipLodBias;
	bool		mSetLodRange;
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
	union
	{
#if defined(DIRECT3D12)
		struct
		{
			/// Description for creating the Sampler descriptor for this sampler
			D3D12_SAMPLER_DESC mDesc;
			/// Descriptor handle of the Sampler in a CPU visible descriptor heap
			DxDescriptorID     mDescriptor;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			/// Native handle of the underlying resource
			VkSampler                    pVkSampler;
			VkSamplerYcbcrConversion     pVkSamplerYcbcrConversion;
			VkSamplerYcbcrConversionInfo mVkSamplerYcbcrConversionInfo;
		} mVulkan;
#endif
#if defined(METAL)
		struct
		{
			/// Native handle of the underlying resource
			id<MTLSamplerState> mtlSamplerState;
		};
#endif
#if defined(DIRECT3D11)
		struct
		{
			/// Native handle of the underlying resource
			ID3D11SamplerState* pSamplerState;
		} mD3D11;
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
	};
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
	uint32_t    mType;
	uint32_t    mDim : 4;
	uint32_t    mRootDescriptor : 1;
	uint32_t    mStaticSampler : 1;
	uint32_t    mUpdateFrequency : 3;
	uint32_t    mSize;
	uint32_t    mHandleIndex;
	union
	{
#if defined(DIRECT3D12)
		struct
		{
			uint64_t mPadA;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			uint32_t mVkType;
			uint32_t mReg : 20;
			uint32_t mVkStages : 8;
		} mVulkan;
#endif
#if defined(METAL)
		struct
		{
			id<MTLSamplerState> mtlStaticSampler;
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
		} mD3D11;
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
	};
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
	/// Local root signature used mainly in raytracing shaders
	ROOT_SIGNATURE_FLAG_LOCAL_BIT = 0x1,
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
	uint32_t mDescriptorCount;
	/// Graphics or Compute
	PipelineType mPipelineType;
	/// Array of all descriptors declared in the root signature layout
	DescriptorInfo* pDescriptors;
	/// Translates hash of descriptor name to descriptor index in pDescriptors array
	DescriptorIndexMap* pDescriptorNameToIndexMap;
	union
	{
#if defined(DIRECT3D12)
		struct
		{
			ID3D12RootSignature* pDxRootSignature;
			uint8_t              mDxViewDescriptorTableRootIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
			uint8_t              mDxSamplerDescriptorTableRootIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
			uint32_t             mDxCumulativeViewDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
			uint32_t             mDxCumulativeSamplerDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
			uint16_t             mDxViewDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
			uint16_t             mDxSamplerDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
			uint64_t             mPadA;
			uint64_t             mPadB;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			VkPipelineLayout            pPipelineLayout;
			VkDescriptorSetLayout       mVkDescriptorSetLayouts[DESCRIPTOR_UPDATE_FREQ_COUNT];
			uint8_t                     mVkDynamicDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
			VkDescriptorPoolSize        mPoolSizes[DESCRIPTOR_UPDATE_FREQ_COUNT][MAX_DESCRIPTOR_POOL_SIZE_ARRAY_COUNT];
			uint8_t                     mPoolSizeCount[DESCRIPTOR_UPDATE_FREQ_COUNT];
			VkDescriptorPool            pEmptyDescriptorPool[DESCRIPTOR_UPDATE_FREQ_COUNT];
			VkDescriptorSet             pEmptyDescriptorSet[DESCRIPTOR_UPDATE_FREQ_COUNT];
		} mVulkan;
#endif
#if defined(METAL)
		struct
		{
			NSMutableArray<MTLArgumentDescriptor*>*
					 mArgumentDescriptors[DESCRIPTOR_UPDATE_FREQ_COUNT] API_AVAILABLE(macos(10.13), ios(11.0));
			uint32_t mRootTextureCount : 10;
			uint32_t mRootBufferCount : 10;
			uint32_t mRootSamplerCount : 10;
		};
#endif
#if defined(DIRECT3D11)
		struct
		{
			ID3D11SamplerState** ppStaticSamplers;
			uint32_t*            pStaticSamplerSlots;
			ShaderStage*         pStaticSamplerStages;
			uint32_t             mStaticSamplerCount;
			uint32_t             mSrvCount : 10;
			uint32_t             mUavCount : 10;
			uint32_t             mCbvCount : 10;
			uint32_t             mSamplerCount : 10;
			uint32_t             mDynamicCbvCount : 10;
			uint32_t             mPadA;
		} mD3D11;
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
	};

#ifdef ENABLE_DEPENDENCY_TRACKER
	ShaderDependencyNode* pShaderDependencies;
	PipelineDependencyNode* pPipelineDependencies;
#endif

} RootSignature;
#if defined(VULKAN)
COMPILE_ASSERT(sizeof(RootSignature) <= 72 * sizeof(uint64_t));
#elif defined(ORBIS) || defined(PROSPERO) || defined(DIRECT3D12) || defined(ENABLE_DEPENDENCY_TRACKER)
// 2 cache lines
COMPILE_ASSERT(sizeof(RootSignature) <= 16 * sizeof(uint64_t));
#else
// 1 cache line
COMPILE_ASSERT(sizeof(RootSignature) == 8 * sizeof(uint64_t));
#endif

typedef struct DescriptorDataRange
{
	uint32_t mOffset;
	uint32_t mSize;
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
			uint16_t            mUAVMipSlice;
			// Binds entire mip chain as array of UAV
			bool                mBindMipChain;
		};
		struct
		{
			// Bind MTLIndirectCommandBuffer along with the MTLBuffer
			const char*         pICBName;
			uint32_t            mICBIndex;
			bool                mBindICB;
		};
	};
	/// Array of resources containing descriptor handles or constant to be used in ring buffer memory - DescriptorRange can hold only one resource type array
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
	union
	{
#if defined(DIRECT3D12)
		struct
		{
			/// Start handle to cbv srv uav descriptor table
			DxDescriptorID             mCbvSrvUavHandle;
			/// Start handle to sampler descriptor table
			DxDescriptorID             mSamplerHandle;
			/// Stride of the cbv srv uav descriptor table (number of descriptors * descriptor size)
			uint32_t                   mCbvSrvUavStride;
			/// Stride of the sampler descriptor table (number of descriptors * descriptor size)
			uint32_t                   mSamplerStride;
			const RootSignature*       pRootSignature;
			uint32_t                   mMaxSets : 16;
			uint32_t                   mUpdateFrequency : 3;
			uint32_t                   mNodeIndex : 4;
			uint32_t                   mCbvSrvUavRootIndex : 4;
			uint32_t                   mSamplerRootIndex : 4;
			uint32_t                   mPipelineType : 3;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			VkDescriptorSet*             pHandles;
			const RootSignature*         pRootSignature;
			struct DynamicUniformData*   pDynamicUniformData;
			VkDescriptorPool             pDescriptorPool;
			uint32_t                     mMaxSets;
			uint8_t                      mDynamicOffsetCount;
			uint8_t                      mUpdateFrequency;
			uint8_t                      mNodeIndex;
			uint8_t                      mPadA;
		} mVulkan;
#endif
#if defined(METAL)
		struct
		{
			id<MTLArgumentEncoder>         mArgumentEncoder API_AVAILABLE(macos(10.13), ios(11.0));
			Buffer*                        mArgumentBuffer API_AVAILABLE(macos(10.13), ios(11.0));
			struct UntrackedResourceData** ppUntrackedData;
			const RootSignature*           pRootSignature;
			/// Descriptors that are bound without argument buffers
			/// This is necessary since there are argument buffers bugs in some iOS Metal drivers which causes shader compiler crashes or incorrect shader generation. This makes it necessary to keep fallback descriptor binding path alive
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
		} mD3D11;
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
	};
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
		ID3D12CommandAllocator* pDxCmdAlloc;
#endif
#if defined(VULKAN)
		VkCommandPool pVkCmdPool;
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
	union
	{
#if defined(DIRECT3D12)
		struct
		{
#if defined(XBOX)
			DmaCmd mDma;
#endif
			ID3D12GraphicsCommandList* pDxCmdList;

			// Cached in beginCmd to avoid fetching them during rendering
			struct DescriptorHeap*      pBoundHeaps[2];
			D3D12_GPU_DESCRIPTOR_HANDLE mBoundHeapStartHandles[2];

			// Command buffer state
			const ID3D12RootSignature* pBoundRootSignature;
			DescriptorSet*             pBoundDescriptorSets[DESCRIPTOR_UPDATE_FREQ_COUNT];
			uint16_t                   mBoundDescriptorSetIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
			uint32_t                   mNodeIndex : 4;
			uint32_t                   mType : 3;
			CmdPool*                   pCmdPool;
#if !defined(XBOX)
			uint64_t mPadB;
#endif
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			VkCommandBuffer  pVkCmdBuf;
			VkRenderPass     pVkActiveRenderPass;
			VkPipelineLayout pBoundPipelineLayout;
			CmdPool*         pCmdPool;
			uint32_t         mNodeIndex : 4;
			uint32_t         mType : 3;
		} mVulkan;
#endif
#if defined(METAL)
		struct
		{
			id<MTLCommandBuffer>         mtlCommandBuffer;
			id<MTLRenderCommandEncoder>  mtlRenderEncoder;
			id<MTLComputeCommandEncoder> mtlComputeEncoder;
			id<MTLBlitCommandEncoder>    mtlBlitEncoder;
			// Stored in cmdBindPipeline. Used in
			// - cmdDraw functions to check for tessellation and patch control point count
			// - cmdDispatch functions to check for num threads per group (Metal needs to specify numThreadsPerThreadGroup explicitly)
			Pipeline*                    pBoundPipeline;
			// To store the begin-end timestamp for this command buffer
			QueryPool*                   pLastFrameQuery;
			// Stored in cmdBindIndexBuffer and used in cmdDrawIndexed functions (no bindIndexBuffer in Metal)
			NOREFS id<MTLBuffer>         mBoundIndexBuffer;
			// Stored in cmdBindIndexBuffer and used in cmdDrawIndexed functions (no bindIndexBuffer in Metal)
			uint32_t                     mBoundIndexBufferOffset;
			// Stored in cmdBindIndexBuffer and used in cmdDrawIndexed functions (no bindIndexBuffer in Metal)
			uint32_t                     mIndexType : 2;
			// Stored in cmdBindIndexBuffer and used in cmdDrawIndexed functions (no bindIndexBuffer in Metal)
			uint32_t                     mIndexStride : 3;
			// Stored in cmdBindPipeline and used in all draw functions (primitive type does not go in PSO but specified in the draw call)
			uint32_t                     mSelectedPrimitiveType : 4;
			uint32_t                     mPipelineType : 3;
#ifdef ENABLE_DRAW_INDEX_BASE_VERTEX_FALLBACK
			// When first vertex is not supported for indexed draw, we have to offset the
			// vertex buffer manually using setVertexBufferOffset
			// mOffsets, mStrides stored in cmdBindVertexBuffer and used in cmdDrawIndexed functions
			uint32_t                     mOffsets[MAX_VERTEX_BINDINGS];
			uint32_t                     mStrides[MAX_VERTEX_BINDINGS];
			uint32_t                     mFirstVertex;
#endif
		};
#endif
#if defined(DIRECT3D11)
		struct
		{
			ID3D11Buffer* pRootConstantBuffer;
			uint64_t      mPadB[10];
		} mD3D11;
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
	};
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
	union
	{
#if defined(DIRECT3D12)
		struct
		{
			ID3D12Fence* pDxFence;
			HANDLE       pDxWaitIdleFenceEvent;
			uint64_t     mFenceValue;
			uint64_t     mPadA;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			VkFence  pVkFence;
			uint32_t mSubmitted : 1;
			uint32_t mPadA;
			uint64_t mPadB;
			uint64_t mPadC;
		} mVulkan;
#endif
#if defined(METAL)
		struct
		{
			dispatch_semaphore_t pMtlSemaphore;
			uint32_t             mSubmitted : 1;
			uint32_t             mPadA;
			uint64_t             mPadB;
			uint64_t             mPadC;
		};
#endif
#if defined(DIRECT3D11)
		struct
		{
			ID3D11Query* pDX11Query;
			uint32_t     mSubmitted : 1;
			uint32_t     mPadA;
			uint64_t     mPadB;
			uint64_t     mPadC;
		} mD3D11;
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
	};
} Fence;

typedef struct Semaphore
{
	union
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
			ID3D12Fence* pDxFence;
			HANDLE       pDxWaitIdleFenceEvent;
			uint64_t     mFenceValue;
			uint64_t     mPadA;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			VkSemaphore pVkSemaphore;
			uint32_t    mCurrentNodeIndex : 5;
			uint32_t    mSignaled : 1;
			uint32_t    mPadA;
			uint64_t    mPadB;
			uint64_t    mPadC;
		} mVulkan;
#endif
#if defined(METAL)
		struct
		{
			id<MTLEvent> pMtlSemaphore API_AVAILABLE(macos(10.14), ios(12.0));
			uint32_t                   mSignaled;
			uint32_t                   mPadA;
			uint64_t                   mPadB;
		};
#endif
#if defined(GLES)
		struct
		{
			uint32_t mSignaled : 1;
		} mGLES;
#endif
#if defined(ORBIS)
		OrbisFence mStruct;
#endif
#if defined(PROSPERO)
		ProsperoSemaphore mStruct;
#endif
	};
} Semaphore;

typedef struct QueueDesc
{
	QueueType     mType;
	QueueFlag     mFlag;
	QueuePriority mPriority;
	uint32_t      mNodeIndex;
} QueueDesc;

typedef struct Queue
{
	union
	{
#if defined(DIRECT3D12)
		struct
		{
			ID3D12CommandQueue* pDxQueue;
			Fence*              pFence;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			VkQueue  pVkQueue;
			Mutex*   pSubmitMutex;
			uint32_t mFlags;
			float    mTimestampPeriod;
			uint32_t mVkQueueFamilyIndex : 5;
			uint32_t mVkQueueIndex : 5;
			uint32_t mGpuMode : 3;
		} mVulkan;
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
			id<MTLCommandQueue> mtlCommandQueue;
			id<MTLFence> mtlQueueFence API_AVAILABLE(macos(10.13), ios(10.0));
			uint32_t                   mBarrierFlags;
			uint32_t                   mPadB;
		};
#endif
#if defined(DIRECT3D11)
		struct
		{
			ID3D11Device*        pDxDevice;
			ID3D11DeviceContext* pDxContext;
			Fence*               pFence;
		} mD3D11;
#endif
#if defined(ORBIS)
		OrbisQueue mStruct;
#endif
#if defined(PROSPERO)
		ProsperoQueue mStruct;
#endif
	};
	uint32_t mType : 3;
	uint32_t mNodeIndex : 4;
} Queue;

typedef struct ShaderMacro
{
	const char* definition;
	const char* value;
} ShaderMacro;

/// ShaderConstant only supported by Vulkan and Metal APIs
typedef struct ShaderConstant
{
	const void*    pValue;
	uint32_t       mIndex;
	uint32_t       mSize;
} ShaderConstant;

#if defined(TARGET_IOS)
typedef struct ShaderStageDesc
{
	const char*  pName;
	const char*  pCode;
	const char*  pEntryPoint;
	ShaderMacro* pMacros;
	uint32_t     mMacroCount;
} ShaderStageDesc;

typedef struct ShaderDesc
{
	ShaderStage           mStages;
	ShaderStageDesc       mVert;
	ShaderStageDesc       mFrag;
	ShaderStageDesc       mGeom;
	ShaderStageDesc       mHull;
	ShaderStageDesc       mDomain;
	ShaderStageDesc       mComp;
	const ShaderConstant* pConstants;
	uint32_t              mConstantCount;
} ShaderDesc;
#endif

typedef struct BinaryShaderStageDesc
{
#if defined(PROSPERO)
	ProsperoBinaryShaderStageDesc mStruct;
#else
	/// Byte code array
	void*       pByteCode;
	uint32_t    mByteCodeSize;
	const char* pEntryPoint;
#if defined(METAL)
	// Shader source is needed for reflection
	char*    pSource;
	uint32_t mSourceSize;
#endif
#if defined(GLES)
	GLuint   mShader;
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
} BinaryShaderDesc;

typedef struct Shader
{
	ShaderStage mStages : 31;
    bool        mIsMultiviewVR : 1;
	uint32_t    mNumThreadsPerGroup[3];
	union
	{
#if defined(DIRECT3D12)
		struct
		{
			IDxcBlobEncoding** pShaderBlobs;
			LPCWSTR*           pEntryNames;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			VkShaderModule*       pShaderModules;
			char**                pEntryNames;
			VkSpecializationInfo* pSpecializationInfo;
		} mVulkan;
#endif
#if defined(METAL)
		struct
		{
			id<MTLFunction> mtlVertexShader;
			id<MTLFunction> mtlFragmentShader;
			id<MTLFunction> mtlComputeShader;
			id<MTLLibrary>  mtlLibrary;
			char**          pEntryNames;
			uint32_t        mTessellation : 1;
		};
#endif
#if defined(DIRECT3D11)
		struct
		{
			union
			{
				struct
				{
					ID3D11VertexShader*   pDxVertexShader;
					ID3D11PixelShader*    pDxPixelShader;
					ID3D11GeometryShader* pDxGeometryShader;
					ID3D11DomainShader*   pDxDomainShader;
					ID3D11HullShader*     pDxHullShader;
				};
				ID3D11ComputeShader* pDxComputeShader;
			};
			ID3DBlob* pDxInputSignature;
		} mD3D11;
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
	};
	PipelineReflection* pReflection;

#ifdef ENABLE_DEPENDENCY_TRACKER
	RootSignatureDependencyNode* pRootSignatureDependencies;
#endif

} Shader;

typedef struct BlendStateDesc
{
	/// Source blend factor per render target.
	BlendConstant mSrcFactors[MAX_RENDER_TARGET_ATTACHMENTS];
	/// Destination blend factor per render target.
	BlendConstant mDstFactors[MAX_RENDER_TARGET_ATTACHMENTS];
	/// Source alpha blend factor per render target.
	BlendConstant mSrcAlphaFactors[MAX_RENDER_TARGET_ATTACHMENTS];
	/// Destination alpha blend factor per render target.
	BlendConstant mDstAlphaFactors[MAX_RENDER_TARGET_ATTACHMENTS];
	/// Blend mode per render target.
	BlendMode mBlendModes[MAX_RENDER_TARGET_ATTACHMENTS];
	/// Alpha blend mode per render target.
	BlendMode mBlendAlphaModes[MAX_RENDER_TARGET_ATTACHMENTS];
	/// Write mask per render target.
	int32_t mMasks[MAX_RENDER_TARGET_ATTACHMENTS];
	/// Mask that identifies the render targets affected by the blend state.
	BlendStateTargets mRenderTargetMask;
	/// Set whether alpha to coverage should be enabled.
	bool mAlphaToCoverage;
	/// Set whether each render target has an unique blend function. When false the blend function in slot 0 will be used for all render targets.
	bool mIndependentBlend;
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

typedef enum VertexAttribRate
{
	VERTEX_ATTRIB_RATE_VERTEX = 0,
	VERTEX_ATTRIB_RATE_INSTANCE = 1,
	VERTEX_ATTRIB_RATE_COUNT,
} VertexAttribRate;

typedef struct VertexAttrib
{
	ShaderSemantic   mSemantic;
	uint32_t         mSemanticNameLength;
	char             mSemanticName[MAX_SEMANTIC_NAME_LENGTH];
	TinyImageFormat  mFormat;
	uint32_t         mBinding;
	uint32_t         mLocation;
	uint32_t         mOffset;
	VertexAttribRate mRate;
} VertexAttrib;

typedef struct VertexLayout
{
	uint32_t     mAttribCount;
	VertexAttrib mAttribs[MAX_VERTEX_ATTRIBS];
	uint32_t     mStrides[MAX_VERTEX_BINDINGS];
} VertexLayout;

/************************************************************************/
// Defines which shaders will be used by this hit group
// #pIntersectionShaderName - Name of shader used to test intersection with ray
//	  This will be NULL as long as user does not specify ACCELERATION_STRUCTURE_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS
// #pAnyHitShaderName - Name of shader executed when a ray hits
//	  This will be NULL if user specifies ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE in geometry creation
//	  This shader is usually used for work like alpha testing geometry to see if an intersection is valid
// #pClosestHitShaderName - Name of shader executed for the intersection closest to the ray
//	  This shader will do most of the work like shading
// #pHitGroupName - User defined name of the hit group. Use the same name when creating the shader table
/************************************************************************/
typedef struct RaytracingHitGroup
{
	RootSignature* pRootSignature;
	Shader* pIntersectionShader;
	Shader* pAnyHitShader;
	Shader* pClosestHitShader;
	const char* pHitGroupName;
} RaytracingHitGroup;

/************************************************************************/
// #pGlobalRootSignature - Root Signature used by all shaders in the ppShaders array
// #ppShaders - Array of all shaders which can be called during the raytracing operation
//	  This includes the ray generation shader, all miss, any hit, closest hit shaders
// #pHitGroups - Name of the hit groups which will tell the pipeline about which combination of hit shaders to use
// #mPayloadSize - Size of the payload struct for passing data to and from the shaders.
//	  Example - float4 payload sent by raygen shader which will be filled by miss shader as a skybox color
//				  or by hit shader as shaded color
// #mAttributeSize - Size of the intersection attribute. As long as user uses the default intersection shader
//	  this size is sizeof(float2) which represents the ZW of the barycentric co-ordinates of the intersection
/************************************************************************/
typedef struct RaytracingPipelineDesc
{
	Raytracing*         pRaytracing;
	RootSignature*      pGlobalRootSignature;
	Shader*             pRayGenShader;
	RootSignature*      pRayGenRootSignature;
	Shader**            ppMissShaders;
	RootSignature**     ppMissRootSignatures;
	RaytracingHitGroup* pHitGroups;
	RootSignature*      pEmptyRootSignature;
	unsigned            mMissShaderCount;
	unsigned            mHitGroupCount;
	// #TODO : Remove this after adding shader reflection for raytracing shaders
	unsigned mPayloadSize;
	// #TODO : Remove this after adding shader reflection for raytracing shaders
	unsigned mAttributeSize;
	unsigned mMaxTraceRecursionDepth;
	unsigned mMaxRaysCount;
} RaytracingPipelineDesc;

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
	StoreActionType*     pColorResolveActions;
#endif
	uint32_t             mRenderTargetCount;
	SampleCount          mSampleCount;
	uint32_t             mSampleQuality;
	TinyImageFormat      mDepthStencilFormat;
	PrimitiveTopology    mPrimitiveTopo;
	bool                 mSupportIndirectCommandBuffer;
    bool                 mVRFoveatedRendering;
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
		ComputePipelineDesc    mComputeDesc;
		GraphicsPipelineDesc   mGraphicsDesc;
		RaytracingPipelineDesc mRaytracingDesc;
	};
	PipelineCache* pCache;
	void*          pPipelineExtensions;
	const char*    pName;
	PipelineType   mType;
	uint32_t       mExtensionCount;
} PipelineDesc;

#ifdef METAL
typedef struct RaytracingPipeline RaytracingPipeline;
#endif

typedef struct DEFINE_ALIGNED(Pipeline, 64)
{
	union
	{
#if defined(DIRECT3D12)
		struct
		{
			ID3D12PipelineState* pDxPipelineState;
#ifdef ENABLE_RAYTRACING
			ID3D12StateObject* pDxrPipeline;
#endif
			ID3D12RootSignature*   pRootSignature;
			PipelineType           mType;
			D3D_PRIMITIVE_TOPOLOGY mDxPrimitiveTopology;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			VkPipeline   pVkPipeline;
			PipelineType mType;
			uint32_t     mShaderStageCount;
			//In DX12 this information is stored in ID3D12StateObject.
			//But for Vulkan we need to store it manually
			const char** ppShaderStageNames;
		} mVulkan;
#endif
#if defined(METAL)
		struct
		{
			id<MTLRenderPipelineState>  mtlRenderPipelineState;
			id<MTLComputePipelineState> mtlComputePipelineState;
			id<MTLDepthStencilState>    mtlDepthStencilState;
			RaytracingPipeline*         pRaytracingPipeline;
			union
			{
				// Graphics
				struct
				{
					uint32_t            mCullMode : 3;
					uint32_t            mFillMode : 3;
					uint32_t            mWinding : 3;
					uint32_t            mDepthClipMode : 1;
					uint32_t            mMtlPrimitiveType : 4;
					// Between 0-32
					uint32_t            mPatchControlPointCount : 6;
					uint32_t            mTessellation : 1;
					float               mDepthBias;
					float               mSlopeScale;
				};
				// Compute
				struct
				{
					MTLSize             mNumThreadsPerGroup;
				};
			};
			PipelineType                mType;
		};
#endif
#if defined(DIRECT3D11)
		struct
		{
			ID3D11VertexShader*      pDxVertexShader;
			ID3D11PixelShader*       pDxPixelShader;
			ID3D11GeometryShader*    pDxGeometryShader;
			ID3D11DomainShader*      pDxDomainShader;
			ID3D11HullShader*        pDxHullShader;
			ID3D11ComputeShader*     pDxComputeShader;
			ID3D11InputLayout*       pDxInputLayout;
			ID3D11BlendState*        pBlendState;
			ID3D11DepthStencilState* pDepthState;
			ID3D11RasterizerState*   pRasterizerState;
			PipelineType             mType;
			D3D_PRIMITIVE_TOPOLOGY   mDxPrimitiveTopology;
		} mD3D11;
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
	};

#ifdef ENABLE_DEPENDENCY_TRACKER
	RootSignatureDependencyNode* pRootSignatureDependencies;
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
	void* pData;
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
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			VkPipelineCache pCache;
		} mVulkan;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
	};
#endif
} PipelineCache;

typedef enum SwapChainCreationFlags
{
    SWAP_CHAIN_CREATION_FLAG_NONE = 0x0,
    SWAP_CHAIN_CREATION_FLAG_ENABLE_FOVEATED_RENDERING_VR = 0x1,
} SwapChainCreationFlags;
MAKE_ENUM_FLAG(uint32_t, SwapChainCreationFlags);

typedef struct SwapChainDesc
{
	/// Window handle
	WindowHandle mWindowHandle;
	/// Queues which should be allowed to present
	Queue** ppPresentQueues;
	/// Number of present queues
	uint32_t mPresentQueueCount;
	/// Number of backbuffers in this swapchain
	uint32_t mImageCount;
	/// Width of the swapchain
	uint32_t mWidth;
	/// Height of the swapchain
	uint32_t mHeight;
	/// Color format of the swapchain
	TinyImageFormat mColorFormat;
	/// Clear value
	ClearValue mColorClearValue;
    /// Swapchain creation flags
    SwapChainCreationFlags mFlags;
	/// Set whether swap chain will be presented using vsync
	bool mEnableVsync;
	/// We can toggle to using FLIP model if app desires.
	bool mUseFlipSwapEffect;
} SwapChainDesc;

typedef struct SwapChain
{
	/// Render targets created from the swapchain back buffers
	RenderTarget** ppRenderTargets;
	union
	{
#if defined(DIRECT3D12)
		struct
		{
#if defined(XBOX)
			uint64_t mFramePipelineToken;
			/// Sync interval to specify how interval for vsync
			uint32_t mDxSyncInterval : 3;
			uint32_t mFlags : 10;
			uint32_t mIndex;
			void*    pWindow;
			Queue*   pPresentQueue;
			uint64_t mPadB[5];
#else
			/// Use IDXGISwapChain3 for now since IDXGISwapChain4
			/// isn't supported by older devices.
			IDXGISwapChain3* pDxSwapChain;
			/// Sync interval to specify how interval for vsync
			uint32_t                                 mDxSyncInterval : 3;
			uint32_t                                 mFlags : 10;
			uint32_t                                 mPadA;
			uint64_t                                 mPadB[5];
#endif
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			/// Present queue if one exists (queuePresent will use this queue if the hardware has a dedicated present queue)
			VkQueue        pPresentQueue;
			VkSwapchainKHR pSwapChain;
			VkSurfaceKHR   pVkSurface;
			SwapChainDesc* pDesc;
			uint32_t       mPresentQueueFamilyIndex : 5;
			uint32_t       mPadA;
		} mVulkan;
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
			uint64_t             mPadB[4];
		};
#endif
#if defined(DIRECT3D11)
		struct
		{
			/// Use IDXGISwapChain3 for now since IDXGISwapChain4
			/// isn't supported by older devices.
			IDXGISwapChain* pDxSwapChain;
			/// Sync interval to specify how interval for vsync
			uint32_t         mDxSyncInterval : 3;
			uint32_t         mFlags : 10;
			uint32_t         mImageIndex : 3;
			DXGI_SWAP_EFFECT mSwapEffect;
			uint32_t         mPadA;
			uint64_t         mPadB[5];
		} mD3D11;
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
	};
#if defined(QUEST_VR)
    struct
    {
        struct ovrTextureSwapChain* pSwapChain;
        VkExtent2D* pFragmentDensityTextureSizes;
        RenderTarget** ppFragmentDensityMasks;
    } mVR;
#endif
	uint32_t mImageCount : 3;
	uint32_t mEnableVsync : 1;
} SwapChain;

typedef enum ShaderTarget
{
// We only need SM 5.0 for supporting D3D11 fallback
#if defined(DIRECT3D11)
	shader_target_5_0,
#endif
	// 5.1 is supported on all DX12 hardware
	shader_target_5_1,
	shader_target_6_0,
	shader_target_6_1,
	shader_target_6_2,
	shader_target_6_3,    //required for Raytracing
	shader_target_6_4,    //required for VRS
} ShaderTarget;

typedef enum GpuMode
{
	GPU_MODE_SINGLE = 0,
	GPU_MODE_LINKED,
	GPU_MODE_UNLINKED,
} GpuMode;

typedef struct ExtendedSettings
{
	uint32_t			numSettings = 0;
	uint32_t*			pSettings	= NULL;
	const char**	ppSettingNames = NULL;
} ExtendedSettings;

typedef struct RendererDesc
{
#if defined(USE_MULTIPLE_RENDER_APIS)
	union
	{
#endif
#if defined(DIRECT3D12)
		D3D_FEATURE_LEVEL mDxFeatureLevel;
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
			bool mRequestAllAvailableQueues;
		} mVulkan;
#endif
#if defined(DIRECT3D11)
		struct
		{
			/// Set whether to force feature level 10 for compatibility
			bool mUseDx10;
			/// Set whether to pick the first valid GPU or use our GpuConfig
			bool mUseDefaultGpu;
		} mD3D11;
#endif
#if defined(GLES)
		struct
		{
			const char** ppDeviceExtensions;
			uint32_t     mDeviceExtensionCount;
		} mGLES;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
	};
#endif

	LogFn        pLogFn;
	ShaderTarget mShaderTarget;
	GpuMode      mGpuMode;
	
	/// Apps may want to query additional state for their applications. That information is transferred through here.
	ExtendedSettings* pExtendedSettings;

	/// Required when creating unlinked multiple renderers. Optional otherwise, can be used for explicit GPU selection.
	RendererContext* pContext;
	uint32_t         mGpuIndex;

	/// This results in new validation not possible during API calls on the CPU, by creating patched shaders that have validation added directly to the shader.
	/// However, it can slow things down a lot, especially for applications with numerous PSOs. Time to see the first render frame may take several minutes
	bool mEnableGPUBasedValidation;

	bool mD3D11Supported;
	bool mGLESSupported;
#if defined(VULKAN) && defined(ANDROID)
	bool mPreferVulkan;
#endif
} RendererDesc;

typedef struct GPUVendorPreset
{
	char           mVendorId[MAX_GPU_VENDOR_STRING_LENGTH];
	char           mModelId[MAX_GPU_VENDOR_STRING_LENGTH];
	char           mRevisionId[MAX_GPU_VENDOR_STRING_LENGTH];    // Optional as not all gpu's have that. Default is : 0x00
	GPUPresetLevel mPresetLevel;
	char           mGpuName[MAX_GPU_VENDOR_STRING_LENGTH];    //If GPU Name is missing then value will be empty string
	char           mGpuDriverVersion[MAX_GPU_VENDOR_STRING_LENGTH];
	char           mGpuDriverDate[MAX_GPU_VENDOR_STRING_LENGTH];
	uint32_t       mRTCoresCount;
} GPUVendorPreset;

typedef struct GPUCapBits
{
	bool canShaderReadFrom[TinyImageFormat_Count];
	bool canShaderWriteTo[TinyImageFormat_Count];
	bool canRenderTargetWriteTo[TinyImageFormat_Count];
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

typedef struct GPUSettings
{
	uint64_t            mVRAM;
	uint32_t            mUniformBufferAlignment;
	uint32_t            mUploadBufferTextureAlignment;
	uint32_t            mUploadBufferTextureRowAlignment;
	uint32_t            mMaxVertexInputBindings;
	uint32_t            mMaxRootSignatureDWORDS;
	uint32_t            mWaveLaneCount;
	WaveOpsSupportFlags mWaveOpsSupportFlags;
	GPUVendorPreset     mGpuVendorPreset;
	// Variable Rate Shading
	ShadingRate         mShadingRates;
	ShadingRateCaps     mShadingRateCaps;
	uint32_t            mShadingRateTexelWidth;
	uint32_t            mShadingRateTexelHeight;

	uint32_t            mMultiDrawIndirect : 1;
	uint32_t            mIndirectRootConstant : 1;
	uint32_t            mBuiltinDrawID : 1;
	uint32_t            mIndirectCommandBuffer : 1;
	uint32_t            mROVsSupported : 1;
	uint32_t            mTessellationSupported : 1;
	uint32_t            mGeometryShaderSupported : 1;
	uint32_t            mGpuBreadcrumbs : 1;
	uint32_t            mHDRSupported : 1;
#ifdef VULKAN
	uint32_t            mSamplerAnisotropySupported : 1;
#endif
#ifdef METAL
	uint32_t            mArgumentBufferMaxTextures;
	uint32_t            mHeaps : 1;
	uint32_t            mPlacementHeaps : 1;
	uint32_t            mTessellationIndirectDrawSupported : 1;
#ifdef TARGET_IOS
	/// Whether or not this iOS device can handle vertex-offset drawIndexed calls.
	uint32_t            mDrawIndexVertexOffsetSupported : 1;
#endif
#endif
#if defined(GLES)
	uint32_t            mMaxTextureImageUnits;
#endif
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
			struct DescriptorHeap**   pCPUDescriptorHeaps;
			struct DescriptorHeap**   pCbvSrvUavHeaps;
			struct DescriptorHeap**   pSamplerHeaps;
			class D3D12MA::Allocator* pResourceAllocator;
#if defined(XBOX)
			IDXGIFactory2* pDXGIFactory;
			IDXGIAdapter*  pDxActiveGPU;
			ID3D12Device*  pDxDevice;
			EsramManager*  pESRAMManager;
#elif defined(DIRECT3D12)
			IDXGIFactory6*                           pDXGIFactory;
			IDXGIAdapter4*                           pDxActiveGPU;
			ID3D12Device*                            pDxDevice;
#if defined(_WINDOWS) && defined(DRED)
			ID3D12DeviceRemovedExtendedDataSettings* pDredSettings;
#else
			uint64_t mPadA;
#endif
#endif
			ID3D12Debug* pDXDebug;
#if defined(_WINDOWS)
			ID3D12InfoQueue* pDxDebugValidation;
#endif
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			VkInstance                   pVkInstance;
			VkPhysicalDevice             pVkActiveGPU;
			VkPhysicalDeviceProperties2* pVkActiveGPUProperties;
			VkDevice                     pVkDevice;
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
			VkDebugUtilsMessengerEXT pVkDebugUtilsMessenger;
#else
			VkDebugReportCallbackEXT                 pVkDebugReport;
#endif
			uint32_t**             pAvailableQueueCount;
			uint32_t**             pUsedQueueCount;
			VkDescriptorPool       pEmptyDescriptorPool;
			VkDescriptorSetLayout  pEmptyDescriptorSetLayout;
			VkDescriptorSet        pEmptyDescriptorSet;
			struct VmaAllocator_T* pVmaAllocator;
			uint32_t               mRaytracingSupported : 1;
			uint32_t               mYCbCrExtension : 1;
			uint32_t               mKHRSpirv14Extension : 1;
			uint32_t               mKHRAccelerationStructureExtension : 1;
			uint32_t               mKHRRayTracingPipelineExtension : 1;
			uint32_t               mKHRRayQueryExtension : 1;
			uint32_t               mAMDGCNShaderExtension : 1;
			uint32_t               mAMDDrawIndirectCountExtension : 1;
			uint32_t               mDescriptorIndexingExtension : 1;
			uint32_t               mShaderSampledImageArrayDynamicIndexingSupported : 1;
			uint32_t               mShaderFloatControlsExtension : 1;
			uint32_t               mBufferDeviceAddressExtension : 1;
			uint32_t               mDeferredHostOperationsExtension : 1;
			uint32_t               mDrawIndirectCountExtension : 1;
			uint32_t               mDedicatedAllocationExtension : 1;
			uint32_t               mExternalMemoryExtension : 1;
			uint32_t               mDebugMarkerSupport : 1;
			uint32_t               mOwnInstance : 1;
#if defined(QUEST_VR)
			uint32_t               mMultiviewExtension : 1;
#endif
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
		} mVulkan;
#endif
#if defined(METAL)
		struct
		{
			id<MTLDevice>          pDevice;
			struct VmaAllocator_T* pVmaAllocator;
			NOREFS id<MTLHeap>* pHeaps API_AVAILABLE(macos(10.13), ios(10.0));
			uint32_t                   mHeapCount;
			uint32_t                   mHeapCapacity;
			// To synchronize resource allocation done through automatic heaps
			Mutex*   pHeapMutex;
			uint64_t mPadA[3];
		};
#endif
#if defined(DIRECT3D11)
		struct
		{
			IDXGIFactory1*           pDXGIFactory;
			IDXGIAdapter1*           pDxActiveGPU;
			ID3D11Device*            pDxDevice;
			ID3D11DeviceContext*     pDxContext;
			ID3D11DeviceContext1*    pDxContext1;
			ID3D11BlendState*        pDefaultBlendState;
			ID3D11DepthStencilState* pDefaultDepthState;
			ID3D11RasterizerState*   pDefaultRasterizerState;
			uint32_t                 mPartialUpdateConstantBufferSupported : 1;
			D3D_FEATURE_LEVEL        mFeatureLevel;
#if defined(ENABLE_PERFORMANCE_MARKER)
			ID3DUserDefinedAnnotation* pUserDefinedAnnotation;
#else
			uint64_t                                 mPadB;
#endif
			uint32_t mPadA;
		} mD3D11;
#endif
#if defined(GLES)
		struct
		{
			GLContext pContext;
			GLConfig  pConfig;
		} mGLES;
#endif
#if defined(ORBIS)
		struct
		{
			uint64_t mPadA;
			uint64_t mPadB;
		};
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
	};
#endif

#if defined(ENABLE_NSIGHT_AFTERMATH)
	// GPU crash dump tracker using Nsight Aftermath instrumentation
	AftermathTracker mAftermathTracker;
	bool             mAftermathSupport;
	bool             mDiagnosticsConfigSupport;
	bool             mDiagnosticCheckPointsSupport;
#endif
	struct NullDescriptors* pNullDescriptors;
	char*                   pName;
	GPUSettings*            pActiveGpuSettings;
	GPUCapBits*             pCapBits;
	uint32_t                mLinkedNodeCount : 4;
	uint32_t                mUnlinkedRendererIndex : 4;
	uint32_t                mGpuMode : 3;
	uint32_t                mShaderTarget : 4;
	uint32_t                mEnableGpuBasedValidation : 1;
	char*                   pApiName;
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
		D3D_FEATURE_LEVEL mDxFeatureLevel;
#endif
#if defined(VULKAN)
		struct
		{
			const char** ppInstanceLayers;
			const char** ppInstanceExtensions;
			uint32_t     mInstanceLayerCount;
			uint32_t     mInstanceExtensionCount;
		} mVulkan;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
	};
#endif
	bool mEnableGPUBasedValidation;
} RendererContextDesc;

typedef struct GpuInfo
{
#if defined(USE_MULTIPLE_RENDER_APIS)
	union
	{
#endif
#if defined (DIRECT3D12) && defined(_WINDOWS)
		struct
		{
			IDXGIAdapter4*               pDxGPU;
			D3D_FEATURE_LEVEL            mMaxSupportedFeatureLevel;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			VkPhysicalDevice             pGPU;
			VkPhysicalDeviceProperties2  mGPUProperties;
		} mVulkan;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
	};
#endif
#if defined(METAL)
	id<MTLDevice> pGPU;
#endif
	GPUSettings mSettings;
} GpuInfo;

typedef struct RendererContext
{
#if defined(USE_MULTIPLE_RENDER_APIS)
	union
	{
#endif
#if defined(DIRECT3D12) && defined(_WINDOWS)

		struct
		{
			IDXGIFactory6* pDXGIFactory;
			ID3D12Debug*   pDXDebug;
		} mD3D12;
#endif
#if defined(VULKAN)
		struct
		{
			VkInstance               pVkInstance;
#ifdef ENABLE_DEBUG_UTILS_EXTENSION
			VkDebugUtilsMessengerEXT pVkDebugUtilsMessenger;
#else
			VkDebugReportCallbackEXT pVkDebugReport;
#endif
		} mVulkan;
#endif
#if defined(USE_MULTIPLE_RENDER_APIS)
	};
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
	bool mPacked;
} CommandSignatureDesc;

typedef struct CommandSignature
{
#if defined(DIRECT3D12)
	ID3D12CommandSignature* pDxHandle;
#endif
	IndirectArgumentType    mDrawType;
	uint32_t                mStride;
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

#ifdef __INTELLISENSE__
// IntelliSense is the code completion engine in Visual Studio. When it parses the source files, __INTELLISENSE__ macro is defined.
// Here we trick IntelliSense into thinking that the renderer functions are not function pointers, but just regular functions.
// What this achieves is filtering out duplicated function names from code completion results and improving the code completion for function parameters.
// This dramatically improves the quality of life for Visual Studio users.
#define DECLARE_RENDERER_FUNCTION(ret, name, ...)                     \
	ret name(__VA_ARGS__);
#else
#define DECLARE_RENDERER_FUNCTION(ret, name, ...)                     \
	typedef ret(FORGE_CALLCONV* name##Fn)(__VA_ARGS__); \
	FORGE_RENDERER_API extern name##Fn       name;
#endif

#ifdef ENABLE_DEPENDENCY_TRACKER
FORGE_RENDERER_API void removeShaderDependencies(Shader* pShaderProgram);
FORGE_RENDERER_API void removeRootSignatureDependencies(RootSignature* pRootSignature);
FORGE_RENDERER_API void removePipelineDependencies(Pipeline* pPipeline);
FORGE_RENDERER_API void addShaderDependencies(Shader* pShaderProgram, const BinaryShaderDesc*);
FORGE_RENDERER_API void addRootSignatureDependencies(RootSignature* pRootSignature, const RootSignatureDesc* pRootSignatureDesc);
FORGE_RENDERER_API void addPipelineDependencies(Pipeline* pPipeline, const PipelineDesc* pPipelineDesc);
#else
#define removeShaderDependencies(...)
#define removeRootSignatureDependencies(...)
#define removePipelineDependencies(...)
#define addShaderDependencies(...)
#define addRootSignatureDependencies(...)
#define addPipelineDependencies(...)
#endif

// clang-format off
// API functions

// Multiple renderer API (optional)
FORGE_RENDERER_API void FORGE_CALLCONV initRendererContext(const char* app_name, const RendererContextDesc* p_settings, RendererContext** ppContext);
FORGE_RENDERER_API void FORGE_CALLCONV exitRendererContext(RendererContext* pContext);

// allocates memory and initializes the renderer -> returns pRenderer
//
FORGE_RENDERER_API void FORGE_CALLCONV initRenderer(const char* app_name, const RendererDesc* p_settings, Renderer** pRenderer);
FORGE_RENDERER_API void FORGE_CALLCONV exitRenderer(Renderer* pRenderer);

DECLARE_RENDERER_FUNCTION(void, addFence, Renderer* pRenderer, Fence** p_fence)
DECLARE_RENDERER_FUNCTION(void, removeFence, Renderer* pRenderer, Fence* p_fence)

DECLARE_RENDERER_FUNCTION(void, addSemaphore, Renderer* pRenderer, Semaphore** p_semaphore)
DECLARE_RENDERER_FUNCTION(void, removeSemaphore, Renderer* pRenderer, Semaphore* p_semaphore)

DECLARE_RENDERER_FUNCTION(void, addQueue, Renderer* pRenderer, QueueDesc* pQDesc, Queue** pQueue)
DECLARE_RENDERER_FUNCTION(void, removeQueue, Renderer* pRenderer, Queue* pQueue)

DECLARE_RENDERER_FUNCTION(void, addSwapChain, Renderer* pRenderer, const SwapChainDesc* p_desc, SwapChain** p_swap_chain)
DECLARE_RENDERER_FUNCTION(void, removeSwapChain, Renderer* pRenderer, SwapChain* p_swap_chain)

// command pool functions
DECLARE_RENDERER_FUNCTION(void, addCmdPool, Renderer* pRenderer, const CmdPoolDesc* p_desc, CmdPool** p_cmd_pool)
DECLARE_RENDERER_FUNCTION(void, removeCmdPool, Renderer* pRenderer, CmdPool* p_CmdPool)
DECLARE_RENDERER_FUNCTION(void, addCmd, Renderer* pRenderer, const CmdDesc* p_desc, Cmd** p_cmd)
DECLARE_RENDERER_FUNCTION(void, removeCmd, Renderer* pRenderer, Cmd* pCmd)
DECLARE_RENDERER_FUNCTION(void, addCmd_n, Renderer* pRenderer, const CmdDesc* p_desc, uint32_t cmd_count, Cmd*** p_cmds)
DECLARE_RENDERER_FUNCTION(void, removeCmd_n, Renderer* pRenderer, uint32_t cmd_count, Cmd** p_cmds)

//
// All buffer, texture loading handled by resource system -> IResourceLoader.*
//

DECLARE_RENDERER_FUNCTION(void, addRenderTarget, Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
DECLARE_RENDERER_FUNCTION(void, removeRenderTarget, Renderer* pRenderer, RenderTarget* pRenderTarget)
DECLARE_RENDERER_FUNCTION(void, addSampler, Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** p_sampler)
DECLARE_RENDERER_FUNCTION(void, removeSampler, Renderer* pRenderer, Sampler* p_sampler)

// shader functions
#if defined(TARGET_IOS)
DECLARE_RENDERER_FUNCTION(void, addIosShader, Renderer* pRenderer, const ShaderDesc* p_desc, Shader** p_shader_program);
#endif
DECLARE_RENDERER_FUNCTION(void, addShaderBinary, Renderer* pRenderer, const BinaryShaderDesc* p_desc, Shader** p_shader_program)
DECLARE_RENDERER_FUNCTION(void, removeShader, Renderer* pRenderer, Shader* p_shader_program)

DECLARE_RENDERER_FUNCTION(void, addRootSignature, Renderer* pRenderer, const RootSignatureDesc* pDesc, RootSignature** pRootSignature)
DECLARE_RENDERER_FUNCTION(void, removeRootSignature, Renderer* pRenderer, RootSignature* pRootSignature)

// pipeline functions
DECLARE_RENDERER_FUNCTION(void, addPipeline, Renderer* pRenderer, const PipelineDesc* p_pipeline_settings, Pipeline** p_pipeline)
DECLARE_RENDERER_FUNCTION(void, removePipeline, Renderer* pRenderer, Pipeline* p_pipeline)
DECLARE_RENDERER_FUNCTION(void, addPipelineCache, Renderer* pRenderer, const PipelineCacheDesc* pDesc, PipelineCache** ppPipelineCache)
DECLARE_RENDERER_FUNCTION(void, getPipelineCacheData, Renderer* pRenderer, PipelineCache* pPipelineCache, size_t* pSize, void* pData)
DECLARE_RENDERER_FUNCTION(void, removePipelineCache, Renderer* pRenderer, PipelineCache* pPipelineCache)

// Descriptor Set functions
DECLARE_RENDERER_FUNCTION(void, addDescriptorSet, Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** pDescriptorSet)
DECLARE_RENDERER_FUNCTION(void, removeDescriptorSet, Renderer* pRenderer, DescriptorSet* pDescriptorSet)
DECLARE_RENDERER_FUNCTION(void, updateDescriptorSet, Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)

// command buffer functions
DECLARE_RENDERER_FUNCTION(void, resetCmdPool, Renderer* pRenderer, CmdPool* pCmdPool)
DECLARE_RENDERER_FUNCTION(void, beginCmd, Cmd* p_cmd)
DECLARE_RENDERER_FUNCTION(void, endCmd, Cmd* p_cmd)
DECLARE_RENDERER_FUNCTION(void, cmdBindRenderTargets, Cmd* p_cmd, uint32_t render_target_count, RenderTarget** p_render_targets, RenderTarget* p_depth_stencil, const LoadActionsDesc* loadActions, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice, uint32_t depthMipSlice)
DECLARE_RENDERER_FUNCTION(void, cmdSetShadingRate, Cmd* p_cmd, ShadingRate shading_rate, Texture* p_texture, ShadingRateCombiner post_rasterizer_rate, ShadingRateCombiner final_rate);
DECLARE_RENDERER_FUNCTION(void, cmdSetViewport, Cmd* p_cmd, float x, float y, float width, float height, float min_depth, float max_depth)
DECLARE_RENDERER_FUNCTION(void, cmdSetScissor, Cmd* p_cmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
DECLARE_RENDERER_FUNCTION(void, cmdSetStencilReferenceValue, Cmd* p_cmd, uint32_t val)
DECLARE_RENDERER_FUNCTION(void, cmdBindPipeline, Cmd* p_cmd, Pipeline* p_pipeline)
DECLARE_RENDERER_FUNCTION(void, cmdBindDescriptorSet, Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
DECLARE_RENDERER_FUNCTION(void, cmdBindPushConstants, Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
DECLARE_RENDERER_FUNCTION(void, cmdBindDescriptorSetWithRootCbvs, Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
DECLARE_RENDERER_FUNCTION(void, cmdBindIndexBuffer, Cmd* p_cmd, Buffer* p_buffer, uint32_t indexType, uint64_t offset)
DECLARE_RENDERER_FUNCTION(void, cmdBindVertexBuffer, Cmd* p_cmd, uint32_t buffer_count, Buffer** pp_buffers, const uint32_t* pStrides, const uint64_t* pOffsets)
DECLARE_RENDERER_FUNCTION(void, cmdDraw, Cmd* p_cmd, uint32_t vertex_count, uint32_t first_vertex)
DECLARE_RENDERER_FUNCTION(void, cmdDrawInstanced, Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
DECLARE_RENDERER_FUNCTION(void, cmdDrawIndexed, Cmd* p_cmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex)
DECLARE_RENDERER_FUNCTION(void, cmdDrawIndexedInstanced, Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
DECLARE_RENDERER_FUNCTION(void, cmdDispatch, Cmd* p_cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)

// Transition Commands
DECLARE_RENDERER_FUNCTION(void, cmdResourceBarrier, Cmd* p_cmd, uint32_t buffer_barrier_count, BufferBarrier* p_buffer_barriers, uint32_t texture_barrier_count, TextureBarrier* p_texture_barriers, uint32_t rt_barrier_count, RenderTargetBarrier* p_rt_barriers)

// Virtual Textures
DECLARE_RENDERER_FUNCTION(void, cmdUpdateVirtualTexture, Cmd* pCmd, Texture* pTexture, uint32_t currentImage)

// queue/fence/swapchain functions
DECLARE_RENDERER_FUNCTION(void, acquireNextImage, Renderer* pRenderer, SwapChain* p_swap_chain, Semaphore* p_signal_semaphore, Fence* p_fence, uint32_t* p_image_index)
DECLARE_RENDERER_FUNCTION(void, queueSubmit, Queue* p_queue, const QueueSubmitDesc* p_desc)
DECLARE_RENDERER_FUNCTION(void, queuePresent, Queue* p_queue, const QueuePresentDesc* p_desc)
DECLARE_RENDERER_FUNCTION(void, waitQueueIdle, Queue* p_queue)
DECLARE_RENDERER_FUNCTION(void, getFenceStatus, Renderer* pRenderer, Fence* p_fence, FenceStatus* p_fence_status)
DECLARE_RENDERER_FUNCTION(void, waitForFences, Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
DECLARE_RENDERER_FUNCTION(void, toggleVSync, Renderer* pRenderer, SwapChain** ppSwapchain)

//Returns the recommended format for the swapchain.
//If true is passed for the hintHDR parameter, it will return an HDR format IF the platform supports it
//If false is passed or the platform does not support HDR a non HDR format is returned.
//If true is passed for the hintSrgb parameter, it will return format that is will do gamma correction automatically
//If false is passed for the hintSrgb parameter the gamma correction should be done as a postprocess step before submitting image to swapchain
DECLARE_RENDERER_FUNCTION(TinyImageFormat, getRecommendedSwapchainFormat, bool hintHDR, bool hintSRGB)

//indirect Draw functions
DECLARE_RENDERER_FUNCTION(void, addIndirectCommandSignature, Renderer* pRenderer, const CommandSignatureDesc* p_desc, CommandSignature** ppCommandSignature)
DECLARE_RENDERER_FUNCTION(void, removeIndirectCommandSignature, Renderer* pRenderer, CommandSignature* pCommandSignature)
DECLARE_RENDERER_FUNCTION(void, cmdExecuteIndirect, Cmd* pCmd, CommandSignature* pCommandSignature, unsigned int maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset)

/************************************************************************/
// GPU Query Interface
/************************************************************************/
DECLARE_RENDERER_FUNCTION(void, getTimestampFrequency, Queue* pQueue, double* pFrequency)
DECLARE_RENDERER_FUNCTION(void, addQueryPool, Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
DECLARE_RENDERER_FUNCTION(void, removeQueryPool, Renderer* pRenderer, QueryPool* pQueryPool)
DECLARE_RENDERER_FUNCTION(void, cmdResetQueryPool, Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
DECLARE_RENDERER_FUNCTION(void, cmdBeginQuery, Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
DECLARE_RENDERER_FUNCTION(void, cmdEndQuery, Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
DECLARE_RENDERER_FUNCTION(void, cmdResolveQuery, Cmd* pCmd, QueryPool* pQueryPool, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
/************************************************************************/
// Stats Info Interface
/************************************************************************/
DECLARE_RENDERER_FUNCTION(void, calculateMemoryStats, Renderer* pRenderer, char** stats)
DECLARE_RENDERER_FUNCTION(void, calculateMemoryUse, Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
DECLARE_RENDERER_FUNCTION(void, freeMemoryStats, Renderer* pRenderer, char* stats)
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
FORGE_RENDERER_API uint32_t getDescriptorIndexFromName(const RootSignature* pRootSignature, const char* pName);
// clang-format on

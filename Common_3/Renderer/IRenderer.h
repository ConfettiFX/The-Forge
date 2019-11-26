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

#pragma once

#if defined(DIRECT3D11)
#include <d3d11_1.h>
#include <dxgi1_2.h>
#endif
#if defined(_DURANGO)
#ifndef DIRECT3D12
#define DIRECT3D12
#endif
#include <d3d12_x.h>
#elif defined(DIRECT3D12)
#include <d3d12.h>
#include <dxgi1_5.h>
#include <dxgidebug.h>
#endif
#if defined(VULKAN)
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__ANDROID__)
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#elif defined(__linux__) && !defined(VK_USE_PLATFORM_GGP)
#define VK_USE_PLATFORM_XLIB_KHR    //Use Xlib or Xcb as display server, defaults to Xlib
#endif
#include "../ThirdParty/OpenSource/volk/volk.h"
#endif
#if defined(METAL)
#import <MetalKit/MetalKit.h>
#endif

#if defined(VULKAN)
// Set this define to enable renderdoc layer
// NOTE: Setting this define will disable use of the khr dedicated allocation extension since it conflicts with the renderdoc capture layer
//#define USE_RENDER_DOC

// Raytracing
#ifdef VK_NV_RAY_TRACING_SPEC_VERSION
#define ENABLE_RAYTRACING
#endif
#elif defined(DIRECT3D12)
//#define USE_PIX

// Raytracing
#ifdef D3D12_RAYTRACING_AABB_BYTE_ALIGNMENT
#define ENABLE_RAYTRACING
#endif
#elif defined(METAL)
#define ENABLE_RAYTRACING
#endif

#include "../OS/Interfaces/IOperatingSystem.h"
#include "../OS/Interfaces/IThread.h"
#include "../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"

#ifdef __cplusplus
#ifndef MAKE_ENUM_FLAG
#define MAKE_ENUM_FLAG(TYPE, ENUM_TYPE)                                                                        \
	static inline ENUM_TYPE operator|(ENUM_TYPE a, ENUM_TYPE b) { return (ENUM_TYPE)((TYPE)(a) | (TYPE)(b)); } \
	static inline ENUM_TYPE operator&(ENUM_TYPE a, ENUM_TYPE b) { return (ENUM_TYPE)((TYPE)(a) & (TYPE)(b)); } \
	static inline ENUM_TYPE operator|=(ENUM_TYPE& a, ENUM_TYPE b) { return a = (a | b); }                      \
	static inline ENUM_TYPE operator&=(ENUM_TYPE& a, ENUM_TYPE b) { return a = (a & b); }

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
	MAX_GPUS = 10,
	MAX_RENDER_TARGET_ATTACHMENTS = 8,
	MAX_SUBMIT_CMDS = 20,    // max number of command lists / command buffers
	MAX_SUBMIT_WAIT_SEMAPHORES = 8,
	MAX_SUBMIT_SIGNAL_SEMAPHORES = 8,
	MAX_PRESENT_WAIT_SEMAPHORES = 8,
	MAX_VERTEX_BINDINGS = 15,
	MAX_VERTEX_ATTRIBS = 15,
	MAX_SEMANTIC_NAME_LENGTH = 128,
	MAX_MIP_LEVELS = 0xFFFFFFFF,
	MAX_GPU_VENDOR_STRING_LENGTH = 64    //max size for GPUVendorPreset strings
};
#endif

typedef enum RendererApi
{
	RENDERER_API_D3D12 = 0,
	RENDERER_API_VULKAN,
	RENDERER_API_METAL,
	RENDERER_API_XBOX_D3D12,
	RENDERER_API_D3D11
} RendererApi;

typedef enum LogType
{
	LOG_TYPE_INFO = 0,
	LOG_TYPE_WARN,
	LOG_TYPE_DEBUG,
	LOG_TYPE_ERROR
} LogType;

typedef enum CmdPoolType
{
	CMD_POOL_DIRECT,
	CMD_POOL_BUNDLE,
	CMD_POOL_COPY,
	CMD_POOL_COMPUTE,
	MAX_CMD_TYPE
} CmdPoolType;

typedef enum QueueFlag
{
	QUEUE_FLAG_NONE,
	QUEUE_FLAG_DISABLE_GPU_TIMEOUT,
	QUEUE_FLAG_INIT_MICROPROFILE,
	MAX_QUEUE_FLAG
} QueueFlag;

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

typedef void(*LogFn)(LogType, const char*, const char*);

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
	RESOURCE_STATE_SHADER_RESOURCE = 0x40 | 0x80,
	RESOURCE_STATE_STREAM_OUT = 0x100,
	RESOURCE_STATE_INDIRECT_ARGUMENT = 0x200,
	RESOURCE_STATE_COPY_DEST = 0x400,
	RESOURCE_STATE_COPY_SOURCE = 0x800,
	RESOURCE_STATE_GENERIC_READ = (((((0x1 | 0x2) | 0x40) | 0x80) | 0x200) | 0x800),
	RESOURCE_STATE_PRESENT = 0x4000,
	RESOURCE_STATE_COMMON = 0x8000,
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
	RESOURCE_MEMORY_USAGE_MAX_ENUM = 0x7FFFFFFF
} ResourceMemoryUsage;

// Forward declarations
typedef struct Renderer              Renderer;
typedef struct Queue                 Queue;
typedef struct Pipeline              Pipeline;
typedef struct BlendState            BlendState;
typedef struct Buffer                Buffer;
typedef struct Texture               Texture;
typedef struct ShaderReflectionInfo  ShaderReflectionInfo;
typedef struct Shader                Shader;
typedef struct DescriptorSet         DescriptorSet;
typedef struct DescriptorIndexMap    DescriptorIndexMap;
typedef struct ShaderDescriptors     ShaderDescriptors;

// Raytracing
typedef struct Raytracing            Raytracing;
typedef struct RaytracingHitGroup    RaytracingHitGroup;
typedef struct AccelerationStructure AccelerationStructure;

typedef struct EsramManager          EsramManager;

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

typedef enum IndirectArgumentType
{
	INDIRECT_DRAW,
	INDIRECT_DRAW_INDEX,
	INDIRECT_DISPATCH,
	INDIRECT_VERTEX_BUFFER,
	INDIRECT_INDEX_BUFFER,
	INDIRECT_CONSTANT,
	INDIRECT_DESCRIPTOR_TABLE,        // only for vulkan
	INDIRECT_PIPELINE,                // only for vulkan now, probally will add to dx when it comes to xbox
	INDIRECT_CONSTANT_BUFFER_VIEW,    // only for dx
	INDIRECT_SHADER_RESOURCE_VIEW,    // only for dx
	INDIRECT_UNORDERED_ACCESS_VIEW,   // only for dx
  INDIRECT_COMMAND_BUFFER,          // metal ICB
  INDIRECT_COMMAND_BUFFER_OPTIMIZE  // metal indirect buffer optimization
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
	DESCRIPTOR_TYPE_VERTEX_BUFFER = (DESCRIPTOR_TYPE_UNIFORM_BUFFER << 1),
	DESCRIPTOR_TYPE_INDEX_BUFFER = (DESCRIPTOR_TYPE_VERTEX_BUFFER << 1),
	DESCRIPTOR_TYPE_INDIRECT_BUFFER = (DESCRIPTOR_TYPE_INDEX_BUFFER << 1),
	/// Push constant / Root constant
	DESCRIPTOR_TYPE_ROOT_CONSTANT = (DESCRIPTOR_TYPE_INDIRECT_BUFFER << 1),
	/// Cubemap SRV
	DESCRIPTOR_TYPE_TEXTURE_CUBE = (DESCRIPTOR_TYPE_TEXTURE | (DESCRIPTOR_TYPE_ROOT_CONSTANT << 1)),
	/// RTV / DSV per array slice
	DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES = (DESCRIPTOR_TYPE_ROOT_CONSTANT << 2),
	/// RTV / DSV per depth slice
	DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES = (DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES << 1),
	DESCRIPTOR_TYPE_RAY_TRACING = (DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES << 1),
#if defined(VULKAN)
	/// Subpass input (descriptor type only available in Vulkan)
	DESCRIPTOR_TYPE_INPUT_ATTACHMENT = (DESCRIPTOR_TYPE_RAY_TRACING << 1),
	DESCRIPTOR_TYPE_TEXEL_BUFFER = (DESCRIPTOR_TYPE_INPUT_ATTACHMENT << 1),
	DESCRIPTOR_TYPE_RW_TEXEL_BUFFER = (DESCRIPTOR_TYPE_TEXEL_BUFFER << 1),
#endif
#if defined(METAL)
    DESCRIPTOR_TYPE_ARGUMENT_BUFFER = (DESCRIPTOR_TYPE_RAY_TRACING << 1),
    DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER = (DESCRIPTOR_TYPE_ARGUMENT_BUFFER << 1),
    DESCRIPTOR_TYPE_RENDER_PIPELINE_STATE = (DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER << 1),
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
	SHADER_STAGE_RAYTRACING  = 0X00000040,
	SHADER_STAGE_ALL_GRAPHICS = ((uint32_t)SHADER_STAGE_VERT | (uint32_t)SHADER_STAGE_TESC | (uint32_t)SHADER_STAGE_TESE | (uint32_t)SHADER_STAGE_GEOM | (uint32_t)SHADER_STAGE_FRAG),
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
	BC_INV_BLEND_FACTOR,
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

typedef enum DepthStencilClearFlags
{
	ClEAR_DEPTH = 0x01,
	CLEAR_STENCIL = 0x02
} DepthStencilClearFlags;
MAKE_ENUM_FLAG(uint32_t, DepthStencilClearFlags)

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
    
#ifdef METAL
    /* ICB Flags */
    /// Ihnerit pipeline in ICB
    BUFFER_CREATION_FLAG_ICB_INHERIT_PIPELINE = 0x100,
    /// Ihnerit pipeline in ICB
    BUFFER_CREATION_FLAG_ICB_INHERIT_BUFFERS = 0x200,
    
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
	Buffer*        pBuffer;
	ResourceState  mNewState;
	bool           mSplit;
} BufferBarrier;

typedef struct TextureBarrier
{
	Texture*       pTexture;
	ResourceState  mNewState;
	bool           mSplit;
} TextureBarrier;

typedef struct ReadRange
{
	uint64_t mOffset;
	uint64_t mSize;
} ReadRange;

typedef struct Region3D
{
	uint32_t mXOffset;
	uint32_t mYOffset;
	uint32_t mZOffset;
	uint32_t mWidth;
	uint32_t mHeight;
	uint32_t mDepth;
} Region3D;

typedef struct Extent3D
{
	uint32_t mWidth;
	uint32_t mHeight;
	uint32_t mDepth;
} Extent3D;

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
	QueryPoolDesc    mDesc;
#if defined(DIRECT3D12)
	ID3D12QueryHeap* pDxQueryHeap;
#endif
#if defined(VULKAN)
	VkQueryPool      pVkQueryPool;
#endif
#if defined(DIRECT3D11)
	ID3D11Query**    ppDxQueries;
#endif
#if defined(METAL)
    uint64_t         mGpuTimestampStart;
    uint64_t         mGpuTimestampEnd;
#endif
} QueryPool;

/// Data structure holding necessary info to create a Buffer
typedef struct BufferDesc
{
	/// Size of the buffer (in bytes)
	uint64_t mSize;
	/// Decides which memory heap buffer will use (default, upload, readback)
	ResourceMemoryUsage mMemoryUsage;
	/// Creation flags of the buffer
	BufferCreationFlags mFlags;
	/// What state will the buffer get created in
	ResourceState mStartState;
	/// Specifies whether the buffer will have 32 bit or 16 bit indices (applicable to BUFFER_USAGE_INDEX)
	IndexType mIndexType;
	/// Vertex stride of the buffer (applicable to BUFFER_USAGE_VERTEX)
	uint32_t mVertexStride;
	/// Index of the first element accessible by the SRV/UAV (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	uint64_t mFirstElement;
	/// Number of elements in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	uint64_t mElementCount;
	/// Size of each element (in bytes) in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	uint64_t mStructStride;
    /// ICB draw type
    IndirectArgumentType mICBDrawType;
    /// ICB max vertex buffers slots count
    uint32_t mICBMaxVertexBufferBind;
    /// ICB max vertex buffers slots count
    uint32_t mICBMaxFragmentBufferBind;
	/// Set this to specify a counter buffer for this buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	struct Buffer* pCounterBuffer;
	/// Format of the buffer (applicable to typed storage buffers (Buffer<T>)
	TinyImageFormat mFormat;
	/// Flags specifying the suitable usage of this buffer (Uniform buffer, Vertex Buffer, Index Buffer,...)
	DescriptorType mDescriptors;
	/// Debug name used in gpu profile
	const wchar_t* pDebugName;
	uint32_t*      pSharedNodeIndices;
	uint32_t       mNodeIndex;
	uint32_t       mSharedNodeIndexCount;
} BufferDesc;

typedef struct Buffer
{
	/// Position of dynamic buffer memory in the mapped resource
	uint64_t mPositionInHeap;
	/// CPU address of the mapped buffer (appliacable to buffers created in CPU accessible heaps (CPU, CPU_TO_GPU, GPU_TO_CPU)
	void* pCpuMappedAddress;
#if defined(DIRECT3D12)
	/// GPU Address
	D3D12_GPU_VIRTUAL_ADDRESS mDxGpuAddress;
	/// Descriptor handle of the CBV in a CPU visible descriptor heap (applicable to BUFFER_USAGE_UNIFORM)
	D3D12_CPU_DESCRIPTOR_HANDLE mDxCbvHandle;
	/// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to BUFFER_USAGE_STORAGE_SRV)
	D3D12_CPU_DESCRIPTOR_HANDLE mDxSrvHandle;
	/// Descriptor handle of the UAV in a CPU visible descriptor heap (applicable to BUFFER_USAGE_STORAGE_UAV)
	D3D12_CPU_DESCRIPTOR_HANDLE mDxUavHandle;
	/// Native handle of the underlying resource
	ID3D12Resource* pDxResource;
	/// Contains resource allocation info such as parent heap, offset in heap
	struct ResourceAllocation* pDxAllocation;
#endif
#if defined(DIRECT3D11)
	ID3D11Buffer*              pDxResource;
	ID3D11ShaderResourceView*  pDxSrvHandle;
	ID3D11UnorderedAccessView* pDxUavHandle;
#endif
#if defined(VULKAN)
	/// Native handle of the underlying resource
	VkBuffer pVkBuffer;
	/// Buffer view
	VkBufferView pVkStorageTexelView;
	VkBufferView pVkUniformTexelView;
	/// Contains resource allocation info such as parent heap, offset in heap
	struct VmaAllocation_T* pVkAllocation;
	/// Description for creating the descriptor for this buffer (applicable to BUFFER_USAGE_UNIFORM, BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	VkDescriptorBufferInfo mVkBufferInfo;
#endif
#if defined(METAL)
	/// Contains resource allocation info such as parent heap, offset in heap
	struct ResourceAllocation* pMtlAllocation;
	/// Native handle of the underlying resource
    union {
        id<MTLBuffer> mtlBuffer;
        id<MTLIndirectCommandBuffer> mtlIndirectCommandBuffer;
    };
#endif
	/// Buffer creation info
	BufferDesc mDesc;
	/// Current state of the buffer
	ResourceState mCurrentState;
	/// State of the buffer before mCurrentState (used for state tracking during a split barrier)
	ResourceState mPreviousState;
#if defined(DIRECT3D12)
	DXGI_FORMAT mDxIndexFormat;
#endif
#if defined(DIRECT3D11)
	DXGI_FORMAT mDxIndexFormat;
#endif
} Buffer;

typedef struct ClearValue
{
	// Anonymous structures generates warnings in C++11.
	// See discussion here for more info: https://stackoverflow.com/questions/2253878/why-does-c-disallow-anonymous-structs
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4201)    // warning C4201: nonstandard extension used: nameless struct/union
#endif
	union
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
			float  depth;
			uint32 stencil;
		};
	};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
} ClearValue;

/// Data structure holding necessary info to create a Texture
typedef struct TextureDesc
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
	/// Number of multisamples per pixel (currently Textures created with mUsage TEXTURE_USAGE_SAMPLED_IMAGE only support SAMPLE_COUNT_1)
	SampleCount mSampleCount;
	/// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value appropriate for mSampleCount
	uint32_t mSampleQuality;
	///  image format
	TinyImageFormat mFormat;
	/// Optimized clear value (recommended to use this same value when clearing the rendertarget)
	ClearValue mClearValue;
	/// What state will the texture get created in
	ResourceState mStartState;
	/// Descriptor creation
	DescriptorType mDescriptors;
	/// Pointer to native texture handle if the texture does not own underlying resource
	const void* pNativeHandle;
	/// Debug name used in gpu profile
	const wchar_t* pDebugName;
	/// GPU indices to share this texture
	uint32_t* pSharedNodeIndices;
	/// Number of GPUs to share this texture
	uint32_t mSharedNodeIndexCount;
	/// GPU which will own this texture
	uint32_t mNodeIndex;
	/// Is the texture CPU accessible (applicable on hardware supporting CPU mapped textures (UMA))
	bool mHostVisible;
} TextureDesc;

// Virtual texture page as a part of the partially resident texture
// Contains memory bindings, offsets and status information
struct VirtualTexturePage
{
	/// Buffer which contains the image data and be used for copying it to Virtual texture
	Buffer*	pIntermediateBuffer;
	/// Miplevel for this page
	uint32_t mipLevel;
	/// Array layer for this page																
	uint32_t layer;
	/// Index for this page
	uint32_t index;
#if defined(DIRECT3D12)
	/// Offset for this page
	D3D12_TILED_RESOURCE_COORDINATE offset;
	/// Size for this page
	D3D12_TILED_RESOURCE_COORDINATE extent;
	/// Byte size for this page
	uint32_t size;	
#endif

#if defined(VULKAN)
	/// Offset for this page
	VkOffset3D offset;
	/// Size for this page
	VkExtent3D extent;
	/// Sparse image memory bind for this page
	VkSparseImageMemoryBind imageMemoryBind;						
	/// Byte size for this page
	VkDeviceSize size;
#endif

	VirtualTexturePage()
	{
		pIntermediateBuffer = NULL;
#if defined(VULKAN)
		imageMemoryBind.memory = VK_NULL_HANDLE;
#endif
	}
};

typedef struct Texture
{
#if defined(DIRECT3D12)
	/// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to TEXTURE_USAGE_SAMPLED_IMAGE)
	D3D12_CPU_DESCRIPTOR_HANDLE  mDxSRVDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE* pDxUAVDescriptors;
	/// Native handle of the underlying resource
	ID3D12Resource* pDxResource;
	/// Contains resource allocation info such as parent heap, offset in heap
	struct ResourceAllocation* pDxAllocation;
	/// Memory for Sparse texture
	ID3D12Heap* pSparseImageMemory;
	/// Array for Sparse texture's pages
	void* pSparseCoordinates;
	/// Array for heap memory offsets
	void* pHeapRangeStartOffsets;
#endif
#if defined(VULKAN)
	/// Opaque handle used by shaders for doing read/write operations on the texture
	VkImageView pVkSRVDescriptor;
	/// Opaque handle used by shaders for doing read/write operations on the texture
	VkImageView* pVkUAVDescriptors;
	/// Opaque handle used by shaders for doing read/write operations on the texture
	VkImageView* pVkSRVStencilDescriptor;
	/// Native handle of the underlying resource
	VkImage pVkImage;
	/// Contains resource allocation info such as parent heap, offset in heap
	struct VmaAllocation_T* pVkAllocation;
	/// Flags specifying which aspects (COLOR,DEPTH,STENCIL) are included in the pVkImageView
	VkImageAspectFlags mVkAspectMask;
	/// Sparse queue binding information
	VkBindSparseInfo mBindSparseInfo;	
	/// Sparse image memory bindings of all memory-backed virtual tables
	void* pSparseImageMemoryBinds;
	/// Sparse ?aque memory bindings for the mip tail (if present)	
	void* pOpaqueMemoryBinds;
	/// First mip level in mip tail
	uint32_t mMipTailStart;
	/// Lstly filled mip level in mip tail						
	uint32_t mLastFilledMip;
	/// Memory type for Sparse texture's memory
	uint32_t mSparseMemoryTypeIndex;
	/// Sparse image memory bind info 
	VkSparseImageMemoryBindInfo mImageMemoryBindInfo;
	/// Sparse image opaque memory bind info (mip tail)				
	VkSparseImageOpaqueMemoryBindInfo mOpaqueMemoryBindInfo;
	/// First mip level in mip tail			
	uint32_t mipTailStart;	
#endif
#if defined(METAL)
	/// Contains resource allocation info such as parent heap, offset in heap
	struct ResourceAllocation* pMtlAllocation;
	/// Native handle of the underlying resource
	id<MTLTexture> mtlTexture;
	id<MTLTexture> __strong* pMtlUAVDescriptors;
	id 						 mpsTextureAllocator;
	MTLPixelFormat           mtlPixelFormat;
	bool                     mIsCompressed;
#endif
#if defined(DIRECT3D11)
	ID3D11Resource*             pDxResource;
	ID3D11ShaderResourceView*   pDxSRVDescriptor;
	ID3D11UnorderedAccessView** pDxUAVDescriptors;
#endif
	/// Virtual Texture members
	/// Contains all virtual pages of the texture						
	void* pPages;
	/// Visibility data
	Buffer* mVisibility;
	/// PrevVisibility data
	Buffer* mPrevVisibility;
	/// Alive Page's Index
	Buffer* mAlivePage;
	/// The count of Alive pages
	Buffer* mAlivePageCount;
	/// Page's Index which should be removed
	Buffer* mRemovePage;
	/// the count of pages which should be removed
	Buffer* mRemovePageCount;
	/// Original Pixel image data
	void* mVirtualImageData;
	/// Sparse Virtual Texture Width
	uint64_t mSparseVirtualTexturePageWidth;
	/// Sparse Virtual Texture Height
	uint64_t mSparseVirtualTexturePageHeight;
	/// Texture creation info
	TextureDesc mDesc;
	/// Size of the texture (in bytes)
	uint64_t mTextureSize;
	/// Current state of the texture
	ResourceState mCurrentState;
	/// State of the texture before mCurrentState (used for state tracking during a split barrier)
	ResourceState mPreviousState;
	/// This value will be false if the underlying resource is not owned by the texture (swapchain textures,...)
	bool mOwnsImage;	
} Texture;

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
	/// Optimized clear value (recommended to use this same value when clearing the rendertarget)
	ClearValue mClearValue;
	/// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value appropriate for mSampleCount
	uint32_t mSampleQuality;
	/// Descriptor creation
	DescriptorType mDescriptors;
	const void*    pNativeHandle;
	/// Debug name used in gpu profile
	const wchar_t* pDebugName;
	/// GPU indices to share this texture
	uint32_t* pSharedNodeIndices;
	/// Number of GPUs to share this texture
	uint32_t mSharedNodeIndexCount;
	/// GPU which will own this texture
	uint32_t mNodeIndex;
} RenderTargetDesc;

typedef struct RenderTarget
{
	RenderTargetDesc mDesc;
	Texture*         pTexture;

#if defined(DIRECT3D12)
	D3D12_CPU_DESCRIPTOR_HANDLE* pDxDescriptors;
#endif
#if defined(VULKAN)
	VkImageView* pVkDescriptors;
	uint64_t     mId;
#endif
#if defined(DIRECT3D11)
	union
	{
		/// Resources
		ID3D11RenderTargetView** pDxRtvDescriptors;
		ID3D11DepthStencilView** pDxDsvDescriptors;
	};
#endif
} RenderTarget;

typedef struct LoadActionsDesc
{
	ClearValue     mClearColorValues[MAX_RENDER_TARGET_ATTACHMENTS];
	LoadActionType mLoadActionsColor[MAX_RENDER_TARGET_ATTACHMENTS];
	ClearValue     mClearDepth;
	LoadActionType mLoadActionDepth;
	LoadActionType mLoadActionStencil;
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
	float       mMaxAnisotropy;
	CompareMode mCompareFunc;
} SamplerDesc;

typedef struct Sampler
{
#if defined(DIRECT3D12)
	/// Description for creating the Sampler descriptor for ths sampler
	D3D12_SAMPLER_DESC mDxDesc;
	/// Descriptor handle of the Sampler in a CPU visible descriptor heap
	D3D12_CPU_DESCRIPTOR_HANDLE mDxSamplerHandle;
#endif
#if defined(VULKAN)
	/// Native handle of the underlying resource
	VkSampler pVkSampler;
#endif
#if defined(METAL)
	/// Native handle of the underlying resource
	id<MTLSamplerState> mtlSamplerState;
#endif
#if defined(DIRECT3D11)
	/// Native handle of the underlying resource
	ID3D11SamplerState* pSamplerState;
#endif
} Sampler;

typedef enum DescriptorUpdateFrequency
{
    DESCRIPTOR_UPDATE_FREQ_NONE = 0,
	DESCRIPTOR_UPDATE_FREQ_PER_FRAME,
	DESCRIPTOR_UPDATE_FREQ_PER_BATCH,
	DESCRIPTOR_UPDATE_FREQ_PER_DRAW,
	DESCRIPTOR_UPDATE_FREQ_COUNT,
} DescriptorUpdateFrequency;

/// Data structure holding the layout for a descriptor
typedef struct DescriptorInfo
{
	/// Binding information generated from the shader reflection
	ShaderResource mDesc;
	/// Index in the descriptor set
	uint32_t mIndexInParent;
	/// Update frequency of this descriptor
	DescriptorUpdateFrequency mUpdateFrquency;
	uint32_t                  mHandleIndex;
#if defined(METAL)
    Sampler*                  mStaticSampler;
#endif
#if defined(DIRECT3D12)
	D3D12_ROOT_PARAMETER_TYPE mDxType;
#endif
#if defined(VULKAN)
	VkDescriptorType   mVkType;
	VkShaderStageFlags mVkStages;
	uint32_t           mDynamicUniformIndex;
#endif
} DescriptorInfo;

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
	Shader**               ppShaders;
	uint32_t               mShaderCount;
	uint32_t               mMaxBindlessTextures;
	const char**           ppStaticSamplerNames;
	Sampler**              ppStaticSamplers;
	uint32_t               mStaticSamplerCount;
	RootSignatureFlags     mFlags;
} RootSignatureDesc;

typedef struct RootSignature
{
	/// Number of descriptors declared in the root signature layout
	uint32_t             mDescriptorCount;
	/// Array of all descriptors declared in the root signature layout
	DescriptorInfo*      pDescriptors;
	/// Translates hash of descriptor name to descriptor index in pDescriptors array
	DescriptorIndexMap*  pDescriptorNameToIndexMap;

	PipelineType         mPipelineType;
#if defined(DIRECT3D12)
	uint32_t             mDxViewDescriptorTableRootIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t             mDxSamplerDescriptorTableRootIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t*            pDxViewDescriptorIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t             mDxViewDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t             mDxCumulativeViewDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t*            pDxSamplerDescriptorIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t             mDxSamplerDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t             mDxCumulativeSamplerDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t*            pDxRootDescriptorRootIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t             mDxRootDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t*            pDxRootConstantRootIndices;
	uint32_t             mDxRootConstantCount;
	ID3D12RootSignature* pDxRootSignature;
	ID3DBlob*            pDxSerializedRootSignatureString;
#endif
#if defined(VULKAN)
	VkDescriptorSetLayout mVkDescriptorSetLayouts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t              mVkDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t              mVkDynamicDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t              mVkRaytracingDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t              mVkCumulativeDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	VkPushConstantRange*  pVkPushConstantRanges;
	VkPipelineLayout      pPipelineLayout;
	VkDescriptorUpdateTemplate mUpdateTemplates[DESCRIPTOR_UPDATE_FREQ_COUNT];
	VkDescriptorSet       mVkEmptyDescriptorSets[DESCRIPTOR_UPDATE_FREQ_COUNT];
	void*                 pUpdateTemplateData[DESCRIPTOR_UPDATE_FREQ_COUNT][MAX_GPUS];
	uint32_t              mVkPushConstantCount;
#endif
#if defined(METAL)
    // paramIndex support
    typedef struct IndexedDescriptor
	{
        const DescriptorInfo**  pDescriptors;
        uint32_t                mDescriptorCount;
    } IndexedDescriptor;
	
	Sampler**           ppStaticSamplers;
	uint32_t*           pStaticSamplerSlots;
	ShaderStage*        pStaticSamplerStages;
	uint32_t            mStaticSamplerCount;
    ShaderDescriptors*  pShaderDescriptors;
    uint32_t            mShaderDescriptorsCount;
    IndexedDescriptor*  mIndexedDescriptorInfo;
	uint32_t            mIndexedDescriptorCount;
#endif
#if defined(DIRECT3D11)
	ID3D11SamplerState** ppStaticSamplers;
	uint32_t*            pStaticSamplerSlots;
	ShaderStage*         pStaticSamplerStages;
	uint32_t             mStaticSamplerCount;
#endif
} RootSignature;

typedef struct DescriptorData
{
	/// User can either set name of descriptor or index (index in pRootSignature->pDescriptors array)
	/// Name of descriptor
	const char* pName;
	union
	{
		struct
		{
			/// Offset to bind the buffer descriptor
			uint64_t* pOffsets;
			uint64_t* pSizes;
		};

        // Descriptor set buffer extraction options
        struct
        {
            uint32_t    mDescriptorSetBufferIndex;
            Shader*     mDescriptorSetShader;
            ShaderStage mDescriptorSetShaderStage;
        };

        uint32_t mUAVMipSlice;
		bool mBindStencilResource;
	};
	/// Array of resources containing descriptor handles or constant to be used in ring buffer memory - DescriptorRange can hold only one resource type array
	union
	{
		/// Array of texture descriptors (srv and uav textures)
		Texture** ppTextures;
		/// Array of sampler descriptors
		Sampler** ppSamplers;
		/// Array of buffer descriptors (srv, uav and cbv buffers)
		Buffer** ppBuffers;
        /// Array of pipline descriptors
        Pipeline** ppPipelines;
        /// DescriptorSet buffer extraction
        DescriptorSet** ppDescriptorSet;
        /// Custom binding (raytracing acceleration structure ...)
		AccelerationStructure** ppAccelerationStructures;
	};
	/// Number of resources in the descriptor(applies to array of textures, buffers,...)
	uint32_t mCount;
	uint32_t mIndex = (uint32_t)-1;
    bool     mExtractBuffer = false;
} DescriptorData;

typedef struct CmdPoolDesc
{
	CmdPoolType mCmdPoolType;
} CmdPoolDesc;

typedef struct CmdPool
{
	Queue*      pQueue;
	CmdPoolDesc mCmdPoolDesc;
#if defined(DIRECT3D12)
	// Temporarily move to Cmd struct until we get the command allocator pool logic working
	//ID3D12CommandAllocator*	   pDxCmdAlloc;
#endif
#if defined(VULKAN)
	VkCommandPool pVkCmdPool;
#endif
} CmdPool;

typedef struct Cmd
{
	Renderer* pRenderer;
	CmdPool*  pCmdPool;

	const RootSignature* pBoundRootSignature;
	uint32_t             mNodeIndex;
#if defined(DIRECT3D12)
	// For now each command list will have its own allocator until we get the command allocator pool logic working
	ID3D12CommandAllocator*     pDxCmdAlloc;
	ID3D12GraphicsCommandList*  pDxCmdList;
	DescriptorSet*              pBoundDescriptorSets[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint16_t                    mBoundDescriptorSetIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
#endif
#if defined(VULKAN)
	VkCommandBuffer pVkCmdBuf;
	VkRenderPass    pVkActiveRenderPass;
#endif
#if defined(METAL)
	id<MTLCommandBuffer>         mtlCommandBuffer;
	id<MTLRenderCommandEncoder>  mtlRenderEncoder;
	id<MTLComputeCommandEncoder> mtlComputeEncoder;
	id<MTLBlitCommandEncoder>    mtlBlitEncoder;
	MTLRenderPassDescriptor*     pRenderPassDesc;
	Shader*                      pShader;
	Buffer*                      selectedIndexBuffer;
	uint64_t                     mSelectedIndexBufferOffset;
	QueryPool*                   pLastFrameQuery;
	MTLPrimitiveType             selectedPrimitiveType;
#endif
#if defined(DIRECT3D11)
	uint8_t* pDescriptorCache;
	Buffer*  pRootConstantBuffer;
	Buffer*  pTransientConstantBuffer;
	uint32_t mDescriptorCacheOffset;
#endif
} Cmd;

typedef struct QueueDesc
{
	QueueFlag     mFlag;
	QueuePriority mPriority;
	CmdPoolType   mType;
	uint32_t      mNodeIndex;
} QueueDesc;

typedef enum FenceStatus
{
	FENCE_STATUS_COMPLETE = 0,
	FENCE_STATUS_INCOMPLETE,
	FENCE_STATUS_NOTSUBMITTED,
} FenceStatus;

typedef struct Fence
{
#if defined(DIRECT3D12)
	ID3D12Fence* pDxFence;
	HANDLE       pDxWaitIdleFenceEvent;
	uint64       mFenceValue;
#endif
#if defined(VULKAN)
	VkFence pVkFence;
	bool    mSubmitted;
#endif
#if defined(METAL)
	dispatch_semaphore_t pMtlSemaphore;
	bool                 mSubmitted;
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
	Fence* pFence;
#endif
#if defined(VULKAN)
	VkSemaphore pVkSemaphore;
	uint32_t    mCurrentNodeIndex;
	bool        mSignaled;
#endif
#if defined(METAL)
	dispatch_semaphore_t pMtlSemaphore;
#endif
} Semaphore;

typedef struct Queue
{
	Renderer* pRenderer;
#if defined(DIRECT3D12)
	ID3D12CommandQueue* pDxQueue;
	Fence*              pQueueFence;
#endif
#if defined(VULKAN)
	VkQueue  pVkQueue;
	uint32_t mVkQueueFamilyIndex;
	uint32_t mVkQueueIndex;
#endif
#if defined(METAL)
	id<MTLCommandQueue>     mtlCommandQueue;
	uint32_t                mBarrierFlags;
	id<MTLFence>            mtlQueueFence;
#endif
	QueueDesc mQueueDesc;
	Extent3D  mUploadGranularity;
} Queue;

typedef struct ShaderMacro
{
	const char* definition;
	const char* value;
} ShaderMacro;

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
	ShaderStage     mStages;
	ShaderStageDesc mVert;
	ShaderStageDesc mFrag;
	ShaderStageDesc mGeom;
	ShaderStageDesc mHull;
	ShaderStageDesc mDomain;
	ShaderStageDesc mComp;
} ShaderDesc;
#endif

typedef struct BinaryShaderStageDesc
{
	/// Byte code array
	const char*    pByteCode;
	uint32_t       mByteCodeSize;
	const char*    pEntryPoint;
#if defined(METAL)
	// Shader source is needed for reflection
	char*          pSource;
	uint32_t       mSourceSize;
#endif
} BinaryShaderStageDesc;

typedef struct BinaryShaderDesc
{
	ShaderStage           mStages;
	BinaryShaderStageDesc mVert;
	BinaryShaderStageDesc mFrag;
	BinaryShaderStageDesc mGeom;
	BinaryShaderStageDesc mHull;
	BinaryShaderStageDesc mDomain;
	BinaryShaderStageDesc mComp;
} BinaryShaderDesc;

typedef struct Shader
{
	ShaderStage           mStages;
	PipelineReflection    mReflection;
#if defined(DIRECT3D12)
	ID3DBlob**            pShaderBlobs;
	LPCWSTR*              pEntryNames;
#endif
#if defined(VULKAN)
	VkShaderModule*       pShaderModules;
	char**                pEntryNames;
#endif
#if defined(METAL)
	id<MTLFunction>       mtlVertexShader;
	id<MTLFunction>       mtlFragmentShader;
	id<MTLFunction>       mtlComputeShader;
	id<MTLLibrary>		  mtlLibrary;
	char**                pEntryNames;
	uint32_t              mNumThreadsPerGroup[3];
#endif
#if defined(DIRECT3D11)
	ID3D11VertexShader*   pDxVertexShader;
	ID3D11PixelShader*    pDxPixelShader;
	ID3D11GeometryShader* pDxGeometryShader;
	ID3D11DomainShader*   pDxDomainShader;
	ID3D11HullShader*     pDxHullShader;
	ID3D11ComputeShader*  pDxComputeShader;
	ID3DBlob*             pDxInputSignature;
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

typedef struct BlendState
{
#if defined(DIRECT3D12)
	D3D12_BLEND_DESC mDxBlendDesc;
#endif
#if defined(VULKAN)
	VkPipelineColorBlendAttachmentState RTBlendStates[MAX_RENDER_TARGET_ATTACHMENTS];
	VkBool32                            LogicOpEnable;
	VkLogicOp                           LogicOp;
#endif
#if defined(METAL)
	struct BlendStateData
	{
		MTLBlendFactor    srcFactor;
		MTLBlendFactor    destFactor;
		MTLBlendFactor    srcAlphaFactor;
		MTLBlendFactor    destAlphaFactor;
		MTLBlendOperation blendMode;
		MTLBlendOperation blendAlphaMode;
	};
	BlendStateData blendStatePerRenderTarget[MAX_RENDER_TARGET_ATTACHMENTS];
	bool           alphaToCoverage;
#endif
#if defined(DIRECT3D11)
	ID3D11BlendState* pBlendState;
#endif
} BlendState;

typedef struct DepthStateDesc
{
	bool        mDepthTest;
	bool        mDepthWrite;
	CompareMode mDepthFunc = CompareMode::CMP_LEQUAL;
	bool        mStencilTest;
	uint8_t     mStencilReadMask;
	uint8_t     mStencilWriteMask;
	CompareMode mStencilFrontFunc = CompareMode::CMP_ALWAYS;
	StencilOp   mStencilFrontFail;
	StencilOp   mDepthFrontFail;
	StencilOp   mStencilFrontPass;
	CompareMode mStencilBackFunc = CompareMode::CMP_ALWAYS;
	StencilOp   mStencilBackFail;
	StencilOp   mDepthBackFail;
	StencilOp   mStencilBackPass;
} DepthStateDesc;

typedef struct DepthState
{
#if defined(DIRECT3D12)
	D3D12_DEPTH_STENCIL_DESC mDxDepthStencilDesc;
#endif
#if defined(VULKAN)
	VkBool32         DepthTestEnable;
	VkBool32         DepthWriteEnable;
	VkCompareOp      DepthCompareOp;
	VkBool32         DepthBoundsTestEnable;
	VkBool32         StencilTestEnable;
	VkStencilOpState Front;
	VkStencilOpState Back;
	float            MinDepthBounds;
	float            MaxDepthBounds;
#endif
#if defined(METAL)
	id<MTLDepthStencilState> mtlDepthState;
#endif
#if defined(DIRECT3D11)
	ID3D11DepthStencilState* pDxDepthStencilState;
#endif
} DepthState;

typedef struct RasterizerStateDesc
{
	CullMode  mCullMode;
	int32_t   mDepthBias;
	float     mSlopeScaledDepthBias;
	FillMode  mFillMode;
	bool      mMultiSample;
	bool      mScissor;
	FrontFace mFrontFace;
} RasterizerStateDesc;

typedef struct RasterizerState
{
#if defined(DIRECT3D12)
	D3D12_RASTERIZER_DESC mDxRasterizerDesc;
#endif
#if defined(VULKAN)
	VkBool32        DepthClampEnable;
	VkPolygonMode   PolygonMode;
	VkCullModeFlags CullMode;
	VkFrontFace     FrontFace;
	VkBool32        DepthBiasEnable;
	float           DepthBiasConstantFactor;
	float           DepthBiasClamp;
	float           DepthBiasSlopeFactor;
	float           LineWidth;
#endif
#if defined(METAL)
	MTLCullMode         cullMode;
	MTLTriangleFillMode fillMode;
	float               depthBiasSlopeFactor;
	float               depthBias;
	bool                scissorEnable;
	bool                multisampleEnable;
	MTLWinding          frontFace;
#endif
#if defined(DIRECT3D11)
	ID3D11RasterizerState* pDxRasterizerState;
#endif
} RasterizerState;

typedef enum VertexAttribRate
{
	VERTEX_ATTRIB_RATE_VERTEX = 0,
	VERTEX_ATTRIB_RATE_INSTANCE = 1,
	VERTEX_ATTRIB_RATE_COUNT,
} VertexAttribRate;

typedef struct VertexAttrib
{
	ShaderSemantic    mSemantic;
	uint32_t          mSemanticNameLength;
	char              mSemanticName[MAX_SEMANTIC_NAME_LENGTH];
	TinyImageFormat	  mFormat;
	uint32_t          mBinding;
	uint32_t          mLocation;
	uint32_t          mOffset;
	VertexAttribRate  mRate;

} VertexAttrib;

typedef struct VertexLayout
{
	uint32_t     mAttribCount;
	VertexAttrib mAttribs[MAX_VERTEX_ATTRIBS];
} VertexLayout;

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
	Raytracing*			pRaytracing;
	RootSignature*		pGlobalRootSignature;
	Shader*             pRayGenShader;
	RootSignature*		pRayGenRootSignature;
	Shader**            ppMissShaders;
	RootSignature**		ppMissRootSignatures;
	RaytracingHitGroup* pHitGroups;
	RootSignature*		pEmptyRootSignature;
	unsigned			mMissShaderCount;
	unsigned			mHitGroupCount;
	// #TODO : Remove this after adding shader reflection for raytracing shaders
	unsigned			mPayloadSize;
	// #TODO : Remove this after adding shader reflection for raytracing shaders
	unsigned			mAttributeSize;
	unsigned			mMaxTraceRecursionDepth;
	unsigned            mMaxRaysCount;
} RaytracingPipelineDesc;


typedef struct GraphicsPipelineDesc
{
	Shader*            pShaderProgram;
	RootSignature*     pRootSignature;
	VertexLayout*      pVertexLayout;
	BlendState*        pBlendState;
	DepthState*        pDepthState;
	RasterizerState*   pRasterizerState;
	TinyImageFormat* 	 pColorFormats;
	uint32_t           mRenderTargetCount;
	SampleCount        mSampleCount;
	uint32_t           mSampleQuality;
	TinyImageFormat  	 mDepthStencilFormat;
	PrimitiveTopology  mPrimitiveTopo;
    bool               mSupportIndirectCommandBuffer;
} GraphicsPipelineDesc;

typedef struct ComputePipelineDesc
{
	Shader*        pShaderProgram;
	RootSignature* pRootSignature;
} ComputePipelineDesc;

typedef struct PipelineDesc
{
	PipelineType mType;
	union {
		ComputePipelineDesc		mComputeDesc;
		GraphicsPipelineDesc	mGraphicsDesc;
		RaytracingPipelineDesc	mRaytracingDesc;
	};
} PipelineDesc;

//this is needed because unit tests have different WindowsSDK versions.
//Minimum 10.0.17763.0 is required in every project to remove this typedef
typedef struct ID3D12StateObject ID3D12StateObject;

#ifdef METAL
typedef struct RaytracingPipeline RaytracingPipeline;
#endif

typedef struct Pipeline
{
	union
	{
		GraphicsPipelineDesc mGraphics;
		ComputePipelineDesc  mCompute;
	};
	PipelineType mType;
#if defined(DIRECT3D12)
	ID3D12PipelineState*   pDxPipelineState;
	D3D_PRIMITIVE_TOPOLOGY mDxPrimitiveTopology;
	ID3D12StateObject*	   pDxrPipeline;
#endif
#if defined(VULKAN)
	VkPipeline pVkPipeline;
	
	//In DX12 this information is stored in ID3D12StateObject.
	//But for Vulkan we need to store it manually
	const char**                ppShaderStageNames;
	uint32_t                    mShaderStageCount;
#endif
#if defined(METAL)
	RenderTarget*               pRenderPasspRenderTarget;
	Shader*                     pShader;
	id<MTLRenderPipelineState>  mtlRenderPipelineState;
	id<MTLComputePipelineState> mtlComputePipelineState;
    RaytracingPipeline*         pRaytracingPipeline;
	MTLCullMode                 mCullMode;
#endif
#if defined(DIRECT3D11)
	ID3D11VertexShader*    pDxVertexShader;
	ID3D11PixelShader*     pDxPixelShader;
	ID3D11GeometryShader*  pDxGeometryShader;
	ID3D11DomainShader*    pDxDomainShader;
	ID3D11HullShader*      pDxHullShader;
	ID3D11ComputeShader*   pDxComputeShader;
	ID3D11InputLayout*     pDxInputLayout;
	D3D_PRIMITIVE_TOPOLOGY mDxPrimitiveTopology;
#endif
} Pipeline;

typedef struct SubresourceDataDesc
{
	// Source description
	uint64_t mBufferOffset;
	uint32_t mRowPitch;
	uint32_t mSlicePitch;
	// Destination description
	uint32_t mArrayLayer;
	uint32_t mMipLevel;
	Region3D mRegion;
} SubresourceDataDesc;

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
	/// Sample count
	SampleCount mSampleCount;
	/// Sample quality (DirectX12 only)
	uint32_t mSampleQuality;
	/// Color format of the swapchain
	TinyImageFormat mColorFormat;
	/// Clear value
	ClearValue mColorClearValue;
	/// Set whether swap chain will be presented using vsync
	bool mEnableVsync;
} SwapChainDesc;

typedef struct SwapChain
{
	SwapChainDesc mDesc;
	/// Render targets created from the swapchain back buffers
	RenderTarget** ppSwapchainRenderTargets;
#if defined(_DURANGO)
	IDXGISwapChain1* pDxSwapChain;
	UINT             mDxSyncInterval;
	ID3D12Resource** ppDxSwapChainResources;
	uint32_t         mFlags;
#elif defined(DIRECT3D12)
	/// Use IDXGISwapChain3 for now since IDXGISwapChain4
	/// isn't supported by older devices.
	IDXGISwapChain3* pDxSwapChain;
	/// Sync interval to specify how interval for vsync
	UINT             mDxSyncInterval;
	ID3D12Resource** ppDxSwapChainResources;
	uint32_t         mFlags;
#endif
#if defined(DIRECT3D11)
	/// Use IDXGISwapChain3 for now since IDXGISwapChain4
	/// isn't supported by older devices.
	IDXGISwapChain* pDxSwapChain;
	/// Sync interval to specify how interval for vsync
	UINT             mDxSyncInterval;
	ID3D11Resource** ppDxSwapChainResources;
	uint32_t         mFlags;
#endif
#if defined(VULKAN)
	/// Present queue if one exists (queuePresent will use this queue if the hardware has a dedicated present queue)
	VkQueue        pPresentQueue;
	VkSwapchainKHR pSwapChain;
	VkSurfaceKHR   pVkSurface;
	VkImage*       ppVkSwapChainImages;
	uint32_t       mPresentQueueFamilyIndex;
#endif
#if defined(METAL)
#   if defined(TARGET_IOS)
        UIView*              pForgeView;
#   else
        NSView*              pForgeView;
#endif
	id<CAMetalDrawable>  mMTKDrawable;
	id<MTLCommandBuffer> presentCommandBuffer;
#endif
} SwapChain;

typedef enum ShaderTarget
{
	// We only need SM 5.0 for supporting D3D11 fallback
#if defined(DIRECT3D11)
	shader_target_5_0,
#else
    // 5.1 is supported on all DX12 hardware
    shader_target_5_1,
    shader_target_6_0,
	shader_target_6_1,
	shader_target_6_2,
	shader_target_6_3, //required for Raytracing
#endif
} ShaderTarget;

typedef enum GpuMode
{
	GPU_MODE_SINGLE = 0,
	GPU_MODE_LINKED,
	// #TODO GPU_MODE_UNLINKED,
} GpuMode;

typedef struct RendererDesc
{
	LogFn                        pLogFn;
	RendererApi                  mApi;
	ShaderTarget                 mShaderTarget;
	GpuMode                      mGpuMode;
#if defined(VULKAN)
	const char**                 ppInstanceLayers;
	uint32_t                     mInstanceLayerCount;
	const char**                 ppInstanceExtensions;
	uint32_t                     mInstanceExtensionCount;
	const char**                 ppDeviceExtensions;
	uint32_t                     mDeviceExtensionCount;
#endif
#if defined(DIRECT3D12)
	D3D_FEATURE_LEVEL            mDxFeatureLevel;
#endif
	/// This results in new validation not possible during API calls on the CPU, by creating patched shaders that have validation added directly to the shader.
	/// However, it can slow things down a lot, especially for applications with numerous PSOs. Time to see the first render frame may take several minutes
	bool                         mEnableGPUBasedValidation;
} RendererDesc;

typedef struct GPUVendorPreset
{
	char           mVendorId[MAX_GPU_VENDOR_STRING_LENGTH];
	char           mModelId[MAX_GPU_VENDOR_STRING_LENGTH];
	char           mRevisionId[MAX_GPU_VENDOR_STRING_LENGTH];    // OPtional as not all gpu's have that. Default is : 0x00
	GPUPresetLevel mPresetLevel;
	char           mGpuName[MAX_GPU_VENDOR_STRING_LENGTH];    //If GPU Name is missing then value will be empty string
} GPUVendorPreset;

typedef struct GPUCapBits {
	bool canShaderReadFrom[TinyImageFormat_Count];
	bool canShaderWriteTo[TinyImageFormat_Count];
	bool canRenderTargetWriteTo[TinyImageFormat_Count];
} GPUCapBits;

typedef enum DefaultResourceAlignment
{
	RESOURCE_BUFFER_ALIGNMENT = 4U,
} DefaultResourceAlignment;

typedef struct GPUSettings
{
	uint32_t        mUniformBufferAlignment;
	uint32_t        mUploadBufferTextureAlignment;
	uint32_t        mUploadBufferTextureRowAlignment;
	uint32_t        mMaxVertexInputBindings;
	uint32_t        mMaxRootSignatureDWORDS;
	uint32_t        mWaveLaneCount;
	GPUVendorPreset mGpuVendorPreset;
	bool            mMultiDrawIndirect;
	bool            mROVsSupported;
	bool			mPartialUpdateConstantBufferSupported;
#ifdef METAL
    uint32_t        mArgumentBufferMaxTextures;
#endif
} GPUSettings;

typedef struct Renderer
{
	char*        pName;
	RendererDesc mSettings;
	uint32_t     mNumOfGPUs;
	GPUSettings* pActiveGpuSettings;
	GPUSettings  mGpuSettings[MAX_GPUS];
	uint32_t     mLinkedNodeCount;

#if defined(DIRECT3D12)
	// Default NULL Descriptors for binding at empty descriptor slots to make sure all descriptors are bound at submit
	D3D12_CPU_DESCRIPTOR_HANDLE mNullTextureSRV[TEXTURE_DIM_COUNT];
	D3D12_CPU_DESCRIPTOR_HANDLE mNullTextureUAV[TEXTURE_DIM_COUNT];
	D3D12_CPU_DESCRIPTOR_HANDLE mNullBufferSRV;
	D3D12_CPU_DESCRIPTOR_HANDLE mNullBufferUAV;
	D3D12_CPU_DESCRIPTOR_HANDLE mNullBufferCBV;
	D3D12_CPU_DESCRIPTOR_HANDLE mNullSampler;

	// API specific descriptor heap and memory allocator
	struct DescriptorHeap*      pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	struct DescriptorHeap*      pCbvSrvUavHeaps[MAX_GPUS];
	struct DescriptorHeap*      pSamplerHeaps[MAX_GPUS];
	struct ResourceAllocator*   pResourceAllocator;

#if defined(_DEBUG) || defined(PROFILE)
	ID3D12Debug* pDXDebug;
#endif
#endif

#if defined(_DURANGO)
	IDXGIFactory2* pDXGIFactory;
	IDXGIAdapter*  pDxGPUs[MAX_GPUS];
	IDXGIAdapter*  pDxActiveGPU;
	ID3D12Device*  pDxDevice;
	EsramManager*  pESRAMManager;
#elif defined(DIRECT3D12)
	IDXGIFactory5* pDXGIFactory;
	IDXGIAdapter3* pDxGPUs[MAX_GPUS];
	IDXGIAdapter3* pDxActiveGPU;
	ID3D12Device*  pDxDevice;
#endif
#if defined(DIRECT3D11)
	IDXGIFactory1*       pDXGIFactory;
	IDXGIAdapter1*       pDxGPUs[MAX_GPUS];
	IDXGIAdapter1*       pDxActiveGPU;
	ID3D11Device*        pDxDevice;
	ID3D11DeviceContext* pDxContext;
#endif
#if defined(VULKAN)
	VkInstance                        pVkInstance;
	VkPhysicalDevice                  pVkGPUs[MAX_GPUS];
	VkPhysicalDevice                  pVkActiveGPU;
	VkPhysicalDeviceProperties2       mVkGpuProperties[MAX_GPUS];
#ifdef VK_NV_RAY_TRACING_SPEC_VERSION
	VkPhysicalDeviceRayTracingPropertiesNV mVkRaytracingProperties[MAX_GPUS];
#endif
	VkPhysicalDeviceMemoryProperties  mVkGpuMemoryProperties[MAX_GPUS];
	VkPhysicalDeviceFeatures2KHR      mVkGpuFeatures[MAX_GPUS];
	uint32_t                          mVkQueueFamilyPropertyCount[MAX_GPUS];
	VkQueueFamilyProperties*          mVkQueueFamilyProperties[MAX_GPUS];
	uint32_t                          mActiveGPUIndex;
	VkPhysicalDeviceMemoryProperties* pVkActiveGpuMemoryProperties;
	VkPhysicalDeviceFeatures2KHR*     pVkActiveGpuFeatures;
#ifdef VK_NV_RAY_TRACING_SPEC_VERSION
	VkPhysicalDeviceRayTracingPropertiesNV* pVkActiveCPURaytracingProperties;
#endif
	VkPhysicalDeviceProperties2*      pVkActiveGPUProperties;
	VkDevice                          pVkDevice;
#ifdef USE_DEBUG_UTILS_EXTENSION
	VkDebugUtilsMessengerEXT          pVkDebugUtilsMessenger;
#else
	VkDebugReportCallbackEXT          pVkDebugReport;
#endif
	const char**                      ppInstanceLayers;
	uint32_t                          mInstanceLayerCount;
	uint32_t                          mVkUsedQueueCount[MAX_GPUS][16];

	Texture* pDefaultTextureSRV[MAX_GPUS][TEXTURE_DIM_COUNT];
	Texture* pDefaultTextureUAV[MAX_GPUS][TEXTURE_DIM_COUNT];
	Buffer*  pDefaultBufferSRV[MAX_GPUS];
	Buffer*  pDefaultBufferUAV[MAX_GPUS];
	Sampler* pDefaultSampler;

	struct DescriptorPool*      pDescriptorPool;
	struct VmaAllocator_T*      pVmaAllocator;

	// These are the extensions that we have loaded
	const char* gVkInstanceExtensions[MAX_INSTANCE_EXTENSIONS];
	// These are the extensions that we have loaded
	const char* gVkDeviceExtensions[MAX_DEVICE_EXTENSIONS];
#endif
#if defined(METAL)
	id<MTLDevice>               pDevice;
	struct ResourceAllocator*   pResourceAllocator;
#endif
	uint32_t         mCurrentFrameIdx;
	// Default states used if user does not specify them in pipeline creation
	BlendState*      pDefaultBlendState;
	DepthState*      pDefaultDepthState;
	RasterizerState* pDefaultRasterizerState;

	ShaderMacro* pBuiltinShaderDefines;
	uint32_t     mBuiltinShaderDefinesCount;
	GPUCapBits   capBits;

} Renderer;

// Indirect command sturcture define
typedef struct IndirectArgumentDescriptor
{
	IndirectArgumentType mType;
	const char*          pName;
	uint32_t             mIndex;
	uint32_t             mCount;
	uint32_t             mDivisor;

} IndirectArgumentDescriptor;

typedef struct CommandSignatureDesc
{
	CmdPool*                    pCmdPool;
	RootSignature*              pRootSignature;
	uint32_t                    mIndirectArgCount;
	IndirectArgumentDescriptor* pArgDescs;
} CommandSignatureDesc;

typedef struct CommandSignature
{
	CommandSignatureDesc mDesc;
	uint32_t             mIndirectArgDescCounts;
	uint32_t             mDrawCommandStride;
#if defined(DIRECT3D12)
	ID3D12CommandSignature* pDxCommandSignautre;
#endif
#if defined(VULKAN)
	IndirectArgumentType mDrawType;
#endif
#if defined(METAL)
	IndirectArgumentType mDrawType;
#endif
} CommandSignature;

typedef struct DescriptorSetDesc
{
	RootSignature*             pRootSignature;
	DescriptorUpdateFrequency  mUpdateFrequency;
	uint32_t                   mMaxSets;
	uint32_t                   mNodeIndex;
//    const wchar_t*             pDebugName;
} DescriptorSetDesc;


#define API_INTERFACE

// clang-format off
// API functions
// allocates memory and initializes the renderer -> returns pRenderer
//
API_INTERFACE void FORGE_CALLCONV initRenderer(const char* app_name, const RendererDesc* p_settings, Renderer** ppRenderer);
API_INTERFACE void FORGE_CALLCONV removeRenderer(Renderer* pRenderer);

API_INTERFACE void FORGE_CALLCONV addFence(Renderer* pRenderer, Fence** pp_fence);
API_INTERFACE void FORGE_CALLCONV removeFence(Renderer* pRenderer, Fence* p_fence);

API_INTERFACE void FORGE_CALLCONV addSemaphore(Renderer* pRenderer, Semaphore** pp_semaphore);
API_INTERFACE void FORGE_CALLCONV removeSemaphore(Renderer* pRenderer, Semaphore* p_semaphore);

API_INTERFACE void FORGE_CALLCONV addQueue(Renderer* pRenderer, QueueDesc* pQDesc, Queue** ppQueue);
API_INTERFACE void FORGE_CALLCONV removeQueue(Queue* pQueue);

API_INTERFACE void FORGE_CALLCONV addSwapChain(Renderer* pRenderer, const SwapChainDesc* p_desc, SwapChain** pp_swap_chain);
API_INTERFACE void FORGE_CALLCONV removeSwapChain(Renderer* pRenderer, SwapChain* p_swap_chain);

// command pool functions
API_INTERFACE void FORGE_CALLCONV addCmdPool(Renderer* pRenderer, Queue* p_queue, bool transient, CmdPool** pp_CmdPool);
API_INTERFACE void FORGE_CALLCONV removeCmdPool(Renderer* pRenderer, CmdPool* p_CmdPool);
API_INTERFACE void FORGE_CALLCONV addCmd(CmdPool* p_CmdPool, bool secondary, Cmd** pp_cmd);
API_INTERFACE void FORGE_CALLCONV removeCmd(CmdPool* p_CmdPool, Cmd* p_cmd);
API_INTERFACE void FORGE_CALLCONV addCmd_n(CmdPool* p_CmdPool, bool secondary, uint32_t cmd_count, Cmd*** ppp_cmd);
API_INTERFACE void FORGE_CALLCONV removeCmd_n(CmdPool* p_CmdPool, uint32_t cmd_count, Cmd** pp_cmd);

//
// All buffer, texture loading handled by resource system -> IResourceLoader.*
//

API_INTERFACE void FORGE_CALLCONV addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* p_desc, RenderTarget** pp_render_target);
API_INTERFACE void FORGE_CALLCONV removeRenderTarget(Renderer* pRenderer, RenderTarget* p_render_target);
API_INTERFACE void FORGE_CALLCONV addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** pp_sampler);
API_INTERFACE void FORGE_CALLCONV removeSampler(Renderer* pRenderer, Sampler* p_sampler);

// shader functions
#if defined(TARGET_IOS)
API_INTERFACE void FORGE_CALLCONV addShader(Renderer* pRenderer, const ShaderDesc* p_desc, Shader** p_shader_program);
#endif
API_INTERFACE void FORGE_CALLCONV addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* p_desc, Shader** p_shader_program);
API_INTERFACE void FORGE_CALLCONV removeShader(Renderer* pRenderer, Shader* p_shader_program);

API_INTERFACE void FORGE_CALLCONV addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootDesc, RootSignature** pp_root_signature);
API_INTERFACE void FORGE_CALLCONV removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature);

// pipeline functions
API_INTERFACE void FORGE_CALLCONV addPipeline(Renderer* pRenderer, const PipelineDesc* p_pipeline_settings, Pipeline** pp_pipeline); 
API_INTERFACE void FORGE_CALLCONV removePipeline(Renderer* pRenderer, Pipeline* p_pipeline);

// Descriptor Set functions
API_INTERFACE void FORGE_CALLCONV addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet);
API_INTERFACE void FORGE_CALLCONV removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet);
API_INTERFACE void FORGE_CALLCONV updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams);

/// Pipeline State Functions
API_INTERFACE void FORGE_CALLCONV addBlendState(Renderer* pRenderer, const BlendStateDesc* pDesc, BlendState** ppBlendState);
API_INTERFACE void FORGE_CALLCONV removeBlendState(BlendState* pBlendState);

API_INTERFACE void FORGE_CALLCONV addDepthState(Renderer* pRenderer, const DepthStateDesc* pDesc, DepthState** ppDepthState);
API_INTERFACE void FORGE_CALLCONV removeDepthState(DepthState* pDepthState);

API_INTERFACE void FORGE_CALLCONV addRasterizerState(Renderer* pRenderer, const RasterizerStateDesc* pDesc, RasterizerState** ppRasterizerState);
API_INTERFACE void FORGE_CALLCONV removeRasterizerState(RasterizerState* pRasterizerState);

// command buffer functions
API_INTERFACE void FORGE_CALLCONV beginCmd(Cmd* p_cmd);
API_INTERFACE void FORGE_CALLCONV endCmd(Cmd* p_cmd);
API_INTERFACE void FORGE_CALLCONV cmdBindRenderTargets(Cmd* p_cmd, uint32_t render_target_count, RenderTarget** pp_render_targets, RenderTarget* p_depth_stencil, const LoadActionsDesc* loadActions, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice, uint32_t depthMipSlice);
API_INTERFACE void FORGE_CALLCONV cmdSetViewport(Cmd* p_cmd, float x, float y, float width, float height, float min_depth, float max_depth);
API_INTERFACE void FORGE_CALLCONV cmdSetScissor(Cmd* p_cmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
API_INTERFACE void FORGE_CALLCONV cmdBindPipeline(Cmd* p_cmd, Pipeline* p_pipeline);
API_INTERFACE void FORGE_CALLCONV cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet);
API_INTERFACE void FORGE_CALLCONV cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, const char* pName, const void* pConstants);
API_INTERFACE void FORGE_CALLCONV cmdBindPushConstantsByIndex(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants);
API_INTERFACE void FORGE_CALLCONV cmdBindIndexBuffer(Cmd* p_cmd, Buffer* p_buffer, uint64_t offset);
API_INTERFACE void FORGE_CALLCONV cmdBindVertexBuffer(Cmd* p_cmd, uint32_t buffer_count, Buffer** pp_buffers, uint64_t* pOffsets);
API_INTERFACE void FORGE_CALLCONV cmdDraw(Cmd* p_cmd, uint32_t vertex_count, uint32_t first_vertex);
API_INTERFACE void FORGE_CALLCONV cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance);
API_INTERFACE void FORGE_CALLCONV cmdDrawIndexed(Cmd* p_cmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex);
API_INTERFACE void FORGE_CALLCONV cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
API_INTERFACE void FORGE_CALLCONV cmdDispatch(Cmd* p_cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);

// Transition Commands
API_INTERFACE void FORGE_CALLCONV cmdResourceBarrier(Cmd* p_cmd, uint32_t buffer_barrier_count, BufferBarrier* p_buffer_barriers, uint32_t texture_barrier_count, TextureBarrier* p_texture_barriers);

//
// All buffer, texture update handled by resource system -> IResourceLoader.*
//

// queue/fence/swapchain functions
API_INTERFACE void FORGE_CALLCONV acquireNextImage(Renderer* pRenderer, SwapChain* p_swap_chain, Semaphore* p_signal_semaphore, Fence* p_fence, uint32_t* p_image_index);
API_INTERFACE void FORGE_CALLCONV queueSubmit(Queue* p_queue, uint32_t cmd_count, Cmd** pp_cmds, Fence* pFence, uint32_t wait_semaphore_count, Semaphore** pp_wait_semaphores, uint32_t signal_semaphore_count, Semaphore** pp_signal_semaphores);
API_INTERFACE void FORGE_CALLCONV queuePresent(Queue* p_queue, SwapChain* p_swap_chain, uint32_t swap_chain_image_index, uint32_t wait_semaphore_count, Semaphore** pp_wait_semaphores);
API_INTERFACE void FORGE_CALLCONV waitQueueIdle(Queue* p_queue);
API_INTERFACE void FORGE_CALLCONV getFenceStatus(Renderer* pRenderer, Fence* p_fence, FenceStatus* p_fence_status);
API_INTERFACE void FORGE_CALLCONV waitForFences(Renderer* pRenderer, uint32_t fence_count, Fence** pp_fences);
API_INTERFACE void FORGE_CALLCONV toggleVSync(Renderer* pRenderer, SwapChain** ppSwapchain);

//Returns the recommended format for the swapchain.
//If true is passed for the hintHDR parameter, it will return an HDR format IF the platform supports it
//If false is passed or the platform does not support HDR a non HDR format is returned.
API_INTERFACE TinyImageFormat FORGE_CALLCONV getRecommendedSwapchainFormat(bool hintHDR);

//indirect Draw functions
API_INTERFACE void FORGE_CALLCONV addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* p_desc, CommandSignature** ppCommandSignature);
API_INTERFACE void FORGE_CALLCONV removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature);
API_INTERFACE void FORGE_CALLCONV cmdExecuteIndirect(Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset);
/************************************************************************/
// GPU Query Interface
/************************************************************************/
API_INTERFACE void FORGE_CALLCONV getTimestampFrequency(Queue* pQueue, double* pFrequency);
API_INTERFACE void FORGE_CALLCONV addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool);
API_INTERFACE void FORGE_CALLCONV removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool);
API_INTERFACE void FORGE_CALLCONV cmdResetQueryPool(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount);
API_INTERFACE void FORGE_CALLCONV cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery);
API_INTERFACE void FORGE_CALLCONV cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery);
API_INTERFACE void FORGE_CALLCONV cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount);
/************************************************************************/
// Stats Info Interface
/************************************************************************/
API_INTERFACE void FORGE_CALLCONV calculateMemoryStats(Renderer* pRenderer, char** stats);
API_INTERFACE void FORGE_CALLCONV freeMemoryStats(Renderer* pRenderer, char* stats);
/************************************************************************/
// Debug Marker Interface
/************************************************************************/
API_INTERFACE void FORGE_CALLCONV cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName);
API_INTERFACE void FORGE_CALLCONV cmdEndDebugMarker(Cmd* pCmd);
API_INTERFACE void FORGE_CALLCONV cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName);
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
API_INTERFACE void FORGE_CALLCONV setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName);
API_INTERFACE void FORGE_CALLCONV setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName);
/************************************************************************/
/************************************************************************/
// clang-format on

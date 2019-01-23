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
#include "../../Xbox/CommonXBOXOne_3/OS/XBoxHeaders.h"
#ifndef DIRECT3D12
#define DIRECT3D12
#endif
#elif defined(DIRECT3D12)
#include <d3d12.h>
#include <dxgi1_5.h>
#include <dxgidebug.h>
#endif
#if defined(VULKAN)
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#if defined(__ANDROID__)
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#elif defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR    //Use Xlib or Xcb as display server, defaults to Xlib
#endif
#include "../../Common_3/ThirdParty/OpenSource/volk/volk.h"
#endif
#if defined(METAL)
#import <MetalKit/MetalKit.h>
#endif

#if defined(VULKAN)
// Set this define to enable renderdoc layer
// NOTE: Setting this define will disable use of the khr dedicated allocation extension since it conflicts with the renderdoc capture layer
//#define USE_RENDER_DOC
#elif defined(DIRECT3D12)
//#define USE_PIX
#endif

#include "../OS/Image/Image.h"
#include "../ThirdParty/OpenSource/TinySTL/string.h"
#include "../ThirdParty/OpenSource/TinySTL/vector.h"
#include "../ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../OS/Interfaces/IOperatingSystem.h"
#include "../OS/Interfaces/IThread.h"

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
	MAX_BATCH_BARRIERS = 64,
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

typedef void (*LogFn)(LogType, const char*, const char*);

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
typedef struct Renderer             Renderer;
typedef struct Queue                Queue;
typedef struct Pipeline             Pipeline;
typedef struct ResidencyObject      ResidencyObject;
typedef struct ResidencySet         ResidencySet;
typedef struct BlendState           BlendState;
typedef struct ShaderReflectionInfo ShaderReflectionInfo;
typedef struct Shader               Shader;

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
	INDIRECT_UNORDERED_ACCESS_VIEW    // only for dx
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
#if defined(VULKAN)
	/// Subpass input (descriptor type only available in Vulkan)
	DESCRIPTOR_TYPE_INPUT_ATTACHMENT = (DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES << 1),
	DESCRIPTOR_TYPE_TEXEL_BUFFER = (DESCRIPTOR_TYPE_INPUT_ATTACHMENT << 1),
	DESCRIPTOR_TYPE_RW_TEXEL_BUFFER = (DESCRIPTOR_TYPE_TEXEL_BUFFER << 1),
#endif
} DescriptorType;
MAKE_ENUM_FLAG(uint32_t, DescriptorType)

typedef enum TextureDimension
{
	TEXTURE_DIM_UNDEFINED = 0,
	TEXTURE_DIM_1D,
	TEXTURE_DIM_2D,
	TEXTURE_DIM_3D,
	TEXTURE_DIM_1D_ARRAY,
	TEXTURE_DIM_2D_ARRAY,
	TEXTURE_DIM_CUBE,
} TextureDimension;
MAKE_ENUM_FLAG(uint32_t, TextureDimension)

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
	SHADER_STAGE_ALL_GRAPHICS = 0X00000003,
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
	SHADER_STAGE_ALL_GRAPHICS = 0X0000001F,
	SHADER_STAGE_HULL = SHADER_STAGE_TESC,
	SHADER_STAGE_DOMN = SHADER_STAGE_TESE,
	SHADER_STAGE_COUNT = 6,
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
	struct Buffer* pBuffer;
	ResourceState  mNewState;
	bool           mSplit;
} BufferBarrier;

typedef struct TextureBarrier
{
	struct Texture* pTexture;
	ResourceState   mNewState;
	bool            mSplit;
} TextureBarrier;

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

typedef struct QueryHeapDesc
{
	QueryType mType;
	uint32_t  mQueryCount;
	uint32_t  mNodeIndex;
} QueryHeapDesc;

typedef struct QueryDesc
{
	uint32_t mIndex;
} QueryDesc;

typedef struct QueryHeap
{
	QueryHeapDesc mDesc;
#if defined(DIRECT3D12)
	ID3D12QueryHeap* pDxQueryHeap;
#endif
#if defined(VULKAN)
	VkQueryPool pVkQueryPool;
#endif
#if defined(DIRECT3D11)
	ID3D11Query** ppDxQueries;
#endif
} QueryHeap;

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
	/// Set this to specify a counter buffer for this buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	struct Buffer* pCounterBuffer;
	/// Format of the buffer (applicable to typed storage buffers (Buffer<T>)
	ImageFormat::Enum mFormat;
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
	/// A unique id used for hashing buffers during resource binding
	uint64_t mBufferId;
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
	id<MTLBuffer> mtlBuffer;
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
	/// Internal image format
	ImageFormat::Enum mFormat;
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
	/// Set whether texture is srgb
	bool mSrgb;
	/// Is the texture CPU accessible (applicable on hardware supporting CPU mapped textures (UMA))
	bool mHostVisible;
} TextureDesc;

typedef struct Texture
{
	/// A unique id used for hashing textures during resource binding
	uint64_t mTextureId;
#if defined(DIRECT3D12)
	/// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to TEXTURE_USAGE_SAMPLED_IMAGE)
	D3D12_CPU_DESCRIPTOR_HANDLE  mDxSRVDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE* pDxUAVDescriptors;
	/// Native handle of the underlying resource
	ID3D12Resource* pDxResource;
	/// Contains resource allocation info such as parent heap, offset in heap
	struct ResourceAllocation* pDxAllocation;
#endif
#if defined(VULKAN)
	/// Opaque handle used by shaders for doing read/write operations on the texture
	VkImageView pVkSRVDescriptor;
	/// Opaque handle used by shaders for doing read/write operations on the texture
	VkImageView* pVkUAVDescriptors;
	/// Native handle of the underlying resource
	VkImage pVkImage;
	/// Device memory handle
	VkDeviceMemory pVkMemory;
	/// Contains resource allocation info such as parent heap, offset in heap
	struct VmaAllocation_T* pVkAllocation;
	/// Flags specifying which aspects (COLOR,DEPTH,STENCIL) are included in the pVkImageView
	VkImageAspectFlags mVkAspectMask;
#endif
#if defined(METAL)
	/// Contains resource allocation info such as parent heap, offset in heap
	struct ResourceAllocation* pMtlAllocation;
	/// Native handle of the underlying resource
	id<MTLTexture> mtlTexture;
	id<MTLTexture> __strong* pMtlUAVDescriptors;
	MTLPixelFormat           mtlPixelFormat;
	bool                     mIsCompressed;
#endif
#if defined(DIRECT3D11)
	ID3D11Resource*             pDxResource;
	ID3D11ShaderResourceView*   pDxSRVDescriptor;
	ID3D11UnorderedAccessView** pDxUAVDescriptors;
#endif
	/// Texture creation info
	TextureDesc mDesc;    //88
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
	ImageFormat::Enum mFormat;
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
	/// Set whether rendertarget is srgb
	bool mSrgb;
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
#endif
#if defined(TARGET_IOS)
	// A separate texture is needed for stencil rendering on iOS.
	Texture* pStencil;
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
	float       mMipLosBias;
	float       mMaxAnisotropy;
	CompareMode mCompareFunc;
} SamplerDesc;

typedef struct Sampler
{
	/// A unique id used for hashing samplers during resource binding
	uint64_t mSamplerId;
#if defined(DIRECT3D12)
	/// Description for creating the Sampler descriptor for ths sampler
	D3D12_SAMPLER_DESC mDxSamplerDesc;
	/// Descriptor handle of the Sampler in a CPU visible descriptor heap
	D3D12_CPU_DESCRIPTOR_HANDLE mDxSamplerHandle;
#endif
#if defined(VULKAN)
	/// Native handle of the underlying resource
	VkSampler pVkSampler;
	/// Description for creating the descriptor for this sampler
	VkDescriptorImageInfo mVkSamplerView;
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
#if defined(DIRECT3D12)
	D3D12_ROOT_PARAMETER_TYPE mDxType;
#endif
#if defined(VULKAN)
	VkDescriptorType   mVkType;
	VkShaderStageFlags mVkStages;
	uint32_t           mDynamicUniformIndex;
#endif
} DescriptorInfo;

typedef struct RootSignatureDesc
{
	Shader**     ppShaders;
	uint32_t     mShaderCount;
	uint32_t     mMaxBindlessTextures;
	const char** ppStaticSamplerNames;
	Sampler**    ppStaticSamplers;
	uint32_t     mStaticSamplerCount;
#if defined(VULKAN)
	const char** ppDynamicUniformBufferNames;
	uint32_t     mDynamicUniformBufferCount;
#endif
} RootSignatureDesc;

typedef struct RootSignature
{
	/// Number of descriptors declared in the root signature layout
	uint32_t mDescriptorCount;
	/// Array of all descriptors declared in the root signature layout
	DescriptorInfo* pDescriptors;
	/// Translates hash of descriptor name to descriptor index
	tinystl::unordered_map<uint32_t, uint32_t> pDescriptorNameToIndexMap;

	PipelineType mPipelineType;
#if defined(DIRECT3D12)
	uint32_t             mDxViewDescriptorTableRootIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t             mDxSamplerDescriptorTableRootIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t*            pDxViewDescriptorIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t             mDxViewDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t             mDxCumulativeViewDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t*            pDxSamplerDescriptorIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t             mDxSamplerDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t             mDxCumulativeSamplerDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t*            pDxRootDescriptorRootIndices;
	uint32_t*            pDxRootConstantRootIndices;
	uint32_t             mDxRootDescriptorCount;
	uint32_t             mDxRootConstantCount;
	ID3D12RootSignature* pDxRootSignature;
	ID3DBlob*            pDxSerializedRootSignatureString;
#endif
#if defined(VULKAN)
	VkDescriptorSetLayout mVkDescriptorSetLayouts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t*             pVkDescriptorIndices[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t              mVkDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t              mVkDynamicDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t              mVkCumulativeDescriptorCounts[DESCRIPTOR_UPDATE_FREQ_COUNT];
	VkPushConstantRange*  pVkPushConstantRanges;
	uint32_t              mVkPushConstantCount;

	VkPipelineLayout pPipelineLayout;
#endif
#if defined(METAL)
	Sampler**    ppStaticSamplers;
	uint32_t*    pStaticSamplerSlots;
	ShaderStage* pStaticSamplerStages;
	uint32_t     mStaticSamplerCount;
#endif
#if defined(DIRECT3D11)
	ID3D11SamplerState** ppStaticSamplers;
	uint32_t*            pStaticSamplerSlots;
	ShaderStage*         pStaticSamplerStages;
	uint32_t             mStaticSamplerCount;
#endif

	using ThreadLocalDescriptorManager = tinystl::unordered_map<ThreadID, struct DescriptorManager*>;

	/// Api specific binding manager
	ThreadLocalDescriptorManager pDescriptorManagerMap;
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
		uint32_t mUAVMipSlice;
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
		/// Constant data in system memory to be bound as root / push constant
		void* pRootConstant;
	};
	/// Number of resources in the descriptor(applies to array of textures, buffers,...)
	uint32_t mCount;
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
	uint32_t*            pBoundColorFormats;
	bool*                pBoundSrgbValues;
	uint32_t             mBoundDepthStencilFormat;
	uint32_t             mBoundRenderTargetCount;
	SampleCount          mBoundSampleCount;
	uint32_t             mBoundSampleQuality;
	uint32_t             mBoundWidth;
	uint32_t             mBoundHeight;
	uint32_t             mNodeIndex;
	uint64_t             mRenderPassHash;
#if defined(DIRECT3D12)
	// For now each command list will have its own allocator until we get the command allocator pool logic working
	ID3D12CommandAllocator*    pDxCmdAlloc;
	ID3D12GraphicsCommandList* pDxCmdList;
	D3D12_RESOURCE_BARRIER     pBatchBarriers[MAX_BATCH_BARRIERS];
	// Small ring buffer to be used for copying root constant data in case the root constant was converted to root cbv
	struct UniformRingBuffer*   pRootConstantRingBuffer;
	D3D12_CPU_DESCRIPTOR_HANDLE mViewCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mViewGpuHandle;
	uint64_t                    mViewPosition;
	D3D12_CPU_DESCRIPTOR_HANDLE mSamplerCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mSamplerGpuHandle;
	uint64_t                    mSamplerPosition;
	D3D12_CPU_DESCRIPTOR_HANDLE mTransientCBVs;
	uint64_t                    mTransientCBVPosition;
	uint32_t                    mBatchBarrierCount;
#endif
#if defined(VULKAN)
	VkCommandBuffer pVkCmdBuf;
	VkRenderPass    pVkActiveRenderPass;

	VkImageMemoryBarrier        pBatchImageMemoryBarriers[MAX_BATCH_BARRIERS];
	VkBufferMemoryBarrier       pBatchBufferMemoryBarriers[MAX_BATCH_BARRIERS];
	struct DescriptorStoreHeap* pDescriptorPool;
	uint32_t                    mBatchImageMemoryBarrierCount;
	uint32_t                    mBatchBufferMemoryBarrierCount;
#endif
#if defined(METAL)
	id<MTLCommandBuffer>         mtlCommandBuffer;
	id<MTLFence>                 mtlEncoderFence;    // Used to sync different types of encoders recording in the same Cmd.
	id<MTLRenderCommandEncoder>  mtlRenderEncoder;
	id<MTLComputeCommandEncoder> mtlComputeEncoder;
	id<MTLBlitCommandEncoder>    mtlBlitEncoder;
	MTLRenderPassDescriptor*     pRenderPassDesc;
	Shader*                      pShader;
	Buffer*                      selectedIndexBuffer;
	uint64_t                     mSelectedIndexBufferOffset;
	MTLPrimitiveType             selectedPrimitiveType;
	bool                         mRenderPassActive;
#endif
#if defined(DIRECT3D11)
	uint8_t* pDescriptorStructPool;
	uint8_t* pDescriptorNamePool;
	uint8_t* pDescriptorResourcesPool;
	uint64_t mDescriptorStructPoolOffset;
	uint64_t mDescriptorNamePoolOffset;
	uint64_t mDescriptorResourcePoolOffset;
	Buffer*  pRootConstantBuffer;
	Buffer*  pTransientConstantBuffer;
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
	id<MTLCommandQueue>  mtlCommandQueue;
	dispatch_semaphore_t pMtlSemaphore;
#endif
	QueueDesc mQueueDesc;
} Queue;

typedef struct ShaderMacro
{
	tinystl::string definition;
	tinystl::string value;
} ShaderMacro;

typedef struct RendererShaderDefinesDesc
{
	ShaderMacro* rendererShaderDefines;
	uint32_t     rendererShaderDefinesCnt;
} RendererShaderDefinesDesc;

#if defined(METAL)
typedef struct ShaderStageDesc
{
	tinystl::string              mName;
	tinystl::string              mCode;
	tinystl::string              mEntryPoint;
	tinystl::vector<ShaderMacro> mMacros;
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
	char*    pByteCode;
	uint32_t mByteCodeSize;
#if defined(METAL)
	// Shader source is needed for reflection
	tinystl::string mSource;
	/// Entry point is needed for Metal
	tinystl::string mEntryPoint;
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
	ShaderStage        mStages;
	PipelineReflection mReflection;
#if defined(DIRECT3D12)
	ID3DBlob** pShaderBlobs;
#endif
#if defined(VULKAN)
	VkShaderModule* pShaderModules;
#endif
#if defined(METAL)
	id<MTLFunction> mtlVertexShader;
	id<MTLFunction> mtlFragmentShader;
	id<MTLFunction> mtlComputeShader;
	uint32_t        mNumThreadsPerGroup[3];
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
	ImageFormat::Enum mFormat;
	uint32_t          mBinding;
	uint32_t          mLocation;
	uint32_t          mOffset;
	VertexAttribRate  mRate;

#ifdef FORGE_JHABLE_EDITS_V01
	uint32_t mSemanticType;
	uint32_t mSemanticIndex;
#endif
} VertexAttrib;

typedef struct VertexLayout
{
	uint32_t     mAttribCount;
	VertexAttrib mAttribs[MAX_VERTEX_ATTRIBS];
} VertexLayout;

typedef struct GraphicsPipelineDesc
{
	Shader*            pShaderProgram;
	RootSignature*     pRootSignature;
	VertexLayout*      pVertexLayout;
	BlendState*        pBlendState;
	DepthState*        pDepthState;
	RasterizerState*   pRasterizerState;
	ImageFormat::Enum* pColorFormats;
	bool*              pSrgbValues;
	uint32_t           mRenderTargetCount;
	SampleCount        mSampleCount;
	uint32_t           mSampleQuality;
	ImageFormat::Enum  mDepthStencilFormat;
	PrimitiveTopology  mPrimitiveTopo;
} GraphicsPipelineDesc;

typedef struct ComputePipelineDesc
{
	Shader*        pShaderProgram;
	RootSignature* pRootSignature;
} ComputePipelineDesc;

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
#endif
#if defined(VULKAN)
	VkPipeline pVkPipeline;
#endif
#if defined(METAL)
	RenderTarget*               pRenderPasspRenderTarget;
	Shader*                     pShader;
	id<MTLRenderPipelineState>  mtlRenderPipelineState;
	id<MTLComputePipelineState> mtlComputePipelineState;
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
#if defined(DIRECT3D12) || defined(METAL) || defined(DIRECT3D11)
	uint32_t mRowPitch;
	uint32_t mSlicePitch;
	void*    pData;
#endif
#if defined(VULKAN)
	uint32_t mMipLevel;
	uint32_t mArrayLayer;
	uint32_t mWidth;
	uint32_t mHeight;
	uint32_t mDepth;
	uint32_t mArraySize;
	uint64_t mBufferOffset;
#endif
} SubresourceDataDesc;

typedef struct SwapChainDesc
{
	/// Window handle
	WindowsDesc* pWindow;
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
	ImageFormat::Enum mColorFormat;
	/// Clear value
	ClearValue mColorClearValue;
	/// Set whether this swapchain using srgb color space
	bool mSrgb;
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
	uint32_t         mImageIndex;
	uint32_t         mFlags;
#elif defined(DIRECT3D12)
	/// Use IDXGISwapChain3 for now since IDXGISwapChain4
	/// isn't supported by older devices.
	IDXGISwapChain3* pDxSwapChain;
	/// Sync interval to specify how interval for vsync
	UINT             mDxSyncInterval;
	ID3D12Resource** ppDxSwapChainResources;
	uint32_t         mImageIndex;
	uint32_t         mFlags;
#endif
#if defined(DIRECT3D11)
	/// Use IDXGISwapChain3 for now since IDXGISwapChain4
	/// isn't supported by older devices.
	IDXGISwapChain* pDxSwapChain;
	/// Sync interval to specify how interval for vsync
	UINT             mDxSyncInterval;
	ID3D11Resource** ppDxSwapChainResources;
	uint32_t         mImageIndex;
	uint32_t         mFlags;
#endif
#if defined(VULKAN)
	/// Present queue if one exists (queuePresent will use this queue if the hardware has a dedicated present queue)
	VkQueue        pPresentQueue;
	VkSwapchainKHR pSwapChain;
	VkSurfaceKHR   pVkSurface;
	VkImage*       ppVkSwapChainImages;
#endif
#if defined(METAL)
	MTKView*             pMTKView;
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
#endif
} DXShaderTarget;

typedef enum GpuMode
{
	GPU_MODE_SINGLE = 0,
	GPU_MODE_LINKED,
	// #TODO GPU_MODE_UNLINKED,
} GpuMode;

typedef struct RendererDesc
{
	LogFn        pLogFn;
	RendererApi  mApi;
	ShaderTarget mShaderTarget;
	GpuMode      mGpuMode;
#if defined(VULKAN)
	tinystl::vector<tinystl::string> mInstanceLayers;
	tinystl::vector<tinystl::string> mInstanceExtensions;
	tinystl::vector<tinystl::string> mDeviceExtensions;
	PFN_vkDebugReportCallbackEXT     pVkDebugFn;
#endif
#if defined(DIRECT3D12)
	D3D_FEATURE_LEVEL mDxFeatureLevel;
#endif
} RendererDesc;

typedef struct GPUVendorPreset
{
	char           mVendorId[MAX_GPU_VENDOR_STRING_LENGTH];
	char           mModelId[MAX_GPU_VENDOR_STRING_LENGTH];
	char           mRevisionId[MAX_GPU_VENDOR_STRING_LENGTH];    // OPtional as not all gpu's have that. Default is : 0x00
	GPUPresetLevel mPresetLevel;
	char           mGpuName[MAX_GPU_VENDOR_STRING_LENGTH];    //If GPU Name is missing then value will be empty string
} GPUVendorPreset;

typedef struct GPUSettings
{
	uint64_t        mUniformBufferAlignment;
	uint32_t        mMaxVertexInputBindings;
	bool            mMultiDrawIndirect;
	uint32_t        mMaxRootSignatureDWORDS;
	uint32_t        mWaveLaneCount;
	bool            mROVsSupported;
	GPUVendorPreset mGpuVendorPreset;
} GPUSettings;

typedef struct Renderer
{
	char*        pName;
	RendererDesc mSettings;
	uint32_t     mNumOfGPUs;
	GPUSettings* pActiveGpuSettings;
	GPUSettings  mGpuSettings[MAX_GPUS];
#if defined(_DURANGO)
	IDXGIFactory2* pDXGIFactory;
	IDXGIAdapter*  pDxGPUs[MAX_GPUS];
	IDXGIAdapter*  pDxActiveGPU;
	ID3D12Device*  pDxDevice;
	EsramManager*  pESRAMManager;
#if defined(_DEBUG) || defined(PROFILE)
	ID3D12Debug* pDXDebug;
#endif
	// Default NULL Descriptors for binding at empty descriptor slots to make sure all descriptors are bound at submit
	D3D12_CPU_DESCRIPTOR_HANDLE mSrvNullDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE mUavNullDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE mCbvNullDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE mSamplerNullDescriptor;

	// API specific descriptor heap and memory allocator
	struct DescriptorStoreHeap* pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	struct DescriptorStoreHeap* pCbvSrvUavHeap[MAX_GPUS];
	struct DescriptorStoreHeap* pSamplerHeap[MAX_GPUS];
	struct ResourceAllocator*   pResourceAllocator;
	uint32_t                    mDxLinkedNodeCount;
#elif defined(DIRECT3D12)
	IDXGIFactory5* pDXGIFactory;
	IDXGIAdapter3* pDxGPUs[MAX_GPUS];
	IDXGIAdapter3* pDxActiveGPU;
	ID3D12Device*  pDxDevice;
#if defined(_DEBUG)
	ID3D12Debug*   pDXDebug;
#endif
	// Default NULL Descriptors for binding at empty descriptor slots to make sure all descriptors are bound at submit
	D3D12_CPU_DESCRIPTOR_HANDLE mSrvNullDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE mUavNullDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE mCbvNullDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE mSamplerNullDescriptor;

	// API specific descriptor heap and memory allocator
	struct DescriptorStoreHeap* pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	struct DescriptorStoreHeap* pCbvSrvUavHeap[MAX_GPUS];
	struct DescriptorStoreHeap* pSamplerHeap[MAX_GPUS];
	struct ResourceAllocator*   pResourceAllocator;
	uint32_t                    mDxLinkedNodeCount;
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
	VkPhysicalDeviceMemoryProperties  mVkGpuMemoryProperties[MAX_GPUS];
	VkPhysicalDeviceFeatures          mVkGpuFeatures[MAX_GPUS];
	uint32_t                          mVkQueueFamilyPropertyCount[MAX_GPUS];
	VkQueueFamilyProperties*          mVkQueueFamilyProperties[MAX_GPUS];
	uint32_t                          mActiveGPUIndex;
	VkPhysicalDeviceMemoryProperties* pVkActiveGpuMemoryProperties;
	VkPhysicalDeviceProperties2*      pVkActiveGPUProperties;
	VkDevice                          pVkDevice;
#ifdef USE_DEBUG_UTILS_EXTENSION
	VkDebugUtilsMessengerEXT pVkDebugUtilsMessenger;
#endif
	VkDebugReportCallbackEXT     pVkDebugReport;
	tinystl::vector<const char*> mInstanceLayers;
	uint32_t                     mVkUsedQueueCount[MAX_GPUS][16];
	uint32_t                     mVkLinkedNodeCount;

	Texture* pDefaultTexture1DSRV;
	Texture* pDefaultTexture1DUAV;
	Texture* pDefaultTexture1DArraySRV;
	Texture* pDefaultTexture1DArrayUAV;
	Texture* pDefaultTexture2DSRV;
	Texture* pDefaultTexture2DUAV;
	Texture* pDefaultTexture2DArraySRV;
	Texture* pDefaultTexture2DArrayUAV;
	Texture* pDefaultTexture3DSRV;
	Texture* pDefaultTexture3DUAV;
	Texture* pDefaultTextureCubeSRV;
	Buffer*  pDefaultBuffer;
	Sampler* pDefaultSampler;

	struct VmaAllocator_T*      pVmaAllocator;
	struct DescriptorStoreHeap* pDescriptorPool;

	// These are the extensions that we have loaded
	const char* gVkInstanceExtensions[MAX_INSTANCE_EXTENSIONS];
	// These are the extensions that we have loaded
	const char* gVkDeviceExtensions[MAX_DEVICE_EXTENSIONS];
#endif
#if defined(METAL)
	id<MTLDevice>             pDevice;
	struct ResourceAllocator* pResourceAllocator;
#endif

	// Default states used if user does not specify them in pipeline creation
	BlendState*      pDefaultBlendState;
	DepthState*      pDefaultDepthState;
	RasterizerState* pDefaultRasterizerState;
} Renderer;

// Indirect command sturcture define
typedef struct IndirectArgumentDescriptor
{
	IndirectArgumentType mType;
	uint32_t             mRootParameterIndex;
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

#define API_INTERFACE

// clang-format off
#if !defined(ENABLE_RENDERER_RUNTIME_SWITCH)
// API functions
// allocates memory and initializes the renderer -> returns pRenderer
//
API_INTERFACE void CALLTYPE initRenderer(const char* app_name, const RendererDesc* p_settings, Renderer** ppRenderer);
API_INTERFACE void CALLTYPE removeRenderer(Renderer* pRenderer);

API_INTERFACE void CALLTYPE addFence(Renderer* pRenderer, Fence** pp_fence);
API_INTERFACE void CALLTYPE removeFence(Renderer* pRenderer, Fence* p_fence);

API_INTERFACE void CALLTYPE addSemaphore(Renderer* pRenderer, Semaphore** pp_semaphore);
API_INTERFACE void CALLTYPE removeSemaphore(Renderer* pRenderer, Semaphore* p_semaphore);

API_INTERFACE void CALLTYPE addQueue(Renderer* pRenderer, QueueDesc* pQDesc, Queue** ppQueue);
API_INTERFACE void CALLTYPE removeQueue(Queue* pQueue);

API_INTERFACE void CALLTYPE addSwapChain(Renderer* pRenderer, const SwapChainDesc* p_desc, SwapChain** pp_swap_chain);
API_INTERFACE void CALLTYPE removeSwapChain(Renderer* pRenderer, SwapChain* p_swap_chain);

// command pool functions
API_INTERFACE void CALLTYPE addCmdPool(Renderer* pRenderer, Queue* p_queue, bool transient, CmdPool** pp_CmdPool);
API_INTERFACE void CALLTYPE removeCmdPool(Renderer* pRenderer, CmdPool* p_CmdPool);
API_INTERFACE void CALLTYPE addCmd(CmdPool* p_CmdPool, bool secondary, Cmd** pp_cmd);
API_INTERFACE void CALLTYPE removeCmd(CmdPool* p_CmdPool, Cmd* p_cmd);
API_INTERFACE void CALLTYPE addCmd_n(CmdPool* p_CmdPool, bool secondary, uint32_t cmd_count, Cmd*** ppp_cmd);
API_INTERFACE void CALLTYPE removeCmd_n(CmdPool* p_CmdPool, uint32_t cmd_count, Cmd** pp_cmd);

//
// All buffer, texture loading handled by resource system -> IResourceLoader.*
//

API_INTERFACE void CALLTYPE addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* p_desc, RenderTarget** pp_render_target);
API_INTERFACE void CALLTYPE removeRenderTarget(Renderer* pRenderer, RenderTarget* p_render_target);
API_INTERFACE void CALLTYPE addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** pp_sampler);
API_INTERFACE void CALLTYPE removeSampler(Renderer* pRenderer, Sampler* p_sampler);

// shader functions
#if defined(METAL)
API_INTERFACE void CALLTYPE addShader(Renderer* pRenderer, const ShaderDesc* p_desc, Shader** p_shader_program);
#endif
API_INTERFACE void CALLTYPE addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* p_desc, Shader** p_shader_program);
API_INTERFACE void CALLTYPE removeShader(Renderer* pRenderer, Shader* p_shader_program);

// pipeline functions
API_INTERFACE void CALLTYPE addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootDesc, RootSignature** pp_root_signature);
API_INTERFACE void CALLTYPE removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature);
API_INTERFACE void CALLTYPE addPipeline(Renderer* pRenderer, const GraphicsPipelineDesc* p_pipeline_settings, Pipeline** pp_pipeline);
API_INTERFACE void CALLTYPE addComputePipeline(Renderer* pRenderer, const ComputePipelineDesc* p_pipeline_settings, Pipeline** p_pipeline);
API_INTERFACE void CALLTYPE removePipeline(Renderer* pRenderer, Pipeline* p_pipeline);

/// Pipeline State Functions
API_INTERFACE void CALLTYPE addBlendState(Renderer* pRenderer, const BlendStateDesc* pDesc, BlendState** ppBlendState);
API_INTERFACE void CALLTYPE removeBlendState(BlendState* pBlendState);

API_INTERFACE void CALLTYPE addDepthState(Renderer* pRenderer, const DepthStateDesc* pDesc, DepthState** ppDepthState);
API_INTERFACE void CALLTYPE removeDepthState(DepthState* pDepthState);

API_INTERFACE void CALLTYPE addRasterizerState(Renderer* pRenderer, const RasterizerStateDesc* pDesc, RasterizerState** ppRasterizerState);
API_INTERFACE void CALLTYPE removeRasterizerState(RasterizerState* pRasterizerState);

// command buffer functions
API_INTERFACE void CALLTYPE beginCmd(Cmd* p_cmd);
API_INTERFACE void CALLTYPE endCmd(Cmd* p_cmd);
API_INTERFACE void CALLTYPE cmdBindRenderTargets(Cmd* p_cmd, uint32_t render_target_count, RenderTarget** pp_render_targets, RenderTarget* p_depth_stencil, const LoadActionsDesc* loadActions, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice, uint32_t depthMipSlice);
API_INTERFACE void CALLTYPE cmdSetViewport(Cmd* p_cmd, float x, float y, float width, float height, float min_depth, float max_depth);
API_INTERFACE void CALLTYPE cmdSetScissor(Cmd* p_cmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
API_INTERFACE void CALLTYPE cmdBindPipeline(Cmd* p_cmd, Pipeline* p_pipeline);
API_INTERFACE void CALLTYPE cmdBindDescriptors(Cmd* pCmd, RootSignature* pRootSignature, uint32_t numDescriptors, DescriptorData* pDescParams);
API_INTERFACE void CALLTYPE cmdBindIndexBuffer(Cmd* p_cmd, Buffer* p_buffer, uint64_t offset);
API_INTERFACE void CALLTYPE cmdBindVertexBuffer(Cmd* p_cmd, uint32_t buffer_count, Buffer** pp_buffers, uint64_t* pOffsets);
API_INTERFACE void CALLTYPE cmdDraw(Cmd* p_cmd, uint32_t vertex_count, uint32_t first_vertex);
API_INTERFACE void CALLTYPE cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance);
API_INTERFACE void CALLTYPE cmdDrawIndexed(Cmd* p_cmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex);
API_INTERFACE void CALLTYPE cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
API_INTERFACE void CALLTYPE cmdDispatch(Cmd* p_cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);

// Transition Commands
API_INTERFACE void CALLTYPE cmdResourceBarrier(Cmd* p_cmd, uint32_t buffer_barrier_count, BufferBarrier* p_buffer_barriers, uint32_t texture_barrier_count, TextureBarrier* p_texture_barriers, bool batch);
API_INTERFACE void CALLTYPE cmdSynchronizeResources(Cmd* p_cmd, uint32_t buffer_count, Buffer** p_buffers, uint32_t texture_count, Texture** p_textures, bool batch);
/// Flushes all the batched transitions requested in cmdResourceBarrier
API_INTERFACE void CALLTYPE cmdFlushBarriers(Cmd* p_cmd);

//
// All buffer, texture update handled by resource system -> IResourceLoader.*
//

// queue/fence/swapchain functions
API_INTERFACE void CALLTYPE acquireNextImage(Renderer* pRenderer, SwapChain* p_swap_chain, Semaphore* p_signal_semaphore, Fence* p_fence, uint32_t* p_image_index);
API_INTERFACE void CALLTYPE queueSubmit(Queue* p_queue, uint32_t cmd_count, Cmd** pp_cmds, Fence* pFence, uint32_t wait_semaphore_count, Semaphore** pp_wait_semaphores, uint32_t signal_semaphore_count, Semaphore** pp_signal_semaphores);
API_INTERFACE void CALLTYPE queuePresent(Queue* p_queue, SwapChain* p_swap_chain, uint32_t swap_chain_image_index, uint32_t wait_semaphore_count, Semaphore** pp_wait_semaphores);
API_INTERFACE void CALLTYPE getFenceStatus(Renderer* pRenderer, Fence* p_fence, FenceStatus* p_fence_status);
API_INTERFACE void CALLTYPE waitForFences(Queue* p_queue, uint32_t fence_count, Fence** pp_fences, bool signal);
API_INTERFACE void CALLTYPE toggleVSync(Renderer* pRenderer, SwapChain** ppSwapchain);

// image related functions
API_INTERFACE bool CALLTYPE isImageFormatSupported(ImageFormat::Enum format);
//Returns the recommended format for the swapchain.
//If true is passed for the hintHDR parameter, it will return an HDR format IF the platform supports it
//If false is passed or the platform does not support HDR a non HDR format is returned.
API_INTERFACE ImageFormat::Enum CALLTYPE getRecommendedSwapchainFormat(bool hintHDR);

//indirect Draw functions
API_INTERFACE void CALLTYPE addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* p_desc, CommandSignature** ppCommandSignature);
API_INTERFACE void CALLTYPE removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature);
API_INTERFACE void CALLTYPE cmdExecuteIndirect(Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset);
/************************************************************************/
// GPU Query Interface
/************************************************************************/
API_INTERFACE void CALLTYPE getTimestampFrequency(Queue* pQueue, double* pFrequency);
API_INTERFACE void CALLTYPE addQueryHeap(Renderer* pRenderer, const QueryHeapDesc* pDesc, QueryHeap** ppQueryHeap);
API_INTERFACE void CALLTYPE removeQueryHeap(Renderer* pRenderer, QueryHeap* pQueryHeap);
API_INTERFACE void CALLTYPE cmdBeginQuery(Cmd* pCmd, QueryHeap* pQueryHeap, QueryDesc* pQuery);
API_INTERFACE void CALLTYPE cmdEndQuery(Cmd* pCmd, QueryHeap* pQueryHeap, QueryDesc* pQuery);
API_INTERFACE void CALLTYPE cmdResolveQuery(Cmd* pCmd, QueryHeap* pQueryHeap, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount);
/************************************************************************/
// Stats Info Interface
/************************************************************************/
API_INTERFACE void CALLTYPE calculateMemoryStats(Renderer* pRenderer, char** stats);
API_INTERFACE void CALLTYPE freeMemoryStats(Renderer* pRenderer, char* stats);
/************************************************************************/
// Debug Marker Interface
/************************************************************************/
API_INTERFACE void CALLTYPE cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName);
API_INTERFACE void CALLTYPE cmdEndDebugMarker(Cmd* pCmd);
API_INTERFACE void CALLTYPE cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName);
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
API_INTERFACE void CALLTYPE setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName);
API_INTERFACE void CALLTYPE setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName);
/************************************************************************/
/************************************************************************/
#else
// DLL Interface must be generated using the ExportRendererDLLFunctions.py script under TheForge/Tools/
#include "IRendererDLL.h"
#endif
// clang-format on

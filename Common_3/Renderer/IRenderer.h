/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

#if defined(_DURANGO)
#include "../../CommonXBOXOne_3/OS/XBoxHeaders.h"
#ifndef DIRECT3D12
#define DIRECT3D12
#endif
#elif defined(DIRECT3D12)
#include <d3d12.h>
#include <dxgi1_5.h>
#include <dxgidebug.h>
#elif defined(VULKAN)
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>
#elif defined(METAL)
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

#define MAKE_ENUM_FLAG(TYPE,ENUM_TYPE) \
static inline ENUM_TYPE operator|(ENUM_TYPE a, ENUM_TYPE b) \
{ \
	return (ENUM_TYPE)((TYPE)(a) | (TYPE)(b)); \
} \
static inline ENUM_TYPE operator&(ENUM_TYPE a, ENUM_TYPE b) \
{ \
	return (ENUM_TYPE)((TYPE)(a) & (TYPE)(b)); \
} \
static inline ENUM_TYPE operator|=(ENUM_TYPE& a, ENUM_TYPE b) \
{ \
	return a = (a | b); \
} \
static inline ENUM_TYPE operator&=(ENUM_TYPE& a, ENUM_TYPE b) \
{ \
	return a = (a & b); \
} \

#endif
#else
#define MAKE_ENUM_FLAG(TYPE,ENUM_TYPE)
#endif

//
// default capability levels of the renderer
//
#if ! defined(RENDERER_CUSTOM_MAX)
enum {
	MAX_INSTANCE_EXTENSIONS = 64,
	MAX_DEVICE_EXTENSIONS = 64,
	MAX_GPUS = 4,
	MAX_RENDER_TARGET_ATTACHMENTS = 8,
	MAX_SUBMIT_CMDS = 20,					// max number of command lists / command buffers
	MAX_SUBMIT_WAIT_SEMAPHORES = 8,
	MAX_SUBMIT_SIGNAL_SEMAPHORES = 8,
	MAX_PRESENT_WAIT_SEMAPHORES = 8,
	MAX_VERTEX_BINDINGS = 15,
	MAX_VERTEX_ATTRIBS = 15,
	MAX_SEMANTIC_NAME_LENGTH = 128,
	MAX_MIP_LEVELS = 0xFFFFFFFF,
	MAX_BATCH_BARRIERS = 64,
};
#endif

typedef enum LogType {
	LOG_TYPE_INFO = 0,
	LOG_TYPE_WARN,
	LOG_TYPE_DEBUG,
	LOG_TYPE_ERROR
} LogType;

typedef enum CmdPoolType {
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
}QueueFlag;

typedef enum QueuePriority
{
	QUEUE_PRIORITY_NORMAL,
	QUEUE_PRIORITY_HIGH,
	QUEUE_PRIORITY_GLOBAL_REALTIME,
	MAX_QUEUE_PRIORITY
}QueuePriority;

typedef enum LoadActionType
{
    LOAD_ACTION_DONTCARE,
    LOAD_ACTION_LOAD,
    LOAD_ACTION_CLEAR
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

typedef enum BufferUsage {
	BUFFER_USAGE_UPLOAD = 0X00000001,
	BUFFER_USAGE_UNIFORM = 0X00000002,
    BUFFER_USAGE_STORAGE_SRV = 0X00000010,
	BUFFER_USAGE_STORAGE_UAV = 0X00000020,
	BUFFER_USAGE_INDEX = 0X00000040,
	BUFFER_USAGE_VERTEX = 0X00000080,
	BUFFER_USAGE_INDIRECT = 0x00000100,
} BufferUsage;
MAKE_ENUM_FLAG(uint32_t, BufferUsage);

typedef enum BufferFeatureFlags {
	BUFFER_FEATURE_NONE = 0X00000000,
	BUFFER_FEATURE_RAW = 0x00000002
} BufferFeatureFlags;
MAKE_ENUM_FLAG(uint32_t, BufferFeatureFlags);

typedef enum TextureType {
	TEXTURE_TYPE_UNDEFINED = 0,
	TEXTURE_TYPE_1D,
	TEXTURE_TYPE_2D,
	TEXTURE_TYPE_3D,
	TEXTURE_TYPE_CUBE,
} TextureType;

typedef enum RenderTargetType {
	RENDER_TARGET_TYPE_UNKNOWN = 0,
	RENDER_TARGET_TYPE_1D,
	RENDER_TARGET_TYPE_2D,
	RENDER_TARGET_TYPE_3D,
	RENDER_TARGET_TYPE_BUFFER,
} RenderTargetType;

typedef enum TextureUsage {
	TEXTURE_USAGE_UNDEFINED = 0x00,
	TEXTURE_USAGE_SAMPLED_IMAGE = 0x01,
	TEXTURE_USAGE_UNORDERED_ACCESS = 0x02
} TextureUsage;
MAKE_ENUM_FLAG(uint32_t, TextureUsage)

typedef enum RenderTargetUsage {
	RENDER_TARGET_USAGE_UNDEFINED = 0x00,
	RENDER_TARGET_USAGE_COLOR = 0x01,
	RENDER_TARGET_USAGE_DEPTH_STENCIL = 0x02,
	RENDER_TARGET_USAGE_UNORDERED_ACCESS = 0x04,
} RenderTargetUsage;
MAKE_ENUM_FLAG(uint32_t, RenderTargetUsage)

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
typedef struct Renderer Renderer;
typedef struct Queue        Queue;
typedef struct Pipeline     Pipeline;
typedef struct ResidencyObject ResidencyObject;
typedef struct ResidencySet ResidencySet;
typedef struct BlendState   BlendState;
typedef struct ShaderReflectionInfo ShaderReflectionInfo;
typedef struct Shader       Shader;

typedef struct IndirectDrawArguments
{
	uint32_t mVertexCount;
	uint32_t mInstanceCount;
	uint32_t mStartVertex;
	uint32_t mStartInstance;
}IndirectDrawArguments;

typedef struct IndirectDrawIndexArguments
{
	uint32_t mIndexCount;
	uint32_t mInstanceCount;
	uint32_t mStartIndex;
	uint32_t mVertexOffset;
	uint32_t mStartInstance;
}IndirectDrawIndexArguments;

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
  INDIRECT_DESCRIPTOR_TABLE,  // only for vulkan
  INDIRECT_PIPELINE, // only for vulkan now, probally will add to dx when it comes to xbox 
  INDIRECT_CONSTANT_BUFFER_VIEW, // only for dx
  INDIRECT_SHADER_RESOURCE_VIEW, // only for dx
  INDIRECT_UNORDERED_ACCESS_VIEW // only for dx
}IndirectArgumentType;
/************************************************/

typedef enum DescriptorType {
	DESCRIPTOR_TYPE_UNDEFINED = 0,
	DESCRIPTOR_TYPE_SAMPLER,
	// SRV Read only texture
	DESCRIPTOR_TYPE_TEXTURE,
	/// UAV Texture
	DESCRIPTOR_TYPE_RW_TEXTURE,
	// SRV Read only buffer
	DESCRIPTOR_TYPE_BUFFER,
	/// UAV Buffer
	DESCRIPTOR_TYPE_RW_BUFFER,
	/// Uniform buffer data stays same throughout the frame
	DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	/// Push constant / Root constant
	DESCRIPTOR_TYPE_ROOT_CONSTANT,
#if defined(VULKAN)
	/// Subpass input (descriptor type only available in Vulkan)
    DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
#endif
	DESCRIPTOR_TYPE_COUNT,
} DescriptorType;

typedef enum SampleCount {
	SAMPLE_COUNT_1 = 1,
	SAMPLE_COUNT_2 = 2,
	SAMPLE_COUNT_4 = 4,
	SAMPLE_COUNT_8 = 8,
	SAMPLE_COUNT_16 = 16,
} SampleCount;

#ifdef METAL
typedef enum ShaderStage {
    SHADER_STAGE_NONE = 0,
    SHADER_STAGE_VERT = 0X00000001,
    SHADER_STAGE_FRAG = 0X00000002,
    SHADER_STAGE_COMP = 0X00000004,
    SHADER_STAGE_ALL_GRAPHICS = 0X00000003,
    SHADER_STAGE_COUNT = 3,
} ShaderStage;
#else
typedef enum ShaderStage {
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

typedef enum PrimitiveTopology {
	PRIMITIVE_TOPO_POINT_LIST = 0,
	PRIMITIVE_TOPO_LINE_LIST,
	PRIMITIVE_TOPO_LINE_STRIP,
	PRIMITIVE_TOPO_TRI_LIST,
	PRIMITIVE_TOPO_TRI_STRIP,
	PRIMITIVE_TOPO_PATCH_LIST,
	PRIMITIVE_TOPO_COUNT,
} PrimitiveTopology;

typedef enum IndexType {
	INDEX_TYPE_UINT32 = 0,
	INDEX_TYPE_UINT16,
} IndexType;

typedef enum ShaderSemantic {
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
typedef enum eBlendStateMRTTargets
{
	eBlendStateMRTRenderTarget0 = 0x1,
	eBlendStateMRTRenderTarget1 = 0x2,
	eBlendStateMRTRenderTarget2 = 0x4,
	eBlendStateMRTRenderTarget3 = 0x8,
	eBlendStateMRTRenderTarget4 = 0x10,
	eBlendStateMRTRenderTarget5 = 0x20,
	eBlendStateMRTRenderTarget6 = 0x40,
	eBlendStateMRTRenderTarget7 = 0x80,
} eBlendStateMRTTargets;

typedef enum CullMode {
	CULL_MODE_NONE = 0,
	CULL_MODE_BACK,
	CULL_MODE_FRONT,
	CULL_MODE_BOTH,
	MAX_CULL_MODES
} CullMode;

typedef enum FrontFace {
	FRONT_FACE_CCW = 0,
	FRONT_FACE_CW
} FrontFace;

typedef enum FillMode
{
	FILL_MODE_SOLID,
	FILL_MODE_WIREFRAME,
	MAX_FILL_MODES
} FillMode;

typedef enum PipelineType {
	PIPELINE_TYPE_UNDEFINED = 0,
	PIPELINE_TYPE_COMPUTE,
	PIPELINE_TYPE_GRAPHICS,
	PIPELINE_TYPE_COUNT,
} PipelineType;

typedef enum FilterType {
	FILTER_NEAREST = 0,
	FILTER_LINEAR,
	FILTER_BILINEAR,
	FILTER_TRILINEAR,
	FILTER_BILINEAR_ANISO,
	FILTER_TRILINEAR_ANISO,
} FilterType;

typedef enum AddressMode {
	ADDRESS_MODE_MIRROR,
	ADDRESS_MODE_REPEAT,
	ADDRESS_MODE_CLAMP_TO_EDGE,
	ADDRESS_MODE_CLAMP_TO_BORDER
} AddressMode;

typedef enum MipMapMode
{
	MIPMAP_MODE_NEAREST = 0,
	MIPMAP_MODE_LINEAR
}MipMapMode;

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

typedef struct BufferBarrier
{
	struct Buffer*	pBuffer;
	ResourceState	mNewState;
	bool			mSplit;
} BufferBarrier;

typedef struct TextureBarrier
{
	struct Texture*	pTexture;
	ResourceState	mNewState;
	bool			mSplit;
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
	QueryType	mType;
	uint32_t	mQueryCount;
} QueryHeapDesc;

typedef struct QueryDesc
{
	uint32_t	mIndex;
} QueryDesc;

typedef struct QueryHeap
{
	QueryHeapDesc		mDesc;
#if defined(DIRECT3D12)
	ID3D12QueryHeap*	pDxQueryHeap;
#elif defined(VULKAN)
	VkQueryPool			pVkQueryPool;
#elif defined(METAL)
#endif
} QueryHeap;

/// Data structure holding necessary info to create a Buffer
typedef struct BufferDesc
{
	/// Flags specifying the suitable usage of this buffer (Uniform buffer, Vertex Buffer, Index Buffer,...)
	BufferUsage							mUsage;
	/// Size of the buffer (in bytes)
	uint64_t                            mSize;
	/// Decides which memory heap buffer will use (default, upload, readback)
	ResourceMemoryUsage					mMemoryUsage;
	/// Creation flags of the buffer
	BufferCreationFlags					mFlags;
	/// What state will the buffer get created in
	ResourceState						mStartState;
	/// Specifies whether the buffer will have 32 bit or 16 bit indices (applicable to BUFFER_USAGE_INDEX)
	IndexType							mIndexType;
	/// Vertex stride of the buffer (applicable to BUFFER_USAGE_VERTEX)
	uint32_t							mVertexStride;
	/// Index of the first element accessible by the SRV/UAV (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	uint64_t							mFirstElement;
	/// Number of elements in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	uint64_t							mElementCount;
	/// Size of each element (in bytes) in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	uint64_t							mStructStride;
	/// Set this to specify a counter buffer for this buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	struct Buffer*						pCounterBuffer;
	/// Format of the buffer (applicable to typed storage buffers (Buffer<T>)
	ImageFormat::Enum					mFormat;
	/// Flags specifying the special features of the buffer(typeless buffer,...) (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	BufferFeatureFlags					mFeatures;
	/// Debug name used in gpu profile
	wchar_t*							pDebugName;
} BufferDesc;

typedef struct Buffer {
	Renderer*							pRenderer;
	/// Buffer creation info
	BufferDesc							mDesc;
	/// Position of dynamic buffer memory in the mapped resource
	uint64_t							mPositionInHeap;
	/// CPU address of the mapped buffer (appliacable to buffers created in CPU accessible heaps (CPU, CPU_TO_GPU, GPU_TO_CPU)
	void*                               pCpuMappedAddress;
	/// Current state of the buffer
	ResourceState						mCurrentState;
	/// State of the buffer before mCurrentState (used for state tracking during a split barrier)
	ResourceState						mPreviousState;
	/// A unique id used for hashing buffers during resource binding
	uint64_t							mBufferId;
#if defined(DIRECT3D12)
	/// Native handle of the underlying resource
	ID3D12Resource*                     pDxResource;
	/// Contains resource allocation info such as parent heap, offset in heap
	struct ResourceAllocation*			pDxAllocation;
	/// Description for creating the CBV for ths buffer (applicable to BUFFER_USAGE_UNIFORM)
	D3D12_CONSTANT_BUFFER_VIEW_DESC     mDxCbvDesc;
	/// Description for creating the SRV for ths buffer (applicable to BUFFER_USAGE_STORAGE_SRV)
	D3D12_SHADER_RESOURCE_VIEW_DESC     mDxSrvDesc;
	/// Description for creating the UAV for ths buffer (applicable to BUFFER_USAGE_STORAGE_UAV)
	D3D12_UNORDERED_ACCESS_VIEW_DESC    mDxUavDesc;
	/// Description for binding this buffer as an index buffer (applicable to BUFFER_USAGE_INDEX)
	D3D12_INDEX_BUFFER_VIEW             mDxIndexBufferView;
	/// Description for binding this buffer as a vertex buffer (applicable to BUFFER_USAGE_VERTEX)
	D3D12_VERTEX_BUFFER_VIEW            mDxVertexBufferView;
	/// Descriptor handle of the CBV in a CPU visible descriptor heap (applicable to BUFFER_USAGE_UNIFORM)
	D3D12_CPU_DESCRIPTOR_HANDLE			mDxCbvHandle;
	/// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to BUFFER_USAGE_STORAGE_SRV)
	D3D12_CPU_DESCRIPTOR_HANDLE			mDxSrvHandle;
	/// Descriptor handle of the UAV in a CPU visible descriptor heap (applicable to BUFFER_USAGE_STORAGE_UAV)
	D3D12_CPU_DESCRIPTOR_HANDLE			mDxUavHandle;
#elif defined(VULKAN)
	/// Native handle of the underlying resource
	VkBuffer                            pVkBuffer;
	/// Contains resource allocation info such as parent heap, offset in heap
	struct VmaAllocation_T*             pVkMemory;
	/// Description for creating the descriptor for this buffer (applicable to BUFFER_USAGE_UNIFORM, BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
	VkDescriptorBufferInfo              mVkBufferInfo;
#elif defined(METAL)
    /// Contains resource allocation info such as parent heap, offset in heap
    struct ResourceAllocation*          pMtlAllocation;
	/// Native handle of the underlying resource
    id<MTLBuffer>                       mtlBuffer;
#endif
} Buffer;

typedef struct ClearValue {
	union {
		struct {
			float r;
			float g;
			float b;
			float a;
		};
		struct {
			float    depth;
			uint32 stencil;
		};
	};
} ClearValue;

typedef struct LoadActionsDesc {
	ClearValue				mClearColorValues[MAX_RENDER_TARGET_ATTACHMENTS];
	LoadActionType			mLoadActionsColor[MAX_RENDER_TARGET_ATTACHMENTS];
	ClearValue				mClearDepth;
	LoadActionType			mLoadActionDepth;
	LoadActionType			mLoadActionStencil;
} LoadActionsDesc;

/// Data structure holding necessary info to create a Texture
typedef struct TextureDesc
{
	/// Type of texture (1D, 2D, 3D, Cube)
	TextureType				mType;
	/// Texture creation flags (decides memory allocation strategy, sharing access,...)
	TextureCreationFlags	mFlags;
	/// Width
	uint32_t				mWidth;
	/// Height
	uint32_t				mHeight;

	/// Depth (Should be 1 if not a mType is not TEXTURE_TYPE_3D)
	uint32_t				mDepth;
	/// Start array layer (Should be between 0 and max array layers for this texture)
	uint32_t				mBaseArrayLayer;
	/// Texture array size (Should be 1 if texture is not a texture array or cubemap)
	uint32_t				mArraySize;
	/// Most detailed mip level (Should be between 0 and max mip levels for this texture)
	uint32_t				mBaseMipLevel;

	/// Number of mip levels
	uint32_t				mMipLevels;
	/// Number of multisamples per pixel (currently Textures created with mUsage TEXTURE_USAGE_SAMPLED_IMAGE only support SAMPLE_COUNT_1)
	SampleCount				mSampleCount;
	/// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value appropriate for mSampleCount
	uint32_t				mSampleQuality;
	/// Internal image format
	ImageFormat::Enum		mFormat;
	/// Optimized clear value (recommended to use this same value when clearing the rendertarget)
	ClearValue				mClearValue;
	/// Flags specifying suitable usage of the texture(UAV, SRV, RTV, DSV)
	TextureUsage			mUsage;
	/// What state will the texture get created in
	ResourceState			mStartState;
	/// Pointer to native texture handle if the texture does not own underlying resource
	const void*				pNativeHandle;
	/// Set whether texture is srgb
	bool				    mSrgb;
	/// Is the texture CPU accessible (applicable on hardware supporting CPU mapped textures (UMA))
	bool					mHostVisible;
	/// Debug name used in gpu profile
	wchar_t*				pDebugName;
} TextureDesc;

typedef struct Texture {
	Renderer*							pRenderer; //8
	/// Texture creation info
	TextureDesc							mDesc;   //88
	/// CPU mapped address (applicable to Textures created with mHostVisible = true)
	void*								pCpuMappedAddress;
	/// Size of the texture (in bytes)
	uint64_t							mTextureSize;
	/// This value will be false if the underlying resource is not owned by the texture (swapchain textures,...)
	bool								mOwnsImage;
	/// Current state of the texture
	ResourceState						mCurrentState;
	/// State of the texture before mCurrentState (used for state tracking during a split barrier)
	ResourceState						mPreviousState;
	uint32_t padding; //Padding to put the textureID adn the SRV/UAV handle on the same cache line
	/// A unique id used for hashing textures during resource binding
	uint64_t							mTextureId;
#if defined(DIRECT3D12)
	/// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to TEXTURE_USAGE_SAMPLED_IMAGE)
	D3D12_CPU_DESCRIPTOR_HANDLE			mDxSrvHandle;
	/// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to TEXTURE_USAGE_UNORDERED_ACCESS)
	D3D12_CPU_DESCRIPTOR_HANDLE			mDxUavHandle;
	/// Native handle of the underlying resource
	ID3D12Resource*                     pDxResource;
	/// Contains resource allocation info such as parent heap, offset in heap
	struct ResourceAllocation*			pDxAllocation;
	/// Description for creating the SRV for this texture (applicable to TEXTURE_USAGE_SAMPLED_IMAGE)
	D3D12_SHADER_RESOURCE_VIEW_DESC		mDxSrvDesc;
	/// Description for creating the UAV for this texture (applicable to TEXTURE_USAGE_UNORDERED_ACCESS)
	D3D12_UNORDERED_ACCESS_VIEW_DESC	mDxUavDesc;
#elif defined(VULKAN)
	/// Native handle of the underlying resource
	VkImage								pVkImage;
	/// Contains resource allocation info such as parent heap, offset in heap
	struct VmaAllocation_T*				pVkMemory;
	/// Opaque handle used by shaders for doing read/write operations on the texture
	VkImageView							pVkImageView;
	/// Flags specifying which aspects (COLOR,DEPTH,STENCIL) are included in the pVkImageView
	VkImageAspectFlags					mVkAspectMask;
	/// Description for creating the descriptor for this texture (applicable to TEXTURE_USAGE_SAMPLED_IMAGE, TEXTURE_USAGE_UNORDERED_ACCESS)
	VkDescriptorImageInfo				mVkTextureView;
#elif defined(METAL)
    /// Contains resource allocation info such as parent heap, offset in heap
    struct ResourceAllocation*			pMtlAllocation;
	/// Native handle of the underlying resource
	id<MTLTexture>						mtlTexture;
	MTLPixelFormat						mtlPixelFormat;
	bool								mIsCompressed;
#endif
} Texture;

typedef struct RenderTargetDesc
{
	/// Type of texture (1D, 2D, 3D, Cube)
	RenderTargetType		mType;
	/// Texture creation flags (decides memory allocation strategy, sharing access,...)
	TextureCreationFlags	mFlags;
	/// Width
	uint32_t				mWidth;
	/// Height
	uint32_t				mHeight;
	/// Depth (Should be 1 if not a mType is not TEXTURE_TYPE_3D)
	uint32_t				mDepth;
	/// Start array layer (Should be between 0 and max array layers for this texture)
	uint32_t				mBaseArrayLayer;
	/// Texture array size (Should be 1 if texture is not a texture array or cubemap)
	uint32_t				mArraySize;
	/// Mip level to render into
	uint32_t				mBaseMipLevel;
	/// Number of multisamples per pixel (currently Textures created with mUsage TEXTURE_USAGE_SAMPLED_IMAGE only support SAMPLE_COUNT_1)
	SampleCount				mSampleCount;
	/// Internal image format
	ImageFormat::Enum		mFormat;
	/// Optimized clear value (recommended to use this same value when clearing the rendertarget)
	ClearValue				mClearValue;
	/// Flags specifying suitable usage of the texture(UAV, SRV, RTV, DSV)
	RenderTargetUsage		mUsage;
	/// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value appropriate for mSampleCount
	uint32_t				mSampleQuality;
	/// Set whether rendertarget is srgb
    bool					mSrgb;
	/// Debug name used in gpu profile
	wchar_t*				pDebugName;
} RenderTargetDesc;

typedef struct RenderTarget
{
	RenderTargetDesc				mDesc;
	Texture*						pTexture;

#if defined(DIRECT3D12)
	/// Description for creating the RTV for this texture (applicable to RENDER_TARGET_USAGE_COLOR)
	D3D12_RENDER_TARGET_VIEW_DESC	mDxRtvDesc;
	/// Description for creating the RTV for this texture (applicable to RENDER_TARGET_USAGE_DEPTH_STENCIL)
	D3D12_DEPTH_STENCIL_VIEW_DESC	mDxDsvDesc;
	/// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to RENDER_TARGET_USAGE_COLOR)
	D3D12_CPU_DESCRIPTOR_HANDLE		mDxRtvHandle;
	/// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to RENDER_TARGET_USAGE_DEPTH_STENCIL)
	D3D12_CPU_DESCRIPTOR_HANDLE		mDxDsvHandle;
#elif defined(TARGET_IOS)
    // A separate texture is needed for stencil rendering on iOS.
    Texture*						pStencil;
#endif
} RenderTarget;

typedef struct Sampler {
	Renderer*						pRenderer;
	/// A unique id used for hashing samplers during resource binding
	uint64_t						mSamplerId;
#if defined(DIRECT3D12)
	/// Description for creating the Sampler descriptor for ths sampler
	D3D12_SAMPLER_DESC				mDxSamplerDesc;
	/// Descriptor handle of the Sampler in a CPU visible descriptor heap
	D3D12_CPU_DESCRIPTOR_HANDLE		mDxSamplerHandle;
#elif defined(VULKAN)
	/// Native handle of the underlying resource
	VkSampler						pVkSampler;
	/// Description for creating the descriptor for this sampler
	VkDescriptorImageInfo			mVkSamplerView;
#elif defined(METAL)
	/// Native handle of the underlying resource
    id<MTLSamplerState>				mtlSamplerState;
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
typedef struct DescriptorInfo {

	/// Binding information generated from the shader reflection
	ShaderResource				mDesc;
	/// Index in the descriptor set
	uint32_t					mIndexInParent;
	/// Update frequency of this descriptor
	DescriptorUpdateFrequency	mUpdateFrquency;

#if defined(DIRECT3D12)
	D3D12_ROOT_PARAMETER_TYPE	mDxType;
	uint32_t					mHandleIndex;
#elif defined(VULKAN)
	VkDescriptorType			mVkType;
	VkShaderStageFlags			mVkStages;
	uint32_t					mDynamicUniformIndex;
	uint32_t					mHandleIndex;
#elif defined(METAL)
#endif
} DescriptorInfo;

/// Array of root descriptors  belonging to this update frequency layout (D3D12_ROOT_PARAMETER_CBV/SRV/UAV or VK_DESCRIPTOR_TYPE_(UNIFORM/STORAGE)_BUFFER_DYNAMIC)
typedef struct RootDescriptorLayout
{
	/// Number of root descriptors
	/// In DirectX12 this is the number of root cbvs / srvs/ uavs
	/// In Vulkan this is the number of VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC / VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
	uint32_t		mRootDescriptorCount;
	/// Index of descriptor in pDescriptors array in the root signature
	uint32_t*		pDescriptorIndices;
#if defined(DIRECT3D12)
	/// Root parameter index used in SetGraphicsRootDescriptor / SetComputeRootDescriptor
	uint32_t*		pRootIndices;
#endif

} RootDescriptorLayout;

typedef struct RootConstantLayout
{
	uint32_t				mDescriptorIndex;
#if defined(DIRECT3D12)
	uint32_t				mRootIndex;
#elif defined(VULKAN)
	/// Array of push constant layouts
	VkPushConstantRange		mVkPushConstantRange;
#endif
} RootConstantLayout;

/// Array of descriptor ranges belonging to this update frequency layout
typedef struct DescriptorSetLayout
{
	/// Number of descriptors excluding descriptors in arrays
	uint32_t				mDescriptorCount;
	/// Total number of descriptors including descriptors in arrays
	uint32_t				mCumulativeDescriptorCount;
	/// Indices of the descriptors belonging to this set
	uint32_t*				pDescriptorIndices;
#if defined(DIRECT3D12)
	/// Root parameter index used in SetGraphicsRootDescriptorTable / SetComputeRootDescriptorTable
	uint32_t				mRootIndex;
#elif defined(VULKAN)
	VkDescriptorSetLayout	pVkSetLayout;
	/// Total number of descriptors including descriptors in arrays
	uint32_t				mCumulativeBufferDescriptorCount;
	uint32_t				mCumulativeImageDescriptorCount;
#elif defined(METAL)
#endif
} DescriptorSetLayout;

typedef struct RootSignatureDesc
{
    //TODO: Remove constructor to keep C-Style interface
	RootSignatureDesc()
	{
		mMaxBindlessDescriptors[DESCRIPTOR_TYPE_TEXTURE] = 256U;
		mMaxBindlessDescriptors[DESCRIPTOR_TYPE_RW_TEXTURE] = 32U;
		mMaxBindlessDescriptors[DESCRIPTOR_TYPE_BUFFER] = 32U;
		mMaxBindlessDescriptors[DESCRIPTOR_TYPE_RW_BUFFER] = 32U;
		mMaxBindlessDescriptors[DESCRIPTOR_TYPE_UNIFORM_BUFFER] = 32U;
		mMaxBindlessDescriptors[DESCRIPTOR_TYPE_SAMPLER] = 32U;
	}
	uint32_t											mMaxBindlessDescriptors[DESCRIPTOR_TYPE_COUNT];
	tinystl::unordered_map<tinystl::string, Sampler*>	mStaticSamplers;
#if defined(VULKAN)
	tinystl::vector<tinystl::string>					mDynamicUniformBuffers;
#endif
} RootSignatureDesc;

typedef struct RootSignature
{
	/// Number of descriptors declared in the root signature layout
	uint32_t									mDescriptorCount;
	/// Array of all descriptors declared in the root signature layout
	DescriptorInfo*								pDescriptors;
	/// Translates hash of descriptor name to descriptor index
	tinystl::unordered_map<uint32_t, uint32_t>	pDescriptorNameToIndexMap;

	/// Number of root constants in the root signature
	uint32_t									mRootConstantCount;
	/// Array of root constant layouts
	RootConstantLayout*							pRootConstantLayouts;
	PipelineType								mPipelineType;
#if defined(DIRECT3D12)
	/// DX12 separates samplers and views in two heaps so we need two table layouts
	/// Array of view descriptor table layouts of size DESCRIPTOR_UPDATE_FREQ_NUM_TYPES
	DescriptorSetLayout*						pViewTableLayouts;
	/// Array of sampler descriptor table layouts of size DESCRIPTOR_UPDATE_FREQ_NUM_TYPES
	DescriptorSetLayout*						pSamplerTableLayouts;
	/// Array of root descriptor layouts of size DESCRIPTOR_UPDATE_FREQ_NUM_TYPES
	RootDescriptorLayout*						pRootDescriptorLayouts;

	ID3D12RootSignature*						pDxRootSignature;
	ID3DBlob*									pDxSerializedRootSignatureString;
#elif defined(VULKAN)
	DescriptorSetLayout*						pDescriptorSetLayouts;
	/// Array of root descriptor / root constant of size DESCRIPTOR_UPDATE_FREQ_NUM_TYPES
	RootDescriptorLayout*						pRootDescriptorLayouts;

	VkPipelineLayout							pPipelineLayout;
#elif defined(METAL)
#endif

	using ThreadLocalDescriptorManager = tinystl::unordered_map<ThreadID, struct DescriptorManager*>;

	/// Api specific binding manager
	ThreadLocalDescriptorManager				pDescriptorManagerMap;
} RootSignature;

typedef struct DescriptorData
{
    //TODO: Remove constructor to keep C-Style interface
    DescriptorData() :
			pName(NULL),
			mIndex((uint32_t)-1),
			mCount(1),
            mOffset(0),
            ppTextures(NULL) {}
    
	/// User can either set name of descriptor or index (index in pRootSignature->pDescriptors array)
    /// Name of descriptor
    const char*     pName;
	/// Index of descriptor
	uint32_t		mIndex;
    /// Number of resources in the descriptor(applies to array of textures, buffers,...)
    uint32_t        mCount;
    /// Offset to bind the descriptor at
    uint64_t        mOffset;
    
    /// Array of resources containing descriptor handles or constant to be used in ring buffer memory - DescriptorRange can hold only one resource type array
    union
    {
		/// Array of texture descriptors (srv and uav textures)
        Texture**   ppTextures;
		/// Array of sampler descriptors
        Sampler**   ppSamplers;
		/// Array of buffer descriptors (srv, uav and cbv buffers)
        Buffer**    ppBuffers;
		/// Constant data in system memory to be bound as root / push constant
		void*		pRootConstant;
    };
} DescriptorData;

typedef struct CmdPoolDesc
{
	CmdPoolType mCmdPoolType;
}CmdPoolDesc;

typedef struct CmdPool {
	Renderer*					    pRenderer;
	CmdPoolDesc						mCmdPoolDesc;
    Queue*							pQueue;
#if defined(VULKAN)
	VkCommandPool					pVkCmdPool;
#elif defined(DIRECT3D12)
	// Temporarily move to Cmd struct until we get the command allocator pool logic working
	//ID3D12CommandAllocator*          pDxCmdAlloc;
#endif
} CmdPool;

typedef struct Cmd {
	CmdPool*								pCmdPool;

	const RootSignature*					pBoundRootSignature;
#if defined(DIRECT3D12)
	// For now each command list will have its own allocator until we get the command allocator pool logic working
	ID3D12CommandAllocator*					pDxCmdAlloc;
	ID3D12GraphicsCommandList*				pDxCmdList;

	D3D12_RESOURCE_BARRIER					pBatchBarriers[MAX_BATCH_BARRIERS];
	uint32_t								mBatchBarrierCount;

	// Small ring buffer to be used for copying root constant data in case the root constant was converted to root cbv
	struct UniformRingBuffer*				pRootConstantRingBuffer;
	D3D12_CPU_DESCRIPTOR_HANDLE				mViewCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE				mViewGpuHandle;
	uint64_t								mViewPosition;
	D3D12_CPU_DESCRIPTOR_HANDLE				mSamplerCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE				mSamplerGpuHandle;
	uint64_t								mSamplerPosition;
#elif defined(VULKAN)
	VkCommandBuffer							pVkCmdBuf;

	VkImageMemoryBarrier					pBatchImageMemoryBarriers[MAX_BATCH_BARRIERS];
	uint32_t								mBatchImageMemoryBarrierCount;
	VkBufferMemoryBarrier					pBatchBufferMemoryBarriers[MAX_BATCH_BARRIERS];
	uint32_t								mBatchBufferMemoryBarrierCount;
	struct DescriptorStoreHeap*				pDescriptorPool;
#elif defined(METAL)
	id<MTLCommandBuffer>					mtlCommandBuffer;
    id<MTLFence>                            mtlEncoderFence; // Used to sync different types of encoders recording in the same Cmd.
	id<MTLRenderCommandEncoder>				mtlRenderEncoder;
	id<MTLComputeCommandEncoder>			mtlComputeEncoder;
	id<MTLBlitCommandEncoder>				mtlBlitEncoder;
	MTLRenderPassDescriptor*				pRenderPassDesc;
	MTLPrimitiveType						selectedPrimitiveType;
	Buffer*									selectedIndexBuffer;
    Shader*                                 pShader;
    RenderTarget*                           pRenderTarget;
#endif
} Cmd;

typedef struct QueueDesc
{
	QueueFlag mFlag;
	QueuePriority mPriority;
	CmdPoolType mType; //same types for cmd pool and queue
}QueueDesc;

typedef enum FenceStatus
{
	FENCE_STATUS_COMPLETE = 0,
	FENCE_STATUS_INCOMPLETE,
} FenceStatus;

typedef struct Fence {
	Renderer* pRenderer;
#if defined(VULKAN)
	VkFence                             pVkFence;
	bool								mSubmitted;
#elif defined(DIRECT3D12)
	ID3D12Fence*                        pDxFence;
	HANDLE                              pDxWaitIdleFenceEvent;
	uint64								mFenceValue;
#elif defined(METAL)
    dispatch_semaphore_t                pMtlSemaphore;
    bool                                mSubmitted;
#endif
} Fence;

typedef struct Semaphore {
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
	Fence*								pFence;
#elif defined(VULKAN)
	VkSemaphore                         pVkSemaphore;
	bool								mSignaled;
#elif defined(METAL)
	dispatch_semaphore_t                pMtlSemaphore;
#endif
} Semaphore;

typedef struct Queue {
	Renderer*			    pRenderer;
#if defined(VULKAN)
	VkQueue					pVkQueue;
	uint32_t				mVkQueueFamilyIndex;
	uint32_t				mVkQueueIndex;
#elif defined(DIRECT3D12)
	ID3D12CommandQueue*		pDxQueue;
	Fence*					pQueueFence;
#elif defined(METAL)
    id<MTLCommandQueue>		mtlCommandQueue;
    dispatch_semaphore_t	pMtlSemaphore;
#endif
	QueueDesc				mQueueDesc;
} Queue;

typedef struct ShaderMacro
{
	tinystl::string definition;
	tinystl::string value;
} ShaderMacro;

#if defined(METAL)
typedef struct ShaderStageDesc
{
	tinystl::string					mName;
	tinystl::string					mCode;
	tinystl::string					mEntryPoint;
	tinystl::vector<ShaderMacro>	mMacros;
} ShaderStageDesc;

typedef struct ShaderDesc
{
	ShaderStage		mStages;
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
	char*					pByteCode;
	uint32_t				mByteCodeSize;
#if defined(METAL)
    // Shader source is needed for reflection
    tinystl::string         mSource;
	/// Entry point is needed for Metal
	tinystl::string			mEntryPoint;
#endif
} BinaryShaderStageDesc;

typedef struct BinaryShaderDesc
{
	ShaderStage				mStages;
	BinaryShaderStageDesc	mVert;
	BinaryShaderStageDesc	mFrag;
	BinaryShaderStageDesc	mGeom;
	BinaryShaderStageDesc	mHull;
	BinaryShaderStageDesc	mDomain;
	BinaryShaderStageDesc	mComp;
} BinaryShaderDesc;

typedef struct Shader {
	Renderer*               pRenderer;
	ShaderStage				mStages;
	PipelineReflection		mReflection;

	uint32_t				mNumThreadsPerGroup[3];
	uint32_t				mNumControlPoint;

#if defined(VULKAN)
	VkShaderModule			pVkVert;
	VkShaderModule			pVkTesc;
	VkShaderModule			pVkTese;
	VkShaderModule			pVkGeom;
	VkShaderModule			pVkFrag;
	VkShaderModule			pVkComp;
#elif defined(DIRECT3D12)
	ID3DBlob*				pDxVert;
	ID3DBlob*				pDxHull;
	ID3DBlob*				pDxDomn;
	ID3DBlob*				pDxGeom;
	ID3DBlob*				pDxFrag;
	ID3DBlob*				pDxComp;
#elif defined(METAL)
	id<MTLFunction>			mtlVertexShader;
	id<MTLFunction>			mtlFragmentShader;
	id<MTLFunction>			mtlComputeShader;
#endif
} Shader;

typedef struct BlendState
{
#if defined(DIRECT3D12)
	D3D12_BLEND_DESC					mDxBlendDesc;
#elif defined(VULKAN)
	VkPipelineColorBlendAttachmentState	RTBlendStates[MAX_RENDER_TARGET_ATTACHMENTS];
	VkBool32							LogicOpEnable;
	VkLogicOp							LogicOp;
#elif defined(METAL)
    struct BlendStateData
    {
        MTLBlendFactor					srcFactor;
        MTLBlendFactor					destFactor;
        MTLBlendFactor					srcAlphaFactor;
        MTLBlendFactor					destAlphaFactor;
        MTLBlendOperation				blendMode;
        MTLBlendOperation				blendAlphaMode;
    };
    BlendStateData						blendStatePerRenderTarget[MAX_RENDER_TARGET_ATTACHMENTS];
    bool								alphaToCoverage;
#endif
} BlendState;

typedef struct DepthState
{
#if defined(DIRECT3D12)
	D3D12_DEPTH_STENCIL_DESC	mDxDepthStencilDesc;
#elif defined(VULKAN)
	VkBool32					DepthTestEnable;
	VkBool32					DepthWriteEnable;
	VkCompareOp					DepthCompareOp;
	VkBool32					DepthBoundsTestEnable;
	VkBool32					StencilTestEnable;
	VkStencilOpState			Front;
	VkStencilOpState			Back;
	float						MinDepthBounds;
	float						MaxDepthBounds;
#elif defined(METAL)
    id<MTLDepthStencilState>	mtlDepthState;
#endif
} DepthState;

typedef struct RasterizerState
{
#if defined(DIRECT3D12)
	D3D12_RASTERIZER_DESC	mDxRasterizerDesc;
#elif defined(VULKAN)
	VkBool32				DepthClampEnable;
	VkPolygonMode			PolygonMode;
	VkCullModeFlags			CullMode;
	VkFrontFace				FrontFace;
	VkBool32				DepthBiasEnable;
	float					DepthBiasConstantFactor;
	float					DepthBiasClamp;
	float					DepthBiasSlopeFactor;
	float					LineWidth;
#elif defined(METAL)
    MTLCullMode				cullMode;
    MTLTriangleFillMode		fillMode;
    float					depthBiasSlopeFactor;
    float					depthBias;
    bool					scissorEnable;
    bool					multisampleEnable;
#endif
} RasterizerState;

typedef struct VertexAttrib {
	ShaderSemantic		mSemantic;
	uint32_t			mSemanticNameLength;
	char				mSemanticName[MAX_SEMANTIC_NAME_LENGTH];
	ImageFormat::Enum	mFormat;
	uint32_t			mBinding;
	uint32_t			mLocation;
	uint32_t			mOffset;
} VertexAttrib;

typedef struct VertexLayout {
	uint32_t                        mAttribCount;
	VertexAttrib                    mAttribs[MAX_VERTEX_ATTRIBS];
} VertexLayout;

typedef struct GraphicsPipelineDesc
{
	Shader*				pShaderProgram;
	RootSignature*		pRootSignature;
	VertexLayout*		pVertexLayout;
	BlendState*			pBlendState;
	DepthState*			pDepthState;
	RasterizerState*	pRasterizerState;
	ImageFormat::Enum*	pColorFormats;
	bool*				pSrgbValues;
	uint32_t			mRenderTargetCount;
	SampleCount			mSampleCount;
	uint32_t			mSampleQuality;
	ImageFormat::Enum	mDepthStencilFormat;
	PrimitiveTopology	mPrimitiveTopo;
} GraphicsPipelineDesc;

typedef struct ComputePipelineDesc {

	Shader*				pShaderProgram;
	RootSignature*		pRootSignature;
} ComputePipelineDesc;

typedef struct Pipeline {
	Renderer*					    pRenderer;
	union
	{
		GraphicsPipelineDesc		mGraphics;
		ComputePipelineDesc			mCompute;
	};
	PipelineType					mType;
#if defined(VULKAN)
	VkPipeline						pVkPipeline;
#elif defined(DIRECT3D12)
	ID3D12PipelineState*			pDxPipelineState;
	D3D_PRIMITIVE_TOPOLOGY			mDxPrimitiveTopology;
#elif defined(METAL)
	RenderTarget*					pRenderPasspRenderTarget;
	Shader*							pShader;
	id<MTLRenderPipelineState>		mtlRenderPipelineState;
	id<MTLComputePipelineState>		mtlComputePipelineState;
	MTLCullMode						mCullMode;
#endif
} Pipeline;

typedef struct SubresourceDataDesc
{
#if defined(DIRECT3D12) || defined(METAL)
	uint32_t mRowPitch;
	uint32_t mSlicePitch;
	void* pData;
#else
	uint32_t mMipLevel;
	uint32_t mArrayLayer;
	uint32_t mWidth;
	uint32_t mHeight;
	uint32_t mDepth;
	uint32_t mArraySize;
	uint64_t mBufferOffset;
#endif
} SubresourceDataDesc;

typedef struct SwapChainDesc {
	/// Window handle
	WindowsDesc*		pWindow;
	/// Queue which will be flushed on swapchain present
	Queue*				pQueue;
	/// Number of backbuffers in this swapchain
	uint32_t			mImageCount;
	/// Width of the swapchain
	uint32_t			mWidth;
	/// Height of the swapchain
	uint32_t			mHeight;
	/// Sample count
	SampleCount			mSampleCount;
	/// Sample quality (DirectX12 only)
	uint32_t			mSampleQuality;
	/// Color format of the swapchain
	ImageFormat::Enum	mColorFormat;
	/// Clear value
	ClearValue			mColorClearValue;
	/// Set whether this swapchain using srgb color space
	bool				mSrgb;
	bool				mEnableVsync;
} SwapChainDesc;

typedef struct SwapChain
{
	SwapChainDesc			mDesc;
	/// Render targets created from the swapchain back buffers
	RenderTarget**			ppSwapchainRenderTargets;
#if defined(_DURANGO)
	IDXGISwapChain1*		pSwapChain;
	UINT					mDxSyncInterval;
	ID3D12Resource**		ppDxSwapChainResources;
	uint32_t				mImageIndex;
	uint32_t				mFlags;
#elif defined(DIRECT3D12)
	/// Use IDXGISwapChain3 for now since IDXGISwapChain4
	/// isn't supported by older devices.
	IDXGISwapChain3*		pSwapChain;
	/// Sync interval to specify how interval for vsync
	UINT					mDxSyncInterval;
	ID3D12Resource**		ppDxSwapChainResources;
	uint32_t				mImageIndex;
	uint32_t				mFlags;
#elif defined(VULKAN)
	/// Present queue if one exists (queuePresent will use this queue if the hardware has a dedicated present queue)
	VkQueue					pPresentQueue;
	VkSwapchainKHR			pSwapChain;
	VkSurfaceKHR			pVkSurface;
	VkImage*				ppVkSwapChainImages;
#elif defined(METAL)
    MTKView*                pMTKView;
    id<MTLCommandBuffer>    presentCommandBuffer;
#endif
} SwapChain;

typedef enum ShaderTarget {
	// 5.1 is supported on all DX12 hardware
	shader_target_5_1,
	shader_target_6_0,
} DXShaderTarget;

typedef struct RendererDesc {
	LogFn							pLogFn;
	ShaderTarget					mShaderTarget;
#if defined(VULKAN)
	tinystl::vector<String>			mInstanceLayers;
	tinystl::vector<String>			mInstanceExtensions;
	tinystl::vector<String>			mDeviceLayers;
	tinystl::vector<String>			mDeviceExtensions;
	PFN_vkDebugReportCallbackEXT	pVkDebugFn;
#elif defined(DIRECT3D12)
	D3D_FEATURE_LEVEL				mDxFeatureLevel;
#elif defined(METAL)
#endif
} RendererDesc;

typedef struct GPUSettings
{
	uint64_t	mUniformBufferAlignment;
	uint32_t	mMaxVertexInputBindings;
	bool		mMultiDrawIndirect;
	uint32_t	mMaxRootSignatureDWORDS;

} GPUSettings;

typedef struct Renderer {
	char*								pName;
	RendererDesc						mSettings;
	uint32_t							mNumOfGPUs;
	GPUSettings*						pActiveGpuSettings;
	GPUSettings							mGpuSettings[MAX_GPUS];
#if defined(_DURANGO)
	IDXGIFactory2*						pDXGIFactory;
	IDXGIAdapter*						pGPUs[MAX_GPUS];
	IDXGIAdapter*						pActiveGPU;
	ID3D12Device*						pDevice;
	EsramManager*						pESRAMManager;
#if defined(_DEBUG) || defined ( PROFILE )
	ID3D12Debug*						pDXDebug;
#endif
	// Default NULL Descriptors for binding at empty descriptor slots to make sure all descriptors are bound at submit
	D3D12_CPU_DESCRIPTOR_HANDLE			mSrvNullDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE			mUavNullDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE			mCbvNullDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE			mSamplerNullDescriptor;

	// API specific descriptor heap and memory allocator
	struct DescriptorStoreHeap*			pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	struct DescriptorStoreHeap*			pCbvSrvUavHeap;
	struct DescriptorStoreHeap*			pSamplerHeap;
	struct ResourceAllocator*			pResourceAllocator;
#elif (DIRECT3D12)
	IDXGIFactory5*						pDXGIFactory;
	IDXGIAdapter3*						pGPUs[MAX_GPUS];
	IDXGIAdapter3*						pActiveGPU;
	ID3D12Device*						pDevice;
#if defined(_DEBUG)
	ID3D12Debug*						pDXDebug;
#endif
	// Default NULL Descriptors for binding at empty descriptor slots to make sure all descriptors are bound at submit
	D3D12_CPU_DESCRIPTOR_HANDLE			mSrvNullDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE			mUavNullDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE			mCbvNullDescriptor;
	D3D12_CPU_DESCRIPTOR_HANDLE			mSamplerNullDescriptor;

	// API specific descriptor heap and memory allocator
	struct DescriptorStoreHeap*			pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	struct DescriptorStoreHeap*			pCbvSrvUavHeap;
	struct DescriptorStoreHeap*			pSamplerHeap;
	struct ResourceAllocator*			pResourceAllocator;
#elif defined (VULKAN)
	VkInstance							pVKInstance;
	VkPhysicalDevice					pGPUs[MAX_GPUS];
	VkPhysicalDevice					pActiveGPU;
	VkPhysicalDeviceProperties			mVkGpuProperties[MAX_GPUS];
	VkPhysicalDeviceMemoryProperties	mVkGpuMemoryProperties[MAX_GPUS];
	uint32_t							mVkQueueFamilyPropertyCount[MAX_GPUS];
	VkQueueFamilyProperties*			mVkQueueFamilyProperties[MAX_GPUS];
	uint32_t							mActiveGPUIndex;
	VkPhysicalDeviceMemoryProperties*	pVkActiveGpuMemoryProperties;
	VkPhysicalDeviceProperties*			pVkActiveGPUProperties;
	VkQueueFamilyProperties*			pVkActiveQueueFamilyProperties;
	uint32_t							mVkActiveQueueFamilyPropertyCount;
	VkDevice							pDevice;
	VkDebugReportCallbackEXT			pDebugReport;
	tinystl::vector<const char*>		mInstanceLayers;
	uint32_t							mUsedQueueCount[MAX_GPUS][16];

	Texture*							pDefaultTexture;
	Buffer*								pDefaultBuffer;
	Sampler*							pDefaultSampler;

	struct VmaAllocator_T*				pVmaAllocator;
	struct DescriptorStoreHeap*			pDescriptorPool;

	// These are the extensions that we have loaded
	const char* gVkInstanceExtensions[MAX_INSTANCE_EXTENSIONS];
	// These are the extensions that we have loaded
	const char* gVkDeviceExtensions[MAX_DEVICE_EXTENSIONS];
#elif defined(METAL)
    id<MTLDevice>						pDevice;
    struct ResourceAllocator*           pResourceAllocator;
#endif

	// Default states used if user does not specify them in pipeline creation
	BlendState*							pDefaultBlendState;
	DepthState*							pDefaultDepthState;
	RasterizerState*					pDefaultRasterizerState;
} Renderer;

// Indirect command sturcture define
typedef struct IndirectArgumentDescriptor
{
	IndirectArgumentType mType;
	uint32_t mRootParameterIndex;
	uint32_t mCount;
	uint32_t mDivisor;

}IndirectArgumentDescriptor;

typedef struct CommandSignatureDesc
{
	CmdPool*					pCmdPool;
	RootSignature*				pRootSignature;
	uint32_t					mIndirectArgCount;
	IndirectArgumentDescriptor* pArgDescs;
} CommandSignatureDesc;

typedef struct CommandSignature
{
	CommandSignatureDesc				mDesc;
	uint32_t							mIndirectArgDescCounts;
	uint32_t							mDrawCommandStride;
#if defined(DIRECT3D12)
	ID3D12CommandSignature*				pDxCommandSignautre;
#elif defined(VULKAN)
	IndirectArgumentType				mDrawType;
#elif defined(METAL)
	IndirectArgumentType				mDrawType;
#endif
}CommandSignature;

#if defined(VULKAN)
#define ApiExport //extern "C"
#else
#define ApiExport 
#endif


//Returns the recommended format for the swapchain.
//If true is passed for the hintHDR parameter, it will return an HDR format IF the platform supports it
//If false is passed or the platform does not support HDR a non HDR format is returned.
ApiExport ImageFormat::Enum getRecommendedSwapchainFormat(bool hintHDR);

// API functions
// allocates memory and initializes the renderer -> returns pRenderer
//
// maybe a better word then ApiExport
ApiExport void initRenderer(const char* app_name, const RendererDesc* p_settings, Renderer** ppRenderer);
ApiExport void removeRenderer(Renderer* pRenderer);

ApiExport void addFence(Renderer* pRenderer, Fence** pp_fence, uint64 mFenceValue = 1);
ApiExport void removeFence(Renderer* pRenderer, Fence* p_fence);

ApiExport void addSemaphore(Renderer* pRenderer, Semaphore** pp_semaphore);
ApiExport void removeSemaphore(Renderer* pRenderer, Semaphore* p_semaphore);

ApiExport void addQueue(Renderer* pRenderer, QueueDesc* pQDesc, Queue** ppQueue);
ApiExport void removeQueue(Queue* pQueue);

ApiExport void addSwapChain(Renderer* pRenderer, const SwapChainDesc* p_desc, SwapChain** pp_swap_chain);
ApiExport void removeSwapChain(Renderer* pRenderer, SwapChain* p_swap_chain);

// command pool functions
ApiExport void addCmdPool(Renderer* pRenderer, Queue* p_queue, bool transient, CmdPool** pp_CmdPool, CmdPoolDesc * cmdPoolDesc = NULL);
ApiExport void removeCmdPool(Renderer* pRenderer, CmdPool* p_CmdPool);
ApiExport void addCmd(CmdPool* p_CmdPool, bool secondary, Cmd** pp_cmd);
ApiExport void removeCmd(CmdPool* p_CmdPool, Cmd* p_cmd);
ApiExport void addCmd_n(CmdPool* p_CmdPool, bool secondary, uint32_t cmd_count, Cmd*** ppp_cmd);
ApiExport void removeCmd_n(CmdPool* p_CmdPool, uint32_t cmd_count, Cmd** pp_cmd);

//
// All buffer, texture loading handled by resource system -> IResourceLoader.*
//

ApiExport uint32_t calculateVertexLayoutStride(const VertexLayout* p_vertex_layout);

ApiExport void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* p_desc, RenderTarget** pp_render_target, void* pNativeHandle = NULL);
ApiExport void removeRenderTarget(Renderer* pRenderer, RenderTarget* p_render_target);
ApiExport void addSampler(Renderer* pRenderer, Sampler** pp_sampler, FilterType minFilter = FILTER_LINEAR, FilterType magFilter = FILTER_LINEAR, MipMapMode mipMapMode = MIPMAP_MODE_LINEAR, AddressMode addressU = ADDRESS_MODE_CLAMP_TO_BORDER, AddressMode addressV = ADDRESS_MODE_CLAMP_TO_BORDER, AddressMode addressW = ADDRESS_MODE_CLAMP_TO_BORDER, float mipLosBias = 0.0f, float maxAnisotropy = 0.0f);
ApiExport void removeSampler(Renderer* pRenderer, Sampler* p_sampler);

// shader functions
#if defined(METAL)
ApiExport void addShader(Renderer* pRenderer, const ShaderDesc* p_desc, Shader** p_shader_program);
#endif
ApiExport void addShader(Renderer* pRenderer, const BinaryShaderDesc* p_desc, Shader** p_shader_program);
ApiExport void removeShader(Renderer* pRenderer, Shader* p_shader_program);

// pipeline functions
ApiExport void addRootSignature(Renderer* pRenderer, uint32_t num_shaders, Shader* const* pp_shaders, RootSignature** pp_root_signature, const RootSignatureDesc* pRootDesc = NULL);
ApiExport void removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature);
ApiExport void addPipeline(Renderer* pRenderer, const GraphicsPipelineDesc* p_pipeline_settings, Pipeline** pp_pipeline);
ApiExport void addComputePipeline(Renderer* pRenderer, const ComputePipelineDesc* p_pipeline_settings, Pipeline** p_pipeline);
ApiExport void removePipeline(Renderer* pRenderer, Pipeline* p_pipeline);

/// Pipeline State Functions
ApiExport void addBlendState(BlendState** ppBlendState, BlendConstant srcFactor, BlendConstant destFactor, BlendConstant srcAlphaFactor,
	BlendConstant destAlphaFactor, BlendMode blendMode = BlendMode::BM_ADD, BlendMode blendAlphaMode = BlendMode::BM_ADD,
	const int mask = ALL, const int MRTRenderTargetNumber = eBlendStateMRTRenderTarget0, const bool alphaToCoverage = false);

ApiExport void removeBlendState(BlendState* pBlendState);

ApiExport void addDepthState(Renderer* pRenderer, DepthState** ppDepthState, const bool depthTest, const bool depthWrite,
    const CompareMode depthFunc = CompareMode::CMP_LEQUAL,
	const bool stencilTest = false,
	const uint8 stencilReadMask = 0xFF,
	const uint8 stencilWriteMask = 0xFF,
	const CompareMode stencilFrontFunc = CompareMode::CMP_ALWAYS,
	const StencilOp stencilFrontFail = StencilOp::STENCIL_OP_KEEP,
	const StencilOp depthFrontFail = StencilOp::STENCIL_OP_KEEP,
	const StencilOp stencilFrontPass = StencilOp::STENCIL_OP_KEEP,
	const CompareMode stencilBackFunc = CompareMode::CMP_ALWAYS,
	const StencilOp stencilBackFail = StencilOp::STENCIL_OP_KEEP,
	const StencilOp depthBackFail = StencilOp::STENCIL_OP_KEEP,
	const StencilOp stencilBackPass = StencilOp::STENCIL_OP_KEEP);

ApiExport void removeDepthState(DepthState* pDepthState);

ApiExport void addRasterizerState(RasterizerState** ppRasterizerState, const CullMode cullMode,
	const int depthBias = 0,
	const float slopeScaledDepthBias = 0,
	const FillMode fillMode = FillMode::FILL_MODE_SOLID,
	const bool multiSample = false,
	const bool scissor = false);

ApiExport void removeRasterizerState(RasterizerState* pRasterizerState);

// command buffer functions
ApiExport void beginCmd(Cmd* p_cmd);
ApiExport void endCmd(Cmd* p_cmd);
ApiExport void cmdBeginRender(Cmd* p_cmd, uint32_t render_target_count, RenderTarget** pp_render_targets, RenderTarget* p_depth_stencil, const LoadActionsDesc* loadActions = NULL);
ApiExport void cmdEndRender(Cmd* p_cmd, uint32_t render_target_count, RenderTarget** pp_render_targets, RenderTarget* p_depth_stencil);
ApiExport void cmdSetViewport(Cmd* p_cmd, float x, float y, float width, float height, float min_depth, float max_depth);
ApiExport void cmdSetScissor(Cmd* p_cmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
ApiExport void cmdBindPipeline(Cmd* p_cmd, Pipeline* p_pipeline);
ApiExport void cmdBindDescriptors(Cmd* pCmd, RootSignature* pRootSignature, uint32_t numDescriptors, DescriptorData* pDescParams);
ApiExport void cmdBindIndexBuffer(Cmd* p_cmd, Buffer* p_buffer);
ApiExport void cmdBindVertexBuffer(Cmd* p_cmd, uint32_t buffer_count, Buffer** pp_buffers);
ApiExport void cmdDraw(Cmd* p_cmd, uint32_t vertex_count, uint32_t first_vertex);
ApiExport void cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount);
ApiExport void cmdDrawIndexed(Cmd* p_cmd, uint32_t index_count, uint32_t first_index);
ApiExport void cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount);
ApiExport void cmdDispatch(Cmd* p_cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);

// Transition Commands
ApiExport void cmdResourceBarrier(Cmd* p_cmd, uint32_t buffer_barrier_count, BufferBarrier* p_buffer_barriers, uint32_t texture_barrier_count, TextureBarrier* p_texture_barriers, bool batch);
ApiExport void cmdSynchronizeResources(Cmd* p_cmd, uint32_t buffer_count, Buffer** p_buffers, uint32_t texture_count, Texture** p_textures, bool batch);
/// Flushes all the batched transitions requested in cmdResourceBarrier
ApiExport void cmdFlushBarriers(Cmd* p_cmd);


//
// All buffer, texture update handled by resource system -> IResourceLoader.*
//

// queue/fence/swapchain functions
ApiExport void acquireNextImage(Renderer* pRenderer, SwapChain* p_swap_chain, Semaphore* p_signal_semaphore, Fence* p_fence, uint32_t* p_image_index);
ApiExport void queueSubmit(Queue* p_queue, uint32_t cmd_count, Cmd** pp_cmds, Fence* pFence, uint32_t wait_semaphore_count, Semaphore** pp_wait_semaphores, uint32_t signal_semaphore_count, Semaphore** pp_signal_semaphores);
ApiExport void queuePresent(Queue* p_queue, SwapChain* p_swap_chain, uint32_t swap_chain_image_index, uint32_t wait_semaphore_count, Semaphore** pp_wait_semaphores);
ApiExport void getFenceStatus(Fence* p_fence, FenceStatus* p_fence_status);
ApiExport void waitForFences(Queue* p_queue, uint32_t fence_count, Fence** pp_fences);

// image related functions
ApiExport bool isImageFormatSupported(ImageFormat::Enum format);

//indirect Draw functions
ApiExport void addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* p_desc, CommandSignature** ppCommandSignature);
ApiExport void removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature);
ApiExport void cmdExecuteIndirect(Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset);
/************************************************************************/
// Stats Info Interface
/************************************************************************/
void calculateMemoryStats(Renderer* pRenderer, char** stats);
void freeMemoryStats(Renderer* pRenderer, char* stats);
/************************************************************************/
// Debug Marker Interface
/************************************************************************/
void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName);
void cmdBeginDebugMarkerf(Cmd* pCmd, float r, float g, float b, const char* pFormat, ...);
void cmdEndDebugMarker(Cmd* pCmd);

void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName);
void cmdAddDebugMarkerf(Cmd* pCmd, float r, float g, float b, const char* pFormat, ...);
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void setName(Renderer* pRenderer, Buffer* pBuffer, const char* pName);
void setName(Renderer* pRenderer, Texture* pTexture, const char* pName);
/************************************************************************/
/************************************************************************/

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

#ifdef DIRECT3D12
#define RENDERER_IMPLEMENTATION
#define MAX_FRAMES_IN_FLIGHT 3U

#ifdef _DURANGO
#include "..\..\..\Xbox\CommonXBOXOne_3\OS\XBoxPrivateHeaders.h"
#else
#define IID_ARGS IID_PPV_ARGS
#endif

#include "../../ThirdParty/OpenSource/TinySTL/string.h"
#include "../../ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../../OS/Interfaces/ILogManager.h"
#include "../IRenderer.h"
#include "../../OS/Core/RingBuffer.h"
#include "../../ThirdParty/OpenSource/TinySTL/hash.h"
#include "../../ThirdParty/OpenSource/winpixeventruntime/Include/WinPixEventRuntime/pix3.h"
#include "../../OS/Core/GPUConfig.h"

#include "Direct3D12Hooks.h"

#if !defined(_WIN32)
#error "Windows is needed!"
#endif

//
// C++ is the only language supported by D3D12:
//   https://msdn.microsoft.com/en-us/library/windows/desktop/dn899120(v=vs.85).aspx
//
#if !defined(__cplusplus)
#error "D3D12 requires C++! Sorry!"
#endif

// Pull in minimal Windows headers
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#ifdef FORGE_JHABLE_EDITS_V01
// These are placed separately in Vulkan.cpp and Direct3D12.cpp, but they should be in one place
// with an option for the user to append custom attributes. However, it should be initialized once
// so that we can parse them during shader reflection, not during pipeline binding.
// clang-format off
static const char * g_hackSemanticList[] =
{
	"POSITION",
	"NORMAL",
	"UV",
	"COLOR",
	"TANGENT",
	"BINORMAL",
	"TANGENT_TW",
	"TEXCOORD",
};
// clang-format on
#endif

#if !defined(_DURANGO)
// Prefer Higher Performance GPU on switchable GPU systems
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 1;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#include "Direct3D12MemoryAllocator.h"
#include "../../OS/Interfaces/IMemoryManager.h"

extern void
			d3d12_createShaderReflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection);
extern long d3d12_createBuffer(
	MemoryAllocator* pAllocator, const BufferCreateInfo* pCreateInfo, const AllocatorMemoryRequirements* pMemoryRequirements,
	Buffer* pBuffer);
extern void d3d12_destroyBuffer(MemoryAllocator* pAllocator, struct Buffer* pBuffer);
extern long d3d12_createTexture(
	MemoryAllocator* pAllocator, const TextureCreateInfo* pCreateInfo, const AllocatorMemoryRequirements* pMemoryRequirements,
	Texture* pTexture);
extern void d3d12_destroyTexture(MemoryAllocator* pAllocator, struct Texture* pTexture);

// clang-format off
D3D12_BLEND_OP gDx12BlendOpTranslator[BlendMode::MAX_BLEND_MODES] =
{
	D3D12_BLEND_OP_ADD,
	D3D12_BLEND_OP_SUBTRACT,
	D3D12_BLEND_OP_REV_SUBTRACT,
	D3D12_BLEND_OP_MIN,
	D3D12_BLEND_OP_MAX,
};

D3D12_BLEND gDx12BlendConstantTranslator[BlendConstant::MAX_BLEND_CONSTANTS] =
{
	D3D12_BLEND_ZERO,
	D3D12_BLEND_ONE,
	D3D12_BLEND_SRC_COLOR,
	D3D12_BLEND_INV_SRC_COLOR,
	D3D12_BLEND_DEST_COLOR,
	D3D12_BLEND_INV_DEST_COLOR,
	D3D12_BLEND_SRC_ALPHA,
	D3D12_BLEND_INV_SRC_ALPHA,
	D3D12_BLEND_DEST_ALPHA,
	D3D12_BLEND_INV_DEST_ALPHA,
	D3D12_BLEND_SRC_ALPHA_SAT,
	D3D12_BLEND_BLEND_FACTOR,
	D3D12_BLEND_INV_BLEND_FACTOR,
};

D3D12_COMPARISON_FUNC gDx12ComparisonFuncTranslator[CompareMode::MAX_COMPARE_MODES] =
{
	D3D12_COMPARISON_FUNC_NEVER,
	D3D12_COMPARISON_FUNC_LESS,
	D3D12_COMPARISON_FUNC_EQUAL,
	D3D12_COMPARISON_FUNC_LESS_EQUAL,
	D3D12_COMPARISON_FUNC_GREATER,
	D3D12_COMPARISON_FUNC_NOT_EQUAL,
	D3D12_COMPARISON_FUNC_GREATER_EQUAL,
	D3D12_COMPARISON_FUNC_ALWAYS,
};

D3D12_STENCIL_OP gDx12StencilOpTranslator[StencilOp::MAX_STENCIL_OPS] =
{
	D3D12_STENCIL_OP_KEEP,
	D3D12_STENCIL_OP_ZERO,
	D3D12_STENCIL_OP_REPLACE,
	D3D12_STENCIL_OP_INVERT,
	D3D12_STENCIL_OP_INCR,
	D3D12_STENCIL_OP_DECR,
	D3D12_STENCIL_OP_INCR_SAT,
	D3D12_STENCIL_OP_DECR_SAT,
};

D3D12_CULL_MODE gDx12CullModeTranslator[MAX_CULL_MODES] =
{
	D3D12_CULL_MODE_NONE,
	D3D12_CULL_MODE_BACK,
	D3D12_CULL_MODE_FRONT,
};

D3D12_FILL_MODE gDx12FillModeTranslator[MAX_FILL_MODES] =
{
	D3D12_FILL_MODE_SOLID,
	D3D12_FILL_MODE_WIREFRAME,
};

const DXGI_FORMAT gDX12FormatTranslatorTypeless[] = {
	DXGI_FORMAT_UNKNOWN,
	DXGI_FORMAT_R8_TYPELESS,
	DXGI_FORMAT_R8G8_TYPELESS,
	DXGI_FORMAT_UNKNOWN,
	DXGI_FORMAT_R8G8B8A8_TYPELESS,
	DXGI_FORMAT_R16_TYPELESS,
	DXGI_FORMAT_R16G16_TYPELESS,
	DXGI_FORMAT_UNKNOWN,
	DXGI_FORMAT_R16G16B16A16_TYPELESS,
	DXGI_FORMAT_R8_TYPELESS,
	DXGI_FORMAT_R16G16_TYPELESS,
	DXGI_FORMAT_UNKNOWN,							// ImageFormat::RGB8S not directly supported
	DXGI_FORMAT_R8G8B8A8_TYPELESS,
	DXGI_FORMAT_R32_TYPELESS,
	DXGI_FORMAT_R32G32_TYPELESS,
	DXGI_FORMAT_UNKNOWN,  // RGB16S not directly supported
	DXGI_FORMAT_R32G32B32A32_TYPELESS,
	DXGI_FORMAT_R16_TYPELESS,
	DXGI_FORMAT_R16G16_TYPELESS,
	DXGI_FORMAT_UNKNOWN,  // RGB16F not directly supported
	DXGI_FORMAT_R16G16B16A16_TYPELESS,
	DXGI_FORMAT_R32_TYPELESS,
	DXGI_FORMAT_R32G32_TYPELESS,
	DXGI_FORMAT_R32G32B32_TYPELESS,
	DXGI_FORMAT_R32G32B32A32_TYPELESS,
	DXGI_FORMAT_R16_TYPELESS,
	DXGI_FORMAT_R16G16_TYPELESS,
	DXGI_FORMAT_UNKNOWN,  // RGB16I not directly supported
	DXGI_FORMAT_R16G16B16A16_TYPELESS,
	DXGI_FORMAT_R32_TYPELESS,
	DXGI_FORMAT_R32G32_TYPELESS,
	DXGI_FORMAT_R32G32B32_TYPELESS,
	DXGI_FORMAT_R32G32B32A32_TYPELESS,
	DXGI_FORMAT_R16_TYPELESS,
	DXGI_FORMAT_R16G16_TYPELESS,
	DXGI_FORMAT_UNKNOWN,  // RGB16UI not directly supported
	DXGI_FORMAT_R16G16B16A16_TYPELESS,
	DXGI_FORMAT_R32_TYPELESS,
	DXGI_FORMAT_R32G32_TYPELESS,
	DXGI_FORMAT_R32G32B32_TYPELESS,
	DXGI_FORMAT_R32G32B32A32_TYPELESS,
	DXGI_FORMAT_UNKNOWN,  // RGBE8 not directly supported
	DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
	DXGI_FORMAT_R11G11B10_FLOAT,
	DXGI_FORMAT_B5G6R5_UNORM,
	DXGI_FORMAT_UNKNOWN,  // RGBA4 not directly supported
	DXGI_FORMAT_R10G10B10A2_TYPELESS,
	DXGI_FORMAT_R16_TYPELESS,
	DXGI_FORMAT_R24G8_TYPELESS,
	DXGI_FORMAT_R24G8_TYPELESS,
	DXGI_FORMAT_R32_TYPELESS,  //D32F
	DXGI_FORMAT_BC1_TYPELESS,
	DXGI_FORMAT_BC2_TYPELESS,
	DXGI_FORMAT_BC3_TYPELESS,
	DXGI_FORMAT_BC4_TYPELESS, //ATI2N
	DXGI_FORMAT_BC5_TYPELESS, //ATI2N
	// PVR formats
	DXGI_FORMAT_UNKNOWN, // PVR_2BPP = 56,
	DXGI_FORMAT_UNKNOWN, // PVR_2BPPA = 57,
	DXGI_FORMAT_UNKNOWN, // PVR_4BPP = 58,
	DXGI_FORMAT_UNKNOWN, // PVR_4BPPA = 59,
	DXGI_FORMAT_UNKNOWN, // INTZ = 60,  //  NVidia hack. Supported on all DX10+ HW
	//  XBox 360 specific fron buffer formats. NOt listed in other renderers. Please, add them when extend this structure.
	DXGI_FORMAT_UNKNOWN, // LE_XRGB8 = 61,
	DXGI_FORMAT_UNKNOWN, // LE_ARGB8 = 62,
	DXGI_FORMAT_UNKNOWN, // LE_X2RGB10 = 63,
	DXGI_FORMAT_UNKNOWN, // LE_A2RGB10 = 64,
	// compressed mobile forms
	DXGI_FORMAT_UNKNOWN, // ETC1 = 65,  //  RGB
	DXGI_FORMAT_UNKNOWN, // ATC = 66,   //  RGB
	DXGI_FORMAT_UNKNOWN, // ATCA = 67,  //  RGBA, explicit alpha
	DXGI_FORMAT_UNKNOWN, // ATCI = 68,  //  RGBA, interpolated alpha
	DXGI_FORMAT_UNKNOWN, // RAWZ = 69, //depth only, Nvidia (requires recombination of data) //FIX IT: PS3 as well?
	DXGI_FORMAT_UNKNOWN, // DF16 = 70, //depth only, Intel/AMD
	DXGI_FORMAT_UNKNOWN, // STENCILONLY = 71, // stencil ony usage
	DXGI_FORMAT_UNKNOWN, // GNF_BC1 = 72,
	DXGI_FORMAT_UNKNOWN, // GNF_BC2 = 73,
	DXGI_FORMAT_UNKNOWN, // GNF_BC3 = 74,
	DXGI_FORMAT_UNKNOWN, // GNF_BC4 = 75,
	DXGI_FORMAT_UNKNOWN, // GNF_BC5 = 76,
#ifdef FORGE_JHABLE_EDITS_V01
	// should have 2 bc6h formats
	DXGI_FORMAT_BC6H_SF16, // GNF_BC6 = 77,
	DXGI_FORMAT_BC7_UNORM, // GNF_BC7 = 78,
#else
	DXGI_FORMAT_UNKNOWN, // GNF_BC6 = 77,
	DXGI_FORMAT_UNKNOWN, // GNF_BC7 = 78,
#endif
	// Reveser Form
	DXGI_FORMAT_B8G8R8A8_UNORM, // BGRA8 = 79,
	// Extend for DXGI
	DXGI_FORMAT_UNKNOWN, // X8D24PAX32 = 80,
	DXGI_FORMAT_UNKNOWN, // S8 = 81,
	DXGI_FORMAT_UNKNOWN, // D16S8 = 82,
	DXGI_FORMAT_UNKNOWN, // D32S8 = 83,
};
const DXGI_FORMAT gDX12FormatTranslator[] = {
	DXGI_FORMAT_UNKNOWN,							// ImageFormat::NONE
	DXGI_FORMAT_R8_UNORM,						   // ImageFormat::R8
	DXGI_FORMAT_R8G8_UNORM,						 // ImageFormat::RG8
	DXGI_FORMAT_UNKNOWN,							// ImageFormat::RGB8 not directly supported
	DXGI_FORMAT_R8G8B8A8_UNORM,					 // ImageFormat::RGBA8
	DXGI_FORMAT_R16_UNORM,						  // ImageFormat::R16
	DXGI_FORMAT_R16G16_UNORM,					   // ImageFormat::RG16
	DXGI_FORMAT_UNKNOWN,							// ImageFormat::RGB16 not directly supported
	DXGI_FORMAT_R16G16B16A16_UNORM,				 // ImageFormat::RGBA16
	DXGI_FORMAT_R8_SNORM,						   // ImageFormat::R8S
	DXGI_FORMAT_R8G8_SNORM,						 // ImageFormat::RG8S
	DXGI_FORMAT_UNKNOWN,							// ImageFormat::RGB8S not directly supported
	DXGI_FORMAT_R8G8B8A8_SNORM,
	DXGI_FORMAT_R16_SNORM,
	DXGI_FORMAT_R16G16_SNORM,
	DXGI_FORMAT_UNKNOWN,  // RGB16S not directly supported
	DXGI_FORMAT_R16G16B16A16_SNORM,
	DXGI_FORMAT_R16_FLOAT,
	DXGI_FORMAT_R16G16_FLOAT,
	DXGI_FORMAT_UNKNOWN,  // RGB16F not directly supported
	DXGI_FORMAT_R16G16B16A16_FLOAT,
	DXGI_FORMAT_R32_FLOAT,
	DXGI_FORMAT_R32G32_FLOAT,
	DXGI_FORMAT_R32G32B32_FLOAT,
	DXGI_FORMAT_R32G32B32A32_FLOAT,
	DXGI_FORMAT_R16_SINT,
	DXGI_FORMAT_R16G16_SINT,
	DXGI_FORMAT_UNKNOWN,  // RGB16I not directly supported
	DXGI_FORMAT_R16G16B16A16_SINT,
	DXGI_FORMAT_R32_SINT,
	DXGI_FORMAT_R32G32_SINT,
	DXGI_FORMAT_R32G32B32_SINT,
	DXGI_FORMAT_R32G32B32A32_SINT,
	DXGI_FORMAT_R16_UINT,
	DXGI_FORMAT_R16G16_UINT,
	DXGI_FORMAT_UNKNOWN,  // RGB16UI not directly supported
	DXGI_FORMAT_R16G16B16A16_UINT,
	DXGI_FORMAT_R32_UINT,
	DXGI_FORMAT_R32G32_UINT,
	DXGI_FORMAT_R32G32B32_UINT,
	DXGI_FORMAT_R32G32B32A32_UINT,
	DXGI_FORMAT_UNKNOWN,  // RGBE8 not directly supported
	DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
	DXGI_FORMAT_R11G11B10_FLOAT,
	DXGI_FORMAT_B5G6R5_UNORM,
	DXGI_FORMAT_UNKNOWN,  // RGBA4 not directly supported
	DXGI_FORMAT_R10G10B10A2_UNORM,
	DXGI_FORMAT_D16_UNORM,
	DXGI_FORMAT_D24_UNORM_S8_UINT,
	DXGI_FORMAT_D24_UNORM_S8_UINT,
	DXGI_FORMAT_D32_FLOAT,  //D32F
	DXGI_FORMAT_BC1_UNORM,
	DXGI_FORMAT_BC2_UNORM,
	DXGI_FORMAT_BC3_UNORM,
	DXGI_FORMAT_BC4_UNORM, //ATI2N
	DXGI_FORMAT_BC5_UNORM, //ATI2N
	// PVR formats
	DXGI_FORMAT_UNKNOWN, // PVR_2BPP = 56,
	DXGI_FORMAT_UNKNOWN, // PVR_2BPPA = 57,
	DXGI_FORMAT_UNKNOWN, // PVR_4BPP = 58,
	DXGI_FORMAT_UNKNOWN, // PVR_4BPPA = 59,
	DXGI_FORMAT_UNKNOWN, // INTZ = 60,  //  NVidia hack. Supported on all DX10+ HW
	//  XBox 360 specific fron buffer formats. NOt listed in other renderers. Please, add them when extend this structure.
	DXGI_FORMAT_UNKNOWN, // LE_XRGB8 = 61,
	DXGI_FORMAT_UNKNOWN, // LE_ARGB8 = 62,
	DXGI_FORMAT_UNKNOWN, // LE_X2RGB10 = 63,
	DXGI_FORMAT_UNKNOWN, // LE_A2RGB10 = 64,
	// compressed mobile forms
	DXGI_FORMAT_UNKNOWN, // ETC1 = 65,  //  RGB
	DXGI_FORMAT_UNKNOWN, // ATC = 66,   //  RGB
	DXGI_FORMAT_UNKNOWN, // ATCA = 67,  //  RGBA, explicit alpha
	DXGI_FORMAT_UNKNOWN, // ATCI = 68,  //  RGBA, interpolated alpha
	DXGI_FORMAT_UNKNOWN, // RAWZ = 69, //depth only, Nvidia (requires recombination of data) //FIX IT: PS3 as well?
	DXGI_FORMAT_UNKNOWN, // DF16 = 70, //depth only, Intel/AMD
	DXGI_FORMAT_UNKNOWN, // STENCILONLY = 71, // stencil ony usage
	DXGI_FORMAT_UNKNOWN, // GNF_BC1 = 72,
	DXGI_FORMAT_UNKNOWN, // GNF_BC2 = 73,
	DXGI_FORMAT_UNKNOWN, // GNF_BC3 = 74,
	DXGI_FORMAT_UNKNOWN, // GNF_BC4 = 75,
	DXGI_FORMAT_UNKNOWN, // GNF_BC5 = 76,
#ifdef FORGE_JHABLE_EDITS_V01
	DXGI_FORMAT_BC6H_SF16, // GNF_BC6 = 77,
	DXGI_FORMAT_BC7_UNORM, // GNF_BC7 = 78,
#else
	DXGI_FORMAT_UNKNOWN, // GNF_BC6 = 77,
	DXGI_FORMAT_UNKNOWN, // GNF_BC7 = 78,
#endif
	// Reveser Form
	DXGI_FORMAT_B8G8R8A8_UNORM, // BGRA8 = 79,
	// Extend for DXGI
	DXGI_FORMAT_UNKNOWN, // X8D24PAX32 = 80,
	DXGI_FORMAT_UNKNOWN, // S8 = 81,
	DXGI_FORMAT_UNKNOWN, // D16S8 = 82,
	DXGI_FORMAT_UNKNOWN, // D32S8 = 83,
};

const D3D12_COMMAND_LIST_TYPE gDx12CmdTypeTranslator[CmdPoolType::MAX_CMD_TYPE] =
{
	D3D12_COMMAND_LIST_TYPE_DIRECT,
	D3D12_COMMAND_LIST_TYPE_BUNDLE,
	D3D12_COMMAND_LIST_TYPE_COPY,
	D3D12_COMMAND_LIST_TYPE_COMPUTE
};

const D3D12_COMMAND_QUEUE_FLAGS gDx12QueueFlagTranslator[QueueFlag::MAX_QUEUE_FLAG] =
{
	D3D12_COMMAND_QUEUE_FLAG_NONE,
	D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT
};

const D3D12_COMMAND_QUEUE_PRIORITY gDx12QueuePriorityTranslator[QueuePriority::MAX_QUEUE_PRIORITY]
{
	D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
	D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
#ifndef _DURANGO
	D3D12_COMMAND_QUEUE_PRIORITY_GLOBAL_REALTIME,
#endif
};
// clang-format on

// =================================================================================================
// IMPLEMENTATION
// =================================================================================================

#if defined(RENDERER_IMPLEMENTATION)

#ifndef _DURANGO
#include "../../../Common_3/ThirdParty/OpenSource/DirectXShaderCompiler/dxcapi.use.h"
#include <d3dcompiler.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#endif

#define SAFE_FREE(p_var)  \
	if (p_var)            \
	{                     \
		conf_free(p_var); \
	}

#if defined(__cplusplus)
#define DECLARE_ZERO(type, var) type var = {};
#else
#define DECLARE_ZERO(type, var) type var = { 0 };
#endif

#define SAFE_RELEASE(p_var) \
	if (p_var)              \
	{                       \
		p_var->Release();   \
		p_var = NULL;       \
	}

// Internal utility functions (may become external one day)
uint64_t                    util_dx_determine_storage_counter_offset(uint64_t buffer_size);
DXGI_FORMAT                 util_to_dx_image_format_typeless(ImageFormat::Enum format);
DXGI_FORMAT                 util_to_dx_uav_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx_dsv_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx_srv_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx_stencil_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT                 util_to_dx_image_format(ImageFormat::Enum format, bool srgb);
DXGI_FORMAT                 util_to_dx_swapchain_format(ImageFormat::Enum format);
D3D12_SHADER_VISIBILITY     util_to_dx_shader_visibility(ShaderStage stages);
D3D12_DESCRIPTOR_RANGE_TYPE util_to_dx_descriptor_range(DescriptorType type);
D3D12_RESOURCE_STATES       util_to_dx_resource_state(ResourceState state);
D3D12_FILTER util_to_dx_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool aniso, bool comparisonFilterEnabled);
D3D12_TEXTURE_ADDRESS_MODE    util_to_dx_texture_address_mode(AddressMode addressMode);
D3D12_PRIMITIVE_TOPOLOGY_TYPE util_to_dx_primitive_topology_type(PrimitiveTopology topology);

//
// internal functions start with a capital letter / API starts with a small letter

// internal functions are capital first letter and capital letter of the next word

// Functions points for functions that need to be loaded
PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER           fnD3D12CreateRootSignatureDeserializer = NULL;
PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE           fnD3D12SerializeVersionedRootSignature = NULL;
PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER fnD3D12CreateVersionedRootSignatureDeserializer = NULL;

// Declare hooks for platform specific behavior
PFN_HOOK_ADD_DESCRIPTIOR_HEAP              fnHookAddDescriptorHeap = NULL;
PFN_HOOK_POST_INIT_RENDERER                fnHookPostInitRenderer = NULL;
PFN_HOOK_ADD_BUFFER                        fnHookAddBuffer = NULL;
PFN_HOOK_ENABLE_DEBUG_LAYER                fnHookEnableDebugLayer = NULL;
PFN_HOOK_HEAP_DESC                         fnHookHeapDesc = NULL;
PFN_HOOK_GET_RECOMMENDED_SWAP_CHAIN_FORMAT fnHookGetRecommendedSwapChainFormat = NULL;
PFN_HOOK_MODIFY_SWAP_CHAIN_DESC            fnHookModifySwapChainDesc = NULL;
PFN_HOOK_GET_SWAP_CHAIN_IMAGE_INDEX        fnHookGetSwapChainImageIndex = NULL;
PFN_HOOK_SHADER_COMPILE_FLAGS              fnHookShaderCompileFlags = NULL;
PFN_HOOK_RESOURCE_ALLOCATION_INFO          fnHookResourceAllocationInfo = NULL;
PFN_HOOK_SPECIAL_BUFFER_ALLOCATION         fnHookSpecialBufferAllocation = NULL;
PFN_HOOK_SPECIAL_TEXTURE_ALLOCATION        fnHookSpecialTextureAllocation = NULL;
PFN_HOOK_RESOURCE_FLAGS                    fnHookResourceFlags = NULL;
/************************************************************************/
// Descriptor Heap Defines
/************************************************************************/
#define D3D12_GPU_VIRTUAL_ADDRESS_NULL ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN ((D3D12_GPU_VIRTUAL_ADDRESS)-1)

typedef struct DescriptorHeapProperties
{
	uint32_t                    mMaxDescriptors;
	D3D12_DESCRIPTOR_HEAP_FLAGS mFlags;
} DescriptorHeapProperties;

DescriptorHeapProperties gCpuDescriptorHeapProperties[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {
	{ 1024 * 256, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },    // CBV SRV UAV
	{ 2048, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },          // Sampler
	{ 512, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },           // RTV
	{ 512, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },           // DSV
};

DescriptorHeapProperties gGpuDescriptorHeapProperties[2] = {
	{ 1024 * 256, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE },    // CBV SRV UAV
	{ 2048, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE },          // Sampler
};
/************************************************************************/
// Descriptor Heap Structures
/************************************************************************/
/// CPU Visible Heap to store all the resources needing CPU read / write operations - Textures/Buffers/RTV
typedef struct DescriptorStoreHeap
{
	uint32_t mNumDescriptors;
	/// DescriptorInfo Increment Size
	uint32_t mDescriptorSize;
	/// Bitset for finding SAFE_FREE descriptor slots
	uint32_t* flags;
	/// Lock for multi-threaded descriptor allocations
	Mutex*   pAllocationMutex;
	uint64_t mUsedDescriptors;
	/// Type of descriptor heap -> CBV / DSV / ...
	D3D12_DESCRIPTOR_HEAP_TYPE mType;
	/// DX Heap
	ID3D12DescriptorHeap* pCurrentHeap;
	/// Start position in the heap
	D3D12_CPU_DESCRIPTOR_HANDLE mStartCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mStartGpuHandle;
} DescriptorStoreHeap;
/************************************************************************/
// Static Descriptor Heap Implementation
/************************************************************************/
static void add_descriptor_heap(
	ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, uint32_t numDescriptors, uint32_t nodeMask,
	DescriptorStoreHeap** ppDescHeap)
{
	if (fnHookAddDescriptorHeap != NULL)
		numDescriptors = fnHookAddDescriptorHeap(type, numDescriptors);

	DescriptorStoreHeap* pHeap = (DescriptorStoreHeap*)conf_calloc(1, sizeof(*pHeap));

	// Need new since object allocates memory in constructor
	pHeap->pAllocationMutex = conf_placement_new<Mutex>(conf_calloc(1, sizeof(Mutex)));

	// Keep 32 aligned for easy remove
	numDescriptors = round_up(numDescriptors, 32);

	D3D12_DESCRIPTOR_HEAP_DESC Desc;
	Desc.Type = type;
	Desc.NumDescriptors = numDescriptors;
	Desc.Flags = flags;
	Desc.NodeMask = nodeMask;

	HRESULT hres = pDevice->CreateDescriptorHeap(&Desc, IID_ARGS(&pHeap->pCurrentHeap));
	ASSERT(SUCCEEDED(hres));

	pHeap->mNumDescriptors = numDescriptors;
	pHeap->mType = type;
	pHeap->mStartCpuHandle = pHeap->pCurrentHeap->GetCPUDescriptorHandleForHeapStart();
	pHeap->mStartGpuHandle = pHeap->pCurrentHeap->GetGPUDescriptorHandleForHeapStart();
	pHeap->mDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(type);

	const size_t sizeInBytes = pHeap->mNumDescriptors / 32 * sizeof(uint32_t);
	pHeap->flags = (uint32_t*)conf_calloc(1, sizeInBytes);

	*ppDescHeap = pHeap;
}

/// Resets the CPU Handle to start of heap and clears all stored resource ids
static void reset_descriptor_heap(DescriptorStoreHeap* pHeap)
{
	pHeap->mStartCpuHandle = pHeap->pCurrentHeap->GetCPUDescriptorHandleForHeapStart();
	pHeap->mStartGpuHandle = pHeap->pCurrentHeap->GetGPUDescriptorHandleForHeapStart();
	memset(pHeap->flags, 0, pHeap->mNumDescriptors / 32 * sizeof(uint32_t));
}

static void remove_descriptor_heap(DescriptorStoreHeap* pHeap)
{
	SAFE_RELEASE(pHeap->pCurrentHeap);
	SAFE_FREE(pHeap->flags);

	// Need delete since object frees allocated memory in destructor
	pHeap->pAllocationMutex->~Mutex();
	conf_free(pHeap->pAllocationMutex);

	SAFE_FREE(pHeap);
}

D3D12_CPU_DESCRIPTOR_HANDLE
add_cpu_descriptor_handles(DescriptorStoreHeap* pHeap, uint32_t numDescriptors, uint32_t* pDescriptorIndex = NULL)
{
	int       result = -1;
	MutexLock lockGuard(*pHeap->pAllocationMutex);

	tinystl::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles;

	for (uint32_t i = 0; i < pHeap->mNumDescriptors / 32; ++i)
	{
		const uint32_t flag = pHeap->flags[i];
		if (flag == 0xffffffff)
		{
			for (D3D12_CPU_DESCRIPTOR_HANDLE& handle : handles)
			{
				uint32_t       id = (uint32_t)((handle.ptr - pHeap->mStartCpuHandle.ptr) / pHeap->mDescriptorSize);
				const uint32_t x = id / 32;
				const uint32_t mask = ~(1 << (id % 32));
				pHeap->flags[x] &= mask;
			}
			handles.clear();
			continue;
		}

		for (int j = 0, mask = 1; j < 32; ++j, mask <<= 1)
		{
			if ((flag & mask) == 0)
			{
				pHeap->flags[i] |= mask;
				result = i * 32 + j;

				ASSERT(result != -1 && "Out of descriptors");

				handles.push_back({
					{ pHeap->mStartCpuHandle.ptr + (result * pHeap->mDescriptorSize) },
				});

				if ((uint32_t)handles.size() == numDescriptors)
				{
					if (pDescriptorIndex)
						*pDescriptorIndex = (uint32_t)((handles.front().ptr - pHeap->mStartCpuHandle.ptr) / pHeap->mDescriptorSize);
					pHeap->mUsedDescriptors += numDescriptors;
					return handles.front();
				}
			}
		}
	}

	ASSERT(result != -1 && "Out of descriptors");
	if (pDescriptorIndex)
		*pDescriptorIndex = (uint32_t)((handles.front().ptr - pHeap->mStartCpuHandle.ptr) / pHeap->mDescriptorSize);
	pHeap->mUsedDescriptors += numDescriptors;
	return handles.front();
}

void add_gpu_descriptor_handles(
	DescriptorStoreHeap* pHeap, D3D12_CPU_DESCRIPTOR_HANDLE* pStartCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* pStartGpuHandle,
	uint32_t numDescriptors)
{
	int       result = -1;
	MutexLock lockGuard(*pHeap->pAllocationMutex);

	tinystl::vector<tinystl::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> > handles;

	for (uint32_t i = 0; i < pHeap->mNumDescriptors / 32; ++i)
	{
		const uint32_t flag = pHeap->flags[i];
		if (flag == 0xffffffff)
		{
			for (tinystl::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>& handle : handles)
			{
				uint32_t       id = (uint32_t)((handle.first.ptr - pHeap->mStartCpuHandle.ptr) / pHeap->mDescriptorSize);
				const uint32_t x = id / 32;
				const uint32_t mask = ~(1 << (id % 32));
				pHeap->flags[x] &= mask;
			}
			handles.clear();
			continue;
		}

		for (int j = 0, mask = 1; j < 32; ++j, mask <<= 1)
		{
			if ((flag & mask) == 0)
			{
				pHeap->flags[i] |= mask;
				result = i * 32 + j;

				ASSERT(result != -1 && "Out of descriptors");

				handles.push_back({
					{ pHeap->mStartCpuHandle.ptr + (result * pHeap->mDescriptorSize) },
					{ pHeap->mStartGpuHandle.ptr + (result * pHeap->mDescriptorSize) },
				});

				if ((uint32_t)handles.size() == numDescriptors)
				{
					*pStartCpuHandle = handles.front().first;
					*pStartGpuHandle = handles.front().second;
					pHeap->mUsedDescriptors += numDescriptors;
					return;
				}
			}
		}
	}

	ASSERT(result != -1 && "Out of descriptors");
	*pStartCpuHandle = handles.front().first;
	*pStartGpuHandle = handles.front().second;
	pHeap->mUsedDescriptors += numDescriptors;
}

void remove_gpu_descriptor_handles(DescriptorStoreHeap* pHeap, D3D12_GPU_DESCRIPTOR_HANDLE* startHandle, uint64_t numDescriptors)
{
	MutexLock lockGuard(*pHeap->pAllocationMutex);

	for (uint32_t idx = 0; idx < numDescriptors; ++idx)
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle = { startHandle->ptr + idx * pHeap->mDescriptorSize };
		uint32_t                    id = (uint32_t)((handle.ptr - pHeap->mStartGpuHandle.ptr) / pHeap->mDescriptorSize);

		const uint32_t i = id / 32;
		const uint32_t mask = ~(1 << (id % 32));
		pHeap->flags[i] &= mask;
	}
	pHeap->mUsedDescriptors -= numDescriptors;
}

void remove_cpu_descriptor_handles(DescriptorStoreHeap* pHeap, D3D12_CPU_DESCRIPTOR_HANDLE* startHandle, uint64_t numDescriptors)
{
	MutexLock lockGuard(*pHeap->pAllocationMutex);

	for (uint32_t idx = 0; idx < numDescriptors; ++idx)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle = { startHandle->ptr + idx * pHeap->mDescriptorSize };
		uint32_t                    id = (uint32_t)((handle.ptr - pHeap->mStartCpuHandle.ptr) / pHeap->mDescriptorSize);

		const uint32_t i = id / 32;
		const uint32_t mask = ~(1 << (id % 32));
		pHeap->flags[i] &= mask;
	}
	pHeap->mUsedDescriptors -= numDescriptors;
}
/************************************************************************/
// Descriptor Manager Implementation
/************************************************************************/
/// Descriptor table structure holding the native descriptor set handle
typedef struct DescriptorTable
{
	/// Handle to the start of the cbv_srv_uav descriptor table in the gpu visible cbv_srv_uav heap
	D3D12_CPU_DESCRIPTOR_HANDLE mBaseCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mBaseGpuHandle;
	uint32_t                    mDescriptorCount;
	uint32_t                    mNodeIndex;
} DescriptorTable;

using DescriptorTableMap = tinystl::unordered_map<uint64_t, DescriptorTable>;
using ConstDescriptorTableMapIterator = tinystl::unordered_map<uint64_t, DescriptorTable>::const_iterator;
using DescriptorTableMapNode = tinystl::unordered_hash_node<uint64_t, DescriptorTable>;
using DescriptorNameToIndexMap = tinystl::unordered_map<uint32_t, uint32_t>;

typedef struct DescriptorManager
{
	/// The root signature associated with this descriptor manager
	RootSignature* pRootSignature;
	/// Array of flags to check whether a descriptor table of the update frequency is already bound to avoid unnecessary rebinding of descriptor tables
	bool mBoundCbvSrvUavTables[DESCRIPTOR_UPDATE_FREQ_COUNT];
	bool mBoundSamplerTables[DESCRIPTOR_UPDATE_FREQ_COUNT];
	/// Array of view descriptor handles per update frequency to be copied into the gpu visible view heap
	D3D12_CPU_DESCRIPTOR_HANDLE* pViewDescriptorHandles[DESCRIPTOR_UPDATE_FREQ_COUNT];
	/// Array of sampler descriptor handles per update frequency to be copied into the gpu visible sampler heap
	D3D12_CPU_DESCRIPTOR_HANDLE* pSamplerDescriptorHandles[DESCRIPTOR_UPDATE_FREQ_COUNT];
	/// Triple buffered Hash map to check if a descriptor table with a descriptor hash already exists to avoid redundant copy descriptors operations
	DescriptorTableMap mStaticCbvSrvUavDescriptorTableMap[MAX_FRAMES_IN_FLIGHT];
	DescriptorTableMap mStaticSamplerDescriptorTableMap[MAX_FRAMES_IN_FLIGHT];
	/// Triple buffered array of number of descriptor tables allocated per update frequency
	/// Only used for recording stats
	uint32_t mDescriptorTableCount[MAX_FRAMES_IN_FLIGHT][DESCRIPTOR_UPDATE_FREQ_COUNT];

	Cmd*     pCurrentCmd;
	uint32_t mFrameIdx;
} DescriptorManager;

Mutex gDescriptorMutex;

static void add_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature, DescriptorManager** ppManager)
{
	DescriptorManager* pManager = (DescriptorManager*)conf_calloc(1, sizeof(*pManager));
	pManager->pRootSignature = pRootSignature;
	pManager->mFrameIdx = (uint32_t)-1;

	const uint32_t setCount = DESCRIPTOR_UPDATE_FREQ_COUNT;

	// Fill the descriptor handles with null descriptors
	for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
	{
		const uint32_t viewCount = pRootSignature->mDxViewDescriptorCounts[setIndex];
		const uint32_t samplerCount = pRootSignature->mDxSamplerDescriptorCounts[setIndex];
		const uint32_t descCount = viewCount + samplerCount;

		if (viewCount)
		{
			pManager->pViewDescriptorHandles[setIndex] = (D3D12_CPU_DESCRIPTOR_HANDLE*)conf_calloc(
				pRootSignature->mDxCumulativeViewDescriptorCounts[setIndex], sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));

			for (uint32_t i = 0; i < viewCount; ++i)
			{
				const DescriptorInfo* pDesc = &pRootSignature->pDescriptors[pRootSignature->pDxViewDescriptorIndices[setIndex][i]];
				DescriptorType        type = pDesc->mDesc.type;
				switch (type)
				{
					case DESCRIPTOR_TYPE_TEXTURE:
					case DESCRIPTOR_TYPE_BUFFER:
						for (uint32_t j = 0; j < pDesc->mDesc.size; ++j)
							pManager->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = pRenderer->mSrvNullDescriptor;
						break;
					case DESCRIPTOR_TYPE_RW_TEXTURE:
					case DESCRIPTOR_TYPE_RW_BUFFER:
						for (uint32_t j = 0; j < pDesc->mDesc.size; ++j)
							pManager->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = pRenderer->mUavNullDescriptor;
						break;
					case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
						for (uint32_t j = 0; j < pDesc->mDesc.size; ++j)
							pManager->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = pRenderer->mCbvNullDescriptor;
						break;
					default: break;
				}
			}
		}

		if (samplerCount)
		{
			pManager->pSamplerDescriptorHandles[setIndex] = (D3D12_CPU_DESCRIPTOR_HANDLE*)conf_calloc(
				pRootSignature->mDxCumulativeSamplerDescriptorCounts[setIndex], sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));
			for (uint32_t i = 0; i < samplerCount; ++i)
			{
				pManager->pSamplerDescriptorHandles[setIndex][i] = pRenderer->mSamplerNullDescriptor;
			}
		}
	}

	*ppManager = pManager;
}

static void remove_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature, DescriptorManager* pManager)
{
	UNREF_PARAM(pRenderer);
	const uint32_t setCount = DESCRIPTOR_UPDATE_FREQ_COUNT;
	const uint32_t frameCount = MAX_FRAMES_IN_FLIGHT;

	for (uint32_t frameIdx = 0; frameIdx < frameCount; ++frameIdx)
	{
		for (DescriptorTableMapNode table : pManager->mStaticCbvSrvUavDescriptorTableMap[frameIdx])
		{
			remove_gpu_descriptor_handles(
				pRenderer->pCbvSrvUavHeap[table.second.mNodeIndex], &table.second.mBaseGpuHandle, (uint64_t)table.second.mDescriptorCount);
		}
		for (DescriptorTableMapNode table : pManager->mStaticSamplerDescriptorTableMap[frameIdx])
		{
			remove_gpu_descriptor_handles(
				pRenderer->pSamplerHeap[table.second.mNodeIndex], &table.second.mBaseGpuHandle, (uint64_t)table.second.mDescriptorCount);
		}
		pManager->mStaticCbvSrvUavDescriptorTableMap[frameIdx].~DescriptorTableMap();
		pManager->mStaticSamplerDescriptorTableMap[frameIdx].~DescriptorTableMap();
	}

	// Free staging data tables
	for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
	{
		SAFE_FREE(pManager->pViewDescriptorHandles[setIndex]);
		SAFE_FREE(pManager->pSamplerDescriptorHandles[setIndex]);
	}

	SAFE_FREE(pManager);
}

// This function returns the descriptor manager belonging to this thread
// If a descriptor manager does not exist for this thread, a new one is created
// With this approach we make sure that descriptor binding is thread safe and lock conf_free at the same time
static DescriptorManager* get_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature)
{
	tinystl::unordered_hash_node<ThreadID, DescriptorManager*>* pNode =
		pRootSignature->pDescriptorManagerMap.find(Thread::GetCurrentThreadID()).node;
	if (pNode == NULL)
	{
		// Only need a lock when creating a new descriptor manager for this thread
		MutexLock          lock(gDescriptorMutex);
		DescriptorManager* pManager = NULL;
		add_descriptor_manager(pRenderer, pRootSignature, &pManager);
		pRootSignature->pDescriptorManagerMap.insert({ Thread::GetCurrentThreadID(), pManager });
		return pManager;
	}
	else
	{
		return pNode->second;
	}
}

const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName, uint32_t* pIndex)
{
	DescriptorNameToIndexMap::const_iterator it = pRootSignature->pDescriptorNameToIndexMap.find(tinystl::hash(pResName));
	if (it.node)
	{
		*pIndex = it.node->second;
		return &pRootSignature->pDescriptors[it.node->second];
	}
	else
	{
		LOGERRORF("Invalid descriptor param (%s)", pResName);
		return NULL;
	}
}
/************************************************************************/
// Get renderer shader macros
/************************************************************************/
#define MAX_DYNAMIC_VIEW_DESCRIPTORS_PER_FRAME gGpuDescriptorHeapProperties[0].mMaxDescriptors / 16
#define MAX_DYNAMIC_SAMPLER_DESCRIPTORS_PER_FRAME gGpuDescriptorHeapProperties[1].mMaxDescriptors / 16
#define MAX_TRANSIENT_CBVS_PER_FRAME 256U
/************************************************************************/
// Gloabals
/************************************************************************/
static const uint32_t gDescriptorTableDWORDS = 1;
static const uint32_t gRootDescriptorDWORDS = 2;

static volatile uint64_t gBufferIds = 0;
static volatile uint64_t gTextureIds = 0;
static volatile uint64_t gSamplerIds = 0;

static uint32_t gMaxRootConstantsPerRootParam = 4U;
/************************************************************************/
// Logging functions
/************************************************************************/
// Proxy log callback
static void internal_log(LogType type, const char* msg, const char* component)
{
	switch (type)
	{
		case LOG_TYPE_INFO: LOGINFOF("%s ( %s )", component, msg); break;
		case LOG_TYPE_WARN: LOGWARNINGF("%s ( %s )", component, msg); break;
		case LOG_TYPE_DEBUG: LOGDEBUGF("%s ( %s )", component, msg); break;
		case LOG_TYPE_ERROR: LOGERRORF("%s ( %s )", component, msg); break;
		default: break;
	}
}

static void add_srv(
	Renderer* pRenderer, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	*pHandle = add_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], 1);
	pRenderer->pDxDevice->CreateShaderResourceView(pResource, pSrvDesc, *pHandle);
}

static void remove_srv(Renderer* pRenderer, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	remove_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], pHandle, 1);
}

static void add_uav(
	Renderer* pRenderer, ID3D12Resource* pResource, ID3D12Resource* pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc,
	D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	*pHandle = add_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], 1);
	pRenderer->pDxDevice->CreateUnorderedAccessView(pResource, pCounterResource, pUavDesc, *pHandle);
}

static void remove_uav(Renderer* pRenderer, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	remove_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], pHandle, 1);
}

static void add_cbv(Renderer* pRenderer, const D3D12_CONSTANT_BUFFER_VIEW_DESC* pCbvDesc, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	*pHandle = add_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], 1);
	pRenderer->pDxDevice->CreateConstantBufferView(pCbvDesc, *pHandle);
}

static void remove_cbv(Renderer* pRenderer, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	remove_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], pHandle, 1);
}

static void add_rtv(
	Renderer* pRenderer, ID3D12Resource* pResource, DXGI_FORMAT format, uint32_t mipSlice, uint32_t arraySlice,
	D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	*pHandle = add_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV], 1);
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	D3D12_RESOURCE_DESC           desc = pResource->GetDesc();
	D3D12_RESOURCE_DIMENSION      type = desc.Dimension;

	rtvDesc.Format = format;

	switch (type)
	{
		case D3D12_RESOURCE_DIMENSION_BUFFER: break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			if (desc.DepthOrArraySize > 1)
			{
				rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
				rtvDesc.Texture1DArray.MipSlice = mipSlice;
				if (arraySlice != -1)
				{
					rtvDesc.Texture1DArray.ArraySize = 1;
					rtvDesc.Texture1DArray.FirstArraySlice = arraySlice;
				}
				else
				{
					rtvDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
				}
			}
			else
			{
				rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
				rtvDesc.Texture1D.MipSlice = mipSlice;
			}
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			if (desc.SampleDesc.Count > 1)
			{
				if (desc.DepthOrArraySize > 1)
				{
					rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
					if (arraySlice != -1)
					{
						rtvDesc.Texture2DMSArray.ArraySize = 1;
						rtvDesc.Texture2DMSArray.FirstArraySlice = arraySlice;
					}
					else
					{
						rtvDesc.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
					}
				}
				else
				{
					rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
				}
			}
			else
			{
				if (desc.DepthOrArraySize > 1)
				{
					rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
					rtvDesc.Texture2DArray.MipSlice = mipSlice;
					if (arraySlice != -1)
					{
						rtvDesc.Texture2DArray.ArraySize = 1;
						rtvDesc.Texture2DArray.FirstArraySlice = arraySlice;
					}
					else
					{
						rtvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
					}
				}
				else
				{
					rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
					rtvDesc.Texture2D.MipSlice = mipSlice;
				}
			}
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
			rtvDesc.Texture3D.MipSlice = mipSlice;
			if (arraySlice != -1)
			{
				rtvDesc.Texture3D.WSize = 1;
				rtvDesc.Texture3D.FirstWSlice = arraySlice;
			}
			else
			{
				rtvDesc.Texture3D.WSize = desc.DepthOrArraySize;
			}
			break;
		default: break;
	}

	pRenderer->pDxDevice->CreateRenderTargetView(pResource, &rtvDesc, *pHandle);
}

static void add_dsv(
	Renderer* pRenderer, ID3D12Resource* pResource, DXGI_FORMAT format, uint32_t mipSlice, uint32_t arraySlice,
	D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	*pHandle = add_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV], 1);
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	D3D12_RESOURCE_DESC           desc = pResource->GetDesc();
	D3D12_RESOURCE_DIMENSION      type = desc.Dimension;

	dsvDesc.Format = format;

	switch (type)
	{
		case D3D12_RESOURCE_DIMENSION_BUFFER: break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			if (desc.DepthOrArraySize > 1)
			{
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
				dsvDesc.Texture1DArray.MipSlice = mipSlice;
				if (arraySlice != -1)
				{
					dsvDesc.Texture1DArray.ArraySize = 1;
					dsvDesc.Texture1DArray.FirstArraySlice = arraySlice;
				}
				else
				{
					dsvDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
				}
			}
			else
			{
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
				dsvDesc.Texture1D.MipSlice = mipSlice;
			}
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			if (desc.SampleDesc.Count > 1)
			{
				if (desc.DepthOrArraySize > 1)
				{
					dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
					if (arraySlice != -1)
					{
						dsvDesc.Texture2DMSArray.ArraySize = 1;
						dsvDesc.Texture2DMSArray.FirstArraySlice = arraySlice;
					}
					else
					{
						dsvDesc.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
					}
				}
				else
				{
					dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
				}
			}
			else
			{
				if (desc.DepthOrArraySize > 1)
				{
					dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
					dsvDesc.Texture2DArray.MipSlice = mipSlice;
					if (arraySlice != -1)
					{
						dsvDesc.Texture2DArray.ArraySize = 1;
						dsvDesc.Texture2DArray.FirstArraySlice = arraySlice;
					}
					else
					{
						dsvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
					}
				}
				else
				{
					dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
					dsvDesc.Texture2D.MipSlice = mipSlice;
				}
			}
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D: ASSERT(false && "Cannot create 3D Depth Stencil"); break;
		default: break;
	}

	pRenderer->pDxDevice->CreateDepthStencilView(pResource, &dsvDesc, *pHandle);
}

static void remove_rtv(Renderer* pRenderer, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	remove_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV], pHandle, 1);
}

static void remove_dsv(Renderer* pRenderer, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	remove_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV], pHandle, 1);
}

static void add_sampler(Renderer* pRenderer, const D3D12_SAMPLER_DESC* pSamplerDesc, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	*pHandle =
		add_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER], 1);
	pRenderer->pDxDevice->CreateSampler(pSamplerDesc, *pHandle);
}

static void remove_sampler(Renderer* pRenderer, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
{
	remove_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER], pHandle, 1);
}

static void create_default_resources(Renderer* pRenderer)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8_UINT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R8_UINT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;

	D3D12_SAMPLER_DESC samplerDesc = {};
	samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;

	// Create NULL descriptors in case user does not specify some descriptors we can bind null descriptor handles at those points
	add_srv(pRenderer, NULL, &srvDesc, &pRenderer->mSrvNullDescriptor);
	add_uav(pRenderer, NULL, NULL, &uavDesc, &pRenderer->mUavNullDescriptor);
	add_cbv(pRenderer, NULL, &pRenderer->mCbvNullDescriptor);
	add_sampler(pRenderer, &samplerDesc, &pRenderer->mSamplerNullDescriptor);

	BlendStateDesc blendStateDesc = {};
	blendStateDesc.mDstAlphaFactors[0] = BC_ZERO;
	blendStateDesc.mDstFactors[0] = BC_ZERO;
	blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
	blendStateDesc.mSrcFactors[0] = BC_ONE;
	blendStateDesc.mMasks[0] = ALL;
	blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
	blendStateDesc.mIndependentBlend = false;
	addBlendState(pRenderer, &blendStateDesc, &pRenderer->pDefaultBlendState);

	DepthStateDesc depthStateDesc = {};
	depthStateDesc.mDepthFunc = CMP_LEQUAL;
	depthStateDesc.mDepthTest = false;
	depthStateDesc.mDepthWrite = false;
	depthStateDesc.mStencilBackFunc = CMP_ALWAYS;
	depthStateDesc.mStencilFrontFunc = CMP_ALWAYS;
	depthStateDesc.mStencilReadMask = 0xFF;
	depthStateDesc.mStencilWriteMask = 0xFF;
	addDepthState(pRenderer, &depthStateDesc, &pRenderer->pDefaultDepthState);

	RasterizerStateDesc rasterizerStateDesc = {};
	rasterizerStateDesc.mCullMode = CULL_MODE_BACK;
	addRasterizerState(pRenderer, &rasterizerStateDesc, &pRenderer->pDefaultRasterizerState);
}

static void destroy_default_resources(Renderer* pRenderer)
{
	remove_srv(pRenderer, &pRenderer->mSrvNullDescriptor);
	remove_uav(pRenderer, &pRenderer->mUavNullDescriptor);
	remove_cbv(pRenderer, &pRenderer->mCbvNullDescriptor);
	remove_sampler(pRenderer, &pRenderer->mSamplerNullDescriptor);

	removeBlendState(pRenderer->pDefaultBlendState);
	removeDepthState(pRenderer->pDefaultDepthState);
	removeRasterizerState(pRenderer->pDefaultRasterizerState);
}

typedef enum GpuVendor
{
	GPU_VENDOR_NVIDIA,
	GPU_VENDOR_AMD,
	GPU_VENDOR_INTEL,
	GPU_VENDOR_UNKNOWN,
	GPU_VENDOR_COUNT,
} GpuVendor;

static uint32_t gRootSignatureDWORDS[GpuVendor::GPU_VENDOR_COUNT] = {
	64U,
	13U,
	64U,
	64U,
};

#define VENDOR_ID_NVIDIA 0x10DE
#define VENDOR_ID_AMD 0x1002
#define VENDOR_ID_AMD_1 0x1022
#define VENDOR_ID_INTEL 0x163C
#define VENDOR_ID_INTEL_1 0x8086
#define VENDOR_ID_INTEL_2 0x8087

static GpuVendor util_to_internal_gpu_vendor(UINT vendorId)
{
	if (vendorId == VENDOR_ID_NVIDIA)
		return GPU_VENDOR_NVIDIA;
	else if (vendorId == VENDOR_ID_AMD || vendorId == VENDOR_ID_AMD_1)
		return GPU_VENDOR_AMD;
	else if (vendorId == VENDOR_ID_INTEL || vendorId == VENDOR_ID_INTEL_1 || vendorId == VENDOR_ID_INTEL_2)
		return GPU_VENDOR_INTEL;
	else
		return GPU_VENDOR_UNKNOWN;
}
/************************************************************************/
// Internal Root Signature Functions
/************************************************************************/
typedef struct UpdateFrequencyLayoutInfo
{
	tinystl::vector<DescriptorInfo*>                  mCbvSrvUavTable;
	tinystl::vector<DescriptorInfo*>                  mSamplerTable;
	tinystl::vector<DescriptorInfo*>                  mConstantParams;
	tinystl::vector<DescriptorInfo*>                  mRootConstants;
	tinystl::unordered_map<DescriptorInfo*, uint32_t> mDescriptorIndexMap;
} UpdateFrequencyLayoutInfo;

/// Calculates the total size of the root signature (in DWORDS) from the input layouts
uint32_t calculate_root_signature_size(UpdateFrequencyLayoutInfo* pLayouts, uint32_t numLayouts)
{
	uint32_t size = 0;
	for (uint32_t i = 0; i < numLayouts; ++i)
	{
		if ((uint32_t)pLayouts[i].mCbvSrvUavTable.size())
			size += gDescriptorTableDWORDS;
		if ((uint32_t)pLayouts[i].mSamplerTable.size())
			size += gDescriptorTableDWORDS;

		for (uint32_t c = 0; c < (uint32_t)pLayouts[i].mConstantParams.size(); ++c)
		{
			size += gRootDescriptorDWORDS;
		}
		for (uint32_t c = 0; c < (uint32_t)pLayouts[i].mRootConstants.size(); ++c)
		{
			DescriptorInfo* pDesc = pLayouts[i].mRootConstants[c];
			size += pDesc->mDesc.size;
		}
	}

	return size;
}

/// Creates a root descriptor table parameter from the input table layout for root signature version 1_1
void create_descriptor_table(
	uint32_t numDescriptors, DescriptorInfo** tableRef, D3D12_DESCRIPTOR_RANGE1* pRange, D3D12_ROOT_PARAMETER1* pRootParam)
{
	pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	ShaderStage stageCount = SHADER_STAGE_NONE;
	for (uint32_t i = 0; i < numDescriptors; ++i)
	{
		const DescriptorInfo* pDesc = tableRef[i];
		pRange[i].BaseShaderRegister = pDesc->mDesc.reg;
		pRange[i].RegisterSpace = pDesc->mDesc.set;
		pRange[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
		pRange[i].NumDescriptors = pDesc->mDesc.size;
		pRange[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		pRange[i].RangeType = util_to_dx_descriptor_range(pDesc->mDesc.type);
		stageCount |= pDesc->mDesc.used_stages;
	}
	pRootParam->ShaderVisibility = util_to_dx_shader_visibility(stageCount);
	pRootParam->DescriptorTable.NumDescriptorRanges = numDescriptors;
	pRootParam->DescriptorTable.pDescriptorRanges = pRange;
}

/// Creates a root descriptor table parameter from the input table layout for root signature version 1_0
void create_descriptor_table_1_0(
	uint32_t numDescriptors, DescriptorInfo** tableRef, D3D12_DESCRIPTOR_RANGE* pRange, D3D12_ROOT_PARAMETER* pRootParam)
{
	pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	ShaderStage stageCount = SHADER_STAGE_NONE;

	for (uint32_t i = 0; i < numDescriptors; ++i)
	{
		const DescriptorInfo* pDesc = tableRef[i];
		pRange[i].BaseShaderRegister = pDesc->mDesc.reg;
		pRange[i].RegisterSpace = pDesc->mDesc.set;
		pRange[i].NumDescriptors = pDesc->mDesc.size;
		pRange[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		pRange[i].RangeType = util_to_dx_descriptor_range(pDesc->mDesc.type);
		stageCount |= pDesc->mDesc.used_stages;
	}

	pRootParam->ShaderVisibility = util_to_dx_shader_visibility(stageCount);
	pRootParam->DescriptorTable.NumDescriptorRanges = numDescriptors;
	pRootParam->DescriptorTable.pDescriptorRanges = pRange;
}

/// Creates a root descriptor / root constant parameter for root signature version 1_1
void create_root_descriptor(const DescriptorInfo* pDesc, D3D12_ROOT_PARAMETER1* pRootParam)
{
	pRootParam->ShaderVisibility = util_to_dx_shader_visibility(pDesc->mDesc.used_stages);
	pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pRootParam->Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
	pRootParam->Descriptor.ShaderRegister = pDesc->mDesc.reg;
	pRootParam->Descriptor.RegisterSpace = pDesc->mDesc.set;
}

/// Creates a root descriptor / root constant parameter for root signature version 1_0
void create_root_descriptor_1_0(const DescriptorInfo* pDesc, D3D12_ROOT_PARAMETER* pRootParam)
{
	pRootParam->ShaderVisibility = util_to_dx_shader_visibility(pDesc->mDesc.used_stages);
	pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pRootParam->Descriptor.ShaderRegister = pDesc->mDesc.reg;
	pRootParam->Descriptor.RegisterSpace = pDesc->mDesc.set;
}

void create_root_constant(const DescriptorInfo* pDesc, D3D12_ROOT_PARAMETER1* pRootParam)
{
	pRootParam->ShaderVisibility = util_to_dx_shader_visibility(pDesc->mDesc.used_stages);
	pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pRootParam->Constants.Num32BitValues = pDesc->mDesc.size;
	pRootParam->Constants.ShaderRegister = pDesc->mDesc.reg;
	pRootParam->Constants.RegisterSpace = pDesc->mDesc.set;
}

void create_root_constant_1_0(const DescriptorInfo* pDesc, D3D12_ROOT_PARAMETER* pRootParam)
{
	pRootParam->ShaderVisibility = util_to_dx_shader_visibility(pDesc->mDesc.used_stages);
	pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pRootParam->Constants.Num32BitValues = pDesc->mDesc.size;
	pRootParam->Constants.ShaderRegister = pDesc->mDesc.reg;
	pRootParam->Constants.RegisterSpace = pDesc->mDesc.set;
}
/************************************************************************/
// Internal utility functions
/************************************************************************/
D3D12_FILTER util_to_dx_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool aniso, bool comparisonFilterEnabled)
{
	if (aniso)
		return (comparisonFilterEnabled ? D3D12_FILTER_COMPARISON_ANISOTROPIC : D3D12_FILTER_ANISOTROPIC);

	// control bit : minFilter  magFilter   mipMapMode
	//   point   :   00	  00	   00
	//   linear  :   01	  01	   01
	// ex : trilinear == 010101
	int filter = (minFilter << 4) | (magFilter << 2) | mipMapMode;
	int baseFilter = comparisonFilterEnabled ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_POINT;
	return (D3D12_FILTER)(baseFilter + filter);
}

D3D12_TEXTURE_ADDRESS_MODE util_to_dx_texture_address_mode(AddressMode addressMode)
{
	switch (addressMode)
	{
		case ADDRESS_MODE_MIRROR: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		case ADDRESS_MODE_REPEAT: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		case ADDRESS_MODE_CLAMP_TO_EDGE: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		case ADDRESS_MODE_CLAMP_TO_BORDER: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE util_to_dx_primitive_topology_type(PrimitiveTopology topology)
{
	switch (topology)
	{
		case PRIMITIVE_TOPO_POINT_LIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		case PRIMITIVE_TOPO_LINE_LIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case PRIMITIVE_TOPO_LINE_STRIP: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case PRIMITIVE_TOPO_TRI_LIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		case PRIMITIVE_TOPO_TRI_STRIP: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		case PRIMITIVE_TOPO_PATCH_LIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	}
	return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
}

uint64_t util_dx_determine_storage_counter_offset(uint64_t buffer_size)
{
	uint64_t alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
	uint64_t result = (buffer_size + (alignment - 1)) & ~(alignment - 1);
	return result;
}

DXGI_FORMAT util_to_dx_uav_format(DXGI_FORMAT defaultFormat)
{
	switch (defaultFormat)
	{
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;

		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;

		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8X8_UNORM;

		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;

#ifdef _DEBUG
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		case DXGI_FORMAT_D16_UNORM: ErrorMsg("Requested a UAV format for a depth stencil format");
#endif

		default: return defaultFormat;
	}
}

DXGI_FORMAT util_to_dx_dsv_format(DXGI_FORMAT defaultFormat)
{
	switch (defaultFormat)
	{
			// 32-bit Z w/ Stencil
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

			// No Stencil
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
			return DXGI_FORMAT_D32_FLOAT;

			// 24-bit Z
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return DXGI_FORMAT_D24_UNORM_S8_UINT;

			// 16-bit Z w/o Stencil
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM: return DXGI_FORMAT_D16_UNORM;

		default: return defaultFormat;
	}
}

DXGI_FORMAT util_to_dx_srv_format(DXGI_FORMAT defaultFormat)
{
	switch (defaultFormat)
	{
			// 32-bit Z w/ Stencil
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

			// No Stencil
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
			return DXGI_FORMAT_R32_FLOAT;

			// 24-bit Z
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

			// 16-bit Z w/o Stencil
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM: return DXGI_FORMAT_R16_UNORM;

		case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;

		default: return defaultFormat;
	}
}

DXGI_FORMAT util_to_dx_stencil_format(DXGI_FORMAT defaultFormat)
{
	switch (defaultFormat)
	{
			// 32-bit Z w/ Stencil
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

			// 24-bit Z
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT: return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

		default: return DXGI_FORMAT_UNKNOWN;
	}
}

DXGI_FORMAT util_to_dx_swapchain_format(ImageFormat::Enum format)
{
	DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;

	// FLIP_DISCARD and FLIP_SEQEUNTIAL swapchain buffers only support these formats
	switch (format)
	{
		case ImageFormat::RGBA16F: result = DXGI_FORMAT_R16G16B16A16_FLOAT;
		case ImageFormat::BGRA8: result = DXGI_FORMAT_B8G8R8A8_UNORM; break;
		case ImageFormat::RGBA8: result = DXGI_FORMAT_R8G8B8A8_UNORM; break;
		case ImageFormat::RGB10A2: result = DXGI_FORMAT_R10G10B10A2_UNORM; break;
		default: break;
	}

	if (result == DXGI_FORMAT_UNKNOWN)
	{
		LOGERRORF("Image Format (%u) not supported for creating swapchain buffer", (uint32_t)format);
	}

	return result;
}

DXGI_FORMAT util_to_dx_image_format_typeless(ImageFormat::Enum format)
{
	DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;
	if (format >= sizeof(gDX12FormatTranslatorTypeless) / sizeof(DXGI_FORMAT))
	{
		LOGERRORF("Failed to Map from ConfettilFileFromat to DXGI format, should add map method in gDX12FormatTranslator");
	}
	else
	{
		result = gDX12FormatTranslatorTypeless[format];
	}

	return result;
}

DXGI_FORMAT util_to_dx_image_format(ImageFormat::Enum format, bool srgb)
{
	DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;
	if (format >= sizeof(gDX12FormatTranslator) / sizeof(DXGI_FORMAT))
	{
		LOGERRORF("Failed to Map from ConfettilFileFromat to DXGI format, should add map method in gDX12FormatTranslator");
	}
	else
	{
		result = gDX12FormatTranslator[format];
		if (srgb)
		{
			if (result == DXGI_FORMAT_R8G8B8A8_UNORM)
				result = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			else if (result == DXGI_FORMAT_B8G8R8A8_UNORM)
				result = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
			else if (result == DXGI_FORMAT_B8G8R8X8_UNORM)
				result = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
			else if (result == DXGI_FORMAT_BC1_UNORM)
				result = DXGI_FORMAT_BC1_UNORM_SRGB;
			else if (result == DXGI_FORMAT_BC2_UNORM)
				result = DXGI_FORMAT_BC2_UNORM_SRGB;
			else if (result == DXGI_FORMAT_BC3_UNORM)
				result = DXGI_FORMAT_BC3_UNORM_SRGB;
			else if (result == DXGI_FORMAT_BC7_UNORM)
				result = DXGI_FORMAT_BC7_UNORM_SRGB;
		}
	}

	return result;
}

D3D12_SHADER_VISIBILITY util_to_dx_shader_visibility(ShaderStage stages)
{
	D3D12_SHADER_VISIBILITY res = D3D12_SHADER_VISIBILITY_ALL;
	uint32_t                stageCount = 0;

	if (stages & SHADER_STAGE_COMP)
	{
		return D3D12_SHADER_VISIBILITY_ALL;
	}
	if (stages & SHADER_STAGE_VERT)
	{
		res = D3D12_SHADER_VISIBILITY_VERTEX;
		++stageCount;
	}
	if (stages & SHADER_STAGE_GEOM)
	{
		res = D3D12_SHADER_VISIBILITY_GEOMETRY;
		++stageCount;
	}
	if (stages & SHADER_STAGE_HULL)
	{
		res = D3D12_SHADER_VISIBILITY_HULL;
		++stageCount;
	}
	if (stages & SHADER_STAGE_DOMN)
	{
		res = D3D12_SHADER_VISIBILITY_DOMAIN;
		++stageCount;
	}
	if (stages & SHADER_STAGE_FRAG)
	{
		res = D3D12_SHADER_VISIBILITY_PIXEL;
		++stageCount;
	}
	ASSERT(stageCount > 0);
	return stageCount > 1 ? D3D12_SHADER_VISIBILITY_ALL : res;
}

D3D12_DESCRIPTOR_RANGE_TYPE util_to_dx_descriptor_range(DescriptorType type)
{
	switch (type)
	{
		case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case DESCRIPTOR_TYPE_ROOT_CONSTANT: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		case DESCRIPTOR_TYPE_TEXTURE:
		case DESCRIPTOR_TYPE_BUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case DESCRIPTOR_TYPE_RW_BUFFER:
		case DESCRIPTOR_TYPE_RW_TEXTURE: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case DESCRIPTOR_TYPE_SAMPLER: return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		default: ASSERT("Invalid DescriptorInfo Type"); return (D3D12_DESCRIPTOR_RANGE_TYPE)-1;
	}
}

D3D12_RESOURCE_STATES util_to_dx_resource_state(ResourceState state)
{
	D3D12_RESOURCE_STATES ret = (D3D12_RESOURCE_STATES)state;

	// These states cannot be combined with other states so we just do an == check
	if (state == RESOURCE_STATE_GENERIC_READ)
		return D3D12_RESOURCE_STATE_GENERIC_READ;
	if (state == RESOURCE_STATE_COMMON)
		return D3D12_RESOURCE_STATE_COMMON;
	if (state == RESOURCE_STATE_PRESENT)
		return D3D12_RESOURCE_STATE_PRESENT;

	if (state & RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	if (state & RESOURCE_STATE_INDEX_BUFFER)
		ret |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
	if (state & RESOURCE_STATE_RENDER_TARGET)
		ret |= D3D12_RESOURCE_STATE_RENDER_TARGET;
	if (state & RESOURCE_STATE_UNORDERED_ACCESS)
		ret |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	if (state & RESOURCE_STATE_DEPTH_WRITE)
		ret |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
	if (state & RESOURCE_STATE_DEPTH_READ)
		ret |= D3D12_RESOURCE_STATE_DEPTH_READ;
	if (state & RESOURCE_STATE_STREAM_OUT)
		ret |= D3D12_RESOURCE_STATE_STREAM_OUT;
	if (state & RESOURCE_STATE_INDIRECT_ARGUMENT)
		ret |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	if (state & RESOURCE_STATE_COPY_DEST)
		ret |= D3D12_RESOURCE_STATE_COPY_DEST;
	if (state & RESOURCE_STATE_COPY_SOURCE)
		ret |= D3D12_RESOURCE_STATE_COPY_SOURCE;

	if (state == RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		ret |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	else if (state & RESOURCE_STATE_SHADER_RESOURCE)
		ret |= (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	return ret;
}

D3D12_QUERY_HEAP_TYPE util_to_dx_query_heap_type(QueryType type)
{
	switch (type)
	{
		case QUERY_TYPE_TIMESTAMP: return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		case QUERY_TYPE_PIPELINE_STATISTICS: return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
		case QUERY_TYPE_OCCLUSION: return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
		default: ASSERT(false && "Invalid query heap type"); return D3D12_QUERY_HEAP_TYPE(-1);
	}
}

D3D12_QUERY_TYPE util_to_dx_query_type(QueryType type)
{
	switch (type)
	{
		case QUERY_TYPE_TIMESTAMP: return D3D12_QUERY_TYPE_TIMESTAMP;
		case QUERY_TYPE_PIPELINE_STATISTICS: return D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
		case QUERY_TYPE_OCCLUSION: return D3D12_QUERY_TYPE_OCCLUSION;
		default: ASSERT(false && "Invalid query heap type"); return D3D12_QUERY_TYPE(-1);
	}
}
/************************************************************************/
// Multi GPU Helper Functions
/************************************************************************/
uint32_t util_calculate_shared_node_mask(Renderer* pRenderer)
{
	if (pRenderer->mSettings.mGpuMode == GPU_MODE_LINKED)
		return (1 << pRenderer->mDxLinkedNodeCount) - 1;
	else
		return 0;
}

uint32_t util_calculate_node_mask(Renderer* pRenderer, uint32_t i)
{
	if (pRenderer->mSettings.mGpuMode == GPU_MODE_LINKED)
		return (1 << i);
	else
		return 0;
}
/************************************************************************/
// Internal init functions
/************************************************************************/
#ifndef _DURANGO
dxc::DxcDllSupport gDxcDllHelper;

// Note that Windows 10 Creator Update SDK is required for enabling Shader Model 6 feature.
static HRESULT EnableExperimentalShaderModels()
{
	static const GUID D3D12ExperimentalShaderModelsID = { /* 76f5573e-f13a-40f5-b297-81ce9e18933f */
														  0x76f5573e,
														  0xf13a,
														  0x40f5,
														  { 0xb2, 0x97, 0x81, 0xce, 0x9e, 0x18, 0x93, 0x3f }
	};

	return D3D12EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModelsID, nullptr, nullptr);
}
#endif

#ifdef _DURANGO
static UINT HANGBEGINCALLBACK(UINT64 Flags)
{
	LOGINFOF("( %d )", Flags);
	return (UINT)Flags;
}

static void HANGPRINTCALLBACK(const CHAR* strLine)
{
	LOGINFOF("( %s )", strLine);
	return;
}

static void HANGDUMPCALLBACK(const WCHAR* strFileName) { return; }
#endif

static void AddDevice(Renderer* pRenderer)
{
#if defined(_DEBUG) || defined(PROFILE)
	//add debug layer if in debug mode
	if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(pRenderer->pDXDebug), (void**)&(pRenderer->pDXDebug))))
	{
		if (fnHookEnableDebugLayer != NULL)
			fnHookEnableDebugLayer(pRenderer);
	}
#endif
#ifndef _DURANGO
	if (pRenderer->mSettings.mShaderTarget >= shader_target_6_0)
	{
		ASSERT(SUCCEEDED(EnableExperimentalShaderModels()));
	}
#endif

	D3D_FEATURE_LEVEL feature_levels[4] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

#ifdef _DURANGO
	// Create the DX12 API device object.
	HRESULT hres = D3D12CreateDevice(NULL, feature_levels[0], IID_ARGS(&pRenderer->pDxDevice));
	ASSERT(SUCCEEDED(hres));

#if defined(_DEBUG) || defined(PROFILE)
	//Sets the callback functions to invoke when the GPU hangs
	//pRenderer->pDxDevice->SetHangCallbacksX(HANGBEGINCALLBACK, HANGPRINTCALLBACK, NULL);
#endif

	// First, retrieve the underlying DXGI device from the D3D device.
	IDXGIDevice1* dxgiDevice;
	hres = pRenderer->pDxDevice->QueryInterface(IID_ARGS(&dxgiDevice));
	ASSERT(SUCCEEDED(hres));

	// Identify the physical adapter (GPU or card) this device is running on.
	IDXGIAdapter* dxgiAdapter;
	hres = dxgiDevice->GetAdapter(&dxgiAdapter);
	ASSERT(SUCCEEDED(hres));

	// And obtain the factory object that created it.
	hres = dxgiAdapter->GetParent(IID_ARGS(&pRenderer->pDXGIFactory));
	ASSERT(SUCCEEDED(hres));

	typedef struct GpuDesc
	{
		IDXGIAdapter*     pGpu = NULL;
		D3D_FEATURE_LEVEL mMaxSupportedFeatureLevel = (D3D_FEATURE_LEVEL)0;
	} GpuDesc;

	GpuDesc gpuDesc[MAX_GPUS] = {};
	dxgiAdapter->QueryInterface(IID_ARGS(&gpuDesc->pGpu));
	gpuDesc->mMaxSupportedFeatureLevel = feature_levels[0];
	pRenderer->mNumOfGPUs = 1;

	dxgiAdapter->Release();
#else
	UINT flags = 0;
#if defined(_DEBUG)
	flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
	HRESULT hres = CreateDXGIFactory2(flags, __uuidof(pRenderer->pDXGIFactory), (void**)&(pRenderer->pDXGIFactory));
	ASSERT(SUCCEEDED(hres));

	typedef struct GpuDesc
	{
		Renderer*                         pRenderer = NULL;
		IDXGIAdapter3*                    pGpu = NULL;
		D3D_FEATURE_LEVEL                 mMaxSupportedFeatureLevel = (D3D_FEATURE_LEVEL)0;
		D3D12_FEATURE_DATA_D3D12_OPTIONS  mFeatureDataOptions;
		D3D12_FEATURE_DATA_D3D12_OPTIONS1 mFeatureDataOptions1;
		SIZE_T                            mDedicatedVideoMemory = 0;
		char                              mVendorId[MAX_GPU_VENDOR_STRING_LENGTH];
		char                              mDeviceId[MAX_GPU_VENDOR_STRING_LENGTH];
		char                              mRevisionId[MAX_GPU_VENDOR_STRING_LENGTH];
		char                              mName[MAX_GPU_VENDOR_STRING_LENGTH];
		GPUPresetLevel                    mPreset;
	} GpuDesc;

	GpuDesc gpuDesc[MAX_GPUS] = {};

	IDXGIAdapter3* adapter = NULL;
	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != pRenderer->pDXGIFactory->EnumAdapters1(i, (IDXGIAdapter1**)&adapter); ++i)
	{
		DECLARE_ZERO(DXGI_ADAPTER_DESC1, desc);
		adapter->GetDesc1(&desc);

		if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
		{
			for (uint32_t level = 0; level < sizeof(feature_levels) / sizeof(feature_levels[0]); ++level)
			{
				// Make sure the adapter can support a D3D12 device
				if (SUCCEEDED(D3D12CreateDevice(adapter, feature_levels[level], __uuidof(ID3D12Device), NULL)))
				{
					hres = adapter->QueryInterface(IID_ARGS(&gpuDesc[pRenderer->mNumOfGPUs].pGpu));
					if (SUCCEEDED(hres))
					{
						D3D12CreateDevice(adapter, feature_levels[level], IID_PPV_ARGS(&pRenderer->pDxDevice));

						// Query the level of support of Shader Model.
						D3D12_FEATURE_DATA_D3D12_OPTIONS  featureData = {};
						D3D12_FEATURE_DATA_D3D12_OPTIONS1 featureData1 = {};
						// Query the level of support of Wave Intrinsics.
						pRenderer->pDxDevice->CheckFeatureSupport(
							(D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS, &featureData, sizeof(featureData));
						pRenderer->pDxDevice->CheckFeatureSupport(
							(D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &featureData1, sizeof(featureData1));

						gpuDesc[pRenderer->mNumOfGPUs].mMaxSupportedFeatureLevel = feature_levels[level];
						gpuDesc[pRenderer->mNumOfGPUs].mDedicatedVideoMemory = desc.DedicatedVideoMemory;
						gpuDesc[pRenderer->mNumOfGPUs].mFeatureDataOptions = featureData;
						gpuDesc[pRenderer->mNumOfGPUs].mFeatureDataOptions1 = featureData1;
						gpuDesc[pRenderer->mNumOfGPUs].pRenderer = pRenderer;

						//save vendor and model Id as string
						//char hexChar[10];
						//convert deviceId and assign it
						sprintf(gpuDesc[pRenderer->mNumOfGPUs].mDeviceId, "%#x\0", desc.DeviceId);
						//convert modelId and assign it
						sprintf(gpuDesc[pRenderer->mNumOfGPUs].mVendorId, "%#x\0", desc.VendorId);
						//convert Revision Id
						sprintf(gpuDesc[pRenderer->mNumOfGPUs].mRevisionId, "%#x\0", desc.Revision);

						//get preset for current gpu description
						gpuDesc[pRenderer->mNumOfGPUs].mPreset = getGPUPresetLevel(
							gpuDesc[pRenderer->mNumOfGPUs].mVendorId, gpuDesc[pRenderer->mNumOfGPUs].mDeviceId,
							gpuDesc[pRenderer->mNumOfGPUs].mRevisionId);

						//save gpu name (Some situtations this can show description instead of name)
						//char sName[MAX_PATH];
						wcstombs(gpuDesc[pRenderer->mNumOfGPUs].mName, desc.Description, MAX_PATH);
						++pRenderer->mNumOfGPUs;
						SAFE_RELEASE(pRenderer->pDxDevice);
						break;
					}
				}
			}
		}

		adapter->Release();
	}

	ASSERT(pRenderer->mNumOfGPUs > 0);

	// Sort GPUs by poth Preset and highest feature level gpu at front
	//Prioritize Preset first
	qsort(gpuDesc, pRenderer->mNumOfGPUs, sizeof(GpuDesc), [](const void* lhs, const void* rhs) {
		GpuDesc* gpu1 = (GpuDesc*)lhs;
		GpuDesc* gpu2 = (GpuDesc*)rhs;

		// If shader model 6.0 or higher is requested, prefer the GPU which supports it
		if (gpu1->pRenderer->mSettings.mShaderTarget >= shader_target_6_0)
		{
			if (gpu1->mFeatureDataOptions1.WaveOps != gpu2->mFeatureDataOptions1.WaveOps)
				return gpu1->mFeatureDataOptions1.WaveOps ? -1 : 1;
		}

		// Check feature level first, sort the greatest feature level gpu to the front
		if ((int)gpu1->mPreset != (int)gpu2->mPreset)
		{
			return gpu1->mPreset > gpu2->mPreset ? -1 : 1;
		}
		else if ((int)gpu1->mMaxSupportedFeatureLevel != (int)gpu2->mMaxSupportedFeatureLevel)
		{
			return (int)gpu2->mMaxSupportedFeatureLevel - (int)gpu1->mMaxSupportedFeatureLevel;
		}
		// If feature levels are the same, push the gpu with most video memory support to the front
		else
		{
			return gpu1->mDedicatedVideoMemory > gpu2->mDedicatedVideoMemory ? -1 : 1;
		}
	});
#endif

	for (uint32_t i = 0; i < pRenderer->mNumOfGPUs; ++i)
	{
		pRenderer->pDxGPUs[i] = gpuDesc[i].pGpu;
		pRenderer->mGpuSettings[i].mUniformBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
		pRenderer->mGpuSettings[i].mMultiDrawIndirect = true;
		pRenderer->mGpuSettings[i].mMaxVertexInputBindings = 32U;
#ifndef _DURANGO
		//assign device ID
		strncpy(pRenderer->mGpuSettings[i].mGpuVendorPreset.mModelId, gpuDesc[i].mDeviceId, MAX_GPU_VENDOR_STRING_LENGTH);
		//assign vendor ID
		strncpy(pRenderer->mGpuSettings[i].mGpuVendorPreset.mVendorId, gpuDesc[i].mVendorId, MAX_GPU_VENDOR_STRING_LENGTH);
		//assign Revision ID
		strncpy(pRenderer->mGpuSettings[i].mGpuVendorPreset.mRevisionId, gpuDesc[i].mRevisionId, MAX_GPU_VENDOR_STRING_LENGTH);
		//get name from api
		strncpy(pRenderer->mGpuSettings[i].mGpuVendorPreset.mGpuName, gpuDesc[i].mName, MAX_GPU_VENDOR_STRING_LENGTH);
		//get preset
		pRenderer->mGpuSettings[i].mGpuVendorPreset.mPresetLevel = gpuDesc[i].mPreset;
		//get wave lane count
		pRenderer->mGpuSettings[i].mWaveLaneCount = gpuDesc[i].mFeatureDataOptions1.WaveLaneCountMin;
		pRenderer->mGpuSettings[i].mROVsSupported = gpuDesc[i].mFeatureDataOptions.ROVsSupported ? true : false;
#else
		//Default XBox values
		strncpy(pRenderer->mGpuSettings[i].mGpuVendorPreset.mModelId, "XboxOne", MAX_GPU_VENDOR_STRING_LENGTH);
		strncpy(pRenderer->mGpuSettings[i].mGpuVendorPreset.mVendorId, "XboxOne", MAX_GPU_VENDOR_STRING_LENGTH);
		strncpy(pRenderer->mGpuSettings[i].mGpuVendorPreset.mRevisionId, "0x00", MAX_GPU_VENDOR_STRING_LENGTH);
		strncpy(pRenderer->mGpuSettings[i].mGpuVendorPreset.mGpuName, "XboxOne", MAX_GPU_VENDOR_STRING_LENGTH);

		pRenderer->mGpuSettings[i].mGpuVendorPreset.mPresetLevel = GPUPresetLevel::GPU_PRESET_HIGH;
#endif
		// Determine root signature size for this gpu driver
		DXGI_ADAPTER_DESC adapterDesc;
		pRenderer->pDxGPUs[i]->GetDesc(&adapterDesc);
		pRenderer->mGpuSettings[i].mMaxRootSignatureDWORDS = gRootSignatureDWORDS[util_to_internal_gpu_vendor(adapterDesc.VendorId)];
		LOGINFOF(
			"GPU[%i] detected. Vendor ID: %x, Revision ID: %x, GPU Name: %S", i, adapterDesc.VendorId, adapterDesc.Revision,
			adapterDesc.Description);
	}

	uint32_t gpuIndex = 0;
#if defined(ACTIVE_TESTING_GPU) && !defined(_DURANGO) && defined(AUTOMATED_TESTING)
	//Read active GPU if AUTOMATED_TESTING and ACTIVE_TESTING_GPU are defined
	GPUVendorPreset activeTestingPreset;
	bool            activeTestingGpu = getActiveGpuConfig(activeTestingPreset);
	if (activeTestingGpu)
	{
		for (uint32_t i = 0; i < pRenderer->mNumOfGPUs; i++)
		{
			if (pRenderer->mGpuSettings[i].mGpuVendorPreset.mVendorId == activeTestingPreset.mVendorId &&
				pRenderer->mGpuSettings[i].mGpuVendorPreset.mModelId == activeTestingPreset.mModelId)
			{
				//if revision ID is valid then use it to select active GPU
				if (pRenderer->mGpuSettings[i].mGpuVendorPreset.mRevisionId != "0x00" &&
					pRenderer->mGpuSettings[i].mGpuVendorPreset.mRevisionId != activeTestingPreset.mRevisionId)
					continue;
				gpuIndex = i;
				break;
			}
		}
	}
#endif
	// Get the latest and greatest feature level gpu
	pRenderer->pDxActiveGPU = pRenderer->pDxGPUs[gpuIndex];
	ASSERT(pRenderer->pDxActiveGPU != NULL);
	pRenderer->pActiveGpuSettings = &pRenderer->mGpuSettings[gpuIndex];

	//print selected GPU information
	LOGINFOF("GPU[%d] is selected as default GPU", gpuIndex);
	LOGINFOF("Name of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mGpuName);
	LOGINFOF("Vendor id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mVendorId);
	LOGINFOF("Model id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mModelId);
	LOGINFOF("Revision id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mRevisionId);

	// Load functions
	{
#ifdef _DURANGO
		HMODULE module = get_d3d12_module_handle();
#else
		HMODULE module = ::GetModuleHandle(TEXT("d3d12.dll"));
#endif
		fnD3D12CreateRootSignatureDeserializer =
			(PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress(module, "D3D12SerializeVersionedRootSignature");

		fnD3D12SerializeVersionedRootSignature =
			(PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress(module, "D3D12SerializeVersionedRootSignature");

		fnD3D12CreateVersionedRootSignatureDeserializer =
			(PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress(module, "D3D12CreateVersionedRootSignatureDeserializer");
	}

#ifndef _DURANGO
	hres = D3D12CreateDevice(pRenderer->pDxActiveGPU, gpuDesc[gpuIndex].mMaxSupportedFeatureLevel, IID_ARGS(&pRenderer->pDxDevice));
	ASSERT(SUCCEEDED(hres));
	// #TODO - Let user specify these through RendererSettings
	//ID3D12InfoQueue* pd3dInfoQueue = nullptr;
	//HRESULT hr = pRenderer->pDxDevice->QueryInterface(IID_ARGS(&pd3dInfoQueue));
	//if (SUCCEEDED(hr))
	//{
	//  pd3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
	//  pd3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
	//  pd3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
	//}
#endif

	//pRenderer->mSettings.mDxFeatureLevel = target_feature_level;  // this is not used anywhere?
}

static void RemoveDevice(Renderer* pRenderer)
{
	SAFE_RELEASE(pRenderer->pDXGIFactory);

	for (uint32_t i = 0; i < pRenderer->mNumOfGPUs; ++i)
	{
		SAFE_RELEASE(pRenderer->pDxGPUs[i]);
	}

#if defined(_DURANGO)
	SAFE_RELEASE(pRenderer->pDxDevice);
#elif defined(_DEBUG) || defined(PROFILE)
	ID3D12DebugDevice* pDebugDevice = NULL;
	pRenderer->pDxDevice->QueryInterface(&pDebugDevice);

	SAFE_RELEASE(pRenderer->pDXDebug);
	SAFE_RELEASE(pRenderer->pDxDevice);

	pDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
	pDebugDevice->Release();
#else
	SAFE_RELEASE(pRenderer->pDxDevice);
#endif
}
#if defined(__cplusplus) && defined(ENABLE_RENDERER_RUNTIME_SWITCH)
namespace d3d12 {
#endif

/************************************************************************/
// Functions not exposed in IRenderer but still need to be assigned when using runtime switching of renderers
/************************************************************************/
// clang-format off
API_INTERFACE void CALLTYPE addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
API_INTERFACE void CALLTYPE removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
API_INTERFACE void CALLTYPE addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture);
API_INTERFACE void CALLTYPE removeTexture(Renderer* pRenderer, Texture* pTexture);
API_INTERFACE void CALLTYPE mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
API_INTERFACE void CALLTYPE unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
API_INTERFACE void CALLTYPE cmdUpdateBuffer(Cmd* pCmd, uint64_t srcOffset, uint64_t dstOffset, uint64_t size, Buffer* pSrcBuffer, Buffer* pBuffer);
API_INTERFACE void CALLTYPE cmdUpdateSubresources(Cmd* pCmd, uint32_t startSubresource, uint32_t numSubresources, SubresourceDataDesc* pSubresources, Buffer* pIntermediate,uint64_t intermediateOffset, Texture* pTexture);
API_INTERFACE void CALLTYPE compileShader(Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code,	uint32_t macroCount, ShaderMacro* pMacros, void* (*allocator)(size_t a), uint32_t* pByteCodeSize, char** ppByteCode);
API_INTERFACE const RendererShaderDefinesDesc CALLTYPE get_renderer_shaderdefines(Renderer* pRenderer);
// clang-format on
/************************************************************************/
// Renderer Init Remove
/************************************************************************/
void initRenderer(const char* appName, const RendererDesc* settings, Renderer** ppRenderer)
{
	initHooks();

	Renderer* pRenderer = (Renderer*)conf_calloc(1, sizeof(*pRenderer));
	ASSERT(pRenderer);

	pRenderer->pName = (char*)conf_calloc(strlen(appName) + 1, sizeof(char));
	memcpy(pRenderer->pName, appName, strlen(appName));

	// Copy settings
	memcpy(&(pRenderer->mSettings), settings, sizeof(*settings));
#if defined(_DURANGO)
	pRenderer->mSettings.mApi = RENDERER_API_XBOX_D3D12;
#else
	pRenderer->mSettings.mApi = RENDERER_API_D3D12;
#endif

	// Initialize the D3D12 bits
	{
		AddDevice(pRenderer);

#ifndef _DURANGO
		//anything below LOW preset is not supported and we will exit
		if (pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel < GPU_PRESET_LOW)
		{
			//have the condition in the assert as well so its cleared when the assert message box appears

			ASSERT(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel >= GPU_PRESET_LOW);

			SAFE_FREE(pRenderer->pName);

			//remove device and any memory we allocated in just above as this is the first function called
			//when initializing the forge
			RemoveDevice(pRenderer);
			SAFE_FREE(pRenderer);
			LOGERROR("Selected GPU has an Office Preset in gpu.cfg.");
			LOGERROR("Office preset is not supported by The Forge.");

			//return NULL pRenderer so that client can gracefully handle exit
			//This is better than exiting from here in case client has allocated memory or has fallbacks
			ppRenderer = NULL;
			return;
		}
		if (pRenderer->mSettings.mShaderTarget >= shader_target_6_0)
		{
			// Query the level of support of Shader Model.
			D3D12_FEATURE_DATA_SHADER_MODEL   shaderModelSupport = { D3D_SHADER_MODEL_6_0 };
			D3D12_FEATURE_DATA_D3D12_OPTIONS1 m_WaveIntrinsicsSupport = {};
			if (!SUCCEEDED(pRenderer->pDxDevice->CheckFeatureSupport(
					(D3D12_FEATURE)D3D12_FEATURE_SHADER_MODEL, &shaderModelSupport, sizeof(shaderModelSupport))))
			{
				return;
			}
			// Query the level of support of Wave Intrinsics.
			if (!SUCCEEDED(pRenderer->pDxDevice->CheckFeatureSupport(
					(D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &m_WaveIntrinsicsSupport, sizeof(m_WaveIntrinsicsSupport))))
			{
				return;
			}

			// If the device doesn't support SM6 or Wave Intrinsics, try enabling the experimental feature for Shader Model 6 and creating the device again.
			if (shaderModelSupport.HighestShaderModel != D3D_SHADER_MODEL_6_0 || m_WaveIntrinsicsSupport.WaveOps != TRUE)
			{
				// If the device still doesn't support SM6 or Wave Intrinsics after enabling the experimental feature, you could set up your application to use the highest supported shader model.
				// For simplicity we just exit the application here.
				if (shaderModelSupport.HighestShaderModel != D3D_SHADER_MODEL_6_0 || m_WaveIntrinsicsSupport.WaveOps != TRUE)
				{
					RemoveDevice(pRenderer);
					LOGERROR("Hardware does not support Shader Model 6.0");
					return;
				}
			}
		}
#endif

		/************************************************************************/
		// Multi GPU - SLI Node Count
		/************************************************************************/
		if (pRenderer->mSettings.mGpuMode == GPU_MODE_LINKED)
		{
			pRenderer->mDxLinkedNodeCount = pRenderer->pDxDevice->GetNodeCount();
			if (pRenderer->mDxLinkedNodeCount < 2)
				pRenderer->mSettings.mGpuMode = GPU_MODE_SINGLE;
		}
		/************************************************************************/
		/************************************************************************/
		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			add_descriptor_heap(
				pRenderer->pDxDevice, (D3D12_DESCRIPTOR_HEAP_TYPE)i, gCpuDescriptorHeapProperties[i].mFlags,
				gCpuDescriptorHeapProperties[i].mMaxDescriptors,
				0,    // CPU Descriptor Heap - Node mask is irrelevant
				&pRenderer->pCPUDescriptorHeaps[i]);
		}

		uint32_t gpuCount = 1;
		if (pRenderer->mSettings.mGpuMode == GPU_MODE_LINKED)
			gpuCount = pRenderer->mDxLinkedNodeCount;

		for (uint32_t i = 0; i < gpuCount; ++i)
		{
			if (pRenderer->mSettings.mGpuMode == GPU_MODE_SINGLE && i > 0)
				break;

			uint32_t nodeMask = util_calculate_node_mask(pRenderer, i);

			add_descriptor_heap(
				pRenderer->pDxDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				gGpuDescriptorHeapProperties[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].mFlags,
				gGpuDescriptorHeapProperties[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].mMaxDescriptors, nodeMask,
				&pRenderer->pCbvSrvUavHeap[i]);

			add_descriptor_heap(
				pRenderer->pDxDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
				gGpuDescriptorHeapProperties[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].mFlags,
				gGpuDescriptorHeapProperties[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].mMaxDescriptors, nodeMask, &pRenderer->pSamplerHeap[i]);
		}

		AllocatorCreateInfo info = { 0 };
		info.pRenderer = pRenderer;
		info.device = pRenderer->pDxDevice;
		info.physicalDevice = pRenderer->pDxActiveGPU;
		createAllocator(&info, &pRenderer->pResourceAllocator);
	}

	create_default_resources(pRenderer);
	/************************************************************************/
	/************************************************************************/
	if (fnHookPostInitRenderer != NULL)
		fnHookPostInitRenderer(pRenderer);

#ifndef _DURANGO
	if (pRenderer->mSettings.mShaderTarget >= shader_target_6_0)
	{
		HRESULT dxrSuccess = gDxcDllHelper.Initialize();
		if (!SUCCEEDED(dxrSuccess))
		{
			pRenderer = NULL;
			return;
		}
	}
#endif

	// Renderer is good! Assign it to result!
	*(ppRenderer) = pRenderer;
}

void removeRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);

#ifndef _DURANGO
	if (gDxcDllHelper.IsEnabled())
	{
		gDxcDllHelper.Cleanup();
	}
#endif

	SAFE_FREE(pRenderer->pName);

	destroy_default_resources(pRenderer);

	// Destroy the Direct3D12 bits
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		remove_descriptor_heap(pRenderer->pCPUDescriptorHeaps[i]);
	}

	uint32_t gpuCount = 1;
	if (pRenderer->mSettings.mGpuMode == GPU_MODE_LINKED)
		gpuCount = pRenderer->mDxLinkedNodeCount;

	for (uint32_t i = 0; i < gpuCount; ++i)
	{
		remove_descriptor_heap(pRenderer->pCbvSrvUavHeap[i]);
		remove_descriptor_heap(pRenderer->pSamplerHeap[i]);
	}
	destroyAllocator(pRenderer->pResourceAllocator);
	RemoveDevice(pRenderer);

	// Free all the renderer components
	SAFE_FREE(pRenderer);
}
/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void addFence(Renderer* pRenderer, Fence** ppFence)
{
	//ASSERT that renderer is valid
	ASSERT(pRenderer);

	//create a Fence and ASSERT that it is valid
	Fence* pFence = (Fence*)conf_calloc(1, sizeof(*pFence));
	ASSERT(pFence);

	HRESULT hres = pRenderer->pDxDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_ARGS(&pFence->pDxFence));
	pFence->mFenceValue = 1;
	ASSERT(SUCCEEDED(hres));

	pFence->pDxWaitIdleFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	//set given pointer to new fence
	*ppFence = pFence;
}

void removeFence(Renderer* pRenderer, Fence* pFence)
{
	//ASSERT that renderer is valid
	ASSERT(pRenderer);
	//ASSERT that given fence to remove is valid
	ASSERT(pFence);

	SAFE_RELEASE(pFence->pDxFence);
	CloseHandle(pFence->pDxWaitIdleFenceEvent);

	//delete memory
	SAFE_FREE(pFence);
}

void addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
	//ASSERT that renderer is valid
	ASSERT(pRenderer);

	//create a semaphore and ASSERT that it is valid
	Semaphore* pSemaphore = (Semaphore*)conf_calloc(1, sizeof(*pSemaphore));
	ASSERT(pSemaphore);

	::addFence(pRenderer, &pSemaphore->pFence);

	//save newly created semaphore in given pointer
	*ppSemaphore = pSemaphore;
}

void removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
	//ASSERT that renderer and given semaphore are valid
	ASSERT(pRenderer);
	ASSERT(pSemaphore);

	::removeFence(pRenderer, pSemaphore->pFence);

	//safe delete that check for valid pointer
	SAFE_FREE(pSemaphore);
}

void addQueue(Renderer* pRenderer, QueueDesc* pQDesc, Queue** ppQueue)
{
	Queue* pQueue = (Queue*)conf_calloc(1, sizeof(*pQueue));
	ASSERT(pQueue != NULL);
	if (pQDesc->mNodeIndex)
	{
		ASSERT(pRenderer->mSettings.mGpuMode == GPU_MODE_LINKED && "Node Masking can only be used with Linked Multi GPU");
	}

	//provided description for queue creation
	pQueue->mQueueDesc = *pQDesc;

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = gDx12QueueFlagTranslator[pQueue->mQueueDesc.mFlag];
	queueDesc.Type = gDx12CmdTypeTranslator[pQueue->mQueueDesc.mType];
	queueDesc.Priority = gDx12QueuePriorityTranslator[pQueue->mQueueDesc.mPriority];
	queueDesc.NodeMask = util_calculate_node_mask(pRenderer, pQDesc->mNodeIndex);

	HRESULT hr = pRenderer->pDxDevice->CreateCommandQueue(&queueDesc, __uuidof(pQueue->pDxQueue), (void**)&(pQueue->pDxQueue));

	ASSERT(SUCCEEDED(hr));

	tinystl::string queueType;
	switch (queueDesc.Type)
	{
		case D3D12_COMMAND_LIST_TYPE_DIRECT: queueType = "GRAPHICS QUEUE"; break;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE: queueType = "COMPUTE QUEUE"; break;
		case D3D12_COMMAND_LIST_TYPE_COPY: queueType = "COPY QUEUE"; break;
		default: break;
	}

	tinystl::string queueName = tinystl::string::format("%s %u", queueType.c_str(), pQDesc->mNodeIndex);
	WCHAR           finalName[MAX_PATH] = {};
	mbstowcs(finalName, queueName.c_str(), queueName.size());
	pQueue->pDxQueue->SetName(finalName);

	pQueue->pRenderer = pRenderer;

	// Add queue fence. This fence will make sure we finish all GPU works before releasing the queue
	::addFence(pQueue->pRenderer, &pQueue->pQueueFence);

	*ppQueue = pQueue;
}

void removeQueue(Queue* pQueue)
{
	ASSERT(pQueue != NULL);

	// Make sure we finished all GPU works before we remove the queue
	FenceStatus fenceStatus;
	pQueue->pDxQueue->Signal(pQueue->pQueueFence->pDxFence, pQueue->pQueueFence->mFenceValue++);
	::getFenceStatus(pQueue->pRenderer, pQueue->pQueueFence, &fenceStatus);
	uint64_t fenceValue = pQueue->pQueueFence->mFenceValue - 1;
	if (fenceStatus == FENCE_STATUS_INCOMPLETE)
	{
		pQueue->pQueueFence->pDxFence->SetEventOnCompletion(fenceValue, pQueue->pQueueFence->pDxWaitIdleFenceEvent);
		WaitForSingleObject(pQueue->pQueueFence->pDxWaitIdleFenceEvent, INFINITE);
	}
	::removeFence(pQueue->pRenderer, pQueue->pQueueFence);

	SAFE_RELEASE(pQueue->pDxQueue);
	SAFE_FREE(pQueue);
}

void addCmdPool(Renderer* pRenderer, Queue* pQueue, bool transient, CmdPool** ppCmdPool)
{
	UNREF_PARAM(transient);
	//ASSERT that renderer is valid
	ASSERT(pRenderer);

	//create one new CmdPool and add to renderer
	CmdPool* pCmdPool = (CmdPool*)conf_calloc(1, sizeof(*pCmdPool));
	ASSERT(pCmdPool);

	CmdPoolDesc defaultDesc = {};
	defaultDesc.mCmdPoolType = pQueue->mQueueDesc.mType;

	pCmdPool->pQueue = pQueue;
	pCmdPool->mCmdPoolDesc.mCmdPoolType = defaultDesc.mCmdPoolType;

	*ppCmdPool = pCmdPool;
}

void removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	//check validity of given renderer and command pool
	ASSERT(pRenderer);
	ASSERT(pCmdPool);
	SAFE_FREE(pCmdPool);
}

void addCmd(CmdPool* pCmdPool, bool secondary, Cmd** ppCmd)
{
	UNREF_PARAM(secondary);
	//verify that given pool is valid
	ASSERT(pCmdPool);

	//allocate new command
	Cmd* pCmd = (Cmd*)conf_calloc(1, sizeof(*pCmd));
	ASSERT(pCmd);

	//set command pool of new command
	pCmd->pRenderer = pCmdPool->pQueue->pRenderer;
	pCmd->pCmdPool = pCmdPool;
	pCmd->mNodeIndex = pCmdPool->pQueue->mQueueDesc.mNodeIndex;

	//add command to pool
	//ASSERT(pCmdPool->pDxCmdAlloc);
	ASSERT(pCmdPool->pQueue->pRenderer);
	ASSERT(pCmdPool->mCmdPoolDesc.mCmdPoolType < CmdPoolType::MAX_CMD_TYPE);

	ASSERT(pCmd->pRenderer->pDxDevice);
	ASSERT(pCmdPool->mCmdPoolDesc.mCmdPoolType < CmdPoolType::MAX_CMD_TYPE);
	HRESULT hres = pCmd->pRenderer->pDxDevice->CreateCommandAllocator(
		gDx12CmdTypeTranslator[pCmdPool->mCmdPoolDesc.mCmdPoolType], __uuidof(pCmd->pDxCmdAlloc), (void**)&(pCmd->pDxCmdAlloc));
	ASSERT(SUCCEEDED(hres));

	ID3D12PipelineState* initialState = NULL;
	hres = pCmd->pRenderer->pDxDevice->CreateCommandList(
		pCmdPool->pQueue->pDxQueue->GetDesc().NodeMask, gDx12CmdTypeTranslator[pCmdPool->mCmdPoolDesc.mCmdPoolType], pCmd->pDxCmdAlloc,
		initialState, __uuidof(pCmd->pDxCmdList), (void**)&(pCmd->pDxCmdList));
	ASSERT(SUCCEEDED(hres));

	// Command lists are addd in the recording state, but there is nothing
	// to record yet. The main loop expects it to be closed, so close it now.
	hres = pCmd->pDxCmdList->Close();
	ASSERT(SUCCEEDED(hres));

	if (pCmdPool->mCmdPoolDesc.mCmdPoolType == CMD_POOL_DIRECT)
	{
		pCmd->pBoundColorFormats = (uint32_t*)conf_calloc(MAX_RENDER_TARGET_ATTACHMENTS, sizeof(uint32_t));
		pCmd->pBoundSrgbValues = (bool*)conf_calloc(MAX_RENDER_TARGET_ATTACHMENTS, sizeof(bool));
	}

	//set new command
	*ppCmd = pCmd;
}

void removeCmd(CmdPool* pCmdPool, Cmd* pCmd)
{
	//verify that given command and pool are valid
	ASSERT(pCmdPool);
	ASSERT(pCmd);

	if (pCmd->mViewGpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		remove_gpu_descriptor_handles(
			pCmd->pRenderer->pCbvSrvUavHeap[pCmd->mNodeIndex], &pCmd->mViewGpuHandle, MAX_DYNAMIC_VIEW_DESCRIPTORS_PER_FRAME);

	if (pCmd->mSamplerGpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		remove_gpu_descriptor_handles(
			pCmd->pRenderer->pSamplerHeap[pCmd->mNodeIndex], &pCmd->mSamplerGpuHandle, MAX_DYNAMIC_SAMPLER_DESCRIPTORS_PER_FRAME);

	if (pCmd->mTransientCBVs.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		remove_cpu_descriptor_handles(
			pCmd->pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], &pCmd->mTransientCBVs,
			MAX_TRANSIENT_CBVS_PER_FRAME);

	if (pCmd->pRootConstantRingBuffer)
		removeUniformRingBuffer(pCmd->pRootConstantRingBuffer);

	if (pCmd->pBoundColorFormats)
		SAFE_FREE(pCmd->pBoundColorFormats);

	if (pCmd->pBoundSrgbValues)
		SAFE_FREE(pCmd->pBoundSrgbValues);

	//remove command from pool
	SAFE_RELEASE(pCmd->pDxCmdAlloc);
	SAFE_RELEASE(pCmd->pDxCmdList);

	//delete command
	SAFE_FREE(pCmd);
}

void addCmd_n(CmdPool* pCmdPool, bool secondary, uint32_t cmdCount, Cmd*** pppCmd)
{
	//verify that ***cmd is valid
	ASSERT(pppCmd);

	//create new n command depending on cmdCount
	Cmd** ppCmd = (Cmd**)conf_calloc(cmdCount, sizeof(*ppCmd));
	ASSERT(ppCmd);

	//add n new cmds to given pool
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		::addCmd(pCmdPool, secondary, &(ppCmd[i]));
	}
	//return new list of cmds
	*pppCmd = ppCmd;
}

void removeCmd_n(CmdPool* pCmdPool, uint32_t cmdCount, Cmd** ppCmd)
{
	//verify that given command list is valid
	ASSERT(ppCmd);

	//remove every given cmd in array
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		::removeCmd(pCmdPool, ppCmd[i]);
	}

	SAFE_FREE(ppCmd);
}

void toggleVSync(Renderer* pRenderer, SwapChain** ppSwapChain)
{
	ASSERT(*ppSwapChain);

	//set descriptor vsync boolean
	(*ppSwapChain)->mDesc.mEnableVsync = !(*ppSwapChain)->mDesc.mEnableVsync;
#ifndef _DURANGO
	if (!(*ppSwapChain)->mDesc.mEnableVsync)
	{
		(*ppSwapChain)->mFlags |= DXGI_PRESENT_ALLOW_TEARING;
	}
	else
	{
		(*ppSwapChain)->mFlags &= ~DXGI_PRESENT_ALLOW_TEARING;
	}
#endif

	//toggle vsync present flag (this can go up to 4 but we don't need to refresh on nth vertical sync)
	(*ppSwapChain)->mDxSyncInterval = ((*ppSwapChain)->mDxSyncInterval + 1) % 2;
}

void addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppSwapChain);

	SwapChain* pSwapChain = (SwapChain*)conf_calloc(1, sizeof(*pSwapChain));
	pSwapChain->mDesc = *pDesc;
	pSwapChain->mDxSyncInterval = pSwapChain->mDesc.mEnableVsync ? 1 : 0;

	if (pSwapChain->mDesc.mSampleCount > SAMPLE_COUNT_1)
	{
		LOGWARNING("DirectX12 does not support multi-sample swapchains. Falling back to single sample swapchain");
		pSwapChain->mDesc.mSampleCount = SAMPLE_COUNT_1;
	}

	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width = pSwapChain->mDesc.mWidth;
	desc.Height = pSwapChain->mDesc.mHeight;
	desc.Format = util_to_dx_swapchain_format(pSwapChain->mDesc.mColorFormat);
	desc.Stereo = false;
	desc.SampleDesc.Count = 1;    // If multisampling is needed, we'll resolve it later
	desc.SampleDesc.Quality = pSwapChain->mDesc.mSampleQuality;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
	desc.BufferCount = pSwapChain->mDesc.mImageCount;
	desc.Scaling = DXGI_SCALING_STRETCH;
#ifdef _DURANGO
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
#else
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
#endif
	desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

#ifdef _DURANGO
	desc.Flags = desc.Format == DXGI_FORMAT_R10G10B10A2_UNORM
					 ? DXGIX_SWAP_CHAIN_FLAG_COLORIMETRY_RGB_BT2020_ST2084 | DXGIX_SWAP_CHAIN_FLAG_AUTOMATIC_GAMEDVR_TONEMAP
					 : 0;
#else
	desc.Flags = 0;
#endif

#if !defined(_DURANGO)
	BOOL allowTearing = FALSE;
	pRenderer->pDXGIFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
	desc.Flags |= allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	pSwapChain->mFlags |= (!pDesc->mEnableVsync && allowTearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;
#endif

	if (fnHookModifySwapChainDesc)
		fnHookModifySwapChainDesc(&desc);

	IDXGISwapChain1* swapchain;

#ifdef _DURANGO
	HRESULT hres = create_swap_chain(pRenderer, pSwapChain, &desc, &swapchain);
	ASSERT(SUCCEEDED(hres));
#else
	HWND hwnd = (HWND)pSwapChain->mDesc.pWindow->handle;

	HRESULT hres =
		pRenderer->pDXGIFactory->CreateSwapChainForHwnd(pDesc->ppPresentQueues[0]->pDxQueue, hwnd, &desc, NULL, NULL, &swapchain);
	ASSERT(SUCCEEDED(hres));

	hres = pRenderer->pDXGIFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	ASSERT(SUCCEEDED(hres));
#endif

	hres = swapchain->QueryInterface(__uuidof(pSwapChain->pDxSwapChain), (void**)&(pSwapChain->pDxSwapChain));
	ASSERT(SUCCEEDED(hres));
	swapchain->Release();

#ifndef _DURANGO
	// Allowing multiple command queues to present for applications like Alternate Frame Rendering
	if (pRenderer->mSettings.mGpuMode == GPU_MODE_LINKED && pDesc->mPresentQueueCount > 1)
	{
		ASSERT(pDesc->mPresentQueueCount == pDesc->mImageCount);

		IUnknown** ppQueues = (IUnknown**)alloca(pDesc->mPresentQueueCount * sizeof(IUnknown*));
		UINT*      pCreationMasks = (UINT*)alloca(pDesc->mPresentQueueCount * sizeof(UINT));
		for (uint32_t i = 0; i < pDesc->mPresentQueueCount; ++i)
		{
			ppQueues[i] = pDesc->ppPresentQueues[i]->pDxQueue;
			pCreationMasks[i] = (1 << pDesc->ppPresentQueues[i]->mQueueDesc.mNodeIndex);
		}

		if (pDesc->mPresentQueueCount)
		{
			pSwapChain->pDxSwapChain->ResizeBuffers1(
				desc.BufferCount, desc.Width, desc.Height, desc.Format, desc.Flags, pCreationMasks, ppQueues);
		}
	}
#endif

	// Create rendertargets from swapchain
	pSwapChain->ppDxSwapChainResources =
		(ID3D12Resource**)conf_calloc(pSwapChain->mDesc.mImageCount, sizeof(*pSwapChain->ppDxSwapChainResources));
	ASSERT(pSwapChain->ppDxSwapChainResources);
	for (uint32_t i = 0; i < pSwapChain->mDesc.mImageCount; ++i)
	{
		hres = pSwapChain->pDxSwapChain->GetBuffer(i, IID_ARGS(&pSwapChain->ppDxSwapChainResources[i]));
		ASSERT(SUCCEEDED(hres) && pSwapChain->ppDxSwapChainResources[i]);
	}

	RenderTargetDesc descColor = {};
	descColor.mWidth = pSwapChain->mDesc.mWidth;
	descColor.mHeight = pSwapChain->mDesc.mHeight;
	descColor.mDepth = 1;
	descColor.mArraySize = 1;
	descColor.mFormat = pSwapChain->mDesc.mColorFormat;
	descColor.mClearValue = pSwapChain->mDesc.mColorClearValue;
	descColor.mSampleCount = SAMPLE_COUNT_1;
	descColor.mSampleQuality = 0;
	descColor.mSrgb = pSwapChain->mDesc.mSrgb;

	pSwapChain->ppSwapchainRenderTargets =
		(RenderTarget**)conf_calloc(pSwapChain->mDesc.mImageCount, sizeof(*pSwapChain->ppSwapchainRenderTargets));

	for (uint32_t i = 0; i < pSwapChain->mDesc.mImageCount; ++i)
	{
		descColor.pNativeHandle = (void*)pSwapChain->ppDxSwapChainResources[i];
		::addRenderTarget(pRenderer, &descColor, &pSwapChain->ppSwapchainRenderTargets[i]);
	}

	*ppSwapChain = pSwapChain;
}

void removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
	for (unsigned i = 0; i < pSwapChain->mDesc.mImageCount; ++i)
	{
		::removeRenderTarget(pRenderer, pSwapChain->ppSwapchainRenderTargets[i]);
		SAFE_RELEASE(pSwapChain->ppDxSwapChainResources[i]);
	}

	SAFE_RELEASE(pSwapChain->pDxSwapChain);
	SAFE_FREE(pSwapChain->ppSwapchainRenderTargets);
	SAFE_FREE(pSwapChain->ppDxSwapChainResources);
	SAFE_FREE(pSwapChain);
}

void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
{
	//verify renderer validity
	ASSERT(pRenderer);
	//verify adding at least 1 buffer
	ASSERT(pDesc);
	ASSERT(pDesc->mSize > 0);

	//allocate new buffer
	Buffer* pBuffer = (Buffer*)conf_calloc(1, sizeof(*pBuffer));
	ASSERT(pBuffer);

	//set properties
	pBuffer->mDesc = *pDesc;

	//add to renderer
	// Align the buffer size to multiples of 256
	if ((pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER))
	{
		pBuffer->mDesc.mSize = round_up_64(pBuffer->mDesc.mSize, pRenderer->pActiveGpuSettings->mUniformBufferAlignment);
	}

	DECLARE_ZERO(D3D12_RESOURCE_DESC, desc);
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	//Alignment must be 64KB (D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) or 0, which is effectively 64KB.
	//https://msdn.microsoft.com/en-us/library/windows/desktop/dn903813(v=vs.85).aspx
	desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	desc.Width = pBuffer->mDesc.mSize;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	if (fnHookAddBuffer != NULL)
		fnHookAddBuffer(pBuffer, desc);

	if (pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER)
	{
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	// Adjust for padding
	UINT64 padded_size = 0;
	pRenderer->pDxDevice->GetCopyableFootprints(&desc, 0, 1, 0, NULL, NULL, NULL, &padded_size);
	pBuffer->mDesc.mSize = (uint64_t)padded_size;
	desc.Width = padded_size;

	if (pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU || pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_ONLY)
	{
		pBuffer->mDesc.mStartState = RESOURCE_STATE_GENERIC_READ;
	}

	D3D12_RESOURCE_STATES res_states = util_to_dx_resource_state(pBuffer->mDesc.mStartState);

	AllocatorMemoryRequirements mem_reqs = { 0 };
	mem_reqs.usage = (ResourceMemoryUsage)pBuffer->mDesc.mMemoryUsage;
	mem_reqs.flags = 0;
	if (pBuffer->mDesc.mFlags & BUFFER_CREATION_FLAG_OWN_MEMORY_BIT)
		mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT;
	if (pBuffer->mDesc.mFlags & BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT)
		mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT;
	if (pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_INDIRECT_BUFFER)
		mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_ALLOW_INDIRECT_BUFFER;
	if (pBuffer->mDesc.mNodeIndex || pBuffer->mDesc.pSharedNodeIndices)
		mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT;

	BufferCreateInfo alloc_info = { &desc, res_states, pBuffer->mDesc.pDebugName };
	HRESULT          hres = d3d12_createBuffer(pRenderer->pResourceAllocator, &alloc_info, &mem_reqs, pBuffer);
	ASSERT(SUCCEEDED(hres));

	// If buffer is a suballocation use offset in heap else use zero offset (placed resource / committed resource)
	if (pBuffer->pDxAllocation->GetResource())
		pBuffer->mPositionInHeap = pBuffer->pDxAllocation->GetOffset();
	else
		pBuffer->mPositionInHeap = 0;

	pBuffer->mCurrentState = pBuffer->mDesc.mStartState;
	pBuffer->mDxGpuAddress = pBuffer->pDxResource->GetGPUVirtualAddress() + pBuffer->mPositionInHeap;

	if ((pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER) &&
		!(pBuffer->mDesc.mFlags & BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION))
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = pBuffer->mDxGpuAddress;
		cbvDesc.SizeInBytes = (UINT)pBuffer->mDesc.mSize;
		add_cbv(pRenderer, &cbvDesc, &pBuffer->mDxCbvHandle);
	}

	if (pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_INDEX_BUFFER)
	{
		//set type of index (16 bit, 32 bit) int
		pBuffer->mDxIndexFormat = (INDEX_TYPE_UINT16 == pBuffer->mDesc.mIndexType) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	}

	if (pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_VERTEX_BUFFER)
	{
		if (pBuffer->mDesc.mVertexStride == 0)
		{
			LOGERRORF("Vertex Stride must be a non zero value");
			ASSERT(false);
		}
	}

	if ((pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_BUFFER) &&
		!(pBuffer->mDesc.mFlags & BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION))
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = pBuffer->mDesc.mFirstElement;
		srvDesc.Buffer.NumElements = (UINT)(pBuffer->mDesc.mElementCount);
		srvDesc.Buffer.StructureByteStride = (UINT)(pBuffer->mDesc.mStructStride);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		srvDesc.Format = util_to_dx_image_format(pDesc->mFormat, false);
		if (DESCRIPTOR_TYPE_BUFFER_RAW == (pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER_RAW))
		{
			if (pDesc->mFormat != ImageFormat::NONE)
				LOGWARNING("Raw buffers use R32 typeless format. Format will be ignored");
			srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			srvDesc.Buffer.Flags |= D3D12_BUFFER_SRV_FLAG_RAW;
		}
		// Cannot create a typed StructuredBuffer
		if (srvDesc.Format != DXGI_FORMAT_UNKNOWN)
		{
			srvDesc.Buffer.StructureByteStride = 0;
		}

		add_srv(pRenderer, pBuffer->pDxResource, &srvDesc, &pBuffer->mDxSrvHandle);
	}

	if ((pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER) &&
		!(pBuffer->mDesc.mFlags & BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION))
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = pBuffer->mDesc.mFirstElement;
		uavDesc.Buffer.NumElements = (UINT)(pBuffer->mDesc.mElementCount);
		uavDesc.Buffer.StructureByteStride = (UINT)(pBuffer->mDesc.mStructStride);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		if (DESCRIPTOR_TYPE_RW_BUFFER_RAW == (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER_RAW))
		{
			if (pDesc->mFormat != ImageFormat::NONE)
				LOGWARNING("Raw buffers use R32 typeless format. Format will be ignored");
			uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			uavDesc.Buffer.Flags |= D3D12_BUFFER_UAV_FLAG_RAW;
		}
		else if (pDesc->mFormat != ImageFormat::NONE)
		{
			uavDesc.Format = util_to_dx_image_format(pDesc->mFormat, false);
			D3D12_FEATURE_DATA_FORMAT_SUPPORT FormatSupport = { uavDesc.Format, D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE };
			HRESULT hr = pRenderer->pDxDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &FormatSupport, sizeof(FormatSupport));
			if (!SUCCEEDED(hr) || !(FormatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) ||
				!(FormatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
			{
				// Format does not support UAV Typed Load
				LOGWARNINGF("Cannot use Typed UAV for buffer format %u", (uint32_t)pDesc->mFormat);
				uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			}
		}
		// Cannot create a typed RWStructuredBuffer
		if (uavDesc.Format != DXGI_FORMAT_UNKNOWN)
		{
			uavDesc.Buffer.StructureByteStride = 0;
		}

		ID3D12Resource* pCounterResource = pBuffer->mDesc.pCounterBuffer ? pBuffer->mDesc.pCounterBuffer->pDxResource : NULL;
		add_uav(pRenderer, pBuffer->pDxResource, pCounterResource, &uavDesc, &pBuffer->mDxUavHandle);
	}

	pBuffer->mBufferId = (++gBufferIds << 8U) + Thread::GetCurrentThreadID();

	*pp_buffer = pBuffer;
}

void removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	UNREF_PARAM(pRenderer);
	ASSERT(pRenderer);
	ASSERT(pBuffer);

	d3d12_destroyBuffer(pRenderer->pResourceAllocator, pBuffer);

	if ((pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER) &&
		!(pBuffer->mDesc.mFlags & BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION))
	{
		remove_cbv(pRenderer, &pBuffer->mDxCbvHandle);
	}
	if ((pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_BUFFER) &&
		!(pBuffer->mDesc.mFlags & BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION))
	{
		remove_srv(pRenderer, &pBuffer->mDxSrvHandle);
	}
	if ((pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER) &&
		!(pBuffer->mDesc.mFlags & BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION))
	{
		remove_uav(pRenderer, &pBuffer->mDxUavHandle);
	}

	SAFE_FREE(pBuffer);
}

void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
	UNREF_PARAM(pRenderer);
	ASSERT(pBuffer->mDesc.mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to map non-cpu accessible resource");

	D3D12_RANGE range = { pBuffer->mPositionInHeap, pBuffer->mPositionInHeap + pBuffer->mDesc.mSize };
	if (pRange)
	{
		range.Begin += pRange->mOffset;
		range.End = range.Begin + pRange->mSize;
	}
	HRESULT hr = pBuffer->pDxResource->Map(0, &range, &pBuffer->pCpuMappedAddress);
	ASSERT(SUCCEEDED(hr) && pBuffer->pCpuMappedAddress);
}

void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	UNREF_PARAM(pRenderer);
	ASSERT(pBuffer->mDesc.mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to unmap non-cpu accessible resource");

	pBuffer->pDxResource->Unmap(0, NULL);
	pBuffer->pCpuMappedAddress = NULL;
}

void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mWidth && pDesc->mHeight && (pDesc->mDepth || pDesc->mArraySize));
	if (pDesc->mSampleCount > SAMPLE_COUNT_1 && pDesc->mMipLevels > 1)
	{
		LOGERROR("Multi-Sampled textures cannot have mip maps");
		ASSERT(false);
		return;
	}

	//allocate new texture
	Texture* pTexture = (Texture*)conf_calloc(1, sizeof(*pTexture));
	ASSERT(pTexture);

	//set texture properties
	pTexture->mDesc = *pDesc;
	pTexture->mTextureId = (++gTextureIds << 8U) + Thread::GetCurrentThreadID();

	if (pDesc->pNativeHandle)
	{
		pTexture->mOwnsImage = false;
		pTexture->pDxResource = (ID3D12Resource*)pDesc->pNativeHandle;
	}
	else
	{
		pTexture->mOwnsImage = true;
	}

	//add to gpu
	D3D12_RESOURCE_DESC desc = {};
	DXGI_FORMAT         dxFormat = util_to_dx_image_format(pDesc->mFormat, pDesc->mSrgb);
	DescriptorType      descriptors = pDesc->mDescriptors;

	ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

	if (NULL == pTexture->pDxResource)
	{
		D3D12_RESOURCE_DIMENSION res_dim = D3D12_RESOURCE_DIMENSION_UNKNOWN;
		if (pDesc->mDepth > 1)
			res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		else if (pDesc->mHeight > 1)
			res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		else
			res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE1D;

		desc.Dimension = res_dim;
		//On PC, If Alignment is set to 0, the runtime will use 4MB for MSAA textures and 64KB for everything else.
		//On XBox, We have to explicitlly assign D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT if MSAA is used
		desc.Alignment = (UINT)pDesc->mSampleCount > 1 ? D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT : 0;
		desc.Width = pDesc->mWidth;
		desc.Height = pDesc->mHeight;
		desc.DepthOrArraySize = (UINT16)(pDesc->mArraySize != 1 ? pDesc->mArraySize : pDesc->mDepth);
		desc.MipLevels = (UINT16)pDesc->mMipLevels;
		desc.Format = util_to_dx_image_format_typeless(pDesc->mFormat);
		desc.SampleDesc.Count = (UINT)pDesc->mSampleCount;
		desc.SampleDesc.Quality = (UINT)pDesc->mSampleQuality;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS data;
		data.Format = desc.Format;
		data.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
		data.SampleCount = desc.SampleDesc.Count;
		pRenderer->pDxDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(data));
		while (data.NumQualityLevels == 0 && data.SampleCount > 0)
		{
			LOGWARNINGF("Sample Count (%u) not supported. Trying a lower sample count (%u)", data.SampleCount, data.SampleCount / 2);
			data.SampleCount = desc.SampleDesc.Count / 2;
			pRenderer->pDxDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(data));
		}
		desc.SampleDesc.Count = data.SampleCount;
		pTexture->mDesc.mSampleCount = (SampleCount)desc.SampleDesc.Count;

		// Decide UAV flags
		if (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		// Decide render target flags
		if (pDesc->mStartState & RESOURCE_STATE_RENDER_TARGET)
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		else if (pDesc->mStartState & RESOURCE_STATE_DEPTH_WRITE)
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		// Decide sharing flags
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT)
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		}

		if (fnHookResourceFlags != NULL)
			fnHookResourceFlags(desc.Flags, pDesc->mFlags);

		DECLARE_ZERO(D3D12_CLEAR_VALUE, clearValue);
		clearValue.Format = dxFormat;
		if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
		{
			clearValue.DepthStencil.Depth = pDesc->mClearValue.depth;
			clearValue.DepthStencil.Stencil = (UINT8)pDesc->mClearValue.stencil;
		}
		else
		{
			clearValue.Color[0] = pDesc->mClearValue.r;
			clearValue.Color[1] = pDesc->mClearValue.g;
			clearValue.Color[2] = pDesc->mClearValue.b;
			clearValue.Color[3] = pDesc->mClearValue.a;
		}

		D3D12_CLEAR_VALUE*    pClearValue = NULL;
		D3D12_RESOURCE_STATES res_states = util_to_dx_resource_state(pDesc->mStartState);

		if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
		{
			pClearValue = &clearValue;
		}

		AllocatorMemoryRequirements mem_reqs = { 0 };
		mem_reqs.usage = (ResourceMemoryUsage)RESOURCE_MEMORY_USAGE_GPU_ONLY;
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT)
			mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT;
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_BIT)
			mem_reqs.flags |= (RESOURCE_MEMORY_REQUIREMENT_SHARED_BIT | RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT);
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT)
			mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_SHARED_ADAPTER_BIT;
		if (pDesc->mNodeIndex > 0 || pDesc->pSharedNodeIndices)
			mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT;

		TextureCreateInfo alloc_info = { pDesc, &desc, pClearValue, res_states, pDesc->pDebugName };
		HRESULT           hr = d3d12_createTexture(pRenderer->pResourceAllocator, &alloc_info, &mem_reqs, pTexture);
		ASSERT(SUCCEEDED(hr));

		UINT64 buffer_size = 0;
		pRenderer->pDxDevice->GetCopyableFootprints(
			&desc, 0, desc.MipLevels * (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1 : desc.DepthOrArraySize), 0, NULL, NULL,
			NULL, &buffer_size);

		pTexture->mTextureSize = buffer_size;
		pTexture->mCurrentState = pDesc->mStartState;
	}
	else
	{
		desc = pTexture->pDxResource->GetDesc();
		dxFormat = desc.Format;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC  srvDesc = {};
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	switch (desc.Dimension)
	{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		{
			if (desc.DepthOrArraySize > 1)
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
				// SRV
				srvDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
				srvDesc.Texture1DArray.FirstArraySlice = 0;
				srvDesc.Texture1DArray.MipLevels = desc.MipLevels;
				srvDesc.Texture1DArray.MostDetailedMip = 0;
				// UAV
				uavDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
				uavDesc.Texture1DArray.FirstArraySlice = 0;
				uavDesc.Texture1DArray.MipSlice = 0;
			}
			else
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
				// SRV
				srvDesc.Texture1D.MipLevels = desc.MipLevels;
				srvDesc.Texture1D.MostDetailedMip = 0;
				// UAV
				uavDesc.Texture1D.MipSlice = 0;
			}
			break;
		}
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		{
			if (DESCRIPTOR_TYPE_TEXTURE_CUBE == (descriptors & DESCRIPTOR_TYPE_TEXTURE_CUBE))
			{
				ASSERT(desc.DepthOrArraySize % 6 == 0);

				if (desc.DepthOrArraySize > 6)
				{
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
					// SRV
					srvDesc.TextureCubeArray.First2DArrayFace = 0;
					srvDesc.TextureCubeArray.MipLevels = desc.MipLevels;
					srvDesc.TextureCubeArray.MostDetailedMip = 0;
					srvDesc.TextureCubeArray.NumCubes = desc.DepthOrArraySize / 6;
				}
				else
				{
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
					// SRV
					srvDesc.TextureCube.MipLevels = desc.MipLevels;
					srvDesc.TextureCube.MostDetailedMip = 0;
				}

				// UAV
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				uavDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
				uavDesc.Texture2DArray.FirstArraySlice = 0;
				uavDesc.Texture2DArray.MipSlice = 0;
				uavDesc.Texture2DArray.PlaneSlice = 0;
			}
			else
			{
				if (desc.DepthOrArraySize > 1)
				{
					if (desc.SampleDesc.Count > SAMPLE_COUNT_1)
					{
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
						// Cannot create a multisampled uav
						// SRV
						srvDesc.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
						srvDesc.Texture2DMSArray.FirstArraySlice = 0;
						// No UAV
					}
					else
					{
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
						uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
						// SRV
						srvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
						srvDesc.Texture2DArray.FirstArraySlice = 0;
						srvDesc.Texture2DArray.MipLevels = desc.MipLevels;
						srvDesc.Texture2DArray.MostDetailedMip = 0;
						srvDesc.Texture2DArray.PlaneSlice = 0;
						// UAV
						uavDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
						uavDesc.Texture2DArray.FirstArraySlice = 0;
						uavDesc.Texture2DArray.MipSlice = 0;
						uavDesc.Texture2DArray.PlaneSlice = 0;
					}
				}
				else
				{
					if (desc.SampleDesc.Count > SAMPLE_COUNT_1)
					{
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
						// Cannot create a multisampled uav
					}
					else
					{
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
						uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
						// SRV
						srvDesc.Texture2D.MipLevels = desc.MipLevels;
						srvDesc.Texture2D.MostDetailedMip = 0;
						srvDesc.Texture2D.PlaneSlice = 0;
						// UAV
						uavDesc.Texture2D.MipSlice = 0;
						uavDesc.Texture2D.PlaneSlice = 0;
					}
				}
			}
			break;
		}
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
			// SRV
			srvDesc.Texture3D.MipLevels = desc.MipLevels;
			srvDesc.Texture3D.MostDetailedMip = 0;
			// UAV
			uavDesc.Texture3D.MipSlice = 0;
			uavDesc.Texture3D.FirstWSlice = 0;
			uavDesc.Texture3D.WSize = desc.DepthOrArraySize;
			break;
		}
		default: break;
	}

	if (descriptors & DESCRIPTOR_TYPE_TEXTURE)
	{
		ASSERT(srvDesc.ViewDimension != D3D12_SRV_DIMENSION_UNKNOWN);

		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = util_to_dx_srv_format(dxFormat);
		add_srv(pRenderer, pTexture->pDxResource, &srvDesc, &pTexture->mDxSRVDescriptor);
	}

	if (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
	{
		uavDesc.Format = util_to_dx_uav_format(dxFormat);
		pTexture->pDxUAVDescriptors = (D3D12_CPU_DESCRIPTOR_HANDLE*)conf_calloc(desc.MipLevels, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));
		for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
		{
			uavDesc.Texture1DArray.MipSlice = i;
			add_uav(pRenderer, pTexture->pDxResource, NULL, &uavDesc, &pTexture->pDxUAVDescriptors[i]);
		}
	}

	//save tetxure in given pointer
	*ppTexture = pTexture;

	// TODO: Handle host visible textures in a better way
	if (pDesc->mHostVisible)
	{
		internal_log(
			LOG_TYPE_WARN,
			"D3D12 does not support host visible textures, memory of resulting texture will not be mapped for CPU visibility",
			"addTexture");
	}
}

void removeTexture(Renderer* pRenderer, Texture* pTexture)
{
	ASSERT(pRenderer);
	ASSERT(pTexture);

	//delete texture descriptors
	if (pTexture->mDxSRVDescriptor.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
		remove_srv(pRenderer, &pTexture->mDxSRVDescriptor);

	if (pTexture->pDxUAVDescriptors)
	{
		for (uint32_t i = 0; i < pTexture->mDesc.mMipLevels; ++i)
		{
			remove_uav(pRenderer, &pTexture->pDxUAVDescriptors[i]);
		}
	}

	if (pTexture->mOwnsImage)
	{
		d3d12_destroyTexture(pRenderer->pResourceAllocator, pTexture);
	}

	SAFE_FREE(pTexture->pDxUAVDescriptors);
	SAFE_FREE(pTexture);
}

void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppRenderTarget);

	bool isDepth = ImageFormat::IsDepthFormat(pDesc->mFormat);

	ASSERT(!((isDepth) && (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)) && "Cannot use depth stencil as UAV");

	((RenderTargetDesc*)pDesc)->mMipLevels = max(1U, pDesc->mMipLevels);

	RenderTarget* pRenderTarget = (RenderTarget*)conf_calloc(1, sizeof(*pRenderTarget));
	pRenderTarget->mDesc = *pDesc;

	//add to gpu
	DXGI_FORMAT dxFormat = util_to_dx_image_format(pRenderTarget->mDesc.mFormat, pDesc->mSrgb);
	ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

	TextureDesc textureDesc = {};
	textureDesc.mArraySize = pDesc->mArraySize;
	textureDesc.mClearValue = pDesc->mClearValue;
	textureDesc.mDepth = pDesc->mDepth;
	textureDesc.mFlags = pDesc->mFlags;
	textureDesc.mFormat = pDesc->mFormat;
	textureDesc.mHeight = pDesc->mHeight;
	textureDesc.mMipLevels = pDesc->mMipLevels;
	textureDesc.mSampleCount = pDesc->mSampleCount;
	textureDesc.mSampleQuality = pDesc->mSampleQuality;

	if (!isDepth)
		textureDesc.mStartState |= RESOURCE_STATE_RENDER_TARGET;
	else
		textureDesc.mStartState |= RESOURCE_STATE_DEPTH_WRITE;

	// Set this by default to be able to sample the rendertarget in shader
	textureDesc.mWidth = pDesc->mWidth;
	textureDesc.pNativeHandle = pDesc->pNativeHandle;
	textureDesc.mSrgb = pDesc->mSrgb;
	textureDesc.pDebugName = pDesc->pDebugName;
	textureDesc.mNodeIndex = pDesc->mNodeIndex;
	textureDesc.pSharedNodeIndices = pDesc->pSharedNodeIndices;
	textureDesc.mSharedNodeIndexCount = pDesc->mSharedNodeIndexCount;
	textureDesc.mDescriptors = pDesc->mDescriptors;
	// Create SRV by default for a render target
	textureDesc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE;

	addTexture(pRenderer, &textureDesc, &pRenderTarget->pTexture);

	D3D12_RESOURCE_DESC desc = pRenderTarget->pTexture->pDxResource->GetDesc();

	uint32_t numRTVs = desc.MipLevels;
	if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		numRTVs *= desc.DepthOrArraySize;

	pRenderTarget->pDxDescriptors = (D3D12_CPU_DESCRIPTOR_HANDLE*)conf_calloc(numRTVs + 1, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));
	!ImageFormat::IsDepthFormat(pRenderTarget->mDesc.mFormat)
		? add_rtv(pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, 0, -1, &pRenderTarget->pDxDescriptors[0])
		: add_dsv(pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, 0, -1, &pRenderTarget->pDxDescriptors[0]);

	for (uint32_t i = 0; i < desc.MipLevels; ++i)
	{
		if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
			(pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		{
			for (uint32_t j = 0; j < desc.DepthOrArraySize; ++j)
			{
				!ImageFormat::IsDepthFormat(pRenderTarget->mDesc.mFormat)
					? add_rtv(
						  pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, i, j,
						  &pRenderTarget->pDxDescriptors[1 + i * desc.DepthOrArraySize + j])
					: add_dsv(
						  pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, i, j,
						  &pRenderTarget->pDxDescriptors[1 + i * desc.DepthOrArraySize + j]);
			}
		}
		else
		{
			!ImageFormat::IsDepthFormat(pRenderTarget->mDesc.mFormat)
				? add_rtv(pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, i, -1, &pRenderTarget->pDxDescriptors[1 + i])
				: add_dsv(pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, i, -1, &pRenderTarget->pDxDescriptors[1 + i]);
		}
	}

	*ppRenderTarget = pRenderTarget;
}

void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
	removeTexture(pRenderer, pRenderTarget->pTexture);

	!ImageFormat::IsDepthFormat(pRenderTarget->mDesc.mFormat) ? remove_rtv(pRenderer, &pRenderTarget->pDxDescriptors[0])
															  : remove_dsv(pRenderer, &pRenderTarget->pDxDescriptors[0]);

	const uint32_t depthOrArraySize = pRenderTarget->mDesc.mArraySize * pRenderTarget->mDesc.mDepth;
	if ((pRenderTarget->mDesc.mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pRenderTarget->mDesc.mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
	{
		for (uint32_t i = 0; i < pRenderTarget->mDesc.mMipLevels; ++i)
			for (uint32_t j = 0; j < depthOrArraySize; ++j)
				!ImageFormat::IsDepthFormat(pRenderTarget->mDesc.mFormat)
					? remove_rtv(pRenderer, &pRenderTarget->pDxDescriptors[1 + i * depthOrArraySize + j])
					: remove_dsv(pRenderer, &pRenderTarget->pDxDescriptors[1 + i * depthOrArraySize + j]);
	}
	else
	{
		for (uint32_t i = 0; i < pRenderTarget->mDesc.mMipLevels; ++i)
			!ImageFormat::IsDepthFormat(pRenderTarget->mDesc.mFormat) ? remove_rtv(pRenderer, &pRenderTarget->pDxDescriptors[1 + i])
																	  : remove_dsv(pRenderer, &pRenderTarget->pDxDescriptors[1 + i]);
	}

	SAFE_FREE(pRenderTarget->pDxDescriptors);
	SAFE_FREE(pRenderTarget);
}

void addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDxDevice);
	ASSERT(pDesc->mCompareFunc < MAX_COMPARE_MODES);

	//allocate new sampler
	Sampler* pSampler = (Sampler*)conf_calloc(1, sizeof(*pSampler));
	ASSERT(pSampler);

	//add sampler to gpu
	pSampler->mDxSamplerDesc.Filter = util_to_dx_filter(
		pDesc->mMinFilter, pDesc->mMagFilter, pDesc->mMipMapMode, pDesc->mMaxAnisotropy > 0.0f,
		(pDesc->mCompareFunc != CMP_NEVER ? true : false));
	pSampler->mDxSamplerDesc.AddressU = util_to_dx_texture_address_mode(pDesc->mAddressU);
	pSampler->mDxSamplerDesc.AddressV = util_to_dx_texture_address_mode(pDesc->mAddressV);
	pSampler->mDxSamplerDesc.AddressW = util_to_dx_texture_address_mode(pDesc->mAddressW);
	pSampler->mDxSamplerDesc.MipLODBias = pDesc->mMipLosBias;
	pSampler->mDxSamplerDesc.MaxAnisotropy = max((UINT)pDesc->mMaxAnisotropy, 1U);
	pSampler->mDxSamplerDesc.ComparisonFunc = gDx12ComparisonFuncTranslator[pDesc->mCompareFunc];
	pSampler->mDxSamplerDesc.BorderColor[0] = 0.0f;
	pSampler->mDxSamplerDesc.BorderColor[1] = 0.0f;
	pSampler->mDxSamplerDesc.BorderColor[2] = 0.0f;
	pSampler->mDxSamplerDesc.BorderColor[3] = 0.0f;
	pSampler->mDxSamplerDesc.MinLOD = 0.0f;
	pSampler->mDxSamplerDesc.MaxLOD = ((pDesc->mMipMapMode == MIPMAP_MODE_LINEAR) ? D3D12_FLOAT32_MAX : 0.0f);

	add_sampler(pRenderer, &pSampler->mDxSamplerDesc, &pSampler->mDxSamplerHandle);

	pSampler->mSamplerId = (++gSamplerIds << 8U) + Thread::GetCurrentThreadID();

	*ppSampler = pSampler;
}

void removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
	ASSERT(pRenderer);
	ASSERT(pSampler);

	remove_sampler(pRenderer, &pSampler->mDxSamplerHandle);

	// Nop op

	SAFE_FREE(pSampler);
}
/************************************************************************/
// Shader Functions
/************************************************************************/
template <typename BlobType>
static inline tinystl::string convertBlobToString(BlobType* pBlob)
{
	tinystl::vector<char> infoLog(pBlob->GetBufferSize() + 1);
	memcpy(infoLog.data(), pBlob->GetBufferPointer(), pBlob->GetBufferSize());
	infoLog[pBlob->GetBufferSize()] = 0;
	return tinystl::string(infoLog.data());
}

void compileShader(
	Renderer* pRenderer, ShaderTarget shaderTarget, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code,
	uint32_t macroCount, ShaderMacro* pMacros, void* (*allocator)(size_t a), uint32_t* pByteCodeSize, char** ppByteCode)
{
	if (shaderTarget > pRenderer->mSettings.mShaderTarget)
	{
		ErrorMsg(
			"Requested shader target (%u) is higher than the shader target that the renderer supports (%u). Shader wont be compiled",
			(uint32_t)shaderTarget, (uint32_t)pRenderer->mSettings.mShaderTarget);
		return;
	}
#ifndef _DURANGO
	if (shaderTarget >= shader_target_6_0)
	{
#define d3d_call(x)      \
	if (!SUCCEEDED((x))) \
	{                    \
		ASSERT(false);   \
		return;          \
	}

		IDxcCompiler* pCompiler;
		IDxcLibrary*  pLibrary;
		d3d_call(gDxcDllHelper.CreateInstance(CLSID_DxcCompiler, &pCompiler));
		d3d_call(gDxcDllHelper.CreateInstance(CLSID_DxcLibrary, &pLibrary));

		/************************************************************************/
		// Determine shader target
		/************************************************************************/
		int major;
		int minor;
		switch (shaderTarget)
		{
			default:
			case shader_target_6_0:
			{
				major = 6;
				minor = 0;
			}
			break;
		}
		tinystl::string target;
		switch (stage)
		{
			case SHADER_STAGE_VERT: target = tinystl::string::format("vs_%d_%d", major, minor); break;
			case SHADER_STAGE_TESC: target = tinystl::string::format("hs_%d_%d", major, minor); break;
			case SHADER_STAGE_TESE: target = tinystl::string::format("ds_%d_%d", major, minor); break;
			case SHADER_STAGE_GEOM: target = tinystl::string::format("gs_%d_%d", major, minor); break;
			case SHADER_STAGE_FRAG: target = tinystl::string::format("ps_%d_%d", major, minor); break;
			case SHADER_STAGE_COMP: target = tinystl::string::format("cs_%d_%d", major, minor); break;
			default: break;
		}
		WCHAR* wTarget = (WCHAR*)alloca((target.size() + 1) * sizeof(WCHAR));
		mbstowcs(wTarget, target.c_str(), target.size());
		wTarget[target.size()] = L'\0';
		/************************************************************************/
		// Collect macros
		/************************************************************************/
		uint32_t namePoolSize = 0;
		for (uint32_t i = 0; i < macroCount; ++i)
		{
			namePoolSize += (uint32_t)pMacros[i].definition.size() + 1;
			namePoolSize += (uint32_t)pMacros[i].value.size() + 1;
		}
		WCHAR* namePool = NULL;
		if (namePoolSize)
			namePool = (WCHAR*)alloca(namePoolSize * sizeof(WCHAR));

		// Extract shader macro definitions into D3D_SHADER_MACRO scruct
		// Allocate Size+2 structs: one for D3D12 1 definition and one for null termination
		DxcDefine* macros = (DxcDefine*)alloca((macroCount + 1) * sizeof(DxcDefine));
		macros[0] = { L"D3D12", L"1" };
		WCHAR* pCurrent = namePool;
		for (uint32_t j = 0; j < macroCount; ++j)
		{
			uint32_t len = (uint32_t)pMacros[j].definition.size();
			mbstowcs(pCurrent, pMacros[j].definition.c_str(), len);
			pCurrent[len] = L'\0';
			macros[j + 1].Name = pCurrent;
			pCurrent += (len + 1);

			len = (uint32_t)pMacros[j].value.size();
			mbstowcs(pCurrent, pMacros[j].value.c_str(), len);
			pCurrent[len] = L'\0';
			macros[j + 1].Value = pCurrent;
			pCurrent += (len + 1);
		}
		/************************************************************************/
		// Compiler args
		/************************************************************************/
		tinystl::vector<const WCHAR*> compilerArgs;
		compilerArgs.push_back(L"-Zi");
		compilerArgs.push_back(L"-all_resources_bound");
#if defined(_DEBUG)
		compilerArgs.push_back(L"-Od");
#else
		compilerArgs.push_back(L"-O3");
#endif
		/************************************************************************/
		// Create blob from the string
		/************************************************************************/
		IDxcBlobEncoding* pTextBlob;
		d3d_call(pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)code, (UINT32)codeSize, 0, &pTextBlob));
		IDxcOperationResult* pResult;
		WCHAR                filename[MAX_PATH] = {};
		mbstowcs(filename, fileName, strlen(fileName));
		IDxcIncludeHandler* pInclude = NULL;
		pLibrary->CreateIncludeHandler(&pInclude);
		d3d_call(pCompiler->Compile(
			pTextBlob, filename, L"main", wTarget, compilerArgs.data(), (UINT32)compilerArgs.size(), macros, macroCount + 1, pInclude,
			&pResult));

		pInclude->Release();
		pLibrary->Release();
		pCompiler->Release();
		/************************************************************************/
		// Verify the result
		/************************************************************************/
		HRESULT resultCode;
		d3d_call(pResult->GetStatus(&resultCode));
		if (FAILED(resultCode))
		{
			IDxcBlobEncoding* pError;
			d3d_call(pResult->GetErrorBuffer(&pError));
			tinystl::string log = convertBlobToString(pError);
			ErrorMsg(log.c_str());
			pError->Release();
			return;
		}
		/************************************************************************/
		// Collect blob
		/************************************************************************/
		IDxcBlob* pBlob;
		d3d_call(pResult->GetResult(&pBlob));

		char* pByteCode = (char*)allocator(pBlob->GetBufferSize());
		memcpy(pByteCode, pBlob->GetBufferPointer(), pBlob->GetBufferSize());
		*pByteCodeSize = (uint32_t)pBlob->GetBufferSize();
		*ppByteCode = pByteCode;

		pBlob->Release();
		/************************************************************************/
		/************************************************************************/
	}
	else
#endif
	{
#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compile_flags = D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compile_flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		compile_flags |= (D3DCOMPILE_DEBUG | D3DCOMPILE_ALL_RESOURCES_BOUND | D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);

		int major;
		int minor;
		switch (shaderTarget)
		{
			default:
			case shader_target_5_1:
			{
				major = 5;
				minor = 1;
			}
			break;
			case shader_target_6_0:
			{
				major = 6;
				minor = 0;
			}
			break;
		}

		tinystl::string target;
		switch (stage)
		{
			case SHADER_STAGE_VERT: target = tinystl::string::format("vs_%d_%d", major, minor); break;
			case SHADER_STAGE_TESC: target = tinystl::string::format("hs_%d_%d", major, minor); break;
			case SHADER_STAGE_TESE: target = tinystl::string::format("ds_%d_%d", major, minor); break;
			case SHADER_STAGE_GEOM: target = tinystl::string::format("gs_%d_%d", major, minor); break;
			case SHADER_STAGE_FRAG: target = tinystl::string::format("ps_%d_%d", major, minor); break;
			case SHADER_STAGE_COMP: target = tinystl::string::format("cs_%d_%d", major, minor); break;
			default: break;
		}

		// Extract shader macro definitions into D3D_SHADER_MACRO scruct
		// Allocate Size+2 structs: one for D3D12 1 definition and one for null termination
		D3D_SHADER_MACRO* macros = (D3D_SHADER_MACRO*)alloca((macroCount + 2) * sizeof(D3D_SHADER_MACRO));
		macros[0] = { "D3D12", "1" };
		for (uint32_t j = 0; j < macroCount; ++j)
		{
			macros[j + 1] = { pMacros[j].definition, pMacros[j].value };
		}
		macros[macroCount + 1] = { NULL, NULL };

		if (fnHookShaderCompileFlags != NULL)
			fnHookShaderCompileFlags(compile_flags);

		tinystl::string entryPoint = "main";
		ID3DBlob*       compiled_code = NULL;
		ID3DBlob*       error_msgs = NULL;
		HRESULT         hres = D3DCompile2(
            code, (size_t)codeSize, fileName, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint.c_str(), target, compile_flags, 0, 0,
            NULL, 0, &compiled_code, &error_msgs);
		if (FAILED(hres))
		{
			char* msg = (char*)conf_calloc(error_msgs->GetBufferSize() + 1, sizeof(*msg));
			ASSERT(msg);
			memcpy(msg, error_msgs->GetBufferPointer(), error_msgs->GetBufferSize());
			tinystl::string error = tinystl::string(fileName) + " " + msg;
			ErrorMsg(error);
			SAFE_FREE(msg);
		}
		ASSERT(SUCCEEDED(hres));

		char* pByteCode = (char*)allocator(compiled_code->GetBufferSize());
		memcpy(pByteCode, compiled_code->GetBufferPointer(), compiled_code->GetBufferSize());

		*pByteCodeSize = (uint32_t)compiled_code->GetBufferSize();
		*ppByteCode = pByteCode;
		SAFE_RELEASE(compiled_code);
	}
}

// renderer shader macros allocated on stack
const RendererShaderDefinesDesc get_renderer_shaderdefines(Renderer* pRenderer)
{
	UNREF_PARAM(pRenderer);

	RendererShaderDefinesDesc defineDesc = { NULL, 0 };
	return defineDesc;
}

void addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mStages);
	ASSERT(ppShaderProgram);

	Shader* pShaderProgram = (Shader*)conf_calloc(1, sizeof(*pShaderProgram));
	ASSERT(pShaderProgram);
	pShaderProgram->mStages = pDesc->mStages;

	uint32_t                   reflectionCount = 0;
	tinystl::vector<ID3DBlob*> blobs(SHADER_STAGE_COUNT);

	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		ShaderStage                  stage_mask = (ShaderStage)(1 << i);
		const BinaryShaderStageDesc* pStage = NULL;

		if (stage_mask == (pShaderProgram->mStages & stage_mask))
		{
			switch (stage_mask)
			{
				case SHADER_STAGE_VERT:
				{
					pStage = &pDesc->mVert;
				}
				break;
				case SHADER_STAGE_HULL:
				{
					pStage = &pDesc->mHull;
				}
				break;
				case SHADER_STAGE_DOMN:
				{
					pStage = &pDesc->mDomain;
				}
				break;
				case SHADER_STAGE_GEOM:
				{
					pStage = &pDesc->mGeom;
				}
				break;
				case SHADER_STAGE_FRAG:
				{
					pStage = &pDesc->mFrag;
				}
				break;
				case SHADER_STAGE_COMP:
				{
					pStage = &pDesc->mComp;
				}
				break;
			}

			D3DCreateBlob(pStage->mByteCodeSize, &blobs[reflectionCount]);
			memcpy(blobs[reflectionCount]->GetBufferPointer(), pStage->pByteCode, pStage->mByteCodeSize);

			d3d12_createShaderReflection(
				(uint8_t*)(blobs[reflectionCount]->GetBufferPointer()), (uint32_t)blobs[reflectionCount]->GetBufferSize(), stage_mask,
				&pShaderProgram->mReflection.mStageReflections[reflectionCount]);

			reflectionCount++;
		}
	}

	pShaderProgram->pShaderBlobs = (ID3DBlob**)conf_calloc(reflectionCount, sizeof(ID3DBlob*));
	for (uint32_t i = 0; i < reflectionCount; ++i)
	{
		blobs[i]->QueryInterface(__uuidof(ID3DBlob), (void**)&pShaderProgram->pShaderBlobs[i]);
		blobs[i]->Release();
	}

	createPipelineReflection(pShaderProgram->mReflection.mStageReflections, reflectionCount, &pShaderProgram->mReflection);

	*ppShaderProgram = pShaderProgram;
}

void removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
	UNREF_PARAM(pRenderer);

	//remove given shader
	for (uint32_t i = 0; i < pShaderProgram->mReflection.mStageReflectionCount; ++i)
		SAFE_RELEASE(pShaderProgram->pShaderBlobs[i]);
	destroyPipelineReflection(&pShaderProgram->mReflection);

	SAFE_FREE(pShaderProgram->pShaderBlobs);
	SAFE_FREE(pShaderProgram);
}
/************************************************************************/
// Root Signature Functions
/************************************************************************/
void addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
{
	ASSERT(pRenderer->pActiveGpuSettings->mMaxRootSignatureDWORDS > 0);

	RootSignature* pRootSignature = (RootSignature*)conf_calloc(1, sizeof(*pRootSignature));
	ASSERT(pRootSignature);

	tinystl::vector<UpdateFrequencyLayoutInfo>                 layouts(DESCRIPTOR_UPDATE_FREQ_COUNT);
	tinystl::vector<ShaderResource>                            shaderResources;
	tinystl::vector<uint32_t>                                  constantSizes;
	tinystl::vector<tinystl::pair<DescriptorInfo*, Sampler*> > staticSamplers;
	ShaderStage                                                shaderStages = SHADER_STAGE_NONE;
	bool                                                       useInputLayout = false;

	tinystl::unordered_map<tinystl::string, Sampler*> staticSamplerMap;
	for (uint32_t i = 0; i < pRootSignatureDesc->mStaticSamplerCount; ++i)
		staticSamplerMap.insert({ pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i] });

	//pRootSignature->pDescriptorNameToIndexMap;
	conf_placement_new<tinystl::unordered_map<uint32_t, uint32_t> >(&pRootSignature->pDescriptorNameToIndexMap);

	// Collect all unique shader resources in the given shaders
	// Resources are parsed by name (two resources named "XYZ" in two shaders will be considered the same resource)
	for (uint32_t sh = 0; sh < pRootSignatureDesc->mShaderCount; ++sh)
	{
		PipelineReflection const* pReflection = &pRootSignatureDesc->ppShaders[sh]->mReflection;

		// KEep track of the used pipeline stages
		shaderStages |= pReflection->mShaderStages;

		if (pReflection->mShaderStages & SHADER_STAGE_COMP)
			pRootSignature->mPipelineType = PIPELINE_TYPE_COMPUTE;
		else
			pRootSignature->mPipelineType = PIPELINE_TYPE_GRAPHICS;

		if (pReflection->mShaderStages & SHADER_STAGE_VERT)
		{
			if (pReflection->mStageReflections[pReflection->mVertexStageIndex].mVertexInputsCount)
			{
				useInputLayout = true;
			}
		}
		for (uint32_t i = 0; i < pReflection->mShaderResourceCount; ++i)
		{
			ShaderResource const* pRes = &pReflection->pShaderResources[i];
			uint32_t              setIndex = pRes->set;

			// If the size of the resource is zero, assume its a bindless resource
			// All bindless resources will go in the static descriptor table
			if (pRes->size == 0)
				setIndex = 0;

			// Find all unique resources
			tinystl::unordered_hash_node<uint32_t, uint32_t>* pNode =
				pRootSignature->pDescriptorNameToIndexMap.find(tinystl::hash(pRes->name)).node;
			if (!pNode)
			{
				pRootSignature->pDescriptorNameToIndexMap.insert({ tinystl::hash(pRes->name), (uint32_t)shaderResources.size() });
				shaderResources.push_back(*pRes);

				uint32_t constantSize = 0;

				if (pRes->type == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
				{
					for (uint32_t v = 0; v < pReflection->mVariableCount; ++v)
					{
						if (pReflection->pVariables[v].parent_index == i)
							constantSize += pReflection->pVariables[v].size;
					}
				}

				//shaderStages |= pRes->used_stages;
				constantSizes.push_back(constantSize);
			}
			// If the resource was already collected, just update the shader stage mask in case it is used in a different
			// shader stage in this case
			else
			{
				if (shaderResources[pNode->second].reg != pRes->reg)
				{
					ErrorMsg(
						"\nFailed to create root signature\n"
						"Shared shader resource %s has mismatching register. All shader resources "
						"shared by multiple shaders specified in addRootSignature "
						"have the same register and space",
						pRes->name);
					return;
				}
				if (shaderResources[pNode->second].set != pRes->set)
				{
					ErrorMsg(
						"\nFailed to create root signature\n"
						"Shared shader resource %s has mismatching space. All shader resources "
						"shared by multiple shaders specified in addRootSignature "
						"have the same register and space",
						pRes->name);
					return;
				}

				for (ShaderResource& res : shaderResources)
				{
					if (tinystl::hash(res.name) == pNode->first)
					{
						res.used_stages |= pRes->used_stages;
						break;
					}
				}
			}
		}
	}

	if ((uint32_t)shaderResources.size())
	{
		pRootSignature->mDescriptorCount = (uint32_t)shaderResources.size();
		pRootSignature->pDescriptors = (DescriptorInfo*)conf_calloc(pRootSignature->mDescriptorCount, sizeof(DescriptorInfo));
	}

	// Fill the descriptor array to be stored in the root signature
	for (uint32_t i = 0; i < (uint32_t)shaderResources.size(); ++i)
	{
		DescriptorInfo* pDesc = &pRootSignature->pDescriptors[i];
		ShaderResource* pRes = &shaderResources[i];
		uint32_t        setIndex = pRes->set;
		if (pRes->size == 0)
			setIndex = 0;

		DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)setIndex;

		pDesc->mDesc.reg = pRes->reg;
		pDesc->mDesc.set = pRes->set;
		pDesc->mDesc.size = pRes->size;
		pDesc->mDesc.type = pRes->type;
		pDesc->mDesc.used_stages = pRes->used_stages;
		pDesc->mUpdateFrquency = updateFreq;

		pDesc->mDesc.name_size = pRes->name_size;
		pDesc->mDesc.name = (const char*)conf_calloc(pDesc->mDesc.name_size + 1, sizeof(char));
		memcpy((char*)pDesc->mDesc.name, pRes->name, pRes->name_size);

		if (pDesc->mDesc.size == 0 && pDesc->mDesc.type == DESCRIPTOR_TYPE_TEXTURE)
		{
			pDesc->mDesc.size = pRootSignatureDesc->mMaxBindlessTextures;
		}

		// Find the D3D12 type of the descriptors
		if (pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER)
		{
			// If the sampler is a static sampler, no need to put it in the descriptor table
			const tinystl::unordered_hash_node<tinystl::string, Sampler*>* pNode = staticSamplerMap.find(pDesc->mDesc.name).node;

			if (pNode)
			{
				LOGINFOF("Descriptor (%s) : User specified Static Sampler", pDesc->mDesc.name);
				// Set the index to invalid value so we can use this later for error checking if user tries to update a static sampler
				pDesc->mIndexInParent = ~0u;
				staticSamplers.push_back({ pDesc, pNode->second });
			}
			else
			{
				// In D3D12, sampler descriptors cannot be placed in a table containing view descriptors
				layouts[setIndex].mSamplerTable.emplace_back(pDesc);
			}
		}
		// No support for arrays of constant buffers to be used as root descriptors as this might bloat the root signature size
		else if (pDesc->mDesc.type == DESCRIPTOR_TYPE_UNIFORM_BUFFER && pDesc->mDesc.size == 1)
		{
			// D3D12 has no special syntax to declare root constants like Vulkan
			// So we assume that all constant buffers with the word "rootconstant" (case insensitive) are root constants
			if (tinystl::string(pRes->name).to_lower().find("rootconstant", 0) != tinystl::string::npos ||
				pDesc->mDesc.type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
			{
				// Make the root param a 32 bit constant if the user explicitly specifies it in the shader
				pDesc->mDxType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
				pDesc->mDesc.type = DESCRIPTOR_TYPE_ROOT_CONSTANT;
				layouts[setIndex].mRootConstants.emplace_back(pDesc);

				pDesc->mDesc.size = constantSizes[i] / sizeof(uint32_t);
			}
			else
			{
				// By default DESCRIPTOR_TYPE_UNIFORM_BUFFER maps to D3D12_ROOT_PARAMETER_TYPE_CBV
				// But since the size of root descriptors is 2 DWORDS, some of these uniform buffers might get placed in descriptor tables
				// if the size of the root signature goes over the max recommended size on the specific hardware
				layouts[setIndex].mConstantParams.emplace_back(pDesc);
				pDesc->mDxType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			}
		}
		else
		{
			layouts[setIndex].mCbvSrvUavTable.emplace_back(pDesc);
		}

		layouts[setIndex].mDescriptorIndexMap[pDesc] = i;
	}

	uint32_t rootSize = calculate_root_signature_size(layouts.data(), (uint32_t)layouts.size());

	// If the root signature size has crossed the recommended hardware limit try to optimize it
	if (rootSize > pRenderer->pActiveGpuSettings->mMaxRootSignatureDWORDS)
	{
		// Cconvert some of the root constants to root descriptors
		for (uint32_t i = 0; i < (uint32_t)layouts.size(); ++i)
		{
			if (!layouts[i].mRootConstants.size())
				continue;

			UpdateFrequencyLayoutInfo& layout = layouts[i];
			DescriptorInfo**           convertIt = layout.mRootConstants.end() - 1;
			DescriptorInfo**           endIt = layout.mRootConstants.begin();
			while (layout.mRootConstants.size() &&
				   pRenderer->pActiveGpuSettings->mMaxRootSignatureDWORDS <
					   calculate_root_signature_size(layouts.data(), (uint32_t)layouts.size()) &&
				   convertIt >= endIt)
			{
				layout.mConstantParams.push_back(*convertIt);
				layout.mRootConstants.erase(layout.mRootConstants.find(*convertIt));
				(*convertIt)->mDxType = D3D12_ROOT_PARAMETER_TYPE_CBV;

				LOGWARNINGF(
					"Converting root constant (%s) to root cbv to keep root signature size below hardware limit", (*convertIt)->mDesc.name);
			}
		}

		// If the root signature size is still above the recommended max, we need to place some of the less updated root descriptors
		// in descriptor tables of the same update frequency
		for (uint32_t i = 0; i < (uint32_t)layouts.size(); ++i)
		{
			if (!layouts[i].mConstantParams.size())
				continue;

			UpdateFrequencyLayoutInfo& layout = layouts[i];

			while (pRenderer->pActiveGpuSettings->mMaxRootSignatureDWORDS <
				   calculate_root_signature_size(layouts.data(), (uint32_t)layouts.size()))
			{
				if (!layout.mConstantParams.size())
					break;
				DescriptorInfo** constantIt = layout.mConstantParams.end() - 1;
				DescriptorInfo** endIt = layout.mConstantParams.begin();
				if ((constantIt != endIt) || (constantIt == layout.mConstantParams.begin() && endIt == layout.mConstantParams.begin()))
				{
					layout.mCbvSrvUavTable.push_back(*constantIt);
					layout.mConstantParams.erase(layout.mConstantParams.find(*constantIt));

					LOGWARNINGF(
						"Placing root descriptor (%s) in descriptor table to keep root signature size below hardware limit",
						(*constantIt)->mDesc.name);
				}
			}
		}
	}

	// We should never reach inside this if statement. If we do, something got messed up
	if (pRenderer->pActiveGpuSettings->mMaxRootSignatureDWORDS < calculate_root_signature_size(layouts.data(), (uint32_t)layouts.size()))
	{
		LOGWARNING("Root Signature size greater than the specified max size");
		ASSERT(false);
	}

	// D3D12 currently has two versions of root signatures (1_0, 1_1)
	// So we fill the structs of both versions and in the end use the structs compatible with the supported version
	tinystl::vector<tinystl::vector<D3D12_DESCRIPTOR_RANGE1> > cbvSrvUavRange((uint32_t)layouts.size());
	tinystl::vector<tinystl::vector<D3D12_DESCRIPTOR_RANGE1> > samplerRange((uint32_t)layouts.size());
	tinystl::vector<D3D12_ROOT_PARAMETER1>                     rootParams;

	tinystl::vector<tinystl::vector<D3D12_DESCRIPTOR_RANGE> > cbvSrvUavRange_1_0((uint32_t)layouts.size());
	tinystl::vector<tinystl::vector<D3D12_DESCRIPTOR_RANGE> > samplerRange_1_0((uint32_t)layouts.size());
	tinystl::vector<D3D12_ROOT_PARAMETER>                     rootParams_1_0;

	tinystl::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplerDescs(staticSamplers.size());
	for (uint32_t i = 0; i < (uint32_t)staticSamplers.size(); ++i)
	{
		staticSamplerDescs[i].Filter = staticSamplers[i].second->mDxSamplerDesc.Filter;
		staticSamplerDescs[i].AddressU = staticSamplers[i].second->mDxSamplerDesc.AddressU;
		staticSamplerDescs[i].AddressV = staticSamplers[i].second->mDxSamplerDesc.AddressV;
		staticSamplerDescs[i].AddressW = staticSamplers[i].second->mDxSamplerDesc.AddressW;
		staticSamplerDescs[i].MipLODBias = staticSamplers[i].second->mDxSamplerDesc.MipLODBias;
		staticSamplerDescs[i].MaxAnisotropy = staticSamplers[i].second->mDxSamplerDesc.MaxAnisotropy;
		staticSamplerDescs[i].ComparisonFunc = staticSamplers[i].second->mDxSamplerDesc.ComparisonFunc;
		staticSamplerDescs[i].MinLOD = staticSamplers[i].second->mDxSamplerDesc.MinLOD;
		staticSamplerDescs[i].MaxLOD = staticSamplers[i].second->mDxSamplerDesc.MaxLOD;
		staticSamplerDescs[i].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		staticSamplerDescs[i].RegisterSpace = staticSamplers[i].first->mDesc.set;
		staticSamplerDescs[i].ShaderRegister = staticSamplers[i].first->mDesc.reg;
		staticSamplerDescs[i].ShaderVisibility = util_to_dx_shader_visibility(staticSamplers[i].first->mDesc.used_stages);
	}

	for (uint32_t i = 0; i < (uint32_t)layouts.size(); ++i)
	{
		cbvSrvUavRange[i].resize(layouts[i].mCbvSrvUavTable.size());
		cbvSrvUavRange_1_0[i].resize(layouts[i].mCbvSrvUavTable.size());

		samplerRange[i].resize(layouts[i].mSamplerTable.size());
		samplerRange_1_0[i].resize(layouts[i].mSamplerTable.size());
	}

	pRootSignature->mDescriptorCount = (uint32_t)shaderResources.size();

	for (uint32_t i = 0; i < (uint32_t)layouts.size(); ++i)
	{
		pRootSignature->mDxRootConstantCount += (uint32_t)layouts[i].mRootConstants.size();
		pRootSignature->mDxRootDescriptorCount += (uint32_t)layouts[i].mConstantParams.size();
	}
	if (pRootSignature->mDxRootConstantCount)
		pRootSignature->pDxRootConstantRootIndices =
			(uint32_t*)conf_calloc(pRootSignature->mDxRootConstantCount, sizeof(*pRootSignature->pDxRootConstantRootIndices));
	if (pRootSignature->mDxRootDescriptorCount)
		pRootSignature->pDxRootDescriptorRootIndices =
			(uint32_t*)conf_calloc(pRootSignature->mDxRootDescriptorCount, sizeof(*pRootSignature->pDxRootDescriptorRootIndices));

	// Start collecting root parameters
	// Start with root descriptors since they will be the most frequently updated descriptors
	// This also makes sure that if we spill, the root descriptors in the front of the root signature will most likely still remain in the root
	uint32_t rootDescriptorIndex = 0;
	// Collect all root descriptors
	// Put most frequently changed params first
	for (uint32_t i = (uint32_t)layouts.size(); i-- > 0U;)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[i];
		if (layout.mConstantParams.size())
		{
			for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mConstantParams.size(); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mConstantParams[descIndex];
				pDesc->mIndexInParent = rootDescriptorIndex;
				pRootSignature->pDxRootDescriptorRootIndices[pDesc->mIndexInParent] = (uint32_t)rootParams.size();

				D3D12_ROOT_PARAMETER1 rootParam;
				D3D12_ROOT_PARAMETER  rootParam_1_0;
				create_root_descriptor(pDesc, &rootParam);
				create_root_descriptor_1_0(pDesc, &rootParam_1_0);

				rootParams.push_back(rootParam);
				rootParams_1_0.push_back(rootParam_1_0);

				++rootDescriptorIndex;
			}
		}
	}

	uint32_t rootConstantIndex = 0;

	// Collect all root constants
	for (uint32_t setIndex = 0; setIndex < (uint32_t)layouts.size(); ++setIndex)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[setIndex];

		if (!layout.mRootConstants.size())
			continue;

		for (uint32_t i = 0; i < (uint32_t)layouts[setIndex].mRootConstants.size(); ++i)
		{
			DescriptorInfo* pDesc = layout.mRootConstants[i];
			pDesc->mIndexInParent = rootConstantIndex;
			pRootSignature->pDxRootConstantRootIndices[pDesc->mIndexInParent] = (uint32_t)rootParams.size();

			D3D12_ROOT_PARAMETER1 rootParam;
			D3D12_ROOT_PARAMETER  rootParam_1_0;
			create_root_constant(pDesc, &rootParam);
			create_root_constant_1_0(pDesc, &rootParam_1_0);

			rootParams.push_back(rootParam);
			rootParams_1_0.push_back(rootParam_1_0);

			if (pDesc->mDesc.size > gMaxRootConstantsPerRootParam)
			{
				//64 DWORDS for NVIDIA, 16 for AMD but 3 are used by driver so we get 13 SGPR
				//DirectX12
				//Root descriptors - 2
				//Root constants - Number of 32 bit constants
				//Descriptor tables - 1
				//Static samplers - 0
				LOGINFOF(
					"Root constant (%s) has (%u) 32 bit values. It is recommended to have root constant number less or equal than 13",
					pDesc->mDesc.name, pDesc->mDesc.size);
			}

			++rootConstantIndex;
		}
	}

	// Collect descriptor table parameters
	// Put most frequently changed descriptor tables in the front of the root signature
	for (uint32_t i = (uint32_t)layouts.size(); i-- > 0U;)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[i];

		// Fill the descriptor table layout for the view descriptor table of this update frequency
		if (layout.mCbvSrvUavTable.size())
		{
			// sort table by type (CBV/SRV/UAV) by register by space
			layout.mCbvSrvUavTable.sort(
				[](DescriptorInfo* const& lhs, DescriptorInfo* const& rhs) { return (int)(lhs->mDesc.reg - rhs->mDesc.reg); });
			layout.mCbvSrvUavTable.sort(
				[](DescriptorInfo* const& lhs, DescriptorInfo* const& rhs) { return (int)(lhs->mDesc.set - rhs->mDesc.set); });
			layout.mCbvSrvUavTable.sort(
				[](DescriptorInfo* const& lhs, DescriptorInfo* const& rhs) { return (int)(lhs->mDesc.type - rhs->mDesc.type); });

			D3D12_ROOT_PARAMETER1 rootParam;
			create_descriptor_table(
				(uint32_t)layout.mCbvSrvUavTable.size(), layout.mCbvSrvUavTable.data(), cbvSrvUavRange[i].data(), &rootParam);

			D3D12_ROOT_PARAMETER rootParam_1_0;
			create_descriptor_table_1_0(
				(uint32_t)layout.mCbvSrvUavTable.size(), layout.mCbvSrvUavTable.data(), cbvSrvUavRange_1_0[i].data(), &rootParam_1_0);

			// Store some of the binding info which will be required later when binding the descriptor table
			// We need the root index when calling SetRootDescriptorTable
			pRootSignature->mDxViewDescriptorTableRootIndices[i] = (uint32_t)rootParams.size();
			pRootSignature->mDxViewDescriptorCounts[i] = (uint32_t)layout.mCbvSrvUavTable.size();
			pRootSignature->pDxViewDescriptorIndices[i] = (uint32_t*)conf_calloc(layout.mCbvSrvUavTable.size(), sizeof(uint32_t));

			for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mCbvSrvUavTable.size(); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mCbvSrvUavTable[descIndex];
				pDesc->mIndexInParent = descIndex;

				// Store the d3d12 related info in the descriptor to avoid constantly calling the util_to_dx mapping functions
				pDesc->mDxType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				pDesc->mHandleIndex = pRootSignature->mDxCumulativeViewDescriptorCounts[i];

				// Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
				// This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays) in the descriptor table
				pRootSignature->mDxCumulativeViewDescriptorCounts[i] += pDesc->mDesc.size;
				pRootSignature->pDxViewDescriptorIndices[i][descIndex] = layout.mDescriptorIndexMap[pDesc];
			}

			rootParams.push_back(rootParam);
			rootParams_1_0.push_back(rootParam_1_0);
		}

		// Fill the descriptor table layout for the sampler descriptor table of this update frequency
		if (layout.mSamplerTable.size())
		{
			D3D12_ROOT_PARAMETER1 rootParam;
			create_descriptor_table((uint32_t)layout.mSamplerTable.size(), layout.mSamplerTable.data(), samplerRange[i].data(), &rootParam);

			D3D12_ROOT_PARAMETER rootParam_1_0;
			create_descriptor_table_1_0(
				(uint32_t)layout.mSamplerTable.size(), layout.mSamplerTable.data(), samplerRange_1_0[i].data(), &rootParam_1_0);

			// Store some of the binding info which will be required later when binding the descriptor table
			// We need the root index when calling SetRootDescriptorTable
			pRootSignature->mDxSamplerDescriptorTableRootIndices[i] = (uint32_t)rootParams.size();
			pRootSignature->mDxSamplerDescriptorCounts[i] = (uint32_t)layout.mSamplerTable.size();
			pRootSignature->pDxSamplerDescriptorIndices[i] = (uint32_t*)conf_calloc(layout.mSamplerTable.size(), sizeof(uint32_t));
			//table.pDescriptorIndices = (uint32_t*)conf_calloc(table.mDescriptorCount, sizeof(uint32_t));

			for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mSamplerTable.size(); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mSamplerTable[descIndex];
				pDesc->mIndexInParent = descIndex;

				// Store the d3d12 related info in the descriptor to avoid constantly calling the util_to_dx mapping functions
				pDesc->mDxType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				pDesc->mHandleIndex = pRootSignature->mDxCumulativeSamplerDescriptorCounts[i];

				// Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
				// This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays) in the descriptor table
				pRootSignature->mDxCumulativeSamplerDescriptorCounts[i] += pDesc->mDesc.size;
				pRootSignature->pDxSamplerDescriptorIndices[i][descIndex] = layout.mDescriptorIndexMap[pDesc];
			}

			rootParams.push_back(rootParam);
			rootParams_1_0.push_back(rootParam_1_0);
		}
	}

	DECLARE_ZERO(D3D12_FEATURE_DATA_ROOT_SIGNATURE, feature_data);
	feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	HRESULT hres = pRenderer->pDxDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data));

	if (FAILED(hres))
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

	// Specify the deny flags to avoid unnecessary shader stages being notified about descriptor modifications
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	if (useInputLayout)
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	if (!(shaderStages & SHADER_STAGE_VERT))
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
	if (!(shaderStages & SHADER_STAGE_HULL))
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
	if (!(shaderStages & SHADER_STAGE_DOMN))
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
	if (!(shaderStages & SHADER_STAGE_GEOM))
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
	if (!(shaderStages & SHADER_STAGE_FRAG))
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	ID3DBlob* error_msgs = NULL;
#ifdef _DURANGO
	DECLARE_ZERO(D3D12_ROOT_SIGNATURE_DESC, desc);
	CD3DX12_ROOT_SIGNATURE_DESC::Init(
		desc, (UINT)rootParams_1_0.size(), rootParams_1_0.empty() == false ? rootParams_1_0.data() : NULL, (UINT)staticSamplerDescs.size(),
		staticSamplerDescs.data(), rootSignatureFlags);

	hres = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pRootSignature->pDxSerializedRootSignatureString, &error_msgs);
#else
	DECLARE_ZERO(D3D12_VERSIONED_ROOT_SIGNATURE_DESC, desc);

	if (D3D_ROOT_SIGNATURE_VERSION_1_1 == feature_data.HighestVersion)
	{
		desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		desc.Desc_1_1.NumParameters = (uint32_t)rootParams.size();
		desc.Desc_1_1.pParameters = rootParams.data();
		desc.Desc_1_1.NumStaticSamplers = (UINT)staticSamplerDescs.size();
		desc.Desc_1_1.pStaticSamplers = staticSamplerDescs.data();
		desc.Desc_1_1.Flags = rootSignatureFlags;
	}
	else
	{
		desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
		desc.Desc_1_0.NumParameters = (uint32_t)rootParams_1_0.size();
		desc.Desc_1_0.pParameters = rootParams_1_0.data();
		desc.Desc_1_0.NumStaticSamplers = (UINT)staticSamplerDescs.size();
		desc.Desc_1_0.pStaticSamplers = staticSamplerDescs.data();
		desc.Desc_1_0.Flags = rootSignatureFlags;
	}

	hres = D3D12SerializeVersionedRootSignature(&desc, &pRootSignature->pDxSerializedRootSignatureString, &error_msgs);
#endif

	if (!SUCCEEDED(hres))
	{
		char* pMsg = (char*)conf_calloc(error_msgs->GetBufferSize(), sizeof(char));
		memcpy(pMsg, error_msgs->GetBufferPointer(), error_msgs->GetBufferSize());
		LOGERRORF("Failed to serialize root signature with error (%s)", pMsg);
		conf_free(pMsg);
	}

	// If running Linked Mode (SLI) create root signature for all nodes
	// #NOTE : In non SLI mode, mNodeCount will be 0 which sets nodeMask to default value
	hres = pRenderer->pDxDevice->CreateRootSignature(
		util_calculate_shared_node_mask(pRenderer), pRootSignature->pDxSerializedRootSignatureString->GetBufferPointer(),
		pRootSignature->pDxSerializedRootSignatureString->GetBufferSize(), IID_ARGS(&pRootSignature->pDxRootSignature));
	ASSERT(SUCCEEDED(hres));

	SAFE_RELEASE(error_msgs);

	conf_placement_new<RootSignature::ThreadLocalDescriptorManager>(&pRootSignature->pDescriptorManagerMap);
	// Create descriptor manager for this thread
	DescriptorManager* pManager = NULL;
	add_descriptor_manager(pRenderer, pRootSignature, &pManager);
	pRootSignature->pDescriptorManagerMap.insert({ Thread::GetCurrentThreadID(), pManager });

	*ppRootSignature = pRootSignature;
}

void removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
	for (tinystl::unordered_hash_node<ThreadID, DescriptorManager*>& it : pRootSignature->pDescriptorManagerMap)
	{
		remove_descriptor_manager(pRenderer, pRootSignature, it.second);
	}

	for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
	{
		SAFE_FREE(pRootSignature->pDxViewDescriptorIndices[i]);
		SAFE_FREE(pRootSignature->pDxSamplerDescriptorIndices[i]);
	}

	for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
	{
		SAFE_FREE((void*)pRootSignature->pDescriptors[i].mDesc.name);
	}

	pRootSignature->pDescriptorManagerMap.~unordered_map();
	pRootSignature->pDescriptorNameToIndexMap.~unordered_map();

	SAFE_FREE(pRootSignature->pDescriptors);
	SAFE_FREE(pRootSignature->pDxRootDescriptorRootIndices);
	SAFE_FREE(pRootSignature->pDxRootConstantRootIndices);

	SAFE_RELEASE(pRootSignature->pDxRootSignature);
	SAFE_RELEASE(pRootSignature->pDxSerializedRootSignatureString);

	SAFE_FREE(pRootSignature);
}
/************************************************************************/
// Pipeline State Functions
/************************************************************************/
void addPipeline(Renderer* pRenderer, const GraphicsPipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);

	//allocate new pipeline
	Pipeline* pPipeline = (Pipeline*)conf_calloc(1, sizeof(*pPipeline));
	ASSERT(pPipeline);

	const Shader*       pShaderProgram = pDesc->pShaderProgram;
	const VertexLayout* pVertexLayout = pDesc->pVertexLayout;

	//copy the given pipeline settings into new pipeline
	memcpy(&(pPipeline->mGraphics), pDesc, sizeof(*pDesc));
	pPipeline->mType = PIPELINE_TYPE_GRAPHICS;

	//add to gpu
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, VS);
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, PS);
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, DS);
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, HS);
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, GS);
	if (pShaderProgram->mStages & SHADER_STAGE_VERT)
	{
		VS.BytecodeLength = pShaderProgram->pShaderBlobs[pShaderProgram->mReflection.mVertexStageIndex]->GetBufferSize();
		VS.pShaderBytecode = pShaderProgram->pShaderBlobs[pShaderProgram->mReflection.mVertexStageIndex]->GetBufferPointer();
	}
	if (pShaderProgram->mStages & SHADER_STAGE_FRAG)
	{
		PS.BytecodeLength = pShaderProgram->pShaderBlobs[pShaderProgram->mReflection.mPixelStageIndex]->GetBufferSize();
		PS.pShaderBytecode = pShaderProgram->pShaderBlobs[pShaderProgram->mReflection.mPixelStageIndex]->GetBufferPointer();
	}
	if (pShaderProgram->mStages & SHADER_STAGE_HULL)
	{
		HS.BytecodeLength = pShaderProgram->pShaderBlobs[pShaderProgram->mReflection.mHullStageIndex]->GetBufferSize();
		HS.pShaderBytecode = pShaderProgram->pShaderBlobs[pShaderProgram->mReflection.mHullStageIndex]->GetBufferPointer();
	}
	if (pShaderProgram->mStages & SHADER_STAGE_DOMN)
	{
		DS.BytecodeLength = pShaderProgram->pShaderBlobs[pShaderProgram->mReflection.mDomainStageIndex]->GetBufferSize();
		DS.pShaderBytecode = pShaderProgram->pShaderBlobs[pShaderProgram->mReflection.mDomainStageIndex]->GetBufferPointer();
	}
	if (pShaderProgram->mStages & SHADER_STAGE_GEOM)
	{
		GS.BytecodeLength = pShaderProgram->pShaderBlobs[pShaderProgram->mReflection.mGeometryStageIndex]->GetBufferSize();
		GS.pShaderBytecode = pShaderProgram->pShaderBlobs[pShaderProgram->mReflection.mGeometryStageIndex]->GetBufferPointer();
	}

	DECLARE_ZERO(D3D12_STREAM_OUTPUT_DESC, stream_output_desc);
	stream_output_desc.pSODeclaration = NULL;
	stream_output_desc.NumEntries = 0;
	stream_output_desc.pBufferStrides = NULL;
	stream_output_desc.NumStrides = 0;
	stream_output_desc.RasterizedStream = 0;

	DECLARE_ZERO(D3D12_DEPTH_STENCILOP_DESC, depth_stencilop_desc);
	depth_stencilop_desc.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depth_stencilop_desc.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depth_stencilop_desc.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depth_stencilop_desc.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	uint32_t input_elementCount = 0;
	DECLARE_ZERO(D3D12_INPUT_ELEMENT_DESC, input_elements[MAX_VERTEX_ATTRIBS]);

	// Make sure there's attributes
	if (pVertexLayout != NULL)
	{
		DECLARE_ZERO(char, semantic_names[MAX_VERTEX_ATTRIBS][MAX_SEMANTIC_NAME_LENGTH]);

		//uint32_t attrib_count = min(pVertexLayout->mAttribCount, MAX_VERTEX_ATTRIBS);  //Not used
		for (uint32_t attrib_index = 0; attrib_index < pVertexLayout->mAttribCount; ++attrib_index)
		{
			const VertexAttrib* attrib = &(pVertexLayout->mAttribs[attrib_index]);

#ifdef FORGE_JHABLE_EDITS_V01
			input_elements[input_elementCount].SemanticName = g_hackSemanticList[attrib->mSemanticType];
			input_elements[input_elementCount].SemanticIndex = attrib->mSemanticIndex;

			if (attrib->mSemanticNameLength > 0)
			{
				uint32_t name_length = min(MAX_SEMANTIC_NAME_LENGTH, attrib->mSemanticNameLength);
				strncpy_s(semantic_names[attrib_index], attrib->mSemanticName, name_length);
			}

#else
			ASSERT(SEMANTIC_UNDEFINED != attrib->mSemantic);

			if (attrib->mSemanticNameLength > 0)
			{
				uint32_t name_length = min(MAX_SEMANTIC_NAME_LENGTH, attrib->mSemanticNameLength);
				strncpy_s(semantic_names[attrib_index], attrib->mSemanticName, name_length);
			}
			else
			{
				DECLARE_ZERO(char, name[MAX_SEMANTIC_NAME_LENGTH]);
				switch (attrib->mSemantic)
				{
					case SEMANTIC_POSITION: sprintf_s(name, "POSITION"); break;
					case SEMANTIC_NORMAL: sprintf_s(name, "NORMAL"); break;
					case SEMANTIC_COLOR: sprintf_s(name, "COLOR"); break;
					case SEMANTIC_TANGENT: sprintf_s(name, "TANGENT"); break;
					case SEMANTIC_BITANGENT: sprintf_s(name, "BINORMAL"); break;
					case SEMANTIC_TEXCOORD0: sprintf_s(name, "TEXCOORD"); break;
					case SEMANTIC_TEXCOORD1: sprintf_s(name, "TEXCOORD"); break;
					case SEMANTIC_TEXCOORD2: sprintf_s(name, "TEXCOORD"); break;
					case SEMANTIC_TEXCOORD3: sprintf_s(name, "TEXCOORD"); break;
					case SEMANTIC_TEXCOORD4: sprintf_s(name, "TEXCOORD"); break;
					case SEMANTIC_TEXCOORD5: sprintf_s(name, "TEXCOORD"); break;
					case SEMANTIC_TEXCOORD6: sprintf_s(name, "TEXCOORD"); break;
					case SEMANTIC_TEXCOORD7: sprintf_s(name, "TEXCOORD"); break;
					case SEMANTIC_TEXCOORD8: sprintf_s(name, "TEXCOORD"); break;
					case SEMANTIC_TEXCOORD9: sprintf_s(name, "TEXCOORD"); break;
					default: break;
				}
				ASSERT(0 != strlen(name));
				strncpy_s(semantic_names[attrib_index], name, strlen(name));
			}

			UINT semantic_index = 0;
			switch (attrib->mSemantic)
			{
				case SEMANTIC_TEXCOORD0: semantic_index = 0; break;
				case SEMANTIC_TEXCOORD1: semantic_index = 1; break;
				case SEMANTIC_TEXCOORD2: semantic_index = 2; break;
				case SEMANTIC_TEXCOORD3: semantic_index = 3; break;
				case SEMANTIC_TEXCOORD4: semantic_index = 4; break;
				case SEMANTIC_TEXCOORD5: semantic_index = 5; break;
				case SEMANTIC_TEXCOORD6: semantic_index = 6; break;
				case SEMANTIC_TEXCOORD7: semantic_index = 7; break;
				case SEMANTIC_TEXCOORD8: semantic_index = 8; break;
				case SEMANTIC_TEXCOORD9: semantic_index = 9; break;
				default: break;
			}

			input_elements[input_elementCount].SemanticName = semantic_names[attrib_index];
			input_elements[input_elementCount].SemanticIndex = semantic_index;
#endif
			input_elements[input_elementCount].Format = util_to_dx_image_format(attrib->mFormat, false);
			input_elements[input_elementCount].InputSlot = attrib->mBinding;
			input_elements[input_elementCount].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
			if (attrib->mRate == VERTEX_ATTRIB_RATE_INSTANCE)
			{
				input_elements[input_elementCount].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
				input_elements[input_elementCount].InstanceDataStepRate = 1;
			}
			else
			{
				input_elements[input_elementCount].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
				input_elements[input_elementCount].InstanceDataStepRate = 0;
			}
			++input_elementCount;
		}
	}

	DECLARE_ZERO(D3D12_INPUT_LAYOUT_DESC, input_layout_desc);
	input_layout_desc.pInputElementDescs = input_elementCount ? input_elements : NULL;
	input_layout_desc.NumElements = input_elementCount;

	uint32_t render_target_count = min(pDesc->mRenderTargetCount, MAX_RENDER_TARGET_ATTACHMENTS);
	render_target_count = min(render_target_count, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

	DECLARE_ZERO(DXGI_SAMPLE_DESC, sample_desc);
	sample_desc.Count = (UINT)(pDesc->mSampleCount);
	sample_desc.Quality = (UINT)(pDesc->mSampleQuality);

	DECLARE_ZERO(D3D12_CACHED_PIPELINE_STATE, cached_pso_desc);
	cached_pso_desc.pCachedBlob = NULL;
	cached_pso_desc.CachedBlobSizeInBytes = 0;

	DECLARE_ZERO(D3D12_GRAPHICS_PIPELINE_STATE_DESC, pipeline_state_desc);
	pipeline_state_desc.pRootSignature = pDesc->pRootSignature->pDxRootSignature;
	pipeline_state_desc.VS = VS;
	pipeline_state_desc.PS = PS;
	pipeline_state_desc.DS = DS;
	pipeline_state_desc.HS = HS;
	pipeline_state_desc.GS = GS;
	pipeline_state_desc.StreamOutput = stream_output_desc;
	pipeline_state_desc.BlendState =
		pDesc->pBlendState != NULL ? pDesc->pBlendState->mDxBlendDesc : pRenderer->pDefaultBlendState->mDxBlendDesc;

	pipeline_state_desc.SampleMask = UINT_MAX;

	pipeline_state_desc.RasterizerState = pDesc->pRasterizerState != NULL ? pDesc->pRasterizerState->mDxRasterizerDesc
																		  : pRenderer->pDefaultRasterizerState->mDxRasterizerDesc;

	pipeline_state_desc.DepthStencilState =
		pDesc->pDepthState != NULL ? pDesc->pDepthState->mDxDepthStencilDesc : pRenderer->pDefaultDepthState->mDxDepthStencilDesc;

	pipeline_state_desc.InputLayout = input_layout_desc;
	pipeline_state_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	pipeline_state_desc.PrimitiveTopologyType = util_to_dx_primitive_topology_type(pDesc->mPrimitiveTopo);
	pipeline_state_desc.NumRenderTargets = render_target_count;
	pipeline_state_desc.DSVFormat = util_to_dx_image_format(pDesc->mDepthStencilFormat, false);

	pipeline_state_desc.SampleDesc = sample_desc;
	pipeline_state_desc.CachedPSO = cached_pso_desc;
	pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	for (uint32_t attrib_index = 0; attrib_index < render_target_count; ++attrib_index)
	{
		pipeline_state_desc.RTVFormats[attrib_index] =
			util_to_dx_image_format(pDesc->pColorFormats[attrib_index], pDesc->pSrgbValues[attrib_index]);
	}

	// If running Linked Mode (SLI) create pipeline for all nodes
	// #NOTE : In non SLI mode, mNodeCount will be 0 which sets nodeMask to default value
	pipeline_state_desc.NodeMask = util_calculate_shared_node_mask(pRenderer);

	HRESULT hres = pRenderer->pDxDevice->CreateGraphicsPipelineState(
		&pipeline_state_desc, __uuidof(pPipeline->pDxPipelineState), (void**)&(pPipeline->pDxPipelineState));
	ASSERT(SUCCEEDED(hres));

	D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	switch (pPipeline->mGraphics.mPrimitiveTopo)
	{
		case PRIMITIVE_TOPO_POINT_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
		case PRIMITIVE_TOPO_LINE_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
		case PRIMITIVE_TOPO_LINE_STRIP: topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
		case PRIMITIVE_TOPO_TRI_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
		case PRIMITIVE_TOPO_TRI_STRIP: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
		case PRIMITIVE_TOPO_PATCH_LIST:
		{
			const PipelineReflection* pReflection = &pPipeline->mGraphics.pShaderProgram->mReflection;
			uint32_t                  controlPoint = pReflection->mStageReflections[pReflection->mHullStageIndex].mNumControlPoint;
			topology = (D3D_PRIMITIVE_TOPOLOGY)(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (controlPoint - 1));
		}
		break;

		default: break;
	}
	ASSERT(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED != topology);
	pPipeline->mDxPrimitiveTopology = topology;

	//save new pipeline in given pointer
	*ppPipeline = pPipeline;
}

void addComputePipeline(Renderer* pRenderer, const ComputePipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);
	ASSERT(pDesc->pShaderProgram->pShaderBlobs[0]);

	//allocate new pipeline
	Pipeline* pPipeline = (Pipeline*)conf_calloc(1, sizeof(*pPipeline));
	ASSERT(pPipeline);

	//copy pipeline settings
	memcpy(&(pPipeline->mCompute), pDesc, sizeof(*pDesc));
	pPipeline->mType = PIPELINE_TYPE_COMPUTE;

	//add pipeline specifying its for compute purposes
	DECLARE_ZERO(D3D12_SHADER_BYTECODE, CS);
	CS.BytecodeLength = pDesc->pShaderProgram->pShaderBlobs[0]->GetBufferSize();
	CS.pShaderBytecode = pDesc->pShaderProgram->pShaderBlobs[0]->GetBufferPointer();

	DECLARE_ZERO(D3D12_CACHED_PIPELINE_STATE, cached_pso_desc);
	cached_pso_desc.pCachedBlob = NULL;
	cached_pso_desc.CachedBlobSizeInBytes = 0;

	DECLARE_ZERO(D3D12_COMPUTE_PIPELINE_STATE_DESC, pipeline_state_desc);
	pipeline_state_desc.pRootSignature = pDesc->pRootSignature->pDxRootSignature;
	pipeline_state_desc.CS = CS;
	pipeline_state_desc.CachedPSO = cached_pso_desc;
#ifndef _DURANGO
	pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
#endif

	// If running Linked Mode (SLI) create pipeline for all nodes
	// #NOTE : In non SLI mode, mNodeCount will be 0 which sets nodeMask to default value
	pipeline_state_desc.NodeMask = util_calculate_shared_node_mask(pRenderer);

	HRESULT hres = pRenderer->pDxDevice->CreateComputePipelineState(
		&pipeline_state_desc, __uuidof(pPipeline->pDxPipelineState), (void**)&(pPipeline->pDxPipelineState));
	ASSERT(SUCCEEDED(hres));

	*ppPipeline = pPipeline;
}

void removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pPipeline);

	//delete pipeline from device
	SAFE_RELEASE(pPipeline->pDxPipelineState);

	SAFE_FREE(pPipeline);
}

void addBlendState(Renderer* pRenderer, const BlendStateDesc* pDesc, BlendState** ppBlendState)
{
	UNREF_PARAM(pRenderer);

	int blendDescIndex = 0;
#ifdef _DEBUG

	for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
	{
		if (pDesc->mRenderTargetMask & (1 << i))
		{
			ASSERT(pDesc->mSrcFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mDstFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mSrcAlphaFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mDstAlphaFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mBlendModes[blendDescIndex] < BlendMode::MAX_BLEND_MODES);
			ASSERT(pDesc->mBlendAlphaModes[blendDescIndex] < BlendMode::MAX_BLEND_MODES);
		}

		if (pDesc->mIndependentBlend)
			++blendDescIndex;
	}

	blendDescIndex = 0;
#endif

	BlendState* pBlendState = (BlendState*)conf_calloc(1, sizeof(*pBlendState));

	pBlendState->mDxBlendDesc.AlphaToCoverageEnable = (BOOL)pDesc->mAlphaToCoverage;
	pBlendState->mDxBlendDesc.IndependentBlendEnable = TRUE;
	for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; i++)
	{
		if (pDesc->mRenderTargetMask & (1 << i))
		{
			BOOL blendEnable =
				(gDx12BlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]] != D3D12_BLEND_ONE ||
				 gDx12BlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]] != D3D12_BLEND_ZERO ||
				 gDx12BlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]] != D3D12_BLEND_ONE ||
				 gDx12BlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]] != D3D12_BLEND_ZERO);

			pBlendState->mDxBlendDesc.RenderTarget[i].BlendEnable = blendEnable;
			pBlendState->mDxBlendDesc.RenderTarget[i].RenderTargetWriteMask = (UINT8)pDesc->mMasks[blendDescIndex];
			pBlendState->mDxBlendDesc.RenderTarget[i].BlendOp = gDx12BlendOpTranslator[pDesc->mBlendModes[blendDescIndex]];
			pBlendState->mDxBlendDesc.RenderTarget[i].SrcBlend = gDx12BlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]];
			pBlendState->mDxBlendDesc.RenderTarget[i].DestBlend = gDx12BlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]];
			pBlendState->mDxBlendDesc.RenderTarget[i].BlendOpAlpha = gDx12BlendOpTranslator[pDesc->mBlendAlphaModes[blendDescIndex]];
			pBlendState->mDxBlendDesc.RenderTarget[i].SrcBlendAlpha = gDx12BlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]];
			pBlendState->mDxBlendDesc.RenderTarget[i].DestBlendAlpha =
				gDx12BlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]];
		}

		if (pDesc->mIndependentBlend)
			++blendDescIndex;
	}

	*ppBlendState = pBlendState;
}

void removeBlendState(BlendState* pBlendState) { SAFE_FREE(pBlendState); }

void addDepthState(Renderer* pRenderer, const DepthStateDesc* pDesc, DepthState** ppDepthState)
{
	UNREF_PARAM(pRenderer);

	ASSERT(pDesc->mDepthFunc < CompareMode::MAX_COMPARE_MODES);
	ASSERT(pDesc->mStencilFrontFunc < CompareMode::MAX_COMPARE_MODES);
	ASSERT(pDesc->mStencilFrontFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mDepthFrontFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mStencilFrontPass < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mStencilBackFunc < CompareMode::MAX_COMPARE_MODES);
	ASSERT(pDesc->mStencilBackFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mDepthBackFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mStencilBackPass < StencilOp::MAX_STENCIL_OPS);

	DepthState* pDepthState = (DepthState*)conf_calloc(1, sizeof(*pDepthState));

	pDepthState->mDxDepthStencilDesc.DepthEnable = (BOOL)pDesc->mDepthTest;
	pDepthState->mDxDepthStencilDesc.DepthWriteMask = pDesc->mDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	pDepthState->mDxDepthStencilDesc.DepthFunc = gDx12ComparisonFuncTranslator[pDesc->mDepthFunc];
	pDepthState->mDxDepthStencilDesc.StencilEnable = (BOOL)pDesc->mStencilTest;
	pDepthState->mDxDepthStencilDesc.StencilReadMask = pDesc->mStencilReadMask;
	pDepthState->mDxDepthStencilDesc.StencilWriteMask = pDesc->mStencilWriteMask;
	pDepthState->mDxDepthStencilDesc.BackFace.StencilFunc = gDx12ComparisonFuncTranslator[pDesc->mStencilBackFunc];
	pDepthState->mDxDepthStencilDesc.FrontFace.StencilFunc = gDx12ComparisonFuncTranslator[pDesc->mStencilFrontFunc];
	pDepthState->mDxDepthStencilDesc.BackFace.StencilDepthFailOp = gDx12StencilOpTranslator[pDesc->mDepthBackFail];
	pDepthState->mDxDepthStencilDesc.FrontFace.StencilDepthFailOp = gDx12StencilOpTranslator[pDesc->mDepthFrontFail];
	pDepthState->mDxDepthStencilDesc.BackFace.StencilFailOp = gDx12StencilOpTranslator[pDesc->mStencilBackFail];
	pDepthState->mDxDepthStencilDesc.FrontFace.StencilFailOp = gDx12StencilOpTranslator[pDesc->mStencilFrontFail];
	pDepthState->mDxDepthStencilDesc.BackFace.StencilPassOp = gDx12StencilOpTranslator[pDesc->mStencilFrontPass];
	pDepthState->mDxDepthStencilDesc.FrontFace.StencilPassOp = gDx12StencilOpTranslator[pDesc->mStencilBackPass];

	*ppDepthState = pDepthState;
}

void removeDepthState(DepthState* pDepthState) { SAFE_FREE(pDepthState); }

void addRasterizerState(Renderer* pRenderer, const RasterizerStateDesc* pDesc, RasterizerState** ppRasterizerState)
{
	UNREF_PARAM(pRenderer);

	ASSERT(pDesc->mFillMode < FillMode::MAX_FILL_MODES);
	ASSERT(pDesc->mCullMode < CullMode::MAX_CULL_MODES);
	ASSERT(pDesc->mFrontFace == FRONT_FACE_CCW || pDesc->mFrontFace == FRONT_FACE_CW);

	RasterizerState* pRasterizerState = (RasterizerState*)conf_calloc(1, sizeof(*pRasterizerState));

	pRasterizerState->mDxRasterizerDesc.FillMode = gDx12FillModeTranslator[pDesc->mFillMode];
	pRasterizerState->mDxRasterizerDesc.CullMode = gDx12CullModeTranslator[pDesc->mCullMode];
	pRasterizerState->mDxRasterizerDesc.FrontCounterClockwise = pDesc->mFrontFace == FRONT_FACE_CCW;
	pRasterizerState->mDxRasterizerDesc.DepthBias = pDesc->mDepthBias;
	pRasterizerState->mDxRasterizerDesc.DepthBiasClamp = 0.0f;
	pRasterizerState->mDxRasterizerDesc.SlopeScaledDepthBias = pDesc->mSlopeScaledDepthBias;
	pRasterizerState->mDxRasterizerDesc.DepthClipEnable = TRUE;
	pRasterizerState->mDxRasterizerDesc.MultisampleEnable = pDesc->mMultiSample ? TRUE : FALSE;
	pRasterizerState->mDxRasterizerDesc.AntialiasedLineEnable = FALSE;
	pRasterizerState->mDxRasterizerDesc.ForcedSampleCount = 0;
	pRasterizerState->mDxRasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	*ppRasterizerState = pRasterizerState;
}

void removeRasterizerState(RasterizerState* pRasterizerState) { SAFE_FREE(pRasterizerState); }
/************************************************************************/
// Command buffer Functions
/************************************************************************/
void beginCmd(Cmd* pCmd)
{
	ASSERT(pCmd);
	ASSERT(pCmd->pDxCmdList);
	ASSERT(pCmd->pDxCmdAlloc);

	HRESULT hres = pCmd->pDxCmdAlloc->Reset();
	ASSERT(SUCCEEDED(hres));

	hres = pCmd->pDxCmdList->Reset(pCmd->pDxCmdAlloc, NULL);
	ASSERT(SUCCEEDED(hres));

	if (pCmd->pDxCmdList->GetType() != D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY)
	{
		ID3D12DescriptorHeap* heaps[2] = { pCmd->pRenderer->pCbvSrvUavHeap[pCmd->mNodeIndex]->pCurrentHeap,
										   pCmd->pRenderer->pSamplerHeap[pCmd->mNodeIndex]->pCurrentHeap };
		pCmd->pDxCmdList->SetDescriptorHeaps(2, heaps);
	}

	pCmd->pBoundRootSignature = NULL;
	pCmd->mViewPosition = 0;
	pCmd->mSamplerPosition = 0;
	pCmd->mTransientCBVPosition = 0;
}

void endCmd(Cmd* pCmd)
{
	ASSERT(pCmd);

	::cmdFlushBarriers(pCmd);

	ASSERT(pCmd->pDxCmdList);

	HRESULT hres = pCmd->pDxCmdList->Close();
	ASSERT(SUCCEEDED(hres));
}

void cmdBindRenderTargets(
	Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil,
	const LoadActionsDesc* pLoadActions /* = NULL*/, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice,
	uint32_t depthMipSlice)
{
	ASSERT(pCmd);
	ASSERT(pCmd->pDxCmdList);

	if (!renderTargetCount && !pDepthStencil)
		return;

	uint64_t                     renderPassHash = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE* p_dsv_handle = NULL;
	D3D12_CPU_DESCRIPTOR_HANDLE* p_rtv_handles =
		renderTargetCount ? (D3D12_CPU_DESCRIPTOR_HANDLE*)alloca(renderTargetCount * sizeof(D3D12_CPU_DESCRIPTOR_HANDLE)) : NULL;
	for (uint32_t i = 0; i < renderTargetCount; ++i)
	{
		uint32_t handle = 0;
		if (pColorMipSlices)
		{
			if (pColorArraySlices)
				handle = 1 + pColorMipSlices[i] * ppRenderTargets[i]->mDesc.mArraySize + pColorArraySlices[i];
			else
				handle = 1 + pColorMipSlices[i];
		}
		else if (pColorArraySlices)
		{
			handle = 1 + pColorArraySlices[i];
		}

		p_rtv_handles[i] = ppRenderTargets[i]->pDxDescriptors[handle];
		pCmd->pBoundColorFormats[i] = ppRenderTargets[i]->mDesc.mFormat;
		pCmd->pBoundSrgbValues[i] = ppRenderTargets[i]->mDesc.mSrgb;
		pCmd->mBoundWidth = ppRenderTargets[i]->mDesc.mWidth;
		pCmd->mBoundHeight = ppRenderTargets[i]->mDesc.mHeight;

		uint32_t hashValues[] = {
			(uint32_t)ppRenderTargets[i]->mDesc.mFormat,
			(uint32_t)ppRenderTargets[i]->mDesc.mSampleCount,
			(uint32_t)ppRenderTargets[i]->mDesc.mSrgb,
		};
		renderPassHash = tinystl::hash_state(hashValues, 3, renderPassHash);
	}

	if (pDepthStencil)
	{
		uint32_t handle = 0;
		if (depthMipSlice != -1)
		{
			if (depthArraySlice != -1)
				handle = 1 + depthMipSlice * pDepthStencil->mDesc.mArraySize + depthArraySlice;
			else
				handle = 1 + depthMipSlice;
		}
		else if (depthArraySlice != -1)
		{
			handle = 1 + depthArraySlice;
		}

		p_dsv_handle = &pDepthStencil->pDxDescriptors[handle];
		pCmd->mBoundDepthStencilFormat = pDepthStencil->mDesc.mFormat;
		pCmd->mBoundWidth = pDepthStencil->mDesc.mWidth;
		pCmd->mBoundHeight = pDepthStencil->mDesc.mHeight;

		uint32_t hashValues[] = {
			(uint32_t)pDepthStencil->mDesc.mFormat,
			(uint32_t)pDepthStencil->mDesc.mSampleCount,
			(uint32_t)pDepthStencil->mDesc.mSrgb,
		};
		renderPassHash = tinystl::hash_state(hashValues, 3, renderPassHash);
	}

	SampleCount sampleCount = renderTargetCount ? ppRenderTargets[0]->mDesc.mSampleCount : pDepthStencil->mDesc.mSampleCount;
	pCmd->mBoundSampleCount = sampleCount;
	pCmd->mBoundRenderTargetCount = renderTargetCount;
	pCmd->mRenderPassHash = renderPassHash;

	pCmd->pDxCmdList->OMSetRenderTargets(renderTargetCount, p_rtv_handles, FALSE, p_dsv_handle);

	//process clear actions (clear color/depth)
	if (pLoadActions)
	{
		for (uint32_t i = 0; i < renderTargetCount; ++i)
		{
			if (pLoadActions->mLoadActionsColor[i] == LOAD_ACTION_CLEAR)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE handle = p_rtv_handles[i];

				DECLARE_ZERO(FLOAT, color_rgba[4]);
				color_rgba[0] = pLoadActions->mClearColorValues[i].r;
				color_rgba[1] = pLoadActions->mClearColorValues[i].g;
				color_rgba[2] = pLoadActions->mClearColorValues[i].b;
				color_rgba[3] = pLoadActions->mClearColorValues[i].a;

				pCmd->pDxCmdList->ClearRenderTargetView(handle, color_rgba, 0, NULL);
			}
		}
		if (pLoadActions->mLoadActionDepth == LOAD_ACTION_CLEAR || pLoadActions->mLoadActionStencil == LOAD_ACTION_CLEAR)
		{
			D3D12_CLEAR_FLAGS flags = (D3D12_CLEAR_FLAGS)0;
			if (pLoadActions->mLoadActionDepth == LOAD_ACTION_CLEAR)
				flags |= D3D12_CLEAR_FLAG_DEPTH;
			if (pLoadActions->mLoadActionStencil == LOAD_ACTION_CLEAR)
				flags |= D3D12_CLEAR_FLAG_STENCIL;
			ASSERT(flags);
			pCmd->pDxCmdList->ClearDepthStencilView(
				*p_dsv_handle, flags, pLoadActions->mClearDepth.depth, (UINT8)pLoadActions->mClearDepth.stencil, 0, NULL);
		}
	}
}

void cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
	ASSERT(pCmd);

	//set new viewport
	ASSERT(pCmd->pDxCmdList);

	D3D12_VIEWPORT viewport;
	viewport.TopLeftX = x;
	viewport.TopLeftY = y;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinDepth = minDepth;
	viewport.MaxDepth = maxDepth;

	pCmd->pDxCmdList->RSSetViewports(1, &viewport);
}

void cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	ASSERT(pCmd);

	//set new scissor values
	ASSERT(pCmd->pDxCmdList);

	D3D12_RECT scissor;
	scissor.left = x;
	scissor.top = y;
	scissor.right = x + width;
	scissor.bottom = y + height;

	pCmd->pDxCmdList->RSSetScissorRects(1, &scissor);
}

void cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
	ASSERT(pCmd);
	ASSERT(pPipeline);

	//bind given pipeline
	ASSERT(pCmd->pDxCmdList);
	ASSERT(pPipeline->pDxPipelineState);

	if (pPipeline->mType == PIPELINE_TYPE_GRAPHICS)
	{
		pCmd->pDxCmdList->IASetPrimitiveTopology(pPipeline->mDxPrimitiveTopology);
	}

	pCmd->pDxCmdList->SetPipelineState(pPipeline->pDxPipelineState);
}

void cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t offset)
{
	ASSERT(pCmd);
	ASSERT(pBuffer);
	ASSERT(pCmd->pDxCmdList);
	ASSERT(D3D12_GPU_VIRTUAL_ADDRESS_NULL != pBuffer->mDxGpuAddress);

#ifdef _DURANGO
	BufferBarrier bufferBarriers[] = { { pBuffer, RESOURCE_STATE_INDEX_BUFFER } };
	cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, false);
#endif

	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = pBuffer->mDxGpuAddress + offset;
	ibView.Format = pBuffer->mDxIndexFormat;
	ibView.SizeInBytes = (UINT)(pBuffer->mDesc.mSize - offset);

	//bind given index buffer
	pCmd->pDxCmdList->IASetIndexBuffer(&ibView);
}

void cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, uint64_t* pOffsets)
{
	ASSERT(pCmd);
	ASSERT(0 != bufferCount);
	ASSERT(ppBuffers);
	ASSERT(pCmd->pDxCmdList);
	//bind given vertex buffer

	DECLARE_ZERO(D3D12_VERTEX_BUFFER_VIEW, views[MAX_VERTEX_ATTRIBS]);
	for (uint32_t i = 0; i < bufferCount; ++i)
	{
		ASSERT(D3D12_GPU_VIRTUAL_ADDRESS_NULL != ppBuffers[i]->mDxGpuAddress);

		views[i].BufferLocation = (ppBuffers[i]->mDxGpuAddress + (pOffsets ? pOffsets[i] : 0));
		views[i].SizeInBytes = (UINT)(ppBuffers[i]->mDesc.mSize - (pOffsets ? pOffsets[i] : 0));
		views[i].StrideInBytes = (UINT)ppBuffers[i]->mDesc.mVertexStride;
#ifdef _DURANGO
		BufferBarrier bufferBarriers[] = { { ppBuffers[i], RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER } };
		cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, false);
#endif
	}

	pCmd->pDxCmdList->IASetVertexBuffers(0, bufferCount, views);
}

void cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
	ASSERT(pCmd);

	//draw given vertices
	ASSERT(pCmd->pDxCmdList);

	pCmd->pDxCmdList->DrawInstanced((UINT)vertexCount, (UINT)1, (UINT)firstVertex, (UINT)0);
}

void cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
	ASSERT(pCmd);

	//draw given vertices
	ASSERT(pCmd->pDxCmdList);

	pCmd->pDxCmdList->DrawInstanced((UINT)vertexCount, (UINT)instanceCount, (UINT)firstVertex, (UINT)firstInstance);
}

void cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
	ASSERT(pCmd);

	//draw indexed mesh
	ASSERT(pCmd->pDxCmdList);

	pCmd->pDxCmdList->DrawIndexedInstanced((UINT)indexCount, (UINT)1, (UINT)firstIndex, (UINT)firstVertex, (UINT)0);
}

void cmdDrawIndexedInstanced(
	Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstInstance, uint32_t firstVertex)
{
	ASSERT(pCmd);

	//draw indexed mesh
	ASSERT(pCmd->pDxCmdList);

	pCmd->pDxCmdList->DrawIndexedInstanced((UINT)indexCount, (UINT)instanceCount, (UINT)firstIndex, (UINT)firstVertex, (UINT)firstInstance);
}

void cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	ASSERT(pCmd);

	//dispatch given command
	ASSERT(pCmd->pDxCmdList != NULL);

	pCmd->pDxCmdList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

void cmdBindDescriptors(Cmd* pCmd, RootSignature* pRootSignature, uint32_t numDescriptors, DescriptorData* pDescParams)
{
	const uint32_t     nodeIndex = pCmd->mNodeIndex;
	Renderer*          pRenderer = pCmd->pRenderer;
	const uint32_t     setCount = DESCRIPTOR_UPDATE_FREQ_COUNT;
	DescriptorManager* pm = get_descriptor_manager(pRenderer, pRootSignature);

	DescriptorStoreHeap* pCbvSrvUavHeap = pRenderer->pCbvSrvUavHeap[nodeIndex];
	DescriptorStoreHeap* pSamplerHeap = pRenderer->pSamplerHeap[nodeIndex];

	for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
	{
		// Reset other data
		pm->mBoundCbvSrvUavTables[setIndex] = true;
		pm->mBoundSamplerTables[setIndex] = true;
	}

	// Compare the currently bound root signature with the root signature of the descriptor manager
	// If these values dont match, we must bind the root signature of the descriptor manager
	// If the values match, no op is required
	if (pCmd->pBoundRootSignature != pRootSignature)
	{
		if (pm->pCurrentCmd != pCmd)
		{
			pm->pCurrentCmd = pCmd;
			pm->mFrameIdx = (pm->mFrameIdx + 1) % MAX_FRAMES_IN_FLIGHT;
		}

		// Bind root signature
		pCmd->pBoundRootSignature = pRootSignature;

		if (pRootSignature->mPipelineType == PIPELINE_TYPE_GRAPHICS)
			pCmd->pDxCmdList->SetGraphicsRootSignature(pRootSignature->pDxRootSignature);
		else if (pRootSignature->mPipelineType == PIPELINE_TYPE_COMPUTE)
			pCmd->pDxCmdList->SetComputeRootSignature(pRootSignature->pDxRootSignature);
	}

	// 64 bit unsigned value for hashing the mTextureId / mBufferId / mSamplerId of the input descriptors
	// This value will be later used as look up to find if a descriptor table with the given hash already exists
	// This way we will copy descriptor handles for a particular set of descriptors only once
	// Then we only need to do a look up into the mDescriptorTableMap with pHash[setIndex] as the key and retrieve the DescriptorTable* value
	uint64_t pCbvSrvUavHash[setCount] = { 0 };
	uint64_t pSamplerHash[setCount] = { 0 };

	// Loop through input params to check for new data
	for (uint32_t i = 0; i < numDescriptors; ++i)
	{
		const DescriptorData* pParam = &pDescParams[i];

		ASSERT(pParam);
		if (!pParam->pName)
		{
			LOGERRORF("Name of Descriptor at index (%u) is NULL", i);
			return;
		}

		uint32_t              descIndex = ~0u;
		const DescriptorInfo* pDesc = get_descriptor(pRootSignature, pParam->pName, &descIndex);
		if (!pDesc)
			continue;

		// Find the update frequency of the descriptor
		const DescriptorUpdateFrequency setIndex = pDesc->mUpdateFrquency;

		// If input param is a root constant or root descriptor no need to do any further checks
		if (pDesc->mDxType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
		{
			if (!pParam->pRootConstant)
			{
				LOGERRORF("Root constant (%s) is NULL", pParam->pName);
				continue;
			}
			if (pRootSignature->mPipelineType == PIPELINE_TYPE_COMPUTE)
			{
				pCmd->pDxCmdList->SetComputeRoot32BitConstants(
					pRootSignature->pDxRootConstantRootIndices[pDesc->mIndexInParent], pDesc->mDesc.size, pParam->pRootConstant, 0);
			}
			else if (pRootSignature->mPipelineType == PIPELINE_TYPE_GRAPHICS)
			{
				pCmd->pDxCmdList->SetGraphicsRoot32BitConstants(
					pRootSignature->pDxRootConstantRootIndices[pDesc->mIndexInParent], pDesc->mDesc.size, pParam->pRootConstant, 0);
			}
			continue;
		}
		else if (pDesc->mDxType == D3D12_ROOT_PARAMETER_TYPE_CBV)
		{
			if (!pParam->ppBuffers[0])
			{
				LOGERRORF("Root descriptor CBV (%s) is NULL", pParam->pName);
				continue;
			}
			D3D12_GPU_VIRTUAL_ADDRESS cbv = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;

			if (pDesc->mDesc.type == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			{
				cbv = pParam->ppBuffers[0]->mDxGpuAddress + (pParam->pOffsets ? pParam->pOffsets[0] : 0);
			}
			// If this descriptor is a root constant which was converted to a root cbv, use the internal ring buffer
			else if (pDesc->mDesc.type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
			{
				if (!pCmd->pRootConstantRingBuffer)
				{
					// 4KB ring buffer should be enough since size of root constant data is usually pretty small (< 32 bytes)
					addUniformRingBuffer(pRenderer, 4000U, &pCmd->pRootConstantRingBuffer);
				}
				uint32_t         size = pDesc->mDesc.size * sizeof(uint32_t);
				RingBufferOffset offset = getUniformBufferOffset(pCmd->pRootConstantRingBuffer, size);
				memcpy((uint8_t*)offset.pBuffer->pCpuMappedAddress + offset.mOffset, pParam->pRootConstant, size);
				cbv = offset.pBuffer->mDxGpuAddress + offset.mOffset;
			}

			if (pRootSignature->mPipelineType == PIPELINE_TYPE_COMPUTE)
			{
				pCmd->pDxCmdList->SetComputeRootConstantBufferView(
					pRootSignature->pDxRootDescriptorRootIndices[pDesc->mIndexInParent], cbv);
			}
			else if (pRootSignature->mPipelineType == PIPELINE_TYPE_GRAPHICS)
			{
				pCmd->pDxCmdList->SetGraphicsRootConstantBufferView(
					pRootSignature->pDxRootDescriptorRootIndices[pDesc->mIndexInParent], cbv);
			}
			continue;
		}

		// Unbind current descriptor table so we can bind a new one
		pm->mBoundCbvSrvUavTables[setIndex] = false;
		pm->mBoundSamplerTables[setIndex] = false;

		DescriptorType type = pDesc->mDesc.type;
		const uint32_t arrayCount = max(1U, pParam->mCount);

		switch (type)
		{
			case DESCRIPTOR_TYPE_SAMPLER:
				if (pDesc->mIndexInParent == -1)
				{
					LOGERRORF(
						"Trying to bind a static sampler (%s). All static samplers must be bound in addRootSignature through "
						"RootSignatureDesc::mStaticSamplers",
						pParam->pName);
					continue;
				}
				if (!pParam->ppSamplers)
				{
					LOGERRORF("Sampler descriptor (%s) is NULL", pParam->pName);
					return;
				}
				for (uint32_t j = 0; j < arrayCount; ++j)
				{
					if (!pParam->ppSamplers[j])
					{
						LOGERRORF("Sampler descriptor (%s) at array index (%u) is NULL", pParam->pName, j);
						return;
					}
					pSamplerHash[setIndex] = tinystl::hash_state(&pParam->ppSamplers[j]->mSamplerId, 1, pSamplerHash[setIndex]);
					pm->pSamplerDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = pParam->ppSamplers[j]->mDxSamplerHandle;
				}
				break;
			case DESCRIPTOR_TYPE_TEXTURE:
			{
				if (!pParam->ppTextures)
				{
					LOGERRORF("Texture descriptor (%s) is NULL", pParam->pName);
					return;
				}
				D3D12_CPU_DESCRIPTOR_HANDLE* handlePtr = &pm->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex];
				Texture* const*              ppTextures = pParam->ppTextures;

				for (uint32_t j = 0; j < arrayCount; ++j)
				{
#ifdef _DEBUG
					if (!pParam->ppTextures[j])
					{
						LOGERRORF("Texture descriptor (%s) at array index (%u) is NULL", pParam->pName, j);
						return;
					}
#endif
					//if (j < arrayCount - 1)
					//  _mm_prefetch((char*)&ppTextures[j]->mTextureId, _MM_HINT_T0);
					Texture* tex = ppTextures[j];
					pCbvSrvUavHash[setIndex] = tinystl::hash_state(&tex->mTextureId, 1, pCbvSrvUavHash[setIndex]);
					handlePtr[j] = tex->mDxSRVDescriptor;
					//pm->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = tex->mDxSrvHandle;
				}
				break;
			}
			case DESCRIPTOR_TYPE_RW_TEXTURE:
			{
				if (!pParam->ppTextures)
				{
					LOGERRORF("RW Texture descriptor (%s) is NULL", pParam->pName);
					return;
				}
				D3D12_CPU_DESCRIPTOR_HANDLE* handlePtr = &pm->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex];
				Texture* const*              ppTextures = pParam->ppTextures;
				if (pParam->mUAVMipSlice)
					pCbvSrvUavHash[setIndex] = tinystl::hash_state(&pParam->mUAVMipSlice, 1, pCbvSrvUavHash[setIndex]);

				for (uint32_t j = 0; j < arrayCount; ++j)
				{
#ifdef _DEBUG
					if (!pParam->ppTextures[j])
					{
						LOGERRORF("RW Texture descriptor (%s) at array index (%u) is NULL", pParam->pName, j);
						return;
					}
#endif
					//if (j < arrayCount - 1)
					//  _mm_prefetch((char*)&ppTextures[j]->mTextureId, _MM_HINT_T0);
					Texture* tex = ppTextures[j];
					pCbvSrvUavHash[setIndex] = tinystl::hash_state(&tex->mTextureId, 1, pCbvSrvUavHash[setIndex]);
					handlePtr[j] = tex->pDxUAVDescriptors[pParam->mUAVMipSlice];
					//pm->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = tex->mDxSrvHandle;
				}
				break;
			}
			case DESCRIPTOR_TYPE_BUFFER:
				if (!pParam->ppBuffers)
				{
					LOGERRORF("Buffer descriptor (%s) is NULL", pParam->pName);
					return;
				}
				for (uint32_t j = 0; j < arrayCount; ++j)
				{
					if (!pParam->ppBuffers[j])
					{
						LOGERRORF("Buffer descriptor (%s) at array index (%u) is NULL", pParam->pName, j);
						return;
					}
					pCbvSrvUavHash[setIndex] = tinystl::hash_state(&pParam->ppBuffers[j]->mBufferId, 1, pCbvSrvUavHash[setIndex]);
					pm->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = pParam->ppBuffers[j]->mDxSrvHandle;
#ifdef _DURANGO
					ResourceState state = RESOURCE_STATE_SHADER_RESOURCE;

					if (pCmd->pCmdPool->mCmdPoolDesc.mCmdPoolType != CMD_POOL_DIRECT)
					{
						state = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
					}

					BufferBarrier bufferBarriers[] = { { pParam->ppBuffers[j], state } };
					cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, true);
#endif
				}
				break;
			case DESCRIPTOR_TYPE_RW_BUFFER:
				if (!pParam->ppBuffers)
				{
					LOGERRORF("Buffer descriptor (%s) is NULL", pParam->pName);
					return;
				}
				for (uint32_t j = 0; j < arrayCount; ++j)
				{
					if (!pParam->ppBuffers[j])
					{
						LOGERRORF("Buffer descriptor (%s) at array index (%u) is NULL", pParam->pName, j);
						return;
					}
					pCbvSrvUavHash[setIndex] = tinystl::hash_state(&pParam->ppBuffers[j]->mBufferId, 1, pCbvSrvUavHash[setIndex]);
					pm->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = pParam->ppBuffers[j]->mDxUavHandle;
#ifdef _DURANGO
					BufferBarrier bufferBarriers[] = { { pParam->ppBuffers[j], RESOURCE_STATE_UNORDERED_ACCESS } };
					cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, true);
#endif
				}
				break;
			case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			{
				if (!pParam->ppBuffers)
				{
					LOGERRORF("Buffer descriptor (%s) is NULL", pParam->pName);
					return;
				}

				UINT64 descriptorSize = pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->mDescriptorSize;

				for (uint32_t j = 0; j < arrayCount; ++j)
				{
					if (!pParam->ppBuffers[j])
					{
						LOGERRORF("Buffer descriptor (%s) at array index (%u) is NULL", pParam->pName, j);
						return;
					}

					pCbvSrvUavHash[setIndex] = tinystl::hash_state(&pParam->ppBuffers[j]->mBufferId, 1, pCbvSrvUavHash[setIndex]);

					if ((pParam->ppBuffers[j]->mDesc.mFlags & BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION) ||
						(pParam->pOffsets && pParam->pOffsets[j] != 0))
					{
						if (pCmd->mTransientCBVs.ptr == D3D12_GPU_VIRTUAL_ADDRESS_NULL)
						{
							pCmd->mTransientCBVs = add_cpu_descriptor_handles(
								pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], MAX_TRANSIENT_CBVS_PER_FRAME);
						}

						D3D12_CPU_DESCRIPTOR_HANDLE handle = { pCmd->mTransientCBVs.ptr + pCmd->mTransientCBVPosition * descriptorSize };
						++pCmd->mTransientCBVPosition;
						D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
						cbvDesc.BufferLocation = pParam->ppBuffers[j]->mDxGpuAddress + pParam->pOffsets[j];
						cbvDesc.SizeInBytes =
							(pParam->pSizes ? (UINT)pParam->pSizes[i] : min(65536, (UINT)pParam->ppBuffers[j]->mDesc.mSize));
						pRenderer->pDxDevice->CreateConstantBufferView(&cbvDesc, handle);
						pm->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = handle;

						if (pParam->pOffsets)
							pCbvSrvUavHash[setIndex] = tinystl::hash_state(&pParam->pOffsets[j], 1, pCbvSrvUavHash[setIndex]);
						if (pParam->pSizes)
							pCbvSrvUavHash[setIndex] = tinystl::hash_state(&pParam->pSizes[j], 1, pCbvSrvUavHash[setIndex]);
					}
					else
					{
						pm->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = pParam->ppBuffers[j]->mDxCbvHandle;
					}
				}
				break;
			}
			default: break;
		}
	}

#ifdef _DURANGO
	cmdFlushBarriers(pCmd);
#endif

	for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
	{
		uint32_t descCount = pRootSignature->mDxViewDescriptorCounts[setIndex] + pRootSignature->mDxSamplerDescriptorCounts[setIndex];
		uint32_t viewCount = pRootSignature->mDxCumulativeViewDescriptorCounts[setIndex];
		uint32_t samplerCount = pRootSignature->mDxCumulativeSamplerDescriptorCounts[setIndex];

		if (descCount)
		{
			DescriptorTable cbvSrvUavTable = {};
			DescriptorTable samplerTable = {};

			if (setIndex == DESCRIPTOR_UPDATE_FREQ_NONE)
			{
				if (viewCount && !pm->mBoundCbvSrvUavTables[setIndex])
				{
					// Search for the generated hash of descriptors in the descriptor table map
					// If the hash already exists, it means we have already created a descriptor table with the input descriptors
					// Now we just bind that descriptor table and no other op is required
					ConstDescriptorTableMapIterator it =
						pm->mStaticCbvSrvUavDescriptorTableMap[pm->mFrameIdx].find(pCbvSrvUavHash[setIndex]);
					if (it.node)
					{
						cbvSrvUavTable = it.node->second;
					}
					// If the given hash does not exist, we create a new descriptor table and insert it into the descriptor table map
					else
					{
						add_gpu_descriptor_handles(
							pCbvSrvUavHeap, &cbvSrvUavTable.mBaseCpuHandle, &cbvSrvUavTable.mBaseGpuHandle, viewCount);
						cbvSrvUavTable.mDescriptorCount = viewCount;
						cbvSrvUavTable.mNodeIndex = nodeIndex;

						// Copy the descriptor handles from the cpu view heap to the gpu view heap
						// The source handles pm->pViewDescriptors are collected at the same time when we hashed the descriptors
						for (uint32_t i = 0; i < viewCount; ++i)
							pRenderer->pDxDevice->CopyDescriptorsSimple(
								1, { cbvSrvUavTable.mBaseCpuHandle.ptr + i * pCbvSrvUavHeap->mDescriptorSize },
								pm->pViewDescriptorHandles[setIndex][i], pCbvSrvUavHeap->mType);

						pm->mStaticCbvSrvUavDescriptorTableMap[pm->mFrameIdx]
							.insert({ pCbvSrvUavHash[setIndex], cbvSrvUavTable })
							.first.node->second;
					}
				}
				if (samplerCount && !pm->mBoundSamplerTables[setIndex])
				{
					// Search for the generated hash of descriptors in the descriptor table map
					// If the hash already exists, it means we have already created a descriptor table with the input descriptors
					// Now we just bind that descriptor table and no other op is required
					ConstDescriptorTableMapIterator it = pm->mStaticSamplerDescriptorTableMap[pm->mFrameIdx].find(pSamplerHash[setIndex]);
					if (it.node)
					{
						samplerTable = it.node->second;
					}
					// If the given hash does not exist, we create a new descriptor table and insert it into the descriptor table map
					else
					{
						add_gpu_descriptor_handles(pSamplerHeap, &samplerTable.mBaseCpuHandle, &samplerTable.mBaseGpuHandle, samplerCount);
						samplerTable.mDescriptorCount = samplerCount;
						samplerTable.mNodeIndex = nodeIndex;

						// Copy the descriptor handles from the cpu view heap to the gpu view heap
						// The source handles pm->pViewDescriptors are collected at the same time when we hashed the descriptors
						for (uint32_t i = 0; i < samplerCount; ++i)
							pRenderer->pDxDevice->CopyDescriptorsSimple(
								1, { samplerTable.mBaseCpuHandle.ptr + i * pSamplerHeap->mDescriptorSize },
								pm->pSamplerDescriptorHandles[setIndex][i], pSamplerHeap->mType);

						pm->mStaticSamplerDescriptorTableMap[pm->mFrameIdx]
							.insert({ pSamplerHash[setIndex], samplerTable })
							.first.node->second;
					}
				}
			}
			// Dynamic descriptors
			else
			{
				if (viewCount)
				{
					if (pCmd->mViewCpuHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_NULL)
						add_gpu_descriptor_handles(
							pCbvSrvUavHeap, &pCmd->mViewCpuHandle, &pCmd->mViewGpuHandle, MAX_DYNAMIC_VIEW_DESCRIPTORS_PER_FRAME);

					cbvSrvUavTable.mBaseCpuHandle = { pCmd->mViewCpuHandle.ptr + pCmd->mViewPosition * pCbvSrvUavHeap->mDescriptorSize };
					cbvSrvUavTable.mBaseGpuHandle = { pCmd->mViewGpuHandle.ptr + pCmd->mViewPosition * pCbvSrvUavHeap->mDescriptorSize };
					pCmd->mViewPosition += viewCount;
				}
				if (samplerCount)
				{
					if (pCmd->mSamplerCpuHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_NULL)
						add_gpu_descriptor_handles(
							pSamplerHeap, &pCmd->mSamplerCpuHandle, &pCmd->mSamplerGpuHandle, MAX_DYNAMIC_SAMPLER_DESCRIPTORS_PER_FRAME);

					samplerTable.mBaseCpuHandle = { pCmd->mSamplerCpuHandle.ptr + pCmd->mSamplerPosition * pSamplerHeap->mDescriptorSize };
					samplerTable.mBaseGpuHandle = { pCmd->mSamplerGpuHandle.ptr + pCmd->mSamplerPosition * pSamplerHeap->mDescriptorSize };
					pCmd->mSamplerPosition += samplerCount;
				}

				// Copy the descriptor handles from the cpu view heap to the gpu view heap
				// The source handles pm->pViewDescriptors are collected at the same time when we hashed the descriptors
				for (uint32_t i = 0; i < viewCount; ++i)
					pRenderer->pDxDevice->CopyDescriptorsSimple(
						1, { cbvSrvUavTable.mBaseCpuHandle.ptr + i * pCbvSrvUavHeap->mDescriptorSize },
						pm->pViewDescriptorHandles[setIndex][i], pCbvSrvUavHeap->mType);

				// Copy the descriptor handles from the cpu sampler heap to the gpu sampler heap
				// The source handles pm->pSamplerDescriptors are collected at the same time when we hashed the descriptors
				for (uint32_t i = 0; i < samplerCount; ++i)
					pRenderer->pDxDevice->CopyDescriptorsSimple(
						1, { samplerTable.mBaseCpuHandle.ptr + i * pSamplerHeap->mDescriptorSize },
						pm->pSamplerDescriptorHandles[setIndex][i], pSamplerHeap->mType);
			}

			// Bind the view descriptor table if one exists
			if (cbvSrvUavTable.mBaseGpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
			{
				if (pRootSignature->mPipelineType == PIPELINE_TYPE_COMPUTE)
				{
					pCmd->pDxCmdList->SetComputeRootDescriptorTable(
						pRootSignature->mDxViewDescriptorTableRootIndices[setIndex], cbvSrvUavTable.mBaseGpuHandle);
				}
				else if (pRootSignature->mPipelineType == PIPELINE_TYPE_GRAPHICS)
				{
					pCmd->pDxCmdList->SetGraphicsRootDescriptorTable(
						pRootSignature->mDxViewDescriptorTableRootIndices[setIndex], cbvSrvUavTable.mBaseGpuHandle);
				}

				// Set the bound flag for the descriptor table of this update frequency
				// This way in the future if user tries to bind the same descriptor table, we can avoid unnecessary rebinds
				pm->mBoundCbvSrvUavTables[setIndex] = true;
			}

			// Bind the sampler descriptor table if one exists
			if (samplerTable.mBaseGpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
			{
				if (pRootSignature->mPipelineType == PIPELINE_TYPE_COMPUTE)
				{
					pCmd->pDxCmdList->SetComputeRootDescriptorTable(
						pRootSignature->mDxSamplerDescriptorTableRootIndices[setIndex], samplerTable.mBaseGpuHandle);
				}
				else if (pRootSignature->mPipelineType == PIPELINE_TYPE_GRAPHICS)
				{
					pCmd->pDxCmdList->SetGraphicsRootDescriptorTable(
						pRootSignature->mDxSamplerDescriptorTableRootIndices[setIndex], samplerTable.mBaseGpuHandle);
				}

				// Set the bound flag for the descriptor table of this update frequency
				// This way in the future if user tries to bind the same descriptor table, we can avoid unnecessary rebinds
				pm->mBoundSamplerTables[setIndex] = true;
			}
		}
	}
}

void cmdResourceBarrier(
	Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers,
	bool batch)
{
	D3D12_RESOURCE_BARRIER* barriers =
		(D3D12_RESOURCE_BARRIER*)alloca((numBufferBarriers + numTextureBarriers) * sizeof(D3D12_RESOURCE_BARRIER));
	uint32_t transitionCount = 0;

	for (uint32_t i = 0; i < numBufferBarriers; ++i)
	{
		BufferBarrier*          pTransBarrier = &pBufferBarriers[i];
		D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
		Buffer*                 pBuffer = pTransBarrier->pBuffer;

		// Only transition GPU visible resources.
		// Note: General CPU_TO_GPU resources have to stay in generic read state. They are created in upload heap.
		// There is one corner case: CPU_TO_GPU resources with UAV usage can have state transition. And they are created in custom heap.
		if (pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY ||
			pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU ||
			(pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU && pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER))
		{
			//if (!(pBuffer->mCurrentState & pTransBarrier->mNewState) && pBuffer->mCurrentState != pTransBarrier->mNewState)
			if (pBuffer->mCurrentState != pTransBarrier->mNewState)
			{
				if (pTransBarrier->mSplit)
				{
					ResourceState currentState = pBuffer->mCurrentState;
					// Determine if the barrier is begin only or end only
					// If the previous state and new state are same, we know this is end only since the state was already set in begin only
					if (pBuffer->mPreviousState & pTransBarrier->mNewState)
					{
						pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
						pBuffer->mPreviousState = RESOURCE_STATE_UNDEFINED;
						pBuffer->mCurrentState = pTransBarrier->mNewState;
					}
					else
					{
						pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
						pBuffer->mPreviousState = pTransBarrier->mNewState;
					}

					pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
					pBarrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
					pBarrier->Transition.pResource = pBuffer->pDxResource;
					pBarrier->Transition.StateBefore = util_to_dx_resource_state(currentState);
					pBarrier->Transition.StateAfter = util_to_dx_resource_state(pTransBarrier->mNewState);

					++transitionCount;
				}
				else
				{
					pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
					pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
					pBarrier->Transition.pResource = pBuffer->pDxResource;
					pBarrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
					pBarrier->Transition.StateBefore = util_to_dx_resource_state(pBuffer->mCurrentState);
					pBarrier->Transition.StateAfter = util_to_dx_resource_state(pTransBarrier->mNewState);

					pBuffer->mCurrentState = pTransBarrier->mNewState;

					++transitionCount;
				}
			}
			else if (pBuffer->mCurrentState == RESOURCE_STATE_UNORDERED_ACCESS)
			{
				pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				pBarrier->UAV.pResource = pBuffer->pDxResource;
				++transitionCount;
			}
		}
	}
	for (uint32_t i = 0; i < numTextureBarriers; ++i)
	{
		TextureBarrier*         pTransBarrier = &pTextureBarriers[i];
		D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
		Texture*                pTexture = pTransBarrier->pTexture;
		{
			if (pTexture->mCurrentState != pTransBarrier->mNewState)
			{
				if (pTransBarrier->mSplit)
				{
					ResourceState currentState = pTexture->mCurrentState;
					// Determine if the barrier is begin only or end only
					// If the previous state and new state are same, we know this is end only since the state was already set in begin only
					if (pTexture->mPreviousState & pTransBarrier->mNewState)
					{
						pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
						pTexture->mPreviousState = RESOURCE_STATE_UNDEFINED;
						pTexture->mCurrentState = pTransBarrier->mNewState;
					}
					else
					{
						pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
						pTexture->mPreviousState = pTransBarrier->mNewState;
					}

					pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
					pBarrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
					pBarrier->Transition.pResource = pTexture->pDxResource;
					pBarrier->Transition.StateBefore = util_to_dx_resource_state(currentState);
					pBarrier->Transition.StateAfter = util_to_dx_resource_state(pTransBarrier->mNewState);

					++transitionCount;
				}
				else
				{
					pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
					pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
					pBarrier->Transition.pResource = pTexture->pDxResource;
					pBarrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
					pBarrier->Transition.StateBefore = util_to_dx_resource_state(pTexture->mCurrentState);
					pBarrier->Transition.StateAfter = util_to_dx_resource_state(pTransBarrier->mNewState);
					pTexture->mCurrentState = pTransBarrier->mNewState;

					++transitionCount;
				}
			}
		}
	}

	if (transitionCount)
	{
		if (batch && (transitionCount + pCmd->mBatchBarrierCount <= MAX_BATCH_BARRIERS))
		{
			memcpy(pCmd->pBatchBarriers + pCmd->mBatchBarrierCount, barriers, sizeof(D3D12_RESOURCE_BARRIER) * transitionCount);
			pCmd->mBatchBarrierCount += transitionCount;
		}
		else
		{
			pCmd->pDxCmdList->ResourceBarrier(transitionCount, barriers);
		}
	}
}

void cmdSynchronizeResources(Cmd* pCmd, uint32_t numBuffers, Buffer** ppBuffers, uint32_t numTextures, Texture** ppTextures, bool batch)
{
	D3D12_RESOURCE_BARRIER* barriers = (D3D12_RESOURCE_BARRIER*)alloca((numBuffers + numTextures) * sizeof(D3D12_RESOURCE_BARRIER));
	uint32_t                transitionCount = 0;

	for (uint32_t i = 0; i < numBuffers; ++i)
	{
		D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
		{
			pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			pBarrier->UAV.pResource = ppBuffers[i]->pDxResource;
			++transitionCount;
		}
	}
	for (uint32_t i = 0; i < numTextures; ++i)
	{
		D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
		{
			pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			pBarrier->UAV.pResource = ppTextures[i]->pDxResource;
			++transitionCount;
		}
	}

	if (transitionCount)
	{
		if (batch && (transitionCount + pCmd->mBatchBarrierCount <= MAX_BATCH_BARRIERS))
		{
			memcpy(pCmd->pBatchBarriers + pCmd->mBatchBarrierCount, barriers, sizeof(D3D12_RESOURCE_BARRIER) * transitionCount);
			pCmd->mBatchBarrierCount += transitionCount;
		}
		else
		{
			pCmd->pDxCmdList->ResourceBarrier(transitionCount, barriers);
		}
	}
}

void cmdFlushBarriers(Cmd* pCmd)
{
	if (pCmd->mBatchBarrierCount)
	{
		pCmd->pDxCmdList->ResourceBarrier(pCmd->mBatchBarrierCount, pCmd->pBatchBarriers);
		pCmd->mBatchBarrierCount = 0;
	}
}

void cmdUpdateBuffer(Cmd* pCmd, uint64_t srcOffset, uint64_t dstOffset, uint64_t size, Buffer* pSrcBuffer, Buffer* pBuffer)
{
	ASSERT(pCmd);
	ASSERT(pSrcBuffer);
	ASSERT(pSrcBuffer->pDxResource);
	ASSERT(pBuffer);
	ASSERT(pBuffer->pDxResource);

#ifdef _DURANGO
	BufferBarrier bufferBarriers[] = { { pBuffer, RESOURCE_STATE_COPY_DEST } };
	::cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, false);
#endif

	pCmd->pDxCmdList->CopyBufferRegion(pBuffer->pDxResource, dstOffset, pSrcBuffer->pDxResource, srcOffset, size);

#ifdef _DURANGO
	{
		bufferBarriers[0].mNewState =
			((pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_VERTEX_BUFFER) ? RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
																		   : RESOURCE_STATE_COMMON);
		::cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, false);
	}
#endif
}

void cmdUpdateSubresources(
	Cmd* pCmd, uint32_t startSubresource, uint32_t numSubresources, SubresourceDataDesc* pSubresources, Buffer* pIntermediate,
	uint64_t intermediateOffset, Texture* pTexture)
{
	UINT64 RequiredSize = 0;
	UINT64 MemToAlloc = (sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) + sizeof(UINT) + sizeof(UINT64)) * numSubresources;
	if (MemToAlloc > UINT64_MAX)
	{
		return;
	}
	void* pMem = alloca(MemToAlloc);
	if (pMem == NULL)
	{
		return;
	}
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT*)(pMem);
	UINT64*                             pRowSizesInBytes = (UINT64*)(pLayouts + numSubresources);
	UINT*                               pNumRows = (UINT*)(pRowSizesInBytes + numSubresources);

	D3D12_RESOURCE_DESC Desc = pTexture->pDxResource->GetDesc();
	pCmd->pRenderer->pDxDevice->GetCopyableFootprints(
		&Desc, startSubresource, numSubresources, intermediateOffset, pLayouts, pNumRows, pRowSizesInBytes, &RequiredSize);

	for (UINT i = 0; i < numSubresources; ++i)
	{
		if (pRowSizesInBytes[i] > (UINT64)-1)
			return;

		D3D12_MEMCPY_DEST DestData = { (BYTE*)pIntermediate->pCpuMappedAddress + pLayouts[i].Offset, pLayouts[i].Footprint.RowPitch,
									   pLayouts[i].Footprint.RowPitch * pNumRows[i] };

		// Row-by-row memcpy
		for (UINT z = 0; z < pLayouts[i].Footprint.Depth; ++z)
		{
			BYTE*       pDestSlice = (BYTE*)(DestData.pData) + DestData.SlicePitch * z;
			const BYTE* pSrcSlice = (BYTE*)(pSubresources[i].pData) + pSubresources[i].mSlicePitch * z;
			for (UINT y = 0; y < pNumRows[i]; ++y)
			{
				memcpy(pDestSlice + DestData.RowPitch * y, pSrcSlice + pSubresources[i].mRowPitch * y, pRowSizesInBytes[i]);
			}
		}
	}

	for (UINT i = 0; i < numSubresources; ++i)
	{
		D3D12_TEXTURE_COPY_LOCATION Dst = {};
		Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		Dst.pResource = pTexture->pDxResource;
		Dst.SubresourceIndex = i + startSubresource;

		D3D12_TEXTURE_COPY_LOCATION Src = {};
		Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		Src.pResource = pIntermediate->pDxResource;
		Src.PlacedFootprint = pLayouts[i];

		pCmd->pDxCmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, NULL);
	}
}
/************************************************************************/
// Queue Fence Semaphore Functions
/************************************************************************/
void acquireNextImage(
	Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pSwapChainImageIndex)
{
	UNREF_PARAM(pSignalSemaphore);
	UNREF_PARAM(pFence);
	ASSERT(pRenderer);

	//get latest backbuffer image
	ASSERT(pSwapChain->pDxSwapChain);
	ASSERT(pSwapChainImageIndex);

	pSwapChain->mImageIndex = *pSwapChainImageIndex = fnHookGetSwapChainImageIndex(pSwapChain);
}

void queueSubmit(
	Queue* pQueue, uint32_t cmdCount, Cmd** ppCmds, Fence* pFence, uint32_t waitSemaphoreCount, Semaphore** ppWaitSemaphores,
	uint32_t signalSemaphoreCount, Semaphore** ppSignalSemaphores)
{
	//ASSERT that given cmd list and given params are valid
	ASSERT(pQueue);
	ASSERT(cmdCount > 0);
	ASSERT(ppCmds);
	if (waitSemaphoreCount > 0)
	{
		ASSERT(ppWaitSemaphores);
	}
	if (signalSemaphoreCount > 0)
	{
		ASSERT(ppSignalSemaphores);
	}

	//execute given command list
	ASSERT(pQueue->pDxQueue);

	cmdCount = cmdCount > MAX_SUBMIT_CMDS ? MAX_SUBMIT_CMDS : cmdCount;
	ID3D12CommandList** cmds = (ID3D12CommandList**)alloca(cmdCount * sizeof(ID3D12CommandList*));
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		cmds[i] = ppCmds[i]->pDxCmdList;
	}

	for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
		pQueue->pDxQueue->Wait(ppWaitSemaphores[i]->pFence->pDxFence, ppWaitSemaphores[i]->pFence->mFenceValue - 1);

	for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
	{
		waitForFences(pQueue, 1, &ppWaitSemaphores[i]->pFence, false);
	}

	pQueue->pDxQueue->ExecuteCommandLists(cmdCount, cmds);
	pQueue->pDxQueue->Signal(pFence->pDxFence, pFence->mFenceValue++);

	for (uint32_t i = 0; i < signalSemaphoreCount; ++i)
		pQueue->pDxQueue->Signal(ppSignalSemaphores[i]->pFence->pDxFence, ppSignalSemaphores[i]->pFence->mFenceValue++);
}

void queuePresent(
	Queue* pQueue, SwapChain* pSwapChain, uint32_t swapChainImageIndex, uint32_t waitSemaphoreCount, Semaphore** ppWaitSemaphores)
{
	UNREF_PARAM(swapChainImageIndex);
	ASSERT(pQueue);
	ASSERT(pSwapChain->pDxSwapChain);

	if (waitSemaphoreCount > 0)
	{
		ASSERT(ppWaitSemaphores);
	}

#if defined(_DURANGO)

	if (pSwapChain->mDesc.mColorFormat == ImageFormat::RGB10A2)
	{
		RECT presentRect;

		presentRect.top = 0;
		presentRect.left = 0;
		presentRect.bottom = pSwapChain->mDesc.mHeight;
		presentRect.right = pSwapChain->mDesc.mWidth;

		DXGIX_PRESENTARRAY_PARAMETERS presentParameterSets[1] = {};
		presentParameterSets[0].SourceRect = presentRect;
		presentParameterSets[0].ScaleFactorHorz = 1.0f;
		presentParameterSets[0].ScaleFactorVert = 1.0f;

		HRESULT hr = DXGIXPresentArray(
			pSwapChain->mDxSyncInterval, 0, pSwapChain->mFlags, _countof(presentParameterSets), &pSwapChain->pDxSwapChain,
			presentParameterSets);

		if (FAILED(hr))
		{
			hr = pQueue->pRenderer->pDxDevice->GetDeviceRemovedReason();
			if (FAILED(hr))
				ASSERT(false);    //TODO: let's do something with the error
		}
	}
	else
	{
		HRESULT hr = pSwapChain->pDxSwapChain->Present(pSwapChain->mDxSyncInterval, pSwapChain->mFlags);
		if (FAILED(hr))
		{
			hr = pQueue->pRenderer->pDxDevice->GetDeviceRemovedReason();
			if (FAILED(hr))
				ASSERT(false);    //TODO: let's do something with the error
		}
	}
#else
	HRESULT hr = pSwapChain->pDxSwapChain->Present(pSwapChain->mDxSyncInterval, pSwapChain->mFlags);
	if (FAILED(hr))
	{
		hr = pQueue->pRenderer->pDxDevice->GetDeviceRemovedReason();
		if (FAILED(hr))
			ASSERT(false);    //TODO: let's do something with the error
	}
#endif
}

bool queueSignal(Queue* pQueue, Fence* fence, uint64_t value)
{
	ASSERT(pQueue);
	ASSERT(fence);

	HRESULT hres = pQueue->pDxQueue->Signal(fence->pDxFence, value);

	return SUCCEEDED(hres);
}

void waitForFences(Queue* pQueue, uint32_t fenceCount, Fence** ppFences, bool signal)
{
	// Wait for fence completion
	for (uint32_t i = 0; i < fenceCount; ++i)
	{
		// Usecase A: If we want to wait for an already signaled fence, we shouldn't issue new signal here.
		// Usecase B: If we want to wait for all works in the queue complete, we should signal and wait.
		// Our current vis buffer implemnetation uses this function as Usecase A. Thus we should not signal again.
		if (signal)
			pQueue->pDxQueue->Signal(ppFences[i]->pDxFence, ppFences[i]->mFenceValue++);

		FenceStatus fenceStatus;
		::getFenceStatus(pQueue->pRenderer, ppFences[i], &fenceStatus);
		uint64_t fenceValue = ppFences[i]->mFenceValue - 1;
		//if (completedValue < fenceValue)
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
		{
			ppFences[i]->pDxFence->SetEventOnCompletion(fenceValue, ppFences[i]->pDxWaitIdleFenceEvent);
			WaitForSingleObject(ppFences[i]->pDxWaitIdleFenceEvent, INFINITE);
		}
	}
}

void getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
	UNREF_PARAM(pRenderer);

	if (pFence->pDxFence->GetCompletedValue() < pFence->mFenceValue - 1)
		*pFenceStatus = FENCE_STATUS_INCOMPLETE;
	else
		*pFenceStatus = FENCE_STATUS_COMPLETE;
}

bool fenceSetEventOnCompletion(Fence* fence, uint64_t value, HANDLE fenceEvent)
{
	ASSERT(fence);
	HRESULT hres = fence->pDxFence->SetEventOnCompletion(value, fenceEvent);
	return SUCCEEDED(hres);
}
/************************************************************************/
// Utility functions
/************************************************************************/
ImageFormat::Enum getRecommendedSwapchainFormat(bool hintHDR)
{
	if (fnHookGetRecommendedSwapChainFormat)
		return fnHookGetRecommendedSwapChainFormat(hintHDR);
	else
		return ImageFormat::BGRA8;
}

bool isImageFormatSupported(ImageFormat::Enum format)
{
	//verifies that given image format is valid
	return gDX12FormatTranslator[format] != DXGI_FORMAT_UNKNOWN;
}
/************************************************************************/
// Execute Indirect Implementation
/************************************************************************/
D3D12_INDIRECT_ARGUMENT_TYPE util_to_dx_indirect_argument_type(IndirectArgumentType argType)
{
	D3D12_INDIRECT_ARGUMENT_TYPE res = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
	switch (argType)
	{
		case INDIRECT_DRAW: res = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW; break;
		case INDIRECT_DRAW_INDEX: res = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED; break;
		case INDIRECT_DISPATCH: res = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH; break;
		case INDIRECT_VERTEX_BUFFER: res = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW; break;
		case INDIRECT_INDEX_BUFFER: res = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW; break;
		case INDIRECT_CONSTANT: res = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT; break;
		case INDIRECT_CONSTANT_BUFFER_VIEW: res = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW; break;
		case INDIRECT_SHADER_RESOURCE_VIEW: res = D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW; break;
		case INDIRECT_UNORDERED_ACCESS_VIEW: res = D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW; break;
		case INDIRECT_DESCRIPTOR_TABLE: LOGERROR("Dx12 Doesn't support DescriptorTable in Indirect Command"); break;
		case INDIRECT_PIPELINE: LOGERROR("Dx12 Doesn't support the Pipeline in Indirect Command"); break;
	}
	return res;
}

void addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(pDesc->pArgDescs);

	CommandSignature* pCommandSignature = (CommandSignature*)conf_calloc(1, sizeof(*pCommandSignature));
	pCommandSignature->mDesc = *pDesc;

	bool change = false;
	// calculate size through arguement types
	uint32_t commandStride = 0;

	// temporary use
	D3D12_INDIRECT_ARGUMENT_DESC* argumentDescs =
		(D3D12_INDIRECT_ARGUMENT_DESC*)alloca(pDesc->mIndirectArgCount * sizeof(D3D12_INDIRECT_ARGUMENT_DESC));

	for (uint32_t i = 0; i < pDesc->mIndirectArgCount; ++i)
	{
		if (pDesc->pArgDescs[i].mType == INDIRECT_DESCRIPTOR_TABLE || pDesc->pArgDescs[i].mType == INDIRECT_PIPELINE)
		{
			LOGERROR("Dx12 Doesn't support DescriptorTable or Pipeline in Indirect Command");
		}

		argumentDescs[i].Type = util_to_dx_indirect_argument_type(pDesc->pArgDescs[i].mType);
		switch (argumentDescs[i].Type)
		{
			case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
				argumentDescs[i].Constant.RootParameterIndex = pDesc->pArgDescs[i].mRootParameterIndex;
				argumentDescs[i].Constant.DestOffsetIn32BitValues = 0;
				argumentDescs[i].Constant.Num32BitValuesToSet = pDesc->pArgDescs[i].mCount;
				commandStride += sizeof(UINT) * argumentDescs[i].Constant.Num32BitValuesToSet;
				change = true;
				break;
			case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
				argumentDescs[i].UnorderedAccessView.RootParameterIndex = pDesc->pArgDescs[i].mRootParameterIndex;
				commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
				change = true;
				break;
			case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
				argumentDescs[i].ShaderResourceView.RootParameterIndex = pDesc->pArgDescs[i].mRootParameterIndex;
				commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
				change = true;
				break;
			case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
				argumentDescs[i].ConstantBufferView.RootParameterIndex = pDesc->pArgDescs[i].mRootParameterIndex;
				commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
				change = true;
				break;
			case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
				argumentDescs[i].VertexBuffer.Slot = pDesc->pArgDescs[i].mRootParameterIndex;
				commandStride += sizeof(D3D12_VERTEX_BUFFER_VIEW);
				change = true;
				break;
			case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
				argumentDescs[i].VertexBuffer.Slot = pDesc->pArgDescs[i].mRootParameterIndex;
				commandStride += sizeof(D3D12_INDEX_BUFFER_VIEW);
				change = true;
				break;
			case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW: commandStride += sizeof(IndirectDrawArguments); break;
			case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED: commandStride += sizeof(IndirectDrawIndexArguments); break;
			case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH: commandStride += sizeof(IndirectDispatchArguments); break;
			default: ASSERT(false); break;
		}
	}

	if (change)
	{
		ASSERT(pDesc->pRootSignature);
	}

	commandStride = round_up(commandStride, 16);

	pCommandSignature->mDrawCommandStride = commandStride;
	pCommandSignature->mIndirectArgDescCounts = pDesc->mIndirectArgCount;

	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = pDesc->mIndirectArgCount;
	commandSignatureDesc.ByteStride = commandStride;
	// If running Linked Mode (SLI) create command signature for all nodes
	// #NOTE : In non SLI mode, mNodeCount will be 0 which sets nodeMask to default value
	commandSignatureDesc.NodeMask = util_calculate_shared_node_mask(pRenderer);
	HRESULT hres = pRenderer->pDxDevice->CreateCommandSignature(
		&commandSignatureDesc, change ? pDesc->pRootSignature->pDxRootSignature : NULL, IID_ARGS(&pCommandSignature->pDxCommandSignautre));
	ASSERT(SUCCEEDED(hres));

	*ppCommandSignature = pCommandSignature;
}

void removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
{
	UNREF_PARAM(pRenderer);
	SAFE_RELEASE(pCommandSignature->pDxCommandSignautre);
	SAFE_FREE(pCommandSignature);
}

void cmdExecuteIndirect(
	Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
	Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
	ASSERT(pCommandSignature);
	ASSERT(pIndirectBuffer);

#ifdef _DURANGO
	BufferBarrier bufferBarriers[] = { { pIndirectBuffer, RESOURCE_STATE_INDIRECT_ARGUMENT } };
	cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, false);
#endif
	if (!pCounterBuffer)
		pCmd->pDxCmdList->ExecuteIndirect(
			pCommandSignature->pDxCommandSignautre, maxCommandCount, pIndirectBuffer->pDxResource, bufferOffset, NULL, 0);
	else
		pCmd->pDxCmdList->ExecuteIndirect(
			pCommandSignature->pDxCommandSignautre, maxCommandCount, pIndirectBuffer->pDxResource, bufferOffset,
			pCounterBuffer->pDxResource, counterBufferOffset);
}
/************************************************************************/
// Query Heap Implementation
/************************************************************************/
void getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
	ASSERT(pQueue);
	ASSERT(pFrequency);

	UINT64 freq = 0;
	pQueue->pDxQueue->GetTimestampFrequency(&freq);
	*pFrequency = (double)freq;
}

void addQueryHeap(Renderer* pRenderer, const QueryHeapDesc* pDesc, QueryHeap** ppQueryHeap)
{
	QueryHeap* pQueryHeap = (QueryHeap*)conf_calloc(1, sizeof(*pQueryHeap));
	pQueryHeap->mDesc = *pDesc;

	D3D12_QUERY_HEAP_DESC desc = {};
	desc.Count = pDesc->mQueryCount;
	desc.NodeMask = util_calculate_node_mask(pRenderer, pDesc->mNodeIndex);
	desc.Type = util_to_dx_query_heap_type(pDesc->mType);
	pRenderer->pDxDevice->CreateQueryHeap(&desc, IID_ARGS(&pQueryHeap->pDxQueryHeap));

	*ppQueryHeap = pQueryHeap;
}

void removeQueryHeap(Renderer* pRenderer, QueryHeap* pQueryHeap)
{
	UNREF_PARAM(pRenderer);
	SAFE_RELEASE(pQueryHeap->pDxQueryHeap);
	SAFE_FREE(pQueryHeap);
}

void cmdBeginQuery(Cmd* pCmd, QueryHeap* pQueryHeap, QueryDesc* pQuery)
{
	D3D12_QUERY_TYPE type = util_to_dx_query_type(pQueryHeap->mDesc.mType);
	switch (type)
	{
		case D3D12_QUERY_TYPE_OCCLUSION: break;
		case D3D12_QUERY_TYPE_BINARY_OCCLUSION: break;
		case D3D12_QUERY_TYPE_TIMESTAMP: pCmd->pDxCmdList->EndQuery(pQueryHeap->pDxQueryHeap, type, pQuery->mIndex); break;
		case D3D12_QUERY_TYPE_PIPELINE_STATISTICS: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM1: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM2: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM3: break;
		default: break;
	}
}

void cmdEndQuery(Cmd* pCmd, QueryHeap* pQueryHeap, QueryDesc* pQuery)
{
	D3D12_QUERY_TYPE type = util_to_dx_query_type(pQueryHeap->mDesc.mType);
	switch (type)
	{
		case D3D12_QUERY_TYPE_OCCLUSION: break;
		case D3D12_QUERY_TYPE_BINARY_OCCLUSION: break;
		case D3D12_QUERY_TYPE_TIMESTAMP: pCmd->pDxCmdList->EndQuery(pQueryHeap->pDxQueryHeap, type, pQuery->mIndex); break;
		case D3D12_QUERY_TYPE_PIPELINE_STATISTICS: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM1: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM2: break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM3: break;
		default: break;
	}
}

void cmdResolveQuery(Cmd* pCmd, QueryHeap* pQueryHeap, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
{
	pCmd->pDxCmdList->ResolveQueryData(
		pQueryHeap->pDxQueryHeap, util_to_dx_query_type(pQueryHeap->mDesc.mType), startQuery, queryCount, pReadbackBuffer->pDxResource,
		startQuery * 8);
}
/************************************************************************/
// Memory Stats Implementation
/************************************************************************/
void calculateMemoryStats(Renderer* pRenderer, char** stats) { resourceAllocBuildStatsString(pRenderer->pResourceAllocator, stats, 0); }

void freeMemoryStats(Renderer* pRenderer, char* stats) { resourceAllocFreeStatsString(pRenderer->pResourceAllocator, stats); }
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	// note: USE_PIX isn't the ideal test because we might be doing a debug build where pix
	// is not installed, or a variety of other reasons. It should be a separate #ifdef flag?
#ifndef FORGE_JHABLE_EDITS_V01
#if defined(USE_PIX)
	//color is in B8G8R8X8 format where X is padding
	uint64_t color = packColorF32(r, g, b, 0 /*there is no alpha, that's padding*/);
	PIXBeginEvent(pCmd->pDxCmdList, color, pName);
#endif
#endif
}

void cmdEndDebugMarker(Cmd* pCmd)
{
#ifndef FORGE_JHABLE_EDITS_V01
#if defined(USE_PIX)
	PIXEndEvent(pCmd->pDxCmdList);
#endif
#endif
}

void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#ifndef FORGE_JHABLE_EDITS_V01
#if defined(USE_PIX)
	//color is in B8G8R8X8 format where X is padding
	uint64_t color = packColorF32(r, g, b, 0 /*there is no alpha, that's padding*/);
	PIXSetMarker(pCmd->pDxCmdList, color, pName);
#endif
#endif
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
	ASSERT(pRenderer);
	ASSERT(pBuffer);
	ASSERT(pName);

	size_t length = strlen(pName);

	ASSERT(length < MAX_PATH && "Name too long");

	wchar_t wName[MAX_PATH] = {};
	wName[strlen(pName)] = '\0';
	size_t numConverted = 0;
	mbstowcs_s(&numConverted, wName, pName, strlen(pName));
	pBuffer->pDxResource->SetName(wName);
}

void setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
	ASSERT(pRenderer);
	ASSERT(pTexture);
	ASSERT(pName);

	size_t length = strlen(pName);

	ASSERT(length < MAX_PATH && "Name too long");

	wchar_t wName[MAX_PATH] = {};
	wName[strlen(pName)] = '\0';
	size_t numConverted = 0;
	mbstowcs_s(&numConverted, wName, pName, strlen(pName));
	pTexture->pDxResource->SetName(wName);
}
/************************************************************************/
/************************************************************************/
#endif    // RENDERER_IMPLEMENTATION
#endif
#if defined(__cplusplus) && defined(ENABLE_RENDERER_RUNTIME_SWITCH)
}
#endif

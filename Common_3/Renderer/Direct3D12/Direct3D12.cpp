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

#ifdef DIRECT3D12
#define RENDERER_IMPLEMENTATION
#define MAX_FRAMES_IN_FLIGHT 3U

#ifdef _DURANGO
#include "..\..\..\CommonXBOXOne_3\OS\XBoxPrivateHeaders.h"
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

#include "Direct3D12Hooks.h"

#if ! defined(_WIN32)
#error "Windows is needed!"
#endif

//
// C++ is the only language supported by D3D12:
//     https://msdn.microsoft.com/en-us/library/windows/desktop/dn899120(v=vs.85).aspx
//
#if ! defined(__cplusplus)
#error "D3D12 requires C++! Sorry!"
#endif 

// Pull in minimal Windows headers
#if ! defined(NOMINMAX)
#define NOMINMAX
#endif
#if ! defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#if !defined(_DURANGO)
// Prefer Higher Performance GPU on switchable GPU systems
extern "C"
{
	__declspec(dllexport) DWORD	NvOptimusEnablement = 1;
	__declspec(dllexport) int	AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#include "../IMemoryAllocator.h"
#include "../../OS/Interfaces/IMemoryManager.h"

#if defined(__cplusplus) && defined(RENDERER_CPP_NAMESPACE)
namespace RENDERER_CPP_NAMESPACE {
#endif

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
		DXGI_FORMAT_R16_TYPELESS,
		DXGI_FORMAT_R16G16_TYPELESS,
		DXGI_FORMAT_UNKNOWN,							// ImageFormat::RGB8S not directly supported
		DXGI_FORMAT_R16G16B16A16_TYPELESS,
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
		DXGI_FORMAT_UNKNOWN, // INTZ = 60,	//	NVidia hack. Supported on all DX10+ HW
		//	XBox 360 specific fron buffer formats. NOt listed in other renderers. Please, add them when extend this structure.
		DXGI_FORMAT_UNKNOWN, // LE_XRGB8 = 61,
		DXGI_FORMAT_UNKNOWN, // LE_ARGB8 = 62,
		DXGI_FORMAT_UNKNOWN, // LE_X2RGB10 = 63,
		DXGI_FORMAT_UNKNOWN, // LE_A2RGB10 = 64,
		// compressed mobile forms
		DXGI_FORMAT_UNKNOWN, // ETC1 = 65,	//	RGB
		DXGI_FORMAT_UNKNOWN, // ATC = 66,	//	RGB
		DXGI_FORMAT_UNKNOWN, // ATCA = 67,	//	RGBA, explicit alpha
		DXGI_FORMAT_UNKNOWN, // ATCI = 68,	//	RGBA, interpolated alpha
		DXGI_FORMAT_UNKNOWN, // RAWZ = 69, //depth only, Nvidia (requires recombination of data) //FIX IT: PS3 as well?
		DXGI_FORMAT_UNKNOWN, // DF16 = 70, //depth only, Intel/AMD
		DXGI_FORMAT_UNKNOWN, // STENCILONLY = 71, // stencil ony usage
		DXGI_FORMAT_UNKNOWN, // GNF_BC1 = 72,
		DXGI_FORMAT_UNKNOWN, // GNF_BC2 = 73,
		DXGI_FORMAT_UNKNOWN, // GNF_BC3 = 74,
		DXGI_FORMAT_UNKNOWN, // GNF_BC4 = 75,
		DXGI_FORMAT_UNKNOWN, // GNF_BC5 = 76,
		DXGI_FORMAT_UNKNOWN, // GNF_BC6 = 77,
		DXGI_FORMAT_UNKNOWN, // GNF_BC7 = 78,
		// Reveser Form
		DXGI_FORMAT_B8G8R8A8_UNORM, // BGRA8 = 79,
		// Extend for DXGI
		DXGI_FORMAT_UNKNOWN, // X8D24PAX32 = 80,
		DXGI_FORMAT_UNKNOWN, // S8 = 81,
		DXGI_FORMAT_UNKNOWN, // D16S8 = 82,
		DXGI_FORMAT_UNKNOWN, // D32S8 = 83,
	};
	const DXGI_FORMAT gDX12FormatTranslator[] = {
		DXGI_FORMAT_UNKNOWN,							// ImageFormat::None
		DXGI_FORMAT_R8_UNORM,							// ImageFormat::R8
		DXGI_FORMAT_R8G8_UNORM,							// ImageFormat::RG8
		DXGI_FORMAT_UNKNOWN,							// ImageFormat::RGB8 not directly supported
		DXGI_FORMAT_R8G8B8A8_UNORM,						// ImageFormat::RGBA8
		DXGI_FORMAT_R16_UNORM,							// ImageFormat::R16
		DXGI_FORMAT_R16G16_UNORM,						// ImageFormat::RG16
		DXGI_FORMAT_UNKNOWN,							// ImageFormat::RGB16 not directly supported
		DXGI_FORMAT_R16G16B16A16_UNORM,					// ImageFormat::RGBA16
		DXGI_FORMAT_R8_SNORM,							// ImageFormat::R8S
		DXGI_FORMAT_R8G8_SNORM,							// ImageFormat::RG8S
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
		DXGI_FORMAT_UNKNOWN, // INTZ = 60,	//	NVidia hack. Supported on all DX10+ HW
		//	XBox 360 specific fron buffer formats. NOt listed in other renderers. Please, add them when extend this structure.
		DXGI_FORMAT_UNKNOWN, // LE_XRGB8 = 61,
		DXGI_FORMAT_UNKNOWN, // LE_ARGB8 = 62,
		DXGI_FORMAT_UNKNOWN, // LE_X2RGB10 = 63,
		DXGI_FORMAT_UNKNOWN, // LE_A2RGB10 = 64,
		// compressed mobile forms
		DXGI_FORMAT_UNKNOWN, // ETC1 = 65,	//	RGB
		DXGI_FORMAT_UNKNOWN, // ATC = 66,	//	RGB
		DXGI_FORMAT_UNKNOWN, // ATCA = 67,	//	RGBA, explicit alpha
		DXGI_FORMAT_UNKNOWN, // ATCI = 68,	//	RGBA, interpolated alpha
		DXGI_FORMAT_UNKNOWN, // RAWZ = 69, //depth only, Nvidia (requires recombination of data) //FIX IT: PS3 as well?
		DXGI_FORMAT_UNKNOWN, // DF16 = 70, //depth only, Intel/AMD
		DXGI_FORMAT_UNKNOWN, // STENCILONLY = 71, // stencil ony usage
		DXGI_FORMAT_UNKNOWN, // GNF_BC1 = 72,
		DXGI_FORMAT_UNKNOWN, // GNF_BC2 = 73,
		DXGI_FORMAT_UNKNOWN, // GNF_BC3 = 74,
		DXGI_FORMAT_UNKNOWN, // GNF_BC4 = 75,
		DXGI_FORMAT_UNKNOWN, // GNF_BC5 = 76,
		DXGI_FORMAT_UNKNOWN, // GNF_BC6 = 77,
		DXGI_FORMAT_UNKNOWN, // GNF_BC7 = 78,
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
		D3D12_COMMAND_QUEUE_PRIORITY_HIGH
#ifndef _DURANGO
		,D3D12_COMMAND_QUEUE_PRIORITY_GLOBAL_REALTIME
#endif
	};
	// =================================================================================================
	// IMPLEMENTATION
	// =================================================================================================

#if defined(RENDERER_IMPLEMENTATION)

#ifndef _DURANGO
#include <d3dcompiler.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#endif

#define SAFE_FREE(p_var)	\
    if (p_var) {			\
       conf_free(p_var);			\
    }

#if defined(__cplusplus)  
#define DECLARE_ZERO(type, var) \
            type var = {};
#else
#define DECLARE_ZERO(type, var) \
            type var = {0};                        
#endif

#define SAFE_RELEASE(p_var) \
    if (p_var) {               \
       p_var->Release();               \
       p_var = NULL;                   \
    }

	// Internal utility functions (may become external one day)
	uint64_t						util_dx_determine_storage_counter_offset(uint64_t buffer_size);
	DXGI_FORMAT						util_to_dx_image_format_typeless(ImageFormat::Enum format);
	DXGI_FORMAT						util_to_dx_uav_format(DXGI_FORMAT defaultFormat);
	DXGI_FORMAT						util_to_dx_dsv_format(DXGI_FORMAT defaultFormat);
	DXGI_FORMAT						util_to_dx_srv_format(DXGI_FORMAT defaultFormat);
	DXGI_FORMAT						util_to_dx_stencil_format(DXGI_FORMAT defaultFormat);
	DXGI_FORMAT						util_to_dx_image_format(ImageFormat::Enum format, bool srgb);
	DXGI_FORMAT						util_to_dx_swapchain_format(ImageFormat::Enum format);
	D3D12_SHADER_VISIBILITY			util_to_dx_shader_visibility(ShaderStage stages);
	D3D12_DESCRIPTOR_RANGE_TYPE		util_to_dx_descriptor_range(DescriptorType type);
	D3D12_RESOURCE_STATES			util_to_dx_resource_state(ResourceState state);
	D3D12_FILTER					util_to_dx_filter(FilterType minFilter, FilterType magFilter, MipMapMode  mipMapMode);
	D3D12_TEXTURE_ADDRESS_MODE		util_to_dx_texture_address_mode(AddressMode addressMode);
	D3D12_PRIMITIVE_TOPOLOGY_TYPE	util_to_dx_primitive_topology_type(PrimitiveTopology topology);

	//
	// internal functions start with a capital letter / API starts with a small letter

	// internal functions are capital first letter and capital letter of the next word

	// Internal init functions
	void AddDevice(Renderer* pRenderer);
	void RemoveDevice(Renderer* pRenderer);

	// Functions points for functions that need to be loaded
	PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER           fnD3D12CreateRootSignatureDeserializer = NULL;
	PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE           fnD3D12SerializeVersionedRootSignature = NULL;
	PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER fnD3D12CreateVersionedRootSignatureDeserializer = NULL;

	// Declare hooks for platform specific behavior
	PFN_HOOK_ADD_DESCRIPTIOR_HEAP				fnHookAddDescriptorHeap = NULL;
	PFN_HOOK_POST_INIT_RENDERER					fnHookPostInitRenderer = NULL;
	PFN_HOOK_ADD_BUFFER							fnHookAddBuffer = NULL;
	PFN_HOOK_ENABLE_DEBUG_LAYER					fnHookEnableDebugLayer = NULL;
	PFN_HOOK_HEAP_DESC							fnHookHeapDesc = NULL;
	PFN_HOOK_GET_RECOMMENDED_SWAP_CHAIN_FORMAT	fnHookGetRecommendedSwapChainFormat = NULL;
	PFN_HOOK_MODIFY_SWAP_CHAIN_DESC				fnHookModifySwapChainDesc = NULL;
	PFN_HOOK_GET_SWAP_CHAIN_IMAGE_INDEX			fnHookGetSwapChainImageIndex = NULL;
	PFN_HOOK_SHADER_COMPILE_FLAGS               fnHookShaderCompileFlags = NULL;
	PFN_HOOK_RESOURCE_ALLOCATION_INFO           fnHookResourceAllocationInfo = NULL;
	PFN_HOOK_SPECIAL_BUFFER_ALLOCATION			fnHookSpecialBufferAllocation = NULL;
	PFN_HOOK_SPECIAL_TEXTURE_ALLOCATION			fnHookSpecialTextureAllocation = NULL;

	/************************************************************************/
	// Dynamic Memory Allocator Defines
	/************************************************************************/
#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)
	/************************************************************************/
	// Dynamic Memory Allocator Structures
	/************************************************************************/
	typedef struct DynamicMemoryAllocator
	{
		/// Size of mapped resources to be created
		uint64_t mSize;
		/// Current offset in the used page
		uint64_t mCurrentPos;

		Buffer* pBuffer;
		D3D12_GPU_VIRTUAL_ADDRESS mGpuVirtualAddress;

		Mutex* pAllocationMutex;
	} DynamicMemoryAllocator;
	/************************************************************************/
	// Dynamic Memory Allocator Implementation
	/************************************************************************/
	void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
	void removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
	void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture);
	void removeTexture(Renderer* pRenderer, Texture* pTexture);

	void add_dynamic_memory_allocator(Renderer* pRenderer, uint64_t size, DynamicMemoryAllocator** ppAllocator)
	{
		ASSERT(pRenderer);
		ASSERT(pRenderer->pDevice);

		DynamicMemoryAllocator* pAllocator = (DynamicMemoryAllocator*)conf_calloc(1, sizeof(*pAllocator));
		pAllocator->mCurrentPos = 0;
		pAllocator->mSize = size;
		pAllocator->pAllocationMutex = conf_placement_new<Mutex>(conf_calloc(1, sizeof(Mutex)));

		BufferDesc bufferDesc = {};
		bufferDesc.mUsage = BUFFER_USAGE_UPLOAD;
		bufferDesc.mSize = pAllocator->mSize;
		bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
		addBuffer(pRenderer, &bufferDesc, &pAllocator->pBuffer);

		pAllocator->mGpuVirtualAddress = pAllocator->pBuffer->pDxResource->GetGPUVirtualAddress();
		pAllocator->pBuffer->pDxResource->Map(0, NULL, &pAllocator->pBuffer->pCpuMappedAddress);
		ASSERT(pAllocator->pBuffer->pCpuMappedAddress);

		*ppAllocator = pAllocator;
	}

	void remove_dynamic_memory_allocator(Renderer* pRenderer, DynamicMemoryAllocator* pAllocator)
	{
		ASSERT(pAllocator);

		pAllocator->pBuffer->pDxResource->Unmap(0, NULL);
		removeBuffer(pRenderer, pAllocator->pBuffer);

		pAllocator->pAllocationMutex->~Mutex();
		conf_free(pAllocator->pAllocationMutex);

		SAFE_FREE(pAllocator);
	}

	void reset_dynamic_memory_allocator(DynamicMemoryAllocator* pAllocator)
	{
		ASSERT(pAllocator);
		pAllocator->mCurrentPos = 0;
	}

	void consume_dynamic_memory_allocator_lock_free(DynamicMemoryAllocator* p_linear_allocator, uint64_t size, void** ppCpuAddress, D3D12_GPU_VIRTUAL_ADDRESS* pGpuAddress)
	{
		if (p_linear_allocator->mCurrentPos + size > p_linear_allocator->mSize)
			reset_dynamic_memory_allocator(p_linear_allocator);

		*ppCpuAddress = (uint8_t*)p_linear_allocator->pBuffer->pCpuMappedAddress + p_linear_allocator->mCurrentPos;
		*pGpuAddress = p_linear_allocator->mGpuVirtualAddress + p_linear_allocator->mCurrentPos;

		// Increment position by multiple of 256 to use CBVs in same heap as other buffers
		p_linear_allocator->mCurrentPos += round_up_64(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	}

	void consume_dynamic_memory_allocator(DynamicMemoryAllocator* p_linear_allocator, uint64_t size, void** ppCpuAddress, D3D12_GPU_VIRTUAL_ADDRESS* pGpuAddress)
	{
		MutexLock lock(*p_linear_allocator->pAllocationMutex);

		consume_dynamic_memory_allocator_lock_free(p_linear_allocator, size, ppCpuAddress, pGpuAddress);
	}
	/************************************************************************/
	// Descriptor Heap Defines
	/************************************************************************/
	typedef struct DescriptorHeapProperties
	{
		uint32_t mMaxDescriptors;
		D3D12_DESCRIPTOR_HEAP_FLAGS mFlags;
	} DescriptorHeapProperties;

	DescriptorHeapProperties gCpuDescriptorHeapProperties[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] =
	{
		{ 1024 * 256,	D3D12_DESCRIPTOR_HEAP_FLAG_NONE },	// CBV SRV UAV
		{ 2048,		D3D12_DESCRIPTOR_HEAP_FLAG_NONE },	// Sampler
		{ 128,		D3D12_DESCRIPTOR_HEAP_FLAG_NONE },	// RTV
		{ 128,		D3D12_DESCRIPTOR_HEAP_FLAG_NONE },	// DSV
	};

	DescriptorHeapProperties gGpuDescriptorHeapProperties[2] =
	{
		{ 1024 * 256,	D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE },	// CBV SRV UAV
		{ 2048,		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE },	// Sampler
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
		Mutex* pAllocationMutex;

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
	static void add_descriptor_heap(Renderer* pRenderer, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, uint32_t numDescriptors, DescriptorStoreHeap** ppDescHeap)
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
		Desc.NodeMask = 1;

		HRESULT hres = pRenderer->pDevice->CreateDescriptorHeap(&Desc, IID_ARGS(&pHeap->pCurrentHeap));
		ASSERT(SUCCEEDED(hres));

		pHeap->mNumDescriptors = numDescriptors;
		pHeap->mType = type;
		pHeap->mStartCpuHandle = pHeap->pCurrentHeap->GetCPUDescriptorHandleForHeapStart();
		pHeap->mStartGpuHandle = pHeap->pCurrentHeap->GetGPUDescriptorHandleForHeapStart();
		pHeap->mDescriptorSize = pRenderer->pDevice->GetDescriptorHandleIncrementSize(type);

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

	static D3D12_CPU_DESCRIPTOR_HANDLE add_cpu_descriptor_handles(DescriptorStoreHeap* pHeap, uint32_t numDescriptors)
	{
		int result = -1;
		MutexLock lockGuard(*pHeap->pAllocationMutex);

		tinystl::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles;

		for (uint32_t i = 0; i < pHeap->mNumDescriptors / 32; ++i)
		{
			const uint32_t flag = pHeap->flags[i];
			if (flag == 0xffffffff)
			{
				for (D3D12_CPU_DESCRIPTOR_HANDLE& handle : handles)
				{
					uint32_t id = (uint32_t)((handle.ptr - pHeap->mStartCpuHandle.ptr) / pHeap->mDescriptorSize);
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

					if (handles.getCount() == numDescriptors)
					{
						return handles.front();
					}
				}
			}
		}

		ASSERT(result != -1 && "Out of descriptors");
		return handles.front();
	}

	static void add_gpu_descriptor_handles(DescriptorStoreHeap* pHeap, D3D12_CPU_DESCRIPTOR_HANDLE* pStartCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* pStartGpuHandle, uint32_t numDescriptors)
	{
		int result = -1;
		MutexLock lockGuard(*pHeap->pAllocationMutex);

		tinystl::vector <tinystl::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> > handles;

		for (uint32_t i = 0; i < pHeap->mNumDescriptors / 32; ++i)
		{
			const uint32_t flag = pHeap->flags[i];
			if (flag == 0xffffffff)
			{
				for (tinystl::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>& handle : handles)
				{
					uint32_t id = (uint32_t)((handle.first.ptr - pHeap->mStartCpuHandle.ptr) / pHeap->mDescriptorSize);
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

					if (handles.getCount() == numDescriptors)
					{
						*pStartCpuHandle = handles.front().first;
						*pStartGpuHandle = handles.front().second;
						return;
					}
				}
			}
		}

		ASSERT(result != -1 && "Out of descriptors");
		*pStartCpuHandle = handles.front().first;
		*pStartGpuHandle = handles.front().second;
	}

	static void remove_gpu_descriptor_handles(DescriptorStoreHeap* pHeap, D3D12_GPU_DESCRIPTOR_HANDLE* startHandle, uint64_t numDescriptors)
	{
		MutexLock lockGuard(*pHeap->pAllocationMutex);

		for (uint32_t idx = 0; idx < numDescriptors; ++idx)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE handle = { startHandle->ptr + idx * pHeap->mDescriptorSize };
			uint32_t id = (uint32_t)((handle.ptr - pHeap->mStartGpuHandle.ptr) / pHeap->mDescriptorSize);

			const uint32_t i = id / 32;
			const uint32_t mask = ~(1 << (id % 32));
			pHeap->flags[i] &= mask;
		}
	}

	static void remove_cpu_descriptor_handles(DescriptorStoreHeap* pHeap, D3D12_CPU_DESCRIPTOR_HANDLE* startHandle, uint64_t numDescriptors)
	{
		MutexLock lockGuard(*pHeap->pAllocationMutex);

		for (uint32_t idx = 0; idx < numDescriptors; ++idx)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE handle = { startHandle->ptr + idx * pHeap->mDescriptorSize };
			uint32_t id = (uint32_t)((handle.ptr - pHeap->mStartCpuHandle.ptr) / pHeap->mDescriptorSize);

			const uint32_t i = id / 32;
			const uint32_t mask = ~(1 << (id % 32));
			pHeap->flags[i] &= mask;
		}
	}
	/************************************************************************/
	// Descriptor Manager Implementation
	/************************************************************************/
	/// Descriptor table structure holding the native descriptor set handle
	typedef struct DescriptorTable {
		/// Handle to the start of the cbv_srv_uav descriptor table in the gpu visible cbv_srv_uav heap
		D3D12_CPU_DESCRIPTOR_HANDLE		mBaseViewCpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE		mBaseViewGpuHandle;
		/// Handle to the start of the sampler descriptor table in the gpu visible sampler heap
		D3D12_CPU_DESCRIPTOR_HANDLE		mBaseSamplerCpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE		mBaseSamplerGpuHandle;
	} DescriptorTable;

	using DescriptorTableMap = tinystl::unordered_map<uint64_t, DescriptorTable>;
	using ConstDescriptorTableMapIterator = tinystl::unordered_map<uint64_t, DescriptorTable>::const_iterator;
	using DescriptorTableMapNode = tinystl::unordered_hash_node<uint64_t, DescriptorTable>;
	using DescriptorNameToIndexMap = tinystl::unordered_map<uint32_t, uint32_t>;

	typedef struct DescriptorManager
	{
		/// The root signature associated with this descriptor manager
		RootSignature*					pRootSignature;
		/// Array of flags to check whether a descriptor table of the update frequency is already bound to avoid unnecessary rebinding of descriptor tables
		bool							mBoundTables[DESCRIPTOR_UPDATE_FREQ_COUNT];
		/// Array of view descriptor handles per update frequency to be copied into the gpu visible view heap
		D3D12_CPU_DESCRIPTOR_HANDLE*	pViewDescriptorHandles[DESCRIPTOR_UPDATE_FREQ_COUNT];
		/// Array of sampler descriptor handles per update frequency to be copied into the gpu visible sampler heap
		D3D12_CPU_DESCRIPTOR_HANDLE*	pSamplerDescriptorHandles[DESCRIPTOR_UPDATE_FREQ_COUNT];
		/// Triple buffered Hash map to check if a descriptor table with a descriptor hash already exists to avoid redundant copy descriptors operations
		DescriptorTableMap				mStaticDescriptorTableMap[MAX_FRAMES_IN_FLIGHT];
		/// Triple buffered array of number of descriptor tables allocated per update frequency
		/// Only used for recording stats
		uint32_t						mDescriptorTableCount[MAX_FRAMES_IN_FLIGHT][DESCRIPTOR_UPDATE_FREQ_COUNT];

		Cmd*							pCurrentCmd;
		uint32_t						mFrameIdx;
	} DescriptorManager;

	Mutex gDescriptorMutex;

	void add_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature, DescriptorManager** ppManager)
	{
		DescriptorManager* pManager = (DescriptorManager*)conf_calloc(1, sizeof(*pManager));
		pManager->pRootSignature = pRootSignature;
		pManager->mFrameIdx = (uint32_t)-1;

		const uint32_t setCount = DESCRIPTOR_UPDATE_FREQ_COUNT;

		// Fill the descriptor handles with null descriptors
		for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
		{
			const DescriptorSetLayout* pViewLayout = &pRootSignature->pViewTableLayouts[setIndex];
			const DescriptorSetLayout* pSamplerLayout = &pRootSignature->pSamplerTableLayouts[setIndex];
			const uint32_t viewCount = pViewLayout->mDescriptorCount;
			const uint32_t samplerCount = pSamplerLayout->mDescriptorCount;
			const uint32_t descCount = viewCount + samplerCount;

			if (viewCount)
			{
				pManager->pViewDescriptorHandles[setIndex] = (D3D12_CPU_DESCRIPTOR_HANDLE*)conf_calloc(pViewLayout->mCumulativeDescriptorCount, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));

				for (uint32_t i = 0; i < viewCount; ++i)
				{
					const DescriptorInfo* pDesc = &pRootSignature->pDescriptors[pViewLayout->pDescriptorIndices[i]];
					DescriptorType type = pDesc->mDesc.type;
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
					default:
						break;
					}
				}
			}

			if (samplerCount)
			{
				pManager->pSamplerDescriptorHandles[setIndex] = (D3D12_CPU_DESCRIPTOR_HANDLE*)conf_calloc(pSamplerLayout->mCumulativeDescriptorCount, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));
				for (uint32_t i = 0; i < samplerCount; ++i)
				{
					pManager->pSamplerDescriptorHandles[setIndex][i] = pRenderer->mSamplerNullDescriptor;
				}
			}
		}

		*ppManager = pManager;
	}

	void remove_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature, DescriptorManager* pManager)
	{
    UNREF_PARAM(pRenderer);
		const uint32_t setCount = DESCRIPTOR_UPDATE_FREQ_COUNT;
		const uint32_t frameCount = MAX_FRAMES_IN_FLIGHT;

		for (uint32_t frameIdx = 0; frameIdx < frameCount; ++frameIdx)
		{
			pManager->mStaticDescriptorTableMap[frameIdx].~DescriptorTableMap();
		}

		// Free staging data tables
		for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
		{
			const DescriptorSetLayout* pViewLayout = &pRootSignature->pViewTableLayouts[setIndex];
			const DescriptorSetLayout* pSamplerLayout = &pRootSignature->pSamplerTableLayouts[setIndex];
			const uint32_t viewCount = pViewLayout->mDescriptorCount;
			const uint32_t samplerCount = pSamplerLayout->mDescriptorCount;
			const uint32_t descCount = viewCount + samplerCount;

			if (viewCount)
			{
				SAFE_FREE(pManager->pViewDescriptorHandles[setIndex]);
			}
			if (samplerCount)
			{
				SAFE_FREE(pManager->pSamplerDescriptorHandles[setIndex]);
			}
		}

		SAFE_FREE(pManager);
	}

	// This function returns the descriptor manager belonging to this thread
	// If a descriptor manager does not exist for this thread, a new one is created
	// With this approach we make sure that descriptor binding is thread safe and lock conf_free at the same time
	DescriptorManager* get_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature)
	{
		tinystl::unordered_hash_node<ThreadID, DescriptorManager*>* pNode = pRootSignature->pDescriptorManagerMap.find(Thread::GetCurrentThreadID()).node;
		if (pNode == NULL)
		{
			// Only need a lock when creating a new descriptor manager for this thread
			MutexLock lock(gDescriptorMutex);
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

#define MAX_DYNAMIC_VIEW_DESCRIPTORS_PER_FRAME gGpuDescriptorHeapProperties[0].mMaxDescriptors / 16
#define MAX_DYNAMIC_SAMPLER_DESCRIPTORS_PER_FRAME gGpuDescriptorHeapProperties[1].mMaxDescriptors / 16

	void cmdBindDescriptors(Cmd* pCmd, RootSignature* pRootSignature, uint32_t numDescriptors, DescriptorData* pDescParams)
	{
		Renderer* pRenderer = pCmd->pCmdPool->pRenderer;
		const uint32_t setCount = DESCRIPTOR_UPDATE_FREQ_COUNT;
		DescriptorManager* pm = get_descriptor_manager(pRenderer, pRootSignature);

		for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
		{
			// Reset other data
			pm->mBoundTables[setIndex] = true;
		}

		// Compare the currently bound root signature with the root signature of the descriptor manager
		// If these values dont match, we must bind the root signature of the descriptor manager
		// If the values match, no op is required
		if (pCmd->pBoundRootSignature != pRootSignature)
		{
			if (pm->pCurrentCmd != pCmd)
			{
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
		uint64_t pHash[setCount] = {};
		for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
			pHash[setIndex] = 0;

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

			uint32_t descIndex = ~0u;
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
						pRootSignature->pRootConstantLayouts[pDesc->mIndexInParent].mRootIndex,
						pDesc->mDesc.size, pParam->pRootConstant, 0);
				}
				else if (pRootSignature->mPipelineType == PIPELINE_TYPE_GRAPHICS)
				{
					pCmd->pDxCmdList->SetGraphicsRoot32BitConstants(
						pRootSignature->pRootConstantLayouts[pDesc->mIndexInParent].mRootIndex,
						pDesc->mDesc.size, pParam->pRootConstant, 0);
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
					cbv = pParam->ppBuffers[0]->mDxCbvDesc.BufferLocation + pParam->mOffset;
				}
				// If this descriptor is a root constant which was converted to a root cbv, use the internal ring buffer
				else if (pDesc->mDesc.type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
				{
					if (!pCmd->pRootConstantRingBuffer)
					{
						// 4KB ring buffer should be enough since size of root constant data is usually pretty small (< 32 bytes)
						addUniformRingBuffer(pRenderer, 4000U, &pCmd->pRootConstantRingBuffer);
					}
					uint32_t size = pDesc->mDesc.size * sizeof(uint32_t);
					UniformBufferOffset offset = getUniformBufferOffset(pCmd->pRootConstantRingBuffer, size);
					memcpy((uint8_t*)offset.pUniformBuffer->pCpuMappedAddress + offset.mOffset, pParam->pRootConstant, size);
					cbv = offset.pUniformBuffer->mDxCbvDesc.BufferLocation + offset.mOffset;
				}

				if (pRootSignature->mPipelineType == PIPELINE_TYPE_COMPUTE)
				{
					pCmd->pDxCmdList->SetComputeRootConstantBufferView(
						pRootSignature->pRootDescriptorLayouts[setIndex].pRootIndices[pDesc->mIndexInParent],
						cbv);
				}
				else if (pRootSignature->mPipelineType == PIPELINE_TYPE_GRAPHICS)
				{
					pCmd->pDxCmdList->SetGraphicsRootConstantBufferView(
						pRootSignature->pRootDescriptorLayouts[setIndex].pRootIndices[pDesc->mIndexInParent],
						cbv);
				}
				continue;
			}

			// Unbind current descriptor table so we can bind a new one
			pm->mBoundTables[setIndex] = false;

			DescriptorType type = pDesc->mDesc.type;
			switch (type)
			{
			case DESCRIPTOR_TYPE_SAMPLER:
				if (pDesc->mIndexInParent == -1)
				{
					LOGERRORF("Trying to bind a static sampler (%s). All static samplers must be bound in addRootSignature through RootSignatureDesc::mStaticSamplers", pParam->pName);
					continue;
				}
				if (!pParam->ppSamplers)
				{
					LOGERRORF("Sampler descriptor (%s) is NULL", pParam->pName);
					return;
				}
				for (uint32_t j = 0; j < pParam->mCount; ++j)
				{
					if (!pParam->ppSamplers[j]) {
						LOGERRORF("Sampler descriptor (%s) at array index (%u) is NULL", pParam->pName, j);
						return;
					}
					pHash[setIndex] = tinystl::hash_state(&pParam->ppSamplers[j]->mSamplerId, 1, pHash[setIndex]);
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
				Texture** ppTextures = pParam->ppTextures;
				for (uint32_t j = 0; j < pParam->mCount; ++j)
				{
#ifdef _DEBUG
					if (!pParam->ppTextures[j])
					{
						LOGERRORF("Texture descriptor (%s) at array index (%u) is NULL", pParam->pName, j);
						return;
					}
#endif

					//if (j < pParam->mCount - 1)
					//	_mm_prefetch((char*)&ppTextures[j]->mTextureId, _MM_HINT_T0);
					Texture* tex = ppTextures[j];
					pHash[setIndex] = tinystl::hash_state(&tex->mTextureId, 1, pHash[setIndex]);
					handlePtr[j] = tex->mDxSrvHandle;
					//pm->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = tex->mDxSrvHandle;
				}
				break;
			}
			case DESCRIPTOR_TYPE_RW_TEXTURE:
				if (!pParam->ppTextures)
				{
					LOGERRORF("Texture descriptor (%s) is NULL", pParam->pName);
					return;
				}
				for (uint32_t j = 0; j < pParam->mCount; ++j)
				{
					if (!pParam->ppBuffers[j])
					{
						LOGERRORF("Texture descriptor (%s) at array index (%u) is NULL", pParam->pName, j);
						return;
					}
					pHash[setIndex] = tinystl::hash_state(&pParam->ppTextures[j]->mTextureId, 1, pHash[setIndex]);
					pm->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = pParam->ppTextures[j]->mDxUavHandle;
#ifdef _DURANGO
					TextureBarrier textureBarriers[] = {
						{ pParam->ppTextures[j], RESOURCE_STATE_UNORDERED_ACCESS }
					};
					cmdResourceBarrier(pCmd, 0, NULL, 1, textureBarriers, true);
#endif
				}
				break;
			case DESCRIPTOR_TYPE_BUFFER:
				if (!pParam->ppBuffers)
				{
					LOGERRORF("Buffer descriptor (%s) is NULL", pParam->pName);
					return;
				}
				for (uint32_t j = 0; j < pParam->mCount; ++j)
				{
					if (!pParam->ppBuffers[j])
					{
						LOGERRORF("Buffer descriptor (%s) at array index (%u) is NULL", pParam->pName, j);
						return;
					}
					pHash[setIndex] = tinystl::hash_state(&pParam->ppBuffers[j]->mBufferId, 1, pHash[setIndex]);
					pm->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = pParam->ppBuffers[j]->mDxSrvHandle;
#ifdef _DURANGO
					ResourceState state = RESOURCE_STATE_SHADER_RESOURCE;

					if (pCmd->pCmdPool->mCmdPoolDesc.mCmdPoolType != CMD_POOL_DIRECT)
					{
						state = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
					}

					BufferBarrier bufferBarriers[] = {
						{ pParam->ppBuffers[j], state }
					};
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
				for (uint32_t j = 0; j < pParam->mCount; ++j)
				{
					if (!pParam->ppBuffers[j])
					{
						LOGERRORF("Buffer descriptor (%s) at array index (%u) is NULL", pParam->pName, j);
						return;
					}
					pHash[setIndex] = tinystl::hash_state(&pParam->ppBuffers[j]->mBufferId, 1, pHash[setIndex]);
					pm->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = pParam->ppBuffers[j]->mDxUavHandle;
#ifdef _DURANGO
					BufferBarrier bufferBarriers[] = {
						{ pParam->ppBuffers[j], RESOURCE_STATE_UNORDERED_ACCESS }
					};
					cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, true);
#endif
				}
				break;
			case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				if (!pParam->ppBuffers)
				{
					LOGERRORF("Buffer descriptor (%s) is NULL", pParam->pName);
					return;
				}
				for (uint32_t j = 0; j < pParam->mCount; ++j)
				{
					if (!pParam->ppBuffers[j])
					{
						LOGERRORF("Buffer descriptor (%s) at array index (%u) is NULL", pParam->pName, j);
						return;
					}
					pHash[setIndex] = tinystl::hash_state(&pParam->ppBuffers[j]->mBufferId, 1, pHash[setIndex]);
					pm->pViewDescriptorHandles[setIndex][pDesc->mHandleIndex + j] = pParam->ppBuffers[j]->mDxCbvHandle;
				}
				break;
			default:
				break;
			}
		}
		
#ifdef _DURANGO
		cmdFlushBarriers(pCmd);
#endif


		for (uint32_t setIndex = 0; setIndex < setCount; ++setIndex)
		{
			uint32_t descCount = pRootSignature->pViewTableLayouts[setIndex].mDescriptorCount +
				pRootSignature->pSamplerTableLayouts[setIndex].mDescriptorCount;
			uint32_t samplerCount = pRootSignature->pSamplerTableLayouts[setIndex].mCumulativeDescriptorCount;
			uint32_t viewCount = pRootSignature->pViewTableLayouts[setIndex].mCumulativeDescriptorCount;

			if (descCount && !pm->mBoundTables[setIndex])
			{
				DescriptorTable descTable = {};

				if (setIndex == DESCRIPTOR_UPDATE_FREQ_NONE)
				{
					// Search for the generated hash of descriptors in the descriptor table map
					// If the hash already exists, it means we have already created a descriptor table with the input descriptors
					// Now we just bind that descriptor table and no other op is required
					ConstDescriptorTableMapIterator it = pm->mStaticDescriptorTableMap[pm->mFrameIdx].find(pHash[setIndex]);
					if (it.node)
					{
						descTable = it.node->second;
					}
					// If the given hash does not exist, we create a new descriptor table and insert it into the descriptor table map
					else
					{
						if (viewCount)
							add_gpu_descriptor_handles(pRenderer->pCbvSrvUavHeap, &descTable.mBaseViewCpuHandle, &descTable.mBaseViewGpuHandle, viewCount);
						if (samplerCount)
							add_gpu_descriptor_handles(pRenderer->pSamplerHeap, &descTable.mBaseSamplerCpuHandle, &descTable.mBaseSamplerGpuHandle, samplerCount);
						// Copy the descriptor handles from the cpu view heap to the gpu view heap
						// The source handles pm->pViewDescriptors are collected at the same time when we hashed the descriptors
						for (uint32_t i = 0; i < viewCount; ++i)
							pRenderer->pDevice->CopyDescriptorsSimple(1,
							{ descTable.mBaseViewCpuHandle.ptr + i * pRenderer->pCbvSrvUavHeap->mDescriptorSize },
								pm->pViewDescriptorHandles[setIndex][i], pRenderer->pCbvSrvUavHeap->mType);

						// Copy the descriptor handles from the cpu sampler heap to the gpu sampler heap
						// The source handles pm->pSamplerDescriptors are collected at the same time when we hashed the descriptors
						for (uint32_t i = 0; i < samplerCount; ++i)
							pRenderer->pDevice->CopyDescriptorsSimple(1,
							{ descTable.mBaseSamplerCpuHandle.ptr + i * pRenderer->pSamplerHeap->mDescriptorSize },
								pm->pSamplerDescriptorHandles[setIndex][i], pRenderer->pSamplerHeap->mType);

						pm->mStaticDescriptorTableMap[pm->mFrameIdx].insert({ pHash[setIndex], descTable }).first.node->second;
					}
				}
				// Dynamic descriptors
				else
				{
					if (viewCount)
					{
						if (pCmd->mViewCpuHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_NULL)
							add_gpu_descriptor_handles(pRenderer->pCbvSrvUavHeap, &pCmd->mViewCpuHandle, &pCmd->mViewGpuHandle, MAX_DYNAMIC_VIEW_DESCRIPTORS_PER_FRAME);

						descTable.mBaseViewCpuHandle = { pCmd->mViewCpuHandle.ptr + pCmd->mViewPosition * pRenderer->pCbvSrvUavHeap->mDescriptorSize };
						descTable.mBaseViewGpuHandle = { pCmd->mViewGpuHandle.ptr + pCmd->mViewPosition * pRenderer->pCbvSrvUavHeap->mDescriptorSize };
						pCmd->mViewPosition += viewCount;
					}
					if (samplerCount)
					{
						if (pCmd->mSamplerCpuHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_NULL)
							add_gpu_descriptor_handles(pRenderer->pSamplerHeap, &pCmd->mSamplerCpuHandle, &pCmd->mSamplerGpuHandle, MAX_DYNAMIC_SAMPLER_DESCRIPTORS_PER_FRAME);

						descTable.mBaseSamplerCpuHandle = { pCmd->mSamplerCpuHandle.ptr + pCmd->mSamplerPosition * pRenderer->pSamplerHeap->mDescriptorSize };
						descTable.mBaseSamplerGpuHandle = { pCmd->mSamplerGpuHandle.ptr + pCmd->mSamplerPosition * pRenderer->pSamplerHeap->mDescriptorSize };
						pCmd->mSamplerPosition += samplerCount;
					}

					// Copy the descriptor handles from the cpu view heap to the gpu view heap
					// The source handles pm->pViewDescriptors are collected at the same time when we hashed the descriptors
					for (uint32_t i = 0; i < viewCount; ++i)
						pRenderer->pDevice->CopyDescriptorsSimple(1,
						{ descTable.mBaseViewCpuHandle.ptr + i * pRenderer->pCbvSrvUavHeap->mDescriptorSize },
							pm->pViewDescriptorHandles[setIndex][i], pRenderer->pCbvSrvUavHeap->mType);

					// Copy the descriptor handles from the cpu sampler heap to the gpu sampler heap
					// The source handles pm->pSamplerDescriptors are collected at the same time when we hashed the descriptors
					for (uint32_t i = 0; i < samplerCount; ++i)
						pRenderer->pDevice->CopyDescriptorsSimple(1,
						{ descTable.mBaseSamplerCpuHandle.ptr + i * pRenderer->pSamplerHeap->mDescriptorSize },
							pm->pSamplerDescriptorHandles[setIndex][i], pRenderer->pSamplerHeap->mType);
				}

				// Bind the view descriptor table if one exists
				if (descTable.mBaseViewGpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
				{
					if (pRootSignature->mPipelineType == PIPELINE_TYPE_COMPUTE)
					{
						pCmd->pDxCmdList->SetComputeRootDescriptorTable(
							pRootSignature->pViewTableLayouts[setIndex].mRootIndex,
							descTable.mBaseViewGpuHandle);
					}
					else if (pRootSignature->mPipelineType == PIPELINE_TYPE_GRAPHICS)
					{
						pCmd->pDxCmdList->SetGraphicsRootDescriptorTable(
							pRootSignature->pViewTableLayouts[setIndex].mRootIndex,
							descTable.mBaseViewGpuHandle);
					}
				}

				// Bind the sampler descriptor table if one exists
				if (descTable.mBaseSamplerGpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
				{
					if (pRootSignature->mPipelineType == PIPELINE_TYPE_COMPUTE)
					{
						pCmd->pDxCmdList->SetComputeRootDescriptorTable(
							pRootSignature->pSamplerTableLayouts[setIndex].mRootIndex,
							descTable.mBaseSamplerGpuHandle);
					}
					else if (pRootSignature->mPipelineType == PIPELINE_TYPE_GRAPHICS)
					{
						pCmd->pDxCmdList->SetGraphicsRootDescriptorTable(
							pRootSignature->pSamplerTableLayouts[setIndex].mRootIndex,
							descTable.mBaseSamplerGpuHandle);
					}
				}

				// Set the bound flag for the descriptor table of this update frequency
				// This way in the future if user tries to bind the same descriptor table, we can avoid unnecessary rebinds
				pm->mBoundTables[setIndex] = true;
			}
		}
	}
	// -------------------------------------------------------------------------------------------------
	// Query Heap Implementation
	// -------------------------------------------------------------------------------------------------
	D3D12_QUERY_HEAP_TYPE util_to_dx_query_heap_type(QueryType type)
	{
		switch (type)
		{
		case QUERY_TYPE_TIMESTAMP:
			return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		case QUERY_TYPE_PIPELINE_STATISTICS:
			return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
		case QUERY_TYPE_OCCLUSION:
			return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
		default:
			ASSERT(false && "Invalid query heap type");
			return D3D12_QUERY_HEAP_TYPE(-1);
		}
	}

	D3D12_QUERY_TYPE util_to_dx_query_type(QueryType type)
	{
		switch (type)
		{
		case QUERY_TYPE_TIMESTAMP:
			return D3D12_QUERY_TYPE_TIMESTAMP;
		case QUERY_TYPE_PIPELINE_STATISTICS:
			return D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
		case QUERY_TYPE_OCCLUSION:
			return D3D12_QUERY_TYPE_OCCLUSION;
		default:
			ASSERT(false && "Invalid query heap type");
			return D3D12_QUERY_TYPE(-1);
		}
	}

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
		desc.NodeMask = 0;
		desc.Type = util_to_dx_query_heap_type(pDesc->mType);
		pRenderer->pDevice->CreateQueryHeap(&desc, IID_ARGS(&pQueryHeap->pDxQueryHeap));

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
		case D3D12_QUERY_TYPE_OCCLUSION:
			break;
		case D3D12_QUERY_TYPE_BINARY_OCCLUSION:
			break;
		case D3D12_QUERY_TYPE_TIMESTAMP:
			pCmd->pDxCmdList->EndQuery(pQueryHeap->pDxQueryHeap, type, pQuery->mIndex);
			break;
		case D3D12_QUERY_TYPE_PIPELINE_STATISTICS:
			break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0:
			break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM1:
			break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM2:
			break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM3:
			break;
		default:
			break;
		}
	}

	void cmdEndQuery(Cmd* pCmd, QueryHeap* pQueryHeap, QueryDesc* pQuery)
	{
		D3D12_QUERY_TYPE type = util_to_dx_query_type(pQueryHeap->mDesc.mType);
		switch (type)
		{
		case D3D12_QUERY_TYPE_OCCLUSION:
			break;
		case D3D12_QUERY_TYPE_BINARY_OCCLUSION:
			break;
		case D3D12_QUERY_TYPE_TIMESTAMP:
			pCmd->pDxCmdList->EndQuery(pQueryHeap->pDxQueryHeap, type, pQuery->mIndex);
			break;
		case D3D12_QUERY_TYPE_PIPELINE_STATISTICS:
			break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0:
			break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM1:
			break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM2:
			break;
		case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM3:
			break;
		default:
			break;
		}
	}

	void cmdResolveQuery(Cmd* pCmd, QueryHeap* pQueryHeap, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
	{
		pCmd->pDxCmdList->ResolveQueryData(pQueryHeap->pDxQueryHeap, util_to_dx_query_type(pQueryHeap->mDesc.mType), startQuery, queryCount, pReadbackBuffer->pDxResource, startQuery * 8);
	}
	// -------------------------------------------------------------------------------------------------
	// Gloabals
	// -------------------------------------------------------------------------------------------------
	static const uint32_t		gDescriptorTableDWORDS = 1;
	static const uint32_t		gRootDescriptorDWORDS = 2;

	static volatile uint64_t	gBufferIds	= 0;
	static volatile uint64_t	gTextureIds	= 0;
	static volatile uint64_t	gSamplerIds	= 0;
	// -------------------------------------------------------------------------------------------------
	// Logging functions
	// -------------------------------------------------------------------------------------------------

	// Proxy log callback
	static void internal_log(LogType type, const char* msg, const char* component)
	{
		switch (type)
		{
		case LOG_TYPE_INFO:
			LOGINFOF("%s ( %s )", component, msg);
			break;
		case LOG_TYPE_WARN:
			LOGWARNINGF("%s ( %s )", component, msg);
			break;
		case LOG_TYPE_DEBUG:
			LOGDEBUGF("%s ( %s )", component, msg);
			break;
		case LOG_TYPE_ERROR:
			LOGERRORF("%s ( %s )", component, msg);
			break;
		default:
			break;
		}
	}

	void calculateMemoryStats(Renderer* pRenderer, char** stats)
	{
		resourceAllocBuildStatsString(pRenderer->pResourceAllocator, stats, 0);
	}

	void freeMemoryStats(Renderer* pRenderer, char* stats)
	{
		resourceAllocFreeStatsString(pRenderer->pResourceAllocator, stats);
	}

	void add_srv(Renderer* pRenderer, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
	{
		*pHandle = add_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], 1);
		pRenderer->pDevice->CreateShaderResourceView(pResource, pSrvDesc, *pHandle);
	}

	void remove_srv(Renderer* pRenderer, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
	{
		remove_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], pHandle, 1);
	}

	void add_uav(Renderer* pRenderer, ID3D12Resource* pResource, ID3D12Resource* pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
	{
		*pHandle = add_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], 1);
		pRenderer->pDevice->CreateUnorderedAccessView(pResource, pCounterResource, pUavDesc, *pHandle);
	}

	void remove_uav(Renderer* pRenderer, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
	{
		remove_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], pHandle, 1);
	}

	void add_cbv(Renderer* pRenderer, const D3D12_CONSTANT_BUFFER_VIEW_DESC* pCbvDesc, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
	{
		*pHandle = add_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], 1);
		pRenderer->pDevice->CreateConstantBufferView(pCbvDesc, *pHandle);
	}

	void remove_cbv(Renderer* pRenderer, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
	{
		remove_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], pHandle, 1);
	}

	void add_rtv(Renderer* pRenderer, ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
	{
		*pHandle = add_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV], 1);
		pRenderer->pDevice->CreateRenderTargetView(pResource, pRtvDesc, *pHandle);
	}

	void remove_rtv(Renderer* pRenderer, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
	{
		remove_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV], pHandle, 1);
	}

	void add_dsv(Renderer* pRenderer, ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDsvDesc, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
	{
		*pHandle = add_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV], 1);
		pRenderer->pDevice->CreateDepthStencilView(pResource, pDsvDesc, *pHandle);
	}

	void remove_dsv(Renderer* pRenderer, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
	{
		remove_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV], pHandle, 1);
	}

	void add_sampler(Renderer* pRenderer, const D3D12_SAMPLER_DESC* pSamplerDesc, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
	{
		*pHandle = add_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER], 1);
		pRenderer->pDevice->CreateSampler(pSamplerDesc, *pHandle);
	}

	void remove_sampler(Renderer* pRenderer, D3D12_CPU_DESCRIPTOR_HANDLE* pHandle)
	{
		remove_cpu_descriptor_handles(pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER], pHandle, 1);
	}

	void create_default_resources(Renderer* pRenderer)
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

		addBlendState(&pRenderer->pDefaultBlendState, BC_ONE, BC_ZERO, BC_ONE, BC_ZERO);
		addDepthState(pRenderer, &pRenderer->pDefaultDepthState, false, true);
		addRasterizerState(&pRenderer->pDefaultRasterizerState, CullMode::CULL_MODE_BACK);
	}

	void destroy_default_resources(Renderer* pRenderer)
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

	// -------------------------------------------------------------------------------------------------
	// API functions
	// -------------------------------------------------------------------------------------------------

  ImageFormat::Enum getRecommendedSwapchainFormat(bool hintHDR)
  {
    if (fnHookGetRecommendedSwapChainFormat)
      return fnHookGetRecommendedSwapChainFormat(hintHDR);
    else
      return ImageFormat::BGRA8;
  }

	void initRenderer(const char *appName, const RendererDesc* settings, Renderer** ppRenderer)
	{
		initHooks();

		Renderer* pRenderer = (Renderer*)conf_calloc(1, sizeof(*pRenderer));
		ASSERT(pRenderer);

		pRenderer->pName = (char*)conf_calloc(strlen(appName) + 1, sizeof(char));
		memcpy(pRenderer->pName, appName, strlen(appName));

		// Copy settings
		memcpy(&(pRenderer->mSettings), settings, sizeof(*settings));

		// Initialize the D3D12 bits
		{
			AddDevice(pRenderer);

			for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
			{
				add_descriptor_heap(pRenderer, (D3D12_DESCRIPTOR_HEAP_TYPE)i,
					gCpuDescriptorHeapProperties[i].mFlags,
					gCpuDescriptorHeapProperties[i].mMaxDescriptors,
					&pRenderer->pCPUDescriptorHeaps[i]);
			}

			add_descriptor_heap(pRenderer, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				gGpuDescriptorHeapProperties[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].mFlags,
				gGpuDescriptorHeapProperties[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].mMaxDescriptors,
				&pRenderer->pCbvSrvUavHeap);

			add_descriptor_heap(pRenderer, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
				gGpuDescriptorHeapProperties[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].mFlags,
				gGpuDescriptorHeapProperties[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].mMaxDescriptors,
				&pRenderer->pSamplerHeap);

			AllocatorCreateInfo info = { 0 };
			info.device = pRenderer->pDevice;
			info.physicalDevice = pRenderer->pActiveGPU;
			createAllocator(&info, &pRenderer->pResourceAllocator);
		}

		create_default_resources(pRenderer);

		if (fnHookPostInitRenderer != NULL)
			fnHookPostInitRenderer(pRenderer);

		// Renderer is good! Assign it to result!
		*(ppRenderer) = pRenderer;
	}

	void removeRenderer(Renderer* pRenderer)
	{
		ASSERT(pRenderer);

		SAFE_FREE(pRenderer->pName);

		destroy_default_resources(pRenderer);

		// Destroy the Direct3D12 bits
		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			remove_descriptor_heap(pRenderer->pCPUDescriptorHeaps[i]);
		}

		remove_descriptor_heap(pRenderer->pCbvSrvUavHeap);
		remove_descriptor_heap(pRenderer->pSamplerHeap);
		destroyAllocator(pRenderer->pResourceAllocator);
		RemoveDevice(pRenderer);

		// Free all the renderer components
		SAFE_FREE(pRenderer);
	}

	void addFence(Renderer *pRenderer, Fence** ppFence, uint64 mFenceValue)
	{
		//ASSERT that renderer is valid
		ASSERT(pRenderer);

		//create a Fence and ASSERT that it is valid
		Fence* pFence = (Fence*)conf_calloc(1, sizeof(*pFence));
		ASSERT(pFence);

		HRESULT hres = pRenderer->pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_ARGS(&pFence->pDxFence));
		pFence->mFenceValue = 1;
		ASSERT(SUCCEEDED(hres));

		pFence->pDxWaitIdleFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		pFence->mFenceValue = mFenceValue;

		//set given pointer to new fence
		*ppFence = pFence;
	}

	void removeFence(Renderer *pRenderer, Fence* pFence)
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

	void addSemaphore(Renderer *pRenderer, Semaphore** ppSemaphore)
	{
		//ASSERT that renderer is valid
		ASSERT(pRenderer);

		//create a semaphore and ASSERT that it is valid
		Semaphore* pSemaphore = (Semaphore*)conf_calloc(1, sizeof(*pSemaphore));
		ASSERT(pSemaphore);

		addFence(pRenderer, &pSemaphore->pFence);

		//save newly created semaphore in given pointer
		*ppSemaphore = pSemaphore;
	}

	void removeSemaphore(Renderer *pRenderer, Semaphore* pSemaphore)
	{
		//ASSERT that renderer and given semaphore are valid
		ASSERT(pRenderer);
		ASSERT(pSemaphore);

		removeFence(pRenderer, pSemaphore->pFence);

		//safe delete that check for valid pointer
		SAFE_FREE(pSemaphore);
	}

	void addQueue(Renderer* pRenderer, QueueDesc* pQDesc, Queue** ppQueue)
	{
		Queue * pQueue = (Queue*)conf_calloc(1, sizeof(*pQueue));
		ASSERT(pQueue != NULL);

		//provided description for queue creation
		if (pQDesc)
		{
			pQueue->mQueueDesc.mFlag = pQDesc->mFlag;
			pQueue->mQueueDesc.mType = pQDesc->mType;
			pQueue->mQueueDesc.mPriority = pQDesc->mPriority;
		}
		else
		{
			//default queue values
			pQueue->mQueueDesc.mFlag = QUEUE_FLAG_NONE;
			pQueue->mQueueDesc.mType = CMD_POOL_DIRECT;
			pQueue->mQueueDesc.mPriority = QUEUE_PRIORITY_NORMAL;
		}


		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = gDx12QueueFlagTranslator[pQueue->mQueueDesc.mFlag];
		queueDesc.Type = gDx12CmdTypeTranslator[pQueue->mQueueDesc.mType];
		queueDesc.Priority = gDx12QueuePriorityTranslator[pQueue->mQueueDesc.mPriority];

		HRESULT hr = pRenderer->pDevice->CreateCommandQueue(&queueDesc,
			__uuidof(pQueue->pDxQueue),
			(void**)&(pQueue->pDxQueue));

		ASSERT(SUCCEEDED(hr));

		switch (queueDesc.Type)
		{
		case D3D12_COMMAND_LIST_TYPE_DIRECT:
			pQueue->pDxQueue->SetName(L"GRAPHICS QUEUE");
			break;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE:
			pQueue->pDxQueue->SetName(L"COMPUTE QUEUE");
			break;
		case D3D12_COMMAND_LIST_TYPE_COPY:
			pQueue->pDxQueue->SetName(L"COPY QUEUE");
			break;
		default:
			break;
		}

		pQueue->pRenderer = pRenderer;

		// Add queue fence. This fence will make sure we finish all GPU works before releasing the queue
		addFence(pQueue->pRenderer, &pQueue->pQueueFence);

		*ppQueue = pQueue;
	}

	void removeQueue(Queue * pQueue)
	{
		ASSERT(pQueue != NULL);

		// Make sure we finished all GPU works before we remove the queue
		FenceStatus fenceStatus;
		pQueue->pDxQueue->Signal(pQueue->pQueueFence->pDxFence, pQueue->pQueueFence->mFenceValue++);
		getFenceStatus(pQueue->pQueueFence, &fenceStatus);
		uint64_t fenceValue = pQueue->pQueueFence->mFenceValue - 1;
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
		{
			pQueue->pQueueFence->pDxFence->SetEventOnCompletion(fenceValue, pQueue->pQueueFence->pDxWaitIdleFenceEvent);
			WaitForSingleObject(pQueue->pQueueFence->pDxWaitIdleFenceEvent, INFINITE);
		}
		removeFence(pQueue->pRenderer, pQueue->pQueueFence);

		SAFE_RELEASE(pQueue->pDxQueue);
		SAFE_FREE(pQueue);
	}

	void addCmdPool(Renderer *pRenderer, Queue* pQueue, bool transient, CmdPool** ppCmdPool, CmdPoolDesc * pCmdPoolDesc)
	{
		UNREF_PARAM(transient);
		//ASSERT that renderer is valid
		ASSERT(pRenderer);

		//create one new CmdPool and add to renderer
		CmdPool* pCmdPool = (CmdPool*)conf_calloc(1, sizeof(*pCmdPool));
		ASSERT(pCmdPool);

		CmdPoolDesc defaultDesc = {};
		defaultDesc.mCmdPoolType = pQueue->mQueueDesc.mType;
		if (pCmdPoolDesc != NULL)
			defaultDesc.mCmdPoolType = pCmdPoolDesc->mCmdPoolType;

		pCmdPool->pRenderer = pRenderer;
		pCmdPool->pQueue = pQueue;
		pCmdPool->mCmdPoolDesc.mCmdPoolType = defaultDesc.mCmdPoolType;

		*ppCmdPool = pCmdPool;
	}

	void removeCmdPool(Renderer *pRenderer, CmdPool* pCmdPool)
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
		pCmd->pCmdPool = pCmdPool;

		//add command to pool
		//ASSERT(pCmdPool->pDxCmdAlloc);
		ASSERT(pCmdPool->pRenderer);
		ASSERT(pCmdPool->mCmdPoolDesc.mCmdPoolType < CmdPoolType::MAX_CMD_TYPE);

		ASSERT(pCmdPool->pRenderer->pDevice);
		ASSERT(pCmdPool->mCmdPoolDesc.mCmdPoolType < CmdPoolType::MAX_CMD_TYPE);
		HRESULT hres = pCmdPool->pRenderer->pDevice->CreateCommandAllocator(
			gDx12CmdTypeTranslator[pCmdPool->mCmdPoolDesc.mCmdPoolType],
			__uuidof(pCmd->pDxCmdAlloc), (void**)&(pCmd->pDxCmdAlloc));
		ASSERT(SUCCEEDED(hres));

		ID3D12PipelineState* initialState = NULL;
		hres = pCmdPool->pRenderer->pDevice->CreateCommandList(
			0, gDx12CmdTypeTranslator[pCmdPool->mCmdPoolDesc.mCmdPoolType], pCmd->pDxCmdAlloc, initialState,
			__uuidof(pCmd->pDxCmdList), (void**)&(pCmd->pDxCmdList));
		ASSERT(SUCCEEDED(hres));

		// Command lists are addd in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		hres = pCmd->pDxCmdList->Close();
		ASSERT(SUCCEEDED(hres));

		//set new command
		*ppCmd = pCmd;
	}

	void removeCmd(CmdPool* pCmdPool, Cmd* pCmd)
	{
		//verify that given command and pool are valid
		ASSERT(pCmdPool);
		ASSERT(pCmd);

		if (pCmd->pRootConstantRingBuffer)
			removeUniformRingBuffer(pCmd->pRootConstantRingBuffer);

		//remove command from pool
		SAFE_RELEASE(pCmd->pDxCmdAlloc);
		SAFE_RELEASE(pCmd->pDxCmdList);

		//delete command
		SAFE_FREE(pCmd);
	}

	void addCmd_n(CmdPool *pCmdPool, bool secondary, uint32_t cmdCount, Cmd*** pppCmd)
	{
		//verify that ***cmd is valid
		ASSERT(pppCmd);

		//create new n command depending on cmdCount
		Cmd** ppCmd = (Cmd**)conf_calloc(cmdCount, sizeof(*ppCmd));
		ASSERT(ppCmd);

		//add n new cmds to given pool
		for (uint32_t i = 0; i < cmdCount; ++i) {
			addCmd(pCmdPool, secondary, &(ppCmd[i]));
		}
		//return new list of cmds
		*pppCmd = ppCmd;
	}

	void removeCmd_n(CmdPool *pCmdPool, uint32_t cmdCount, Cmd** ppCmd)
	{
		//verify that given command list is valid
		ASSERT(ppCmd);

		//remove every given cmd in array
		for (uint32_t i = 0; i < cmdCount; ++i) {
			removeCmd(pCmdPool, ppCmd[i]);
		}

		SAFE_FREE(ppCmd);
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
		desc.SampleDesc.Count = 1; // If multisampling is needed, we'll resolve it later
		desc.SampleDesc.Quality = pSwapChain->mDesc.mSampleQuality;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = pSwapChain->mDesc.mImageCount;
		desc.Scaling = DXGI_SCALING_STRETCH;
#ifdef _DURANGO
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
#else
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
#endif
		desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		desc.Flags = 0;

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

		HRESULT hres = pRenderer->pDXGIFactory->CreateSwapChainForHwnd(
			pSwapChain->mDesc.pQueue->pDxQueue, hwnd,
			&desc, NULL, NULL, &swapchain);
		ASSERT(SUCCEEDED(hres));

		hres = pRenderer->pDXGIFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
		ASSERT(SUCCEEDED(hres));
#endif

		hres = swapchain->QueryInterface(__uuidof(pSwapChain->pSwapChain), (void**)&(pSwapChain->pSwapChain));
		ASSERT(SUCCEEDED(hres));
		swapchain->Release();

		// Create rendertargets from swapchain
		pSwapChain->ppDxSwapChainResources = (ID3D12Resource**)conf_calloc(pSwapChain->mDesc.mImageCount, sizeof(*pSwapChain->ppDxSwapChainResources));
		ASSERT(pSwapChain->ppDxSwapChainResources);
		for (uint32_t i = 0; i < pSwapChain->mDesc.mImageCount; ++i)
		{
			hres = pSwapChain->pSwapChain->GetBuffer(i, IID_ARGS(&pSwapChain->ppDxSwapChainResources[i]));
			ASSERT(SUCCEEDED(hres) && pSwapChain->ppDxSwapChainResources[i]);
		}

		RenderTargetDesc descColor = {};
		descColor.mType = RENDER_TARGET_TYPE_2D;
		descColor.mUsage = RENDER_TARGET_USAGE_COLOR;
		descColor.mWidth = pSwapChain->mDesc.mWidth;
		descColor.mHeight = pSwapChain->mDesc.mHeight;
		descColor.mDepth = 1;
		descColor.mArraySize = 1;
		descColor.mFormat = pSwapChain->mDesc.mColorFormat;
		descColor.mClearValue = pSwapChain->mDesc.mColorClearValue;
		descColor.mSampleCount = SAMPLE_COUNT_1;
		descColor.mSampleQuality = 0;
		descColor.mSrgb = pSwapChain->mDesc.mSrgb;

		pSwapChain->ppSwapchainRenderTargets = (RenderTarget**)conf_calloc(pSwapChain->mDesc.mImageCount, sizeof(*pSwapChain->ppSwapchainRenderTargets));

		for (uint32_t i = 0; i < pSwapChain->mDesc.mImageCount; ++i) {
			addRenderTarget(pRenderer, &descColor, &pSwapChain->ppSwapchainRenderTargets[i], (void*)pSwapChain->ppDxSwapChainResources[i]);
		}

		*ppSwapChain = pSwapChain;
	}

	void removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
	{
		for (unsigned i = 0; i < pSwapChain->mDesc.mImageCount; ++i)
		{
			removeRenderTarget(pRenderer, pSwapChain->ppSwapchainRenderTargets[i]);
			SAFE_RELEASE(pSwapChain->ppDxSwapChainResources[i]);
		}

		SAFE_RELEASE(pSwapChain->pSwapChain);
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
		pBuffer->pRenderer = pRenderer;
		pBuffer->mDesc = *pDesc;

		//add to renderer
		// Align the buffer size to multiples of 256
		if ((pBuffer->mDesc.mUsage & BUFFER_USAGE_UNIFORM)) {
			pBuffer->mDesc.mSize = round_up_64(pBuffer->mDesc.mSize, pRenderer->pActiveGpuSettings->mUniformBufferAlignment);
		}

		// Buffer will be used multiple times every frame
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

		if (pBuffer->mDesc.mUsage & BUFFER_USAGE_STORAGE_UAV)
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}

		// Adjust for padding
		UINT64 padded_size = 0;
		pRenderer->pDevice->GetCopyableFootprints(&desc, 0, 1, 0, NULL, NULL, NULL, &padded_size);
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
		if (pBuffer->mDesc.mUsage & BUFFER_USAGE_INDIRECT)
			mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_ALLOW_INDIRECT_BUFFER;

		BufferCreateInfo alloc_info = { &desc, res_states };
		HRESULT hres = createBuffer(pRenderer->pResourceAllocator, &alloc_info, &mem_reqs, pBuffer);
		ASSERT(SUCCEEDED(hres));

		// If buffer is a suballocation use offset in heap else use zero offset (placed resource / committed resource)
		if (pBuffer->pDxAllocation->GetResource())
			pBuffer->mPositionInHeap = pBuffer->pDxAllocation->GetOffset();
		else
			pBuffer->mPositionInHeap = 0;

		pBuffer->mCurrentState = pBuffer->mDesc.mStartState;

		if (pBuffer->mDesc.mUsage & BUFFER_USAGE_UNIFORM) {
			pBuffer->mDxCbvDesc.BufferLocation = pBuffer->pDxResource->GetGPUVirtualAddress() + pBuffer->mPositionInHeap;
			pBuffer->mDxCbvDesc.SizeInBytes = (UINT)pBuffer->mDesc.mSize;
			add_cbv(pRenderer, &pBuffer->mDxCbvDesc, &pBuffer->mDxCbvHandle);
		}

		if (pBuffer->mDesc.mUsage & BUFFER_USAGE_INDEX) {
			pBuffer->mDxIndexBufferView.BufferLocation = pBuffer->pDxResource->GetGPUVirtualAddress() + pBuffer->mPositionInHeap;
			pBuffer->mDxIndexBufferView.SizeInBytes = (UINT)pBuffer->mDesc.mSize;
			//set type of index (16 bit, 32 bit) int
			pBuffer->mDxIndexBufferView.Format = (INDEX_TYPE_UINT16 == pBuffer->mDesc.mIndexType) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

		}

		if (pBuffer->mDesc.mUsage & BUFFER_USAGE_VERTEX) {
			if (pBuffer->mDesc.mVertexStride == 0)
			{
				LOGERRORF("Vertex Stride must be a non zero value");
				ASSERT(false);
			}
			pBuffer->mDxVertexBufferView.BufferLocation = pBuffer->pDxResource->GetGPUVirtualAddress() + pBuffer->mPositionInHeap;
			pBuffer->mDxVertexBufferView.SizeInBytes = (UINT)pBuffer->mDesc.mSize;
			pBuffer->mDxVertexBufferView.StrideInBytes = pBuffer->mDesc.mVertexStride;
		}

		if (pBuffer->mDesc.mUsage & BUFFER_USAGE_STORAGE_SRV) {
			pBuffer->mDxSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
			pBuffer->mDxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			pBuffer->mDxSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			pBuffer->mDxSrvDesc.Buffer.FirstElement = pBuffer->mDesc.mFirstElement;
			pBuffer->mDxSrvDesc.Buffer.NumElements = (UINT)(pBuffer->mDesc.mElementCount);
			pBuffer->mDxSrvDesc.Buffer.StructureByteStride = (UINT)(pBuffer->mDesc.mStructStride);
			pBuffer->mDxSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			if (pBuffer->mDesc.mFeatures & BUFFER_FEATURE_RAW) {
				pBuffer->mDxSrvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				pBuffer->mDxSrvDesc.Buffer.Flags |= D3D12_BUFFER_SRV_FLAG_RAW;
			}

			add_srv(pRenderer, pBuffer->pDxResource, &pBuffer->mDxSrvDesc, &pBuffer->mDxSrvHandle);
		}

		if (pBuffer->mDesc.mUsage & BUFFER_USAGE_STORAGE_UAV) {
			pBuffer->mDxUavDesc.Format = DXGI_FORMAT_UNKNOWN;
			pBuffer->mDxUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			pBuffer->mDxUavDesc.Buffer.FirstElement = pBuffer->mDesc.mFirstElement;
			pBuffer->mDxUavDesc.Buffer.NumElements = (UINT)(pBuffer->mDesc.mElementCount);
			pBuffer->mDxUavDesc.Buffer.StructureByteStride = (UINT)(pBuffer->mDesc.mStructStride);
			pBuffer->mDxUavDesc.Buffer.CounterOffsetInBytes = 0;
			pBuffer->mDxUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
			if (pBuffer->mDesc.mFeatures & BUFFER_FEATURE_RAW) {
				pBuffer->mDxUavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				pBuffer->mDxUavDesc.Buffer.Flags |= D3D12_BUFFER_UAV_FLAG_RAW;
			}

			ID3D12Resource* pCounterResource = pBuffer->mDesc.pCounterBuffer ? pBuffer->mDesc.pCounterBuffer->pDxResource : NULL;
			add_uav(pRenderer, pBuffer->pDxResource, pCounterResource, &pBuffer->mDxUavDesc, &pBuffer->mDxUavHandle);
		}

		pBuffer->mBufferId = (++gBufferIds << 8U) + Thread::GetCurrentThreadID();

		*pp_buffer = pBuffer;
	}

	void removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
	{
    UNREF_PARAM(pRenderer);
		ASSERT(pRenderer);
		ASSERT(pBuffer);

		destroyBuffer(pRenderer->pResourceAllocator, pBuffer);

		if (pBuffer->mDesc.mUsage & BUFFER_USAGE_UNIFORM)
		{
			remove_cbv(pRenderer, &pBuffer->mDxCbvHandle);
		}
		if (pBuffer->mDesc.mUsage & BUFFER_USAGE_STORAGE_SRV)
		{
			remove_srv(pRenderer, &pBuffer->mDxSrvHandle);
		}
		if (pBuffer->mDesc.mUsage & BUFFER_USAGE_STORAGE_UAV)
		{
			remove_uav(pRenderer, &pBuffer->mDxUavHandle);
		}

		SAFE_FREE(pBuffer);
	}

	void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange = NULL)
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
		pTexture->pRenderer = pRenderer;
		pTexture->mDesc = *pDesc;
		pTexture->pCpuMappedAddress = NULL;
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
		DXGI_FORMAT dxFormat = util_to_dx_image_format(pTexture->mDesc.mFormat, pTexture->mDesc.mSrgb);
		ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

		if (NULL == pTexture->pDxResource)
		{
			D3D12_RESOURCE_DIMENSION res_dim = D3D12_RESOURCE_DIMENSION_UNKNOWN;
			switch (pTexture->mDesc.mType) {
			case TEXTURE_TYPE_1D: res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE1D; break;
			case TEXTURE_TYPE_2D: res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE2D; break;
			case TEXTURE_TYPE_3D: res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE3D; break;
			case TEXTURE_TYPE_CUBE: res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE2D; break;
			}
			ASSERT(D3D12_RESOURCE_DIMENSION_UNKNOWN != res_dim);

			DECLARE_ZERO(D3D12_RESOURCE_DESC, desc);
			desc.Dimension = res_dim;
			//On PC, If Alignment is set to 0, the runtime will use 4MB for MSAA textures and 64KB for everything else.
			//On XBox, We have to explicitlly assign D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT if MSAA is used
			desc.Alignment = (UINT)pTexture->mDesc.mSampleCount > 1 ? D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT : 0;
			desc.Width = pTexture->mDesc.mWidth;
			desc.Height = pTexture->mDesc.mHeight;
			if (pTexture->mDesc.mType == TEXTURE_TYPE_CUBE)
				desc.DepthOrArraySize = (UINT16)(pTexture->mDesc.mArraySize * 6);
			else
				desc.DepthOrArraySize = (UINT16)(pTexture->mDesc.mArraySize != 1 ? pTexture->mDesc.mArraySize : pTexture->mDesc.mDepth);
			desc.MipLevels = (UINT16)pTexture->mDesc.mMipLevels;
			desc.Format = util_to_dx_image_format_typeless(pTexture->mDesc.mFormat);
			desc.SampleDesc.Count = (UINT)pTexture->mDesc.mSampleCount;
			desc.SampleDesc.Quality = (UINT)pTexture->mDesc.mSampleQuality;
			desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS data;
			data.Format = desc.Format;
			data.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
			data.SampleCount = desc.SampleDesc.Count;
			pRenderer->pDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(data));
			while (data.NumQualityLevels == 0 && data.SampleCount > 0)
			{
				LOGWARNINGF("Sample Count (%u) not supported. Trying a lower sample count (%u)", data.SampleCount, data.SampleCount / 2);
				data.SampleCount = desc.SampleDesc.Count / 2;
				pRenderer->pDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(data));
			}
			desc.SampleDesc.Count = data.SampleCount;
			pTexture->mDesc.mSampleCount = (SampleCount)desc.SampleDesc.Count;

			// Decide UAV flags
			if (pTexture->mDesc.mUsage & TEXTURE_USAGE_UNORDERED_ACCESS)
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

			DECLARE_ZERO(D3D12_CLEAR_VALUE, clearValue);
			clearValue.Format = dxFormat;
			if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
				clearValue.DepthStencil.Depth = pTexture->mDesc.mClearValue.depth;
				clearValue.DepthStencil.Stencil = (UINT8)pTexture->mDesc.mClearValue.stencil;
			}
			else {
				clearValue.Color[0] = pTexture->mDesc.mClearValue.r;
				clearValue.Color[1] = pTexture->mDesc.mClearValue.g;
				clearValue.Color[2] = pTexture->mDesc.mClearValue.b;
				clearValue.Color[3] = pTexture->mDesc.mClearValue.a;
			}

			D3D12_CLEAR_VALUE* pClearValue = NULL;
			D3D12_RESOURCE_STATES res_states = util_to_dx_resource_state(pTexture->mDesc.mStartState);

			if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) {
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

			TextureCreateInfo alloc_info = { &desc, pClearValue, res_states };
			HRESULT hr = createTexture(pRenderer->pResourceAllocator, &alloc_info, &mem_reqs, pTexture);
			ASSERT(SUCCEEDED(hr));
			pTexture->pCpuMappedAddress = pTexture->pDxAllocation->GetMemory();

			UINT64 buffer_size = 0;
			pRenderer->pDevice->GetCopyableFootprints(&desc, 0,
				pTexture->mDesc.mType == TEXTURE_TYPE_3D ? desc.MipLevels : desc.DepthOrArraySize * desc.MipLevels, 0, NULL, NULL, NULL, &buffer_size);

			pTexture->mTextureSize = buffer_size;
			pTexture->mCurrentState = pTexture->mDesc.mStartState;
		}
		else
		{
			dxFormat = pTexture->pDxResource->GetDesc().Format;
		}

		TextureType type = pTexture->mDesc.mType;
		switch (type)
		{
		case TEXTURE_TYPE_1D:
			if (pTexture->mDesc.mArraySize > 1)
			{
				pTexture->mDxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
				pTexture->mDxUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
				// SRV
				pTexture->mDxSrvDesc.Texture1DArray.ArraySize = pTexture->mDesc.mArraySize;
				pTexture->mDxSrvDesc.Texture1DArray.FirstArraySlice = pTexture->mDesc.mBaseArrayLayer;
				pTexture->mDxSrvDesc.Texture1DArray.MipLevels = pTexture->mDesc.mMipLevels;
				pTexture->mDxSrvDesc.Texture1DArray.MostDetailedMip = pTexture->mDesc.mBaseMipLevel;
				// UAV
				pTexture->mDxUavDesc.Texture1DArray.ArraySize = pTexture->mDesc.mArraySize;
				pTexture->mDxUavDesc.Texture1DArray.FirstArraySlice = pTexture->mDesc.mBaseArrayLayer;
				pTexture->mDxUavDesc.Texture1DArray.MipSlice = pTexture->mDesc.mBaseMipLevel;
			}
			else
			{
				pTexture->mDxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
				pTexture->mDxUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
				// SRV
				pTexture->mDxSrvDesc.Texture1D.MipLevels = pTexture->mDesc.mMipLevels;
				pTexture->mDxSrvDesc.Texture1D.MostDetailedMip = pTexture->mDesc.mBaseMipLevel;
				// UAV
				pTexture->mDxUavDesc.Texture1D.MipSlice = pTexture->mDesc.mBaseMipLevel;
			}
			break;
		case TEXTURE_TYPE_2D:
			if (pTexture->mDesc.mArraySize > 1)
			{
				if (pTexture->mDesc.mSampleCount > SAMPLE_COUNT_1)
				{
					pTexture->mDxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
					// Cannot create a multisampled uav
					// SRV
					pTexture->mDxSrvDesc.Texture2DMSArray.ArraySize = pTexture->mDesc.mArraySize;
					pTexture->mDxSrvDesc.Texture2DMSArray.FirstArraySlice = pTexture->mDesc.mBaseArrayLayer;
					// No UAV
				}
				else
				{
					pTexture->mDxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					pTexture->mDxUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
					// SRV
					pTexture->mDxSrvDesc.Texture2DArray.ArraySize = pTexture->mDesc.mArraySize;
					pTexture->mDxSrvDesc.Texture2DArray.FirstArraySlice = pTexture->mDesc.mBaseArrayLayer;
					pTexture->mDxSrvDesc.Texture2DArray.MipLevels = pTexture->mDesc.mMipLevels;
					pTexture->mDxSrvDesc.Texture2DArray.MostDetailedMip = pTexture->mDesc.mBaseMipLevel;
					pTexture->mDxSrvDesc.Texture2DArray.PlaneSlice = 0;
					// UAV
					pTexture->mDxUavDesc.Texture2DArray.ArraySize = pTexture->mDesc.mArraySize;
					pTexture->mDxUavDesc.Texture2DArray.FirstArraySlice = pTexture->mDesc.mBaseArrayLayer;
					pTexture->mDxUavDesc.Texture2DArray.MipSlice = pTexture->mDesc.mBaseMipLevel;
					pTexture->mDxUavDesc.Texture2DArray.PlaneSlice = 0;
				}
			}
			else
			{
				if (pTexture->mDesc.mSampleCount > SAMPLE_COUNT_1)
				{
					pTexture->mDxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
					// Cannot create a multisampled uav
				}
				else
				{
					pTexture->mDxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					pTexture->mDxUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
					// SRV
					pTexture->mDxSrvDesc.Texture2D.MipLevels = pTexture->mDesc.mMipLevels;
					pTexture->mDxSrvDesc.Texture2D.MostDetailedMip = pTexture->mDesc.mBaseMipLevel;
					pTexture->mDxSrvDesc.Texture2D.PlaneSlice = 0;
					// UAV
					pTexture->mDxUavDesc.Texture2D.MipSlice = pTexture->mDesc.mBaseMipLevel;
					pTexture->mDxUavDesc.Texture2D.PlaneSlice = 0;
				}
			}
			break;
		case TEXTURE_TYPE_3D:
			pTexture->mDxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			pTexture->mDxUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
			// SRV
			pTexture->mDxSrvDesc.Texture3D.MipLevels = pTexture->mDesc.mMipLevels;
			pTexture->mDxSrvDesc.Texture3D.MostDetailedMip = pTexture->mDesc.mBaseMipLevel;
			// UAV
			pTexture->mDxUavDesc.Texture3D.MipSlice = pTexture->mDesc.mBaseMipLevel;
			pTexture->mDxUavDesc.Texture3D.FirstWSlice = 0;
			pTexture->mDxUavDesc.Texture3D.WSize = (UINT)-1;
			break;
		case TEXTURE_TYPE_CUBE:
			if (pTexture->mDesc.mArraySize > 1)
			{
				pTexture->mDxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
				// Cannot create a cubemap array uav
				// SRV
				pTexture->mDxSrvDesc.TextureCubeArray.First2DArrayFace = 0;
				pTexture->mDxSrvDesc.TextureCubeArray.MipLevels = pTexture->mDesc.mMipLevels;
				pTexture->mDxSrvDesc.TextureCubeArray.MostDetailedMip = pTexture->mDesc.mBaseMipLevel;
				pTexture->mDxSrvDesc.TextureCubeArray.NumCubes = pTexture->mDesc.mArraySize;
				// No UAV
			}
			else
			{
				pTexture->mDxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				// Cannot create a cubemap uav
				// SRV
				pTexture->mDxSrvDesc.TextureCube.MipLevels = pTexture->mDesc.mMipLevels;
				pTexture->mDxSrvDesc.TextureCube.MostDetailedMip = pTexture->mDesc.mBaseMipLevel;
				// No UAV
			}
			break;
		default:
			break;
		}

		if (pTexture->mDesc.mUsage & TEXTURE_USAGE_SAMPLED_IMAGE)
		{
			ASSERT(pTexture->mDxSrvDesc.ViewDimension != D3D12_SRV_DIMENSION_UNKNOWN);
			pTexture->mDxSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			pTexture->mDxSrvDesc.Format = util_to_dx_srv_format(dxFormat);
			add_srv(pRenderer, pTexture->pDxResource, &pTexture->mDxSrvDesc, &pTexture->mDxSrvHandle);
		}

		if (pTexture->mDesc.mUsage & TEXTURE_USAGE_UNORDERED_ACCESS)
		{
			ASSERT(pTexture->mDxUavDesc.ViewDimension != D3D12_UAV_DIMENSION_UNKNOWN);
			pTexture->mDxUavDesc.Format = util_to_dx_uav_format(dxFormat);
			add_uav(pRenderer, pTexture->pDxResource, NULL,  &pTexture->mDxUavDesc, &pTexture->mDxUavHandle);
		}
		//save tetxure in given pointer
		*ppTexture = pTexture;

		 // TODO: Handle host visible textures in a better way
		if (pTexture->mDesc.mHostVisible) {
			internal_log(LOG_TYPE_WARN, "D3D12 does not support host visible textures, memory of resulting texture will not be mapped for CPU visibility", "addTexture");
		}
	}

	void mapTexture(Renderer* pRenderer, Texture* pTexture)
	{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pTexture);
		LOGWARNING("Map Unmap Operations not supported for Textures on DX12");
	}

	void unmapTexture(Renderer* pRenderer, Texture* pTexture)
	{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pTexture);
		LOGWARNING("Map Unmap Operations not supported for Textures on DX12");
	}

	void removeTexture(Renderer* pRenderer, Texture* pTexture)
	{
		ASSERT(pRenderer);
		ASSERT(pTexture);

		//delete texture memory
		if (pTexture->mDesc.mUsage & TEXTURE_USAGE_SAMPLED_IMAGE)
		{
			remove_srv(pRenderer, &pTexture->mDxSrvHandle);
		}
		if (pTexture->mDesc.mUsage & TEXTURE_USAGE_UNORDERED_ACCESS)
		{
			remove_uav(pRenderer, &pTexture->mDxUavHandle);
		}

		if (pTexture->mOwnsImage)
		{
			destroyTexture(pRenderer->pResourceAllocator, pTexture);
		}

		SAFE_FREE(pTexture);
	}

	void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget, void* pNativeHandle)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc);
		ASSERT(ppRenderTarget);
		// Assert that render target is used as either color or depth attachment
		ASSERT(((pDesc->mUsage & RENDER_TARGET_USAGE_COLOR) || (pDesc->mUsage & RENDER_TARGET_USAGE_DEPTH_STENCIL)));
		// Assert that render target is not used as both color and depth attachment
		ASSERT(!((pDesc->mUsage & RENDER_TARGET_USAGE_COLOR) && (pDesc->mUsage & RENDER_TARGET_USAGE_DEPTH_STENCIL)));

		RenderTarget* pRenderTarget = (RenderTarget*)conf_calloc(1, sizeof(*pRenderTarget));
		pRenderTarget->mDesc = *pDesc;

		//add to gpu
		DXGI_FORMAT dxFormat = util_to_dx_image_format(pRenderTarget->mDesc.mFormat, pDesc->mSrgb);
		ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

		TextureDesc textureDesc = {};
		textureDesc.mBaseArrayLayer = pDesc->mBaseArrayLayer;
		textureDesc.mArraySize = pDesc->mArraySize;
		textureDesc.mClearValue = pDesc->mClearValue;
		textureDesc.mDepth = pDesc->mDepth;
		textureDesc.mFlags = pDesc->mFlags;
		textureDesc.mFormat = pDesc->mFormat;
		textureDesc.mHeight = pDesc->mHeight;
		textureDesc.mBaseMipLevel = pDesc->mBaseMipLevel;
		textureDesc.mMipLevels = 1;
		textureDesc.mSampleCount = pDesc->mSampleCount;
		textureDesc.mSampleQuality = pDesc->mSampleQuality;

		if (pDesc->mUsage & RENDER_TARGET_USAGE_COLOR)
			textureDesc.mStartState |= RESOURCE_STATE_RENDER_TARGET;
		else if (pDesc->mUsage & RENDER_TARGET_USAGE_DEPTH_STENCIL)
			textureDesc.mStartState |= RESOURCE_STATE_DEPTH_WRITE;

		// Set this by default to be able to sample the rendertarget in shader
		textureDesc.mUsage = TEXTURE_USAGE_SAMPLED_IMAGE;
		if (pDesc->mUsage & RENDER_TARGET_USAGE_UNORDERED_ACCESS)
			textureDesc.mUsage |= TEXTURE_USAGE_UNORDERED_ACCESS;

		textureDesc.mWidth = pDesc->mWidth;
		textureDesc.pNativeHandle = pNativeHandle;
		textureDesc.mSrgb = pDesc->mSrgb;

		switch (pDesc->mType)
		{
		case RENDER_TARGET_TYPE_1D:
			textureDesc.mType = TEXTURE_TYPE_1D;
			break;
		case RENDER_TARGET_TYPE_2D:
			textureDesc.mType = TEXTURE_TYPE_2D;
			break;
		case RENDER_TARGET_TYPE_3D:
			textureDesc.mType = TEXTURE_TYPE_3D;
			break;
		default:
			break;
		}

		addTexture(pRenderer, &textureDesc, &pRenderTarget->pTexture);

		RenderTargetType type = pRenderTarget->mDesc.mType;
		switch (type)
		{
		case RENDER_TARGET_TYPE_1D:
			if (pRenderTarget->mDesc.mArraySize > 1)
			{
				pRenderTarget->mDxRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
				pRenderTarget->mDxDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
				// RTV
				pRenderTarget->mDxRtvDesc.Texture1DArray.ArraySize = pRenderTarget->mDesc.mArraySize;
				pRenderTarget->mDxRtvDesc.Texture1DArray.FirstArraySlice = pRenderTarget->mDesc.mBaseMipLevel;
				pRenderTarget->mDxRtvDesc.Texture1DArray.MipSlice = pRenderTarget->mDesc.mBaseMipLevel;
				// DSV
				pRenderTarget->mDxDsvDesc.Texture1DArray.ArraySize = pRenderTarget->mDesc.mArraySize;
				pRenderTarget->mDxDsvDesc.Texture1DArray.FirstArraySlice = pRenderTarget->mDesc.mBaseArrayLayer;
				pRenderTarget->mDxDsvDesc.Texture1DArray.MipSlice = pRenderTarget->mDesc.mBaseMipLevel;
			}
			else
			{
				pRenderTarget->mDxRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
				pRenderTarget->mDxDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
				// RTV
				pRenderTarget->mDxRtvDesc.Texture1D.MipSlice = pRenderTarget->mDesc.mBaseMipLevel;
				// DSV
				pRenderTarget->mDxDsvDesc.Texture1D.MipSlice = pRenderTarget->mDesc.mBaseMipLevel;
			}
			break;
		case RENDER_TARGET_TYPE_2D:
			if (pRenderTarget->mDesc.mArraySize > 1)
			{
				if (pRenderTarget->mDesc.mSampleCount > SAMPLE_COUNT_1)
				{
					pRenderTarget->mDxRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
					pRenderTarget->mDxDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
					// RTV
					pRenderTarget->mDxRtvDesc.Texture2DMSArray.ArraySize = pRenderTarget->mDesc.mArraySize;
					pRenderTarget->mDxRtvDesc.Texture2DMSArray.FirstArraySlice = pRenderTarget->mDesc.mBaseArrayLayer;
					// DSV
					pRenderTarget->mDxDsvDesc.Texture2DMSArray.ArraySize = pRenderTarget->mDesc.mArraySize;
					pRenderTarget->mDxDsvDesc.Texture2DMSArray.FirstArraySlice = pRenderTarget->mDesc.mBaseArrayLayer;
				}
				else
				{
					pRenderTarget->mDxRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
					pRenderTarget->mDxDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
					// RTV
					pRenderTarget->mDxRtvDesc.Texture2DArray.ArraySize = pRenderTarget->mDesc.mArraySize;
					pRenderTarget->mDxRtvDesc.Texture2DArray.FirstArraySlice = pRenderTarget->mDesc.mBaseArrayLayer;
					pRenderTarget->mDxRtvDesc.Texture2DArray.MipSlice = pRenderTarget->mDesc.mBaseMipLevel;
					pRenderTarget->mDxRtvDesc.Texture2DArray.PlaneSlice = 0;
					// DSV
					pRenderTarget->mDxDsvDesc.Texture2DArray.ArraySize = pRenderTarget->mDesc.mArraySize;
					pRenderTarget->mDxDsvDesc.Texture2DArray.FirstArraySlice = pRenderTarget->mDesc.mBaseArrayLayer;
					pRenderTarget->mDxDsvDesc.Texture2DArray.MipSlice = pRenderTarget->mDesc.mBaseMipLevel;
				}
			}
			else
			{
				if (pRenderTarget->mDesc.mSampleCount > SAMPLE_COUNT_1)
				{
					pRenderTarget->mDxRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
					pRenderTarget->mDxDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
				}
				else
				{
					pRenderTarget->mDxRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
					pRenderTarget->mDxDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
					// RTV
					pRenderTarget->mDxRtvDesc.Texture2D.MipSlice = pRenderTarget->mDesc.mBaseMipLevel;
					pRenderTarget->mDxRtvDesc.Texture2D.PlaneSlice = 0;
					// DSV
					pRenderTarget->mDxDsvDesc.Texture2D.MipSlice = pRenderTarget->mDesc.mBaseMipLevel;
				}
			}
			break;
		case RENDER_TARGET_TYPE_3D:
			pRenderTarget->mDxRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
			// Cannot create a 3D depth stencil
			pRenderTarget->mDxRtvDesc.Texture3D.MipSlice = pRenderTarget->mDesc.mBaseMipLevel;
			pRenderTarget->mDxRtvDesc.Texture3D.WSize = ~0u;
			break;
		default:
			break;
		}

		if (pRenderTarget->mDesc.mUsage & RENDER_TARGET_USAGE_COLOR)
		{
			ASSERT(pRenderTarget->mDxRtvDesc.ViewDimension != D3D12_RTV_DIMENSION_UNKNOWN);
			pRenderTarget->mDxRtvDesc.Format = dxFormat;
			add_rtv(pRenderer, pRenderTarget->pTexture->pDxResource, &pRenderTarget->mDxRtvDesc, &pRenderTarget->mDxRtvHandle);
		}
		else if (pRenderTarget->mDesc.mUsage & RENDER_TARGET_USAGE_DEPTH_STENCIL)
		{
			ASSERT(pRenderTarget->mDxDsvDesc.ViewDimension != D3D12_DSV_DIMENSION_UNKNOWN);
			pRenderTarget->mDxDsvDesc.Format = util_to_dx_dsv_format(dxFormat);
			add_dsv(pRenderer, pRenderTarget->pTexture->pDxResource, &pRenderTarget->mDxDsvDesc, &pRenderTarget->mDxDsvHandle);
		}

		*ppRenderTarget = pRenderTarget;
	}

	void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
	{
		removeTexture(pRenderer, pRenderTarget->pTexture);

		if (pRenderTarget->mDesc.mUsage == RENDER_TARGET_USAGE_COLOR)
		{
			remove_rtv(pRenderer,&pRenderTarget->mDxRtvHandle);
		}
		else if (pRenderTarget->mDesc.mUsage == RENDER_TARGET_USAGE_DEPTH_STENCIL)
		{
			remove_dsv(pRenderer, &pRenderTarget->mDxDsvHandle);
		}

		SAFE_FREE(pRenderTarget);
	}

	inline bool hasMipmaps(const FilterType filter) { return (filter >= FILTER_BILINEAR); }
	inline bool hasAniso(const FilterType filter) { return (filter >= FILTER_BILINEAR_ANISO); }

	void addSampler(Renderer* pRenderer, Sampler** ppSampler, FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, AddressMode addressU, AddressMode addressV, AddressMode addressW, float mipLosBias, float maxAnisotropy)
	{
		ASSERT(pRenderer);

		//allocate new sampler
		Sampler* pSampler = (Sampler*)conf_calloc(1, sizeof(*pSampler));
		ASSERT(pSampler);

		//set current render as sampler renderer
		pSampler->pRenderer = pRenderer;

		//add sampler to gpu
		ASSERT(pRenderer->pDevice);
		pSampler->mDxSamplerDesc.Filter = util_to_dx_filter(minFilter, magFilter, mipMapMode);
		pSampler->mDxSamplerDesc.AddressU = util_to_dx_texture_address_mode(addressU);
		pSampler->mDxSamplerDesc.AddressV = util_to_dx_texture_address_mode(addressV);
		pSampler->mDxSamplerDesc.AddressW = util_to_dx_texture_address_mode(addressW);
		pSampler->mDxSamplerDesc.MipLODBias = mipLosBias;
		pSampler->mDxSamplerDesc.MaxAnisotropy = (hasAniso(minFilter) || hasAniso(magFilter)) ? (UINT)maxAnisotropy : 1U;
		pSampler->mDxSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		pSampler->mDxSamplerDesc.BorderColor[0] = 0.0f;
		pSampler->mDxSamplerDesc.BorderColor[1] = 0.0f;
		pSampler->mDxSamplerDesc.BorderColor[2] = 0.0f;
		pSampler->mDxSamplerDesc.BorderColor[3] = 0.0f;
		pSampler->mDxSamplerDesc.MinLOD = 0.0f;
		pSampler->mDxSamplerDesc.MaxLOD = (hasMipmaps(minFilter) || hasMipmaps(magFilter)) ? D3D12_FLOAT32_MAX : 0.0f;

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

	void compileShader(Renderer* pRenderer, ShaderStage stage, const String& fileName, const String& code, uint32_t macroCount, ShaderMacro* pMacros, tinystl::vector<char>* pByteCode)
	{
#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compile_flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		compile_flags |= (D3DCOMPILE_ALL_RESOURCES_BOUND | D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);

		int major;
		int minor;
		switch (pRenderer->mSettings.mShaderTarget) {
		default:
		case shader_target_5_1: { major = 5; minor = 1; } break;
		case shader_target_6_0: { major = 6; minor = 0; } break;
		}

		String target;
		switch (stage)
		{
		case SHADER_STAGE_VERT:
			target = String().sprintf("vs_%d_%d", major, minor);
			break;
		case SHADER_STAGE_TESC:
			target = String().sprintf("hs_%d_%d", major, minor);
			break;
		case SHADER_STAGE_TESE:
			target = String().sprintf("ds_%d_%d", major, minor);
			break;
		case SHADER_STAGE_GEOM:
			target = String().sprintf("gs_%d_%d", major, minor);
			break;
		case SHADER_STAGE_FRAG:
			target = String().sprintf("ps_%d_%d", major, minor);
			break;
		case SHADER_STAGE_COMP:
			target = String().sprintf("cs_%d_%d", major, minor);
			break;
		default:
			break;
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

		String entryPoint = "main";
		ID3DBlob* compiled_code = NULL;
		ID3DBlob* error_msgs = NULL;
		HRESULT hres = D3DCompile2(code.c_str(), code.size(), fileName.c_str(),
			macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entryPoint.c_str(), target, compile_flags,
			0, 0, NULL, 0,
			&compiled_code, &error_msgs);
		if (FAILED(hres)) {
			char* msg = (char*)conf_calloc(error_msgs->GetBufferSize() + 1, sizeof(*msg));
			ASSERT(msg);
			memcpy(msg, error_msgs->GetBufferPointer(), error_msgs->GetBufferSize());
			String error = fileName + " " + msg;
			ErrorMsg(error);
			SAFE_FREE(msg);
		}
		ASSERT(SUCCEEDED(hres));

		pByteCode->resize(compiled_code->GetBufferSize());
		memcpy(pByteCode->data(), compiled_code->GetBufferPointer(), compiled_code->GetBufferSize());
		SAFE_RELEASE(compiled_code);
	}

	void addShader(Renderer* pRenderer, const ShaderDesc* pDesc, Shader** ppShaderProgram)
	{
		Shader* pShaderProgram = (Shader*)conf_calloc(1, sizeof(*pShaderProgram));
		pShaderProgram->mStages = pDesc->mStages;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compile_flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		compile_flags |= (D3DCOMPILE_ALL_RESOURCES_BOUND  | D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);

		int major;
		int minor;
		switch (pRenderer->mSettings.mShaderTarget) {
		default:
		case shader_target_5_1: { major = 5; minor = 1; } break;
		case shader_target_6_0: { major = 6; minor = 0; } break;
		}

		DECLARE_ZERO(char, vsTarget[16]);
		DECLARE_ZERO(char, hsTarget[16]);
		DECLARE_ZERO(char, dsTarget[16]);
		DECLARE_ZERO(char, gsTarget[16]);
		DECLARE_ZERO(char, psTarget[16]);
		DECLARE_ZERO(char, csTarget[16]);

		sprintf_s(vsTarget, "vs_%d_%d", major, minor);
		sprintf_s(hsTarget, "hs_%d_%d", major, minor);
		sprintf_s(dsTarget, "ds_%d_%d", major, minor);
		sprintf_s(gsTarget, "gs_%d_%d", major, minor);
		sprintf_s(psTarget, "ps_%d_%d", major, minor);
		sprintf_s(csTarget, "cs_%d_%d", major, minor);

		uint32_t reflectionCount = 0;

		for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i) {
			ShaderStage stage_mask = (ShaderStage)(1 << i);
			if (stage_mask == (pShaderProgram->mStages & stage_mask)) {
				const ShaderStageDesc* pStage = NULL;
				const char* target = NULL;
				ID3DBlob**  compiled_code = NULL;
				switch (stage_mask) {
				case SHADER_STAGE_VERT: {
					pStage = &pDesc->mVert;
					target = vsTarget;
					compiled_code = &(pShaderProgram->pDxVert);
				} break;
				case SHADER_STAGE_HULL: {
					pStage = &pDesc->mHull;
					target = hsTarget;
					compiled_code = &(pShaderProgram->pDxHull);
				} break;
				case SHADER_STAGE_DOMN: {
					pStage = &pDesc->mDomain;
					target = dsTarget;
					compiled_code = &(pShaderProgram->pDxDomn);
				} break;
				case SHADER_STAGE_GEOM: {
					pStage = &pDesc->mGeom;
					target = gsTarget;
					compiled_code = &(pShaderProgram->pDxGeom);
				} break;
				case SHADER_STAGE_FRAG: {
					pStage = &pDesc->mFrag;
					target = psTarget;
					compiled_code = &(pShaderProgram->pDxFrag);
				} break;
				case SHADER_STAGE_COMP: {
					pStage = &pDesc->mComp;
					target = csTarget;
					compiled_code = &(pShaderProgram->pDxComp);
				} break;
				}

				// Extract shader macro definitions into D3D_SHADER_MACRO scruct
				// Allocate Size+2 structs: one for D3D12 1 definition and one for null termination
				D3D_SHADER_MACRO* macros = (D3D_SHADER_MACRO*)alloca((pStage->mMacros.size() + 2) * sizeof(D3D_SHADER_MACRO));
				macros[0] = { "D3D12", "1" };
				for (uint32_t j = 0; j < (uint32_t)pStage->mMacros.size(); ++j)
				{
					macros[j + 1] = { pStage->mMacros[j].definition, pStage->mMacros[j].value };
				}
				macros[pStage->mMacros.size() + 1] = { NULL, NULL };

				if (fnHookShaderCompileFlags != NULL)
					fnHookShaderCompileFlags(compile_flags);


				ID3DBlob* error_msgs = NULL;
				HRESULT hres = D3DCompile2(pStage->mCode.c_str(), pStage->mCode.size(), pStage->mName.c_str(),
					macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
					pStage->mEntryPoint.c_str(), target, compile_flags,
					0, 0, NULL, 0,
					compiled_code, &error_msgs);
				if (FAILED(hres)) {
					char* msg = (char*)conf_calloc(error_msgs->GetBufferSize() + 1, sizeof(*msg));
					ASSERT(msg);
					memcpy(msg, error_msgs->GetBufferPointer(), error_msgs->GetBufferSize());
					String error = pStage->mName + " " + msg;
					ErrorMsg(error);
					SAFE_FREE(msg);
				}
				ASSERT(SUCCEEDED(hres));

				createShaderReflection(
					(uint8_t*)((*compiled_code)->GetBufferPointer()),
					(uint32_t)(*compiled_code)->GetBufferSize(),
					stage_mask,
					&pShaderProgram->mReflection.mStageReflections[reflectionCount]);

				if (stage_mask == SHADER_STAGE_COMP)
					memcpy(pShaderProgram->mNumThreadsPerGroup, pShaderProgram->mReflection.mStageReflections[reflectionCount].mNumThreadsPerGroup, sizeof(pShaderProgram->mNumThreadsPerGroup));
				else if (stage_mask == SHADER_STAGE_TESC)
					memcpy(&pShaderProgram->mNumControlPoint, &pShaderProgram->mReflection.mStageReflections[reflectionCount].mNumControlPoint, sizeof(pShaderProgram->mNumControlPoint));

				reflectionCount++;
			}
		}

		createPipelineReflection(
			pShaderProgram->mReflection.mStageReflections,
			reflectionCount,
			&pShaderProgram->mReflection);

		*ppShaderProgram = pShaderProgram;
	}

	void addShader(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc && pDesc->mStages);
		ASSERT(ppShaderProgram);

		Shader* pShaderProgram = (Shader*)conf_calloc(1, sizeof(*pShaderProgram));
		ASSERT(pShaderProgram);
		pShaderProgram->mStages = pDesc->mStages;

		uint32_t reflectionCount = 0;

		for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
		{
			ShaderStage stage_mask = (ShaderStage)(1 << i);
			const BinaryShaderStageDesc* pStage = NULL;
			ID3DBlob** compiled_code = NULL;

			if (stage_mask == (pShaderProgram->mStages & stage_mask))
			{
				switch (stage_mask) {
				case SHADER_STAGE_VERT: {
					pStage = &pDesc->mVert;
					compiled_code = &(pShaderProgram->pDxVert);
				} break;
				case SHADER_STAGE_HULL: {
					pStage = &pDesc->mHull;
					compiled_code = &(pShaderProgram->pDxHull);
				} break;
				case SHADER_STAGE_DOMN: {
					pStage = &pDesc->mDomain;
					compiled_code = &(pShaderProgram->pDxDomn);
				} break;
				case SHADER_STAGE_GEOM: {
					pStage = &pDesc->mGeom;
					compiled_code = &(pShaderProgram->pDxGeom);
				} break;
				case SHADER_STAGE_FRAG: {
					pStage = &pDesc->mFrag;
					compiled_code = &(pShaderProgram->pDxFrag);
				} break;
				case SHADER_STAGE_COMP: {
					pStage = &pDesc->mComp;
					compiled_code = &(pShaderProgram->pDxComp);
				} break;
				}

				D3DCreateBlob(pStage->mByteCode.size(), compiled_code);
				memcpy((*compiled_code)->GetBufferPointer(), pStage->mByteCode.data(), pStage->mByteCode.size() * sizeof(unsigned char));

				createShaderReflection(
					(uint8_t*)((*compiled_code)->GetBufferPointer()),
					(uint32_t)(*compiled_code)->GetBufferSize(),
					stage_mask,
					&pShaderProgram->mReflection.mStageReflections[reflectionCount]);

				if (stage_mask == SHADER_STAGE_COMP)
					memcpy(pShaderProgram->mNumThreadsPerGroup, pShaderProgram->mReflection.mStageReflections[reflectionCount].mNumThreadsPerGroup, sizeof(pShaderProgram->mNumThreadsPerGroup));
				else if (stage_mask == SHADER_STAGE_TESC)
					memcpy(&pShaderProgram->mNumControlPoint, &pShaderProgram->mReflection.mStageReflections[reflectionCount].mNumControlPoint, sizeof(pShaderProgram->mNumControlPoint));

				reflectionCount++;
			}
		}

		createPipelineReflection(
			pShaderProgram->mReflection.mStageReflections,
			reflectionCount,
			&pShaderProgram->mReflection);

		*ppShaderProgram = pShaderProgram;
	}

	void removeShader(Renderer* pRenderer, Shader* pShaderProgram)
	{
		UNREF_PARAM(pRenderer);

		//remove given shader
		SAFE_RELEASE(pShaderProgram->pDxVert);
		SAFE_RELEASE(pShaderProgram->pDxHull);
		SAFE_RELEASE(pShaderProgram->pDxDomn);
		SAFE_RELEASE(pShaderProgram->pDxGeom);
		SAFE_RELEASE(pShaderProgram->pDxFrag);
		SAFE_RELEASE(pShaderProgram->pDxComp);
		destroyPipelineReflection(&pShaderProgram->mReflection);

		SAFE_FREE(pShaderProgram);
	}

	typedef struct UpdateFrequencyLayoutInfo
	{
		tinystl::vector <DescriptorInfo*> mCbvSrvUavTable;
		tinystl::vector <DescriptorInfo*> mSamplerTable;
		tinystl::vector <DescriptorInfo*> mConstantParams;
		tinystl::vector <DescriptorInfo*> mRootConstants;
		tinystl::unordered_map<DescriptorInfo*, uint32_t> mDescriptorIndexMap;
	} UpdateFrequencyLayoutInfo;

	/// Calculates the total size of the root signature (in DWORDS) from the input layouts
	uint32_t calculate_root_signature_size(UpdateFrequencyLayoutInfo* pLayouts, uint32_t numLayouts)
	{
		uint32_t size = 0;
		for (uint32_t i = 0; i < numLayouts; ++i)
		{
			if (pLayouts[i].mCbvSrvUavTable.getCount())
				size += gDescriptorTableDWORDS;
			if (pLayouts[i].mSamplerTable.getCount())
				size += gDescriptorTableDWORDS;

			for (uint32_t c = 0; c < pLayouts[i].mConstantParams.getCount(); ++c)
			{
				size += gRootDescriptorDWORDS;
			}
			for (uint32_t c = 0; c < pLayouts[i].mRootConstants.getCount(); ++c)
			{
				DescriptorInfo* pDesc = pLayouts[i].mRootConstants[c];
				size += pDesc->mDesc.size;
			}
		}

		return size;
	}

	/// Creates a root descriptor table parameter from the input table layout for root signature version 1_1
	void create_descriptor_table(uint32_t numDescriptors, DescriptorInfo** tableRef, D3D12_DESCRIPTOR_RANGE1* pRange, D3D12_ROOT_PARAMETER1* pRootParam)
	{
		pRootParam->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		ShaderStage stageCount = SHADER_STAGE_NONE;
		for (uint32_t i = 0; i < numDescriptors; ++i)
		{
			const DescriptorInfo* pDesc = tableRef[i];
			pRange[i].BaseShaderRegister = pDesc->mDesc.reg;
			pRange[i].RegisterSpace = pDesc->mDesc.set;
			pRange[i].Flags =  D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
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
	void create_descriptor_table_1_0(uint32_t numDescriptors, DescriptorInfo** tableRef, D3D12_DESCRIPTOR_RANGE* pRange, D3D12_ROOT_PARAMETER* pRootParam)
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

	static const uint32_t gMaxRootConstantsPerRootParam = 4U;
	static const RootSignatureDesc gDefaultRootSignatureDesc = {};

	void addRootSignature(Renderer* pRenderer, uint32_t numShaders, Shader* const* ppShaders, RootSignature** ppRootSignature, const RootSignatureDesc* pRootDesc)
	{
		RootSignature* pRootSignature = (RootSignature*)conf_calloc(1, sizeof(*pRootSignature));

		ASSERT(pRenderer->pActiveGpuSettings->mMaxRootSignatureDWORDS >= 2);

		tinystl::vector<UpdateFrequencyLayoutInfo> layouts(DESCRIPTOR_UPDATE_FREQ_COUNT);
		tinystl::vector<ShaderResource> shaderResources;
		tinystl::vector<uint32_t> constantSizes;
		tinystl::vector <tinystl::pair<DescriptorInfo*, Sampler*> > staticSamplers;
		ShaderStage shaderStages = SHADER_STAGE_NONE;
		bool useInputLayout = false;
		const RootSignatureDesc* pRootSignatureDesc = pRootDesc ? pRootDesc : &gDefaultRootSignatureDesc;

		//pRootSignature->pDescriptorNameToIndexMap;
		conf_placement_new<tinystl::unordered_map<uint32_t, uint32_t> >(
			&pRootSignature->pDescriptorNameToIndexMap);

		// Collect all unique shader resources in the given shaders
		// Resources are parsed by name (two resources named "XYZ" in two shaders will be considered the same resource)
		for (uint32_t sh = 0; sh < numShaders; ++sh)
		{
			PipelineReflection const* pReflection = &ppShaders[sh]->mReflection;

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
				uint32_t setIndex = pRes->set;

				// If the size of the resource is zero, assume its a bindless resource
				// All bindless resources will go in the static descriptor table
				if (pRes->size == 0)
					setIndex = 0;

				// Find all unique resources
				tinystl::unordered_hash_node<uint32_t, uint32_t>* pNode = pRootSignature->pDescriptorNameToIndexMap.find(tinystl::hash(pRes->name)).node;
				if (!pNode)
				{
					pRootSignature->pDescriptorNameToIndexMap.insert({ tinystl::hash(pRes->name), shaderResources.getCount() });
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

		if (shaderResources.getCount())
		{
			pRootSignature->mDescriptorCount = shaderResources.getCount();
			pRootSignature->pDescriptors = (DescriptorInfo*)conf_calloc(pRootSignature->mDescriptorCount, sizeof(DescriptorInfo));
		}

		// Fill the descriptor array to be stored in the root signature
		for (uint32_t i = 0; i < shaderResources.getCount(); ++i)
		{
			DescriptorInfo* pDesc = &pRootSignature->pDescriptors[i];
			ShaderResource* pRes = &shaderResources[i];
			uint32_t setIndex = pRes->set;
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

			if (pDesc->mDesc.size == 0)
			{
				pDesc->mDesc.size = pRootSignatureDesc->mMaxBindlessDescriptors[pDesc->mDesc.type];
			}

			// Find the D3D12 type of the descriptors
			if (pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER)
			{
				// If the sampler is a static sampler, no need to put it in the descriptor table
				const tinystl::unordered_hash_node<tinystl::string, Sampler*>* pNode = pRootSignatureDesc->mStaticSamplers.find(pDesc->mDesc.name).node;

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
				if (tinystl::string(pRes->name).to_lower().find("rootconstant") || pDesc->mDesc.type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
				{
					// Make the root param a 32 bit constant if the user explicitly specifies it in the shader
					pDesc->mDxType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
					pDesc->mDesc.type = DESCRIPTOR_TYPE_ROOT_CONSTANT;
					layouts[0].mRootConstants.emplace_back(pDesc);

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
				layouts[setIndex].mCbvSrvUavTable.emplace_back (pDesc);
			}

			layouts[setIndex].mDescriptorIndexMap[pDesc] = i;
		}

		uint32_t rootSize = calculate_root_signature_size(layouts.data(), layouts.getCount());

		// If the root signature size has crossed the recommended hardware limit try to optimize it
		if (rootSize > pRenderer->pActiveGpuSettings->mMaxRootSignatureDWORDS)
		{
			// Cconvert some of the root constants to root descriptors
			for (uint32_t i = 0; i < layouts.getCount(); ++i)
			{
				if (!layouts[i].mRootConstants.getCount())
					continue;

				UpdateFrequencyLayoutInfo& layout = layouts[i];
				DescriptorInfo** convertIt = layout.mRootConstants.end() - 1;
				DescriptorInfo** endIt = layout.mRootConstants.begin();
				while (pRenderer->pActiveGpuSettings->mMaxRootSignatureDWORDS < calculate_root_signature_size(layouts.data(), layouts.getCount()) && convertIt >= endIt)
				{
					layout.mCbvSrvUavTable.push_back(*convertIt);
					layout.mRootConstants.erase(layout.mRootConstants.find(*convertIt));
					(*convertIt)->mDxType = D3D12_ROOT_PARAMETER_TYPE_CBV;

					LOGWARNINGF("Converting root constant (%s) to root cbv to keep root signature size below hardware limit", (*convertIt)->mDesc.name);
				}
			}

			// If the root signature size is still above the recommended max, we need to place some of the less updated root descriptors
			// in descriptor tables of the same update frequency
			for (uint32_t i = 0; i < layouts.getCount(); ++i)
			{
				if (!layouts[i].mConstantParams.getCount())
					continue;

				UpdateFrequencyLayoutInfo& layout = layouts[i];

				while (pRenderer->pActiveGpuSettings->mMaxRootSignatureDWORDS < calculate_root_signature_size(layouts.data(), layouts.getCount()))
				{
					if (!layout.mConstantParams.getCount())
						break;
					DescriptorInfo** constantIt = layout.mConstantParams.end() - 1;
					DescriptorInfo** endIt = layout.mConstantParams.begin();
					if ((constantIt != endIt) || (constantIt == layout.mConstantParams.begin() && endIt == layout.mConstantParams.begin()))
					{
						layout.mCbvSrvUavTable.push_back(*constantIt);
						layout.mConstantParams.erase(layout.mConstantParams.find(*constantIt));

						LOGWARNINGF("Placing root descriptor (%s) in descriptor table to keep root signature size below hardware limit", (*constantIt)->mDesc.name);
					}
				}
			}
		}

		// We should never reach inside this if statement. If we do, something got messed up
		if (pRenderer->pActiveGpuSettings->mMaxRootSignatureDWORDS < calculate_root_signature_size(layouts.data(), layouts.getCount()))
		{
			LOGWARNING("Root Signature size greater than the specified max size");
			ASSERT(false);
		}

		// D3D12 currently has two versions of root signatures (1_0, 1_1)
		// So we fill the structs of both versions and in the end use the structs compatible with the supported version
		tinystl::vector <tinystl::vector <D3D12_DESCRIPTOR_RANGE1> > cbvSrvUavRange(layouts.getCount());
		tinystl::vector <tinystl::vector <D3D12_DESCRIPTOR_RANGE1> > samplerRange(layouts.getCount());
		tinystl::vector <D3D12_ROOT_PARAMETER1> rootParams;

		tinystl::vector <tinystl::vector <D3D12_DESCRIPTOR_RANGE> > cbvSrvUavRange_1_0(layouts.getCount());
		tinystl::vector <tinystl::vector <D3D12_DESCRIPTOR_RANGE> > samplerRange_1_0(layouts.getCount());
		tinystl::vector <D3D12_ROOT_PARAMETER> rootParams_1_0;

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

		for (uint32_t i = 0; i < layouts.getCount(); ++i)
		{
			cbvSrvUavRange[i].resize(layouts[i].mCbvSrvUavTable.size());
			cbvSrvUavRange_1_0[i].resize(layouts[i].mCbvSrvUavTable.size());

			samplerRange[i].resize(layouts[i].mSamplerTable.size());
			samplerRange_1_0[i].resize(layouts[i].mSamplerTable.size());
		}

		pRootSignature->pViewTableLayouts = (DescriptorSetLayout*)conf_calloc(layouts.getCount(), sizeof(*pRootSignature->pViewTableLayouts));
		pRootSignature->pSamplerTableLayouts = (DescriptorSetLayout*)conf_calloc(layouts.getCount(), sizeof(*pRootSignature->pSamplerTableLayouts));
		pRootSignature->pRootDescriptorLayouts = (RootDescriptorLayout*)conf_calloc(layouts.getCount(), sizeof(*pRootSignature->pRootDescriptorLayouts));
		pRootSignature->mDescriptorCount = shaderResources.getCount();

		pRootSignature->mRootConstantCount = layouts[0].mRootConstants.getCount();
		if (pRootSignature->mRootConstantCount)
			pRootSignature->pRootConstantLayouts = (RootConstantLayout*)conf_calloc(pRootSignature->mRootConstantCount, sizeof(*pRootSignature->pRootConstantLayouts));

		// Start collecting root parameters
		// Start with root descriptors since they will be the most frequently updated descriptors
		// This also makes sure that if we spill, the root descriptors in the front of the root signature will most likely still remain in the root

		// Collect all root descriptors
		// Put most frequently changed params first
		for (uint32_t i = layouts.getCount(); i-- > 0U;)
		{
			UpdateFrequencyLayoutInfo& layout = layouts[i];
			if (layout.mConstantParams.getCount())
			{
				RootDescriptorLayout& root = pRootSignature->pRootDescriptorLayouts[i];

				root.mRootDescriptorCount = layout.mConstantParams.getCount();
				root.pDescriptorIndices = (uint32_t*)conf_calloc(root.mRootDescriptorCount, sizeof(uint32_t));
				root.pRootIndices = (uint32_t*)conf_calloc(root.mRootDescriptorCount, sizeof(uint32_t));

				for (uint32_t descIndex = 0; descIndex < layout.mConstantParams.getCount(); ++descIndex)
				{
					DescriptorInfo* pDesc = layout.mConstantParams[descIndex];
					D3D12_ROOT_PARAMETER1 rootParam;
					D3D12_ROOT_PARAMETER rootParam_1_0;
					create_root_descriptor(pDesc, &rootParam);
					create_root_descriptor_1_0(pDesc, &rootParam_1_0);

					root.pDescriptorIndices[descIndex] = layout.mDescriptorIndexMap[pDesc];
					root.pRootIndices[descIndex] = rootParams.getCount();

					rootParams.push_back(rootParam);
					rootParams_1_0.push_back(rootParam_1_0);

					pDesc->mIndexInParent = descIndex;
				}
			}
		}

		// Collect all root constants
		for (uint32_t i = 0; i < pRootSignature->mRootConstantCount; ++i)
		{
			RootConstantLayout* pLayout = &pRootSignature->pRootConstantLayouts[i];
			DescriptorInfo* pDesc = layouts[0].mRootConstants[i];
			pDesc->mIndexInParent = i;
			pLayout->mRootIndex = rootParams.getCount();
			pLayout->mDescriptorIndex = layouts[0].mDescriptorIndexMap[pDesc];

			D3D12_ROOT_PARAMETER1 rootParam;
			D3D12_ROOT_PARAMETER rootParam_1_0;
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
				LOGINFOF("Root constant (%s) has (%u) 32 bit values. It is recommended to have root constant number less than 14", pDesc->mDesc.name, pDesc->mDesc.size);
			}
		}

		// Collect descriptor table parameters
		// Put most frequently changed descriptor tables in the front of the root signature
		for (uint32_t i = layouts.getCount(); i-- > 0U;)
		{
			UpdateFrequencyLayoutInfo& layout = layouts[i];

			// Fill the descriptor table layout for the view descriptor table of this update frequency
			if (layout.mCbvSrvUavTable.getCount())
			{
				// sort table by type (CBV/SRV/UAV) by register by space
				layout.mCbvSrvUavTable.sort([](DescriptorInfo* const& lhs, DescriptorInfo* const& rhs)
				{
					return (int)(lhs->mDesc.reg - rhs->mDesc.reg);
				});
				layout.mCbvSrvUavTable.sort([](DescriptorInfo* const& lhs, DescriptorInfo* const& rhs)
				{
					return (int)(lhs->mDesc.set - rhs->mDesc.set);
				});
				layout.mCbvSrvUavTable.sort([](DescriptorInfo* const& lhs, DescriptorInfo* const& rhs)
				{
					return (int)(lhs->mDesc.type - rhs->mDesc.type);
				});

				D3D12_ROOT_PARAMETER1 rootParam;
				create_descriptor_table(layout.mCbvSrvUavTable.getCount(), layout.mCbvSrvUavTable.data(), cbvSrvUavRange[i].data(), &rootParam);

				D3D12_ROOT_PARAMETER rootParam_1_0;
				create_descriptor_table_1_0(layout.mCbvSrvUavTable.getCount(), layout.mCbvSrvUavTable.data(), cbvSrvUavRange_1_0[i].data(), &rootParam_1_0);

				DescriptorSetLayout& table = pRootSignature->pViewTableLayouts[i];

				// Store some of the binding info which will be required later when binding the descriptor table
				// We need the root index when calling SetRootDescriptorTable
				table.mRootIndex = rootParams.getCount();
				table.mDescriptorCount = layout.mCbvSrvUavTable.getCount();
				table.pDescriptorIndices = (uint32_t*)conf_calloc(table.mDescriptorCount, sizeof(uint32_t));

				for (uint32_t descIndex = 0; descIndex < layout.mCbvSrvUavTable.getCount(); ++descIndex)
				{
					DescriptorInfo* pDesc = layout.mCbvSrvUavTable[descIndex];
					pDesc->mIndexInParent = descIndex;

					// Store the d3d12 related info in the descriptor to avoid constantly calling the util_to_dx mapping functions
					pDesc->mDxType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
					pDesc->mHandleIndex = table.mCumulativeDescriptorCount;

					// Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
					// This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays) in the descriptor table
					table.mCumulativeDescriptorCount += pDesc->mDesc.size;
					table.pDescriptorIndices[descIndex] = layout.mDescriptorIndexMap[pDesc];
				}

				rootParams.push_back(rootParam);
				rootParams_1_0.push_back(rootParam_1_0);
			}

			// Fill the descriptor table layout for the sampler descriptor table of this update frequency
			if (layout.mSamplerTable.getCount())
			{
				D3D12_ROOT_PARAMETER1 rootParam;
				create_descriptor_table(layout.mSamplerTable.getCount(), layout.mSamplerTable.data(), samplerRange[i].data(), &rootParam);

				D3D12_ROOT_PARAMETER rootParam_1_0;
				create_descriptor_table_1_0(layout.mSamplerTable.getCount(), layout.mSamplerTable.data(), samplerRange_1_0[i].data(), &rootParam_1_0);

				DescriptorSetLayout& table = pRootSignature->pSamplerTableLayouts[i];

				// Store some of the binding info which will be required later when binding the descriptor table
				// We need the root index when calling SetRootDescriptorTable
				table.mRootIndex = rootParams.getCount();
				table.mDescriptorCount = layout.mSamplerTable.getCount();
				table.pDescriptorIndices = (uint32_t*)conf_calloc(table.mDescriptorCount, sizeof(uint32_t));

				for (uint32_t descIndex = 0; descIndex < layout.mSamplerTable.getCount(); ++descIndex)
				{
					DescriptorInfo* pDesc = layout.mSamplerTable[descIndex];
					pDesc->mIndexInParent = descIndex;

					// Store the d3d12 related info in the descriptor to avoid constantly calling the util_to_dx mapping functions
					pDesc->mDxType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
					pDesc->mHandleIndex = table.mCumulativeDescriptorCount;

					// Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
					// This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays) in the descriptor table
					table.mCumulativeDescriptorCount += pDesc->mDesc.size;
					table.pDescriptorIndices[descIndex] = layout.mDescriptorIndexMap[pDesc];
				}

				rootParams.push_back(rootParam);
				rootParams_1_0.push_back(rootParam_1_0);
			}
		}

		DECLARE_ZERO (D3D12_FEATURE_DATA_ROOT_SIGNATURE, feature_data);
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		HRESULT hres = pRenderer->pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof (feature_data));

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
		CD3DX12_ROOT_SIGNATURE_DESC::Init(desc, (UINT)rootParams_1_0.size(), rootParams_1_0.empty() == false ? rootParams_1_0.data() : nullptr, (UINT)staticSamplerDescs.size(), staticSamplerDescs.data(), rootSignatureFlags);

		hres = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pRootSignature->pDxSerializedRootSignatureString, &error_msgs);
#else
		DECLARE_ZERO (D3D12_VERSIONED_ROOT_SIGNATURE_DESC, desc);

		if (D3D_ROOT_SIGNATURE_VERSION_1_1 == feature_data.HighestVersion)
		{
			desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
			desc.Desc_1_1.NumParameters = rootParams.getCount ();
			desc.Desc_1_1.pParameters = rootParams.data ();
			desc.Desc_1_1.NumStaticSamplers = (UINT)staticSamplerDescs.size();
			desc.Desc_1_1.pStaticSamplers = staticSamplerDescs.data();
			desc.Desc_1_1.Flags = rootSignatureFlags;
		}
		else
		{
			desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
			desc.Desc_1_0.NumParameters = rootParams_1_0.getCount ();
			desc.Desc_1_0.pParameters = rootParams_1_0.data ();
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

		hres = pRenderer->pDevice->CreateRootSignature(0, pRootSignature->pDxSerializedRootSignatureString->GetBufferPointer (), pRootSignature->pDxSerializedRootSignatureString->GetBufferSize (), IID_ARGS(&pRootSignature->pDxRootSignature));
		ASSERT(SUCCEEDED(hres));

		SAFE_RELEASE (error_msgs);

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

		pRootSignature->pDescriptorManagerMap.~unordered_map();

		SAFE_RELEASE(pRootSignature->pDxRootSignature);
		SAFE_RELEASE(pRootSignature->pDxSerializedRootSignatureString);

		for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
		{
			SAFE_FREE(pRootSignature->pViewTableLayouts[i].pDescriptorIndices);
			SAFE_FREE(pRootSignature->pSamplerTableLayouts[i].pDescriptorIndices);
			SAFE_FREE(pRootSignature->pRootDescriptorLayouts[i].pDescriptorIndices);
			SAFE_FREE(pRootSignature->pRootDescriptorLayouts[i].pRootIndices);
		}

		for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
		{
			SAFE_FREE((void*)pRootSignature->pDescriptors[i].mDesc.name);
		}

		pRootSignature->pDescriptorNameToIndexMap.~unordered_map();

		SAFE_FREE(pRootSignature->pDescriptors);
		SAFE_FREE(pRootSignature->pViewTableLayouts);
		SAFE_FREE(pRootSignature->pSamplerTableLayouts);
		SAFE_FREE(pRootSignature->pRootDescriptorLayouts);
		SAFE_FREE(pRootSignature->pRootConstantLayouts);

		SAFE_FREE(pRootSignature);
	}

	void addPipeline(Renderer* pRenderer, const GraphicsPipelineDesc* pDesc, Pipeline** ppPipeline)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc);
		ASSERT(pDesc->pShaderProgram);
		ASSERT(pDesc->pRootSignature);

		//allocate new pipeline
		Pipeline* pPipeline = (Pipeline*)conf_calloc(1, sizeof(*pPipeline));
		ASSERT(pPipeline);

		const Shader* pShaderProgram = pDesc->pShaderProgram;
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
		if (pShaderProgram->pDxVert) {
			VS.BytecodeLength = pShaderProgram->pDxVert->GetBufferSize();
			VS.pShaderBytecode = pShaderProgram->pDxVert->GetBufferPointer();
		}
		if (pShaderProgram->pDxFrag) {
			PS.BytecodeLength = pShaderProgram->pDxFrag->GetBufferSize();
			PS.pShaderBytecode = pShaderProgram->pDxFrag->GetBufferPointer();
		}
		if (pShaderProgram->pDxDomn) {
			DS.BytecodeLength = pShaderProgram->pDxDomn->GetBufferSize();
			DS.pShaderBytecode = pShaderProgram->pDxDomn->GetBufferPointer();
		}
		if (pShaderProgram->pDxHull) {
			HS.BytecodeLength = pShaderProgram->pDxHull->GetBufferSize();
			HS.pShaderBytecode = pShaderProgram->pDxHull->GetBufferPointer();
		}
		if (pShaderProgram->pDxGeom) {
			GS.BytecodeLength = pShaderProgram->pDxGeom->GetBufferSize();
			GS.pShaderBytecode = pShaderProgram->pDxGeom->GetBufferPointer();
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
				input_elements[input_elementCount].Format = util_to_dx_image_format(attrib->mFormat, false);
				input_elements[input_elementCount].InputSlot = attrib->mBinding;
				input_elements[input_elementCount].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
				input_elements[input_elementCount].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
				input_elements[input_elementCount].InstanceDataStepRate = 0;
				++input_elementCount;
			}
		}

		DECLARE_ZERO(D3D12_INPUT_LAYOUT_DESC, input_layout_desc);
		input_layout_desc.pInputElementDescs = input_elementCount ? input_elements : NULL;
		input_layout_desc.NumElements = input_elementCount;

		uint32_t render_target_count = min(pDesc->mRenderTargetCount, MAX_RENDER_TARGET_ATTACHMENTS);
		render_target_count = min(render_target_count, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

		DECLARE_ZERO(DXGI_SAMPLE_DESC, sample_desc);
		sample_desc.Count = (UINT)(pDesc->pDepthStencil ? pDesc->pDepthStencil->mDesc.mSampleCount : pDesc->ppRenderTargets[0]->mDesc.mSampleCount);
		sample_desc.Quality = (UINT)(pDesc->pDepthStencil ? pDesc->pDepthStencil->mDesc.mSampleQuality : pDesc->ppRenderTargets[0]->mDesc.mSampleQuality);

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

		pipeline_state_desc.RasterizerState =
			pDesc->pRasterizerState != NULL ? pDesc->pRasterizerState->mDxRasterizerDesc : pRenderer->pDefaultRasterizerState->mDxRasterizerDesc;

		pipeline_state_desc.DepthStencilState =
			pDesc->pDepthState != NULL ? pDesc->pDepthState->mDxDepthStencilDesc : pRenderer->pDefaultDepthState->mDxDepthStencilDesc;

		pipeline_state_desc.InputLayout = input_layout_desc;
		pipeline_state_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
		pipeline_state_desc.PrimitiveTopologyType = util_to_dx_primitive_topology_type(pDesc->mPrimitiveTopo);
		pipeline_state_desc.NumRenderTargets = render_target_count;
		pipeline_state_desc.DSVFormat = pDesc->pDepthStencil ? util_to_dx_image_format(pDesc->pDepthStencil->mDesc.mFormat, false) : DXGI_FORMAT_UNKNOWN;

		pipeline_state_desc.SampleDesc = sample_desc;
		pipeline_state_desc.NodeMask = 0;
		pipeline_state_desc.CachedPSO = cached_pso_desc;
		pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		for (uint32_t attrib_index = 0; attrib_index < render_target_count; ++attrib_index) {
			pipeline_state_desc.RTVFormats[attrib_index] = util_to_dx_image_format(pDesc->ppRenderTargets[attrib_index]->mDesc.mFormat, pDesc->ppRenderTargets[attrib_index]->mDesc.mSrgb);
		}

		HRESULT hres = pRenderer->pDevice->CreateGraphicsPipelineState(
			&pipeline_state_desc,
			__uuidof(pPipeline->pDxPipelineState), (void**)&(pPipeline->pDxPipelineState));
		ASSERT(SUCCEEDED(hres));


		D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
		switch (pPipeline->mGraphics.mPrimitiveTopo) {
		case PRIMITIVE_TOPO_POINT_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
		case PRIMITIVE_TOPO_LINE_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
		case PRIMITIVE_TOPO_LINE_STRIP: topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
		case PRIMITIVE_TOPO_TRI_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
		case PRIMITIVE_TOPO_TRI_STRIP: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
		case PRIMITIVE_TOPO_PATCH_LIST:
		{
			uint32_t controlPoint = pPipeline->mGraphics.pShaderProgram->mNumControlPoint;
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

		//allocate new pipeline
		Pipeline* pPipeline = (Pipeline*)conf_calloc(1, sizeof(*pPipeline));
		ASSERT(pPipeline);

		//copy pipeline settings
		memcpy(&(pPipeline->mCompute), pDesc, sizeof(*pDesc));
		pPipeline->mType = PIPELINE_TYPE_COMPUTE;

		//add pipeline specifying its for compute purposes
		DECLARE_ZERO(D3D12_SHADER_BYTECODE, CS);
		if (pDesc->pShaderProgram->pDxComp) {
			CS.BytecodeLength = pDesc->pShaderProgram->pDxComp->GetBufferSize();
			CS.pShaderBytecode = pDesc->pShaderProgram->pDxComp->GetBufferPointer();
		}

		DECLARE_ZERO(D3D12_CACHED_PIPELINE_STATE, cached_pso_desc);
		cached_pso_desc.pCachedBlob = NULL;
		cached_pso_desc.CachedBlobSizeInBytes = 0;

		DECLARE_ZERO(D3D12_COMPUTE_PIPELINE_STATE_DESC, pipeline_state_desc);
		pipeline_state_desc.pRootSignature = pDesc->pRootSignature->pDxRootSignature;
		pipeline_state_desc.CS = CS;
		pipeline_state_desc.NodeMask = 0;
		pipeline_state_desc.CachedPSO = cached_pso_desc;
#ifndef _DURANGO
		pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
#endif
		HRESULT hres = pRenderer->pDevice->CreateComputePipelineState(
			&pipeline_state_desc,
			__uuidof(pPipeline->pDxPipelineState), (void**)&(pPipeline->pDxPipelineState));
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

	void addBlendState(BlendState** ppBlendState, BlendConstant srcFactor, BlendConstant destFactor,
		BlendConstant srcAlphaFactor, BlendConstant destAlphaFactor,
		BlendMode blendMode /*= BlendMode::BM_REPLACE*/, BlendMode blendAlphaMode /*= BlendMode::BM_REPLACE*/,
		const int mask /*= ALL*/, const int MRTRenderTargetNumber /*= eBlendStateMRTRenderTarget0*/, const bool alphaToCoverage /*= false*/)
	{
		ASSERT(srcFactor < BlendConstant::MAX_BLEND_CONSTANTS);
		ASSERT(destFactor < BlendConstant::MAX_BLEND_CONSTANTS);
		ASSERT(srcAlphaFactor < BlendConstant::MAX_BLEND_CONSTANTS);
		ASSERT(destAlphaFactor < BlendConstant::MAX_BLEND_CONSTANTS);
		ASSERT(blendMode < BlendMode::MAX_BLEND_MODES);
		ASSERT(blendAlphaMode < BlendMode::MAX_BLEND_MODES);

		BOOL blendEnable = (gDx12BlendConstantTranslator[srcFactor] != D3D12_BLEND_ONE || gDx12BlendConstantTranslator[destFactor] != D3D12_BLEND_ZERO ||
			gDx12BlendConstantTranslator[srcAlphaFactor] != D3D12_BLEND_ONE || gDx12BlendConstantTranslator[destAlphaFactor] != D3D12_BLEND_ZERO);

		BlendState* pBlendState = (BlendState*)conf_calloc(1, sizeof(*pBlendState));

		pBlendState->mDxBlendDesc.AlphaToCoverageEnable = (BOOL)alphaToCoverage;
		pBlendState->mDxBlendDesc.IndependentBlendEnable = TRUE;
		for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; i++)
		{
			pBlendState->mDxBlendDesc.RenderTarget[i].RenderTargetWriteMask = (UINT8)mask;
			if (MRTRenderTargetNumber & (1 << i))
			{
				pBlendState->mDxBlendDesc.RenderTarget[i].BlendEnable = blendEnable;
				pBlendState->mDxBlendDesc.RenderTarget[i].BlendOp = gDx12BlendOpTranslator[blendMode];
				pBlendState->mDxBlendDesc.RenderTarget[i].SrcBlend = gDx12BlendConstantTranslator[srcFactor];
				pBlendState->mDxBlendDesc.RenderTarget[i].DestBlend = gDx12BlendConstantTranslator[destFactor];
				pBlendState->mDxBlendDesc.RenderTarget[i].BlendOpAlpha = gDx12BlendOpTranslator[blendAlphaMode];
				pBlendState->mDxBlendDesc.RenderTarget[i].SrcBlendAlpha = gDx12BlendConstantTranslator[srcAlphaFactor];
				pBlendState->mDxBlendDesc.RenderTarget[i].DestBlendAlpha = gDx12BlendConstantTranslator[destAlphaFactor];
			}
		}

		*ppBlendState = pBlendState;
	}

	void removeBlendState(BlendState* pBlendState)
	{
		SAFE_FREE(pBlendState);
	}

	void addDepthState(Renderer* pRenderer, DepthState** ppDepthState, const bool depthTest, const bool depthWrite,
		const CompareMode depthFunc /*= CompareMode::CMP_LEQUAL*/,
		const bool stencilTest /*= false*/,
		const uint8 stencilReadMask /*= 0xFF*/,
		const uint8 stencilWriteMask /*= 0xFF*/,
		const CompareMode stencilFrontFunc /*= CompareMode::CMP_ALWAYS*/,
		const StencilOp stencilFrontFail /*= StencilOp::STENCIL_OP_KEEP*/,
		const StencilOp depthFrontFail /*= StencilOp::STENCIL_OP_KEEP*/,
		const StencilOp stencilFrontPass /*= StencilOp::STENCIL_OP_KEEP*/,
		const CompareMode stencilBackFunc /*= CompareMode::CMP_ALWAYS*/,
		const StencilOp stencilBackFail /*= StencilOp::STENCIL_OP_KEEP*/,
		const StencilOp depthBackFail /*= StencilOp::STENCIL_OP_KEEP*/,
		const StencilOp stencilBackPass /*= StencilOp::STENCIL_OP_KEEP*/)
	{
    UNREF_PARAM(pRenderer);
		ASSERT(depthFunc < CompareMode::MAX_COMPARE_MODES);
		ASSERT(stencilFrontFunc < CompareMode::MAX_COMPARE_MODES);
		ASSERT(stencilFrontFail < StencilOp::MAX_STENCIL_OPS);
		ASSERT(depthFrontFail < StencilOp::MAX_STENCIL_OPS);
		ASSERT(stencilFrontPass < StencilOp::MAX_STENCIL_OPS);
		ASSERT(stencilBackFunc < CompareMode::MAX_COMPARE_MODES);
		ASSERT(stencilBackFail < StencilOp::MAX_STENCIL_OPS);
		ASSERT(depthBackFail < StencilOp::MAX_STENCIL_OPS);
		ASSERT(stencilBackPass < StencilOp::MAX_STENCIL_OPS);

		DepthState* pDepthState = (DepthState*)conf_calloc(1, sizeof(*pDepthState));

		pDepthState->mDxDepthStencilDesc.DepthEnable = (BOOL)depthTest;
		pDepthState->mDxDepthStencilDesc.DepthWriteMask = depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		pDepthState->mDxDepthStencilDesc.DepthFunc = gDx12ComparisonFuncTranslator[depthFunc];
		pDepthState->mDxDepthStencilDesc.StencilEnable = (BOOL)stencilTest;
		pDepthState->mDxDepthStencilDesc.StencilReadMask = stencilReadMask;
		pDepthState->mDxDepthStencilDesc.StencilWriteMask = stencilWriteMask;
		pDepthState->mDxDepthStencilDesc.BackFace.StencilFunc = gDx12ComparisonFuncTranslator[stencilBackFunc];
		pDepthState->mDxDepthStencilDesc.FrontFace.StencilFunc = gDx12ComparisonFuncTranslator[stencilFrontFunc];
		pDepthState->mDxDepthStencilDesc.BackFace.StencilDepthFailOp = gDx12StencilOpTranslator[depthBackFail];
		pDepthState->mDxDepthStencilDesc.FrontFace.StencilDepthFailOp = gDx12StencilOpTranslator[depthFrontFail];
		pDepthState->mDxDepthStencilDesc.BackFace.StencilFailOp = gDx12StencilOpTranslator[stencilBackFail];
		pDepthState->mDxDepthStencilDesc.FrontFace.StencilFailOp = gDx12StencilOpTranslator[stencilFrontFail];
		pDepthState->mDxDepthStencilDesc.BackFace.StencilPassOp = gDx12StencilOpTranslator[stencilFrontPass];
		pDepthState->mDxDepthStencilDesc.FrontFace.StencilPassOp = gDx12StencilOpTranslator[stencilBackPass];

		*ppDepthState = pDepthState;
	}

	void removeDepthState(DepthState* pDepthState)
	{
		SAFE_FREE(pDepthState);
	}

	void addRasterizerState(RasterizerState** ppRasterizerState, const CullMode cullMode, const int depthBias /*= 0*/, const float slopeScaledDepthBias /*= 0*/, const FillMode fillMode /*= FillMode::FILL_MODE_SOLID*/, const bool multiSample /*= false*/, const bool scissor /*= false*/)
	{
    UNREF_PARAM(scissor);
		ASSERT(fillMode < FillMode::MAX_FILL_MODES);
		ASSERT(cullMode < CullMode::MAX_CULL_MODES);

		RasterizerState* pRasterizerState = (RasterizerState*)conf_calloc(1, sizeof(*pRasterizerState));

		pRasterizerState->mDxRasterizerDesc.FillMode = gDx12FillModeTranslator[fillMode];
		pRasterizerState->mDxRasterizerDesc.CullMode = gDx12CullModeTranslator[cullMode];
		pRasterizerState->mDxRasterizerDesc.FrontCounterClockwise = TRUE;
		pRasterizerState->mDxRasterizerDesc.DepthBias = depthBias;
		pRasterizerState->mDxRasterizerDesc.DepthBiasClamp = 0.0f;
		pRasterizerState->mDxRasterizerDesc.SlopeScaledDepthBias = slopeScaledDepthBias;
		pRasterizerState->mDxRasterizerDesc.DepthClipEnable = TRUE;
		pRasterizerState->mDxRasterizerDesc.MultisampleEnable = multiSample ? TRUE : FALSE;
		pRasterizerState->mDxRasterizerDesc.AntialiasedLineEnable = FALSE;
		pRasterizerState->mDxRasterizerDesc.ForcedSampleCount = 0;
		pRasterizerState->mDxRasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		*ppRasterizerState = pRasterizerState;
	}

	void removeRasterizerState(RasterizerState* pRasterizerState)
	{
		SAFE_FREE(pRasterizerState);
	}
	// -------------------------------------------------------------------------------------------------
	// Command buffer functions
	// -------------------------------------------------------------------------------------------------
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
			ID3D12DescriptorHeap* heaps[2] = {
				pCmd->pCmdPool->pRenderer->pCbvSrvUavHeap->pCurrentHeap,
				pCmd->pCmdPool->pRenderer->pSamplerHeap->pCurrentHeap
			};
			pCmd->pDxCmdList->SetDescriptorHeaps(2, heaps);
		}

		pCmd->pBoundRootSignature = NULL;
		pCmd->mViewPosition = 0;
		pCmd->mSamplerPosition = 0;
	}

	void endCmd(Cmd* pCmd)
	{
		ASSERT(pCmd);

		cmdFlushBarriers(pCmd);

		ASSERT(pCmd->pDxCmdList);

		HRESULT hres = pCmd->pDxCmdList->Close();
		ASSERT(SUCCEEDED(hres));
	}

	void cmdBeginRender(Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil, const LoadActionsDesc* pLoadActions/* = NULL*/)
	{
		ASSERT(pCmd);
		ASSERT(ppRenderTargets || pDepthStencil);

		//start frame
		ASSERT(pCmd->pDxCmdList);

		D3D12_CPU_DESCRIPTOR_HANDLE* p_dsv_handle = NULL;
		D3D12_CPU_DESCRIPTOR_HANDLE* p_rtv_handles = renderTargetCount ?
			(D3D12_CPU_DESCRIPTOR_HANDLE*)alloca(renderTargetCount * sizeof(D3D12_CPU_DESCRIPTOR_HANDLE)) : NULL;
		for (uint32_t i = 0; i < renderTargetCount; ++i)
			p_rtv_handles[i] = ppRenderTargets[i]->mDxRtvHandle;

		if (pDepthStencil) {
			p_dsv_handle = &pDepthStencil->mDxDsvHandle;
		}

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
				pCmd->pDxCmdList->ClearDepthStencilView(*p_dsv_handle, flags, pLoadActions->mClearDepth.depth, (UINT8)pLoadActions->mClearDepth.stencil, 0, NULL);
			}
		}
	}

	void cmdEndRender(Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil)
	{

    UNREF_PARAM(pCmd);
    UNREF_PARAM(renderTargetCount);
    UNREF_PARAM(ppRenderTargets);
    UNREF_PARAM(pDepthStencil);
		// No-op on DirectX-12
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

		if (pPipeline->mType == PIPELINE_TYPE_GRAPHICS) {
			pCmd->pDxCmdList->IASetPrimitiveTopology(pPipeline->mDxPrimitiveTopology);
		}

		pCmd->pDxCmdList->SetPipelineState(pPipeline->pDxPipelineState);
	}

	void cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer)
	{
		ASSERT(pCmd);
		ASSERT(pBuffer);
		ASSERT(pCmd->pDxCmdList);
		ASSERT(D3D12_GPU_VIRTUAL_ADDRESS_NULL != pBuffer->mDxIndexBufferView.BufferLocation);

#ifdef _DURANGO
		BufferBarrier bufferBarriers[] = {
			{ pBuffer, RESOURCE_STATE_INDEX_BUFFER }
		};
		cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, false);
#endif
		//bind given index buffer
		pCmd->pDxCmdList->IASetIndexBuffer(&(pBuffer->mDxIndexBufferView));
	}

	void cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers)
	{
		ASSERT(pCmd);
		ASSERT(0 != bufferCount);
		ASSERT(ppBuffers);
		ASSERT(pCmd->pDxCmdList);
		//bind given vertex buffer

		DECLARE_ZERO(D3D12_VERTEX_BUFFER_VIEW, views[MAX_VERTEX_ATTRIBS]);
		for (uint32_t i = 0; i < bufferCount; ++i) {
			ASSERT(D3D12_GPU_VIRTUAL_ADDRESS_NULL != ppBuffers[i]->mDxVertexBufferView.BufferLocation);

			views[i] = ppBuffers[i]->mDxVertexBufferView;

#ifdef _DURANGO
			BufferBarrier bufferBarriers[] = {
				{ ppBuffers[i], RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER }
			};
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

		pCmd->pDxCmdList->DrawInstanced(
			(UINT)vertexCount,
			(UINT)1,
			(UINT)firstVertex,
			(UINT)0
		);
	}

	void cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount)
	{
		ASSERT(pCmd);

		//draw given vertices
		ASSERT(pCmd->pDxCmdList);

		pCmd->pDxCmdList->DrawInstanced(
			(UINT)vertexCount,
			(UINT)instanceCount,
			(UINT)firstVertex,
			(UINT)0
		);
	}

	void cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex)
	{
		ASSERT(pCmd);

		//draw indexed mesh
		ASSERT(pCmd->pDxCmdList);

		pCmd->pDxCmdList->DrawIndexedInstanced(
			(UINT)indexCount,
			(UINT)1,
			(UINT)firstIndex,
			(UINT)0,
			(UINT)0
		);
	}

	void cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount)
	{
		ASSERT(pCmd);

		//draw indexed mesh
		ASSERT(pCmd->pDxCmdList);

		pCmd->pDxCmdList->DrawIndexedInstanced(
			(UINT)indexCount,
			(UINT)instanceCount,
			(UINT)firstIndex,
			(UINT)0,
			(UINT)0
		);
	}

	void cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
		ASSERT(pCmd);

		//dispatch given command
		ASSERT(pCmd->pDxCmdList != NULL);

		pCmd->pDxCmdList->Dispatch(groupCountX, groupCountY, groupCountZ);
	}

	void cmdResourceBarrier(Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers, bool batch)
	{
		D3D12_RESOURCE_BARRIER* barriers = (D3D12_RESOURCE_BARRIER*)alloca((numBufferBarriers + numTextureBarriers) * sizeof(D3D12_RESOURCE_BARRIER));
		uint32_t transitionCount = 0;

		for (uint32_t i = 0; i < numBufferBarriers; ++i)
		{
			BufferBarrier* pTransBarrier = &pBufferBarriers[i];
			D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
			Buffer* pBuffer = pTransBarrier->pBuffer;

			// Only transition GPU visible resources
			if (pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY || pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
			{
				//if (!(pBuffer->mCurrentState & pTransBarrier->mNewState) && pBuffer->mCurrentState != pTransBarrier->mNewState)
				if (pBuffer->mCurrentState != pTransBarrier->mNewState && pBuffer->mCurrentState != pTransBarrier->mNewState)
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
			}
		}
		for (uint32_t i = 0; i < numTextureBarriers; ++i)
		{
			TextureBarrier* pTransBarrier = &pTextureBarriers[i];
			D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
			Texture* pTexture = pTransBarrier->pTexture;
			{
				if (!(pTexture->mCurrentState & pTransBarrier->mNewState) && pTexture->mCurrentState != pTransBarrier->mNewState)
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
		uint32_t transitionCount = 0;

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
		BufferBarrier bufferBarriers[] = {
			{ pBuffer, RESOURCE_STATE_COPY_DEST }
		};
		cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, false);
#endif

		pCmd->pDxCmdList->CopyBufferRegion(pBuffer->pDxResource, dstOffset,
			pSrcBuffer->pDxResource, srcOffset,
			size);

#ifdef _DURANGO
		{
			bufferBarriers[0].mNewState = (pBuffer->mDesc.mUsage == BUFFER_USAGE_VERTEX ? RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER : RESOURCE_STATE_COMMON);
			cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, false);
		}
#endif
	}

	void cmdUpdateSubresources(Cmd* pCmd, uint32_t startSubresource, uint32_t numSubresources, SubresourceDataDesc* pSubresources, Buffer* pIntermediate, uint64_t intermediateOffset, Texture* pTexture)
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
		UINT64* pRowSizesInBytes = (UINT64*)(pLayouts + numSubresources);
		UINT* pNumRows = (UINT*)(pRowSizesInBytes + numSubresources);

		D3D12_RESOURCE_DESC Desc = pTexture->pDxResource->GetDesc();
		pCmd->pCmdPool->pRenderer->pDevice->GetCopyableFootprints(&Desc, startSubresource, numSubresources, intermediateOffset, pLayouts, pNumRows, pRowSizesInBytes, &RequiredSize);

		for (UINT i = 0; i < numSubresources; ++i)
		{
			if (pRowSizesInBytes[i] > (UINT64)-1)
				return;

			D3D12_MEMCPY_DEST DestData = { (BYTE*)pIntermediate->pCpuMappedAddress + pLayouts[i].Offset, pLayouts[i].Footprint.RowPitch, pLayouts[i].Footprint.RowPitch * pNumRows[i] };

			// Row-by-row memcpy
			for (UINT z = 0; z < pLayouts[i].Footprint.Depth; ++z)
			{
				BYTE* pDestSlice = (BYTE*)(DestData.pData) + DestData.SlicePitch * z;
				const BYTE* pSrcSlice = (BYTE*)(pSubresources[i].pData) + pSubresources[i].mSlicePitch * z;
				for (UINT y = 0; y < pNumRows[i]; ++y)
				{
					memcpy(pDestSlice + DestData.RowPitch * y,
						pSrcSlice + pSubresources[i].mRowPitch * y,
						pRowSizesInBytes[i]);
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

			pCmd->pDxCmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
		}
	}

	void acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pSwapChainImageIndex)
	{
    UNREF_PARAM(pSignalSemaphore);
    UNREF_PARAM(pFence);
		ASSERT(pRenderer);

		//get latest backbuffer image
		ASSERT(pSwapChain->pSwapChain);
		ASSERT(pSwapChainImageIndex);

		pSwapChain->mImageIndex = *pSwapChainImageIndex = fnHookGetSwapChainImageIndex(pSwapChain);
	}

	void queueSubmit(
		Queue*      pQueue,
		uint32_t       cmdCount,
		Cmd**       ppCmds,
		Fence* pFence,
		uint32_t       waitSemaphoreCount,
		Semaphore** ppWaitSemaphores,
		uint32_t       signalSemaphoreCount,
		Semaphore** ppSignalSemaphores
	)
	{
		//ASSERT that given cmd list and given params are valid
		ASSERT(pQueue);
		ASSERT(cmdCount > 0);
		ASSERT(ppCmds);
		if (waitSemaphoreCount > 0) {
			ASSERT(ppWaitSemaphores);
		}
		if (signalSemaphoreCount > 0) {
			ASSERT(ppSignalSemaphores);
		}

		//execute given command list
		ASSERT(pQueue->pDxQueue);

		cmdCount = cmdCount > MAX_SUBMIT_CMDS ? MAX_SUBMIT_CMDS : cmdCount;
		ID3D12CommandList** cmds = (ID3D12CommandList**)alloca(cmdCount * sizeof(ID3D12CommandList*));
		for (uint32_t i = 0; i < cmdCount; ++i) {
			cmds[i] = ppCmds[i]->pDxCmdList;
		}

		for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
			pQueue->pDxQueue->Wait(ppWaitSemaphores[i]->pFence->pDxFence, ppWaitSemaphores[i]->pFence->mFenceValue - 1);

		pQueue->pDxQueue->ExecuteCommandLists(cmdCount, cmds);
		pQueue->pDxQueue->Signal(pFence->pDxFence, pFence->mFenceValue++);

		for (uint32_t i = 0; i < signalSemaphoreCount; ++i)
			pQueue->pDxQueue->Signal(ppSignalSemaphores[i]->pFence->pDxFence, ppSignalSemaphores[i]->pFence->mFenceValue++);
	}

	void queuePresent(Queue* pQueue, SwapChain* pSwapChain, uint32_t swapChainImageIndex, uint32_t waitSemaphoreCount, Semaphore** ppWaitSemaphores)
	{
		UNREF_PARAM(swapChainImageIndex);
		ASSERT(pQueue);
		ASSERT(pSwapChain->pSwapChain);

		if (waitSemaphoreCount > 0) {
			ASSERT(ppWaitSemaphores);
		}

		pSwapChain->pSwapChain->Present(pSwapChain->mDxSyncInterval, pSwapChain->mFlags);

		HRESULT hr = pQueue->pRenderer->pDevice->GetDeviceRemovedReason();
		if (FAILED(hr))
			ASSERT(false);//TODO: let's do something with the error
	}

	bool queueSignal(Queue* pQueue, Fence* fence, uint64_t value)
	{
		ASSERT(pQueue);
		ASSERT(fence);

		HRESULT hres = pQueue->pDxQueue->Signal(fence->pDxFence, value);

		return SUCCEEDED(hres);
	}
	
	void waitForFences(Queue* pQueue, uint32_t fenceCount, Fence** ppFences)
	{
		// Wait for fence completion
		for (uint32_t i = 0; i < fenceCount; ++i)
		{
			// TODO: we should consider the use case of this function. 
			// Usecase A: If we want to wait for an already signaled fence, we shouldn't issue new signal here.
			// Usecase B: If we want to wait for all works in the queue complete, we should signal and wait.
			// Our current vis buffer implemnetation uses this function as Usecase A. Thus we should not signal again.
			//pQueue->pDxQueue->Signal(ppFences[i]->pDxFence, ppFences[i]->mFenceValue++);
			
			FenceStatus fenceStatus;
			getFenceStatus(ppFences[i], &fenceStatus);
			uint64_t fenceValue = ppFences[i]->mFenceValue - 1;
			//if (completedValue < fenceValue)
			if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			{
				ppFences[i]->pDxFence->SetEventOnCompletion(fenceValue, ppFences[i]->pDxWaitIdleFenceEvent);
				WaitForSingleObject(ppFences[i]->pDxWaitIdleFenceEvent, INFINITE);
			}
		}
	}

	void getFenceStatus(Fence* pFence, FenceStatus* pFenceStatus)
	{
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

	// -------------------------------------------------------------------------------------------------
	// Utility functions
	// -------------------------------------------------------------------------------------------------
	bool isImageFormatSupported(ImageFormat::Enum format)
	{
		//verifies that given image format is valid
		bool result = false;
		switch (format) {
			// 1 channel
		case ImageFormat::R8: result = true; break;
		case ImageFormat::R16: result = true; break;
		case ImageFormat::R16F: result = true; break;
		case ImageFormat::R32UI: result = true; break;
		case ImageFormat::R32F: result = true; break;
			// 2 channel
		case ImageFormat::RG8: result = true; break;
		case ImageFormat::RG16: result = true; break;
		case ImageFormat::RG16F: result = true; break;
		case ImageFormat::RG32UI: result = true; break;
		case ImageFormat::RG32F: result = true; break;
			// 3 channel
		case ImageFormat::RGB8: result = true; break;
		case ImageFormat::RGB16: result = true; break;
		case ImageFormat::RGB16F: result = true; break;
		case ImageFormat::RGB32UI: result = true; break;
		case ImageFormat::RGB32F: result = true; break;
			// 4 channel
		case ImageFormat::RGBA8: result = true; break;
		case ImageFormat::RGBA16: result = true; break;
		case ImageFormat::RGBA16F: result = true; break;
		case ImageFormat::RGBA32UI: result = true; break;
		case ImageFormat::RGBA32F: result = true; break;
		}
		return result;
	}

	uint32_t calculateVertexLayoutStride(const VertexLayout* pVertexLayout)
	{
		ASSERT(pVertexLayout);

		uint32_t result = 0;
		for (uint32_t i = 0; i < pVertexLayout->mAttribCount; ++i) {
			result += calculateImageFormatStride(pVertexLayout->mAttribs[i].mFormat);
		}
		return result;
	}

	ImageFormat::Enum convertDXToInternalImageFormat(DXGI_FORMAT format)
	{
		ImageFormat::Enum result = ImageFormat::None;
		switch (format) {
			// 1 channel
		case DXGI_FORMAT_R8_UNORM: result = ImageFormat::R8; break;
		case DXGI_FORMAT_R16_UNORM: result = ImageFormat::R16; break;
		case DXGI_FORMAT_R16_FLOAT: result = ImageFormat::R16F; break;
		case DXGI_FORMAT_R32_UINT: result = ImageFormat::R32UI; break;
		case DXGI_FORMAT_R32_FLOAT: result = ImageFormat::R32F; break;
			// 2 channel
		case DXGI_FORMAT_R8G8_UNORM: result = ImageFormat::RG8; break;
		case DXGI_FORMAT_R16G16_UNORM: result = ImageFormat::RG16; break;
		case DXGI_FORMAT_R16G16_FLOAT: result = ImageFormat::RG16F; break;
		case DXGI_FORMAT_R32G32_UINT: result = ImageFormat::RG32UI; break;
		case DXGI_FORMAT_R32G32_FLOAT: result = ImageFormat::RG32F; break;
			// 3 channel
		case DXGI_FORMAT_R32G32B32_UINT: result = ImageFormat::RGB32UI; break;
		case DXGI_FORMAT_R32G32B32_FLOAT: result = ImageFormat::RGB32F; break;
			// 4 channel
		case DXGI_FORMAT_B8G8R8A8_UNORM: result = ImageFormat::BGRA8; break;
		case DXGI_FORMAT_R8G8B8A8_UNORM: result = ImageFormat::RGBA8; break;
		case DXGI_FORMAT_R16G16B16A16_UNORM: result = ImageFormat::RGBA16; break;
		case DXGI_FORMAT_R16G16B16A16_FLOAT: result = ImageFormat::RGBA16F; break;
		case DXGI_FORMAT_R32G32B32A32_UINT: result = ImageFormat::RGBA32UI; break;
		case DXGI_FORMAT_R32G32B32A32_FLOAT: result = ImageFormat::RGBA32F; break;
			// Depth/stencil
		case DXGI_FORMAT_D16_UNORM: result = ImageFormat::D16; break;
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: result = ImageFormat::X8D24PAX32; break;
		case DXGI_FORMAT_D32_FLOAT: result = ImageFormat::D32F; break;
		case DXGI_FORMAT_D24_UNORM_S8_UINT: result = ImageFormat::D24S8; break;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: result = ImageFormat::D32S8; break;
		}
		return result;
	}
	// Array used to translate 'Filter' into 'D3D12_PRIMITIVE_TOPOLOGY'
	const D3D12_FILTER gDX12FilterTranslator[] =
	{
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
		D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_FILTER_ANISOTROPIC
	};

	// -------------------------------------------------------------------------------------------------
	// Internal utility functions
	// -------------------------------------------------------------------------------------------------
	D3D12_FILTER util_to_dx_filter(FilterType minFilter, FilterType magFilter, MipMapMode  mipMapMode)
	{
    UNREF_PARAM(magFilter);
    UNREF_PARAM(mipMapMode);
		return gDX12FilterTranslator[minFilter];
		
#if 0 //Unreachable
    // control bit : minFilter  magFilter   mipMapMode
		//     point   :     00         00          00
		//     linear  :     01         01          01
		// ex : trilinear == 010101
		int filter = (minFilter << 4) | (magFilter << 2) | mipMapMode;
		return  (D3D12_FILTER)filter;
#endif
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
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			return DXGI_FORMAT_R8G8B8A8_UNORM;

		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			return DXGI_FORMAT_B8G8R8A8_UNORM;

		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			return DXGI_FORMAT_B8G8R8X8_UNORM;

		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_R32_FLOAT:
			return DXGI_FORMAT_R32_FLOAT;

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
		case DXGI_FORMAT_D16_UNORM:

			ErrorMsg("Requested a UAV format for a depth stencil format");
#endif

		default:
			return defaultFormat;
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
		case DXGI_FORMAT_R16_UNORM:
			return DXGI_FORMAT_D16_UNORM;

		default:
			return defaultFormat;
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
		case DXGI_FORMAT_R16_UNORM:
			return DXGI_FORMAT_R16_UNORM;

		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			return DXGI_FORMAT_R8G8B8A8_UNORM;

		default:
			return defaultFormat;
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
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	}

	DXGI_FORMAT util_to_dx_swapchain_format(ImageFormat::Enum format)
	{
		DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;

		// FLIP_DISCARD and FLIP_SEQEUNTIAL swapchain buffers only support these formats
		switch (format)
		{
		case ImageFormat::RGBA16F:
			result = DXGI_FORMAT_R16G16B16A16_FLOAT;
		case ImageFormat::BGRA8:
			result = DXGI_FORMAT_B8G8R8A8_UNORM;
			break;
		case ImageFormat::RGBA8:
			result = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case ImageFormat::RGB10A2:
			result = DXGI_FORMAT_R10G10B10A2_UNORM;
			break;
		default:
			break;
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
				if (result == DXGI_FORMAT_R8G8B8A8_UNORM) result = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				else if (result == DXGI_FORMAT_B8G8R8A8_UNORM) result = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
				else if (result == DXGI_FORMAT_B8G8R8X8_UNORM) result = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
				else if (result == DXGI_FORMAT_BC1_UNORM) result = DXGI_FORMAT_BC1_UNORM_SRGB;
				else if (result == DXGI_FORMAT_BC2_UNORM) result = DXGI_FORMAT_BC2_UNORM_SRGB;
				else if (result == DXGI_FORMAT_BC3_UNORM) result = DXGI_FORMAT_BC3_UNORM_SRGB;
				else if (result == DXGI_FORMAT_BC7_UNORM) result = DXGI_FORMAT_BC7_UNORM_SRGB;
			}
		}

		return result;
	}

	D3D12_SHADER_VISIBILITY util_to_dx_shader_visibility(ShaderStage stages)
	{
		D3D12_SHADER_VISIBILITY res = D3D12_SHADER_VISIBILITY_ALL;
		uint32_t stageCount = 0;

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
			return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		case DESCRIPTOR_TYPE_TEXTURE:
		case DESCRIPTOR_TYPE_BUFFER:
			return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case DESCRIPTOR_TYPE_RW_BUFFER:
		case DESCRIPTOR_TYPE_RW_TEXTURE:
			return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case DESCRIPTOR_TYPE_SAMPLER:
			return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		default:
			ASSERT("Invalid DescriptorInfo Type");
			return (D3D12_DESCRIPTOR_RANGE_TYPE)-1;
		}
	}

	D3D12_RESOURCE_STATES util_to_dx_resource_state(ResourceState state)
	{
		D3D12_RESOURCE_STATES ret = D3D12_RESOURCE_STATE_COMMON;

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
	// -------------------------------------------------------------------------------------------------
	// Internal init functions
	// -------------------------------------------------------------------------------------------------
	void AddDevice (Renderer* pRenderer)
	{
#if defined( _DEBUG ) || defined ( PROFILE )
		//add debug layer if in debug mode
		if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(pRenderer->pDXDebug), (void**)&(pRenderer->pDXDebug)))) {
			if (fnHookEnableDebugLayer != NULL)
				fnHookEnableDebugLayer(pRenderer);
		}
#endif
		
		D3D_FEATURE_LEVEL feature_levels[4] =
		{
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
		};

#ifdef _DURANGO
		// Create the DX12 API device object.
		HRESULT hres = D3D12CreateDevice(
			nullptr,
			feature_levels[0],
			IID_ARGS(&pRenderer->pDevice)
		);
		ASSERT(SUCCEEDED(hres));

		// First, retrieve the underlying DXGI device from the D3D device.
		IDXGIDevice1* dxgiDevice;
		hres = pRenderer->pDevice->QueryInterface(IID_ARGS(&dxgiDevice));
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
			IDXGIAdapter* pGpu = NULL;
			D3D_FEATURE_LEVEL mMaxSupportedFeatureLevel = (D3D_FEATURE_LEVEL)0;
		} GpuDesc;

		GpuDesc gpuDesc[MAX_GPUS] = {};
		dxgiAdapter->QueryInterface(IID_ARGS(&gpuDesc->pGpu));
		gpuDesc->mMaxSupportedFeatureLevel = feature_levels[0];
		pRenderer->mNumOfGPUs = 1;

		dxgiAdapter->Release();
#else
		UINT flags = 0;
#if defined( _DEBUG )
		flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
		HRESULT hres = CreateDXGIFactory2(flags, __uuidof(pRenderer->pDXGIFactory), (void**)&(pRenderer->pDXGIFactory));
		ASSERT (SUCCEEDED (hres));

		typedef struct GpuDesc
		{
			IDXGIAdapter3* pGpu = NULL;
			D3D_FEATURE_LEVEL mMaxSupportedFeatureLevel = (D3D_FEATURE_LEVEL)0;
			SIZE_T mDedicatedVideoMemory = 0;
		} GpuDesc;

		GpuDesc gpuDesc[MAX_GPUS] = {};

		IDXGIAdapter3* adapter = NULL;
		for (UINT i = 0; DXGI_ERROR_NOT_FOUND != pRenderer->pDXGIFactory->EnumAdapters1(i, (IDXGIAdapter1**)&adapter); ++i)
		{
			DECLARE_ZERO (DXGI_ADAPTER_DESC1, desc);
			adapter->GetDesc1(&desc);

			if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
			{
				for (uint32_t level = 0; level < _countof (feature_levels); ++level)
				{
					// Make sure the adapter can support a D3D12 device
					if (SUCCEEDED (D3D12CreateDevice(adapter, feature_levels[level], __uuidof(pRenderer->pDevice), NULL)))
					{
						hres = adapter->QueryInterface(IID_ARGS(&gpuDesc[pRenderer->mNumOfGPUs].pGpu));
						if (SUCCEEDED (hres))
						{
							gpuDesc[pRenderer->mNumOfGPUs].mMaxSupportedFeatureLevel = feature_levels[level];
							gpuDesc[pRenderer->mNumOfGPUs].mDedicatedVideoMemory = desc.DedicatedVideoMemory;
							++pRenderer->mNumOfGPUs;
							break;
						}
					}
				}
			}

			adapter->Release();
		}

		ASSERT (pRenderer->mNumOfGPUs > 0);
		// Sort GPUs to get highest feature level gpu at front
		qsort(gpuDesc, pRenderer->mNumOfGPUs, sizeof (GpuDesc), [](const void* lhs, const void* rhs) {
			GpuDesc* gpu1 = (GpuDesc*)lhs;
			GpuDesc* gpu2 = (GpuDesc*)rhs;
			// Check feature level first, sort the greatest feature level gpu to the front
			if ((int)gpu1->mMaxSupportedFeatureLevel != (int)gpu2->mMaxSupportedFeatureLevel)
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
			pRenderer->pGPUs[i] = gpuDesc[i].pGpu;
			pRenderer->mGpuSettings[i].mUniformBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
			pRenderer->mGpuSettings[i].mMultiDrawIndirect = true;
			pRenderer->mGpuSettings[i].mMaxVertexInputBindings = 32U;

			// Determine root signature size for this gpu driver
			DXGI_ADAPTER_DESC adapterDesc;
			pRenderer->pGPUs[i]->GetDesc(&adapterDesc);
			pRenderer->mGpuSettings[i].mMaxRootSignatureDWORDS = gRootSignatureDWORDS[util_to_internal_gpu_vendor(adapterDesc.VendorId)];
			LOGINFOF("GPU[%i] detected. Vendor ID: %x GPU Name: %S",i, adapterDesc.VendorId, adapterDesc.Description);
		}

		// Get the latest and greatest feature level gpu
		pRenderer->pActiveGPU = pRenderer->pGPUs[0];
		pRenderer->pActiveGpuSettings = &pRenderer->mGpuSettings[0];
		LOGINFOF("GPU[0] is selected as default GPU");

		ASSERT (pRenderer->pActiveGPU != NULL);

		// Load functions
		{
#ifdef _DURANGO
			HMODULE module = get_d3d12_module_handle();
#else
			HMODULE module = ::GetModuleHandle(TEXT("d3d12.dll"));
#endif
			fnD3D12CreateRootSignatureDeserializer
				= (PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress (module,
					"D3D12SerializeVersionedRootSignature");

			fnD3D12SerializeVersionedRootSignature
				= (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress (module,
					"D3D12SerializeVersionedRootSignature");

			fnD3D12CreateVersionedRootSignatureDeserializer
				= (PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress (module,
					"D3D12CreateVersionedRootSignatureDeserializer");
		}

#ifndef _DURANGO
		hres = D3D12CreateDevice(pRenderer->pActiveGPU, gpuDesc[0].mMaxSupportedFeatureLevel, IID_ARGS(&pRenderer->pDevice));
		ASSERT (SUCCEEDED (hres));
#endif

		//pRenderer->mSettings.mDxFeatureLevel = target_feature_level;  // this is not used anywhere?
	}

	void RemoveDevice(Renderer* pRenderer)
	{
		SAFE_RELEASE(pRenderer->pDXGIFactory);

		for (uint32_t i = 0; i < pRenderer->mNumOfGPUs; ++i) {
			SAFE_RELEASE(pRenderer->pGPUs[i]);
		}

		SAFE_RELEASE(pRenderer->pDevice);

#if defined(_DEBUG)
		SAFE_RELEASE (pRenderer->pDXDebug);
#endif
	}
	// -------------------------------------------------------------------------------------------------
	// Internal queue functions
	// -------------------------------------------------------------------------------------------------
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
		case INDIRECT_DESCRIPTOR_TABLE:  LOGERROR("Dx12 Doesn't support DescriptorTable in Indirect Command"); break;
		case INDIRECT_PIPELINE:  LOGERROR("Dx12 Doesn't support the Pipeline in Indirect Command"); break;
		}
		return res;
	}

	void addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
	{
		ASSERT(pRenderer);
		ASSERT(pDesc);
		ASSERT(pDesc->pRootSignature);
		ASSERT(pDesc->pArgDescs);

		CommandSignature* pCommandSignature = (CommandSignature*)conf_calloc(1, sizeof(*pCommandSignature));
		pCommandSignature->mDesc = *pDesc;

		bool change = false;
		// calculate size through arguement types
		uint32_t commandStride = 0;

		// temporary use
		D3D12_INDIRECT_ARGUMENT_DESC* argumentDescs = (D3D12_INDIRECT_ARGUMENT_DESC*)alloca(pDesc->mIndirectArgCount * sizeof(D3D12_INDIRECT_ARGUMENT_DESC));

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
			case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
				commandStride += sizeof(IndirectDrawArguments);
				break;
			case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
				commandStride += sizeof(IndirectDrawIndexArguments);
				break;
			case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
				commandStride += sizeof(IndirectDispatchArguments);
				break;
			default:
				ASSERT(false);
				break;
			}
		}

		commandStride = round_up(commandStride, 16);

		pCommandSignature->mDrawCommandStride = commandStride;
		pCommandSignature->mIndirectArgDescCounts = pDesc->mIndirectArgCount;

		D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
		commandSignatureDesc.pArgumentDescs = argumentDescs;
		commandSignatureDesc.NumArgumentDescs = pDesc->mIndirectArgCount;
		commandSignatureDesc.ByteStride = commandStride;

		HRESULT hres = pRenderer->pDevice->CreateCommandSignature(&commandSignatureDesc, change ? pDesc->pRootSignature->pDxRootSignature : NULL, IID_ARGS(&pCommandSignature->pDxCommandSignautre));
		ASSERT(SUCCEEDED(hres));

		*ppCommandSignature = pCommandSignature;
	}

	void removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
	{
    UNREF_PARAM(pRenderer);
		SAFE_RELEASE(pCommandSignature->pDxCommandSignautre);
		SAFE_FREE(pCommandSignature);
	}

	void cmdExecuteIndirect(Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset)
	{
		ASSERT(pCommandSignature);
		ASSERT(pIndirectBuffer);

#ifdef _DURANGO
		BufferBarrier bufferBarriers[] = {
			{ pIndirectBuffer, RESOURCE_STATE_INDIRECT_ARGUMENT }
		};
		cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, false);
#endif
		if (!pCounterBuffer)
			pCmd->pDxCmdList->ExecuteIndirect(pCommandSignature->pDxCommandSignautre, maxCommandCount, pIndirectBuffer->pDxResource, bufferOffset, nullptr, 0);
		else
			pCmd->pDxCmdList->ExecuteIndirect(pCommandSignature->pDxCommandSignautre, maxCommandCount, pIndirectBuffer->pDxResource, bufferOffset, pCounterBuffer->pDxResource, counterBufferOffset);
	}
	/************************************************************************/
	// Debug Marker Implementation
	/************************************************************************/
	void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
	{
#if defined(USE_PIX)
		//color is in B8G8R8X8 format where X is padding
		uint64_t color = packColorF32(r, g, b, 0/*there is no alpha, that's padding*/);
		PIXBeginEvent(pCmd->pDxCmdList, color, pName);
#endif
	}
	void cmdBeginDebugMarkerf(Cmd* pCmd, float r, float g, float b, const char* pFormat, ...)
	{
#if defined(USE_PIX)
		va_list argptr;
		va_start(argptr, pFormat);
		char buffer[65536];
		vsnprintf_s(buffer, sizeof(buffer), pFormat, argptr);
		va_end(argptr);
		cmdBeginDebugMarker(pCmd, r, g, b, buffer);
#endif
	}

	void cmdEndDebugMarker(Cmd* pCmd)
	{
#if defined(USE_PIX)
		PIXEndEvent(pCmd->pDxCmdList);
#endif
	}

	void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
	{
#if defined(USE_PIX)
		//color is in B8G8R8X8 format where X is padding
		uint64_t color = packColorF32(r, g, b, 0/*there is no alpha, that's padding*/);
		PIXSetMarker(pCmd->pDxCmdList, color, pName);
#endif
	}
	void cmdAddDebugMarkerf(Cmd* pCmd, float r, float g, float b, const char* pFormat, ...)
	{
#if defined(USE_PIX)
		va_list argptr;
		va_start(argptr, pFormat);
		char buffer[65536];
		vsnprintf_s(buffer, sizeof(buffer), pFormat, argptr);
		va_end(argptr);

		cmdAddDebugMarker(pCmd, r, g, b, buffer);
#endif
	}
	/************************************************************************/
	/************************************************************************/

#endif // RENDERER_IMPLEMENTATION
#endif
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

/* Lightweight D3D11 Fallback layer.
 * This implementation retrofits the new low level interface to D3D11.
 * TODO: explain how GPU resource dependencies are handled...
 */

#ifdef DIRECT3D11
#define RENDERER_IMPLEMENTATION
#define IID_ARGS IID_PPV_ARGS

#include "../../ThirdParty/OpenSource/TinySTL/string.h"
#include "../../ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../../ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../OS/Interfaces/ILogManager.h"
#include "../IRenderer.h"
#include "../../OS/Core/RingBuffer.h"
#include "../../ThirdParty/OpenSource/TinySTL/hash.h"
#include "../../ThirdParty/OpenSource/winpixeventruntime/Include/WinPixEventRuntime/pix3.h"
#include "../../OS/Core/GPUConfig.h"
#include "Direct3D11Commands.h"

#if !defined(_WIN32)
#error "Windows is needed!"
#endif

#if !defined(__cplusplus)
#error "D3D11 requires C++! Sorry!"
#endif

// Pull in minimal Windows headers
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

// Prefer Higher Performance GPU on switchable GPU systems
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 1;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

//#include "Direct3D11MemoryAllocator.h"
#include "../../OS/Interfaces/IMemoryManager.h"

// clang-format off
extern void d3d11_createShaderReflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection);

D3D11_BLEND_OP gBlendOpTranslator[BlendMode::MAX_BLEND_MODES] =
{
	D3D11_BLEND_OP_ADD,
	D3D11_BLEND_OP_SUBTRACT,
	D3D11_BLEND_OP_REV_SUBTRACT,
	D3D11_BLEND_OP_MIN,
	D3D11_BLEND_OP_MAX,
};

D3D11_BLEND gBlendConstantTranslator[BlendConstant::MAX_BLEND_CONSTANTS] =
{
	D3D11_BLEND_ZERO,
	D3D11_BLEND_ONE,
	D3D11_BLEND_SRC_COLOR,
	D3D11_BLEND_INV_SRC_COLOR,
	D3D11_BLEND_DEST_COLOR,
	D3D11_BLEND_INV_DEST_COLOR,
	D3D11_BLEND_SRC_ALPHA,
	D3D11_BLEND_INV_SRC_ALPHA,
	D3D11_BLEND_DEST_ALPHA,
	D3D11_BLEND_INV_DEST_ALPHA,
	D3D11_BLEND_SRC_ALPHA_SAT,
	D3D11_BLEND_BLEND_FACTOR,
	D3D11_BLEND_INV_BLEND_FACTOR,
};

D3D11_COMPARISON_FUNC gComparisonFuncTranslator[CompareMode::MAX_COMPARE_MODES] =
{
	D3D11_COMPARISON_NEVER,
	D3D11_COMPARISON_LESS,
	D3D11_COMPARISON_EQUAL,
	D3D11_COMPARISON_LESS_EQUAL,
	D3D11_COMPARISON_GREATER,
	D3D11_COMPARISON_NOT_EQUAL,
	D3D11_COMPARISON_GREATER_EQUAL,
	D3D11_COMPARISON_ALWAYS,
};

D3D11_STENCIL_OP gStencilOpTranslator[StencilOp::MAX_STENCIL_OPS] =
{
	D3D11_STENCIL_OP_KEEP,
	D3D11_STENCIL_OP_ZERO,
	D3D11_STENCIL_OP_REPLACE,
	D3D11_STENCIL_OP_INVERT,
	D3D11_STENCIL_OP_INCR,
	D3D11_STENCIL_OP_DECR,
	D3D11_STENCIL_OP_INCR_SAT,
	D3D11_STENCIL_OP_DECR_SAT,
};

D3D11_CULL_MODE gCullModeTranslator[MAX_CULL_MODES] =
{
	D3D11_CULL_NONE,
	D3D11_CULL_BACK,
	D3D11_CULL_FRONT,
};

D3D11_FILL_MODE gFillModeTranslator[MAX_FILL_MODES] =
{
	D3D11_FILL_SOLID,
	D3D11_FILL_WIREFRAME,
};

const DXGI_FORMAT gFormatTranslatorTypeless[] = {
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

const DXGI_FORMAT gFormatTranslator[] = {
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
// clang-format on

// DX Commands cache which gets processed on queue submission.
// Actual implementation of the processing is done in Direct3D11Commands.cpp.
typedef tinystl::unordered_map<Cmd*, tinystl::vector<CachedCmd> > CachedCmds;
tinystl::unordered_map<Cmd*, tinystl::vector<CachedCmd> >         gCachedCmds;

// =================================================================================================
// IMPLEMENTATION
// =================================================================================================

#if defined(RENDERER_IMPLEMENTATION)

#include <d3dcompiler.h>
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

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
//uint64_t					  util_dx_determine_storage_counter_offset(uint64_t buffer_size);
DXGI_FORMAT util_to_dx_image_format_typeless(ImageFormat::Enum format);
DXGI_FORMAT util_to_dx_uav_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT util_to_dx_dsv_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT util_to_dx_srv_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT util_to_dx_stencil_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT util_to_dx_image_format(ImageFormat::Enum format, bool srgb);
//DXGI_FORMAT					   util_to_dx_swapchain_format(ImageFormat::Enum format);
//D3D12_SHADER_VISIBILITY		   util_to_dx_shader_visibility(ShaderStage stages);
//D3D12_DESCRIPTOR_RANGE_TYPE	   util_to_dx_descriptor_range(DescriptorType type);
//D3D12_RESOURCE_STATES		 util_to_dx_resource_state(ResourceState state);
D3D11_FILTER util_to_dx_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool aniso, bool comparisonFilterEnabled);
D3D11_TEXTURE_ADDRESS_MODE util_to_dx_texture_address_mode(AddressMode addressMode);
//D3D12_PRIMITIVE_TOPOLOGY_TYPE util_to_dx_primitive_topology_type(PrimitiveTopology topology);

D3D11_FILTER util_to_dx_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool aniso, bool comparisonFilterEnabled)
{
	if (aniso)
		return comparisonFilterEnabled ? D3D11_FILTER_COMPARISON_ANISOTROPIC : D3D11_FILTER_ANISOTROPIC;

	// control bit : minFilter  magFilter   mipMapMode
	//   point   :   00	  00	   00
	//   linear  :   01	  01	   01
	// ex : trilinear == 010101
	int filter = (minFilter << 4) | (magFilter << 2) | mipMapMode;
	int baseFilter = comparisonFilterEnabled ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_POINT;
	return (D3D11_FILTER)(baseFilter + filter);
}

D3D11_TEXTURE_ADDRESS_MODE util_to_dx_texture_address_mode(AddressMode addressMode)
{
	switch (addressMode)
	{
		case ADDRESS_MODE_MIRROR: return D3D11_TEXTURE_ADDRESS_MIRROR;
		case ADDRESS_MODE_REPEAT: return D3D11_TEXTURE_ADDRESS_WRAP;
		case ADDRESS_MODE_CLAMP_TO_EDGE: return D3D11_TEXTURE_ADDRESS_CLAMP;
		case ADDRESS_MODE_CLAMP_TO_BORDER: return D3D11_TEXTURE_ADDRESS_BORDER;
		default: return D3D11_TEXTURE_ADDRESS_WRAP;
	}
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
	if (format >= sizeof(gFormatTranslatorTypeless) / sizeof(DXGI_FORMAT))
	{
		LOGERRORF("Failed to Map from ConfettilFileFromat to DXGI format, should add map method in gDX12FormatTranslator");
	}
	else
	{
		result = gFormatTranslatorTypeless[format];
	}

	return result;
}

DXGI_FORMAT util_to_dx_image_format(ImageFormat::Enum format, bool srgb)
{
	DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;
	if (format >= sizeof(gFormatTranslator) / sizeof(DXGI_FORMAT))
	{
		LOGERRORF("Failed to Map from ConfettilFileFromat to DXGI format, should add map method in gDX12FormatTranslator");
	}
	else
	{
		result = gFormatTranslator[format];
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

typedef enum GpuVendor
{
	GPU_VENDOR_NVIDIA,
	GPU_VENDOR_AMD,
	GPU_VENDOR_INTEL,
	GPU_VENDOR_UNKNOWN,
	GPU_VENDOR_COUNT,
} GpuVendor;

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

static void add_rtv(
	Renderer* pRenderer, ID3D11Resource* pResource, DXGI_FORMAT format, uint32_t mipSlice, uint32_t arraySlice,
	ID3D11RenderTargetView** pHandle)
{
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	D3D11_RESOURCE_DIMENSION      type;
	pResource->GetType(&type);

	rtvDesc.Format = format;

	switch (type)
	{
		case D3D11_RESOURCE_DIMENSION_BUFFER: break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		{
			D3D11_TEXTURE1D_DESC desc = {};
			((ID3D11Texture1D*)pResource)->GetDesc(&desc);

			if (desc.ArraySize > 1)
			{
				rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
				rtvDesc.Texture1DArray.MipSlice = mipSlice;
				if (arraySlice != -1)
				{
					rtvDesc.Texture1DArray.ArraySize = 1;
					rtvDesc.Texture1DArray.FirstArraySlice = arraySlice;
				}
				else
				{
					rtvDesc.Texture1DArray.ArraySize = desc.ArraySize;
				}
			}
			else
			{
				rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
				rtvDesc.Texture1D.MipSlice = mipSlice;
			}
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
		{
			D3D11_TEXTURE2D_DESC desc = {};
			((ID3D11Texture2D*)pResource)->GetDesc(&desc);

			if (desc.SampleDesc.Count > 1)
			{
				if (desc.ArraySize > 1)
				{
					rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
					if (arraySlice != -1)
					{
						rtvDesc.Texture2DMSArray.ArraySize = 1;
						rtvDesc.Texture2DMSArray.FirstArraySlice = arraySlice;
					}
					else
					{
						rtvDesc.Texture2DMSArray.ArraySize = desc.ArraySize;
					}
				}
				else
				{
					rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
				}
			}
			else
			{
				if (desc.ArraySize > 1)
				{
					rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
					rtvDesc.Texture2DArray.MipSlice = mipSlice;
					if (arraySlice != -1)
					{
						rtvDesc.Texture2DArray.ArraySize = 1;
						rtvDesc.Texture2DArray.FirstArraySlice = arraySlice;
					}
					else
					{
						rtvDesc.Texture2DArray.ArraySize = desc.ArraySize;
					}
				}
				else
				{
					rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
					rtvDesc.Texture2D.MipSlice = mipSlice;
				}
			}
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
		{
			D3D11_TEXTURE3D_DESC desc = {};
			((ID3D11Texture3D*)pResource)->GetDesc(&desc);

			rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
			rtvDesc.Texture3D.MipSlice = mipSlice;
			if (arraySlice != -1)
			{
				rtvDesc.Texture3D.WSize = 1;
				rtvDesc.Texture3D.FirstWSlice = arraySlice;
			}
			else
			{
				rtvDesc.Texture3D.WSize = desc.Depth;
			}
			break;
		}
		default: break;
	}

	pRenderer->pDxDevice->CreateRenderTargetView(pResource, &rtvDesc, pHandle);
}

static void add_dsv(
	Renderer* pRenderer, ID3D11Resource* pResource, DXGI_FORMAT format, uint32_t mipSlice, uint32_t arraySlice,
	ID3D11DepthStencilView** pHandle)
{
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	D3D11_RESOURCE_DIMENSION      type;
	pResource->GetType(&type);

	dsvDesc.Format = format;

	switch (type)
	{
		case D3D11_RESOURCE_DIMENSION_BUFFER: break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		{
			D3D11_TEXTURE1D_DESC desc = {};
			((ID3D11Texture1D*)pResource)->GetDesc(&desc);

			if (desc.ArraySize > 1)
			{
				dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
				dsvDesc.Texture1DArray.MipSlice = mipSlice;
				if (arraySlice != -1)
				{
					dsvDesc.Texture1DArray.ArraySize = 1;
					dsvDesc.Texture1DArray.FirstArraySlice = arraySlice;
				}
				else
				{
					dsvDesc.Texture1DArray.ArraySize = desc.ArraySize;
				}
			}
			else
			{
				dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
				dsvDesc.Texture1D.MipSlice = mipSlice;
			}
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
		{
			D3D11_TEXTURE2D_DESC desc = {};
			((ID3D11Texture2D*)pResource)->GetDesc(&desc);

			if (desc.SampleDesc.Count > 1)
			{
				if (desc.ArraySize > 1)
				{
					dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
					if (arraySlice != -1)
					{
						dsvDesc.Texture2DMSArray.ArraySize = 1;
						dsvDesc.Texture2DMSArray.FirstArraySlice = arraySlice;
					}
					else
					{
						dsvDesc.Texture2DMSArray.ArraySize = desc.ArraySize;
					}
				}
				else
				{
					dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
				}
			}
			else
			{
				if (desc.ArraySize > 1)
				{
					dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
					dsvDesc.Texture2DArray.MipSlice = mipSlice;
					if (arraySlice != -1)
					{
						dsvDesc.Texture2DArray.ArraySize = 1;
						dsvDesc.Texture2DArray.FirstArraySlice = arraySlice;
					}
					else
					{
						dsvDesc.Texture2DArray.ArraySize = desc.ArraySize;
					}
				}
				else
				{
					dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
					dsvDesc.Texture2D.MipSlice = mipSlice;
				}
			}
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D: ASSERT(false && "Cannot create 3D Depth Stencil"); break;
		default: break;
	}

	pRenderer->pDxDevice->CreateDepthStencilView(pResource, &dsvDesc, pHandle);
}
/************************************************************************/
// Functions not exposed in IRenderer but still need to be exported in dll
/************************************************************************/
// clang-format off
API_INTERFACE void CALLTYPE addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
API_INTERFACE void CALLTYPE removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
API_INTERFACE void CALLTYPE addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture);
API_INTERFACE void CALLTYPE removeTexture(Renderer* pRenderer, Texture* pTexture);
API_INTERFACE void CALLTYPE mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
API_INTERFACE void CALLTYPE unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
API_INTERFACE void CALLTYPE cmdUpdateBuffer(Cmd* pCmd, uint64_t srcOffset, uint64_t dstOffset, uint64_t size, Buffer* pSrcBuffer, Buffer* pBuffer);
API_INTERFACE void CALLTYPE cmdUpdateSubresources(Cmd* pCmd, uint32_t startSubresource, uint32_t numSubresources, SubresourceDataDesc* pSubresources, Buffer* pIntermediate, uint64_t intermediateOffset, Texture* pTexture);
API_INTERFACE void CALLTYPE compileShader(Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code, uint32_t macroCount, ShaderMacro* pMacros, void* (*allocator)(size_t a), uint32_t* pByteCodeSize, char** ppByteCode);
API_INTERFACE const RendererShaderDefinesDesc CALLTYPE get_renderer_shaderdefines(Renderer* pRenderer);

// clang-format on
void cmdUpdateBuffer(Cmd* pCmd, uint64_t srcOffset, uint64_t dstOffset, uint64_t size, Buffer* pSrcBuffer, Buffer* pBuffer)
{
	ASSERT(pCmd);
	ASSERT(pSrcBuffer);
	//ASSERT(pSrcBuffer->pDxResource);
	ASSERT(pBuffer);
	//ASSERT(pBuffer->pDxResource);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdUpdateBuffer;
	cmd.mUpdateBufferCmd.srcOffset = srcOffset;
	cmd.mUpdateBufferCmd.dstOffset = dstOffset;
	cmd.mUpdateBufferCmd.size = size;
	cmd.mUpdateBufferCmd.pSrcBuffer = pSrcBuffer;
	cmd.mUpdateBufferCmd.pBuffer = pBuffer;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdUpdateSubresources(
	Cmd* pCmd, uint32_t startSubresource, uint32_t numSubresources, SubresourceDataDesc* pSubresources, Buffer* pIntermediate,
	uint64_t intermediateOffset, Texture* pTexture)
{
	ASSERT(pCmd);
	ASSERT(pSubresources);
	//ASSERT(pSrcBuffer->pDxResource);
	ASSERT(pTexture);
	//ASSERT(pBuffer->pDxResource);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdUpdateSubresources;
	cmd.mUpdateSubresourcesCmd.startSubresource = startSubresource;
	cmd.mUpdateSubresourcesCmd.numSubresources = numSubresources;
	cmd.mUpdateSubresourcesCmd.pSubresources = (SubresourceDataDesc*)conf_calloc(numSubresources, sizeof(SubresourceDataDesc));
	memcpy(cmd.mUpdateSubresourcesCmd.pSubresources, pSubresources, numSubresources * sizeof(SubresourceDataDesc));
	for (uint32_t i = 0; i < numSubresources; ++i)
	{
		cmd.mUpdateSubresourcesCmd.pSubresources[i].pData =
			conf_calloc(cmd.mUpdateSubresourcesCmd.pSubresources[i].mSlicePitch, sizeof(uint8_t));
		memcpy(cmd.mUpdateSubresourcesCmd.pSubresources[i].pData, pSubresources[i].pData, pSubresources[i].mSlicePitch);
	}
	cmd.mUpdateSubresourcesCmd.pIntermediate = pIntermediate;
	cmd.mUpdateSubresourcesCmd.intermediateOffset = intermediateOffset;
	cmd.mUpdateSubresourcesCmd.pTexture = pTexture;
	cachedCmdsIter->second.push_back(cmd);
}

const RendererShaderDefinesDesc get_renderer_shaderdefines(Renderer* pRenderer) { return RendererShaderDefinesDesc(); }
/************************************************************************/
// Internal init functions
/************************************************************************/
static void AddDevice(Renderer* pRenderer)
{
	const uint32_t    NUM_SUPPORTED_FEATURE_LEVELS = 2;
	D3D_FEATURE_LEVEL feature_levels[NUM_SUPPORTED_FEATURE_LEVELS] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	HRESULT hr = 0;

	IDXGIAdapter1* dxgiAdapter = NULL;

	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pRenderer->pDXGIFactory)))
	{
		LOGERROR("Could not create DXGI factory.");
		return;
	}
	ASSERT(pRenderer->pDXGIFactory);

	// Enumerate all adapters
	typedef struct GpuDesc
	{
		IDXGIAdapter1*                    pGpu = NULL;
		D3D_FEATURE_LEVEL                 mMaxSupportedFeatureLevel = (D3D_FEATURE_LEVEL)0;
		D3D11_FEATURE_DATA_D3D11_OPTIONS2 mFeatureDataOptions2 = {};
		SIZE_T                            mDedicatedVideoMemory = 0;
		char                              mVendorId[MAX_GPU_VENDOR_STRING_LENGTH];
		char                              mDeviceId[MAX_GPU_VENDOR_STRING_LENGTH];
		char                              mRevisionId[MAX_GPU_VENDOR_STRING_LENGTH];
		char                              mName[MAX_GPU_VENDOR_STRING_LENGTH];
		GPUPresetLevel                    mPreset;
	} GpuDesc;

	GpuDesc gpuDesc[MAX_GPUS] = {};

	IDXGIAdapter1* adapter = NULL;
	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != pRenderer->pDXGIFactory->EnumAdapters1(i, (IDXGIAdapter1**)&adapter); ++i)
	{
		DECLARE_ZERO(DXGI_ADAPTER_DESC1, desc);
		adapter->GetDesc1(&desc);

		if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
		{
			// Make sure the adapter can support a D3D11 device
			D3D_FEATURE_LEVEL featLevelOut;
			if (SUCCEEDED(D3D11CreateDevice(
					adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, feature_levels, 2, D3D11_SDK_VERSION, NULL, &featLevelOut, NULL)))
			{
				hr = adapter->QueryInterface(IID_ARGS(&gpuDesc[pRenderer->mNumOfGPUs].pGpu));
				if (SUCCEEDED(hr))
				{
					D3D11CreateDevice(
						adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, feature_levels, 2, D3D11_SDK_VERSION, &pRenderer->pDxDevice,
						&featLevelOut, NULL);

					D3D11_FEATURE_DATA_D3D11_OPTIONS2 featureData2 = {};
					pRenderer->pDxDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2, &featureData2, sizeof(featureData2));

					gpuDesc[pRenderer->mNumOfGPUs].mMaxSupportedFeatureLevel = featLevelOut;
					gpuDesc[pRenderer->mNumOfGPUs].mDedicatedVideoMemory = desc.DedicatedVideoMemory;
					gpuDesc[pRenderer->mNumOfGPUs].mFeatureDataOptions2 = featureData2;

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

		adapter->Release();
	}
	ASSERT(pRenderer->mNumOfGPUs > 0);

	// Sort GPUs by poth Preset and highest feature level gpu at front
	//Prioritize Preset first
	qsort(gpuDesc, pRenderer->mNumOfGPUs, sizeof(GpuDesc), [](const void* lhs, const void* rhs) {
		GpuDesc* gpu1 = (GpuDesc*)lhs;
		GpuDesc* gpu2 = (GpuDesc*)rhs;
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

	for (uint32_t i = 0; i < pRenderer->mNumOfGPUs; ++i)
	{
		pRenderer->pDxGPUs[i] = gpuDesc[i].pGpu;
		pRenderer->mGpuSettings[i].mUniformBufferAlignment = 256;
		pRenderer->mGpuSettings[i].mMultiDrawIndirect = false;    // no such thing
		pRenderer->mGpuSettings[i].mMaxVertexInputBindings = 32U;

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

		// Determine root signature size for this gpu driver
		pRenderer->mGpuSettings[i].mMaxRootSignatureDWORDS = 0U;    // no such thing
		pRenderer->mGpuSettings[i].mROVsSupported = gpuDesc[i].mFeatureDataOptions2.ROVsSupported ? true : false;
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

	// Create the actual device
	DWORD deviceFlags = 0;

#ifdef _DEBUG
	deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL featLevelOut;
	hr = D3D11CreateDevice(
		pRenderer->pDxActiveGPU, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, deviceFlags, feature_levels, 2, D3D11_SDK_VERSION,
		&pRenderer->pDxDevice,
		&featLevelOut,    // max feature level
		&pRenderer->pDxContext);
	ASSERT(SUCCEEDED(hr));
	if (FAILED(hr))
		LOGERROR("Failed to create D3D11 device and context.");
}

static void RemoveDevice(Renderer* pRenderer)
{
	SAFE_RELEASE(pRenderer->pDXGIFactory);

	for (uint32_t i = 0; i < pRenderer->mNumOfGPUs; ++i)
	{
		SAFE_RELEASE(pRenderer->pDxGPUs[i]);
	}

	SAFE_RELEASE(pRenderer->pDxContext);
#ifdef _DEBUG
	ID3D11Debug* pDebugDevice = NULL;
	pRenderer->pDxDevice->QueryInterface(&pDebugDevice);
	SAFE_RELEASE(pRenderer->pDxDevice);

	pDebugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
	pDebugDevice->Release();
#else
	SAFE_RELEASE(pRenderer->pDxDevice);
#endif
}

static void create_default_resources(Renderer* pRenderer)
{
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
	removeBlendState(pRenderer->pDefaultBlendState);
	removeDepthState(pRenderer->pDefaultDepthState);
	removeRasterizerState(pRenderer->pDefaultRasterizerState);
}
/************************************************************************/
// Renderer Init Remove
/************************************************************************/
void initRenderer(const char* appName, const RendererDesc* settings, Renderer** ppRenderer)
{
	Renderer* pRenderer = (Renderer*)conf_calloc(1, sizeof(*pRenderer));
	ASSERT(pRenderer);

	pRenderer->pName = (char*)conf_calloc(strlen(appName) + 1, sizeof(char));
	memcpy(pRenderer->pName, appName, strlen(appName));

	// Copy settings
	memcpy(&(pRenderer->mSettings), settings, sizeof(*settings));
	pRenderer->mSettings.mApi = RENDERER_API_D3D11;
	pRenderer->mSettings.mShaderTarget = shader_target_5_0;

	// Initialize the D3D12 bits
	{
		AddDevice(pRenderer);

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
	}

	create_default_resources(pRenderer);

	// Renderer is good! Assign it to result!
	*(ppRenderer) = pRenderer;
}

void removeRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);

	SAFE_FREE(pRenderer->pName);

	destroy_default_resources(pRenderer);

	RemoveDevice(pRenderer);

	// Free all the renderer components
	SAFE_FREE(pRenderer);
}

/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void addFence(Renderer* pRenderer, Fence** ppFence)
{
	// NOTE: We will still use it to be able to generate
	// a dependency graph to serialize parallel GPU workload.

	//ASSERT that renderer is valid
	ASSERT(pRenderer);

	//create a Fence and ASSERT that it is valid
	Fence* pFence = (Fence*)conf_calloc(1, sizeof(*pFence));
	ASSERT(pFence);

	//set given pointer to new fence
	*ppFence = pFence;
}

void removeFence(Renderer* pRenderer, Fence* pFence)
{
	//ASSERT that renderer is valid
	ASSERT(pRenderer);
	//ASSERT that given fence to remove is valid
	ASSERT(pFence);

	//delete memory
	SAFE_FREE(pFence);
}

void addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
	// NOTE: We will still use it to be able to generate
	// a dependency graph to serialize parallel GPU workload.

	//ASSERT that renderer is valid
	ASSERT(pRenderer);

	//create a semaphore and ASSERT that it is valid
	Semaphore* pSemaphore = (Semaphore*)conf_calloc(1, sizeof(*pSemaphore));
	ASSERT(pSemaphore);

	//save newly created semaphore in given pointer
	*ppSemaphore = pSemaphore;
}

void removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
	//ASSERT that renderer and given semaphore are valid
	ASSERT(pRenderer);
	ASSERT(pSemaphore);

	//safe delete that check for valid pointer
	SAFE_FREE(pSemaphore);
}

void addQueue(Renderer* pRenderer, QueueDesc* pQDesc, Queue** ppQueue)
{
	// DX11 doesn't use queues -- so just create a dummy object for the client
	// NOTE: We will still use it to reference the renderer in the queue and to be able to generate
	// a dependency graph to serialize parallel GPU workload.
	Queue* pQueue = (Queue*)conf_calloc(1, sizeof(*pQueue));
	ASSERT(pQueue != NULL);

	// Provided description for queue creation.
	// Note these don't really mean much w/ DX11 but we can use it for debugging
	// what the client is intending to do.
	pQueue->mQueueDesc = *pQDesc;

	tinystl::string queueType = "DUMMY QUEUE FOR DX11 BACKEND";
	pQueue->pRenderer = pRenderer;

	*ppQueue = pQueue;
}

void removeQueue(Queue* pQueue)
{
	ASSERT(pQueue != NULL);
	SAFE_FREE(pQueue);
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

	HWND hwnd = (HWND)pSwapChain->mDesc.pWindow->handle;

	DXGI_SWAP_CHAIN_DESC desc = {};
	desc.BufferDesc.Width = pSwapChain->mDesc.mWidth;
	desc.BufferDesc.Height = pSwapChain->mDesc.mHeight;
	desc.BufferDesc.Format = util_to_dx_swapchain_format(pSwapChain->mDesc.mColorFormat);
	desc.SampleDesc.Count = 1;    // If multisampling is needed, we'll resolve it later
	desc.SampleDesc.Quality = pSwapChain->mDesc.mSampleQuality;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
	desc.BufferCount = 1;    // pSwapChain->mDesc.mImageCount;
	desc.OutputWindow = hwnd;
	desc.Windowed = (BOOL)(!pDesc->pWindow->fullScreen);
	desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	desc.Flags = 0;

	IDXGISwapChain* swapchain;

#ifdef _DURANGO
	HRESULT hres = create_swap_chain(pRenderer, pSwapChain, &desc, &swapchain);
	ASSERT(SUCCEEDED(hres));
#else
	HRESULT hres = pRenderer->pDXGIFactory->CreateSwapChain(pRenderer->pDxDevice, &desc, &swapchain);
	ASSERT(SUCCEEDED(hres));

	hres = pRenderer->pDXGIFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	ASSERT(SUCCEEDED(hres));
#endif

	hres = swapchain->QueryInterface(__uuidof(pSwapChain->pDxSwapChain), (void**)&(pSwapChain->pDxSwapChain));
	ASSERT(SUCCEEDED(hres));
	swapchain->Release();

	// Create rendertargets from swapchain
	pSwapChain->ppDxSwapChainResources =
		(ID3D11Resource**)conf_calloc(pSwapChain->mDesc.mImageCount, sizeof(*pSwapChain->ppDxSwapChainResources));
	ASSERT(pSwapChain->ppDxSwapChainResources);
	for (uint32_t i = 0; i < pSwapChain->mDesc.mImageCount; ++i)
	{
		hres = pSwapChain->pDxSwapChain->GetBuffer(0, IID_ARGS(&pSwapChain->ppDxSwapChainResources[i]));
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
	for (uint32_t i = 0; i < pSwapChain->mDesc.mImageCount; ++i)
	{
		::removeRenderTarget(pRenderer, pSwapChain->ppSwapchainRenderTargets[i]);
		SAFE_RELEASE(pSwapChain->ppDxSwapChainResources[i]);
	}

	SAFE_RELEASE(pSwapChain->pDxSwapChain);
	SAFE_FREE(pSwapChain->ppSwapchainRenderTargets);
	SAFE_FREE(pSwapChain->ppDxSwapChainResources);
	SAFE_FREE(pSwapChain);
}
/************************************************************************/
// Command Pool Functions
/************************************************************************/
void addCmdPool(Renderer* pRenderer, Queue* pQueue, bool transient, CmdPool** ppCmdPool)
{
	// NOTE: We will still use cmd pools to be able to generate
	// a dependency graph to serialize parallel GPU workload.

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

	if (pCmd->pBoundColorFormats)
		SAFE_FREE(pCmd->pBoundColorFormats);

	if (pCmd->pBoundSrgbValues)
		SAFE_FREE(pCmd->pBoundSrgbValues);

	if (pCmd->pRootConstantBuffer)
		removeBuffer(pCmd->pRenderer, pCmd->pRootConstantBuffer);

	if (pCmd->pTransientConstantBuffer)
		removeBuffer(pCmd->pRenderer, pCmd->pTransientConstantBuffer);

	SAFE_FREE(pCmd->pDescriptorStructPool);
	SAFE_FREE(pCmd->pDescriptorNamePool);
	SAFE_FREE(pCmd->pDescriptorResourcesPool);

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

/************************************************************************/
// All buffer, texture loading handled by resource system -> IResourceLoader.
/************************************************************************/
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

	uint32_t numRTVs = pDesc->mMipLevels;
	if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		numRTVs *= (pDesc->mDepth * pDesc->mArraySize);

	if (isDepth)
	{
		pRenderTarget->pDxDsvDescriptors = (ID3D11DepthStencilView**)conf_calloc(numRTVs + 1, sizeof(ID3D11DepthStencilView*));
	}
	else
	{
		pRenderTarget->pDxRtvDescriptors = (ID3D11RenderTargetView**)conf_calloc(numRTVs + 1, sizeof(ID3D11RenderTargetView*));
	}

	!isDepth ? add_rtv(pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, 0, -1, &pRenderTarget->pDxRtvDescriptors[0])
			 : add_dsv(pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, 0, -1, &pRenderTarget->pDxDsvDescriptors[0]);

	for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
	{
		const uint32_t depthOrArraySize = pDesc->mDepth * pDesc->mArraySize;
		if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
			(pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		{
			for (uint32_t j = 0; j < depthOrArraySize; ++j)
			{
				!ImageFormat::IsDepthFormat(pRenderTarget->mDesc.mFormat)
					? add_rtv(
						  pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, i, j,
						  &pRenderTarget->pDxRtvDescriptors[1 + i * depthOrArraySize + j])
					: add_dsv(
						  pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, i, j,
						  &pRenderTarget->pDxDsvDescriptors[1 + i * depthOrArraySize + j]);
			}
		}
		else
		{
			!ImageFormat::IsDepthFormat(pRenderTarget->mDesc.mFormat)
				? add_rtv(pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, i, -1, &pRenderTarget->pDxRtvDescriptors[1 + i])
				: add_dsv(pRenderer, pRenderTarget->pTexture->pDxResource, dxFormat, i, -1, &pRenderTarget->pDxDsvDescriptors[1 + i]);
		}
	}

	*ppRenderTarget = pRenderTarget;
}

void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
	removeTexture(pRenderer, pRenderTarget->pTexture);

	if (!ImageFormat::IsDepthFormat(pRenderTarget->mDesc.mFormat))
	{
		SAFE_RELEASE(pRenderTarget->pDxRtvDescriptors[0]);
	}
	else
	{
		SAFE_RELEASE(pRenderTarget->pDxDsvDescriptors[0]);
	}

	const uint32_t depthOrArraySize = pRenderTarget->mDesc.mArraySize * pRenderTarget->mDesc.mDepth;
	if ((pRenderTarget->mDesc.mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pRenderTarget->mDesc.mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
	{
		for (uint32_t i = 0; i < pRenderTarget->mDesc.mMipLevels; ++i)
			for (uint32_t j = 0; j < depthOrArraySize; ++j)
				if (!ImageFormat::IsDepthFormat(pRenderTarget->mDesc.mFormat))
				{
					SAFE_RELEASE(pRenderTarget->pDxRtvDescriptors[1 + i * depthOrArraySize + j]);
				}
				else
				{
					SAFE_RELEASE(pRenderTarget->pDxDsvDescriptors[1 + i * depthOrArraySize + j]);
				}
	}
	else
	{
		for (uint32_t i = 0; i < pRenderTarget->mDesc.mMipLevels; ++i)
			if (!ImageFormat::IsDepthFormat(pRenderTarget->mDesc.mFormat))
			{
				SAFE_RELEASE(pRenderTarget->pDxRtvDescriptors[1 + i]);
			}
			else
			{
				SAFE_RELEASE(pRenderTarget->pDxDsvDescriptors[1 + i]);
			}
	}

	SAFE_FREE(pRenderTarget->pDxRtvDescriptors);
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
	D3D11_SAMPLER_DESC desc;
	desc.Filter = util_to_dx_filter(
		pDesc->mMinFilter, pDesc->mMagFilter, pDesc->mMipMapMode, pDesc->mMaxAnisotropy > 0.0f,
		(pDesc->mCompareFunc != CMP_NEVER ? true : false));
	desc.AddressU = util_to_dx_texture_address_mode(pDesc->mAddressU);
	desc.AddressV = util_to_dx_texture_address_mode(pDesc->mAddressV);
	desc.AddressW = util_to_dx_texture_address_mode(pDesc->mAddressW);
	desc.MipLODBias = pDesc->mMipLosBias;
	desc.MaxAnisotropy = max((UINT)pDesc->mMaxAnisotropy, 1U);
	desc.ComparisonFunc = gComparisonFuncTranslator[pDesc->mCompareFunc];
	desc.BorderColor[0] = 0.0f;
	desc.BorderColor[1] = 0.0f;
	desc.BorderColor[2] = 0.0f;
	desc.BorderColor[3] = 0.0f;
	desc.MinLOD = 0.0f;
	desc.MaxLOD = ((pDesc->mMipMapMode == MIPMAP_MODE_LINEAR) ? D3D11_FLOAT32_MAX : 0.0f);

	if (FAILED(pRenderer->pDxDevice->CreateSamplerState(&desc, &pSampler->pSamplerState)))
		LOGERROR("Failed to create sampler state.");

	pSampler->mSamplerId = (++gSamplerIds << 8U) + Thread::GetCurrentThreadID();

	*ppSampler = pSampler;
}

void removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
	ASSERT(pRenderer);
	ASSERT(pSampler);

	SAFE_RELEASE(pSampler->pSamplerState);

	SAFE_FREE(pSampler);
}

/************************************************************************/
// Shader Functions
/************************************************************************/
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
		case shader_target_5_0:
		{
			major = 5;
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

	//if (fnHookShaderCompileFlags != NULL)
	//  fnHookShaderCompileFlags(compile_flags);

	tinystl::string entryPoint = "main";
	ID3DBlob*       compiled_code = NULL;
	ID3DBlob*       error_msgs = NULL;
	HRESULT         hres = D3DCompile2(
        code, (size_t)codeSize, fileName, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint.c_str(), target, compile_flags, 0, 0, NULL,
        0, &compiled_code, &error_msgs);
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

void addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
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
		ShaderStage                  stage_mask = (ShaderStage)(1 << i);
		const BinaryShaderStageDesc* pStage = NULL;

		if (stage_mask == (pShaderProgram->mStages & stage_mask))
		{
			switch (stage_mask)
			{
				case SHADER_STAGE_VERT:
				{
					pStage = &pDesc->mVert;
					HRESULT hr = pRenderer->pDxDevice->CreateVertexShader(
						pStage->pByteCode, pStage->mByteCodeSize, NULL, &pShaderProgram->pDxVertexShader);
					ASSERT(SUCCEEDED(hr));
					hr = D3DGetInputSignatureBlob(pStage->pByteCode, pStage->mByteCodeSize, &pShaderProgram->pDxInputSignature);
					ASSERT(SUCCEEDED(hr));
				}
				break;
				case SHADER_STAGE_HULL:
				{
					pStage = &pDesc->mHull;
					HRESULT hr = pRenderer->pDxDevice->CreateHullShader(
						pStage->pByteCode, pStage->mByteCodeSize, NULL, &pShaderProgram->pDxHullShader);
					ASSERT(SUCCEEDED(hr));
				}
				break;
				case SHADER_STAGE_DOMN:
				{
					pStage = &pDesc->mDomain;
					HRESULT hr = pRenderer->pDxDevice->CreateDomainShader(
						pStage->pByteCode, pStage->mByteCodeSize, NULL, &pShaderProgram->pDxDomainShader);
					ASSERT(SUCCEEDED(hr));
				}
				break;
				case SHADER_STAGE_GEOM:
				{
					pStage = &pDesc->mGeom;
					HRESULT hr = pRenderer->pDxDevice->CreateGeometryShader(
						pStage->pByteCode, pStage->mByteCodeSize, NULL, &pShaderProgram->pDxGeometryShader);
					ASSERT(SUCCEEDED(hr));
				}
				break;
				case SHADER_STAGE_FRAG:
				{
					pStage = &pDesc->mFrag;
					HRESULT hr = pRenderer->pDxDevice->CreatePixelShader(
						pStage->pByteCode, pStage->mByteCodeSize, NULL, &pShaderProgram->pDxPixelShader);
					ASSERT(SUCCEEDED(hr));
				}
				break;
				case SHADER_STAGE_COMP:
				{
					pStage = &pDesc->mComp;
					HRESULT hr = pRenderer->pDxDevice->CreateComputeShader(
						pStage->pByteCode, pStage->mByteCodeSize, NULL, &pShaderProgram->pDxComputeShader);
					ASSERT(SUCCEEDED(hr));
				}
				break;
			}

			d3d11_createShaderReflection(
				(uint8_t*)(pStage->pByteCode), (uint32_t)pStage->mByteCodeSize, stage_mask,
				&pShaderProgram->mReflection.mStageReflections[reflectionCount]);

			reflectionCount++;
		}
	}

	createPipelineReflection(pShaderProgram->mReflection.mStageReflections, reflectionCount, &pShaderProgram->mReflection);

	*ppShaderProgram = pShaderProgram;
}

void removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
	UNREF_PARAM(pRenderer);

	//remove given shader
	destroyPipelineReflection(&pShaderProgram->mReflection);

	SAFE_RELEASE(pShaderProgram->pDxVertexShader);
	SAFE_RELEASE(pShaderProgram->pDxPixelShader);
	SAFE_RELEASE(pShaderProgram->pDxComputeShader);
	SAFE_RELEASE(pShaderProgram->pDxGeometryShader);
	SAFE_RELEASE(pShaderProgram->pDxDomainShader);
	SAFE_RELEASE(pShaderProgram->pDxHullShader);
	SAFE_RELEASE(pShaderProgram->pDxInputSignature);

	SAFE_FREE(pShaderProgram);
}

D3D11_BIND_FLAG util_determine_dx_bind_flags(DescriptorType type, ResourceState state)
{
	uint32_t ret = {};
	if (type & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		ret |= D3D11_BIND_CONSTANT_BUFFER;
	if (type & DESCRIPTOR_TYPE_VERTEX_BUFFER)
		ret |= D3D11_BIND_VERTEX_BUFFER;
	if (type & DESCRIPTOR_TYPE_INDEX_BUFFER)
		ret |= D3D11_BIND_INDEX_BUFFER;
	if (type & DESCRIPTOR_TYPE_BUFFER)
		ret |= D3D11_BIND_SHADER_RESOURCE;
	if (type & DESCRIPTOR_TYPE_RW_BUFFER)
		ret |= D3D11_BIND_UNORDERED_ACCESS;
	if (type & DESCRIPTOR_TYPE_TEXTURE)
		ret |= D3D11_BIND_SHADER_RESOURCE;
	if (type & DESCRIPTOR_TYPE_RW_TEXTURE)
		ret |= D3D11_BIND_UNORDERED_ACCESS;
	if (state & RESOURCE_STATE_RENDER_TARGET)
		ret |= D3D11_BIND_RENDER_TARGET;
	if (state & RESOURCE_STATE_DEPTH_WRITE)
		ret |= D3D11_BIND_DEPTH_STENCIL;

	return (D3D11_BIND_FLAG)ret;
}

D3D11_CPU_ACCESS_FLAG util_determine_dx_cpu_access_flags(ResourceMemoryUsage mem)
{
	switch (mem)
	{
		case RESOURCE_MEMORY_USAGE_GPU_ONLY: return (D3D11_CPU_ACCESS_FLAG)0;
		case RESOURCE_MEMORY_USAGE_CPU_ONLY: return (D3D11_CPU_ACCESS_FLAG)(D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE);
		case RESOURCE_MEMORY_USAGE_CPU_TO_GPU: return (D3D11_CPU_ACCESS_FLAG)(D3D11_CPU_ACCESS_WRITE);
		case RESOURCE_MEMORY_USAGE_GPU_TO_CPU: return (D3D11_CPU_ACCESS_FLAG)(D3D11_CPU_ACCESS_READ);
		default: ASSERT(false && "Invalid Memory Usage"); return (D3D11_CPU_ACCESS_FLAG)(-1);
	}
}

D3D11_RESOURCE_MISC_FLAG util_determine_dx_resource_misc_flags(DescriptorType type, ImageFormat::Enum format)
{
	uint32_t ret = {};
	if (DESCRIPTOR_TYPE_TEXTURE_CUBE == (type & DESCRIPTOR_TYPE_TEXTURE_CUBE))
		ret |= D3D11_RESOURCE_MISC_TEXTURECUBE;
	if (type & DESCRIPTOR_TYPE_INDIRECT_BUFFER)
		ret |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

	if (DESCRIPTOR_TYPE_BUFFER_RAW == (type & DESCRIPTOR_TYPE_BUFFER_RAW))
		ret |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	else if ((type & DESCRIPTOR_TYPE_BUFFER) && format == ImageFormat::NONE)
		ret |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	if (DESCRIPTOR_TYPE_RW_BUFFER_RAW == (type & DESCRIPTOR_TYPE_RW_BUFFER_RAW))
		ret |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	else if ((type & DESCRIPTOR_TYPE_RW_BUFFER) && format == ImageFormat::NONE)
		ret |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	return (D3D11_RESOURCE_MISC_FLAG)ret;
}

D3D11_USAGE util_to_dx_usage(ResourceMemoryUsage mem)
{
	switch (mem)
	{
		case RESOURCE_MEMORY_USAGE_GPU_ONLY: return D3D11_USAGE_DEFAULT;
		case RESOURCE_MEMORY_USAGE_CPU_ONLY: return D3D11_USAGE_STAGING;
		case RESOURCE_MEMORY_USAGE_CPU_TO_GPU: return D3D11_USAGE_DYNAMIC;
		case RESOURCE_MEMORY_USAGE_GPU_TO_CPU: return D3D11_USAGE_STAGING;
		default: ASSERT(false && "Invalid Memory Usage"); return (D3D11_USAGE)(-1);
	}
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

	D3D11_BUFFER_DESC desc = {};
	desc.BindFlags = util_determine_dx_bind_flags(pDesc->mDescriptors, RESOURCE_STATE_COMMON);
	desc.ByteWidth = (UINT)pBuffer->mDesc.mSize;
	desc.CPUAccessFlags = util_determine_dx_cpu_access_flags(pDesc->mMemoryUsage);
	desc.MiscFlags = util_determine_dx_resource_misc_flags(pDesc->mDescriptors, pDesc->mFormat);
	desc.StructureByteStride = (UINT)((desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) ? 0 : pDesc->mStructStride);
	desc.Usage = util_to_dx_usage(pDesc->mMemoryUsage);

	if (pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU || pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_ONLY)
	{
		pBuffer->mDesc.mStartState = RESOURCE_STATE_GENERIC_READ;
	}

	HRESULT hres = pRenderer->pDxDevice->CreateBuffer(&desc, NULL, &pBuffer->pDxResource);
	ASSERT(SUCCEEDED(hres));

	pBuffer->mPositionInHeap = 0;
	pBuffer->mCurrentState = pBuffer->mDesc.mStartState;

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
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = (UINT)pBuffer->mDesc.mFirstElement;
		srvDesc.Buffer.NumElements = (UINT)(pBuffer->mDesc.mElementCount);
		srvDesc.Buffer.ElementWidth = (UINT)(pBuffer->mDesc.mStructStride);
		srvDesc.Buffer.ElementOffset = 0;
		srvDesc.Format = util_to_dx_image_format(pDesc->mFormat, false);
		if (DESCRIPTOR_TYPE_BUFFER_RAW == (pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER_RAW))
		{
			if (pDesc->mFormat != ImageFormat::NONE)
				LOGWARNING("Raw buffers use R32 typeless format. Format will be ignored");
			srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		}
		// Cannot create a typed StructuredBuffer
		if (srvDesc.Format != DXGI_FORMAT_UNKNOWN)
		{
			srvDesc.Buffer.ElementWidth = 0;
		}

		pRenderer->pDxDevice->CreateShaderResourceView(pBuffer->pDxResource, &srvDesc, &pBuffer->pDxSrvHandle);
	}

	if ((pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER) &&
		!(pBuffer->mDesc.mFlags & BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION))
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = (UINT)pBuffer->mDesc.mFirstElement;
		uavDesc.Buffer.NumElements = (UINT)(pBuffer->mDesc.mElementCount);
		uavDesc.Buffer.Flags = 0;
		if (DESCRIPTOR_TYPE_RW_BUFFER_RAW == (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER_RAW))
		{
			if (pDesc->mFormat != ImageFormat::NONE)
				LOGWARNING("Raw buffers use R32 typeless format. Format will be ignored");
			uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			uavDesc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_RAW;
		}
		else if (pDesc->mFormat != ImageFormat::NONE)
		{
			uavDesc.Format = util_to_dx_image_format(pDesc->mFormat, false);
		}

		pRenderer->pDxDevice->CreateUnorderedAccessView(pBuffer->pDxResource, &uavDesc, &pBuffer->pDxUavHandle);
	}

	pBuffer->mBufferId = (++gBufferIds << 8U) + Thread::GetCurrentThreadID();

	*pp_buffer = pBuffer;
}

void removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	UNREF_PARAM(pRenderer);
	ASSERT(pRenderer);
	ASSERT(pBuffer);

	SAFE_RELEASE(pBuffer->pDxSrvHandle);
	SAFE_RELEASE(pBuffer->pDxUavHandle);
	SAFE_RELEASE(pBuffer->pDxResource);
	SAFE_FREE(pBuffer);
}

void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
	UNREF_PARAM(pRange);

	D3D11_MAP           mapType = {};
	ResourceMemoryUsage mem = pBuffer->mDesc.mMemoryUsage;
	switch (mem)
	{
		case RESOURCE_MEMORY_USAGE_CPU_ONLY: mapType = D3D11_MAP_READ_WRITE; break;
		case RESOURCE_MEMORY_USAGE_CPU_TO_GPU: mapType = D3D11_MAP_WRITE_NO_OVERWRITE; break;
		case RESOURCE_MEMORY_USAGE_GPU_TO_CPU: mapType = D3D11_MAP_READ; break;
		default: break;
	}
	D3D11_MAPPED_SUBRESOURCE sub = {};
	pRenderer->pDxContext->Map(pBuffer->pDxResource, 0, mapType, 0, &sub);
	pBuffer->pCpuMappedAddress = sub.pData;
}

void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer) { pRenderer->pDxContext->Unmap(pBuffer->pDxResource, 0); }

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
		pTexture->pDxResource = (ID3D11Resource*)pDesc->pNativeHandle;
	}
	else
	{
		pTexture->mOwnsImage = true;
	}

	//add to gpu
	DXGI_FORMAT    dxFormat = util_to_dx_image_format(pDesc->mFormat, pDesc->mSrgb);
	DescriptorType descriptors = pDesc->mDescriptors;

	ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

	if (NULL == pTexture->pDxResource)
	{
		if (pDesc->mDepth > 1)
		{
			ID3D11Texture3D*     pTex2D;
			D3D11_TEXTURE3D_DESC desc = {};
			desc.BindFlags = util_determine_dx_bind_flags(pDesc->mDescriptors, pDesc->mStartState);
			desc.CPUAccessFlags = 0;
			desc.Depth = pDesc->mDepth;
			desc.Format = util_to_dx_image_format_typeless(pDesc->mFormat);
			desc.Height = pDesc->mHeight;
			desc.MipLevels = pDesc->mMipLevels;
			desc.MiscFlags = util_determine_dx_resource_misc_flags(pDesc->mDescriptors, pDesc->mFormat);
			if (pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_BIT)
				desc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
			desc.Usage = util_to_dx_usage(RESOURCE_MEMORY_USAGE_GPU_ONLY);
			desc.Width = pDesc->mWidth;
			pRenderer->pDxDevice->CreateTexture3D(&desc, NULL, &pTex2D);
			pTexture->pDxResource = pTex2D;
		}
		else if (pDesc->mHeight > 1)
		{
			ID3D11Texture2D*     pTex2D;
			D3D11_TEXTURE2D_DESC desc = {};
			desc.ArraySize = pDesc->mArraySize;
			desc.BindFlags = util_determine_dx_bind_flags(pDesc->mDescriptors, pDesc->mStartState);
			desc.CPUAccessFlags = 0;
			desc.Format = util_to_dx_image_format_typeless(pDesc->mFormat);
			desc.Height = pDesc->mHeight;
			desc.MipLevels = pDesc->mMipLevels;
			desc.MiscFlags = util_determine_dx_resource_misc_flags(pDesc->mDescriptors, pDesc->mFormat);
			desc.SampleDesc.Count = pDesc->mSampleCount;
			desc.SampleDesc.Quality = pDesc->mSampleQuality;
			desc.Usage = util_to_dx_usage(RESOURCE_MEMORY_USAGE_GPU_ONLY);
			desc.Width = pDesc->mWidth;
			pRenderer->pDxDevice->CreateTexture2D(&desc, NULL, &pTex2D);
			pTexture->pDxResource = pTex2D;
		}
		else
		{
			ID3D11Texture1D*     pTex1D;
			D3D11_TEXTURE1D_DESC desc = {};
			desc.ArraySize = pDesc->mArraySize;
			desc.BindFlags = util_determine_dx_bind_flags(pDesc->mDescriptors, pDesc->mStartState);
			desc.CPUAccessFlags = 0;
			desc.Format = util_to_dx_image_format_typeless(pDesc->mFormat);
			desc.MipLevels = pDesc->mMipLevels;
			desc.MiscFlags = util_determine_dx_resource_misc_flags(pDesc->mDescriptors, pDesc->mFormat);
			desc.Usage = util_to_dx_usage(RESOURCE_MEMORY_USAGE_GPU_ONLY);
			desc.Width = pDesc->mWidth;
			pRenderer->pDxDevice->CreateTexture1D(&desc, NULL, &pTex1D);
			pTexture->pDxResource = pTex1D;
		}

		Image img;
		img.RedefineDimensions(
			pTexture->mDesc.mFormat, pTexture->mDesc.mWidth, pTexture->mDesc.mHeight, pTexture->mDesc.mDepth, pTexture->mDesc.mMipLevels);
		pTexture->mTextureSize = pDesc->mArraySize * img.GetMipMappedSize(0, pTexture->mDesc.mMipLevels);

		pTexture->mCurrentState = pDesc->mStartState;
	}
	else
	{
		D3D11_RESOURCE_DIMENSION type = {};
		pTexture->pDxResource->GetType(&type);
		switch (type)
		{
			case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			{
				D3D11_TEXTURE1D_DESC desc = {};
				((ID3D11Texture1D*)pTexture->pDxResource)->GetDesc(&desc);
				dxFormat = desc.Format;
				break;
			}
			case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			{
				D3D11_TEXTURE2D_DESC desc = {};
				((ID3D11Texture2D*)pTexture->pDxResource)->GetDesc(&desc);
				dxFormat = desc.Format;
				break;
			}
			case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			{
				D3D11_TEXTURE3D_DESC desc = {};
				((ID3D11Texture3D*)pTexture->pDxResource)->GetDesc(&desc);
				dxFormat = desc.Format;
				break;
			}
			default: break;
		}
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC  srvDesc = {};
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	D3D11_RESOURCE_DIMENSION type = {};
	pTexture->pDxResource->GetType(&type);

	switch (type)
	{
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		{
			D3D11_TEXTURE1D_DESC desc = {};
			((ID3D11Texture1D*)pTexture->pDxResource)->GetDesc(&desc);

			if (desc.ArraySize > 1)
			{
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
				uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
				// SRV
				srvDesc.Texture1DArray.ArraySize = desc.ArraySize;
				srvDesc.Texture1DArray.FirstArraySlice = 0;
				srvDesc.Texture1DArray.MipLevels = desc.MipLevels;
				srvDesc.Texture1DArray.MostDetailedMip = 0;
				// UAV
				uavDesc.Texture1DArray.ArraySize = desc.ArraySize;
				uavDesc.Texture1DArray.FirstArraySlice = 0;
				uavDesc.Texture1DArray.MipSlice = 0;
			}
			else
			{
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
				uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
				// SRV
				srvDesc.Texture1D.MipLevels = desc.MipLevels;
				srvDesc.Texture1D.MostDetailedMip = 0;
				// UAV
				uavDesc.Texture1D.MipSlice = 0;
			}
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
		{
			D3D11_TEXTURE2D_DESC desc = {};
			((ID3D11Texture2D*)pTexture->pDxResource)->GetDesc(&desc);
			if (DESCRIPTOR_TYPE_TEXTURE_CUBE == (descriptors & DESCRIPTOR_TYPE_TEXTURE_CUBE))
			{
				ASSERT(desc.ArraySize % 6 == 0);

				if (desc.ArraySize > 6)
				{
					srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
					// SRV
					srvDesc.TextureCubeArray.First2DArrayFace = 0;
					srvDesc.TextureCubeArray.MipLevels = desc.MipLevels;
					srvDesc.TextureCubeArray.MostDetailedMip = 0;
					srvDesc.TextureCubeArray.NumCubes = desc.ArraySize / 6;
				}
				else
				{
					srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
					// SRV
					srvDesc.TextureCube.MipLevels = desc.MipLevels;
					srvDesc.TextureCube.MostDetailedMip = 0;
				}

				// UAV
				uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
				uavDesc.Texture2DArray.ArraySize = desc.ArraySize;
				uavDesc.Texture2DArray.FirstArraySlice = 0;
				uavDesc.Texture2DArray.MipSlice = 0;
			}
			else
			{
				if (desc.ArraySize > 1)
				{
					if (desc.SampleDesc.Count > SAMPLE_COUNT_1)
					{
						srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
						// Cannot create a multisampled uav
						// SRV
						srvDesc.Texture2DMSArray.ArraySize = desc.ArraySize;
						srvDesc.Texture2DMSArray.FirstArraySlice = 0;
						// No UAV
					}
					else
					{
						srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
						uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
						// SRV
						srvDesc.Texture2DArray.ArraySize = desc.ArraySize;
						srvDesc.Texture2DArray.FirstArraySlice = 0;
						srvDesc.Texture2DArray.MipLevels = desc.MipLevels;
						srvDesc.Texture2DArray.MostDetailedMip = 0;
						// UAV
						uavDesc.Texture2DArray.ArraySize = desc.ArraySize;
						uavDesc.Texture2DArray.FirstArraySlice = 0;
						uavDesc.Texture2DArray.MipSlice = 0;
					}
				}
				else
				{
					if (desc.SampleDesc.Count > SAMPLE_COUNT_1)
					{
						srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
						// Cannot create a multisampled uav
					}
					else
					{
						srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
						uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
						// SRV
						srvDesc.Texture2D.MipLevels = desc.MipLevels;
						srvDesc.Texture2D.MostDetailedMip = 0;
						// UAV
						uavDesc.Texture2D.MipSlice = 0;
					}
				}
			}
			break;
		}
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
		{
			D3D11_TEXTURE3D_DESC desc = {};
			((ID3D11Texture3D*)pTexture->pDxResource)->GetDesc(&desc);

			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
			// SRV
			srvDesc.Texture3D.MipLevels = desc.MipLevels;
			srvDesc.Texture3D.MostDetailedMip = 0;
			// UAV
			uavDesc.Texture3D.MipSlice = 0;
			uavDesc.Texture3D.FirstWSlice = 0;
			uavDesc.Texture3D.WSize = desc.Depth;
			break;
		}
		default: break;
	}

	if (descriptors & DESCRIPTOR_TYPE_TEXTURE)
	{
		ASSERT(srvDesc.ViewDimension != D3D11_SRV_DIMENSION_UNKNOWN);

		srvDesc.Format = util_to_dx_srv_format(dxFormat);
		pRenderer->pDxDevice->CreateShaderResourceView(pTexture->pDxResource, &srvDesc, &pTexture->pDxSRVDescriptor);
	}

	if (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
	{
		uavDesc.Format = util_to_dx_uav_format(dxFormat);
		pTexture->pDxUAVDescriptors = (ID3D11UnorderedAccessView**)conf_calloc(pDesc->mMipLevels, sizeof(ID3D11UnorderedAccessView*));
		for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
		{
			uavDesc.Texture1DArray.MipSlice = i;
			pRenderer->pDxDevice->CreateUnorderedAccessView(pTexture->pDxResource, &uavDesc, &pTexture->pDxUAVDescriptors[i]);
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

	SAFE_RELEASE(pTexture->pDxSRVDescriptor);
	if (pTexture->pDxUAVDescriptors)
	{
		for (uint32_t i = 0; i < pTexture->mDesc.mMipLevels; ++i)
			SAFE_RELEASE(pTexture->pDxUAVDescriptors[i]);
	}

	if (pTexture->mOwnsImage)
		SAFE_RELEASE(pTexture->pDxResource);

	SAFE_FREE(pTexture->pDxUAVDescriptors);
	SAFE_FREE(pTexture);
}
/************************************************************************/
// Pipeline Functions
/************************************************************************/
void addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
{
	RootSignature* pRootSignature = (RootSignature*)conf_calloc(1, sizeof(*pRootSignature));
	ASSERT(pRootSignature);

	tinystl::vector<ShaderResource>                            shaderResources;
	tinystl::vector<uint32_t>                                  constantSizes;
	tinystl::vector<tinystl::pair<DescriptorInfo*, Sampler*> > staticSamplers;
	ShaderStage                                                shaderStages = SHADER_STAGE_NONE;
	bool                                                       useInputLayout = false;

	tinystl::unordered_map<tinystl::string, Sampler*> staticSamplerMap;
	for (uint32_t i = 0; i < pRootSignatureDesc->mStaticSamplerCount; ++i)
		staticSamplerMap.insert({ pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i] });

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
		pDesc->mDesc.constant_size = pRes->constant_size;
		pDesc->mUpdateFrquency = updateFreq;

		pDesc->mDesc.name_size = pRes->name_size;
		pDesc->mDesc.name = (const char*)conf_calloc(pDesc->mDesc.name_size + 1, sizeof(char));
		memcpy((char*)pDesc->mDesc.name, pRes->name, pRes->name_size);

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
				// TODO
				//layouts[setIndex].mSamplerTable.emplace_back(pDesc);
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
				pDesc->mDesc.type = DESCRIPTOR_TYPE_ROOT_CONSTANT;
				pDesc->mDesc.size = constantSizes[i] / sizeof(uint32_t);
			}
			else
			{
				// By default DESCRIPTOR_TYPE_UNIFORM_BUFFER maps to D3D12_ROOT_PARAMETER_TYPE_CBV
				// But since the size of root descriptors is 2 DWORDS, some of these uniform buffers might get placed in descriptor tables
				// if the size of the root signature goes over the max recommended size on the specific hardware
				// TODO
				//layouts[setIndex].mConstantParams.emplace_back(pDesc);
			}
		}
		else
		{
			// TODO
			//layouts[setIndex].mCbvSrvUavTable.emplace_back (pDesc);
		}

		// TODO
		//layouts[setIndex].mDescriptorIndexMap[pDesc] = i;
	}

	pRootSignature->mStaticSamplerCount = (uint32_t)staticSamplers.size();
	pRootSignature->ppStaticSamplers = (ID3D11SamplerState**)conf_calloc(staticSamplers.size(), sizeof(ID3D11SamplerState*));
	pRootSignature->pStaticSamplerStages = (ShaderStage*)conf_calloc(staticSamplers.size(), sizeof(ShaderStage));
	pRootSignature->pStaticSamplerSlots = (uint32_t*)conf_calloc(staticSamplers.size(), sizeof(uint32_t));
	for (uint32_t i = 0; i < pRootSignature->mStaticSamplerCount; ++i)
	{
		pRootSignature->ppStaticSamplers[i] = staticSamplers[i].second->pSamplerState;
		pRootSignature->pStaticSamplerStages[i] = staticSamplers[i].first->mDesc.used_stages;
		pRootSignature->pStaticSamplerSlots[i] = staticSamplers[i].first->mDesc.reg;
	}

	*ppRootSignature = pRootSignature;
}

void removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
	for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
	{
		SAFE_FREE((void*)pRootSignature->pDescriptors[i].mDesc.name);
	}

	pRootSignature->pDescriptorManagerMap.~unordered_map();
	pRootSignature->pDescriptorNameToIndexMap.~unordered_map();

	SAFE_FREE(pRootSignature->pDescriptors);
	SAFE_FREE(pRootSignature->ppStaticSamplers);
	SAFE_FREE(pRootSignature->pStaticSamplerStages);
	SAFE_FREE(pRootSignature->pStaticSamplerSlots);

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

	const Shader*       pShaderProgram = pDesc->pShaderProgram;
	const VertexLayout* pVertexLayout = pDesc->pVertexLayout;

	//copy the given pipeline settings into new pipeline
	memcpy(&(pPipeline->mGraphics), pDesc, sizeof(*pDesc));
	pPipeline->mType = PIPELINE_TYPE_GRAPHICS;

	//add to gpu
	if (pShaderProgram->mStages & SHADER_STAGE_VERT)
	{
		pPipeline->pDxVertexShader = pShaderProgram->pDxVertexShader;
	}
	if (pShaderProgram->mStages & SHADER_STAGE_FRAG)
	{
		pPipeline->pDxPixelShader = pShaderProgram->pDxPixelShader;
	}
	if (pShaderProgram->mStages & SHADER_STAGE_HULL)
	{
		pPipeline->pDxHullShader = pShaderProgram->pDxHullShader;
	}
	if (pShaderProgram->mStages & SHADER_STAGE_DOMN)
	{
		pPipeline->pDxDomainShader = pShaderProgram->pDxDomainShader;
	}
	if (pShaderProgram->mStages & SHADER_STAGE_GEOM)
	{
		pPipeline->pDxGeometryShader = pShaderProgram->pDxGeometryShader;
	}

	uint32_t input_elementCount = 0;
	DECLARE_ZERO(D3D11_INPUT_ELEMENT_DESC, input_elements[MAX_VERTEX_ATTRIBS]);

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
				uint32_t name_length = min((uint32_t)MAX_SEMANTIC_NAME_LENGTH, attrib->mSemanticNameLength);
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
			input_elements[input_elementCount].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			if (attrib->mRate == VERTEX_ATTRIB_RATE_INSTANCE)
			{
				input_elements[input_elementCount].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
				input_elements[input_elementCount].InstanceDataStepRate = 1;
			}
			else
			{
				input_elements[input_elementCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
				input_elements[input_elementCount].InstanceDataStepRate = 0;
			}
			++input_elementCount;
		}
	}
	if (input_elementCount)
	{
		pRenderer->pDxDevice->CreateInputLayout(
			input_elements, input_elementCount, pDesc->pShaderProgram->pDxInputSignature->GetBufferPointer(),
			pDesc->pShaderProgram->pDxInputSignature->GetBufferSize(), &pPipeline->pDxInputLayout);
	}

	pPipeline->mGraphics.pRasterizerState = pDesc->pRasterizerState != NULL ? pDesc->pRasterizerState : pRenderer->pDefaultRasterizerState;
	pPipeline->mGraphics.pDepthState = pDesc->pDepthState != NULL ? pDesc->pDepthState : pRenderer->pDefaultDepthState;
	pPipeline->mGraphics.pBlendState = pDesc->pBlendState != NULL ? pDesc->pBlendState : pRenderer->pDefaultBlendState;

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

	//allocate new pipeline
	Pipeline* pPipeline = (Pipeline*)conf_calloc(1, sizeof(*pPipeline));
	ASSERT(pPipeline);

	//copy pipeline settings
	memcpy(&(pPipeline->mCompute), pDesc, sizeof(*pDesc));
	pPipeline->mType = PIPELINE_TYPE_COMPUTE;

	pPipeline->pDxComputeShader = pDesc->pShaderProgram->pDxComputeShader;

	*ppPipeline = pPipeline;
}

void removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pPipeline);

	//delete pipeline from device
	SAFE_RELEASE(pPipeline->pDxInputLayout);

	SAFE_FREE(pPipeline);
}
/************************************************************************/
// Pipeline State Functions
/************************************************************************/
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

	D3D11_BLEND_DESC desc = {};

	desc.AlphaToCoverageEnable = (BOOL)pDesc->mAlphaToCoverage;
	desc.IndependentBlendEnable = TRUE;
	for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; i++)
	{
		if (pDesc->mRenderTargetMask & (1 << i))
		{
			BOOL blendEnable =
				(gBlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]] != D3D11_BLEND_ONE ||
				 gBlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]] != D3D11_BLEND_ZERO ||
				 gBlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]] != D3D11_BLEND_ONE ||
				 gBlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]] != D3D11_BLEND_ZERO);

			desc.RenderTarget[i].BlendEnable = blendEnable;
			desc.RenderTarget[i].RenderTargetWriteMask = (UINT8)pDesc->mMasks[blendDescIndex];
			desc.RenderTarget[i].BlendOp = gBlendOpTranslator[pDesc->mBlendModes[blendDescIndex]];
			desc.RenderTarget[i].SrcBlend = gBlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]];
			desc.RenderTarget[i].DestBlend = gBlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]];
			desc.RenderTarget[i].BlendOpAlpha = gBlendOpTranslator[pDesc->mBlendAlphaModes[blendDescIndex]];
			desc.RenderTarget[i].SrcBlendAlpha = gBlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]];
			desc.RenderTarget[i].DestBlendAlpha = gBlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]];
		}

		if (pDesc->mIndependentBlend)
			++blendDescIndex;
	}

	if (FAILED(pRenderer->pDxDevice->CreateBlendState(&desc, &pBlendState->pBlendState)))
		LOGERROR("Failed to create blend state.");

	*ppBlendState = pBlendState;
}

void removeBlendState(BlendState* pBlendState)
{
	SAFE_RELEASE(pBlendState->pBlendState);
	SAFE_FREE(pBlendState);
}

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

	D3D11_DEPTH_STENCIL_DESC desc;
	desc.DepthEnable = (BOOL)pDesc->mDepthTest;
	desc.DepthWriteMask = pDesc->mDepthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = gComparisonFuncTranslator[pDesc->mDepthFunc];
	desc.StencilEnable = (BOOL)pDesc->mStencilTest;
	desc.StencilReadMask = pDesc->mStencilReadMask;
	desc.StencilWriteMask = pDesc->mStencilWriteMask;
	desc.BackFace.StencilFunc = gComparisonFuncTranslator[pDesc->mStencilBackFunc];
	desc.FrontFace.StencilFunc = gComparisonFuncTranslator[pDesc->mStencilFrontFunc];
	desc.BackFace.StencilDepthFailOp = gStencilOpTranslator[pDesc->mDepthBackFail];
	desc.FrontFace.StencilDepthFailOp = gStencilOpTranslator[pDesc->mDepthFrontFail];
	desc.BackFace.StencilFailOp = gStencilOpTranslator[pDesc->mStencilBackFail];
	desc.FrontFace.StencilFailOp = gStencilOpTranslator[pDesc->mStencilFrontFail];
	desc.BackFace.StencilPassOp = gStencilOpTranslator[pDesc->mStencilFrontPass];
	desc.FrontFace.StencilPassOp = gStencilOpTranslator[pDesc->mStencilBackPass];

	if (FAILED(pRenderer->pDxDevice->CreateDepthStencilState(&desc, &pDepthState->pDxDepthStencilState)))
		LOGERROR("Failed to create depth state.");

	*ppDepthState = pDepthState;
}

void removeDepthState(DepthState* pDepthState)
{
	SAFE_RELEASE(pDepthState->pDxDepthStencilState);
	SAFE_FREE(pDepthState);
}

void addRasterizerState(Renderer* pRenderer, const RasterizerStateDesc* pDesc, RasterizerState** ppRasterizerState)
{
	UNREF_PARAM(pRenderer);

	ASSERT(pDesc->mFillMode < FillMode::MAX_FILL_MODES);
	ASSERT(pDesc->mCullMode < CullMode::MAX_CULL_MODES);
	ASSERT(pDesc->mFrontFace == FRONT_FACE_CCW || pDesc->mFrontFace == FRONT_FACE_CW);

	RasterizerState* pRasterizerState = (RasterizerState*)conf_calloc(1, sizeof(*pRasterizerState));

	D3D11_RASTERIZER_DESC desc;
	desc.FillMode = gFillModeTranslator[pDesc->mFillMode];
	desc.CullMode = gCullModeTranslator[pDesc->mCullMode];
	desc.FrontCounterClockwise = pDesc->mFrontFace == FRONT_FACE_CCW;
	desc.DepthBias = pDesc->mDepthBias;
	desc.DepthBiasClamp = 0.0f;
	desc.SlopeScaledDepthBias = pDesc->mSlopeScaledDepthBias;
	desc.DepthClipEnable = TRUE;
	desc.MultisampleEnable = pDesc->mMultiSample ? TRUE : FALSE;
	desc.AntialiasedLineEnable = FALSE;

	if (FAILED(pRenderer->pDxDevice->CreateRasterizerState(&desc, &pRasterizerState->pDxRasterizerState)))
		LOGERROR("Failed to create depth state.");

	*ppRasterizerState = pRasterizerState;
}

void removeRasterizerState(RasterizerState* pRasterizerState)
{
	SAFE_RELEASE(pRasterizerState->pDxRasterizerState);
	SAFE_FREE(pRasterizerState);
}
/************************************************************************/
// Command buffer Functions
/************************************************************************/
void beginCmd(Cmd* pCmd)
{
	ASSERT(pCmd);

	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	if (cachedCmdsIter != gCachedCmds.end())
		cachedCmdsIter->second.clear();    // found it so clear the cmds list
	else
		gCachedCmds[pCmd];    // create a new cached cmd list

	pCmd->mDescriptorStructPoolOffset = 0;
	pCmd->mDescriptorNamePoolOffset = 0;
	pCmd->mDescriptorResourcePoolOffset = 0;
}

void endCmd(Cmd* pCmd)
{
	ASSERT(pCmd);

	// TODO: should we do anything particular here?
}

void cmdBindRenderTargets(
	Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil,
	const LoadActionsDesc* pLoadActions /* = NULL*/, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice,
	uint32_t depthMipSlice)
{
	ASSERT(pCmd);

	if (!renderTargetCount && !pDepthStencil)
		return;

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdBindRenderTargets;
	cmd.mBindRenderTargetsCmd.renderTargetCount = renderTargetCount;
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
		cmd.mBindRenderTargetsCmd.ppRenderTargets[i] = ppRenderTargets[i]->pDxRtvDescriptors[handle];
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
		cmd.mBindRenderTargetsCmd.pDepthStencil = pDepthStencil->pDxDsvDescriptors[handle];
	}
	cmd.mBindRenderTargetsCmd.mLoadActions = pLoadActions ? *pLoadActions : LoadActionsDesc{};
	cachedCmdsIter->second.push_back(cmd);

	pCmd->mBoundWidth = pDepthStencil ? pDepthStencil->mDesc.mWidth : ppRenderTargets[0]->mDesc.mWidth;
	pCmd->mBoundHeight = pDepthStencil ? pDepthStencil->mDesc.mHeight : ppRenderTargets[0]->mDesc.mHeight;
}

void cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
	ASSERT(pCmd);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdSetViewport;
	cmd.mSetViewportCmd.x = x;
	cmd.mSetViewportCmd.y = y;
	cmd.mSetViewportCmd.width = width;
	cmd.mSetViewportCmd.height = height;
	cmd.mSetViewportCmd.minDepth = minDepth;
	cmd.mSetViewportCmd.maxDepth = maxDepth;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	ASSERT(pCmd);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdSetScissor;
	cmd.mSetScissorCmd.x = x;
	cmd.mSetScissorCmd.y = y;
	cmd.mSetScissorCmd.width = width;
	cmd.mSetScissorCmd.height = height;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
	ASSERT(pCmd);
	ASSERT(pPipeline);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdBindPipeline;
	cmd.mBindPipelineCmd.pPipeline = pPipeline;
	cachedCmdsIter->second.push_back(cmd);
}

const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName, uint32_t* pIndex)
{
	using DescriptorNameToIndexMap = tinystl::unordered_map<uint32_t, uint32_t>;
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

void cmdBindDescriptors(Cmd* pCmd, RootSignature* pRootSignature, uint32_t numDescriptors, DescriptorData* pDescParams)
{
	ASSERT(pCmd);
	ASSERT(pRootSignature);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	// Create descriptor pool for storing the descriptor data
	if (!pCmd->pDescriptorNamePool)
	{
		pCmd->pDescriptorNamePool = (uint8_t*)conf_calloc(1024 * 32, sizeof(uint8_t));
		pCmd->pDescriptorStructPool = (uint8_t*)conf_calloc(1024 * 32, sizeof(uint8_t));
		pCmd->pDescriptorResourcesPool = (uint8_t*)conf_calloc(1024 * 32, sizeof(uint8_t));
	}

	DescriptorData* pBegin = NULL;

	for (uint32_t i = 0; i < numDescriptors; ++i)
	{
		uint8_t*              pPool = pCmd->pDescriptorStructPool + pCmd->mDescriptorStructPoolOffset;
		const DescriptorData* pSrc = &pDescParams[i];
		DescriptorData*       pDst = (DescriptorData*)pPool;
		uint32_t              index = 0;
		const DescriptorInfo* pDesc = get_descriptor(pRootSignature, pSrc->pName, &index);

		if (i == 0)
			pBegin = pDst;

		memcpy(pDst, pSrc, sizeof(DescriptorData));
		pDst->mCount = max(1U, pDst->mCount);
		pCmd->mDescriptorStructPoolOffset += sizeof(DescriptorData);

		pDst->pName = (char*)(pCmd->pDescriptorNamePool + pCmd->mDescriptorNamePoolOffset);
		memcpy((void*)pDst->pName, pSrc->pName, strlen(pSrc->pName));
		((char*)pDst->pName)[strlen(pSrc->pName)] = '\0';
		pCmd->mDescriptorNamePoolOffset += strlen(pSrc->pName) + 1;

		const uint32_t count = max(1U, pSrc->mCount);

		if (pDesc->mDesc.type == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		{
			if (pSrc->pOffsets)
			{
				pDst->pOffsets = (uint64_t*)(pCmd->pDescriptorResourcesPool + pCmd->mDescriptorResourcePoolOffset);
				memcpy(pDst->pOffsets, pSrc->pOffsets, count * sizeof(uint64_t));
				pCmd->mDescriptorResourcePoolOffset += round_up_64(count * sizeof(uint64_t), 16);
			}
			if (pSrc->pSizes)
			{
				pDst->pSizes = (uint64_t*)(pCmd->pDescriptorResourcesPool + pCmd->mDescriptorResourcePoolOffset);
				memcpy(pDst->pSizes, pSrc->pSizes, count * sizeof(uint64_t));
				pCmd->mDescriptorResourcePoolOffset += round_up_64(count * sizeof(uint64_t), 16);
			}
		}
		if (pDesc && pDesc->mDesc.type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
		{
			pDst->pRootConstant = pCmd->pDescriptorResourcesPool + pCmd->mDescriptorResourcePoolOffset;
			memcpy(pDst->pRootConstant, pSrc->pRootConstant, pDesc->mDesc.size * sizeof(uint32_t));
			pCmd->mDescriptorResourcePoolOffset += round_up_64(pDesc->mDesc.size * sizeof(uint32_t), 16);
		}
		else
		{
			pDst->ppTextures = (Texture**)(pCmd->pDescriptorResourcesPool + pCmd->mDescriptorResourcePoolOffset);
			memcpy(pDst->ppTextures, pSrc->ppTextures, count * sizeof(Texture*));
			pCmd->mDescriptorResourcePoolOffset += round_up_64(count * sizeof(Texture*), 16);
		}
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdBindDescriptors;
	cmd.mBindDescriptorsCmd.pRootSignature = pRootSignature;
	cmd.mBindDescriptorsCmd.numDescriptors = numDescriptors;
	cmd.mBindDescriptorsCmd.pDescParams = pBegin;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t offset)
{
	ASSERT(pCmd);
	ASSERT(pBuffer);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdBindIndexBuffer;
	cmd.mBindIndexBufferCmd.pBuffer = pBuffer;
	cmd.mBindIndexBufferCmd.offset = (uint32_t)offset;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, uint64_t* pOffsets)
{
	ASSERT(pCmd);
	ASSERT(0 != bufferCount);
	ASSERT(ppBuffers);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdBindVertexBuffer;
	cmd.mBindVertexBufferCmd.bufferCount = bufferCount;
	cmd.mBindVertexBufferCmd.ppBuffers = (ID3D11Buffer**)conf_calloc(bufferCount, sizeof(ID3D11Buffer*));
	cmd.mBindVertexBufferCmd.pOffsets = (uint32_t*)conf_calloc(bufferCount, sizeof(uint32_t));
	cmd.mBindVertexBufferCmd.pStrides = (uint32_t*)conf_calloc(bufferCount, sizeof(uint32_t));
	for (uint32_t i = 0; i < bufferCount; ++i)
	{
		cmd.mBindVertexBufferCmd.ppBuffers[i] = ppBuffers[i]->pDxResource;
		cmd.mBindVertexBufferCmd.pStrides[i] = ppBuffers[i]->mDesc.mVertexStride;
		cmd.mBindVertexBufferCmd.pOffsets[i] = (uint32_t)(pOffsets ? pOffsets[i] : 0);
	}
	cachedCmdsIter->second.push_back(cmd);
}

void cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
	ASSERT(pCmd);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdDraw;
	cmd.mDrawCmd.vertexCount = vertexCount;
	cmd.mDrawCmd.firstVertex = firstVertex;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
	ASSERT(pCmd);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdDrawInstanced;
	cmd.mDrawInstancedCmd.vertexCount = vertexCount;
	cmd.mDrawInstancedCmd.firstVertex = firstVertex;
	cmd.mDrawInstancedCmd.instanceCount = instanceCount;
	cmd.mDrawInstancedCmd.firstInstance = firstInstance;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
	ASSERT(pCmd);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdDrawIndexed;
	cmd.mDrawIndexedCmd.indexCount = indexCount;
	cmd.mDrawIndexedCmd.firstIndex = firstIndex;
	cmd.mDrawIndexedCmd.firstVertex = firstVertex;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdDrawIndexedInstanced(
	Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstInstance, uint32_t firstVertex)
{
	ASSERT(pCmd);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdDrawIndexedInstanced;
	cmd.mDrawIndexedInstancedCmd.indexCount = indexCount;
	cmd.mDrawIndexedInstancedCmd.firstIndex = firstIndex;
	cmd.mDrawIndexedInstancedCmd.instanceCount = instanceCount;
	cmd.mDrawIndexedInstancedCmd.firstInstance = firstInstance;
	cmd.mDrawIndexedInstancedCmd.firstVertex = firstVertex;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	ASSERT(pCmd);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdDispatch;
	cmd.mDispatchCmd.groupCountX = groupCountX;
	cmd.mDispatchCmd.groupCountY = groupCountY;
	cmd.mDispatchCmd.groupCountZ = groupCountZ;
	cachedCmdsIter->second.push_back(cmd);
}
/************************************************************************/
// Transition Commands
/************************************************************************/
void cmdResourceBarrier(
	Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers,
	bool batch)
{
	ASSERT(pCmd);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdResourceBarrier;
	cmd.mResourceBarrierCmd.numBufferBarriers = numBufferBarriers;
	cmd.mResourceBarrierCmd.pBufferBarriers = pBufferBarriers;
	cmd.mResourceBarrierCmd.numTextureBarriers = numTextureBarriers;
	cmd.mResourceBarrierCmd.pTextureBarriers = pTextureBarriers;
	cmd.mResourceBarrierCmd.batch = batch;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdSynchronizeResources(Cmd* pCmd, uint32_t numBuffers, Buffer** ppBuffers, uint32_t numTextures, Texture** ppTextures, bool batch)
{
	ASSERT(pCmd);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdSynchronizeResources;
	cmd.mSynchronizeResourcesCmd.numBuffers = numBuffers;
	cmd.mSynchronizeResourcesCmd.ppBuffers = ppBuffers;
	cmd.mSynchronizeResourcesCmd.numTextures = numTextures;
	cmd.mSynchronizeResourcesCmd.ppTextures = ppTextures;
	cmd.mSynchronizeResourcesCmd.batch = batch;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdFlushBarriers(Cmd* pCmd)
{
	ASSERT(pCmd);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdFlushBarriers;
	cachedCmdsIter->second.push_back(cmd);
}
/************************************************************************/
// Queue Fence Semaphore Functions
/************************************************************************/
void acquireNextImage(
	Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pSwapChainImageIndex)
{
	*pSwapChainImageIndex = 0;
}

void reset_shader_resources(ID3D11DeviceContext* pContext)
{
	static ID3D11ShaderResourceView* pNullSRV[32] = { NULL };
	pContext->VSSetShaderResources(0, 32, pNullSRV);
	pContext->PSSetShaderResources(0, 32, pNullSRV);
	pContext->HSSetShaderResources(0, 32, pNullSRV);
	pContext->DSSetShaderResources(0, 32, pNullSRV);
	pContext->GSSetShaderResources(0, 32, pNullSRV);
	pContext->CSSetShaderResources(0, 32, pNullSRV);
}

void reset_uavs(ID3D11DeviceContext* pContext)
{
	static ID3D11UnorderedAccessView* pNullUAV[D3D11_PS_CS_UAV_REGISTER_COUNT] = { NULL };
	pContext->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, pNullUAV, NULL);
}

void set_constant_buffers(ID3D11DeviceContext* pContext, ShaderStage used_stages, uint32_t reg, uint32_t count, ID3D11Buffer** pCBVs)
{
	if (used_stages & SHADER_STAGE_VERT)
		pContext->VSSetConstantBuffers(reg, count, pCBVs);
	if (used_stages & SHADER_STAGE_FRAG)
		pContext->PSSetConstantBuffers(reg, count, pCBVs);
	if (used_stages & SHADER_STAGE_HULL)
		pContext->HSSetConstantBuffers(reg, count, pCBVs);
	if (used_stages & SHADER_STAGE_DOMN)
		pContext->DSSetConstantBuffers(reg, count, pCBVs);
	if (used_stages & SHADER_STAGE_GEOM)
		pContext->GSSetConstantBuffers(reg, count, pCBVs);
	if (used_stages & SHADER_STAGE_COMP)
		pContext->CSSetConstantBuffers(reg, count, pCBVs);
}

void set_shader_resources(
	ID3D11DeviceContext* pContext, ShaderStage used_stages, uint32_t reg, uint32_t count, ID3D11ShaderResourceView** pSRVs)
{
	if (used_stages & SHADER_STAGE_VERT)
		pContext->VSSetShaderResources(reg, count, pSRVs);
	if (used_stages & SHADER_STAGE_FRAG)
		pContext->PSSetShaderResources(reg, count, pSRVs);
	if (used_stages & SHADER_STAGE_HULL)
		pContext->HSSetShaderResources(reg, count, pSRVs);
	if (used_stages & SHADER_STAGE_DOMN)
		pContext->DSSetShaderResources(reg, count, pSRVs);
	if (used_stages & SHADER_STAGE_GEOM)
		pContext->GSSetShaderResources(reg, count, pSRVs);
	if (used_stages & SHADER_STAGE_COMP)
		pContext->CSSetShaderResources(reg, count, pSRVs);
}

void set_samplers(ID3D11DeviceContext* pContext, ShaderStage used_stages, uint32_t reg, uint32_t count, ID3D11SamplerState** pSamplers)
{
	if (used_stages & SHADER_STAGE_VERT)
		pContext->VSSetSamplers(reg, count, pSamplers);
	if (used_stages & SHADER_STAGE_FRAG)
		pContext->PSSetSamplers(reg, count, pSamplers);
	if (used_stages & SHADER_STAGE_HULL)
		pContext->HSSetSamplers(reg, count, pSamplers);
	if (used_stages & SHADER_STAGE_DOMN)
		pContext->DSSetSamplers(reg, count, pSamplers);
	if (used_stages & SHADER_STAGE_GEOM)
		pContext->GSSetSamplers(reg, count, pSamplers);
	if (used_stages & SHADER_STAGE_COMP)
		pContext->CSSetSamplers(reg, count, pSamplers);
}

void queueSubmit(
	Queue* pQueue, uint32_t cmdCount, Cmd** ppCmds, Fence* pFence, uint32_t waitSemaphoreCount, Semaphore** ppWaitSemaphores,
	uint32_t signalSemaphoreCount, Semaphore** ppSignalSemaphores)
{
	ID3D11DeviceContext* pContext = pQueue->pRenderer->pDxContext;
	ID3D11Device*        pDevice = pQueue->pRenderer->pDxDevice;
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		Cmd*                              pCmd = ppCmds[i];
		const tinystl::vector<CachedCmd>& cmds = gCachedCmds[pCmd];
		for (uint32_t cmdIndex = 0; cmdIndex < (uint32_t)cmds.size(); ++cmdIndex)
		{
			const CachedCmd& cmd = cmds[cmdIndex];
			CmdType          type = cmd.sType;
			switch (type)
			{
				case CMD_TYPE_cmdBindRenderTargets:
				{
					reset_shader_resources(pContext);
					static ID3D11RenderTargetView* pNullRTV[MAX_RENDER_TARGET_ATTACHMENTS] = { NULL };
					pContext->OMSetRenderTargets(MAX_RENDER_TARGET_ATTACHMENTS, pNullRTV, NULL);

					const BindRenderTargetsCmd& rt = cmd.mBindRenderTargetsCmd;
					pContext->OMSetRenderTargets(rt.renderTargetCount, rt.ppRenderTargets, rt.pDepthStencil);
					for (uint32_t i = 0; i < rt.renderTargetCount; ++i)
					{
						if (rt.mLoadActions.mLoadActionsColor[i] == LOAD_ACTION_CLEAR)
						{
							FLOAT clear[4] = { rt.mLoadActions.mClearColorValues[i].r, rt.mLoadActions.mClearColorValues[i].g,
											   rt.mLoadActions.mClearColorValues[i].b, rt.mLoadActions.mClearColorValues[i].a };
							pContext->ClearRenderTargetView(rt.ppRenderTargets[i], clear);
						}
					}
					if (rt.pDepthStencil &&
						(rt.mLoadActions.mLoadActionDepth == LOAD_ACTION_CLEAR || rt.mLoadActions.mLoadActionStencil == LOAD_ACTION_CLEAR))
					{
						uint32_t clearFlag = 0;
						if (rt.mLoadActions.mLoadActionDepth == LOAD_ACTION_CLEAR)
							clearFlag |= D3D11_CLEAR_DEPTH;
						if (rt.mLoadActions.mLoadActionStencil == LOAD_ACTION_CLEAR)
							clearFlag |= D3D11_CLEAR_STENCIL;
						pContext->ClearDepthStencilView(
							rt.pDepthStencil, clearFlag, rt.mLoadActions.mClearDepth.depth, rt.mLoadActions.mClearDepth.stencil);
					}
					break;
				}
				case CMD_TYPE_cmdSetViewport:
				{
					D3D11_VIEWPORT viewport = {};
					viewport.Height = cmd.mSetViewportCmd.height;
					viewport.MaxDepth = cmd.mSetViewportCmd.maxDepth;
					viewport.MinDepth = cmd.mSetViewportCmd.minDepth;
					viewport.TopLeftX = cmd.mSetViewportCmd.x;
					viewport.TopLeftY = cmd.mSetViewportCmd.y;
					viewport.Width = cmd.mSetViewportCmd.width;
					pContext->RSSetViewports(1, &viewport);
					break;
				}
				case CMD_TYPE_cmdSetScissor:
				{
					D3D11_RECT scissor = {};
					scissor.left = cmd.mSetScissorCmd.x;
					scissor.top = cmd.mSetScissorCmd.y;
					scissor.right = cmd.mSetScissorCmd.x + cmd.mSetScissorCmd.width;
					scissor.bottom = cmd.mSetScissorCmd.y + cmd.mSetScissorCmd.height;
					pContext->RSSetScissorRects(1, &scissor);
					break;
				}
				case CMD_TYPE_cmdBindPipeline:
				{
					static const float dummyClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					const Pipeline*    pPipeline = cmd.mBindPipelineCmd.pPipeline;
					if (pPipeline->mType == PIPELINE_TYPE_GRAPHICS)
					{
						pContext->IASetPrimitiveTopology(pPipeline->mDxPrimitiveTopology);
						pContext->IASetInputLayout(pPipeline->pDxInputLayout);
						pContext->RSSetState(pPipeline->mGraphics.pRasterizerState->pDxRasterizerState);
						pContext->OMSetDepthStencilState(pPipeline->mGraphics.pDepthState->pDxDepthStencilState, 0);
						pContext->OMSetBlendState(pPipeline->mGraphics.pBlendState->pBlendState, dummyClearColor, ~0);

						reset_uavs(pContext);
						pContext->VSSetShader(pPipeline->pDxVertexShader, NULL, 0);
						pContext->PSSetShader(pPipeline->pDxPixelShader, NULL, 0);
						pContext->GSSetShader(pPipeline->pDxGeometryShader, NULL, 0);
						pContext->DSSetShader(pPipeline->pDxDomainShader, NULL, 0);
						pContext->HSSetShader(pPipeline->pDxHullShader, NULL, 0);
					}
					else
					{
						pContext->RSSetState(NULL);
						pContext->OMSetDepthStencilState(NULL, 0);
						pContext->OMSetBlendState(NULL, dummyClearColor, ~0);

						reset_uavs(pContext);
						reset_shader_resources(pContext);
						pContext->CSSetShader(pPipeline->pDxComputeShader, NULL, 0);
					}
					break;
				}
				case CMD_TYPE_cmdBindDescriptors:
				{
					/************************************************************************/
					// Bind static samplers
					/************************************************************************/
					const BindDescriptorsCmd& bind = cmd.mBindDescriptorsCmd;
					const RootSignature*      pRootSignature = bind.pRootSignature;
					for (uint32_t i = 0; i < pRootSignature->mStaticSamplerCount; ++i)
					{
						set_samplers(
							pContext, pRootSignature->pStaticSamplerStages[i], pRootSignature->pStaticSamplerSlots[i], 1,
							&pRootSignature->ppStaticSamplers[i]);
					}
					/************************************************************************/
					// Bind regular shader variables
					/************************************************************************/
					for (uint32_t i = 0; i < bind.numDescriptors; ++i)
					{
						const DescriptorData* pParam = &bind.pDescParams[i];

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
						const ShaderResource* pRes = &pDesc->mDesc;
						const DescriptorType  type = pRes->type;
						switch (type)
						{
							case DESCRIPTOR_TYPE_BUFFER:
							{
								ID3D11ShaderResourceView** pSRVs =
									(ID3D11ShaderResourceView**)alloca(pParam->mCount * sizeof(ID3D11ShaderResourceView*));
								for (uint32_t i = 0; i < pParam->mCount; ++i)
									pSRVs[i] = pParam->ppBuffers[i]->pDxSrvHandle;
								set_shader_resources(pContext, pRes->used_stages, pRes->reg, pParam->mCount, pSRVs);
								break;
							}
							case DESCRIPTOR_TYPE_RW_BUFFER:
							{
								ID3D11UnorderedAccessView** pUAVs =
									(ID3D11UnorderedAccessView**)alloca(pParam->mCount * sizeof(ID3D11UnorderedAccessView*));
								for (uint32_t i = 0; i < pParam->mCount; ++i)
									pUAVs[i] = pParam->ppBuffers[i]->pDxUavHandle;

								ASSERT(pRes->used_stages == SHADER_STAGE_COMP);
								pContext->CSSetUnorderedAccessViews(pRes->reg, pParam->mCount, pUAVs, NULL);
								break;
							}
							case DESCRIPTOR_TYPE_ROOT_CONSTANT:
							{
								if (!pCmd->pRootConstantBuffer)
								{
									BufferDesc bufDesc = {};
									bufDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
									bufDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
									bufDesc.mSize = 256;
									addBuffer(pCmd->pRenderer, &bufDesc, &pCmd->pRootConstantBuffer);
								}

								D3D11_MAPPED_SUBRESOURCE sub = {};
								pContext->Map(pCmd->pRootConstantBuffer->pDxResource, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
								memcpy(sub.pData, pParam->pRootConstant, pDesc->mDesc.size * sizeof(uint32_t));
								pContext->Unmap(pCmd->pRootConstantBuffer->pDxResource, 0);
								set_constant_buffers(pContext, pRes->used_stages, pRes->reg, 1, &pCmd->pRootConstantBuffer->pDxResource);
								break;
							}
							case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
							{
								if (pParam->pOffsets || pParam->pSizes)
								{
									if (!pCmd->pTransientConstantBuffer)
									{
										BufferDesc bufDesc = {};
										bufDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
										bufDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
										// Max Constant Buffer Size
										bufDesc.mSize = 65536;
										addBuffer(pCmd->pRenderer, &bufDesc, &pCmd->pTransientConstantBuffer);
									}
									for (uint32_t i = 0; i < pParam->mCount; ++i)
									{
										D3D11_MAPPED_SUBRESOURCE read = {};
										pContext->Map(pParam->ppBuffers[i]->pDxResource, 0, D3D11_MAP_READ, 0, &read);
										D3D11_MAPPED_SUBRESOURCE sub = {};
										pContext->Map(pCmd->pTransientConstantBuffer->pDxResource, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
										memcpy(sub.pData, (uint8_t*)read.pData + pParam->pOffsets[i], pRes->constant_size);
										pContext->Unmap(pCmd->pTransientConstantBuffer->pDxResource, 0);
										pContext->Unmap(pParam->ppBuffers[i]->pDxResource, 0);
										set_constant_buffers(
											pContext, pRes->used_stages, pRes->reg + i, 1, &pCmd->pTransientConstantBuffer->pDxResource);
									}
								}
								else
								{
									ID3D11Buffer** pCBVs = (ID3D11Buffer**)alloca(pParam->mCount * sizeof(ID3D11Buffer*));
									for (uint32_t i = 0; i < pParam->mCount; ++i)
										pCBVs[i] = pParam->ppBuffers[i]->pDxResource;
									set_constant_buffers(pContext, pRes->used_stages, pRes->reg, pParam->mCount, pCBVs);
								}
								break;
							}
							case DESCRIPTOR_TYPE_TEXTURE:
							{
								ID3D11ShaderResourceView** pSRVs =
									(ID3D11ShaderResourceView**)alloca(pParam->mCount * sizeof(ID3D11ShaderResourceView*));
								for (uint32_t i = 0; i < pParam->mCount; ++i)
									pSRVs[i] = pParam->ppTextures[i]->pDxSRVDescriptor;
								set_shader_resources(pContext, pRes->used_stages, pRes->reg, pParam->mCount, pSRVs);
								break;
							}
							case DESCRIPTOR_TYPE_RW_TEXTURE:
							{
								ID3D11UnorderedAccessView** pUAVs =
									(ID3D11UnorderedAccessView**)alloca(pParam->mCount * sizeof(ID3D11UnorderedAccessView*));
								for (uint32_t i = 0; i < pParam->mCount; ++i)
									pUAVs[i] = pParam->ppTextures[i]->pDxUAVDescriptors[pParam->mUAVMipSlice];

								ASSERT(pRes->used_stages == SHADER_STAGE_COMP);
								pContext->CSSetUnorderedAccessViews(pRes->reg, pParam->mCount, pUAVs, NULL);
								break;
							}
							case DESCRIPTOR_TYPE_SAMPLER:
							{
								ID3D11SamplerState** pSamplers = (ID3D11SamplerState**)alloca(pParam->mCount * sizeof(ID3D11SamplerState*));
								for (uint32_t i = 0; i < pParam->mCount; ++i)
									pSamplers[i] = pParam->ppSamplers[i]->pSamplerState;
								set_samplers(pContext, pRes->used_stages, pRes->reg, pParam->mCount, pSamplers);
								break;
							}
							default: break;
						}
					}
					/************************************************************************/
					/************************************************************************/
					break;
				}
				case CMD_TYPE_cmdBindIndexBuffer:
				{
					pContext->IASetIndexBuffer(
						(ID3D11Buffer*)cmd.mBindIndexBufferCmd.pBuffer->pDxResource, cmd.mBindIndexBufferCmd.pBuffer->mDxIndexFormat,
						cmd.mBindIndexBufferCmd.offset);
					break;
				}
				case CMD_TYPE_cmdBindVertexBuffer:
				{
					pContext->IASetVertexBuffers(
						0, cmd.mBindVertexBufferCmd.bufferCount, cmd.mBindVertexBufferCmd.ppBuffers, cmd.mBindVertexBufferCmd.pStrides,
						cmd.mBindVertexBufferCmd.pOffsets);

					conf_free(cmd.mBindVertexBufferCmd.ppBuffers);
					conf_free(cmd.mBindVertexBufferCmd.pStrides);
					conf_free(cmd.mBindVertexBufferCmd.pOffsets);
					break;
				}
				case CMD_TYPE_cmdDraw:
				{
					pContext->Draw(cmd.mDrawCmd.vertexCount, cmd.mDrawCmd.firstVertex);
					break;
				}
				case CMD_TYPE_cmdDrawInstanced:
				{
					pContext->DrawInstanced(
						cmd.mDrawInstancedCmd.vertexCount, cmd.mDrawInstancedCmd.instanceCount, cmd.mDrawInstancedCmd.firstVertex,
						cmd.mDrawInstancedCmd.firstInstance);
					break;
				}
				case CMD_TYPE_cmdDrawIndexed:
				{
					pContext->DrawIndexed(cmd.mDrawIndexedCmd.indexCount, cmd.mDrawIndexedCmd.firstIndex, cmd.mDrawIndexedCmd.firstVertex);
					break;
				}
				case CMD_TYPE_cmdDrawIndexedInstanced:
				{
					pContext->DrawIndexedInstanced(
						cmd.mDrawIndexedInstancedCmd.indexCount, cmd.mDrawIndexedInstancedCmd.instanceCount,
						cmd.mDrawIndexedInstancedCmd.firstIndex, cmd.mDrawIndexedInstancedCmd.firstVertex,
						cmd.mDrawIndexedInstancedCmd.firstInstance);
					break;
				}
				case CMD_TYPE_cmdDispatch:
				{
					pContext->Dispatch(cmd.mDispatchCmd.groupCountX, cmd.mDispatchCmd.groupCountY, cmd.mDispatchCmd.groupCountZ);
					break;
				}
				case CMD_TYPE_cmdResourceBarrier: break;
				case CMD_TYPE_cmdSynchronizeResources: break;
				case CMD_TYPE_cmdFlushBarriers: break;
				case CMD_TYPE_cmdExecuteIndirect: break;
				case CMD_TYPE_cmdBeginQuery:
				{
					const BeginQueryCmd& query = cmd.mBeginQueryCmd;
					pContext->End(query.pQueryHeap->ppDxQueries[query.mQuery.mIndex]);
					break;
				}
				case CMD_TYPE_cmdEndQuery:
				{
					const EndQueryCmd& query = cmd.mEndQueryCmd;
					pContext->End(query.pQueryHeap->ppDxQueries[query.mQuery.mIndex]);
					break;
				}
				case CMD_TYPE_cmdResolveQuery:
				{
					const ResolveQueryCmd& resolve = cmd.mResolveQueryCmd;
					if (resolve.queryCount)
					{
						uint64_t* pResults = (uint64_t*)alloca(resolve.queryCount * sizeof(uint64_t));
						for (uint32_t i = resolve.startQuery; i < resolve.startQuery + resolve.queryCount; ++i)
						{
							while (pContext->GetData(resolve.pQueryHeap->ppDxQueries[i], &pResults[i], sizeof(uint64_t), 0) != S_OK)
								Thread::Sleep(0);
						}
						D3D11_MAPPED_SUBRESOURCE sub = {};
						pContext->Map(resolve.pReadbackBuffer->pDxResource, 0, D3D11_MAP_WRITE, 0, &sub);
						memcpy(sub.pData, pResults, resolve.queryCount * sizeof(uint64_t));
						pContext->Unmap(resolve.pReadbackBuffer->pDxResource, 0);
					}
					break;
				}
				case CMD_TYPE_cmdBeginDebugMarker: break;
				case CMD_TYPE_cmdEndDebugMarker: break;
				case CMD_TYPE_cmdAddDebugMarker: break;
				case CMD_TYPE_cmdUpdateBuffer:
				{
					const UpdateBufferCmd&   update = cmd.mUpdateBufferCmd;
					D3D11_MAPPED_SUBRESOURCE sub = {};
					pContext->Map(update.pSrcBuffer->pDxResource, 0, D3D11_MAP_READ, 0, &sub);
					pContext->UpdateSubresource(
						update.pBuffer->pDxResource, 0, NULL, ((uint8_t*)sub.pData) + update.srcOffset, sub.RowPitch, sub.DepthPitch);
					pContext->Unmap(update.pSrcBuffer->pDxResource, 0);
					break;
				}
				case CMD_TYPE_cmdUpdateSubresources:
				{
					const UpdateSubresourcesCmd& update = cmd.mUpdateSubresourcesCmd;
					for (uint32_t i = 0; i < update.numSubresources; ++i)
					{
						const SubresourceDataDesc* pData = &update.pSubresources[i];
						pContext->UpdateSubresource(
							update.pTexture->pDxResource, i, NULL, ((uint8_t*)pData->pData), pData->mRowPitch, pData->mSlicePitch);
						conf_free(pData->pData);
					}
					conf_free(update.pSubresources);
					break;
				}
				default: break;
			}
		}
	}
}

void queuePresent(
	Queue* pQueue, SwapChain* pSwapChain, uint32_t swapChainImageIndex, uint32_t waitSemaphoreCount, Semaphore** ppWaitSemaphores)
{
	pSwapChain->pDxSwapChain->Present(pSwapChain->mDxSyncInterval, 0);
}

void getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus) { *pFenceStatus = FENCE_STATUS_COMPLETE; }

void waitForFences(Queue* pQueue, uint32_t fenceCount, Fence** ppFences, bool signal) {}

void toggleVSync(Renderer* pRenderer, SwapChain** ppSwapChain)
{
	// Initial vsync value is passed in with the desc when client creates a swapchain.
	ASSERT(*ppSwapChain);
	(*ppSwapChain)->mDesc.mEnableVsync = !(*ppSwapChain)->mDesc.mEnableVsync;

	//toggle vsync present flag (this can go up to 4 but we don't need to refresh on nth vertical sync)
	(*ppSwapChain)->mDxSyncInterval = ((*ppSwapChain)->mDxSyncInterval + 1) % 2;
}
/************************************************************************/
// Utility functions
/************************************************************************/
bool isImageFormatSupported(ImageFormat::Enum format)
{
	//verifies that given image format is valid
	return gFormatTranslator[format] != DXGI_FORMAT_UNKNOWN;
}

ImageFormat::Enum getRecommendedSwapchainFormat(bool hintHDR) { return ImageFormat::RGBA8; }
/************************************************************************/
// Indirect Draw functions
/************************************************************************/
void addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature) {}

void removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature) {}

void cmdExecuteIndirect(
	Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
	Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
	ASSERT(pCmd);
	ASSERT(pIndirectBuffer);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdExecuteIndirect;
	cmd.mExecuteIndirectCmd.pCommandSignature = pCommandSignature;
	cmd.mExecuteIndirectCmd.maxCommandCount = maxCommandCount;
	cmd.mExecuteIndirectCmd.pIndirectBuffer = pIndirectBuffer;
	cmd.mExecuteIndirectCmd.bufferOffset = bufferOffset;
	cmd.mExecuteIndirectCmd.pCounterBuffer = pCounterBuffer;
	cmd.mExecuteIndirectCmd.counterBufferOffset = counterBufferOffset;
	cachedCmdsIter->second.push_back(cmd);
}
/************************************************************************/
// GPU Query Implementation
/************************************************************************/
D3D11_QUERY util_to_dx_query(QueryType type)
{
	switch (type)
	{
		case QUERY_TYPE_TIMESTAMP: return D3D11_QUERY_TIMESTAMP;
		case QUERY_TYPE_PIPELINE_STATISTICS: return D3D11_QUERY_PIPELINE_STATISTICS;
		case QUERY_TYPE_OCCLUSION: return D3D11_QUERY_OCCLUSION;
		default: ASSERT(false && "Invalid query type"); return D3D11_QUERY(-1);
	}
}

void getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
	ID3D11DeviceContext* pContext = pQueue->pRenderer->pDxContext;
	ID3D11Query*         pDisjointQuery = NULL;
	D3D11_QUERY_DESC     desc = {};
	desc.MiscFlags = 0;
	desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	pQueue->pRenderer->pDxDevice->CreateQuery(&desc, &pDisjointQuery);

	pContext->Begin(pDisjointQuery);
	pContext->End(pDisjointQuery);
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT data = {};
	while (pContext->GetData(pDisjointQuery, &data, sizeof(data), 0) != S_OK)
		Thread::Sleep(0);
	*pFrequency = (double)data.Frequency;

	SAFE_RELEASE(pDisjointQuery);
}

void addQueryHeap(Renderer* pRenderer, const QueryHeapDesc* pDesc, QueryHeap** ppQueryHeap)
{
	QueryHeap* pQueryHeap = (QueryHeap*)conf_calloc(1, sizeof(*pQueryHeap));
	pQueryHeap->mDesc = *pDesc;

	pQueryHeap->ppDxQueries = (ID3D11Query**)conf_calloc(pDesc->mQueryCount, sizeof(ID3D11Query*));

	D3D11_QUERY_DESC desc = {};
	desc.MiscFlags = 0;
	desc.Query = util_to_dx_query(pDesc->mType);

	for (uint32_t i = 0; i < pDesc->mQueryCount; ++i)
	{
		pRenderer->pDxDevice->CreateQuery(&desc, &pQueryHeap->ppDxQueries[i]);
	}

	*ppQueryHeap = pQueryHeap;
}

void removeQueryHeap(Renderer* pRenderer, QueryHeap* pQueryHeap)
{
	UNREF_PARAM(pRenderer);
	for (uint32_t i = 0; i < pQueryHeap->mDesc.mQueryCount; ++i)
	{
		SAFE_RELEASE(pQueryHeap->ppDxQueries[i]);
	}
	SAFE_FREE(pQueryHeap->ppDxQueries);
	SAFE_FREE(pQueryHeap);
}

void cmdBeginQuery(Cmd* pCmd, QueryHeap* pQueryHeap, QueryDesc* pQuery)
{
	ASSERT(pCmd);
	ASSERT(pQuery);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdBeginQuery;
	cmd.mBeginQueryCmd.pQueryHeap = pQueryHeap;
	cmd.mBeginQueryCmd.mQuery = *pQuery;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdEndQuery(Cmd* pCmd, QueryHeap* pQueryHeap, QueryDesc* pQuery)
{
	ASSERT(pCmd);
	ASSERT(pQuery);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdEndQuery;
	cmd.mEndQueryCmd.pQueryHeap = pQueryHeap;
	cmd.mEndQueryCmd.mQuery = *pQuery;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdResolveQuery(Cmd* pCmd, QueryHeap* pQueryHeap, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
{
	ASSERT(pCmd);
	ASSERT(pReadbackBuffer);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdResolveQuery;
	cmd.mResolveQueryCmd.pQueryHeap = pQueryHeap;
	cmd.mResolveQueryCmd.pReadbackBuffer = pReadbackBuffer;
	cmd.mResolveQueryCmd.startQuery = startQuery;
	cmd.mResolveQueryCmd.queryCount = queryCount;
	cachedCmdsIter->second.push_back(cmd);
}
/************************************************************************/
// Memory Stats Implementation
/************************************************************************/
void calculateMemoryStats(Renderer* pRenderer, char** stats) {}

void freeMemoryStats(Renderer* pRenderer, char* stats) {}
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	ASSERT(pCmd);
	ASSERT(pName);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdBeginDebugMarker;
	cmd.mBeginDebugMarkerCmd.r = r;
	cmd.mBeginDebugMarkerCmd.g = g;
	cmd.mBeginDebugMarkerCmd.b = b;
	cmd.mBeginDebugMarkerCmd.pName = pName;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdEndDebugMarker(Cmd* pCmd)
{
	ASSERT(pCmd);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdEndDebugMarker;
	cachedCmdsIter->second.push_back(cmd);
}

void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	ASSERT(pCmd);
	ASSERT(pName);

	// Ensure beingCmd was actually called
	CachedCmds::iterator cachedCmdsIter = gCachedCmds.find(pCmd);
	ASSERT(cachedCmdsIter != gCachedCmds.end());
	if (cachedCmdsIter == gCachedCmds.end())
	{
		LOGERRORF("beginCmd was never called for that specific Cmd buffer!");
		return;
	}

	DECLARE_ZERO(CachedCmd, cmd);
	cmd.pCmd = pCmd;
	cmd.sType = CMD_TYPE_cmdAddDebugMarker;
	cmd.mAddDebugMarkerCmd.r = r;
	cmd.mAddDebugMarkerCmd.g = g;
	cmd.mAddDebugMarkerCmd.b = b;
	cmd.mAddDebugMarkerCmd.pName = pName;
	cachedCmdsIter->second.push_back(cmd);
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName) {}

void setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName) {}
/************************************************************************/
/************************************************************************/
#endif    // RENDERER_IMPLEMENTATION
#endif
#if defined(__cplusplus) && defined(ENABLE_RENDERER_RUNTIME_SWITCH)
}
#endif

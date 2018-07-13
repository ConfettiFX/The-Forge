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



/* Lightweight D3D11 Fallback layer.
 * This implementation retrofits the new low level interface to D3D11.
 * TODO: explain how GPU resource dependencies are handled...
 */

#ifdef DIRECT3D11
#define RENDERER_IMPLEMENTATION
#define IID_ARGS IID_PPV_ARGS


#include "../../ThirdParty/OpenSource/TinySTL/string.h"
#include "../../ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../../OS/Interfaces/ILogManager.h"
#include "../IRenderer.h"
#include "../../OS/Core/RingBuffer.h"
#include "../../ThirdParty/OpenSource/TinySTL/hash.h"
#include "../../ThirdParty/OpenSource/winpixeventruntime/Include/WinPixEventRuntime/pix3.h"
#include "../../OS/Core/GPUConfig.h"

#if ! defined(_WIN32)
#error "Windows is needed!"
#endif

#if ! defined(__cplusplus)
#error "D3D11 requires C++! Sorry!"
#endif

// Pull in minimal Windows headers
#if ! defined(NOMINMAX)
#define NOMINMAX
#endif
#if ! defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

// Prefer Higher Performance GPU on switchable GPU systems
extern "C"
{
	__declspec(dllexport) DWORD	NvOptimusEnablement = 1;
	__declspec(dllexport) int	AmdPowerXpressRequestHighPerformance = 1;
}

//#include "Direct3D11MemoryAllocator.h"
#include "../../OS/Interfaces/IMemoryManager.h"

extern void d3d11_createShaderReflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection);

	// Array used to translate sampling filter
	const D3D11_FILTER gDX11FilterTranslator[] =
	{
		D3D11_FILTER_MIN_MAG_MIP_POINT,
		D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
		D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
		D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		D3D11_FILTER_ANISOTROPIC,
		D3D11_FILTER_ANISOTROPIC
	};

	// Array used to translate compasion sampling filter
	const D3D11_FILTER gDX11ComparisonFilterTranslator[] =
	{
		D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT,
		D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
		D3D11_FILTER_COMPARISON_ANISOTROPIC,
		D3D11_FILTER_COMPARISON_ANISOTROPIC
	};

	D3D11_BLEND_OP gDx11BlendOpTranslator[BlendMode::MAX_BLEND_MODES] =
	{
		D3D11_BLEND_OP_ADD,
		D3D11_BLEND_OP_SUBTRACT,
		D3D11_BLEND_OP_REV_SUBTRACT,
		D3D11_BLEND_OP_MIN,
		D3D11_BLEND_OP_MAX,
	};

	D3D11_BLEND gDx11BlendConstantTranslator[BlendConstant::MAX_BLEND_CONSTANTS] =
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

	D3D11_COMPARISON_FUNC gDx11ComparisonFuncTranslator[CompareMode::MAX_COMPARE_MODES] =
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

	D3D11_STENCIL_OP gDx11StencilOpTranslator[StencilOp::MAX_STENCIL_OPS] =
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

	D3D11_CULL_MODE gDx11CullModeTranslator[MAX_CULL_MODES] =
	{
		D3D11_CULL_NONE,
		D3D11_CULL_BACK,
		D3D11_CULL_FRONT,
	};

	D3D11_FILL_MODE gDx11FillModeTranslator[MAX_FILL_MODES] =
	{
		D3D11_FILL_SOLID,
		D3D11_FILL_WIREFRAME,
	};

	const DXGI_FORMAT gDX11FormatTranslatorTypeless[] = {
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
	const DXGI_FORMAT gDX11FormatTranslator[] = {
		DXGI_FORMAT_UNKNOWN,							// ImageFormat::NONE
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
	
	// =================================================================================================
	// IMPLEMENTATION
	// =================================================================================================

#if defined(RENDERER_IMPLEMENTATION)

#include <d3dcompiler.h>
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define SAFE_FREE(p_var)	\
    if (p_var) {			\
       conf_free(p_var);	\
    }

#if defined(__cplusplus)  
#define DECLARE_ZERO(type, var)		\
            type var = {};
#else
#define DECLARE_ZERO(type, var)		\
            type var = {0};                        
#endif

#define SAFE_RELEASE(p_var)			\
    if (p_var) {					\
       p_var->Release();			\
       p_var = NULL;				\
    }

	// Internal utility functions (may become external one day)
	//uint64_t						util_dx_determine_storage_counter_offset(uint64_t buffer_size);
	//DXGI_FORMAT						util_to_dx_image_format_typeless(ImageFormat::Enum format);
	//DXGI_FORMAT						util_to_dx_uav_format(DXGI_FORMAT defaultFormat);
	DXGI_FORMAT						util_to_dx_dsv_format(DXGI_FORMAT defaultFormat);
	//DXGI_FORMAT						util_to_dx_srv_format(DXGI_FORMAT defaultFormat);
	//DXGI_FORMAT						util_to_dx_stencil_format(DXGI_FORMAT defaultFormat);
	DXGI_FORMAT						util_to_dx_image_format(ImageFormat::Enum format, bool srgb);
	//DXGI_FORMAT						util_to_dx_swapchain_format(ImageFormat::Enum format);
	//D3D12_SHADER_VISIBILITY			util_to_dx_shader_visibility(ShaderStage stages);
	//D3D12_DESCRIPTOR_RANGE_TYPE		util_to_dx_descriptor_range(DescriptorType type);
	//D3D12_RESOURCE_STATES			util_to_dx_resource_state(ResourceState state);
	D3D11_FILTER					util_to_dx_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool comparisonFilterEnabled);
	D3D11_TEXTURE_ADDRESS_MODE		util_to_dx_texture_address_mode(AddressMode addressMode);
	//D3D12_PRIMITIVE_TOPOLOGY_TYPE	util_to_dx_primitive_topology_type(PrimitiveTopology topology);

	inline bool hasMipmaps(const FilterType filter) { return (filter >= FILTER_BILINEAR); }
	inline bool hasAniso(const FilterType filter) { return (filter >= FILTER_BILINEAR_ANISO); }

	D3D11_FILTER util_to_dx_filter(FilterType minFilter, FilterType magFilter, MipMapMode  mipMapMode, bool comparisonFilterEnabled)
	{
		UNREF_PARAM(magFilter);
		UNREF_PARAM(mipMapMode);
		if (!comparisonFilterEnabled)
		{
			return gDX11FilterTranslator[minFilter];
		}
		else
		{
			return gDX11ComparisonFilterTranslator[minFilter];
		}
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

	DXGI_FORMAT util_to_dx_image_format_typeless(ImageFormat::Enum format)
	{
		DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;
		if (format >= sizeof(gDX11FormatTranslatorTypeless) / sizeof(DXGI_FORMAT))
		{
			LOGERRORF("Failed to Map from ConfettilFileFromat to DXGI format, should add map method in gDX12FormatTranslator");
		}
		else
		{
			result = gDX11FormatTranslatorTypeless[format];
		}

		return result;
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
	
	/************************************************************************/
	// Gloabals
	/************************************************************************/
	static const uint32_t		gDescriptorTableDWORDS = 1;
	static const uint32_t		gRootDescriptorDWORDS = 2;

	static volatile uint64_t	gBufferIds	= 0;
	static volatile uint64_t	gTextureIds	= 0;
	static volatile uint64_t	gSamplerIds	= 0;

	static uint32_t				gMaxRootConstantsPerRootParam = 4U;
	
	/************************************************************************/
	// Logging functions
	/************************************************************************/
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
			
	/************************************************************************/
	// Functions not exposed in IRenderer but still need to be exported in dll
	/************************************************************************/
	API_INTERFACE void CALLTYPE addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
	API_INTERFACE void CALLTYPE removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
	API_INTERFACE void CALLTYPE addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture);
	API_INTERFACE void CALLTYPE removeTexture(Renderer* pRenderer, Texture* pTexture);
	API_INTERFACE void CALLTYPE mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
	API_INTERFACE void CALLTYPE unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
	API_INTERFACE void CALLTYPE cmdUpdateBuffer(Cmd* pCmd, uint64_t srcOffset, uint64_t dstOffset, uint64_t size, Buffer* pSrcBuffer, Buffer* pBuffer);
	API_INTERFACE void CALLTYPE cmdUpdateSubresources(Cmd* pCmd, uint32_t startSubresource, uint32_t numSubresources, SubresourceDataDesc* pSubresources, Buffer* pIntermediate, uint64_t intermediateOffset, Texture* pTexture);
	API_INTERFACE void CALLTYPE compileShader(Renderer* pRenderer, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code, uint32_t macroCount, ShaderMacro* pMacros, void*(*allocator)(size_t a), uint32_t* pByteCodeSize, char** ppByteCode);
	API_INTERFACE const RendererShaderDefinesDesc CALLTYPE get_renderer_shaderdefines(Renderer* pRenderer);

	void cmdUpdateBuffer(Cmd* pCmd, uint64_t srcOffset, uint64_t dstOffset, uint64_t size, Buffer* pSrcBuffer, Buffer* pBuffer)
	{
	}


	void cmdUpdateSubresources(Cmd* pCmd, uint32_t startSubresource, uint32_t numSubresources, SubresourceDataDesc* pSubresources, Buffer* pIntermediate, uint64_t intermediateOffset, Texture* pTexture)
	{
	}

	const RendererShaderDefinesDesc get_renderer_shaderdefines(Renderer* pRenderer)
	{
		return RendererShaderDefinesDesc();
	}



	/************************************************************************/
	// Internal init functions
	/************************************************************************/
	static void AddDevice(Renderer* pRenderer)
	{		
		const uint32_t NUM_SUPPORTED_FEATURE_LEVELS = 2;
		D3D_FEATURE_LEVEL feature_levels[NUM_SUPPORTED_FEATURE_LEVELS] =
		{
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
		};

		HRESULT hr = 0;
			
		IDXGIAdapter1 *dxgiAdapter = NULL;

		if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&pRenderer->pDXGIFactory)))
		{
			LOGERROR("Could not create DXGI factory.");
			return;
		}
		ASSERT(pRenderer->pDXGIFactory);

		// Enumerate all adapters
		typedef struct GpuDesc
		{
			IDXGIAdapter1* pGpu = NULL;
			D3D_FEATURE_LEVEL mMaxSupportedFeatureLevel = (D3D_FEATURE_LEVEL)0;
			SIZE_T mDedicatedVideoMemory = 0;
			char mVendorId[MAX_GPU_VENDOR_STRING_LENGTH];
			char mDeviceId[MAX_GPU_VENDOR_STRING_LENGTH];
			char mRevisionId[MAX_GPU_VENDOR_STRING_LENGTH];
			char mName[MAX_GPU_VENDOR_STRING_LENGTH];
			GPUPresetLevel mPreset;
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
				if (SUCCEEDED(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, feature_levels, 2, D3D11_SDK_VERSION, NULL, &featLevelOut, NULL)))
				{
					hr = adapter->QueryInterface(IID_ARGS(&gpuDesc[pRenderer->mNumOfGPUs].pGpu));
					if (SUCCEEDED(hr))
					{
						gpuDesc[pRenderer->mNumOfGPUs].mMaxSupportedFeatureLevel = featLevelOut;
						gpuDesc[pRenderer->mNumOfGPUs].mDedicatedVideoMemory = desc.DedicatedVideoMemory;

						//save vendor and model Id as string
						//char hexChar[10];
						//convert deviceId and assign it
						sprintf(gpuDesc[pRenderer->mNumOfGPUs].mDeviceId, "%#x\0", desc.DeviceId);
						//convert modelId and assign it
						sprintf(gpuDesc[pRenderer->mNumOfGPUs].mVendorId, "%#x\0", desc.VendorId);
						//convert Revision Id
						sprintf(gpuDesc[pRenderer->mNumOfGPUs].mRevisionId, "%#x\0", desc.Revision);

						//get preset for current gpu description
						gpuDesc[pRenderer->mNumOfGPUs].mPreset = getGPUPresetLevel(gpuDesc[pRenderer->mNumOfGPUs].mVendorId, gpuDesc[pRenderer->mNumOfGPUs].mDeviceId, gpuDesc[pRenderer->mNumOfGPUs].mRevisionId);

						//save gpu name (Some situtations this can show description instead of name)
						//char sName[MAX_PATH];
						wcstombs(gpuDesc[pRenderer->mNumOfGPUs].mName, desc.Description, MAX_PATH);
						++pRenderer->mNumOfGPUs;
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
			pRenderer->mGpuSettings[i].mUniformBufferAlignment = 0U; // no such thing for dx11
			pRenderer->mGpuSettings[i].mMultiDrawIndirect = false; // no such thing
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
			pRenderer->mGpuSettings[i].mMaxRootSignatureDWORDS = 0U; // no such thing			
		}

		uint32_t gpuIndex = 0;
#if defined(ACTIVE_TESTING_GPU) && !defined(_DURANGO) && defined(AUTOMATED_TESTING)
		//Read active GPU if AUTOMATED_TESTING and ACTIVE_TESTING_GPU are defined
		GPUVendorPreset activeTestingPreset;
		bool activeTestingGpu = getActiveGpuConfig(activeTestingPreset);
		if (activeTestingGpu) 
		{
			for (uint32_t i = 0; i < pRenderer->mNumOfGPUs; i++) {
				if (pRenderer->mGpuSettings[i].mGpuVendorPreset.mVendorId == activeTestingPreset.mVendorId
					&& pRenderer->mGpuSettings[i].mGpuVendorPreset.mModelId == activeTestingPreset.mModelId)
				{
					//if revision ID is valid then use it to select active GPU
					if (pRenderer->mGpuSettings[i].mGpuVendorPreset.mRevisionId != "0x00" && pRenderer->mGpuSettings[i].mGpuVendorPreset.mRevisionId != activeTestingPreset.mRevisionId)
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
		hr = D3D11CreateDevice(pRenderer->pDxActiveGPU,
			D3D_DRIVER_TYPE_UNKNOWN,
			(HMODULE)0,
			deviceFlags,
			feature_levels,
			2,
			D3D11_SDK_VERSION,
			&pRenderer->pDxDevice,
			&featLevelOut,			// max feature level
			&pRenderer->pDxContext);
		ASSERT(SUCCEEDED(hr));
		if (FAILED(hr))
			LOGERROR("Failed to create D3D11 device and context.");
	}

	static void RemoveDevice(Renderer* pRenderer)
	{
		SAFE_RELEASE(pRenderer->pDXGIFactory);

		for (uint32_t i = 0; i < pRenderer->mNumOfGPUs; ++i) {
			SAFE_RELEASE(pRenderer->pDxGPUs[i]);
		}


		SAFE_RELEASE(pRenderer->pDxContext);
		SAFE_RELEASE(pRenderer->pDxDevice);
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

		// TODO:
		// create_default_resources(pRenderer);

		// Renderer is good! Assign it to result!
		*(ppRenderer) = pRenderer;
	}

	void removeRenderer(Renderer* pRenderer)
	{
		ASSERT(pRenderer);

		SAFE_FREE(pRenderer->pName);

		// TODO:
		// destroy_default_resources(pRenderer);
				
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
		Queue * pQueue = (Queue*)conf_calloc(1, sizeof(*pQueue));
		ASSERT(pQueue != NULL);

		// Provided description for queue creation.
		// Note these don't really mean much w/ DX11 but we can use it for debugging
		// what the client is intending to do.
		pQueue->mQueueDesc = *pQDesc;
		
		String queueType = "DUMMY QUEUE FOR DX11 BACKEND";
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
	}

	void removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
	{
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
		for (uint32_t i = 0; i < cmdCount; ++i) {
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
		for (uint32_t i = 0; i < cmdCount; ++i) {
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
		// Assert that render target is used as either color or depth attachment
		ASSERT(((pDesc->mUsage & RENDER_TARGET_USAGE_COLOR) || (pDesc->mUsage & RENDER_TARGET_USAGE_DEPTH_STENCIL)));
		// Assert that render target is not used as both color and depth attachment
		ASSERT(!((pDesc->mUsage & RENDER_TARGET_USAGE_COLOR) && (pDesc->mUsage & RENDER_TARGET_USAGE_DEPTH_STENCIL)));
		ASSERT(!((pDesc->mUsage & RENDER_TARGET_USAGE_DEPTH_STENCIL) && (pDesc->mUsage & RENDER_TARGET_USAGE_UNORDERED_ACCESS)) && "Cannot use depth stencil as UAV");

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
		textureDesc.pNativeHandle = pDesc->pNativeHandle;
		textureDesc.mSrgb = pDesc->mSrgb;
		textureDesc.pDebugName = pDesc->pDebugName;
		textureDesc.mNodeIndex = pDesc->mNodeIndex;
		textureDesc.pSharedNodeIndices = pDesc->pSharedNodeIndices;
		textureDesc.mSharedNodeIndexCount = pDesc->mSharedNodeIndexCount;

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
		ASSERT(pRenderTarget->pTexture);

		RenderTargetType type = pRenderTarget->mDesc.mType;
		switch (type)
		{
		case RENDER_TARGET_TYPE_1D:
			if (pRenderTarget->mDesc.mArraySize > 1)
			{
				pRenderTarget->mDxRtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
				pRenderTarget->mDxDsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
				// RTV
				pRenderTarget->mDxRtvDesc.Texture1DArray.ArraySize = pRenderTarget->mDesc.mArraySize;
				pRenderTarget->mDxRtvDesc.Texture1DArray.FirstArraySlice = 0;
				pRenderTarget->mDxRtvDesc.Texture1DArray.MipSlice = 0;
				// DSV
				pRenderTarget->mDxDsvDesc.Texture1DArray.ArraySize = pRenderTarget->mDesc.mArraySize;
				pRenderTarget->mDxDsvDesc.Texture1DArray.FirstArraySlice = 0;
				pRenderTarget->mDxDsvDesc.Texture1DArray.MipSlice = 0;
			}
			else
			{
				pRenderTarget->mDxRtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
				pRenderTarget->mDxDsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
				// RTV
				pRenderTarget->mDxRtvDesc.Texture1D.MipSlice = 0;
				// DSV
				pRenderTarget->mDxDsvDesc.Texture1D.MipSlice = 0;
			}
			break;
		case RENDER_TARGET_TYPE_2D:
			if (pRenderTarget->mDesc.mArraySize > 1)
			{
				if (pRenderTarget->mDesc.mSampleCount > SAMPLE_COUNT_1)
				{
					pRenderTarget->mDxRtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
					pRenderTarget->mDxDsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
					// RTV
					pRenderTarget->mDxRtvDesc.Texture2DMSArray.ArraySize = pRenderTarget->mDesc.mArraySize;
					pRenderTarget->mDxRtvDesc.Texture2DMSArray.FirstArraySlice = 0;
					// DSV
					pRenderTarget->mDxDsvDesc.Texture2DMSArray.ArraySize = pRenderTarget->mDesc.mArraySize;
					pRenderTarget->mDxDsvDesc.Texture2DMSArray.FirstArraySlice = 0;
				}
				else
				{
					pRenderTarget->mDxRtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
					pRenderTarget->mDxDsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
					// RTV
					pRenderTarget->mDxRtvDesc.Texture2DArray.ArraySize = pRenderTarget->mDesc.mArraySize;
					pRenderTarget->mDxRtvDesc.Texture2DArray.FirstArraySlice = 0;
					pRenderTarget->mDxRtvDesc.Texture2DArray.MipSlice = 0;					
					// DSV
					pRenderTarget->mDxDsvDesc.Texture2DArray.ArraySize = pRenderTarget->mDesc.mArraySize;
					pRenderTarget->mDxDsvDesc.Texture2DArray.FirstArraySlice = 0;
					pRenderTarget->mDxDsvDesc.Texture2DArray.MipSlice = 0;
				}
			}
			else
			{
				if (pRenderTarget->mDesc.mSampleCount > SAMPLE_COUNT_1)
				{
					pRenderTarget->mDxRtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
					pRenderTarget->mDxDsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
				}
				else
				{
					pRenderTarget->mDxRtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
					pRenderTarget->mDxDsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
					// RTV
					pRenderTarget->mDxRtvDesc.Texture2D.MipSlice = 0;					
					// DSV
					pRenderTarget->mDxDsvDesc.Texture2D.MipSlice = 0;
				}
			}
			break;
		case RENDER_TARGET_TYPE_3D:
			pRenderTarget->mDxRtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
			// Cannot create a 3D depth stencil
			pRenderTarget->mDxRtvDesc.Texture3D.MipSlice = 0;
			pRenderTarget->mDxRtvDesc.Texture3D.WSize = ~0u;
			break;
		default:
			break;
		}

		if (pRenderTarget->mDesc.mUsage & RENDER_TARGET_USAGE_COLOR)
		{
			ASSERT(pRenderTarget->mDxRtvDesc.ViewDimension != D3D11_RTV_DIMENSION_UNKNOWN);
			pRenderTarget->mDxRtvDesc.Format = dxFormat;
			
			if (FAILED(pRenderer->pDxDevice->CreateRenderTargetView(pRenderTarget->pTexture->pDxResource, &pRenderTarget->mDxRtvDesc, &pRenderTarget->pDxRtvResource)))
				LOGERROR("Failed to create RTV.");
		}
		else if (pRenderTarget->mDesc.mUsage & RENDER_TARGET_USAGE_DEPTH_STENCIL)
		{
			ASSERT(pRenderTarget->mDxDsvDesc.ViewDimension != D3D11_DSV_DIMENSION_UNKNOWN);
			pRenderTarget->mDxDsvDesc.Format = util_to_dx_dsv_format(dxFormat);

			if (FAILED(pRenderer->pDxDevice->CreateDepthStencilView(pRenderTarget->pTexture->pDxResource, &pRenderTarget->mDxDsvDesc, &pRenderTarget->pDxDsvResource)))
				LOGERROR("Failed to create DSV.");			
		}

		*ppRenderTarget = pRenderTarget;
	}

	void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
	{
		removeTexture(pRenderer, pRenderTarget->pTexture);

		if (pRenderTarget->mDesc.mUsage == RENDER_TARGET_USAGE_COLOR)
		{
			SAFE_RELEASE(pRenderTarget->pDxRtvResource);
		}
		else if (pRenderTarget->mDesc.mUsage == RENDER_TARGET_USAGE_DEPTH_STENCIL)
		{
			SAFE_RELEASE(pRenderTarget->pDxDsvResource);
		}

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
		desc.Filter = util_to_dx_filter(pDesc->mMinFilter, pDesc->mMagFilter, pDesc->mMipMapMode, (pDesc->mCompareFunc != CMP_NEVER ? true : false));
		desc.AddressU = util_to_dx_texture_address_mode(pDesc->mAddressU);
		desc.AddressV = util_to_dx_texture_address_mode(pDesc->mAddressV);
		desc.AddressW = util_to_dx_texture_address_mode(pDesc->mAddressW);
		desc.MipLODBias = pDesc->mMipLosBias;
		desc.MaxAnisotropy = (hasAniso(pDesc->mMinFilter) || hasAniso(pDesc->mMagFilter)) ? (UINT)pDesc->mMaxAnisotropy : 1U;
		desc.ComparisonFunc = gDx11ComparisonFuncTranslator[pDesc->mCompareFunc];
		desc.BorderColor[0] = 0.0f;
		desc.BorderColor[1] = 0.0f;
		desc.BorderColor[2] = 0.0f;
		desc.BorderColor[3] = 0.0f;
		desc.MinLOD = 0.0f;
		desc.MaxLOD = (hasMipmaps(pDesc->mMinFilter) || hasMipmaps(pDesc->mMagFilter)) ? D3D11_FLOAT32_MAX : 0.0f;
							
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
	void compileShader(Renderer* pRenderer, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code, uint32_t macroCount, ShaderMacro* pMacros, void*(*allocator)(size_t a), uint32_t* pByteCodeSize, char** ppByteCode)
	{
	}

	void addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
	{
	}

	void removeShader(Renderer* pRenderer, Shader* pShaderProgram)
	{
	}




	void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
	{
	}

	void removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
	{
	}





	void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
	{
	}

	void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
	{
	}

	void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
	{
		
	}

	void removeTexture(Renderer* pRenderer, Texture* pTexture)
	{
		ASSERT(pRenderer);
		ASSERT(pTexture);

		SAFE_RELEASE(pTexture->pDxSrvResource);
		SAFE_RELEASE(pTexture->pDxUavResource);
		SAFE_RELEASE(pTexture->pDxResource);

		SAFE_FREE(pTexture);
	}
	
	/************************************************************************/
	// Pipeline Functions
	/************************************************************************/
	void addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
	{
	}

	void removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
	{
	}

	void addPipeline(Renderer* pRenderer, const GraphicsPipelineDesc* pDesc, Pipeline** ppPipeline)
	{
	}

	void addComputePipeline(Renderer* pRenderer, const ComputePipelineDesc* pDesc, Pipeline** ppPipeline)
	{
	}

	void removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
	{
	}

	/************************************************************************/
	// Pipeline State Functions
	/************************************************************************/
	void addBlendState(Renderer* pRenderer, const BlendStateDesc* pDesc, BlendState** ppBlendState)
	{
		UNREF_PARAM(pRenderer);

		ASSERT(pDesc->mSrcFactor < BlendConstant::MAX_BLEND_CONSTANTS);
		ASSERT(pDesc->mDstFactor < BlendConstant::MAX_BLEND_CONSTANTS);
		ASSERT(pDesc->mSrcAlphaFactor < BlendConstant::MAX_BLEND_CONSTANTS);
		ASSERT(pDesc->mDstAlphaFactor < BlendConstant::MAX_BLEND_CONSTANTS);
		ASSERT(pDesc->mBlendMode < BlendMode::MAX_BLEND_MODES);
		ASSERT(pDesc->mBlendAlphaMode < BlendMode::MAX_BLEND_MODES);

		BOOL blendEnable = (gDx11BlendConstantTranslator[pDesc->mSrcFactor] != D3D11_BLEND_ONE || gDx11BlendConstantTranslator[pDesc->mDstFactor] != D3D11_BLEND_ZERO ||
			gDx11BlendConstantTranslator[pDesc->mSrcAlphaFactor] != D3D11_BLEND_ONE || gDx11BlendConstantTranslator[pDesc->mDstAlphaFactor] != D3D11_BLEND_ZERO);

		BlendState* pBlendState = (BlendState*)conf_calloc(1, sizeof(*pBlendState));
				
		D3D11_BLEND_DESC desc;

		desc.AlphaToCoverageEnable = (BOOL)pDesc->mAlphaToCoverage;
		desc.IndependentBlendEnable = TRUE;
		for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; i++)
		{
			desc.RenderTarget[i].RenderTargetWriteMask = (UINT8)pDesc->mMask;
			if (pDesc->mRenderTargetMask & (1 << i))
			{
				desc.RenderTarget[i].BlendEnable = blendEnable;
				desc.RenderTarget[i].BlendOp = gDx11BlendOpTranslator[pDesc->mBlendMode];
				desc.RenderTarget[i].SrcBlend = gDx11BlendConstantTranslator[pDesc->mSrcFactor];
				desc.RenderTarget[i].DestBlend = gDx11BlendConstantTranslator[pDesc->mDstFactor];
				desc.RenderTarget[i].BlendOpAlpha = gDx11BlendOpTranslator[pDesc->mBlendAlphaMode];
				desc.RenderTarget[i].SrcBlendAlpha = gDx11BlendConstantTranslator[pDesc->mSrcAlphaFactor];
				desc.RenderTarget[i].DestBlendAlpha = gDx11BlendConstantTranslator[pDesc->mDstAlphaFactor];
			}
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
		desc.DepthFunc = gDx11ComparisonFuncTranslator[pDesc->mDepthFunc];
		desc.StencilEnable = (BOOL)pDesc->mStencilTest;
		desc.StencilReadMask = pDesc->mStencilReadMask;
		desc.StencilWriteMask = pDesc->mStencilWriteMask;
		desc.BackFace.StencilFunc = gDx11ComparisonFuncTranslator[pDesc->mStencilBackFunc];
		desc.FrontFace.StencilFunc = gDx11ComparisonFuncTranslator[pDesc->mStencilFrontFunc];
		desc.BackFace.StencilDepthFailOp = gDx11StencilOpTranslator[pDesc->mDepthBackFail];
		desc.FrontFace.StencilDepthFailOp = gDx11StencilOpTranslator[pDesc->mDepthFrontFail];
		desc.BackFace.StencilFailOp = gDx11StencilOpTranslator[pDesc->mStencilBackFail];
		desc.FrontFace.StencilFailOp = gDx11StencilOpTranslator[pDesc->mStencilFrontFail];
		desc.BackFace.StencilPassOp = gDx11StencilOpTranslator[pDesc->mStencilFrontPass];
		desc.FrontFace.StencilPassOp = gDx11StencilOpTranslator[pDesc->mStencilBackPass];

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

		RasterizerState* pRasterizerState = (RasterizerState*)conf_calloc(1, sizeof(*pRasterizerState));

		D3D11_RASTERIZER_DESC desc;
		desc.FillMode = gDx11FillModeTranslator[pDesc->mFillMode];
		desc.CullMode = gDx11CullModeTranslator[pDesc->mCullMode];
		desc.FrontCounterClockwise = TRUE;
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
	}

	void endCmd(Cmd* pCmd)
	{
	}

	void cmdBindRenderTargets(Cmd* p_cmd, uint32_t render_target_count, RenderTarget** pp_render_targets, RenderTarget* p_depth_stencil, const LoadActionsDesc* loadActions, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice, uint32_t depthMipSlice)
	{
	}

	void cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
	{
	}

	void cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
	}

	void cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
	{
	}

	void cmdBindDescriptors(Cmd* pCmd, RootSignature* pRootSignature, uint32_t numDescriptors, DescriptorData* pDescParams)
	{
	}

	void cmdBindIndexBuffer(Cmd* p_cmd, Buffer* p_buffer, uint64_t offset)
	{
	}

	void cmdBindVertexBuffer(Cmd* p_cmd, uint32_t buffer_count, Buffer** pp_buffers, uint64_t* pOffsets)
	{	
	}

	void cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
	{
	}
	
	void cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount)
	{
	}

	void cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex)
	{
	}

	void cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount)
	{
	}

	void cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
	}


	/************************************************************************/
	// Transition Commands
	/************************************************************************/

	void cmdResourceBarrier(Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers, bool batch)
	{
	}

	void cmdSynchronizeResources(Cmd* pCmd, uint32_t numBuffers, Buffer** ppBuffers, uint32_t numTextures, Texture** ppTextures, bool batch)
	{
	}

	void cmdFlushBarriers(Cmd* pCmd)
	{
	}

	/************************************************************************/
	// Queue Fence Semaphore Functions
	/************************************************************************/
	void acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pSwapChainImageIndex)
	{
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
	}

	void queuePresent(Queue* pQueue, SwapChain* pSwapChain, uint32_t swapChainImageIndex, uint32_t waitSemaphoreCount, Semaphore** ppWaitSemaphores)
	{
	}

	void getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
	{
	}

	void waitForFences(Queue* pQueue, uint32_t fenceCount, Fence** ppFences, bool signal)
	{
	}

	void toggleVSync(Renderer* pRenderer, SwapChain** ppSwapChain)
	{		
		// Initial vsync value is passed in with the desc when client creates a swapchain.
		ASSERT(*ppSwapChain);
		(*ppSwapChain)->mDesc.mEnableVsync = !(*ppSwapChain)->mDesc.mEnableVsync;
	}

	/************************************************************************/
	// Utility functions
	/************************************************************************/
	bool isImageFormatSupported(ImageFormat::Enum format)
	{
		//verifies that given image format is valid
		return gDX11FormatTranslator[format] != DXGI_FORMAT_UNKNOWN;
	}

	ImageFormat::Enum getRecommendedSwapchainFormat(bool hintHDR)
	{
		return ImageFormat::RGBA8;
	}
		

	/************************************************************************/
	// Indirect Draw functions
	/************************************************************************/
	void addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
	{
	}

	void removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
	{
	}

	void cmdExecuteIndirect(Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset)
	{
	}

	/************************************************************************/
	// GPU Query Implementation
	/************************************************************************/
	void getTimestampFrequency(Queue* pQueue, double* pFrequency)
	{
	}

	void addQueryHeap(Renderer* pRenderer, const QueryHeapDesc* pDesc, QueryHeap** ppQueryHeap)
	{
	}

	void removeQueryHeap(Renderer* pRenderer, QueryHeap* pQueryHeap)
	{
	}

	void cmdBeginQuery(Cmd* pCmd, QueryHeap* pQueryHeap, QueryDesc* pQuery)
	{
	}

	void cmdEndQuery(Cmd* pCmd, QueryHeap* pQueryHeap, QueryDesc* pQuery)
	{
	}

	void cmdResolveQuery(Cmd* pCmd, QueryHeap* pQueryHeap, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
	{
	}

	/************************************************************************/
	// Memory Stats Implementation
	/************************************************************************/
	void calculateMemoryStats(Renderer* pRenderer, char** stats)
	{
	}

	void freeMemoryStats(Renderer* pRenderer, char* stats)
	{
	}

	/************************************************************************/
	// Debug Marker Implementation
	/************************************************************************/
	void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
	{
	}

	void cmdEndDebugMarker(Cmd* pCmd)
	{
	}

	void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
	{
	}

	/************************************************************************/
	// Resource Debug Naming Interface
	/************************************************************************/
	void setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
	{
	}

	void setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
	{
	}
	/************************************************************************/
	/************************************************************************/
#endif // RENDERER_IMPLEMENTATION
#endif
#if defined(__cplusplus) && defined(ENABLE_RENDERER_RUNTIME_SWITCH)
}
#endif

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

/* Lightweight D3D11 Fallback layer.
 * This implementation retrofits the new low level interface to D3D11.
 * TODO: explain how GPU resource dependencies are handled...
 */
#include "../GraphicsConfig.h"

#ifdef DIRECT3D11
#define RENDERER_IMPLEMENTATION
#define IID_ARGS IID_PPV_ARGS

#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"
#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../Interfaces/IGraphics.h"
#include "../../Utilities/RingBuffer.h"
#include "../../OS/ThirdParty/OpenSource/winpixeventruntime/Include/WinPixEventRuntime/pix3.h"
#include "../GPUConfig.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"
#include "Direct3D11CapBuilder.h"

//#include "../../ThirdParty/OpenSource/gpudetect/include/GpuDetectHelper.h"

#if !defined(_WINDOWS) && !defined(XBOX)
#error "Windows is needed!"
#endif

#if !defined(__cplusplus)
#error "D3D11 requires C++! Sorry!"
#endif

#include <Windows.h>

// Prefer Higher Performance GPU on switchable GPU systems
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 1;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

#include "../../Utilities/Interfaces/IMemory.h"

#define D3D11_REQ_CONSTANT_BUFFER_SIZE (D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16u)

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

// clang-format on

// =================================================================================================
// IMPLEMENTATION
// =================================================================================================

#if defined(RENDERER_IMPLEMENTATION)

#include <d3dcompiler.h>
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define SAFE_FREE(p_var) \
	if (p_var)           \
	{                    \
		tf_free(p_var);  \
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

#define CHECK_HRESULT_DEVICE(device, exp)                                                             \
	{                                                                                                 \
		HRESULT hres = (exp);                                                                         \
		if (!SUCCEEDED(hres))                                                                         \
		{                                                                                             \
			LOGF(eERROR, "%s: FAILED with HRESULT: %u", #exp, (uint32_t)hres);                        \
			if (hres == DXGI_ERROR_DEVICE_REMOVED)                                                    \
				LOGF(eERROR, "ERROR_DEVICE_REMOVED: %u", (uint32_t)device->GetDeviceRemovedReason()); \
			ASSERT(false);                                                                            \
		}                                                                                             \
	}

// Internal utility functions (may become external one day)
//uint64_t					  util_dx_determine_storage_counter_offset(uint64_t buffer_size);
DXGI_FORMAT util_to_dx11_uav_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT util_to_dx11_dsv_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT util_to_dx11_srv_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT util_to_dx11_stencil_format(DXGI_FORMAT defaultFormat);
DXGI_FORMAT util_to_dx11_swapchain_format(TinyImageFormat format);
D3D11_FILTER
						   util_to_dx11_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool aniso, bool comparisonFilterEnabled);
D3D11_TEXTURE_ADDRESS_MODE util_to_dx11_texture_address_mode(AddressMode addressMode);

D3D11_FILTER
	util_to_dx11_filter(FilterType minFilter, FilterType magFilter, MipMapMode mipMapMode, bool aniso, bool comparisonFilterEnabled)
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

D3D11_TEXTURE_ADDRESS_MODE util_to_dx11_texture_address_mode(AddressMode addressMode)
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

DXGI_FORMAT util_to_dx11_uav_format(DXGI_FORMAT defaultFormat)
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

#if defined(ENABLE_GRAPHICS_DEBUG)
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		case DXGI_FORMAT_D16_UNORM: LOGF(eERROR, "Requested a UAV format for a depth stencil format");
#endif

		default: return defaultFormat;
	}
}

DXGI_FORMAT util_to_dx11_dsv_format(DXGI_FORMAT defaultFormat)
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

DXGI_FORMAT util_to_dx11_srv_format(DXGI_FORMAT defaultFormat)
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

DXGI_FORMAT util_to_dx11_stencil_format(DXGI_FORMAT defaultFormat)
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

DXGI_FORMAT util_to_dx11_swapchain_format(TinyImageFormat format)
{
	DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;

	// FLIP_DISCARD and FLIP_SEQEUNTIAL swapchain buffers only support these formats
	switch (format)
	{
		case TinyImageFormat_R16G16B16A16_SFLOAT: result = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
		case TinyImageFormat_B8G8R8A8_UNORM:
		case TinyImageFormat_B8G8R8A8_SRGB: result = DXGI_FORMAT_B8G8R8A8_UNORM; break;
		case TinyImageFormat_R8G8B8A8_UNORM:
		case TinyImageFormat_R8G8B8A8_SRGB: result = DXGI_FORMAT_R8G8B8A8_UNORM; break;
		case TinyImageFormat_R10G10B10A2_UNORM: result = DXGI_FORMAT_R10G10B10A2_UNORM; break;
		default: break;
	}

	if (result == DXGI_FORMAT_UNKNOWN)
	{
		LOGF(LogLevel::eERROR, "Image Format (%u) not supported for creating swapchain buffer", (uint32_t)format);
	}

	return result;
}

/************************************************************************/
// Globals
/************************************************************************/
static const uint32_t gDescriptorTableDWORDS = 1;
static const uint32_t gRootDescriptorDWORDS = 2;

static uint32_t gMaxRootConstantsPerRootParam = 4U;

typedef struct DescriptorIndexMap
{
	char* key;
	uint32_t value;
}DescriptorIndexMap;

/************************************************************************/
// Logging functions
/************************************************************************/
// Proxy log callback
static void internal_log(LogLevel level, const char* msg, const char* component)
{
	LOGF(level, "%s ( %s )", component, msg);
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

	pRenderer->mD3D11.pDxDevice->CreateRenderTargetView(pResource, &rtvDesc, pHandle);
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

	pRenderer->mD3D11.pDxDevice->CreateDepthStencilView(pResource, &dsvDesc, pHandle);
}
/************************************************************************/
// Functions not exposed in IRenderer but still need to be exported in dll
/************************************************************************/
// clang-format off
DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(void, mapBuffer, Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
DECLARE_RENDERER_FUNCTION(void, unmapBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(void, cmdUpdateBuffer, Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
DECLARE_RENDERER_FUNCTION(void, cmdUpdateSubresource, Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_RENDERER_FUNCTION(void, addTexture, Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
DECLARE_RENDERER_FUNCTION(void, removeTexture, Renderer* pRenderer, Texture* pTexture)
DECLARE_RENDERER_FUNCTION(void, addVirtualTexture, Cmd* pCmd, const TextureDesc* pDesc, Texture** ppTexture, void* pImageData)
DECLARE_RENDERER_FUNCTION(void, removeVirtualTexture, Renderer* pRenderer, VirtualTexture* pTexture)
// clang-format on
void d3d11_cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
	//TODO: Use CopySubresource region to update the contents of the destination buffer without mapping them to cpu.

	ASSERT(pCmd);
	ASSERT(pSrcBuffer);
	ASSERT(pBuffer);
	ASSERT(pBuffer->mD3D11.pDxResource);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	D3D11_MAPPED_SUBRESOURCE sub = {};
	D3D11_BOX                dstBox = { (UINT)dstOffset, 0, 0, (UINT)(dstOffset + size), 1, 1 };

	// Not persistently mapped buffer.
	if (!pSrcBuffer->pCpuMappedAddress)
	{
		pContext->Map(pSrcBuffer->mD3D11.pDxResource, 0, D3D11_MAP_READ, 0, &sub);
	}
	else
	{
		sub = { pSrcBuffer->pCpuMappedAddress, 0, 0 };
	}

	pContext->UpdateSubresource(pBuffer->mD3D11.pDxResource, 0, &dstBox, (uint8_t*)sub.pData + srcOffset, (UINT)size, 0);

	if (!pSrcBuffer->pCpuMappedAddress)
	{
		pContext->Unmap(pSrcBuffer->mD3D11.pDxResource, 0);
	}
}

struct SubresourceDataDesc
{
	uint64_t mSrcOffset;
	uint32_t mMipLevel;
	uint32_t mArrayLayer;
	uint32_t mRowPitch;
	uint32_t mSlicePitch;
};

void d3d11_cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pSubresourceDesc)
{
	ASSERT(pCmd);
	ASSERT(pSubresourceDesc);
	ASSERT(pSrcBuffer);
	ASSERT(pTexture->mD3D11.pDxResource);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	D3D11_MAPPED_SUBRESOURCE sub = {};
	UINT subresource = D3D11CalcSubresource(pSubresourceDesc->mMipLevel, pSubresourceDesc->mArrayLayer, (uint32_t)pTexture->mMipLevels);

	if (!pSrcBuffer->pCpuMappedAddress)
	{
		pContext->Map(pSrcBuffer->mD3D11.pDxResource, 0, D3D11_MAP_READ, 0, &sub);
	}
	else
	{
		sub = { pSrcBuffer->pCpuMappedAddress, 0, 0 };
	}

	pContext->UpdateSubresource(
		pTexture->mD3D11.pDxResource, subresource, NULL, (uint8_t*)sub.pData + pSubresourceDesc->mSrcOffset, pSubresourceDesc->mRowPitch,
		pSubresourceDesc->mSlicePitch);

	if (!pSrcBuffer->pCpuMappedAddress)
	{
		pContext->Unmap(pSrcBuffer->mD3D11.pDxResource, 0);
	}
}
/************************************************************************/
// Internal init functions
/************************************************************************/
static bool AddDevice(Renderer* pRenderer, const RendererDesc* pDesc)
{
	int               levelIndex = pDesc->mD3D11.mUseDx10 ? 2 : 0;
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	const uint32_t featureLevelCount = sizeof(featureLevels) / sizeof(featureLevels[0]) - levelIndex;

	HRESULT hr = 0;

	IDXGIAdapter1* dxgiAdapter = NULL;

	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pRenderer->mD3D11.pDXGIFactory)))
	{
		LOGF(LogLevel::eERROR, "Could not create DXGI factory.");
		return false;
	}
	ASSERT(pRenderer->mD3D11.pDXGIFactory);

	// Enumerate all adapters
	typedef struct GpuDesc
	{
		IDXGIAdapter1*                   pGpu;
		D3D_FEATURE_LEVEL                mMaxSupportedFeatureLevel;
		D3D11_FEATURE_DATA_D3D11_OPTIONS mFeatureDataOptions;
#if WINVER > _WIN32_WINNT_WINBLUE
		D3D11_FEATURE_DATA_D3D11_OPTIONS2 mFeatureDataOptions2;
#endif
		SIZE_T         mDedicatedVideoMemory;
		char           mVendorId[MAX_GPU_VENDOR_STRING_LENGTH];
		char           mDeviceId[MAX_GPU_VENDOR_STRING_LENGTH];
		char           mRevisionId[MAX_GPU_VENDOR_STRING_LENGTH];
		char           mName[MAX_GPU_VENDOR_STRING_LENGTH];
		GPUPresetLevel mPreset;
		GpuVendor      mVendor;
	} GpuDesc;

	uint32_t       gpuCount = 0;
	IDXGIAdapter1* adapter = NULL;
	bool           foundSoftwareAdapter = false;

	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != pRenderer->mD3D11.pDXGIFactory->EnumAdapters1(i, (IDXGIAdapter1**)&adapter); ++i)
	{
		DECLARE_ZERO(DXGI_ADAPTER_DESC1, desc);
		adapter->GetDesc1(&desc);

		if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
		{
			// Make sure the adapter can support a D3D11 device
			D3D_FEATURE_LEVEL featLevelOut;
			hr = D3D11CreateDevice(
				adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, &featureLevels[levelIndex], featureLevelCount, D3D11_SDK_VERSION, NULL,
				&featLevelOut, NULL);
			if (E_INVALIDARG == hr)
			{
				hr = D3D11CreateDevice(
					adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, &featureLevels[levelIndex + 1], featureLevelCount - 1,
					D3D11_SDK_VERSION, NULL, &featLevelOut, NULL);
			}
			if (SUCCEEDED(hr))
			{
				GpuDesc gpuDesc = {};
				hr = adapter->QueryInterface(IID_ARGS(&gpuDesc.pGpu));
				if (SUCCEEDED(hr))
				{
					SAFE_RELEASE(gpuDesc.pGpu);
					++gpuCount;
				}
			}
		}
		else
		{
			foundSoftwareAdapter = true;
		}

		adapter->Release();
	}

	// If the only adapter we found is a software adapter, log error message for QA
	if (!gpuCount && foundSoftwareAdapter)
	{
		LOGF(eERROR, "The only available GPU has DXGI_ADAPTER_FLAG_SOFTWARE. Early exiting");
		ASSERT(false);
		return false;
	}

	ASSERT(gpuCount);
	GpuDesc* gpuDesc = (GpuDesc*)alloca(gpuCount * sizeof(GpuDesc));
	memset(gpuDesc, 0, gpuCount * sizeof(GpuDesc));
	gpuCount = 0;

	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != pRenderer->mD3D11.pDXGIFactory->EnumAdapters1(i, (IDXGIAdapter1**)&adapter); ++i)
	{
		DECLARE_ZERO(DXGI_ADAPTER_DESC1, desc);
		adapter->GetDesc1(&desc);

		// Test for display output presence and hardware adapter first to even consider the adapter
		IDXGIOutput* outputs;
		if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && SUCCEEDED(adapter->EnumOutputs(0, &outputs)))
		{
			// Make sure the adapter can support a D3D11 device
			D3D_FEATURE_LEVEL featLevelOut;
			hr = D3D11CreateDevice(
				adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, &featureLevels[levelIndex], featureLevelCount, D3D11_SDK_VERSION, NULL,
				&featLevelOut, NULL);
			if (E_INVALIDARG == hr)
			{
				hr = D3D11CreateDevice(
					adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, &featureLevels[levelIndex + 1], featureLevelCount - 1,
					D3D11_SDK_VERSION, NULL, &featLevelOut, NULL);
			}
			if (SUCCEEDED(hr))
			{
				hr = adapter->QueryInterface(IID_ARGS(&gpuDesc[gpuCount].pGpu));
				if (SUCCEEDED(hr))
				{
					hr = D3D11CreateDevice(
						adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, &featureLevels[levelIndex], featureLevelCount, D3D11_SDK_VERSION,
						&pRenderer->mD3D11.pDxDevice, &featLevelOut, NULL);

					if (E_INVALIDARG == hr)
					{
						hr = D3D11CreateDevice(
							adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, &featureLevels[levelIndex + 1], featureLevelCount - 1,
							D3D11_SDK_VERSION, &pRenderer->mD3D11.pDxDevice, &featLevelOut, NULL);
					}

					if (!pRenderer->mD3D11.pDxDevice)
					{
						LOGF(eERROR, "Device creation failed for adapter %s with code %u", gpuDesc[gpuCount].mName, (uint32_t)hr);
						++gpuCount;
						continue;
					}

					D3D11_FEATURE_DATA_D3D11_OPTIONS featureData = {};
					hr = pRenderer->mD3D11.pDxDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &featureData, sizeof(featureData));
					if (FAILED(hr))
						LOGF(LogLevel::eINFO, "D3D11 CheckFeatureSupport D3D11_FEATURE_D3D11_OPTIONS error 0x%x", hr);
#if WINVER > _WIN32_WINNT_WINBLUE
					D3D11_FEATURE_DATA_D3D11_OPTIONS2 featureData2 = {};
					hr =
						pRenderer->mD3D11.pDxDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2, &featureData2, sizeof(featureData2));
					if (FAILED(hr))
						LOGF(LogLevel::eINFO, "D3D11 CheckFeatureSupport D3D11_FEATURE_D3D11_OPTIONS2 error 0x%x", hr);

					gpuDesc[gpuCount].mFeatureDataOptions2 = featureData2;
#endif
					gpuDesc[gpuCount].mMaxSupportedFeatureLevel = featLevelOut;
					gpuDesc[gpuCount].mDedicatedVideoMemory = desc.DedicatedVideoMemory;
					gpuDesc[gpuCount].mFeatureDataOptions = featureData;
					gpuDesc[gpuCount].mVendor = util_to_internal_gpu_vendor(desc.VendorId);

					//save vendor and model Id as string
					//char hexChar[10];
					//convert deviceId and assign it
					sprintf(gpuDesc[gpuCount].mDeviceId, "%#x\0", desc.DeviceId);
					//convert modelId and assign it
					sprintf(gpuDesc[gpuCount].mVendorId, "%#x\0", desc.VendorId);
					//convert Revision Id
					sprintf(gpuDesc[gpuCount].mRevisionId, "%#x\0", desc.Revision);

					//get preset for current gpu description
					gpuDesc[gpuCount].mPreset =
						getGPUPresetLevel(gpuDesc[gpuCount].mVendorId, gpuDesc[gpuCount].mDeviceId, gpuDesc[gpuCount].mRevisionId);

					//save gpu name (Some situtations this can show description instead of name)
					//char sName[MAX_PATH];
					wcstombs(gpuDesc[gpuCount].mName, desc.Description, FS_MAX_PATH);
					++gpuCount;
					SAFE_RELEASE(pRenderer->mD3D11.pDxDevice);

					// Default GPU is the first GPU received in EnumAdapters
					if (pDesc->mD3D11.mUseDefaultGpu)
					{
						break;
					}
				}
			}
		}

		adapter->Release();
	}

	ASSERT(gpuCount);

	// Sort GPUs by poth Preset and highest feature level gpu at front
	//Prioritize Preset first
	qsort(gpuDesc, gpuCount, sizeof(GpuDesc), [](const void* lhs, const void* rhs) {
		GpuDesc* gpu1 = (GpuDesc*)lhs;
		GpuDesc* gpu2 = (GpuDesc*)rhs;
		// Check feature level first, sort the greatest feature level gpu to the front
		if ((int)gpu1->mPreset != (int)gpu2->mPreset)
		{
			return gpu1->mPreset > gpu2->mPreset ? -1 : 1;
		}
		else if (GPU_VENDOR_INTEL == gpu1->mVendor && GPU_VENDOR_INTEL != gpu2->mVendor)
		{
			return 1;
		}
		else if (GPU_VENDOR_INTEL != gpu1->mVendor && GPU_VENDOR_INTEL == gpu2->mVendor)
		{
			return -1;
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

	GPUSettings* gpuSettings = (GPUSettings*)alloca(gpuCount * sizeof(GPUSettings));

	for (uint32_t i = 0; i < gpuCount; ++i)
	{
		gpuSettings[i] = {};
		gpuSettings[i].mUniformBufferAlignment = 256;
		gpuSettings[i].mUploadBufferTextureAlignment = 1;
		gpuSettings[i].mUploadBufferTextureRowAlignment = 1;
		gpuSettings[i].mMultiDrawIndirect = false;    // no such thing
		gpuSettings[i].mMaxVertexInputBindings = 32U;
		gpuSettings[i].mIndirectRootConstant = false;
		gpuSettings[i].mBuiltinDrawID = false;

		//assign device ID
		strncpy(gpuSettings[i].mGpuVendorPreset.mModelId, gpuDesc[i].mDeviceId, MAX_GPU_VENDOR_STRING_LENGTH);
		//assign vendor ID
		strncpy(gpuSettings[i].mGpuVendorPreset.mVendorId, gpuDesc[i].mVendorId, MAX_GPU_VENDOR_STRING_LENGTH);
		//assign Revision ID
		strncpy(gpuSettings[i].mGpuVendorPreset.mRevisionId, gpuDesc[i].mRevisionId, MAX_GPU_VENDOR_STRING_LENGTH);
		//get name from api
		strncpy(gpuSettings[i].mGpuVendorPreset.mGpuName, gpuDesc[i].mName, MAX_GPU_VENDOR_STRING_LENGTH);
		//get preset
		gpuSettings[i].mGpuVendorPreset.mPresetLevel = gpuDesc[i].mPreset;

		// Determine root signature size for this gpu driver
		gpuSettings[i].mMaxRootSignatureDWORDS = 0U;    // no such thing
#if WINVER > _WIN32_WINNT_WINBLUE
		gpuSettings[i].mROVsSupported = gpuDesc[i].mFeatureDataOptions2.ROVsSupported ? true : false;
#else
		gpuSettings[i].mROVsSupported = false;
#endif
		gpuSettings[i].mTessellationSupported = gpuDesc[i].mMaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_11_0;
		gpuSettings[i].mGeometryShaderSupported = gpuDesc[i].mMaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_10_0;
		gpuSettings[i].mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_NONE;

		// Determine root signature size for this gpu driver
		DXGI_ADAPTER_DESC adapterDesc;
		gpuDesc[i].pGpu->GetDesc(&adapterDesc);
		LOGF(
			LogLevel::eINFO, "GPU[%i] detected. Vendor ID: %x, Model ID: %x, Revision ID: %x, Preset: %s, GPU Name: %S", i,
			adapterDesc.VendorId, adapterDesc.DeviceId, adapterDesc.Revision,
			presetLevelToString(gpuSettings[i].mGpuVendorPreset.mPresetLevel), adapterDesc.Description);
	}

	uint32_t gpuIndex = 0;



	// Get the latest and greatest feature level gpu
	gpuDesc[gpuIndex].pGpu->QueryInterface(&pRenderer->mD3D11.pDxActiveGPU);
	ASSERT(pRenderer->mD3D11.pDxActiveGPU != NULL);
	pRenderer->pActiveGpuSettings = (GPUSettings*)tf_malloc(sizeof(GPUSettings));
	*pRenderer->pActiveGpuSettings = gpuSettings[gpuIndex];
	pRenderer->mLinkedNodeCount = 1;
	pRenderer->mD3D11.mPartialUpdateConstantBufferSupported = gpuDesc[gpuIndex].mFeatureDataOptions.ConstantBufferPartialUpdate;

	//char driverVer[MAX_GPU_VENDOR_STRING_LENGTH];
	//char driverDate[MAX_GPU_VENDOR_STRING_LENGTH];


	//print selected GPU information
	LOGF(LogLevel::eINFO, "GPU[%d] is selected as default GPU", gpuIndex);
	LOGF(LogLevel::eINFO, "Name of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mGpuName);
	LOGF(LogLevel::eINFO, "Vendor id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mVendorId);
	LOGF(LogLevel::eINFO, "Model id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mModelId);
	LOGF(LogLevel::eINFO, "Revision id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mRevisionId);
	LOGF(LogLevel::eINFO, "Preset of selected gpu: %s", presetLevelToString(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel));

	pRenderer->pActiveGpuSettings->mGpuVendorPreset.mGpuDriverVersion[0] = 0;
	pRenderer->pActiveGpuSettings->mGpuVendorPreset.mGpuDriverDate[0] = 0;

	// Create the actual device
	DWORD deviceFlags = 0;

	// The D3D debug layer (as well as Microsoft PIX and other graphics debugger
	// tools using an injection library) is not compatible with Nsight Aftermath.
	// If Aftermath detects that any of these tools are present it will fail initialization.
#if defined(ENABLE_GRAPHICS_DEBUG) && !defined(ENABLE_NSIGHT_AFTERMATH)
	deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

#if defined(ENABLE_NSIGHT_AFTERMATH)
	// Enable Nsight Aftermath GPU crash dump creation.
	// This needs to be done before the Vulkan device is created.
	CreateAftermathTracker(pRenderer->pName, &pRenderer->mAftermathTracker);
#endif

	D3D_FEATURE_LEVEL featureLevel;
	hr = D3D11CreateDevice(
		pRenderer->mD3D11.pDxActiveGPU, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, deviceFlags, &featureLevels[levelIndex], featureLevelCount,
		D3D11_SDK_VERSION, &pRenderer->mD3D11.pDxDevice,
		&featureLevel,    // max feature level
		&pRenderer->mD3D11.pDxContext);

	if (E_INVALIDARG == hr)
	{
		hr = D3D11CreateDevice(
			pRenderer->mD3D11.pDxActiveGPU, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, deviceFlags, &featureLevels[levelIndex + 1],
			featureLevelCount - 1, D3D11_SDK_VERSION, &pRenderer->mD3D11.pDxDevice,
			&featureLevel,    // max feature level
			&pRenderer->mD3D11.pDxContext);
	}
	ASSERT(SUCCEEDED(hr));
	if (FAILED(hr))
		LOGF(LogLevel::eERROR, "Failed to create D3D11 device and context.");

	hr = pRenderer->mD3D11.pDxContext->QueryInterface(&pRenderer->mD3D11.pDxContext1);
	if (SUCCEEDED(hr))
	{
		LOGF(LogLevel::eINFO, "Device supports ID3D11DeviceContext1.");
	}
	else
	{
		pRenderer->mD3D11.pDxContext1 = NULL;
	}

	for (uint32_t i = 0; i < gpuCount; ++i)
	{
		SAFE_RELEASE(gpuDesc[i].pGpu);
	}

	pRenderer->mD3D11.mFeatureLevel = featureLevel;

#if defined(ENABLE_NSIGHT_AFTERMATH)
	SetAftermathDevice(pRenderer->mD3D11.pDxDevice);
#endif

#if defined(ENABLE_PERFORMANCE_MARKER)
	hr = pRenderer->mD3D11.pDxContext->QueryInterface(
		__uuidof(pRenderer->pUserDefinedAnnotation), (void**)(&pRenderer->pUserDefinedAnnotation));
	if (FAILED(hr))
		LOGF(LogLevel::eERROR, "Failed to query interface ID3DUserDefinedAnnotation.");
#endif

	return true;
}

static void RemoveDevice(Renderer* pRenderer)
{
	SAFE_RELEASE(pRenderer->mD3D11.pDXGIFactory);
	SAFE_RELEASE(pRenderer->mD3D11.pDxActiveGPU);

	SAFE_RELEASE(pRenderer->mD3D11.pDxContext1);
	SAFE_RELEASE(pRenderer->mD3D11.pDxContext);
#if defined(ENABLE_GRAPHICS_DEBUG) && !defined(ENABLE_NSIGHT_AFTERMATH)
	ID3D11Debug* pDebugDevice = NULL;
	pRenderer->mD3D11.pDxDevice->QueryInterface(&pDebugDevice);
	SAFE_RELEASE(pRenderer->mD3D11.pDxDevice);

	if (pDebugDevice)
	{
#if WINVER > _WIN32_WINNT_WINBLUE
		pDebugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
		pDebugDevice->Release();
#else
		// Debug device is released first so report live objects don't show its ref as a warning.
		pDebugDevice->Release();
		pDebugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
#endif
	}
#else
	SAFE_RELEASE(pRenderer->mD3D11.pDxDevice);
#endif
	SAFE_FREE(pRenderer->pActiveGpuSettings);

#if defined(ENABLE_NSIGHT_AFTERMATH)
	DestroyAftermathTracker(&pRenderer->mAftermathTracker);
#endif
}
/************************************************************************/
// Pipeline State Functions
/************************************************************************/
static ID3D11BlendState* util_to_blend_state(Renderer* pRenderer, const BlendStateDesc* pDesc)
{
	UNREF_PARAM(pRenderer);

	int blendDescIndex = 0;
#if defined(ENABLE_GRAPHICS_DEBUG)

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

	D3D11_BLEND_DESC desc = {};

	desc.AlphaToCoverageEnable = (BOOL)pDesc->mAlphaToCoverage;
	desc.IndependentBlendEnable = pDesc->mIndependentBlend;
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

	ID3D11BlendState* out = NULL;
	if (FAILED(pRenderer->mD3D11.pDxDevice->CreateBlendState(&desc, &out)))
		LOGF(LogLevel::eERROR, "Failed to create blend state.");

	return out;
}

static ID3D11DepthStencilState* util_to_depth_state(Renderer* pRenderer, const DepthStateDesc* pDesc)
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

	ID3D11DepthStencilState* out = NULL;
	if (FAILED(pRenderer->mD3D11.pDxDevice->CreateDepthStencilState(&desc, &out)))
		LOGF(LogLevel::eERROR, "Failed to create depth state.");

	return out;
}

static ID3D11RasterizerState* util_to_rasterizer_state(Renderer* pRenderer, const RasterizerStateDesc* pDesc)
{
	UNREF_PARAM(pRenderer);

	ASSERT(pDesc->mFillMode < FillMode::MAX_FILL_MODES);
	ASSERT(pDesc->mCullMode < CullMode::MAX_CULL_MODES);
	ASSERT(pDesc->mFrontFace == FRONT_FACE_CCW || pDesc->mFrontFace == FRONT_FACE_CW);

	D3D11_RASTERIZER_DESC desc = {};
	desc.FillMode = gFillModeTranslator[pDesc->mFillMode];
	desc.CullMode = gCullModeTranslator[pDesc->mCullMode];
	desc.FrontCounterClockwise = pDesc->mFrontFace == FRONT_FACE_CCW;
	desc.DepthBias = pDesc->mDepthBias;
	desc.DepthBiasClamp = 0.0f;
	desc.SlopeScaledDepthBias = pDesc->mSlopeScaledDepthBias;
	desc.DepthClipEnable = !pDesc->mDepthClampEnable;
	desc.MultisampleEnable = pDesc->mMultiSample ? TRUE : FALSE;
	desc.AntialiasedLineEnable = FALSE;
	desc.ScissorEnable = pDesc->mScissor;

	ID3D11RasterizerState* out = NULL;
	if (FAILED(pRenderer->mD3D11.pDxDevice->CreateRasterizerState(&desc, &out)))
		LOGF(LogLevel::eERROR, "Failed to create depth state.");

	return out;
}

static void add_default_resources(Renderer* pRenderer)
{
	BlendStateDesc blendStateDesc = {};
	blendStateDesc.mDstAlphaFactors[0] = BC_ZERO;
	blendStateDesc.mDstFactors[0] = BC_ZERO;
	blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
	blendStateDesc.mSrcFactors[0] = BC_ONE;
	blendStateDesc.mMasks[0] = ALL;
	blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
	blendStateDesc.mIndependentBlend = false;
	pRenderer->mD3D11.pDefaultBlendState = util_to_blend_state(pRenderer, &blendStateDesc);

	DepthStateDesc depthStateDesc = {};
	depthStateDesc.mDepthFunc = CMP_LEQUAL;
	depthStateDesc.mDepthTest = false;
	depthStateDesc.mDepthWrite = false;
	depthStateDesc.mStencilBackFunc = CMP_ALWAYS;
	depthStateDesc.mStencilFrontFunc = CMP_ALWAYS;
	depthStateDesc.mStencilReadMask = 0xFF;
	depthStateDesc.mStencilWriteMask = 0xFF;
	pRenderer->mD3D11.pDefaultDepthState = util_to_depth_state(pRenderer, &depthStateDesc);

	RasterizerStateDesc rasterizerStateDesc = {};
	rasterizerStateDesc.mCullMode = CULL_MODE_BACK;
	pRenderer->mD3D11.pDefaultRasterizerState = util_to_rasterizer_state(pRenderer, &rasterizerStateDesc);
}

static void remove_default_resources(Renderer* pRenderer)
{
	SAFE_RELEASE(pRenderer->mD3D11.pDefaultBlendState);
	SAFE_RELEASE(pRenderer->mD3D11.pDefaultDepthState);
	SAFE_RELEASE(pRenderer->mD3D11.pDefaultRasterizerState);
}
/************************************************************************/
// Renderer Init Remove
/************************************************************************/
void d3d11_initRenderer(const char* appName, const RendererDesc* settings, Renderer** ppRenderer)
{
	ASSERT(ppRenderer);
	ASSERT(settings);
	ASSERT(settings->mShaderTarget <= shader_target_5_0);

	Renderer* pRenderer = (Renderer*)tf_calloc_memalign(1, alignof(Renderer), sizeof(Renderer));
	ASSERT(pRenderer);

	pRenderer->mGpuMode = settings->mGpuMode;
	pRenderer->mShaderTarget = shader_target_5_0;
	pRenderer->mEnableGpuBasedValidation = settings->mEnableGPUBasedValidation;

	pRenderer->pName = (char*)tf_calloc(strlen(appName) + 1, sizeof(char));
	strcpy(pRenderer->pName, appName);

	// Initialize the D3D11 bits
	{
		if (!AddDevice(pRenderer, settings))
		{
			*ppRenderer = NULL;
			return;
		}

		//anything below LOW preset is not supported and we will exit
		if (pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel < GPU_PRESET_LOW)
		{
			//have the condition in the assert as well so its cleared when the assert message box appears

			ASSERT(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel >= GPU_PRESET_LOW);    //-V547

			SAFE_FREE(pRenderer->pName);

			//remove device and any memory we allocated in just above as this is the first function called
			//when initializing the forge
			RemoveDevice(pRenderer);
			SAFE_FREE(pRenderer);
			LOGF(LogLevel::eERROR, "Selected GPU has an Office Preset in gpu.cfg.");
			LOGF(LogLevel::eERROR, "Office preset is not supported by The Forge.");

			//return NULL pRenderer so that client can gracefully handle exit
			//This is better than exiting from here in case client has allocated memory or has fallbacks
			*ppRenderer = NULL;
			return;
		}
	}

	d3d11_utils_caps_builder(pRenderer);

	add_default_resources(pRenderer);

	// Renderer is good!
	*ppRenderer = pRenderer;
}

void d3d11_exitRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);

	remove_default_resources(pRenderer);

	RemoveDevice(pRenderer);

	// Free all the renderer components
	SAFE_FREE(pRenderer->pCapBits);
	SAFE_FREE(pRenderer->pName);
	SAFE_FREE(pRenderer);
}

/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void d3d11_addFence(Renderer* pRenderer, Fence** ppFence)
{
	//ASSERT that renderer is valid
	ASSERT(pRenderer);
	ASSERT(ppFence);

	//create a Fence and ASSERT that it is valid
	Fence* pFence = (Fence*)tf_calloc(1, sizeof(Fence));
	ASSERT(pFence);

	D3D11_QUERY_DESC desc = {};
	desc.Query = D3D11_QUERY_EVENT;

	CHECK_HRESULT(pRenderer->mD3D11.pDxDevice->CreateQuery(&desc, &pFence->mD3D11.pDX11Query));

	pFence->mD3D11.mSubmitted = false;

	*ppFence = pFence;
}

void d3d11_removeFence(Renderer* pRenderer, Fence* pFence)
{
	//ASSERT that renderer is valid
	ASSERT(pRenderer);
	//ASSERT that given fence to remove is valid
	ASSERT(pFence);

	SAFE_RELEASE(pFence->mD3D11.pDX11Query);

	SAFE_FREE(pFence);
}

void d3d11_addSemaphore(Renderer* pRenderer, Semaphore** pSemaphore)
{
	// NOTE: We will still use it to be able to generate
	// a dependency graph to serialize parallel GPU workload.

	//ASSERT that renderer is valid
	ASSERT(pRenderer);
	ASSERT(pSemaphore);
}

void d3d11_removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
	//ASSERT that renderer and given semaphore are valid
	ASSERT(pRenderer);
}

void d3d11_addQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppQueue);

	// DX11 doesn't use queues -- so just create a dummy object for the client
	// NOTE: We will still use it to reference the renderer in the queue and to be able to generate
	// a dependency graph to serialize parallel GPU workload.
	Queue* pQueue = (Queue*)tf_calloc(1, sizeof(Queue));
	ASSERT(pQueue);

	// Provided description for queue creation.
	// Note these don't really mean much w/ DX11 but we can use it for debugging
	// what the client is intending to do.
	pQueue->mD3D11.pDxContext = pRenderer->mD3D11.pDxContext;
	pQueue->mNodeIndex = pDesc->mNodeIndex;
	pQueue->mType = pDesc->mType;
	pQueue->mD3D11.pDxDevice = pRenderer->mD3D11.pDxDevice;
	//eastl::string queueType = "DUMMY QUEUE FOR DX11 BACKEND";

	*ppQueue = pQueue;
}

void d3d11_removeQueue(Renderer* pRenderer, Queue* pQueue)
{
	ASSERT(pRenderer);
	ASSERT(pQueue);

	pQueue->mD3D11.pDxContext = NULL;

	SAFE_FREE(pQueue);
}

void d3d11_addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppSwapChain);
	ASSERT(pDesc->mImageCount <= MAX_SWAPCHAIN_IMAGES);

	SwapChain* pSwapChain = (SwapChain*)tf_calloc(1, sizeof(SwapChain) + pDesc->mImageCount * sizeof(RenderTarget*));
	ASSERT(pSwapChain);

	pSwapChain->mD3D11.mDxSyncInterval = pDesc->mEnableVsync ? 1 : 0;
	pSwapChain->ppRenderTargets = (RenderTarget**)(pSwapChain + 1);
	ASSERT(pSwapChain->ppRenderTargets);

	HWND hwnd = (HWND)pDesc->mWindowHandle.window;

	DXGI_SWAP_CHAIN_DESC desc = {};
	desc.BufferDesc.Width = pDesc->mWidth;
	desc.BufferDesc.Height = pDesc->mHeight;
	desc.BufferDesc.Format = util_to_dx11_swapchain_format(pDesc->mColorFormat);
	desc.BufferDesc.RefreshRate.Numerator = 1;
	desc.BufferDesc.RefreshRate.Denominator = 60;
	desc.SampleDesc.Count = 1;    // If multisampling is needed, we'll resolve it later
	desc.SampleDesc.Quality = 0;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = max(1u, pDesc->mImageCount);    // Count includes the front buffer so 1+2 is triple buffering.
	desc.OutputWindow = hwnd;
	desc.Windowed = TRUE;
	desc.Flags = 0;

	DXGI_SWAP_EFFECT swapEffects[] = {
#if WINVER > _WIN32_WINNT_WINBLUE
		DXGI_SWAP_EFFECT_FLIP_DISCARD,
#endif
		DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
		DXGI_SWAP_EFFECT_DISCARD,
		DXGI_SWAP_EFFECT_SEQUENTIAL
	};

	HRESULT         hres = E_FAIL;
	IDXGISwapChain* swapchain = NULL;
	uint32_t        swapEffectsCount = (sizeof swapEffects / sizeof DXGI_SWAP_EFFECT);
	uint32_t        i = pDesc->mUseFlipSwapEffect ? 0 : swapEffectsCount - 2;

	for (; i < swapEffectsCount; ++i)
	{
		desc.SwapEffect = swapEffects[i];
		hres = pRenderer->mD3D11.pDXGIFactory->CreateSwapChain(pRenderer->mD3D11.pDxDevice, &desc, &swapchain);
		if (SUCCEEDED(hres))
		{
			pSwapChain->mD3D11.mSwapEffect = swapEffects[i];
			LOGF(eINFO, "Swapchain creation SUCCEEDED with SwapEffect %d", swapEffects[i]);
			break;
		}
	}
	if (!SUCCEEDED(hres))
	{
		LOGF(eERROR, "Swapchain creation FAILED with HRESULT: %u", (uint32_t)hres);
		ASSERT(false);
	}

	CHECK_HRESULT(pRenderer->mD3D11.pDXGIFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

	CHECK_HRESULT(swapchain->QueryInterface(__uuidof(pSwapChain->mD3D11.pDxSwapChain), (void**)&(pSwapChain->mD3D11.pDxSwapChain)));
	swapchain->Release();

	ID3D11Resource** buffers = (ID3D11Resource**)alloca(pDesc->mImageCount * sizeof(ID3D11Resource*));

	// Create rendertargets from swapchain
	for (uint32_t i = 0; i < pDesc->mImageCount; ++i)
	{
		CHECK_HRESULT(pSwapChain->mD3D11.pDxSwapChain->GetBuffer(0, IID_ARGS(&buffers[i])));
	}

	RenderTargetDesc descColor = {};
	descColor.mWidth = pDesc->mWidth;
	descColor.mHeight = pDesc->mHeight;
	descColor.mDepth = 1;
	descColor.mArraySize = 1;
	descColor.mFormat = pDesc->mColorFormat;
	descColor.mClearValue = pDesc->mColorClearValue;
	descColor.mSampleCount = SAMPLE_COUNT_1;
	descColor.mSampleQuality = 0;
	descColor.mFlags |= TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET;

	for (uint32_t i = 0; i < pDesc->mImageCount; ++i)
	{
		descColor.pNativeHandle = (void*)buffers[i];
		::addRenderTarget(pRenderer, &descColor, &pSwapChain->ppRenderTargets[i]);
	}

	pSwapChain->mImageCount = pDesc->mImageCount;
	pSwapChain->mD3D11.mImageIndex = 0;
	pSwapChain->mEnableVsync = pDesc->mEnableVsync;

	*ppSwapChain = pSwapChain;
}

void d3d11_removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
	for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
	{
		ID3D11Resource* resource = pSwapChain->ppRenderTargets[i]->pTexture->mD3D11.pDxResource;
		removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[i]);
		SAFE_RELEASE(resource);
	}

	SAFE_RELEASE(pSwapChain->mD3D11.pDxSwapChain);
	SAFE_FREE(pSwapChain);
}
/************************************************************************/
// Command Pool Functions
/************************************************************************/
void d3d11_addCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
{
	// NOTE: We will still use cmd pools to be able to generate
	// a dependency graph to serialize parallel GPU workload.

	//ASSERT that renderer is valid
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppCmdPool);

	// initialize to zero
	CmdPool* pCmdPool = (CmdPool*)tf_calloc(1, sizeof(CmdPool));
	ASSERT(pCmdPool);

	pCmdPool->pQueue = pDesc->pQueue;

	*ppCmdPool = pCmdPool;
}

void d3d11_removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	//check validity of given renderer and command pool
	ASSERT(pRenderer);
	ASSERT(pCmdPool);

	SAFE_FREE(pCmdPool);
}

void d3d11_addCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
{
	//verify that given pool is valid
	ASSERT(pRenderer);
	ASSERT(ppCmd);

	//allocate new command
	Cmd* pCmd = (Cmd*)tf_calloc_memalign(1, alignof(Cmd), sizeof(Cmd));
	ASSERT(pCmd);

	//set command pool of new command
	pCmd->pRenderer = pRenderer;

	*ppCmd = pCmd;
}

void d3d11_removeCmd(Renderer* pRenderer, Cmd* pCmd)
{
	//verify that given command and pool are valid
	ASSERT(pRenderer);
	ASSERT(pCmd);

	SAFE_RELEASE(pCmd->mD3D11.pRootConstantBuffer);
	SAFE_FREE(pCmd);
}

void d3d11_addCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmd)
{
	//verify that ***cmd is valid
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(cmdCount);
	ASSERT(pppCmd);

	Cmd** ppCmds = (Cmd**)tf_calloc(cmdCount, sizeof(Cmd*));
	ASSERT(ppCmds);

	//add n new cmds to given pool
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		::addCmd(pRenderer, pDesc, &ppCmds[i]);
	}

	*pppCmd = ppCmds;
}

void d3d11_removeCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
{
	//verify that given command list is valid
	ASSERT(ppCmds);

	//remove every given cmd in array
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		removeCmd(pRenderer, ppCmds[i]);
	}

	SAFE_FREE(ppCmds);
}

/************************************************************************/
// All buffer, texture loading handled by resource system -> IResourceLoader.
/************************************************************************/
void d3d11_addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppRenderTarget);

	bool const isDepth = TinyImageFormat_IsDepthAndStencil(pDesc->mFormat) || TinyImageFormat_IsDepthOnly(pDesc->mFormat);

	ASSERT(!((isDepth) && (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)) && "Cannot use depth stencil as UAV");

	((RenderTargetDesc*)pDesc)->mMipLevels = max(1U, pDesc->mMipLevels);

	uint32_t numRTVs = pDesc->mMipLevels;
	if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		numRTVs *= (pDesc->mDepth * pDesc->mArraySize);

	size_t totalSize = sizeof(RenderTarget);
	totalSize += numRTVs * sizeof(ID3D11DepthStencilView*);
	RenderTarget* pRenderTarget = (RenderTarget*)tf_calloc_memalign(1, alignof(RenderTarget), totalSize);
	ASSERT(pRenderTarget);

	if (isDepth)
		pRenderTarget->mD3D11.pDxDsvSliceDescriptors = (ID3D11DepthStencilView**)(pRenderTarget + 1);
	else
		pRenderTarget->mD3D11.pDxRtvSliceDescriptors = (ID3D11RenderTargetView**)(pRenderTarget + 1);

	//add to gpu
	DXGI_FORMAT dxFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mFormat);
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
#if defined(ENABLE_GRAPHICS_DEBUG)
	textureDesc.pName = pDesc->pName;
#endif
	textureDesc.mNodeIndex = pDesc->mNodeIndex;
	textureDesc.pSharedNodeIndices = pDesc->pSharedNodeIndices;
	textureDesc.mSharedNodeIndexCount = pDesc->mSharedNodeIndexCount;
	textureDesc.mDescriptors = pDesc->mDescriptors;

	if (!(textureDesc.mFlags & TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET))
		textureDesc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE;

	addTexture(pRenderer, &textureDesc, &pRenderTarget->pTexture);

	if (isDepth)
		add_dsv(pRenderer, pRenderTarget->pTexture->mD3D11.pDxResource, dxFormat, 0, -1, &pRenderTarget->mD3D11.pDxDsvDescriptor);
	else
		add_rtv(pRenderer, pRenderTarget->pTexture->mD3D11.pDxResource, dxFormat, 0, -1, &pRenderTarget->mD3D11.pDxRtvDescriptor);

	for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
	{
		const uint32_t depthOrArraySize = pDesc->mDepth * pDesc->mArraySize;
		if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
			(pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
		{
			for (uint32_t j = 0; j < depthOrArraySize; ++j)
			{
				if (isDepth)
					add_dsv(
						pRenderer, pRenderTarget->pTexture->mD3D11.pDxResource, dxFormat, i, j,
						&pRenderTarget->mD3D11.pDxDsvSliceDescriptors[i * depthOrArraySize + j]);
				else
					add_rtv(
						pRenderer, pRenderTarget->pTexture->mD3D11.pDxResource, dxFormat, i, j,
						&pRenderTarget->mD3D11.pDxRtvSliceDescriptors[i * depthOrArraySize + j]);
			}
		}
		else
		{
			if (isDepth)
				add_dsv(
					pRenderer, pRenderTarget->pTexture->mD3D11.pDxResource, dxFormat, i, -1,
					&pRenderTarget->mD3D11.pDxDsvSliceDescriptors[i]);
			else
				add_rtv(
					pRenderer, pRenderTarget->pTexture->mD3D11.pDxResource, dxFormat, i, -1,
					&pRenderTarget->mD3D11.pDxRtvSliceDescriptors[i]);
		}
	}

	pRenderTarget->mWidth = pDesc->mWidth;
	pRenderTarget->mHeight = pDesc->mHeight;
	pRenderTarget->mArraySize = pDesc->mArraySize;
	pRenderTarget->mDepth = pDesc->mDepth;
	pRenderTarget->mMipLevels = pDesc->mMipLevels;
	pRenderTarget->mSampleCount = pDesc->mSampleCount;
	pRenderTarget->mSampleQuality = pDesc->mSampleQuality;
	pRenderTarget->mFormat = pDesc->mFormat;
	pRenderTarget->mClearValue = pDesc->mClearValue;

	*ppRenderTarget = pRenderTarget;
}

void d3d11_removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
	bool const isDepth = TinyImageFormat_HasDepth(pRenderTarget->mFormat);

	removeTexture(pRenderer, pRenderTarget->pTexture);

	if (isDepth)
	{
		SAFE_RELEASE(pRenderTarget->mD3D11.pDxDsvDescriptor);
	}
	else
	{
		SAFE_RELEASE(pRenderTarget->mD3D11.pDxRtvDescriptor);
	}

	const uint32_t depthOrArraySize = pRenderTarget->mArraySize * pRenderTarget->mDepth;
	if ((pRenderTarget->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
		(pRenderTarget->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
	{
		for (uint32_t i = 0; i < pRenderTarget->mMipLevels; ++i)
		{
			for (uint32_t j = 0; j < depthOrArraySize; ++j)
			{
				if (isDepth)
				{
					SAFE_RELEASE(pRenderTarget->mD3D11.pDxDsvSliceDescriptors[i * depthOrArraySize + j]);
				}
				else
				{
					SAFE_RELEASE(pRenderTarget->mD3D11.pDxRtvSliceDescriptors[i * depthOrArraySize + j]);
				}
			}
		}
	}
	else
	{
		for (uint32_t i = 0; i < pRenderTarget->mMipLevels; ++i)
		{
			if (isDepth)
			{
				SAFE_RELEASE(pRenderTarget->mD3D11.pDxDsvSliceDescriptors[i]);
			}
			else
			{
				SAFE_RELEASE(pRenderTarget->mD3D11.pDxRtvSliceDescriptors[i]);
			}
		}
	}

	SAFE_FREE(pRenderTarget);
}

void d3d11_addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->mD3D11.pDxDevice);
	ASSERT(pDesc->mCompareFunc < MAX_COMPARE_MODES);
	ASSERT(ppSampler);

	// initialize to zero
	Sampler* pSampler = (Sampler*)tf_calloc_memalign(1, alignof(Sampler), sizeof(Sampler));
	ASSERT(pSampler);
	
	//default sampler lod values
	//used if not overriden by mSetLodRange or not Linear mipmaps
	float minSamplerLod = 0;
	float maxSamplerLod = pDesc->mMipMapMode == MIPMAP_MODE_LINEAR ? D3D11_FLOAT32_MAX : 0;
	//user provided lods
	if(pDesc->mSetLodRange)
	{
		minSamplerLod = pDesc->mMinLod;
		maxSamplerLod = pDesc->mMaxLod;
	}

	//add sampler to gpu
	D3D11_SAMPLER_DESC desc;
	desc.Filter = util_to_dx11_filter(
		pDesc->mMinFilter, pDesc->mMagFilter, pDesc->mMipMapMode, pDesc->mMaxAnisotropy > 0.0f,
		(pDesc->mCompareFunc != CMP_NEVER ? true : false));
	desc.AddressU = util_to_dx11_texture_address_mode(pDesc->mAddressU);
	desc.AddressV = util_to_dx11_texture_address_mode(pDesc->mAddressV);
	desc.AddressW = util_to_dx11_texture_address_mode(pDesc->mAddressW);
	desc.MipLODBias = pDesc->mMipLodBias;
	desc.MaxAnisotropy = max((UINT)pDesc->mMaxAnisotropy, 1U);
	desc.ComparisonFunc = gComparisonFuncTranslator[pDesc->mCompareFunc];
	desc.BorderColor[0] = 0.0f;
	desc.BorderColor[1] = 0.0f;
	desc.BorderColor[2] = 0.0f;
	desc.BorderColor[3] = 0.0f;
	desc.MinLOD = minSamplerLod;
	desc.MaxLOD = maxSamplerLod;

	if (FAILED(pRenderer->mD3D11.pDxDevice->CreateSamplerState(&desc, &pSampler->mD3D11.pSamplerState)))
		LOGF(LogLevel::eERROR, "Failed to create sampler state.");

	*ppSampler = pSampler;
}

void d3d11_removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
	ASSERT(pRenderer);
	ASSERT(pSampler);

	SAFE_RELEASE(pSampler->mD3D11.pSamplerState);
	SAFE_FREE(pSampler);
}

/************************************************************************/
// Shader Functions
/************************************************************************/
void d3d11_compileShader(
	Renderer* pRenderer, ShaderTarget shaderTarget, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code, bool,
	uint32_t macroCount, ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compile_flags = D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compile_flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

#if WINVER > _WIN32_WINNT_WINBLUE
	compile_flags |= (D3DCOMPILE_DEBUG | D3DCOMPILE_ALL_RESOURCES_BOUND | D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);

#else
	compile_flags |= (D3DCOMPILE_DEBUG);
#endif

	bool featureLevel11 = pRenderer->mD3D11.mFeatureLevel >= D3D_FEATURE_LEVEL_11_0;

	int  major = featureLevel11 ? 5 : 4;
	int  minor = 0;
	char target[32] = {};
	switch (stage)
	{
		case SHADER_STAGE_VERT: sprintf(target, "vs_%d_%d", major, minor); break;
		case SHADER_STAGE_TESC: sprintf(target, "hs_%d_%d", major, minor); break;
		case SHADER_STAGE_TESE: sprintf(target, "ds_%d_%d", major, minor); break;
		case SHADER_STAGE_GEOM: sprintf(target, "gs_%d_%d", major, minor); break;
		case SHADER_STAGE_FRAG: sprintf(target, "ps_%d_%d", major, minor); break;
		case SHADER_STAGE_COMP: sprintf(target, "cs_%d_%d", major, minor); break;
		default: break;
	}

	if (!featureLevel11)
	{
		// Exclude compute shaders as well since there are many restrictions like no typed UAVs in SM 4.0
		if (stage != SHADER_STAGE_VERT && stage != SHADER_STAGE_FRAG)
		{
			LOGF(eERROR, "%s profile not supported in feature level %u", target, pRenderer->mD3D11.mFeatureLevel);
			ASSERT(false);
			return;
		}
	}

	// Extract shader macro definitions into D3D_SHADER_MACRO scruct
	// Allocate Size+2 structs: one for D3D11 1 definition and one for null termination
	D3D_SHADER_MACRO* macros = (D3D_SHADER_MACRO*)alloca((macroCount + 2) * sizeof(D3D_SHADER_MACRO));
	macros[0] = { "D3D11", "1" };
	for (uint32_t j = 0; j < macroCount; ++j)
	{
		macros[j + 1] = { pMacros[j].definition, pMacros[j].value };
	}
	macros[macroCount + 1] = { NULL, NULL };

	// provide path to source file to enable shader includes
	char relativeSourceName[FS_MAX_PATH] = { 0 };
	fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_SOURCES), fileName, relativeSourceName);

	const char* entryPoint = pEntryPoint ? pEntryPoint : "main";
	ID3DBlob*   compiled_code = NULL;
	ID3DBlob*   error_msgs = NULL;
	HRESULT     hres = D3DCompile2(
        code, (size_t)codeSize, relativeSourceName, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, target, compile_flags, 0, 0,
        NULL, 0, &compiled_code, &error_msgs);
	if (FAILED(hres))
	{
		char* msg = (char*)tf_calloc(error_msgs->GetBufferSize() + 1, sizeof(*msg));
		ASSERT(msg);
		memcpy(msg, error_msgs->GetBufferPointer(), error_msgs->GetBufferSize());
		LOGF(eERROR, "%s %s", fileName, msg);
		SAFE_FREE(msg);
	}
	ASSERT(SUCCEEDED(hres));

	char* pByteCode = (char*)tf_malloc(compiled_code->GetBufferSize());
	memcpy(pByteCode, compiled_code->GetBufferPointer(), compiled_code->GetBufferSize());

	pOut->mByteCodeSize = (uint32_t)compiled_code->GetBufferSize();
	pOut->pByteCode = pByteCode;
	SAFE_RELEASE(compiled_code);
}

void d3d11_addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mStages);
	ASSERT(ppShaderProgram);

	Shader* pShaderProgram = (Shader*)tf_calloc(1, sizeof(Shader) + sizeof(PipelineReflection));
	ASSERT(pShaderProgram);

	pShaderProgram->mStages = pDesc->mStages;
	pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1);    //-V1027

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
					CHECK_HRESULT(pRenderer->mD3D11.pDxDevice->CreateVertexShader(
						pStage->pByteCode, pStage->mByteCodeSize, NULL, &pShaderProgram->mD3D11.pDxVertexShader));
					CHECK_HRESULT(
						D3DGetInputSignatureBlob(pStage->pByteCode, pStage->mByteCodeSize, &pShaderProgram->mD3D11.pDxInputSignature));
				}
				break;
				case SHADER_STAGE_HULL:
				{
					pStage = &pDesc->mHull;
					CHECK_HRESULT(pRenderer->mD3D11.pDxDevice->CreateHullShader(
						pStage->pByteCode, pStage->mByteCodeSize, NULL, &pShaderProgram->mD3D11.pDxHullShader));
				}
				break;
				case SHADER_STAGE_DOMN:
				{
					pStage = &pDesc->mDomain;
					CHECK_HRESULT(pRenderer->mD3D11.pDxDevice->CreateDomainShader(
						pStage->pByteCode, pStage->mByteCodeSize, NULL, &pShaderProgram->mD3D11.pDxDomainShader));
				}
				break;
				case SHADER_STAGE_GEOM:
				{
					pStage = &pDesc->mGeom;
					CHECK_HRESULT(pRenderer->mD3D11.pDxDevice->CreateGeometryShader(
						pStage->pByteCode, pStage->mByteCodeSize, NULL, &pShaderProgram->mD3D11.pDxGeometryShader));
				}
				break;
				case SHADER_STAGE_FRAG:
				{
					pStage = &pDesc->mFrag;
					CHECK_HRESULT(pRenderer->mD3D11.pDxDevice->CreatePixelShader(
						pStage->pByteCode, pStage->mByteCodeSize, NULL, &pShaderProgram->mD3D11.pDxPixelShader));
				}
				break;
				case SHADER_STAGE_COMP:
				{
					pStage = &pDesc->mComp;
					CHECK_HRESULT(pRenderer->mD3D11.pDxDevice->CreateComputeShader(
						pStage->pByteCode, pStage->mByteCodeSize, NULL, &pShaderProgram->mD3D11.pDxComputeShader));
				}
				break;
				default:
				{
					LOGF(LogLevel::eERROR, "Unknown shader stage %i", stage_mask);
				}
				break;
			}

			d3d11_createShaderReflection(
				(uint8_t*)(pStage->pByteCode), (uint32_t)pStage->mByteCodeSize, stage_mask,    //-V522
				&pShaderProgram->pReflection->mStageReflections[reflectionCount]);

			reflectionCount++;
		}
	}

	createPipelineReflection(pShaderProgram->pReflection->mStageReflections, reflectionCount, pShaderProgram->pReflection);

	addShaderDependencies(pShaderProgram, pDesc);

	*ppShaderProgram = pShaderProgram;
}

void d3d11_removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
	UNREF_PARAM(pRenderer);

	removeShaderDependencies(pShaderProgram);

	//remove given shader
	destroyPipelineReflection(pShaderProgram->pReflection);

	if (pShaderProgram->mStages & SHADER_STAGE_COMP)
	{
		SAFE_RELEASE(pShaderProgram->mD3D11.pDxComputeShader);
	}
	else
	{
		SAFE_RELEASE(pShaderProgram->mD3D11.pDxVertexShader);
		SAFE_RELEASE(pShaderProgram->mD3D11.pDxPixelShader);
		SAFE_RELEASE(pShaderProgram->mD3D11.pDxGeometryShader);
		SAFE_RELEASE(pShaderProgram->mD3D11.pDxDomainShader);
		SAFE_RELEASE(pShaderProgram->mD3D11.pDxHullShader);
		SAFE_RELEASE(pShaderProgram->mD3D11.pDxInputSignature);
	}

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
		default: ASSERT(false && "Invalid Memory Usage"); return (D3D11_CPU_ACCESS_FLAG)0;
	}
}

D3D11_RESOURCE_MISC_FLAG util_determine_dx_resource_misc_flags(DescriptorType type, TinyImageFormat format)
{
	uint32_t ret = {};
	if (DESCRIPTOR_TYPE_TEXTURE_CUBE == (type & DESCRIPTOR_TYPE_TEXTURE_CUBE))
		ret |= D3D11_RESOURCE_MISC_TEXTURECUBE;
	if (type & DESCRIPTOR_TYPE_INDIRECT_BUFFER)
		ret |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

	if (DESCRIPTOR_TYPE_BUFFER_RAW == (type & DESCRIPTOR_TYPE_BUFFER_RAW))
		ret |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	else if ((type & DESCRIPTOR_TYPE_BUFFER) && format == TinyImageFormat_UNDEFINED)
		ret |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	if (DESCRIPTOR_TYPE_RW_BUFFER_RAW == (type & DESCRIPTOR_TYPE_RW_BUFFER_RAW))
		ret |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	else if ((type & DESCRIPTOR_TYPE_RW_BUFFER) && format == TinyImageFormat_UNDEFINED)
		ret |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	return (D3D11_RESOURCE_MISC_FLAG)ret;
}

D3D11_USAGE util_to_dx_usage(ResourceMemoryUsage mem)
{
	switch (mem)
	{
		case RESOURCE_MEMORY_USAGE_GPU_ONLY: return D3D11_USAGE_DEFAULT;
		case RESOURCE_MEMORY_USAGE_CPU_TO_GPU: return D3D11_USAGE_DYNAMIC;
		case RESOURCE_MEMORY_USAGE_CPU_ONLY:
		case RESOURCE_MEMORY_USAGE_GPU_TO_CPU: return D3D11_USAGE_STAGING;
		default: ASSERT(false && "Invalid Memory Usage"); return D3D11_USAGE_DEFAULT;
	}
}

void d3d11_addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
	//verify renderer validity
	ASSERT(pRenderer);
	//verify adding at least 1 buffer
	ASSERT(pDesc);
	ASSERT(pDesc->mSize > 0);
	ASSERT(ppBuffer);

	// Vertex buffer cannot have SRV or UAV in D3D11
	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER && pDesc->mDescriptors & DESCRIPTOR_TYPE_VERTEX_BUFFER)
		((BufferDesc*)pDesc)->mDescriptors = pDesc->mDescriptors & (DescriptorType)~DESCRIPTOR_TYPE_BUFFER;
	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER && pDesc->mDescriptors & DESCRIPTOR_TYPE_VERTEX_BUFFER)
		((BufferDesc*)pDesc)->mDescriptors = pDesc->mDescriptors & (DescriptorType)~DESCRIPTOR_TYPE_RW_BUFFER;

	// initialize to zero
	Buffer* pBuffer = (Buffer*)tf_calloc_memalign(1, alignof(Buffer), sizeof(Buffer));
	ASSERT(pBuffer);

	uint64_t allocationSize = pDesc->mSize;
	//add to renderer
	// Align the buffer size to multiples of 256
	if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER))
	{
		allocationSize = round_up_64(allocationSize, pRenderer->pActiveGpuSettings->mUniformBufferAlignment);
	}

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = (UINT)allocationSize;
	desc.CPUAccessFlags = util_determine_dx_cpu_access_flags(pDesc->mMemoryUsage);
	desc.Usage = util_to_dx_usage(pDesc->mMemoryUsage);
	// Staging resource can't have bind flags.
	if (desc.Usage != D3D11_USAGE_STAGING)
	{
		desc.BindFlags = util_determine_dx_bind_flags(pDesc->mDescriptors, RESOURCE_STATE_COMMON);
	}
	desc.MiscFlags = util_determine_dx_resource_misc_flags(pDesc->mDescriptors, pDesc->mFormat);
	desc.StructureByteStride = (UINT)((desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) ? 0 : pDesc->mStructStride);
	// Structured buffer need to align to 4 bytes and not use invalid bind flags
	if (desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
	{
		desc.StructureByteStride = (UINT)round_up_64(pDesc->mStructStride, 4);
		desc.BindFlags &=
			~(D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER | D3D11_BIND_CONSTANT_BUFFER | D3D11_BIND_STREAM_OUTPUT |
			  D3D11_BIND_RENDER_TARGET | D3D11_BIND_DEPTH_STENCIL);
	}
	CHECK_HRESULT_DEVICE(pRenderer->mD3D11.pDxDevice, pRenderer->mD3D11.pDxDevice->CreateBuffer(&desc, NULL, &pBuffer->mD3D11.pDxResource));

	if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER) && !(pDesc->mFlags & BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION))
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		if (DESCRIPTOR_TYPE_BUFFER_RAW == (pDesc->mDescriptors & DESCRIPTOR_TYPE_BUFFER_RAW))
		{
			if (pDesc->mFormat != TinyImageFormat_UNDEFINED)
			{
				LOGF(LogLevel::eWARNING, "Raw buffers use R32 typeless format. Format will be ignored");
			}

			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			srvDesc.BufferEx.FirstElement = (UINT)pDesc->mFirstElement;
			srvDesc.BufferEx.NumElements = (UINT)(pDesc->mElementCount);
			srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
			srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		}
		else
		{
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			srvDesc.Buffer.FirstElement = (UINT)pDesc->mFirstElement;
			srvDesc.Buffer.NumElements = (UINT)(pDesc->mElementCount);
			srvDesc.Buffer.ElementWidth = (UINT)(pDesc->mStructStride);
			srvDesc.Buffer.ElementOffset = 0;
			srvDesc.Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mFormat);

			// Cannot create a typed StructuredBuffer
			if (srvDesc.Format != DXGI_FORMAT_UNKNOWN)
			{
				srvDesc.Buffer.ElementWidth = 0;
			}
		}

		pRenderer->mD3D11.pDxDevice->CreateShaderResourceView(pBuffer->mD3D11.pDxResource, &srvDesc, &pBuffer->mD3D11.pDxSrvHandle);
	}

	if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER) && !(pDesc->mFlags & BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION))
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = (UINT)pDesc->mFirstElement;
		uavDesc.Buffer.NumElements = (UINT)(pDesc->mElementCount);
		uavDesc.Buffer.Flags = 0;
		if (DESCRIPTOR_TYPE_RW_BUFFER_RAW == (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER_RAW))
		{
			if (pDesc->mFormat != TinyImageFormat_UNDEFINED)
				LOGF(LogLevel::eWARNING, "Raw buffers use R32 typeless format. Format will be ignored");
			uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			uavDesc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_RAW;
		}
		else if (pDesc->mFormat != TinyImageFormat_UNDEFINED)
		{
			uavDesc.Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mFormat);
		}

		pRenderer->mD3D11.pDxDevice->CreateUnorderedAccessView(pBuffer->mD3D11.pDxResource, &uavDesc, &pBuffer->mD3D11.pDxUavHandle);
	}

	if (pDesc->pName)
	{
		setBufferName(pRenderer, pBuffer, pDesc->pName);
	}

	pBuffer->mSize = (uint32_t)pDesc->mSize;
	pBuffer->mMemoryUsage = pDesc->mMemoryUsage;
	pBuffer->mNodeIndex = pDesc->mNodeIndex;
	pBuffer->mDescriptors = pDesc->mDescriptors;
	pBuffer->mD3D11.mFlags = pDesc->mFlags;

	*ppBuffer = pBuffer;
}

void d3d11_removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	UNREF_PARAM(pRenderer);
	ASSERT(pRenderer);
	ASSERT(pBuffer);

	SAFE_RELEASE(pBuffer->mD3D11.pDxSrvHandle);
	SAFE_RELEASE(pBuffer->mD3D11.pDxUavHandle);
	SAFE_RELEASE(pBuffer->mD3D11.pDxResource);
	SAFE_FREE(pBuffer);
}

void d3d11_mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
	UNREF_PARAM(pRange);

	D3D11_MAP           mapType = {};
	ResourceMemoryUsage mem = (ResourceMemoryUsage)pBuffer->mMemoryUsage;
	switch (mem)
	{
		case RESOURCE_MEMORY_USAGE_CPU_ONLY: mapType = D3D11_MAP_READ_WRITE; break;
		case RESOURCE_MEMORY_USAGE_CPU_TO_GPU:
			// its possible a driver doesn't support partial updates to constant buffers
			// this errors out in that case if a partial update is asked for
			// To maintain API compatibility on these devices, a cpu buffer
			// should shadow and upload when all updates are finished but thats not
			// an easy thing as its not on Unmap but first usage...
			if (pBuffer->mDescriptors == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			{
				if (pRange == NULL)
					mapType = D3D11_MAP_WRITE_DISCARD;
				else if (pRenderer->mD3D11.mPartialUpdateConstantBufferSupported)
					mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
				else
					LOGF(LogLevel::eERROR, "Device doesn't support partial uniform buffer updates");
			}
			else
			{
				mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
			}
			break;
		case RESOURCE_MEMORY_USAGE_GPU_TO_CPU: mapType = D3D11_MAP_READ; break;
		default: break;
	}
	D3D11_MAPPED_SUBRESOURCE sub = {};
	CHECK_HRESULT_DEVICE(pRenderer->mD3D11.pDxDevice, pRenderer->mD3D11.pDxContext->Map(pBuffer->mD3D11.pDxResource, 0, mapType, 0, &sub));
	pBuffer->pCpuMappedAddress = sub.pData;
}

void d3d11_unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	pRenderer->mD3D11.pDxContext->Unmap(pBuffer->mD3D11.pDxResource, 0);
	pBuffer->pCpuMappedAddress = nullptr;
}

void d3d11_addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mWidth && pDesc->mHeight && (pDesc->mDepth || pDesc->mArraySize));
	ASSERT(ppTexture);

	if (pDesc->mSampleCount > SAMPLE_COUNT_1 && pDesc->mMipLevels > 1)
	{
		LOGF(LogLevel::eERROR, "Multi-Sampled textures cannot have mip maps");
		ASSERT(false);
		return;
	}

	// initialize to zero
	size_t totalSize = sizeof(Texture);
	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
		totalSize += pDesc->mMipLevels * sizeof(ID3D11UnorderedAccessView*);

	Texture* pTexture = (Texture*)tf_calloc_memalign(1, alignof(Texture), totalSize);
	ASSERT(pTexture);

	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
		pTexture->mD3D11.pDxUAVDescriptors = (ID3D11UnorderedAccessView**)(pTexture + 1);

	if (pDesc->pNativeHandle)
	{
		pTexture->mOwnsImage = false;
		pTexture->mD3D11.pDxResource = (ID3D11Resource*)pDesc->pNativeHandle;
	}
	else
	{
		pTexture->mOwnsImage = true;
	}

	//add to gpu
	DXGI_FORMAT              dxFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mFormat);
	DescriptorType           descriptors = pDesc->mDescriptors;
	D3D11_RESOURCE_DIMENSION res_dim = {};

	bool featureLevel11 = pRenderer->mD3D11.mFeatureLevel >= D3D_FEATURE_LEVEL_11_0;

	if (!featureLevel11)
	{
		// Cannot create SRV of depth stencil in feature level 10 or less
		if ((descriptors & DESCRIPTOR_TYPE_TEXTURE) &&
			(TinyImageFormat_HasDepth(pDesc->mFormat) || TinyImageFormat_HasStencil(pDesc->mFormat)))
		{
			descriptors = descriptors & (DescriptorType)(~DESCRIPTOR_TYPE_TEXTURE);
		}
	}

	if (pDesc->mFlags & TEXTURE_CREATION_FLAG_FORCE_2D)
	{
		ASSERT(pDesc->mDepth == 1);
		res_dim = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
	}
	else if (pDesc->mFlags & TEXTURE_CREATION_FLAG_FORCE_3D)
	{
		res_dim = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
	}
	else
	{
		if (pDesc->mDepth > 1)
			res_dim = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
		else if (pDesc->mHeight > 1)
			res_dim = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
		else
			res_dim = D3D11_RESOURCE_DIMENSION_TEXTURE1D;
	}

	ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

	if (NULL == pTexture->mD3D11.pDxResource)
	{
		switch (res_dim)
		{
			case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			{
				ID3D11Texture1D*     pTex1D;
				D3D11_TEXTURE1D_DESC desc = {};
				desc.ArraySize = pDesc->mArraySize;
				desc.BindFlags = util_determine_dx_bind_flags(pDesc->mDescriptors, pDesc->mStartState);
				desc.CPUAccessFlags = 0;
				desc.Format =
					featureLevel11 ? (DXGI_FORMAT)TinyImageFormat_DXGI_FORMATToTypeless((TinyImageFormat_DXGI_FORMAT)dxFormat) : dxFormat;
				desc.MipLevels = pDesc->mMipLevels;
				desc.MiscFlags = util_determine_dx_resource_misc_flags(pDesc->mDescriptors, pDesc->mFormat);
				desc.Usage = util_to_dx_usage(RESOURCE_MEMORY_USAGE_GPU_ONLY);
				desc.Width = pDesc->mWidth;
				CHECK_HRESULT_DEVICE(pRenderer->mD3D11.pDxDevice, pRenderer->mD3D11.pDxDevice->CreateTexture1D(&desc, NULL, &pTex1D));
				pTexture->mD3D11.pDxResource = pTex1D;
				break;
			}
			case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			{
				ID3D11Texture2D*     pTex2D;
				D3D11_TEXTURE2D_DESC desc = {};
				desc.ArraySize = pDesc->mArraySize;
				desc.BindFlags = util_determine_dx_bind_flags(descriptors, pDesc->mStartState);
				desc.CPUAccessFlags = 0;
				desc.Format =
					featureLevel11 ? (DXGI_FORMAT)TinyImageFormat_DXGI_FORMATToTypeless((TinyImageFormat_DXGI_FORMAT)dxFormat) : dxFormat;
				desc.Height = pDesc->mHeight;
				desc.MipLevels = pDesc->mMipLevels;
				desc.MiscFlags = util_determine_dx_resource_misc_flags(descriptors, pDesc->mFormat);
				desc.SampleDesc.Count = pDesc->mSampleCount;
				desc.SampleDesc.Quality = pDesc->mSampleQuality;
				desc.Usage = util_to_dx_usage(RESOURCE_MEMORY_USAGE_GPU_ONLY);
				desc.Width = pDesc->mWidth;
				CHECK_HRESULT_DEVICE(pRenderer->mD3D11.pDxDevice, pRenderer->mD3D11.pDxDevice->CreateTexture2D(&desc, NULL, &pTex2D));
				pTexture->mD3D11.pDxResource = pTex2D;
				break;
			}
			case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			{
				ID3D11Texture3D*     pTex3D;
				D3D11_TEXTURE3D_DESC desc = {};
				desc.BindFlags = util_determine_dx_bind_flags(descriptors, pDesc->mStartState);
				desc.CPUAccessFlags = 0;
				desc.Depth = pDesc->mDepth;
				desc.Format =
					featureLevel11 ? (DXGI_FORMAT)TinyImageFormat_DXGI_FORMATToTypeless((TinyImageFormat_DXGI_FORMAT)dxFormat) : dxFormat;
				desc.Height = pDesc->mHeight;
				desc.MipLevels = pDesc->mMipLevels;
				desc.MiscFlags = util_determine_dx_resource_misc_flags(descriptors, pDesc->mFormat);
				if (pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_BIT)
					desc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
				desc.Usage = util_to_dx_usage(RESOURCE_MEMORY_USAGE_GPU_ONLY);
				desc.Width = pDesc->mWidth;
				CHECK_HRESULT_DEVICE(pRenderer->mD3D11.pDxDevice, pRenderer->mD3D11.pDxDevice->CreateTexture3D(&desc, NULL, &pTex3D));
				pTexture->mD3D11.pDxResource = pTex3D;
				break;
			}
			default: break;
		}
	}
	else
	{
		D3D11_RESOURCE_DIMENSION type = {};
		pTexture->mD3D11.pDxResource->GetType(&type);
		switch (type)
		{
			case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			{
				D3D11_TEXTURE1D_DESC desc = {};
				((ID3D11Texture1D*)pTexture->mD3D11.pDxResource)->GetDesc(&desc);
				dxFormat = desc.Format;
				break;
			}
			case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			{
				D3D11_TEXTURE2D_DESC desc = {};
				((ID3D11Texture2D*)pTexture->mD3D11.pDxResource)->GetDesc(&desc);
				dxFormat = desc.Format;
				break;
			}
			case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			{
				D3D11_TEXTURE3D_DESC desc = {};
				((ID3D11Texture3D*)pTexture->mD3D11.pDxResource)->GetDesc(&desc);
				dxFormat = desc.Format;
				break;
			}
			default: break;
		}
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC  srvDesc = {};
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	D3D11_RESOURCE_DIMENSION type = {};
	pTexture->mD3D11.pDxResource->GetType(&type);

	switch (type)
	{
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		{
			D3D11_TEXTURE1D_DESC desc = {};
			((ID3D11Texture1D*)pTexture->mD3D11.pDxResource)->GetDesc(&desc);

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
			((ID3D11Texture2D*)pTexture->mD3D11.pDxResource)->GetDesc(&desc);
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
			((ID3D11Texture3D*)pTexture->mD3D11.pDxResource)->GetDesc(&desc);

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

		srvDesc.Format = util_to_dx11_srv_format(dxFormat);
		pRenderer->mD3D11.pDxDevice->CreateShaderResourceView(pTexture->mD3D11.pDxResource, &srvDesc, &pTexture->mD3D11.pDxSRVDescriptor);
	}

	if (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
	{
		uavDesc.Format = util_to_dx11_uav_format(dxFormat);
		for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
		{
			uavDesc.Texture1DArray.MipSlice = i;
			pRenderer->mD3D11.pDxDevice->CreateUnorderedAccessView(
				pTexture->mD3D11.pDxResource, &uavDesc, &pTexture->mD3D11.pDxUAVDescriptors[i]);
		}
	}

	if (pDesc->pName)
	{
		setTextureName(pRenderer, pTexture, pDesc->pName);
	}

	pTexture->mNodeIndex = pDesc->mNodeIndex;
	pTexture->mUav = descriptors & DESCRIPTOR_TYPE_RW_TEXTURE;
	pTexture->mMipLevels = pDesc->mMipLevels;
	pTexture->mWidth = pDesc->mWidth;
	pTexture->mHeight = pDesc->mHeight;
	pTexture->mDepth = pDesc->mDepth;
	pTexture->mArraySizeMinusOne = pDesc->mArraySize - 1;
	pTexture->mFormat = pDesc->mFormat;

	*ppTexture = pTexture;
}

void d3d11_removeTexture(Renderer* pRenderer, Texture* pTexture)
{
	ASSERT(pRenderer);
	ASSERT(pTexture);

	SAFE_RELEASE(pTexture->mD3D11.pDxSRVDescriptor);
	if (pTexture->mD3D11.pDxUAVDescriptors)
	{
		for (uint32_t i = 0; i < pTexture->mMipLevels; ++i)
			SAFE_RELEASE(pTexture->mD3D11.pDxUAVDescriptors[i]);
	}

	if (pTexture->mOwnsImage)
		SAFE_RELEASE(pTexture->mD3D11.pDxResource);

	SAFE_FREE(pTexture);
}

void d3d11_addVirtualTexture(Cmd* pCmd, const TextureDesc* pDesc, Texture** ppTexture, void* pImageData) {}

void d3d11_removeVirtualTexture(Renderer* pRenderer, VirtualTexture* pSvt) {}

/************************************************************************/
// Pipeline Functions
/************************************************************************/
void d3d11_addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
{
	ASSERT(pRenderer);
	ASSERT(pRootSignatureDesc);
	ASSERT(ppRootSignature);

	typedef struct StaticSamplerInfo
	{
		DescriptorInfo* pDescriptorInfo;
		Sampler*		pSampler;
	}StaticSamplerInfo;

	StaticSamplerInfo* staticSamplers = NULL;
	arrsetcap(staticSamplers, pRootSignatureDesc->mStaticSamplerCount);
	ShaderResource*		shaderResources = NULL;
	uint32_t*			constantSizes = NULL;
	arrsetcap(shaderResources, 64);
	arrsetcap(constantSizes, 64);

	ShaderStage			shaderStages = SHADER_STAGE_NONE;
	bool				useInputLayout = false;
	DescriptorIndexMap*	indexMap = NULL;
	PipelineType		pipelineType = PIPELINE_TYPE_UNDEFINED;
	sh_new_arena(indexMap);

	typedef struct StaticSamplerNode
	{
		char* key;
		Sampler* value;
	}StaticSamplerNode;

	StaticSamplerNode* staticSamplerMap = NULL;
	sh_new_arena(staticSamplerMap);
	for (uint32_t i = 0; i < pRootSignatureDesc->mStaticSamplerCount; ++i)
		shput(staticSamplerMap, pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i]);

	// Collect all unique shader resources in the given shaders
	// Resources are parsed by name (two resources named "XYZ" in two shaders will be considered the same resource)
	for (uint32_t sh = 0; sh < pRootSignatureDesc->mShaderCount; ++sh)
	{
		PipelineReflection const* pReflection = pRootSignatureDesc->ppShaders[sh]->pReflection;

		// KEep track of the used pipeline stages
		shaderStages |= pReflection->mShaderStages;

		if (pReflection->mShaderStages & SHADER_STAGE_COMP)
			pipelineType = PIPELINE_TYPE_COMPUTE;
		else
			pipelineType = PIPELINE_TYPE_GRAPHICS;

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

			DescriptorIndexMap* pNode = shgetp_null(indexMap, pRes->name);

			// Find all unique resources
			if (pNode == NULL)
			{
				shput(indexMap, pRes->name, (uint32_t)arrlenu(shaderResources));
				arrpush(shaderResources, *pRes);

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
				arrpush(constantSizes, constantSize);
			}
			// If the resource was already collected, just update the shader stage mask in case it is used in a different
			// shader stage in this case
			else
			{
				if (shaderResources[pNode->value].reg != pRes->reg) //-V::522, 595
				{
					LOGF(
						eERROR,
						"\nFailed to create root signature\n"
						"Shared shader resource %s has mismatching register. All shader resources "
						"shared by multiple shaders specified in addRootSignature "
						"have the same register and space",
						pRes->name);
					return;
				}
				if (shaderResources[pNode->value].set != pRes->set) //-V::522, 595
				{
					LOGF(
						eERROR,
						"\nFailed to create root signature\n"
						"Shared shader resource %s has mismatching space. All shader resources "
						"shared by multiple shaders specified in addRootSignature "
						"have the same register and space",
						pRes->name);
					return;
				}

				for (ptrdiff_t i = 0; i < arrlen(shaderResources); ++i)
				{
					//ShaderResource* pRes = &shaderResources[i];
					if (strcmp(shaderResources[i].name, pNode->key) == 0)
					{
						shaderResources[i].used_stages |= pRes->used_stages;
						break;
					}
				}
			}
		}
	}

	size_t totalSize = sizeof(RootSignature);
	totalSize += arrlenu(shaderResources) * sizeof(DescriptorInfo);
	// If any of the asserts below fail that means that alig
	COMPILE_ASSERT(alignof(DescriptorInfo) <= alignof(RootSignature));
	COMPILE_ASSERT(alignof(DescriptorIndexMap) <= alignof(DescriptorInfo));

	RootSignature* pRootSignature = (RootSignature*)tf_calloc_memalign(1, alignof(RootSignature), totalSize);
	ASSERT(pRootSignature);

	pRootSignature->mDescriptorCount = (uint32_t)arrlenu(shaderResources);

	pRootSignature->pDescriptors = (DescriptorInfo*)(pRootSignature + 1);    //-V1027
	pRootSignature->pDescriptorNameToIndexMap = indexMap;

	pRootSignature->mPipelineType = pipelineType;

	uint32_t srvCount = 0;
	uint32_t uavCount = 0;
	uint32_t cbvCount = 0;
	uint32_t samplerCount = 0;
	uint32_t dynamicCbvCount = 0;


	// Fill the descriptor array to be stored in the root signature
	for (ptrdiff_t i = 0; i < arrlen(shaderResources); ++i)
	{
		DescriptorInfo* pDesc = &pRootSignature->pDescriptors[i];
		ShaderResource* pRes = &shaderResources[i];
		uint32_t        setIndex = pRes->set;
		if (pRes->size == 0)
			setIndex = 0;

		DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)setIndex;

		pDesc->mD3D11.mReg = pRes->reg;
		pDesc->mSize = pRes->size;
		pDesc->mType = pRes->type;
		pDesc->mD3D11.mUsedStages = pRes->used_stages;
		pDesc->mUpdateFrequency = updateFreq;
		pDesc->pName = pRes->name;
		pDesc->mHandleIndex = 0;

		DescriptorType type = pRes->type;
		switch (type)
		{
			case DESCRIPTOR_TYPE_TEXTURE:
			case DESCRIPTOR_TYPE_BUFFER:
			case DESCRIPTOR_TYPE_BUFFER_RAW: pDesc->mHandleIndex = srvCount++; break;
			case DESCRIPTOR_TYPE_RW_TEXTURE:
			case DESCRIPTOR_TYPE_RW_BUFFER:
			case DESCRIPTOR_TYPE_RW_BUFFER_RAW: pDesc->mHandleIndex = uavCount++; break;
			default: break;
		}

		// Find the D3D11 type of the descriptors
		if (pDesc->mType == DESCRIPTOR_TYPE_SAMPLER)
		{
			// If the sampler is a static sampler, no need to put it in the descriptor table
			StaticSamplerNode* pNode = shgetp_null(staticSamplerMap, pDesc->pName);

			if (pNode != NULL)
			{
				LOGF(LogLevel::eINFO, "Descriptor (%s) : User specified Static Sampler", pDesc->pName);
				// Set the index to invalid value so we can use this later for error checking if user tries to update a static sampler
				pDesc->mStaticSampler = true;
				StaticSamplerInfo info = { pDesc, pNode->value };
				arrpush(staticSamplers, info);
			}
			else
			{
				pDesc->mHandleIndex = samplerCount++;
			}
		}
		// No support for arrays of constant buffers to be used as root descriptors as this might bloat the root signature size
		else if (pDesc->mType == DESCRIPTOR_TYPE_UNIFORM_BUFFER && pDesc->mSize == 1)
		{
			// D3D11 has no special syntax to declare root constants like Vulkan
			// So we assume that all constant buffers with the word "rootconstant" (case insensitive) are root constants
			if (isDescriptorRootConstant(pRes->name))
			{
				// Make the root param a 32 bit constant if the user explicitly specifies it in the shader
				pDesc->mType = DESCRIPTOR_TYPE_ROOT_CONSTANT;
				pDesc->mSize = constantSizes[i] / sizeof(uint32_t);
			}
			else if (isDescriptorRootCbv(pRes->name))
			{
				pDesc->mRootDescriptor = true;
				pDesc->mHandleIndex = dynamicCbvCount++;
			}
		}

		if (DESCRIPTOR_TYPE_UNIFORM_BUFFER == pDesc->mType && !pDesc->mRootDescriptor)
			pDesc->mHandleIndex = cbvCount++;
	}

	pRootSignature->mD3D11.mSrvCount = srvCount;
	pRootSignature->mD3D11.mUavCount = uavCount;
	pRootSignature->mD3D11.mCbvCount = cbvCount;
	pRootSignature->mD3D11.mSamplerCount = samplerCount;
	pRootSignature->mD3D11.mDynamicCbvCount = dynamicCbvCount;

	pRootSignature->mD3D11.mStaticSamplerCount = (uint32_t)arrlenu(staticSamplers);
	if (pRootSignature->mD3D11.mStaticSamplerCount)
	{
		pRootSignature->mD3D11.ppStaticSamplers = (ID3D11SamplerState**)tf_calloc(arrlenu(staticSamplers), sizeof(ID3D11SamplerState*));
		pRootSignature->mD3D11.pStaticSamplerStages = (ShaderStage*)tf_calloc(arrlenu(staticSamplers), sizeof(ShaderStage));
		pRootSignature->mD3D11.pStaticSamplerSlots = (uint32_t*)tf_calloc(arrlenu(staticSamplers), sizeof(uint32_t));
		for (uint32_t i = 0; i < pRootSignature->mD3D11.mStaticSamplerCount; ++i)
		{
			pRootSignature->mD3D11.ppStaticSamplers[i] = staticSamplers[i].pSampler->mD3D11.pSamplerState;
			pRootSignature->mD3D11.pStaticSamplerStages[i] = (ShaderStage)staticSamplers[i].pDescriptorInfo->mD3D11.mUsedStages;
			pRootSignature->mD3D11.pStaticSamplerSlots[i] = staticSamplers[i].pDescriptorInfo->mD3D11.mReg;
		}
	}
	
	shfree(staticSamplerMap);
	arrfree(constantSizes);
	arrfree(shaderResources);
	arrfree(staticSamplers);

	addRootSignatureDependencies(pRootSignature, pRootSignatureDesc);

	*ppRootSignature = pRootSignature;
}

void d3d11_removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
	ASSERT(pRootSignature);

	removeRootSignatureDependencies(pRootSignature);

	shfree(pRootSignature->pDescriptorNameToIndexMap);

	SAFE_FREE(pRootSignature->mD3D11.ppStaticSamplers);
	SAFE_FREE(pRootSignature->mD3D11.pStaticSamplerStages);
	SAFE_FREE(pRootSignature->mD3D11.pStaticSamplerSlots);
	tf_free(pRootSignature);
}

void addGraphicsPipeline(Renderer* pRenderer, const GraphicsPipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(ppPipeline);
	ASSERT(pDesc);
	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);

	const Shader*       pShaderProgram = pDesc->pShaderProgram;
	const VertexLayout* pVertexLayout = pDesc->pVertexLayout;

	Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
	ASSERT(pPipeline);

	pPipeline->mD3D11.mType = PIPELINE_TYPE_GRAPHICS;

	//add to gpu
	if (pShaderProgram->mStages & SHADER_STAGE_VERT)
	{
		pPipeline->mD3D11.pDxVertexShader = pShaderProgram->mD3D11.pDxVertexShader;
	}
	if (pShaderProgram->mStages & SHADER_STAGE_FRAG)
	{
		pPipeline->mD3D11.pDxPixelShader = pShaderProgram->mD3D11.pDxPixelShader;
	}
	if (pShaderProgram->mStages & SHADER_STAGE_HULL)
	{
		pPipeline->mD3D11.pDxHullShader = pShaderProgram->mD3D11.pDxHullShader;
	}
	if (pShaderProgram->mStages & SHADER_STAGE_DOMN)
	{
		pPipeline->mD3D11.pDxDomainShader = pShaderProgram->mD3D11.pDxDomainShader;
	}
	if (pShaderProgram->mStages & SHADER_STAGE_GEOM)
	{
		pPipeline->mD3D11.pDxGeometryShader = pShaderProgram->mD3D11.pDxGeometryShader;
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
					case SEMANTIC_JOINTS: sprintf_s(name, "JOINTS"); break;
					case SEMANTIC_WEIGHTS: sprintf_s(name, "WEIGHTS"); break;
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
				ASSERT(0 != name[0]);
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
			input_elements[input_elementCount].Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(attrib->mFormat);
			input_elements[input_elementCount].InputSlot = attrib->mBinding;
			input_elements[input_elementCount].AlignedByteOffset = attrib->mOffset;
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
		pRenderer->mD3D11.pDxDevice->CreateInputLayout(
			input_elements, input_elementCount, pDesc->pShaderProgram->mD3D11.pDxInputSignature->GetBufferPointer(),
			pDesc->pShaderProgram->mD3D11.pDxInputSignature->GetBufferSize(), &pPipeline->mD3D11.pDxInputLayout);
	}

	if (pDesc->pRasterizerState)
		pPipeline->mD3D11.pRasterizerState = util_to_rasterizer_state(pRenderer, pDesc->pRasterizerState);
	else
		pRenderer->mD3D11.pDefaultRasterizerState->QueryInterface(&pPipeline->mD3D11.pRasterizerState);

	if (pDesc->pDepthState)
		pPipeline->mD3D11.pDepthState = util_to_depth_state(pRenderer, pDesc->pDepthState);
	else
		pRenderer->mD3D11.pDefaultDepthState->QueryInterface(&pPipeline->mD3D11.pDepthState);

	if (pDesc->pBlendState)
		pPipeline->mD3D11.pBlendState = util_to_blend_state(pRenderer, pDesc->pBlendState);
	else
		pRenderer->mD3D11.pDefaultBlendState->QueryInterface(&pPipeline->mD3D11.pBlendState);

	D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	switch (pDesc->mPrimitiveTopo)
	{
		case PRIMITIVE_TOPO_POINT_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
		case PRIMITIVE_TOPO_LINE_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
		case PRIMITIVE_TOPO_LINE_STRIP: topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
		case PRIMITIVE_TOPO_TRI_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
		case PRIMITIVE_TOPO_TRI_STRIP: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
		case PRIMITIVE_TOPO_PATCH_LIST:
		{
			const PipelineReflection* pReflection = pDesc->pShaderProgram->pReflection;
			uint32_t                  controlPoint = pReflection->mStageReflections[pReflection->mHullStageIndex].mNumControlPoint;
			topology = (D3D_PRIMITIVE_TOPOLOGY)(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (controlPoint - 1));
		}
		break;

		default: break;
	}
	ASSERT(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED != topology);
	pPipeline->mD3D11.mDxPrimitiveTopology = topology;

	*ppPipeline = pPipeline;
}

void d3d11_addPipeline(Renderer* pRenderer, const PipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(ppPipeline);
	ASSERT(pDesc);

	switch (pDesc->mType)
	{
		case (PIPELINE_TYPE_COMPUTE):
		{
			ASSERT(pDesc->mComputeDesc.pShaderProgram);
			ASSERT(pDesc->mComputeDesc.pRootSignature);

			Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
			ASSERT(ppPipeline);
			pPipeline->mD3D11.mType = PIPELINE_TYPE_COMPUTE;
			pPipeline->mD3D11.pDxComputeShader = pDesc->mComputeDesc.pShaderProgram->mD3D11.pDxComputeShader;
			*ppPipeline = pPipeline;
			break;
		}
		case (PIPELINE_TYPE_GRAPHICS):
		{
			addGraphicsPipeline(pRenderer, &pDesc->mGraphicsDesc, ppPipeline);
			break;
		}
		case (PIPELINE_TYPE_RAYTRACING):
		default:
		{
			ASSERT(false);
			*ppPipeline = {};
			break;
		}
	}

	addPipelineDependencies(*ppPipeline, pDesc);
}

void d3d11_removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pPipeline);

	removePipelineDependencies(pPipeline);

	//delete pipeline from device
	SAFE_RELEASE(pPipeline->mD3D11.pDxInputLayout);
	SAFE_RELEASE(pPipeline->mD3D11.pBlendState);
	SAFE_RELEASE(pPipeline->mD3D11.pDepthState);
	SAFE_RELEASE(pPipeline->mD3D11.pRasterizerState);
	SAFE_FREE(pPipeline);
}

void d3d11_addPipelineCache(Renderer*, const PipelineCacheDesc*, PipelineCache**) {}

void d3d11_removePipelineCache(Renderer*, PipelineCache*) {}

void d3d11_getPipelineCacheData(Renderer*, PipelineCache*, size_t*, void*) {}
/************************************************************************/
// Descriptor Set Implementation
/************************************************************************/
const DescriptorInfo* d3d11_get_descriptor(const RootSignature* pRootSignature, const char* pResName)
{
	DescriptorIndexMap* pNode = shgetp_null(pRootSignature->pDescriptorNameToIndexMap, pResName);

	if (pNode)
	{
		return &pRootSignature->pDescriptors[pNode->value];
	}
	else
	{
		LOGF(LogLevel::eERROR, "Invalid descriptor param (%s)", pResName);
		return NULL;
	}
}

typedef struct CBV
{
	ID3D11Buffer* pHandle;
	uint32_t      mOffset;
	uint32_t      mSize;
	ShaderStage   mStage;
	uint32_t      mBinding;
} CBV;

typedef struct DescriptorHandle
{
	void*       pHandle;
	ShaderStage mStage;
	uint32_t    mBinding;
} DescriptorHandle;

typedef struct DescriptorDataArray
{
	struct DescriptorHandle* pSRVs;
	struct DescriptorHandle* pUAVs;
	struct CBV*              pCBVs;
	struct DescriptorHandle* pSamplers;
} DescriptorDataArray;

void d3d11_addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppDescriptorSet);

	const RootSignature* pRootSignature = pDesc->pRootSignature;

	size_t totalSize = sizeof(DescriptorSet);
	totalSize += pDesc->mMaxSets * sizeof(DescriptorDataArray);
	totalSize += pDesc->mMaxSets * pRootSignature->mD3D11.mSrvCount * sizeof(DescriptorHandle);
	totalSize += pDesc->mMaxSets * pRootSignature->mD3D11.mUavCount * sizeof(DescriptorHandle);
	totalSize += pDesc->mMaxSets * pRootSignature->mD3D11.mSamplerCount * sizeof(DescriptorHandle);
	totalSize += pDesc->mMaxSets * pRootSignature->mD3D11.mCbvCount * sizeof(CBV);

	DescriptorSet* pDescriptorSet = (DescriptorSet*)tf_calloc_memalign(1, alignof(DescriptorSet), totalSize);
	pDescriptorSet->mD3D11.mMaxSets = pDesc->mMaxSets;
	pDescriptorSet->mD3D11.pRootSignature = pRootSignature;

	pDescriptorSet->mD3D11.pHandles = (DescriptorDataArray*)(pDescriptorSet + 1);    //-V1027

	uint8_t* mem = (uint8_t*)(pDescriptorSet->mD3D11.pHandles + pDesc->mMaxSets);

	for (uint32_t i = 0; i < pDesc->mMaxSets; ++i)
	{
		pDescriptorSet->mD3D11.pHandles[i].pSRVs = (DescriptorHandle*)mem;
		mem += pRootSignature->mD3D11.mSrvCount * sizeof(DescriptorHandle);

		pDescriptorSet->mD3D11.pHandles[i].pUAVs = (DescriptorHandle*)mem;
		mem += pRootSignature->mD3D11.mUavCount * sizeof(DescriptorHandle);

		pDescriptorSet->mD3D11.pHandles[i].pSamplers = (DescriptorHandle*)mem;
		mem += pRootSignature->mD3D11.mSamplerCount * sizeof(DescriptorHandle);

		pDescriptorSet->mD3D11.pHandles[i].pCBVs = (CBV*)mem;
		mem += pRootSignature->mD3D11.mCbvCount * sizeof(CBV);
	}

	*ppDescriptorSet = pDescriptorSet;
}

void d3d11_removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
{
	SAFE_FREE(pDescriptorSet);
}

#if defined(ENABLE_GRAPHICS_DEBUG)
#define VALIDATE_DESCRIPTOR(descriptor, ...)                                                            \
	if (!(descriptor))                                                                                  \
	{                                                                                                   \
		LOGF(LogLevel::eERROR,  __FUNCTION__ " : " __VA_ARGS__);                                        \
		_FailedAssert(__FILE__, __LINE__, __FUNCTION__);												\
		continue;                                                                                       \
	}
#else
#define VALIDATE_DESCRIPTOR(descriptor, ...)
#endif

void d3d11_updateDescriptorSet(
	Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
	ASSERT(pRenderer);
	ASSERT(pDescriptorSet);
	ASSERT(index < pDescriptorSet->mD3D11.mMaxSets);

	const RootSignature* pRootSignature = pDescriptorSet->mD3D11.pRootSignature;

	for (uint32_t i = 0; i < count; ++i)
	{
		const DescriptorData* pParam = pParams + i;
		uint32_t              paramIndex = pParam->mBindByIndex ? pParam->mIndex : UINT32_MAX;
		const DescriptorInfo* pDesc =
			(paramIndex != UINT32_MAX) ? (pRootSignature->pDescriptors + paramIndex) : d3d11_get_descriptor(pRootSignature, pParam->pName);

		if (!pDesc)
		{
			LOGF(LogLevel::eERROR, "Unable to get DescriptorInfo for descriptor param %u", i);
			continue;
		}

		paramIndex = pDesc->mHandleIndex;
		const DescriptorType type = (DescriptorType)pDesc->mType;
		const uint32_t       arrayStart = pParam->mArrayOffset;
		const uint32_t       arrayCount = max(1U, pParam->mCount);

		switch (type)
		{
			case DESCRIPTOR_TYPE_SAMPLER:
			{
				// Index is invalid when descriptor is a static sampler
				VALIDATE_DESCRIPTOR(
					!pDesc->mStaticSampler,
					"Trying to update a static sampler (%s). All static samplers must be set in addRootSignature and cannot be updated "
					"later",
					pDesc->pName);

				VALIDATE_DESCRIPTOR(pParam->ppSamplers, "NULL Sampler (%s)", pDesc->pName);

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppSamplers[arr], "NULL Sampler (%s [%u] )", pDesc->pName, arr);

					pDescriptorSet->mD3D11.pHandles[index].pSamplers[paramIndex + arrayStart + arr] = (DescriptorHandle{
						pParam->ppSamplers[arr]->mD3D11.pSamplerState, (ShaderStage)pDesc->mD3D11.mUsedStages, pDesc->mD3D11.mReg + arrayStart + arr });
				}
				break;
			}
			case DESCRIPTOR_TYPE_TEXTURE:
			{
				VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)", pDesc->pName);

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);

					pDescriptorSet->mD3D11.pHandles[index].pSRVs[paramIndex + arrayStart + arr] =
						(DescriptorHandle{ pParam->ppTextures[arr]->mD3D11.pDxSRVDescriptor, (ShaderStage)pDesc->mD3D11.mUsedStages,
										   pDesc->mD3D11.mReg + arrayStart + arr });
				}
				break;
			}
			case DESCRIPTOR_TYPE_RW_TEXTURE:
			{
				VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL RW Texture (%s)", pDesc->pName);
				const uint32_t mipSlice = pParam->mUAVMipSlice;

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL RW Texture (%s [%u] )", pDesc->pName, arr);
					VALIDATE_DESCRIPTOR(
						mipSlice < pParam->ppTextures[arr]->mMipLevels, "Descriptor : (%s [%u] ) Mip Slice (%u) exceeds mip levels (%u)",
						pDesc->pName, arr, mipSlice, pParam->ppTextures[arr]->mMipLevels);

					pDescriptorSet->mD3D11.pHandles[index].pUAVs[paramIndex + arrayStart + arr] =
						(DescriptorHandle{ pParam->ppTextures[arr]->mD3D11.pDxUAVDescriptors[pParam->mUAVMipSlice],
										   (ShaderStage)pDesc->mD3D11.mUsedStages, pDesc->mD3D11.mReg + arrayStart + arr });
				}
				break;
			}
			case DESCRIPTOR_TYPE_BUFFER:
			case DESCRIPTOR_TYPE_BUFFER_RAW:
			{
				VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Buffer (%s)", pDesc->pName);

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Buffer (%s [%u] )", pDesc->pName, arr);

					pDescriptorSet->mD3D11.pHandles[index].pSRVs[paramIndex + arrayStart + arr] = (DescriptorHandle{
						pParam->ppBuffers[arr]->mD3D11.pDxSrvHandle, (ShaderStage)pDesc->mD3D11.mUsedStages, pDesc->mD3D11.mReg + arrayStart + arr });
				}
				break;
			}
			case DESCRIPTOR_TYPE_RW_BUFFER:
			case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
			{
				VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Buffer (%s)", pDesc->pName);

				for (uint32_t arr = 0; arr < arrayCount; ++arr)
				{
					VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Buffer (%s [%u] )", pDesc->pName, arr);

					pDescriptorSet->mD3D11.pHandles[index].pUAVs[paramIndex + arrayStart + arr] = (DescriptorHandle{
						pParam->ppBuffers[arr]->mD3D11.pDxUavHandle, (ShaderStage)pDesc->mD3D11.mUsedStages, pDesc->mD3D11.mReg + arrayStart + arr });
				}
				break;
			}
			case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			{
				VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Uniform Buffer (%s)", pDesc->pName);

				if (pDesc->mRootDescriptor)
				{
					VALIDATE_DESCRIPTOR(
						false,
						"Descriptor (%s) - Trying to update a root cbv through updateDescriptorSet. All root cbvs must be updated through cmdBindDescriptorSetWithRootCbvs",
						pDesc->pName);
				}
				else
				{
					for (uint32_t arr = 0; arr < arrayCount; ++arr)
					{
						VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Uniform Buffer (%s [%u] )", pDesc->pName, arr);
						if (pParam->pRanges)
						{
							DescriptorDataRange range = pParam->pRanges[arr];
							VALIDATE_DESCRIPTOR(range.mSize > 0, "Descriptor (%s) - pRanges[%u].mSize is zero", pDesc->pName, arr);
							VALIDATE_DESCRIPTOR(
								range.mSize <= D3D11_REQ_CONSTANT_BUFFER_SIZE, "Descriptor (%s) - pRanges[%u].mSize is %u which exceeds max size %u",
								pDesc->pName, arr, range.mSize, D3D11_REQ_CONSTANT_BUFFER_SIZE);
						}

						pDescriptorSet->mD3D11.pHandles[index].pCBVs[paramIndex + arrayStart + arr] = (CBV{
							pParam->ppBuffers[arr]->mD3D11.pDxResource,
							pParam->pRanges ? (uint32_t)pParam->pRanges[arr].mOffset : 0U,
							pParam->pRanges ? (uint32_t)pParam->pRanges[arr].mSize : 0U,
							(ShaderStage)pDesc->mD3D11.mUsedStages,
							pDesc->mD3D11.mReg + arrayStart + arr,
						});
					}
				}
				break;
			}
			default: break;
		}
	}
}
/************************************************************************/
// Command buffer Functions
/************************************************************************/
void d3d11_resetCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	UNREF_PARAM(pRenderer);
	UNREF_PARAM(pCmdPool);
}

void d3d11_beginCmd(Cmd* pCmd)
{
	//TODO: Maybe put lock here if required.
}

void d3d11_endCmd(Cmd* pCmd)
{
	//TODO: Maybe put unlock here if required.
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

// Beginning of the rendering pass.
void d3d11_cmdBindRenderTargets(
	Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil,
	const LoadActionsDesc* pLoadActions /* = NULL*/, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice,
	uint32_t depthMipSlice)
{
	ASSERT(pCmd);

	if (!renderTargetCount && !pDepthStencil)
		return;

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	// Reset any shader settings left from previous render passes.
	reset_shader_resources(pContext);

	// Remove all rendertargets from previous render passes.
	static ID3D11RenderTargetView* pNullRTV[MAX_RENDER_TARGET_ATTACHMENTS] = { NULL };
	pContext->OMSetRenderTargets(MAX_RENDER_TARGET_ATTACHMENTS, pNullRTV, NULL);

	ID3D11RenderTargetView* ppDxTargets[MAX_RENDER_TARGET_ATTACHMENTS] = { 0 };

	// Make a list of d3d11 rendertargets to bind.
	for (uint32_t i = 0; i < renderTargetCount; ++i)
	{
		if (!pColorMipSlices && !pColorArraySlices)
		{
			ppDxTargets[i] = ppRenderTargets[i]->mD3D11.pDxRtvDescriptor;
		}
		else
		{
			uint32_t handle = 0;
			if (pColorMipSlices)
			{
				if (pColorArraySlices)
				{
					handle = pColorMipSlices[i] * ppRenderTargets[i]->mArraySize + pColorArraySlices[i];
				}
				else
				{
					handle = pColorMipSlices[i];
				}
			}
			else if (pColorArraySlices)
			{
				handle = pColorArraySlices[i];
			}

			ppDxTargets[i] = ppRenderTargets[i]->mD3D11.pDxRtvSliceDescriptors[handle];
		}
	}

	// Get the d3d11 depth/stencil target to bind.
	ID3D11DepthStencilView* pDxDepthStencilTarget = 0;

	if (pDepthStencil)
	{
		if (-1 == depthMipSlice && -1 == depthArraySlice)
		{
			pDxDepthStencilTarget = pDepthStencil->mD3D11.pDxDsvDescriptor;
		}
		else
		{
			uint32_t handle = 0;
			if (depthMipSlice != -1)
			{
				if (depthArraySlice != -1)
					handle = depthMipSlice * pDepthStencil->mArraySize + depthArraySlice;
				else
					handle = depthMipSlice;
			}
			else if (depthArraySlice != -1)
			{
				handle = depthArraySlice;
			}
			pDxDepthStencilTarget = pDepthStencil->mD3D11.pDxDsvSliceDescriptors[handle];
		}
	}

	// Bind the render targets and the depth-stencil target.
	pContext->OMSetRenderTargets(renderTargetCount, ppDxTargets, pDxDepthStencilTarget);

	// Clear RTs and Depth-Stencil views bound to the render pass.
	LoadActionsDesc targetLoadActions = {};

	if (pLoadActions)
	{
		targetLoadActions = *pLoadActions;
	}

	for (uint32_t i = 0; i < renderTargetCount; ++i)
	{
		if (targetLoadActions.mLoadActionsColor[i] == LOAD_ACTION_CLEAR)
		{
			FLOAT clear[4] = { targetLoadActions.mClearColorValues[i].r, targetLoadActions.mClearColorValues[i].g,
							   targetLoadActions.mClearColorValues[i].b, targetLoadActions.mClearColorValues[i].a };

			pContext->ClearRenderTargetView(ppDxTargets[i], clear);
		}
	}

	if (pDxDepthStencilTarget &&
		(targetLoadActions.mLoadActionDepth == LOAD_ACTION_CLEAR || targetLoadActions.mLoadActionStencil == LOAD_ACTION_CLEAR))
	{
		uint32_t clearFlag = 0;
		if (targetLoadActions.mLoadActionDepth == LOAD_ACTION_CLEAR)
			clearFlag |= D3D11_CLEAR_DEPTH;
		if (targetLoadActions.mLoadActionStencil == LOAD_ACTION_CLEAR)
			clearFlag |= D3D11_CLEAR_STENCIL;
		pContext->ClearDepthStencilView(
			pDxDepthStencilTarget, clearFlag, targetLoadActions.mClearDepth.depth, targetLoadActions.mClearDepth.stencil);
	}
}

void d3d11_cmdSetShadingRate(
	Cmd* pCmd, ShadingRate shadingRate, Texture* pTexture, ShadingRateCombiner postRasterizerRate, ShadingRateCombiner finalRate)
{
}

void d3d11_cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
	ASSERT(pCmd);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	D3D11_VIEWPORT viewport = {};
	viewport.Width = width;
	viewport.Height = height;
	viewport.MaxDepth = maxDepth;
	viewport.MinDepth = minDepth;
	viewport.TopLeftX = x;
	viewport.TopLeftY = y;
	pContext->RSSetViewports(1, &viewport);
}

void d3d11_cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	ASSERT(pCmd);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	D3D11_RECT scissor = {};
	scissor.left = x;
	scissor.top = y;
	scissor.right = x + width;
	scissor.bottom = y + height;
	pContext->RSSetScissorRects(1, &scissor);
}

void d3d11_cmdSetStencilReferenceValue(Cmd* pCmd, uint32_t val)
{
	ASSERT(pCmd);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	ID3D11DepthStencilState* pDepth = NULL;
	uint32_t                 value = 0;

	pContext->OMGetDepthStencilState(&pDepth, &value);
	value = val;
	pContext->OMSetDepthStencilState(pDepth, val);
}

void reset_uavs(ID3D11DeviceContext* pContext)
{
	static ID3D11UnorderedAccessView* pNullUAV[D3D11_PS_CS_UAV_REGISTER_COUNT] = { NULL };
	pContext->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, pNullUAV, NULL);
}

void d3d11_cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
	ASSERT(pCmd);
	ASSERT(pPipeline);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	static const float dummyClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	if (pCmd->pRenderer->mD3D11.mFeatureLevel >= D3D_FEATURE_LEVEL_11_0)
	{
		reset_uavs(pContext);
	}

	if (pPipeline->mD3D11.mType == PIPELINE_TYPE_GRAPHICS)
	{
		// Set InputAssembly, Raster and Output state settings.
		pContext->IASetPrimitiveTopology(pPipeline->mD3D11.mDxPrimitiveTopology);
		pContext->IASetInputLayout(pPipeline->mD3D11.pDxInputLayout);
		pContext->RSSetState(pPipeline->mD3D11.pRasterizerState);
		pContext->OMSetDepthStencilState(pPipeline->mD3D11.pDepthState, 0);
		pContext->OMSetBlendState(pPipeline->mD3D11.pBlendState, dummyClearColor, ~0);

		// Set shaders.
		pContext->VSSetShader(pPipeline->mD3D11.pDxVertexShader, NULL, 0);
		pContext->PSSetShader(pPipeline->mD3D11.pDxPixelShader, NULL, 0);
		pContext->GSSetShader(pPipeline->mD3D11.pDxGeometryShader, NULL, 0);
		pContext->DSSetShader(pPipeline->mD3D11.pDxDomainShader, NULL, 0);
		pContext->HSSetShader(pPipeline->mD3D11.pDxHullShader, NULL, 0);
	}
	else
	{
		pContext->RSSetState(NULL);
		pContext->OMSetDepthStencilState(NULL, 0);
		pContext->OMSetBlendState(NULL, dummyClearColor, ~0);
		reset_shader_resources(pContext);
		pContext->CSSetShader(pPipeline->mD3D11.pDxComputeShader, NULL, 0);
	}
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

void set_constant_buffer_offset(ID3D11DeviceContext1* pContext1, ShaderStage used_stages, uint32_t reg, uint32_t offset, uint32_t size, ID3D11Buffer* pCBV)
{
	ASSERT(pContext1);
	// https://docs.microsoft.com/en-us/windows/win32/api/d3d11_1/nf-d3d11_1-id3d11devicecontext1-pssetconstantbuffers1
	uint32_t firstConstant = offset / 16;
	// Offset and Count must be multiples of 16
	uint32_t constantCount = round_up(size / 16, 16);

	if (used_stages & SHADER_STAGE_VERT)
		pContext1->VSSetConstantBuffers1(reg, 1, &pCBV, &firstConstant, &constantCount);
	if (used_stages & SHADER_STAGE_FRAG)
		pContext1->PSSetConstantBuffers1(reg, 1, &pCBV, &firstConstant, &constantCount);
	if (used_stages & SHADER_STAGE_HULL)
		pContext1->HSSetConstantBuffers1(reg, 1, &pCBV, &firstConstant, &constantCount);
	if (used_stages & SHADER_STAGE_DOMN)
		pContext1->DSSetConstantBuffers1(reg, 1, &pCBV, &firstConstant, &constantCount);
	if (used_stages & SHADER_STAGE_GEOM)
		pContext1->GSSetConstantBuffers1(reg, 1, &pCBV, &firstConstant, &constantCount);
	if (used_stages & SHADER_STAGE_COMP)
		pContext1->CSSetConstantBuffers1(reg, 1, &pCBV, &firstConstant, &constantCount);
}

void d3d11_cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
{
	ASSERT(pCmd);
	ASSERT(pDescriptorSet);
	ASSERT(index < pDescriptorSet->mD3D11.mMaxSets);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;
	ID3D11DeviceContext1* pContext1 = pRenderer->mD3D11.pDxContext1;

	const RootSignature* pRootSignature = pDescriptorSet->mD3D11.pRootSignature;

	// Texture samplers.
	for (uint32_t i = 0; i < pRootSignature->mD3D11.mStaticSamplerCount; ++i)
	{
		set_samplers(
			pContext, pRootSignature->mD3D11.pStaticSamplerStages[i], pRootSignature->mD3D11.pStaticSamplerSlots[i], 1,
			&pRootSignature->mD3D11.ppStaticSamplers[i]);
	}

	// Shader resources.
	for (uint32_t i = 0; i < pRootSignature->mD3D11.mSrvCount; ++i)
	{
		DescriptorHandle* pHandle = &pDescriptorSet->mD3D11.pHandles[index].pSRVs[i];
		if (pHandle->pHandle)
			set_shader_resources(pContext, pHandle->mStage, pHandle->mBinding, 1, (ID3D11ShaderResourceView**)&pHandle->pHandle);
	}

	// UAVs
	for (uint32_t i = 0; i < pRootSignature->mD3D11.mUavCount; ++i)
	{
		DescriptorHandle* pHandle = &pDescriptorSet->mD3D11.pHandles[index].pUAVs[i];
		if (pHandle->pHandle)
			pContext->CSSetUnorderedAccessViews(pHandle->mBinding, 1, (ID3D11UnorderedAccessView**)&pHandle->pHandle, NULL);
	}

	// Constant buffers(Uniform buffers)
	for (uint32_t i = 0; i < pRootSignature->mD3D11.mCbvCount; ++i)
	{
		CBV* pHandle = &pDescriptorSet->mD3D11.pHandles[index].pCBVs[i];
		if (pHandle->pHandle)
		{
			if (pHandle->mOffset || pHandle->mSize)
			{
				ASSERT(pContext1);
				set_constant_buffer_offset(pContext1, pHandle->mStage, pHandle->mBinding, pHandle->mOffset, pHandle->mSize, pHandle->pHandle);
			}
			else
			{
				set_constant_buffers(pContext, pHandle->mStage, pHandle->mBinding, 1, (ID3D11Buffer**)&pHandle->pHandle);
			}
		}
	}

	for (uint32_t i = 0; i < pRootSignature->mD3D11.mSamplerCount; ++i)
	{
		DescriptorHandle* pHandle = &pDescriptorSet->mD3D11.pHandles[index].pSamplers[i];
		if (pHandle->pHandle)
			set_samplers(pContext, pHandle->mStage, pHandle->mBinding, 1, (ID3D11SamplerState**)&pHandle->pHandle);
	}
}

void d3d11_cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
{
	ASSERT(pCmd);
	ASSERT(pConstants);
	ASSERT(pRootSignature);
	ASSERT(paramIndex >= 0 && paramIndex < pRootSignature->mDescriptorCount);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	if (!pCmd->mD3D11.pRootConstantBuffer)
	{
		Buffer*    buffer = {};
		BufferDesc bufDesc = {};
		bufDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bufDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		bufDesc.mSize = 256;
		addBuffer(pCmd->pRenderer, &bufDesc, &buffer);
		pCmd->mD3D11.pRootConstantBuffer = buffer->mD3D11.pDxResource;
		SAFE_FREE(buffer);
	}

	D3D11_MAPPED_SUBRESOURCE sub = {};
	const DescriptorInfo*    pDesc = pRootSignature->pDescriptors + paramIndex;

	pContext->Map(pCmd->mD3D11.pRootConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
	memcpy(sub.pData, pConstants, pDesc->mSize * sizeof(uint32_t));
	pContext->Unmap(pCmd->mD3D11.pRootConstantBuffer, 0);
	set_constant_buffers(pContext, (ShaderStage)pDesc->mD3D11.mUsedStages, pDesc->mD3D11.mReg, 1, &pCmd->mD3D11.pRootConstantBuffer);
}

void d3d11_cmdBindDescriptorSetWithRootCbvs(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
	ASSERT(pCmd);
	ASSERT(pDescriptorSet);
	ASSERT(pParams);

	d3d11_cmdBindDescriptorSet(pCmd, index, pDescriptorSet);

	const RootSignature* pRootSignature = pDescriptorSet->mD3D11.pRootSignature;
	ID3D11DeviceContext1* pContext1 = pCmd->pRenderer->mD3D11.pDxContext1;
	ASSERT(pContext1);

	for (uint32_t i = 0; i < count; ++i)
	{
		const DescriptorData* pParam = pParams + i;
		uint32_t              paramIndex = pParam->mBindByIndex ? pParam->mIndex : UINT32_MAX;
		const DescriptorInfo* pDesc =
			(paramIndex != UINT32_MAX) ? (pRootSignature->pDescriptors + paramIndex) : d3d11_get_descriptor(pRootSignature, pParam->pName);

		if (!pDesc)
		{
			LOGF(LogLevel::eERROR, "Unable to get DescriptorInfo for descriptor param %u", i);
			continue;
		}

		VALIDATE_DESCRIPTOR(pDesc->mRootDescriptor, "Descriptor (%s) - must be a root cbv", pDesc->pName);
		VALIDATE_DESCRIPTOR(pParam->mCount <= 1, "Descriptor (%s) - cmdBindDescriptorSetWithRootCbvs does not support arrays", pDesc->pName);
		VALIDATE_DESCRIPTOR(pParam->pRanges, "Descriptor (%s) - pRanges must be provided for cmdBindDescriptorSetWithRootCbvs", pDesc->pName);

		DescriptorDataRange range = pParam->pRanges[0];
		VALIDATE_DESCRIPTOR(range.mSize > 0, "Descriptor (%s) - pRange->mSize is zero", pDesc->pName);
		VALIDATE_DESCRIPTOR(
			range.mSize <= D3D11_REQ_CONSTANT_BUFFER_SIZE, "Descriptor (%s) - pRange->mSize is %u which exceeds max size %u",
			pDesc->pName, range.mSize, D3D11_REQ_CONSTANT_BUFFER_SIZE);

		set_constant_buffer_offset(pContext1, (ShaderStage)pDesc->mD3D11.mUsedStages, pDesc->mD3D11.mReg,
			range.mOffset, range.mSize, pParam->ppBuffers[0]->mD3D11.pDxResource);
	}
}

void d3d11_cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
	ASSERT(pCmd);
	ASSERT(pBuffer->mD3D11.pDxResource);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	pContext->IASetIndexBuffer(
		(ID3D11Buffer*)pBuffer->mD3D11.pDxResource, INDEX_TYPE_UINT16 == indexType ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
		(uint32_t)offset);
}

void d3d11_cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
{
	ASSERT(pCmd);
	ASSERT(bufferCount);
	ASSERT(ppBuffers);
	ASSERT(pStrides);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	ID3D11Buffer* buffers[MAX_VERTEX_BINDINGS] = {};
	uint32_t offsets[MAX_VERTEX_BINDINGS] = {};

	for (uint32_t i = 0; i < bufferCount; ++i)
	{
		buffers[i] = ppBuffers[i]->mD3D11.pDxResource;
		offsets[i] = (uint32_t)(pOffsets ? pOffsets[i] : 0);
	}

	pContext->IASetVertexBuffers(0, bufferCount, buffers, pStrides, offsets);
}

void d3d11_cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
	ASSERT(pCmd);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	pContext->Draw(vertexCount, firstVertex);
}

void d3d11_cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
	ASSERT(pCmd);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	pContext->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void d3d11_cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
	ASSERT(pCmd);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	pContext->DrawIndexed(indexCount, firstIndex, firstVertex);
}

void d3d11_cmdDrawIndexedInstanced(
	Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstInstance, uint32_t firstVertex)
{
	ASSERT(pCmd);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	pContext->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, firstVertex, firstInstance);
}

void d3d11_cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	ASSERT(pCmd);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	pContext->Dispatch(groupCountX, groupCountY, groupCountZ);
}

void d3d11_cmdUpdateVirtualTexture(Cmd* cmd, Texture* pTexture, uint32_t currentImage) {}

/************************************************************************/
// Transition Commands
/************************************************************************/
void d3d11_cmdResourceBarrier(
	Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers,
	uint32_t numRtBarriers, RenderTargetBarrier* pRtBarriers)
{
}
/************************************************************************/
// Queue Fence Semaphore Functions
/************************************************************************/
void d3d11_acquireNextImage(
	Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pSwapChainImageIndex)
{
	// We only have read/write access to the first buffer.
	pSwapChain->mD3D11.mImageIndex = 0;
	*pSwapChainImageIndex = pSwapChain->mD3D11.mImageIndex;
}

static void util_wait_for_fence(ID3D11DeviceContext* pDxContext, Fence* pFence)
{
	HRESULT hres = S_FALSE;
	while (hres != S_OK && pFence->mD3D11.mSubmitted)
	{
		hres = pDxContext->GetData(pFence->mD3D11.pDX11Query, NULL, 0, 0);
		threadSleep(0);
	}
	pFence->mD3D11.mSubmitted = false;
}

void d3d11_queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc)
{
	Fence* pFence = pDesc->pSignalFence;
	Cmd**  ppCmds = pDesc->ppCmds;
	pQueue->mD3D11.pFence = pFence;
	ID3D11DeviceContext* pContext = ppCmds[0]->pRenderer->mD3D11.pDxContext;

	if (pFence)
	{
		util_wait_for_fence(pContext, pFence);
		pFence->mD3D11.mSubmitted = true;
		pContext->End(pFence->mD3D11.pDX11Query);
	}
}

void d3d11_queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc)
{
	if (pDesc->pSwapChain)
	{
		HRESULT hr = pDesc->pSwapChain->mD3D11.pDxSwapChain->Present(pDesc->pSwapChain->mD3D11.mDxSyncInterval, 0);
		if (!SUCCEEDED(hr))
		{
			LOGF(
				eERROR, "%s: FAILED with HRESULT: %u", "pDesc->pSwapChain->pDxSwapChain->Present(pDesc->pSwapChain->mDxSyncInterval, 0)",
				(uint32_t)hr);

#if defined(_WINDOWS)
			if (hr == DXGI_ERROR_DEVICE_REMOVED)
			{
				LOGF(eERROR, "ERROR_DEVICE_REMOVED: %u", (uint32_t)pQueue->mD3D11.pDxDevice->GetDeviceRemovedReason());
				threadSleep(5000);    // Wait for a few seconds to allow the driver to come back online before doing a reset.
				ResetDesc resetDescriptor;
				resetDescriptor.mType = RESET_TYPE_DEVICE_LOST;
				requestReset(&resetDescriptor);
			}
#endif

#if defined(ENABLE_NSIGHT_AFTERMATH)
			// DXGI_ERROR error notification is asynchronous to the NVIDIA display
			// driver's GPU crash handling. Give the Nsight Aftermath GPU crash dump
			// thread some time to do its work before terminating the process.
			sleep(3000);
#endif

			ASSERT(false);
		}
	}
}

void d3d11_getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
	UNREF_PARAM(pRenderer);

	if (pFence->mD3D11.mSubmitted)
	{
		HRESULT hres = pRenderer->mD3D11.pDxContext->GetData(pFence->mD3D11.pDX11Query, NULL, 0, 0);
		if (hres == S_OK)
		{
			pFence->mD3D11.mSubmitted = false;
			*pFenceStatus = FENCE_STATUS_COMPLETE;
		}
		else
		{
			*pFenceStatus = FENCE_STATUS_INCOMPLETE;
		}
	}
	else
	{
		*pFenceStatus = FENCE_STATUS_NOTSUBMITTED;
	}
}

void d3d11_waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
{
	for (uint32_t i = 0; i < fenceCount; ++i)
	{
		util_wait_for_fence(pRenderer->mD3D11.pDxContext, ppFences[i]);
	}
}

void d3d11_waitQueueIdle(Queue* pQueue)
{
	if (pQueue && pQueue->mD3D11.pFence && pQueue->mD3D11.pFence->mD3D11.mSubmitted)
	{
		util_wait_for_fence(pQueue->mD3D11.pDxContext, pQueue->mD3D11.pFence);
	}
}

void d3d11_toggleVSync(Renderer* pRenderer, SwapChain** ppSwapChain)
{
	UNREF_PARAM(pRenderer);
	// Initial vsync value is passed in with the desc when client creates a swapchain.
	ASSERT(*ppSwapChain);
	(*ppSwapChain)->mEnableVsync = !(*ppSwapChain)->mEnableVsync;

	//toggle vsync present flag (this can go up to 4 but we don't need to refresh on nth vertical sync)
	(*ppSwapChain)->mD3D11.mDxSyncInterval = ((*ppSwapChain)->mD3D11.mDxSyncInterval + 1) % 2;
}
/************************************************************************/
// Utility functions
/************************************************************************/
TinyImageFormat d3d11_getRecommendedSwapchainFormat(bool hintHDR, bool hintSRGB) 
{
	if (hintSRGB)
		return TinyImageFormat_B8G8R8A8_SRGB;
	else
		return TinyImageFormat_B8G8R8A8_UNORM;
}
/************************************************************************/
// Indirect Draw functions
/************************************************************************/
void d3d11_addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature) {}

void d3d11_removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature) {}

void d3d11_cmdExecuteIndirect(
	Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
	Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
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
		default: ASSERT(false && "Invalid query type"); return D3D11_QUERY_OCCLUSION;
	}
}

void d3d11_getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
	ID3D11DeviceContext* pContext = pQueue->mD3D11.pDxContext;
	ID3D11Device*        pDevice = NULL;
	ID3D11Query*         pDisjointQuery = NULL;
	D3D11_QUERY_DESC     desc = {};
	desc.MiscFlags = 0;
	desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

	pContext->GetDevice(&pDevice);
	pDevice->CreateQuery(&desc, &pDisjointQuery);

	pContext->Begin(pDisjointQuery);
	pContext->End(pDisjointQuery);
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT data = {};
	while (pContext->GetData(pDisjointQuery, &data, sizeof(data), 0) != S_OK)
		threadSleep(0);
	*pFrequency = (double)data.Frequency;

	SAFE_RELEASE(pDevice);
	SAFE_RELEASE(pDisjointQuery);
}

void d3d11_addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppQueryPool);

	QueryPool* pQueryPool = (QueryPool*)tf_calloc(1, sizeof(QueryPool) + pDesc->mQueryCount * sizeof(ID3D11Query*));
	ASSERT(pQueryPool);

	pQueryPool->mCount = pDesc->mQueryCount;
	pQueryPool->mD3D11.mType = util_to_dx_query(pDesc->mType);

	pQueryPool->mD3D11.ppDxQueries = (ID3D11Query**)(pQueryPool + 1);

	D3D11_QUERY_DESC desc = {};
	desc.MiscFlags = 0;
	desc.Query = util_to_dx_query(pDesc->mType);

	for (uint32_t i = 0; i < pDesc->mQueryCount; ++i)
	{
		pRenderer->mD3D11.pDxDevice->CreateQuery(&desc, &pQueryPool->mD3D11.ppDxQueries[i]);
	}

	*ppQueryPool = pQueryPool;
}

void d3d11_removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
	UNREF_PARAM(pRenderer);
	for (uint32_t i = 0; i < pQueryPool->mCount; ++i)
	{
		SAFE_RELEASE(pQueryPool->mD3D11.ppDxQueries[i]);
	}
	SAFE_FREE(pQueryPool);
}

void d3d11_cmdResetQueryPool(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
	UNREF_PARAM(pCmd);
	UNREF_PARAM(pQueryPool);
	UNREF_PARAM(startQuery);
	UNREF_PARAM(queryCount);
}

void d3d11_cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
	ASSERT(pCmd);
	ASSERT(pQuery);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	pContext->End(pQueryPool->mD3D11.ppDxQueries[pQuery->mIndex]);
}

void d3d11_cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)    //-V524
{
	ASSERT(pCmd);
	ASSERT(pQuery);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	pContext->End(pQueryPool->mD3D11.ppDxQueries[pQuery->mIndex]);
}

void d3d11_cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
{
	ASSERT(pCmd);
	ASSERT(pReadbackBuffer);

	Renderer*            pRenderer = pCmd->pRenderer;
	ID3D11DeviceContext* pContext = pRenderer->mD3D11.pDxContext;

	if (queryCount)
	{
		D3D11_MAPPED_SUBRESOURCE sub = {};
		pContext->Map(pReadbackBuffer->mD3D11.pDxResource, 0, D3D11_MAP_READ, 0, &sub);
		uint64_t* pResults = (uint64_t*)sub.pData;

		for (uint32_t i = startQuery; i < startQuery + queryCount; ++i)
		{
			while (pContext->GetData(pQueryPool->mD3D11.ppDxQueries[i], &pResults[i], sizeof(uint64_t), 0) != S_OK)
				threadSleep(0);
		}

		pContext->Unmap(pReadbackBuffer->mD3D11.pDxResource, 0);
	}
}
/************************************************************************/
// Memory Stats Implementation
/************************************************************************/
void d3d11_calculateMemoryStats(Renderer* pRenderer, char** stats) {}

void d3d11_calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes) {}

void d3d11_freeMemoryStats(Renderer* pRenderer, char* stats) {}
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void d3d11_cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if defined(ENABLE_PERFORMANCE_MARKER)
	wchar_t markerName[256] = { 0 };
	int     nameLen = int(strlen(pName));
	MultiByteToWideChar(0, 0, pName, nameLen, markerName, nameLen);
	pCmd->pRenderer->pUserDefinedAnnotation->BeginEvent(markerName);
#endif
}

void d3d11_cmdEndDebugMarker(Cmd* pCmd)
{
#if defined(ENABLE_PERFORMANCE_MARKER)
	pCmd->pRenderer->pUserDefinedAnnotation->EndEvent();
#endif
}

void d3d11_cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if defined(ENABLE_PERFORMANCE_MARKER)
	wchar_t markerName[256] = { 0 };
	int     nameLen = int(strlen(pName));
	MultiByteToWideChar(0, 0, pName, nameLen, markerName, nameLen);
	pCmd->pRenderer->pUserDefinedAnnotation->SetMarker(markerName);
#endif

#if defined(ENABLE_NSIGHT_AFTERMATH)
	SetAftermathMarker(&pCmd->pRenderer->mAftermathTracker, pCmd->pRenderer->mD3D11.pDxContext, pName);
#endif
}

uint32_t d3d11_cmdWriteMarker(Cmd* pCmd, MarkerType markerType, uint32_t markerValue, Buffer* pBuffer, size_t offset, bool useAutoFlags)
{
	return 0;
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void d3d11_setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	ASSERT(pRenderer);
	ASSERT(pBuffer);
	ASSERT(pName);

	UNREF_PARAM(pRenderer);
	pBuffer->mD3D11.pDxResource->SetPrivateData(WKPDID_D3DDebugObjectName, (uint32_t)strlen(pName) + 1, pName);
#endif
}

void d3d11_setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG)
	ASSERT(pRenderer);
	ASSERT(pTexture);
	ASSERT(pName);

	UNREF_PARAM(pRenderer);
	pTexture->mD3D11.pDxResource->SetPrivateData(WKPDID_D3DDebugObjectName, (uint32_t)strlen(pName) + 1u, pName);
#endif
}

void d3d11_setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName)
{
	setTextureName(pRenderer, pRenderTarget->pTexture, pName);
}

void d3d11_setPipelineName(Renderer*, Pipeline*, const char*) {}
#endif

void initD3D11Renderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer)
{
	// API functions
	addFence = d3d11_addFence;
	removeFence = d3d11_removeFence;
	addSemaphore = d3d11_addSemaphore;
	removeSemaphore = d3d11_removeSemaphore;
	addQueue = d3d11_addQueue;
	removeQueue = d3d11_removeQueue;
	addSwapChain = d3d11_addSwapChain;
	removeSwapChain = d3d11_removeSwapChain;

	// command pool functions
	addCmdPool = d3d11_addCmdPool;
	removeCmdPool = d3d11_removeCmdPool;
	addCmd = d3d11_addCmd;
	removeCmd = d3d11_removeCmd;
	addCmd_n = d3d11_addCmd_n;
	removeCmd_n = d3d11_removeCmd_n;

	addRenderTarget = d3d11_addRenderTarget;
	removeRenderTarget = d3d11_removeRenderTarget;
	addSampler = d3d11_addSampler;
	removeSampler = d3d11_removeSampler;

	// Resource Load functions
	addBuffer = d3d11_addBuffer;
	removeBuffer = d3d11_removeBuffer;
	mapBuffer = d3d11_mapBuffer;
	unmapBuffer = d3d11_unmapBuffer;
	cmdUpdateBuffer = d3d11_cmdUpdateBuffer;
	cmdUpdateSubresource = d3d11_cmdUpdateSubresource;
	addTexture = d3d11_addTexture;
	removeTexture = d3d11_removeTexture;
	addVirtualTexture = d3d11_addVirtualTexture;
	removeVirtualTexture = d3d11_removeVirtualTexture;

	// shader functions
	addShaderBinary = d3d11_addShaderBinary;
	removeShader = d3d11_removeShader;

	addRootSignature = d3d11_addRootSignature;
	removeRootSignature = d3d11_removeRootSignature;

	// pipeline functions
	addPipeline = d3d11_addPipeline;
	removePipeline = d3d11_removePipeline;
	addPipelineCache = d3d11_addPipelineCache;
	getPipelineCacheData = d3d11_getPipelineCacheData;
	removePipelineCache = d3d11_removePipelineCache;

	// Descriptor Set functions
	addDescriptorSet = d3d11_addDescriptorSet;
	removeDescriptorSet = d3d11_removeDescriptorSet;
	updateDescriptorSet = d3d11_updateDescriptorSet;

	// command buffer functions
	resetCmdPool = d3d11_resetCmdPool;
	beginCmd = d3d11_beginCmd;
	endCmd = d3d11_endCmd;
	cmdBindRenderTargets = d3d11_cmdBindRenderTargets;
	cmdSetShadingRate = d3d11_cmdSetShadingRate;
	cmdSetViewport = d3d11_cmdSetViewport;
	cmdSetScissor = d3d11_cmdSetScissor;
	cmdSetStencilReferenceValue = d3d11_cmdSetStencilReferenceValue;
	cmdBindPipeline = d3d11_cmdBindPipeline;
	cmdBindDescriptorSet = d3d11_cmdBindDescriptorSet;
	cmdBindPushConstants = d3d11_cmdBindPushConstants;
	cmdBindDescriptorSetWithRootCbvs = d3d11_cmdBindDescriptorSetWithRootCbvs;
	cmdBindIndexBuffer = d3d11_cmdBindIndexBuffer;
	cmdBindVertexBuffer = d3d11_cmdBindVertexBuffer;
	cmdDraw = d3d11_cmdDraw;
	cmdDrawInstanced = d3d11_cmdDrawInstanced;
	cmdDrawIndexed = d3d11_cmdDrawIndexed;
	cmdDrawIndexedInstanced = d3d11_cmdDrawIndexedInstanced;
	cmdDispatch = d3d11_cmdDispatch;

	// Transition Commands
	cmdResourceBarrier = d3d11_cmdResourceBarrier;
	// Virtual Textures
	cmdUpdateVirtualTexture = d3d11_cmdUpdateVirtualTexture;

	// queue/fence/swapchain functions
	acquireNextImage = d3d11_acquireNextImage;
	queueSubmit = d3d11_queueSubmit;
	queuePresent = d3d11_queuePresent;
	waitQueueIdle = d3d11_waitQueueIdle;
	getFenceStatus = d3d11_getFenceStatus;
	waitForFences = d3d11_waitForFences;
	toggleVSync = d3d11_toggleVSync;

	getRecommendedSwapchainFormat = d3d11_getRecommendedSwapchainFormat;

	//indirect Draw functions
	addIndirectCommandSignature = d3d11_addIndirectCommandSignature;
	removeIndirectCommandSignature = d3d11_removeIndirectCommandSignature;
	cmdExecuteIndirect = d3d11_cmdExecuteIndirect;

	/************************************************************************/
	// GPU Query Interface
	/************************************************************************/
	getTimestampFrequency = d3d11_getTimestampFrequency;
	addQueryPool = d3d11_addQueryPool;
	removeQueryPool = d3d11_removeQueryPool;
	cmdResetQueryPool = d3d11_cmdResetQueryPool;
	cmdBeginQuery = d3d11_cmdBeginQuery;
	cmdEndQuery = d3d11_cmdEndQuery;
	cmdResolveQuery = d3d11_cmdResolveQuery;
	/************************************************************************/
	// Stats Info Interface
	/************************************************************************/
	calculateMemoryStats = d3d11_calculateMemoryStats;
	calculateMemoryUse = d3d11_calculateMemoryUse;
	freeMemoryStats = d3d11_freeMemoryStats;
	/************************************************************************/
	// Debug Marker Interface
	/************************************************************************/
	cmdBeginDebugMarker = d3d11_cmdBeginDebugMarker;
	cmdEndDebugMarker = d3d11_cmdEndDebugMarker;
	cmdAddDebugMarker = d3d11_cmdAddDebugMarker;
	cmdWriteMarker = d3d11_cmdWriteMarker;
	/************************************************************************/
	// Resource Debug Naming Interface
	/************************************************************************/
	setBufferName = d3d11_setBufferName;
	setTextureName = d3d11_setTextureName;
	setRenderTargetName = d3d11_setRenderTargetName;
	setPipelineName = d3d11_setPipelineName;

	d3d11_initRenderer(appName, pSettings, ppRenderer);
}

void exitD3D11Renderer(Renderer* pRenderer)
{
	ASSERT(pRenderer); 

	d3d11_exitRenderer(pRenderer);
}
#endif

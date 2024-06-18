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

/* Lightweight D3D11 Fallback layer.
 * This implementation retrofits the new low level interface to D3D11.
 * TODO: explain how GPU resource dependencies are handled...
 */
#include "../GraphicsConfig.h"

#ifdef DIRECT3D11
#define RENDERER_IMPLEMENTATION
#define IID_ARGS IID_PPV_ARGS

#include "../../OS/ThirdParty/OpenSource/winpixeventruntime/Include/WinPixEventRuntime/pix3.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../Interfaces/IGraphics.h"

#include "../../Utilities/RingBuffer.h"

#include "Direct3D11CapBuilder.h"

// #include "../../ThirdParty/OpenSource/gpudetect/include/GpuDetectHelper.h"

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

#if defined(AUTOMATED_TESTING)
#include "../../Application/Interfaces/IScreenshot.h"
#endif

#define D3D11_REQ_CONSTANT_BUFFER_SIZE (D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16u)

typedef HRESULT(WINAPI* PFN_D3DReflect)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData, _In_ SIZE_T SrcDataSize, _In_ REFIID pInterface,
                                        _Out_ void** ppReflector);

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
#pragma comment(lib, "d3dcompiler.lib")

#if !defined(FORGE_D3D11_DYNAMIC_LOADING)
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#endif

//-V:SAFE_FREE:779
#define SAFE_FREE(p_var) \
    if (p_var)           \
    {                    \
        tf_free(p_var);  \
        p_var = NULL;    \
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

/************************************************************************/
// D3D11 Dynamic Loader
/************************************************************************/

#if defined(FORGE_D3D11_DYNAMIC_LOADING)

typedef HRESULT(WINAPI* PFN_DXGI_CreateDXGIFactory1)(REFIID riid, void** ppFactory);

static bool                        gD3D11dllInited = false;
static PFN_D3D11_CREATE_DEVICE     gPfnD3D11dllCreateDevice = NULL;
static PFN_DXGI_CreateDXGIFactory1 gPfnD3D11dllCreateDXGIFactory1 = NULL;
static HMODULE                     gD3D11dll = NULL;
static HMODULE                     gDXGIdll = NULL;

#endif

void d3d11dll_exit()
{
#if defined(FORGE_D3D11_DYNAMIC_LOADING)
    if (gD3D11dll)
    {
        LOGF(LogLevel::eINFO, "Unloading d3d11.dll");
        gPfnD3D11dllCreateDevice = NULL;
        FreeLibrary(gD3D11dll);
        gD3D11dll = NULL;
    }

    if (gDXGIdll)
    {
        LOGF(LogLevel::eINFO, "Unloading dxgi.dll");
        gPfnD3D11dllCreateDXGIFactory1 = NULL;
        FreeLibrary(gDXGIdll);
        gDXGIdll = NULL;
    }

    gD3D11dllInited = false;
#endif
}

bool d3d11dll_init()
{
#if defined(FORGE_D3D11_DYNAMIC_LOADING)
    if (gD3D11dllInited)
        return true;

    LOGF(LogLevel::eINFO, "Loading d3d11.dll");
    gD3D11dll = LoadLibraryExA("d3d11.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (gD3D11dll)
    {
        gPfnD3D11dllCreateDevice = (PFN_D3D11_CREATE_DEVICE)(GetProcAddress(gD3D11dll, "D3D11CreateDevice"));
    }

    LOGF(LogLevel::eINFO, "Loading dxgi.dll");
    gDXGIdll = LoadLibraryExA("dxgi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (gDXGIdll)
    {
        gPfnD3D11dllCreateDXGIFactory1 = (PFN_DXGI_CreateDXGIFactory1)(GetProcAddress(gDXGIdll, "CreateDXGIFactory1"));
    }

    gD3D11dllInited = gDXGIdll && gD3D11dll;
    if (!gD3D11dllInited)
        d3d11dll_exit();
    return gD3D11dllInited;
#else
    return true;
#endif
}

static HRESULT WINAPI d3d11dll_CreateDevice(IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
                                            const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
                                            ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel,
                                            ID3D11DeviceContext** ppImmediateContext)
{
#if defined(FORGE_D3D11_DYNAMIC_LOADING)
    if (gPfnD3D11dllCreateDevice)
        return gPfnD3D11dllCreateDevice(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice,
                                        pFeatureLevel, ppImmediateContext);
    return D3D11_ERROR_FILE_NOT_FOUND;
#else
    return D3D11CreateDevice(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel,
                             ppImmediateContext);
#endif
}

static HRESULT WINAPI d3d11dll_CreateDXGIFactory1(REFIID riid, void** ppFactory) //-V835
{                                                                                //-V835
#if defined(FORGE_D3D11_DYNAMIC_LOADING)                                         //-V835
    if (gPfnD3D11dllCreateDXGIFactory1)
        return gPfnD3D11dllCreateDXGIFactory1(riid, ppFactory);
    return DXGI_ERROR_UNSUPPORTED;
#else
    return CreateDXGIFactory1(riid, ppFactory);
#endif
}

// Internal utility functions (may become external one day)
// uint64_t					  util_dx_determine_storage_counter_offset(uint64_t buffer_size);
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
    case ADDRESS_MODE_MIRROR:
        return D3D11_TEXTURE_ADDRESS_MIRROR;
    case ADDRESS_MODE_REPEAT:
        return D3D11_TEXTURE_ADDRESS_WRAP;
    case ADDRESS_MODE_CLAMP_TO_EDGE:
        return D3D11_TEXTURE_ADDRESS_CLAMP;
    case ADDRESS_MODE_CLAMP_TO_BORDER:
        return D3D11_TEXTURE_ADDRESS_BORDER;
    default:
        return D3D11_TEXTURE_ADDRESS_WRAP;
    }
}

DXGI_FORMAT util_to_dx11_uav_format(DXGI_FORMAT defaultFormat)
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
    case DXGI_FORMAT_D16_UNORM:
        LOGF(eERROR, "Requested a UAV format for a depth stencil format");
#endif

    default:
        return defaultFormat;
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
    case DXGI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_D16_UNORM;

    default:
        return defaultFormat;
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
    case DXGI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_R16_UNORM;

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    default:
        return defaultFormat;
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
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT util_to_dx11_swapchain_format(TinyImageFormat format)
{
    DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;

    // FLIP_DISCARD and FLIP_SEQEUNTIAL swapchain buffers only support these formats
    switch (format)
    {
    case TinyImageFormat_R16G16B16A16_SFLOAT:
        result = DXGI_FORMAT_R16G16B16A16_FLOAT;
        break;
    case TinyImageFormat_B8G8R8A8_UNORM:
    case TinyImageFormat_B8G8R8A8_SRGB:
        result = DXGI_FORMAT_B8G8R8A8_UNORM;
        break;
    case TinyImageFormat_R8G8B8A8_UNORM:
    case TinyImageFormat_R8G8B8A8_SRGB:
        result = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    case TinyImageFormat_R10G10B10A2_UNORM:
        result = DXGI_FORMAT_R10G10B10A2_UNORM;
        break;
    default:
        break;
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
    char*    key;
    uint32_t value;
} DescriptorIndexMap;

/************************************************************************/
// Logging functions
/************************************************************************/
static void add_rtv(Renderer* pRenderer, ID3D11Resource* pResource, DXGI_FORMAT format, uint32_t mipSlice, uint32_t arraySlice,
                    ID3D11RenderTargetView** pHandle)
{
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    D3D11_RESOURCE_DIMENSION      type;
    pResource->GetType(&type);

    rtvDesc.Format = format;

    switch (type)
    {
    case D3D11_RESOURCE_DIMENSION_BUFFER:
        break;
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
    default:
        break;
    }

    pRenderer->mDx11.pDevice->CreateRenderTargetView(pResource, &rtvDesc, pHandle);
}

static void add_dsv(Renderer* pRenderer, ID3D11Resource* pResource, DXGI_FORMAT format, uint32_t mipSlice, uint32_t arraySlice,
                    ID3D11DepthStencilView** pHandle)
{
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    D3D11_RESOURCE_DIMENSION      type;
    pResource->GetType(&type);

    dsvDesc.Format = format;

    switch (type)
    {
    case D3D11_RESOURCE_DIMENSION_BUFFER:
        break;
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
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
        ASSERT(false && "Cannot create 3D Depth Stencil");
        break;
    default:
        break;
    }

    pRenderer->mDx11.pDevice->CreateDepthStencilView(pResource, &dsvDesc, pHandle);
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

static void SetResourceName(ID3D11DeviceChild* pResource, const char* pName);

// clang-format on
void d3d11_cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
    // TODO: Use CopySubresource region to update the contents of the destination buffer without mapping them to cpu.

    ASSERT(pCmd);
    ASSERT(pSrcBuffer);
    ASSERT(pBuffer);
    ASSERT(pBuffer->mDx11.pResource);

    Renderer*            pRenderer = pCmd->pRenderer;
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

    D3D11_MAPPED_SUBRESOURCE sub = {};
    D3D11_BOX                dstBox = { (UINT)dstOffset, 0, 0, (UINT)(dstOffset + size), 1, 1 };

    // Not persistently mapped buffer.
    if (!pSrcBuffer->pCpuMappedAddress)
    {
        pContext->Map(pSrcBuffer->mDx11.pResource, 0, D3D11_MAP_READ, 0, &sub);
    }
    else
    {
        sub = { pSrcBuffer->pCpuMappedAddress, 0, 0 };
    }

    pContext->UpdateSubresource(pBuffer->mDx11.pResource, 0, &dstBox, (uint8_t*)sub.pData + srcOffset, (UINT)size, 0);

    if (!pSrcBuffer->pCpuMappedAddress)
    {
        pContext->Unmap(pSrcBuffer->mDx11.pResource, 0);
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
    ASSERT(pTexture->mDx11.pResource);

    Renderer*            pRenderer = pCmd->pRenderer;
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

    D3D11_MAPPED_SUBRESOURCE sub = {};
    UINT subresource = D3D11CalcSubresource(pSubresourceDesc->mMipLevel, pSubresourceDesc->mArrayLayer, (uint32_t)pTexture->mMipLevels);

    if (!pSrcBuffer->pCpuMappedAddress)
    {
        pContext->Map(pSrcBuffer->mDx11.pResource, 0, D3D11_MAP_READ, 0, &sub);
    }
    else
    {
        sub = { pSrcBuffer->pCpuMappedAddress, 0, 0 };
    }

    pContext->UpdateSubresource(pTexture->mDx11.pResource, subresource, NULL, (uint8_t*)sub.pData + pSubresourceDesc->mSrcOffset,
                                pSubresourceDesc->mRowPitch, pSubresourceDesc->mSlicePitch);

    if (!pSrcBuffer->pCpuMappedAddress)
    {
        pContext->Unmap(pSrcBuffer->mDx11.pResource, 0);
    }
}
/************************************************************************/
// Internal init functions
/************************************************************************/
static bool AddDevice(Renderer* pRenderer, const RendererDesc* pDesc)
{
    int               levelIndex = pDesc->mDx11.mUseDx10 ? 2 : 0;
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    const uint32_t featureLevelCount = TF_ARRAY_COUNT(featureLevels) - levelIndex;

    GPUSettings gpuSettings[MAX_MULTIPLE_GPUS] = {};

    for (uint32_t i = 0; i < pRenderer->pContext->mGpuCount; ++i)
    {
        gpuSettings[i] = pRenderer->pContext->mGpus[i].mSettings;
    }

    // Select Best GPUs by poth Preset and highest feature level gpu at front
    uint32_t gpuIndex = util_select_best_gpu(gpuSettings, pRenderer->pContext->mGpuCount);

    // Get the latest and greatest feature level gpu
    pRenderer->pGpu = &pRenderer->pContext->mGpus[gpuIndex];
    pRenderer->mLinkedNodeCount = 1;

    // rejection rules from gpu.cfg
    bool driverValid = checkDriverRejectionSettings(&gpuSettings[gpuIndex]);
    if (!driverValid)
    {
        setRendererInitializationError("Driver rejection return invalid result.\nPlease, update your driver to the latest version.");
        return false;
    }

    // print selected GPU information
    LOGF(LogLevel::eINFO, "GPU[%u] is selected as default GPU", gpuIndex);
    LOGF(LogLevel::eINFO, "Name of selected gpu: %s", pRenderer->pGpu->mSettings.mGpuVendorPreset.mGpuName);
    LOGF(LogLevel::eINFO, "Vendor id of selected gpu: %#x", pRenderer->pGpu->mSettings.mGpuVendorPreset.mVendorId);
    LOGF(LogLevel::eINFO, "Model id of selected gpu: %#x", pRenderer->pGpu->mSettings.mGpuVendorPreset.mModelId);
    LOGF(LogLevel::eINFO, "Revision id of selected gpu: %#x", pRenderer->pGpu->mSettings.mGpuVendorPreset.mRevisionId);
    LOGF(LogLevel::eINFO, "Preset of selected gpu: %s", presetLevelToString(pRenderer->pGpu->mSettings.mGpuVendorPreset.mPresetLevel));

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
    HRESULT           hr = d3d11dll_CreateDevice(pRenderer->pGpu->mDx11.pGpu, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, deviceFlags,
                                       &featureLevels[levelIndex], featureLevelCount, D3D11_SDK_VERSION, &pRenderer->mDx11.pDevice,
                                       &featureLevel, // max feature level
                                       &pRenderer->mDx11.pContext);

    if (FAILED(hr))
    {
        LOGF(eWARNING, "Failed to initialize D3D11 debug device, fallback to non-debug");
        hr = d3d11dll_CreateDevice(pRenderer->pGpu->mDx11.pGpu, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, &featureLevels[levelIndex],
                                   featureLevelCount, D3D11_SDK_VERSION, &pRenderer->mDx11.pDevice,
                                   &featureLevel, // max feature level
                                   &pRenderer->mDx11.pContext);
    }

    if (E_INVALIDARG == hr)
    {
        hr = d3d11dll_CreateDevice(pRenderer->pGpu->mDx11.pGpu, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, deviceFlags,
                                   &featureLevels[levelIndex + 1], featureLevelCount - 1, D3D11_SDK_VERSION, &pRenderer->mDx11.pDevice,
                                   &featureLevel, // max feature level
                                   &pRenderer->mDx11.pContext);
    }
    ASSERT(SUCCEEDED(hr));
    if (FAILED(hr))
        LOGF(LogLevel::eERROR, "Failed to create D3D11 device and context.");

    pRenderer->mDx11.pContext1 = NULL;
    hr = pRenderer->mDx11.pContext->QueryInterface(&pRenderer->mDx11.pContext1);
    if (SUCCEEDED(hr))
    {
        LOGF(LogLevel::eINFO, "Device supports ID3D11DeviceContext1.");
    }

#if defined(ENABLE_NSIGHT_AFTERMATH)
    SetAftermathDevice(pRenderer->mDx11.pDevice);
#endif

#if defined(ENABLE_GRAPHICS_DEBUG)
    hr = pRenderer->mDx11.pContext->QueryInterface(__uuidof(pRenderer->mDx11.pUserDefinedAnnotation),
                                                   (void**)(&pRenderer->mDx11.pUserDefinedAnnotation));
    if (FAILED(hr))
    {
        LOGF(LogLevel::eERROR, "Failed to query interface ID3DUserDefinedAnnotation.");
    }
#endif

    return true;
}

static void RemoveDevice(Renderer* pRenderer)
{
    SAFE_RELEASE(pRenderer->mDx11.pUserDefinedAnnotation);
    SAFE_RELEASE(pRenderer->mDx11.pContext1);
    SAFE_RELEASE(pRenderer->mDx11.pContext);
#if defined(ENABLE_GRAPHICS_DEBUG) && !defined(ENABLE_NSIGHT_AFTERMATH)
    ID3D11Debug* pDebugDevice = NULL;
    pRenderer->mDx11.pDevice->QueryInterface(&pDebugDevice);
    SAFE_RELEASE(pRenderer->mDx11.pDevice);

    if (pDebugDevice)
    {
        LOGF(eWARNING, "Printing live D3D11 objects to the debugger output window...");
        pDebugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
        pDebugDevice->Release();
    }
    else
    {
        LOGF(eWARNING, "Unable to retrieve the D3D11 debug interface, cannot print live D3D11 objects.");
    }
#else
    SAFE_RELEASE(pRenderer->mDx11.pDevice);
#endif

#if defined(ENABLE_NSIGHT_AFTERMATH)
    DestroyAftermathTracker(&pRenderer->mAftermathTracker);
#endif
}
/************************************************************************/
// Pipeline State Functions
/************************************************************************/
static inline FORGE_CONSTEXPR uint8_t ToColorWriteMask(ColorMask mask)
{
    uint8_t ret = 0;
    if (mask & COLOR_MASK_RED)
    {
        ret |= D3D11_COLOR_WRITE_ENABLE_RED;
    }
    if (mask & COLOR_MASK_GREEN)
    {
        ret |= D3D11_COLOR_WRITE_ENABLE_GREEN;
    }
    if (mask & COLOR_MASK_BLUE)
    {
        ret |= D3D11_COLOR_WRITE_ENABLE_BLUE;
    }
    if (mask & COLOR_MASK_ALPHA)
    {
        ret |= D3D11_COLOR_WRITE_ENABLE_ALPHA;
    }

    return ret;
}

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
            BOOL blendEnable = (gBlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]] != D3D11_BLEND_ONE ||
                                gBlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]] != D3D11_BLEND_ZERO ||
                                gBlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]] != D3D11_BLEND_ONE ||
                                gBlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]] != D3D11_BLEND_ZERO);

            desc.RenderTarget[i].BlendEnable = blendEnable;
            desc.RenderTarget[i].RenderTargetWriteMask = ToColorWriteMask(pDesc->mColorWriteMasks[blendDescIndex]);
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
    if (FAILED(pRenderer->mDx11.pDevice->CreateBlendState(&desc, &out)))
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
    desc.BackFace.StencilPassOp = gStencilOpTranslator[pDesc->mStencilBackPass];
    desc.FrontFace.StencilPassOp = gStencilOpTranslator[pDesc->mStencilFrontPass];

    ID3D11DepthStencilState* out = NULL;
    if (FAILED(pRenderer->mDx11.pDevice->CreateDepthStencilState(&desc, &out)))
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
    if (FAILED(pRenderer->mDx11.pDevice->CreateRasterizerState(&desc, &out)))
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
    blendStateDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
    blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
    blendStateDesc.mIndependentBlend = false;
    pRenderer->mDx11.pDefaultBlendState = util_to_blend_state(pRenderer, &blendStateDesc);

    DepthStateDesc depthStateDesc = {};
    depthStateDesc.mDepthFunc = CMP_LEQUAL;
    depthStateDesc.mDepthTest = false;
    depthStateDesc.mDepthWrite = false;
    depthStateDesc.mStencilBackFunc = CMP_ALWAYS;
    depthStateDesc.mStencilFrontFunc = CMP_ALWAYS;
    depthStateDesc.mStencilReadMask = 0xFF;
    depthStateDesc.mStencilWriteMask = 0xFF;
    pRenderer->mDx11.pDefaultDepthState = util_to_depth_state(pRenderer, &depthStateDesc);

    RasterizerStateDesc rasterizerStateDesc = {};
    rasterizerStateDesc.mCullMode = CULL_MODE_BACK;
    pRenderer->mDx11.pDefaultRasterizerState = util_to_rasterizer_state(pRenderer, &rasterizerStateDesc);
}

static void remove_default_resources(Renderer* pRenderer)
{
    SAFE_RELEASE(pRenderer->mDx11.pDefaultBlendState);
    SAFE_RELEASE(pRenderer->mDx11.pDefaultDepthState);
    SAFE_RELEASE(pRenderer->mDx11.pDefaultRasterizerState);
}
/************************************************************************/
// Renderer Init Remove
/************************************************************************/
void d3d11_initRendererContext(const char* appName, const RendererContextDesc* pDesc, RendererContext** ppContext)
{
    UNREF_PARAM(appName);
    d3d11dll_init();

    RendererContext* pContext = (RendererContext*)tf_calloc_memalign(1, alignof(RendererContext), sizeof(RendererContext));
    ASSERT(pContext);

    for (uint32_t i = 0; i < TF_ARRAY_COUNT(pContext->mGpus); ++i)
    {
        setDefaultGPUSettings(&pContext->mGpus[i].mSettings);
    }

    if (FAILED(d3d11dll_CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pContext->mDx11.pDXGIFactory)))
    {
        LOGF(LogLevel::eERROR, "Could not create DXGI factory.");
        return;
    }
    ASSERT(pContext->mDx11.pDXGIFactory);

    int               levelIndex = pDesc->mDx11.mUseDx10 ? 2 : 0;
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    const uint32_t featureLevelCount = TF_ARRAY_COUNT(featureLevels) - levelIndex;

    HRESULT hr = 0;

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
        uint32_t       mVendorId;
        uint32_t       mDeviceId;
        uint32_t       mRevisionId;
        char           mName[MAX_GPU_VENDOR_STRING_LENGTH];
        GPUPresetLevel mPreset;
    } GpuDesc;

    uint32_t       gpuCount = 0;
    IDXGIAdapter1* adapter = NULL;
    bool           foundSoftwareAdapter = false;

    for (UINT i = 0; DXGI_ERROR_NOT_FOUND != pContext->mDx11.pDXGIFactory->EnumAdapters1(i, (IDXGIAdapter1**)&adapter); ++i)
    {
        DECLARE_ZERO(DXGI_ADAPTER_DESC1, desc);
        adapter->GetDesc1(&desc);

        if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
        {
            // Make sure the adapter can support a D3D11 device
            D3D_FEATURE_LEVEL featLevelOut;
            hr = d3d11dll_CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, &featureLevels[levelIndex], featureLevelCount,
                                       D3D11_SDK_VERSION, NULL, &featLevelOut, NULL);
            if (E_INVALIDARG == hr)
            {
                hr = d3d11dll_CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, &featureLevels[levelIndex + 1],
                                           featureLevelCount - 1, D3D11_SDK_VERSION, NULL, &featLevelOut, NULL);
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
        return;
    }

    ASSERT(gpuCount);
    GpuDesc* gpuDesc = (GpuDesc*)alloca(gpuCount * sizeof(GpuDesc));
    memset(gpuDesc, 0, gpuCount * sizeof(GpuDesc));
    gpuCount = 0;

    for (UINT i = 0; DXGI_ERROR_NOT_FOUND != pContext->mDx11.pDXGIFactory->EnumAdapters1(i, (IDXGIAdapter1**)&adapter); ++i)
    {
        DXGI_ADAPTER_DESC1 desc = {};
        adapter->GetDesc1(&desc);

        // Test for display output presence and hardware adapter first to even consider the adapter
        IDXGIOutput* outputs;
        if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && SUCCEEDED(adapter->EnumOutputs(0, &outputs)))
        {
            // Make sure the adapter can support a D3D11 device
            D3D_FEATURE_LEVEL featLevelOut;
            hr = d3d11dll_CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, &featureLevels[levelIndex], featureLevelCount,
                                       D3D11_SDK_VERSION, NULL, &featLevelOut, NULL);
            if (E_INVALIDARG == hr)
            {
                hr = d3d11dll_CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, &featureLevels[levelIndex + 1],
                                           featureLevelCount - 1, D3D11_SDK_VERSION, NULL, &featLevelOut, NULL);
            }
            if (SUCCEEDED(hr))
            {
                hr = adapter->QueryInterface(IID_ARGS(&gpuDesc[gpuCount].pGpu));
                if (SUCCEEDED(hr))
                {
                    ID3D11Device* device = NULL;
                    hr = d3d11dll_CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, &featureLevels[levelIndex],
                                               featureLevelCount, D3D11_SDK_VERSION, &device, &featLevelOut, NULL);

                    if (E_INVALIDARG == hr)
                    {
                        hr = d3d11dll_CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, (HMODULE)0, 0, &featureLevels[levelIndex + 1],
                                                   featureLevelCount - 1, D3D11_SDK_VERSION, &device, &featLevelOut, NULL);
                    }

                    if (!device)
                    {
                        LOGF(eERROR, "Device creation failed for adapter %s with code %u", gpuDesc[gpuCount].mName, (uint32_t)hr);
                        ++gpuCount;
                        continue;
                    }

                    d3d11CapsBuilder(device, &pContext->mGpus[i].mCapBits);

                    D3D11_FEATURE_DATA_D3D11_OPTIONS featureData = {};
                    hr = device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &featureData, sizeof(featureData));
                    if (FAILED(hr))
                        LOGF(LogLevel::eINFO, "D3D11 CheckFeatureSupport D3D11_FEATURE_D3D11_OPTIONS error 0x%x", hr);
#if WINVER > _WIN32_WINNT_WINBLUE
                    D3D11_FEATURE_DATA_D3D11_OPTIONS2 featureData2 = {};
                    hr = device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2, &featureData2, sizeof(featureData2));
                    if (FAILED(hr))
                        LOGF(LogLevel::eINFO, "D3D11 CheckFeatureSupport D3D11_FEATURE_D3D11_OPTIONS2 error 0x%x", hr);

                    gpuDesc[gpuCount].mFeatureDataOptions2 = featureData2;
#endif
                    gpuDesc[gpuCount].mMaxSupportedFeatureLevel = featLevelOut;
                    gpuDesc[gpuCount].mDedicatedVideoMemory = desc.DedicatedVideoMemory;
                    gpuDesc[gpuCount].mFeatureDataOptions = featureData;

                    // save vendor and model Id as string
                    // char hexChar[10];
                    // convert deviceId and assign it
                    gpuDesc[gpuCount].mDeviceId = desc.DeviceId;
                    // convert modelId and assign it
                    gpuDesc[gpuCount].mVendorId = desc.VendorId;
                    // convert Revision Id
                    gpuDesc[gpuCount].mRevisionId = desc.Revision;
                    // save gpu name (Some situtations this can show description instead of name)
                    // char sName[MAX_PATH];
                    wcstombs(gpuDesc[gpuCount].mName, desc.Description, FS_MAX_PATH);

                    // get preset for current gpu description
                    gpuDesc[gpuCount].mPreset = getGPUPresetLevel(gpuDesc[gpuCount].mVendorId, gpuDesc[gpuCount].mDeviceId,
                                                                  getGPUVendorName(gpuDesc[gpuCount].mVendorId), gpuDesc[gpuCount].mName);

                    ++gpuCount;
                    SAFE_RELEASE(device);

                    // Default GPU is the first GPU received in EnumAdapters
                    if (pDesc->mDx11.mUseDefaultGpu)
                    {
                        break;
                    }
                }
            }
        }

        adapter->Release();
    }

    ASSERT(gpuCount);

    // update renderer gpu settings
    pContext->mGpuCount = gpuCount;

    for (uint32_t i = 0; i < gpuCount; ++i)
    {
        GpuInfo* gpu = &pContext->mGpus[i];
        gpuDesc[i].pGpu->QueryInterface(&gpu->mDx11.pGpu);
        SAFE_RELEASE(gpuDesc[i].pGpu);

        gpu->mSettings.mUniformBufferAlignment = 256;
        gpu->mSettings.mUploadBufferTextureAlignment = 1;
        gpu->mSettings.mUploadBufferTextureRowAlignment = 1;
        gpu->mSettings.mMultiDrawIndirect = false; // no such thing
        gpu->mSettings.mMaxVertexInputBindings = 32U;
        gpu->mSettings.mMaxBoundTextures = 128;
        gpu->mSettings.mIndirectRootConstant = false;
        gpu->mSettings.mBuiltinDrawID = false;
        gpu->mSettings.mTimestampQueries = true;
        gpu->mSettings.mOcclusionQueries = true;
        gpu->mSettings.mPipelineStatsQueries = true;
        gpu->mSettings.mVRAM = gpuDesc[i].mDedicatedVideoMemory;

        // assign device ID
        gpu->mSettings.mGpuVendorPreset.mModelId = gpuDesc[i].mDeviceId;
        // assign vendor ID
        gpu->mSettings.mGpuVendorPreset.mVendorId = gpuDesc[i].mVendorId;
        // assign Revision ID
        gpu->mSettings.mGpuVendorPreset.mRevisionId = gpuDesc[i].mRevisionId;
        // get name from api
        strncpy(gpu->mSettings.mGpuVendorPreset.mGpuName, gpuDesc[i].mName, MAX_GPU_VENDOR_STRING_LENGTH);
        // get preset
        gpu->mSettings.mGpuVendorPreset.mPresetLevel = gpuDesc[i].mPreset;

        // Determine root signature size for this gpu driver
#if WINVER > _WIN32_WINNT_WINBLUE
        gpu->mSettings.mROVsSupported = gpuDesc[i].mFeatureDataOptions2.ROVsSupported ? true : false;
#else
        gpu->mSettings.mROVsSupported = false;
#endif
        gpu->mSettings.mTessellationSupported = gpuDesc[i].mMaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_11_0;
        gpu->mSettings.mGeometryShaderSupported = gpuDesc[i].mMaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_10_0;
        gpu->mSettings.mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_NONE;
        gpu->mSettings.mWaveOpsSupportedStageFlags = SHADER_STAGE_NONE;

        if (gpuDesc[i].mMaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_10_0)
        {
            // https://learn.microsoft.com/en-us/windows/win32/direct3d11/direct3d-11-advanced-stages-compute-shader
            gpu->mSettings.mMaxTotalComputeThreads = gpuDesc[i].mMaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_11_0
                                                         ? D3D11_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP
                                                         : D3D11_CS_4_X_THREAD_GROUP_MAX_THREADS_PER_GROUP;
            gpu->mSettings.mMaxComputeThreads[0] = gpuDesc[i].mMaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_11_0
                                                       ? D3D11_CS_THREAD_GROUP_MAX_X
                                                       : D3D11_CS_4_X_THREAD_GROUP_MAX_X;
            gpu->mSettings.mMaxComputeThreads[1] = gpuDesc[i].mMaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_11_0
                                                       ? D3D11_CS_THREAD_GROUP_MAX_Y
                                                       : D3D11_CS_4_X_THREAD_GROUP_MAX_Y;
            gpu->mSettings.mMaxComputeThreads[2] = gpuDesc[i].mMaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_11_0
                                                       ? D3D11_CS_THREAD_GROUP_MAX_Z
                                                       : D3D11_CS_4_X_DISPATCH_MAX_THREAD_GROUPS_IN_Z_DIMENSION;
        }

        gpu->mDx11.mPartialUpdateConstantBufferSupported = gpuDesc[i].mFeatureDataOptions.ConstantBufferPartialUpdate;
        gpu->mSettings.mFeatureLevel = gpuDesc[i].mMaxSupportedFeatureLevel;

        applyGPUConfigurationRules(&gpu->mSettings, &gpu->mCapBits);

        // Determine root signature size for this gpu driver
        DXGI_ADAPTER_DESC adapterDesc;
        gpu->mDx11.pGpu->GetDesc(&adapterDesc);
        LOGF(LogLevel::eINFO, "GPU[%u] detected. Vendor ID: %x, Model ID: %x, Revision ID: %x, Preset: %s, GPU Name: %S", i,
             adapterDesc.VendorId, adapterDesc.DeviceId, adapterDesc.Revision,
             presetLevelToString(gpu->mSettings.mGpuVendorPreset.mPresetLevel), adapterDesc.Description);
    }

    *ppContext = pContext;
}

void d3d11_exitRendererContext(RendererContext* pContext)
{
    ASSERT(pContext);

    for (uint32_t i = 0; i < pContext->mGpuCount; ++i)
    {
        SAFE_RELEASE(pContext->mGpus[i].mDx11.pGpu);
    }

    SAFE_RELEASE(pContext->mDx11.pDXGIFactory);
    d3d11dll_exit();
    SAFE_FREE(pContext);
}

void d3d11_initRenderer(const char* appName, const RendererDesc* settings, Renderer** ppRenderer)
{
    ASSERT(ppRenderer);
    ASSERT(settings);
    ASSERT(settings->mShaderTarget <= SHADER_TARGET_5_0);

    Renderer* pRenderer = (Renderer*)tf_calloc_memalign(1, alignof(Renderer), sizeof(Renderer));
    ASSERT(pRenderer);

    pRenderer->mRendererApi = RENDERER_API_D3D11;
    pRenderer->mGpuMode = settings->mGpuMode;
    pRenderer->mShaderTarget = SHADER_TARGET_5_0;
    pRenderer->pName = appName;

    if (settings->pContext)
    {
        pRenderer->pContext = settings->pContext;
        pRenderer->mOwnsContext = false;
    }
    else
    {
        RendererContextDesc contextDesc = {};
        contextDesc.mEnableGpuBasedValidation = settings->mEnableGpuBasedValidation;
        contextDesc.mD3D11Supported = settings->mD3D11Supported;
        COMPILE_ASSERT(sizeof(contextDesc.mDx11) == sizeof(settings->mDx11));
        memcpy(&contextDesc.mDx11, &settings->mDx11, sizeof(settings->mDx11));
        d3d11_initRendererContext(appName, &contextDesc, &pRenderer->pContext);
        pRenderer->mOwnsContext = true;
    }

    // Initialize the D3D11 bits
    {
        if (!AddDevice(pRenderer, settings))
        {
            *ppRenderer = NULL;
            return;
        }

        // anything below LOW preset is not supported and we will exit
        if (pRenderer->pGpu->mSettings.mGpuVendorPreset.mPresetLevel < GPU_PRESET_VERYLOW)
        {
            // remove device and any memory we allocated in just above as this is the first function called
            // when initializing the forge
            RemoveDevice(pRenderer);
            SAFE_FREE(pRenderer);
            LOGF(LogLevel::eERROR, "Selected GPU has an Office Preset in gpu.cfg.");
            LOGF(LogLevel::eERROR, "Office preset is not supported by The Forge.");

            // have the condition in the assert as well so its cleared when the assert message box appears
            ASSERT(pRenderer->pGpu->mSettings.mGpuVendorPreset.mPresetLevel >= GPU_PRESET_VERYLOW); //-V547

            // return NULL pRenderer so that client can gracefully handle exit
            // This is better than exiting from here in case client has allocated memory or has fallbacks
            *ppRenderer = NULL;
            return;
        }
    }

    add_default_resources(pRenderer);

    // Renderer is good!
    *ppRenderer = pRenderer;
}

void d3d11_exitRenderer(Renderer* pRenderer)
{
    ASSERT(pRenderer);

    remove_default_resources(pRenderer);

    RemoveDevice(pRenderer);

    if (pRenderer->mOwnsContext)
    {
        d3d11_exitRendererContext(pRenderer->pContext);
    }

    // Free all the renderer components
    SAFE_FREE(pRenderer);
}

/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void d3d11_addFence(Renderer* pRenderer, Fence** ppFence)
{
    // ASSERT that renderer is valid
    ASSERT(pRenderer);
    ASSERT(ppFence);

    // create a Fence and ASSERT that it is valid
    Fence* pFence = (Fence*)tf_calloc(1, sizeof(Fence));
    ASSERT(pFence);

    D3D11_QUERY_DESC desc = {};
    desc.Query = D3D11_QUERY_EVENT;

    CHECK_HRESULT(pRenderer->mDx11.pDevice->CreateQuery(&desc, &pFence->mDx11.pDX11Query));

    pFence->mDx11.mSubmitted = false;

    *ppFence = pFence;
}

void d3d11_removeFence(Renderer* pRenderer, Fence* pFence)
{
    // ASSERT that renderer is valid
    ASSERT(pRenderer);
    // ASSERT that given fence to remove is valid
    ASSERT(pFence);

    SAFE_RELEASE(pFence->mDx11.pDX11Query);

    SAFE_FREE(pFence);
}

void d3d11_addSemaphore(Renderer* pRenderer, Semaphore** pSemaphore)
{
    // NOTE: We will still use it to be able to generate
    // a dependency graph to serialize parallel GPU workload.

    // ASSERT that renderer is valid
    ASSERT(pRenderer);
    ASSERT(pSemaphore);
}

void d3d11_removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
    UNREF_PARAM(pSemaphore);
    // ASSERT that renderer and given semaphore are valid
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
    pQueue->mDx11.pContext = pRenderer->mDx11.pContext;
    pQueue->mNodeIndex = pDesc->mNodeIndex;
    pQueue->mType = pDesc->mType;
    pQueue->mDx11.pDevice = pRenderer->mDx11.pDevice;
    // eastl::string queueType = "DUMMY QUEUE FOR DX11 BACKEND";

    *ppQueue = pQueue;
}

void d3d11_removeQueue(Renderer* pRenderer, Queue* pQueue)
{
    ASSERT(pRenderer);
    ASSERT(pQueue);

    pQueue->mDx11.pContext = NULL;

    SAFE_FREE(pQueue);
}

void d3d11_addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppSwapChain);
    ASSERT(pDesc->mImageCount <= MAX_SWAPCHAIN_IMAGES);

    LOGF(LogLevel::eINFO, "Adding D3D11 swapchain @ %ux%u", pDesc->mWidth, pDesc->mHeight);

    SwapChain* pSwapChain = (SwapChain*)tf_calloc(1, sizeof(SwapChain) + pDesc->mImageCount * sizeof(RenderTarget*));
    ASSERT(pSwapChain);

    pSwapChain->mDx11.mSyncInterval = pDesc->mEnableVsync ? 1 : 0;
    pSwapChain->ppRenderTargets = (RenderTarget**)(pSwapChain + 1);
    ASSERT(pSwapChain->ppRenderTargets);

    HWND hwnd = (HWND)pDesc->mWindowHandle.window;

    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferDesc.Width = pDesc->mWidth;
    desc.BufferDesc.Height = pDesc->mHeight;
    desc.BufferDesc.Format = util_to_dx11_swapchain_format(pDesc->mColorFormat);
    desc.BufferDesc.RefreshRate.Numerator = 1;
    desc.BufferDesc.RefreshRate.Denominator = 60;
    desc.SampleDesc.Count = 1; // If multisampling is needed, we'll resolve it later
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = max(1u, pDesc->mImageCount); // Count includes the front buffer so 1+2 is triple buffering.
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

    HRESULT         hr = E_FAIL;
    IDXGISwapChain* swapchain = NULL;
    uint32_t        swapEffectsCount = (sizeof(swapEffects) / sizeof(DXGI_SWAP_EFFECT));
    uint32_t        i = pDesc->mUseFlipSwapEffect ? 0 : swapEffectsCount - 2;

    for (; i < swapEffectsCount; ++i)
    {
        desc.SwapEffect = swapEffects[i];
        hr = pRenderer->pContext->mDx11.pDXGIFactory->CreateSwapChain(pRenderer->mDx11.pDevice, &desc, &swapchain);
        if (SUCCEEDED(hr))
        {
            pSwapChain->mDx11.mSwapEffect = swapEffects[i];
            LOGF(eINFO, "Swapchain creation SUCCEEDED with SwapEffect %d", swapEffects[i]);
            break;
        }
    }
    if (!SUCCEEDED(hr))
    {
        LOGF(eERROR, "Swapchain creation FAILED with HRESULT: %u", (uint32_t)hr);
        ASSERT(false);
    }

    CHECK_HRESULT(pRenderer->pContext->mDx11.pDXGIFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

    CHECK_HRESULT(swapchain->QueryInterface(__uuidof(pSwapChain->mDx11.pSwapChain), (void**)&(pSwapChain->mDx11.pSwapChain)));
    swapchain->Release();

    ID3D11Resource** buffers = (ID3D11Resource**)alloca(pDesc->mImageCount * sizeof(ID3D11Resource*));

    // Create rendertargets from swapchain
    for (uint32_t j = 0; j < pDesc->mImageCount; ++j)
    {
        CHECK_HRESULT(pSwapChain->mDx11.pSwapChain->GetBuffer(0, IID_ARGS(&buffers[j])));
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

    for (uint32_t j = 0; j < pDesc->mImageCount; ++j)
    {
        descColor.pNativeHandle = (void*)buffers[j];
        ::addRenderTarget(pRenderer, &descColor, &pSwapChain->ppRenderTargets[j]);
    }

    pSwapChain->mImageCount = pDesc->mImageCount;
    pSwapChain->mDx11.mImageIndex = 0;
    pSwapChain->mEnableVsync = pDesc->mEnableVsync;

    *ppSwapChain = pSwapChain;
}

void d3d11_removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
    for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
    {
        ID3D11Resource* resource = pSwapChain->ppRenderTargets[i]->pTexture->mDx11.pResource;
        removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[i]);
        SAFE_RELEASE(resource);
    }

    SAFE_RELEASE(pSwapChain->mDx11.pSwapChain);
    SAFE_FREE(pSwapChain);
}
/************************************************************************/
// Command Pool Functions
/************************************************************************/
void d3d11_addCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
{
    // NOTE: We will still use cmd pools to be able to generate
    // a dependency graph to serialize parallel GPU workload.

    // ASSERT that renderer is valid
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
    // check validity of given renderer and command pool
    ASSERT(pRenderer);
    ASSERT(pCmdPool);

    SAFE_FREE(pCmdPool);
}

void d3d11_addCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
{
    UNREF_PARAM(pDesc);
    // verify that given pool is valid
    ASSERT(pRenderer);
    ASSERT(ppCmd);

    // allocate new command
    Cmd* pCmd = (Cmd*)tf_calloc_memalign(1, alignof(Cmd), sizeof(Cmd));
    ASSERT(pCmd);

    // set command pool of new command
    pCmd->pRenderer = pRenderer;

    *ppCmd = pCmd;
}

void d3d11_removeCmd(Renderer* pRenderer, Cmd* pCmd)
{
    // verify that given command and pool are valid
    ASSERT(pRenderer);
    ASSERT(pCmd);

    SAFE_RELEASE(pCmd->mDx11.pRootConstantBuffer);
    SAFE_FREE(pCmd);
}

void d3d11_addCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmd)
{
    // verify that ***cmd is valid
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(cmdCount);
    ASSERT(pppCmd);

    Cmd** ppCmds = (Cmd**)tf_calloc(cmdCount, sizeof(Cmd*));
    ASSERT(ppCmds);

    // add n new cmds to given pool
    for (uint32_t i = 0; i < cmdCount; ++i)
    {
        ::addCmd(pRenderer, pDesc, &ppCmds[i]);
    }

    *pppCmd = ppCmds;
}

void d3d11_removeCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
{
    // verify that given command list is valid
    ASSERT(ppCmds);

    // remove every given cmd in array
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
        pRenderTarget->mDx11.pDsvSliceDescriptors = (ID3D11DepthStencilView**)(pRenderTarget + 1);
    else
        pRenderTarget->mDx11.pRtvSliceDescriptors = (ID3D11RenderTargetView**)(pRenderTarget + 1);

    // add to gpu
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
        add_dsv(pRenderer, pRenderTarget->pTexture->mDx11.pResource, dxFormat, 0, (uint32_t)-1, &pRenderTarget->mDx11.pDsvDescriptor);
    else
        add_rtv(pRenderer, pRenderTarget->pTexture->mDx11.pResource, dxFormat, 0, (uint32_t)-1, &pRenderTarget->mDx11.pRtvDescriptor);

    for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
    {
        const uint32_t depthOrArraySize = pDesc->mDepth * pDesc->mArraySize;
        if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
            (pDesc->mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
        {
            for (uint32_t j = 0; j < depthOrArraySize; ++j)
            {
                if (isDepth)
                    add_dsv(pRenderer, pRenderTarget->pTexture->mDx11.pResource, dxFormat, i, j,
                            &pRenderTarget->mDx11.pDsvSliceDescriptors[i * depthOrArraySize + j]);
                else
                    add_rtv(pRenderer, pRenderTarget->pTexture->mDx11.pResource, dxFormat, i, j,
                            &pRenderTarget->mDx11.pRtvSliceDescriptors[i * depthOrArraySize + j]);
            }
        }
        else
        {
            if (isDepth)
                add_dsv(pRenderer, pRenderTarget->pTexture->mDx11.pResource, dxFormat, i, (uint32_t)-1,
                        &pRenderTarget->mDx11.pDsvSliceDescriptors[i]);
            else
                add_rtv(pRenderer, pRenderTarget->pTexture->mDx11.pResource, dxFormat, i, (uint32_t)-1,
                        &pRenderTarget->mDx11.pRtvSliceDescriptors[i]);
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
    pRenderTarget->mDescriptors = pDesc->mDescriptors;

    *ppRenderTarget = pRenderTarget;
}

void d3d11_removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
    bool const isDepth = TinyImageFormat_HasDepth(pRenderTarget->mFormat);

    removeTexture(pRenderer, pRenderTarget->pTexture);

    if (isDepth)
    {
        SAFE_RELEASE(pRenderTarget->mDx11.pDsvDescriptor);
    }
    else
    {
        SAFE_RELEASE(pRenderTarget->mDx11.pRtvDescriptor);
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
                    SAFE_RELEASE(pRenderTarget->mDx11.pDsvSliceDescriptors[i * depthOrArraySize + j]);
                }
                else
                {
                    SAFE_RELEASE(pRenderTarget->mDx11.pRtvSliceDescriptors[i * depthOrArraySize + j]);
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
                SAFE_RELEASE(pRenderTarget->mDx11.pDsvSliceDescriptors[i]);
            }
            else
            {
                SAFE_RELEASE(pRenderTarget->mDx11.pRtvSliceDescriptors[i]);
            }
        }
    }

    SAFE_FREE(pRenderTarget);
}

void d3d11_addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
    ASSERT(pRenderer);
    ASSERT(pRenderer->mDx11.pDevice);
    ASSERT(pDesc->mCompareFunc < MAX_COMPARE_MODES);
    ASSERT(ppSampler);

    // initialize to zero
    Sampler* pSampler = (Sampler*)tf_calloc_memalign(1, alignof(Sampler), sizeof(Sampler));
    ASSERT(pSampler);

    // default sampler lod values
    // used if not overriden by mSetLodRange or not Linear mipmaps
    float minSamplerLod = 0;
    float maxSamplerLod = pDesc->mMipMapMode == MIPMAP_MODE_LINEAR ? D3D11_FLOAT32_MAX : 0;
    // user provided lods
    if (pDesc->mSetLodRange)
    {
        minSamplerLod = pDesc->mMinLod;
        maxSamplerLod = pDesc->mMaxLod;
    }

    // add sampler to gpu
    D3D11_SAMPLER_DESC desc;
    desc.Filter = util_to_dx11_filter(pDesc->mMinFilter, pDesc->mMagFilter, pDesc->mMipMapMode, pDesc->mMaxAnisotropy > 0.0f,
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

    if (FAILED(pRenderer->mDx11.pDevice->CreateSamplerState(&desc, &pSampler->mDx11.pSamplerState)))
        LOGF(LogLevel::eERROR, "Failed to create sampler state.");

    *ppSampler = pSampler;
}

void d3d11_removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
    ASSERT(pRenderer);
    ASSERT(pSampler);

    SAFE_RELEASE(pSampler->mDx11.pSamplerState);
    SAFE_FREE(pSampler);
}

/************************************************************************/
// Shader Functions
/************************************************************************/
void d3d11_addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
    ASSERT(pRenderer);
    ASSERT(pDesc && pDesc->mStages);
    ASSERT(ppShaderProgram);

    Shader* pShaderProgram = (Shader*)tf_calloc(1, sizeof(Shader) + sizeof(PipelineReflection));
    ASSERT(pShaderProgram);

    pShaderProgram->mStages = pDesc->mStages;
    pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1); //-V1027

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
                CHECK_HRESULT(pRenderer->mDx11.pDevice->CreateVertexShader(pStage->pByteCode, pStage->mByteCodeSize, NULL,
                                                                           &pShaderProgram->mDx11.pVertexShader));
                CHECK_HRESULT(D3DGetInputSignatureBlob(pStage->pByteCode, pStage->mByteCodeSize, &pShaderProgram->mDx11.pInputSignature));
                SetResourceName(pShaderProgram->mDx11.pVertexShader, pDesc->mVert.pName);
            }
            break;
            case SHADER_STAGE_HULL:
            {
                pStage = &pDesc->mHull;
                CHECK_HRESULT(pRenderer->mDx11.pDevice->CreateHullShader(pStage->pByteCode, pStage->mByteCodeSize, NULL,
                                                                         &pShaderProgram->mDx11.pHullShader));
                SetResourceName(pShaderProgram->mDx11.pHullShader, pDesc->mHull.pName);
            }
            break;
            case SHADER_STAGE_DOMN:
            {
                pStage = &pDesc->mDomain;
                CHECK_HRESULT(pRenderer->mDx11.pDevice->CreateDomainShader(pStage->pByteCode, pStage->mByteCodeSize, NULL,
                                                                           &pShaderProgram->mDx11.pDomainShader));
                SetResourceName(pShaderProgram->mDx11.pDomainShader, pDesc->mDomain.pName);
            }
            break;
            case SHADER_STAGE_GEOM:
            {
                pStage = &pDesc->mGeom;
                CHECK_HRESULT(pRenderer->mDx11.pDevice->CreateGeometryShader(pStage->pByteCode, pStage->mByteCodeSize, NULL,
                                                                             &pShaderProgram->mDx11.pGeometryShader));
                SetResourceName(pShaderProgram->mDx11.pGeometryShader, pDesc->mGeom.pName);
            }
            break;
            case SHADER_STAGE_FRAG:
            {
                pStage = &pDesc->mFrag;
                CHECK_HRESULT(pRenderer->mDx11.pDevice->CreatePixelShader(pStage->pByteCode, pStage->mByteCodeSize, NULL,
                                                                          &pShaderProgram->mDx11.pPixelShader));
                SetResourceName(pShaderProgram->mDx11.pPixelShader, pDesc->mFrag.pName);
            }
            break;
            case SHADER_STAGE_COMP:
            {
                pStage = &pDesc->mComp;
                CHECK_HRESULT(pRenderer->mDx11.pDevice->CreateComputeShader(pStage->pByteCode, pStage->mByteCodeSize, NULL,
                                                                            &pShaderProgram->mDx11.pComputeShader));
                SetResourceName(pShaderProgram->mDx11.pComputeShader, pDesc->mComp.pName);
            }
            break;
            default:
            {
                LOGF(LogLevel::eERROR, "Unknown shader stage %i", stage_mask);
            }
            break;
            }

            d3d11_createShaderReflection((uint8_t*)(pStage->pByteCode), (uint32_t)pStage->mByteCodeSize, stage_mask, //-V522
                                         &pShaderProgram->pReflection->mStageReflections[reflectionCount]);

            reflectionCount++;
        }
    }

    createPipelineReflection(pShaderProgram->pReflection->mStageReflections, reflectionCount, pShaderProgram->pReflection);

    *ppShaderProgram = pShaderProgram;
}

void d3d11_removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
    UNREF_PARAM(pRenderer);

    // remove given shader
    destroyPipelineReflection(pShaderProgram->pReflection);

    if (pShaderProgram->mStages & SHADER_STAGE_COMP)
    {
        SAFE_RELEASE(pShaderProgram->mDx11.pComputeShader);
    }
    else
    {
        SAFE_RELEASE(pShaderProgram->mDx11.pVertexShader);
        SAFE_RELEASE(pShaderProgram->mDx11.pPixelShader);
        SAFE_RELEASE(pShaderProgram->mDx11.pGeometryShader);
        SAFE_RELEASE(pShaderProgram->mDx11.pDomainShader);
        SAFE_RELEASE(pShaderProgram->mDx11.pHullShader);
        SAFE_RELEASE(pShaderProgram->mDx11.pInputSignature);
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
    case RESOURCE_MEMORY_USAGE_GPU_ONLY:
        return (D3D11_CPU_ACCESS_FLAG)0;
    case RESOURCE_MEMORY_USAGE_CPU_ONLY:
        return (D3D11_CPU_ACCESS_FLAG)(D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE);
    case RESOURCE_MEMORY_USAGE_CPU_TO_GPU:
        return (D3D11_CPU_ACCESS_FLAG)(D3D11_CPU_ACCESS_WRITE);
    case RESOURCE_MEMORY_USAGE_GPU_TO_CPU:
        return (D3D11_CPU_ACCESS_FLAG)(D3D11_CPU_ACCESS_READ);
    default:
        ASSERT(false && "Invalid Memory Usage");
        return (D3D11_CPU_ACCESS_FLAG)0;
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
    case RESOURCE_MEMORY_USAGE_GPU_ONLY:
        return D3D11_USAGE_DEFAULT;
    case RESOURCE_MEMORY_USAGE_CPU_TO_GPU:
        return D3D11_USAGE_DYNAMIC;
    case RESOURCE_MEMORY_USAGE_CPU_ONLY:
    case RESOURCE_MEMORY_USAGE_GPU_TO_CPU:
        return D3D11_USAGE_STAGING;
    default:
        ASSERT(false && "Invalid Memory Usage");
        return D3D11_USAGE_DEFAULT;
    }
}

void d3d11_addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
    // verify renderer validity
    ASSERT(pRenderer);
    // verify adding at least 1 buffer
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
    // add to renderer
    //  Align the buffer size to multiples of 256
    if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER))
    {
        allocationSize = round_up_64(allocationSize, pRenderer->pGpu->mSettings.mUniformBufferAlignment);
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
        desc.BindFlags &= ~(D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER | D3D11_BIND_CONSTANT_BUFFER | D3D11_BIND_STREAM_OUTPUT |
                            D3D11_BIND_RENDER_TARGET | D3D11_BIND_DEPTH_STENCIL);
    }
    CHECK_HRESULT_DEVICE(pRenderer->mDx11.pDevice, pRenderer->mDx11.pDevice->CreateBuffer(&desc, NULL, &pBuffer->mDx11.pResource));

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
            srvDesc.Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mFormat);
        }

        pRenderer->mDx11.pDevice->CreateShaderResourceView(pBuffer->mDx11.pResource, &srvDesc, &pBuffer->mDx11.pSrvHandle);
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

        pRenderer->mDx11.pDevice->CreateUnorderedAccessView(pBuffer->mDx11.pResource, &uavDesc, &pBuffer->mDx11.pUavHandle);
    }

    if (pDesc->pName)
    {
        setBufferName(pRenderer, pBuffer, pDesc->pName);
    }

    pBuffer->mSize = (uint32_t)pDesc->mSize;
    pBuffer->mMemoryUsage = pDesc->mMemoryUsage;
    pBuffer->mNodeIndex = pDesc->mNodeIndex;
    pBuffer->mDescriptors = pDesc->mDescriptors;
    pBuffer->mDx11.mFlags = pDesc->mFlags;

    *ppBuffer = pBuffer;
}

void d3d11_removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRenderer);
    ASSERT(pBuffer);

    SAFE_RELEASE(pBuffer->mDx11.pSrvHandle);
    SAFE_RELEASE(pBuffer->mDx11.pUavHandle);
    SAFE_RELEASE(pBuffer->mDx11.pResource);
    SAFE_FREE(pBuffer);
}

void d3d11_mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
    UNREF_PARAM(pRange);

    D3D11_MAP           mapType = {};
    ResourceMemoryUsage mem = (ResourceMemoryUsage)pBuffer->mMemoryUsage;
    switch (mem)
    {
    case RESOURCE_MEMORY_USAGE_CPU_ONLY:
        mapType = D3D11_MAP_READ_WRITE;
        break;
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
            else if (pRenderer->pGpu->mDx11.mPartialUpdateConstantBufferSupported)
                mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
            else
                LOGF(LogLevel::eERROR, "Device doesn't support partial uniform buffer updates");
        }
        else
        {
            mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
        }
        break;
    case RESOURCE_MEMORY_USAGE_GPU_TO_CPU:
        mapType = D3D11_MAP_READ;
        break;
    default:
        break;
    }
    D3D11_MAPPED_SUBRESOURCE sub = {};
    CHECK_HRESULT_DEVICE(pRenderer->mDx11.pDevice, pRenderer->mDx11.pContext->Map(pBuffer->mDx11.pResource, 0, mapType, 0, &sub));
    pBuffer->pCpuMappedAddress = sub.pData;
}

void d3d11_unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
    pRenderer->mDx11.pContext->Unmap(pBuffer->mDx11.pResource, 0);
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
        pTexture->mDx11.pUAVDescriptors = (ID3D11UnorderedAccessView**)(pTexture + 1);

    if (pDesc->pNativeHandle)
    {
        pTexture->mOwnsImage = false;
        pTexture->mDx11.pResource = (ID3D11Resource*)pDesc->pNativeHandle;
    }
    else
    {
        pTexture->mOwnsImage = true;
    }

    // add to gpu
    DXGI_FORMAT              dxFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(pDesc->mFormat);
    DescriptorType           descriptors = pDesc->mDescriptors;
    D3D11_RESOURCE_DIMENSION res_dim = {};

    bool featureLevel11 = pRenderer->pGpu->mSettings.mFeatureLevel >= D3D_FEATURE_LEVEL_11_0;

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

    if (NULL == pTexture->mDx11.pResource)
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
            CHECK_HRESULT_DEVICE(pRenderer->mDx11.pDevice, pRenderer->mDx11.pDevice->CreateTexture1D(&desc, NULL, &pTex1D));
            pTexture->mDx11.pResource = pTex1D;
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
            CHECK_HRESULT_DEVICE(pRenderer->mDx11.pDevice, pRenderer->mDx11.pDevice->CreateTexture2D(&desc, NULL, &pTex2D));
            pTexture->mDx11.pResource = pTex2D;
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
            CHECK_HRESULT_DEVICE(pRenderer->mDx11.pDevice, pRenderer->mDx11.pDevice->CreateTexture3D(&desc, NULL, &pTex3D));
            pTexture->mDx11.pResource = pTex3D;
            break;
        }
        default:
            break;
        }
    }
    else
    {
        D3D11_RESOURCE_DIMENSION type = {};
        pTexture->mDx11.pResource->GetType(&type);
        switch (type)
        {
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
        {
            D3D11_TEXTURE1D_DESC desc = {};
            ((ID3D11Texture1D*)pTexture->mDx11.pResource)->GetDesc(&desc);
            dxFormat = desc.Format;
            break;
        }
        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
        {
            D3D11_TEXTURE2D_DESC desc = {};
            ((ID3D11Texture2D*)pTexture->mDx11.pResource)->GetDesc(&desc);
            dxFormat = desc.Format;
            break;
        }
        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
        {
            D3D11_TEXTURE3D_DESC desc = {};
            ((ID3D11Texture3D*)pTexture->mDx11.pResource)->GetDesc(&desc);
            dxFormat = desc.Format;
            break;
        }
        default:
            break;
        }
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC  srvDesc = {};
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

    D3D11_RESOURCE_DIMENSION type = {};
    pTexture->mDx11.pResource->GetType(&type);

    switch (type)
    {
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
    {
        D3D11_TEXTURE1D_DESC desc = {};
        ((ID3D11Texture1D*)pTexture->mDx11.pResource)->GetDesc(&desc);

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
        ((ID3D11Texture2D*)pTexture->mDx11.pResource)->GetDesc(&desc);
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
        ((ID3D11Texture3D*)pTexture->mDx11.pResource)->GetDesc(&desc);

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
    default:
        break;
    }

    if (descriptors & DESCRIPTOR_TYPE_TEXTURE)
    {
        ASSERT(srvDesc.ViewDimension != D3D11_SRV_DIMENSION_UNKNOWN);

        srvDesc.Format = util_to_dx11_srv_format(dxFormat);
        pRenderer->mDx11.pDevice->CreateShaderResourceView(pTexture->mDx11.pResource, &srvDesc, &pTexture->mDx11.pSRVDescriptor);
    }

    if (descriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
    {
        uavDesc.Format = util_to_dx11_uav_format(dxFormat);
        for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
        {
            uavDesc.Texture1DArray.MipSlice = i;
            pRenderer->mDx11.pDevice->CreateUnorderedAccessView(pTexture->mDx11.pResource, &uavDesc, &pTexture->mDx11.pUAVDescriptors[i]);
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

    SAFE_RELEASE(pTexture->mDx11.pSRVDescriptor);
    if (pTexture->mDx11.pUAVDescriptors)
    {
        for (uint32_t i = 0; i < pTexture->mMipLevels; ++i)
            SAFE_RELEASE(pTexture->mDx11.pUAVDescriptors[i]);
    }

    if (pTexture->mOwnsImage)
        SAFE_RELEASE(pTexture->mDx11.pResource);

    SAFE_FREE(pTexture);
}
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
        Sampler*        pSampler;
    } StaticSamplerInfo;

    StaticSamplerInfo* staticSamplers = NULL;
    arrsetcap(staticSamplers, pRootSignatureDesc->mStaticSamplerCount);
    ShaderResource* shaderResources = NULL;
    uint32_t*       constantSizes = NULL;
    arrsetcap(shaderResources, 64);
    arrsetcap(constantSizes, 64);

    ShaderStage         shaderStages = SHADER_STAGE_NONE;
    bool                useInputLayout = false;
    DescriptorIndexMap* indexMap = NULL;
    PipelineType        pipelineType = PIPELINE_TYPE_UNDEFINED;
    sh_new_arena(indexMap);

    typedef struct StaticSamplerNode
    {
        char*    key;
        Sampler* value;
    } StaticSamplerNode;

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

                // shaderStages |= pRes->used_stages;
                arrpush(constantSizes, constantSize);
            }
            // If the resource was already collected, just update the shader stage mask in case it is used in a different
            // shader stage in this case
            else
            {
                if (shaderResources[pNode->value].reg != pRes->reg) //-V::522, 595
                {
                    LOGF(eERROR,
                         "\nFailed to create root signature\n"
                         "Shared shader resource %s has mismatching register. All shader resources "
                         "shared by multiple shaders specified in addRootSignature "
                         "have the same register and space",
                         pRes->name);
                    return;
                }
                if (shaderResources[pNode->value].set != pRes->set) //-V::522, 595
                {
                    LOGF(eERROR,
                         "\nFailed to create root signature\n"
                         "Shared shader resource %s has mismatching space. All shader resources "
                         "shared by multiple shaders specified in addRootSignature "
                         "have the same register and space",
                         pRes->name);
                    return;
                }

                for (ptrdiff_t j = 0; j < arrlen(shaderResources); ++j)
                {
                    // ShaderResource* pRes = &shaderResources[j];
                    if (strcmp(shaderResources[j].name, pNode->key) == 0)
                    {
                        shaderResources[j].used_stages |= pRes->used_stages;
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

    pRootSignature->pDescriptors = (DescriptorInfo*)(pRootSignature + 1); //-V1027
    pRootSignature->pDescriptorNameToIndexMap = indexMap;

    pRootSignature->mPipelineType = pipelineType;

    // Fill the descriptor array to be stored in the root signature
    for (ptrdiff_t i = 0; i < arrlen(shaderResources); ++i)
    {
        DescriptorInfo* pDesc = &pRootSignature->pDescriptors[i];
        ShaderResource* pRes = &shaderResources[i];
        uint32_t        setIndex = pRes->set;
        if (pRes->size == 0)
            setIndex = 0;

        DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)setIndex;

        pDesc->mDx11.mReg = pRes->reg;
        pDesc->mSize = pRes->size;
        pDesc->mType = pRes->type;
        pDesc->mDx11.mUsedStages = pRes->used_stages;
        pDesc->mUpdateFrequency = updateFreq;
        pDesc->pName = pRes->name;
        pDesc->mHandleIndex = 0;

        DescriptorType type = pRes->type;
        switch (type)
        {
        case DESCRIPTOR_TYPE_TEXTURE:
        case DESCRIPTOR_TYPE_BUFFER:
        case DESCRIPTOR_TYPE_BUFFER_RAW:
            pDesc->mHandleIndex = pRootSignature->mDx11.mSrvCounts[updateFreq]++;
            break;
        case DESCRIPTOR_TYPE_RW_TEXTURE:
        case DESCRIPTOR_TYPE_RW_BUFFER:
        case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
            pDesc->mHandleIndex = pRootSignature->mDx11.mUavCounts[updateFreq]++;
            break;
        default:
            break;
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
                pDesc->mHandleIndex = pRootSignature->mDx11.mSamplerCounts[updateFreq]++;
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
                pDesc->mHandleIndex = pRootSignature->mDx11.mDynamicCbvCount[updateFreq]++;
            }
        }

        if (DESCRIPTOR_TYPE_UNIFORM_BUFFER == pDesc->mType && !pDesc->mRootDescriptor)
            pDesc->mHandleIndex = pRootSignature->mDx11.mCbvCounts[updateFreq]++;
    }

    pRootSignature->mDx11.mStaticSamplerCount = (uint32_t)arrlenu(staticSamplers);
    if (pRootSignature->mDx11.mStaticSamplerCount)
    {
        pRootSignature->mDx11.ppStaticSamplers = (ID3D11SamplerState**)tf_calloc(arrlenu(staticSamplers), sizeof(ID3D11SamplerState*));
        pRootSignature->mDx11.pStaticSamplerStages = (ShaderStage*)tf_calloc(arrlenu(staticSamplers), sizeof(ShaderStage));
        pRootSignature->mDx11.pStaticSamplerSlots = (uint32_t*)tf_calloc(arrlenu(staticSamplers), sizeof(uint32_t));
        for (uint32_t i = 0; i < pRootSignature->mDx11.mStaticSamplerCount; ++i)
        {
            pRootSignature->mDx11.ppStaticSamplers[i] = staticSamplers[i].pSampler->mDx11.pSamplerState;
            pRootSignature->mDx11.pStaticSamplerStages[i] = (ShaderStage)staticSamplers[i].pDescriptorInfo->mDx11.mUsedStages;
            pRootSignature->mDx11.pStaticSamplerSlots[i] = staticSamplers[i].pDescriptorInfo->mDx11.mReg;
        }
    }

    shfree(staticSamplerMap);
    arrfree(constantSizes);
    arrfree(shaderResources);
    arrfree(staticSamplers);

    *ppRootSignature = pRootSignature;
}

void d3d11_removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRootSignature);

    shfree(pRootSignature->pDescriptorNameToIndexMap);

    SAFE_FREE(pRootSignature->mDx11.ppStaticSamplers);
    SAFE_FREE(pRootSignature->mDx11.pStaticSamplerStages);
    SAFE_FREE(pRootSignature->mDx11.pStaticSamplerSlots);
    tf_free(pRootSignature);
}

uint32_t d3d11_getDescriptorIndexFromName(const RootSignature* pRootSignature, const char* pName)
{
    for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
    {
        if (!strcmp(pName, pRootSignature->pDescriptors[i].pName))
        {
            return i;
        }
    }

    return UINT32_MAX;
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

    pPipeline->mDx11.mType = PIPELINE_TYPE_GRAPHICS;

    // add to gpu
    if (pShaderProgram->mStages & SHADER_STAGE_VERT)
    {
        pPipeline->mDx11.pVertexShader = pShaderProgram->mDx11.pVertexShader;
    }
    if (pShaderProgram->mStages & SHADER_STAGE_FRAG)
    {
        pPipeline->mDx11.pPixelShader = pShaderProgram->mDx11.pPixelShader;
    }
    if (pShaderProgram->mStages & SHADER_STAGE_HULL)
    {
        pPipeline->mDx11.pHullShader = pShaderProgram->mDx11.pHullShader;
    }
    if (pShaderProgram->mStages & SHADER_STAGE_DOMN)
    {
        pPipeline->mDx11.pDomainShader = pShaderProgram->mDx11.pDomainShader;
    }
    if (pShaderProgram->mStages & SHADER_STAGE_GEOM)
    {
        pPipeline->mDx11.pGeometryShader = pShaderProgram->mDx11.pGeometryShader;
    }

    uint32_t input_elementCount = 0;
    DECLARE_ZERO(D3D11_INPUT_ELEMENT_DESC, input_elements[MAX_VERTEX_ATTRIBS]);

    // Make sure there's attributes
    if (pVertexLayout != NULL)
    {
        DECLARE_ZERO(char, semantic_names[MAX_VERTEX_ATTRIBS][MAX_SEMANTIC_NAME_LENGTH]);

        ASSERT(pVertexLayout->mAttribCount && pVertexLayout->mBindingCount);

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
                case SEMANTIC_POSITION:
                    sprintf_s(name, "POSITION");
                    break;
                case SEMANTIC_NORMAL:
                    sprintf_s(name, "NORMAL");
                    break;
                case SEMANTIC_COLOR:
                    sprintf_s(name, "COLOR");
                    break;
                case SEMANTIC_TANGENT:
                    sprintf_s(name, "TANGENT");
                    break;
                case SEMANTIC_BITANGENT:
                    sprintf_s(name, "BINORMAL");
                    break;
                case SEMANTIC_JOINTS:
                    sprintf_s(name, "JOINTS");
                    break;
                case SEMANTIC_WEIGHTS:
                    sprintf_s(name, "WEIGHTS");
                    break;
                case SEMANTIC_TEXCOORD0:
                    sprintf_s(name, "TEXCOORD");
                    break;
                case SEMANTIC_TEXCOORD1:
                    sprintf_s(name, "TEXCOORD");
                    break;
                case SEMANTIC_TEXCOORD2:
                    sprintf_s(name, "TEXCOORD");
                    break;
                case SEMANTIC_TEXCOORD3:
                    sprintf_s(name, "TEXCOORD");
                    break;
                case SEMANTIC_TEXCOORD4:
                    sprintf_s(name, "TEXCOORD");
                    break;
                case SEMANTIC_TEXCOORD5:
                    sprintf_s(name, "TEXCOORD");
                    break;
                case SEMANTIC_TEXCOORD6:
                    sprintf_s(name, "TEXCOORD");
                    break;
                case SEMANTIC_TEXCOORD7:
                    sprintf_s(name, "TEXCOORD");
                    break;
                case SEMANTIC_TEXCOORD8:
                    sprintf_s(name, "TEXCOORD");
                    break;
                case SEMANTIC_TEXCOORD9:
                    sprintf_s(name, "TEXCOORD");
                    break;
                default:
                    break;
                }
                ASSERT(0 != name[0]);
                strncpy_s(semantic_names[attrib_index], name, strlen(name));
            }

            UINT semantic_index = 0;
            switch (attrib->mSemantic)
            {
            case SEMANTIC_TEXCOORD0:
                semantic_index = 0;
                break;
            case SEMANTIC_TEXCOORD1:
                semantic_index = 1;
                break;
            case SEMANTIC_TEXCOORD2:
                semantic_index = 2;
                break;
            case SEMANTIC_TEXCOORD3:
                semantic_index = 3;
                break;
            case SEMANTIC_TEXCOORD4:
                semantic_index = 4;
                break;
            case SEMANTIC_TEXCOORD5:
                semantic_index = 5;
                break;
            case SEMANTIC_TEXCOORD6:
                semantic_index = 6;
                break;
            case SEMANTIC_TEXCOORD7:
                semantic_index = 7;
                break;
            case SEMANTIC_TEXCOORD8:
                semantic_index = 8;
                break;
            case SEMANTIC_TEXCOORD9:
                semantic_index = 9;
                break;
            default:
                break;
            }

            input_elements[input_elementCount].SemanticName = semantic_names[attrib_index];
            input_elements[input_elementCount].SemanticIndex = semantic_index;
            input_elements[input_elementCount].Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(attrib->mFormat);
            input_elements[input_elementCount].InputSlot = attrib->mBinding;
            input_elements[input_elementCount].AlignedByteOffset = attrib->mOffset;
            if (pVertexLayout->mBindings[attrib->mBinding].mRate == VERTEX_BINDING_RATE_INSTANCE)
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
        pRenderer->mDx11.pDevice->CreateInputLayout(
            input_elements, input_elementCount, pDesc->pShaderProgram->mDx11.pInputSignature->GetBufferPointer(),
            pDesc->pShaderProgram->mDx11.pInputSignature->GetBufferSize(), &pPipeline->mDx11.pInputLayout);
    }

    if (pDesc->pRasterizerState)
        pPipeline->mDx11.pRasterizerState = util_to_rasterizer_state(pRenderer, pDesc->pRasterizerState);
    else
        pRenderer->mDx11.pDefaultRasterizerState->QueryInterface(&pPipeline->mDx11.pRasterizerState);

    if (pDesc->pDepthState)
        pPipeline->mDx11.pDepthState = util_to_depth_state(pRenderer, pDesc->pDepthState);
    else
        pRenderer->mDx11.pDefaultDepthState->QueryInterface(&pPipeline->mDx11.pDepthState);

    if (pDesc->pBlendState)
        pPipeline->mDx11.pBlendState = util_to_blend_state(pRenderer, pDesc->pBlendState);
    else
        pRenderer->mDx11.pDefaultBlendState->QueryInterface(&pPipeline->mDx11.pBlendState);

    D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    switch (pDesc->mPrimitiveTopo)
    {
    case PRIMITIVE_TOPO_POINT_LIST:
        topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        break;
    case PRIMITIVE_TOPO_LINE_LIST:
        topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        break;
    case PRIMITIVE_TOPO_LINE_STRIP:
        topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        break;
    case PRIMITIVE_TOPO_TRI_LIST:
        topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        break;
    case PRIMITIVE_TOPO_TRI_STRIP:
        topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        break;
    case PRIMITIVE_TOPO_PATCH_LIST:
    {
        const PipelineReflection* pReflection = pDesc->pShaderProgram->pReflection;
        uint32_t                  controlPoint = pReflection->mStageReflections[pReflection->mHullStageIndex].mNumControlPoint;
        topology = (D3D_PRIMITIVE_TOPOLOGY)(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (controlPoint - 1));
    }
    break;

    default:
        break;
    }
    ASSERT(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED != topology);
    pPipeline->mDx11.mPrimitiveTopology = topology;

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
        pPipeline->mDx11.mType = PIPELINE_TYPE_COMPUTE;
        pPipeline->mDx11.pComputeShader = pDesc->mComputeDesc.pShaderProgram->mDx11.pComputeShader;
        *ppPipeline = pPipeline;
        break;
    }
    case (PIPELINE_TYPE_GRAPHICS):
    {
        addGraphicsPipeline(pRenderer, &pDesc->mGraphicsDesc, ppPipeline);
        break;
    }
    default:
    {
        ASSERTFAIL("Unknown pipeline type %i", pDesc->mType);
        *ppPipeline = {};
        break;
    }
    }
}

void d3d11_removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
    ASSERT(pRenderer);
    ASSERT(pPipeline);

    // delete pipeline from device
    SAFE_RELEASE(pPipeline->mDx11.pInputLayout);
    SAFE_RELEASE(pPipeline->mDx11.pBlendState);
    SAFE_RELEASE(pPipeline->mDx11.pDepthState);
    SAFE_RELEASE(pPipeline->mDx11.pRasterizerState);
    SAFE_FREE(pPipeline);
}

void d3d11_addPipelineCache(Renderer*, const PipelineCacheDesc*, PipelineCache**) {}

void d3d11_removePipelineCache(Renderer*, PipelineCache*) {}

void                  d3d11_getPipelineCacheData(Renderer*, PipelineCache*, size_t*, void*) {}
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

    const RootSignature*            pRootSignature = pDesc->pRootSignature;
    const DescriptorUpdateFrequency updateFreq = pDesc->mUpdateFrequency;

    size_t totalSize = sizeof(DescriptorSet);
    totalSize += pDesc->mMaxSets * sizeof(DescriptorDataArray);
    totalSize += pDesc->mMaxSets * pRootSignature->mDx11.mSrvCounts[updateFreq] * sizeof(DescriptorHandle);
    totalSize += pDesc->mMaxSets * pRootSignature->mDx11.mUavCounts[updateFreq] * sizeof(DescriptorHandle);
    totalSize += pDesc->mMaxSets * pRootSignature->mDx11.mSamplerCounts[updateFreq] * sizeof(DescriptorHandle);
    totalSize += pDesc->mMaxSets * pRootSignature->mDx11.mCbvCounts[updateFreq] * sizeof(CBV);

    DescriptorSet* pDescriptorSet = (DescriptorSet*)tf_calloc_memalign(1, alignof(DescriptorSet), totalSize);
    pDescriptorSet->mDx11.mMaxSets = (uint16_t)pDesc->mMaxSets;
    pDescriptorSet->mDx11.pRootSignature = pRootSignature;
    pDescriptorSet->mDx11.mUpdateFrequency = updateFreq;

    pDescriptorSet->mDx11.pHandles = (DescriptorDataArray*)(pDescriptorSet + 1); //-V1027

    uint8_t* mem = (uint8_t*)(pDescriptorSet->mDx11.pHandles + pDesc->mMaxSets);

    for (uint32_t i = 0; i < pDesc->mMaxSets; ++i)
    {
        pDescriptorSet->mDx11.pHandles[i].pSRVs = (DescriptorHandle*)mem;
        mem += pRootSignature->mDx11.mSrvCounts[updateFreq] * sizeof(DescriptorHandle);

        pDescriptorSet->mDx11.pHandles[i].pUAVs = (DescriptorHandle*)mem;
        mem += pRootSignature->mDx11.mUavCounts[updateFreq] * sizeof(DescriptorHandle);

        pDescriptorSet->mDx11.pHandles[i].pSamplers = (DescriptorHandle*)mem;
        mem += pRootSignature->mDx11.mSamplerCounts[updateFreq] * sizeof(DescriptorHandle);

        pDescriptorSet->mDx11.pHandles[i].pCBVs = (CBV*)mem;
        mem += pRootSignature->mDx11.mCbvCounts[updateFreq] * sizeof(CBV);
    }

    *ppDescriptorSet = pDescriptorSet;
}

void d3d11_removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
{
    UNREF_PARAM(pRenderer);
    SAFE_FREE(pDescriptorSet);
}

#if defined(ENABLE_GRAPHICS_DEBUG) || defined(PVS_STUDIO)
#define VALIDATE_DESCRIPTOR(descriptor, msgFmt, ...)                           \
    if (!VERIFYMSG((descriptor), "%s : " msgFmt, __FUNCTION__, ##__VA_ARGS__)) \
    {                                                                          \
        continue;                                                              \
    }
#else
#define VALIDATE_DESCRIPTOR(descriptor, ...)
#endif

void d3d11_updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count,
                               const DescriptorData* pParams)
{
    ASSERT(pRenderer);
    ASSERT(pDescriptorSet);
    ASSERT(index < pDescriptorSet->mDx11.mMaxSets);

    const RootSignature* pRootSignature = pDescriptorSet->mDx11.pRootSignature;

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

                pDescriptorSet->mDx11.pHandles[index].pSamplers[paramIndex + arrayStart + arr] =
                    (DescriptorHandle{ pParam->ppSamplers[arr]->mDx11.pSamplerState, (ShaderStage)pDesc->mDx11.mUsedStages,
                                       pDesc->mDx11.mReg + arrayStart + arr });
            }
            break;
        }
        case DESCRIPTOR_TYPE_TEXTURE:
        {
            VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)", pDesc->pName);

            for (uint32_t arr = 0; arr < arrayCount; ++arr)
            {
                VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture (%s [%u] )", pDesc->pName, arr);

                pDescriptorSet->mDx11.pHandles[index].pSRVs[paramIndex + arrayStart + arr] =
                    (DescriptorHandle{ pParam->ppTextures[arr]->mDx11.pSRVDescriptor, (ShaderStage)pDesc->mDx11.mUsedStages,
                                       pDesc->mDx11.mReg + arrayStart + arr });
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
                VALIDATE_DESCRIPTOR(mipSlice < pParam->ppTextures[arr]->mMipLevels,
                                    "Descriptor : (%s [%u] ) Mip Slice (%u) exceeds mip levels (%u)", pDesc->pName, arr, mipSlice,
                                    pParam->ppTextures[arr]->mMipLevels);

                pDescriptorSet->mDx11.pHandles[index].pUAVs[paramIndex + arrayStart + arr] =
                    (DescriptorHandle{ pParam->ppTextures[arr]->mDx11.pUAVDescriptors[pParam->mUAVMipSlice],
                                       (ShaderStage)pDesc->mDx11.mUsedStages, pDesc->mDx11.mReg + arrayStart + arr });
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

                pDescriptorSet->mDx11.pHandles[index].pSRVs[paramIndex + arrayStart + arr] =
                    (DescriptorHandle{ pParam->ppBuffers[arr]->mDx11.pSrvHandle, (ShaderStage)pDesc->mDx11.mUsedStages,
                                       pDesc->mDx11.mReg + arrayStart + arr });
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

                pDescriptorSet->mDx11.pHandles[index].pUAVs[paramIndex + arrayStart + arr] =
                    (DescriptorHandle{ pParam->ppBuffers[arr]->mDx11.pUavHandle, (ShaderStage)pDesc->mDx11.mUsedStages,
                                       pDesc->mDx11.mReg + arrayStart + arr });
            }
            break;
        }
        case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        {
            VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Uniform Buffer (%s)", pDesc->pName);

            if (pDesc->mRootDescriptor)
            {
                VALIDATE_DESCRIPTOR(false,
                                    "Descriptor (%s) - Trying to update a root cbv through updateDescriptorSet. All root cbvs must be "
                                    "updated through cmdBindDescriptorSetWithRootCbvs",
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
                        VALIDATE_DESCRIPTOR(range.mSize <= D3D11_REQ_CONSTANT_BUFFER_SIZE,
                                            "Descriptor (%s) - pRanges[%u].mSize is %u which exceeds max size %u", pDesc->pName, arr,
                                            range.mSize, D3D11_REQ_CONSTANT_BUFFER_SIZE);
                    }

                    pDescriptorSet->mDx11.pHandles[index].pCBVs[paramIndex + arrayStart + arr] = (CBV{
                        pParam->ppBuffers[arr]->mDx11.pResource,
                        pParam->pRanges ? (uint32_t)pParam->pRanges[arr].mOffset : 0U,
                        pParam->pRanges ? (uint32_t)pParam->pRanges[arr].mSize : 0U,
                        (ShaderStage)pDesc->mDx11.mUsedStages,
                        pDesc->mDx11.mReg + arrayStart + arr,
                    });
                }
            }
            break;
        }
        default:
            break;
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
    UNREF_PARAM(pCmd);
    // TODO: Maybe put lock here if required.
}

void d3d11_endCmd(Cmd* pCmd)
{
    UNREF_PARAM(pCmd);
    // TODO: Maybe put unlock here if required.
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

void set_shader_resources(ID3D11DeviceContext* pContext, ShaderStage used_stages, uint32_t reg, uint32_t count,
                          ID3D11ShaderResourceView** pSRVs)
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
void d3d11_cmdBindRenderTargets(Cmd* pCmd, const BindRenderTargetsDesc* pDesc)
{
    ASSERT(pCmd);

    Renderer*            pRenderer = pCmd->pRenderer;
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

    if (!pDesc)
    {
        return;
    }

    if (!pDesc->mRenderTargetCount && !pDesc->mDepthStencil.pDepthStencil)
    {
        pContext->OMSetRenderTargets(0, NULL, NULL);
        return;
    }

    // Reset any shader settings left from previous render passes.
    reset_shader_resources(pContext);

    // Remove all rendertargets from previous render passes.
    static ID3D11RenderTargetView* pNullRTV[MAX_RENDER_TARGET_ATTACHMENTS] = { NULL };
    pContext->OMSetRenderTargets(MAX_RENDER_TARGET_ATTACHMENTS, pNullRTV, NULL);

    ID3D11RenderTargetView* ppTargets[MAX_RENDER_TARGET_ATTACHMENTS] = { 0 };

    // Make a list of d3d11 rendertargets to bind.
    for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i)
    {
        const BindRenderTargetDesc* desc = &pDesc->mRenderTargets[i];
        if (!desc->mUseMipSlice && !desc->mUseArraySlice)
        {
            ppTargets[i] = desc->pRenderTarget->mDx11.pRtvDescriptor;
        }
        else
        {
            uint32_t handle = 0;
            if (desc->mUseMipSlice)
            {
                if (desc->mUseArraySlice)
                {
                    handle = desc->mMipSlice * desc->pRenderTarget->mArraySize + desc->mArraySlice;
                }
                else
                {
                    handle = desc->mMipSlice;
                }
            }
            else if (desc->mUseArraySlice)
            {
                handle = desc->mArraySlice;
            }

            ppTargets[i] = desc->pRenderTarget->mDx11.pRtvSliceDescriptors[handle];
        }

        if (desc->mLoadAction == LOAD_ACTION_CLEAR)
        {
            const FLOAT* clear = (FLOAT*)(desc->mOverrideClearValue ? &desc->mClearValue.r : &desc->pRenderTarget->mClearValue.r);
            pContext->ClearRenderTargetView(ppTargets[i], clear);
        }
    }

    // Get the d3d11 depth/stencil target to bind.
    ID3D11DepthStencilView* pDepthStencilTarget = 0;

    if (pDesc->mDepthStencil.pDepthStencil)
    {
        const BindDepthTargetDesc* desc = &pDesc->mDepthStencil;
        if (!desc->mUseMipSlice && !desc->mUseArraySlice)
        {
            pDepthStencilTarget = desc->pDepthStencil->mDx11.pDsvDescriptor;
        }
        else
        {
            uint32_t handle = 0;
            if (desc->mUseMipSlice)
            {
                if (desc->mUseArraySlice)
                    handle = desc->mMipSlice * desc->pDepthStencil->mArraySize + desc->mArraySlice;
                else
                    handle = desc->mMipSlice;
            }
            else if (desc->mUseArraySlice)
            {
                handle = desc->mArraySlice;
            }
            pDepthStencilTarget = desc->pDepthStencil->mDx11.pDsvSliceDescriptors[handle];
        }

        if (desc->mLoadAction == LOAD_ACTION_CLEAR || desc->mLoadActionStencil == LOAD_ACTION_CLEAR)
        {
            uint32_t clearFlag = 0;
            if (desc->mLoadAction == LOAD_ACTION_CLEAR)
            {
                clearFlag |= D3D11_CLEAR_DEPTH;
            }
            if (desc->mLoadActionStencil == LOAD_ACTION_CLEAR)
            {
                clearFlag |= D3D11_CLEAR_STENCIL;
                ASSERT(TinyImageFormat_HasStencil(desc->pDepthStencil->mFormat));
            }

            const ClearValue* clear = desc->mOverrideClearValue ? &desc->mClearValue : &desc->pDepthStencil->mClearValue;
            pContext->ClearDepthStencilView(pDepthStencilTarget, clearFlag, clear->depth, (UINT8)clear->stencil);
        }
    }

    // Bind the render targets and the depth-stencil target.
    pContext->OMSetRenderTargets(pDesc->mRenderTargetCount, ppTargets, pDepthStencilTarget);
}

void d3d11_cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
    ASSERT(pCmd);

    Renderer*            pRenderer = pCmd->pRenderer;
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

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
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

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
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

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
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

    static const float dummyClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (pCmd->pRenderer->pGpu->mSettings.mFeatureLevel >= D3D_FEATURE_LEVEL_11_0)
    {
        reset_uavs(pContext);
    }

    if (pPipeline->mDx11.mType == PIPELINE_TYPE_GRAPHICS)
    {
        // Set InputAssembly, Raster and Output state settings.
        pContext->IASetPrimitiveTopology(pPipeline->mDx11.mPrimitiveTopology);
        pContext->IASetInputLayout(pPipeline->mDx11.pInputLayout);
        pContext->RSSetState(pPipeline->mDx11.pRasterizerState);
        pContext->OMSetDepthStencilState(pPipeline->mDx11.pDepthState, 0);
        pContext->OMSetBlendState(pPipeline->mDx11.pBlendState, dummyClearColor, ~0u);

        // Set shaders.
        pContext->VSSetShader(pPipeline->mDx11.pVertexShader, NULL, 0);
        pContext->PSSetShader(pPipeline->mDx11.pPixelShader, NULL, 0);
        pContext->GSSetShader(pPipeline->mDx11.pGeometryShader, NULL, 0);
        pContext->DSSetShader(pPipeline->mDx11.pDomainShader, NULL, 0);
        pContext->HSSetShader(pPipeline->mDx11.pHullShader, NULL, 0);
    }
    else
    {
        pContext->RSSetState(NULL);
        pContext->OMSetDepthStencilState(NULL, 0);
        pContext->OMSetBlendState(NULL, dummyClearColor, ~0u);
        reset_shader_resources(pContext);
        pContext->CSSetShader(pPipeline->mDx11.pComputeShader, NULL, 0);
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

void set_constant_buffer_offset(ID3D11DeviceContext1* pContext1, ShaderStage used_stages, uint32_t reg, uint32_t offset, uint32_t size,
                                ID3D11Buffer* pCBV)
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
    ASSERT(index < pDescriptorSet->mDx11.mMaxSets);

    Renderer*             pRenderer = pCmd->pRenderer;
    ID3D11DeviceContext*  pContext = pRenderer->mDx11.pContext;
    ID3D11DeviceContext1* pContext1 = pRenderer->mDx11.pContext1;
    const uint32_t        updateFreq = pDescriptorSet->mDx11.mUpdateFrequency;

    const RootSignature* pRootSignature = pDescriptorSet->mDx11.pRootSignature;

    // Texture samplers.
    for (uint32_t i = 0; i < pRootSignature->mDx11.mStaticSamplerCount; ++i)
    {
        set_samplers(pContext, pRootSignature->mDx11.pStaticSamplerStages[i], pRootSignature->mDx11.pStaticSamplerSlots[i], 1,
                     &pRootSignature->mDx11.ppStaticSamplers[i]);
    }

    // Shader resources.
    for (uint32_t i = 0; i < pRootSignature->mDx11.mSrvCounts[updateFreq]; ++i)
    {
        DescriptorHandle* pHandle = &pDescriptorSet->mDx11.pHandles[index].pSRVs[i];
        if (pHandle->pHandle)
            set_shader_resources(pContext, pHandle->mStage, pHandle->mBinding, 1, (ID3D11ShaderResourceView**)&pHandle->pHandle);
    }

    // UAVs
    for (uint32_t i = 0; i < pRootSignature->mDx11.mUavCounts[updateFreq]; ++i)
    {
        DescriptorHandle* pHandle = &pDescriptorSet->mDx11.pHandles[index].pUAVs[i];
        if (pHandle->pHandle)
            pContext->CSSetUnorderedAccessViews(pHandle->mBinding, 1, (ID3D11UnorderedAccessView**)&pHandle->pHandle, NULL);
    }

    // Constant buffers(Uniform buffers)
    for (uint32_t i = 0; i < pRootSignature->mDx11.mCbvCounts[updateFreq]; ++i)
    {
        CBV* pHandle = &pDescriptorSet->mDx11.pHandles[index].pCBVs[i];
        if (pHandle->pHandle)
        {
            if (pHandle->mOffset || pHandle->mSize)
            {
                ASSERT(pContext1);
                set_constant_buffer_offset(pContext1, pHandle->mStage, pHandle->mBinding, pHandle->mOffset, pHandle->mSize,
                                           pHandle->pHandle);
            }
            else
            {
                set_constant_buffers(pContext, pHandle->mStage, pHandle->mBinding, 1, (ID3D11Buffer**)&pHandle->pHandle);
            }
        }
    }

    for (uint32_t i = 0; i < pRootSignature->mDx11.mSamplerCounts[updateFreq]; ++i)
    {
        DescriptorHandle* pHandle = &pDescriptorSet->mDx11.pHandles[index].pSamplers[i];
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
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

    if (!pCmd->mDx11.pRootConstantBuffer)
    {
        Buffer*    buffer = {};
        BufferDesc bufDesc = {};
        bufDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bufDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        bufDesc.mSize = 256;
        addBuffer(pCmd->pRenderer, &bufDesc, &buffer);
        pCmd->mDx11.pRootConstantBuffer = buffer->mDx11.pResource;
        SAFE_FREE(buffer);
    }

    D3D11_MAPPED_SUBRESOURCE sub = {};
    const DescriptorInfo*    pDesc = pRootSignature->pDescriptors + paramIndex;

    pContext->Map(pCmd->mDx11.pRootConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
    memcpy(sub.pData, pConstants, pDesc->mSize * sizeof(uint32_t));
    pContext->Unmap(pCmd->mDx11.pRootConstantBuffer, 0);
    set_constant_buffers(pContext, (ShaderStage)pDesc->mDx11.mUsedStages, pDesc->mDx11.mReg, 1, &pCmd->mDx11.pRootConstantBuffer);
}

void d3d11_cmdBindDescriptorSetWithRootCbvs(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count,
                                            const DescriptorData* pParams)
{
    ASSERT(pCmd);
    ASSERT(pDescriptorSet);
    ASSERT(pParams);

    d3d11_cmdBindDescriptorSet(pCmd, index, pDescriptorSet);

    const RootSignature*  pRootSignature = pDescriptorSet->mDx11.pRootSignature;
    ID3D11DeviceContext1* pContext1 = pCmd->pRenderer->mDx11.pContext1;
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
        VALIDATE_DESCRIPTOR(pParam->mCount <= 1, "Descriptor (%s) - cmdBindDescriptorSetWithRootCbvs does not support arrays",
                            pDesc->pName);
        VALIDATE_DESCRIPTOR(pParam->pRanges, "Descriptor (%s) - pRanges must be provided for cmdBindDescriptorSetWithRootCbvs",
                            pDesc->pName);

        DescriptorDataRange range = pParam->pRanges[0];
        VALIDATE_DESCRIPTOR(range.mSize > 0, "Descriptor (%s) - pRange->mSize is zero", pDesc->pName);
        VALIDATE_DESCRIPTOR(range.mSize <= D3D11_REQ_CONSTANT_BUFFER_SIZE,
                            "Descriptor (%s) - pRange->mSize is %u which exceeds max size %u", pDesc->pName, range.mSize,
                            D3D11_REQ_CONSTANT_BUFFER_SIZE);

        set_constant_buffer_offset(pContext1, (ShaderStage)pDesc->mDx11.mUsedStages, pDesc->mDx11.mReg, range.mOffset, range.mSize,
                                   pParam->ppBuffers[0]->mDx11.pResource);
    }
}

void d3d11_cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
    ASSERT(pCmd);
    ASSERT(pBuffer->mDx11.pResource);

    Renderer*            pRenderer = pCmd->pRenderer;
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

    pContext->IASetIndexBuffer((ID3D11Buffer*)pBuffer->mDx11.pResource,
                               INDEX_TYPE_UINT16 == indexType ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, (uint32_t)offset);
}

void d3d11_cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
{
    ASSERT(pCmd);
    ASSERT(bufferCount);
    ASSERT(ppBuffers);
    ASSERT(pStrides);

    Renderer*            pRenderer = pCmd->pRenderer;
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

    ID3D11Buffer* buffers[MAX_VERTEX_BINDINGS] = {};
    uint32_t      offsets[MAX_VERTEX_BINDINGS] = {};

    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        buffers[i] = ppBuffers[i]->mDx11.pResource;
        offsets[i] = (uint32_t)(pOffsets ? pOffsets[i] : 0);
    }

    pContext->IASetVertexBuffers(0, bufferCount, buffers, pStrides, offsets);
}

void d3d11_cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
    ASSERT(pCmd);

    Renderer*            pRenderer = pCmd->pRenderer;
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

    pContext->Draw(vertexCount, firstVertex);
}

void d3d11_cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
    ASSERT(pCmd);

    Renderer*            pRenderer = pCmd->pRenderer;
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

    pContext->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void d3d11_cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
    ASSERT(pCmd);

    Renderer*            pRenderer = pCmd->pRenderer;
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

    pContext->DrawIndexed(indexCount, firstIndex, firstVertex);
}

void d3d11_cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex,
                                   uint32_t firstInstance)
{
    ASSERT(pCmd);

    Renderer*            pRenderer = pCmd->pRenderer;
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

    pContext->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, firstVertex, firstInstance);
}

void d3d11_cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    ASSERT(pCmd);

    Renderer*            pRenderer = pCmd->pRenderer;
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

    pContext->Dispatch(groupCountX, groupCountY, groupCountZ);
}

/************************************************************************/
// Transition Commands
/************************************************************************/
void d3d11_cmdResourceBarrier(Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers,
                              TextureBarrier* pTextureBarriers, uint32_t numRtBarriers, RenderTargetBarrier* pRtBarriers)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(numBufferBarriers);
    UNREF_PARAM(pBufferBarriers);
    UNREF_PARAM(numTextureBarriers);
    UNREF_PARAM(pTextureBarriers);
    UNREF_PARAM(numRtBarriers);
    UNREF_PARAM(pRtBarriers);
}
/************************************************************************/
// Queue Fence Semaphore Functions
/************************************************************************/
void d3d11_acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence,
                            uint32_t* pSwapChainImageIndex)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pSignalSemaphore);
    UNREF_PARAM(pFence);
    // We only have read/write access to the first buffer.
    pSwapChain->mDx11.mImageIndex = 0;
    *pSwapChainImageIndex = pSwapChain->mDx11.mImageIndex;
}

static void util_wait_for_fence(ID3D11DeviceContext* pContext, Fence* pFence)
{
    HRESULT hres = S_FALSE;
    while (hres != S_OK && pFence->mDx11.mSubmitted)
    {
        hres = pContext->GetData(pFence->mDx11.pDX11Query, NULL, 0, 0);
        threadSleep(0);
    }
    pFence->mDx11.mSubmitted = false;
}

void d3d11_queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc)
{
    Fence* pFence = pDesc->pSignalFence;
    Cmd**  ppCmds = pDesc->ppCmds;
    pQueue->mDx11.pFence = pFence;
    ID3D11DeviceContext* pContext = ppCmds[0]->pRenderer->mDx11.pContext;

    if (pFence)
    {
        util_wait_for_fence(pContext, pFence);
        pFence->mDx11.mSubmitted = true;
        pContext->End(pFence->mDx11.pDX11Query);
    }
}

void d3d11_queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc)
{
    if (pDesc->pSwapChain)
    {
#if defined(AUTOMATED_TESTING)
        // take a screenshot
        captureScreenshot(pDesc->pSwapChain, pDesc->mIndex, true, false);
#endif

        HRESULT hr = pDesc->pSwapChain->mDx11.pSwapChain->Present(pDesc->pSwapChain->mDx11.mSyncInterval, 0);
        if (!SUCCEEDED(hr))
        {
            LOGF(eERROR, "%s: FAILED with HRESULT: %u", "pDesc->pSwapChain->pSwapChain->Present(pDesc->pSwapChain->mSyncInterval, 0)",
                 (uint32_t)hr);

#if defined(_WINDOWS)
            if (hr == DXGI_ERROR_DEVICE_REMOVED)
            {
                LOGF(eERROR, "ERROR_DEVICE_REMOVED: %u", (uint32_t)pQueue->mDx11.pDevice->GetDeviceRemovedReason());
                threadSleep(5000); // Wait for a few seconds to allow the driver to come back online before doing a reset.
                ResetDesc resetDescriptor;
                resetDescriptor.mType = RESET_TYPE_DEVICE_LOST;
                requestReset(&resetDescriptor);
            }
#endif

#if defined(ENABLE_NSIGHT_AFTERMATH)
            // DXGI_ERROR error notification is asynchronous to the NVIDIA display
            // driver's GPU crash handling. Give the Nsight Aftermath GPU crash dump
            // thread some time to do its work before terminating the process.
            threadSleep(3000);
#endif

            ASSERT(false);
        }
    }
}

void d3d11_getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
    UNREF_PARAM(pRenderer);

    if (pFence->mDx11.mSubmitted)
    {
        HRESULT hres = pRenderer->mDx11.pContext->GetData(pFence->mDx11.pDX11Query, NULL, 0, 0);
        if (hres == S_OK)
        {
            pFence->mDx11.mSubmitted = false;
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
        util_wait_for_fence(pRenderer->mDx11.pContext, ppFences[i]);
    }
}

void d3d11_waitQueueIdle(Queue* pQueue)
{
    if (pQueue && pQueue->mDx11.pFence && pQueue->mDx11.pFence->mDx11.mSubmitted)
    {
        util_wait_for_fence(pQueue->mDx11.pContext, pQueue->mDx11.pFence);
    }
}

void d3d11_toggleVSync(Renderer* pRenderer, SwapChain** ppSwapChain)
{
    UNREF_PARAM(pRenderer);
    // Initial vsync value is passed in with the desc when client creates a swapchain.
    ASSERT(*ppSwapChain);
    (*ppSwapChain)->mEnableVsync = !(*ppSwapChain)->mEnableVsync;

    // toggle vsync present flag (this can go up to 4 but we don't need to refresh on nth vertical sync)
    (*ppSwapChain)->mDx11.mSyncInterval = ((*ppSwapChain)->mDx11.mSyncInterval + 1) % 2;
}
/************************************************************************/
// Utility functions
/************************************************************************/
TinyImageFormat d3d11_getSupportedSwapchainFormat(Renderer* pRenderer, const SwapChainDesc* pDesc, ColorSpace colorSpace)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pDesc);
    if (COLOR_SPACE_SDR_LINEAR == colorSpace)
        return TinyImageFormat_B8G8R8A8_UNORM;
    else if (COLOR_SPACE_SDR_SRGB == colorSpace)
        return TinyImageFormat_B8G8R8A8_SRGB;
    return TinyImageFormat_UNDEFINED;
}

uint32_t d3d11_getRecommendedSwapchainImageCount(Renderer* pRenderer, const WindowHandle* hwnd)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(hwnd);
    return 2;
}
/************************************************************************/
// Indirect Draw functions
/************************************************************************/
void d3d11_addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pDesc);
    UNREF_PARAM(ppCommandSignature);
}

void d3d11_removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pCommandSignature);
}

void d3d11_cmdExecuteIndirect(Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer,
                              uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(pCommandSignature);
    UNREF_PARAM(maxCommandCount);
    UNREF_PARAM(pIndirectBuffer);
    UNREF_PARAM(bufferOffset);
    UNREF_PARAM(pCounterBuffer);
    UNREF_PARAM(counterBufferOffset);
}
/************************************************************************/
// GPU Query Implementation
/************************************************************************/
static inline FORGE_CONSTEXPR D3D11_QUERY ToDXQuery(QueryType type)
{
    switch (type)
    {
    case QUERY_TYPE_TIMESTAMP:
        return D3D11_QUERY_TIMESTAMP;
    case QUERY_TYPE_PIPELINE_STATISTICS:
        return D3D11_QUERY_PIPELINE_STATISTICS;
    case QUERY_TYPE_OCCLUSION:
        return D3D11_QUERY_OCCLUSION;
    default:
        return (D3D11_QUERY)-1;
    }
}

static inline FORGE_CONSTEXPR uint32_t ToQueryWidth(QueryType type)
{
    switch (type)
    {
    case QUERY_TYPE_PIPELINE_STATISTICS:
        return sizeof(D3D11_QUERY_DATA_PIPELINE_STATISTICS);
    default:
        return sizeof(uint64_t);
    }
}

void d3d11_getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
    ID3D11DeviceContext* pContext = pQueue->mDx11.pContext;
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
    {
        threadSleep(0);
    }
    *pFrequency = (double)data.Frequency;

    SAFE_RELEASE(pDevice);
    SAFE_RELEASE(pDisjointQuery);
}

void d3d11_addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppQueryPool);

    const uint32_t queryCount = pDesc->mQueryCount * (QUERY_TYPE_TIMESTAMP == pDesc->mType ? 2 : 1);

    QueryPool* pQueryPool = (QueryPool*)tf_calloc(1, sizeof(QueryPool) + queryCount * sizeof(ID3D11Query*));
    ASSERT(pQueryPool);

    pQueryPool->mCount = queryCount;
    pQueryPool->mStride = ToQueryWidth(pDesc->mType);
    pQueryPool->mDx11.mType = ToDXQuery(pDesc->mType);
    pQueryPool->mDx11.ppQueries = (ID3D11Query**)(pQueryPool + 1);

    D3D11_QUERY_DESC desc = {};
    desc.MiscFlags = 0;
    desc.Query = pQueryPool->mDx11.mType;

    for (uint32_t i = 0; i < queryCount; ++i)
    {
        pRenderer->mDx11.pDevice->CreateQuery(&desc, &pQueryPool->mDx11.ppQueries[i]);
    }

    *ppQueryPool = pQueryPool;
}

void d3d11_removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
    UNREF_PARAM(pRenderer);
    for (uint32_t i = 0; i < pQueryPool->mCount; ++i)
    {
        SAFE_RELEASE(pQueryPool->mDx11.ppQueries[i]);
    }
    SAFE_FREE(pQueryPool);
}

void d3d11_cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    ASSERT(pCmd);
    ASSERT(pQuery);

    Renderer*            pRenderer = pCmd->pRenderer;
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;
    D3D11_QUERY          type = pQueryPool->mDx11.mType;
    switch (type)
    {
    case D3D11_QUERY_TIMESTAMP:
    {
        const uint32_t index = pQuery->mIndex * 2;
        pContext->End(pQueryPool->mDx11.ppQueries[index]);
        break;
    }
    case D3D11_QUERY_OCCLUSION:
    case D3D11_QUERY_PIPELINE_STATISTICS:
    {
        const uint32_t index = pQuery->mIndex;
        pContext->Begin(pQueryPool->mDx11.ppQueries[index]);
        break;
    }
    default:
        ASSERT(false && "Not implemented");
    }
}

void d3d11_cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery) //-V524
{
    ASSERT(pCmd);
    ASSERT(pQuery);

    Renderer*            pRenderer = pCmd->pRenderer;
    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

    D3D11_QUERY type = pQueryPool->mDx11.mType;
    switch (type)
    {
    case D3D11_QUERY_TIMESTAMP:
    {
        const uint32_t index = pQuery->mIndex * 2 + 1;
        pContext->End(pQueryPool->mDx11.ppQueries[index]);
        break;
    }
    case D3D11_QUERY_OCCLUSION:
    case D3D11_QUERY_PIPELINE_STATISTICS:
    {
        const uint32_t index = pQuery->mIndex;
        pContext->End(pQueryPool->mDx11.ppQueries[index]);
        break;
    }
    default:
        ASSERT(false && "Not implemented");
    }
}

void d3d11_cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(pQueryPool);
    UNREF_PARAM(startQuery);
    UNREF_PARAM(queryCount);
}

void d3d11_cmdResetQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(pQueryPool);
    UNREF_PARAM(startQuery);
    UNREF_PARAM(queryCount);
}

void d3d11_getQueryData(Renderer* pRenderer, QueryPool* pQueryPool, uint32_t queryIndex, QueryData* pOutData)
{
    ASSERT(pRenderer);
    ASSERT(pQueryPool);
    ASSERT(pOutData);

    const D3D11_QUERY type = pQueryPool->mDx11.mType;
    *pOutData = {};

    ID3D11DeviceContext* pContext = pRenderer->mDx11.pContext;

    switch (type)
    {
    case D3D11_QUERY_TIMESTAMP:
    {
        HRESULT valid1 = pContext->GetData(pQueryPool->mDx11.ppQueries[queryIndex * 2], &pOutData->mBeginTimestamp, pQueryPool->mStride, 0);
        HRESULT valid2 =
            pContext->GetData(pQueryPool->mDx11.ppQueries[queryIndex * 2 + 1], &pOutData->mEndTimestamp, pQueryPool->mStride, 0);
        pOutData->mValid = S_OK == valid1 && S_OK == valid2;
        break;
    }
    case D3D11_QUERY_OCCLUSION:
    {
        pOutData->mValid =
            S_OK == pContext->GetData(pQueryPool->mDx11.ppQueries[queryIndex], &pOutData->mOcclusionCounts, pQueryPool->mStride, 0);
        break;
    }
    case D3D11_QUERY_PIPELINE_STATISTICS:
    {
        COMPILE_ASSERT(sizeof(pOutData->mPipelineStats) == sizeof(D3D11_QUERY_DATA_PIPELINE_STATISTICS));
        pOutData->mValid =
            S_OK == pContext->GetData(pQueryPool->mDx11.ppQueries[queryIndex], &pOutData->mPipelineStats, pQueryPool->mStride, 0);
        break;
    }
    default:
        ASSERT(false && "Not implemented");
    }
}
/************************************************************************/
// Memory Stats Implementation
/************************************************************************/
void d3d11_calculateMemoryStats(Renderer* pRenderer, char** stats)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(stats);
}

void d3d11_calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(usedBytes);
    UNREF_PARAM(totalAllocatedBytes);
}

void d3d11_freeMemoryStats(Renderer* pRenderer, char* stats)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(stats);
}
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void d3d11_cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
    UNREF_PARAM(r);
    UNREF_PARAM(g);
    UNREF_PARAM(b);
    if (!pCmd->pRenderer->mDx11.pUserDefinedAnnotation)
    {
        return;
    }
    wchar_t markerName[256] = { 0 };
    int     nameLen = int(strlen(pName));
    MultiByteToWideChar(0, 0, pName, nameLen, markerName, nameLen);
    pCmd->pRenderer->mDx11.pUserDefinedAnnotation->BeginEvent(markerName);
}

void d3d11_cmdEndDebugMarker(Cmd* pCmd)
{
    if (!pCmd->pRenderer->mDx11.pUserDefinedAnnotation)
    {
        return;
    }
    pCmd->pRenderer->mDx11.pUserDefinedAnnotation->EndEvent();
}

void d3d11_cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
    UNREF_PARAM(r);
    UNREF_PARAM(g);
    UNREF_PARAM(b);
    if (pCmd->pRenderer->mDx11.pUserDefinedAnnotation)
    {
        wchar_t markerName[256] = { 0 };
        int     nameLen = int(strlen(pName));
        MultiByteToWideChar(0, 0, pName, nameLen, markerName, nameLen);
        pCmd->pRenderer->mDx11.pUserDefinedAnnotation->SetMarker(markerName);
    }

#if defined(ENABLE_NSIGHT_AFTERMATH)
    SetAftermathMarker(&pCmd->pRenderer->mAftermathTracker, pCmd->pRenderer->mDx11.pContext, pName);
#endif
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void SetResourceName(ID3D11DeviceChild* pResource, const char* pName)
{
    UNREF_PARAM(pResource);
    UNREF_PARAM(pName);
#if defined(ENABLE_GRAPHICS_DEBUG)
    if (!pName)
    {
        return;
    }
    pResource->SetPrivateData(WKPDID_D3DDebugObjectName, (uint32_t)strlen(pName) + 1, pName);
#endif
}

void d3d11_setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pBuffer);
    UNREF_PARAM(pName);
#if defined(ENABLE_GRAPHICS_DEBUG)
    ASSERT(pRenderer);
    ASSERT(pBuffer);
    ASSERT(pName);

    UNREF_PARAM(pRenderer);
    SetResourceName(pBuffer->mDx11.pResource, pName);
#endif
}

void d3d11_setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pTexture);
    UNREF_PARAM(pName);
#if defined(ENABLE_GRAPHICS_DEBUG)
    ASSERT(pRenderer);
    ASSERT(pTexture);
    ASSERT(pName);

    UNREF_PARAM(pRenderer);
    SetResourceName(pTexture->mDx11.pResource, pName);
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

    // shader functions
    addShaderBinary = d3d11_addShaderBinary;
    removeShader = d3d11_removeShader;

    addRootSignature = d3d11_addRootSignature;
    removeRootSignature = d3d11_removeRootSignature;
    getDescriptorIndexFromName = d3d11_getDescriptorIndexFromName;

    // pipeline functions
    addPipeline = d3d11_addPipeline;
    removePipeline = d3d11_removePipeline;
    addPipelineCache = d3d11_addPipelineCache;
    getPipelineCacheData = d3d11_getPipelineCacheData;
    removePipelineCache = d3d11_removePipelineCache;
#if defined(SHADER_STATS_AVAILABLE)
    addPipelineStats = NULL;
    removePipelineStats = NULL;
#endif

    // Descriptor Set functions
    addDescriptorSet = d3d11_addDescriptorSet;
    removeDescriptorSet = d3d11_removeDescriptorSet;
    updateDescriptorSet = d3d11_updateDescriptorSet;

    // command buffer functions
    resetCmdPool = d3d11_resetCmdPool;
    beginCmd = d3d11_beginCmd;
    endCmd = d3d11_endCmd;
    cmdBindRenderTargets = d3d11_cmdBindRenderTargets;
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

    // queue/fence/swapchain functions
    acquireNextImage = d3d11_acquireNextImage;
    queueSubmit = d3d11_queueSubmit;
    queuePresent = d3d11_queuePresent;
    waitQueueIdle = d3d11_waitQueueIdle;
    getFenceStatus = d3d11_getFenceStatus;
    waitForFences = d3d11_waitForFences;
    toggleVSync = d3d11_toggleVSync;

    getSupportedSwapchainFormat = d3d11_getSupportedSwapchainFormat;
    getRecommendedSwapchainImageCount = d3d11_getRecommendedSwapchainImageCount;

    // indirect Draw functions
    addIndirectCommandSignature = d3d11_addIndirectCommandSignature;
    removeIndirectCommandSignature = d3d11_removeIndirectCommandSignature;
    cmdExecuteIndirect = d3d11_cmdExecuteIndirect;

    /************************************************************************/
    // GPU Query Interface
    /************************************************************************/
    getTimestampFrequency = d3d11_getTimestampFrequency;
    addQueryPool = d3d11_addQueryPool;
    removeQueryPool = d3d11_removeQueryPool;
    cmdBeginQuery = d3d11_cmdBeginQuery;
    cmdEndQuery = d3d11_cmdEndQuery;
    cmdResolveQuery = d3d11_cmdResolveQuery;
    cmdResetQuery = d3d11_cmdResetQuery;
    getQueryData = d3d11_getQueryData;
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

void initD3D11RendererContext(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext)
{
    // No need to initialize API function pointers, initRenderer MUST be called before using anything else anyway.
    d3d11_initRendererContext(appName, pSettings, ppContext);
}

void exitD3D11RendererContext(RendererContext* pContext)
{
    ASSERT(pContext);

    d3d11_exitRendererContext(pContext);
}
#endif

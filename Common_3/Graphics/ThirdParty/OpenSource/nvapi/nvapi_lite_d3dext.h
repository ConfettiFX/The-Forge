 /************************************************************************************************************************************\
|*                                                                                                                                    *|
|*     Copyright ?2012 NVIDIA Corporation.  All rights reserved.                                                                     *|
|*                                                                                                                                    *|
|*  NOTICE TO USER:                                                                                                                   *|
|*                                                                                                                                    *|
|*  This software is subject to NVIDIA ownership rights under U.S. and international Copyright laws.                                  *|
|*                                                                                                                                    *|
|*  This software and the information contained herein are PROPRIETARY and CONFIDENTIAL to NVIDIA                                     *|
|*  and are being provided solely under the terms and conditions of an NVIDIA software license agreement.                             *|
|*  Otherwise, you have no rights to use or access this software in any manner.                                                       *|
|*                                                                                                                                    *|
|*  If not covered by the applicable NVIDIA software license agreement:                                                               *|
|*  NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.                                            *|
|*  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.                                                           *|
|*  NVIDIA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,                                                                     *|
|*  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.                       *|
|*  IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,                               *|
|*  OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT,                         *|
|*  NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.            *|
|*                                                                                                                                    *|
|*  U.S. Government End Users.                                                                                                        *|
|*  This software is a "commercial item" as that term is defined at 48 C.F.R. 2.101 (OCT 1995),                                       *|
|*  consisting  of "commercial computer  software"  and "commercial computer software documentation"                                  *|
|*  as such terms are  used in 48 C.F.R. 12.212 (SEPT 1995) and is provided to the U.S. Government only as a commercial end item.     *|
|*  Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995),                                          *|
|*  all U.S. Government End Users acquire the software with only those rights set forth herein.                                       *|
|*                                                                                                                                    *|
|*  Any use of this software in individual and commercial software must include,                                                      *|
|*  in the user documentation and internal comments to the code,                                                                      *|
|*  the above Disclaimer (as applicable) and U.S. Government End Users Notice.                                                        *|
|*                                                                                                                                    *|
 \************************************************************************************************************************************/
#pragma once
#include"nvapi_lite_salstart.h"
#include"nvapi_lite_common.h"
#pragma pack(push,8)
#ifdef __cplusplus
extern "C" {
#endif
#if defined(__cplusplus) && (defined(__d3d10_h__) || defined(__d3d10_1_h__) || defined(__d3d11_h__))
//! \ingroup dx
//! D3D_FEATURE_LEVEL supported - used in NvAPI_D3D11_CreateDevice() and NvAPI_D3D11_CreateDeviceAndSwapChain()
typedef enum
{
    NVAPI_DEVICE_FEATURE_LEVEL_NULL       = -1,
    NVAPI_DEVICE_FEATURE_LEVEL_10_0       = 0,
    NVAPI_DEVICE_FEATURE_LEVEL_10_0_PLUS  = 1,
    NVAPI_DEVICE_FEATURE_LEVEL_10_1       = 2,
    NVAPI_DEVICE_FEATURE_LEVEL_11_0       = 3,
} NVAPI_DEVICE_FEATURE_LEVEL;

#endif  //defined(__cplusplus) && (defined(__d3d10_h__) || defined(__d3d10_1_h__) || defined(__d3d11_h__))
#if defined(__cplusplus) && defined(__d3d11_h__)

///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME: NvAPI_D3D11_CreateDevice
//
//!   DESCRIPTION: This function tries to create a DirectX 11 device. If the call fails (if we are running
//!                on pre-DirectX 11 hardware), depending on the type of hardware it will try to create a DirectX 10.1 OR DirectX 10.0+
//!                OR DirectX 10.0 device. The function call is the same as D3D11CreateDevice(), but with an extra 
//!                argument (D3D_FEATURE_LEVEL supported by the device) that the function fills in. This argument
//!                can contain -1 (NVAPI_DEVICE_FEATURE_LEVEL_NULL), if the requested featureLevel is less than DirecX 10.0.
//!
//!            NOTE: When NvAPI_D3D11_CreateDevice is called with 10+ feature level we have an issue on few set of
//!                  tesla hardware (G80/G84/G86/G92/G94/G96) which does not support all feature level 10+ functionality
//!                  e.g. calling driver with mismatch between RenderTarget and Depth Buffer. App developers should
//!                  take into consideration such limitation when using NVAPI on such tesla hardwares.
//! SUPPORTED OS:  Windows 7 and higher
//!
//!
//! \since Release: 185
//!
//! \param [in]   pAdapter
//! \param [in]   DriverType
//! \param [in]   Software
//! \param [in]   Flags
//! \param [in]   *pFeatureLevels
//! \param [in]   FeatureLevels
//! \param [in]   SDKVersion
//! \param [in]   **ppDevice
//! \param [in]   *pFeatureLevel
//! \param [in]   **ppImmediateContext
//! \param [in]   *pSupportedLevel  D3D_FEATURE_LEVEL supported
//!
//! \return NVAPI_OK if the createDevice call succeeded.
//!
//! \ingroup dx
///////////////////////////////////////////////////////////////////////////////
NVAPI_INTERFACE NvAPI_D3D11_CreateDevice(IDXGIAdapter* pAdapter,
                                         D3D_DRIVER_TYPE DriverType,
                                         HMODULE Software,
                                         UINT Flags,
                                         CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                                         UINT FeatureLevels,
                                         UINT SDKVersion,
                                         ID3D11Device **ppDevice,
                                         D3D_FEATURE_LEVEL *pFeatureLevel,
                                         ID3D11DeviceContext **ppImmediateContext,
                                         NVAPI_DEVICE_FEATURE_LEVEL *pSupportedLevel);


#endif //defined(__cplusplus) && defined(__d3d11_h__)
#if defined(__cplusplus) && defined(__d3d11_h__)

///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME: NvAPI_D3D11_CreateDeviceAndSwapChain
//
//!   DESCRIPTION: This function tries to create a DirectX 11 device and swap chain. If the call fails (if we are 
//!                running on pre=DirectX 11 hardware), depending on the type of hardware it will try to create a DirectX 10.1 OR 
//!                DirectX 10.0+ OR DirectX 10.0 device. The function call is the same as D3D11CreateDeviceAndSwapChain,  
//!                but with an extra argument (D3D_FEATURE_LEVEL supported by the device) that the function fills
//!                in. This argument can contain -1 (NVAPI_DEVICE_FEATURE_LEVEL_NULL), if the requested featureLevel
//!                is less than DirectX 10.0.
//!
//! SUPPORTED OS:  Windows 7 and higher
//!
//!
//! \since Release: 185
//!
//! \param [in]     pAdapter
//! \param [in]     DriverType
//! \param [in]     Software
//! \param [in]     Flags
//! \param [in]     *pFeatureLevels
//! \param [in]     FeatureLevels
//! \param [in]     SDKVersion
//! \param [in]     *pSwapChainDesc
//! \param [in]     **ppSwapChain
//! \param [in]     **ppDevice
//! \param [in]     *pFeatureLevel
//! \param [in]     **ppImmediateContext
//! \param [in]     *pSupportedLevel  D3D_FEATURE_LEVEL supported
//!
//!return  NVAPI_OK if the createDevice with swap chain call succeeded.
//!
//! \ingroup dx
///////////////////////////////////////////////////////////////////////////////
NVAPI_INTERFACE NvAPI_D3D11_CreateDeviceAndSwapChain(IDXGIAdapter* pAdapter,
                                         D3D_DRIVER_TYPE DriverType,
                                         HMODULE Software,
                                         UINT Flags,
                                         CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                                         UINT FeatureLevels,
                                         UINT SDKVersion,
                                         CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
                                         IDXGISwapChain **ppSwapChain,
                                         ID3D11Device **ppDevice,
                                         D3D_FEATURE_LEVEL *pFeatureLevel,
                                         ID3D11DeviceContext **ppImmediateContext,
                                         NVAPI_DEVICE_FEATURE_LEVEL *pSupportedLevel);



#endif //defined(__cplusplus) && defined(__d3d11_h__)
#if defined(__cplusplus) && defined(__d3d11_h__)

///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME: NvAPI_D3D11_SetDepthBoundsTest
//
//!   DESCRIPTION: This function enables/disables the depth bounds test
//!
//! SUPPORTED OS:  Windows 7 and higher
//!
//!
//! \param [in]        pDeviceOrContext   The device or device context to set depth bounds test
//! \param [in]        bEnable            Enable(non-zero)/disable(zero) the depth bounds test
//! \param [in]        fMinDepth          The minimum depth for depth bounds test
//! \param [in]        fMaxDepth          The maximum depth for depth bounds test
//!                                       The valid values for fMinDepth and fMaxDepth
//!                                       are such that 0 <= fMinDepth <= fMaxDepth <= 1
//!
//! \return  ::NVAPI_OK if the depth bounds test was correcly enabled or disabled
//!
//! \ingroup dx
///////////////////////////////////////////////////////////////////////////////
NVAPI_INTERFACE NvAPI_D3D11_SetDepthBoundsTest(IUnknown* pDeviceOrContext,
                                               NvU32 bEnable,
                                               float fMinDepth,
                                               float fMaxDepth);

#endif //defined(__cplusplus) && defined(__d3d11_h__)

#include"nvapi_lite_salend.h"
#ifdef __cplusplus
}
#endif
#pragma pack(pop)

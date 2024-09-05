 /************************************************************************************************************************************\
|*                                                                                                                                    *|
|*     Copyright ? 2012 NVIDIA Corporation.  All rights reserved.                                                                     *|
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
//! SUPPORTED OS:  Windows 7 and higher
//!
///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME:   NvAPI_DISP_GetGDIPrimaryDisplayId
//
//! DESCRIPTION:     This API returns the Display ID of the GDI Primary.
//!
//! \param [out]     displayId   Display ID of the GDI Primary display.
//!
//! \retval ::NVAPI_OK:                          Capabilties have been returned.
//! \retval ::NVAPI_NVIDIA_DEVICE_NOT_FOUND:     GDI Primary not on an NVIDIA GPU.
//! \retval ::NVAPI_INVALID_ARGUMENT:            One or more args passed in are invalid.
//! \retval ::NVAPI_API_NOT_INTIALIZED:          The NvAPI API needs to be initialized first
//! \retval ::NVAPI_NO_IMPLEMENTATION:           This entrypoint not available
//! \retval ::NVAPI_ERROR:                       Miscellaneous error occurred
//!
//! \ingroup dispcontrol
///////////////////////////////////////////////////////////////////////////////
NVAPI_INTERFACE NvAPI_DISP_GetGDIPrimaryDisplayId(NvU32* displayId);
#define NV_MOSAIC_MAX_DISPLAYS      (64)
//! SUPPORTED OS:  Windows 7 and higher
//!
///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME:   NvAPI_Mosaic_GetDisplayViewportsByResolution
//
//! DESCRIPTION:     This API returns the viewports that would be applied on
//!                  the requested display.
//!
//! \param [in]      displayId       Display ID of a single display in the active
//!                                  mosaic topology to query.
//! \param [in]      srcWidth        Width of full display topology. If both
//!                                  width and height are 0, the current
//!                                  resolution is used.
//! \param [in]      srcHeight       Height of full display topology. If both
//!                                  width and height are 0, the current
//!                                  resolution is used.
//! \param [out]     viewports       Array of NV_RECT viewports which represent
//!                                  the displays as identified in
//!                                  NvAPI_Mosaic_EnumGridTopologies. If the
//!                                  requested resolution is a single-wide
//!                                  resolution, only viewports[0] will
//!                                  contain the viewport details, regardless
//!                                  of which display is driving the display.
//! \param [out]     bezelCorrected  Returns 1 if the requested resolution is
//!                                  bezel corrected. May be NULL.
//!
//! \retval ::NVAPI_OK                          Capabilties have been returned.
//! \retval ::NVAPI_INVALID_ARGUMENT            One or more args passed in are invalid.
//! \retval ::NVAPI_API_NOT_INTIALIZED          The NvAPI API needs to be initialized first
//! \retval ::NVAPI_MOSAIC_NOT_ACTIVE           The display does not belong to an active Mosaic Topology
//! \retval ::NVAPI_NO_IMPLEMENTATION           This entrypoint not available
//! \retval ::NVAPI_ERROR                       Miscellaneous error occurred
//!
//! \ingroup mosaicapi
///////////////////////////////////////////////////////////////////////////////
NVAPI_INTERFACE NvAPI_Mosaic_GetDisplayViewportsByResolution(NvU32 displayId, NvU32 srcWidth, NvU32 srcHeight, NV_RECT viewports[NV_MOSAIC_MAX_DISPLAYS], NvU8* bezelCorrected);

#include"nvapi_lite_salend.h"
#ifdef __cplusplus
}
#endif
#pragma pack(pop)

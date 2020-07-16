/************************************************************************************************************************************\
|*                                                                                                                                    *|
|*     Copyright © 2012 NVIDIA Corporation.  All rights reserved.                                                                     *|
|*                                                                                                                                    *|
|*  NOTICE TO USER:                                                                                                                   *|
|*                                                                                                                                    *|
|*  This software is subject to NVIDIA ownership rights under U.S. and international Copyright laws.                                  *|
|*                                                                                                                                    *|
|*  This software and the information contained herein are PROPRIETARY and CONFIDENTIAL to NVIDIA                                     *|
|*  and are being provided solely under the terms and conditions of an NVIDIA software license agreement                              *|
|*  and / or non-disclosure agreement.  Otherwise, you have no rights to use or access this software in any manner.                   *|
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
//////////////////////////////////////////////////////////////////////////////////////////
// @brief:    This sample code shows how to use the NvAPI NvAPI_Disp_ColorControl to control the color values.
// @driver support: R304+
//////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "nvapi.h"
#include <stdarg.h>

NvAPI_ShortString errorDescStr;

char* GetNvAPIStatusString(NvAPI_Status nvapiErrorStatus)
{
    NvAPI_GetErrorMessage(nvapiErrorStatus, errorDescStr);
    return errorDescStr;
}

NvAPI_Status Initialize_NVAPI()
{
    NvAPI_Status nvapiReturnStatus = NVAPI_ERROR;

    nvapiReturnStatus = NvAPI_Initialize();
    if (nvapiReturnStatus != NVAPI_OK)
    {
        printf("\nNvAPI_Initialize() failed.\nReturn Error : %s", GetNvAPIStatusString( nvapiReturnStatus));
    }
    else
    {
        printf("\nNVAPI Initialized successfully");
    }

    return nvapiReturnStatus;
}

NvAPI_Status GetGPUs(NvPhysicalGpuHandle gpuHandleArray[NVAPI_MAX_PHYSICAL_GPUS], NvU32 &gpuCount)
{
    NvAPI_Status nvapiReturnStatus = NVAPI_ERROR;

    // Get all gpu handles.
    nvapiReturnStatus = NvAPI_EnumPhysicalGPUs(gpuHandleArray, &gpuCount);
    if (nvapiReturnStatus != NVAPI_OK)
    {
        printf("\nNvAPI_EnumPhysicalGPUs() failed.\nReturn Error : %s", GetNvAPIStatusString(nvapiReturnStatus));
        return nvapiReturnStatus;
    }

    return nvapiReturnStatus;
}

NvAPI_Status GetConnectedDisplays(NvPhysicalGpuHandle gpuHandle, NV_GPU_DISPLAYIDS *pDisplayID, NvU32 &displayIdCount)
{
    // First call to get the no. of displays connected by passing NULL
    NvAPI_Status nvapiReturnStatus = NVAPI_ERROR;
    NvU32 displayCount = 0;
    nvapiReturnStatus = NvAPI_GPU_GetConnectedDisplayIds(gpuHandle, NULL, &displayCount, 0);
    if (nvapiReturnStatus != NVAPI_OK)
    {
        printf("\nNvAPI_GPU_GetConnectedDisplayIds() failed.\nReturn Error : %s", GetNvAPIStatusString(nvapiReturnStatus));
        return nvapiReturnStatus;
    }

    if (displayCount == 0)
        return nvapiReturnStatus;

    // alocation for the display ids
    NV_GPU_DISPLAYIDS *dispIds = new NV_GPU_DISPLAYIDS[displayCount];
    if (!dispIds)
    {
        return NVAPI_OUT_OF_MEMORY;
    }

    dispIds[0].version = NV_GPU_DISPLAYIDS_VER;

    nvapiReturnStatus = NvAPI_GPU_GetConnectedDisplayIds(gpuHandle, dispIds, &displayCount, 0);
    if (nvapiReturnStatus == NVAPI_OK)
    {
        memcpy_s(pDisplayID, sizeof(NV_GPU_DISPLAYIDS) * NVAPI_MAX_DISPLAYS, dispIds, sizeof(NV_GPU_DISPLAYIDS) * displayCount);
        displayIdCount = displayCount;
    }

    delete[] dispIds;
    return nvapiReturnStatus;
}

void ColorControl(NV_COLOR_CMD command)
{
    NvAPI_Status nvapiReturnStatus = NVAPI_OK;

    switch (command)
    {
        case NV_COLOR_CMD_GET:
        case NV_COLOR_CMD_SET:
            break;
        default:
            nvapiReturnStatus = NVAPI_INVALID_ARGUMENT;
            break;
    }

    if (nvapiReturnStatus != NVAPI_OK)
    {
        printf("\nColorControl failed with error code : %s", GetNvAPIStatusString(nvapiReturnStatus));
        return;
    }

    NV_GPU_DISPLAYIDS pDisplayID[NVAPI_MAX_DISPLAYS];
    NvU32 displayIdCount = 0;

    NvPhysicalGpuHandle gpuHandleArray[NVAPI_MAX_PHYSICAL_GPUS] = { 0 };
    NvU32 gpuCount = 0;

    nvapiReturnStatus = GetGPUs(gpuHandleArray, gpuCount);
    if (nvapiReturnStatus != NVAPI_OK)
    {
        printf("\nGetGPUs failed with error code : %s ", GetNvAPIStatusString(nvapiReturnStatus));
        return;
    }

    // Get all active outputs info for all gpu's
    for (NvU32 i = 0; i < gpuCount; ++i)
    {
        printf("\n\nGPU %d (Gpu handle : 0x%x )", i + 1, gpuHandleArray[i]);
        nvapiReturnStatus = GetConnectedDisplays(gpuHandleArray[i], pDisplayID, displayIdCount);
        if (nvapiReturnStatus != NVAPI_OK)
        {
            printf("\nGetConnectedDisplays failed for this GPU with error code : %s ", GetNvAPIStatusString(nvapiReturnStatus));
            continue;
        }

        if (!displayIdCount)
        {
            printf("\n\tNo displays connected on this GPU");
            continue;
        }

        NV_COLOR_DATA colorData = { 0 };
        colorData.version = NV_COLOR_DATA_VER;
        colorData.size = sizeof(NV_COLOR_DATA);

        switch(command)
        {
            case NV_COLOR_CMD_GET:
                 colorData.cmd = NV_COLOR_CMD_GET;
                 break;
            case NV_COLOR_CMD_SET:
                 colorData.cmd = NV_COLOR_CMD_SET;
                 colorData.data.bpc = NV_BPC_10;
                 colorData.data.colorFormat = NV_COLOR_FORMAT_DEFAULT;
                 colorData.data.colorimetry = NV_COLOR_COLORIMETRY_DEFAULT;
                 colorData.data.colorSelectionPolicy = NV_COLOR_SELECTION_POLICY_USER;
                 colorData.data.dynamicRange = NV_DYNAMIC_RANGE_AUTO;
                 break;
        }

        for (NvU32 j = 0; j < displayIdCount; j++)
        {
            printf("\n\tDisplay %d (DisplayId 0x%x):", j + 1, pDisplayID[j].displayId);
            nvapiReturnStatus = NvAPI_Disp_ColorControl(pDisplayID[j].displayId, &colorData);
            if (nvapiReturnStatus != NVAPI_OK)
            {
                printf("\n\t\tNvAPI_Disp_ColorControl failed for this display with error code : %s ", GetNvAPIStatusString(nvapiReturnStatus));
                continue;
            }
            else
            {
                printf("\n\t\tNvAPI_Disp_ColorControl returned successfully for this display");
            }
        }
    }
}

void GetColorControl()
{
    printf("\n\nGetColorControl started\n");
    NvAPI_Status nvapiReturnStatus = NVAPI_ERROR;
    ColorControl(NV_COLOR_CMD_GET);
}

void SetColorControl()
{
    printf("\n\n\nSetColorControl started\n");
    NvAPI_Status nvapiReturnStatus = NVAPI_ERROR;
    ColorControl(NV_COLOR_CMD_SET);
}
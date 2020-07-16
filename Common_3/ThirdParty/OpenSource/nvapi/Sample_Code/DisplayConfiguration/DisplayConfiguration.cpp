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
// @brief:	This sample code checks the current configuration of the displays in the system.
//			Assuming we have two displays on the system, we shall set the different display
//			configurations on the displays.
// 
// @assumptions:The system is assumed to have a WinVista+ OS, with atleast 2 Displays.
//				 An NVIDIA adapter is assumed to be present on the system with the appropriate
//				 driver installed.
//
// @driver support: R304+
//////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include "targetver.h"
#include "nvapi.h"

NvAPI_Status AllocateAndGetDisplayConfig(NvU32* pathInfoCount, NV_DISPLAYCONFIG_PATH_INFO** pPathInfo);
void ShowCurrentDisplayConfig(void);
NvAPI_Status SetMode(void);

int _tmain(int argc, _TCHAR* argv[])
{
	NvAPI_Status ret = NVAPI_OK;
	ret = NvAPI_Initialize();
    if(ret != NVAPI_OK)
    {
        printf("NvAPI_Initialize() failed = 0x%x", ret);
		return 1; // Initialization failed
    }
    ret = SetMode();//to get the current mode, set to single view and then apply clone mode    
	if(ret != NVAPI_OK)
    {
        printf("Failed to Set Mode... return code = 0x%x", ret);
        return 1;//Failed to Set Mode
    }
	printf("\nDisplay Configuration Successful!\n");
	printf("\nPress any key to exit...");
	getchar();
    return 0;
}

// This function is used to do the NvAPI_DISP_GetDisplayConfig to get the current state of the system
NvAPI_Status AllocateAndGetDisplayConfig(NvU32* pathInfoCount, NV_DISPLAYCONFIG_PATH_INFO** pPathInfo)
{
    NvAPI_Status ret;

    // Retrieve the display path information
    NvU32 pathCount							= 0;
    NV_DISPLAYCONFIG_PATH_INFO *pathInfo	= NULL;

    ret = NvAPI_DISP_GetDisplayConfig(&pathCount, NULL);
    if (ret != NVAPI_OK)    return ret;

    pathInfo = (NV_DISPLAYCONFIG_PATH_INFO*) malloc(pathCount * sizeof(NV_DISPLAYCONFIG_PATH_INFO));
    if (!pathInfo)
    {
        return NVAPI_OUT_OF_MEMORY;
    }

    memset(pathInfo, 0, pathCount * sizeof(NV_DISPLAYCONFIG_PATH_INFO));
    for (NvU32 i = 0; i < pathCount; i++)
    {
        pathInfo[i].version = NV_DISPLAYCONFIG_PATH_INFO_VER;
    }

    // Retrieve the targetInfo counts
    ret = NvAPI_DISP_GetDisplayConfig(&pathCount, pathInfo);
    if (ret != NVAPI_OK)
    {
        return ret;
    }

    for (NvU32 i = 0; i < pathCount; i++)
    {
        // Allocate the source mode info
 
        if(pathInfo[i].version == NV_DISPLAYCONFIG_PATH_INFO_VER1 || pathInfo[i].version == NV_DISPLAYCONFIG_PATH_INFO_VER2)
        {
            pathInfo[i].sourceModeInfo = (NV_DISPLAYCONFIG_SOURCE_MODE_INFO*) malloc(sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));
        }
        else
        {

#ifdef NV_DISPLAYCONFIG_PATH_INFO_VER3
pathInfo[i].sourceModeInfo = (NV_DISPLAYCONFIG_SOURCE_MODE_INFO*) malloc(pathInfo[i].sourceModeInfoCount * sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));
#endif

        }
        if (pathInfo[i].sourceModeInfo == NULL)
        {
            return NVAPI_OUT_OF_MEMORY;
        }
        memset(pathInfo[i].sourceModeInfo, 0, sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));

        // Allocate the target array
        pathInfo[i].targetInfo = (NV_DISPLAYCONFIG_PATH_TARGET_INFO*) malloc(pathInfo[i].targetInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
        if (pathInfo[i].targetInfo == NULL)
        {
            return NVAPI_OUT_OF_MEMORY;
        }
        // Allocate the target details
        memset(pathInfo[i].targetInfo, 0, pathInfo[i].targetInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
        for (NvU32 j = 0 ; j < pathInfo[i].targetInfoCount ; j++)
        {
            pathInfo[i].targetInfo[j].details = (NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO*) malloc(sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO));    
            memset(pathInfo[i].targetInfo[j].details, 0, sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO));
            pathInfo[i].targetInfo[j].details->version = NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO_VER;
        }
    }

    // Retrieve the full path info
    ret = NvAPI_DISP_GetDisplayConfig(&pathCount, pathInfo);
    if (ret != NVAPI_OK)    
    {
        return ret;
    }

    *pathInfoCount = pathCount;
    *pPathInfo = pathInfo;
    return NVAPI_OK;
}

// This function is used to display the current GPU configuration and the connected displays
void ShowCurrentDisplayConfig(void)
{
	NvAPI_Status ret						= NVAPI_OK;
	NV_DISPLAYCONFIG_PATH_INFO *pathInfo	= NULL;
	NvU32 pathCount							= 0;
	NV_DISPLAYCONFIG_PATH_INFO *pathInfo1	= NULL;
	NvU32 nDisplayIds						= 0;
    NvU32 physicalGpuCount					= 0;
    NV_GPU_DISPLAYIDS* pDisplayIds			= NULL;
    NvPhysicalGpuHandle hPhysicalGpu[NVAPI_MAX_PHYSICAL_GPUS];

	for (NvU32 PhysicalGpuIndex = 0; PhysicalGpuIndex < NVAPI_MAX_PHYSICAL_GPUS; PhysicalGpuIndex++)
    {
        hPhysicalGpu[PhysicalGpuIndex]=0;
    }

	printf("\nThe currently running display configuration is as follows:\n");
	for(NvU32 count = 0; count < 60; count++)	printf("#");
	printf("\nGPU index\tGPU ID\t\tDisplayIDs of displays\n");
	
    
        
    // Enumerate the physical GPU handle
    ret = NvAPI_EnumPhysicalGPUs(hPhysicalGpu, &physicalGpuCount);
	if(ret != NVAPI_OK)
	{
		printf("Cannot enumerate GPUs in the system...\n");
		getchar();
		exit(1);
	}
    // get the display ids of connected displays
	NvU32 DisplayGpuIndex					= 0;

	for(NvU32 GpuIndex = 0; GpuIndex < physicalGpuCount; GpuIndex++)
	{
		ret = NvAPI_GPU_GetConnectedDisplayIds(hPhysicalGpu[GpuIndex], pDisplayIds, &nDisplayIds, 0);
		if((ret == NVAPI_OK) && nDisplayIds)
		{
			DisplayGpuIndex					= GpuIndex;
			pDisplayIds						= (NV_GPU_DISPLAYIDS*)malloc(nDisplayIds * sizeof(NV_GPU_DISPLAYIDS));
			if (pDisplayIds)
			{
				memset(pDisplayIds, 0, nDisplayIds * sizeof(NV_GPU_DISPLAYIDS));
				pDisplayIds[GpuIndex].version		= NV_GPU_DISPLAYIDS_VER;
				ret = NvAPI_GPU_GetConnectedDisplayIds(hPhysicalGpu[DisplayGpuIndex], pDisplayIds, &nDisplayIds, 0);
				for(NvU32 DisplayIdIndex = 0; DisplayIdIndex < nDisplayIds; DisplayIdIndex++)
				{
					printf("%2d\t\t0x%x\t0x%x", GpuIndex, hPhysicalGpu[DisplayGpuIndex], pDisplayIds[DisplayIdIndex].displayId);
					if(!pDisplayIds[DisplayIdIndex].displayId)printf("(NONE)");
					printf("\n");
				}
			}
		}
		else
			printf("%2d\t\t0x%x\n", GpuIndex, hPhysicalGpu[GpuIndex]);
	}
	for(NvU32 count = 0; count < 60; count++)printf("#");
	printf("\n");

	ret = AllocateAndGetDisplayConfig(&pathCount, &pathInfo);
    if (ret != NVAPI_OK)
	{
		printf("AllocateAndGetDisplayConfig failed!\n");
		getchar();
		exit(1);
	}
	if( pathCount == 1 )
    {
        if( pathInfo[0].targetInfoCount == 1 ) // if pathCount = 1 and targetInfoCount =1 it is Single Mode
            printf("Single MODE\n");
        else if( pathInfo[0].targetInfoCount > 1) // if pathCount >= 1 and targetInfoCount >1 it is Clone Mode
            printf("Monitors in Clone MODE\n");
    }
    else
    {
        for (NvU32 PathIndex = 0; PathIndex < pathCount; PathIndex++)
        {   
            if(pathInfo[PathIndex].targetInfoCount == 1)
            {
                printf("Monitor with Display Id 0x%x is in Extended MODE\n",pathInfo[PathIndex].targetInfo->displayId);  
                // if pathCount > 1 and targetInfoCount =1 it is Extended Mode
            }
            else if( pathInfo[PathIndex].targetInfoCount > 1)
            {
                for (NvU32 TargetIndex = 0; TargetIndex < pathInfo[PathIndex].targetInfoCount; TargetIndex++)
                {
					// if pathCount >= 1 and targetInfoCount > 1 it is Clone Mode
					printf("Monitors with Display Id 0x%x are in Clone MODE\n",pathInfo[PathIndex].targetInfo[TargetIndex].displayId);
                }
            }
        }
    }
}
// This function actually sets the possible display configurations on the multi-monitor setup
NvAPI_Status SetMode(void)
{
    NvU32 totalTargets						= 0;
    NvAPI_Status ret						= NVAPI_OK;
    NvU32 DisplayID							= 0;
    NvU32 pathCount							= 0;
    NV_DISPLAYCONFIG_PATH_INFO *pathInfo	= NULL;
    NvDisplayHandle *pNvDispHandle			= NULL;

	//Retrieve the display path information
	ShowCurrentDisplayConfig();

	ret = AllocateAndGetDisplayConfig(&pathCount, &pathInfo);
    if (ret != NVAPI_OK)
       return ret;

    NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO *details = NULL;
    details = (NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO*)malloc(sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO));

    if( pathCount == 1 )
    {
        totalTargets		+= pathInfo[0].targetInfoCount;
        if(pathInfo[0].targetInfoCount > 1)
            DisplayID		= pathInfo[0].targetInfo[1].displayId; // Store displayId to use later
    }
    else
    {
        for (NvU32 i = 0; i < pathCount; i++)
        {
            totalTargets	+= pathInfo[i].targetInfoCount; // Count all targets
            DisplayID	= pathInfo[i].targetInfo[0].displayId; // Store displayId to use later
            if(pathInfo[i].targetInfo[0].details)
            {
                details      =  pathInfo[i].targetInfo[0].details;
            }
        }
    }

	
	// Activate and Set 2 targets in Clone mode; pathCount >= 1 and targetInfoCount > 1	
    if (totalTargets > 1) 
    {
        printf("\nActivating clone mode display on system");
        if(pathInfo[0].version == NV_DISPLAYCONFIG_PATH_INFO_VER1 || pathInfo[0].version == NV_DISPLAYCONFIG_PATH_INFO_VER2)
        {

		    pathInfo[0].targetInfoCount						= 2;
            NV_DISPLAYCONFIG_PATH_TARGET_INFO* primary		= (NV_DISPLAYCONFIG_PATH_TARGET_INFO*) malloc(pathInfo[0].targetInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
		    printf(".");
            memset(primary, 0, pathInfo[0].targetInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
            primary->displayId								= pathInfo[0].targetInfo[0].displayId;
		    printf(".");
            delete pathInfo[0].targetInfo;
            pathInfo[0].targetInfo							= primary;
            pathInfo[0].targetInfo[1].displayId				= DisplayID;
		    pathInfo[0].sourceModeInfo[0].bGDIPrimary		= 1;	// Decide the primary display
        }
        else
        {
            if( pathCount > 1 )
            {
		        pathInfo[0].targetInfoCount						= 2;
                NV_DISPLAYCONFIG_PATH_TARGET_INFO* primary		= (NV_DISPLAYCONFIG_PATH_TARGET_INFO*) malloc(pathInfo[0].targetInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
		        printf(".");
                memset(primary, 0, pathInfo[0].targetInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
                primary->displayId								= pathInfo[0].targetInfo[0].displayId;
		        printf(".");
                primary->details = pathInfo[0].targetInfo[0].details;
                primary++;
                primary->displayId = DisplayID;
                primary->details = details;
                primary--;
                delete pathInfo[0].targetInfo;
                pathInfo[0].targetInfo							= (NV_DISPLAYCONFIG_PATH_TARGET_INFO*) malloc(2 * sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
                pathInfo[0].targetInfo = primary;
            }
            pathInfo[0].sourceModeInfo[0].bGDIPrimary       = 1;// Decide the primary display

#ifdef NV_DISPLAYCONFIG_PATH_INFO_VER3 
pathInfo[0].sourceModeInfoCount                 = 1;
#endif
        }

        ret = NvAPI_DISP_SetDisplayConfig(1, pathInfo, 0);
		printf(".\n");
        NvU32 tempPathCount								= 0;
        NV_DISPLAYCONFIG_PATH_INFO *tempPathInfo		= NULL;
        if (ret == NVAPI_OK)    
        {
            //Validation of set
            ret = AllocateAndGetDisplayConfig(&tempPathCount, &tempPathInfo);
            if (ret != NVAPI_OK)
            {
                ret = NVAPI_ERROR;
                return ret;
            }
            else
            {
                (tempPathCount == 1 && pathInfo[0].targetInfoCount == tempPathInfo[0].targetInfoCount) ? ret = NVAPI_OK : ret = NVAPI_ERROR;
            }
        }

       if (ret != NVAPI_OK)
            return ret;

        printf("Clone mode set!\n");
		printf("\nPress any key to continue...");
		getchar();
    }
	
    // Activate and Set 2 targets in Extended mode; pathCount > 1 and targetInfoCount = 1
	
    if (totalTargets > 1) 
    {
        printf("\nActivating extended mode display on system");
		NV_DISPLAYCONFIG_PATH_INFO *pathInfo1			= NULL;
        NvU32 nDisplayIds								= 0;
        NvU32 physicalGpuCount							= 0;
        NV_GPU_DISPLAYIDS* pDisplayIds					= NULL;
        
        NvPhysicalGpuHandle hPhysicalGpu[NVAPI_MAX_PHYSICAL_GPUS];
        for (NvU32 i = 0; i<NVAPI_MAX_PHYSICAL_GPUS; i++)
        {
            hPhysicalGpu[i]								= 0;
        }
        
        // Enumerate the physical GPU handle
        ret = NvAPI_EnumPhysicalGPUs(hPhysicalGpu, &physicalGpuCount);
		printf(".");	// GPUs enumerated

        // get the display ids of connected displays
		NvU32 DisplayGpuIndex							= 0;
		for(DisplayGpuIndex = 0; DisplayGpuIndex < physicalGpuCount; DisplayGpuIndex++)
		{
			ret = NvAPI_GPU_GetConnectedDisplayIds(hPhysicalGpu[DisplayGpuIndex], pDisplayIds, &nDisplayIds, 0);
			if(nDisplayIds)break;
			else continue;
		}
		printf(".");//Located the GPU on which active display is present
        if ((ret == NVAPI_OK) && (nDisplayIds))
        {
            pDisplayIds									= (NV_GPU_DISPLAYIDS*)malloc(nDisplayIds * sizeof(NV_GPU_DISPLAYIDS));
            if (pDisplayIds)
            {
                memset(pDisplayIds, 0, nDisplayIds * sizeof(NV_GPU_DISPLAYIDS));
                pDisplayIds[0].version					= NV_GPU_DISPLAYIDS_VER;
                ret = NvAPI_GPU_GetConnectedDisplayIds(hPhysicalGpu[DisplayGpuIndex], pDisplayIds, &nDisplayIds, 0);
            }
        }

        pathInfo1										= (NV_DISPLAYCONFIG_PATH_INFO*) malloc(nDisplayIds * sizeof(NV_DISPLAYCONFIG_PATH_INFO));
        if (!pathInfo1)
        {
            return NVAPI_OUT_OF_MEMORY;
        }

        memset(pathInfo1, 0, nDisplayIds * sizeof(NV_DISPLAYCONFIG_PATH_INFO));
        
        for (NvU32 i = 0; i < nDisplayIds; i++)
        {
            pathInfo1[i].version						= NV_DISPLAYCONFIG_PATH_INFO_VER;
            pathInfo1[i].targetInfoCount				= 1; // extended mode
            pathInfo1[i].targetInfo						= (NV_DISPLAYCONFIG_PATH_TARGET_INFO*) malloc(sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
            memset(pathInfo1[i].targetInfo, 0,sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
            pathInfo1[i].targetInfo->displayId			= pDisplayIds[i].displayId;
        }

        ret = NvAPI_DISP_SetDisplayConfig(nDisplayIds, pathInfo1, 0);
		printf(".\n");//Display config set to extended mode
        NvU32 tempPathCount								= 0;
        NV_DISPLAYCONFIG_PATH_INFO *tempPathInfo		= NULL;
        if (ret == NVAPI_OK)    
        {
            //Validation of set
            ret = AllocateAndGetDisplayConfig(&tempPathCount, &tempPathInfo);
            if (ret != NVAPI_OK)
            {
                ret = NVAPI_ERROR;
            }
            else
            {
                (tempPathCount == nDisplayIds) ? ret = NVAPI_OK : ret = NVAPI_ERROR;
            }
        }

        if (ret != NVAPI_OK)
            return ret;

        printf("Extended mode set!\n");

    }
    return ret;
}


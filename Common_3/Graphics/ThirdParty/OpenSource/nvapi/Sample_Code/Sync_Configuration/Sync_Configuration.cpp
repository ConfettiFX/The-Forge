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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <windows.h>
#include"nvapi.h"


static const char* g_connector[] = {"NONE", "PRIMARY", "SECONDARY", "TERTIARY","QUARTERNARY"};
static const char* g_dispSyncState[] = {"UNSYNCED", "SLAVE", "MASTER"};
static const char* g_RJ45Status[] = {"OUTPUT", "INPUT", "UNUSED"};
static const char* g_RJ45Ethernet[] = {"NO", "YES"};

#define NVAPI_MAX_GPUS_PER_GSYNC 4

typedef struct __GSyncSySInfo
{
    NvU32 gsyncCount;       // gsync board (default 0)
    struct{
        NvGSyncDeviceHandle hNvGSyncDevice;
        NvU32 gpuCount;
        NV_GSYNC_GPU gpus[NVAPI_MAX_GPUS_PER_GSYNC];         // gpu attached gsync board (default 0)
        NvU32 refreshRate;      // refresh rate (default 60)
        NV_GSYNC_CONTROL_PARAMS Ctrlparams;
    } gsyncTopo[NVAPI_MAX_GSYNC_DEVICES];
    NvU32 dispCount;
    NV_GSYNC_DISPLAY displays[NVAPI_MAX_GPUS_PER_GSYNC*NV_MAX_HEADS];
} GSyncSySInfo;
GSyncSySInfo gsyncSysInfo;

typedef struct __GSyncUserSettings
{
    NvGSyncDeviceHandle hNvGSyncDevice;;       // gsync board (default 0)
    NvU32 gpuBoard;         // gpu attached gsync board (default 0)
    NvU32 dispCount;
    NV_GSYNC_DISPLAY displays[NVAPI_MAX_GPUS_PER_GSYNC*NV_MAX_HEADS];
    NV_GSYNC_CONTROL_PARAMS Ctrlparams;
} GSyncUserSettings;

NvAPI_Status gSyncQueryParameters(NvGSyncDeviceHandle hNvGSyncDevice)
{
    // Status params
    NvAPI_Status ret;
    NV_GSYNC_CONTROL_PARAMS Ctrlparams = {0};
    NV_GSYNC_STATUS_PARAMS StatusParams = {0};

    Ctrlparams.version = NV_GSYNC_CONTROL_PARAMS_VER;
    ret = NvAPI_GSync_GetControlParameters( hNvGSyncDevice, &Ctrlparams );

    if( NVAPI_OK == ret )
    {
        printf("Polarity                 : %d\n", (NvU32)Ctrlparams.polarity);
        printf("Video Mode               : %d\n", (NvU32)Ctrlparams.vmode);
        printf("Sync Interval            : %d\n", Ctrlparams.interval);
        printf("Source                   : %d\n", (NvU32)Ctrlparams.source);
    }
    else
    {
        printf("Error querying control parameters:%d\n\n", ret);
        return ret;
    }

   // Status params
    StatusParams.version = NV_GSYNC_STATUS_PARAMS_VER;
    ret = NvAPI_GSync_GetStatusParameters( hNvGSyncDevice, &StatusParams );

    if( NVAPI_OK == ret )
    {
        printf("Refresh Rate             : %d\n", StatusParams.refreshRate);
        printf("Incoming house sync freq : %d\n", StatusParams.houseSyncIncoming);
        printf("House sync present       : %d\n", StatusParams.bHouseSync);
        printf("RJ45[0]                  : %s\n", g_RJ45Status[(NvU32)StatusParams.RJ45_IO[0]]);
        printf("RJ45[1]                  : %s\n", g_RJ45Status[(NvU32)StatusParams.RJ45_IO[1]]);
        printf("RJ45[0] to ethernet?     : %s\n", g_RJ45Ethernet[StatusParams.RJ45_Ethernet[0]]);
        printf("RJ45[1] to ethernet?     : %s\n", g_RJ45Ethernet[StatusParams.RJ45_Ethernet[1]]);
    }
    else
    {
        printf("Error querying status parameters:%d\n\n", ret);
    }

    printf("---------------------------\n");
    return ret;
}

NvAPI_Status gSyncQuerySyncStatus(NvGSyncDeviceHandle nvGSyncHandle)
{
    NV_GSYNC_STATUS status = {0};
    NvAPI_Status ret = NVAPI_OK;
    NvU32 gsyncGpuCount = 0;

    ret = NvAPI_GSync_GetTopology(nvGSyncHandle, &gsyncGpuCount, NULL, NULL, NULL);
    if ((ret != NVAPI_OK) && (gsyncGpuCount < 1))
    {
        printf("GetTopology Call failed\n");
        return ret;
    }

    NV_GSYNC_GPU *pGsyncGpu = new NV_GSYNC_GPU[gsyncGpuCount];
    pGsyncGpu[0].version = NV_GSYNC_GPU_VER;

    ret = NvAPI_GSync_GetTopology(nvGSyncHandle, &gsyncGpuCount, pGsyncGpu, NULL, NULL);
    if (ret != NVAPI_OK)
    {
        printf("GetTopology Call failed\n");
        goto cleanup;
        return ret;
    }

    for(NvU32 i=0; i<gsyncGpuCount; i++)
    {
        printf("\t\tGPU(%d) with GPU Handle       : 0x%x\n", i, pGsyncGpu[i].hPhysicalGpu);
        status.version = NV_GSYNC_STATUS_VER;
        ret = NvAPI_GSync_GetSyncStatus(nvGSyncHandle, pGsyncGpu[i].hPhysicalGpu, &status);
        printf("GSync Target %d:\n", i);
        if (NVAPI_OK == ret)
        {
            printf("Is Synced                 : %d\n", status.bIsSynced);
            printf("Is Stereo Synced          : %d\n", status.bIsStereoSynced);
            printf("Is sync signal available? : %d\n", status.bIsSyncSignalAvailable);
        }
        else
        {
            printf("Error querying sync status:%d\n\n", ret);
            goto cleanup;
            return ret;
        }

    }
    printf("---------------------------\n");

cleanup:
    delete [] pGsyncGpu;
    pGsyncGpu = NULL;

    return ret;
}

NvAPI_Status gsyncGetTopology(NvGSyncDeviceHandle *nvGSyncHandle, NvU32 count)
{
    NvAPI_Status ret = NVAPI_OK;
    NvU32 m = 0;
    for(NvU32 k=0;k<count;k++)
    {
        NvU32 gsyncGpuCount = 0;
        NvU32 gsyncDisplayCount = 0;

        ret = NvAPI_GSync_GetTopology(nvGSyncHandle[k], &gsyncGpuCount, NULL, &gsyncDisplayCount, NULL);
        if ((ret != NVAPI_OK))
        {
            printf("GetTopology Call failed\n");
            return ret;
        }

        NV_GSYNC_GPU *pGsyncGpu = new NV_GSYNC_GPU[gsyncGpuCount];
        pGsyncGpu[0].version = NV_GSYNC_GPU_VER;
        
        NV_GSYNC_DISPLAY *pGsyncDisp = new NV_GSYNC_DISPLAY[gsyncDisplayCount];
        pGsyncDisp[0].version = NV_GSYNC_DISPLAY_VER;
        
        gsyncSysInfo.gsyncTopo[k].hNvGSyncDevice = nvGSyncHandle[k];
        ret = NvAPI_GSync_GetTopology(nvGSyncHandle[k], &gsyncGpuCount, pGsyncGpu, &gsyncDisplayCount, pGsyncDisp);
        if (ret != NVAPI_OK)
        {
            printf("GetTopology Call failed\n");

            if(pGsyncGpu)
            {
                delete [] pGsyncGpu;
                pGsyncGpu = NULL;
            }

            if(pGsyncDisp)
            {
                delete [] pGsyncDisp;
                pGsyncDisp = NULL;
            }
            return ret;
        }

        gsyncSysInfo.gsyncTopo[k].gpuCount = gsyncGpuCount;
        for(NvU32 i=0; i<gsyncGpuCount; i++)
        {
            gsyncSysInfo.gsyncTopo[k].gpus[i].hPhysicalGpu = pGsyncGpu[i].hPhysicalGpu;
            gsyncSysInfo.gsyncTopo[k].gpus[i].connector = pGsyncGpu[i].connector;
            gsyncSysInfo.gsyncTopo[k].gpus[i].hProxyPhysicalGpu = pGsyncGpu[i].hProxyPhysicalGpu;
            gsyncSysInfo.gsyncTopo[k].gpus[i].isSynced = pGsyncGpu[i].isSynced;
          
        }
        for(NvU32 j=0; j<gsyncDisplayCount; j++)
        {    
            gsyncSysInfo.displays[m].displayId = pGsyncDisp[j].displayId;
            gsyncSysInfo.displays[m].isMasterable = pGsyncDisp[j].isMasterable;
            gsyncSysInfo.displays[m].syncState = pGsyncDisp[j].syncState;
            m++;

        }
    
        if(pGsyncGpu)
        {
            delete [] pGsyncGpu;
            pGsyncGpu = NULL;
        }

        if(pGsyncDisp)
        {
            delete [] pGsyncDisp;
            pGsyncDisp = NULL;
        }

        NV_GSYNC_CONTROL_PARAMS Ctrlparams = {0};
        NV_GSYNC_STATUS_PARAMS StatusParams = {0};

        Ctrlparams.version = NV_GSYNC_CONTROL_PARAMS_VER;
        ret = NvAPI_GSync_GetControlParameters( nvGSyncHandle[k], &Ctrlparams );

        if( NVAPI_OK == ret )
        {
            gsyncSysInfo.gsyncTopo[k].Ctrlparams.polarity = Ctrlparams.polarity;
            gsyncSysInfo.gsyncTopo[k].Ctrlparams.vmode = Ctrlparams.vmode;
            gsyncSysInfo.gsyncTopo[k].Ctrlparams.interval = Ctrlparams.interval;
            gsyncSysInfo.gsyncTopo[k].Ctrlparams.source = Ctrlparams.source;
           
        }
        else
        {
            printf("Error querying control parameters:%d\n\n", ret);
            return ret;
        }
    }
    gsyncSysInfo.dispCount = m;

    return ret;
}

void printGsyncTopo()
{
    NvAPI_Status ret = NVAPI_OK;

    printf("Number of GSync device(s) detected: %d\n\n", gsyncSysInfo.gsyncCount);
    for(NvU32 k=0;k<gsyncSysInfo.gsyncCount;k++)
    {
        printf("GSync(%d) with GSync Handle: 0x%x\n", k, gsyncSysInfo.gsyncTopo[k].hNvGSyncDevice);
        printf("\tNumber of GPUs connected to GSync(%d)     : %d\n", k,gsyncSysInfo.gsyncTopo[k].gpuCount);
        for(NvU32 i=0; i<gsyncSysInfo.gsyncTopo[k].gpuCount; i++)
        {
            printf("\t\tGPU Handle       : 0x%x\n", gsyncSysInfo.gsyncTopo[k].gpus[i].hPhysicalGpu);
            printf("\t\tConnector        : %s\n", g_connector[gsyncSysInfo.gsyncTopo[k].gpus[i].connector]);
            printf("\t\tProxy GPU Handle : 0x%x\n", gsyncSysInfo.gsyncTopo[k].gpus[i].hProxyPhysicalGpu);
            printf("\t\tIs synced        : %d\n", gsyncSysInfo.gsyncTopo[k].gpus[i].isSynced);
            NvPhysicalGpuHandle hPhysicalGpu = NULL;
            for(NvU32 j=0; j<gsyncSysInfo.dispCount; j++)
            {    
                ret = NvAPI_SYS_GetPhysicalGpuFromDisplayId(gsyncSysInfo.displays[j].displayId, &hPhysicalGpu);
                if(hPhysicalGpu != gsyncSysInfo.gsyncTopo[k].gpus[i].hPhysicalGpu)
                    continue;
                printf("\t\t\tdisplay(%d) with displayId : 0x%x\n", j, gsyncSysInfo.displays[j].displayId);
                printf("\t\t\t\tmasterable : 0x%x\n", gsyncSysInfo.displays[j].isMasterable);
                printf("\t\t\t\tsyncstate : %s\n", g_dispSyncState[gsyncSysInfo.displays[j].syncState]);
            }
        }
    }
}

void printHelp()
{
    printf("  queryTopo   -> It returns current Sync topology including GPU and displays of system, for all available Sync devices. \n" );
    printf("  setSync     -> It sets the default Sync state for active displays in Sync topology. \n" );
    printf("  unSync      -> It un-sets the Sync state for displays in Sync topology. \n" );
    printf("  queryParams -> It returns control parameters and status parameters, for all available Sync devices. \n" );
    printf("  toggleSrc   -> Toggles between houseSync and vSync. \n");
}

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        printf("check help\n");
        return -1;
    }
    
    NvAPI_Status ret = NVAPI_OK;
    NvGSyncDeviceHandle nvGSyncHandles[NVAPI_MAX_GSYNC_DEVICES];
    NvU32 gsyncCount = 0;
    ret = NvAPI_GSync_EnumSyncDevices(nvGSyncHandles, &gsyncCount);
    if (NVAPI_OK != ret)
    {
        if (NVAPI_NVIDIA_DEVICE_NOT_FOUND == ret) 
        {
            gsyncCount = 0;
            printf("No GSync Devices found on this system (cannot continue testing)\n");
        }
        return -1;
    }

    gsyncSysInfo.gsyncCount = gsyncCount;

    ret = gsyncGetTopology(nvGSyncHandles, gsyncCount);
    if(ret != NVAPI_OK)
    {
        return -1;
    }

    GSyncUserSettings gsyncUserSettings = {0};  

    int index = 1;
    if(stricmp(argv[index], "help") == 0)
    {
        printHelp();
        return 0;
    }
    else if(stricmp(argv[index], "queryTopo") == 0)
    {
        printGsyncTopo();
        return 0;
    }
    // SetSync with one master and rest slaves
    else if(stricmp(argv[index], "setSync") == 0)
    {
        NvU32 flags = 0 ;
        GSyncUserSettings gsyncUserSettings;
        memset(&gsyncUserSettings,0,sizeof(GSyncUserSettings));
        gsyncUserSettings.displays[0].version = NV_GSYNC_DISPLAY_VER;

        NvU32 dispIndex = 0;
        bool isMasterableFound = false;
        gsyncUserSettings.dispCount = gsyncSysInfo.dispCount;
        for (dispIndex = 0 ; dispIndex < gsyncSysInfo.dispCount; dispIndex++)
        {
            gsyncUserSettings.displays[dispIndex].displayId    = gsyncSysInfo.displays[dispIndex].displayId;            
            gsyncUserSettings.displays[dispIndex].reserved     = gsyncSysInfo.displays[dispIndex].reserved;
            if (!isMasterableFound && gsyncSysInfo.displays[dispIndex].isMasterable)
            {
                gsyncUserSettings.displays[dispIndex].syncState = NVAPI_GSYNC_DISPLAY_SYNC_STATE_MASTER;
                isMasterableFound = true;
            }
            else
            {
                gsyncUserSettings.displays[dispIndex].syncState = NVAPI_GSYNC_DISPLAY_SYNC_STATE_SLAVE;
            }
        }
        ret = NvAPI_GSync_SetSyncStateSettings(gsyncUserSettings.dispCount, gsyncUserSettings.displays, flags);
        if(ret!=NVAPI_OK)
        {
            printf("setSync failed with %d error code\n",ret);
            return -1;
        }
        else
        {
            printf("settings applied--see topo as below\n");
            gsyncGetTopology(nvGSyncHandles, gsyncCount);
            printGsyncTopo();
            return 0;
        }
    }
    // Disable = UnSync all displays connected
    else if(stricmp(argv[index], "unSync") == 0)
    {
        NvU32 flags = 0 ;
        GSyncUserSettings gsyncUserSettings = {0};  
        gsyncUserSettings.displays[0].version = NV_GSYNC_DISPLAY_VER;

        gsyncUserSettings.dispCount = 0;
        ret = NvAPI_GSync_SetSyncStateSettings(gsyncUserSettings.dispCount, gsyncUserSettings.displays, flags);
        if(ret!=NVAPI_OK)
        {
            printf("unSync failed with %d error code\n",ret);
            return -1;
        }
        else
        {
            printf("settings applied--see topo as below\n");
            gsyncGetTopology(nvGSyncHandles, gsyncCount);
            printGsyncTopo();
            return 0;
        }
    }
    else if(stricmp(argv[index], "queryParams") == 0)
    {
        ret = gSyncQueryParameters(nvGSyncHandles[0]);
        if(ret !=NVAPI_OK)
        {
            return -1;
        }
    }
    else if(stricmp(argv[index], "toggleSrc") == 0)
    {
        gsyncSysInfo.gsyncTopo[0].Ctrlparams.source = (NVAPI_GSYNC_SYNC_SOURCE)!(NvU32)gsyncSysInfo.gsyncTopo[0].Ctrlparams.source;
        gsyncSysInfo.gsyncTopo[0].Ctrlparams.version = NV_GSYNC_CONTROL_PARAMS_VER;
         ret = NvAPI_GSync_SetControlParameters(nvGSyncHandles[0], &gsyncSysInfo.gsyncTopo[0].Ctrlparams);
         if(ret!=NVAPI_OK)
         {
            printf("SetControl failed with %d error code\n",ret);
            return -1;
         }
    }

    return 0;
}


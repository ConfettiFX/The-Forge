#pragma once
#include "../../../GraphicsConfig.h"

#if defined(_WINDOWS) && !defined(DURANGO)
#include "ags_lib/inc/amd_ags.h"
#define AMDAGS
#else
enum AGSReturnCode
{
	AGS_SUCCESS,                    ///< Successful function call
	AGS_FAILURE,                    ///< Failed to complete call for some unspecified reason
	AGS_INVALID_ARGS,               ///< Invalid arguments into the function
	AGS_OUT_OF_MEMORY,              ///< Out of memory when allocating space internally
	AGS_MISSING_D3D_DLL,            ///< Returned when a D3D dll fails to load
	AGS_LEGACY_DRIVER,              ///< Returned if a feature is not present in the installed driver
	AGS_NO_AMD_DRIVER_INSTALLED,    ///< Returned if the AMD GPU driver does not appear to be installed
	AGS_EXTENSION_NOT_SUPPORTED,    ///< Returned if the driver does not support the requested driver extension
	AGS_ADL_FAILURE,                ///< Failure in ADL (the AMD Display Library)
	AGS_DX_FAILURE                  ///< Failure from DirectX runtime
};
#endif

#if defined(AMDAGS)
static AGSReturnCode gAgsStatus = AGS_FAILURE;
static AGSContext* pAgsContext = NULL;
static AGSGPUInfo  gAgsGpuInfo = {};
#endif

static AGSReturnCode agsInit()
{
#if defined(AMDAGS)
	AGSConfiguration config = {};
	gAgsStatus = agsInitialize(AGS_CURRENT_VERSION, &config, &pAgsContext, &gAgsGpuInfo);
	return gAgsStatus;
#else
	return AGS_SUCCESS;
#endif
}

static void agsExit()
{
#if defined(AMDAGS)
	agsDeInitialize(pAgsContext);
#endif
}

static void agsPrintDriverInfo()
{
#if defined(AMDAGS)
	if (pAgsContext)
	{
		LOGF(eINFO, "AMD Display Driver Version %u", gAgsGpuInfo.driverVersion);
		LOGF(eINFO, "AMD Radeon Software Version %s", gAgsGpuInfo.radeonSoftwareVersion);
	}
#endif
}

#if defined(AMDAGS)
static AGSDeviceInfo::AsicFamily agsGetAsicFamily(uint32_t deviceId)
{
	if (pAgsContext)
	{
		for (int i = 0; i < gAgsGpuInfo.numDevices; i++)
		{
			if ((uint32_t)gAgsGpuInfo.devices[i].deviceId == deviceId)
			{
				return gAgsGpuInfo.devices[i].asicFamily;
			}
		}
	}

	return AGSDeviceInfo::AsicFamily_Unknown;
}
#endif
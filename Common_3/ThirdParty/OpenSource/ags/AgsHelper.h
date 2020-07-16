#pragma once

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
static AGSContext* pAgsContext = NULL;
static AGSGPUInfo  gAgsGpuInfo = {};
#endif

static AGSReturnCode agsInit()
{
#if defined(AMDAGS)
	AGSConfiguration config = {};
	return agsInit(&pAgsContext, &config, &gAgsGpuInfo);
#endif

	return AGS_SUCCESS;
}

static void agsExit()
{
#if defined(AMDAGS)
	agsDeInit(pAgsContext);
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

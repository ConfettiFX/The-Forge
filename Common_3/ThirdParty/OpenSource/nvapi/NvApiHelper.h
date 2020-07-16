#pragma once

#if defined(_WINDOWS) && !defined(DURANGO)
#include "nvapi.h"
#define NVAPI
#else
typedef enum NvAPI_Status
{
	NVAPI_OK = 0,          //!< Success. Request is completed.
	NVAPI_ERROR = -1,      //!< Generic error
} NvAPI_Status;
#endif

static NvAPI_Status nvapiInit()
{
#if defined(NVAPI)
	return NvAPI_Initialize();
#endif

	return NvAPI_Status::NVAPI_OK;
}

static void nvapiExit()
{
#if defined(NVAPI)
	NvAPI_Unload();
#endif
}

static void nvapiPrintDriverInfo()
{
#if defined(NVAPI)
	NvU32 driverVersion = 0;
	NvAPI_ShortString buildBranch = {};
	NvAPI_Status status = NvAPI_SYS_GetDriverAndBranchVersion(&driverVersion, buildBranch);
	if (NvAPI_Status::NVAPI_OK == status)
	{
		LOGF(eINFO, "NVIDIA Display Driver Version %u", driverVersion);
		LOGF(eINFO, "NVIDIA Build Branch %s", buildBranch);
	}
#endif
}

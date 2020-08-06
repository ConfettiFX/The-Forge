/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#pragma once
static int regGetValueHelper(HKEY keyHandle, LPSTR subKeyName, LPSTR valName, char* resultStr, DWORD restultStrSize)
{
	DWORD strSize = 0;

	int returnCode = ::RegGetValueA(
		keyHandle,
		subKeyName,
		valName,
		RRF_RT_REG_SZ,
		nullptr,
		nullptr,
		&strSize
	);

	strSize += 1;

	if (returnCode == ERROR_SUCCESS)
	{
		if (strSize >= 128)
		{
			ASSERT(0);
			return -1;
		}

		char stringValStr[128];

		returnCode = ::RegGetValueA(
			keyHandle,
			subKeyName,
			valName,
			RRF_RT_REG_SZ,
			nullptr,
			stringValStr,
			&strSize
		);

		strcpy_s(resultStr, restultStrSize, stringValStr);
	}

	return returnCode;
}

static int initGpuDriverVersion(const char* selectedAdapter, char* driverVersion, uint32_t driverVersionSize, char* driverDate, uint32_t driverDateSize)
{

	// Fetch registry data
	HKEY dxKeyHandle = nullptr;
	DWORD numEntries = 0;

	//4d36e968-e325-11ce-bfc1-08002be10318 -- Always the display driver's GUID
	LSTATUS returnCode = ::RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\", 0, KEY_READ, &dxKeyHandle);
	DWORD subKeyMaxLength = 0;

	returnCode = ::RegQueryInfoKeyA(
		dxKeyHandle,
		nullptr,
		nullptr,
		nullptr,
		&numEntries,
		&subKeyMaxLength,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr
	);

	if (returnCode != ERROR_SUCCESS)
	{
		return -1;
	}

	subKeyMaxLength += 1; // include the null character

	bool foundSubkey = false;
	LPSTR subKeyName = (LPSTR)tf_malloc(subKeyMaxLength);

	for (DWORD i = 0; i < numEntries; ++i)
	{
		DWORD subKeyLength = subKeyMaxLength;

		returnCode = ::RegEnumKeyExA(
			dxKeyHandle,
			i,
			subKeyName,
			&subKeyLength,
			nullptr,
			nullptr,
			nullptr,
			nullptr
		);

		if (returnCode == ERROR_SUCCESS)
		{

			char gpuName[128];
			returnCode = regGetValueHelper(dxKeyHandle, subKeyName, "DriverDesc", gpuName, 128);

			if (returnCode == ERROR_SUCCESS && (selectedAdapter == 0 || strcmp(gpuName, selectedAdapter) == 0))
			{
				LOGF(eINFO, "Adapter Name: %s", gpuName);

				returnCode = regGetValueHelper(dxKeyHandle, subKeyName, "DriverVersion", driverVersion, driverVersionSize);

				if (returnCode != ERROR_SUCCESS)
				{
					return -1;
				}

				LOGF(eINFO, "Driver Version: %s", driverVersion);

				returnCode = regGetValueHelper(dxKeyHandle, subKeyName, "DriverDate", driverDate, driverDateSize);

				if (returnCode != ERROR_SUCCESS)
				{
					return -1;
				}

				LOGF(eINFO, "Driver Date: %s", driverDate);
			}
		}
	}

	returnCode = ::RegCloseKey(dxKeyHandle);
	ASSERT(returnCode == ERROR_SUCCESS);
	tf_free(subKeyName);
	return ERROR_SUCCESS;
}

static void gpuPrintDriverInfo(char* driverVersion, uint32_t driverVersionSize, char* driverDate, uint32_t driverDateSize)
{
	LOGF(eINFO, "----Begin printing all graphics adapters available on the system----");
	initGpuDriverVersion(nullptr, driverVersion, driverVersionSize, driverDate, driverDateSize);
	LOGF(eINFO, "----Finish printing all graphics adapters available on the system----");
}

static void gpuDetectDriverInfo(char* selectedGPU, char* driverVersion, uint32_t driverVersionSize, char* driverDate, uint32_t driverDateSize)
{
	initGpuDriverVersion(selectedGPU, driverVersion, driverVersionSize, driverDate, driverDateSize);
}

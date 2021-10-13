/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

#include "Config.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/IFileSystem.h"
#include "../../Renderer/IRenderer.h"

#include <regex>

inline void fsReadFromStreamLine(FileStream* stream, char* pOutLine)
{
	uint32_t charIndex = 0;
	while (!fsStreamAtEnd(stream))
	{
		char nextChar = 0;
		fsReadFromStream(stream, &nextChar, sizeof(nextChar));
		if (nextChar == 0 || nextChar == '\n')
		{
			break;
		}
		if (nextChar == '\r')
		{
			char newLine = 0;
			fsReadFromStream(stream, &newLine, sizeof(newLine));
			if (newLine == '\n')
			{
				break;
			}
			else
			{
				// We're not looking at a "\r\n" sequence, so add the '\r' to the buffer.
				fsSeekStream(stream, SBO_CURRENT_POSITION, -1);
			}
		}
		pOutLine[charIndex++] = nextChar;
	}

	pOutLine[charIndex] = 0;
}

inline GPUPresetLevel stringToPresetLevel(const char* presetLevel)
{
	if (!stricmp(presetLevel, "office"))
		return GPU_PRESET_OFFICE;
	if (!stricmp(presetLevel, "low"))
		return GPU_PRESET_LOW;
	if (!stricmp(presetLevel, "medium"))
		return GPU_PRESET_MEDIUM;
	if (!stricmp(presetLevel, "high"))
		return GPU_PRESET_HIGH;
	if (!stricmp(presetLevel, "ultra"))
		return GPU_PRESET_ULTRA;

	return GPU_PRESET_NONE;
}

inline const char* presetLevelToString(GPUPresetLevel preset)
{
	switch (preset)
	{
	case GPU_PRESET_NONE: return "";
	case GPU_PRESET_OFFICE: return "office";
	case GPU_PRESET_LOW: return "low";
	case GPU_PRESET_MEDIUM: return "medium";
	case GPU_PRESET_HIGH: return "high";
	case GPU_PRESET_ULTRA: return "ultra";
	default: return NULL;
	}
}

inline bool parseConfigLine(
	const char* pLine,
	const char* pInVendorId,
	const char* pInModelId,
	const char* pInGpuName,
	char pOutVendorId[MAX_GPU_VENDOR_STRING_LENGTH],
	char pOutModelId[MAX_GPU_VENDOR_STRING_LENGTH],
	char pOutRevisionId[MAX_GPU_VENDOR_STRING_LENGTH],
	char pOutModelName[MAX_GPU_VENDOR_STRING_LENGTH],
    GPUPresetLevel* pOutPresetLevel)
{
	//     VendorId;         ModelId;        Preset;             Name;               RevisionId (optional); Codename (can be null)
	// ([0xA-Fa-f0-9]+); ([0xA-Fa-f0-9]+); ([A-Za-z]); ([A-Za-z0-9 /\\(\\);]+)[; ]*([0xA-Fa-f0-9]*)
	char buffer[128] = {};
	sprintf(
		buffer,
		"%s; %s; ([A-Za-z]+); ([A-Za-z0-9 /\\(\\);]+)[; ]*([0xA-Fa-f0-9]*)",
		// If input is unspecified it means we need to fill it
		pInVendorId ? pInVendorId : "([0xA-Fa-f0-9]+)",
		pInModelId ? pInModelId : "([0xA-Fa-f0-9]+)");

	const uint32_t offsetIndex = (pInVendorId ? 1 : 0) + (pInModelId ? 1 : 0);
	const uint32_t modelIndex = pInVendorId ? 1 : 2;
	const uint32_t presetIndex   = 3 - offsetIndex;
	const uint32_t gpuNameIndex  = 4 - offsetIndex;
	const uint32_t revisionIndex = 5 - offsetIndex;

	std::regex expr(buffer, std::regex::optimize);
	std::cmatch match;
	if (std::regex_match(pLine, match, expr))
	{
		if (!pInVendorId)
		{
			strncpy(pOutVendorId, match[1].first, match[1].second - match[1].first);
		}

		if (!pInModelId)
		{
			strncpy(pOutModelId, match[modelIndex].first, match[modelIndex].second - match[modelIndex].first);
		}

		char presetLevel[MAX_GPU_VENDOR_STRING_LENGTH] = {};
		strncpy(presetLevel, match[presetIndex].first,   match[presetIndex].second -   match[presetIndex].first);
		strncpy(pOutModelName,  match[gpuNameIndex].first,  match[gpuNameIndex].second -  match[gpuNameIndex].first);
		strncpy(pOutRevisionId,  match[revisionIndex].first, match[revisionIndex].second - match[revisionIndex].first);

		*pOutPresetLevel = stringToPresetLevel(presetLevel);
		
		if (pInGpuName)
			return strcmp(pInGpuName, pOutModelName) == 0;

		return true;
	}

	*pOutPresetLevel = GPU_PRESET_LOW;
	return false;
}

#if !defined(__ANDROID__)

#if !defined(NX64)

#if !defined(__APPLE__)

static GPUPresetLevel getSinglePresetLevel(const char* line, const char* inVendorId, const char* inModelId, const char* inRevId)
{
	char vendorId[MAX_GPU_VENDOR_STRING_LENGTH] = {};
	char deviceId[MAX_GPU_VENDOR_STRING_LENGTH] = {};
	GPUPresetLevel presetLevel = {};
	char gpuName[MAX_GPU_VENDOR_STRING_LENGTH] = {};
	char revisionId[MAX_GPU_VENDOR_STRING_LENGTH] = {};

	//check if current vendor line is one of the selected gpu's
	if (!parseConfigLine(line, inVendorId, inModelId, NULL, vendorId, deviceId, revisionId, gpuName, &presetLevel))
		return GPU_PRESET_NONE;

	//if we have a revision Id then we want to match it as well
	if (stricmp(inRevId, "0x00") != 0 && strlen(revisionId) && stricmp(revisionId, "0x00") != 0 && stricmp(inRevId, revisionId) != 0)
		return GPU_PRESET_NONE;

	return presetLevel;
}

#endif

//TODO: Add name matching as well.
static void checkForPresetLevel(const char* line, Renderer* pRenderer, uint32_t gpuCount, GPUSettings* pGpuSettings)
{
	char vendorId[MAX_GPU_VENDOR_STRING_LENGTH] = {};
	char deviceId[MAX_GPU_VENDOR_STRING_LENGTH] = {};
	GPUPresetLevel presetLevel = {};
	char gpuName[MAX_GPU_VENDOR_STRING_LENGTH] = {};
	char revisionId[MAX_GPU_VENDOR_STRING_LENGTH] = {};

	//search if any of the current gpu's match the current gpu cfg entry
	for (uint32_t i = 0; i < gpuCount; i++)
	{
		GPUSettings* currentSettings = &pGpuSettings[i];

		//check if current vendor line is one of the selected gpu's
		if (!parseConfigLine(line,
			currentSettings->mGpuVendorPreset.mVendorId,
			currentSettings->mGpuVendorPreset.mModelId,
			NULL,
			vendorId, deviceId, revisionId, gpuName, &presetLevel))
			return;

		//if we have a revision Id then we want to match it as well
		if (strcmp(currentSettings->mGpuVendorPreset.mRevisionId, "0x00") != 0 &&
			strlen(revisionId) && strcmp(revisionId, "0x00") != 0 &&
			strcmp(currentSettings->mGpuVendorPreset.mRevisionId, revisionId) == 0)
			continue;

		currentSettings->mGpuVendorPreset.mPresetLevel = presetLevel;

		//Extra information for GPU
		//Not all gpu's will have that info in the gpu.cfg file
		strncpy(currentSettings->mGpuVendorPreset.mGpuName, gpuName, MAX_GPU_VENDOR_STRING_LENGTH);
	}
}

#if !defined(__APPLE__)

static bool checkForActiveGPU(const char* line, GPUVendorPreset& pActiveGpu)
{
	if (!parseConfigLine(
		line,
		NULL, NULL, NULL,
		pActiveGpu.mVendorId,
		pActiveGpu.mModelId,
		pActiveGpu.mRevisionId,
		pActiveGpu.mGpuName,
		&pActiveGpu.mPresetLevel))
		return false;

	// #TODO: Hardcoded for now as its only used for automated testing
	// We will want to test with different presets
	pActiveGpu.mPresetLevel = GPU_PRESET_ULTRA;

	return true;
}

#endif

//Reads the gpu config and sets the preset level of all available gpu's
inline void setGPUPresetLevel(Renderer* pRenderer, uint32_t gpuCount, GPUSettings* pGpuSettings)
{
	FileStream fh = {};
	if (!fsOpenStreamFromPath(RD_GPU_CONFIG, "gpu.cfg", FM_READ, NULL, &fh))
	{
		LOGF(LogLevel::eWARNING, "gpu.cfg could not be found, setting preset to Low as a default.");
		return;
	}

	char configStr[1024] = {};
	while (!fsStreamAtEnd(&fh))
	{
		fsReadFromStreamLine(&fh, configStr);
		checkForPresetLevel(configStr, pRenderer, gpuCount, pGpuSettings);
		// Do something with the tok
	}

	fsCloseStream(&fh);
}

#endif
#endif

#if !defined(__APPLE__)

#if defined(VULKAN)
//Reads the gpu config and sets the preset level of all available gpu's
static GPUPresetLevel getGPUPresetLevel(const eastl::string& vendorId, const eastl::string& modelId, const eastl::string& revId)
{
	LOGF(LogLevel::eINFO, "No gpu.cfg support. Preset set to Low");
	GPUPresetLevel foundLevel = GPU_PRESET_LOW;
	return foundLevel;
}
#endif

#if !defined(NX64)
#if !defined(__ANDROID__)

//Reads the gpu config and sets the preset level of all available gpu's
static GPUPresetLevel getGPUPresetLevel(const char* vendorId, const char* modelId, const char* revId)
{
	FileStream fh = {};
	if (!fsOpenStreamFromPath(RD_GPU_CONFIG, "gpu.cfg", FM_READ, NULL, &fh))
	{
		LOGF(LogLevel::eWARNING, "gpu.cfg could not be found, setting preset to Low as a default.");
		return GPU_PRESET_LOW;
	}

	GPUPresetLevel foundLevel = GPU_PRESET_LOW;

	char gpuCfgString[1024] = {};
	while (!fsStreamAtEnd(&fh))
	{
		fsReadFromStreamLine(&fh, gpuCfgString);
		GPUPresetLevel level = getSinglePresetLevel(gpuCfgString, vendorId, modelId, revId);
		// Do something with the tok
		if (level != GPU_PRESET_NONE)
		{
			foundLevel = level;
			break;
		}
	}

	fsCloseStream(&fh);
	return foundLevel;
}

static bool getActiveGpuConfig(GPUVendorPreset& pActiveGpu)
{
	FileStream fh = {};
	if (!fsOpenStreamFromPath(RD_GPU_CONFIG, "activeTestingGpu.cfg", FM_READ, NULL, &fh))
	{
		LOGF(LogLevel::eINFO, "activeTestingGpu.cfg could not be found, Using default GPU.");
		return false;
	}

	bool successFinal = false;
	char gpuCfgString[1024] = {};
	while (!fsStreamAtEnd(&fh) && !successFinal)
	{
		fsReadFromStreamLine(&fh, gpuCfgString);
		successFinal = checkForActiveGPU(gpuCfgString, pActiveGpu);
	}

	fsCloseStream(&fh);

	return successFinal;
}

static void selectActiveGpu(GPUSettings* pGpuSettings, uint32_t* pGpuIndex, uint32_t gpuCount)
{
	GPUVendorPreset activeTestingPreset;
	bool            activeTestingGpu = getActiveGpuConfig(activeTestingPreset);
	if (activeTestingGpu)
	{
		for (uint32_t i = 0; i < gpuCount; i++)
		{
			if (strcmp(pGpuSettings[i].mGpuVendorPreset.mVendorId,activeTestingPreset.mVendorId)==0 &&
				strcmp(pGpuSettings[i].mGpuVendorPreset.mModelId,activeTestingPreset.mModelId)==0)
			{
				//if revision ID is valid then use it to select active GPU
				if (strcmp(pGpuSettings[i].mGpuVendorPreset.mRevisionId,"0x00")!=0 &&
					strcmp(pGpuSettings[i].mGpuVendorPreset.mRevisionId,activeTestingPreset.mRevisionId)!=0)
					continue;

				*pGpuIndex = i;
				break;
			}
		}
	}
}

#endif

static bool isGPUWhitelisted(const char* vendorId, const char* modelId, const char* gpuName)
{
	FileStream fh = {};
	if (!fsOpenStreamFromPath(RD_GPU_CONFIG, "gpuWhitelist.cfg", FM_READ, NULL, &fh))
	{
		LOGF(LogLevel::eINFO, "gpuWhitelist.cfg could not be found, Using default API.");
		return false;
	}

	GPUVendorPreset gpuPreset = {};

	bool successFinal = false;
	char gpuCfgString[1024] = {};
	while (!fsStreamAtEnd(&fh) && !successFinal)
	{
		fsReadFromStreamLine(&fh, gpuCfgString);
		successFinal = parseConfigLine(gpuCfgString, vendorId, modelId, gpuName,
			gpuPreset.mVendorId, gpuPreset.mModelId, gpuPreset.mRevisionId, gpuPreset.mGpuName, &gpuPreset.mPresetLevel);

	}

	fsCloseStream(&fh);
	return successFinal;
}

#endif
#endif

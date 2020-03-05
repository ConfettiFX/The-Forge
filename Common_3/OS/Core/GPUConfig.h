#pragma once

#ifndef TARGET_IOS
#include "../../ThirdParty/OpenSource/EASTL/string.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/IFileSystem.h"
#include "../../Renderer/IRenderer.h"

static bool parseConfigLine(
	eastl::string line, eastl::string& vendorId, eastl::string& deviceId, eastl::string& revId, eastl::string& deviceName,
    eastl::string& presetLevel)
{
	auto parseNext = [](eastl::string line, size_t& it) {
		if (it == eastl::string::npos) return eastl::string();
		size_t prev = it;
		it = line.find_first_of(';', it);
		it += (it != eastl::string::npos);
		return line.substr(prev, it == eastl::string::npos ? eastl::string::npos : it - prev - 1);
	};

	line.trim();
	//don't parse if commented line
	if (line.empty() || line.at(0) == '#')
		return false;

	size_t pos = 0;

	vendorId = parseNext(line, pos);
	if (pos == eastl::string::npos)
		return false;

	deviceId = parseNext(line, pos);
	if (pos == eastl::string::npos)
		return false;

	presetLevel = parseNext(line, pos);
	deviceName = parseNext(line, pos);
	revId = parseNext(line, pos);

	vendorId.trim();
	vendorId.make_lower();
	deviceId.trim();
	deviceId.make_lower();
	presetLevel.trim();
	presetLevel.make_lower();
	deviceName.trim();
	revId.trim();

	if (revId.empty())
		revId = "0x00";

	return true;
}

static GPUPresetLevel stringToPresetLevel(eastl::string& presetLevel)
{
	if (presetLevel == "office")
		return GPU_PRESET_OFFICE;
	if (presetLevel == "low")
		return GPU_PRESET_LOW;
	if (presetLevel == "medium")
		return GPU_PRESET_MEDIUM;
	if (presetLevel == "high")
		return GPU_PRESET_HIGH;
	if (presetLevel == "ultra")
		return GPU_PRESET_ULTRA;

	return GPU_PRESET_NONE;
}

static const char* presetLevelToString(GPUPresetLevel preset)
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

#if !defined(METAL) && !defined(NX64)
static GPUPresetLevel
	getSinglePresetLevel(eastl::string line, const eastl::string& inVendorId, const eastl::string& inModelId, const eastl::string& inRevId)
{
	eastl::string vendorId;
	eastl::string deviceId;
	eastl::string presetLevel;
	eastl::string gpuName;
	eastl::string revisionId;

	if (!parseConfigLine(line, vendorId, deviceId, revisionId, gpuName, presetLevel))
		return GPU_PRESET_NONE;

	//check if current vendor line is one of the selected gpu's
	//compare both ModelId and VendorId
	if (inVendorId == vendorId && inModelId == deviceId)
	{
		//if we have a revision Id then we want to match it as well
		if (inRevId != "0x00" && revisionId != "0x00" && inRevId != revisionId)
			return GPU_PRESET_NONE;

		return stringToPresetLevel(presetLevel);
	}

	return GPU_PRESET_NONE;
}
#endif

#if !defined(__ANDROID__) && !defined(NX64)
//TODO: Add name matching as well.
static void checkForPresetLevel(eastl::string line, Renderer* pRenderer, uint32_t gpuCount, GPUSettings* pGpuSettings)
{
	eastl::string vendorId;
	eastl::string deviceId;
	eastl::string presetLevel;
	eastl::string gpuName;
	eastl::string revisionId;

	if (!parseConfigLine(line, vendorId, deviceId, revisionId, gpuName, presetLevel))
		return;

	//search if any of the current gpu's match the current gpu cfg entry
	for (uint32_t i = 0; i < gpuCount; i++)
	{
		GPUSettings* currentSettings = &pGpuSettings[i];
		//check if current vendor line is one of the selected gpu's
		//compare both ModelId and VendorId
		if (strcmp(currentSettings->mGpuVendorPreset.mVendorId, vendorId.c_str()) == 0 &&
			strcmp(currentSettings->mGpuVendorPreset.mModelId, deviceId.c_str()) == 0)
		{
			//if we have a revision Id then we want to match it as well
			if (strcmp(currentSettings->mGpuVendorPreset.mRevisionId, "0x00") != 0 && revisionId.compare("0x00") != 0 &&
				strcmp(currentSettings->mGpuVendorPreset.mRevisionId, revisionId.c_str()) == 0)
				continue;

			currentSettings->mGpuVendorPreset.mPresetLevel = stringToPresetLevel(presetLevel);

			//Extra information for GPU
			//Not all gpu's will have that info in the gpu.cfg file
			strncpy(currentSettings->mGpuVendorPreset.mGpuName, gpuName.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);
		}
	}
}
#endif

#if !defined(METAL) && !defined(__ANDROID__) && !defined(NX64)
static bool checkForActiveGPU(eastl::string line, GPUVendorPreset& pActiveGpu)
{
	eastl::string vendorId;
	eastl::string deviceId;
	eastl::string presetLevel;
	eastl::string gpuName;
	eastl::string revisionId;

	if (!parseConfigLine(line, vendorId, deviceId, revisionId, gpuName, presetLevel))
		return false;

	strncpy(pActiveGpu.mModelId, deviceId.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);
	strncpy(pActiveGpu.mVendorId, vendorId.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);
	strncpy(pActiveGpu.mGpuName, gpuName.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);
	strncpy(pActiveGpu.mRevisionId, revisionId.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);

	//TODO: Hardcoded for now as its only used for automated testing
	//We will want to test with different presets
	pActiveGpu.mPresetLevel = GPU_PRESET_ULTRA;

	return true;
}
#endif

#if !defined(__ANDROID__) && !defined(NX64)
//Reads the gpu config and sets the preset level of all available gpu's
static void setGPUPresetLevel(Renderer* pRenderer, uint32_t gpuCount, GPUSettings* pGpuSettings)
{
	FileStream* fh = fsOpenFileInResourceDirectory(RD_GPU_CONFIG, "gpu.cfg", FM_READ);
	if (!fh)
	{
		LOGF(LogLevel::eWARNING, "gpu.cfg could not be found, setting preset to Low as a default.");
		return;
	}

    char configStr[2048];
	while (!fsStreamAtEnd(fh))
	{
        fsReadFromStreamLine(fh, configStr, 2048);
		checkForPresetLevel(configStr, pRenderer, gpuCount, pGpuSettings);
		// Do something with the tok
	}

    fsCloseStream(fh);
}
#endif


#if defined(__ANDROID__) || defined(NX64)
//Reads the gpu config and sets the preset level of all available gpu's
static GPUPresetLevel getGPUPresetLevel(const eastl::string vendorId, const eastl::string modelId, const eastl::string revId)
{
	LOGF(LogLevel::eINFO, "No gpu.cfg support. Preset set to Low");
	GPUPresetLevel foundLevel = GPU_PRESET_LOW;
	return foundLevel;
}
#endif


#if !defined(METAL) && !defined(__ANDROID__) && !defined(NX64)
//Reads the gpu config and sets the preset level of all available gpu's
static GPUPresetLevel getGPUPresetLevel(const eastl::string vendorId, const eastl::string modelId, const eastl::string revId)
{

	FileStream* fh = fsOpenFileInResourceDirectory(RD_GPU_CONFIG, "gpu.cfg", FM_READ_BINARY);
	if (!fh)
	{
		LOGF(LogLevel::eWARNING, "gpu.cfg could not be found, setting preset to Low as a default.");
		return GPU_PRESET_LOW;
	}

	GPUPresetLevel foundLevel = GPU_PRESET_LOW;

	while (!fsStreamAtEnd(fh))
	{
		eastl::string  gpuCfgString = fsReadFromStreamSTLLine(fh);
		GPUPresetLevel level = getSinglePresetLevel(gpuCfgString, vendorId, modelId, revId);
		// Do something with the tok
		if (level != GPU_PRESET_NONE)
		{
			foundLevel = level;
			break;
		}
	}

	fsCloseStream(fh);
	return foundLevel;
}

#if defined(AUTOMATED_TESTING) && defined(ACTIVE_TESTING_GPU)
static bool getActiveGpuConfig(GPUVendorPreset& pActiveGpu)
{
	FileStream* fh = fsOpenFileInResourceDirectory(RD_GPU_CONFIG, "activeTestingGpu.cfg", FM_READ_BINARY);
	if (!fh)
	{
		LOGF(LogLevel::eINFO, "activeTestingGpu.cfg could not be found, Using default GPU.");
		return false;
	}

	bool successFinal = false;
	while (!fsStreamAtEnd(fh) && !successFinal)
	{
		eastl::string gpuCfgString = fsReadFromStreamSTLLine(fh);
		successFinal = checkForActiveGPU(gpuCfgString, pActiveGpu);
	}

	fsCloseStream(fh);

	return successFinal;
}
#endif
#endif
#endif

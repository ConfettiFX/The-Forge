#pragma once

#ifndef TARGET_IOS
#include "../../ThirdParty/OpenSource/TinySTL/string.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IFileSystem.h"
#include "../../Renderer/IRenderer.h"

static GPUPresetLevel getSinglePresetLevel(String line,const String& inVendorId, const String& inModelId, const String& inRevId)
{
	//remove extra whitespace to check if line is a comment (starts with #)
	line = line.trimmed();

	//don't parse if commented line
	if (line.at(0) == '#')
		return GPU_PRESET_NONE;

	//remote comment from line
	tinystl::vector<String> parsedString = line.split(';');

	//We need at least 3 entries (vendor, Model, Preset)
	//The file is layed out the following way:
	//Model ID; Vendor ID; Preset; GPU Name; Revision ID;
	if (parsedString.size() < 3)
		return GPU_PRESET_NONE;

	String vendorId = parsedString[0].to_lower();
	String deviceId = parsedString[1].to_lower();
	String presetLevel = parsedString[2].to_lower();
	String gpuName = "";
	String revisionId = "0x00";

	if (parsedString.size() >= 4)
	{
		gpuName = parsedString[3];
	}

	if (parsedString.size() >= 5)
	{
		revisionId = parsedString[4].to_lower();
	}

	//trim whitespace
	vendorId = vendorId.trimmed();
	deviceId = deviceId.trimmed();
	presetLevel = presetLevel.trimmed();
	gpuName = gpuName.trimmed();
	revisionId = revisionId.trimmed();

	//check if current vendor line is one of the selected gpu's
	//compare both ModelId and VendorId
	if (inVendorId == vendorId && inModelId == deviceId) {

		//if we have a revision Id then we want to match it as well
		if (inRevId != "0x00" && revisionId!= "0x00" && inRevId != revisionId)
			return GPU_PRESET_NONE;

		//assign preset to gpu that's been identified.
		if (presetLevel == "office")
			return GPU_PRESET_OFFICE;
		else if (presetLevel == "low")
			return GPU_PRESET_LOW;
		else if (presetLevel == "medium")
			return GPU_PRESET_MEDIUM;
		else if (presetLevel == "high")
			return GPU_PRESET_HIGH;
		else if (presetLevel == "ultra")
			return GPU_PRESET_ULTRA;
		else
			return GPU_PRESET_NONE;
	}

	return GPU_PRESET_NONE;
	
}

//TODO: Add name matching as well.
static void checkForPresetLevel(String line, Renderer * pRenderer)
{
	//remove extra whitespace to check if line is a comment (starts with #)
	line = line.trimmed();

	//don't parse if commented line
	if (line.at(0) == '#')
		return;

	//remote comment from line
	tinystl::vector<String> parsedString = line.split(';');
	
	//We need at least 3 entries (vendor, Model, Preset)
	//The file is layed out the following way:
	//Model ID; Vendor ID; Preset; GPU Name; Revision ID;
	if (parsedString.size() < 3)
		return;

	String vendorId = parsedString[0].to_lower();
	String deviceId = parsedString[1].to_lower();
	String presetLevel = parsedString[2].to_lower();
	String gpuName = "";
	String revisionId = "0x00";

	if(parsedString.size() >= 4)
	{
		gpuName = parsedString[3];
	}

	if (parsedString.size() >= 5)
	{
		revisionId = parsedString[4].to_lower();
	}

	//trim whitespace
	vendorId = vendorId.trimmed();
	deviceId = deviceId.trimmed();
	presetLevel = presetLevel.trimmed();
	gpuName = gpuName.trimmed();
	revisionId = revisionId.trimmed();

	//search if any of the current gpu's match the current gpu cfg entry
	for(uint32_t i = 0 ; i < pRenderer->mNumOfGPUs ; i++) {
		GPUSettings* currentSettings = &pRenderer->mGpuSettings[i];
		//check if current vendor line is one of the selected gpu's
		//compare both ModelId and VendorId
		if (strcmp(currentSettings->mGpuVendorPreset.mVendorId, vendorId.c_str()) == 0 && strcmp(currentSettings->mGpuVendorPreset.mModelId, deviceId.c_str()) == 0) {
			
			//if we have a revision Id then we want to match it as well
			if (strcmp(currentSettings->mGpuVendorPreset.mRevisionId, "0x00") != 0 && strcmp(revisionId, "0x00") != 0 && strcmp(currentSettings->mGpuVendorPreset.mRevisionId, revisionId.c_str()) == 0)
				continue;

			//assign preset to gpu that's been identified.
			if (presetLevel == "office")
				currentSettings->mGpuVendorPreset.mPresetLevel = GPU_PRESET_OFFICE;
			else if (presetLevel == "low")
				currentSettings->mGpuVendorPreset.mPresetLevel = GPU_PRESET_LOW;
			else if (presetLevel == "medium")
				currentSettings->mGpuVendorPreset.mPresetLevel = GPU_PRESET_MEDIUM;
			else if (presetLevel == "high")
				currentSettings->mGpuVendorPreset.mPresetLevel = GPU_PRESET_HIGH;
			else if (presetLevel == "ultra")
				currentSettings->mGpuVendorPreset.mPresetLevel = GPU_PRESET_ULTRA;
			else
				currentSettings->mGpuVendorPreset.mPresetLevel = GPU_PRESET_NONE;

			//Extra information for GPU
			//Not all gpu's will have that info in the gpu.cfg file
			strncpy(currentSettings->mGpuVendorPreset.mGpuName, gpuName.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);
		}
	}
}

static bool checkForActiveGPU(String line, GPUVendorPreset &pActiveGpu)
{
	//remove extra whitespace to check if line is a comment (starts with #)
	line = line.trimmed();

	//remote comment from line
	if(line.at(0) == '#')
		return false;

	tinystl::vector<String> parsedString = line.split(';');

	//for every valid entry there's a comment
	if (parsedString.size() < 3)
		return false;

	//TODO: Parse SLI
	String vendorId = parsedString[0].to_lower();
	String deviceId = parsedString[1].to_lower();
	String presetLevel = parsedString[2].to_lower();

	String gpuName = "";
	String revisionId = "0x00";

	if (parsedString.size() >= 4)
	{
		gpuName = parsedString[3];
	}

	if (parsedString.size() >= 5)
	{
		revisionId = parsedString[4].to_lower();
	}

	//trim whitespace
	vendorId = vendorId.trimmed();
	deviceId = deviceId.trimmed();
	presetLevel = presetLevel.trimmed();
	gpuName = gpuName.trimmed();
	revisionId = revisionId.trimmed();

	strncpy(pActiveGpu.mModelId, deviceId.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);
	strncpy(pActiveGpu.mVendorId, vendorId.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);
	strncpy(pActiveGpu.mGpuName, gpuName.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);
	strncpy(pActiveGpu.mRevisionId, revisionId.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);

	//TODO: Hardcoded for now as its only used for automated testing
	//We will want to test with different presets
	pActiveGpu.mPresetLevel = GPU_PRESET_ULTRA;
	
	return true;
}

//Reads the gpu config and sets the preset level of all available gpu's
static void setGPUPresetLevel(Renderer * pRenderer)
{
	File gpuCfgFile = {};
	gpuCfgFile.Open("gpu.cfg",FM_Read, FSR_GpuConfig);
	if(!gpuCfgFile.IsOpen()) {
		LOGWARNING("gpu.cfg could not be found, setting preset to Low as a default.");
		return;
	}

	while (!gpuCfgFile.IsEof()) {
		String gpuCfgString = gpuCfgFile.ReadLine();
		checkForPresetLevel(gpuCfgString, pRenderer);
		// Do something with the tok
	}

	gpuCfgFile.Close();
}

//Reads the gpu config and sets the preset level of all available gpu's
static GPUPresetLevel getGPUPresetLevel(const String vendorId, const String modelId, const String revId)
{
	File gpuCfgFile = {};
	gpuCfgFile.Open("gpu.cfg", FM_Read, FSR_GpuConfig);
	if (!gpuCfgFile.IsOpen()) {
		LOGWARNING("gpu.cfg could not be found, setting preset to Low as a default.");
		return GPU_PRESET_LOW;
	}

	GPUPresetLevel foundLevel = GPU_PRESET_LOW;

	while (!gpuCfgFile.IsEof()) {
		String gpuCfgString = gpuCfgFile.ReadLine();
		GPUPresetLevel level = getSinglePresetLevel(gpuCfgString, vendorId, modelId, revId);
		// Do something with the tok
		if (level != GPU_PRESET_NONE)
		{
			foundLevel = level;
			break;
		}
	}

	gpuCfgFile.Close();
	return foundLevel;
}

static bool getActiveGpuConfig(GPUVendorPreset &pActiveGpu)
{
	File gpuCfgFile = {};
	gpuCfgFile.Open("activeTestingGpu.cfg", FM_Read, FSR_GpuConfig);
	if (!gpuCfgFile.IsOpen()) {
		LOGINFO("activeTestingGpu.cfg could not be found, Using default GPU.");
		return false;
	}

	bool successFinal = false;
	while (!gpuCfgFile.IsEof() && !successFinal) {
		String gpuCfgString = gpuCfgFile.ReadLine();
		successFinal = checkForActiveGPU(gpuCfgString, pActiveGpu);
	}

	gpuCfgFile.Close();

	return successFinal;
}

#endif

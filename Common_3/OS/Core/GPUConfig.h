#pragma once

#ifndef TARGET_IOS
#include "../../ThirdParty/OpenSource/TinySTL/string.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IFileSystem.h"
#include "../../Renderer/IRenderer.h"

//TODO: Add name matching as well.
static void checkForPresetLevel(String line, Renderer * pRenderer)
{
	//remote comment from line
	tinystl::vector<String> splitComment = line.split(';');
	
	//for every valid entry there's a comment
	if (splitComment.size() < 2)
		return;
	
	tinystl::vector<String> parsedString = splitComment[0].split(',');

	//we need 3 entry (vendor, model, preset)
	if (parsedString.size() < 3)
		return;

	String vendorId = parsedString[0].to_lower();
	String deviceId = parsedString[1].to_lower();
	String presetLevel = parsedString[2].to_lower();

	//trim whitespace
	vendorId = vendorId.trimmed();
	deviceId = deviceId.trimmed();
	presetLevel = presetLevel.trimmed();

	//search if any of the current gpu's match the current gpu cfg entry
	for(uint32_t i = 0 ; i < pRenderer->mNumOfGPUs ; i++) {
		GPUSettings* currentSettings = &pRenderer->mGpuSettings[i];
		//check if current vendor line is one of the selected gpu's
		//compare both ModelId and VendorId
		if (currentSettings->mGpuVendorPreset.mVendorId == vendorId && currentSettings->mGpuVendorPreset.mModelId == deviceId) {

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

		}
	}
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

#endif
/*
 * Copyright (c) 2019 Confetti Interactive Inc.
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
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/ResourceLoader.h"
#include "../../Common_3/OS/Image/Image.h"
#include "../../Common_3/OS/Image/ImageEnums.h"
#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

#include "AppHelpers.h"

void loadTexturesTask(void* data, uintptr_t i)
{
	TextureLoadTaskData* pTaskData = (TextureLoadTaskData*)data;
	TextureLoadDesc desc = pTaskData->mDesc;
	desc.pFilename = pTaskData->mNames[i];
	desc.ppTexture = &pTaskData->mTextures[i];
	addResource(&desc, true);
};

void computePBRMapsTask(void* data, uintptr_t mode)
{
	ComputePBRMapsTaskData* pTaskData = (ComputePBRMapsTaskData*)data;
	computePBRMaps(pTaskData);
	if (mode != GPU_TASK_BEHAVIOR_RECORD_ONLY)
	{
		queueSubmit(pTaskData->pQueue, 1, &pTaskData->pCmd, pTaskData->pFence, 0, 0, uint32_t(pTaskData->pSemaphore != 0), &pTaskData->pSemaphore);
	}
	if (mode != GPU_TASK_BEHAVIOR_SUBMIT_ONLY)
	{
		waitForFences(pTaskData->pRenderer, 1, &pTaskData->pFence);
	}
}

void computePBRMaps(ComputePBRMapsTaskData* pTaskData)
{
	Renderer* pRenderer = pTaskData->pRenderer;

	SamplerDesc samplerDesc = {
		FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0, 16
	};
	addSampler(pRenderer, &samplerDesc, &pTaskData->pSkyboxSampler);

	// Load the skybox panorama texture.
	SyncToken       token;
	TextureLoadDesc panoDesc = {};
	panoDesc.mRoot = FSR_Textures;
	panoDesc.mUseMipmaps = true;
	panoDesc.pFilename = pTaskData->mSourceName;
	panoDesc.ppTexture = &pTaskData->pPanoSkybox;
	addResource(&panoDesc, &token);

	TextureDesc skyboxImgDesc = {};
	skyboxImgDesc.mArraySize = 6;
	skyboxImgDesc.mDepth = 1;
	skyboxImgDesc.mFormat = ImageFormat::RGBA32F;
	skyboxImgDesc.mHeight = pTaskData->mSkyboxSize;
	skyboxImgDesc.mWidth = pTaskData->mSkyboxSize;
	skyboxImgDesc.mMipLevels = pTaskData->mSkyboxMips;
	skyboxImgDesc.mSampleCount = SAMPLE_COUNT_1;
	skyboxImgDesc.mSrgb = false;
	skyboxImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
	skyboxImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
	skyboxImgDesc.pDebugName = L"skyboxImgBuff";

	TextureLoadDesc skyboxLoadDesc = {};
	skyboxLoadDesc.pDesc = &skyboxImgDesc;
	skyboxLoadDesc.ppTexture = &pTaskData->pSkybox;
	addResource(&skyboxLoadDesc);

	TextureDesc irrImgDesc = {};
	irrImgDesc.mArraySize = 6;
	irrImgDesc.mDepth = 1;
	irrImgDesc.mFormat = ImageFormat::RGBA32F;
	irrImgDesc.mHeight = pTaskData->mIrradianceSize;
	irrImgDesc.mWidth = pTaskData->mIrradianceSize;
	irrImgDesc.mMipLevels = 1;
	irrImgDesc.mSampleCount = SAMPLE_COUNT_1;
	irrImgDesc.mSrgb = false;
	irrImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
	irrImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
	irrImgDesc.pDebugName = L"irrImgBuff";

	TextureLoadDesc irrLoadDesc = {};
	irrLoadDesc.pDesc = &irrImgDesc;
	irrLoadDesc.ppTexture = &pTaskData->pIrradianceMap;
	addResource(&irrLoadDesc);

	TextureDesc specImgDesc = {};
	specImgDesc.mArraySize = 6;
	specImgDesc.mDepth = 1;
	specImgDesc.mFormat = ImageFormat::RGBA32F;
	specImgDesc.mHeight = pTaskData->mSpecularSize;
	specImgDesc.mWidth = pTaskData->mSpecularSize;
	specImgDesc.mMipLevels = pTaskData->mSpecularMips;
	specImgDesc.mSampleCount = SAMPLE_COUNT_1;
	specImgDesc.mSrgb = false;
	specImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
	specImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
	specImgDesc.pDebugName = L"specImgBuff";

	TextureLoadDesc specImgLoadDesc = {};
	specImgLoadDesc.pDesc = &specImgDesc;
	specImgLoadDesc.ppTexture = &pTaskData->pSpecularMap;
	addResource(&specImgLoadDesc);

	// Create empty texture for BRDF integration map.
	TextureLoadDesc brdfIntegrationLoadDesc = {};
	TextureDesc     brdfIntegrationDesc = {};
	brdfIntegrationDesc.mWidth = pTaskData->mBRDFIntegrationSize;
	brdfIntegrationDesc.mHeight = pTaskData->mBRDFIntegrationSize;
	brdfIntegrationDesc.mDepth = 1;
	brdfIntegrationDesc.mArraySize = 1;
	brdfIntegrationDesc.mMipLevels = 1;
	brdfIntegrationDesc.mFormat = ImageFormat::RG32F;
	brdfIntegrationDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
	brdfIntegrationDesc.mSampleCount = SAMPLE_COUNT_1;
	brdfIntegrationDesc.mHostVisible = false;
	brdfIntegrationLoadDesc.pDesc = &brdfIntegrationDesc;
	brdfIntegrationLoadDesc.ppTexture = &pTaskData->pBRDFIntegrationMap;
	addResource(&brdfIntegrationLoadDesc);

	// Load pre-processing shaders.
	ShaderLoadDesc panoToCubeShaderDesc = {};
	panoToCubeShaderDesc.mStages[0] = { "panoToCube.comp", NULL, 0, FSR_SrcShaders };

	GPUPresetLevel presetLevel = pTaskData->mPresetLevel;
	uint32_t       importanceSampleCounts[GPUPresetLevel::GPU_PRESET_COUNT] = { 0, 0, 64, 128, 256, 1024 };
	uint32_t       importanceSampleCount = importanceSampleCounts[presetLevel];
	ShaderMacro    importanceSampleMacro = { "IMPORTANCE_SAMPLE_COUNT", eastl::string().sprintf("%u", importanceSampleCount) };

	ShaderLoadDesc brdfIntegrationShaderDesc = {};
	brdfIntegrationShaderDesc.mStages[0] = { "BRDFIntegration.comp", &importanceSampleMacro, 1, FSR_SrcShaders };

	ShaderLoadDesc irradianceShaderDesc = {};
	irradianceShaderDesc.mStages[0] = { "computeIrradianceMap.comp", NULL, 0, FSR_SrcShaders };

	ShaderLoadDesc specularShaderDesc = {};
	specularShaderDesc.mStages[0] = { "computeSpecularMap.comp", &importanceSampleMacro, 1, FSR_SrcShaders };

	addShader(pRenderer, &panoToCubeShaderDesc, &pTaskData->pPanoToCubeShader);
	addShader(pRenderer, &brdfIntegrationShaderDesc, &pTaskData->pBRDFIntegrationShader);
	addShader(pRenderer, &irradianceShaderDesc, &pTaskData->pIrradianceShader);
	addShader(pRenderer, &specularShaderDesc, &pTaskData->pSpecularShader);

	const char*       pStaticSamplerNames[] = { "skyboxSampler" };
	RootSignatureDesc panoRootDesc = { &pTaskData->pPanoToCubeShader, 1 };
	panoRootDesc.mStaticSamplerCount = 1;
	panoRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
	panoRootDesc.ppStaticSamplers = &pTaskData->pSkyboxSampler;
	RootSignatureDesc brdfRootDesc = { &pTaskData->pBRDFIntegrationShader, 1 };
	brdfRootDesc.mStaticSamplerCount = 1;
	brdfRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
	brdfRootDesc.ppStaticSamplers = &pTaskData->pSkyboxSampler;
	RootSignatureDesc irradianceRootDesc = { &pTaskData->pIrradianceShader, 1 };
	irradianceRootDesc.mStaticSamplerCount = 1;
	irradianceRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
	irradianceRootDesc.ppStaticSamplers = &pTaskData->pSkyboxSampler;
	RootSignatureDesc specularRootDesc = { &pTaskData->pSpecularShader, 1 };
	specularRootDesc.mStaticSamplerCount = 1;
	specularRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
	specularRootDesc.ppStaticSamplers = &pTaskData->pSkyboxSampler;
	addRootSignature(pRenderer, &panoRootDesc, &pTaskData->pPanoToCubeRootSignature);
	addRootSignature(pRenderer, &brdfRootDesc, &pTaskData->pBRDFIntegrationRootSignature);
	addRootSignature(pRenderer, &irradianceRootDesc, &pTaskData->pIrradianceRootSignature);
	addRootSignature(pRenderer, &specularRootDesc, &pTaskData->pSpecularRootSignature);

	DescriptorBinderDesc descriptorBinderDesc[4] = {
		{ pTaskData->pPanoToCubeRootSignature, pTaskData->mSkyboxMips + 1, pTaskData->mSkyboxMips + 1 },
		{ pTaskData->pBRDFIntegrationRootSignature },
		{ pTaskData->pIrradianceRootSignature },
		{ pTaskData->pSpecularRootSignature, pTaskData->mSkyboxMips + 1, pTaskData->mSpecularMips + 1 }
	};

	addDescriptorBinder(pRenderer, 0, 4, descriptorBinderDesc, &pTaskData->pPBRDescriptorBinder);

	PipelineDesc desc = {};
	desc.mType = PIPELINE_TYPE_COMPUTE;
	ComputePipelineDesc& pipelineSettings = desc.mComputeDesc;
	pipelineSettings.pShaderProgram = pTaskData->pPanoToCubeShader;
	pipelineSettings.pRootSignature = pTaskData->pPanoToCubeRootSignature;
	addPipeline(pRenderer, &desc, &pTaskData->pPanoToCubePipeline);
	pipelineSettings.pShaderProgram = pTaskData->pBRDFIntegrationShader;
	pipelineSettings.pRootSignature = pTaskData->pBRDFIntegrationRootSignature;
	addPipeline(pRenderer, &desc, &pTaskData->pBRDFIntegrationPipeline);
	pipelineSettings.pShaderProgram = pTaskData->pIrradianceShader;
	pipelineSettings.pRootSignature = pTaskData->pIrradianceRootSignature;
	addPipeline(pRenderer, &desc, &pTaskData->pIrradiancePipeline);
	pipelineSettings.pShaderProgram = pTaskData->pSpecularShader;
	pipelineSettings.pRootSignature = pTaskData->pSpecularRootSignature;
	addPipeline(pRenderer, &desc, &pTaskData->pSpecularPipeline);

	// Compute the BRDF Integration map.
	beginCmd(pTaskData->pCmd);

	TextureBarrier uavBarriers[4] = {
		{ pTaskData->pSkybox, RESOURCE_STATE_UNORDERED_ACCESS },
		{ pTaskData->pIrradianceMap, RESOURCE_STATE_UNORDERED_ACCESS },
		{ pTaskData->pSpecularMap, RESOURCE_STATE_UNORDERED_ACCESS },
		{ pTaskData->pBRDFIntegrationMap, RESOURCE_STATE_UNORDERED_ACCESS },
	};
	cmdResourceBarrier(pTaskData->pCmd, 0, NULL, 4, uavBarriers, false);

	cmdBindPipeline(pTaskData->pCmd, pTaskData->pBRDFIntegrationPipeline);
	DescriptorData params[2] = {};
	params[0].pName = "dstTexture";
	params[0].ppTextures = &pTaskData->pBRDFIntegrationMap;
	cmdBindDescriptors(pTaskData->pCmd, pTaskData->pPBRDescriptorBinder, pTaskData->pBRDFIntegrationRootSignature, 1, params);
	const uint32_t* pThreadGroupSize = pTaskData->pBRDFIntegrationShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
	cmdDispatch(
		pTaskData->pCmd, pTaskData->mBRDFIntegrationSize / pThreadGroupSize[0], pTaskData->mBRDFIntegrationSize / pThreadGroupSize[1],
		pThreadGroupSize[2]);

	TextureBarrier srvBarrier[1] = { { pTaskData->pBRDFIntegrationMap, RESOURCE_STATE_SHADER_RESOURCE } };

	cmdResourceBarrier(pTaskData->pCmd, 0, NULL, 1, srvBarrier, true);

	// Store the panorama texture inside a cubemap.
	cmdBindPipeline(pTaskData->pCmd, pTaskData->pPanoToCubePipeline);
	params[0].pName = "srcTexture";
	params[0].ppTextures = &pTaskData->pPanoSkybox;
	cmdBindDescriptors(pTaskData->pCmd, pTaskData->pPBRDescriptorBinder, pTaskData->pPanoToCubeRootSignature, 1, params);

	struct
	{
		uint32_t mip;
		uint32_t textureSize;
	} rootConstantData = { 0, pTaskData->mSkyboxSize };

	for (uint32_t i = 0; i < pTaskData->mSkyboxMips; ++i)
	{
		rootConstantData.mip = i;
		params[0].pName = "RootConstant";
		params[0].pRootConstant = &rootConstantData;
		params[1].pName = "dstTexture";
		params[1].ppTextures = &pTaskData->pSkybox;
		params[1].mUAVMipSlice = i;
		cmdBindDescriptors(pTaskData->pCmd, pTaskData->pPBRDescriptorBinder, pTaskData->pPanoToCubeRootSignature, 2, params);

		pThreadGroupSize = pTaskData->pPanoToCubeShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(
			pTaskData->pCmd, max(1u, (uint32_t)(rootConstantData.textureSize >> i) / pThreadGroupSize[0]),
			max(1u, (uint32_t)(rootConstantData.textureSize >> i) / pThreadGroupSize[1]), 6);
	}

	TextureBarrier srvBarriers[1] = { { pTaskData->pSkybox, RESOURCE_STATE_SHADER_RESOURCE } };
	cmdResourceBarrier(pTaskData->pCmd, 0, NULL, 1, srvBarriers, false);
	/************************************************************************/
	// Compute sky irradiance
	/************************************************************************/
	params[0] = {};
	params[1] = {};
	cmdBindPipeline(pTaskData->pCmd, pTaskData->pIrradiancePipeline);
	params[0].pName = "srcTexture";
	params[0].ppTextures = &pTaskData->pSkybox;
	params[1].pName = "dstTexture";
	params[1].ppTextures = &pTaskData->pIrradianceMap;
	cmdBindDescriptors(pTaskData->pCmd, pTaskData->pPBRDescriptorBinder, pTaskData->pIrradianceRootSignature, 2, params);
	pThreadGroupSize = pTaskData->pIrradianceShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
	cmdDispatch(pTaskData->pCmd, pTaskData->mIrradianceSize / pThreadGroupSize[0], pTaskData->mIrradianceSize / pThreadGroupSize[1], 6);
	/************************************************************************/
	// Compute specular sky
	/************************************************************************/
	cmdBindPipeline(pTaskData->pCmd, pTaskData->pSpecularPipeline);
	params[0].pName = "srcTexture";
	params[0].ppTextures = &pTaskData->pSkybox;
	cmdBindDescriptors(pTaskData->pCmd, pTaskData->pPBRDescriptorBinder, pTaskData->pSpecularRootSignature, 1, params);

	struct PrecomputeSkySpecularData
	{
		uint  mipSize;
		float roughness;
	};

	for (uint32_t i = 0; i < pTaskData->mSpecularMips; i++)
	{
		PrecomputeSkySpecularData data = {};
		data.roughness = (float)i / (float)(pTaskData->mSpecularMips - 1);
		data.mipSize = pTaskData->mSpecularSize >> i;
		params[0].pName = "RootConstant";
		params[0].pRootConstant = &data;
		params[1].pName = "dstTexture";
		params[1].ppTextures = &pTaskData->pSpecularMap;
		params[1].mUAVMipSlice = i;
		cmdBindDescriptors(pTaskData->pCmd, pTaskData->pPBRDescriptorBinder, pTaskData->pSpecularRootSignature, 2, params);
		pThreadGroupSize = pTaskData->pIrradianceShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(
			pTaskData->pCmd, max(1u, (pTaskData->mSpecularSize >> i) / pThreadGroupSize[0]),
			max(1u, (pTaskData->mSpecularSize >> i) / pThreadGroupSize[1]), 6);
	}
	/************************************************************************/
	/************************************************************************/
	TextureBarrier srvBarriers2[2] = { { pTaskData->pIrradianceMap, RESOURCE_STATE_SHADER_RESOURCE },
									   { pTaskData->pSpecularMap, RESOURCE_STATE_SHADER_RESOURCE } };
	cmdResourceBarrier(pTaskData->pCmd, 0, NULL, 2, srvBarriers2, false);

	endCmd(pTaskData->pCmd);
	waitTokenCompleted(token);
}

void cleanupPBRMapsTaskData(ComputePBRMapsTaskData* pTaskData)
{
	// Remove temporary resources.
	Renderer* pRenderer = pTaskData->pRenderer;

	removePipeline(pRenderer, pTaskData->pSpecularPipeline);
	removeRootSignature(pRenderer, pTaskData->pSpecularRootSignature);
	removeShader(pRenderer, pTaskData->pSpecularShader);
	removePipeline(pRenderer, pTaskData->pIrradiancePipeline);
	removeRootSignature(pRenderer, pTaskData->pIrradianceRootSignature);
	removeShader(pRenderer, pTaskData->pIrradianceShader);
	removePipeline(pRenderer, pTaskData->pBRDFIntegrationPipeline);
	removeRootSignature(pRenderer, pTaskData->pBRDFIntegrationRootSignature);
	removeShader(pRenderer, pTaskData->pBRDFIntegrationShader);
	removePipeline(pRenderer, pTaskData->pPanoToCubePipeline);
	removeRootSignature(pRenderer, pTaskData->pPanoToCubeRootSignature);
	removeShader(pRenderer, pTaskData->pPanoToCubeShader);
	removeDescriptorBinder(pRenderer, pTaskData->pPBRDescriptorBinder);
	removeResource(pTaskData->pPanoSkybox);
	removeSampler(pRenderer, pTaskData->pSkyboxSampler);
}

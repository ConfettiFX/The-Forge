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

struct TextureLoadTaskData
{
	Texture**       mTextures;
	const char**    mNames;
	TextureLoadDesc mDesc;
};
void loadTexturesTask(void* data, uintptr_t i);

enum GPUTaskBehavior
{
	GPU_TASK_BEHAVIOR_SUBMIT_END_WAIT,
	GPU_TASK_BEHAVIOR_SUBMIT_ONLY,
	GPU_TASK_BEHAVIOR_RECORD_ONLY,
};

struct ComputePBRMapsTaskData
{
	Renderer*  pRenderer;
	Queue*     pQueue;
	Cmd*       pCmd;
	Fence*     pFence;
	Semaphore* pSemaphore;

	// Input data
	const char*    mSourceName;
	GPUPresetLevel mPresetLevel;
	uint32_t       mSkyboxSize;
	uint32_t       mSkyboxMips;
	uint32_t       mSpecularSize;
	uint32_t       mSpecularMips;
	uint32_t       mBRDFIntegrationSize;
	uint32_t       mIrradianceSize;

	// Output data
	Texture* pSkybox;
	Texture* pBRDFIntegrationMap;
	Texture* pIrradianceMap;
	Texture* pSpecularMap;

	// Temporary data
	Texture*          pPanoSkybox = NULL;
	Shader*           pPanoToCubeShader = NULL;
	RootSignature*    pPanoToCubeRootSignature = NULL;
	Pipeline*         pPanoToCubePipeline = NULL;
	Shader*           pBRDFIntegrationShader = NULL;
	RootSignature*    pBRDFIntegrationRootSignature = NULL;
	Pipeline*         pBRDFIntegrationPipeline = NULL;
	Shader*           pIrradianceShader = NULL;
	RootSignature*    pIrradianceRootSignature = NULL;
	Pipeline*         pIrradiancePipeline = NULL;
	Shader*           pSpecularShader = NULL;
	RootSignature*    pSpecularRootSignature = NULL;
	Pipeline*         pSpecularPipeline = NULL;
	Sampler*          pSkyboxSampler = NULL;
	DescriptorBinder* pPBRDescriptorBinder = NULL;

};
void computePBRMapsTask(void* data, uintptr_t mode);
void computePBRMaps(ComputePBRMapsTaskData* pTaskData);
void cleanupPBRMapsTaskData(ComputePBRMapsTaskData* pTaskData);

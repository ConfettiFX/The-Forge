/*
* Copyright (c) 2018-2019 Confetti Interactive Inc.
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

// Unit Test to create Bottom and Top Level Acceleration Structures using Raytracing API.

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/DebugRenderer.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/GpuProfiler.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

// Raytracing
#include "../../../../CommonRaytracing_3/Interfaces/IRaytracing.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

// Shader
#include "D3D12/Compiled/RayGen.h"
#include "D3D12/Compiled/ClosestHit.h"
#include "D3D12/Compiled/ClosestHitPlane.h"
#include "D3D12/Compiled/ClosestHitShadow.h"
#include "D3D12/Compiled/Miss.h"
#include "D3D12/Compiled/MissShadow.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

const char* pszBases[FSR_Count] = {
	"",                                     // FSR_BinShaders
	"",                                     // FSR_SrcShaders
	"",                                     // FSR_Textures
	"",                                     // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"",                                     // FSR_GpuConfig
	"",                                     // FSR_Animation
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

class UnitTest_MultipleGeometries: public IApp
{
	public:
	bool Init()
	{
		/************************************************************************/
		// 01 Init Raytracing
		/************************************************************************/
		RendererDesc desc = {};
		initRenderer(GetName(), &desc, &pRenderer);
		initResourceLoaderInterface(pRenderer);
		initRaytracing(pRenderer, &pRaytracing);

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		addQueue(pRenderer, &queueDesc, &pQueue);
		addCmdPool(pRenderer, pQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, gImageCount, &ppCmds);
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}

		addGpuProfiler(pRenderer, pQueue, &pGpuProfiler);

		if (!mAppUI.Init(pRenderer))
			return false;

		mAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);
		/************************************************************************/
		// 02 Creation Acceleration Structure
		/************************************************************************/
		// Create Vertex Buffer
		const float3 vertices[] = {
			float3(0.0f, 1.0f, 0.0f),
			float3(0.866f, -0.5f, 0.0f),
			float3(-0.866f, -0.5f, 0.0f),
		};
		BufferLoadDesc vbDesc = {};
		vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vbDesc.mDesc.mSize = sizeof(vertices);
		vbDesc.mDesc.mVertexStride = sizeof(float3);
		vbDesc.pData = vertices;
		vbDesc.ppBuffer = &pVertexBufferTriangle;
		addResource(&vbDesc);

		const float3 planeVertices[] = {
			float3(-100, -1, -2), float3(100, -1, 100), float3(-100, -1, 100),

			float3(-100, -1, -2), float3(100, -1, -2),  float3(100, -1, 100),
		};
		vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vbDesc.mDesc.mSize = sizeof(planeVertices);
		vbDesc.mDesc.mVertexStride = sizeof(float3);
		vbDesc.pData = planeVertices;
		vbDesc.ppBuffer = &pVertexBufferPlane;
		addResource(&vbDesc);

		// Specify Geometry Used in Raytracing Structure
		AccelerationStructureGeometryDesc geomDescs[2] = {};
		geomDescs[0].mFlags = ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE;
		geomDescs[0].mType = ACCELERATION_STRUCTURE_GEOMETRY_TYPE_TRIANGLES;
		geomDescs[0].pVertexBuffer = pVertexBufferTriangle;

		geomDescs[1].mFlags = ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE;
		geomDescs[1].mType = ACCELERATION_STRUCTURE_GEOMETRY_TYPE_TRIANGLES;
		geomDescs[1].pVertexBuffer = pVertexBufferPlane;

		uint32_t bottomASScratchBufferSize = 0;

		// 08 - Model containing triangle and plane
		AccelerationStructureDesc bottomASDesc = {};
		bottomASDesc.mDescCount = 1;
		bottomASDesc.mFlags = ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		bottomASDesc.mType = ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bottomASDesc.pGeometryDescs = geomDescs;
		addAccelerationStructure(pRaytracing, &bottomASDesc, &bottomASScratchBufferSize, &pBottomLevelASTriangle);

		// 08 - Model containing only triangle
		bottomASDesc = {};
		bottomASDesc.mDescCount = 2;
		bottomASDesc.mFlags = ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		bottomASDesc.mType = ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bottomASDesc.pGeometryDescs = geomDescs;
		addAccelerationStructure(pRaytracing, &bottomASDesc, &bottomASScratchBufferSize, &pBottomLevelASTrianglePlane);

		// Specify Instance Used in Raytracing Structure
		// The transformation matrices for the instances
		mat4 transformation[3];
		transformation[0] = mat4::identity();    // Identity
		transformation[1] = transpose(mat4::translation(vec3(-2, 0, 0)));
		transformation[2] = transpose(mat4::translation(vec3(2, 0, 0)));
		AccelerationStructureInstanceDesc instanceDescs[3] = {};

		instanceDescs[0].mFlags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_NONE;
		instanceDescs[0].mInstanceContributionToHitGroupIndex = 0;
		instanceDescs[0].mInstanceID = 0;
		instanceDescs[0].mInstanceMask = 1;
		memcpy(instanceDescs[0].mTransform, &transformation[0], sizeof(float[12]));
		instanceDescs[0].pAccelerationStructure = pBottomLevelASTrianglePlane;

		for (uint32_t i = 1; i < 3; ++i)
		{
			instanceDescs[i].mFlags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_NONE;
			instanceDescs[i].mInstanceContributionToHitGroupIndex = 1;
			instanceDescs[i].mInstanceID = 0;
			instanceDescs[i].mInstanceMask = 1;
			memcpy(instanceDescs[i].mTransform, &transformation[i], sizeof(float[12]));
			instanceDescs[i].pAccelerationStructure = pBottomLevelASTriangle;
		}

		uint32_t                  topASScratchBufferSize = 0;
		AccelerationStructureDesc topASDesc = {};
		topASDesc.mDescCount = 3;
		topASDesc.mFlags = ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		topASDesc.mType = ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		topASDesc.pInstanceDescs = instanceDescs;
		addAccelerationStructure(pRaytracing, &topASDesc, &topASScratchBufferSize, &pTopLevelAS);

		Buffer*        pScratchBuffer = NULL;
		BufferLoadDesc scratchBufferDesc = {};
		scratchBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
		scratchBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		scratchBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
		scratchBufferDesc.mDesc.mSize = max(bottomASScratchBufferSize, topASScratchBufferSize);
		scratchBufferDesc.ppBuffer = &pScratchBuffer;
		addResource(&scratchBufferDesc);

		// Build Acceleration Structure
		beginCmd(ppCmds[0]);
		cmdBuildAccelerationStructure(ppCmds[0], pRaytracing, pScratchBuffer, pBottomLevelASTriangle);
		cmdBuildAccelerationStructure(ppCmds[0], pRaytracing, pScratchBuffer, pBottomLevelASTrianglePlane);
		cmdBuildAccelerationStructure(ppCmds[0], pRaytracing, pScratchBuffer, pTopLevelAS);
		endCmd(ppCmds[0]);
		queueSubmit(pQueue, 1, &ppCmds[0], pRenderCompleteFences[0], 0, NULL, 0, NULL);
		waitForFences(pQueue, 1, &pRenderCompleteFences[0], false);

		// Safe to remove scratch buffer since the GPU is done using it
		removeResource(pScratchBuffer);
		/************************************************************************/
		// Currently, dxc does not support reflecting raytracing shaders
		// So we hard-code the root signature
		// Once reflection is supported, we will go back to passing shaders for rootsignature creation
		// addRaytracingRootSignature(pRaytracing, ppShaders, 2, &pRootSignature);
		/************************************************************************/
		ShaderResource shaderResources[2] = {};
		shaderResources[0].name = "gOutput";
		shaderResources[0].name_size = (uint32_t)strlen(shaderResources[0].name);
		shaderResources[0].reg = 0;
		shaderResources[0].set = 0;
		shaderResources[0].size = 1;
		shaderResources[0].type = DESCRIPTOR_TYPE_RW_TEXTURE;
		shaderResources[1].name = "gLightDirectionRootConstant";
		shaderResources[1].name_size = (uint32_t)strlen(shaderResources[1].name);
		shaderResources[1].reg = 0;
		shaderResources[1].set = 0;
		shaderResources[1].size = sizeof(float3);
		shaderResources[1].type = DESCRIPTOR_TYPE_ROOT_CONSTANT;
		addRaytracingRootSignature(pRaytracing, shaderResources, 2, false, &pRootSignature);
		/************************************************************************/
		// 03 - Create Raytracing Pipeline
		/************************************************************************/
		const char* pNames[] = { "rayGen", "miss", "chs", "chsPlane", "chsShadow", "missShadow" };
		addRaytracingShader(pRaytracing, gShader_RayGen, sizeof(gShader_RayGen), pNames[0], &pShaderRayGen);
		addRaytracingShader(pRaytracing, gShader_ClosestHit, sizeof(gShader_ClosestHit), pNames[2], &pShaderHit);
		addRaytracingShader(pRaytracing, gShader_ClosestHitPlane, sizeof(gShader_ClosestHitPlane), pNames[3], &pShaderHitPlane);
		addRaytracingShader(pRaytracing, gShader_ClosestHitShadow, sizeof(gShader_ClosestHitShadow), pNames[4], &pShaderHitShadow);
		addRaytracingShader(pRaytracing, gShader_Miss, sizeof(gShader_Miss), pNames[1], &pShaderMiss);
		addRaytracingShader(pRaytracing, gShader_MissShadow, sizeof(gShader_MissShadow), pNames[5], &pShaderMissShadow);

		RaytracingHitGroup hitGroups[3] = {};
		hitGroups[0].pClosestHitShader = pShaderHit;
		hitGroups[0].pHitGroupName = "hitGroup";
		hitGroups[1].pClosestHitShader = pShaderHitPlane;
		hitGroups[1].pHitGroupName = "hitGroupPlane";
		hitGroups[2].pClosestHitShader = pShaderHitShadow;
		hitGroups[2].pHitGroupName = "hitGroupShadow";

		RaytracingShader*      pMissShaders[] = { pShaderMiss, pShaderMissShadow };
		RaytracingPipelineDesc pipelineDesc = {};
		pipelineDesc.mAttributeSize = sizeof(float2);
		pipelineDesc.mMaxTraceRecursionDepth = 2;
		pipelineDesc.mPayloadSize = sizeof(float3);
		pipelineDesc.pGlobalRootSignature = pRootSignature;
		pipelineDesc.pRayGenShader = pShaderRayGen;
		pipelineDesc.ppMissShaders = pMissShaders;
		pipelineDesc.mMissShaderCount = 2;
		pipelineDesc.pHitGroups = hitGroups;
		pipelineDesc.mHitGroupCount = 3;
		addRaytracingPipeline(pRaytracing, &pipelineDesc, &pPipeline);
		/************************************************************************/
		// 04 - Create Shader Binding Table to connect Pipeline with Acceleration Structure
		/************************************************************************/
		RaytracingShaderTableRecordDesc rayGenRecord = { "rayGen" };
		RaytracingShaderTableRecordDesc missRecords[2] = { { "miss" }, { "missShadow" } };
		RaytracingShaderTableRecordDesc hitRecords[] = {
			{ "hitGroup" },          // Triangle 0
			{ "hitGroup" },          // Triangles 1 2
			{ "hitGroupPlane" },     // Plane
			{ "hitGroupShadow" },    // Shadow
		};

		RaytracingShaderTableDesc shaderTableDesc = {};
		shaderTableDesc.pPipeline = pPipeline;
		shaderTableDesc.pRayGenShader = &rayGenRecord;
		shaderTableDesc.mMissShaderCount = 2;
		shaderTableDesc.pMissShaders = missRecords;
		shaderTableDesc.mHitGroupCount = 4;
		shaderTableDesc.pHitGroups = hitRecords;
		addRaytracingShaderTable(pRaytracing, &shaderTableDesc, &pShaderTable);
		/************************************************************************/
		// GUI
		/************************************************************************/
		GuiDesc guiDesc = {};
		guiDesc.mStartSize = vec2(300.0f, 250.0f);
		guiDesc.mStartPosition = vec2(0.0f, guiDesc.mStartSize.getY());
		pGuiWindow = mAppUI.AddGuiComponent(GetName(), &guiDesc);

		pGuiWindow->AddWidget(SliderFloatWidget("Light Direction X", &mLightDirection.x, -2.0f, 2.0f, 0.001f));
		pGuiWindow->AddWidget(SliderFloatWidget("Light Direction Y", &mLightDirection.y, -2.0f, 2.0f, 0.001f));
		pGuiWindow->AddWidget(SliderFloatWidget("Light Direction Z", &mLightDirection.z, -2.0f, 2.0f, 0.001f));
		/************************************************************************/
		/************************************************************************/
		return true;
	}

	void Exit()
	{
		waitForFences(pQueue, 1, &pRenderCompleteFences[mFrameIdx], true);

		removeDebugRendererInterface();

		mAppUI.Exit();

		removeGpuProfiler(pRenderer, pGpuProfiler);

		removeRaytracingShaderTable(pRaytracing, pShaderTable);
		removeRaytracingPipeline(pRaytracing, pPipeline);
		removeRootSignature(pRenderer, pRootSignature);
		removeRaytracingShader(pRaytracing, pShaderRayGen);
		removeRaytracingShader(pRaytracing, pShaderHit);
		removeRaytracingShader(pRaytracing, pShaderHitPlane);
		removeRaytracingShader(pRaytracing, pShaderMiss);
		removeResource(pVertexBufferTriangle);
		removeResource(pVertexBufferPlane);
		removeAccelerationStructure(pRaytracing, pTopLevelAS);
		removeAccelerationStructure(pRaytracing, pBottomLevelASTrianglePlane);
		removeAccelerationStructure(pRaytracing, pBottomLevelASTriangle);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);
		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);
		removeQueue(pQueue);
		removeRaytracing(pRenderer, pRaytracing);
		removeResourceLoaderInterface(pRenderer);
		removeRenderer(pRenderer);
	}

	bool Load()
	{
		/************************************************************************/
		// 04 - Create Output Resources
		/************************************************************************/
		TextureDesc uavDesc = {};
		uavDesc.mArraySize = 1;
		uavDesc.mDepth = 1;
		uavDesc.mFormat = getRecommendedSwapchainFormat(true);
		uavDesc.mHeight = mSettings.mHeight;
		uavDesc.mMipLevels = 1;
		uavDesc.mSampleCount = SAMPLE_COUNT_1;
		uavDesc.mSrgb = false;
		uavDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		uavDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
		uavDesc.mWidth = mSettings.mWidth;
		TextureLoadDesc loadDesc = {};
		loadDesc.pDesc = &uavDesc;
		loadDesc.ppTexture = &pComputeOutput;
		addResource(&loadDesc);

		SwapChainDesc swapChainDesc = {};
		swapChainDesc.mColorClearValue = { 1, 1, 1, 1 };
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mEnableVsync = false;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
		swapChainDesc.mSrgb = false;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.ppPresentQueues = &pQueue;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.pWindow = pWindow;
		addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
		/************************************************************************/
		/************************************************************************/

		if (!mAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

		return true;
	}

	void Unload()
	{
		waitForFences(pQueue, 1, &pRenderCompleteFences[mFrameIdx], true);

		mAppUI.Unload();

		removeSwapChain(pRenderer, pSwapChain);
		removeResource(pComputeOutput);
	}

	void Update(float deltaTime) { mAppUI.Update(deltaTime); }

	void Draw()
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &mFrameIdx);

		FenceStatus fenceStatus = {};
		getFenceStatus(pRenderer, pRenderCompleteFences[mFrameIdx], &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pQueue, 1, &pRenderCompleteFences[mFrameIdx], false);

		Cmd*          pCmd = ppCmds[mFrameIdx];
		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[mFrameIdx];
		beginCmd(pCmd);
		cmdBeginGpuFrameProfile(pCmd, pGpuProfiler, true);
		/************************************************************************/
		// Transition UAV texture so raytracing shader can write to it
		/************************************************************************/
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Raytrace Triangle", true);
		TextureBarrier uavBarrier = { pComputeOutput, RESOURCE_STATE_UNORDERED_ACCESS };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &uavBarrier, false);
		/************************************************************************/
		// Perform raytracing
		/************************************************************************/
		DescriptorData params[2] = {};
		params[0].pName = "gOutput";
		params[0].ppTextures = &pComputeOutput;
		params[1].pName = "gLightDirectionRootConstant";
		params[1].pRootConstant = &mLightDirection;
		cmdBindDescriptors(pCmd, pRootSignature, 2, params);

		RaytracingDispatchDesc dispatchDesc = {};
		dispatchDesc.mHeight = mSettings.mHeight;
		dispatchDesc.mWidth = mSettings.mWidth;
		dispatchDesc.pShaderTable = pShaderTable;
		dispatchDesc.pTopLevelAccelerationStructure = pTopLevelAS;
		cmdDispatchRays(pCmd, pRaytracing, &dispatchDesc);
		/************************************************************************/
		// Transition UAV to be used as source and swapchain as destination in copy operation
		/************************************************************************/
		TextureBarrier copyBarriers[] = {
			{ pComputeOutput, RESOURCE_STATE_COPY_SOURCE },
			{ pRenderTarget->pTexture, RESOURCE_STATE_COPY_DEST },
		};
		cmdResourceBarrier(pCmd, 0, NULL, 2, copyBarriers, false);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		/************************************************************************/
		// Perform copy
		/************************************************************************/
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Copy UAV", true);
		cmdCopyTexture(pCmd, pRenderTarget->pTexture, pComputeOutput);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		/************************************************************************/
		// Present to screen
		/************************************************************************/
		TextureBarrier rtBarrier = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &rtBarrier, false);

		cmdBindRenderTargets(pCmd, 1, &pRenderTarget, NULL, NULL, NULL, NULL, -1, -1);
		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)mSettings.mWidth, (float)mSettings.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, mSettings.mWidth, mSettings.mHeight);
		drawDebugGpuProfile(pCmd, 15.0f, 40.0f, pGpuProfiler, NULL);
		mAppUI.Gui(pGuiWindow);
		mAppUI.Draw(pCmd);

		TextureBarrier presentBarrier = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &presentBarrier, true);

		cmdEndGpuFrameProfile(pCmd, pGpuProfiler);
		endCmd(pCmd);
		queueSubmit(
			pQueue, 1, &pCmd, pRenderCompleteFences[mFrameIdx], 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphores[mFrameIdx]);
		queuePresent(pQueue, pSwapChain, mFrameIdx, 1, &pRenderCompleteSemaphores[mFrameIdx]);
		/************************************************************************/
		/************************************************************************/
	}

	tinystl::string GetName() { return "Raytrace Triangle"; }
	/************************************************************************/
	// Data
	/************************************************************************/
	private:
	static const uint32_t gImageCount = 3;

	Renderer*              pRenderer;
	Raytracing*            pRaytracing;
	Queue*                 pQueue;
	CmdPool*               pCmdPool;
	Cmd**                  ppCmds;
	Fence*                 pRenderCompleteFences[gImageCount];
	Buffer*                pVertexBufferTriangle;
	Buffer*                pVertexBufferPlane;
	AccelerationStructure* pBottomLevelASTriangle;
	AccelerationStructure* pBottomLevelASTrianglePlane;
	AccelerationStructure* pTopLevelAS;
	RaytracingShader*      pShaderRayGen;
	RaytracingShader*      pShaderHit;
	RaytracingShader*      pShaderHitPlane;
	RaytracingShader*      pShaderHitShadow;
	RaytracingShader*      pShaderMiss;
	RaytracingShader*      pShaderMissShadow;
	RootSignature*         pRootSignature;
	RaytracingPipeline*    pPipeline;
	RaytracingShaderTable* pShaderTable;
	SwapChain*             pSwapChain;
	Texture*               pComputeOutput;
	Semaphore*             pRenderCompleteSemaphores[gImageCount];
	Semaphore*             pImageAcquiredSemaphore;
	GpuProfiler*           pGpuProfiler;
	UIApp                  mAppUI;
	uint32_t               mFrameIdx = 0;

	GuiComponent* pGuiWindow;
	float3        mLightDirection = float3(0.5f, 0.5f, -0.5f);
};

DEFINE_APPLICATION_MAIN(UnitTest_MultipleGeometries)
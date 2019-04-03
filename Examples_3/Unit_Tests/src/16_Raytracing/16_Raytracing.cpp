/*
* Copyright (c) 2018 Confetti Interactive Inc.
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
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/GpuProfiler.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

// Raytracing
#include "../../../../Common_3/Renderer/IRay.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"
#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

const char* pszBases[FSR_Count] =
{
	"../../../src/16_Raytracing/",       // FSR_BinShaders
	"../../../src/16_Raytracing/",       // FSR_SrcShaders
	"../../../UnitTestResources/",                  // FSR_Textures
	"../../../UnitTestResources/",                  // FSR_Meshes
	"../../../UnitTestResources/",                  // FSR_Builtin_Fonts
	"../../../src/16_Raytracing/",       // FSR_GpuConfig
	"",                                             // FSR_Animation
	"",                                             // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",            // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",              // FSR_MIDDLEWARE_UI
};

LogManager				gLogManager;
IWidget*				pCameraXWidget = NULL;
IWidget*				pCameraYWidget = NULL;
IWidget*				pCameraZWidget = NULL;
float2					gCameraXLimits(-1.0f, 1.0f);
float2					gCameraYLimits(-1.0f, 2.0f);
float2					gCameraZLimits(-4.0f, 0.0f);
float3					mCameraOrigin = float3(0.0f, 0.0f, -2.0f);

struct ShadersConfigBlock
{
    float3 mCameraPosition;
	uint32_t pad1;
	float3 mLightDirection;
	uint32_t pad2;
};

struct RayPlaneConfigBlock
{
    float3 mLightDirection;
};

class UnitTest_NativeRaytracing : public IApp
{
public:
	UnitTest_NativeRaytracing()
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
	}
	
	bool Init()
	{
		/************************************************************************/
		// 01 Init Raytracing
		/************************************************************************/
		RendererDesc desc = {};
#ifndef DIRECT3D11
		desc.mShaderTarget = shader_target_6_3;
#endif
		initRenderer(GetName(), &desc, &pRenderer);
		initResourceLoaderInterface(pRenderer);

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

		/************************************************************************/
		// GUI
		/************************************************************************/
		GuiDesc guiDesc = {};
		guiDesc.mStartSize = vec2(300.0f, 250.0f);
		guiDesc.mStartPosition = vec2(0.0f, guiDesc.mStartSize.getY());
		pGuiWindow = mAppUI.AddGuiComponent(GetName(), &guiDesc);

        /************************************************************************/
        // Blit texture
        /************************************************************************/
        ShaderLoadDesc displayShader = {};
        displayShader.mStages[0] = { "DisplayTexture.vert", NULL, 0, FSR_SrcShaders };
        displayShader.mStages[1] = { "DisplayTexture.frag", NULL, 0, FSR_SrcShaders };
        addShader(pRenderer, &displayShader, &pDisplayTextureShader);
        
        SamplerDesc samplerDesc = { FILTER_NEAREST,
                                    FILTER_NEAREST,
                                    MIPMAP_MODE_NEAREST,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE };
        addSampler(pRenderer, &samplerDesc, &pSampler);
        
        const char*       pStaticSamplers[] = { "uSampler0" };
        RootSignatureDesc rootDesc = {};
        rootDesc.mStaticSamplerCount = 1;
        rootDesc.ppStaticSamplerNames = pStaticSamplers;
        rootDesc.ppStaticSamplers = &pSampler;
        rootDesc.mShaderCount = 1;
        rootDesc.ppShaders = &pDisplayTextureShader;
        addRootSignature(pRenderer, &rootDesc, &pDisplayTextureSignature);

        RasterizerStateDesc rasterizerStateDesc = {};
        rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
        addRasterizerState(pRenderer, &rasterizerStateDesc, &pRast);

		tinystl::vector<DescriptorBinderDesc> descBinderDesc;
		descBinderDesc.push_back({ pDisplayTextureSignature });
        
		if (!isRaytracingSupported(pRenderer))
		{
			pRaytracing = NULL;
			addDescriptorBinder(pRenderer, 0, (uint32_t)descBinderDesc.size(), descBinderDesc.data(), &pDescriptorBinder);
			pGuiWindow->AddWidget(LabelWidget("Raytracing is NOT SUPPORTED"));
			return true;
		}

		pGuiWindow->AddWidget(SliderFloatWidget("Light Direction X", &mLightDirection.x, -2.0f, 2.0f, 0.001f));
		pGuiWindow->AddWidget(SliderFloatWidget("Light Direction Y", &mLightDirection.y, -2.0f, 2.0f, 0.001f));
		pGuiWindow->AddWidget(SliderFloatWidget("Light Direction Z", &mLightDirection.z, -2.0f, 2.0f, 0.001f));
		pCameraXWidget = pGuiWindow->AddWidget(SliderFloatWidget("Camera Origin X", &mCameraOrigin.x, gCameraXLimits.x, gCameraXLimits.y, 0.001f));
		pCameraYWidget = pGuiWindow->AddWidget(SliderFloatWidget("Camera Origin Y", &mCameraOrigin.y, gCameraYLimits.x, gCameraYLimits.y, 0.001f));
		pCameraZWidget = pGuiWindow->AddWidget(SliderFloatWidget("Camera Origin Z", &mCameraOrigin.z, gCameraZLimits.x, gCameraZLimits.y, 0.001f));
		/************************************************************************/
		/************************************************************************/

		initRaytracing(pRenderer, &pRaytracing);
		/************************************************************************/
		// 02 Creation Acceleration Structure
		/************************************************************************/
        // Create Vertex Buffer
        float3 triangleVertices[] =
        {
            float3(0.0f,    1.0f,  0.0f),
            float3(0.866f,  -0.5f, 0.0f),
            float3(-0.866f, -0.5f, 0.0f),
        };
        float3 planeVertices[] =
        {
            float3(-10, -1,  -2),
            float3(10, -1,  10),
            float3(-10, -1,  10),
            
            float3(-10, -1,  -2),
            float3(10, -1,  -2),
            float3(10, -1,  10),
        };
        
        // Construct descriptions for Bottom Acceleration Structure
        AccelerationStructureGeometryDesc geomDescs[2] = {};
        geomDescs[0].mFlags = ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE;
        geomDescs[0].pVertexArray = &triangleVertices[0];
        geomDescs[0].vertexCount = 3;
        
        geomDescs[1].mFlags = ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE;
        geomDescs[1].pVertexArray = &planeVertices[0];
        geomDescs[1].vertexCount = 6;
        
        // 08 - Model containing plane
        AccelerationStructureDescBottom bottomASDesc[2] = {};
        bottomASDesc[0].mDescCount = 1;
        bottomASDesc[0].mFlags = ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        bottomASDesc[0].pGeometryDescs = &geomDescs[0];
        
        // 08 - Model containing triangle
        bottomASDesc[1].mDescCount = 1;
        bottomASDesc[1].mFlags = ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        bottomASDesc[1].pGeometryDescs = &geomDescs[1];

        AccelerationStructureDescTop topAS = {};
        topAS.mBottomASDescs = &bottomASDesc[0];
        topAS.mBottomASDescsCount = 2;
        
        // The transformation matrices for the instances
        mat4 transformation[3];
        transformation[0] = mat4::identity(); // Identity
        transformation[1] = transpose(mat4::translation(vec3(-2, 0, 0)));
        transformation[2] = transpose(mat4::translation(vec3(2, 0, 0)));
        
        //Construct descriptions for Acceleration Structures Instances
        AccelerationStructureInstanceDesc instanceDescs[3] = {};
        
        instanceDescs[0].mFlags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_NONE;
        instanceDescs[0].mInstanceContributionToHitGroupIndex = 2;
        instanceDescs[0].mInstanceID = 2;
        instanceDescs[0].mInstanceMask = 1;
        memcpy(instanceDescs[0].mTransform, &transformation[0], sizeof(float[12]));
        instanceDescs[0].mAccelerationStructureIndex = 1;
        
        for (uint32_t i = 1; i < 3; ++i)
        {
            instanceDescs[i].mFlags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_NONE;
            instanceDescs[i].mInstanceContributionToHitGroupIndex = i % 1;
            instanceDescs[i].mInstanceID = i - 1; //accessible in shader via InstanceID()
            instanceDescs[i].mInstanceMask = 1;
            memcpy(instanceDescs[i].mTransform, &transformation[i], sizeof(float[12]));
            instanceDescs[i].mAccelerationStructureIndex = 0;
        }
        
        topAS.mInstancesDescCount = 3;
        topAS.pInstanceDescs = &instanceDescs[0];
        addAccelerationStructure(pRaytracing, &topAS, &pTopLevelAS);
        
        // Build Acceleration Structure
		RaytracingBuildASDesc buildASDesc = {};
		unsigned bottomASIndices[] = { 0, 1 };
		buildASDesc.pAccelerationStructure = pTopLevelAS;
		buildASDesc.pBottomASIndices = &bottomASIndices[0];
		buildASDesc.mBottomASIndicesCount = 2;
        beginCmd(ppCmds[0]);
		cmdBuildAccelerationStructure(ppCmds[0], pRaytracing, &buildASDesc);
        endCmd(ppCmds[0]);
        queueSubmit(pQueue, 1, &ppCmds[0], pRenderCompleteFences[0], 0, NULL, 0, NULL);
        waitForFences(pRenderer, 1, &pRenderCompleteFences[0]);

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
        shaderResources[0].used_stages = SHADER_STAGE_COMP;
#ifdef VULKAN
		shaderResources[0].dim = TEXTURE_DIM_2D;
#endif
        shaderResources[1].name = "gSettings";
        shaderResources[1].name_size = (uint32_t)strlen(shaderResources[1].name);
        shaderResources[1].reg = RaytracingUserdataStartBufferRegister;
        shaderResources[1].set = 0;
        shaderResources[1].size = 1;
        shaderResources[1].type = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        shaderResources[1].used_stages = SHADER_STAGE_COMP;
		RootSignatureDesc signatureDesc = {};
		signatureDesc.mSignatureType = ROOT_SIGNATURE_RAYTRACING_GLOBAL;
		signatureDesc.pRaytracingShaderResources = &shaderResources[0];
		signatureDesc.pRaytracingResourcesCount = 2;
		addRootSignature(pRenderer, &signatureDesc, &pRootSignature);

		//Empty root signature for shaders we don't provide one
		RootSignatureDesc emptySignatureDesc = {};
		emptySignatureDesc.mSignatureType = ROOT_SIGNATURE_RAYTRACING_EMPTY;
		addRootSignature(pRenderer, &emptySignatureDesc, &pEmptyRootSignature);
        /************************************************************************/
        // 03 - Create Raytracing Pipeline
        /************************************************************************/
        {
            ShaderLoadDesc desc = {};
            desc.mStages[0] = { "RayGen.rgen", NULL, 0, FSR_SrcShaders, "rayGen"};
#ifndef DIRECT3D11
            desc.mTarget = shader_target_6_3;
#endif
            addShader(pRenderer, &desc, &pShaderRayGen);
            
            desc.mStages[0] = { "ClosestHit.rchit", NULL, 0, FSR_SrcShaders, "chs"};
            addShader(pRenderer, &desc, &pShaderHitTriangle);
            
            desc.mStages[0] = { "ClosestHitPlane.rchit", NULL, 0, FSR_SrcShaders, "chsPlane"};
            addShader(pRenderer, &desc, &pShaderHitPlane);
            
            desc.mStages[0] = { "ClosestHitShadow.rchit", NULL, 0, FSR_SrcShaders, "chsShadow"};
            addShader(pRenderer, &desc, &pShaderHitShadow);
            
            desc.mStages[0] = { "Miss.rmiss", NULL, 0, FSR_SrcShaders, "miss"};
            addShader(pRenderer, &desc, &pShaderMiss);
            
            desc.mStages[0] = { "MissShadow.rmiss", NULL, 0, FSR_SrcShaders, "missShadow"};
            addShader(pRenderer, &desc, &pShaderMissShadow);
        }

        /************************************************************************/
        
        RaytracingHitGroup hitGroups[3] = {};
        hitGroups[0].pClosestHitShader    = pShaderHitTriangle;
        hitGroups[0].pHitGroupName        = "hitGroup";
        
        hitGroups[1].pClosestHitShader    = pShaderHitPlane;
        hitGroups[1].pHitGroupName        = "hitGroupPlane";
        
        hitGroups[2].pClosestHitShader    = pShaderHitShadow;
        hitGroups[2].pHitGroupName        = "hitGroupShadow";
        
        Shader* pMissShaders[] = { pShaderMiss, pShaderMissShadow };
		PipelineDesc rtPipelineDesc = {};
		rtPipelineDesc.mType = PIPELINE_TYPE_RAYTRACING;
        RaytracingPipelineDesc& pipelineDesc = rtPipelineDesc.mRaytracingDesc;
        pipelineDesc.mAttributeSize			= sizeof(float2);
        pipelineDesc.mMaxTraceRecursionDepth = 2;
        pipelineDesc.mPayloadSize			= sizeof(float3);
        pipelineDesc.pGlobalRootSignature	= pRootSignature;
        pipelineDesc.pRayGenShader			= pShaderRayGen;
		pipelineDesc.pRayGenRootSignature	= nullptr;// pRayGenSignature; //nullptr to bind empty LRS
		pipelineDesc.pEmptyRootSignature	= pEmptyRootSignature;
        pipelineDesc.ppMissShaders			= pMissShaders;
        pipelineDesc.mMissShaderCount		= 2;
        pipelineDesc.pHitGroups				= hitGroups;
        pipelineDesc.mHitGroupCount			= 3;
		pipelineDesc.pRaytracing			= pRaytracing;
#ifdef METAL
        //use screen resolution on iOS device
        pipelineDesc.mMaxRaysCount = mSettings.mHeight * mSettings.mWidth;
#else
        pipelineDesc.mMaxRaysCount = 1920 * 1080; //1 ray per pixel in FHD resolution
#endif
        addPipeline(pRenderer, &rtPipelineDesc, &pPipeline);
        /************************************************************************/
        // 04 - Create Shader Binding Table to connect Pipeline with Acceleration Structure
        /************************************************************************/
		{
			// Update uniform buffers
			ShadersConfigBlock configBlock;
			configBlock.mCameraPosition = mCameraOrigin;
			configBlock.mLightDirection = mLightDirection;

			BufferLoadDesc ubDesc = {};
			ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			ubDesc.mDesc.mSize = sizeof(ShadersConfigBlock);
			ubDesc.pData = &configBlock;
			for (uint32_t i = 0; i < gImageCount; i++)
			{
				ubDesc.ppBuffer = &pRayGenConfigBuffer[i];
				addResource(&ubDesc);
			}
		}
        RaytracingShaderTableRecordDesc rayGenRecord = { "rayGen" };
        rayGenRecord.mInvokeTraceRay = true;
        rayGenRecord.mHitShaderIndex = 0;
        rayGenRecord.mMissShaderIndex = 0;
        
        RaytracingShaderTableRecordDesc missRecords[2] = { { "miss" },{ "missShadow" } };
        RaytracingShaderTableRecordDesc hitRecords[] = {
            { "hitGroup" },         // Triangle 0
            { "hitGroup" },         // Triangle 1
            { "hitGroupPlane" },    // Plane
            { "hitGroupShadow" },   // Shadow
        };

        {
            // Update uniform buffers
            RayPlaneConfigBlock configBlock;
            configBlock.mLightDirection = mLightDirection;
            
            BufferLoadDesc ubDesc = {};
            ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
            ubDesc.mDesc.mSize = sizeof(RayPlaneConfigBlock);
            //ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
            ubDesc.pData = &configBlock;
			for(uint32_t i = 0 ; i < gImageCount; i++)
			{
				ubDesc.ppBuffer = &pHitPlaneConfigBuffer[i];
				addResource(&ubDesc);
			}
        }
        hitRecords[2].mInvokeTraceRay = true;
        hitRecords[2].mHitShaderIndex = 3; //hitGroupShadow
        hitRecords[2].mMissShaderIndex = 1; //missShadow

		descBinderDesc.push_back({ pRootSignature });

		addDescriptorBinder(pRenderer, 0, (uint32_t)descBinderDesc.size(), descBinderDesc.data(), &pDescriptorBinder);

        RaytracingShaderTableDesc shaderTableDesc = {};
        shaderTableDesc.pPipeline = pPipeline;
		shaderTableDesc.pEmptyRootSignature = pEmptyRootSignature;
        shaderTableDesc.pRayGenShader = &rayGenRecord;
        shaderTableDesc.mMissShaderCount = 2;
        shaderTableDesc.pMissShaders = missRecords;
        shaderTableDesc.mHitGroupCount = 4;
        shaderTableDesc.pHitGroups = hitRecords;
		shaderTableDesc.pDescriptorBinder = pDescriptorBinder;
        addRaytracingShaderTable(pRaytracing, &shaderTableDesc, &pShaderTable);

		return true;
	}

	void Exit()
	{
		waitQueueIdle(pQueue);

		mAppUI.Exit();

		removeGpuProfiler(pRenderer, pGpuProfiler);

		if (pRaytracing != NULL)
		{
			removeRaytracingShaderTable(pRaytracing, pShaderTable);
			removePipeline(pRenderer, pPipeline);
			removeRootSignature(pRenderer, pRootSignature);
			removeRootSignature(pRenderer, pEmptyRootSignature);
			for(uint32_t i = 0 ; i < gImageCount; i++)
			{
				removeResource(pRayGenConfigBuffer[i]);
				removeResource(pHitPlaneConfigBuffer[i]);
			}
			removeShader(pRenderer, pShaderRayGen);
			removeShader(pRenderer, pShaderHitTriangle);
			removeShader(pRenderer, pShaderHitPlane);
			removeShader(pRenderer, pShaderMiss);
			removeShader(pRenderer, pShaderHitShadow);
			removeShader(pRenderer, pShaderMissShadow);
			removeAccelerationStructure(pRaytracing, pTopLevelAS);
			removeRaytracing(pRenderer, pRaytracing);
		}
		removeDescriptorBinder(pRenderer, pDescriptorBinder);

		removeSampler(pRenderer, pSampler);
		removeRasterizerState(pRast);
		removeShader(pRenderer, pDisplayTextureShader);
		removeRootSignature(pRenderer, pDisplayTextureSignature);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);
		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);
		removeQueue(pQueue);
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
		uavDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;// RESOURCE_STATE_UNORDERED_ACCESS;
		uavDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		uavDesc.mWidth = mSettings.mWidth;
		TextureLoadDesc loadDesc = {};
		loadDesc.pDesc = &uavDesc;
		for(uint i = 0 ; i < gImageCount; i++)
		{
			loadDesc.ppTexture = &pComputeOutput[i];
			addResource(&loadDesc);
		}
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
        
        VertexLayout vertexLayout = {};
        vertexLayout.mAttribCount = 0;
		PipelineDesc graphicsPipelineDesc = {};
		graphicsPipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineSettings = graphicsPipelineDesc.mGraphicsDesc;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.pRasterizerState = pRast;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
        pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
        pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
        pipelineSettings.pVertexLayout = &vertexLayout;
        pipelineSettings.pRootSignature = pDisplayTextureSignature;
        pipelineSettings.pShaderProgram = pDisplayTextureShader;
        addPipeline(pRenderer, &graphicsPipelineDesc, &pDisplayTexturePipeline);
		/************************************************************************/
		/************************************************************************/

		if (!mAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pQueue);

		mAppUI.Unload();

		removePipeline(pRenderer, pDisplayTexturePipeline);
		removeSwapChain(pRenderer, pSwapChain);
		for(uint i = 0; i < gImageCount; i++)
			removeResource(pComputeOutput[i]);
	}

	void Update(float deltaTime)
	{
		mAppUI.Update(deltaTime);
	}

	void Draw()
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &mFrameIdx);
		

		FenceStatus fenceStatus = {};
		getFenceStatus(pRenderer, pRenderCompleteFences[mFrameIdx], &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFences[mFrameIdx]);

		if (pRaytracing != NULL)
		{
			RayPlaneConfigBlock cb;
			cb.mLightDirection = mLightDirection;
			
			BufferUpdateDesc bufferUpdate;
			bufferUpdate.pBuffer = pHitPlaneConfigBuffer[mFrameIdx];
			bufferUpdate.pData = &cb;
			bufferUpdate.mSize = sizeof(cb);
			updateResource(&bufferUpdate);
		}
		if (pRaytracing != NULL)
		{
			ShadersConfigBlock cb;
			cb.mCameraPosition = mCameraOrigin;
			cb.mLightDirection = mLightDirection;
			
			BufferUpdateDesc bufferUpdate;
			bufferUpdate.pBuffer = pRayGenConfigBuffer[mFrameIdx];
			bufferUpdate.pData = &cb;
			bufferUpdate.mSize = sizeof(cb);
			updateResource(&bufferUpdate);
		}
		
		
		Cmd* pCmd = ppCmds[mFrameIdx];
		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[mFrameIdx];
		beginCmd(pCmd);
		cmdBeginGpuFrameProfile(pCmd, pGpuProfiler, true);
		/************************************************************************/
		// Transition UAV texture so raytracing shader can write to it
		/************************************************************************/
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Raytrace Triangle", true);
		TextureBarrier uavBarrier = { pComputeOutput[mFrameIdx], RESOURCE_STATE_UNORDERED_ACCESS };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &uavBarrier, false);
		/************************************************************************/
		// Perform raytracing
		/************************************************************************/
		if (pRaytracing != NULL)
		{
			DescriptorData params[2] = {};
			params[0].pName = "gOutput";
			params[0].ppTextures = &pComputeOutput[mFrameIdx];
			params[1].pName = "gSettings";
			params[1].ppBuffers = &pRayGenConfigBuffer[mFrameIdx];

			RaytracingDispatchDesc dispatchDesc = {};
			dispatchDesc.mHeight = mSettings.mHeight;
			dispatchDesc.mWidth = mSettings.mWidth;
			dispatchDesc.pShaderTable = pShaderTable;
			dispatchDesc.pTopLevelAccelerationStructure = pTopLevelAS;
			dispatchDesc.pRootSignatureDescriptorData = &params[0];
			dispatchDesc.mRootSignatureDescriptorsCount = 2;
			dispatchDesc.pPipeline = pPipeline;
			dispatchDesc.pRootSignature = pRootSignature;
			dispatchDesc.pDescriptorBinder = pDescriptorBinder;
			cmdDispatchRays(pCmd, pRaytracing, &dispatchDesc);
		}
		/************************************************************************/
		// Transition UAV to be used as source and swapchain as destination in copy operation
		/************************************************************************/
		TextureBarrier copyBarriers[] = {
			{ pComputeOutput[mFrameIdx], RESOURCE_STATE_SHADER_RESOURCE },
			{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(pCmd, 0, NULL, 2, copyBarriers, false);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		/************************************************************************/
		// Present to screen
		/************************************************************************/
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 0.0f;
		loadActions.mClearColorValues[0].g = 0.0f;
		loadActions.mClearColorValues[0].b = 0.0f;
		loadActions.mClearColorValues[0].a = 1.0f;
		cmdBindRenderTargets(pCmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)mSettings.mWidth, (float)mSettings.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, mSettings.mWidth, mSettings.mHeight);
        
		if(pRaytracing != NULL)
		{
			/************************************************************************/
			// Perform copy
			/************************************************************************/
			cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Render result", true);
			// Draw computed results
			DescriptorData params[1] = {};
			cmdBindPipeline(pCmd, pDisplayTexturePipeline);
			params[0].pName = "uTex0";
			params[0].ppTextures = &pComputeOutput[mFrameIdx];
			cmdBindDescriptors(pCmd, pDescriptorBinder, pDisplayTextureSignature, 1, params);
			cmdDraw(pCmd, 3, 0);
			cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
        }
		mAppUI.Gui(pGuiWindow);
		mAppUI.Draw(pCmd);

		TextureBarrier presentBarrier = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &presentBarrier, true);

		cmdEndGpuFrameProfile(pCmd, pGpuProfiler);
		
		endCmd(pCmd);
		queueSubmit(pQueue, 1, &pCmd, pRenderCompleteFences[mFrameIdx], 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphores[mFrameIdx]);
		queuePresent(pQueue, pSwapChain, mFrameIdx, 1, &pRenderCompleteSemaphores[mFrameIdx]);
		/************************************************************************/
		/************************************************************************/
	}

	tinystl::string GetName()
	{
		return "Raytrace Triangles!";
	}

	/************************************************************************/
	// Data
	/************************************************************************/
private:
	static const uint32_t   gImageCount = 3;

	Renderer*			   pRenderer;
	Raytracing*			 pRaytracing;
	Queue*				  pQueue;
	CmdPool*				pCmdPool;
	Cmd**				   ppCmds;
	Fence*				  pRenderCompleteFences[gImageCount];
	Buffer*				 pVertexBufferTriangle;
	Buffer*				 pVertexBufferPlane;
	Buffer*				 pRayGenConfigBuffer[gImageCount];
	Buffer*				 pHitPlaneConfigBuffer[gImageCount];
	AccelerationStructure*  pBottomLevelASTriangle;
	AccelerationStructure*  pBottomLevelASPlane;
	AccelerationStructure*  pTopLevelAS;
	Shader*	   pShaderRayGen;
	Shader*	   pShaderHitTriangle;
	Shader*	   pShaderHitPlane;
	Shader*	   pShaderHitShadow;
	Shader*	   pShaderMiss;
	Shader*	   pShaderMissShadow;
    RasterizerState*        pRast;
    Shader*                 pDisplayTextureShader;
    Sampler*                pSampler;
	RootSignature*			pRootSignature;
	RootSignature*			pEmptyRootSignature;
    RootSignature*          pDisplayTextureSignature;
	DescriptorBinder*       pDescriptorBinder;
	Pipeline*				pPipeline;
    Pipeline*               pDisplayTexturePipeline;
	RaytracingShaderTable*  pShaderTable;
	SwapChain*				pSwapChain;
	Texture*				pComputeOutput[gImageCount];
	Semaphore*				pRenderCompleteSemaphores[gImageCount];
	Semaphore*				pImageAcquiredSemaphore;
	GpuProfiler*			pGpuProfiler;
	UIApp					mAppUI;
	uint32_t				mFrameIdx = 0;
	GuiComponent*			pGuiWindow;
	float3					mLightDirection = float3(0.5f, 0.5f, -0.5f);
};

DEFINE_APPLICATION_MAIN(UnitTest_NativeRaytracing)

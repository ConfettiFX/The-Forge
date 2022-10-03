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
#include "GraphicsConfig.h"
#include "../Utilities/Interfaces/ILog.h"
#include "Interfaces/IGraphics.h"
#include "Interfaces/IRay.h"
#include "GPUConfig.h"


// API functions
addFenceFn						addFence;
removeFenceFn					removeFence;
addSemaphoreFn					addSemaphore;
removeSemaphoreFn				removeSemaphore;
addQueueFn						addQueue;
removeQueueFn					removeQueue;
addSwapChainFn					addSwapChain;
removeSwapChainFn				removeSwapChain;

// command pool functions
addCmdPoolFn					addCmdPool;
removeCmdPoolFn					removeCmdPool;
addCmdFn						addCmd;
removeCmdFn						removeCmd;
addCmd_nFn						addCmd_n;
removeCmd_nFn					removeCmd_n;

addRenderTargetFn				addRenderTarget;
removeRenderTargetFn			removeRenderTarget;
addSamplerFn					addSampler;
removeSamplerFn					removeSampler;

// shader functions
#if defined(TARGET_IOS)
addIosShaderFn					addIosShader;
#endif
addShaderBinaryFn				addShaderBinary;
removeShaderFn					removeShader;

addRootSignatureFn				addRootSignature;
removeRootSignatureFn			removeRootSignature;

// pipeline functions
addPipelineFn					addPipeline;
removePipelineFn				removePipeline;
addPipelineCacheFn				addPipelineCache;
getPipelineCacheDataFn			getPipelineCacheData;
removePipelineCacheFn			removePipelineCache;

// Descriptor Set functions
addDescriptorSetFn				addDescriptorSet;
removeDescriptorSetFn			removeDescriptorSet;
updateDescriptorSetFn			updateDescriptorSet;

// command buffer functions
resetCmdPoolFn					resetCmdPool;
beginCmdFn						beginCmd;
endCmdFn						endCmd;
cmdBindRenderTargetsFn			cmdBindRenderTargets;
cmdSetShadingRateFn				cmdSetShadingRate;
cmdSetViewportFn				cmdSetViewport;
cmdSetScissorFn					cmdSetScissor;
cmdSetStencilReferenceValueFn	cmdSetStencilReferenceValue;
cmdBindPipelineFn				cmdBindPipeline;
cmdBindDescriptorSetFn			cmdBindDescriptorSet;
cmdBindPushConstantsFn			cmdBindPushConstants;
cmdBindDescriptorSetWithRootCbvsFn	cmdBindDescriptorSetWithRootCbvs;
cmdBindIndexBufferFn			cmdBindIndexBuffer;
cmdBindVertexBufferFn			cmdBindVertexBuffer;
cmdDrawFn						cmdDraw;
cmdDrawInstancedFn				cmdDrawInstanced;
cmdDrawIndexedFn				cmdDrawIndexed;
cmdDrawIndexedInstancedFn		cmdDrawIndexedInstanced;
cmdDispatchFn					cmdDispatch;

// Transition Commands
cmdResourceBarrierFn			cmdResourceBarrier;
// Virtual Textures
cmdUpdateVirtualTextureFn		cmdUpdateVirtualTexture;

// queue/fence/swapchain functions
acquireNextImageFn				acquireNextImage;
queueSubmitFn					queueSubmit;
queuePresentFn					queuePresent;
waitQueueIdleFn					waitQueueIdle;
getFenceStatusFn				getFenceStatus;
waitForFencesFn					waitForFences;
toggleVSyncFn					toggleVSync;

getRecommendedSwapchainFormatFn getRecommendedSwapchainFormat;

//indirect Draw functions
addIndirectCommandSignatureFn	addIndirectCommandSignature;
removeIndirectCommandSignatureFn removeIndirectCommandSignature;
cmdExecuteIndirectFn			cmdExecuteIndirect;

/************************************************************************/
// GPU Query Interface
/************************************************************************/
getTimestampFrequencyFn			getTimestampFrequency;
addQueryPoolFn					addQueryPool;
removeQueryPoolFn				removeQueryPool;
cmdResetQueryPoolFn				cmdResetQueryPool;
cmdBeginQueryFn					cmdBeginQuery;
cmdEndQueryFn					cmdEndQuery;
cmdResolveQueryFn				cmdResolveQuery;
/************************************************************************/
// Stats Info Interface
/************************************************************************/
calculateMemoryStatsFn			calculateMemoryStats;
calculateMemoryUseFn			calculateMemoryUse;
freeMemoryStatsFn				freeMemoryStats;
/************************************************************************/
// Debug Marker Interface
/************************************************************************/
cmdBeginDebugMarkerFn			cmdBeginDebugMarker;
cmdEndDebugMarkerFn				cmdEndDebugMarker;
cmdAddDebugMarkerFn				cmdAddDebugMarker;
cmdWriteMarkerFn				cmdWriteMarker;
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
setBufferNameFn					setBufferName;
setTextureNameFn				setTextureName;
setRenderTargetNameFn			setRenderTargetName;
setPipelineNameFn				setPipelineName;

/************************************************************************/
// IRay Interface
/************************************************************************/
isRaytracingSupportedFn			isRaytracingSupported;
initRaytracingFn				initRaytracing;
removeRaytracingFn				removeRaytracing;
addAccelerationStructureFn		addAccelerationStructure;
removeAccelerationStructureFn	removeAccelerationStructure;
removeAccelerationStructureScratchFn	removeAccelerationStructureScratch;
addRaytracingShaderTableFn		addRaytracingShaderTable;
removeRaytracingShaderTableFn	removeRaytracingShaderTable;

cmdBuildAccelerationStructureFn	cmdBuildAccelerationStructure;
cmdDispatchRaysFn				cmdDispatchRays;

#if defined(METAL)
addSSVGFDenoiserFn					addSSVGFDenoiser;
removeSSVGFDenoiserFn				removeSSVGFDenoiser;
clearSSVGFDenoiserTemporalHistoryFn	clearSSVGFDenoiserTemporalHistory;
cmdSSVGFDenoiseFn					cmdSSVGFDenoise;
#endif

/************************************************************************/
// Internal Resource Load Functions
/************************************************************************/
#define DECLARE_INTERNAL_RENDERER_FUNCTION(ret,name,...) \
typedef ret(FORGE_CALLCONV *name##Fn)(__VA_ARGS__); \
name##Fn name; \

DECLARE_INTERNAL_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, mapBuffer, Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, unmapBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, cmdUpdateBuffer, Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, cmdUpdateSubresource, Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, cmdCopySubresource, Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, addTexture, Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, removeTexture, Renderer* pRenderer, Texture* pTexture)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, addVirtualTexture, Cmd* pCmd, const TextureDesc* pDesc, Texture** ppTexture, void* pImageData)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, removeVirtualTexture, Renderer* pRenderer, VirtualTexture* pTexture)

/************************************************************************/
// Internal initialization settings
/************************************************************************/
RendererApi gSelectedRendererApi;
bool        gD3D11Unsupported = false; 
bool        gGLESUnsupported = false; 

/************************************************************************/
// Internal initialization functions
/************************************************************************/
#if defined(DIRECT3D11)
extern void initD3D11Renderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
extern void initD3D11RaytracingFunctions();
extern void exitD3D11Renderer(Renderer* pRenderer); 
#endif

#if defined(DIRECT3D12)
extern void initD3D12Renderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
extern void initD3D12RaytracingFunctions();
extern void exitD3D12Renderer(Renderer* pRenderer);
extern void initD3D12RendererContext(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext);
extern void exitD3D12RendererContext(RendererContext* pContext);
#endif

#if defined(VULKAN)
extern void initVulkanRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
extern void initVulkanRaytracingFunctions();
extern void exitVulkanRenderer(Renderer* pRenderer);
extern void initVulkanRendererContext(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext);
extern void exitVulkanRendererContext(RendererContext* pContext);
#endif

#if defined(METAL)
extern void initMetalRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
extern void initMetalRaytracingFunctions();
extern void exitMetalRenderer(Renderer* pRenderer);
extern void initMetalRendererContext(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext);
extern void exitMetalRendererContext(RendererContext* pContext);
#endif

#if defined(GLES)
extern void initGLESRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
extern void exitGLESRenderer(Renderer* pRenderer);
#endif

#if defined(ORBIS)
extern void initOrbisRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
extern void exitOrbisRenderer(Renderer* pRenderer);
#endif

#if defined(PROSPERO)
extern void initProsperoRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
extern void exitProsperoRenderer(Renderer* pRenderer);
#endif

static bool apiIsUnsupported(const RendererApi api)
{
#if defined(GLES)
	if (api == RENDERER_API_GLES && gGLESUnsupported)
		return true;
#endif

#if defined(DIRECT3D11)
	if (api == RENDERER_API_D3D11 && gD3D11Unsupported)
		return true;
#endif

	return false; 
}

static void initRendererAPI(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer, const RendererApi api)
{
	switch (api)
	{
#if defined(DIRECT3D11)
	case RENDERER_API_D3D11:
		initD3D11RaytracingFunctions();
		initD3D11Renderer(appName, pSettings, ppRenderer);
		break;
#endif
#if defined(DIRECT3D12)
	case RENDERER_API_D3D12:
		initD3D12RaytracingFunctions();
		initD3D12Renderer(appName, pSettings, ppRenderer);
		break;
#endif
#if defined(VULKAN)
	case RENDERER_API_VULKAN:
		initVulkanRaytracingFunctions();
		initVulkanRenderer(appName, pSettings, ppRenderer);
		break;
#endif
#if defined(METAL)
	case RENDERER_API_METAL:
		initMetalRaytracingFunctions();
		initMetalRenderer(appName, pSettings, ppRenderer);
		break;
#endif
#if defined(GLES)
	case RENDERER_API_GLES:
		initGLESRenderer(appName, pSettings, ppRenderer);
		break;
#endif
#if defined(ORBIS)
	case RENDERER_API_ORBIS:
		initOrbisRenderer(appName, pSettings, ppRenderer);
		break;
#endif
#if defined(PROSPERO)
	case RENDERER_API_PROSPERO:
		initProsperoRenderer(appName, pSettings, ppRenderer);
		break;
#endif
	default:
		LOGF(LogLevel::eERROR, "No Renderer API defined!");
		break;
	}
}

static void exitRendererAPI(Renderer* pRenderer, const RendererApi api)
{
	switch (api)
	{
#if defined(DIRECT3D11)
	case RENDERER_API_D3D11:
		exitD3D11Renderer(pRenderer);
		break;
#endif
#if defined(DIRECT3D12)
	case RENDERER_API_D3D12:
		exitD3D12Renderer(pRenderer);
		break;
#endif
#if defined(VULKAN)
	case RENDERER_API_VULKAN:
		exitVulkanRenderer(pRenderer);
		break;
#endif
#if defined(METAL)
	case RENDERER_API_METAL:
		exitMetalRenderer(pRenderer);
		break;
#endif
#if defined(GLES)
	case RENDERER_API_GLES:
		exitGLESRenderer(pRenderer);
		break;
#endif
#if defined(ORBIS)
	case RENDERER_API_ORBIS:
		exitOrbisRenderer(pRenderer);
		break;
#endif
#if defined(PROSPERO)
	case RENDERER_API_PROSPERO:
		exitProsperoRenderer(pRenderer);
		break;
#endif
	default:
		LOGF(LogLevel::eERROR, "No Renderer API defined!");
		break;
	}
}

static void initRendererContextAPI(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext, const RendererApi api)
{
	switch (api)
	{
#if defined(DIRECT3D12)
	case RENDERER_API_D3D12:
		initD3D12RendererContext(appName, pSettings, ppContext);
		break;
#endif
#if defined(VULKAN)
	case RENDERER_API_VULKAN:
		initVulkanRendererContext(appName, pSettings, ppContext);
		break;
#endif
#if defined(METAL)
	case RENDERER_API_METAL:
		initMetalRendererContext(appName, pSettings, ppContext);
		break;
#endif
#if defined(DIRECT3D11)
	case RENDERER_API_D3D11:
#endif
	default:
		LOGF(LogLevel::eERROR, "No Renderer API defined!");
		break;
	}
}

static void exitRendererContextAPI(RendererContext* pContext, const RendererApi api)
{
	switch (api)
	{
#if defined(DIRECT3D12)
	case RENDERER_API_D3D12:
		exitD3D12RendererContext(pContext);
		break;
#endif
#if defined(VULKAN)
	case RENDERER_API_VULKAN:
		exitVulkanRendererContext(pContext);
		break;
#endif
#if defined(METAL)
	case RENDERER_API_METAL:
		exitMetalRendererContext(pContext);
		break;
#endif
#if defined(DIRECT3D11)
	case RENDERER_API_D3D11:
#endif
	default:
		LOGF(LogLevel::eERROR, "No Renderer API defined!");
		break;
	}
}

void initRendererContext(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext)
{
	ASSERT(ppContext);
	ASSERT(*ppContext == NULL);

	ASSERT(pSettings);

	gD3D11Unsupported = true;
	gGLESUnsupported = true;

	// Init requested renderer API
	if (!apiIsUnsupported(gSelectedRendererApi))
	{
		initRendererContextAPI(appName, pSettings, ppContext, gSelectedRendererApi);
	}
	else
	{
		LOGF(LogLevel::eWARNING, "Requested Graphics API has been marked as disabled and/or not supported in the Renderer's descriptor!");
		LOGF(LogLevel::eWARNING, "Falling back to the first available API...");
	}

#if defined(USE_MULTIPLE_RENDER_APIS)
	// Fallback on other available APIs
	for (uint32_t i = 0; i < RENDERER_API_COUNT && !*ppContext; ++i)
	{
		if (i == gSelectedRendererApi || apiIsUnsupported((RendererApi)i))
			continue;

		gSelectedRendererApi = (RendererApi)i;
		initRendererContextAPI(appName, pSettings, ppContext, gSelectedRendererApi);
	}
#endif
}

void exitRendererContext(RendererContext* pContext)
{
	ASSERT(pContext);

	exitRendererContextAPI(pContext, gSelectedRendererApi);
}

void initRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer)
{
	ASSERT(ppRenderer);
	ASSERT(*ppRenderer == NULL);

	ASSERT(pSettings);

	gD3D11Unsupported = !pSettings->mD3D11Supported;
	gGLESUnsupported = !pSettings->mGLESSupported;

	// Init requested renderer API
	if (!apiIsUnsupported(gSelectedRendererApi))
	{
		initRendererAPI(appName, pSettings, ppRenderer, gSelectedRendererApi);
	}
	else
	{
		LOGF(LogLevel::eWARNING, "Requested Graphics API has been marked as disabled and/or not supported in the Renderer's descriptor!");
		LOGF(LogLevel::eWARNING, "Falling back to the first available API...");
	}

#if defined(USE_MULTIPLE_RENDER_APIS)
	// Fallback on other available APIs
	for (uint32_t i = 0; i < RENDERER_API_COUNT && !*ppRenderer; ++i)
	{
		if (i == gSelectedRendererApi || apiIsUnsupported((RendererApi)i))
			continue;

		gSelectedRendererApi = (RendererApi)i;
		initRendererAPI(appName, pSettings, ppRenderer, gSelectedRendererApi);
	}
#endif

	if (pSettings->pExtendedSettings && *ppRenderer)
		setExtendedSettings(pSettings->pExtendedSettings, (*ppRenderer)->pActiveGpuSettings);
}

void exitRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);

	exitRendererAPI(pRenderer, gSelectedRendererApi);

	gD3D11Unsupported = false;
	gGLESUnsupported = false;
}

uint32_t getDescriptorIndexFromName(const RootSignature* pRootSignature, const char* pName)
{
	for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
	{
		if (!strcmp(pName, pRootSignature->pDescriptors[i].pName))
		{
			return i;
		}
	}

	return UINT32_MAX;
}

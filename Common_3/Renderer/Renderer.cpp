#include "../OS/Interfaces/ILog.h"
#include "IRenderer.h"
#include "IRay.h"

// API functions
exitRendererFn					exitRenderer;
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
cmdBindPushConstantsByIndexFn	cmdBindPushConstantsByIndex;
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
typedef API_INTERFACE ret(FORGE_CALLCONV *name##Fn)(__VA_ARGS__); \
name##Fn name; \

DECLARE_INTERNAL_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, mapBuffer, Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, unmapBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, cmdUpdateBuffer, Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, cmdUpdateSubresource, Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, addTexture, Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, removeTexture, Renderer* pRenderer, Texture* pTexture)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, addVirtualTexture, Cmd* pCmd, const TextureDesc* pDesc, Texture** ppTexture, void* pImageData)
DECLARE_INTERNAL_RENDERER_FUNCTION(void, removeVirtualTexture, Renderer* pRenderer, VirtualTexture* pTexture)

/************************************************************************/
// Internal initialization settings
/************************************************************************/
RendererApi gSelectedRendererApi;

/************************************************************************/
// Internal initialization functions
/************************************************************************/
#if defined(DIRECT3D11)
extern void initD3D11Renderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
extern void initD3D11RaytracingFunctions();
#endif

#if defined(DIRECT3D12)
extern void initD3D12Renderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
extern void initD3D12RaytracingFunctions();
#endif

#if defined(VULKAN)
extern void initVulkanRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
extern void initVulkanRaytracingFunctions();
#endif

#if defined(METAL)
extern void initMetalRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
extern void initMetalRaytracingFunctions();
#endif

#if defined(GLES)
extern void initGLESRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
#endif

#if defined(ORBIS)
extern void initOrbisRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
#endif

#if defined(PROSPERO)
extern void initProsperoRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer);
#endif

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

void initRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer)
{
	ASSERT(ppRenderer);
	ASSERT(pSettings);

	// Init requested renderer API
	gSelectedRendererApi = pSettings->mApi;
	initRendererAPI(appName, pSettings, ppRenderer, gSelectedRendererApi);

#if defined(USE_MULTIPLE_RENDER_APIS)
	// Fallback on other available APIs
	for (uint32_t i = 0; i < RENDERER_API_COUNT && !*ppRenderer; ++i)
	{
		if (i == pSettings->mApi)
			continue;

		gSelectedRendererApi = (RendererApi)i;
		initRendererAPI(appName, pSettings, ppRenderer, gSelectedRendererApi);
	}
#endif
}

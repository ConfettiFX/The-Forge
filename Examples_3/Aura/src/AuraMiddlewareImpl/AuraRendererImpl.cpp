/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
 *
 * This is a part of Aura.
 * 
 * This file(code) is licensed under a 
 * Creative Commons Attribution-NonCommercial 4.0 International License 
 *
 *   (https://creativecommons.org/licenses/by-nc/4.0/legalcode) 
 *
 * Based on a work at https://github.com/ConfettiFX/The-Forge.
 * You may not use the material for commercial purposes.
 *
*/

#include "../../../../../Custom-Middleware/Aura/Interfaces/IAuraRenderer.h"
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/string_hash_map.h"
#include "../../../../Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"

DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(void, mapBuffer, Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
DECLARE_RENDERER_FUNCTION(void, unmapBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(void, addTexture, Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
DECLARE_RENDERER_FUNCTION(void, removeTexture, Renderer* pRenderer, Texture* pTexture)

extern RendererApi gSelectedRendererApi;

#ifdef ORBIS
namespace aura {
extern void cmdCopyResourceOrbis(Cmd* p_cmd, const aura::TextureDesc* pDesc, Texture* pSrc, Buffer* pDst);
}
#endif

// We need this function since to enable copying of the gpu texture to cpu without any stalls (asynchronous)
void cmdCopyTexture(Cmd* p_cmd, Texture* pSrc, Texture* pDst)
{
	ASSERT(p_cmd);
	ASSERT(pSrc);
	ASSERT(pDst);
#if defined(DIRECT3D12)
	if (gSelectedRendererApi == RENDERER_API_D3D12)
	{
		p_cmd->mD3D12.pDxCmdList->CopyResource(pDst->mD3D12.pDxResource, pSrc->mD3D12.pDxResource);
	}
#elif defined(VULKAN)
#elif defined(METAL)
#endif
}

typedef struct DescriptorIndexMap
{
	eastl::string_hash_map<uint32_t> mMap;
} DescriptorIndexMap;

namespace aura {
eastl::vector<Buffer*> gAsyncBufferPointers;

void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** pp_texture)
{
	COMPILE_ASSERT(sizeof(TextureDesc) == sizeof(::TextureDesc));
	::addTexture(pRenderer, (::TextureDesc*)pDesc, pp_texture);
}
void removeTexture(Renderer* pRenderer, Texture* p_texture) { ::removeTexture(pRenderer, p_texture); }
void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* p_desc, RenderTarget** pp_render_target)
{
	COMPILE_ASSERT(sizeof(RenderTargetDesc) == sizeof(::RenderTargetDesc));
	::addRenderTarget(pRenderer, (::RenderTargetDesc*)p_desc, pp_render_target);
}
void removeRenderTarget(Renderer* pRenderer, RenderTarget* p_render_target) { ::removeRenderTarget(pRenderer, p_render_target); }
void addUniformBuffer(Renderer* pRenderer, uint32_t size, Buffer** ppUniformBuffer)
{
	::BufferDesc ubDesc = {};
	ubDesc.mDescriptors = (::DescriptorType)(DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	ubDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	ubDesc.mSize = (uint64_t)size;
	addBuffer(pRenderer, &ubDesc, ppUniformBuffer);
}
void addReadbackBuffer(Renderer* pRenderer, uint32_t size, Buffer** ppReadbackBuffer)
{
	::BufferDesc readbackDesc = {};
	readbackDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
	readbackDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	readbackDesc.mStartState = ::RESOURCE_STATE_COPY_DEST;
	readbackDesc.mSize = (uint64_t)size;
	readbackDesc.mAlignment = 65536;
	readbackDesc.pName = "Readback Buffer";
	addBuffer(pRenderer, &readbackDesc, ppReadbackBuffer);
}
void addUploadBuffer(Renderer* pRenderer, uint32_t size, Buffer** ppUploadBuffer)
{
	::BufferDesc uploadDesc = {};
	uploadDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	uploadDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	uploadDesc.mStartState = ::RESOURCE_STATE_GENERIC_READ;
	uploadDesc.mSize = (uint64_t)size;
	uploadDesc.pName = "Upload Buffer";
	addBuffer(pRenderer, &uploadDesc, ppUploadBuffer);
}
void removeBuffer(Renderer* pRenderer, Buffer* pUniformBuffer) { ::removeBuffer(pRenderer, pUniformBuffer); }
void updateUniformBuffer(Renderer* pRenderer, Buffer* pBuffer, uint32_t dstOffset, const void* pData, uint32_t srcOffset, uint32_t size)
{
	BufferUpdateDesc updateDesc = { pBuffer, (uint64_t)dstOffset, (uint64_t)size };
	beginUpdateResource(&updateDesc);
	memcpy(updateDesc.pMappedData, (uint8_t*)pData + srcOffset, size);
	endUpdateResource(&updateDesc, NULL);
}
void addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** pp_sampler)
{
	COMPILE_ASSERT(sizeof(SamplerDesc) == sizeof(::SamplerDesc));
	::addSampler(pRenderer, (::SamplerDesc*)pDesc, pp_sampler);
}
void removeSampler(Renderer* pRenderer, Sampler* p_sampler) { ::removeSampler(pRenderer, p_sampler); }
void getTextureFromRenderTarget(RenderTarget* pRenderTarget, Texture** ppTexture) { *ppTexture = pRenderTarget->pTexture; }

void getCpuMappedAddress(Buffer* pBuffer, void** pData) { *pData = pBuffer->pCpuMappedAddress; }

bool hasShaderResource(RootSignature* pRootSignature, const char* pResName)
{
	return pRootSignature->pDescriptorNameToIndexMap->mMap.find(pResName) != pRootSignature->pDescriptorNameToIndexMap->mMap.end();
}
// shader functions
void addShader(Renderer* pRenderer, const ShaderLoadDesc* p_desc, Shader** p_shader_program)
{
	COMPILE_ASSERT(sizeof(ShaderLoadDesc) == sizeof(::ShaderLoadDesc));
	::addShader(pRenderer, (::ShaderLoadDesc*)p_desc, p_shader_program);
}
void removeShader(Renderer* pRenderer, Shader* p_shader_program) { ::removeShader(pRenderer, p_shader_program); }
// pipeline functions
void addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootDesc, RootSignature** pp_root_signature)
{
	COMPILE_ASSERT(sizeof(RootSignatureDesc) == sizeof(::RootSignatureDesc));
	::addRootSignature(pRenderer, (::RootSignatureDesc*)pRootDesc, pp_root_signature);
}
void removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature) { ::removeRootSignature(pRenderer, pRootSignature); }
void addPipeline(Renderer* pRenderer, const PipelineDesc* p_pipeline_settings, Pipeline** pp_pipeline)
{
	COMPILE_ASSERT(sizeof(PipelineDesc) == sizeof(::PipelineDesc));
	::addPipeline(pRenderer, (::PipelineDesc*)p_pipeline_settings, pp_pipeline);
}
void removePipeline(Renderer* pRenderer, Pipeline* p_pipeline) { ::removePipeline(pRenderer, p_pipeline); }
void addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
{
	COMPILE_ASSERT(sizeof(DescriptorSetDesc) == sizeof(::DescriptorSetDesc));
	::addDescriptorSet(pRenderer, (::DescriptorSetDesc*)pDesc, ppDescriptorSet);
}
void removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet) { ::removeDescriptorSet(pRenderer, pDescriptorSet); }
void updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
	COMPILE_ASSERT(sizeof(DescriptorData) == sizeof(::DescriptorData));
	::updateDescriptorSet(pRenderer, index, pDescriptorSet, count, (::DescriptorData*)pParams);
}
void cmdBindRenderTargets(
	Cmd* p_cmd, uint32_t render_target_count, RenderTarget** pp_render_targets, RenderTarget* p_depth_stencil,
	const LoadActionsDesc* loadActions, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice,
	uint32_t depthMipSlice)
{
	COMPILE_ASSERT(sizeof(LoadActionsDesc) == sizeof(::LoadActionsDesc));
	::cmdBindRenderTargets(
		p_cmd, render_target_count, pp_render_targets, p_depth_stencil, (const ::LoadActionsDesc*)loadActions, pColorArraySlices,
		pColorMipSlices, depthArraySlice, depthMipSlice);
}
void cmdSetViewport(Cmd* p_cmd, float x, float y, float width, float height, float min_depth, float max_depth)
{
	::cmdSetViewport(p_cmd, x, y, width, height, min_depth, max_depth);
}
void cmdSetScissor(Cmd* p_cmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height) { ::cmdSetScissor(p_cmd, x, y, width, height); }
void cmdBindPipeline(Cmd* p_cmd, Pipeline* p_pipeline) { ::cmdBindPipeline(p_cmd, p_pipeline); }
void cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet) { ::cmdBindDescriptorSet(pCmd, index, pDescriptorSet); }
void cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
{
	::cmdBindPushConstants(pCmd, pRootSignature, paramIndex, pConstants);
}
void cmdBindIndexBuffer(Cmd* p_cmd, uint32_t indexType, Buffer* p_buffer) { ::cmdBindIndexBuffer(p_cmd, p_buffer, indexType, 0); }
void cmdBindVertexBuffer(Cmd* p_cmd, uint32_t buffer_count, uint32_t* strides, Buffer** pp_buffers)
{
	::cmdBindVertexBuffer(p_cmd, buffer_count, pp_buffers, strides, NULL);
}
void cmdDraw(Cmd* p_cmd, uint32_t vertex_count, uint32_t first_vertex) { ::cmdDraw(p_cmd, vertex_count, first_vertex); }
void cmdDrawInstanced(Cmd* p_cmd, uint32_t vertex_count, uint32_t first_vertex, uint32_t instance_count, uint32_t first_instance)
{
	::cmdDrawInstanced(p_cmd, vertex_count, first_vertex, instance_count, first_instance);
}
void cmdDispatch(Cmd* p_cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	::cmdDispatch(p_cmd, group_count_x, group_count_y, group_count_z);
}
void cmdCopyTexture(Cmd* p_cmd, Texture* pSrc, Texture* pDst) { ::cmdCopyTexture(p_cmd, pSrc, pDst); }
void cmdCopyResource(Cmd* p_cmd, const TextureDesc* pDesc, Texture* pSrc, Buffer* pDst)
{
	ASSERT(p_cmd);
	ASSERT(pSrc);
	ASSERT(pDst);

#if defined(DIRECT3D12)
	if (gSelectedRendererApi == RENDERER_API_D3D12)
	{
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
		D3D12_RESOURCE_DESC                Desc = pSrc->mD3D12.pDxResource->GetDesc();
		p_cmd->pRenderer->mD3D12.pDxDevice->GetCopyableFootprints(&Desc, 0, 1, 0, &layout, NULL, NULL, NULL);

		D3D12_TEXTURE_COPY_LOCATION Src = {};
		Src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		Src.pResource = pSrc->mD3D12.pDxResource;
		Src.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION Dst = {};
		Dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		Dst.pResource = pDst->mD3D12.pDxResource;
		Dst.PlacedFootprint = layout;

		p_cmd->mD3D12.pDxCmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
	}
#endif
#if defined(VULKAN)
	if (gSelectedRendererApi == RENDERER_API_VULKAN)
	{
		VkBufferImageCopy pCopy = {};
		pCopy.bufferOffset = 0;
		pCopy.bufferRowLength = 0;
		pCopy.bufferImageHeight = 0;
		pCopy.imageSubresource.aspectMask = (VkImageAspectFlags)pSrc->mAspectMask;
		pCopy.imageSubresource.mipLevel = 0;
		pCopy.imageSubresource.baseArrayLayer = 0;
		pCopy.imageSubresource.layerCount = 1;
		pCopy.imageOffset.x = 0;
		pCopy.imageOffset.y = 0;
		pCopy.imageOffset.z = 0;
		pCopy.imageExtent.width = pDesc->mWidth;
		pCopy.imageExtent.height = pDesc->mHeight;
		pCopy.imageExtent.depth = pDesc->mDepth;
		vkCmdCopyImageToBuffer(
			p_cmd->mVulkan.pVkCmdBuf, pSrc->mVulkan.pVkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pDst->mVulkan.pVkBuffer, 1, &pCopy);
	}
#endif
#if defined(ORBIS)
	cmdCopyResourceOrbis(p_cmd, pDesc, pSrc, pDst);
#endif

	gAsyncBufferPointers.push_back(pDst);
}

void cmdCopyResource(Cmd* p_cmd, Buffer* pSrc, Texture* pDst)
{
#if defined(DIRECT3D12)
	if (gSelectedRendererApi == RENDERER_API_D3D12)
	{
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
		D3D12_RESOURCE_DESC                Desc = pDst->mD3D12.pDxResource->GetDesc();
		p_cmd->pRenderer->mD3D12.pDxDevice->GetCopyableFootprints(&Desc, 0, 1, 0, &layout, NULL, NULL, NULL);

		D3D12_TEXTURE_COPY_LOCATION Src = {};
		Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		Src.pResource = pSrc->mD3D12.pDxResource;
		Src.PlacedFootprint = layout;

		D3D12_TEXTURE_COPY_LOCATION Dst = {};
		Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		Dst.pResource = pDst->mD3D12.pDxResource;
		Dst.SubresourceIndex = 0;

		p_cmd->mD3D12.pDxCmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
	}
#endif
}

void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer) { ::unmapBuffer(pRenderer, pBuffer); }

void mapAsynchronousResources(Renderer* pRenderer)
{
	for (Buffer* buffer : gAsyncBufferPointers)
	{
		if (buffer->pCpuMappedAddress == nullptr)
		{
			ReadRange range = { 0, 0 };
			mapBuffer(pRenderer, buffer, &range);
		}
	}

	gAsyncBufferPointers.clear();
}

void removeAsynchronousResources() { gAsyncBufferPointers.set_capacity(0); }

//
////Debug markers
////Color is in XRGB format (X being padding) 0xff0000 -> red, 0x00ff00 -> green, 0x0000ff -> blue
//void cmdPushDebugMarker(Cmd* pCmd, uint32_t color, const char* name, ...);
//void cmdPopDebugMarker(Cmd* pCmd);

// Transition Commands
void cmdResourceBarrier(
	Cmd* p_cmd, uint32_t buffer_barrier_count, BufferBarrier* p_buffer_barriers, uint32_t texture_barrier_count,
	TextureBarrier* p_texture_barriers, uint32_t rt_barrier_count, RenderTargetBarrier* p_rt_barriers)
{
	::cmdResourceBarrier(
		p_cmd, buffer_barrier_count, (::BufferBarrier*)p_buffer_barriers, texture_barrier_count, (::TextureBarrier*)p_texture_barriers,
		rt_barrier_count, (::RenderTargetBarrier*)p_rt_barriers);
}
////Queue debug markers
////Color is in XRGB format (X being padding) 0xff0000 -> red, 0x00ff00 -> green, 0x0000ff -> blue
//void queuePushDebugMarker(Queue* pQueue, uint32_t color, const char* name, ...);
//void queuePopDebugMarker(Queue* pQueue);
/************************************************************************/
// Debug Marker Interface
/************************************************************************/
void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName) { ::cmdBeginDebugMarker(pCmd, r, g, b, pName); }
void cmdEndDebugMarker(Cmd* pCmd) { ::cmdEndDebugMarker(pCmd); }
void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName) { ::cmdAddDebugMarker(pCmd, r, g, b, pName); }
/************************************************************************/
uint32_t getDescriptorIndexFromName(const RootSignature* pRootSignature, const char* pName)
{
	return ::getDescriptorIndexFromName(pRootSignature, pName);
}
}    // namespace aura

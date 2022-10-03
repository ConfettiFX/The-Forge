/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "Interfaces/IScreenshot.h"

#if defined(ENABLE_SCREENSHOT)
#include "../Utilities/Math/MathTypes.h"

#include "../Utilities/Interfaces/ILog.h"
#include "../Utilities/Interfaces/IFileSystem.h"
#include "../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

#if defined(ORBIS)
#include "../../PS4/Common_3/OS/Orbis/OrbisScreenshot.h"
#elif defined(PROSPERO)
#include "../../Prospero/Common_3/OS/Prospero/ProsperoScreenshot.h"
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_MALLOC tf_malloc
#define STBIW_REALLOC tf_realloc
#define STBIW_FREE tf_free
#define STBIW_ASSERT ASSERT
#include "../Utilities/ThirdParty/OpenSource/Nothings/stb_image_write.h"

static Cmd*        pCmd = 0;
static CmdPool*    pCmdPool = 0;
static Renderer*   pRendererRef = 0;
extern RendererApi gSelectedRendererApi;

void initScreenshotInterface(Renderer* pRenderer, Queue* pGraphicsQueue)
{
	ASSERT(pRenderer);
	ASSERT(pGraphicsQueue);

	pRendererRef = pRenderer;

	// Allocate a command buffer for the GPU work. We use the app's rendering queue to avoid additional sync.
	CmdPoolDesc cmdPoolDesc = {};
	cmdPoolDesc.pQueue = pGraphicsQueue;
	cmdPoolDesc.mTransient = true;
	addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPool);

	CmdDesc cmdDesc = {};
	cmdDesc.pPool = pCmdPool;
	addCmd(pRenderer, &cmdDesc, &pCmd);
}
// Helper function to generate screenshot data. Not part of IScreenshot.h
void mapRenderTarget(
	Renderer* pRenderer, Queue* pQueue, Cmd* pCmd, RenderTarget* pRenderTarget, ResourceState currentResourceState, void* pImageData)
{
	ASSERT(pImageData);
	ASSERT(pRenderTarget);
	ASSERT(pRenderer);

#if defined(VULKAN)
	if (gSelectedRendererApi == RENDERER_API_VULKAN)
	{
		DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
		DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)

		// Add a staging buffer.
		uint16_t   formatByteWidth = TinyImageFormat_BitSizeOfBlock(pRenderTarget->mFormat) / 8;
		Buffer*    buffer = 0;
		BufferDesc bufferDesc = {};
		bufferDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
		bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
		bufferDesc.mSize = pRenderTarget->mWidth * pRenderTarget->mHeight * formatByteWidth;
		bufferDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
		bufferDesc.mStartState = RESOURCE_STATE_COPY_DEST;
		addBuffer(pRenderer, &bufferDesc, &buffer);

		beginCmd(pCmd);

		RenderTargetBarrier srcBarrier = { pRenderTarget, currentResourceState, RESOURCE_STATE_COPY_SOURCE };
		cmdResourceBarrier(pCmd, 0, 0, 0, 0, 1, &srcBarrier);

		uint32_t              rowPitch = pRenderTarget->mWidth * formatByteWidth;
		const uint32_t        width = pRenderTarget->pTexture->mWidth;
		const uint32_t        height = pRenderTarget->pTexture->mHeight;
		const uint32_t        depth = max<uint32_t>(1, pRenderTarget->pTexture->mDepth);
		const TinyImageFormat fmt = (TinyImageFormat)pRenderTarget->pTexture->mFormat;
		const uint32_t        numBlocksWide = rowPitch / (TinyImageFormat_BitSizeOfBlock(fmt) >> 3);

		VkBufferImageCopy copy = {};
		copy.bufferOffset = 0;
		copy.bufferRowLength = numBlocksWide * TinyImageFormat_WidthOfBlock(fmt);
		copy.bufferImageHeight = 0;
		copy.imageSubresource.aspectMask = (VkImageAspectFlags)pRenderTarget->pTexture->mAspectMask;
		copy.imageSubresource.mipLevel = 0;
		copy.imageSubresource.baseArrayLayer = 0;
		copy.imageSubresource.layerCount = 1;
		copy.imageOffset.x = 0;
		copy.imageOffset.y = 0;
		copy.imageOffset.z = 0;
		copy.imageExtent.width = width;
		copy.imageExtent.height = height;
		copy.imageExtent.depth = depth;
		vkCmdCopyImageToBuffer(
			pCmd->mVulkan.pVkCmdBuf, pRenderTarget->pTexture->mVulkan.pVkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			buffer->mVulkan.pVkBuffer, 1, &copy);

		srcBarrier = { pRenderTarget, RESOURCE_STATE_COPY_SOURCE, currentResourceState };
		cmdResourceBarrier(pCmd, 0, 0, 0, 0, 1, &srcBarrier);

		endCmd(pCmd);

		// Submit the gpu work.
		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.ppCmds = &pCmd;

		queueSubmit(pQueue, &submitDesc);

		// Wait for work to finish on the GPU.
		waitQueueIdle(pQueue);

		// Copy to CPU memory.
		memcpy(pImageData, buffer->pCpuMappedAddress, pRenderTarget->mWidth * pRenderTarget->mHeight * formatByteWidth);
		removeBuffer(pRenderer, buffer);
	}
#endif
#if defined(DIRECT3D11)
	if (gSelectedRendererApi == RENDERER_API_D3D11)
	{
		// Add a staging texture.
		ID3D11Texture2D* pNewTexture = NULL;

		D3D11_TEXTURE2D_DESC description = {};
		((ID3D11Texture2D*)pRenderTarget->pTexture->mD3D11.pDxResource)->GetDesc(&description);
		description.BindFlags = 0;
		description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		description.Usage = D3D11_USAGE_STAGING;

		pRenderer->mD3D11.pDxDevice->CreateTexture2D(&description, NULL, &pNewTexture);

		beginCmd(pCmd);

		pRenderer->mD3D11.pDxContext->CopyResource(pNewTexture, pRenderTarget->pTexture->mD3D11.pDxResource);

		endCmd(pCmd);

		// Submit the gpu work.
		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.ppCmds = &pCmd;

		queueSubmit(pQueue, &submitDesc);

		// Wait for work to finish on the GPU.
		waitQueueIdle(pQueue);

		// Map texture for copy.
		D3D11_MAPPED_SUBRESOURCE resource = {};
		unsigned int             subresource = D3D11CalcSubresource(0, 0, 0);
		pRenderer->mD3D11.pDxContext->Map(pNewTexture, subresource, D3D11_MAP_READ, 0, &resource);

		// Copy to CPU memory.
		uint16_t formatByteWidth = TinyImageFormat_BitSizeOfBlock(pRenderTarget->mFormat) / 8;
		for (uint32_t i = 0; i < pRenderTarget->mHeight; ++i)
		{
			memcpy(
				(uint8_t*)pImageData + i * pRenderTarget->mWidth * formatByteWidth, (uint8_t*)resource.pData + i * resource.RowPitch,
				pRenderTarget->mWidth * formatByteWidth);
		}

		pNewTexture->Release();
	}
#endif
#if defined(DIRECT3D12)
	if (gSelectedRendererApi == RENDERER_API_D3D12)
	{
		DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
		DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)

		// Calculate the size of buffer required for copying the src texture.
		D3D12_RESOURCE_DESC                resourceDesc = pRenderTarget->pTexture->mD3D12.pDxResource->GetDesc();
		uint64_t                           padded_size = 0;
		uint64_t                           row_size = 0;
		uint32_t                           num_rows = 0;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT imageLayout = {};
		pRenderer->mD3D12.pDxDevice->GetCopyableFootprints(&resourceDesc, 0, 1, 0, &imageLayout, &num_rows, &row_size, &padded_size);

		// Add a staging buffer.
		uint16_t   formatByteWidth = TinyImageFormat_BitSizeOfBlock(pRenderTarget->mFormat) / 8;
		Buffer*    buffer = 0;
		BufferDesc bufferDesc = {};
		bufferDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
		bufferDesc.mSize = padded_size;
		bufferDesc.mFlags = BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		bufferDesc.mStartState = RESOURCE_STATE_COPY_DEST;
		bufferDesc.mFormat = pRenderTarget->mFormat;
		addBuffer(pRenderer, &bufferDesc, &buffer);

		beginCmd(pCmd);

		// Transition layout to copy data out.
		RenderTargetBarrier srcBarrier = { pRenderTarget, currentResourceState, RESOURCE_STATE_COPY_SOURCE };
		cmdResourceBarrier(pCmd, 0, 0, 0, 0, 1, &srcBarrier);

		uint32_t subresource = 0;

		D3D12_TEXTURE_COPY_LOCATION src = {};
		D3D12_TEXTURE_COPY_LOCATION dst = {};

		src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src.pResource = pRenderTarget->pTexture->mD3D12.pDxResource;
		src.SubresourceIndex = subresource;

		dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dst.pResource = buffer->mD3D12.pDxResource;
		pCmd->pRenderer->mD3D12.pDxDevice->GetCopyableFootprints(&resourceDesc, 0, 1, 0, &dst.PlacedFootprint, NULL, NULL, NULL);

		pCmd->mD3D12.pDxCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

		// Transition layout to original state.
		srcBarrier = { pRenderTarget, RESOURCE_STATE_COPY_SOURCE, currentResourceState };
		cmdResourceBarrier(pCmd, 0, 0, 0, 0, 1, &srcBarrier);

		endCmd(pCmd);

		// Submit the GPU work.
		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.ppCmds = &pCmd;

		queueSubmit(pQueue, &submitDesc);

		// Wait for work to finish on the GPU.
		waitQueueIdle(pQueue);

		uint8_t* mappedData = (uint8_t*)pImageData;
		uint8_t* srcData = (uint8_t*)buffer->pCpuMappedAddress;
		uint64_t src_row_size = imageLayout.Footprint.RowPitch;

		// Copy row-wise to CPU memory.
		for (uint32_t i = 0; i < num_rows; ++i)
		{
			memcpy(mappedData, srcData, row_size);
			mappedData += row_size;
			srcData += src_row_size;
		}

		removeBuffer(pRenderer, buffer);
	}
#endif
#if defined(METAL)
	DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
	DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)
	DECLARE_RENDERER_FUNCTION(void, mapBuffer, Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
	DECLARE_RENDERER_FUNCTION(void, unmapBuffer, Renderer* pRenderer, Buffer* pBuffer)

	// Add a staging buffer.
	uint16_t   formatByteWidth = TinyImageFormat_BitSizeOfBlock(pRenderTarget->mFormat) / 8;
	Buffer*    buffer = 0;
	BufferDesc bufferDesc = {};
	bufferDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
	bufferDesc.mSize = pRenderTarget->mWidth * pRenderTarget->mHeight * formatByteWidth;
	bufferDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
	bufferDesc.mStartState = RESOURCE_STATE_COPY_DEST;
	addBuffer(pRenderer, &bufferDesc, &buffer);

	beginCmd(pCmd);

	RenderTargetBarrier srcBarrier = { pRenderTarget, currentResourceState, RESOURCE_STATE_COPY_SOURCE };
	cmdResourceBarrier(pCmd, 0, 0, 0, 0, 1, &srcBarrier);

	if (!pCmd->mtlBlitEncoder)
	{
		pCmd->mtlBlitEncoder = [pCmd->mtlCommandBuffer blitCommandEncoder];
	}

	// Copy to staging buffer.
	[pCmd->mtlBlitEncoder copyFromTexture:pRenderTarget->pTexture->mtlTexture
							  sourceSlice:0
							  sourceLevel:0
							 sourceOrigin:MTLOriginMake(0, 0, 0)
							   sourceSize:MTLSizeMake(pRenderTarget->mWidth, pRenderTarget->mHeight, pRenderTarget->mDepth)
								 toBuffer:buffer->mtlBuffer
						destinationOffset:0
				   destinationBytesPerRow:pRenderTarget->mWidth * formatByteWidth
				 destinationBytesPerImage:bufferDesc.mSize];

	srcBarrier = { pRenderTarget, RESOURCE_STATE_COPY_SOURCE, currentResourceState };
	cmdResourceBarrier(pCmd, 0, 0, 0, 0, 1, &srcBarrier);

	endCmd(pCmd);

	// Submit the gpu work.
	QueueSubmitDesc submitDesc = {};
	submitDesc.mCmdCount = 1;
	submitDesc.ppCmds = &pCmd;

	queueSubmit(pQueue, &submitDesc);

	// Wait for work to finish on the GPU.
	waitQueueIdle(pQueue);

	mapBuffer(pRenderer, buffer, 0);
	memcpy(pImageData, buffer->pCpuMappedAddress, pRenderTarget->mWidth * pRenderTarget->mHeight * formatByteWidth);
	unmapBuffer(pRenderer, buffer);
	removeBuffer(pRenderer, buffer);
#elif defined(ORBIS)
	mapRenderTargetOrbis(pRenderTarget, pImageData);
#elif defined(PROSPERO)
	mapRenderTargetProspero(pRenderer, pQueue, pCmd, pRenderTarget, currentResourceState, pImageData);
#endif
}

bool prepareScreenshot(SwapChain* pSwapChain)
{
#if defined(METAL)
	if(@available(ios 13.0, *))
	{
		CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pForgeView.layer;
		if (layer.framebufferOnly)
		{
			layer.framebufferOnly = false;
			return false;
		}
	}
#endif
	return true;
}

void captureScreenshot(
	SwapChain* pSwapChain, uint32_t swapChainRtIndex, ResourceState renderTargetCurrentState, const char* pngFileName, bool noAlpha)
{
	ASSERT(pRendererRef);
	ASSERT(pCmdPool->pQueue);
	ASSERT(pSwapChain);
	// initScreenshotInterface not called.
	ASSERT(pCmd);

#if defined(METAL)
	if(@available(ios 13.0, *))
	{
		CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pForgeView.layer;
		if (layer.framebufferOnly)
		{
			LOGF(eERROR, "prepareScreenshot() must be used one frame before using captureScreenshot()");
			ASSERT(0);
			return;
		}
	}
#endif

	RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapChainRtIndex];

	// Wait for queue to finish rendering.
	waitQueueIdle(pCmdPool->pQueue);

	// Allocate temp space
	uint16_t byteSize = TinyImageFormat_BitSizeOfBlock(pRenderTarget->mFormat) / 8;
	uint8_t  channelCount = TinyImageFormat_ChannelCount(pRenderTarget->mFormat);
	void*    alloc = tf_malloc(pRenderTarget->mWidth * pRenderTarget->mHeight * byteSize);

	resetCmdPool(pRendererRef, pCmdPool);

	// Generate image data buffer.
	mapRenderTarget(pRendererRef, pCmdPool->pQueue, pCmd, pRenderTarget, renderTargetCurrentState, alloc);

	// Flip the BGRA to RGBA
	const bool flipRedBlueChannel = pRenderTarget->mFormat != TinyImageFormat_R8G8B8A8_UNORM;
	if (flipRedBlueChannel)
	{
		int8_t* imageData = ((int8_t*)alloc);

		for (uint32_t h = 0; h < pRenderTarget->mHeight; ++h)
		{
			for (uint32_t w = 0; w < pRenderTarget->mWidth; ++w)
			{
				uint32_t pixelIndex = (h * pRenderTarget->mWidth + w) * channelCount;
				int8_t*  pixel = imageData + pixelIndex;

				// Swap blue and red.
				int8_t r = pixel[0];
				pixel[0] = pixel[2];
				pixel[2] = r;
			}
		}
	}

	if (noAlpha)
	{
		uint8_t* imageData = ((uint8_t*)alloc);
		for (uint32_t i = 3; i < (uint32_t)(pRenderTarget->mWidth * pRenderTarget->mHeight * byteSize); i += byteSize)
		{
			imageData[i] = 255u;
		}
	}

	// Convert image data to png.
	int            len = 0;
	unsigned char* png = stbi_write_png_to_mem(
		(unsigned char*)alloc, pRenderTarget->mWidth * byteSize, pRenderTarget->mWidth, pRenderTarget->mHeight, channelCount, &len);

	// Save png to disk.
	FileStream fs = {};
	fsOpenStreamFromPath(RD_SCREENSHOTS, pngFileName, FM_WRITE_BINARY, NULL, &fs);
	fsWriteToStream(&fs, png, (size_t)len);
	fsCloseStream(&fs);

	tf_free(alloc);
	tf_free(png);

#if defined(METAL)
	if(@available(ios 13.0, *))
	{
		CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pForgeView.layer;
		layer.framebufferOnly = true;
	}
#endif
}

void captureScreenshot(SwapChain* pSwapChain, uint32_t swapChainRtIndex, ResourceState renderTargetCurrentState, const char* pngFileName)
{
	captureScreenshot(pSwapChain, swapChainRtIndex, renderTargetCurrentState, pngFileName, true);
}

void exitScreenshotInterface()
{
	removeCmd(pRendererRef, pCmd);
	removeCmdPool(pRendererRef, pCmdPool);
}
#else
void initScreenshotInterface(Renderer* pRenderer, Queue* pQueue) {}
bool prepareScreenshot(SwapChain* pSwapChain) { return false; }
void captureScreenshot(SwapChain* pSwapChain, uint32_t swapChainRtIndex, ResourceState renderTargetCurrentState, const char* pngFileName) {}
void captureScreenshot(
	SwapChain* pSwapChain, uint32_t swapChainRtIndex, ResourceState renderTargetCurrentState, const char* pngFileName, bool noAlpha)
{
}
void exitScreenshotInterface() {}
#endif

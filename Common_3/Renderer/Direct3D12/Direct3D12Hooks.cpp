/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#include "../IRenderer.h"
#include "Direct3D12Hooks.h"

#include "../../OS/Interfaces/IMemory.h"

HMODULE hook_get_d3d12_module_handle()
{
	return GetModuleHandle(TEXT("d3d12.dll"));
}

void hook_post_init_renderer(Renderer*)
{
}

void hook_post_remove_renderer(Renderer*)
{
}

void hook_enable_debug_layer(Renderer* pRenderer)
{
	UNREF_PARAM(pRenderer);
#if defined(ENABLE_GRAPHICS_DEBUG)
	pRenderer->pDXDebug->EnableDebugLayer();
	ID3D12Debug1* pDebug1 = NULL;
	if (SUCCEEDED(pRenderer->pDXDebug->QueryInterface(IID_PPV_ARGS(&pDebug1))))
	{
		pDebug1->SetEnableGPUBasedValidation(pRenderer->mEnableGpuBasedValidation);
		pDebug1->Release();
	}
#endif
}

HRESULT hook_create_device(void* pAdapter, D3D_FEATURE_LEVEL featureLevel, ID3D12Device** ppDevice)
{
	return D3D12CreateDevice((IUnknown*)pAdapter, featureLevel, IID_PPV_ARGS(ppDevice));
}

HRESULT hook_create_command_queue(ID3D12Device* pDevice, const D3D12_COMMAND_QUEUE_DESC* pDesc, ID3D12CommandQueue** ppQueue)
{
	return pDevice->CreateCommandQueue(pDesc, IID_PPV_ARGS(ppQueue));
}

HRESULT hook_create_copy_cmd(ID3D12Device* pDevice, uint32_t nodeMask, ID3D12CommandAllocator* pAlloc, Cmd* pCmd)
{
	return pDevice->CreateCommandList(nodeMask, D3D12_COMMAND_LIST_TYPE_COPY, pAlloc, NULL, IID_PPV_ARGS(&pCmd->pDxCmdList));
}

void hook_remove_copy_cmd(Cmd* pCmd)
{
	if (pCmd->pDxCmdList)
	{
		pCmd->pDxCmdList->Release();
		pCmd->pDxCmdList = NULL;
	}
}

HRESULT hook_create_graphics_pipeline_state(ID3D12Device* pDevice, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, void*, uint32_t, ID3D12PipelineState** ppPipeline)
{
	return pDevice->CreateGraphicsPipelineState(pDesc, IID_PPV_ARGS(ppPipeline));
}

HRESULT hook_create_compute_pipeline_state(ID3D12Device* pDevice, const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, void*, uint32_t, ID3D12PipelineState** ppPipeline)
{
	return pDevice->CreateComputePipelineState(pDesc, IID_PPV_ARGS(ppPipeline));
}

HRESULT hook_create_special_resource(
	Renderer*,
	const D3D12_RESOURCE_DESC*,
	const D3D12_CLEAR_VALUE*,
	D3D12_RESOURCE_STATES,
	uint32_t,
	ID3D12Resource**)
{
	return E_NOINTERFACE;
}

TinyImageFormat hook_get_recommended_swapchain_format(bool)
{
	return TinyImageFormat_B8G8R8A8_UNORM;
}

uint32_t hook_get_swapchain_image_index(SwapChain* pSwapChain)
{
	return pSwapChain->pDxSwapChain->GetCurrentBackBufferIndex();
}

HRESULT hook_acquire_next_image(ID3D12Device*, SwapChain*)
{
	return S_OK;
}

HRESULT hook_queue_present(Queue*, SwapChain* pSwapChain, uint32_t)
{
	return pSwapChain->pDxSwapChain->Present(pSwapChain->mDxSyncInterval, 0);
}

void hook_dispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	pCmd->pDxCmdList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

void hook_signal(Queue* pQueue, ID3D12Fence* pDxFence, uint64_t fenceValue)
{
	pQueue->pDxQueue->Signal(pDxFence, fenceValue);
}

extern void hook_fill_gpu_desc(Renderer* pRenderer, D3D_FEATURE_LEVEL featureLevel, GpuDesc* pInOutDesc)
{
	// Query the level of support of Shader Model.
	D3D12_FEATURE_DATA_D3D12_OPTIONS  featureData = {};
	D3D12_FEATURE_DATA_D3D12_OPTIONS1 featureData1 = {};
	// Query the level of support of Wave Intrinsics.
	pRenderer->pDxDevice->CheckFeatureSupport(
		(D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS, &featureData, sizeof(featureData));
	pRenderer->pDxDevice->CheckFeatureSupport(
		(D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &featureData1, sizeof(featureData1));

	GpuDesc& gpuDesc = *pInOutDesc;
	DXGI_ADAPTER_DESC3 desc = {};
	gpuDesc.pGpu->GetDesc3(&desc);

	gpuDesc.mMaxSupportedFeatureLevel = featureLevel;
	gpuDesc.mDedicatedVideoMemory = desc.DedicatedVideoMemory;
	gpuDesc.mFeatureDataOptions = featureData;
	gpuDesc.mFeatureDataOptions1 = featureData1;
	gpuDesc.pRenderer = pRenderer;

	//save vendor and model Id as string
	//char hexChar[10];
	//convert deviceId and assign it
	sprintf_s(gpuDesc.mDeviceId, "%#x\0", desc.DeviceId);
	//convert modelId and assign it
	sprintf_s(gpuDesc.mVendorId, "%#x\0", desc.VendorId);
	//convert Revision Id
	sprintf_s(gpuDesc.mRevisionId, "%#x\0", desc.Revision);

	//save gpu name (Some situtations this can show description instead of name)
	//char sName[MAX_PATH];
	size_t numConverted = 0;
	wcstombs_s(&numConverted, gpuDesc.mName, desc.Description, MAX_GPU_VENDOR_STRING_LENGTH);
}

void hook_modify_descriptor_heap_size(D3D12_DESCRIPTOR_HEAP_TYPE, uint32_t*)
{
}

void hook_modify_swapchain_desc(DXGI_SWAP_CHAIN_DESC1*)
{
}

void hook_modify_heap_flags(DescriptorType, D3D12_HEAP_FLAGS*)
{
}

void hook_modify_buffer_resource_desc(const BufferDesc*, D3D12_RESOURCE_DESC*)
{
}

void hook_modify_texture_resource_flags(TextureCreationFlags, D3D12_RESOURCE_FLAGS*)
{
}

void hook_modify_shader_compile_flags(uint32_t, bool, const WCHAR**, uint32_t* pOutNumFlags)
{
	*pOutNumFlags = 0;
}

void hook_modify_rootsignature_flags(uint32_t, D3D12_ROOT_SIGNATURE_FLAGS*)
{
}

void hook_modify_command_signature_desc(D3D12_COMMAND_SIGNATURE_DESC* pInOutDesc, uint32_t padding)
{
	pInOutDesc->ByteStride += padding;
}
/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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
#include "../GraphicsConfig.h"

#ifdef DIRECT3D12

#include "../ThirdParty/OpenSource/D3D12MemoryAllocator/Direct3D12MemoryAllocator.h"

#include "../Interfaces/IGraphics.h"

#include "../../../Common_3/Utilities/Math/MathTypes.h"

#include "Direct3D12Hooks.h"

#include "../../Utilities/Interfaces/IMemory.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p_var) \
    if (p_var)              \
    {                       \
        p_var->Release();   \
        p_var = NULL;       \
    }
#endif

extern DXGI_FORMAT           util_to_dx12_swapchain_format(TinyImageFormat format);
extern DXGI_COLOR_SPACE_TYPE util_to_dx12_colorspace(ColorSpace colorspace);

HMODULE hook_get_d3d12_module_handle() { return GetModuleHandle(TEXT("d3d12.dll")); }

void hook_post_init_renderer(Renderer*) {}

void hook_post_remove_renderer(Renderer*) {}

HRESULT WINAPI d3d12dll_DXGIGetDebugInterface1(UINT Flags, REFIID riid, void** ppFactory);

void hook_enable_debug_layer(const RendererContextDesc* pDesc, RendererContext* pContext)
{
    UNREF_PARAM(pDesc);
    UNREF_PARAM(pContext);
#if defined(ENABLE_GRAPHICS_DEBUG)
    pContext->mDx.pDebug->EnableDebugLayer();

    ID3D12Debug1* pDebug1 = NULL;
    if (SUCCEEDED(pContext->mDx.pDebug->QueryInterface(IID_PPV_ARGS(&pDebug1))))
    {
        pDebug1->SetEnableGPUBasedValidation(pDesc->mEnableGpuBasedValidation);
        pDebug1->Release();
    }
#endif
}

HRESULT WINAPI d3d12dll_CreateDevice(void* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice);

HRESULT hook_create_device(void* pAdapter, D3D_FEATURE_LEVEL featureLevel, ID3D12Device** ppDevice)
{
    return d3d12dll_CreateDevice(pAdapter, featureLevel, IID_PPV_ARGS(ppDevice));
}

HRESULT hook_create_command_queue(ID3D12Device* pDevice, const D3D12_COMMAND_QUEUE_DESC* pDesc, ID3D12CommandQueue** ppQueue)
{
    return pDevice->CreateCommandQueue(pDesc, IID_PPV_ARGS(ppQueue));
}

HRESULT hook_create_copy_cmd(ID3D12Device* pDevice, uint32_t nodeMask, ID3D12CommandAllocator* pAlloc, Cmd* pCmd)
{
    return pDevice->CreateCommandList(nodeMask, D3D12_COMMAND_LIST_TYPE_COPY, pAlloc, NULL, IID_PPV_ARGS(&pCmd->mDx.pCmdList));
}

void hook_remove_copy_cmd(Cmd* pCmd)
{
    if (pCmd->mDx.pCmdList)
    {
        pCmd->mDx.pCmdList->Release();
        pCmd->mDx.pCmdList = NULL;
    }
}

HRESULT hook_create_graphics_pipeline_state(ID3D12Device* pDevice, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, void*, uint32_t,
                                            ID3D12PipelineState** ppPipeline)
{
    return pDevice->CreateGraphicsPipelineState(pDesc, IID_PPV_ARGS(ppPipeline));
}

HRESULT hook_create_compute_pipeline_state(ID3D12Device* pDevice, const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, void*, uint32_t,
                                           ID3D12PipelineState** ppPipeline)
{
    return pDevice->CreateComputePipelineState(pDesc, IID_PPV_ARGS(ppPipeline));
}

void hook_remove_pipeline(Pipeline* pPipeline) { SAFE_RELEASE(pPipeline->mDx.pPipelineState); }

HRESULT hook_add_special_resource(Renderer* pRenderer, const D3D12_RESOURCE_DESC* pDesc, const D3D12_CLEAR_VALUE*,
                                  D3D12_RESOURCE_STATES state, uint32_t flags, Buffer* pBuffer)
{
    if (!(flags & BUFFER_CREATION_FLAG_MARKER))
    {
        return E_NOINTERFACE;
    }
    // CreatePlacedResource requires 64KB granularity for D3D12_RESOURCE_DIMENSION_BUFFER resources
    const size_t requiredResourceHeapAlign = 64 * TF_KB;
    size_t       requiredDataSize = pDesc->Width;
    size_t       bufferSize = round_up_64(requiredDataSize, requiredResourceHeapAlign);
    void*        allocatedData = VirtualAlloc(nullptr, bufferSize, MEM_COMMIT, PAGE_READWRITE);
    if (!allocatedData)
    {
        return E_NOINTERFACE;
    }

    memset(allocatedData, 0xFF, requiredDataSize);
    if (bufferSize > requiredDataSize)
    {
        memset((uint8_t*)allocatedData + requiredDataSize, 0xCF, bufferSize - requiredDataSize);
    }
    ID3D12Device3* device = NULL;
    HRESULT        hres = pRenderer->mDx.pDevice->QueryInterface(&device);
    if (!SUCCEEDED(hres))
    {
        VirtualFree(allocatedData, 0, MEM_DECOMMIT);
        return E_NOINTERFACE;
    }
    ID3D12Heap* heap = NULL;
    hres = device->OpenExistingHeapFromAddress(allocatedData, IID_PPV_ARGS(&heap));
    ASSERT(SUCCEEDED(hres));
    D3D12_RESOURCE_DESC desc = *pDesc;
    desc.Alignment = 0;
    desc.Width = bufferSize;
    desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
    ASSERT(D3D12_RESOURCE_STATE_COPY_DEST == state);
    hres = device->CreatePlacedResource(heap, 0, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pBuffer->mDx.pResource));
    SAFE_RELEASE(device);
    ASSERT(SUCCEEDED(hres));
    pBuffer->mDx.pMarkerBufferHeap = heap;
    pBuffer->mDx.mMarkerBuffer = true;
    return S_OK;
}

HRESULT hook_add_special_resource(Renderer* pRenderer, const D3D12_RESOURCE_DESC* pDesc, const D3D12_CLEAR_VALUE* pClearValue,
                                  D3D12_RESOURCE_STATES startState, uint32_t flags, Texture* pOutTexture)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pDesc);
    UNREF_PARAM(startState);
    UNREF_PARAM(flags);
    UNREF_PARAM(pClearValue);
    UNREF_PARAM(pOutTexture);
    return E_NOINTERFACE;
}

HRESULT hook_add_placed_resource(Renderer* pRenderer, const ResourcePlacement* pPlacement, const D3D12_RESOURCE_DESC* pDesc,
                                 const D3D12_CLEAR_VALUE* pClearValue, D3D12_RESOURCE_STATES startState, ID3D12Resource** ppOutResource)
{
    return pRenderer->mDx.pDevice->CreatePlacedResource(pPlacement->pHeap->mDx.pHeap, pPlacement->mOffset, pDesc, startState, pClearValue,
                                                        IID_PPV_ARGS(ppOutResource));
}

static inline LONG util_compute_intersection(const RECT rect1, const RECT rect2)
{
    LONG x_intersection = max(0L, min(rect1.right, rect2.right) - max(rect1.left, rect2.left));
    LONG y_intersection = max(0L, min(rect1.bottom, rect2.bottom) - max(rect1.top, rect2.top));
    LONG intersection = x_intersection * y_intersection;
    return intersection;
}

TinyImageFormat hook_get_recommended_swapchain_format(Renderer* pRenderer, const SwapChainDesc* pDesc, ColorSpace colorSpace)
{
    if (COLOR_SPACE_SDR_LINEAR == colorSpace)
        return TinyImageFormat_B8G8R8A8_UNORM;
    if (COLOR_SPACE_SDR_SRGB == colorSpace)
        return TinyImageFormat_B8G8R8A8_SRGB;

    TinyImageFormat format = TinyImageFormat_UNDEFINED;
    if (COLOR_SPACE_P2020 == colorSpace)
        format = TinyImageFormat_R10G10B10A2_UNORM;
    if (COLOR_SPACE_EXTENDED_SRGB == colorSpace)
        format = TinyImageFormat_R16G16B16A16_SFLOAT;

    // check adapter
    RECT windowBounds = {};
    GetWindowRect((HWND)pDesc->mWindowHandle.window, &windowBounds);
    IDXGIAdapter1* dxgiAdapter = NULL;
    CHECK_HRESULT(pRenderer->pContext->mDx.pDXGIFactory->EnumAdapters1(0, &dxgiAdapter));
    UINT         i = 0;
    IDXGIOutput* currentOutput = NULL;
    IDXGIOutput* bestOutput = NULL;
    LONG         bestIntersection = -1;
    while (dxgiAdapter->EnumOutputs(i, &currentOutput) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_OUTPUT_DESC desc;
        currentOutput->GetDesc(&desc);
        LONG intersection = util_compute_intersection(windowBounds, desc.DesktopCoordinates);
        if (intersection > bestIntersection)
        {
            SAFE_RELEASE(bestOutput);
            bestIntersection = intersection;
            bestOutput = currentOutput;
        }
        else
        {
            SAFE_RELEASE(currentOutput);
        }
        ++i;
    }
    SAFE_RELEASE(dxgiAdapter);

    IDXGIOutput6* output6;
    CHECK_HRESULT(bestOutput->QueryInterface(IID_PPV_ARGS(&output6)));
    DXGI_OUTPUT_DESC1 desc1;
    CHECK_HRESULT(output6->GetDesc1(&desc1));
    SAFE_RELEASE(output6);
    SAFE_RELEASE(bestOutput);
    const bool outputSupportsHDR = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 == desc1.ColorSpace;
    if (outputSupportsHDR)
    {
        // check swapchain
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = 1;
        desc.Height = 1;
        desc.Format = util_to_dx12_swapchain_format(format);
        desc.Stereo = false;
        desc.SampleDesc.Count = 1; // If multisampling is needed, we'll resolve it later
        desc.SampleDesc.Quality = 0;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 3;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        desc.Flags = 0;

        IDXGISwapChain1* tmpSwapchain1;
        Queue*           pQueue = pDesc->ppPresentQueues[0];
        CHECK_HRESULT(
            pRenderer->pContext->mDx.pDXGIFactory->CreateSwapChainForComposition(pQueue->mDx.pQueue, &desc, NULL, &tmpSwapchain1));

        IDXGISwapChain3* tmpSwapchain3;
        CHECK_HRESULT(tmpSwapchain1->QueryInterface(IID_PPV_ARGS(&tmpSwapchain3)));
        UINT colorSpaceSupport = 0;
        CHECK_HRESULT(tmpSwapchain3->CheckColorSpaceSupport(util_to_dx12_colorspace(colorSpace), &colorSpaceSupport));
        tmpSwapchain1->Release();

        if (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)
        {
            return format;
        }
    }

    return TinyImageFormat_UNDEFINED;
}

uint32_t hook_get_swapchain_image_index(SwapChain* pSwapChain) { return pSwapChain->mDx.pSwapChain->GetCurrentBackBufferIndex(); }

HRESULT hook_acquire_next_image(ID3D12Device*, SwapChain*) { return S_OK; }

HRESULT hook_queue_present(Queue*, SwapChain* pSwapChain, uint32_t)
{
    return pSwapChain->mDx.pSwapChain->Present(pSwapChain->mDx.mSyncInterval, pSwapChain->mDx.mFlags);
}

void hook_dispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    pCmd->mDx.pCmdList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

HRESULT hook_signal(Queue* pQueue, ID3D12Fence* pFence, uint64_t fenceValue) { return pQueue->mDx.pQueue->Signal(pFence, fenceValue); }

HRESULT hook_signal_flush(Queue* pQueue, ID3D12Fence* pFence, uint64_t fenceValue) { return hook_signal(pQueue, pFence, fenceValue); }

extern void hook_fill_gpu_desc(ID3D12Device* pDevice, D3D_FEATURE_LEVEL featureLevel, GpuDesc* pInOutDesc)
{
    // Query the level of support of Shader Model.
    D3D12_FEATURE_DATA_D3D12_OPTIONS  featureData = {};
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 featureData1 = {};
    // Query the level of support of Wave Intrinsics.
    pDevice->CheckFeatureSupport((D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS, &featureData, sizeof(featureData));
    pDevice->CheckFeatureSupport((D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &featureData1, sizeof(featureData1));

    GpuDesc&           gpuDesc = *pInOutDesc;
    DXGI_ADAPTER_DESC3 desc = {};
    gpuDesc.pGpu->GetDesc3(&desc);

    gpuDesc.mMaxSupportedFeatureLevel = featureLevel;
    gpuDesc.mDedicatedVideoMemory = desc.DedicatedVideoMemory;
    gpuDesc.mFeatureDataOptions = featureData;
    gpuDesc.mFeatureDataOptions1 = featureData1;

    // save vendor and model Id as string
    // char hexChar[10];
    // convert deviceId and assign it
    gpuDesc.mDeviceId = desc.DeviceId;
    // convert modelId and assign it
    gpuDesc.mVendorId = desc.VendorId;
    // convert Revision Id
    gpuDesc.mRevisionId = desc.Revision;

    // save gpu name (Some situtations this can show description instead of name)
    // char sName[MAX_PATH];
    size_t numConverted = 0;
    wcstombs_s(&numConverted, gpuDesc.mName, desc.Description, MAX_GPU_VENDOR_STRING_LENGTH);
}

void hook_modify_descriptor_heap_size(D3D12_DESCRIPTOR_HEAP_TYPE, uint32_t*) {}

void hook_modify_swapchain_desc(DXGI_SWAP_CHAIN_DESC1*) {}

void hook_modify_heap_flags(DescriptorType, D3D12_HEAP_FLAGS*) {}

void hook_modify_buffer_resource_desc(const BufferDesc*, D3D12_RESOURCE_DESC*) {}

void hook_modify_texture_resource_flags(TextureCreationFlags, D3D12_RESOURCE_FLAGS*) {}

void hook_modify_shader_compile_flags(uint32_t, bool, const WCHAR**, uint32_t* pOutNumFlags) { *pOutNumFlags = 0; }

void hook_modify_rootsignature_flags(uint32_t, D3D12_ROOT_SIGNATURE_FLAGS*) {}

void hook_modify_command_signature_desc(D3D12_COMMAND_SIGNATURE_DESC* pInOutDesc, uint32_t padding) { pInOutDesc->ByteStride += padding; }

void hook_pre_resolve_query(Cmd* pCmd) { UNREF_PARAM(pCmd); }
#endif
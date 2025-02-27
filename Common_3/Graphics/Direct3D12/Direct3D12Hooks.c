/*
 * Copyright (c) 2017-2025 The Forge Interactive Inc.
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

//#include "../ThirdParty/OpenSource/D3D12MemoryAllocator/Direct3D12MemoryAllocator.h"

#include "../Interfaces/IGraphics.h"

// #include "../../../Common_3/Utilities/Math/MathTypes.h"

#include "Direct3D12Hooks.h"

#include "../../Utilities/Interfaces/IMemory.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p_var)       \
    if (p_var)                    \
    {                             \
        COM_CALL(Release, p_var); \
        p_var = NULL;             \
    }
#endif

extern uint64_t              round_up_64(uint64_t a, uint64_t b);
extern DXGI_FORMAT           util_to_dx12_swapchain_format(TinyImageFormat format);
extern DXGI_COLOR_SPACE_TYPE util_to_dx12_colorspace(ColorSpace colorspace);

HMODULE hook_get_d3d12_module_handle() { return GetModuleHandle(TEXT("d3d12.dll")); }

void hook_post_init_renderer(Renderer* pRenderer) { UNREF_PARAM(pRenderer); }

void hook_post_remove_renderer(Renderer* pRenderer) { UNREF_PARAM(pRenderer); }

HRESULT WINAPI d3d12dll_DXGIGetDebugInterface1(UINT Flags, REFIID riid, void** ppFactory);

void hook_enable_debug_layer(const RendererContextDesc* pDesc, RendererContext* pContext)
{
    UNREF_PARAM(pDesc);
    UNREF_PARAM(pContext);
#if defined(ENABLE_GRAPHICS_VALIDATION)
    COM_CALL(EnableDebugLayer, pContext->mDx.pDebug);

    ID3D12Debug1* pDebug1 = NULL;
    if (SUCCEEDED(COM_CALL(QueryInterface, pContext->mDx.pDebug, IID_ARGS(ID3D12Debug1, &pDebug1))))
    {
        COM_CALL(SetEnableGPUBasedValidation, pDebug1, pDesc->mEnableGpuBasedValidation);
        COM_CALL(Release, pDebug1);
    }
#endif
}

HRESULT WINAPI d3d12dll_CreateDevice(void* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice);

HRESULT hook_create_device(void* pAdapter, D3D_FEATURE_LEVEL featureLevel, bool setupFrameEvent, ID3D12Device** ppDevice)
{
    UNREF_PARAM(setupFrameEvent);
    return d3d12dll_CreateDevice(pAdapter, featureLevel, IID_ARGS(ID3D12Device, ppDevice));
}

HRESULT hook_create_command_queue(ID3D12Device* pDevice, const D3D12_COMMAND_QUEUE_DESC* pDesc, ID3D12CommandQueue** ppQueue)
{
    return COM_CALL(CreateCommandQueue, pDevice, pDesc, IID_ARGS(ID3D12CommandQueue, ppQueue));
}

HRESULT hook_create_graphics_pipeline_state(ID3D12Device* pDevice, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, void* pData,
                                            uint32_t size, ID3D12PipelineState** ppPipeline)
{
    UNREF_PARAM(pData);
    UNREF_PARAM(size);
    return COM_CALL(CreateGraphicsPipelineState, pDevice, pDesc, IID_ARGS(ID3D12PipelineState, ppPipeline));
}

HRESULT hook_create_compute_pipeline_state(ID3D12Device* pDevice, const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, void* pData,
                                           uint32_t size, ID3D12PipelineState** ppPipeline)
{
    UNREF_PARAM(pData);
    UNREF_PARAM(size);
    return COM_CALL(CreateComputePipelineState, pDevice, pDesc, IID_ARGS(ID3D12PipelineState, ppPipeline));
}

void hook_remove_pipeline(Pipeline* pPipeline)
{
#if defined(ENABLE_WORKGRAPH)
    if (PIPELINE_TYPE_WORKGRAPH == pPipeline->mDx.mType)
    {
        SAFE_RELEASE(pPipeline->mDx.pStateObject);
    }
    else
#endif
    {
        SAFE_RELEASE(pPipeline->mDx.pPipelineState);
    }
}

HRESULT hook_add_special_buffer(Renderer* pRenderer, const D3D12_RESOURCE_DESC* pDesc, const D3D12_CLEAR_VALUE* pClearValue,
                                D3D12_RESOURCE_STATES state, uint32_t flags, Buffer* pBuffer)
{
    UNREF_PARAM(pClearValue);
    if (!(flags & BUFFER_CREATION_FLAG_MARKER))
    {
        return E_NOINTERFACE;
    }
    // CreatePlacedResource requires 64KB granularity for D3D12_RESOURCE_DIMENSION_BUFFER resources
    const size_t requiredResourceHeapAlign = 64 * TF_KB;
    size_t       requiredDataSize = pDesc->Width;
    size_t       bufferSize = round_up_64(requiredDataSize, requiredResourceHeapAlign);
    void*        allocatedData = VirtualAlloc(NULL, bufferSize, MEM_COMMIT, PAGE_READWRITE);
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
    HRESULT        hres = COM_CALL(QueryInterface, pRenderer->mDx.pDevice, IID_ARGS(ID3D12Device3, &device));
    if (!SUCCEEDED(hres))
    {
        VirtualFree(allocatedData, 0, MEM_DECOMMIT);
        return E_NOINTERFACE;
    }
    ID3D12Heap* heap = NULL;
    hres = COM_CALL(OpenExistingHeapFromAddress, device, allocatedData, IID_ARGS(ID3D12Heap, &heap));
    ASSERT(SUCCEEDED(hres));
    D3D12_RESOURCE_DESC desc = *pDesc;
    desc.Alignment = 0;
    desc.Width = bufferSize;
    desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
    ASSERT(D3D12_RESOURCE_STATE_COPY_DEST == state);
    hres = COM_CALL(CreatePlacedResource, device, heap, 0, &desc, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                    IID_ARGS(ID3D12Resource, &pBuffer->mDx.pResource));
    SAFE_RELEASE(device);
    ASSERT(SUCCEEDED(hres));
    pBuffer->mDx.pMarkerBufferHeap = heap;
    pBuffer->mDx.mMarkerBuffer = true;
    return S_OK;
}

HRESULT hook_add_special_texture(Renderer* pRenderer, const D3D12_RESOURCE_DESC* pDesc, const D3D12_CLEAR_VALUE* pClearValue,
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
    return COM_CALL(CreatePlacedResource, pRenderer->mDx.pDevice, pPlacement->pHeap->mDx.pHeap, pPlacement->mOffset, pDesc, startState,
                    pClearValue, IID_ARGS(ID3D12Resource, ppOutResource));
}

static inline LONG util_compute_intersection(const RECT rect1, const RECT rect2)
{
    LONG x_intersection = max(0L, min(rect1.right, rect2.right) - max(rect1.left, rect2.left));
    LONG y_intersection = max(0L, min(rect1.bottom, rect2.bottom) - max(rect1.top, rect2.top));
    LONG intersection = x_intersection * y_intersection;
    return intersection;
}

EXTERN_C const GUID IID_IDXGIOutput6_Copy;

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
    RECT windowBounds = { 0 };
    GetWindowRect((HWND)pDesc->mWindowHandle.window, &windowBounds);
    IDXGIAdapter1* dxgiAdapter = NULL;
    CHECK_HRESULT(COM_CALL(EnumAdapters1, pRenderer->pContext->mDx.pDXGIFactory, 0, &dxgiAdapter));
    UINT         i = 0;
    IDXGIOutput* currentOutput = NULL;
    IDXGIOutput* bestOutput = NULL;
    LONG         bestIntersection = -1;
    while (COM_CALL(EnumOutputs, dxgiAdapter, i, &currentOutput) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_OUTPUT_DESC desc;
        COM_CALL(GetDesc, currentOutput, &desc);
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
    CHECK_HRESULT(COM_CALL(QueryInterface, bestOutput, IID_REF(IDXGIOutput6_Copy), &output6));
    DXGI_OUTPUT_DESC1 desc1;
    CHECK_HRESULT(COM_CALL(GetDesc1, output6, &desc1));
    SAFE_RELEASE(output6);
    SAFE_RELEASE(bestOutput);
    const bool outputSupportsHDR = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 == desc1.ColorSpace;
    if (outputSupportsHDR)
    {
        // check swapchain
        DXGI_SWAP_CHAIN_DESC1 desc = {
            .Width = 1,
            .Height = 1,
            .Format = util_to_dx12_swapchain_format(format),
            .Stereo = false,
            .SampleDesc.Count = 1, // If multisampling is needed, we'll resolve it later
            .SampleDesc.Quality = 0,
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = 3,
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
        };
        IDXGISwapChain1* tmpSwapchain1;
        Queue*           pQueue = pDesc->ppPresentQueues[0];
        CHECK_HRESULT(COM_CALL(CreateSwapChainForComposition, pRenderer->pContext->mDx.pDXGIFactory, // -V1027
                               (IUnknown*)pQueue->mDx.pQueue,                                        // -V1027
                               &desc, NULL, &tmpSwapchain1));

        IDXGISwapChain3* tmpSwapchain3;
        CHECK_HRESULT(COM_CALL(QueryInterface, tmpSwapchain1, IID_ARGS(IDXGISwapChain3, &tmpSwapchain3)));
        UINT colorSpaceSupport = 0;
        CHECK_HRESULT(COM_CALL(CheckColorSpaceSupport, tmpSwapchain3, util_to_dx12_colorspace(colorSpace), &colorSpaceSupport));
        COM_CALL(Release, tmpSwapchain1);

        if (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)
        {
            return format;
        }
    }

    return TinyImageFormat_UNDEFINED;
}

uint32_t hook_get_swapchain_image_index(SwapChain* pSwapChain) { return COM_CALL(GetCurrentBackBufferIndex, pSwapChain->mDx.pSwapChain); }

HRESULT hook_acquire_next_image(ID3D12Device* pDevice, SwapChain* pSwapChain)
{
    UNREF_PARAM(pDevice);
    UNREF_PARAM(pSwapChain);
    return S_OK;
}

HRESULT hook_queue_present(Queue* pQueue, SwapChain* pSwapChain, uint32_t imageIndex)
{
    UNREF_PARAM(pQueue);
    UNREF_PARAM(imageIndex);
    return COM_CALL(Present, pSwapChain->mDx.pSwapChain, pSwapChain->mDx.mSyncInterval, pSwapChain->mDx.mFlags);
}

void hook_dispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    COM_CALL(Dispatch, pCmd->mDx.pCmdList, groupCountX, groupCountY, groupCountZ);
}

HRESULT hook_signal(Queue* pQueue, ID3D12Fence* pFence, uint64_t fenceValue)
{
    return COM_CALL(Signal, pQueue->mDx.pQueue, pFence, fenceValue);
}

HRESULT hook_wait(Queue* pQueue, ID3D12Fence* pFence, uint64_t fenceValue)
{
    return COM_CALL(Wait, pQueue->mDx.pQueue, pFence, fenceValue);
}

HRESULT hook_signal_flush(Queue* pQueue, ID3D12Fence* pFence, uint64_t fenceValue) { return hook_signal(pQueue, pFence, fenceValue); }

extern void hook_fill_gpu_desc(ID3D12Device* pDevice, D3D_FEATURE_LEVEL featureLevel, DXGPUInfo* pInOutDesc)
{
    // Query the level of support of Shader Model.
    D3D12_FEATURE_DATA_D3D12_OPTIONS  featureData = { 0 };
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 featureData1 = { 0 };
    // Query the level of support of Wave Intrinsics.
    COM_CALL(CheckFeatureSupport, pDevice, (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS, &featureData, sizeof(featureData));
    COM_CALL(CheckFeatureSupport, pDevice, (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &featureData1, sizeof(featureData1));

    DXGPUInfo*         pGpuDesc = pInOutDesc;
    DXGI_ADAPTER_DESC3 desc = { 0 };
    COM_CALL(GetDesc3, pGpuDesc->pGpu, &desc);

    pGpuDesc->mMaxSupportedFeatureLevel = featureLevel;
    pGpuDesc->mDedicatedVideoMemory = desc.DedicatedVideoMemory;
    pGpuDesc->mFeatureDataOptions = *(const D3D12_FEATURE_DATA_D3D12_OPTIONS_EX*)&featureData;
    pGpuDesc->mFeatureDataOptions1 = featureData1;

    // save vendor and model Id as string
    // char hexChar[10];
    // convert deviceId and assign it
    pGpuDesc->mDeviceId = desc.DeviceId;
    // convert modelId and assign it
    pGpuDesc->mVendorId = desc.VendorId;
    // convert Revision Id
    pGpuDesc->mRevisionId = desc.Revision;

    // save gpu name (Some situtations this can show description instead of name)
    // char sName[MAX_PATH];
    size_t numConverted = 0;
    wcstombs_s(&numConverted, pGpuDesc->mName, MAX_GPU_VENDOR_STRING_LENGTH, desc.Description, MAX_GPU_VENDOR_STRING_LENGTH);
}

void hook_modify_descriptor_heap_size(D3D12_DESCRIPTOR_HEAP_TYPE a, uint32_t* b)
{
    UNREF_PARAM(a);
    UNREF_PARAM(b);
}

void hook_modify_swapchain_desc(DXGI_SWAP_CHAIN_DESC1* a) { UNREF_PARAM(a); }

void hook_modify_heap_flags(DescriptorType a, D3D12_HEAP_FLAGS* b)
{
    UNREF_PARAM(a);
    UNREF_PARAM(b);
}

void hook_modify_buffer_resource_desc(const BufferDesc* a, D3D12_RESOURCE_DESC* b)
{
    UNREF_PARAM(a);
    UNREF_PARAM(b);
}

void hook_modify_texture_resource_flags(TextureCreationFlags a, D3D12_RESOURCE_FLAGS* b)
{
    UNREF_PARAM(a);
    UNREF_PARAM(b);
}

void hook_modify_shader_compile_flags(uint32_t a, bool b, const WCHAR** c, uint32_t* pOutNumFlags)
{
    UNREF_PARAM(a);
    UNREF_PARAM(b);
    UNREF_PARAM(c);
    *pOutNumFlags = 0;
}

void hook_modify_rootsignature_flags(uint32_t a, D3D12_ROOT_SIGNATURE_FLAGS* b)
{
    UNREF_PARAM(a);
    UNREF_PARAM(b);
}

void hook_fill_dispatch_indirect_argument_desc(D3D12_INDIRECT_ARGUMENT_DESC* pInOutDesc, bool async)
{
    UNREF_PARAM(async);
    pInOutDesc->Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
}
void hook_modify_command_signature_desc(D3D12_COMMAND_SIGNATURE_DESC* pInOutDesc, uint32_t padding) { pInOutDesc->ByteStride += padding; }

void hook_pre_resolve_query(Cmd* pCmd) { UNREF_PARAM(pCmd); }

void hook_fill_occlusion_query(Cmd* pCmd, uint32_t queryIndex)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(queryIndex);
}

extern const D3D12_COMMAND_LIST_TYPE gDx12CmdTypeTranslator[MAX_QUEUE_TYPE];
HRESULT hook_create_cmd(ID3D12Device* pDevice, QueueType queueType, uint32_t nodeMask, ID3D12CommandAllocator* pAlloc, Cmd* pCmd)
{
    return COM_CALL(CreateCommandList, pDevice, nodeMask, gDx12CmdTypeTranslator[queueType], pAlloc, NULL,
                    IID_ARGS(ID3D12GraphicsCommandList1, &pCmd->mDx.pCmdList));
}

void hook_remove_cmd(Cmd* pCmd) { COM_RELEASE(pCmd->mDx.pCmdList); }

void hook_resource_barrier(Cmd* pCmd, UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers)
{
    COM_CALL(ResourceBarrier, pCmd->mDx.pCmdList, NumBarriers, pBarriers);
}

void hook_copy_buffer_region(Cmd* pCmd, ID3D12Resource* pDstBuffer, UINT64 DstOffset, ID3D12Resource* pSrcBuffer, UINT64 SrcOffset,
                             UINT64 NumBytes)
{
    COM_CALL(CopyBufferRegion, pCmd->mDx.pCmdList, pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, NumBytes);
}

void hook_copy_texture_region(Cmd* pCmd, const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT DstX, UINT DstY, UINT DstZ,
                              const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox)
{
    COM_CALL(CopyTextureRegion, pCmd->mDx.pCmdList, pDst, DstX, DstY, DstZ, pSrc, pSrcBox);
}

HRESULT hook_GetTimestampFrequency(_Inout_ ID3D12CommandQueue* pQueue, _Out_ UINT64* pFreq)
{
    return COM_CALL(GetTimestampFrequency, pQueue, pFreq);
}

HRESULT hook_CreatePipelineLibrary(ID3D12Device1* This, _In_reads_(BlobLength) const void* pLibraryBlob, SIZE_T BlobLength, REFIID riid,
                                   _COM_Outptr_ void** ppPipelineLibrary)
{
    return COM_CALL(CreatePipelineLibrary, This, pLibraryBlob, BlobLength, riid, ppPipelineLibrary);
}

void hook_WriteBufferImmediate(ID3D12GraphicsCommandList2* This, UINT Count,
                               _In_reads_(Count) const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER* pParams,
                               _In_reads_opt_(Count) const D3D12_WRITEBUFFERIMMEDIATE_MODE*  pModes)
{
    COM_CALL(WriteBufferImmediate, This, Count, pParams, pModes);
}

void hook_SetDescriptorHeaps(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ UINT NumDescriptorHeaps,
                             _In_reads_(NumDescriptorHeaps) ID3D12DescriptorHeap* const* pDescriptorHeaps)
{
    COM_CALL(SetDescriptorHeaps, pCommandList, NumDescriptorHeaps, pDescriptorHeaps);
}
void hook_SetComputeRootSignature(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ ID3D12RootSignature* pRootSignature)
{
    COM_CALL(SetComputeRootSignature, pCommandList, pRootSignature);
}

void hook_SetGraphicsRootSignature(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ ID3D12RootSignature* pRootSignature)
{
    COM_CALL(SetGraphicsRootSignature, pCommandList, pRootSignature);
}

void hook_SetComputeRootDescriptorTable(_Inout_ ID3D12GraphicsCommandList1* pCommandList, UINT RootParameterIndex,
                                        _In_ D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
    COM_CALL(SetComputeRootDescriptorTable, pCommandList, RootParameterIndex, BaseDescriptor);
}

void hook_SetGraphicsRootDescriptorTable(_Inout_ ID3D12GraphicsCommandList1* pCommandList, UINT RootParameterIndex,
                                         _In_ D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
    COM_CALL(SetGraphicsRootDescriptorTable, pCommandList, RootParameterIndex, BaseDescriptor);
}

void hook_SetComputeRoot32BitConstants(_Inout_ ID3D12GraphicsCommandList1* pCommandList, UINT RootParameterIndex,
                                       _In_ UINT Num32BitValuesToSet, _In_reads_(Num32BitValuesToSet * sizeof(UINT)) const void* pSrcData,
                                       _In_ UINT DestOffsetIn32BitValues)
{
    COM_CALL(SetComputeRoot32BitConstants, pCommandList, RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}

void hook_SetGraphicsRoot32BitConstants(_Inout_ ID3D12GraphicsCommandList1* pCommandList, UINT RootParameterIndex,
                                        _In_ UINT Num32BitValuesToSet, _In_reads_(Num32BitValuesToSet * sizeof(UINT)) const void* pSrcData,
                                        _In_ UINT DestOffsetIn32BitValues)
{
    COM_CALL(SetGraphicsRoot32BitConstants, pCommandList, RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}

void hook_SetComputeRootConstantBufferView(_Inout_ ID3D12GraphicsCommandList1* pCommandList, UINT RootParameterIndex,
                                           _In_ D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
    COM_CALL(SetComputeRootConstantBufferView, pCommandList, RootParameterIndex, BufferLocation);
}

void hook_SetGraphicsRootConstantBufferView(_Inout_ ID3D12GraphicsCommandList1* pCommandList, UINT RootParameterIndex,
                                            _In_ D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
    COM_CALL(SetGraphicsRootConstantBufferView, pCommandList, RootParameterIndex, BufferLocation);
}

void hook_OMSetRenderTargets(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ UINT NumRenderTargetDescriptors,
                             _In_ const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors, _In_ BOOL RTsSingleHandleToDescriptorRange,
                             _In_opt_ const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor)
{
    COM_CALL(OMSetRenderTargets, pCommandList, NumRenderTargetDescriptors, pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange,
             pDepthStencilDescriptor);
}

void hook_ClearDepthStencilView(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView,
                                _In_ D3D12_CLEAR_FLAGS ClearFlags, _In_ FLOAT Depth, _In_ UINT8 Stencil, _In_ UINT NumRects,
                                _In_reads_(NumRects) const D3D12_RECT* pRect)
{
    COM_CALL(ClearDepthStencilView, pCommandList, DepthStencilView, ClearFlags, Depth, Stencil, NumRects, pRect);
}

void hook_ClearRenderTargetView(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView,
                                _In_ const FLOAT ColorRGBA[4], _In_ UINT NumRects, _In_reads_(NumRects) const D3D12_RECT* pRects)
{
    COM_CALL(ClearRenderTargetView, pCommandList, RenderTargetView, ColorRGBA, NumRects, pRects);
}

void hook_IASetIndexBuffer(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_opt_ const D3D12_INDEX_BUFFER_VIEW* pView)
{
    COM_CALL(IASetIndexBuffer, pCommandList, pView);
}

void hook_IASetVertexBuffers(_Inout_ ID3D12GraphicsCommandList1*                               pCommandList,
                             _In_range_(0, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1) UINT StartSlot,
                             _In_range_(1, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT) UINT     NumViews,
                             _In_reads_opt_(NumViews) const D3D12_VERTEX_BUFFER_VIEW*          pViews)
{
    COM_CALL(IASetVertexBuffers, pCommandList, StartSlot, NumViews, pViews);
}

void hook_IASetPrimitiveTopology(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
    COM_CALL(IASetPrimitiveTopology, pCommandList, PrimitiveTopology);
}

void hook_RSSetViewports(_Inout_ ID3D12GraphicsCommandList1*                                          pCommandList,
                         _In_range_(0, D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE) UINT Count,
                         _In_reads_(Count) const D3D12_VIEWPORT*                                      pViewports)
{
    COM_CALL(RSSetViewports, pCommandList, Count, pViewports);
}

void hook_RSSetScissorRects(_Inout_ ID3D12GraphicsCommandList1*                                          pCommandList,
                            _In_range_(0, D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE) UINT Count,
                            _In_reads_(Count) const D3D12_RECT*                                          pRects)
{
    COM_CALL(RSSetScissorRects, pCommandList, Count, pRects);
}

void hook_OMSetStencilRef(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ UINT StencilRef)
{
    COM_CALL(OMSetStencilRef, pCommandList, StencilRef);
}

void hook_SetPipelineState(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ ID3D12PipelineState* pPipelineState)
{
    COM_CALL(SetPipelineState, pCommandList, pPipelineState);
}

void hook_ExecuteIndirect(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ ID3D12CommandSignature* pCommandSignature,
                          _In_ UINT MaxCommandCount, _In_ ID3D12Resource* pArgumentBuffer, _In_ UINT64 ArgumentBufferOffset,
                          _In_opt_ ID3D12Resource* pCountBuffer, _In_ UINT64 CountBufferOffset)
{
    COM_CALL(ExecuteIndirect, pCommandList, pCommandSignature, MaxCommandCount, pArgumentBuffer, ArgumentBufferOffset, pCountBuffer,
             CountBufferOffset);
}

void hook_SetSamplePositions(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ UINT NumSamplesPerPixel, _In_ UINT NumPixels,
                             _In_reads_(NumSamplesPerPixel* NumPixels) D3D12_SAMPLE_POSITION* pSamplePositions)
{
    COM_CALL(SetSamplePositions, pCommandList, NumSamplesPerPixel, NumPixels, pSamplePositions);
}

void hook_ResourceBarrier(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ UINT NumBarriers,
                          _In_reads_(NumBarriers) const D3D12_RESOURCE_BARRIER* pBarriers)
{
    COM_CALL(ResourceBarrier, pCommandList, NumBarriers, pBarriers);
}

void hook_BeginQuery(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ ID3D12QueryHeap* pQueryHeap, _In_ D3D12_QUERY_TYPE Type,
                     _In_ UINT Index)
{
    COM_CALL(BeginQuery, pCommandList, pQueryHeap, Type, Index);
}

void hook_EndQuery(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ ID3D12QueryHeap* pQueryHeap, _In_ D3D12_QUERY_TYPE Type,
                   _In_ UINT Index)
{
    COM_CALL(EndQuery, pCommandList, pQueryHeap, Type, Index);
}

void hook_ResolveQueryData(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ ID3D12QueryHeap* pQueryHeap, _In_ D3D12_QUERY_TYPE Type,
                           _In_ UINT StartElement, _In_ UINT ElementCount, _In_ ID3D12Resource* pDestinationBuffer,
                           _In_ UINT64 AlignedDestinationBufferOffset)
{
    COM_CALL(ResolveQueryData, pCommandList, pQueryHeap, Type, StartElement, ElementCount, pDestinationBuffer,
             AlignedDestinationBufferOffset);
}
#if defined(D3D12_WORKGRAPH_AVAILABLE)

HRESULT hook_CreateStateObject(ID3D12Device5* This, const D3D12_STATE_OBJECT_DESC* pDesc, REFIID riid, _COM_Outptr_ void** ppStateObject)
{
    return COM_CALL(CreateStateObject, This, pDesc, riid, ppStateObject);
}

#endif

#if defined(D3D12_RAYTRACING_AVAILABLE)

void hook_GetRaytracingAccelerationStructurePrebuildInfo(ID3D12Device5* This,
                                                         _In_ const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS* pDesc,
                                                         _Out_ D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO* pInfo)
{
    COM_CALL(GetRaytracingAccelerationStructurePrebuildInfo, This, pDesc, pInfo);
}

void hook_BuildRaytracingAccelerationStructure(ID3D12GraphicsCommandList4* This,
                                               _In_ const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC* pDesc,
                                               _In_ UINT                                                      NumPostbuildInfoDescs,
                                               _In_reads_opt_(NumPostbuildInfoDescs)
                                                   const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* pPostbuildInfoDescs)
{
    COM_CALL(BuildRaytracingAccelerationStructure, This, pDesc, NumPostbuildInfoDescs, pPostbuildInfoDescs);
}

#endif

#if defined(DRED)

HRESULT hook_GetAutoBreadcrumbsOutput(ID3D12DeviceRemovedExtendedData* This, _Out_ D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT* pOutput)
{
    return COM_CALL(GetAutoBreadcrumbsOutput, This, pOutput);
}

HRESULT hook_GetPageFaultAllocationOutput(ID3D12DeviceRemovedExtendedData* This, _Out_ D3D12_DRED_PAGE_FAULT_OUTPUT* pOutput)
{
    return COM_CALL(GetPageFaultAllocationOutput, This, pOutput);
}

#endif

#endif

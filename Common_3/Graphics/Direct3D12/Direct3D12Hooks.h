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

#pragma once

#include "../GraphicsConfig.h"

#ifdef DIRECT3D12
// PC/Xbox have different member offsets/alignments for `D3D12_FEATURE_DATA_D3D12_OPTIONS`.
// Since we can't just use the PC definition on Xbox (and vice versa), we need to rewrite
// the definition so it has the correct layout for both PC and Xbox and use that instead.
typedef struct D3D12_FEATURE_DATA_D3D12_OPTIONS_EX
{
    _Out_ BOOL                                  DoublePrecisionFloatShaderOps;
    _Out_ BOOL                                  OutputMergerLogicOp;
    _Out_ D3D12_SHADER_MIN_PRECISION_SUPPORT    MinPrecisionSupport;
    _Out_ D3D12_TILED_RESOURCES_TIER            TiledResourcesTier;
    _Out_ D3D12_RESOURCE_BINDING_TIER           ResourceBindingTier;
    _Out_ BOOL                                  PSSpecifiedStencilRefSupported;
    _Out_ BOOL                                  TypedUAVLoadAdditionalFormats;
    _Out_ BOOL                                  ROVsSupported;
    _Out_ D3D12_CONSERVATIVE_RASTERIZATION_TIER ConservativeRasterizationTier;
    _Out_ UINT                                  MaxGPUVirtualAddressBitsPerResource;
    _Out_ BOOL                                  StandardSwizzle64KBSupported;
#ifdef XBOX
    _Out_ int ASTCProfile; // D3D12_ASTC_PROFILE
#endif
    _Out_ D3D12_CROSS_NODE_SHARING_TIER CrossNodeSharingTier;
    _Out_ BOOL                          CrossAdapterRowMajorTextureSupported;
    _Out_ BOOL                          VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation;
    _Out_ D3D12_RESOURCE_HEAP_TIER      ResourceHeapTier;
} D3D12_FEATURE_DATA_D3D12_OPTIONS_EX;

typedef struct DXGPUInfo
{
#if defined(XBOX)
    IDXGIAdapter* pGpu;
#else
    IDXGIAdapter4* pGpu;
#endif
    D3D_FEATURE_LEVEL                   mMaxSupportedFeatureLevel;
    D3D12_FEATURE_DATA_D3D12_OPTIONS_EX mFeatureDataOptions;
    D3D12_FEATURE_DATA_D3D12_OPTIONS1   mFeatureDataOptions1;
    SIZE_T                              mDedicatedVideoMemory;
    uint32_t                            mVendorId;
    uint32_t                            mDeviceId;
    uint32_t                            mRevisionId;
    char                                mName[MAX_GPU_VENDOR_STRING_LENGTH];
    GPUPresetLevel                      mPreset;
} DXGPUInfo;

#ifdef __cplusplus
extern "C"
{
#endif

    extern HMODULE hook_get_d3d12_module_handle();

    extern void hook_post_init_renderer(Renderer* pRenderer);
    extern void hook_post_remove_renderer(Renderer* pRenderer);
    extern void hook_enable_debug_layer(const RendererContextDesc* pDesc, RendererContext* pRenderer);

    extern HRESULT hook_create_device(void* pAdapter, D3D_FEATURE_LEVEL featureLevel, bool setupFrameEvent, ID3D12Device** ppDevice);
    extern HRESULT hook_create_command_queue(ID3D12Device* pDevice, const D3D12_COMMAND_QUEUE_DESC* qDesc, ID3D12CommandQueue** ppQueue);
    extern HRESULT hook_create_cmd(ID3D12Device* pDevice, QueueType queueType, uint32_t nodeMask, ID3D12CommandAllocator* pAlloc,
                                   Cmd* pCmd);
    extern void    hook_remove_cmd(Cmd* pCmd);
    extern HRESULT hook_create_graphics_pipeline_state(ID3D12Device* pDevice, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc,
                                                       void* pExtensions, uint32_t extensionCout, ID3D12PipelineState** ppPipeline);
    extern HRESULT hook_create_compute_pipeline_state(ID3D12Device* pDevice, const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc,
                                                      void* pExtensions, uint32_t extensionCout, ID3D12PipelineState** ppPipeline);
    extern HRESULT hook_add_special_buffer(Renderer* pRenderer, const D3D12_RESOURCE_DESC* pDesc, const D3D12_CLEAR_VALUE* pClearValue,
                                           D3D12_RESOURCE_STATES startState, uint32_t flags, Buffer* pOutBuffer);
    extern HRESULT hook_add_special_texture(Renderer* pRenderer, const D3D12_RESOURCE_DESC* pDesc, const D3D12_CLEAR_VALUE* pClearValue,
                                            D3D12_RESOURCE_STATES startState, uint32_t flags, Texture* pOutTexture);
    extern HRESULT hook_add_placed_resource(Renderer* pRenderer, const ResourcePlacement* pPlacement, const D3D12_RESOURCE_DESC* pDesc,
                                            const D3D12_CLEAR_VALUE* pClearValue, D3D12_RESOURCE_STATES startState,
                                            ID3D12Resource** ppOutResource);
    // HRESULT create_swap_chain(struct Renderer* pRenderer, struct SwapChain* pSwapChain, DXGI_SWAP_CHAIN_DESC1* desc, IDXGISwapChain1**
    // swapchain);
    extern void    hook_remove_pipeline(Pipeline* pPipeline);

    extern TinyImageFormat hook_get_recommended_swapchain_format(Renderer* pRenderer, const SwapChainDesc* pDesc, ColorSpace colorSpace);
    extern uint32_t        hook_get_swapchain_image_index(SwapChain* pSwapChain);
    extern HRESULT         hook_acquire_next_image(ID3D12Device* pDevice, SwapChain* pSwapChain);
    extern HRESULT         hook_queue_present(Queue* pQueue, SwapChain* pSwapChain, uint32_t swapChainImageIndex);
    extern void            hook_dispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    extern HRESULT         hook_signal(Queue* pQueue, ID3D12Fence* pFence, uint64_t fenceValue);
    extern HRESULT         hook_wait(Queue* pQueue, ID3D12Fence* pFence, uint64_t fenceValue);
    extern HRESULT         hook_signal_flush(Queue* pQueue, ID3D12Fence* pFence, uint64_t fenceValue);

    extern void hook_fill_gpu_desc(ID3D12Device* pDevice, D3D_FEATURE_LEVEL featureLevel, DXGPUInfo* pInOutDesc);
    extern void hook_modify_descriptor_heap_size(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t* pInOutSize);
    extern void hook_modify_heap_flags(DescriptorType type, D3D12_HEAP_FLAGS* pInOutFlags);
    extern void hook_modify_buffer_resource_desc(const BufferDesc* pBuffer, D3D12_RESOURCE_DESC* pInOutDesc);
    extern void hook_modify_texture_resource_flags(TextureCreationFlags creationFlags, D3D12_RESOURCE_FLAGS* pInOutFlags);
    extern void hook_modify_shader_compile_flags(uint32_t stage, bool enablePrimitiveId, const WCHAR** pData, uint32_t* pInOutNumFlags);
    extern void hook_modify_rootsignature_flags(uint32_t stages, D3D12_ROOT_SIGNATURE_FLAGS* pInOutFlags);
    extern void hook_fill_dispatch_indirect_argument_desc(D3D12_INDIRECT_ARGUMENT_DESC* pInOutDesc, bool async);
    extern void hook_modify_command_signature_desc(D3D12_COMMAND_SIGNATURE_DESC* pInOutDesc, uint32_t padding);
    extern void hook_pre_resolve_query(Cmd* pCmd);
    extern void hook_fill_occlusion_query(Cmd* pCmd, uint32_t sampleCount);
    // Converts to platform-specific format when applicable, otherwise return DXGI_FORAMT_UNKOWN to let the caller handle common formats
    extern DXGI_FORMAT hook_to_dx12_srv_platform_format(DXGI_FORMAT defaultFormat, bool isStencil);

#if defined(XBOX)
    extern void hook_remove_device(ID3D12Device* pDevice);
    extern void hook_cmd_write_marker(Cmd*, const MarkerDesc*);
#endif

    // Smooth over differences between ID3D12GraphicsCommandList1/ID3D12XboxDmaCommandList
    extern void hook_resource_barrier(Cmd* pCmd, UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers);
    extern void hook_copy_buffer_region(Cmd* pCmd, ID3D12Resource* pDstBuffer, UINT64 DstOffset, ID3D12Resource* pSrcBuffer,
                                        UINT64 SrcOffset, UINT64 NumBytes);
    extern void hook_copy_texture_region(Cmd* pCmd, const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT DstX, UINT DstY, UINT DstZ,
                                         const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox);

    // Certain DX12 function pointers on PC/Xbox have different offsets (mostly in ID3D12GraphicsCommandList), so the differences are
    // handled here These functions are just wrappers that forward the function call to the correct DirectX function pointer depending on
    // the platform. For fast/easy manipulation, the signature of these functions is identical to the corresponding COM function, with the
    // only difference being the `hook_` prefix on the function name.
    extern HRESULT hook_GetTimestampFrequency(_Inout_ ID3D12CommandQueue* pQueue, _Out_ UINT64* pFreq);

    extern HRESULT hook_CreatePipelineLibrary(ID3D12Device1* This, _In_reads_(BlobLength) const void* pLibraryBlob, SIZE_T BlobLength,
                                              REFIID riid, _COM_Outptr_ void** ppPipelineLibrary);

    extern void hook_WriteBufferImmediate(ID3D12GraphicsCommandList2* This, UINT Count,
                                          _In_reads_(Count) const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER* pParams,
                                          _In_reads_opt_(Count) const D3D12_WRITEBUFFERIMMEDIATE_MODE*  pModes);

    extern void hook_SetDescriptorHeaps(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ UINT NumDescriptorHeaps,
                                        _In_reads_(NumDescriptorHeaps) ID3D12DescriptorHeap* const* pDescriptorHeaps);

    extern void hook_SetComputeRootSignature(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ ID3D12RootSignature* pRootSignature);

    extern void hook_SetGraphicsRootSignature(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ ID3D12RootSignature* pRootSignature);

    extern void hook_SetComputeRootDescriptorTable(_Inout_ ID3D12GraphicsCommandList1* pCommandList, UINT RootParameterIndex,
                                                   _In_ D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor);

    extern void hook_SetGraphicsRootDescriptorTable(_Inout_ ID3D12GraphicsCommandList1* pCommandList, UINT RootParameterIndex,
                                                    _In_ D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor);

    extern void hook_SetComputeRoot32BitConstants(_Inout_ ID3D12GraphicsCommandList1* pCommandList, UINT RootParameterIndex,
                                                  _In_ UINT                                                  Num32BitValuesToSet,
                                                  _In_reads_(Num32BitValuesToSet * sizeof(UINT)) const void* pSrcData,
                                                  _In_ UINT                                                  DestOffsetIn32BitValues);

    extern void hook_SetGraphicsRoot32BitConstants(_Inout_ ID3D12GraphicsCommandList1* pCommandList, UINT RootParameterIndex,
                                                   _In_ UINT                                                  Num32BitValuesToSet,
                                                   _In_reads_(Num32BitValuesToSet * sizeof(UINT)) const void* pSrcData,
                                                   _In_ UINT                                                  DestOffsetIn32BitValues);

    extern void hook_SetComputeRootConstantBufferView(_Inout_ ID3D12GraphicsCommandList1* pCommandList, UINT RootParameterIndex,
                                                      _In_ D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

    extern void hook_SetGraphicsRootConstantBufferView(_Inout_ ID3D12GraphicsCommandList1* pCommandList, UINT RootParameterIndex,
                                                       _In_ D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

    extern void hook_OMSetRenderTargets(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ UINT NumRenderTargetDescriptors,
                                        _In_ const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors,
                                        _In_ BOOL                               RTsSingleHandleToDescriptorRange,
                                        _In_opt_ const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor);

    extern void hook_ClearDepthStencilView(_Inout_ ID3D12GraphicsCommandList1* pCommandList,
                                           _In_ D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, _In_ D3D12_CLEAR_FLAGS ClearFlags,
                                           _In_ FLOAT Depth, _In_ UINT8 Stencil, _In_ UINT NumRects,
                                           _In_reads_(NumRects) const D3D12_RECT* pRect);

    extern void hook_ClearRenderTargetView(_Inout_ ID3D12GraphicsCommandList1* pCommandList,
                                           _In_ D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, _In_ const FLOAT ColorRGBA[4],
                                           _In_ UINT NumRects, _In_reads_(NumRects) const D3D12_RECT* pRects);

    extern void hook_IASetIndexBuffer(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_opt_ const D3D12_INDEX_BUFFER_VIEW* pView);

    extern void hook_IASetVertexBuffers(_Inout_ ID3D12GraphicsCommandList1*                               pCommandList,
                                        _In_range_(0, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1) UINT StartSlot,
                                        _In_range_(1, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT) UINT     NumViews,
                                        _In_reads_opt_(NumViews) const D3D12_VERTEX_BUFFER_VIEW*          pViews);

    extern void hook_IASetPrimitiveTopology(_Inout_ ID3D12GraphicsCommandList1* pCommandList,
                                            _In_ D3D12_PRIMITIVE_TOPOLOGY       PrimitiveTopology);

    extern void hook_RSSetViewports(_Inout_ ID3D12GraphicsCommandList1*                                          pCommandList,
                                    _In_range_(0, D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE) UINT Count,
                                    _In_reads_(Count) const D3D12_VIEWPORT*                                      pViewports);

    extern void hook_RSSetScissorRects(_Inout_ ID3D12GraphicsCommandList1*                                          pCommandList,
                                       _In_range_(0, D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE) UINT Count,
                                       _In_reads_(Count) const D3D12_RECT*                                          pRects);

    extern void hook_OMSetStencilRef(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ UINT StencilRef);

    extern void hook_SetPipelineState(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ ID3D12PipelineState* pPipelineState);

    extern void hook_ExecuteIndirect(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ ID3D12CommandSignature* pCommandSignature,
                                     _In_ UINT MaxCommandCount, _In_ ID3D12Resource* pArgumentBuffer, _In_ UINT64 ArgumentBufferOffset,
                                     _In_opt_ ID3D12Resource* pCountBuffer, _In_ UINT64 CountBufferOffset);

    extern void hook_SetSamplePositions(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ UINT NumSamplesPerPixel, _In_ UINT NumPixels,
                                        _In_reads_(NumSamplesPerPixel* NumPixels) D3D12_SAMPLE_POSITION* pSamplePositions);

    extern void hook_ResourceBarrier(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ UINT NumBarriers,
                                     _In_reads_(NumBarriers) const D3D12_RESOURCE_BARRIER* pBarriers);

    extern void hook_BeginQuery(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ ID3D12QueryHeap* pQueryHeap,
                                _In_ D3D12_QUERY_TYPE Type, _In_ UINT Index);

    extern void hook_EndQuery(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ ID3D12QueryHeap* pQueryHeap,
                              _In_ D3D12_QUERY_TYPE Type, _In_ UINT Index);

    extern void hook_ResolveQueryData(_Inout_ ID3D12GraphicsCommandList1* pCommandList, _In_ ID3D12QueryHeap* pQueryHeap,
                                      _In_ D3D12_QUERY_TYPE Type, _In_ UINT StartElement, _In_ UINT ElementCount,
                                      _In_ ID3D12Resource* pDestinationBuffer, _In_ UINT64 AlignedDestinationBufferOffset);

#if defined(D3D12_WORKGRAPH_AVAILABLE)
    extern HRESULT hook_CreateStateObject(ID3D12Device5* This, const D3D12_STATE_OBJECT_DESC* pDesc, REFIID riid,
                                          _Out_ void** ppStateObject);
#endif

#if defined(D3D12_RAYTRACING_AVAILABLE)
    extern void hook_GetRaytracingAccelerationStructurePrebuildInfo(ID3D12Device5* This,
                                                                    _In_ const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS* pDesc,
                                                                    _Out_ D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO* pInfo);

    extern void hook_BuildRaytracingAccelerationStructure(
        ID3D12GraphicsCommandList4* This, _In_ const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC* pDesc,
        _In_ UINT                                                                                                NumPostbuildInfoDescs,
        _In_reads_opt_(NumPostbuildInfoDescs) const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* pPostbuildInfoDescs);
#endif

#if defined(DRED)
    extern HRESULT hook_GetAutoBreadcrumbsOutput(ID3D12DeviceRemovedExtendedData* This, _Out_ D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT* pOutput);
    extern HRESULT hook_GetPageFaultAllocationOutput(ID3D12DeviceRemovedExtendedData* This, _Out_ D3D12_DRED_PAGE_FAULT_OUTPUT* pOutput);
#endif

#ifdef __cplusplus
}
#endif

#endif

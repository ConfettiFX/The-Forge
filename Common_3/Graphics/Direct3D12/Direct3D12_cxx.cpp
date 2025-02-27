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
#include "Direct3D12_cxx.h"

#define AMD_AGS_HELPER_IMPL
#include "../../../Common_3/Graphics/ThirdParty/OpenSource/ags/AgsHelper.h"

#include "../../../Common_3/Graphics/ThirdParty/OpenSource/DirectXShaderCompiler/inc/dxcapi.h"

#define D3D12MA_IMPLEMENTATION
#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"
#include "../ThirdParty/OpenSource/D3D12MemoryAllocator/Direct3D12MemoryAllocator.h"

#if defined(FORGE_PROFILE)
#define PROFILE_BUILD // This turns on USE_PIX so that we can set markers even in release build
#endif
#if defined(XBOX)
#include <pix3.h>
#else
#include "../../../Common_3/Graphics/ThirdParty/OpenSource/winpixeventruntime/Include/WinPixEventRuntime/pix3.h"
#endif
#if defined(FORGE_PROFILE)
#undef PROFILE_BUILD
#endif

extern "C" void PIX_BeginEvent(ID3D12GraphicsCommandList1* context, float r, float g, float b, const char* pName)
{
    // note: USE_PIX isn't the ideal test because we might be doing a debug build where pix
    // is not installed, or a variety of other reasons. It should be a separate #ifdef flag?
#ifdef USE_PIX
    // color is in B8G8R8X8 format where X is padding
    PIXBeginEvent(context, PIX_COLOR((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255)), pName);
#else
    UNREF_PARAM(context);
    UNREF_PARAM(r);
    UNREF_PARAM(g);
    UNREF_PARAM(b);
    UNREF_PARAM(pName);
#endif
}

extern "C" void PIX_EndEvent(ID3D12GraphicsCommandList1* context)
{
#ifdef USE_PIX
    PIXEndEvent(context);
#else
    UNREF_PARAM(context);
#endif
}

extern "C" void PIX_SetMarker(ID3D12GraphicsCommandList1* context, float r, float g, float b, const char* pName)
{
#ifdef USE_PIX
    // color is in B8G8R8X8 format where X is padding
    PIXSetMarker(context, PIX_COLOR((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255)), pName);
#else
    UNREF_PARAM(context);
    UNREF_PARAM(r);
    UNREF_PARAM(g);
    UNREF_PARAM(b);
    UNREF_PARAM(pName);
#endif
}

extern "C" HRESULT IDxcBlobEncoding_QueryInterface(struct IDxcBlobEncoding* pEncoding, struct IDxcBlobUtf8** pOut)
{
    return pEncoding->QueryInterface(pOut);
}

extern "C" void IDxcBlobEncoding_Release(struct IDxcBlobEncoding* pEncoding) { pEncoding->Release(); }

extern "C" LPVOID IDxcBlobEncoding_GetBufferPointer(struct IDxcBlobEncoding* pEncoding) { return pEncoding->GetBufferPointer(); }

extern "C" SIZE_T IDxcBlobEncoding_GetBufferSize(struct IDxcBlobEncoding* pEncoding) { return pEncoding->GetBufferSize(); }

extern "C" LPCSTR IDxcBlobUtf8_GetStringPointer(struct IDxcBlobUtf8* pBlob) { return pBlob->GetStringPointer(); }

extern "C" SIZE_T IDxcBlobUtf8_GetStringLength(struct IDxcBlobUtf8* pBlob) { return pBlob->GetStringLength(); }

extern "C" HRESULT IDxcUtils_CreateBlob(void* pByteCode, uint32_t byteCodeSize, struct IDxcBlobEncoding** ppEncoding)
{
    IDxcUtils* pUtils;
    HRESULT    res = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
    if (!SUCCEEDED(res))
    {
        return res;
    }
    pUtils->CreateBlob(pByteCode, byteCodeSize, DXC_CP_ACP, ppEncoding); //-V522
    pUtils->Release();
    return 0;
}

extern "C" HRESULT D3D12MA_CreateAllocator(ID3D12Device* pDevice, D3D12MA_IDXGIAdapter* pGpu, struct D3D12MAAllocator_** ppOut)
{
    D3D12MA::ALLOCATOR_DESC desc = {};
    desc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
    desc.pDevice = pDevice;
    desc.pAdapter = pGpu;

    D3D12MA::ALLOCATION_CALLBACKS allocationCallbacks = {};
    allocationCallbacks.pAllocate = [](size_t size, size_t alignment, void*) { return tf_memalign(alignment, size); };
    allocationCallbacks.pFree = [](void* ptr, void*) { tf_free(ptr); };
    desc.pAllocationCallbacks = &allocationCallbacks;
    return D3D12MA::CreateAllocator(&desc, (D3D12MAAllocator**)ppOut);
}

extern "C" void D3D12MA_ReleaseAllocator(struct D3D12MAAllocator_* pAllocator)
{
    if (pAllocator)
    {
        ((D3D12MAAllocator*)pAllocator)->Release();
    }
}

extern "C" HRESULT D3D12MA_CreateResource(struct D3D12MAAllocator_* pAllocator, const D3D12MA_ALLOCATION_DESC* pDesc,
                                          const D3D12_RESOURCE_DESC* pResDesc, struct D3D12MAAllocation_** ppAlloc,
                                          ID3D12Resource** ppResource)
{
    D3D12MA::ALLOCATION_DESC alloc_desc = {};
    alloc_desc.HeapType = pDesc->HeapType;
    alloc_desc.ExtraHeapFlags = pDesc->ExtraHeapFlags;
    alloc_desc.CreationNodeMask = pDesc->CreationNodeMask;
    alloc_desc.VisibleNodeMask = pDesc->VisibleNodeMask;
    if (pDesc->mUseDedicatedAllocation)
    {
        alloc_desc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;
    }
    return ((D3D12MAAllocator*)pAllocator)
        ->CreateResource(&alloc_desc, pResDesc, pDesc->ResourceStates, pDesc->pOptimizedClearValue, (D3D12MAAllocation**)ppAlloc,
                         IID_ID3D12Resource, (void**)ppResource);
}

extern "C" void D3D12MA_ReleaseAllocation(struct D3D12MAAllocation_* pAlloc)
{
    if (pAlloc)
    {
        ((D3D12MAAllocation*)pAlloc)->Release();
    }
}

extern "C" void D3D12MA_CalculateMemoryUse(struct D3D12MAAllocator_* pAllocator, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
{
    D3D12MA::TotalStatistics stats;
    ((D3D12MAAllocator*)pAllocator)->CalculateStatistics(&stats);
    *usedBytes = stats.Total.Stats.BlockBytes;
    *totalAllocatedBytes = stats.Total.Stats.AllocationBytes;
}

extern "C" void D3D12MA_BuildStatsString(struct D3D12MAAllocator_* pAllocator, BOOL detailedMap)
{
    ((D3D12MAAllocator*)pAllocator)->BuildStatsString(detailedMap);
}
#endif

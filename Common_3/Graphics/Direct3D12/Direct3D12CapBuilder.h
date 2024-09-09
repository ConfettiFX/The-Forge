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

#pragma once

#include "../GraphicsConfig.h"

#ifdef DIRECT3D12

#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"

inline void d3d12CapsBuilder(ID3D12Device* pDevice, GpuDesc* pGpuDesc)
{
    // While DirectX-Specs state that any format that reports load support must also report store support
    // I think it's just better to double check for both.
    D3D12_FORMAT_SUPPORT2 loadStore = D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD | D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE;
    for (uint32_t i = 0; i < TinyImageFormat_Count; ++i)
    {
        DXGI_FORMAT fmt = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)i);
        if (fmt == DXGI_FORMAT_UNKNOWN)
            continue;

        D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport = { fmt };

        pDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));
        if (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE)
        {
            pGpuDesc->mFormatCaps[i] |= FORMAT_CAP_LINEAR_FILTER | FORMAT_CAP_READ;
        }
        if (formatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE)
        {
            pGpuDesc->mFormatCaps[i] |= FORMAT_CAP_WRITE;
        }
        if (formatSupport.Support2 & loadStore)
        {
            pGpuDesc->mFormatCaps[i] |= FORMAT_CAP_READ_WRITE;
        }
        if (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET)
        {
            pGpuDesc->mFormatCaps[i] |= FORMAT_CAP_RENDER_TARGET;
        }
    }
}
#endif

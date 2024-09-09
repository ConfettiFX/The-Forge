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

#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

#include "../GraphicsConfig.h"

inline void vkCapsBuilder(GpuDesc* pGpuDesc)
{
    for (uint32_t i = 0; i < TinyImageFormat_Count; ++i)
    {
        VkFormatProperties formatSupport;
        VkFormat           fmt = (VkFormat)TinyImageFormat_ToVkFormat((TinyImageFormat)i);
        if (VK_FORMAT_UNDEFINED == fmt
            // these values were deprecated and will cause validation errors
            || VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG == fmt || VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG == fmt ||
            VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG == fmt || VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG == fmt ||
            VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG == fmt || VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG == fmt ||
            VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG == fmt || VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG == fmt)
        {
            continue;
        }

        vkGetPhysicalDeviceFormatProperties(pGpuDesc->mVk.pGpu, fmt, &formatSupport);

        if (formatSupport.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
        {
            pGpuDesc->mFormatCaps[i] |= FORMAT_CAP_LINEAR_FILTER;
        }
        if (formatSupport.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
        {
            pGpuDesc->mFormatCaps[i] |= FORMAT_CAP_READ;
        }
        if (formatSupport.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
        {
            pGpuDesc->mFormatCaps[i] |= FORMAT_CAP_WRITE | FORMAT_CAP_READ_WRITE;
        }
        if (formatSupport.optimalTilingFeatures & (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
        {
            pGpuDesc->mFormatCaps[i] |= FORMAT_CAP_RENDER_TARGET;
        }
    }
}

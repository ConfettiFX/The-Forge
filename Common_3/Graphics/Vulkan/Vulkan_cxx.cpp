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

#ifdef VULKAN

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wswitch"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wswitch"
#endif

#define VMA_IMPLEMENTATION
#include "../ThirdParty/OpenSource/VulkanMemoryAllocator/VulkanMemoryAllocator.h"

#define AMD_AGS_HELPER_IMPL
#include "../../../Common_3/Graphics/ThirdParty/OpenSource/ags/AgsHelper.h"

#if VMA_STATS_STRING_ENABLED
extern "C" void vmaInitBufferUsage(VmaAllocation alloc, const VkBufferCreateInfo* pCreateInfo, bool useKhrMaintenance5)
{
    alloc->InitBufferUsage(*pCreateInfo, useKhrMaintenance5);
}
extern "C" void vmaInitImageUsage(VmaAllocation alloc, const VkImageCreateInfo* pCreateInfo) { alloc->InitImageUsage(*pCreateInfo); }
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif

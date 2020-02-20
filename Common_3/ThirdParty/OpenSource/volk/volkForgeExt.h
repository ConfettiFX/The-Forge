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

#ifndef VOLK_FORGE_EXT_H_
#define VOLK_FORGE_EXT_H_

#include "../../../Renderer/IRenderer.h"
#if defined(VK_USE_PLATFORM_GGP)
#include <vulkan/vk_layer_dispatch_table.h>
#else
#include <LayerFactory/Project/vk_layer_dispatch_table.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize library by using provided dispatch tables and VK instance/device.
 * It'll init all VK func pointers from the dispatch tables that are passed in.
 * This function also fills out the following members of Forge's Renderer struct:
 * - pVkInstance
 * - pVkDevice
 * - pVkActiveGPU
 * Returns VK_SUCCESS on success and VK_ERROR_INITIALIZATION_FAILED otherwise.
 */
VkResult volkInitializeWithDispatchTables(Renderer* pRenderer);

#ifdef __cplusplus
}
#endif

#endif
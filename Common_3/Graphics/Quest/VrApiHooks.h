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

#if defined(QUEST_VR)

#include "../Interfaces/IGraphics.h"

bool hook_add_vk_instance_extensions(const char** instanceExtensionCache, uint* extensionCount, uint maxExtensionCount, char* pBuffer,
                                     uint bufferSize);
bool hook_add_vk_device_extensions(const char** deviceExtensionCache, uint* extensionCount, uint maxExtensionCount, char* pBuffer,
                                   uint bufferSize);

bool hook_post_init_renderer(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
void hook_pre_remove_renderer();

void hook_add_swap_chain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain);
void hook_remove_swap_chain(Renderer* pRenderer, SwapChain* pSwapChain);

void hook_acquire_next_image(SwapChain* pSwapChain, uint32_t* pImageIndex);
void hook_queue_present(const QueuePresentDesc* pQueuePresentDesc);

#endif

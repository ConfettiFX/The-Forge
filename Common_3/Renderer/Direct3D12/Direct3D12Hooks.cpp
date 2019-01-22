/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#if defined(DIRECT3D12)
#include "../IRenderer.h"
#include "Direct3D12Hooks.h"
#include "../../../Common_3\OS\Interfaces\IMemoryManager.h"

static void enable_debug_layer_hook(Renderer* pRenderer)
{
#if defined(_DEBUG) || defined(PROFILE)
	pRenderer->pDXDebug->EnableDebugLayer();
#endif
}

static ImageFormat::Enum get_recommended_swapchain_format(bool hintHDR) { return ImageFormat::BGRA8; }

static uint32_t get_swap_chain_image_index(SwapChain* pSwapChain) { return pSwapChain->pDxSwapChain->GetCurrentBackBufferIndex(); }

void initHooks()
{
	fnHookEnableDebugLayer = enable_debug_layer_hook;
	fnHookGetRecommendedSwapChainFormat = get_recommended_swapchain_format;
	fnHookGetSwapChainImageIndex = get_swap_chain_image_index;
}
#endif

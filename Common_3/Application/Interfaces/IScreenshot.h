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

#include "../../Graphics/Interfaces/IGraphics.h"

FORGE_API void initScreenshotInterface(Renderer* pRenderer, Queue* pGraphicsQueue);

// Use one renderpass prior to calling captureScreenshot() to prepare pSwapChain for copy.
FORGE_API bool prepareScreenshot(SwapChain* pSwapChain);

// Attempts to capture a screenshot if setCaptureScreenshot was called for this frame
FORGE_API void captureScreenshot(SwapChain* pSwapChain, uint32_t swapChainRtIndex, bool noAlpha, bool forceFlipRedBlue);

FORGE_API void exitScreenshotInterface();

// Schedules the next drawn frame to be captured
FORGE_API void setCaptureScreenshot(const char* name);

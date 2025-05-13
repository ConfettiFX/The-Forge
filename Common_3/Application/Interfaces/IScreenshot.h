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

#include "../../Graphics/Interfaces/IGraphics.h"

typedef struct ScreenshotDesc
{
    RenderTarget* pRenderTarget;
    Semaphore**   ppWaitSemaphores;
    uint32_t      mWaitSemaphoresCount;
    ColorSpace    mColorSpace;
    bool          discardAlpha;
    bool          flipRedBlue;
} ScreenshotDesc;

#ifdef __cplusplus
extern "C"
{
#endif

    // Schedules the next drawn frame to  captured
    FORGE_API void requestScreenshotCapture(const char* name);

    FORGE_API bool isScreenshotCaptureRequested();

    FORGE_API void initScreenshotCapturer(Renderer* pRenderer, Queue* pGraphcisQueue, const char* appName);

    FORGE_API void setScreenshotName(char* pName);

    FORGE_API char* getScreenshotName();

    FORGE_API void exitScreenshotCapturer();

    FORGE_API void captureScreenshot(ScreenshotDesc* pDesc);

#ifdef __cplusplus
}
#endif

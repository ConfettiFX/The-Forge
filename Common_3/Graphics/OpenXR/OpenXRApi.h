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

#if defined(QUEST_VR)
#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../Interfaces/IGraphics.h"

#include <openxr/openxr_platform.h>
#include <meta_openxr_preview/openxr_oculus_helpers.h>
#include <android_native_app_glue.h>

#define MAX_OPENXR_LAYERS 16
#define MAX_OPENXR_VIEWS  2
#define LEFT_EYE_VIEW     0
#define RIGHT_EYE_VIEW    1

#define LOG_OXR_ERRORS

#define ENGINE_NAME "The-Forge"

typedef struct OpenXRApp
{
    XrInstance     mInstance;
    XrSession      mSession;
    XrSystemId     mSystemId;
    XrSessionState mSessionState;
    bool           mSessionStarted;

#if defined(XR_USE_GRAPHICS_API_VULKAN)
    XrGraphicsBindingVulkanKHR mGraphicsBinding;
#endif // XR_USE_GRAPHICS_API_VULKAN

    XrViewConfigurationProperties mViewConfig;
    XrViewConfigurationView       mViewConfigViews[MAX_OPENXR_VIEWS];
    XrView                        mViews[MAX_OPENXR_VIEWS];
    uint32_t                      mViewCount;

    XrSpace mViewSpace;
    XrSpace mLocalSpace;

    XrFrameState mCurrentFrameState;
#if defined(XR_USE_PLATFORM_ANDROID)
    ANativeActivity* mAndroidActivity;
#endif // XR_USE_PLATFORM_ANDROID

    XrFoveationLevelFB mFoveationLevel;
} OpenXRApp;

#ifdef __cplusplus
extern "C"
{
#endif

    extern OpenXRApp* pOXR;

    bool InitOpenXRLoader(struct android_app* pAndroidApp);
    void ExitOpenXRLoader();

    // Initialize OpenXR
    bool CreateOpenXRInstance(const char* appName);
    void InitOpenXRSystem();
    void InitOpenXRSession();

    // Functions to handle device/application settings
    void QueryOpenXRRefreshRates(float* outSupportedRefreshRates, uint32_t* outRefreshRatesCount, uint32_t* currentRefreshRateIndex);
    bool RequestOpenXRRefreshRate(float refreshRate);
    void GetOpenXRRecommendedResolution(RectDesc* rect);

    // Functions to manage XRSwapchains
    bool VerifySwapchainFormat(int64_t format);
    void CreateOpenXRSwapchain(uint32_t width, uint32_t height, int64_t format, uint32_t sampleCount, bool enableFoveation,
                               XrSwapchain* outSwapchain, uint32_t* outSwapchainImagesCount);
    void DestroyOpenXRSwapchain(const XrSwapchain swapchainHandle);
    void GetOpenXRSwapchainImages(const XrSwapchain swapchainHandle, uint32_t swapchainImagesCount,
                                  XrSwapchainImageBaseHeader* outSwapchainImages);
    void SetOpenXRFoveation(XrFoveationLevelFB level, float verticalOffset, XrFoveationDynamicFB dynamic,
                            const XrSwapchain swapchainHandle);

    void PollOpenXREvent(bool* exitRequired);

    void BeginOpenXRDraw();
    void EndOpenXRDraw(const XrSwapchain swapchainHandle, int32_t swapchainWidth, int32_t swapchainHeight);

    void AcquireOpenXRSwapchainImage(const XrSwapchain swapchainHandle, uint32_t* outSwapchainImageIndex);
    void ReleaseOpenXRSwapchainImage(const XrSwapchain swapchainHandle);

#ifdef __cplusplus
}
#endif

#endif // QUEST_VR

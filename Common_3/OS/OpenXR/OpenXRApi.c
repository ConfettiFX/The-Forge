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

#include "../../Utilities/Interfaces/ILog.h"

#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Graphics/Interfaces/IGraphics.h"

#include <openxr/openxr_platform.h>
#include <meta_openxr_preview/openxr_oculus_helpers.h>
#include <android_native_app_glue.h>

#define MAX_OPENXR_VIEWS 2
#define LEFT_EYE_VIEW    0
#define RIGHT_EYE_VIEW   1

#define LOG_OXR_ERRORS

#define ENGINE_NAME "The-Forge"

typedef struct OpenXRApp
{
    XrInstance     mInstance;
    XrSystemId     mSystemId;
    XrSessionState mSessionState;
    bool           mSessionStarted;

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

    XrSession mSession;

    // Move to global in OpenXRInput.c
    struct OpenXRInputBindings* pInputBindings;
} OpenXRApp;

OpenXRApp* pOXR = NULL;

// Getters to be used by shared code (input, rendering, platform)
XrInstance                    GetCurrentXRInstance() { return pOXR->mInstance; }
XrSession                     GetCurrentXRSession() { return pOXR->mSession; }
XrSystemId                    GetXRSystemId() { return pOXR->mSystemId; }
XrFrameState*                 GetCurrentXRFrameState() { return &pOXR->mCurrentFrameState; }
XrViewConfigurationProperties GetXRViewConfig() { return pOXR->mViewConfig; }
uint32_t                      GetXRViewCount() { return pOXR->mViewCount; }
XrView*                       GetXRViews() { return &pOXR->mViews[0]; }
XrView                        GetXRView(uint32_t viewIndex) { return pOXR->mViews[viewIndex]; }
XrSpace                       GetXRLocalSpace() { return pOXR->mLocalSpace; }
XrSpace                       GetXRViewSpace() { return pOXR->mViewSpace; }

XrResult VerifyOXRResult(XrResult res, const char* func, const char* cmd, bool assertOnErr)
{
    if (XR_FAILED(res))
    {
#ifdef LOG_OXR_ERRORS
        char errStr[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(GetCurrentXRInstance(), res, errStr);
        LOGF(eERROR, "[%s] | OpenXR Error while running cmd %s : %s", func, cmd, errStr);
#endif // LOG_OXR_ERRORS
        ASSERTMSG(!assertOnErr, "Fatal OpenXR Error: %s", errStr);
    }

    return res;
}

// OpenXR specific debugging macros to check function results (similar to CHECK_VKRESULT)
#define CHECK_OXRCMD(func)           VerifyOXRResult(func, __FUNCTION__, #func, true)
#define CHECK_OXRRESULT(res, cmdStr) VerifyOXRResult(res, __FUNCTION__, cmdStr, false) == XR_SUCCESS

const char* const REQUIRED_EXTENSIONS[] = {
    XR_FB_COLOR_SPACE_EXTENSION_NAME,
    XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME,
    XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME,
    XR_FB_FOVEATION_EXTENSION_NAME,
    XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME,

#if defined(XR_USE_GRAPHICS_API_VULKAN)
    XR_KHR_VULKAN_ENABLE_EXTENSION_NAME,
    XR_FB_FOVEATION_VULKAN_EXTENSION_NAME,
#endif // XR_USE_GRAPHICS_API_VULKAN

#if defined(XR_USE_PLATFORM_ANDROID)
    XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
#endif // XR_USE_GRAPHICS_API_VULKAN
};
const uint32_t REQUIRED_EXTENSION_COUNT = TF_ARRAY_COUNT(REQUIRED_EXTENSIONS);

const XrViewConfigurationType DEFAULT_VIEW_CONFIG = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

bool InitOpenXRLoader(struct android_app* pAndroidApp)
{
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
    XrResult result = xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&xrInitializeLoaderKHR);
    if (xrInitializeLoaderKHR != NULL)
    {
        pOXR = (OpenXRApp*)tf_malloc(sizeof(OpenXRApp));
        memset(pOXR, 0, sizeof(OpenXRApp));
        pOXR->mAndroidActivity = pAndroidApp->activity;

        XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid = { XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR };
        loaderInitializeInfoAndroid.applicationVM = pOXR->mAndroidActivity->vm;
        loaderInitializeInfoAndroid.applicationContext = pOXR->mAndroidActivity->clazz;
        result = xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loaderInitializeInfoAndroid);
    }

    return CHECK_OXRRESULT(result, "xrGetInstanceProcAddr-xrIntializeLoaderKHR");
}

void LogAvailableLayers()
{
    uint32_t layerCount = 0;
    CHECK_OXRCMD(xrEnumerateApiLayerProperties(0, &layerCount, NULL));

    LOGF(eINFO, "xrEnumerateApiLayerProperties Found %d layers", layerCount);
    XrApiLayerProperties* layerProperties = (XrApiLayerProperties*)tf_malloc(layerCount * sizeof(XrApiLayerProperties));

    for (uint32_t layerIndex = 0; layerIndex < layerCount; layerIndex++)
    {
        layerProperties[layerIndex].type = XR_TYPE_API_LAYER_PROPERTIES;
        layerProperties[layerIndex].next = NULL;
    }

    CHECK_OXRCMD(xrEnumerateApiLayerProperties(layerCount, &layerCount, layerProperties));

    for (uint32_t layerIndex = 0; layerIndex < layerCount; layerIndex++)
    {
        LOGF(eINFO, "Found layer %s", layerProperties[layerIndex].layerName);
    }

    tf_free(layerProperties);
}

void GetAvailableViewConfigurations()
{
    // Enumerate the viewport configurations.
    uint32_t viewportConfigTypeCount = 0;
    CHECK_OXRCMD(xrEnumerateViewConfigurations(pOXR->mInstance, pOXR->mSystemId, 0, &viewportConfigTypeCount, NULL));

    XrViewConfigurationType* viewportConfigurationTypes =
        (XrViewConfigurationType*)tf_malloc(viewportConfigTypeCount * sizeof(XrViewConfigurationType));

    CHECK_OXRCMD(xrEnumerateViewConfigurations(pOXR->mInstance, pOXR->mSystemId, viewportConfigTypeCount, &viewportConfigTypeCount,
                                               viewportConfigurationTypes));

    LOGF(eINFO, "Available Viewport Configuration Types: %d", viewportConfigTypeCount);

    for (uint32_t i = 0; i < viewportConfigTypeCount; i++)
    {
        const XrViewConfigurationType viewportConfigType = viewportConfigurationTypes[i];

        LOGF(eINFO, "Viewport configuration type %d : %s", viewportConfigType, viewportConfigType == DEFAULT_VIEW_CONFIG ? "Selected" : "");

        XrViewConfigurationProperties viewportConfig = { .type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES };
        CHECK_OXRCMD(xrGetViewConfigurationProperties(pOXR->mInstance, pOXR->mSystemId, viewportConfigType, &viewportConfig));
        LOGF(eINFO, "FovMutable=%s ConfigurationType %d", viewportConfig.fovMutable ? "true" : "false",
             viewportConfig.viewConfigurationType);

        CHECK_OXRCMD(xrEnumerateViewConfigurationViews(pOXR->mInstance, pOXR->mSystemId, viewportConfigType, 0, &pOXR->mViewCount, NULL));

        if (pOXR->mViewCount > 0)
        {
            XrViewConfigurationView* elements = (XrViewConfigurationView*)tf_malloc(pOXR->mViewCount * sizeof(XrViewConfigurationView));

            for (uint32_t e = 0; e < pOXR->mViewCount; e++)
            {
                elements[e].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
                elements[e].next = NULL;
            }

            CHECK_OXRCMD(xrEnumerateViewConfigurationViews(pOXR->mInstance, pOXR->mSystemId, viewportConfigType, pOXR->mViewCount,
                                                           &pOXR->mViewCount, elements));

            // Log the view config info for each view type for debugging purposes.
            for (uint32_t e = 0; e < pOXR->mViewCount; e++)
            {
                const XrViewConfigurationView* element = &elements[e];

                LOGF(eINFO, "Viewport [%d]: Recommended Width=%d Height=%d SampleCount=%d", e, element->recommendedImageRectWidth,
                     element->recommendedImageRectHeight, element->recommendedSwapchainSampleCount);

                LOGF(eINFO, "Viewport [%d]: Max Width=%d Height=%d SampleCount=%d", e, element->maxImageRectWidth,
                     element->maxImageRectHeight, element->maxSwapchainSampleCount);
            }

            if (viewportConfigType == DEFAULT_VIEW_CONFIG)
            {
                ASSERT(pOXR->mViewCount == MAX_OPENXR_VIEWS);
                for (uint32_t e = 0; e < pOXR->mViewCount; e++)
                {
                    pOXR->mViewConfigViews[e] = elements[e];
                    pOXR->mViews[e].type = XR_TYPE_VIEW;
                }
            }

            tf_free(elements);
        }
        else
        {
            LOGF(eERROR, "Empty viewport configuration type: %d", pOXR->mViewCount);
        }
    }

    tf_free(viewportConfigurationTypes);

    pOXR->mViewConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;

    CHECK_OXRCMD(xrGetViewConfigurationProperties(pOXR->mInstance, pOXR->mSystemId, DEFAULT_VIEW_CONFIG, &pOXR->mViewConfig));
}

bool CheckAvailableExtensions()
{
    uint32_t extensionCount = 0;
    CHECK_OXRCMD(xrEnumerateInstanceExtensionProperties(NULL, 0, &extensionCount, NULL));

    LOGF(eINFO, "xrEnumerateInstanceExtensionProperties found %u extension(s).", extensionCount);

    XrExtensionProperties* extensionProperties = (XrExtensionProperties*)tf_malloc(extensionCount * sizeof(XrExtensionProperties));

    for (uint32_t i = 0; i < extensionCount; i++)
    {
        extensionProperties[i].type = XR_TYPE_EXTENSION_PROPERTIES;
        extensionProperties[i].next = NULL;
    }

    CHECK_OXRCMD(xrEnumerateInstanceExtensionProperties(NULL, extensionCount, &extensionCount, extensionProperties));
    for (uint32_t extensionIndex = 0; extensionIndex < extensionCount; extensionIndex++)
    {
        LOGF(eINFO, "Extension #%d = '%s'.", extensionIndex, extensionProperties[extensionIndex].extensionName);
    }

    for (uint32_t requiredExtension = 0; requiredExtension < REQUIRED_EXTENSION_COUNT; requiredExtension++)
    {
        bool extensionFound = false;
        for (uint32_t extensionIndex = 0; extensionIndex < extensionCount; extensionIndex++)
        {
            if (!strcmp(REQUIRED_EXTENSIONS[requiredExtension], extensionProperties[extensionIndex].extensionName))
            {
                LOGF(eINFO, "Found required extension %s", REQUIRED_EXTENSIONS[requiredExtension]);
                extensionFound = true;
                break;
            }
        }
        if (!extensionFound)
        {
            LOGF(eINFO, "Failed to find required extension %s", REQUIRED_EXTENSIONS[requiredExtension]);
            return false;
        }
    }

    tf_free(extensionProperties);
    return true;
}

void InitOpenXRSystem()
{
    ASSERT(pOXR->mInstance != XR_NULL_HANDLE);
    ASSERT(pOXR->mSystemId == XR_NULL_SYSTEM_ID);

    XrSystemGetInfo systemInfo = { .type = XR_TYPE_SYSTEM_GET_INFO };
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY; // Quest only supports this form factor
    CHECK_OXRCMD(xrGetSystem(pOXR->mInstance, &systemInfo, &pOXR->mSystemId));

    ASSERT(pOXR->mInstance != XR_NULL_HANDLE);
    ASSERT(pOXR->mSystemId != XR_NULL_SYSTEM_ID);

    XrSystemProperties systemProperties = { XR_TYPE_SYSTEM_PROPERTIES };
    CHECK_OXRCMD(xrGetSystemProperties(pOXR->mInstance, pOXR->mSystemId, &systemProperties));

    LOGF(eINFO, "System Properties: Name=%s VendorId=%x", systemProperties.systemName, systemProperties.vendorId);
    LOGF(eINFO, "System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
         systemProperties.graphicsProperties.maxSwapchainImageWidth, systemProperties.graphicsProperties.maxSwapchainImageHeight,
         systemProperties.graphicsProperties.maxLayerCount);
    LOGF(eINFO, "System Tracking Properties: OrientationTracking=%s PositionTracking=%s",
         systemProperties.trackingProperties.orientationTracking ? "True" : "False",
         systemProperties.trackingProperties.positionTracking ? "True" : "False");

    GetAvailableViewConfigurations();
}

void ExitOpenXRLoader()
{
    if (pOXR->mLocalSpace != XR_NULL_HANDLE)
    {
        xrDestroySpace(pOXR->mLocalSpace);
    }

    if (pOXR->mViewSpace != XR_NULL_HANDLE)
    {
        xrDestroySpace(pOXR->mViewSpace);
    }

    if (pOXR->mSession != XR_NULL_HANDLE)
    {
        xrDestroySession(pOXR->mSession);
    }

    if (pOXR->mInstance != XR_NULL_HANDLE)
    {
        xrDestroyInstance(pOXR->mInstance);
    }

    tf_free(pOXR);
}

bool CreateOpenXRInstance(const char* appName)
{
    if (!VERIFYMSG(CheckAvailableExtensions(), "Missing required extensions to run OpenXR"))
    {
        ExitOpenXRLoader();
        return false;
    }

    LogAvailableLayers();

    ASSERT(pOXR->mInstance == XR_NULL_HANDLE);
    // Create the OpenXR instance.
    XrApplicationInfo appInfo;
    memset(&appInfo, 0, sizeof(appInfo));
    strcpy(appInfo.applicationName, appName);
    appInfo.applicationVersion = 0;
    strcpy(appInfo.engineName, ENGINE_NAME);
    appInfo.engineVersion = 0; // TODO: Perhaps specify the release version here?
    appInfo.apiVersion = XR_API_VERSION_1_0;

    XrInstanceCreateInfoAndroidKHR instanceCreateInfoAndroid = { XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR };
    instanceCreateInfoAndroid.applicationVM = pOXR->mAndroidActivity->vm;
    instanceCreateInfoAndroid.applicationActivity = pOXR->mAndroidActivity->clazz;

    XrInstanceCreateInfo instanceCreateInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
    instanceCreateInfo.applicationInfo = appInfo;
    instanceCreateInfo.enabledExtensionCount = REQUIRED_EXTENSION_COUNT;
    instanceCreateInfo.enabledExtensionNames = REQUIRED_EXTENSIONS;
    instanceCreateInfo.next = (XrBaseInStructure*)&instanceCreateInfoAndroid;

    XrResult initResult = xrCreateInstance(&instanceCreateInfo, &pOXR->mInstance);
    if (CHECK_OXRRESULT(initResult, "xrCreateInstance"))
    {
        XrInstanceProperties instanceProps = { .type = XR_TYPE_INSTANCE_PROPERTIES };
        CHECK_OXRCMD(xrGetInstanceProperties(pOXR->mInstance, &instanceProps));
        LOGF(eINFO, "OpenXR: Created Instance | Runtime %s: Version info: %u.%u.%u", instanceProps.runtimeName,
             XR_VERSION_MAJOR(instanceProps.runtimeVersion), XR_VERSION_MINOR(instanceProps.runtimeVersion),
             XR_VERSION_PATCH(instanceProps.runtimeVersion));
        return true;
    }
    else
    {
        ExitOpenXRLoader();
        return false;
    }
}

void InitOpenXR(struct android_app* pAndroidApp, const char* appName)
{
    InitOpenXRLoader(pAndroidApp);
    CreateOpenXRInstance(appName);
    InitOpenXRSystem();
}

void SetColorSpace()
{
    PFN_xrEnumerateColorSpacesFB pfnxrEnumerateColorSpacesFB = NULL;
    CHECK_OXRCMD(xrGetInstanceProcAddr(pOXR->mInstance, "xrEnumerateColorSpacesFB", (PFN_xrVoidFunction*)(&pfnxrEnumerateColorSpacesFB)));

    uint32_t colorSpaceCountOutput = 0;
    CHECK_OXRCMD(pfnxrEnumerateColorSpacesFB(pOXR->mSession, 0, &colorSpaceCountOutput, NULL));

    XrColorSpaceFB* colorSpaces = (XrColorSpaceFB*)malloc(colorSpaceCountOutput * sizeof(XrColorSpaceFB));

    CHECK_OXRCMD(pfnxrEnumerateColorSpacesFB(pOXR->mSession, colorSpaceCountOutput, &colorSpaceCountOutput, colorSpaces));
    LOGF(eINFO, "Supported ColorSpaces:");

    for (uint32_t i = 0; i < colorSpaceCountOutput; i++)
    {
        LOGF(eINFO, "%d:%d", i, colorSpaces[i]);
    }

    const XrColorSpaceFB requestColorSpace = XR_COLOR_SPACE_REC2020_FB;

    PFN_xrSetColorSpaceFB pfnxrSetColorSpaceFB = NULL;
    CHECK_OXRCMD(xrGetInstanceProcAddr(pOXR->mInstance, "xrSetColorSpaceFB", (PFN_xrVoidFunction*)(&pfnxrSetColorSpaceFB)));

    CHECK_OXRCMD(pfnxrSetColorSpaceFB(pOXR->mSession, requestColorSpace));

    free(colorSpaces);
}

void QueryOpenXRRefreshRates(float* outSupportedRefreshRates, uint32_t* outRefreshRatesCount, uint32_t* currentRefreshRateIndex)
{
    PFN_xrEnumerateDisplayRefreshRatesFB pfnxrEnumerateDisplayRefreshRatesFB = NULL;
    CHECK_OXRCMD(xrGetInstanceProcAddr(pOXR->mInstance, "xrEnumerateDisplayRefreshRatesFB",
                                       (PFN_xrVoidFunction*)(&pfnxrEnumerateDisplayRefreshRatesFB)));

    PFN_xrGetDisplayRefreshRateFB pfnGetDisplayRefreshRate;
    CHECK_OXRCMD(xrGetInstanceProcAddr(pOXR->mInstance, "xrGetDisplayRefreshRateFB", (PFN_xrVoidFunction*)(&pfnGetDisplayRefreshRate)));

    float currentDisplayRefreshRate = 0.0f;
    CHECK_OXRCMD(pfnGetDisplayRefreshRate(pOXR->mSession, &currentDisplayRefreshRate));
    LOGF(eINFO, "Current System Display Refresh Rate: %f", currentDisplayRefreshRate);

    CHECK_OXRCMD(pfnxrEnumerateDisplayRefreshRatesFB(pOXR->mSession, 0, outRefreshRatesCount, NULL));

    CHECK_OXRCMD(
        pfnxrEnumerateDisplayRefreshRatesFB(pOXR->mSession, *outRefreshRatesCount, outRefreshRatesCount, outSupportedRefreshRates));
    LOGF(eINFO, "Supported Refresh Rates:");
    for (uint32_t refreshRateIndex = 0; refreshRateIndex < *outRefreshRatesCount; refreshRateIndex++)
    {
        LOGF(eINFO, "%d:%f", refreshRateIndex, outSupportedRefreshRates[refreshRateIndex]);

        if (currentDisplayRefreshRate == outSupportedRefreshRates[refreshRateIndex])
        {
            *currentRefreshRateIndex = refreshRateIndex;
        }
    }
}

bool RequestOpenXRRefreshRate(float refreshRate)
{
    PFN_xrRequestDisplayRefreshRateFB pfnRequestDisplayRefreshRate;
    xrGetInstanceProcAddr(pOXR->mInstance, "xrRequestDisplayRefreshRateFB", (PFN_xrVoidFunction*)(&pfnRequestDisplayRefreshRate));

    XrResult result = pfnRequestDisplayRefreshRate(pOXR->mSession, refreshRate);
    LOGF(eINFO, "Requesting display refresh rate: %f", refreshRate);

    return CHECK_OXRRESULT(result, "xrRequestDisplayRefreshRateFB");
}

void GetOpenXRRecommendedResolution(RectDesc* rect)
{
    rect->left = 0;
    rect->top = 0;
    rect->bottom = pOXR->mViewConfigViews[LEFT_EYE_VIEW].recommendedImageRectHeight;
    rect->right = pOXR->mViewConfigViews[LEFT_EYE_VIEW].recommendedImageRectWidth;
}

void GetReferenceSpaces()
{
    ASSERT(pOXR->mSession != XR_NULL_HANDLE);

    uint32_t spaceCount;
    CHECK_OXRCMD(xrEnumerateReferenceSpaces(pOXR->mSession, 0, &spaceCount, NULL));
    const uint32_t       MAX_SPACES = 32;
    XrReferenceSpaceType spaces[MAX_SPACES];
    ASSERT(spaceCount < MAX_SPACES);
    CHECK_OXRCMD(xrEnumerateReferenceSpaces(pOXR->mSession, spaceCount, &spaceCount, spaces));

    LOGF(eINFO, "Available reference spaces: %d", spaceCount);
    for (uint32_t spaceIndex = 0; spaceIndex < spaceCount; spaceIndex++)
    {
        LOGF(eINFO, "  Name: %d", spaces[spaceIndex]);
    }
}

void InitXRSession(Renderer* pRenderer)
{
    ASSERT(pOXR->mInstance != XR_NULL_HANDLE);
    ASSERT(pOXR->mSession == XR_NULL_HANDLE);

    XrSessionCreateInfo createInfo = { .type = XR_TYPE_SESSION_CREATE_INFO };
    createInfo.next = (const XrBaseInStructure*)&pRenderer->pContext->mVR.mXRGraphicsBinding;
    createInfo.systemId = pOXR->mSystemId;
    CHECK_OXRCMD(xrCreateSession(pOXR->mInstance, &createInfo, &pOXR->mSession));

    SetColorSpace();
    GetReferenceSpaces();

    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo = { .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    XrPosef_CreateIdentity(&referenceSpaceCreateInfo.poseInReferenceSpace);
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    CHECK_OXRCMD(xrCreateReferenceSpace(pOXR->mSession, &referenceSpaceCreateInfo, &pOXR->mLocalSpace));

    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    CHECK_OXRCMD(xrCreateReferenceSpace(pOXR->mSession, &referenceSpaceCreateInfo, &pOXR->mViewSpace));
}

bool PollOpenXREvent(bool* exitRequired)
{
    XrEventDataBuffer eventDataBuffer = { .type = XR_TYPE_EVENT_DATA_BUFFER };

    for (;;)
    {
        XrEventDataBaseHeader* baseEventHeader = (XrEventDataBaseHeader*)(&eventDataBuffer);
        baseEventHeader->type = XR_TYPE_EVENT_DATA_BUFFER;
        baseEventHeader->next = NULL;
        XrResult res = xrPollEvent(pOXR->mInstance, &eventDataBuffer);
        if (CHECK_OXRRESULT(res, "xrPollEvent") == false)
        {
            break;
        }

        switch (baseEventHeader->type)
        {
        case XR_TYPE_EVENT_DATA_EVENTS_LOST:
        {
            LOGF(eWARNING, "xrPollEvent: returned XR_TYPE_EVENT_DATA_EVENTS_LOST");
            break;
        }
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
        {
            const XrEventDataInstanceLossPending* lossPendingEvent = (XrEventDataInstanceLossPending*)(baseEventHeader);
            LOGF(eWARNING, "xrPollEvent: returned XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING with time %lld", lossPendingEvent->lossTime);
            break;
        }
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
        {
            const XrEventDataSessionStateChanged* sessionStateChangedEvent = (XrEventDataSessionStateChanged*)(baseEventHeader);
            LOGF(eINFO, "XrEventDataSessionStateChanged: Session %lld | Changed from %p to %p | Time %lld",
                 sessionStateChangedEvent->session, (void*)pOXR->mSessionState, (void*)sessionStateChangedEvent->state,
                 sessionStateChangedEvent->time);
            pOXR->mSessionState = sessionStateChangedEvent->state;
            switch (pOXR->mSessionState)
            {
            case XR_SESSION_STATE_READY:
            {
                ASSERT(pOXR->mSession != XR_NULL_HANDLE);
                XrSessionBeginInfo sessionBeginInfo = { .type = XR_TYPE_SESSION_BEGIN_INFO };
                sessionBeginInfo.primaryViewConfigurationType = pOXR->mViewConfig.viewConfigurationType;
                CHECK_OXRCMD(xrBeginSession(pOXR->mSession, &sessionBeginInfo));
                pOXR->mSessionStarted = true;
                break;
            }
            case XR_SESSION_STATE_STOPPING:
            {
                ASSERT(pOXR->mSession != XR_NULL_HANDLE);
                CHECK_OXRCMD(xrEndSession(pOXR->mSession));
                pOXR->mSessionStarted = false;
                break;
            }
            case XR_SESSION_STATE_EXITING:
            {
                *exitRequired = true;
                break;
            }
            case XR_SESSION_STATE_LOSS_PENDING:
            {
                *exitRequired = true;
            }
            default:
                break;
            }
            break;
        }
        default:
        {
            LOGF(eINFO, "Received event type %d.", baseEventHeader->type);
            break;
        }
        }
    }

    return pOXR->mSessionStarted;
}

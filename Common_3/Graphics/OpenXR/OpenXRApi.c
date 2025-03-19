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

#include "OpenXRApi.h"

OpenXRApp* pOXR = NULL;
XrInstance GetCurrentXRInstance() { return pOXR->mInstance; }

static inline XrResult VerifyOXRResult(XrResult res, const char* func, const char* cmd, bool assertOnErr)
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

#define VERIFY_OXR_CMD(func)          VerifyOXRResult(func, __FUNCTION__, #func, true)

#define CHECK_OXR_RESULT(res, cmdStr) VerifyOXRResult(res, __FUNCTION__, cmdStr, false) == XR_SUCCESS

const char* const REQUIRED_EXTENSIONS[] = {
    XR_FB_COLOR_SPACE_EXTENSION_NAME,
    XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME,
    XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME,
    XR_FB_FOVEATION_EXTENSION_NAME,

#if defined(XR_USE_GRAPHICS_API_VULKAN)
    XR_KHR_VULKAN_ENABLE_EXTENSION_NAME,
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

    return CHECK_OXR_RESULT(result, "xrGetInstanceProcAddr-xrIntializeLoaderKHR");
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

void LogAvailableLayers()
{
    uint32_t layerCount = 0;
    VERIFY_OXR_CMD(xrEnumerateApiLayerProperties(0, &layerCount, NULL));

    LOGF(eINFO, "xrEnumerateApiLayerProperties Found %d layers", layerCount);
    XrApiLayerProperties* layerProperties = (XrApiLayerProperties*)tf_malloc(layerCount * sizeof(XrApiLayerProperties));

    for (uint32_t layerIndex = 0; layerIndex < layerCount; layerIndex++)
    {
        layerProperties[layerIndex].type = XR_TYPE_API_LAYER_PROPERTIES;
        layerProperties[layerIndex].next = NULL;
    }

    VERIFY_OXR_CMD(xrEnumerateApiLayerProperties(layerCount, &layerCount, layerProperties));

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
    VERIFY_OXR_CMD(xrEnumerateViewConfigurations(pOXR->mInstance, pOXR->mSystemId, 0, &viewportConfigTypeCount, NULL));

    XrViewConfigurationType* viewportConfigurationTypes =
        (XrViewConfigurationType*)tf_malloc(viewportConfigTypeCount * sizeof(XrViewConfigurationType));

    VERIFY_OXR_CMD(xrEnumerateViewConfigurations(pOXR->mInstance, pOXR->mSystemId, viewportConfigTypeCount, &viewportConfigTypeCount,
                                                 viewportConfigurationTypes));

    LOGF(eINFO, "Available Viewport Configuration Types: %d", viewportConfigTypeCount);

    for (uint32_t i = 0; i < viewportConfigTypeCount; i++)
    {
        const XrViewConfigurationType viewportConfigType = viewportConfigurationTypes[i];

        LOGF(eINFO, "Viewport configuration type %d : %s", viewportConfigType, viewportConfigType == DEFAULT_VIEW_CONFIG ? "Selected" : "");

        XrViewConfigurationProperties viewportConfig = { .type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES };
        VERIFY_OXR_CMD(xrGetViewConfigurationProperties(pOXR->mInstance, pOXR->mSystemId, viewportConfigType, &viewportConfig));
        LOGF(eINFO, "FovMutable=%s ConfigurationType %d", viewportConfig.fovMutable ? "true" : "false",
             viewportConfig.viewConfigurationType);

        VERIFY_OXR_CMD(xrEnumerateViewConfigurationViews(pOXR->mInstance, pOXR->mSystemId, viewportConfigType, 0, &pOXR->mViewCount, NULL));

        if (pOXR->mViewCount > 0)
        {
            XrViewConfigurationView* elements = (XrViewConfigurationView*)tf_malloc(pOXR->mViewCount * sizeof(XrViewConfigurationView));

            for (uint32_t e = 0; e < pOXR->mViewCount; e++)
            {
                elements[e].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
                elements[e].next = NULL;
            }

            VERIFY_OXR_CMD(xrEnumerateViewConfigurationViews(pOXR->mInstance, pOXR->mSystemId, viewportConfigType, pOXR->mViewCount,
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

    VERIFY_OXR_CMD(xrGetViewConfigurationProperties(pOXR->mInstance, pOXR->mSystemId, DEFAULT_VIEW_CONFIG, &pOXR->mViewConfig));
}

bool CheckAvailableExtensions()
{
    uint32_t extensionCount = 0;
    VERIFY_OXR_CMD(xrEnumerateInstanceExtensionProperties(NULL, 0, &extensionCount, NULL));

    LOGF(eINFO, "xrEnumerateInstanceExtensionProperties found %u extension(s).", extensionCount);

    XrExtensionProperties* extensionProperties = (XrExtensionProperties*)tf_malloc(extensionCount * sizeof(XrExtensionProperties));

    for (uint32_t i = 0; i < extensionCount; i++)
    {
        extensionProperties[i].type = XR_TYPE_EXTENSION_PROPERTIES;
        extensionProperties[i].next = NULL;
    }

    VERIFY_OXR_CMD(xrEnumerateInstanceExtensionProperties(NULL, extensionCount, &extensionCount, extensionProperties));
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

void SetColorSpace()
{
    PFN_xrEnumerateColorSpacesFB pfnxrEnumerateColorSpacesFB = NULL;
    VERIFY_OXR_CMD(xrGetInstanceProcAddr(pOXR->mInstance, "xrEnumerateColorSpacesFB", (PFN_xrVoidFunction*)(&pfnxrEnumerateColorSpacesFB)));

    uint32_t colorSpaceCountOutput = 0;
    VERIFY_OXR_CMD(pfnxrEnumerateColorSpacesFB(pOXR->mSession, 0, &colorSpaceCountOutput, NULL));

    XrColorSpaceFB* colorSpaces = (XrColorSpaceFB*)malloc(colorSpaceCountOutput * sizeof(XrColorSpaceFB));

    VERIFY_OXR_CMD(pfnxrEnumerateColorSpacesFB(pOXR->mSession, colorSpaceCountOutput, &colorSpaceCountOutput, colorSpaces));
    LOGF(eINFO, "Supported ColorSpaces:");

    for (uint32_t i = 0; i < colorSpaceCountOutput; i++)
    {
        LOGF(eINFO, "%d:%d", i, colorSpaces[i]);
    }

    const XrColorSpaceFB requestColorSpace = XR_COLOR_SPACE_REC2020_FB;

    PFN_xrSetColorSpaceFB pfnxrSetColorSpaceFB = NULL;
    VERIFY_OXR_CMD(xrGetInstanceProcAddr(pOXR->mInstance, "xrSetColorSpaceFB", (PFN_xrVoidFunction*)(&pfnxrSetColorSpaceFB)));

    VERIFY_OXR_CMD(pfnxrSetColorSpaceFB(pOXR->mSession, requestColorSpace));

    free(colorSpaces);
}

void QueryOpenXRRefreshRates(float* outSupportedRefreshRates, uint32_t* outRefreshRatesCount, uint32_t* currentRefreshRateIndex)
{
    PFN_xrEnumerateDisplayRefreshRatesFB pfnxrEnumerateDisplayRefreshRatesFB = NULL;
    VERIFY_OXR_CMD(xrGetInstanceProcAddr(pOXR->mInstance, "xrEnumerateDisplayRefreshRatesFB",
                                         (PFN_xrVoidFunction*)(&pfnxrEnumerateDisplayRefreshRatesFB)));

    PFN_xrGetDisplayRefreshRateFB pfnGetDisplayRefreshRate;
    VERIFY_OXR_CMD(xrGetInstanceProcAddr(pOXR->mInstance, "xrGetDisplayRefreshRateFB", (PFN_xrVoidFunction*)(&pfnGetDisplayRefreshRate)));

    float currentDisplayRefreshRate = 0.0f;
    VERIFY_OXR_CMD(pfnGetDisplayRefreshRate(pOXR->mSession, &currentDisplayRefreshRate));
    LOGF(eINFO, "Current System Display Refresh Rate: %f", currentDisplayRefreshRate);

    VERIFY_OXR_CMD(pfnxrEnumerateDisplayRefreshRatesFB(pOXR->mSession, 0, outRefreshRatesCount, NULL));

    VERIFY_OXR_CMD(
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

    return CHECK_OXR_RESULT(result, "xrRequestDisplayRefreshRateFB");
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
    VERIFY_OXR_CMD(xrEnumerateReferenceSpaces(pOXR->mSession, 0, &spaceCount, NULL));
    const uint32_t       MAX_SPACES = 32;
    XrReferenceSpaceType spaces[MAX_SPACES];
    ASSERT(spaceCount < MAX_SPACES);
    VERIFY_OXR_CMD(xrEnumerateReferenceSpaces(pOXR->mSession, spaceCount, &spaceCount, spaces));

    LOGF(eINFO, "Available reference spaces: %d", spaceCount);
    for (uint32_t spaceIndex = 0; spaceIndex < spaceCount; spaceIndex++)
    {
        LOGF(eINFO, "  Name: %d", spaces[spaceIndex]);
    }
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
    if (CHECK_OXR_RESULT(initResult, "xrCreateInstance"))
    {
        XrInstanceProperties instanceProps = { .type = XR_TYPE_INSTANCE_PROPERTIES };
        VERIFY_OXR_CMD(xrGetInstanceProperties(pOXR->mInstance, &instanceProps));
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

void InitOpenXRSystem()
{
    ASSERT(pOXR->mInstance != XR_NULL_HANDLE);
    ASSERT(pOXR->mSystemId == XR_NULL_SYSTEM_ID);

    XrSystemGetInfo systemInfo = { .type = XR_TYPE_SYSTEM_GET_INFO };
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY; // Quest only supports this form factor
    VERIFY_OXR_CMD(xrGetSystem(pOXR->mInstance, &systemInfo, &pOXR->mSystemId));

    ASSERT(pOXR->mInstance != XR_NULL_HANDLE);
    ASSERT(pOXR->mSystemId != XR_NULL_SYSTEM_ID);

    XrSystemProperties systemProperties = { XR_TYPE_SYSTEM_PROPERTIES };
    VERIFY_OXR_CMD(xrGetSystemProperties(pOXR->mInstance, pOXR->mSystemId, &systemProperties));

    LOGF(eINFO, "System Properties: Name=%s VendorId=%x", systemProperties.systemName, systemProperties.vendorId);
    LOGF(eINFO, "System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
         systemProperties.graphicsProperties.maxSwapchainImageWidth, systemProperties.graphicsProperties.maxSwapchainImageHeight,
         systemProperties.graphicsProperties.maxLayerCount);
    LOGF(eINFO, "System Tracking Properties: OrientationTracking=%s PositionTracking=%s",
         systemProperties.trackingProperties.orientationTracking ? "True" : "False",
         systemProperties.trackingProperties.positionTracking ? "True" : "False");

    GetAvailableViewConfigurations();
}

void InitOpenXRSession()
{
    ASSERT(pOXR->mInstance != XR_NULL_HANDLE);
    ASSERT(pOXR->mSession == XR_NULL_HANDLE);

    XrSessionCreateInfo createInfo = { .type = XR_TYPE_SESSION_CREATE_INFO };
    createInfo.next = (const XrBaseInStructure*)&pOXR->mGraphicsBinding;
    createInfo.systemId = pOXR->mSystemId;
    VERIFY_OXR_CMD(xrCreateSession(pOXR->mInstance, &createInfo, &pOXR->mSession));

    SetColorSpace();
    GetReferenceSpaces();

    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo = { .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    XrPosef_CreateIdentity(&referenceSpaceCreateInfo.poseInReferenceSpace);
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    VERIFY_OXR_CMD(xrCreateReferenceSpace(pOXR->mSession, &referenceSpaceCreateInfo, &pOXR->mLocalSpace));

    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    VERIFY_OXR_CMD(xrCreateReferenceSpace(pOXR->mSession, &referenceSpaceCreateInfo, &pOXR->mViewSpace));
}

bool VerifySwapchainFormat(int64_t format)
{
    uint32_t swapchainFormatCount;
    VERIFY_OXR_CMD(xrEnumerateSwapchainFormats(pOXR->mSession, 0, &swapchainFormatCount, NULL));
    int64_t* swapchainFormats = (int64_t*)tf_malloc(swapchainFormatCount * sizeof(int64_t));
    VERIFY_OXR_CMD(xrEnumerateSwapchainFormats(pOXR->mSession, swapchainFormatCount, &swapchainFormatCount, swapchainFormats));
    bool verified = false;
    for (uint32_t formatIndex = 0; formatIndex < swapchainFormatCount; formatIndex++)
    {
        if (format == swapchainFormats[formatIndex])
        {
            verified = true;
            break;
        }
    }
    tf_free(swapchainFormats);

    return verified;
}

void CreateOpenXRSwapchain(uint32_t width, uint32_t height, int64_t format, uint32_t sampleCount, bool enableFoveation,
                           XrSwapchain* outSwapchain, uint32_t* outSwapchainImagesCount)
{
    ASSERT(pOXR->mViewCount == MAX_OPENXR_VIEWS);

    LOGF(eINFO, "Creating OpenXR Swapchains with dimensions %d X %d and SampleCount %d", width, height, sampleCount);

    XrSwapchainCreateInfo swapchainCreateInfo = { .type = XR_TYPE_SWAPCHAIN_CREATE_INFO };
    swapchainCreateInfo.arraySize = pOXR->mViewCount;
    swapchainCreateInfo.format = format;
    swapchainCreateInfo.width = width;
    swapchainCreateInfo.height = height;
    swapchainCreateInfo.mipCount = 1;
    swapchainCreateInfo.faceCount = 1;
    swapchainCreateInfo.sampleCount =
        sampleCount == 0 ? pOXR->mViewConfigViews[LEFT_EYE_VIEW].recommendedSwapchainSampleCount : sampleCount;
    swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

    if (enableFoveation)
    {
        XrSwapchainCreateInfoFoveationFB swapchainFoveationCreateInfo = { XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB };
        swapchainCreateInfo.next = &swapchainFoveationCreateInfo;
    }

    VERIFY_OXR_CMD(xrCreateSwapchain(pOXR->mSession, &swapchainCreateInfo, outSwapchain));

    VERIFY_OXR_CMD(xrEnumerateSwapchainImages(*outSwapchain, 0, outSwapchainImagesCount, NULL));
}

void DestroyOpenXRSwapchain(const XrSwapchain swapchainHandle)
{
    ASSERT(swapchainHandle != XR_NULL_HANDLE);
    VERIFY_OXR_CMD(xrDestroySwapchain(swapchainHandle));
}

void GetOpenXRSwapchainImages(const XrSwapchain swapchainHandle, uint32_t swapchainImagesCount,
                              XrSwapchainImageBaseHeader* outSwapchainImages)
{
    ASSERT(swapchainHandle != XR_NULL_HANDLE);

    VERIFY_OXR_CMD(xrEnumerateSwapchainImages(swapchainHandle, swapchainImagesCount, &swapchainImagesCount, outSwapchainImages));
}

void SetOpenXRFoveation(XrFoveationLevelFB level, float verticalOffset, XrFoveationDynamicFB dynamic, const XrSwapchain swapchainHandle)
{
    PFN_xrCreateFoveationProfileFB pfnCreateFoveationProfileFB;
    VERIFY_OXR_CMD(
        xrGetInstanceProcAddr(pOXR->mInstance, "xrCreateFoveationProfileFB", (PFN_xrVoidFunction*)(&pfnCreateFoveationProfileFB)));

    PFN_xrDestroyFoveationProfileFB pfnDestroyFoveationProfileFB;
    VERIFY_OXR_CMD(
        xrGetInstanceProcAddr(pOXR->mInstance, "xrDestroyFoveationProfileFB", (PFN_xrVoidFunction*)(&pfnDestroyFoveationProfileFB)));

    PFN_xrUpdateSwapchainFB pfnUpdateSwapchainFB;
    VERIFY_OXR_CMD(xrGetInstanceProcAddr(pOXR->mInstance, "xrUpdateSwapchainFB", (PFN_xrVoidFunction*)(&pfnUpdateSwapchainFB)));

    XrFoveationLevelProfileCreateInfoFB levelProfileCreateInfo = { .type = XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB };
    levelProfileCreateInfo.level = level;
    levelProfileCreateInfo.verticalOffset = verticalOffset;
    levelProfileCreateInfo.dynamic = dynamic;

    XrFoveationProfileCreateInfoFB profileCreateInfo = { .type = XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB };
    profileCreateInfo.next = &levelProfileCreateInfo;

    XrFoveationProfileFB foveationProfile;

    pfnCreateFoveationProfileFB(pOXR->mSession, &profileCreateInfo, &foveationProfile);

    XrSwapchainStateFoveationFB foveationUpdateState = { .type = XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB };
    foveationUpdateState.profile = foveationProfile;

    pfnUpdateSwapchainFB(swapchainHandle, (XrSwapchainStateBaseHeaderFB*)(&foveationUpdateState));

    pfnDestroyFoveationProfileFB(foveationProfile);
}

void PollOpenXREvent(bool* exitRequired)
{
    XrEventDataBuffer eventDataBuffer = { .type = XR_TYPE_EVENT_DATA_BUFFER };

    for (;;)
    {
        XrEventDataBaseHeader* baseEventHeader = (XrEventDataBaseHeader*)(&eventDataBuffer);
        baseEventHeader->type = XR_TYPE_EVENT_DATA_BUFFER;
        baseEventHeader->next = NULL;
        XrResult res = xrPollEvent(pOXR->mInstance, &eventDataBuffer);
        if (CHECK_OXR_RESULT(res, "xrPollEvent") == false)
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
                VERIFY_OXR_CMD(xrBeginSession(pOXR->mSession, &sessionBeginInfo));
                pOXR->mSessionStarted = true;
                break;
            }
            case XR_SESSION_STATE_STOPPING:
            {
                ASSERT(pOXR->mSession != XR_NULL_HANDLE);
                VERIFY_OXR_CMD(xrEndSession(pOXR->mSession));
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
}

void BeginOpenXRDraw()
{
    ASSERT(pOXR->mSession != XR_NULL_HANDLE);

    XrFrameWaitInfo frameWaitInfo = { .type = XR_TYPE_FRAME_WAIT_INFO };
    memset(&pOXR->mCurrentFrameState, 0, sizeof(XrFrameState));
    pOXR->mCurrentFrameState.type = XR_TYPE_FRAME_STATE;
    VERIFY_OXR_CMD(xrWaitFrame(pOXR->mSession, &frameWaitInfo, &pOXR->mCurrentFrameState));

    XrFrameBeginInfo frameBeginInfo = { .type = XR_TYPE_FRAME_BEGIN_INFO };
    VERIFY_OXR_CMD(xrBeginFrame(pOXR->mSession, &frameBeginInfo));

    XrViewState viewState;
    viewState.type = XR_TYPE_VIEW_STATE;
    uint32_t viewCapacityInput = (uint32_t)TF_ARRAY_COUNT(pOXR->mViews);
    uint32_t viewCountOutput;

    XrViewLocateInfo viewLocateInfo = { .type = XR_TYPE_VIEW_LOCATE_INFO };
    viewLocateInfo.viewConfigurationType = pOXR->mViewConfig.viewConfigurationType;
    viewLocateInfo.displayTime = pOXR->mCurrentFrameState.predictedDisplayTime;
    viewLocateInfo.space = pOXR->mLocalSpace;

    VERIFY_OXR_CMD(xrLocateViews(pOXR->mSession, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, pOXR->mViews));
    ASSERTMSG(viewCountOutput == pOXR->mViewCount, "Mismatch in Viewcount from xrLocateViews");

    XrView mViewSpaceViews[MAX_OPENXR_VIEWS];
    mViewSpaceViews[0].type = XR_TYPE_VIEW;
    mViewSpaceViews[1].type = XR_TYPE_VIEW;

    viewLocateInfo.space = pOXR->mViewSpace;
    VERIFY_OXR_CMD(xrLocateViews(pOXR->mSession, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, mViewSpaceViews));
}

void EndOpenXRDraw(const XrSwapchain swapchainHandle, int32_t swapchainWidth, int32_t swapchainHeight)
{
    ASSERT(pOXR->mSession != XR_NULL_HANDLE);

    XrCompositionLayerProjectionView    projectionLayerViews[MAX_OPENXR_VIEWS] = { [LEFT_EYE_VIEW] = { 0 }, [RIGHT_EYE_VIEW] = { 0 } };
    const XrCompositionLayerBaseHeader* layerPtrs[MAX_OPENXR_LAYERS] = { 0 };

    for (uint32_t layerViewIndex = 0; layerViewIndex < pOXR->mViewCount; layerViewIndex++)
    {
        projectionLayerViews[layerViewIndex].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        projectionLayerViews[layerViewIndex].pose = pOXR->mViews[layerViewIndex].pose;
        projectionLayerViews[layerViewIndex].fov = pOXR->mViews[layerViewIndex].fov;
        projectionLayerViews[layerViewIndex].subImage.swapchain = swapchainHandle;
        projectionLayerViews[layerViewIndex].subImage.imageRect.offset.x = 0;
        projectionLayerViews[layerViewIndex].subImage.imageRect.offset.y = 0;
        projectionLayerViews[layerViewIndex].subImage.imageRect.extent.width = swapchainWidth;
        projectionLayerViews[layerViewIndex].subImage.imageRect.extent.height = swapchainHeight;
        projectionLayerViews[layerViewIndex].subImage.imageArrayIndex = layerViewIndex;
    }

    // Only require 1 layer currently.
    XrCompositionLayerProjection layer = { .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    layer.space = pOXR->mLocalSpace;
    layer.layerFlags = 0;
    layer.viewCount = pOXR->mViewCount;
    layer.views = projectionLayerViews;
    layerPtrs[0] = (XrCompositionLayerBaseHeader*)&layer;

    ASSERT(pOXR->mCurrentFrameState.type == XR_TYPE_FRAME_STATE);
    XrFrameEndInfo frameEndInfo = { .type = XR_TYPE_FRAME_END_INFO };
    frameEndInfo.displayTime = pOXR->mCurrentFrameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE; // TODO: Add support for additional blend modes?
    frameEndInfo.layerCount = 1;
    frameEndInfo.layers = layerPtrs;
    VERIFY_OXR_CMD(xrEndFrame(pOXR->mSession, &frameEndInfo));
}

void AcquireOpenXRSwapchainImage(const XrSwapchain swapchainHandle, uint32_t* outSwapchainImageIndex)
{
    XrSwapchainImageAcquireInfo acquireInfo = { .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    VERIFY_OXR_CMD(xrAcquireSwapchainImage(swapchainHandle, &acquireInfo, outSwapchainImageIndex));

    XrSwapchainImageWaitInfo waitInfo = { .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = XR_INFINITE_DURATION;
    VERIFY_OXR_CMD(xrWaitSwapchainImage(swapchainHandle, &waitInfo));
}

void ReleaseOpenXRSwapchainImage(const XrSwapchain swapchainHandle)
{
    XrSwapchainImageReleaseInfo releaseInfo = { .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    VERIFY_OXR_CMD(xrReleaseSwapchainImage(swapchainHandle, &releaseInfo));
}

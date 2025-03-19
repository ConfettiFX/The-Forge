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

#if defined(QUEST_VR)
#include "OpenXRVulkan.h"

#include "../../Graphics/OpenXR/OpenXRApi.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

XrResult GetVulkanGraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsVulkanKHR* graphicsRequirements)
{
    PFN_xrGetVulkanGraphicsRequirementsKHR pfnGetVulkanGraphicsRequirementsKHR = NULL;
    xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&pfnGetVulkanGraphicsRequirementsKHR);

    pfnGetVulkanGraphicsRequirementsKHR(instance, systemId, graphicsRequirements);

    return XR_SUCCESS;
}

XrResult GetVulkanGraphicsDeviceKHR(XrInstance instance, const XrSystemId systemId, VkInstance vkInstance, VkPhysicalDevice* physicalDevice)
{
    PFN_xrGetVulkanGraphicsDeviceKHR pfnGetVulkanGraphicsDeviceKHR = NULL;
    xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction*)&pfnGetVulkanGraphicsDeviceKHR);

    return pfnGetVulkanGraphicsDeviceKHR(instance, systemId, vkInstance, physicalDevice);
}

bool OpenXRAddVKInstanceExt(const char** instanceExtensionCache, uint* extensionCount, uint maxExtensionCount, char* pExtensionsBuffer,
                            uint extensionsBufferSize)
{
    PFN_xrGetVulkanInstanceExtensionsKHR pfnGetVulkanInstanceExtensionsKHR = NULL;
    xrGetInstanceProcAddr(pOXR->mInstance, "xrGetVulkanInstanceExtensionsKHR", (PFN_xrVoidFunction*)&pfnGetVulkanInstanceExtensionsKHR);

    pfnGetVulkanInstanceExtensionsKHR(pOXR->mInstance, pOXR->mSystemId, 0, &extensionsBufferSize, NULL);
    pfnGetVulkanInstanceExtensionsKHR(pOXR->mInstance, pOXR->mSystemId, extensionsBufferSize, &extensionsBufferSize, pExtensionsBuffer);

    const char* vrExtensions[32];
    vrExtensions[0] = pExtensionsBuffer;
    int vrExtensionCount = 1;
    for (int i = 0; i < extensionsBufferSize; ++i)
    {
        if (pExtensionsBuffer[i] == ' ')
        {
            vrExtensions[vrExtensionCount] = &pExtensionsBuffer[i + 1];
            pExtensionsBuffer[i] = '\0';
            ++vrExtensionCount;
        }
        else if (pExtensionsBuffer[i] == '\0')
            break;
    }

    for (int i = 0; i < vrExtensionCount; ++i)
    {
        bool alreadyInCache = false;
        for (int j = 0; j < (*extensionCount); ++j)
        {
            if (strcmp(vrExtensions[i], instanceExtensionCache[j]) == 0)
            {
                alreadyInCache = true;
                break;
            }
        }

        if (!alreadyInCache)
        {
            if ((*extensionCount) == maxExtensionCount)
            {
                LOGF(eERROR, "Reached maximum instance extension count");
                ASSERT(false);
                return false;
            }
            instanceExtensionCache[*extensionCount] = vrExtensions[i];
            LOGF(eINFO, "Adding %s Vk Instance Extensions requested by the OpenXR runtime", vrExtensions[i]);
            ++(*extensionCount);
        }
    }

    return true;
}

bool OpenXRAddVKDeviceExt(const char** deviceExtensionCache, uint* extensionCount, uint maxExtensionCount, char* pExtensionsBuffer,
                          uint extensionsBufferSize)
{
    PFN_xrGetVulkanDeviceExtensionsKHR pfnGetVulkanDeviceExtensionsKHR = NULL;
    xrGetInstanceProcAddr(pOXR->mInstance, "xrGetVulkanDeviceExtensionsKHR", (PFN_xrVoidFunction*)(&pfnGetVulkanDeviceExtensionsKHR));

    extensionsBufferSize = 0;
    pfnGetVulkanDeviceExtensionsKHR(pOXR->mInstance, pOXR->mSystemId, 0, &extensionsBufferSize, NULL);
    if (extensionsBufferSize > 0)
    {
        pfnGetVulkanDeviceExtensionsKHR(pOXR->mInstance, pOXR->mSystemId, extensionsBufferSize, &extensionsBufferSize, pExtensionsBuffer);
    }

    const char* vrExtensions[32];
    vrExtensions[0] = pExtensionsBuffer;
    int vrExtensionCount = 1;
    for (int i = 0; i < extensionsBufferSize; ++i)
    {
        if (pExtensionsBuffer[i] == ' ')
        {
            vrExtensions[vrExtensionCount] = &pExtensionsBuffer[i + 1];
            pExtensionsBuffer[i] = '\0';
            ++vrExtensionCount;
        }
        else if (pExtensionsBuffer[i] == '\0')
            break;
    }

    for (int i = 0; i < vrExtensionCount; ++i)
    {
        bool alreadyInCache = false;
        for (int j = 0; j < (*extensionCount); ++j)
        {
            if (strcmp(vrExtensions[i], deviceExtensionCache[j]) == 0)
            {
                alreadyInCache = true;
                break;
            }
        }

        if (!alreadyInCache)
        {
            if ((*extensionCount) == maxExtensionCount)
            {
                LOGF(eERROR, "Reached maximum device extension count");
                ASSERT(false);
                return false;
            }
            deviceExtensionCache[*extensionCount] = vrExtensions[i];
            LOGF(eINFO, "Adding %s Vk Device Extensions requested by the OpenXR runtime", vrExtensions[i]);
            ++(*extensionCount);
        }
    }

    return true;
}

bool OpenXRPostInitVKRenderer(VkInstance instance, VkDevice device, uint8_t queueFamilyIndex)
{
    // Create the Vulkan device for the adapter associated with the system.
    // Extension function must be loaded by name
    XrGraphicsRequirementsVulkanKHR graphicsRequirements = { .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };
    GetVulkanGraphicsRequirementsKHR(pOXR->mInstance, pOXR->mSystemId, &graphicsRequirements);

    VkPhysicalDevice vkPhysicalDevice;
    GetVulkanGraphicsDeviceKHR(pOXR->mInstance, pOXR->mSystemId, instance, &vkPhysicalDevice);

    memset(&pOXR->mGraphicsBinding, 0, sizeof(XrGraphicsBindingVulkanKHR));
    pOXR->mGraphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
    pOXR->mGraphicsBinding.instance = instance;
    pOXR->mGraphicsBinding.physicalDevice = vkPhysicalDevice;
    pOXR->mGraphicsBinding.device = device;
    pOXR->mGraphicsBinding.queueFamilyIndex = queueFamilyIndex;
    pOXR->mGraphicsBinding.queueIndex = 0;
    return true;
}

void OpenXRAddVKSwapchain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
    VkFormat vkFormat = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mColorFormat);
    ASSERTMSG(VerifySwapchainFormat(vkFormat), "Requested swapchain format is not a supported OpenXR swapchain format");

    // TODO: Add support for multiple foveation levels.
    bool        foveationEnabled = pDesc->mFlags & SWAP_CHAIN_CREATION_FLAG_ENABLE_FOVEATED_RENDERING_VR;
    XrSwapchain swapchain;
    uint32_t    imageCount;
    CreateOpenXRSwapchain(pDesc->mWidth, pDesc->mHeight, vkFormat, VK_SAMPLE_COUNT_1_BIT, foveationEnabled, &swapchain, &imageCount);
    ASSERT(imageCount >= pDesc->mImageCount);

    SwapChain* pSwapChain = (SwapChain*)tf_calloc(1, sizeof(SwapChain) + imageCount * sizeof(RenderTarget*) +
                                                         imageCount * sizeof(XrSwapchainImageVulkanKHR) + sizeof(SwapChainDesc));
    ASSERT(pSwapChain);

    pSwapChain->ppRenderTargets = (RenderTarget**)(pSwapChain + 1);
    pSwapChain->mVR.pSwapchainImages = (XrSwapchainImageBaseHeader*)(pSwapChain->ppRenderTargets + imageCount);
    pSwapChain->mVk.pDesc = (SwapChainDesc*)((XrSwapchainImageVulkanKHR*)pSwapChain->mVR.pSwapchainImages + imageCount);

    pSwapChain->mVR.pSwapchain = swapchain;

    for (uint32_t imageIndex = 0; imageIndex < imageCount; imageIndex++)
    {
        XrSwapchainImageVulkanKHR* pSwapchainImage = ((XrSwapchainImageVulkanKHR*)pSwapChain->mVR.pSwapchainImages) + imageIndex;
        pSwapchainImage->type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
        pSwapchainImage->next = NULL;
        pSwapchainImage->image = VK_NULL_HANDLE;
    }
    GetOpenXRSwapchainImages(pSwapChain->mVR.pSwapchain, imageCount, pSwapChain->mVR.pSwapchainImages);

    RenderTargetDesc rtDesc = {};
    rtDesc.mWidth = pDesc->mWidth;
    rtDesc.mHeight = pDesc->mHeight;
    rtDesc.mDepth = 1;
    rtDesc.mArraySize = 1; // TODO: Support non multiview rendering
    rtDesc.mFormat = pDesc->mColorFormat;
    rtDesc.mClearValue = pDesc->mColorClearValue;
    rtDesc.mSampleCount = SAMPLE_COUNT_1;
    rtDesc.mSampleQuality = 0;
    rtDesc.mStartState = RESOURCE_STATE_PRESENT;
    rtDesc.mFlags |= TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
    if (foveationEnabled)
    {
        rtDesc.mFlags |= TEXTURE_CREATION_FLAG_VR_FOVEATED_RENDERING;
        SetOpenXRFoveation(XR_FOVEATION_LEVEL_HIGH_FB, 0, XR_FOVEATION_DYNAMIC_DISABLED_FB, pSwapChain->mVR.pSwapchain);
    }

    for (uint32_t rtIndex = 0; rtIndex < imageCount; rtIndex++)
    {
        XrSwapchainImageVulkanKHR* pSwapchainImage = &((XrSwapchainImageVulkanKHR*)pSwapChain->mVR.pSwapchainImages)[rtIndex];
        rtDesc.pNativeHandle = (void*)pSwapchainImage->image;
        addRenderTarget(pRenderer, &rtDesc, &pSwapChain->ppRenderTargets[rtIndex]);
    }

    *pSwapChain->mVk.pDesc = *pDesc;
    pSwapChain->mEnableVsync = pDesc->mEnableVsync;
    pSwapChain->mImageCount = imageCount;
    pSwapChain->mVk.pSurface = NULL;
    pSwapChain->mVk.mPresentQueueFamilyIndex = 0;
    pSwapChain->mVk.pPresentQueue = NULL;
    pSwapChain->mVk.pSwapChain = NULL;
    pSwapChain->mFormat = pDesc->mColorFormat;

    *ppSwapChain = pSwapChain;
}

void OpenXRRemoveVKSwapchain(Renderer* pRenderer, SwapChain* pSwapChain)
{
    for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
    {
        removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[i]);
    }

    DestroyOpenXRSwapchain(pSwapChain->mVR.pSwapchain);

    if (pSwapChain)
        tf_free(pSwapChain);
}

void OpenXRAcquireNextVKImage(SwapChain* pSwapChain, uint32_t* pImageIndex)
{
    BeginOpenXRDraw();
    AcquireOpenXRSwapchainImage(pSwapChain->mVR.pSwapchain, pImageIndex);
}

void OpenXRVKQueuePresent(const QueuePresentDesc* pQueuePresentDesc)
{
    SwapChain* pSwapchain = pQueuePresentDesc->pSwapChain;

    ReleaseOpenXRSwapchainImage(pSwapchain->mVR.pSwapchain);
    EndOpenXRDraw(pSwapchain->mVR.pSwapchain, pSwapchain->mVk.pDesc->mWidth, pSwapchain->mVk.pDesc->mHeight);
}

#endif // QUEST_VR
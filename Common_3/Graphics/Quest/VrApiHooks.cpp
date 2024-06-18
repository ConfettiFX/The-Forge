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

#if defined(QUEST_VR)
#include "VrApiHooks.h"

#include "../../OS/ThirdParty/PrivateOculus/ovr_sdk_mobile/VrApi/Include/VrApi.h"
#include "../../OS/ThirdParty/PrivateOculus/ovr_sdk_mobile/VrApi/Include/VrApi_Vulkan.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

#include "../../Utilities/Interfaces/ILog.h"

#include "../../OS/Quest/VrApi.h"

#include "../../Utilities/Interfaces/IMemory.h"

Queue*        pSynchronisationQueue = NULL;
RenderTarget* pFragmentDensityMask = NULL;

extern QuestVR* pQuest;

bool hook_add_vk_instance_extensions(const char** instanceExtensionCache, uint* extensionCount, uint maxExtensionCount, char* pBuffer,
                                     uint bufferSize)
{
    if (vrapi_GetInstanceExtensionsVulkan(pBuffer, &bufferSize))
    {
        LOGF(eERROR, "vrapi_GetInstanceExtensionsVulkan FAILED");
        ASSERT(false);
        return false;
    }

    const char* vrExtensions[32];
    vrExtensions[0] = pBuffer;
    int vrExtensionCount = 1;
    for (int i = 0; i < bufferSize; ++i)
    {
        if (pBuffer[i] == ' ')
        {
            vrExtensions[vrExtensionCount] = &pBuffer[i + 1];
            pBuffer[i] = '\0';
            ++vrExtensionCount;
        }
        else if (pBuffer[i] == '\0')
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
            ++(*extensionCount);
        }
    }

    return true;
}

bool hook_add_vk_device_extensions(const char** deviceExtensionCache, uint* extensionCount, uint maxExtensionCount, char* pBuffer,
                                   uint bufferSize)
{
    const char requiredExtensions[] = "VK_KHR_multiview VK_EXT_fragment_density_map VK_EXT_fragment_density_map";
    ASSERT(bufferSize > sizeof(requiredExtensions));
    memcpy(pBuffer, requiredExtensions, sizeof(requiredExtensions));

    pBuffer[sizeof(requiredExtensions) - 1] = ' ';
    uint remainingBufferSize = bufferSize - sizeof(requiredExtensions);

    if (vrapi_GetDeviceExtensionsVulkan(pBuffer + sizeof(requiredExtensions), &remainingBufferSize))
    {
        LOGF(eERROR, "vrapi_GetDeviceExtensionsVulkan FAILED");
        ASSERT(false);
        return false;
    }

    bufferSize = remainingBufferSize + sizeof(requiredExtensions);
    const char* vrExtensions[32];
    vrExtensions[0] = pBuffer;
    int vrExtensionCount = 1;
    for (int i = 0; i < bufferSize; ++i)
    {
        if (pBuffer[i] == ' ')
        {
            vrExtensions[vrExtensionCount] = &pBuffer[i + 1];
            pBuffer[i] = '\0';
            ++vrExtensionCount;
        }
        else if (pBuffer[i] == '\0')
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
            ++(*extensionCount);
        }
    }

    return true;
}

bool hook_post_init_renderer(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
{
    ovrSystemCreateInfoVulkan systemInfo;
    systemInfo.Instance = instance;
    systemInfo.Device = device;
    systemInfo.PhysicalDevice = physicalDevice;
    ovrResult initResult = vrapi_CreateSystemVulkan(&systemInfo);
    if (initResult != ovrSuccess)
    {
        LOGF(eERROR, "Failed to create VrApi Vulkan System");
        return false;
    }

    return true;
}

void hook_pre_remove_renderer() { vrapi_DestroySystemVulkan(); }

void hook_add_swap_chain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
    ovrSwapChainCreateInfo createInfo = {};
    createInfo.Format = (VkFormat)TinyImageFormat_ToVkFormat(pDesc->mColorFormat);
    createInfo.Width = pDesc->mWidth;
    createInfo.Height = pDesc->mHeight;
    createInfo.Levels = 1;
    createInfo.FaceCount = 1;
    createInfo.ArraySize = 2; // TODO: Support non multiview rendering
    createInfo.BufferCount = pDesc->mImageCount;
    createInfo.CreateFlags = 0;
    createInfo.UsageFlags = ovrSwapChainUsageFlags::VRAPI_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    ovrTextureSwapChain* swapChainTexture = vrapi_CreateTextureSwapChain4(&createInfo);
    ASSERT(swapChainTexture);

    if (pDesc->mColorFormat == getSupportedSwapchainFormat(pRenderer, pDesc, COLOR_SPACE_SDR_SRGB))
    {
        pQuest->isSrgb = true;
    }
    else
    {
        pQuest->isSrgb = false;
    }

    uint imageCount = vrapi_GetTextureSwapChainLength(swapChainTexture);

    ASSERT(imageCount >= pDesc->mImageCount);

    SwapChain* pSwapChain = (SwapChain*)tf_calloc(1, sizeof(SwapChain) + imageCount * 2 * sizeof(RenderTarget*) +
                                                         sizeof(VkExtent2D) * imageCount + sizeof(SwapChainDesc));
    pSwapChain->ppRenderTargets = (RenderTarget**)(pSwapChain + 1);
    pSwapChain->mVR.ppFragmentDensityMasks = (RenderTarget**)(pSwapChain->ppRenderTargets + imageCount);
    pSwapChain->mVR.pFragmentDensityTextureSizes = (VkExtent2D*)(pSwapChain->mVR.ppFragmentDensityMasks + imageCount);
    pSwapChain->mVk.pDesc = (SwapChainDesc*)(pSwapChain->mVR.pFragmentDensityTextureSizes + imageCount);
    ASSERT(pSwapChain);

    VkImage* images = (VkImage*)alloca(imageCount * 2 * sizeof(VkImage));

    for (int i = 0; i < imageCount; ++i)
    {
        images[i] = vrapi_GetTextureSwapChainBufferVulkan(swapChainTexture, i);
        ovrResult result = vrapi_GetTextureSwapChainBufferFoveationVulkan(swapChainTexture, i, &images[imageCount + i],
                                                                          &pSwapChain->mVR.pFragmentDensityTextureSizes[i].width,
                                                                          &pSwapChain->mVR.pFragmentDensityTextureSizes[i].height);
        if (result != ovrSuccess)
        {
            LOGF(eERROR, "Failed to get foveation textures from swap chain");
        }
    }

    RenderTargetDesc descColor = {};
    descColor.mWidth = pDesc->mWidth;
    descColor.mHeight = pDesc->mHeight;
    descColor.mDepth = 1;
    descColor.mArraySize = 1; // TODO: Support non multiview rendering
    descColor.mFormat = pDesc->mColorFormat;
    descColor.mClearValue = pDesc->mColorClearValue;
    descColor.mSampleCount = SAMPLE_COUNT_1;
    descColor.mSampleQuality = 0;
    descColor.mStartState = RESOURCE_STATE_PRESENT;
    descColor.mFlags |= TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
    if (pDesc->mFlags & SWAP_CHAIN_CREATION_FLAG_ENABLE_FOVEATED_RENDERING_VR)
    {
        descColor.mFlags |= TEXTURE_CREATION_FLAG_VR_FOVEATED_RENDERING;

        vrapi_SetPropertyInt(&pQuest->mJava, VRAPI_DYNAMIC_FOVEATION_ENABLED, true);
        vrapi_SetPropertyInt(&pQuest->mJava, VRAPI_FOVEATION_LEVEL, 4);
        pQuest->mFoveatedRenderingEnabled = true;
    }

    RenderTargetDesc descFragDensity = {};
    descFragDensity.mDepth = 1;
    descFragDensity.mArraySize = 1;
    descFragDensity.mFormat = TinyImageFormat_R8G8_UNORM;
    descFragDensity.mSampleCount = SAMPLE_COUNT_1;
    descFragDensity.mSampleQuality = 0;
    descFragDensity.mStartState = RESOURCE_STATE_SHADING_RATE_SOURCE;
    descFragDensity.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;

    // Populate the vk_image field and add the Vulkan texture objects
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        descColor.pNativeHandle = (void*)images[i];
        addRenderTarget(pRenderer, &descColor, &pSwapChain->ppRenderTargets[i]);

        descFragDensity.mWidth = pSwapChain->mVR.pFragmentDensityTextureSizes[i].width;
        descFragDensity.mHeight = pSwapChain->mVR.pFragmentDensityTextureSizes[i].height;
        descFragDensity.pNativeHandle = (void*)images[imageCount + i];
        addRenderTarget(pRenderer, &descFragDensity, &pSwapChain->mVR.ppFragmentDensityMasks[i]);
    }

    /************************************************************************/
    /************************************************************************/
    *pSwapChain->mVk.pDesc = *pDesc;
    pSwapChain->mEnableVsync = pDesc->mEnableVsync;
    pSwapChain->mImageCount = imageCount;
    pSwapChain->mVk.pSurface = NULL;
    pSwapChain->mVk.mPresentQueueFamilyIndex = 0;
    pSwapChain->mVk.pPresentQueue = NULL;
    pSwapChain->mVk.pSwapChain = NULL;
    pSwapChain->mFormat = pDesc->mColorFormat;

    pSwapChain->mVR.pSwapChain = swapChainTexture;

    *ppSwapChain = pSwapChain;
}

void hook_remove_swap_chain(Renderer* pRenderer, SwapChain* pSwapChain)
{
    for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
    {
        removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[i]);
        removeRenderTarget(pRenderer, pSwapChain->mVR.ppFragmentDensityMasks[i]);
    }

    if (pSwapChain->mVR.pSwapChain)
        vrapi_DestroyTextureSwapChain(pSwapChain->mVR.pSwapChain);

    if (pSwapChain)
        tf_free(pSwapChain);
}

void hook_acquire_next_image(SwapChain* pSwapChain, uint32_t* pImageIndex)
{
    ASSERT(vrapi_BeginFrame(pQuest->pOvr, pQuest->mFrameIndex) == ovrSuccess);
    *pImageIndex = pQuest->mFrameIndex % pSwapChain->mImageCount;
    pFragmentDensityMask = pSwapChain->mVR.ppFragmentDensityMasks[*pImageIndex];
}

inline ovrMatrix4f TanAngleMatrixFromProjectionYFlipped(const ovrMatrix4f* projection)
{
    /*
        A projection matrix goes from a view point to NDC, or -1 to 1 space.
        Scale and bias to convert that to a 0 to 1 space.
        const ovrMatrix3f m =
        { {
            { projection->M[0][0],                0.0f, projection->M[0][2] },
            {                0.0f, projection->M[1][1], projection->M[1][2] },
            {                0.0f,                0.0f,               -1.0f }
        } };
        // Note that there is no Y-flip because eye buffers have 0,0 = left-bottom.
        const ovrMatrix3f s = ovrMatrix3f_CreateScaling( 0.5f, 0.5f );
        const ovrMatrix3f t = ovrMatrix3f_CreateTranslation( 0.5f, 0.5f );
        const ovrMatrix3f r0 = ovrMatrix3f_Multiply( &s, &m );
        const ovrMatrix3f r1 = ovrMatrix3f_Multiply( &t, &r0 );
        return r1;
        clipZ = ( z * projection[2][2] + projection[2][3] ) / ( projection[3][2] * z )
        z = projection[2][3] / ( clipZ * projection[3][2] - projection[2][2] )
        z = ( projection[2][3] / projection[3][2] ) / ( clipZ - projection[2][2] / projection[3][2] )
    */
    const ovrMatrix4f tanAngleMatrix = { { { 0.5f * projection->M[0][0], 0.0f, 0.5f * projection->M[0][2] - 0.5f, 0.0f },
                                           { 0.0f, -0.5f * projection->M[1][1], -0.5f * projection->M[1][2] - 0.5f, 0.0f },
                                           { 0.0f, 0.0f, -1.0f, 0.0f },
                                           // Store the values to convert a clip-Z to a linear depth in the unused matrix elements.
                                           { projection->M[2][2], projection->M[2][3], projection->M[3][2], 1.0f } } };

    return tanAngleMatrix;
}

void hook_queue_present(const QueuePresentDesc* pQueuePresentDesc)
{
    SwapChain* pSwapChain = pQueuePresentDesc->pSwapChain;
    ASSERT(pSwapChain);

    ovrTracking2 headsetTracking = pQuest->mHeadsetTracking;

    ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
    layer.HeadPose = headsetTracking.HeadPose;
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
    {
        // TODO: Support non multiview rendering
        layer.Textures[eye].ColorSwapChain = pSwapChain->mVR.pSwapChain;
        layer.Textures[eye].SwapChainIndex = pQueuePresentDesc->mIndex;
        layer.Textures[eye].TexCoordsFromTanAngles = TanAngleMatrixFromProjectionYFlipped(&headsetTracking.Eye[eye].ProjectionMatrix);
    }

    ovrSubmitFrameDescription2 frameDesc = {};
    frameDesc.Flags = 0;
    frameDesc.SwapInterval = 1; // TODO: VSync
    frameDesc.FrameIndex = pQuest->mFrameIndex;
    frameDesc.DisplayTime = pQuest->mPredictedDisplayTime;
    frameDesc.LayerCount = 1;
    const ovrLayerHeader2* layers[] = { &layer.Header };
    frameDesc.Layers = layers;

    // Hand over the eye images to the time warp.
    vrapi_SubmitFrame2(pQuest->pOvr, &frameDesc);
}

#endif

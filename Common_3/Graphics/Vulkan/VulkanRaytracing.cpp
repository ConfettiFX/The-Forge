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

#include "../GraphicsConfig.h"

#ifdef VULKAN

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#include "../../Utilities/Interfaces/ILog.h"

#include "../../Utilities/Math/MathTypes.h"

// Renderer
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

#include "../Interfaces/IGraphics.h"
#include "../Interfaces/IRay.h"

#include "../../Utilities/Interfaces/IMemory.h"

extern VkAllocationCallbacks* GetAllocationCallbacks(VkObjectType objType);

struct Raytracing
{
    Renderer*                                       pRenderer;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR mRayTracingPipelineProperties;
};

struct AccelerationStructure
{
    Buffer*                              pASBuffer;
    uint64_t                             mASDeviceAddress;
    Buffer*                              pScratchBuffer;
    uint64_t                             mScratchBufferDeviceAddress;
    Buffer*                              pInstanceDescBuffer;
    VkAccelerationStructureGeometryKHR*  pGeometryDescs;
    uint32_t*                            pMaxPrimitiveCountPerGeometry;
    VkAccelerationStructureKHR           mAccelerationStructure;
    uint32_t                             mPrimitiveCount;
    uint32_t                             mDescCount;
    VkBuildAccelerationStructureFlagsKHR mFlags;
    VkAccelerationStructureTypeKHR       mType;
};

DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)

extern VkDeviceMemory get_vk_device_memory(Renderer* pRenderer, Buffer* pBuffer);
extern VkDeviceSize   get_vk_device_memory_offset(Renderer* pRenderer, Buffer* pBuffer);

extern uint32_t util_get_memory_type(uint32_t typeBits, const VkPhysicalDeviceMemoryProperties& memoryProperties,
                                     VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr);

static uint64_t GetBufferDeviceAddress(Renderer* pRenderer, Buffer* pBuffer)
{
    VkBufferDeviceAddressInfoKHR bufferDeviceAI = {};
    bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAI.buffer = pBuffer->mVk.pBuffer;
    return vkGetBufferDeviceAddressKHR(pRenderer->mVk.pDevice, &bufferDeviceAI);
}

static inline FORGE_CONSTEXPR VkAccelerationStructureTypeKHR ToVkASType(AccelerationStructureType type)
{
    return ACCELERATION_STRUCTURE_TYPE_BOTTOM == type ? VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
                                                      : VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
}

static inline FORGE_CONSTEXPR VkBuildAccelerationStructureFlagsKHR ToVkBuildASFlags(AccelerationStructureBuildFlags flags)
{
    VkBuildAccelerationStructureFlagsKHR ret = 0;
    if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION)
        ret |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
    if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE)
        ret |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY)
        ret |= VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR;
    if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE)
        ret |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD)
        ret |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE)
        ret |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

    return ret;
}

VkGeometryFlagsKHR util_to_vk_geometry_flags(AccelerationStructureGeometryFlags flags)
{
    VkGeometryFlagsKHR ret = 0;
    if (flags & ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE)
        ret |= VK_GEOMETRY_OPAQUE_BIT_KHR;
    if (flags & ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION)
        ret |= VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

    return ret;
}

VkGeometryInstanceFlagsKHR util_to_vk_instance_flags(AccelerationStructureInstanceFlags flags)
{
    VkGeometryInstanceFlagsKHR ret = 0;
    if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_OPAQUE)
        ret |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
    if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_NON_OPAQUE)
        ret |= VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR;
    if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE)
        ret |= VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE)
        ret |= VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR;

    return ret;
}

bool vk_initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing)
{
    ASSERT(pRenderer);
    ASSERT(ppRaytracing);

    if (!pRenderer->pGpu->mSettings.mRaytracingSupported)
    {
        return false;
    }

    Raytracing* pRaytracing = (Raytracing*)tf_calloc(1, sizeof(*pRaytracing));
    ASSERT(pRaytracing);

    // Get properties and features
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2KHR deviceProperties2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
                                                         &rayTracingPipelineProperties };
    vkGetPhysicalDeviceProperties2KHR(pRenderer->pGpu->mVk.pGpu, &deviceProperties2);

    pRaytracing->pRenderer = pRenderer;
    pRaytracing->mRayTracingPipelineProperties = rayTracingPipelineProperties;

    *ppRaytracing = pRaytracing;
    return true;
}

void vk_removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing)
{
    UNREF_PARAM(pRenderer);
    // Do nothing here because in case of Vulkan struct Raytracing contains
    // only shorthands
    tf_free(pRaytracing);
}

void vk_addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDesc* pDesc,
                                 AccelerationStructure** ppAccelerationStructure)
{
    ASSERT(pRaytracing);
    ASSERT(pRaytracing->pRenderer);
    ASSERT(pDesc);
    ASSERT(ppAccelerationStructure);

    Renderer* pRenderer = pRaytracing->pRenderer;

    size_t memSize = sizeof(AccelerationStructure);
    if (ACCELERATION_STRUCTURE_TYPE_BOTTOM == pDesc->mType)
    {
        memSize += pDesc->mBottom.mDescCount * sizeof(VkAccelerationStructureGeometryKHR);
        memSize += pDesc->mBottom.mDescCount * sizeof(uint32_t);
    }
    else
    {
        memSize += 1 * sizeof(VkAccelerationStructureGeometryKHR);
        memSize += 1 * sizeof(uint32_t);
    }

    AccelerationStructure* pAS = (AccelerationStructure*)tf_calloc(1, memSize);
    ASSERT(pAS);

    pAS->mFlags = ToVkBuildASFlags(pDesc->mFlags);
    pAS->mType = ToVkASType(pDesc->mType);
    pAS->pGeometryDescs = (VkAccelerationStructureGeometryKHR*)(pAS + 1);
    pAS->mPrimitiveCount = 0;
    // TODO these would ideally be freed as soon as the AS is built. Right now they stay alive until the AS is removed.
    pAS->pGeometryDescs = (VkAccelerationStructureGeometryKHR*)(pAS + 1);
    pAS->pMaxPrimitiveCountPerGeometry = (uint32_t*)(pAS->pGeometryDescs + pDesc->mBottom.mDescCount);

    VkDeviceSize scratchBufferSize = 0;

    if (ACCELERATION_STRUCTURE_TYPE_BOTTOM == pDesc->mType)
    {
        pAS->mDescCount = pDesc->mBottom.mDescCount;

        for (uint32_t j = 0; j < pAS->mDescCount; ++j)
        {
            AccelerationStructureGeometryDesc*  pGeom = &pDesc->mBottom.pGeometryDescs[j];
            VkAccelerationStructureGeometryKHR* pGeometry = &pAS->pGeometryDescs[j];
            *pGeometry = VkAccelerationStructureGeometryKHR({});
            pGeometry->sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            pGeometry->flags = util_to_vk_geometry_flags(pGeom->mFlags);
            pGeometry->geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            pGeometry->geometry.triangles = VkAccelerationStructureGeometryTrianglesDataKHR{};
            pGeometry->geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;

            uint32_t primitiveCount = 0;

            if (pGeom->mIndexCount)
            {
                ASSERT(pGeom->pIndexBuffer);

                VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress = {};
                indexBufferDeviceAddress.deviceAddress = GetBufferDeviceAddress(pRenderer, pGeom->pIndexBuffer) + pGeom->mIndexOffset;

                pGeometry->geometry.triangles.indexData = indexBufferDeviceAddress;
                pGeometry->geometry.triangles.indexType =
                    (INDEX_TYPE_UINT16 == pGeom->mIndexType) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

                primitiveCount = pGeom->mIndexCount / 3;
            }
            else
            {
                pGeometry->geometry.triangles.indexData = VkDeviceOrHostAddressConstKHR{};
                pGeometry->geometry.triangles.indexType = VK_INDEX_TYPE_NONE_KHR;

                // read 3 vertices at a time.
                primitiveCount = pGeom->mVertexCount / 3;
            }

            ASSERT(pGeom->pVertexBuffer);
            ASSERT(pGeom->mVertexCount);
            ASSERT(primitiveCount);

            VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress = {};
            vertexBufferDeviceAddress.deviceAddress = GetBufferDeviceAddress(pRenderer, pGeom->pVertexBuffer) + pGeom->mVertexOffset;

            pGeometry->geometry.triangles.vertexData = vertexBufferDeviceAddress;
            pGeometry->geometry.triangles.maxVertex = pGeom->mVertexCount;
            pGeometry->geometry.triangles.vertexStride = pGeom->mVertexStride;
            pGeometry->geometry.triangles.vertexFormat = (VkFormat)TinyImageFormat_ToVkFormat(pGeom->mVertexFormat);

            pAS->mPrimitiveCount += primitiveCount;
            pAS->pMaxPrimitiveCountPerGeometry[j] = primitiveCount;
        }

        // Get size info
        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = {};
        accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationStructureBuildGeometryInfo.flags = pAS->mFlags;
        accelerationStructureBuildGeometryInfo.geometryCount = pAS->mDescCount;
        accelerationStructureBuildGeometryInfo.pGeometries = pAS->pGeometryDescs;

        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = {};
        accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(pRenderer->mVk.pDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &accelerationStructureBuildGeometryInfo, pAS->pMaxPrimitiveCountPerGeometry,
                                                &accelerationStructureBuildSizesInfo);

        BufferDesc bufferDesc = {};
        bufferDesc.mDescriptors = DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE;
        bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_SHADER_DEVICE_ADDRESS |
                            BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
        bufferDesc.mStructStride = 0;
        bufferDesc.mFirstElement = 0;
        bufferDesc.mSize = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        bufferDesc.mStartState = RESOURCE_STATE_GENERIC_READ;
        addBuffer(pRenderer, &bufferDesc, &pAS->pASBuffer);

        VkAccelerationStructureCreateInfoKHR accelerationStructureCreate_info = {};
        accelerationStructureCreate_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreate_info.buffer = pAS->pASBuffer->mVk.pBuffer;
        accelerationStructureCreate_info.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        accelerationStructureCreate_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        CHECK_VKRESULT(vkCreateAccelerationStructureKHR(pRenderer->mVk.pDevice, &accelerationStructureCreate_info,
                                                        GetAllocationCallbacks(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR),
                                                        &pAS->mAccelerationStructure));

        VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo = {};
        accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        accelerationDeviceAddressInfo.accelerationStructure = pAS->mAccelerationStructure;
        pAS->mASDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(pRenderer->mVk.pDevice, &accelerationDeviceAddressInfo);

        scratchBufferSize = accelerationStructureBuildSizesInfo.buildScratchSize;
    }
    else
    {
        // This naming is a bit confusing now, since on the TLAS, the blases are the primitives.
        pAS->mDescCount = 1;
        pAS->mPrimitiveCount = pDesc->mTop.mDescCount;
        pAS->pMaxPrimitiveCountPerGeometry[0] = pAS->mPrimitiveCount;
        /************************************************************************/
        /*  Construct buffer with instances descriptions                        */
        /************************************************************************/
        VkAccelerationStructureInstanceKHR* instanceDescs = NULL;
        arrsetlen(instanceDescs, pDesc->mTop.mDescCount);
        // Silence PVS
        if (!instanceDescs)
        {
            ASSERT(false);
            return;
        }

        for (uint32_t i = 0; i < pDesc->mTop.mDescCount; ++i)
        {
            AccelerationStructureInstanceDesc* pInst = &pDesc->mTop.pInstanceDescs[i];

            instanceDescs[i].accelerationStructureReference = pInst->pBottomAS->mASDeviceAddress;
            instanceDescs[i].flags = util_to_vk_instance_flags(pInst->mFlags);
            instanceDescs[i].instanceShaderBindingTableRecordOffset =
                pInst->mInstanceContributionToHitGroupIndex; // NOTE(Alex): Not sure about this...
            instanceDescs[i].instanceCustomIndex = pInst->mInstanceID;
            instanceDescs[i].mask = pInst->mInstanceMask;
            memcpy(&instanceDescs[i].transform.matrix, pInst->mTransform, sizeof(float[12]));
        }

        BufferDesc instanceDesc = {};
        instanceDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_SHADER_DEVICE_ADDRESS |
                              BUFFER_CREATION_FLAG_ACCELERATION_STRUCTURE_BUILD_INPUT;
        instanceDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        instanceDesc.mSize = arrlenu(instanceDescs) * sizeof(instanceDescs[0]);
        addBuffer(pRenderer, &instanceDesc, &pAS->pInstanceDescBuffer);
        memcpy(pAS->pInstanceDescBuffer->pCpuMappedAddress, instanceDescs, instanceDesc.mSize);
        arrfree(instanceDescs);

        VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress = {};
        instanceDataDeviceAddress.deviceAddress = GetBufferDeviceAddress(pRenderer, pAS->pInstanceDescBuffer);

        VkAccelerationStructureGeometryKHR accelerationStructureGeometry = {};
        accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
        accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

        // Get size info
        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = {};
        accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        accelerationStructureBuildGeometryInfo.flags = pAS->mFlags;
        accelerationStructureBuildGeometryInfo.geometryCount = 1;
        accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = {};
        accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(pRenderer->mVk.pDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &accelerationStructureBuildGeometryInfo, &pAS->mPrimitiveCount,
                                                &accelerationStructureBuildSizesInfo);

        BufferDesc bufferDesc = {};
        bufferDesc.mDescriptors = DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE;
        bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_SHADER_DEVICE_ADDRESS |
                            BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
        bufferDesc.mStructStride = 0;
        bufferDesc.mFirstElement = 0;
        bufferDesc.mSize = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        bufferDesc.mStartState = RESOURCE_STATE_GENERIC_READ;
        addBuffer(pRenderer, &bufferDesc, &pAS->pASBuffer);

        VkAccelerationStructureCreateInfoKHR accelerationStructureCreate_info = {};
        accelerationStructureCreate_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreate_info.buffer = pAS->pASBuffer->mVk.pBuffer;
        accelerationStructureCreate_info.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        accelerationStructureCreate_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        CHECK_VKRESULT(vkCreateAccelerationStructureKHR(pRenderer->mVk.pDevice, &accelerationStructureCreate_info,
                                                        GetAllocationCallbacks(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR),
                                                        &pAS->mAccelerationStructure));

        VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo = {};
        accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        accelerationDeviceAddressInfo.accelerationStructure = pAS->mAccelerationStructure;
        pAS->mASDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(pRenderer->mVk.pDevice, &accelerationDeviceAddressInfo);

        memcpy(pAS->pGeometryDescs, &accelerationStructureGeometry, sizeof(VkAccelerationStructureGeometryKHR));

        scratchBufferSize = accelerationStructureBuildSizesInfo.buildScratchSize;
    }

    // Create scratch buffer
    BufferDesc scratchBufferDesc = {};
    scratchBufferDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
    scratchBufferDesc.mFlags = BUFFER_CREATION_FLAG_SHADER_DEVICE_ADDRESS | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
    scratchBufferDesc.mSize = scratchBufferSize;
    scratchBufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    addBuffer(pRenderer, &scratchBufferDesc, &pAS->pScratchBuffer);
    // Buffer device address
    pAS->mScratchBufferDeviceAddress = GetBufferDeviceAddress(pRenderer, pAS->pScratchBuffer);

    *ppAccelerationStructure = pAS;
}

void vk_cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc)
{
    UNREF_PARAM(pRaytracing);
    ASSERT(pDesc);

    AccelerationStructure* as = pDesc->pAccelerationStructure;

    VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo = {};
    accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    accelerationBuildGeometryInfo.type = as->mType;
    accelerationBuildGeometryInfo.flags = as->mFlags;
    accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    accelerationBuildGeometryInfo.dstAccelerationStructure = as->mAccelerationStructure;
    accelerationBuildGeometryInfo.geometryCount = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR == as->mType ? as->mDescCount : 1;
    accelerationBuildGeometryInfo.pGeometries = as->pGeometryDescs;
    accelerationBuildGeometryInfo.scratchData.deviceAddress = as->mScratchBufferDeviceAddress;

    VkAccelerationStructureBuildRangeInfoKHR* accelerationStructureBuildRangeInfos =
        (VkAccelerationStructureBuildRangeInfoKHR*)alloca(as->mDescCount * sizeof(VkAccelerationStructureBuildRangeInfoKHR));
    for (uint32_t i = 0; i < as->mDescCount; i++)
    {
        VkAccelerationStructureBuildRangeInfoKHR& accelerationStructureBuildRangeInfo = accelerationStructureBuildRangeInfos[i];
        accelerationStructureBuildRangeInfo = {};
        accelerationStructureBuildRangeInfo.primitiveCount = as->pMaxPrimitiveCountPerGeometry[i];
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
    }

    VkAccelerationStructureBuildRangeInfoKHR* accelerationBuildStructureRangeInfos[] = { accelerationStructureBuildRangeInfos };

    vkCmdBuildAccelerationStructuresKHR(pCmd->mVk.pCmdBuf, 1, &accelerationBuildGeometryInfo, accelerationBuildStructureRangeInfos);

    if (pDesc->mIssueRWBarrier)
    {
        BufferBarrier barrier = { as->pASBuffer, RESOURCE_STATE_ACCELERATION_STRUCTURE_WRITE, RESOURCE_STATE_ACCELERATION_STRUCTURE_READ };
        cmdResourceBarrier(pCmd, 1, &barrier, 0, NULL, 0, NULL);
    }
}

void vk_removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
    ASSERT(pRaytracing);
    ASSERT(pAccelerationStructure);

    removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pASBuffer);
    if (VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR == pAccelerationStructure->mType)
    {
        removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pInstanceDescBuffer);
    }

    if (pAccelerationStructure->pScratchBuffer)
    {
        removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pScratchBuffer);
        pAccelerationStructure->pScratchBuffer = NULL;
    }

    vkDestroyAccelerationStructureKHR(pRaytracing->pRenderer->mVk.pDevice, pAccelerationStructure->mAccelerationStructure,
                                      GetAllocationCallbacks(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR));

    tf_free(pAccelerationStructure);
}

void vk_removeAccelerationStructureScratch(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
    if (!pAccelerationStructure->pScratchBuffer)
    {
        return;
    }
    removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pScratchBuffer);
    pAccelerationStructure->pScratchBuffer = NULL;
}

void vk_FillRaytracingDescriptorData(AccelerationStructure* ppAccelerationStructures, VkAccelerationStructureKHR* pOutHandle)
{
    *pOutHandle = ppAccelerationStructures->mAccelerationStructure;
}

void initVulkanRaytracingFunctions()
{
    initRaytracing = vk_initRaytracing;
    removeRaytracing = vk_removeRaytracing;
    addAccelerationStructure = vk_addAccelerationStructure;
    removeAccelerationStructure = vk_removeAccelerationStructure;
    removeAccelerationStructureScratch = vk_removeAccelerationStructureScratch;
    cmdBuildAccelerationStructure = vk_cmdBuildAccelerationStructure;
}

#endif
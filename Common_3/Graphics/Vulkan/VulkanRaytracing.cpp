/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../../Utilities/Interfaces/ILog.h"

#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"
#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

// Renderer
#include "../Interfaces/IGraphics.h"
#include "../Interfaces/IRay.h"
#include "../../Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include "../../Utilities/Interfaces/IMemory.h"

#ifdef VK_RAYTRACING_AVAILABLE

extern VkAllocationCallbacks gVkAllocationCallbacks;

struct AccelerationStructureBottom
{
	Buffer*                              pVertexBuffer;
	Buffer*                              pIndexBuffer;
	Buffer*                              pASBuffer;
	VkAccelerationStructureKHR           mAccelerationStructure;
	uint64_t                             mStructureDeviceAddress; 
	VkAccelerationStructureGeometryKHR*  pGeometryDescs;
	VkBuildAccelerationStructureFlagsKHR mFlags;
	VkBuffer                             mScratchBufferBottom; 
	VkDeviceMemory                       mScratchBufferBottomMemory; 
	uint64_t                             mScratchBufferBottomDeviceAddress;
	uint32_t                             mDescCount;
	uint32_t                             mPrimitiveCount;
};

struct AccelerationStructure
{
	AccelerationStructureBottom          mBottomAS;
	Buffer*                              pInstanceDescBuffer;
	Buffer*                              pASBuffer;
	VkBuffer                             mScratchBufferTop;
	VkDeviceMemory                       mScratchBufferTopMemory;
	uint64_t                             mScratchBufferTopDeviceAddress;
	uint32_t                             mInstanceDescCount;
	uint64_t                             mScratchBufferSize;
	VkAccelerationStructureGeometryKHR   mInstanceBufferDesc;
	VkBuildAccelerationStructureFlagsKHR mFlags;
	VkAccelerationStructureKHR           mAccelerationStructure;
	uint64_t                             mStructureDeviceAddress;
	uint32_t                             mPrimitiveCount;
};

struct ShaderLocalData
{
	RootSignature*  pLocalRootSignature;
	DescriptorData* pRootData;
	uint32_t        mRootDataCount;
};

struct RaytracingShaderTable
{
	Pipeline*                       pPipeline;
	VkDeviceMemory                  mShaderTableMemory;
	void*                           pShaderTableMemoryMap; 
	VkBuffer                        pRaygenTableBuffer;
	VkStridedDeviceAddressRegionKHR mRaygenShaderEntry; 
	VkBuffer                        pMissTableBuffer;
	VkStridedDeviceAddressRegionKHR mMissShaderEntry;
	VkBuffer                        pHitGroupTableBuffer;
	VkStridedDeviceAddressRegionKHR mHitGroupShaderEntry;
	VkBuffer                        pCallableTableBuffer;
	VkStridedDeviceAddressRegionKHR mCallableShaderEntry;
	uint64_t                        mMaxEntrySize;
	uint64_t                        mMissRecordSize;
	uint64_t                        mHitGroupRecordSize;
	ShaderLocalData                 mRaygenLocalData;
	ShaderLocalData*				mHitMissLocalData;
};

//This structure is not defined in Vulkan headers but this layout is used on GPU side for
//top level AS
struct VkGeometryInstanceKHR
{
	float    transform[12];
	uint32_t instanceCustomIndex : 24;
	uint32_t mask : 8;
	uint32_t instanceOffset : 24;
	uint32_t flags : 8;
	uint64_t accelerationStructureHandle;
};

DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)

extern VkDeviceMemory get_vk_device_memory(Renderer* pRenderer, Buffer* pBuffer);
extern VkDeviceSize   get_vk_device_memory_offset(Renderer* pRenderer, Buffer* pBuffer);

extern uint32_t util_get_memory_type(
	uint32_t typeBits, const VkPhysicalDeviceMemoryProperties& memoryProperties, const VkMemoryPropertyFlags& properties,
	VkBool32* memTypeFound = nullptr);

uint64_t util_get_buffer_device_address(Renderer* pRenderer, Buffer* pBuffer)
{
	VkBufferDeviceAddressInfoKHR bufferDeviceAI = {};
	bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferDeviceAI.buffer = pBuffer->mVulkan.pVkBuffer;
	return vkGetBufferDeviceAddressKHR(pRenderer->mVulkan.pVkDevice, &bufferDeviceAI);
}

VkBuildAccelerationStructureFlagsKHR util_to_vk_acceleration_structure_build_flags(AccelerationStructureBuildFlags flags)
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
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE)
		ret |= VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE)
		ret |= VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR;

	return ret;
}

VkResult util_create_bottom_as(
	Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, uint64_t* pScratchBufferSize, AccelerationStructureBottom* pOut)
{
	ASSERT(pRaytracing);
	ASSERT(pRaytracing->pRenderer);
	ASSERT(pDesc);
	ASSERT(pScratchBufferSize);
	ASSERT(pDesc->mBottomASDesc);
	
	Renderer* pRenderer = pRaytracing->pRenderer;

	uint32_t numTriangles = 0; 
	AccelerationStructureBottom blas = {};

	blas.mDescCount = pDesc->mBottomASDesc->mDescCount;
	blas.mFlags = util_to_vk_acceleration_structure_build_flags(pDesc->mBottomASDesc->mFlags);
	blas.pGeometryDescs = (VkAccelerationStructureGeometryKHR*)tf_calloc(blas.mDescCount, sizeof(VkAccelerationStructureGeometryKHR));
	for (uint32_t j = 0; j < blas.mDescCount; ++j)
	{
		AccelerationStructureGeometryDesc*  pGeom = &pDesc->mBottomASDesc->pGeometryDescs[j];
		VkAccelerationStructureGeometryKHR* pGeometry = &blas.pGeometryDescs[j];
		*pGeometry = VkAccelerationStructureGeometryKHR({});
		pGeometry->sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		pGeometry->flags = util_to_vk_geometry_flags(pGeom->mFlags);
		pGeometry->geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		pGeometry->geometry.triangles = VkAccelerationStructureGeometryTrianglesDataKHR{};
		pGeometry->geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;

		blas.pIndexBuffer = {};
		if (pGeom->mIndexCount > 0)
		{
			SyncToken tok = {};
			BufferLoadDesc ibDesc = {};
			ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER | DESCRIPTOR_TYPE_SHADER_DEVICE_ADDRESS | DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT;
			ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			ibDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
			ibDesc.mDesc.mSize = (pGeom->mIndexType == INDEX_TYPE_UINT32 ? sizeof(uint32_t) : sizeof(uint16_t)) * pGeom->mIndexCount;
			ibDesc.pData = pGeom->mIndexType == INDEX_TYPE_UINT32 ? (void*)pGeom->pIndices32 : (void*)pGeom->pIndices16;
			ibDesc.ppBuffer = &blas.pIndexBuffer;
			addResource(&ibDesc, &tok);
			waitForToken(&tok); 

			VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress = {};
			indexBufferDeviceAddress.deviceAddress = util_get_buffer_device_address(pRenderer, blas.pIndexBuffer);

			pGeometry->geometry.triangles.indexData = indexBufferDeviceAddress;
			pGeometry->geometry.triangles.indexType = (INDEX_TYPE_UINT16 == pGeom->mIndexType) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

			numTriangles += pGeom->mIndexCount / 3;
		}
		else
		{
			pGeometry->geometry.triangles.indexData = VkDeviceOrHostAddressConstKHR{};
			pGeometry->geometry.triangles.indexType = VK_INDEX_TYPE_NONE_KHR;

			numTriangles += pGeom->mVertexCount - 2; 
		}

		SyncToken tok = {};
		BufferLoadDesc vbDesc = {};
		vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_SHADER_DEVICE_ADDRESS | DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT;
		vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vbDesc.mDesc.mSize = sizeof(float3) * pGeom->mVertexCount;
		vbDesc.pData = pGeom->pVertexArray;
		vbDesc.ppBuffer = &blas.pVertexBuffer;
		addResource(&vbDesc, &tok);
		waitForToken(&tok); 

		VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress = {}; 
		vertexBufferDeviceAddress.deviceAddress = util_get_buffer_device_address(pRenderer, blas.pVertexBuffer);

		pGeometry->geometry.triangles.vertexData = vertexBufferDeviceAddress;
		pGeometry->geometry.triangles.maxVertex = pGeom->mVertexCount; 
		pGeometry->geometry.triangles.vertexStride = sizeof(float3);
		pGeometry->geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;

		// #TODO (LEFT OVER FROM NV RAYTRACING)
		//if (pGeometry->geometry.triangles.vertexStride == sizeof(float))
		//	pGeometry->geometry.triangles.vertexFormat = VK_FORMAT_R32_SFLOAT;
		//else if (pGeometry->geometry.triangles.vertexStride == sizeof(float) * 2)
		//	pGeometry->geometry.triangles.vertexFormat = VK_FORMAT_R32G32_SFLOAT;
		//else if (pGeometry->geometry.triangles.vertexStride == sizeof(float) * 3)
		//	pGeometry->geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		//else if (pGeometry->geometry.triangles.vertexStride == sizeof(float) * 4)
		//	pGeometry->geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	}

	// Get size info
	VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = {};
	accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationStructureBuildGeometryInfo.flags = blas.mFlags;
	accelerationStructureBuildGeometryInfo.geometryCount = blas.mDescCount;
	accelerationStructureBuildGeometryInfo.pGeometries = blas.pGeometryDescs;

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = {};
	accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizesKHR(
		pRenderer->mVulkan.pVkDevice,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&accelerationStructureBuildGeometryInfo,
		&numTriangles,
		&accelerationStructureBuildSizesInfo);

	BufferDesc bufferDesc = {};
	bufferDesc.mDescriptors = DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE | DESCRIPTOR_TYPE_SHADER_DEVICE_ADDRESS;
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
	bufferDesc.mStructStride = 0;
	bufferDesc.mFirstElement = 0;
	bufferDesc.mSize = accelerationStructureBuildSizesInfo.accelerationStructureSize;
	bufferDesc.mStartState = RESOURCE_STATE_GENERIC_READ;
	addBuffer(pRenderer, &bufferDesc, &blas.pASBuffer);

	VkAccelerationStructureCreateInfoKHR accelerationStructureCreate_info = {};
	accelerationStructureCreate_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	accelerationStructureCreate_info.buffer = blas.pASBuffer->mVulkan.pVkBuffer;
	accelerationStructureCreate_info.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
	accelerationStructureCreate_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	CHECK_VKRESULT(vkCreateAccelerationStructureKHR(pRenderer->mVulkan.pVkDevice, &accelerationStructureCreate_info, &gVkAllocationCallbacks, &blas.mAccelerationStructure));

	VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo = {};
	accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	accelerationDeviceAddressInfo.accelerationStructure = blas.mAccelerationStructure;
	blas.mStructureDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(pRenderer->mVulkan.pVkDevice, &accelerationDeviceAddressInfo);

	blas.mPrimitiveCount = numTriangles;

	*pScratchBufferSize = accelerationStructureBuildSizesInfo.buildScratchSize;
	*pOut = blas;

	// Create scratch buffer
	// NOTE: Allocating buffer explicitly to ensure device address alignment. 

	// Buffer and memory
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = accelerationStructureBuildSizesInfo.buildScratchSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	CHECK_VKRESULT(vkCreateBuffer(pRenderer->mVulkan.pVkDevice, &bufferCreateInfo, nullptr, &pOut->mScratchBufferBottom));

	VkMemoryRequirements memoryRequirements{};
	vkGetBufferMemoryRequirements(pRenderer->mVulkan.pVkDevice, pOut->mScratchBufferBottom, &memoryRequirements);

	VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
	memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	//memoryAllocateInfo.memoryTypeIndex = 0;

	CHECK_VKRESULT(vkAllocateMemory(pRenderer->mVulkan.pVkDevice, &memoryAllocateInfo, nullptr, &pOut->mScratchBufferBottomMemory));
	CHECK_VKRESULT(vkBindBufferMemory(pRenderer->mVulkan.pVkDevice, pOut->mScratchBufferBottom, pOut->mScratchBufferBottomMemory, 0));
	
	// Buffer device address
	VkBufferDeviceAddressInfoKHR bufferDeviceAddresInfo{};
	bufferDeviceAddresInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferDeviceAddresInfo.buffer = pOut->mScratchBufferBottom;
	pOut->mScratchBufferBottomDeviceAddress = vkGetBufferDeviceAddressKHR(pRenderer->mVulkan.pVkDevice, &bufferDeviceAddresInfo);

	return VK_SUCCESS; 
}

VkResult util_create_top_as(
	Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, uint64_t* pScratchBufferSize, AccelerationStructure* pOut)
{
	ASSERT(pRaytracing);
	ASSERT(pRaytracing->pRenderer);
	ASSERT(pDesc);
	ASSERT(pScratchBufferSize);
	ASSERT(pOut);

	Renderer* pRenderer = pRaytracing->pRenderer; 

	VkTransformMatrixKHR transformMatrix = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f 
	};
	/************************************************************************/
	/*  Construct buffer with instances descriptions                        */
	/************************************************************************/
	VkAccelerationStructureInstanceKHR* instanceDescs = NULL;
	arrsetlen(instanceDescs, pDesc->mInstancesDescCount);
	for (uint32_t i = 0; i < pDesc->mInstancesDescCount; ++i)
	{
		AccelerationStructureInstanceDesc* pInst = &pDesc->pInstanceDescs[i];

		instanceDescs[i].accelerationStructureReference = pOut->mBottomAS.mStructureDeviceAddress;
		instanceDescs[i].flags = util_to_vk_instance_flags(pInst->mFlags);
		instanceDescs[i].transform = transformMatrix; 
		instanceDescs[i].instanceShaderBindingTableRecordOffset = pInst->mInstanceContributionToHitGroupIndex; // NOTE(Alex): Not sure about this...
		instanceDescs[i].instanceCustomIndex = pInst->mInstanceID;
		instanceDescs[i].mask = pInst->mInstanceMask;

		memcpy(&instanceDescs[i].transform, &transformMatrix, sizeof(VkTransformMatrixKHR)); //-V595
	}

	BufferDesc instanceDesc = {};
	instanceDesc.mDescriptors = DESCRIPTOR_TYPE_SHADER_DEVICE_ADDRESS | DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT;
	instanceDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	instanceDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	instanceDesc.mSize = arrlenu(instanceDescs) * sizeof(instanceDescs[0]);
	addBuffer(pRenderer, &instanceDesc, &pOut->pInstanceDescBuffer);
	if (instanceDescs)
		memcpy(pOut->pInstanceDescBuffer->pCpuMappedAddress, instanceDescs, instanceDesc.mSize);
	arrfree(instanceDescs);

	VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress = {};
	instanceDataDeviceAddress.deviceAddress = util_get_buffer_device_address(pRenderer, pOut->pInstanceDescBuffer);

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
	accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationStructureBuildGeometryInfo.geometryCount = 1;
	accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

	uint32_t primitive_count = 1;

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = {};
	accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizesKHR(
		pRenderer->mVulkan.pVkDevice,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&accelerationStructureBuildGeometryInfo,
		&primitive_count,
		&accelerationStructureBuildSizesInfo);

	BufferDesc bufferDesc = {};
	bufferDesc.mDescriptors = DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE | DESCRIPTOR_TYPE_SHADER_DEVICE_ADDRESS;
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
	bufferDesc.mStructStride = 0;
	bufferDesc.mFirstElement = 0;
	bufferDesc.mSize = accelerationStructureBuildSizesInfo.accelerationStructureSize;
	bufferDesc.mStartState = RESOURCE_STATE_GENERIC_READ;
	addBuffer(pRenderer, &bufferDesc, &pOut->pASBuffer);

	VkAccelerationStructureCreateInfoKHR accelerationStructureCreate_info = {};
	accelerationStructureCreate_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	accelerationStructureCreate_info.buffer = pOut->pASBuffer->mVulkan.pVkBuffer;
	accelerationStructureCreate_info.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
	accelerationStructureCreate_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	CHECK_VKRESULT(vkCreateAccelerationStructureKHR(pRenderer->mVulkan.pVkDevice, &accelerationStructureCreate_info, &gVkAllocationCallbacks, &pOut->mAccelerationStructure));

	VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo = {};
	accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	accelerationDeviceAddressInfo.accelerationStructure = pOut->mAccelerationStructure;
	pOut->mStructureDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(pRenderer->mVulkan.pVkDevice, &accelerationDeviceAddressInfo);

	memcpy(&pOut->mInstanceBufferDesc, &accelerationStructureGeometry, sizeof(VkAccelerationStructureGeometryKHR));
	pOut->mPrimitiveCount = primitive_count; 

	*pScratchBufferSize = accelerationStructureBuildSizesInfo.buildScratchSize;

	// Create scratch buffer
	// NOTE: Allocating buffer explicitly to ensure device address alignment. 

	// Buffer and memory
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = accelerationStructureBuildSizesInfo.buildScratchSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	CHECK_VKRESULT(vkCreateBuffer(pRenderer->mVulkan.pVkDevice, &bufferCreateInfo, nullptr, &pOut->mScratchBufferTop));

	VkMemoryRequirements memoryRequirements{};
	vkGetBufferMemoryRequirements(pRenderer->mVulkan.pVkDevice, pOut->mScratchBufferTop, &memoryRequirements);

	VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
	memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	//memoryAllocateInfo.memoryTypeIndex = 0;

	CHECK_VKRESULT(vkAllocateMemory(pRenderer->mVulkan.pVkDevice, &memoryAllocateInfo, nullptr, &pOut->mScratchBufferTopMemory));
	CHECK_VKRESULT(vkBindBufferMemory(pRenderer->mVulkan.pVkDevice, pOut->mScratchBufferTop, pOut->mScratchBufferTopMemory, 0));

	// Buffer device address
	VkBufferDeviceAddressInfoKHR bufferDeviceAddresInfo{};
	bufferDeviceAddresInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferDeviceAddresInfo.buffer = pOut->mScratchBufferTop;
	pOut->mScratchBufferTopDeviceAddress = vkGetBufferDeviceAddressKHR(pRenderer->mVulkan.pVkDevice, &bufferDeviceAddresInfo);

	return VK_SUCCESS; 
}

void util_build_acceleration_structure(
	VkCommandBuffer pCmd, uint64_t scratchBufferDeviceAddress, VkAccelerationStructureKHR pAccelerationStructure, 
	VkAccelerationStructureTypeKHR type, VkBuildAccelerationStructureFlagsKHR flags, const VkAccelerationStructureGeometryKHR* pGeometryDescs, 
	uint32_t geometriesCount, uint32_t primitiveCount)
{
	ASSERT(pCmd);

	VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo = {};
	accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationBuildGeometryInfo.type = type;
	accelerationBuildGeometryInfo.flags = flags;
	accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	accelerationBuildGeometryInfo.dstAccelerationStructure = pAccelerationStructure;
	accelerationBuildGeometryInfo.geometryCount = geometriesCount;
	accelerationBuildGeometryInfo.pGeometries = pGeometryDescs;
	accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBufferDeviceAddress;

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo = {};
	accelerationStructureBuildRangeInfo.primitiveCount = primitiveCount;
	accelerationStructureBuildRangeInfo.primitiveOffset = 0;
	accelerationStructureBuildRangeInfo.firstVertex = 0;
	accelerationStructureBuildRangeInfo.transformOffset = 0;
	VkAccelerationStructureBuildRangeInfoKHR* accelerationBuildStructureRangeInfos[] = { &accelerationStructureBuildRangeInfo };

	vkCmdBuildAccelerationStructuresKHR(pCmd, 1, &accelerationBuildGeometryInfo, accelerationBuildStructureRangeInfos);

	VkMemoryBarrier memoryBarrier;
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.pNext = nullptr;
	memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	vkCmdPipelineBarrier(
		pCmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &memoryBarrier, 0,
		0, 0, 0);
}

void util_calculate_max_shader_record_size(const char* const* pRecords, uint32_t shaderCount, uint64_t& maxShaderTableSize) {}

void util_fill_shader_identifiers(
	const char* const* pRecords, uint32_t shaderCount, uint64_t maxShaderTableSize, Pipeline* pTablePipeline, uint8_t* pBufferMap,
	Raytracing* pRaytracing, ShaderLocalData* mShaderLocalData, const uint8_t* shaderHandleStorage)
{
	uint32_t dstIndex = 0; 
	for (uint32_t idx = 0; idx < shaderCount; ++idx)
	{
		uint32_t      index = -1;
		const char* nameStr = pRecords[idx];
		ASSERT(nameStr != NULL);
		const char** it = pTablePipeline->mVulkan.ppShaderStageNames;
		const char* const* pEnd = pTablePipeline->mVulkan.ppShaderStageNames + pTablePipeline->mVulkan.mShaderStageCount;
		for (;it != pEnd; ++it)
		{
			if (strcmp(*it, nameStr) == 0)
				break;
		}
		if (it != pEnd)
		{
			index = (uint32_t)(it - pTablePipeline->mVulkan.ppShaderStageNames);
		}
		else
		{
			// This is allowed if we are provided with a hit group that has no shaders associated.
			// In all other cases this is an error.
			LOGF(
				LogLevel::eINFO, "Could not find shader name %s identifier. This is only valid if %s is a hit group with no shaders.",
				nameStr, nameStr);
			dstIndex += 1;
			continue;
		}

		uint64_t       currentPosition = maxShaderTableSize * dstIndex++;
		uint8_t*       dst = pBufferMap + currentPosition;
		size_t         handleSize = pRaytracing->mRayTracingPipelineProperties.shaderGroupHandleSize;
		const uint8_t* src = &shaderHandleStorage[index * handleSize];
		memcpy(dst, src, handleSize);
	}
}

bool vk_isRaytracingSupported(Renderer* pRenderer) 
{ 
	return pRenderer->mVulkan.mRaytracingSupported == 1; 
}

bool vk_initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing) 
{
	ASSERT(pRenderer);
	ASSERT(ppRaytracing);

	if (!isRaytracingSupported(pRenderer))
	{
		return false;
	}

	Raytracing* pRaytracing = (Raytracing*)tf_calloc(1, sizeof(*pRaytracing));
	ASSERT(pRaytracing);

	// Get properties and features
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
	VkPhysicalDeviceProperties2KHR deviceProperties2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR, &rayTracingPipelineProperties };
	vkGetPhysicalDeviceProperties2KHR(pRenderer->mVulkan.pVkActiveGPU, &deviceProperties2);

	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
	VkPhysicalDeviceFeatures2KHR deviceFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR, &accelerationStructureFeatures };
	vkGetPhysicalDeviceFeatures2KHR(pRenderer->mVulkan.pVkActiveGPU, &deviceFeatures2);

	pRaytracing->pRenderer = pRenderer;
	pRaytracing->mRayTracingPipelineProperties = rayTracingPipelineProperties;
	pRaytracing->mAccelerationStructureFeatures = accelerationStructureFeatures; 

	*ppRaytracing = pRaytracing;
	return true;
}

void vk_removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing) 
{
	//Do nothing here because in case of Vulkan struct Raytracing contains
	//only shorthands
	tf_free(pRaytracing);
}

void vk_addAccelerationStructure(
	Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, AccelerationStructure** ppAccelerationStructure)
{
	ASSERT(pRaytracing);
	ASSERT(pRaytracing->pRenderer);
	ASSERT(pDesc);
	ASSERT(ppAccelerationStructure);

	AccelerationStructure* pAccelerationStructure = (AccelerationStructure*)tf_calloc(1, sizeof(*pAccelerationStructure));
	ASSERT(pAccelerationStructure);

	uint64_t scratchBottomBufferSize = 0;
	CHECK_VKRESULT(util_create_bottom_as(pRaytracing, pDesc, &scratchBottomBufferSize, &pAccelerationStructure->mBottomAS));

	uint64_t scratchTopBufferSize = 0;
	pAccelerationStructure->mInstanceDescCount = pDesc->mInstancesDescCount;
	CHECK_VKRESULT(util_create_top_as(pRaytracing, pDesc, &scratchTopBufferSize, pAccelerationStructure));

	pAccelerationStructure->mScratchBufferSize = max(scratchBottomBufferSize, scratchTopBufferSize);
	pAccelerationStructure->mFlags = util_to_vk_acceleration_structure_build_flags(pDesc->mFlags);

	*ppAccelerationStructure = pAccelerationStructure;
}

void vk_cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc) 
{
	ASSERT(pDesc);
	ASSERT(pDesc->ppAccelerationStructures);

	for (unsigned i = 0; i < pDesc->mBottomASIndicesCount; ++i)
	{
		uint32_t               index = pDesc->pBottomASIndices[i];
		AccelerationStructure* pAccelerationStructure = pDesc->ppAccelerationStructures[index];

		util_build_acceleration_structure(
			pCmd->mVulkan.pVkCmdBuf, pAccelerationStructure->mBottomAS.mScratchBufferBottomDeviceAddress, pAccelerationStructure->mBottomAS.mAccelerationStructure,
			VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, pAccelerationStructure->mBottomAS.mFlags, pAccelerationStructure->mBottomAS.pGeometryDescs,
			pAccelerationStructure->mBottomAS.mDescCount, pAccelerationStructure->mBottomAS.mPrimitiveCount);
	}

	for (uint32_t i = 0; i < pDesc->mCount; ++i)
	{
		AccelerationStructure* pAccelerationStructure = pDesc->ppAccelerationStructures[i];

		util_build_acceleration_structure(
			pCmd->mVulkan.pVkCmdBuf, pAccelerationStructure->mScratchBufferTopDeviceAddress, pAccelerationStructure->mAccelerationStructure,
			VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, pAccelerationStructure->mFlags, &pAccelerationStructure->mInstanceBufferDesc, 1, 
			pAccelerationStructure->mPrimitiveCount);
	}
}

void vk_addRaytracingShaderTable(Raytracing* pRaytracing, const RaytracingShaderTableDesc* pDesc, RaytracingShaderTable** ppTable) 
{
	ASSERT(pRaytracing);
	ASSERT(pRaytracing->pRenderer);
	ASSERT(pDesc);
	ASSERT(pDesc->pPipeline);
	ASSERT(ppTable);

	Renderer* pRenderer = pRaytracing->pRenderer; 

	RaytracingShaderTable* pTable = (RaytracingShaderTable*)tf_calloc(1, sizeof(*pTable));
	ASSERT(pTable);

	pTable->pPipeline = pDesc->pPipeline;

	const uint32_t raygenCount = 1; 
	const uint32_t missCount = pDesc->mMissShaderCount;
	const uint32_t hitCount = pDesc->mHitGroupCount;
	uint64_t       maxShaderTableSize = 0;

	/************************************************************************/
	// Calculate max size for each element in the shader table
	/************************************************************************/

	util_calculate_max_shader_record_size(&pDesc->pRayGenShader, 1, maxShaderTableSize);
	util_calculate_max_shader_record_size(pDesc->pMissShaders, pDesc->mMissShaderCount, maxShaderTableSize);
	util_calculate_max_shader_record_size(pDesc->pHitGroups, pDesc->mHitGroupCount, maxShaderTableSize);

	/************************************************************************/
	// Align max size
	/************************************************************************/

	uint64_t baseAlignment = pRaytracing->mRayTracingPipelineProperties.shaderGroupBaseAlignment;
	const uint32_t groupHandleSize = pRaytracing->mRayTracingPipelineProperties.shaderGroupHandleSize;
	maxShaderTableSize = round_up_64(groupHandleSize + maxShaderTableSize, baseAlignment);
	pTable->mMaxEntrySize = maxShaderTableSize;

	/************************************************************************/
	// Create shader table buffers
	/************************************************************************/

	// NOTE: We allocate these buffers explicitly to ensure that shader table buffer device addresses
	// are properly aligned on AMD GPUs. 

	VkDevice device = pRenderer->mVulkan.pVkDevice;

	VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	uint64_t raygenSize = maxShaderTableSize * raygenCount;
	uint64_t missSize = maxShaderTableSize * missCount;
	uint64_t hitSize = maxShaderTableSize * hitCount;

	VkBufferCreateInfo bufferCreate = {};
	bufferCreate.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreate.usage = usageFlags;
	bufferCreate.size = raygenSize;
	CHECK_VKRESULT(vkCreateBuffer(device, &bufferCreate, nullptr, &pTable->pRaygenTableBuffer));
	bufferCreate.size = missSize;
	CHECK_VKRESULT(vkCreateBuffer(device, &bufferCreate, nullptr, &pTable->pMissTableBuffer));
	bufferCreate.size = hitSize;
	CHECK_VKRESULT(vkCreateBuffer(device, &bufferCreate, nullptr, &pTable->pHitGroupTableBuffer));

	VkMemoryRequirements memReqs = {};
	vkGetBufferMemoryRequirements(device, pTable->pRaygenTableBuffer, &memReqs); 

	raygenSize = round_up_64(raygenSize, memReqs.alignment); 
	missSize = round_up_64(missSize, memReqs.alignment); 
	hitSize = round_up_64(hitSize, memReqs.alignment);
	uint64_t allocSize = raygenSize + missSize + hitSize;

	VkPhysicalDeviceMemoryProperties memProps = {};
	vkGetPhysicalDeviceMemoryProperties(pRenderer->mVulkan.pVkActiveGPU, &memProps);

	VkMemoryAllocateFlagsInfoKHR allocFlagsInfo = {};
	allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

	VkMemoryAllocateInfo memAlloc = {};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAlloc.allocationSize = allocSize; 
	memAlloc.memoryTypeIndex = util_get_memory_type(memReqs.memoryTypeBits, memProps, memoryFlags); 
	memAlloc.pNext = &allocFlagsInfo;
	CHECK_VKRESULT(vkAllocateMemory(device, &memAlloc, nullptr, &pTable->mShaderTableMemory));

	CHECK_VKRESULT(vkBindBufferMemory(device, pTable->pRaygenTableBuffer, pTable->mShaderTableMemory, 0));
	CHECK_VKRESULT(vkBindBufferMemory(device, pTable->pMissTableBuffer, pTable->mShaderTableMemory, raygenSize));
	CHECK_VKRESULT(vkBindBufferMemory(device, pTable->pHitGroupTableBuffer, pTable->mShaderTableMemory, raygenSize + missSize));

	VkBufferDeviceAddressInfoKHR bufferDeviceAI = {};
	bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;

	bufferDeviceAI.buffer = pTable->pRaygenTableBuffer;
	pTable->mRaygenShaderEntry.deviceAddress = vkGetBufferDeviceAddressKHR(device, &bufferDeviceAI);
	pTable->mRaygenShaderEntry.stride = pTable->mMaxEntrySize;
	pTable->mRaygenShaderEntry.size = raygenCount * pTable->mMaxEntrySize;
	ASSERT(pTable->mRaygenShaderEntry.deviceAddress % baseAlignment == 0);

	bufferDeviceAI.buffer = pTable->pMissTableBuffer;
	pTable->mMissShaderEntry.deviceAddress = vkGetBufferDeviceAddressKHR(device, &bufferDeviceAI);
	pTable->mMissShaderEntry.stride = pTable->mMaxEntrySize;
	pTable->mMissShaderEntry.size = missCount * pTable->mMaxEntrySize;
	ASSERT(pTable->mMissShaderEntry.deviceAddress % baseAlignment == 0);

	bufferDeviceAI.buffer = pTable->pHitGroupTableBuffer;
	pTable->mHitGroupShaderEntry.deviceAddress = vkGetBufferDeviceAddressKHR(device, &bufferDeviceAI);
	pTable->mHitGroupShaderEntry.stride = pTable->mMaxEntrySize;
	pTable->mHitGroupShaderEntry.size = hitCount * pTable->mMaxEntrySize;
	ASSERT(pTable->mHitGroupShaderEntry.deviceAddress % baseAlignment == 0);

	CHECK_VKRESULT(vkMapMemory(device, pTable->mShaderTableMemory, 0, allocSize, 0, &pTable->pShaderTableMemoryMap));

	// TODO: If a situation arises where callable shaders are needed, add buffers and implement properly here.
	pTable->mCallableShaderEntry.deviceAddress = 0; 
	pTable->mCallableShaderEntry.stride = 0; // pTable->mMaxEntrySize;
	pTable->mCallableShaderEntry.size = 0; // callableCount * pTable->mMaxEntrySize;

	/************************************************************************/
	// Copy shader identifiers into the buffer
	/************************************************************************/

	uint32_t groupCount = (uint32_t)pDesc->pPipeline->mVulkan.mShaderStageCount;
	uint8_t* shaderHandleStorage = (uint8_t*)tf_calloc(groupCount, sizeof(uint8_t) * groupHandleSize);

	uint8_t* pRaygenMap   = (uint8_t*)pTable->pShaderTableMemoryMap;
	uint8_t* pMissMap     = (uint8_t*)pTable->pShaderTableMemoryMap + raygenSize;
	uint8_t* pHitGroupMap = (uint8_t*)pTable->pShaderTableMemoryMap + raygenSize + missSize;

	vkGetRayTracingShaderGroupHandlesKHR(
		pRaytracing->pRenderer->mVulkan.pVkDevice, pDesc->pPipeline->mVulkan.pVkPipeline, 0, groupCount, groupHandleSize * groupCount,
		shaderHandleStorage);

	arrsetlen(pTable->mHitMissLocalData, pDesc->mMissShaderCount + pDesc->mHitGroupCount + 1);
	util_fill_shader_identifiers(
		&pDesc->pRayGenShader, 1, maxShaderTableSize, pTable->pPipeline, pRaygenMap, pRaytracing, pTable->mHitMissLocalData, shaderHandleStorage);

	pTable->mMissRecordSize = maxShaderTableSize * pDesc->mMissShaderCount;
	util_fill_shader_identifiers(
		pDesc->pMissShaders, pDesc->mMissShaderCount, maxShaderTableSize, pTable->pPipeline, pMissMap, pRaytracing, &pTable->mHitMissLocalData[1],
		shaderHandleStorage);

	pTable->mHitGroupRecordSize = maxShaderTableSize * pDesc->mHitGroupCount;
	util_fill_shader_identifiers(
		pDesc->pHitGroups, pDesc->mHitGroupCount, maxShaderTableSize, pTable->pPipeline, pHitGroupMap, pRaytracing,
		&pTable->mHitMissLocalData[1 + pDesc->mMissShaderCount], shaderHandleStorage);

	vkUnmapMemory(device, pTable->mShaderTableMemory); 

	tf_free(shaderHandleStorage);

	*ppTable = pTable;
}

void vk_cmdDispatchRays(Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc) 
{
	RaytracingShaderTable* pTable = pDesc->pShaderTable;

	vkCmdTraceRaysKHR(
		pCmd->mVulkan.pVkCmdBuf,
		&pTable->mRaygenShaderEntry,
		&pTable->mMissShaderEntry,
		&pTable->mHitGroupShaderEntry,
		&pTable->mCallableShaderEntry,
		pDesc->mWidth,
		pDesc->mHeight,
		1
	);
}

void vk_removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure) 
{
	ASSERT(pRaytracing);
	ASSERT(pAccelerationStructure);

	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pASBuffer);
	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pInstanceDescBuffer);

	if (pAccelerationStructure->mBottomAS.mScratchBufferBottomMemory)
		vkFreeMemory(pRaytracing->pRenderer->mVulkan.pVkDevice, pAccelerationStructure->mBottomAS.mScratchBufferBottomMemory, nullptr);
	if (pAccelerationStructure->mBottomAS.mScratchBufferBottom)
		vkDestroyBuffer(pRaytracing->pRenderer->mVulkan.pVkDevice, pAccelerationStructure->mBottomAS.mScratchBufferBottom, nullptr);

	if (pAccelerationStructure->mScratchBufferTopMemory)
		vkFreeMemory(pRaytracing->pRenderer->mVulkan.pVkDevice, pAccelerationStructure->mScratchBufferTopMemory, nullptr);
	if (pAccelerationStructure->mScratchBufferTop)
		vkDestroyBuffer(pRaytracing->pRenderer->mVulkan.pVkDevice, pAccelerationStructure->mScratchBufferTop, nullptr);

	vkDestroyAccelerationStructureKHR(
		pRaytracing->pRenderer->mVulkan.pVkDevice, pAccelerationStructure->mAccelerationStructure, &gVkAllocationCallbacks);

	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->mBottomAS.pASBuffer);
	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->mBottomAS.pVertexBuffer);
	if (pAccelerationStructure->mBottomAS.pIndexBuffer->mVulkan.pVkBuffer != VK_NULL_HANDLE)
		removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->mBottomAS.pIndexBuffer);
	vkDestroyAccelerationStructureKHR(
		pRaytracing->pRenderer->mVulkan.pVkDevice, pAccelerationStructure->mBottomAS.mAccelerationStructure, &gVkAllocationCallbacks);

	tf_free(pAccelerationStructure->mBottomAS.pGeometryDescs);
	tf_free(pAccelerationStructure);
}

void vk_removeAccelerationStructureScratch(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure) 
{
	// NOT IMPLEMENTED
}

void vk_removeRaytracingShaderTable(Raytracing* pRaytracing, RaytracingShaderTable* pTable) 
{
	ASSERT(pRaytracing);
	ASSERT(pTable);

	VkDevice device = pRaytracing->pRenderer->mVulkan.pVkDevice;

	vkDestroyBuffer(device, pTable->pRaygenTableBuffer, nullptr);
	vkDestroyBuffer(device, pTable->pMissTableBuffer, nullptr);
	vkDestroyBuffer(device, pTable->pHitGroupTableBuffer, nullptr);

	vkFreeMemory(device, pTable->mShaderTableMemory, nullptr); 

	arrfree(pTable->mHitMissLocalData);
	tf_free(pTable);
}

void vk_addRaytracingPipeline(const PipelineDesc* pMainDesc, Pipeline** ppPipeline) 
{
	const RaytracingPipelineDesc* pDesc = &pMainDesc->mRaytracingDesc;
	VkPipelineCache               psoCache = pMainDesc->pCache ? pMainDesc->pCache->mVulkan.pCache : VK_NULL_HANDLE;

	Pipeline* pResult = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
	ASSERT(pResult);

	pResult->mVulkan.mType = PIPELINE_TYPE_RAYTRACING;
	VkPipelineShaderStageCreateInfo*		stages = NULL;
	VkRayTracingShaderGroupCreateInfoKHR*	groups = NULL;
	/************************************************************************/
	// Generate Stage Names
	/************************************************************************/
	arrsetcap(stages, 1 + pDesc->mMissShaderCount + pDesc->mHitGroupCount);
	arrsetcap(groups, 1 + pDesc->mMissShaderCount + pDesc->mHitGroupCount);
	pResult->mVulkan.mShaderStageCount = 0;
	pResult->mVulkan.ppShaderStageNames = (const char**)tf_calloc(1 + pDesc->mMissShaderCount + pDesc->mHitGroupCount * 3, sizeof(char*));

	//////////////////////////////////////////////////////////////////////////
	//1. Ray-gen shader
	{
		VkPipelineShaderStageCreateInfo stageCreateInfo = {};
		stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageCreateInfo.pNext = nullptr;
		stageCreateInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		stageCreateInfo.module = pDesc->pRayGenShader->mVulkan.pShaderModules[0];
		stageCreateInfo.pName = "main";
		stageCreateInfo.flags = 0;
		stageCreateInfo.pSpecializationInfo = pDesc->pRayGenShader->mVulkan.pSpecializationInfo;
		arrpush(stages, stageCreateInfo);

		pResult->mVulkan.ppShaderStageNames[pResult->mVulkan.mShaderStageCount] = pDesc->pRayGenShader->mVulkan.pEntryNames[0];
		pResult->mVulkan.mShaderStageCount += 1;

		VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
		groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		groupInfo.pNext = nullptr;
		groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		groupInfo.generalShader = (uint32_t)arrlen(stages) - 1;
		groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
		groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
		groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
		arrpush(groups, groupInfo);
	}

	//////////////////////////////////////////////////////////////////////////
	//2. Miss shaders
	{
		VkPipelineShaderStageCreateInfo stageCreateInfo = {};
		stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageCreateInfo.pNext = nullptr;
		stageCreateInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
		stageCreateInfo.module = VK_NULL_HANDLE;
		stageCreateInfo.pName = "main";
		stageCreateInfo.flags = 0;

		VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
		groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		groupInfo.pNext = nullptr;
		groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		groupInfo.generalShader = VK_SHADER_UNUSED_KHR;
		groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
		groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
		groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
		for (uint32_t i = 0; i < pDesc->mMissShaderCount; ++i)
		{
			stageCreateInfo.module = pDesc->ppMissShaders[i]->mVulkan.pShaderModules[0];
			stageCreateInfo.pSpecializationInfo = pDesc->ppMissShaders[i]->mVulkan.pSpecializationInfo;
			arrpush(stages, stageCreateInfo);

			pResult->mVulkan.ppShaderStageNames[pResult->mVulkan.mShaderStageCount] = pDesc->ppMissShaders[i]->mVulkan.pEntryNames[0];
			pResult->mVulkan.mShaderStageCount += 1;

			groupInfo.generalShader = (uint32_t)arrlen(stages) - 1;
			arrpush(groups, groupInfo);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//3. Hit group
	{
		VkPipelineShaderStageCreateInfo stageCreateInfo = {};
		stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageCreateInfo.pNext = nullptr;
		stageCreateInfo.stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		stageCreateInfo.module = VK_NULL_HANDLE;
		stageCreateInfo.pName = "main";
		stageCreateInfo.flags = 0;

		VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
		groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		groupInfo.pNext = nullptr;
		groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;

		for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
		{
			groupInfo.generalShader = VK_SHADER_UNUSED_KHR;
			groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
			groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
			groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

			if (pDesc->pHitGroups[i].pIntersectionShader)
			{
				stageCreateInfo.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
				stageCreateInfo.module = pDesc->pHitGroups[i].pIntersectionShader->mVulkan.pShaderModules[0];
				stageCreateInfo.pSpecializationInfo = pDesc->pHitGroups[i].pIntersectionShader->mVulkan.pSpecializationInfo;
				arrpush(stages, stageCreateInfo);
				pResult->mVulkan.ppShaderStageNames[pResult->mVulkan.mShaderStageCount] = pDesc->pHitGroups[i].pHitGroupName;
				pResult->mVulkan.mShaderStageCount += 1;

				groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
				groupInfo.intersectionShader = (uint32_t)arrlen(stages) - 1;
			}
			if (pDesc->pHitGroups[i].pAnyHitShader)
			{
				stageCreateInfo.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
				stageCreateInfo.module = pDesc->pHitGroups[i].pAnyHitShader->mVulkan.pShaderModules[0];
				stageCreateInfo.pSpecializationInfo = pDesc->pHitGroups[i].pAnyHitShader->mVulkan.pSpecializationInfo;
				arrpush(stages, stageCreateInfo);
				pResult->mVulkan.ppShaderStageNames[pResult->mVulkan.mShaderStageCount] = pDesc->pHitGroups[i].pHitGroupName;
				pResult->mVulkan.mShaderStageCount += 1;

				groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
				groupInfo.anyHitShader = (uint32_t)arrlen(stages) - 1;
			}
			if (pDesc->pHitGroups[i].pClosestHitShader)
			{
				stageCreateInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
				stageCreateInfo.module = pDesc->pHitGroups[i].pClosestHitShader->mVulkan.pShaderModules[0];
				stageCreateInfo.pSpecializationInfo = pDesc->pHitGroups[i].pClosestHitShader->mVulkan.pSpecializationInfo;
				arrpush(stages, stageCreateInfo);
				pResult->mVulkan.ppShaderStageNames[pResult->mVulkan.mShaderStageCount] = pDesc->pHitGroups[i].pHitGroupName;
				pResult->mVulkan.mShaderStageCount += 1;

				groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
				groupInfo.closestHitShader = (uint32_t)arrlen(stages) - 1;
			}
			arrpush(groups, groupInfo);
		}
	}
	/************************************************************************/
	// Create Pipeline
	/************************************************************************/

	uint32_t maxRecursionDepth = min(pDesc->mMaxTraceRecursionDepth, pDesc->pRaytracing->mRayTracingPipelineProperties.maxRayRecursionDepth);

	VkRayTracingPipelineCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	createInfo.flags = 0;    //VkPipelineCreateFlagBits
	createInfo.stageCount = (uint32_t)arrlen(stages);
	createInfo.pStages = stages;
	createInfo.groupCount = (uint32_t)arrlen(groups);    //ray-gen groups
	createInfo.pGroups = groups;
	createInfo.maxPipelineRayRecursionDepth = maxRecursionDepth;
	createInfo.layout = pDesc->pGlobalRootSignature->mVulkan.pPipelineLayout;
	createInfo.basePipelineHandle = VK_NULL_HANDLE;
	createInfo.basePipelineIndex = 0;

	CHECK_VKRESULT(vkCreateRayTracingPipelinesKHR(
		pDesc->pRaytracing->pRenderer->mVulkan.pVkDevice, VK_NULL_HANDLE, psoCache, 1, &createInfo, &gVkAllocationCallbacks,
		&pResult->mVulkan.pVkPipeline));

	arrfree(stages);
	arrfree(groups);

	*ppPipeline = pResult;
}

void vk_FillRaytracingDescriptorData(AccelerationStructure* ppAccelerationStructures, VkAccelerationStructureKHR* pOutHandle)
{
	*pOutHandle = ppAccelerationStructures->mAccelerationStructure;
}

#else

bool vk_isRaytracingSupported(Renderer* pRenderer) { return false; }

bool vk_initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing) { return false; }

void vk_removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing) {}

void vk_addAccelerationStructure(
	Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, AccelerationStructure** ppAccelerationStructure)
{
}

void vk_cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc) {}

void vk_addRaytracingShaderTable(Raytracing* pRaytracing, const RaytracingShaderTableDesc* pDesc, RaytracingShaderTable** ppTable) {}

void vk_cmdDispatchRays(Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc) {}

void vk_removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure) {}

void vk_removeAccelerationStructureScratch(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure) {}

void vk_removeRaytracingShaderTable(Raytracing* pRaytracing, RaytracingShaderTable* pTable) {}

#endif

void initVulkanRaytracingFunctions()
{
	isRaytracingSupported = vk_isRaytracingSupported;
	initRaytracing = vk_initRaytracing;
	removeRaytracing = vk_removeRaytracing;
	addAccelerationStructure = vk_addAccelerationStructure;
	removeAccelerationStructure = vk_removeAccelerationStructure;
	removeAccelerationStructureScratch = vk_removeAccelerationStructureScratch;
	addRaytracingShaderTable = vk_addRaytracingShaderTable;
	removeRaytracingShaderTable = vk_removeRaytracingShaderTable;
	cmdBuildAccelerationStructure = vk_cmdBuildAccelerationStructure;
	cmdDispatchRays = vk_cmdDispatchRays;
}
#endif
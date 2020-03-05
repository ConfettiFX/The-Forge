#ifdef VULKAN

#include "../../OS/Interfaces/ILog.h"

#include "../../ThirdParty/OpenSource/EASTL/sort.h"

// Renderer
#include "../IRenderer.h"
#include "../IRay.h"
#include "../IResourceLoader.h"

#include "../../OS/Interfaces/IMemory.h"

#ifdef ENABLE_RAYTRACING

struct AccelerationStructureBottom
{
	Buffer*											pVertexBuffer;
	Buffer*											pIndexBuffer;
	Buffer*											pASBuffer;
	VkAccelerationStructureNV						pAccelerationStructure;
	VkGeometryNV*									pGeometryDescs;
	VkBuildAccelerationStructureFlagsNV				mFlags;
	uint32_t										mDescCount;
};

struct AccelerationStructure
{
	Buffer*		pInstanceDescBuffer;
	Buffer*		pASBuffer;
	Buffer*		pScratchBuffer;
	AccelerationStructureBottom* ppBottomAS;
	uint32_t	mInstanceDescCount;
	uint32_t	mBottomASCount;
	uint32_t	mScratchBufferSize;
	VkBuildAccelerationStructureFlagsNV mFlags;
	VkAccelerationStructureNV mAccelerationStructure;
};

struct ShaderLocalData
{
	RootSignature*     pLocalRootSignature;
	DescriptorData*    pRootData;
	uint32_t           mRootDataCount;
};

struct RaytracingShaderTable
{
	Pipeline*			pPipeline;
	Buffer*				pBuffer;
	uint64_t			mMaxEntrySize;
	uint64_t			mMissRecordSize;
	uint64_t			mHitGroupRecordSize;
	ShaderLocalData		mRaygenLocalData;
	eastl::vector<ShaderLocalData> mHitMissLocalData;
};

//This structure is not defined in Vulkan headers but this layout is used on GPU side for
//top level AS
struct VkGeometryInstanceNV
{
	float          transform[12];
	uint32_t       instanceCustomIndex : 24;
	uint32_t       mask : 8;
	uint32_t       instanceOffset : 24;
	uint32_t       flags : 8;
	uint64_t       accelerationStructureHandle;
};

extern void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
extern void removeBuffer(Renderer* pRenderer, Buffer* p_buffer);

extern VkDeviceMemory get_vk_device_memory(Renderer* pRenderer, Buffer* pBuffer);
extern VkDeviceSize get_vk_device_memory_offset(Renderer* pRenderer, Buffer* pBuffer);

VkBuildAccelerationStructureFlagsNV util_to_vk_acceleration_structure_build_flags(AccelerationStructureBuildFlags flags);
VkGeometryFlagsNV util_to_vk_geometry_flags(AccelerationStructureGeometryFlags flags);
VkGeometryInstanceFlagsNV util_to_vk_instance_flags(AccelerationStructureInstanceFlags flags);

bool isRaytracingSupported(Renderer* pRenderer)
{
	return pRenderer->mRaytracingExtension == 1;
}

bool initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing)
{
	ASSERT(pRenderer);
	ASSERT(ppRaytracing);

	if (!isRaytracingSupported(pRenderer)) return false;

	Raytracing* pRaytracing = (Raytracing*)conf_calloc(1, sizeof(*pRaytracing));
	ASSERT(pRaytracing);

	VkPhysicalDeviceRayTracingPropertiesNV gpuRaytracingProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV };
	VkPhysicalDeviceProperties2KHR gpuProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR, &gpuRaytracingProperties };
	vkGetPhysicalDeviceProperties2KHR(pRenderer->pVkActiveGPU, &gpuProperties);

	pRaytracing->pRenderer = pRenderer;
	pRaytracing->mRayTracingProperties = gpuRaytracingProperties;

	*ppRaytracing = pRaytracing;
	return true;
}

void removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing)
{
	//Do nothing here because in case of Vulkan struct Raytracing contains
	//only shorthands
	conf_free(pRaytracing);
}

AccelerationStructureBottom* createBottomAS(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, uint32_t* pScratchBufferSize)
{
	ASSERT(pRaytracing);
	ASSERT(pDesc);
	ASSERT(pScratchBufferSize);
	ASSERT(pDesc->mBottomASDescsCount > 0);

	uint32_t scratchBufferSize = 0;
	AccelerationStructureBottom* pResult = (AccelerationStructureBottom*)conf_calloc(pDesc->mBottomASDescsCount, sizeof(AccelerationStructureBottom));

	for (uint32_t i = 0; i < pDesc->mBottomASDescsCount; ++i)
	{
		pResult[i].mDescCount = pDesc->mBottomASDescs[i].mDescCount;
		pResult[i].mFlags = util_to_vk_acceleration_structure_build_flags(pDesc->mBottomASDescs[i].mFlags);
		pResult[i].pGeometryDescs = (VkGeometryNV*)conf_calloc(pResult[i].mDescCount, sizeof(VkGeometryNV));
		for (uint32_t j = 0; j < pResult[i].mDescCount; ++j)
		{
			AccelerationStructureGeometryDesc* pGeom = &pDesc->mBottomASDescs[i].pGeometryDescs[j];
			VkGeometryNV* pGeometry = &pResult[i].pGeometryDescs[j];
			*pGeometry = VkGeometryNV({});
			pGeometry->sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
			pGeometry->flags = util_to_vk_geometry_flags(pGeom->mFlags);
			pGeometry->geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
			pGeometry->geometry.triangles = VkGeometryTrianglesNV{};
			pGeometry->geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;

			pResult[i].pIndexBuffer = {};
			if (pGeom->indicesCount > 0)
			{
				BufferLoadDesc ibDesc = {};
				ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
				ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
				ibDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
				ibDesc.mDesc.mSize = (pGeom->indexType == INDEX_TYPE_UINT32 ? sizeof(uint32_t) : sizeof(uint16_t)) * pGeom->indicesCount;
				ibDesc.pData = pGeom->indexType == INDEX_TYPE_UINT32 ? (void*)pGeom->pIndices32 : (void*)pGeom->pIndices16;
				ibDesc.ppBuffer = &pResult[i].pIndexBuffer;
				addResource(&ibDesc, NULL, LOAD_PRIORITY_NORMAL);

				pGeometry->geometry.triangles.indexData = pResult[i].pIndexBuffer->pVkBuffer;
				pGeometry->geometry.triangles.indexOffset = 0;
				pGeometry->geometry.triangles.indexCount = (uint32_t)ibDesc.mDesc.mSize /
					(pGeom->indexType == INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t));
				pGeometry->geometry.triangles.indexType = (INDEX_TYPE_UINT16 == pGeom->indexType) ?
															VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
			}
			else
			{
				pGeometry->geometry.triangles.indexData = VK_NULL_HANDLE;
				pGeometry->geometry.triangles.indexType = VK_INDEX_TYPE_NONE_NV;
				pGeometry->geometry.triangles.indexOffset = 0;
				pGeometry->geometry.triangles.indexCount = 0;
			}

			BufferLoadDesc vbDesc = {};
			vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
			vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			vbDesc.mDesc.mSize = sizeof(float3) * pGeom->vertexCount;
			vbDesc.pData = pGeom->pVertexArray;
			vbDesc.ppBuffer = &pResult[i].pVertexBuffer;
			addResource(&vbDesc, NULL, LOAD_PRIORITY_NORMAL);

			pGeometry->geometry.triangles.vertexData = pResult[i].pVertexBuffer->pVkBuffer;
			pGeometry->geometry.triangles.vertexOffset = 0;
			pGeometry->geometry.triangles.vertexCount = (uint32_t)vbDesc.mDesc.mSize / sizeof(float3);
			pGeometry->geometry.triangles.vertexStride = sizeof(float3);

			if (pGeometry->geometry.triangles.vertexStride == sizeof(float))
				pGeometry->geometry.triangles.vertexFormat = VK_FORMAT_R32_SFLOAT;
			else if (pGeometry->geometry.triangles.vertexStride == sizeof(float) * 2)
				pGeometry->geometry.triangles.vertexFormat = VK_FORMAT_R32G32_SFLOAT;
			else if (pGeometry->geometry.triangles.vertexStride == sizeof(float) * 3)
				pGeometry->geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
			else if (pGeometry->geometry.triangles.vertexStride == sizeof(float) * 4)
				pGeometry->geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

			//initialize AABBs
			pGeometry->geometry.aabbs = VkGeometryAABBNV{};
			pGeometry->geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
			pGeometry->geometry.aabbs.aabbData = VK_NULL_HANDLE;
		}
		
		VkAccelerationStructureInfoNV accelerationStructInfo = {};
		accelerationStructInfo.sType			= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
		accelerationStructInfo.type				= VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
		accelerationStructInfo.geometryCount	= pResult[i].mDescCount;
		accelerationStructInfo.instanceCount	= 0;
		accelerationStructInfo.flags			= pResult[i].mFlags;
		accelerationStructInfo.pGeometries		= pResult[i].pGeometryDescs;

		VkAccelerationStructureCreateInfoNV createInfo = {};
		createInfo.sType			= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
		createInfo.info				= accelerationStructInfo;
		createInfo.compactedSize	= 0;

		vkCreateAccelerationStructureNV(pRaytracing->pRenderer->pVkDevice, &createInfo, nullptr, &pResult[i].pAccelerationStructure);

		VkAccelerationStructureMemoryRequirementsInfoNV memReqInfo = {};
		memReqInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
		memReqInfo.accelerationStructure = pResult[i].pAccelerationStructure;
		memReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;

		VkMemoryRequirements2KHR memReq = {};
		vkGetAccelerationStructureMemoryRequirementsNV(pRaytracing->pRenderer->pVkDevice, &memReqInfo, &memReq);
		scratchBufferSize = scratchBufferSize > memReq.memoryRequirements.size ? (uint32_t)scratchBufferSize : (uint32_t)memReq.memoryRequirements.size;
		
		memReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_UPDATE_SCRATCH_NV;
		vkGetAccelerationStructureMemoryRequirementsNV(pRaytracing->pRenderer->pVkDevice, &memReqInfo, &memReq);
		scratchBufferSize = scratchBufferSize > memReq.memoryRequirements.size ? (uint32_t)scratchBufferSize : (uint32_t)memReq.memoryRequirements.size;

		memReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
		vkGetAccelerationStructureMemoryRequirementsNV(pRaytracing->pRenderer->pVkDevice, &memReqInfo, &memReq);
		scratchBufferSize = scratchBufferSize > memReq.memoryRequirements.size ? (uint32_t)scratchBufferSize : (uint32_t)memReq.memoryRequirements.size;

		BufferDesc bufferDesc = {};
		bufferDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_RAY_TRACING;
		bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
		bufferDesc.mStructStride = 0;
		bufferDesc.mFirstElement = 0;
		bufferDesc.mSize = memReq.memoryRequirements.size;
		bufferDesc.mStartState = RESOURCE_STATE_GENERIC_READ;
		addBuffer(pRaytracing->pRenderer, &bufferDesc, &pResult[i].pASBuffer);

		VkBindAccelerationStructureMemoryInfoNV bindInfo = {};
		bindInfo.sType					= VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
		bindInfo.pNext					= nullptr;
		bindInfo.accelerationStructure	= pResult[i].pAccelerationStructure;
		bindInfo.memory					= get_vk_device_memory(pRaytracing->pRenderer, pResult[i].pASBuffer);
		bindInfo.memoryOffset			= get_vk_device_memory_offset(pRaytracing->pRenderer, pResult[i].pASBuffer);
		bindInfo.deviceIndexCount		= 0;
		bindInfo.pDeviceIndices			= nullptr;

		vkBindAccelerationStructureMemoryNV(pRaytracing->pRenderer->pVkDevice, 1, &bindInfo);

	}

	*pScratchBufferSize = scratchBufferSize;
	return pResult;
}

Buffer* createTopAS(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, const AccelerationStructureBottom* pASBottom, uint32_t* pScratchBufferSize, Buffer** ppInstanceDescBuffer, VkAccelerationStructureNV *pAccelerationStructure)
{
	ASSERT(pRaytracing);
	ASSERT(pDesc);
	ASSERT(pScratchBufferSize);
	ASSERT(pASBottom);
	ASSERT(ppInstanceDescBuffer);
	/************************************************************************/
	// Get the size requirement for the Acceleration Structures
	/************************************************************************/
	uint32_t scratchBufferSize = *pScratchBufferSize;

	VkAccelerationStructureInfoNV accelerationStructureInfo = {};
	accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
	accelerationStructureInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
	accelerationStructureInfo.geometryCount = 0;
	accelerationStructureInfo.instanceCount = 1;// pDesc->mInstancesDescCount;
	accelerationStructureInfo.flags =  util_to_vk_acceleration_structure_build_flags(pDesc->mFlags);
	accelerationStructureInfo.pGeometries = nullptr;
	
	VkAccelerationStructureCreateInfoNV createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
	createInfo.info = accelerationStructureInfo;
	createInfo.compactedSize = 0;

	vkCreateAccelerationStructureNV(pRaytracing->pRenderer->pVkDevice, &createInfo, nullptr, pAccelerationStructure);

	VkAccelerationStructureMemoryRequirementsInfoNV memReqInfo = {};
	memReqInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
	memReqInfo.accelerationStructure = *pAccelerationStructure;
	memReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;

	VkMemoryRequirements2KHR memReq = {};
	vkGetAccelerationStructureMemoryRequirementsNV(pRaytracing->pRenderer->pVkDevice, &memReqInfo, &memReq);
	VkDeviceSize accelerationStructureSize = memReq.memoryRequirements.size;
	scratchBufferSize = scratchBufferSize > memReq.memoryRequirements.size ? (uint32_t)scratchBufferSize : (uint32_t)memReq.memoryRequirements.size;

	memReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_UPDATE_SCRATCH_NV;
	vkGetAccelerationStructureMemoryRequirementsNV(pRaytracing->pRenderer->pVkDevice, &memReqInfo, &memReq);
	scratchBufferSize = scratchBufferSize > memReq.memoryRequirements.size ? (uint32_t)scratchBufferSize : (uint32_t)memReq.memoryRequirements.size;

	memReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
	vkGetAccelerationStructureMemoryRequirementsNV(pRaytracing->pRenderer->pVkDevice, &memReqInfo, &memReq);
	scratchBufferSize = scratchBufferSize > memReq.memoryRequirements.size ? (uint32_t)scratchBufferSize : (uint32_t)memReq.memoryRequirements.size;
	/************************************************************************/
	/*  Construct buffer with instances descriptions                        */
	/************************************************************************/
	eastl::vector<VkGeometryInstanceNV> instanceDescs(pDesc->mInstancesDescCount);
	for (uint32_t i = 0; i < pDesc->mInstancesDescCount; ++i)
	{
		AccelerationStructureInstanceDesc* pInst = &pDesc->pInstanceDescs[i];

		uint64_t accelerationStructureHandle = 0;
		VkResult error = vkGetAccelerationStructureHandleNV(pRaytracing->pRenderer->pVkDevice,
			pASBottom[pInst->mAccelerationStructureIndex].pAccelerationStructure, sizeof(uint64_t), &accelerationStructureHandle);
		ASSERT(error == VK_SUCCESS);

		const Buffer* pASBuffer = pASBottom[pInst->mAccelerationStructureIndex].pASBuffer;
		instanceDescs[i].accelerationStructureHandle = accelerationStructureHandle;
		instanceDescs[i].flags = util_to_vk_instance_flags(pInst->mFlags);

		instanceDescs[i].instanceOffset = pInst->mInstanceContributionToHitGroupIndex;
		instanceDescs[i].instanceCustomIndex = pInst->mInstanceID;
		instanceDescs[i].mask = pInst->mInstanceMask;

		memcpy(instanceDescs[i].transform, pInst->mTransform, sizeof(float[12]));
	}

	BufferDesc instanceDesc = {};
	instanceDesc.mDescriptors = DESCRIPTOR_TYPE_RAY_TRACING;
	instanceDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	instanceDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	instanceDesc.mSize = instanceDescs.size() * sizeof(instanceDescs[0]);
	addBuffer(pRaytracing->pRenderer, &instanceDesc, ppInstanceDescBuffer);
	memcpy((*ppInstanceDescBuffer)->pCpuMappedAddress, instanceDescs.data(), instanceDesc.mSize);

	/************************************************************************/
	// Allocate Acceleration Structure Buffer
	/************************************************************************/
	BufferDesc bufferDesc = {};
	bufferDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_RAY_TRACING;
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
	bufferDesc.mSize = accelerationStructureSize;
	bufferDesc.mStartState = RESOURCE_STATE_GENERIC_READ;
	Buffer* pTopASBuffer = {};
	addBuffer(pRaytracing->pRenderer, &bufferDesc, &pTopASBuffer);

	VkBindAccelerationStructureMemoryInfoNV bindInfo = {};
	bindInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
	bindInfo.accelerationStructure = *pAccelerationStructure;
	bindInfo.memory = get_vk_device_memory(pRaytracing->pRenderer, pTopASBuffer);
	bindInfo.memoryOffset = get_vk_device_memory_offset(pRaytracing->pRenderer, pTopASBuffer);
	vkBindAccelerationStructureMemoryNV(pRaytracing->pRenderer->pVkDevice, 1, &bindInfo);

	*pScratchBufferSize = scratchBufferSize;
	return pTopASBuffer;
}

void addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, AccelerationStructure** ppAccelerationStructure)
{
	ASSERT(pRaytracing);
	ASSERT(pDesc);
	ASSERT(ppAccelerationStructure);

	AccelerationStructure* pAccelerationStructure = (AccelerationStructure*)conf_calloc(1, sizeof(*pAccelerationStructure));
	ASSERT(pAccelerationStructure);

	uint32_t scratchBottomBufferSize = 0;
	pAccelerationStructure->mBottomASCount = pDesc->mBottomASDescsCount;
	pAccelerationStructure->ppBottomAS = createBottomAS(pRaytracing, pDesc, &scratchBottomBufferSize);

	uint32_t scratchTopBufferSize = 0;
	pAccelerationStructure->mInstanceDescCount = pDesc->mInstancesDescCount;
	pAccelerationStructure->pASBuffer = createTopAS(pRaytracing, pDesc, 
													pAccelerationStructure->ppBottomAS, 
													&scratchTopBufferSize, 
													&pAccelerationStructure->pInstanceDescBuffer, 
													&pAccelerationStructure->mAccelerationStructure);
	pAccelerationStructure->mScratchBufferSize = max(scratchBottomBufferSize, scratchTopBufferSize);
	pAccelerationStructure->mFlags = util_to_vk_acceleration_structure_build_flags(pDesc->mFlags);

	//Create scratch buffer
	BufferLoadDesc scratchBufferDesc = {};
	scratchBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
	scratchBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	scratchBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
	scratchBufferDesc.mDesc.mSize = pAccelerationStructure->mScratchBufferSize;
	scratchBufferDesc.ppBuffer = &pAccelerationStructure->pScratchBuffer;
	addResource(&scratchBufferDesc, NULL, LOAD_PRIORITY_NORMAL);

	*ppAccelerationStructure = pAccelerationStructure;
}

void util_build_acceleration_structure(VkCommandBuffer pCmd, VkBuffer pScratchBuffer, 
	VkAccelerationStructureNV pAccelerationStructure,
	VkAccelerationStructureTypeNV type,
	VkBuildAccelerationStructureFlagsNV flags,
	const VkGeometryNV* pGeometryDescs,
	uint32_t geometriesCount,
	const VkBuffer pInstanceDescBuffer,
	uint32_t descCount)
{
	ASSERT(pCmd);

	VkAccelerationStructureInfoNV info = {};
	info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
	info.type = type;
	info.flags = flags;
	info.pGeometries = pGeometryDescs;
	info.geometryCount = geometriesCount;
	info.instanceCount = descCount;

	VkBuffer instanceData = VK_NULL_HANDLE;
	if (descCount > 0)
		instanceData = pInstanceDescBuffer;

	VkBuffer scratchBuffer = pScratchBuffer;

	vkCmdBuildAccelerationStructureNV(pCmd, &info, instanceData, 0, VK_FALSE, pAccelerationStructure, VK_NULL_HANDLE, scratchBuffer, 0);

	VkMemoryBarrier memoryBarrier;
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.pNext = nullptr;
	memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
	memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
	vkCmdPipelineBarrier(pCmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);
}

void cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc)
{
	ASSERT(pDesc);
	ASSERT(pDesc->pAccelerationStructure);

	AccelerationStructure* pAccelerationStructure = pDesc->pAccelerationStructure;

	for (unsigned i = 0; i < pDesc->mBottomASIndicesCount; ++i)
	{
		uint32_t index = pDesc->pBottomASIndices[i];

		util_build_acceleration_structure(pCmd->pVkCmdBuf,
			pAccelerationStructure->pScratchBuffer->pVkBuffer,
			pAccelerationStructure->ppBottomAS[index].pAccelerationStructure,
			VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV,
			pAccelerationStructure->ppBottomAS[index].mFlags,
			pAccelerationStructure->ppBottomAS[index].pGeometryDescs,
			pAccelerationStructure->ppBottomAS[index].mDescCount,
			NULL, 0);
	}

	util_build_acceleration_structure(pCmd->pVkCmdBuf,
		pAccelerationStructure->pScratchBuffer->pVkBuffer,
		pAccelerationStructure->mAccelerationStructure,
		VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV,
		pAccelerationStructure->mFlags,
		NULL, 0,
		pAccelerationStructure->pInstanceDescBuffer->pVkBuffer,
		pAccelerationStructure->mInstanceDescCount);
}

void CalculateMaxShaderRecordSize(const char* const* pRecords, uint32_t shaderCount, uint64_t& maxShaderTableSize)
{
}

void FillShaderIdentifiers(	const char* const* pRecords, uint32_t shaderCount,
							uint64_t maxShaderTableSize, uint64_t& dstIndex,
							RaytracingShaderTable* pTable, Raytracing* pRaytracing, ShaderLocalData* mShaderLocalData, 
							const uint8_t* shaderHandleStorage)
{
	Pipeline* pipeline = pTable->pPipeline;

	for (uint32_t idx = 0; idx < shaderCount; ++idx)
	{
		uint32_t index = -1;
		eastl::string nameStr(pRecords[idx]);
		const char** it = eastl::find(pipeline->ppShaderStageNames, pipeline->ppShaderStageNames + pipeline->mShaderStageCount, nameStr.c_str(),
			[](const char* a, const char* b) { return strcmp(a, b) == 0; });
		if (it != pipeline->ppShaderStageNames + pipeline->mShaderStageCount)
		{
			index = (uint32_t)(it - pipeline->ppShaderStageNames);
		}
		else
		{
			// This is allowed if we are provided with a hit group that has no shaders associated.
			// In all other cases this is an error.
			LOGF(LogLevel::eINFO, "Could not find shader name %s identifier. This is only valid if %s is a hit group with no shaders.", nameStr.c_str(), nameStr.c_str());
			dstIndex += 1;
			continue;
		}

		uint64_t currentPosition = maxShaderTableSize * dstIndex++;
		uint8_t* dst = (uint8_t*)pTable->pBuffer->pCpuMappedAddress + currentPosition;
		size_t handleSize = pRaytracing->mRayTracingProperties.shaderGroupHandleSize;
		const uint8_t* src = &shaderHandleStorage[index * handleSize];
		memcpy(dst, src, handleSize);
	}
}

void addRaytracingShaderTable(Raytracing* pRaytracing, const RaytracingShaderTableDesc* pDesc, RaytracingShaderTable** ppTable) 
{
	ASSERT(pRaytracing);
	ASSERT(pDesc);
	ASSERT(pDesc->pPipeline);
	ASSERT(ppTable);

	RaytracingShaderTable* pTable = (RaytracingShaderTable*)conf_calloc(1, sizeof(*pTable));
	conf_placement_new<RaytracingShaderTable>((void*)pTable);
	ASSERT(pTable);

	pTable->pPipeline = pDesc->pPipeline;

	const uint32_t recordCount = 1 + pDesc->mMissShaderCount + pDesc->mHitGroupCount;
	uint64_t maxShaderTableSize = 0;
	/************************************************************************/
	// Calculate max size for each element in the shader table
	/************************************************************************/
	CalculateMaxShaderRecordSize(&pDesc->pRayGenShader, 1, maxShaderTableSize);
	CalculateMaxShaderRecordSize(pDesc->pMissShaders, pDesc->mMissShaderCount, maxShaderTableSize);
	CalculateMaxShaderRecordSize(pDesc->pHitGroups, pDesc->mHitGroupCount, maxShaderTableSize);
	/************************************************************************/
	// Align max size
	/************************************************************************/
	const uint32_t groupHandleSize = pRaytracing->mRayTracingProperties.shaderGroupHandleSize;
	maxShaderTableSize		= round_up_64(groupHandleSize + maxShaderTableSize, pRaytracing->mRayTracingProperties.shaderGroupBaseAlignment);
	pTable->mMaxEntrySize	= maxShaderTableSize;
	/************************************************************************/
	// Create shader table buffer
	/************************************************************************/
	BufferDesc bufferDesc = {};
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	bufferDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	bufferDesc.mSize = maxShaderTableSize * recordCount;
	bufferDesc.pDebugName = L"RTShadersTable";
	addBuffer(pRaytracing->pRenderer, &bufferDesc, &pTable->pBuffer);
	/************************************************************************/
	// Copy shader identifiers into the buffer
	/************************************************************************/
	uint32_t groupCount = (uint32_t)pDesc->pPipeline->mShaderStageCount;
	uint8_t* shaderHandleStorage = (uint8_t*)conf_calloc(groupCount, sizeof(uint8_t) * groupHandleSize);
	VkResult code = vkGetRayTracingShaderGroupHandlesNV(pRaytracing->pRenderer->pVkDevice, 
														pDesc->pPipeline->pVkPipeline, 0, groupCount, 
														groupHandleSize * groupCount, shaderHandleStorage);

	pTable->mHitMissLocalData.resize(pDesc->mMissShaderCount + pDesc->mHitGroupCount + 1);
	uint64_t index = 0;
	FillShaderIdentifiers(	&pDesc->pRayGenShader, 1, maxShaderTableSize, index, pTable, pRaytracing, 
							pTable->mHitMissLocalData.data(), shaderHandleStorage);

	pTable->mMissRecordSize = maxShaderTableSize * pDesc->mMissShaderCount;
	FillShaderIdentifiers(	pDesc->pMissShaders, pDesc->mMissShaderCount, 
							maxShaderTableSize, index, pTable, pRaytracing, 
							&pTable->mHitMissLocalData[1], shaderHandleStorage);

	pTable->mHitGroupRecordSize = maxShaderTableSize * pDesc->mHitGroupCount;
	FillShaderIdentifiers(	pDesc->pHitGroups, pDesc->mHitGroupCount,
							maxShaderTableSize, index, pTable, pRaytracing, 
							&pTable->mHitMissLocalData[1 + pDesc->mMissShaderCount] ,shaderHandleStorage);

	conf_free(shaderHandleStorage);

	*ppTable = pTable;
}

void cmdDispatchRays(Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc)
{
	RaytracingShaderTable* table = pDesc->pShaderTable;
	vkCmdTraceRaysNV(
		pCmd->pVkCmdBuf,
		table->pBuffer->pVkBuffer, 0,
		table->pBuffer->pVkBuffer, table->mMaxEntrySize, table->mMaxEntrySize,
		table->pBuffer->pVkBuffer, table->mMaxEntrySize + table->mMissRecordSize, table->mMaxEntrySize,
		VK_NULL_HANDLE, 0, 0,
		pDesc->mWidth, pDesc->mHeight, 1
	);
}

void removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure) 
{
	ASSERT(pRaytracing);
	ASSERT(pAccelerationStructure);

	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pASBuffer);
	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pInstanceDescBuffer);
	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pScratchBuffer);
	vkDestroyAccelerationStructureNV(pRaytracing->pRenderer->pVkDevice, pAccelerationStructure->mAccelerationStructure, nullptr);

	for (unsigned i = 0; i < pAccelerationStructure->mBottomASCount; ++i)
	{
		removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->ppBottomAS[i].pASBuffer);
		removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->ppBottomAS[i].pVertexBuffer);
		if (pAccelerationStructure->ppBottomAS[i].pIndexBuffer->pVkBuffer != VK_NULL_HANDLE)
			removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->ppBottomAS[i].pIndexBuffer);
		conf_free(pAccelerationStructure->ppBottomAS[i].pGeometryDescs);
		vkDestroyAccelerationStructureNV(pRaytracing->pRenderer->pVkDevice, pAccelerationStructure->ppBottomAS[i].pAccelerationStructure, nullptr);
	}
	conf_free(pAccelerationStructure->ppBottomAS);
	conf_free(pAccelerationStructure);
}

void removeRaytracingShaderTable(Raytracing* pRaytracing, RaytracingShaderTable* pTable) 
{
	ASSERT(pRaytracing);
	ASSERT(pTable);

	removeBuffer(pRaytracing->pRenderer, pTable->pBuffer);
	pTable->~RaytracingShaderTable();
	conf_free(pTable);
}

VkBuildAccelerationStructureFlagsNV util_to_vk_acceleration_structure_build_flags(AccelerationStructureBuildFlags flags)
{
	VkBuildAccelerationStructureFlagsNV ret = 0;
	if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION)
		ret |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_NV;
	if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE)
		ret |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV;
	if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY)
		ret |= VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_NV;
	if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE)
		ret |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV;
	if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD)
		ret |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_NV;
	if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE)
		ret |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV;

	return ret;
}

VkGeometryFlagsNV util_to_vk_geometry_flags(AccelerationStructureGeometryFlags flags)
{
	VkGeometryFlagsNV ret = 0;
	if (flags & ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE)
		ret |= VK_GEOMETRY_OPAQUE_BIT_NV;
	if (flags & ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION)
		ret |= VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_NV;

	return ret;
}

VkGeometryInstanceFlagsNV util_to_vk_instance_flags(AccelerationStructureInstanceFlags flags)
{
	VkGeometryInstanceFlagsNV ret = 0;
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_OPAQUE)
		ret |= VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_NV;
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_OPAQUE)
		ret |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_NV;
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE)
		ret |= VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE)
		ret |= VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_NV;

	return ret;
}

void addRaytracingPipelineImpl(const RaytracingPipelineDesc* pDesc, Pipeline** ppPipeline)
{
	Pipeline* pResult = (Pipeline*)conf_calloc(1, sizeof(Pipeline));
	ASSERT(pResult);

	pResult->mType = PIPELINE_TYPE_RAYTRACING;
	eastl::vector<VkPipelineShaderStageCreateInfo> stages;
	eastl::vector<VkRayTracingShaderGroupCreateInfoNV> groups;
	/************************************************************************/
	// Generate Stage Names
	/************************************************************************/
	stages.reserve(1 + pDesc->mMissShaderCount + pDesc->mHitGroupCount);
	groups.reserve(1 + pDesc->mMissShaderCount + pDesc->mHitGroupCount);
	pResult->mShaderStageCount = 0;
	pResult->ppShaderStageNames = (const char**)conf_calloc(1 + pDesc->mMissShaderCount + pDesc->mHitGroupCount * 3, sizeof(char*));
	//////////////////////////////////////////////////////////////////////////
	//1. Ray-gen shader
	{
		VkPipelineShaderStageCreateInfo stageCreateInfo = {};
		stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageCreateInfo.pNext = nullptr;
		stageCreateInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_NV;
		stageCreateInfo.module = pDesc->pRayGenShader->pShaderModules[0];
		//nVidia comment: This member has to be 'main', regardless of the actual entry point of the shader
		stageCreateInfo.pName = "main";
		stageCreateInfo.flags = 0;
		stageCreateInfo.pSpecializationInfo = nullptr;
		stages.push_back(stageCreateInfo);
		//stagesNames.push_back(pDesc->pRayGenShader->mName);
		pResult->ppShaderStageNames[pResult->mShaderStageCount] = pDesc->pRayGenShader->pEntryNames[0];
		pResult->mShaderStageCount += 1;

		VkRayTracingShaderGroupCreateInfoNV groupInfo;
		groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
		groupInfo.pNext = nullptr;
		groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
		groupInfo.generalShader = (uint32_t)stages.size() - 1;
		groupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		groupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		groupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		groups.push_back(groupInfo);
	}

	//////////////////////////////////////////////////////////////////////////
	//2. Miss shaders
	{
		VkPipelineShaderStageCreateInfo stageCreateInfo = {};
		stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageCreateInfo.pNext = nullptr;
		stageCreateInfo.stage = VK_SHADER_STAGE_MISS_BIT_NV;
		stageCreateInfo.module = VK_NULL_HANDLE;
		//nVidia comment: This member has to be 'main', regardless of the actual entry point of the shader
		stageCreateInfo.pName = "main";
		stageCreateInfo.flags = 0;
		stageCreateInfo.pSpecializationInfo = nullptr;

		VkRayTracingShaderGroupCreateInfoNV groupInfo;
		groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
		groupInfo.pNext = nullptr;
		groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
		groupInfo.generalShader = VK_SHADER_UNUSED_NV;
		groupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		groupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		groupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		for (uint32_t i = 0; i < pDesc->mMissShaderCount; ++i)
		{
			stageCreateInfo.module = pDesc->ppMissShaders[i]->pShaderModules[0];
			stages.push_back(stageCreateInfo);
			//stagesNames.push_back(pDesc->ppMissShaders[i]->mName);
			pResult->ppShaderStageNames[pResult->mShaderStageCount] = pDesc->ppMissShaders[i]->pEntryNames[0];
			pResult->mShaderStageCount += 1;

			groupInfo.generalShader = (uint32_t)stages.size() - 1;
			groups.push_back(groupInfo);
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
		//nVidia comment: This member has to be 'main', regardless of the actual entry point of the shader
		stageCreateInfo.pName = "main";
		stageCreateInfo.flags = 0;
		stageCreateInfo.pSpecializationInfo = nullptr;

		VkRayTracingShaderGroupCreateInfoNV groupInfo;
		groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
		groupInfo.pNext = nullptr;
		groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_NV;

		for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
		{
			groupInfo.generalShader = VK_SHADER_UNUSED_NV;
			groupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
			groupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
			groupInfo.intersectionShader = VK_SHADER_UNUSED_NV;

			uint32_t hitGroupIndex = (uint32_t)groups.size() - 1;
			if (pDesc->pHitGroups[i].pIntersectionShader)
			{
				stageCreateInfo.stage = VK_SHADER_STAGE_INTERSECTION_BIT_NV;
				stageCreateInfo.module = pDesc->pHitGroups[i].pIntersectionShader->pShaderModules[0];
				stages.push_back(stageCreateInfo);
				pResult->ppShaderStageNames[pResult->mShaderStageCount] = pDesc->pHitGroups[i].pHitGroupName;
				pResult->mShaderStageCount += 1;

				groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV;
				groupInfo.intersectionShader = (uint32_t)stages.size() - 1;
			}
			if (pDesc->pHitGroups[i].pAnyHitShader)
			{
				stageCreateInfo.stage = VK_SHADER_STAGE_ANY_HIT_BIT_NV;
				stageCreateInfo.module = pDesc->pHitGroups[i].pAnyHitShader->pShaderModules[0];
				stages.push_back(stageCreateInfo);
				pResult->ppShaderStageNames[pResult->mShaderStageCount] = pDesc->pHitGroups[i].pHitGroupName;
				pResult->mShaderStageCount += 1;

				groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
				groupInfo.anyHitShader = (uint32_t)stages.size() - 1;
			}
			if (pDesc->pHitGroups[i].pClosestHitShader)
			{
				stageCreateInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;
				stageCreateInfo.module = pDesc->pHitGroups[i].pClosestHitShader->pShaderModules[0];
				stages.push_back(stageCreateInfo);
				pResult->ppShaderStageNames[pResult->mShaderStageCount] = pDesc->pHitGroups[i].pHitGroupName;
				pResult->mShaderStageCount += 1;

				groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
				groupInfo.closestHitShader = (uint32_t)stages.size() - 1;
			}
			groups.push_back(groupInfo);
		}
	}
	/************************************************************************/
	// Create Pipeline
	/************************************************************************/
	VkRayTracingPipelineCreateInfoNV createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
	createInfo.flags = 0;			//VkPipelineCreateFlagBits
	createInfo.stageCount = (uint32_t)stages.size();
	createInfo.pStages = stages.data();
	createInfo.groupCount = (uint32_t)groups.size(); //ray-gen groups
	createInfo.pGroups = groups.data();
	createInfo.maxRecursionDepth = pDesc->mMaxTraceRecursionDepth;
	createInfo.layout = pDesc->pGlobalRootSignature->pPipelineLayout;
	createInfo.basePipelineHandle = VK_NULL_HANDLE;
	createInfo.basePipelineIndex = 0;

	VkResult vk_result = vkCreateRayTracingPipelinesNV(pDesc->pRaytracing->pRenderer->pVkDevice, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pResult->pVkPipeline);
	ASSERT(VK_SUCCESS == vk_result);

	*ppPipeline = pResult;
}

void vk_FillRaytracingDescriptorData(const AccelerationStructure* pAccelerationStructure, void* pHandle)
{
	VkWriteDescriptorSetAccelerationStructureNV* pWriteNV = (VkWriteDescriptorSetAccelerationStructureNV*)pHandle;
	pWriteNV->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
	pWriteNV->pNext = NULL;
	pWriteNV->accelerationStructureCount = 1;
	pWriteNV->pAccelerationStructures = &pAccelerationStructure->mAccelerationStructure;
}
#else
bool isRaytracingSupported(Renderer* pRenderer)
{
	return false;
}

bool initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing)
{
	return false;
}

void removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing)
{
}

void addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, AccelerationStructure** ppAccelerationStructure)
{
}

void cmdBuildTopAS(Cmd* pCmd, Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
}

void cmdBuildBottomAS(Cmd* pCmd, Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure, unsigned bottomASIndex)
{
}

void cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc)
{
}

void addRaytracingShaderTable(Raytracing* pRaytracing, const RaytracingShaderTableDesc* pDesc, RaytracingShaderTable** ppTable)
{
}

void cmdDispatchRays(Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc)
{
}

void removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
}

void removeRaytracingShaderTable(Raytracing* pRaytracing, RaytracingShaderTable* pTable)
{
}
#endif
#endif

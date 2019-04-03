#include "../../OS/Interfaces/ILogManager.h"

// Renderer
#include "../IRenderer.h"
#include "../IRay.h"
#include "../ResourceLoader.h"

#include "../../OS/Interfaces/IMemoryManager.h"

#ifdef VK_NV_RAY_TRACING_SPEC_VERSION
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
	tinystl::vector<ShaderLocalData> mHitMissLocalData;
};

//This structure is not defined in Vulkan headers but this layout is used on GPU side for
//top level AS
struct VkGeometryInstanceNV {
	float          transform[12];
	uint32_t       instanceCustomIndex : 24;
	uint32_t       mask : 8;
	uint32_t       instanceOffset : 24;
	uint32_t       flags : 8;
	uint64_t       accelerationStructureHandle;
};

extern void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
extern void removeBuffer(Renderer* pRenderer, Buffer* p_buffer);
extern VkDeviceMemory getDeviceMemory(Buffer* buffer);
extern VkDeviceSize getDeviceMemoryOffset(Buffer* buffer);
extern VkDescriptorType util_to_vk_descriptor_type(DescriptorType type);
extern VkShaderStageFlags util_to_vk_shader_stage_flags(ShaderStage stages);
extern const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName, uint32_t* pIndex);
extern const RendererShaderDefinesDesc get_renderer_shaderdefines(Renderer* pRenderer);
extern bool load_shader_stage_byte_code( Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const char* fileName, FSRoot root, uint32_t macroCount,
	ShaderMacro* pMacros, uint32_t rendererMacroCount, ShaderMacro* pRendererMacros, tinystl::vector<char>& byteCode);
extern void cmdBindDescriptors(Cmd* pCmd, DescriptorBinder* pDescriptorBinder, RootSignature* pRootSignature, uint32_t numDescriptors, DescriptorData* pDescParams);
VkBuildAccelerationStructureFlagsNV util_to_vk_acceleration_structure_build_flags(AccelerationStructureBuildFlags flags);
VkGeometryFlagsNV util_to_vk_geometry_flags(AccelerationStructureGeometryFlags flags);
VkGeometryInstanceFlagsNV util_to_vk_instance_flags(AccelerationStructureInstanceFlags flags);

bool isRaytracingSupported(Renderer* pRenderer) {
	for (int i = 0; i < MAX_DEVICE_EXTENSIONS; ++i)
	{
		if (pRenderer->gVkDeviceExtensions[i] == nullptr) continue; //for some reason 0th element is nullptr
		if (strcmp(pRenderer->gVkDeviceExtensions[i], VK_NV_RAY_TRACING_EXTENSION_NAME) == 0)
			return true;
	}
	return false;
}

bool initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing) {
	ASSERT(pRenderer);
	ASSERT(ppRaytracing);

	if (!isRaytracingSupported(pRenderer)) return false;

	Raytracing* pRaytracing = (Raytracing*)conf_calloc(1, sizeof(*pRaytracing));
	ASSERT(pRaytracing);

	pRaytracing->pRenderer = pRenderer;
	pRaytracing->pRayTracingProperties = pRenderer->mVkRaytracingProperties;

	*ppRaytracing = pRaytracing;
	return true;
}

void removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing) {
	//Do nothing here because in case of Vulkan struct Raytracing contains
	//only shorthands
	conf_free(pRaytracing);
}

Buffer* createGeomVertexBuffer(const AccelerationStructureGeometryDesc* desc)
{
	ASSERT(desc->pVertexArray);
	ASSERT(desc->vertexCount > 0);

	Buffer* result = nullptr;
	BufferLoadDesc vbDesc = {};
	vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	vbDesc.mDesc.mSize = sizeof(float3) * desc->vertexCount;
	vbDesc.mDesc.mVertexStride = sizeof(float3);
	vbDesc.pData = desc->pVertexArray;
	vbDesc.ppBuffer = &result;
	addResource(&vbDesc);

	return result;
}

Buffer* createGeomIndexBuffer(const AccelerationStructureGeometryDesc* desc)
{
	ASSERT(desc->pIndices16 != nullptr || desc->pIndices32 != nullptr);
	ASSERT(desc->indicesCount > 0);

	Buffer* result = nullptr;
	BufferLoadDesc indexBufferDesc = {};
	indexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
	indexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	indexBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	indexBufferDesc.mDesc.mSize = sizeof(uint) * desc->indicesCount;
	indexBufferDesc.mDesc.mIndexType = desc->indexType;
	indexBufferDesc.pData = desc->indexType == INDEX_TYPE_UINT32 ? (void*)desc->pIndices32 : (void*)desc->pIndices16;
	indexBufferDesc.ppBuffer = &result;
	addResource(&indexBufferDesc);

	return result;
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

			pResult[i].pIndexBuffer = nullptr;
			if (pGeom->indicesCount > 0)
			{
				
				pResult[i].pIndexBuffer = createGeomIndexBuffer(pGeom);
				pGeometry->geometry.triangles.indexData = pResult[i].pIndexBuffer->pVkBuffer;
				pGeometry->geometry.triangles.indexOffset = pResult[i].pIndexBuffer->mVkBufferInfo.offset;
				pGeometry->geometry.triangles.indexCount = (uint32_t)pResult[i].pIndexBuffer->mDesc.mSize /
					(pResult[i].pIndexBuffer->mDesc.mIndexType == INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t));
				pGeometry->geometry.triangles.indexType = (INDEX_TYPE_UINT16 == pResult[i].pIndexBuffer->mDesc.mIndexType) ? 
															VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
			}
			else
			{
				pGeometry->geometry.triangles.indexData = VK_NULL_HANDLE;
				pGeometry->geometry.triangles.indexType = VK_INDEX_TYPE_NONE_NV;
				pGeometry->geometry.triangles.indexOffset = 0;
				pGeometry->geometry.triangles.indexCount = 0;
			}


			pResult[i].pVertexBuffer = createGeomVertexBuffer(pGeom);
			pGeometry->geometry.triangles.vertexData = pResult[i].pVertexBuffer->pVkBuffer;
			pGeometry->geometry.triangles.vertexOffset = pResult[i].pVertexBuffer->mVkBufferInfo.offset;
			pGeometry->geometry.triangles.vertexCount = (uint32_t)pResult[i].pVertexBuffer->mDesc.mSize / (uint32_t)pResult[i].pVertexBuffer->mDesc.mVertexStride;
			pGeometry->geometry.triangles.vertexStride = (VkDeviceSize)pResult[i].pVertexBuffer->mDesc.mVertexStride;

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
		bindInfo.memory					= getDeviceMemory(pResult[i].pASBuffer);
		bindInfo.memoryOffset			= getDeviceMemoryOffset(pResult[i].pASBuffer);
		bindInfo.deviceIndexCount		= 0;
		bindInfo.pDeviceIndices			= nullptr;

		vkBindAccelerationStructureMemoryNV(pRaytracing->pRenderer->pVkDevice, 1, &bindInfo);

	}

	*pScratchBufferSize = scratchBufferSize;
	return pResult;
}

Buffer* createTopAS(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, const AccelerationStructureBottom* pASBottom, uint32_t* pScratchBufferSize, Buffer** ppInstanceDescBuffer, VkAccelerationStructureNV *pAccelerationStructure){
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
	accelerationStructureInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV;//  util_to_vk_acceleration_structure_build_flags(pDesc->mFlags);
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
	tinystl::vector<VkGeometryInstanceNV> instanceDescs(pDesc->mInstancesDescCount);
	for (uint32_t i = 0; i < pDesc->mInstancesDescCount; ++i)
	{
		AccelerationStructureInstanceDesc* pInst = &pDesc->pInstanceDescs[i];

		uint64_t accelerationStructureHandle = 0;
		VkResult error = vkGetAccelerationStructureHandleNV(pRaytracing->pRenderer->pVkDevice,
			pASBottom[pInst->mAccelerationStructureIndex].pAccelerationStructure, sizeof(uint64_t), &accelerationStructureHandle);
		ASSERT(error == VK_SUCCESS);

		Buffer* pASBuffer = pASBottom[pInst->mAccelerationStructureIndex].pASBuffer;
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
	Buffer* pTopASBuffer = nullptr;
	addBuffer(pRaytracing->pRenderer, &bufferDesc, &pTopASBuffer);

	VkBindAccelerationStructureMemoryInfoNV bindInfo = {};
	bindInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
	bindInfo.accelerationStructure = *pAccelerationStructure;
	bindInfo.memory = getDeviceMemory(pTopASBuffer);
	bindInfo.memoryOffset = getDeviceMemoryOffset(pTopASBuffer);
	vkBindAccelerationStructureMemoryNV(pRaytracing->pRenderer->pVkDevice, 1, &bindInfo);

	*pScratchBufferSize = scratchBufferSize;
	return pTopASBuffer;
}

void addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, AccelerationStructure** ppAccelerationStructure) {
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
	addResource(&scratchBufferDesc);

	*ppAccelerationStructure = pAccelerationStructure;
}

void cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, Buffer* pScratchBuffer, 
	VkAccelerationStructureNV accelerationStructure,
	VkAccelerationStructureTypeNV ASType,
	VkBuildAccelerationStructureFlagsNV ASFlags,
	const VkGeometryNV * pGeometryDescs,
	uint32_t geometriesCount,
	const Buffer* pInstanceDescBuffer,
	uint32_t descCount)
{
	ASSERT(pCmd);
	ASSERT(pRaytracing);

	VkAccelerationStructureInfoNV info = {};
	info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
	info.type = ASType;
	info.flags = ASFlags;
	info.pGeometries = pGeometryDescs;
	info.geometryCount = geometriesCount;
	info.instanceCount = descCount;

	VkBuffer instanceData = VK_NULL_HANDLE;
	if (descCount > 0)
		instanceData = pInstanceDescBuffer->pVkBuffer;

	VkBuffer scratchBuffer = pScratchBuffer->pVkBuffer;
	VkAccelerationStructureNV astruct = accelerationStructure;

	vkCmdBuildAccelerationStructureNV(pCmd->pVkCmdBuf, &info, instanceData, 0, VK_FALSE, astruct, VK_NULL_HANDLE, scratchBuffer, 0);

	VkMemoryBarrier memoryBarrier;
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.pNext = nullptr;
	memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
	memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
	vkCmdPipelineBarrier(pCmd->pVkCmdBuf, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);
}

void cmdBuildTopAS(Cmd* pCmd, Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
	cmdBuildAccelerationStructure(pCmd, pRaytracing,
		pAccelerationStructure->pScratchBuffer,
		pAccelerationStructure->mAccelerationStructure,
		VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV,
		pAccelerationStructure->mFlags,
		nullptr, 0,
		pAccelerationStructure->pInstanceDescBuffer,
		pAccelerationStructure->mInstanceDescCount);
}

void cmdBuildBottomAS(Cmd* pCmd, Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure, unsigned bottomASIndex)
{
	cmdBuildAccelerationStructure(pCmd, pRaytracing,
		pAccelerationStructure->pScratchBuffer,
		pAccelerationStructure->ppBottomAS[bottomASIndex].pAccelerationStructure,
		VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV,
		pAccelerationStructure->ppBottomAS[bottomASIndex].mFlags,
		pAccelerationStructure->ppBottomAS[bottomASIndex].pGeometryDescs,
		pAccelerationStructure->ppBottomAS[bottomASIndex].mDescCount,
		nullptr, 0);
}

void cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc)
{
	ASSERT(pDesc);
	ASSERT(pDesc->pAccelerationStructure);
	for (unsigned i = 0; i < pDesc->mBottomASIndicesCount; ++i)
	{
		cmdBuildBottomAS(pCmd, pRaytracing, pDesc->pAccelerationStructure, pDesc->pBottomASIndices[i]);
	}
	cmdBuildTopAS(pCmd, pRaytracing, pDesc->pAccelerationStructure);
}

typedef struct UpdateFrequencyLayoutInfo
{
	/// Array of all bindings in the descriptor set
	tinystl::vector<VkDescriptorSetLayoutBinding> mBindings;
	/// Array of all descriptors in this descriptor set
	tinystl::vector<DescriptorInfo*> mDescriptors;
	/// Array of all descriptors marked as dynamic in this descriptor set (applicable to DESCRIPTOR_TYPE_UNIFORM_BUFFER)
	tinystl::vector<DescriptorInfo*> mDynamicDescriptors;
	/// Hash map to get index of the descriptor in the root signature
	tinystl::unordered_map<DescriptorInfo*, uint32_t> mDescriptorIndexMap;
} UpdateFrequencyLayoutInfo;

void addRaytracingRootSignature(Renderer* pRenderer, const ShaderResource* pResources, uint32_t resourceCount, bool local, RootSignature** ppRootSignature, const RootSignatureDesc* pRootDesc) 
{
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	RootSignature* pRootSignature = (RootSignature*)conf_calloc(1, sizeof(*pRootSignature));
	if (local)
	{
		memset(pRootSignature, 0, sizeof(RootSignature));
		*ppRootSignature = pRootSignature;
		return;
	}

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	tinystl::vector<UpdateFrequencyLayoutInfo> layouts(DESCRIPTOR_UPDATE_FREQ_COUNT);
	tinystl::vector<DescriptorInfo*>           pushConstantDescriptors;

	tinystl::unordered_map<tinystl::string, Sampler*> staticSamplerMap;
	tinystl::vector<tinystl::string>                  dynamicUniformBuffers;

	if (pRootDesc)
	{
		for (uint32_t i = 0; i < pRootDesc->mStaticSamplerCount; ++i)
			staticSamplerMap.insert({ pRootDesc->ppStaticSamplerNames[i], pRootDesc->ppStaticSamplers[i] });
		for (uint32_t i = 0; i < pRootDesc->mDynamicUniformBufferCount; ++i)
			dynamicUniformBuffers.push_back(pRootDesc->ppDynamicUniformBufferNames[i]);
	}

	conf_placement_new<tinystl::unordered_map<uint32_t, uint32_t> >(&pRootSignature->pDescriptorNameToIndexMap);

	uint32_t additionalResources = local ? 0 : 1;

	if (resourceCount + additionalResources)
	{
		pRootSignature->mDescriptorCount = resourceCount + additionalResources;
		pRootSignature->pDescriptors = (DescriptorInfo*)conf_calloc(pRootSignature->mDescriptorCount, sizeof(DescriptorInfo));
	}

	// Fill the descriptor array to be stored in the root signature
	for (uint32_t i = 0; i < resourceCount; ++i)
	{
		DescriptorInfo*           pDesc = &pRootSignature->pDescriptors[i];
		ShaderResource const*     pRes = &pResources[i];
		uint32_t                  setIndex = pRes->set;
		DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)setIndex;

		// Copy the binding information generated from the shader reflection into the descriptor
		pDesc->mDesc.reg = pRes->reg;
		pDesc->mDesc.set = pRes->set;
		pDesc->mDesc.size = pRes->size;
		pDesc->mDesc.type = pRes->type;
		pDesc->mDesc.used_stages = pRes->used_stages;
		pDesc->mDesc.name_size = pRes->name_size;
		pDesc->mDesc.name = (const char*)conf_calloc(pDesc->mDesc.name_size + 1, sizeof(char));
		pDesc->mDesc.dim = pRes->dim;
		memcpy((char*)pDesc->mDesc.name, pRes->name, pRes->name_size);

		pRootSignature->pDescriptorNameToIndexMap.insert({ tinystl::hash(pRes->name), i });

		// If descriptor is not a root constant create a new layout binding for this descriptor and add it to the binding array
		if (pDesc->mDesc.type != DESCRIPTOR_TYPE_ROOT_CONSTANT)
		{
			VkDescriptorSetLayoutBinding binding = {};
			binding.binding = pDesc->mDesc.reg;
			binding.descriptorCount = pDesc->mDesc.size;

			binding.descriptorType = util_to_vk_descriptor_type(pDesc->mDesc.type);

			// If a user specified a uniform buffer to be used as a dynamic uniform buffer change its type to VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
			// Also log a message for debugging purpose
			if (dynamicUniformBuffers.find(pDesc->mDesc.name) != dynamicUniformBuffers.end())
			{
				if (pDesc->mDesc.size == 1)
				{
					LOGINFOF("Descriptor (%s) : User specified VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC", pDesc->mDesc.name);
					binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				}
				else
				{
					LOGWARNINGF("Descriptor (%s) : Cannot use VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC for arrays", pDesc->mDesc.name);
				}
			}

			binding.stageFlags =	VK_SHADER_STAGE_RAYGEN_BIT_NV | 
									VK_SHADER_STAGE_ANY_HIT_BIT_NV |
									VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
									VK_SHADER_STAGE_MISS_BIT_NV |
									VK_SHADER_STAGE_INTERSECTION_BIT_NV |
									VK_SHADER_STAGE_CALLABLE_BIT_NV;

			// Store the vulkan related info in the descriptor to avoid constantly calling the util_to_vk mapping functions
			pDesc->mVkType = binding.descriptorType;
			pDesc->mVkStages = binding.stageFlags;
			pDesc->mUpdateFrquency = updateFreq;

			if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
			{
				layouts[setIndex].mDynamicDescriptors.emplace_back(pDesc);
			}

			// Find if the given descriptor is a static sampler
			const tinystl::unordered_hash_node<tinystl::string, Sampler*>* pNode = staticSamplerMap.find(pDesc->mDesc.name).node;
			if (pNode)
			{
				LOGINFOF("Descriptor (%s) : User specified Static Sampler", pDesc->mDesc.name);

				// Set the index to an invalid value so we can use this later for error checking if user tries to update a static sampler
				pDesc->mIndexInParent = -1;
				binding.pImmutableSamplers = &pNode->second->pVkSampler;
			}
			else
			{
				layouts[setIndex].mDescriptors.emplace_back(pDesc);
			}

			layouts[setIndex].mBindings.push_back(binding);
		}
		// If descriptor is a root constant, add it to the root constant array
		else
		{
			pDesc->mDesc.set = 0;
			pDesc->mVkStages = util_to_vk_shader_stage_flags(pDesc->mDesc.used_stages);
			setIndex = 0;
			pushConstantDescriptors.emplace_back(pDesc);
		}

		layouts[setIndex].mDescriptorIndexMap[pDesc] = i;
	}

	//Add acceleration structure
	if (!local)
	{
		const uint32_t setIndex = 1;
		DescriptorInfo* pDesc = &pRootSignature->pDescriptors[resourceCount];
		*pDesc = DescriptorInfo{};
		// Copy the binding information generated from the shader reflection into the descriptor
		pDesc->mDesc.reg = 0;
		pDesc->mDesc.set = setIndex;
		pDesc->mDesc.size = 1;
		pDesc->mDesc.type = DESCRIPTOR_TYPE_BUFFER;
		pDesc->mDesc.used_stages = SHADER_STAGE_COMP;
		const char* name = "gRtScene";
		pDesc->mDesc.name_size = (uint32_t)strlen(name);
		pDesc->mDesc.name = (const char*)conf_calloc(pDesc->mDesc.name_size + 1, sizeof(char));
		memcpy((char*)pDesc->mDesc.name, name, pDesc->mDesc.name_size);
		pRootSignature->pDescriptorNameToIndexMap.insert({ tinystl::hash(name), resourceCount });

		{
			VkDescriptorSetLayoutBinding binding = {};
			binding.binding = pDesc->mDesc.reg;
			binding.descriptorCount = pDesc->mDesc.size;

			binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
			binding.stageFlags =	VK_SHADER_STAGE_RAYGEN_BIT_NV		|
									VK_SHADER_STAGE_ANY_HIT_BIT_NV		|
									VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV	|
									VK_SHADER_STAGE_MISS_BIT_NV			|
									VK_SHADER_STAGE_INTERSECTION_BIT_NV |
									VK_SHADER_STAGE_CALLABLE_BIT_NV;

			// Store the vulkan related info in the descriptor to avoid constantly calling the util_to_vk mapping functions
			pDesc->mVkType = binding.descriptorType;
			pDesc->mVkStages = binding.stageFlags;
			pDesc->mUpdateFrquency = (DescriptorUpdateFrequency)setIndex; //set index

			layouts[setIndex].mDescriptors.emplace_back(pDesc);
			layouts[setIndex].mBindings.push_back(binding);
		}
		layouts[setIndex].mDescriptorIndexMap[pDesc] = resourceCount;
	}

	pRootSignature->mVkPushConstantCount = (uint32_t)pushConstantDescriptors.size();
	if (pRootSignature->mVkPushConstantCount)
		pRootSignature->pVkPushConstantRanges =
		(VkPushConstantRange*)conf_calloc(pRootSignature->mVkPushConstantCount, sizeof(*pRootSignature->pVkPushConstantRanges));

	// Create push constant ranges
	for (uint32_t i = 0; i < pRootSignature->mVkPushConstantCount; ++i)
	{
		VkPushConstantRange* pConst = &pRootSignature->pVkPushConstantRanges[i];
		DescriptorInfo*      pDesc = pushConstantDescriptors[i];
		pDesc->mIndexInParent = i;
		pConst->offset = 0;
		pConst->size = pDesc->mDesc.size;
		pConst->stageFlags = util_to_vk_shader_stage_flags(pDesc->mDesc.used_stages);
	}

	// Create descriptor layouts
	// Put most frequently changed params first
	for (uint32_t i = (uint32_t)layouts.size(); i-- > 0U;)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[i];

		if (layouts[i].mBindings.size())
		{
			// sort table by type (CBV/SRV/UAV) by register
			layout.mBindings.sort([](const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs) {
				return (int)(lhs.binding - rhs.binding);
			});
			layout.mBindings.sort([](const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs) {
				return (int)(lhs.descriptorType - rhs.descriptorType);
			});
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.pNext = NULL;
		layoutInfo.bindingCount = (uint32_t)layout.mBindings.size();
		layoutInfo.pBindings = layout.mBindings.data();
		layoutInfo.flags = 0;

		vkCreateDescriptorSetLayout(pRenderer->pVkDevice, &layoutInfo, NULL, &pRootSignature->mVkDescriptorSetLayouts[i]);

		if (!layouts[i].mBindings.size())
			continue;

		pRootSignature->mVkDescriptorCounts[i] = (uint32_t)layout.mDescriptors.size();
		pRootSignature->pVkDescriptorIndices[i] = (uint32_t*)conf_calloc(layout.mDescriptors.size(), sizeof(uint32_t));

		// Loop through descriptors belonging to this update frequency and increment the cumulative descriptor count
		for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mDescriptors.size(); ++descIndex)
		{
			DescriptorInfo* pDesc = layout.mDescriptors[descIndex];
			pDesc->mIndexInParent = descIndex;
			pDesc->mHandleIndex = pRootSignature->mVkCumulativeDescriptorCounts[i];
			pRootSignature->pVkDescriptorIndices[i][descIndex] = layout.mDescriptorIndexMap[pDesc];
			pRootSignature->mVkCumulativeDescriptorCounts[i] += pDesc->mDesc.size;
		}

		layout.mDynamicDescriptors.sort(
			[](DescriptorInfo* const& lhs, DescriptorInfo* const& rhs) { return (int)(lhs->mDesc.reg - rhs->mDesc.reg); });

		pRootSignature->mVkDynamicDescriptorCounts[i] = (uint32_t)layout.mDynamicDescriptors.size();
		for (uint32_t descIndex = 0; descIndex < pRootSignature->mVkDynamicDescriptorCounts[i]; ++descIndex)
		{
			DescriptorInfo* pDesc = layout.mDynamicDescriptors[descIndex];
			pDesc->mDynamicUniformIndex = descIndex;
		}
	}
	/************************************************************************/
	// Pipeline layout
	/************************************************************************/
	tinystl::vector<VkDescriptorSetLayout> descriptorSetLayouts(DESCRIPTOR_UPDATE_FREQ_COUNT);
	tinystl::vector<VkPushConstantRange>   pushConstants(pRootSignature->mVkPushConstantCount);
	for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
		descriptorSetLayouts[i] = pRootSignature->mVkDescriptorSetLayouts[i];
	for (uint32_t i = 0; i < pRootSignature->mVkPushConstantCount; ++i)
		pushConstants[i] = pRootSignature->pVkPushConstantRanges[i];

	VkPipelineLayoutCreateInfo add_info = {};
	add_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
	add_info.pSetLayouts = descriptorSetLayouts.data();
	add_info.pushConstantRangeCount = pRootSignature->mVkPushConstantCount;
	add_info.pPushConstantRanges = pushConstants.data();
	VkResult vk_res = vkCreatePipelineLayout(pRenderer->pVkDevice, &add_info, NULL, &(pRootSignature->pPipelineLayout));
	ASSERT(VK_SUCCESS == vk_res);
	/************************************************************************/
	/************************************************************************/

	pRootSignature->mPipelineType = PIPELINE_TYPE_RAYTRACING;
	*ppRootSignature = pRootSignature;
}

void CalculateMaxShaderRecordSize(const RaytracingShaderTableRecordDesc* pRecords, uint32_t shaderCount, uint64_t& maxShaderTableSize)
{
	for (uint32_t i = 0; i < shaderCount; ++i)
	{
		const RaytracingShaderTableRecordDesc* pRecord = &pRecords[i];
		uint32_t shaderSize = 0;
		
		//add place for hit shader index and miss shader index
		shaderSize += pRecord->mInvokeTraceRay ? sizeof(uint32) * 2 : 0;

		maxShaderTableSize = maxShaderTableSize > shaderSize ? maxShaderTableSize : shaderSize;
	}
}

uint32_t GetShaderStageIndex(Pipeline* pipeline, const char* name)
{
	tinystl::string nameStr(name);
	for (uint32_t i = 0; i < pipeline->mShadersStagesNames.size(); ++i)
	{
		if (pipeline->mShadersStagesNames[i] == nameStr)
			return i;
	}
	ASSERT(false);
	return 0;
}

void FillShaderIdentifiers(	const RaytracingShaderTableRecordDesc* pRecords, uint32_t shaderCount,
							uint64_t maxShaderTableSize, uint64_t& dstIndex,
							RaytracingShaderTable* pTable, Raytracing* pRaytracing, ShaderLocalData* mShaderLocalData, 
							const uint8_t* shaderHandleStorage)
{
	Pipeline* pipeline = pTable->pPipeline;

	for (uint32_t idx = 0; idx < shaderCount; ++idx)
	{
		uint32_t index = GetShaderStageIndex(pipeline, pRecords[idx].pName);

		uint64_t currentPosition = maxShaderTableSize * dstIndex++;
		uint8_t* dst = (uint8_t*)pTable->pBuffer->pCpuMappedAddress + currentPosition;
		size_t handleSize = pRaytracing->pRenderer->mVkRaytracingProperties->shaderGroupHandleSize;
		const uint8_t* src = &shaderHandleStorage[index * handleSize];
		memcpy(dst, src, handleSize);

		const RaytracingShaderTableRecordDesc* pRecord = &pRecords[idx];

		if (pRecord->mInvokeTraceRay)
		{
			dst += handleSize;
			memcpy(dst, &pRecord->mHitShaderIndex, sizeof(uint32_t));
			dst += sizeof(uint32_t);
			memcpy(dst, &pRecord->mMissShaderIndex, sizeof(uint32_t));
		}
	}
};

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
	CalculateMaxShaderRecordSize(pDesc->pRayGenShader, 1, maxShaderTableSize);
	CalculateMaxShaderRecordSize(pDesc->pMissShaders, pDesc->mMissShaderCount, maxShaderTableSize);
	CalculateMaxShaderRecordSize(pDesc->pHitGroups, pDesc->mHitGroupCount, maxShaderTableSize);
	/************************************************************************/
	// Align max size
	/************************************************************************/
	const uint32_t groupHandleSize = pRaytracing->pRayTracingProperties->shaderGroupHandleSize;
	maxShaderTableSize		= round_up_64(groupHandleSize + maxShaderTableSize, pRaytracing->pRayTracingProperties->shaderGroupBaseAlignment);
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
	uint32_t groupCount = (uint32_t)pDesc->pPipeline->mShadersStagesNames.size();
	uint8_t* shaderHandleStorage = (uint8_t*)conf_calloc(groupCount, sizeof(uint8_t) * groupHandleSize);
	VkResult code = vkGetRayTracingShaderGroupHandlesNV(pRaytracing->pRenderer->pVkDevice, 
														pDesc->pPipeline->pVkPipeline, 0, groupCount, 
														groupHandleSize * groupCount, shaderHandleStorage);

	pTable->mHitMissLocalData.resize(pDesc->mMissShaderCount + pDesc->mHitGroupCount + 1);
	uint64_t index = 0;
	FillShaderIdentifiers(	pDesc->pRayGenShader, 1, maxShaderTableSize, index, pTable, pRaytracing, 
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

void GenerateStagesGroupsData(const RaytracingPipelineDesc* pDesc, 
								tinystl::vector<VkPipelineShaderStageCreateInfo>& stages,
								tinystl::vector<tinystl::string>&	stagesNames,
								tinystl::vector<VkRayTracingShaderGroupCreateInfoNV>& groups)
{
	stages.clear();
	stages.reserve(1 + pDesc->mMissShaderCount + pDesc->mHitGroupCount);
	groups.clear();
	groups.reserve(1 + pDesc->mMissShaderCount + pDesc->mHitGroupCount);
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
		stagesNames.push_back(pDesc->pRayGenShader->mEntryNames[0]);

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
			stagesNames.push_back(pDesc->ppMissShaders[i]->mEntryNames[0]);

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
				stagesNames.push_back(pDesc->pHitGroups[i].pHitGroupName);

				groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV;
				groupInfo.intersectionShader = (uint32_t)stages.size() - 1;
			}
			if (pDesc->pHitGroups[i].pAnyHitShader)
			{
				stageCreateInfo.stage = VK_SHADER_STAGE_ANY_HIT_BIT_NV;
				stageCreateInfo.module = pDesc->pHitGroups[i].pAnyHitShader->pShaderModules[0];
				stages.push_back(stageCreateInfo);
				stagesNames.push_back(pDesc->pHitGroups[i].pHitGroupName);

				groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
				groupInfo.anyHitShader = (uint32_t)stages.size() - 1;
			}
			if (pDesc->pHitGroups[i].pClosestHitShader)
			{
				stageCreateInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;
				stageCreateInfo.module = pDesc->pHitGroups[i].pClosestHitShader->pShaderModules[0];
				stages.push_back(stageCreateInfo);
				stagesNames.push_back(pDesc->pHitGroups[i].pHitGroupName);

				groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
				groupInfo.closestHitShader = (uint32_t)stages.size() - 1;
			}
			groups.push_back(groupInfo);
		}
	}
}

void addRaytracingPipeline(const RaytracingPipelineDesc* pDesc, Pipeline** ppPipeline) 
{
	Pipeline* pResult = (Pipeline*)conf_calloc(1, sizeof(Pipeline));
	conf_placement_new<Pipeline>(pResult);

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	tinystl::vector<VkPipelineShaderStageCreateInfo> stages;
	tinystl::vector<VkRayTracingShaderGroupCreateInfoNV> groups;
	GenerateStagesGroupsData(pDesc, stages, pResult->mShadersStagesNames, groups);

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
	
	vkCreateRayTracingPipelinesNV(pDesc->pRaytracing->pRenderer->pVkDevice, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pResult->pVkPipeline);

	*ppPipeline = pResult;
}

extern VkPipelineBindPoint gPipelineBindPoint[PIPELINE_TYPE_COUNT];

void get_descriptor_set_update_info(RootSignature* rootSignature, struct DescriptorBinder* pDescriptorPool,
									const int setIndex, VkDescriptorUpdateTemplate* updateTemplate,
									void** pUpdateData, uint32_t** pDynamicOffset,
									VkDescriptorPool* pDescriptorPoolHeap);
void get_descriptor_set(DescriptorBinder* pDescriptorBinder, Renderer* pRenderer, int setIndex, RootSignature* pRootSignature, VkDescriptorSet* pDescriptorSet);

void cmdDispatchRays(Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc)
{
	vkCmdBindPipeline(pCmd->pVkCmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pDesc->pPipeline->pVkPipeline);
	cmdBindDescriptors(pCmd, pDesc->pDescriptorBinder,
						pDesc->pRootSignature,
						pDesc->mRootSignatureDescriptorsCount,
						pDesc->pRootSignatureDescriptorData);

	//Here is the initial code to bind acceleration structure.
	//It is required because cmdBindDescriptors do not support it yet
	const uint32_t accelStructSetIndex = 1;
	const uint32_t accelStructBindingPoint = 0;
	void* pUpdateData = nullptr;
	uint32_t* pDynamicOffset = nullptr;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkDescriptorUpdateTemplate updateTemplate = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

	get_descriptor_set(pDesc->pDescriptorBinder, pRaytracing->pRenderer, accelStructSetIndex, pDesc->pRootSignature, &descriptorSet);
	get_descriptor_set_update_info(pDesc->pRootSignature, pDesc->pDescriptorBinder, accelStructSetIndex,
		&updateTemplate, &pUpdateData, &pDynamicOffset, &descriptorPool);

	VkWriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo;
	descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
	descriptorAccelerationStructureInfo.pNext = nullptr;
	descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
	descriptorAccelerationStructureInfo.pAccelerationStructures = &pDesc->pTopLevelAccelerationStructure->mAccelerationStructure;

	VkWriteDescriptorSet accelerationStructureWrite;
	accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo; // Notice that pNext is assigned here!
	accelerationStructureWrite.dstSet = descriptorSet;
	accelerationStructureWrite.dstBinding = 0;
	accelerationStructureWrite.dstArrayElement = 0;
	accelerationStructureWrite.descriptorCount = 1;
	accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
	accelerationStructureWrite.pImageInfo = nullptr;
	accelerationStructureWrite.pBufferInfo = nullptr;
	accelerationStructureWrite.pTexelBufferView = nullptr;
	vkUpdateDescriptorSets(pRaytracing->pRenderer->pVkDevice, 1, &accelerationStructureWrite, 0, VK_NULL_HANDLE);
	vkCmdBindDescriptorSets(pCmd->pVkCmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV,
		pDesc->pRootSignature->pPipelineLayout, accelStructSetIndex, 1, &descriptorSet, 0, nullptr);

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
		if (pAccelerationStructure->ppBottomAS[i].pIndexBuffer != nullptr)
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

#else

bool isRaytracingSupported(Renderer* pRenderer) {
	return false;
}

bool initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing)
{
	*ppRaytracing = NULL;
	return false;
}

void removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing)
{
}

/// pScratchBufferSize - Holds the size of scratch buffer to be passed to cmdBuildAccelerationStructure
void addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, AccelerationStructure** ppAccelerationStructure)
{
	*ppAccelerationStructure = NULL;
}

void removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
}

void addRaytracingShaderTable(Raytracing* pRaytracing, const RaytracingShaderTableDesc* pDesc, RaytracingShaderTable** ppTable)
{
	*ppTable = NULL;
}

void removeRaytracingShaderTable(Raytracing* pRaytracing, RaytracingShaderTable* pTable)
{
}

void cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc)
{
}

void cmdDispatchRays(Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc)
{
}

#endif
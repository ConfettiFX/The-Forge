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

#ifdef DIRECT3D12

// Socket is used in microprofile this header need to be included before d3d12 headers
#include <WinSock2.h>

#ifdef XBOX
#include "../../../Xbox/Common_3/Graphics/Direct3D12/Direct3D12X.h"
#else
#define IID_ARGS IID_PPV_ARGS
#include <d3d12.h>
#include <d3dcompiler.h>
#endif

// OS
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

// Renderer
#include "../Interfaces/IGraphics.h"
#include "../Interfaces/IRay.h"
#include "../../Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "Direct3D12Hooks.h"
#include "../../Utilities/Interfaces/IMemory.h"

//check if WindowsSDK is used which supports raytracing
#ifdef D3D12_RAYTRACING_AVAILABLE

DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)

// Enable experimental features and return if they are supported.
// To test them being supported we need to check both their enablement as well as device creation afterwards.
inline bool EnableD3D12ExperimentalFeatures(UUID* experimentalFeatures, uint32_t featureCount)
{
	ID3D12Device* testDevice = NULL;
	bool          ret = SUCCEEDED(D3D12EnableExperimentalFeatures(featureCount, experimentalFeatures, NULL, NULL)) &&
			   SUCCEEDED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_ARGS(&testDevice)));
	if (ret)
		testDevice->Release();

	return ret;
}

// Enable experimental features required for compute-based raytracing fallback.
// This will set active D3D12 devices to DEVICE_REMOVED state.
// Returns bool whether the call succeeded and the device supports the feature.
inline bool EnableComputeRaytracingFallback()
{
	UUID experimentalFeatures[] = { D3D12ExperimentalShaderModels };
	return EnableD3D12ExperimentalFeatures(experimentalFeatures, 1);
}

/************************************************************************/
// Utility Functions Declarations
/************************************************************************/
D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS util_to_dx_acceleration_structure_build_flags(AccelerationStructureBuildFlags flags);
D3D12_RAYTRACING_GEOMETRY_FLAGS                     util_to_dx_geometry_flags(AccelerationStructureGeometryFlags flags);
D3D12_RAYTRACING_INSTANCE_FLAGS                     util_to_dx_instance_flags(AccelerationStructureInstanceFlags flags);
/************************************************************************/
// Forge Raytracing Implementation using DXR
/************************************************************************/
struct AccelerationStructureBottom
{
	Buffer*                                             pVertexBuffer;
	Buffer*                                             pIndexBuffer;
	Buffer*                                             pASBuffer;
	D3D12_RAYTRACING_GEOMETRY_DESC*                     pGeometryDescs;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS mFlags;
	uint32_t                                            mDescCount;
};

struct AccelerationStructure
{
	AccelerationStructureBottom                         mBottomAS;
	Buffer*                                             pInstanceDescBuffer;
	Buffer*                                             pASBuffer;
	Buffer*                                             pScratchBuffer;
	uint32_t                                            mInstanceDescCount;
	uint32_t                                            mScratchBufferSize;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS mFlags;
};

struct RaytracingShaderTable
{
	Pipeline*                   pPipeline;
	Buffer*                     pBuffer;
	D3D12_GPU_DESCRIPTOR_HANDLE mViewGpuDescriptorHandle[DESCRIPTOR_UPDATE_FREQ_COUNT];
	D3D12_GPU_DESCRIPTOR_HANDLE mSamplerGpuDescriptorHandle[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t                    mViewDescriptorCount[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t                    mSamplerDescriptorCount[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint64_t                    mMaxEntrySize;
	uint64_t                    mMissRecordSize;
	uint64_t                    mHitGroupRecordSize;
};

bool d3d12_isRaytracingSupported(Renderer* pRenderer)
{
	ASSERT(pRenderer);
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 opts5 = {};
	pRenderer->mD3D12.pDxDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opts5, sizeof(opts5));
	return (opts5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);
}

bool d3d12_initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing)
{
	ASSERT(pRenderer);
	ASSERT(ppRaytracing);

	if (!isRaytracingSupported(pRenderer))
	{
		return false;
	}

	Raytracing* pRaytracing = (Raytracing*)tf_calloc(1, sizeof(*pRaytracing));
	ASSERT(pRaytracing);

	pRaytracing->pRenderer = pRenderer;
	pRenderer->mD3D12.pDxDevice->QueryInterface(IID_ARGS(&pRaytracing->pDxrDevice));

	*ppRaytracing = pRaytracing;
	return true;
}

void d3d12_removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing)
{
	ASSERT(pRenderer);
	ASSERT(pRaytracing);

	if (pRaytracing->pDxrDevice)
		pRaytracing->pDxrDevice->Release();

	tf_free(pRaytracing);
}

HRESULT createBottomAS(
	Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, uint32_t* pScratchBufferSize, AccelerationStructureBottom* pOut)
{
	ASSERT(pRaytracing);
	ASSERT(pDesc);
	ASSERT(pScratchBufferSize);

	uint32_t                    scratchBufferSize = 0;
	AccelerationStructureBottom blas = {};

	blas.mDescCount = pDesc->mBottomASDesc->mDescCount;
	blas.mFlags = util_to_dx_acceleration_structure_build_flags(pDesc->mBottomASDesc->mFlags);
	blas.pGeometryDescs = (D3D12_RAYTRACING_GEOMETRY_DESC*)tf_calloc(blas.mDescCount, sizeof(D3D12_RAYTRACING_GEOMETRY_DESC));
	for (uint32_t j = 0; j < blas.mDescCount; ++j)
	{
		AccelerationStructureGeometryDesc* pGeom = &pDesc->mBottomASDesc->pGeometryDescs[j];
		D3D12_RAYTRACING_GEOMETRY_DESC*    pGeomD3D12 = &blas.pGeometryDescs[j];

		pGeomD3D12->Flags = util_to_dx_geometry_flags(pGeom->mFlags);

		blas.pIndexBuffer = {};
		if (pGeom->mIndexCount)
		{
			ASSERT(pGeom->pIndices16 != NULL || pGeom->pIndices32 != NULL);

			BufferLoadDesc ibDesc = {};
			ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
			ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			ibDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
			ibDesc.mDesc.mSize = (pGeom->mIndexType == INDEX_TYPE_UINT32 ? sizeof(uint32_t) : sizeof(uint16_t)) * pGeom->mIndexCount;
			ibDesc.pData = pGeom->mIndexType == INDEX_TYPE_UINT32 ? (void*)pGeom->pIndices32 : (void*)pGeom->pIndices16;
			ibDesc.ppBuffer = &blas.pIndexBuffer;
			addResource(&ibDesc, NULL);

			pGeomD3D12->Triangles.IndexBuffer = blas.pIndexBuffer->mD3D12.mDxGpuAddress;
			pGeomD3D12->Triangles.IndexCount =
				(UINT)ibDesc.mDesc.mSize / (pGeom->mIndexType == INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t));
			pGeomD3D12->Triangles.IndexFormat = (pGeom->mIndexType == INDEX_TYPE_UINT16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);
		}

		ASSERT(pGeom->pVertexArray);
		ASSERT(pGeom->mVertexCount);

		BufferLoadDesc vbDesc = {};
		vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vbDesc.mDesc.mSize = sizeof(float3) * pGeom->mVertexCount;
		vbDesc.pData = pGeom->pVertexArray;
		vbDesc.ppBuffer = &blas.pVertexBuffer;
		addResource(&vbDesc, NULL);

		pGeomD3D12->Triangles.VertexBuffer.StartAddress = blas.pVertexBuffer->mD3D12.mDxGpuAddress;
		pGeomD3D12->Triangles.VertexBuffer.StrideInBytes = (UINT)sizeof(float3);
		pGeomD3D12->Triangles.VertexCount = (UINT)vbDesc.mDesc.mSize / (UINT)sizeof(float3);
		if (pGeomD3D12->Triangles.VertexBuffer.StrideInBytes == sizeof(float))
			pGeomD3D12->Triangles.VertexFormat = DXGI_FORMAT_R32_FLOAT;
		else if (pGeomD3D12->Triangles.VertexBuffer.StrideInBytes == sizeof(float) * 2)
			pGeomD3D12->Triangles.VertexFormat = DXGI_FORMAT_R32G32_FLOAT;
		else if (pGeomD3D12->Triangles.VertexBuffer.StrideInBytes == sizeof(float) * 3)
			pGeomD3D12->Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		else if (pGeomD3D12->Triangles.VertexBuffer.StrideInBytes == sizeof(float) * 4)
			pGeomD3D12->Triangles.VertexFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	}

	/************************************************************************/
	// Get the size requirement for the Acceleration Structures
	/************************************************************************/
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc = {};
	prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	prebuildDesc.Flags = blas.mFlags;
	prebuildDesc.NumDescs = blas.mDescCount;
	prebuildDesc.pGeometryDescs = blas.pGeometryDescs;
	prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	pRaytracing->pDxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

	/************************************************************************/
	// Allocate Acceleration Structure Buffer
	/************************************************************************/
	BufferDesc bufferDesc = {};
	bufferDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
	bufferDesc.mStructStride = 0;
	bufferDesc.mFirstElement = 0;
	bufferDesc.mElementCount = info.ResultDataMaxSizeInBytes / sizeof(UINT32);
	bufferDesc.mSize = info.ResultDataMaxSizeInBytes;    //Rustam: isn't this should be sizeof(UINT32) ?
	bufferDesc.mStartState = RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	addBuffer(pRaytracing->pRenderer, &bufferDesc, &blas.pASBuffer);
	/************************************************************************/
	// Store the scratch buffer size so user can create the scratch buffer accordingly
	/************************************************************************/
	scratchBufferSize = (UINT)info.ScratchDataSizeInBytes > scratchBufferSize ? (UINT)info.ScratchDataSizeInBytes : scratchBufferSize;

	*pScratchBufferSize = scratchBufferSize;
	*pOut = blas;

	return S_OK;
}

HRESULT createTopAS(
	Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, const AccelerationStructureBottom* pASBottom,
	uint32_t* pScratchBufferSize, Buffer** ppInstanceDescBuffer, Buffer** ppOut)
{
	ASSERT(pRaytracing);
	ASSERT(pDesc);
	ASSERT(pScratchBufferSize);
	ASSERT(pASBottom);
	ASSERT(ppInstanceDescBuffer);
	/************************************************************************/
	// Get the size requirement for the Acceleration Structures
	/************************************************************************/
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc = {};
	prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	prebuildDesc.Flags = util_to_dx_acceleration_structure_build_flags(pDesc->mFlags);
	prebuildDesc.NumDescs = pDesc->mInstancesDescCount;
	prebuildDesc.pGeometryDescs = NULL;
	prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	pRaytracing->pDxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

	/************************************************************************/
	/*  Construct buffer with instances descriptions                        */
	/************************************************************************/
	D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs = NULL;
	arrsetlen(instanceDescs, pDesc->mInstancesDescCount);
	for (uint32_t i = 0; i < pDesc->mInstancesDescCount; ++i)
	{
		AccelerationStructureInstanceDesc* pInst = &pDesc->pInstanceDescs[i];
		const Buffer*                      pASBuffer = pASBottom[pInst->mAccelerationStructureIndex].pASBuffer;
		instanceDescs[i].AccelerationStructure = pASBuffer->mD3D12.pDxResource->GetGPUVirtualAddress();
		instanceDescs[i].Flags = util_to_dx_instance_flags(pInst->mFlags);
		instanceDescs[i].InstanceContributionToHitGroupIndex = pInst->mInstanceContributionToHitGroupIndex;
		instanceDescs[i].InstanceID = pInst->mInstanceID;
		instanceDescs[i].InstanceMask = pInst->mInstanceMask;

		memcpy(instanceDescs[i].Transform, pInst->mTransform, sizeof(float[12])); //-V595
	}

	BufferDesc instanceDesc = {};
	instanceDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	instanceDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	instanceDesc.mSize = arrlenu(instanceDescs) * sizeof(instanceDescs[0]);
	Buffer* pInstanceDescBuffer = {};
	addBuffer(pRaytracing->pRenderer, &instanceDesc, &pInstanceDescBuffer);
	if (arrlen( instanceDescs))
		memcpy(pInstanceDescBuffer->pCpuMappedAddress, instanceDescs, instanceDesc.mSize);
	arrfree(instanceDescs);
	/************************************************************************/
	// Allocate Acceleration Structure Buffer
	/************************************************************************/
	BufferDesc bufferDesc = {};
	bufferDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER_RAW | DESCRIPTOR_TYPE_BUFFER_RAW;
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	bufferDesc.mStructStride = 0;
	bufferDesc.mFirstElement = 0;
	bufferDesc.mElementCount = info.ResultDataMaxSizeInBytes / sizeof(UINT32);
	bufferDesc.mSize = info.ResultDataMaxSizeInBytes;
	bufferDesc.mStartState = RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	Buffer* pTopASBuffer = {};
	addBuffer(pRaytracing->pRenderer, &bufferDesc, &pTopASBuffer);

	extern void add_srv(Renderer*, ID3D12Resource*, const D3D12_SHADER_RESOURCE_VIEW_DESC*, DxDescriptorID*);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.RaytracingAccelerationStructure.Location = pTopASBuffer->mD3D12.mDxGpuAddress;
	DxDescriptorID srv = pTopASBuffer->mD3D12.mDescriptors + pTopASBuffer->mD3D12.mSrvDescriptorOffset;
	add_srv(pRaytracing->pRenderer, NULL, &srvDesc, &srv);

	*pScratchBufferSize = (UINT)info.ScratchDataSizeInBytes;
	*ppInstanceDescBuffer = pInstanceDescBuffer;
	*ppOut = pTopASBuffer;

	return S_OK;
}

void d3d12_addAccelerationStructure(
	Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, AccelerationStructure** ppAccelerationStructure)
{
	ASSERT(pRaytracing);
	ASSERT(pDesc);
	ASSERT(ppAccelerationStructure);

	AccelerationStructure* pAccelerationStructure = (AccelerationStructure*)tf_calloc(1, sizeof(*pAccelerationStructure));
	ASSERT(pAccelerationStructure);

	uint32_t scratchBottomBufferSize = 0;
	CHECK_HRESULT(createBottomAS(pRaytracing, pDesc, &scratchBottomBufferSize, &pAccelerationStructure->mBottomAS));

	uint32_t scratchTopBufferSize = 0;
	pAccelerationStructure->mInstanceDescCount = pDesc->mInstancesDescCount;
	CHECK_HRESULT(createTopAS(
		pRaytracing, pDesc, &pAccelerationStructure->mBottomAS, &scratchTopBufferSize, &pAccelerationStructure->pInstanceDescBuffer,
		&pAccelerationStructure->pASBuffer));

	pAccelerationStructure->mScratchBufferSize = max(scratchBottomBufferSize, scratchTopBufferSize);
	pAccelerationStructure->mFlags = util_to_dx_acceleration_structure_build_flags(pDesc->mFlags);

	//Create scratch buffer
	BufferLoadDesc scratchBufferDesc = {};
	scratchBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
	scratchBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	scratchBufferDesc.mDesc.mStartState = RESOURCE_STATE_COMMON;
	scratchBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
	scratchBufferDesc.mDesc.mSize = pAccelerationStructure->mScratchBufferSize;
	scratchBufferDesc.ppBuffer = &pAccelerationStructure->pScratchBuffer;
	addResource(&scratchBufferDesc, NULL);

	*ppAccelerationStructure = pAccelerationStructure;
}

void d3d12_removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
	ASSERT(pRaytracing);
	ASSERT(pAccelerationStructure);

	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pASBuffer);
	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pInstanceDescBuffer);
	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pScratchBuffer);

	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->mBottomAS.pASBuffer);
	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->mBottomAS.pVertexBuffer);
	if (pAccelerationStructure->mBottomAS.pIndexBuffer)
		removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->mBottomAS.pIndexBuffer);

	tf_free(pAccelerationStructure->mBottomAS.pGeometryDescs);
	tf_free(pAccelerationStructure);
}

void d3d12_removeAccelerationStructureScratch(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
	//NOT IMPLEMENTED
}

static const uint64_t gShaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

void FillShaderIdentifiers(
	const char* const* pRecords, uint32_t shaderCount, ID3D12StateObjectProperties* pRtsoProps, uint64_t maxShaderTableSize,
	uint32_t& index, RaytracingShaderTable* pTable, Raytracing* pRaytracing)
{
	for (uint32_t i = 0; i < shaderCount; ++i)
	{
		const char* pRecordName = pRecords[i];
		const void* pIdentifier = NULL;

		const size_t kMaxRecordNameLength = 511;
		const size_t recordNameLength = strlen(pRecordName);
		WCHAR        pName[kMaxRecordNameLength + 1];
		ASSERT(recordNameLength <= kMaxRecordNameLength);
		mbstowcs(pName, pRecordName, kMaxRecordNameLength);
		pName[min(recordNameLength, kMaxRecordNameLength)] = 0;

		pIdentifier = pRtsoProps->GetShaderIdentifier(pName);

		ASSERT(pIdentifier);

		uint64_t currentPosition = maxShaderTableSize * index++;
		memcpy((uint8_t*)pTable->pBuffer->pCpuMappedAddress + currentPosition, pIdentifier, gShaderIdentifierSize);

		// #TODO
		//if (!pRecord->pRootSignature)
		//	continue;

		currentPosition += gShaderIdentifierSize;
		/************************************************************************/
		// #NOTE : User can specify root data in any order but we need to fill
		// it into the buffer based on the root index associated with each root data entry
		// So we collect them here and do a lookup when looping through the descriptor array
		// from the root signature
		/************************************************************************/
		// #TODO
		//eastl::string_hash_map<const DescriptorData*> data;
		//for (uint32_t desc = 0; desc < pRecord->mRootDataCount; ++desc)
		//{
		//	data.insert(pRecord->pRootData[desc].pName, &pRecord->pRootData[desc]);
		//}

		//for (uint32_t desc = 0; desc < pRecord->pRootSignature->mDescriptorCount; ++desc)
		//{
		//	uint32_t descIndex = -1;
		//	const DescriptorInfo* pDesc = &pRecord->pRootSignature->pDescriptors[desc];
		//	eastl::string_hash_map<const DescriptorData*>::iterator it = data.find(pDesc->mDesc.name);
		//	const DescriptorData* pData = it->second;

		//	switch (pDesc->mDxType)
		//	{
		//	case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
		//	{
		//		memcpy((uint8_t*)pTable->pBuffer->pCpuMappedAddress + currentPosition, pData->pRootConstant, pDesc->mDesc.size * sizeof(uint32_t));
		//		currentPosition += pDesc->mDesc.size * sizeof(uint32_t);
		//		break;
		//	}
		//	case D3D12_ROOT_PARAMETER_TYPE_CBV:
		//	case D3D12_ROOT_PARAMETER_TYPE_SRV:
		//	case D3D12_ROOT_PARAMETER_TYPE_UAV:
		//	{
		//		// Root Descriptors need to be aligned to 8 byte address
		//		currentPosition = round_up_64(currentPosition, gLocalRootDescriptorSize);
		//		uint64_t offset = pData->pOffsets ? pData->pOffsets[0] : 0;
		//		D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = pData->ppBuffers[0]->pDxResource->GetGPUVirtualAddress() + offset;
		//		memcpy((uint8_t*)pTable->pBuffer->pCpuMappedAddress + currentPosition, &cbvAddress, sizeof(D3D12_GPU_VIRTUAL_ADDRESS));
		//		currentPosition += gLocalRootDescriptorSize;
		//		break;
		//	}
		//	default:
		//		break;
		//	}
		//}
	}
}

void d3d12_CalculateMaxShaderRecordSize(const char* const* pRecords, uint32_t shaderCount, uint64_t& maxShaderTableSize)
{
	// #TODO
	//for (uint32_t i = 0; i < shaderCount; ++i)
	//{
	//	eastl::hash_set<uint32_t> addedTables;
	//	const RaytracingShaderTableRecordDesc* pRecord = &pRecords[i];
	//	uint32_t shaderSize = 0;
	//	for (uint32_t desc = 0; desc < pRecord->mRootDataCount; ++desc)
	//	{
	//		uint32_t descIndex = -1;
	//		const DescriptorInfo* pDesc = get_descriptor(pRecord->pRootSignature, pRecord->pRootData[desc].pName, &descIndex);
	//		ASSERT(pDesc);

	//		switch (pDesc->mDxType)
	//		{
	//		case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
	//			shaderSize += pDesc->mDesc.size * gLocalRootConstantSize;
	//			break;
	//		case D3D12_ROOT_PARAMETER_TYPE_CBV:
	//		case D3D12_ROOT_PARAMETER_TYPE_SRV:
	//		case D3D12_ROOT_PARAMETER_TYPE_UAV:
	//			shaderSize += gLocalRootDescriptorSize;
	//			break;
	//		case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
	//		{
	//			const uint32_t rootIndex = pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER ?
	//				pRecord->pRootSignature->mDxSamplerDescriptorTableRootIndices[pDesc->mUpdateFrquency] :
	//				pRecord->pRootSignature->mDxViewDescriptorTableRootIndices[pDesc->mUpdateFrquency];
	//			if (addedTables.find(rootIndex) == addedTables.end())
	//				shaderSize += gLocalRootDescriptorTableSize;
	//			else
	//				addedTables.insert(rootIndex);
	//			break;
	//		}
	//		default:
	//			break;
	//		}
	//	}

	//	maxShaderTableSize = max(maxShaderTableSize, shaderSize);
	//}
}

void d3d12_addRaytracingShaderTable(Raytracing* pRaytracing, const RaytracingShaderTableDesc* pDesc, RaytracingShaderTable** ppTable)
{
	ASSERT(pRaytracing);
	ASSERT(pDesc);
	ASSERT(pDesc->pPipeline);
	ASSERT(ppTable);
	ASSERT(pDesc->pRayGenShader);

	RaytracingShaderTable* pTable = (RaytracingShaderTable*)tf_calloc(1, sizeof(*pTable));
	tf_placement_new<RaytracingShaderTable>((void*)pTable);
	ASSERT(pTable);

	pTable->pPipeline = pDesc->pPipeline;

	const uint32_t rayGenShaderCount = 1;
	const uint32_t recordCount = rayGenShaderCount + pDesc->mMissShaderCount + pDesc->mHitGroupCount;
	uint64_t       maxShaderTableSize = 0;
	/************************************************************************/
	// Calculate max size for each element in the shader table
	/************************************************************************/
	d3d12_CalculateMaxShaderRecordSize(&pDesc->pRayGenShader, 1, maxShaderTableSize);
	d3d12_CalculateMaxShaderRecordSize(pDesc->pMissShaders, pDesc->mMissShaderCount, maxShaderTableSize);
	d3d12_CalculateMaxShaderRecordSize(pDesc->pHitGroups, pDesc->mHitGroupCount, maxShaderTableSize);
	/************************************************************************/
	// Align max size
	/************************************************************************/
	maxShaderTableSize = round_up_64(gShaderIdentifierSize + maxShaderTableSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	pTable->mMaxEntrySize = maxShaderTableSize;
	/************************************************************************/
	// Create shader table buffer
	/************************************************************************/
	BufferDesc bufferDesc = {};
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	bufferDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	bufferDesc.mSize = maxShaderTableSize * recordCount;
	bufferDesc.pName = "RTShadersTable";
	addBuffer(pRaytracing->pRenderer, &bufferDesc, &pTable->pBuffer);
	/************************************************************************/
	// Copy shader identifiers into the buffer
	/************************************************************************/
	ID3D12StateObjectProperties* pRtsoProps = NULL;
	pDesc->pPipeline->mD3D12.pDxrPipeline->QueryInterface(IID_ARGS(&pRtsoProps));

	uint32_t index = 0;
	FillShaderIdentifiers(&pDesc->pRayGenShader, 1, pRtsoProps, maxShaderTableSize, index, pTable, pRaytracing);

	pTable->mMissRecordSize = maxShaderTableSize * pDesc->mMissShaderCount;
	FillShaderIdentifiers(pDesc->pMissShaders, pDesc->mMissShaderCount, pRtsoProps, maxShaderTableSize, index, pTable, pRaytracing);

	pTable->mHitGroupRecordSize = maxShaderTableSize * pDesc->mHitGroupCount;
	FillShaderIdentifiers(pDesc->pHitGroups, pDesc->mHitGroupCount, pRtsoProps, maxShaderTableSize, index, pTable, pRaytracing);

	if (pRtsoProps)
		pRtsoProps->Release();
	/************************************************************************/
	/************************************************************************/

	*ppTable = pTable;
}

void d3d12_removeRaytracingShaderTable(Raytracing* pRaytracing, RaytracingShaderTable* pTable)
{
	ASSERT(pRaytracing);
	ASSERT(pTable);

	removeBuffer(pRaytracing->pRenderer, pTable->pBuffer);

	pTable->~RaytracingShaderTable();
	tf_free(pTable);
}

/************************************************************************/
// Raytracing Command Buffer Functions Implementation
/************************************************************************/
void util_build_acceleration_structure(
	ID3D12GraphicsCommandList4* pDxrCmd, ID3D12Resource* pScratchBuffer, ID3D12Resource* pASBuffer,
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE ASType, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS ASFlags,
	const D3D12_RAYTRACING_GEOMETRY_DESC* pGeometryDescs, D3D12_GPU_VIRTUAL_ADDRESS pInstanceDescBuffer, uint32_t descCount)
{
	ASSERT(pDxrCmd);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	buildDesc.Inputs.Type = ASType;
	buildDesc.DestAccelerationStructureData = pASBuffer->GetGPUVirtualAddress();
	buildDesc.Inputs.Flags = ASFlags;
	buildDesc.Inputs.pGeometryDescs = NULL;

	if (ASType == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL)
		buildDesc.Inputs.pGeometryDescs = pGeometryDescs;
	else if (ASType == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL)
		buildDesc.Inputs.InstanceDescs = pInstanceDescBuffer;

	buildDesc.Inputs.NumDescs = descCount;
	buildDesc.ScratchAccelerationStructureData = pScratchBuffer->GetGPUVirtualAddress();

	pDxrCmd->BuildRaytracingAccelerationStructure(&buildDesc, 0, NULL);

	D3D12_RESOURCE_BARRIER uavBarrier = {};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = pASBuffer;
	pDxrCmd->ResourceBarrier(1, &uavBarrier);
}

void d3d12_cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc)
{
	ASSERT(pDesc);
	ASSERT(pDesc->ppAccelerationStructures);

	ID3D12GraphicsCommandList4* pDxrCmd = NULL;
	pCmd->mD3D12.pDxCmdList->QueryInterface(IID_ARGS(&pDxrCmd));
	ASSERT(pDxrCmd);

	for (uint32_t i = 0; i < pDesc->mBottomASIndicesCount; ++i)
	{
		AccelerationStructure* as = pDesc->ppAccelerationStructures[pDesc->pBottomASIndices[i]];

		util_build_acceleration_structure(
			pDxrCmd, as->pScratchBuffer->mD3D12.pDxResource, as->mBottomAS.pASBuffer->mD3D12.pDxResource,
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL, as->mBottomAS.mFlags, as->mBottomAS.pGeometryDescs, NULL,
			as->mBottomAS.mDescCount);
	}

	for (uint32_t i = 0; i < pDesc->mCount; ++i)
	{
		AccelerationStructure* as = pDesc->ppAccelerationStructures[i];

		util_build_acceleration_structure(
			pDxrCmd, as->pScratchBuffer->mD3D12.pDxResource, as->pASBuffer->mD3D12.pDxResource,
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL, as->mFlags, NULL,
			as->pInstanceDescBuffer->mD3D12.pDxResource->GetGPUVirtualAddress(), as->mInstanceDescCount);
	}

	pDxrCmd->Release();
}

void d3d12_cmdDispatchRays(Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc)
{
	const RaytracingShaderTable* pShaderTable = pDesc->pShaderTable;
	/************************************************************************/
	// Compute shader table GPU addresses
	// #TODO: Support for different offsets into the shader table
	/************************************************************************/
	D3D12_GPU_VIRTUAL_ADDRESS startAddress = pDesc->pShaderTable->pBuffer->mD3D12.pDxResource->GetGPUVirtualAddress();

	D3D12_GPU_VIRTUAL_ADDRESS_RANGE rayGenShaderRecord = {};
	rayGenShaderRecord.SizeInBytes = pShaderTable->mMaxEntrySize;
	rayGenShaderRecord.StartAddress = startAddress + pShaderTable->mMaxEntrySize * 0;

	D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE missShaderTable = {};
	missShaderTable.SizeInBytes = pShaderTable->mMissRecordSize;
	missShaderTable.StartAddress = startAddress + pShaderTable->mMaxEntrySize;
	missShaderTable.StrideInBytes = pShaderTable->mMaxEntrySize;

	D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE hitGroupTable = {};
	hitGroupTable.SizeInBytes = pShaderTable->mHitGroupRecordSize;
	hitGroupTable.StartAddress = startAddress + pShaderTable->mMaxEntrySize + pShaderTable->mMissRecordSize;
	hitGroupTable.StrideInBytes = pShaderTable->mMaxEntrySize;
	/************************************************************************/
	/************************************************************************/
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	dispatchDesc.Height = pDesc->mHeight;
	dispatchDesc.Width = pDesc->mWidth;
	dispatchDesc.Depth = 1;
	dispatchDesc.RayGenerationShaderRecord = rayGenShaderRecord;
	dispatchDesc.MissShaderTable = missShaderTable;
	dispatchDesc.HitGroupTable = hitGroupTable;

	ID3D12GraphicsCommandList4* pDxrCmd = NULL;
	pCmd->mD3D12.pDxCmdList->QueryInterface(IID_ARGS(&pDxrCmd));
	pDxrCmd->SetPipelineState1(pShaderTable->pPipeline->mD3D12.pDxrPipeline);
	pDxrCmd->DispatchRays(&dispatchDesc);
	pDxrCmd->Release();
}
/************************************************************************/
// Utility Functions Implementation
/************************************************************************/
D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS util_to_dx_acceleration_structure_build_flags(AccelerationStructureBuildFlags flags)
{
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS ret = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION)
		ret |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;
	if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE)
		ret |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY)
		ret |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY;
	if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE)
		ret |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
	if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD)
		ret |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	if (flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE)
		ret |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	return ret;
}

D3D12_RAYTRACING_GEOMETRY_FLAGS util_to_dx_geometry_flags(AccelerationStructureGeometryFlags flags)
{
	D3D12_RAYTRACING_GEOMETRY_FLAGS ret = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
	if (flags & ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE)
		ret |= D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	if (flags & ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION)
		ret |= D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION;

	return ret;
}

D3D12_RAYTRACING_INSTANCE_FLAGS util_to_dx_instance_flags(AccelerationStructureInstanceFlags flags)
{
	D3D12_RAYTRACING_INSTANCE_FLAGS ret = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_OPAQUE)
		ret |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE)
		ret |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE)
		ret |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;

	return ret;
}

void addRaytracingPipeline(const RaytracingPipelineDesc* pDesc, Pipeline** ppPipeline)
{
	Raytracing* pRaytracing = pDesc->pRaytracing;

	ASSERT(pRaytracing);
	ASSERT(pDesc);
	ASSERT(ppPipeline);

	Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
	ASSERT(pPipeline);

	pPipeline->mD3D12.mType = PIPELINE_TYPE_RAYTRACING;
	pPipeline->mD3D12.pRootSignature = pDesc->pGlobalRootSignature->mD3D12.pDxRootSignature;
	/************************************************************************/
	// Pipeline Creation
	/************************************************************************/
	typedef struct ExportAssociation
	{
		uint32_t								mIndex;
		D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*	pAssociation;
	}ExportAssociation;
	D3D12_STATE_SUBOBJECT*	subobjects = NULL;
	D3D12_STATE_SUBOBJECT	tmpSubobject;
	ExportAssociation		tmpExportAssociation;
	ExportAssociation*		exportAssociationsDelayed = NULL;
	// Reserve average number of subobject space in the beginning
	arrsetcap(subobjects, 10);
	/************************************************************************/
	// Step 1 - Create DXIL Libraries
	/************************************************************************/
	D3D12_DXIL_LIBRARY_DESC* dxilLibDescs = NULL;
	D3D12_EXPORT_DESC**      exportDesc = NULL;

	D3D12_DXIL_LIBRARY_DESC rayGenDesc = {};
	D3D12_EXPORT_DESC       rayGenExportDesc = {};
	rayGenExportDesc.ExportToRename = NULL;
	rayGenExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;
	rayGenExportDesc.Name = pDesc->pRayGenShader->mD3D12.pEntryNames[0];

	rayGenDesc.DXILLibrary.BytecodeLength = pDesc->pRayGenShader->mD3D12.pShaderBlobs[0]->GetBufferSize();
	rayGenDesc.DXILLibrary.pShaderBytecode = pDesc->pRayGenShader->mD3D12.pShaderBlobs[0]->GetBufferPointer();
	rayGenDesc.NumExports = 1;
	rayGenDesc.pExports = &rayGenExportDesc;

	arrpush(dxilLibDescs, rayGenDesc);

	LPCWSTR* missShadersEntries = NULL;
	arrsetlen(missShadersEntries, pDesc->mMissShaderCount);
	for (uint32_t i = 0; i < pDesc->mMissShaderCount; ++i)
	{
		D3D12_EXPORT_DESC* pMissExportDesc = (D3D12_EXPORT_DESC*)tf_calloc(1, sizeof(*pMissExportDesc));
		pMissExportDesc->ExportToRename = NULL;
		pMissExportDesc->Flags = D3D12_EXPORT_FLAG_NONE;

		pMissExportDesc->Name = pDesc->ppMissShaders[i]->mD3D12.pEntryNames[0];
		missShadersEntries[i] = pMissExportDesc->Name;

		D3D12_DXIL_LIBRARY_DESC missDesc = {};
		missDesc.DXILLibrary.BytecodeLength = pDesc->ppMissShaders[i]->mD3D12.pShaderBlobs[0]->GetBufferSize();
		missDesc.DXILLibrary.pShaderBytecode = pDesc->ppMissShaders[i]->mD3D12.pShaderBlobs[0]->GetBufferPointer();
		missDesc.NumExports = 1;
		missDesc.pExports = pMissExportDesc;

		arrpush(exportDesc, pMissExportDesc);
		arrpush(dxilLibDescs, missDesc);
	}

	LPCWSTR* hitGroupsIntersectionsEntries = NULL;
	LPCWSTR* hitGroupsAnyHitEntries = NULL;
	LPCWSTR* hitGroupsClosestHitEntries = NULL;

	arrsetlen(hitGroupsIntersectionsEntries, pDesc->mHitGroupCount);
	arrsetlen(hitGroupsAnyHitEntries, pDesc->mHitGroupCount);
	arrsetlen(hitGroupsClosestHitEntries, pDesc->mHitGroupCount);

	for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
	{
		if (pDesc->pHitGroups[i].pIntersectionShader)
		{
			D3D12_EXPORT_DESC* pIntersectionExportDesc = (D3D12_EXPORT_DESC*)tf_calloc(1, sizeof(*pIntersectionExportDesc));
			pIntersectionExportDesc->ExportToRename = NULL;
			pIntersectionExportDesc->Flags = D3D12_EXPORT_FLAG_NONE;
			pIntersectionExportDesc->Name = pDesc->pHitGroups[i].pIntersectionShader->mD3D12.pEntryNames[0];
			hitGroupsIntersectionsEntries[i] = pIntersectionExportDesc->Name;

			D3D12_DXIL_LIBRARY_DESC intersectionDesc = {};
			intersectionDesc.DXILLibrary.BytecodeLength = pDesc->pHitGroups[i].pIntersectionShader->mD3D12.pShaderBlobs[0]->GetBufferSize();
			intersectionDesc.DXILLibrary.pShaderBytecode =
				pDesc->pHitGroups[i].pIntersectionShader->mD3D12.pShaderBlobs[0]->GetBufferPointer();
			intersectionDesc.NumExports = 1;
			intersectionDesc.pExports = pIntersectionExportDesc;

			arrpush(exportDesc, pIntersectionExportDesc);
			arrpush(dxilLibDescs, intersectionDesc);
		}
		if (pDesc->pHitGroups[i].pAnyHitShader)
		{
			D3D12_EXPORT_DESC* pAnyHitExportDesc = (D3D12_EXPORT_DESC*)tf_calloc(1, sizeof(*pAnyHitExportDesc));
			pAnyHitExportDesc->ExportToRename = NULL;
			pAnyHitExportDesc->Flags = D3D12_EXPORT_FLAG_NONE;
			pAnyHitExportDesc->Name = pDesc->pHitGroups[i].pAnyHitShader->mD3D12.pEntryNames[0];
			hitGroupsAnyHitEntries[i] = pAnyHitExportDesc->Name;

			D3D12_DXIL_LIBRARY_DESC anyHitDesc = {};
			anyHitDesc.DXILLibrary.BytecodeLength = pDesc->pHitGroups[i].pAnyHitShader->mD3D12.pShaderBlobs[0]->GetBufferSize();
			anyHitDesc.DXILLibrary.pShaderBytecode = pDesc->pHitGroups[i].pAnyHitShader->mD3D12.pShaderBlobs[0]->GetBufferPointer();
			anyHitDesc.NumExports = 1;
			anyHitDesc.pExports = pAnyHitExportDesc;

			arrpush(exportDesc, pAnyHitExportDesc);
			arrpush(dxilLibDescs, anyHitDesc);
		}
		if (pDesc->pHitGroups[i].pClosestHitShader)
		{
			D3D12_EXPORT_DESC* pClosestHitExportDesc = (D3D12_EXPORT_DESC*)tf_calloc(1, sizeof(*pClosestHitExportDesc));
			pClosestHitExportDesc->ExportToRename = NULL;
			pClosestHitExportDesc->Flags = D3D12_EXPORT_FLAG_NONE;
			pClosestHitExportDesc->Name = pDesc->pHitGroups[i].pClosestHitShader->mD3D12.pEntryNames[0];
			hitGroupsClosestHitEntries[i] = pClosestHitExportDesc->Name;

			D3D12_DXIL_LIBRARY_DESC closestHitDesc = {};
			closestHitDesc.DXILLibrary.BytecodeLength = pDesc->pHitGroups[i].pClosestHitShader->mD3D12.pShaderBlobs[0]->GetBufferSize();
			closestHitDesc.DXILLibrary.pShaderBytecode = pDesc->pHitGroups[i].pClosestHitShader->mD3D12.pShaderBlobs[0]->GetBufferPointer();
			closestHitDesc.NumExports = 1;
			closestHitDesc.pExports = pClosestHitExportDesc;

			arrpush(exportDesc, pClosestHitExportDesc);
			arrpush(dxilLibDescs, closestHitDesc);
		}
	}

	for (ptrdiff_t i = 0; i < arrlen(dxilLibDescs); ++i)
	{
		tmpSubobject = { D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &dxilLibDescs[i] };
		arrpush(subobjects, tmpSubobject);
	}
	/************************************************************************/
	// Step 2 - Create Hit Groups
	/************************************************************************/
	D3D12_HIT_GROUP_DESC* hitGroupDescs = NULL;
	WCHAR**               hitGroupNames = NULL;

	arrsetlen(hitGroupDescs, pDesc->mHitGroupCount);
	arrsetlen(hitGroupNames, pDesc->mHitGroupCount);
	if (hitGroupDescs)
		memset(hitGroupDescs, 0x0, sizeof(D3D12_HIT_GROUP_DESC) * pDesc->mHitGroupCount);

	for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
	{
		const RaytracingHitGroup* pHitGroup = &pDesc->pHitGroups[i];
		ASSERT(pDesc->pHitGroups[i].pHitGroupName);
		hitGroupNames[i] = (WCHAR*)tf_calloc(strlen(pDesc->pHitGroups[i].pHitGroupName) + 1, sizeof(WCHAR));
		mbstowcs(hitGroupNames[i], pDesc->pHitGroups[i].pHitGroupName, strlen(pDesc->pHitGroups[i].pHitGroupName));

		if (pHitGroup->pAnyHitShader)
		{
			hitGroupDescs[i].AnyHitShaderImport = hitGroupsAnyHitEntries[i];
		}
		else
			hitGroupDescs[i].AnyHitShaderImport = NULL;

		if (pHitGroup->pClosestHitShader)
		{
			hitGroupDescs[i].ClosestHitShaderImport = hitGroupsClosestHitEntries[i];
		}
		else
			hitGroupDescs[i].ClosestHitShaderImport = NULL;

		if (pHitGroup->pIntersectionShader)
		{
			hitGroupDescs[i].IntersectionShaderImport = hitGroupsIntersectionsEntries[i];
		}
		else
			hitGroupDescs[i].IntersectionShaderImport = NULL;

		hitGroupDescs[i].HitGroupExport = hitGroupNames[i];

		tmpSubobject = { D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitGroupDescs[i] };
		arrpush(subobjects, tmpSubobject);
	}
	/************************************************************************/
	// Step 4 = Pipeline Config
	/************************************************************************/
	D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
	pipelineConfig.MaxTraceRecursionDepth = pDesc->mMaxTraceRecursionDepth;
	tmpSubobject = { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipelineConfig };
	arrpush(subobjects, tmpSubobject);
	/************************************************************************/
	// Step 5 - Global Root Signature
	/************************************************************************/
	tmpSubobject = { D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE,
					 pDesc->pGlobalRootSignature ? &pDesc->pGlobalRootSignature->mD3D12.pDxRootSignature : NULL };
	arrpush(subobjects, tmpSubobject);
	/************************************************************************/
	// Step 6 - Local Root Signatures
	/************************************************************************/
	// Local Root Signature for Ray Generation Shader
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rayGenRootSignatureAssociation = {};
	D3D12_LOCAL_ROOT_SIGNATURE             rayGenRSdesc = {};
	if (pDesc->pRayGenRootSignature)
	{
		rayGenRSdesc.pLocalRootSignature = pDesc->pRayGenRootSignature->mD3D12.pDxRootSignature;
		tmpSubobject = { D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &rayGenRSdesc };
		arrpush(subobjects, tmpSubobject);

		rayGenRootSignatureAssociation.NumExports = 1;
		rayGenRootSignatureAssociation.pExports = &rayGenExportDesc.Name;

		tmpExportAssociation = { (uint32_t)arrlen(subobjects) - 1, &rayGenRootSignatureAssociation };
		arrpush(exportAssociationsDelayed, tmpExportAssociation);
	}

	// Local Root Signatures for Miss Shaders
	D3D12_STATE_SUBOBJECT*                  missRootSignatures = NULL;
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION* missRootSignaturesAssociation = NULL;
	D3D12_LOCAL_ROOT_SIGNATURE*             mMissShaderRSDescs = NULL;

	arrsetlen(missRootSignatures, pDesc->mMissShaderCount);
	arrsetlen(missRootSignaturesAssociation, pDesc->mMissShaderCount);
	arrsetlen(mMissShaderRSDescs, pDesc->mMissShaderCount);

	for (uint32_t i = 0; i < pDesc->mMissShaderCount; ++i)
	{
		if (pDesc->ppMissRootSignatures && pDesc->ppMissRootSignatures[i])
		{
			missRootSignatures[i].Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
			mMissShaderRSDescs[i].pLocalRootSignature = pDesc->ppMissRootSignatures[i]->mD3D12.pDxRootSignature;
			missRootSignatures[i].pDesc = &mMissShaderRSDescs[i];
			arrpush(subobjects, missRootSignatures[i]);

			missRootSignaturesAssociation[i].NumExports = 1;
			missRootSignaturesAssociation[i].pExports = &missShadersEntries[i];

			tmpExportAssociation = { (uint32_t)arrlen(subobjects) - 1, &missRootSignaturesAssociation[i] };
			arrpush(exportAssociationsDelayed, tmpExportAssociation);
		}
	}

	// Local Root Signatures for Hit Groups
	D3D12_STATE_SUBOBJECT*					hitGroupRootSignatures = NULL;
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*	hitGroupRootSignatureAssociation = NULL;
	D3D12_LOCAL_ROOT_SIGNATURE*				hitGroupRSDescs = NULL;

	arrsetlen(hitGroupRootSignatures, pDesc->mHitGroupCount);
	arrsetlen(hitGroupRootSignatureAssociation, pDesc->mHitGroupCount);
	arrsetlen(hitGroupRSDescs, pDesc->mHitGroupCount);

	for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
	{
		if (pDesc->pHitGroups[i].pRootSignature)
		{
			hitGroupRootSignatures[i].Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
			if (pDesc->pHitGroups[i].pRootSignature)
				hitGroupRSDescs[i].pLocalRootSignature = pDesc->pHitGroups[i].pRootSignature->mD3D12.pDxRootSignature;
			else
				hitGroupRSDescs[i].pLocalRootSignature = pDesc->pEmptyRootSignature->mD3D12.pDxRootSignature;
			hitGroupRootSignatures[i].pDesc = &hitGroupRSDescs[i];
			arrpush(subobjects, hitGroupRootSignatures[i]);

			hitGroupRootSignatureAssociation[i].NumExports = 1;
			hitGroupRootSignatureAssociation[i].pExports = &hitGroupDescs[i].HitGroupExport;

			tmpExportAssociation = { (uint32_t)arrlen(subobjects) - 1, &hitGroupRootSignatureAssociation[i] };
			arrpush(exportAssociationsDelayed, tmpExportAssociation);
		}
	}
	/************************************************************************/
	// Shader Config
	/************************************************************************/
	D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
	shaderConfig.MaxAttributeSizeInBytes = pDesc->mAttributeSize;
	shaderConfig.MaxPayloadSizeInBytes = pDesc->mPayloadSize;
	tmpSubobject = { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderConfig };
	arrpush(subobjects, tmpSubobject);
	/************************************************************************/
	// Export Associations
	/************************************************************************/
	size_t base = arrlenu(subobjects);
	size_t numExportAssocs = arrlenu(exportAssociationsDelayed);
	arrsetlen(subobjects, base + numExportAssocs);
	for (size_t i = 0; i < numExportAssocs; ++i)
	{
		exportAssociationsDelayed[i].pAssociation->pSubobjectToAssociate = &subobjects[exportAssociationsDelayed[i].mIndex];
		//D3D12_STATE_SUBOBJECT exportAssociationLocalRootSignature;
		//exportAssociationLocalRootSignature.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
		//D3D12_LOCAL_ROOT_SIGNATURE desc;
		//desc.pLocalRootSignature = (ID3D12RootSignature *)subobjects[exportAssociationsDelayed[i].first].pDesc;
		//exportAssociationLocalRootSignature.pDesc = &desc;
		//subobjects.push_back(exportAssociationLocalRootSignature);

		subobjects[base + i].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		subobjects[base + i].pDesc = exportAssociationsDelayed[i].pAssociation; //-V595
	}
	/************************************************************************/
	// Step 7 - Create State Object
	/************************************************************************/
	D3D12_STATE_OBJECT_DESC pipelineDesc = {};
	pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	pipelineDesc.NumSubobjects = (UINT)arrlen(subobjects);
	pipelineDesc.pSubobjects = subobjects;

	// Create the state object.
	CHECK_HRESULT(pRaytracing->pDxrDevice->CreateStateObject(&pipelineDesc, IID_ARGS(&pPipeline->mD3D12.pDxrPipeline)));
	/************************************************************************/
	// Clean up
	/************************************************************************/
	for (ptrdiff_t i = 0; i < arrlen(exportDesc); ++i)
		tf_free(exportDesc[i]);

	for (ptrdiff_t i = 0; i < arrlen(hitGroupNames); ++i)
		tf_free(hitGroupNames[i]);
	/************************************************************************/
	/************************************************************************/

	*ppPipeline = pPipeline;
	arrfree(subobjects);
	arrfree(exportAssociationsDelayed);
	arrfree(dxilLibDescs);
	arrfree(exportDesc);
	arrfree(missShadersEntries);
	arrfree(hitGroupsIntersectionsEntries);
	arrfree(hitGroupsAnyHitEntries);
	arrfree(hitGroupsClosestHitEntries);
	arrfree(hitGroupDescs);
	arrfree(hitGroupNames);
	arrfree(missRootSignatures);
	arrfree(missRootSignaturesAssociation);
	arrfree(mMissShaderRSDescs);
	arrfree(hitGroupRootSignatures);
	arrfree(hitGroupRootSignatureAssociation);
	arrfree(hitGroupRSDescs);
}

void fillRaytracingDescriptorHandle(AccelerationStructure* pAccelerationStructure, DxDescriptorID* pOutId)
{
	*pOutId = pAccelerationStructure->pASBuffer->mD3D12.mDescriptors + pAccelerationStructure->pASBuffer->mD3D12.mSrvDescriptorOffset;
}

void cmdBindRaytracingPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
	ASSERT(pPipeline->mD3D12.pDxrPipeline);
	ID3D12GraphicsCommandList4* pDxrCmd = NULL;
	pCmd->mD3D12.pDxCmdList->QueryInterface(IID_ARGS(&pDxrCmd));
	pDxrCmd->SetPipelineState1(pPipeline->mD3D12.pDxrPipeline);
	pDxrCmd->Release();
}
#else
bool d3d12_isRaytracingSupported(Renderer* pRenderer) { return false; }

bool d3d12_initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing) { return false; }

void d3d12_removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing) {}

void d3d12_addAccelerationStructure(
	Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, AccelerationStructure** ppAccelerationStructure)
{
}

void d3d12_cmdBuildTopAS(Cmd* pCmd, Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure) {}

void d3d12_cmdBuildBottomAS(Cmd* pCmd, Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure, unsigned bottomASIndex) {}

void d3d12_cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc) {}

void d3d12_addRaytracingShaderTable(Raytracing* pRaytracing, const RaytracingShaderTableDesc* pDesc, RaytracingShaderTable** ppTable) {}

void d3d12_cmdDispatchRays(Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc) {}

void d3d12_removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure) {}

void d3d12_removeAccelerationStructureScratch(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure) {}

void d3d12_removeRaytracingShaderTable(Raytracing* pRaytracing, RaytracingShaderTable* pTable) {}
#endif

void initD3D12RaytracingFunctions()
{
	isRaytracingSupported = d3d12_isRaytracingSupported;
	initRaytracing = d3d12_initRaytracing;
	removeRaytracing = d3d12_removeRaytracing;
	addAccelerationStructure = d3d12_addAccelerationStructure;
	removeAccelerationStructure = d3d12_removeAccelerationStructure;
	removeAccelerationStructureScratch = d3d12_removeAccelerationStructureScratch;
	addRaytracingShaderTable = d3d12_addRaytracingShaderTable;
	removeRaytracingShaderTable = d3d12_removeRaytracingShaderTable;
	cmdBuildAccelerationStructure = d3d12_cmdBuildAccelerationStructure;
	cmdDispatchRays = d3d12_cmdDispatchRays;
}

#endif
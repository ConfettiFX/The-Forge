/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#if defined(DIRECT3D12)

#include "..\IRenderer.h"
#include "..\..\OS\Interfaces\ILogManager.h"

#ifdef _DURANGO
#include "..\..\..\Xbox\CommonXBOXOne_3\OS\XBoxPrivateHeaders.h"
#else
#define IID_ARGS IID_PPV_ARGS
#endif

#include "Direct3D12Hooks.h"
#include "Direct3D12MemoryAllocator.h"

void d3d12_destroyTexture(ResourceAllocator* allocator, Texture* pTexture)
{
	if (pTexture->pDxResource != RESOURCE_NULL)
	{
		ASSERT(allocator);

		RESOURCE_DEBUG_LOG("resourceAllocDestroyImage");

		RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

		pTexture->pDxResource->Release();

		allocator->FreeMemory(pTexture->pDxAllocation);
	}
}

HRESULT createAllocator(const AllocatorCreateInfo* pCreateInfo, ResourceAllocator** pAllocator)
{
	ASSERT(pCreateInfo && pAllocator);
	RESOURCE_DEBUG_LOG("resourceAllocCreateAllocator");
	*pAllocator = resourceAlloc_new(ResourceAllocator, pCreateInfo);
	return S_OK;
}

void destroyAllocator(ResourceAllocator* allocator)
{
	if (allocator != RESOURCE_NULL)
	{
		RESOURCE_DEBUG_LOG("resourceAllocDestroyAllocator");
		//VkAllocationCallbacks allocationCallbacks = allocator->m_AllocationCallbacks;
		resourceAlloc_delete(allocator);
	}
}

void resourceAllocGetPhysicalDeviceProperties(ResourceAllocator* allocator, const DXGI_ADAPTER_DESC** ppPhysicalDeviceProperties)
{
	ASSERT(allocator && ppPhysicalDeviceProperties);
	*ppPhysicalDeviceProperties = &allocator->m_PhysicalDeviceProperties;
}

void resourceAllocCalculateStats(ResourceAllocator* allocator, AllocatorStats* pStats)
{
	ASSERT(allocator && pStats);
	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK
	allocator->CalculateStats(pStats);
}

//#if RESOURCE_STATS_STRING_ENABLED

static void AllocatorPrintStatInfo(AllocatorStringBuilder& sb, const AllocatorStatInfo& stat)
{
	sb.Add("{ \"Allocations\": ");
	sb.AddNumber(stat.AllocationCount);
	sb.Add(", \"Suballocations\": ");
	sb.AddNumber(stat.SuballocationCount);
	sb.Add(", \"UnusedRanges\": ");
	sb.AddNumber(stat.UnusedRangeCount);
	sb.Add(", \"UsedBytes\": ");
	sb.AddNumber(stat.UsedBytes);
	sb.Add(", \"UnusedBytes\": ");
	sb.AddNumber(stat.UnusedBytes);
	sb.Add(", \"SuballocationSize\": { \"Min\": ");
	sb.AddNumber(stat.SuballocationSizeMin);
	sb.Add(", \"Avg\": ");
	sb.AddNumber(stat.SuballocationSizeAvg);
	sb.Add(", \"Max\": ");
	sb.AddNumber(stat.SuballocationSizeMax);
	sb.Add(" }, \"UnusedRangeSize\": { \"Min\": ");
	sb.AddNumber(stat.UnusedRangeSizeMin);
	sb.Add(", \"Avg\": ");
	sb.AddNumber(stat.UnusedRangeSizeAvg);
	sb.Add(", \"Max\": ");
	sb.AddNumber(stat.UnusedRangeSizeMax);
	sb.Add(" } }");
}

//#endif // #if RESOURCE_STATS_STRING_ENABLED

//#if RESOURCE_STATS_STRING_ENABLED

void resourceAllocBuildStatsString(ResourceAllocator* allocator, char** ppStatsString, uint32_t detailedMap)
{
	ASSERT(allocator && ppStatsString);
	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	AllocatorStringBuilder sb(allocator);
	{
		AllocatorStats stats;
		allocator->CalculateStats(&stats);

		sb.Add("{\n\"Total\": ");
		AllocatorPrintStatInfo(sb, stats.total);

		for (uint32_t heapIndex = 0; heapIndex < allocator->GetMemoryHeapCount(); ++heapIndex)
		{
			sb.Add(",\n\"Heap ");
			sb.AddNumber(heapIndex);
			sb.Add("\": {\n\t\"Size\": ");
			//sb.AddNumber(allocator->m_MemProps.memoryHeaps[heapIndex].size);
			sb.Add(",\n\t\"Flags\": ");
			if (gHeapProperties[heapIndex].mProps.Type == D3D12_HEAP_TYPE_DEFAULT)
			{
				sb.AddString("DEVICE_LOCAL");
			}
			else
			{
				sb.AddString("");
			}
			if (stats.memoryHeap[heapIndex].AllocationCount > 0)
			{
				sb.Add(",\n\t\"Stats:\": ");
				AllocatorPrintStatInfo(sb, stats.memoryHeap[heapIndex]);
			}

			for (uint32_t typeIndex = 0; typeIndex < allocator->GetMemoryTypeCount(); ++typeIndex)
			{
				//if (allocator->m_MemProps.memoryTypes[typeIndex].heapIndex == heapIndex)
				{
					sb.Add(",\n\t\"Type ");
					sb.AddNumber(typeIndex);
					sb.Add("\": {\n\t\t\"Flags\": \"");
					if (gHeapProperties[typeIndex].mProps.Type == D3D12_HEAP_TYPE_DEFAULT)
					{
						sb.Add(" DEVICE_LOCAL");
					}
					if (gHeapProperties[typeIndex].mProps.Type == D3D12_HEAP_TYPE_UPLOAD)
					{
						sb.Add(" HOST_VISIBLE");
					}
					if (gHeapProperties[typeIndex].mProps.Type == D3D12_HEAP_TYPE_READBACK)
					{
						sb.Add(" HOST_COHERENT");
					}
					sb.Add("\"");
					if (stats.memoryType[typeIndex].AllocationCount > 0)
					{
						sb.Add(",\n\t\t\"Stats\": ");
						AllocatorPrintStatInfo(sb, stats.memoryType[typeIndex]);
					}
					sb.Add("\n\t}");
				}
			}
			sb.Add("\n}");
		}
		if (detailedMap == 1)
		{
			allocator->PrintDetailedMap(sb);
		}
		sb.Add("\n}\n");
	}

	const size_t len = sb.GetLength();
	char* const  pChars = resourceAlloc_new_array(char, len + 1);
	if (len > 0)
	{
		memcpy(pChars, sb.GetData(), len);
	}
	pChars[len] = '\0';
	*ppStatsString = pChars;
}

void resourceAllocFreeStatsString(ResourceAllocator* allocator, char* pStatsString)
{
	if (pStatsString != RESOURCE_NULL)
	{
		ASSERT(allocator);
		size_t len = strlen(pStatsString);
		resourceAlloc_delete_array(pStatsString, len + 1);
	}
}

//#endif // #if RESOURCE_STATS_STRING_ENABLED

/** This function is not protected by any mutex because it just reads immutable data.
*/
HRESULT resourceAllocFindMemoryTypeIndex(
	ResourceAllocator* allocator, const D3D12_RESOURCE_ALLOCATION_INFO* pAllocInfo, const AllocatorMemoryRequirements* pMemoryRequirements,
	const AllocatorSuballocationType pSuballocType, uint32_t* pMemoryTypeIndex)
{
	ASSERT(allocator != RESOURCE_NULL);
	ASSERT(pMemoryRequirements != RESOURCE_NULL);
	ASSERT(pMemoryTypeIndex != RESOURCE_NULL);

	*pMemoryTypeIndex = UINT32_MAX;

	switch (pSuballocType)
	{
		case RESOURCE_SUBALLOCATION_TYPE_BUFFER:
			if (pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_DEFAULT_BUFFER;
			else if (
				pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_CPU_ONLY ||
				pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_UPLOAD_BUFFER;
			else if (pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_READBACK_BUFFER;
			break;
		case RESOURCE_SUBALLOCATION_TYPE_IMAGE_OPTIMAL:
			if (pAllocInfo->Alignment == D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_SMALL;
			if (pAllocInfo->Alignment == D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_DEFAULT;
			if (pAllocInfo->Alignment == D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_MS;
			break;
		case RESOURCE_SUBALLOCATION_TYPE_IMAGE_RTV_DSV:
			if (pAllocInfo->Alignment == D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT)
			{
				if (pMemoryRequirements->flags & RESOURCE_MEMORY_REQUIREMENT_SHARED_ADAPTER_BIT)
					*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_SHARED_ADAPTER_MS;
				else if (pMemoryRequirements->flags & RESOURCE_MEMORY_REQUIREMENT_SHARED_BIT)
					*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_SHARED_MS;
				else
					*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_MS;
			}
			else
			{
				if (pMemoryRequirements->flags & RESOURCE_MEMORY_REQUIREMENT_SHARED_ADAPTER_BIT)
					*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_SHARED_ADAPTER;
				else if (pMemoryRequirements->flags & RESOURCE_MEMORY_REQUIREMENT_SHARED_BIT)
					*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV_SHARED;
				else
					*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_TEXTURE_RTV_DSV;
			}
			break;
		case RESOURCE_SUBALLOCATION_TYPE_BUFFER_SRV_UAV:
			if (pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_DEFAULT_UAV;
			else if (
				pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_CPU_ONLY ||
				pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_UPLOAD_UAV;
			else if (pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
				*pMemoryTypeIndex = RESOURCE_MEMORY_TYPE_READBACK_UAV;
			break;
		default: break;
	}

	return (*pMemoryTypeIndex != UINT32_MAX) ? S_OK : E_NOTIMPL;
}

HRESULT resourceAllocAllocateMemory(
	ResourceAllocator* allocator, const D3D12_RESOURCE_ALLOCATION_INFO* pVkMemoryRequirements,
	const AllocatorMemoryRequirements* pAllocatorMemoryRequirements, ResourceAllocation** pAllocation,
	ResourceAllocationInfo* pAllocationInfo)
{
	ASSERT(allocator && pVkMemoryRequirements && pAllocatorMemoryRequirements && pAllocation);

	RESOURCE_DEBUG_LOG("resourceAllocAllocateMemory");

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	return allocator->AllocateMemory(
		*pVkMemoryRequirements, *pAllocatorMemoryRequirements, RESOURCE_SUBALLOCATION_TYPE_UNKNOWN, pAllocation);

	if (pAllocationInfo)
	{
		allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
	}
}

HRESULT resourceAllocAllocateMemoryForBuffer(
	ResourceAllocator* allocator, D3D12_RESOURCE_DESC* buffer, const AllocatorMemoryRequirements* pMemoryRequirements,
	ResourceAllocation** pAllocation, ResourceAllocationInfo* pAllocationInfo)
{
	ASSERT(allocator && buffer != RESOURCE_NULL && pMemoryRequirements && pAllocation);

	RESOURCE_DEBUG_LOG("resourceAllocAllocateMemoryForBuffer");

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	D3D12_RESOURCE_ALLOCATION_INFO vkMemReq = allocator->m_hDevice->GetResourceAllocationInfo(0, 1, buffer);

	return allocator->AllocateMemory(vkMemReq, *pMemoryRequirements, RESOURCE_SUBALLOCATION_TYPE_BUFFER, pAllocation);

	if (pAllocationInfo)
	{
		allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
	}
}

HRESULT resourceAllocMapMemory(ResourceAllocator* allocator, ResourceAllocation* allocation, void** ppData)
{
	ASSERT(allocator && allocation && ppData);

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	if (allocation->GetResource())
		return allocation->GetResource()->Map(0, NULL, ppData);

	return S_FALSE;
}

void resourceAllocUnmapMemory(ResourceAllocator* allocator, ResourceAllocation* allocation)
{
	ASSERT(allocator && allocation);

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	if (allocation->GetResource())
		allocation->GetResource()->Unmap(0, NULL);
}

void resourceAllocUnmapPersistentlyMappedMemory(ResourceAllocator* allocator)
{
	ASSERT(allocator);

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	allocator->UnmapPersistentlyMappedMemory();
}

HRESULT resourceAllocMapPersistentlyMappedMemory(ResourceAllocator* allocator)
{
	ASSERT(allocator);

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	return allocator->MapPersistentlyMappedMemory();
}

HRESULT resourceAllocFindSuballocType(const D3D12_RESOURCE_DESC* pDesc, AllocatorSuballocationType* suballocType)
{
	*suballocType = RESOURCE_SUBALLOCATION_TYPE_UNKNOWN;
	if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET || pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
		*suballocType = RESOURCE_SUBALLOCATION_TYPE_IMAGE_RTV_DSV;
	else
		*suballocType = RESOURCE_SUBALLOCATION_TYPE_IMAGE_OPTIMAL;

	return (*suballocType != RESOURCE_SUBALLOCATION_TYPE_UNKNOWN) ? S_OK : S_FALSE;
}

HRESULT resourceAllocAllocateMemoryForImage(
	ResourceAllocator* allocator, D3D12_RESOURCE_DESC* image, D3D12_RESOURCE_STATES states,
	const AllocatorMemoryRequirements* pMemoryRequirements, ResourceAllocation** pAllocation, ResourceAllocationInfo* pAllocationInfo)
{
	UNREF_PARAM(states);
	ASSERT(allocator && image != RESOURCE_NULL && pMemoryRequirements && pAllocation);

	RESOURCE_DEBUG_LOG("resourceAllocAllocateMemoryForImage");

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	AllocatorSuballocationType suballocType;
	if (!SUCCEEDED(resourceAllocFindSuballocType(image, &suballocType)))
		return S_FALSE;

	return AllocateMemoryForImage(allocator, image, pMemoryRequirements, suballocType, pAllocation);

	if (pAllocationInfo)
	{
		allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
	}
}

void resourceAllocFreeMemory(ResourceAllocator* allocator, ResourceAllocation* allocation)
{
	ASSERT(allocator && allocation);

	RESOURCE_DEBUG_LOG("resourceAllocFreeMemory");

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	allocator->FreeMemory(allocation);
}

void resourceAllocGetAllocationInfo(ResourceAllocator* allocator, ResourceAllocation* allocation, ResourceAllocationInfo* pAllocationInfo)
{
	ASSERT(allocator && allocation && pAllocationInfo);

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	allocator->GetAllocationInfo(allocation, pAllocationInfo);
}

void resourceAllocSetAllocationUserData(ResourceAllocator* allocator, ResourceAllocation* allocation, void* pUserData)
{
	ASSERT(allocator && allocation);

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	allocation->SetUserData(pUserData);
}

long d3d12_createBuffer(
	ResourceAllocator* allocator, const BufferCreateInfo* pCreateInfo, const AllocatorMemoryRequirements* pMemoryRequirements,
	Buffer* pBuffer)
{
	ASSERT(allocator && pCreateInfo && pMemoryRequirements && pBuffer);

	RESOURCE_DEBUG_LOG("resourceAllocCreateBuffer");

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	AllocatorSuballocationType suballocType = RESOURCE_SUBALLOCATION_TYPE_BUFFER;

	// For GPU buffers, use special memory type
	// For CPU mapped UAV / SRV buffers, just use suballocation strategy
	if (((pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER) || (pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_BUFFER)) &&
		pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
		suballocType = RESOURCE_SUBALLOCATION_TYPE_BUFFER_SRV_UAV;

	// 2. vkGetBufferMemoryRequirements.
	D3D12_RESOURCE_ALLOCATION_INFO vkMemReq = allocator->m_hDevice->GetResourceAllocationInfo(0, 1, pCreateInfo->pDesc);
	if (suballocType == RESOURCE_SUBALLOCATION_TYPE_BUFFER)
		vkMemReq.Alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
#ifdef _DURANGO
	else if (suballocType == RESOURCE_SUBALLOCATION_TYPE_BUFFER_SRV_UAV)
	{
		vkMemReq.Alignment =
			1024 * 64;    // XBox returns an error with the alignment calculated.Using 64k as requested by the console in the error message.
	}
#endif

	vkMemReq.SizeInBytes = pCreateInfo->pDesc->Width * pCreateInfo->pDesc->Height;

	// 3. Allocate memory using allocator.
	HRESULT res = allocator->AllocateMemory(vkMemReq, *pMemoryRequirements, suballocType, &pBuffer->pDxAllocation);

	if (SUCCEEDED(res))
	{
		if (pBuffer->pDxAllocation->GetType() == ResourceAllocation::ALLOCATION_TYPE_BLOCK)
		{
			if (suballocType == RESOURCE_SUBALLOCATION_TYPE_BUFFER)
			{
				pBuffer->pDxResource = pBuffer->pDxAllocation->GetResource();
			}
			else
			{
				if (fnHookSpecialBufferAllocation != NULL &&
					fnHookSpecialBufferAllocation(allocator->pRenderer, pBuffer, pCreateInfo, allocator))
				{
					LOGINFOF("Allocated memory in special platform-specific buffer");
				}
				else
				{
					res = allocator->m_hDevice->CreatePlacedResource(
						pBuffer->pDxAllocation->GetMemory(), pBuffer->pDxAllocation->GetOffset(), pCreateInfo->pDesc,
						pCreateInfo->mStartState, NULL, IID_ARGS(&pBuffer->pDxResource));
					pBuffer->pDxResource->SetName(pCreateInfo->pDebugName ? pCreateInfo->pDebugName : L"PLACED BUFFER RESOURCE");
				}
				if (pMemoryRequirements->flags & RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT)
				{
					if (pMemoryRequirements->usage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
					{
						LOGWARNINGF(
							"Cannot map memory not visible on CPU. Use a readback buffer instead for reading the memory to a cpu visible "
							"buffer");
					}
					else
					{
						pBuffer->pDxResource->Map(0, NULL, &pBuffer->pDxAllocation->GetBlock()->m_pMappedData);
					}
				}
			}
		}
		else
		{
			// If buffer is a UAV to be used in CPU mapped memory use write combine memory with a custom heap
			if (pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU &&
				(pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_RW_BUFFER))
			{
				D3D12_HEAP_PROPERTIES heapProps = gHeapProperties[pBuffer->pDxAllocation->GetMemoryTypeIndex()].mProps;
				heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
				heapProps.Type = D3D12_HEAP_TYPE_CUSTOM;
				heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
				heapProps.CreationNodeMask = (1 << pBuffer->mDesc.mNodeIndex);
				heapProps.VisibleNodeMask = heapProps.CreationNodeMask;
				for (uint32_t i = 0; i < pBuffer->mDesc.mSharedNodeIndexCount; ++i)
					heapProps.VisibleNodeMask |= (1 << pBuffer->mDesc.pSharedNodeIndices[i]);

				res = allocator->m_hDevice->CreateCommittedResource(
					&heapProps, D3D12_HEAP_FLAG_NONE, pCreateInfo->pDesc, pCreateInfo->mStartState, NULL, IID_ARGS(&pBuffer->pDxResource));
			}
			else
			{
				D3D12_HEAP_PROPERTIES heapProps = gHeapProperties[pBuffer->pDxAllocation->GetMemoryTypeIndex()].mProps;
				heapProps.CreationNodeMask = (1 << pBuffer->mDesc.mNodeIndex);
				heapProps.VisibleNodeMask = heapProps.CreationNodeMask;
				for (uint32_t i = 0; i < pBuffer->mDesc.mSharedNodeIndexCount; ++i)
					heapProps.VisibleNodeMask |= (1 << pBuffer->mDesc.pSharedNodeIndices[i]);

				res = allocator->m_hDevice->CreateCommittedResource(
					&heapProps, D3D12_HEAP_FLAG_NONE, pCreateInfo->pDesc, pCreateInfo->mStartState, NULL, IID_ARGS(&pBuffer->pDxResource));
			}
			pBuffer->pDxResource->SetName(pCreateInfo->pDebugName ? pCreateInfo->pDebugName : L"OWN BUFFER RESOURCE");

			if (pMemoryRequirements->flags & RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT &&
				pMemoryRequirements->usage != RESOURCE_MEMORY_USAGE_GPU_ONLY)
			{
				pBuffer->pDxResource->Map(0, NULL, &pBuffer->pDxAllocation->GetOwnAllocation()->m_pMappedData);
			}
		}

		// 3. Bind buffer with memory.
		if (pBuffer->pDxResource)
		{
			// All steps succeeded.
			ResourceAllocationInfo allocInco = {};
			allocator->GetAllocationInfo(pBuffer->pDxAllocation, &allocInco);
			pBuffer->pCpuMappedAddress = allocInco.pMappedData;
			return S_OK;
		}

		allocator->FreeMemory(pBuffer->pDxAllocation);
		return res;
	}

	return res;
}

void d3d12_destroyBuffer(ResourceAllocator* allocator, Buffer* pBuffer)
{
	if (pBuffer->pDxResource != NULL)
	{
		ASSERT(allocator);
		RESOURCE_DEBUG_LOG("resourceAllocDestroyBuffer");

		RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

		if (!pBuffer->pDxAllocation->GetResource())
			pBuffer->pDxResource->Release();

		allocator->FreeMemory(pBuffer->pDxAllocation);
	}
}

long d3d12_createTexture(
	ResourceAllocator* allocator, const TextureCreateInfo* pCreateInfo, const AllocatorMemoryRequirements* pMemoryRequirements,
	Texture* pTexture)
{
	ASSERT(allocator && pCreateInfo->pDesc && pMemoryRequirements && pTexture);

	RESOURCE_DEBUG_LOG("resourceAllocCreateImage");

	RESOURCE_DEBUG_GLOBAL_MUTEX_LOCK

	AllocatorSuballocationType suballocType;
	if (!SUCCEEDED(resourceAllocFindSuballocType(pCreateInfo->pDesc, &suballocType)))
		return S_FALSE;

	// 2. Allocate memory using allocator.
	HRESULT res = AllocateMemoryForImage(allocator, pCreateInfo->pDesc, pMemoryRequirements, suballocType, &pTexture->pDxAllocation);
	if (SUCCEEDED(res))
	{
		if (pTexture->pDxAllocation->GetType() == ResourceAllocation::ALLOCATION_TYPE_BLOCK)
		{
			if (fnHookSpecialTextureAllocation != NULL &&
				fnHookSpecialTextureAllocation(allocator->pRenderer, pTexture, pCreateInfo, allocator))
			{
				LOGINFOF("Allocated memory in special platform-specific buffer");
			}
			else
			{
				res = allocator->m_hDevice->CreatePlacedResource(
					pTexture->pDxAllocation->GetMemory(), pTexture->pDxAllocation->GetOffset(), pCreateInfo->pDesc,
					pCreateInfo->mStartState, pCreateInfo->pClearValue, IID_ARGS(&pTexture->pDxResource));
				pTexture->pDxResource->SetName(pCreateInfo->pDebugName ? pCreateInfo->pDebugName : L"PLACED TEXTURE RESOURCE");
			}
		}
		else
		{
			D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
			if (pMemoryRequirements->flags & RESOURCE_MEMORY_REQUIREMENT_SHARED_ADAPTER_BIT)
				heapFlags |= (D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER);
			else if (pMemoryRequirements->flags & RESOURCE_MEMORY_REQUIREMENT_SHARED_BIT)
				heapFlags |= D3D12_HEAP_FLAG_SHARED;

			D3D12_HEAP_PROPERTIES heapProps = gHeapProperties[pTexture->pDxAllocation->GetMemoryTypeIndex()].mProps;
			heapProps.CreationNodeMask = (1 << pCreateInfo->pTextureDesc->mNodeIndex);
			heapProps.VisibleNodeMask = heapProps.CreationNodeMask;
			for (uint32_t i = 0; i < pCreateInfo->pTextureDesc->mSharedNodeIndexCount; ++i)
				heapProps.VisibleNodeMask |= (1 << pCreateInfo->pTextureDesc->pSharedNodeIndices[i]);

			res = allocator->m_hDevice->CreateCommittedResource(
				&heapProps, heapFlags, pCreateInfo->pDesc, pCreateInfo->mStartState, pCreateInfo->pClearValue,
				IID_ARGS(&pTexture->pDxResource));

			pTexture->pDxResource->SetName(pCreateInfo->pDebugName ? pCreateInfo->pDebugName : L"OWN TEXTURE RESOURCE");
		}

		if (SUCCEEDED(res))
		{
			// Store allocation info in the texture if there is a need in the future
			//// All steps succeeded.
			//if (pAllocationInfo != RESOURCE_NULL)
			//{
			//  allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
			//}
			return res;
		}
		pTexture->pDxResource->Release();
		allocator->FreeMemory(pTexture->pDxAllocation);
		return res;
	}
	return res;
}

ID3D12Heap* ResourceAllocation::GetMemory() const
{
	return (m_Type == ALLOCATION_TYPE_BLOCK) ? m_BlockAllocation.m_Block->m_hMemory : NULL;
}

ID3D12Resource* ResourceAllocation::GetResource() const
{
	if (m_Type == ALLOCATION_TYPE_OWN)
	{
		return NULL;
	}
	return (m_SuballocationType == RESOURCE_SUBALLOCATION_TYPE_BUFFER) ? m_BlockAllocation.m_Block->m_hResource : NULL;
}

uint32_t ResourceAllocation::GetMemoryTypeIndex() const
{
	return (m_Type == ALLOCATION_TYPE_BLOCK) ? m_BlockAllocation.m_Block->m_MemoryTypeIndex : m_OwnAllocation.m_MemoryTypeIndex;
}

RESOURCE_BLOCK_VECTOR_TYPE ResourceAllocation::GetBlockVectorType() const
{
	return (m_Type == ALLOCATION_TYPE_BLOCK)
			   ? m_BlockAllocation.m_Block->m_BlockVectorType
			   : (m_OwnAllocation.m_PersistentMap ? RESOURCE_BLOCK_VECTOR_TYPE_MAPPED : RESOURCE_BLOCK_VECTOR_TYPE_UNMAPPED);
}

void* ResourceAllocation::GetMappedData() const
{
	switch (m_Type)
	{
		case ALLOCATION_TYPE_BLOCK:
			if (m_BlockAllocation.m_Block->m_pMappedData != RESOURCE_NULL)
			{
				return (char*)m_BlockAllocation.m_Block->m_pMappedData + m_BlockAllocation.m_Offset;
			}
			else
			{
				return RESOURCE_NULL;
			}
			break;
		case ALLOCATION_TYPE_OWN: return m_OwnAllocation.m_pMappedData;
		default: ASSERT(0); return RESOURCE_NULL;
	}
}

struct AllocatorSuballocationItemSizeLess
{
	bool operator()(const AllocatorSuballocationList::iterator lhs, const AllocatorSuballocationList::iterator rhs) const
	{
		return lhs->size < rhs->size;
	}
	bool operator()(const AllocatorSuballocationList::iterator lhs, UINT64 rhsSize) const { return lhs->size < rhsSize; }
};

AllocatorBlock::AllocatorBlock(ResourceAllocator* hAllocator):
	m_MemoryTypeIndex(UINT32_MAX),
	m_BlockVectorType(RESOURCE_BLOCK_VECTOR_TYPE_COUNT),
	m_hMemory(NULL),
	m_Size(0),
	m_PersistentMap(false),
	m_pMappedData(RESOURCE_NULL),
	m_FreeCount(0),
	m_SumFreeSize(0)
//m_Suballocations (AllocatorStlAllocator<AllocatorSuballocation> (hAllocator->GetAllocationCallbacks ()))
//m_FreeSuballocationsBySize (AllocatorStlAllocator<AllocatorSuballocationList::iterator> (hAllocator->GetAllocationCallbacks ()))
{
	UNREF_PARAM(hAllocator);
}

void AllocatorBlock::Init(
	uint32_t newMemoryTypeIndex, RESOURCE_BLOCK_VECTOR_TYPE newBlockVectorType, ID3D12Heap* newMemory, UINT64 newSize, bool persistentMap,
	void* pMappedData)
{
	ASSERT(m_hMemory == NULL);

	m_MemoryTypeIndex = newMemoryTypeIndex;
	m_BlockVectorType = newBlockVectorType;
	m_hMemory = newMemory;
	m_Size = newSize;
	m_PersistentMap = persistentMap;
	m_pMappedData = pMappedData;
	m_FreeCount = 1;
	m_SumFreeSize = newSize;

	m_Suballocations.clear();
	m_FreeSuballocationsBySize.clear();

	AllocatorSuballocation suballoc = {};
	suballoc.offset = 0;
	suballoc.size = newSize;
	suballoc.type = RESOURCE_SUBALLOCATION_TYPE_FREE;

	m_Suballocations.push_back(suballoc);
	AllocatorSuballocationList::iterator suballocItem = m_Suballocations.end();
	--suballocItem;
	m_FreeSuballocationsBySize.push_back(suballocItem);
}

void AllocatorBlock::Init(
	uint32_t newMemoryTypeIndex, RESOURCE_BLOCK_VECTOR_TYPE newBlockVectorType, ID3D12Resource* newMemory, UINT64 newSize,
	bool persistentMap, void* pMappedData)
{
	ASSERT(m_hMemory == NULL);

	m_MemoryTypeIndex = newMemoryTypeIndex;
	m_BlockVectorType = newBlockVectorType;
	m_hResource = newMemory;
	m_Size = newSize;
	m_PersistentMap = persistentMap;
	m_pMappedData = pMappedData;
	m_FreeCount = 1;
	m_SumFreeSize = newSize;

	m_Suballocations.clear();
	m_FreeSuballocationsBySize.clear();

	AllocatorSuballocation suballoc = {};
	suballoc.offset = 0;
	suballoc.size = newSize;
	suballoc.type = RESOURCE_SUBALLOCATION_TYPE_FREE;

	m_Suballocations.push_back(suballoc);
	AllocatorSuballocationList::iterator suballocItem = m_Suballocations.end();
	--suballocItem;
	m_FreeSuballocationsBySize.push_back(suballocItem);
}

void AllocatorBlock::Destroy(ResourceAllocator* allocator)
{
	UNREF_PARAM(allocator);
	ASSERT(m_hResource != NULL || m_hMemory != NULL);
	if (m_pMappedData != RESOURCE_NULL)
	{
		//vkUnmapMemory(allocator->m_hDevice, m_hMemory);
		m_pMappedData = RESOURCE_NULL;
	}

	//// Callback.
	//if (allocator->m_DeviceMemoryCallbacks.pfnFree != RESOURCE_NULL)
	//{
	//  (*allocator->m_DeviceMemoryCallbacks.pfnFree)(allocator, m_MemoryTypeIndex, m_hMemory, m_Size);
	//}

	if (m_hMemory)
		m_hMemory->Release();
	else
		m_hResource->Release();
	m_hMemory = NULL;
}

bool AllocatorBlock::Validate() const
{
	if ((m_hMemory == NULL) || (m_Size == 0) || m_Suballocations.empty())
	{
		return false;
	}

	// Expected offset of new suballocation as calculates from previous ones.
	UINT64 calculatedOffset = 0;
	// Expected number of free suballocations as calculated from traversing their list.
	uint32_t calculatedFreeCount = 0;
	// Expected sum size of free suballocations as calculated from traversing their list.
	UINT64 calculatedSumFreeSize = 0;
	// Expected number of free suballocations that should be registered in
	// m_FreeSuballocationsBySize calculated from traversing their list.
	size_t freeSuballocationsToRegister = 0;
	// True if previous visisted suballocation was free.
	bool prevFree = false;

	for (AllocatorSuballocationList::const_iterator suballocItem = m_Suballocations.cbegin(); suballocItem != m_Suballocations.cend();
		 ++suballocItem)
	{
		const AllocatorSuballocation& subAlloc = *suballocItem;

		// Actual offset of this suballocation doesn't match expected one.
		if (subAlloc.offset != calculatedOffset)
		{
			return false;
		}

		const bool currFree = (subAlloc.type == RESOURCE_SUBALLOCATION_TYPE_FREE);
		// Two adjacent free suballocations are invalid. They should be merged.
		if (prevFree && currFree)
		{
			return false;
		}
		prevFree = currFree;

		if (currFree)
		{
			calculatedSumFreeSize += subAlloc.size;
			++calculatedFreeCount;
			if (subAlloc.size >= RESOURCE_MIN_FREE_SUBALLOCATION_SIZE_TO_REGISTER)
			{
				++freeSuballocationsToRegister;
			}
		}

		calculatedOffset += subAlloc.size;
	}

	// Number of free suballocations registered in m_FreeSuballocationsBySize doesn't
	// match expected one.
	if (m_FreeSuballocationsBySize.size() != freeSuballocationsToRegister)
	{
		return false;
	}

	UINT64 lastSize = 0;
	for (size_t i = 0; i < m_FreeSuballocationsBySize.size(); ++i)
	{
		AllocatorSuballocationList::iterator suballocItem = m_FreeSuballocationsBySize[i];

		// Only free suballocations can be registered in m_FreeSuballocationsBySize.
		if (suballocItem->type != RESOURCE_SUBALLOCATION_TYPE_FREE)
		{
			return false;
		}
		// They must be sorted by size ascending.
		if (suballocItem->size < lastSize)
		{
			return false;
		}

		lastSize = suballocItem->size;
	}

	// Check if totals match calculacted values.
	return (calculatedOffset == m_Size) && (calculatedSumFreeSize == m_SumFreeSize) && (calculatedFreeCount == m_FreeCount);
}

/*
How many suitable free suballocations to analyze before choosing best one.
- Set to 1 to use First-Fit algorithm - first suitable free suballocation will
be chosen.
- Set to UINT32_MAX to use Best-Fit/Worst-Fit algorithm - all suitable free
suballocations will be analized and best one will be chosen.
- Any other value is also acceptable.
*/
//static const uint32_t MAX_SUITABLE_SUBALLOCATIONS_TO_CHECK = 8;

bool AllocatorBlock::CreateAllocationRequest(
	UINT64 bufferImageGranularity, UINT64 allocSize, UINT64 allocAlignment, AllocatorSuballocationType allocType,
	AllocatorAllocationRequest* pAllocationRequest)
{
	ASSERT(allocSize > 0);
	ASSERT(allocType != RESOURCE_SUBALLOCATION_TYPE_FREE);
	ASSERT(pAllocationRequest != RESOURCE_NULL);
	RESOURCE_HEAVY_ASSERT(Validate());

	// There is not enough total free space in this allocation to fullfill the request: Early return.
	if (m_SumFreeSize < allocSize)
	{
		return false;
	}

	// Old brute-force algorithm, linearly searching suballocations.
	/*
	uint32_t suitableSuballocationsFound = 0;
	for(AllocatorSuballocationList::iterator suballocItem = suballocations.Front();
	suballocItem != RESOURCE_NULL &&
	suitableSuballocationsFound < MAX_SUITABLE_SUBALLOCATIONS_TO_CHECK;
	suballocItem = suballocItem->Next)
	{
	if(suballocItem->Value.type == RESOURCE_SUBALLOCATION_TYPE_FREE)
	{
	VkDeviceSize offset = 0, cost = 0;
	if(CheckAllocation(bufferImageGranularity, allocSize, allocAlignment, allocType, suballocItem, &offset, &cost))
	{
	++suitableSuballocationsFound;
	if(cost < costLimit)
	{
	pAllocationRequest->freeSuballocationItem = suballocItem;
	pAllocationRequest->offset = offset;
	pAllocationRequest->cost = cost;
	if(cost == 0)
	return true;
	costLimit = cost;
	betterSuballocationFound = true;
	}
	}
	}
	}
	*/

	// New algorithm, efficiently searching freeSuballocationsBySize.
	const size_t freeSuballocCount = m_FreeSuballocationsBySize.size();
	if (freeSuballocCount > 0)
	{
		if (RESOURCE_BEST_FIT)
		{
			// Find first free suballocation with size not less than allocSize.
			AllocatorSuballocationList::iterator* const it = AllocatorBinaryFindFirstNotLess(
				m_FreeSuballocationsBySize.data(), m_FreeSuballocationsBySize.data() + freeSuballocCount, allocSize,
				AllocatorSuballocationItemSizeLess());
			size_t index = it - m_FreeSuballocationsBySize.data();
			for (; index < freeSuballocCount; ++index)
			{
				UINT64                                     offset = 0;
				const AllocatorSuballocationList::iterator suballocItem = m_FreeSuballocationsBySize[index];
				if (CheckAllocation(bufferImageGranularity, allocSize, allocAlignment, allocType, suballocItem, &offset))
				{
					pAllocationRequest->freeSuballocationItem = suballocItem;
					pAllocationRequest->offset = offset;
					return true;
				}
			}
		}
		else
		{
			// Search staring from biggest suballocations.
			for (size_t index = freeSuballocCount; index--;)
			{
				UINT64                                     offset = 0;
				const AllocatorSuballocationList::iterator suballocItem = m_FreeSuballocationsBySize[index];
				if (CheckAllocation(bufferImageGranularity, allocSize, allocAlignment, allocType, suballocItem, &offset))
				{
					pAllocationRequest->freeSuballocationItem = suballocItem;
					pAllocationRequest->offset = offset;
					return true;
				}
			}
		}
	}

	return false;
}

bool AllocatorBlock::CheckAllocation(
	UINT64 bufferImageGranularity, UINT64 allocSize, UINT64 allocAlignment, AllocatorSuballocationType allocType,
	AllocatorSuballocationList::const_iterator freeSuballocItem, UINT64* pOffset) const
{
	ASSERT(allocSize > 0);
	ASSERT(allocType != RESOURCE_SUBALLOCATION_TYPE_FREE);
	ASSERT(freeSuballocItem != m_Suballocations.cend());
	ASSERT(pOffset != RESOURCE_NULL);

	const AllocatorSuballocation& suballoc = *freeSuballocItem;
	ASSERT(suballoc.type == RESOURCE_SUBALLOCATION_TYPE_FREE);

	// Size of this suballocation is too small for this request: Early return.
	if (suballoc.size < allocSize)
	{
		return false;
	}

	// Start from offset equal to beginning of this suballocation.
	*pOffset = suballoc.offset;

	// Apply RESOURCE_DEBUG_MARGIN at the beginning.
#if RESOURCE_DEBUG_MARGIN > 0
	if (freeSuballocItem != m_Suballocations.cbegin())
	{
		*pOffset += RESOURCE_DEBUG_MARGIN;
	}
#endif

	// Apply alignment.
	const UINT64 alignment = RESOURCE_MAX(allocAlignment, static_cast<UINT64>(RESOURCE_DEBUG_ALIGNMENT));
	*pOffset = AllocatorAlignUp(*pOffset, alignment);

	// Check previous suballocations for BufferImageGranularity conflicts.
	// Make bigger alignment if necessary.
	if (bufferImageGranularity > 1)
	{
		bool                                       bufferImageGranularityConflict = false;
		AllocatorSuballocationList::const_iterator prevSuballocItem = freeSuballocItem;
		while (prevSuballocItem != m_Suballocations.cbegin())
		{
			--prevSuballocItem;
			const AllocatorSuballocation& prevSuballoc = *prevSuballocItem;
			if (AllocatorBlocksOnSamePage(prevSuballoc.offset, prevSuballoc.size, *pOffset, bufferImageGranularity))
			{
				if (AllocatorIsBufferImageGranularityConflict(prevSuballoc.type, allocType))
				{
					bufferImageGranularityConflict = true;
					break;
				}
			}
			else
				// Already on previous page.
				break;
		}
		if (bufferImageGranularityConflict)
		{
			*pOffset = AllocatorAlignUp(*pOffset, bufferImageGranularity);
		}
	}

	// Calculate padding at the beginning based on current offset.
	const UINT64 paddingBegin = *pOffset - suballoc.offset;

	// Calculate required margin at the end if this is not last suballocation.
	AllocatorSuballocationList::const_iterator next = freeSuballocItem;
	++next;
	const UINT64 requiredEndMargin = (next != m_Suballocations.cend()) ? RESOURCE_DEBUG_MARGIN : 0;

	// Fail if requested size plus margin before and after is bigger than size of this suballocation.
	if (paddingBegin + allocSize + requiredEndMargin > suballoc.size)
	{
		return false;
	}

	// Check next suballocations for BufferImageGranularity conflicts.
	// If conflict exists, allocation cannot be made here.
	if (bufferImageGranularity > 1)
	{
		AllocatorSuballocationList::const_iterator nextSuballocItem = freeSuballocItem;
		++nextSuballocItem;
		while (nextSuballocItem != m_Suballocations.cend())
		{
			const AllocatorSuballocation& nextSuballoc = *nextSuballocItem;
			if (AllocatorBlocksOnSamePage(*pOffset, allocSize, nextSuballoc.offset, bufferImageGranularity))
			{
				if (AllocatorIsBufferImageGranularityConflict(allocType, nextSuballoc.type))
				{
					return false;
				}
			}
			else
			{
				// Already on next page.
				break;
			}
			++nextSuballocItem;
		}
	}

	// All tests passed: Success. pOffset is already filled.
	return true;
}

bool AllocatorBlock::IsEmpty() const { return (m_Suballocations.size() == 1) && (m_FreeCount == 1); }

void AllocatorBlock::Alloc(const AllocatorAllocationRequest& request, AllocatorSuballocationType type, UINT64 allocSize)
{
	ASSERT(request.freeSuballocationItem != m_Suballocations.end());
	AllocatorSuballocation& suballoc = *request.freeSuballocationItem;
	// Given suballocation is a free block.
	ASSERT(suballoc.type == RESOURCE_SUBALLOCATION_TYPE_FREE);
	// Given offset is inside this suballocation.
	ASSERT(request.offset >= suballoc.offset);
	const UINT64 paddingBegin = request.offset - suballoc.offset;
	ASSERT(suballoc.size >= paddingBegin + allocSize);
	const UINT64 paddingEnd = suballoc.size - paddingBegin - allocSize;

	// Unregister this free suballocation from m_FreeSuballocationsBySize and update
	// it to become used.
	UnregisterFreeSuballocation(request.freeSuballocationItem);

	suballoc.offset = request.offset;
	suballoc.size = allocSize;
	suballoc.type = type;

	// If there are any free bytes remaining at the end, insert new free suballocation after current one.
	if (paddingEnd)
	{
		AllocatorSuballocation paddingSuballoc = {};
		paddingSuballoc.offset = request.offset + allocSize;
		paddingSuballoc.size = paddingEnd;
		paddingSuballoc.type = RESOURCE_SUBALLOCATION_TYPE_FREE;
		AllocatorSuballocationList::iterator next = request.freeSuballocationItem;
		++next;
		const AllocatorSuballocationList::iterator paddingEndItem = m_Suballocations.insert(next, paddingSuballoc);
		RegisterFreeSuballocation(paddingEndItem);
	}

	// If there are any free bytes remaining at the beginning, insert new free suballocation before current one.
	if (paddingBegin)
	{
		AllocatorSuballocation paddingSuballoc = {};
		paddingSuballoc.offset = request.offset - paddingBegin;
		paddingSuballoc.size = paddingBegin;
		paddingSuballoc.type = RESOURCE_SUBALLOCATION_TYPE_FREE;
		const AllocatorSuballocationList::iterator paddingBeginItem =
			m_Suballocations.insert(request.freeSuballocationItem, paddingSuballoc);
		RegisterFreeSuballocation(paddingBeginItem);
	}

	// Update totals.
	m_FreeCount = m_FreeCount - 1;
	if (paddingBegin > 0)
	{
		++m_FreeCount;
	}
	if (paddingEnd > 0)
	{
		++m_FreeCount;
	}
	m_SumFreeSize -= allocSize;
}

void AllocatorBlock::FreeSuballocation(AllocatorSuballocationList::iterator suballocItem)
{
	// Change this suballocation to be marked as free.
	AllocatorSuballocation& suballoc = *suballocItem;
	suballoc.type = RESOURCE_SUBALLOCATION_TYPE_FREE;

	// Update totals.
	++m_FreeCount;
	m_SumFreeSize += suballoc.size;

	// Merge with previous and/or next suballocation if it's also free.
	bool mergeWithNext = false;
	bool mergeWithPrev = false;

	AllocatorSuballocationList::iterator nextItem = suballocItem;
	++nextItem;
	if ((nextItem != m_Suballocations.end()) && (nextItem->type == RESOURCE_SUBALLOCATION_TYPE_FREE))
	{
		mergeWithNext = true;
	}

	AllocatorSuballocationList::iterator prevItem = suballocItem;
	if (suballocItem != m_Suballocations.begin())
	{
		--prevItem;
		if (prevItem->type == RESOURCE_SUBALLOCATION_TYPE_FREE)
		{
			mergeWithPrev = true;
		}
	}

	if (mergeWithNext)
	{
		UnregisterFreeSuballocation(nextItem);
		MergeFreeWithNext(suballocItem);
	}

	if (mergeWithPrev)
	{
		UnregisterFreeSuballocation(prevItem);
		MergeFreeWithNext(prevItem);
		RegisterFreeSuballocation(prevItem);
	}
	else
		RegisterFreeSuballocation(suballocItem);
}

void AllocatorBlock::Free(const ResourceAllocation* allocation)
{
	const UINT64 allocationOffset = allocation->GetOffset();
	for (AllocatorSuballocationList::iterator suballocItem = m_Suballocations.begin(); suballocItem != m_Suballocations.end();
		 ++suballocItem)
	{
		AllocatorSuballocation& suballoc = *suballocItem;
		if (suballoc.offset == allocationOffset)
		{
			FreeSuballocation(suballocItem);
			RESOURCE_HEAVY_ASSERT(Validate());
			return;
		}
	}
	ASSERT(0 && "Not found!");
}

//#if RESOURCE_STATS_STRING_ENABLED

void AllocatorBlock::PrintDetailedMap(class AllocatorStringBuilder& sb) const
{
	sb.Add("{\n\t\t\t\"Bytes\": ");
	sb.AddNumber(m_Size);
	sb.Add(",\n\t\t\t\"FreeBytes\": ");
	sb.AddNumber(m_SumFreeSize);
	sb.Add(",\n\t\t\t\"Suballocations\": ");
	sb.AddNumber(m_Suballocations.size());
	sb.Add(",\n\t\t\t\"FreeSuballocations\": ");
	sb.AddNumber(m_FreeCount);
	sb.Add(",\n\t\t\t\"SuballocationList\": [");

	size_t i = 0;
	for (AllocatorSuballocationList::const_iterator suballocItem = m_Suballocations.cbegin(); suballocItem != m_Suballocations.cend();
		 ++suballocItem, ++i)
	{
		if (i > 0)
		{
			sb.Add(",\n\t\t\t\t{ \"Type\": ");
		}
		else
		{
			sb.Add("\n\t\t\t\t{ \"Type\": ");
		}
		sb.AddString(RESOURCE_SUBALLOCATION_TYPE_NAMES[suballocItem->type]);
		sb.Add(", \"Size\": ");
		sb.AddNumber(suballocItem->size);
		sb.Add(", \"Offset\": ");
		sb.AddNumber(suballocItem->offset);
		sb.Add(" }");
	}

	sb.Add("\n\t\t\t]\n\t\t}");
}

//#endif // #if RESOURCE_STATS_STRING_ENABLED

void AllocatorBlock::MergeFreeWithNext(AllocatorSuballocationList::iterator item)
{
	ASSERT(item != m_Suballocations.end());
	ASSERT(item->type == RESOURCE_SUBALLOCATION_TYPE_FREE);

	AllocatorSuballocationList::iterator nextItem = item;
	++nextItem;
	ASSERT(nextItem != m_Suballocations.end());
	ASSERT(nextItem->type == RESOURCE_SUBALLOCATION_TYPE_FREE);

	item->size += nextItem->size;
	--m_FreeCount;
	m_Suballocations.erase(nextItem);
}

void AllocatorBlock::RegisterFreeSuballocation(AllocatorSuballocationList::iterator item)
{
	ASSERT(item->type == RESOURCE_SUBALLOCATION_TYPE_FREE);
	ASSERT(item->size > 0);

	if (item->size >= RESOURCE_MIN_FREE_SUBALLOCATION_SIZE_TO_REGISTER)
	{
		if (m_FreeSuballocationsBySize.empty())
		{
			m_FreeSuballocationsBySize.push_back(item);
		}
		else
		{
			AllocatorSuballocationList::iterator* const it = AllocatorBinaryFindFirstNotLess(
				m_FreeSuballocationsBySize.data(), m_FreeSuballocationsBySize.data() + m_FreeSuballocationsBySize.size(), item,
				AllocatorSuballocationItemSizeLess());
			size_t index = it - m_FreeSuballocationsBySize.data();
			VectorInsert(m_FreeSuballocationsBySize, index, item);
		}
	}
}

void AllocatorBlock::UnregisterFreeSuballocation(AllocatorSuballocationList::iterator item)
{
	ASSERT(item->type == RESOURCE_SUBALLOCATION_TYPE_FREE);
	ASSERT(item->size > 0);

	if (item->size >= RESOURCE_MIN_FREE_SUBALLOCATION_SIZE_TO_REGISTER)
	{
		AllocatorSuballocationList::iterator* const it = AllocatorBinaryFindFirstNotLess(
			m_FreeSuballocationsBySize.data(), m_FreeSuballocationsBySize.data() + m_FreeSuballocationsBySize.size(), item,
			AllocatorSuballocationItemSizeLess());
		for (size_t index = it - m_FreeSuballocationsBySize.data(); index < m_FreeSuballocationsBySize.size(); ++index)
		{
			if (m_FreeSuballocationsBySize[index] == item)
			{
				VectorRemove(m_FreeSuballocationsBySize, index);
				return;
			}
			ASSERT((m_FreeSuballocationsBySize[index]->size == item->size) && "Not found.");
		}
		ASSERT(0 && "Not found.");
	}
}

void AllocatorStringBuilder::Add(const char* pStr)
{
	const size_t strLen = strlen(pStr);
	if (strLen > 0)
	{
		const size_t oldCount = m_Data.size();
		m_Data.resize(oldCount + strLen);
		memcpy(m_Data.data() + oldCount, pStr, strLen);
	}
}

void AllocatorStringBuilder::AddNumber(uint32_t num)
{
	char buf[11];
	AllocatorUint32ToStr(buf, sizeof(buf), num);
	Add(buf);
}

void AllocatorStringBuilder::AddNumber(uint64_t num)
{
	char buf[21];
	AllocatorUint64ToStr(buf, sizeof(buf), num);
	Add(buf);
}

void AllocatorStringBuilder::AddString(const char* pStr)
{
	Add('"');
	const size_t strLen = strlen(pStr);
	for (size_t i = 0; i < strLen; ++i)
	{
		char ch = pStr[i];
		if (ch == '\'')
		{
			Add("\\\\");
		}
		else if (ch == '"')
		{
			Add("\\\"");
		}
		else if (ch >= 32)
		{
			Add(ch);
		}
		else
			switch (ch)
			{
				case '\n': Add("\\n"); break;
				case '\r': Add("\\r"); break;
				case '\t': Add("\\t"); break;
				default: ASSERT(0 && "Character not currently supported."); break;
			}
	}
	Add('"');
}
//#endif

////////////////////////////////////////////////////////////////////////////////

static void InitStatInfo(AllocatorStatInfo& outInfo)
{
	memset(&outInfo, 0, sizeof(outInfo));
	outInfo.SuballocationSizeMin = UINT64_MAX;
	outInfo.UnusedRangeSizeMin = UINT64_MAX;
}

static void CalcAllocationStatInfo(AllocatorStatInfo& outInfo, const AllocatorBlock& alloc)
{
	outInfo.AllocationCount = 1;

	const uint32_t rangeCount = (uint32_t)alloc.m_Suballocations.size();
	outInfo.SuballocationCount = rangeCount - alloc.m_FreeCount;
	outInfo.UnusedRangeCount = alloc.m_FreeCount;

	outInfo.UnusedBytes = alloc.m_SumFreeSize;
	outInfo.UsedBytes = alloc.m_Size - outInfo.UnusedBytes;

	outInfo.SuballocationSizeMin = UINT64_MAX;
	outInfo.SuballocationSizeMax = 0;
	outInfo.UnusedRangeSizeMin = UINT64_MAX;
	outInfo.UnusedRangeSizeMax = 0;

	for (AllocatorSuballocationList::const_iterator suballocItem = alloc.m_Suballocations.cbegin();
		 suballocItem != alloc.m_Suballocations.cend(); ++suballocItem)
	{
		const AllocatorSuballocation& suballoc = *suballocItem;
		if (suballoc.type != RESOURCE_SUBALLOCATION_TYPE_FREE)
		{
			outInfo.SuballocationSizeMin = RESOURCE_MIN(outInfo.SuballocationSizeMin, suballoc.size);
			outInfo.SuballocationSizeMax = RESOURCE_MAX(outInfo.SuballocationSizeMax, suballoc.size);
		}
		else
		{
			outInfo.UnusedRangeSizeMin = RESOURCE_MIN(outInfo.UnusedRangeSizeMin, suballoc.size);
			outInfo.UnusedRangeSizeMax = RESOURCE_MAX(outInfo.UnusedRangeSizeMax, suballoc.size);
		}
	}
}

// Adds statistics srcInfo into inoutInfo, like: inoutInfo += srcInfo.
static void AllocatorAddStatInfo(AllocatorStatInfo& inoutInfo, const AllocatorStatInfo& srcInfo)
{
	inoutInfo.AllocationCount += srcInfo.AllocationCount;
	inoutInfo.SuballocationCount += srcInfo.SuballocationCount;
	inoutInfo.UnusedRangeCount += srcInfo.UnusedRangeCount;
	inoutInfo.UsedBytes += srcInfo.UsedBytes;
	inoutInfo.UnusedBytes += srcInfo.UnusedBytes;
	inoutInfo.SuballocationSizeMin = RESOURCE_MIN(inoutInfo.SuballocationSizeMin, srcInfo.SuballocationSizeMin);
	inoutInfo.SuballocationSizeMax = RESOURCE_MAX(inoutInfo.SuballocationSizeMax, srcInfo.SuballocationSizeMax);
	inoutInfo.UnusedRangeSizeMin = RESOURCE_MIN(inoutInfo.UnusedRangeSizeMin, srcInfo.UnusedRangeSizeMin);
	inoutInfo.UnusedRangeSizeMax = RESOURCE_MAX(inoutInfo.UnusedRangeSizeMax, srcInfo.UnusedRangeSizeMax);
}

static void AllocatorPostprocessCalcStatInfo(AllocatorStatInfo& inoutInfo)
{
	inoutInfo.SuballocationSizeAvg =
		(inoutInfo.SuballocationCount > 0) ? AllocatorRoundDiv<UINT64>(inoutInfo.UsedBytes, inoutInfo.SuballocationCount) : 0;
	inoutInfo.UnusedRangeSizeAvg =
		(inoutInfo.UnusedRangeCount > 0) ? AllocatorRoundDiv<UINT64>(inoutInfo.UnusedBytes, inoutInfo.UnusedRangeCount) : 0;
}

AllocatorBlockVector::AllocatorBlockVector(ResourceAllocator* hAllocator): m_hAllocator(hAllocator)
//m_Blocks (AllocatorStlAllocator<AllocatorBlock*> (hAllocator->GetAllocationCallbacks ()))
{
}

AllocatorBlockVector::~AllocatorBlockVector()
{
	for (size_t i = m_Blocks.size(); i--;)
	{
		m_Blocks[i]->Destroy(m_hAllocator);
		resourceAlloc_delete(m_Blocks[i]);
	}
}

void AllocatorBlockVector::Remove(AllocatorBlock* pBlock)
{
	for (uint32_t blockIndex = 0; blockIndex < m_Blocks.size(); ++blockIndex)
	{
		if (m_Blocks[blockIndex] == pBlock)
		{
			VectorRemove(m_Blocks, blockIndex);
			return;
		}
	}
	ASSERT(0);
}

void AllocatorBlockVector::IncrementallySortBlocks()
{
	// Bubble sort only until first swap.
	for (size_t i = 1; i < m_Blocks.size(); ++i)
	{
		if (m_Blocks[i - 1]->m_SumFreeSize > m_Blocks[i]->m_SumFreeSize)
		{
			RESOURCE_SWAP(m_Blocks[i - 1], m_Blocks[i]);
			return;
		}
	}
}

//#if RESOURCE_STATS_STRING_ENABLED

void AllocatorBlockVector::PrintDetailedMap(class AllocatorStringBuilder& sb) const
{
	for (size_t i = 0; i < m_Blocks.size(); ++i)
	{
		if (i > 0)
		{
			sb.Add(",\n\t\t");
		}
		else
		{
			sb.Add("\n\t\t");
		}
		m_Blocks[i]->PrintDetailedMap(sb);
	}
}

//#endif // #if RESOURCE_STATS_STRING_ENABLED

void AllocatorBlockVector::UnmapPersistentlyMappedMemory()
{
	for (size_t i = m_Blocks.size(); i--;)
	{
		AllocatorBlock* pBlock = m_Blocks[i];
		if (pBlock->m_pMappedData != RESOURCE_NULL)
		{
			pBlock->m_hResource->Unmap(0, NULL);

			ASSERT(pBlock->m_PersistentMap != false);
			pBlock->m_pMappedData = RESOURCE_NULL;
		}
	}
}

HRESULT AllocatorBlockVector::MapPersistentlyMappedMemory()
{
	HRESULT finalResult = S_OK;
	for (size_t i = 0, count = m_Blocks.size(); i < count; ++i)
	{
		AllocatorBlock* pBlock = m_Blocks[i];
		if (pBlock->m_PersistentMap)
		{
			pBlock->m_hResource->Map(0, NULL, &pBlock->m_pMappedData);
		}
	}
	return finalResult;
}

void AllocatorBlockVector::AddStats(AllocatorStats* pStats, uint32_t memTypeIndex, uint32_t memHeapIndex) const
{
	for (uint32_t allocIndex = 0; allocIndex < m_Blocks.size(); ++allocIndex)
	{
		const AllocatorBlock* const pBlock = m_Blocks[allocIndex];
		ASSERT(pBlock);
		RESOURCE_HEAVY_ASSERT(pBlock->Validate());
		AllocatorStatInfo allocationStatInfo;
		CalcAllocationStatInfo(allocationStatInfo, *pBlock);
		AllocatorAddStatInfo(pStats->total, allocationStatInfo);
		AllocatorAddStatInfo(pStats->memoryType[memTypeIndex], allocationStatInfo);
		AllocatorAddStatInfo(pStats->memoryHeap[memHeapIndex], allocationStatInfo);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Allocator_T

ResourceAllocator::ResourceAllocator(const AllocatorCreateInfo* pCreateInfo):
	m_UseMutex((pCreateInfo->flags & RESOURCE_ALLOCATOR_EXTERNALLY_SYNCHRONIZED_BIT) == 0),
	m_PhysicalDevice(pCreateInfo->physicalDevice),
	m_hDevice(pCreateInfo->device),
	m_PreferredLargeHeapBlockSize(0),
	m_PreferredSmallHeapBlockSize(0),
	m_UnmapPersistentlyMappedMemoryCounter(0)
{
	ASSERT(pCreateInfo->physicalDevice && pCreateInfo->device);

	memset(&m_pBlockVectors, 0, sizeof(m_pBlockVectors));
	memset(&m_HasEmptyBlock, 0, sizeof(m_HasEmptyBlock));
	memset(&m_pOwnAllocations, 0, sizeof(m_pOwnAllocations));
	m_PreferredLargeHeapBlockSize = (pCreateInfo->preferredLargeHeapBlockSize != 0)
										? pCreateInfo->preferredLargeHeapBlockSize
										: static_cast<UINT64>(RESOURCE_DEFAULT_LARGE_HEAP_BLOCK_SIZE);
	m_PreferredSmallHeapBlockSize = (pCreateInfo->preferredSmallHeapBlockSize != 0)
										? pCreateInfo->preferredSmallHeapBlockSize
										: static_cast<UINT64>(RESOURCE_DEFAULT_SMALL_HEAP_BLOCK_SIZE);

	m_PhysicalDevice->GetDesc(&m_PhysicalDeviceProperties);

	pRenderer = pCreateInfo->pRenderer;

	for (size_t i = 0; i < GetMemoryTypeCount(); ++i)
	{
		for (size_t j = 0; j < RESOURCE_BLOCK_VECTOR_TYPE_COUNT; ++j)
		{
			m_pBlockVectors[i][j] = resourceAlloc_new(AllocatorBlockVector, this);
			m_pOwnAllocations[i][j] = resourceAlloc_new(AllocationVectorType);
		}
	}
}

ResourceAllocator::~ResourceAllocator()
{
	for (size_t i = GetMemoryTypeCount(); i--;)
	{
		for (size_t j = RESOURCE_BLOCK_VECTOR_TYPE_COUNT; j--;)
		{
			resourceAlloc_delete(m_pOwnAllocations[i][j]);
			resourceAlloc_delete(m_pBlockVectors[i][j]);
		}
	}
}

UINT64 ResourceAllocator::GetPreferredBlockSize(ResourceMemoryUsage memUsage, uint32_t memTypeIndex) const
{
	UNREF_PARAM(memUsage);
	return gHeapProperties[memTypeIndex].mBlockSize;
}

HRESULT ResourceAllocator::AllocateMemoryOfType(
	const D3D12_RESOURCE_ALLOCATION_INFO& vkMemReq, const AllocatorMemoryRequirements& resourceAllocMemReq, uint32_t memTypeIndex,
	AllocatorSuballocationType suballocType, ResourceAllocation** pAllocation)
{
	ASSERT(pAllocation != RESOURCE_NULL);
	RESOURCE_DEBUG_LOG("  AllocateMemory: MemoryTypeIndex=%u, Size=%llu", memTypeIndex, vkMemReq.SizeInBytes);

	const UINT64 preferredBlockSize = GetPreferredBlockSize(resourceAllocMemReq.usage, memTypeIndex);
	// Heuristics: Allocate own memory if requested size if greater than half of preferred block size.
	const bool ownMemory = (resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT) != 0 ||
						   RESOURCE_DEBUG_ALWAYS_OWN_MEMORY ||
						   ((resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_NEVER_ALLOCATE_BIT) == 0 &&
							vkMemReq.SizeInBytes > preferredBlockSize / 2);

	if (ownMemory)
	{
		if ((resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_NEVER_ALLOCATE_BIT) != 0)
		{
			return E_OUTOFMEMORY;
		}
		else
		{
			return AllocateOwnMemory(
				vkMemReq.SizeInBytes, suballocType, memTypeIndex,
				(resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT) != 0, resourceAllocMemReq.pUserData,
				pAllocation);
		}
	}
	else
	{
		uint32_t blockVectorType = AllocatorMemoryRequirementFlagsToBlockVectorType(resourceAllocMemReq.flags);

		AllocatorMutexLock          lock(m_BlocksMutex[memTypeIndex], m_UseMutex);
		AllocatorBlockVector* const blockVector = m_pBlockVectors[memTypeIndex][blockVectorType];
		ASSERT(blockVector);

#ifdef _DURANGO
		if ((resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_ALLOW_INDIRECT_BUFFER) == 0)
		{
#endif
			// 1. Search existing allocations.
			// Forward order - prefer blocks with smallest amount of free space.
			for (size_t allocIndex = 0; allocIndex < blockVector->m_Blocks.size(); ++allocIndex)
			{
				AllocatorBlock* const pBlock = blockVector->m_Blocks[allocIndex];
				ASSERT(pBlock);
				AllocatorAllocationRequest allocRequest = {};
				// Check if can allocate from pBlock.
				if (pBlock->CreateAllocationRequest(
						GetBufferImageGranularity(), vkMemReq.SizeInBytes, vkMemReq.Alignment, suballocType, &allocRequest))
				{
					// We no longer have an empty Allocation.
					if (pBlock->IsEmpty())
					{
						m_HasEmptyBlock[memTypeIndex] = false;
					}
					// Allocate from this pBlock.
					pBlock->Alloc(allocRequest, suballocType, vkMemReq.SizeInBytes);
					*pAllocation = resourceAlloc_new(ResourceAllocation);
					(*pAllocation)
						->InitBlockAllocation(
							pBlock, allocRequest.offset, vkMemReq.Alignment, vkMemReq.SizeInBytes, suballocType,
							resourceAllocMemReq.pUserData);
					RESOURCE_HEAVY_ASSERT(pBlock->Validate());
					RESOURCE_DEBUG_LOG("    Returned from existing allocation #%u", (uint32_t)allocIndex);
					return S_OK;
				}
			}
#ifdef _DURANGO
		}
#endif

		// 2. Create new Allocation.
		if ((resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_NEVER_ALLOCATE_BIT) != 0)
		{
			RESOURCE_DEBUG_LOG("    FAILED due to RESOURCE_MEMORY_REQUIREMENT_NEVER_ALLOCATE_BIT");
			return E_OUTOFMEMORY;
		}
		else
		{
			HRESULT res;
			if (suballocType == RESOURCE_SUBALLOCATION_TYPE_BUFFER)
			{
				D3D12_RESOURCE_DESC desc = {};
				desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				desc.Flags = D3D12_RESOURCE_FLAG_NONE;
				desc.Width = preferredBlockSize;
				desc.Height = 1;
				desc.MipLevels = 1;
				desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				desc.SampleDesc = { 1, 0 };
				desc.DepthOrArraySize = 1;

				D3D12_HEAP_PROPERTIES heapProps = gHeapProperties[memTypeIndex].mProps;
				D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
				if (heapProps.Type == D3D12_HEAP_TYPE_UPLOAD)
					state = D3D12_RESOURCE_STATE_GENERIC_READ;
				else if (heapProps.Type == D3D12_HEAP_TYPE_READBACK)
					state = D3D12_RESOURCE_STATE_COPY_DEST;

				ID3D12Resource* mem = NULL;
				res = m_hDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, state, NULL, IID_ARGS(&mem));
				mem->SetName(L"BLOCK RESOURCE");

				if (!SUCCEEDED(res))
				{
					mem->Release();
					// 3. Try half the size.
					desc.Width /= 2;
					if (desc.Width >= vkMemReq.SizeInBytes)
					{
						res = m_hDevice->CreateCommittedResource(
							&gHeapProperties[memTypeIndex].mProps, D3D12_HEAP_FLAG_NONE, &desc, state, NULL, IID_ARGS(&mem));
						mem->SetName(L"BLOCK RESOURCE");
						if (!SUCCEEDED(res))
						{
							mem->Release();
							// 4. Try quarter the size.
							desc.Width /= 2;
							if (desc.Width >= vkMemReq.SizeInBytes)
							{
								res = m_hDevice->CreateCommittedResource(
									&gHeapProperties[memTypeIndex].mProps, D3D12_HEAP_FLAG_NONE, &desc, state, NULL, IID_ARGS(&mem));
								mem->SetName(L"BLOCK RESOURCE");
							}
						}
					}
				}
				if (!SUCCEEDED(res))
				{
					// 5. Try OwnAlloc.
					res = AllocateOwnMemory(
						vkMemReq.SizeInBytes, suballocType, memTypeIndex,
						(resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT) != 0, resourceAllocMemReq.pUserData,
						pAllocation);
					if (SUCCEEDED(res))
					{
						// Succeeded: AllocateOwnMemory function already filld pMemory, nothing more to do here.
						RESOURCE_DEBUG_LOG("    Allocated as OwnMemory");
					}
					else
					{
						// Everything failed: Return error code.
						RESOURCE_DEBUG_LOG("    vkAllocateMemory FAILED");
					}

					return res;
				}

				// Create new Allocation for it.
				AllocatorBlock* const pBlock = resourceAlloc_new(AllocatorBlock, this);
				pBlock->Init(memTypeIndex, (RESOURCE_BLOCK_VECTOR_TYPE)blockVectorType, mem, desc.Width, false, NULL);

				if (blockVectorType == RESOURCE_BLOCK_VECTOR_TYPE_MAPPED && resourceAllocMemReq.usage != RESOURCE_MEMORY_USAGE_GPU_ONLY)
				{
					mem->Map(0, NULL, &pBlock->m_pMappedData);
				}

				blockVector->m_Blocks.push_back(pBlock);

				// Allocate from pBlock. Because it is empty, dstAllocRequest can be trivially filled.
				AllocatorAllocationRequest allocRequest = {};
				allocRequest.freeSuballocationItem = pBlock->m_Suballocations.begin();
				allocRequest.offset = 0;
				pBlock->Alloc(allocRequest, suballocType, vkMemReq.SizeInBytes);
				*pAllocation = resourceAlloc_new(ResourceAllocation);
				(*pAllocation)
					->InitBlockAllocation(
						pBlock, allocRequest.offset, vkMemReq.Alignment, vkMemReq.SizeInBytes, suballocType, resourceAllocMemReq.pUserData);
				RESOURCE_HEAVY_ASSERT(pBlock->Validate());

				RESOURCE_DEBUG_LOG("    Created new allocation Size=%llu", desc.Width);
			}
			else
			{
				D3D12_HEAP_FLAGS heapFlags = gHeapProperties[memTypeIndex].mFlags;

				D3D12_HEAP_DESC allocInfo = { 0 };
				allocInfo.Alignment = vkMemReq.Alignment;
				allocInfo.SizeInBytes = preferredBlockSize;
				allocInfo.Properties = gHeapProperties[memTypeIndex].mProps;
				allocInfo.Flags = heapFlags;

				if (fnHookHeapDesc != NULL)
					fnHookHeapDesc(resourceAllocMemReq.flags, allocInfo);

				// Start with full preferredBlockSize.
				ID3D12Heap* mem = NULL;
				res = m_hDevice->CreateHeap(&allocInfo, IID_ARGS(&mem));
				if (mem)
					mem->SetName(L"HEAP");
				if (!SUCCEEDED(res))
				{
					if (mem)
						mem->Release();
					// 3. Try half the size.
					allocInfo.SizeInBytes /= 2;
					if (allocInfo.SizeInBytes >= vkMemReq.SizeInBytes)
					{
						res = m_hDevice->CreateHeap(&allocInfo, IID_ARGS(&mem));
						if (mem)
							mem->SetName(L"HEAP");
						if (!SUCCEEDED(res))
						{
							if (mem)
								mem->Release();
							// 4. Try quarter the size.
							allocInfo.SizeInBytes /= 2;
							if (allocInfo.SizeInBytes >= vkMemReq.SizeInBytes)
							{
								res = m_hDevice->CreateHeap(&allocInfo, IID_ARGS(&mem));
								if (mem)
									mem->SetName(L"HEAP");
							}
						}
					}
				}
				if (!SUCCEEDED(res))
				{
					// 5. Try OwnAlloc.
					res = AllocateOwnMemory(
						vkMemReq.SizeInBytes, suballocType, memTypeIndex,
						(resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT) != 0, resourceAllocMemReq.pUserData,
						pAllocation);
					if (SUCCEEDED(res))
					{
						// Succeeded: AllocateOwnMemory function already filld pMemory, nothing more to do here.
						RESOURCE_DEBUG_LOG("    Allocated as OwnMemory");
					}
					else
					{
						// Everything failed: Return error code.
						RESOURCE_DEBUG_LOG("    vkAllocateMemory FAILED");
					}

					return res;
				}

				// Create new Allocation for it.
				AllocatorBlock* const pBlock = resourceAlloc_new(AllocatorBlock, this);
				pBlock->Init(memTypeIndex, (RESOURCE_BLOCK_VECTOR_TYPE)blockVectorType, mem, allocInfo.SizeInBytes, false, NULL);

				blockVector->m_Blocks.push_back(pBlock);

				// Allocate from pBlock. Because it is empty, dstAllocRequest can be trivially filled.
				AllocatorAllocationRequest allocRequest = {};
				allocRequest.freeSuballocationItem = pBlock->m_Suballocations.begin();
				allocRequest.offset = 0;
				pBlock->Alloc(allocRequest, suballocType, vkMemReq.SizeInBytes);
				*pAllocation = resourceAlloc_new(ResourceAllocation);
				(*pAllocation)
					->InitBlockAllocation(
						pBlock, allocRequest.offset, vkMemReq.Alignment, vkMemReq.SizeInBytes, suballocType, resourceAllocMemReq.pUserData);
				RESOURCE_HEAVY_ASSERT(pBlock->Validate());

				RESOURCE_DEBUG_LOG("    Created new allocation Size=%llu", allocInfo.SizeInBytes);
			}
			return S_OK;
		}
	}
}

HRESULT ResourceAllocator::AllocateOwnMemory(
	UINT64 size, AllocatorSuballocationType suballocType, uint32_t memTypeIndex, bool map, void* pUserData,
	ResourceAllocation** pAllocation)
{
	ASSERT(pAllocation);

	*pAllocation = resourceAlloc_new(ResourceAllocation);
	(*pAllocation)->InitOwnAllocation(memTypeIndex, suballocType, map, NULL, size, pUserData);

	// Register it in m_pOwnAllocations.
	{
		AllocatorMutexLock    lock(m_OwnAllocationsMutex[memTypeIndex], m_UseMutex);
		AllocationVectorType* pOwnAllocations =
			m_pOwnAllocations[memTypeIndex][map ? RESOURCE_BLOCK_VECTOR_TYPE_MAPPED : RESOURCE_BLOCK_VECTOR_TYPE_UNMAPPED];
		ASSERT(pOwnAllocations);
		ResourceAllocation** const pOwnAllocationsBeg = pOwnAllocations->data();
		ResourceAllocation** const pOwnAllocationsEnd = pOwnAllocationsBeg + pOwnAllocations->size();
		const size_t               indexToInsert =
			AllocatorBinaryFindFirstNotLess(pOwnAllocationsBeg, pOwnAllocationsEnd, *pAllocation, AllocatorPointerLess()) -
			pOwnAllocationsBeg;
		VectorInsert(*pOwnAllocations, indexToInsert, *pAllocation);
	}

	RESOURCE_DEBUG_LOG("    Allocated OwnMemory MemoryTypeIndex=#%u", memTypeIndex);

	return S_OK;
}

HRESULT ResourceAllocator::AllocateMemory(
	const D3D12_RESOURCE_ALLOCATION_INFO& vkMemReq, const AllocatorMemoryRequirements& resourceAllocMemReq,
	AllocatorSuballocationType suballocType, ResourceAllocation** pAllocation)
{
	if ((resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT) != 0 &&
		(resourceAllocMemReq.flags & RESOURCE_MEMORY_REQUIREMENT_NEVER_ALLOCATE_BIT) != 0)
	{
		ASSERT(
			0 &&
			"Specifying RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT together with RESOURCE_MEMORY_REQUIREMENT_NEVER_ALLOCATE_BIT makes no "
			"sense.");
		return E_OUTOFMEMORY;
	}

	// Bit mask of memory Vulkan types acceptable for this allocation.
	uint32_t memTypeIndex = UINT32_MAX;
	HRESULT  res = resourceAllocFindMemoryTypeIndex(this, &vkMemReq, &resourceAllocMemReq, suballocType, &memTypeIndex);
	if (SUCCEEDED(res))
	{
		res = AllocateMemoryOfType(vkMemReq, resourceAllocMemReq, memTypeIndex, suballocType, pAllocation);
		// Succeeded on first try.
		if (SUCCEEDED(res))
		{
			return res;
		}
		// Allocation from this memory type failed. Try other compatible memory types.
		else
		{
			return E_OUTOFMEMORY;
		}
	}
	// Can't find any single memory type maching requirements. res is VK_ERROR_FEATURE_NOT_PRESENT.
	else
		return res;
}

void ResourceAllocator::FreeMemory(ResourceAllocation* allocation)
{
	ASSERT(allocation);

	if (allocation->GetType() == ResourceAllocation::ALLOCATION_TYPE_BLOCK)
	{
		AllocatorBlock* pBlockToDelete = RESOURCE_NULL;

		const uint32_t                   memTypeIndex = allocation->GetMemoryTypeIndex();
		const RESOURCE_BLOCK_VECTOR_TYPE blockVectorType = allocation->GetBlockVectorType();
		{
			AllocatorMutexLock lock(m_BlocksMutex[memTypeIndex], m_UseMutex);

			AllocatorBlockVector* pBlockVector = m_pBlockVectors[memTypeIndex][blockVectorType];
			AllocatorBlock*       pBlock = allocation->GetBlock();

			pBlock->Free(allocation);
			RESOURCE_HEAVY_ASSERT(pBlock->Validate());

			RESOURCE_DEBUG_LOG("  Freed from MemoryTypeIndex=%u", memTypeIndex);

			// pBlock became empty after this deallocation.
			if (pBlock->IsEmpty())
			{
				// Already has empty Allocation. We don't want to have two, so delete this one.
				if (m_HasEmptyBlock[memTypeIndex])
				{
					pBlockToDelete = pBlock;
					pBlockVector->Remove(pBlock);
				}
				// We now have first empty Allocation.
				else
				{
					m_HasEmptyBlock[memTypeIndex] = true;
				}
			}
			// Must be called after srcBlockIndex is used, because later it may become invalid!
			pBlockVector->IncrementallySortBlocks();
		}
		// Destruction of a free Allocation. Deferred until this point, outside of mutex
		// lock, for performance reason.
		if (pBlockToDelete != RESOURCE_NULL)
		{
			RESOURCE_DEBUG_LOG("    Deleted empty allocation");
			pBlockToDelete->Destroy(this);
			resourceAlloc_delete(pBlockToDelete);
		}

		resourceAlloc_delete(allocation);
	}
	else    // AllocatorAllocation_T::ALLOCATION_TYPE_OWN
	{
		FreeOwnMemory(allocation);
	}
}

void ResourceAllocator::CalculateStats(AllocatorStats* pStats)
{
	InitStatInfo(pStats->total);
	for (size_t i = 0; i < RESOURCE_MEMORY_TYPE_NUM_TYPES; ++i)
		InitStatInfo(pStats->memoryType[i]);
	for (size_t i = 0; i < RESOURCE_MEMORY_TYPE_NUM_TYPES; ++i)
		InitStatInfo(pStats->memoryHeap[i]);

	for (uint32_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
	{
		AllocatorMutexLock allocationsLock(m_BlocksMutex[memTypeIndex], m_UseMutex);
		const uint32_t     heapIndex = memTypeIndex;
		for (uint32_t blockVectorType = 0; blockVectorType < RESOURCE_BLOCK_VECTOR_TYPE_COUNT; ++blockVectorType)
		{
			const AllocatorBlockVector* const pBlockVector = m_pBlockVectors[memTypeIndex][blockVectorType];
			ASSERT(pBlockVector);
			pBlockVector->AddStats(pStats, memTypeIndex, heapIndex);
		}
	}

	AllocatorPostprocessCalcStatInfo(pStats->total);
	for (size_t i = 0; i < GetMemoryTypeCount(); ++i)
		AllocatorPostprocessCalcStatInfo(pStats->memoryType[i]);
	for (size_t i = 0; i < GetMemoryHeapCount(); ++i)
		AllocatorPostprocessCalcStatInfo(pStats->memoryHeap[i]);
}

static const uint32_t RESOURCE_VENDOR_ID_AMD = 4098;

void ResourceAllocator::UnmapPersistentlyMappedMemory()
{
	if (m_UnmapPersistentlyMappedMemoryCounter++ == 0)
	{
		if (m_PhysicalDeviceProperties.VendorId == RESOURCE_VENDOR_ID_AMD)
		{
			size_t memTypeIndex = D3D12_HEAP_TYPE_UPLOAD;
			{
				{
					// Process OwnAllocations.
					{
						AllocatorMutexLock    lock(m_OwnAllocationsMutex[memTypeIndex], m_UseMutex);
						AllocationVectorType* pOwnAllocationsVector = m_pOwnAllocations[memTypeIndex][RESOURCE_BLOCK_VECTOR_TYPE_MAPPED];
						for (size_t ownAllocIndex = pOwnAllocationsVector->size(); ownAllocIndex--;)
						{
							ResourceAllocation* hAlloc = (*pOwnAllocationsVector)[ownAllocIndex];
							hAlloc->OwnAllocUnmapPersistentlyMappedMemory();
						}
					}

					// Process normal Allocations.
					{
						AllocatorMutexLock    lock(m_BlocksMutex[memTypeIndex], m_UseMutex);
						AllocatorBlockVector* pBlockVector = m_pBlockVectors[memTypeIndex][RESOURCE_BLOCK_VECTOR_TYPE_MAPPED];
						pBlockVector->UnmapPersistentlyMappedMemory();
					}
				}
			}
		}
	}
}

HRESULT ResourceAllocator::MapPersistentlyMappedMemory()
{
	ASSERT(m_UnmapPersistentlyMappedMemoryCounter > 0);
	if (--m_UnmapPersistentlyMappedMemoryCounter == 0)
	{
		HRESULT finalResult = S_OK;
		if (m_PhysicalDeviceProperties.VendorId == RESOURCE_VENDOR_ID_AMD)
		{
			size_t memTypeIndex = D3D12_HEAP_TYPE_UPLOAD;
			{
				{
					// Process OwnAllocations.
					{
						AllocatorMutexLock    lock(m_OwnAllocationsMutex[memTypeIndex], m_UseMutex);
						AllocationVectorType* pAllocationsVector = m_pOwnAllocations[memTypeIndex][RESOURCE_BLOCK_VECTOR_TYPE_MAPPED];
						for (size_t ownAllocIndex = 0, ownAllocCount = pAllocationsVector->size(); ownAllocIndex < ownAllocCount;
							 ++ownAllocIndex)
						{
							ResourceAllocation* hAlloc = (*pAllocationsVector)[ownAllocIndex];
							hAlloc->OwnAllocMapPersistentlyMappedMemory();
						}
					}

					// Process normal Allocations.
					{
						AllocatorMutexLock    lock(m_BlocksMutex[memTypeIndex], m_UseMutex);
						AllocatorBlockVector* pBlockVector = m_pBlockVectors[memTypeIndex][RESOURCE_BLOCK_VECTOR_TYPE_MAPPED];
						HRESULT               localResult = pBlockVector->MapPersistentlyMappedMemory();
						if (!SUCCEEDED(localResult))
						{
							finalResult = localResult;
						}
					}
				}
			}
		}
		return finalResult;
	}
	else
		return S_OK;
}

void ResourceAllocator::GetAllocationInfo(ResourceAllocation* hAllocation, ResourceAllocationInfo* pAllocationInfo)
{
	pAllocationInfo->memoryType = hAllocation->GetMemoryTypeIndex();
	pAllocationInfo->deviceMemory = hAllocation->GetMemory();
	pAllocationInfo->resource = hAllocation->GetResource();
	pAllocationInfo->offset = hAllocation->GetOffset();
	pAllocationInfo->size = hAllocation->GetSize();
	pAllocationInfo->pMappedData = hAllocation->GetMappedData();
	pAllocationInfo->pUserData = hAllocation->GetUserData();
}

void ResourceAllocator::FreeOwnMemory(ResourceAllocation* allocation)
{
	ASSERT(allocation && allocation->GetType() == ResourceAllocation::ALLOCATION_TYPE_OWN);

	const uint32_t memTypeIndex = allocation->GetMemoryTypeIndex();
	{
		AllocatorMutexLock          lock(m_OwnAllocationsMutex[memTypeIndex], m_UseMutex);
		AllocationVectorType* const pOwnAllocations = m_pOwnAllocations[memTypeIndex][allocation->GetBlockVectorType()];
		ASSERT(pOwnAllocations);
		ResourceAllocation** const pOwnAllocationsBeg = pOwnAllocations->data();
		ResourceAllocation** const pOwnAllocationsEnd = pOwnAllocationsBeg + pOwnAllocations->size();
		ResourceAllocation** const pOwnAllocationIt =
			AllocatorBinaryFindFirstNotLess(pOwnAllocationsBeg, pOwnAllocationsEnd, allocation, AllocatorPointerLess());
		if (pOwnAllocationIt != pOwnAllocationsEnd)
		{
			const size_t ownAllocationIndex = pOwnAllocationIt - pOwnAllocationsBeg;
			VectorRemove(*pOwnAllocations, ownAllocationIndex);
		}
		else
		{
			ASSERT(0);
		}
	}

	RESOURCE_DEBUG_LOG("    Freed OwnMemory MemoryTypeIndex=%u", memTypeIndex);

	resourceAlloc_delete(allocation);
}

//#if RESOURCE_STATS_STRING_ENABLED

void ResourceAllocator::PrintDetailedMap(AllocatorStringBuilder& sb)
{
	bool ownAllocationsStarted = false;
	for (size_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
	{
		AllocatorMutexLock ownAllocationsLock(m_OwnAllocationsMutex[memTypeIndex], m_UseMutex);
		for (uint32_t blockVectorType = 0; blockVectorType < RESOURCE_BLOCK_VECTOR_TYPE_COUNT; ++blockVectorType)
		{
			AllocationVectorType* const pOwnAllocVector = m_pOwnAllocations[memTypeIndex][blockVectorType];
			ASSERT(pOwnAllocVector);
			if (pOwnAllocVector->empty() == false)
			{
				if (ownAllocationsStarted)
				{
					sb.Add(",\n\t\"Type ");
				}
				else
				{
					sb.Add(",\n\"OwnAllocations\": {\n\t\"Type ");
					ownAllocationsStarted = true;
				}
				sb.AddNumber(memTypeIndex);
				if (blockVectorType == RESOURCE_BLOCK_VECTOR_TYPE_MAPPED)
				{
					sb.Add(" Mapped");
				}
				sb.Add("\": [");

				for (size_t i = 0; i < pOwnAllocVector->size(); ++i)
				{
					const ResourceAllocation* hAlloc = (*pOwnAllocVector)[i];
					if (i > 0)
					{
						sb.Add(",\n\t\t{ \"Size\": ");
					}
					else
					{
						sb.Add("\n\t\t{ \"Size\": ");
					}
					sb.AddNumber(hAlloc->GetSize());
					sb.Add(", \"Type\": ");
					sb.AddString(RESOURCE_SUBALLOCATION_TYPE_NAMES[hAlloc->GetSuballocationType()]);
					sb.Add(" }");
				}

				sb.Add("\n\t]");
			}
		}
	}
	if (ownAllocationsStarted)
	{
		sb.Add("\n}");
	}

	{
		bool allocationsStarted = false;
		for (size_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
		{
			AllocatorMutexLock globalAllocationsLock(m_BlocksMutex[memTypeIndex], m_UseMutex);
			for (uint32_t blockVectorType = 0; blockVectorType < RESOURCE_BLOCK_VECTOR_TYPE_COUNT; ++blockVectorType)
			{
				if (m_pBlockVectors[memTypeIndex][blockVectorType]->IsEmpty() == false)
				{
					if (allocationsStarted)
					{
						sb.Add(",\n\t\"Type ");
					}
					else
					{
						sb.Add(",\n\"Allocations\": {\n\t\"Type ");
						allocationsStarted = true;
					}
					sb.AddNumber(memTypeIndex);
					if (blockVectorType == RESOURCE_BLOCK_VECTOR_TYPE_MAPPED)
					{
						sb.Add(" Mapped");
					}
					sb.Add("\": [");

					m_pBlockVectors[memTypeIndex][blockVectorType]->PrintDetailedMap(sb);

					sb.Add("\n\t]");
				}
			}
		}
		if (allocationsStarted)
		{
			sb.Add("\n}");
		}
	}
}

#endif

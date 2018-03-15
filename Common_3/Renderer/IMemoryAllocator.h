/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

#pragma once

typedef struct BufferCreateInfo
{
#if defined(DIRECT3D12)
	const D3D12_RESOURCE_DESC*	pDesc;
	D3D12_RESOURCE_STATES		mStartState;
	wchar_t*					pDebugName;
#elif defined(VULKAN)
	const VkBufferCreateInfo*	pDesc;
#elif defined(METAL)
    const uint64_t              mSize;
    //const uint64_t              mAlignment;
#endif
} BufferCreateInfo;

typedef struct TextureCreateInfo
{
#if defined(DIRECT3D12)
	const D3D12_RESOURCE_DESC*	pDesc;
	const D3D12_CLEAR_VALUE*	pClearValue;
	D3D12_RESOURCE_STATES		mStartState;
	wchar_t*					pDebugName;
#elif defined(VULKAN)
	VkImageCreateInfo*			pDesc;
#elif defined(METAL)
    MTLTextureDescriptor*       pDesc;
    const bool                  mIsRT;
    const bool                  mIsMS;
#endif
} TextureCreateInfo;

#if defined(DIRECT3D12)
typedef struct ResourceAllocator MemoryAllocator;
#include "Direct3D12/Direct3D12MemoryAllocator.h"
#elif defined(VULKAN)
typedef struct VmaAllocator_T MemoryAllocator;
typedef struct VmaAllocationCreateInfo AllocatorMemoryRequirements;
#include "../ThirdParty/OpenSource/VulkanMemoryAllocator/VulkanMemoryAllocator.h"
#elif defined(METAL)
typedef struct ResourceAllocator MemoryAllocator;
#include "Metal/MetalMemoryAllocator.h"
#endif

long createBuffer(MemoryAllocator* pAllocator, const BufferCreateInfo* pCreateInfo, const AllocatorMemoryRequirements* pMemoryRequirements, Buffer* pBuffer);
void destroyBuffer(MemoryAllocator* pAllocator, struct Buffer* pBuffer);

long createTexture(MemoryAllocator* pAllocator, const TextureCreateInfo* pCreateInfo, const AllocatorMemoryRequirements* pMemoryRequirements, Texture* pTexture);
void destroyTexture(MemoryAllocator* pAllocator, struct Texture* pTexture);

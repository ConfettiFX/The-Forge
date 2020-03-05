/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

void initHooks();

typedef uint32_t (*PFN_HOOK_ADD_DESCRIPTIOR_HEAP)(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);

typedef void (*PFN_HOOK_POST_INIT_RENDERER)(Renderer* pRenderer);

typedef void (*PFN_HOOK_POST_REMOVE_RENDERER)(Renderer* pRenderer);

typedef void (*PFN_HOOK_ADD_BUFFER)(const BufferDesc* pBuffer, D3D12_RESOURCE_DESC& desc);

typedef void (*PFN_HOOK_ENABLE_DEBUG_LAYER)(Renderer* pRenderer);

typedef void (*PFN_HOOK_HEAP_FLAGS)(DescriptorType type, D3D12_HEAP_FLAGS& flags);

typedef TinyImageFormat (*PFN_HOOK_GET_RECOMMENDED_SWAP_CHAIN_FORMAT)(bool hintHDR);

typedef void (*PFN_HOOK_MODIFY_SWAP_CHAIN_DESC)(DXGI_SWAP_CHAIN_DESC1* desc);

typedef uint32_t (*PFN_HOOK_GET_SWAP_CHAIN_IMAGE_INDEX)(SwapChain* pSwapChain);

typedef void (*PFN_HOOK_SHADER_COMPILE_FLAGS)(uint32_t& compileFlags);

typedef void (*PFN_HOOK_RESOURCE_ALLOCATION_INFO)(D3D12_RESOURCE_ALLOCATION_INFO& info, UINT64 alignment);

typedef bool (*PFN_HOOK_SPECIAL_BUFFER_ALLOCATION)(
	Renderer* pRenderer, const D3D12_RESOURCE_DESC* pCreateInfo, D3D12_RESOURCE_STATES startState, BufferCreationFlags flags, ID3D12Resource** res);

typedef bool (*PFN_HOOK_SPECIAL_TEXTURE_ALLOCATION)(
	Renderer* pRenderer, const D3D12_RESOURCE_DESC* pCreateInfo, const D3D12_CLEAR_VALUE* clearValue, D3D12_RESOURCE_STATES startState, TextureCreationFlags flags, ID3D12Resource** res);

typedef void (*PFN_HOOK_RESOURCE_FLAGS)(D3D12_RESOURCE_FLAGS& resourceFlags, TextureCreationFlags creationFlags);

extern PFN_HOOK_ADD_DESCRIPTIOR_HEAP              fnHookAddDescriptorHeap;
extern PFN_HOOK_POST_INIT_RENDERER                fnHookPostInitRenderer;
extern PFN_HOOK_POST_REMOVE_RENDERER              fnHookPostRemoveRenderer;
extern PFN_HOOK_ADD_BUFFER                        fnHookAddBuffer;
extern PFN_HOOK_ENABLE_DEBUG_LAYER                fnHookEnableDebugLayer;
extern PFN_HOOK_HEAP_FLAGS                        fnHookHeapFlags;
extern PFN_HOOK_GET_RECOMMENDED_SWAP_CHAIN_FORMAT fnHookGetRecommendedSwapChainFormat;
extern PFN_HOOK_MODIFY_SWAP_CHAIN_DESC            fnHookModifySwapChainDesc;
extern PFN_HOOK_GET_SWAP_CHAIN_IMAGE_INDEX        fnHookGetSwapChainImageIndex;
extern PFN_HOOK_SHADER_COMPILE_FLAGS              fnHookShaderCompileFlags;
extern PFN_HOOK_RESOURCE_ALLOCATION_INFO          fnHookResourceAllocationInfo;
extern PFN_HOOK_SPECIAL_BUFFER_ALLOCATION         fnHookSpecialBufferAllocation;
extern PFN_HOOK_SPECIAL_TEXTURE_ALLOCATION        fnHookSpecialTextureAllocation;
extern PFN_HOOK_RESOURCE_FLAGS                    fnHookResourceFlags;

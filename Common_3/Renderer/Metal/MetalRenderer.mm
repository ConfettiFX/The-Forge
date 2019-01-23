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

#ifdef METAL

#define RENDERER_IMPLEMENTATION
#define MAX_FRAMES_IN_FLIGHT 3

#if !defined(__APPLE__) && !defined(TARGET_OS_MAC)
#error "MacOs is needed!"
#endif
#import <simd/simd.h>
#import <MetalKit/MetalKit.h>

#import "../IRenderer.h"
#include "MetalMemoryAllocator.h"
#include "../../OS/Interfaces/ILogManager.h"
#include "../../OS/Core/GPUConfig.h"
#include "../../OS/Interfaces/IMemoryManager.h"

#define MAX_BUFFER_BINDINGS 31

extern void mtl_createShaderReflection(
	Renderer* pRenderer, Shader* shader, const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage,
	tinystl::unordered_map<uint32_t, MTLVertexFormat>* vertexAttributeFormats, ShaderReflection* pOutReflection);

#if defined(__cplusplus) && defined(RENDERER_CPP_NAMESPACE)
namespace RENDERER_CPP_NAMESPACE {
#endif

// clang-format off
	MTLBlendOperation gMtlBlendOpTranslator[BlendMode::MAX_BLEND_MODES] =
	{
		MTLBlendOperationAdd,
		MTLBlendOperationSubtract,
		MTLBlendOperationReverseSubtract,
		MTLBlendOperationMin,
		MTLBlendOperationMax,
	};

	MTLBlendFactor gMtlBlendConstantTranslator[BlendConstant::MAX_BLEND_CONSTANTS] =
	{
		MTLBlendFactorZero,
		MTLBlendFactorOne,
		MTLBlendFactorSourceColor,
		MTLBlendFactorOneMinusSourceColor,
		MTLBlendFactorDestinationColor,
		MTLBlendFactorOneMinusDestinationColor,
		MTLBlendFactorSourceAlpha,
		MTLBlendFactorOneMinusSourceAlpha,
		MTLBlendFactorDestinationAlpha,
		MTLBlendFactorOneMinusDestinationAlpha,
		MTLBlendFactorSourceAlphaSaturated,
		MTLBlendFactorBlendColor,
		MTLBlendFactorOneMinusBlendColor,
		//MTLBlendFactorBlendAlpha,
		//MTLBlendFactorOneMinusBlendAlpha,
		//MTLBlendFactorSource1Color,
		//MTLBlendFactorOneMinusSource1Color,
		//MTLBlendFactorSource1Alpha,
		//MTLBlendFactorOneMinusSource1Alpha,
	};

	MTLCompareFunction gMtlComparisonFunctionTranslator[CompareMode::MAX_COMPARE_MODES] =
	{
		MTLCompareFunctionNever,
		MTLCompareFunctionLess,
		MTLCompareFunctionEqual,
		MTLCompareFunctionLessEqual,
		MTLCompareFunctionGreater,
		MTLCompareFunctionNotEqual,
		MTLCompareFunctionGreaterEqual,
		MTLCompareFunctionAlways,
	};

	MTLStencilOperation gMtlStencilOpTranslator[StencilOp::MAX_STENCIL_OPS] = {
		MTLStencilOperationKeep,
		MTLStencilOperationZero,
		MTLStencilOperationReplace,
		MTLStencilOperationInvert,
		MTLStencilOperationIncrementWrap,
		MTLStencilOperationDecrementWrap,
		MTLStencilOperationIncrementClamp,
		MTLStencilOperationDecrementClamp,
	};

	MTLCullMode gMtlCullModeTranslator[CullMode::MAX_CULL_MODES] =
	{
		MTLCullModeNone,
		MTLCullModeBack,
		MTLCullModeFront,
	};

	MTLTriangleFillMode gMtlFillModeTranslator[FillMode::MAX_FILL_MODES] =
	{
		MTLTriangleFillModeFill,
		MTLTriangleFillModeLines,
	};

	static const MTLSamplerAddressMode gMtlAddressModeTranslator[] =
	{
		MTLSamplerAddressModeMirrorRepeat,
		MTLSamplerAddressModeRepeat,
		MTLSamplerAddressModeClampToEdge,
#ifndef TARGET_IOS
		MTLSamplerAddressModeClampToBorderColor,
#else
		MTLSamplerAddressModeClampToEdge,
#endif
	};

	static const MTLPixelFormat gMtlFormatTranslator[] =
	{
		MTLPixelFormatInvalid,

		MTLPixelFormatR8Unorm,
		MTLPixelFormatRG8Unorm,
		MTLPixelFormatInvalid, //RGB8 not directly supported
		MTLPixelFormatRGBA8Unorm,

		MTLPixelFormatR16Unorm,
		MTLPixelFormatRG16Unorm,
		MTLPixelFormatInvalid, //RGB16 not directly supported
		MTLPixelFormatRGBA16Unorm,

		MTLPixelFormatR8Snorm,
		MTLPixelFormatRG8Snorm,
		MTLPixelFormatInvalid, //RGB8S not directly supported
		MTLPixelFormatRGBA8Snorm,

		MTLPixelFormatR16Snorm,
		MTLPixelFormatRG16Snorm,
		MTLPixelFormatInvalid, //RGB16S not directly supported
		MTLPixelFormatRGBA16Snorm,

		MTLPixelFormatR16Float,
		MTLPixelFormatRG16Float,
		MTLPixelFormatInvalid, //RGB16F not directly supported
		MTLPixelFormatRGBA16Float,

		MTLPixelFormatR32Float,
		MTLPixelFormatRG32Float,
		MTLPixelFormatInvalid, //RGB32F not directly supported
		MTLPixelFormatRGBA32Float,

		MTLPixelFormatR16Sint,
		MTLPixelFormatRG16Sint,
		MTLPixelFormatInvalid, //RGB16I not directly supported
		MTLPixelFormatRGBA16Sint,

		MTLPixelFormatR32Sint,
		MTLPixelFormatRG32Sint,
		MTLPixelFormatInvalid, //RGG32I not directly supported
		MTLPixelFormatRGBA32Sint,

		MTLPixelFormatR16Uint,
		MTLPixelFormatRG16Uint,
		MTLPixelFormatInvalid, //RGB16UI not directly supported
		MTLPixelFormatRGBA16Uint,

		MTLPixelFormatR32Uint,
		MTLPixelFormatRG32Uint,
		MTLPixelFormatInvalid, //RGB32UI not directly supported
		MTLPixelFormatRGBA32Uint,

		MTLPixelFormatInvalid, //RGBE8 not directly supported
		MTLPixelFormatRGB9E5Float,
		MTLPixelFormatRG11B10Float,
		MTLPixelFormatInvalid, //B5G6R5 not directly supported
		MTLPixelFormatInvalid, //RGBA4 not directly supported
		MTLPixelFormatRGB10A2Unorm,

#ifndef TARGET_IOS
		MTLPixelFormatDepth16Unorm,
		MTLPixelFormatDepth24Unorm_Stencil8,
		MTLPixelFormatDepth24Unorm_Stencil8,
#else
		// Only 32-bit depth formats are supported on iOS.
		MTLPixelFormatDepth32Float,
		MTLPixelFormatDepth32Float,
		MTLPixelFormatDepth32Float,
#endif
		MTLPixelFormatDepth32Float,

#ifndef TARGET_IOS
		MTLPixelFormatBC1_RGBA,
		MTLPixelFormatBC2_RGBA,
		MTLPixelFormatBC3_RGBA,
		MTLPixelFormatBC4_RUnorm,
		MTLPixelFormatBC5_RGUnorm,
#else
		MTLPixelFormatInvalid,
		MTLPixelFormatInvalid,
		MTLPixelFormatInvalid,
		MTLPixelFormatInvalid,
		MTLPixelFormatInvalid,
#endif

		// PVR formats
		MTLPixelFormatInvalid, // PVR_2BPP = 56,
		MTLPixelFormatInvalid, // PVR_2BPPA = 57,
		MTLPixelFormatInvalid, // PVR_4BPP = 58,
		MTLPixelFormatInvalid, // PVR_4BPPA = 59,
		MTLPixelFormatInvalid, // INTZ = 60,	// Nvidia hack. Supported on all DX10+ HW
		// XBox 360 specific fron buffer formats.
		MTLPixelFormatInvalid, // LE_XRGB8 = 61,
		MTLPixelFormatInvalid, // LE_ARGB8 = 62,
		MTLPixelFormatInvalid, // LE_X2RGB10 = 63,
		MTLPixelFormatInvalid, // LE_A2RGB10 = 64,
		// Compressed mobile forms
		MTLPixelFormatInvalid, // ETC1 = 65,	//  RGB
		MTLPixelFormatInvalid, // ATC = 66, //  RGB
		MTLPixelFormatInvalid, // ATCA = 67,	//  RGBA, explicit alpha
		MTLPixelFormatInvalid, // ATCI = 68,	//  RGBA, interpolated alpha
		MTLPixelFormatInvalid, // RAWZ = 69, //depth only, Nvidia (requires recombination of data) //FIX IT: PS3 as well?
		MTLPixelFormatInvalid, // DF16 = 70, //depth only, Intel/AMD
		MTLPixelFormatInvalid, // STENCILONLY = 71, // stencil ony usage
		MTLPixelFormatInvalid, // GNF_BC1 = 72,
		MTLPixelFormatInvalid, // GNF_BC2 = 73,
		MTLPixelFormatInvalid, // GNF_BC3 = 74,
		MTLPixelFormatInvalid, // GNF_BC4 = 75,
		MTLPixelFormatInvalid, // GNF_BC5 = 76,
		MTLPixelFormatInvalid, // GNF_BC6 = 77,
		MTLPixelFormatInvalid, // GNF_BC7 = 78,
		// Reveser Form
		MTLPixelFormatBGRA8Unorm, // BGRA8 = 79,
		// Extend for DXGI
		MTLPixelFormatInvalid, // X8D24PAX32 = 80,
		MTLPixelFormatStencil8,// S8 = 81,
		MTLPixelFormatInvalid, // D16S8 = 82,
		MTLPixelFormatDepth32Float_Stencil8, // D32S8 = 83,
	};
// clang-format on

// =================================================================================================
// IMPLEMENTATION
// =================================================================================================

#if defined(RENDERER_IMPLEMENTATION)

#define SAFE_FREE(p_var)         \
	if (p_var)                   \
	{                            \
		conf_free((void*)p_var); \
	}

#if defined(__cplusplus)
#define DECLARE_ZERO(type, var) type var = {};
#else
#define DECLARE_ZERO(type, var) type var = { 0 };
#endif

// Internal utility functions (may become external one day)
uint64_t util_pthread_to_uint64(const pthread_t& value);

bool util_is_compatible_texture_view(const MTLTextureType& textureType, const MTLTextureType& subviewTye);

MTLPixelFormat  util_to_mtl_pixel_format(const ImageFormat::Enum& format, const bool& srgb);
bool            util_is_mtl_depth_pixel_format(const MTLPixelFormat& format);
bool            util_is_mtl_compressed_pixel_format(const MTLPixelFormat& format);
MTLVertexFormat util_to_mtl_vertex_format(const ImageFormat::Enum& format);
MTLLoadAction   util_to_mtl_load_action(const LoadActionType& loadActionType);

void util_bind_argument_buffer(Cmd* pCmd, DescriptorManager* pManager, const DescriptorInfo* descInfo, const DescriptorData* descData);
void util_end_current_encoders(Cmd* pCmd);
bool util_sync_encoders(Cmd* pCmd, const CmdPoolType& newEncoderType);

void add_texture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture, const bool isRT = false);

/************************************************************************/
// Dynamic Memory Allocator
/************************************************************************/
typedef struct DynamicMemoryAllocator
{
	/// Size of mapped resources to be created
	uint64_t mSize;
	/// Current offset in the used page
	uint64_t mCurrentPos;
	/// Buffer alignment
	uint64_t mAlignment;
	Buffer*  pBuffer;

	Mutex* pAllocationMutex;
} DynamicMemoryAllocator;

void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
void removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture);
void removeTexture(Renderer* pRenderer, Texture* pTexture);

void add_dynamic_memory_allocator(Renderer* pRenderer, uint64_t size, DynamicMemoryAllocator** ppAllocator)
{
	ASSERT(pRenderer);

	DynamicMemoryAllocator* pAllocator = (DynamicMemoryAllocator*)conf_calloc(1, sizeof(*pAllocator));
	pAllocator->mCurrentPos = 0;
	pAllocator->mSize = size;
	pAllocator->pAllocationMutex = conf_placement_new<Mutex>(conf_calloc(1, sizeof(Mutex)));

	BufferDesc desc = {};
	desc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER | DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	desc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	desc.mSize = pAllocator->mSize;
	desc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	addBuffer(pRenderer, &desc, &pAllocator->pBuffer);

	pAllocator->mAlignment = pRenderer->pActiveGpuSettings->mUniformBufferAlignment;

	*ppAllocator = pAllocator;
}

void remove_dynamic_memory_allocator(Renderer* pRenderer, DynamicMemoryAllocator* pAllocator)
{
	ASSERT(pAllocator);

	removeBuffer(pRenderer, pAllocator->pBuffer);

	pAllocator->pAllocationMutex->~Mutex();
	conf_free(pAllocator->pAllocationMutex);

	SAFE_FREE(pAllocator);
}

void reset_dynamic_memory_allocator(DynamicMemoryAllocator* pAllocator)
{
	ASSERT(pAllocator);
	pAllocator->mCurrentPos = 0;
}

void consume_dynamic_memory_allocator(
	DynamicMemoryAllocator* p_linear_allocator, uint64_t size, void** ppCpuAddress, uint64_t* pOffset, id<MTLBuffer> ppMtlBuffer)
{
	MutexLock lock(*p_linear_allocator->pAllocationMutex);

	if (p_linear_allocator->mCurrentPos + size > p_linear_allocator->mSize)
		reset_dynamic_memory_allocator(p_linear_allocator);

	*ppCpuAddress = (uint8_t*)p_linear_allocator->pBuffer->pCpuMappedAddress + p_linear_allocator->mCurrentPos;
	*pOffset = p_linear_allocator->mCurrentPos;
	if (ppMtlBuffer)
		ppMtlBuffer = p_linear_allocator->pBuffer->mtlBuffer;

	// Increment position by multiple of 256 to use CBVs in same heap as other buffers
	p_linear_allocator->mCurrentPos += round_up_64(size, p_linear_allocator->mAlignment);
}

void consume_dynamic_memory_allocator_lock_free(
	DynamicMemoryAllocator* p_linear_allocator, uint64_t size, void** ppCpuAddress, uint64_t* pOffset, id<MTLBuffer> ppMtlBuffer)
{
	if (p_linear_allocator->mCurrentPos + size > p_linear_allocator->mSize)
		reset_dynamic_memory_allocator(p_linear_allocator);

	*ppCpuAddress = (uint8_t*)p_linear_allocator->pBuffer->pCpuMappedAddress + p_linear_allocator->mCurrentPos;
	*pOffset = p_linear_allocator->mCurrentPos;
	if (ppMtlBuffer)
		ppMtlBuffer = p_linear_allocator->pBuffer->mtlBuffer;

	// Increment position by multiple of 256 to use CBVs in same heap as other buffers
	p_linear_allocator->mCurrentPos += round_up_64(size, p_linear_allocator->mAlignment);
}

/************************************************************************/
// Globals
/************************************************************************/
static Texture* pDefault1DTexture = NULL;
static Texture* pDefault1DTextureArray = NULL;
static Texture* pDefault2DTexture = NULL;
static Texture* pDefault2DTextureArray = NULL;
static Texture* pDefault3DTexture = NULL;
static Texture* pDefaultCubeTexture = NULL;
static Texture* pDefaultCubeTextureArray = NULL;

static Buffer*  pDefaultBuffer = NULL;
static Sampler* pDefaultSampler = NULL;

static BlendState*      pDefaultBlendState = NULL;
static DepthState*      pDefaultDepthState = NULL;
static RasterizerState* pDefaultRasterizerState = NULL;

static volatile uint64_t gBufferIds = 0;
static volatile uint64_t gTextureIds = 0;
static volatile uint64_t gSamplerIds = 0;

/************************************************************************/
// Descriptor Manager Implementation
/************************************************************************/
// Since there are no descriptor tables in Metal, we just hold a map of all descriptors.
using DescriptorMap = tinystl::unordered_map<uint64_t, DescriptorInfo>;
using ConstDescriptorMapIterator = tinystl::unordered_map<uint64_t, DescriptorInfo>::const_iterator;
using DescriptorMapNode = tinystl::unordered_hash_node<uint64_t, DescriptorInfo>;
using DescriptorNameToIndexMap = tinystl::unordered_map<uint32_t, uint32_t>;

typedef struct DescriptorManager
{
	/// The root signature associated with this descriptor manager.
	RootSignature* pRootSignature;
	/// The descriptor data bound to the current rootSignature;
	DescriptorData* pDescriptorDataArray;
	/// Array of flags to check whether a descriptor has already been bound.
	bool* pBoundDescriptors;
	bool  mBoundStaticSamplers;

	/// Map that holds all the argument buffers bound by this descriptor manager for each root signature.
	tinystl::unordered_map<uint32_t, tinystl::pair<Buffer*, bool>> mArgumentBuffers;
} DescriptorManager;

Mutex gDescriptorMutex;

void add_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature, DescriptorManager** ppManager)
{
	DescriptorManager* pManager = (DescriptorManager*)conf_calloc(1, sizeof(*pManager));
	pManager->pRootSignature = pRootSignature;

	// Allocate enough memory to hold all the necessary data for all the descriptors of this rootSignature.
	pManager->pDescriptorDataArray = (DescriptorData*)conf_calloc(pRootSignature->mDescriptorCount, sizeof(DescriptorData));
	pManager->pBoundDescriptors = (bool*)conf_calloc(pRootSignature->mDescriptorCount, sizeof(bool));

	// Fill all the descriptors in the rootSignature with their default values.
	for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
	{
		DescriptorInfo* descriptorInfo = &pRootSignature->pDescriptors[i];

		// Create a DescriptorData structure for a default resource.
		pManager->pDescriptorDataArray[i].pName = "";
		pManager->pDescriptorDataArray[i].mCount = 1;
		pManager->pDescriptorDataArray[i].pOffsets = NULL;

		// Metal requires that the bound textures match the texture type present in the shader.
		Texture** ppDefaultTexture = nil;
		if (descriptorInfo->mDesc.type == DESCRIPTOR_TYPE_RW_TEXTURE || descriptorInfo->mDesc.type == DESCRIPTOR_TYPE_TEXTURE)
		{
			switch ((MTLTextureType)descriptorInfo->mDesc.mtlTextureType)
			{
				case MTLTextureType1D: ppDefaultTexture = &pDefault1DTexture; break;
				case MTLTextureType1DArray: ppDefaultTexture = &pDefault1DTextureArray; break;
				case MTLTextureType2D: ppDefaultTexture = &pDefault2DTexture; break;
				case MTLTextureType2DArray: ppDefaultTexture = &pDefault2DTextureArray; break;
				case MTLTextureType3D: ppDefaultTexture = &pDefault3DTexture; break;
				case MTLTextureTypeCube: ppDefaultTexture = &pDefaultCubeTexture; break;
				case MTLTextureTypeCubeArray: ppDefaultTexture = &pDefaultCubeTextureArray; break;
				default: break;
			}
		}

		// Point to the appropiate default resource depending of the type of descriptor.
		switch (descriptorInfo->mDesc.type)
		{
			case DESCRIPTOR_TYPE_RW_TEXTURE:
			case DESCRIPTOR_TYPE_TEXTURE: pManager->pDescriptorDataArray[i].ppTextures = ppDefaultTexture; break;
			case DESCRIPTOR_TYPE_SAMPLER: pManager->pDescriptorDataArray[i].ppSamplers = &pDefaultSampler; break;
			case DESCRIPTOR_TYPE_ROOT_CONSTANT:
				// Default root constants can be bound the same way buffers are.
				pManager->pDescriptorDataArray[i].pRootConstant = &pDefaultBuffer;
				break;
			case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case DESCRIPTOR_TYPE_RW_BUFFER:
			case DESCRIPTOR_TYPE_BUFFER:
			{
				pManager->pDescriptorDataArray[i].ppBuffers = &pDefaultBuffer;
				break;
				default: break;
			}
		}
	}

	*ppManager = pManager;
}

void remove_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature, DescriptorManager* pManager)
{
	pManager->mArgumentBuffers.clear();
	SAFE_FREE(pManager->pDescriptorDataArray);
	SAFE_FREE(pManager->pBoundDescriptors);
	SAFE_FREE(pManager);
}

// This function returns the descriptor manager belonging to this thread
// If a descriptor manager does not exist for this thread, a new one is created
// With this approach we make sure that descriptor binding is thread safe and lock conf_free at the same time
DescriptorManager* get_descriptor_manager(Renderer* pRenderer, RootSignature* pRootSignature)
{
	tinystl::unordered_hash_node<ThreadID, DescriptorManager*>* pNode =
		pRootSignature->pDescriptorManagerMap.find(Thread::GetCurrentThreadID()).node;
	if (pNode == NULL)
	{
		// Only need a lock when creating a new descriptor manager for this thread
		MutexLock          lock(gDescriptorMutex);
		DescriptorManager* pManager = NULL;
		add_descriptor_manager(pRenderer, pRootSignature, &pManager);
		pRootSignature->pDescriptorManagerMap.insert({ Thread::GetCurrentThreadID(), pManager });
		return pManager;
	}
	else
	{
		return pNode->second;
	}
}

const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName, uint32_t* pIndex)
{
	DescriptorNameToIndexMap::const_iterator it = pRootSignature->pDescriptorNameToIndexMap.find(tinystl::hash(pResName));
	if (it.node)
	{
		*pIndex = it.node->second;
		return &pRootSignature->pDescriptors[it.node->second];
	}
	else
	{
		LOGERRORF("Invalid descriptor param (%s)", pResName);
		return NULL;
	}
}

void reset_bound_resources(DescriptorManager* pManager)
{
	pManager->mBoundStaticSamplers = false;
	for (uint32_t i = 0; i < pManager->pRootSignature->mDescriptorCount; ++i)
	{
		DescriptorInfo* descInfo = &pManager->pRootSignature->pDescriptors[i];

		pManager->pDescriptorDataArray[i].mCount = 1;
		pManager->pDescriptorDataArray[i].pOffsets = NULL;
		pManager->pDescriptorDataArray[i].pSizes = NULL;

		// Metal requires that the bound textures match the texture type present in the shader.
		Texture** ppDefaultTexture = nil;
		if (descInfo->mDesc.type == DESCRIPTOR_TYPE_RW_TEXTURE || descInfo->mDesc.type == DESCRIPTOR_TYPE_TEXTURE)
		{
			switch ((MTLTextureType)descInfo->mDesc.mtlTextureType)
			{
				case MTLTextureType1D: ppDefaultTexture = &pDefault1DTexture; break;
				case MTLTextureType1DArray: ppDefaultTexture = &pDefault1DTextureArray; break;
				case MTLTextureType2D: ppDefaultTexture = &pDefault2DTexture; break;
				case MTLTextureType2DArray: ppDefaultTexture = &pDefault2DTextureArray; break;
				case MTLTextureType3D: ppDefaultTexture = &pDefault3DTexture; break;
				case MTLTextureTypeCube: ppDefaultTexture = &pDefaultCubeTexture; break;
				case MTLTextureTypeCubeArray: ppDefaultTexture = &pDefaultCubeTextureArray; break;
				default: break;
			}
		}

		switch (descInfo->mDesc.type)
		{
			case DESCRIPTOR_TYPE_RW_TEXTURE:
			case DESCRIPTOR_TYPE_TEXTURE: pManager->pDescriptorDataArray[i].ppTextures = ppDefaultTexture; break;
			case DESCRIPTOR_TYPE_SAMPLER: pManager->pDescriptorDataArray[i].ppSamplers = &pDefaultSampler; break;
			case DESCRIPTOR_TYPE_ROOT_CONSTANT: pManager->pDescriptorDataArray[i].pRootConstant = &pDefaultBuffer; break;
			case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case DESCRIPTOR_TYPE_RW_BUFFER:
			case DESCRIPTOR_TYPE_BUFFER:
			{
				pManager->pDescriptorDataArray[i].ppBuffers = &pDefaultBuffer;
				break;
				default: break;
			}
		}
		pManager->pBoundDescriptors[i] = false;
	}
}

/************************************************************************/
// Get renderer shader macros
/************************************************************************/
// renderer shader macros allocated on stack
const RendererShaderDefinesDesc get_renderer_shaderdefines(Renderer* pRenderer)
{
	RendererShaderDefinesDesc defineDesc = { NULL, 0 };
	return defineDesc;
}

void cmdBindDescriptors(Cmd* pCmd, RootSignature* pRootSignature, uint32_t numDescriptors, DescriptorData* pDescParams)
{
	ASSERT(pCmd);
	ASSERT(pRootSignature);

	Renderer*          pRenderer = pCmd->pRenderer;
	DescriptorManager* pManager = get_descriptor_manager(pRenderer, pRootSignature);
	// Compare the currently bound root signature with the root signature of the descriptor manager
	// If these values dont match, we must bind the root signature of the descriptor manager
	// If the values match, no op is required
	if (pCmd->pBoundRootSignature != pRootSignature)
	{
		// Bind the new root signature and reset its bound resources (if any).
		pCmd->pBoundRootSignature = pRootSignature;
		reset_bound_resources(pManager);
	}

	// Loop through input params to check for new data
	for (uint32_t paramIdx = 0; paramIdx < numDescriptors; ++paramIdx)
	{
		const DescriptorData* pParam = &pDescParams[paramIdx];
		ASSERT(pParam);
		if (!pParam->pName)
		{
			LOGERRORF("Name of Descriptor at index (%u) is NULL", paramIdx);
			return;
		}

		uint32_t              hash = tinystl::hash(pParam->pName);
		uint32_t              descIndex = -1;
		const DescriptorInfo* pDesc = get_descriptor(pRootSignature, pParam->pName, &descIndex);
		if (!pDesc)
			continue;

		const uint32_t arrayCount = max(1U, pParam->mCount);

		// Replace the default DescriptorData by the new data pased into this function.
		pManager->pDescriptorDataArray[descIndex].pName = pParam->pName;
		pManager->pDescriptorDataArray[descIndex].mCount = arrayCount;
		pManager->pDescriptorDataArray[descIndex].pOffsets = pParam->pOffsets;
		switch (pDesc->mDesc.type)
		{
			case DESCRIPTOR_TYPE_RW_TEXTURE:
			case DESCRIPTOR_TYPE_TEXTURE:
				if (!pParam->ppTextures)
				{
					LOGERRORF("Texture descriptor (%s) is NULL", pParam->pName);
					return;
				}
				pManager->pDescriptorDataArray[descIndex].ppTextures = pParam->ppTextures;
				break;
			case DESCRIPTOR_TYPE_SAMPLER:
				if (!pParam->ppSamplers)
				{
					LOGERRORF("Sampler descriptor (%s) is NULL", pParam->pName);
					return;
				}
				pManager->pDescriptorDataArray[descIndex].ppSamplers = pParam->ppSamplers;
				break;
			case DESCRIPTOR_TYPE_ROOT_CONSTANT:
				if (!pParam->pRootConstant)
				{
					LOGERRORF("RootConstant array (%s) is NULL", pParam->pName);
					return;
				}
				pManager->pDescriptorDataArray[descIndex].pRootConstant = pParam->pRootConstant;
				break;
			case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case DESCRIPTOR_TYPE_RW_BUFFER:
			case DESCRIPTOR_TYPE_BUFFER:
				if (!pParam->ppBuffers)
				{
					LOGERRORF("Buffer descriptor (%s) is NULL", pParam->pName);
					return;
				}
				pManager->pDescriptorDataArray[descIndex].ppBuffers = pParam->ppBuffers;

				// In case we're binding an argument buffer, signal that we need to re-encode the resources into the buffer.
				if (arrayCount > 1 && pManager->mArgumentBuffers.find(hash).node)
					pManager->mArgumentBuffers[hash].second = true;

				break;
			default: break;
		}

		// Mark this descriptor as unbound, so it's values are updated.
		pManager->pBoundDescriptors[descIndex] = false;
	}

	// If we're binding descriptors for a compute pipeline, we must ensure that we have a correct compute enconder recording commands.
	if (pCmd->pBoundRootSignature->mPipelineType == PIPELINE_TYPE_COMPUTE && !pCmd->mtlComputeEncoder)
	{
		util_end_current_encoders(pCmd);
		pCmd->mtlComputeEncoder = [pCmd->mtlCommandBuffer computeCommandEncoder];
	}

	// Bind all the unbound root signature descriptors.
	for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
	{
		const DescriptorInfo* descriptorInfo = &pRootSignature->pDescriptors[i];
		const DescriptorData* descriptorData = &pManager->pDescriptorDataArray[i];

		if (!pManager->pBoundDescriptors[i])
		{
			ShaderStage usedStagesMask = descriptorInfo->mDesc.used_stages;
			switch (descriptorInfo->mDesc.type)
			{
				case DESCRIPTOR_TYPE_RW_TEXTURE:
				{
					uint32_t textureCount = max(1U, descriptorData->mCount);
					for (uint32_t j = 0; j < textureCount; j++)
					{
						if (!descriptorData->ppTextures[j] || !descriptorData->ppTextures[j]->mtlTexture)
						{
							LOGERRORF("RW Texture descriptor (%s) at array index (%u) is NULL", descriptorData->pName, j);
							return;
						}

						if ((usedStagesMask & SHADER_STAGE_VERT) != 0)
							[pCmd->mtlRenderEncoder
								setVertexTexture:descriptorData->ppTextures[j]->pMtlUAVDescriptors[descriptorData->mUAVMipSlice]
										 atIndex:descriptorInfo->mDesc.reg + j];
						if ((usedStagesMask & SHADER_STAGE_FRAG) != 0)
						{
							[pCmd->mtlRenderEncoder
								setFragmentTexture:descriptorData->ppTextures[j]->pMtlUAVDescriptors[descriptorData->mUAVMipSlice]
										   atIndex:descriptorInfo->mDesc.reg + j];
						}
						if ((usedStagesMask & SHADER_STAGE_COMP) != 0)
						{
							[pCmd->mtlComputeEncoder
								setTexture:descriptorData->ppTextures[j]->pMtlUAVDescriptors[descriptorData->mUAVMipSlice]
								   atIndex:descriptorInfo->mDesc.reg + j];
						}
					}
					break;
				}
				case DESCRIPTOR_TYPE_TEXTURE:
				{
					uint32_t textureCount = max(1U, descriptorData->mCount);
					for (uint32_t j = 0; j < textureCount; j++)
					{
						if (!descriptorData->ppTextures[j] || !descriptorData->ppTextures[j]->mtlTexture)
						{
							LOGERRORF("Texture descriptor (%s) at array index (%u) is NULL", descriptorData->pName, j);
							return;
						}

						if ((usedStagesMask & SHADER_STAGE_VERT) != 0)
							[pCmd->mtlRenderEncoder setVertexTexture:descriptorData->ppTextures[j]->mtlTexture
															 atIndex:descriptorInfo->mDesc.reg + j];
						if ((usedStagesMask & SHADER_STAGE_FRAG) != 0)
						{
							[pCmd->mtlRenderEncoder setFragmentTexture:descriptorData->ppTextures[j]->mtlTexture
															   atIndex:descriptorInfo->mDesc.reg + j];
						}
						if ((usedStagesMask & SHADER_STAGE_COMP) != 0)
						{
							[pCmd->mtlComputeEncoder setTexture:descriptorData->ppTextures[j]->mtlTexture
														atIndex:descriptorInfo->mDesc.reg + j];
						}
					}
					break;
				}
				case DESCRIPTOR_TYPE_SAMPLER:
				{
					uint32_t samplerCount = max(1U, descriptorData->mCount);
					for (uint32_t j = 0; j < samplerCount; j++)
					{
						if (!descriptorData->ppSamplers[j] || !descriptorData->ppSamplers[j]->mtlSamplerState)
						{
							LOGERRORF("Texture descriptor (%s) at array index (%u) is NULL", descriptorData->pName, j);
							return;
						}

						if ((usedStagesMask & SHADER_STAGE_VERT) != 0)
							[pCmd->mtlRenderEncoder setVertexSamplerState:descriptorData->ppSamplers[j]->mtlSamplerState
																  atIndex:descriptorInfo->mDesc.reg + j];
						if ((usedStagesMask & SHADER_STAGE_FRAG) != 0)
							[pCmd->mtlRenderEncoder setFragmentSamplerState:descriptorData->ppSamplers[j]->mtlSamplerState
																	atIndex:descriptorInfo->mDesc.reg + j];
						if ((usedStagesMask & SHADER_STAGE_COMP) != 0)
							[pCmd->mtlComputeEncoder setSamplerState:descriptorData->ppSamplers[j]->mtlSamplerState
															 atIndex:descriptorInfo->mDesc.reg + j];
					}
					break;
				}
				case DESCRIPTOR_TYPE_ROOT_CONSTANT:
					if ((usedStagesMask & SHADER_STAGE_VERT) != 0)
						[pCmd->mtlRenderEncoder setVertexBytes:descriptorData->pRootConstant
														length:descriptorInfo->mDesc.size
													   atIndex:descriptorInfo->mDesc.reg];
					if ((usedStagesMask & SHADER_STAGE_FRAG) != 0)
						[pCmd->mtlRenderEncoder setFragmentBytes:descriptorData->pRootConstant
														  length:descriptorInfo->mDesc.size
														 atIndex:descriptorInfo->mDesc.reg];
					if ((usedStagesMask & SHADER_STAGE_COMP) != 0)
						[pCmd->mtlComputeEncoder setBytes:descriptorData->pRootConstant
												   length:descriptorInfo->mDesc.size
												  atIndex:descriptorInfo->mDesc.reg];
					break;
				case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				case DESCRIPTOR_TYPE_RW_BUFFER:
				case DESCRIPTOR_TYPE_BUFFER:
				{
					// If we're trying to bind a buffer with an mCount > 1, it means we're binding many descriptors into an argument buffer.
					if (descriptorData->mCount > 1)
					{
						util_bind_argument_buffer(pCmd, pManager, descriptorInfo, descriptorData);
					}
					else
					{
						if ((usedStagesMask & SHADER_STAGE_VERT) != 0)
							[pCmd->mtlRenderEncoder setVertexBuffer:descriptorData->ppBuffers[0]->mtlBuffer
															 offset:(descriptorData->ppBuffers[0]->mPositionInHeap +
																	 (descriptorData->pOffsets ? descriptorData->pOffsets[0] : 0))
															atIndex:descriptorInfo->mDesc.reg];
						if ((usedStagesMask & SHADER_STAGE_FRAG) != 0)
							[pCmd->mtlRenderEncoder setFragmentBuffer:descriptorData->ppBuffers[0]->mtlBuffer
															   offset:(descriptorData->ppBuffers[0]->mPositionInHeap +
																	   (descriptorData->pOffsets ? descriptorData->pOffsets[0] : 0))
															  atIndex:descriptorInfo->mDesc.reg];
						if ((usedStagesMask & SHADER_STAGE_COMP) != 0)
							[pCmd->mtlComputeEncoder setBuffer:descriptorData->ppBuffers[0]->mtlBuffer
														offset:(descriptorData->ppBuffers[0]->mPositionInHeap +
																(descriptorData->pOffsets ? descriptorData->pOffsets[0] : 0))
													   atIndex:descriptorInfo->mDesc.reg];
					}
					break;
				}
				default: break;
			}
			pManager->pBoundDescriptors[i] = true;
		}
	}

	// We need to bind static samplers manually since Metal API has no concept of static samplers
	if (!pManager->mBoundStaticSamplers)
	{
		pManager->mBoundStaticSamplers = true;

		for (uint32_t i = 0; i < pRootSignature->mStaticSamplerCount; ++i)
		{
			ShaderStage usedStagesMask = pRootSignature->pStaticSamplerStages[i];
			Sampler*    pSampler = pRootSignature->ppStaticSamplers[i];
			uint32_t    reg = pRootSignature->pStaticSamplerSlots[i];
			if ((usedStagesMask & SHADER_STAGE_VERT) != 0)
				[pCmd->mtlRenderEncoder setVertexSamplerState:pSampler->mtlSamplerState atIndex:reg];
			if ((usedStagesMask & SHADER_STAGE_FRAG) != 0)
				[pCmd->mtlRenderEncoder setFragmentSamplerState:pSampler->mtlSamplerState atIndex:reg];
			if ((usedStagesMask & SHADER_STAGE_COMP) != 0)
				[pCmd->mtlComputeEncoder setSamplerState:pSampler->mtlSamplerState atIndex:reg];
		}
	}
}

/************************************************************************/
// Logging
/************************************************************************/
// Proxy log callback
static void internal_log(LogType type, const char* msg, const char* component)
{
	switch (type)
	{
		case LOG_TYPE_INFO: LOGINFOF("%s ( %s )", component, msg); break;
		case LOG_TYPE_WARN: LOGWARNINGF("%s ( %s )", component, msg); break;
		case LOG_TYPE_DEBUG: LOGDEBUGF("%s ( %s )", component, msg); break;
		case LOG_TYPE_ERROR: LOGERRORF("%s ( %s )", component, msg); break;
		default: break;
	}
}

// Resource allocation statistics.
void calculateMemoryStats(Renderer* pRenderer, char** stats) { resourceAllocBuildStatsString(pRenderer->pResourceAllocator, stats, 0); }
void freeMemoryStats(Renderer* pRenderer, char* stats) { resourceAllocFreeStatsString(pRenderer->pResourceAllocator, stats); }

/************************************************************************/
// Create default resources to be used a null descriptors in case user does not specify some descriptors
/************************************************************************/
void create_default_resources(Renderer* pRenderer)
{
	TextureDesc texture1DDesc = {};
	texture1DDesc.mArraySize = 1;
	texture1DDesc.mDepth = 1;
	texture1DDesc.mFormat = ImageFormat::R8;
	texture1DDesc.mHeight = 1;
	texture1DDesc.mMipLevels = 1;
	texture1DDesc.mSampleCount = SAMPLE_COUNT_1;
	texture1DDesc.mStartState = RESOURCE_STATE_COMMON;
	texture1DDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
	texture1DDesc.mWidth = 2;
	addTexture(pRenderer, &texture1DDesc, &pDefault1DTexture);

	TextureDesc texture1DArrayDesc = texture1DDesc;
	texture1DArrayDesc.mArraySize = 2;
	addTexture(pRenderer, &texture1DArrayDesc, &pDefault1DTextureArray);

	TextureDesc texture2DDesc = texture1DDesc;
	texture2DDesc.mHeight = 2;
	addTexture(pRenderer, &texture2DDesc, &pDefault2DTexture);

	TextureDesc texture2DArrayDesc = texture2DDesc;
	texture2DArrayDesc.mArraySize = 2;
	addTexture(pRenderer, &texture2DArrayDesc, &pDefault2DTextureArray);

	TextureDesc texture3DDesc = texture2DDesc;
	texture3DDesc.mDepth = 2;
	addTexture(pRenderer, &texture3DDesc, &pDefault3DTexture);

	TextureDesc textureCubeDesc = texture2DDesc;
	textureCubeDesc.mArraySize = 6;
	textureCubeDesc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE_CUBE;
	addTexture(pRenderer, &textureCubeDesc, &pDefaultCubeTexture);

	TextureDesc textureCubeArrayDesc = textureCubeDesc;
	textureCubeArrayDesc.mArraySize *= 2;
#ifndef TARGET_IOS
	addTexture(pRenderer, &textureCubeArrayDesc, &pDefaultCubeTextureArray);
#else
	if ([pRenderer->pDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v1])
		addTexture(pRenderer, &textureCubeArrayDesc, &pDefaultCubeTextureArray);
#endif

	BufferDesc bufferDesc = {};
	bufferDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	bufferDesc.mStartState = RESOURCE_STATE_COMMON;
	bufferDesc.mSize = sizeof(uint32_t);
	bufferDesc.mFirstElement = 0;
	bufferDesc.mElementCount = 1;
	bufferDesc.mStructStride = sizeof(uint32_t);
	addBuffer(pRenderer, &bufferDesc, &pDefaultBuffer);

	SamplerDesc samplerDesc = {};
	samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_BORDER;
	addSampler(pRenderer, &samplerDesc, &pDefaultSampler);

	BlendStateDesc blendStateDesc = {};
	blendStateDesc.mDstAlphaFactors[0] = BC_ZERO;
	blendStateDesc.mDstFactors[0] = BC_ZERO;
	blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
	blendStateDesc.mSrcFactors[0] = BC_ONE;
	blendStateDesc.mMasks[0] = ALL;
	blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
	blendStateDesc.mIndependentBlend = false;
	addBlendState(pRenderer, &blendStateDesc, &pDefaultBlendState);

	DepthStateDesc depthStateDesc = {};
	depthStateDesc.mDepthFunc = CMP_LEQUAL;
	depthStateDesc.mDepthTest = false;
	depthStateDesc.mDepthWrite = true;
	depthStateDesc.mStencilBackFunc = CMP_ALWAYS;
	depthStateDesc.mStencilFrontFunc = CMP_ALWAYS;
	depthStateDesc.mStencilReadMask = 0xFF;
	depthStateDesc.mStencilWriteMask = 0xFF;
	addDepthState(pRenderer, &depthStateDesc, &pDefaultDepthState);

	RasterizerStateDesc rasterizerStateDesc = {};
	rasterizerStateDesc.mCullMode = CULL_MODE_BACK;
	addRasterizerState(pRenderer, &rasterizerStateDesc, &pDefaultRasterizerState);
}

void destroy_default_resources(Renderer* pRenderer)
{
	removeTexture(pRenderer, pDefault1DTexture);
	removeTexture(pRenderer, pDefault1DTextureArray);
	removeTexture(pRenderer, pDefault2DTexture);
	removeTexture(pRenderer, pDefault2DTextureArray);
	removeTexture(pRenderer, pDefault3DTexture);
	removeTexture(pRenderer, pDefaultCubeTexture);
	removeTexture(pRenderer, pDefaultCubeTextureArray);

	removeBuffer(pRenderer, pDefaultBuffer);
	removeSampler(pRenderer, pDefaultSampler);

	removeBlendState(pDefaultBlendState);
	removeDepthState(pDefaultDepthState);
	removeRasterizerState(pDefaultRasterizerState);
}

// -------------------------------------------------------------------------------------------------
// API functions
// -------------------------------------------------------------------------------------------------

ImageFormat::Enum getRecommendedSwapchainFormat(bool hintHDR) { return ImageFormat::BGRA8; }
#ifndef TARGET_IOS
// Returns the CFDictionary that contains the system profiler data type described in inDataType.
CFDictionaryRef findDictionaryForDataType(const CFArrayRef inArray, CFStringRef inDataType)
{
	UInt8           i;
	CFDictionaryRef theDictionary;

	// Search the array of dictionaries for a CFDictionary that matches
	for (i = 0; i < CFArrayGetCount(inArray); i++)
	{
		theDictionary = (CFDictionaryRef)CFArrayGetValueAtIndex(inArray, i);

		// If the CFDictionary at this index has a key/value pair with the value equal to inDataType, retain and return it.
		if (CFDictionaryContainsValue(theDictionary, inDataType))
		{
			return (theDictionary);
		}
	}

	return (NULL);
}

// Returns the CFArray of “item” dictionaries.
CFArrayRef getItemsArrayFromDictionary(const CFDictionaryRef inDictionary)
{
	CFArrayRef itemsArray;

	// Retrieve the CFDictionary that has a key/value pair with the key equal to “_items”.
	itemsArray = (CFArrayRef)CFDictionaryGetValue(inDictionary, CFSTR("_items"));
	if (itemsArray != NULL)
		CFRetain(itemsArray);

	return (itemsArray);
}
//Used to call system profiler to retrieve GPU information such as vendor id and model id
void retrieveSystemProfilerInformation(tinystl::string& outVendorId)
{
	FILE*           sys_profile;
	size_t          bytesRead = 0;
	char            streamBuffer[1024 * 512];
	UInt8           i = 0;
	CFDataRef       xmlData;
	CFDictionaryRef hwInfoDict;
	CFArrayRef      itemsArray;
	CFIndex         arrayCount;

	// popen will fork and invoke the system_profiler command and return a stream reference with its result data
	sys_profile = popen("system_profiler SPDisplaysDataType -xml", "r");
	// Read the stream into a memory buffer
	bytesRead = fread(streamBuffer, sizeof(char), sizeof(streamBuffer), sys_profile);
	// Close the stream
	pclose(sys_profile);
	// Create a CFDataRef with the xml data
	xmlData = CFDataCreate(kCFAllocatorDefault, (UInt8*)streamBuffer, bytesRead);
	// CFPropertyListCreateFromXMLData reads in the XML data and will parse it into a CFArrayRef for us.
	CFStringRef errorString;
	//read xml data
	CFArrayRef propertyArray =
		((CFArrayRef)CFPropertyListCreateWithData(kCFAllocatorDefault, xmlData, kCFPropertyListImmutable, NULL, (CFErrorRef*)&errorString));

	// This will be the dictionary that contains all the Hardware information that system_profiler knows about.
	hwInfoDict = findDictionaryForDataType(propertyArray, CFSTR("SPDisplaysDataType"));
	if (hwInfoDict != NULL)
	{
		itemsArray = getItemsArrayFromDictionary(hwInfoDict);

		if (itemsArray != NULL)
		{
			// Find out how many items in this category – each one is a dictionary
			arrayCount = CFArrayGetCount(itemsArray);

			for (i = 0; i < arrayCount; i++)
			{
				CFMutableStringRef outputString;

				// Create a mutable CFStringRef with the dictionary value found with key “machine_name”
				// This is the machine_name of this mac machine.
				// Here you can give any value in key tag,to get its corresponding content
				outputString = CFStringCreateMutableCopy(
					kCFAllocatorDefault, 0,
					(CFStringRef)CFDictionaryGetValue(
						(CFDictionaryRef)CFArrayGetValueAtIndex(itemsArray, i), CFSTR("spdisplays_device-id")));
				NSString* outNS = (__bridge NSString*)outputString;
				outVendorId = [outNS.lowercaseString UTF8String];
				//your code here
				//(you can append output string OR modify your function according to your need )
				CFRelease(outputString);
			}

			CFRelease(itemsArray);
		}
		hwInfoDict = nil;
	}
	CFRelease(xmlData);
	CFRelease(propertyArray);
}
//Used to go through the given registry ID for the select device.
//Multiple id's can be found so they get filtered using the inModel id that was taken
//from system profile
void displayGraphicsInfo(uint64_t regId, tinystl::string inModel, GPUVendorPreset& vendorVecOut)
{
	// Get dictionary of all the PCI Devices
	//CFMutableDictionaryRef matchDict = IOServiceMatching("IOPCIDevice");
	CFMutableDictionaryRef matchDict = IORegistryEntryIDMatching(regId);
	// Create an iterator
	io_iterator_t iterator;

	if (IOServiceGetMatchingServices(kIOMasterPortDefault, matchDict, &iterator) == kIOReturnSuccess)
	{
		// Iterator for devices found
		io_registry_entry_t regEntry;

		while ((regEntry = IOIteratorNext(iterator)))
		{
			// Put this services object into a dictionary object.
			CFMutableDictionaryRef serviceDictionary;
			if (IORegistryEntryCreateCFProperties(regEntry, &serviceDictionary, kCFAllocatorDefault, kNilOptions) != kIOReturnSuccess)
			{
				// Service dictionary creation failed.
				IOObjectRelease(regEntry);
				continue;
			}
			NSString* ioPCIMatch = nil;
			//on macbook IOPCIPrimaryMatch is used
			if (CFDictionaryContainsKey(serviceDictionary, CFSTR("IOPCIPrimaryMatch")))
			{
				ioPCIMatch = (NSString*)CFDictionaryGetValue(serviceDictionary, CFSTR("IOPCIPrimaryMatch"));
			}
			else
			{
				//on iMac IOPCIMatch is used
				ioPCIMatch = (NSString*)CFDictionaryGetValue(serviceDictionary, CFSTR("IOPCIMatch"));
			}

			if (ioPCIMatch)
			{
				//get list of vendors from PCI Match above
				//this is a reflection of the display kext
				NSArray* vendors = [ioPCIMatch componentsSeparatedByString:@" "];
				for (id vendor in vendors)
				{
					NSString* modelId = [vendor substringToIndex:6];
					NSString* vendorId = [vendor substringFromIndex:6];
					vendorId = [@"0x" stringByAppendingString:vendorId];
					tinystl::string modelIdString = [modelId.lowercaseString UTF8String];
					tinystl::string vendorIdString = [vendorId.lowercaseString UTF8String];
					//filter out unwated model id's
					if (modelIdString != inModel)
						continue;

					strncpy(vendorVecOut.mModelId, modelIdString.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);
					strncpy(vendorVecOut.mVendorId, vendorIdString.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);
					vendorVecOut.mPresetLevel = GPUPresetLevel::GPU_PRESET_LOW;
					break;
				}
			}

			// Release the dictionary
			CFRelease(serviceDictionary);
			// Release the serviceObject
			IOObjectRelease(regEntry);
		}
		// Release the iterator
		IOObjectRelease(iterator);
	}
}
#endif
uint32_t queryThreadExecutionWidth(Renderer* pRenderer)
{
	if (!pRenderer)
		return 0;

	NSError*  error = nil;
	NSString* defaultComputeShader =
		@"#include <metal_stdlib>\n"
		 "using namespace metal;\n"
		 "kernel void simplest(texture2d<float, access::write> output [[texture(0)]],uint2 gid [[thread_position_in_grid]])\n"
		 "{output.write(float4(0, 0, 0, 1), gid);}";

	// Load all the shader files with a .metal file extension in the project
	id<MTLLibrary> defaultLibrary = [pRenderer->pDevice newLibraryWithSource:defaultComputeShader options:nil error:&error];

	if (error != nil)
	{
		LOGWARNINGF("Could not create library for simple compute shader: %s", [[error localizedDescription] UTF8String]);
		return 0;
	}

	// Load the kernel function from the library
	id<MTLFunction> kernelFunction = [defaultLibrary newFunctionWithName:@"simplest"];

	// Create a compute pipeline state
	id<MTLComputePipelineState> computePipelineState = [pRenderer->pDevice newComputePipelineStateWithFunction:kernelFunction error:&error];
	if (error != nil)
	{
		LOGWARNINGF("Could not create compute pipeline state for simple compute shader: %s", [[error localizedDescription] UTF8String]);
		return 0;
	}

	return (uint32_t)computePipelineState.threadExecutionWidth;
}

void initRenderer(const char* appName, const RendererDesc* settings, Renderer** ppRenderer)
{
	Renderer* pRenderer = (Renderer*)conf_calloc(1, sizeof(*pRenderer));
	ASSERT(pRenderer);

	pRenderer->pName = (char*)conf_calloc(strlen(appName) + 1, sizeof(char));
	memcpy(pRenderer->pName, appName, strlen(appName));

	// Copy settings
	memcpy(&(pRenderer->mSettings), settings, sizeof(*settings));
	pRenderer->mSettings.mApi = RENDERER_API_METAL;

	// Initialize the Metal bits
	{
		// Get the systems default device.
		pRenderer->pDevice = MTLCreateSystemDefaultDevice();

		//get gpu vendor and model id.
		GPUVendorPreset gpuVendor;
		gpuVendor.mPresetLevel = GPUPresetLevel::GPU_PRESET_LOW;
#ifndef TARGET_IOS
		tinystl::string outModelId;
		retrieveSystemProfilerInformation(outModelId);
		displayGraphicsInfo(pRenderer->pDevice.registryID, outModelId, gpuVendor);
		tinystl::string mDeviceName = [pRenderer->pDevice.name UTF8String];
		strncpy(gpuVendor.mGpuName, mDeviceName.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);
		LOGINFOF("Current Gpu Name: %s", gpuVendor.mGpuName);
		LOGINFOF("Current Gpu Vendor ID: %s", gpuVendor.mVendorId);
		LOGINFOF("Current Gpu Model ID: %s", gpuVendor.mModelId);
#else
		strncpy(gpuVendor.mVendorId, "Apple", MAX_GPU_VENDOR_STRING_LENGTH);
		strncpy(gpuVendor.mModelId, "iOS", MAX_GPU_VENDOR_STRING_LENGTH);

#endif
		// Set the default GPU settings.
		pRenderer->mNumOfGPUs = 1;
		pRenderer->mGpuSettings[0].mMaxVertexInputBindings =
			MAX_VERTEX_BINDINGS;    // there are no special vertex buffers for input in Metal, only regular buffers
		pRenderer->mGpuSettings[0].mUniformBufferAlignment = 256;
		pRenderer->mGpuSettings[0].mMultiDrawIndirect =
			false;    // multi draw indirect is not supported on Metal: only single draw indirect
		pRenderer->mGpuSettings[0].mGpuVendorPreset = gpuVendor;
		pRenderer->pActiveGpuSettings = &pRenderer->mGpuSettings[0];
		pRenderer->mGpuSettings[0].mROVsSupported = [pRenderer->pDevice areRasterOrderGroupsSupported];
		pRenderer->mGpuSettings[0].mWaveLaneCount = queryThreadExecutionWidth(pRenderer);
#ifndef TARGET_IOS
		setGPUPresetLevel(pRenderer);
		//exit app if gpu being used has an office preset.
		if (pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel < GPU_PRESET_LOW)
		{
			ASSERT(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel >= GPU_PRESET_LOW);

			//remove allocated name
			SAFE_FREE(pRenderer->pName);
			//set device to null
			pRenderer->pDevice = nil;
			//remove allocated renderer
			SAFE_FREE(pRenderer);

			LOGERROR("Selected GPU has an office Preset in gpu.cfg");
			LOGERROR("Office Preset is not supported by the Forge");

			ppRenderer = NULL;
#ifdef AUTOMATED_TESTING
			//exit with success return code not to show failure on Jenkins
			exit(0);
#endif
			return;
		}
#endif
		// Create a resource allocator.
		AllocatorCreateInfo info = { 0 };
		info.device = pRenderer->pDevice;
		//info.physicalDevice = pRenderer->pActiveGPU;
		createAllocator(&info, &pRenderer->pResourceAllocator);

		// Create default resources.
		create_default_resources(pRenderer);

		// Renderer is good! Assign it to result!
		*(ppRenderer) = pRenderer;
	}
}

void removeRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);
	SAFE_FREE(pRenderer->pName);
	destroyAllocator(pRenderer->pResourceAllocator);
	pRenderer->pDevice = nil;
	SAFE_FREE(pRenderer);
}

void addFence(Renderer* pRenderer, Fence** ppFence)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);

	Fence* pFence = (Fence*)conf_calloc(1, sizeof(*pFence));
	ASSERT(pFence);

	pFence->pMtlSemaphore = dispatch_semaphore_create(0);
	pFence->mSubmitted = false;

	*ppFence = pFence;
}
void removeFence(Renderer* pRenderer, Fence* pFence)
{
	ASSERT(pFence);
	SAFE_FREE(pFence);
}

void addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
	ASSERT(pRenderer);

	Semaphore* pSemaphore = (Semaphore*)conf_calloc(1, sizeof(*pSemaphore));
	ASSERT(pSemaphore);

	pSemaphore->pMtlSemaphore = dispatch_semaphore_create(0);

	*ppSemaphore = pSemaphore;
}
void removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
	ASSERT(pSemaphore);
	SAFE_FREE(pSemaphore);
}

void addQueue(Renderer* pRenderer, QueueDesc* pQDesc, Queue** ppQueue)
{
	ASSERT(pQDesc);

	Queue* pQueue = (Queue*)conf_calloc(1, sizeof(*pQueue));
	ASSERT(pQueue);

	pQueue->pRenderer = pRenderer;
	pQueue->mtlCommandQueue = [pRenderer->pDevice newCommandQueue];
	pQueue->pMtlSemaphore = dispatch_semaphore_create(0);

	ASSERT(pQueue->mtlCommandQueue != nil);

	*ppQueue = pQueue;
}
void removeQueue(Queue* pQueue)
{
	ASSERT(pQueue);
	pQueue->mtlCommandQueue = nil;
	SAFE_FREE(pQueue);
}

void addCmdPool(Renderer* pRenderer, Queue* pQueue, bool transient, CmdPool** ppCmdPool)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);

	CmdPool* pCmdPool = (CmdPool*)conf_calloc(1, sizeof(*pCmdPool));
	ASSERT(pCmdPool);

	pCmdPool->mCmdPoolDesc = { pQueue->mQueueDesc.mType };
	pCmdPool->pQueue = pQueue;

	*ppCmdPool = pCmdPool;
}
void removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	ASSERT(pCmdPool);
	SAFE_FREE(pCmdPool);
}

void addCmd(CmdPool* pCmdPool, bool secondary, Cmd** ppCmd)
{
	ASSERT(pCmdPool);
	ASSERT(pCmdPool->pQueue->pRenderer->pDevice != nil);

	Cmd* pCmd = (Cmd*)conf_calloc(1, sizeof(*pCmd));
	ASSERT(pCmd);

	pCmd->pRenderer = pCmdPool->pQueue->pRenderer;
	pCmd->pCmdPool = pCmdPool;
	pCmd->mtlEncoderFence = [pCmd->pRenderer->pDevice newFence];

	if (pCmdPool->mCmdPoolDesc.mCmdPoolType == CMD_POOL_DIRECT)
	{
		pCmd->pBoundColorFormats = (uint32_t*)conf_calloc(MAX_RENDER_TARGET_ATTACHMENTS, sizeof(uint32_t));
		pCmd->pBoundSrgbValues = (bool*)conf_calloc(MAX_RENDER_TARGET_ATTACHMENTS, sizeof(bool));
	}

	*ppCmd = pCmd;
}
void removeCmd(CmdPool* pCmdPool, Cmd* pCmd)
{
	ASSERT(pCmd);
	pCmd->mtlEncoderFence = nil;
	pCmd->mtlCommandBuffer = nil;

	if (pCmd->pBoundColorFormats)
		SAFE_FREE(pCmd->pBoundColorFormats);

	if (pCmd->pBoundSrgbValues)
		SAFE_FREE(pCmd->pBoundSrgbValues);

	SAFE_FREE(pCmd);
}

void addCmd_n(CmdPool* pCmdPool, bool secondary, uint32_t cmdCount, Cmd*** pppCmd)
{
	ASSERT(pppCmd);

	Cmd** ppCmd = (Cmd**)conf_calloc(cmdCount, sizeof(*ppCmd));
	ASSERT(ppCmd);

	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		addCmd(pCmdPool, secondary, &(ppCmd[i]));
	}

	*pppCmd = ppCmd;
}
void removeCmd_n(CmdPool* pCmdPool, uint32_t cmdCount, Cmd** ppCmd)
{
	ASSERT(ppCmd);

	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		removeCmd(pCmdPool, ppCmd[i]);
	}

	SAFE_FREE(ppCmd);
}

void toggleVSync(Renderer* pRenderer, SwapChain** pSwapchain)
{
#if !defined(TARGET_IOS)
	(*pSwapchain)->mDesc.mEnableVsync = !(*pSwapchain)->mDesc.mEnableVsync;
	//no need to have vsync on layers otherwise we will wait on semaphores
	//get a copy of the layer for nextDrawables
	CAMetalLayer* layer = (CAMetalLayer*)(*pSwapchain)->pMTKView.layer;

	//only available on mac OS.
	//VSync seems to be necessary on iOS.
	if (!(*pSwapchain)->mDesc.mEnableVsync)
	{
		(*pSwapchain)->pMTKView.enableSetNeedsDisplay = YES;
		(*pSwapchain)->pMTKView.paused = YES;
		layer.displaySyncEnabled = false;
	}
	else
	{
		(*pSwapchain)->pMTKView.enableSetNeedsDisplay = NO;
		(*pSwapchain)->pMTKView.paused = NO;
		layer.displaySyncEnabled = true;
	}
#endif
}

void addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppSwapChain);

	SwapChain* pSwapChain = (SwapChain*)conf_calloc(1, sizeof(*pSwapChain));
	pSwapChain->mDesc = *pDesc;

	// Assign MTKView to the swapchain.
	pSwapChain->pMTKView = (MTKView*)CFBridgingRelease(pDesc->pWindow->handle);
	pSwapChain->pMTKView.device = pRenderer->pDevice;
	pSwapChain->pMTKView.autoresizesSubviews = TRUE;
	pSwapChain->pMTKView.preferredFramesPerSecond = 60.0;
	pSwapChain->pMTKView.enableSetNeedsDisplay = NO;
	pSwapChain->pMTKView.paused = NO;

#if !defined(TARGET_IOS)
	//no need to have vsync on layers otherwise we will wait on semaphores
	//get a copy of the layer for nextDrawables
	CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pMTKView.layer;
	pSwapChain->pMTKView.layer = layer;

	//only available on mac OS.
	//VSync seems to be necessary on iOS.
	if (!pDesc->mEnableVsync)
	{
		pSwapChain->pMTKView.enableSetNeedsDisplay = YES;
		pSwapChain->pMTKView.paused = YES;

		//This needs to be set to false to have working non-vsync
		//otherwise present drawables will wait on vsync.
		layer.displaySyncEnabled = false;
	}
	else
		//This needs to be set to false to have working vsync
		layer.displaySyncEnabled = true;

	pSwapChain->pMTKView.wantsLayer = YES;
#endif
	pSwapChain->mMTKDrawable = nil;

	// Set the view pixel format to match the swapchain's pixel format.
	pSwapChain->pMTKView.colorPixelFormat = util_to_mtl_pixel_format(pSwapChain->mDesc.mColorFormat, pSwapChain->mDesc.mSrgb);

	// Create present command buffer for the swapchain.
	pSwapChain->presentCommandBuffer = [pSwapChain->mDesc.ppPresentQueues[0]->mtlCommandQueue commandBuffer];

	// Create the swapchain RT descriptor.
	RenderTargetDesc descColor = {};
	descColor.mWidth = pSwapChain->mDesc.mWidth;
	descColor.mHeight = pSwapChain->mDesc.mHeight;
	descColor.mDepth = 1;
	descColor.mArraySize = 1;
	descColor.mFormat = pSwapChain->mDesc.mColorFormat;
	descColor.mSrgb = pSwapChain->mDesc.mSrgb;
	descColor.mClearValue = pSwapChain->mDesc.mColorClearValue;
	descColor.mSampleCount = SAMPLE_COUNT_1;
	descColor.mSampleQuality = 0;

	pSwapChain->ppSwapchainRenderTargets =
		(RenderTarget**)conf_calloc(pSwapChain->mDesc.mImageCount, sizeof(*pSwapChain->ppSwapchainRenderTargets));
	for (uint32_t i = 0; i < pSwapChain->mDesc.mImageCount; ++i)
	{
		addRenderTarget(pRenderer, &descColor, &pSwapChain->ppSwapchainRenderTargets[i]);
	}

	*ppSwapChain = pSwapChain;
}

void removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pSwapChain);

	pSwapChain->presentCommandBuffer = nil;

	for (uint32_t i = 0; i < pSwapChain->mDesc.mImageCount; ++i)
		removeRenderTarget(pRenderer, pSwapChain->ppSwapchainRenderTargets[i]);

	SAFE_FREE(pSwapChain->ppSwapchainRenderTargets);
	SAFE_FREE(pSwapChain);
}

void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(pDesc->mSize > 0);
	ASSERT(pRenderer->pDevice != nil);

	Buffer* pBuffer = (Buffer*)conf_calloc(1, sizeof(*pBuffer));
	ASSERT(pBuffer);
	pBuffer->mDesc = *pDesc;

	// Align the buffer size to multiples of the dynamic uniform buffer minimum size
	if (pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
	{
		uint64_t minAlignment = pRenderer->pActiveGpuSettings->mUniformBufferAlignment;
		pBuffer->mDesc.mSize = round_up_64(pBuffer->mDesc.mSize, minAlignment);
	}

	//Use isLowPower to determine if running intel integrated gpu
	//There's currently an intel driver bug with placed resources so we need to create
	//new resources that are GPU only in their own memory space
	//0x8086 is intel vendor id
	if (strcmp(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mVendorId, "0x8086") == 0 &&
		(ResourceMemoryUsage)pBuffer->mDesc.mMemoryUsage & RESOURCE_MEMORY_USAGE_GPU_ONLY)
		pBuffer->mDesc.mFlags |= BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;

	// Get the proper memory requiremnets for the given buffer.
	AllocatorMemoryRequirements mem_reqs = { 0 };
	mem_reqs.usage = (ResourceMemoryUsage)pBuffer->mDesc.mMemoryUsage;
	mem_reqs.flags = 0;
	if (pBuffer->mDesc.mFlags & BUFFER_CREATION_FLAG_OWN_MEMORY_BIT)
		mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT;
	if (pBuffer->mDesc.mFlags & BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT)
		mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT;

	BufferCreateInfo alloc_info = { pBuffer->mDesc.mSize };
	bool             allocSuccess;
	allocSuccess = createBuffer(pRenderer->pResourceAllocator, &alloc_info, &mem_reqs, pBuffer);
	ASSERT(allocSuccess);

	pBuffer->mBufferId = (++gBufferIds << 8U) + util_pthread_to_uint64(Thread::GetCurrentThreadID());
	pBuffer->mCurrentState = pBuffer->mDesc.mStartState;

	// If buffer is a suballocation use offset in heap else use zero offset (placed resource / committed resource)
	if (pBuffer->pMtlAllocation->GetResource())
		pBuffer->mPositionInHeap = pBuffer->pMtlAllocation->GetOffset();
	else
		pBuffer->mPositionInHeap = 0;

	*ppBuffer = pBuffer;
}
void removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	ASSERT(pBuffer);
	destroyBuffer(pRenderer->pResourceAllocator, pBuffer);
	SAFE_FREE(pBuffer);
}

void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mWidth && pDesc->mHeight && (pDesc->mDepth || pDesc->mArraySize));
	if (pDesc->mSampleCount > SAMPLE_COUNT_1 && pDesc->mMipLevels > 1)
	{
		internal_log(LOG_TYPE_ERROR, "Multi-Sampled textures cannot have mip maps", "MetalRenderer");
		return;
	}
	add_texture(pRenderer, pDesc, ppTexture, false);
}

void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppRenderTarget);

	((RenderTargetDesc*)pDesc)->mMipLevels = max(1U, pDesc->mMipLevels);

	RenderTarget* pRenderTarget = (RenderTarget*)conf_calloc(1, sizeof(*pRenderTarget));
	pRenderTarget->mDesc = *pDesc;

	TextureDesc rtDesc = {};
	rtDesc.mFlags = pRenderTarget->mDesc.mFlags;
	rtDesc.mWidth = pRenderTarget->mDesc.mWidth;
	rtDesc.mHeight = pRenderTarget->mDesc.mHeight;
	rtDesc.mDepth = pRenderTarget->mDesc.mDepth;
	rtDesc.mArraySize = pRenderTarget->mDesc.mArraySize;
	rtDesc.mMipLevels = pRenderTarget->mDesc.mMipLevels;
	rtDesc.mSampleCount = pRenderTarget->mDesc.mSampleCount;
	rtDesc.mSampleQuality = pRenderTarget->mDesc.mSampleQuality;
	rtDesc.mFormat = pRenderTarget->mDesc.mFormat;
	rtDesc.mClearValue = pRenderTarget->mDesc.mClearValue;
	rtDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	rtDesc.mStartState = RESOURCE_STATE_UNDEFINED;
	rtDesc.pNativeHandle = pDesc->pNativeHandle;
	rtDesc.mSrgb = pRenderTarget->mDesc.mSrgb;
	rtDesc.mHostVisible = false;

	rtDesc.mDescriptors |= pDesc->mDescriptors;

#ifndef TARGET_IOS
	add_texture(pRenderer, &rtDesc, &pRenderTarget->pTexture, true);
#else
	if (pDesc->mFormat != ImageFormat::D24S8)
		add_texture(pRenderer, &rtDesc, &pRenderTarget->pTexture, true);
	// Combined depth stencil is not supported on iOS.
	else
	{
		rtDesc.mFormat = ImageFormat::D24;
		add_texture(pRenderer, &rtDesc, &pRenderTarget->pTexture, true);
		rtDesc.mFormat = ImageFormat::S8;
		add_texture(pRenderer, &rtDesc, &pRenderTarget->pStencil, true);
	}
#endif

	*ppRenderTarget = pRenderTarget;
}
void removeTexture(Renderer* pRenderer, Texture* pTexture)
{
	ASSERT(pTexture);

	// Destroy descriptors
	if (pTexture->mDesc.mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
	{
		for (uint32_t i = 0; i < pTexture->mDesc.mMipLevels; ++i)
		{
			pTexture->pMtlUAVDescriptors[i] = nil;
		}
	}

	destroyTexture(pRenderer->pResourceAllocator, pTexture);
	SAFE_FREE(pTexture->pMtlUAVDescriptors);
	SAFE_FREE(pTexture);
}
void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
	removeTexture(pRenderer, pRenderTarget->pTexture);
#ifdef TARGET_IOS
	if (pRenderTarget->pStencil)
		removeTexture(pRenderer, pRenderTarget->pStencil);
#endif
	SAFE_FREE(pRenderTarget);
}

void addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(pDesc->mCompareFunc < MAX_COMPARE_MODES);

	Sampler* pSampler = (Sampler*)conf_calloc(1, sizeof(*pSampler));
	ASSERT(pSampler);

	MTLSamplerDescriptor* samplerDesc = [[MTLSamplerDescriptor alloc] init];
	samplerDesc.minFilter = (pDesc->mMinFilter == FILTER_NEAREST ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear);
	samplerDesc.magFilter = (pDesc->mMagFilter == FILTER_NEAREST ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear);
	samplerDesc.mipFilter = (pDesc->mMipMapMode == MIPMAP_MODE_NEAREST ? MTLSamplerMipFilterNearest : MTLSamplerMipFilterLinear);
	samplerDesc.maxAnisotropy = (pDesc->mMaxAnisotropy == 0 ? 1 : pDesc->mMaxAnisotropy);    // 0 is not allowed in Metal
	samplerDesc.sAddressMode = gMtlAddressModeTranslator[pDesc->mAddressU];
	samplerDesc.tAddressMode = gMtlAddressModeTranslator[pDesc->mAddressV];
	samplerDesc.rAddressMode = gMtlAddressModeTranslator[pDesc->mAddressW];
	samplerDesc.compareFunction = gMtlComparisonFunctionTranslator[pDesc->mCompareFunc];

	pSampler->mtlSamplerState = [pRenderer->pDevice newSamplerStateWithDescriptor:samplerDesc];
	pSampler->mSamplerId = (++gSamplerIds << 8U) + util_pthread_to_uint64(Thread::GetCurrentThreadID());

	*ppSampler = pSampler;
}
void removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
	ASSERT(pSampler);
	pSampler->mtlSamplerState = nil;
	SAFE_FREE(pSampler);
}

void addShader(Renderer* pRenderer, const ShaderDesc* pDesc, Shader** ppShaderProgram)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(pRenderer->pDevice != nil);

	Shader* pShaderProgram = (Shader*)conf_calloc(1, sizeof(*pShaderProgram));
	pShaderProgram->mStages = pDesc->mStages;

	tinystl::unordered_map<uint32_t, MTLVertexFormat> vertexAttributeFormats;

	uint32_t         shaderReflectionCounter = 0;
	ShaderReflection stageReflections[SHADER_STAGE_COUNT];
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		tinystl::string              source = NULL;
		const char*                  entry_point = NULL;
		const char*                  shader_name = NULL;
		tinystl::vector<ShaderMacro> shader_macros;
		__strong id<MTLFunction>* compiled_code = nullptr;

		ShaderStage stage_mask = (ShaderStage)(1 << i);
		if (stage_mask == (pShaderProgram->mStages & stage_mask))
		{
			switch (stage_mask)
			{
				case SHADER_STAGE_VERT:
				{
					source = pDesc->mVert.mCode;
					entry_point = pDesc->mVert.mEntryPoint.c_str();
					shader_name = pDesc->mVert.mName.c_str();
					shader_macros = pDesc->mVert.mMacros;
					compiled_code = &(pShaderProgram->mtlVertexShader);
				}
				break;
				case SHADER_STAGE_FRAG:
				{
					source = pDesc->mFrag.mCode;
					entry_point = pDesc->mFrag.mEntryPoint.c_str();
					shader_name = pDesc->mFrag.mName.c_str();
					shader_macros = pDesc->mFrag.mMacros;
					compiled_code = &(pShaderProgram->mtlFragmentShader);
				}
				break;
				case SHADER_STAGE_COMP:
				{
					source = pDesc->mComp.mCode;
					entry_point = pDesc->mComp.mEntryPoint.c_str();
					shader_name = pDesc->mComp.mName.c_str();
					shader_macros = pDesc->mComp.mMacros;
					compiled_code = &(pShaderProgram->mtlComputeShader);
				}
				break;
				default: break;
			}

			// Create a NSDictionary for all the shader macros.
			NSNumberFormatter* numberFormatter =
				[[NSNumberFormatter alloc] init];    // Used for reading NSNumbers macro values from strings.
			numberFormatter.numberStyle = NSNumberFormatterDecimalStyle;

			NSArray* defArray = [[NSArray alloc] init];
			NSArray* valArray = [[NSArray alloc] init];
			for (uint i = 0; i < shader_macros.size(); i++)
			{
				defArray = [defArray arrayByAddingObject:[[NSString alloc] initWithUTF8String:shader_macros[i].definition]];

				// Try reading the macro value as a NSNumber. If failed, use it as an NSString.
				NSString* valueString = [[NSString alloc] initWithUTF8String:shader_macros[i].value];
				NSNumber* valueNumber = [numberFormatter numberFromString:valueString];
				if (valueNumber)
					valArray = [valArray arrayByAddingObject:valueNumber];
				else
					valArray = [valArray arrayByAddingObject:valueString];
			}
			NSDictionary* macroDictionary = [[NSDictionary alloc] initWithObjects:valArray forKeys:defArray];

			// Compile the code
			NSString* shaderSource = [[NSString alloc] initWithUTF8String:source.c_str()];
			NSError*  error = nil;

			MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
			options.preprocessorMacros = macroDictionary;
			id<MTLLibrary> lib = [pRenderer->pDevice newLibraryWithSource:shaderSource options:options error:&error];

			// Warning
			if (error)
			{
				if (lib)
				{
					LOGWARNINGF(
						"Loaded shader %s with the following warnings:\n %s", shader_name, [[error localizedDescription] UTF8String]);
					error = 0;    //  error string is an autorelease object.
				}
				// Error
				else
				{
					LOGERRORF(
						"Couldn't load shader %s with the following error:\n %s", shader_name, [[error localizedDescription] UTF8String]);
					error = 0;    //  error string is an autorelease object.
				}
			}

			if (lib)
			{
				NSString*       entryPointNStr = [[NSString alloc] initWithUTF8String:entry_point];
				id<MTLFunction> function = [lib newFunctionWithName:entryPointNStr];
				assert(function != nil && "Entry point not found in shader.");
				*compiled_code = function;
			}

			mtl_createShaderReflection(
				pRenderer, pShaderProgram, (const uint8_t*)source.c_str(), (uint32_t)source.size(), stage_mask, &vertexAttributeFormats,
				&stageReflections[shaderReflectionCounter++]);
		}
	}

	createPipelineReflection(stageReflections, shaderReflectionCounter, &pShaderProgram->mReflection);

	*ppShaderProgram = pShaderProgram;
}

void addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mStages);
	ASSERT(ppShaderProgram);

	Shader* pShaderProgram = (Shader*)conf_calloc(1, sizeof(*pShaderProgram));
	ASSERT(pShaderProgram);

	pShaderProgram->mStages = pDesc->mStages;

	tinystl::unordered_map<uint32_t, MTLVertexFormat> vertexAttributeFormats;

	uint32_t reflectionCount = 0;
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		ShaderStage                  stage_mask = (ShaderStage)(1 << i);
		const BinaryShaderStageDesc* pStage = NULL;
		__strong id<MTLFunction>* compiled_code = nullptr;

		if (stage_mask == (pShaderProgram->mStages & stage_mask))
		{
			switch (stage_mask)
			{
				case SHADER_STAGE_VERT:
				{
					pStage = &pDesc->mVert;
					compiled_code = &(pShaderProgram->mtlVertexShader);
				}
				break;
				case SHADER_STAGE_FRAG:
				{
					pStage = &pDesc->mFrag;
					compiled_code = &(pShaderProgram->mtlFragmentShader);
				}
				break;
				case SHADER_STAGE_COMP:
				{
					pStage = &pDesc->mComp;
					compiled_code = &(pShaderProgram->mtlComputeShader);
				}
				break;
				default: break;
			}

			// Create a MTLLibrary from bytecode.
			dispatch_data_t byteCode =
				dispatch_data_create(pStage->pByteCode, pStage->mByteCodeSize, nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
			id<MTLLibrary> lib = [pRenderer->pDevice newLibraryWithData:byteCode error:nil];

			// Create a MTLFunction from the loaded MTLLibrary.
			NSString*       entryPointNStr = [[NSString alloc] initWithUTF8String:pStage->mEntryPoint];
			id<MTLFunction> function = [lib newFunctionWithName:entryPointNStr];
			*compiled_code = function;

			mtl_createShaderReflection(
				pRenderer, pShaderProgram, (const uint8_t*)pStage->mSource.c_str(), (uint32_t)pStage->mSource.size(), stage_mask,
				&vertexAttributeFormats, &pShaderProgram->mReflection.mStageReflections[reflectionCount++]);
		}
	}

	createPipelineReflection(pShaderProgram->mReflection.mStageReflections, reflectionCount, &pShaderProgram->mReflection);

	*ppShaderProgram = pShaderProgram;
}

void removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
	ASSERT(pShaderProgram);
	pShaderProgram->mtlVertexShader = nil;
	pShaderProgram->mtlFragmentShader = nil;
	pShaderProgram->mtlComputeShader = nil;

	// free allocated resources during reflection.
	for (uint32_t i = 0; i < MAX_SHADER_STAGE_COUNT; i++)
	{
		SAFE_FREE(pShaderProgram->mReflection.mStageReflections[i].pNamePool);
		SAFE_FREE(pShaderProgram->mReflection.mStageReflections[i].pVertexInputs);
		SAFE_FREE(pShaderProgram->mReflection.mStageReflections[i].pShaderResources);
		SAFE_FREE(pShaderProgram->mReflection.mStageReflections[i].pVariables);
	}

	SAFE_FREE(pShaderProgram);
}

void addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);

	RootSignature*                         pRootSignature = (RootSignature*)conf_calloc(1, sizeof(*pRootSignature));
	tinystl::vector<ShaderResource const*> shaderResources;

	// Collect static samplers
	tinystl::vector<tinystl::pair<ShaderResource const*, Sampler*>> staticSamplers;
	tinystl::unordered_map<tinystl::string, Sampler*>               staticSamplerMap;
	for (uint32_t i = 0; i < pRootSignatureDesc->mStaticSamplerCount; ++i)
		staticSamplerMap.insert({ pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i] });

	conf_placement_new<tinystl::unordered_map<uint32_t, uint32_t>>(&pRootSignature->pDescriptorNameToIndexMap);

	// Collect all unique shader resources in the given shaders
	// Resources are parsed by name (two resources named "XYZ" in two shaders will be considered the same resource)
	for (uint32_t sh = 0; sh < pRootSignatureDesc->mShaderCount; ++sh)
	{
		PipelineReflection const* pReflection = &pRootSignatureDesc->ppShaders[sh]->mReflection;

		if (pReflection->mShaderStages & SHADER_STAGE_COMP)
			pRootSignature->mPipelineType = PIPELINE_TYPE_COMPUTE;
		else
			pRootSignature->mPipelineType = PIPELINE_TYPE_GRAPHICS;

		for (uint32_t i = 0; i < pReflection->mShaderResourceCount; ++i)
		{
			ShaderResource const* pRes = &pReflection->pShaderResources[i];

			// Find all unique resources
			tinystl::unordered_hash_node<uint32_t, uint32_t>* pNode =
				pRootSignature->pDescriptorNameToIndexMap.find(tinystl::hash(pRes->name)).node;
			if (!pNode)
			{
				if (pRes->type == DESCRIPTOR_TYPE_SAMPLER)
				{
					// If the sampler is a static sampler, no need to put it in the descriptor table
					const tinystl::unordered_hash_node<tinystl::string, Sampler*>* pNode = staticSamplerMap.find(pRes->name).node;

					if (pNode)
					{
						LOGINFOF("Descriptor (%s) : User specified Static Sampler", pRes->name);
						staticSamplers.push_back({ pRes, pNode->second });
					}
					else
					{
						pRootSignature->pDescriptorNameToIndexMap.insert({ tinystl::hash(pRes->name), (uint32_t)shaderResources.size() });
						shaderResources.emplace_back(pRes);
					}
				}
				else
				{
					pRootSignature->pDescriptorNameToIndexMap.insert({ tinystl::hash(pRes->name), (uint32_t)shaderResources.size() });
					shaderResources.emplace_back(pRes);
				}
			}
		}
	}

	if ((uint32_t)shaderResources.size())
	{
		pRootSignature->mDescriptorCount = (uint32_t)shaderResources.size();
		pRootSignature->pDescriptors = (DescriptorInfo*)conf_calloc(pRootSignature->mDescriptorCount, sizeof(DescriptorInfo));
	}

	// Fill the descriptor array to be stored in the root signature
	for (uint32_t i = 0; i < (uint32_t)shaderResources.size(); ++i)
	{
		DescriptorInfo*           pDesc = &pRootSignature->pDescriptors[i];
		ShaderResource const*     pRes = shaderResources[i];
		uint32_t                  setIndex = pRes->set;
		DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)setIndex;

		pDesc->mDesc.reg = pRes->reg;
		pDesc->mDesc.set = pRes->set;
		pDesc->mDesc.size = pRes->size;
		pDesc->mDesc.type = pRes->type;
		pDesc->mDesc.used_stages = pRes->used_stages;
		pDesc->mDesc.name_size = pRes->name_size;
		pDesc->mDesc.name = (const char*)conf_calloc(pDesc->mDesc.name_size + 1, sizeof(char));
		memcpy((char*)pDesc->mDesc.name, pRes->name, pRes->name_size);
		pDesc->mUpdateFrquency = updateFreq;

		// In case we're binding a texture, we need to specify the texture type so the bound resource type matches the one defined in the shader.
		if (pRes->type == DESCRIPTOR_TYPE_TEXTURE || pRes->type == DESCRIPTOR_TYPE_RW_TEXTURE)
		{
			pDesc->mDesc.mtlTextureType = pRes->mtlTextureType;
		}

		// If we're binding an argument buffer, we also need to get the type of the resources that this buffer will store.
		if (pRes->mtlArgumentBufferType != DESCRIPTOR_TYPE_UNDEFINED)
		{
			pDesc->mDesc.mtlArgumentBufferType = pRes->mtlArgumentBufferType;
		}
	}

	pRootSignature->mStaticSamplerCount = (uint32_t)staticSamplers.size();
	pRootSignature->ppStaticSamplers = (Sampler**)conf_calloc(staticSamplers.size(), sizeof(Sampler*));
	pRootSignature->pStaticSamplerStages = (ShaderStage*)conf_calloc(staticSamplers.size(), sizeof(ShaderStage));
	pRootSignature->pStaticSamplerSlots = (uint32_t*)conf_calloc(staticSamplers.size(), sizeof(uint32_t));
	for (uint32_t i = 0; i < pRootSignature->mStaticSamplerCount; ++i)
	{
		pRootSignature->ppStaticSamplers[i] = staticSamplers[i].second;
		pRootSignature->pStaticSamplerStages[i] = staticSamplers[i].first->used_stages;
		pRootSignature->pStaticSamplerSlots[i] = staticSamplers[i].first->reg;
	}

	// Create descriptor manager for this thread.
	DescriptorManager* pManager = NULL;
	add_descriptor_manager(pRenderer, pRootSignature, &pManager);
	pRootSignature->pDescriptorManagerMap.insert({ Thread::GetCurrentThreadID(), pManager });

	*ppRootSignature = pRootSignature;
}

void removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
	for (tinystl::unordered_hash_node<ThreadID, DescriptorManager*>& it : pRootSignature->pDescriptorManagerMap)
	{
		remove_descriptor_manager(pRenderer, pRootSignature, it.second);
	}

	pRootSignature->pDescriptorManagerMap.~unordered_map();

	pRootSignature->pDescriptorNameToIndexMap.~unordered_map();

	SAFE_FREE(pRootSignature->ppStaticSamplers);
	SAFE_FREE(pRootSignature->pStaticSamplerStages);
	SAFE_FREE(pRootSignature->pStaticSamplerSlots);
	SAFE_FREE(pRootSignature);
}

uint32_t util_calculate_vertex_layout_stride(const VertexLayout* pVertexLayout)
{
	ASSERT(pVertexLayout);

	uint32_t result = 0;
	for (uint32_t i = 0; i < pVertexLayout->mAttribCount; ++i)
	{
		result += calculateImageFormatStride(pVertexLayout->mAttribs[i].mFormat);
	}
	return result;
}

void addPipeline(Renderer* pRenderer, const GraphicsPipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(pDesc);
	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);

	Pipeline* pPipeline = (Pipeline*)conf_calloc(1, sizeof(*pPipeline));
	ASSERT(pPipeline);

	memcpy(&(pPipeline->mGraphics), pDesc, sizeof(*pDesc));
	pPipeline->mType = PIPELINE_TYPE_GRAPHICS;
	pPipeline->pShader = pPipeline->mGraphics.pShaderProgram;

	// create metal pipeline descriptor
	MTLRenderPipelineDescriptor* renderPipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
	renderPipelineDesc.vertexFunction = pDesc->pShaderProgram->mtlVertexShader;
	renderPipelineDesc.fragmentFunction = pDesc->pShaderProgram->mtlFragmentShader;
	renderPipelineDesc.sampleCount = pDesc->mSampleCount;

	uint32_t inputBindingCount = 0;
	// add vertex layout to descriptor
	if (pPipeline->mGraphics.pVertexLayout != nil)
	{
		uint32_t bindingValue = UINT32_MAX;
		// setup vertex descriptors
		for (uint i = 0; i < pPipeline->mGraphics.pVertexLayout->mAttribCount; i++)
		{
			const VertexAttrib* attrib = pPipeline->mGraphics.pVertexLayout->mAttribs + i;

			if (bindingValue != attrib->mBinding)
			{
				bindingValue = attrib->mBinding;
				inputBindingCount++;
			}

			renderPipelineDesc.vertexDescriptor.attributes[i].offset = attrib->mOffset;
			renderPipelineDesc.vertexDescriptor.attributes[i].bufferIndex = attrib->mBinding;
			renderPipelineDesc.vertexDescriptor.attributes[i].format = util_to_mtl_vertex_format(attrib->mFormat);

			//setup layout for all bindings instead of just 0.
			renderPipelineDesc.vertexDescriptor.layouts[inputBindingCount - 1].stride += calculateImageFormatStride(attrib->mFormat);
			renderPipelineDesc.vertexDescriptor.layouts[inputBindingCount - 1].stepRate = 1;
			renderPipelineDesc.vertexDescriptor.layouts[inputBindingCount - 1].stepFunction =
				pPipeline->pShader->mtlVertexShader.patchType != MTLPatchTypeNone ? MTLVertexStepFunctionPerPatchControlPoint
																				  : MTLVertexStepFunctionPerVertex;
		}
	}

#if !defined(TARGET_IOS)
	// add pipeline settings to descriptor
	switch (pDesc->mPrimitiveTopo)
	{
		case PRIMITIVE_TOPO_POINT_LIST: renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassPoint; break;
		case PRIMITIVE_TOPO_LINE_LIST: renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassLine; break;
		case PRIMITIVE_TOPO_LINE_STRIP: renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassLine; break;
		case PRIMITIVE_TOPO_TRI_LIST: renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle; break;
		case PRIMITIVE_TOPO_TRI_STRIP: renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle; break;
		default: renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle; break;
	}
#endif

	// assign render target pixel format for all attachments
	const BlendState* blendState = pDesc->pBlendState ? pDesc->pBlendState : pDefaultBlendState;
	for (uint32_t i = 0; i < pDesc->mRenderTargetCount; i++)
	{
		renderPipelineDesc.colorAttachments[i].pixelFormat = util_to_mtl_pixel_format(pDesc->pColorFormats[i], pDesc->pSrgbValues[i]);

		// set blend state
		bool hasBlendState = (blendState != nil);
		renderPipelineDesc.colorAttachments[i].blendingEnabled = hasBlendState;
		if (hasBlendState)
		{
			renderPipelineDesc.colorAttachments[i].rgbBlendOperation = blendState->blendStatePerRenderTarget[i].blendMode;
			renderPipelineDesc.colorAttachments[i].alphaBlendOperation = blendState->blendStatePerRenderTarget[i].blendAlphaMode;
			renderPipelineDesc.colorAttachments[i].sourceRGBBlendFactor = blendState->blendStatePerRenderTarget[i].srcFactor;
			renderPipelineDesc.colorAttachments[i].destinationRGBBlendFactor = blendState->blendStatePerRenderTarget[i].destFactor;
			renderPipelineDesc.colorAttachments[i].sourceAlphaBlendFactor = blendState->blendStatePerRenderTarget[i].srcAlphaFactor;
			renderPipelineDesc.colorAttachments[i].destinationAlphaBlendFactor = blendState->blendStatePerRenderTarget[i].destAlphaFactor;
		}
	}

	// assign pixel format form depth attachment
	if (pDesc->mDepthStencilFormat != ImageFormat::NONE)
	{
		renderPipelineDesc.depthAttachmentPixelFormat = util_to_mtl_pixel_format(pDesc->mDepthStencilFormat, false);
#ifndef TARGET_IOS
		if (renderPipelineDesc.depthAttachmentPixelFormat == MTLPixelFormatDepth24Unorm_Stencil8)
			renderPipelineDesc.stencilAttachmentPixelFormat = renderPipelineDesc.depthAttachmentPixelFormat;
#else
		if (pDesc->mDepthStencilFormat == ImageFormat::D24S8)
			renderPipelineDesc.stencilAttachmentPixelFormat = MTLPixelFormatStencil8;
#endif
	}

	// assign common tesselation configuration if needed.
	if (pPipeline->pShader->mtlVertexShader.patchType != MTLPatchTypeNone)
	{
		renderPipelineDesc.tessellationFactorScaleEnabled = NO;
		renderPipelineDesc.tessellationFactorFormat = MTLTessellationFactorFormatHalf;
		renderPipelineDesc.tessellationControlPointIndexType = MTLTessellationControlPointIndexTypeNone;
		renderPipelineDesc.tessellationFactorStepFunction = MTLTessellationFactorStepFunctionConstant;
		renderPipelineDesc.tessellationOutputWindingOrder = MTLWindingClockwise;
		renderPipelineDesc.tessellationPartitionMode = MTLTessellationPartitionModeFractionalEven;
#if TARGET_OS_IOS
		// In iOS, the maximum tessellation factor is 16
		renderPipelineDesc.maxTessellationFactor = 16;
#elif TARGET_OS_OSX
		// In OS X, the maximum tessellation factor is 64
		renderPipelineDesc.maxTessellationFactor = 64;
#endif
	}

	// create pipeline from descriptor
	NSError* error = nil;
	pPipeline->mtlRenderPipelineState = [pRenderer->pDevice newRenderPipelineStateWithDescriptor:renderPipelineDesc
																						 options:MTLPipelineOptionNone
																					  reflection:nil
																						   error:&error];
	if (!pPipeline->mtlRenderPipelineState)
	{
		LOGERRORF("Failed to create render pipeline state, error:\n%s", [[error localizedDescription] UTF8String]);
		return;
	}

	*ppPipeline = pPipeline;
}

void addComputePipeline(Renderer* pRenderer, const ComputePipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(pDesc);
	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);

	Pipeline* pPipeline = (Pipeline*)conf_calloc(1, sizeof(*pPipeline));
	ASSERT(pPipeline);

	memcpy(&(pPipeline->mCompute), pDesc, sizeof(*pDesc));
	pPipeline->mType = PIPELINE_TYPE_COMPUTE;
	pPipeline->pShader = pPipeline->mCompute.pShaderProgram;

	NSError* error = nil;
	pPipeline->mtlComputePipelineState = [pRenderer->pDevice newComputePipelineStateWithFunction:pDesc->pShaderProgram->mtlComputeShader
																						   error:nil];
	if (!pPipeline->mtlComputePipelineState)
	{
		LOGERRORF("Failed to create compute pipeline state, error:\n%s", [[error localizedDescription] UTF8String]);
		SAFE_FREE(pPipeline);
		return;
	}

	*ppPipeline = pPipeline;
}

void removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
	ASSERT(pPipeline);
	pPipeline->mtlRenderPipelineState = nil;
	pPipeline->mtlComputePipelineState = nil;
	SAFE_FREE(pPipeline);
}

void addBlendState(Renderer* pRenderer, const BlendStateDesc* pDesc, BlendState** ppBlendState)
{
	int blendDescIndex = 0;
#ifdef _DEBUG

	for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
	{
		if (pDesc->mRenderTargetMask & (1 << i))
		{
			ASSERT(pDesc->mSrcFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mDstFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mSrcAlphaFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mDstAlphaFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mBlendModes[blendDescIndex] < BlendMode::MAX_BLEND_MODES);
			ASSERT(pDesc->mBlendAlphaModes[blendDescIndex] < BlendMode::MAX_BLEND_MODES);
		}

		if (pDesc->mIndependentBlend)
			++blendDescIndex;
	}

	blendDescIndex = 0;
#endif

	BlendState blendState = {};

	// Go over each RT blend state.
	for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
	{
		if (pDesc->mRenderTargetMask & (1 << i))
		{
			blendState.blendStatePerRenderTarget[i].srcFactor = gMtlBlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]];
			blendState.blendStatePerRenderTarget[i].destFactor = gMtlBlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]];
			blendState.blendStatePerRenderTarget[i].srcAlphaFactor = gMtlBlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]];
			blendState.blendStatePerRenderTarget[i].destAlphaFactor = gMtlBlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]];
			blendState.blendStatePerRenderTarget[i].blendMode = gMtlBlendOpTranslator[pDesc->mBlendModes[blendDescIndex]];
			blendState.blendStatePerRenderTarget[i].blendAlphaMode = gMtlBlendOpTranslator[pDesc->mBlendAlphaModes[blendDescIndex]];
		}

		if (pDesc->mIndependentBlend)
			++blendDescIndex;
	}
	blendState.alphaToCoverage = pDesc->mAlphaToCoverage;

	*ppBlendState = (BlendState*)conf_malloc(sizeof(blendState));
	memcpy(*ppBlendState, &blendState, sizeof(blendState));
}

void removeBlendState(BlendState* pBlendState)
{
	ASSERT(pBlendState);
	SAFE_FREE(pBlendState);
}

void addDepthState(Renderer* pRenderer, const DepthStateDesc* pDesc, DepthState** ppDepthState)
{
	ASSERT(pDesc->mDepthFunc < CompareMode::MAX_COMPARE_MODES);
	ASSERT(pDesc->mStencilFrontFunc < CompareMode::MAX_COMPARE_MODES);
	ASSERT(pDesc->mStencilFrontFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mDepthFrontFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mStencilFrontPass < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mStencilBackFunc < CompareMode::MAX_COMPARE_MODES);
	ASSERT(pDesc->mStencilBackFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mDepthBackFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mStencilBackPass < StencilOp::MAX_STENCIL_OPS);

	MTLDepthStencilDescriptor* descriptor = [[MTLDepthStencilDescriptor alloc] init];
	descriptor.depthCompareFunction = gMtlComparisonFunctionTranslator[pDesc->mDepthFunc];
	descriptor.depthWriteEnabled = pDesc->mDepthWrite;
	descriptor.backFaceStencil.stencilCompareFunction = gMtlComparisonFunctionTranslator[pDesc->mStencilBackFunc];
	descriptor.backFaceStencil.depthFailureOperation = gMtlStencilOpTranslator[pDesc->mDepthBackFail];
	descriptor.backFaceStencil.stencilFailureOperation = gMtlStencilOpTranslator[pDesc->mStencilBackFail];
	descriptor.backFaceStencil.depthStencilPassOperation = gMtlStencilOpTranslator[pDesc->mStencilBackPass];
	descriptor.backFaceStencil.readMask = pDesc->mStencilReadMask;
	descriptor.backFaceStencil.writeMask = pDesc->mStencilWriteMask;
	descriptor.frontFaceStencil.stencilCompareFunction = gMtlComparisonFunctionTranslator[pDesc->mStencilFrontFunc];
	descriptor.frontFaceStencil.depthFailureOperation = gMtlStencilOpTranslator[pDesc->mDepthFrontFail];
	descriptor.frontFaceStencil.stencilFailureOperation = gMtlStencilOpTranslator[pDesc->mStencilFrontFail];
	descriptor.frontFaceStencil.depthStencilPassOperation = gMtlStencilOpTranslator[pDesc->mStencilFrontPass];
	descriptor.frontFaceStencil.readMask = pDesc->mStencilReadMask;
	descriptor.frontFaceStencil.writeMask = pDesc->mStencilWriteMask;

	DepthState* pDepthState = (DepthState*)conf_calloc(1, sizeof(*pDepthState));
	pDepthState->mtlDepthState = [pRenderer->pDevice newDepthStencilStateWithDescriptor:descriptor];

	*ppDepthState = pDepthState;
}

void removeDepthState(DepthState* pDepthState)
{
	ASSERT(pDepthState);
	pDepthState->mtlDepthState = nil;
	SAFE_FREE(pDepthState);
}

void addRasterizerState(Renderer* pRenderer, const RasterizerStateDesc* pDesc, RasterizerState** ppRasterizerState)
{
	ASSERT(pDesc->mFillMode < FillMode::MAX_FILL_MODES);
	ASSERT(pDesc->mCullMode < CullMode::MAX_CULL_MODES);
	ASSERT(pDesc->mFrontFace == FRONT_FACE_CCW || pDesc->mFrontFace == FRONT_FACE_CW);

	RasterizerState rasterizerState = {};

	rasterizerState.cullMode = MTLCullModeNone;
	if (pDesc->mCullMode == CULL_MODE_BACK)
		rasterizerState.cullMode = MTLCullModeBack;
	else if (pDesc->mCullMode == CULL_MODE_FRONT)
		rasterizerState.cullMode = MTLCullModeFront;

	rasterizerState.fillMode = (pDesc->mFillMode == FILL_MODE_SOLID ? MTLTriangleFillModeFill : MTLTriangleFillModeLines);
	rasterizerState.depthBias = pDesc->mDepthBias;
	rasterizerState.depthBiasSlopeFactor = pDesc->mSlopeScaledDepthBias;
	rasterizerState.scissorEnable = pDesc->mScissor;
	rasterizerState.multisampleEnable = pDesc->mMultiSample;
	rasterizerState.frontFace = (pDesc->mFrontFace == FRONT_FACE_CCW ? MTLWindingCounterClockwise : MTLWindingClockwise);

	*ppRasterizerState = (RasterizerState*)conf_malloc(sizeof(rasterizerState));
	memcpy(*ppRasterizerState, &rasterizerState, sizeof(rasterizerState));
}

void removeRasterizerState(RasterizerState* pRasterizerState)
{
	ASSERT(pRasterizerState);
	SAFE_FREE(pRasterizerState);
}

void addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
{
	assert(pRenderer != nil);
	assert(pDesc != nil);

	CommandSignature* pCommandSignature = (CommandSignature*)conf_calloc(1, sizeof(CommandSignature));

	for (uint32_t i = 0; i < pDesc->mIndirectArgCount; i++)
	{
		const IndirectArgumentDescriptor* argDesc = pDesc->pArgDescs + i;
		if (argDesc->mType != INDIRECT_DRAW && argDesc->mType != INDIRECT_DISPATCH && argDesc->mType != INDIRECT_DRAW_INDEX)
		{
			assert(!"Unsupported indirect argument type.");
			SAFE_FREE(pCommandSignature);
			return;
		}

		if (i == 0)
		{
			pCommandSignature->mDrawType = argDesc->mType;
		}
		else if (pCommandSignature->mDrawType != argDesc->mType)
		{
			assert(!"All elements in the root signature must be of the same type.");
			SAFE_FREE(pCommandSignature);
			return;
		}
	}
	pCommandSignature->mIndirectArgDescCounts = pDesc->mIndirectArgCount;

	*ppCommandSignature = pCommandSignature;
}

void removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
{
	ASSERT(pCommandSignature);
	SAFE_FREE(pCommandSignature);
}

// -------------------------------------------------------------------------------------------------
// Buffer functions
// -------------------------------------------------------------------------------------------------
void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
	ASSERT(pBuffer->mDesc.mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to map non-cpu accessible resource");
	pBuffer->pCpuMappedAddress = pBuffer->mtlBuffer.contents;
}
void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	ASSERT(pBuffer->mDesc.mMemoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to unmap non-cpu accessible resource");
	pBuffer->pCpuMappedAddress = nil;
}

// -------------------------------------------------------------------------------------------------
// Command buffer functions
// -------------------------------------------------------------------------------------------------

void beginCmd(Cmd* pCmd)
{
	@autoreleasepool
	{
		ASSERT(pCmd);
		pCmd->mtlRenderEncoder = nil;
		pCmd->mtlComputeEncoder = nil;
		pCmd->mtlBlitEncoder = nil;
		pCmd->pShader = nil;
		pCmd->pRenderPassDesc = nil;
		pCmd->selectedIndexBuffer = nil;
		pCmd->pBoundRootSignature = nil;
		pCmd->mtlCommandBuffer = [pCmd->pCmdPool->pQueue->mtlCommandQueue commandBuffer];
	}
}

void endCmd(Cmd* pCmd)
{
	if (pCmd->mRenderPassActive)
	{
		// Reset the bound resources flags for the current root signature's descriptor manager.
		const tinystl::unordered_hash_node<ThreadID, DescriptorManager*>* pNode =
			pCmd->pBoundRootSignature->pDescriptorManagerMap.find(Thread::GetCurrentThreadID()).node;
		if (pNode)
			reset_bound_resources(pNode->second);

		@autoreleasepool
		{
			util_end_current_encoders(pCmd);
		}
	}

	pCmd->mRenderPassActive = false;
	pCmd->mBoundRenderTargetCount = 0;
	pCmd->mBoundDepthStencilFormat = ImageFormat::NONE;

	// Reset the bound resources flags for the current root signature's descriptor manager.
	if (pCmd->pBoundRootSignature)
	{
		const tinystl::unordered_hash_node<ThreadID, DescriptorManager*>* pNode =
			pCmd->pBoundRootSignature->pDescriptorManagerMap.find(Thread::GetCurrentThreadID()).node;
		if (pNode)
			reset_bound_resources(pNode->second);
	}
}

void cmdBindRenderTargets(
	Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil, const LoadActionsDesc* pLoadActions,
	uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice, uint32_t depthMipSlice)
{
	ASSERT(pCmd);

	if (pCmd->mRenderPassActive)
	{
		if (pCmd->pBoundRootSignature)
		{
			// Reset the bound resources flags for the current root signature's descriptor manager.
			const tinystl::unordered_hash_node<ThreadID, DescriptorManager*>* pNode =
				pCmd->pBoundRootSignature->pDescriptorManagerMap.find(Thread::GetCurrentThreadID()).node;
			if (pNode)
				reset_bound_resources(pNode->second);
		}
		else
		{
			LOGWARNINGF("Render pass is active but no root signature is bound!");
		}

		@autoreleasepool
		{
			util_end_current_encoders(pCmd);
		}

		pCmd->mRenderPassActive = false;
		pCmd->mBoundRenderTargetCount = 0;
		pCmd->mBoundDepthStencilFormat = ImageFormat::NONE;
	}

	if (!renderTargetCount && !pDepthStencil)
		return;

	uint64_t renderPassHash = 0;

	@autoreleasepool
	{
		pCmd->pRenderPassDesc = [MTLRenderPassDescriptor renderPassDescriptor];

		// Flush color attachments
		for (uint32_t i = 0; i < renderTargetCount; i++)
		{
			Texture* colorAttachment = ppRenderTargets[i]->pTexture;

			pCmd->pRenderPassDesc.colorAttachments[i].texture = colorAttachment->mtlTexture;
			pCmd->pRenderPassDesc.colorAttachments[i].level = pColorMipSlices ? pColorMipSlices[i] : 0;
			if (colorAttachment->mDesc.mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES)
				pCmd->pRenderPassDesc.colorAttachments[i].slice = pColorArraySlices ? pColorArraySlices[i] : 0;
			else if (colorAttachment->mDesc.mDescriptors & DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES)
				pCmd->pRenderPassDesc.colorAttachments[i].depthPlane = pColorArraySlices ? pColorArraySlices[i] : 0;
#ifndef TARGET_IOS
			pCmd->pRenderPassDesc.colorAttachments[i].loadAction =
				(pLoadActions != NULL ? util_to_mtl_load_action(pLoadActions->mLoadActionsColor[i]) : MTLLoadActionLoad);
			pCmd->pRenderPassDesc.colorAttachments[i].storeAction = MTLStoreActionStore;
#else
			if (colorAttachment->mtlTexture.storageMode == MTLStorageModeMemoryless)
			{
				pCmd->pRenderPassDesc.colorAttachments[i].loadAction = MTLLoadActionDontCare;
				pCmd->pRenderPassDesc.colorAttachments[i].storeAction = MTLStoreActionDontCare;
			}
			else
			{
				pCmd->pRenderPassDesc.colorAttachments[i].loadAction =
					(pLoadActions != NULL ? util_to_mtl_load_action(pLoadActions->mLoadActionsColor[i]) : MTLLoadActionLoad);
				pCmd->pRenderPassDesc.colorAttachments[i].storeAction = MTLStoreActionStore;
			}
#endif
			if (pLoadActions != NULL)
			{
				const ClearValue& clearValue = pLoadActions->mClearColorValues[i];
				pCmd->pRenderPassDesc.colorAttachments[i].clearColor =
					MTLClearColorMake(clearValue.r, clearValue.g, clearValue.b, clearValue.a);
			}

			pCmd->pBoundColorFormats[i] = ppRenderTargets[i]->mDesc.mFormat;
			pCmd->pBoundSrgbValues[i] = ppRenderTargets[i]->mDesc.mSrgb;

			uint32_t hashValues[] = {
				(uint32_t)ppRenderTargets[i]->mDesc.mFormat,
				(uint32_t)ppRenderTargets[i]->mDesc.mSampleCount,
				(uint32_t)ppRenderTargets[i]->mDesc.mSrgb,
			};
			renderPassHash = tinystl::hash_state(hashValues, 3, renderPassHash);
		}

		if (pDepthStencil != nil)
		{
			pCmd->pRenderPassDesc.depthAttachment.texture = pDepthStencil->pTexture->mtlTexture;
			pCmd->pRenderPassDesc.depthAttachment.level = (depthMipSlice != -1 ? depthMipSlice : 0);
			pCmd->pRenderPassDesc.depthAttachment.slice = (depthArraySlice != -1 ? depthArraySlice : 0);
#ifndef TARGET_IOS
			bool isStencilEnabled = pDepthStencil->pTexture->mtlPixelFormat == MTLPixelFormatDepth24Unorm_Stencil8;
			if (isStencilEnabled)
			{
				pCmd->pRenderPassDesc.stencilAttachment.texture = pDepthStencil->pTexture->mtlTexture;
				pCmd->pRenderPassDesc.stencilAttachment.level = (depthMipSlice != -1 ? depthMipSlice : 0);
				pCmd->pRenderPassDesc.stencilAttachment.slice = (depthArraySlice != -1 ? depthArraySlice : 0);
			}

			pCmd->pRenderPassDesc.depthAttachment.loadAction =
				pLoadActions ? util_to_mtl_load_action(pLoadActions->mLoadActionDepth) : MTLLoadActionClear;
			pCmd->pRenderPassDesc.depthAttachment.storeAction = MTLStoreActionStore;
			if (isStencilEnabled)
			{
				pCmd->pRenderPassDesc.stencilAttachment.loadAction =
					pLoadActions ? util_to_mtl_load_action(pLoadActions->mLoadActionStencil) : MTLLoadActionDontCare;
				pCmd->pRenderPassDesc.stencilAttachment.storeAction = MTLStoreActionStore;
			}
			else
			{
				pCmd->pRenderPassDesc.stencilAttachment.loadAction = MTLLoadActionDontCare;
				pCmd->pRenderPassDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
			}
#else
			bool isStencilEnabled = pDepthStencil->pStencil != nil;
			if (isStencilEnabled)
			{
				pCmd->pRenderPassDesc.stencilAttachment.texture = pDepthStencil->pStencil->mtlTexture;
				pCmd->pRenderPassDesc.stencilAttachment.level = (depthMipSlice != -1 ? depthMipSlice : 0);
				pCmd->pRenderPassDesc.stencilAttachment.slice = (depthArraySlice != -1 ? depthArraySlice : 0);
			}

			if (pDepthStencil->pTexture->mtlTexture.storageMode != MTLStorageModeMemoryless)
			{
				pCmd->pRenderPassDesc.depthAttachment.loadAction =
					pLoadActions ? util_to_mtl_load_action(pLoadActions->mLoadActionDepth) : MTLLoadActionDontCare;
				pCmd->pRenderPassDesc.depthAttachment.storeAction = MTLStoreActionStore;
				if (isStencilEnabled)
				{
					pCmd->pRenderPassDesc.stencilAttachment.loadAction =
						pLoadActions ? util_to_mtl_load_action(pLoadActions->mLoadActionStencil) : MTLLoadActionDontCare;
					pCmd->pRenderPassDesc.stencilAttachment.storeAction = MTLStoreActionStore;
				}
				else
				{
					pCmd->pRenderPassDesc.stencilAttachment.loadAction = MTLLoadActionDontCare;
					pCmd->pRenderPassDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
				}
			}
			else
			{
				pCmd->pRenderPassDesc.depthAttachment.loadAction = MTLLoadActionDontCare;
				pCmd->pRenderPassDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
				pCmd->pRenderPassDesc.stencilAttachment.loadAction = MTLLoadActionDontCare;
				pCmd->pRenderPassDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
			}
#endif
			if (pLoadActions)
			{
				pCmd->pRenderPassDesc.depthAttachment.clearDepth = pLoadActions->mClearDepth.depth;
				if (isStencilEnabled)
					pCmd->pRenderPassDesc.stencilAttachment.clearStencil = 0;
			}

			pCmd->mBoundDepthStencilFormat = pDepthStencil->mDesc.mFormat;

			uint32_t hashValues[] = {
				(uint32_t)pDepthStencil->mDesc.mFormat,
				(uint32_t)pDepthStencil->mDesc.mSampleCount,
				(uint32_t)pDepthStencil->mDesc.mSrgb,
			};
			renderPassHash = tinystl::hash_state(hashValues, 3, renderPassHash);
		}
		else
		{
			pCmd->pRenderPassDesc.depthAttachment.loadAction = MTLLoadActionDontCare;
			pCmd->pRenderPassDesc.stencilAttachment.loadAction = MTLLoadActionDontCare;
			pCmd->pRenderPassDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
			pCmd->pRenderPassDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
			pCmd->mBoundDepthStencilFormat = ImageFormat::NONE;
		}

		SampleCount sampleCount = renderTargetCount ? ppRenderTargets[0]->mDesc.mSampleCount : pDepthStencil->mDesc.mSampleCount;
		pCmd->mBoundWidth = renderTargetCount ? ppRenderTargets[0]->mDesc.mWidth : pDepthStencil->mDesc.mWidth;
		pCmd->mBoundHeight = renderTargetCount ? ppRenderTargets[0]->mDesc.mHeight : pDepthStencil->mDesc.mHeight;
		pCmd->mBoundSampleCount = sampleCount;
		pCmd->mBoundRenderTargetCount = renderTargetCount;

		bool switchedEncoders =
			util_sync_encoders(pCmd, CMD_POOL_DIRECT);    // Check if we need to sync different types of encoders (only on direct cmds).
		util_end_current_encoders(pCmd);
		pCmd->mtlRenderEncoder = [pCmd->mtlCommandBuffer renderCommandEncoderWithDescriptor:pCmd->pRenderPassDesc];
		if (switchedEncoders)
			[pCmd->mtlRenderEncoder waitForFence:pCmd->mtlEncoderFence beforeStages:MTLRenderStageVertex];

		pCmd->mRenderPassActive = true;
	}
}

void cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
	ASSERT(pCmd);
	if (pCmd->mtlRenderEncoder == nil)
	{
		internal_log(LOG_TYPE_ERROR, "Using cmdSetViewport out of a cmdBeginRender / cmdEndRender block is not allowed", "cmdSetViewport");
		return;
	}

	MTLViewport viewport;
	viewport.originX = x;
	viewport.originY = y;
	viewport.width = width;
	viewport.height = height;
	viewport.znear = minDepth;
	viewport.zfar = maxDepth;

	[pCmd->mtlRenderEncoder setViewport:viewport];
}

void cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	ASSERT(pCmd);
	if (pCmd->mtlRenderEncoder == nil)
	{
		internal_log(LOG_TYPE_ERROR, "Using cmdSetScissor out of a cmdBeginRender / cmdEndRender block is not allowed", "cmdSetScissor");
		return;
	}

	// Get the maximum safe scissor values for the current render pass.
	uint32_t maxScissorX = pCmd->pRenderPassDesc.colorAttachments[0].texture.width > 0
							   ? (uint32_t)pCmd->pRenderPassDesc.colorAttachments[0].texture.width
							   : (uint32_t)pCmd->pRenderPassDesc.depthAttachment.texture.width;
	uint32_t maxScissorY = pCmd->pRenderPassDesc.colorAttachments[0].texture.height > 0
							   ? (uint32_t)pCmd->pRenderPassDesc.colorAttachments[0].texture.height
							   : (uint32_t)pCmd->pRenderPassDesc.depthAttachment.texture.height;
	uint32_t maxScissorW = maxScissorX - int32_t(max(x, 0));
	uint32_t maxScissorH = maxScissorY - int32_t(max(y, 0));

	// Make sure neither width or height are 0 (unsupported by Metal).
	if (width == 0u)
		width = 1u;
	if (height == 0u)
		height = 1u;

	MTLScissorRect scissor;
	scissor.x = min(x, maxScissorX);
	scissor.y = min(y, maxScissorY);
	scissor.width = min(width, maxScissorW);
	scissor.height = min(height, maxScissorH);

	[pCmd->mtlRenderEncoder setScissorRect:scissor];
}

void cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
	ASSERT(pCmd);
	ASSERT(pPipeline);

	pCmd->pShader = pPipeline->pShader;

	@autoreleasepool
	{
		if (pPipeline->mType == PIPELINE_TYPE_GRAPHICS)
		{
			[pCmd->mtlRenderEncoder setRenderPipelineState:pPipeline->mtlRenderPipelineState];

			RasterizerState* rasterizerState =
				pPipeline->mGraphics.pRasterizerState ? pPipeline->mGraphics.pRasterizerState : pDefaultRasterizerState;
			[pCmd->mtlRenderEncoder setCullMode:rasterizerState->cullMode];
			[pCmd->mtlRenderEncoder setTriangleFillMode:rasterizerState->fillMode];
			[pCmd->mtlRenderEncoder setFrontFacingWinding:rasterizerState->frontFace];

			if (pCmd->pRenderPassDesc.depthAttachment.texture != nil)
			{
				DepthState* depthState = pPipeline->mGraphics.pDepthState ? pPipeline->mGraphics.pDepthState : pDefaultDepthState;
				[pCmd->mtlRenderEncoder setDepthStencilState:depthState->mtlDepthState];
			}

			switch (pPipeline->mGraphics.mPrimitiveTopo)
			{
				case PRIMITIVE_TOPO_POINT_LIST: pCmd->selectedPrimitiveType = MTLPrimitiveTypePoint; break;
				case PRIMITIVE_TOPO_LINE_LIST: pCmd->selectedPrimitiveType = MTLPrimitiveTypeLine; break;
				case PRIMITIVE_TOPO_LINE_STRIP: pCmd->selectedPrimitiveType = MTLPrimitiveTypeLineStrip; break;
				case PRIMITIVE_TOPO_TRI_LIST: pCmd->selectedPrimitiveType = MTLPrimitiveTypeTriangle; break;
				case PRIMITIVE_TOPO_TRI_STRIP: pCmd->selectedPrimitiveType = MTLPrimitiveTypeTriangleStrip; break;
				default: pCmd->selectedPrimitiveType = MTLPrimitiveTypeTriangle; break;
			}
		}
		else
		{
			if (!pCmd->mtlComputeEncoder)
			{
				util_end_current_encoders(pCmd);
				pCmd->mtlComputeEncoder = [pCmd->mtlCommandBuffer computeCommandEncoder];
			}
			[pCmd->mtlComputeEncoder setComputePipelineState:pPipeline->mtlComputePipelineState];
		}
	}
}

void cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t offset)
{
	ASSERT(pCmd);
	ASSERT(pBuffer);

	pCmd->selectedIndexBuffer = pBuffer;
	pCmd->mSelectedIndexBufferOffset = offset;
}

void cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, uint64_t* pOffsets)
{
	ASSERT(pCmd);
	ASSERT(0 != bufferCount);
	ASSERT(ppBuffers);

	// When using a poss-tessellation vertex shader, the first vertex buffer bound is used as the tessellation factors buffer.
	uint startIdx = 0;
	if (pCmd->pShader->mtlVertexShader.patchType != MTLPatchTypeNone)
	{
		startIdx = 1;
		[pCmd->mtlRenderEncoder setTessellationFactorBuffer:ppBuffers[0]->mtlBuffer offset:0 instanceStride:0];
	}

	for (uint32_t i = startIdx; i < bufferCount; i++)
	{
		[pCmd->mtlRenderEncoder setVertexBuffer:ppBuffers[i]->mtlBuffer
										 offset:(ppBuffers[i]->mPositionInHeap + (pOffsets ? pOffsets[i] : 0))
										atIndex:(i - startIdx)];
	}
}

void cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
	ASSERT(pCmd);
	if (pCmd->pShader->mtlVertexShader.patchType == MTLPatchTypeNone)
	{
		[pCmd->mtlRenderEncoder drawPrimitives:pCmd->selectedPrimitiveType vertexStart:firstVertex vertexCount:vertexCount];
	}
	else    // Tessellated draw version.
	{
		[pCmd->mtlRenderEncoder drawPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
								 patchStart:firstVertex
								 patchCount:vertexCount
						   patchIndexBuffer:nil
					 patchIndexBufferOffset:0
							  instanceCount:1
							   baseInstance:0];
	}
}

void cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
	ASSERT(pCmd);
	if (pCmd->pShader->mtlVertexShader.patchType == MTLPatchTypeNone)
	{
		if (firstInstance == 0)
		{
			[pCmd->mtlRenderEncoder drawPrimitives:pCmd->selectedPrimitiveType
									   vertexStart:firstVertex
									   vertexCount:vertexCount
									 instanceCount:instanceCount];
		}
		else
		{
			[pCmd->mtlRenderEncoder drawPrimitives:pCmd->selectedPrimitiveType
									   vertexStart:firstVertex
									   vertexCount:vertexCount
									 instanceCount:instanceCount
									  baseInstance:firstInstance];
		}
	}
	else    // Tessellated draw version.
	{
		[pCmd->mtlRenderEncoder drawPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
								 patchStart:firstVertex
								 patchCount:vertexCount
						   patchIndexBuffer:nil
					 patchIndexBufferOffset:0
							  instanceCount:instanceCount
							   baseInstance:firstInstance];
	}
}

void cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
	ASSERT(pCmd);
	Buffer*      indexBuffer = pCmd->selectedIndexBuffer;
	MTLIndexType indexType = (indexBuffer->mDesc.mIndexType == INDEX_TYPE_UINT16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32);
	uint64_t     offset = pCmd->mSelectedIndexBufferOffset + (firstIndex * (indexBuffer->mDesc.mIndexType == INDEX_TYPE_UINT16 ? 2 : 4));

	if (pCmd->pShader->mtlVertexShader.patchType == MTLPatchTypeNone)
	{
		//only ios devices supporting gpu family 3_v1 and above can use baseVertex and baseInstance
		//if lower than 3_v1 render without base info but artifacts will occur if used.
#ifdef TARGET_IOS
		if ([pCmd->pRenderer->pDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1])
#endif
		{
			[pCmd->mtlRenderEncoder drawIndexedPrimitives:pCmd->selectedPrimitiveType
											   indexCount:indexCount
												indexType:indexType
											  indexBuffer:indexBuffer->mtlBuffer
										indexBufferOffset:offset
											instanceCount:1
											   baseVertex:firstVertex
											 baseInstance:0];
		}
#ifdef TARGET_IOS
		else
		{
			LOGERRORF("Current device does not support ios gpuFamily 3_v1 feature set.");
			return;
		}
#endif
	}
	else    // Tessellated draw version.
	{
		//to supress warning passing nil to controlPointIndexBuffer
		//todo: Add control point index buffer to be passed when necessary
		id<MTLBuffer> _Nullable indexBuf = nil;
		[pCmd->mtlRenderEncoder drawIndexedPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
										patchStart:firstIndex
										patchCount:indexCount
								  patchIndexBuffer:indexBuffer->mtlBuffer
							patchIndexBufferOffset:0
						   controlPointIndexBuffer:indexBuf
					 controlPointIndexBufferOffset:0
									 instanceCount:1
									  baseInstance:0];
	}
}

void cmdDrawIndexedInstanced(
	Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstInstance, uint32_t firstVertex)
{
	ASSERT(pCmd);

	Buffer*      indexBuffer = pCmd->selectedIndexBuffer;
	MTLIndexType indexType = (indexBuffer->mDesc.mIndexType == INDEX_TYPE_UINT16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32);
	uint64_t     offset = pCmd->mSelectedIndexBufferOffset + (firstIndex * (indexBuffer->mDesc.mIndexType == INDEX_TYPE_UINT16 ? 2 : 4));

	if (pCmd->pShader->mtlVertexShader.patchType == MTLPatchTypeNone)
	{
		[pCmd->mtlRenderEncoder drawIndexedPrimitives:pCmd->selectedPrimitiveType
										   indexCount:indexCount
											indexType:indexType
										  indexBuffer:indexBuffer->mtlBuffer
									indexBufferOffset:offset
										instanceCount:instanceCount
										   baseVertex:firstVertex
										 baseInstance:firstInstance];
	}
	else    // Tessellated draw version.
	{
		//to supress warning passing nil to controlPointIndexBuffer
		//todo: Add control point index buffer to be passed when necessary
		id<MTLBuffer> _Nullable indexBuf = nil;
		[pCmd->mtlRenderEncoder drawIndexedPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
										patchStart:firstIndex
										patchCount:indexCount
								  patchIndexBuffer:indexBuffer->mtlBuffer
							patchIndexBufferOffset:0
						   controlPointIndexBuffer:indexBuf
					 controlPointIndexBufferOffset:0
									 instanceCount:instanceCount
									  baseInstance:firstInstance];
	}
}

void cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	ASSERT(pCmd);
	ASSERT(pCmd->mtlComputeEncoder != nil);

	Shader* shader = pCmd->pShader;

	MTLSize threadsPerThreadgroup =
		MTLSizeMake(shader->mNumThreadsPerGroup[0], shader->mNumThreadsPerGroup[1], shader->mNumThreadsPerGroup[2]);
	MTLSize threadgroupCount = MTLSizeMake(groupCountX, groupCountY, groupCountZ);

	[pCmd->mtlComputeEncoder dispatchThreadgroups:threadgroupCount threadsPerThreadgroup:threadsPerThreadgroup];
}

void cmdExecuteIndirect(
	Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
	Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
	for (uint32_t i = 0; i < maxCommandCount; i++)
	{
		if (pCommandSignature->mDrawType == INDIRECT_DRAW)
		{
			uint64_t indirectBufferOffset = bufferOffset + sizeof(IndirectDrawArguments) * i;
			if (pCmd->pShader->mtlVertexShader.patchType == MTLPatchTypeNone)
			{
				[pCmd->mtlRenderEncoder drawPrimitives:pCmd->selectedPrimitiveType
										indirectBuffer:pIndirectBuffer->mtlBuffer
								  indirectBufferOffset:indirectBufferOffset];
			}
			else    // Tessellated draw version.
			{
#ifndef TARGET_IOS
				[pCmd->mtlRenderEncoder drawPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
								   patchIndexBuffer:nil
							 patchIndexBufferOffset:0
									 indirectBuffer:pIndirectBuffer->mtlBuffer
							   indirectBufferOffset:indirectBufferOffset];
#else
				// Tessellated indirect-draw is not supported on iOS.
				// Instead, read regular draw arguments from the indirect draw buffer.
				mapBuffer(pCmd->pRenderer, pIndirectBuffer, NULL);
				IndirectDrawArguments* pDrawArgs = (IndirectDrawArguments*)(pIndirectBuffer->pCpuMappedAddress) + indirectBufferOffset;
				unmapBuffer(pCmd->pRenderer, pIndirectBuffer);

				[pCmd->mtlRenderEncoder drawPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
										 patchStart:pDrawArgs->mStartVertex
										 patchCount:pDrawArgs->mVertexCount
								   patchIndexBuffer:nil
							 patchIndexBufferOffset:0
									  instanceCount:pDrawArgs->mInstanceCount
									   baseInstance:pDrawArgs->mStartInstance];
#endif
			}
		}
		else if (pCommandSignature->mDrawType == INDIRECT_DRAW_INDEX)
		{
			Buffer*      indexBuffer = pCmd->selectedIndexBuffer;
			MTLIndexType indexType = (indexBuffer->mDesc.mIndexType == INDEX_TYPE_UINT16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32);
			uint64_t     indirectBufferOffset = bufferOffset + sizeof(IndirectDrawIndexArguments) * i;

			if (pCmd->pShader->mtlVertexShader.patchType == MTLPatchTypeNone)
			{
				[pCmd->mtlRenderEncoder drawIndexedPrimitives:pCmd->selectedPrimitiveType
													indexType:indexType
												  indexBuffer:indexBuffer->mtlBuffer
											indexBufferOffset:0
											   indirectBuffer:pIndirectBuffer->mtlBuffer
										 indirectBufferOffset:indirectBufferOffset];
			}
			else    // Tessellated draw version.
			{
#ifndef TARGET_IOS
				[pCmd->mtlRenderEncoder drawPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
								   patchIndexBuffer:indexBuffer->mtlBuffer
							 patchIndexBufferOffset:0
									 indirectBuffer:pIndirectBuffer->mtlBuffer
							   indirectBufferOffset:indirectBufferOffset];
#else
				// Tessellated indirect-draw is not supported on iOS.
				// Instead, read regular draw arguments from the indirect draw buffer.
				mapBuffer(pCmd->pRenderer, pIndirectBuffer, NULL);
				IndirectDrawIndexArguments* pDrawArgs =
					(IndirectDrawIndexArguments*)(pIndirectBuffer->pCpuMappedAddress) + indirectBufferOffset;
				unmapBuffer(pCmd->pRenderer, pIndirectBuffer);

				//to supress warning passing nil to controlPointIndexBuffer
				//todo: Add control point index buffer to be passed when necessary
				id<MTLBuffer> _Nullable ctrlPtIndexBuf = nil;
				[pCmd->mtlRenderEncoder drawIndexedPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
												patchStart:pDrawArgs->mStartIndex
												patchCount:pDrawArgs->mIndexCount
										  patchIndexBuffer:indexBuffer->mtlBuffer
									patchIndexBufferOffset:0
								   controlPointIndexBuffer:ctrlPtIndexBuf
							 controlPointIndexBufferOffset:0
											 instanceCount:pDrawArgs->mInstanceCount
											  baseInstance:pDrawArgs->mStartInstance];
#endif
			}
		}
		else if (pCommandSignature->mDrawType == INDIRECT_DISPATCH)
		{
			//TODO: Implement.
			ASSERT(0);
		}
	}
}

void cmdResourceBarrier(
	Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers,
	bool batch)
{
}
void cmdSynchronizeResources(Cmd* pCmd, uint32_t numBuffers, Buffer** ppBuffers, uint32_t numTextures, Texture** ppTextures, bool batch) {}
void cmdFlushBarriers(Cmd* pCmd) {}

void cmdUpdateBuffer(Cmd* pCmd, uint64_t srcOffset, uint64_t dstOffset, uint64_t size, Buffer* pSrcBuffer, Buffer* pBuffer)
{
	ASSERT(pCmd);
	ASSERT(pSrcBuffer);
	ASSERT(pSrcBuffer->mtlBuffer);
	ASSERT(pBuffer);
	ASSERT(pBuffer->mtlBuffer);
	ASSERT(srcOffset + size <= pSrcBuffer->mDesc.mSize);
	ASSERT(dstOffset + size <= pBuffer->mDesc.mSize);

	util_end_current_encoders(pCmd);
	pCmd->mtlBlitEncoder = [pCmd->mtlCommandBuffer blitCommandEncoder];

	[pCmd->mtlBlitEncoder copyFromBuffer:pSrcBuffer->mtlBuffer
							sourceOffset:srcOffset
								toBuffer:pBuffer->mtlBuffer
					   destinationOffset:dstOffset
									size:size];
}

void cmdUpdateSubresources(
	Cmd* pCmd, uint32_t startSubresource, uint32_t numSubresources, SubresourceDataDesc* pSubresources, Buffer* pIntermediate,
	uint64_t intermediateOffset, Texture* pTexture)
{
	util_end_current_encoders(pCmd);
	pCmd->mtlBlitEncoder = [pCmd->mtlCommandBuffer blitCommandEncoder];

	uint nLayers = pTexture->mDesc.mArraySize;
	uint nFaces = 1;
	uint nMips = pTexture->mDesc.mMipLevels;

	uint32_t subresourceOffset = 0;
	for (uint32_t layer = 0; layer < nLayers; ++layer)
	{
		for (uint32_t face = 0; face < nFaces; ++face)
		{
			for (uint32_t mip = 0; mip < nMips; ++mip)
			{
				SubresourceDataDesc* pRes = &pSubresources[(layer * nFaces * nMips) + (face * nMips) + mip];
				uint32_t             mipmapWidth = max(pTexture->mDesc.mWidth >> mip, 1);
				uint32_t             mipmapHeight = max(pTexture->mDesc.mHeight >> mip, 1);

				// Copy the data for this resource to an intermediate buffer.
				memcpy((uint8_t*)pIntermediate->pCpuMappedAddress + intermediateOffset + subresourceOffset, pRes->pData, pRes->mSlicePitch);

				// Copy to the texture's final subresource.
				[pCmd->mtlBlitEncoder copyFromBuffer:pIntermediate->mtlBuffer
										sourceOffset:intermediateOffset + subresourceOffset
								   sourceBytesPerRow:pRes->mRowPitch
								 sourceBytesPerImage:pRes->mSlicePitch
										  sourceSize:MTLSizeMake(mipmapWidth, mipmapHeight, 1)
										   toTexture:pTexture->mtlTexture
									destinationSlice:layer * nFaces + face
									destinationLevel:mip
								   destinationOrigin:MTLOriginMake(0, 0, 0)];

				// Increase the subresource offset.
				subresourceOffset += pRes->mSlicePitch;
			}
		}
	}
}

void acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(pSwapChain);
	ASSERT(pSignalSemaphore || pFence);

	CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pMTKView.layer;

	if (pSwapChain->mMTKDrawable == nil)
		pSwapChain->mMTKDrawable = [layer nextDrawable];

	// Look for the render target containing this texture.
	// If not found: assign it to an empty slot
	for (uint32_t i = 0; i < pSwapChain->mDesc.mImageCount; i++)
	{
		RenderTarget* renderTarget = pSwapChain->ppSwapchainRenderTargets[i];
		if (renderTarget->pTexture->mtlTexture == pSwapChain->mMTKDrawable.texture)
		{
			*pImageIndex = i;
			return;
		}
	}

	// Not found: assign the texture to an empty slot
	for (uint32_t i = 0; i < pSwapChain->mDesc.mImageCount; i++)
	{
		RenderTarget* renderTarget = pSwapChain->ppSwapchainRenderTargets[i];
		if (renderTarget->pTexture->mtlTexture == nil)
		{
			renderTarget->pTexture->mtlTexture = pSwapChain->mMTKDrawable.texture;

			// Update the swapchain RT size according to the new drawable's size.
			renderTarget->pTexture->mDesc.mWidth = (uint32_t)pSwapChain->mMTKDrawable.texture.width;
			renderTarget->pTexture->mDesc.mHeight = (uint32_t)pSwapChain->mMTKDrawable.texture.height;
			pSwapChain->ppSwapchainRenderTargets[i]->mDesc.mWidth = renderTarget->pTexture->mDesc.mWidth;
			pSwapChain->ppSwapchainRenderTargets[i]->mDesc.mHeight = renderTarget->pTexture->mDesc.mHeight;

			*pImageIndex = i;
			return;
		}
	}

	// The swapchain textures have changed internally:
	// Invalidate the texures and re-acquire the render targets
	for (uint32_t i = 0; i < pSwapChain->mDesc.mImageCount; i++)
	{
		pSwapChain->ppSwapchainRenderTargets[i]->pTexture->mtlTexture = nil;
	}
	acquireNextImage(pRenderer, pSwapChain, pSignalSemaphore, pFence, pImageIndex);
}

void queueSubmit(
	Queue* pQueue, uint32_t cmdCount, Cmd** ppCmds, Fence* pFence, uint32_t waitSemaphoreCount, Semaphore** ppWaitSemaphores,
	uint32_t signalSemaphoreCount, Semaphore** ppSignalSemaphores)
{
	ASSERT(pQueue);
	ASSERT(cmdCount > 0);
	ASSERT(ppCmds);
	if (waitSemaphoreCount > 0)
	{
		ASSERT(ppWaitSemaphores);
	}
	if (signalSemaphoreCount > 0)
	{
		ASSERT(ppSignalSemaphores);
	}

	// set the queue built-in semaphore to signal when all command lists finished their execution
	__block uint32_t commandsFinished = 0;
	__weak dispatch_semaphore_t blockSemaphore = pQueue->pMtlSemaphore;
	__weak dispatch_semaphore_t completedFence = nil;
	if (pFence)
	{
		completedFence = pFence->pMtlSemaphore;
		pFence->mSubmitted = true;
	}
	for (uint32_t i = 0; i < cmdCount; i++)
	{
		[ppCmds[i]->mtlCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
			commandsFinished++;
			if (commandsFinished == cmdCount)
			{
				dispatch_semaphore_signal(blockSemaphore);
				if (completedFence)
					dispatch_semaphore_signal(completedFence);
			}
		}];
	}

	// commit the command lists
	for (uint32_t i = 0; i < cmdCount; i++)
	{
		// register the following semaphores for signaling after the work has been done
		for (uint32_t j = 0; j < signalSemaphoreCount; j++)
		{
			__weak dispatch_semaphore_t blockSemaphore = ppSignalSemaphores[j]->pMtlSemaphore;
			[ppCmds[i]->mtlCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) { dispatch_semaphore_signal(blockSemaphore); }];
		}

		// Commit any uncommited encoder. This is necessary before committing the command buffer
		util_end_current_encoders(ppCmds[i]);
		[ppCmds[i]->mtlCommandBuffer commit];
	}
}

void queuePresent(
	Queue* pQueue, SwapChain* pSwapChain, uint32_t swapChainImageIndex, uint32_t waitSemaphoreCount, Semaphore** ppWaitSemaphores)
{
	ASSERT(pQueue);
	if (waitSemaphoreCount > 0)
	{
		ASSERT(ppWaitSemaphores);
	}
	ASSERT(pQueue->mtlCommandQueue != nil);

	@autoreleasepool
	{
#ifndef TARGET_IOS
		[pSwapChain->presentCommandBuffer presentDrawable:pSwapChain->mMTKDrawable];
#else
		[pSwapChain->presentCommandBuffer presentDrawable:pSwapChain->mMTKDrawable
									 afterMinimumDuration:1.0 / pSwapChain->pMTKView.preferredFramesPerSecond];
		//[pSwapChain->presentCommandBuffer presentDrawable:pSwapChain->pMTKView.currentDrawable];
#endif
	}

	for (uint32_t i = 0; i < waitSemaphoreCount; i++)
	{
		dispatch_semaphore_wait(ppWaitSemaphores[i]->pMtlSemaphore, DISPATCH_TIME_FOREVER);
	}

	[pSwapChain->presentCommandBuffer commit];

	// after committing a command buffer no more commands can be encoded on it: create a new command buffer for future commands
	pSwapChain->presentCommandBuffer = [pQueue->mtlCommandQueue commandBuffer];
	pSwapChain->mMTKDrawable = nil;
}

void waitForFences(Queue* pQueue, uint32_t fenceCount, Fence** ppFences, bool signal)
{
	ASSERT(pQueue);
	ASSERT(fenceCount);
	ASSERT(ppFences);

	for (uint32_t i = 0; i < fenceCount; i++)
	{
		if (ppFences[i]->mSubmitted)
			dispatch_semaphore_wait(ppFences[i]->pMtlSemaphore, DISPATCH_TIME_FOREVER);
		ppFences[i]->mSubmitted = false;
	}
}

void getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
	ASSERT(pFence);
	*pFenceStatus = FENCE_STATUS_COMPLETE;
	if (pFence->mSubmitted)
	{
		// Check the fence status (and mark it as unsubmitted it if it has succesfully decremented).
		long status = dispatch_semaphore_wait(pFence->pMtlSemaphore, DISPATCH_TIME_NOW);
		if (status == 0)
			pFence->mSubmitted = false;

		*pFenceStatus = (status == 0 ? FENCE_STATUS_COMPLETE : FENCE_STATUS_INCOMPLETE);
	}
}

void getRawTextureHandle(Renderer* pRenderer, Texture* pTexture, void** ppHandle)
{
	ASSERT(pRenderer);
	ASSERT(pTexture);
	ASSERT(ppHandle);

	*ppHandle = (void*)CFBridgingRetain(pTexture->mtlTexture);
}

/************************************************************************/
// Debug Marker Implementation
/************************************************************************/

void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	if (pCmd->mtlRenderEncoder)
		[pCmd->mtlRenderEncoder pushDebugGroup:[NSString stringWithFormat:@"%s", pName]];
	else if (pCmd->mtlComputeEncoder)
		[pCmd->mtlComputeEncoder pushDebugGroup:[NSString stringWithFormat:@"%s", pName]];
	else if (pCmd->mtlBlitEncoder)
		[pCmd->mtlBlitEncoder pushDebugGroup:[NSString stringWithFormat:@"%s", pName]];
	else
		[pCmd->mtlCommandBuffer pushDebugGroup:[NSString stringWithFormat:@"%s", pName]];
}

void cmdBeginDebugMarkerf(Cmd* pCmd, float r, float g, float b, const char* pFormat, ...)
{
	va_list argptr;
	va_start(argptr, pFormat);
	char buffer[65536];
	vsnprintf(buffer, sizeof(buffer), pFormat, argptr);
	va_end(argptr);
	cmdBeginDebugMarker(pCmd, r, g, b, buffer);
}

void cmdEndDebugMarker(Cmd* pCmd)
{
	if (pCmd->mtlRenderEncoder)
		[pCmd->mtlRenderEncoder popDebugGroup];
	else if (pCmd->mtlComputeEncoder)
		[pCmd->mtlComputeEncoder popDebugGroup];
	else if (pCmd->mtlBlitEncoder)
		[pCmd->mtlBlitEncoder popDebugGroup];
	else
		[pCmd->mtlCommandBuffer popDebugGroup];
}

void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	if (pCmd->mtlRenderEncoder)
		[pCmd->mtlRenderEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
	else if (pCmd->mtlComputeEncoder)
		[pCmd->mtlComputeEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
	else if (pCmd->mtlBlitEncoder)
		[pCmd->mtlBlitEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
}

void cmdAddDebugMarkerf(Cmd* pCmd, float r, float g, float b, const char* pFormat, ...)
{
	va_list argptr;
	va_start(argptr, pFormat);
	char buffer[65536];
	vsnprintf(buffer, sizeof(buffer), pFormat, argptr);
	va_end(argptr);

	cmdAddDebugMarker(pCmd, r, g, b, buffer);
}

// -------------------------------------------------------------------------------------------------
// Utility functions
// -------------------------------------------------------------------------------------------------

bool isImageFormatSupported(ImageFormat::Enum format)
{
	bool result = false;
	switch (format)
	{
			// 1 channel
		case ImageFormat::R8: result = true; break;
		case ImageFormat::R16: result = true; break;
		case ImageFormat::R16F: result = true; break;
		case ImageFormat::R32UI: result = true; break;
		case ImageFormat::R32F:
			result = true;
			break;
			// 2 channel
		case ImageFormat::RG8: result = true; break;
		case ImageFormat::RG16: result = true; break;
		case ImageFormat::RG16F: result = true; break;
		case ImageFormat::RG32UI: result = true; break;
		case ImageFormat::RG32F:
			result = true;
			break;
			// 3 channel
		case ImageFormat::RGB8: result = true; break;
		case ImageFormat::RGB16: result = true; break;
		case ImageFormat::RGB16F: result = true; break;
		case ImageFormat::RGB32UI: result = true; break;
		case ImageFormat::RGB32F:
			result = true;
			break;
			// 4 channel
		case ImageFormat::BGRA8: result = true; break;
		case ImageFormat::RGBA16: result = true; break;
		case ImageFormat::RGBA16F: result = true; break;
		case ImageFormat::RGBA32UI: result = true; break;
		case ImageFormat::RGBA32F: result = true; break;

		default: result = false; break;
	}
	return result;
}
// -------------------------------------------------------------------------------------------------
// Internal utility functions
// -------------------------------------------------------------------------------------------------

uint64_t util_pthread_to_uint64(const pthread_t& value)
{
	uint64_t threadId = 0;
	memcpy(&threadId, &value, sizeof(value));
	return threadId;
}

MTLPixelFormat util_to_mtl_pixel_format(const ImageFormat::Enum& format, const bool& srgb)
{
	MTLPixelFormat result = MTLPixelFormatInvalid;

	if (format >= sizeof(gMtlFormatTranslator) / sizeof(gMtlFormatTranslator[0]))
	{
		LOGERROR("Failed to Map from ConfettilFileFromat to MTLPixelFormat, should add map method in gMtlFormatTranslator");
	}
	else
	{
		result = gMtlFormatTranslator[format];
		if (srgb)
		{
			if (result == MTLPixelFormatRGBA8Unorm)
				result = MTLPixelFormatRGBA8Unorm_sRGB;
			else if (result == MTLPixelFormatBGRA8Unorm)
				result = MTLPixelFormatBGRA8Unorm_sRGB;
#ifndef TARGET_IOS
			else if (result == MTLPixelFormatBC1_RGBA)
				result = MTLPixelFormatBC1_RGBA_sRGB;
			else if (result == MTLPixelFormatBC2_RGBA)
				result = MTLPixelFormatBC2_RGBA_sRGB;
			else if (result == MTLPixelFormatBC3_RGBA)
				result = MTLPixelFormatBC3_RGBA_sRGB;
			else if (result == MTLPixelFormatBC7_RGBAUnorm)
				result = MTLPixelFormatBC7_RGBAUnorm_sRGB;
#endif
		}
	}

	return result;
}

bool util_is_mtl_depth_pixel_format(const MTLPixelFormat& format)
{
	return format == MTLPixelFormatDepth32Float || format == MTLPixelFormatDepth32Float_Stencil8
#ifndef TARGET_IOS
		   || format == MTLPixelFormatDepth16Unorm || format == MTLPixelFormatDepth24Unorm_Stencil8
#endif
		;
}

bool util_is_mtl_compressed_pixel_format(const MTLPixelFormat& format)
{
#ifndef TARGET_IOS
	return format == MTLPixelFormatBC1_RGBA || format == MTLPixelFormatBC1_RGBA_sRGB || format == MTLPixelFormatBC2_RGBA ||
		   format == MTLPixelFormatBC2_RGBA_sRGB || format == MTLPixelFormatBC3_RGBA || format == MTLPixelFormatBC3_RGBA_sRGB ||
		   format == MTLPixelFormatBC4_RUnorm || format == MTLPixelFormatBC4_RSnorm || format == MTLPixelFormatBC5_RGUnorm ||
		   format == MTLPixelFormatBC5_RGSnorm || format == MTLPixelFormatBC6H_RGBFloat || format == MTLPixelFormatBC6H_RGBUfloat ||
		   format == MTLPixelFormatBC7_RGBAUnorm || format == MTLPixelFormatBC7_RGBAUnorm_sRGB;
#else
	return false;    // Note: BC texture formats are not supported on iOS.
#endif
}

MTLVertexFormat util_to_mtl_vertex_format(const ImageFormat::Enum& format)
{
	switch (format)
	{
		case ImageFormat::RG8: return MTLVertexFormatUChar2Normalized;
		case ImageFormat::RGB8: return MTLVertexFormatUChar3Normalized;
		case ImageFormat::RGBA8: return MTLVertexFormatUChar4Normalized;

		case ImageFormat::RG8S: return MTLVertexFormatChar2Normalized;
		case ImageFormat::RGB8S: return MTLVertexFormatChar3Normalized;
		case ImageFormat::RGBA8S: return MTLVertexFormatChar4Normalized;

		case ImageFormat::RG16: return MTLVertexFormatUShort2Normalized;
		case ImageFormat::RGB16: return MTLVertexFormatUShort3Normalized;
		case ImageFormat::RGBA16: return MTLVertexFormatUShort4Normalized;

		case ImageFormat::RG16S: return MTLVertexFormatShort2Normalized;
		case ImageFormat::RGB16S: return MTLVertexFormatShort3Normalized;
		case ImageFormat::RGBA16S: return MTLVertexFormatShort4Normalized;

		case ImageFormat::RG16I: return MTLVertexFormatShort2;
		case ImageFormat::RGB16I: return MTLVertexFormatShort3;
		case ImageFormat::RGBA16I: return MTLVertexFormatShort4;

		case ImageFormat::RG16UI: return MTLVertexFormatUShort2;
		case ImageFormat::RGB16UI: return MTLVertexFormatUShort3;
		case ImageFormat::RGBA16UI: return MTLVertexFormatUShort4;

		case ImageFormat::RG16F: return MTLVertexFormatHalf2;
		case ImageFormat::RGB16F: return MTLVertexFormatHalf3;
		case ImageFormat::RGBA16F: return MTLVertexFormatHalf4;

		case ImageFormat::R32F: return MTLVertexFormatFloat;
		case ImageFormat::RG32F: return MTLVertexFormatFloat2;
		case ImageFormat::RGB32F: return MTLVertexFormatFloat3;
		case ImageFormat::RGBA32F: return MTLVertexFormatFloat4;

		case ImageFormat::R32I: return MTLVertexFormatInt;
		case ImageFormat::RG32I: return MTLVertexFormatInt2;
		case ImageFormat::RGB32I: return MTLVertexFormatInt3;
		case ImageFormat::RGBA32I: return MTLVertexFormatInt4;

		case ImageFormat::R32UI: return MTLVertexFormatUInt;
		case ImageFormat::RG32UI: return MTLVertexFormatUInt2;
		case ImageFormat::RGB32UI: return MTLVertexFormatUInt3;
		case ImageFormat::RGBA32UI: return MTLVertexFormatUInt4;

		case ImageFormat::RGB10A2: return MTLVertexFormatUInt1010102Normalized;
		default: break;
	}
	LOGERRORF("Unknown vertex format: %d", format);
	return MTLVertexFormatInvalid;
}

MTLLoadAction util_to_mtl_load_action(const LoadActionType& loadActionType)
{
	if (loadActionType == LOAD_ACTION_DONTCARE)
		return MTLLoadActionDontCare;
	else if (loadActionType == LOAD_ACTION_LOAD)
		return MTLLoadActionLoad;
	else
		return MTLLoadActionClear;
}

void util_bind_argument_buffer(Cmd* pCmd, DescriptorManager* pManager, const DescriptorInfo* descInfo, const DescriptorData* descData)
{
	Buffer* argumentBuffer;
	bool    bufferNeedsReencoding = false;

	id<MTLArgumentEncoder> argumentEncoder = nil;
	id<MTLFunction>        shaderStage = nil;

	// Look for the argument buffer (or create one if needed).
	uint32_t hash = tinystl::hash(descData->pName);
	{
		tinystl::unordered_map<uint32_t, tinystl::pair<Buffer*, bool>>::iterator jt = pManager->mArgumentBuffers.find(hash);
		// If not previous argument buffer was found, create a new bufffer.
		if (jt.node == nil)
		{
			// Find a shader stage using this argument buffer.
			ShaderStage stageMask = descInfo->mDesc.used_stages;
			if ((stageMask & SHADER_STAGE_VERT) != 0)
				shaderStage = pCmd->pShader->mtlVertexShader;
			else if ((stageMask & SHADER_STAGE_FRAG) != 0)
				shaderStage = pCmd->pShader->mtlFragmentShader;
			else if ((stageMask & SHADER_STAGE_COMP) != 0)
				shaderStage = pCmd->pShader->mtlComputeShader;
			assert(shaderStage != nil);

			// Create the argument buffer/encoder pair.
			argumentEncoder = [shaderStage newArgumentEncoderWithBufferIndex:descInfo->mDesc.reg];
			BufferDesc bufferDesc = {};
			bufferDesc.mSize = argumentEncoder.encodedLength;
			bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
			addBuffer(pCmd->pRenderer, &bufferDesc, &argumentBuffer);

			pManager->mArgumentBuffers[hash] = { argumentBuffer, true };
			bufferNeedsReencoding = true;
		}
		else
		{
			argumentBuffer = jt->second.first;
			bufferNeedsReencoding = jt->second.second;
		}
	}

	// Update the argument buffer's data.
	if (bufferNeedsReencoding)
	{
		if (!argumentEncoder)
			argumentEncoder = [shaderStage newArgumentEncoderWithBufferIndex:descInfo->mDesc.reg];

		[argumentEncoder setArgumentBuffer:argumentBuffer->mtlBuffer offset:0];
		for (uint32_t i = 0; i < descData->mCount; i++)
		{
			switch (descInfo->mDesc.mtlArgumentBufferType)
			{
				case DESCRIPTOR_TYPE_SAMPLER: [argumentEncoder setSamplerState:descData->ppSamplers[i]->mtlSamplerState atIndex:i]; break;
				case DESCRIPTOR_TYPE_BUFFER:
					[pCmd->mtlRenderEncoder useResource:descData->ppBuffers[i]->mtlBuffer
												  usage:(MTLResourceUsageRead | MTLResourceUsageSample)];
					[argumentEncoder setBuffer:descData->ppBuffers[i]->mtlBuffer
										offset:(descData->ppBuffers[i]->mPositionInHeap + (descData->pOffsets ? descData->pOffsets[i] : 0))
									   atIndex:i];
					break;
				case DESCRIPTOR_TYPE_TEXTURE:
					[pCmd->mtlRenderEncoder useResource:descData->ppTextures[i]->mtlTexture usage:MTLResourceUsageRead];
					[argumentEncoder setTexture:descData->ppTextures[i]->mtlTexture atIndex:i];
					break;
			}
		}

		pManager->mArgumentBuffers[hash].second = false;
	}

	// Bind the argument buffer.
	if ((descInfo->mDesc.used_stages & SHADER_STAGE_VERT) != 0)
		[pCmd->mtlRenderEncoder setVertexBuffer:argumentBuffer->mtlBuffer
										 offset:argumentBuffer->mPositionInHeap
										atIndex:descInfo->mDesc.reg];
	if ((descInfo->mDesc.used_stages & SHADER_STAGE_FRAG) != 0)
		[pCmd->mtlRenderEncoder setFragmentBuffer:argumentBuffer->mtlBuffer
										   offset:argumentBuffer->mPositionInHeap
										  atIndex:descInfo->mDesc.reg];
	if ((descInfo->mDesc.used_stages & SHADER_STAGE_COMP) != 0)
		[pCmd->mtlComputeEncoder setBuffer:argumentBuffer->mtlBuffer offset:argumentBuffer->mPositionInHeap atIndex:descInfo->mDesc.reg];
}

void util_end_current_encoders(Cmd* pCmd)
{
	if (pCmd->mtlRenderEncoder != nil)
	{
		[pCmd->mtlRenderEncoder endEncoding];
		pCmd->mtlRenderEncoder = nil;
	}
	if (pCmd->mtlComputeEncoder != nil)
	{
		[pCmd->mtlComputeEncoder endEncoding];
		pCmd->mtlComputeEncoder = nil;
	}
	if (pCmd->mtlBlitEncoder != nil)
	{
		[pCmd->mtlBlitEncoder endEncoding];
		pCmd->mtlBlitEncoder = nil;
	}
}

bool util_sync_encoders(Cmd* pCmd, const CmdPoolType& newEncoderType)
{
	if (newEncoderType != CMD_POOL_DIRECT && pCmd->mtlRenderEncoder != nil)
	{
		[pCmd->mtlRenderEncoder updateFence:pCmd->mtlEncoderFence afterStages:MTLRenderStageFragment];
		return true;
	}
	if (newEncoderType != CMD_POOL_COMPUTE && pCmd->mtlComputeEncoder != nil)
	{
		[pCmd->mtlComputeEncoder updateFence:pCmd->mtlEncoderFence];
		return true;
	}
	if (newEncoderType != CMD_POOL_COPY && pCmd->mtlBlitEncoder != nil)
	{
		[pCmd->mtlBlitEncoder updateFence:pCmd->mtlEncoderFence];
		return true;
	}
	return false;
}

bool util_is_compatible_texture_view(const MTLTextureType& textureType, const MTLTextureType& subviewTye)
{
	switch (textureType)
	{
		case MTLTextureType1D:
			if (subviewTye != MTLTextureType1D)
				return false;
			return true;
		case MTLTextureType2D:
			if (subviewTye != MTLTextureType2D && subviewTye != MTLTextureType2DArray)
				return false;
			return true;
		case MTLTextureType2DArray:
		case MTLTextureTypeCube:
		case MTLTextureTypeCubeArray:
			if (subviewTye != MTLTextureType2D && subviewTye != MTLTextureType2DArray && subviewTye != MTLTextureTypeCube &&
				subviewTye != MTLTextureTypeCubeArray)
				return false;
			return true;
		case MTLTextureType3D:
			if (subviewTye != MTLTextureType3D)
				return false;
			return true;
		default: return false;
	}

	return false;
}

void add_texture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture, const bool isRT)
{
	Texture* pTexture = (Texture*)conf_calloc(1, sizeof(*pTexture));
	ASSERT(pTexture);

	if (pDesc->mHeight == 1)
		((TextureDesc*)pDesc)->mMipLevels = 1;

	pTexture->mDesc = *pDesc;
	pTexture->mTextureId = (++gTextureIds << 8U) + util_pthread_to_uint64(Thread::GetCurrentThreadID());

	pTexture->mtlPixelFormat = util_to_mtl_pixel_format(pTexture->mDesc.mFormat, pTexture->mDesc.mSrgb);
#ifndef TARGET_IOS
	if (pTexture->mtlPixelFormat == MTLPixelFormatDepth24Unorm_Stencil8 && ![pRenderer->pDevice isDepth24Stencil8PixelFormatSupported])
	{
		internal_log(LOG_TYPE_WARN, "Format D24S8 is not supported on this device. Using D32 instead", "addTexture");
		pTexture->mtlPixelFormat = MTLPixelFormatDepth32Float;
		pTexture->mDesc.mFormat = ImageFormat::D32F;
	}
#endif

	pTexture->mIsCompressed = util_is_mtl_compressed_pixel_format(pTexture->mtlPixelFormat);
	Image img;
	img.RedefineDimensions(
		pTexture->mDesc.mFormat, pTexture->mDesc.mWidth, pTexture->mDesc.mHeight, pTexture->mDesc.mDepth, pTexture->mDesc.mMipLevels);
	pTexture->mTextureSize = img.GetMipMappedSize(0, pTexture->mDesc.mMipLevels);
	if (pTexture->mDesc.mHostVisible)
	{
		internal_log(
			LOG_TYPE_WARN, "Host visible textures are not supported, memory of resulting texture will not be mapped for CPU visibility",
			"addTexture");
	}

	// If we've passed a native handle, it means the texture is already on device memory, and we just need to assign it.
	if (pDesc->pNativeHandle)
	{
		pTexture->mOwnsImage = false;
		pTexture->mtlTexture = (id<MTLTexture>)CFBridgingRelease(pDesc->pNativeHandle);
	}
	// Otherwise, we need to create a new texture.
	else
	{
		pTexture->mOwnsImage = true;

		// Create a MTLTextureDescriptor that matches our requirements.
		MTLTextureDescriptor* textureDesc = [[MTLTextureDescriptor alloc] init];

		textureDesc.pixelFormat = pTexture->mtlPixelFormat;
		textureDesc.width = pTexture->mDesc.mWidth;
		textureDesc.height = pTexture->mDesc.mHeight;
		textureDesc.depth = pTexture->mDesc.mDepth;
		textureDesc.mipmapLevelCount = pTexture->mDesc.mMipLevels;
		textureDesc.sampleCount = pTexture->mDesc.mSampleCount;
		textureDesc.arrayLength = pTexture->mDesc.mArraySize;
		textureDesc.storageMode = MTLStorageModePrivate;
		textureDesc.cpuCacheMode = MTLCPUCacheModeDefaultCache;

		if (pDesc->mDepth > 1)
		{
			textureDesc.textureType = MTLTextureType3D;
		}
		else if (pDesc->mHeight > 1)
		{
			if (DESCRIPTOR_TYPE_TEXTURE_CUBE == (pDesc->mDescriptors & DESCRIPTOR_TYPE_TEXTURE_CUBE))
			{
				if (pTexture->mDesc.mArraySize == 6)
				{
					textureDesc.textureType = MTLTextureTypeCube;
					textureDesc.arrayLength = 1;
				}
#ifndef TARGET_IOS
				else
				{
					textureDesc.textureType = MTLTextureTypeCubeArray;
					textureDesc.arrayLength /= 6;
				}
#else
				else if ([pRenderer->pDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v1])
				{
					textureDesc.textureType = MTLTextureTypeCubeArray;
					textureDesc.arrayLength /= 6;
				}
				else
				{
					internal_log(LOG_TYPE_ERROR, "Cube Array textures are not supported on this iOS device", "addTexture");
				}
#endif
			}
			else
			{
				if (pDesc->mArraySize > 1)
					textureDesc.textureType = MTLTextureType2DArray;
				else
					textureDesc.textureType = MTLTextureType2D;
			}
		}
		else
		{
			if (pDesc->mArraySize > 1)
				textureDesc.textureType = MTLTextureType1DArray;
			else
				textureDesc.textureType = MTLTextureType1D;
		}

		bool isDepthBuffer = util_is_mtl_depth_pixel_format(pTexture->mtlPixelFormat);
		bool isMultiSampled = pTexture->mDesc.mSampleCount > 1;
		if (isDepthBuffer || isMultiSampled)
			textureDesc.resourceOptions = MTLResourceStorageModePrivate;
#ifdef TARGET_IOS
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_ON_TILE)
		{
			textureDesc.resourceOptions = MTLResourceStorageModeMemoryless;
		}
#endif

		if (isRT || isDepthBuffer)
			textureDesc.usage |= MTLTextureUsageRenderTarget;
		//Create texture views only if DESCRIPTOR_RW_TEXTURE was used.
		if ((pTexture->mDesc.mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE) != 0)
		{
			textureDesc.usage |= MTLTextureUsagePixelFormatView;
			textureDesc.usage |= MTLTextureUsageShaderWrite;
		}

		// Allocate the texture's memory.
		AllocatorMemoryRequirements mem_reqs = { 0 };
		mem_reqs.usage = (ResourceMemoryUsage)RESOURCE_MEMORY_USAGE_GPU_ONLY;
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT)
			mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT;
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_BIT)
			mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_SHARED_BIT;
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT)
			mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_SHARED_ADAPTER_BIT;

		TextureCreateInfo alloc_info = { textureDesc, isRT || isDepthBuffer, isMultiSampled };
		bool              allocSuccess;
		allocSuccess = createTexture(pRenderer->pResourceAllocator, &alloc_info, &mem_reqs, pTexture);
		ASSERT(allocSuccess);
	}

	NSRange slices = NSMakeRange(0, pDesc->mArraySize);

	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
	{
		MTLTextureType uavType = pTexture->mtlTexture.textureType;
		if (pTexture->mtlTexture.textureType == MTLTextureTypeCube || pTexture->mtlTexture.textureType == MTLTextureTypeCubeArray)
		{
			uavType = MTLTextureType2DArray;
		}
		pTexture->pMtlUAVDescriptors = (id<MTLTexture> __strong*)conf_calloc(pDesc->mMipLevels, sizeof(id<MTLTexture>));
		for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
		{
			NSRange levels = NSMakeRange(i, 1);
			pTexture->pMtlUAVDescriptors[i] = [pTexture->mtlTexture newTextureViewWithPixelFormat:pTexture->mtlTexture.pixelFormat
																					  textureType:uavType
																						   levels:levels
																						   slices:slices];
		}
	}

	*ppTexture = pTexture;
}

/************************************************************************/
/************************************************************************/
#endif    // RENDERER_IMPLEMENTATION

#if defined(__cplusplus) && defined(RENDERER_CPP_NAMESPACE)
}    // namespace RENDERER_CPP_NAMESPACE
#endif
#endif

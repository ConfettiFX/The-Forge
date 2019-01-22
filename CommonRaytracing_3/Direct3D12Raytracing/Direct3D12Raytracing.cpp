// DXR
#include "../ThirdParty/DXR/include/d3d12_1.h"
#include "../ThirdParty/DXR/include/D3D12RaytracingFallback.h"

#include <d3dcompiler.h>

// OS
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/unordered_set.h"

// Renderer
#include "../Interfaces/IRaytracing.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/Direct3D12/Direct3D12Hooks.h"
#include "../../Common_3/Renderer/Direct3D12/Direct3D12MemoryAllocator.h"
#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN ((D3D12_GPU_VIRTUAL_ADDRESS)-1)

typedef struct DescriptorStoreHeap
{
	uint32_t mNumDescriptors;
	/// DescriptorInfo Increment Size
	uint32_t mDescriptorSize;
	/// Bitset for finding SAFE_FREE descriptor slots
	uint32_t* flags;
	/// Lock for multi-threaded descriptor allocations
	Mutex*   pAllocationMutex;
	uint64_t mUsedDescriptors;
	/// Type of descriptor heap -> CBV / DSV / ...
	D3D12_DESCRIPTOR_HEAP_TYPE mType;
	/// DX Heap
	ID3D12DescriptorHeap* pCurrentHeap;
	/// Start position in the heap
	D3D12_CPU_DESCRIPTOR_HANDLE mStartCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mStartGpuHandle;
} DescriptorStoreHeap;

extern void addBuffer(Renderer* pRenderer, const BufferDesc* desc, Buffer** pp_buffer);
extern void removeBuffer(Renderer* pRenderer, Buffer* p_buffer);

extern void add_descriptor_heap(
	Renderer* pRenderer, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, uint32_t numDescriptors,
	struct DescriptorStoreHeap** ppDescHeap);
extern void reset_descriptor_heap(struct DescriptorStoreHeap* pHeap);
extern void remove_descriptor_heap(struct DescriptorStoreHeap* pHeap);
extern D3D12_CPU_DESCRIPTOR_HANDLE
			add_cpu_descriptor_handles(struct DescriptorStoreHeap* pHeap, uint32_t numDescriptors, uint32_t* pDescriptorIndex = NULL);
extern void add_gpu_descriptor_handles(
	struct DescriptorStoreHeap* pHeap, D3D12_CPU_DESCRIPTOR_HANDLE* pStartCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* pStartGpuHandle,
	uint32_t numDescriptors);
extern void remove_gpu_descriptor_handles(DescriptorStoreHeap* pHeap, D3D12_GPU_DESCRIPTOR_HANDLE* startHandle, uint64_t numDescriptors);
/************************************************************************/
// Static initialization of DX Raytracing API
/************************************************************************/
// Enable experimental features and return if they are supported.
// To test them being supported we need to check both their enablement as well as device creation afterwards.
inline bool EnableD3D12ExperimentalFeatures(UUID* experimentalFeatures, uint32_t featureCount)
{
	ID3D12Device* testDevice = NULL;
	bool          ret = SUCCEEDED(D3D12EnableExperimentalFeatures(featureCount, experimentalFeatures, nullptr, nullptr)) &&
			   SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)));
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

// Enable experimental features required for driver and compute-based fallback raytracing.
// This will set active D3D12 devices to DEVICE_REMOVED state.
// Returns bool whether the call succeeded and the device supports the feature.
inline bool EnableRaytracing()
{
	UUID experimentalFeatures[] = { D3D12ExperimentalShaderModels, D3D12RaytracingPrototype };
	return EnableD3D12ExperimentalFeatures(experimentalFeatures, 2);
}

struct RaytracingStaticInitializer
{
	RaytracingStaticInitializer()
	{
		mFallback = false;

		if (!EnableRaytracing())
		{
			mFallback = true;

			InfoMsg(
				"Could not enable raytracing driver (D3D12EnableExperimentalFeatures() failed).\n"
				"Possible reasons:\n"
				"  1) your OS is not in developer mode.\n"
				"  2) your GPU driver doesn't match the D3D12 runtime loaded by the app (d3d12.dll and friends).\n"
				"  3) your D3D12 runtime doesn't match the D3D12 headers used by your app (in particular, the GUID passed to "
				"D3D12EnableExperimentalFeatures).\n\n");

			InfoMsg("Enabling compute based fallback raytracing support.\n");
			if (!EnableComputeRaytracingFallback())
			{
				ErrorMsg("Could not enable compute based fallback raytracing support (D3D12EnableExperimentalFeatures() failed).\n");
				ExitProcess(-1);
			}
		}
	}

	bool mFallback;

} gRaytracingStaticInitializer;
/************************************************************************/
// Utility Functions Declarations
/************************************************************************/
D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE        util_to_dx_acceleration_structure_type(AccelerationStructureType type);
D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS util_to_dx_acceleration_structure_build_flags(AccelerationStructureBuildFlags flags);
D3D12_RAYTRACING_GEOMETRY_FLAGS                     util_to_dx_geometry_flags(AccelerationStructureGeometryFlags flags);
D3D12_RAYTRACING_INSTANCE_FLAGS                     util_to_dx_instance_flags(AccelerationStructureInstanceFlags flags);
// Create a wrapped pointer for the Fallback Layer path.
WRAPPED_GPU_POINTER util_create_fallback_wrapped_pointer(Raytracing* pRaytracing, ID3D12Resource* resource, UINT bufferNumElements);
WRAPPED_GPU_POINTER util_create_fallback_wrapped_pointer(Raytracing* pRaytracing, ID3D12Resource* resource, UINT bufferNumElements);
/************************************************************************/
// Forge Raytracing Implementation using DXR
/************************************************************************/
struct Raytracing
{
	Renderer*                        pRenderer;
	ID3D12DeviceRaytracingPrototype* pDxrDevice;
	ID3D12RaytracingFallbackDevice*  pFallbackDevice;
	//DescriptorStoreHeap*			  pDescriptorHeap;
	uint64_t mDescriptorsAllocated;
};

struct RaytracingShader
{
	ID3DBlob* pShaderBlob;
	LPCWSTR   pName;
};

struct AccelerationStructure
{
	Buffer*                                             pBuffer;
	D3D12_RAYTRACING_GEOMETRY_DESC*                     pGeometryDescs;
	Buffer*                                             pInstanceDescBuffer;
	WRAPPED_GPU_POINTER                                 mFallbackPointer;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE        mType;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS mFlags;
	uint32_t                                            mDescCount;
};

struct RaytracingPipeline
{
	union
	{
		ID3D12StateObjectPrototype*          pDxrPipeline;
		ID3D12RaytracingFallbackStateObject* pFallbackPipeline;
	};
};

struct RaytracingShaderTable
{
	RaytracingPipeline*         pPipeline;
	Buffer*                     pBuffer;
	D3D12_GPU_DESCRIPTOR_HANDLE mViewGpuDescriptorHandle[DESCRIPTOR_UPDATE_FREQ_COUNT];
	D3D12_GPU_DESCRIPTOR_HANDLE mSamplerGpuDescriptorHandle[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t                    mViewDescriptorCount[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t                    mSamplerDescriptorCount[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint64_t                    mMaxEntrySize;
	uint64_t                    mMissRecordSize;
	uint64_t                    mHitGroupRecordSize;
};

void initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing)
{
	ASSERT(pRenderer);
	ASSERT(ppRaytracing);

	Raytracing* pRaytracing = (Raytracing*)conf_calloc(1, sizeof(*pRaytracing));
	ASSERT(pRaytracing);

	pRaytracing->pRenderer = pRenderer;

	if (gRaytracingStaticInitializer.mFallback)
	{
		D3D12CreateRaytracingFallbackDevice(
			pRenderer->pDxDevice, CreateRaytracingFallbackDeviceFlags::ForceComputeFallback, 0,
			IID_PPV_ARGS(&pRaytracing->pFallbackDevice));
	}
	else
	{
		pRenderer->pDxDevice->QueryInterface(&pRaytracing->pDxrDevice);
	}

	*ppRaytracing = pRaytracing;
}

void removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing)
{
	ASSERT(pRenderer);
	ASSERT(pRaytracing);

	if (pRaytracing->pDxrDevice)
		pRaytracing->pDxrDevice->Release();

	if (pRaytracing->pFallbackDevice)
		pRaytracing->pFallbackDevice->Release();

	conf_free(pRaytracing);
}

void addAccelerationStructure(
	Raytracing* pRaytracing, const AccelerationStructureDesc* pDesc, unsigned* pScratchBufferSize,
	AccelerationStructure** ppAccelerationStructure)
{
	ASSERT(pRaytracing);
	ASSERT(pDesc);
	ASSERT(ppAccelerationStructure);

	AccelerationStructure* pAccelerationStructure = (AccelerationStructure*)conf_calloc(1, sizeof(*pAccelerationStructure));
	ASSERT(pAccelerationStructure);

	pAccelerationStructure->mType = util_to_dx_acceleration_structure_type(pDesc->mType);
	pAccelerationStructure->mFlags = util_to_dx_acceleration_structure_build_flags(pDesc->mFlags);
	pAccelerationStructure->mDescCount = pDesc->mDescCount;

	if (pDesc->mType == ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL)
	{
		pAccelerationStructure->pGeometryDescs =
			(D3D12_RAYTRACING_GEOMETRY_DESC*)conf_calloc(pDesc->mDescCount, sizeof(D3D12_RAYTRACING_GEOMETRY_DESC));
		for (uint32_t i = 0; i < pDesc->mDescCount; ++i)
		{
			AccelerationStructureGeometryDesc* pGeom = &pDesc->pGeometryDescs[i];
			D3D12_RAYTRACING_GEOMETRY_DESC*    pOut = &pAccelerationStructure->pGeometryDescs[i];
			if (pGeom->mType == ACCELERATION_STRUCTURE_GEOMETRY_TYPE_TRIANGLES)
			{
				pOut->Flags = util_to_dx_geometry_flags(pGeom->mFlags);

				if (pGeom->pIndexBuffer)
				{
					pOut->Triangles.IndexBuffer = pGeom->pIndexBuffer->mDxGpuAddress;
					pOut->Triangles.IndexCount =
						(UINT)pGeom->pIndexBuffer->mDesc.mSize /
						(pGeom->pIndexBuffer->mDesc.mIndexType == INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t));
					pOut->Triangles.IndexFormat = pGeom->pIndexBuffer->mDxIndexFormat;
					;
				}

				pOut->Triangles.VertexBuffer.StartAddress = pGeom->pVertexBuffer->mDxGpuAddress;
				pOut->Triangles.VertexBuffer.StrideInBytes = (UINT)pGeom->pVertexBuffer->mDesc.mVertexStride;
				pOut->Triangles.VertexCount = (UINT)pGeom->pVertexBuffer->mDesc.mSize / (UINT)pGeom->pVertexBuffer->mDesc.mVertexStride;
				if (pOut->Triangles.VertexBuffer.StrideInBytes == sizeof(float))
					pOut->Triangles.VertexFormat = DXGI_FORMAT_R32_FLOAT;
				else if (pOut->Triangles.VertexBuffer.StrideInBytes == sizeof(float) * 2)
					pOut->Triangles.VertexFormat = DXGI_FORMAT_R32G32_FLOAT;
				else if (pOut->Triangles.VertexBuffer.StrideInBytes == sizeof(float) * 3)
					pOut->Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				else if (pOut->Triangles.VertexBuffer.StrideInBytes == sizeof(float) * 4)
					pOut->Triangles.VertexFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
			}
			// #TODO
			else if (pGeom->mType == ACCELERATION_STRUCTURE_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS)
			{
			}
		}
	}
	/************************************************************************/
	// Get the size requirement for the Acceleration Structures
	/************************************************************************/
	D3D12_GET_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO_DESC prebuildDesc = {};
	prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	prebuildDesc.Flags = pAccelerationStructure->mFlags;
	prebuildDesc.NumDescs = pDesc->mDescCount;
	prebuildDesc.pGeometryDescs = pAccelerationStructure->pGeometryDescs;
	prebuildDesc.Type = pAccelerationStructure->mType;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	if (gRaytracingStaticInitializer.mFallback)
		pRaytracing->pFallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);
	else
		pRaytracing->pDxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);
	/************************************************************************/
	// Create Instance Data Buffer for Top Level Acceleration Structure
	/************************************************************************/
	if (pDesc->mType == ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL)
	{
		if (gRaytracingStaticInitializer.mFallback)
		{
			tinystl::vector<D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC> fallbackInstanceDescs(pDesc->mDescCount);

			for (uint32_t i = 0; i < pDesc->mDescCount; ++i)
			{
				AccelerationStructureInstanceDesc* pInst = &pDesc->pInstanceDescs[i];
				fallbackInstanceDescs[i].AccelerationStructure = util_create_fallback_wrapped_pointer(
					pRaytracing, pInst->pAccelerationStructure->pBuffer->pDxResource,
					(UINT)pInst->pAccelerationStructure->pBuffer->mDesc.mSize / sizeof(UINT));
				fallbackInstanceDescs[i].Flags = util_to_dx_instance_flags(pInst->mFlags);
				fallbackInstanceDescs[i].InstanceContributionToHitGroupIndex = pInst->mInstanceContributionToHitGroupIndex;
				fallbackInstanceDescs[i].InstanceID = pInst->mInstanceID;
				fallbackInstanceDescs[i].InstanceMask = pInst->mInstanceMask;
				memcpy(fallbackInstanceDescs[i].Transform, pInst->mTransform, sizeof(float[12]));
			}

			BufferDesc instanceDesc = {};
			instanceDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			instanceDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
			instanceDesc.mSize = fallbackInstanceDescs.size() * sizeof(fallbackInstanceDescs[0]);
			addBuffer(pRaytracing->pRenderer, &instanceDesc, &pAccelerationStructure->pInstanceDescBuffer);
			memcpy(pAccelerationStructure->pInstanceDescBuffer->pCpuMappedAddress, fallbackInstanceDescs.data(), instanceDesc.mSize);
		}
		else
		{
			tinystl::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs(pDesc->mDescCount);

			for (uint32_t i = 0; i < pDesc->mDescCount; ++i)
			{
				AccelerationStructureInstanceDesc* pInst = &pDesc->pInstanceDescs[i];
				instanceDescs[i].AccelerationStructure = pInst->pAccelerationStructure->pBuffer->pDxResource->GetGPUVirtualAddress();
				instanceDescs[i].Flags = util_to_dx_instance_flags(pInst->mFlags);
				instanceDescs[i].InstanceContributionToHitGroupIndex = pInst->mInstanceContributionToHitGroupIndex;
				instanceDescs[i].InstanceID = pInst->mInstanceID;
				instanceDescs[i].InstanceMask = pInst->mInstanceMask;
				memcpy(instanceDescs[i].Transform, pInst->mTransform, sizeof(float[12]));
			}

			BufferDesc instanceDesc = {};
			instanceDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			instanceDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
			instanceDesc.mSize = instanceDescs.size() * sizeof(instanceDescs[0]);
			addBuffer(pRaytracing->pRenderer, &instanceDesc, &pAccelerationStructure->pInstanceDescBuffer);
			memcpy(pAccelerationStructure->pInstanceDescBuffer->pCpuMappedAddress, instanceDescs.data(), instanceDesc.mSize);
		}
	}
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
	bufferDesc.mSize = info.ResultDataMaxSizeInBytes;
	if (gRaytracingStaticInitializer.mFallback)
		bufferDesc.mStartState = (ResourceState)pRaytracing->pFallbackDevice->GetAccelerationStructureResourceState();
	else
		bufferDesc.mStartState = (ResourceState)D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	addBuffer(pRaytracing->pRenderer, &bufferDesc, &pAccelerationStructure->pBuffer);
	/************************************************************************/
	// Create Fallback Pointer for Top Level Strcuture
	/************************************************************************/
	if (gRaytracingStaticInitializer.mFallback && pDesc->mType == ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL)
	{
		pAccelerationStructure->mFallbackPointer = util_create_fallback_wrapped_pointer(
			pRaytracing, pAccelerationStructure->pBuffer->pDxResource, (UINT)info.ResultDataMaxSizeInBytes / sizeof(UINT));
	}
	/************************************************************************/
	// Store the scratch buffer size so user can create the scratch buffer accordingly
	/************************************************************************/
	*pScratchBufferSize = (UINT)info.ScratchDataSizeInBytes;
	/************************************************************************/
	/************************************************************************/
	*ppAccelerationStructure = pAccelerationStructure;
}

void removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
	ASSERT(pRaytracing);
	ASSERT(pAccelerationStructure);

	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pBuffer);
	if (pAccelerationStructure->mType == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL)
		removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pInstanceDescBuffer);

	if (pAccelerationStructure->mType == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL)
		conf_free(pAccelerationStructure->pGeometryDescs);

	conf_free(pAccelerationStructure);
}

void addRaytracingShader(
	Raytracing* pRaytracing, const unsigned char* pByteCode, unsigned byteCodeSize, const char* pName, RaytracingShader** ppShader)
{
	ASSERT(pRaytracing);
	ASSERT(pByteCode);
	ASSERT(byteCodeSize);
	ASSERT(pName);
	ASSERT(ppShader);

	RaytracingShader* pShader = (RaytracingShader*)conf_calloc(1, sizeof(*pShader));
	ASSERT(pShader);

	D3DCreateBlob(byteCodeSize, &pShader->pShaderBlob);
	memcpy(pShader->pShaderBlob->GetBufferPointer(), pByteCode, byteCodeSize);

	pShader->pName = (WCHAR*)conf_calloc(strlen(pName) + 1, sizeof(WCHAR));
	mbstowcs((WCHAR*)pShader->pName, pName, strlen(pName));

	*ppShader = pShader;
}

void removeRaytracingShader(Raytracing* pRaytracing, RaytracingShader* pShader)
{
	ASSERT(pRaytracing);
	ASSERT(pShader);

	pShader->pShaderBlob->Release();

	conf_free((WCHAR*)pShader->pName);
	conf_free(pShader);
}

typedef struct UpdateFrequencyLayoutInfo
{
	tinystl::vector<DescriptorInfo*>                  mCbvSrvUavTable;
	tinystl::vector<DescriptorInfo*>                  mSamplerTable;
	tinystl::vector<DescriptorInfo*>                  mConstantParams;
	tinystl::vector<DescriptorInfo*>                  mRootConstants;
	tinystl::unordered_map<DescriptorInfo*, uint32_t> mDescriptorIndexMap;
} UpdateFrequencyLayoutInfo;

static const RootSignatureDesc gDefaultRootSignatureDesc = {};
extern void                    create_descriptor_table_1_0(
					   uint32_t numDescriptors, DescriptorInfo** tableRef, D3D12_DESCRIPTOR_RANGE* pRange, D3D12_ROOT_PARAMETER* pRootParam);
extern void                  create_root_descriptor_1_0(const DescriptorInfo* pDesc, D3D12_ROOT_PARAMETER* pRootParam);
extern void                  create_root_constant_1_0(const DescriptorInfo* pDesc, D3D12_ROOT_PARAMETER* pRootParam);
extern const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName, uint32_t* pIndex);

void addRaytracingRootSignature(
	Raytracing* pRaytracing, const ShaderResource* pResources, uint32_t resourceCount, bool local, RootSignature** ppRootSignature,
	const RootSignatureDesc* pRootDesc)
{
	ASSERT(pRaytracing);
	ASSERT(ppRootSignature);

	RootSignature* pRootSignature = (RootSignature*)conf_calloc(1, sizeof(*pRootSignature));
	ASSERT(pRootSignature);

	pRootSignature->mDescriptorCount = resourceCount;
	// Raytracing is executed in compute path
	pRootSignature->mPipelineType = PIPELINE_TYPE_COMPUTE;

	if (resourceCount)
		pRootSignature->pDescriptors =
			(DescriptorInfo*)conf_calloc(pRootSignature->mDescriptorCount, sizeof(*pRootSignature->pDescriptors));

	//pRootSignature->pDescriptorNameToIndexMap;
	conf_placement_new<tinystl::unordered_map<uint32_t, uint32_t> >(&pRootSignature->pDescriptorNameToIndexMap);

	const RootSignatureDesc* pRootSignatureDesc = pRootDesc ? pRootDesc : &gDefaultRootSignatureDesc;

	tinystl::unordered_map<tinystl::string, Sampler*> staticSamplerMap;
	for (uint32_t i = 0; i < pRootSignatureDesc->mStaticSamplerCount; ++i)
		staticSamplerMap.insert({ pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i] });

	tinystl::vector<UpdateFrequencyLayoutInfo>                 layouts(DESCRIPTOR_UPDATE_FREQ_COUNT);
	tinystl::vector<tinystl::pair<DescriptorInfo*, Sampler*> > staticSamplers;
	/************************************************************************/
	// Fill Descriptor Info
	/************************************************************************/
	// Fill the descriptor array to be stored in the root signature
	for (uint32_t i = 0; i < resourceCount; ++i)
	{
		DescriptorInfo*       pDesc = &pRootSignature->pDescriptors[i];
		const ShaderResource* pRes = &pResources[i];
		uint32_t              setIndex = pRes->set;
		if (pRes->size == 0)
			setIndex = 0;

		DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)setIndex;

		pDesc->mDesc.reg = pRes->reg;
		pDesc->mDesc.set = pRes->set;
		pDesc->mDesc.size = pRes->size;
		pDesc->mDesc.type = pRes->type;
		pDesc->mDesc.used_stages = SHADER_STAGE_COMP;
		pDesc->mUpdateFrquency = updateFreq;

		pDesc->mDesc.name_size = pRes->name_size;
		pDesc->mDesc.name = (const char*)conf_calloc(pDesc->mDesc.name_size + 1, sizeof(char));
		memcpy((char*)pDesc->mDesc.name, pRes->name, pRes->name_size);

		if (pDesc->mDesc.size == 0 && pDesc->mDesc.type == DESCRIPTOR_TYPE_TEXTURE)
		{
			pDesc->mDesc.size = pRootSignatureDesc->mMaxBindlessTextures;
		}

		// Find the D3D12 type of the descriptors
		if (pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER)
		{
			// If the sampler is a static sampler, no need to put it in the descriptor table
			const tinystl::unordered_hash_node<tinystl::string, Sampler*>* pNode = staticSamplerMap.find(pDesc->mDesc.name).node;

			if (pNode)
			{
				LOGINFOF("Descriptor (%s) : User specified Static Sampler", pDesc->mDesc.name);
				// Set the index to invalid value so we can use this later for error checking if user tries to update a static sampler
				pDesc->mIndexInParent = ~0u;
				staticSamplers.push_back({ pDesc, pNode->second });
			}
			else
			{
				// In D3D12, sampler descriptors cannot be placed in a table containing view descriptors
				layouts[setIndex].mSamplerTable.emplace_back(pDesc);
			}
		}
		// No support for arrays of constant buffers to be used as root descriptors as this might bloat the root signature size
		else if (
			(pDesc->mDesc.type == DESCRIPTOR_TYPE_UNIFORM_BUFFER && pDesc->mDesc.size == 1) ||
			pDesc->mDesc.type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
		{
			// D3D12 has no special syntax to declare root constants like Vulkan
			// So we assume that all constant buffers with the word "rootconstant" (case insensitive) are root constants
			if (tinystl::string(pRes->name).to_lower().find("rootconstant", 0) != tinystl::string::npos ||
				pDesc->mDesc.type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
			{
				// Make the root param a 32 bit constant if the user explicitly specifies it in the shader
				pDesc->mDxType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
				pDesc->mDesc.type = DESCRIPTOR_TYPE_ROOT_CONSTANT;
				layouts[0].mRootConstants.emplace_back(pDesc);

				pDesc->mDesc.size = pDesc->mDesc.size / sizeof(uint32_t);
			}
			else
			{
				// By default DESCRIPTOR_TYPE_UNIFORM_BUFFER maps to D3D12_ROOT_PARAMETER_TYPE_CBV
				// But since the size of root descriptors is 2 DWORDS, some of these uniform buffers might get placed in descriptor tables
				// if the size of the root signature goes over the max recommended size on the specific hardware
				layouts[setIndex].mConstantParams.emplace_back(pDesc);
				pDesc->mDxType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			}
		}
		else
		{
			layouts[setIndex].mCbvSrvUavTable.emplace_back(pDesc);
		}

		pRootSignature->pDescriptorNameToIndexMap.insert({ tinystl::hash(pDesc->mDesc.name), i });
		layouts[setIndex].mDescriptorIndexMap[pDesc] = i;
	}

	tinystl::vector<tinystl::vector<D3D12_DESCRIPTOR_RANGE> > cbvSrvUavRange_1_0((uint32_t)layouts.size());
	tinystl::vector<tinystl::vector<D3D12_DESCRIPTOR_RANGE> > samplerRange_1_0((uint32_t)layouts.size());
	tinystl::vector<D3D12_ROOT_PARAMETER>                     rootParams_1_0;

	tinystl::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplerDescs(staticSamplers.size());
	for (uint32_t i = 0; i < (uint32_t)staticSamplers.size(); ++i)
	{
		staticSamplerDescs[i].Filter = staticSamplers[i].second->mDxSamplerDesc.Filter;
		staticSamplerDescs[i].AddressU = staticSamplers[i].second->mDxSamplerDesc.AddressU;
		staticSamplerDescs[i].AddressV = staticSamplers[i].second->mDxSamplerDesc.AddressV;
		staticSamplerDescs[i].AddressW = staticSamplers[i].second->mDxSamplerDesc.AddressW;
		staticSamplerDescs[i].MipLODBias = staticSamplers[i].second->mDxSamplerDesc.MipLODBias;
		staticSamplerDescs[i].MaxAnisotropy = staticSamplers[i].second->mDxSamplerDesc.MaxAnisotropy;
		staticSamplerDescs[i].ComparisonFunc = staticSamplers[i].second->mDxSamplerDesc.ComparisonFunc;
		staticSamplerDescs[i].MinLOD = staticSamplers[i].second->mDxSamplerDesc.MinLOD;
		staticSamplerDescs[i].MaxLOD = staticSamplers[i].second->mDxSamplerDesc.MaxLOD;
		staticSamplerDescs[i].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		staticSamplerDescs[i].RegisterSpace = staticSamplers[i].first->mDesc.set;
		staticSamplerDescs[i].ShaderRegister = staticSamplers[i].first->mDesc.reg;
		staticSamplerDescs[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	for (uint32_t i = 0; i < (uint32_t)layouts.size(); ++i)
	{
		cbvSrvUavRange_1_0[i].resize(layouts[i].mCbvSrvUavTable.size());
		samplerRange_1_0[i].resize(layouts[i].mSamplerTable.size());
	}

	pRootSignature->mDescriptorCount = resourceCount;

	for (uint32_t i = 0; i < (uint32_t)layouts.size(); ++i)
	{
		pRootSignature->mDxRootConstantCount += (uint32_t)layouts[i].mRootConstants.size();
		pRootSignature->mDxRootDescriptorCount += (uint32_t)layouts[i].mConstantParams.size();
	}
	if (pRootSignature->mDxRootConstantCount)
		pRootSignature->pDxRootConstantRootIndices =
			(uint32_t*)conf_calloc(pRootSignature->mDxRootConstantCount, sizeof(*pRootSignature->pDxRootConstantRootIndices));
	if (pRootSignature->mDxRootDescriptorCount)
		pRootSignature->pDxRootDescriptorRootIndices =
			(uint32_t*)conf_calloc(pRootSignature->mDxRootDescriptorCount, sizeof(*pRootSignature->pDxRootDescriptorRootIndices));
	/************************************************************************/
	// We dont expose binding of the Top Level AS to the app
	// Just add the root parameter for it here
	// Currently Framework assumes Top Level AS will always be bound to shader register T0
	// #TODO: Figure out a way for app to use Top Level AS using DescriptorTables instead of RootDescriptors
	/************************************************************************/
	if (!local)
	{
		D3D12_ROOT_PARAMETER topLevelAS = {};
		topLevelAS.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		topLevelAS.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		topLevelAS.Descriptor.RegisterSpace = 0;
		topLevelAS.Descriptor.ShaderRegister = 0;
		rootParams_1_0.emplace_back(topLevelAS);
	}
	// Start collecting root parameters
	// Start with root descriptors since they will be the most frequently updated descriptors
	// This also makes sure that if we spill, the root descriptors in the front of the root signature will most likely still remain in the root
	uint32_t rootDescriptorIndex = 0;
	// Collect all root descriptors
	// Put most frequently changed params first
	for (uint32_t i = (uint32_t)layouts.size(); i-- > 0U;)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[i];
		if (layout.mConstantParams.size())
		{
			for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mConstantParams.size(); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mConstantParams[descIndex];
				pDesc->mIndexInParent = rootDescriptorIndex;
				pRootSignature->pDxRootDescriptorRootIndices[pDesc->mIndexInParent] = (uint32_t)rootParams_1_0.size();

				D3D12_ROOT_PARAMETER rootParam_1_0;
				create_root_descriptor_1_0(pDesc, &rootParam_1_0);

				rootParams_1_0.push_back(rootParam_1_0);

				++rootDescriptorIndex;
			}
		}
	}

	uint32_t rootConstantIndex = 0;

	// Collect all root constants
	for (uint32_t setIndex = 0; setIndex < (uint32_t)layouts.size(); ++setIndex)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[setIndex];

		if (!layout.mRootConstants.size())
			continue;

		for (uint32_t i = 0; i < (uint32_t)layouts[setIndex].mRootConstants.size(); ++i)
		{
			DescriptorInfo* pDesc = layout.mRootConstants[i];
			pDesc->mIndexInParent = rootConstantIndex;
			pRootSignature->pDxRootConstantRootIndices[pDesc->mIndexInParent] = (uint32_t)rootParams_1_0.size();

			D3D12_ROOT_PARAMETER rootParam_1_0;
			create_root_constant_1_0(pDesc, &rootParam_1_0);

			rootParams_1_0.push_back(rootParam_1_0);

			++rootConstantIndex;
		}
	}

	// Collect descriptor table parameters
	// Put most frequently changed descriptor tables in the front of the root signature
	for (uint32_t i = (uint32_t)layouts.size(); i-- > 0U;)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[i];

		// Fill the descriptor table layout for the view descriptor table of this update frequency
		if (layout.mCbvSrvUavTable.size())
		{
			// sort table by type (CBV/SRV/UAV) by register by space
			layout.mCbvSrvUavTable.sort(
				[](DescriptorInfo* const& lhs, DescriptorInfo* const& rhs) { return (int)(lhs->mDesc.reg - rhs->mDesc.reg); });
			layout.mCbvSrvUavTable.sort(
				[](DescriptorInfo* const& lhs, DescriptorInfo* const& rhs) { return (int)(lhs->mDesc.set - rhs->mDesc.set); });
			layout.mCbvSrvUavTable.sort(
				[](DescriptorInfo* const& lhs, DescriptorInfo* const& rhs) { return (int)(lhs->mDesc.type - rhs->mDesc.type); });

			D3D12_ROOT_PARAMETER rootParam_1_0;
			create_descriptor_table_1_0(
				(uint32_t)layout.mCbvSrvUavTable.size(), layout.mCbvSrvUavTable.data(), cbvSrvUavRange_1_0[i].data(), &rootParam_1_0);

			// Store some of the binding info which will be required later when binding the descriptor table
			// We need the root index when calling SetRootDescriptorTable
			pRootSignature->mDxViewDescriptorTableRootIndices[i] = (uint32_t)rootParams_1_0.size();
			pRootSignature->mDxViewDescriptorCounts[i] = (uint32_t)layout.mCbvSrvUavTable.size();
			pRootSignature->pDxViewDescriptorIndices[i] = (uint32_t*)conf_calloc(layout.mCbvSrvUavTable.size(), sizeof(uint32_t));

			for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mCbvSrvUavTable.size(); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mCbvSrvUavTable[descIndex];
				pDesc->mIndexInParent = descIndex;

				// Store the d3d12 related info in the descriptor to avoid constantly calling the util_to_dx mapping functions
				pDesc->mDxType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				pDesc->mHandleIndex = pRootSignature->mDxCumulativeViewDescriptorCounts[i];

				// Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
				// This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays) in the descriptor table
				pRootSignature->mDxCumulativeViewDescriptorCounts[i] += pDesc->mDesc.size;
				pRootSignature->pDxViewDescriptorIndices[i][descIndex] = layout.mDescriptorIndexMap[pDesc];
			}

			rootParams_1_0.push_back(rootParam_1_0);
		}

		// Fill the descriptor table layout for the sampler descriptor table of this update frequency
		if (layout.mSamplerTable.size())
		{
			D3D12_ROOT_PARAMETER rootParam_1_0;
			create_descriptor_table_1_0(
				(uint32_t)layout.mSamplerTable.size(), layout.mSamplerTable.data(), samplerRange_1_0[i].data(), &rootParam_1_0);

			// Store some of the binding info which will be required later when binding the descriptor table
			// We need the root index when calling SetRootDescriptorTable
			pRootSignature->mDxSamplerDescriptorTableRootIndices[i] = (uint32_t)rootParams_1_0.size();
			pRootSignature->mDxSamplerDescriptorCounts[i] = (uint32_t)layout.mSamplerTable.size();
			pRootSignature->pDxSamplerDescriptorIndices[i] = (uint32_t*)conf_calloc(layout.mSamplerTable.size(), sizeof(uint32_t));
			//table.pDescriptorIndices = (uint32_t*)conf_calloc(table.mDescriptorCount, sizeof(uint32_t));

			for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mSamplerTable.size(); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mSamplerTable[descIndex];
				pDesc->mIndexInParent = descIndex;

				// Store the d3d12 related info in the descriptor to avoid constantly calling the util_to_dx mapping functions
				pDesc->mDxType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				pDesc->mHandleIndex = pRootSignature->mDxCumulativeSamplerDescriptorCounts[i];

				// Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
				// This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays) in the descriptor table
				pRootSignature->mDxCumulativeSamplerDescriptorCounts[i] += pDesc->mDesc.size;
				pRootSignature->pDxSamplerDescriptorIndices[i][descIndex] = layout.mDescriptorIndexMap[pDesc];
			}

			rootParams_1_0.push_back(rootParam_1_0);
		}
	}

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = local ? D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE : D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSignatureDesc.NumParameters = (UINT)rootParams_1_0.size();
	rootSignatureDesc.NumStaticSamplers = (UINT)staticSamplerDescs.size();
	rootSignatureDesc.pParameters = rootParams_1_0.data();
	rootSignatureDesc.pStaticSamplers = staticSamplerDescs.data();
	ID3DBlob* error_msgs = NULL;

	if (gRaytracingStaticInitializer.mFallback)
	{
		HRESULT hr = pRaytracing->pFallbackDevice->D3D12SerializeRootSignature(
			&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pRootSignature->pDxSerializedRootSignatureString, &error_msgs);

		if (!SUCCEEDED(hr))
		{
			char* pMsg = (char*)conf_calloc(error_msgs->GetBufferSize(), sizeof(char));
			memcpy(pMsg, error_msgs->GetBufferPointer(), error_msgs->GetBufferSize());
			LOGERRORF("Failed to serialize root signature with error (%s)", pMsg);
			conf_free(pMsg);
		}

		ASSERT(pRootSignature->pDxSerializedRootSignatureString);

		hr = pRaytracing->pFallbackDevice->CreateRootSignature(
			0, pRootSignature->pDxSerializedRootSignatureString->GetBufferPointer(),
			pRootSignature->pDxSerializedRootSignatureString->GetBufferSize(), IID_PPV_ARGS(&(pRootSignature->pDxRootSignature)));
		ASSERT(SUCCEEDED(hr));
	}
	else
	{
		HRESULT hr = D3D12SerializeRootSignature(
			&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pRootSignature->pDxSerializedRootSignatureString, &error_msgs);

		if (!SUCCEEDED(hr))
		{
			char* pMsg = (char*)conf_calloc(error_msgs->GetBufferSize(), sizeof(char));
			memcpy(pMsg, error_msgs->GetBufferPointer(), error_msgs->GetBufferSize());
			LOGERRORF("Failed to serialize root signature with error (%s)", pMsg);
			conf_free(pMsg);
		}

		ASSERT(pRootSignature->pDxSerializedRootSignatureString);

		hr = pRaytracing->pRenderer->pDxDevice->CreateRootSignature(
			0, pRootSignature->pDxSerializedRootSignatureString->GetBufferPointer(),
			pRootSignature->pDxSerializedRootSignatureString->GetBufferSize(), IID_PPV_ARGS(&pRootSignature->pDxRootSignature));
		ASSERT(SUCCEEDED(hr));
	}
	/************************************************************************/
	/************************************************************************/
	*ppRootSignature = pRootSignature;
}

void addRaytracingPipeline(Raytracing* pRaytracing, const RaytracingPipelineDesc* pDesc, RaytracingPipeline** ppPipeline)
{
	ASSERT(pRaytracing);
	ASSERT(pDesc);
	ASSERT(ppPipeline);

	RaytracingPipeline* pPipeline = (RaytracingPipeline*)conf_calloc(1, sizeof(*pPipeline));
	ASSERT(pPipeline);
	/************************************************************************/
	// Pipeline Creation
	/************************************************************************/
	tinystl::vector<D3D12_STATE_SUBOBJECT>                                             subobjects;
	tinystl::vector<tinystl::pair<uint32_t, D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*> > exportAssociationsDelayed;
	// Reserve average number of subobject space in the beginning
	subobjects.reserve(10);
	/************************************************************************/
	// Step 1 - Create DXIL Libraries
	/************************************************************************/
	tinystl::vector<D3D12_DXIL_LIBRARY_DESC> dxilLibDescs;
	tinystl::vector<D3D12_STATE_SUBOBJECT>   stateSubobject = {};
	tinystl::vector<D3D12_EXPORT_DESC*>      exportDesc = {};

	D3D12_DXIL_LIBRARY_DESC rayGenDesc = {};
	D3D12_EXPORT_DESC       rayGenExportDesc = {};
	rayGenExportDesc.ExportToRename = NULL;
	rayGenExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;
	rayGenExportDesc.Name = pDesc->pRayGenShader->pName;

	rayGenDesc.DXILLibrary.BytecodeLength = pDesc->pRayGenShader->pShaderBlob->GetBufferSize();
	rayGenDesc.DXILLibrary.pShaderBytecode = pDesc->pRayGenShader->pShaderBlob->GetBufferPointer();
	rayGenDesc.NumExports = 1;
	rayGenDesc.pExports = &rayGenExportDesc;

	dxilLibDescs.emplace_back(rayGenDesc);

	for (uint32_t i = 0; i < pDesc->mMissShaderCount; ++i)
	{
		D3D12_EXPORT_DESC* pMissExportDesc = (D3D12_EXPORT_DESC*)conf_calloc(1, sizeof(*pMissExportDesc));
		pMissExportDesc->ExportToRename = NULL;
		pMissExportDesc->Flags = D3D12_EXPORT_FLAG_NONE;
		pMissExportDesc->Name = pDesc->ppMissShaders[i]->pName;

		D3D12_DXIL_LIBRARY_DESC missDesc = {};
		missDesc.DXILLibrary.BytecodeLength = pDesc->ppMissShaders[i]->pShaderBlob->GetBufferSize();
		missDesc.DXILLibrary.pShaderBytecode = pDesc->ppMissShaders[i]->pShaderBlob->GetBufferPointer();
		missDesc.NumExports = 1;
		missDesc.pExports = pMissExportDesc;

		exportDesc.emplace_back(pMissExportDesc);
		dxilLibDescs.emplace_back(missDesc);
	}

	for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
	{
		if (pDesc->pHitGroups[i].pIntersectionShader)
		{
			D3D12_EXPORT_DESC* pIntersectionExportDesc = (D3D12_EXPORT_DESC*)conf_calloc(1, sizeof(*pIntersectionExportDesc));
			pIntersectionExportDesc->ExportToRename = NULL;
			pIntersectionExportDesc->Flags = D3D12_EXPORT_FLAG_NONE;
			pIntersectionExportDesc->Name = pDesc->pHitGroups[i].pIntersectionShader->pName;

			D3D12_DXIL_LIBRARY_DESC intersectionDesc = {};
			intersectionDesc.DXILLibrary.BytecodeLength = pDesc->pHitGroups[i].pIntersectionShader->pShaderBlob->GetBufferSize();
			intersectionDesc.DXILLibrary.pShaderBytecode = pDesc->pHitGroups[i].pIntersectionShader->pShaderBlob->GetBufferPointer();
			intersectionDesc.NumExports = 1;
			intersectionDesc.pExports = pIntersectionExportDesc;

			exportDesc.emplace_back(pIntersectionExportDesc);
			dxilLibDescs.emplace_back(intersectionDesc);
		}
		if (pDesc->pHitGroups[i].pAnyHitShader)
		{
			D3D12_EXPORT_DESC* pAnyHitExportDesc = (D3D12_EXPORT_DESC*)conf_calloc(1, sizeof(*pAnyHitExportDesc));
			pAnyHitExportDesc->ExportToRename = NULL;
			pAnyHitExportDesc->Flags = D3D12_EXPORT_FLAG_NONE;
			pAnyHitExportDesc->Name = pDesc->pHitGroups[i].pAnyHitShader->pName;

			D3D12_DXIL_LIBRARY_DESC anyHitDesc = {};
			anyHitDesc.DXILLibrary.BytecodeLength = pDesc->pHitGroups[i].pAnyHitShader->pShaderBlob->GetBufferSize();
			anyHitDesc.DXILLibrary.pShaderBytecode = pDesc->pHitGroups[i].pAnyHitShader->pShaderBlob->GetBufferPointer();
			anyHitDesc.NumExports = 1;
			anyHitDesc.pExports = pAnyHitExportDesc;

			exportDesc.emplace_back(pAnyHitExportDesc);
			dxilLibDescs.emplace_back(anyHitDesc);
		}
		if (pDesc->pHitGroups[i].pClosestHitShader)
		{
			D3D12_EXPORT_DESC* pClosestHitExportDesc = (D3D12_EXPORT_DESC*)conf_calloc(1, sizeof(*pClosestHitExportDesc));
			pClosestHitExportDesc->ExportToRename = NULL;
			pClosestHitExportDesc->Flags = D3D12_EXPORT_FLAG_NONE;
			pClosestHitExportDesc->Name = pDesc->pHitGroups[i].pClosestHitShader->pName;

			D3D12_DXIL_LIBRARY_DESC closestHitDesc = {};
			closestHitDesc.DXILLibrary.BytecodeLength = pDesc->pHitGroups[i].pClosestHitShader->pShaderBlob->GetBufferSize();
			closestHitDesc.DXILLibrary.pShaderBytecode = pDesc->pHitGroups[i].pClosestHitShader->pShaderBlob->GetBufferPointer();
			closestHitDesc.NumExports = 1;
			closestHitDesc.pExports = pClosestHitExportDesc;

			exportDesc.emplace_back(pClosestHitExportDesc);
			dxilLibDescs.emplace_back(closestHitDesc);
		}
	}

	for (uint32_t i = 0; i < (uint32_t)dxilLibDescs.size(); ++i)
	{
		D3D12_STATE_SUBOBJECT subobject = {};
		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		subobject.pDesc = &dxilLibDescs[i];
		subobjects.emplace_back(subobject);
	}
	/************************************************************************/
	// Step 2 - Create Hit Groups
	/************************************************************************/
	tinystl::vector<D3D12_HIT_GROUP_DESC>  hitGroupDescs(pDesc->mHitGroupCount);
	tinystl::vector<D3D12_STATE_SUBOBJECT> hitGroupObjects(pDesc->mHitGroupCount);
	tinystl::vector<WCHAR*>                hitGroupNames(pDesc->mHitGroupCount);

	for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
	{
		const RaytracingHitGroup* pHitGroup = &pDesc->pHitGroups[i];
		ASSERT(pDesc->pHitGroups[i].pHitGroupName);
		hitGroupNames[i] = (WCHAR*)conf_calloc(strlen(pDesc->pHitGroups[i].pHitGroupName) + 1, sizeof(WCHAR));
		mbstowcs(hitGroupNames[i], pDesc->pHitGroups[i].pHitGroupName, strlen(pDesc->pHitGroups[i].pHitGroupName));

		hitGroupDescs[i].AnyHitShaderImport = pHitGroup->pAnyHitShader ? pHitGroup->pAnyHitShader->pName : NULL;
		hitGroupDescs[i].ClosestHitShaderImport = pHitGroup->pClosestHitShader ? pHitGroup->pClosestHitShader->pName : NULL;
		hitGroupDescs[i].HitGroupExport = hitGroupNames[i];
		hitGroupDescs[i].IntersectionShaderImport = pHitGroup->pIntersectionShader ? pHitGroup->pIntersectionShader->pName : NULL;

		hitGroupObjects[i].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		hitGroupObjects[i].pDesc = &hitGroupDescs[i];
		subobjects.emplace_back(hitGroupObjects[i]);
	}
	/************************************************************************/
	// Step 4 = Pipeline Config
	/************************************************************************/
	D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
	pipelineConfig.MaxTraceRecursionDepth = pDesc->mMaxTraceRecursionDepth;
	D3D12_STATE_SUBOBJECT pipelineConfigObject = {};
	pipelineConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
	pipelineConfigObject.pDesc = &pipelineConfig;
	subobjects.emplace_back(pipelineConfigObject);
	/************************************************************************/
	// Step 5 - Global Root Signature
	/************************************************************************/
	D3D12_STATE_SUBOBJECT globalRootSignatureObject = {};
	globalRootSignatureObject.Type = D3D12_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
	globalRootSignatureObject.pDesc = pDesc->pGlobalRootSignature ? &pDesc->pGlobalRootSignature->pDxRootSignature : NULL;
	subobjects.emplace_back(globalRootSignatureObject);
	/************************************************************************/
	// Step 6 - Local Root Signatures
	/************************************************************************/
	// Local Root Signature for Ray Generation Shader
	D3D12_STATE_SUBOBJECT                  rayGenRootSignatureObject = {};
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rayGenRootSignatureAssociation = {};
	if (pDesc->pRayGenRootSignature)
	{
		rayGenRootSignatureObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
		rayGenRootSignatureObject.pDesc = &pDesc->pRayGenRootSignature->pDxRootSignature;
		subobjects.emplace_back(rayGenRootSignatureObject);

		rayGenRootSignatureAssociation.NumExports = 1;
		rayGenRootSignatureAssociation.pExports = &pDesc->pRayGenShader->pName;

		exportAssociationsDelayed.push_back({ (uint32_t)subobjects.size() - 1, &rayGenRootSignatureAssociation });
	}

	// Local Root Signatures for Miss Shaders
	tinystl::vector<D3D12_STATE_SUBOBJECT>                  missRootSignatures(pDesc->mMissShaderCount);
	tinystl::vector<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION> missRootSignaturesAssociation(pDesc->mMissShaderCount);
	for (uint32_t i = 0; i < pDesc->mMissShaderCount; ++i)
	{
		if (pDesc->ppMissRootSignatures && pDesc->ppMissRootSignatures[i])
		{
			missRootSignatures[i].Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
			missRootSignatures[i].pDesc = &pDesc->ppMissRootSignatures[i]->pDxRootSignature;
			subobjects.emplace_back(missRootSignatures[i]);

			missRootSignaturesAssociation[i].NumExports = 1;
			missRootSignaturesAssociation[i].pExports = &pDesc->ppMissShaders[i]->pName;

			exportAssociationsDelayed.push_back({ (uint32_t)subobjects.size() - 1, &missRootSignaturesAssociation[i] });
		}
	}

	// Local Root Signatures for Hit Groups
	tinystl::vector<D3D12_STATE_SUBOBJECT>                  hitGroupRootSignatures(pDesc->mHitGroupCount);
	tinystl::vector<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION> hitGroupRootSignatureAssociation(pDesc->mHitGroupCount);
	for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
	{
		if (pDesc->pHitGroups[i].pRootSignature)
		{
			hitGroupRootSignatures[i].Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
			hitGroupRootSignatures[i].pDesc = &pDesc->pHitGroups[i].pRootSignature->pDxRootSignature;
			subobjects.emplace_back(hitGroupRootSignatures[i]);

			hitGroupRootSignatureAssociation[i].NumExports = 1;
			hitGroupRootSignatureAssociation[i].pExports = &hitGroupDescs[i].HitGroupExport;

			exportAssociationsDelayed.push_back({ (uint32_t)subobjects.size() - 1, &hitGroupRootSignatureAssociation[i] });
		}
	}
	/************************************************************************/
	// Export Associations
	/************************************************************************/
	for (uint32_t i = 0; i < (uint32_t)exportAssociationsDelayed.size(); ++i)
	{
		exportAssociationsDelayed[i].second->pSubobjectToAssociate = &subobjects[exportAssociationsDelayed[i].first];

		D3D12_STATE_SUBOBJECT exportAssociationObject = {};
		exportAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		exportAssociationObject.pDesc = exportAssociationsDelayed[i].second;
		subobjects.push_back(exportAssociationObject);
	}
	/************************************************************************/
	// Step 7 - Create State Object
	/************************************************************************/
	D3D12_STATE_OBJECT_DESC pipelineDesc = {};
	pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	pipelineDesc.NumSubobjects = (UINT)subobjects.size();
	pipelineDesc.pSubobjects = subobjects.data();

	// Create the state object.
	if (gRaytracingStaticInitializer.mFallback)
	{
		HRESULT hr = pRaytracing->pFallbackDevice->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&pPipeline->pFallbackPipeline));
		ASSERT(SUCCEEDED(hr));
	}
	else    // DirectX Raytracing
	{
		HRESULT hr = pRaytracing->pDxrDevice->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&pPipeline->pDxrPipeline));
		ASSERT(SUCCEEDED(hr));
	}
	/************************************************************************/
	// Clean up
	/************************************************************************/
	for (uint32_t i = 0; i < (uint32_t)exportDesc.size(); ++i)
		conf_free(exportDesc[i]);

	for (uint32_t i = 0; i < (uint32_t)hitGroupNames.size(); ++i)
		conf_free(hitGroupNames[i]);
	/************************************************************************/
	/************************************************************************/
	*ppPipeline = pPipeline;
}

void removeRaytracingPipeline(Raytracing* pRaytracing, RaytracingPipeline* pPipeline)
{
	ASSERT(pRaytracing);
	ASSERT(pPipeline);

	if (gRaytracingStaticInitializer.mFallback)
		pPipeline->pFallbackPipeline->Release();
	else
		pPipeline->pDxrPipeline->Release();

	conf_free(pPipeline);
}

static const uint32_t gLocalRootConstantSize = sizeof(UINT);
static const uint32_t gLocalRootDescriptorSize = sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
static const uint32_t gLocalRootDescriptorTableSize = sizeof(D3D12_GPU_DESCRIPTOR_HANDLE);

void addRaytracingShaderTable(Raytracing* pRaytracing, const RaytracingShaderTableDesc* pDesc, RaytracingShaderTable** ppTable)
{
	ASSERT(pRaytracing);
	ASSERT(pDesc);
	ASSERT(pDesc->pPipeline);
	ASSERT(ppTable);

	RaytracingShaderTable* pTable = (RaytracingShaderTable*)conf_calloc(1, sizeof(*pTable));
	ASSERT(pTable);

	pTable->pPipeline = pDesc->pPipeline;

	uint64_t shaderIdentifierSize = 0;
	if (gRaytracingStaticInitializer.mFallback)
		shaderIdentifierSize = (uint64_t)pRaytracing->pFallbackDevice->GetShaderIdentifierSize();
	else
		shaderIdentifierSize = (uint64_t)pRaytracing->pDxrDevice->GetShaderIdentifierSize();

	ASSERT(shaderIdentifierSize);

	const uint32_t recordCount = 1 + pDesc->mMissShaderCount + pDesc->mHitGroupCount;
	uint64_t       maxShaderTableSize = 0;
	/************************************************************************/
	// Calculate max size for each element in the shader table
	/************************************************************************/
	auto CalculateMaxShaderRecordSize = [&](const RaytracingShaderTableRecordDesc* pRecords, uint32_t shaderCount) {
		for (uint32_t i = 0; i < shaderCount; ++i)
		{
			tinystl::unordered_set<uint32_t>       addedTables;
			const RaytracingShaderTableRecordDesc* pRecord = &pRecords[i];
			uint32_t                               shaderSize = 0;
			for (uint32_t desc = 0; desc < pRecord->mRootDataCount; ++desc)
			{
				uint32_t              descIndex = -1;
				const DescriptorInfo* pDesc = get_descriptor(pRecord->pRootSignature, pRecord->pRootData[desc].pName, &descIndex);
				ASSERT(pDesc);

				switch (pDesc->mDxType)
				{
					case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS: shaderSize += pDesc->mDesc.size * gLocalRootConstantSize; break;
					case D3D12_ROOT_PARAMETER_TYPE_CBV:
					case D3D12_ROOT_PARAMETER_TYPE_SRV:
					case D3D12_ROOT_PARAMETER_TYPE_UAV: shaderSize += gLocalRootDescriptorSize; break;
					case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
					{
						const uint32_t rootIndex =
							pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER
								? pRecord->pRootSignature->mDxSamplerDescriptorTableRootIndices[pDesc->mUpdateFrquency]
								: pRecord->pRootSignature->mDxViewDescriptorTableRootIndices[pDesc->mUpdateFrquency];
						if (addedTables.find(rootIndex) == addedTables.end())
							shaderSize += gLocalRootDescriptorTableSize;
						else
							addedTables.insert(rootIndex);
						break;
					}
					default: break;
				}
			}

			maxShaderTableSize = max(maxShaderTableSize, shaderSize);
		}
	};

	CalculateMaxShaderRecordSize(pDesc->pRayGenShader, 1);
	CalculateMaxShaderRecordSize(pDesc->pMissShaders, pDesc->mMissShaderCount);
	CalculateMaxShaderRecordSize(pDesc->pHitGroups, pDesc->mHitGroupCount);
	/************************************************************************/
	// Align max size
	/************************************************************************/
	maxShaderTableSize = round_up_64(shaderIdentifierSize + maxShaderTableSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
	pTable->mMaxEntrySize = maxShaderTableSize;
	/************************************************************************/
	// Create shader table buffer
	/************************************************************************/
	BufferDesc bufferDesc = {};
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	bufferDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	bufferDesc.mSize = maxShaderTableSize * recordCount;
	addBuffer(pRaytracing->pRenderer, &bufferDesc, &pTable->pBuffer);
	/************************************************************************/
	// Copy shader identifiers into the buffer
	/************************************************************************/
	ID3D12StateObjectPropertiesPrototype* pRtsoProps = NULL;
	if (!gRaytracingStaticInitializer.mFallback)
		pDesc->pPipeline->pDxrPipeline->QueryInterface(IID_PPV_ARGS(&pRtsoProps));

	uint32_t index = 0;

	auto FillShaderIdentifiers = [&](const RaytracingShaderTableRecordDesc* pRecords, uint32_t shaderCount) {
		for (uint32_t i = 0; i < shaderCount; ++i)
		{
			tinystl::unordered_set<uint32_t> addedTables;

			const RaytracingShaderTableRecordDesc* pRecord = &pRecords[i];
			void*                                  pIdentifier = NULL;
			WCHAR*                                 pName = (WCHAR*)conf_calloc(strlen(pRecord->pName) + 1, sizeof(WCHAR));
			mbstowcs(pName, pRecord->pName, strlen(pRecord->pName));

			if (gRaytracingStaticInitializer.mFallback)
				pIdentifier = pDesc->pPipeline->pFallbackPipeline->GetShaderIdentifier(pName);
			else
				pIdentifier = pRtsoProps->GetShaderIdentifier(pName);

			ASSERT(pIdentifier);
			conf_free(pName);

			uint64_t currentPosition = maxShaderTableSize * index++;
			memcpy((uint8_t*)pTable->pBuffer->pCpuMappedAddress + currentPosition, pIdentifier, shaderIdentifierSize);

			if (!pRecord->pRootSignature)
				continue;

			currentPosition += shaderIdentifierSize;
			/************************************************************************/
			// #NOTE : User can specify root data in any order but we need to fill
			// it into the buffer based on the root index associated with each root data entry
			// So we collect them here and do a lookup when looping through the descriptor array
			// from the root signature
			/************************************************************************/
			tinystl::unordered_map<uint32_t, const DescriptorData*> data;
			for (uint32_t desc = 0; desc < pRecord->mRootDataCount; ++desc)
			{
				data.insert({ tinystl::hash(pRecord->pRootData[desc].pName), &pRecord->pRootData[desc] });
			}

			for (uint32_t desc = 0; desc < pRecord->pRootSignature->mDescriptorCount; ++desc)
			{
				uint32_t              descIndex = -1;
				const DescriptorInfo* pDesc = &pRecord->pRootSignature->pDescriptors[desc];
				const DescriptorData* pData = data.find(tinystl::hash(pDesc->mDesc.name)).node->second;

				switch (pDesc->mDxType)
				{
					case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
					{
						memcpy(
							(uint8_t*)pTable->pBuffer->pCpuMappedAddress + currentPosition, pData->pRootConstant,
							pDesc->mDesc.size * sizeof(uint32_t));
						currentPosition += pDesc->mDesc.size * sizeof(uint32_t);
						break;
					}
					case D3D12_ROOT_PARAMETER_TYPE_CBV:
					case D3D12_ROOT_PARAMETER_TYPE_SRV:
					case D3D12_ROOT_PARAMETER_TYPE_UAV:
					{
						// Root Descriptors need to be aligned to 8 byte address
						currentPosition = round_up_64(currentPosition, gLocalRootDescriptorSize);
						D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = pData->ppBuffers[0]->pDxResource->GetGPUVirtualAddress() +
															   pData->ppBuffers[0]->mPositionInHeap + pData->pOffsets[0];
						memcpy(
							(uint8_t*)pTable->pBuffer->pCpuMappedAddress + currentPosition, &cbvAddress, sizeof(D3D12_GPU_VIRTUAL_ADDRESS));
						currentPosition += gLocalRootDescriptorSize;
						break;
					}
					case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
					{
						RootSignature* pRootSignature = pRecord->pRootSignature;
						const uint32_t setIndex = pDesc->mUpdateFrquency;
						const uint32_t rootIndex = pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER
													   ? pRootSignature->mDxSamplerDescriptorTableRootIndices[pDesc->mUpdateFrquency]
													   : pRootSignature->mDxViewDescriptorTableRootIndices[pDesc->mUpdateFrquency];
						// Construct a new descriptor table from shader visible heap
						if (addedTables.find(rootIndex) == addedTables.end())
						{
							D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
							D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
							if (pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER)
							{
								add_gpu_descriptor_handles(
									pRaytracing->pRenderer->pSamplerHeap[0], &cpuHandle, &gpuHandle,
									pRootSignature->mDxCumulativeSamplerDescriptorCounts[setIndex]);
								pTable->mSamplerGpuDescriptorHandle[pDesc->mUpdateFrquency] = gpuHandle;
								pTable->mSamplerDescriptorCount[pDesc->mUpdateFrquency] =
									pRootSignature->mDxCumulativeSamplerDescriptorCounts[setIndex];

								for (uint32_t i = 0; i < pRootSignature->mDxSamplerDescriptorCounts[setIndex]; ++i)
								{
									const DescriptorInfo* pTableDesc =
										&pRootSignature->pDescriptors[pRootSignature->pDxSamplerDescriptorIndices[setIndex][i]];
									const DescriptorData* pTableData = data.find(tinystl::hash(pTableDesc->mDesc.name)).node->second;
									const uint32_t        arrayCount = max(1U, pTableData->mCount);
									for (uint32_t samplerIndex = 0; samplerIndex < arrayCount; ++samplerIndex)
									{
										pRaytracing->pRenderer->pDxDevice->CopyDescriptorsSimple(
											1,
											{ cpuHandle.ptr + (pTableDesc->mHandleIndex + samplerIndex) *
																  pRaytracing->pRenderer->pSamplerHeap[0]->mDescriptorSize },
											pTableData->ppSamplers[samplerIndex]->mDxSamplerHandle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
									}
								}
							}
							else
							{
								add_gpu_descriptor_handles(
									pRaytracing->pRenderer->pCbvSrvUavHeap[0], &cpuHandle, &gpuHandle,
									pRootSignature->mDxCumulativeViewDescriptorCounts[setIndex]);
								pTable->mViewGpuDescriptorHandle[pDesc->mUpdateFrquency] = gpuHandle;
								pTable->mViewDescriptorCount[pDesc->mUpdateFrquency] =
									pRootSignature->mDxCumulativeViewDescriptorCounts[setIndex];

								for (uint32_t i = 0; i < pRootSignature->mDxViewDescriptorCounts[setIndex]; ++i)
								{
									const DescriptorInfo* pTableDesc =
										&pRootSignature->pDescriptors[pRootSignature->pDxViewDescriptorIndices[setIndex][i]];
									const DescriptorData* pTableData = data.find(tinystl::hash(pTableDesc->mDesc.name)).node->second;
									const DescriptorType  type = pTableDesc->mDesc.type;
									const uint32_t        arrayCount = max(1U, pTableData->mCount);
									switch (type)
									{
										case DESCRIPTOR_TYPE_TEXTURE:
											for (uint32_t textureIndex = 0; textureIndex < arrayCount; ++textureIndex)
											{
												pRaytracing->pRenderer->pDxDevice->CopyDescriptorsSimple(
													1,
													{ cpuHandle.ptr + (pTableDesc->mHandleIndex + textureIndex) *
																		  pRaytracing->pRenderer->pCbvSrvUavHeap[0]->mDescriptorSize },
													pTableData->ppTextures[textureIndex]->mDxSRVDescriptor,
													D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
											}
											break;
										case DESCRIPTOR_TYPE_RW_TEXTURE:
											for (uint32_t textureIndex = 0; textureIndex < arrayCount; ++textureIndex)
											{
												pRaytracing->pRenderer->pDxDevice->CopyDescriptorsSimple(
													1,
													{ cpuHandle.ptr + (pTableDesc->mHandleIndex + textureIndex) *
																		  pRaytracing->pRenderer->pCbvSrvUavHeap[0]->mDescriptorSize },
													pTableData->ppTextures[textureIndex]->pDxUAVDescriptors[pTableData->mUAVMipSlice],
													D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
											}
											break;
										case DESCRIPTOR_TYPE_BUFFER:
											for (uint32_t bufferIndex = 0; bufferIndex < arrayCount; ++bufferIndex)
											{
												pRaytracing->pRenderer->pDxDevice->CopyDescriptorsSimple(
													1,
													{ cpuHandle.ptr + (pTableDesc->mHandleIndex + bufferIndex) *
																		  pRaytracing->pRenderer->pCbvSrvUavHeap[0]->mDescriptorSize },
													pTableData->ppBuffers[bufferIndex]->mDxSrvHandle,
													D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
											}
											break;
										case DESCRIPTOR_TYPE_RW_BUFFER:
											for (uint32_t bufferIndex = 0; bufferIndex < arrayCount; ++bufferIndex)
											{
												pRaytracing->pRenderer->pDxDevice->CopyDescriptorsSimple(
													1,
													{ cpuHandle.ptr + (pTableDesc->mHandleIndex + bufferIndex) *
																		  pRaytracing->pRenderer->pCbvSrvUavHeap[0]->mDescriptorSize },
													pTableData->ppBuffers[bufferIndex]->mDxUavHandle,
													D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
											}
											break;
											// #TODO : Add DESCRIPTOR_TYPE_UNIFORM_BUFFER if needed.
											// Currently we use root descriptors for uniform buffers in raytracing root signature
										default: break;
									}
								}
							}

							// Root Descriptor Tables need to be aligned to 8 byte address
							currentPosition = round_up_64(currentPosition, gLocalRootDescriptorSize);
							memcpy(
								(uint8_t*)pTable->pBuffer->pCpuMappedAddress + currentPosition, &gpuHandle,
								sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
							currentPosition += gLocalRootDescriptorTableSize;
						}
						else
						{
							addedTables.insert(rootIndex);
						}
						break;
					}
					default: break;
				}
			}
		}
	};

	FillShaderIdentifiers(pDesc->pRayGenShader, 1);

	pTable->mMissRecordSize = maxShaderTableSize * pDesc->mMissShaderCount;
	FillShaderIdentifiers(pDesc->pMissShaders, pDesc->mMissShaderCount);

	pTable->mHitGroupRecordSize = maxShaderTableSize * pDesc->mHitGroupCount;
	FillShaderIdentifiers(pDesc->pHitGroups, pDesc->mHitGroupCount);

	if (pRtsoProps)
		pRtsoProps->Release();
	/************************************************************************/
	/************************************************************************/
	*ppTable = pTable;
}

void removeRaytracingShaderTable(Raytracing* pRaytracing, RaytracingShaderTable* pTable)
{
	ASSERT(pRaytracing);
	ASSERT(pTable);

	removeBuffer(pRaytracing->pRenderer, pTable->pBuffer);

	for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
	{
		if (pTable->mViewGpuDescriptorHandle[i].ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
			remove_gpu_descriptor_handles(
				pRaytracing->pRenderer->pCbvSrvUavHeap[0], &pTable->mViewGpuDescriptorHandle[i], pTable->mViewDescriptorCount[i]);

		if (pTable->mSamplerGpuDescriptorHandle[i].ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
			remove_gpu_descriptor_handles(
				pRaytracing->pRenderer->pCbvSrvUavHeap[0], &pTable->mSamplerGpuDescriptorHandle[i], pTable->mSamplerDescriptorCount[i]);
	}

	conf_free(pTable);
}
/************************************************************************/
// Raytracing Command Buffer Functions Implementation
/************************************************************************/
void cmdBuildAccelerationStructure(
	Cmd* pCmd, Raytracing* pRaytracing, Buffer* pScratchBuffer, AccelerationStructure* pAccelerationStructure)
{
	ASSERT(pCmd);
	ASSERT(pRaytracing);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	buildDesc.Type = pAccelerationStructure->mType;
	buildDesc.DestAccelerationStructureData.SizeInBytes = pAccelerationStructure->pBuffer->mDesc.mSize;
	buildDesc.DestAccelerationStructureData.StartAddress = pAccelerationStructure->pBuffer->pDxResource->GetGPUVirtualAddress();
	buildDesc.Flags = pAccelerationStructure->mFlags;
	buildDesc.pGeometryDescs = NULL;

	if (pAccelerationStructure->mType == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL)
		buildDesc.pGeometryDescs = pAccelerationStructure->pGeometryDescs;
	else if (pAccelerationStructure->mType == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL)
		buildDesc.InstanceDescs = pAccelerationStructure->pInstanceDescBuffer->pDxResource->GetGPUVirtualAddress() +
								  pAccelerationStructure->pInstanceDescBuffer->mPositionInHeap;

	buildDesc.NumDescs = pAccelerationStructure->mDescCount;
	buildDesc.ScratchAccelerationStructureData.SizeInBytes = pScratchBuffer->mDesc.mSize;
	buildDesc.ScratchAccelerationStructureData.StartAddress = pScratchBuffer->pDxResource->GetGPUVirtualAddress();

	// Build acceleration structure.
	if (gRaytracingStaticInitializer.mFallback)
	{
		ID3D12RaytracingFallbackCommandList* pFallbackCmd = NULL;
		pRaytracing->pFallbackDevice->QueryRaytracingCommandList(pCmd->pDxCmdList, IID_PPV_ARGS(&pFallbackCmd));
		// Set the descriptor heaps to be used during acceleration structure build for the Fallback Layer.
		if (buildDesc.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL)
		{
			ID3D12DescriptorHeap* pHeaps[2] = { pRaytracing->pRenderer->pCbvSrvUavHeap[0]->pCurrentHeap,
												pRaytracing->pRenderer->pSamplerHeap[0]->pCurrentHeap };
			pFallbackCmd->SetDescriptorHeaps(ARRAYSIZE(pHeaps), pHeaps);
		}

		pFallbackCmd->BuildRaytracingAccelerationStructure(&buildDesc);
		pFallbackCmd->Release();
	}
	else    // DirectX Raytracing
	{
		ID3D12CommandListRaytracingPrototype* pDxrCmd = NULL;
		pCmd->pDxCmdList->QueryInterface(&pDxrCmd);
		pDxrCmd->BuildRaytracingAccelerationStructure(&buildDesc);
		pDxrCmd->Release();
	}

	// Make sure the Acceleration Structure is ready before using it in a raytracing operation
	cmdSynchronizeResources(pCmd, 1, &pAccelerationStructure->pBuffer, 0, NULL, false);
}

void cmdDispatchRays(Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc)
{
	const RaytracingShaderTable* pShaderTable = pDesc->pShaderTable;
	/************************************************************************/
	// Compute shader table GPU addresses
	// #TODO: Support for different offsets into the shader table
	/************************************************************************/
	D3D12_GPU_VIRTUAL_ADDRESS startAddress =
		pDesc->pShaderTable->pBuffer->pDxResource->GetGPUVirtualAddress() + pShaderTable->pBuffer->mPositionInHeap;

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
	if (gRaytracingStaticInitializer.mFallback)
	{
		ID3D12RaytracingFallbackCommandList* pFallbackCmd = NULL;
		pRaytracing->pFallbackDevice->QueryRaytracingCommandList(pCmd->pDxCmdList, IID_PPV_ARGS(&pFallbackCmd));

		D3D12_FALLBACK_DISPATCH_RAYS_DESC fallbackDispatchDesc = {};
		fallbackDispatchDesc.Height = pDesc->mHeight;
		fallbackDispatchDesc.Width = pDesc->mWidth;
		fallbackDispatchDesc.RayGenerationShaderRecord = rayGenShaderRecord;
		fallbackDispatchDesc.MissShaderTable = missShaderTable;
		fallbackDispatchDesc.HitGroupTable = hitGroupTable;

		ID3D12DescriptorHeap* pHeaps[2] = { pRaytracing->pRenderer->pCbvSrvUavHeap[0]->pCurrentHeap,
											pRaytracing->pRenderer->pSamplerHeap[0]->pCurrentHeap };
		// Set the descriptor heaps on the fallback command list since it will need them in the dispatch call
		pFallbackCmd->SetDescriptorHeaps(ARRAYSIZE(pHeaps), pHeaps);
		// We dont expose binding the top level AS to the app since that would require changing the cmdBindDescriptors function in the core framework
		pFallbackCmd->SetTopLevelAccelerationStructure(1, pDesc->pTopLevelAccelerationStructure->mFallbackPointer);
		pFallbackCmd->DispatchRays(pShaderTable->pPipeline->pFallbackPipeline, &fallbackDispatchDesc);

		pFallbackCmd->Release();
	}
	else
	{
		D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
		dispatchDesc.Height = pDesc->mHeight;
		dispatchDesc.Width = pDesc->mWidth;
		dispatchDesc.RayGenerationShaderRecord = rayGenShaderRecord;
		dispatchDesc.MissShaderTable = missShaderTable;
		dispatchDesc.HitGroupTable = hitGroupTable;

		// We dont expose binding the top level AS to the app since that would require changing the cmdBindDescriptors function in the core framework
		pCmd->pDxCmdList->SetComputeRootShaderResourceView(
			1, pDesc->pTopLevelAccelerationStructure->pBuffer->pDxResource->GetGPUVirtualAddress());

		ID3D12CommandListRaytracingPrototype* pDxrCmd = NULL;
		pCmd->pDxCmdList->QueryInterface(&pDxrCmd);
		pDxrCmd->DispatchRays(pShaderTable->pPipeline->pDxrPipeline, &dispatchDesc);
		pDxrCmd->Release();
	}
}

void cmdCopyTexture(Cmd* pCmd, Texture* pDst, Texture* pSrc)
{
	ASSERT(pCmd);
	ASSERT(pDst);
	ASSERT(pSrc);

	pCmd->pDxCmdList->CopyResource(pDst->pDxResource, pSrc->pDxResource);
}
/************************************************************************/
// Utility Functions Implementation
/************************************************************************/
D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE util_to_dx_acceleration_structure_type(AccelerationStructureType type)
{
	switch (type)
	{
		case ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		case ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL: return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		default: ASSERT(false && "Invalid Acceleration Structure Type"); return (D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE)-1;
	}
}

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
		ret |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE;
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_OPAQUE)
		ret |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE)
		ret |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE)
		ret |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;

	return ret;
}

// Create a wrapped pointer for the Fallback Layer path.
WRAPPED_GPU_POINTER util_create_fallback_wrapped_pointer(Raytracing* pRaytracing, ID3D12Resource* resource, UINT bufferNumElements)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC rawBufferUavDesc = {};
	rawBufferUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	rawBufferUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
	rawBufferUavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	rawBufferUavDesc.Buffer.NumElements = bufferNumElements;

	D3D12_CPU_DESCRIPTOR_HANDLE bottomLevelDescriptor;

	// Only compute fallback requires a valid descriptor index when creating a wrapped pointer.
	uint32_t descriptorHeapIndex = 0;
	if (!pRaytracing->pFallbackDevice->UsingRaytracingDriver())
	{
		bottomLevelDescriptor = add_cpu_descriptor_handles(pRaytracing->pRenderer->pCbvSrvUavHeap[0], 1, &descriptorHeapIndex);
		pRaytracing->pRenderer->pDxDevice->CreateUnorderedAccessView(resource, nullptr, &rawBufferUavDesc, bottomLevelDescriptor);
	}
	return pRaytracing->pFallbackDevice->GetWrappedPointerSimple((UINT)descriptorHeapIndex, resource->GetGPUVirtualAddress());
}
/************************************************************************/
/************************************************************************/
// DXR
#include "../ThirdParty/DXR/include/d3d12_1.h"
#include "../ThirdParty/DXR/include/D3D12RaytracingFallback.h"

#include <d3dcompiler.h>

// OS
#include "../../Common_3/OS/Interfaces/ILogManager.h"

// Renderer
#include "../Interfaces/IRaytracing.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/Direct3D12/Direct3D12Hooks.h"
#include "../../Common_3/Renderer/Direct3D12/Direct3D12MemoryAllocator.h"
#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)

typedef struct DescriptorStoreHeap
{
	uint32_t mNumDescriptors;
	/// DescriptorInfo Increment Size
	uint32_t mDescriptorSize;
	/// Bitset for finding SAFE_FREE descriptor slots
	uint32_t* flags;
	/// Lock for multi-threaded descriptor allocations
	Mutex* pAllocationMutex;

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

extern void add_descriptor_heap(Renderer* pRenderer, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, uint32_t numDescriptors, struct DescriptorStoreHeap** ppDescHeap);
extern void reset_descriptor_heap(struct DescriptorStoreHeap* pHeap);
extern void remove_descriptor_heap(struct DescriptorStoreHeap* pHeap);
extern D3D12_CPU_DESCRIPTOR_HANDLE add_cpu_descriptor_handles(struct DescriptorStoreHeap* pHeap, uint32_t numDescriptors, uint32_t* pDescriptorIndex = NULL);
extern void add_gpu_descriptor_handles(struct DescriptorStoreHeap* pHeap, D3D12_CPU_DESCRIPTOR_HANDLE* pStartCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* pStartGpuHandle, uint32_t numDescriptors);
/************************************************************************/
// Static initialization of DX Raytracing API
/************************************************************************/
// Enable experimental features and return if they are supported.
// To test them being supported we need to check both their enablement as well as device creation afterwards.
inline bool EnableD3D12ExperimentalFeatures(UUID* experimentalFeatures, uint32_t featureCount)
{
	ID3D12Device* testDevice = NULL;
	bool ret = SUCCEEDED(D3D12EnableExperimentalFeatures(featureCount, experimentalFeatures, nullptr, nullptr))
		&& SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)));
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
				"Could not enable raytracing driver (D3D12EnableExperimentalFeatures() failed).\n" \
				"Possible reasons:\n" \
				"  1) your OS is not in developer mode.\n" \
				"  2) your GPU driver doesn't match the D3D12 runtime loaded by the app (d3d12.dll and friends).\n" \
				"  3) your D3D12 runtime doesn't match the D3D12 headers used by your app (in particular, the GUID passed to D3D12EnableExperimentalFeatures).\n\n");

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
D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE util_to_dx_acceleration_structure_type(AccelerationStructureType type);
D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS util_to_dx_acceleration_structure_build_flags(AccelerationStructureBuildFlags flags);
D3D12_RAYTRACING_GEOMETRY_FLAGS util_to_dx_geometry_flags(AccelerationStructureGeometryFlags flags);
D3D12_RAYTRACING_INSTANCE_FLAGS util_to_dx_instance_flags(AccelerationStructureInstanceFlags flags);
// Create a wrapped pointer for the Fallback Layer path.
WRAPPED_GPU_POINTER util_create_fallback_wrapped_pointer(Raytracing* pRaytracing, ID3D12Resource* resource, UINT bufferNumElements);
WRAPPED_GPU_POINTER util_create_fallback_wrapped_pointer(Raytracing* pRaytracing, ID3D12Resource* resource, UINT bufferNumElements);
/************************************************************************/
// Forge Raytracing Implementation using DXR
/************************************************************************/
struct Raytracing
{
	Renderer*							pRenderer;
	ID3D12DeviceRaytracingPrototype*	pDxrDevice;
	ID3D12RaytracingFallbackDevice*		pFallbackDevice;
	//DescriptorStoreHeap*				pDescriptorHeap;
	uint64_t mDescriptorsAllocated;
};

struct RaytracingShader
{
	WCHAR**		ppExports;
	ID3DBlob*	pShaderBlob;
	uint32_t	mExportCount;
};

struct AccelerationStructure
{
	Buffer*												pBuffer;
	D3D12_RAYTRACING_GEOMETRY_DESC*						pGeometryDescs;
	Buffer*												pInstanceDescBuffer;
	WRAPPED_GPU_POINTER									mFallbackPointer;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE		mType;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS mFlags;
	uint32_t											mDescCount;
};

struct RaytracingPipeline
{
	union
	{
		ID3D12StateObjectPrototype*				pDxrPipeline;
		ID3D12RaytracingFallbackStateObject*	pFallbackPipeline;
	};
};

struct RaytracingShaderTable
{
	RaytracingPipeline* pPipeline;
	Buffer*				pBuffer;
	uint64_t			mMaxEntrySize;
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
		D3D12CreateRaytracingFallbackDevice(pRenderer->pDevice, CreateRaytracingFallbackDeviceFlags::ForceComputeFallback, 0, IID_PPV_ARGS(&pRaytracing->pFallbackDevice));
	}
	else
	{
		pRenderer->pDevice->QueryInterface(&pRaytracing->pDxrDevice);
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

void addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDesc* pDesc, unsigned* pScratchBufferSize, AccelerationStructure** ppAccelerationStructure)
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
		pAccelerationStructure->pGeometryDescs = (D3D12_RAYTRACING_GEOMETRY_DESC*)conf_calloc(pDesc->mDescCount, sizeof(D3D12_RAYTRACING_GEOMETRY_DESC));
		for (uint32_t i = 0; i < pDesc->mDescCount; ++i)
		{
			AccelerationStructureGeometryDesc* pGeom = &pDesc->pGeometryDescs[i];
			D3D12_RAYTRACING_GEOMETRY_DESC* pOut = &pAccelerationStructure->pGeometryDescs[i];
			if (pGeom->mType == ACCELERATION_STRUCTURE_GEOMETRY_TYPE_TRIANGLES)
			{
				pOut->Flags = util_to_dx_geometry_flags(pGeom->mFlags);

				if (pGeom->pIndexBuffer)
				{
					pOut->Triangles.IndexBuffer = pGeom->pIndexBuffer->mDxIndexBufferView.BufferLocation;
					pOut->Triangles.IndexCount = (UINT)pGeom->pIndexBuffer->mDesc.mSize /
						(pGeom->pIndexBuffer->mDesc.mIndexType == INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t));
					pOut->Triangles.IndexFormat = pGeom->pIndexBuffer->mDxIndexBufferView.Format;
;				}

				pOut->Triangles.VertexBuffer.StartAddress = pGeom->pVertexBuffer->mDxVertexBufferView.BufferLocation;
				pOut->Triangles.VertexBuffer.StrideInBytes = pGeom->pVertexBuffer->mDxVertexBufferView.StrideInBytes;
				pOut->Triangles.VertexCount = pGeom->pVertexBuffer->mDxVertexBufferView.SizeInBytes / pGeom->pVertexBuffer->mDxVertexBufferView.StrideInBytes;
				if (pGeom->pVertexBuffer->mDxVertexBufferView.StrideInBytes == sizeof(float))
					pOut->Triangles.VertexFormat = DXGI_FORMAT_R32_FLOAT;
				else if (pGeom->pVertexBuffer->mDxVertexBufferView.StrideInBytes == sizeof(float) * 2)
					pOut->Triangles.VertexFormat = DXGI_FORMAT_R32G32_FLOAT;
				else if (pGeom->pVertexBuffer->mDxVertexBufferView.StrideInBytes == sizeof(float) * 3)
					pOut->Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				else if (pGeom->pVertexBuffer->mDxVertexBufferView.StrideInBytes == sizeof(float) * 4)
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
				fallbackInstanceDescs[i].AccelerationStructure =
					util_create_fallback_wrapped_pointer(pRaytracing, pInst->pAccelerationStructure->pBuffer->pDxResource,
					(UINT)pInst->pAccelerationStructure->pBuffer->mDesc.mSize / sizeof(UINT));
				fallbackInstanceDescs[i].Flags = util_to_dx_instance_flags(pInst->mFlags);
				fallbackInstanceDescs[i].InstanceContributionToHitGroupIndex = pInst->mInstanceContributionToHitGroupIndex;
				fallbackInstanceDescs[i].InstanceID = pInst->mInstanceID;
				fallbackInstanceDescs[i].InstanceMask = pInst->mInstanceMask;
				memcpy(fallbackInstanceDescs[i].Transform, pInst->mTransform, sizeof(float[12]));
			}

			BufferDesc instanceDesc = {};
			instanceDesc.mUsage = BUFFER_USAGE_UPLOAD;
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
			instanceDesc.mUsage = BUFFER_USAGE_UPLOAD;
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
	bufferDesc.mUsage = BUFFER_USAGE_STORAGE_UAV;
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
		pAccelerationStructure->mFallbackPointer =
			util_create_fallback_wrapped_pointer(pRaytracing, pAccelerationStructure->pBuffer->pDxResource, (UINT)info.ResultDataMaxSizeInBytes / sizeof(UINT));
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

void addRaytracingShader(Raytracing* pRaytracing, const unsigned char* pByteCode, unsigned byteCodeSize, const char** ppNames, unsigned nameCount, RaytracingShader** ppShader)
{
	ASSERT(pRaytracing);
	ASSERT(pByteCode);
	ASSERT(byteCodeSize);
	ASSERT(ppNames);
	ASSERT(nameCount);
	ASSERT(ppShader);

	RaytracingShader* pShader = (RaytracingShader*)conf_calloc(1, sizeof(*pShader));
	ASSERT(pShader);

	D3DCreateBlob(byteCodeSize, &pShader->pShaderBlob);
	memcpy(pShader->pShaderBlob->GetBufferPointer(), pByteCode, byteCodeSize);

	pShader->mExportCount = nameCount;
	pShader->ppExports = (WCHAR**)conf_calloc(pShader->mExportCount, sizeof(*pShader->ppExports));
	for (uint32_t i = 0; i < pShader->mExportCount; ++i)
	{
		pShader->ppExports[i] = (WCHAR*)conf_calloc(strlen(ppNames[i]) + 1, sizeof(WCHAR));
		mbstowcs(pShader->ppExports[i], ppNames[i], strlen(ppNames[i]));
	}

	*ppShader = pShader;
}

void removeRaytracingShader(Raytracing* pRaytracing, RaytracingShader* pShader)
{
	ASSERT(pRaytracing);
	ASSERT(pShader);

	pShader->pShaderBlob->Release();

	for (uint32_t i = 0; i < pShader->mExportCount; ++i)
		conf_free(pShader->ppExports[i]);

	conf_free(pShader->ppExports);
	conf_free(pShader);
}

typedef struct UpdateFrequencyLayoutInfo
{
	tinystl::vector <DescriptorInfo*> mCbvSrvUavTable;
	tinystl::vector <DescriptorInfo*> mSamplerTable;
	tinystl::vector <DescriptorInfo*> mConstantParams;
	tinystl::vector <DescriptorInfo*> mRootConstants;
	tinystl::unordered_map<DescriptorInfo*, uint32_t> mDescriptorIndexMap;
} UpdateFrequencyLayoutInfo;

static const RootSignatureDesc gDefaultRootSignatureDesc = {};
extern void create_descriptor_table_1_0(uint32_t numDescriptors, DescriptorInfo** tableRef, D3D12_DESCRIPTOR_RANGE* pRange, D3D12_ROOT_PARAMETER* pRootParam);
extern void create_root_descriptor_1_0(const DescriptorInfo* pDesc, D3D12_ROOT_PARAMETER* pRootParam);
extern void create_root_constant_1_0(const DescriptorInfo* pDesc, D3D12_ROOT_PARAMETER* pRootParam);

void addRaytracingRootSignature(Raytracing* pRaytracing, const ShaderResource* pResources, uint32_t resourceCount, bool local, RootSignature** ppRootSignature, const RootSignatureDesc* pRootDesc)
{
	ASSERT(pRaytracing);
	ASSERT(ppRootSignature);

	RootSignature* pRootSignature = (RootSignature*)conf_calloc(1, sizeof(*pRootSignature));
	ASSERT(pRootSignature);

	pRootSignature->mDescriptorCount = resourceCount;
	// Raytracing is executed in compute path
	pRootSignature->mPipelineType = PIPELINE_TYPE_COMPUTE;
	
	if (resourceCount)
		pRootSignature->pDescriptors = (DescriptorInfo*)conf_calloc(pRootSignature->mDescriptorCount, sizeof(*pRootSignature->pDescriptors));

	//pRootSignature->pDescriptorNameToIndexMap;
	conf_placement_new<tinystl::unordered_map<uint32_t, uint32_t> >(
		&pRootSignature->pDescriptorNameToIndexMap);

	const RootSignatureDesc* pRootSignatureDesc = pRootDesc ? pRootDesc : &gDefaultRootSignatureDesc;
	tinystl::vector<UpdateFrequencyLayoutInfo> layouts(DESCRIPTOR_UPDATE_FREQ_COUNT);
	tinystl::vector <tinystl::pair<DescriptorInfo*, Sampler*> > staticSamplers;
	/************************************************************************/
	// Fill Descriptor Info
	/************************************************************************/
	// Fill the descriptor array to be stored in the root signature
	for (uint32_t i = 0; i < resourceCount; ++i)
	{
		DescriptorInfo* pDesc = &pRootSignature->pDescriptors[i];
		const ShaderResource* pRes = &pResources[i];
		uint32_t setIndex = pRes->set;
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

		if (pDesc->mDesc.size == 0)
		{
			pDesc->mDesc.size = pRootSignatureDesc->mMaxBindlessDescriptors[pDesc->mDesc.type];
		}

		// Find the D3D12 type of the descriptors
		if (pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER)
		{
			// If the sampler is a static sampler, no need to put it in the descriptor table
			const tinystl::unordered_hash_node<tinystl::string, Sampler*>* pNode = pRootSignatureDesc->mStaticSamplers.find(pDesc->mDesc.name).node;

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
		else if (pDesc->mDesc.type == DESCRIPTOR_TYPE_UNIFORM_BUFFER && pDesc->mDesc.size == 1)
		{
			// D3D12 has no special syntax to declare root constants like Vulkan
			// So we assume that all constant buffers with the word "rootconstant" (case insensitive) are root constants
			if (tinystl::string(pRes->name).to_lower().find("rootconstant", 0) != String::npos || pDesc->mDesc.type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
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
			layouts[setIndex].mCbvSrvUavTable.emplace_back (pDesc);
		}


		pRootSignature->pDescriptorNameToIndexMap.insert({ tinystl::hash(pDesc->mDesc.name), i });
		layouts[setIndex].mDescriptorIndexMap[pDesc] = i;
	}

	tinystl::vector <tinystl::vector <D3D12_DESCRIPTOR_RANGE> > cbvSrvUavRange_1_0((uint32_t)layouts.size());
	tinystl::vector <tinystl::vector <D3D12_DESCRIPTOR_RANGE> > samplerRange_1_0((uint32_t)layouts.size());
	tinystl::vector <D3D12_ROOT_PARAMETER> rootParams_1_0;

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

	pRootSignature->pViewTableLayouts = (DescriptorSetLayout*)conf_calloc((uint32_t)layouts.size(), sizeof(*pRootSignature->pViewTableLayouts));
	pRootSignature->pSamplerTableLayouts = (DescriptorSetLayout*)conf_calloc((uint32_t)layouts.size(), sizeof(*pRootSignature->pSamplerTableLayouts));
	pRootSignature->pRootDescriptorLayouts = (RootDescriptorLayout*)conf_calloc((uint32_t)layouts.size(), sizeof(*pRootSignature->pRootDescriptorLayouts));
	pRootSignature->mDescriptorCount = resourceCount;

	pRootSignature->mRootConstantCount = (uint32_t)layouts[0].mRootConstants.size();
	if (pRootSignature->mRootConstantCount)
		pRootSignature->pRootConstantLayouts = (RootConstantLayout*)conf_calloc(pRootSignature->mRootConstantCount, sizeof(*pRootSignature->pRootConstantLayouts));
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

	// Collect all root descriptors
	// Put most frequently changed params first
	for (uint32_t i = (uint32_t)layouts.size(); i-- > 0U;)
	{
		UpdateFrequencyLayoutInfo& layout = layouts[i];
		if (layout.mConstantParams.size())
		{
			RootDescriptorLayout& root = pRootSignature->pRootDescriptorLayouts[i];

			root.mRootDescriptorCount = (uint32_t)layout.mConstantParams.size();
			root.pDescriptorIndices = (uint32_t*)conf_calloc(root.mRootDescriptorCount, sizeof(uint32_t));
			root.pRootIndices = (uint32_t*)conf_calloc(root.mRootDescriptorCount, sizeof(uint32_t));

			for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mConstantParams.size(); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mConstantParams[descIndex];
				D3D12_ROOT_PARAMETER rootParam_1_0;
				create_root_descriptor_1_0(pDesc, &rootParam_1_0);

				root.pDescriptorIndices[descIndex] = layout.mDescriptorIndexMap[pDesc];
				root.pRootIndices[descIndex] = (uint32_t)rootParams_1_0.size();

				rootParams_1_0.push_back(rootParam_1_0);

				pDesc->mIndexInParent = descIndex;
			}
		}
	}

	// Collect all root constants
	for (uint32_t i = 0; i < pRootSignature->mRootConstantCount; ++i)
	{
		RootConstantLayout* pLayout = &pRootSignature->pRootConstantLayouts[i];
		DescriptorInfo* pDesc = layouts[0].mRootConstants[i];
		pDesc->mIndexInParent = i;
		pLayout->mRootIndex = (uint32_t)rootParams_1_0.size();
		pLayout->mDescriptorIndex = layouts[0].mDescriptorIndexMap[pDesc];

		D3D12_ROOT_PARAMETER rootParam_1_0;
		create_root_constant_1_0(pDesc, &rootParam_1_0);

		rootParams_1_0.push_back(rootParam_1_0);
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
			layout.mCbvSrvUavTable.sort([](DescriptorInfo* const& lhs, DescriptorInfo* const& rhs)
			{
				return (int)(lhs->mDesc.reg - rhs->mDesc.reg);
			});
			layout.mCbvSrvUavTable.sort([](DescriptorInfo* const& lhs, DescriptorInfo* const& rhs)
			{
				return (int)(lhs->mDesc.set - rhs->mDesc.set);
			});
			layout.mCbvSrvUavTable.sort([](DescriptorInfo* const& lhs, DescriptorInfo* const& rhs)
			{
				return (int)(lhs->mDesc.type - rhs->mDesc.type);
			});

			D3D12_ROOT_PARAMETER rootParam_1_0;
			create_descriptor_table_1_0((uint32_t)layout.mCbvSrvUavTable.size(), layout.mCbvSrvUavTable.data(), cbvSrvUavRange_1_0[i].data(), &rootParam_1_0);

			DescriptorSetLayout& table = pRootSignature->pViewTableLayouts[i];

			// Store some of the binding info which will be required later when binding the descriptor table
			// We need the root index when calling SetRootDescriptorTable
			table.mRootIndex = (uint32_t)rootParams_1_0.size();
			table.mDescriptorCount = (uint32_t)layout.mCbvSrvUavTable.size();
			table.pDescriptorIndices = (uint32_t*)conf_calloc(table.mDescriptorCount, sizeof(uint32_t));

			for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mCbvSrvUavTable.size(); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mCbvSrvUavTable[descIndex];
				pDesc->mIndexInParent = descIndex;

				// Store the d3d12 related info in the descriptor to avoid constantly calling the util_to_dx mapping functions
				pDesc->mDxType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				pDesc->mHandleIndex = table.mCumulativeDescriptorCount;

				// Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
				// This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays) in the descriptor table
				table.mCumulativeDescriptorCount += pDesc->mDesc.size;
				table.pDescriptorIndices[descIndex] = layout.mDescriptorIndexMap[pDesc];
			}

			rootParams_1_0.push_back(rootParam_1_0);
		}

		// Fill the descriptor table layout for the sampler descriptor table of this update frequency
		if (layout.mSamplerTable.size())
		{
			D3D12_ROOT_PARAMETER rootParam_1_0;
			create_descriptor_table_1_0((uint32_t)layout.mSamplerTable.size(), layout.mSamplerTable.data(), samplerRange_1_0[i].data(), &rootParam_1_0);

			DescriptorSetLayout& table = pRootSignature->pSamplerTableLayouts[i];

			// Store some of the binding info which will be required later when binding the descriptor table
			// We need the root index when calling SetRootDescriptorTable
			table.mRootIndex = (uint32_t)rootParams_1_0.size();
			table.mDescriptorCount = (uint32_t)layout.mSamplerTable.size();
			table.pDescriptorIndices = (uint32_t*)conf_calloc(table.mDescriptorCount, sizeof(uint32_t));

			for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.mSamplerTable.size(); ++descIndex)
			{
				DescriptorInfo* pDesc = layout.mSamplerTable[descIndex];
				pDesc->mIndexInParent = descIndex;

				// Store the d3d12 related info in the descriptor to avoid constantly calling the util_to_dx mapping functions
				pDesc->mDxType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				pDesc->mHandleIndex = table.mCumulativeDescriptorCount;

				// Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
				// This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays) in the descriptor table
				table.mCumulativeDescriptorCount += pDesc->mDesc.size;
				table.pDescriptorIndices[descIndex] = layout.mDescriptorIndexMap[pDesc];
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
		HRESULT hr = pRaytracing->pFallbackDevice->D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pRootSignature->pDxSerializedRootSignatureString, &error_msgs);

		if (!SUCCEEDED(hr))
		{
			char* pMsg = (char*)conf_calloc(error_msgs->GetBufferSize(), sizeof(char));
			memcpy(pMsg, error_msgs->GetBufferPointer(), error_msgs->GetBufferSize());
			LOGERRORF("Failed to serialize root signature with error (%s)", pMsg);
			conf_free(pMsg);
		}

		ASSERT(pRootSignature->pDxSerializedRootSignatureString);

		hr = pRaytracing->pFallbackDevice->CreateRootSignature(0, pRootSignature->pDxSerializedRootSignatureString->GetBufferPointer(),
			pRootSignature->pDxSerializedRootSignatureString->GetBufferSize(),
			IID_PPV_ARGS(&(pRootSignature->pDxRootSignature)));
		ASSERT(SUCCEEDED(hr));
	}
	else
	{
		HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pRootSignature->pDxSerializedRootSignatureString, &error_msgs);

		if (!SUCCEEDED(hr))
		{
			char* pMsg = (char*)conf_calloc(error_msgs->GetBufferSize(), sizeof(char));
			memcpy(pMsg, error_msgs->GetBufferPointer(), error_msgs->GetBufferSize());
			LOGERRORF("Failed to serialize root signature with error (%s)", pMsg);
			conf_free(pMsg);
		}

		ASSERT(pRootSignature->pDxSerializedRootSignatureString);

		hr = pRaytracing->pRenderer->pDevice->CreateRootSignature(0,
			pRootSignature->pDxSerializedRootSignatureString->GetBufferPointer(),
			pRootSignature->pDxSerializedRootSignatureString->GetBufferSize(),
			IID_PPV_ARGS(&pRootSignature->pDxRootSignature));
		ASSERT(SUCCEEDED(hr));
	}
	/************************************************************************/
	/************************************************************************/
	*ppRootSignature = pRootSignature;
}

struct DxilLibrary
{
	DxilLibrary(ID3DBlob* pBlob, WCHAR** entryPoint, uint32_t entryPointCount) : pShaderBlob(pBlob)
	{
		stateSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		stateSubobject.pDesc = &dxilLibDesc;

		dxilLibDesc = {};
		exportDesc.resize(entryPointCount);
		exportName.resize(entryPointCount);
		if (pBlob)
		{
			dxilLibDesc.DXILLibrary.pShaderBytecode = pBlob->GetBufferPointer();
			dxilLibDesc.DXILLibrary.BytecodeLength = pBlob->GetBufferSize();
			dxilLibDesc.NumExports = entryPointCount;
			dxilLibDesc.pExports = exportDesc.data();

			for (uint32_t i = 0; i < entryPointCount; i++)
			{
				exportName[i] = entryPoint[i];
				exportDesc[i].Name = exportName[i];
				exportDesc[i].Flags = D3D12_EXPORT_FLAG_NONE;
				exportDesc[i].ExportToRename = nullptr;
			}
		}
	}

	DxilLibrary() : DxilLibrary(nullptr, nullptr, 0) {}

	D3D12_DXIL_LIBRARY_DESC dxilLibDesc = {};
	D3D12_STATE_SUBOBJECT stateSubobject{};
	ID3DBlob* pShaderBlob;
	tinystl::vector<D3D12_EXPORT_DESC> exportDesc;
	tinystl::vector<const WCHAR*> exportName;
};

struct HitProgram
{
	HitProgram(const char* intersectionExport, const char* ahsExport, const char* chsExport, const char* name)
	{
		desc = {};
		if (intersectionExport)
		{
			desc.IntersectionShaderImport = (LPCWSTR)conf_calloc(strlen(intersectionExport) + 1, sizeof(WCHAR));
			mbstowcs((WCHAR*)desc.IntersectionShaderImport, intersectionExport, strlen(intersectionExport));
		}
		if (ahsExport)
		{
			desc.AnyHitShaderImport = (LPCWSTR)conf_calloc(strlen(ahsExport) + 1, sizeof(WCHAR));
			mbstowcs((WCHAR*)desc.AnyHitShaderImport, ahsExport, strlen(ahsExport));
		}
		if (chsExport)
		{
			desc.ClosestHitShaderImport = (LPCWSTR)conf_calloc(strlen(chsExport) + 1, sizeof(WCHAR));
			mbstowcs((WCHAR*)desc.ClosestHitShaderImport, chsExport, strlen(chsExport));
		}
		{
			desc.HitGroupExport = (LPCWSTR)conf_calloc(strlen(name) + 1, sizeof(WCHAR));
			mbstowcs((WCHAR*)desc.HitGroupExport, name, strlen(name));
		}

		subObject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		subObject.pDesc = &desc;
	}
	~HitProgram()
	{
		if (desc.AnyHitShaderImport)
			conf_free((WCHAR*)desc.AnyHitShaderImport);
		if (desc.ClosestHitShaderImport)
			conf_free((WCHAR*)desc.ClosestHitShaderImport);
		if (desc.HitGroupExport)
			conf_free((WCHAR*)desc.HitGroupExport);
		if (desc.IntersectionShaderImport)
			conf_free((WCHAR*)desc.IntersectionShaderImport);
	}

	D3D12_HIT_GROUP_DESC desc;
	D3D12_STATE_SUBOBJECT subObject;
};

struct ExportAssociation
{
	ExportAssociation(const WCHAR** exportNames, uint32_t exportCount, const D3D12_STATE_SUBOBJECT* pSubobjectToAssociate)
	{
		exports.resize(exportCount);
		for (uint32_t i = 0; i < exportCount; ++i)
		{
			exports[i] = exportNames[i];
		}

		association.NumExports = exportCount;
		association.pExports = exports.data();
		association.pSubobjectToAssociate = pSubobjectToAssociate;

		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		subobject.pDesc = &association;
	}

	D3D12_STATE_SUBOBJECT subobject = {};
	tinystl::vector<const WCHAR*> exports;
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION association = {};
};

struct LocalRootSignature
{
	LocalRootSignature(ID3D12RootSignature* pRootSignature)
	{
		pInterface = pRootSignature;
		subobject.pDesc = &pInterface;
		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	}
	ID3D12RootSignature* pInterface = nullptr;
	D3D12_STATE_SUBOBJECT subobject = {};
};

struct GlobalRootSignature
{
	GlobalRootSignature(ID3D12RootSignature* pRootSignature)
	{
		pInterface = pRootSignature;
		subobject.pDesc = &pInterface;
		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
	}
	ID3D12RootSignature* pInterface = nullptr;
	D3D12_STATE_SUBOBJECT subobject = {};
};

struct ShaderConfig
{
	ShaderConfig(uint32_t maxAttributeSizeInBytes, uint32_t maxPayloadSizeInBytes)
	{
		shaderConfig.MaxAttributeSizeInBytes = maxAttributeSizeInBytes;
		shaderConfig.MaxPayloadSizeInBytes = maxPayloadSizeInBytes;

		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
		subobject.pDesc = &shaderConfig;
	}

	D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
	D3D12_STATE_SUBOBJECT subobject = {};
};

struct PipelineConfig
{
	PipelineConfig(uint32_t maxTraceRecursionDepth)
	{
		config.MaxTraceRecursionDepth = maxTraceRecursionDepth;

		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
		subobject.pDesc = &config;
	}

	D3D12_RAYTRACING_PIPELINE_CONFIG config = {};
	D3D12_STATE_SUBOBJECT subobject = {};
};

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
	D3D12_STATE_SUBOBJECT subobjects[10] = {};
	uint32_t index = 0;
	/************************************************************************/
	// Step 1 - Create DXIL Libraries
	/************************************************************************/
	tinystl::vector<DxilLibrary*> dxilLibraries(pDesc->mShaderCount);
	for (uint32_t i = 0; i < pDesc->mShaderCount; ++i)
	{
		dxilLibraries[i] = conf_placement_new<DxilLibrary>
			(conf_calloc(1, sizeof(DxilLibrary)),
				pDesc->ppShaders[i]->pShaderBlob, pDesc->ppShaders[i]->ppExports, pDesc->ppShaders[i]->mExportCount);
		subobjects[index++] = dxilLibraries[i]->stateSubobject;
	}
	/************************************************************************/
	// Step 2 - Create Hit Groups
	/************************************************************************/
	tinystl::vector<HitProgram*> hitPrograms(pDesc->mHitGroupCount);
	for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
	{
		ASSERT(pDesc->pHitGroups[i].pHitGroupName);
		hitPrograms[i] = conf_placement_new<HitProgram>
			(conf_calloc(1, sizeof(HitProgram)),
				pDesc->pHitGroups[i].pIntersectionShaderName,
			pDesc->pHitGroups[i].pAnyHitShaderName, pDesc->pHitGroups[i].pClosestHitShaderName,
			pDesc->pHitGroups[i].pHitGroupName);

		subobjects[index++] = hitPrograms[i]->subObject;
	}
	/************************************************************************/
	// Step 3 - Shader Config
	/************************************************************************/
	// Collect Shader Exports
	tinystl::vector<const WCHAR*> shaderConfigExports;
	for (uint32_t i = 0; i < (uint32_t)dxilLibraries.size(); ++i)
	{
		for (uint32_t j = 0; j < (uint32_t)dxilLibraries[i]->exportName.size(); ++j)
			shaderConfigExports.emplace_back(dxilLibraries[i]->exportName[i]);
	}
	for (uint32_t i = 0; i < (uint32_t)hitPrograms.size(); ++i)
	{
		shaderConfigExports.emplace_back(hitPrograms[i]->desc.HitGroupExport);
	}
	// Bind the payload size to the programs
	ShaderConfig shaderConfig(pDesc->mAttributeSize, pDesc->mPayloadSize);
	subobjects[index] = shaderConfig.subobject;
	uint32_t shaderConfigIndex = index++;
	ExportAssociation configAssociation(shaderConfigExports.data(), (uint32_t)shaderConfigExports.size(), &(subobjects[shaderConfigIndex]));
	subobjects[index++] = configAssociation.subobject;
	/************************************************************************/
	// Step 4 = Pipeline Config
	/************************************************************************/
	PipelineConfig config = PipelineConfig(pDesc->mMaxTraceRecursionDepth);
	subobjects[index++] = config.subobject;
	/************************************************************************/
	// Step 5 - Global Root Signature
	/************************************************************************/
	GlobalRootSignature globalRootSignature = GlobalRootSignature(pDesc->pGlobalRootSignature->pDxRootSignature);
	subobjects[index++] = globalRootSignature.subobject;
	/************************************************************************/
	// #TODO Step 6 - Local Root Signatures
	/************************************************************************/
	/************************************************************************/
	// Step 7 - Create State Object
	/************************************************************************/
	D3D12_STATE_OBJECT_DESC pipelineDesc = {};
	pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	pipelineDesc.NumSubobjects = index;
	pipelineDesc.pSubobjects = subobjects;

	// Create the state object.
	if (gRaytracingStaticInitializer.mFallback)
	{
		HRESULT hr = pRaytracing->pFallbackDevice->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&pPipeline->pFallbackPipeline));
		ASSERT(SUCCEEDED(hr));
	}
	else // DirectX Raytracing
	{
		HRESULT hr = pRaytracing->pDxrDevice->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&pPipeline->pDxrPipeline));
		ASSERT(SUCCEEDED(hr));
	}
	/************************************************************************/
	// Clean up
	/************************************************************************/
	for (uint32_t i = 0; i < (uint32_t)dxilLibraries.size(); ++i)
	{
		dxilLibraries[i]->~DxilLibrary();
		conf_free(dxilLibraries[i]);
	}

	for (uint32_t i = 0; i < (uint32_t)hitPrograms.size(); ++i)
	{
		hitPrograms[i]->~HitProgram();
		conf_free(hitPrograms[i]);
	}
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

	const uint32_t shaderCount = pDesc->mRayGenShaderCount + pDesc->mMissShaderCount + pDesc->mHitGroupCount;
	uint64_t maxShaderTableSize = shaderIdentifierSize;
	/************************************************************************/
	// #TODO Calculate max size for each element in the shader table
	/************************************************************************/

	/************************************************************************/
	// Align max size
	/************************************************************************/
	maxShaderTableSize = round_up_64(maxShaderTableSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
	pTable->mMaxEntrySize = maxShaderTableSize;
	/************************************************************************/
	// Create shader table buffer
	/************************************************************************/
	BufferDesc bufferDesc = {};
	bufferDesc.mUsage = BUFFER_USAGE_UPLOAD;
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	bufferDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	bufferDesc.mSize = maxShaderTableSize * shaderCount;
	addBuffer(pRaytracing->pRenderer, &bufferDesc, &pTable->pBuffer);
	/************************************************************************/
	// Copy shader identifiers into the buffer
	/************************************************************************/
	ID3D12StateObjectPropertiesPrototype* pRtsoProps = NULL;
	if (!gRaytracingStaticInitializer.mFallback)
		pDesc->pPipeline->pDxrPipeline->QueryInterface(IID_PPV_ARGS(&pRtsoProps));

	uint32_t index = 0;

	auto FillShaderIdentifiers = [&](const char** ppShaders, uint32_t shaderCount)
	{
		for (uint32_t i = 0; i < shaderCount; ++i)
		{
			void* pIdentifier = NULL;
			WCHAR* pName = (WCHAR*)conf_calloc(strlen(ppShaders[i]) + 1, sizeof(WCHAR));
			mbstowcs(pName, ppShaders[i], strlen(ppShaders[i]));

			if (gRaytracingStaticInitializer.mFallback)
				pIdentifier = pDesc->pPipeline->pFallbackPipeline->GetShaderIdentifier(pName);
			else
				pIdentifier = pRtsoProps->GetShaderIdentifier(pName);

			ASSERT(pIdentifier);
			conf_free(pName);

			memcpy((uint8_t*)pTable->pBuffer->pCpuMappedAddress + maxShaderTableSize * index++, pIdentifier, shaderIdentifierSize);
		}
	};

	FillShaderIdentifiers(pDesc->ppRayGenShaders, pDesc->mRayGenShaderCount);
	FillShaderIdentifiers(pDesc->ppMissShaders, pDesc->mMissShaderCount);
	FillShaderIdentifiers(pDesc->ppHitGroups, pDesc->mHitGroupCount);

	if (pRtsoProps)
		pRtsoProps->Release();
	/************************************************************************/
	// #TODO Copy descriptor data into the buffer
	/************************************************************************/
	/************************************************************************/
	/************************************************************************/
	*ppTable = pTable;
}

void removeRaytracingShaderTable(Raytracing* pRaytracing, RaytracingShaderTable* pTable)
{
	ASSERT(pRaytracing);
	ASSERT(pTable);

	removeBuffer(pRaytracing->pRenderer, pTable->pBuffer);

	conf_free(pTable);
}
/************************************************************************/
// Raytracing Command Buffer Functions Implementation
/************************************************************************/
void cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, Buffer* pScratchBuffer, AccelerationStructure* pAccelerationStructure)
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
		buildDesc.InstanceDescs = pAccelerationStructure->pInstanceDescBuffer->pDxResource->GetGPUVirtualAddress() + pAccelerationStructure->pInstanceDescBuffer->mPositionInHeap;

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
			ID3D12DescriptorHeap* pHeaps[2] = {
				pRaytracing->pRenderer->pCbvSrvUavHeap->pCurrentHeap,
				pRaytracing->pRenderer->pSamplerHeap->pCurrentHeap
			};
			pFallbackCmd->SetDescriptorHeaps(ARRAYSIZE(pHeaps), pHeaps);
		}

		pFallbackCmd->BuildRaytracingAccelerationStructure(&buildDesc);
		pFallbackCmd->Release();
	}
	else // DirectX Raytracing
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
	D3D12_GPU_VIRTUAL_ADDRESS startAddress = pDesc->pShaderTable->pBuffer->pDxResource->GetGPUVirtualAddress() + pShaderTable->pBuffer->mPositionInHeap;
	D3D12_GPU_VIRTUAL_ADDRESS_RANGE rayGenShaderRecord = {};
	rayGenShaderRecord.SizeInBytes = pShaderTable->mMaxEntrySize;
	rayGenShaderRecord.StartAddress = startAddress + pShaderTable->mMaxEntrySize * 0;
	D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE missShaderTable = {};
	missShaderTable.SizeInBytes = pShaderTable->mMaxEntrySize;
	missShaderTable.StartAddress = startAddress + pShaderTable->mMaxEntrySize * 1;
	missShaderTable.StrideInBytes = pShaderTable->mMaxEntrySize;
	D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE hitGroupTable = {};
	hitGroupTable.SizeInBytes = pShaderTable->mMaxEntrySize;
	hitGroupTable.StartAddress = startAddress + pShaderTable->mMaxEntrySize * 2;
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

		ID3D12DescriptorHeap* pHeaps[2] = {
			pRaytracing->pRenderer->pCbvSrvUavHeap->pCurrentHeap,
			pRaytracing->pRenderer->pSamplerHeap->pCurrentHeap
		};
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
		pCmd->pDxCmdList->SetComputeRootShaderResourceView(1, pDesc->pTopLevelAccelerationStructure->pBuffer->pDxResource->GetGPUVirtualAddress());

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
	case ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL:
		return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	case ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL:
		return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	default:
		ASSERT(false && "Invalid Acceleration Structure Type");
		return (D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE)-1;
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
		bottomLevelDescriptor = add_cpu_descriptor_handles(pRaytracing->pRenderer->pCbvSrvUavHeap, 1, &descriptorHeapIndex);
		pRaytracing->pRenderer->pDevice->CreateUnorderedAccessView(resource, nullptr, &rawBufferUavDesc, bottomLevelDescriptor);
	}
	return pRaytracing->pFallbackDevice->GetWrappedPointerSimple((UINT)descriptorHeapIndex, resource->GetGPUVirtualAddress());
}
/************************************************************************/
/************************************************************************/
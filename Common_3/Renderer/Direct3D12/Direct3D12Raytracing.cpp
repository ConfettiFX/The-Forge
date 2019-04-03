#ifdef DIRECT3D12

// Socket is used in microprofile this header need to be included before d3d12 headers
#include <WinSock2.h>
#include <d3d12.h>
#include <d3dcompiler.h>

// OS
#include "../../OS/Interfaces/ILogManager.h"
#include "../../ThirdParty/OpenSource/TinySTL/unordered_set.h"

// Renderer
#include "../IRay.h"
#include "../IRenderer.h"
#include "../ResourceLoader.h"
#include "Direct3D12Hooks.h"
#include "Direct3D12MemoryAllocator.h"
#include "../../OS/Interfaces/IMemoryManager.h"

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL	((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)

//check if WindowsSDK is used which supports raytracing
#ifdef D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT

// TODO: all thesae definitions are also declared in Direct3D12: move to a common H file
typedef struct DescriptorStoreHeap
{
	uint32_t mNumDescriptors;
	/// DescriptorInfo Increment Size
	uint32_t mDescriptorSize;
	/// Bitset for finding SAFE_FREE descriptor slots
	uint32_t* flags;
	/// Lock for multi-threaded descriptor allocations
	Mutex* pAllocationMutex;
	uint64_t mUsedDescriptors;
	/// Type of descriptor heap -> CBV / DSV / ...
	D3D12_DESCRIPTOR_HEAP_TYPE mType;
	/// DX Heap
	ID3D12DescriptorHeap* pCurrentHeap;
	/// Start position in the heap
	D3D12_CPU_DESCRIPTOR_HANDLE mStartCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mStartGpuHandle;
} DescriptorStoreHeap;

/// Descriptor table structure holding the native descriptor set handle
typedef struct DescriptorTable
{
	/// Handle to the start of the cbv_srv_uav descriptor table in the gpu visible cbv_srv_uav heap
	D3D12_CPU_DESCRIPTOR_HANDLE mBaseCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mBaseGpuHandle;
	uint32_t                    mDescriptorCount;
	uint32_t                    mNodeIndex;
} DescriptorTable;

#define MAX_FRAMES_IN_FLIGHT 3U
using HashMap = tinystl::unordered_map<uint64_t, uint32_t>;

typedef struct DescriptorBinderNode
{
	DescriptorTable* pCbvSrvUavTables[MAX_FRAMES_IN_FLIGHT][DESCRIPTOR_UPDATE_FREQ_COUNT];
	DescriptorTable* pSamplerTables[MAX_FRAMES_IN_FLIGHT][DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t         mCbvSrvUavUsageCount[MAX_FRAMES_IN_FLIGHT][DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t         mSamplerUsageCount[MAX_FRAMES_IN_FLIGHT][DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint64_t         mUpdatedNoneFreqHash[MAX_FRAMES_IN_FLIGHT][2];  // 2 is for CbvSrvUav (0) and Sampler (1)
	uint64_t         mUpdatedFrameFreqHash[MAX_FRAMES_IN_FLIGHT][2];
	HashMap          mUpdatedBatchFreqHashes[MAX_FRAMES_IN_FLIGHT][2];
	uint32_t         mCbvSrvUavUpdatesThisFrame[MAX_FRAMES_IN_FLIGHT][DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t         mSamplerUpdatesThisFrame[MAX_FRAMES_IN_FLIGHT][DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t         mLastFrameUpdated;
	uint32_t         mMaxUsagePerSet[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t         numDescriptorsPerSet[DESCRIPTOR_UPDATE_FREQ_COUNT][2];

	/// Array of flags to check whether a descriptor table of the update frequency is already bound to avoid unnecessary rebinding of descriptor tables
	bool mBoundCbvSrvUavTables[DESCRIPTOR_UPDATE_FREQ_COUNT];
	bool mBoundSamplerTables[DESCRIPTOR_UPDATE_FREQ_COUNT];
	/// Array of view descriptor handles per update frequency to be copied into the gpu visible view heap
	D3D12_CPU_DESCRIPTOR_HANDLE* pViewDescriptorHandles[DESCRIPTOR_UPDATE_FREQ_COUNT];
	/// Array of sampler descriptor handles per update frequency to be copied into the gpu visible sampler heap
	D3D12_CPU_DESCRIPTOR_HANDLE* pSamplerDescriptorHandles[DESCRIPTOR_UPDATE_FREQ_COUNT];
	/// Triple buffered array of number of descriptor tables allocated per update frequency
	/// Only used for recording stats
	uint32_t mDescriptorTableCount[MAX_FRAMES_IN_FLIGHT][DESCRIPTOR_UPDATE_FREQ_COUNT];
} DescriptorBinderNode;

using DescriptorBinderMap = tinystl::unordered_map<const RootSignature*, DescriptorBinderNode>;
using DescriptorBinderMapNode = tinystl::unordered_hash_node<const RootSignature*, DescriptorBinderNode>;

typedef struct DescriptorBinder
{
	DescriptorStoreHeap* pCbvSrvUavHeap[MAX_GPUS];
	DescriptorStoreHeap* pSamplerHeap[MAX_GPUS];
	Cmd*                 pCurrentCmd;
	DescriptorBinderMap  mRootSignatureNodes;
} DescriptorBinder;

#ifndef ENABLE_RENDERER_RUNTIME_SWITCH
extern void addBuffer(Renderer* pRenderer, const BufferDesc* desc, Buffer** pp_buffer);
extern void removeBuffer(Renderer* pRenderer, Buffer* p_buffer);
#endif

extern void add_descriptor_heap(Renderer* pRenderer, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, uint32_t numDescriptors, struct DescriptorStoreHeap** ppDescHeap);
extern void reset_descriptor_heap(struct DescriptorStoreHeap* pHeap);
extern void remove_descriptor_heap(struct DescriptorStoreHeap* pHeap);
extern D3D12_CPU_DESCRIPTOR_HANDLE add_cpu_descriptor_handles(struct DescriptorStoreHeap* pHeap, uint32_t numDescriptors, uint32_t* pDescriptorIndex = NULL);
extern void add_gpu_descriptor_handles(struct DescriptorStoreHeap* pHeap, D3D12_CPU_DESCRIPTOR_HANDLE* pStartCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* pStartGpuHandle, uint32_t numDescriptors);
extern void remove_gpu_descriptor_handles(DescriptorStoreHeap* pHeap, D3D12_GPU_DESCRIPTOR_HANDLE* startHandle, uint64_t numDescriptors);

// Enable experimental features and return if they are supported.
// To test them being supported we need to check both their enablement as well as device creation afterwards.
inline bool EnableD3D12ExperimentalFeatures(UUID* experimentalFeatures, uint32_t featureCount)
{
	ID3D12Device* testDevice = NULL;
	bool ret = SUCCEEDED(D3D12EnableExperimentalFeatures(featureCount, experimentalFeatures, NULL, NULL))
		&& SUCCEEDED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)));
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
D3D12_RAYTRACING_GEOMETRY_FLAGS util_to_dx_geometry_flags(AccelerationStructureGeometryFlags flags);
D3D12_RAYTRACING_INSTANCE_FLAGS util_to_dx_instance_flags(AccelerationStructureInstanceFlags flags);
/************************************************************************/
// Forge Raytracing Implementation using DXR
/************************************************************************/
struct RaytracingShader
{
	Shader*		pShader;
};

struct RayConfigBlock
{
	uint32_t mHitGroupIndex;
	uint32_t mMissGroupIndex;
};

struct AccelerationStructureBottom
{
	Buffer*											 pVertexBuffer;
	Buffer*											 pIndexBuffer;
	Buffer*											 pASBuffer;
	D3D12_RAYTRACING_GEOMETRY_DESC*					 pGeometryDescs;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS mFlags;
	uint32_t											mDescCount;
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
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS mFlags;
};

struct RaytracingShaderTable
{
	Pipeline*					pPipeline;
	Buffer*						pBuffer;
	D3D12_GPU_DESCRIPTOR_HANDLE mViewGpuDescriptorHandle[DESCRIPTOR_UPDATE_FREQ_COUNT];
	D3D12_GPU_DESCRIPTOR_HANDLE mSamplerGpuDescriptorHandle[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t					mViewDescriptorCount[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint32_t					mSamplerDescriptorCount[DESCRIPTOR_UPDATE_FREQ_COUNT];
	uint64_t					mMaxEntrySize;
	uint64_t					mMissRecordSize;
	uint64_t					mHitGroupRecordSize;

	tinystl::vector<Buffer*> hitConfigBuffers;
	tinystl::vector<Buffer*> missConfigBuffers;
	Buffer* pRayGenConfigBuffer;
	DescriptorBinder* pDescriptorBinder;
};

struct RaytracingShaderTableRecordDXDesc
{
	RaytracingShaderTableRecordDesc common; //api agnostic part

	RootSignature*  pRootSignature;
	DescriptorData* pRootData;
	unsigned		mRootDataCount;
};

struct RaytracingShaderTableDXDesc
{
	Pipeline*						    pPipeline;
	RootSignature*						pEmptyRootSignature;
	DescriptorBinder*					pDescriptorBinder;
	RaytracingShaderTableRecordDXDesc*	pRayGenShader;
	RaytracingShaderTableRecordDXDesc*	pMissShaders;
	RaytracingShaderTableRecordDXDesc*	pHitGroups;
	unsigned							mMissShaderCount;
	unsigned							mHitGroupCount;
};

bool isRaytracingSupported(Renderer* pRenderer)
{
	ASSERT(pRenderer);
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 opts5 = {};
	pRenderer->pDxDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opts5, sizeof(opts5));
	return (opts5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);
}

bool initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing)
{
	ASSERT(pRenderer);
	ASSERT(ppRaytracing);

	if (!isRaytracingSupported(pRenderer)) return false;

	Raytracing* pRaytracing = (Raytracing*)conf_calloc(1, sizeof(*pRaytracing));
	ASSERT(pRaytracing);

	pRaytracing->pRenderer = pRenderer;
	pRenderer->pDxDevice->QueryInterface(&pRaytracing->pDxrDevice);

	*ppRaytracing = pRaytracing;
	return true;
}

void removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing)
{
	ASSERT(pRenderer);
	ASSERT(pRaytracing);

	if (pRaytracing->pDxrDevice)
		pRaytracing->pDxrDevice->Release();

	conf_free(pRaytracing);
}

Buffer* createGeomVertexBuffer(const AccelerationStructureGeometryDesc* desc)
{
	ASSERT(desc->pVertexArray);
	ASSERT(desc->vertexCount > 0);

	Buffer* result = NULL;
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
	ASSERT(desc->pIndices16 != NULL || desc->pIndices32 != NULL);
	ASSERT(desc->indicesCount > 0);

	Buffer* result = NULL;
	BufferLoadDesc indexBufferDesc = {};
	indexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
	indexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	indexBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	indexBufferDesc.mDesc.mSize = sizeof(uint) * desc->indicesCount;
	indexBufferDesc.mDesc.mIndexType = desc->indexType;
	indexBufferDesc.pData = desc->indexType == INDEX_TYPE_UINT32 ? (void*) desc->pIndices32 : (void*) desc->pIndices16;
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
		pResult[i].mFlags = util_to_dx_acceleration_structure_build_flags(pDesc->mBottomASDescs[i].mFlags);
		pResult[i].pGeometryDescs = (D3D12_RAYTRACING_GEOMETRY_DESC*)conf_calloc(pResult[i].mDescCount, sizeof(D3D12_RAYTRACING_GEOMETRY_DESC));
		for (uint32_t j = 0; j < pResult[i].mDescCount; ++j)
		{
			AccelerationStructureGeometryDesc* pGeom = &pDesc->mBottomASDescs[i].pGeometryDescs[j];
			D3D12_RAYTRACING_GEOMETRY_DESC* pGeomD3D12 = &pResult[i].pGeometryDescs[j];

			pGeomD3D12->Flags = util_to_dx_geometry_flags(pGeom->mFlags);

			pResult[i].pIndexBuffer = NULL;
			if (pGeom->indicesCount > 0)
			{
				pResult[i].pIndexBuffer = createGeomIndexBuffer(pGeom);
				pGeomD3D12->Triangles.IndexBuffer = pResult[i].pIndexBuffer->mDxGpuAddress;
				pGeomD3D12->Triangles.IndexCount = (UINT)pResult[i].pIndexBuffer->mDesc.mSize /
					(pResult[i].pIndexBuffer->mDesc.mIndexType == INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t));
				pGeomD3D12->Triangles.IndexFormat = pResult[i].pIndexBuffer->mDxIndexFormat;
			}

			pResult[i].pVertexBuffer = createGeomVertexBuffer(pGeom);
			pGeomD3D12->Triangles.VertexBuffer.StartAddress = pResult[i].pVertexBuffer->mDxGpuAddress;
			pGeomD3D12->Triangles.VertexBuffer.StrideInBytes = (UINT)pResult[i].pVertexBuffer->mDesc.mVertexStride;
			pGeomD3D12->Triangles.VertexCount = (UINT)pResult[i].pVertexBuffer->mDesc.mSize / (UINT)pResult[i].pVertexBuffer->mDesc.mVertexStride;
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
		prebuildDesc.Flags = pResult[i].mFlags;
		prebuildDesc.NumDescs = pResult[i].mDescCount;
		prebuildDesc.pGeometryDescs = pResult[i].pGeometryDescs;
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
		bufferDesc.mSize = info.ResultDataMaxSizeInBytes; //Rustam: isn't this should be sizeof(UINT32) ?
		bufferDesc.mStartState = (ResourceState)D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		addBuffer(pRaytracing->pRenderer, &bufferDesc, &pResult[i].pASBuffer);
		/************************************************************************/
		// Store the scratch buffer size so user can create the scratch buffer accordingly
		/************************************************************************/
		scratchBufferSize = (UINT)info.ScratchDataSizeInBytes > scratchBufferSize ? (UINT)info.ScratchDataSizeInBytes  : scratchBufferSize;
	}

	*pScratchBufferSize = scratchBufferSize;
	return pResult;
}

Buffer* createTopAS(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, const AccelerationStructureBottom* pASBottom, uint32_t* pScratchBufferSize, Buffer** ppInstanceDescBuffer)
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
	tinystl::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs(pDesc->mInstancesDescCount);
	for (uint32_t i = 0; i < pDesc->mInstancesDescCount; ++i)
	{
		AccelerationStructureInstanceDesc* pInst = &pDesc->pInstanceDescs[i];
		Buffer* pASBuffer = pASBottom[pInst->mAccelerationStructureIndex].pASBuffer;
		instanceDescs[i].AccelerationStructure = pASBuffer->pDxResource->GetGPUVirtualAddress();
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
	Buffer* pInstanceDescBuffer = NULL;
	addBuffer(pRaytracing->pRenderer, &instanceDesc, &pInstanceDescBuffer);
	memcpy(pInstanceDescBuffer->pCpuMappedAddress, instanceDescs.data(), instanceDesc.mSize);

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
	bufferDesc.mStartState = (ResourceState)D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	Buffer* pTopASBuffer = NULL;
	addBuffer(pRaytracing->pRenderer, &bufferDesc, &pTopASBuffer);

	*pScratchBufferSize = (UINT)info.ScratchDataSizeInBytes;
	*ppInstanceDescBuffer = pInstanceDescBuffer;
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
	pAccelerationStructure->pASBuffer = createTopAS(pRaytracing, pDesc, pAccelerationStructure->ppBottomAS, &scratchTopBufferSize, &pAccelerationStructure->pInstanceDescBuffer);
	pAccelerationStructure->mScratchBufferSize = max(scratchBottomBufferSize, scratchTopBufferSize);
	pAccelerationStructure->mFlags = util_to_dx_acceleration_structure_build_flags(pDesc->mFlags);

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

void removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
	ASSERT(pRaytracing);
	ASSERT(pAccelerationStructure);

	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pASBuffer);
	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pInstanceDescBuffer);
	removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pScratchBuffer);

	for (unsigned i = 0; i < pAccelerationStructure->mBottomASCount; ++i)
	{
		removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->ppBottomAS[i].pASBuffer);
		removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->ppBottomAS[i].pVertexBuffer);
		if (pAccelerationStructure->ppBottomAS[i].pIndexBuffer != NULL)
			removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->ppBottomAS[i].pIndexBuffer);
		conf_free(pAccelerationStructure->ppBottomAS[i].pGeometryDescs);
	}
	conf_free(pAccelerationStructure->ppBottomAS);
	conf_free(pAccelerationStructure);
}

void addRaytracingShader(Raytracing* pRaytracing, const unsigned char* pByteCode, unsigned byteCodeSize, const char* pName, RaytracingShader** ppShader)
{
	ASSERT(pRaytracing);
	ASSERT(pByteCode);
	ASSERT(byteCodeSize);
	ASSERT(pName);
	ASSERT(ppShader);
	
	RaytracingShader* pShader = (RaytracingShader*)conf_calloc(1, sizeof(*pShader));
	ASSERT(pShader);

	{
		ShaderLoadDesc desc = {};
		desc.mStages[0] = { (char*)pByteCode, NULL, 0, FSR_SrcShaders };
		desc.mTarget = shader_target_6_3;

		addShader(pRaytracing->pRenderer, &desc, &pShader->pShader);
	}

	*ppShader = pShader;
}

void removeRaytracingShader(Raytracing* pRaytracing, RaytracingShader* pShader)
{
	ASSERT(pRaytracing);
	ASSERT(pShader);

	removeShader(pRaytracing->pRenderer, pShader->pShader);
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
extern const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName, uint32_t* pIndex);

uint32_t setupDescAndLayout(	const ShaderResource* pRes, DescriptorInfo* pDesc,
		tinystl::vector<UpdateFrequencyLayoutInfo>& layouts,
		tinystl::vector <tinystl::pair<DescriptorInfo*, Sampler*> >& staticSamplers,
		const tinystl::unordered_map<tinystl::string, Sampler*>& staticSamplerMap,
		uint32_t maxBindlessTextures)
{
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

	if (pDesc->mDesc.size == 0 && pDesc->mDesc.type == DESCRIPTOR_TYPE_TEXTURE)
	{
		pDesc->mDesc.size = maxBindlessTextures;
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
	else if ((pDesc->mDesc.type == DESCRIPTOR_TYPE_UNIFORM_BUFFER && pDesc->mDesc.size == 1) || pDesc->mDesc.type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
	{
		// D3D12 has no special syntax to declare root constants like Vulkan
		// So we assume that all constant buffers with the word "rootconstant" (case insensitive) are root constants
		if (tinystl::string(pRes->name).to_lower().find("rootconstant", 0) != tinystl::string::npos || pDesc->mDesc.type == DESCRIPTOR_TYPE_ROOT_CONSTANT)
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

	return setIndex;
}

void addRaytracingRootSignature(Renderer* pRenderer, const ShaderResource* pResources, uint32_t resourceCount, 
								bool local, RootSignature** ppRootSignature, const RootSignatureDesc* pRootDesc)
{
	ASSERT(ppRootSignature);

	RootSignature* pRootSignature = (RootSignature*)conf_calloc(1, sizeof(*pRootSignature));
	ASSERT(pRootSignature);
	
	uint32_t additinalResourcesCount = 0;
	if (local)
	{
		//Add shader settings if it is a local root signature
		additinalResourcesCount = 1;
	}

	pRootSignature->mDescriptorCount = resourceCount + additinalResourcesCount;
	// Raytracing is executed in compute path
	pRootSignature->mPipelineType = PIPELINE_TYPE_COMPUTE;

	if (resourceCount + additinalResourcesCount > 0)
		pRootSignature->pDescriptors = (DescriptorInfo*)conf_calloc(pRootSignature->mDescriptorCount, sizeof(*pRootSignature->pDescriptors));

	//pRootSignature->pDescriptorNameToIndexMap;
	conf_placement_new<tinystl::unordered_map<uint32_t, uint32_t> >(
		&pRootSignature->pDescriptorNameToIndexMap);

	const RootSignatureDesc* pRootSignatureDesc = pRootDesc ? pRootDesc : &gDefaultRootSignatureDesc;

	tinystl::unordered_map<tinystl::string, Sampler*> staticSamplerMap;
	for (uint32_t i = 0; i < pRootSignatureDesc->mStaticSamplerCount; ++i)
		staticSamplerMap.insert({ pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i] });

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
		uint32_t setIndex = setupDescAndLayout(pRes, pDesc, layouts, staticSamplers, staticSamplerMap, pRootSignatureDesc->mMaxBindlessTextures);
		pRootSignature->pDescriptorNameToIndexMap.insert({ tinystl::hash(pDesc->mDesc.name), i });
		layouts[setIndex].mDescriptorIndexMap[pDesc] = i;
	}
	if (local)
	{
		DescriptorInfo* pDesc = &pRootSignature->pDescriptors[resourceCount];
		ShaderResource res = {};
		res.name = RaytracingShaderSettingsBufferName;
		res.name_size = (uint32_t)strlen(res.name);
		res.reg = 1;
		res.set = 0;
		res.size = 1;
		res.type = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		res.used_stages = SHADER_STAGE_COMP;

		uint32_t setIndex = setupDescAndLayout(&res, pDesc, layouts, staticSamplers, staticSamplerMap, pRootSignatureDesc->mMaxBindlessTextures);
		pRootSignature->pDescriptorNameToIndexMap.insert({ tinystl::hash(pDesc->mDesc.name), resourceCount });
		layouts[setIndex].mDescriptorIndexMap[pDesc] = resourceCount;
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

	for (uint32_t i = 0; i < (uint32_t)layouts.size(); ++i)
	{
		pRootSignature->mDxRootConstantCount += (uint32_t)layouts[i].mRootConstants.size();
		pRootSignature->mDxRootDescriptorCount += (uint32_t)layouts[i].mConstantParams.size();
	}
	if (pRootSignature->mDxRootConstantCount)
		pRootSignature->pDxRootConstantRootIndices = (uint32_t*)conf_calloc(pRootSignature->mDxRootConstantCount, sizeof(*pRootSignature->pDxRootConstantRootIndices));
	if (pRootSignature->mDxRootDescriptorCount)
		pRootSignature->pDxRootDescriptorRootIndices = (uint32_t*)conf_calloc(pRootSignature->mDxRootDescriptorCount, sizeof(*pRootSignature->pDxRootDescriptorRootIndices));
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
			create_descriptor_table_1_0((uint32_t)layout.mSamplerTable.size(), layout.mSamplerTable.data(), samplerRange_1_0[i].data(), &rootParam_1_0);

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

	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pRootSignature->pDxSerializedRootSignatureString, &error_msgs);

	if (!SUCCEEDED(hr))
	{
		char* pMsg = (char*)conf_calloc(error_msgs->GetBufferSize(), sizeof(char));
		memcpy(pMsg, error_msgs->GetBufferPointer(), error_msgs->GetBufferSize());
		LOGERRORF("Failed to serialize root signature with error (%s)", pMsg);
		conf_free(pMsg);
	}

	ASSERT(pRootSignature->pDxSerializedRootSignatureString);

	hr = pRenderer->pDxDevice->CreateRootSignature(0,
		pRootSignature->pDxSerializedRootSignatureString->GetBufferPointer(),
		pRootSignature->pDxSerializedRootSignatureString->GetBufferSize(),
		IID_PPV_ARGS(&pRootSignature->pDxRootSignature));
	ASSERT(SUCCEEDED(hr));

	/************************************************************************/
	/************************************************************************/
	*ppRootSignature = pRootSignature;
}

void addRaytracingPipeline(const RaytracingPipelineDesc* pDesc, Pipeline** ppPipeline)
{
	Raytracing* pRaytracing = pDesc->pRaytracing;

	ASSERT(pRaytracing);
	ASSERT(pDesc);
	ASSERT(ppPipeline);

	Pipeline* pPipeline = (Pipeline*)conf_calloc(1, sizeof(*pPipeline));
	ASSERT(pPipeline);
	/************************************************************************/
	// Pipeline Creation
	/************************************************************************/
	tinystl::vector<D3D12_STATE_SUBOBJECT> subobjects;
	tinystl::vector<tinystl::pair<uint32_t, D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*> > exportAssociationsDelayed;
	// Reserve average number of subobject space in the beginning
	subobjects.reserve(10);
	/************************************************************************/
	// Step 1 - Create DXIL Libraries
	/************************************************************************/
	tinystl::vector<D3D12_DXIL_LIBRARY_DESC> dxilLibDescs;
	tinystl::vector<D3D12_STATE_SUBOBJECT> stateSubobject = {};
	tinystl::vector<D3D12_EXPORT_DESC*> exportDesc = {};

	D3D12_DXIL_LIBRARY_DESC rayGenDesc = {};
	D3D12_EXPORT_DESC rayGenExportDesc = {};
	rayGenExportDesc.ExportToRename = NULL;
	rayGenExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;
	rayGenExportDesc.Name = pDesc->pRayGenShader->pEntryNames[0];

	rayGenDesc.DXILLibrary.BytecodeLength = pDesc->pRayGenShader->pShaderBlobs[0]->GetBufferSize();
	rayGenDesc.DXILLibrary.pShaderBytecode = pDesc->pRayGenShader->pShaderBlobs[0]->GetBufferPointer();
	rayGenDesc.NumExports = 1;
	rayGenDesc.pExports = &rayGenExportDesc;

	dxilLibDescs.emplace_back(rayGenDesc);

	tinystl::vector<LPCWSTR> missShadersEntries(pDesc->mMissShaderCount);
	for (uint32_t i = 0; i < pDesc->mMissShaderCount; ++i)
	{
		D3D12_EXPORT_DESC* pMissExportDesc = (D3D12_EXPORT_DESC*)conf_calloc(1, sizeof(*pMissExportDesc));
		pMissExportDesc->ExportToRename = NULL;
		pMissExportDesc->Flags = D3D12_EXPORT_FLAG_NONE;

		pMissExportDesc->Name = pDesc->ppMissShaders[i]->pEntryNames[0];
		missShadersEntries[i] = pMissExportDesc->Name;

		D3D12_DXIL_LIBRARY_DESC missDesc = {};
		missDesc.DXILLibrary.BytecodeLength = pDesc->ppMissShaders[i]->pShaderBlobs[0]->GetBufferSize();
		missDesc.DXILLibrary.pShaderBytecode = pDesc->ppMissShaders[i]->pShaderBlobs[0]->GetBufferPointer();
		missDesc.NumExports = 1;
		missDesc.pExports = pMissExportDesc;

		exportDesc.emplace_back(pMissExportDesc);
		dxilLibDescs.emplace_back(missDesc);
	}

	tinystl::vector<LPCWSTR> hitGroupsIntersectionsEntries(pDesc->mHitGroupCount);
	tinystl::vector<LPCWSTR> hitGroupsAnyHitEntries(pDesc->mHitGroupCount);
	tinystl::vector<LPCWSTR> hitGroupsClosestHitEntries(pDesc->mHitGroupCount);
	for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
	{
		if (pDesc->pHitGroups[i].pIntersectionShader)
		{
			D3D12_EXPORT_DESC* pIntersectionExportDesc = (D3D12_EXPORT_DESC*)conf_calloc(1, sizeof(*pIntersectionExportDesc));
			pIntersectionExportDesc->ExportToRename = NULL;
			pIntersectionExportDesc->Flags = D3D12_EXPORT_FLAG_NONE;
			pIntersectionExportDesc->Name = pDesc->pHitGroups[i].pIntersectionShader->pEntryNames[0];
			hitGroupsIntersectionsEntries[i] = pIntersectionExportDesc->Name;

			D3D12_DXIL_LIBRARY_DESC intersectionDesc = {};
			intersectionDesc.DXILLibrary.BytecodeLength = pDesc->pHitGroups[i].pIntersectionShader->pShaderBlobs[0]->GetBufferSize();
			intersectionDesc.DXILLibrary.pShaderBytecode = pDesc->pHitGroups[i].pIntersectionShader->pShaderBlobs[0]->GetBufferPointer();
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
			pAnyHitExportDesc->Name = pDesc->pHitGroups[i].pAnyHitShader->pEntryNames[0];
			hitGroupsAnyHitEntries[i] = pAnyHitExportDesc->Name;

			D3D12_DXIL_LIBRARY_DESC anyHitDesc = {};
			anyHitDesc.DXILLibrary.BytecodeLength = pDesc->pHitGroups[i].pAnyHitShader->pShaderBlobs[0]->GetBufferSize();
			anyHitDesc.DXILLibrary.pShaderBytecode = pDesc->pHitGroups[i].pAnyHitShader->pShaderBlobs[0]->GetBufferPointer();
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
			pClosestHitExportDesc->Name = pDesc->pHitGroups[i].pClosestHitShader->pEntryNames[0];
			hitGroupsClosestHitEntries[i] = pClosestHitExportDesc->Name;

			D3D12_DXIL_LIBRARY_DESC closestHitDesc = {};
			closestHitDesc.DXILLibrary.BytecodeLength = pDesc->pHitGroups[i].pClosestHitShader->pShaderBlobs[0]->GetBufferSize();
			closestHitDesc.DXILLibrary.pShaderBytecode = pDesc->pHitGroups[i].pClosestHitShader->pShaderBlobs[0]->GetBufferPointer();
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
	tinystl::vector<D3D12_HIT_GROUP_DESC> hitGroupDescs(pDesc->mHitGroupCount);
	tinystl::vector<D3D12_STATE_SUBOBJECT> hitGroupObjects(pDesc->mHitGroupCount);
	tinystl::vector<WCHAR*> hitGroupNames(pDesc->mHitGroupCount);

	for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
	{
		const RaytracingHitGroup* pHitGroup = &pDesc->pHitGroups[i];
		ASSERT(pDesc->pHitGroups[i].pHitGroupName);
		hitGroupNames[i] = (WCHAR*)conf_calloc(strlen(pDesc->pHitGroups[i].pHitGroupName) + 1, sizeof(WCHAR));
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
	globalRootSignatureObject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	globalRootSignatureObject.pDesc = pDesc->pGlobalRootSignature ? &pDesc->pGlobalRootSignature->pDxRootSignature : NULL;
	subobjects.emplace_back(globalRootSignatureObject);
	/************************************************************************/
	// Step 6 - Local Root Signatures
	/************************************************************************/
	// Local Root Signature for Ray Generation Shader
	D3D12_STATE_SUBOBJECT rayGenRootSignatureObject = {};
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rayGenRootSignatureAssociation = {};
	D3D12_LOCAL_ROOT_SIGNATURE rayGenRSdesc = {};
	//if (pDesc->pRayGenRootSignature)
	{
		rayGenRootSignatureObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
		rayGenRSdesc.pLocalRootSignature = pDesc->pRayGenRootSignature ?
			pDesc->pRayGenRootSignature->pDxRootSignature :
			pDesc->pEmptyRootSignature->pDxRootSignature;
		rayGenRootSignatureObject.pDesc = &rayGenRSdesc;
		subobjects.emplace_back(rayGenRootSignatureObject);

		rayGenRootSignatureAssociation.NumExports = 1;
		rayGenRootSignatureAssociation.pExports = &rayGenExportDesc.Name;

		exportAssociationsDelayed.push_back({ (uint32_t)subobjects.size() - 1, &rayGenRootSignatureAssociation });
	}

	// Local Root Signatures for Miss Shaders
	tinystl::vector<D3D12_STATE_SUBOBJECT> missRootSignatures(pDesc->mMissShaderCount);
	tinystl::vector<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION> missRootSignaturesAssociation(pDesc->mMissShaderCount);
	tinystl::vector<D3D12_LOCAL_ROOT_SIGNATURE> mMissShaderRSDescs(pDesc->mMissShaderCount);;
	for (uint32_t i = 0; i < pDesc->mMissShaderCount; ++i)
	{
		//if (pDesc->ppMissRootSignatures && pDesc->ppMissRootSignatures[i])
		{
			missRootSignatures[i].Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
			if (pDesc->ppMissRootSignatures && pDesc->ppMissRootSignatures[i])
				mMissShaderRSDescs[i].pLocalRootSignature = pDesc->ppMissRootSignatures[i]->pDxRootSignature;
			else
				mMissShaderRSDescs[i].pLocalRootSignature = pDesc->pEmptyRootSignature->pDxRootSignature;
			missRootSignatures[i].pDesc = &mMissShaderRSDescs[i];
			subobjects.emplace_back(missRootSignatures[i]);

			missRootSignaturesAssociation[i].NumExports = 1;
			missRootSignaturesAssociation[i].pExports = &missShadersEntries[i];

			exportAssociationsDelayed.push_back({ (uint32_t)subobjects.size() - 1, &missRootSignaturesAssociation[i] });
		}
	}

	// Local Root Signatures for Hit Groups
	tinystl::vector<D3D12_STATE_SUBOBJECT> hitGroupRootSignatures(pDesc->mHitGroupCount);
	tinystl::vector<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION> hitGroupRootSignatureAssociation(pDesc->mHitGroupCount);
	tinystl::vector<D3D12_LOCAL_ROOT_SIGNATURE> hitGroupRSDescs(pDesc->mHitGroupCount);
	for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
	{
		//if (pDesc->pHitGroups[i].pRootSignature)
		{
			hitGroupRootSignatures[i].Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
			if (pDesc->pHitGroups[i].pRootSignature)
				hitGroupRSDescs[i].pLocalRootSignature = pDesc->pHitGroups[i].pRootSignature->pDxRootSignature;
			else
				hitGroupRSDescs[i].pLocalRootSignature = pDesc->pEmptyRootSignature->pDxRootSignature;
			hitGroupRootSignatures[i].pDesc = &hitGroupRSDescs[i];
			subobjects.emplace_back(hitGroupRootSignatures[i]);

			hitGroupRootSignatureAssociation[i].NumExports = 1;
			hitGroupRootSignatureAssociation[i].pExports = &hitGroupDescs[i].HitGroupExport;

			exportAssociationsDelayed.push_back({ (uint32_t)subobjects.size() - 1, &hitGroupRootSignatureAssociation[i] });
		}
	}
	/************************************************************************/
	// Shader Config
	/************************************************************************/
	{
		D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
		shaderConfig.MaxAttributeSizeInBytes = pDesc->mAttributeSize;
		shaderConfig.MaxPayloadSizeInBytes = pDesc->mPayloadSize;

		D3D12_STATE_SUBOBJECT shaderConfigObject = {};
		shaderConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
		shaderConfigObject.pDesc = &shaderConfig;

		subobjects.push_back(shaderConfigObject);
	}
	/************************************************************************/
	// Export Associations
	/************************************************************************/
	for (uint32_t i = 0; i < (uint32_t)exportAssociationsDelayed.size(); ++i)
	{
		exportAssociationsDelayed[i].second->pSubobjectToAssociate = &subobjects[exportAssociationsDelayed[i].first];
		//D3D12_STATE_SUBOBJECT exportAssociationLocalRootSignature;
		//exportAssociationLocalRootSignature.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
		//D3D12_LOCAL_ROOT_SIGNATURE desc;
		//desc.pLocalRootSignature = (ID3D12RootSignature *)subobjects[exportAssociationsDelayed[i].first].pDesc;
		//exportAssociationLocalRootSignature.pDesc = &desc;
		//subobjects.push_back(exportAssociationLocalRootSignature);

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
	HRESULT hr = pRaytracing->pDxrDevice->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&pPipeline->pDxrPipeline));
	ASSERT(SUCCEEDED(hr));
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

static const uint32_t gLocalRootConstantSize = sizeof(UINT);
static const uint32_t gLocalRootDescriptorSize = sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
static const uint32_t gLocalRootDescriptorTableSize = sizeof(D3D12_GPU_DESCRIPTOR_HANDLE);
static const uint64_t gShaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

void FillShaderIdentifiers(	const RaytracingShaderTableRecordDXDesc* pRecords, uint32_t shaderCount, 
							ID3D12StateObjectProperties* pRtsoProps, uint64_t& maxShaderTableSize,
							uint32_t& index, RaytracingShaderTable* pTable, Raytracing* pRaytracing)
{
	for (uint32_t i = 0; i < shaderCount; ++i)
	{
		tinystl::unordered_set<uint32_t> addedTables;

		const RaytracingShaderTableRecordDXDesc* pRecord = &pRecords[i];
		void* pIdentifier = NULL;
		WCHAR* pName = (WCHAR*)conf_calloc(strlen(pRecord->common.pName) + 1, sizeof(WCHAR));
		mbstowcs(pName, pRecord->common.pName, strlen(pRecord->common.pName));

		pIdentifier = pRtsoProps->GetShaderIdentifier(pName);

		ASSERT(pIdentifier);
		conf_free(pName);

		uint64_t currentPosition = maxShaderTableSize * index++;
		memcpy((uint8_t*)pTable->pBuffer->pCpuMappedAddress + currentPosition, pIdentifier, gShaderIdentifierSize);

		if (!pRecord->pRootSignature)
			continue;

		currentPosition += gShaderIdentifierSize;
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
			uint32_t descIndex = -1;
			const DescriptorInfo* pDesc = &pRecord->pRootSignature->pDescriptors[desc];
			tinystl::unordered_map<uint32_t, const DescriptorData*>::iterator it = data.find(tinystl::hash(pDesc->mDesc.name));
			const DescriptorData* pData = it.node->second;

			switch (pDesc->mDxType)
			{
			case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
			{
				memcpy((uint8_t*)pTable->pBuffer->pCpuMappedAddress + currentPosition, pData->pRootConstant, pDesc->mDesc.size * sizeof(uint32_t));
				currentPosition += pDesc->mDesc.size * sizeof(uint32_t);
				break;
			}
			case D3D12_ROOT_PARAMETER_TYPE_CBV:
			case D3D12_ROOT_PARAMETER_TYPE_SRV:
			case D3D12_ROOT_PARAMETER_TYPE_UAV:
			{
				// Root Descriptors need to be aligned to 8 byte address
				currentPosition = round_up_64(currentPosition, gLocalRootDescriptorSize);
				uint64_t offset = pData->pOffsets ? pData->pOffsets[0] : 0;
				D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = pData->ppBuffers[0]->pDxResource->GetGPUVirtualAddress() + pData->ppBuffers[0]->mPositionInHeap + offset;
				memcpy((uint8_t*)pTable->pBuffer->pCpuMappedAddress + currentPosition, &cbvAddress, sizeof(D3D12_GPU_VIRTUAL_ADDRESS));
				currentPosition += gLocalRootDescriptorSize;
				break;
			}
			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
			{
				RootSignature* pRootSignature = pRecord->pRootSignature;
				const uint32_t setIndex = pDesc->mUpdateFrquency;
				const uint32_t rootIndex = pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER ?
					pRootSignature->mDxSamplerDescriptorTableRootIndices[pDesc->mUpdateFrquency] :
					pRootSignature->mDxViewDescriptorTableRootIndices[pDesc->mUpdateFrquency];
				// Construct a new descriptor table from shader visible heap
				if (addedTables.find(rootIndex) == addedTables.end())
				{
					D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
					D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
					if (pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER)
					{
						add_gpu_descriptor_handles(pTable->pDescriptorBinder->pSamplerHeap[0], &cpuHandle, &gpuHandle,
							pRootSignature->mDxCumulativeSamplerDescriptorCounts[setIndex]);
						pTable->mSamplerGpuDescriptorHandle[pDesc->mUpdateFrquency] = gpuHandle;
						pTable->mSamplerDescriptorCount[pDesc->mUpdateFrquency] = pRootSignature->mDxCumulativeSamplerDescriptorCounts[setIndex];

						for (uint32_t i = 0; i < pRootSignature->mDxSamplerDescriptorCounts[setIndex]; ++i)
						{
							const DescriptorInfo* pTableDesc = &pRootSignature->pDescriptors[pRootSignature->pDxSamplerDescriptorIndices[setIndex][i]];
							const DescriptorData* pTableData = data.find(tinystl::hash(pTableDesc->mDesc.name)).node->second;
							const uint32_t arrayCount = max(1U, pTableData->mCount);
							for (uint32_t samplerIndex = 0; samplerIndex < arrayCount; ++samplerIndex)
							{
								pRaytracing->pRenderer->pDxDevice->CopyDescriptorsSimple(1,
									{ cpuHandle.ptr + (pTableDesc->mHandleIndex + samplerIndex) * pTable->pDescriptorBinder->pSamplerHeap[0]->mDescriptorSize },
									pTableData->ppSamplers[samplerIndex]->mDxSamplerHandle,
									D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
							}
						}
					}
					else
					{
						add_gpu_descriptor_handles(pTable->pDescriptorBinder->pCbvSrvUavHeap[0], &cpuHandle, &gpuHandle,
							pRootSignature->mDxCumulativeViewDescriptorCounts[setIndex]);
						pTable->mViewGpuDescriptorHandle[pDesc->mUpdateFrquency] = gpuHandle;
						pTable->mViewDescriptorCount[pDesc->mUpdateFrquency] = pRootSignature->mDxCumulativeViewDescriptorCounts[setIndex];

						for (uint32_t i = 0; i < pRootSignature->mDxViewDescriptorCounts[setIndex]; ++i)
						{
							const DescriptorInfo* pTableDesc = &pRootSignature->pDescriptors[pRootSignature->pDxViewDescriptorIndices[setIndex][i]];
							const DescriptorData* pTableData = data.find(tinystl::hash(pTableDesc->mDesc.name)).node->second;
							const DescriptorType type = pTableDesc->mDesc.type;
							const uint32_t arrayCount = max(1U, pTableData->mCount);
							switch (type)
							{
							case DESCRIPTOR_TYPE_TEXTURE:
								for (uint32_t textureIndex = 0; textureIndex < arrayCount; ++textureIndex)
								{
									pRaytracing->pRenderer->pDxDevice->CopyDescriptorsSimple(1,
										{ cpuHandle.ptr + (pTableDesc->mHandleIndex + textureIndex) * pTable->pDescriptorBinder->pCbvSrvUavHeap[0]->mDescriptorSize },
										pTableData->ppTextures[textureIndex]->mDxSRVDescriptor,
										D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
								}
								break;
							case DESCRIPTOR_TYPE_RW_TEXTURE:
								for (uint32_t textureIndex = 0; textureIndex < arrayCount; ++textureIndex)
								{
									pRaytracing->pRenderer->pDxDevice->CopyDescriptorsSimple(1,
										{ cpuHandle.ptr + (pTableDesc->mHandleIndex + textureIndex) * pTable->pDescriptorBinder->pCbvSrvUavHeap[0]->mDescriptorSize },
										pTableData->ppTextures[textureIndex]->pDxUAVDescriptors[pTableData->mUAVMipSlice],
										D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
								}
								break;
							case DESCRIPTOR_TYPE_BUFFER:
								for (uint32_t bufferIndex = 0; bufferIndex < arrayCount; ++bufferIndex)
								{
									pRaytracing->pRenderer->pDxDevice->CopyDescriptorsSimple(1,
										{ cpuHandle.ptr + (pTableDesc->mHandleIndex + bufferIndex) * pTable->pDescriptorBinder->pCbvSrvUavHeap[0]->mDescriptorSize },
										pTableData->ppBuffers[bufferIndex]->mDxSrvHandle,
										D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
								}
								break;
							case DESCRIPTOR_TYPE_RW_BUFFER:
								for (uint32_t bufferIndex = 0; bufferIndex < arrayCount; ++bufferIndex)
								{
									pRaytracing->pRenderer->pDxDevice->CopyDescriptorsSimple(1,
										{ cpuHandle.ptr + (pTableDesc->mHandleIndex + bufferIndex) * pTable->pDescriptorBinder->pCbvSrvUavHeap[0]->mDescriptorSize },
										pTableData->ppBuffers[bufferIndex]->mDxUavHandle,
										D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
								}
								break;
								// #TODO : Add DESCRIPTOR_TYPE_UNIFORM_BUFFER if needed.
								// Currently we use root descriptors for uniform buffers in raytracing root signature
							default:
								break;
							}
						}
					}

					// Root Descriptor Tables need to be aligned to 8 byte address
					currentPosition = round_up_64(currentPosition, gLocalRootDescriptorSize);
					memcpy((uint8_t*)pTable->pBuffer->pCpuMappedAddress + currentPosition, &gpuHandle, sizeof(D3D12_GPU_DESCRIPTOR_HANDLE));
					currentPosition += gLocalRootDescriptorTableSize;
				}
				else
				{
					addedTables.insert(rootIndex);
				}
				break;
			}
			default:
				break;
			}
		}
	}
};

void CalculateMaxShaderRecordSize(const RaytracingShaderTableRecordDXDesc* pRecords, uint32_t shaderCount, uint64_t& maxShaderTableSize)
{
	for (uint32_t i = 0; i < shaderCount; ++i)
	{
		tinystl::unordered_set<uint32_t> addedTables;
		const RaytracingShaderTableRecordDXDesc* pRecord = &pRecords[i];
		uint32_t shaderSize = 0;
		for (uint32_t desc = 0; desc < pRecord->mRootDataCount; ++desc)
		{
			uint32_t descIndex = -1;
			const DescriptorInfo* pDesc = get_descriptor(pRecord->pRootSignature, pRecord->pRootData[desc].pName, &descIndex);
			ASSERT(pDesc);

			switch (pDesc->mDxType)
			{
			case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				shaderSize += pDesc->mDesc.size * gLocalRootConstantSize;
				break;
			case D3D12_ROOT_PARAMETER_TYPE_CBV:
			case D3D12_ROOT_PARAMETER_TYPE_SRV:
			case D3D12_ROOT_PARAMETER_TYPE_UAV:
				shaderSize += gLocalRootDescriptorSize;
				break;
			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
			{
				const uint32_t rootIndex = pDesc->mDesc.type == DESCRIPTOR_TYPE_SAMPLER ?
					pRecord->pRootSignature->mDxSamplerDescriptorTableRootIndices[pDesc->mUpdateFrquency] :
					pRecord->pRootSignature->mDxViewDescriptorTableRootIndices[pDesc->mUpdateFrquency];
				if (addedTables.find(rootIndex) == addedTables.end())
					shaderSize += gLocalRootDescriptorTableSize;
				else
					addedTables.insert(rootIndex);
				break;
			}
			default:
				break;
			}
		}

		maxShaderTableSize = max(maxShaderTableSize, shaderSize);
	}
};

void SetupEmptyLocalRootDescriptors(RaytracingShaderTableRecordDXDesc* records, unsigned count, Buffer** ppBuffers, RootSignature* pDefaultLocalRootSignature)
{
	for (unsigned i = 0; i < count; ++i)
	{
		if (!records[i].common.mInvokeTraceRay)
			continue;

		if (records[i].pRootSignature == NULL)
		{
			records[i].pRootSignature = pDefaultLocalRootSignature;
			records[i].pRootData = (DescriptorData*)conf_calloc(1, sizeof(DescriptorData));
			records[i].mRootDataCount = 1;
			records[i].pRootData[0].pName = RaytracingShaderSettingsBufferName;
			records[i].pRootData[0].ppBuffers = ppBuffers + i;
		}
		else
		{
			DescriptorData* old = records[i].pRootData;
			unsigned oldCount = records[i].mRootDataCount;
			records[i].pRootData = (DescriptorData*)conf_calloc(records[i].mRootDataCount + 1, sizeof(DescriptorData));
			memcpy(records[i].pRootData, old, oldCount * sizeof(DescriptorData));

			//set settings buffer
			records[i].pRootData[oldCount].pName		= RaytracingShaderSettingsBufferName;
			records[i].pRootData[oldCount].ppBuffers	= ppBuffers + i;
			records[i].mRootDataCount					= oldCount + 1;
		}
	}
}

void SetupSingleConfigBuffer(const RaytracingShaderTableRecordDesc* desc, Buffer** ppBuffer)
{
	RayConfigBlock configBlock;
	configBlock.mHitGroupIndex = desc->mHitShaderIndex;
	configBlock.mMissGroupIndex = desc->mMissShaderIndex;

	BufferLoadDesc ubDesc = {};
	ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	ubDesc.mDesc.mSize = sizeof(RayConfigBlock);
	ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	ubDesc.pData = &configBlock;
	ubDesc.ppBuffer = ppBuffer;
	addResource(&ubDesc);
}

void SetupConfigBuffers(const RaytracingShaderTableDesc* pDesc, tinystl::vector<Buffer*>& hitBuffers, tinystl::vector<Buffer*>& missBuffers, Buffer** pRayGenBuffer)
{
	if (pDesc->pRayGenShader->mInvokeTraceRay)
	{
		// Update uniform buffers
		SetupSingleConfigBuffer(pDesc->pRayGenShader, pRayGenBuffer);
	}

	hitBuffers.clear();
	hitBuffers.resize(pDesc->mHitGroupCount, NULL);
	for (unsigned i = 0; i < pDesc->mHitGroupCount; ++i)
	{
		if (pDesc->pHitGroups[i].mInvokeTraceRay)
		{
			SetupSingleConfigBuffer(&pDesc->pHitGroups[i], &hitBuffers[i]);
		}
	}

	missBuffers.clear();
	missBuffers.resize(pDesc->mMissShaderCount, NULL);
	for (unsigned i = 0; i < pDesc->mMissShaderCount; ++i)
	{
		if (pDesc->pMissShaders[i].mInvokeTraceRay)
		{
			SetupSingleConfigBuffer(&pDesc->pMissShaders[i], &missBuffers[i]);
		}
	}
}

RaytracingShaderTableDXDesc* CopyTableAndSetupLocalSignature(	const RaytracingShaderTableDesc* pInitialDesc,
															tinystl::vector<Buffer*>& hitBuffers,
															tinystl::vector<Buffer*>& missBuffers,
															Buffer** ppRayGenBuffer)
{
	RaytracingShaderTableDXDesc* newDesc = (RaytracingShaderTableDXDesc*)conf_calloc(1, sizeof(RaytracingShaderTableDXDesc));
	newDesc->pPipeline			= pInitialDesc->pPipeline;
	newDesc->pDescriptorBinder  = pInitialDesc->pDescriptorBinder;
	newDesc->mHitGroupCount		= pInitialDesc->mHitGroupCount;
	newDesc->mMissShaderCount	= pInitialDesc->mMissShaderCount;
	newDesc->pHitGroups		= (RaytracingShaderTableRecordDXDesc*)conf_calloc(newDesc->mHitGroupCount, sizeof(RaytracingShaderTableRecordDXDesc));
	newDesc->pMissShaders	= (RaytracingShaderTableRecordDXDesc*)conf_calloc(newDesc->mMissShaderCount, sizeof(RaytracingShaderTableRecordDXDesc));
	newDesc->pRayGenShader = (RaytracingShaderTableRecordDXDesc*)conf_calloc(1, sizeof(RaytracingShaderTableRecordDXDesc));

	memset(newDesc->pHitGroups, 0, newDesc->mHitGroupCount * sizeof(RaytracingShaderTableRecordDXDesc));
	memset(newDesc->pMissShaders, 0, newDesc->mMissShaderCount * sizeof(RaytracingShaderTableRecordDXDesc));
	memset(newDesc->pRayGenShader, 0, sizeof(RaytracingShaderTableRecordDXDesc));

	for (unsigned i = 0; i < newDesc->mHitGroupCount; ++i)
		memcpy(&newDesc->pHitGroups[i].common, &pInitialDesc->pHitGroups[i], sizeof(RaytracingShaderTableRecordDesc));

	for (unsigned i = 0; i < newDesc->mMissShaderCount; ++i)
		memcpy(&newDesc->pMissShaders[i].common, &pInitialDesc->pMissShaders[i], sizeof(RaytracingShaderTableRecordDesc));

	memcpy(&newDesc->pRayGenShader->common, pInitialDesc->pRayGenShader, sizeof(RaytracingShaderTableRecordDesc));


	SetupConfigBuffers(pInitialDesc, hitBuffers, missBuffers, ppRayGenBuffer);

	//now iterate over records and put default local root signature where there is no one
	SetupEmptyLocalRootDescriptors(newDesc->pHitGroups, newDesc->mHitGroupCount, hitBuffers.data(), pInitialDesc->pEmptyRootSignature);
	SetupEmptyLocalRootDescriptors(newDesc->pMissShaders, newDesc->mMissShaderCount, missBuffers.data(), pInitialDesc->pEmptyRootSignature);
	SetupEmptyLocalRootDescriptors(newDesc->pRayGenShader, 1, ppRayGenBuffer, pInitialDesc->pEmptyRootSignature);

	return newDesc;
}

void removeRaytracingShaderTableDesc(RaytracingShaderTableDXDesc* pDesc)
{
	for (unsigned i = 0; i < pDesc->mHitGroupCount; ++i)
	{
		if (pDesc->pHitGroups[i].common.mInvokeTraceRay)
		{
			conf_free(pDesc->pHitGroups[i].pRootData);
		}
	}

	for (unsigned i = 0; i < pDesc->mMissShaderCount; ++i)
	{
		if (pDesc->pMissShaders[i].common.mInvokeTraceRay)
		{
			conf_free(pDesc->pMissShaders[i].pRootData);
		}
	}

	if (pDesc->pRayGenShader->common.mInvokeTraceRay)
	{
		conf_free(pDesc->pRayGenShader->pRootData);
	}

	conf_free(pDesc->pHitGroups);
	conf_free(pDesc->pMissShaders);
	conf_free(pDesc->pRayGenShader);
	conf_free(pDesc);
}

void addRaytracingShaderTable(Raytracing* pRaytracing, const RaytracingShaderTableDesc* pInitialDesc, RaytracingShaderTable** ppTable)
{
	ASSERT(pRaytracing);
	ASSERT(pInitialDesc);
	ASSERT(pInitialDesc->pPipeline);
	ASSERT(ppTable);

	RaytracingShaderTable* pTable = (RaytracingShaderTable*)conf_calloc(1, sizeof(*pTable));
	conf_placement_new<RaytracingShaderTable>((void*)pTable);
	ASSERT(pTable);

	RaytracingShaderTableDXDesc* pDesc = CopyTableAndSetupLocalSignature(pInitialDesc, 
																		pTable->hitConfigBuffers, 
																		pTable->missConfigBuffers, 
																		&pTable->pRayGenConfigBuffer);
	pTable->pPipeline = pDesc->pPipeline;
	pTable->pDescriptorBinder = pDesc->pDescriptorBinder;

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
	maxShaderTableSize = round_up_64(gShaderIdentifierSize + maxShaderTableSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	pTable->mMaxEntrySize = maxShaderTableSize;
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
	ID3D12StateObjectProperties* pRtsoProps = NULL;
	pDesc->pPipeline->pDxrPipeline->QueryInterface(IID_PPV_ARGS(&pRtsoProps));
	   
	uint32_t index = 0;
	FillShaderIdentifiers(	pDesc->pRayGenShader, 1, pRtsoProps,
							maxShaderTableSize, index, pTable, pRaytracing);

	pTable->mMissRecordSize = maxShaderTableSize * pDesc->mMissShaderCount;
	FillShaderIdentifiers(	pDesc->pMissShaders, pDesc->mMissShaderCount, pRtsoProps, 
							maxShaderTableSize, index, pTable, pRaytracing);

	pTable->mHitGroupRecordSize = maxShaderTableSize * pDesc->mHitGroupCount;
	FillShaderIdentifiers(	pDesc->pHitGroups, pDesc->mHitGroupCount, pRtsoProps, 
							maxShaderTableSize, index, pTable, pRaytracing);

	if (pRtsoProps)
		pRtsoProps->Release();
	/************************************************************************/
	/************************************************************************/

	removeRaytracingShaderTableDesc(pDesc);

	*ppTable = pTable;
}

void removeRaytracingShaderTable(Raytracing* pRaytracing, RaytracingShaderTable* pTable)
{
	ASSERT(pRaytracing);
	ASSERT(pTable);

	removeBuffer(pRaytracing->pRenderer, pTable->pBuffer);
	removeBuffer(pRaytracing->pRenderer, pTable->pRayGenConfigBuffer);
	for (unsigned i = 0; i < pTable->hitConfigBuffers.size(); ++i)
	{
		if (pTable->hitConfigBuffers[i] != NULL)
			removeBuffer(pRaytracing->pRenderer, pTable->hitConfigBuffers[i]);
	}
	for (unsigned i = 0; i < pTable->missConfigBuffers.size(); ++i)
	{
		if (pTable->missConfigBuffers[i] != NULL)
			removeBuffer(pRaytracing->pRenderer, pTable->missConfigBuffers[i]);
	}

	for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
	{
		if (pTable->mViewGpuDescriptorHandle[i].ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
			remove_gpu_descriptor_handles(pTable->pDescriptorBinder->pCbvSrvUavHeap[0], &pTable->mViewGpuDescriptorHandle[i], pTable->mViewDescriptorCount[i]);

		if (pTable->mSamplerGpuDescriptorHandle[i].ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL)
			remove_gpu_descriptor_handles(pTable->pDescriptorBinder->pCbvSrvUavHeap[0], &pTable->mSamplerGpuDescriptorHandle[i], pTable->mSamplerDescriptorCount[i]);
	}

	pTable->~RaytracingShaderTable();
	conf_free(pTable);
}

/************************************************************************/
// Raytracing Command Buffer Functions Implementation
/************************************************************************/
void cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, Buffer* pScratchBuffer, Buffer* pASBuffer, 
									D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE ASType, 
									D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS ASFlags,
									const D3D12_RAYTRACING_GEOMETRY_DESC * pGeometryDescs,
									const Buffer* pInstanceDescBuffer,
									uint32_t descCount)
{
	ASSERT(pCmd);
	ASSERT(pRaytracing);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	buildDesc.Inputs.Type = ASType;
	buildDesc.DestAccelerationStructureData = pASBuffer->pDxResource->GetGPUVirtualAddress();
	buildDesc.Inputs.Flags = ASFlags;
	buildDesc.Inputs.pGeometryDescs = NULL;

	if (ASType == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL)
		buildDesc.Inputs.pGeometryDescs = pGeometryDescs;
	else if (ASType == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL)
		buildDesc.Inputs.InstanceDescs = pInstanceDescBuffer->pDxResource->GetGPUVirtualAddress() + pInstanceDescBuffer->mPositionInHeap;

	buildDesc.Inputs.NumDescs = descCount;
	buildDesc.ScratchAccelerationStructureData = pScratchBuffer->pDxResource->GetGPUVirtualAddress();

	ID3D12GraphicsCommandList4* pDxrCmd = NULL;
	pCmd->pDxCmdList->QueryInterface(&pDxrCmd);
	pDxrCmd->BuildRaytracingAccelerationStructure(&buildDesc, 0, NULL);
	pDxrCmd->Release();

	// Make sure the Acceleration Structure is ready before using it in a raytracing operation
	cmdSynchronizeResources(pCmd, 1, &pASBuffer, 0, NULL, false);
}

void cmdBuildTopAS(Cmd* pCmd, Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
	cmdBuildAccelerationStructure(pCmd, pRaytracing,
									pAccelerationStructure->pScratchBuffer,
									pAccelerationStructure->pASBuffer,
									D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
									pAccelerationStructure->mFlags,
									NULL,
									pAccelerationStructure->pInstanceDescBuffer,
									pAccelerationStructure->mInstanceDescCount);
}

void cmdBuildBottomAS(Cmd* pCmd, Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure, unsigned bottomASIndex)
{
	cmdBuildAccelerationStructure(pCmd, pRaytracing, 
									pAccelerationStructure->pScratchBuffer, 
									pAccelerationStructure->ppBottomAS[bottomASIndex].pASBuffer,
									D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
									pAccelerationStructure->ppBottomAS[bottomASIndex].mFlags,
									pAccelerationStructure->ppBottomAS[bottomASIndex].pGeometryDescs,
									NULL, pAccelerationStructure->ppBottomAS[bottomASIndex].mDescCount);
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

void cmdDispatchRays(Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc)
{
	cmdBindDescriptors(pCmd, pDesc->pShaderTable->pDescriptorBinder, 
						pDesc->pRootSignature,
						pDesc->mRootSignatureDescriptorsCount, 
						pDesc->pRootSignatureDescriptorData);

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

	// We dont expose binding the top level AS to the app since that would require changing the cmdBindDescriptors function in the core framework
	pCmd->pDxCmdList->SetComputeRootShaderResourceView(0, pDesc->pTopLevelAccelerationStructure->pASBuffer->pDxResource->GetGPUVirtualAddress());

	ID3D12GraphicsCommandList4* pDxrCmd = NULL;
	pCmd->pDxCmdList->QueryInterface(&pDxrCmd);
	pDxrCmd->SetPipelineState1(pShaderTable->pPipeline->pDxrPipeline);
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
		ret |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE;
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_OPAQUE)
		ret |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE)
		ret |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
	if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE)
		ret |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;

	return ret;
}

#else
void addRaytracingPipeline(const RaytracingPipelineDesc* pDesc, Pipeline** ppPipeline)
{
}

void addRaytracingRootSignature(Renderer* pRenderer, const ShaderResource* pResources, uint32_t resourceCount,
	bool local, RootSignature** ppRootSignature, const RootSignatureDesc* pRootDesc)
{
}
#endif
#endif
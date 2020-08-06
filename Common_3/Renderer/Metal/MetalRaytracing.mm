/*
 * Copyright (c) 2018 The Forge Interactive Inc.
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

/*
 https://developer.apple.com/documentation/metalperformanceshaders/metal_for_accelerating_ray_tracing
 
 iOS 12.0+
 macOS 10.14+
 Xcode 10.0+
 */

#ifdef METAL

#if !defined(__APPLE__) && !defined(TARGET_OS_MAC)
#error "MacOs is needed!"
#endif

#import <simd/simd.h>
#import <MetalKit/MetalKit.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

#include "../../ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../../ThirdParty/OpenSource/EASTL/unordered_set.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"
#import "../IRenderer.h"
#import "../IRay.h"
#import "../IResourceLoader.h"
#include "../../../Middleware_3/ParallelPrimitives/ParallelPrimitives.h"
#include "../../OS/Interfaces/ILog.h"
#include "../../OS/Interfaces/IMemory.h"

#if defined(ENABLE_RAYTRACING)

extern void util_barrier_required(Cmd* pCmd, const QueueType& encoderType);

extern void util_set_resources_compute(Cmd* pCmd);

static const char* pClassificationShader = R"(
#include <metal_stdlib>
using namespace metal;

struct PathCallStackHeader {
	short missShaderIndex; // -1 if we're not generating any rays, and < 0 if there's no miss shader.
	uchar nextFunctionIndex;
//	uchar rayContributionToHitGroupIndex : 4;
//	uchar multiplierForGeometryContributionToHitGroupIndex : 4;
	uchar shaderIndexFactors; // where the lower four bits are rayContributionToHitGroupIndex and the upper four are multiplierForGeometryContributionToHitGroupIndex.
};

struct PathCallStack {
	device PathCallStackHeader& header;
	device ushort* functions;
	uint maxCallStackDepth;
	
	PathCallStack(device PathCallStackHeader* headers, device ushort* functions, uint pathIndex, uint maxCallStackDepth) :
		header(headers[pathIndex]), functions(&functions[pathIndex * maxCallStackDepth]), maxCallStackDepth(maxCallStackDepth) {
	}
	
	void Initialize() thread {
		header.nextFunctionIndex = 0;
		ResetRayParams();
	}
	
	void ResetRayParams() thread {
		header.missShaderIndex = -1;
		header.shaderIndexFactors = 0;
	}
	
	uchar GetRayContributionToHitGroupIndex() thread const {
		return header.shaderIndexFactors & 0b1111;
	}
	
	void SetRayContributionToHitGroupIndex(uchar contribution) thread {
		header.shaderIndexFactors &= ~0b1111;
		header.shaderIndexFactors |= contribution & 0b1111;
	}
	
	uchar GetMultiplierForGeometryContributionToHitGroupIndex() thread const {
		return header.shaderIndexFactors >> 4;
	}
	
	void SetMultiplierForGeometryContributionToHitGroupIndex(uchar multiplier) thread {
		header.shaderIndexFactors &= ~(0b1111 << 4);
		header.shaderIndexFactors |= multiplier << 4;
	}

	ushort GetMissShaderIndex() const thread {
		return header.missShaderIndex;
	}

	void SetMissShaderIndex(ushort missShaderIndex) thread {
		header.missShaderIndex = (short)missShaderIndex;
	}
	
	void SetHitShaderOnly() thread {
		header.missShaderIndex = SHRT_MIN;
	}
	
	void PushFunction(ushort functionIndex) thread {
		if (header.nextFunctionIndex >= maxCallStackDepth) {
			return;
		}
		functions[header.nextFunctionIndex] = functionIndex;
		header.nextFunctionIndex += 1;
	}
	
	ushort PopFunction() thread {
		if (header.nextFunctionIndex == 0) {
			return ~0;
		}
		header.nextFunctionIndex -= 1;
		return functions[header.nextFunctionIndex];
	}
};

struct Intersection {
    float distance;
    unsigned primitiveIndex;
    unsigned instanceIndex;
    float2 coordinates;
};

struct ClassifyIntersectionsArguments {
    const device Intersection* intersections;
	device PathCallStackHeader* pathCallStackHeaders;
	device ushort* pathCallStackFunctions;
    device uint* pathShaders;
	uint maxCallStackDepth;
};

kernel void classifyIntersections(constant ClassifyIntersectionsArguments& arguments [[ buffer(0) ]],
							  const device uint& activePathCount [[ buffer(1) ]],
							  const device uint* instanceHitGroups [[ buffer(2) ]],
							  const device packed_short3* hitGroupShaders [[ buffer(3) ]], // Intersection, any hit, closest hit.
							  constant ushort* missShaderIndices [[ buffer(4) ]], // Intersection, any hit, closest hit.
							  const device uint* pathIndices [[ buffer(5) ]],
                              uint threadIndex [[ thread_position_in_grid ]]) {
    if (threadIndex >= activePathCount) {
        return;
    }

	uint pathIndex = pathIndices[threadIndex];

	PathCallStack callStack(arguments.pathCallStackHeaders, arguments.pathCallStackFunctions, pathIndex, arguments.maxCallStackDepth);

	short missShader = callStack.GetMissShaderIndex();
	if (missShader == -1) {
		// Miss shader of -1 means no ray generation was requested, and we should fall back to the parent caller.
		// Note that this means we need a dummy miss shader if the user requested a ray trace but didn't provide a miss
		// shader; this dummy shader would trigger no work.
		// Miss shader of < 0 means rays were requested but we don't have a miss shader.
	} else {
		const device Intersection& intersection = arguments.intersections[threadIndex];

		if (intersection.distance < 0) {
			if (missShader >= 0) {
				callStack.ResetRayParams();
				arguments.pathShaders[threadIndex] = (uint)missShaderIndices[missShader];
				return;
			}
		} else {
			uint rayContributionToHitGroupIndex = callStack.GetRayContributionToHitGroupIndex();
			uint geometryContributionToHitGroupIndex = callStack.GetMultiplierForGeometryContributionToHitGroupIndex();
			if (geometryContributionToHitGroupIndex > 0) {
				// TODO: multiply the geometry contribution to hit group index by the geometry index within the bottom level acceleration structure.
				// geometryContributionToHitGroupIndex *= ;
			}
			uint hitGroupIndex = geometryContributionToHitGroupIndex + instanceHitGroups[intersection.instanceIndex] + rayContributionToHitGroupIndex;

			short3 hgShaders = hitGroupShaders[hitGroupIndex];
			// Push the shaders onto the stack in reverse order.
			if (hgShaders.z >= 0) {
				// Closest hit.
				callStack.PushFunction(hgShaders.z);
			}
			if (hgShaders.y >= 0) {
				// Any hit.
				callStack.PushFunction(hgShaders.y);
			}
			if (hgShaders.x >= 0) {
				// Intersection.
				callStack.PushFunction(hgShaders.x);
			}
		}
	}
	
	callStack.ResetRayParams();

	if (callStack.header.nextFunctionIndex == 0) {
		// This path has finished all of its work.
		arguments.pathShaders[threadIndex] = UINT_MAX;
		return;
	}
	uint nextFunction = callStack.PopFunction();
	arguments.pathShaders[threadIndex] = nextFunction;
	return;
}
)";

#define MAX_BUFFER_BINDINGS 31

#define THREADS_PER_THREADGROUP 64

extern void mtl_createShaderReflection(Renderer* pRenderer, Shader* shader, const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, eastl::unordered_map<uint32_t, MTLVertexFormat>* vertexAttributeFormats, ShaderReflection* pOutReflection);
extern void add_texture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** pTexture, const bool isRT);

struct API_AVAILABLE(macos(10.14), ios(12.0)) AccelerationStructure
{
	MPSAccelerationStructureGroup* pSharedGroup;
	NSMutableArray<MPSTriangleAccelerationStructure*>* pBottomAS;
	MPSInstanceAccelerationStructure *pInstanceAccel;
	Buffer* mVertexPositionBuffer;
	Buffer* mIndexBuffer;
	Buffer* mMasks;
	Buffer* mHitGroupIndices;
	eastl::vector<uint32_t> mActiveHitGroups;
};

struct RaytracingShaderTable
{
	Pipeline* pPipeline;

	id<MTLComputePipelineState>		mRayGenPipeline;
	Buffer*                         mHitGroupShadersBuffer; // For each hit group index, packed_short3 of intersection, any hit, and closest hit shader indices.
	uint16_t*                       mMissShaderIndicesBuffer;
	uint32_t                        mMissShaderIndicesSize;
	
	uint32_t*						pMissShaderIndices; // Into RaytracingPipeline's mMissPipelines
	unsigned						mMissShaderCount;
	
	uint32_t*						pHitGroupIndices; // Into RaytracingPipeline's mHitGroup arrays
	unsigned						mHitGroupCount;
};

struct RaysDispatchUniformBuffer
{
	unsigned int width;
	unsigned int height;
	unsigned int blocksWide;
	unsigned int maxCallStackDepth;
};

//struct used in shaders. Here it is declared to use its sizeof() for rayStride
struct Ray {
	float3 origin;
	uint mask;
	float3 direction;
	float maxDistance;
};


struct HitShaderSettings
{
	uint32_t hitGroupID;
};

struct HitGroupShaders {
	int16_t intersection;
	int16_t anyHit;
	int16_t closestHit;
};

struct API_AVAILABLE(macos(10.14), ios(12.0)) RaytracingPipeline
{
	NSMutableArray<id <MTLComputePipelineState> >*  mMetalPipelines; // Element 0 is always the ray pipeline.
	HitGroupShaders*	pHitGroupShaders; // Into mMetalPipelines.
	HitGroupShaders*	pHitGroupShaderCounts; // How many intersection/any hit/closest hit shaders
	char**				mHitGroupNames;
	
	char**				mMissGroupNames;
	uint16_t*			pMissShaderIndices; // Into mMetalPipelines
	
	MPSRayIntersector* mIntersector;
	
	id <MTLBuffer> mRaysBuffer;
	id <MTLBuffer> mIntersectionBuffer;
	id <MTLBuffer> mSettingsBuffer;
	id <MTLBuffer> mPayloadBuffer;
	id <MTLBuffer> mPathCallStackBuffer;
	id <MTLBuffer> mClassificationArgumentsBuffer;
	id <MTLBuffer> mRaytracingArgumentsBuffer;
	
	Buffer*      pRayCountBuffer[2];
	Buffer*      pPathHitGroupsBuffer;
	Buffer*      pSortedPathHitGroupsBuffer;
	Buffer*      pPathIndicesBuffer;
	Buffer*      pSortedPathIndicesBuffer;
	Buffer*      pHitGroupsOffsetBuffer;
	Buffer*      pHitGroupIndirectArgumentsBuffer;
	
	uint32_t	mMaxRaysCount;
	uint32_t	mPayloadRecordSize;
	uint32_t	mMaxTraceRecursionDepth;
	uint32_t	mRayGenShaderCount;
	uint32_t	mHitGroupCount;
	uint32_t	mMissShaderCount;
};

//implemented in MetalRenderer.mm
extern void util_end_current_encoders(Cmd* pCmd, bool forceBarrier);

bool isRaytracingSupported(Renderer* pRenderer)
{
	return true;
}

API_AVAILABLE(macos(10.14), ios(12.0))
bool initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing)
{
	Raytracing* pRaytracing = tf_new(Raytracing);
	// Create a raytracer for our Metal device
	pRaytracing->pIntersector = [[MPSRayIntersector alloc] initWithDevice: pRenderer->pDevice];
	
	MPSRayOriginMinDistanceDirectionMaxDistance s;
	pRaytracing->pIntersector.rayDataType = MPSRayDataTypeOriginMaskDirectionMaxDistance;
	pRaytracing->pIntersector.rayStride = sizeof(Ray);
	pRaytracing->pIntersector.rayMaskOptions = MPSRayMaskOptionPrimitive;
	pRaytracing->pRenderer = pRenderer;
	
	pRaytracing->pParallelPrimitives = tf_new(ParallelPrimitives, pRenderer);
	
	NSString* classificationShaderSource = [NSString stringWithUTF8String:pClassificationShader];
	NSError* error = nil;
	id <MTLLibrary> classificationLibrary = [pRenderer->pDevice newLibraryWithSource:classificationShaderSource options:nil error:&error];
	id <MTLFunction> classificationFunction = [classificationLibrary newFunctionWithName:@"classifyIntersections"];
	pRaytracing->mClassificationPipeline = [pRenderer->pDevice newComputePipelineStateWithFunction:classificationFunction error:nil];
	pRaytracing->mClassificationArgumentEncoder = [classificationFunction newArgumentEncoderWithBufferIndex:0];
	
	*ppRaytracing = pRaytracing;
	return true;
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
void computeBuffersSize(const AccelerationStructureDescTop* pDesc, unsigned* pVbSize, unsigned* pIbSize, unsigned* pMasksSize)
{
	ASSERT(pDesc->mIndexType == INDEX_TYPE_UINT16 || pDesc->mIndexType == INDEX_TYPE_UINT32);
	unsigned indexSize = pDesc->mIndexType == INDEX_TYPE_UINT16 ? 2 : 4;
	unsigned vbSize = 0;
	unsigned ibSize = 0;
	unsigned mskSize = 0;
	{
		for (unsigned idesc = 0; idesc < pDesc->mBottomASDesc->mDescCount; ++idesc)
		{
			vbSize += pDesc->mBottomASDesc->pGeometryDescs[idesc].mVertexCount * sizeof(float3);
			ibSize += pDesc->mBottomASDesc->pGeometryDescs[idesc].mIndexCount * indexSize;
			
			if (pDesc->mBottomASDesc->pGeometryDescs[idesc].mIndexCount > 0)
				mskSize += (pDesc->mBottomASDesc->pGeometryDescs[idesc].mIndexCount / 3) * sizeof(uint32_t);
			else
				mskSize += (pDesc->mBottomASDesc->pGeometryDescs[idesc].mVertexCount / 3) * sizeof(uint32_t);
		}
	}
	if (pVbSize != NULL)
	{
		*pVbSize = vbSize;
	}
	if (pIbSize != NULL)
	{
		*pIbSize = ibSize;
	}
	if (pMasksSize != NULL)
	{
		*pMasksSize = mskSize;
	}
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
struct ASOffset{
	eastl::vector<unsigned> vbGeometriesOffsets;
	eastl::vector<unsigned> ibGeometriesOffsets;
	unsigned vbSize;
	unsigned ibSize;
	unsigned trianglesCount;
	unsigned vertexStride;
};

template<typename T1, typename T2>
void copyIndices(T1* dst, T2* src, unsigned count, unsigned baseOffset)
{
	for (unsigned i = 0; i < count; ++i)
		dst[i] = src[i] + baseOffset;
}

void createVertexAndIndexBuffers(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc,
								 Buffer** pVb, Buffer** pIb,
								 ASOffset* pOutOffset)
{
	unsigned vbSize = 0, ibSize = 0, mskSize = 0;
	computeBuffersSize(pDesc, &vbSize, &ibSize, &mskSize);
	
	BufferLoadDesc vbDesc = {};
	vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	vbDesc.mDesc.mSize = vbSize;
	vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	vbDesc.ppBuffer = pVb;
	addResource(&vbDesc, NULL);
	
	unsigned vbOffset = 0;
	unsigned ibOffset = 0;
	unsigned vertexOffset = 0;
	
	BufferUpdateDesc ibUpdateDesc = {};
	BufferUpdateDesc vbUpdateDesc = { *pVb };
	beginUpdateResource(&vbUpdateDesc);
	uint8_t* vbDstPtr = static_cast<uint8_t*>(vbUpdateDesc.pMappedData);
	uint8_t* ibDstPtr = NULL;
	if (ibSize > 0)
	{
		BufferLoadDesc ibDesc = {};
		ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		ibDesc.mDesc.mSize = ibSize;
		ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		ibDesc.ppBuffer = pIb;
		addResource(&ibDesc, NULL);
		
		ibUpdateDesc = { *pIb };
		beginUpdateResource(&ibUpdateDesc);
		ibDstPtr = static_cast<uint8_t*>(ibUpdateDesc.pMappedData);
	}
	
	{
		ASOffset& asoffset = *pOutOffset;
		asoffset.vbGeometriesOffsets.resize(pDesc->mBottomASDesc->mDescCount);
		asoffset.ibGeometriesOffsets.resize(pDesc->mBottomASDesc->mDescCount);
		asoffset.vbSize = 0;
		asoffset.ibSize = 0;
		asoffset.trianglesCount = 0;
		asoffset.vertexStride = sizeof(float3);
		for (unsigned j = 0; j < pDesc->mBottomASDesc->mDescCount; ++j)
		{
			asoffset.vbGeometriesOffsets[j] = vbOffset;
			asoffset.ibGeometriesOffsets[j] = ibOffset;
			
			// Copy vertex data into buffers
			memcpy(&vbDstPtr[vbOffset],
				   pDesc->mBottomASDesc->pGeometryDescs[j].pVertexArray,
				   pDesc->mBottomASDesc->pGeometryDescs[j].mVertexCount * sizeof(float3));
			vbOffset += pDesc->mBottomASDesc->pGeometryDescs[j].mVertexCount * sizeof(float3);
			asoffset.vbSize += pDesc->mBottomASDesc->pGeometryDescs[j].mVertexCount * sizeof(float3);
			
			if (ibDstPtr != NULL)
			{
				unsigned indexSize = (pDesc->mIndexType == INDEX_TYPE_UINT16 ? 2 : 4);
				if (pDesc->mBottomASDesc->pGeometryDescs[j].mIndexType == INDEX_TYPE_UINT16)
				{
					if (pDesc->mIndexType == INDEX_TYPE_UINT16)
						copyIndices<uint16_t, uint16_t>((uint16_t*)&ibDstPtr[ibOffset],
														pDesc->mBottomASDesc->pGeometryDescs[j].pIndices16,
														pDesc->mBottomASDesc->pGeometryDescs[j].mIndexCount,
														vertexOffset);
					else
						copyIndices<uint16_t, uint32_t>((uint16_t*)&ibDstPtr[ibOffset],
														pDesc->mBottomASDesc->pGeometryDescs[j].pIndices32,
														pDesc->mBottomASDesc->pGeometryDescs[j].mIndexCount,
														vertexOffset);
				}
				else if (pDesc->mBottomASDesc->pGeometryDescs[j].mIndexType == INDEX_TYPE_UINT32)
				{
					if (pDesc->mIndexType == INDEX_TYPE_UINT16)
						copyIndices<uint32_t, uint16_t>((uint32_t*)&ibDstPtr[ibOffset],
														pDesc->mBottomASDesc->pGeometryDescs[j].pIndices16,
														pDesc->mBottomASDesc->pGeometryDescs[j].mIndexCount,
														vertexOffset);
					else
						copyIndices<uint32_t, uint32_t>((uint32_t*)&ibDstPtr[ibOffset],
														pDesc->mBottomASDesc->pGeometryDescs[j].pIndices32,
														pDesc->mBottomASDesc->pGeometryDescs[j].mIndexCount,
														vertexOffset);
				}
				else
				{
					ASSERT(false && "New index type was introduced!?");
				}
				
				ibOffset += pDesc->mBottomASDesc->pGeometryDescs[j].mIndexCount * indexSize;
				asoffset.ibSize += pDesc->mBottomASDesc->pGeometryDescs[j].mIndexCount * indexSize;
				asoffset.trianglesCount += pDesc->mBottomASDesc->pGeometryDescs[j].mIndexCount / 3;
			} else
			{
				asoffset.trianglesCount += pDesc->mBottomASDesc->pGeometryDescs[j].mVertexCount / 3;
			}
			vertexOffset += pDesc->mBottomASDesc->pGeometryDescs[j].mVertexCount;
		}
	}
	
#if !TARGET_OS_IPHONE
	endUpdateResource(&vbUpdateDesc, NULL);
	if (ibSize > 0)
	{
		endUpdateResource(&ibUpdateDesc, NULL);
	}
#endif
}

void createInstanceIDBuffer(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, Buffer** pBuffer)
{
	unsigned bufferSize = pDesc->mInstancesDescCount * sizeof(uint32_t);
	BufferLoadDesc bufferDesc = {};
	bufferDesc.mDesc.mSize = bufferSize;
	bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	bufferDesc.ppBuffer = pBuffer;
	addResource(&bufferDesc, NULL);
	
	BufferUpdateDesc updateDesc = { *pBuffer };
	beginUpdateResource(&updateDesc);
	uint32_t *ptr = (uint32_t*)updateDesc.pMappedData;
	for (unsigned i = 0; i < pDesc->mInstancesDescCount; ++i)
	{
		ptr[i] = pDesc->pInstanceDescs[i].mInstanceID;
	}
	endUpdateResource(&updateDesc, NULL);
}

id <MTLBuffer> createInstancesIndicesBuffer(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc)
{
	eastl::vector<uint32_t> instancesIndices(pDesc->mInstancesDescCount);
	for (unsigned i = 0; i < pDesc->mInstancesDescCount; ++i)
		instancesIndices[i] = pDesc->pInstanceDescs[i].mAccelerationStructureIndex;
	
	ASSERT(pDesc->mInstancesDescCount > 0);
	unsigned instanceBufferLength = sizeof(uint32_t) * pDesc->mInstancesDescCount;
	
	MTLResourceOptions options = 0;
#if !TARGET_OS_IPHONE
	options = MTLResourceStorageModeManaged;
#else
	options = MTLResourceStorageModeShared;
#endif
	
	id <MTLBuffer> instanceBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:instanceBufferLength options:options];
	memcpy(instanceBuffer.contents, instancesIndices.data(), instanceBufferLength);
	
#if !TARGET_OS_IPHONE
	[instanceBuffer didModifyRange:NSMakeRange(0, instanceBufferLength)];
#endif
	return instanceBuffer;
}

id <MTLBuffer> createTransformationsBuffer(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc)
{
#if !TARGET_OS_IPHONE
	MTLResourceOptions options = MTLResourceStorageModeManaged;
#else
	MTLResourceOptions options = MTLResourceStorageModeShared;
#endif
	
	unsigned instanceBufferLength = sizeof(matrix_float4x4) * pDesc->mInstancesDescCount;
	id <MTLBuffer> transformations = [pRaytracing->pRenderer->pDevice newBufferWithLength:instanceBufferLength options:options];
	uint8_t* ptr = static_cast<uint8_t*>(transformations.contents);
	for (unsigned i = 0; i < pDesc->mInstancesDescCount; ++i)
	{
		float* tr = &pDesc->pInstanceDescs[i].mTransform[0];
		vector_float4 v1 = {tr[0], tr[4], tr[8], 0.0};
		vector_float4 v2 = {tr[1], tr[5], tr[9], 0.0};
		vector_float4 v3 = {tr[2], tr[6], tr[10], 0.0};
		vector_float4 v4 = {tr[3], tr[7], tr[11], 1.0};
		matrix_float4x4 m = {v1,v2,v3,v4};
		memcpy(&ptr[i * sizeof(matrix_float4x4)], &m, sizeof(matrix_float4x4));
	}
	
#if !TARGET_OS_IPHONE
	[transformations didModifyRange:NSMakeRange(0, instanceBufferLength)];
#endif
	return transformations;
}

uint32_t encodeInstanceMask(uint32_t mask, uint32_t hitGroupIndex, uint32_t instanceID)
{
	ASSERT(mask < 0x100); //in DXR limited to 8 bits
	ASSERT(hitGroupIndex < 0x10); //in DXR limited to 16
	ASSERT(instanceID < 0x10000);
	
	return (instanceID << 16) | (hitGroupIndex << 8) | (mask);
}

void createInstancesMaskBuffer(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, Buffer** pMask)
{
	unsigned instanceBufferLength = sizeof(uint32_t) * pDesc->mInstancesDescCount;
	
	BufferLoadDesc maskDesc = {};
	maskDesc.mDesc.mSize = instanceBufferLength;
	maskDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	maskDesc.ppBuffer = pMask;
	addResource(&maskDesc, NULL);
	
	BufferUpdateDesc maskUpdateDesc = { *pMask };
	beginUpdateResource(&maskUpdateDesc);
	uint32_t* ptr = static_cast<uint32_t*>(maskUpdateDesc.pMappedData);
	for (unsigned i = 0; i < pDesc->mInstancesDescCount; ++i)
	{
		ptr[i] = encodeInstanceMask(pDesc->pInstanceDescs[i].mInstanceMask,
									pDesc->pInstanceDescs[i].mInstanceContributionToHitGroupIndex,
									pDesc->pInstanceDescs[i].mInstanceID);
	}

	endUpdateResource(&maskUpdateDesc, NULL);
}

void createHitGroupIndicesBuffer (Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, Buffer** pBuffer)
{
	unsigned instanceBufferLength = sizeof(uint32_t) * pDesc->mInstancesDescCount;
	BufferLoadDesc bufferDesc = {};
	bufferDesc.mDesc.mSize = instanceBufferLength;
	bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	bufferDesc.ppBuffer = pBuffer;
	addResource(&bufferDesc, NULL);
	
	BufferUpdateDesc updateDesc = { *pBuffer };
	beginUpdateResource(&updateDesc);
	uint32_t* ptr = static_cast<uint32_t*>(updateDesc.pMappedData);
	for (unsigned i = 0; i < pDesc->mInstancesDescCount; ++i)
	{
		ptr[i] = pDesc->pInstanceDescs[i].mInstanceContributionToHitGroupIndex;
	}
	endUpdateResource(&updateDesc, NULL);
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
API_AVAILABLE(macos(10.14), ios(12.0))
void addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, AccelerationStructure** ppAccelerationStructure)
{
	ASSERT(pRaytracing);
	ASSERT(pRaytracing->pRenderer);
	ASSERT(pRaytracing->pIntersector);
	ASSERT(ppAccelerationStructure);
	
	AccelerationStructure* AS = (AccelerationStructure*)tf_malloc(sizeof(AccelerationStructure));
	memset(AS, 0, sizeof(*AS));
	tf_placement_new<eastl::vector<uint32_t> >(&AS->mActiveHitGroups);
	AS->pBottomAS = [[NSMutableArray alloc] init];
	
	//pDesc->mFlags. Just ignore this
	Buffer* _vertexPositionBuffer = NULL;
	Buffer* _indexBuffer = NULL;
	ASOffset offset = {};
	
	//copy vertices and indices to buffer
	createVertexAndIndexBuffers(pRaytracing, pDesc, &_vertexPositionBuffer, &_indexBuffer, &offset);
	
	MPSAccelerationStructureGroup* group = [[MPSAccelerationStructureGroup alloc] initWithDevice:pRaytracing->pRenderer->pDevice];
	AS->pSharedGroup = group;
	MPSInstanceAccelerationStructure* as = [[MPSInstanceAccelerationStructure alloc] initWithGroup:group];
	AS->pInstanceAccel = as;
	AS->mIndexBuffer = _indexBuffer;
	AS->mVertexPositionBuffer = _vertexPositionBuffer;
	createHitGroupIndicesBuffer(pRaytracing, pDesc, &AS->mHitGroupIndices);
	for (unsigned i = 0; i < pDesc->mInstancesDescCount; ++i)
	{
		uint32_t hitID = pDesc->pInstanceDescs[i].mInstanceContributionToHitGroupIndex;
		//find for vector is O(n) but this is done once. It is faster for iteration (than iteration over set) which will happen often
		auto it = eastl::find(AS->mActiveHitGroups.begin(), AS->mActiveHitGroups.end(), hitID);
		if (it == AS->mActiveHitGroups.end())
			AS->mActiveHitGroups.push_back(hitID);
	}
	{
		// Create an acceleration structure from our vertex position data
		MPSTriangleAccelerationStructure* _accelerationStructure = [[MPSTriangleAccelerationStructure alloc] initWithGroup:group];
		
		_accelerationStructure.vertexBuffer = _vertexPositionBuffer->mtlBuffer;
		_accelerationStructure.vertexBufferOffset = _vertexPositionBuffer->mOffset + offset.vbGeometriesOffsets[0];
		_accelerationStructure.vertexStride = offset.vertexStride;
		
		_accelerationStructure.indexBuffer = _indexBuffer->mtlBuffer;
		_accelerationStructure.indexBufferOffset = _indexBuffer->mOffset;
		_accelerationStructure.indexType = pDesc->mIndexType == INDEX_TYPE_UINT32 ? MPSDataTypeUInt32 : MPSDataTypeUInt16;
		_accelerationStructure.indexBufferOffset = offset.ibGeometriesOffsets[0];
		
		_accelerationStructure.maskBuffer = nil;
		_accelerationStructure.maskBufferOffset = 0;
		
		_accelerationStructure.triangleCount = offset.trianglesCount;
		
		[AS->pBottomAS addObject:_accelerationStructure];
	}
	AS->pInstanceAccel.accelerationStructures = AS->pBottomAS;
	AS->pInstanceAccel.instanceCount = pDesc->mInstancesDescCount;
	AS->pInstanceAccel.instanceBuffer = createInstancesIndicesBuffer(pRaytracing, pDesc);
	
	//generate buffer for instances transformations
	{
		AS->pInstanceAccel.transformBuffer = createTransformationsBuffer(pRaytracing, pDesc);
		AS->pInstanceAccel.transformType = MPSTransformTypeFloat4x4;
	}
	//generate instances ID buffer
	 createInstancesMaskBuffer(pRaytracing, pDesc, &AS->mMasks);
	AS->pInstanceAccel.maskBuffer = AS->mMasks->mtlBuffer;
	AS->pInstanceAccel.maskBufferOffset = AS->mMasks->mOffset;
	
	*ppAccelerationStructure = AS;
}

API_AVAILABLE(macos(10.14), ios(12.0))
void cmdBuildTopAS(Cmd* pCmd, Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
	ASSERT(pRaytracing);
	ASSERT(pAccelerationStructure);
	ASSERT(pAccelerationStructure->pInstanceAccel);
	[pAccelerationStructure->pInstanceAccel rebuild];
}

API_AVAILABLE(macos(10.14), ios(12.0))
void cmdBuildBottomAS(Cmd* pCmd, Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure, unsigned bottomASIndex)
{
	ASSERT(pRaytracing);
	ASSERT(pAccelerationStructure);
	ASSERT(bottomASIndex < pAccelerationStructure->pBottomAS.count);
	ASSERT(pAccelerationStructure->pBottomAS[bottomASIndex]);
	[pAccelerationStructure->pBottomAS[bottomASIndex] rebuild];
}

API_AVAILABLE(macos(10.14), ios(12.0))
void cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc)
{
	for (unsigned i = 0; i < pDesc->mBottomASIndicesCount; ++i)
	{
		AccelerationStructure* as = pDesc->ppAccelerationStructures[pDesc->pBottomASIndices[i]];
		cmdBuildBottomAS(pCmd, pRaytracing, as, pDesc->pBottomASIndices[i]);
	}
	
	for (unsigned i = 0; i < pDesc->mBottomASIndicesCount; ++i)
	{
		cmdBuildTopAS(pCmd, pRaytracing, pDesc->ppAccelerationStructures[i]);
	}
}

API_AVAILABLE(macos(10.14), ios(12.0))
void removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing)
{
	ASSERT(pRaytracing);
	pRaytracing->mClassificationPipeline = nil;
	pRaytracing->mClassificationArgumentEncoder = nil;
	tf_delete(pRaytracing->pParallelPrimitives);
	pRaytracing->pParallelPrimitives = NULL;
	pRaytracing->pIntersector = nil;
	pRaytracing->~Raytracing();
	memset(pRaytracing, 0, sizeof(*pRaytracing));
	tf_free(pRaytracing);
}

API_AVAILABLE(macos(10.14), ios(12.0))
void addSubFunctions(MTLComputePipelineDescriptor* computeDescriptor, id<MTLLibrary> library, NSMutableArray* pipelineStates) {
	NSString* functionName = computeDescriptor.computeFunction.name;
	for (uint32_t i = 0;; i += 1)
	{
		NSString *subFunctionName = [functionName stringByAppendingFormat:@"_%d", i];
		id<MTLFunction> computeFunction = [library newFunctionWithName:subFunctionName];
		if (!computeFunction) break;
		
		computeDescriptor.computeFunction = computeFunction;
		
		NSError *error = nil;
		id<MTLComputePipelineState> pipeline = [library.device
												  newComputePipelineStateWithDescriptor:computeDescriptor
												  options: 0
												  reflection:nil
												  error:&error];
		if (!pipeline)
		{
			LOGF(LogLevel::eERROR, "Failed to create compute pipeline state, error:\n%s", [[error localizedDescription] UTF8String]);
		}
		[pipelineStates addObject: pipeline];
	}
}

API_AVAILABLE(macos(10.14), ios(12.0))
void addRaytracingPipeline(const RaytracingPipelineDesc* pDesc, Pipeline** ppGenericPipeline)
{
	ASSERT(pDesc);
	ASSERT(ppGenericPipeline);
	
	Raytracing* pRaytracing = pDesc->pRaytracing;
	ASSERT(pRaytracing);
	
	Pipeline* pGenericPipeline =(Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
	ASSERT(pGenericPipeline);
	
	pGenericPipeline->pShader = pDesc->pRayGenShader;
	pGenericPipeline->mType = PIPELINE_TYPE_RAYTRACING;
	
	RaytracingPipeline* pPipeline = (RaytracingPipeline*)tf_calloc(1, sizeof(RaytracingPipeline));
	memset(pPipeline, 0, sizeof(*pPipeline));
	
	pGenericPipeline->pRaytracingPipeline = pPipeline;
	/******************************/
	// Create compute pipelines
	
	pPipeline->mMetalPipelines = [[NSMutableArray alloc] initWithCapacity:pDesc->mHitGroupCount + pDesc->mMissShaderCount + 1];
	
	MTLComputePipelineDescriptor *computeDescriptor = [[MTLComputePipelineDescriptor alloc] init];
	// Set to YES to allow compiler to make certain optimizations
	computeDescriptor.threadGroupSizeIsMultipleOfThreadExecutionWidth = YES;
	computeDescriptor.computeFunction = pDesc->pRayGenShader->mtlComputeShader;
	
	NSError *error = NULL;
	id<MTLComputePipelineState> rayGenPipeline = [pRaytracing->pRenderer->pDevice
												  newComputePipelineStateWithDescriptor:computeDescriptor
												  options:0
												  reflection:nil
												  error:&error];
	[pPipeline->mMetalPipelines addObject:rayGenPipeline];
	addSubFunctions(computeDescriptor, pDesc->pRayGenShader->mtlLibrary, pPipeline->mMetalPipelines);
	
	pPipeline->mRayGenShaderCount = (uint32_t)pPipeline->mMetalPipelines.count;
	
#if !TARGET_OS_IPHONE
	MTLResourceOptions options = MTLResourceStorageModeManaged;
#else
	MTLResourceOptions options = MTLResourceStorageModeShared;
#endif
	pPipeline->pHitGroupShaders = (HitGroupShaders*)tf_calloc(pDesc->mHitGroupCount, sizeof(HitGroupShaders));
	pPipeline->mHitGroupNames = (char**)tf_calloc(pDesc->mHitGroupCount, sizeof(char*));
	pPipeline->pHitGroupShaderCounts = (HitGroupShaders*)tf_calloc(pDesc->mHitGroupCount, sizeof(HitGroupShaders));
	
	memset(pPipeline->pHitGroupShaderCounts, 0, pDesc->mHitGroupCount * sizeof(HitGroupShaders));
	
	for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
	{
		const char* groupName = pDesc->pHitGroups[i].pHitGroupName;
		pPipeline->mHitGroupNames[i] = (char*)tf_calloc(strlen(groupName) + 1, sizeof(char));
		memcpy(pPipeline->mHitGroupNames[i], groupName, strlen(groupName) + 1);
	}
	
	for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
	{
		if (pDesc->pHitGroups[i].pIntersectionShader != NULL)
		{
			computeDescriptor.computeFunction = pDesc->pHitGroups[i].pIntersectionShader->mtlComputeShader;
			
			id <MTLComputePipelineState> pipeline = [pRaytracing->pRenderer->pDevice
													  newComputePipelineStateWithDescriptor:computeDescriptor
													  options: 0
													  reflection:nil
													  error:&error];
			if (!pipeline)
			{
				LOGF(LogLevel::eERROR, "Failed to create compute pipeline state, error:\n%s", [[error localizedDescription] UTF8String]);
			}
			pPipeline->pHitGroupShaders[i].intersection = (int16_t)pPipeline->mMetalPipelines.count;
			[pPipeline->mMetalPipelines addObject: pipeline];
			
			addSubFunctions(computeDescriptor, pDesc->pHitGroups[i].pIntersectionShader->mtlLibrary, pPipeline->mMetalPipelines);

			pPipeline->pHitGroupShaderCounts[i].intersection = (uint16_t)pPipeline->mMetalPipelines.count - (uint16_t)pPipeline->pHitGroupShaders[i].intersection;
		}
		else
		{
			pPipeline->pHitGroupShaders[i].intersection = -1;
		}
		
		if (pDesc->pHitGroups[i].pAnyHitShader != NULL)
		{
			computeDescriptor.computeFunction = pDesc->pHitGroups[i].pAnyHitShader->mtlComputeShader;
			
			id <MTLComputePipelineState> pipeline = [pRaytracing->pRenderer->pDevice
													  newComputePipelineStateWithDescriptor:computeDescriptor
													  options: 0
													  reflection:nil
													  error:&error];
			if (!pipeline)
			{
				LOGF(LogLevel::eERROR, "Failed to create compute pipeline state, error:\n%s", [[error localizedDescription] UTF8String]);
			}
			pPipeline->pHitGroupShaders[i].anyHit = (int16_t)pPipeline->mMetalPipelines.count;
			[pPipeline->mMetalPipelines addObject: pipeline];
			
			addSubFunctions(computeDescriptor, pDesc->pHitGroups[i].pAnyHitShader->mtlLibrary, pPipeline->mMetalPipelines);
			
			pPipeline->pHitGroupShaderCounts[i].anyHit = (uint16_t)pPipeline->mMetalPipelines.count - (uint16_t)pPipeline->pHitGroupShaders[i].anyHit;
		}
		else
		{
			pPipeline->pHitGroupShaders[i].anyHit = -1;
		}
		
		if (pDesc->pHitGroups[i].pClosestHitShader != NULL)
		{
			computeDescriptor.computeFunction = pDesc->pHitGroups[i].pClosestHitShader->mtlComputeShader;
			
			id <MTLComputePipelineState> pipeline = [pRaytracing->pRenderer->pDevice
													  newComputePipelineStateWithDescriptor:computeDescriptor
													  options: 0
													  reflection:nil
													  error:&error];
			if (!pipeline)
			{
				LOGF(LogLevel::eERROR, "Failed to create compute pipeline state, error:\n%s", [[error localizedDescription] UTF8String]);
			}
			pPipeline->pHitGroupShaders[i].closestHit = (int16_t)pPipeline->mMetalPipelines.count;
			[pPipeline->mMetalPipelines addObject: pipeline];
			
			addSubFunctions(computeDescriptor, pDesc->pHitGroups[i].pClosestHitShader->mtlLibrary, pPipeline->mMetalPipelines);
			
			pPipeline->pHitGroupShaderCounts[i].closestHit = (uint16_t)pPipeline->mMetalPipelines.count - (uint16_t)pPipeline->pHitGroupShaders[i].closestHit;
		}
		else
		{
			pPipeline->pHitGroupShaders[i].closestHit = -1;
		}
	}
	
	pPipeline->mMissGroupNames = (char**)tf_calloc(pDesc->mMissShaderCount, sizeof(char*));
	pPipeline->pMissShaderIndices = (uint16_t*)tf_calloc(pDesc->mMissShaderCount + 1, sizeof(uint16_t));
	
	for (uint32_t i = 0; i < pDesc->mMissShaderCount; ++i)
	{
		pPipeline->pMissShaderIndices[i] = (uint16_t)pPipeline->mMetalPipelines.count;
		
		MTLComputePipelineDescriptor *computeDescriptor = [[MTLComputePipelineDescriptor alloc] init];
		// Set to YES to allow compiler to make certain optimizations
		computeDescriptor.threadGroupSizeIsMultipleOfThreadExecutionWidth = YES;
		computeDescriptor.computeFunction = pDesc->ppMissShaders[i]->mtlComputeShader;
		
		id <MTLComputePipelineState> _pipeline = [pRaytracing->pRenderer->pDevice
												  newComputePipelineStateWithDescriptor:computeDescriptor
												  options: 0
												  reflection:nil
												  error:&error];
		if (!_pipeline)
		{
			LOGF(LogLevel::eERROR, "Failed to create compute pipeline state, error:\n%s", [[error localizedDescription] UTF8String]);
		}
		[pPipeline->mMetalPipelines addObject: _pipeline];
		
		addSubFunctions(computeDescriptor, pDesc->ppMissShaders[i]->mtlLibrary, pPipeline->mMetalPipelines);
		
		const char* groupName = pDesc->ppMissShaders[i]->pEntryNames[0];
		pPipeline->mMissGroupNames[i] = (char*)tf_calloc(strlen(groupName) + 1, sizeof(char));
		memcpy(pPipeline->mMissGroupNames[i], groupName, strlen(groupName) + 1);
	}
	pPipeline->pMissShaderIndices[pDesc->mMissShaderCount] = (uint16_t)pPipeline->mMetalPipelines.count;
	
	/******************************/
	// Create a raytracer for our Metal device
	pPipeline->mIntersector = [[MPSRayIntersector alloc] initWithDevice:pRaytracing->pRenderer->pDevice];
	
	pPipeline->mIntersector.rayDataType     = MPSRayDataTypeOriginMaskDirectionMaxDistance;
	pPipeline->mIntersector.rayStride       = sizeof(Ray);
	pPipeline->mIntersector.rayMaskOptions  = MPSRayMaskOptionInstance;
	
	//MPSIntersectionDistancePrimitiveIndexInstanceIndexCoordinates
	pPipeline->mIntersector.intersectionDataType = MPSIntersectionDataTypeDistancePrimitiveIndexInstanceIndexCoordinates;
	pPipeline->mMaxRaysCount = pDesc->mMaxRaysCount;
	pPipeline->mPayloadRecordSize = pDesc->mPayloadSize;
	pPipeline->mMaxTraceRecursionDepth = pDesc->mMaxTraceRecursionDepth;
	pPipeline->mHitGroupCount = pDesc->mHitGroupCount;
	pPipeline->mMissShaderCount = pDesc->mMissShaderCount;
	
	//Create rays buffer for RayGen shader
	NSUInteger raysBufferSize = pPipeline->mIntersector.rayStride * pDesc->mMaxRaysCount;
	pPipeline->mRaysBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:raysBufferSize options:MTLResourceStorageModePrivate];
	
	NSUInteger payloadBufferSize = pDesc->mPayloadSize * pDesc->mMaxRaysCount;
	pPipeline->mPayloadBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:payloadBufferSize options:MTLResourceStorageModePrivate];
	
	//Create buffer for settings (width, height,...)
	static const size_t alignedUniformsSize = (sizeof(RaysDispatchUniformBuffer) + 255) & ~255;
	NSUInteger uniformBufferSize = alignedUniformsSize; //Rustam: do multiple sized buffer for "Ring-buffer" approach
	pPipeline->mSettingsBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:uniformBufferSize options:options];
	
	//Create intersections buffer for initial intersection test
	NSUInteger intersectionsBufferSize = sizeof(MPSIntersectionDistancePrimitiveIndexInstanceIndexCoordinates) * pDesc->mMaxRaysCount;
	pPipeline->mIntersectionBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:intersectionsBufferSize
																				  options:MTLResourceStorageModePrivate];
	
	BufferLoadDesc loadDesc = {};
	loadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
	loadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	loadDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
	loadDesc.pData = NULL;
	
	loadDesc.mDesc.mSize = 4 * sizeof(uint32_t);
	for (uint32_t i = 0; i < 2; i += 1) {
		loadDesc.ppBuffer = &pPipeline->pRayCountBuffer[i];
		addResource(&loadDesc, NULL);
	}
	
	loadDesc.mDesc.mSize = pPipeline->mMaxRaysCount * sizeof(uint32_t);
	loadDesc.ppBuffer = &pPipeline->pPathHitGroupsBuffer;
	addResource(&loadDesc, NULL);
	
	loadDesc.ppBuffer = &pPipeline->pSortedPathHitGroupsBuffer;
	addResource(&loadDesc, NULL);
	
	loadDesc.ppBuffer = &pPipeline->pPathIndicesBuffer;
	addResource(&loadDesc, NULL);
	
	loadDesc.ppBuffer = &pPipeline->pSortedPathIndicesBuffer;
	addResource(&loadDesc, NULL);
	
	loadDesc.mDesc.mSize = pPipeline->mMetalPipelines.count * sizeof(uint32_t);
	loadDesc.ppBuffer = &pPipeline->pHitGroupsOffsetBuffer;
	addResource(&loadDesc, NULL);
	
	loadDesc.mDesc.mSize = pPipeline->mMetalPipelines.count * 8 * sizeof(uint32_t);
	loadDesc.ppBuffer = &pPipeline->pHitGroupIndirectArgumentsBuffer;
	addResource(&loadDesc, NULL);
	
	NSInteger pathCallStackHeadersLength = (sizeof(int16_t) + 2 * sizeof(uint8_t)) * pPipeline->mMaxRaysCount;
	NSInteger pathCallStackFunctionsLength = pPipeline->mMaxTraceRecursionDepth * pPipeline->mMaxRaysCount * sizeof(int16_t);
	pPipeline->mPathCallStackBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:pathCallStackHeadersLength + pathCallStackFunctionsLength options:MTLResourceStorageModePrivate];
	
	id <MTLArgumentEncoder> classificationArgumentEncoder = pRaytracing->mClassificationArgumentEncoder;
	pPipeline->mClassificationArgumentsBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:classificationArgumentEncoder.encodedLength options:MTLResourceStorageModeShared];

	[classificationArgumentEncoder setArgumentBuffer:pPipeline->mClassificationArgumentsBuffer offset:0];
	[classificationArgumentEncoder setBuffer:pPipeline->mIntersectionBuffer offset:0 atIndex:0];
	[classificationArgumentEncoder setBuffer:pPipeline->mPathCallStackBuffer offset:0 atIndex:1];
	[classificationArgumentEncoder setBuffer:pPipeline->mPathCallStackBuffer offset:4 * pDesc->mMaxRaysCount atIndex:2];
	[classificationArgumentEncoder setBuffer:pPipeline->pPathHitGroupsBuffer->mtlBuffer offset:0 atIndex:3];
	memcpy([classificationArgumentEncoder constantDataAtIndex:4], &pPipeline->mMaxTraceRecursionDepth, sizeof(uint32_t));
	
	id <MTLArgumentEncoder> raytracingArgumentEncoder = [pDesc->pRayGenShader->mtlComputeShader newArgumentEncoderWithBufferIndex:DESCRIPTOR_UPDATE_FREQ_COUNT];
	pPipeline->mRaytracingArgumentsBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:raytracingArgumentEncoder.encodedLength options:MTLResourceStorageModeShared];
	[raytracingArgumentEncoder setArgumentBuffer:pPipeline->mRaytracingArgumentsBuffer offset:0];
	[raytracingArgumentEncoder setBuffer:pPipeline->mSettingsBuffer offset:0 atIndex:0];
	[raytracingArgumentEncoder setBuffer:pPipeline->mRaysBuffer offset:0 atIndex:1];
	[raytracingArgumentEncoder setBuffer:pPipeline->mIntersectionBuffer offset:0 atIndex:2];
	[raytracingArgumentEncoder setBuffer:pPipeline->mPathCallStackBuffer offset:0 atIndex:3];
	[raytracingArgumentEncoder setBuffer:pPipeline->mPathCallStackBuffer offset:pPipeline->mMaxRaysCount * 4 atIndex:4];
	[raytracingArgumentEncoder setBuffer:pPipeline->mPayloadBuffer offset:0 atIndex:5];
	
	*ppGenericPipeline = pGenericPipeline;
}

API_AVAILABLE(macos(10.14), ios(12.0))
void removeRaytracingPipeline(RaytracingPipeline* pPipeline)
{
	ASSERT(pPipeline);
	
	pPipeline->mMetalPipelines = nil;
	tf_free(pPipeline->pHitGroupShaders);
	tf_free(pPipeline->pHitGroupShaderCounts);
	tf_free(pPipeline->pMissShaderIndices);
	
	for (uint32_t i = 0; i < pPipeline->mHitGroupCount; ++i)
		tf_free(pPipeline->mHitGroupNames[i]);
	tf_free(pPipeline->mHitGroupNames);
	
	for (uint32_t i = 0; i < pPipeline->mMissShaderCount; ++i)
		tf_free(pPipeline->mMissGroupNames[i]);
	tf_free(pPipeline->mMissGroupNames);
	
	pPipeline->mIntersector = nil;
	pPipeline->mRaysBuffer = nil;
	pPipeline->mSettingsBuffer = nil;
	pPipeline->mPayloadBuffer = nil;
	pPipeline->mPathCallStackBuffer = nil;
	pPipeline->mClassificationArgumentsBuffer = nil;
	pPipeline->mRaytracingArgumentsBuffer = nil;
	
	removeResource(pPipeline->pRayCountBuffer[0]);
	removeResource(pPipeline->pRayCountBuffer[1]);
	removeResource(pPipeline->pPathHitGroupsBuffer);
	removeResource(pPipeline->pSortedPathHitGroupsBuffer);
	removeResource(pPipeline->pPathIndicesBuffer);
	removeResource(pPipeline->pSortedPathIndicesBuffer);
	removeResource(pPipeline->pHitGroupsOffsetBuffer);
	removeResource(pPipeline->pHitGroupIndirectArgumentsBuffer);
	
//	pPipeline->~RaytracingPipeline();
//	memset(pPipeline, 0, sizeof(*pPipeline));
	
	tf_free(pPipeline);
}

API_AVAILABLE(macos(10.14), ios(12.0))
void removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
	pAccelerationStructure->pSharedGroup = nil;
	pAccelerationStructure->pBottomAS = nil;
	pAccelerationStructure->pInstanceAccel = nil;
	removeResource(pAccelerationStructure->mVertexPositionBuffer);
	if (pAccelerationStructure->mIndexBuffer)
	{
		removeResource(pAccelerationStructure->mIndexBuffer);
	}
	removeResource(pAccelerationStructure->mMasks);
	removeResource(pAccelerationStructure->mHitGroupIndices);
	
	pAccelerationStructure->~AccelerationStructure();
	tf_free(pAccelerationStructure);
}

API_AVAILABLE(macos(10.14), ios(12.0))
void addRaytracingShaderTable(Raytracing* pRaytracing, const RaytracingShaderTableDesc* pDesc, RaytracingShaderTable** ppTable)
{
	ASSERT(pRaytracing);
	ASSERT(pDesc);
	ASSERT(ppTable);
	
	RaytracingShaderTable* table = (RaytracingShaderTable*)tf_calloc(1, sizeof(RaytracingShaderTable));
	memset(table, 0, sizeof(RaytracingShaderTable));
	
	table->pPipeline = pDesc->pPipeline;
	/***************************************************************/
	/*Setup shaders settings*/
	/***************************************************************/
	RaytracingPipeline* pPipeline = pDesc->pPipeline->pRaytracingPipeline;
	
	// FIXME: is it even valid to specify a different ray gen for the shader table than was used for the RaytracingPipeline?
	table->mRayGenPipeline = pPipeline->mMetalPipelines[0];
	
	table->pHitGroupIndices = (uint32_t*)tf_calloc(pDesc->mHitGroupCount, sizeof(uint32_t));
	table->mHitGroupCount = pDesc->mHitGroupCount;
	for (uint32_t i = 0; i < pDesc->mHitGroupCount; i += 1)
	{
		const char* name = pDesc->pHitGroups[i];
		table->pHitGroupIndices[i] = ~0;
		
		for (uint32_t j = 0; j < pPipeline->mHitGroupCount; j += 1)
		{
			if (strcmp(pPipeline->mHitGroupNames[j], name) == 0)
			{
				table->pHitGroupIndices[i] = j;
				break;
			}
		}
	}
	
	BufferLoadDesc hitGroupDesc = {};
	hitGroupDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	hitGroupDesc.mDesc.mSize = sizeof(HitGroupShaders) * pDesc->mHitGroupCount;
	hitGroupDesc.ppBuffer = &table->mHitGroupShadersBuffer;
	addResource(&hitGroupDesc, NULL);

	BufferUpdateDesc hitGroupUpdateDesc = { table->mHitGroupShadersBuffer };
	beginUpdateResource(&hitGroupUpdateDesc);
	HitGroupShaders* hitGroupShaders = (HitGroupShaders*)(hitGroupUpdateDesc.pMappedData);

	for (uint32_t i = 0; i < pDesc->mHitGroupCount; i += 1)
	{
		hitGroupShaders[i] = pPipeline->pHitGroupShaders[table->pHitGroupIndices[i]];
	}
	
	endUpdateResource(&hitGroupUpdateDesc, NULL);
	
	table->pMissShaderIndices = (uint32_t*)tf_calloc(pDesc->mMissShaderCount, sizeof(uint32_t));
	table->mMissShaderCount = pDesc->mMissShaderCount;
	for (uint32_t i = 0; i < pDesc->mMissShaderCount; i += 1)
	{
		const char* name = pDesc->pMissShaders[i];
		table->pMissShaderIndices[i] = ~0;
		
		for (uint32_t j = 0; j < pPipeline->mMissShaderCount; j += 1)
		{
			if (strcmp(pPipeline->mMissGroupNames[j], name) == 0)
			{
				table->pMissShaderIndices[i] = j;
				break;
			}
		}
	}

	table->mMissShaderIndicesSize = sizeof(uint16_t) * pDesc->mMissShaderCount;
	if (table->mMissShaderIndicesSize)
	{
		table->mMissShaderIndicesBuffer = (uint16_t*)tf_malloc(sizeof(uint16_t) * pDesc->mMissShaderCount);
		for (uint32_t i = 0; i < pDesc->mMissShaderCount; i += 1)
		{
			table->mMissShaderIndicesBuffer[i] = pPipeline->pMissShaderIndices[table->pMissShaderIndices[i]];
		}
	}
	
	*ppTable = table;
}

API_AVAILABLE(macos(10.14), ios(12.0))
void removeRaytracingShaderTable(Raytracing* pRaytracing, RaytracingShaderTable* pTable)
{
	ASSERT(pTable);
	
	pTable->mRayGenPipeline = nil;
	removeResource(pTable->mHitGroupShadersBuffer);
	tf_free(pTable->mMissShaderIndicesBuffer);
	
	tf_free(pTable->pMissShaderIndices);
	tf_free(pTable->pHitGroupIndices);
	
	pTable->~RaytracingShaderTable();
	memset(pTable, 0, sizeof(*pTable));
	tf_free(pTable);
}

API_AVAILABLE(macos(10.14), ios(12.0))
void dispatchShader(id<MTLComputeCommandEncoder> computeEncoder, RaytracingPipeline* pPipeline, uint16_t shaderIndex, eastl::unordered_set<uint16_t>& dispatchedShaders)
{
	if (dispatchedShaders.find(shaderIndex) != dispatchedShaders.end())
		return; // Don't dispatch the same shader multiple times per iteration.
	
	NSInteger indirectBufferOffset = shaderIndex * 8 * sizeof(uint32_t);

	[computeEncoder setBuffer:pPipeline->pHitGroupIndirectArgumentsBuffer->mtlBuffer offset:indirectBufferOffset atIndex:DESCRIPTOR_UPDATE_FREQ_COUNT + 2];
	[computeEncoder setBytes:&shaderIndex length:sizeof(shaderIndex) atIndex:DESCRIPTOR_UPDATE_FREQ_COUNT + 3];
	
	[computeEncoder setComputePipelineState:pPipeline->mMetalPipelines[shaderIndex]];
	
	[computeEncoder dispatchThreadgroupsWithIndirectBuffer:pPipeline->pHitGroupIndirectArgumentsBuffer->mtlBuffer indirectBufferOffset:indirectBufferOffset + 2 * sizeof(uint32_t) threadsPerThreadgroup:MTLSizeMake(THREADS_PER_THREADGROUP, 1, 1)];
	
	dispatchedShaders.insert(shaderIndex);
}

API_AVAILABLE(macos(10.14), ios(12.0))
void invokeShaders(Cmd* pCmd, Raytracing* pRaytracing,
				   const RaytracingDispatchDesc* pDesc,
				   id <MTLBuffer> payloadBuffer,
				   MTLSize threadgroups)
{
	RaytracingShaderTable* pShaderTable = pDesc->pShaderTable;
	RaytracingPipeline* pPipeline = pShaderTable->pPipeline->pRaytracingPipeline;
	
	ASSERT(pPipeline->pHitGroupIndirectArgumentsBuffer->mtlBuffer.length >= pPipeline->mMetalPipelines.count * 8 * sizeof(uint32_t));
	
	eastl::unordered_set<uint16_t> dispatchedShaders;
	
	for (uint32_t i = 0; i <= pPipeline->mMaxTraceRecursionDepth; i += 1)
	{
		[pCmd->mtlComputeEncoder updateFence:pCmd->pQueue->mtlQueueFence];
		[pCmd->mtlComputeEncoder endEncoding];
		pCmd->mtlComputeEncoder = nil;
		
		[pCmd->mtlCommandBuffer pushDebugGroup:[NSString stringWithFormat:@"Raytracing batch %d", i]];
		
		// First, intersect rays with the scene.
		RaytracingPipeline* pPipeline = pShaderTable->pPipeline->pRaytracingPipeline;
		[pPipeline->mIntersector encodeIntersectionToCommandBuffer:pCmd->mtlCommandBuffer      // Command buffer to encode into
												  intersectionType:MPSIntersectionTypeNearest  // Intersection test type //Rustam: Get this from RayFlags from shader
														 rayBuffer:pPipeline->mRaysBuffer   // Ray buffer
												   rayBufferOffset:0                           // Offset into ray buffer
												intersectionBuffer:pPipeline->mIntersectionBuffer // Intersection buffer (destination)
										  intersectionBufferOffset:0                           // Offset into intersection buffer
													rayCountBuffer:pPipeline->pRayCountBuffer[0]->mtlBuffer  // Number of rays
											   rayCountBufferOffset:0
											 accelerationStructure:pDesc->pTopLevelAccelerationStructure->pInstanceAccel];    // Acceleration structure
		

		id <MTLComputeCommandEncoder> computeEncoder = [pCmd->mtlCommandBuffer computeCommandEncoderWithDispatchType:MTLDispatchTypeConcurrent];
		pCmd->mtlComputeEncoder = computeEncoder;
		[computeEncoder waitForFence:pCmd->pQueue->mtlQueueFence];
		
		{
			// Declare all the resources we'll use.
			util_set_resources_compute(pCmd);
			
			// In the raytracing arguments buffer
			[computeEncoder useResource:pPipeline->mSettingsBuffer usage:MTLResourceUsageRead];
			[computeEncoder useResource:pPipeline->mRaysBuffer usage:MTLResourceUsageRead | MTLResourceUsageWrite];
			[computeEncoder useResource:pPipeline->mIntersectionBuffer usage:MTLResourceUsageRead | MTLResourceUsageWrite];
			[computeEncoder useResource:pPipeline->pPathIndicesBuffer->mtlBuffer usage:MTLResourceUsageRead | MTLResourceUsageWrite];
			[computeEncoder useResource:pPipeline->pSortedPathIndicesBuffer->mtlBuffer usage:MTLResourceUsageRead | MTLResourceUsageWrite];
			[computeEncoder useResource:pPipeline->mPathCallStackBuffer usage:MTLResourceUsageRead | MTLResourceUsageWrite];
			[computeEncoder useResource:pPipeline->mPayloadBuffer usage:MTLResourceUsageRead | MTLResourceUsageWrite];
			
			// In the classification arguments buffer
			[computeEncoder useResource:pPipeline->pPathHitGroupsBuffer->mtlBuffer usage:MTLResourceUsageRead | MTLResourceUsageWrite];
			
			
			[computeEncoder useResource:pPipeline->pRayCountBuffer[0]->mtlBuffer usage:MTLResourceUsageRead | MTLResourceUsageWrite];
			[computeEncoder useResource:pPipeline->pRayCountBuffer[1]->mtlBuffer usage:MTLResourceUsageRead | MTLResourceUsageWrite];
		}
		
		// Next, classify each ray's intersection.
		[computeEncoder setComputePipelineState:pRaytracing->mClassificationPipeline];
		[computeEncoder setBuffer:pPipeline->mClassificationArgumentsBuffer offset:0 atIndex:0];
		[computeEncoder setBuffer:pPipeline->pRayCountBuffer[0]->mtlBuffer offset:0 atIndex:1];
		[computeEncoder setBuffer:pDesc->pTopLevelAccelerationStructure->mHitGroupIndices->mtlBuffer offset:pDesc->pTopLevelAccelerationStructure->mHitGroupIndices->mOffset atIndex:2];
		[computeEncoder setBuffer:pShaderTable->mHitGroupShadersBuffer->mtlBuffer offset:pShaderTable->mHitGroupShadersBuffer->mOffset atIndex:3];
		[computeEncoder setBytes:pShaderTable->mMissShaderIndicesBuffer length:pShaderTable->mMissShaderIndicesSize atIndex:4];
		[computeEncoder setBuffer:pPipeline->pPathIndicesBuffer->mtlBuffer offset:0 atIndex:5];
		[computeEncoder dispatchThreadgroupsWithIndirectBuffer:pPipeline->pRayCountBuffer[0]->mtlBuffer indirectBufferOffset:sizeof(uint32_t) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
		
		[computeEncoder memoryBarrierWithScope:MTLBarrierScopeBuffers];
		
		IndirectCountBuffer indirectCount(pDesc->mWidth * pDesc->mHeight, pPipeline->pRayCountBuffer[0]);
		uint32_t totalShaderCount = (uint32_t)pPipeline->mMetalPipelines.count;
		
		// Sort the intersections.
		pRaytracing->pParallelPrimitives->sortRadixKeysValues(pCmd, pPipeline->pPathHitGroupsBuffer, pPipeline->pPathIndicesBuffer, pPipeline->pSortedPathHitGroupsBuffer, pPipeline->pSortedPathIndicesBuffer, indirectCount, totalShaderCount);
		
		// Generate an offset buffer.
		pRaytracing->pParallelPrimitives->generateOffsetBuffer(pCmd, pPipeline->pSortedPathHitGroupsBuffer, pPipeline->pHitGroupsOffsetBuffer, pPipeline->pRayCountBuffer[1], indirectCount, totalShaderCount, THREADS_PER_THREADGROUP);
		
		// Fill the indirect arguments buffer for each hit group from the offset buffer.
		pRaytracing->pParallelPrimitives->generateIndirectArgumentsFromOffsetBuffer(pCmd, pPipeline->pHitGroupsOffsetBuffer, pPipeline->pRayCountBuffer[1], pPipeline->pHitGroupIndirectArgumentsBuffer, totalShaderCount, THREADS_PER_THREADGROUP);
	
		[computeEncoder memoryBarrierWithScope:MTLBarrierScopeBuffers];
		
		Buffer* newRayCountBuffer = pPipeline->pRayCountBuffer[1];
		pPipeline->pRayCountBuffer[1] = pPipeline->pRayCountBuffer[0];
		pPipeline->pRayCountBuffer[0] = newRayCountBuffer;
		
		Buffer* newPathIndicesBuffer = pPipeline->pSortedPathIndicesBuffer;
		pPipeline->pSortedPathIndicesBuffer = pPipeline->pPathIndicesBuffer;
		pPipeline->pPathIndicesBuffer = newPathIndicesBuffer;
		
		// Restore bindings
		for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
		{
			if (pDesc->pSets[i])
				cmdBindDescriptorSet(pCmd, pDesc->pIndexes[i], pDesc->pSets[i]);
		}
		
		// Bind the raytracing arguments
		[computeEncoder setBuffer:pPipeline->mRaytracingArgumentsBuffer offset:0 atIndex:DESCRIPTOR_UPDATE_FREQ_COUNT + 0];
		[computeEncoder setBuffer:pPipeline->pPathIndicesBuffer->mtlBuffer offset:0 atIndex:DESCRIPTOR_UPDATE_FREQ_COUNT + 1];
		
		// Iterate through subshaders of the ray generation shader (which always starts at index 0).
		for (uint32_t j = 1; j < pPipeline->mRayGenShaderCount; j += 1)
		{
			dispatchShader(computeEncoder, pPipeline, (uint16_t)j, dispatchedShaders);
		}
		
		// Iterate through the hit group shaders.
		for (uint32_t j = 0; j < pShaderTable->mHitGroupCount; j += 1)
		{
			HitGroupShaders shaders = pPipeline->pHitGroupShaders[pShaderTable->pHitGroupIndices[j]];
			HitGroupShaders shaderCounts = pPipeline->pHitGroupShaderCounts[pShaderTable->pHitGroupIndices[j]];
			for (int16_t k = shaders.intersection; k < shaders.intersection + shaderCounts.intersection; k += 1)
			{
				dispatchShader(computeEncoder, pPipeline, (uint16_t)k, dispatchedShaders);
			}
			for (int16_t k = shaders.anyHit; k < shaders.anyHit + shaderCounts.anyHit; k += 1)
			{
				dispatchShader(computeEncoder, pPipeline, (uint16_t)k, dispatchedShaders);
			}
			for (int16_t k = shaders.closestHit; k < shaders.closestHit + shaderCounts.closestHit; k += 1)
			{
				dispatchShader(computeEncoder, pPipeline, (uint16_t)k, dispatchedShaders);
			}
		}
		
		// Iterate through the miss shaders
		for (uint32_t j = 0; j < pShaderTable->mMissShaderCount; j += 1)
		{
			uint16_t firstShader = pPipeline->pMissShaderIndices[pShaderTable->pMissShaderIndices[j]];
			uint16_t upperBoundShader = pPipeline->pMissShaderIndices[pShaderTable->pMissShaderIndices[j] + 1]; // pPipeline->pMissShaderIndices contains the count at the end, so this doesn't overrun the array bounds.
			for (uint16_t k = firstShader; k < upperBoundShader; k += 1)
			{
				dispatchShader(computeEncoder, pPipeline, k, dispatchedShaders);
			}
		}

		dispatchedShaders.clear();
		
		[pCmd->mtlComputeEncoder memoryBarrierWithScope:MTLBarrierScopeBuffers];
		[pCmd->mtlCommandBuffer popDebugGroup];
	}
	
	util_end_current_encoders(pCmd, true);
	
}

API_AVAILABLE(macos(10.14), ios(12.0))
void cmdDispatchRays(Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc)
{
	util_barrier_required(pCmd, QUEUE_TYPE_GRAPHICS);
	
	NSUInteger width = (NSUInteger)pDesc->mWidth;
	NSUInteger height = (NSUInteger)pDesc->mHeight;
	
	RaytracingPipeline* pRaytracingPipeline = pDesc->pShaderTable->pPipeline->pRaytracingPipeline;
	/*setup settings values in buffer*/
	//Rustam: implement ring-buffer
	RaysDispatchUniformBuffer *uniforms = (RaysDispatchUniformBuffer *)pRaytracingPipeline->mSettingsBuffer.contents;
	uniforms->width = (unsigned int)width;
	uniforms->height = (unsigned int)height;
	uniforms->blocksWide = (unsigned int)(width + 15) / 16;
	uniforms->maxCallStackDepth = pRaytracingPipeline->mMaxTraceRecursionDepth;
	
#if !TARGET_OS_IPHONE
	[pRaytracingPipeline->mSettingsBuffer didModifyRange:NSMakeRange(0, pRaytracingPipeline->mSettingsBuffer.length)];
#endif
	
	// We will launch a rectangular grid of threads on the GPU to generate the rays. Threads are launched in
	// groups called "threadgroups". We need to align the number of threads to be a multiple of the threadgroup
	// size. We indicated when compiling the pipeline that the threadgroup size would be a multiple of the thread
	// execution width (SIMD group size) which is typically 32 or 64 so 8x8 is a safe threadgroup size which
	// should be small to be supported on most devices. A more advanced application would choose the threadgroup
	// size dynamically.
	MTLSize threadsPerThreadgroup = MTLSizeMake(8, 8, 1);
	ASSERT(threadsPerThreadgroup.width * threadsPerThreadgroup.height == THREADS_PER_THREADGROUP);
	MTLSize threadgroups = MTLSizeMake((width  + threadsPerThreadgroup.width  - 1) / threadsPerThreadgroup.width,
									   (height + threadsPerThreadgroup.height - 1) / threadsPerThreadgroup.height,
									   1);
	
	// First, we will generate rays on the GPU. We create a compute command encoder which will be used to add
	// commands to the command buffer.
	id <MTLComputeCommandEncoder> computeEncoder = pCmd->mtlComputeEncoder;
	
	ASSERT(computeEncoder != nil);
	
	/*********************************************************************************/
	//Now we work with initial RayGen shader
	/*********************************************************************************/
	[computeEncoder pushDebugGroup:@"Ray Pipeline"];
	
	// Restore bindings
	for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
	{
		if (pDesc->pSets[i])
			cmdBindDescriptorSet(pCmd, pDesc->pIndexes[i], pDesc->pSets[i]);
	}
	
	// Bind buffers needed by the compute pipeline
	
	[computeEncoder useResource:pRaytracingPipeline->mSettingsBuffer usage:MTLResourceUsageRead];
	[computeEncoder useResource:pRaytracingPipeline->mRaysBuffer usage:MTLResourceUsageWrite];
//	[computeEncoder useResource:pRaytracingPipeline->mIntersectionBuffer usage:MTLResourceUsageRead];
	[computeEncoder useResource:pRaytracingPipeline->pPathIndicesBuffer->mtlBuffer usage:MTLResourceUsageWrite];
	[computeEncoder useResource:pRaytracingPipeline->mPathCallStackBuffer usage:MTLResourceUsageRead | MTLResourceUsageWrite];
	[computeEncoder useResource:pRaytracingPipeline->mPayloadBuffer usage:MTLResourceUsageWrite];
	[computeEncoder useResource:pRaytracingPipeline->pRayCountBuffer[0]->mtlBuffer usage:MTLResourceUsageWrite];
	
	short shaderIndex = 0;
	[computeEncoder setBuffer:pRaytracingPipeline->mRaytracingArgumentsBuffer       offset:0				atIndex:DESCRIPTOR_UPDATE_FREQ_COUNT];
	[computeEncoder setBuffer:pRaytracingPipeline->pPathIndicesBuffer->mtlBuffer 	offset:0 				atIndex:DESCRIPTOR_UPDATE_FREQ_COUNT + 1];
	[computeEncoder setBuffer:pRaytracingPipeline->pRayCountBuffer[0]->mtlBuffer 	offset:0 			   	atIndex:DESCRIPTOR_UPDATE_FREQ_COUNT + 2];
	[computeEncoder setBytes:&shaderIndex length:sizeof(shaderIndex) atIndex:DESCRIPTOR_UPDATE_FREQ_COUNT + 3];
	
	// Bind the ray generation compute pipeline
	[computeEncoder setComputePipelineState:pRaytracingPipeline->mMetalPipelines[0]];
	// Launch threads
	[computeEncoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadsPerThreadgroup];
	
	[computeEncoder popDebugGroup];
	
	invokeShaders(pCmd, pRaytracing, pDesc,
				  pRaytracingPipeline->mPayloadBuffer,
				  threadgroups);
}

void mtl_cmdBindRaytracingPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
	UNREF_PARAM(pCmd);
	UNREF_PARAM(pPipeline);
}

struct SSVGFDenoiser
{
	id mtlDenoiser; // MPSSVGFDenoiser*
};

void addSSVGFDenoiser(Renderer* pRenderer, SSVGFDenoiser** ppDenoiser)
{
	if (@available(macOS 10.15, iOS 13, *))
	{
		SSVGFDenoiser *denoiser = (SSVGFDenoiser*)tf_calloc(1, sizeof(SSVGFDenoiser));
		denoiser->mtlDenoiser = [[MPSSVGFDenoiser alloc] initWithDevice:pRenderer->pDevice];
		*ppDenoiser = denoiser;
	}
	else
	{
		*ppDenoiser = NULL;
	}
}

void removeSSVGFDenoiser(SSVGFDenoiser* pDenoiser)
{
	if (!pDenoiser)
	{
		return;
	}
	
	pDenoiser->mtlDenoiser = nil;
	tf_free(pDenoiser);
}

void clearSSVGFDenoiserTemporalHistory(SSVGFDenoiser* pDenoiser)
{
	ASSERT(pDenoiser);
	
	if (@available(macOS 10.15, iOS 13, *))
	{
		[(MPSSVGFDenoiser*)pDenoiser->mtlDenoiser clearTemporalHistory];
	}
}

void cmdSSVGFDenoise(Cmd* pCmd, SSVGFDenoiser* pDenoiser, Texture* pSourceTexture, Texture* pMotionVectorTexture, Texture* pDepthNormalTexture, Texture* pPreviousDepthNormalTexture, Texture** ppOut)
{
	ASSERT(pDenoiser);
	
	if (@available(macOS 10.15, iOS 13, *))
	{
		if (pCmd->mtlComputeEncoder)
		{
			[pCmd->mtlComputeEncoder memoryBarrierWithScope:MTLBarrierScopeTextures];
		}
		
		util_end_current_encoders(pCmd, false);
		
		MPSSVGFDenoiser* denoiser = (MPSSVGFDenoiser*)pDenoiser->mtlDenoiser;
		
		id<MTLTexture> resultTexture = [denoiser encodeToCommandBuffer:pCmd->mtlCommandBuffer sourceTexture:pSourceTexture->mtlTexture motionVectorTexture:pMotionVectorTexture->mtlTexture depthNormalTexture:pDepthNormalTexture->mtlTexture previousDepthNormalTexture:pPreviousDepthNormalTexture->mtlTexture];
		
		TextureDesc resultTextureDesc = {};
		resultTextureDesc.mFlags = TEXTURE_CREATION_FLAG_NONE;
		resultTextureDesc.mWidth = (uint32_t)resultTexture.width;
		resultTextureDesc.mHeight = (uint32_t)resultTexture.height;
		resultTextureDesc.mDepth = (uint32_t)resultTexture.depth;
		resultTextureDesc.mArraySize = (uint32_t)resultTexture.arrayLength;
		resultTextureDesc.mMipLevels = (uint32_t)resultTexture.mipmapLevelCount;
		resultTextureDesc.mSampleCount = SAMPLE_COUNT_1;
		resultTextureDesc.mSampleQuality = 0;
		resultTextureDesc.mFormat = TinyImageFormat_FromMTLPixelFormat((TinyImageFormat_MTLPixelFormat)resultTexture.pixelFormat);
		resultTextureDesc.mClearValue = {};
		resultTextureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		resultTextureDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		resultTextureDesc.pNativeHandle = CFBridgingRetain(resultTexture);
		resultTextureDesc.mHostVisible = resultTexture.storageMode != MTLStorageModePrivate;

		resultTextureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;

		add_texture(pCmd->pRenderer, &resultTextureDesc, ppOut, false);
		(*ppOut)->mpsTextureAllocator = denoiser.textureAllocator;
	}
	else
	{
		*ppOut = {};
		return;
	}
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


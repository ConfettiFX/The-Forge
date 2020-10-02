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

// Adapted from AMD Parallel Primitives:
/**********************************************************************
 Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ********************************************************************/

#include "ParallelPrimitives.h"

#include "../../Common_3/Renderer/IResourceLoader.h"
#include "../../Common_3/OS/Interfaces/ILog.h"
#include "../../Common_3/OS/Interfaces/IMemory.h"

ParallelPrimitives::PipelineComponents::PipelineComponents() : mNextSetIndex(0), pDescriptorSet(NULL), pShader(NULL), pPipeline(NULL), pRootSignature(NULL) {}

void ParallelPrimitives::PipelineComponents::init(Renderer* renderer, const char* functionName) {
	pRenderer = renderer;
	
	ShaderLoadDesc shaderLoadDesc = {};
	shaderLoadDesc.mStages[0].pFileName = "ParallelPrimitives.comp";
	shaderLoadDesc.mStages[0].pEntryPointName = functionName;
	addShader(pRenderer, &shaderLoadDesc, &pShader);
	
	RootSignatureDesc rootSigDesc = {};
	rootSigDesc.mShaderCount = 1;
	rootSigDesc.ppShaders = &pShader;
	addRootSignature(pRenderer, &rootSigDesc, &pRootSignature);
	
	PipelineDesc pipelineDesc = {};
	pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
	ComputePipelineDesc& computePipelineDesc = pipelineDesc.mComputeDesc;
	computePipelineDesc.pShaderProgram = pShader;
	computePipelineDesc.pRootSignature = pRootSignature;
	addPipeline(pRenderer, &pipelineDesc, &pPipeline);
	
	DescriptorSetDesc descSetDesc = {};
	descSetDesc.mMaxSets = ParallelPrimitives::setsPerPipeline;
	descSetDesc.mUpdateFrequency = DESCRIPTOR_UPDATE_FREQ_PER_DRAW;
	descSetDesc.pRootSignature = pRootSignature;
	addDescriptorSet(pRenderer, &descSetDesc, &pDescriptorSet);
}

ParallelPrimitives::PipelineComponents::~PipelineComponents() {
	if (pDescriptorSet) {
		removeDescriptorSet(pRenderer, pDescriptorSet);
	}
	if (pPipeline) {
		removePipeline(pRenderer, pPipeline);
	}
	if (pRootSignature) {
		removeRootSignature(pRenderer, pRootSignature);
	}
	if (pShader) {
		removeShader(pRenderer, pShader);
	}
}

ParallelPrimitives::ParallelPrimitives(Renderer* renderer) : pRenderer(renderer) {
	const char *functionNames[] = { "scan_exclusive_int4", "scan_exclusive_part_int4", "distribute_part_sum_int4", "BitHistogram", "ScatterKeys", "ScatterKeysAndValues", "ClearOffsetBuffer", "GenerateOffsetBuffer", "GenerateIndirectArgumentsFromOffsetBuffer"  };
	PipelineComponents* components[] = { &mScanExclusiveInt4, &mScanExclusivePartInt4, &mDistributePartSumInt4, &mBitHistogram, &mScatterKeys, &mScatterKeysAndValues, &mClearOffsetBuffer, &mGenerateOffsetBuffer, &mIndirectArgsFromOffsetBuffer };
	
	for (size_t i = 0; i < sizeof(components) / sizeof(components[0]); i += 1) {
		components[i]->init(pRenderer, functionNames[i]);
	}

	IndirectArgumentDescriptor argDescriptor = { };
	argDescriptor.mType = INDIRECT_DISPATCH;
	
	CommandSignatureDesc commandSignatureDesc = { };
	commandSignatureDesc.mIndirectArgCount = 1;
	commandSignatureDesc.pArgDescs = &argDescriptor;
	commandSignatureDesc.mPacked = true;
	addIndirectCommandSignature(pRenderer, &commandSignatureDesc, &pCommandSignature);
}

ParallelPrimitives::~ParallelPrimitives() {
	for (Buffer* buffer : mTemporaryBuffers) {
		removeResource(buffer);
	}
	mTemporaryBuffers.set_capacity(0);
	removeIndirectCommandSignature(pRenderer, pCommandSignature);
}

Buffer* ParallelPrimitives::temporaryBuffer(size_t length) {
	eastl::vector<Buffer*>::const_iterator bestBufferIt = mTemporaryBuffers.end();
	Buffer* buffer = NULL;
	for (eastl::vector<Buffer*>::const_iterator it = mTemporaryBuffers.begin(); it < mTemporaryBuffers.end(); ++it) {
		Buffer* candidateBuffer = *it;
		uint64_t candidateLength = candidateBuffer->mSize;
		if (candidateLength >= length && (buffer == NULL || buffer->mSize > candidateLength)) {
			buffer = candidateBuffer;
			bestBufferIt = it;
		}
	}
	
	if (!buffer) {
		BufferLoadDesc bufferLoadDesc = {};
		bufferLoadDesc.ppBuffer = &buffer;
		
		bufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		bufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufferLoadDesc.mDesc.mSize = length;
		bufferLoadDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		bufferLoadDesc.mDesc.mElementCount = length / sizeof(int32_t);
		addResource(&bufferLoadDesc, NULL);
	} else {
		mTemporaryBuffers.erase_unsorted(bestBufferIt);
	}
	
	return buffer;
}

void ParallelPrimitives::depositTemporaryBuffer(Buffer* buffer) {
	ASSERT(buffer);
	mTemporaryBuffers.push_back(buffer);
}

void ParallelPrimitives::scanExclusiveAddWG(Cmd* pCmd, Buffer* input, Buffer* output, uint32_t elementCount) {
	
	cmdBindPipeline(pCmd, mScanExclusiveInt4.pPipeline);
	cmdBindPushConstants(pCmd, mScanExclusiveInt4.pRootSignature, "elementCountRootConstant", &elementCount);
	
	DescriptorData params[2] = {};
	
	params[0].ppBuffers = &input;
	params[0].pName = "inputArray";
	params[1].ppBuffers = &output;
	params[1].pName = "outputArray";
	
	uint32_t setIndex = incrementSetIndex(&mScanExclusiveInt4.mNextSetIndex);
	updateDescriptorSet(pCmd->pRenderer, setIndex, mScanExclusiveInt4.pDescriptorSet, 2, params);
	cmdBindDescriptorSet(pCmd, setIndex, mScanExclusiveInt4.pDescriptorSet);
	
	cmdDispatch(pCmd, 1, 1, 1);
	

	BufferBarrier barriers[] = {
		{ output, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
	};
	cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
}

void ParallelPrimitives::scanExclusiveAddTwoLevel(Cmd* pCmd, Buffer* input, Buffer* output, uint32_t elementCount) {
	uint32_t groupBlockSizeScan = ParallelPrimitives::workgroupSize << 3;
	uint32_t groupBlockSizeDistribute = ParallelPrimitives::workgroupSize << 2;
	
	uint32_t bottomLevelScanGroupCount = (elementCount + groupBlockSizeScan - 1) / groupBlockSizeScan;
	uint32_t topLevelScanGroupCount = (bottomLevelScanGroupCount + groupBlockSizeScan - 1) / groupBlockSizeScan;
	
	uint32_t bottomLevelDistributeGroupCount = (elementCount + groupBlockSizeDistribute - 1) / groupBlockSizeDistribute;
	
	Buffer* devicePartSums = this->temporaryBuffer(sizeof(int32_t) * max(bottomLevelScanGroupCount, (uint32_t)4));
	
	PipelineComponents& bottomLevelScan = mScanExclusivePartInt4;
	PipelineComponents& topLevelScan = mScanExclusiveInt4;
	PipelineComponents& distributeSums = mDistributePartSumInt4;
	
	DescriptorData params[4] = {};
	
	{
		cmdBindPipeline(pCmd, bottomLevelScan.pPipeline);
		
		params[0].ppBuffers = &input;
		params[0].pName = "inputArray";
		params[1].ppBuffers = &output;
		params[1].pName = "outputArray";
		params[2].ppBuffers = &devicePartSums;
		params[2].pName = "outputSums";
		
		cmdBindPushConstants(pCmd, bottomLevelScan.pRootSignature, "elementCountRootConstant", &elementCount);
		
		uint32_t setIndex = incrementSetIndex(&bottomLevelScan.mNextSetIndex);
		updateDescriptorSet(pCmd->pRenderer, setIndex, bottomLevelScan.pDescriptorSet, 3, params);
		cmdBindDescriptorSet(pCmd, setIndex, bottomLevelScan.pDescriptorSet);
		
		cmdDispatch(pCmd, (uint32_t)bottomLevelScanGroupCount, 1, 1);

		BufferBarrier barriers[] = {
			{ output, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
			{ devicePartSums, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false }
		};
		cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
	}
	
	{
		cmdBindPipeline(pCmd, topLevelScan.pPipeline);
		
		params[0].ppBuffers = &devicePartSums;
		params[0].pName = "inputArray";
		params[1].ppBuffers = &devicePartSums;
		params[1].pName = "outputArray";
		
		cmdBindPushConstants(pCmd, topLevelScan.pRootSignature, "elementCountRootConstant", &bottomLevelScanGroupCount);
		
		uint32_t setIndex = incrementSetIndex(&topLevelScan.mNextSetIndex);
		updateDescriptorSet(pCmd->pRenderer, setIndex, topLevelScan.pDescriptorSet, 2, params);
		cmdBindDescriptorSet(pCmd, setIndex, topLevelScan.pDescriptorSet);
		cmdDispatch(pCmd, topLevelScanGroupCount, 1, 1);
		
		BufferBarrier barriers[] = {
			{ devicePartSums, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false }
		};
		cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
	}
	
	{
		cmdBindPipeline(pCmd, distributeSums.pPipeline);
		
		params[0].ppBuffers = &devicePartSums;
		params[0].pName = "inputSums";
		params[1].ppBuffers = &output;
		params[1].pName = "inoutArray";
		
		cmdBindPushConstants(pCmd, distributeSums.pRootSignature, "elementCountRootConstant", &elementCount);
		
		uint32_t setIndex = incrementSetIndex(&distributeSums.mNextSetIndex);
		updateDescriptorSet(pCmd->pRenderer, setIndex, distributeSums.pDescriptorSet, 2, params);
		cmdBindDescriptorSet(pCmd, setIndex, distributeSums.pDescriptorSet);
		
		cmdDispatch(pCmd, bottomLevelDistributeGroupCount, 1, 1);
		
		BufferBarrier barriers[] = {
			{ output, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false }
		};
		cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
	}
	
	depositTemporaryBuffer(devicePartSums);
}


void ParallelPrimitives::scanExclusiveAddThreeLevel(Cmd* pCmd, Buffer* input, Buffer* output, uint32_t elementCount) {
	uint32_t groupBlockSizeScan = (ParallelPrimitives::workgroupSize << 3);
	uint32_t groupBlockSizeDistribute = (ParallelPrimitives::workgroupSize << 2);
	
	uint32_t bottomLevelScanGroupCount = (elementCount + groupBlockSizeScan - 1) / groupBlockSizeScan;
	uint32_t midLevelScanGroupCount = (bottomLevelScanGroupCount + groupBlockSizeScan - 1) / groupBlockSizeScan;
	uint32_t topLevelScanGroupCount = (midLevelScanGroupCount + groupBlockSizeScan - 1) / groupBlockSizeScan;
	
	uint32_t bottomLevelDistributeGroupCount = (elementCount + groupBlockSizeDistribute - 1) / groupBlockSizeDistribute;
	uint32_t midLevelDistributeGroupCount = (bottomLevelDistributeGroupCount + groupBlockSizeDistribute - 1) / groupBlockSizeDistribute;
	
	Buffer* devicePartSumsBottomLevel = this->temporaryBuffer(sizeof(int32_t) * max(bottomLevelScanGroupCount, 4u));
	Buffer* devicePartSumsMidLevel = this->temporaryBuffer(sizeof(int32_t) * max(midLevelScanGroupCount, 4u));
	
	PipelineComponents& bottomLevelScan = mScanExclusivePartInt4;
	PipelineComponents& topLevelScan = mScanExclusiveInt4;
	PipelineComponents& distributeSums = mDistributePartSumInt4;
	
	DescriptorData params[3] = {};
	
	{
		cmdBindPipeline(pCmd, bottomLevelScan.pPipeline);
		
		params[0].ppBuffers = &input;
		params[0].pName = "inputArray";
		params[1].ppBuffers = &output;
		params[1].pName = "outputArray";
		params[2].ppBuffers = &devicePartSumsBottomLevel;
		params[2].pName = "outputSums";
		
		cmdBindPushConstants(pCmd, bottomLevelScan.pRootSignature, "elementCountRootConstant", &elementCount);
		
		uint32_t setIndex = incrementSetIndex(&bottomLevelScan.mNextSetIndex);
		updateDescriptorSet(pCmd->pRenderer, setIndex, bottomLevelScan.pDescriptorSet, 3, params);
		cmdBindDescriptorSet(pCmd, setIndex, bottomLevelScan.pDescriptorSet);
		
		cmdDispatch(pCmd, bottomLevelScanGroupCount, 1, 1);
		
		BufferBarrier barriers[] = {
			{ output, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
			{ devicePartSumsBottomLevel, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
		};
		cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
	}
	
	{
		cmdBindPipeline(pCmd, bottomLevelScan.pPipeline);
		
		params[0].ppBuffers = &devicePartSumsBottomLevel;
		params[0].pName = "inputArray";
		params[1].ppBuffers = &devicePartSumsBottomLevel;
		params[1].pName = "outputArray";
		params[2].ppBuffers = &devicePartSumsMidLevel;
		params[2].pName = "outputSums";
		
		cmdBindPushConstants(pCmd, bottomLevelScan.pRootSignature, "elementCountRootConstant", &bottomLevelScanGroupCount);
		
		uint32_t setIndex = incrementSetIndex(&bottomLevelScan.mNextSetIndex);
		updateDescriptorSet(pCmd->pRenderer, setIndex, bottomLevelScan.pDescriptorSet, 3, params);
		cmdBindDescriptorSet(pCmd, setIndex, bottomLevelScan.pDescriptorSet);
		
		cmdDispatch(pCmd, midLevelScanGroupCount, 1, 1);
		
		BufferBarrier barriers[] = {
			{ devicePartSumsBottomLevel, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
			{ devicePartSumsMidLevel, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
		};
		cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
	}
	
	{
		cmdBindPipeline(pCmd, topLevelScan.pPipeline);
		
		params[0].ppBuffers = &devicePartSumsMidLevel;
		params[0].pName = "inputArray";
		params[1].ppBuffers = &devicePartSumsMidLevel;
		params[1].pName = "outputArray";
		
		cmdBindPushConstants(pCmd, topLevelScan.pRootSignature, "elementCountRootConstant", &midLevelScanGroupCount);
		
		uint32_t setIndex = incrementSetIndex(&topLevelScan.mNextSetIndex);
		updateDescriptorSet(pCmd->pRenderer, setIndex, topLevelScan.pDescriptorSet, 2, params);
		cmdBindDescriptorSet(pCmd, setIndex, topLevelScan.pDescriptorSet);
		
		cmdDispatch(pCmd, topLevelScanGroupCount, 1, 1);
		
		BufferBarrier barriers[] = {
			{ devicePartSumsMidLevel, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
		};
		cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
	}
	
	{
		cmdBindPipeline(pCmd, distributeSums.pPipeline);
		
		params[0].ppBuffers = &devicePartSumsMidLevel;
		params[0].pName = "inputArray";
		params[1].ppBuffers = &devicePartSumsBottomLevel;
		params[1].pName = "outputArray";
		
		cmdBindPushConstants(pCmd, distributeSums.pRootSignature, "elementCountRootConstant", &bottomLevelScanGroupCount);
		
		uint32_t setIndex = incrementSetIndex(&distributeSums.mNextSetIndex);
		updateDescriptorSet(pCmd->pRenderer, setIndex, distributeSums.pDescriptorSet, 2, params);
		cmdBindDescriptorSet(pCmd, setIndex, distributeSums.pDescriptorSet);
		
		cmdDispatch(pCmd, midLevelDistributeGroupCount, 1, 1);
		
		BufferBarrier barriers[] = {
			{ devicePartSumsBottomLevel, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
		};
		cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
	}
	
	{
		cmdBindPipeline(pCmd, distributeSums.pPipeline);
		
		params[0].ppBuffers = &devicePartSumsBottomLevel;
		params[0].pName = "inputSums";
		params[1].ppBuffers = &output;
		params[1].pName = "inoutArray";
		
		cmdBindPushConstants(pCmd, distributeSums.pRootSignature, "elementCountRootConstant", &elementCount);
		
		uint32_t setIndex = incrementSetIndex(&distributeSums.mNextSetIndex);
		updateDescriptorSet(pCmd->pRenderer, setIndex, distributeSums.pDescriptorSet, 2, params);
		cmdBindDescriptorSet(pCmd, setIndex, distributeSums.pDescriptorSet);
		
		cmdDispatch(pCmd, bottomLevelDistributeGroupCount, 1, 1);
		
		BufferBarrier barriers[] = {
			{ output, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
		};
		cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
	}
	
	depositTemporaryBuffer(devicePartSumsBottomLevel);
	depositTemporaryBuffer(devicePartSumsMidLevel);
}

void ParallelPrimitives::scanExclusiveAdd(Cmd* pCmd, Buffer* input, Buffer* output, uint32_t elementCount) {
	if (elementCount < ParallelPrimitives::scanElementsPerWorkgroup) {
		return scanExclusiveAddWG(pCmd, input, output, elementCount);
	} else if (elementCount <= ParallelPrimitives::scanElementsPerWorkgroup * ParallelPrimitives::scanElementsPerWorkgroup) {
		return scanExclusiveAddTwoLevel(pCmd, input, output, elementCount);
	} else if (elementCount <= ParallelPrimitives::scanElementsPerWorkgroup * ParallelPrimitives::scanElementsPerWorkgroup * ParallelPrimitives::scanElementsPerWorkgroup) {
		return scanExclusiveAddThreeLevel(pCmd, input, output, elementCount);
	} else {
		ASSERT(false && "The maximum number of elements for scan exceeded.");
	}
}

void ParallelPrimitives::sortRadixKeysValues(Cmd* pCmd, Buffer* inputKeys, Buffer* inputValues, Buffer* outputKeys, Buffer* outputValues, IndirectCountBuffer elementCount, uint32_t maxKey) {
	ASSERT(elementCount.mUpperLimit <= (1 << 25) && "Radix sort currently only works on up to 2^25 items due to an unknown bug.");
	
	cmdBeginDebugMarker(pCmd, 0, 1, 0, "Radix Sort");
	
	uint32_t groupBlockSize = (ParallelPrimitives::workgroupSize * 4 * 8);
	uint32_t blockCount = (elementCount.mUpperLimit + groupBlockSize - 1) / groupBlockSize;
	
	Buffer* deviceHistograms = this->temporaryBuffer(sizeof(int32_t) * blockCount * 16);
	Buffer* deviceTempKeysBuffer = this->temporaryBuffer(sizeof(int32_t) * elementCount.mUpperLimit);
	Buffer* deviceTempValsBuffer = this->temporaryBuffer(sizeof(int32_t) * elementCount.mUpperLimit);
	
	Buffer* fromKeys = inputKeys;
	Buffer* fromVals = inputValues;
	Buffer* toKeys = deviceTempKeysBuffer;
	Buffer* toVals = deviceTempValsBuffer;
	
	PipelineComponents& histogramKernel = mBitHistogram;
	PipelineComponents& scatterKeysAndVals = mScatterKeysAndValues;
	
	uint32_t zeroBits = clz32a(maxKey);
	
	uint32_t swapCount = (32 - zeroBits + 3) / 4;
	
	if (swapCount % 2 == 1) {
		toKeys = outputKeys;
		toVals = outputValues;
	}
	
	DescriptorData params[6] = {};
	
	for (uint32_t offset = 0; offset < 32 - zeroBits; offset += 4) {
		// Split
		{
			cmdBindPipeline(pCmd, histogramKernel.pPipeline);
			
			cmdBindPushConstants(pCmd, histogramKernel.pRootSignature, "rootConstants", &offset);
			
			params[0].ppBuffers = &fromKeys;
			params[0].pName = "inputArray";
			params[1].ppBuffers = &deviceHistograms;
			params[1].pName = "outHistogram";
			params[2].ppBuffers = &elementCount.pBuffer;
			params[2].pName = "elementCount";

			uint32_t setIndex = incrementSetIndex(&histogramKernel.mNextSetIndex);
			updateDescriptorSet(pCmd->pRenderer, setIndex, histogramKernel.pDescriptorSet, 3, params);
			cmdBindDescriptorSet(pCmd, setIndex, histogramKernel.pDescriptorSet);
			
			cmdDispatch(pCmd, blockCount, 1, 1);

			BufferBarrier barriers[] = {
				{ deviceHistograms, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
			};
			cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
		}
		
		// Scan histograms
		this->scanExclusiveAdd(pCmd, deviceHistograms, deviceHistograms, blockCount * 16);
		
		// Scatter keys
		{
			cmdBindPipeline(pCmd, scatterKeysAndVals.pPipeline);
			
			cmdBindPushConstants(pCmd, scatterKeysAndVals.pRootSignature, "rootConstants", &offset);
			
			params[0].ppBuffers = &fromKeys;
			params[0].pName = "inputKeys";
			params[1].ppBuffers = &fromVals;
			params[1].pName = "inputValues";
			params[2].ppBuffers = &elementCount.pBuffer;
			params[2].pName = "elementCount";
			params[3].ppBuffers = &deviceHistograms;
			params[3].pName = "inputHistograms";
			params[4].ppBuffers = &toKeys;
			params[4].pName = "outputKeys";
			params[5].ppBuffers = &toVals;
			params[5].pName = "outputValues";
			
			uint32_t setIndex = incrementSetIndex(&scatterKeysAndVals.mNextSetIndex);
			updateDescriptorSet(pCmd->pRenderer, setIndex, scatterKeysAndVals.pDescriptorSet, 6, params);
			cmdBindDescriptorSet(pCmd, setIndex, scatterKeysAndVals.pDescriptorSet);
			
			cmdDispatch(pCmd, blockCount, 1, 1);
			
			BufferBarrier barriers[] = {
				{ toKeys, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
				{ toVals, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
			};
			cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
		}
		
		if (offset == 0) {
			if (swapCount % 2 == 1) {
				fromKeys = deviceTempKeysBuffer;
				fromVals = deviceTempValsBuffer;
			} else {
				fromKeys = outputKeys;
				fromVals = outputValues;
			}
		}
		
		// Swap pointers
		Buffer* tmpKeys = fromKeys;
		fromKeys = toKeys;
		toKeys = tmpKeys;
		
		Buffer* tmpVals = fromVals;
		fromVals = toVals;
		toVals = tmpVals;
	}
	
	depositTemporaryBuffer(deviceHistograms);
	depositTemporaryBuffer(deviceTempKeysBuffer);
	depositTemporaryBuffer(deviceTempValsBuffer);
	
	ASSERT(fromKeys == outputKeys);
	ASSERT(fromVals == outputValues);
	
	cmdEndDebugMarker(pCmd);
}

void ParallelPrimitives::sortRadix(Cmd* pCmd, Buffer* inputKeys, Buffer* outputKeys, IndirectCountBuffer elementCount, uint32_t maxKey) {
	ASSERT(elementCount.mUpperLimit <= (1 << 25) && "Radix sort currently only works on up to 2^25 items due to an unknown bug.");
	
	cmdBeginDebugMarker(pCmd, 0, 1, 0, "Radix Sort");
	
	uint32_t groupBlockSize = (ParallelPrimitives::workgroupSize * 4 * 8);
	uint32_t blockCount = (elementCount.mUpperLimit + groupBlockSize - 1) / groupBlockSize;
	
	Buffer* deviceHistograms = this->temporaryBuffer(sizeof(int32_t) * blockCount * 16);
	Buffer* deviceTempKeys = this->temporaryBuffer(sizeof(int32_t) * elementCount.mUpperLimit);
	
	Buffer* fromKeys = inputKeys;
	Buffer* toKeys = deviceTempKeys;
	
	PipelineComponents& histogramKernel = mBitHistogram;
	PipelineComponents& scatterKeys = mScatterKeys;
	
	uint32_t zeroBits = clz32a(maxKey);
	
	uint32_t swapCount = (32 - zeroBits + 3) / 4;
	
	if (swapCount % 2 == 1) {
		toKeys = outputKeys;
	}
	
	DescriptorData params[4] = {};
	
	for (uint32_t offset = 0; offset < 32 - zeroBits; offset += 4) {
		// Split
		{
			cmdBindPipeline(pCmd, histogramKernel.pPipeline);
			
			cmdBindPushConstants(pCmd, histogramKernel.pRootSignature, "rootConstants", &offset);
			params[0].ppBuffers = &fromKeys;
			params[0].pName = "inputArray";
			params[1].ppBuffers = &deviceHistograms;
			params[1].pName = "outHistogram";
			params[2].ppBuffers = &elementCount.pBuffer;
			params[2].pName = "elementCount";
			
			uint32_t setIndex = incrementSetIndex(&histogramKernel.mNextSetIndex);
			updateDescriptorSet(pCmd->pRenderer, setIndex, histogramKernel.pDescriptorSet, 3, params);
			cmdBindDescriptorSet(pCmd, setIndex, histogramKernel.pDescriptorSet);
			
			cmdDispatch(pCmd, blockCount, 1, 1);
			
			BufferBarrier barriers[] = {
				{ deviceHistograms, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
			};
			cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
		}
		
		// Scan histograms
		this->scanExclusiveAdd(pCmd, deviceHistograms, deviceHistograms, blockCount * 16);
		
		// Scatter keys
		{
			cmdBindPipeline(pCmd, scatterKeys.pPipeline);
			
			cmdBindPushConstants(pCmd, scatterKeys.pRootSignature, "rootConstants", &offset);
			params[0].ppBuffers = &fromKeys;
			params[0].pName = "inputKeys";
			params[1].ppBuffers = &elementCount.pBuffer;
			params[1].pName = "elementCount";
			params[2].ppBuffers = &deviceHistograms;
			params[2].pName = "inputHistograms";
			params[3].ppBuffers = &toKeys;
			params[3].pName = "outputKeys";
			
			uint32_t setIndex = incrementSetIndex(&scatterKeys.mNextSetIndex);
			updateDescriptorSet(pCmd->pRenderer, setIndex, scatterKeys.pDescriptorSet, 4, params);
			cmdBindDescriptorSet(pCmd, setIndex, scatterKeys.pDescriptorSet);
			
			cmdDispatch(pCmd, blockCount, 1, 1);
			
			BufferBarrier barriers[] = {
				{ toKeys, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
			};
			cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
		}
		
		if (offset == 0) {
			if (swapCount % 2 == 1) {
				fromKeys = deviceTempKeys;
			} else {
				fromKeys = outputKeys;
			}
		}
		
		// Swap pointers
		Buffer* tmpKeys = fromKeys;
		fromKeys = toKeys;
		toKeys = tmpKeys;
	}
	
	ASSERT(fromKeys == outputKeys);
	
	cmdEndDebugMarker(pCmd);
}

void ParallelPrimitives::generateOffsetBuffer(Cmd* pCmd, Buffer* sortedCategoryIndices, Buffer* outputBuffer, Buffer* totalCountOutputBuffer, IndirectCountBuffer sortedIndicesCount, uint32_t categoryCount, uint32_t indirectThreadsPerThreadgroup) {
	ASSERT(outputBuffer->mSize >= categoryCount * sizeof(uint32_t));
	ASSERT(totalCountOutputBuffer->mSize >= 4 * sizeof(uint32_t));
	
	cmdBeginDebugMarker(pCmd, 0, 1, 0, "Offset Buffer Generation");
	
	struct {
		uint32_t categoryCount;
		uint32_t indirectThreadsPerThreadgroup;
	} pushConstants;
	
	pushConstants.categoryCount = categoryCount;
	pushConstants.indirectThreadsPerThreadgroup = indirectThreadsPerThreadgroup;
	
	DescriptorData params[4] = {};
	
	params[0].ppBuffers = &sortedCategoryIndices;
	params[0].pName = "sortedIndices";
	params[1].ppBuffers = &sortedIndicesCount.pBuffer;
	params[1].pName = "sortedIndicesCount";
	params[2].ppBuffers = &outputBuffer;
	params[2].pName = "offsetBuffer";
	params[3].ppBuffers = &totalCountOutputBuffer;
	params[3].pName = "totalCountAndIndirectArgs";

	{
		cmdBindPipeline(pCmd, mClearOffsetBuffer.pPipeline);
		
		cmdBindPushConstants(pCmd, mClearOffsetBuffer.pRootSignature, "rootConstants", &pushConstants);
		
		uint32_t setIndex = incrementSetIndex(&mClearOffsetBuffer.mNextSetIndex);
		updateDescriptorSet(pCmd->pRenderer, setIndex, mClearOffsetBuffer.pDescriptorSet, 4, params);
		cmdBindDescriptorSet(pCmd, setIndex, mClearOffsetBuffer.pDescriptorSet);
		
		cmdExecuteIndirect(pCmd, pCommandSignature, 1, sortedIndicesCount.pBuffer, 4, NULL, 0);
		
		BufferBarrier barriers[] = {
			{ outputBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
		};
		cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
	}
	
	{
		cmdBindPipeline(pCmd, mGenerateOffsetBuffer.pPipeline);
		
		cmdBindPushConstants(pCmd, mGenerateOffsetBuffer.pRootSignature, "rootConstants", &pushConstants);
		
		uint32_t setIndex = incrementSetIndex(&mGenerateOffsetBuffer.mNextSetIndex);
		updateDescriptorSet(pCmd->pRenderer, setIndex, mGenerateOffsetBuffer.pDescriptorSet, 4, params);
		cmdBindDescriptorSet(pCmd, setIndex, mGenerateOffsetBuffer.pDescriptorSet);
		
		cmdExecuteIndirect(pCmd, pCommandSignature, 1, sortedIndicesCount.pBuffer, 4, NULL, 0);
		
		BufferBarrier barriers[] = {
			{ outputBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
			{ totalCountOutputBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
		};
		cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
	}
	
	
	cmdEndDebugMarker(pCmd);
}

void ParallelPrimitives::generateIndirectArgumentsFromOffsetBuffer(Cmd* pCmd, Buffer* offsetBuffer, Buffer* activeIndexCountBuffer, Buffer* outIndirectArgumentsBuffer, uint32_t categoryCount, uint32_t indirectThreadsPerThreadgroup) {
	ASSERT(outIndirectArgumentsBuffer->mSize >= categoryCount * 4 * sizeof(uint32_t));
	
	cmdBeginDebugMarker(pCmd, 0, 1, 0, "Indirect Arguments from Offset Buffer");
	
	struct {
		uint32_t categoryCount;
		uint32_t indirectThreadsPerThreadgroup;
	} pushConstants;
	
	pushConstants.categoryCount = categoryCount;
	pushConstants.indirectThreadsPerThreadgroup = indirectThreadsPerThreadgroup;
	
	cmdBindPipeline(pCmd, mIndirectArgsFromOffsetBuffer.pPipeline);
	
	cmdBindPushConstants(pCmd, mIndirectArgsFromOffsetBuffer.pRootSignature, "rootConstants", &pushConstants);
	
	DescriptorData params[3] = {};
	
	params[0].ppBuffers = &offsetBuffer;
	params[0].pName = "offsetBuffer";
	params[1].ppBuffers = &activeIndexCountBuffer;
	params[1].pName = "totalIndexCount";
	params[2].ppBuffers = &outIndirectArgumentsBuffer;
	params[2].pName = "indirectArgumentsBuffer";
	
	uint32_t setIndex = incrementSetIndex(&mIndirectArgsFromOffsetBuffer.mNextSetIndex);
	updateDescriptorSet(pCmd->pRenderer, setIndex, mIndirectArgsFromOffsetBuffer.pDescriptorSet, 3, params);
	cmdBindDescriptorSet(pCmd, setIndex, mIndirectArgsFromOffsetBuffer.pDescriptorSet);
		
	uint32_t blockCount = (categoryCount + ParallelPrimitives::workgroupSize - 1) / ParallelPrimitives::workgroupSize;
	
	cmdDispatch(pCmd, blockCount, 1, 1);

	BufferBarrier barriers[] = {
		{ outIndirectArgumentsBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS, false },
	};
	cmdResourceBarrier(pCmd, sizeof(barriers) / sizeof(barriers[0]), barriers, 0, NULL, 0, NULL);
	
	cmdEndDebugMarker(pCmd);
}

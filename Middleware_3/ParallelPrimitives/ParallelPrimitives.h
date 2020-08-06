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

#include "../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/OS/Interfaces/ILog.h"

struct IndirectCountBuffer {
	Buffer *pBuffer; // Containing four `uint32_t`s: the count, the threadgroups X (ceil(count / THREADS_PER_THREADGROUP)), and threadgroups Y/Z (should be 1).
	uint32_t mUpperLimit;
	
	IndirectCountBuffer(uint32_t count, Buffer* buffer) : mUpperLimit(count), pBuffer(buffer) {};
};

struct ParallelPrimitives {
public:
	static const uint32_t setsPerPipeline = 1024;
	
	static const uint32_t workgroupSize = 64;
	static const uint32_t scanElementsPerWorkItem = 8;
	static const uint32_t segmentScanElementsPerWorkItem = 1;
	static const uint32_t scanElementsPerWorkgroup = workgroupSize * scanElementsPerWorkItem;
	static const uint32_t segmentScanElementsPerWorkgroup = workgroupSize * segmentScanElementsPerWorkItem;
	
	ParallelPrimitives(Renderer* pRenderer);
	~ParallelPrimitives();
    
	void scanExclusiveAdd(Cmd* pCmd, Buffer* input, Buffer* output, uint32_t elementCount);
	void sortRadix(Cmd* pCmd, Buffer* inputKeys, Buffer* outputKeys, IndirectCountBuffer elementCount, uint32_t maxKey = ~0);
	void sortRadixKeysValues(Cmd* pCmd, Buffer* inputKeys, Buffer* inputValues, Buffer* outputKeys, Buffer* outputValues, IndirectCountBuffer elementCount, uint32_t maxKey = ~0);
	
	void generateOffsetBuffer(Cmd* pCmd, Buffer* sortedCategoryIndices, Buffer* outputBuffer, Buffer* totalCountOutputBuffer, IndirectCountBuffer sortedIndicesCount, uint32_t categoryCount, uint32_t indirectThreadsPerThreadgroup);
	
	void generateIndirectArgumentsFromOffsetBuffer(Cmd* pCmd, Buffer* offsetBuffer, Buffer* activeIndexCountBuffer, Buffer* outIndirectArgumentsBuffer, uint32_t categoryCount, uint32_t indirectThreadsPerThreadgroup);
	
private:
	struct PipelineComponents {
		uint32_t mNextSetIndex;
		Renderer* pRenderer;
		DescriptorSet* pDescriptorSet;
		Shader* pShader;
		Pipeline* pPipeline;
		RootSignature* pRootSignature;
		
		PipelineComponents();
		~PipelineComponents();
		
		void init(Renderer* renderer, const char* functionName);
	};
	
	Renderer* pRenderer;
	
	PipelineComponents mScanExclusiveInt4;
	PipelineComponents mScanExclusivePartInt4;
	PipelineComponents mDistributePartSumInt4;
	PipelineComponents mBitHistogram;
	PipelineComponents mScatterKeys;
	PipelineComponents mScatterKeysAndValues;
	PipelineComponents mClearOffsetBuffer;
	PipelineComponents mGenerateOffsetBuffer;
	PipelineComponents mIndirectArgsFromOffsetBuffer;
	
	eastl::vector<Buffer*> mTemporaryBuffers;
	
	CommandSignature* pCommandSignature;
	
	Buffer* temporaryBuffer(size_t length);
	void depositTemporaryBuffer(Buffer* buffer);
	
	
	inline uint32_t incrementSetIndex(uint32_t* setIndex) {
		uint32_t index = *setIndex;
		*setIndex = (*setIndex + 1) % ParallelPrimitives::setsPerPipeline;
		return index;
	}
	
	inline unsigned int clz32a( uint32_t x ) /* 32-bit clz */
	{
		unsigned int n;
		if (x == 0)
			n = 32;
		else
			for (n = 0; ((x & 0x80000000) == 0); n++, x <<= 1) ;
		return n;
	}
	
	void scanExclusiveAddWG(Cmd* pCmd, Buffer* input, Buffer* output, uint32_t elementCount);
    
	void scanExclusiveAddTwoLevel(Cmd* pCmd, Buffer* input, Buffer* output, uint32_t elementCount);
    
	void scanExclusiveAddThreeLevel(Cmd* pCmd, Buffer* input, Buffer* output, uint32_t elementCount);
};

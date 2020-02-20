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

#ifdef METAL

#include "../../OS/Interfaces/ILog.h"

#include "../../ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../IRenderer.h"
#include <string.h>
#include "../../OS/Interfaces/IMemory.h"

#define MAX_REFLECT_STRING_LENGTH 128
#define MAX_BUFFER_BINDINGS 31

struct BufferStructMember
{
	char                name[MAX_REFLECT_STRING_LENGTH];
	int                 bufferIndex;
	unsigned long       offset;    //byte offset within the uniform block
	int                 sizeInBytes;
    
    ArgumentDescriptor  descriptor;
    
	BufferStructMember()
        : sizeInBytes(0)
        , offset(0)
        , bufferIndex(0)
    {
        name[0] = '\0';
    }
};

struct BufferInfo
{
	char           name[MAX_REFLECT_STRING_LENGTH];
	int            bufferIndex;
	size_t         sizeInBytes;
    size_t         alignment;
	int            currentOffset;
	bool           isUAV;
	bool           isArgBuffer;

	BufferInfo()
        : sizeInBytes(0)
        , alignment(0)
		, bufferIndex(-1)
		, currentOffset(0)
		, isUAV(false)
		, isArgBuffer(false)
	{
	}
};

struct SamplerInfo
{
	char name[MAX_REFLECT_STRING_LENGTH];
	int  slotIndex;
};

struct TextureInfo
{
	char           name[MAX_REFLECT_STRING_LENGTH];
	MTLTextureType type;
	int            slotIndex;
	bool           isUAV;
};

struct ShaderReflectionInfo
{
	eastl::vector<BufferStructMember> variableMembers;
	eastl::vector<BufferInfo>         buffers;
	eastl::vector<SamplerInfo>        samplers;
	eastl::vector<TextureInfo>        textures;
};

int getSizeFromDataType(MTLDataType dataType)
{
	const int HALF_SIZE = 4;
	const int FLOAT_SIZE = 4;
	const int INT_SIZE = 4;
	switch (dataType)
	{
		case MTLDataTypeFloat: return FLOAT_SIZE;
		case MTLDataTypeFloat2: return FLOAT_SIZE * 2;
		case MTLDataTypeFloat3: return FLOAT_SIZE * 3;
		case MTLDataTypeFloat4: return FLOAT_SIZE * 4;
		case MTLDataTypeFloat2x2: return FLOAT_SIZE * 2 * 2;
		case MTLDataTypeFloat2x3: return FLOAT_SIZE * 2 * 3;
		case MTLDataTypeFloat2x4: return FLOAT_SIZE * 2 * 4;
		case MTLDataTypeFloat3x2: return FLOAT_SIZE * 3 * 2;
		case MTLDataTypeFloat3x3: return FLOAT_SIZE * 3 * 3;
		case MTLDataTypeFloat3x4: return FLOAT_SIZE * 3 * 4;
		case MTLDataTypeFloat4x2: return FLOAT_SIZE * 4 * 2;
		case MTLDataTypeFloat4x3: return FLOAT_SIZE * 4 * 3;
		case MTLDataTypeFloat4x4: return FLOAT_SIZE * 4 * 4;
		case MTLDataTypeHalf: return HALF_SIZE;
		case MTLDataTypeHalf2: return HALF_SIZE * 2;
		case MTLDataTypeHalf3: return HALF_SIZE * 3;
		case MTLDataTypeHalf4: return HALF_SIZE * 4;
		case MTLDataTypeHalf2x2: return HALF_SIZE * 2 * 2;
		case MTLDataTypeHalf2x3: return HALF_SIZE * 2 * 3;
		case MTLDataTypeHalf2x4: return HALF_SIZE * 2 * 4;
		case MTLDataTypeHalf3x2: return HALF_SIZE * 3 * 2;
		case MTLDataTypeHalf3x3: return HALF_SIZE * 3 * 3;
		case MTLDataTypeHalf3x4: return HALF_SIZE * 3 * 4;
		case MTLDataTypeHalf4x2: return HALF_SIZE * 4 * 2;
		case MTLDataTypeHalf4x3: return HALF_SIZE * 4 * 3;
		case MTLDataTypeHalf4x4: return HALF_SIZE * 4 * 4;
		case MTLDataTypeInt: return INT_SIZE;
		case MTLDataTypeInt2: return INT_SIZE * 2;
		case MTLDataTypeInt3: return INT_SIZE * 3;
		case MTLDataTypeInt4: return INT_SIZE * 4;
		case MTLDataTypeUInt: return INT_SIZE;
		case MTLDataTypeUInt2: return INT_SIZE * 2;
		case MTLDataTypeUInt3: return INT_SIZE * 3;
		case MTLDataTypeUInt4: return INT_SIZE * 4;
		case MTLDataTypeShort: return HALF_SIZE;
		case MTLDataTypeShort2: return HALF_SIZE * 2;
		case MTLDataTypeShort3: return HALF_SIZE * 3;
		case MTLDataTypeShort4: return HALF_SIZE * 4;
		case MTLDataTypeUShort: return HALF_SIZE;
		case MTLDataTypeUShort2: return HALF_SIZE * 2;
		case MTLDataTypeUShort3: return HALF_SIZE * 3;
		case MTLDataTypeUShort4: return HALF_SIZE * 4;
		case MTLDataTypeChar: return 1;
		case MTLDataTypeChar2: return 2;
		case MTLDataTypeChar3: return 3;
		case MTLDataTypeChar4: return 4;
		case MTLDataTypeUChar: return 1;
		case MTLDataTypeUChar2: return 2;
		case MTLDataTypeUChar3: return 3;
		case MTLDataTypeUChar4: return 4;
		case MTLDataTypeBool: return 1;
		case MTLDataTypeBool2: return 2;
		case MTLDataTypeBool3: return 3;
		case MTLDataTypeBool4: return 4;
		case MTLDataTypeTexture: return 8;
        case MTLDataTypePointer: return 8;
        case MTLDataTypeSampler: return 0;
        case MTLDataTypeIndirectCommandBuffer: return 0;
        case MTLDataTypeRenderPipeline: return 0;
		default: break;
	}
	ASSERT(0 && "Unknown metal type");
	return -1;
}

// Returns the total size of the struct
uint32_t reflectShaderStruct(ShaderReflectionInfo* info, unsigned int bufferIndex, unsigned long parentOffset, MTLStructType* structObj)
{
	uint32_t totalSize = 0;
	for (MTLStructMember* member in structObj.members)
	{
		BufferStructMember bufferMember;
		strlcpy(bufferMember.name, [member.name UTF8String], MAX_REFLECT_STRING_LENGTH);
		bufferMember.bufferIndex = bufferIndex;
		bufferMember.offset = member.offset + parentOffset;
		bufferMember.descriptor.mDataType = member.dataType;
        bufferMember.descriptor.mArgumentIndex = (uint32_t)member.argumentIndex;
        bufferMember.descriptor.mBufferIndex = bufferIndex;
        bufferMember.descriptor.mTextureType = MTLTextureType1D;
        bufferMember.descriptor.mAccessType = MTLArgumentAccessReadOnly;
        bufferMember.descriptor.mAlignment = 0;
        bufferMember.descriptor.mArrayLength = 0;
        
		//  process each MTLStructMember
		if (member.dataType == MTLDataTypeStruct)
		{
			MTLStructType* nestedStruct = member.structType;
			if (nestedStruct != nil)
				bufferMember.sizeInBytes = reflectShaderStruct(info, bufferIndex, bufferMember.offset, nestedStruct);
			else
				bufferMember.sizeInBytes = getSizeFromDataType(bufferMember.descriptor.mDataType);
		}
		else if (member.dataType == MTLDataTypeArray)
		{
            bufferMember.descriptor.mArrayLength = (uint32_t)member.arrayType.arrayLength;

            bufferMember.descriptor.mDataType = member.arrayType.elementType;
            
			ASSERT(member.arrayType != nil);
			int arrayLength = (int)member.arrayType.arrayLength;
			if (member.arrayType.elementType == MTLDataTypeStruct)
			{
				MTLStructType* nestedStruct = member.arrayType.elementStructType;
				bufferMember.sizeInBytes = reflectShaderStruct(info, bufferIndex, bufferMember.offset, nestedStruct) * arrayLength;
			}
			else
            {
				bufferMember.sizeInBytes = getSizeFromDataType(member.arrayType.elementType) * arrayLength;
            }
		}
        else if (member.dataType == MTLDataTypeTexture)
        {
            bufferMember.descriptor.mAccessType = member.textureReferenceType.access;
            bufferMember.descriptor.mTextureType = member.textureReferenceType.textureType;
            
            bufferMember.sizeInBytes = getSizeFromDataType(member.dataType);
        }
        else if (member.dataType == MTLDataTypePointer)
        {
            bufferMember.descriptor.mAccessType = member.pointerType.access;
            bufferMember.descriptor.mAlignment = member.pointerType.alignment;
            
            bufferMember.sizeInBytes = getSizeFromDataType(member.dataType);
        }
		else
		{
			// member is neither struct nor array
			// analyze it; no need to drill down further
			bufferMember.sizeInBytes = getSizeFromDataType(member.dataType);
		}

		info->variableMembers.push_back(bufferMember);
		totalSize += bufferMember.sizeInBytes;
	}

	return totalSize;
}

void reflectShaderBufferArgument(ShaderReflectionInfo* info, MTLArgument* arg)
{
	if (arg.bufferDataType == MTLDataTypeStruct)
	{
		// We do this for constant buffer initialization. Constant buffers are always defined in structs,
		// so we only care about structs here.
		MTLStructType* theStruct = arg.bufferStructType;
		reflectShaderStruct(info, (uint32_t)arg.index, 0, theStruct);
	}
	else if (arg.bufferDataType == MTLDataTypeArray)
	{
		ASSERT(!"TODO: Implement");
	}

	// Reflect buffer info
	BufferInfo bufferInfo;
	strlcpy(bufferInfo.name, [arg.name UTF8String], MAX_REFLECT_STRING_LENGTH);
	bufferInfo.bufferIndex = (uint32_t)arg.index;
	bufferInfo.sizeInBytes = arg.bufferDataSize;
    bufferInfo.alignment   = arg.bufferAlignment;
	bufferInfo.isUAV = (arg.access == MTLArgumentAccessReadWrite || arg.access == MTLArgumentAccessWriteOnly);
	bufferInfo.isArgBuffer = arg.bufferPointerType.elementIsArgumentBuffer;
    
	info->buffers.push_back(bufferInfo);
}

void reflectShaderSamplerArgument(ShaderReflectionInfo* info, MTLArgument* arg)
{
	SamplerInfo samplerInfo;
	strlcpy(samplerInfo.name, [arg.name UTF8String], MAX_REFLECT_STRING_LENGTH);
	samplerInfo.slotIndex = (uint32_t)arg.index;
	info->samplers.push_back(samplerInfo);
}

void reflectShaderTextureArgument(ShaderReflectionInfo* info, MTLArgument* arg)
{
	TextureInfo textureInfo;
	strlcpy(textureInfo.name, [arg.name UTF8String], MAX_REFLECT_STRING_LENGTH);
	textureInfo.slotIndex = (uint32_t)arg.index;
	textureInfo.type = arg.textureType;
	textureInfo.isUAV = (arg.access == MTLArgumentAccessReadWrite || arg.access == MTLArgumentAccessWriteOnly);
	info->textures.push_back(textureInfo);
}

void reflectShader(ShaderReflectionInfo* info, NSArray<MTLArgument*>* shaderArgs)
{
	for (MTLArgument* arg in shaderArgs)
	{
		if (arg.type == MTLArgumentTypeBuffer)
		{
			reflectShaderBufferArgument(info, arg);
		}
		else if (arg.type == MTLArgumentTypeSampler)
		{
			reflectShaderSamplerArgument(info, arg);
		}
		else if (arg.type == MTLArgumentTypeTexture)
		{
			reflectShaderTextureArgument(info, arg);
		}
	}
}

uint32_t calculateNamePoolSize(const ShaderReflectionInfo* shaderReflectionInfo)
{
	uint32_t namePoolSize = 0;
	for (uint32_t i = 0; i < shaderReflectionInfo->variableMembers.size(); i++)
	{
		const BufferStructMember& varMember = shaderReflectionInfo->variableMembers[i];
		namePoolSize += (uint32_t)strlen(varMember.name) + 1;
	}
	for (uint32_t i = 0; i < shaderReflectionInfo->buffers.size(); i++)
	{
		const BufferInfo& buffer = shaderReflectionInfo->buffers[i];
		namePoolSize += (uint32_t)strlen(buffer.name) + 1;
        
        if (buffer.isArgBuffer)
        {
            for (uint32_t i = 0; i < shaderReflectionInfo->variableMembers.size(); ++i)
            {
                const BufferStructMember& bufferMember = shaderReflectionInfo->variableMembers[i];
                
                if (bufferMember.bufferIndex == buffer.bufferIndex)
                {
                    namePoolSize += (uint32_t)strlen(bufferMember.name) + 1;
                }
            }
        }
	}
	for (uint32_t i = 0; i < shaderReflectionInfo->textures.size(); i++)
	{
		const TextureInfo& tex = shaderReflectionInfo->textures[i];
		namePoolSize += (uint32_t)strlen(tex.name) + 1;
	}
	for (uint32_t i = 0; i < shaderReflectionInfo->samplers.size(); i++)
	{
		const SamplerInfo& sampler = shaderReflectionInfo->samplers[i];
		namePoolSize += (uint32_t)strlen(sampler.name) + 1;
	}
	return namePoolSize;
}

bool startsWith(const char* str, const char* preffix) { return strncmp(preffix, str, strlen(preffix)) == 0; }

bool isInputVertexBuffer(const BufferInfo& bufferInfo, ShaderStage shaderStage)
{
	return (startsWith(bufferInfo.name, "vertexBuffer.") && shaderStage == SHADER_STAGE_VERT);
}

void addShaderResource(ShaderResource* pResources, uint32_t idx, DescriptorType type, uint32_t bindingPoint, size_t sizeInBytes, size_t alignment, ShaderStage shaderStage, char** ppCurrentName, char* name)
{
	pResources[idx].type = type;
	pResources[idx].set = DESCRIPTOR_UPDATE_FREQ_NONE + 10;
	pResources[idx].reg = bindingPoint;
	pResources[idx].size = (uint32_t)sizeInBytes;
    pResources[idx].alignment = (uint32_t)alignment;
	pResources[idx].used_stages = shaderStage;
	pResources[idx].name = *ppCurrentName;
	pResources[idx].name_size = (uint32_t)strlen(name);
	// we dont own the names memory we need to copy it to the name pool
	memcpy(*ppCurrentName, name, pResources[idx].name_size);
	*ppCurrentName += pResources[idx].name_size + 1;
}

void mtl_createShaderReflection(
	Renderer* pRenderer, Shader* shader, const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage,
	eastl::unordered_map<uint32_t, MTLVertexFormat>* vertexAttributeFormats, ShaderReflection* pOutReflection)
{
	if (pOutReflection == NULL)
	{
		ASSERT(0);
		return;    // TODO: error msg
	}

	NSError* error = nil;

	ShaderReflectionInfo* pReflectionInfo = nil;

	// Setup temporary pipeline to get reflection data
	if (shaderStage == SHADER_STAGE_COMP)
	{
		MTLComputePipelineDescriptor* computePipelineDesc = [[MTLComputePipelineDescriptor alloc] init];
		computePipelineDesc.computeFunction = shader->mtlComputeShader;

		MTLComputePipelineReflection* ref;
		id<MTLComputePipelineState>   pipelineState =
			[pRenderer->pDevice newComputePipelineStateWithDescriptor:computePipelineDesc
															  options:MTLPipelineOptionBufferTypeInfo
														   reflection:&ref
																error:&error];

		if (!pipelineState)
		{
			NSLog(@ "Error generation compute pipeline object: %@", error);
			ASSERT(!"Compute pipeline object shouldn't fail to be created.");
			return;
		}

		pReflectionInfo = (ShaderReflectionInfo*)conf_calloc(1, sizeof(ShaderReflectionInfo));
		reflectShader(pReflectionInfo, ref.arguments);

		// Note: Metal compute shaders don't specify the number of threads per group in the shader code.
		// Instead this must be specified from the client API.
		// To fix this, the parser expects the metal shader code to specify it like other languages do in the following way:
		// Ex: // [numthreads(256, 1, 1)]
		// Notice that it is a commented out line, since we don't want this line to be taken into account by the Metal shader compiler.

		const char* numThreadsStart = strstr((const char*)shaderCode, "[numthreads(");
		if (numThreadsStart == NULL)
		{
			ASSERT(!"Compute shaders require: [numthreads(x,y,z)]");
			return;
		}
		numThreadsStart += strlen("[numthreads(");
		const char* numThreadsEnd = strstr(numThreadsStart, ")");
		if (numThreadsEnd == NULL)
		{
			ASSERT(!"Malformed[numthreads(x,y,z)]");
			return;
		}

		char   buff[128] = "";
		size_t len = numThreadsEnd - numThreadsStart;
		memcpy(buff, numThreadsStart, len);
		int count = sscanf(
			buff, "%d,%d,%d", &pOutReflection->mNumThreadsPerGroup[0], &pOutReflection->mNumThreadsPerGroup[1],
			&pOutReflection->mNumThreadsPerGroup[2]);
		if (count != 3)
		{
			ASSERT(!"Malformed[numthreads(x,y,z)]");
			return;
		}

		shader->mNumThreadsPerGroup[0] = pOutReflection->mNumThreadsPerGroup[0];
		shader->mNumThreadsPerGroup[1] = pOutReflection->mNumThreadsPerGroup[1];
		shader->mNumThreadsPerGroup[2] = pOutReflection->mNumThreadsPerGroup[2];
	}
	else
	{
		MTLRenderPipelineDescriptor* renderPipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];

		renderPipelineDesc.vertexFunction = shader->mtlVertexShader;
		renderPipelineDesc.fragmentFunction = shader->mtlFragmentShader;

		uint maxColorAttachments = MAX_RENDER_TARGET_ATTACHMENTS;
#ifdef TARGET_IOS
		if (![pRenderer->pDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v1])
			maxColorAttachments = 4;
#endif
		for (uint i = 0; i < maxColorAttachments; i++)
		{
			renderPipelineDesc.colorAttachments[i].pixelFormat = MTLPixelFormatR8Unorm;
		}
		renderPipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

		// We need to create a vertex descriptor if needed to obtain reflection information
		// We are forced to initialize the vertex descriptor with dummy information just to get
		// the reflection information.
		MTLVertexDescriptor* vertexDesc = [[MTLVertexDescriptor alloc] init];

		if (shaderStage == SHADER_STAGE_VERT)
		{
			// read line by line and find vertex attribute definitions
			char *p, *temp;
			p = strtok_r((char*)shaderCode, "\n", &temp);
			do
			{
				const char* pattern = "attribute(";
				const char* start = strstr(p, pattern);
				if (start != nil)
				{
					// vertex attribute definition found: create a vertex descriptor for this
					int             attrNumber = atoi(start + strlen(pattern));
					MTLVertexFormat vf = (strstr((const char*)p, "int") ? MTLVertexFormatInt : MTLVertexFormatFloat);
					vf = (strstr((const char*)p, "uint") ? MTLVertexFormatUInt : vf);
					// In case this is defined through a macro and we dont have
					// the numerical value just find an empty attribute index
					// Example: float4 position [attribute(UNIT_VB_PASS)];
					if (!isdigit(start[strlen(pattern)]))
						while (vertexAttributeFormats->find(attrNumber) != vertexAttributeFormats->end())
							++attrNumber;
					(*vertexAttributeFormats)[attrNumber] = vf;
				}
			} while ((p = strtok_r(NULL, "\n", &temp)) != NULL);
		}

		for (uint32_t i = 0; i < MAX_VERTEX_ATTRIBS; i++)
		{
			vertexDesc.attributes[i].offset = 0;
			vertexDesc.attributes[i].bufferIndex = 0;

			MTLVertexFormat vf = MTLVertexFormatFloat;

			eastl::unordered_map<uint32_t, MTLVertexFormat>::iterator it = vertexAttributeFormats->find(i);
			if (it != vertexAttributeFormats->end())
				vf = it->second;

			vertexDesc.attributes[i].format = vf;
		}
		vertexDesc.layouts[0].stride = MAX_VERTEX_ATTRIBS * sizeof(float);
		vertexDesc.layouts[0].stepRate = 1;
		vertexDesc.layouts[0].stepFunction = shader->mtlVertexShader.patchType != MTLPatchTypeNone
												 ? MTLVertexStepFunctionPerPatchControlPoint
												 : MTLVertexStepFunctionPerVertex;

		renderPipelineDesc.vertexDescriptor = vertexDesc;

		MTLRenderPipelineReflection* ref;
		id<MTLRenderPipelineState>   pipelineState = [pRenderer->pDevice newRenderPipelineStateWithDescriptor:renderPipelineDesc
                                                                                                    options:MTLPipelineOptionBufferTypeInfo
                                                                                                 reflection:&ref
                                                                                                      error:&error];
		if (!pipelineState)
		{
			NSLog(@ "Error generation render pipeline object: %@", error);
			ASSERT(!"Render pipeline object shouldn't fail to create.");
			return;
		}

		if (shaderStage == SHADER_STAGE_VERT)
		{
			pReflectionInfo = (ShaderReflectionInfo*)conf_calloc(1, sizeof(ShaderReflectionInfo));
			reflectShader(pReflectionInfo, ref.vertexArguments);
		}
		else if (shaderStage == SHADER_STAGE_FRAG)
		{
			pReflectionInfo = (ShaderReflectionInfo*)conf_calloc(1, sizeof(ShaderReflectionInfo));
			reflectShader(pReflectionInfo, ref.fragmentArguments);
		}
		else
		{
			ASSERT(!"No reflection information found in shader!");
		}
	}

	ASSERT(pReflectionInfo != nil);

	// lets find out the size of the name pool we need
	// also get number of resources while we are at it
	uint32_t namePoolSize = calculateNamePoolSize(pReflectionInfo);
	uint32_t resourceCount = 0;
	uint32_t variablesCount = (uint32_t)pReflectionInfo->variableMembers.size();

	eastl::vector<BufferInfo> vertexBuffers;

	for (uint32_t i = 0; i < pReflectionInfo->buffers.size(); ++i)
	{
		const BufferInfo& bufferInfo = pReflectionInfo->buffers[i];
		// The name of the vertex buffers declared as stage_in are internally named by Metal starting with preffix "vertexBuffer."
		if (isInputVertexBuffer(bufferInfo, shaderStage))
		{
			vertexBuffers.push_back(bufferInfo);
		}
        else if (bufferInfo.isArgBuffer)
        {
            // argument buffer
            ++resourceCount;
            
            // iterate over argument buffer fields
            for (uint32_t i = 0; i < pReflectionInfo->variableMembers.size(); ++i)
            {
                const BufferStructMember& bufferMember = pReflectionInfo->variableMembers[i];
                
                if (bufferMember.bufferIndex == bufferInfo.bufferIndex)
                    ++resourceCount;
            }
        }
		else
        {
			++resourceCount;
        }
	}

	resourceCount += pReflectionInfo->textures.size();
	resourceCount += pReflectionInfo->samplers.size();

	// we now have the size of the memory pool and number of resources
	char* namePool = (char*)conf_calloc(namePoolSize, 1);
	char* pCurrentName = namePool;

	// start with the vertex input
	VertexInput*   pVertexInputs = NULL;
	const uint32_t vertexInputCount = (uint32_t)vertexBuffers.size();
	if (shaderStage == SHADER_STAGE_VERT && vertexInputCount > 0)
	{
		pVertexInputs = (VertexInput*)conf_malloc(sizeof(VertexInput) * vertexInputCount);

		for (uint32_t i = 0; i < vertexBuffers.size(); ++i)
		{
			const BufferInfo& vertexBufferInfo = vertexBuffers[i];
			pVertexInputs[i].size = (uint32_t)vertexBufferInfo.sizeInBytes;
			pVertexInputs[i].name = pCurrentName;
			pVertexInputs[i].name_size = (uint32_t)strlen(vertexBufferInfo.name);
			// we don't own the names memory we need to copy it to the name pool
			memcpy(pCurrentName, vertexBufferInfo.name, pVertexInputs[i].name_size);
			pCurrentName += pVertexInputs[i].name_size + 1;
		}
	}

	uint32_t resourceIdxByBufferIdx[MAX_BUFFER_BINDINGS];

	// continue with resources
	ShaderResource* pResources = NULL;
	if (resourceCount > 0)
	{
		pResources = (ShaderResource*)conf_calloc(resourceCount, sizeof(ShaderResource));
		uint32_t resourceIdx = 0;
		for (uint32_t i = 0; i < pReflectionInfo->buffers.size(); ++i)
		{
			const BufferInfo& bufferInfo = pReflectionInfo->buffers[i];
			if (!isInputVertexBuffer(bufferInfo, shaderStage))
			{
				if (bufferInfo.isArgBuffer)
                {
                    // argument buffer info
                    addShaderResource(pResources, resourceIdx, DESCRIPTOR_TYPE_ARGUMENT_BUFFER, bufferInfo.bufferIndex, bufferInfo.sizeInBytes, bufferInfo.alignment, shaderStage, &pCurrentName, (char*)bufferInfo.name);
                    
                    resourceIdxByBufferIdx[bufferInfo.bufferIndex] = resourceIdx++;
                    
                    // argument buffer fields
                    for (uint32_t i = 0; i < pReflectionInfo->variableMembers.size(); ++i)
                    {
                        const BufferStructMember& bufferMember = pReflectionInfo->variableMembers[i];
                        
                        if (bufferMember.bufferIndex == bufferInfo.bufferIndex)
                        {
                            DescriptorType descriptorType;
                            switch (bufferMember.descriptor.mDataType)
                            {
                                case MTLDataTypeTexture:
                                    if (bufferMember.descriptor.mAccessType == MTLArgumentAccessReadOnly)
                                    {
                                        descriptorType = DESCRIPTOR_TYPE_TEXTURE;
                                    }
                                    else
                                    {
                                        descriptorType = DESCRIPTOR_TYPE_RW_TEXTURE;
                                    }
                                    break;
                                case MTLDataTypeSampler:
                                    descriptorType = DESCRIPTOR_TYPE_SAMPLER;
                                    break;
                                case MTLDataTypeArray:
                                case MTLDataTypePointer:
                                    descriptorType = DESCRIPTOR_TYPE_BUFFER;
                                    break;
                                case MTLDataTypeIndirectCommandBuffer:
                                    descriptorType = DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER;
                                    break;
                                case MTLDataTypeRenderPipeline:
                                    descriptorType = DESCRIPTOR_TYPE_RENDER_PIPELINE_STATE;
                                    break;
                                default:
                                    ASSERT(0);
                                    descriptorType = DESCRIPTOR_TYPE_UNDEFINED;
                                    break;
                            }
                            
                            addShaderResource(pResources, resourceIdx, descriptorType, bufferInfo.bufferIndex, bufferMember.sizeInBytes, 0, shaderStage, &pCurrentName, (char*)bufferMember.name);
                            
                            pResources[resourceIdx].mIsArgumentBufferField = true;
                            pResources[resourceIdx].mtlArgumentDescriptors = bufferMember.descriptor;
                            
                            resourceIdx++;
                        }
                    }
                }
                else
                {
                    eastl::string bufferName = bufferInfo.name;
                    bufferName.make_lower();
                    DescriptorType descriptorType = (bufferName.find("rootconstant", 0) != eastl::string::npos ? DESCRIPTOR_TYPE_ROOT_CONSTANT : DESCRIPTOR_TYPE_BUFFER);
                    
                    addShaderResource(pResources, resourceIdx, descriptorType, bufferInfo.bufferIndex, bufferInfo.sizeInBytes, bufferInfo.alignment, shaderStage, &pCurrentName, (char*)bufferInfo.name);
                    
                    pResources[resourceIdx].mIsArgumentBufferField = false;
                    //pResources[resourceIdx].mtlArgumentBufferType = RESOURCE_STATE_UNDEFINED;
                    
                    resourceIdxByBufferIdx[bufferInfo.bufferIndex] = resourceIdx++;
                }
			}
            
            ASSERT(resourceCount + 1 > resourceIdx);
		}
		for (uint32_t i = 0; i < pReflectionInfo->textures.size(); ++i, ++resourceIdx)
		{
			const TextureInfo& texInfo = pReflectionInfo->textures[i];
			addShaderResource(
				pResources, resourceIdx, texInfo.isUAV ? DESCRIPTOR_TYPE_RW_TEXTURE : DESCRIPTOR_TYPE_TEXTURE, texInfo.slotIndex,
				0 /*size*/, 0, shaderStage, &pCurrentName, (char*)texInfo.name);

			pResources[resourceIdx].mtlTextureType = static_cast<uint32_t>(texInfo.type);
            pResources[resourceIdx].mIsArgumentBufferField = false;
			//pResources[resourceIdx].mtlArgumentBufferType = RESOURCE_STATE_UNDEFINED;
		}
		for (uint32_t i = 0; i < pReflectionInfo->samplers.size(); ++i, ++resourceIdx)
		{
			const SamplerInfo& samplerInfo = pReflectionInfo->samplers[i];
			addShaderResource(
				pResources, resourceIdx, DESCRIPTOR_TYPE_SAMPLER, samplerInfo.slotIndex, 0 /*samplerInfo.sizeInBytes*/, 0, shaderStage,
				&pCurrentName, (char*)samplerInfo.name);
			//pResources[resourceIdx].mtlArgumentBufferType = RESOURCE_STATE_UNDEFINED;
            pResources[resourceIdx].mIsArgumentBufferField = false;
		}
	}
    
	ShaderVariable* pVariables = NULL;
	// now do variables
	if (variablesCount > 0)
	{
		pVariables = (ShaderVariable*)conf_malloc(sizeof(ShaderVariable) * variablesCount);
		for (uint32_t i = 0; i < variablesCount; ++i)
		{
			const BufferStructMember& variable = pReflectionInfo->variableMembers[i];

			pVariables[i].offset = (uint32_t)variable.offset;
			pVariables[i].size = variable.sizeInBytes;
			pVariables[i].parent_index = resourceIdxByBufferIdx
				[variable.bufferIndex];    // parent_index is an index into the resources list, not just to the buffers list

			pVariables[i].name = pCurrentName;
			pVariables[i].name_size = (uint32_t)strlen(variable.name);
			// we dont own the names memory we need to copy it to the name pool
			memcpy(pCurrentName, variable.name, pVariables[i].name_size);
			pCurrentName += pVariables[i].name_size + 1;
		}
	}

	// all refection structs should be built now
	pOutReflection->mShaderStage = shaderStage;

	pOutReflection->pNamePool = namePool;
	pOutReflection->mNamePoolSize = namePoolSize;

	pOutReflection->pVertexInputs = pVertexInputs;
	pOutReflection->mVertexInputsCount = vertexInputCount;

	pOutReflection->pShaderResources = pResources;
	pOutReflection->mShaderResourceCount = resourceCount;

	pOutReflection->pVariables = pVariables;
	pOutReflection->mVariableCount = variablesCount;

	pReflectionInfo->variableMembers.~vector();
	pReflectionInfo->buffers.~vector();
	pReflectionInfo->samplers.~vector();
	pReflectionInfo->textures.~vector();
	conf_free(pReflectionInfo);
}
#endif    // #ifdef METAL

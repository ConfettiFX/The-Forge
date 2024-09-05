/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#include "../../Graphics/GraphicsConfig.h"

#ifdef METAL

#include <string.h>

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#include "../../Graphics/Interfaces/IGraphics.h"
#include "../../Utilities/Interfaces/ILog.h"

#include "../../Utilities/Interfaces/IMemory.h"

#define MAX_REFLECT_STRING_LENGTH 128
#define MAX_BUFFER_BINDINGS       31

struct BufferStructMember
{
    char          name[MAX_REFLECT_STRING_LENGTH];
    int           bufferIndex;
    unsigned long offset; // byte offset within the uniform block
    int           sizeInBytes;

    ArgumentDescriptor descriptor;

    BufferStructMember(): bufferIndex(0), offset(0), sizeInBytes(0) { name[0] = '\0'; }
};

struct BufferInfo
{
    char   name[MAX_REFLECT_STRING_LENGTH] = {};
    int    bufferIndex = -1;
    size_t sizeInBytes = 0;
    size_t alignment = 0;
    int    currentOffset = 0;
    bool   isUAV = false;
    bool   isArgBuffer = false;
    bool   isAccelerationStructure = false;
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
    int            arrayLength;
};

struct ShaderReflectionInfo
{
    BufferStructMember* variableMembers;
    BufferInfo*         buffers;
    SamplerInfo*        samplers;
    TextureInfo*        textures;
    BufferInfo*         vertexAttributes;
};

static void freeShaderReflectionInfo(ShaderReflectionInfo* pInfo)
{
    arrfree(pInfo->variableMembers);
    arrfree(pInfo->buffers);
    arrfree(pInfo->samplers);
    arrfree(pInfo->textures);
    arrfree(pInfo->vertexAttributes);
}

int getSizeFromDataType(MTLDataType dataType)
{
    const int HALF_SIZE = 4;
    const int FLOAT_SIZE = 4;
    const int INT_SIZE = 4;
    switch (dataType)
    {
    case MTLDataTypeFloat:
        return FLOAT_SIZE;
    case MTLDataTypeFloat2:
        return FLOAT_SIZE * 2;
    case MTLDataTypeFloat3:
        return FLOAT_SIZE * 3;
    case MTLDataTypeFloat4:
        return FLOAT_SIZE * 4;
    case MTLDataTypeFloat2x2:
        return FLOAT_SIZE * 2 * 2;
    case MTLDataTypeFloat2x3:
        return FLOAT_SIZE * 2 * 3;
    case MTLDataTypeFloat2x4:
        return FLOAT_SIZE * 2 * 4;
    case MTLDataTypeFloat3x2:
        return FLOAT_SIZE * 3 * 2;
    case MTLDataTypeFloat3x3:
        return FLOAT_SIZE * 3 * 3;
    case MTLDataTypeFloat3x4:
        return FLOAT_SIZE * 3 * 4;
    case MTLDataTypeFloat4x2:
        return FLOAT_SIZE * 4 * 2;
    case MTLDataTypeFloat4x3:
        return FLOAT_SIZE * 4 * 3;
    case MTLDataTypeFloat4x4:
        return FLOAT_SIZE * 4 * 4;
    case MTLDataTypeHalf:
        return HALF_SIZE;
    case MTLDataTypeHalf2:
        return HALF_SIZE * 2;
    case MTLDataTypeHalf3:
        return HALF_SIZE * 3;
    case MTLDataTypeHalf4:
        return HALF_SIZE * 4;
    case MTLDataTypeHalf2x2:
        return HALF_SIZE * 2 * 2;
    case MTLDataTypeHalf2x3:
        return HALF_SIZE * 2 * 3;
    case MTLDataTypeHalf2x4:
        return HALF_SIZE * 2 * 4;
    case MTLDataTypeHalf3x2:
        return HALF_SIZE * 3 * 2;
    case MTLDataTypeHalf3x3:
        return HALF_SIZE * 3 * 3;
    case MTLDataTypeHalf3x4:
        return HALF_SIZE * 3 * 4;
    case MTLDataTypeHalf4x2:
        return HALF_SIZE * 4 * 2;
    case MTLDataTypeHalf4x3:
        return HALF_SIZE * 4 * 3;
    case MTLDataTypeHalf4x4:
        return HALF_SIZE * 4 * 4;
    case MTLDataTypeInt:
        return INT_SIZE;
    case MTLDataTypeInt2:
        return INT_SIZE * 2;
    case MTLDataTypeInt3:
        return INT_SIZE * 3;
    case MTLDataTypeInt4:
        return INT_SIZE * 4;
    case MTLDataTypeUInt:
        return INT_SIZE;
    case MTLDataTypeUInt2:
        return INT_SIZE * 2;
    case MTLDataTypeUInt3:
        return INT_SIZE * 3;
    case MTLDataTypeUInt4:
        return INT_SIZE * 4;
    case MTLDataTypeShort:
        return HALF_SIZE;
    case MTLDataTypeShort2:
        return HALF_SIZE * 2;
    case MTLDataTypeShort3:
        return HALF_SIZE * 3;
    case MTLDataTypeShort4:
        return HALF_SIZE * 4;
    case MTLDataTypeUShort:
        return HALF_SIZE;
    case MTLDataTypeUShort2:
        return HALF_SIZE * 2;
    case MTLDataTypeUShort3:
        return HALF_SIZE * 3;
    case MTLDataTypeUShort4:
        return HALF_SIZE * 4;
    case MTLDataTypeChar:
        return 1;
    case MTLDataTypeChar2:
        return 2;
    case MTLDataTypeChar3:
        return 3;
    case MTLDataTypeChar4:
        return 4;
    case MTLDataTypeUChar:
        return 1;
    case MTLDataTypeUChar2:
        return 2;
    case MTLDataTypeUChar3:
        return 3;
    case MTLDataTypeUChar4:
        return 4;
    case MTLDataTypeBool:
        return 1;
    case MTLDataTypeBool2:
        return 2;
    case MTLDataTypeBool3:
        return 3;
    case MTLDataTypeBool4:
        return 4;
    case MTLDataTypeTexture:
        return 8;
    case MTLDataTypePointer:
        return 8;
    case MTLDataTypeSampler:
        return 0;
    case MTLDataTypeIndirectCommandBuffer:
        return 0;
#if defined(MTL_RAYTRACING_AVAILABLE)
    case MTLDataTypeInstanceAccelerationStructure:
    case MTLDataTypePrimitiveAccelerationStructure:
        return 0;
#endif
    case MTLDataTypeRenderPipeline:
        return 0;
    default:
        break;
    }
    ASSERT(0 && "Unknown metal type");
    return -1;
}

inline FORGE_CONSTEXPR TextureDimension getTextureDimFromType(MTLTextureType type)
{
    switch (type)
    {
    case MTLTextureType1D:
        return TEXTURE_DIM_1D;
    case MTLTextureType1DArray:
        return TEXTURE_DIM_1D_ARRAY;
    case MTLTextureType2D:
        return TEXTURE_DIM_2D;
    case MTLTextureType2DArray:
        return TEXTURE_DIM_2D_ARRAY;
    case MTLTextureType2DMultisample:
        return TEXTURE_DIM_2DMS;
    case MTLTextureTypeCube:
        return TEXTURE_DIM_CUBE;
    case MTLTextureType3D:
        return TEXTURE_DIM_3D;
    default:
        break;
    }

    if (MTLTextureTypeCubeArray == type)
    {
        return TEXTURE_DIM_CUBE_ARRAY;
    }
    if (MTLTextureType2DMultisampleArray == type)
    {
        return TEXTURE_DIM_2DMS_ARRAY;
    }

    return TEXTURE_DIM_UNDEFINED;
}

bool startsWith(const char* str, const char* prefix) { return strncmp(prefix, str, strlen(prefix)) == 0; }

bool isInputVertexBuffer(const BufferInfo& bufferInfo, ShaderStage shaderStage)
{
    return (startsWith(bufferInfo.name, "vertexBuffer.") && shaderStage == SHADER_STAGE_VERT);
}

// Returns the total size of the struct
static uint32_t reflectShaderStruct(ShaderReflectionInfo* info, unsigned int bufferIndex, unsigned long parentOffset,
                                    MTLStructType* structObj)
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
        bufferMember.descriptor.mAccessType = MTL_ACCESS_ENUM(ReadOnly);
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
                if (MTLDataTypePointer == member.arrayType.elementType)
                {
                    bufferMember.descriptor.mAccessType = member.arrayType.elementPointerType.access;
                    bufferMember.descriptor.mAlignment = member.arrayType.elementPointerType.alignment;
                }
                else if (MTLDataTypeTexture == member.arrayType.elementType)
                {
                    bufferMember.descriptor.mAccessType = member.arrayType.elementTextureReferenceType.access;
                    bufferMember.descriptor.mTextureType = member.arrayType.elementTextureReferenceType.textureType;
                }
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

        arrpush(info->variableMembers, bufferMember);
        totalSize += bufferMember.sizeInBytes;
    }

    return totalSize;
}

static void reflectShader(ShaderReflectionInfo* info, NSArray<MTLArgument*>* shaderArgs, NSArray<id>* shaderBindings)
{
#ifdef ENABLE_REFLECTION_BINDING_API
    if (IOS17_RUNTIME)
    {
        for (uint32_t argIndex = 0; argIndex < [shaderBindings count]; ++argIndex)
        {
            id<MTLBinding> binding = shaderBindings[argIndex];
            if (binding.type == MTLBindingTypeBuffer)
            {
                id<MTLBufferBinding> binding = shaderBindings[argIndex];
                if (binding.bufferDataType == MTLDataTypeStruct)
                {
                    // We do this for constant buffer initialization. Constant buffers are always defined in structs,
                    // so we only care about structs here.
                    MTLStructType* theStruct = binding.bufferStructType;
                    if (!theStruct)
                    {
                        continue;
                    }
                    reflectShaderStruct(info, (uint32_t)binding.index, 0, theStruct);
                }

                if ([binding.name hasPrefix:@"_fslS"])
                {
                    continue;
                }

                // Reflect buffer info
                BufferInfo bufferInfo = {};
                strlcpy(bufferInfo.name, [binding.name UTF8String], MAX_REFLECT_STRING_LENGTH);
                bufferInfo.bufferIndex = (uint32_t)binding.index;
                bufferInfo.sizeInBytes = binding.bufferDataSize;
                bufferInfo.alignment = binding.bufferAlignment;
                bufferInfo.isUAV = (binding.access == MTL_ACCESS_ENUM(ReadWrite) || binding.access == MTL_ACCESS_ENUM(WriteOnly));
                bufferInfo.isArgBuffer = binding.bufferPointerType.elementIsArgumentBuffer;

                arrpush(info->buffers, bufferInfo);
            }
            else if (binding.type == MTLBindingTypeSampler)
            {
                SamplerInfo samplerInfo;
                strlcpy(samplerInfo.name, [binding.name UTF8String], MAX_REFLECT_STRING_LENGTH);
                samplerInfo.slotIndex = (uint32_t)binding.index;
                arrpush(info->samplers, samplerInfo);
            }
            else if (binding.type == MTLBindingTypeTexture)
            {
                id<MTLTextureBinding> binding = shaderBindings[argIndex];
                TextureInfo           textureInfo;
                strlcpy(textureInfo.name, [binding.name UTF8String], MAX_REFLECT_STRING_LENGTH);
                textureInfo.slotIndex = (uint32_t)binding.index;
                textureInfo.type = binding.textureType;
                textureInfo.isUAV = (binding.access == MTL_ACCESS_ENUM(ReadWrite) || binding.access == MTL_ACCESS_ENUM(WriteOnly));
                textureInfo.arrayLength = (int)binding.arrayLength;
                arrpush(info->textures, textureInfo);
            }
            if (binding.type == MTLBindingTypeInstanceAccelerationStructure || binding.type == MTLBindingTypePrimitiveAccelerationStructure)
            {
                // Reflect buffer info
                BufferInfo bufferInfo = {};
                strlcpy(bufferInfo.name, [binding.name UTF8String], MAX_REFLECT_STRING_LENGTH);
                bufferInfo.bufferIndex = (uint32_t)binding.index;
                bufferInfo.sizeInBytes = 0;
                bufferInfo.alignment = 0;
                bufferInfo.isUAV = (binding.access == MTL_ACCESS_ENUM(ReadWrite) || binding.access == MTL_ACCESS_ENUM(WriteOnly));
                bufferInfo.isAccelerationStructure = true;
                arrpush(info->buffers, bufferInfo);
            }
        }
    }
    else
#endif
    {
        for (uint32_t argIndex = 0; argIndex < [shaderArgs count]; ++argIndex)
        {
            MTLArgument* arg = shaderArgs[argIndex];
            if (arg.type == MTLArgumentTypeBuffer)
            {
                if (arg.bufferDataType == MTLDataTypeStruct)
                {
                    // We do this for constant buffer initialization. Constant buffers are always defined in structs,
                    // so we only care about structs here.
                    MTLStructType* theStruct = arg.bufferStructType;
                    if (!theStruct)
                    {
                        continue;
                    }
                    reflectShaderStruct(info, (uint32_t)arg.index, 0, theStruct);
                }

                if ([arg.name hasPrefix:@"_fslS"])
                {
                    continue;
                }

                // Reflect buffer info
                BufferInfo bufferInfo = {};
                strlcpy(bufferInfo.name, [arg.name UTF8String], MAX_REFLECT_STRING_LENGTH);
                bufferInfo.bufferIndex = (uint32_t)arg.index;
                bufferInfo.sizeInBytes = arg.bufferDataSize;
                bufferInfo.alignment = arg.bufferAlignment;
                bufferInfo.isUAV = (arg.access == MTL_ACCESS_ENUM(ReadWrite) || arg.access == MTL_ACCESS_ENUM(WriteOnly));
                bufferInfo.isArgBuffer = arg.bufferPointerType.elementIsArgumentBuffer;

                arrpush(info->buffers, bufferInfo);
            }
            else if (arg.type == MTLArgumentTypeSampler)
            {
                SamplerInfo samplerInfo;
                strlcpy(samplerInfo.name, [arg.name UTF8String], MAX_REFLECT_STRING_LENGTH);
                samplerInfo.slotIndex = (uint32_t)arg.index;
                arrpush(info->samplers, samplerInfo);
            }
            else if (arg.type == MTLArgumentTypeTexture)
            {
                TextureInfo textureInfo;
                strlcpy(textureInfo.name, [arg.name UTF8String], MAX_REFLECT_STRING_LENGTH);
                textureInfo.slotIndex = (uint32_t)arg.index;
                textureInfo.type = arg.textureType;
                textureInfo.isUAV = (arg.access == MTL_ACCESS_ENUM(ReadWrite) || arg.access == MTL_ACCESS_ENUM(WriteOnly));
                textureInfo.arrayLength = (int)arg.arrayLength;
                arrpush(info->textures, textureInfo);
            }
        }
    }
}

static uint32_t calculateNamePoolSize(const ShaderReflectionInfo* shaderReflectionInfo)
{
    uint32_t namePoolSize = 0;
    for (ptrdiff_t i = 0; i < arrlen(shaderReflectionInfo->variableMembers); i++)
    {
        const BufferStructMember& varMember = shaderReflectionInfo->variableMembers[i];
        namePoolSize += (uint32_t)strlen(varMember.name) + 1;
    }
    for (ptrdiff_t i = 0; i < arrlen(shaderReflectionInfo->buffers); i++)
    {
        const BufferInfo& buffer = shaderReflectionInfo->buffers[i];
        namePoolSize += (uint32_t)strlen(buffer.name) + 1;

        if (buffer.isArgBuffer)
        {
            for (ptrdiff_t i = 0; i < arrlen(shaderReflectionInfo->variableMembers); ++i)
            {
                const BufferStructMember& bufferMember = shaderReflectionInfo->variableMembers[i];

                if (bufferMember.bufferIndex == buffer.bufferIndex)
                {
                    namePoolSize += (uint32_t)strlen(bufferMember.name) + 1;
                }
            }
        }
    }
    for (ptrdiff_t i = 0; i < arrlen(shaderReflectionInfo->textures); i++)
    {
        const TextureInfo& tex = shaderReflectionInfo->textures[i];
        namePoolSize += (uint32_t)strlen(tex.name) + 1;
    }
    for (ptrdiff_t i = 0; i < arrlen(shaderReflectionInfo->samplers); i++)
    {
        const SamplerInfo& sampler = shaderReflectionInfo->samplers[i];
        namePoolSize += (uint32_t)strlen(sampler.name) + 1;
    }
    for (ptrdiff_t i = 0; i < arrlen(shaderReflectionInfo->vertexAttributes); i++)
    {
        const BufferInfo& attr = shaderReflectionInfo->vertexAttributes[i];
        namePoolSize += (uint32_t)strlen(attr.name) + 1;
    }
    return namePoolSize;
}

void addShaderResource(ShaderResource* pResources, uint32_t idx, DescriptorType type, uint32_t bindingPoint, size_t sizeInBytes,
                       size_t alignment, ShaderStage shaderStage, char** ppCurrentName, char* name)
{
    uint32_t set = DESCRIPTOR_UPDATE_FREQ_NONE + 10;
    if (0 == strncmp(name, "_fsl", 4))
    {
        set = name[5] - '0';
        name += 6;
        if (name[-2] == 'A')
        {
            char  arrsize[32] = {};
            char* delimiter = strstr(name, "_");
            strncpy(arrsize, name, delimiter - name);
            sizeInBytes = atoi(arrsize);
            name = delimiter;
        }
        name++;
    }

    pResources[idx].type = type;
    pResources[idx].set = set;
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

void mtl_addShaderReflection(Renderer* pRenderer, Shader* shader, ShaderStage shaderStage, ShaderReflection* pOutReflection)
{
    if (pOutReflection == NULL)
    {
        ASSERT(0);
        return; // TODO: error msg
    }

    NSError* error = nil;

    ShaderReflectionInfo reflectionInfo = {};

    // Setup temporary pipeline to get reflection data
    if (shaderStage == SHADER_STAGE_COMP)
    {
        MTLComputePipelineDescriptor* computePipelineDesc = [[MTLComputePipelineDescriptor alloc] init];
        computePipelineDesc.computeFunction = shader->pComputeShader;

        MTLComputePipelineReflection* ref;
        id<MTLComputePipelineState>   pipelineState =
            [pRenderer->pDevice newComputePipelineStateWithDescriptor:computePipelineDesc
                                                              options:MTLPipelineOptionBufferTypeInfo
                                                           reflection:&ref
                                                                error:&error];

        if (!pipelineState)
        {
            LOGF(LogLevel::eERROR, "Error generation compute pipeline object: %s", [error.description UTF8String]);
            ASSERT(!"Compute pipeline object shouldn't fail to be created.");
            freeShaderReflectionInfo(&reflectionInfo);
            return;
        }

#ifdef ENABLE_REFLECTION_BINDING_API
        if (IOS17_RUNTIME)
        {
            reflectShader(&reflectionInfo, NULL, ref.bindings);
        }
        else
#endif // ENABLE_REFLECTION_BINDING_API
        {
            reflectShader(&reflectionInfo, ref.arguments, NULL);
        }

        shader->mNumThreadsPerGroup[0] = pOutReflection->mNumThreadsPerGroup[0];
        shader->mNumThreadsPerGroup[1] = pOutReflection->mNumThreadsPerGroup[1];
        shader->mNumThreadsPerGroup[2] = pOutReflection->mNumThreadsPerGroup[2];
    }
    else
    {
        MTLRenderPipelineDescriptor* renderPipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];

        renderPipelineDesc.vertexFunction = shader->pVertexShader;
        renderPipelineDesc.fragmentFunction = shader->pFragmentShader;

        uint maxColorAttachments = MAX_RENDER_TARGET_ATTACHMENTS;
#ifdef TARGET_IOS
        if (![pRenderer->pDevice supportsFamily:MTLGPUFamilyApple2])
            maxColorAttachments = 4;
#endif
        for (uint i = 0; i < maxColorAttachments; i++)
        {
            renderPipelineDesc.colorAttachments[i].pixelFormat =
                ((pOutReflection->mOutputRenderTargetTypesMask & (1 << i)) != 0) ? MTLPixelFormatR8Uint : MTLPixelFormatR8Unorm;
        }
        renderPipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

        // We need to create a vertex descriptor if needed to obtain reflection information
        // We are forced to initialize the vertex descriptor with dummy information just to get
        // the reflection information.
        MTLVertexDescriptor* vertexDesc = [[MTLVertexDescriptor alloc] init];

        for (uint32_t i = 0; i < shader->pVertexShader.vertexAttributes.count; ++i)
        {
            MTLVertexAttribute* attr = shader->pVertexShader.vertexAttributes[i];
            MTLDataType         type = attr.attributeType;
            uint32_t            index = (uint32_t)attr.attributeIndex;

            BufferInfo attrRef = {};
            attrRef.bufferIndex = index;
            strlcpy(attrRef.name, [attr.name UTF8String], MAX_REFLECT_STRING_LENGTH);
            attrRef.sizeInBytes = getSizeFromDataType(type);
            arrpush(reflectionInfo.vertexAttributes, attrRef);

            switch (type)
            {
            case MTLDataTypeFloat:
            case MTLDataTypeFloat2:
            case MTLDataTypeFloat3:
            case MTLDataTypeFloat4:
            case MTLDataTypeHalf:
            case MTLDataTypeHalf2:
            case MTLDataTypeHalf3:
            case MTLDataTypeHalf4:
                vertexDesc.attributes[index].format = MTLVertexFormatFloat;
                break;
            case MTLDataTypeInt:
            case MTLDataTypeInt2:
            case MTLDataTypeInt3:
            case MTLDataTypeInt4:
            case MTLDataTypeShort:
            case MTLDataTypeShort2:
            case MTLDataTypeShort3:
            case MTLDataTypeShort4:
                vertexDesc.attributes[index].format = MTLVertexFormatInt;
                break;
            case MTLDataTypeUInt:
            case MTLDataTypeUInt2:
            case MTLDataTypeUInt3:
            case MTLDataTypeUInt4:
            case MTLDataTypeUShort:
            case MTLDataTypeUShort2:
            case MTLDataTypeUShort3:
            case MTLDataTypeUShort4:
                vertexDesc.attributes[index].format = MTLVertexFormatUInt;
                break;
            default:
                vertexDesc.attributes[index].format = MTLVertexFormatFloat;
                break;
            }

            vertexDesc.attributes[index].offset = 0;
            vertexDesc.attributes[index].bufferIndex = 0;
        }

        vertexDesc.layouts[0].stride = MAX_VERTEX_ATTRIBS * sizeof(float);
        vertexDesc.layouts[0].stepRate = 1;
        vertexDesc.layouts[0].stepFunction = shader->pVertexShader.patchType != MTLPatchTypeNone ? MTLVertexStepFunctionPerPatchControlPoint
                                                                                                 : MTLVertexStepFunctionPerVertex;

        renderPipelineDesc.vertexDescriptor = vertexDesc;

        // added dummy topology
        // for layered rendering, metal requires the primitive topology beforehand.
        // if no prim. topo. is specified and shader reflection finds 'render_target_array_index'
        // pipelineState won't be created.
        renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassPoint;

        MTLRenderPipelineReflection* ref;
        id<MTLRenderPipelineState>   pipelineState = [pRenderer->pDevice newRenderPipelineStateWithDescriptor:renderPipelineDesc
                                                                                                    options:MTLPipelineOptionBufferTypeInfo
                                                                                                 reflection:&ref
                                                                                                      error:&error];
        if (!pipelineState)
        {
            LOGF(LogLevel::eERROR, "Error generation render pipeline object: %s", [error.description UTF8String]);
            ASSERT(!"Render pipeline object shouldn't fail to create.");
            freeShaderReflectionInfo(&reflectionInfo);
            return;
        }

#ifdef ENABLE_REFLECTION_BINDING_API
        if (IOS17_RUNTIME)
        {
            if (shaderStage == SHADER_STAGE_VERT)
            {
                reflectShader(&reflectionInfo, NULL, ref.vertexBindings);
            }
            else if (shaderStage == SHADER_STAGE_FRAG)
            {
                reflectShader(&reflectionInfo, NULL, ref.fragmentBindings);
            }
            else
            {
                ASSERT(!"No reflection information found in shader!");
            }
        }
        else
#endif // ENABLE_REFLECTION_BINDING_API
        {
            if (shaderStage == SHADER_STAGE_VERT)
            {
                reflectShader(&reflectionInfo, ref.vertexArguments, NULL);
            }
            else if (shaderStage == SHADER_STAGE_FRAG)
            {
                reflectShader(&reflectionInfo, ref.fragmentArguments, NULL);
            }
            else
            {
                ASSERT(!"No reflection information found in shader!");
            }
        }
    }

    // lets find out the size of the name pool we need
    // also get number of resources while we are at it
    uint32_t namePoolSize = calculateNamePoolSize(&reflectionInfo);
    uint32_t resourceCount = 0;
    uint32_t variablesCount = (uint32_t)arrlen(reflectionInfo.variableMembers);

    for (ptrdiff_t i = 0; i < arrlen(reflectionInfo.buffers); ++i)
    {
        const BufferInfo& bufferInfo = reflectionInfo.buffers[i];
        // The name of the vertex buffers declared as stage_in are internally named by Metal starting with prefix "vertexBuffer."
        ASSERT(!isInputVertexBuffer(bufferInfo, shaderStage));

        if (bufferInfo.isArgBuffer)
        {
            // argument buffer
            ++resourceCount;

            // iterate over argument buffer fields
            for (ptrdiff_t i = 0; i < arrlen(reflectionInfo.variableMembers); ++i)
            {
                const BufferStructMember& bufferMember = reflectionInfo.variableMembers[i];

                if (bufferMember.bufferIndex == bufferInfo.bufferIndex)
                    ++resourceCount;
            }
        }
        else
        {
            ++resourceCount;
        }
    }

    resourceCount += (uint32_t)arrlen(reflectionInfo.textures);
    resourceCount += (uint32_t)arrlen(reflectionInfo.samplers);

    // we now have the size of the memory pool and number of resources
    char* namePool = (char*)tf_calloc(namePoolSize, 1);
    char* pCurrentName = namePool;

    // start with the vertex input
    VertexInput*   pVertexInputs = NULL;
    const uint32_t vertexInputCount = (uint32_t)arrlen(reflectionInfo.vertexAttributes);
    if (shaderStage == SHADER_STAGE_VERT && vertexInputCount > 0)
    {
        pVertexInputs = (VertexInput*)tf_malloc(sizeof(VertexInput) * vertexInputCount);

        for (uint32_t i = 0; i < arrlen(reflectionInfo.vertexAttributes); ++i)
        {
            const BufferInfo& vertexBufferInfo = reflectionInfo.vertexAttributes[i];
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
        pResources = (ShaderResource*)tf_calloc(resourceCount, sizeof(ShaderResource));
        uint32_t resourceIdx = 0;
        for (ptrdiff_t i = 0; i < arrlen(reflectionInfo.buffers); ++i)
        {
            const BufferInfo& bufferInfo = reflectionInfo.buffers[i];
            if (!isInputVertexBuffer(bufferInfo, shaderStage))
            {
                if (bufferInfo.isArgBuffer)
                {
                    // argument buffer info
                    addShaderResource(pResources, resourceIdx, DESCRIPTOR_TYPE_UNDEFINED, bufferInfo.bufferIndex, bufferInfo.sizeInBytes,
                                      bufferInfo.alignment, shaderStage, &pCurrentName, (char*)bufferInfo.name);

                    resourceIdxByBufferIdx[bufferInfo.bufferIndex] = resourceIdx++;

                    // argument buffer fields
                    for (ptrdiff_t i = 0; i < arrlen(reflectionInfo.variableMembers); ++i)
                    {
                        const BufferStructMember& bufferMember = reflectionInfo.variableMembers[i];

                        if (bufferMember.bufferIndex == bufferInfo.bufferIndex)
                        {
                            DescriptorType descriptorType;
                            switch (bufferMember.descriptor.mDataType)
                            {
                            case MTLDataTypeTexture:
                                if (bufferMember.descriptor.mAccessType == MTL_ACCESS_ENUM(ReadOnly))
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
#if defined(MTL_RAYTRACING_AVAILABLE)
                            case MTLDataTypeInstanceAccelerationStructure:
                            case MTLDataTypePrimitiveAccelerationStructure:
                                descriptorType = DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE;
                                break;
#endif
                            default:
                                ASSERT(0);
                                descriptorType = DESCRIPTOR_TYPE_UNDEFINED;
                                break;
                            }

                            addShaderResource(pResources, resourceIdx, descriptorType, bufferInfo.bufferIndex, bufferMember.sizeInBytes, 0,
                                              shaderStage, &pCurrentName, (char*)bufferMember.name);

                            pResources[resourceIdx].mIsArgumentBufferField = true;
                            pResources[resourceIdx].mArgumentDescriptor = bufferMember.descriptor;
                            pResources[resourceIdx].dim = getTextureDimFromType(bufferMember.descriptor.mTextureType);

                            resourceIdx++;
                        }
                    }
                }
                else
                {
                    DescriptorType descriptorType =
                        isDescriptorRootConstant(bufferInfo.name) ? DESCRIPTOR_TYPE_ROOT_CONSTANT : DESCRIPTOR_TYPE_BUFFER;
                    descriptorType = bufferInfo.isAccelerationStructure ? DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE : descriptorType;

                    addShaderResource(pResources, resourceIdx, descriptorType, bufferInfo.bufferIndex, bufferInfo.sizeInBytes,
                                      bufferInfo.alignment, shaderStage, &pCurrentName, (char*)bufferInfo.name);

                    pResources[resourceIdx].mIsArgumentBufferField = false;
                    // pResources[resourceIdx].pArgumentBufferType = RESOURCE_STATE_UNDEFINED;

                    resourceIdxByBufferIdx[bufferInfo.bufferIndex] = resourceIdx++;
                }
            }

            ASSERT(resourceCount + 1 > resourceIdx);
        }
        for (ptrdiff_t i = 0; i < arrlen(reflectionInfo.textures); ++i, ++resourceIdx)
        {
            const TextureInfo& texInfo = reflectionInfo.textures[i];
            addShaderResource(pResources, resourceIdx, texInfo.isUAV ? DESCRIPTOR_TYPE_RW_TEXTURE : DESCRIPTOR_TYPE_TEXTURE,
                              texInfo.slotIndex, texInfo.arrayLength, 0, shaderStage, &pCurrentName, (char*)texInfo.name);

            pResources[resourceIdx].dim = getTextureDimFromType(texInfo.type);
            pResources[resourceIdx].mIsArgumentBufferField = false;
            // pResources[resourceIdx].pArgumentBufferType = RESOURCE_STATE_UNDEFINED;
        }
        for (ptrdiff_t i = 0; i < arrlen(reflectionInfo.samplers); ++i, ++resourceIdx)
        {
            const SamplerInfo& samplerInfo = reflectionInfo.samplers[i];
            addShaderResource(pResources, resourceIdx, DESCRIPTOR_TYPE_SAMPLER, samplerInfo.slotIndex, 0 /*samplerInfo.sizeInBytes*/, 0,
                              shaderStage, &pCurrentName, (char*)samplerInfo.name);
            // pResources[resourceIdx].pArgumentBufferType = RESOURCE_STATE_UNDEFINED;
            pResources[resourceIdx].mIsArgumentBufferField = false;
        }
    }

    ShaderVariable* pVariables = NULL;
    // now do variables
    if (variablesCount > 0)
    {
        pVariables = (ShaderVariable*)tf_malloc(sizeof(ShaderVariable) * variablesCount);
        for (uint32_t i = 0; i < variablesCount; ++i)
        {
            const BufferStructMember& variable = reflectionInfo.variableMembers[i];

            pVariables[i].offset = (uint32_t)variable.offset;
            pVariables[i].size = variable.sizeInBytes;
            pVariables[i].parent_index = resourceIdxByBufferIdx[variable.bufferIndex]; // parent_index is an index into the resources list,
                                                                                       // not just to the buffers list

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
    freeShaderReflectionInfo(&reflectionInfo);
}
#endif // #ifdef METAL

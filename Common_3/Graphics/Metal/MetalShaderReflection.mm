/*
 * Copyright (c) 2017-2025 The Forge Interactive Inc.
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

#endif // #ifdef METAL

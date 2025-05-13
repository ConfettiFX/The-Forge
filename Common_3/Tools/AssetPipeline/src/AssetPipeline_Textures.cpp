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

#include "../../../OS/Interfaces/IOperatingSystem.h"
#include "../../../Utilities/Interfaces/IFileSystem.h"
#include "../../../Utilities/Interfaces/ILog.h"
#include "../../../Utilities/Interfaces/IToolFileSystem.h"

#include "AssetPipeline.h"

// Math
#include "../../../Utilities/ThirdParty/OpenSource/ModifiedSonyMath/vectormath.hpp"

#include "../../../Resources/ResourceLoader/TextureContainers.h"

// TinyImage
#include "../../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"
#include "../../../Resources/ResourceLoader/ThirdParty/OpenSource/tinydds/tinydds.h"
#include "../../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyktx/tinyktx.h"

// ISPC texcomp
#include "../../../../Common_3/Tools/ThirdParty/OpenSource/ISPCTextureCompressor/ispc_texcomp/ispc_texcomp.h"

// Nothings
#define STBI_NO_STDIO
#define STBI_ASSERT(x)         ASSERT(x)
#define STBI_MALLOC(sz)        tf_malloc(sz)
#define STBI_REALLOC(p, newsz) tf_realloc(p, newsz)
#define STBI_FREE(p)           tf_free(p)
#define STB_IMAGE_IMPLEMENTATION
#include "../../../Utilities/ThirdParty/OpenSource/Nothings/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_ASSERT(x)     ASSERT(x)
// Note: We don't use custom allocation contexts for stbi
#define STBIR_MALLOC(sz, c) ((c) == NULL) ? tf_malloc(sz) : NULL
#define STBIR_FREE(p, c)    ((c) == NULL) ? tf_free(p) : ((void)0)
#include "../../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../../Utilities/ThirdParty/OpenSource/Nothings/stb_image_resize.h"

#include "../../../Utilities/Interfaces/IMemory.h" //NOTE: this should be the last include in a .cpp

#define IS_POWER_OF_TWO(x) ((x) != 0 && ((x) & ((x)-1)) == 0)

const char* gExtensions[] = { "dds", "ktx"
#ifdef PROSPERO_GNF
                              ,
                              "gnf", "gnf"
#endif
#ifdef XBOX_SCARLETT_DDS
                              ,
                              "dds"
#endif
};

TinyDDS_WriteCallbacks ddsWriteCallbacks{ [](void* user, char const* msg)
                                          {
                                              UNREF_PARAM(user);
                                              LOGF(eERROR, "%s", msg);
                                          },
                                          [](void* user, size_t size)
                                          {
                                              UNREF_PARAM(user);
                                              return tf_malloc(size);
                                          },
                                          [](void* user, void* memory)
                                          {
                                              UNREF_PARAM(user);
                                              tf_free(memory);
                                          },
                                          [](void* user, const void* buffer, size_t byteCount)
                                          { fsWriteToStream((FileStream*)user, buffer, (ssize_t)byteCount); } };

TinyDDS_Callbacks ddsReadCallbacks{ [](void* user, char const* msg)
                                    {
                                        UNREF_PARAM(user);
                                        LOGF(eERROR, "%s", msg);
                                    },
                                    [](void* user, size_t size)
                                    {
                                        UNREF_PARAM(user);
                                        return tf_malloc(size);
                                    },
                                    [](void* user, void* memory)
                                    {
                                        UNREF_PARAM(user);
                                        tf_free(memory);
                                    },
                                    [](void* user, void* buffer, size_t byteCount)
                                    { return fsReadFromStream((FileStream*)user, buffer, (ssize_t)byteCount); },
                                    [](void* user, int64_t offset)
                                    { return fsSeekStream((FileStream*)user, SBO_START_OF_FILE, (ssize_t)offset); },
                                    [](void* user) { return (int64_t)fsGetStreamSeekPosition((FileStream*)user); } };

TinyKtx_WriteCallbacks ktxWriteCallbacks{ [](void* user, char const* msg)
                                          {
                                              UNREF_PARAM(user);
                                              LOGF(eERROR, "%s", msg);
                                          },
                                          [](void* user, size_t size)
                                          {
                                              UNREF_PARAM(user);
                                              return tf_malloc(size);
                                          },
                                          [](void* user, void* memory)
                                          {
                                              UNREF_PARAM(user);
                                              tf_free(memory);
                                          },
                                          [](void* user, const void* buffer, size_t byteCount)
                                          { fsWriteToStream((FileStream*)user, buffer, (ssize_t)byteCount); } };

TinyKtx_Callbacks ktxReadCallbacks{ [](void* user, char const* msg)
                                    {
                                        UNREF_PARAM(user);
                                        LOGF(eERROR, "%s", msg);
                                    },
                                    [](void* user, size_t size)
                                    {
                                        UNREF_PARAM(user);
                                        return tf_malloc(size);
                                    },
                                    [](void* user, void* memory)
                                    {
                                        UNREF_PARAM(user);
                                        tf_free(memory);
                                    },
                                    [](void* user, void* buffer, size_t byteCount)
                                    { return fsReadFromStream((FileStream*)user, buffer, (ssize_t)byteCount); },
                                    [](void* user, int64_t offset)
                                    { return fsSeekStream((FileStream*)user, SBO_START_OF_FILE, (ssize_t)offset); },
                                    [](void* user) { return (int64_t)fsGetStreamSeekPosition((FileStream*)user); } };

typedef enum TextureDataType
{
    TEX_DATA_TYPE_U8 = 0,
    TEX_DATA_TYPE_U16 = 1,
    TEX_DATA_TYPE_F16 = 2,
    TEX_DATA_TYPE_F32 = 3
} TextureDataType;

typedef struct InputTextureData
{
    TextureDesc     mDesc;
    void*           pData[MAX_MIPLEVELS];
    uint32_t        mDataSize[MAX_MIPLEVELS];
    TextureDataType mDataType;
    bool            isCompressed;
} InputTextureData;

typedef struct CompressImageDescriptor
{
    TextureCompression mCompression;
    ASTC               mASTCCompression;
    DXT                mDXTCompression;
} CompressImageDescriptor;

uint8_t* ResizeImage(uint8_t* ppData, const uint32_t width, const uint32_t height, const uint32_t newWidth, const uint32_t newHeight,
                     const uint32_t components, const uint32_t mip)
{
    uint8_t* pResizedImageData = (uint8_t*)tf_malloc(newWidth * newHeight * components);
    // stbir_resize_uint8(ppData, width, height, width * components, pResizedImageData, newWidth, newHeight, newWidth * components,
    // components);
    stbir_resize_uint8_srgb_edgemode(ppData, width, height, width * components, pResizedImageData, newWidth, newHeight,
                                     newWidth * components, components, 1, 0, stbir_edge::STBIR_EDGE_CLAMP);
    LOGF(eINFO, "Resized image %ux%u -> %ux%u (%ux%u)", width, height, newWidth, newHeight, newWidth << mip, newHeight << mip);
    return pResizedImageData;
}

TinyImageFormat GetASTCFormat(ASTC astc, bool isSrgb, bool isFloat)
{
    TinyImageFormat outFormat = TinyImageFormat_UNDEFINED;
    switch (astc)
    {
    case ASTC_4x4:
    case ASTC_4x4_SLOW:
        outFormat = isFloat ? TinyImageFormat_ASTC_4x4_SFLOAT : (isSrgb ? TinyImageFormat_ASTC_4x4_SRGB : TinyImageFormat_ASTC_4x4_UNORM);
        break;
    case ASTC_5x4:
    case ASTC_5x4_SLOW:
        outFormat = isFloat ? TinyImageFormat_ASTC_5x4_SFLOAT : (isSrgb ? TinyImageFormat_ASTC_5x4_SRGB : TinyImageFormat_ASTC_5x4_UNORM);
        break;
    case ASTC_5x5:
    case ASTC_5x5_SLOW:
        outFormat = isFloat ? TinyImageFormat_ASTC_5x5_SFLOAT : (isSrgb ? TinyImageFormat_ASTC_5x5_SRGB : TinyImageFormat_ASTC_5x5_UNORM);
        break;
    case ASTC_6x6:
    case ASTC_6x6_SLOW:
        outFormat = isFloat ? TinyImageFormat_ASTC_6x6_SFLOAT : (isSrgb ? TinyImageFormat_ASTC_6x6_SRGB : TinyImageFormat_ASTC_6x6_UNORM);
        break;
    case ASTC_8x5:
    case ASTC_8x5_SLOW:
        outFormat = isFloat ? TinyImageFormat_ASTC_8x5_SFLOAT : (isSrgb ? TinyImageFormat_ASTC_8x5_SRGB : TinyImageFormat_ASTC_8x5_UNORM);
        break;
    case ASTC_8x6:
    case ASTC_8x6_SLOW:
        outFormat = isFloat ? TinyImageFormat_ASTC_8x6_SFLOAT : (isSrgb ? TinyImageFormat_ASTC_8x6_SRGB : TinyImageFormat_ASTC_8x6_UNORM);
        break;
    case ASTC_8x8:
    case ASTC_8x8_SLOW:
        outFormat = isFloat ? TinyImageFormat_ASTC_8x8_SFLOAT : (isSrgb ? TinyImageFormat_ASTC_8x8_SRGB : TinyImageFormat_ASTC_8x8_UNORM);
        break;
    default:
        LOGF(eERROR, "Unknown ASTC compression!");
    }
    return outFormat;
}

TinyImageFormat GetBCFormat(DXT dxt, uint32_t channels, bool isSrgb, bool isSigned)
{
    TinyImageFormat outFormat = TinyImageFormat_UNDEFINED;
    switch (dxt)
    {
    case DXT_BC1:
        outFormat = isSrgb ? (channels < 4 ? TinyImageFormat_DXBC1_RGB_SRGB : TinyImageFormat_DXBC1_RGBA_SRGB)
                           : (channels < 4 ? TinyImageFormat_DXBC1_RGB_UNORM : TinyImageFormat_DXBC1_RGBA_UNORM);
        break;
    case DXT_BC3:
        outFormat = isSrgb ? TinyImageFormat_DXBC3_SRGB : TinyImageFormat_DXBC3_UNORM;
        break;
    case DXT_BC4:
        outFormat = isSigned ? TinyImageFormat_DXBC4_SNORM : TinyImageFormat_DXBC4_UNORM;
        break;
    case DXT_BC5:
        outFormat = isSigned ? TinyImageFormat_DXBC5_SNORM : TinyImageFormat_DXBC5_UNORM;
        break;
    case DXT_BC6:
        outFormat = isSigned ? TinyImageFormat_DXBC6H_SFLOAT : TinyImageFormat_DXBC6H_UFLOAT;
        break;
    case DXT_BC7:
        outFormat = isSrgb ? TinyImageFormat_DXBC7_SRGB : TinyImageFormat_DXBC7_UNORM;
        break;
    default:
        LOGF(eERROR, "Unknown DXT compression!");
    }

    return outFormat;
}

TinyImageFormat GetOutputTextureFormat(ProcessTexturesParams* pTexturesParams, TextureDesc* pDesc,
                                       CompressImageDescriptor* pOutCompressImageDescriptor)
{
    const uint32_t  channels = TinyImageFormat_ChannelCount(pDesc->mFormat);
    TinyImageFormat outFormat = TinyImageFormat_UNDEFINED;
    bool            isSigned = TinyImageFormat_IsSigned(pDesc->mFormat);
    bool            isSrgb = TinyImageFormat_IsSRGB(pDesc->mFormat);
    bool            isFloat = TinyImageFormat_IsFloat(pDesc->mFormat);

    ASTC astcCompression = ASTC_NONE;
    DXT  dxtCompression = DXT_NONE;

    switch (pTexturesParams->mCompression)
    {
    case COMPRESSION_ASTC:
        if (pTexturesParams->mOverrideASTC == ASTC_NONE)
        {
            if (channels == 1)
            {
                astcCompression = ASTC_6x6; // Eg. Grayscale images. 3.56 bpp
            }
            else if (channels == 2)
            {
                astcCompression = ASTC_4x4; // Eg. Normal maps where xy is stored and z reconstructed. 8 bpp
            }
            else if (channels == 3)
            {
                astcCompression = ASTC_6x6; // Eg. RGB textures with potential for a single bit to store alpha. 3.56 bpp
            }
            else if (channels == 4)
            {
                astcCompression = ASTC_4x4; // Eg. Full RGBA Textures. 8 bpp
            }
        }
        else
        {
            astcCompression = pTexturesParams->mOverrideASTC;
        }
        outFormat = GetASTCFormat(astcCompression, isSrgb, isFloat);
        break;
    case COMPRESSION_BC:
        if (pTexturesParams->mOverrideBC == DXT_NONE)
        {
            if (channels == 1)
            {
                dxtCompression = DXT_BC4; // Eg. Grayscale images. 8 bytes per block
            }
            else if (channels == 2)
            {
                dxtCompression = DXT_BC5; // Eg. Normal maps where xy is stored and z reconstructed. 16 bytes per block
            }
            else if (channels == 3)
            {
                if (isFloat)
                {
                    dxtCompression = DXT_BC6; // Eg. HDR textures with 16:16:16 components. 16 bytes per block
                }
                else
                {
                    dxtCompression = DXT_BC1; // Eg. RGB textures with potential for a single bit to store alpha. 8 bytes per block
                }
            }
            else if (channels == 4)
            {
                dxtCompression = DXT_BC7; // Eg. Full RGBA Textures. 16 bytes per block
            }
        }
        else
        {
            dxtCompression = pTexturesParams->mOverrideBC;
        }
        outFormat = GetBCFormat(dxtCompression, channels, isSrgb, isSigned);
        break;
    default:
        outFormat = pDesc->mFormat;
        break;
    }

    pOutCompressImageDescriptor->mCompression = pTexturesParams->mCompression;
    pOutCompressImageDescriptor->mASTCCompression = astcCompression;
    pOutCompressImageDescriptor->mDXTCompression = dxtCompression;
    return outFormat;
}

TinyImageFormat TextureFormatFromImageInfo(uint32_t componentCount, uint32_t bitDepth, bool srgb, bool hdr)
{
    if (bitDepth == 32)
    {
        switch (componentCount)
        {
        case 1:
            return TinyImageFormat_R32_SFLOAT;
        case 2:
            return TinyImageFormat_R32G32_SFLOAT;
        case 3:
            return TinyImageFormat_R32G32B32_SFLOAT;
        case 4:
            return TinyImageFormat_R32G32B32A32_SFLOAT;
        }
    }
    else if (bitDepth == 16)
    {
        switch (componentCount)
        {
        case 1:
            return hdr ? TinyImageFormat_R16_SFLOAT : TinyImageFormat_R16_UNORM;
        case 2:
            return hdr ? TinyImageFormat_R16G16_SFLOAT : TinyImageFormat_R16G16_UNORM;
        case 3:
            return hdr ? TinyImageFormat_R16G16B16_SFLOAT : TinyImageFormat_R16G16B16_UNORM;
        case 4:
            return hdr ? TinyImageFormat_R16G16B16A16_SFLOAT : TinyImageFormat_R16G16B16A16_UNORM;
        }
    }
    else if (bitDepth == 8)
    {
        switch (componentCount)
        {
        case 1:
            return srgb ? TinyImageFormat_R8_SRGB : TinyImageFormat_R8_UNORM;
        case 2:
            return srgb ? TinyImageFormat_R8G8_SRGB : TinyImageFormat_R8G8_UNORM;
        case 3:
            return srgb ? TinyImageFormat_R8G8B8_SRGB : TinyImageFormat_R8G8B8_UNORM;
        case 4:
            return srgb ? TinyImageFormat_R8G8B8A8_SRGB : TinyImageFormat_R8G8B8A8_UNORM;
        }
    }
    return TinyImageFormat_UNDEFINED;
}

bool LoadTextureData(ResourceDirectory resourceDir, const char* pFilepath, const char* pExtension, ProcessTexturesParams* pTextureParams,
                     InputTextureData* pOut)
{
    ASSERT(pFilepath);
    ASSERT(pOut);
    ASSERT(pExtension);
    ASSERT(pTextureParams);

    bool success = true;

    // Read input file
    FileStream file = {};
    if (!fsOpenStreamFromPath(resourceDir, pFilepath, FileMode(FM_READ | FM_ALLOW_READ), &file))
    {
        LOGF(eERROR, "Could not open file '%s'.", pFilepath);
        success = false;
        return success;
    }

    ssize_t fileSize = fsGetStreamFileSize(&file);
    if (fileSize > INT32_MAX)
    {
        LOGF(eWARNING, "File '%s' exceeds max handled filesize, size: %zi max: %i", pFilepath, fileSize, INT32_MAX);
        success = false;
    }
    if (STRCMP(pExtension, gExtensions[CONTAINER_KTX]))
    {
        LOGF(LogLevel::eERROR, "Unsupported input container format. KTX is not supported as an input format");
        return false;
    }
    else if (STRCMP(pExtension, gExtensions[CONTAINER_DDS]))
    {
        LOGF(LogLevel::eERROR, "Unsupported input container format. DDS is not supported as an input format");
        return false;
    }
    else
    {
        uint8_t* pFileData = (uint8_t*)tf_malloc(fileSize);
        ssize_t  fileReadBytes = fsReadFromStream(&file, pFileData, fileSize);

        if (fileSize != fileReadBytes)
        {
            LOGF(eERROR, "Could not read all bytes from file '%s' %zi/%zi", pFilepath, fileReadBytes, fileSize);
            success = false;
        }

        int32_t componentCount = 0;
        int32_t forceComponents = 0; // Optional to foce loading a certain amount of channels
        int32_t bitDepth = 8;
        bool    isHdr = false;

        int32_t imageWidth = 0;
        int32_t imageHeight = 0;
        int32_t imageDepth = 1; // TODO: support loading 3D images stbi_image seems not to support this?
        stbi_info_from_memory(pFileData, (int32_t)fileSize, &imageWidth, &imageHeight, &componentCount);

        forceComponents = componentCount;

        if (pTextureParams->mInputLinearColorSpace)
        {
            // BC compression (or uncompressed) doesn't need to have forced channels.
            // Unless we need to do BC1 compression; ISPC Texture Compressor expects 32bit/pixel for BC1
            if (pTextureParams->mOverrideBC == DXT_BC1)
            {
                forceComponents = 4;
            }

            if (forceComponents < pTextureParams->mSwizzleChannelCount)
            {
                // If input swizzling channels is more than what the image has, also extend the channels needed to be loaded
                forceComponents = pTextureParams->mSwizzleChannelCount;
            }
        }
        else
        {
            // sRGB Color Space
            // DDS container srgb only supports 4 components
            // KTX container with srgb doesn't support 3 components (at least gives some issues on Android)
            forceComponents = 4;
        }

        if (pTextureParams->mProcessAsNormalMap)
        {
            if (componentCount != 2)
            {
                LOGF(LogLevel::eERROR, "Unsupport component count for Normal maps. Expecting a 2 Component normal map");
                success = false;
            }
        }

        // Handle color swizzling explicitly for ASTC compression.
        // We always need 4 components, but it's swizzled optimally for the compression
        if (pTextureParams->mCompression == TextureCompression::COMPRESSION_ASTC)
        {
            forceComponents = 4;
            pTextureParams->mSwizzleChannelCount = 4;
            switch (componentCount)
            {
            case 1:
                pTextureParams->mSwizzle = { 'x', 'x', 'x', '1' };
                break;
            case 2:
                pTextureParams->mSwizzle = { 'x', 'x', 'x', 'y' };
                break;
            case 3:
                pTextureParams->mSwizzle = { 'x', 'y', 'z', '1' };
                break;
            case 4:
                pTextureParams->mSwizzle = { 'x', 'y', 'z', 'a' };
                break;
            }
        }

        if (stbi_is_hdr_from_memory(pFileData, (int32_t)fileSize))
        {
            isHdr = true;
            bitDepth = 32;
        }

        if (stbi_is_16_bit_from_memory(pFileData, (int32_t)fileSize))
        {
            bitDepth = 16;
        }

        TinyImageFormat textureFormat =
            TextureFormatFromImageInfo(forceComponents, bitDepth, !pTextureParams->mInputLinearColorSpace, isHdr);

        if (textureFormat == TinyImageFormat_UNDEFINED)
        {
            LOGF(LogLevel::eERROR, "Cannot process texture with texure format UNDEFINED");
            success = false;
        }
        uint32_t bpc = 1;

        if (success)
        {
            pOut->pData[0] = NULL;
            if (bitDepth == 32)
            {
                pOut->pData[0] =
                    stbi_loadf_from_memory(pFileData, (int32_t)fileSize, &imageWidth, &imageHeight, &componentCount, forceComponents);
                pOut->mDataType = TEX_DATA_TYPE_F32;
            }
            else if (bitDepth == 16)
            {
                pOut->pData[0] =
                    stbi_load_16_from_memory(pFileData, (int32_t)fileSize, &imageWidth, &imageHeight, &componentCount, forceComponents);
                pOut->mDataType = TEX_DATA_TYPE_U16;
            }
            else
            {
                pOut->pData[0] =
                    stbi_load_from_memory(pFileData, (int32_t)fileSize, &imageWidth, &imageHeight, &componentCount, forceComponents);
                pOut->mDataType = TEX_DATA_TYPE_U8;
            }

            bpc = bitDepth / 8;
            pOut->mDataSize[0] = imageWidth * imageHeight * max(forceComponents, componentCount) * bpc;

            pOut->mDesc.mWidth = imageWidth;
            pOut->mDesc.mHeight = imageHeight;
            pOut->mDesc.mDepth = imageDepth;

            pOut->mDesc.mMipLevels = 1;
            pOut->mDesc.mArraySize = 1;
            pOut->mDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            pOut->mDesc.mFormat = textureFormat;
        }

        // Release loaded input file data
        if (pFileData)
        {
            tf_free(pFileData);
        }
    }

    fsCloseStream(&file);

    return success;
}

static inline bool IsASTCSlow(ASTC method)
{
    ASSERT(method != ASTC_NONE);
    switch (method)
    {
    case ASTC_4x4_SLOW:
    case ASTC_5x4_SLOW:
    case ASTC_6x6_SLOW:
    case ASTC_8x5_SLOW:
    case ASTC_8x6_SLOW:
    case ASTC_8x8_SLOW:
        return true;
    default:
        return false;
    }
}

// Older Compression performed using ISPC
bool ASTCCompression(void* ppData[MAX_MIPLEVELS], void* ppOutCompressed[MAX_MIPLEVELS], uint32_t* pCompressedSize,
                     CompressImageDescriptor* pDesc, TextureDesc* pTexDesc)
{
    ASSERT(ppData[0]);
    ASSERT(pDesc);
    ASSERT(pTexDesc->mWidth && pTexDesc->mHeight); // widht/height cannot be 0

    uint32_t blockSizeX = 0;
    uint32_t blockSizeY = 0;

    switch (pDesc->mASTCCompression)
    {
    case ASTC_4x4:
    case ASTC_4x4_SLOW:
        blockSizeX = 4;
        blockSizeY = 4;
        break;
    case ASTC_5x4:
    case ASTC_5x4_SLOW:
        blockSizeX = 5;
        blockSizeY = 4;
        break;
    case ASTC_5x5:
    case ASTC_5x5_SLOW:
        blockSizeX = 5;
        blockSizeY = 5;
        break;
    case ASTC_6x6:
    case ASTC_6x6_SLOW:
        blockSizeX = 6;
        blockSizeY = 6;
        break;
    case ASTC_8x5:
    case ASTC_8x5_SLOW:
        blockSizeX = 8;
        blockSizeY = 5;
        break;
    case ASTC_8x6:
    case ASTC_8x6_SLOW:
        blockSizeX = 8;
        blockSizeY = 6;
        break;
    case ASTC_8x8:
    case ASTC_8x8_SLOW:
        blockSizeX = 8;
        blockSizeY = 8;
        break;
    case ASTC_NONE:
    default:
        blockSizeX = 4;
        blockSizeY = 4;
    }

    const uint32_t channels = TinyImageFormat_ChannelCount(pTexDesc->mFormat);
    ASSERT(channels >= 3); // ISPC astc compression requires atleast 3 channels

    uint32_t bytesPerChannel = (TinyImageFormat_BitSizeOfBlock(pTexDesc->mFormat) / 8) / 4;

    // Get astc encoder settings
    astc_enc_settings astcEncSettings = {};
    if (channels > 3)
    {
        if (pDesc->mASTCCompression == ASTC_4x4_SLOW || pDesc->mASTCCompression == ASTC_5x4_SLOW ||
            pDesc->mASTCCompression == ASTC_5x5_SLOW || pDesc->mASTCCompression == ASTC_6x6_SLOW ||
            pDesc->mASTCCompression == ASTC_8x5_SLOW || pDesc->mASTCCompression == ASTC_8x6_SLOW ||
            pDesc->mASTCCompression == ASTC_8x8_SLOW)
        {
            GetProfile_astc_alpha_slow(&astcEncSettings, blockSizeX, blockSizeY);
        }
        else
        {
            GetProfile_astc_alpha_fast(&astcEncSettings, blockSizeX, blockSizeY);
        }
    }
    else
    {
        GetProfile_astc_fast(&astcEncSettings, blockSizeX, blockSizeY);
    }
    astcEncSettings.bytesPerChannel = bytesPerChannel;

    // Store if texture mip 0 is padded and use that in the and for the texture descriptor
    uint32_t adjustedWidth = pTexDesc->mWidth;
    uint32_t adjustedHeight = pTexDesc->mHeight;
    uint32_t slices = pTexDesc->mArraySize;

    for (uint32_t i = 0; i < pTexDesc->mMipLevels; ++i)
    {
        uint32_t width = max(1u, (pTexDesc->mWidth >> i));
        uint32_t height = max(1u, (pTexDesc->mHeight >> i));
        uint8_t* pData = NULL;
        for (uint32_t slice_index = 0; slice_index < slices; ++slice_index)
        {
            bool     padded = false;
            uint32_t slice_offset = width * height * channels * slice_index;

            if (width % blockSizeX != 0 || height % blockSizeY != 0)
            {
                rgba_surface input;
                input.width = width;
                input.height = height;
                input.stride = width * channels;
                input.ptr = (uint8_t*)ppData[i] + slice_offset;

                uint32_t resizedImageWidth = width % blockSizeX == 0 ? width : width + (blockSizeX - width % blockSizeX);
                uint32_t resizedImageHeight = height % blockSizeY == 0 ? height : height + (blockSizeY - height % blockSizeY);

                pData = (uint8_t*)tf_malloc(resizedImageWidth * resizedImageHeight * channels);
                rgba_surface input_padded;
                input_padded.width = resizedImageWidth;
                input_padded.height = resizedImageHeight;
                input_padded.stride = resizedImageWidth * channels;
                input_padded.ptr = pData;

                ReplicateBorders(&input_padded, &input, 0, 0, 32 /* or 64 */);

                width = resizedImageWidth;
                height = resizedImageHeight;
                padded = true;

                if (i == 0)
                {
                    adjustedWidth = width;
                    adjustedHeight = height;
                }
            }
            else
            {
                pData = (uint8_t*)ppData[i] + slice_offset;
            }

            const uint32_t xblocks = (width + blockSizeX - 1) / blockSizeX;
            const uint32_t yblocks = (height + blockSizeY - 1) / blockSizeY;
            const uint32_t bytesPerBlock = 16;

            uint32_t compressed_offset = 0;
            if (ppOutCompressed[i] == NULL)
            {
                pCompressedSize[i] = xblocks * yblocks * bytesPerBlock;
                ppOutCompressed[i] = (uint8_t*)tf_malloc(pCompressedSize[i]);
            }
            else
            {
                compressed_offset = pCompressedSize[i];
                pCompressedSize[i] += xblocks * yblocks * bytesPerBlock;
                ppOutCompressed[i] = (uint8_t*)tf_realloc(ppOutCompressed[i], pCompressedSize[i]);
            }

            rgba_surface input;
            input.width = width;
            input.height = height;
            input.stride = width * channels * bytesPerChannel;
            input.ptr = pData;

            CompressBlocksASTC(&input, (uint8_t*)ppOutCompressed[i] + compressed_offset, &astcEncSettings);

            if (padded)
            {
                tf_free(pData);
            }
        }
    }

    // Set image size to padding size
    pTexDesc->mWidth = adjustedWidth;
    pTexDesc->mHeight = adjustedHeight;

    return true;
}

typedef void (*BCCompressionFunc)(const rgba_surface* src, uint8_t* dst);

#define DECLARE_COMPRESS_FUNCTION_BC6H(profile)                              \
    void CompressBlocksBC6H_##profile(const rgba_surface* src, uint8_t* dst) \
    {                                                                        \
        bc6h_enc_settings settings;                                          \
        GetProfile_bc6h_##profile(&settings);                                \
        CompressBlocksBC6H(src, dst, &settings);                             \
    }

DECLARE_COMPRESS_FUNCTION_BC6H(veryfast);
DECLARE_COMPRESS_FUNCTION_BC6H(fast);
DECLARE_COMPRESS_FUNCTION_BC6H(basic);
DECLARE_COMPRESS_FUNCTION_BC6H(slow);
DECLARE_COMPRESS_FUNCTION_BC6H(veryslow);

#define DECLARE_COMPRESS_FUNCTION_BC7(profile)                              \
    void CompressBlocksBC7_##profile(const rgba_surface* src, uint8_t* dst) \
    {                                                                       \
        bc7_enc_settings settings;                                          \
        GetProfile_##profile(&settings);                                    \
        CompressBlocksBC7(src, dst, &settings);                             \
    }

DECLARE_COMPRESS_FUNCTION_BC7(ultrafast);
DECLARE_COMPRESS_FUNCTION_BC7(veryfast);
DECLARE_COMPRESS_FUNCTION_BC7(fast);
DECLARE_COMPRESS_FUNCTION_BC7(basic);
DECLARE_COMPRESS_FUNCTION_BC7(slow);
DECLARE_COMPRESS_FUNCTION_BC7(alpha_ultrafast);
DECLARE_COMPRESS_FUNCTION_BC7(alpha_veryfast);
DECLARE_COMPRESS_FUNCTION_BC7(alpha_fast);
DECLARE_COMPRESS_FUNCTION_BC7(alpha_basic);
DECLARE_COMPRESS_FUNCTION_BC7(alpha_slow);

bool BCCompression(void* ppData[MAX_MIPLEVELS], void* ppOutCompressed[MAX_MIPLEVELS], uint32_t* pCompressedSize,
                   CompressImageDescriptor* pDesc, TextureDesc* pTexDesc)
{
    ASSERT(ppData[0]);
    ASSERT(pDesc);
    ASSERT(pTexDesc->mWidth && pTexDesc->mHeight); // width/height cannot be 0

    const uint32_t    blockSize = 4;
    BCCompressionFunc bcCompress = CompressBlocksBC7_alpha_fast;
    uint32_t          bytesPerBlock = 16;

    uint32_t inputChannels = TinyImageFormat_ChannelCount(pTexDesc->mFormat);
    uint32_t requiredInputChannels = 4;
    uint32_t bitsPerPixel = TinyImageFormat_BitSizeOfBlock(pTexDesc->mFormat);
    uint32_t bytesPerChannel = (bitsPerPixel / 4) / 8;
    //-LDR input is 32 bit / pixel(sRGB), HDR is 64 bit / pixel(half float)
    //	- for BC4 input is 8bit / pixel(R8), for BC5 input is 16bit / pixel(RG8)
    //	- dst buffer must be allocated with enough space for the compressed texture

    switch (pDesc->mDXTCompression)
    {
    case DXT_BC1:
        bcCompress = CompressBlocksBC1;
        bytesPerBlock = 8;
        break;
    case DXT_BC3:
        bcCompress = CompressBlocksBC3;
        break;
    case DXT_BC4:
        bcCompress = CompressBlocksBC4;
        bytesPerBlock = 8;
        requiredInputChannels = 1;
        break;
    case DXT_BC5:
        bcCompress = CompressBlocksBC5;
        requiredInputChannels = 2;
        break;
    case DXT_BC6:
        bcCompress = CompressBlocksBC6H_fast;
        requiredInputChannels = 4;
        if (bitsPerPixel != 64 && !TinyImageFormat_IsFloat(pTexDesc->mFormat))
        {
            LOGF(LogLevel::eERROR, "%s is an unsupported format for BC6 compression", TinyImageFormat_Name(pTexDesc->mFormat));
            return false;
        }
        break;
    case DXT_BC7:
        bcCompress = inputChannels > 3 ? CompressBlocksBC7_alpha_fast : CompressBlocksBC7_fast;
        break;
    default:
        ASSERT(false && "Unknown BC compression request");
    }
    ASSERT(requiredInputChannels <= inputChannels && "Input should always have more data available");

    // Store if texture mip 0 is padded and use that in the and for the texture descriptor
    uint32_t adjustedWidth = pTexDesc->mWidth;
    uint32_t adjustedHeight = pTexDesc->mHeight;
    uint32_t slices = pTexDesc->mArraySize;

    for (uint32_t i = 0; i < pTexDesc->mMipLevels; ++i)
    {
        uint32_t width = max(1u, (pTexDesc->mWidth >> i));
        uint32_t height = max(1u, (pTexDesc->mHeight >> i));
        uint8_t* pData = NULL;

        for (uint32_t slice_index = 0; slice_index < slices; ++slice_index)
        {
            bool     padded = false;
            uint32_t slice_offset = width * height * inputChannels * slice_index;

            if (width % blockSize != 0 || height % blockSize != 0)
            {
                rgba_surface input;
                input.width = width;
                input.height = height;
                input.stride = width * inputChannels;
                input.ptr = (uint8_t*)ppData[i] + slice_offset;

                uint32_t resizedImageWidth = width % blockSize == 0 ? width : width + (blockSize - width % blockSize);
                uint32_t resizedImageHeight = height % blockSize == 0 ? height : height + (blockSize - height % blockSize);

                pData = (uint8_t*)tf_malloc(resizedImageWidth * resizedImageHeight * inputChannels);
                rgba_surface input_padded;
                input_padded.width = resizedImageWidth;
                input_padded.height = resizedImageHeight;
                input_padded.stride = resizedImageWidth * inputChannels;
                input_padded.ptr = pData;

                ReplicateBorders(&input_padded, &input, 0, 0, bitsPerPixel);

                width = resizedImageWidth;
                height = resizedImageHeight;
                padded = true;

                if (i == 0)
                {
                    adjustedWidth = width;
                    adjustedHeight = height;
                }
            }
            else
            {
                pData = (uint8_t*)ppData[i] + slice_offset;
            }

            const uint32_t xblocks = (width + blockSize - 1) / blockSize;
            const uint32_t yblocks = (height + blockSize - 1) / blockSize;

            uint32_t compressed_offset = 0;
            if (ppOutCompressed[i] == NULL)
            {
                pCompressedSize[i] = xblocks * yblocks * bytesPerBlock;
                ppOutCompressed[i] = (uint8_t*)tf_malloc(pCompressedSize[i]);
            }
            else
            {
                compressed_offset = pCompressedSize[i];
                pCompressedSize[i] += xblocks * yblocks * bytesPerBlock;
                ppOutCompressed[i] = (uint8_t*)tf_realloc(ppOutCompressed[i], pCompressedSize[i]);
            }

            // Reorder data
            if (requiredInputChannels != inputChannels)
            {
                const uint32_t pixelCount = width * height;
                for (uint32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
                {
                    uint32_t inputDataIndex = pixelIndex * inputChannels;
                    uint32_t outputDataIndex = pixelIndex * requiredInputChannels;
                    for (uint32_t reorderChannel = 0; reorderChannel < requiredInputChannels; ++reorderChannel)
                    {
                        pData[outputDataIndex + reorderChannel] = pData[inputDataIndex + reorderChannel];
                    }
                }
            }

            rgba_surface input;
            input.width = width;
            input.height = height;
            input.stride = width * requiredInputChannels * bytesPerChannel;
            input.ptr = pData;

            bcCompress(&input, (uint8_t*)ppOutCompressed[i] + compressed_offset);

            if (padded)
            {
                tf_free(pData);
            }
        }
    }

    // Set image size to padding size
    pTexDesc->mWidth = adjustedWidth;
    pTexDesc->mHeight = adjustedHeight;

    return true;
}

bool CompressImageData(void* ppData[MAX_MIPLEVELS], void* ppOutCompressed[MAX_MIPLEVELS], uint32_t* pCompressedSize,
                       CompressImageDescriptor* pDesc, TextureDesc* pTextDesc)
{
    if (!ppData[0])
    {
        ppOutCompressed[0] = NULL;
        pCompressedSize[0] = 0;
        return false;
    }

    switch (pDesc->mCompression)
    {
    case COMPRESSION_ASTC:
        return ASTCCompression(ppData, ppOutCompressed, pCompressedSize, pDesc, pTextDesc);
    case COMPRESSION_BC:
        return BCCompression(ppData, ppOutCompressed, pCompressedSize, pDesc, pTextDesc);
    default:
        LOGF(eERROR, "Unknown compression!");
        return false;
    }
}

void GenerateMipmaps(uint8_t* ppData[MAX_MIPLEVELS], uint32_t* pImageDataSize, TextureDesc* pTextDesc)
{
    uint32_t width = pTextDesc->mWidth;
    uint32_t height = pTextDesc->mHeight;
    uint32_t numLevels = max((uint32_t)log2(width), (uint32_t)log2(height)) + 1u;
    uint32_t channels = TinyImageFormat_ChannelCount(pTextDesc->mFormat);

    for (uint32_t i = 1; i < numLevels; ++i)
    {
        uint32_t prevWidth = max(width >> (i - 1u), 1u);
        uint32_t prevHeight = max(height >> (i - 1u), 1u);
        uint32_t mipWidth = max(width >> i, 1u);
        uint32_t mipHeight = max(height >> i, 1u);

        ppData[i] = (uint8_t*)tf_malloc(mipWidth * mipHeight * channels);
        pImageDataSize[i] = mipWidth * mipHeight * channels;

        uint8_t* inImageData = ppData[i - 1];
        uint8_t* outImageData = ppData[i];

        // TODO:
        //  - different color spaces
        //  - other filter options
        //  - other clamping ways
        int result = stbir_resize_uint8_generic(
            inImageData, prevWidth, prevHeight, channels * prevWidth, outImageData, mipWidth, mipHeight, channels * mipWidth, channels,
            STBIR_ALPHA_CHANNEL_NONE, STBIR_FLAG_ALPHA_USES_COLORSPACE, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT,
            TinyImageFormat_IsSRGB(pTextDesc->mFormat) ? STBIR_COLORSPACE_SRGB : STBIR_COLORSPACE_LINEAR, nullptr);

        ASSERT(result == 1);
    }

    pTextDesc->mMipLevels = numLevels;
}

void GenerateVMFFilteredMipmaps(uint8_t* ppData[MAX_MIPLEVELS], uint32_t* pImageDataSize, TextureDesc* pTextureDesc, uint32_t channelCount,
                                void* pUserData)
{
    UNREF_PARAM(pImageDataSize);
    ASSERT(channelCount > 2 && "Minimum of 3 channels input is required");

    const uint32_t width = pTextureDesc->mWidth;
    const uint32_t height = pTextureDesc->mHeight;
    const uint32_t numLevels = max((uint32_t)log2(width), (uint32_t)log2(height)) + 1u;
    pTextureDesc->mMipLevels = numLevels;

    vec3* rData[MAX_MIPLEVELS];
    rData[0] = (vec3*)pUserData;

    for (uint32_t mipMap = 1; mipMap < numLevels; ++mipMap)
    {
        uint32_t prevWidth = max(width >> (mipMap - 1u), 1u);
        uint32_t prevHeight = max(height >> (mipMap - 1u), 1u);
        uint32_t mipWidth = max(width >> mipMap, 1u);
        uint32_t mipHeight = max(height >> mipMap, 1u);
        rData[mipMap] = (vec3*)tf_malloc(mipWidth * mipHeight * sizeof(vec3));
        ppData[mipMap] = (uint8_t*)tf_malloc(mipWidth * mipHeight * channelCount);

        for (uint32_t y = 0; y < mipHeight; ++y)
        {
            for (uint32_t x = 0; x < mipWidth; ++x)
            {
                const uint32_t mipPixelIndex = x + y * mipWidth;
                const float    xCenter = clamp((x << 1) + 0.5f, 0.0f, prevWidth - 1.0f);
                const float    yCenter = clamp((y << 1) + 0.5f, 0.0f, prevHeight - 1.0f);
                float          xl = floorf(xCenter);
                float          xu = ceilf(xCenter);
                float          yl = floorf(yCenter);
                float          yu = ceilf(yCenter);

                // bilinear interpolation
                vec3 xlValue = rData[mipMap - 1][(uint32_t)(xl + yl * prevWidth)];
                vec3 xuValue = rData[mipMap - 1][(uint32_t)(xu + yl * prevWidth)];

                vec3 ylValue = rData[mipMap - 1][(uint32_t)(xl + yu * prevWidth)];
                vec3 yuValue = rData[mipMap - 1][(uint32_t)(xu + yu * prevWidth)];

                vec3 horizontalL = xlValue * (xu - xCenter) / (xu - xl) + xuValue * (xCenter - xl) / (xu - xl);
                vec3 horizontalU = ylValue * (xu - xCenter) / (xu - xl) + yuValue * (xCenter - xl) / (xu - xl);

                vec3 rFiltered = horizontalL * (yu - yCenter) / (yu - yl) + horizontalU * (yCenter - yl) / (yu - yl);

                rData[mipMap][mipPixelIndex] = rFiltered;
                rFiltered = normalize(rFiltered);
                // Move normal back to range [0, 1]
                rFiltered = rFiltered * 0.5f + vec3(0.5f);

                const uint32_t mipChannelIndex = mipPixelIndex * channelCount;
                ppData[mipMap][mipChannelIndex + 0] = (uint8_t)(fmaxf(fminf(1.0f, rFiltered[0]), 0.0f) * (float)UINT8_MAX + 0.5f);
                ppData[mipMap][mipChannelIndex + 1] = (uint8_t)(fmaxf(fminf(1.0f, rFiltered[1]), 0.0f) * (float)UINT8_MAX + 0.5f);
                ppData[mipMap][mipChannelIndex + 2] = (uint8_t)(fmaxf(fminf(1.0f, rFiltered[2]), 0.0f) * (float)UINT8_MAX + 0.5f);

                if (channelCount == 4)
                {
                    // Fill alpha if possible
                    ppData[mipMap][mipChannelIndex + 3] = UINT8_MAX;
                }
            }
        }
    }

    for (uint32_t mipMap = 1; mipMap < numLevels; ++mipMap)
    {
        tf_free(rData[mipMap]);
    }
}

bool GenerateVMFLayer(InputTextureData* pNormalTextureData, InputTextureData* pRoughnessTextureData, vec3* rData)
{
    ASSERT(pNormalTextureData);
    ASSERT(pRoughnessTextureData);

    bool success = true;

    /////////////////////////////////
    // Validate input
    /////////////////////////////////
    if (pNormalTextureData->isCompressed || pRoughnessTextureData->isCompressed)
    {
        LOGF(LogLevel::eERROR, "%s: Compressed input data given, requires uncompressed texture data!", __FUNCTION__);
        success = false;
    }

    if (pNormalTextureData->mDesc.mWidth != pRoughnessTextureData->mDesc.mWidth ||
        pNormalTextureData->mDesc.mHeight != pRoughnessTextureData->mDesc.mHeight)
    {
        LOGF(LogLevel::eERROR, "%s: width/height of normal {%u/%u} and roughness {%u/%u} texture do not match!", __FUNCTION__,
             pNormalTextureData->mDesc.mWidth, pNormalTextureData->mDesc.mHeight, pRoughnessTextureData->mDesc.mWidth,
             pRoughnessTextureData->mDesc.mHeight);

        success = false;
    }

    const uint32_t normalTextureChannels = TinyImageFormat_ChannelCount(pNormalTextureData->mDesc.mFormat);
    const uint32_t roughnessTextureChannels = TinyImageFormat_ChannelCount(pRoughnessTextureData->mDesc.mFormat);
    if (normalTextureChannels < 3)
    {
        LOGF(LogLevel::eERROR, "%s: Normal input texture has to few channels %u!", __FUNCTION__, normalTextureChannels);
        success = false;
    }

    /////////////////////////////////
    // Generate layer
    /////////////////////////////////
    if (success)
    {
        uint8_t* textureData = (uint8_t*)pNormalTextureData->pData[0];
        for (uint32_t y = 0; y < pNormalTextureData->mDesc.mHeight; ++y)
        {
            for (uint32_t x = 0; x < pNormalTextureData->mDesc.mWidth; ++x)
            {
                const uint32_t pixelIndex = x + y * pNormalTextureData->mDesc.mWidth;
                const uint32_t pixelIndexNormal = pixelIndex * normalTextureChannels;
                const uint32_t pixelIndexRoughness = pixelIndex * roughnessTextureChannels;

                vec3 normal =
                    vec3(textureData[pixelIndexNormal + 0] / (float)UINT8_MAX, textureData[pixelIndexNormal + 1] / (float)UINT8_MAX,
                         textureData[pixelIndexNormal + 2] / (float)UINT8_MAX);

                // Set normal to range [-1.0, 1.0]
                normal = normal * 2.0f - vec3(1.0f);

                // TODO Optional: Give channel to look into for the rougness value as argument
                // For now expect the first channel to contain the actuall roughness value
                const float roughnessValue = textureData[pixelIndexRoughness + 0] / (float)UINT8_MAX;

                // Convert to r form
                const float invLambda = 0.5f * roughnessValue * roughnessValue;
                const float exp2l = exp(-2.0f / invLambda);
                const float cothLambda = invLambda > 0.1f ? (1.0f + exp2l) / (1.0f - exp2l) : 1.0f;
                vec3        r = normal * (cothLambda - invLambda);
                rData[pixelIndex] = r;
            }
        }
    }

    return success;
}

void SwizzlePixel(uint8_t* pixelData, const TextureSwizzle* swizzle)
{
    const uint32_t channelCount = 4;
    uint8_t        inputData[channelCount];
    memcpy(inputData, pixelData, sizeof(uint8_t) * channelCount);

    for (uint32_t i = 0; i < channelCount; ++i)
    {
        switch (swizzle->mIndices[i])
        {
        case 'r':
        case 'x':
            pixelData[i] = inputData[0];
            break;
        case 'g':
        case 'y':
            pixelData[i] = inputData[1];
            break;
        case 'b':
        case 'z':
            pixelData[i] = inputData[2];
            break;
        case 'a':
        case 'w':
            pixelData[i] = inputData[3];
            break;
        case 'i':
            pixelData[i] = UINT8_MAX - inputData[0];
            break;
        case 'j':
            pixelData[i] = UINT8_MAX - inputData[1];
            break;
        case 'k':
            pixelData[i] = UINT8_MAX - inputData[2];
            break;
        case 'l':
            pixelData[i] = UINT8_MAX - inputData[3];
            break;
        case '1':
            pixelData[i] = UINT8_MAX;
            break;
        case '0':
        default:
            pixelData[i] = 0;
        }
    }
}

void SwizzlePixel(uint16_t* pixelData, const TextureSwizzle* swizzle)
{
    const uint32_t channelCount = 4;
    uint16_t       inputData[channelCount];
    memcpy(inputData, pixelData, sizeof(uint16_t) * channelCount);

    for (uint32_t i = 0; i < channelCount; ++i)
    {
        switch (swizzle->mIndices[i])
        {
        case 'r':
        case 'x':
            pixelData[i] = inputData[0];
            break;
        case 'g':
        case 'y':
            pixelData[i] = inputData[1];
            break;
        case 'b':
        case 'z':
            pixelData[i] = inputData[2];
            break;
        case 'a':
        case 'w':
            pixelData[i] = inputData[3];
            break;
        case '1':
            pixelData[i] = UINT16_MAX;
            break;
        case '0':
        default:
            pixelData[i] = 0;
        }
    }
}

void SwizzlePixel(float_t* pixelData, const TextureSwizzle* swizzle)
{
    const uint32_t channelCount = 4;
    float_t        inputData[channelCount];
    memcpy(inputData, pixelData, sizeof(float_t) * channelCount);

    for (uint32_t i = 0; i < channelCount; ++i)
    {
        switch (swizzle->mIndices[i])
        {
        case 'r':
        case 'x':
            pixelData[i] = inputData[0];
            break;
        case 'g':
        case 'y':
            pixelData[i] = inputData[1];
            break;
        case 'b':
        case 'z':
            pixelData[i] = inputData[2];
            break;
        case 'a':
        case 'w':
            pixelData[i] = inputData[3];
            break;
        case '1':
            pixelData[i] = 1.0f;
            break;
        case '0':
        default:
            pixelData[i] = 0;
        }
    }
}

void SwizzleUncompressedData(void* ppData[MAX_MIPLEVELS], const TextureDesc* pTexDesc, TextureSwizzle swizzle,
                             const TextureDataType dataType)
{
    const uint32_t channelCount = TinyImageFormat_ChannelCount(pTexDesc->mFormat);

    for (uint32_t mip = 0; mip < pTexDesc->mMipLevels; ++mip)
    {
        uint32_t mipWidth = max(pTexDesc->mWidth >> mip, 1u);
        uint32_t mipHeight = max(pTexDesc->mHeight >> mip, 1u);

        for (uint32_t y = 0; y < mipHeight; ++y)
        {
            for (uint32_t x = 0; x < mipWidth; ++x)
            {
                const uint32_t pixelIndex = (x + y * mipWidth) * channelCount;
                uint8_t*       p8Data = NULL;
                uint16_t*      p16Data = NULL;
                float_t*       pFData = NULL;
                switch (dataType)
                {
                case TEX_DATA_TYPE_U8:
                    p8Data = (uint8_t*)ppData[mip] + pixelIndex;
                    SwizzlePixel(p8Data, &swizzle);
                    break;
                case TEX_DATA_TYPE_U16:
                case TEX_DATA_TYPE_F16:
                    p16Data = (uint16_t*)ppData[mip] + pixelIndex;
                    SwizzlePixel(p16Data, &swizzle);
                    break;
                case TEX_DATA_TYPE_F32:
                    pFData = (float_t*)ppData[mip] + pixelIndex;
                    SwizzlePixel(pFData, &swizzle);
                    break;
                }
            }
        }
    }
}

void onTextureFound(ResourceDirectory resourceDir, const char* filename, void* pUserData)
{
    UNREF_PARAM(resourceDir);
    bstring** fileNames = (bstring**)pUserData;
    arrpush(*fileNames, bdynfromcstr(filename));
}

bool ProcessTextures(AssetPipelineParams* assetParams, ProcessTexturesParams* texturesParams)
{
    // TODO:
    //  - Texture arrays
    //  - Cubemaps
    //  - HDR texture support

    bool     error = false;
    // Get all image files
    bstring* inputImgFileNames = NULL;

    if (assetParams->mPathMode == PROCESS_MODE_FILE)
    {
        arrpush(inputImgFileNames, bdynfromcstr(assetParams->mInFilePath));
    }
    else
    {
        DirectorySearch(assetParams->mRDInput, NULL, texturesParams->mInExt, onTextureFound, (void*)&inputImgFileNames,
                        assetParams->mPathMode == PROCESS_MODE_DIRECTORY_RECURSIVE);
    }

    uint32_t imgFileCount = (uint32_t)arrlenu(inputImgFileNames);

    for (uint32_t i = 0; i < imgFileCount; ++i)
    {
        TinyImageFormat       outFormat = TinyImageFormat_UNDEFINED;
        ProcessTexturesParams copyTextureParams = *texturesParams;
        bool                  useVMF = copyTextureParams.pRoughnessFilePath != NULL;

        const char* inFileName = (char*)inputImgFileNames[i].data;

        char inExtension[FS_MAX_PATH] = { 0 };
        fsGetPathExtension(inFileName, inExtension);

        char outFileName[FS_MAX_PATH] = { 0 };

        if (assetParams->mOutSubdir)
        {
            char fileName[FS_MAX_PATH] = {};
            fsGetPathFileName(inFileName, fileName);

            strcat(fileName, ".tex");

            fsAppendPathComponent(assetParams->mOutSubdir, fileName, outFileName);
        }
        else
        {
            fsReplacePathExtension(inFileName, "tex", outFileName);
        }

        // If input file newer than output file redo compression
        if (!assetParams->mSettings.force && fsFileExist(assetParams->mRDOutput, outFileName))
        {
            time_t lastModified = fsGetLastModifiedTime(assetParams->mRDInput, inFileName);
            if (assetParams->mAdditionalModifiedTime != 0)
                lastModified = max(lastModified, assetParams->mAdditionalModifiedTime);

            time_t lastProcessed = fsGetLastModifiedTime(assetParams->mRDOutput, outFileName);

            if (lastModified < lastProcessed)
            {
                LOGF(eINFO, "Skipping %s", inFileName);
                continue;
            }
        }

        LOGF(eINFO, "Converting texture %s from .%s to .%s with output container : %s", inFileName, copyTextureParams.mInExt, "tex",
             gExtensions[copyTextureParams.mContainer]);

        /////////////////////////////////
        // Load raw image data
        ////////////////////////////////
        InputTextureData inputTextureData = {};
        if (!LoadTextureData(assetParams->mRDInput, inFileName, inExtension, &copyTextureParams, &inputTextureData))
        {
            error = true;
            continue;
        }

        /////////////////////////////////
        // vMF
        /////////////////////////////////
        vec3* rData = nullptr;

        if (useVMF)
        {
            InputTextureData inputRoughnessTextureData = {};
            if (!LoadTextureData(assetParams->mRDInput, copyTextureParams.pRoughnessFilePath, inExtension, &copyTextureParams,
                                 &inputRoughnessTextureData))
            {
                error = true;
                continue;
            }

            rData = (vec3*)tf_malloc(inputTextureData.mDesc.mWidth * inputTextureData.mDesc.mHeight * sizeof(vec3));

            if (!GenerateVMFLayer(&inputTextureData, &inputRoughnessTextureData, rData))
            {
                error = true;
            }

            // Release rougness texture data
            for (size_t mip = 0; mip < inputTextureData.mDesc.mMipLevels; ++mip)
            {
                tf_free(inputRoughnessTextureData.pData[mip]);
                inputRoughnessTextureData.pData[mip] = NULL;
                inputRoughnessTextureData.mDataSize[mip] = 0;
            }

            copyTextureParams.pCallbackUserData = rData;
            copyTextureParams.mGenerateMipmaps = TextureMipmap::MIPMAP_CUSTOM;
            copyTextureParams.pGenerateMipmapsCallback = GenerateVMFFilteredMipmaps;

            if (copyTextureParams.mCompression == COMPRESSION_BC && copyTextureParams.mOverrideBC == DXT_NONE)
            {
                LOGF(eINFO, "Using DXT_BC5 compression for vMF output");
                copyTextureParams.mOverrideBC = DXT_BC5;
            }
        }

        /////////////////////////////////
        // Generate mipmaps
        /////////////////////////////////
        if (copyTextureParams.mGenerateMipmaps == MIPMAP_CUSTOM)
        {
            ASSERT(copyTextureParams.pGenerateMipmapsCallback && "MIPMAP_CUSTOM requires pGenerateMipmapsCallback to be set");
            if (inputTextureData.pData[0])
            {
                uint32_t channels = TinyImageFormat_ChannelCount(inputTextureData.mDesc.mFormat);
                copyTextureParams.pGenerateMipmapsCallback((uint8_t**)inputTextureData.pData, inputTextureData.mDataSize,
                                                           &inputTextureData.mDesc, channels, copyTextureParams.pCallbackUserData);
            }
        }

        if (copyTextureParams.mGenerateMipmaps == MIPMAP_DEFAULT && inputTextureData.mDesc.mMipLevels <= 1 &&
            !inputTextureData.isCompressed)
        {
            if (inputTextureData.pData[0])
            {
                GenerateMipmaps((uint8_t**)inputTextureData.pData, inputTextureData.mDataSize, &inputTextureData.mDesc);
            }
        }

        if (useVMF)
        {
            tf_free(rData);
        }

        /////////////////////////////////
        // Swizzle uncompressed data
        ////////////////////////////////
        if (texturesParams->mSwizzleChannelCount > 0)
        {
            if (inputTextureData.isCompressed)
            {
                LOGF(eWARNING, "Input texture is compressed, cannot be swizzled");
                error = true;
                continue;
            }
            SwizzleUncompressedData(inputTextureData.pData, &inputTextureData.mDesc, texturesParams->mSwizzle, inputTextureData.mDataType);
        }

        /////////////////////////////////
        // Compress
        /////////////////////////////////
        CompressImageDescriptor compressDesc = {};

        if (inputTextureData.isCompressed)
        {
            outFormat = inputTextureData.mDesc.mFormat;
            LOGF(eWARNING, "Input texture '%s' is already compressed {%s}, copy texture to destination", inFileName,
                 TinyImageFormat_Name(outFormat));
        }
        else
        {
            outFormat = GetOutputTextureFormat(&copyTextureParams, &inputTextureData.mDesc, &compressDesc);
        }

        void*    pCompressedData[MAX_MIPLEVELS] = { NULL };
        uint32_t compressedDataSize[MAX_MIPLEVELS] = { 0 };

        if (outFormat == TinyImageFormat_UNDEFINED)
        {
            LOGF(eERROR, "Undefined Image format");
            error = true;
            continue;
        }

        if (!inputTextureData.isCompressed && copyTextureParams.mCompression != TextureCompression::COMPRESSION_NONE)
        {
            // Process raw image data
            if (!CompressImageData(inputTextureData.pData, pCompressedData, compressedDataSize, &compressDesc, &inputTextureData.mDesc))
            {
                LOGF(eERROR, "Failed to compress texture %s", inFileName);
                error = true;
            }

            // Free raw image data, can be released once image is compressed
            if (inputTextureData.pData[0])
            {
                // Release image data
                for (uint32_t mip = 0; mip < inputTextureData.mDesc.mMipLevels; ++mip)
                {
                    stbi_image_free(inputTextureData.pData[mip]);
                    inputTextureData.pData[mip] = NULL;
                }
            }
        }
        else
        {
            // Set raw pImageData as out data
            for (uint32_t mip = 0; mip < inputTextureData.mDesc.mMipLevels; ++mip)
            {
                pCompressedData[mip] = inputTextureData.pData[mip];
                compressedDataSize[mip] = inputTextureData.mDataSize[mip];
            }
        }

        /////////////////////////////////
        // Write output
        /////////////////////////////////

        // Remove old file
        if (!error)
        {
            fsRemoveFile(assetParams->mRDOutput, outFileName);
        }

        // Make sure output folder exists
        {
            char assetPath[FS_MAX_PATH] = {};
            fsGetParentPath(outFileName, assetPath);
            fsCreateDirectory(assetParams->mRDOutput, assetPath, true);
        }

        FileStream outFile = {};
        if (!fsOpenStreamFromPath(assetParams->mRDOutput, outFileName, FM_WRITE, &outFile))
        {
            LOGF(eERROR, "Could not open file '%s' for write.", outFileName);
            error = true;
        }

        // Write .ktx file
        const bool     isCubemap = (inputTextureData.mDesc.mDescriptors & DESCRIPTOR_TYPE_TEXTURE_CUBE) == DESCRIPTOR_TYPE_TEXTURE_CUBE;
        // Array size in the disk image needs to be 1 since we'll already multiply it by 6 when loading the texture in runtime
        const uint32_t arraySize = isCubemap ? inputTextureData.mDesc.mArraySize / 6 : inputTextureData.mDesc.mArraySize;
        if (copyTextureParams.mContainer == CONTAINER_KTX)
        {
            TinyKtx_Format outKtxFormat = TinyImageFormat_ToTinyKtxFormat(outFormat);
            if (!TinyKtx_WriteImage(&ktxWriteCallbacks, &outFile, inputTextureData.mDesc.mWidth, inputTextureData.mDesc.mHeight,
                                    inputTextureData.mDesc.mDepth, arraySize, inputTextureData.mDesc.mMipLevels, outKtxFormat, isCubemap,
                                    compressedDataSize, (const void**)pCompressedData))
            {
                LOGF(eERROR, "Couldn't create ktx file '%s' with format '%s'", outFileName, TinyImageFormat_Name(outFormat));
                error = true;
            }
        }
        // Write .dds file
        else if (copyTextureParams.mContainer == CONTAINER_DDS)
        {
            TinyDDS_Format outDDSFormat = TinyImageFormat_ToTinyDDSFormat(outFormat);
            if (!TinyDDS_WriteImage(&ddsWriteCallbacks, &outFile, inputTextureData.mDesc.mWidth, inputTextureData.mDesc.mHeight,
                                    inputTextureData.mDesc.mDepth, arraySize, inputTextureData.mDesc.mMipLevels, outDDSFormat, isCubemap,
                                    false, compressedDataSize, (const void**)pCompressedData))
            {
                LOGF(eERROR, "Couldn't create dds file '%s' with format '%s'", outFileName, TinyImageFormat_Name(outFormat));
                error = true;
            }
        }
#ifdef XBOX_SCARLETT_DDS
        else if (copyTextureParams.mContainer == CONTAINER_SCARLETT_DDS)
        {
            extern bool swizzleAndWriteDds(TinyDDS_WriteCallbacks const* callbacks, void* user, uint32_t width, uint32_t height,
                                           uint32_t depth, uint32_t slices, uint32_t mipmaplevels, TinyDDS_Format format, bool cubemap,
                                           uint32_t const* mipmapsizes, void const** mipmaps);

            TinyDDS_Format outDDSFormat = TinyImageFormat_ToTinyDDSFormat(outFormat);
            if (!swizzleAndWriteDds(&ddsWriteCallbacks, &outFile, inputTextureData.mDesc.mWidth, inputTextureData.mDesc.mHeight,
                                    inputTextureData.mDesc.mDepth, inputTextureData.mDesc.mArraySize, inputTextureData.mDesc.mMipLevels,
                                    outDDSFormat, isCubemap, compressedDataSize, (const void**)pCompressedData))
            {
                LOGF(eERROR, "Couldn't create Scarlett dds file '%s' with format '%s'", outFileName, TinyImageFormat_Name(outFormat));
                error = true;
            }
        }
#endif
#ifdef PROSPERO_GNF
        else if (copyTextureParams.mContainer == CONTAINER_GNF_ORBIS || copyTextureParams.mContainer == CONTAINER_GNF_PROSPERO)
        {
            extern bool writeGnfTexture(FileStream * outFile, uint32_t width, uint32_t height, uint32_t depth, uint32_t slices,
                                        uint32_t mipmaplevels, TinyImageFormat format, bool cubemap, TextureContainer outTexContainer,
                                        uint32_t tilingQuality, uint32_t const* mipmapsizes, void const** mipmaps);

            if (!writeGnfTexture(&outFile, inputTextureData.mDesc.mWidth, inputTextureData.mDesc.mHeight, inputTextureData.mDesc.mDepth,
                                 inputTextureData.mDesc.mArraySize, inputTextureData.mDesc.mMipLevels, outFormat, isCubemap,
                                 copyTextureParams.mContainer, 1, compressedDataSize, (const void**)pCompressedData))
            {
                LOGF(eERROR, "Couldn't create gnf file '%s' with format '%s'", outFileName, TinyImageFormat_Name(outFormat));
                error = true;
            }
        }
#endif
        else
        {
            ASSERT(false && "No supported output extension");
        }

        // Close out file stream
        fsCloseStream(&outFile);

        if (pCompressedData[0])
        {
            for (size_t mip = 0; mip < inputTextureData.mDesc.mMipLevels; ++mip)
            {
                tf_free(pCompressedData[mip]);
                pCompressedData[mip] = NULL;
                compressedDataSize[mip] = 0;
            }
        }

        if (copyTextureParams.ppOutProcessedTextureData)
        {
            ProcessedTextureData outData = {};
            outData.mOutputFilePath = bdynfromcstr(outFileName);
            outData.mWidth = inputTextureData.mDesc.mWidth;
            outData.mHeight = inputTextureData.mDesc.mHeight;
            outData.mDepth = inputTextureData.mDesc.mDepth;
            outData.mArraySize = inputTextureData.mDesc.mArraySize;
            outData.mMipLevels = inputTextureData.mDesc.mMipLevels;
            outData.mFormat = (uint32_t)inputTextureData.mDesc.mFormat;
            arrpush(*copyTextureParams.ppOutProcessedTextureData, outData);
        }

        // Remove the output file if it was created but process textures failed.
        if (error)
        {
            fsRemoveFile(assetParams->mRDOutput, outFileName);
        }
    }

    if (inputImgFileNames)
    {
        for (uint32_t i = 0; i < imgFileCount; ++i)
        {
            bdestroy(&inputImgFileNames[i]);
        }

        arrfree(inputImgFileNames);
        inputImgFileNames = NULL;
    }

    return error;
}

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

#pragma once

#include "ThirdParty/OpenSource/tinydds/tinydds.h"
#include "ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"
#include "ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "ThirdParty/OpenSource/tinyimageformat/tinyimageformat_bits.h"
#include "ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "ThirdParty/OpenSource/tinyktx/tinyktx.h"

#include "../../Graphics/Interfaces/IGraphics.h"
#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"

/************************************************************************/
// Surface Utils
/************************************************************************/
static inline bool util_get_surface_info(uint32_t width, uint32_t height, TinyImageFormat fmt, uint32_t* outNumBytes, uint32_t* outRowBytes,
                                         uint32_t* outNumRows)
{
    uint64_t numBytes = 0;
    uint64_t rowBytes = 0;
    uint64_t numRows = 0;

    uint32_t bpp = TinyImageFormat_BitSizeOfBlock(fmt);
    bool     compressed = TinyImageFormat_IsCompressed(fmt);
    bool     planar = TinyImageFormat_IsPlanar(fmt);
    // #TODO
    bool     packed = false;

    if (compressed)
    {
        uint32_t blockWidth = TinyImageFormat_WidthOfBlock(fmt);
        uint32_t blockHeight = TinyImageFormat_HeightOfBlock(fmt);
        uint32_t numBlocksWide = 0;
        uint32_t numBlocksHigh = 0;
        if (width > 0)
        {
            numBlocksWide = max(1U, (width + (blockWidth - 1)) / blockWidth);
        }
        if (height > 0)
        {
            numBlocksHigh = max(1u, (height + (blockHeight - 1)) / blockHeight);
        }

        rowBytes = numBlocksWide * (bpp >> 3);
        numRows = numBlocksHigh;
        numBytes = rowBytes * numBlocksHigh;
    }
    else if (packed) //-V547
    {
        LOGF(eERROR, "Not implemented");
        return false;
        // rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
        // numRows = uint64_t(height);
        // numBytes = rowBytes * height;
    }
    else if (planar)
    {
        uint32_t numOfPlanes = TinyImageFormat_NumOfPlanes(fmt);

        for (uint32_t i = 0; i < numOfPlanes; ++i)
        {
            numBytes += TinyImageFormat_PlaneWidth(fmt, i, width) * TinyImageFormat_PlaneHeight(fmt, i, height) *
                        TinyImageFormat_PlaneSizeOfBlock(fmt, i);
        }

        numRows = 1;
        rowBytes = numBytes;
    }
    else
    {
        if (!bpp)
            return false;

        rowBytes = (uint64_t(width) * bpp + 7u) / 8u; // round up to nearest byte
        numRows = uint64_t(height);
        numBytes = rowBytes * height;
    }

    if (numBytes > UINT32_MAX || rowBytes > UINT32_MAX || numRows > UINT32_MAX) //-V560
        return false;

    if (outNumBytes)
    {
        *outNumBytes = (uint32_t)numBytes;
    }
    if (outRowBytes)
    {
        *outRowBytes = (uint32_t)rowBytes;
    }
    if (outNumRows)
    {
        *outNumRows = (uint32_t)numRows;
    }

    return true;
}

static inline uint32_t util_get_surface_size(TinyImageFormat format, uint32_t width, uint32_t height, uint32_t depth, uint32_t rowStride,
                                             uint32_t sliceStride, uint32_t baseMipLevel, uint32_t mipLevels, uint32_t baseArrayLayer,
                                             uint32_t arrayLayers)
{
    uint32_t requiredSize = 0;
    for (uint32_t s = baseArrayLayer; s < baseArrayLayer + arrayLayers; ++s)
    {
        uint32_t w = width;
        uint32_t h = height;
        uint32_t d = depth;

        for (uint32_t m = baseMipLevel; m < baseMipLevel + mipLevels; ++m)
        {
            uint32_t rowBytes = 0;
            uint32_t numRows = 0;

            if (!util_get_surface_info(w, h, format, NULL, &rowBytes, &numRows))
            {
                return 0;
            }

            requiredSize += round_up(d * round_up(rowBytes, rowStride) * numRows, sliceStride);

            w = w >> 1;
            h = h >> 1;
            d = d >> 1;
            if (w == 0)
            {
                w = 1;
            }
            if (h == 0)
            {
                h = 1;
            }
            if (d == 0)
            {
                d = 1;
            }
        }
    }

    return requiredSize;
}

#define RETURN_IF_FAILED(exp) \
    if (!(exp))               \
    {                         \
        return false;         \
    }

/************************************************************************/
// DDS Loading
/************************************************************************/
inline bool loadDDSTextureDesc(FileStream* pStream, TextureDesc* pOutDesc)
{
    RETURN_IF_FAILED(pStream);

    ssize_t ddsDataSize = fsGetStreamFileSize(pStream);
    RETURN_IF_FAILED(ddsDataSize <= UINT32_MAX);

    TinyDDS_Callbacks callbacks{ [](void* user, char const* msg)
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

    TinyDDS_ContextHandle ctx = TinyDDS_CreateContext(&callbacks, (void*)pStream);
    bool                  headerOkay = TinyDDS_ReadHeader(ctx);
    if (!headerOkay)
    {
        TinyDDS_DestroyContext(ctx);
        return false;
    }

    TextureDesc& textureDesc = *pOutDesc;
    textureDesc.mWidth = TinyDDS_Width(ctx);
    textureDesc.mHeight = TinyDDS_Height(ctx);
    textureDesc.mDepth = max(1U, TinyDDS_Depth(ctx));
    textureDesc.mArraySize = max(1U, TinyDDS_ArraySlices(ctx));
    textureDesc.mMipLevels = max(1U, TinyDDS_NumberOfMipmaps(ctx));
    textureDesc.mFormat = TinyImageFormat_FromTinyDDSFormat(TinyDDS_GetFormat(ctx));
    textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
    textureDesc.mSampleCount = SAMPLE_COUNT_1;

    if (textureDesc.mFormat == TinyImageFormat_UNDEFINED)
    {
        TinyDDS_DestroyContext(ctx);
        return false;
    }

    if (TinyDDS_IsCubemap(ctx))
    {
        textureDesc.mArraySize *= 6;
        textureDesc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE_CUBE;
    }

    TinyDDS_DestroyContext(ctx);

    return true;
}
/************************************************************************/
// KTX Loading
/************************************************************************/
inline bool loadKTXTextureDesc(FileStream* pStream, TextureDesc* pOutDesc)
{
    RETURN_IF_FAILED(pStream);

    ssize_t ktxDataSize = fsGetStreamFileSize(pStream);
    RETURN_IF_FAILED(ktxDataSize <= UINT32_MAX);

    TinyKtx_Callbacks callbacks{ [](void* user, char const* msg)
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

    TinyKtx_ContextHandle ctx = TinyKtx_CreateContext(&callbacks, pStream);
    bool                  headerOkay = TinyKtx_ReadHeader(ctx);
    if (!headerOkay)
    {
        TinyKtx_DestroyContext(ctx);
        return false;
    }

    TextureDesc& textureDesc = *pOutDesc;
    textureDesc.mWidth = TinyKtx_Width(ctx);
    textureDesc.mHeight = TinyKtx_Height(ctx);
    textureDesc.mDepth = max(1U, TinyKtx_Depth(ctx));
    textureDesc.mArraySize = max(1U, TinyKtx_ArraySlices(ctx));
    textureDesc.mMipLevels = max(1U, TinyKtx_NumberOfMipmaps(ctx));
    textureDesc.mFormat = TinyImageFormat_FromTinyKtxFormat(TinyKtx_GetFormat(ctx));
    textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
    textureDesc.mSampleCount = SAMPLE_COUNT_1;

    if (textureDesc.mFormat == TinyImageFormat_UNDEFINED)
    {
        TinyKtx_DestroyContext(ctx);
        return false;
    }

    if (TinyKtx_IsCubemap(ctx))
    {
        textureDesc.mArraySize *= 6;
        textureDesc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE_CUBE;
    }

    TinyKtx_DestroyContext(ctx);

    return true;
}

/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"

#include "../../Graphics/Interfaces/IGraphics.h"

#include "ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "ThirdParty/OpenSource/tinyimageformat/tinyimageformat_bits.h"
#include "ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

#include "ThirdParty/OpenSource/tinyktx/tinyktx.h"
#include "ThirdParty/OpenSource/tinydds/tinydds.h"

#include "../../Utilities/ThirdParty/OpenSource/basis_universal/transcoder/basisu_transcoder.h"

/************************************************************************/
// Surface Utils
/************************************************************************/
static inline bool util_get_surface_info(
	uint32_t width,
	uint32_t height,
	TinyImageFormat fmt,
	uint32_t* outNumBytes,
	uint32_t* outRowBytes,
	uint32_t* outNumRows)
{
	uint64_t numBytes = 0;
	uint64_t rowBytes = 0;
	uint64_t numRows = 0;

	uint32_t bpp = TinyImageFormat_BitSizeOfBlock(fmt);
	bool compressed = TinyImageFormat_IsCompressed(fmt);
	bool planar = TinyImageFormat_IsPlanar(fmt);
	// #TODO
	bool packed = false;

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
		//rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
		//numRows = uint64_t(height);
		//numBytes = rowBytes * height;
	}
	else if (planar)
	{
		uint32_t numOfPlanes = TinyImageFormat_NumOfPlanes(fmt);

		for (uint32_t i = 0; i < numOfPlanes; ++i)
		{
			numBytes += TinyImageFormat_PlaneWidth(fmt, i, width) * TinyImageFormat_PlaneHeight(fmt, i, height) * TinyImageFormat_PlaneSizeOfBlock(fmt, i);
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

static inline uint32_t util_get_surface_size(
	TinyImageFormat format,
	uint32_t width, uint32_t height, uint32_t depth,
	uint32_t rowStride, uint32_t sliceStride,
	uint32_t baseMipLevel, uint32_t mipLevels,
	uint32_t baseArrayLayer, uint32_t arrayLayers)
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
if (!(exp))                   \
{                             \
	return false;             \
}

/************************************************************************/
// DDS Loading
/************************************************************************/
static bool loadDDSTextureDesc(FileStream* pStream, TextureDesc* pOutDesc)
{
	RETURN_IF_FAILED(pStream);

	ssize_t ddsDataSize = fsGetStreamFileSize(pStream);
	RETURN_IF_FAILED(ddsDataSize <= UINT32_MAX);

	TinyDDS_Callbacks callbacks
	{
		[](void* user, char const* msg) { LOGF(eERROR, msg); },
		[](void* user, size_t size) { return tf_malloc(size); },
		[](void* user, void* memory) { tf_free(memory); },
		[](void* user, void* buffer, size_t byteCount) { return fsReadFromStream((FileStream*)user, buffer, (ssize_t)byteCount); },
		[](void* user, int64_t offset) { return fsSeekStream((FileStream*)user, SBO_START_OF_FILE, (ssize_t)offset); },
		[](void* user) { return (int64_t)fsGetStreamSeekPosition((FileStream*)user); }
	};

	TinyDDS_ContextHandle ctx = TinyDDS_CreateContext(&callbacks, pStream);
	bool headerOkay = TinyDDS_ReadHeader(ctx);
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

	TinyKtx_Callbacks callbacks
	{
		[](void* user, char const* msg) { LOGF(eERROR, msg); },
		[](void* user, size_t size) { return tf_malloc(size); },
		[](void* user, void* memory) { tf_free(memory); },
		[](void* user, void* buffer, size_t byteCount) { return fsReadFromStream((FileStream*)user, buffer, (ssize_t)byteCount); },
		[](void* user, int64_t offset) { return fsSeekStream((FileStream*)user, SBO_START_OF_FILE, (ssize_t)offset); },
		[](void *user) { return (int64_t)fsGetStreamSeekPosition((FileStream*)user); }
	};

	TinyKtx_ContextHandle ctx = TinyKtx_CreateContext(&callbacks, pStream);
	bool headerOkay = TinyKtx_ReadHeader(ctx);
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
/************************************************************************/
// BASIS Loading
/************************************************************************/
inline bool loadBASISTextureDesc(FileStream* pStream, TextureDesc* pOutDesc, void** ppOutData, uint32_t* pOutDataSize)
{
	if (pStream == NULL || fsGetStreamFileSize(pStream) <= 0)
		return false;

	basist::etc1_global_selector_codebook sel_codebook(basist::g_global_selector_cb_size, basist::g_global_selector_cb);

	size_t memSize = (size_t)fsGetStreamFileSize(pStream);
	void* basisData = tf_malloc(memSize);
	fsReadFromStream(pStream, basisData, memSize);

	basist::basisu_transcoder decoder(&sel_codebook);

	basist::basisu_file_info fileinfo;
	if (!decoder.get_file_info(basisData, (uint32_t)memSize, fileinfo))
	{
		LOGF(LogLevel::eERROR, "Failed retrieving Basis file information!");
		return false;
	}

	ASSERT(fileinfo.m_total_images == fileinfo.m_image_mipmap_levels.size());
	ASSERT(fileinfo.m_total_images == decoder.get_total_images(basisData, (uint32_t)memSize));

	basist::basisu_image_info imageinfo;
	decoder.get_image_info(basisData, (uint32_t)memSize, imageinfo, 0);

	TextureDesc& textureDesc = *pOutDesc;
	textureDesc.mWidth = imageinfo.m_width;
	textureDesc.mHeight = imageinfo.m_height;
	textureDesc.mDepth = 1;
	textureDesc.mMipLevels = fileinfo.m_image_mipmap_levels[0];
	textureDesc.mArraySize = fileinfo.m_total_images;
	textureDesc.mSampleCount = SAMPLE_COUNT_1;
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	textureDesc.mFormat = TinyImageFormat_UNDEFINED;

	bool isSRGB = (pOutDesc->mFlags & TEXTURE_CREATION_FLAG_SRGB) != 0;

	basist::transcoder_texture_format basisTextureFormat = basist::transcoder_texture_format::cTFTotalTextureFormats;

#if defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)
#if defined(TARGET_IOS)
	// Use PVRTC on iOS whenever possible
	// This makes sure that PVRTC support is maintained
	if (isPowerOf2(textureDesc.mWidth) && isPowerOf2(textureDesc.mHeight))
	{
		textureDesc.mFormat = isSRGB ? TinyImageFormat_PVRTC1_4BPP_SRGB : TinyImageFormat_PVRTC1_4BPP_UNORM;
		basisTextureFormat = imageinfo.m_alpha_flag ? basist::transcoder_texture_format::cTFPVRTC1_4_RGB : basist::transcoder_texture_format::cTFPVRTC1_4_RGBA;
	}
#endif
	if (TinyImageFormat_UNDEFINED == textureDesc.mFormat)
	{
		textureDesc.mFormat = isSRGB ? TinyImageFormat_ASTC_4x4_SRGB : TinyImageFormat_ASTC_4x4_UNORM;
		basisTextureFormat = basist::transcoder_texture_format::cTFASTC_4x4_RGBA;
	}
#else
	bool isNormalMap = (fileinfo.m_userdata0 == 1) || ((pOutDesc->mFlags & TEXTURE_CREATION_FLAG_NORMAL_MAP) != 0);
	if (!isNormalMap)
	{
		if (!imageinfo.m_alpha_flag)
		{
			textureDesc.mFormat = isSRGB ? TinyImageFormat_DXBC7_SRGB : TinyImageFormat_DXBC7_UNORM;
			basisTextureFormat = basist::transcoder_texture_format::cTFBC7_M6_RGB;
		}
		else
		{
			textureDesc.mFormat = isSRGB ? TinyImageFormat_DXBC7_SRGB : TinyImageFormat_DXBC7_UNORM;
			basisTextureFormat = basist::transcoder_texture_format::cTFBC7_M5;
		}
	}
	else
	{
		textureDesc.mFormat = TinyImageFormat_DXBC5_UNORM;
		basisTextureFormat = basist::transcoder_texture_format::cTFBC5_RG;
	}
#endif

	decoder.start_transcoding(basisData, (uint32_t)memSize);

	uint32_t requiredSize = util_get_surface_size(textureDesc.mFormat,
		textureDesc.mWidth, textureDesc.mHeight, textureDesc.mDepth, 1, 1,
		0, textureDesc.mMipLevels,
		0, textureDesc.mArraySize);
	void* startData = tf_malloc(requiredSize);
	uint8_t* data = (uint8_t*)startData;

	for (uint32_t s = 0; s < fileinfo.m_total_images; ++s)
	{
		uint32_t w = textureDesc.mWidth;
		uint32_t h = textureDesc.mHeight;
		uint32_t d = textureDesc.mDepth;

		for (uint32_t m = 0; m < fileinfo.m_image_mipmap_levels[s]; ++m)
		{
			uint32_t rowPitch = 0;
			uint32_t numBytes = 0;
			if (!util_get_surface_info(w, h, textureDesc.mFormat, &numBytes, &rowPitch, NULL))
			{
				return false;
			}

			uint32_t rowPitchInBlocks = rowPitch / (TinyImageFormat_BitSizeOfBlock(textureDesc.mFormat) >> 3);
			basist::basisu_image_level_info level_info;

			if (!decoder.get_image_level_info(basisData, (uint32_t)memSize, level_info, s, m))
			{
				LOGF(LogLevel::eERROR, "Failed retrieving image level information (%u %u)!\n", s, m);
				tf_free(basisData);
				tf_free(startData);
				return false;
			}

			if (!decoder.transcode_image_level(basisData, (uint32_t)memSize, s, m, data,
				(uint32_t)(rowPitchInBlocks * imageinfo.m_num_blocks_y), basisTextureFormat, 0, rowPitchInBlocks))
			{
				LOGF(LogLevel::eERROR, "Failed transcoding image level (%u %u)!", s, m);
				tf_free(basisData);
				tf_free(startData);
				return false;
			}

			data += numBytes;

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

	tf_free(basisData);

	*ppOutData = startData;
	*pOutDataSize = requiredSize;

	return true;
}
/************************************************************************/
// SVT Loading
/************************************************************************/
struct SVT_HEADER
{
	uint32_t mWidth;
	uint32_t mHeight;
	uint32_t mMipLevels;
	uint32_t mPageSize;
	uint32_t mComponentCount;
};

#if defined(DIRECT3D12) || defined(VULKAN)
inline bool loadSVTTextureDesc(FileStream* pStream, TextureDesc* pOutDesc)
{
#define RETURN_IF_FAILED(exp) \
if (!(exp))                   \
{                             \
	return false;             \
}

	RETURN_IF_FAILED(pStream);

	ssize_t svtDataSize = fsGetStreamFileSize(pStream);
	RETURN_IF_FAILED(svtDataSize <= UINT32_MAX);
	RETURN_IF_FAILED((svtDataSize > (ssize_t)(sizeof(SVT_HEADER))));

	SVT_HEADER header = {};
	ssize_t bytesRead = fsReadFromStream(pStream, &header, sizeof(SVT_HEADER));
	RETURN_IF_FAILED(bytesRead == sizeof(SVT_HEADER));

	TextureDesc& textureDesc = *pOutDesc;
	textureDesc.mWidth = header.mWidth;
	textureDesc.mHeight = header.mHeight;
	textureDesc.mMipLevels = header.mMipLevels;
	textureDesc.mDepth = 1;
	textureDesc.mArraySize = 1;
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	textureDesc.mSampleCount = SAMPLE_COUNT_1;
	textureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;

	return true;
}
#endif

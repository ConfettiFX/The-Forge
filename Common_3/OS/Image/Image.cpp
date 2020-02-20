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
//this is needed for unix as PATH_MAX is defined instead of MAX_PATH
#ifndef _WIN32
#include <limits.h>
#define MAX_PATH PATH_MAX
#endif

#include "../../ThirdParty/OpenSource/EASTL/functional.h"
#include "../../ThirdParty/OpenSource/EASTL/unordered_map.h"

#define IMAGE_CLASS_ALLOWED
#include "Image.h"
#include "../Interfaces/ILog.h"
#ifndef IMAGE_DISABLE_TINYEXR
#include "../../ThirdParty/OpenSource/TinyEXR/tinyexr.h"
#endif

#ifndef IMAGE_DISABLE_STB
//stb_image
#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC conf_malloc
#define STBI_REALLOC conf_realloc
#define STBI_FREE conf_free
#define STBI_ASSERT ASSERT
#if defined(__ANDROID__)
#define STBI_NO_SIMD
#endif
#include "../../ThirdParty/OpenSource/Nothings/stb_image.h"
//stb_image_write
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_MALLOC conf_malloc
#define STBIW_REALLOC conf_realloc
#define STBIW_FREE conf_free
#define STBIW_ASSERT ASSERT
#include "../../ThirdParty/OpenSource/Nothings/stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../../ThirdParty/OpenSource/Nothings/stb_image_resize.h"
#endif

#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_bits.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_decode.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_encode.h"
#define TINYDDS_IMPLEMENTATION
#include "../../ThirdParty/OpenSource/tinydds/tinydds.h"
#ifndef IMAGE_DISABLE_KTX
#define TINYKTX_IMPLEMENTATION
#include "../../ThirdParty/OpenSource/tinyktx/tinyktx.h"
#endif
#include "ImageHelper.h"

#include "../FileSystem/MemoryStream.h"

#include "../Interfaces/IMemory.h"

// Describes the header of a PVR header-texture
typedef struct PVR_Header_Texture_TAG
{
	uint32_t 	mVersion;
	uint32_t 	mFlags; //!< Various format flags.
	uint64_t 	mPixelFormat; //!< The pixel format, 8cc value storing the 4 channel identifiers and their respective sizes.
	uint32_t 	mColorSpace; //!< The Color Space of the texture, currently either linear RGB or sRGB.
	uint32_t 	mChannelType; //!< Variable type that the channel is stored in. Supports signed/uint32_t/short/char/float.
	uint32_t 	mHeight; //!< Height of the texture.
	uint32_t	mWidth; //!< Width of the texture.
	uint32_t 	mDepth; //!< Depth of the texture. (Z-slices)
	uint32_t 	mNumSurfaces; //!< Number of members in a Texture Array.
	uint32_t 	mNumFaces; //!< Number of faces in a Cube Map. Maybe be a value other than 6.
	uint32_t 	mNumMipMaps; //!< Number of MIP Maps in the texture - NB: Includes top level.
	uint32_t 	mMetaDataSize; //!< Size of the accompanying meta data.
} PVR_Texture_Header;

#ifdef TARGET_IOS
const uint32_t gPvrtexV3HeaderVersion = 0x03525650;
#endif

static uint32_t getBytesPerRow(uint32_t width, TinyImageFormat sourceFormat, uint32_t alignment)
{
	uint32_t blockWidth = TinyImageFormat_WidthOfBlock(sourceFormat);
	uint32_t blocksPerRow = (width + blockWidth - 1) / blockWidth;
	return round_up(blocksPerRow * TinyImageFormat_BitSizeOfBlock(sourceFormat) / 8, alignment);
}

// --- BLOCK DECODING ---

// TODO Decode these decode block don't handle SRGB properly
void iDecodeColorBlock(
	unsigned char* dest, int w, int h, int xOff, int yOff, TinyImageFormat format, int red, int blue, unsigned char* src)
{
	unsigned char colors[4][3];

	uint16 c0 = *(uint16*)src;
	uint16 c1 = *(uint16*)(src + 2);

	colors[0][0] = ((c0 >> 11) & 0x1F) << 3;
	colors[0][1] = ((c0 >> 5) & 0x3F) << 2;
	colors[0][2] = (c0 & 0x1F) << 3;

	colors[1][0] = ((c1 >> 11) & 0x1F) << 3;
	colors[1][1] = ((c1 >> 5) & 0x3F) << 2;
	colors[1][2] = (c1 & 0x1F) << 3;

	if (c0 > c1 || (format == TinyImageFormat_DXBC3_UNORM || format == TinyImageFormat_DXBC3_SRGB))
	{
		for (int i = 0; i < 3; i++)
		{
			colors[2][i] = (2 * colors[0][i] + colors[1][i] + 1) / 3;
			colors[3][i] = (colors[0][i] + 2 * colors[1][i] + 1) / 3;
		}
	}
	else
	{
		for (int i = 0; i < 3; i++)
		{
			colors[2][i] = (colors[0][i] + colors[1][i] + 1) >> 1;
			colors[3][i] = 0;
		}
	}

	src += 4;
	for (int y = 0; y < h; y++)
	{
		unsigned char* dst = dest + yOff * y;
		unsigned int   indexes = src[y];
		for (int x = 0; x < w; x++)
		{
			unsigned int index = indexes & 0x3;
			dst[red] = colors[index][0];
			dst[1] = colors[index][1];
			dst[blue] = colors[index][2];
			indexes >>= 2;

			dst += xOff;
		}
	}
}

void iDecodeDXT3Block(unsigned char* dest, int w, int h, int xOff, int yOff, unsigned char* src)
{
	for (int y = 0; y < h; y++)
	{
		unsigned char* dst = dest + yOff * y;
		unsigned int   alpha = ((unsigned short*)src)[y];
		for (int x = 0; x < w; x++)
		{
			*dst = (alpha & 0xF) * 17;
			alpha >>= 4;
			dst += xOff;
		}
	}
}

void iDecodeDXT5Block(unsigned char* dest, int w, int h, int xOff, int yOff, unsigned char* src)
{
	unsigned char a0 = src[0];
	unsigned char a1 = src[1];
	uint64_t      alpha = (*(uint64_t*)src) >> 16;

	for (int y = 0; y < h; y++)
	{
		unsigned char* dst = dest + yOff * y;
		for (int x = 0; x < w; x++)
		{
			int k = ((unsigned int)alpha) & 0x7;
			if (k == 0)
			{
				*dst = a0;
			}
			else if (k == 1)
			{
				*dst = a1;
			}
			else if (a0 > a1)
			{
				*dst = (unsigned char)(((8 - k) * a0 + (k - 1) * a1) / 7);
			}
			else if (k >= 6)
			{
				*dst = (k == 6) ? 0 : 255;
			}
			else
			{
				*dst = (unsigned char)(((6 - k) * a0 + (k - 1) * a1) / 5);
			}
			alpha >>= 3;

			dst += xOff;
		}
		if (w < 4)
			alpha >>= (3 * (4 - w));
	}
}

void iDecodeCompressedImage(unsigned char* dest, unsigned char* src, const int width, const int height, const TinyImageFormat format, uint srcRowPadding, uint dstRowStride)
{
	int sx = (width < 4) ? width : 4;
	int sy = (height < 4) ? height : 4;
	
	int nChannels = TinyImageFormat_ChannelCount(format);

	for (int y = 0; y < height; y += 4)
	{
		for (int x = 0; x < width; x += 4)
		{
			unsigned char* dst = dest + y * dstRowStride + x * nChannels;
			if (format == TinyImageFormat_DXBC2_UNORM || format == TinyImageFormat_DXBC2_SRGB)
			{
				iDecodeDXT3Block(dst + 3, sx, sy, nChannels, width * nChannels, src);
			}
			else if (format == TinyImageFormat_DXBC3_UNORM || format == TinyImageFormat_DXBC3_SRGB)
			{
				iDecodeDXT5Block(dst + 3, sx, sy, nChannels, width * nChannels, src);
			}
			if ((format == TinyImageFormat_DXBC1_RGBA_UNORM || format == TinyImageFormat_DXBC1_RGB_UNORM) ||
					(format == TinyImageFormat_DXBC1_RGBA_SRGB || format == TinyImageFormat_DXBC1_RGB_SRGB))
            {
				iDecodeColorBlock(dst, sx, sy, nChannels, width * nChannels, format, 0, 2, src);
			}
			else
			{
				if (format == TinyImageFormat_DXBC4_UNORM || format == TinyImageFormat_DXBC4_SNORM)
				{
					iDecodeDXT5Block(dst, sx, sy, 1, width, src);
				}
				else if (format == TinyImageFormat_DXBC5_UNORM || format == TinyImageFormat_DXBC5_SNORM)
				{
					iDecodeDXT5Block(dst, sx, sy, 2, width * 2, src + 8);
					iDecodeDXT5Block(dst + 1, sx, sy, 2, width * 2, src);
				}
				else
					return;
			}
            src += TinyImageFormat_BitSizeOfBlock(format) / 8;

		}
		src += srcRowPadding;
	}
}

Image::Image()
{
	pData = NULL;
	mLoadFilePath = NULL;
	mWidth = 0;
	mHeight = 0;
	mDepth = 0;
	mMipMapCount = 0;
	mArrayCount = 0;
	mFormat = TinyImageFormat_UNDEFINED;
	mOwnsMemory = true;
	mLinearLayout = true;
	mRowAlignment = 1;
	mSubtextureAlignment = 1;

#ifndef IMAGE_DISABLE_GOOGLE_BASIS
	// Init basisu
	basist::basisu_transcoder_init();
#endif
}

Image::Image(const Image& img)
{
	mWidth = img.mWidth;
	mHeight = img.mHeight;
	mDepth = img.mDepth;
	mMipMapCount = img.mMipMapCount;
	mArrayCount = img.mArrayCount;
	mFormat = img.mFormat;
	mLinearLayout = img.mLinearLayout;
	mRowAlignment = 1;
	mSubtextureAlignment = 1;

	size_t size = GetSizeInBytes();
	pData = (unsigned char*)conf_malloc(sizeof(unsigned char) * size);
	memcpy(pData, img.pData, size);
	mLoadFilePath = fsCopyPath(img.mLoadFilePath);
}

unsigned char* Image::Create(const TinyImageFormat fmt, const int w, const int h, const int d, const int mipMapCount, const int arraySize, const unsigned char* rawData, const int rowAlignment, const int subtextureAlignment)
{
	mFormat = fmt;
	mWidth = w;
	mHeight = h;
	mDepth = d;
	mMipMapCount = mipMapCount;
	mArrayCount = arraySize;
	mOwnsMemory = false;
	mMipsAfterSlices = false;
	mRowAlignment = rowAlignment;
	mSubtextureAlignment = subtextureAlignment;

	pData = (uint8_t*)rawData;
	mLoadFilePath = NULL;

	return pData;
}

void Image::RedefineDimensions(
	const TinyImageFormat fmt, const int w, const int h, const int d, const int mipMapCount, const int arraySize)
{
	//Redefine image that was loaded in
	mFormat = fmt;
	mWidth = w;
	mHeight = h;
	mDepth = d;
	mMipMapCount = mipMapCount;
	mArrayCount = arraySize;

	switch (mFormat)
	{
	case TinyImageFormat_PVRTC1_2BPP_UNORM:
	case TinyImageFormat_PVRTC1_2BPP_SRGB:
	case TinyImageFormat_PVRTC1_4BPP_UNORM:
	case TinyImageFormat_PVRTC1_4BPP_SRGB:
		mLinearLayout = false;
		break;
	default:
		mLinearLayout = true;
	}
}

void Image::Destroy()
{
    if (mLoadFilePath)
    {
        mLoadFilePath = NULL;
    }
    
	if (pData && mOwnsMemory)
	{
		conf_free(pData);
		pData = NULL;
	}
}

void Image::Clear()
{
	Destroy();

	mWidth = 0;
	mHeight = 0;
	mDepth = 0;
	mMipMapCount = 0;
	mArrayCount = 0;
	mFormat = TinyImageFormat_UNDEFINED;
	mMipsAfterSlices = false;
	mRowAlignment = 1;
}

unsigned char* Image::GetPixels(unsigned char* pDstData, const uint mipMapLevel, const uint dummy)
{
	UNREF_PARAM(dummy);
	return (mipMapLevel < mMipMapCount) ? pDstData + GetMipMappedSize(0, mipMapLevel) : NULL;
}

unsigned char* Image::GetPixels(const uint mipMapLevel) const
{
	return (mipMapLevel < mMipMapCount) ? pData + GetMipMappedSize(0, mipMapLevel) : NULL;
}

unsigned char* Image::GetPixels(const uint mipMapLevel, const uint arraySlice) const
{
	if (mipMapLevel >= mMipMapCount || arraySlice >= mArrayCount * (IsCube() ? 6 : 1))
		return NULL;
    
    // two ways of storing slices and mipmaps
    // 1. Old Image way. memory slices * ((w*h*d)*mipmaps)
    // 2. Mips after slices way. There are w*h*d*s*mipmaps where slices stays constant(doesn't reduce)
    if(!mMipsAfterSlices) {
        return pData + GetMipMappedSize(0, mMipMapCount) * arraySlice + GetMipMappedSize(0, mipMapLevel);
    } else {
        return pData + GetMipMappedSize(0, mipMapLevel) + arraySlice * GetArraySliceSize(mipMapLevel);
    }
}

size_t Image::GetSizeInBytes() const
{
	return GetMipMappedSize(0, mMipMapCount) * mArrayCount;
}

uint32_t Image::GetBytesPerRow(const uint32_t mipMapLevel) const
{
	return getBytesPerRow(GetWidth(mipMapLevel), mFormat, mRowAlignment);
}

uint32_t Image::GetRowCount(const uint32_t mipMapLevel) const
{
	uint32_t blockHeight = TinyImageFormat_HeightOfBlock(mFormat);
	return (GetHeight(mipMapLevel) + blockHeight - 1) / blockHeight;
}

uint32_t Image::GetWidth(const int mipMapLevel) const
{
	uint32_t a = mWidth >> mipMapLevel;
	return (a == 0) ? 1 : a;
}

uint32_t Image::GetHeight(const int mipMapLevel) const
{
	uint32_t a = mHeight >> mipMapLevel;
	return (a == 0) ? 1 : a;
}

uint Image::GetDepth(const int mipMapLevel) const
{
	uint32_t a = mDepth >> mipMapLevel;
	return (a == 0) ? 1 : a;
}

uint32_t Image::GetMipMapCountFromDimensions() const
{
	uint32_t m = max(mWidth, mHeight);
	m = max(m, mDepth);

	uint32_t i = 0;
	while (m > 0)
	{
		m >>= 1;
		i++;
	}

	return i;
}

uint Image::GetArraySliceSize(const uint mipMapLevel, TinyImageFormat srcFormat) const
{
	uint32_t w = GetWidth(mipMapLevel);
    uint32_t d = GetDepth(mipMapLevel);
    if(d == 0) d = 1;

    if (srcFormat == TinyImageFormat_UNDEFINED)
        srcFormat = mFormat;
	
	uint32_t bz = TinyImageFormat_DepthOfBlock(srcFormat);
	
	uint32_t rowCount = GetRowCount(mipMapLevel);
	uint32_t bytesPerRow = getBytesPerRow(w, srcFormat, mRowAlignment);
	
	return bytesPerRow * rowCount * ((d + bz - 1) / bz);
}

uint Image::GetNumberOfPixels(const uint firstMipMapLevel, uint nMipMapLevels) const
{
	int w = GetWidth(firstMipMapLevel);
	int h = GetHeight(firstMipMapLevel);
	int d = GetDepth(firstMipMapLevel);
	int size = 0;
	while (nMipMapLevels)
	{
		size += w * h * d;
		w >>= 1;
		h >>= 1;
		d >>= 1;
		if (w + h + d == 0)
			break;
		if (w == 0)
			w = 1;
		if (h == 0)
			h = 1;
		if (d == 0)
			d = 1;

		nMipMapLevels--;
	}

	return (mDepth == 0) ? 6 * size : size;
}

bool Image::GetColorRange(float& min, float& max)
{
	// TODO Deano replace with TinyImageFormat decode calls

	if (TinyImageFormat_IsFloat(mFormat) && TinyImageFormat_ChannelBitWidth(mFormat, TinyImageFormat_LC_Red) == 32)
		return false;

	uint32_t nElements = GetNumberOfPixels(0, mMipMapCount) * TinyImageFormat_ChannelCount(mFormat) * mArrayCount;

	if (nElements <= 0)
		return false;

	float minVal = FLT_MAX;
	float maxVal = -FLT_MAX;
	for (uint32_t i = 0; i < nElements; i++)
	{
		float d = ((float*)pData)[i];
		if (d > maxVal)
			maxVal = d;
		if (d < minVal)
			minVal = d;
	}
	max = maxVal;
	min = minVal;

	return true;
}
bool Image::Normalize()
{
	// TODO Deano replace with TinyImageFormat decode calls

	if (TinyImageFormat_IsFloat(mFormat) && TinyImageFormat_ChannelBitWidth(mFormat, TinyImageFormat_LC_Red) == 32)
		return false;

	float min, max;
	GetColorRange(min, max);

	uint32_t nElements = GetMipMappedSize();

	float s = 1.0f / (max - min);
	float b = -min * s;
	for (uint32_t i = 0; i < nElements; i++)
	{
		float d = ((float*)pData)[i];
		((float*)pData)[i] = d * s + b;
	}

	return true;
}

bool Image::Uncompress(uint newRowAlignment, uint newSubtextureAlignment)
{
	// only dxtc at the moment
	uint64_t const tifname = (TinyImageFormat_Code(mFormat) & TinyImageFormat_NAMESPACE_REQUIRED_BITS);
	if( tifname != TinyImageFormat_NAMESPACE_DXTC)
		return false;

	// only BC 1 to 5 at the moment
	if(mFormat == TinyImageFormat_DXBC6H_UFLOAT || mFormat == TinyImageFormat_DXBC6H_SFLOAT ||
			mFormat == TinyImageFormat_DXBC7_UNORM || mFormat == TinyImageFormat_DXBC7_SRGB)
		return false;

	TinyImageFormat destFormat;
	switch(TinyImageFormat_ChannelCount(mFormat)) {
	case 1: destFormat = TinyImageFormat_R8_UNORM; break;
	case 2: destFormat = TinyImageFormat_R8G8_UNORM; break;
	case 3: destFormat = TinyImageFormat_R8G8B8_UNORM; break;
	case 4: destFormat = TinyImageFormat_R8G8B8A8_UNORM; break;
	default:
		ASSERT(false);
		destFormat = TinyImageFormat_R8_UNORM;
		break;
	}
	
	uint srcRowAlignment = GetRowAlignment();
	uint srcSubtextureAlignment = GetSubtextureAlignment();
	mRowAlignment = newRowAlignment;
	mSubtextureAlignment = max(newSubtextureAlignment, newRowAlignment);
	
	ubyte* newPixels = (ubyte*)conf_malloc(sizeof(ubyte) * GetMipMappedSize(0, mMipMapCount, destFormat));

	int    level = 0;
	ubyte *src, *dst = newPixels;
	while ((src = GetPixels(level)) != NULL)
	{
		int w = GetWidth(level);
		int h = GetHeight(level);
		int d = (mDepth == 0) ? 6 : GetDepth(level);

		int dstSliceSize = GetArraySliceSize(level, destFormat);
		int srcSliceSize = GetArraySliceSize(level, mFormat);
		
		uint srcRowPadding = getBytesPerRow(w, mFormat, srcRowAlignment) - getBytesPerRow(w, mFormat, 1);
		uint dstRowStride = GetBytesPerRow(level);

		for (int slice = 0; slice < d; slice++)
		{
			iDecodeCompressedImage(dst, src, w, h, mFormat, srcRowPadding, dstRowStride);

			dst += round_up(dstSliceSize, mSubtextureAlignment);
			src += round_up(srcSliceSize, srcSubtextureAlignment);
		}
		level++;
	}

	mFormat = destFormat;

	Destroy();
	pData = newPixels;

	return true;
}

bool Image::Unpack()
{
	TinyImageFormat destFormat;
	if(TinyImageFormat_IsFloat(mFormat)) {
		switch (TinyImageFormat_ChannelCount(mFormat)) {
		case 1: destFormat = TinyImageFormat_R32_SFLOAT;
			break;
		case 2: destFormat = TinyImageFormat_R32G32_SFLOAT;
			break;
		case 3: destFormat = TinyImageFormat_R32G32B32_SFLOAT;
			break;
		case 4: destFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
			break;
		default: ASSERT(false);
			destFormat = TinyImageFormat_R32_SFLOAT;
			break;
		}

	} else if(TinyImageFormat_IsSigned(mFormat)) {
		switch (TinyImageFormat_ChannelCount(mFormat)) {
		case 1: destFormat = TinyImageFormat_R8_SNORM;
			break;
		case 2: destFormat = TinyImageFormat_R8G8_SNORM;
			break;
		case 3: destFormat = TinyImageFormat_R8G8B8_SNORM;
			break;
		case 4: destFormat = TinyImageFormat_R8G8B8A8_SNORM;
			break;
		default: ASSERT(false);
			destFormat = TinyImageFormat_R8_SNORM;
			break;
		}

	} else if(TinyImageFormat_IsSRGB(mFormat)) {
		switch (TinyImageFormat_ChannelCount(mFormat)) {
		case 1: destFormat = TinyImageFormat_R8_SRGB;
			break;
		case 2: destFormat = TinyImageFormat_R8G8_SRGB;
			break;
		case 3: destFormat = TinyImageFormat_R8G8B8_SRGB;
			break;
		case 4: destFormat = TinyImageFormat_R8G8B8A8_SRGB;
			break;
		default: ASSERT(false);
			destFormat = TinyImageFormat_R8_SRGB;
			break;
		}
	} else {
		switch (TinyImageFormat_ChannelCount(mFormat)) {
		case 1: destFormat = TinyImageFormat_R8_UNORM;
			break;
		case 2: destFormat = TinyImageFormat_R8G8_UNORM;
			break;
		case 3: destFormat = TinyImageFormat_R8G8B8_UNORM;
			break;
		case 4: destFormat = TinyImageFormat_R8G8B8A8_UNORM;
			break;
		default: ASSERT(false);
			destFormat = TinyImageFormat_R8_UNORM;
			break;
		}
	}

	return Convert(destFormat);
}

uint32_t Image::GetMipMappedSize(const uint firstMipMapLevel, uint32_t nMipMapLevels, TinyImageFormat srcFormat) const
{
	uint32_t w = GetWidth(firstMipMapLevel);
	uint32_t h = GetHeight(firstMipMapLevel);
	uint32_t d = GetDepth(firstMipMapLevel);
    d = d ? d : 1; // if a cube map treats a 2D texture for calculations
    uint32_t const s = GetArrayCount();
	uint32_t subtextureAlignment = GetSubtextureAlignment();

	if (srcFormat == TinyImageFormat_UNDEFINED)
		srcFormat = mFormat;
	
	if (nMipMapLevels == ALL_MIPLEVELS)
		nMipMapLevels = mMipMapCount - firstMipMapLevel;
	
	// PVR formats get special case
	uint64_t const tifname = (TinyImageFormat_Code(mFormat) & TinyImageFormat_NAMESPACE_REQUIRED_BITS);
	if( tifname == TinyImageFormat_NAMESPACE_PVRTC)
	{
        // AFAIK pvr isn't supported for arrays
        ASSERT(s == 1);
		uint32_t totalSize = 0;
		uint32_t sizeX = w;
		uint32_t sizeY = h;
		uint32_t sizeD = d;
		int level = nMipMapLevels;
		
		uint minWidth = 8;
		uint minHeight = 8;
		uint minDepth = 1;
		int bpp = 4;
		
		if (srcFormat == TinyImageFormat_PVRTC1_2BPP_UNORM || srcFormat == TinyImageFormat_PVRTC1_2BPP_SRGB)
		{
			minWidth = 16;
			minHeight = 8;
			bpp = 2;
		}
		
		while (level > 0)
		{
			// If pixel format is compressed, the dimensions need to be padded.
			uint paddedWidth = sizeX + ((-1 * sizeX) % minWidth);
			uint paddedHeight = sizeY + ((-1 * sizeY) % minHeight);
			uint paddedDepth = sizeD + ((-1 * sizeD) % minDepth);
			
			int mipSize = paddedWidth * paddedHeight * paddedDepth * bpp / 8;
			
			totalSize += mipSize;
			
			unsigned int MinimumSize = 1;
			sizeX = max(sizeX / 2, MinimumSize);
			sizeY = max(sizeY / 2, MinimumSize);
			sizeD = max(sizeD / 2, MinimumSize);
			level--;
		}
		
		return round_up(totalSize, subtextureAlignment);
	}
	
	uint32_t size = 0;
	for (uint32_t i = 0; i < nMipMapLevels; i += 1)
	{
		uint32_t by = TinyImageFormat_HeightOfBlock(srcFormat);
		uint32_t bz = TinyImageFormat_DepthOfBlock(srcFormat);
		
		uint32_t rowCount = (h + by - 1) / by;
		uint32_t bytesPerRow = GetBytesPerRow(i + firstMipMapLevel);
		
		size += round_up(bytesPerRow * rowCount * ((d + bz - 1) / bz), subtextureAlignment);

		w >>= 1;
		h >>= 1;
		d >>= 1;
		if (w + h + d == 0)
			break;
		if (w == 0)
			w = 1;
		if (h == 0)
			h = 1;
		if (d == 0)
			d = 1;
	}

	// mips after slices means the slice count is included in mipsize but slices doesn't reduce
	// as slices are included, cubemaps also just fall out
	if(mMipsAfterSlices) return (mDepth == 0) ? 6 * size * s : size * s;
	else return (mDepth == 0) ? 6 * size : size;
}

static void tinyktxddsCallbackError(void *user, char const *msg) {
	LOGF(LogLevel::eERROR, "Tiny_ ERROR: %s", msg);
}
static void *tinyktxddsCallbackAlloc(void *user, size_t size) {
	return conf_malloc(size);
}
static void tinyktxddsCallbackFree(void *user, void *data) {
	conf_free(data);
}
static size_t tinyktxddsCallbackRead(void *user, void* data, size_t size) {
	FileStream* file = (FileStream*) user;
    return fsReadFromStream(file, data, size);
}
static bool tinyktxddsCallbackSeek(void *user, int64_t offset) {
    FileStream* file = (FileStream*) user;
    return fsSeekStream(file, SBO_START_OF_FILE, offset);

}
static int64_t tinyktxddsCallbackTell(void *user) {
    FileStream* file = (FileStream*) user;
    return fsGetStreamSeekPosition(file);
}

static void tinyktxddsCallbackWrite(void* user, void const* data, size_t size) {
    FileStream* file = (FileStream*) user;
    fsWriteToStream(file, data, size);
}

// Load Image Data form mData functions
#ifndef IMAGE_DISABLE_STB
ImageLoadingResult iLoadSTBFromStream(Image* pImage,
	FileStream* pStream, memoryAllocationFunc pAllocator, void* pUserData)
{
	if (pStream == NULL || fsGetStreamFileSize(pStream) == 0)
		return IMAGE_LOADING_RESULT_DECODING_FAILED;

	LOGF(LogLevel::eWARNING, "Image %s is not in a GPU-compressed format; please use a GPU-compressed format for best performance.", fsGetPathAsNativeString(pImage->GetPath()));

	int width, height, components;
	
	size_t memSize = fsGetStreamFileSize(pStream);
	void* memory = fsGetStreamBufferIfPresent(pStream);
	stbi_uc* imageBytes = NULL;
	
	if (memory)
	{
		imageBytes = stbi_load_from_memory((stbi_uc*)memory, (int)memSize, &width, &height, &components, 0);
	}
	else
	{
		memory = conf_malloc(memSize);
		fsReadFromStream(pStream, memory, memSize);
		imageBytes = stbi_load_from_memory((stbi_uc*)memory, (int)memSize, &width, &height, &components, 0);
		conf_free(memory);
	}

	if (!imageBytes)
	{
		return IMAGE_LOADING_RESULT_DECODING_FAILED;
	}
	
	TinyImageFormat format;
	switch (components) {
		case 1:
			format = TinyImageFormat_R8_UNORM;
			break;
		case 2:
			format = TinyImageFormat_R8G8_UNORM;
			break;
		default:
			format = TinyImageFormat_R8G8B8A8_UNORM;
			break;
	}
	
	pImage->RedefineDimensions(format, width, height, 1, 1, 1);
	
	size_t destinationBytesPerRow = pImage->GetBytesPerRow();
	size_t imageByteSize = pImage->GetSizeInBytes();
	
	if (pAllocator)
		pImage->SetPixels((unsigned char*)pAllocator(pImage, imageByteSize, pImage->GetSubtextureAlignment(), pUserData));
	else
		pImage->SetPixels((unsigned char*)conf_malloc(imageByteSize), true);
	
	if (!pImage->GetPixels())
		return IMAGE_LOADING_RESULT_ALLOCATION_FAILED;
	
	unsigned char* dstPixels = pImage->GetPixels(0, 0);
	if (components == 3)
	{
		size_t sourceBytesPerRow = 3 * width;
		for (uint32_t y = 0; y < (uint32_t)height; y += 1)
		{
			unsigned char* srcRow = imageBytes + y * sourceBytesPerRow;
			unsigned char* dstRow = dstPixels + y * destinationBytesPerRow;
			
			for (uint32_t x = 0; x < (uint32_t)width; x += 1)
			{
				dstRow[4 * x + 0] = srcRow[3 * x + 0];
				dstRow[4 * x + 1] = srcRow[3 * x + 1];
				dstRow[4 * x + 2] = srcRow[3 * x + 2];
				dstRow[4 * x + 3] = ~0;
			}
		}
	}
	else
	{
		size_t sourceBytesPerRow = TinyImageFormat_BitSizeOfBlock(format) / 8 * width;
		for (uint32_t y = 0; y < (uint32_t)height; y += 1)
		{
			unsigned char* srcRow = imageBytes + y * sourceBytesPerRow;
			unsigned char* dstRow = dstPixels + y * destinationBytesPerRow;
			memcpy(dstRow, srcRow, sourceBytesPerRow);
		}
	}
	
	stbi_image_free(imageBytes);
    
	return IMAGE_LOADING_RESULT_SUCCESS;
}
#endif

ImageLoadingResult iLoadDDSFromStream(Image* pImage,
	FileStream* pStream, memoryAllocationFunc pAllocator, void* pUserData)
{
	if (pStream == NULL || fsGetStreamFileSize(pStream) == 0)
		return IMAGE_LOADING_RESULT_DECODING_FAILED;

	TinyDDS_Callbacks callbacks {
			&tinyktxddsCallbackError,
			&tinyktxddsCallbackAlloc,
			&tinyktxddsCallbackFree,
			tinyktxddsCallbackRead,
			&tinyktxddsCallbackSeek,
			&tinyktxddsCallbackTell
	};

	auto ctx = TinyDDS_CreateContext(&callbacks, (void*)pStream);
	bool headerOkay = TinyDDS_ReadHeader(ctx);
	if(!headerOkay) {
		TinyDDS_DestroyContext(ctx);
		return IMAGE_LOADING_RESULT_DECODING_FAILED;
	}

	uint32_t w = TinyDDS_Width(ctx);
	uint32_t h = TinyDDS_Height(ctx);
	uint32_t d = TinyDDS_Depth(ctx);
	uint32_t s = TinyDDS_ArraySlices(ctx);
	uint32_t mm = TinyDDS_NumberOfMipmaps(ctx);
	TinyImageFormat fmt = TinyImageFormat_FromTinyDDSFormat(TinyDDS_GetFormat(ctx));
	if(fmt == TinyImageFormat_UNDEFINED)
	{
		TinyDDS_DestroyContext(ctx);
		return IMAGE_LOADING_RESULT_DECODING_FAILED;
	}

	// TheForge Image uses d = 0 as cubemap marker
	if(TinyDDS_IsCubemap(ctx))
		d = 0;
	else
		d = d ? d : 1;
	s = s ? s : 1;

	pImage->RedefineDimensions(fmt, w, h, d, mm, s);
	pImage->SetMipsAfterSlices(true); // tinyDDS API is mips after slices even if DDS traditionally weren't

	if (d == 0)
		s *= 6;

	size_t size = pImage->GetSizeInBytes();

	if (pAllocator)
		pImage->SetPixels((unsigned char*)pAllocator(pImage, size, pImage->GetSubtextureAlignment(), pUserData));
	else
		pImage->SetPixels((unsigned char*)conf_malloc(sizeof(unsigned char) * size), true);
	
	if (!pImage->GetPixels())
	{
		TinyDDS_DestroyContext(ctx);
		return IMAGE_LOADING_RESULT_ALLOCATION_FAILED;
	}
	
	for (uint mipMapLevel = 0; mipMapLevel < pImage->GetMipMapCount(); mipMapLevel++)
	{
		size_t blocksPerRow = (w + TinyImageFormat_WidthOfBlock(fmt) - 1) / TinyImageFormat_WidthOfBlock(fmt);
		size_t sourceBytesPerRow = TinyImageFormat_BitSizeOfBlock(fmt) / 8 * blocksPerRow;
		size_t destinationBytesPerRow = pImage->GetBytesPerRow(mipMapLevel);
		size_t rowCount = (h + TinyImageFormat_HeightOfBlock(fmt) - 1) / TinyImageFormat_HeightOfBlock(fmt);
		size_t depthCount = d == 0 ? 1 : (TinyDDS_IsCubemap(ctx) ? 1 : d);

		size_t const expectedSize = sourceBytesPerRow * rowCount * depthCount * s;
		size_t const fileSize = TinyDDS_ImageSize(ctx, mipMapLevel);
		if (expectedSize != fileSize)
		{
			LOGF(LogLevel::eERROR, "DDS file %s mipmap %i size error %liu < %liu", fsGetPathAsNativeString(pImage->GetPath()), mipMapLevel, expectedSize, fileSize);
			return IMAGE_LOADING_RESULT_DECODING_FAILED;
		}
		
		const unsigned char* src = (const unsigned char*)TinyDDS_ImageRawData(ctx, mipMapLevel);
		
		for (uint slice = 0; slice < s; slice += 1)
		{
			unsigned char* dst = pImage->GetPixels(mipMapLevel, slice);
			for (uint row = 0; row < rowCount * depthCount; row += 1)
			{
				const unsigned char* srcRow = src + (slice * rowCount + row) * sourceBytesPerRow;
				unsigned char* dstRow = dst + row * destinationBytesPerRow;
				memcpy(dstRow, srcRow, sourceBytesPerRow);
			}
		}
		
		w /= 2;
		h /= 2;
		d /= 2;
	}

	TinyDDS_DestroyContext(ctx);
    
	return IMAGE_LOADING_RESULT_SUCCESS;
}

ImageLoadingResult iLoadPVRFromStream(Image* pImage, FileStream* pStream, memoryAllocationFunc pAllocator, void* pUserData)
{
#ifndef TARGET_IOS
	LOGF(LogLevel::eERROR, "Load PVR failed: Only supported on iOS targets.");
	return IMAGE_LOADING_RESULT_DECODING_FAILED;
#else
	
	// TODO: Image
	// - no support for PVRTC2 at the moment since it isn't supported on iOS devices.
	// - only new PVR header V3 is supported at the moment.  Should we add legacy for V2 and V1?
	// - metadata is ignored for now.  Might be useful to implement it if the need for metadata arises (eg. padding, atlas coordinates, orientations, border data, etc...).
	// - flags are also ignored for now.  Currently a flag of 0x02 means that the color have been pre-multiplied byt the alpha values.
	
	// Assumptions:
	// - it's assumed that the texture is already twiddled (ie. Morton).  This should always be the case for PVRTC V3.
	
	PVR_Texture_Header psPVRHeader = {};
	fsReadFromStream(pStream, &psPVRHeader, sizeof(PVR_Texture_Header));

	if (psPVRHeader.mVersion != gPvrtexV3HeaderVersion)
	{
		LOGF(LogLevel::eERROR, "Load PVR failed: Not a valid PVR V3 header.");
		return IMAGE_LOADING_RESULT_DECODING_FAILED;
	}
	
	if (psPVRHeader.mPixelFormat > 3)
	{
		LOGF(LogLevel::eERROR, "Load PVR failed: Not a supported PVR pixel format.  Only PVRTC is supported at the moment.");
		return IMAGE_LOADING_RESULT_DECODING_FAILED;
	}
	
	if (psPVRHeader.mNumSurfaces > 1 && psPVRHeader.mNumFaces > 1)
	{
		LOGF(LogLevel::eERROR, "Load PVR failed: Loading arrays of cubemaps isn't supported.");
		return IMAGE_LOADING_RESULT_DECODING_FAILED;
	}

	uint32_t width = psPVRHeader.mWidth;
	uint32_t height = psPVRHeader.mHeight;
	uint32_t depth = (psPVRHeader.mNumFaces > 1) ? 0 : psPVRHeader.mDepth;
	uint32_t mipMapCount = psPVRHeader.mNumMipMaps;
	uint32_t arrayCount = psPVRHeader.mNumSurfaces;
	bool const srgb = (psPVRHeader.mColorSpace == 1);
	TinyImageFormat imageFormat = TinyImageFormat_UNDEFINED;

	switch (psPVRHeader.mPixelFormat)
	{
	case 0:
    case 1:
            imageFormat = srgb ? TinyImageFormat_PVRTC1_2BPP_SRGB : TinyImageFormat_PVRTC1_2BPP_UNORM;
		break;
	case 2:
    case 3:
            imageFormat = srgb ? TinyImageFormat_PVRTC1_4BPP_SRGB : TinyImageFormat_PVRTC1_4BPP_UNORM;
		break;
	default:    // NOT SUPPORTED
		LOGF(LogLevel::eERROR, "Load PVR failed: pixel type not supported. ");
		ASSERT(0);
		return IMAGE_LOADING_RESULT_DECODING_FAILED;
	}

	if (depth != 0)
		arrayCount *= psPVRHeader.mNumFaces;

	pImage->RedefineDimensions(imageFormat, width, height, depth, mipMapCount, arrayCount);


	// Extract the pixel data
	size_t totalHeaderSizeWithMetadata = sizeof(PVR_Texture_Header) + psPVRHeader.mMetaDataSize;
	size_t pixelDataSize = pImage->GetSizeInBytes();

	if (pAllocator)
		pImage->SetPixels((unsigned char*)pAllocator(pImage, sizeof(unsigned char) * pixelDataSize, pImage->GetSubtextureAlignment(), pUserData));
	else
		pImage->SetPixels((unsigned char*)conf_malloc(sizeof(unsigned char) * pixelDataSize), true);
	
	if (!pImage->GetPixels())
		return IMAGE_LOADING_RESULT_ALLOCATION_FAILED;

	fsSeekStream(pStream, SBO_CURRENT_POSITION, psPVRHeader.mMetaDataSize);
	fsReadFromStream(pStream, pImage->GetPixels(), pixelDataSize);

	return IMAGE_LOADING_RESULT_SUCCESS;
#endif
}

#ifndef IMAGE_DISABLE_KTX
ImageLoadingResult iLoadKTXFromStream(Image* pImage, FileStream* pStream, memoryAllocationFunc pAllocator /*= NULL*/, void* pUserData /*= NULL*/)
{
	TinyKtx_Callbacks callbacks {
			&tinyktxddsCallbackError,
			&tinyktxddsCallbackAlloc,
			&tinyktxddsCallbackFree,
			&tinyktxddsCallbackRead,
			&tinyktxddsCallbackSeek,
			&tinyktxddsCallbackTell
	};

	auto ctx = TinyKtx_CreateContext( &callbacks, (void*)pStream);
	bool headerOkay = TinyKtx_ReadHeader(ctx);
	if(!headerOkay) {
		TinyKtx_DestroyContext(ctx);
		return IMAGE_LOADING_RESULT_DECODING_FAILED;
	}

	uint32_t w = TinyKtx_Width(ctx);
	uint32_t h = TinyKtx_Height(ctx);
	uint32_t d = TinyKtx_Depth(ctx);
	uint32_t s = TinyKtx_ArraySlices(ctx);
	uint32_t mm = TinyKtx_NumberOfMipmaps(ctx);
	TinyImageFormat fmt = TinyImageFormat_FromTinyKtxFormat(TinyKtx_GetFormat(ctx));
	if(fmt == TinyImageFormat_UNDEFINED) {
		TinyKtx_DestroyContext(ctx);
		return IMAGE_LOADING_RESULT_DECODING_FAILED;
	}

    // TheForge Image uses d = 0 as cubemap marker
    if(TinyKtx_IsCubemap(ctx)) d = 0;
    else d = d ? d : 1;
    s = s ? s : 1;
	
	if (d == 0)
		s *= 6;

	pImage->RedefineDimensions(fmt, w, h, d, mm, s);
    pImage->SetMipsAfterSlices(true); // tinyDDS API is mips after slices even if DDS traditionally weren't

	size_t size = pImage->GetSizeInBytes();

	if (pAllocator)
		pImage->SetPixels((uint8_t*)pAllocator(pImage, size, pImage->GetSubtextureAlignment(), pUserData));
	else
		pImage->SetPixels((uint8_t*)conf_malloc(sizeof(uint8_t) * size), true);
	
	if (!pImage->GetPixels())
	{
		TinyKtx_DestroyContext(ctx);
		return IMAGE_LOADING_RESULT_ALLOCATION_FAILED;
	}
    
    for (uint mipMapLevel = 0; mipMapLevel < pImage->GetMipMapCount(); mipMapLevel++)
    {
		uint blocksPerRow = (w + TinyImageFormat_WidthOfBlock(fmt) - 1) / TinyImageFormat_WidthOfBlock(fmt);
		uint rowCount = pImage->GetRowCount(mipMapLevel);
		uint8_t const* src = (uint8_t const*) TinyKtx_ImageRawData(ctx, mipMapLevel);

		uint32_t srcStride = TinyKtx_UnpackedRowStride(ctx, mipMapLevel);
		if (!TinyKtx_IsMipMapLevelUnpacked(ctx, mipMapLevel))
			srcStride = (TinyImageFormat_BitSizeOfBlock(fmt) / 8) * blocksPerRow;
		
		uint32_t const dstStride = pImage->GetBytesPerRow(mipMapLevel);
		ASSERT(srcStride <= dstStride);

		for (uint32_t ww = 0u; ww < s; ++ww) {
			uint8_t *dst = pImage->GetPixels(mipMapLevel, ww);
			for (uint32_t zz = 0; zz < d; ++zz) {
				for (uint32_t yy = 0; yy < rowCount; ++yy) {
					memcpy(dst, src, srcStride);
					src += srcStride;
					dst += dstStride;
				}
			}
		}
		w = max(w >> 1u, 1u);
		h = max(h >> 1u, 1u);
		d = max(d >> 1u, 1u);
	}

	TinyKtx_DestroyContext(ctx);
	return IMAGE_LOADING_RESULT_SUCCESS;
}
#endif

#if defined(ORBIS)
ImageLoadingResult iLoadGNFFromStream(Image* pImage, FileStream* pStream, memoryAllocationFunc pAllocator /*= NULL*/, void* pUserData /*= NULL*/);
#endif

//------------------------------------------------------------------------------
#ifndef IMAGE_DISABLE_GOOGLE_BASIS
//  Loads a Basis data from memory.
//
ImageLoadingResult iLoadBASISFromStream(Image* pImage, FileStream* pStream, memoryAllocationFunc pAllocator /*= NULL*/, void* pUserData /*= NULL*/)
{
	if (pStream == NULL || fsGetStreamFileSize(pStream) <= 0)
		return IMAGE_LOADING_RESULT_DECODING_FAILED;

	basist::etc1_global_selector_codebook sel_codebook(basist::g_global_selector_cb_size, basist::g_global_selector_cb);

	void* basisData = fsGetStreamBufferIfPresent(pStream);
	size_t memSize = (size_t)fsGetStreamFileSize(pStream);

	if (!basisData)
	{
		basisData = conf_malloc(memSize);
		fsReadFromStream(pStream, basisData, memSize);
	}

	basist::basisu_transcoder decoder(&sel_codebook);

	basist::basisu_file_info fileinfo;
	if (!decoder.get_file_info(basisData, (uint32_t)memSize, fileinfo))
	{
		LOGF(LogLevel::eERROR, "Failed retrieving Basis file information!");
		return IMAGE_LOADING_RESULT_DECODING_FAILED;
	}

	ASSERT(fileinfo.m_total_images == fileinfo.m_image_mipmap_levels.size());
	ASSERT(fileinfo.m_total_images == decoder.get_total_images(basisData, (uint32_t)memSize));

	basist::basisu_image_info imageinfo;
	decoder.get_image_info(basisData, (uint32_t)memSize, imageinfo, 0);

	uint32_t width = imageinfo.m_width;
	uint32_t height = imageinfo.m_height;
	uint32_t depth = 1;
	uint32_t mipMapCount = max(1U, fileinfo.m_image_mipmap_levels[0]);
	uint32_t arrayCount = fileinfo.m_total_images;

	TinyImageFormat imageFormat = TinyImageFormat_UNDEFINED;

	bool isNormalMap;

	if (fileinfo.m_userdata0 == 1)
		isNormalMap = true;
	else
		isNormalMap = false;

	basist::transcoder_texture_format basisTextureFormat;

#ifdef TARGET_IOS
	if (!isNormalMap)
	{
		if (!imageinfo.m_alpha_flag)
		{
			imageFormat = TinyImageFormat_ASTC_4x4_UNORM;
			basisTextureFormat = basist::transcoder_texture_format::cTFASTC_4x4_RGBA;
		}
		else
		{
			imageFormat = TinyImageFormat_ASTC_4x4_UNORM;
			basisTextureFormat = basist::transcoder_texture_format::cTFASTC_4x4;
		}
	}
	else
	{
        imageFormat = TinyImageFormat_ASTC_4x4_UNORM;
        basisTextureFormat = basist::transcoder_texture_format::cTFASTC_4x4;
	}
#else
	if (!isNormalMap)
	{
		if (!imageinfo.m_alpha_flag)
		{
			imageFormat = TinyImageFormat_DXBC1_RGBA_UNORM;
			basisTextureFormat = basist::transcoder_texture_format::cTFBC1;
		}
		else
		{
			imageFormat = TinyImageFormat_DXBC3_UNORM;
			basisTextureFormat = basist::transcoder_texture_format::cTFBC3;
		}
	}
	else
	{
        imageFormat = TinyImageFormat_DXBC5_UNORM;
        basisTextureFormat = basist::transcoder_texture_format::cTFBC5;
	}
#endif

	pImage->RedefineDimensions(imageFormat, width, height, depth, mipMapCount, arrayCount);

	size_t size = pImage->GetSizeInBytes();

	if (pAllocator)
		pImage->SetPixels((unsigned char*)pAllocator(pImage, size, pImage->GetSubtextureAlignment(), pUserData));
	else
		pImage->SetPixels((unsigned char*)conf_malloc(sizeof(unsigned char) * size), true);
	
	if (!pImage->GetPixels())
		return IMAGE_LOADING_RESULT_ALLOCATION_FAILED;

	decoder.start_transcoding(basisData, (uint32_t)memSize);
	
	for (uint32_t image_index = 0; image_index < fileinfo.m_total_images; image_index++)
	{
		for (uint32_t level_index = 0; level_index < fileinfo.m_image_mipmap_levels[image_index]; level_index++)
		{
			uint rowPitchInBlocks = pImage->GetBytesPerRow(level_index) / (TinyImageFormat_BitSizeOfBlock(pImage->GetFormat()) / 8);
			ASSERT(pImage->GetBytesPerRow(level_index) % (TinyImageFormat_BitSizeOfBlock(pImage->GetFormat()) / 8) == 0);
			
			basist::basisu_image_level_info level_info;

			if (!decoder.get_image_level_info(basisData, (uint32_t)memSize, level_info, image_index, level_index))
			{
				LOGF(LogLevel::eERROR, "Failed retrieving image level information (%u %u)!\n", image_index, level_index);
				if (!fsGetStreamBufferIfPresent(pStream)) { conf_free(basisData); }
				return IMAGE_LOADING_RESULT_DECODING_FAILED;
			}

			if (basisTextureFormat == basist::transcoder_texture_format::cTFPVRTC1_4_RGB)
			{
				if (!isPowerOf2(level_info.m_width) || !isPowerOf2(level_info.m_height))
				{
					LOGF(LogLevel::eWARNING, "Warning: Will not transcode image %u level %u res %ux%u to PVRTC1 (one or more dimension is not a power of 2)\n", image_index, level_index, level_info.m_width, level_info.m_height);

					// Can't transcode this image level to PVRTC because it's not a pow2 (we're going to support transcoding non-pow2 to the next larger pow2 soon)
					continue;
				}
			}

			if (!decoder.transcode_image_level(basisData, (uint32_t)memSize, image_index, level_index, pImage->GetPixels(level_index, image_index), (uint32_t)(rowPitchInBlocks * imageinfo.m_num_blocks_y), basisTextureFormat, 0, rowPitchInBlocks))
			{
				LOGF(LogLevel::eERROR, "Failed transcoding image level (%u %u)!\n", image_index, level_index);
				
				if (!fsGetStreamBufferIfPresent(pStream)) { conf_free(basisData); }
				return IMAGE_LOADING_RESULT_DECODING_FAILED;
			}
		}
	}

	if (!fsGetStreamBufferIfPresent(pStream)) { conf_free(basisData); }

	return IMAGE_LOADING_RESULT_SUCCESS;
}
#endif


ImageLoadingResult iLoadSVTFromStream(Image* pImage, FileStream* pStream, memoryAllocationFunc pAllocator /*= NULL*/, void* pUserData /*= NULL*/)
{
	if (pStream == NULL || fsGetStreamFileSize(pStream) == 0)
		return IMAGE_LOADING_RESULT_DECODING_FAILED;

	uint width = fsReadFromStreamUInt32(pStream);
	uint height = fsReadFromStreamUInt32(pStream);
	uint mipMapCount = fsReadFromStreamUInt32(pStream);
	/* uint pageSize = */ fsReadFromStreamUInt32(pStream);
	uint numberOfComponents = fsReadFromStreamUInt32(pStream);

	if(numberOfComponents == 4)
		pImage->RedefineDimensions(TinyImageFormat_R8G8B8A8_UNORM, width, height, 1, mipMapCount, 1);
	else
		pImage->RedefineDimensions(TinyImageFormat_R8G8B8A8_UNORM, width, height, 1, mipMapCount, 1);

	size_t size = pImage->GetSizeInBytes();
	
	if (pAllocator)
		pImage->SetPixels((unsigned char*)pAllocator(pImage, size, pImage->GetSubtextureAlignment(), pUserData));
	else
		pImage->SetPixels((unsigned char*)conf_malloc(size), true);
	
	if (!pImage->GetPixels())
		return IMAGE_LOADING_RESULT_ALLOCATION_FAILED;

	fsReadFromStream(pStream, pImage->GetPixels(), size);

	return IMAGE_LOADING_RESULT_SUCCESS;
}

// Image loading
// struct of table for file format to loading function
struct ImageLoaderDefinition
{
	char const* mExtension;
	Image::ImageLoaderFunction pLoader;
};

#define MAX_IMAGE_LOADERS 12
static ImageLoaderDefinition gImageLoaders[MAX_IMAGE_LOADERS];
uint32_t gImageLoaderCount = 0;

// One time call to initialize all loaders
void Image::Init()
{
#ifndef IMAGE_DISABLE_STB
	gImageLoaders[gImageLoaderCount++] = {"png", iLoadSTBFromStream};
	gImageLoaders[gImageLoaderCount++] = {"jpg", iLoadSTBFromStream};
#endif
	gImageLoaders[gImageLoaderCount++] = {"dds", iLoadDDSFromStream};
	gImageLoaders[gImageLoaderCount++] = {"pvr", iLoadPVRFromStream};
#ifndef IMAGE_DISABLE_KTX
	gImageLoaders[gImageLoaderCount++] = {"ktx", iLoadKTXFromStream};
#endif
#if defined(ORBIS)
	gImageLoaders[gImageLoaderCount++] = {"gnf", iLoadGNFFromStream};
#endif
#ifndef IMAGE_DISABLE_GOOGLE_BASIS
	gImageLoaders[gImageLoaderCount++] = {"basis", iLoadBASISFromStream};
#endif
	gImageLoaders[gImageLoaderCount++] = {"svt", iLoadSVTFromStream};
}

void Image::Exit()
{
}

void Image::AddImageLoader(const char* pExtension, ImageLoaderFunction pFunc) {
	gImageLoaders[gImageLoaderCount++] = { pExtension, pFunc };
}

ImageLoadingResult Image::LoadFromStream(
	FileStream* pStream, char const* extension, memoryAllocationFunc pAllocator, void* pUserData, uint rowAlignment, uint subtextureAlignment)
{
	mRowAlignment = rowAlignment;
	mSubtextureAlignment = subtextureAlignment;
	
	// try loading the format
	ImageLoadingResult result = IMAGE_LOADING_RESULT_DECODING_FAILED;
	for (uint32_t i = 0; i < gImageLoaderCount; ++i)
	{
		ImageLoaderDefinition const& def = gImageLoaders[i];
		if (extension && stricmp(extension, def.mExtension) == 0)
		{
			result = def.pLoader(this, pStream, pAllocator, pUserData);
			if (result == IMAGE_LOADING_RESULT_SUCCESS || result == IMAGE_LOADING_RESULT_ALLOCATION_FAILED)
			{
				return result;
			}
			
		}
	}
	return result;
}

ImageLoadingResult Image::LoadFromFile(const Path* filePath, memoryAllocationFunc pAllocator, void* pUserData, uint rowAlignment, uint subtextureAlignment)
{
	// clear current image
	Clear();
	mRowAlignment = rowAlignment;
	mSubtextureAlignment = subtextureAlignment;

	PathComponent extensionComponent = fsGetPathExtension(filePath);
	uint32_t loaderIndex = -1;

	if (extensionComponent.length != 0) // the string's not empty
	{
		for (int i = 0; i < (int)gImageLoaderCount; i++)
		{
			if (stricmp(extensionComponent.buffer, gImageLoaders[i].mExtension) == 0)
			{
				loaderIndex = i;
				break;
			}
		}

		if (loaderIndex == -1)
		{
			extensionComponent.buffer = NULL;
			extensionComponent.length = 0;
		}
	}

    const char *extension;
    
    PathHandle loadFilePath = NULL;
	// For loading basis file, it should have its extension
	if (extensionComponent.length == 0)
	{
#if defined(__ANDROID__)
		extension = "ktx";
#elif defined(TARGET_IOS)
		extension = "ktx";
#elif defined(__linux__)
		extension = "dds";
#elif defined(__APPLE__)
		extension = "dds";
#elif defined(NX64)
		extension = "ktx";
#elif defined(ORBIS)
		extension = "gnf";
#else
		extension = "dds";
#endif

        loadFilePath = fsAppendPathExtension(filePath, extension);
    } else {
        extension = extensionComponent.buffer;
        loadFilePath = fsCopyPath(filePath);
    }
		
    FileStream* fh = fsOpenFile(loadFilePath, FM_READ_BINARY);
	FileStream* mem = NULL;
	void* zipBuffer = NULL;
	if (FSK_ZIP == fsGetFileSystemKind(fsGetPathFileSystem(loadFilePath)))
	{
		ssize_t size = fsGetStreamFileSize(fh);
		zipBuffer = conf_malloc(size);
		mem = fsOpenReadOnlyMemory(zipBuffer, size);
		fsReadFromStream(fh, zipBuffer, size);
		fsCloseStream(fh);
		fh = mem;
	}
	
	if (!fh)
	{
		LOGF(LogLevel::eERROR, "\"%s\": Image file not found.", fsGetPathAsNativeString(loadFilePath));
		return IMAGE_LOADING_RESULT_DECODING_FAILED;
	}
	
	// load file into memory
	
    ssize_t length = fsGetStreamFileSize(fh);
	if (length <= 0)
	{
		LOGF(LogLevel::eERROR, "\"%s\": Image is an empty file.", fsGetPathAsNativeString(loadFilePath));
        fsCloseStream(fh);
		return IMAGE_LOADING_RESULT_DECODING_FAILED;
	}
	
	mLoadFilePath = loadFilePath;

	// try loading the format
	ImageLoadingResult result = IMAGE_LOADING_RESULT_DECODING_FAILED;
	bool support = false;
	for (int i = 0; i < (int)gImageLoaderCount; i++)
	{
		if (stricmp(extension, gImageLoaders[i].mExtension) == 0)
		{
			support = true;
			result = gImageLoaders[i].pLoader(this, fh, pAllocator, pUserData);
			if (result == IMAGE_LOADING_RESULT_SUCCESS || result == IMAGE_LOADING_RESULT_ALLOCATION_FAILED)
			{
				fsCloseStream(fh);
				if (zipBuffer)
					conf_free(zipBuffer);
				return result;
			}
			fsSeekStream(fh, SBO_START_OF_FILE, 0);
		}
	}
	if (!support)
	{
		LOGF(LogLevel::eERROR, "Can't load this file format for image:  %s", fsGetPathAsNativeString(loadFilePath));
	}

	fsCloseStream(fh);
	if (zipBuffer)
		conf_free(zipBuffer);

	return result;
}

bool Image::Convert(const TinyImageFormat newFormat)
{
	// TODO add RGBE8 to tiny image format
	if(!TinyImageFormat_CanDecodeLogicalPixelsF(mFormat)) return false;
	if(!TinyImageFormat_CanEncodeLogicalPixelsF(newFormat)) return false;

	int pixelCount = GetNumberOfPixels(0, mMipMapCount);

	ubyte* newPixels;
	newPixels = (unsigned char*)conf_malloc(GetSizeInBytes());

	TinyImageFormat_DecodeInput input{};
	input.pixel = pData;
	TinyImageFormat_EncodeOutput output{};
	output.pixel = newPixels;

	float* tmp = (float*)conf_malloc(sizeof(float) * 4 * pixelCount);

	TinyImageFormat_DecodeLogicalPixelsF(mFormat, &input, pixelCount, tmp);
	TinyImageFormat_EncodeLogicalPixelsF(newFormat, tmp, pixelCount, &output);

	conf_free(tmp);

	conf_free(pData);
	pData = newPixels;
	mFormat = newFormat;

	return true;
}

bool Image::GenerateMipMaps(const uint32_t mipMaps)
{
	if (TinyImageFormat_IsCompressed(mFormat))
		return false;
	if (!(mWidth) || !isPowerOf2(mHeight) || !isPowerOf2(mDepth))
		return false;
	if (!mOwnsMemory)
		return false;

	uint actualMipMaps = min(mipMaps, GetMipMapCountFromDimensions());

	if (mMipMapCount != actualMipMaps)
	{
		uint size = GetMipMappedSize(0, actualMipMaps);
		if (mArrayCount > 1)
		{
			ubyte* newPixels = (ubyte*)conf_malloc(sizeof(ubyte) * size * mArrayCount);

			// Copy top mipmap of all array slices to new location
			int firstMipSize = GetMipMappedSize(0, 1);
			int oldSize = GetMipMappedSize(0, mMipMapCount);

			for (uint i = 0; i < mArrayCount; i++)
			{
				memcpy(newPixels + i * size, pData + i * oldSize, firstMipSize);
			}

			conf_free(pData);
			pData = newPixels;
		}
		else
		{
			pData = (ubyte*)conf_realloc(pData, size);
		}
		mMipMapCount = actualMipMaps;
	}

	int nChannels = TinyImageFormat_ChannelCount(mFormat);

	int n = IsCube() ? 6 : 1;

	for (uint arraySlice = 0; arraySlice < mArrayCount; arraySlice++)
	{
		for (uint level = 1; level < mMipMapCount; level++)
		{
			// TODO: downscale 3D textures using an appropriate filter.
			
			uint srcWidth = GetWidth(level - 1);
			uint srcHeight = GetHeight(level - 1);
			uint dstWidth = GetWidth(level - 1);
			uint dstHeight = GetHeight(level - 1);

			uint srcSize = GetMipMappedSize(level - 1, 1) / n;
			uint dstSize = GetMipMappedSize(level, 1) / n;

			uint srcStride = GetBytesPerRow(level - 1);
			uint dstStride = GetBytesPerRow(level);
			
			for (int i = 0; i < n; i++)
			{
				unsigned char* srcPixels = GetPixels(level - 1, arraySlice) + n * srcSize;
				unsigned char* dstPixels = GetPixels(level, arraySlice) + n * dstSize;

				int8_t physicalChannel = TinyImageFormat_LogicalChannelToPhysical(mFormat, TinyImageFormat_LC_Alpha);
				int alphaChannel = physicalChannel;
				if (physicalChannel == TinyImageFormat_PC_CONST_0 || physicalChannel == TinyImageFormat_PC_CONST_1)
					alphaChannel = STBIR_ALPHA_CHANNEL_NONE;
				
				// only homogoenous is supported via this method
				// TODO use decode/encode for the others
				// TODO check these methods work for SNORM
				if(TinyImageFormat_IsHomogenous(mFormat)) {
					uint32 redChanWidth = TinyImageFormat_ChannelBitWidth(mFormat, TinyImageFormat_LC_Red);
					if (redChanWidth == 32 && TinyImageFormat_IsFloat(mFormat))
					{
						stbir_resize_float((float*)srcPixels, srcWidth, srcHeight, srcStride,
										   (float*)dstPixels, dstWidth, dstHeight, dstStride,
										   nChannels);
					}
					else if (redChanWidth == 16)
					{
						stbir_colorspace colorSpace = TinyImageFormat_IsSRGB(mFormat) ? STBIR_COLORSPACE_SRGB : STBIR_COLORSPACE_LINEAR;
						stbir_resize_uint16_generic((uint16_t*)srcPixels, srcWidth, srcHeight, srcStride,
													(uint16_t*)dstPixels, dstWidth, dstHeight, dstStride,
													nChannels, alphaChannel, 0,
													STBIR_EDGE_WRAP, STBIR_FILTER_DEFAULT, colorSpace, NULL);
					}
					else if (redChanWidth == 8)
					{
						
						if (TinyImageFormat_IsSRGB(mFormat))
							stbir_resize_uint8_srgb(srcPixels, srcWidth, srcHeight, srcStride,
													dstPixels, dstWidth, dstHeight, dstStride,
													nChannels, alphaChannel, 0);
						else
							stbir_resize_uint8(srcPixels, srcWidth, srcHeight, srcStride,
											   dstPixels, dstWidth, dstHeight, dstStride,
											   nChannels);
					}
					// TODO: fall back to to be written generic downsizer
				}
			}
		}
	}

	return true;
}

// -- IMAGE SAVING --

bool Image::iSaveDDS(const Path* filePath) {
	TinyDDS_WriteCallbacks callback{
			&tinyktxddsCallbackError,
			&tinyktxddsCallbackAlloc,
			&tinyktxddsCallbackFree,
			&tinyktxddsCallbackWrite,
	};

	TinyDDS_Format fmt = TinyImageFormat_ToTinyDDSFormat(mFormat);
#ifndef IMAGE_DISABLE_KTX
	if (fmt == TDDS_UNDEFINED)
		return convertAndSaveImage(*this, &Image::iSaveKTX, filePath);
#endif

    FileStream* fh = fsOpenFile(filePath, FM_WRITE_BINARY);

	if (!fh)
		return false;

	uint32_t mipmapsizes[TINYDDS_MAX_MIPMAPLEVELS];
	void const* mipmaps[TINYDDS_MAX_MIPMAPLEVELS];
	memset(mipmapsizes, 0, sizeof(uint32_t) * TINYDDS_MAX_MIPMAPLEVELS);
	memset(mipmaps, 0, sizeof(void const*) * TINYDDS_MAX_MIPMAPLEVELS);

	for (unsigned int i = 0; i < mMipMapCount; ++i) {
		mipmapsizes[i] = (uint32_t)Image_GetMipMappedSize(mWidth, mHeight, mDepth, mMipMapCount, mFormat);
		mipmaps[i] = GetPixels(i);
	}

	bool result = TinyDDS_WriteImage(&callback,
		fh,
		mWidth,
		mHeight,
		mDepth,
		mArrayCount,
		mMipMapCount,
		fmt,
		mDepth == 0,
		true,
		mipmapsizes,
		mipmaps);
    
    fsCloseStream(fh);
    return result;
}

#ifndef IMAGE_DISABLE_KTX
bool Image::iSaveKTX(const Path* filePath) {
	TinyKtx_WriteCallbacks callback{
			&tinyktxddsCallbackError,
			&tinyktxddsCallbackAlloc,
			&tinyktxddsCallbackFree,
			&tinyktxddsCallbackWrite,
	};
	
	if (getBytesPerRow(mWidth, mFormat, mRowAlignment) != getBytesPerRow(mWidth, mFormat, 1))
	{
		LOGF(LogLevel::eWARNING, "Saving KTX images with a padded row stride is unimplemented.");
		return false; // TODO: handle padded rows.
	}

	TinyKtx_Format fmt = TinyImageFormat_ToTinyKtxFormat(mFormat);
	if (fmt == TKTX_UNDEFINED)
		return convertAndSaveImage(*this, &Image::iSaveKTX, filePath);
    
    FileStream* fh = fsOpenFile(filePath, FM_WRITE_BINARY);
    
	if (!fh)
		return false;
	
	uint32_t mipmapsizes[TINYKTX_MAX_MIPMAPLEVELS];
	void const* mipmaps[TINYKTX_MAX_MIPMAPLEVELS];
	memset(mipmapsizes, 0, sizeof(uint32_t) * TINYKTX_MAX_MIPMAPLEVELS);
	memset(mipmaps, 0, sizeof(void const*) * TINYKTX_MAX_MIPMAPLEVELS);

	for (unsigned int i = 0; i < mMipMapCount; ++i) {
		mipmapsizes[i] = (uint32_t)Image_GetMipMappedSize(mWidth, mHeight, mDepth, mMipMapCount, mFormat);
		mipmaps[i] = GetPixels(i);
	}

	bool result = TinyKtx_WriteImage(&callback,
		fh,
		mWidth,
		mHeight,
		mDepth,
		mArrayCount,
		mMipMapCount,
		fmt,
		mDepth == 0,
		mipmapsizes,
		mipmaps);
    
    fsCloseStream(fh);
    return result;
}
#endif

bool convertAndSaveImage(const Image& image, bool (Image::*saverFunction)(const Path*), const Path* filePath)
{
	bool  bSaveImageSuccess = false;
	Image imgCopy(image);
	imgCopy.Uncompress();
	if (imgCopy.Convert(TinyImageFormat_R8G8B8A8_UNORM))
	{
		bSaveImageSuccess = (imgCopy.*saverFunction)(filePath);
	}

	imgCopy.Destroy();
	return bSaveImageSuccess;
}

#ifndef IMAGE_DISABLE_STB
bool Image::iSaveTGA(const Path* filePath)
{
    // TODO: should use stbi_write_x_to_func methods rather than relying on the path being a disk path.
	if (getBytesPerRow(mWidth, mFormat, mRowAlignment) != getBytesPerRow(mWidth, mFormat, 1))
	{
		LOGF(LogLevel::eWARNING, "Saving TGA images with a padded row stride is unimplemented.");
		return false; // TODO: handle padded rows.
	}
	
    const char *fileName = fsGetPathAsNativeString(filePath);
	switch (mFormat)
	{
		case TinyImageFormat_R8_UNORM: return 0 != stbi_write_tga(fileName, mWidth, mHeight, 1, pData); break;
		case TinyImageFormat_R8G8_UNORM: return 0 != stbi_write_tga(fileName, mWidth, mHeight, 2, pData); break;
		case TinyImageFormat_R8G8B8_UNORM: return 0 != stbi_write_tga(fileName, mWidth, mHeight, 3, pData); break;
		case TinyImageFormat_R8G8B8A8_UNORM: return 0 != stbi_write_tga(fileName, mWidth, mHeight, 4, pData); break;
		default:
		{
			// uncompress/convert and try again
			return convertAndSaveImage(*this, &Image::iSaveTGA, filePath);
		}
	}

	//return false; //Unreachable
}

bool Image::iSaveBMP(const Path* filePath)
{
    // TODO: should use stbi_write_x_to_func methods rather than relying on the path being a disk path.

	if (getBytesPerRow(mWidth, mFormat, mRowAlignment) != getBytesPerRow(mWidth, mFormat, 1))
	{
		LOGF(LogLevel::eWARNING, "Saving BMP images with a padded row stride is unimplemented.");
		return false; // TODO: handle padded rows.
	}
	
    const char *fileName = fsGetPathAsNativeString(filePath);
	switch (mFormat)
	{
		case TinyImageFormat_R8_UNORM: stbi_write_bmp(fileName, mWidth, mHeight, 1, pData); break;
		case TinyImageFormat_R8G8_UNORM: stbi_write_bmp(fileName, mWidth, mHeight, 2, pData); break;
		case TinyImageFormat_R8G8B8_UNORM: stbi_write_bmp(fileName, mWidth, mHeight, 3, pData); break;
		case TinyImageFormat_R8G8B8A8_UNORM: stbi_write_bmp(fileName, mWidth, mHeight, 4, pData); break;
		default:
		{
			// uncompress/convert and try again
			return convertAndSaveImage(*this, &Image::iSaveBMP, filePath);
		}
	}
	return true;
}

bool Image::iSavePNG(const Path* filePath)
{
    // TODO: should use stbi_write_x_to_func methods rather than relying on the path being a disk path.
    const char *fileName = fsGetPathAsNativeString(filePath);
	switch (mFormat)
	{
		case TinyImageFormat_R8_UNORM: stbi_write_png(fileName, mWidth, mHeight, 1, pData, GetBytesPerRow()); break;
		case TinyImageFormat_R8G8_UNORM: stbi_write_png(fileName, mWidth, mHeight, 2, pData, GetBytesPerRow()); break;
		case TinyImageFormat_R8G8B8_UNORM: stbi_write_png(fileName, mWidth, mHeight, 3, pData, GetBytesPerRow()); break;
		case TinyImageFormat_R8G8B8A8_UNORM: stbi_write_png(fileName, mWidth, mHeight, 4, pData, GetBytesPerRow()); break;
		default:
		{
			// uncompress/convert and try again
			return convertAndSaveImage(*this, &Image::iSavePNG, filePath);
		}
	}

	return true;
}

bool Image::iSaveHDR(const Path* filePath)
{
    // TODO: should use stbi_write_x_to_func methods rather than relying on the path being a disk path.

	if (getBytesPerRow(mWidth, mFormat, mRowAlignment) != getBytesPerRow(mWidth, mFormat, 1))
	{
		LOGF(LogLevel::eWARNING, "Saving HDR images with a padded row stride is unimplemented.");
		return false; // TODO: handle padded rows.
	}
	
    const char *fileName = fsGetPathAsNativeString(filePath);
	switch (mFormat)
	{
		case TinyImageFormat_R32_SFLOAT: stbi_write_hdr(fileName, mWidth, mHeight, 1, (float*)pData); break;
		case TinyImageFormat_R32G32_SFLOAT: stbi_write_hdr(fileName, mWidth, mHeight, 2, (float*)pData); break;
		case TinyImageFormat_R32G32B32_SFLOAT: stbi_write_hdr(fileName, mWidth, mHeight, 3, (float*)pData); break;
		case TinyImageFormat_R32G32B32A32_SFLOAT: stbi_write_hdr(fileName, mWidth, mHeight, 4, (float*)pData); break;
		default:
		{
			// uncompress/convert and try again
			return convertAndSaveImage(*this, &Image::iSaveHDR, filePath);
		}
	}

	return true;
}

bool Image::iSaveJPG(const Path* filePath)
{
    // TODO: should use stbi_write_x_to_func methods rather than relying on the path being a disk path.

	if (getBytesPerRow(mWidth, mFormat, mRowAlignment) != getBytesPerRow(mWidth, mFormat, 1))
	{
		LOGF(LogLevel::eWARNING, "Saving JPEG images with a padded row stride is unimplemented.");
		return false; // TODO: handle padded rows.
	}
	
    const char *fileName = fsGetPathAsNativeString(filePath);
	switch (mFormat)
	{
		case TinyImageFormat_R8_UNORM: stbi_write_jpg(fileName, mWidth, mHeight, 1, pData, 0); break;
		case TinyImageFormat_R8G8_UNORM: stbi_write_jpg(fileName, mWidth, mHeight, 2, pData, 0); break;
		case TinyImageFormat_R8G8B8_UNORM: stbi_write_jpg(fileName, mWidth, mHeight, 3, pData, 0); break;
		case TinyImageFormat_R8G8B8A8_UNORM: stbi_write_jpg(fileName, mWidth, mHeight, 4, pData, 0); break;
		default:
		{
			// uncompress/convert and try again
			return convertAndSaveImage(*this, &Image::iSaveJPG, filePath);
		}
	}

	return true;
}
#endif

bool Image::iSaveSVT(const Path* filePath, uint pageSize)
{
	if (mFormat != TinyImageFormat::TinyImageFormat_R8G8B8A8_UNORM)
	{
		// uncompress/convert
		if (!Convert(TinyImageFormat::TinyImageFormat_R8G8B8A8_UNORM))
			return false;
	}

	if (mMipMapCount == 1)
	{
		if (!GenerateMipMaps((uint)log2f((float)min(mWidth, mHeight))))
			return false;
	}

	FileStream* fh = fsOpenFile(filePath, FM_WRITE_BINARY);

	if (!fh)
		return false;

	//TODO: SVT should support any components somepoint
	const uint numberOfComponents = 4;

	//Header

	fsWriteToStream(fh, &mWidth, sizeof(uint));
	fsWriteToStream(fh, &mHeight, sizeof(uint));

	//mip count
	fsWriteToStream(fh, &mMipMapCount, sizeof(uint));

	//Page size
	fsWriteToStream(fh, &pageSize, sizeof(uint));

	//number of components
	fsWriteToStream(fh, &numberOfComponents, sizeof(uint));

	uint mipPageCount = mMipMapCount - (uint)log2f((float)pageSize);

	// Allocate Pages
	unsigned char** mipLevelPixels = (unsigned char**)conf_calloc(mMipMapCount, sizeof(unsigned char*));
	unsigned char*** pagePixels = (unsigned char***)conf_calloc(mipPageCount + 1, sizeof(unsigned char**));

	uint* mipSizes = (uint*)conf_calloc(mMipMapCount, sizeof(uint));

	for (uint i = 0; i < mMipMapCount; ++i)
	{
		uint mipSize = (mWidth >> i) * (mHeight >> i) * numberOfComponents;
		mipSizes[i] = mipSize;
		mipLevelPixels[i] = (unsigned char*)conf_calloc(mipSize, sizeof(unsigned char));
		memcpy(mipLevelPixels[i], GetPixels(i), mipSize * sizeof(unsigned char));
	}

	// Store Mip data
	for (uint i = 0; i < mipPageCount; ++i)
	{
		uint xOffset = mWidth >> i;
		uint yOffset = mHeight >> i;

		// width and height in tiles
		uint tileWidth = xOffset / pageSize;
		uint tileHeight = yOffset / pageSize;

		uint xMipOffset = 0;
		uint yMipOffset = 0;
		uint pageIndex = 0;

		uint rowLength = xOffset * numberOfComponents;

		pagePixels[i] = (unsigned char**)conf_calloc(tileWidth * tileHeight, sizeof(unsigned char*));

		for (uint j = 0; j < tileHeight; ++j)
		{
			for (uint k = 0; k < tileWidth; ++k)
			{
				pagePixels[i][pageIndex] = (unsigned char*)conf_calloc(pageSize * pageSize, sizeof(unsigned char) * numberOfComponents);

				for (uint y = 0; y < pageSize; ++y)
				{
					for (uint x = 0; x < pageSize; ++x)
					{
						uint mipPageIndex = (y * pageSize + x) * numberOfComponents;
						uint mipIndex = rowLength * (y + yMipOffset) + (numberOfComponents)*(x + xMipOffset);

						pagePixels[i][pageIndex][mipPageIndex + 0] = mipLevelPixels[i][mipIndex + 0];
						pagePixels[i][pageIndex][mipPageIndex + 1] = mipLevelPixels[i][mipIndex + 1];
						pagePixels[i][pageIndex][mipPageIndex + 2] = mipLevelPixels[i][mipIndex + 2];
						pagePixels[i][pageIndex][mipPageIndex + 3] = mipLevelPixels[i][mipIndex + 3];
					}
				}

				xMipOffset += pageSize;
				pageIndex += 1;
			}

			xMipOffset = 0;
			yMipOffset += pageSize;
		}
	}

	uint mipTailPageSize = 0;

	pagePixels[mipPageCount] = (unsigned char**)conf_calloc(1, sizeof(unsigned char*));

	// Calculate mip tail size
	for (uint i = mipPageCount; i < mMipMapCount - 1; ++i)
	{
		uint mipSize = mipSizes[i];
		mipTailPageSize += mipSize;
	}

	pagePixels[mipPageCount][0] = (unsigned char*)conf_calloc(mipTailPageSize, sizeof(unsigned char));

	// Store mip tail data
	uint mipTailPageWrites = 0;
	for (uint i = mipPageCount; i < mMipMapCount - 1; ++i)
	{
		uint mipSize = mipSizes[i];

		for (uint j = 0; j < mipSize; ++j)
		{
			pagePixels[mipPageCount][0][mipTailPageWrites++] = mipLevelPixels[i][j];
		}
	}

	// Write mip data
	for (uint i = 0; i < mipPageCount; ++i)
	{
		// width and height in tiles
		uint mipWidth = (mWidth >> i) / pageSize;
		uint mipHeight = (mHeight >> i) / pageSize;

		for (uint j = 0; j < mipWidth * mipHeight; ++j)
		{
			fsWriteToStream(fh, pagePixels[i][j], pageSize * pageSize * numberOfComponents * sizeof(char));
		}
	}

	// Write mip tail data
	fsWriteToStream(fh, pagePixels[mipPageCount][0], mipTailPageSize * sizeof(char));

	// free memory
	conf_free(mipSizes);

	for (uint i = 0; i < mipPageCount; ++i)
	{
		// width and height in tiles
		uint mipWidth = (mWidth >> i) / pageSize;
		uint mipHeight = (mHeight >> i) / pageSize;
		uint pageIndex = 0;

		for (uint j = 0; j < mipHeight; ++j)
		{
			for (uint k = 0; k < mipWidth; ++k)
			{
				conf_free(pagePixels[i][pageIndex]);
				pageIndex += 1;
			}
		}

		conf_free(pagePixels[i]);
	}
	conf_free(pagePixels[mipPageCount][0]);
	conf_free(pagePixels[mipPageCount]);
	conf_free(pagePixels);

	for (uint i = 0; i < mMipMapCount; ++i)
	{
		conf_free(mipLevelPixels[i]);
	}
	conf_free(mipLevelPixels);

	fsCloseStream(fh);

	return true;
}

struct ImageSaverDefinition
{
	typedef bool (Image::*ImageSaverFunction)(const Path* filePath);
	const char*        Extension;
	ImageSaverFunction Loader;
};

static ImageSaverDefinition gImageSavers[] =
{
#ifndef IMAGE_DISABLE_STB
	{ ".bmp", &Image::iSaveBMP },
	{ ".hdr", &Image::iSaveHDR },
	{ ".png", &Image::iSavePNG },
	{ ".tga", &Image::iSaveTGA },
	{ ".jpg", &Image::iSaveJPG },
#endif
	{ ".dds", &Image::iSaveDDS },
#ifndef IMAGE_DISABLE_KTX
	{ ".ktx", &Image::iSaveKTX }
#endif
};

bool Image::Save(const Path* filePath)
{
    char extension[16];
    fsGetLowercasedPathExtension(filePath, extension, 16);
	bool        support = false;
	;
	for (int i = 0; i < sizeof(gImageSavers) / sizeof(gImageSavers[0]); i++)
	{
		if (stricmp(extension, gImageSavers[i].Extension) == 0)
		{
			support = true;
			return (this->*gImageSavers[i].Loader)(filePath);
		}
	}
	if (!support)
	{
		LOGF(LogLevel::eERROR, "Can't save this file format for image  :  %s", fsGetPathAsNativeString(filePath));
	}

	return false;
}

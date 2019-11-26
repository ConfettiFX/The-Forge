/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

void iDecodeCompressedImage(unsigned char* dest, unsigned char* src, const int width, const int height, const TinyImageFormat format)
{
	int sx = (width < 4) ? width : 4;
	int sy = (height < 4) ? height : 4;

	int nChannels = TinyImageFormat_ChannelCount(format);

	for (int y = 0; y < height; y += 4)
	{
		for (int x = 0; x < width; x += 4)
		{
			unsigned char* dst = dest + (y * width + x) * nChannels;
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
	mAdditionalDataSize = 0;
	pAdditionalData = NULL;
	mOwnsMemory = true;
	mLinearLayout = true;

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

	int size = GetMipMappedSize(0, mMipMapCount) * mArrayCount;
	pData = (unsigned char*)conf_malloc(sizeof(unsigned char) * size);
	memcpy(pData, img.pData, size);
	mLoadFilePath = fsCopyPath(img.mLoadFilePath);

	mAdditionalDataSize = img.mAdditionalDataSize;
	pAdditionalData = (unsigned char*)conf_malloc(sizeof(unsigned char) * mAdditionalDataSize);
	memcpy(pAdditionalData, img.pAdditionalData, mAdditionalDataSize);
}

unsigned char* Image::Create(const TinyImageFormat fmt, const int w, const int h, const int d, const int mipMapCount, const int arraySize)
{
	mFormat = fmt;
	mWidth = w;
	mHeight = h;
	mDepth = d;
	mMipMapCount = mipMapCount;
	mArrayCount = arraySize;
	mOwnsMemory = true;
	mMipsAfterSlices = false;

	uint holder = GetMipMappedSize(0, mMipMapCount);
	pData = (unsigned char*)conf_malloc(sizeof(unsigned char) * holder * mArrayCount);
	memset(pData, 0x00, holder * mArrayCount);
	mLoadFilePath = NULL;

	return pData;
}

unsigned char* Image::Create(const TinyImageFormat fmt, const int w, const int h, const int d, const int mipMapCount, const int arraySize, const unsigned char* rawData)
{
	mFormat = fmt;
	mWidth = w;
	mHeight = h;
	mDepth = d;
	mMipMapCount = mipMapCount;
	mArrayCount = arraySize;
	mOwnsMemory = false;
	mMipsAfterSlices = false;

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

	if (pAdditionalData)
	{
		conf_free(pAdditionalData);
		pAdditionalData = NULL;
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

	mAdditionalDataSize = 0;
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
	if (mipMapLevel >= mMipMapCount || arraySlice >= mArrayCount)
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
	uint32_t h = GetHeight(mipMapLevel);
    uint32_t d = GetDepth(mipMapLevel);
    if(d == 0) d = 1;

    if (srcFormat == TinyImageFormat_UNDEFINED)
        srcFormat = mFormat;

    uint32_t const bw = TinyImageFormat_WidthOfBlock(srcFormat);
    uint32_t const bh = TinyImageFormat_HeightOfBlock(srcFormat);
    uint32_t const bd = TinyImageFormat_DepthOfBlock(srcFormat);
    w = (w + (bw-1)) / bw;
    h = (h + (bh-1)) / bh;
    d = (d + (bd-1)) / bd;

	return (w * h * d * TinyImageFormat_BitSizeOfBlock(srcFormat)) / 8U;
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

	uint32_t nElements = GetNumberOfPixels(0, mMipMapCount) * TinyImageFormat_ChannelCount(mFormat) * mArrayCount;

	float s = 1.0f / (max - min);
	float b = -min * s;
	for (uint32_t i = 0; i < nElements; i++)
	{
		float d = ((float*)pData)[i];
		((float*)pData)[i] = d * s + b;
	}

	return true;
}

bool Image::Uncompress()
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

		for (int slice = 0; slice < d; slice++)
		{
			iDecodeCompressedImage(dst, src, w, h, mFormat);

			dst += dstSliceSize;
			src += srcSliceSize;
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

	if (srcFormat == TinyImageFormat_UNDEFINED)
		srcFormat = mFormat;
	
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
		
		return totalSize;
	}
	
	uint32_t size = 0;
	while (nMipMapLevels)
	{
		uint32_t bx = TinyImageFormat_WidthOfBlock(srcFormat);
		uint32_t by = TinyImageFormat_HeightOfBlock(srcFormat);
		uint32_t bz = TinyImageFormat_DepthOfBlock(srcFormat);
		size += ((w + bx - 1) / bx) * ((h + by - 1) / by) * ((d + bz - 1) / bz);

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

	size *= TinyImageFormat_BitSizeOfBlock(srcFormat) / 8;
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

bool iLoadDDSFromMemory(Image* pImage,
	const char* memory, uint32_t memSize, memoryAllocationFunc pAllocator, void* pUserData)
{
	if (memory == NULL || memSize == 0)
		return false;

	TinyDDS_Callbacks callbacks {
			&tinyktxddsCallbackError,
			&tinyktxddsCallbackAlloc,
			&tinyktxddsCallbackFree,
			tinyktxddsCallbackRead,
			&tinyktxddsCallbackSeek,
			&tinyktxddsCallbackTell
	};

    FileStream *fh = fsOpenReadOnlyMemory(memory, memSize);

	auto ctx = TinyDDS_CreateContext(&callbacks, (void*)fh);
	bool headerOkay = TinyDDS_ReadHeader(ctx);
	if(!headerOkay) {
        fsCloseStream(fh);
		TinyDDS_DestroyContext(ctx);
		return false;
	}

	uint32_t w = TinyDDS_Width(ctx);
	uint32_t h = TinyDDS_Height(ctx);
	uint32_t d = TinyDDS_Depth(ctx);
	uint32_t s = TinyDDS_ArraySlices(ctx);
	uint32_t mm = TinyDDS_NumberOfMipmaps(ctx);
	TinyImageFormat fmt = TinyImageFormat_FromTinyDDSFormat(TinyDDS_GetFormat(ctx));
	if(fmt == TinyImageFormat_UNDEFINED)
	{
        fsCloseStream(fh);
		TinyDDS_DestroyContext(ctx);
		return false;
	}

	// TheForge Image uses d = 0 as cubemap marker
	if(TinyDDS_IsCubemap(ctx))
		d = 0;
	else
		d = d ? d : 1;
	s = s ? s : 1;

	pImage->RedefineDimensions(fmt, w, h, d, mm, s);
	pImage->SetMipsAfterSlices(true); // tinyDDS API is mips after slices even if DDS traditionally weren't

	int size = pImage->GetMipMappedSize();

	if (pAllocator)
		pImage->SetPixels((unsigned char*)pAllocator(pImage, size, pUserData));
	else
		pImage->SetPixels((unsigned char*)conf_malloc(sizeof(unsigned char) * size), true);

	for (uint mipMapLevel = 0; mipMapLevel < pImage->GetMipMapCount(); mipMapLevel++)
	{
		size_t const expectedSize = pImage->GetMipMappedSize(mipMapLevel, 1);
		size_t const fileSize = TinyDDS_ImageSize(ctx, mipMapLevel);
		if (expectedSize != fileSize)
		{
			LOGF(LogLevel::eERROR, "DDS file %s mipmap %i size error %liu < %liu", fsGetPathAsNativeString(pImage->GetPath()), mipMapLevel, expectedSize, fileSize);
            fsCloseStream(fh);
			return false;
		}
		unsigned char *dst = pImage->GetPixels(mipMapLevel, 0);
		memcpy(dst, TinyDDS_ImageRawData(ctx, mipMapLevel), fileSize);
	}

	TinyDDS_DestroyContext(ctx);
    fsCloseStream(fh);
    
	return true;
}

bool iLoadPVRFromMemory(Image* pImage, const char* memory, uint32_t size, memoryAllocationFunc pAllocator, void* pUserData)
{
#ifndef TARGET_IOS
	LOGF(LogLevel::eERROR, "Load PVR failed: Only supported on iOS targets.");
	return false;
#else
	
	// TODO: Image
	// - no support for PVRTC2 at the moment since it isn't supported on iOS devices.
	// - only new PVR header V3 is supported at the moment.  Should we add legacy for V2 and V1?
	// - metadata is ignored for now.  Might be useful to implement it if the need for metadata arises (eg. padding, atlas coordinates, orientations, border data, etc...).
	// - flags are also ignored for now.  Currently a flag of 0x02 means that the color have been pre-multiplied byt the alpha values.
	
	// Assumptions:
	// - it's assumed that the texture is already twiddled (ie. Morton).  This should always be the case for PVRTC V3.
	
	PVR_Texture_Header* psPVRHeader = (PVR_Texture_Header*)memory;

	if (psPVRHeader->mVersion != gPvrtexV3HeaderVersion)
	{
		LOGF(LogLevel::eERROR, "Load PVR failed: Not a valid PVR V3 header.");
		return 0;
	}
	
	if (psPVRHeader->mPixelFormat > 3)
	{
		LOGF(LogLevel::eERROR, "Load PVR failed: Not a supported PVR pixel format.  Only PVRTC is supported at the moment.");
		return 0;
	}
	
	if (psPVRHeader->mNumSurfaces > 1 && psPVRHeader->mNumFaces > 1)
	{
		LOGF(LogLevel::eERROR, "Load PVR failed: Loading arrays of cubemaps isn't supported.");
		return 0;
	}

	uint32_t width = psPVRHeader->mWidth;
	uint32_t height = psPVRHeader->mHeight;
	uint32_t depth = (psPVRHeader->mNumFaces > 1) ? 0 : psPVRHeader->mDepth;
	uint32_t mipMapCount = psPVRHeader->mNumMipMaps;
	uint32_t arrayCount = psPVRHeader->mNumSurfaces;
	bool const srgb = (psPVRHeader->mColorSpace == 1);
	TinyImageFormat imageFormat = TinyImageFormat_UNDEFINED;

	switch (psPVRHeader->mPixelFormat)
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
		return false;
	}

	if (depth != 0)
		arrayCount *= psPVRHeader->mNumFaces;

	pImage->RedefineDimensions(imageFormat, width, height, depth, mipMapCount, arrayCount);


	// Extract the pixel data
	size_t totalHeaderSizeWithMetadata = sizeof(PVR_Texture_Header) + psPVRHeader->mMetaDataSize;
	size_t pixelDataSize = pImage->GetMipMappedSize();

	if (pAllocator)
	{
		pImage->SetPixels((unsigned char*)pAllocator(pImage, sizeof(unsigned char) * pixelDataSize, pUserData));
	}
	else
	{
		pImage->SetPixels((unsigned char*)conf_malloc(sizeof(unsigned char) * pixelDataSize), true);
	}

	memcpy(pImage->GetPixels(), (unsigned char*)psPVRHeader + totalHeaderSizeWithMetadata, pixelDataSize);

	return true;
#endif
}

#ifndef IMAGE_DISABLE_KTX
bool iLoadKTXFromMemory(Image* pImage, const char* memory, uint32_t memSize, memoryAllocationFunc pAllocator /*= NULL*/, void* pUserData /*= NULL*/)
{
	TinyKtx_Callbacks callbacks {
			&tinyktxddsCallbackError,
			&tinyktxddsCallbackAlloc,
			&tinyktxddsCallbackFree,
			&tinyktxddsCallbackRead,
			&tinyktxddsCallbackSeek,
			&tinyktxddsCallbackTell
	};

    FileStream *fh = fsOpenReadOnlyMemory(memory, memSize);

	auto ctx = TinyKtx_CreateContext( &callbacks, (void*)fh);
	bool headerOkay = TinyKtx_ReadHeader(ctx);
	if(!headerOkay) {
        fsCloseStream(fh);
		TinyKtx_DestroyContext(ctx);
		return false;
	}

	uint32_t w = TinyKtx_Width(ctx);
	uint32_t h = TinyKtx_Height(ctx);
	uint32_t d = TinyKtx_Depth(ctx);
	uint32_t s = TinyKtx_ArraySlices(ctx);
	uint32_t mm = TinyKtx_NumberOfMipmaps(ctx);
	TinyImageFormat fmt = TinyImageFormat_FromTinyKtxFormat(TinyKtx_GetFormat(ctx));
	if(fmt == TinyImageFormat_UNDEFINED) {
        fsCloseStream(fh);
		TinyKtx_DestroyContext(ctx);
		return false;
	}

    // TheForge Image uses d = 0 as cubemap marker
    if(TinyKtx_IsCubemap(ctx)) d = 0;
    else d = d ? d : 1;
    s = s ? s : 1;

	pImage->RedefineDimensions(fmt, w, h, d, mm, s);
    pImage->SetMipsAfterSlices(true); // tinyDDS API is mips after slices even if DDS traditionally weren't

	int size = pImage->GetMipMappedSize();

	if (pAllocator)
	{
		pImage->SetPixels((uint8_t*)pAllocator(pImage, size, pUserData));
	}
	else
	{
		pImage->SetPixels((uint8_t*)conf_malloc(sizeof(uint8_t) * size), true);
	}
    
    for (uint mipMapLevel = 0; mipMapLevel < pImage->GetMipMapCount(); mipMapLevel++)
    {
        uint8_t const* src = (uint8_t const*) TinyKtx_ImageRawData(ctx, mipMapLevel);
        uint8_t *dst = pImage->GetPixels(mipMapLevel, 0);

        if(TinyKtx_IsMipMapLevelUnpacked(ctx, mipMapLevel)) {
            uint32_t const srcStride = TinyKtx_UnpackedRowStride(ctx, mipMapLevel);
            uint32_t const dstStride = (pImage->GetWidth(mipMapLevel) * TinyImageFormat_BitSizeOfBlock(fmt)) /
                                        (TinyImageFormat_PixelCountOfBlock(fmt) * 8);

            for (uint32_t ww = 0u; ww < TinyKtx_ArraySlices(ctx); ++ww) {
                for (uint32_t zz = 0; zz < TinyKtx_Depth(ctx); ++zz) {
                    for (uint32_t yy = 0; yy < TinyKtx_Height(ctx); ++yy) {
                        memcpy(dst, src, dstStride);
                        src += srcStride;
                        dst += dstStride;
                    }
                }
            }
        } else {
            // fast path data is packed we can just copy
            size_t const expectedSize = pImage->GetMipMappedSize(mipMapLevel, 1);
            size_t const fileSize = TinyKtx_ImageSize(ctx, mipMapLevel);
            if (expectedSize != fileSize) {
                LOGF(LogLevel::eERROR, "DDS file %s mipmap %i size error %liu < %liu", fsGetPathAsNativeString(pImage->GetPath()),mipMapLevel, expectedSize, fileSize);
                return false;
            }
            memcpy(dst, src, fileSize);
        }
	}

    fsCloseStream(fh);
	TinyKtx_DestroyContext(ctx);
	return true;
}
#endif

#if defined(ORBIS)

// loads GNF header from memory
static GnfError iLoadGnfHeaderFromMemory(struct sce::Gnf::Header* outHeader, MemoryStream* mp)
{
	if (outHeader == NULL)    //  || gnfFile == NULL)
	{
		return kGnfErrorInvalidPointer;
	}
	outHeader->m_magicNumber = 0;
	outHeader->m_contentsSize = 0;

	mp->Read(outHeader, sizeof(sce::Gnf::Header));
	//MemFopen::fread(outHeader, sizeof(sce::Gnf::Header), 1, mp);

	//	fseek(gnfFile, 0, SEEK_SET);
	//	fread(outHeader, sizeof(sce::Gnf::Header), 1, gnfFile);
	if (outHeader->m_magicNumber != sce::Gnf::kMagic)
	{
		return kGnfErrorNotGnfFile;
	}
	return kGnfErrorNone;
}

// content size is sizeof(sce::Gnf::Contents)+gnfContents->m_numTextures*sizeof(sce::Gnm::Texture)+ paddings which is a variable of: gnfContents->alignment
static uint32_t iComputeContentSize(const sce::Gnf::Contents* gnfContents)
{
	// compute the size of used bytes
	uint32_t headerSize = sizeof(sce::Gnf::Header) + sizeof(sce::Gnf::Contents) + gnfContents->m_numTextures * sizeof(sce::Gnm::Texture);
	// add the paddings
	uint32_t align = 1 << gnfContents->m_alignment;    // actual alignment
	size_t   mask = align - 1;
	uint32_t missAligned = (headerSize & mask);    // number of bytes after the alignemnet point
	if (missAligned)                               // if none zero we have to add paddings
	{
		headerSize += align - missAligned;
	}
	return headerSize - sizeof(sce::Gnf::Header);
}

// loads GNF content from memory
static GnfError iReadGnfContentsFromMemory(sce::Gnf::Contents* outContents, uint32_t contentsSizeInBytes, MemoryBuffer* memstart)
{
	// now read the content data ...
	memstart->Read(outContents, contentsSizeInBytes);
	//MemFopen::fread(outContents, contentsSizeInBytes, 1, memstart);

	if (outContents->m_alignment > 31)
	{
		return kGnfErrorAlignmentOutOfRange;
	}

	if (outContents->m_version == 1)
	{
		if ((outContents->m_numTextures * sizeof(sce::Gnm::Texture) + sizeof(sce::Gnf::Contents)) != contentsSizeInBytes)
		{
			return kGnfErrorContentsSizeMismatch;
		}
	}
	else
	{
		if (outContents->m_version != sce::Gnf::kVersion)
		{
			return kGnfErrorVersionMismatch;
		}
		else
		{
			if (iComputeContentSize(outContents) != contentsSizeInBytes)
				return kGnfErrorContentsSizeMismatch;
		}
	}

	return kGnfErrorNone;
}

//------------------------------------------------------------------------------
//  Loads a GNF file from memory.
//
bool Image::iLoadGNFFromMemory(const char* memory, size_t memSize, const bool useMipMaps)
{
	GnfError result = kGnfErrorNone;

	MemoryStream m1(memory, memSize);

	sce::Gnf::Header header;
	result = iLoadGnfHeaderFromMemory(&header, m1);
	if (result != 0)
	{
		return false;
	}

	char*               memoryArray = (char*)conf_calloc(header.m_contentsSize, sizeof(char));
	sce::Gnf::Contents* gnfContents = NULL;
	gnfContents = (sce::Gnf::Contents*)memoryArray;

	// move the pointer behind the header
	const char*  mp = memory + sizeof(sce::Gnf::Header);
	MemoryStream m2(mp, memSize - sizeof(sce::Gnf::Header));

	result = iReadGnfContentsFromMemory(gnfContents, header.m_contentsSize, m2);

	mWidth = gnfContents->m_textures[0].getWidth();
	mHeight = gnfContents->m_textures[0].getHeight();
	mDepth = gnfContents->m_textures[0].getDepth();

	mMipMapCount =
		((!useMipMaps) || (gnfContents->m_textures[0].getLastMipLevel() == 0)) ? 1 : gnfContents->m_textures[0].getLastMipLevel();
	mArrayCount = (gnfContents->m_textures[0].getLastArraySliceIndex() > 1) ? gnfContents->m_textures[0].getLastArraySliceIndex() : 1;

	uint32 dataFormat = gnfContents->m_textures[0].getDataFormat().m_asInt;

	if (dataFormat == sce::Gnm::kDataFormatBc1Unorm.m_asInt || dataFormat == sce::Gnm::kDataFormatBc1UnormSrgb.m_asInt)
		mFormat = ImageFormat::GNF_BC1;
	//	else if(dataFormat == sce::Gnm::kDataFormatBc2Unorm.m_asInt || dataFormat == sce::Gnm::kDataFormatBc2UnormSrgb.m_asInt)
	//		format = ImageFormat::GNF_BC2;
	else if (dataFormat == sce::Gnm::kDataFormatBc3Unorm.m_asInt || dataFormat == sce::Gnm::kDataFormatBc3UnormSrgb.m_asInt)
		mFormat = ImageFormat::GNF_BC3;
	//	else if(dataFormat == sce::Gnm::kDataFormatBc4Unorm.m_asInt || dataFormat == sce::Gnm::kDataFormatBc4UnormSrgb.m_asInt)
	//		format = ImageFormat::GNF_BC4;
	// it seems in the moment there is no kDataFormatBc5UnormSrgb .. so I just check for the SRGB flag
	else if (
		dataFormat == sce::Gnm::kDataFormatBc5Unorm.m_asInt ||
		((dataFormat == sce::Gnm::kDataFormatBc5Unorm.m_asInt) &&
		 (gnfContents->m_textures[0].getDataFormat().getTextureChannelType() == sce::Gnm::kTextureChannelTypeSrgb)))
		mFormat = ImageFormat::GNF_BC5;
	//	else if(dataFormat == sce::Gnm::kDataFormatBc6Unorm.m_asInt || dataFormat == sce::Gnm::kDataFormatBc6UnormSrgb.m_asInt)
	//		format = ImageFormat::GNF_BC6;
	else if (dataFormat == sce::Gnm::kDataFormatBc7Unorm.m_asInt || dataFormat == sce::Gnm::kDataFormatBc7UnormSrgb.m_asInt)
		mFormat = ImageFormat::GNF_BC7;
	else
	{
		LOGF(LogLevel::eERROR, "Couldn't find the data format of the texture");
		return false;
	}

	//
	// storing the GNF header in the extra data
	//
	// we do this because on the addTexture level, we would like to have all this data to allocate and load the data
	//
	pAdditionalData = (unsigned char*)conf_calloc(header.m_contentsSize, sizeof(unsigned char));
	memcpy(pAdditionalData, gnfContents, header.m_contentsSize);

	// storing all the pixel data in pixels
	//
	// unfortunately that means we have the data twice in pixels and then in VRAM ...
	//
	sce::Gnm::SizeAlign pixelsSa = getTexturePixelsSize(gnfContents, 0);

	// move pointer forward
	const char*  memPoint = mp + header.m_contentsSize + getTexturePixelsByteOffset(gnfContents, 0);
	//MemoryBuffer m3(memPoint, memSize - (sizeof(sce::Gnf::Header) + header.m_contentsSize + getTexturePixelsByteOffset(gnfContents, 0)));
	MemoryStream m3(memPoint, memSize - (sizeof(sce::Gnf::Header) + header.m_contentsSize + getTexturePixelsByteOffset(gnfContents, 0)));

	// dealing with mip-map stuff ... ???
	int size = pixelsSa.m_size;    //getMipMappedSize(0, nMipMaps);
	pData = (unsigned char*)conf_malloc(sizeof(unsigned char) * size);

	m3.Read(pData, size);
	//MemFopen::fread(pData, 1, size, m3);

	/*
  if (isCube()){
  for (int face = 0; face < 6; face++)
  {
  for (uint mipMapLevel = 0; mipMapLevel < nMipMaps; mipMapLevel++)
  {
  int faceSize = getMipMappedSize(mipMapLevel, 1) / 6;
  unsigned char *src = getPixels(mipMapLevel) + face * faceSize;

  memread(src, 1, faceSize, mp);
  }
  if ((useMipMaps ) && header.dwMipMapCount > 1)
  {
  memseek(mp, memory, getMipMappedSize(1, header.dwMipMapCount - 1) / 6, SEEK_CUR);
  }
  }
  }
  else
  {
  memread(pixels, 1, size, mpoint);
  }
  */
	conf_free(gnfContents);

	return !result;
}
#endif

//------------------------------------------------------------------------------
#ifndef IMAGE_DISABLE_GOOGLE_BASIS
//  Loads a Basis data from memory.
//
bool iLoadBASISFromMemory(Image* pImage, const char* memory, uint32_t memSize, memoryAllocationFunc pAllocator /*= NULL*/, void* pUserData /*= NULL*/)
{
	if (memory == NULL || memSize == 0)
		return false;

	basist::etc1_global_selector_codebook sel_codebook(basist::g_global_selector_cb_size, basist::g_global_selector_cb);

	eastl::vector<uint8_t> basis_data;
	basis_data.resize(memSize);
	memcpy(basis_data.data(), memory, memSize);

	basist::basisu_transcoder decoder(&sel_codebook);

	basist::basisu_file_info fileinfo;
	if (!decoder.get_file_info(basis_data.data(), (uint32_t)basis_data.size(), fileinfo))
	{
		LOGF(LogLevel::eERROR, "Failed retrieving Basis file information!");
		return false;
	}

	ASSERT(fileinfo.m_total_images == fileinfo.m_image_mipmap_levels.size());
	ASSERT(fileinfo.m_total_images == decoder.get_total_images(&basis_data[0], (uint32_t)basis_data.size()));

	basist::basisu_image_info imageinfo;
	decoder.get_image_info(basis_data.data(), (uint32_t)basis_data.size(), imageinfo, 0);

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

	pImage->RedefineDimensions(imageFormat, width, height, depth, mipMapCount, arrayCount);

	int size = pImage->GetMipMappedSize();

	if (pAllocator)
	{
		pImage->SetPixels((unsigned char*)pAllocator(pImage, size, pUserData));
	}
	else
	{
		pImage->SetPixels((unsigned char*)conf_malloc(sizeof(unsigned char) * size), true);
	}

	decoder.start_transcoding(basis_data.data(), (uint32_t)basis_data.size());


	for (uint32_t image_index = 0; image_index < fileinfo.m_total_images; image_index++)
	{
		for (uint32_t level_index = 0; level_index < fileinfo.m_image_mipmap_levels[image_index]; level_index++)
		{
			basist::basisu_image_level_info level_info;

			if (!decoder.get_image_level_info(&basis_data[0], (uint32_t)basis_data.size(), level_info, image_index, level_index))
			{
				LOGF(LogLevel::eERROR, "Failed retrieving image level information (%u %u)!\n", image_index, level_index);
				return false;
			}

			if (basisTextureFormat == basist::cTFPVRTC1_4_OPAQUE_ONLY)
			{
				if (!isPowerOf2(level_info.m_width) || !isPowerOf2(level_info.m_height))
				{
					LOGF(LogLevel::eWARNING, "Warning: Will not transcode image %u level %u res %ux%u to PVRTC1 (one or more dimension is not a power of 2)\n", image_index, level_index, level_info.m_width, level_info.m_height);

					// Can't transcode this image level to PVRTC because it's not a pow2 (we're going to support transcoding non-pow2 to the next larger pow2 soon)
					continue;
				}
			}

			if (!decoder.transcode_image_level(&basis_data[0], (uint32_t)basis_data.size(), image_index, level_index, pImage->GetPixels(), (uint32_t)(imageinfo.m_num_blocks_x * imageinfo.m_num_blocks_y), basisTextureFormat, 0))
			{
				LOGF(LogLevel::eERROR, "Failed transcoding image level (%u %u)!\n", image_index, level_index);
				return false;
			}
		}
	}

	return true;
}
#endif


bool iLoadSVTFromMemory(Image* pImage, const char* memory, uint32_t memSize, memoryAllocationFunc pAllocator /*= NULL*/, void* pUserData /*= NULL*/)
{
	if (memory == NULL || memSize == 0)
		return false;

	eastl::vector<uint8_t> temp_data;
	temp_data.resize(memSize);
	memcpy(temp_data.data(), memory, memSize);

	uint width;
	memcpy(&width, &temp_data[0], sizeof(uint));

	uint height;
	memcpy(&height, &temp_data[4], sizeof(uint));

	uint mipMapCount;
	memcpy(&mipMapCount, &temp_data[8], sizeof(uint));

	uint pageSize;
	memcpy(&pageSize, &temp_data[12], sizeof(uint));

	uint numberOfComponents;
	memcpy(&numberOfComponents, &temp_data[16], sizeof(uint));

	if(numberOfComponents == 4)
		pImage->RedefineDimensions(TinyImageFormat_R8G8B8A8_UNORM, width, height, 1, mipMapCount, 1);
	else
		pImage->RedefineDimensions(TinyImageFormat_R8G8B8A8_UNORM, width, height, 1, mipMapCount, 1);

	int size = pImage->GetMipMappedSize();

	if (pAllocator)
	{
		pImage->SetPixels((unsigned char*)pAllocator(pImage, size, pUserData));
	}
	else
	{
		pImage->SetPixels((unsigned char*)conf_malloc(sizeof(unsigned char) * size), true);
	}

	memcpy(pImage->GetPixels(), &temp_data[20], sizeof(unsigned char) * size);
	temp_data.set_capacity(0);

	return true;
}

// Image loading
// struct of table for file format to loading function
struct ImageLoaderDefinition
{
	char const* mExtension;
	Image::ImageLoaderFunction pLoader;
};

#define MAX_IMAGE_LOADERS 10
static ImageLoaderDefinition gImageLoaders[MAX_IMAGE_LOADERS];
uint32_t gImageLoaderCount = 0;

// One time call to initialize all loaders
void Image::Init()
{
	gImageLoaders[gImageLoaderCount++] = {"dds", iLoadDDSFromMemory};
	gImageLoaders[gImageLoaderCount++] = {"pvr", iLoadPVRFromMemory};
#ifndef IMAGE_DISABLE_KTX
	gImageLoaders[gImageLoaderCount++] = {"ktx", iLoadKTXFromMemory};
#endif
#if defined(ORBIS)
	gImageLoaders[gImageLoaderCount++] = {"gnf", iLoadGNFFromMemory};
#endif
#ifndef IMAGE_DISABLE_GOOGLE_BASIS
	gImageLoaders[gImageLoaderCount++] = {"basis", iLoadBASISFromMemory};
#endif
	gImageLoaders[gImageLoaderCount++] = {"svt", iLoadSVTFromMemory};
}

void Image::Exit()
{
}

void Image::AddImageLoader(const char* pExtension, ImageLoaderFunction pFunc) {
	gImageLoaders[gImageLoaderCount++] = { pExtension, pFunc };
}

bool Image::LoadFromMemory(
	void const* mem, uint32_t size, char const* extension, memoryAllocationFunc pAllocator, void* pUserData)
{
	// try loading the format
	bool loaded = false;
	for (uint32_t i = 0; i < gImageLoaderCount; ++i)
	{
		ImageLoaderDefinition const& def = gImageLoaders[i];
		if (stricmp(extension, def.mExtension) == 0)
		{
			loaded = def.pLoader(this, (char const*)mem, size, pAllocator, pUserData);
			break;
		}
	}
	return loaded;
}

bool Image::LoadFromFile(const Path* filePath, memoryAllocationFunc pAllocator, void* pUserData)
{
	// clear current image
	Clear();

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
#else
		extension = "dds";
#endif

        loadFilePath = fsAppendPathExtension(filePath, extension);
    } else {
        extension = extensionComponent.buffer;
        loadFilePath = fsCopyPath(filePath);
    }
		
    FileStream* fh = fsOpenFile(loadFilePath, FM_READ_BINARY);
	
	if (!fh)
	{
		LOGF(LogLevel::eERROR, "\"%s\": Image file not found.", fsGetPathAsNativeString(loadFilePath));
		return false;
	}
	
	// load file into memory
	
    ssize_t length = fsGetStreamFileSize(fh);
	if (length <= 0)
	{
		LOGF(LogLevel::eERROR, "\"%s\": Image is an empty file.", fsGetPathAsNativeString(loadFilePath));
        fsCloseStream(fh);
		return false;
	}
	
	// read and close file.
	char* data = (char*)conf_malloc(length * sizeof(char));
    fsReadFromStream(fh, data, length);
    fsCloseStream(fh);

	// try loading the format
	bool loaded = false;
	bool support = false;
	for (int i = 0; i < (int)gImageLoaderCount; i++)
	{
		if (stricmp(extension, gImageLoaders[i].mExtension) == 0)
		{
			support = true;
			loaded = gImageLoaders[i].pLoader(this, data, (uint32_t)length, pAllocator, pUserData);
			if (loaded)
			{
				break;
			}
		}
	}
	if (!support)
	{
		LOGF(LogLevel::eERROR, "Can't load this file format for image  :  %s", fsGetPathAsNativeString(loadFilePath));
	}
	else
	{
		mLoadFilePath = loadFilePath;
	}
	// cleanup the compressed data
	conf_free(data);

	return loaded;
}

bool Image::Convert(const TinyImageFormat newFormat)
{
	// TODO add RGBE8 to tiny image format
	if(!TinyImageFormat_CanDecodeLogicalPixelsF(mFormat)) return false;
	if(!TinyImageFormat_CanEncodeLogicalPixelsF(newFormat)) return false;

	int pixelCount = GetNumberOfPixels(0, mMipMapCount);

	ubyte* newPixels;
	newPixels = (unsigned char*)conf_malloc(sizeof(unsigned char) * GetMipMappedSize(0, mMipMapCount));

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

template <typename T>
void buildMipMap(T* dst, const T* src, const uint w, const uint h, const uint d, const uint c)
{
	uint xOff = (w < 2) ? 0 : c;
	uint yOff = (h < 2) ? 0 : c * w;
	uint zOff = (d < 2) ? 0 : c * w * h;

	for (uint z = 0; z < d; z += 2)
	{
		for (uint y = 0; y < h; y += 2)
		{
			for (uint x = 0; x < w; x += 2)
			{
				for (uint i = 0; i < c; i++)
				{
					*dst++ = (src[0] + src[xOff] + src[yOff] + src[yOff + xOff] + src[zOff] + src[zOff + xOff] + src[zOff + yOff] +
							  src[zOff + yOff + xOff]) /
							 8;
					src++;
				}
				src += xOff;
			}
			src += yOff;
		}
		src += zOff;
	}
}

bool Image::GenerateMipMaps(const uint32_t mipMaps)
{
	if (TinyImageFormat_IsCompressed(mFormat))
		return false;
	if (!(mWidth) || !isPowerOf2(mHeight) || !isPowerOf2(mDepth))
		return false;

	uint actualMipMaps = min(mipMaps, GetMipMapCountFromDimensions());

	if (mMipMapCount != actualMipMaps)
	{
		int size = GetMipMappedSize(0, actualMipMaps);
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
		ubyte* src = GetPixels(0, arraySlice);
		ubyte* dst = GetPixels(1, arraySlice);

		for (uint level = 1; level < mMipMapCount; level++)
		{
			int w = GetWidth(level - 1);
			int h = GetHeight(level - 1);
			int d = GetDepth(level - 1);

			uint32_t srcSize = GetMipMappedSize(level - 1, 1) / n;
			uint32_t dstSize = GetMipMappedSize(level, 1) / n;

			for (int i = 0; i < n; i++)
			{
				// only homogoenous is supported via this method
				// TODO use decode/encode for the others
				// TODO check these methods work for SNORM
				if(TinyImageFormat_IsHomogenous(mFormat)) {
					uint32 redChanWidth = TinyImageFormat_ChannelBitWidth(mFormat, TinyImageFormat_LC_Red);
					if (redChanWidth == 32 && TinyImageFormat_IsFloat(mFormat))
					{
						buildMipMap((float*)dst, (float*)src, w, h, d, nChannels);
					}
					else if (redChanWidth == 32)
					{
						buildMipMap((uint32_t*)dst, (uint32_t*)src, w, h, d, nChannels);
					}
					else if (redChanWidth == 16)
					{
						buildMipMap((uint16_t*)dst, (uint16_t*)src, w, h, d, nChannels);
					}
					else if (redChanWidth == 8)
					{
						buildMipMap((uint8_t*)dst, (uint8_t*)src, w, h, d, nChannels);
					}
					// TODO fall back to to be written generic downsizer
				}
				src += srcSize;
				dst += dstSize;
			}
		}
	}

	return true;
}

// -- IMAGE SAVING --
static void tinyktxCallbackError(void* user, char const* msg) {
	LOGF( LogLevel::eERROR, "Tiny_Ktx ERROR: %s", msg);
}

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
		case TinyImageFormat_R8_UNORM: stbi_write_png(fileName, mWidth, mHeight, 1, pData, 0); break;
		case TinyImageFormat_R8G8_UNORM: stbi_write_png(fileName, mWidth, mHeight, 2, pData, 0); break;
		case TinyImageFormat_R8G8B8_UNORM: stbi_write_png(fileName, mWidth, mHeight, 3, pData, 0); break;
		case TinyImageFormat_R8G8B8A8_UNORM: stbi_write_png(fileName, mWidth, mHeight, 4, pData, 0); break;
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

	const char *fileName = fsGetPathAsNativeString(filePath);

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

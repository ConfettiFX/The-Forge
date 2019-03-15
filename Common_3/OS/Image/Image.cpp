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

#include "Image.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IMemoryManager.h"
#include "../../ThirdParty/OpenSource/TinyEXR/tinyexr.h"
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

// --- IMAGE HEADERS ---

#pragma pack(push, 1)

#define DDPF_ALPHAPIXELS 0x00000001
#define DDPF_FOURCC 0x00000004
#define DDPF_RGB 0x00000040

#define DDSD_CAPS 0x00000001
#define DDSD_HEIGHT 0x00000002
#define DDSD_WIDTH 0x00000004
#define DDSD_PITCH 0x00000008
#define DDSD_PIXELFORMAT 0x00001000
#define DDSD_MIPMAPCOUNT 0x00020000
#define DDSD_LINEARSIZE 0x00080000
#define DDSD_DEPTH 0x00800000

#define DDSCAPS_COMPLEX 0x00000008
#define DDSCAPS_TEXTURE 0x00001000
#define DDSCAPS_MIPMAP 0x00400000

#define DDSCAPS2_CUBEMAP 0x00000200
#define DDSCAPS2_VOLUME 0x00200000

#define DDSCAPS2_CUBEMAP_POSITIVEX 0x00000400
#define DDSCAPS2_CUBEMAP_NEGATIVEX 0x00000800
#define DDSCAPS2_CUBEMAP_POSITIVEY 0x00001000
#define DDSCAPS2_CUBEMAP_NEGATIVEY 0x00002000
#define DDSCAPS2_CUBEMAP_POSITIVEZ 0x00004000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ 0x00008000
#define DDSCAPS2_CUBEMAP_ALL_FACES                                                                                       \
	(DDSCAPS2_CUBEMAP_POSITIVEX | DDSCAPS2_CUBEMAP_NEGATIVEX | DDSCAPS2_CUBEMAP_POSITIVEY | DDSCAPS2_CUBEMAP_NEGATIVEY | \
	 DDSCAPS2_CUBEMAP_POSITIVEZ | DDSCAPS2_CUBEMAP_NEGATIVEZ)

#define D3D10_RESOURCE_MISC_TEXTURECUBE 0x4
#define D3D10_RESOURCE_DIMENSION_BUFFER 1
#define D3D10_RESOURCE_DIMENSION_TEXTURE1D 2
#define D3D10_RESOURCE_DIMENSION_TEXTURE2D 3
#define D3D10_RESOURCE_DIMENSION_TEXTURE3D 4

struct DDSHeader
{
	uint32 mDWMagic;
	uint32 mDWSize;
	uint32 mDWFlags;
	uint32 mDWHeight;
	uint32 mDWWidth;
	uint32 mDWPitchOrLinearSize;
	uint32 mDWDepth;
	uint32 mDWMipMapCount;
	uint32 mReserved[11];

	struct
	{
		uint32 mDWSize;
		uint32 mDWFlags;
		uint32 mDWFourCC;
		uint32 mDWRGBBitCount;
		uint32 mDWRBitMask;
		uint32 mDWGBitMask;
		uint32 mDWBBitMask;
		uint32 mDWRGBAlphaBitMask;
	} mPixelFormat;

	struct
	{
		uint32 mDWCaps1;
		uint32 mDWCaps2;
		uint32 mReserved[2];    //caps3 and caps4
	} mCaps;

	uint32 mDWReserved2;
};

struct DDSHeaderDX10
{
	uint32 mDXGIFormat;
	uint32 mResourceDimension;
	uint32 mMiscFlag;
	uint32 mArraySize;
	uint32 mReserved;
};

#ifdef TARGET_IOS
// Describes the header of a PVR header-texture
typedef struct PVR_Header_Texture_TAG
{
	unsigned int 	mVersion;
	unsigned int 	mFlags; //!< Various format flags.
	unsigned long 	mPixelFormat; //!< The pixel format, 8cc value storing the 4 channel identifiers and their respective sizes.
	unsigned int 	mColorSpace; //!< The Color Space of the texture, currently either linear RGB or sRGB.
	unsigned int 	mChannelType; //!< Variable type that the channel is stored in. Supports signed/unsigned int/short/char/float.
	unsigned int 	mHeight; //!< Height of the texture.
	unsigned int	mWidth; //!< Width of the texture.
	unsigned int 	mDepth; //!< Depth of the texture. (Z-slices)
	unsigned int 	mNumSurfaces; //!< Number of members in a Texture Array.
	unsigned int 	mNumFaces; //!< Number of faces in a Cube Map. Maybe be a value other than 6.
	unsigned int 	mNumMipMaps; //!< Number of MIP Maps in the texture - NB: Includes top level.
	unsigned int 	mMetaDataSize; //!< Size of the accompanying meta data.
} PVR_Texture_Header;

const unsigned int gPvrtexV3HeaderVersion = 0x03525650;
#endif

#pragma pack(pop)

// --- BLOCK DECODING ---

void iDecodeColorBlock(
	unsigned char* dest, int w, int h, int xOff, int yOff, ImageFormat::Enum format, int red, int blue, unsigned char* src)
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

	if (c0 > c1 || format == ImageFormat::DXT5)
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

void iDecodeCompressedImage(unsigned char* dest, unsigned char* src, const int width, const int height, const ImageFormat::Enum format)
{
	int sx = (width < 4) ? width : 4;
	int sy = (height < 4) ? height : 4;

	int nChannels = ImageFormat::GetChannelCount(format);

	for (int y = 0; y < height; y += 4)
	{
		for (int x = 0; x < width; x += 4)
		{
			unsigned char* dst = dest + (y * width + x) * nChannels;
			if (format == ImageFormat::DXT3)
			{
				iDecodeDXT3Block(dst + 3, sx, sy, nChannels, width * nChannels, src);
				src += 8;
			}
			else if (format == ImageFormat::DXT5)
			{
				iDecodeDXT5Block(dst + 3, sx, sy, nChannels, width * nChannels, src);
				src += 8;
			}
			if (format <= ImageFormat::DXT5)
			{
				iDecodeColorBlock(dst, sx, sy, nChannels, width * nChannels, format, 0, 2, src);
				src += 8;
			}
			else
			{
				if (format == ImageFormat::ATI1N)
				{
					iDecodeDXT5Block(dst, sx, sy, 1, width, src);
					src += 8;
				}
				else if ((format == ImageFormat::ATI2N))
				{
					iDecodeDXT5Block(dst, sx, sy, 2, width * 2, src + 8);
					iDecodeDXT5Block(dst + 1, sx, sy, 2, width * 2, src);
					src += 16;
				}
				else
					return;
			}
		}
	}
}

template <typename T>
inline void swapPixelChannels(T* pixels, int num_pixels, const int channels, const int ch0, const int ch1)
{
	for (int i = 0; i < num_pixels; i++)
	{
		T tmp = pixels[ch1];
		pixels[ch1] = pixels[ch0];
		pixels[ch0] = tmp;
		pixels += channels;
	}
}

Image::Image()
{
	pData = NULL;
	mLoadFileName = "";
	mWidth = 0;
	mHeight = 0;
	mDepth = 0;
	mMipMapCount = 0;
	mArrayCount = 0;
	mFormat = ImageFormat::NONE;
	mAdditionalDataSize = 0;
	pAdditionalData = NULL;
	mIsRendertarget = false;
	mOwnsMemory = true;
	mLinearLayout = true;
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
	mLoadFileName = img.mLoadFileName;

	mAdditionalDataSize = img.mAdditionalDataSize;
	pAdditionalData = (unsigned char*)conf_malloc(sizeof(unsigned char) * mAdditionalDataSize);
	memcpy(pAdditionalData, img.pAdditionalData, mAdditionalDataSize);
}

unsigned char* Image::Create(const ImageFormat::Enum fmt, const int w, const int h, const int d, const int mipMapCount, const int arraySize)
{
	mFormat = fmt;
	mWidth = w;
	mHeight = h;
	mDepth = d;
	mMipMapCount = mipMapCount;
	mArrayCount = arraySize;
	mOwnsMemory = true;

	uint holder = GetMipMappedSize(0, mMipMapCount);
	pData = (unsigned char*)conf_malloc(sizeof(unsigned char) * holder * mArrayCount);
	memset(pData, 0x00, holder * mArrayCount);
	mLoadFileName = "Undefined";

	return pData;
}

unsigned char* Image::Create(const ImageFormat::Enum fmt, const int w, const int h, const int d, const int mipMapCount, const int arraySize, unsigned char* rawData)
{
	mFormat = fmt;
	mWidth = w;
	mHeight = h;
	mDepth = d;
	mMipMapCount = mipMapCount;
	mArrayCount = arraySize;
	mOwnsMemory = false;

	pData = rawData;	
	mLoadFileName = "Undefined";

	return pData;
}

void Image::RedefineDimensions(
	const ImageFormat::Enum fmt, const int w, const int h, const int d, const int mipMapCount, const int arraySize)
{
	//Redefine image that was loaded in
	mFormat = fmt;
	mWidth = w;
	mHeight = h;
	mDepth = d;
	mMipMapCount = mipMapCount;
	mArrayCount = arraySize;
}

void Image::Destroy()
{
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
	mFormat = ImageFormat::NONE;

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

	return pData + GetMipMappedSize(0, mMipMapCount) * arraySlice + GetMipMappedSize(0, mipMapLevel);
}

uint Image::GetWidth(const int mipMapLevel) const
{
	int a = mWidth >> mipMapLevel;
	return (a == 0) ? 1 : a;
}

uint Image::GetHeight(const int mipMapLevel) const
{
	int a = mHeight >> mipMapLevel;
	return (a == 0) ? 1 : a;
}

uint Image::GetDepth(const int mipMapLevel) const
{
	int a = mDepth >> mipMapLevel;
	return (a == 0) ? 1 : a;
}

uint Image::GetMipMapCountFromDimensions() const
{
	uint m = max(mWidth, mHeight);
	m = max(m, mDepth);

	int i = 0;
	while (m > 0)
	{
		m >>= 1;
		i++;
	}

	return i;
}

uint Image::GetArraySliceSize(const uint mipMapLevel, ImageFormat::Enum srcFormat) const
{
	int w = GetWidth(mipMapLevel);
	int h = GetHeight(mipMapLevel);

	if (srcFormat == ImageFormat::NONE)
		srcFormat = mFormat;

	int size;
	if (ImageFormat::IsCompressedFormat(srcFormat))
	{
		size = ((w + 3) >> 2) * ((h + 3) >> 2) * ImageFormat::GetBytesPerBlock(srcFormat);
	}
	else
	{
		size = w * h * ImageFormat::GetBytesPerPixel(srcFormat);
	}

	return size;
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
	if (mFormat < ImageFormat::R32F || mFormat > ImageFormat::RGBA32F)
		return false;

	int nElements = GetNumberOfPixels(0, mMipMapCount) * ImageFormat::GetChannelCount(mFormat) * mArrayCount;

	if (nElements <= 0)
		return false;

	float minVal = FLT_MAX;
	float maxVal = -FLT_MAX;
	for (int i = 0; i < nElements; i++)
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
	if (mFormat < ImageFormat::R32F || mFormat > ImageFormat::RGBA32F)
		return false;

	float min, max;
	GetColorRange(min, max);

	int nElements = GetNumberOfPixels(0, mMipMapCount) * ImageFormat::GetChannelCount(mFormat) * mArrayCount;

	float s = 1.0f / (max - min);
	float b = -min * s;
	for (int i = 0; i < nElements; i++)
	{
		float d = ((float*)pData)[i];
		((float*)pData)[i] = d * s + b;
	}

	return true;
}

bool Image::Uncompress()
{
	if (((mFormat >= ImageFormat::PVR_2BPP) && (mFormat <= ImageFormat::PVR_4BPPA)) ||
		((mFormat >= ImageFormat::PVR_2BPP_SRGB) && (mFormat <= ImageFormat::PVR_4BPPA_SRGB)) ||
		((mFormat >= ImageFormat::ETC1) && (mFormat <= ImageFormat::ATCI)))
	{
		//  no decompression
		return false;
	}

	if (ImageFormat::IsCompressedFormat(mFormat))
	{
		ImageFormat::Enum destFormat;
		if (mFormat >= ImageFormat::ATI1N)
		{
			destFormat = (mFormat == ImageFormat::ATI1N) ? ImageFormat::I8 : ImageFormat::IA8;
		}
		else
		{
			destFormat = (mFormat == ImageFormat::DXT1) ? ImageFormat::RGB8 : ImageFormat::RGBA8;
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
	}

	return true;
}

bool Image::Unpack()
{
	int pixelCount = GetNumberOfPixels(0, mMipMapCount);

	ubyte* newPixels;
	if (mFormat == ImageFormat::RGBE8)
	{
		mFormat = ImageFormat::RGB32F;
		newPixels = (unsigned char*)conf_malloc(sizeof(unsigned char) * GetMipMappedSize(0, mMipMapCount));

		for (int i = 0; i < pixelCount; i++)
		{
			((vec3*)newPixels)[i] = rgbeToRGB(pData + 4 * i);
		}
	}
	else if (mFormat == ImageFormat::RGB565)
	{
		mFormat = ImageFormat::RGB8;
		newPixels = (unsigned char*)conf_malloc(sizeof(unsigned char) * GetMipMappedSize(0, mMipMapCount));

		for (int i = 0; i < pixelCount; i++)
		{
			unsigned int rgb565 = (unsigned int)(((uint16_t*)pData)[i]);
			newPixels[3 * i] = (ubyte)(((rgb565 >> 11) * 2106) >> 8);
			newPixels[3 * i + 1] = (ubyte)(((rgb565 >> 5) & 0x3F) * 1037 >> 8);
			newPixels[3 * i + 2] = (ubyte)(((rgb565 & 0x1F) * 2106) >> 8);
		}
	}
	else if (mFormat == ImageFormat::RGBA4)
	{
		mFormat = ImageFormat::RGBA8;
		newPixels = (unsigned char*)conf_malloc(sizeof(unsigned char) * GetMipMappedSize(0, mMipMapCount));

		for (int i = 0; i < pixelCount; i++)
		{
			newPixels[4 * i] = (pData[2 * i + 1] & 0xF) * 17;
			newPixels[4 * i + 1] = (pData[2 * i] >> 4) * 17;
			newPixels[4 * i + 2] = (pData[2 * i] & 0xF) * 17;
			newPixels[4 * i + 3] = (pData[2 * i + 1] >> 4) * 17;
		}
	}
	else if (mFormat == ImageFormat::RGB10A2)
	{
		mFormat = ImageFormat::RGBA16;
		newPixels = (unsigned char*)conf_malloc(sizeof(unsigned char) * GetMipMappedSize(0, mMipMapCount));

		for (int i = 0; i < pixelCount; i++)
		{
			uint32 src = ((uint32*)pData)[i];
			((ushort*)newPixels)[4 * i] = (((src)&0x3FF) * 4198340) >> 16;
			((ushort*)newPixels)[4 * i + 1] = (((src >> 10) & 0x3FF) * 4198340) >> 16;
			((ushort*)newPixels)[4 * i + 2] = (((src >> 20) & 0x3FF) * 4198340) >> 16;
			((ushort*)newPixels)[4 * i + 3] = (((src >> 30) & 0x003) * 21845);
		}
	}
	else
	{
		return false;
	}

	conf_free(pData);
	pData = newPixels;

	return true;
}

uint Image::GetMipMappedSize(const uint firstMipMapLevel, uint nMipMapLevels, ImageFormat::Enum srcFormat) const
{
	uint w = GetWidth(firstMipMapLevel);
	uint h = GetHeight(firstMipMapLevel);
	uint d = GetDepth(firstMipMapLevel);

	if (srcFormat == ImageFormat::NONE)
		srcFormat = mFormat;
	
	// PVR formats get special case
	if ( (srcFormat >= ImageFormat::PVR_2BPP && srcFormat <= ImageFormat::PVR_4BPPA) ||
		 (srcFormat >= ImageFormat::PVR_2BPP_SRGB && srcFormat <= ImageFormat::PVR_4BPPA_SRGB) )
	{
		uint totalSize = 0;
		uint sizeX = w;
		uint sizeY = h;
		uint sizeD = d;
		int level = nMipMapLevels;
		
		uint minWidth = 8;
		uint minHeight = 8;
		uint minDepth = 1;
		int bpp = 4;
		
		if (srcFormat == ImageFormat::PVR_2BPP || srcFormat == ImageFormat::PVR_2BPPA || srcFormat == ImageFormat::PVR_2BPP_SRGB || srcFormat == ImageFormat::PVR_2BPPA_SRGB)
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
	
	int size = 0;
	while (nMipMapLevels)
	{
		if (ImageFormat::IsCompressedFormat(srcFormat))
		{
			size += ((w + 3) >> 2) * ((h + 3) >> 2) * d;
		}
		else
		{
			size += w * h * d;
		}
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

	if (ImageFormat::IsCompressedFormat(srcFormat))
	{
		size *= ImageFormat::GetBytesPerBlock(srcFormat);
	}
	else
	{
		size *= ImageFormat::GetBytesPerPixel(srcFormat);
	}

	return (mDepth == 0) ? 6 * size : size;
}

// Load Image Data form mData functions

bool Image::iLoadDDSFromMemory(
	const char* memory, uint32_t memSize, const bool useMipMaps, memoryAllocationFunc pAllocator, void* pUserData)
{
	DDSHeader header;

	if (memory == NULL || memSize == 0)
		return false;

	MemoryBuffer file(memory, (unsigned)memSize);
	//MemFopen file(memory, memSize);

	file.Read(&header, sizeof(header));
	//MemFopen::FileRead(&header, sizeof(header), 1, file);

	if (header.mDWMagic != MAKE_CHAR4('D', 'D', 'S', ' '))
	{
		return false;
	}

	mWidth = header.mDWWidth;
	mHeight = header.mDWHeight;
	mDepth = (header.mCaps.mDWCaps2 & DDSCAPS2_CUBEMAP) ? 0 : (header.mDWDepth == 0) ? 1 : header.mDWDepth;
	mMipMapCount = (useMipMaps == false || (header.mDWMipMapCount == 0)) ? 1 : header.mDWMipMapCount;
	mArrayCount = 1;

	if (header.mPixelFormat.mDWFourCC == MAKE_CHAR4('D', 'X', '1', '0'))
	{
		DDSHeaderDX10 dx10Header;
		file.Read(&dx10Header, sizeof(dx10Header));
		//MemFopen::FileRead(&dx10Header, sizeof(dx10Header), 1, file);

		switch (dx10Header.mDXGIFormat)
		{
			case 61: mFormat = ImageFormat::R8; break;
			case 49: mFormat = ImageFormat::RG8; break;
			case 28: mFormat = ImageFormat::RGBA8; break;

			case 56: mFormat = ImageFormat::R16; break;
			case 35: mFormat = ImageFormat::RG16; break;
			case 11: mFormat = ImageFormat::RGBA16; break;

			case 54: mFormat = ImageFormat::R16F; break;
			case 34: mFormat = ImageFormat::RG16F; break;
			case 10: mFormat = ImageFormat::RGBA16F; break;

			case 41: mFormat = ImageFormat::R32F; break;
			case 16: mFormat = ImageFormat::RG32F; break;
			case 6: mFormat = ImageFormat::RGB32F; break;
			case 2: mFormat = ImageFormat::RGBA32F; break;

			case 67: mFormat = ImageFormat::RGB9E5; break;
			case 26: mFormat = ImageFormat::RG11B10F; break;
			case 24: mFormat = ImageFormat::RGB10A2; break;

			case 71:
			case 72: mFormat = ImageFormat::DXT1; break;
			case 74: mFormat = ImageFormat::DXT3; break;
			case 77: mFormat = ImageFormat::DXT5; break;
			case 80: mFormat = ImageFormat::ATI1N; break;
			case 83: mFormat = ImageFormat::ATI2N; break;
#ifdef FORGE_JHABLE_EDITS_V01
				// these two should be different
			case 95:    // unsigned float
			case 96:    // signed float
				mFormat = ImageFormat::GNF_BC6;
				break;
			case 98:    // regular
			case 99:    // srgb
				mFormat = ImageFormat::GNF_BC7;
				break;
#endif
			default: return false;
		}
	}
	else
	{
		switch (header.mPixelFormat.mDWFourCC)
		{
			case 34: mFormat = ImageFormat::RG16; break;
			case 36: mFormat = ImageFormat::RGBA16; break;
			case 111: mFormat = ImageFormat::R16F; break;
			case 112: mFormat = ImageFormat::RG16F; break;
			case 113: mFormat = ImageFormat::RGBA16F; break;
			case 114: mFormat = ImageFormat::R32F; break;
			case 115: mFormat = ImageFormat::RG32F; break;
			case 116: mFormat = ImageFormat::RGBA32F; break;
			case MAKE_CHAR4('A', 'T', 'C', ' '): mFormat = ImageFormat::ATC; break;
			case MAKE_CHAR4('A', 'T', 'C', 'A'): mFormat = ImageFormat::ATCA; break;
			case MAKE_CHAR4('A', 'T', 'C', 'I'): mFormat = ImageFormat::ATCI; break;
			case MAKE_CHAR4('A', 'T', 'I', '1'): mFormat = ImageFormat::ATI1N; break;
			case MAKE_CHAR4('A', 'T', 'I', '2'): mFormat = ImageFormat::ATI2N; break;
			case MAKE_CHAR4('E', 'T', 'C', ' '): mFormat = ImageFormat::ETC1; break;
			case MAKE_CHAR4('D', 'X', 'T', '1'): mFormat = ImageFormat::DXT1; break;
			case MAKE_CHAR4('D', 'X', 'T', '3'): mFormat = ImageFormat::DXT3; break;
			case MAKE_CHAR4('D', 'X', 'T', '5'): mFormat = ImageFormat::DXT5; break;
			default:
				switch (header.mPixelFormat.mDWRGBBitCount)
				{
					case 8: mFormat = ImageFormat::I8; break;
					case 16:
						mFormat = (header.mPixelFormat.mDWRGBAlphaBitMask == 0xF000)
									  ? ImageFormat::RGBA4
									  : (header.mPixelFormat.mDWRGBAlphaBitMask == 0xFF00)
											? ImageFormat::IA8
											: (header.mPixelFormat.mDWBBitMask == 0x1F) ? ImageFormat::RGB565 : ImageFormat::I16;
						break;
					case 24: mFormat = ImageFormat::RGB8; break;
					case 32: mFormat = (header.mPixelFormat.mDWRBitMask == 0x3FF00000) ? ImageFormat::RGB10A2 : ImageFormat::RGBA8; break;
					default: return false;
				}
		}
	}

	int size = GetMipMappedSize(0, mMipMapCount);

	if (pAllocator)
	{
		pData = (unsigned char*)pAllocator(this, size, pUserData);
		mOwnsMemory = false;
	}
	else
	{
		pData = (unsigned char*)conf_malloc(sizeof(unsigned char) * size);
	}

	if (IsCube())
	{
		for (int face = 0; face < 6; face++)
		{
			for (uint mipMapLevel = 0; mipMapLevel < mMipMapCount; mipMapLevel++)
			{
				int            faceSize = GetMipMappedSize(mipMapLevel, 1) / 6;
				unsigned char* src = GetPixels(pData, mipMapLevel, 0) + face * faceSize;

				file.Read(src, faceSize);
				//MemFopen::FileRead(src, 1, faceSize, file);
			}
			// skip mipmaps if needed
			if (useMipMaps == false && header.mDWMipMapCount > 1)
			{
				file.Seek(GetMipMappedSize(1, header.mDWMipMapCount - 1) / 6);
				//MemFopen::FileSeek(file, GetMipMappedSize(1, header.mDWMipMapCount - 1) / 6, SEEK_CUR);
			}
		}
	}
	else
	{
		file.Read(pData, size);
		//MemFopen::FileRead(pData, 1, size, file);
	}

	if ((mFormat == ImageFormat::RGB8 || mFormat == ImageFormat::RGBA8) && header.mPixelFormat.mDWBBitMask == 0xFF)
	{
		int nChannels = ImageFormat::GetChannelCount(mFormat);
		swapPixelChannels(pData, size / nChannels, nChannels, 0, 2);
	}

	return true;
}

bool Image::iLoadPVRFromMemory(const char* memory, uint32_t size, const bool useMipmaps, memoryAllocationFunc pAllocator, void* pUserData)
{
#ifndef TARGET_IOS
	LOGERRORF("Load PVR failed: Only supported on iOS targets.");
	return 0;
#else
	
	UNREF_PARAM(useMipmaps);
	UNREF_PARAM(pAllocator);
	UNREF_PARAM(pUserData);
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
		LOGERRORF("Load PVR failed: Not a valid PVR V3 header.");
		return 0;
	}
	
	if (psPVRHeader->mPixelFormat > 3)
	{
		LOGERRORF("Load PVR failed: Not a supported PVR pixel format.  Only PVRTC is supported at the moment.");
		return 0;
	}
	
	if (psPVRHeader->mNumSurfaces > 1 && psPVRHeader->mNumFaces > 1)
	{
		LOGERRORF("Load PVR failed: Loading arrays of cubemaps isn't supported.");
		return 0;
	}

	mArrayCount = psPVRHeader->mNumSurfaces * psPVRHeader->mNumFaces;
	mWidth = psPVRHeader->mWidth;
	mHeight = psPVRHeader->mHeight;
	mDepth = psPVRHeader->mDepth;
	mMipMapCount = psPVRHeader->mNumMipMaps;
	
	bool isSrgb = (psPVRHeader->mColorSpace == 1);

	switch (psPVRHeader->mPixelFormat)
	{
		case 0:
			mFormat = isSrgb ? ImageFormat::PVR_2BPP_SRGB : ImageFormat::PVR_2BPP;
			mLinearLayout = false;
			break;
		case 1:
			mFormat = isSrgb ? ImageFormat::PVR_2BPPA_SRGB : ImageFormat::PVR_2BPPA;
			mLinearLayout = false;
			break;
		case 2:
			mFormat = isSrgb ? ImageFormat::PVR_4BPP_SRGB : ImageFormat::PVR_4BPP;
			mLinearLayout = false;
			break;
		case 3:
			mFormat = isSrgb ? ImageFormat::PVR_4BPPA_SRGB : ImageFormat::PVR_4BPPA;
			mLinearLayout = false;
			break;
		default:    // NOT SUPPORTED
			LOGERRORF("Load PVR failed: pixel type not supported. ");
			ASSERT(0);
			return 0;
	}

	// Extract the pixel data
	size_t totalHeaderSizeWithMetadata = sizeof(PVR_Texture_Header) + psPVRHeader->mMetaDataSize;
	size_t pixelDataSize = GetMipMappedSize(0, mMipMapCount, mFormat);
	pData = (unsigned char*)conf_malloc(sizeof(unsigned char) * pixelDataSize);
	memcpy(pData, (unsigned char*)psPVRHeader + totalHeaderSizeWithMetadata, pixelDataSize);

	return true;
#endif
}

bool Image::iLoadSTBIFromMemory(
	const char* buffer, uint32_t memSize, const bool useMipmaps, memoryAllocationFunc pAllocator, void* pUserData)
{
	// stbi does not generate or load mipmaps. (useMipmaps is ignored)
	if (buffer == 0 || memSize == 0)
		return false;

	int w = 0, h = 0, cmp = 0, requiredCmp = 0;
	stbi_info_from_memory((stbi_uc*)buffer, memSize, &w, &h, &cmp);

	if (w == 0 || h == 0 || cmp == 0)
	{
		return false;
	}

	requiredCmp = cmp;
	if (cmp == 3)
		requiredCmp = 4;

	mWidth = w;
	mHeight = h;
	mDepth = 1;
	mMipMapCount = 1;
	mArrayCount = 1;

	uint64_t memoryRequirement = sizeof(stbi_uc) * mWidth * mHeight * requiredCmp;

	switch (requiredCmp)
	{
		case 1: mFormat = ImageFormat::R8; break;
		case 2: mFormat = ImageFormat::RG8; break;
		case 3: mFormat = ImageFormat::RGB8; break;
		case 4: mFormat = ImageFormat::RGBA8; break;
	}

	stbi_uc* uncompressed = stbi_load_from_memory((stbi_uc*)buffer, (int)memSize, &w, &h, &cmp, requiredCmp);

	if (uncompressed == NULL)
		return false;

	if (pAllocator && !useMipmaps)
	{
		//uint32_t mipMapCount = GetMipMapCountFromDimensions(); //unused
		pData = (stbi_uc*)pAllocator(this, memoryRequirement, pUserData);
		if (pData == NULL)
		{
			LOGERRORF("Allocator returned NULL", mLoadFileName.c_str());
			return false;
		}

		memcpy(pData, uncompressed, memoryRequirement);

		mOwnsMemory = false;
	}
	else
	{
		pData = (unsigned char*)conf_malloc(memoryRequirement);
		memcpy(pData, uncompressed, memoryRequirement);
		if (useMipmaps)
			GenerateMipMaps(GetMipMapCountFromDimensions());
	}

	stbi_image_free(uncompressed);

	return true;
}

bool Image::iLoadSTBIFP32FromMemory(
	const char* buffer, uint32_t memSize, const bool useMipmaps, memoryAllocationFunc pAllocator, void* pUserData)
{
	// stbi does not generate or load mipmaps. (useMipmaps is ignored)
	if (buffer == 0 || memSize == 0)
		return false;

	int w = 0, h = 0, cmp = 0, requiredCmp = 0;
	stbi_info_from_memory((stbi_uc*)buffer, memSize, &w, &h, &cmp);

	if (w == 0 || h == 0 || cmp == 0)
	{
		return false;
	}

	requiredCmp = cmp;

	if (cmp == 3)
		requiredCmp = 4;

	mWidth = w;
	mHeight = h;
	mDepth = 1;
	mMipMapCount = 1;
	mArrayCount = 1;

	switch (requiredCmp)
	{
		case 1: mFormat = ImageFormat::R32F; break;
		case 2: mFormat = ImageFormat::RG32F; break;
		case 3: mFormat = ImageFormat::RGB32F; break;
		case 4: mFormat = ImageFormat::RGBA32F; break;
	}

	uint64_t memoryRequirement = sizeof(float) * mWidth * mHeight * requiredCmp;

	// stbi does not generate or load mipmaps. (useMipmaps is ignored)
	if (buffer == 0 || memSize == 0)
		return false;

	float* uncompressed = stbi_loadf_from_memory((stbi_uc*)buffer, (int)memSize, &w, &h, &cmp, requiredCmp);

	if (uncompressed == 0)
		return false;

	if (pAllocator)
	{
		//uint32_t mipMapCount = GetMipMapCountFromDimensions(); //unused
		pData = (stbi_uc*)pAllocator(this, memoryRequirement, pUserData);
		if (pData == NULL)
		{
			LOGERRORF("Allocator returned NULL", mLoadFileName.c_str());
			return false;
		}

		memcpy(pData, uncompressed, memoryRequirement);

		mOwnsMemory = false;
	}
	else
	{
		pData = (unsigned char*)conf_malloc(memoryRequirement);
		memcpy(pData, uncompressed, memoryRequirement);
		if (useMipmaps)
			GenerateMipMaps(GetMipMapCountFromDimensions());
	}

	stbi_image_free(uncompressed);

	return true;
}

bool Image::iLoadEXRFP32FromMemory(
	const char* buffer, uint32_t memSize, const bool useMipmaps, memoryAllocationFunc pAllocator, void* pUserData)
{
	UNREF_PARAM(useMipmaps);
	UNREF_PARAM(pAllocator);
	UNREF_PARAM(pUserData);
	// tinyexr does not generate or load mipmaps. (useMipmaps is ignored)
	if (buffer == 0 || memSize == 0)
		return false;

	const char* err;

	EXRImage exrImage;
	InitEXRImage(&exrImage);

	int ret = ParseMultiChannelEXRHeaderFromMemory(&exrImage, (const unsigned char*)buffer, &err);
	if (ret != 0)
	{
		LOGERRORF("Parse EXR err: %s\n", err);
		return false;
	}

	// Read HALF image as FLOAT.
	for (int i = 0; i < exrImage.num_channels; i++)
	{
		if (exrImage.pixel_types[i] == TINYEXR_PIXELTYPE_HALF)
			exrImage.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
	}

	ret = LoadMultiChannelEXRFromMemory(&exrImage, (const unsigned char*)buffer, &err);
	if (ret != 0)
	{
		LOGERRORF("Load EXR err: %s\n", err);
		return false;
	}

	// RGBA
	int idxR = -1;
	int idxG = -1;
	int idxB = -1;
	int idxA = -1;
	int numChannels = 0;
	for (int c = 0; c < exrImage.num_channels; c++)
	{
		if (strcmp(exrImage.channel_names[c], "R") == 0)
		{
			idxR = c;
			numChannels++;
		}
		else if (strcmp(exrImage.channel_names[c], "G") == 0)
		{
			idxG = c;
			numChannels++;
		}
		else if (strcmp(exrImage.channel_names[c], "B") == 0)
		{
			idxB = c;
			numChannels++;
		}
		else if (strcmp(exrImage.channel_names[c], "A") == 0)
		{
			idxA = c;
			numChannels++;
		}
	}

	int idxChannels[] = { -1, -1, -1, -1 };
	int idxCur = 0;
	if (idxR != -1)
		idxChannels[idxCur++] = idxR;
	if (idxG != -1)
		idxChannels[idxCur++] = idxG;
	if (idxB != -1)
		idxChannels[idxCur++] = idxB;
	if (idxA != -1)
		idxChannels[idxCur++] = idxA;

	unsigned int* out = (unsigned int*)conf_malloc(numChannels * sizeof(float) * exrImage.width * exrImage.height);
	for (int i = 0; i < exrImage.width * exrImage.height; i++)
		for (int chn = 0; chn < numChannels; chn++)
			out[i * numChannels + chn] = ((unsigned int**)exrImage.images)[idxChannels[chn]][i];

	pData = (unsigned char*)out;

	mWidth = exrImage.width;
	mHeight = exrImage.height;
	mDepth = 1;
	mMipMapCount = 1;
	mArrayCount = 1;

	switch (numChannels)
	{
		case 1: mFormat = ImageFormat::R32F; break;
		case 2: mFormat = ImageFormat::RG32F; break;
		// RGB32F format not supported on all APIs so convert to RGBA32F
		case 3:
			mFormat = ImageFormat::RGB32F;
			Convert(ImageFormat::RGBA32F);
			break;
		case 4: mFormat = ImageFormat::RGBA32F; break;
	}

	return true;
}

#if defined(ORBIS)

// loads GNF header from memory
static GnfError iLoadGnfHeaderFromMemory(struct sce::Gnf::Header* outHeader, MemoryBuffer* mp)
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

	MemoryBuffer m1(memory, memSize);

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
	MemoryBuffer m2(mp, memSize - sizeof(sce::Gnf::Header));

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
		LOGERRORF("Couldn't find the data format of the texture");
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
	MemoryBuffer m3(memPoint, memSize - (sizeof(sce::Gnf::Header) + header.m_contentsSize + getTexturePixelsByteOffset(gnfContents, 0)));

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

// Image loading
// struct of table for file format to loading function
struct ImageLoaderDefinition
{
	tinystl::string            Extension;
	Image::ImageLoaderFunction Loader;
};

static tinystl::vector<ImageLoaderDefinition> gImageLoaders;

struct StaticImageLoader
{
	StaticImageLoader()
	{
#if !defined(NO_STBI)
		gImageLoaders.push_back({ ".hdr", &Image::iLoadSTBIFP32FromMemory });
		gImageLoaders.push_back({ ".jpg", &Image::iLoadSTBIFromMemory });
		gImageLoaders.push_back({ ".jpeg", &Image::iLoadSTBIFromMemory });
		gImageLoaders.push_back({ ".png", &Image::iLoadSTBIFromMemory });
		gImageLoaders.push_back({ ".tga", &Image::iLoadSTBIFromMemory });
		gImageLoaders.push_back({ ".bmp", &Image::iLoadSTBIFromMemory });
		gImageLoaders.push_back({ ".gif", &Image::iLoadSTBIFromMemory });
		gImageLoaders.push_back({ ".psd", &Image::iLoadSTBIFromMemory });
		gImageLoaders.push_back({ ".pic", &Image::iLoadSTBIFromMemory });
		gImageLoaders.push_back({ ".ppm", &Image::iLoadSTBIFromMemory });
#endif
//#if !defined(TARGET_IOS)
		gImageLoaders.push_back({ ".dds", &Image::iLoadDDSFromMemory });
//#endif
		gImageLoaders.push_back({ ".pvr", &Image::iLoadPVRFromMemory });
		// #TODO: Add KTX loader
#ifdef _WIN32
		gImageLoaders.push_back({ ".ktx", NULL });
#endif
		gImageLoaders.push_back({ ".exr", &Image::iLoadEXRFP32FromMemory });
#if defined(ORBIS)
		gImageLoaders.push_back({ ".gnf", &Image::iLoadGNFFromMemory });
#endif
	}
} gImageLoaderInst;

void Image::AddImageLoader(const char* pExtension, ImageLoaderFunction pFunc) { gImageLoaders.push_back({ pExtension, pFunc }); }

void Image::loadFromMemoryXY(
	const void* mem, const int topLeftX, const int topLeftY, const int bottomRightX, const int bottomRightY, const int pitch)
{
	if (ImageFormat::IsPlainFormat(getFormat()) == false)
		return;    // unsupported

	int bpp_dest = ImageFormat::GetBytesPerPixel(getFormat());
	int rowOffset_dest = bpp_dest * mWidth;
	int subHeight = bottomRightY - topLeftY;
	int subWidth = bottomRightX - topLeftX;
	int subRowSize = subWidth * ImageFormat::GetBytesPerPixel(getFormat());

	unsigned char* start = pData;
	start = start + topLeftY * rowOffset_dest + topLeftX * bpp_dest;

	unsigned char* from = (unsigned char*)mem;

	for (int i = 0; i < subHeight; i++)
	{
		memcpy(start, from, subRowSize);
		start += rowOffset_dest;
		from += pitch;
	}
}

bool Image::loadFromMemory(
	void const* mem, uint32_t size, bool useMipmaps, char const* extension, memoryAllocationFunc pAllocator, void* pUserData)
{
	// try loading the format
	bool loaded = false;
	for (uint32_t i = 0; i < (uint32_t)gImageLoaders.size(); ++i)
	{
		ImageLoaderDefinition const& def = gImageLoaders[i];
		if (stricmp(extension, def.Extension) == 0)
		{
			loaded = (this->*(def.Loader))((char const*)mem, size, useMipmaps, pAllocator, pUserData);
			break;
		}
	}
	return loaded;
}

bool Image::loadImage(const char* fileName, bool useMipmaps, memoryAllocationFunc pAllocator, void* pUserData, FSRoot root)
{
	// clear current image
	Clear();

	const char* extension = strrchr(fileName, '.');
	if (extension == NULL)
		return false;

	// open file
	File file = {};
	file.Open(fileName, FM_ReadBinary, root);
	if (!file.IsOpen())
	{
		LOGERRORF("\"%s\": Image file not found.", fileName);
		return false;
	}

	// load file into memory
	uint32_t length = file.GetSize();
	if (length == 0)
	{
		//char output[256];
		//sprintf(output, "\"%s\": Image file is empty.", fileName);
		LOGERRORF("\"%s\": Image is an empty file.", fileName);
		file.Close();
		return false;
	}

	// read and close file.
	char* data = (char*)conf_malloc(length * sizeof(char));
	file.Read(data, (unsigned)length);
	file.Close();

	// try loading the format
	bool loaded = false;
	bool support = false;
	for (int i = 0; i < (int)gImageLoaders.size(); i++)
	{
		if (stricmp(extension, gImageLoaders[i].Extension) == 0)
		{
			support = true;
			loaded = (this->*(gImageLoaders[i].Loader))(data, length, useMipmaps, pAllocator, pUserData);
			if (loaded)
			{
				break;
			}
		}
	}
	if (!support)
	{
#if !defined(TARGET_IOS)
		LOGERRORF("Can't load this file format for image  :  %s", fileName);
#else
		// Try fallback with uncompressed textures: TODO: this shouldn't be here
		char* uncompressedFileName = strdup(fileName);
		char* uncompressedExtension = strrchr(uncompressedFileName, '.');
		uncompressedExtension[0] = '.';
		uncompressedExtension[1] = 't';
		uncompressedExtension[2] = 'g';
		uncompressedExtension[3] = 'a';
		uncompressedExtension[4] = '\0';
		loaded = loadImage(uncompressedFileName, useMipmaps, pAllocator, pUserData, root);
		conf_free(uncompressedFileName);
		if (!loaded)
		{
			LOGERRORF("Can't load this file format for image  :  %s", fileName);
		}
#endif
	}
	else
	{
		mLoadFileName = fileName;
	}
	// cleanup the compressed data
	conf_free(data);

	return loaded;
}

bool Image::Convert(const ImageFormat::Enum newFormat)
{
	ubyte* newPixels;
	uint   nPixels = GetNumberOfPixels(0, mMipMapCount) * mArrayCount;

	if (mFormat == ImageFormat::RGBE8 && (newFormat == ImageFormat::RGB32F || newFormat == ImageFormat::RGBA32F))
	{
		newPixels = (ubyte*)conf_malloc(sizeof(ubyte) * GetMipMappedSize(0, mMipMapCount, newFormat) * mArrayCount);
		float* dest = (float*)newPixels;

		bool   writeAlpha = (newFormat == ImageFormat::RGBA32F);
		ubyte* src = pData;
		do
		{
			*((vec3*)dest) = rgbeToRGB(src);
			if (writeAlpha)
			{
				dest[3] = 1.0f;
				dest += 4;
			}
			else
			{
				dest += 3;
			}
			src += 4;
		} while (--nPixels);
	}
	else
	{
		if (!ImageFormat::IsPlainFormat(mFormat) || !(ImageFormat::IsPlainFormat(newFormat) || newFormat == ImageFormat::RGB10A2 ||
													  newFormat == ImageFormat::RGBE8 || newFormat == ImageFormat::RGB9E5))
		{
			LOGERRORF(
				"Image: %s fail to convert from  %s  to  %s", mLoadFileName.c_str(), ImageFormat::GetFormatString(mFormat),
				ImageFormat::GetFormatString(newFormat));
			return false;
		}
		if (mFormat == newFormat)
			return true;

		ubyte* src = pData;
		ubyte* dest = newPixels = (ubyte*)conf_malloc(sizeof(ubyte) * GetMipMappedSize(0, mMipMapCount, newFormat) * mArrayCount);

		if (mFormat == ImageFormat::RGB8 && newFormat == ImageFormat::RGBA8)
		{
			// Fast path for RGB->RGBA8
			do
			{
				dest[0] = src[0];
				dest[1] = src[1];
				dest[2] = src[2];
				dest[3] = 255;
				dest += 4;
				src += 3;
			} while (--nPixels);
		}
		else if (mFormat == ImageFormat::RGBA8 && newFormat == ImageFormat::BGRA8)
		{
			// Fast path for RGBA8->BGRA8 (just swizzle)
			do
			{
				dest[0] = src[2];
				dest[1] = src[1];
				dest[2] = src[0];
				dest[3] = src[3];
				dest += 4;
				src += 4;
			} while (--nPixels);
		}
		else
		{
			int srcSize = ImageFormat::GetBytesPerPixel(mFormat);
			int nSrcChannels = ImageFormat::GetChannelCount(mFormat);

			int destSize = ImageFormat::GetBytesPerPixel(newFormat);
			int nDestChannels = ImageFormat::GetChannelCount(newFormat);

			do
			{
				float rgba[4];

				if (ImageFormat::IsFloatFormat(mFormat))
				{
					if (mFormat <= ImageFormat::RGBA16F)
					{
						for (int i = 0; i < nSrcChannels; i++)
							rgba[i] = ((half*)src)[i];
					}
					else
					{
						for (int i = 0; i < nSrcChannels; i++)
							rgba[i] = ((float*)src)[i];
					}
				}
				else if (mFormat >= ImageFormat::I16 && mFormat <= ImageFormat::RGBA16)
				{
					for (int i = 0; i < nSrcChannels; i++)
						rgba[i] = ((ushort*)src)[i] * (1.0f / 65535.0f);
				}
				else
				{
					for (int i = 0; i < nSrcChannels; i++)
						rgba[i] = src[i] * (1.0f / 255.0f);
				}
				if (nSrcChannels < 4)
					rgba[3] = 1.0f;
				if (nSrcChannels == 1)
					rgba[2] = rgba[1] = rgba[0];

				if (nDestChannels == 1)
					rgba[0] = 0.30f * rgba[0] + 0.59f * rgba[1] + 0.11f * rgba[2];

				if (ImageFormat::IsFloatFormat(newFormat))
				{
					if (newFormat <= ImageFormat::RGBA32F)
					{
						if (newFormat <= ImageFormat::RGBA16F)
						{
							for (int i = 0; i < nDestChannels; i++)
								((half*)dest)[i] = rgba[i];
						}
						else
						{
							for (int i = 0; i < nDestChannels; i++)
								((float*)dest)[i] = rgba[i];
						}
					}
					else
					{
						if (newFormat == ImageFormat::RGBE8)
						{
							*(uint32*)dest = rgbToRGBE8(vec3(rgba[0], rgba[1], rgba[2]));
						}
						else
						{
							*(uint32*)dest = rgbToRGB9E5(vec3(rgba[0], rgba[1], rgba[2]));
						}
					}
				}
				else if (newFormat >= ImageFormat::I16 && newFormat <= ImageFormat::RGBA16)
				{
					for (int i = 0; i < nDestChannels; i++)
						((ushort*)dest)[i] = (ushort)(65535 * saturate(rgba[i]) + 0.5f);
				}
				else if (/*isPackedFormat(newFormat)*/ newFormat == ImageFormat::RGB10A2)
				{
					*(uint*)dest = (uint(1023.0f * saturate(rgba[0]) + 0.5f) << 22) | (uint(1023.0f * saturate(rgba[1]) + 0.5f) << 12) |
								   (uint(1023.0f * saturate(rgba[2]) + 0.5f) << 2) | (uint(3.0f * saturate(rgba[3]) + 0.5f));
				}
				else
				{
					for (int i = 0; i < nDestChannels; i++)
						dest[i] = (unsigned char)(255 * saturate(rgba[i]) + 0.5f);
				}

				src += srcSize;
				dest += destSize;
			} while (--nPixels);
		}
	}
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
	if (ImageFormat::IsCompressedFormat(mFormat))
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

	int nChannels = ImageFormat::GetChannelCount(mFormat);

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

			int srcSize = GetMipMappedSize(level - 1, 1) / n;
			int dstSize = GetMipMappedSize(level, 1) / n;

			for (int i = 0; i < n; i++)
			{
				if (ImageFormat::IsPlainFormat(mFormat))
				{
					if (ImageFormat::IsFloatFormat(mFormat))
					{
						buildMipMap((float*)dst, (float*)src, w, h, d, nChannels);
					}
					else if (mFormat >= ImageFormat::I16)
					{
						buildMipMap((ushort*)dst, (ushort*)src, w, h, d, nChannels);
					}
					else
					{
						buildMipMap(dst, src, w, h, d, nChannels);
					}
				}
				src += srcSize;
				dst += dstSize;
			}
		}
	}

	return true;
}

bool Image::iSwap(const int c0, const int c1)
{
	if (!ImageFormat::IsPlainFormat(mFormat))
		return false;

	unsigned int nPixels = GetNumberOfPixels(0, mMipMapCount) * mArrayCount;
	unsigned int nChannels = ImageFormat::GetChannelCount(mFormat);

	if (mFormat <= ImageFormat::RGBA8)
	{
		swapPixelChannels((uint8_t*)pData, nPixels, nChannels, c0, c1);
	}
	else if (mFormat <= ImageFormat::RGBA16F)
	{
		swapPixelChannels((uint16_t*)pData, nPixels, nChannels, c0, c1);
	}
	else
	{
		swapPixelChannels((float*)pData, nPixels, nChannels, c0, c1);
	}

	return true;
}

// -- IMAGE SAVING --

bool Image::iSaveDDS(const char* fileName)
{
	DDSHeader     header;
	DDSHeaderDX10 headerDX10;
	memset(&header, 0, sizeof(header));
	memset(&headerDX10, 0, sizeof(headerDX10));

	header.mDWMagic = MAKE_CHAR4('D', 'D', 'S', ' ');
	header.mDWSize = 124;

	header.mDWWidth = mWidth;
	header.mDWHeight = mHeight;
	header.mDWDepth = (mDepth > 1) ? mDepth : 0;
	header.mDWPitchOrLinearSize = 0;
	header.mDWMipMapCount = (mMipMapCount > 1) ? mMipMapCount : 0;
	header.mPixelFormat.mDWSize = 32;

	header.mDWFlags =
		DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | (mMipMapCount > 1 ? DDSD_MIPMAPCOUNT : 0) | (mDepth > 1 ? DDSD_DEPTH : 0);

	int nChannels = ImageFormat::GetChannelCount(mFormat);

	if (mFormat == ImageFormat::RGB10A2 || mFormat <= ImageFormat::I16)
	{
		if (mFormat <= ImageFormat::RGBA8)
		{
			header.mPixelFormat.mDWRGBBitCount = 8 * nChannels;
			header.mPixelFormat.mDWRGBAlphaBitMask = (nChannels == 4) ? 0xFF000000 : (nChannels == 2) ? 0xFF00 : 0;
			header.mPixelFormat.mDWRBitMask = (nChannels > 2) ? 0x00FF0000 : 0xFF;
			header.mPixelFormat.mDWGBitMask = (nChannels > 1) ? 0x0000FF00 : 0;
			header.mPixelFormat.mDWBBitMask = (nChannels > 1) ? 0x000000FF : 0;
		}
		else if (mFormat == ImageFormat::I16)
		{
			header.mPixelFormat.mDWRGBBitCount = 16;
			header.mPixelFormat.mDWRBitMask = 0xFFFF;
		}
		else
		{
			header.mPixelFormat.mDWRGBBitCount = 32;
			header.mPixelFormat.mDWRGBAlphaBitMask = 0xC0000000;
			header.mPixelFormat.mDWRBitMask = 0x3FF00000;
			header.mPixelFormat.mDWGBitMask = 0x000FFC00;
			header.mPixelFormat.mDWBBitMask = 0x000003FF;
		}
		header.mPixelFormat.mDWFlags = ((nChannels < 3) ? 0x00020000 : DDPF_RGB) | ((nChannels & 1) ? 0 : DDPF_ALPHAPIXELS);
	}
	else
	{
		header.mPixelFormat.mDWFlags = DDPF_FOURCC;

		switch (mFormat)
		{
			case ImageFormat::RG16: header.mPixelFormat.mDWFourCC = 34; break;
			case ImageFormat::RGBA16: header.mPixelFormat.mDWFourCC = 36; break;
			case ImageFormat::R16F: header.mPixelFormat.mDWFourCC = 111; break;
			case ImageFormat::RG16F: header.mPixelFormat.mDWFourCC = 112; break;
			case ImageFormat::RGBA16F: header.mPixelFormat.mDWFourCC = 113; break;
			case ImageFormat::R32F: header.mPixelFormat.mDWFourCC = 114; break;
			case ImageFormat::RG32F: header.mPixelFormat.mDWFourCC = 115; break;
			case ImageFormat::RGBA32F: header.mPixelFormat.mDWFourCC = 116; break;
			case ImageFormat::DXT1: header.mPixelFormat.mDWFourCC = MAKE_CHAR4('D', 'X', 'T', '1'); break;
			case ImageFormat::DXT3: header.mPixelFormat.mDWFourCC = MAKE_CHAR4('D', 'X', 'T', '3'); break;
			case ImageFormat::DXT5: header.mPixelFormat.mDWFourCC = MAKE_CHAR4('D', 'X', 'T', '5'); break;
			case ImageFormat::ATI1N: header.mPixelFormat.mDWFourCC = MAKE_CHAR4('A', 'T', 'I', '1'); break;
			case ImageFormat::ATI2N: header.mPixelFormat.mDWFourCC = MAKE_CHAR4('A', 'T', 'I', '2'); break;
			default:
				header.mPixelFormat.mDWFourCC = MAKE_CHAR4('D', 'X', '1', '0');
				headerDX10.mArraySize = 1;
				headerDX10.mDXGIFormat = (mDepth == 0) ? D3D10_RESOURCE_MISC_TEXTURECUBE : 0;
				if (Is1D())
					headerDX10.mResourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE1D;
				else if (Is2D())
					headerDX10.mResourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
				else if (Is3D())
					headerDX10.mResourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE3D;

				switch (mFormat)
				{
					case ImageFormat::RGB32F: headerDX10.mDXGIFormat = 6; break;
					case ImageFormat::RGB9E5: headerDX10.mDXGIFormat = 67; break;
					case ImageFormat::RG11B10F: headerDX10.mDXGIFormat = 26; break;
					default: return false;
				}
		}
	}
	// header.

	header.mCaps.mDWCaps1 =
		DDSCAPS_TEXTURE | (mMipMapCount > 1 ? DDSCAPS_MIPMAP | DDSCAPS_COMPLEX : 0) | (mDepth != 1 ? DDSCAPS_COMPLEX : 0);
	header.mCaps.mDWCaps2 = (mDepth > 1) ? DDSCAPS2_VOLUME : (mDepth == 0) ? DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALL_FACES : 0;
	header.mCaps.mReserved[0] = 0;
	header.mCaps.mReserved[1] = 0;
	header.mDWReserved2 = 0;

	File file;
	if (!file.Open(fileName, FileMode::FM_WriteBinary, FSR_Textures))
		return false;

	file.Write(&header, sizeof(header));

	if (headerDX10.mDXGIFormat)
		file.Write(&headerDX10, sizeof(headerDX10) * 1);

	int size = GetMipMappedSize(0, mMipMapCount);

	// RGB to BGR
	if (mFormat == ImageFormat::RGB8 || mFormat == ImageFormat::RGBA8)
		swapPixelChannels(pData, size / nChannels, nChannels, 0, 2);

	if (IsCube())
	{
		for (int face = 0; face < 6; face++)
		{
			for (uint mipMapLevel = 0; mipMapLevel < mMipMapCount; mipMapLevel++)
			{
				int    faceSize = GetMipMappedSize(mipMapLevel, 1) / 6;
				ubyte* src = GetPixels(mipMapLevel) + face * faceSize;
				file.Write(src, faceSize);
			}
		}
	}
	else
	{
		file.Write(pData, size);
	}
	file.Close();

	// Restore to RGB
	if (mFormat == ImageFormat::RGB8 || mFormat == ImageFormat::RGBA8)
		swapPixelChannels(pData, size / nChannels, nChannels, 0, 2);

	return true;
}

bool convertAndSaveImage(const Image& image, bool (Image::*saverFunction)(const char*), const char* fileName)
{
	bool  bSaveImageSuccess = false;
	Image imgCopy(image);
	imgCopy.Uncompress();
	if (imgCopy.Convert(ImageFormat::RGBA8))
	{
		bSaveImageSuccess = (imgCopy.*saverFunction)(fileName);
	}

	imgCopy.Destroy();
	return bSaveImageSuccess;
}

bool Image::iSaveTGA(const char* fileName)
{
	switch (mFormat)
	{
		case ImageFormat::R8: return 0 != stbi_write_tga(fileName, mWidth, mHeight, 1, pData); break;
		case ImageFormat::RG8: return 0 != stbi_write_tga(fileName, mWidth, mHeight, 2, pData); break;
		case ImageFormat::RGB8: return 0 != stbi_write_tga(fileName, mWidth, mHeight, 3, pData); break;
		case ImageFormat::RGBA8: return 0 != stbi_write_tga(fileName, mWidth, mHeight, 4, pData); break;
		default:
		{
			// uncompress/convert and try again
			return convertAndSaveImage(*this, &Image::iSaveTGA, fileName);
		}
	}

	//return false; //Unreachable
}

bool Image::iSaveBMP(const char* fileName)
{
	switch (mFormat)
	{
		case ImageFormat::R8: stbi_write_bmp(fileName, mWidth, mHeight, 1, pData); break;
		case ImageFormat::RG8: stbi_write_bmp(fileName, mWidth, mHeight, 2, pData); break;
		case ImageFormat::RGB8: stbi_write_bmp(fileName, mWidth, mHeight, 3, pData); break;
		case ImageFormat::RGBA8: stbi_write_bmp(fileName, mWidth, mHeight, 4, pData); break;
		default:
		{
			// uncompress/convert and try again
			return convertAndSaveImage(*this, &Image::iSaveBMP, fileName);
		}
	}
	return true;
}

bool Image::iSavePNG(const char* fileName)
{
	switch (mFormat)
	{
		case ImageFormat::R8: stbi_write_png(fileName, mWidth, mHeight, 1, pData, 0); break;
		case ImageFormat::RG8: stbi_write_png(fileName, mWidth, mHeight, 2, pData, 0); break;
		case ImageFormat::RGB8: stbi_write_png(fileName, mWidth, mHeight, 3, pData, 0); break;
		case ImageFormat::RGBA8: stbi_write_png(fileName, mWidth, mHeight, 4, pData, 0); break;
		default:
		{
			// uncompress/convert and try again
			return convertAndSaveImage(*this, &Image::iSavePNG, fileName);
		}
	}

	return true;
}

bool Image::iSaveHDR(const char* fileName)
{
	switch (mFormat)
	{
		case ImageFormat::R32F: stbi_write_hdr(fileName, mWidth, mHeight, 1, (float*)pData); break;
		case ImageFormat::RG32F: stbi_write_hdr(fileName, mWidth, mHeight, 2, (float*)pData); break;
		case ImageFormat::RGB32F: stbi_write_hdr(fileName, mWidth, mHeight, 3, (float*)pData); break;
		case ImageFormat::RGBA32F: stbi_write_hdr(fileName, mWidth, mHeight, 4, (float*)pData); break;
		default:
		{
			// uncompress/convert and try again
			return convertAndSaveImage(*this, &Image::iSaveHDR, fileName);
		}
	}

	return true;
}

bool Image::iSaveJPG(const char* fileName)
{
	switch (mFormat)
	{
		case ImageFormat::R8: stbi_write_jpg(fileName, mWidth, mHeight, 1, pData, 0); break;
		case ImageFormat::RG8: stbi_write_jpg(fileName, mWidth, mHeight, 2, pData, 0); break;
		case ImageFormat::RGB8: stbi_write_jpg(fileName, mWidth, mHeight, 3, pData, 0); break;
		case ImageFormat::RGBA8: stbi_write_jpg(fileName, mWidth, mHeight, 4, pData, 0); break;
		default:
		{
			// uncompress/convert and try again
			return convertAndSaveImage(*this, &Image::iSaveJPG, fileName);
		}
	}

	return true;
}

struct ImageSaverDefinition
{
	typedef bool (Image::*ImageSaverFunction)(const char*);
	const char*        Extension;
	ImageSaverFunction Loader;
};

static ImageSaverDefinition gImageSavers[] = {
#if !defined(NO_STBI)
	{ ".bmp", &Image::iSaveBMP }, { ".hdr", &Image::iSaveHDR }, { ".png", &Image::iSavePNG },
	{ ".tga", &Image::iSaveTGA }, { ".jpg", &Image::iSaveJPG },
#endif
	{ ".dds", &Image::iSaveDDS }
};

bool Image::SaveImage(const char* fileName)
{
	const char* extension = strrchr(fileName, '.');
	bool        support = false;
	;
	for (int i = 0; i < sizeof(gImageSavers) / sizeof(gImageSavers[0]); i++)
	{
		if (stricmp(extension, gImageSavers[i].Extension) == 0)
		{
			support = true;
			return (this->*gImageSavers[i].Loader)(fileName);
		}
	}
	if (!support)
	{
		LOGERRORF("Can't save this file format for image  :  %s", fileName);
	}

	return false;
}

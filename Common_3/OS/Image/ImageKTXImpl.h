/*
* Copyright (c) 2018 Confetti Interactive Inc.
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

//=============================================================================
// FILE:        os_file.h
//
// DESCRIPTION: includes generic file handling class
//
// AUTHOR:      QUALCOMM
//
//                  Copyright (c) 2011 QUALCOMM Incorporated.
//                            All Rights Reserved.
//                         QUALCOMM Proprietary/GTDR
//=============================================================================

#include "Image.h"
#include "stdio.h"
#include <cstring>
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IFileSystem.h"

#define KTX_IDENTIFIER_REF  { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A }
#define KTX_ENDIAN_REF      (0x04030201)
#define KTX_ENDIAN_REF_REV  (0x01020304)

//	Defines from gl2ext header
#ifndef GL_IMG_texture_compression_pvrtc
#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG                      0x8C00
#define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG                      0x8C01
#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG                     0x8C02
#define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG                     0x8C03
#endif

#ifndef GL_OES_compressed_ETC1_RGB8_texture
#define GL_ETC1_RGB8_OES                                        0x8D64
#endif

#ifndef GL_AMD_compressed_3DC_texture
#define GL_3DC_X_AMD                                            0x87F9
#define GL_3DC_XY_AMD                                           0x87FA
#endif

#ifndef GL_AMD_compressed_ATC_texture
#define GL_ATC_RGB_AMD                                          0x8C92
#define GL_ATC_RGBA_EXPLICIT_ALPHA_AMD                          0x8C93
#define GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD                      0x87EE
#endif

#ifndef GL_RGBA
#define GL_RGBA                           0x1908
#endif

#ifndef GL_UNSIGNED_BYTE
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_FLOAT                          0x1406
#endif




#ifndef GL_OES_texture_half_float
#define GL_HALF_FLOAT_OES                                       0x8D61
#endif

struct KTXHeader
{
	uint8  identifier[12];
	uint32 endianness;
	uint32 glType;
	uint32 glTypeSize;
	uint32 glFormat;
	uint32 glInternalFormat;
	uint32 glBaseInternalFormat;
	uint32 pixelWidth;
	uint32 pixelHeight;
	uint32 pixelDepth;
	uint32 numberOfArrayElements;
	uint32 numberOfFaces;
	uint32 numberOfMipmapLevels;
	uint32 bytesOfKeyValueData;
};
/*
static void SwapEndian16(uint16* pData16, int count)
{
	int i;
	for (i = 0; i < count; ++i)
	{
		uint16 x = *pData16;
		*pData16++ = (x << 8) | (x >> 8);
	}
}
*/
static void SwapEndian32(uint32* pData32, int count)
{
	int i;
	for (i = 0; i < count; ++i)
	{
		uint32 x = *pData32;
		*pData32++ = (x << 24) | ((x & 0xFF00) << 8) | ((x & 0xFF0000) >> 8) | (x >> 24);
	}
}

bool Image::iLoadKTXFromMemory(const char *memory, uint32_t bytes, const bool useMipmaps, memoryAllocationFunc pAllocator, void* pUserData)
{
  UNREF_PARAM(pUserData);
  UNREF_PARAM(pAllocator);
	uint8 identifier_reference[12] = KTX_IDENTIFIER_REF;
	KTXHeader header;

	if (memory == nullptr || bytes == 0)
		return false;
	MemoryBuffer file((char*)memory, (unsigned)bytes);

	//fread(&header, sizeof(header), 1, file);
	file.Read(&header, 64 * 1);
	//MemFopen::FileRead(&header, 64, 1, file);
	if (memcmp(header.identifier, identifier_reference, 12))
	{
		return false;
	}

	if (header.endianness == KTX_ENDIAN_REF_REV)
	{
#ifdef	_XBOX
		/* Convert endianness of header fields if necessary */
		SwapEndian32(&header.glType, 12);
		if (header.glTypeSize != 0 &&
			header.glTypeSize != 1 &&
			header.glTypeSize != 2 &&
			header.glTypeSize != 4)
		{
			//	Not sure how this should work for XBox and/or other platforms
			// Only 8, 16, and 32-bit types supported so far
			// 0 stands for compressed
			return false;
		}
#else	//	_XBOX
		return false;
#endif	//	_XBOX
	}
	else if (header.endianness != KTX_ENDIAN_REF)
	{
		return false;
	}

	mWidth = header.pixelWidth;
	mHeight = (header.pixelHeight == 0) ? 1 : header.pixelHeight;
	mDepth = (header.numberOfFaces == 6) ? 0 : (header.pixelDepth == 0) ? 1 : header.pixelDepth;

	mMipMapCount = (useMipmaps == false || (header.numberOfMipmapLevels == 0)) ? 1 : header.numberOfMipmapLevels;
	mArrayCount = (header.numberOfArrayElements == 0) ? 1 : header.numberOfArrayElements;

	file.Seek(header.bytesOfKeyValueData);
	//MemFopen::FileSeek(file, header.bytesOfKeyValueData, SEEK_CUR);

	switch (header.glBaseInternalFormat)
	{
	case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
		mFormat = ImageFormat::PVR_4BPP;
		break;
	case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
		mFormat = ImageFormat::PVR_2BPP;
		break;
	case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
		mFormat = ImageFormat::PVR_4BPPA;
		break;
	case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
		mFormat = ImageFormat::PVR_2BPPA;
		break;

	case GL_ETC1_RGB8_OES:
		mFormat = ImageFormat::ETC1;
		break;

	case GL_3DC_X_AMD:
		mFormat = ImageFormat::ATI1N;
		break;

	case GL_3DC_XY_AMD:
		mFormat = ImageFormat::ATI2N;
		break;

	case GL_ATC_RGB_AMD:
		mFormat = ImageFormat::ATC;
		break;

	case GL_ATC_RGBA_EXPLICIT_ALPHA_AMD:
		mFormat = ImageFormat::ATCA;
		break;

	case GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD:
		mFormat = ImageFormat::ATCI;
		break;

		//	Uncompressed formats go here
	case GL_RGBA:
		switch (header.glType)
		{
		case GL_UNSIGNED_BYTE:
			mFormat = ImageFormat::RGBA8;
			break;

		case GL_UNSIGNED_SHORT:
			mFormat = ImageFormat::RGBA16;
			break;

		case GL_HALF_FLOAT_OES:
			mFormat = ImageFormat::RGBA16F;
			break;

		case GL_FLOAT:
			mFormat = ImageFormat::RGBA32F;
			break;

		default:
			return false;
		}
		break;

	default:
		return false;
	}

	int size = GetMipMappedSize(0, mMipMapCount);
	pData = new unsigned char[size];

	uint32 mipLevelDataSize;

	if (IsCube()) {
		for (int face = 0; face < 6; face++) {
			for (uint mipMapLevel = 0; mipMapLevel < mMipMapCount; mipMapLevel++)
			{
				int faceSize = GetMipMappedSize(mipMapLevel, 1) / 6;
				unsigned char *src = GetPixels(mipMapLevel) + face * faceSize;

				file.Read(&mipLevelDataSize, sizeof(mipLevelDataSize));
				//MemFopen::FileRead(&mipLevelDataSize, 1, sizeof(mipLevelDataSize), file);
				if (header.endianness == KTX_ENDIAN_REF_REV) { SwapEndian32(&mipLevelDataSize, 1); }
				ASSERT((unsigned int)faceSize >= mipLevelDataSize);

				file.Read(src, mipLevelDataSize);
				//MemFopen::FileRead(src, 1, mipLevelDataSize, file);
			}

			// skip mipmaps if needed
			if (useMipmaps == false && header.numberOfMipmapLevels > 1)
			{
				for (uint mipMapLevel = 1; mipMapLevel < mMipMapCount; ++mipMapLevel)
				{
					file.Read(&mipLevelDataSize, sizeof(mipLevelDataSize));
					//MemFopen::FileRead(&mipLevelDataSize, 1, sizeof(mipLevelDataSize), file);
					if (header.endianness == KTX_ENDIAN_REF_REV) { SwapEndian32(&mipLevelDataSize, 1); }

					file.Seek(mipLevelDataSize);
					//MemFopen::FileSeek(file, mipLevelDataSize, SEEK_CUR);
				}
			}
		}
	}
	else {
		for (uint mipMapLevel = 0; mipMapLevel < mMipMapCount; ++mipMapLevel)
		{
			file.Read(&mipLevelDataSize, sizeof(mipLevelDataSize));
			//MemFopen::FileRead(&mipLevelDataSize, 1, sizeof(mipLevelDataSize), file);
			if (header.endianness == KTX_ENDIAN_REF_REV) { SwapEndian32(&mipLevelDataSize, 1); }
			unsigned char *src = GetPixels(mipMapLevel);

			file.Read(src, mipLevelDataSize);
			//MemFopen::FileRead(src, 1, mipLevelDataSize, file);
		}
	}

	if (header.endianness == KTX_ENDIAN_REF_REV)
	{
		if (header.glBaseInternalFormat == GL_RGBA &&
			header.glType == GL_UNSIGNED_BYTE)
		{
			SwapEndian32((uint32*)pData, size / 4);
		}
		else
		{
			ASSERT(!"Unsupported format!");
			return false;
		}
	}

#if 0
	// Perform endianness conversion on texture data
	if (header.endianness == KTX_ENDIAN_REF_REV && header.glTypeSize == 2)
	{
		SwapEndian16((uint16*)pixels, size / 2);
	}
	else if (header.endianness == KTX_ENDIAN_REF_REV && header.glTypeSize == 4)
	{
		SwapEndian32((uint32*)pixels, size / 4);
	}

	if ((format == ImageFormat::FORMAT_RGB8 || format == ImageFormat::FORMAT_RGBA8) && header.ddpfPixelFormat.dwBBitMask == 0xFF) {
		int nChannels = getChannelCount(format);
		swapChannels(pixels, size / nChannels, nChannels, 0, 2);
	}
#endif

	return true;
}
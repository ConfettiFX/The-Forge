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
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../../ThirdParty/OpenSource/Nothings/stb_image_resize.h"
#include "../Interfaces/IMemory.h"

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

// KTX Container Data
typedef enum KTXInternalFormat
{
	KTXInternalFormat_RGB_UNORM = 0x1907,			//GL_RGB
	KTXInternalFormat_BGR_UNORM = 0x80E0,		//GL_BGR
	KTXInternalFormat_RGBA_UNORM = 0x1908,		//GL_RGBA
	KTXInternalFormat_BGRA_UNORM = 0x80E1,		//GL_BGRA
	KTXInternalFormat_BGRA8_UNORM = 0x93A1,		//GL_BGRA8_EXT

	// unorm formats
	KTXInternalFormat_R8_UNORM = 0x8229,			//GL_R8
	KTXInternalFormat_RG8_UNORM = 0x822B,		//GL_RG8
	KTXInternalFormat_RGB8_UNORM = 0x8051,		//GL_RGB8
	KTXInternalFormat_RGBA8_UNORM = 0x8058,		//GL_RGBA8

	KTXInternalFormat_R16_UNORM = 0x822A,		//GL_R16
	KTXInternalFormat_RG16_UNORM = 0x822C,		//GL_RG16
	KTXInternalFormat_RGB16_UNORM = 0x8054,		//GL_RGB16
	KTXInternalFormat_RGBA16_UNORM = 0x805B,		//GL_RGBA16

	KTXInternalFormat_RGB10A2_UNORM = 0x8059,	//GL_RGB10_A2
	KTXInternalFormat_RGB10A2_SNORM_EXT = 0xFFFC,

	// snorm formats
	KTXInternalFormat_R8_SNORM = 0x8F94,			//GL_R8_SNORM
	KTXInternalFormat_RG8_SNORM = 0x8F95,		//GL_RG8_SNORM
	KTXInternalFormat_RGB8_SNORM = 0x8F96,		//GL_RGB8_SNORM
	KTXInternalFormat_RGBA8_SNORM = 0x8F97,		//GL_RGBA8_SNORM

	KTXInternalFormat_R16_SNORM = 0x8F98,		//GL_R16_SNORM
	KTXInternalFormat_RG16_SNORM = 0x8F99,		//GL_RG16_SNORM
	KTXInternalFormat_RGB16_SNORM = 0x8F9A,		//GL_RGB16_SNORM
	KTXInternalFormat_RGBA16_SNORM = 0x8F9B,		//GL_RGBA16_SNORM

	// unsigned integer formats
	KTXInternalFormat_R8U = 0x8232,				//GL_R8UI
	KTXInternalFormat_RG8U = 0x8238,				//GL_RG8UI
	KTXInternalFormat_RGB8U = 0x8D7D,			//GL_RGB8UI
	KTXInternalFormat_RGBA8U = 0x8D7C,			//GL_RGBA8UI

	KTXInternalFormat_R16U = 0x8234,				//GL_R16UI
	KTXInternalFormat_RG16U = 0x823A,			//GL_RG16UI
	KTXInternalFormat_RGB16U = 0x8D77,			//GL_RGB16UI
	KTXInternalFormat_RGBA16U = 0x8D76,			//GL_RGBA16UI

	KTXInternalFormat_R32U = 0x8236,				//GL_R32UI
	KTXInternalFormat_RG32U = 0x823C,			//GL_RG32UI
	KTXInternalFormat_RGB32U = 0x8D71,			//GL_RGB32UI
	KTXInternalFormat_RGBA32U = 0x8D70,			//GL_RGBA32UI

	KTXInternalFormat_RGB10A2U = 0x906F,			//GL_RGB10_A2UI
	KTXInternalFormat_RGB10A2I_EXT = 0xFFFB,

	// signed integer formats
	KTXInternalFormat_R8I = 0x8231,				//GL_R8I
	KTXInternalFormat_RG8I = 0x8237,				//GL_RG8I
	KTXInternalFormat_RGB8I = 0x8D8F,			//GL_RGB8I
	KTXInternalFormat_RGBA8I = 0x8D8E,			//GL_RGBA8I

	KTXInternalFormat_R16I = 0x8233,				//GL_R16I
	KTXInternalFormat_RG16I = 0x8239,			//GL_RG16I
	KTXInternalFormat_RGB16I = 0x8D89,			//GL_RGB16I
	KTXInternalFormat_RGBA16I = 0x8D88,			//GL_RGBA16I

	KTXInternalFormat_R32I = 0x8235,				//GL_R32I
	KTXInternalFormat_RG32I = 0x823B,			//GL_RG32I
	KTXInternalFormat_RGB32I = 0x8D83,			//GL_RGB32I
	KTXInternalFormat_RGBA32I = 0x8D82,			//GL_RGBA32I

	// Floating formats
	KTXInternalFormat_R16F = 0x822D,				//GL_R16F
	KTXInternalFormat_RG16F = 0x822F,			//GL_RG16F
	KTXInternalFormat_RGB16F = 0x881B,			//GL_RGB16F
	KTXInternalFormat_RGBA16F = 0x881A,			//GL_RGBA16F

	KTXInternalFormat_R32F = 0x822E,				//GL_R32F
	KTXInternalFormat_RG32F = 0x8230,			//GL_RG32F
	KTXInternalFormat_RGB32F = 0x8815,			//GL_RGB32F
	KTXInternalFormat_RGBA32F = 0x8814,			//GL_RGBA32F

	KTXInternalFormat_R64F_EXT = 0xFFFA,			//GL_R64F
	KTXInternalFormat_RG64F_EXT = 0xFFF9,		//GL_RG64F
	KTXInternalFormat_RGB64F_EXT = 0xFFF8,		//GL_RGB64F
	KTXInternalFormat_RGBA64F_EXT = 0xFFF7,		//GL_RGBA64F

	// sRGB formats
	KTXInternalFormat_SR8 = 0x8FBD,				//GL_SR8_EXT
	KTXInternalFormat_SRG8 = 0x8FBE,				//GL_SRG8_EXT
	KTXInternalFormat_SRGB8 = 0x8C41,			//GL_SRGB8
	KTXInternalFormat_SRGB8_ALPHA8 = 0x8C43,		//GL_SRGB8_ALPHA8

	// Packed formats
	KTXInternalFormat_RGB9E5 = 0x8C3D,			//GL_RGB9_E5
	KTXInternalFormat_RG11B10F = 0x8C3A,			//GL_R11F_G11F_B10F
	KTXInternalFormat_RG3B2 = 0x2A10,			//GL_R3_G3_B2
	KTXInternalFormat_R5G6B5 = 0x8D62,			//GL_RGB565
	KTXInternalFormat_RGB5A1 = 0x8057,			//GL_RGB5_A1
	KTXInternalFormat_RGBA4 = 0x8056,			//GL_RGBA4

	KTXInternalFormat_RG4_EXT = 0xFFFE,

	// Luminance Alpha formats
	KTXInternalFormat_LA4 = 0x8043,				//GL_LUMINANCE4_ALPHA4
	KTXInternalFormat_L8 = 0x8040,				//GL_LUMINANCE8
	KTXInternalFormat_A8 = 0x803C,				//GL_ALPHA8
	KTXInternalFormat_LA8 = 0x8045,				//GL_LUMINANCE8_ALPHA8
	KTXInternalFormat_L16 = 0x8042,				//GL_LUMINANCE16
	KTXInternalFormat_A16 = 0x803E,				//GL_ALPHA16
	KTXInternalFormat_LA16 = 0x8048,				//GL_LUMINANCE16_ALPHA16

	// Depth formats
	KTXInternalFormat_D16 = 0x81A5,				//GL_DEPTH_COMPONENT16
	KTXInternalFormat_D24 = 0x81A6,				//GL_DEPTH_COMPONENT24
	KTXInternalFormat_D16S8_EXT = 0xFFF6,
	KTXInternalFormat_D24S8 = 0x88F0,			//GL_DEPTH24_STENCIL8
	KTXInternalFormat_D32 = 0x81A7,				//GL_DEPTH_COMPONENT32
	KTXInternalFormat_D32F = 0x8CAC,				//GL_DEPTH_COMPONENT32F
	KTXInternalFormat_D32FS8X24 = 0x8CAD,		//GL_DEPTH32F_STENCIL8
	KTXInternalFormat_S8_EXT = 0x8D48,			//GL_STENCIL_INDEX8

	// Compressed formats
	KTXInternalFormat_RGB_DXT1 = 0x83F0,						//GL_COMPRESSED_RGB_S3TC_DXT1_EXT
	KTXInternalFormat_RGBA_DXT1 = 0x83F1,					//GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
	KTXInternalFormat_RGBA_DXT3 = 0x83F2,					//GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
	KTXInternalFormat_RGBA_DXT5 = 0x83F3,					//GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
	KTXInternalFormat_R_ATI1N_UNORM = 0x8DBB,				//GL_COMPRESSED_RED_RGTC1
	KTXInternalFormat_R_ATI1N_SNORM = 0x8DBC,				//GL_COMPRESSED_SIGNED_RED_RGTC1
	KTXInternalFormat_RG_ATI2N_UNORM = 0x8DBD,				//GL_COMPRESSED_RG_RGTC2
	KTXInternalFormat_RG_ATI2N_SNORM = 0x8DBE,				//GL_COMPRESSED_SIGNED_RG_RGTC2
	KTXInternalFormat_RGB_BP_UNSIGNED_FLOAT = 0x8E8F,		//GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT
	KTXInternalFormat_RGB_BP_SIGNED_FLOAT = 0x8E8E,			//GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT
	KTXInternalFormat_RGB_BP_UNORM = 0x8E8C,					//GL_COMPRESSED_RGBA_BPTC_UNORM
	KTXInternalFormat_RGB_PVRTC_4BPPV1 = 0x8C00,				//GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG
	KTXInternalFormat_RGB_PVRTC_2BPPV1 = 0x8C01,				//GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG
	KTXInternalFormat_RGBA_PVRTC_4BPPV1 = 0x8C02,			//GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG
	KTXInternalFormat_RGBA_PVRTC_2BPPV1 = 0x8C03,			//GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG
	KTXInternalFormat_RGBA_PVRTC_4BPPV2 = 0x9137,			//GL_COMPRESSED_RGBA_PVRTC_4BPPV2_IMG
	KTXInternalFormat_RGBA_PVRTC_2BPPV2 = 0x9138,			//GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG
	KTXInternalFormat_ATC_RGB = 0x8C92,						//GL_ATC_RGB_AMD
	KTXInternalFormat_ATC_RGBA_EXPLICIT_ALPHA = 0x8C93,		//GL_ATC_RGBA_EXPLICIT_ALPHA_AMD
	KTXInternalFormat_ATC_RGBA_INTERPOLATED_ALPHA = 0x87EE,	//GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD

	KTXInternalFormat_RGB_ETC = 0x8D64,						//GL_COMPRESSED_RGB8_ETC1
	KTXInternalFormat_RGB_ETC2 = 0x9274,						//GL_COMPRESSED_RGB8_ETC2
	KTXInternalFormat_RGBA_PUNCHTHROUGH_ETC2 = 0x9276,		//GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2
	KTXInternalFormat_RGBA_ETC2 = 0x9278,					//GL_COMPRESSED_RGBA8_ETC2_EAC
	KTXInternalFormat_R11_EAC = 0x9270,						//GL_COMPRESSED_R11_EAC
	KTXInternalFormat_SIGNED_R11_EAC = 0x9271,				//GL_COMPRESSED_SIGNED_R11_EAC
	KTXInternalFormat_RG11_EAC = 0x9272,						//GL_COMPRESSED_RG11_EAC
	KTXInternalFormat_SIGNED_RG11_EAC = 0x9273,				//GL_COMPRESSED_SIGNED_RG11_EAC

	KTXInternalFormat_RGBA_ASTC_4x4 = 0x93B0,				//GL_COMPRESSED_RGBA_ASTC_4x4_KHR
	KTXInternalFormat_RGBA_ASTC_5x4 = 0x93B1,				//GL_COMPRESSED_RGBA_ASTC_5x4_KHR
	KTXInternalFormat_RGBA_ASTC_5x5 = 0x93B2,				//GL_COMPRESSED_RGBA_ASTC_5x5_KHR
	KTXInternalFormat_RGBA_ASTC_6x5 = 0x93B3,				//GL_COMPRESSED_RGBA_ASTC_6x5_KHR
	KTXInternalFormat_RGBA_ASTC_6x6 = 0x93B4,				//GL_COMPRESSED_RGBA_ASTC_6x6_KHR
	KTXInternalFormat_RGBA_ASTC_8x5 = 0x93B5,				//GL_COMPRESSED_RGBA_ASTC_8x5_KHR
	KTXInternalFormat_RGBA_ASTC_8x6 = 0x93B6,				//GL_COMPRESSED_RGBA_ASTC_8x6_KHR
	KTXInternalFormat_RGBA_ASTC_8x8 = 0x93B7,				//GL_COMPRESSED_RGBA_ASTC_8x8_KHR
	KTXInternalFormat_RGBA_ASTC_10x5 = 0x93B8,				//GL_COMPRESSED_RGBA_ASTC_10x5_KHR
	KTXInternalFormat_RGBA_ASTC_10x6 = 0x93B9,				//GL_COMPRESSED_RGBA_ASTC_10x6_KHR
	KTXInternalFormat_RGBA_ASTC_10x8 = 0x93BA,				//GL_COMPRESSED_RGBA_ASTC_10x8_KHR
	KTXInternalFormat_RGBA_ASTC_10x10 = 0x93BB,				//GL_COMPRESSED_RGBA_ASTC_10x10_KHR
	KTXInternalFormat_RGBA_ASTC_12x10 = 0x93BC,				//GL_COMPRESSED_RGBA_ASTC_12x10_KHR
	KTXInternalFormat_RGBA_ASTC_12x12 = 0x93BD,				//GL_COMPRESSED_RGBA_ASTC_12x12_KHR

	// sRGB formats
	KTXInternalFormat_SRGB_DXT1 = 0x8C4C,					//GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
	KTXInternalFormat_SRGB_ALPHA_DXT1 = 0x8C4D,				//GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
	KTXInternalFormat_SRGB_ALPHA_DXT3 = 0x8C4E,				//GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
	KTXInternalFormat_SRGB_ALPHA_DXT5 = 0x8C4F,				//GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
	KTXInternalFormat_SRGB_BP_UNORM = 0x8E8D,				//GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM
	KTXInternalFormat_SRGB_PVRTC_2BPPV1 = 0x8A54,			//GL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT
	KTXInternalFormat_SRGB_PVRTC_4BPPV1 = 0x8A55,			//GL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT
	KTXInternalFormat_SRGB_ALPHA_PVRTC_2BPPV1 = 0x8A56,		//GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT
	KTXInternalFormat_SRGB_ALPHA_PVRTC_4BPPV1 = 0x8A57,		//GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT
	KTXInternalFormat_SRGB_ALPHA_PVRTC_2BPPV2 = 0x93F0,		//COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV2_IMG
	KTXInternalFormat_SRGB_ALPHA_PVRTC_4BPPV2 = 0x93F1,		//GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV2_IMG
	KTXInternalFormat_SRGB8_ETC2 = 0x9275,						//GL_COMPRESSED_SRGB8_ETC2
	KTXInternalFormat_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 = 0x9277,	//GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2
	KTXInternalFormat_SRGB8_ALPHA8_ETC2_EAC = 0x9279,			//GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC
	KTXInternalFormat_SRGB8_ALPHA8_ASTC_4x4 = 0x93D0,		//GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR
	KTXInternalFormat_SRGB8_ALPHA8_ASTC_5x4 = 0x93D1,		//GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR
	KTXInternalFormat_SRGB8_ALPHA8_ASTC_5x5 = 0x93D2,		//GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR
	KTXInternalFormat_SRGB8_ALPHA8_ASTC_6x5 = 0x93D3,		//GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR
	KTXInternalFormat_SRGB8_ALPHA8_ASTC_6x6 = 0x93D4,		//GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR
	KTXInternalFormat_SRGB8_ALPHA8_ASTC_8x5 = 0x93D5,		//GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR
	KTXInternalFormat_SRGB8_ALPHA8_ASTC_8x6 = 0x93D6,		//GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR
	KTXInternalFormat_SRGB8_ALPHA8_ASTC_8x8 = 0x93D7,		//GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR
	KTXInternalFormat_SRGB8_ALPHA8_ASTC_10x5 = 0x93D8,		//GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR
	KTXInternalFormat_SRGB8_ALPHA8_ASTC_10x6 = 0x93D9,		//GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR
	KTXInternalFormat_SRGB8_ALPHA8_ASTC_10x8 = 0x93DA,		//GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR
	KTXInternalFormat_SRGB8_ALPHA8_ASTC_10x10 = 0x93DB,		//GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR
	KTXInternalFormat_SRGB8_ALPHA8_ASTC_12x10 = 0x93DC,		//GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR
	KTXInternalFormat_SRGB8_ALPHA8_ASTC_12x12 = 0x93DD,		//GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR

	KTXInternalFormat_ALPHA8 = 0x803C,
	KTXInternalFormat_ALPHA16 = 0x803E,
	KTXInternalFormat_LUMINANCE8 = 0x8040,
	KTXInternalFormat_LUMINANCE16 = 0x8042,
	KTXInternalFormat_LUMINANCE8_ALPHA8 = 0x8045,
	KTXInternalFormat_LUMINANCE16_ALPHA16 = 0x8048,

	KTXInternalFormat_R8_USCALED_GTC = 0xF000,
	KTXInternalFormat_R8_SSCALED_GTC,
	KTXInternalFormat_RG8_USCALED_GTC,
	KTXInternalFormat_RG8_SSCALED_GTC,
	KTXInternalFormat_RGB8_USCALED_GTC,
	KTXInternalFormat_RGB8_SSCALED_GTC,
	KTXInternalFormat_RGBA8_USCALED_GTC,
	KTXInternalFormat_RGBA8_SSCALED_GTC,
	KTXInternalFormat_RGB10A2_USCALED_GTC,
	KTXInternalFormat_RGB10A2_SSCALED_GTC,
	KTXInternalFormat_R16_USCALED_GTC,
	KTXInternalFormat_R16_SSCALED_GTC,
	KTXInternalFormat_RG16_USCALED_GTC,
	KTXInternalFormat_RG16_SSCALED_GTC,
	KTXInternalFormat_RGB16_USCALED_GTC,
	KTXInternalFormat_RGB16_SSCALED_GTC,
	KTXInternalFormat_RGBA16_USCALED_GTC,
	KTXInternalFormat_RGBA16_SSCALED_GTC,
} KTXInternalFormat;

typedef enum KTXExternalFormat
{
	KTXExternalFormat_NONE = 0,					//GL_NONE
	KTXExternalFormat_RED = 0x1903,				//GL_RED
	KTXExternalFormat_RG = 0x8227,				//GL_RG
	KTXExternalFormat_RGB = 0x1907,				//GL_RGB
	KTXExternalFormat_BGR = 0x80E0,				//GL_BGR
	KTXExternalFormat_RGBA = 0x1908,				//GL_RGBA
	KTXExternalFormat_BGRA = 0x80E1,				//GL_BGRA
	KTXExternalFormat_RED_INTEGER = 0x8D94,		//GL_RED_INTEGER
	KTXExternalFormat_RG_INTEGER = 0x8228,		//GL_RG_INTEGER
	KTXExternalFormat_RGB_INTEGER = 0x8D98,		//GL_RGB_INTEGER
	KTXExternalFormat_BGR_INTEGER = 0x8D9A,		//GL_BGR_INTEGER
	KTXExternalFormat_RGBA_INTEGER = 0x8D99,		//GL_RGBA_INTEGER
	KTXExternalFormat_BGRA_INTEGER = 0x8D9B,		//GL_BGRA_INTEGER
	KTXExternalFormat_DEPTH = 0x1902,			//GL_DEPTH_COMPONENT
	KTXExternalFormat_DEPTH_STENCIL = 0x84F9,	//GL_DEPTH_STENCIL
	KTXExternalFormat_STENCIL = 0x1901,			//GL_STENCIL_INDEX

	KTXExternalFormat_LUMINANCE = 0x1909,				//GL_LUMINANCE
	KTXExternalFormat_ALPHA = 0x1906,					//GL_ALPHA
	KTXExternalFormat_LUMINANCE_ALPHA = 0x190A,			//GL_LUMINANCE_ALPHA

	KTXExternalFormat_SRGB_EXT = 0x8C40,					//SRGB_EXT
	KTXExternalFormat_SRGB_ALPHA_EXT = 0x8C42			//SRGB_ALPHA_EXT
} KTXExternalFormat;

typedef enum KTXType
{
	KTXType_NONE = 0,						//GL_NONE
	KTXType_I8 = 0x1400,					//GL_BYTE
	KTXType_U8 = 0x1401,					//GL_UNSIGNED_BYTE
	KTXType_I16 = 0x1402,					//GL_SHORT
	KTXType_U16 = 0x1403,					//GL_UNSIGNED_SHORT
	KTXType_I32 = 0x1404,					//GL_INT
	KTXType_U32 = 0x1405,					//GL_UNSIGNED_INT
	KTXType_I64 = 0x140E,					//GL_INT64_ARB
	KTXType_U64 = 0x140F,					//GL_UNSIGNED_INT64_ARB
	KTXType_F16 = 0x140B,					//GL_HALF_FLOAT
	KTXType_F16_OES = 0x8D61,				//GL_HALF_FLOAT_OES
	KTXType_F32 = 0x1406,					//GL_FLOAT
	KTXType_F64 = 0x140A,					//GL_DOUBLE
	KTXType_UINT32_RGB9_E5_REV = 0x8C3E,	//GL_UNSIGNED_INT_5_9_9_9_REV
	KTXType_UINT32_RG11B10F_REV = 0x8C3B,	//GL_UNSIGNED_INT_10F_11F_11F_REV
	KTXType_UINT8_RG3B2 = 0x8032,			//GL_UNSIGNED_BYTE_3_3_2
	KTXType_UINT8_RG3B2_REV = 0x8362,		//GL_UNSIGNED_BYTE_2_3_3_REV
	KTXType_UINT16_RGB5A1 = 0x8034,		//GL_UNSIGNED_SHORT_5_5_5_1
	KTXType_UINT16_RGB5A1_REV = 0x8366,	//GL_UNSIGNED_SHORT_1_5_5_5_REV
	KTXType_UINT16_R5G6B5 = 0x8363,		//GL_UNSIGNED_SHORT_5_6_5
	KTXType_UINT16_R5G6B5_REV = 0x8364,	//GL_UNSIGNED_SHORT_5_6_5_REV
	KTXType_UINT16_RGBA4 = 0x8033,			//GL_UNSIGNED_SHORT_4_4_4_4
	KTXType_UINT16_RGBA4_REV = 0x8365,		//GL_UNSIGNED_SHORT_4_4_4_4_REV
	KTXType_UINT32_RGBA8 = 0x8035,			//GL_UNSIGNED_SHORT_8_8_8_8
	KTXType_UINT32_RGBA8_REV = 0x8367,		//GL_UNSIGNED_SHORT_8_8_8_8_REV
	KTXType_UINT32_RGB10A2 = 0x8036,		//GL_UNSIGNED_INT_10_10_10_2
	KTXType_UINT32_RGB10A2_REV = 0x8368,	//GL_UNSIGNED_INT_2_10_10_10_REV

	KTXType_UINT8_RG4_REV_GTC = 0xFFFD,
	KTXType_UINT16_A1RGB5_GTC = 0xFFFC
} KTXType;

typedef struct KTXFormatDesc
{
	KTXExternalFormat mExternal;
	KTXType           mType;
} KTXFormatDesc;

static KTXFormatDesc gKTXFormatToImageFormat[ImageFormat::COUNT] = {};
static eastl::unordered_map<KTXInternalFormat, bool> gKTXSrgbFormats;

typedef struct KTXHeader
{
	uint8_t  mIdentifier[12];
	uint32_t mEndianness;
	uint32_t mGlType;
	uint32_t mGlTypeSize;
	uint32_t mGlFormat;
	uint32_t mGlInternalFormat;
	uint32_t mGlBaseInternalFormat;
	uint32_t mWidth;
	uint32_t mHeight;
	uint32_t mDepth;
	uint32_t mArrayElementCount;
	uint32_t mFaceCount;
	uint32_t mMipmapCount;
	uint32_t mKeyValueDataLength;
} KTXHeader;

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
	mSrgb = false;
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
	mSrgb = img.mSrgb;
	
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

unsigned char* Image::Create(const ImageFormat::Enum fmt, const int w, const int h, const int d, const int mipMapCount, const int arraySize, const unsigned char* rawData)
{
	mFormat = fmt;
	mWidth = w;
	mHeight = h;
	mDepth = d;
	mMipMapCount = mipMapCount;
	mArrayCount = arraySize;
	mOwnsMemory = false;

	pData = (uint8_t*)rawData;
	mLoadFileName = "Undefined";

	return pData;
}

void Image::RedefineDimensions(
	const ImageFormat::Enum fmt, const int w, const int h, const int d, const int mipMapCount, const int arraySize, bool srgb)
{
	//Redefine image that was loaded in
	mFormat = fmt;
	mWidth = w;
	mHeight = h;
	mDepth = d;
	mMipMapCount = mipMapCount;
	mArrayCount = arraySize;
	mSrgb = srgb;

	switch (mFormat)
	{
	case ImageFormat::PVR_2BPP:
	case ImageFormat::PVR_2BPPA:
	case ImageFormat::PVR_4BPP:
	case ImageFormat::PVR_4BPPA:
		mLinearLayout = false;
		break;
	default:
		mLinearLayout = true;
	}
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

	return (mDepth == 0 ? 6 : 1) * ImageFormat::GetMipMappedSize(w, h, d, nMipMapLevels - firstMipMapLevel, srcFormat);
}

// Load Image Data form mData functions

bool iLoadDDSFromMemory(Image* pImage,
	const char* memory, uint32_t memSize, memoryAllocationFunc pAllocator, void* pUserData)
{
	DDSHeader header;

	if (memory == NULL || memSize == 0)
		return false;

	MemoryBuffer file(memory, (unsigned)memSize);
	file.Read(&header, sizeof(header));

	if (header.mDWMagic != MAKE_CHAR4('D', 'D', 'S', ' '))
	{
		return false;
	}

	uint32_t width = header.mDWWidth;
	uint32_t height = header.mDWHeight;
	uint32_t depth = (header.mCaps.mDWCaps2 & DDSCAPS2_CUBEMAP) ? 0 : (header.mDWDepth == 0) ? 1 : header.mDWDepth;
	uint32_t mipMapCount = max(1U, header.mDWMipMapCount);
	uint32_t arrayCount = 1;
	ImageFormat::Enum imageFormat = ImageFormat::NONE;
	bool srgb = false;

	if (header.mPixelFormat.mDWFourCC == MAKE_CHAR4('D', 'X', '1', '0'))
	{
		DDSHeaderDX10 dx10Header;
		file.Read(&dx10Header, sizeof(dx10Header));

		switch (dx10Header.mDXGIFormat)
		{
			case 61: imageFormat = ImageFormat::R8; break;
			case 49: imageFormat = ImageFormat::RG8; break;
			case 28: imageFormat = ImageFormat::RGBA8; break;
			case 29: imageFormat = ImageFormat::RGBA8; srgb = true; break;

			case 56: imageFormat = ImageFormat::R16; break;
			case 35: imageFormat = ImageFormat::RG16; break;
			case 11: imageFormat = ImageFormat::RGBA16; break;

			case 54: imageFormat = ImageFormat::R16F; break;
			case 34: imageFormat = ImageFormat::RG16F; break;
			case 10: imageFormat = ImageFormat::RGBA16F; break;

			case 41: imageFormat = ImageFormat::R32F; break;
			case 16: imageFormat = ImageFormat::RG32F; break;
			case 6: imageFormat = ImageFormat::RGB32F; break;
			case 2: imageFormat = ImageFormat::RGBA32F; break;

			case 67: imageFormat = ImageFormat::RGB9E5; break;
			case 26: imageFormat = ImageFormat::RG11B10F; break;
			case 24: imageFormat = ImageFormat::RGB10A2; break;

			case 71: imageFormat = ImageFormat::DXT1; break;
			case 72: imageFormat = ImageFormat::DXT1; srgb = true; break;
			case 74: imageFormat = ImageFormat::DXT3; break;
			case 75: imageFormat = ImageFormat::DXT3; srgb = true; break;
			case 77: imageFormat = ImageFormat::DXT5; break;
			case 78: imageFormat = ImageFormat::DXT5; srgb = true; break;
			case 80: imageFormat = ImageFormat::ATI1N; break;
			case 83: imageFormat = ImageFormat::ATI2N; break;

			case 95:    // unsigned float
				imageFormat = ImageFormat::GNF_BC6HUF; break;
			case 96:    // signed float
				imageFormat = ImageFormat::GNF_BC6HSF; break;
			case 98:    // regular
				imageFormat = ImageFormat::GNF_BC7; break;
			case 99:    // srgb
				imageFormat = ImageFormat::GNF_BC7; srgb = true; break;

			default: return false;
		}
	}
	else
	{
		switch (header.mPixelFormat.mDWFourCC)
		{
			case 34: imageFormat = ImageFormat::RG16; break;
			case 36: imageFormat = ImageFormat::RGBA16; break;
			case 111: imageFormat = ImageFormat::R16F; break;
			case 112: imageFormat = ImageFormat::RG16F; break;
			case 113: imageFormat = ImageFormat::RGBA16F; break;
			case 114: imageFormat = ImageFormat::R32F; break;
			case 115: imageFormat = ImageFormat::RG32F; break;
			case 116: imageFormat = ImageFormat::RGBA32F; break;
			case MAKE_CHAR4('A', 'T', 'C', ' '): imageFormat = ImageFormat::ATC; break;
			case MAKE_CHAR4('A', 'T', 'C', 'A'): imageFormat = ImageFormat::ATCA; break;
			case MAKE_CHAR4('A', 'T', 'C', 'I'): imageFormat = ImageFormat::ATCI; break;
			case MAKE_CHAR4('A', 'T', 'I', '1'): imageFormat = ImageFormat::ATI1N; break;
			case MAKE_CHAR4('A', 'T', 'I', '2'): imageFormat = ImageFormat::ATI2N; break;
			case MAKE_CHAR4('E', 'T', 'C', ' '): imageFormat = ImageFormat::ETC1; break;
			case MAKE_CHAR4('D', 'X', 'T', '1'): imageFormat = ImageFormat::DXT1; break;
			case MAKE_CHAR4('D', 'X', 'T', '3'): imageFormat = ImageFormat::DXT3; break;
			case MAKE_CHAR4('D', 'X', 'T', '5'): imageFormat = ImageFormat::DXT5; break;
			default:
				switch (header.mPixelFormat.mDWRGBBitCount)
				{
					case 8: imageFormat = ImageFormat::I8; break;
					case 16:
						imageFormat = (header.mPixelFormat.mDWRGBAlphaBitMask == 0xF000)
									  ? ImageFormat::RGBA4
									  : (header.mPixelFormat.mDWRGBAlphaBitMask == 0xFF00)
											? ImageFormat::IA8
											: (header.mPixelFormat.mDWBBitMask == 0x1F) ? ImageFormat::RGB565 : ImageFormat::I16;
						break;
					case 24: imageFormat = ImageFormat::RGB8; break;
					case 32: imageFormat = (header.mPixelFormat.mDWRBitMask == 0x3FF00000) ? ImageFormat::RGB10A2 : ImageFormat::RGBA8; break;
					default: return false;
				}
		}
	}

	pImage->RedefineDimensions(imageFormat, width, height, depth, mipMapCount, arrayCount, srgb);

	int size = pImage->GetMipMappedSize();

	if (pAllocator)
	{
		pImage->SetPixels((unsigned char*)pAllocator(pImage, size, pUserData));
	}
	else
	{
		pImage->SetPixels((unsigned char*)conf_malloc(sizeof(unsigned char) * size), true);
	}

	if (pImage->IsCube())
	{
		for (int face = 0; face < 6; face++)
		{
			for (uint mipMapLevel = 0; mipMapLevel < pImage->GetMipMapCount(); mipMapLevel++)
			{
				int            faceSize = pImage->GetMipMappedSize(mipMapLevel, 1) / 6;
				unsigned char* src = pImage->GetPixels(mipMapLevel, 0) + face * faceSize;

				file.Read(src, faceSize);
			}
		}
	}
	else
	{
		file.Read(pImage->GetPixels(), size);
	}

	if ((imageFormat == ImageFormat::RGB8 || imageFormat == ImageFormat::RGBA8) && header.mPixelFormat.mDWBBitMask == 0xFF)
	{
		int nChannels = ImageFormat::GetChannelCount(imageFormat);
		swapPixelChannels(pImage->GetPixels(), size / nChannels, nChannels, 0, 2);
	}

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
	bool srgb = (psPVRHeader->mColorSpace == 1);
	ImageFormat::Enum imageFormat = ImageFormat::NONE;

	switch (psPVRHeader->mPixelFormat)
	{
	case 0:
		imageFormat = ImageFormat::PVR_2BPP;
		break;
	case 1:
		imageFormat = ImageFormat::PVR_2BPPA;
		break;
	case 2:
		imageFormat = ImageFormat::PVR_4BPP;
		break;
	case 3:
		imageFormat = ImageFormat::PVR_4BPPA;
		break;
	default:    // NOT SUPPORTED
		LOGF(LogLevel::eERROR, "Load PVR failed: pixel type not supported. ");
		ASSERT(0);
		return false;
	}

	if (depth != 0)
		arrayCount *= psPVRHeader->mNumFaces;

	pImage->RedefineDimensions(imageFormat, width, height, depth, mipMapCount, arrayCount, srgb);


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

bool iLoadKTXFromMemory(Image* pImage, const char* memory, uint32_t memSize, memoryAllocationFunc pAllocator /*= NULL*/, void* pUserData /*= NULL*/)
{
#define byte_swap(num) ((((num) >> 24) & 0xff) | (((num) << 8) & 0xff0000) | (((num) >> 8) & 0xff00) | (((num) << 24) & 0xff000000))

	// KTX File Structure
	// Header
	// MetaData
	// ImageSize for Mip 0 [ header.arrayCount x header.faceCount ]
	// ImageData for Mip 0
	//   Array 0 to header.arrayCount
	//     Face 0 to header.faceCount
	// ImageSize for Mip 1
	//   Array 0 to header.arrayCount
	//     Face 0 to header.faceCount
	// ...
	
	MemoryBuffer file(memory, (unsigned)memSize);
	KTXHeader header = {};
	
	file.Read(&header, sizeof(header));

	char *format = (char *)(header.mIdentifier + 1);
	if (strncmp(format, "KTX 11", 6) != 0)
	{
		LOGF(LogLevel::eERROR, "Load KTX failed: Not a valid KTX header.");
		return false;
	}

	bool endianSwap = (header.mEndianness == 0x01020304);

	uint32_t width = endianSwap ? (uint32_t)byte_swap(header.mWidth) : header.mWidth;
	uint32_t height = endianSwap ? (uint32_t)byte_swap(header.mHeight) : header.mHeight;
	uint32_t depth = max(1U, endianSwap ? (uint32_t)byte_swap(header.mDepth) : header.mDepth);
	uint32_t arrayCount = max(1U, endianSwap ? (uint32_t)byte_swap(header.mArrayElementCount) : header.mArrayElementCount);
	uint32_t faceCount = endianSwap ? (uint32_t)byte_swap(header.mFaceCount) : header.mFaceCount;
	uint32_t internalFormat = endianSwap ? (uint32_t)byte_swap(header.mGlInternalFormat) : header.mGlInternalFormat;
	uint32_t externalFormat = endianSwap ? (uint32_t)byte_swap(header.mGlFormat) : header.mGlFormat;
	uint32_t type = endianSwap ? (uint32_t)byte_swap(header.mGlType) : header.mGlType;
	uint32_t mipCount = endianSwap ? (uint32_t)byte_swap(header.mMipmapCount) : header.mMipmapCount;
	uint32_t keyValueDataLength = endianSwap ? (uint32_t)byte_swap(header.mKeyValueDataLength) : header.mKeyValueDataLength;
	bool srgb = false;
	ImageFormat::Enum imageFormat = ImageFormat::NONE;

	if (arrayCount > 1 && faceCount > 1)
	{
		LOGF(LogLevel::eERROR, "Load KTX failed: Loading arrays of cubemaps isn't supported.");
		return false;
	}

	if (gKTXSrgbFormats.find((KTXInternalFormat)internalFormat) != gKTXSrgbFormats.end())
	{
		srgb = true;
	}

	// Uncompressed
	if (externalFormat != 0)
	{
		for (uint32_t i = 0; i < ImageFormat::COUNT; ++i)
		{
			if (externalFormat == gKTXFormatToImageFormat[i].mExternal && type == gKTXFormatToImageFormat[i].mType)
			{
				imageFormat = (ImageFormat::Enum)i;
				break;
			}
		}
	}
	// Compressed - Support only ASTC for now
	else
	{
#if !defined(TARGET_IOS) && !defined(__ANDROID__)
		LOGF(LogLevel::eERROR, "Load KTX failed: Compressed formats supported on mobile targets only.");
		return false;
#else

		if (!(internalFormat >= KTXInternalFormat_RGBA_ASTC_4x4 && internalFormat <= KTXInternalFormat_SRGB8_ALPHA8_ASTC_12x12))
		{
			LOGF(LogLevel::eERROR, "Load KTX failed: Support for loading ASTC compressed textures only.");
			return false;
		}

		imageFormat = (ImageFormat::Enum)(ImageFormat::ASTC_4x4 + (internalFormat - (srgb ? KTXInternalFormat_SRGB8_ALPHA8_ASTC_4x4 : KTXInternalFormat_RGBA_ASTC_4x4)));
#endif
	}

	depth = (faceCount > 1) ? 0 : depth;
	if (depth != 0)
		arrayCount *= faceCount;

	pImage->RedefineDimensions(imageFormat, width, height, depth, mipCount, arrayCount, srgb);

	// Skip ahead to the pixel data
	const uint32_t offsetToPixelData = sizeof(KTXHeader) + keyValueDataLength;
	file.Seek(offsetToPixelData, SEEK_DIR_BEGIN);
	const uint32_t dataLength = file.GetSize() - offsetToPixelData;
	
	if (pAllocator)
	{
		pImage->SetPixels((uint8_t*)pAllocator(pImage, dataLength, pUserData));
	}
	else
	{
		pImage->SetPixels((uint8_t*)conf_malloc(sizeof(uint8_t) * dataLength), true);
	}

	uint8_t* pData = pImage->GetPixels();
	uint32_t levelOffset = 0;
	while (!file.IsEof())
	{
		uint32_t levelSize = file.ReadUInt() / pImage->GetArrayCount();
		if (pImage->GetArrayCount() > 1)
			levelSize /= faceCount;
		
		for (uint32_t i = 0; i < pImage->GetArrayCount(); ++i)
		{
			for (uint32_t j = 0; j < faceCount; ++j)
			{
				file.Read(pData + levelOffset, levelSize);
				levelOffset += levelSize;
				
				// No need to worry about cube padding since block size is a multiple of 4
			}
		}
		
		// No need to worry about mip padding since block size is a multiple of 4
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
	eastl::string              mExtension;
	Image::ImageLoaderFunction pLoader;
};

static eastl::vector<ImageLoaderDefinition> gImageLoaders;

struct StaticImageLoader
{
	StaticImageLoader()
	{
		gImageLoaders.push_back({ ".dds", iLoadDDSFromMemory });
		gImageLoaders.push_back({ ".pvr", iLoadPVRFromMemory });
		gImageLoaders.push_back({ ".ktx", iLoadKTXFromMemory });
#if defined(ORBIS)
		gImageLoaders.push_back({ ".gnf", iLoadGNFFromMemory });
#endif

		gKTXFormatToImageFormat[ImageFormat::R8] = { KTXExternalFormat::KTXExternalFormat_RED, KTXType_U8 };
		gKTXFormatToImageFormat[ImageFormat::RG8] = { KTXExternalFormat::KTXExternalFormat_RG, KTXType_U8 };
		gKTXFormatToImageFormat[ImageFormat::RGB8] = { KTXExternalFormat::KTXExternalFormat_RGB, KTXType_U8 };
		gKTXFormatToImageFormat[ImageFormat::RGBA8] = { KTXExternalFormat::KTXExternalFormat_RGBA, KTXType_U8 };

		gKTXFormatToImageFormat[ImageFormat::R8S] = { KTXExternalFormat::KTXExternalFormat_RED, KTXType_I8 };
		gKTXFormatToImageFormat[ImageFormat::RG8S] = { KTXExternalFormat::KTXExternalFormat_RG, KTXType_I8 };
		gKTXFormatToImageFormat[ImageFormat::RGB8S] = { KTXExternalFormat::KTXExternalFormat_RGB, KTXType_I8 };
		gKTXFormatToImageFormat[ImageFormat::RGBA8S] = { KTXExternalFormat::KTXExternalFormat_RGBA, KTXType_I8 };

		gKTXFormatToImageFormat[ImageFormat::R16F] = { KTXExternalFormat::KTXExternalFormat_RED, KTXType_F16 };
		gKTXFormatToImageFormat[ImageFormat::RG16F] = { KTXExternalFormat::KTXExternalFormat_RG, KTXType_F16 };
		gKTXFormatToImageFormat[ImageFormat::RGB16F] = { KTXExternalFormat::KTXExternalFormat_RGB, KTXType_F16 };
		gKTXFormatToImageFormat[ImageFormat::RGBA16F] = { KTXExternalFormat::KTXExternalFormat_RGBA, KTXType_F16 };

		gKTXFormatToImageFormat[ImageFormat::R32F] = { KTXExternalFormat::KTXExternalFormat_RED, KTXType_F32 };
		gKTXFormatToImageFormat[ImageFormat::RG32F] = { KTXExternalFormat::KTXExternalFormat_RG, KTXType_F32 };
		gKTXFormatToImageFormat[ImageFormat::RGB32F] = { KTXExternalFormat::KTXExternalFormat_RGB, KTXType_F32 };
		gKTXFormatToImageFormat[ImageFormat::RGBA32F] = { KTXExternalFormat::KTXExternalFormat_RGBA, KTXType_F32 };

		gKTXSrgbFormats[KTXInternalFormat_SR8] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRG8] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8] = true;
		// Compressed sRGB formats
		gKTXSrgbFormats[KTXInternalFormat_SRGB_DXT1] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB_ALPHA_DXT1] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB_ALPHA_DXT3] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB_ALPHA_DXT5] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB_BP_UNORM] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB_PVRTC_2BPPV1] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB_PVRTC_4BPPV1] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB_ALPHA_PVRTC_2BPPV1] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB_ALPHA_PVRTC_4BPPV1] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB_ALPHA_PVRTC_2BPPV2] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB_ALPHA_PVRTC_4BPPV2] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ETC2] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ETC2_EAC] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ASTC_4x4] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ASTC_5x4] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ASTC_5x5] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ASTC_6x5] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ASTC_6x6] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ASTC_8x5] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ASTC_8x6] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ASTC_8x8] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ASTC_10x5] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ASTC_10x6] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ASTC_10x8] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ASTC_10x10] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ASTC_12x10] = true;
		gKTXSrgbFormats[KTXInternalFormat_SRGB8_ALPHA8_ASTC_12x12] = true;
	}
} gImageLoaderInst;

void Image::AddImageLoader(const char* pExtension, ImageLoaderFunction pFunc) { gImageLoaders.push_back({ pExtension, pFunc }); }

bool Image::LoadFromMemory(
	void const* mem, uint32_t size, char const* extension, memoryAllocationFunc pAllocator, void* pUserData)
{
	// try loading the format
	bool loaded = false;
	for (uint32_t i = 0; i < (uint32_t)gImageLoaders.size(); ++i)
	{
		ImageLoaderDefinition const& def = gImageLoaders[i];
		if (stricmp(extension, def.mExtension.c_str()) == 0)
		{
			loaded = def.pLoader(this, (char const*)mem, size, pAllocator, pUserData);
			break;
		}
	}
	return loaded;
}

bool Image::LoadFromFile(const char* origFileName, memoryAllocationFunc pAllocator, void* pUserData, FSRoot root)
{
	// clear current image
	Clear();

	eastl::string extension = FileSystem::GetExtension(origFileName);
	uint32_t loaderIndex = -1;

	if (extension.size())
	{
		for (int i = 0; i < (int)gImageLoaders.size(); i++)
		{
			if (stricmp(extension.c_str(), gImageLoaders[i].mExtension.c_str()) == 0)
			{
				loaderIndex = i;
				break;
			}
		}

		if (loaderIndex == -1)
			extension = "";
	}

	char fileName[MAX_PATH] = {};
	strcpy(fileName, origFileName);
	if (!extension.size())
	{
#if defined(__ANDROID__)
		extension = ".ktx";
#elif defined(TARGET_IOS)
		extension = ".ktx";
#elif defined(__linux__)
		extension = ".dds";
#elif defined(__APPLE__)
		extension = ".dds";
#else
		extension = ".dds";
#endif

		strcpy(fileName + strlen(origFileName), extension.c_str());
	}

	// open file
	File file = {};
	file.Open(fileName, FM_ReadBinary, root);
	if (!file.IsOpen())
	{
		LOGF(LogLevel::eERROR, "\"%s\": Image file not found.", fileName);
		return false;
	}

	// load file into memory
	uint32_t length = file.GetSize();
	if (length == 0)
	{
		//char output[256];
		//sprintf(output, "\"%s\": Image file is empty.", fileName);
		LOGF(LogLevel::eERROR, "\"%s\": Image is an empty file.", fileName);
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
		if (stricmp(extension.c_str(), gImageLoaders[i].mExtension.c_str()) == 0)
		{
			support = true;
			loaded = gImageLoaders[i].pLoader(this, data, length, pAllocator, pUserData);
			if (loaded)
			{
				break;
			}
		}
	}
	if (!support)
	{
		LOGF(LogLevel::eERROR, "Can't load this file format for image  :  %s", fileName);
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
			LOGF(LogLevel::eERROR, 
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

bool Image::Save(const char* fileName)
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
		LOGF(LogLevel::eERROR, "Can't save this file format for image  :  %s", fileName);
	}

	return false;
}

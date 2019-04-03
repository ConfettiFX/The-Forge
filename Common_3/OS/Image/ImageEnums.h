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

#pragma once

#include "../Interfaces/ILogManager.h"

#if defined(ORBIS)
// Indicates the result of a GNF load operation
enum GnfError
{
	kGnfErrorNone = 0,                     // Operation was successful; no error
	kGnfErrorInvalidPointer = -1,          // Caller passed an invalid/NULL pointer to a GNF loader function
	kGnfErrorNotGnfFile = -2,              // Attempted to load a file that isn't a GNF file (bad magic number in header)
	kGnfErrorCorruptHeader = -3,           // Attempted to load a GNF file with corrupt header data
	kGnfErrorFileIsTooShort = -4,          // Attempted to load a GNF file whose size is smaller than the size reported in its header
	kGnfErrorVersionMismatch = -5,         // Attempted to load a GNF file created by a different version of the GNF code
	kGnfErrorAlignmentOutOfRange = -6,     // Attempted to load a GNF file with corrupt header data (surface alignment > 2^31 bytes)
	kGnfErrorContentsSizeMismatch = -7,    // Attempted to load a GNF file with corrupt header data (wrong size in GNF header contents)
	kGnfErrorCouldNotOpenFile = -8,        // Unable to open a file for reading
	kGnfErrorOutOfMemory = -9,             // Internal memory allocation failed
};
#endif

namespace ImageFormat {
enum Enum
{
	NONE = 0,

	// Unsigned formats
	R8 = 1,
	RG8 = 2,
	RGB8 = 3,
	RGBA8 = 4,

	R16 = 5,
	RG16 = 6,
	RGB16 = 7,
	RGBA16 = 8,

	// Signed formats
	R8S = 9,
	RG8S = 10,
	RGB8S = 11,
	RGBA8S = 12,

	R16S = 13,
	RG16S = 14,
	RGB16S = 15,
	RGBA16S = 16,

	// Float formats
	R16F = 17,
	RG16F = 18,
	RGB16F = 19,
	RGBA16F = 20,

	R32F = 21,
	RG32F = 22,
	RGB32F = 23,
	RGBA32F = 24,

	// Signed integer formats
	R16I = 25,
	RG16I = 26,
	RGB16I = 27,
	RGBA16I = 28,

	R32I = 29,
	RG32I = 30,
	RGB32I = 31,
	RGBA32I = 32,

	// Unsigned integer formats
	R16UI = 33,
	RG16UI = 34,
	RGB16UI = 35,
	RGBA16UI = 36,

	R32UI = 37,
	RG32UI = 38,
	RGB32UI = 39,
	RGBA32UI = 40,

	// Packed formats
	RGBE8 = 41,
	RGB9E5 = 42,
	RG11B10F = 43,
	RGB565 = 44,
	RGBA4 = 45,
	RGB10A2 = 46,

	// Depth formats
	D16 = 47,
	D24 = 48,
	D24S8 = 49,
	D32F = 50,

	// Compressed formats
	DXT1 = 51,
	DXT3 = 52,
	DXT5 = 53,
	ATI1N = 54,
	ATI2N = 55,

	// PVR formats
	PVR_2BPP = 56,
	PVR_2BPPA = 57,
	PVR_4BPP = 58,
	PVR_4BPPA = 59,

	//http://aras-p.info/texts/D3D9GPUHacks.html
	INTZ = 60,    //  NVidia hack. Supported on all DX10+ HW

	//  XBox 360 specific fron buffer formats. NOt listed in other renderers. Please, add them when extend this structure.
	LE_XRGB8 = 61,
	LE_ARGB8 = 62,
	LE_X2RGB10 = 63,
	LE_A2RGB10 = 64,

	// compressed mobile forms
	ETC1 = 65,    //  RGB
	ATC = 66,     //  RGB
	ATCA = 67,    //  RGBA, explicit alpha
	ATCI = 68,    //  RGBA, interpolated alpha

	//http://aras-p.info/texts/D3D9GPUHacks.html
	RAWZ = 69,           //depth only, Nvidia (requires recombination of data) //FIX IT: PS3 as well?
	DF16 = 70,           //depth only, Intel/AMD
	STENCILONLY = 71,    // stencil ony usage

	// BC1 == DXT1
	// BC2 == DXT2
	// BC3 == DXT4 / 5
	// BC4 == ATI1 == One color channel (8 bits)
	// BC5 == ATI2 == Two color channels (8 bits:8 bits)
	// BC6 == Three color channels (16 bits:16 bits:16 bits) in "half" floating point*
	// BC7 == Three color channels (4 to 7 bits per channel) with 0 to 8 bits of alpha
	GNF_BC1 = 72,
	GNF_BC2 = 73,
	GNF_BC3 = 74,
	GNF_BC4 = 75,
	GNF_BC5 = 76,
	GNF_BC6 = 77,
	GNF_BC7 = 78,
	// Reveser Form
	BGRA8 = 79,

	// Extend for DXGI
	X8D24PAX32 = 80,
	S8 = 81,
	D16S8 = 82,
	D32S8 = 83,
	
	// PVR SRGB extensions
	PVR_2BPP_SRGB = 84,
	PVR_2BPPA_SRGB = 85,
	PVR_4BPP_SRGB = 86,
	PVR_4BPPA_SRGB = 87,
	
	// Count identifier - not actually a format.
	COUNT,

	// Aliases
	I8 = R8,
	IA8 = RG8,
	I16 = R16,
	IA16 = RG16,
	I16F = R16F,
	IA16F = RG16F,
	I32F = R32F,
	IA32F = RG32F
};

enum BlockSize
{
	BLOCK_SIZE_1x1,
	BLOCK_SIZE_4x4,
	BLOCK_SIZE_4x8,
};

static inline bool IsPlainFormat(const Enum format)
{
	return (format <= RGBA32UI) || (format == BGRA8);
}

static inline int32_t GetBytesPerPixel(const Enum format)
{
	// Does not accept compressed formats

	static const int32_t bytesPP[] = {
		0, 1, 2,  3,  4,       //  8-bit unsigned
		2, 4, 6,  8,           // 16-bit unsigned
		1, 2, 3,  4,           //  8-bit signed
		2, 4, 6,  8,           // 16-bit signed
		2, 4, 6,  8,           // 16-bit float
		4, 8, 12, 16,          // 32-bit float
		2, 4, 6,  8,           // 16-bit unsigned integer
		4, 8, 12, 16,          // 32-bit unsigned integer
		2, 4, 6,  8,           // 16-bit signed integer
		4, 8, 12, 16,          // 32-bit signed integer
		4, 4, 4,  2,  2, 4,    // Packed
		2, 4, 4,  4,           // Depth
	};

	if (format == BGRA8)
		return 4;

	ASSERT(format <= D32F || format == BGRA8 || format == D32S8);

	return bytesPP[format];
}

static inline int32_t GetBytesPerChannel(const Enum format)
{
	// Accepts only plain formats
	static const int32_t bytesPC[] = {
		1,    //  8-bit unsigned
		2,    // 16-bit unsigned
		1,    //  8-bit signed
		2,    // 16-bit signed
		2,    // 16-bit float
		4,    // 32-bit float
		2,    // 16-bit unsigned integer
		4,    // 32-bit unsigned integer
		2,    // 16-bit signed integer
		4,    // 32-bit signed integer
	};

	ASSERT(format <= RGBA32UI);

	return bytesPC[(format - 1) >> 2];
}

static inline BlockSize GetBlockSize(const Enum format)
{
	switch (format)
	{
	case PVR_2BPP_SRGB:     //  4x8
	case PVR_2BPPA_SRGB:    //  4x8
	case PVR_2BPP:          //  4x8
	case PVR_2BPPA:         //  4x8
		return BLOCK_SIZE_4x8;

	case DXT1:         //  4x4
	case ATI1N:        //  4x4
	case GNF_BC1:      //  4x4
	case ETC1:         //  4x4
	case ATC:          //  4x4
	case PVR_4BPP:     //  4x4
	case PVR_4BPPA:    //  4x4
	case DXT3:         //  4x4
	case DXT5:         //  4x4
	case GNF_BC3:      //  4x4
	case GNF_BC5:      //  4x4
	case ATI2N:        //  4x4
	case ATCA:         //  4x4
	case ATCI:         //  4x4
#ifdef FORGE_JHABLE_EDITS_V01
	case GNF_BC6:    //  4x4
	case GNF_BC7:    //  4x4
#endif
		return BLOCK_SIZE_4x4;

	default: return BLOCK_SIZE_1x1;
	}
}

static inline bool IsIntegerFormat(const Enum format)
{
	return (format >= R16I && format <= RGBA32UI);
}

static inline bool IsCompressedFormat(const Enum format)
{
	return (
		((format >= DXT1) && (format <= PVR_4BPPA)) ||
		((format >= PVR_2BPP_SRGB) && (format <= PVR_4BPPA_SRGB)) ||
		((format >= ETC1) && (format <= ATCI)) ||
		((format >= GNF_BC1) && (format <= GNF_BC7)));
}

static inline int32_t GetBytesPerBlock(const Enum format)
{
	if (IsCompressedFormat(format))
	{
		switch (format)
		{
				// BC1 == DXT1
				// BC2 == DXT2
				// BC3 == DXT4 / 5
				// BC4 == ATI1 == One color channel (8 bits)
				// BC5 == ATI2 == Two color channels (8 bits:8 bits)
				// BC6 == Three color channels (16 bits:16 bits:16 bits) in "half" floating point*
				// BC7 == Three color channels (4 to 7 bits per channel) with 0 to 8 bits of alpha
			case DXT1:              //  4x4
			case ATI1N:             //  4x4
			case GNF_BC1:           //  4x4
			case ETC1:              //  4x4
			case ATC:               //  4x4
			case PVR_4BPP:          //  4x4
			case PVR_4BPPA:         //  4x4
			case PVR_2BPP:          //  4x8
			case PVR_2BPPA:         //  4x8
			case PVR_4BPP_SRGB:     //  4x4
			case PVR_4BPPA_SRGB:    //  4x4
			case PVR_2BPP_SRGB:     //  4x8
			case PVR_2BPPA_SRGB:    //  4x8
				return 8;

			case DXT3:       //  4x4
			case DXT5:       //  4x4
			case GNF_BC3:    //  4x4
			case GNF_BC5:    //  4x4
			case ATI2N:      //  4x4
			case ATCA:       //  4x4
			case ATCI:       //  4x4
#ifdef FORGE_JHABLE_EDITS_V01
			case GNF_BC6:    //  4x4
			case GNF_BC7:    //  4x4
#endif
				return 16;

			default: return 0;
		}
	}

	return GetBytesPerPixel(format);
}

static inline bool IsFloatFormat(const Enum format)
{
	//	return (format >= R16F && format <= RGBA32F);
	return (format >= R16F && format <= RG11B10F) || (format == D32F);
}

static inline bool IsSignedFormat(const Enum format)
{
	return ((format >= R8S) && (format <= RGBA16S)) ||
		((format >= R16I) && (format <= RGBA32I));
}

static inline bool IsStencilFormat(const Enum format)
{
	return (format == D24S8) || (format >= X8D24PAX32 && format <= D32S8);
}

static inline bool IsDepthFormat(const Enum format)
{
	return (format >= D16 && format <= D32F) || (format == X8D24PAX32) || (format == D16S8) || (format == D32S8);
}

static inline bool IsPackedFormat(const Enum format)
{
	return (format >= RGBE8 && format <= RGB10A2);
}

struct ImageFormatString
{
	Enum format;
	const char* string;
};

static inline const ImageFormatString* getFormatStrings()
{
	static const ImageFormatString formatStrings[] = { { NONE, "NONE" },

													   { R8, "R8" },
													   { RG8, "RG8" },
													   { RGB8, "RGB8" },
													   { RGBA8, "RGBA8" },

													   { R16, "R16" },
													   { RG16, "RG16" },
													   { RGB16, "RGB16" },
													   { RGBA16, "RGBA16" },

													   { R16F, "R16F" },
													   { RG16F, "RG16F" },
													   { RGB16F, "RGB16F" },
													   { RGBA16F, "RGBA16F" },

													   { R32F, "R32F" },
													   { RG32F, "RG32F" },
													   { RGB32F, "RGB32F" },
													   { RGBA32F, "RGBA32F" },

													   { RGBE8, "RGBE8" },
													   { RGB565, "RGB565" },
													   { RGBA4, "RGBA4" },
													   { RGB10A2, "RGB10A2" },

													   { DXT1, "DXT1" },
													   { DXT3, "DXT3" },
													   { DXT5, "DXT5" },
													   { ATI1N, "ATI1N" },
													   { ATI2N, "ATI2N" },

													   { PVR_2BPP, "PVR_2BPP" },
													   { PVR_2BPPA, "PVR_2BPPA" },
													   { PVR_4BPP, "PVR_4BPP" },
													   { PVR_4BPPA, "PVR_4BPPA" },

													   { INTZ, "INTZ" },

													   { LE_XRGB8, "LE_XRGB8" },
													   { LE_ARGB8, "LE_ARGB8" },
													   { LE_X2RGB10, "LE_X2RGB10" },
													   { LE_A2RGB10, "LE_A2RGB10" },

													   { ETC1, "ETC1" },
													   { ATC, "ATC" },
													   { ATCA, "ATCA" },
													   { ATCI, "ATCI" },

													   { GNF_BC1, "GNF_BC1" },
													   { GNF_BC2, "GNF_BC2" },
													   { GNF_BC3, "GNF_BC3" },
													   { GNF_BC4, "GNF_BC4" },
													   { GNF_BC5, "GNF_BC5" },
													   { GNF_BC6, "GNF_BC6" },
													   { GNF_BC7, "GNF_BC7" },

													   { BGRA8, "BGRA8" },
													   { X8D24PAX32, "X8D24PAX32" },
													   { S8, "S8" },
													   { D16S8, "D16S8" },
													   { D32S8, "D32S8" },

													   { PVR_2BPP_SRGB, "PVR_2BPP_SRGB" },
													   { PVR_2BPPA_SRGB, "PVR_2BPPA_SRGB" },
													   { PVR_4BPP_SRGB, "PVR_4BPP_SRGB" },
													   { PVR_4BPPA_SRGB, "PVR_4BPPA_SRGB" },

	};
	return formatStrings;
}

static inline const char* GetFormatString(const Enum format)
{
	for (uint32_t i = 0; i < COUNT; i++)
	{
		if (format == getFormatStrings()[i].format)
			return getFormatStrings()[i].string;
	}
	return NULL;
}

static inline Enum GetFormatFromString(char* string)
{
	for (uint32_t i = 0; i < COUNT; i++)
	{
		if (stricmp(string, getFormatStrings()[i].string) == 0)
			return getFormatStrings()[i].format;
	}
	return NONE;
}

static inline int32_t GetChannelCount(const Enum format)
{
	// #REMOVE
	if (format == BGRA8)
		return 4;
	static const int32_t channelCount[] = {
		0, 1, 2, 3, 4,         //  8-bit unsigned
		1, 2, 3, 4,            // 16-bit unsigned
		1, 2, 3, 4,            //  8-bit signed
		1, 2, 3, 4,            // 16-bit signed
		1, 2, 3, 4,            // 16-bit float
		1, 2, 3, 4,            // 32-bit float
		1, 2, 3, 4,            // 16-bit signed integer
		1, 2, 3, 4,            // 32-bit signed integer
		1, 2, 3, 4,            // 16-bit unsigned integer
		1, 2, 3, 4,            // 32-bit unsigned integer
		3, 3, 3, 3, 4, 4,      // Packed
		1, 1, 2, 1,            // Depth
		3, 4, 4, 1, 2,         // Compressed
		3, 4, 3, 4,            // PVR
		1,                     //  INTZ
		3, 4, 3, 4,            //  XBox front buffer formats
		3, 3, 4, 4,            //  ETC, ATC
		1, 1,                  //  RAWZ, DF16
		3, 4, 4, 1, 2, 3, 3,   // GNF_BC1~GNF_BC7
		3, 4, 3, 4,            // PVR sRGB
	};

	if (format >= sizeof(channelCount) / sizeof(int))
	{
		LOGERRORF("Fail to find Channel in format : %s", GetFormatString(format));
		return 0;
	}

	return channelCount[format];
}

static inline uint32_t GetImageFormatStride(Enum format)
{
	uint32_t result = 0;
	switch (format)
	{
		// 1 channel
	case R8: result = 1; break;
	case R16: result = 2; break;
	case R16F: result = 2; break;
	case R32UI: result = 4; break;
	case R32F:
		result = 4;
		break;
		// 2 channel
	case RG8: result = 2; break;
	case RG16: result = 4; break;
	case RG16F: result = 4; break;
	case RG32UI: result = 8; break;
	case RG32F:
		result = 8;
		break;
		// 3 channel
	case RGB8: result = 3; break;
	case RGB16: result = 6; break;
	case RGB16F: result = 6; break;
	case RGB32UI: result = 12; break;
	case RGB32F:
		result = 12;
		break;
		// 4 channel
	case BGRA8: result = 4; break;
	case RGBA8: result = 4; break;
	case RGBA16: result = 8; break;
	case RGBA16F: result = 8; break;
	case RGBA32UI: result = 16; break;
	case RGBA32F:
		result = 16;
		break;
		// Depth/stencil
	case D16: result = 0; break;
	case X8D24PAX32: result = 0; break;
	case D32F: result = 0; break;
	case S8: result = 0; break;
	case D16S8: result = 0; break;
	case D24S8: result = 0; break;
	case D32S8: result = 0; break;
	default: break;
	}
	return result;
}

static inline uint32_t GetImageFormatChannelCount(Enum format)
{
	//  uint32_t result = 0;
	if (format == BGRA8)
		return 3;
	else
	{
		return GetChannelCount(format);
	}
}

}    // namespace ImageFormat

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

}    // namespace ImageFormat
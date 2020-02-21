//---------------------------------------------------------------------------------------
//  Header file that defines the .tfx binary file format. This is the format that will
//  be exported by authoring tools, and is the standard file format for hair data. The game
//  can either read this file directly, or further procsessing can be done offline to improve
//  load times.
//-------------------------------------------------------------------------------------
//
// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

//
// TressFXTFXFileHeader Structure
//
// This structure defines the header of the file. The actual vertex data follows this as specified by the offsets.
struct TressFXTFXFileHeader
{
	float        version;                 // Specifies TressFX version number
	unsigned int numHairStrands;          // Number of hair strands in this file. All strands in this file are guide strands.
										  // Follow hair strands are generated procedurally.
	unsigned int numVerticesPerStrand;    // From 4 to 64 inclusive (POW2 only). This should be a fixed value within tfx value.
										  // The total vertices from the tfx file is numHairStrands * numVerticesPerStrand.

	// Offsets to array data starts here. Offset values are in bytes, aligned on 8 bytes boundaries,
	// and relative to beginning of the .tfx file
	unsigned int offsetVertexPosition;    // Array size: FLOAT4[numHairStrands]
	unsigned int offsetStrandUV;          // Array size: FLOAT2[numHairStrands], if 0 no texture coordinates
	unsigned int offsetVertexUV;    // Array size: FLOAT2[numHairStrands * numVerticesPerStrand], if 0, no per vertex texture coordinates
	unsigned int offsetStrandThickness;    // Array size: float[numHairStrands]
	unsigned int offsetVertexColor;        // Array size: FLOAT4[numHairStrands * numVerticesPerStrand], if 0, no vertex colors

	unsigned int reserved[32];    // Reserved for future versions
};

struct TressFXTFXBoneFileHeader
{
	float        version;
	unsigned int numHairStrands;
	unsigned int numInfluenceBones;
	unsigned int offsetBoneNames;
	unsigned int offsetSkinningData;
	unsigned int reserved[32];
};

//
// TressFXFileObject Structure
//
// This structure defines the start of the file. The actual vertex data follows this as specified by the offsets.
//
struct TressFXFileObject
{
	float version;                // Specifies TressFX version number
	uint32_t  numGuideHairStrands;    // Number of hair strands in this file. All strands in this file are guide strands.
								  // Follow hair strands are generated procedurally.

	uint32_t numVerticesPerStrand;    // From 4 to 64 inclusive (POW2 only). This should be a fixed value, i.e. it should
								  // NOT be allowed to change on a per-strand basis.

	uint32_t numFollowHairsPerGuideHair;    // Ratio of follow hairs to guide hairs.
										// 0 is valid and would mean all hair strands are simulated individually.

	bool bothEndsImmovable;    // A non-zero value indicates that both the root and the end of the hair are fixed.
							   // If this value is 0 only the root vertex position is fixed.

	uint32_t reserved1[31];    // Reserved for future versions

	// Offsets to array data starts here.
	// Offset values are in bytes, aligned on 8 bytes boundaries, and relative to beginning of the .tfx file
	uint32_t verticesOffset;              // Array size: FLOAT3[numGuideHairStrands]
	uint32_t perStrandThicknessOffset;    // Array size: FLOAT[numGuideHairStrands]
	uint32_t perStrandTexCoordOffset;     // Array size: FLOAT2[numHairStrands], if 0 no texture coordinates
	uint32_t perVertexColorOffset;        // Array size: FLOAT4[numHairStrands * numVerticesPerStrand], if 0, no vertex colors
	uint32_t guideHairStrandOffset;       // Array size: uint32_t[numHairStrands], if zero, all strands are guide hairs
	uint32_t perVertexTexCoordOffset;     // Array size: FLOAT2[numHairStrands * numVerticesPerStrand], if 0, no per vertex texture coordinates

	uint32_t reserved2[32];    // Reserved for future versions
};
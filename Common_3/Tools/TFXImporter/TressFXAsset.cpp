//---------------------------------------------------------------------------------------
// Loads and processes TressFX files.
// Inputs are binary files/streams/blobs
// Outputs are raw data that will mostly end up on the GPU.
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

#include "TressFXAsset.h"

#include "../../ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../OS/Interfaces/ILogManager.h"
#include "../../OS/Interfaces/IMemoryManager.h"

#include <math.h>

#define AMD_TRESSFX_V4 4
#define AMD_TRESSFX_V3 3
#define TRESSFX_SIM_THREAD_GROUP_SIZE 64

namespace AMD {
static void GetTangentVectors(const vec3& n, vec3& t0, vec3& t1)
{
	if (fabsf(n[2]) > 0.707f)
	{
		float a = n[1] * n[1] + n[2] * n[2];
		float k = 1.0f / sqrtf(a);
		t0[0] = 0;
		t0[1] = -n[2] * k;
		t0[2] = n[1] * k;

		t1[0] = a * k;
		t1[1] = -n[0] * t0[2];
		t1[2] = n[0] * t0[1];
	}
	else
	{
		float a = n[0] * n[0] + n[1] * n[1];
		float k = 1.0f / sqrtf(a);
		t0[0] = -n[1] * k;
		t0[1] = n[0] * k;
		t0[2] = 0;

		t1[0] = -n[2] * t0[1];
		t1[1] = n[2] * t0[0];
		t1[2] = a * k;
	}
}

static float GetRandom(float Min, float Max) { return ((float(rand()) / float(RAND_MAX)) * (Max - Min)) + Min; }

TressFXAsset::TressFXAsset():
	m_positions(NULL),
	m_strandUV(NULL),
	m_refVectors(NULL),
	m_globalRotations(NULL),
	m_localRotations(NULL),
	m_tangents(NULL),
	m_followRootOffsets(NULL),
	m_strandTypes(NULL),
	m_thicknessCoeffs(NULL),
	m_restLengths(NULL),
	m_triangleIndices(NULL),
	m_boneSkinningData(NULL),
	m_numTotalStrands(0),
	m_numTotalVertices(0),
	m_numVerticesPerStrand(0),
	m_numGuideStrands(0),
	m_numGuideVertices(0),
	m_numFollowStrandsPerGuide(0)

{
}

TressFXAsset::~TressFXAsset() { Clear(); }

void TressFXAsset::Clear()
{
	m_numTotalStrands = 0;
	m_numTotalVertices = 0;
	m_numVerticesPerStrand = 0;
	m_numGuideStrands = 0;
	m_numGuideVertices = 0;
	m_numFollowStrandsPerGuide = 0;

	conf_free(m_positions);
	conf_free(m_strandUV);
	conf_free(m_refVectors);
	conf_free(m_globalRotations);
	conf_free(m_localRotations);
	conf_free(m_tangents);
	conf_free(m_followRootOffsets);
	conf_free(m_strandTypes);
	conf_free(m_thicknessCoeffs);
	conf_free(m_restLengths);
	conf_free(m_triangleIndices);
	conf_free(m_boneSkinningData);
}

bool TressFXAsset::LoadHairData(File* ioObject)
{
	// Clear all data before loading an asset.
	Clear();

	TressFXTFXFileHeader v4Header = {};

	// read the v4Header
	ioObject->Seek(0);    // make sure the stream pos is at the beginning.
	ioObject->Read((void*)&v4Header, sizeof(TressFXTFXFileHeader));

	// If the tfx version is lower than the current major version, exit.
	if (v4Header.version == AMD_TRESSFX_V4)
		return LoadV4(ioObject, &v4Header);

	TressFXFileObject v3Header = {};
	ioObject->Seek(0);
	ioObject->Read((void*)&v3Header, sizeof(TressFXFileObject));

	if (v3Header.version == AMD_TRESSFX_V3)
		return LoadV3(ioObject, &v3Header);

	return false;
}

bool TressFXAsset::LoadV4(File* ioObject, TressFXTFXFileHeader* header)
{
	unsigned int numStrandsInFile = header->numHairStrands;

	// We make the number of strands be multiple of TRESSFX_SIM_THREAD_GROUP_SIZE.
	m_numGuideStrands = (numStrandsInFile - numStrandsInFile % TRESSFX_SIM_THREAD_GROUP_SIZE) + TRESSFX_SIM_THREAD_GROUP_SIZE;

	m_numVerticesPerStrand = header->numVerticesPerStrand;

	// Make sure number of vertices per strand is greater than two and less than or equal to
	// thread group size (64). Also thread group size should be a mulitple of number of
	// vertices per strand. So possible number is 4, 8, 16, 32 and 64.
	ASSERT(
		m_numVerticesPerStrand > 2 && m_numVerticesPerStrand <= TRESSFX_SIM_THREAD_GROUP_SIZE &&
		TRESSFX_SIM_THREAD_GROUP_SIZE % m_numVerticesPerStrand == 0);

	m_numFollowStrandsPerGuide = 0;
	m_numTotalStrands =
		m_numGuideStrands;    // Until we call GenerateFollowHairs, the number of total strands is equal to the number of guide strands.
	m_numGuideVertices = m_numGuideStrands * m_numVerticesPerStrand;
	m_numTotalVertices = m_numGuideVertices;    // Again, the total number of vertices is equal to the number of guide vertices here.

	ASSERT(m_numTotalVertices % TRESSFX_SIM_THREAD_GROUP_SIZE == 0);    // number of total vertices should be multiple of thread group size.
		// This assert is actually redundant because we already made m_numGuideStrands
		// and m_numTotalStrands are multiple of thread group size.
		// Just demonstrating the requirement for number of vertices here in case
		// you are to make your own loader.

	m_positions = conf_placement_new<float4>(conf_malloc(
		m_numTotalVertices * sizeof(float4)));    // size of m_positions = number of total vertices * sizeo of each position vector.

	if (!m_positions)
	{
		Clear();
		return false;
	}

	// Read position data from the io stream.
	ioObject->Seek(header->offsetVertexPosition);
	ioObject->Read(
		(void*)m_positions,
		numStrandsInFile * m_numVerticesPerStrand *
			sizeof(float4));    // note that the position data in io stream contains only guide hairs. If we call GenerateFollowHairs
								// to generate follow hairs, m_positions will be re-allocated.

	// We need to make up some strands to fill up the buffer because the number of strands from stream is not necessarily multile of thread size.
	int numStrandsToMakeUp = m_numGuideStrands - numStrandsInFile;

	for (int i = 0; i < numStrandsToMakeUp; ++i)
	{
		for (int j = 0; j < m_numVerticesPerStrand; ++j)
		{
			int indexLastVertex = (numStrandsInFile - 1) * m_numVerticesPerStrand + j;
			int indexVertex = (numStrandsInFile + i) * m_numVerticesPerStrand + j;
			m_positions[indexVertex] = m_positions[indexLastVertex];
		}
	}

	// Read strand UVs
	ioObject->Seek(header->offsetStrandUV);
	m_strandUV = conf_placement_new<float2>(
		conf_malloc(m_numTotalStrands * sizeof(float2)));    // If we call GenerateFollowHairs to generate follow hairs,
															 // m_strandUV will be re-allocated.

	if (!m_strandUV)
	{
		// If we have failed to allocate memory, then clear all buffers and exit.
		Clear();
		return false;
	}

	ioObject->Read((void*)m_strandUV, numStrandsInFile * sizeof(float2));

	// Fill up the last empty space
	int indexLastStrand = (numStrandsInFile - 1);

	for (int i = 0; i < numStrandsToMakeUp; ++i)
	{
		int indexStrand = (numStrandsInFile + i);
		m_strandUV[indexStrand] = m_strandUV[indexLastStrand];
	}

	m_followRootOffsets = conf_placement_new<float4>(conf_malloc(m_numTotalStrands * sizeof(float4)));

	// Fill m_followRootOffsets with zeros
	memset(m_followRootOffsets, 0, m_numTotalStrands * sizeof(float4));

	// If we have failed to allocate buffers, then clear the allocated ones and exit.
	if (!m_followRootOffsets)
	{
		Clear();
		return false;
	}

	return true;
}

bool TressFXAsset::LoadV3(File* ioObject, TressFXFileObject* header)
{
	uint numStrandsInFile = header->numGuideHairStrands;
	m_numVerticesPerStrand = header->numVerticesPerStrand;
	bool bothEndsImmovable = header->bothEndsImmovable != 0;

	// Make sure number of vertices per strand is greater than two and less than or equal to
	// thread group size (64). Also thread group size should be a mulitple of number of
	// vertices per strand. So possible number is 4, 8, 16, 32 and 64.
	ASSERT(
		m_numVerticesPerStrand > 2 && m_numVerticesPerStrand <= TRESSFX_SIM_THREAD_GROUP_SIZE &&
		TRESSFX_SIM_THREAD_GROUP_SIZE % m_numVerticesPerStrand == 0);

	// number of strands to load from tfx file.
	m_numGuideStrands = (numStrandsInFile - numStrandsInFile % TRESSFX_SIM_THREAD_GROUP_SIZE) + TRESSFX_SIM_THREAD_GROUP_SIZE;

	m_numFollowStrandsPerGuide = 0;
	m_numTotalStrands =
		m_numGuideStrands;    // Until we call GenerateFollowHairs, the number of total strands is equal to the number of guide strands.
	m_numGuideVertices = m_numGuideStrands * m_numVerticesPerStrand;
	m_numTotalVertices = m_numGuideVertices;    // Again, the total number of vertices is equal to the number of guide vertices here.

	ASSERT(m_numTotalVertices % TRESSFX_SIM_THREAD_GROUP_SIZE == 0);    // number of total vertices should be multiple of thread group size.
		// This assert is actually redundant because we already made m_numGuideStrands
		// and m_numTotalStrands are multiple of thread group size.
		// Just demonstrating the requirement for number of vertices here in case
		// you are to make your own loader.

	m_positions = conf_placement_new<float4>(conf_malloc(
		m_numTotalVertices * sizeof(float4)));    // size of m_positions = number of total vertices * sizeo of each position vector.
	if (!m_positions)
	{
		Clear();
		return false;
	}

	float3* vertexData = conf_placement_new<float3>(conf_malloc(m_numTotalVertices * sizeof(float3)));
	if (!vertexData)
	{
		Clear();
		return false;
	}

	ioObject->Seek(header->verticesOffset);
	ioObject->Read(
		(void*)vertexData,
		numStrandsInFile * m_numVerticesPerStrand *
			sizeof(float3));    // note that the position data in io stream contains only guide hairs. If we call GenerateFollowHairs
								// to generate follow hairs, m_positions will be re-allocated.

	// Copy vec3 vertex data to vec4 array
	for (int i = 0; i < m_numGuideVertices; ++i)
		m_positions[i] = float4(
			vertexData[i], (i % m_numVerticesPerStrand) < 2 ? 0.0f : 1.0f);    // First two vertices of a strand are immovable (w == 0)

	if (header->bothEndsImmovable)
	{
		for (int i = 0; i < m_numGuideStrands; ++i)
		{
			int end = (i + 1) * m_numVerticesPerStrand - 1;
			m_positions[end].setW(0.0f);
		}
	}

	conf_free(vertexData);

	int numStrandsToMakeUp = m_numGuideStrands - numStrandsInFile;

	for (int i = 0; i < numStrandsToMakeUp; ++i)
	{
		for (int j = 0; j < m_numVerticesPerStrand; ++j)
		{
			int indexLastVertex = (numStrandsInFile - 1) * m_numVerticesPerStrand + j;
			int indexVertex = (numStrandsInFile + i) * m_numVerticesPerStrand + j;
			m_positions[indexVertex] = m_positions[indexLastVertex];
		}
	}

	// Read strand UVs
	bool usingPerStrandTexCoords = header->perStrandTexCoordOffset != 0;

	m_strandUV = conf_placement_new<float2>(
		conf_malloc(m_numTotalStrands * sizeof(float2)));    // If we call GenerateFollowHairs to generate follow hairs,
															 // m_strandUV will be re-allocated.
	if (!m_strandUV)
	{
		Clear();
		return false;
	}

	if (usingPerStrandTexCoords)
	{
		ioObject->Seek(header->perStrandTexCoordOffset);
		ioObject->Read((void*)m_strandUV, numStrandsInFile * sizeof(float2));
	}
	else
		memset(m_strandUV, 0, numStrandsInFile * sizeof(float2));

	// Fill up the last empty space
	int indexLastStrand = (numStrandsInFile - 1);

	for (int i = 0; i < numStrandsToMakeUp; ++i)
	{
		int indexStrand = (numStrandsInFile + i);
		m_strandUV[indexStrand] = m_strandUV[indexLastStrand];
	}

	m_followRootOffsets = conf_placement_new<float4>(conf_malloc(m_numTotalStrands * sizeof(float4)));

	// Fill m_followRootOffsets with zeros
	memset(m_followRootOffsets, 0, m_numTotalStrands * sizeof(float4));

	// If we have failed to allocate buffers, then clear the allocated ones and exit.
	if (!m_followRootOffsets)
	{
		Clear();
		return false;
	}

	return true;
}

// This generates follow hairs around loaded guide hairs procedually with random distribution within the max radius input.
// Calling this is optional.
bool TressFXAsset::GenerateFollowHairs(int numFollowHairsPerGuideHair, float tipSeparationFactor, float maxRadiusAroundGuideHair)
{
	ASSERT(numFollowHairsPerGuideHair >= 0);

	m_numFollowStrandsPerGuide = numFollowHairsPerGuideHair;

	// Nothing to do, just exit.
	if (numFollowHairsPerGuideHair == 0)
		return false;

	// Recompute total number of hair strands and vertices with considering number of follow hairs per a guide hair.
	m_numTotalStrands = m_numGuideStrands * (m_numFollowStrandsPerGuide + 1);
	m_numTotalVertices = m_numTotalStrands * m_numVerticesPerStrand;

	// keep the old buffers until the end of this function.
	float4* positionsGuide = m_positions;
	float2* strandUVGuide = m_strandUV;

	// re-allocate all buffers
	m_positions = conf_placement_new<float4>(conf_malloc(m_numTotalVertices * sizeof(float4)));
	m_strandUV = conf_placement_new<float2>(conf_malloc(m_numTotalStrands * sizeof(float2)));

	conf_free(m_followRootOffsets);
	m_followRootOffsets = conf_placement_new<float4>(conf_malloc(m_numTotalStrands * sizeof(float4)));

	// If we have failed to allocate buffers, then clear the allocated ones and exit.
	if (!m_positions || !m_strandUV || !m_followRootOffsets)
	{
		Clear();
		return false;
	}

	// type-cast to vec3 to handle data easily.
	ASSERT(sizeof(vec3) == sizeof(float4));    // sizeof(vec3) is 4*sizeof(float)
	vec3* pos = static_cast<vec3*>((void*)m_positions);
	vec3* followOffset = static_cast<vec3*>((void*)m_followRootOffsets);

	// Generate follow hairs
	for (int i = 0; i < m_numGuideStrands; i++)
	{
		int indexGuideStrand = i * (m_numFollowStrandsPerGuide + 1);
		int indexRootVertMaster = indexGuideStrand * m_numVerticesPerStrand;

		memcpy(&pos[indexRootVertMaster], &positionsGuide[i * m_numVerticesPerStrand], sizeof(vec3) * m_numVerticesPerStrand);
		m_strandUV[indexGuideStrand] = strandUVGuide[i];

		followOffset[indexGuideStrand] = vec3(0, 0, 0);
		followOffset[indexGuideStrand].setW((float)indexGuideStrand);
		vec3 v01 = pos[indexRootVertMaster + 1] - pos[indexRootVertMaster];
		v01 = normalize(v01);

		// Find two orthogonal unit tangent vectors to v01
		vec3 t0, t1;
		GetTangentVectors(v01, t0, t1);

		for (int j = 0; j < m_numFollowStrandsPerGuide; j++)
		{
			int indexStrandFollow = indexGuideStrand + j + 1;
			int indexRootVertFollow = indexStrandFollow * m_numVerticesPerStrand;

			m_strandUV[indexStrandFollow] = m_strandUV[indexGuideStrand];

			// offset vector from the guide strand's root vertex position
			vec3 offset = GetRandom(-maxRadiusAroundGuideHair, maxRadiusAroundGuideHair) * t0 +
						  GetRandom(-maxRadiusAroundGuideHair, maxRadiusAroundGuideHair) * t1;
			followOffset[indexStrandFollow] = offset;
			followOffset[indexStrandFollow].setW((float)indexGuideStrand);

			for (int k = 0; k < m_numVerticesPerStrand; k++)
			{
				const vec3* guideVert = &pos[indexRootVertMaster + k];
				vec3*       followVert = &pos[indexRootVertFollow + k];

				float factor = tipSeparationFactor * ((float)k / ((float)m_numVerticesPerStrand)) + 1.0f;
				*followVert = *guideVert + offset * factor;
				(*followVert).setW(guideVert->getW());
			}
		}
	}

	conf_free(positionsGuide);
	conf_free(strandUVGuide);

	return true;
}

bool TressFXAsset::ProcessAsset()
{
	conf_free(m_strandTypes);
	m_strandTypes = conf_placement_new<int>(conf_malloc(m_numTotalStrands * sizeof(int)));

	conf_free(m_tangents);
	m_tangents = conf_placement_new<float4>(conf_malloc(m_numTotalVertices * sizeof(float4)));

	conf_free(m_restLengths);
	m_restLengths = conf_placement_new<float>(conf_malloc(m_numTotalVertices * sizeof(float)));

	conf_free(m_refVectors);
	m_refVectors = conf_placement_new<float4>(conf_malloc(m_numTotalVertices * sizeof(float4)));

	conf_free(m_globalRotations);
	m_globalRotations = conf_placement_new<float4>(conf_malloc(m_numTotalVertices * sizeof(float4)));

	conf_free(m_localRotations);
	m_localRotations = conf_placement_new<float4>(conf_malloc(m_numTotalVertices * sizeof(float4)));

	conf_free(m_thicknessCoeffs);
	m_thicknessCoeffs = conf_placement_new<float>(conf_malloc(m_numTotalVertices * sizeof(float)));

	conf_free(m_triangleIndices);
	m_triangleIndices = conf_placement_new<int>(conf_malloc(GetNumHairTriangleIndices() * sizeof(int32)));

	// If we have failed to allocate buffers, then clear the allocated ones and exit.
	if (!m_strandTypes || !m_tangents || !m_restLengths || !m_refVectors || !m_globalRotations || !m_localRotations || !m_thicknessCoeffs ||
		!m_triangleIndices)
	{
		Clear();
		return false;
	}

	// construct local and global transforms for each hair strand.
	ComputeTransforms();

	// compute tangent vectors
	ComputeStrandTangent();

	// compute thickness coefficients
	ComputeThicknessCoeffs();

	// compute rest lengths
	ComputeRestLengths();

	// triangle index
	FillTriangleIndexArray();

	for (int i = 0; i < m_numTotalStrands; i++)
		m_strandTypes[i] = 0;

	return true;
}

void TressFXAsset::FillTriangleIndexArray()
{
	ASSERT(m_numTotalVertices == m_numTotalStrands * m_numVerticesPerStrand);
	ASSERT(m_triangleIndices != nullptr);

	int id = 0;
	int iCount = 0;

	for (int i = 0; i < m_numTotalStrands; i++)
	{
		for (int j = 0; j < m_numVerticesPerStrand - 1; j++)
		{
			m_triangleIndices[iCount++] = 2 * id;
			m_triangleIndices[iCount++] = 2 * id + 1;
			m_triangleIndices[iCount++] = 2 * id + 2;
			m_triangleIndices[iCount++] = 2 * id + 2;
			m_triangleIndices[iCount++] = 2 * id + 1;
			m_triangleIndices[iCount++] = 2 * id + 3;

			id++;
		}

		id++;
	}

	ASSERT(iCount == 6 * m_numTotalStrands * (m_numVerticesPerStrand - 1));    // iCount == GetNumHairTriangleIndices()
}

void TressFXAsset::ComputeStrandTangent()
{
	vec3* pos = (vec3*)m_positions;
	vec3* tan = (vec3*)m_tangents;

	for (int iStrand = 0; iStrand < m_numTotalStrands; ++iStrand)
	{
		int indexRootVertMaster = iStrand * m_numVerticesPerStrand;

		// vertex 0
		{
			vec3& vert_0 = pos[indexRootVertMaster];
			vec3& vert_1 = pos[indexRootVertMaster + 1];

			vec3 tangent = vert_1 - vert_0;
			tangent = normalize(tangent);
			tan[indexRootVertMaster] = tangent;
		}

		// vertex 1 through n-1
		for (int i = 1; i < (int)m_numVerticesPerStrand - 1; i++)
		{
			vec3& vert_i_minus_1 = pos[indexRootVertMaster + i - 1];
			vec3& vert_i = pos[indexRootVertMaster + i];
			vec3& vert_i_plus_1 = pos[indexRootVertMaster + i + 1];

			vec3 tangent_pre = vert_i - vert_i_minus_1;
			tangent_pre = normalize(tangent_pre);

			vec3 tangent_next = vert_i_plus_1 - vert_i;
			tangent_next = normalize(tangent_next);

			vec3 tangent = tangent_pre + tangent_next;
			tangent = normalize(tangent);

			tan[indexRootVertMaster + i] = tangent;
		}
	}
}

void TressFXAsset::ComputeThicknessCoeffs()
{
	vec3* pos = (vec3*)m_positions;

	int   index = 0;
	float tValues[TRESSFX_SIM_THREAD_GROUP_SIZE] = { 0.0f };

	for (int iStrand = 0; iStrand < m_numTotalStrands; ++iStrand)
	{
		int   indexRootVertMaster = iStrand * m_numVerticesPerStrand;
		float strandLength = 0;
		float tVal = 0;

		// vertex 1 through n
		for (int i = 1; i < (int)m_numVerticesPerStrand; ++i)
		{
			vec3& vert_i_minus_1 = pos[indexRootVertMaster + i - 1];
			vec3& vert_i = pos[indexRootVertMaster + i];

			vec3  vec = vert_i - vert_i_minus_1;
			float disSeg = length(vec);

			tVal += disSeg;
			tValues[i] = tVal;
			strandLength += disSeg;
		}

		for (int i = 0; i < (int)m_numVerticesPerStrand; ++i)
		{
			tVal = tValues[i] / strandLength;
			m_thicknessCoeffs[index++] = sqrtf(1.f - tVal * tVal);
		}
	}
}

void TressFXAsset::ComputeRestLengths()
{
	vec3*  pos = (vec3*)m_positions;
	float* restLen = (float*)m_restLengths;

	int index = 0;

	// Calculate rest lengths
	for (int i = 0; i < m_numTotalStrands; i++)
	{
		int indexRootVert = i * m_numVerticesPerStrand;

		for (int j = 0; j < m_numVerticesPerStrand - 1; j++)
		{
			restLen[index++] = length(pos[indexRootVert + j] - pos[indexRootVert + j + 1]);
		}

		// Since number of edges are one less than number of vertices in hair strand, below
		// line acts as a placeholder.
		restLen[index++] = 0;
	}
}

void TressFXAsset::ComputeTransforms()
{
	vec3* pos = (vec3*)m_positions;
	Quat* globalRot = (Quat*)m_globalRotations;
	Quat* localRot = (Quat*)m_localRotations;
	vec3* ref = (vec3*)m_refVectors;

	// construct local and global transforms for all hair strands
	for (int iStrand = 0; iStrand < m_numTotalStrands; ++iStrand)
	{
		int indexRootVertMaster = iStrand * m_numVerticesPerStrand;

		// vertex 0
		{
			vec3& vert_i = pos[indexRootVertMaster];
			vec3& vert_i_plus_1 = pos[indexRootVertMaster + 1];

			const vec3 vec = vert_i_plus_1 - vert_i;
			vec3       vecX = normalize(vec);

			vec3 vecZ = cross(vecX, vec3(1, 0, 0));

			if (lengthSqr(vecZ) < 0.0001f)
			{
				vecZ = cross(vecX, vec3(0, 1.0f, 0));
			}

			vecZ = normalize(vecZ);
			vec3 vecY = normalize(cross(vecZ, vecX));

			mat3 rotL2W = {};

			rotL2W[0][0] = vecX.getX();
			rotL2W[1][0] = vecY.getX();
			rotL2W[2][0] = vecZ.getX();
			rotL2W[0][1] = vecX.getY();
			rotL2W[1][1] = vecY.getY();
			rotL2W[2][1] = vecZ.getY();
			rotL2W[0][2] = vecX.getZ();
			rotL2W[1][2] = vecY.getZ();
			rotL2W[2][2] = vecZ.getZ();

			Quat rot = Quat(rotL2W);
			localRot[indexRootVertMaster] = globalRot[indexRootVertMaster] =
				rot;    // For vertex 0, local and global transforms are the same.
		}

		// vertex 1 through n-1
		for (int i = 1; i < (int)m_numVerticesPerStrand; i++)
		{
			vec3& vert_i_minus_1 = pos[indexRootVertMaster + i - 1];
			vec3& vert_i = pos[indexRootVertMaster + i];
			vec3  vec = vert_i - vert_i_minus_1;
			vec = rotate(inverse(globalRot[indexRootVertMaster + i - 1]), vec);

			vec3  vecX = normalize(vec);
			vec3  X = vec3(1.0f, 0, 0);
			vec3  rotAxis = cross(X, vecX);
			float angle = acos(dot(X, vecX));

			if (abs(angle) < 0.001 || lengthSqr(rotAxis) < 0.001)
			{
				localRot[indexRootVertMaster + i] = Quat::identity();
			}
			else
			{
				rotAxis = normalize(rotAxis);
				Quat rot = Quat::rotation(angle, rotAxis);
				localRot[indexRootVertMaster + i] = rot;
			}

			globalRot[indexRootVertMaster + i] = globalRot[indexRootVertMaster + i - 1] * localRot[indexRootVertMaster + i];
			ref[indexRootVertMaster + i] = vec;
		}
	}
}
}    // Namespace AMD

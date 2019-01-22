/*
*Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
*
*Permission is hereby granted, free of charge, to any person obtaining a copy
*of this software and associated documentation files (the "Software"), to deal
*in the Software without restriction, including without limitation the rights
*to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*copies of the Software, and to permit persons to whom the Software is
*furnished to do so, subject to the following conditions:
*
*The above copyright notice and this permission notice shall be included in
*all copies or substantial portions of the Software.
*
*THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
*AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
*THE SOFTWARE.
*/

#include "TFXImporter.h"
#include <cstdlib>
#include "../../OS/Interfaces/IMemoryManager.h"
#include "TressFXAsset.h"

bool TFXImporter::ImportTFX(
	const char* filename, FSRoot root, int numFollowHairs, float tipSeperationFactor, float maxRadiusAroundGuideHair, TFXAsset* tfxAsset)
{
	File file = {};
	if (!file.Open(filename, FileMode::FM_ReadBinary, root))
		return false;

	AMD::TressFXAsset tressFXAsset = {};
	if (!tressFXAsset.LoadHairData(&file))
		return false;

	if (numFollowHairs > 0)
	{
		if (!tressFXAsset.GenerateFollowHairs(numFollowHairs, tipSeperationFactor, maxRadiusAroundGuideHair))
			return false;
	}

	if (!tressFXAsset.ProcessAsset())
		return false;

	tfxAsset->mPositions.resize(tressFXAsset.m_numTotalVertices);
	memcpy(tfxAsset->mPositions.data(), tressFXAsset.m_positions, sizeof(float4) * tressFXAsset.m_numTotalVertices);
	tfxAsset->mStrandUV.resize(tressFXAsset.m_numTotalStrands);
	memcpy(tfxAsset->mStrandUV.data(), tressFXAsset.m_strandUV, sizeof(float2) * tressFXAsset.m_numTotalStrands);
	tfxAsset->mRefVectors.resize(tressFXAsset.m_numTotalVertices);
	memcpy(tfxAsset->mRefVectors.data(), tressFXAsset.m_refVectors, sizeof(float4) * tressFXAsset.m_numTotalVertices);
	tfxAsset->mGlobalRotations.resize(tressFXAsset.m_numTotalVertices);
	memcpy(tfxAsset->mGlobalRotations.data(), tressFXAsset.m_globalRotations, sizeof(float4) * tressFXAsset.m_numTotalVertices);
	tfxAsset->mLocalRotations.resize(tressFXAsset.m_numTotalVertices);
	memcpy(tfxAsset->mLocalRotations.data(), tressFXAsset.m_localRotations, sizeof(float4) * tressFXAsset.m_numTotalVertices);
	tfxAsset->mTangents.resize(tressFXAsset.m_numTotalVertices);
	memcpy(tfxAsset->mTangents.data(), tressFXAsset.m_tangents, sizeof(float4) * tressFXAsset.m_numTotalVertices);
	tfxAsset->mFollowRootOffsets.resize(tressFXAsset.m_numTotalVertices);
	memcpy(tfxAsset->mFollowRootOffsets.data(), tressFXAsset.m_followRootOffsets, sizeof(float4) * tressFXAsset.m_numTotalStrands);
	tfxAsset->mStrandTypes.resize(tressFXAsset.m_numTotalStrands);
	memcpy(tfxAsset->mStrandTypes.data(), tressFXAsset.m_strandTypes, sizeof(int) * tressFXAsset.m_numTotalStrands);
	tfxAsset->mThicknessCoeffs.resize(tressFXAsset.m_numTotalVertices);
	memcpy(tfxAsset->mThicknessCoeffs.data(), tressFXAsset.m_thicknessCoeffs, sizeof(float) * tressFXAsset.m_numTotalVertices);
	tfxAsset->mRestLengths.resize(tressFXAsset.m_numTotalVertices);
	memcpy(tfxAsset->mRestLengths.data(), tressFXAsset.m_restLengths, sizeof(float) * tressFXAsset.m_numTotalVertices);
	int numIndices = tressFXAsset.GetNumHairTriangleIndices();
	tfxAsset->mTriangleIndices.resize(numIndices);
	memcpy(tfxAsset->mTriangleIndices.data(), tressFXAsset.m_triangleIndices, sizeof(int) * numIndices);
	tfxAsset->mNumVerticesPerStrand = tressFXAsset.m_numVerticesPerStrand;
	tfxAsset->mNumGuideStrands = tressFXAsset.m_numGuideStrands;

	return true;
}

bool TFXImporter::ImportTFXMesh(const char* filename, FSRoot root, TFXMesh* tfxMesh)
{
	File file = {};
	if (!file.Open(filename, FileMode::FM_ReadBinary, root))
		return false;

	tinystl::vector<tinystl::string> splitLine;
	uint                             numOfBones = 0;
	uint                             bonesFound = 0;
	uint                             numOfVertices = 0;
	uint                             verticesFound = 0;
	uint                             numOfTriangles = 0;
	uint                             trianglesFound = 0;

	while (!file.IsEof())
	{
		tinystl::string line = file.ReadLine();

		if (line[0] == '#')
			continue;

		splitLine = line.split(' ');
		if (splitLine.empty())
			continue;

		if (numOfBones == 0 || bonesFound != numOfBones)
		{
			if (splitLine[0] == "numOfBones")
			{
				numOfBones = atoi(splitLine[1].c_str());
				tfxMesh->mBones.resize(numOfBones);
				continue;
			}

			if (bonesFound != numOfBones)
			{
				uint boneIndex = atoi(splitLine[0].c_str());
				tfxMesh->mBones[boneIndex] = splitLine[1];
				++bonesFound;
				continue;
			}
		}

		if (numOfVertices == 0 || verticesFound != numOfVertices)
		{
			if (splitLine[0] == "numOfVertices")
			{
				numOfVertices = atoi(splitLine[1].c_str());
				tfxMesh->mVertices.resize(numOfVertices);
				continue;
			}

			if (verticesFound != numOfVertices)
			{
				uint vertexIndex = atoi(splitLine[0].c_str());
				tfxMesh->mVertices[vertexIndex].mPosition =
					float3((float)atof(splitLine[1].c_str()), (float)atof(splitLine[2].c_str()), (float)atof(splitLine[3].c_str()));
				tfxMesh->mVertices[vertexIndex].mNormal =
					float3((float)atof(splitLine[4].c_str()), (float)atof(splitLine[5].c_str()), (float)atof(splitLine[6].c_str()));
				for (int i = 0; i < 4; ++i)
					tfxMesh->mVertices[vertexIndex].mBoneIndices[i] = atoi(splitLine[7 + i].c_str());
				for (int i = 0; i < 4; ++i)
					tfxMesh->mVertices[vertexIndex].mBoneWeights[i] = (float)atof(splitLine[11 + i].c_str());
				++verticesFound;
			}
		}

		if (numOfTriangles == 0 || trianglesFound != numOfTriangles)
		{
			if (splitLine[0] == "numOfTriangles")
			{
				numOfTriangles = atoi(splitLine[1].c_str());
				tfxMesh->mIndices.resize(numOfTriangles * 3);
				continue;
			}

			if (trianglesFound != numOfTriangles)
			{
				tfxMesh->mIndices[trianglesFound * 3 + 0] = atoi(splitLine[1].c_str());
				tfxMesh->mIndices[trianglesFound * 3 + 1] = atoi(splitLine[2].c_str());
				tfxMesh->mIndices[trianglesFound * 3 + 2] = atoi(splitLine[3].c_str());
				++trianglesFound;
				continue;
			}
		}
	}

	if (numOfBones != bonesFound)
		return false;

	if (numOfVertices != verticesFound)
		return false;

	if (numOfTriangles != trianglesFound)
		return false;

	return true;
}
/*
 * Copyright (c) 2018 Kostas Anagnostou (https://twitter.com/KostasAAA).
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

#define _USE_MATH_DEFINES

// Unit Test for testing Hybrid Raytracing
// based on https://interplayoflight.wordpress.com/2018/07/04/hybrid-raytraced-shadows-and-reflections/

//EASTL includes
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/unordered_map.h"

//Interfaces
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Application/Interfaces/IApp.h"

#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

//Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

//ui
#include "../../../../Common_3/Application/Interfaces/IUI.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

Timer      gAccumTimer;

struct AABBox
{
	vec3  MinBounds;
	vec3  MaxBounds;
	vec3  Centre;
	vec3  Vertex0;
	vec3  Vertex1;
	vec3  Vertex2;
	int   InstanceID;
	float SurfaceAreaLeft;
	float SurfaceAreaRight;

	AABBox()
	{
		MinBounds = vec3(FLT_MAX);
		MaxBounds = vec3(-FLT_MAX);
		InstanceID = 0;
		SurfaceAreaLeft = 0.0f;
		SurfaceAreaRight = 0.0f;
	}

	void Expand(vec3& point)
	{
		MinBounds = min(MinBounds, point);
		MaxBounds = max(MaxBounds, point);

		Centre = 0.5f * (MaxBounds + MinBounds);
	}

	void Expand(AABBox& aabox)
	{
		Expand(aabox.MinBounds);
		Expand(aabox.MaxBounds);
	}
};

struct BVHNodeBBox
{
	float4 MinBounds;    // OffsetToNextNode in w component
	float4 MaxBounds;
};

struct BVHLeafBBox
{
	float4 Vertex0;    // OffsetToNextNode in w component
	float4 Vertex1MinusVertex0;
	float4 Vertex2MinusVertex0;
};

struct BVHNode
{
	float    SplitCost;
	AABBox   BoundingBox;
	BVHNode* Left;
	BVHNode* Right;
};

const uint32_t gImageCount = 3;

enum Enum
{
	GBuffer,
	Lighting,
	Composite,
	RaytracedShadows,
	//RaytractedReflections,
	CopyToBackbuffer,
	RenderPassCount
};

Shader* pShader[RenderPassCount];

RootSignature* pRootSignature;
RootSignature* pRootSignatureComp;

uint32_t gMapIDRootConstantIndex = 0;

DescriptorSet* pDescriptorSetNonFreq;
DescriptorSet* pDescriptorSetFreq;
DescriptorSet* pDescriptorSetFreqPerDraw;
DescriptorSet* pDescriptorSetCompNonFreq;
DescriptorSet* pDescriptorSetCompFreq;

Pipeline* pPipeline[RenderPassCount];

CmdPool* pCmdPools[gImageCount];
Cmd* pCmds[gImageCount];

Buffer* pGPrepassUniformBuffer[gImageCount];
Buffer* pShadowpassUniformBuffer[gImageCount];
Buffer* pLightpassUniformBuffer[gImageCount];

enum Gbuffers
{
	Albedo,
	Normal,
	GbufferCount
};

RenderTarget* pRenderTargets[GbufferCount] = { NULL };
RenderTarget* pDepthBuffer = NULL;

enum Textures
{
	Texture_Lighting,
	Texture_RaytracedShadows,
	Texture_Composite,	
	TextureCount
};

Texture*	pTextures[TextureCount] = { NULL };


// Per pass data
struct DefaultpassUniformBuffer
{
	vec4 mRTSize;
};

struct GPrepassUniformBuffer
{
	mat4 mProjectView;
};

struct ShadowpassUniformBuffer
{
	mat4 mProjectView;
	mat4 mInvProjectView;
	vec4 mRTSize;
	vec4 mLightDir;
	vec4 mCameraPosition;
};

struct LightpassUniformBuffer
{
	mat4 mProjectView;
	mat4 mInvProjectView;
	vec4 mRTSize;
	vec4 mLightDir;
	vec4 mCameraPosition;
};

// Have a uniform for object data
struct UniformObjData
{
	mat4  mWorldMat;
	float mRoughness = 0.04f;
	float mMetallic = 0.0f;
	int   pbrMaterials = -1;
	int32_t : 32; // padding
};

//Structure of Array for vertex/index data
struct MeshBatch
{
	eastl::vector<float3> PositionsData;
	eastl::vector<uint>   IndicesData;

	Buffer* pPositionStream;
	Buffer* pNormalStream;
	Buffer* pUVStream;
	Buffer* pIndicesStream;
	int     NoofVertices;
	int     NoofIndices;
	int     NoofInstances;
	int     MaterialID;
};

struct PropData
{
	mat4                        WorldMatrix;
	Geometry* Geom;
	Buffer* pConstantBuffer;
};

Renderer* pRenderer = NULL;
Queue* pGraphicsQueue = NULL;

Sampler* pSamplerPointClamp = NULL;
Sampler* pSamplerLinearWrap = NULL;

Fence* pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

SwapChain* pSwapChain = NULL;

Buffer* BVHBoundingBoxesBuffer;

//The render passes used in the demo
//RenderPassMap            RenderPasses;
GPrepassUniformBuffer    gPrepasUniformData;
ShadowpassUniformBuffer  gShadowPassUniformData;
LightpassUniformBuffer   gLightPassUniformData;
DefaultpassUniformBuffer gDefaultPassUniformData;

uint32_t gFrameIndex = 0;

UIComponent* pGuiWindow = NULL;
ICameraController* pCameraController = NULL;

float gLightRotationX = 0.0f;
float gLightRotationZ = 0.0f;

// Create vertex layout for g-prepass
VertexLayout gVertexLayoutGPrepass = {};

uint32_t gMaterialIds[] =
{
	0, 3, 1, 4, 5, 6, 7, 8, 6, 9, 7, 6, 10, 5, 7, 5, 6,
	7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7,
	6, 5, 6, 5, 11, 5, 11, 5, 11, 5, 10, 5, 9, 8, 6, 12,
	2, 5, 13, 0, 14, 15, 16, 14, 15, 14, 16, 15, 13, 17, 18,
	19, 18, 19, 18, 17, 19, 18, 17, 20, 21, 20, 21, 20, 21, 20,
	21, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 22, 23, 4, 23, 4, 5, 24, 5,
};

//Sponza
const char* gModel_Sponza_File = "Sponza.gltf";

const char* pMaterialImageFileNames[] = {
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",

	//common
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/Dielectric_metallic",
	"SponzaPBR_Textures/Metallic_metallic",
	"SponzaPBR_Textures/gi_flag",

	//Background
	"SponzaPBR_Textures/Background/Background_Albedo",
	"SponzaPBR_Textures/Background/Background_Normal",
	"SponzaPBR_Textures/Background/Background_Roughness",

	//ChainTexture
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Albedo",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Metallic",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Normal",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Roughness",

	//Lion
	"SponzaPBR_Textures/Lion/Lion_Albedo",
	"SponzaPBR_Textures/Lion/Lion_Normal",
	"SponzaPBR_Textures/Lion/Lion_Roughness",

	//Sponza_Arch
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_diffuse",
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_normal",
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_roughness",

	//Sponza_Bricks
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Albedo",
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Normal",
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Roughness",

	//Sponza_Ceiling
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_diffuse",
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_normal",
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_roughness",

	//Sponza_Column
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_roughness",

	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_roughness",

	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_roughness",

	//Sponza_Curtain
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Blue_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Blue_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Green_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Green_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Red_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Red_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_metallic",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_roughness",

	//Sponza_Details
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_diffuse",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_metallic",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_normal",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_roughness",

	//Sponza_Fabric
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Blue_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Blue_normal",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Green_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Green_normal",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_metallic",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_roughness",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Red_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Red_normal",

	//Sponza_FlagPole
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_diffuse",
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_normal",
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_roughness",

	//Sponza_Floor
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_diffuse",
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_normal",
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_roughness",

	//Sponza_Roof
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_diffuse",
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_normal",
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_roughness",

	//Sponza_Thorn
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_diffuse",
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_normal",
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_roughness",

	//Vase
	"SponzaPBR_Textures/Vase/Vase_diffuse",
	"SponzaPBR_Textures/Vase/Vase_normal",
	"SponzaPBR_Textures/Vase/Vase_roughness",

	//VaseHanging
	"SponzaPBR_Textures/VaseHanging/VaseHanging_diffuse",
	"SponzaPBR_Textures/VaseHanging/VaseHanging_normal",
	"SponzaPBR_Textures/VaseHanging/VaseHanging_roughness",

	//VasePlant
	"SponzaPBR_Textures/VasePlant/VasePlant_diffuse",
	"SponzaPBR_Textures/VasePlant/VasePlant_normal",
	"SponzaPBR_Textures/VasePlant/VasePlant_roughness",

	//VaseRound
	"SponzaPBR_Textures/VaseRound/VaseRound_diffuse",
	"SponzaPBR_Textures/VaseRound/VaseRound_normal",
	"SponzaPBR_Textures/VaseRound/VaseRound_roughness",

	"lion/lion_albedo",
	"lion/lion_specular",
	"lion/lion_normal",

};

//Workaround for too many bindless textures on iOS
//we bind textures individually for each draw call.
#ifdef TARGET_IOS
const char* pTextureName[] = { "albedoMap", "normalMap", "metallicMap", "roughnessMap", "aoMap" };

#endif

PropData SponzaProp;

#include "Shaders/Shared.h"
Texture* pMaterialTextures[TOTAL_IMGS];

eastl::vector<int> gSponzaTextureIndexforMaterial;

AABBox gWholeSceneBBox;

FontDrawDesc gFrameTimeDraw; 
uint32_t     gFontID = 0; 
ProfileToken gGpuProfileToken;

uint gFrameNumber = 0;

void addPropToPrimitivesAABBList(eastl::vector<AABBox>& bboxData, PropData& prop)
{
	mat4& world = prop.WorldMatrix;
	{
		//number of indices
		uint noofIndices = prop.Geom->mIndexCount;
		uint32_t* indices = (uint32_t*)prop.Geom->pShadow->pIndices;
		float3* positions = (float3*)prop.Geom->pShadow->pAttributes[SEMANTIC_POSITION];

		for (uint j = 0; j < noofIndices; j += 3)
		{
			AABBox bbox;

			int index0 = indices[j + 0];
			int index1 = indices[j + 1];
			int index2 = indices[j + 2];

			bbox.Vertex0 = (world * vec4(f3Tov3(positions[index0]), 1)).getXYZ();
			bbox.Vertex1 = (world * vec4(f3Tov3(positions[index1]), 1)).getXYZ();
			bbox.Vertex2 = (world * vec4(f3Tov3(positions[index2]), 1)).getXYZ();

			bbox.Expand(bbox.Vertex0);
			bbox.Expand(bbox.Vertex1);
			bbox.Expand(bbox.Vertex2);

			bbox.InstanceID = 0;

			bboxData.push_back(bbox);
		}
	}
}

void calculateBounds(eastl::vector<AABBox>& bboxData, int begin, int end, vec3& minBounds, vec3& maxBounds)
{
	minBounds = vec3(FLT_MAX);
	maxBounds = vec3(-FLT_MAX);

	for (int i = begin; i <= end; i++)
	{
		//find bounding box
		minBounds = min(bboxData[i].MinBounds, minBounds);
		maxBounds = max(bboxData[i].MaxBounds, maxBounds);
	}
}

void sortAlongAxis(eastl::vector<AABBox>& bboxData, int begin, int end, int axis)
{
#if 0    // This path is using eastl::sort which seems much slower than std::qsort.
	if (axis == 0)
	{
		bboxData.sort(begin, end, [](const AABBox& a, const AABBox& b)
			{
				const float midPointA = a.Centre[0];
				const float midPointB = b.Centre[0];

				if (midPointA < midPointB)
					return -1;
				else if (midPointA > midPointB)
					return 1;

				return 0;
			});
	}
	else if (axis == 1)
	{
		bboxData.sort(begin, end, [](const AABBox& a, const AABBox& b)
			{
				const float midPointA = a.Centre[1];
				const float midPointB = b.Centre[1];

				if (midPointA < midPointB)
					return -1;
				else if (midPointA > midPointB)
					return 1;

				return 0;
			});
	}
	else
	{
		bboxData.sort(begin, end, [](const AABBox& a, const AABBox& b)
			{
				const float midPointA = a.Centre[2];
				const float midPointB = b.Centre[2];

				if (midPointA < midPointB)
					return -1;
				else if (midPointA > midPointB)
					return 1;

				return 0;
			});
	}
#else
	AABBox* data = bboxData.data() + begin;
	int     count = end - begin + 1;

	if (axis == 0)
		std::qsort(data, count, sizeof(AABBox), [](const void* a, const void* b) {
		const AABBox* arg1 = static_cast<const AABBox*>(a);
		const AABBox* arg2 = static_cast<const AABBox*>(b);

		float midPointA = arg1->Centre[0];
		float midPointB = arg2->Centre[0];

		if (midPointA < midPointB)
			return -1;
		else if (midPointA > midPointB)
			return 1;

		return 0;
			});
	else if (axis == 1)
		std::qsort(data, count, sizeof(AABBox), [](const void* a, const void* b) {
		const AABBox* arg1 = static_cast<const AABBox*>(a);
		const AABBox* arg2 = static_cast<const AABBox*>(b);

		float midPointA = arg1->Centre[1];
		float midPointB = arg2->Centre[1];

		if (midPointA < midPointB)
			return -1;
		else if (midPointA > midPointB)
			return 1;

		return 0;
			});
	else
		std::qsort(data, count, sizeof(AABBox), [](const void* a, const void* b) {
		const AABBox* arg1 = static_cast<const AABBox*>(a);
		const AABBox* arg2 = static_cast<const AABBox*>(b);

		float midPointA = arg1->Centre[2];
		float midPointB = arg2->Centre[2];

		if (midPointA < midPointB)
			return -1;
		else if (midPointA > midPointB)
			return 1;

		return 0;
			});
#endif
}

//BVHNode* createBVHNodeMedianSplit(eastl::vector<AABBox>& bboxData, int begin, int end)
//{
//  int count = end - begin + 1;
//
//  vec3 minBounds;
//  vec3 maxBounds;
//
//  calculateBounds(bboxData, begin, end, minBounds, maxBounds);
//
//  vec3 bboxSize = maxBounds - minBounds;
//
//  float maxDim = maxElem(bboxSize);
//
//  if (maxDim == bboxSize.getX())
//	  sortAlongAxis(bboxData, begin, end, 0);
//  else if (maxDim == bboxSize.getY())
//	  sortAlongAxis(bboxData, begin, end, 1);
//  else
//	  sortAlongAxis(bboxData, begin, end, 2);
//
//  BVHNode* node = (BVHNode*)tf_placement_new<BVHNode>(tf_calloc(1, sizeof(BVHNode)));
//
//  node->BoundingBox.Expand(minBounds);
//  node->BoundingBox.Expand(maxBounds);
//
//  int split = count / 2;
//
//  if (count == 1)
//  {
//	  //this is a leaf node
//	  node->Left = NULL;
//	  node->Right = NULL;
//
//	  node->BoundingBox.InstanceID = bboxData[begin].InstanceID;
//
//	  node->BoundingBox.Vertex0 = bboxData[begin].Vertex0;
//	  node->BoundingBox.Vertex1 = bboxData[begin].Vertex1;
//	  node->BoundingBox.Vertex2 = bboxData[begin].Vertex2;
//  }
//  else
//  {
//	  //create the two branches
//	  node->Left = createBVHNodeMedianSplit(bboxData, begin, begin + split - 1);
//	  node->Right = createBVHNodeMedianSplit(bboxData, begin + split, end);
//
//	  node->BoundingBox.Vertex0 = vec3(0.0f);
//	  node->BoundingBox.Vertex1 = vec3(0.0f);
//	  node->BoundingBox.Vertex2 = vec3(0.0f);
//
//	  //this is an intermediate Node
//	  node->BoundingBox.InstanceID = -1;
//  }
//
//  return node;
//}

inline float calculateSurfaceArea(const AABBox& bbox)
{
	vec3 extents = bbox.MaxBounds - bbox.MinBounds;
	return (extents[0] * extents[1] + extents[1] * extents[2] + extents[2] * extents[0]) * 2.0f;
}

//based on https://github.com/kayru/RayTracedShadows/blob/master/Source/BVHBuilder.cpp
void findBestSplit(eastl::vector<AABBox>& bboxData, int begin, int end, int& split, int& axis, float& splitCost)
{
	int count = end - begin + 1;
	int bestSplit = begin;
	//int globalBestSplit = begin;
	splitCost = FLT_MAX;

	split = begin;
	axis = 0;

	for (int i = 0; i < 3; i++)
	{
		sortAlongAxis(bboxData, begin, end, i);

		AABBox boundsLeft;
		AABBox boundsRight;

		for (int indexLeft = 0; indexLeft < count; ++indexLeft)
		{
			int indexRight = count - indexLeft - 1;

			boundsLeft.Expand(bboxData[begin + indexLeft].MinBounds);
			boundsLeft.Expand(bboxData[begin + indexLeft].MaxBounds);

			boundsRight.Expand(bboxData[begin + indexRight].MinBounds);
			boundsRight.Expand(bboxData[begin + indexRight].MaxBounds);

			float surfaceAreaLeft = calculateSurfaceArea(boundsLeft);
			float surfaceAreaRight = calculateSurfaceArea(boundsRight);

			bboxData[begin + indexLeft].SurfaceAreaLeft = surfaceAreaLeft;
			bboxData[begin + indexRight].SurfaceAreaRight = surfaceAreaRight;
		}

		float bestCost = FLT_MAX;
		for (int mid = begin + 1; mid <= end; ++mid)
		{
			float surfaceAreaLeft = bboxData[mid - 1].SurfaceAreaLeft;
			float surfaceAreaRight = bboxData[mid].SurfaceAreaRight;

			int countLeft = mid - begin;
			int countRight = end - mid;

			float costLeft = surfaceAreaLeft * (float)countLeft;
			float costRight = surfaceAreaRight * (float)countRight;

			float cost = costLeft + costRight;
			if (cost < bestCost)
			{
				bestSplit = mid;
				bestCost = cost;
			}
		}

		if (bestCost < splitCost)
		{
			split = bestSplit;
			splitCost = bestCost;
			axis = i;
		}
	}
}

BVHNode* createBVHNodeSHA(eastl::vector<AABBox>& bboxData, int begin, int end, float parentSplitCost)
{
	int count = end - begin + 1;

	vec3 minBounds;
	vec3 maxBounds;

	calculateBounds(bboxData, begin, end, minBounds, maxBounds);

	BVHNode* node = (BVHNode*)tf_placement_new<BVHNode>(tf_calloc(1, sizeof(BVHNode)));

	node->BoundingBox.Expand(minBounds);
	node->BoundingBox.Expand(maxBounds);

	if (count == 1)
	{
		//this is a leaf node
		node->Left = NULL;
		node->Right = NULL;

		node->BoundingBox.InstanceID = bboxData[begin].InstanceID;

		node->BoundingBox.Vertex0 = bboxData[begin].Vertex0;
		node->BoundingBox.Vertex1 = bboxData[begin].Vertex1;
		node->BoundingBox.Vertex2 = bboxData[begin].Vertex2;
	}
	else
	{
		int   split;
		int   axis;
		float splitCost;

		//find the best axis to sort along and where the split should be according to SAH
		findBestSplit(bboxData, begin, end, split, axis, splitCost);

		//sort along that axis
		sortAlongAxis(bboxData, begin, end, axis);

		//create the two branches
		node->Left = createBVHNodeSHA(bboxData, begin, split - 1, splitCost);
		node->Right = createBVHNodeSHA(bboxData, split, end, splitCost);

		//Access the child with the largest probability of collision first.
		float surfaceAreaLeft = calculateSurfaceArea(node->Left->BoundingBox);
		float surfaceAreaRight = calculateSurfaceArea(node->Right->BoundingBox);

		if (surfaceAreaRight > surfaceAreaLeft)
		{
			BVHNode* temp = node->Right;
			node->Right = node->Left;
			node->Left = temp;
		}

		node->BoundingBox.Vertex0 = vec3(0.0f);
		node->BoundingBox.Vertex1 = vec3(0.0f);
		node->BoundingBox.Vertex2 = vec3(0.0f);

		//this is an intermediate Node
		node->BoundingBox.InstanceID = -1;
	}

	return node;
}

void writeBVHTree(BVHNode* root, BVHNode* node, uint8_t* bboxData, int& dataOffset, int& index)
{
	if (node)
	{
		if (node->BoundingBox.InstanceID < 0.0f)
		{
			int          tempdataOffset = 0;
			BVHNodeBBox* bbox = NULL;

			//do not write the root node, for secondary rays the origin will always be in the scene bounding box
			if (node != root)
			{
				//this is an intermediate node, write bounding box
				bbox = (BVHNodeBBox*)(bboxData + dataOffset);

				bbox->MinBounds = float4(v3ToF3(node->BoundingBox.MinBounds), 0.0f);
				bbox->MaxBounds = float4(v3ToF3(node->BoundingBox.MaxBounds), 0.0f);

				dataOffset += sizeof(BVHNodeBBox);
				index++;

				tempdataOffset = dataOffset;
			}

			writeBVHTree(root, node->Left, bboxData, dataOffset, index);
			writeBVHTree(root, node->Right, bboxData, dataOffset, index);

			if (node != root)
			{
				//when on the left branch, how many float4 elements we need to skip to reach the right branch?
				bbox->MinBounds.w = -(float)(dataOffset - tempdataOffset) / sizeof(float4); //-V522
			}
		}
		else
		{
			//leaf node, write triangle vertices
			BVHLeafBBox* bbox = (BVHLeafBBox*)(bboxData + dataOffset);

			bbox->Vertex0 = float4(v3ToF3(node->BoundingBox.Vertex0), 0.0f);
			bbox->Vertex1MinusVertex0 = float4(v3ToF3(node->BoundingBox.Vertex1 - node->BoundingBox.Vertex0), 0.0f);
			bbox->Vertex2MinusVertex0 = float4(v3ToF3(node->BoundingBox.Vertex2 - node->BoundingBox.Vertex0), 0.0f);

			//when on the left branch, how many float4 elements we need to skip to reach the right branch?
			bbox->Vertex0.w = sizeof(BVHLeafBBox) / sizeof(float4);

			dataOffset += sizeof(BVHLeafBBox);
			index++;
		}
	}
}

void deleteBVHTree(BVHNode* node)
{
	if (node)
	{
		if (node->Left)
			deleteBVHTree(node->Left);
		if (node->Right)
			deleteBVHTree(node->Right);
		node->~BVHNode();
		tf_free(node);
	}
}

class HybridRaytracing : public IApp
{
public:
	HybridRaytracing()
	{
#ifndef METAL
		//set window size
		mSettings.mWidth = 1920;
		mSettings.mHeight = 1080;
#endif

#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
	}

	bool Init()
	{
		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");

		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		initRenderer(GetName(), &settings, &pRenderer);

		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pGraphicsQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPools[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pCmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pCmds[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		// Load fonts
		FontDesc font = {};
		font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
		fntDefineFonts(&font, 1, &gFontID);

		FontSystemDesc fontRenderDesc = {};
		fontRenderDesc.pRenderer = pRenderer;
		if (!initFontSystem(&fontRenderDesc))
			return false; // report?

		// Initialize Forge User Interface Rendering
		UserInterfaceDesc uiRenderDesc = {};
		uiRenderDesc.pRenderer = pRenderer;
		initUserInterface(&uiRenderDesc);

		// Initialize micro profiler and its UI.
		ProfilerDesc profiler = {};
		profiler.pRenderer = pRenderer;
		profiler.mWidthUI = mSettings.mWidth;
		profiler.mHeightUI = mSettings.mHeight;
		initProfiler(&profiler);

		gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

		//Create sampler state objects
		{ //point sampling with clamping
			{ SamplerDesc samplerDesc = { FILTER_NEAREST, FILTER_NEAREST, MIPMAP_MODE_NEAREST, ADDRESS_MODE_CLAMP_TO_EDGE,
										ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE };
			addSampler(pRenderer, &samplerDesc, &pSamplerPointClamp);
			}

			//linear sampling with wrapping
			{
				SamplerDesc samplerDesc = { FILTER_LINEAR,       FILTER_LINEAR,       MIPMAP_MODE_LINEAR,
											ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
				addSampler(pRenderer, &samplerDesc, &pSamplerLinearWrap);
			}
		}

		//Create Constant buffers
		{
			//Gprepass per-pass constant buffer
			{
				BufferLoadDesc ubDesc = {};
				ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
				ubDesc.mDesc.mSize = sizeof(GPrepassUniformBuffer);
				ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
				ubDesc.pData = NULL;
				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					ubDesc.ppBuffer = &pGPrepassUniformBuffer[i];
					addResource(&ubDesc, NULL);
				}
			}

			//Shadow pass per-pass constant buffer
			{
				BufferLoadDesc ubDesc = {};
				ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
				ubDesc.mDesc.mSize = sizeof(ShadowpassUniformBuffer);
				ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
				ubDesc.pData = NULL;
				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					ubDesc.ppBuffer = &pShadowpassUniformBuffer[i];
					addResource(&ubDesc, NULL);
				}
			}

			//Lighting pass per-pass constant buffer
			{
				BufferLoadDesc ubDesc = {};
				ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
				ubDesc.mDesc.mSize = sizeof(LightpassUniformBuffer);
				ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
				ubDesc.pData = NULL;
				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					ubDesc.ppBuffer = &pLightpassUniformBuffer[i];
					addResource(&ubDesc, NULL);
				}
			}
		}

		UIComponentDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);;
		uiCreateComponent(GetName(), &guiDesc, &pGuiWindow);

		SliderFloatWidget lightRotXSlider;
		lightRotXSlider.pData = &gLightRotationX;
		lightRotXSlider.mMin = (float)-M_PI;
		lightRotXSlider.mMax = (float)M_PI;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Light Rotation X", &lightRotXSlider, WIDGET_TYPE_SLIDER_FLOAT));

		SliderFloatWidget lightRotZSlider;
		lightRotZSlider.pData = &gLightRotationZ;
		lightRotZSlider.mMin = (float)-M_PI;
		lightRotZSlider.mMax = (float)M_PI;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Light Rotation Z", &lightRotZSlider, WIDGET_TYPE_SLIDER_FLOAT));

		SyncToken token = {};
		if (!LoadSponza(&token))
			return false;

		waitForToken(&token);
		
		CreateBVHBuffers();

		CameraMotionParameters cmp{ 200.0f, 250.0f, 300.0f };
		vec3                   camPos{ 100.0f, 25.0f, 0.0f };
		vec3                   lookAt{ 0 };

		pCameraController = initFpsCameraController(camPos, lookAt);

		pCameraController->setMotionParameters(cmp);

		InputSystemDesc inputDesc = {};
		inputDesc.pRenderer = pRenderer;
		inputDesc.pWindow = pWindow;
		if (!initInputSystem(&inputDesc))
			return false;

		// App Actions
		InputActionDesc actionDesc = {DefaultInputActions::DUMP_PROFILE_DATA, [](InputActionContext* ctx) {  dumpProfileData(((Renderer*)ctx->pUserData)->pName); return true; }, pRenderer};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TOGGLE_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; }};
		addInputAction(&actionDesc);
		InputActionCallback onUIInput = [](InputActionContext* ctx)
		{
			if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
			{
				uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
			}
			return true;
		};

		typedef bool(*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (*(ctx->pCaptured))
			{
				float2 delta = uiIsFocused() ? float2(0.f, 0.f) : ctx->mFloat2;
				index ? pCameraController->onRotate(delta) : pCameraController->onMove(delta);
			}
			return true;
		};
		actionDesc = {DefaultInputActions::CAPTURE_INPUT, [](InputActionContext* ctx) {setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);	return true; }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::ROTATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TRANSLATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx) { if (!uiWantTextInput()) pCameraController->resetView(); return true; }};
		addInputAction(&actionDesc);
		GlobalInputActionDesc globalInputActionDesc = {GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this};
		setGlobalInputAction(&globalInputActionDesc);

		gFrameIndex = 0; 

		return true;
	}

	void Exit()
	{
		exitInputSystem();
		exitCameraController(pCameraController);

		exitProfiler();

		exitUserInterface();

		exitFontSystem(); 

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeSampler(pRenderer, pSamplerLinearWrap);
		removeSampler(pRenderer, pSamplerPointClamp);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pGPrepassUniformBuffer[i]);
			removeResource(pShadowpassUniformBuffer[i]);
			removeResource(pLightpassUniformBuffer[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}

		gSponzaTextureIndexforMaterial.set_capacity(0);

		//Delete Sponza resources
		removeResource(SponzaProp.Geom);
		removeResource(SponzaProp.pConstantBuffer);

		for (uint i = 0; i < TOTAL_IMGS; ++i)
			removeResource(pMaterialTextures[i]);

		removeResource(BVHBoundingBoxesBuffer);

		exitResourceLoaderInterface(pRenderer);
		removeQueue(pRenderer, pGraphicsQueue);
		exitRenderer(pRenderer);
		pRenderer = NULL; 
	}

	void AssignSponzaTextures()
	{
		int AO = 5;
		int NoMetallic = 6;

		//00 : leaf
		gSponzaTextureIndexforMaterial.push_back(66);
		gSponzaTextureIndexforMaterial.push_back(67);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(68);
		gSponzaTextureIndexforMaterial.push_back(AO);

		//01 : vase_round
		gSponzaTextureIndexforMaterial.push_back(78);
		gSponzaTextureIndexforMaterial.push_back(79);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(80);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 02 : 16___Default (gi_flag)
		gSponzaTextureIndexforMaterial.push_back(8);
		gSponzaTextureIndexforMaterial.push_back(8);    // !!!!!!
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(8);    // !!!!!
		gSponzaTextureIndexforMaterial.push_back(AO);

		//03 : Material__57 (Plant)
		gSponzaTextureIndexforMaterial.push_back(75);
		gSponzaTextureIndexforMaterial.push_back(76);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(77);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 04 : Material__298
		gSponzaTextureIndexforMaterial.push_back(9);
		gSponzaTextureIndexforMaterial.push_back(10);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(11);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 05 : bricks
		gSponzaTextureIndexforMaterial.push_back(22);
		gSponzaTextureIndexforMaterial.push_back(23);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(24);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 06 :  arch
		gSponzaTextureIndexforMaterial.push_back(19);
		gSponzaTextureIndexforMaterial.push_back(20);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(21);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 07 : ceiling
		gSponzaTextureIndexforMaterial.push_back(25);
		gSponzaTextureIndexforMaterial.push_back(26);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(27);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 08 : column_a
		gSponzaTextureIndexforMaterial.push_back(28);
		gSponzaTextureIndexforMaterial.push_back(29);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(30);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 09 : Floor
		gSponzaTextureIndexforMaterial.push_back(60);
		gSponzaTextureIndexforMaterial.push_back(61);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(62);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 10 : column_c
		gSponzaTextureIndexforMaterial.push_back(34);
		gSponzaTextureIndexforMaterial.push_back(35);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(36);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 11 : details
		gSponzaTextureIndexforMaterial.push_back(45);
		gSponzaTextureIndexforMaterial.push_back(47);
		gSponzaTextureIndexforMaterial.push_back(46);
		gSponzaTextureIndexforMaterial.push_back(48);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 12 : column_b
		gSponzaTextureIndexforMaterial.push_back(31);
		gSponzaTextureIndexforMaterial.push_back(32);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(33);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 13 : flagpole
		gSponzaTextureIndexforMaterial.push_back(57);
		gSponzaTextureIndexforMaterial.push_back(58);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(59);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 14 : fabric_e (green)
		gSponzaTextureIndexforMaterial.push_back(51);
		gSponzaTextureIndexforMaterial.push_back(52);
		gSponzaTextureIndexforMaterial.push_back(53);
		gSponzaTextureIndexforMaterial.push_back(54);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 15 : fabric_d (blue)
		gSponzaTextureIndexforMaterial.push_back(49);
		gSponzaTextureIndexforMaterial.push_back(50);
		gSponzaTextureIndexforMaterial.push_back(53);
		gSponzaTextureIndexforMaterial.push_back(54);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 16 : fabric_a (red)
		gSponzaTextureIndexforMaterial.push_back(55);
		gSponzaTextureIndexforMaterial.push_back(56);
		gSponzaTextureIndexforMaterial.push_back(53);
		gSponzaTextureIndexforMaterial.push_back(54);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 17 : fabric_g (curtain_blue)
		gSponzaTextureIndexforMaterial.push_back(37);
		gSponzaTextureIndexforMaterial.push_back(38);
		gSponzaTextureIndexforMaterial.push_back(43);
		gSponzaTextureIndexforMaterial.push_back(44);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 18 : fabric_c (curtain_red)
		gSponzaTextureIndexforMaterial.push_back(41);
		gSponzaTextureIndexforMaterial.push_back(42);
		gSponzaTextureIndexforMaterial.push_back(43);
		gSponzaTextureIndexforMaterial.push_back(44);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 19 : fabric_f (curtain_green)
		gSponzaTextureIndexforMaterial.push_back(39);
		gSponzaTextureIndexforMaterial.push_back(40);
		gSponzaTextureIndexforMaterial.push_back(43);
		gSponzaTextureIndexforMaterial.push_back(44);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 20 : chain
		gSponzaTextureIndexforMaterial.push_back(12);
		gSponzaTextureIndexforMaterial.push_back(14);
		gSponzaTextureIndexforMaterial.push_back(13);
		gSponzaTextureIndexforMaterial.push_back(15);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 21 : vase_hanging
		gSponzaTextureIndexforMaterial.push_back(72);
		gSponzaTextureIndexforMaterial.push_back(73);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(74);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 22 : vase
		gSponzaTextureIndexforMaterial.push_back(69);
		gSponzaTextureIndexforMaterial.push_back(70);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(71);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 23 : Material__25 (lion)
		gSponzaTextureIndexforMaterial.push_back(16);
		gSponzaTextureIndexforMaterial.push_back(17);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(18);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 24 : roof
		gSponzaTextureIndexforMaterial.push_back(63);
		gSponzaTextureIndexforMaterial.push_back(64);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(65);
		gSponzaTextureIndexforMaterial.push_back(AO);

		// 25 : Material__47 - it seems missing
		gSponzaTextureIndexforMaterial.push_back(19);
		gSponzaTextureIndexforMaterial.push_back(20);
		gSponzaTextureIndexforMaterial.push_back(NoMetallic);
		gSponzaTextureIndexforMaterial.push_back(21);
		gSponzaTextureIndexforMaterial.push_back(AO);
	}

	//Loads sponza textures and Sponza mesh
	bool LoadSponza(SyncToken* token)
	{
		//load Sponza
		//eastl::vector<Image> toLoad(TOTAL_IMGS);
		//adding material textures
		for (int i = 0; i < TOTAL_IMGS; ++i)
		{
			TextureLoadDesc textureDesc = {};
			textureDesc.pFileName = pMaterialImageFileNames[i];
			textureDesc.ppTexture = &pMaterialTextures[i];
			if (strstr(pMaterialImageFileNames[i], "diffuse") || strstr(pMaterialImageFileNames[i], "Albedo"))
			{
				// Textures representing color should be stored in SRGB or HDR format
				textureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
			}
			addResource(&textureDesc, NULL);
		}

		{
			gVertexLayoutGPrepass.mAttribCount = 3;

			gVertexLayoutGPrepass.mAttribs[0].mSemantic = SEMANTIC_POSITION;
			gVertexLayoutGPrepass.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
			gVertexLayoutGPrepass.mAttribs[0].mBinding = 0;
			gVertexLayoutGPrepass.mAttribs[0].mLocation = 0;
			gVertexLayoutGPrepass.mAttribs[0].mOffset = 0;

			//normals
			gVertexLayoutGPrepass.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
			gVertexLayoutGPrepass.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
			gVertexLayoutGPrepass.mAttribs[1].mLocation = 1;
			gVertexLayoutGPrepass.mAttribs[1].mBinding = 1;
			gVertexLayoutGPrepass.mAttribs[1].mOffset = 0;

			//texture
			gVertexLayoutGPrepass.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
			gVertexLayoutGPrepass.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
			gVertexLayoutGPrepass.mAttribs[2].mLocation = 2;
			gVertexLayoutGPrepass.mAttribs[2].mBinding = 2;
			gVertexLayoutGPrepass.mAttribs[2].mOffset = 0;
		}

		SponzaProp.WorldMatrix = mat4::identity();

		GeometryLoadDesc loadDesc = {};
		loadDesc.pFileName = gModel_Sponza_File;
		loadDesc.ppGeometry = &SponzaProp.Geom;
		loadDesc.pVertexLayout = &gVertexLayoutGPrepass;
		loadDesc.mFlags = GEOMETRY_LOAD_FLAG_SHADOWED;
		addResource(&loadDesc, token);

		//set constant buffer for sponza
		{
			UniformObjData data = {};
			data.mWorldMat = SponzaProp.WorldMatrix;

			BufferLoadDesc desc = {};
			desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			desc.mDesc.mSize = sizeof(UniformObjData);
			desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
			desc.pData = &data;
			desc.ppBuffer = &SponzaProp.pConstantBuffer;
			addResource(&desc, NULL);
		}

		AssignSponzaTextures();

		return true;
	}

	void CreateBVHBuffers()
	{
		eastl::vector<AABBox> triBBoxes;
		triBBoxes.reserve(1000000);

		//create buffers for BVH

		addPropToPrimitivesAABBList(triBBoxes, SponzaProp);

		removeGeometryShadowData(SponzaProp.Geom);

		int count = (int)triBBoxes.size();
		//BVHNode* bvhRoot = createBVHNode(triBBoxes, 0, count-1);
		BVHNode* bvhRoot = createBVHNodeSHA(triBBoxes, 0, count - 1, FLT_MAX);

		gWholeSceneBBox = bvhRoot->BoundingBox;

		const int maxNoofElements = 1000000;
		uint8_t* bvhTreeNodes = (uint8_t*)tf_malloc(maxNoofElements * sizeof(BVHLeafBBox));

		int dataOffset = 0;
		int index = 0;
		writeBVHTree(bvhRoot, bvhRoot, bvhTreeNodes, dataOffset, index);

		//terminate BVH tree
		BVHNodeBBox* bbox = (BVHNodeBBox*)(bvhTreeNodes + dataOffset);
		bbox->MinBounds.w = 0;
		dataOffset += sizeof(BVHNodeBBox);

		SyncToken token = {};
		BufferLoadDesc desc = {};
		desc.mDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		desc.mDesc.mSize = dataOffset;
		desc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		desc.mDesc.mElementCount = desc.mDesc.mSize / sizeof(float4);
		desc.mDesc.mStructStride = sizeof(float4);
		desc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		desc.pData = bvhTreeNodes;
		desc.ppBuffer = &BVHBoundingBoxesBuffer;
		addResource(&desc, &token);
		waitForToken(&token);
		tf_free(bvhTreeNodes);
		deleteBVHTree(bvhRoot);
	}

	void addRenderTargets()
	{
		{
			ClearValue colorClearBlack = { {0.0f, 0.0f, 0.0f, 0.0f} };

			// Add G-buffer render targets
			{
				RenderTargetDesc rtDesc = {};
				rtDesc.mArraySize = 1;
				rtDesc.mClearValue = colorClearBlack;
				rtDesc.mDepth = 1;
				rtDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;

				rtDesc.mWidth = mSettings.mWidth;
				rtDesc.mHeight = mSettings.mHeight;
				rtDesc.mSampleCount = SAMPLE_COUNT_1;
				rtDesc.mSampleQuality = 0;
				rtDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;

				rtDesc.pName = "G-Buffer RTs";
				
				//Add albedo RT
				rtDesc.mFormat = getRecommendedSwapchainFormat(true, true);
				addRenderTarget(pRenderer, &rtDesc, &pRenderTargets[Albedo]);

				//Add normals RT
				rtDesc.mFormat = TinyImageFormat_R8G8B8A8_SNORM;
				addRenderTarget(pRenderer, &rtDesc, &pRenderTargets[Normal]);
			}

			// Add depth buffer
			{
				RenderTargetDesc depthRT = {};
				depthRT.mArraySize = 1;
				depthRT.mClearValue = { {1.0f, 0} };
				depthRT.mDepth = 1;
				depthRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
				depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
				depthRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
				depthRT.mHeight = mSettings.mHeight;
				depthRT.mSampleCount = SAMPLE_COUNT_1;
				depthRT.mSampleQuality = 0;
				depthRT.mWidth = mSettings.mWidth;
				addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);
			}

			// Add Shadow Pass render target
			{
				TextureLoadDesc textureDesc = {};
				TextureDesc     desc = {};
				desc.mWidth = mSettings.mWidth;
				desc.mHeight = mSettings.mHeight;
				desc.mDepth = 1;
				desc.mArraySize = 1;
				desc.mMipLevels = 1;
				desc.mFormat = TinyImageFormat_R8_UNORM;
				desc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
				desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
				desc.mSampleCount = SAMPLE_COUNT_1;
				textureDesc.pDesc = &desc;				
				textureDesc.ppTexture = &pTextures[Texture_RaytracedShadows];
				addResource(&textureDesc, NULL);
			}

			// Add Lighting Pass render target
			{
				TextureLoadDesc textureDesc = {};
				TextureDesc     desc = {};
				desc.mWidth = mSettings.mWidth;
				desc.mHeight = mSettings.mHeight;
				desc.mDepth = 1;
				desc.mArraySize = 1;
				desc.mMipLevels = 1;
				desc.mFormat = TinyImageFormat_B10G11R11_UFLOAT;
				desc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
				desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
				desc.mSampleCount = SAMPLE_COUNT_1;
				textureDesc.pDesc = &desc;
				textureDesc.ppTexture = &pTextures[Texture_Lighting];
				addResource(&textureDesc, NULL);
			}

			// Add Composite Pass render target
			{
				TextureLoadDesc textureDesc = {};
				TextureDesc     desc = {};
				desc.mWidth = mSettings.mWidth;
				desc.mHeight = mSettings.mHeight;
				desc.mDepth = 1;
				desc.mArraySize = 1;
				desc.mMipLevels = 1;
				desc.mFormat = TinyImageFormat_B10G11R11_UFLOAT;
				desc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
				desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
				desc.mSampleCount = SAMPLE_COUNT_1;
				textureDesc.pDesc = &desc;
				textureDesc.ppTexture = &pTextures[Texture_Composite];
				addResource(&textureDesc, NULL);
			}
		}
	}

	void removeRenderTargets()
	{
		removeRenderTarget(pRenderer, pRenderTargets[Albedo]);
		removeRenderTarget(pRenderer, pRenderTargets[Normal]);

		removeResource(pTextures[Texture_Lighting]);
		removeResource(pTextures[Texture_Composite]);
		removeResource(pTextures[Texture_RaytracedShadows]);
	}

	bool Load(ReloadDesc* pReloadDesc)
	{
		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			addShaders();
			addRootSignatures();
			addDescriptorSets();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			if (!addSwapChain())
				return false;

			addRenderTargets();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			addPipelines();
		}

		if (pReloadDesc->mType == RELOAD_TYPE_ALL)
		{
			//make the camera point towards the centre of the scene;
			vec3 midpoint = 0.5f * (gWholeSceneBBox.MaxBounds + gWholeSceneBBox.MinBounds);
			pCameraController->moveTo(midpoint - vec3(1050, 350, 0));
			pCameraController->lookAt(midpoint - vec3(0, 450, 0));
		}

		waitForAllResourceLoads();

		prepareDescriptorSets();

		UserInterfaceLoadDesc uiLoad = {};
		uiLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
		uiLoad.mHeight = mSettings.mHeight;
		uiLoad.mWidth = mSettings.mWidth;
		uiLoad.mLoadType = pReloadDesc->mType;
		loadUserInterface(&uiLoad);

		FontSystemLoadDesc fontLoad = {};
		fontLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
		fontLoad.mHeight = mSettings.mHeight;
		fontLoad.mWidth = mSettings.mWidth;
		fontLoad.mLoadType = pReloadDesc->mType;
		loadFontSystem(&fontLoad);

		return true;
	}

	void Unload(ReloadDesc* pReloadDesc)
	{
		waitQueueIdle(pGraphicsQueue);

		unloadFontSystem(pReloadDesc->mType);
		unloadUserInterface(pReloadDesc->mType);

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			removePipelines();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			removeSwapChain(pRenderer, pSwapChain);
			removeRenderTarget(pRenderer, pDepthBuffer);
			removeRenderTargets();
		}

		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			removeDescriptorSets();
			removeRootSignatures();
			removeShaders();
		}
	}

	void Update(float deltaTime)
	{
		updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

		pCameraController->update(deltaTime);

		Vector4 lightDir = { 0, 1, 0, 0 };
		mat4    lightRotMat = mat4::rotationX(gLightRotationX) * mat4::rotationZ(gLightRotationZ);

		lightDir = lightRotMat * lightDir;

		/************************************************************************/
		// Compute matrices
		/************************************************************************/
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 6000.0f);
		mat4        projectView = projMat * viewMat;
		mat4        invProjectView = inverse(projectView);

		//update GPrepass constant buffer
		{
			gPrepasUniformData.mProjectView = projectView;
		}

		//update Shadow pass constant buffer
		{
			gShadowPassUniformData.mProjectView = projectView;
			gShadowPassUniformData.mInvProjectView = invProjectView;
			gShadowPassUniformData.mRTSize = vec4(
				(float)mSettings.mWidth,
				(float)mSettings.mHeight,
				1.0f / mSettings.mWidth,
				1.0f / mSettings.mHeight);

			gShadowPassUniformData.mLightDir = lightDir;

			const float horizontalFOV = PI / 2.0f;
			float       pixelSize = tanf(0.5f * horizontalFOV) / float(gShadowPassUniformData.mRTSize.getX());

			gShadowPassUniformData.mCameraPosition = vec4(pCameraController->getViewPosition(), pixelSize);
		}

		//update Lighting pass constant buffer
		{
			gLightPassUniformData.mProjectView = projectView;
			gLightPassUniformData.mInvProjectView = invProjectView;
			gLightPassUniformData.mRTSize = vec4(
				(float)mSettings.mWidth,
				(float)mSettings.mHeight,
				1.0f / mSettings.mWidth,
				1.0f / mSettings.mHeight);
			gLightPassUniformData.mLightDir = lightDir;
		}

		//update Composite pass constant buffer
		{
			gDefaultPassUniformData.mRTSize = vec4(
				(float)mSettings.mWidth,
				(float)mSettings.mHeight,
				1.0f / mSettings.mWidth,
				1.0f / mSettings.mHeight);
		}

		gFrameNumber++;
	}

	void Draw()
	{
		eastl::vector<Cmd*> allCmds;

		if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}

		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus   fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (FENCE_STATUS_INCOMPLETE == fenceStatus)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		/************************************************************************/
		// Update uniform buffers
		/************************************************************************/
		BufferUpdateDesc desc = { pGPrepassUniformBuffer[gFrameIndex] };
		beginUpdateResource(&desc);
		*(GPrepassUniformBuffer*)desc.pMappedData = gPrepasUniformData;
		endUpdateResource(&desc, NULL);

		desc = { pShadowpassUniformBuffer[gFrameIndex] };
		beginUpdateResource(&desc);
		*(ShadowpassUniformBuffer*)desc.pMappedData = gShadowPassUniformData;
		endUpdateResource(&desc, NULL);

		desc = { pLightpassUniformBuffer[gFrameIndex] };
		beginUpdateResource(&desc);
		*(LightpassUniformBuffer*)desc.pMappedData = gLightPassUniformData;
		endUpdateResource(&desc, NULL);

		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		// GPrepass *********************************************************************************
		{
			//Clear G-buffers and Depth buffer
			LoadActionsDesc loadActions = {};
			for (uint32_t i = 0; i < GbufferCount; ++i)
			{
				loadActions.mLoadActionsColor[i] = LOAD_ACTION_CLEAR;
				loadActions.mClearColorValues[i] = pRenderTargets[i]->mClearValue;
			}

			// Clear depth to the far plane and stencil to 0
			loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
			loadActions.mClearDepth = { {1.0f, 0.0f} };

			{
				RenderTargetBarrier barriers[] =
				{
					{ pDepthBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
					{ pRenderTargets[Albedo], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
					{ pRenderTargets[Normal], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET }
				};

				cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 3, barriers);
			}

			RenderTarget* GbufferRTs[] = { pRenderTargets[Albedo], pRenderTargets[Normal] };

			//Set rendertargets and viewports
			cmdBindRenderTargets( cmd, GbufferCount, GbufferRTs, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport( cmd, 0.0f, 0.0f, (float)pRenderTargets[Albedo]->mWidth, (float)pRenderTargets[Albedo]->mHeight, 0.0f, 1.0f);
			cmdSetScissor( cmd, 0, 0, pRenderTargets[Albedo]->mWidth, pRenderTargets[Albedo]->mHeight);

			// Draw props
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "GBuffer Pass");

			cmdBindPipeline(cmd, pPipeline[GBuffer]);

			//draw sponza
			{
				cmdBindVertexBuffer(cmd, 3, SponzaProp.Geom->pVertexBuffers, SponzaProp.Geom->mVertexStrides, NULL);
				cmdBindIndexBuffer(cmd, SponzaProp.Geom->pIndexBuffer, SponzaProp.Geom->mIndexType, 0);

				for (uint32_t i = 0; i < (uint32_t)SponzaProp.Geom->mDrawArgCount; ++i)
				{
					const IndirectDrawIndexArguments& draw = SponzaProp.Geom->pDrawArgs[i];

					//use bindless textures on all platforms except for iOS
#ifndef TARGET_IOS
					struct MaterialMaps
					{
						uint mapIDs[5];
					} data;

					int materialID = gMaterialIds[i];
					materialID *= 5;    //because it uses 5 basic textures for rendering BRDF

					for (int j = 0; j < 5; ++j)
					{
						data.mapIDs[j] = gSponzaTextureIndexforMaterial[materialID + j];
					}
					//TODO: If we use more than albedo on iOS we need to bind every texture manually and update
					//descriptor param count.
					//one descriptor param if using bindless textures
					cmdBindPushConstants(cmd, pRootSignature, gMapIDRootConstantIndex, &data);
#else
					cmdBindDescriptorSet(cmd, i, pDescriptorSetFreqPerDraw);
#endif
					cmdBindDescriptorSet(cmd, 0, pDescriptorSetNonFreq);
					cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetFreq);

					cmdDrawIndexed(cmd, draw.mIndexCount, draw.mStartIndex, draw.mVertexOffset);
				}
				
				cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
			}

			

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
			// Transfer DepthBuffer and normals to SRV State
			RenderTargetBarrier barriers[] =
			{
				{ pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
				{ pRenderTargets[Albedo], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
				{ pRenderTargets[Normal], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
			};
			TextureBarrier uav = { pTextures[Texture_RaytracedShadows], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
			cmdResourceBarrier(cmd, 0, NULL, 1, &uav, 3, barriers);
		}

		// Raytraced shadow pass ************************************************************************
		{
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Raytraced shadow Pass");

			cmdBindPipeline(cmd, pPipeline[RaytracedShadows]);
			cmdBindDescriptorSet(cmd, Texture_RaytracedShadows, pDescriptorSetCompNonFreq);
			cmdBindDescriptorSet(cmd, gImageCount * Texture_RaytracedShadows + gFrameIndex, pDescriptorSetCompFreq);

			const uint32_t threadGroupSizeX = mSettings.mWidth / 8 + 1;
			const uint32_t threadGroupSizeY = mSettings.mHeight / 8 + 1;

			cmdDispatch(cmd, threadGroupSizeX, threadGroupSizeY, 1);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}

		// Lighting pass *********************************************************************************
		{
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Lighting Pass");

			// Transfer shadowbuffer to SRV and lightbuffer to UAV states
			TextureBarrier barriers[] =
			{
				{ pTextures[Texture_RaytracedShadows], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE },
				{ pTextures[Texture_Lighting], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS }
			};

			cmdResourceBarrier(cmd, 0, NULL, 2, barriers, 0, NULL);

			cmdBindPipeline(cmd, pPipeline[Lighting]);
			cmdBindDescriptorSet(cmd, Texture_Lighting, pDescriptorSetCompNonFreq);
			cmdBindDescriptorSet(cmd, gImageCount * Texture_Lighting + gFrameIndex, pDescriptorSetCompFreq);

			const uint32_t threadGroupSizeX = mSettings.mWidth / 16 + 1;
			const uint32_t threadGroupSizeY = mSettings.mHeight / 16 + 1;

			cmdDispatch(cmd, threadGroupSizeX, threadGroupSizeY, 1);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}

		// Composite pass *********************************************************************************
		{
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Composite Pass");

			// Transfer albedo and lighting to SRV State
			TextureBarrier barriers[] =
			{
				{ pTextures[Texture_Lighting], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE },
				{ pTextures[Texture_Composite], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS }
			};
			cmdResourceBarrier(cmd, 0, NULL, 2, barriers, 0, NULL);

			cmdBindPipeline(cmd, pPipeline[Composite]);
			cmdBindDescriptorSet(cmd, Texture_Composite, pDescriptorSetCompNonFreq);

			const uint32_t threadGroupSizeX = mSettings.mWidth / 16 + 1;
			const uint32_t threadGroupSizeY = mSettings.mHeight / 16 + 1;

			cmdDispatch(cmd, threadGroupSizeX, threadGroupSizeY, 1);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

			RenderTargetBarrier rtBarrier = { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET };
			barriers[0] = { pTextures[Texture_Composite], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };

			cmdResourceBarrier(cmd, 0, NULL, 1, barriers, 1, &rtBarrier);
		}

		// Copy results to the backbuffer & draw text *****************************************************************
		{
			LoadActionsDesc loadActions = {};
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Copy to Backbuffer Pass");

			// Draw  results
			cmdBindPipeline(cmd, pPipeline[CopyToBackbuffer]);
			cmdBindDescriptorSet(cmd, 1, pDescriptorSetNonFreq);

			//draw fullscreen triangle
			cmdDraw(cmd, 3, 0);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

			cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");

			gFrameTimeDraw.mFontColor = 0xff00ffff;
			gFrameTimeDraw.mFontSize = 18.0f;
			gFrameTimeDraw.mFontID = gFontID;
			float2 txtSize = cmdDrawCpuProfile(cmd, float2(8, 15), &gFrameTimeDraw);
			cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y + 75.f), gGpuProfileToken, &gFrameTimeDraw);

			cmdDrawUserInterface(cmd);
			cmdEndDebugMarker(cmd);

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
			RenderTargetBarrier barriers[] = { { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT } };
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
		}

		cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
		endCmd(cmd);

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.mSignalSemaphoreCount = 1;
		submitDesc.mWaitSemaphoreCount = 1;
		submitDesc.ppCmds = &cmd;
		submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
		submitDesc.ppWaitSemaphores = &pImageAcquiredSemaphore;
		submitDesc.pSignalFence = pRenderCompleteFence;
		queueSubmit(pGraphicsQueue, &submitDesc);
		QueuePresentDesc presentDesc = {};
		presentDesc.mIndex = swapchainImageIndex;
		presentDesc.mWaitSemaphoreCount = 1;
		presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
		presentDesc.pSwapChain = pSwapChain;
		presentDesc.mSubmitDone = true;
		queuePresent(pGraphicsQueue, &presentDesc);
		flipProfiler();

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "09a_HybridRaytracing"; }

	void prepareDescriptorSets()
	{
		// GBuffer
		{
			DescriptorData params[7] = {};
			params[0].pName = "cbPerProp";
			params[0].ppBuffers = &SponzaProp.pConstantBuffer;
#ifndef TARGET_IOS
			params[1].pName = "textureMaps";
			params[1].ppTextures = pMaterialTextures;
			params[1].mCount = TOTAL_IMGS;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetNonFreq, 2, params);
#else
			updateDescriptorSet(pRenderer, 0, pDescriptorSetNonFreq, 1, params);


			for (uint32_t i = 0; i < (uint32_t)SponzaProp.Geom->mDrawArgCount; ++i)
			{
				int materialID = gMaterialIds[i];
				materialID *= 5;    //because it uses 5 basic textures for rendering BRDF

				//bind textures explicitely for iOS
				//we only use Albedo for the time being so just bind the albedo texture.
				//for (int j = 0; j < 5; ++j)
				{
					params[0].pName = pTextureName[0];    //Albedo texture name
					uint textureId = gSponzaTextureIndexforMaterial[materialID];
					params[0].ppTextures = &pMaterialTextures[textureId];
				}
				//TODO: If we use more than albedo on iOS we need to bind every texture manually and update
				//descriptor param count.
				//one descriptor param if using bindless textures
				updateDescriptorSet(pRenderer, i, pDescriptorSetFreqPerDraw, 1, params);
			}

#endif
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "cbPerPass";
				params[0].ppBuffers = &pGPrepassUniformBuffer[i];
				updateDescriptorSet(pRenderer, gImageCount*GBuffer + i, pDescriptorSetFreq, 1, params);
			}
		}

		// Lighting
		{
			DescriptorData params[4] = {};
			params[0].pName = "normalBuffer";
			params[0].ppTextures = &pRenderTargets[Normal]->pTexture;
			params[1].pName = "shadowbuffer";
			params[1].ppTextures = &pTextures[Texture_RaytracedShadows];
			params[2].pName = "outputRT";
			params[2].ppTextures = &pTextures[Texture_Lighting];
			updateDescriptorSet(pRenderer, Texture_Lighting, pDescriptorSetCompNonFreq, 3, params);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "cbPerPass";
				params[0].ppBuffers = &pLightpassUniformBuffer[i];
				updateDescriptorSet(pRenderer, Texture_Lighting*gImageCount + i, pDescriptorSetCompFreq, 1, params);
			}
		}

		// Raytraced Shadows
		{
			DescriptorData params[9] = {};
			params[0].pName = "depthBuffer";
			params[0].ppTextures = &pDepthBuffer->pTexture;
			params[1].pName = "normalBuffer";
			params[1].ppTextures = &pRenderTargets[Normal]->pTexture;
			params[2].pName = "BVHTree";
			params[2].ppBuffers = &BVHBoundingBoxesBuffer;
			params[3].pName = "outputShadowRT";
			params[3].ppTextures = &pTextures[Texture_RaytracedShadows];
			updateDescriptorSet(pRenderer, Texture_RaytracedShadows, pDescriptorSetCompNonFreq, 4, params);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "cbPerPass";
				params[0].ppBuffers = &pShadowpassUniformBuffer[i];
				updateDescriptorSet(pRenderer, Texture_RaytracedShadows*gImageCount + i, pDescriptorSetCompFreq, 1, params);
			}
		}

		// Composite
		{
			DescriptorData params[4] = {};
			params[0].pName = "albedobuffer";
			params[0].ppTextures = &pRenderTargets[Albedo]->pTexture;
			params[1].pName = "lightbuffer";
			params[1].ppTextures = &pTextures[Texture_Lighting];
			params[2].pName = "outputRT";
			params[2].ppTextures = &pTextures[Texture_Composite];
			updateDescriptorSet(pRenderer, Texture_Composite, pDescriptorSetCompNonFreq, 3, params);
		}

		// Display
		{
			DescriptorData params[4] = {};
			params[0].pName = "inputRT";
			params[0].ppTextures = &pTextures[Texture_Composite];
			updateDescriptorSet(pRenderer, 1, pDescriptorSetNonFreq, 1, params);
		}
	}

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.mWindowHandle = pWindow->handle;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true, true);
		swapChainDesc.mEnableVsync = false;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	void addDescriptorSets()
	{
		DescriptorSetDesc desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
		addDescriptorSet(pRenderer, &desc, &pDescriptorSetNonFreq);

		desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &desc, &pDescriptorSetFreq);

		DescriptorSetDesc compDesc = { pRootSignatureComp, DESCRIPTOR_UPDATE_FREQ_NONE, TextureCount };
		addDescriptorSet(pRenderer, &compDesc, &pDescriptorSetCompNonFreq);

		compDesc = { pRootSignatureComp, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * TextureCount };
		addDescriptorSet(pRenderer, &compDesc, &pDescriptorSetCompFreq);

#ifdef TARGET_IOS
		DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, (uint32_t)SponzaProp.Geom->mDrawArgCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetFreqPerDraw);
#endif
	}

	void removeDescriptorSets()
	{
		removeDescriptorSet(pRenderer, pDescriptorSetNonFreq);
		removeDescriptorSet(pRenderer, pDescriptorSetFreq);
#ifdef TARGET_IOS
		removeDescriptorSet(pRenderer, pDescriptorSetFreqPerDraw);
#endif
		removeDescriptorSet(pRenderer, pDescriptorSetCompNonFreq);
		removeDescriptorSet(pRenderer, pDescriptorSetCompFreq);
	}

	void addRootSignatures()
	{
		const char* pStaticSamplers[] = { "samplerLinear" };

		Shader* shaders[] = { pShader[GBuffer], pShader[CopyToBackbuffer] };

		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplerNames = pStaticSamplers;
		rootDesc.ppStaticSamplers = &pSamplerLinearWrap;
		rootDesc.mShaderCount = 2;
		rootDesc.ppShaders = shaders;
#ifndef TARGET_IOS
		rootDesc.mMaxBindlessTextures = TOTAL_IMGS;
#endif
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);
		gMapIDRootConstantIndex = getDescriptorIndexFromName(pRootSignature, "cbTextureRootConstants");

		Shader* compShaders[] = { pShader[Lighting], pShader[Composite], pShader[RaytracedShadows] };

		RootSignatureDesc rootCompDesc = {};
		rootCompDesc.mShaderCount = 3;
		rootCompDesc.ppShaders = compShaders;
#ifndef TARGET_IOS
		rootCompDesc.mMaxBindlessTextures = TOTAL_IMGS;
#endif
		addRootSignature(pRenderer, &rootCompDesc, &pRootSignatureComp);
	}

	void removeRootSignatures()
	{
		removeRootSignature(pRenderer, pRootSignature);
		removeRootSignature(pRenderer, pRootSignatureComp);
	}

	void addShaders()
	{
		//Load shaders for GPrepass
		ShaderLoadDesc shaderGPrepass = {};
		shaderGPrepass.mStages[0] = { "gbufferPass.vert", NULL, 0 };
		shaderGPrepass.mStages[1] = { "gbufferPass.frag", NULL, 0 };
		addShader(pRenderer, &shaderGPrepass, &pShader[GBuffer]);

		//shader for Shadow pass
		ShaderLoadDesc shadowsShader = {};
		shadowsShader.mStages[0] = { "raytracedShadowsPass.comp", NULL, 0 };
		addShader(pRenderer, &shadowsShader, &pShader[RaytracedShadows]);

		//shader for Lighting pass
		ShaderLoadDesc lightingShader = {};
		lightingShader.mStages[0] = { "lightingPass.comp", NULL, 0 };
		addShader(pRenderer, &lightingShader, &pShader[Lighting]);

		//shader for Composite pass
		ShaderLoadDesc compositeShader = {};
		compositeShader.mStages[0] = { "compositePass.comp", NULL, 0 };
		addShader(pRenderer, &compositeShader, &pShader[Composite]);

		//Load shaders for copy to backbufferpass
		ShaderLoadDesc copyShader = {};
		copyShader.mStages[0] = { "display.vert", NULL, 0 };
		copyShader.mStages[1] = { "display.frag", NULL, 0 };
		addShader(pRenderer, &copyShader, &pShader[CopyToBackbuffer]);
	}

	void removeShaders()
	{
		removeShader(pRenderer, pShader[GBuffer]);
		removeShader(pRenderer, pShader[Lighting]);
		removeShader(pRenderer, pShader[Composite]);
		removeShader(pRenderer, pShader[RaytracedShadows]);
		removeShader(pRenderer, pShader[CopyToBackbuffer]);
	}

	void addPipelines()
	{
		//Create rasteriser state objects
		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		//Create depth state objects
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;

		DepthStateDesc depthStateNoneDesc = {};
		depthStateNoneDesc.mDepthTest = false;
		depthStateNoneDesc.mDepthWrite = false;
		depthStateNoneDesc.mDepthFunc = CMP_ALWAYS;

		//add gbuffer pipeline
		{
			//set up g-prepass buffer formats
			TinyImageFormat deferredFormats[GbufferCount] = {};
			for (uint32_t i = 0; i < GbufferCount; ++i)
			{
				deferredFormats[i] = pRenderTargets[i]->mFormat;
			}

			PipelineDesc desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = GbufferCount;
			pipelineSettings.pDepthState = &depthStateDesc;

			pipelineSettings.pColorFormats = deferredFormats;

			pipelineSettings.mSampleCount = pRenderTargets[0]->mSampleCount;
			pipelineSettings.mSampleQuality = pRenderTargets[0]->mSampleQuality;

			pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
			pipelineSettings.pRootSignature = pRootSignature;
			pipelineSettings.pShaderProgram = pShader[GBuffer];
			pipelineSettings.pVertexLayout = &gVertexLayoutGPrepass;
			pipelineSettings.pRasterizerState = &rasterizerStateDesc;

			addPipeline(pRenderer, &desc, &pPipeline[GBuffer]);
		}

		//create shadows pipeline
		{
			PipelineDesc desc = {};
			desc.mType = PIPELINE_TYPE_COMPUTE;
			ComputePipelineDesc& pipelineDesc = desc.mComputeDesc;
			pipelineDesc.pRootSignature = pRootSignatureComp;
			pipelineDesc.pShaderProgram = pShader[RaytracedShadows];
			addPipeline(pRenderer, &desc, &pPipeline[RaytracedShadows]);
		}

		//create lighting pipeline
		{
			PipelineDesc desc = {};
			desc.mType = PIPELINE_TYPE_COMPUTE;
			ComputePipelineDesc& pipelineDesc = desc.mComputeDesc;
			pipelineDesc.pRootSignature = pRootSignatureComp;
			pipelineDesc.pShaderProgram = pShader[Lighting];
			addPipeline(pRenderer, &desc, &pPipeline[Lighting]);
		}

		//create composite pipeline
		{
			PipelineDesc desc = {};
			desc.mType = PIPELINE_TYPE_COMPUTE;
			ComputePipelineDesc& pipelineDesc = desc.mComputeDesc;
			pipelineDesc.pRootSignature = pRootSignatureComp;
			pipelineDesc.pShaderProgram = pShader[Composite];
			addPipeline(pRenderer, &desc, &pPipeline[Composite]);
		}

		//create copy to backbuffer pipeline
		{
			PipelineDesc desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pRasterizerState = &rasterizerStateDesc;
			pipelineSettings.pDepthState = &depthStateNoneDesc;

			pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
			pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
			pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;

			pipelineSettings.pRootSignature = pRootSignature;
			pipelineSettings.pShaderProgram = pShader[CopyToBackbuffer];

			addPipeline(pRenderer, &desc, &pPipeline[CopyToBackbuffer]);
		}
	}

	void removePipelines()
	{
		removePipeline(pRenderer, pPipeline[GBuffer]);
		removePipeline(pRenderer, pPipeline[RaytracedShadows]);
		removePipeline(pRenderer, pPipeline[Lighting]);
		removePipeline(pRenderer, pPipeline[Composite]);
		removePipeline(pRenderer, pPipeline[CopyToBackbuffer]);
	}
};

DEFINE_APPLICATION_MAIN(HybridRaytracing)

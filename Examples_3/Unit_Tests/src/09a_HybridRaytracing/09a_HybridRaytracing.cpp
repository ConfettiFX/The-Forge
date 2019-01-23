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

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/Renderer/GpuProfiler.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

//ui
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/DebugRenderer.h"

//input
#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"

//asimp importer
#include "../../../../Common_3/Tools/AssimpImporter/AssimpImporter.h"
#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

const float gTimeScale = 0.2f;

FileSystem gFileSystem;
LogManager gLogManager;
Timer      gAccumTimer;
HiresTimer gTimer;

const char* pszBases[FSR_Count] = {
	"../../../src/09a_HybridRaytracing/",    // FSR_BinShaders
	"../../../src/09a_HybridRaytracing/",    // FSR_SrcShaders
	"../../../../../Art/Sponza/",            // FSR_Textures
	"../../../../../Art/Sponza/",            // FSR_Meshes
	"../../../UnitTestResources/",           // FSR_Builtin_Fonts
	"../../../src/09a_HybridRaytracing/",    // FSR_GpuConfig
	"",                                      // FSR_Animation
	"",                                      // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",     // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",       // FSR_MIDDLEWARE_UI
};

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

class RenderPassData
{
	public:
	Shader*                        pShader;
	RootSignature*                 pRootSignature;
	Pipeline*                      pPipeline;
	CmdPool*                       pCmdPool;
	Cmd**                          ppCmds;
	Buffer*                        pPerPassCB[gImageCount];
	tinystl::vector<RenderTarget*> RenderTargets;
	tinystl::vector<Texture*>      Textures;

	RenderPassData(Renderer* pRenderer, Queue* bGraphicsQueue, int ImageCount)
	{
		addCmdPool(pRenderer, bGraphicsQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, ImageCount, &ppCmds);
	}
};

struct RenderPass
{
	enum Enum
	{
		GBuffer,
		Lighting,
		Composite,
		RaytracedShadows,
		RaytractedReflections,
		CopyToBackbuffer
	};
};

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
	float pad;
};

//Structure of Array for vertex/index data
struct MeshBatch
{
	tinystl::vector<float3> PositionsData;
	tinystl::vector<uint>   IndicesData;

	Buffer* pPositionStream;
	Buffer* pNormalStream;
	Buffer* pUVStream;
	Buffer* pIndicesStream;
	int     NoofVertices;
	int     NoofIndices;
	int     NoofInstances;
	int     MaterialID;
};

//Enum for easy access of GBuffer RTs
struct GBufferRT
{
	enum Enum
	{
		Albedo,
		Normals,
		Noof
	};
};

struct PropData
{
	mat4                        WorldMatrix;
	tinystl::vector<MeshBatch*> MeshBatches;
	Buffer*                     pConstantBuffer;
};

typedef tinystl::unordered_map<RenderPass::Enum, RenderPassData*> RenderPassMap;

Renderer* pRenderer = NULL;
Queue*    pGraphicsQueue = NULL;

Sampler* pSamplerPointClamp = NULL;
Sampler* pSamplerLinearWrap = NULL;

RasterizerState* pRasterDefault = NULL;
DepthState*      pDepthDefault = NULL;
DepthState*      pDepthNone = NULL;

Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

SwapChain* pSwapChain = NULL;

Buffer* BVHBoundingBoxesBuffer;

//The render passes used in the demo
RenderPassMap            RenderPasses;
GPrepassUniformBuffer    gPrepasUniformData;
ShadowpassUniformBuffer  gShadowPassUniformData;
LightpassUniformBuffer   gLightPassUniformData;
DefaultpassUniformBuffer gDefaultPassUniformData;

uint32_t gFrameIndex = 0;

UIApp              gAppUI;
GpuProfiler*       pGpuProfiler = NULL;
ICameraController* pCameraController = NULL;

//Sponza
const char*           gModel_Sponza_File = "sponza.obj";
AssimpImporter::Model gModel_Sponza;

const char* pMaterialImageFileNames[] = {
	"SponzaPBR_Textures/ao.png",
	"SponzaPBR_Textures/ao.png",
	"SponzaPBR_Textures/ao.png",
	"SponzaPBR_Textures/ao.png",
	"SponzaPBR_Textures/ao.png",

	//common
	"SponzaPBR_Textures/ao.png",
	"SponzaPBR_Textures/Dielectric_metallic.tga",
	"SponzaPBR_Textures/Metallic_metallic.tga",
	"SponzaPBR_Textures/gi_flag.png",

	//Background
	"SponzaPBR_Textures/Background/Background_Albedo.tga",
	"SponzaPBR_Textures/Background/Background_Normal.tga",
	"SponzaPBR_Textures/Background/Background_Roughness.tga",

	//ChainTexture
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Albedo.tga",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Metallic.tga",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Normal.tga",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Roughness.tga",

	//Lion
	"SponzaPBR_Textures/Lion/Lion_Albedo.tga",
	"SponzaPBR_Textures/Lion/Lion_Normal.tga",
	"SponzaPBR_Textures/Lion/Lion_Roughness.tga",

	//Sponza_Arch
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_normal.tga",
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_roughness.tga",

	//Sponza_Bricks
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Albedo.tga",
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Normal.tga",
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Roughness.tga",

	//Sponza_Ceiling
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_normal.tga",
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_roughness.tga",

	//Sponza_Column
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_normal.tga",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_roughness.tga",

	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_normal.tga",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_roughness.tga",

	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_normal.tga",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_roughness.tga",

	//Sponza_Curtain
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Blue_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Blue_normal.tga",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Green_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Green_normal.tga",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Red_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Red_normal.tga",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_metallic.tga",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_roughness.tga",

	//Sponza_Details
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_metallic.tga",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_normal.tga",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_roughness.tga",

	//Sponza_Fabric
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Blue_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Blue_normal.tga",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Green_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Green_normal.tga",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_metallic.tga",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_roughness.tga",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Red_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Red_normal.tga",

	//Sponza_FlagPole
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_diffuse.tga",
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_normal.tga",
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_roughness.tga",

	//Sponza_Floor
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_normal.tga",
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_roughness.tga",

	//Sponza_Roof
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_normal.tga",
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_roughness.tga",

	//Sponza_Thorn
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_diffuse.tga",
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_normal.tga",
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_roughness.tga",

	//Vase
	"SponzaPBR_Textures/Vase/Vase_diffuse.tga",
	"SponzaPBR_Textures/Vase/Vase_normal.tga",
	"SponzaPBR_Textures/Vase/Vase_roughness.tga",

	//VaseHanging
	"SponzaPBR_Textures/VaseHanging/VaseHanging_diffuse.tga",
	"SponzaPBR_Textures/VaseHanging/VaseHanging_normal.tga",
	"SponzaPBR_Textures/VaseHanging/VaseHanging_roughness.tga",

	//VasePlant
	"SponzaPBR_Textures/VasePlant/VasePlant_diffuse.tga",
	"SponzaPBR_Textures/VasePlant/VasePlant_normal.tga",
	"SponzaPBR_Textures/VasePlant/VasePlant_roughness.tga",

	//VaseRound
	"SponzaPBR_Textures/VaseRound/VaseRound_diffuse.tga",
	"SponzaPBR_Textures/VaseRound/VaseRound_normal.tga",
	"SponzaPBR_Textures/VaseRound/VaseRound_roughness.tga",

	"lion/lion_albedo.png",
	"lion/lion_specular.png",
	"lion/lion_normal.png",

};

//Workaround for too many bindless textures on iOS
//we bind textures individually for each draw call.
#ifdef TARGET_IOS
const char* pTextureName[] = { "albedoMap", "normalMap", "metallicMap", "roughnessMap", "aoMap" };

VirtualJoystickUI gVirtualJoystick;
#endif

PropData SponzaProp;

#define TOTAL_IMGS 84
Texture* pMaterialTextures[TOTAL_IMGS];

tinystl::vector<int> gSponzaTextureIndexforMaterial;

AABBox gWholeSceneBBox;

RenderTarget* pDepthBuffer = NULL;

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

uint gFrameNumber = 0;

void addPropToPrimitivesAABBList(tinystl::vector<AABBox>& bboxData, PropData& prop)
{
	mat4& world = prop.WorldMatrix;

	for (MeshBatch* mesh : prop.MeshBatches)
	{
		//number of indices
		uint noofIndices = mesh->NoofIndices;

		for (uint j = 0; j < noofIndices; j += 3)
		{
			AABBox bbox;

			int index0 = mesh->IndicesData[j + 0];
			int index1 = mesh->IndicesData[j + 1];
			int index2 = mesh->IndicesData[j + 2];

			bbox.Vertex0 = (world * vec4(f3Tov3(mesh->PositionsData[index0]), 1)).getXYZ();
			bbox.Vertex1 = (world * vec4(f3Tov3(mesh->PositionsData[index1]), 1)).getXYZ();
			bbox.Vertex2 = (world * vec4(f3Tov3(mesh->PositionsData[index2]), 1)).getXYZ();

			bbox.Expand(bbox.Vertex0);
			bbox.Expand(bbox.Vertex1);
			bbox.Expand(bbox.Vertex2);

			bbox.InstanceID = 0;

			bboxData.push_back(bbox);
		}
	}
}

void calculateBounds(tinystl::vector<AABBox>& bboxData, int begin, int end, vec3& minBounds, vec3& maxBounds)
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

void sortAlongAxis(tinystl::vector<AABBox>& bboxData, int begin, int end, int axis)
{
#if 0    // This path is using tinystl::sort which seems much slower than std::qsort.
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

//BVHNode* createBVHNodeMedianSplit(tinystl::vector<AABBox>& bboxData, int begin, int end)
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
//  BVHNode* node = (BVHNode*)conf_placement_new<BVHNode>(conf_calloc(1, sizeof(BVHNode)));
//
//  node->BoundingBox.Expand(minBounds);
//  node->BoundingBox.Expand(maxBounds);
//
//  int split = count / 2;
//
//  if (count == 1)
//  {
//	  //this is a leaf node
//	  node->Left = nullptr;
//	  node->Right = nullptr;
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
void findBestSplit(tinystl::vector<AABBox>& bboxData, int begin, int end, int& split, int& axis, float& splitCost)
{
	int count = end - begin + 1;
	int bestSplit = begin;
	int globalBestSplit = begin;
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

BVHNode* createBVHNodeSHA(tinystl::vector<AABBox>& bboxData, int begin, int end, float parentSplitCost)
{
	int count = end - begin + 1;

	vec3 minBounds;
	vec3 maxBounds;

	calculateBounds(bboxData, begin, end, minBounds, maxBounds);

	BVHNode* node = (BVHNode*)conf_placement_new<BVHNode>(conf_calloc(1, sizeof(BVHNode)));

	node->BoundingBox.Expand(minBounds);
	node->BoundingBox.Expand(maxBounds);

	if (count == 1)
	{
		//this is a leaf node
		node->Left = nullptr;
		node->Right = nullptr;

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

void writeBVHTree(BVHNode* root, BVHNode* node, uint8* bboxData, int& dataOffset, int& index)
{
	if (node)
	{
		if (node->BoundingBox.InstanceID < 0.0f)
		{
			int          tempIndex = 0;
			int          tempdataOffset = 0;
			BVHNodeBBox* bbox = nullptr;

			//do not write the root node, for secondary rays the origin will always be in the scene bounding box
			if (node != root)
			{
				//this is an intermediate node, write bounding box
				bbox = (BVHNodeBBox*)(bboxData + dataOffset);

				bbox->MinBounds = float4(v3ToF3(node->BoundingBox.MinBounds), 0.0f);
				bbox->MaxBounds = float4(v3ToF3(node->BoundingBox.MaxBounds), 0.0f);

				tempIndex = index;

				dataOffset += sizeof(BVHNodeBBox);
				index++;

				tempdataOffset = dataOffset;
			}

			writeBVHTree(root, node->Left, bboxData, dataOffset, index);
			writeBVHTree(root, node->Right, bboxData, dataOffset, index);

			if (node != root)
			{
				//when on the left branch, how many float4 elements we need to skip to reach the right branch?
				bbox->MinBounds.w = -(float)(dataOffset - tempdataOffset) / sizeof(float4);
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
	if (node && (node->Left != nullptr || node->Right != nullptr))
	{
		deleteBVHTree(node->Left);
		deleteBVHTree(node->Right);
		node->~BVHNode();
		conf_free(node);
	}
}

class HybridRaytracing: public IApp
{
	public:
	HybridRaytracing()
	{
#ifndef METAL
		//set window size
		mSettings.mWidth = 1920;
		mSettings.mHeight = 1080;
#endif
	}

	bool Init()
	{
		// window and renderer setup
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);

		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		//Add rendering passes

		//Gbuffer pass
		RenderPassData* pass =
			conf_placement_new<RenderPassData>(conf_calloc(1, sizeof(RenderPassData)), pRenderer, pGraphicsQueue, gImageCount);
		RenderPasses.insert(tinystl::pair<RenderPass::Enum, RenderPassData*>(RenderPass::GBuffer, pass));

		//Shadow pass
		pass = conf_placement_new<RenderPassData>(conf_calloc(1, sizeof(RenderPassData)), pRenderer, pGraphicsQueue, gImageCount);
		RenderPasses.insert(tinystl::pair<RenderPass::Enum, RenderPassData*>(RenderPass::RaytracedShadows, pass));

		//Lighting pass
		pass = conf_placement_new<RenderPassData>(conf_calloc(1, sizeof(RenderPassData)), pRenderer, pGraphicsQueue, gImageCount);
		RenderPasses.insert(tinystl::pair<RenderPass::Enum, RenderPassData*>(RenderPass::Lighting, pass));

		//Composite pass
		pass = conf_placement_new<RenderPassData>(conf_calloc(1, sizeof(RenderPassData)), pRenderer, pGraphicsQueue, gImageCount);
		RenderPasses.insert(tinystl::pair<RenderPass::Enum, RenderPassData*>(RenderPass::Composite, pass));

		//Copy to backbuffer
		pass = conf_placement_new<RenderPassData>(conf_calloc(1, sizeof(RenderPassData)), pRenderer, pGraphicsQueue, gImageCount);
		RenderPasses.insert(tinystl::pair<RenderPass::Enum, RenderPassData*>(RenderPass::CopyToBackbuffer, pass));

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

		//Load shaders
		{
			//Load shaders for GPrepass
			ShaderMacro    totalImagesShaderMacro = { "TOTAL_IMGS", tinystl::string::format("%i", TOTAL_IMGS) };
			ShaderLoadDesc shaderGPrepass = {};
			shaderGPrepass.mStages[0] = { "gbufferPass.vert", NULL, 0, FSR_SrcShaders };
#ifndef TARGET_IOS
			shaderGPrepass.mStages[1] = { "gbufferPass.frag", &totalImagesShaderMacro, 1, FSR_SrcShaders };
#else
			//separate fragment gbuffer pass for iOs that does not use bindless textures
			shaderGPrepass.mStages[1] = { "gbufferPass_iOS.frag", NULL, 0, FSR_SrcShaders };
#endif
			addShader(pRenderer, &shaderGPrepass, &RenderPasses[RenderPass::GBuffer]->pShader);

			//shader for Shadow pass
			ShaderLoadDesc shadowsShader = {};
			shadowsShader.mStages[0] = { "raytracedShadowsPass.comp", NULL, 0, FSR_SrcShaders };
			addShader(pRenderer, &shadowsShader, &RenderPasses[RenderPass::RaytracedShadows]->pShader);

			//shader for Lighting pass
			ShaderLoadDesc lightingShader = {};
			lightingShader.mStages[0] = { "lightingPass.comp", NULL, 0, FSR_SrcShaders };
			addShader(pRenderer, &lightingShader, &RenderPasses[RenderPass::Lighting]->pShader);

			//shader for Composite pass
			ShaderLoadDesc compositeShader = {};
			compositeShader.mStages[0] = { "compositePass.comp", NULL, 0, FSR_SrcShaders };
			addShader(pRenderer, &compositeShader, &RenderPasses[RenderPass::Composite]->pShader);

			//Load shaders for copy to backbufferpass
			ShaderLoadDesc copyShader = {};
			copyShader.mStages[0] = { "display.vert", NULL, 0, FSR_SrcShaders };
			copyShader.mStages[1] = { "display.frag", NULL, 0, FSR_SrcShaders };
			addShader(pRenderer, &copyShader, &RenderPasses[RenderPass::CopyToBackbuffer]->pShader);
		}

		//Create rasteriser state objects
		{
			RasterizerStateDesc rasterizerStateDesc = {};
			rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
			addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterDefault);
		}

		//Create depth state objects
		{
			DepthStateDesc depthStateDesc = {};
			depthStateDesc.mDepthTest = true;
			depthStateDesc.mDepthWrite = true;
			depthStateDesc.mDepthFunc = CMP_LEQUAL;
			addDepthState(pRenderer, &depthStateDesc, &pDepthDefault);

			depthStateDesc.mDepthTest = false;
			depthStateDesc.mDepthWrite = false;
			depthStateDesc.mDepthFunc = CMP_ALWAYS;
			addDepthState(pRenderer, &depthStateDesc, &pDepthNone);
		}

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

//Create root signatures
{ //Add root signature for GPrepass
  { const char * pStaticSamplers[] = { "samplerLinear" };
RootSignatureDesc rootDesc = {};
rootDesc.mStaticSamplerCount = 1;
rootDesc.ppStaticSamplerNames = pStaticSamplers;
rootDesc.ppStaticSamplers = &pSamplerLinearWrap;
rootDesc.mShaderCount = 1;
rootDesc.ppShaders = &RenderPasses[RenderPass::GBuffer]->pShader;
#ifndef TARGET_IOS
rootDesc.mMaxBindlessTextures = TOTAL_IMGS;
#endif

addRootSignature(pRenderer, &rootDesc, &RenderPasses[RenderPass::GBuffer]->pRootSignature);
}

//Add root signature for Shadow pass
{
	RootSignatureDesc rootDesc = {};
	rootDesc.mShaderCount = 1;
	rootDesc.ppShaders = &RenderPasses[RenderPass::RaytracedShadows]->pShader;
	addRootSignature(pRenderer, &rootDesc, &RenderPasses[RenderPass::RaytracedShadows]->pRootSignature);
}

//Add root signature for Lighting pass
{
	RootSignatureDesc rootDesc = {};
	rootDesc.mShaderCount = 1;
	rootDesc.ppShaders = &RenderPasses[RenderPass::Lighting]->pShader;
	addRootSignature(pRenderer, &rootDesc, &RenderPasses[RenderPass::Lighting]->pRootSignature);
}

//Add root signature for composite pass
{
	RootSignatureDesc rootDesc = {};
	rootDesc.mShaderCount = 1;
	rootDesc.ppShaders = &RenderPasses[RenderPass::Composite]->pShader;
	addRootSignature(pRenderer, &rootDesc, &RenderPasses[RenderPass::Composite]->pRootSignature);
}

//Add root signature for Copy to Backbuffer Pass
{
	RootSignatureDesc rootDesc = {};
	rootDesc.mShaderCount = 1;
	rootDesc.ppShaders = &RenderPasses[RenderPass::CopyToBackbuffer]->pShader;
	addRootSignature(pRenderer, &rootDesc, &RenderPasses[RenderPass::CopyToBackbuffer]->pRootSignature);
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
			ubDesc.ppBuffer = &RenderPasses[RenderPass::GBuffer]->pPerPassCB[i];
			addResource(&ubDesc);
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
			ubDesc.ppBuffer = &RenderPasses[RenderPass::RaytracedShadows]->pPerPassCB[i];
			addResource(&ubDesc);
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
			ubDesc.ppBuffer = &RenderPasses[RenderPass::Lighting]->pPerPassCB[i];
			addResource(&ubDesc);
		}
	}

	//Composite pass per-pass constant buffer
	{
		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(DefaultpassUniformBuffer);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &RenderPasses[RenderPass::Composite]->pPerPassCB[i];
			addResource(&ubDesc);
		}
	}
}

if (!gAppUI.Init(pRenderer))
	return false;

#ifdef TARGET_IOS
if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Textures))
	return false;
#endif

gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

CameraMotionParameters cmp{ 200.0f, 250.0f, 300.0f };
vec3                   camPos{ 100.0f, 25.0f, 0.0f };
vec3                   lookAt{ 0 };

pCameraController = createFpsCameraController(camPos, lookAt);

#if defined(TARGET_IOS) || defined(__ANDROID__)
gVirtualJoystick.InitLRSticks();
pCameraController->setVirtualJoystick(&gVirtualJoystick);
#endif

requestMouseCapture(true);

pCameraController->setMotionParameters(cmp);
InputSystem::RegisterInputEvent(cameraInputEvent);

if (!LoadSponza())
	return false;

CreateBVHBuffers();

return true;
}

void Exit()
{
	waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

	destroyCameraController(pCameraController);

	removeDebugRendererInterface();

	gAppUI.Exit();

#ifdef TARGET_IOS
	gVirtualJoystick.Exit();
#endif

	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		removeFence(pRenderer, pRenderCompleteFences[i]);
		removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
	}
	removeSemaphore(pRenderer, pImageAcquiredSemaphore);

	removeSampler(pRenderer, pSamplerLinearWrap);
	removeSampler(pRenderer, pSamplerPointClamp);

	removeRasterizerState(pRasterDefault);
	removeDepthState(pDepthDefault);
	removeDepthState(pDepthNone);

	//Delete rendering passes
	for (RenderPassMap::iterator iter = RenderPasses.begin(); iter != RenderPasses.end(); ++iter)
	{
		RenderPassData* pass = iter->second;

		if (!pass)
			continue;

		for (RenderTarget* rt : pass->RenderTargets)
		{
			removeRenderTarget(pRenderer, rt);
		}

		for (Texture* texture : pass->Textures)
		{
			removeResource(texture);
		}

		for (uint32_t j = 0; j < gImageCount; ++j)
		{
			if (pass->pPerPassCB[j])
				removeResource(pass->pPerPassCB[j]);
		}

		removeCmd_n(pass->pCmdPool, gImageCount, pass->ppCmds);
		removeCmdPool(pRenderer, pass->pCmdPool);

		removeShader(pRenderer, pass->pShader);

		removeRootSignature(pRenderer, pass->pRootSignature);
		pass->~RenderPassData();
		conf_free(pass);
	}

	RenderPasses.empty();

	//Delete Sponza resources
	for (MeshBatch* meshBatch : SponzaProp.MeshBatches)
	{
		removeResource(meshBatch->pIndicesStream);
		removeResource(meshBatch->pNormalStream);
		removeResource(meshBatch->pPositionStream);
		removeResource(meshBatch->pUVStream);
		meshBatch->~MeshBatch();
		conf_free(meshBatch);
	}

	removeResource(SponzaProp.pConstantBuffer);
	SponzaProp.MeshBatches.empty();

	for (uint i = 0; i < TOTAL_IMGS; ++i)
		removeResource(pMaterialTextures[i]);

	removeResource(BVHBoundingBoxesBuffer);

	removeGpuProfiler(pRenderer, pGpuProfiler);
	removeResourceLoaderInterface(pRenderer);
	removeQueue(pGraphicsQueue);
	removeRenderer(pRenderer);
}

void AssignSponzaTextures()
{
	int AO = 5;
	int NoMetallic = 6;
	int Metallic = 7;

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

	//02 : Material__57 (Plant)
	gSponzaTextureIndexforMaterial.push_back(75);
	gSponzaTextureIndexforMaterial.push_back(76);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(77);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 03 : Material__298
	gSponzaTextureIndexforMaterial.push_back(9);
	gSponzaTextureIndexforMaterial.push_back(10);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(11);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 04 : 16___Default (gi_flag)
	gSponzaTextureIndexforMaterial.push_back(8);
	gSponzaTextureIndexforMaterial.push_back(8);    // !!!!!!
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(8);    // !!!!!
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

	// 13 : Material__47 - it seems missing
	gSponzaTextureIndexforMaterial.push_back(19);
	gSponzaTextureIndexforMaterial.push_back(20);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(21);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 14 : flagpole
	gSponzaTextureIndexforMaterial.push_back(57);
	gSponzaTextureIndexforMaterial.push_back(58);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(59);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 15 : fabric_e (green)
	gSponzaTextureIndexforMaterial.push_back(51);
	gSponzaTextureIndexforMaterial.push_back(52);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 16 : fabric_d (blue)
	gSponzaTextureIndexforMaterial.push_back(49);
	gSponzaTextureIndexforMaterial.push_back(50);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 17 : fabric_a (red)
	gSponzaTextureIndexforMaterial.push_back(55);
	gSponzaTextureIndexforMaterial.push_back(56);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 18 : fabric_g (curtain_blue)
	gSponzaTextureIndexforMaterial.push_back(37);
	gSponzaTextureIndexforMaterial.push_back(38);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 19 : fabric_c (curtain_red)
	gSponzaTextureIndexforMaterial.push_back(41);
	gSponzaTextureIndexforMaterial.push_back(42);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 20 : fabric_f (curtain_green)
	gSponzaTextureIndexforMaterial.push_back(39);
	gSponzaTextureIndexforMaterial.push_back(40);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 21 : chain
	gSponzaTextureIndexforMaterial.push_back(12);
	gSponzaTextureIndexforMaterial.push_back(14);
	gSponzaTextureIndexforMaterial.push_back(13);
	gSponzaTextureIndexforMaterial.push_back(15);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 22 : vase_hanging
	gSponzaTextureIndexforMaterial.push_back(72);
	gSponzaTextureIndexforMaterial.push_back(73);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(74);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 23 : vase
	gSponzaTextureIndexforMaterial.push_back(69);
	gSponzaTextureIndexforMaterial.push_back(70);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(71);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 24 : Material__25 (lion)
	gSponzaTextureIndexforMaterial.push_back(16);
	gSponzaTextureIndexforMaterial.push_back(17);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(18);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 25 : roof
	gSponzaTextureIndexforMaterial.push_back(63);
	gSponzaTextureIndexforMaterial.push_back(64);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(65);
	gSponzaTextureIndexforMaterial.push_back(AO);
}

//Loads sponza textures and Sponza mesh
bool LoadSponza()
{
	//load Sponza
	//tinystl::vector<Image> toLoad(TOTAL_IMGS);
	//adding material textures
	for (int i = 0; i < TOTAL_IMGS; ++i)
	{
		TextureLoadDesc textureDesc = {};
		textureDesc.mRoot = FSR_Textures;
		textureDesc.mUseMipmaps = true;
		textureDesc.pFilename = pMaterialImageFileNames[i];
		textureDesc.ppTexture = &pMaterialTextures[i];
		addResource(&textureDesc, true);
	}

	AssimpImporter importer;

	tinystl::string sceneFullPath = FileSystem::FixPath(gModel_Sponza_File, FSRoot::FSR_Meshes);
	if (!importer.ImportModel(sceneFullPath.c_str(), &gModel_Sponza))
	{
		ErrorMsg("Failed to load %s", FileSystem::GetFileNameAndExtension(sceneFullPath).c_str());
		finishResourceLoading();
		return false;
	}

	size_t meshCount = gModel_Sponza.mMeshArray.size();
	size_t sponza_matCount = gModel_Sponza.mMaterialList.size();

	SponzaProp.WorldMatrix = mat4::identity();

	for (int i = 0; i < meshCount; i++)
	{
		//skip the large cloth mid-scene
		if (i == 4)
			continue;

		AssimpImporter::Mesh subMesh = gModel_Sponza.mMeshArray[i];

		MeshBatch* pMeshBatch = (MeshBatch*)conf_placement_new<MeshBatch>(conf_calloc(1, sizeof(MeshBatch)));

		SponzaProp.MeshBatches.push_back(pMeshBatch);

		pMeshBatch->MaterialID = subMesh.mMaterialId;
		pMeshBatch->NoofIndices = (int)subMesh.mIndices.size();
		pMeshBatch->NoofVertices = (int)subMesh.mPositions.size();

		for (int j = 0; j < pMeshBatch->NoofIndices; j++)
		{
			pMeshBatch->IndicesData.push_back(subMesh.mIndices[j]);
		}

		for (int j = 0; j < pMeshBatch->NoofVertices; j++)
		{
			pMeshBatch->PositionsData.push_back(subMesh.mPositions[j]);
		}

		// Vertex buffers for sponza
		{
			BufferLoadDesc desc = {};
			desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
			desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			desc.mDesc.mVertexStride = sizeof(float3);
			desc.mDesc.mSize = subMesh.mPositions.size() * desc.mDesc.mVertexStride;
			desc.pData = subMesh.mPositions.data();
			desc.ppBuffer = &pMeshBatch->pPositionStream;
			addResource(&desc);

			desc.mDesc.mVertexStride = sizeof(float3);
			desc.mDesc.mSize = subMesh.mNormals.size() * desc.mDesc.mVertexStride;
			desc.pData = subMesh.mNormals.data();
			desc.ppBuffer = &pMeshBatch->pNormalStream;
			addResource(&desc);

			desc.mDesc.mVertexStride = sizeof(float2);
			desc.mDesc.mSize = subMesh.mUvs.size() * desc.mDesc.mVertexStride;
			desc.pData = subMesh.mUvs.data();
			desc.ppBuffer = &pMeshBatch->pUVStream;
			addResource(&desc);
		}

		// Index buffer for sponza
		{
			// Index buffer for the scene
			BufferLoadDesc desc = {};
			desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
			desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			desc.mDesc.mIndexType = INDEX_TYPE_UINT32;
			desc.mDesc.mSize = sizeof(uint) * (uint)subMesh.mIndices.size();
			desc.pData = subMesh.mIndices.data();
			desc.ppBuffer = &pMeshBatch->pIndicesStream;
			addResource(&desc);
		}
	}

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
		addResource(&desc);
	}

	AssignSponzaTextures();
	finishResourceLoading();
	return true;
}

void CreateBVHBuffers()
{
	tinystl::vector<AABBox> triBBoxes;
	triBBoxes.reserve(1000000);

	//create buffers for BVH

	addPropToPrimitivesAABBList(triBBoxes, SponzaProp);

	int count = (int)triBBoxes.size();
	//BVHNode* bvhRoot = createBVHNode(triBBoxes, 0, count-1);
	BVHNode* bvhRoot = createBVHNodeSHA(triBBoxes, 0, count - 1, FLT_MAX);

	gWholeSceneBBox = bvhRoot->BoundingBox;

	const int maxNoofElements = 1000000;
	uint8*    bvhTreeNodes = (uint8*)conf_malloc(maxNoofElements * sizeof(BVHLeafBBox));

	int dataOffset = 0;
	int index = 0;
	writeBVHTree(bvhRoot, bvhRoot, bvhTreeNodes, dataOffset, index);

	//terminate BVH tree
	BVHNodeBBox* bbox = (BVHNodeBBox*)(bvhTreeNodes + dataOffset);
	bbox->MinBounds.w = 0;
	dataOffset += sizeof(BVHNodeBBox);

	BufferLoadDesc desc = {};
	desc.mDesc.mFormat = ImageFormat::RGBA32F;
	desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
	desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	desc.mDesc.mSize = dataOffset;
	desc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
	desc.mDesc.mElementCount = desc.mDesc.mSize / sizeof(float4);
	desc.mDesc.mStructStride = sizeof(float4);
	desc.pData = bvhTreeNodes;
	desc.ppBuffer = &BVHBoundingBoxesBuffer;
	addResource(&desc);

	conf_free(bvhTreeNodes);
	deleteBVHTree(bvhRoot);
}

void CreateRenderTargets()
{
	//Create rendertargets
	{
		ClearValue colorClearBlack = { 0.0f, 0.0f, 0.0f, 0.0f };

		// Add G-buffer render targets
		{
			RenderTargetDesc rtDesc = {};
			rtDesc.mArraySize = 1;
			rtDesc.mClearValue = colorClearBlack;
			rtDesc.mDepth = 1;

			rtDesc.mWidth = mSettings.mWidth;
			rtDesc.mHeight = mSettings.mHeight;
			rtDesc.mSampleCount = SAMPLE_COUNT_1;
			rtDesc.mSampleQuality = 0;

			rtDesc.pDebugName = L"G-Buffer RTs";

			RenderTarget* rendertarget;

			//Add albedo RT
			rtDesc.mFormat = ImageFormat::RGBA8;
			addRenderTarget(pRenderer, &rtDesc, &rendertarget);
			RenderPasses[RenderPass::GBuffer]->RenderTargets.push_back(rendertarget);

			//Add normals RT
			rtDesc.mFormat = ImageFormat::RGBA8S;
			addRenderTarget(pRenderer, &rtDesc, &rendertarget);
			RenderPasses[RenderPass::GBuffer]->RenderTargets.push_back(rendertarget);
		}

		// Add depth buffer
		{
			RenderTargetDesc depthRT = {};
			depthRT.mArraySize = 1;
			depthRT.mClearValue = { 1.0f, 0 };
			depthRT.mDepth = 1;
			depthRT.mFormat = ImageFormat::D32F;
			depthRT.mHeight = mSettings.mHeight;
			depthRT.mSampleCount = SAMPLE_COUNT_1;
			depthRT.mSampleQuality = 0;
			depthRT.mWidth = mSettings.mWidth;
			depthRT.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
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
			desc.mFormat = ImageFormat::R8;
			desc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
			desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
			desc.mSampleCount = SAMPLE_COUNT_1;
			desc.mHostVisible = false;
			textureDesc.pDesc = &desc;

			Texture* pTexture;
			textureDesc.ppTexture = &pTexture;
			addResource(&textureDesc);

			RenderPasses[RenderPass::RaytracedShadows]->Textures.push_back(pTexture);
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
			desc.mFormat = ImageFormat::RG11B10F;
			desc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
			desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
			desc.mSampleCount = SAMPLE_COUNT_1;
			desc.mHostVisible = false;
			textureDesc.pDesc = &desc;

			Texture* pTexture;
			textureDesc.ppTexture = &pTexture;
			addResource(&textureDesc);

			RenderPasses[RenderPass::Lighting]->Textures.push_back(pTexture);
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
			desc.mFormat = ImageFormat::RG11B10F;
			desc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
			desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
			desc.mSampleCount = SAMPLE_COUNT_1;
			desc.mHostVisible = false;
			textureDesc.pDesc = &desc;

			Texture* pTexture;
			textureDesc.ppTexture = &pTexture;
			addResource(&textureDesc);

			RenderPasses[RenderPass::Composite]->Textures.push_back(pTexture);
		}
	}
}

bool Load()
{
	if (!addSwapChain())
		return false;

	if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
		return false;

	CreateRenderTargets();

	//add gbuffer pipeline
	{
		// Create vertex layout for g-prepass
		VertexLayout vertexLayoutGPrepass = {};
		{
			vertexLayoutGPrepass.mAttribCount = 3;

			vertexLayoutGPrepass.mAttribs[0].mSemantic = SEMANTIC_POSITION;
			vertexLayoutGPrepass.mAttribs[0].mFormat = ImageFormat::RGB32F;
			vertexLayoutGPrepass.mAttribs[0].mBinding = 0;
			vertexLayoutGPrepass.mAttribs[0].mLocation = 0;
			vertexLayoutGPrepass.mAttribs[0].mOffset = 0;

			//normals
			vertexLayoutGPrepass.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
			vertexLayoutGPrepass.mAttribs[1].mFormat = ImageFormat::RGB32F;
			vertexLayoutGPrepass.mAttribs[1].mLocation = 1;
			vertexLayoutGPrepass.mAttribs[1].mBinding = 1;
			vertexLayoutGPrepass.mAttribs[1].mOffset = 0;

			//texture
			vertexLayoutGPrepass.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
			vertexLayoutGPrepass.mAttribs[2].mFormat = ImageFormat::RG32F;
			vertexLayoutGPrepass.mAttribs[2].mLocation = 2;
			vertexLayoutGPrepass.mAttribs[2].mBinding = 2;
			vertexLayoutGPrepass.mAttribs[2].mOffset = 0;
		}

		//set up g-prepass buffer formats
		ImageFormat::Enum deferredFormats[GBufferRT::Noof] = {};
		bool              deferredSrgb[GBufferRT::Noof] = {};
		for (uint32_t i = 0; i < GBufferRT::Noof; ++i)
		{
			deferredFormats[i] = RenderPasses[RenderPass::GBuffer]->RenderTargets[i]->mDesc.mFormat;
			deferredSrgb[i] = false;
		}

		GraphicsPipelineDesc pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = GBufferRT::Noof;
		pipelineSettings.pDepthState = pDepthDefault;

		pipelineSettings.pColorFormats = deferredFormats;
		pipelineSettings.pSrgbValues = deferredSrgb;

		pipelineSettings.mSampleCount = RenderPasses[RenderPass::GBuffer]->RenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = RenderPasses[RenderPass::GBuffer]->RenderTargets[0]->mDesc.mSampleQuality;

		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		pipelineSettings.pRootSignature = RenderPasses[RenderPass::GBuffer]->pRootSignature;
		pipelineSettings.pShaderProgram = RenderPasses[RenderPass::GBuffer]->pShader;
		pipelineSettings.pVertexLayout = &vertexLayoutGPrepass;
		pipelineSettings.pRasterizerState = pRasterDefault;

		addPipeline(pRenderer, &pipelineSettings, &RenderPasses[RenderPass::GBuffer]->pPipeline);
	}

	//create shadows pipeline
	{
		ComputePipelineDesc pipelineDesc = { 0 };
		pipelineDesc.pRootSignature = RenderPasses[RenderPass::RaytracedShadows]->pRootSignature;
		pipelineDesc.pShaderProgram = RenderPasses[RenderPass::RaytracedShadows]->pShader;
		addComputePipeline(pRenderer, &pipelineDesc, &RenderPasses[RenderPass::RaytracedShadows]->pPipeline);
	}

	//create lighting pipeline
	{
		ComputePipelineDesc pipelineDesc = { 0 };
		pipelineDesc.pRootSignature = RenderPasses[RenderPass::Lighting]->pRootSignature;
		pipelineDesc.pShaderProgram = RenderPasses[RenderPass::Lighting]->pShader;
		addComputePipeline(pRenderer, &pipelineDesc, &RenderPasses[RenderPass::Lighting]->pPipeline);
	}

	//create composite pipeline
	{
		ComputePipelineDesc pipelineDesc = { 0 };
		pipelineDesc.pRootSignature = RenderPasses[RenderPass::Composite]->pRootSignature;
		pipelineDesc.pShaderProgram = RenderPasses[RenderPass::Composite]->pShader;
		addComputePipeline(pRenderer, &pipelineDesc, &RenderPasses[RenderPass::Composite]->pPipeline);
	}

	//create copy to backbuffer pipeline
	{
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 0;

		GraphicsPipelineDesc pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pRasterizerState = pRasterDefault;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pDepthState = pDepthNone;

		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;

		pipelineSettings.pRootSignature = RenderPasses[RenderPass::CopyToBackbuffer]->pRootSignature;
		pipelineSettings.pShaderProgram = RenderPasses[RenderPass::CopyToBackbuffer]->pShader;

		addPipeline(pRenderer, &pipelineSettings, &RenderPasses[RenderPass::CopyToBackbuffer]->pPipeline);
	}

	//make the camera point towards the centre of the scene;
	vec3 midpoint = 0.5f * (gWholeSceneBBox.MaxBounds + gWholeSceneBBox.MinBounds);
	pCameraController->moveTo(midpoint - vec3(1050, 350, 0));
	pCameraController->lookAt(midpoint - vec3(0, 450, 0));

#ifdef TARGET_IOS
	if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0], 0))
		return false;
#endif

	return true;
}

void Unload()
{
	waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

	gAppUI.Unload();

#ifdef TARGET_IOS
	gVirtualJoystick.Unload();
#endif

	//Delete rendering passes Textures and Render targets
	for (RenderPassMap::iterator iter = RenderPasses.begin(); iter != RenderPasses.end(); ++iter)
	{
		RenderPassData* pass = iter->second;

		for (RenderTarget* rt : pass->RenderTargets)
		{
			removeRenderTarget(pRenderer, rt);
		}

		for (Texture* texture : pass->Textures)
		{
			removeResource(texture);
		}
		pass->RenderTargets.clear();
		pass->Textures.clear();

		if (pass->pPipeline)
		{
			removePipeline(pRenderer, pass->pPipeline);
			pass->pPipeline = NULL;
		}
	}

	removeRenderTarget(pRenderer, pDepthBuffer);
	removeSwapChain(pRenderer, pSwapChain);
	pDepthBuffer = NULL;
	pSwapChain = NULL;
}

void Update(float deltaTime)
{
	if (getKeyDown(KEY_BUTTON_X))
	{
		RecenterCameraView(85.0f);
	}

	static float LightRotationX = 0.0f;
	if (getKeyDown(KEY_PAD_RIGHT))
	{
		LightRotationX += 0.004f;
	}
	else if (getKeyDown(KEY_PAD_LEFT))
	{
		LightRotationX -= 0.004f;
	}

	static float LightRotationZ = 0.0f;
	if (getKeyDown(KEY_PAD_UP))
	{
		LightRotationZ += 0.004f;
	}
	else if (getKeyDown(KEY_PAD_DOWN))
	{
		LightRotationZ -= 0.004f;
	}

	pCameraController->update(deltaTime);

	Vector4 lightDir = { 0, 1, 0, 0 };
	mat4    lightRotMat = mat4::rotationX(LightRotationX) * mat4::rotationZ(LightRotationZ);

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
			(float)RenderPasses[RenderPass::RaytracedShadows]->Textures[0]->mDesc.mWidth,
			(float)RenderPasses[RenderPass::RaytracedShadows]->Textures[0]->mDesc.mHeight,
			1.0f / RenderPasses[RenderPass::RaytracedShadows]->Textures[0]->mDesc.mWidth,
			1.0f / RenderPasses[RenderPass::RaytracedShadows]->Textures[0]->mDesc.mHeight);

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
			(float)RenderPasses[RenderPass::Lighting]->Textures[0]->mDesc.mWidth,
			(float)RenderPasses[RenderPass::Lighting]->Textures[0]->mDesc.mHeight,
			1.0f / RenderPasses[RenderPass::Lighting]->Textures[0]->mDesc.mWidth,
			1.0f / RenderPasses[RenderPass::Lighting]->Textures[0]->mDesc.mHeight);
		gLightPassUniformData.mLightDir = lightDir;
	}

	//update Composite pass constant buffer
	{
		gDefaultPassUniformData.mRTSize = vec4(
			(float)RenderPasses[RenderPass::Composite]->Textures[0]->mDesc.mWidth,
			(float)RenderPasses[RenderPass::Composite]->Textures[0]->mDesc.mHeight,
			1.0f / RenderPasses[RenderPass::Composite]->Textures[0]->mDesc.mWidth,
			1.0f / RenderPasses[RenderPass::Composite]->Textures[0]->mDesc.mHeight);
	}

	gFrameNumber++;
}

void Draw()
{
	tinystl::vector<Cmd*> allCmds;
	acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);
	/************************************************************************/
	// Update uniform buffers
	/************************************************************************/
	BufferUpdateDesc desc = { RenderPasses[RenderPass::GBuffer]->pPerPassCB[gFrameIndex], &gPrepasUniformData };
	updateResource(&desc);

	desc = { RenderPasses[RenderPass::RaytracedShadows]->pPerPassCB[gFrameIndex], &gShadowPassUniformData };
	updateResource(&desc);

	desc = { RenderPasses[RenderPass::Lighting]->pPerPassCB[gFrameIndex], &gLightPassUniformData };
	updateResource(&desc);

	desc = { RenderPasses[RenderPass::Composite]->pPerPassCB[gFrameIndex], &gDefaultPassUniformData };
	updateResource(&desc);
	/************************************************************************/
	// Rendering
	/************************************************************************/
	RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
	Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
	Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];
	// GPrepass *********************************************************************************
	{
		Cmd* cmd = RenderPasses[RenderPass::GBuffer]->ppCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		//Clear G-buffers and Depth buffer
		LoadActionsDesc loadActions = {};
		for (uint32_t i = 0; i < RenderPasses[RenderPass::GBuffer]->RenderTargets.size(); ++i)
		{
			loadActions.mLoadActionsColor[i] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[i] = RenderPasses[RenderPass::GBuffer]->RenderTargets[i]->mDesc.mClearValue;
		}

		// Clear depth to the far plane and stencil to 0
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = { 1.0f, 0.0f };

		// Transfer G-buffers to render target state for each buffer
		TextureBarrier barrier;
		for (uint32_t i = 0; i < RenderPasses[RenderPass::GBuffer]->RenderTargets.size(); ++i)
		{
			barrier = { RenderPasses[RenderPass::GBuffer]->RenderTargets[i]->pTexture, RESOURCE_STATE_RENDER_TARGET };
			cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);
		}

		// Transfer DepthBuffer to a DephtWrite State
		barrier = { pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		//Set rendertargets and viewports
		cmdBindRenderTargets(
			cmd, (uint32_t)RenderPasses[RenderPass::GBuffer]->RenderTargets.size(), RenderPasses[RenderPass::GBuffer]->RenderTargets.data(),
			pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)RenderPasses[RenderPass::GBuffer]->RenderTargets[0]->mDesc.mWidth,
			(float)RenderPasses[RenderPass::GBuffer]->RenderTargets[0]->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(
			cmd, 0, 0, RenderPasses[RenderPass::GBuffer]->RenderTargets[0]->mDesc.mWidth,
			RenderPasses[RenderPass::GBuffer]->RenderTargets[0]->mDesc.mHeight);

		// Draw props
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "GBuffer Pass", true);

		cmdBindPipeline(cmd, RenderPasses[RenderPass::GBuffer]->pPipeline);

		//draw sponza
		{
			DescriptorData params[7] = {};
			params[0].pName = "cbPerPass";
			params[0].ppBuffers = &RenderPasses[RenderPass::GBuffer]->pPerPassCB[gFrameIndex];
			params[1].pName = "cbPerProp";
			params[1].ppBuffers = &SponzaProp.pConstantBuffer;
#ifndef TARGET_IOS
			params[2].pName = "textureMaps";
			params[2].ppTextures = pMaterialTextures;
			params[2].mCount = TOTAL_IMGS;
			cmdBindDescriptors(cmd, RenderPasses[RenderPass::GBuffer]->pRootSignature, 3, params);
#else
			cmdBindDescriptors(cmd, RenderPasses[RenderPass::GBuffer]->pRootSignature, 2, params);
#endif

			struct MaterialMaps
			{
				uint mapIDs[5];
			} data;

			for (MeshBatch* mesh : SponzaProp.MeshBatches)
			{
				int materialID = mesh->MaterialID;
				materialID *= 5;    //because it uses 5 basic textures for rendering BRDF

				//use bindless textures on all platforms except for iOS
#ifndef TARGET_IOS
				params[0].pName = "cbTextureRootConstants";
				params[0].pRootConstant = &data;

				for (int j = 0; j < 5; ++j)
				{
					data.mapIDs[j] = gSponzaTextureIndexforMaterial[materialID + j];
				}
				//TODO: If we use more than albedo on iOS we need to bind every texture manually and update
				//descriptor param count.
				//one descriptor param if using bindless textures
				cmdBindDescriptors(cmd, RenderPasses[RenderPass::GBuffer]->pRootSignature, 1, params);
#else
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
				cmdBindDescriptors(cmd, RenderPasses[RenderPass::GBuffer]->pRootSignature, 1, params);
#endif

				Buffer* pVertexBuffers[] = { mesh->pPositionStream, mesh->pNormalStream, mesh->pUVStream };
				cmdBindVertexBuffer(cmd, 3, pVertexBuffers, NULL);

				cmdBindIndexBuffer(cmd, mesh->pIndicesStream, 0);

				cmdDrawIndexed(cmd, mesh->NoofIndices, 0, 0);
			}
		}

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		endCmd(cmd);
		allCmds.push_back(cmd);
	}

	// Raytraced shadow pass ************************************************************************
	{
		Cmd* cmd = RenderPasses[RenderPass::RaytracedShadows]->ppCmds[gFrameIndex];

		beginCmd(cmd);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Raytraced shadow Pass", true);

		// Transfer DepthBuffer and normals to SRV State
		TextureBarrier barriers[] = { { pDepthBuffer->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
									  { RenderPasses[RenderPass::GBuffer]->RenderTargets[GBufferRT::Normals]->pTexture,
										RESOURCE_STATE_SHADER_RESOURCE },
									  { RenderPasses[RenderPass::RaytracedShadows]->Textures[0], RESOURCE_STATE_UNORDERED_ACCESS } };
		cmdResourceBarrier(cmd, 0, NULL, 3, barriers, false);

		cmdBindPipeline(cmd, RenderPasses[RenderPass::RaytracedShadows]->pPipeline);

		DescriptorData params[9] = {};
		params[0].pName = "cbPerPass";
		params[0].ppBuffers = &RenderPasses[RenderPass::RaytracedShadows]->pPerPassCB[gFrameIndex];
		params[1].pName = "depthBuffer";
		params[1].ppTextures = &pDepthBuffer->pTexture;
		params[2].pName = "normalBuffer";
		params[2].ppTextures = &RenderPasses[RenderPass::GBuffer]->RenderTargets[GBufferRT::Normals]->pTexture;
		params[3].pName = "BVHTree";
		params[3].ppBuffers = &BVHBoundingBoxesBuffer;
		params[4].pName = "outputRT";
		params[4].ppTextures = &RenderPasses[RenderPass::RaytracedShadows]->Textures[0];

		cmdBindDescriptors(cmd, RenderPasses[RenderPass::RaytracedShadows]->pRootSignature, 5, params);

		const uint32_t threadGroupSizeX = RenderPasses[RenderPass::RaytracedShadows]->Textures[0]->mDesc.mWidth / 8 + 1;
		const uint32_t threadGroupSizeY = RenderPasses[RenderPass::RaytracedShadows]->Textures[0]->mDesc.mHeight / 8 + 1;

		cmdDispatch(cmd, threadGroupSizeX, threadGroupSizeY, 1);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		endCmd(cmd);
		allCmds.push_back(cmd);
	}

	// Lighting pass *********************************************************************************
	{
		Cmd* cmd = RenderPasses[RenderPass::Lighting]->ppCmds[gFrameIndex];

		beginCmd(cmd);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Lighting Pass", true);

		// Transfer shadowbuffer to SRV and lightbuffer to UAV states
		TextureBarrier barriers[] = { { RenderPasses[RenderPass::RaytracedShadows]->Textures[0], RESOURCE_STATE_SHADER_RESOURCE },
									  { RenderPasses[RenderPass::Lighting]->Textures[0], RESOURCE_STATE_UNORDERED_ACCESS } };
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers, false);

		cmdBindPipeline(cmd, RenderPasses[RenderPass::Lighting]->pPipeline);

		DescriptorData params[4] = {};
		params[0].pName = "cbPerPass";
		params[0].ppBuffers = &RenderPasses[RenderPass::Lighting]->pPerPassCB[gFrameIndex];
		params[1].pName = "normalbuffer";
		params[1].ppTextures = &RenderPasses[RenderPass::GBuffer]->RenderTargets[GBufferRT::Normals]->pTexture;
		params[2].pName = "shadowbuffer";
		params[2].ppTextures = &RenderPasses[RenderPass::RaytracedShadows]->Textures[0];
		params[3].pName = "outputRT";
		params[3].ppTextures = &RenderPasses[RenderPass::Lighting]->Textures[0];

		cmdBindDescriptors(cmd, RenderPasses[RenderPass::Lighting]->pRootSignature, 4, params);

		const uint32_t threadGroupSizeX = RenderPasses[RenderPass::Lighting]->Textures[0]->mDesc.mWidth / 16 + 1;
		const uint32_t threadGroupSizeY = RenderPasses[RenderPass::Lighting]->Textures[0]->mDesc.mHeight / 16 + 1;

		cmdDispatch(cmd, threadGroupSizeX, threadGroupSizeY, 1);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		endCmd(cmd);
		allCmds.push_back(cmd);
	}

	// Composite pass *********************************************************************************
	{
		Cmd* cmd = RenderPasses[RenderPass::Composite]->ppCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Composite Pass", true);

		// Transfer albedo and lighting to SRV State
		TextureBarrier barriers[] = { { RenderPasses[RenderPass::Lighting]->Textures[0], RESOURCE_STATE_SHADER_RESOURCE },
									  { RenderPasses[RenderPass::GBuffer]->RenderTargets[GBufferRT::Albedo]->pTexture,
										RESOURCE_STATE_SHADER_RESOURCE },
									  { RenderPasses[RenderPass::Composite]->Textures[0], RESOURCE_STATE_UNORDERED_ACCESS } };
		cmdResourceBarrier(cmd, 0, NULL, 3, barriers, false);

		cmdBindPipeline(cmd, RenderPasses[RenderPass::Composite]->pPipeline);

		DescriptorData params[4] = {};
		params[0].pName = "albedobuffer";
		params[0].ppTextures = &RenderPasses[RenderPass::GBuffer]->RenderTargets[GBufferRT::Albedo]->pTexture;
		params[1].pName = "lightbuffer";
		params[1].ppTextures = &RenderPasses[RenderPass::Lighting]->Textures[0];
		params[2].pName = "outputRT";
		params[2].ppTextures = &RenderPasses[RenderPass::Composite]->Textures[0];

		cmdBindDescriptors(cmd, RenderPasses[RenderPass::Composite]->pRootSignature, 3, params);

		const uint32_t threadGroupSizeX = RenderPasses[RenderPass::Composite]->Textures[0]->mDesc.mWidth / 16 + 1;
		const uint32_t threadGroupSizeY = RenderPasses[RenderPass::Composite]->Textures[0]->mDesc.mHeight / 16 + 1;

		cmdDispatch(cmd, threadGroupSizeX, threadGroupSizeY, 1);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
		barriers[1] = { RenderPasses[RenderPass::Composite]->Textures[0], RESOURCE_STATE_SHADER_RESOURCE };

		cmdResourceBarrier(cmd, 0, NULL, 2, barriers, true);

		endCmd(cmd);
		allCmds.push_back(cmd);
	}

	// Copy results to the backbuffer & draw text *****************************************************************
	{
		Cmd* cmd = RenderPasses[RenderPass::CopyToBackbuffer]->ppCmds[gFrameIndex];
		beginCmd(cmd);

		LoadActionsDesc loadActions = {};
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Copy to Backbuffer Pass");

		// Draw  results
		cmdBindPipeline(cmd, RenderPasses[RenderPass::CopyToBackbuffer]->pPipeline);

		DescriptorData params[4] = {};
		params[0].pName = "inputRT";
		params[0].ppTextures = &RenderPasses[RenderPass::Composite]->Textures[0];
		cmdBindDescriptors(cmd, RenderPasses[RenderPass::CopyToBackbuffer]->pRootSignature, 1, params);

		//draw fullscreen triangle
		cmdDraw(cmd, 3, 0);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		gTimer.GetUSec(true);

#ifdef TARGET_IOS
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		drawDebugText(cmd, 8, 15, tinystl::string::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);

#ifndef METAL    // Metal doesn't support GPU profilers
		drawDebugText(cmd, 8, 40, tinystl::string::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &gFrameTimeDraw);
		drawDebugGpuProfile(cmd, 8, 65, pGpuProfiler, NULL);
#endif

		gAppUI.Draw(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		TextureBarrier barriers[] = {
			{ pRenderTarget->pTexture, RESOURCE_STATE_PRESENT },
		};

		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, true);

		cmdEndGpuFrameProfile(cmd, pGpuProfiler);

		endCmd(cmd);
		allCmds.push_back(cmd);
	}

	queueSubmit(
		pGraphicsQueue, (uint32_t)allCmds.size(), allCmds.data(), pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1,
		&pRenderCompleteSemaphore);
	queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
	waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);
}

tinystl::string GetName() { return "09a_HybridRaytracing"; }

bool addSwapChain()
{
	SwapChainDesc swapChainDesc = {};
	swapChainDesc.pWindow = pWindow;
	swapChainDesc.ppPresentQueues = &pGraphicsQueue;
	swapChainDesc.mPresentQueueCount = 1;
	swapChainDesc.mWidth = mSettings.mWidth;
	swapChainDesc.mHeight = mSettings.mHeight;
	swapChainDesc.mImageCount = gImageCount;
	swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
	swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
	swapChainDesc.mEnableVsync = false;
	::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

	return pSwapChain != NULL;
}

void RecenterCameraView(float maxDistance, vec3 lookAt = vec3(0))
{
	vec3 p = pCameraController->getViewPosition();
	vec3 d = p - lookAt;

	float lenSqr = lengthSqr(d);
	if (lenSqr > (maxDistance * maxDistance))
	{
		d *= (maxDistance / sqrtf(lenSqr));
	}

	p = d + lookAt;
	pCameraController->moveTo(p);
	pCameraController->lookAt(lookAt);
}

/// Camera controller functionality
static bool cameraInputEvent(const ButtonData* data)
{
	pCameraController->onInputEvent(data);
	return true;
}
}
;

DEFINE_APPLICATION_MAIN(HybridRaytracing)

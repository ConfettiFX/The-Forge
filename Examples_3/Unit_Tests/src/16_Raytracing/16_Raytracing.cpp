/*
* Copyright (c) 2017-2022 The Forge Interactive Inc.
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

// Unit Test to create Bottom and Top Level Acceleration Structures using Raytracing API.



//Interfaces
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"

#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

// Raytracing
#include "../../../../Common_3/Graphics/Interfaces/IRay.h"

//tiny stl
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"

//Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"
#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"


// The denoiser is only supported on macOS Catalina and higher. If you want to use the denoiser, set
// USE_DENOISER to 1 in the #if block below.
#if defined(METAL) && !defined(TARGET_IOS) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101500
#define USE_DENOISER 1
#else
#define USE_DENOISER 0
#endif

ICameraController* pCameraController = NULL;

ProfileToken gGpuProfileToken;

struct ShadersConfigBlock
{
	mat4 mCameraToWorld;
	float2 mZ1PlaneSize;
	float mProjNear;
	float mProjFarMinusNear;
	float3 mLightDirection;
	float mRandomSeed;
	float2 mSubpixelJitter;
	uint mFrameIndex;
	uint mFramesSinceCameraMove;
};

struct DenoiserUniforms {
	mat4 mWorldToCamera;
	mat4 mCameraToProjection;
	mat4 mWorldToProjectionPrevious;
	float2 mRTInvSize;
	uint mFrameIndex;
	uint mPadding;
};

//Sponza
const char*           gModel_Sponza_File = "Sponza.gltf";

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

// Have a uniform for object data
struct UniformObjData
{
    mat4  mWorldMat;
    float mRoughness = 0.04f;
    float mMetallic = 0.0f;
    int   pbrMaterials = -1;
    int32_t : 32; // padding
};

struct PropData
{
	Geometry* pGeom;
    Buffer* pMaterialIdStream; // one per primitive
	Buffer* pMaterialTexturesStream; // 5 per material.
	
    Buffer* pConstantBuffer;
};

PropData SponzaProp;

#define TOTAL_IMGS 84
Texture* pMaterialTextures[TOTAL_IMGS];

eastl::vector<int> gSponzaTextureIndexForMaterial;

uint32_t gFontID = 0; 

struct PathTracingData {
	mat4 mHistoryProjView;
	float3 mHistoryLightDirection;
	uint mFrameIndex;
	uint mHaltonIndex;
	uint mLastCameraMoveFrame;
};

void AssignSponzaTextures()
{
	int AO = 5;
	int NoMetallic = 6;

	//00 : leaf
	gSponzaTextureIndexForMaterial.push_back(66);
	gSponzaTextureIndexForMaterial.push_back(67);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(68);
	gSponzaTextureIndexForMaterial.push_back(AO);

	//01 : vase_round
	gSponzaTextureIndexForMaterial.push_back(78);
	gSponzaTextureIndexForMaterial.push_back(79);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(80);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 02 : 16___Default (gi_flag)
	gSponzaTextureIndexForMaterial.push_back(8);
	gSponzaTextureIndexForMaterial.push_back(8);    // !!!!!!
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(8);    // !!!!!
	gSponzaTextureIndexForMaterial.push_back(AO);

	//03 : Material__57 (Plant)
	gSponzaTextureIndexForMaterial.push_back(75);
	gSponzaTextureIndexForMaterial.push_back(76);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(77);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 04 : Material__298
	gSponzaTextureIndexForMaterial.push_back(9);
	gSponzaTextureIndexForMaterial.push_back(10);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(11);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 05 : bricks
	gSponzaTextureIndexForMaterial.push_back(22);
	gSponzaTextureIndexForMaterial.push_back(23);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(24);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 06 :  arch
	gSponzaTextureIndexForMaterial.push_back(19);
	gSponzaTextureIndexForMaterial.push_back(20);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(21);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 07 : ceiling
	gSponzaTextureIndexForMaterial.push_back(25);
	gSponzaTextureIndexForMaterial.push_back(26);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(27);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 08 : column_a
	gSponzaTextureIndexForMaterial.push_back(28);
	gSponzaTextureIndexForMaterial.push_back(29);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(30);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 09 : Floor
	gSponzaTextureIndexForMaterial.push_back(60);
	gSponzaTextureIndexForMaterial.push_back(61);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(62);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 10 : column_c
	gSponzaTextureIndexForMaterial.push_back(34);
	gSponzaTextureIndexForMaterial.push_back(35);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(36);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 11 : details
	gSponzaTextureIndexForMaterial.push_back(45);
	gSponzaTextureIndexForMaterial.push_back(47);
	gSponzaTextureIndexForMaterial.push_back(46);
	gSponzaTextureIndexForMaterial.push_back(48);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 12 : column_b
	gSponzaTextureIndexForMaterial.push_back(31);
	gSponzaTextureIndexForMaterial.push_back(32);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(33);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 13 : flagpole
	gSponzaTextureIndexForMaterial.push_back(57);
	gSponzaTextureIndexForMaterial.push_back(58);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(59);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 14 : fabric_e (green)
	gSponzaTextureIndexForMaterial.push_back(51);
	gSponzaTextureIndexForMaterial.push_back(52);
	gSponzaTextureIndexForMaterial.push_back(53);
	gSponzaTextureIndexForMaterial.push_back(54);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 15 : fabric_d (blue)
	gSponzaTextureIndexForMaterial.push_back(49);
	gSponzaTextureIndexForMaterial.push_back(50);
	gSponzaTextureIndexForMaterial.push_back(53);
	gSponzaTextureIndexForMaterial.push_back(54);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 16 : fabric_a (red)
	gSponzaTextureIndexForMaterial.push_back(55);
	gSponzaTextureIndexForMaterial.push_back(56);
	gSponzaTextureIndexForMaterial.push_back(53);
	gSponzaTextureIndexForMaterial.push_back(54);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 17 : fabric_g (curtain_blue)
	gSponzaTextureIndexForMaterial.push_back(37);
	gSponzaTextureIndexForMaterial.push_back(38);
	gSponzaTextureIndexForMaterial.push_back(43);
	gSponzaTextureIndexForMaterial.push_back(44);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 18 : fabric_c (curtain_red)
	gSponzaTextureIndexForMaterial.push_back(41);
	gSponzaTextureIndexForMaterial.push_back(42);
	gSponzaTextureIndexForMaterial.push_back(43);
	gSponzaTextureIndexForMaterial.push_back(44);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 19 : fabric_f (curtain_green)
	gSponzaTextureIndexForMaterial.push_back(39);
	gSponzaTextureIndexForMaterial.push_back(40);
	gSponzaTextureIndexForMaterial.push_back(43);
	gSponzaTextureIndexForMaterial.push_back(44);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 20 : chain
	gSponzaTextureIndexForMaterial.push_back(12);
	gSponzaTextureIndexForMaterial.push_back(14);
	gSponzaTextureIndexForMaterial.push_back(13);
	gSponzaTextureIndexForMaterial.push_back(15);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 21 : vase_hanging
	gSponzaTextureIndexForMaterial.push_back(72);
	gSponzaTextureIndexForMaterial.push_back(73);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(74);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 22 : vase
	gSponzaTextureIndexForMaterial.push_back(69);
	gSponzaTextureIndexForMaterial.push_back(70);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(71);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 23 : Material__25 (lion)
	gSponzaTextureIndexForMaterial.push_back(16);
	gSponzaTextureIndexForMaterial.push_back(17);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(18);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 24 : roof
	gSponzaTextureIndexForMaterial.push_back(63);
	gSponzaTextureIndexForMaterial.push_back(64);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(65);
	gSponzaTextureIndexForMaterial.push_back(AO);

	// 25 : Material__47 - it seems missing
	gSponzaTextureIndexForMaterial.push_back(19);
	gSponzaTextureIndexForMaterial.push_back(20);
	gSponzaTextureIndexForMaterial.push_back(NoMetallic);
	gSponzaTextureIndexForMaterial.push_back(21);
	gSponzaTextureIndexForMaterial.push_back(AO);
}

bool initSponza()
{
	for (int i = 0; i < TOTAL_IMGS; ++i)
	{
		TextureLoadDesc textureDesc = {};
		textureDesc.pFileName = pMaterialImageFileNames[i];
		textureDesc.ppTexture = &pMaterialTextures[i];
		if (strstr(pMaterialImageFileNames[i], "Albedo") ||
			strstr(pMaterialImageFileNames[i], "albedo") ||
			strstr(pMaterialImageFileNames[i], "diffuse"))
			textureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
		addResource(&textureDesc, NULL);
	}

	VertexLayout vertexLayout = {};
	vertexLayout.mAttribCount = 3;

	vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
	vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
	vertexLayout.mAttribs[0].mBinding = 0;
	vertexLayout.mAttribs[0].mLocation = 0;
	vertexLayout.mAttribs[0].mOffset = 0;

	//normals
	vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
	vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
	vertexLayout.mAttribs[1].mLocation = 1;
	vertexLayout.mAttribs[1].mBinding = 1;
	vertexLayout.mAttribs[1].mOffset = 0;

	//texture
	vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
	vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
	vertexLayout.mAttribs[2].mLocation = 2;
	vertexLayout.mAttribs[2].mBinding = 2;
	vertexLayout.mAttribs[2].mOffset = 0;

	SyncToken token = {};
	GeometryLoadDesc loadDesc = {};
	loadDesc.pFileName = gModel_Sponza_File;
	loadDesc.ppGeometry = &SponzaProp.pGeom;
	loadDesc.pVertexLayout = &vertexLayout;
	loadDesc.mFlags = GEOMETRY_LOAD_FLAG_SHADOWED;
	addResource(&loadDesc, &token);

	waitForToken(&token);

	uint32_t gMaterialIds[] =
	{
		0, 3, 1, 4, 5, 6, 7, 8, 6, 9, 7, 6, 10, 5, 7, 5, 6,
		7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7,
		6, 5, 6, 5, 11, 5, 11, 5, 11, 5, 10, 5, 9, 8, 6, 12,
		2, 5, 13, 0, 14, 15, 16, 14, 15, 14, 16, 15, 13, 17, 18,
		19, 18, 19, 18, 17, 19, 18, 17, 20, 21, 20, 21, 20, 21, 20,
		21, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 22, 23, 4, 23, 4, 5, 24, 5,
	};

	uint32_t totalPrimitiveCount = SponzaProp.pGeom->mIndexCount / 3;
	BufferLoadDesc desc = {};
	desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER_RAW;
	desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	desc.mDesc.mStartState = RESOURCE_STATE_COMMON;
	desc.mDesc.mSize = totalPrimitiveCount * sizeof(uint32_t);
	desc.mDesc.mElementCount = totalPrimitiveCount;
	desc.ppBuffer = &SponzaProp.pMaterialIdStream;
	addResource(&desc, NULL);

	BufferUpdateDesc updateDesc = {};
	updateDesc.pBuffer = SponzaProp.pMaterialIdStream;
	beginUpdateResource(&updateDesc);

	for (uint32_t i = 0, count = 0; i < SponzaProp.pGeom->mDrawArgCount; ++i)
    {
		size_t meshPrimitiveCount = SponzaProp.pGeom->pDrawArgs[i].mIndexCount / 3;
		for (uint32_t j = 0; j < meshPrimitiveCount; ++j)
		{
			((uint32_t*)updateDesc.pMappedData)[count++] = gMaterialIds[i];
		}
	}

	endUpdateResource(&updateDesc, NULL);

    //set constant buffer for sponza
    {
        UniformObjData data = {};
        data.mWorldMat = mat4::identity();

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
	
	desc.mDesc.mSize = gSponzaTextureIndexForMaterial.size() * sizeof(uint32_t);
	desc.mDesc.mElementCount = (uint32_t)gSponzaTextureIndexForMaterial.size();
	desc.ppBuffer = &SponzaProp.pMaterialTexturesStream;
	desc.pData = gSponzaTextureIndexForMaterial.data();
	addResource(&desc, NULL);
	
	return true;
}

void exitSponza()
{
	if (!SponzaProp.pGeom)
		return;
	
    for (int i = 0; i < TOTAL_IMGS; ++i)
		removeResource(pMaterialTextures[i]);

	gSponzaTextureIndexForMaterial.set_capacity(0);
	
	removeResource(SponzaProp.pGeom);
	removeResource(SponzaProp.pMaterialIdStream);
	removeResource(SponzaProp.pMaterialTexturesStream);
	removeResource(SponzaProp.pConstantBuffer);
}

static float haltonSequence(uint index, uint base)
{
    float f = 1.f;
    float r = 0.f;
    
    while (index > 0) {
        f /= (float)base;
        r += f * (float)(index % base);
        index /= base;
        
    }
    
    return r;
}

class UnitTest_NativeRaytracing : public IApp
{
public:
	UnitTest_NativeRaytracing()
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
        ReadCmdArgs();
	}

    void ReadCmdArgs()
    {
        for (int i = 0; i < argc; i += 1)
        {
            if (strcmp(argv[i], "-w") == 0 && i + 1 < argc)
                mSettings.mWidth = min(max(atoi(argv[i + 1]), 64), 10000);
            else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc)
                mSettings.mHeight = min(max(atoi(argv[i + 1]), 64), 10000);
            else if (strcmp(argv[i], "-f") == 0)
            {
                mSettings.mFullScreen = true;
            }
        }
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

		/************************************************************************/
		// 01 Init Raytracing
		/************************************************************************/\

		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		settings.mShaderTarget = shader_target_6_3;
		initRenderer(GetName(), &settings, &pRenderer);
		initResourceLoaderInterface(pRenderer);

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pQueue);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPools[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pCmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pCmds[i]);
		}

		addSemaphore(pRenderer, &pImageAcquiredSemaphore);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}

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

		const char* ppGpuProfilerName[1] = { "Graphics" };

		// Initialize micro profiler and its UI.
		ProfilerDesc profiler = {};
		profiler.pRenderer = pRenderer;
		profiler.ppQueues = &pQueue; 
		profiler.ppProfilerNames = ppGpuProfilerName; 
		profiler.pProfileTokens = &gGpuProfileToken; 
		profiler.mGpuProfilerCount = 1;
		profiler.mWidthUI = mSettings.mWidth;
		profiler.mHeightUI = mSettings.mHeight;
		initProfiler(&profiler);

		/************************************************************************/
		// GUI
		/************************************************************************/
		UIComponentDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.15f);
		uiCreateComponent(GetName(), &guiDesc, &pGuiWindow);

		SamplerDesc samplerDesc = { FILTER_NEAREST,
									FILTER_NEAREST,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pSampler);

		if (!isRaytracingSupported(pRenderer)) //-V560
		{
			pRaytracing = NULL;
			LabelWidget notSupportedLabel;
			luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Raytracing is not supported on this GPU", &notSupportedLabel, WIDGET_TYPE_LABEL));
		}
		else
		{
			SliderFloatWidget lightDirXSlider;
			lightDirXSlider.pData = &mLightDirection.x;
			lightDirXSlider.mMin = -2.0f;
			lightDirXSlider.mMax = 2.0f;
			lightDirXSlider.mStep = 0.001f;
			luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Light Direction X", &lightDirXSlider, WIDGET_TYPE_SLIDER_FLOAT));

			SliderFloatWidget lightDirYSlider;
			lightDirYSlider.pData = &mLightDirection.y;
			lightDirYSlider.mMin = -2.0f;
			lightDirYSlider.mMax = 2.0f;
			lightDirYSlider.mStep = 0.001f;
			luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Light Direction Y", &lightDirYSlider, WIDGET_TYPE_SLIDER_FLOAT));

			SliderFloatWidget lightDirZSlider;
			lightDirZSlider.pData = &mLightDirection.z;
			lightDirZSlider.mMin = -2.0f;
			lightDirZSlider.mMax = 2.0f;
			lightDirZSlider.mStep = 0.001f;
			luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Light Direction Z", &lightDirZSlider, WIDGET_TYPE_SLIDER_FLOAT));

			/************************************************************************/
			/************************************************************************/
			if (!initSponza())
				return false;

			/************************************************************************/
			// Raytracing setup
			/************************************************************************/
			initRaytracing(pRenderer, &pRaytracing);

			/************************************************************************/
			// 02 Creation Acceleration Structure
			/************************************************************************/
			AccelerationStructureGeometryDesc geomDesc = {};
			geomDesc.mFlags = ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE;
			geomDesc.pVertexArray = SponzaProp.pGeom->pShadow->pAttributes[SEMANTIC_POSITION];
			geomDesc.mVertexCount = (uint32_t)SponzaProp.pGeom->mVertexCount;
			geomDesc.pIndices32 = (uint32_t*)SponzaProp.pGeom->pShadow->pIndices;
			geomDesc.mIndexCount = (uint32_t)SponzaProp.pGeom->mIndexCount;
			geomDesc.mIndexType = INDEX_TYPE_UINT32;

			AccelerationStructureDescBottom bottomASDesc = {};
			bottomASDesc.mDescCount = 1;
			bottomASDesc.pGeometryDescs = &geomDesc;
			bottomASDesc.mFlags = ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

			AccelerationStructureDescTop topAS = {};
			topAS.mBottomASDesc = &bottomASDesc;

			// The transformation matrices for the instances
			mat4 transformation = mat4::identity(); // Identity

			//Construct descriptions for Acceleration Structures Instances
			AccelerationStructureInstanceDesc instanceDesc = {};

			instanceDesc.mFlags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_NONE;
			instanceDesc.mInstanceContributionToHitGroupIndex = 0;
			instanceDesc.mInstanceID = 0;
			instanceDesc.mInstanceMask = 1;
			memcpy(instanceDesc.mTransform, &transformation, sizeof(float[12]));
			instanceDesc.mAccelerationStructureIndex = 0;

			topAS.mInstancesDescCount = 1;
			topAS.pInstanceDescs = &instanceDesc;
			addAccelerationStructure(pRaytracing, &topAS, &pSponzaAS);

			waitForAllResourceLoads();

			// Build Acceleration Structure
			RaytracingBuildASDesc buildASDesc = {};
			unsigned bottomASIndices[] = { 0 };
			buildASDesc.ppAccelerationStructures = &pSponzaAS;
			buildASDesc.pBottomASIndices = &bottomASIndices[0];
			buildASDesc.mBottomASIndicesCount = 1;
			buildASDesc.mCount = 1;
			beginCmd(pCmds[0]);
			cmdBuildAccelerationStructure(pCmds[0], pRaytracing, &buildASDesc);
			endCmd(pCmds[0]);

			QueueSubmitDesc submitDesc = {};
			submitDesc.mCmdCount = 1;
			submitDesc.ppCmds = pCmds;
			submitDesc.pSignalFence = pRenderCompleteFences[0];
			submitDesc.mSubmitDone = true;
			queueSubmit(pQueue, &submitDesc);
			waitForFences(pRenderer, 1, &pRenderCompleteFences[0]);

			

			samplerDesc = { FILTER_LINEAR,       FILTER_LINEAR,       MIPMAP_MODE_LINEAR,
										ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
			addSampler(pRenderer, &samplerDesc, &pLinearSampler);


			/************************************************************************/
			// 04 - Create Shader Binding Table to connect Pipeline with Acceleration Structure
			/************************************************************************/
			BufferLoadDesc ubDesc = {};
			ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			ubDesc.mDesc.mStartState = RESOURCE_STATE_COMMON;
			ubDesc.mDesc.mSize = sizeof(ShadersConfigBlock);
			for (uint32_t i = 0; i < gImageCount; i++)
			{
				ubDesc.ppBuffer = &pRayGenConfigBuffer[i];
				addResource(&ubDesc, NULL);
			}
		}

		TextureDesc uavDesc = {};
		uavDesc.mArraySize = 1;
		uavDesc.mDepth = 1;
#if USE_DENOISER
		uavDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
#else
		uavDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
#endif
		uavDesc.mHeight = mSettings.mHeight;
		uavDesc.mMipLevels = 1;
		uavDesc.mSampleCount = SAMPLE_COUNT_1;
		uavDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		uavDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		uavDesc.mWidth = mSettings.mWidth;
#ifdef METAL
		uavDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
		TextureLoadDesc loadDesc = {};
		loadDesc.pDesc = &uavDesc;
		loadDesc.ppTexture = &pComputeOutput;
		addResource(&loadDesc, NULL);

#if USE_DENOISER
		{
			uavDesc.mFormat = TinyImageFormat_B10G10R10A2_UNORM;
			loadDesc.ppTexture = &pAlbedoTexture;
			addResource(&loadDesc, NULL);

			BufferLoadDesc ubDesc = {};
			ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			ubDesc.mDesc.mSize = sizeof(DenoiserUniforms);
			for (uint32_t i = 0; i < gImageCount; i++)
			{
				ubDesc.ppBuffer = &pDenoiserInputsUniformBuffer[i];
				addResource(&ubDesc, NULL);
			}

			addSSVGFDenoiser(pRenderer, &pDenoiser);
		}
#endif

		InputSystemDesc inputDesc = {};
		inputDesc.pRenderer = pRenderer;
		inputDesc.pWindow = pWindow;
		if (!initInputSystem(&inputDesc))
			return false;
		
		CameraMotionParameters cmp{ 200.0f, 250.0f, 300.0f };
		vec3                   camPos{ 100.0f, 300.0f, 0.0f };
		vec3                   lookAt{ 0, 340, 0 };

		pCameraController = initFpsCameraController(camPos, lookAt);

		pCameraController->setMotionParameters(cmp);
		
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

		mFrameIdx = 0; 

		waitForAllResourceLoads();

		return true;
	}
	
	void Exit()
	{
#if USE_DENOISER
		for (uint32_t i = 0; i < gImageCount; i += 1)
		{
			removeResource(pDenoiserInputsUniformBuffer[i]);
		}
		removeResource(pAlbedoTexture);

		removeSSVGFDenoiser(pDenoiser);
#endif
		removeResource(pComputeOutput);

		exitInputSystem();

		exitCameraController(pCameraController);

		exitProfiler();

		exitUserInterface();

		exitFontSystem(); 

		if (pRaytracing != NULL)
		{
			exitSponza();

			removeSampler(pRenderer, pLinearSampler);

			for (uint32_t i = 0; i < gImageCount; i++)
			{
				removeResource(pRayGenConfigBuffer[i]);
			}

			removeAccelerationStructure(pRaytracing, pSponzaAS);
			removeRaytracing(pRenderer, pRaytracing);
		}

		removeSampler(pRenderer, pSampler);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}
		removeQueue(pRenderer, pQueue);
		exitResourceLoaderInterface(pRenderer);
		exitRenderer(pRenderer);
		pRenderer = NULL; 
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

#if USE_DENOISER
			{
				RenderTargetDesc rtDesc = {};
				rtDesc.mClearValue = { { FLT_MAX, 0, 0, 0 } };
				rtDesc.mWidth = mSettings.mWidth;
				rtDesc.mHeight = mSettings.mHeight;
				rtDesc.mDepth = 1;
				rtDesc.mSampleCount = SAMPLE_COUNT_1;
				rtDesc.mSampleQuality = 0;
				rtDesc.mArraySize = 1;

				rtDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
				addRenderTarget(pRenderer, &rtDesc, &pDepthNormalRenderTarget[0]);
				addRenderTarget(pRenderer, &rtDesc, &pDepthNormalRenderTarget[1]);

				rtDesc.mFormat = TinyImageFormat_R16G16_SFLOAT;
				rtDesc.mClearValue = { { 0, 0 } };
				addRenderTarget(pRenderer, &rtDesc, &pMotionVectorRenderTarget);

				rtDesc.mFormat = TinyImageFormat_D32_SFLOAT;
				rtDesc.mClearValue = { { 1.0f, 0 } };
				rtDesc.mFlags = TEXTURE_CREATION_FLAG_ON_TILE;
				addRenderTarget(pRenderer, &rtDesc, &pDepthRenderTarget);
			}
#endif
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			addPipelines();
		}

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
        waitQueueIdle(pQueue);

		unloadFontSystem(pReloadDesc->mType);
		unloadUserInterface(pReloadDesc->mType);

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			removePipelines();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			removeSwapChain(pRenderer, pSwapChain);
#if USE_DENOISER	
			removeRenderTarget(pRenderer, pMotionVectorRenderTarget);
			removeRenderTarget(pRenderer, pDepthNormalRenderTarget[0]);
			removeRenderTarget(pRenderer, pDepthNormalRenderTarget[1]);
			removeRenderTarget(pRenderer, pDepthRenderTarget);
#endif
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
        PROFILER_SET_CPU_SCOPE("Cpu Profile", "update", 0x222222);

		updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

		pCameraController->update(deltaTime);

		++nFrameCount;
	}

	void Draw()
	{
		if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}

		PROFILER_SET_CPU_SCOPE("Cpu Profile", "draw", 0xffffff);

		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		FenceStatus fenceStatus = {};
		getFenceStatus(pRenderer, pRenderCompleteFences[mFrameIdx], &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFences[mFrameIdx]);

		resetCmdPool(pRenderer, pCmdPools[mFrameIdx]);

		if (pRaytracing != NULL)
		{
			mat4 viewMat = pCameraController->getViewMatrix();

			const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
			const float horizontalFOV = PI / 2.0f;
			const float nearPlane = 0.1f;
			const float farPlane = 6000.f;
			mat4 projMat = mat4::perspective(horizontalFOV, aspectInverse, nearPlane, farPlane);
			mat4 projectView = projMat * viewMat;
			
			bool cameraMoved = memcmp(&projectView, &mPathTracingData.mHistoryProjView, sizeof(mat4)) != 0;
			bool lightMoved = memcmp(&mLightDirection, &mPathTracingData.mHistoryLightDirection, sizeof(float3)) != 0; //-V1014
			
#if USE_DENOISER
			if (lightMoved)
			{
				clearSSVGFDenoiserTemporalHistory(pDenoiser);
			}
#else
			if (cameraMoved || lightMoved)
			{
				mPathTracingData.mFrameIndex = 0;
				mPathTracingData.mHaltonIndex = 0;
			}
#endif
			if (cameraMoved)
			{
				mPathTracingData.mLastCameraMoveFrame = mPathTracingData.mFrameIndex;
			}
			
			ShadersConfigBlock cb;
			cb.mCameraToWorld = inverse(viewMat);
			cb.mProjNear = nearPlane;
			cb.mProjFarMinusNear = farPlane - nearPlane;
			cb.mZ1PlaneSize = float2(1.0f / projMat.getElem(0, 0), 1.0f / projMat.getElem(1, 1));
			cb.mLightDirection = v3ToF3(normalize(f3Tov3(mLightDirection)));
			
			cb.mRandomSeed = (float)sin((double)getUSec(false));
			
			// Loop through the first 16 items in the Halton sequence.
            // The Halton sequence takes one-based indices.
            cb.mSubpixelJitter = float2(haltonSequence(mPathTracingData.mHaltonIndex + 1, 2),
                                                haltonSequence(mPathTracingData.mHaltonIndex + 1, 3));
			
			cb.mFrameIndex = mPathTracingData.mFrameIndex;
			
			cb.mFramesSinceCameraMove = mPathTracingData.mFrameIndex - mPathTracingData.mLastCameraMoveFrame;
			
			BufferUpdateDesc bufferUpdate = { pRayGenConfigBuffer[mFrameIdx] };
			beginUpdateResource(&bufferUpdate);
			*(ShadersConfigBlock*)bufferUpdate.pMappedData = cb;
			endUpdateResource(&bufferUpdate, NULL);

#if USE_DENOISER
			bufferUpdate = {};
			bufferUpdate.pBuffer = pDenoiserInputsUniformBuffer[mFrameIdx];
			beginUpdateResource(&bufferUpdate);
			DenoiserUniforms& denoiserUniforms = *(DenoiserUniforms*)bufferUpdate.pMappedData;
			denoiserUniforms.mWorldToCamera = viewMat;
			denoiserUniforms.mCameraToProjection = projMat; // Unjittered since the depth/normal texture needs to be stable for the denoiser.
			denoiserUniforms.mWorldToProjectionPrevious = mPathTracingData.mHistoryProjView;
			denoiserUniforms.mRTInvSize = float2(1.0f / mSettings.mWidth, 1.0f / mSettings.mHeight);
			denoiserUniforms.mFrameIndex = mPathTracingData.mFrameIndex;
			endUpdateResource(&bufferUpdate, NULL);
#endif
			
			mPathTracingData.mHistoryProjView = projectView;
			mPathTracingData.mHistoryLightDirection = mLightDirection;
			mPathTracingData.mFrameIndex += 1;
			mPathTracingData.mHaltonIndex = (mPathTracingData.mHaltonIndex + 1) % 16;
		}
		
		Cmd* pCmd = pCmds[mFrameIdx];
		beginCmd(pCmd);
		cmdBeginGpuFrameProfile(pCmd, gGpuProfileToken);
		
#if USE_DENOISER
		if (pRaytracing != NULL)
		{
			RenderTarget* depthNormalTarget = pDepthNormalRenderTarget[mPathTracingData.mFrameIndex & 0x1];
			
			RenderTargetBarrier barriers[] = {
				{ pDepthRenderTarget, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
				{ depthNormalTarget, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
				{ pMotionVectorRenderTarget, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
			};
			cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 3, barriers);
			
			RenderTarget* denoiserRTs[] = { depthNormalTarget, pMotionVectorRenderTarget };
			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0] = { { FLT_MAX, 0, 0, 0 } };
			loadActions.mLoadActionsColor[1] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[1] = { { 0, 0, 0, 0 } };
			loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
			loadActions.mClearDepth = { { 1.f } };
			
			cmdBeginGpuTimestampQuery(pCmd, gGpuProfileToken, "Generate Denoiser Inputs");
			cmdBindRenderTargets(pCmd, 2, denoiserRTs, pDepthRenderTarget, &loadActions, NULL, NULL, 0, 0);
			
			cmdBindPipeline(pCmd, pDenoiserInputsPipeline);
			
			cmdBindDescriptorSet(pCmd, mFrameIdx, pDenoiserInputsDescriptorSet);
			
			cmdBindVertexBuffer(pCmd, 2, SponzaProp.pGeom->pVertexBuffers, SponzaProp.pGeom->mVertexStrides, NULL);
			
			cmdBindIndexBuffer(pCmd, SponzaProp.pGeom->pIndexBuffer, 0, (IndexType)SponzaProp.pGeom->mIndexType);
			cmdDrawIndexed(pCmd, SponzaProp.pGeom->mIndexCount, 0, 0);
			
			cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, 0, 0);
			cmdEndGpuTimestampQuery(pCmd, gGpuProfileToken);
		}
#endif
		
		/************************************************************************/
		// Transition UAV texture so raytracing shader can write to it
		/************************************************************************/
		cmdBeginGpuTimestampQuery(pCmd, gGpuProfileToken, "Path Trace Scene");
		TextureBarrier uavBarrier = { pComputeOutput, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &uavBarrier, 0, NULL);
		/************************************************************************/
		// Perform raytracing
		/************************************************************************/
		if (pRaytracing != NULL)
		{
            cmdBindPipeline(pCmd, pPipeline);

            cmdBindDescriptorSet(pCmd, 0, pDescriptorSetRaytracing);
			cmdBindDescriptorSet(pCmd, mFrameIdx, pDescriptorSetUniforms);

			RaytracingDispatchDesc dispatchDesc = {};
			dispatchDesc.mHeight = mSettings.mHeight;
			dispatchDesc.mWidth = mSettings.mWidth;
			dispatchDesc.pShaderTable = pShaderTable;
#ifdef METAL
			dispatchDesc.pTopLevelAccelerationStructure = pSponzaAS;

            //dispatchDesc.pIndexes = { 0 };
            //dispatchDesc.pSets = { 0 };
            
            dispatchDesc.pIndexes[DESCRIPTOR_UPDATE_FREQ_NONE] = 0;
			dispatchDesc.pSets[DESCRIPTOR_UPDATE_FREQ_NONE] = pDescriptorSetRaytracing;
            dispatchDesc.pIndexes[DESCRIPTOR_UPDATE_FREQ_PER_FRAME] = mFrameIdx;
            dispatchDesc.pSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME] = pDescriptorSetUniforms;
#endif
			cmdDispatchRays(pCmd, pRaytracing, &dispatchDesc);
		}
		/************************************************************************/
		// Transition UAV to be used as source and swapchain as destination in copy operation
		/************************************************************************/
		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		TextureBarrier copyBarriers[] = {
			{ pComputeOutput, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
		};
		RenderTargetBarrier rtCopyBarriers[] = {
			{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(pCmd, 0, NULL, 1, copyBarriers, 1, rtCopyBarriers);
		
#if USE_DENOISER
		Texture* denoisedTexture = NULL;
		cmdSSVGFDenoise(pCmd, pDenoiser,
						pComputeOutput,
						pMotionVectorRenderTarget->pTexture,
						pDepthNormalRenderTarget[mPathTracingData.mFrameIndex & 0x1]->pTexture,
						pDepthNormalRenderTarget[(mPathTracingData.mFrameIndex + 1) & 0x1]->pTexture, &denoisedTexture);
		
		DescriptorData params[1] = {};
		params[0].pName = "uTex0";
		params[0].ppTextures = &denoisedTexture;
		updateDescriptorSet(pRenderer, mFrameIdx, pDescriptorSetTexture, 1, params);
		
		removeResource(denoisedTexture);
#endif
		
		cmdEndGpuTimestampQuery(pCmd, gGpuProfileToken);
		/************************************************************************/
		// Present to screen
		/************************************************************************/
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTarget->mClearValue;
		cmdBindRenderTargets(pCmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)mSettings.mWidth, (float)mSettings.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, mSettings.mWidth, mSettings.mHeight);
        
		if (pRaytracing != NULL)
		{
			/************************************************************************/
			// Perform copy
			/************************************************************************/
			cmdBeginGpuTimestampQuery(pCmd, gGpuProfileToken, "Render result");
			// Draw computed results
			cmdBindPipeline(pCmd, pDisplayTexturePipeline);
			cmdBindDescriptorSet(pCmd, mFrameIdx, pDescriptorSetTexture);
			cmdDraw(pCmd, 3, 0);
			cmdEndGpuTimestampQuery(pCmd, gGpuProfileToken);
        }

		cmdBeginDebugMarker(pCmd, 0, 1, 0, "Draw UI");
		
		FontDrawDesc frameTimeDraw;
		frameTimeDraw.mFontColor = 0xff0080ff;
		frameTimeDraw.mFontSize = 18.0f;
		frameTimeDraw.mFontID = gFontID;
        float2 txtSize = cmdDrawCpuProfile(pCmd, float2(8.0f, 15.0f), &frameTimeDraw);
        cmdDrawGpuProfile(pCmd, float2(8.f, txtSize.y + 75.f), gGpuProfileToken, &frameTimeDraw);
		
		cmdDrawUserInterface(pCmd);
		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		RenderTargetBarrier presentBarrier = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 1, &presentBarrier);

		cmdEndGpuFrameProfile(pCmd, gGpuProfileToken);
		
		endCmd(pCmd);
		waitForAllResourceLoads();

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.mSignalSemaphoreCount = 1;
		submitDesc.mWaitSemaphoreCount = 1;
		submitDesc.ppCmds = &pCmd;
		submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphores[mFrameIdx];
		submitDesc.ppWaitSemaphores = &pImageAcquiredSemaphore;
		submitDesc.pSignalFence = pRenderCompleteFences[mFrameIdx];
		queueSubmit(pQueue, &submitDesc);
		QueuePresentDesc presentDesc = {};
		presentDesc.mIndex = swapchainImageIndex;
		presentDesc.mWaitSemaphoreCount = 1;
		presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphores[mFrameIdx];
		presentDesc.pSwapChain = pSwapChain;
		presentDesc.mSubmitDone = true;
		queuePresent(pQueue, &presentDesc);
		flipProfiler();

		mFrameIdx = (mFrameIdx + 1) % gImageCount;
		/************************************************************************/
		/************************************************************************/
	}

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.mColorClearValue = { { 1, 1, 1, 1 } };
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true, true);
		swapChainDesc.mEnableVsync = false;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.ppPresentQueues = &pQueue;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.mWindowHandle = pWindow->handle;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	void addDescriptorSets()
	{
		if (!isRaytracingSupported(pRenderer)) //-V560
		{
			DescriptorSetDesc setDesc = { pDisplayTextureSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTexture);
		}
		else
		{
			DescriptorSetDesc setDesc = { pDisplayTextureSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTexture);
			setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetRaytracing);
			setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUniforms);
		}

#if USE_DENOISER
		DescriptorSetDesc setDesc = { pDenoiserInputsRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDenoiserInputsDescriptorSet);
#endif
	}

	void removeDescriptorSets()
	{
#if USE_DENOISER
		removeDescriptorSet(pRenderer, pDenoiserInputsDescriptorSet);
#endif
		if (pRaytracing != NULL)
		{
			removeDescriptorSet(pRenderer, pDescriptorSetRaytracing);
			removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
		}

		removeDescriptorSet(pRenderer, pDescriptorSetTexture);
	}

	void addRootSignatures()
	{
		const char*       pStaticSamplers[] = { "uSampler0" };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplerNames = pStaticSamplers;
		rootDesc.ppStaticSamplers = &pSampler;
		rootDesc.mShaderCount = 1;
		rootDesc.ppShaders = &pDisplayTextureShader;
		addRootSignature(pRenderer, &rootDesc, &pDisplayTextureSignature);

		if (isRaytracingSupported(pRenderer))
		{
			pStaticSamplers[0] = "linearSampler";

			Shader* pShaders[] = { pShaderRayGen, pShaderClosestHit, pShaderMiss, pShaderMissShadow };
			RootSignatureDesc signatureDesc = {};
			signatureDesc.ppShaders = pShaders;
			signatureDesc.mShaderCount = 4;
			signatureDesc.ppStaticSamplerNames = pStaticSamplers;
			signatureDesc.ppStaticSamplers = &pLinearSampler;
			signatureDesc.mStaticSamplerCount = 1;
			addRootSignature(pRenderer, &signatureDesc, &pRootSignature);
		}

#if USE_DENOISER
		RootSignatureDesc rootSignature = {};
		rootSignature.ppShaders = &pDenoiserInputsShader;
		rootSignature.mShaderCount = 1;
		addRootSignature(pRenderer, &rootSignature, &pDenoiserInputsRootSignature);
#endif
	}

	void removeRootSignatures()
	{
#if USE_DENOISER
		removeRootSignature(pRenderer, pDenoiserInputsRootSignature);
#endif
		if (pRaytracing != NULL)
		{
			removeRootSignature(pRenderer, pRootSignature);
		}

		removeRootSignature(pRenderer, pDisplayTextureSignature);
	}

	void addShaders()
	{
		/************************************************************************/
		// Blit texture
		/************************************************************************/
		const char* displayTextureVertShader[2] =
		{
			"DisplayTexture.vert",
			"DisplayTexture_USE_DENOISER.vert"
		};

		const char* displayTextureFragShader[2] =
		{
			"DisplayTexture.frag",
			"DisplayTexture_USE_DENOISER.frag"
		};

		ShaderLoadDesc displayShader = {};
		displayShader.mStages[0] = { displayTextureVertShader[USE_DENOISER] };
		displayShader.mStages[1] = { displayTextureFragShader[USE_DENOISER] };
		addShader(pRenderer, &displayShader, &pDisplayTextureShader);

		/************************************************************************/
		// Create Raytracing Shaders
		/************************************************************************/
		if (isRaytracingSupported(pRenderer))
		{

			ShaderMacro denoiserMacro = { "DENOISER_ENABLED", USE_DENOISER ? "1" : "0" };
			ShaderLoadDesc desc = {};
			desc.mStages[0] = { "RayGen.rgen", &denoiserMacro, 1, "rayGen" };
			desc.mTarget = shader_target_6_3;
			addShader(pRenderer, &desc, &pShaderRayGen);

			desc.mStages[0] = { "ClosestHit.rchit", &denoiserMacro, 1, "chs" };
			addShader(pRenderer, &desc, &pShaderClosestHit);

			desc.mStages[0] = { "Miss.rmiss", &denoiserMacro, 1, "miss" };
			addShader(pRenderer, &desc, &pShaderMiss);

			desc.mStages[0] = { "MissShadow.rmiss", &denoiserMacro, 1, "missShadow" };
			addShader(pRenderer, &desc, &pShaderMissShadow);
		}

#if USE_DENOISER
		ShaderLoadDesc denoiserShader = {};
		denoiserShader.mStages[0] = { "DenoiserInputsPass.vert", NULL, 0 };
		denoiserShader.mStages[1] = { "DenoiserInputsPass.frag", NULL, 0 };
		addShader(pRenderer, &denoiserShader, &pDenoiserInputsShader);
#endif
	}

	void removeShaders()
	{
#if USE_DENOISER
		removeShader(pRenderer, pDenoiserInputsShader);
#endif
		if (pRaytracing != NULL)
		{
			removeShader(pRenderer, pShaderRayGen);
			removeShader(pRenderer, pShaderClosestHit);
			removeShader(pRenderer, pShaderMiss);
			removeShader(pRenderer, pShaderMissShadow);
		}

		removeShader(pRenderer, pDisplayTextureShader);
	}

	void addPipelines()
	{
		/************************************************************************/
		//  Create Raytracing Pipelines
		/************************************************************************/
		if (isRaytracingSupported(pRenderer))
		{
			RaytracingHitGroup hitGroups[2] = {};
			hitGroups[0].pClosestHitShader = pShaderClosestHit;
			hitGroups[0].pHitGroupName = "hitGroup";

			hitGroups[1].pHitGroupName = "missHitGroup";

			Shader* pMissShaders[] = { pShaderMiss, pShaderMissShadow };
			PipelineDesc rtPipelineDesc = {};
			rtPipelineDesc.mType = PIPELINE_TYPE_RAYTRACING;
			RaytracingPipelineDesc& pipelineDesc = rtPipelineDesc.mRaytracingDesc;
			pipelineDesc.mAttributeSize = sizeof(float2);
			pipelineDesc.mMaxTraceRecursionDepth = 5;
#ifdef METAL
			pipelineDesc.mPayloadSize = sizeof(float4) * (5 + USE_DENOISER); // The denoiser additionally stores the albedo.
#else
			pipelineDesc.mPayloadSize = sizeof(float4);
#endif
			pipelineDesc.pGlobalRootSignature = pRootSignature;
			pipelineDesc.pRayGenShader = pShaderRayGen;
			pipelineDesc.pRayGenRootSignature = nullptr;// pRayGenSignature; //nullptr to bind empty LRS
			pipelineDesc.ppMissShaders = pMissShaders;
			pipelineDesc.mMissShaderCount = 2;
			pipelineDesc.pHitGroups = hitGroups;
			pipelineDesc.mHitGroupCount = 2;
			pipelineDesc.pRaytracing = pRaytracing;
			pipelineDesc.mMaxRaysCount = mSettings.mHeight * mSettings.mWidth;
			addPipeline(pRenderer, &rtPipelineDesc, &pPipeline);


			const char* hitGroupNames[2] = { "hitGroup", "missHitGroup" };
			const char* missShaderNames[2] = { "miss", "missShadow" };

			RaytracingShaderTableDesc shaderTableDesc = {};
			shaderTableDesc.pPipeline = pPipeline;
			shaderTableDesc.pRayGenShader = "rayGen";
			shaderTableDesc.mMissShaderCount = 2;
			shaderTableDesc.pMissShaders = missShaderNames;
			shaderTableDesc.mHitGroupCount = 2;
			shaderTableDesc.pHitGroups = hitGroupNames;
			addRaytracingShaderTable(pRaytracing, &shaderTableDesc, &pShaderTable);
		}

#if USE_DENOISER
		{
			RasterizerStateDesc rasterState = {};
			rasterState.mCullMode = CULL_MODE_BACK;
			rasterState.mFrontFace = FRONT_FACE_CW;

			DepthStateDesc depthStateDesc = {};
			depthStateDesc.mDepthTest = true;
			depthStateDesc.mDepthWrite = true;
			depthStateDesc.mDepthFunc = CMP_LEQUAL;

			PipelineDesc pipelineDesc = {};
			pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;

			TinyImageFormat rtFormats[] = { pDepthNormalRenderTarget[0]->mFormat, pMotionVectorRenderTarget->mFormat };

			VertexLayout vertexLayout = {};
			vertexLayout.mAttribCount = 2;
			vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
			vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
			vertexLayout.mAttribs[0].mBinding = 0;
			vertexLayout.mAttribs[0].mLocation = 0;
			vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
			vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
			vertexLayout.mAttribs[1].mBinding = 1;
			vertexLayout.mAttribs[1].mLocation = 1;

			GraphicsPipelineDesc& pipelineSettings = pipelineDesc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.pRasterizerState = &rasterState;
			pipelineSettings.mRenderTargetCount = 2;
			pipelineSettings.pColorFormats = rtFormats;
			pipelineSettings.mDepthStencilFormat = pDepthRenderTarget->mFormat;
			pipelineSettings.pDepthState = &depthStateDesc;
			pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
			pipelineSettings.mSampleQuality = 0;
			pipelineSettings.pVertexLayout = &vertexLayout;
			pipelineSettings.pRootSignature = pDenoiserInputsRootSignature;
			pipelineSettings.pShaderProgram = pDenoiserInputsShader;

			addPipeline(pRenderer, &pipelineDesc, &pDenoiserInputsPipeline);
		}
#endif

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 0;
		PipelineDesc graphicsPipelineDesc = {};
		graphicsPipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = graphicsPipelineDesc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRootSignature = pDisplayTextureSignature;
		pipelineSettings.pShaderProgram = pDisplayTextureShader;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pDisplayTexturePipeline);
	}

	void removePipelines()
	{
		removePipeline(pRenderer, pDisplayTexturePipeline);
#if USE_DENOISER
		removePipeline(pRenderer, pDenoiserInputsPipeline);
#endif

		if (pRaytracing != NULL)
		{
			removeRaytracingShaderTable(pRaytracing, pShaderTable);
			removePipeline(pRenderer, pPipeline);
		}
	}

	void prepareDescriptorSets()
	{
		DescriptorData params[9] = {};

		if (pRaytracing != NULL)
		{
			params[0].pName = "gOutput";
			params[0].ppTextures = &pComputeOutput;


			params[1].pName = "indices";
			params[1].ppBuffers = &SponzaProp.pGeom->pIndexBuffer;
			params[2].pName = "positions";
			params[2].ppBuffers = &SponzaProp.pGeom->pVertexBuffers[0];
			params[3].pName = "normals";
			params[3].ppBuffers = &SponzaProp.pGeom->pVertexBuffers[1];
			params[4].pName = "uvs";
			params[4].ppBuffers = &SponzaProp.pGeom->pVertexBuffers[2];
			params[5].pName = "materialIndices";
			params[5].ppBuffers = &SponzaProp.pMaterialIdStream;
			params[6].pName = "materialTextureIndices";
			params[6].ppBuffers = &SponzaProp.pMaterialTexturesStream;
			params[7].pName = "materialTextures";
			params[7].ppTextures = pMaterialTextures;
			params[7].mCount = TOTAL_IMGS;

#if USE_DENOISER
			params[8].pName = "gAlbedoTex";
			params[8].ppTextures = &pAlbedoTexture;
#endif
#ifndef METAL
			params[8 + USE_DENOISER].pName = "gRtScene";
			params[8 + USE_DENOISER].ppAccelerationStructures = &pSponzaAS;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetRaytracing, 9 + USE_DENOISER, params);
#else
			updateDescriptorSet(pRenderer, 0, pDescriptorSetRaytracing, 8 + USE_DENOISER, params);
#endif
		}

		params[0].pName = "uTex0";
		params[0].ppTextures = &pComputeOutput;
#if USE_DENOISER
		params[1].pName = "albedoTex";
		params[1].ppTextures = &pAlbedoTexture;
#endif
		for (uint32_t i = 0; i < gImageCount; i += 1)
			updateDescriptorSet(pRenderer, i, pDescriptorSetTexture, 1 + USE_DENOISER, params);

#if USE_DENOISER
		params[0].pName = "uniforms";
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			params[0].ppBuffers = &pDenoiserInputsUniformBuffer[i];
			updateDescriptorSet(pRenderer, i, pDenoiserInputsDescriptorSet, 1, params);
		}
#endif

		if (isRaytracingSupported(pRenderer)) //-V560
		{
			params[0].pName = "gSettings";
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].ppBuffers = &pRayGenConfigBuffer[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetUniforms, 1, params);
			}
		}
	}

	const char* GetName()
	{
		return "16_Raytracing";
	}

	/************************************************************************/
	// Data
	/************************************************************************/
private:
	static const uint32_t   gImageCount = 3;

    uint32_t                nFrameCount = 0;	
	Renderer*               pRenderer = NULL;
	Raytracing*	            pRaytracing = NULL;
	Queue*                  pQueue = NULL;
	CmdPool*                pCmdPools[gImageCount] = {};
	Cmd*                    pCmds[gImageCount] = {};
	Fence*                  pRenderCompleteFences[gImageCount] = {};
	Buffer*                 pRayGenConfigBuffer[gImageCount] = {};
	AccelerationStructure*  pSponzaAS = NULL;
	Shader*	                pShaderRayGen = NULL;
	Shader*	                pShaderClosestHit = NULL;
	Shader*	                pShaderMiss = NULL;
	Shader*	                pShaderMissShadow = NULL;
    Shader*                 pDisplayTextureShader = NULL;
    Sampler*                pSampler = NULL;
	Sampler*                pLinearSampler = NULL;
	RootSignature*          pRootSignature = NULL;
    RootSignature*          pDisplayTextureSignature = NULL;
	DescriptorSet*          pDescriptorSetRaytracing = NULL;
	DescriptorSet*          pDescriptorSetUniforms = NULL;
	DescriptorSet*          pDescriptorSetTexture = NULL;
	Pipeline*               pPipeline = NULL;
    Pipeline*               pDisplayTexturePipeline = NULL;
	RaytracingShaderTable*  pShaderTable = NULL;
	SwapChain*              pSwapChain = NULL;
	Texture*                pComputeOutput = NULL;
	Semaphore*              pRenderCompleteSemaphores[gImageCount] = {};
	Semaphore*              pImageAcquiredSemaphore = NULL;
	uint32_t                mFrameIdx = 0;
	PathTracingData         mPathTracingData = {};
	UIComponent*           pGuiWindow = NULL;
	float3                  mLightDirection = float3(0.2f, 0.8f, 0.1f);

#if USE_DENOISER
	Buffer*                 pDenoiserInputsUniformBuffer[gImageCount] = {};
	Texture*                pAlbedoTexture = NULL;
	DescriptorSet*          pDenoiserInputsDescriptorSet = NULL;
	RenderTarget*           pDepthNormalRenderTarget[2] = {};
	RenderTarget*           pMotionVectorRenderTarget = NULL;
	RenderTarget*           pDepthRenderTarget = NULL;
	RootSignature*          pDenoiserInputsRootSignature = NULL;
	Shader*                 pDenoiserInputsShader = NULL;
	Pipeline*               pDenoiserInputsPipeline = NULL;
	SSVGFDenoiser*          pDenoiser = NULL;
#endif
};

DEFINE_APPLICATION_MAIN(UnitTest_NativeRaytracing)

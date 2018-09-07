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

// Unit Test to create Bottom and Top Level Acceleration Structures using Raytracing API.

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

// Raytracing
#include "../../../../CommonRaytracing_3/Interfaces/IRaytracing.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

//Example for using roots or will cause linker error with the extern root in FileSystem.cpp
const char* pszRoots[FSR_Count] =
{
};

class UnitTest_AccelerationStructure : public IApp
{
public:
	bool Init()
	{
		/************************************************************************/
		// 01 Init Raytracing
		/************************************************************************/
		RendererDesc desc = {};
		initRenderer(GetName(), &desc, &pRenderer);
		initResourceLoaderInterface(pRenderer);
		initRaytracing(pRenderer, &pRaytracing);

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		addQueue(pRenderer, &queueDesc, &pQueue);
		addCmdPool(pRenderer, pQueue, false, &pCmdPool);
		addCmd(pCmdPool, false, &pCmd);
		addFence(pRenderer, &pFence);
		/************************************************************************/
		// 02 Creation Acceleration Structure
		/************************************************************************/
		// Create Vertex Buffer
		const float3 vertices[] =
		{
			float3(-100, -1,  -2),
			float3(100, -1,  100),
			float3(-100, -1,  100),

			float3(-100, -1,  -2),
			float3(100, -1,  -2),
			float3(100, -1,  100),
		};
		BufferLoadDesc vbDesc = {};
		vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vbDesc.mDesc.mSize = sizeof(vertices);
		vbDesc.mDesc.mVertexStride = sizeof(float3);
		vbDesc.pData = vertices;
		vbDesc.ppBuffer = &pVertexBuffer;
		addResource(&vbDesc);

		// Specify Geometry Used in Raytracing Structure
		AccelerationStructureGeometryDesc geomDesc = {};
		geomDesc.mFlags = ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE;
		geomDesc.mType = ACCELERATION_STRUCTURE_GEOMETRY_TYPE_TRIANGLES;
		geomDesc.pVertexBuffer = pVertexBuffer;

		uint32_t bottomASScratchBufferSize = 0;
		AccelerationStructureDesc bottomASDesc = {};
		bottomASDesc.mDescCount = 1;
		bottomASDesc.mFlags = ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		bottomASDesc.mType = ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bottomASDesc.pGeometryDescs = &geomDesc;
		addAccelerationStructure(pRaytracing, &bottomASDesc, &bottomASScratchBufferSize, &pBottomLevelAS);

		// Specify Instance Used in Raytracing Structure
		mat4 identity = mat4::identity();
		AccelerationStructureInstanceDesc instanceDesc = {};
		instanceDesc.mFlags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_NONE;
		instanceDesc.mInstanceContributionToHitGroupIndex = 0;
		instanceDesc.mInstanceID = 0;
		instanceDesc.mInstanceMask = 1;
		memcpy(instanceDesc.mTransform, &identity, sizeof(float[12]));
		instanceDesc.pAccelerationStructure = pBottomLevelAS;

		uint32_t topASScratchBufferSize = 0;
		AccelerationStructureDesc topASDesc = {};
		topASDesc.mDescCount = 1;
		topASDesc.mFlags = ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		topASDesc.mType = ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		topASDesc.pInstanceDescs = &instanceDesc;
		addAccelerationStructure(pRaytracing, &topASDesc, &topASScratchBufferSize, &pTopLevelAS);

		Buffer* pScratchBuffer = NULL;
		BufferLoadDesc scratchBufferDesc = {};
		scratchBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
		scratchBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		scratchBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
		scratchBufferDesc.mDesc.mSize = max(bottomASScratchBufferSize, topASScratchBufferSize);
		scratchBufferDesc.ppBuffer = &pScratchBuffer;
		addResource(&scratchBufferDesc);

		// Build Acceleration Structure
		beginCmd(pCmd);
		cmdBuildAccelerationStructure(pCmd, pRaytracing, pScratchBuffer, pBottomLevelAS);
		cmdBuildAccelerationStructure(pCmd, pRaytracing, pScratchBuffer, pTopLevelAS);
		endCmd(pCmd);
		queueSubmit(pQueue, 1, &pCmd, pFence, 0, NULL, 0, NULL);
		waitForFences(pQueue, 1, &pFence, false);

		// Safe to remove scratch buffer since the GPU is done using it
		removeResource(pScratchBuffer);
		/************************************************************************/
		/************************************************************************/
		return true;
	}

	void Exit()
	{
		removeResource(pVertexBuffer);
		removeAccelerationStructure(pRaytracing, pTopLevelAS);
		removeAccelerationStructure(pRaytracing, pBottomLevelAS);
		removeFence(pRenderer, pFence);
		removeCmd(pCmdPool, pCmd);
		removeCmdPool(pRenderer, pCmdPool);
		removeQueue(pQueue);
		removeRaytracing(pRenderer, pRaytracing);
		removeResourceLoaderInterface(pRenderer);
		removeRenderer(pRenderer);
	}

	bool Load()
	{
		return true;
	}

	void Unload()
	{
	}

	void Update(float deltaTime)
	{
	}

	void Draw()
	{
	}

	tinystl::string GetName()
	{
		return "Create Raytracing Acceleration Structure";
	}
	/************************************************************************/
	// Data
	/************************************************************************/
private:
	Renderer * pRenderer;
	Raytracing*				pRaytracing;
	Queue*					pQueue;
	CmdPool*				pCmdPool;
	Cmd*					pCmd;
	Fence*					pFence;
	Buffer*					pVertexBuffer;
	AccelerationStructure*	pBottomLevelAS;
	AccelerationStructure*	pTopLevelAS;
};

DEFINE_APPLICATION_MAIN(UnitTest_AccelerationStructure)
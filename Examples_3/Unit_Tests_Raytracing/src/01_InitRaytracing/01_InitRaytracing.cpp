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

// Unit Test to initialize Raytracing API.

//tiny stl
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../Common_3/OS/Interfaces/IApp.h"
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../Common_3/OS/Interfaces/IUIManager.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/ResourceLoader.h"

// Raytracing
#include "../../CommonRaytracing_3/Interfaces/IRaytracing.h"

//Math
#include "../../Common_3/OS/Math/MathTypes.h"

#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

//Example for using roots or will cause linker error with the extern root in FileSystem.cpp
const char* pszRoots[FSR_Count] =
{
};

class UnitTest_InitRaytracing : public IApp
{
public:
	bool Init()
	{
		RendererDesc desc = {};
		initRenderer(GetName(), &desc, &pRenderer);
		initRaytracing(pRenderer, &pRaytracing);

		return true;
	}

	void Exit()
	{
		removeRaytracing(pRenderer, pRaytracing);
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

	String GetName()
	{
		return "Init Raytracing";
	}
	/************************************************************************/
	// Data
	/************************************************************************/
private:
	Renderer*	pRenderer;
	Raytracing*	pRaytracing;
	Shader*		pRaytracingShader;
};

DEFINE_APPLICATION_MAIN(UnitTest_InitRaytracing)
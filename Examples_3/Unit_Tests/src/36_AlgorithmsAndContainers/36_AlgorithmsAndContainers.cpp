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

// NOTE: This unit test requires AUTOMATED_TESTING macro at the top of Config.h

#include <stdlib.h>

//Interfaces
#include "AlgorithmsTest.h"
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"

//Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#ifdef AUTOMATED_TESTING
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/bstrlib/bstest.h"
#endif

#define STBDS_UNIT_TESTS
#include "../../../../Common_3/Utilities/Math/BStringHashMap.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"


static const char appName[] = "36_BStringTest";

#ifdef AUTOMATED_TESTING
// This variable disables actual assertions for testing purposes
// Should be initialized to true for bstrlib tests
extern "C" bool gIsBstrlibTest;
#endif

static Renderer* pRenderer = NULL;
static uint32_t gFontID = 0;

class Transformations : public IApp
{
public:

	bool Init()
	{
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");

		// Required for proper UI update(called once)
		// window and renderer setup
		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		settings.mD3D11Supported = true;
		settings.mGLESSupported = true;
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		initResourceLoaderInterface(pRenderer);

		
		// Load fonts
		FontDesc font = {};
		font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
		fntDefineFonts(&font, 1, &gFontID);

		FontSystemDesc fontRenderDesc = {};
		fontRenderDesc.pRenderer = pRenderer;
		if (!initFontSystem(&fontRenderDesc))
			return false;

		InputSystemDesc inputDesc{};
		inputDesc.pRenderer = pRenderer;
		inputDesc.pWindow = pWindow;
		if (!initInputSystem(&inputDesc))
			return false;

		// Initialize Forge User Interface Rendering
		UserInterfaceDesc uiRenderDesc = {};
		uiRenderDesc.pRenderer = pRenderer;
		initUserInterface(&uiRenderDesc);


		//////////////////////////////////////////////
		// Actual unit tests
		//////////////////////////////////////////////

		int ret = testStableSort();
		if (ret == 0)
			LOGF(eINFO, "Stable sort test success");
		else
		{
			LOGF(eERROR, "Stable sort test failed.");
			ASSERT(false);
			return false;
		}

#ifdef AUTOMATED_TESTING
		gIsBstrlibTest = true;
		ret = runBstringTests();
		gIsBstrlibTest = false;
		if (ret == 0)
			LOGF(eINFO, "Bstring test success.");
		else
		{
			LOGF(eERROR, "Bstring test failed.");
			ASSERT(false);
			return false;
		}
#else
		LOGF(eERROR, "Bstring tests can't run without AUTOMATED_TESTING macro");
#endif	

		stbds_unit_tests();
		LOGF(eINFO, "stbds test success.");
		stbds_bstring_unit_tests();
		LOGF(eINFO, "stbds bstring extension test success.");
		requestShutdown();
		return true;
	}

	void Exit()
	{
		exitUserInterface();
		exitInputSystem();
		exitFontSystem();
		exitResourceLoaderInterface(pRenderer);
		exitRenderer(pRenderer);
	}

	bool Load(ReloadDesc* pReloadDesc)
	{
		return true;
	}

	void Unload(ReloadDesc* pReloadDesc)
	{}

	void Update(float deltaTime)
	{}

	void Draw()
	{}

	const char* GetName() { return appName; }
};

DEFINE_APPLICATION_MAIN(Transformations)

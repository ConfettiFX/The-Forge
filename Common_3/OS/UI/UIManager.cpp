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

#include "../Interfaces/IUIManager.h"

#include "Fontstash.h"
#include "UI.h"
#include "NuklearGUIDriver.h"
#include "UIRenderer.h"

#include "../Image/Image.h"
#include "../../Renderer/IRenderer.h"
#include "../../Renderer/GpuProfiler.h"
#include "../../Renderer/ResourceLoader.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IFileSystem.h"

#include "../../ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../../ThirdParty/OpenSource/TinySTL/vector.h"
#include "../Interfaces/IMemoryManager.h"

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

#define MAKEQUAD(x0, y0, x1, y1, o)\
	vec2(x0 + o, y0 + o),\
	vec2(x0 + o, y1 - o),\
	vec2(x1 - o, y0 + o),\
	vec2(x1 - o, y1 - o),

#define MAKETEXQUAD(x0, y0, x1, y1, o)\
	TexVertex(float2(x0 + o, y0 + o), float2(0, 0)),\
	TexVertex(float2(x0 + o, y1 - o), float2(0, 1)),\
	TexVertex(float2(x1 - o, y0 + o), float2(1, 0)),\
	TexVertex(float2(x1 - o, y1 - o), float2(1, 1)),

/************************************************************************/
// UIManager Interface Implementation
/************************************************************************/
void addUIManagerInterface(Renderer* pRenderer, const UISettings* pUISettings, UIManager** ppUIManager)
{
	UIManager* pUIManager = (UIManager*)conf_calloc(1, sizeof(*pUIManager));
	memcpy(&pUIManager->mSettings, pUISettings, sizeof(*pUISettings));

	pUIManager->pUIRenderer = conf_placement_new<UIRenderer>(conf_calloc(1, sizeof(UIRenderer)), pRenderer);

	if (pUISettings->pDefaultFontName)
	{
		// #fontstash
		// the following classes require the default font in the fontstash to have it's identifier defined as "default"
		// as they query the fontstash using this identifier to get the default font ID.
		//  - UIAppComponentTextOnly
		//  - NuklearGUIDriver
		pUIManager->mDefaultFontstashID = pUIManager->pUIRenderer->addFontstash(512, 512);
		Fontstash* pFont = pUIManager->pUIRenderer->getFontstash(pUIManager->mDefaultFontstashID);
		pFont->defineFont("default", pUISettings->pDefaultFontName, FSR_Builtin_Fonts);
	}

	*ppUIManager = pUIManager;
}

void removeUIManagerInterface(Renderer* pRenderer, UIManager* pUIManager)
{
  UNREF_PARAM(pRenderer);
	pUIManager->pUIRenderer->~UIRenderer();
	conf_free(pUIManager->pUIRenderer);

	conf_free(pUIManager);
}

void addGui(UIManager* pUIManager, const GuiDesc* pDesc, Gui** ppGui)
{
	ASSERT(pUIManager);
	ASSERT(pDesc);
	ASSERT(ppGui);

	Gui* pGui = (Gui*)conf_calloc(1, sizeof(*pGui));
	pGui->mDesc = *pDesc;

	pGui->pUI = conf_placement_new<UI>(conf_calloc(1, sizeof(UI)));
	pGui->pGui = conf_placement_new<UIAppComponentGui>(conf_calloc(1, sizeof(UIAppComponentGui)), &pDesc->mDefaultTextDrawDesc);
	pGui->pGui->renderer = pUIManager->pUIRenderer;
	pGui->pGui->ui = pGui->pUI;
	pGui->pGui->font = pUIManager->mDefaultFontstashID;
	pGui->pGui->load((int)pDesc->mStartPosition.getX(), (int)pDesc->mStartPosition.getY(),
		(int)pDesc->mStartSize.getX(), (int)pDesc->mStartSize.getY());

	*ppGui = pGui;
}

void removeGui(UIManager* pUIManager, Gui* pGui)
{
	ASSERT(pUIManager);
	ASSERT(pGui);
	pGui->pGui->unload();
	pGui->pGui->~UIAppComponentGui();
	conf_free(pGui->pGui);
	pGui->pUI->~UI();
	conf_free(pGui->pUI);
	conf_free(pGui);
}

void addProperty(Gui* pUIManager, const UIProperty* pProperty, uint32_t* pId)
{
	if (pUIManager->pUI)
	{
		if (pId)
			*pId = pUIManager->pUI->addProperty(*pProperty);
		else
			pUIManager->pUI->addProperty(*pProperty);
	}
}

void addProperty(Gui* pUIManager, const UIProperty pProperty, uint32_t* pId)
{
	if (pUIManager->pUI)
	{
		if (pId)
			*pId = pUIManager->pUI->addProperty(pProperty);
		else
			pUIManager->pUI->addProperty(pProperty);
	}
}

void removeProperty(Gui* pUIManager, uint32_t id)
{
	if (pUIManager->pUI)
	{
		pUIManager->pUI->removeProperty(id);
	}
}


void updateGui(UIManager* pUIManager, Gui* pGui, float deltaTime)
{
  UNREF_PARAM(pUIManager);
	pGui->pGui->update(deltaTime);
}

void cmdUIBeginRender(Cmd* pCmd, UIManager* pUIManager, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil)
{
	pUIManager->pUIRenderer->beginRender(pCmd, renderTargetCount, ppRenderTargets, pDepthStencil);
}

void cmdUIDrawFrameTime(struct Cmd* pCmd, UIManager* pUIManager, const vec2& position, const char* pPrefix, float ms, const TextDrawDesc* pTextDrawDesc /* = NULL */)
{
	char buffer[128];
#ifdef _WIN32
	sprintf_s(buffer, "%s%f ms", pPrefix, ms);
#else
	snprintf(buffer, sizeof(buffer), "%f ms", ms);
#endif
	cmdUIDrawText(pCmd, pUIManager, position, buffer, pTextDrawDesc ? pTextDrawDesc : &pUIManager->mSettings.mDefaultFrameTimeTextDrawDesc);
}

void cmdUIDrawText(struct Cmd* pCmd, UIManager* pUIManager, const vec2& position, const char* pText, const TextDrawDesc* pTextDrawDesc /*= NULL*/)
{
  UNREF_PARAM(pCmd);
	const TextDrawDesc* drawDesc = pTextDrawDesc ? pTextDrawDesc : &pUIManager->mSettings.mDefaultTextDrawDesc;
	//Fontstash* pFont = pUIManager->pUIRenderer->getFontstash(drawDesc->mFontID);
	Fontstash* pFont = pUIManager->pUIRenderer->getFontstash(0);
	ASSERT(pFont);

	pFont->drawText(pText, position.getX(), position.getY(), drawDesc->mFontID, drawDesc->mFontColor, drawDesc->mFontSize, drawDesc->mFontSpacing, drawDesc->mFontBlur);
	
}

void cmdUIDrawTexturedQuad(struct Cmd* pCmd, UIManager* pUIManager, const vec2& position, const vec2& size, Texture* pTexture)
{
  UNREF_PARAM(pCmd);
	// the last variable can be used to create a border
	TexVertex pVertices[] = { MAKETEXQUAD(position.getX(), position.getY(), position.getX() + size.getX(), position.getY() + size.getY(), 0) };
	int nVertices = ARRAY_COUNT(pVertices);
	float4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pUIManager->pUIRenderer->drawTextured(PRIMITIVE_TOPO_TRI_STRIP, pVertices, nVertices, pTexture, &color);
}

void cmdUIDrawGUI(struct Cmd* pCmd, UIManager* pUIManager, Gui* pGui)
{
  UNREF_PARAM(pCmd);
  UNREF_PARAM(pUIManager);
	pGui->pGui->onDrawGUI();
}

void cmdUIDrawGpuProfileData(Cmd* pCmd, struct UIManager* pUIManager, vec2& startPos, const GpuProfileDrawDesc* pDrawDesc, struct GpuProfiler* pGpuProfiler, GpuTimerTree* pRoot)
{
#if defined(DIRECT3D12) || defined(VULKAN)
	if (!pRoot)
		return;

	float originalX = startPos.getX();

	if (pRoot->mGpuTimer.mIndex > 0 && pRoot != &pGpuProfiler->mRoot)
	{
		char buffer[128];
		double time = getAverageGpuTime(pGpuProfiler, &pRoot->mGpuTimer);
		sprintf(buffer, "%s -  %f ms", pRoot->mGpuTimer.mName.c_str(), time * 1000.0);

		cmdUIDrawText(pCmd, pUIManager, startPos, buffer, &pDrawDesc->mDefaultGpuTextDrawDesc);
		startPos.setY(startPos.getY() + pDrawDesc->mHeightOffset);

		if (pRoot->mChildren.getCount())
			startPos.setX(startPos.getX() + pDrawDesc->mChildIndent);
	}

	for (uint32_t i = 0; i < pRoot->mChildren.getCount(); ++i)
	{
		cmdUIDrawGpuProfileData(pCmd, pUIManager, startPos, pDrawDesc, pGpuProfiler, pRoot->mChildren[i]);
	}

	startPos.setX(originalX);
#endif
}

void cmdUIDrawGpuProfileData(Cmd* pCmd, struct UIManager* pUIManager, const vec2& startPos, struct GpuProfiler* pGpuProfiler, const GpuProfileDrawDesc* pDrawDesc)
{
#if defined(DIRECT3D12) || defined(VULKAN)
	vec2 pos = startPos;
	cmdUIDrawText(pCmd, pUIManager, startPos, "-----GPU Times-----");
	pos.setY(pos.getY() + (pDrawDesc ? pDrawDesc->mHeightOffset : pUIManager->mSettings.mDefaultGpuProfileDrawDesc.mHeightOffset));

	cmdUIDrawGpuProfileData(pCmd, pUIManager, pos, pDrawDesc ? pDrawDesc : &pUIManager->mSettings.mDefaultGpuProfileDrawDesc, pGpuProfiler, &pGpuProfiler->mRoot);
#endif
}

void cmdUIEndRender(Cmd* pCmd, UIManager* pUIManager)
{
  UNREF_PARAM(pCmd);
	pUIManager->pUIRenderer->reset();
}

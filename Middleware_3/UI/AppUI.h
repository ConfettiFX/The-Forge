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

#pragma once

#include "../../Common_3/OS/Interfaces/IOperatingSystem.h"
#include "../../Common_3/OS/Interfaces/IMiddleware.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../Text/Fontstash.h"
#include "UIControl.h"

#define IS_BETWEEN(x, a, b) ((a) <= (x) && (x) < (b))
#define IS_INBOX(px, py, x, y, w, h)\
    (IS_BETWEEN(px,x,x+w) && IS_BETWEEN(py,y,y+h))


struct Renderer;
struct Texture;
struct Shader;
struct RootSignature;
struct Pipeline;
struct Sampler;
struct RasterizerState;
struct DepthState;
struct BlendState;
struct MeshRingBuffer;

enum UIMaxFontSize
{
	UI_MAX_FONT_SIZE_UNDEFINED = 0, // Undefined size, will defaults to use UI_MAX_FONT_SIZE_512
	UI_MAX_FONT_SIZE_128 = 128,     // Max font size is 12.8f
	UI_MAX_FONT_SIZE_256 = 256,     // Max font size is 25.6f
	UI_MAX_FONT_SIZE_512 = 512,     // Max font size is 51.2f
	UI_MAX_FONT_SIZE_1024 = 1024    // Max font size is 102.4f
};

typedef struct GuiDesc
{
	GuiDesc(
		  const vec2& startPos = { 0.0f, 150.0f }
		, const vec2& startSize = { 600.0f, 550.0f }
		, const TextDrawDesc& textDrawDesc = { 0, 0xffffffff, 16 }
	) :
		mStartPosition(startPos),
		mStartSize(startSize),
		mDefaultTextDrawDesc(textDrawDesc)
	{}

	vec2			mStartPosition;
	vec2			mStartSize;
	TextDrawDesc	mDefaultTextDrawDesc;
} GuiDesc;

class GuiComponent
{
public:
	uint	AddControl(const UIProperty& control);
	void	RemoveControl(unsigned int controlID);

	struct GuiComponentImpl* pImpl;
};
/************************************************************************/
// Helper Class for removing and adding properties easily
/************************************************************************/
typedef struct DynamicUIControls
{
	tinystl::vector<UIProperty> mDynamicProperties;
	tinystl::vector<uint>   mDynamicPropHandles;

	void ShowDynamicProperties(GuiComponent* pGui)
	{
		for (size_t i = 0; i < mDynamicProperties.size(); ++i)
		{
			mDynamicPropHandles.push_back(0);
			mDynamicPropHandles[i] = pGui->AddControl(mDynamicProperties[i]);
		}
	}

	void HideDynamicProperties(GuiComponent* pGui)
	{
		for (size_t i = 0; i < mDynamicProperties.size(); i++)
		{
			pGui->RemoveControl(mDynamicPropHandles[i]);
		}
		mDynamicPropHandles.clear();
	}

} DynamicUIControls;
/************************************************************************/
// Abstract interface for handling GUI
/************************************************************************/
class GUIDriver
{
public:
	virtual bool init(Renderer* pRenderer) = 0;
	virtual void exit() = 0;

	virtual bool load(class Fontstash* fontID, float fontSize, struct Texture* cursorTexture = 0, float uiwidth = 600, float uiheight = 400) = 0;
	virtual void unload() = 0;

	virtual void* getContext() = 0;

	virtual void draw(Cmd* q, float deltaTime, const char* pTitle, float x, float y, float z, float w, class UIProperty* pControl, uint numControls) = 0;

	virtual bool onInput(const struct ButtonData* data) = 0;
};
/************************************************************************/
// UI interface for App
/************************************************************************/
class UIApp : public IMiddleware
{
public:
	bool			Init(Renderer* renderer);
	void			Exit();

	bool			Load(RenderTarget** rts);
	void			Unload();

	void			Update(float deltaTime);
	void			Draw(Cmd* cmd);

	uint			LoadFont(const char* pFontPath, uint root);
	GuiComponent*	AddGuiComponent(const char* pTitle, const GuiDesc* pDesc);
	void			RemoveGuiComponent(GuiComponent* pComponent);

	void			Gui(GuiComponent* pGui);
	
	/************************************************************************/
	// Data
	/************************************************************************/
	struct UIAppImpl*	pImpl;
};

class VirtualJoystickUI
{
public:
	bool Init(Renderer* pRenderer, const char* pJoystickTexture, uint root);
	void Exit();

	bool Load(RenderTarget* pScreenRT,uint depthFormat = 0);
	void Unload();

	void Draw(Cmd* pCmd, class ICameraController* pController, const float4& color);

private:
	Renderer*			pRenderer;
	Shader*				pShader;
	RootSignature*		pRootSignature;
	Pipeline*			pPipeline;
	Texture*			pTexture;
	Sampler*			pSampler;
	BlendState*			pBlendAlpha;
	DepthState*			pDepthState;
	RasterizerState*	pRasterizerState;
	MeshRingBuffer*		pMeshRingBuffer;
};

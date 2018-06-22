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

#include "AppUI.h"

#include "Fontstash.h"
#include "NuklearGUIDriver.h"
#include "UIRenderer.h"

#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Image/Image.h"

#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/GpuProfiler.h"
#include "../../Common_3/Renderer/ResourceLoader.h"

#include "../../Common_3/ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

namespace PlatformEvents
{
	extern bool skipMouseCapture;
}

static tinystl::vector<class UIAppComponentGui*> gInstances;
static Mutex gMutex;

extern void initGUIDriver(GUIDriver** ppDriver);
extern void removeGUIDriver(GUIDriver* pDriver);

static bool uiKeyboardChar(const KeyboardCharEventData* pData);
static bool uiKeyboardButton(const KeyboardButtonEventData* pData);
static bool uiMouseMove(const MouseMoveEventData* pData);
static bool uiMouseButton(const MouseButtonEventData* pData);
static bool uiMouseWheel(const MouseWheelEventData* pData);
static bool uiJoystickButton(const JoystickButtonEventData* pData);
static bool uiTouch(const TouchEventData* pData);
static bool uiTouchMove(const TouchEventData* pData);
/************************************************************************/
// UI Property Definition
/************************************************************************/
// UI singular property
UIProperty::UIProperty(const char* description, float& value, float min/*=0.0f*/, float max/*=1.0f*/,
	float increment/*=0.1f*/, bool expScale/*=false*/, uint32_t color /*=0xAFAFAFFF*/,
	const char* tree /*=none*/) :
	description(description),
	type(UI_PROPERTY_FLOAT),
	flags(FLAG_VISIBLE),
	source(&value),
	color(color),
	tree(tree)
{
	settings.fMin = min;
	settings.fMax = max;
	settings.fIncrement = increment;
	settings.fExpScale = expScale;
}

UIProperty::UIProperty(const char* description, int steps, float& value,
	float min/*=0.0f*/, float max/*=1.0f*/, uint32_t color /*=0xAFAFAFFF*/,
	const char* tree /*=none*/) :
	description(description),
	type(UI_PROPERTY_FLOAT),
	flags(FLAG_VISIBLE),
	source(&value),
	color(color),
	tree(tree)
{
	settings.fMin = min;
	settings.fMax = max;
	settings.fIncrement = (max - min) / float(steps);
	settings.fExpScale = false;
}

UIProperty::UIProperty(const char* description, int& value, int min/*=-100*/, int max/*=100*/,
	int increment/*=1*/, uint32_t color /*=0xAFAFAFFF*/,
	const char* tree /*=none*/) :
	description(description),
	type(UI_PROPERTY_INT),
	flags(FLAG_VISIBLE),
	source(&value),
	color(color),
	tree(tree)

{
	settings.iMin = min;
	settings.iMax = max;
	settings.iIncrement = increment;
}

UIProperty::UIProperty(const char* description, unsigned int& value,
	unsigned int min/*=0*/, unsigned int max/*=100*/,
	unsigned int increment/*=1*/, uint32_t color /*=0xAFAFAFFF*/,
	const char* tree /*=none*/) :
	description(description),
	type(UI_PROPERTY_UINT),
	flags(FLAG_VISIBLE),
	source(&value),
	color(color),
	tree(tree)
{
	settings.uiMin = min;
	settings.uiMax = max;
	settings.uiIncrement = increment;
}

UIProperty::UIProperty(const char* description, bool& value, uint32_t color /*=0xAFAFAFFF*/,
	const char* tree /*=none*/) :
	description(description),
	type(UI_PROPERTY_BOOL),
	flags(FLAG_VISIBLE),
	source(&value),
	color(color),
	tree(tree)
{
}

UIProperty::UIProperty(const char* description, UIButtonFn fn, void* userdata, uint32_t color /*=0xAFAFAFFF*/,
	const char* tree /*=none*/) :
	description(description),
	type(UI_PROPERTY_BUTTON),
	flags(FLAG_VISIBLE),
	source(*(void**)&fn),
	color(color),
	tree(tree)
{
	settings.pUserData = userdata;
}

UIProperty::UIProperty(const char* description, char* value, unsigned int length, uint32_t color /*=0xAFAFAFFF*/,
	const char* tree /*=none*/) :
	description(description),
	type(UI_PROPERTY_TEXTINPUT),
	flags(FLAG_VISIBLE),
	source(value),
	color(color)
{
	settings.sLen = length;
}

UIProperty::UIProperty(const char* description, uint32_t color /*=0xAFAFAFFF*/,
	const char* tree /*=none*/) :
	description(description),
	type(UI_PROPERTY_TEXT),
	flags(FLAG_VISIBLE),
	color(color),
	tree(tree)
{
}


void UIProperty::setSettings(int steps, float min, float max)
{
	ASSERT(type == UI_PROPERTY_FLOAT);
	settings.fMin = min;
	settings.fMax = max;
	settings.fIncrement = (max - min) / float(steps);
}

void UIProperty::modify(int steps)
{
	switch (type)
	{
	case UI_PROPERTY_FLOAT:
	{
		float& vCurrent = *(float*)source;
		if (settings.fExpScale == false)
			vCurrent += settings.fIncrement * (float)steps; // linear scale
		else
			vCurrent = vCurrent + (vCurrent * settings.fIncrement * (float)steps); // exponential scale

		if (vCurrent > settings.fMax)
			vCurrent = settings.fMax;
		if (vCurrent < settings.fMin)
			vCurrent = settings.fMin;
		break;
	}
	case UI_PROPERTY_INT:
	{
		int& vCurrent = *(int*)source;
		vCurrent += settings.iIncrement * (int)steps;
		if (vCurrent > settings.iMax)
			vCurrent = settings.iMax;
		if (vCurrent < settings.iMin)
			vCurrent = settings.iMin;
		break;
	}
	case UI_PROPERTY_UINT:
	{
		unsigned int& vCurrent = *(unsigned int*)source;
		vCurrent += settings.uiIncrement * (int)steps;
		if (vCurrent > settings.uiMax)
			vCurrent = settings.uiMax;
		if (vCurrent < settings.uiMin)
			vCurrent = settings.uiMin;
		break;
	}
	case UI_PROPERTY_BOOL:
	{
		bool& vCurrent = *(bool*)source;
		if (steps > 0)
			vCurrent = true;
		else
			vCurrent = false;
		break;
	}
	case UI_PROPERTY_ENUM:
	{
		ASSERT(settings.eByteSize == 4); // does not support other enums than those that are 4 bytes in size (yet)
		int& vCurrent = *(int*)source;
		int i = enumComputeIndex();
		const int* source_ = ((const int*)settings.eValues);
		if (steps == 1 && settings.eNames[i + 1] != 0)
			vCurrent = source_[i + 1];
		if (steps == -1 && i != 0)
			vCurrent = source_[i - 1];
	}break;
	case UI_PROPERTY_BUTTON:
	{

	}break;
	case UI_PROPERTY_TEXTINPUT:
	{

	}break;
	case UI_PROPERTY_TEXT:
	{
		
	}break;
	}
}

int UIProperty::enumComputeIndex() const
{
	ASSERT(type == UI_PROPERTY_ENUM);
	ASSERT(settings.eByteSize == 4);

	int& vCurrent = *(int*)source;
	const int* source_ = ((const int*)settings.eValues);

	for (int i = 0; settings.eNames[i] != 0; i++)
	{
		if (source_[i] == vCurrent)
		{
			return i;
		}
	}

	return -1;
}
/************************************************************************/
// UI Implementation
/************************************************************************/
#define IS_BETWEEN(x, a, b) ((a) <= (x) && (x) < (b))
#define IS_INBOX(px, py, x, y, w, h)\
    (IS_BETWEEN(px,x,x+w) && IS_BETWEEN(py,y,y+h))

class UI
{
public:
	unsigned int addProperty(const UIProperty& prop)
	{
		// Try first to fill empty property slot
		for (unsigned int i = 0; i < (uint32_t)properties.size(); i++)
		{
			UIProperty& prop_slot = properties[i];
			if (prop_slot.source != NULL)
				continue;

			prop_slot = prop;
			return i;
		}

		properties.emplace_back(prop);
		return (uint32_t)properties.size() - 1;
	}

	unsigned int getPropertyCount()
	{
		return (uint32_t)properties.size();
	}

	UIProperty& getProperty(unsigned int idx)
	{
		return properties[idx];
	}

	void changedProperty(unsigned int idx)
	{
		if (properties[idx].callback)
			properties[idx].callback(&properties[idx]);
	}

	void clearProperties()
	{
		properties.clear();
	}

	void removeProperty(unsigned int idx)
	{
		UIProperty& prop = properties[idx];
		prop.source = NULL;
		prop.callback = NULL;
	}

	void setPropertyFlag(unsigned int propertyId, UIProperty::FLAG flag, bool state)
	{
		ASSERT(propertyId < getPropertyCount());
		unsigned int& flags = getProperty(propertyId).flags;
		flags = state ? (flags | flag) : (flags & ~flag);
	}

protected:
	PropertyChangedCallback onPropertyChanged;
	tinystl::vector<UIProperty> properties;
};

class UIAppComponentBase
{
public:
	UIAppComponentBase() : ui(0), renderer(0), font(-1), fontSize(12.0f), cursorTexture(NULL) {}
	UI* ui;
	class UIRenderer* renderer;
	int font;
	float fontSize;
	struct Texture* cursorTexture;

	virtual void load() {}
	virtual void unload() {}

protected:
	static const uint UI_APP_COMPONENT_TITLE_MAX_SIZE = 128;
	char title[UI_APP_COMPONENT_TITLE_MAX_SIZE]; // Made protected so that client can't corrupt mem

public:
	void setTitle(const char* title_)
	{
		strncpy_s(this->title, title_, UI_APP_COMPONENT_TITLE_MAX_SIZE);
		this->title[UI_APP_COMPONENT_TITLE_MAX_SIZE - 1] = '\0';
	}
};

class UIAppComponentGui : public UIAppComponentBase
{
public:
	void init(const TextDrawDesc* settings)
	{
		initGUIDriver(&driver);

		fontSize = settings->mFontSize;
		font = settings->mFontID;
		setTitle("Configuration");

		MutexLock lock(gMutex);
		gInstances.emplace_back(this);

		if (gInstances.size() == 1)
		{
#if !defined(_DURANGO) && !defined(TARGET_IOS)
			registerKeyboardCharEvent(uiKeyboardChar);
			registerKeyboardButtonEvent(uiKeyboardButton);
			registerMouseButtonEvent(uiMouseButton);
			registerMouseMoveEvent(uiMouseMove);
			registerMouseWheelEvent(uiMouseWheel);
#elif !defined(TARGET_IOS) && !defined(LINUX)
			registerJoystickButtonEvent(uiJoystickButton);
#else
			registerTouchEvent(uiTouch);
			registerTouchMoveEvent(uiTouchMove);
#endif
		}
	}

	void exit()
	{
		removeGUIDriver(driver);

		MutexLock lock(gMutex);
		gInstances.erase(gInstances.find(this));
	}

	void load(int32_t initialOffsetX, int32_t initialOffsetY, uint32_t initialWidth, uint32_t initialHeight)
	{
		driver->load(renderer, font, fontSize, cursorTexture);

		initialWindowRect =
		{
			float(initialOffsetX),
			// the orginal implmentation had this 600 - y
			600 - float(initialOffsetY),
			float(initialWidth),
			float(initialHeight)
		};
	}

	void unload()
	{
	}

	void update()
	{
		driver->clear();
		driver->processInput();
		driver->window(title, initialWindowRect.x, initialWindowRect.y, initialWindowRect.z, initialWindowRect.w,
			windowRect.x, windowRect.y, windowRect.z, windowRect.w, &ui->getProperty(0), ui->getPropertyCount());
	}

	void draw(struct Cmd* pCmd)
	{
		if (!drawGui)
			return;

		driver->draw(pCmd);
	}

	// returns: 0: no input handled, 1: input handled
	bool onChar(const struct KeyboardCharEventData* pData)
	{
		driver->onChar(pData);
		return wantKeyboardInput;
	}

	bool onKey(const struct KeyboardButtonEventData* pData)
	{
		UNREF_PARAM(pData);
		return wantKeyboardInput;
	}

	bool onJoystickButton(const struct JoystickButtonEventData* pData)
	{
		return driver->onJoystick(pData->button, pData->pressed);
	}

	bool onMouseMove(const struct MouseMoveEventData* pData)
	{
		driver->onMouseMove(pData);

		if (IS_INBOX(pData->x, pData->y, windowRect.x, windowRect.y, windowRect.z, windowRect.w))
		{
			return true;
		}

		return false;
	}

	bool onMouseButton(const struct MouseButtonEventData* pData)
	{
		if (IS_INBOX(pData->x, pData->y, windowRect.x, windowRect.y, windowRect.z, windowRect.w))
		{
			driver->onMouseClick(pData);
			return true;
		}
		// should allways let the mouse be released
		else if (!pData->pressed)
		{
			driver->onMouseClick(pData);
		}

		return false;
	}

	bool onMouseWheel(const struct MouseWheelEventData* pData)
	{
		if (IS_INBOX(pData->x, pData->y, windowRect.x, windowRect.y, windowRect.z, windowRect.w))
		{
			driver->onMouseScroll(pData);
			return true;
		}
		return false;
	}

	bool onTouch(const struct TouchEventData* pData)
	{
		driver->onTouch(pData);
		return true;
	}

	bool onTouchMove(const struct TouchEventData* pData)
	{
		driver->onTouchMove(pData);
		return true;
	}

	GUIDriver* driver;
	float4 initialWindowRect;
	float4 windowRect;

private:

public:
	bool drawGui = true;

private:
	bool wantKeyboardInput;
};
/************************************************************************/
// Event Handlers
/************************************************************************/
#if !defined(_DURANGO) && !defined(TARGET_IOS)
static bool uiKeyboardChar(const KeyboardCharEventData* pData)
{
	for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
		if (gInstances[i]->drawGui && gInstances[i]->onChar(pData))
			return true;

	return false;
}

static bool uiKeyboardButton(const KeyboardButtonEventData* pData)
{
#if !defined(TARGET_IOS)
	if (pData->key == KEY_F1 && pData->pressed)
	{
		for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
			gInstances[i]->drawGui = !gInstances[i]->drawGui;
	}

	for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
		if (gInstances[i]->drawGui && gInstances[i]->onKey(pData))
			return true;
#else
	ASSERT(false && "Unsupported on target iOS");
#endif

	return false;
}

static bool uiMouseMove(const MouseMoveEventData* pData)
{
#if !defined(TARGET_IOS)
	for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
	{
		if (gInstances[i]->drawGui && gInstances[i]->onMouseMove(pData))
		{
			PlatformEvents::skipMouseCapture = gInstances[i]->onMouseMove(pData);
			return true;
		}
	}

	PlatformEvents::skipMouseCapture = false;
#else
	ASSERT(false && "Unsupported on target iOS");
#endif

	return false;
}

static bool uiMouseButton(const MouseButtonEventData* pData)
{
#if !defined(TARGET_IOS)
	for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
		if (gInstances[i]->drawGui && gInstances[i]->onMouseButton(pData))
			return true;
#else
	ASSERT(false && "Unsupported on target iOS");
#endif

	return false;
}

static bool uiMouseWheel(const MouseWheelEventData* pData)
{
#if !defined(TARGET_IOS)
	for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
		if (gInstances[i]->drawGui && gInstances[i]->onMouseWheel(pData))
			return true;
#else
	ASSERT(false && "Unsupported on target iOS");
#endif

	return false;
}
#endif

//unused functions below on macos. Used on iOS and rest of platforms.
static bool uiJoystickButton(const JoystickButtonEventData* pData)
{
#if !defined(TARGET_IOS)
	if (pData->button == BUTTON_MENU && pData->pressed)
	{
		for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
			gInstances[i]->drawGui = !gInstances[i]->drawGui;
	}

	for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
		if (gInstances[i]->drawGui && gInstances[i]->onJoystickButton(pData))
			return true;
#else
	ASSERT(false && "Unsupported on target iOS");
#endif

	return false;
}

static bool uiTouch(const TouchEventData* pData)
{
#if defined(TARGET_IOS)
	for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
		if (gInstances[i]->drawGui && gInstances[i]->onTouch(pData))
			return true;
#else
	ASSERT(false && "Unsupported on this target.");
#endif

	return false;
}

static bool uiTouchMove(const TouchEventData* pData)
{
#if defined(TARGET_IOS)
	for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
		if (gInstances[i]->drawGui && gInstances[i]->onTouchMove(pData))
			return true;
#else
	ASSERT(false && "Unsupported on this target.");
#endif

	return false;
}
/************************************************************************/
/************************************************************************/
struct UIAppImpl
{
	UIRenderer*									pUIRenderer;
	uint32_t									mWidth;
	uint32_t									mHeight;

	tinystl::vector<GuiComponent*>				mComponents;

	tinystl::vector<struct GuiComponentImpl*>	mComponentsToUpdate;
};
UIAppImpl* pInst;


struct GuiComponentImpl
{
	UIAppComponentGui*	pGui;
	UI*					pUI;

	bool Init(class UIApp* pApp, const char* pTitle, const GuiDesc* pDesc)
	{
		pUI = conf_placement_new<UI>(conf_calloc(1, sizeof(UI)));
		pGui = conf_placement_new<UIAppComponentGui>(conf_calloc(1, sizeof(UIAppComponentGui)));
		pGui->init(&pDesc->mDefaultTextDrawDesc);
		pGui->renderer = pApp->pImpl->pUIRenderer;
		pGui->ui = pUI;
		pGui->load((int)pDesc->mStartPosition.getX(), (int)pDesc->mStartPosition.getY(),
			(int)pDesc->mStartSize.getX(), (int)pDesc->mStartSize.getY());
		pGui->setTitle(pTitle);

		return true;
	}

	void Exit()
	{
		pGui->unload();
		pGui->exit();
		pGui->~UIAppComponentGui();
		conf_free(pGui);
		pUI->~UI();
		conf_free(pUI);
	}
};

bool UIApp::Init(Renderer* renderer)
{
	pImpl = (struct UIAppImpl*)conf_calloc(1, sizeof(*pImpl));
	pInst = pImpl;
	pImpl->pUIRenderer = conf_placement_new<UIRenderer>(conf_calloc(1, sizeof(UIRenderer)), renderer);

	// Figure out the max font size for the current configuration
	uint32 uiMaxFrontSize = uint32(UIMaxFontSize::UI_MAX_FONT_SIZE_512);
	// Add and initialize the fontstash 
	pImpl->pUIRenderer->addFontstash(uiMaxFrontSize, uiMaxFrontSize);

	return true;
}

void UIApp::Exit()
{
	// Make copy of vector since RemoveGuiComponent will modify the original vector
	tinystl::vector<GuiComponent*> components = pImpl->mComponents;
	for (uint32_t i = 0; i < (uint32_t)components.size(); ++i)
		RemoveGuiComponent(components[i]);

	pImpl->pUIRenderer->~UIRenderer();
	conf_free(pImpl->pUIRenderer);

	pImpl->~UIAppImpl();
	conf_free(pImpl);
}

bool UIApp::Load(RenderTarget** rts)
{
	pImpl->mWidth = rts[0]->mDesc.mWidth;
	pImpl->mHeight = rts[0]->mDesc.mHeight;
	return true;
}

void UIApp::Unload()
{
}

uint32_t UIApp::LoadFont(const char* pFontPath, uint32_t root)
{
	uint32_t fontID = (uint32_t)pImpl->pUIRenderer->getFontstash(0)->defineFont("default", pFontPath, root);
	ASSERT(fontID != -1);

	return fontID;
}

GuiComponent* UIApp::AddGuiComponent(const char* pTitle, const GuiDesc* pDesc)
{
	GuiComponent* pComponent = conf_placement_new<GuiComponent>(conf_calloc(1, sizeof(GuiComponent)));
	pComponent->pImpl = (struct GuiComponentImpl*)conf_calloc(1, sizeof(GuiComponentImpl));
	pComponent->pImpl->Init(this, pTitle, pDesc);

	pImpl->mComponents.emplace_back(pComponent);

	return pComponent;
}

void UIApp::RemoveGuiComponent(GuiComponent* pComponent)
{
	ASSERT(pComponent);

	pImpl->mComponents.erase(pImpl->mComponents.find(pComponent));

	pComponent->pImpl->Exit();
	conf_free(pComponent->pImpl);
	pComponent->~GuiComponent();
	conf_free(pComponent);
}

void UIApp::Update(float deltaTime)
{
	for (uint32_t i = 0; i < (uint32_t)pImpl->mComponentsToUpdate.size(); ++i)
		pImpl->mComponentsToUpdate[i]->pGui->update();


	pImpl->mComponentsToUpdate.clear();
}

void UIApp::Draw(Cmd* pCmd)
{
	pImpl->pUIRenderer->beginRender(
		pImpl->mWidth, pImpl->mHeight,
		pCmd->mBoundRenderTargetCount, (ImageFormat::Enum*)pCmd->pBoundColorFormats, pCmd->pBoundSrgbValues,
		(ImageFormat::Enum)pCmd->mBoundDepthStencilFormat,
		pCmd->mBoundSampleCount, pCmd->mBoundSampleQuality);

	for (uint32_t i = 0; i < (uint32_t)pImpl->mComponentsToUpdate.size(); ++i)
	{
		pImpl->mComponentsToUpdate[i]->pGui->draw(pCmd);
	}
}

void UIApp::Gui(GuiComponent* pGui)
{
	pImpl->mComponentsToUpdate.emplace_back(pGui->pImpl);
}

uint32_t GuiComponent::AddProperty(const UIProperty& prop)
{
	return pImpl->pUI->addProperty(prop);
}

void GuiComponent::RemoveProperty(uint32_t propID)
{
	pImpl->pUI->removeProperty(propID);
}
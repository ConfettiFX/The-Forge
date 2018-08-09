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

#include "NuklearGUIDriver.h"
#include "UIShaders.h"

#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Image/Image.h"
#include "../../Common_3/OS/Interfaces/ICameraController.h"

#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/GpuProfiler.h"
#include "../../Common_3/Renderer/ResourceLoader.h"


#include "../../Common_3/ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"

#include "../../Middleware_3/Text/Fontstash.h"

#include "../Input/InputSystem.h"
#include "../Input/InputMappings.h"

#include "../../Common_3/OS/Core/RingBuffer.h"

#include "../../Common_3/OS/Interfaces/IMemoryManager.h"




namespace PlatformEvents
{
	extern bool skipMouseCapture;
}

static tinystl::vector<class UIAppComponentGui*> gInstances;
static Mutex gMutex;

extern void initGUIDriver(Renderer* pRenderer, GUIDriver** ppDriver);
extern void removeGUIDriver(GUIDriver* pDriver);

static bool uiKeyboardChar(const KeyboardCharEventData* pData);
static bool uiKeyboardButton(const KeyboardButtonEventData* pData);
static bool uiMouseMove(const MouseMoveEventData* pData);
static bool uiMouseButton(const MouseButtonEventData* pData);
static bool uiMouseWheel(const MouseWheelEventData* pData);
static bool uiJoystickButton(const JoystickButtonEventData* pData);
static bool uiTouch(const TouchEventData* pData);
static bool uiTouchMove(const TouchEventData* pData);

static bool uiInputEvent(const ButtonData* pData);
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
	Fontstash* pFontstash;
	UI* ui;
	Renderer* renderer;
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
	void init(Renderer* pRenderer, const TextDrawDesc* settings, Fontstash* fontstash)
	{
		initGUIDriver(pRenderer, &driver);

		pFontstash = fontstash;
		fontSize = settings->mFontSize;
		font = settings->mFontID;
		setTitle("Configuration");

		MutexLock lock(gMutex);
		gInstances.emplace_back(this);

		if (gInstances.size() == 1)
		{
			InputSystem::RegisterInputEvent(uiInputEvent);

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
		driver->load(pFontstash, fontSize, cursorTexture);

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
	bool onInput(const struct ButtonData* pData)
	{

		if (pData->mUserId == KEY_UI_MOVE)
		{
			driver->onInput(pData);

			if (IS_INBOX(pData->mValue[0], pData->mValue[1], windowRect.x, windowRect.y, windowRect.z, windowRect.w))
			{
				return true;
			}
		}
		else if (pData->mUserId == KEY_CONFIRM)
		{
			ButtonData rightStick = InputSystem::GetButtonData((uint32_t)KEY_UI_MOVE);

			if (IS_INBOX(rightStick.mValue[0], rightStick.mValue[1], windowRect.x, windowRect.y, windowRect.z, windowRect.w))
			{
				ButtonData toSend = *pData;
				toSend.mValue[0] = rightStick.mValue[0];
				toSend.mValue[1] = rightStick.mValue[1];
				driver->onInput(&toSend);
				PlatformEvents::skipMouseCapture = true;
				return true;
			}
			// should allways let the mouse be released
			else
			{
				ButtonData toSend = *pData;
				toSend.mValue[0] = rightStick.mValue[0];
				toSend.mValue[1] = rightStick.mValue[1];
				driver->onInput(&toSend);
				PlatformEvents::skipMouseCapture = false;
			}
		}

		return false;
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
static bool uiInputEvent(const ButtonData * pData)
{
	for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
		if (gInstances[i]->drawGui && gInstances[i]->onInput(pData))
			return true;

	//maps to f1
	if (pData->mUserId == KEY_LEFT_STICK_BUTTON && pData->mIsTriggered)
	{
		for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
			gInstances[i]->drawGui = !gInstances[i]->drawGui;
	}
	
	return false;
}

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
	Renderer*									pRenderer;
	Fontstash*									pFontStash;
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
		pGui->init(pApp->pImpl->pRenderer, &pDesc->mDefaultTextDrawDesc, pApp->pImpl->pFontStash);
		pGui->renderer = pApp->pImpl->pRenderer;
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
	pImpl->pRenderer = renderer;
	// Figure out the max font size for the current configuration
	uint32 uiMaxFrontSize = uint32(UIMaxFontSize::UI_MAX_FONT_SIZE_512);

	// Add and initialize the fontstash 
	pImpl->pFontStash = conf_placement_new<Fontstash>(conf_calloc(1, sizeof(Fontstash)), renderer, (int)uiMaxFrontSize, (int)uiMaxFrontSize);

	return true;
}

void UIApp::Exit()
{
	// Make copy of vector since RemoveGuiComponent will modify the original vector
	tinystl::vector<GuiComponent*> components = pImpl->mComponents;
	for (uint32_t i = 0; i < (uint32_t)components.size(); ++i)
		RemoveGuiComponent(components[i]);

	pImpl->pFontStash->destroy();
	conf_free(pImpl->pFontStash);

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
	uint32_t fontID = (uint32_t)pImpl->pFontStash->defineFont("default", pFontPath, root);
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
/************************************************************************/
/************************************************************************/
bool VirtualJoystickUI::Init(Renderer* renderer, const char* pJoystickTexture, uint32_t root)
{
	pRenderer = renderer;

	TextureLoadDesc loadDesc = {};
	loadDesc.pFilename = pJoystickTexture;
	loadDesc.mRoot = (FSRoot)root;
	loadDesc.ppTexture = &pTexture;
	addResource(&loadDesc);

	if (!pTexture)
		return false;
	/************************************************************************/
	// States
	/************************************************************************/
	SamplerDesc samplerDesc =
	{
		FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_NEAREST,
		ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE
};
	addSampler(pRenderer, &samplerDesc, &pSampler);

	BlendStateDesc blendStateDesc = {};
	blendStateDesc.mSrcFactor = BC_SRC_ALPHA;
	blendStateDesc.mDstFactor = BC_ONE_MINUS_SRC_ALPHA;
	blendStateDesc.mSrcAlphaFactor = BC_SRC_ALPHA;
	blendStateDesc.mDstAlphaFactor = BC_ONE_MINUS_SRC_ALPHA;
	blendStateDesc.mMask = ALL;
	blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
	addBlendState(pRenderer, &blendStateDesc, &pBlendAlpha);

	DepthStateDesc depthStateDesc = {};
	depthStateDesc.mDepthTest = false;
	depthStateDesc.mDepthWrite = false;
	addDepthState(pRenderer, &depthStateDesc, &pDepthState);

	RasterizerStateDesc rasterizerStateDesc = {};
	rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
	rasterizerStateDesc.mScissor = true;
	addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterizerState);
	/************************************************************************/
	// Shader
	/************************************************************************/
#if defined(METAL)
	String texturedShaderFile = "builtin_plain";
	String texturedShader = mtl_builtin_textured;
	ShaderDesc texturedShaderDesc = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { texturedShaderFile, texturedShader, "VSMain" }, { texturedShaderFile, texturedShader, "PSMain" } };
	addShader(pRenderer, &texturedShaderDesc, &pShader);
#elif defined(DIRECT3D12) || defined(VULKAN)
	char* pTexturedVert = NULL; uint32_t texturedVertSize = 0;
	char* pTexturedFrag = NULL; uint32_t texturedFragSize = 0;

	if (pRenderer->mSettings.mApi == RENDERER_API_D3D12 || pRenderer->mSettings.mApi == RENDERER_API_XBOX_D3D12)
	{
		pTexturedVert = (char*)d3d12_builtin_textured_vert; texturedVertSize = sizeof(d3d12_builtin_textured_vert);
		pTexturedFrag = (char*)d3d12_builtin_textured_frag; texturedFragSize = sizeof(d3d12_builtin_textured_frag);
	}
	else if (pRenderer->mSettings.mApi == RENDERER_API_VULKAN)
	{
		pTexturedVert = (char*)vk_builtin_textured_vert; texturedVertSize = sizeof(vk_builtin_textured_vert);
		pTexturedFrag = (char*)vk_builtin_textured_frag; texturedFragSize = sizeof(vk_builtin_textured_frag);
	}
	
	BinaryShaderDesc texturedShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
		{ (char*)pTexturedVert, texturedVertSize },{ (char*)pTexturedFrag, texturedFragSize } };
	addShaderBinary(pRenderer, &texturedShader, &pShader);
#endif
	
	
	const char* pStaticSamplerNames[] = { "uSampler" };
	RootSignatureDesc textureRootDesc = { &pShader, 1 };
	textureRootDesc.mStaticSamplerCount = 1;
	textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
	textureRootDesc.ppStaticSamplers = &pSampler;
	addRootSignature(pRenderer, &textureRootDesc, &pRootSignature);
	
	/************************************************************************/
	// Resources
	/************************************************************************/
	BufferDesc vbDesc = {};
	vbDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	vbDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	vbDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	vbDesc.mSize = 128 * 4 * sizeof(float4);
	vbDesc.mVertexStride = sizeof(float4);
	addMeshRingBuffer(pRenderer, &vbDesc, NULL, &pMeshRingBuffer);
	/************************************************************************/
	/************************************************************************/

	return true;
}

void VirtualJoystickUI::Exit()
{
	removeMeshRingBuffer(pMeshRingBuffer);
	removeRasterizerState(pRasterizerState);
	removeBlendState(pBlendAlpha);
	removeDepthState(pDepthState);
	removeRootSignature(pRenderer, pRootSignature);
	removeShader(pRenderer, pShader);
	removeResource(pTexture);
}

bool VirtualJoystickUI::Load(RenderTarget* pScreenRT, uint32_t depthFormat )
{
	VertexLayout vertexLayout = {};
	vertexLayout.mAttribCount = 2;
	vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
	vertexLayout.mAttribs[0].mFormat = ImageFormat::RG32F;
	vertexLayout.mAttribs[0].mBinding = 0;
	vertexLayout.mAttribs[0].mLocation = 0;
	vertexLayout.mAttribs[0].mOffset = 0;

	vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
	vertexLayout.mAttribs[1].mFormat = ImageFormat::RG32F;
	vertexLayout.mAttribs[1].mBinding = 0;
	vertexLayout.mAttribs[1].mLocation = 1;
	vertexLayout.mAttribs[1].mOffset = calculateImageFormatStride(ImageFormat::RG32F);

	GraphicsPipelineDesc pipelineDesc = {};
	pipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_STRIP;
	pipelineDesc.mDepthStencilFormat = (ImageFormat::Enum)depthFormat;
	pipelineDesc.mRenderTargetCount = 1;
	pipelineDesc.mSampleCount = pScreenRT->mDesc.mSampleCount;
	pipelineDesc.mSampleQuality = pScreenRT->mDesc.mSampleQuality;
	pipelineDesc.pBlendState = pBlendAlpha;
	pipelineDesc.pColorFormats = &pScreenRT->mDesc.mFormat;
	pipelineDesc.pDepthState = pDepthState;
	pipelineDesc.pRasterizerState = pRasterizerState;
	pipelineDesc.pSrgbValues = &pScreenRT->mDesc.mSrgb;
	pipelineDesc.pRootSignature = pRootSignature;
	pipelineDesc.pShaderProgram = pShader;
	pipelineDesc.pVertexLayout = &vertexLayout;
	addPipeline(pRenderer, &pipelineDesc, &pPipeline);

	return true;
}

void VirtualJoystickUI::Unload()
{
	removePipeline(pRenderer, pPipeline);
}

void VirtualJoystickUI::Draw(Cmd* pCmd, class ICameraController* pCameraController, const float4& color)
{
#ifdef TARGET_IOS
	struct RootConstants
	{
		float4 color;
		float2 scaleBias;
	} data = {};

	cmdBindPipeline(pCmd, pPipeline);
	data.color = color;
	data.scaleBias = { 2.0f / (float)pCmd->mBoundWidth, -2.0f / (float)pCmd->mBoundHeight };
	DescriptorData params[2] = {};
	params[0].pName = "uRootConstants";
	params[0].pRootConstant = &data;
	params[1].pName = "uTex";
	params[1].ppTextures = &pTexture;
	cmdBindDescriptors(pCmd, pRootSignature, 2, params);

	// Draw the camera controller's virtual joysticks.
	float extSide = min(pCmd->mBoundHeight, pCmd->mBoundWidth) * pCameraController->getVirtualJoystickExternalRadius();
	float intSide = min(pCmd->mBoundHeight, pCmd->mBoundWidth) * pCameraController->getVirtualJoystickInternalRadius();

	{
		float2 joystickSize = float2(extSide);
		vec2 joystickCenter = pCameraController->getVirtualLeftJoystickCenter();
		float2 joystickPos = float2(joystickCenter.getX() * pCmd->mBoundWidth, joystickCenter.getY() * pCmd->mBoundHeight) - 0.5f * joystickSize;

		// the last variable can be used to create a border
		TexVertex vertices[] = { MAKETEXQUAD(joystickPos.x, joystickPos.y,
			joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0) };
		RingBufferOffset buffer = getVertexBufferOffset(pMeshRingBuffer, sizeof(vertices));
		BufferUpdateDesc updateDesc = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
		updateResource(&updateDesc);
		cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
		cmdDraw(pCmd, 4, 0);
	}
	{
		vec2 joystickCenter = pCameraController->getVirtualRightJoystickCenter();
		float2 joystickSize = float2(extSide);
		float2 joystickPos = float2(joystickCenter.getX() * pCmd->mBoundWidth, joystickCenter.getY() * pCmd->mBoundHeight) - 0.5f * joystickSize;

		// the last variable can be used to create a border
		TexVertex vertices[] = { MAKETEXQUAD(joystickPos.x, joystickPos.y,
			joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0) };
		RingBufferOffset buffer = getVertexBufferOffset(pMeshRingBuffer, sizeof(vertices));
		BufferUpdateDesc updateDesc = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
		updateResource(&updateDesc);
		cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
		cmdDraw(pCmd, 4, 0);
	}
	{
		float2 joystickSize = float2(intSide);
		vec2 joystickCenter = pCameraController->getVirtualLeftJoystickPos();
		float2 joystickPos = float2(joystickCenter.getX() * pCmd->mBoundWidth, joystickCenter.getY() * pCmd->mBoundHeight) - 0.5f * joystickSize;

		// the last variable can be used to create a border
		TexVertex vertices[] = { MAKETEXQUAD(joystickPos.x, joystickPos.y,
			joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0) };
		RingBufferOffset buffer = getVertexBufferOffset(pMeshRingBuffer, sizeof(vertices));
		BufferUpdateDesc updateDesc = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
		updateResource(&updateDesc);
		cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
		cmdDraw(pCmd, 4, 0);
	}
	{
		float2 joystickSize = float2(intSide);
		vec2 joystickCenter = pCameraController->getVirtualRightJoystickPos();
		float2 joystickPos = float2(joystickCenter.getX() * pCmd->mBoundWidth, joystickCenter.getY() * pCmd->mBoundHeight) - 0.5f * joystickSize;

		// the last variable can be used to create a border
		TexVertex vertices[] = { MAKETEXQUAD(joystickPos.x, joystickPos.y,
			joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0) };
		RingBufferOffset buffer = getVertexBufferOffset(pMeshRingBuffer, sizeof(vertices));
		BufferUpdateDesc updateDesc = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
		updateResource(&updateDesc);
		cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
		cmdDraw(pCmd, 4, 0);
	}
#endif
}

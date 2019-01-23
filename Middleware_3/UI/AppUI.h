/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Interfaces/IMiddleware.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"
#include "../Text/Fontstash.h"

typedef void (*WidgetCallback)();

extern FSRoot FSR_MIDDLEWARE_UI;

class IWidget
{
	public:
	IWidget(const tinystl::string& _label):
		pOnHover(NULL),
		pOnActive(NULL),
		pOnFocus(NULL),
		pOnEdited(NULL),
		pOnDeactivated(NULL),
		pOnDeactivatedAfterEdit(NULL),
		mLabel(_label)
	{
	}
	virtual ~IWidget() {}
	virtual IWidget* Clone() const = 0;
	virtual void     Draw() = 0;

	// Common callbacks that can be used by the clients
	WidgetCallback pOnHover;          // Widget is hovered, usable, and not blocked by anything.
	WidgetCallback pOnActive;         // Widget is currently active (ex. button being held)
	WidgetCallback pOnFocus;          // Widget is currently focused (for keyboard/gamepad nav)
	WidgetCallback pOnEdited;         // Widget just changed its underlying value or was pressed.
	WidgetCallback pOnDeactivated;    // Widget was just made inactive from an active state.  This is useful for undo/redo patterns.
	WidgetCallback
		pOnDeactivatedAfterEdit;    // Widget was just made inactive from an active state and changed its underlying value.  This is useful for undo/redo patterns.

	tinystl::string mLabel;

	protected:
	void ProcessCallbacks();

	private:
	// Disable copy
	IWidget(IWidget const&);
	IWidget& operator=(IWidget const&);
};

class CollapsingHeaderWidget: public IWidget
{
	public:
	CollapsingHeaderWidget(const tinystl::string& _label, bool defaultOpen = false, bool collapsed = true):
		IWidget(_label),
		mCollapsed(collapsed),
		mDefaultOpen(defaultOpen),
		mPreviousCollapsed(!collapsed)
	{
	}

	~CollapsingHeaderWidget() { RemoveAllSubWidgets(); }

	IWidget* Clone() const;
	void     Draw();

	IWidget* AddSubWidget(const IWidget& widget)
	{
		mGroupedWidgets.emplace_back(widget.Clone());
		return mGroupedWidgets.back();
	}

	void RemoveSubWidget(IWidget* pWidget)
	{
		decltype(mGroupedWidgets)::iterator it = mGroupedWidgets.find(pWidget);
		if (it != mGroupedWidgets.end())
		{
			IWidget* pWidget = *it;
			mGroupedWidgets.erase(it);
			pWidget->~IWidget();
			conf_free(pWidget);
		}
	}

	void RemoveAllSubWidgets()
	{
		for (size_t i = 0; i < mGroupedWidgets.size(); ++i)
		{
			mGroupedWidgets[i]->~IWidget();
			conf_free(mGroupedWidgets[i]);
		}
	}

	void SetCollapsed(bool collapsed)
	{
		mCollapsed = collapsed;
		mPreviousCollapsed = !mCollapsed;
	}

	uint32_t GetSubWidgetCount() { return (uint32_t)mGroupedWidgets.size(); }

	IWidget* GetSubWidget(uint32_t index)
	{
		if (index < (uint32_t)mGroupedWidgets.size())
			return mGroupedWidgets[index];

		return NULL;
	}

	private:
	tinystl::vector<IWidget*> mGroupedWidgets;
	bool                      mCollapsed;
	bool                      mPreviousCollapsed;
	bool                      mDefaultOpen;
};

class LabelWidget: public IWidget
{
	public:
	LabelWidget(const tinystl::string& _label): IWidget(_label) {}

	IWidget* Clone() const;
	void     Draw();
};

class SeparatorWidget: public IWidget
{
	public:
	SeparatorWidget(): IWidget("") {}

	IWidget* Clone() const;
	void     Draw();
};

class ButtonWidget: public IWidget
{
	public:
	ButtonWidget(const tinystl::string& _label): IWidget(_label) {}

	IWidget* Clone() const;
	void     Draw();
};

class SliderFloatWidget: public IWidget
{
	public:
	SliderFloatWidget(
		const tinystl::string& _label, float* _data, float _min, float _max, float _step = 0.01f, const tinystl::string& _format = "%.3f"):
		IWidget(_label),
		mFormat(_format),
		pData(_data),
		mMin(_min),
		mMax(_max),
		mStep(_step)
	{
	}

	IWidget* Clone() const;
	void     Draw();

	protected:
	tinystl::string mFormat;
	float*          pData;
	float           mMin;
	float           mMax;
	float           mStep;
};

class SliderFloat2Widget: public IWidget
{
	public:
	SliderFloat2Widget(
		const tinystl::string& _label, float2* _data, const float2& _min, const float2& _max, const float2& _step = float2(0.01f, 0.01f),
		const tinystl::string& _format = "%.3f"):
		IWidget(_label),
		mFormat(_format),
		pData(_data),
		mMin(_min),
		mMax(_max),
		mStep(_step)
	{
	}

	IWidget* Clone() const;
	void     Draw();

	protected:
	tinystl::string mFormat;
	float2*         pData;
	float2          mMin;
	float2          mMax;
	float2          mStep;
};

class SliderFloat3Widget: public IWidget
{
	public:
	SliderFloat3Widget(
		const tinystl::string& _label, float3* _data, const float3& _min, const float3& _max,
		const float3& _step = float3(0.01f, 0.01f, 0.01f), const tinystl::string& _format = "%.3f"):
		IWidget(_label),
		mFormat(_format),
		pData(_data),
		mMin(_min),
		mMax(_max),
		mStep(_step)
	{
	}

	IWidget* Clone() const;
	void     Draw();

	protected:
	tinystl::string mFormat;
	float3*         pData;
	float3          mMin;
	float3          mMax;
	float3          mStep;
};

class SliderFloat4Widget: public IWidget
{
	public:
	SliderFloat4Widget(
		const tinystl::string& _label, float4* _data, const float4& _min, const float4& _max,
		const float4& _step = float4(0.01f, 0.01f, 0.01f, 0.01f), const tinystl::string& _format = "%.3f"):
		IWidget(_label),
		mFormat(_format),
		pData(_data),
		mMin(_min),
		mMax(_max),
		mStep(_step)
	{
	}

	IWidget* Clone() const;
	void     Draw();

	protected:
	tinystl::string mFormat;
	float4*         pData;
	float4          mMin;
	float4          mMax;
	float4          mStep;
};

class SliderIntWidget: public IWidget
{
	public:
	SliderIntWidget(
		const tinystl::string& _label, int32_t* _data, int32_t _min, int32_t _max, int32_t _step = 1,
		const tinystl::string& _format = "%d"):
		IWidget(_label),
		mFormat(_format),
		pData(_data),
		mMin(_min),
		mMax(_max),
		mStep(_step)
	{
	}

	IWidget* Clone() const;
	void     Draw();

	protected:
	tinystl::string mFormat;
	int32_t*        pData;
	int32_t         mMin;
	int32_t         mMax;
	int32_t         mStep;
};

class SliderUintWidget: public IWidget
{
	public:
	SliderUintWidget(
		const tinystl::string& _label, uint32_t* _data, uint32_t _min, uint32_t _max, uint32_t _step = 1,
		const tinystl::string& _format = "%d"):
		IWidget(_label),
		mFormat(_format),
		pData(_data),
		mMin(_min),
		mMax(_max),
		mStep(_step)
	{
	}

	IWidget* Clone() const;
	void     Draw();

	protected:
	tinystl::string mFormat;
	uint32_t*       pData;
	uint32_t        mMin;
	uint32_t        mMax;
	uint32_t        mStep;
};

class RadioButtonWidget: public IWidget
{
	public:
	RadioButtonWidget(const tinystl::string& _label, int32_t* _data, const int32_t _radioId):
		IWidget(_label),
		pData(_data),
		mRadioId(_radioId)
	{
	}

	IWidget* Clone() const;
	void     Draw();

	protected:
	int32_t* pData;
	int32_t  mRadioId;
};

class CheckboxWidget: public IWidget
{
	public:
	CheckboxWidget(const tinystl::string& _label, bool* _data): IWidget(_label), pData(_data) {}
	IWidget* Clone() const;
	void     Draw();

	protected:
	bool* pData;
};

class DropdownWidget: public IWidget
{
	public:
	DropdownWidget(const tinystl::string& _label, uint32_t* _data, const char** _names, const uint32_t* _values, uint32_t count):
		IWidget(_label),
		pData(_data)
	{
		mValues.resize(count);
		mNames.resize(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			mValues[i] = _values[i];
			mNames[i] = _names[i];
		}
	}
	IWidget* Clone() const;
	void     Draw();

	protected:
	uint32_t*                        pData;
	tinystl::vector<uint32_t>        mValues;
	tinystl::vector<tinystl::string> mNames;
};

class ProgressBarWidget: public IWidget
{
	public:
	ProgressBarWidget(const tinystl::string& _label, size_t* _data, size_t const _maxProgress):
		IWidget(_label),
		pData(_data),
		mMaxProgress(_maxProgress)
	{
	}

	IWidget* Clone() const;
	void     Draw();

	protected:
	size_t* pData;
	size_t  mMaxProgress;
};

class ColorSliderWidget: public IWidget
{
	public:
	ColorSliderWidget(const tinystl::string& _label, uint32_t* _data): IWidget(_label), pData(_data) {}

	IWidget* Clone() const;
	void     Draw();

	protected:
	uint32_t* pData;
};

class ColorPickerWidget: public IWidget
{
	public:
	ColorPickerWidget(const tinystl::string& _label, uint32_t* _data): IWidget(_label), pData(_data) {}

	IWidget* Clone() const;
	void     Draw();

	protected:
	uint32_t* pData;
};

class TextboxWidget: public IWidget
{
	public:
	TextboxWidget(const tinystl::string& _label, char* _data, uint32_t const _length, bool const _autoSelectAll = true):
		IWidget(_label),
		pData(_data),
		mLength(_length),
		mAutoSelectAll(_autoSelectAll)
	{
	}

	IWidget* Clone() const;
	void     Draw();

	protected:
	char*    pData;
	uint32_t mLength;
	bool     mAutoSelectAll;
};

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

typedef struct GuiDesc
{
	GuiDesc(
		const vec2& startPos = { 0.0f, 150.0f }, const vec2& startSize = { 600.0f, 550.0f },
		const TextDrawDesc& textDrawDesc = { 0, 0xffffffff, 16 }):
		mStartPosition(startPos),
		mStartSize(startSize),
		mDefaultTextDrawDesc(textDrawDesc)
	{
	}

	vec2         mStartPosition;
	vec2         mStartSize;
	TextDrawDesc mDefaultTextDrawDesc;
} GuiDesc;

enum GuiComponentFlags
{
	GUI_COMPONENT_FLAGS_NONE = 0,
	GUI_COMPONENT_FLAGS_NO_TITLE_BAR = 1 << 0,             // Disable title-bar
	GUI_COMPONENT_FLAGS_NO_RESIZE = 1 << 1,                // Disable user resizing
	GUI_COMPONENT_FLAGS_NO_MOVE = 1 << 2,                  // Disable user moving the window
	GUI_COMPONENT_FLAGS_NO_SCROLLBAR = 1 << 3,             // Disable scrollbars (window can still scroll with mouse or programatically)
	GUI_COMPONENT_FLAGS_NO_COLLAPSE = 1 << 4,              // Disable user collapsing window by double-clicking on it
	GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE = 1 << 5,       // Resize every window to its content every frame
	GUI_COMPONENT_FLAGS_NO_INPUTS = 1 << 6,                // Disable catching mouse or keyboard inputs, hovering test with pass through.
	GUI_COMPONENT_FLAGS_MEMU_BAR = 1 << 7,                 // Has a menu-bar
	GUI_COMPONENT_FLAGS_HORIZONTAL_SCROLLBAR = 1 << 8,     // Allow horizontal scrollbar to appear (off by default).
	GUI_COMPONENT_FLAGS_NO_FOCUS_ON_APPEARING = 1 << 9,    // Disable taking focus when transitioning from hidden to visible state
	GUI_COMPONENT_FLAGS_NO_BRING_TO_FRONT_ON_FOCUS =
		1 << 10,    // Disable bringing window to front when taking focus (e.g. clicking on it or programatically giving it focus)
	GUI_COMPONENT_FLAGS_ALWAYS_VERTICAL_SCROLLBAR = 1 << 11,      // Always show vertical scrollbar (even if ContentSize.y < Size.y)
	GUI_COMPONENT_FLAGS_ALWAYS_HORIZONTAL_SCROLLBAR = 1 << 12,    // Always show horizontal scrollbar (even if ContentSize.x < Size.x)
	GUI_COMPONENT_FLAGS_ALWAYS_USE_WINDOW_PADDING =
		1
		<< 13,    // Ensure child windows without border uses style.WindowPadding (ignored by default for non-bordered child windows, because more convenient)
	GUI_COMPONENT_FLAGS_NO_NAV_INPUT = 1 << 14,    // No gamepad/keyboard navigation within the window
	GUI_COMPONENT_FLAGS_NO_NAV_FOCUS =
		1 << 15    // No focusing toward this window with gamepad/keyboard navigation (e.g. skipped by CTRL+TAB)
};

class GuiComponent
{
	public:
	IWidget* AddWidget(const IWidget& widget, bool clone = true);
	void     RemoveWidget(IWidget* pWidget);
	void     RemoveAllWidgets();

	class GUIDriver*          pDriver;
	tinystl::vector<IWidget*> mWidgets;
	tinystl::vector<bool>     mWidgetsClone;
	float4                    mInitialWindowRect;
	float4                    mCurrentWindowRect;
	tinystl::string           mTitle;
	bool                      mActive;
	// UI Component settings that can be modified at runtime by the client.
	bool mHasCloseButton;
	// defaults to GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE
	int32_t mFlags;

	// Contextual menus when right clicking the title bar
	tinystl::vector<tinystl::string> mContextualMenuLabels;
	tinystl::vector<WidgetCallback>  mContextualMenuCallbacks;
};
/************************************************************************/
// Helper Class for removing and adding properties easily
/************************************************************************/
typedef struct DynamicUIControls
{
	tinystl::vector<IWidget*> mDynamicProperties;
	tinystl::vector<IWidget*> mDynamicPropHandles;

	void ShowDynamicProperties(GuiComponent* pGui)
	{
		for (size_t i = 0; i < mDynamicProperties.size(); ++i)
		{
			mDynamicPropHandles.push_back(0);
			mDynamicPropHandles[i] = pGui->AddWidget(*mDynamicProperties[i]);
		}
	}

	void HideDynamicProperties(GuiComponent* pGui)
	{
		for (size_t i = 0; i < mDynamicPropHandles.size(); i++)
		{
			// We should not erase the widgets in this for-loop, otherwise the IDs
			// in mDynamicPropHandles will not match once  GuiComponent::mWidgets changes size.
			pGui->RemoveWidget(mDynamicPropHandles[i]);
		}

		mDynamicPropHandles.clear();
	}

	void Destroy()
	{
		for (size_t i = 0; i < mDynamicProperties.size(); ++i)
		{
			mDynamicProperties[i]->~IWidget();
			conf_free(mDynamicProperties[i]);
		}
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

	virtual bool
				 load(class Fontstash* fontID, float fontSize, struct Texture* cursorTexture = 0, float uiwidth = 600, float uiheight = 400) = 0;
	virtual void unload() = 0;

	virtual void* getContext() = 0;

	virtual bool update(float deltaTime, GuiComponent** pGuiComponents, uint32_t componentCount, bool showDemoWindow) = 0;

	virtual void draw(Cmd* q) = 0;

	virtual void onInput(const struct ButtonData* data) = 0;
	virtual bool isHovering(const float4& windowRect) = 0;
	virtual int  needsTextInput() const = 0;

	protected:
	// Since gestures events always come first, we want to dismiss any other inputs after that
	bool mHandledGestures;
};
/************************************************************************/
// UI interface for App
/************************************************************************/
// undefine the DrawText macro (WinUser.h) if its defined
#ifdef DrawText
#undef DrawText
#endif
class UIApp: public IMiddleware
{
	public:
	bool Init(Renderer* renderer);
	void Exit();

	bool Load(RenderTarget** rts);
	void Unload();

	void Update(float deltaTime);
	void Draw(Cmd* cmd);

	uint          LoadFont(const char* pFontPath, uint root);
	GuiComponent* AddGuiComponent(const char* pTitle, const GuiDesc* pDesc);
	void          RemoveGuiComponent(GuiComponent* pComponent);
	void          RemoveAllGuiComponents();

	void Gui(GuiComponent* pGui);

	// given a text and TextDrawDesc, this function returns the width and height of the printed text in pixels.
	//
	float2 MeasureText(const char* pText, const TextDrawDesc& drawDesc) const;

	// draws the @pText on screen using the @drawDesc descriptor and @screenCoordsInPx.
	//
	// Note:
	// @screenCoordsInPx: (0,0)                       is top left corner of the screen,
	//                    (screenWidth, screenHeight) is bottom right corner of the screen
	//
	void        DrawText(Cmd* cmd, const float2& screenCoordsInPx, const char* pText, const TextDrawDesc& drawDesc) const;
	inline void DrawText(Cmd* cmd, const float2& screenCoordsInPx, const char* pText) const
	{
		TextDrawDesc defaultDesc;
		DrawText(cmd, screenCoordsInPx, pText, defaultDesc);
	}

	// draws the @pText in world space by using the linear transformation pipeline.
	//
	void DrawTextInWorldSpace(Cmd* pCmd, const char* pText, const TextDrawDesc& drawDesc, const mat4& matWorld, const mat4& matProjView);

	/************************************************************************/
	// Data
	/************************************************************************/
	class GUIDriver*  pDriver;
	struct UIAppImpl* pImpl;
	bool              mHovering;

	// Following var is useful for seeing UI capabilities and tweaking style settings.
	// Will only take effect if at least one GUI Component is active.
	bool mShowDemoUiWindow;
};

class VirtualJoystickUI
{
	public:
	VirtualJoystickUI(): mInsideRadius(0.0f), mOutsideRadius(0.f), mDeadzone(0.f), mInitialized(false), mActive(false) {}

	// Init resources
	bool Init(Renderer* pRenderer, const char* pJoystickTexture, uint root);
	// Initialize input behavior parameters
	// This can be called many times in case different camera want to have different values.
	void InitLRSticks(float insideRad = 150.f, float outsideRad = 300.f, float deadzone = 20.f);
	void Exit();
	bool Load(RenderTarget* pScreenRT, uint depthFormat = 0);
	void Unload();
	void Update(float dt);

	// Get normalized diretion of joystick.
	vec2 GetLeftStickDir();
	vec2 GetRightStickDir();
	// Get outer radius of joystick. (Biggest size)
	vec2 GetStickRadius();
	// Retrieve state of specific joystick
	bool IsActive(bool left = true);
	// Check if any joystick is currently active
	bool IsAnyActive();
	// Helper to enable/disable joystick
	void SetActive(bool state);
	bool OnInputEvent(const ButtonData* pData);

	void Draw(Cmd* pCmd, const float4& color);

	private:
	Renderer*        pRenderer;
	Shader*          pShader;
	RootSignature*   pRootSignature;
	Pipeline*        pPipeline;
	Texture*         pTexture;
	Sampler*         pSampler;
	BlendState*      pBlendAlpha;
	DepthState*      pDepthState;
	RasterizerState* pRasterizerState;
	MeshRingBuffer*  pMeshRingBuffer;
	vec2             mRenderSize;
	//input related
	private:
	float mInsideRadius;
	float mOutsideRadius;
	float mDeadzone;
	bool  mInitialized;
	bool  mActive;

	struct StickInput
	{
		uint32_t mTouchIndex;
		bool     mIsPressed;
		vec2     mStartPos;
		vec2     mCurrPos;
		vec2     mDir;
	};
	// Left -> Index 0
	// Right -> Index 1
	StickInput mSticks[2];
};

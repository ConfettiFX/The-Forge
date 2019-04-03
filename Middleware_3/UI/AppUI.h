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
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Interfaces/IMiddleware.h"
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"
#include "../Text/Fontstash.h"

typedef void (*WidgetCallback)();
extern FSRoot FSR_MIDDLEWARE_UI;

struct Texture;

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
	CollapsingHeaderWidget(const tinystl::string& _label, bool defaultOpen = false, bool collapsed = true, bool headerIsVisible = true) :
		IWidget(_label), mCollapsed(collapsed), mPreviousCollapsed(!collapsed), mDefaultOpen(defaultOpen), mHeaderIsVisible(headerIsVisible) {}

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

	void SetDefaultOpen(bool defaultOpen)
	{
		mDefaultOpen = defaultOpen;
	}

	void SetHeaderVisible(bool visible)
	{
		mHeaderIsVisible = visible;
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
	bool					  mHeaderIsVisible;
};

class DebugTexturesWidget : public IWidget
{
public:
	DebugTexturesWidget(const tinystl::string& _label) :
		IWidget(_label),
		mTextureDisplaySize(float2(512.f, 512.f)) {}

	IWidget* Clone() const;
	void     Draw();

	void SetTextures(tinystl::vector<Texture*> const& textures, float2 const& displaySize)
	{
		mTextures = textures;
		mTextureDisplaySize = displaySize;
	}

private:
	tinystl::vector<Texture*> mTextures;
	float2 mTextureDisplaySize;
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
struct DescriptorBinder;
struct Pipeline;
struct Sampler;
struct RasterizerState;
struct DepthState;
struct BlendState;
struct GPURingBuffer;

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
typedef struct DynamicUIWidgets
{
	~DynamicUIWidgets()
	{
		Destroy();
	}

	IWidget* AddWidget(const IWidget& widget)
	{
		mDynamicProperties.emplace_back(widget.Clone());
		return mDynamicProperties.back();
	}

	void ShowWidgets(GuiComponent* pGui)
	{
		for (size_t i = 0; i < mDynamicProperties.size(); ++i)
		{
			pGui->AddWidget(*mDynamicProperties[i], false);
		}
	}

	void HideWidgets(GuiComponent* pGui)
	{
		for (size_t i = 0; i < mDynamicProperties.size(); i++)
		{
			// We should not erase the widgets in this for-loop, otherwise the IDs
			// in mDynamicPropHandles will not match once  GuiComponent::mWidgets changes size.
			pGui->RemoveWidget(mDynamicProperties[i]);
		}
	}

	void Destroy()
	{
		for (size_t i = 0; i < mDynamicProperties.size(); ++i)
		{
			mDynamicProperties[i]->~IWidget();
			conf_free(mDynamicProperties[i]);
		}

		mDynamicProperties.clear();
	}

private:
	tinystl::vector<IWidget*> mDynamicProperties;
} DynamicUIWidgets;
/************************************************************************/
// Abstract interface for handling GUI
/************************************************************************/
class GUIDriver
{
	public:
		struct GUIUpdate
		{
			GuiComponent** pGuiComponents;
			uint32_t componentCount;
			float deltaTime;
			float width;
			float height;
			bool showDemoWindow;
		};

	virtual bool init(Renderer* pRenderer) = 0;
	virtual void exit() = 0;

	virtual bool
				 load(class Fontstash* fontID, float fontSize, struct Texture* cursorTexture = 0, float uiwidth = 600, float uiheight = 400) = 0;
	virtual void unload() = 0;

	virtual void* getContext() = 0;

	virtual bool update(GUIUpdate* update) = 0;

	virtual void draw(Cmd* q) = 0;

	virtual void onInput(const struct ButtonData* data) = 0;
	virtual bool isHovering(const float4& windowRect) = 0;
	virtual int  needsTextInput() const = 0;

	protected:
	// Since gestures events always come first, we want to dismiss any other inputs after that
	bool mHandledGestures;
};

#if ENABLE_MICRO_PROFILER
/* MicroProfile Defines*/
enum
{
	MICROPROFILE_CUSTOM_BARS = 0x1,
	MICROPROFILE_CUSTOM_BAR_SOURCE_MAX = 0x2,
	MICROPROFILE_CUSTOM_BAR_SOURCE_AVG = 0,
	MICROPROFILE_CUSTOM_STACK = 0x4,
	MICROPROFILE_CUSTOM_STACK_SOURCE_MAX = 0x8,
	MICROPROFILE_CUSTOM_STACK_SOURCE_AVG = 0,
};

#define MICROPROFILE_DRAWCURSOR 0
#define MICROPROFILE_DETAILED_BAR_NAMES 1
#define MICROPROFILE_TEXT_WIDTH 10
#define MICROPROFILE_TEXT_HEIGHT 16
#define MICROPROFILE_DETAILED_BAR_HEIGHT 12
#define MICROPROFILE_DETAILED_CONTEXT_SWITCH_HEIGHT 7
#define MICROPROFILE_GRAPH_WIDTH 256
#define MICROPROFILE_GRAPH_HEIGHT 256
#define MICROPROFILE_BORDER_SIZE 1
#define MICROPROFILE_HELP_LEFT "Left-Click"
#define MICROPROFILE_HELP_RIGHT "Right-Click"
#define MICROPROFILE_HELP_MOD "Mod"
#define MICROPROFILE_BAR_WIDTH 100
#define MICROPROFILE_CUSTOM_MAX 8 
#define MICROPROFILE_CUSTOM_MAX_TIMERS 64
#define MICROPROFILE_CUSTOM_PADDING 12
#define MICROPROFILE_FRAME_HISTORY_HEIGHT 50
#define MICROPROFILE_FRAME_HISTORY_WIDTH 7
#define MICROPROFILE_FRAME_HISTORY_COLOR_CPU 0xffff7f27 //255 127 39
#define MICROPROFILE_FRAME_HISTORY_COLOR_GPU 0xff37a0ee //55 160 238
#define MICROPROFILE_FRAME_HISTORY_COLOR_HIGHTLIGHT 0x7733bb44
#define MICROPROFILE_FRAME_COLOR_HIGHTLIGHT 0x20009900
#define MICROPROFILE_FRAME_COLOR_HIGHTLIGHT_GPU 0x20996600
#define MICROPROFILE_NUM_FRAMES (MICROPROFILE_MAX_FRAME_HISTORY - (MICROPROFILE_GPU_FRAME_DELAY+1))
#define MICROPROFILE_TOOLTIP_MAX_STRINGS (32 + MICROPROFILE_MAX_GROUPS*2)
#define MICROPROFILE_TOOLTIP_STRING_BUFFER_SIZE (4*1024)
#define MICROPROFILE_TOOLTIP_MAX_LOCKED 3
#define MICROPROFILE_COUNTER_INDENT 4
#define MICROPROFILE_COUNTER_WIDTH 100
#define MICROPROFILE_UIWINDOW_WIDTH 1960
#define MICROPROFILE_UIWINDOW_HEIGHT 1080

struct MicroProfileStringArray
{
	const char* ppStrings[MICROPROFILE_TOOLTIP_MAX_STRINGS];
	char Buffer[MICROPROFILE_TOOLTIP_STRING_BUFFER_SIZE];
	char* pBufferPos;
	uint32_t nNumStrings;
};

struct MicroProfileGroupMenuItem
{
	uint32_t nIsCategory;
	uint32_t nCategoryIndex;
	uint32_t nIndex;
	const char* pName;
};

struct MicroProfileCustom
{
	char pName[MICROPROFILE_NAME_MAX_LEN];
	uint32_t nFlags;
	uint32_t nAggregateFlip;
	uint32_t nNumTimers;
	uint32_t nMaxTimers;
	uint64_t nGroupMask;
	float fReference;
	uint64_t* pTimers;
};

struct SOptionDesc
{
	SOptionDesc() {}
	SOptionDesc(uint8_t nSubType, uint8_t nIndex, const char* fmt, ...) :nSubType(nSubType), nIndex(nIndex)
	{
		va_list args;
		va_start(args, fmt);
		vsprintf_s(Text, 32, fmt, args);
		va_end(args);
	}
	char Text[32];
	uint8_t nSubType;
	uint8_t nIndex;
	bool bSelected;
};

typedef void(*MicroProfileLoopGroupCallback)(Cmd* pCmd, uint32_t nTimer, uint32_t nIdx, uint32_t nX, uint32_t nY, void* pData);



static uint32_t g_MicroProfileAggregatePresets[] = { 0, 10, 20, 30, 60, 120 };
static float g_MicroProfileReferenceTimePresets[] = { 5.f, 10.f, 15.f,20.f, 33.33f, 66.66f, 100.f, 250.f, 500.f, 1000.f };
static uint32_t g_MicroProfileOpacityPresets[] = { 0x40, 0x80, 0xc0, 0xff };
static const char* g_MicroProfilePresetNames[] =
{
	MICROPROFILE_DEFAULT_PRESET,
	"Render",
	"GPU",
	"Lighting",
	"AI",
	"Visibility",
	"Sound",
};

enum
{
	MICROPROFILE_NUM_REFERENCE_PRESETS = sizeof(g_MicroProfileReferenceTimePresets) / sizeof(g_MicroProfileReferenceTimePresets[0]),
	MICROPROFILE_NUM_OPACITY_PRESETS = sizeof(g_MicroProfileOpacityPresets) / sizeof(g_MicroProfileOpacityPresets[0]),
#if MICROPROFILE_CONTEXT_SWITCH_TRACE
	MICROPROFILE_OPTION_SIZE = MICROPROFILE_NUM_REFERENCE_PRESETS + MICROPROFILE_NUM_OPACITY_PRESETS * 2 + 2 + 6,
#else
	MICROPROFILE_OPTION_SIZE = MICROPROFILE_NUM_REFERENCE_PRESETS + MICROPROFILE_NUM_OPACITY_PRESETS * 2 + 2 + 3,
#endif
};

struct MicroProfileUI
{
	//menu/mouse over stuff
	uint64_t nHoverToken;
	int64_t  nHoverTime;
	int 	 nHoverFrame;
#if MICROPROFILE_DEBUG
	uint64_t nHoverAddressEnter;
	uint64_t nHoverAddressLeave;
#endif

	uint32_t nWidth;
	uint32_t nHeight;

	int nOffsetX[MP_DRAW_SIZE];
	int nOffsetY[MP_DRAW_SIZE];

	float fDetailedOffset; //display offset relative to start of latest displayable frame.
	float fDetailedRange; //no. of ms to display
	float fDetailedOffsetTarget;
	float fDetailedRangeTarget;
	uint32_t nOpacityBackground;
	uint32_t nOpacityForeground;
	bool bShowSpikes;



	uint32_t 				nMouseX;
	uint32_t 				nMouseY;
	uint32_t 				nMouseDownX;
	uint32_t 				nMouseDownY;
	int						nMouseWheelDelta;
	uint32_t				nMouseDownLeft;
	uint32_t				nMouseDownRight;
	uint32_t 				nMouseLeft;
	uint32_t 				nMouseRight;
	uint32_t 				nMouseLeftMod;
	uint32_t 				nMouseRightMod;
	uint32_t				nModDown;
	uint32_t 				nActiveMenu;

	MicroProfileLogEntry*	pDisplayMouseOver;
	uint64_t				nDisplayMouseOverTimerIndex;

	int64_t					nRangeBegin;
	int64_t					nRangeEnd;
	int64_t					nRangeBeginGpu;
	int64_t					nRangeEndGpu;
	uint32_t				nRangeBeginIndex;
	uint32_t 				nRangeEndIndex;
	MicroProfileThreadLog* 	pRangeLog;
	uint32_t				nHoverColor;
	uint32_t				nHoverColorShared;

	int64_t					nTickReferenceCpu;
	int64_t					nTickReferenceGpu;

	MicroProfileStringArray LockedToolTips[MICROPROFILE_TOOLTIP_MAX_LOCKED];
	uint32_t  				nLockedToolTipColor[MICROPROFILE_TOOLTIP_MAX_LOCKED];
	int 					LockedToolTipFront;

	MicroProfileGroupMenuItem 	GroupMenu[MICROPROFILE_MAX_GROUPS + MICROPROFILE_MAX_CATEGORIES];
	uint32_t 					GroupMenuCount;


	uint32_t					nCustomActive;
	uint32_t					nCustomTimerCount;
	uint32_t 					nCustomCount;
	MicroProfileCustom 			Custom[MICROPROFILE_CUSTOM_MAX];
	uint64_t					CustomTimer[MICROPROFILE_CUSTOM_MAX_TIMERS];

	SOptionDesc Options[MICROPROFILE_OPTION_SIZE];

	uint32_t nCounterWidth;
	uint32_t nLimitWidth;
	uint32_t nCounterWidthTemp;
	uint32_t nLimitWidthTemp;


};

#define UI g_MicroProfileUI
static uint32_t g_nMicroProfileBackColors[2] = { 0x474747, 0x313131 };
#define MICROPROFILE_NUM_CONTEXT_SWITCH_COLORS 16
static uint32_t g_nMicroProfileContextSwitchThreadColors[MICROPROFILE_NUM_CONTEXT_SWITCH_COLORS] = //palette generated by http://tools.medialab.sciences-po.fr/iwanthue/index.php
{
	0x63607B,
	0x755E2B,
	0x326A55,
	0x523135,
	0x904F42,
	0x87536B,
	0x346875,
	0x5E6046,
	0x35404C,
	0x224038,
	0x413D1E,
	0x5E3A26,
	0x5D6161,
	0x4C6234,
	0x7D564F,
	0x5C4352,
};


#define SBUF_MAX 32
struct MicroProfileMetaAverageArgs
{
	uint64_t* pCounters;
	float fRcpFrames;
};
typedef const char* (*MicroProfileSubmenuCallback)(int, bool* bSelected);
typedef void(*MicroProfileClickCallback)(int);
class UIApp;
static UIApp* gUIApp_MP = NULL;
static MICROPROFILE_DEFINE(g_MicroProfileDrawGraph, "MicroProfile", "Draw Graph", 0xff44ee00);
static MICROPROFILE_DEFINE(g_MicroProfileDrawBarView, "MicroProfile", "DrawBarView", 0x00dd77);
static MICROPROFILE_DEFINE(g_MicroProfileDraw, "MicroProfile", "Draw", 0x737373);
static MICROPROFILE_DEFINE(g_MicroProfileDetailed, "MicroProfile", "Detailed View", 0x8888000);
static MicroProfileUI g_MicroProfileUI;

#endif

/************************************************************************/
// UI interface for App
/************************************************************************/
// undefine the DrawText macro (WinUser.h) if its defined
#ifdef DrawText
#undef DrawText
#endif

typedef struct GpuProfiler        GpuProfiler;
typedef struct GpuProfileDrawDesc GpuProfileDrawDesc;
struct UIAppImpl
{
	Renderer*  pRenderer;
	Fontstash* pFontStash;

	tinystl::vector<GuiComponent*> mComponents;

	tinystl::vector<GuiComponent*> mComponentsToUpdate;
	bool                           mUpdated;
};
class UIApp: public IMiddleware
{
	public:
	UIApp(int32_t const fontAtlasSize = 0);

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
	void DrawText(Cmd* cmd, const float2& screenCoordsInPx, const char* pText, const TextDrawDesc* pDrawDesc = NULL) const;

	// draws the @pText in world space by using the linear transformation pipeline.
	//
	void DrawTextInWorldSpace(Cmd* pCmd, const char* pText, const mat4& matWorld, const mat4& matProjView, const TextDrawDesc* pDrawDesc = NULL);

	void DrawDebugGpuProfile(Cmd* pCmd, const float2& screenCoordsInPx, GpuProfiler* pGpuProfiler, const GpuProfileDrawDesc* pDrawDesc = NULL);

	/************************************************************************/
	// Data
	/************************************************************************/
	class GUIDriver*  pDriver;
	struct UIAppImpl* pImpl;
	bool              mHovering;

	// Following var is useful for seeing UI capabilities and tweaking style settings.
	// Will only take effect if at least one GUI Component is active.
	bool mShowDemoUiWindow;

	void ActivateMicroProfile(bool isActive);
	bool			  mMicroProfileEnabled;
	private:
	float   mWidth;
	float   mHeight;
#if	ENABLE_MICRO_PROFILER
	void ProfileDraw(Cmd* pCmd, uint32_t nWidth, uint32_t nHeight); //! call if drawing microprofilers
	bool ProfileIsDrawing();
	void ProfileToggleGraph(MicroProfileToken nToken);
	bool ProfileDrawGraph(Cmd* pCmd, uint32_t nScreenWidth, uint32_t nScreenHeight);
	void ProfileToggleDisplayMode(); //switch between off, bars, detailed
	void ProfileSetDisplayMode(int); //switch between off, bars, detailed
	void ProfileClearGraph();
	void updateProfileMousePosition(uint32_t nX, uint32_t nY, int nWheelDelta);
	void updateProfileMouseButton(uint32_t nLeft, uint32_t nRight);
	void ProfileModKey(uint32_t nKeyState);
	void ProfileDrawLineVertical(Cmd* pCmd, int nX, int nTop, int nBottom, uint32_t nColor);
	void ProfileDrawLineHorizontal(Cmd* pCmd, int nLeft, int nRight, int nY, uint32_t nColor);
	void ProfileLoadPreset(const char* pSuffix);
	void ProfileSavePreset(const char* pSuffix);
	void ProfileDumpTimers();
	void ProfileInitUI();
	void ProfileCustomGroupToggle(const char* pCustomName);
	void ProfileCustomGroupEnable(const char* pCustomName);
	void ProfileCustomGroupEnable(uint32_t nIndex);
	void ProfileCustomGroupDisable();
	void ProfileCustomGroup(const char* pCustomName, uint32_t nMaxTimers, uint32_t nAggregateFlip, float fReferenceTime, uint32_t nFlags);
	void ProfileCustomGroupAddTimer(const char* pCustomName, const char* pGroup, const char* pTimer);
	void ProfileStringArrayClear(MicroProfileStringArray* pArray);
	void ProfileStringArrayAddLiteral(MicroProfileStringArray* pArray, const char* pLiteral);
	MICROPROFILE_FORMAT(3, 4) void ProfileStringArrayFormat(MicroProfileStringArray* pArray, const char* fmt, ...);
	void ProfileStringArrayCopy(MicroProfileStringArray* pDest, MicroProfileStringArray* pSrc);
	void ProfileFloatWindowSize(const char** ppStrings, uint32_t nNumStrings, uint32_t* pColors, uint32_t& nWidth, uint32_t& nHeight, uint32_t* pStringLengths = 0);
	void ProfileDrawFloatWindow(Cmd* pCmd, uint32_t nX, uint32_t nY, const char** ppStrings, uint32_t nNumStrings, uint32_t nColor, uint32_t* pColors = 0);
	void ProfileDrawTextBackground(Cmd* pCmd, uint32_t nX, uint32_t nY, uint32_t nColor, uint32_t nBgColor, const char* pString, uint32_t nStrLen);
	void ProfileToolTipMeta(MicroProfileStringArray* pToolTip);
	void ProfileToolTipLabel(MicroProfileStringArray* pToolTip);
	void ProfileDrawFloatTooltip(Cmd* pCmd, uint32_t nX, uint32_t nY, uint32_t nToken, uint64_t nTime);
	int64_t ProfileGetGpuTickSync(int64_t nTickCpu, int64_t nTickGpu);
	void ProfileZoomTo(int64_t nTickStart, int64_t nTickEnd, MicroProfileTokenType eToken);
	void ProfileCenter(int64_t nTickCenter);
	void ProfileDrawDetailedContextSwitchBars(Cmd* pCmd, uint32_t nY, uint32_t nThreadId, uint32_t nContextSwitchStart, uint32_t nContextSwitchEnd, int64_t nBaseTicks, uint32_t nBaseY);
	void ProfileWriteThreadHeader(Cmd* pCmd, uint32_t nY, MicroProfileThreadIdType ThreadId, const char* pNamedThread, const char* pThreadModule);
	uint32_t ProfileWriteProcessHeader(Cmd* pCmd, uint32_t nY, uint32_t nProcessId);
	void ProfileGetFrameRange(int64_t nTicks, int64_t nTicksEnd, int32_t nLogIndex, uint32_t* nFrameBegin, uint32_t* nFrameEnd);
	void ProfileDrawDetailedBars(Cmd* pCmd, uint32_t nWidth, uint32_t nHeight, int nBaseY, int nSelectedFrame);
	void ProfileDrawDetailedFrameHistory(Cmd* pCmd, uint32_t nWidth, uint32_t nHeight, uint32_t nBaseY, uint32_t nSelectedFrame);
	void ProfileDrawDetailedView(Cmd* pCmd, uint32_t nWidth, uint32_t nHeight, bool bDrawBars);
	void ProfileDrawHeader(Cmd* pCmd, int32_t nX, uint32_t nWidth, const char* pName);
	void ProfileLoopActiveGroupsDraw(Cmd* pCmd, int32_t nX, int32_t nY, MicroProfileLoopGroupCallback CB, void* pData);
	void ProfileCalcTimers(float* pTimers, float* pAverage, float* pMax, float* pMin, float* pCallAverage, float* pExclusive, float* pAverageExclusive, float* pMaxExclusive, uint64_t nGroup, uint32_t nSize);
	uint32_t ProfileDrawBarArray(Cmd* pCmd, int32_t nX, int32_t nY, float* pTimers, const char* pName, uint32_t nTotalHeight, float* pTimers2 = NULL);
	uint32_t ProfileDrawBarCallCount(Cmd* pCmd, int32_t nX, int32_t nY, const char* pName);
	uint32_t ProfileDrawBarMetaAverage(Cmd* pCmd, int32_t nX, int32_t nY, uint64_t* pCounters, const char* pName, uint32_t nTotalHeight);
	uint32_t ProfileDrawBarMetaCount(Cmd* pCmd, int32_t nX, int32_t nY, uint64_t* pCounters, const char* pName, uint32_t nTotalHeight);
	uint32_t ProfileDrawBarLegend(Cmd* pCmd, int32_t nX, int32_t nY, uint32_t nTotalHeight, uint32_t nMaxWidth);
	uint32_t ProfileDrawCounterRecursive(Cmd* pCmd, uint32_t nIndex, uint32_t nY, uint32_t nOffset, uint32_t nTimerWidth);
	void ProfileDrawCounterView(Cmd* pCmd, uint32_t nScreenWidth, uint32_t nScreenHeight);
	void ProfileDrawBarView(Cmd* pCmd, uint32_t nScreenWidth, uint32_t nScreenHeight);
	void ProfileDrawMenu(Cmd* pCmd, uint32_t nWidth, uint32_t nHeight);
	void ProfileMoveGraph();
	void ProfileDrawCustom(Cmd* pCmd, uint32_t nWidth, uint32_t nHeight);
	uint32_t ProfileCustomGroupFind(const char* pCustomName);
	uint32_t ProfileCustomGroup(const char* pCustomName);
	
	static void ProfileDrawText(Cmd* pCmd, int nX, int nY, uint32_t nColor, const char* pText, uint32_t nNumCharacters)
	{
		TextDrawDesc frameTimeDrawDesc = TextDrawDesc(0, 0xff00ffff, 15, 0);
		const TextDrawDesc* pDesc = &frameTimeDrawDesc;
		gUIApp_MP->pImpl->pFontStash->drawText(
			pCmd, pText, (float)nX, (float)nY, pDesc->mFontID, pDesc->mFontColor, pDesc->mFontSize,
			pDesc->mFontSpacing, pDesc->mFontBlur);
	}

	static void ProfileDrawBox(Cmd* pCmd, int nX, int nY, int nX1, int nY1, uint32_t nColor, MicroProfileBoxType type = MicroProfileBoxTypeFlat)
	{
		//TODO
		return;
	}
	static void ProfileDrawLine2D(Cmd* pCmd, uint32_t nVertices, float* pVertices, uint32_t nColor)
	{
		//TODO
		return;
	}

	static void ProfileDrawBarArrayCallback(Cmd* pCmd, uint32_t nTimer, uint32_t nIdx, uint32_t nX, uint32_t nY, void* pExtra)
	{
		const uint32_t nHeight = MICROPROFILE_TEXT_HEIGHT;
		const uint32_t nTextWidth = 6 * (1 + MICROPROFILE_TEXT_WIDTH);
		const float fWidth = (float)MICROPROFILE_BAR_WIDTH;

		float* pTimers = ((float**)pExtra)[0];
		float* pTimers2 = ((float**)pExtra)[1];
		MicroProfile& S = *MicroProfileGet();
		char sBuffer[SBUF_MAX];
		if (pTimers2 && pTimers2[nIdx] > 0.1f)
			snprintf(sBuffer, SBUF_MAX - 1, "%5.2f %3.1fx", pTimers[nIdx], pTimers[nIdx] / pTimers2[nIdx]);
		else
			snprintf(sBuffer, SBUF_MAX - 1, "%5.2f", pTimers[nIdx]);
		if (!pTimers2)
			ProfileDrawBox(pCmd, nX + nTextWidth, nY, (int)(nX + nTextWidth + fWidth * pTimers[nIdx + 1]), nY + nHeight, UI.nOpacityForeground | S.TimerInfo[nTimer].nColor, MicroProfileBoxTypeBar);
		ProfileDrawText(pCmd, nX, nY, (uint32_t)-1, sBuffer, (uint32_t)strlen(sBuffer));
	}

	static void ProfileDrawBarCallCountCallback(Cmd* pCmd, uint32_t nTimer, uint32_t nIdx, uint32_t nX, uint32_t nY, void* pExtra)
	{
		(void)nIdx;
		(void)pExtra;

		MicroProfile& S = *MicroProfileGet();
		char sBuffer[SBUF_MAX];
		snprintf(sBuffer, SBUF_MAX - 1, "%5d", S.Frame[nTimer].nCount);//fix
		uint32_t nLen = (uint32_t)strlen(sBuffer);
		ProfileDrawText(pCmd, nX, nY, (uint32_t)-1, sBuffer, nLen);
	}

	static void ProfileDrawBarMetaAverageCallback(Cmd* pCmd, uint32_t nTimer, uint32_t nIdx, uint32_t nX, uint32_t nY, void* pExtra)
	{
		(void)nIdx;

		MicroProfileMetaAverageArgs* pArgs = (MicroProfileMetaAverageArgs*)pExtra;
		uint64_t* pCounters = pArgs->pCounters;
		float fRcpFrames = pArgs->fRcpFrames;
		char sBuffer[SBUF_MAX];
		snprintf(sBuffer, SBUF_MAX - 1, "%5.2f", pCounters[nTimer] * fRcpFrames);
		uint32_t nLen = (uint32_t)strlen(sBuffer);
		ProfileDrawText(pCmd, nX - nLen * (MICROPROFILE_TEXT_WIDTH + 1), nY, (uint32_t)-1, sBuffer, nLen);
	}

	static void ProfileDrawBarMetaCountCallback(Cmd* pCmd, uint32_t nTimer, uint32_t nIdx, uint32_t nX, uint32_t nY, void* pExtra)
	{
		(void)nIdx;

		uint64_t* pCounters = (uint64_t*)pExtra;
		char sBuffer[SBUF_MAX];
		int nLen = snprintf(sBuffer, SBUF_MAX - 1, "%5lld", (long long)pCounters[nTimer]);
		nLen = (int)strlen(sBuffer);
		ProfileDrawText(pCmd, nX - nLen * (MICROPROFILE_TEXT_WIDTH + 1), nY, (uint32_t)-1, sBuffer, nLen);
	}
	static void ProfileDrawTextRight(Cmd* pCmd, uint32_t nX, uint32_t nY, uint32_t nColor, const char* pStr, uint32_t nStrLen)
	{
		ProfileDrawText(pCmd, nX - nStrLen * (MICROPROFILE_TEXT_WIDTH + 1), nY, nColor, pStr, nStrLen);
	}
	static void ProfileDrawBarLegendCallback(Cmd* pCmd, uint32_t nTimer, uint32_t nIdx, uint32_t nX, uint32_t nY, void* pExtra)
	{
		(void)nIdx;
		(void)pExtra;

		MicroProfile& S = *MicroProfileGet();
		if (S.TimerInfo[nTimer].bGraph)
		{
			ProfileDrawText(pCmd, nX, nY, S.TimerInfo[nTimer].nColor, ">", 1);
		}
		ProfileDrawTextRight(pCmd, nX, nY, S.TimerInfo[nTimer].nColor, S.TimerInfo[nTimer].pName, (uint32_t)strlen(S.TimerInfo[nTimer].pName));
		if (UI.nMouseY >= nY && UI.nMouseY < nY + MICROPROFILE_TEXT_HEIGHT + 1)
		{
			UI.nHoverToken = nTimer;
			UI.nHoverTime = 0;
		}
	}


	static const char* ProfileUIMenuMode(int nIndex, bool* bSelected)
	{
		MicroProfile& S = *MicroProfileGet();
		switch (nIndex)
		{
		case 0:
			*bSelected = S.nDisplay == MP_DRAW_DETAILED;
			return "Detailed";
		case 1:
			*bSelected = S.nDisplay == MP_DRAW_BARS;
			return "Timers";
		case 2:
			*bSelected = S.nDisplay == MP_DRAW_COUNTERS;
			return "Counters";
		case 3:
			*bSelected = S.nDisplay == MP_DRAW_FRAME;
			return "Frame";
		case 4:
			*bSelected = S.nDisplay == MP_DRAW_HIDDEN;
			return "Hidden";
		case 5:
			*bSelected = false;
			return "Off";
		case 6:
			*bSelected = false;
			return "------";
		case 7:
			*bSelected = S.nForceEnable != 0;
			return "Force Enable";

		default: return 0;
		}
	}

	static const char* ProfileUIMenuGroups(int nIndex, bool* bSelected)
	{
		MicroProfile& S = *MicroProfileGet();
		*bSelected = false;
		if (nIndex == 0)
		{
			*bSelected = S.nAllGroupsWanted != 0;
			return "[ALL]";
		}
		else
		{
			nIndex = nIndex - 1;
			if (nIndex < (int)UI.GroupMenuCount)
			{
				MicroProfileGroupMenuItem& Item = UI.GroupMenu[nIndex];
				static char buffer[MICROPROFILE_NAME_MAX_LEN + 32];
				if (Item.nIsCategory)
				{
					uint64_t nGroupMask = S.CategoryInfo[Item.nIndex].nGroupMask;
					*bSelected = nGroupMask == (nGroupMask & S.nActiveGroupWanted);
					snprintf(buffer, sizeof(buffer) - 1, "[%s]", Item.pName);
				}
				else
				{
					*bSelected = 0 != (S.nActiveGroupWanted & (1ll << Item.nIndex));
					snprintf(buffer, sizeof(buffer) - 1, "   %s", Item.pName);
				}
				return buffer;
			}
			return 0;
		}
	}

	static const char* ProfileUIMenuAggregate(int nIndex, bool* bSelected)
	{
		MicroProfile& S = *MicroProfileGet();
		int nNumPresets = (int)sizeof(g_MicroProfileAggregatePresets) / (int)sizeof(g_MicroProfileAggregatePresets[0]);
		if (nIndex < nNumPresets)
		{
			int val = g_MicroProfileAggregatePresets[nIndex];
			*bSelected = (int)S.nAggregateFlip == val;
			if (0 == val)
				return "Infinite";
			else
			{
				static char buf[128];
				snprintf(buf, sizeof(buf) - 1, "%7d", val);
				return buf;
			}
		}
		return 0;

	}

	static const char* ProfileUIMenuTimers(int nIndex, bool* bSelected)
	{
		MicroProfile& S = *MicroProfileGet();

		if (nIndex < 8)
		{
			static const char* kNames[] = { "Time", "Average", "Max", "Min", "Call Count", "Exclusive Timers", "Exclusive Average", "Exclusive Max" };

			*bSelected = 0 != (S.nBars & (1 << nIndex));
			return kNames[nIndex];
		}
		else if (nIndex == 8)
		{
			*bSelected = false;
			return "------";
		}
		else
		{
			int nMetaIndex = nIndex - 9;
			if (nMetaIndex < MICROPROFILE_META_MAX)
			{
				*bSelected = 0 != (S.nBars & (MP_DRAW_META_FIRST << nMetaIndex));
				return S.MetaCounters[nMetaIndex].pName;
			}
		}
		return 0;
	}

	static const char* ProfileUIMenuOptions(int nIndex, bool* bSelected)
	{
		MicroProfile& S = *MicroProfileGet();
		if (nIndex >= MICROPROFILE_OPTION_SIZE) return 0;
		switch (UI.Options[nIndex].nSubType)
		{
		case 0:
			*bSelected = S.fReferenceTime == g_MicroProfileReferenceTimePresets[UI.Options[nIndex].nIndex];
			break;
		case 1:
			*bSelected = UI.nOpacityBackground >> 24 == g_MicroProfileOpacityPresets[UI.Options[nIndex].nIndex];
			break;
		case 2:
			*bSelected = UI.nOpacityForeground >> 24 == g_MicroProfileOpacityPresets[UI.Options[nIndex].nIndex];
			break;
		case 3:
			*bSelected = UI.bShowSpikes;
			break;
#if MICROPROFILE_CONTEXT_SWITCH_TRACE
		case 4:
		{
			switch (UI.Options[nIndex].nIndex)
			{
			case 0:
				*bSelected = S.bContextSwitchAllThreads;
				break;
			case 1:
				*bSelected = S.bContextSwitchNoBars;
				break;
			}
		}
		break;
#endif
		}
		return UI.Options[nIndex].Text;
	}

	static const char* ProfileUIMenuPreset(int nIndex, bool* bSelected)
	{
		static char buf[128];
		*bSelected = false;
		int nNumPresets = sizeof(g_MicroProfilePresetNames) / sizeof(g_MicroProfilePresetNames[0]);
		int nIndexSave = nIndex - nNumPresets - 1;
		if (nIndex == nNumPresets)
			return "--";
		else if (nIndexSave >= 0 && nIndexSave < nNumPresets)
		{
			snprintf(buf, sizeof(buf) - 1, "Save '%s'", g_MicroProfilePresetNames[nIndexSave]);
			return buf;
		}
		else if (nIndex < nNumPresets)
		{
			snprintf(buf, sizeof(buf) - 1, "Load '%s'", g_MicroProfilePresetNames[nIndex]);
			return buf;
		}
		else
		{
			return 0;
		}
	}

	static const char* ProfileUIMenuCustom(int nIndex, bool* bSelected)
	{
		if ((uint32_t)-1 == UI.nCustomActive)
		{
			*bSelected = nIndex == 0;
		}
		else
		{
			*bSelected = nIndex - 2 == (int)UI.nCustomActive;
		}
		switch (nIndex)
		{
		case 0: return "Disable";
		case 1: return "--";
		default:
			nIndex -= 2;
			if (nIndex < (int)UI.nCustomCount)
			{
				return UI.Custom[nIndex].pName;
			}
			else
			{
				return 0;
			}
		}
	}

	static const char* ProfileUIMenuDump(int nIndex, bool* bSelected)
	{
		static char buf[128];
		*bSelected = false;

		if (nIndex < 5)
		{
			snprintf(buf, sizeof(buf) - 1, "%d frames", 32 << nIndex);
			return buf;
		}
		else
		{
			return 0;
		}
	}

	static void ProfileUIClickMode(int nIndex)
	{
		MicroProfile& S = *MicroProfileGet();
		switch (nIndex)
		{
		case 0:
			S.nDisplay = MP_DRAW_DETAILED;
			break;
		case 1:
			S.nDisplay = MP_DRAW_BARS;
			break;
		case 2:
			S.nDisplay = MP_DRAW_COUNTERS;
			break;
		case 3:
			S.nDisplay = MP_DRAW_FRAME;
			break;
		case 4:
			S.nDisplay = MP_DRAW_HIDDEN;
			break;
		case 5:
			S.nDisplay = 0;
			break;
		case 6:
			break;
		case 7:
			S.nForceEnable = !S.nForceEnable;
			break;
		}
	}

	static void ProfileUIClickGroups(int nIndex)
	{
		MicroProfile& S = *MicroProfileGet();
		if (nIndex == 0)
			S.nAllGroupsWanted = 1 - S.nAllGroupsWanted;
		else
		{
			nIndex -= 1;
			if (nIndex < (int)UI.GroupMenuCount)
			{
				MicroProfileGroupMenuItem& Item = UI.GroupMenu[nIndex];
				if (Item.nIsCategory)
				{
					uint64_t nGroupMask = S.CategoryInfo[Item.nIndex].nGroupMask;
					if (nGroupMask != (nGroupMask & S.nActiveGroupWanted))
					{
						S.nActiveGroupWanted |= nGroupMask;
					}
					else
					{
						S.nActiveGroupWanted &= ~nGroupMask;
					}
				}
				else
				{
					ASSERT(Item.nIndex < S.nGroupCount);
					S.nActiveGroupWanted ^= (1ll << Item.nIndex);
				}
			}
		}
	}

	static void ProfileUIClickAggregate(int nIndex)
	{
		MicroProfile& S = *MicroProfileGet();
		S.nAggregateFlip = g_MicroProfileAggregatePresets[nIndex];
		if (0 == S.nAggregateFlip)
		{
			S.nAggregateClear = 1;
		}
	}

	static void ProfileUIClickTimers(int nIndex)
	{
		MicroProfile& S = *MicroProfileGet();

		if (nIndex < 8)
		{
			S.nBars ^= (1 << nIndex);
		}
		else if (nIndex != 8)
		{
			int nMetaIndex = nIndex - 9;
			if (nMetaIndex < MICROPROFILE_META_MAX)
			{
				S.nBars ^= (MP_DRAW_META_FIRST << nMetaIndex);
			}
		}
	}

	static void ProfileUIClickOptions(int nIndex)
	{
		MicroProfile& S = *MicroProfileGet();
		switch (UI.Options[nIndex].nSubType)
		{
		case 0:
			S.fReferenceTime = g_MicroProfileReferenceTimePresets[UI.Options[nIndex].nIndex];
			S.fRcpReferenceTime = 1.f / S.fReferenceTime;
			break;
		case 1:
			UI.nOpacityBackground = g_MicroProfileOpacityPresets[UI.Options[nIndex].nIndex] << 24;
			break;
		case 2:
			UI.nOpacityForeground = g_MicroProfileOpacityPresets[UI.Options[nIndex].nIndex] << 24;
			break;
		case 3:
			UI.bShowSpikes = !UI.bShowSpikes;
			break;
#if MICROPROFILE_CONTEXT_SWITCH_TRACE
		case 4:
		{
			switch (UI.Options[nIndex].nIndex)
			{
			case 0:
				S.bContextSwitchAllThreads = !S.bContextSwitchAllThreads;
				break;
			case 1:
				S.bContextSwitchNoBars = !S.bContextSwitchNoBars;
				break;

			}
		}
		break;
#endif
		}
	}

	static void ProfileUIClickPreset(int nIndex)
	{
		int nNumPresets = sizeof(g_MicroProfilePresetNames) / sizeof(g_MicroProfilePresetNames[0]);
		int nIndexSave = nIndex - nNumPresets - 1;
		if (nIndexSave >= 0 && nIndexSave < nNumPresets)
		{
			gUIApp_MP->ProfileSavePreset(g_MicroProfilePresetNames[nIndexSave]);
		}
		else if (nIndex >= 0 && nIndex < nNumPresets)
		{
			gUIApp_MP->ProfileLoadPreset(g_MicroProfilePresetNames[nIndex]);
		}
	}

	static void ProfileUIClickCustom(int nIndex)
	{
		if (nIndex == 0)
		{
			gUIApp_MP->ProfileCustomGroupDisable();
		}
		else
		{
			gUIApp_MP->ProfileCustomGroupEnable(nIndex - 2);
		}
	}

	static void ProfileUIClickDump(int nIndex)
	{
		// Need multi platform support
	}
#endif
	int32_t mFontAtlasSize = 0;
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
	Renderer*         pRenderer;
	Shader*           pShader;
	RootSignature*    pRootSignature;
	DescriptorBinder* pDescriptorBinder;
	Pipeline*         pPipeline;
	Texture*          pTexture;
	Sampler*          pSampler;
	BlendState*       pBlendAlpha;
	DepthState*       pDepthState;
	RasterizerState*  pRasterizerState;
	GPURingBuffer*    pMeshRingBuffer;
	vec2              mRenderSize;
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

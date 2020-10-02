/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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
#include "../../Common_3/OS/Interfaces/ILog.h"
#include "../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../Common_3/ThirdParty/OpenSource/EASTL/string.h"
#include "../Text/Fontstash.h"

typedef void(*WidgetCallback)();

struct Renderer;
struct Texture;
struct Shader;
struct RootSignature;
struct DescriptorSet;
struct Pipeline;
struct Sampler;
struct Buffer;
struct Texture;
struct PipelineCache;

class IWidget
{
public:
	IWidget(const eastl::string& _label) :
		pOnHover(nullptr),
		pOnActive(nullptr),
		pOnFocus(nullptr),
		pOnEdited(nullptr),
		pOnDeactivated(nullptr),
		pOnDeactivatedAfterEdit(nullptr),
		mLabel(_label),
		mDeferred(false),
		mHovered(false),
		mActive(false),
		mFocused(false),
		mEdited(false),
		mDeactivated(false),
		mDeactivatedAfterEdit(false)
	{
	}
	virtual ~IWidget() {}
	virtual IWidget* Clone() const = 0;
	virtual void     Draw() = 0;

	void ProcessCallbacks(bool deferred = false);

	// Common callbacks that can be used by the clients
	WidgetCallback pOnHover;          // Widget is hovered, usable, and not blocked by anything.
	WidgetCallback pOnActive;         // Widget is currently active (ex. button being held)
	WidgetCallback pOnFocus;          // Widget is currently focused (for keyboard/gamepad nav)
	WidgetCallback pOnEdited;         // Widget just changed its underlying value or was pressed.
	WidgetCallback pOnDeactivated;    // Widget was just made inactive from an active state.  This is useful for undo/redo patterns.
	WidgetCallback
		pOnDeactivatedAfterEdit;    // Widget was just made inactive from an active state and changed its underlying value.  This is useful for undo/redo patterns.

	eastl::string mLabel;

	// Set this to process deferred callbacks that may cause global program state changes.
	bool mDeferred;

	bool mHovered;
	bool mActive;
	bool mFocused;
	bool mEdited;
	bool mDeactivated;
	bool mDeactivatedAfterEdit;

protected:
	inline void CloneBase(IWidget* other) const
	{
		other->pOnHover = pOnHover;
		other->pOnActive = pOnActive;
		other->pOnFocus = pOnFocus;
		other->pOnEdited = pOnEdited;
		other->pOnDeactivated = pOnDeactivated;
		other->pOnDeactivatedAfterEdit = pOnDeactivatedAfterEdit;

		other->mDeferred = mDeferred;
	}

private:
	// Disable copy
	IWidget(IWidget const&);
	IWidget& operator=(IWidget const&);
};

class CollapsingHeaderWidget : public IWidget
{
public:
	CollapsingHeaderWidget(const eastl::string& _label, bool defaultOpen = false, bool collapsed = true, bool headerIsVisible = true) :
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
		decltype(mGroupedWidgets)::iterator it = eastl::find(mGroupedWidgets.begin(), mGroupedWidgets.end(), pWidget);
		if (it != mGroupedWidgets.end())
		{
			IWidget* pWidget = *it;
			mGroupedWidgets.erase(it);
			pWidget->~IWidget();
			tf_free(pWidget);
		}
	}

	void RemoveAllSubWidgets()
	{
		for (size_t i = 0; i < mGroupedWidgets.size(); ++i)
		{
			mGroupedWidgets[i]->~IWidget();
			tf_free(mGroupedWidgets[i]);
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
	eastl::vector<IWidget*> mGroupedWidgets;
	bool                      mCollapsed;
	bool                      mPreviousCollapsed;
	bool                      mDefaultOpen;
	bool					  mHeaderIsVisible;
};

class DebugTexturesWidget : public IWidget
{
public:
	DebugTexturesWidget(const eastl::string& _label) :
		IWidget(_label),
		mTextureDisplaySize(float2(512.f, 512.f)) {}

	IWidget* Clone() const;
	void     Draw();

	void SetTextures(eastl::vector<Texture*> const& textures, float2 const& displaySize)
	{
		mTextures = textures;
		mTextureDisplaySize = displaySize;
	}

private:
	eastl::vector<Texture*> mTextures;
	float2 mTextureDisplaySize;
};

class LabelWidget : public IWidget
{
public:
	LabelWidget(const eastl::string& _label) : IWidget(_label) {}

	IWidget* Clone() const;
	void     Draw();
};

class ColorLabelWidget : public IWidget
{
public:
	ColorLabelWidget(const eastl::string& _label, const float4& _color) :
		IWidget(_label),
		mColor(_color) {}

	IWidget* Clone() const;
	void     Draw();

protected:
	float4 mColor;
};

class HorizontalSpaceWidget : public IWidget
{
public:
	HorizontalSpaceWidget() : IWidget("") {}

	IWidget* Clone() const;
	void     Draw();
};

class SeparatorWidget : public IWidget
{
public:
	SeparatorWidget() : IWidget("") {}

	IWidget* Clone() const;
	void     Draw();
};

class VerticalSeparatorWidget : public IWidget
{
public:
	VerticalSeparatorWidget(const uint32_t& _lines) : IWidget(""), mLineCount(_lines) {}

	IWidget* Clone() const;
	void     Draw();

protected:
	uint32_t mLineCount;
};

class ButtonWidget : public IWidget
{
public:
	ButtonWidget(const eastl::string& _label) : IWidget(_label) {}

	IWidget* Clone() const;
	void     Draw();
};

class SliderFloatWidget : public IWidget
{
public:
	SliderFloatWidget(
		const eastl::string& _label, float* _data, float _min, float _max, float _step = 0.01f, const eastl::string& _format = "%.3f") :
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
	eastl::string mFormat;
	float*          pData;
	float           mMin;
	float           mMax;
	float           mStep;
};

class SliderFloat2Widget : public IWidget
{
public:
	SliderFloat2Widget(
		const eastl::string& _label, float2* _data, const float2& _min, const float2& _max, const float2& _step = float2(0.01f, 0.01f),
		const eastl::string& _format = "%.3f") :
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
	eastl::string mFormat;
	float2*         pData;
	float2          mMin;
	float2          mMax;
	float2          mStep;
};

class SliderFloat3Widget : public IWidget
{
public:
	SliderFloat3Widget(
		const eastl::string& _label, float3* _data, const float3& _min, const float3& _max,
		const float3& _step = float3(0.01f, 0.01f, 0.01f), const eastl::string& _format = "%.3f") :
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
	eastl::string mFormat;
	float3*         pData;
	float3          mMin;
	float3          mMax;
	float3          mStep;
};

class SliderFloat4Widget : public IWidget
{
public:
	SliderFloat4Widget(
		const eastl::string& _label, float4* _data, const float4& _min, const float4& _max,
		const float4& _step = float4(0.01f, 0.01f, 0.01f, 0.01f), const eastl::string& _format = "%.3f") :
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
	eastl::string mFormat;
	float4*         pData;
	float4          mMin;
	float4          mMax;
	float4          mStep;
};

class SliderIntWidget : public IWidget
{
public:
	SliderIntWidget(
		const eastl::string& _label, int32_t* _data, int32_t _min, int32_t _max, int32_t _step = 1,
		const eastl::string& _format = "%d") :
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
	eastl::string mFormat;
	int32_t*        pData;
	int32_t         mMin;
	int32_t         mMax;
	int32_t         mStep;
};

class SliderUintWidget : public IWidget
{
public:
	SliderUintWidget(
		const eastl::string& _label, uint32_t* _data, uint32_t _min, uint32_t _max, uint32_t _step = 1,
		const eastl::string& _format = "%d") :
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
	eastl::string mFormat;
	uint32_t*       pData;
	uint32_t        mMin;
	uint32_t        mMax;
	uint32_t        mStep;
};

class RadioButtonWidget : public IWidget
{
public:
	RadioButtonWidget(const eastl::string& _label, int32_t* _data, const int32_t _radioId) :
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

class CheckboxWidget : public IWidget
{
public:
	CheckboxWidget(const eastl::string& _label, bool* _data) : IWidget(_label), pData(_data) {}
	IWidget* Clone() const;
	void     Draw();

protected:
	bool* pData;
};

class OneLineCheckboxWidget : public IWidget
{
public:
	OneLineCheckboxWidget(const eastl::string& _label, bool* _data, const uint32_t& _color) : IWidget(_label), pData(_data), mColor(_color) {}
	IWidget* Clone() const;
	void     Draw();

protected:
	bool* pData;
	uint32_t mColor;
};

class CursorLocationWidget : public IWidget
{
public:
	CursorLocationWidget(const eastl::string& _label, const float2& _location) : IWidget(_label), mLocation(_location) {}
	IWidget* Clone() const;
	void     Draw();

protected:
	float2 mLocation;
};

class DropdownWidget : public IWidget
{
public:
	DropdownWidget(const eastl::string& _label, uint32_t* _data, const char** _names, const uint32_t* _values, uint32_t count) :
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
	eastl::vector<uint32_t>        mValues;
	eastl::vector<eastl::string> mNames;
};

class ColumnWidget : public IWidget
{
public:
	ColumnWidget(const eastl::string& _label, const eastl::vector<IWidget*>& _perColWidgets) : IWidget(_label)
	{
		mNumColumns = (uint32_t)_perColWidgets.size();
		for (uint32_t i = 0; i < _perColWidgets.size(); ++i)
		{
			mPerColumnWidgets.push_back(_perColWidgets[i]);
		}
	}

	IWidget* Clone() const;
	void     Draw();

protected:
	eastl::vector<IWidget*> mPerColumnWidgets;
	uint32_t mNumColumns;
};



class ProgressBarWidget : public IWidget
{
public:
	ProgressBarWidget(const eastl::string& _label, size_t* _data, size_t const _maxProgress) :
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

class ColorSliderWidget : public IWidget
{
public:
	ColorSliderWidget(const eastl::string& _label, uint32_t* _data) : IWidget(_label), pData(_data) {}

	IWidget* Clone() const;
	void     Draw();

protected:
	uint32_t* pData;
};

class HistogramWidget : public IWidget
{
public:
	HistogramWidget(const eastl::string& _label, float* _values, uint32_t _valuesCount, float* _minScale, float* _maxScale, float2 _graphScale, eastl::string* _title) :
		IWidget(_label),
		pValues(_values),
		mCount(_valuesCount),
		mMinScale(_minScale),
		mMaxScale(_maxScale),
		mHistogramSize(_graphScale),
		mHistogramTitle(_title)
	{}

	IWidget* Clone() const;
	void     Draw();

protected:
	float* pValues;
	uint32_t mCount;
	float* mMinScale;
	float* mMaxScale;
	float2 mHistogramSize;
	eastl::string* mHistogramTitle;
};

class PlotLinesWidget : public IWidget
{
public:
	PlotLinesWidget(const eastl::string& _label, float* _values, uint32_t _valueCount, float* _scaleMin, float* _scaleMax, float2* _plotScale, eastl::string* _title)
		: IWidget(_label),
		mValues(_values),
		mNumValues(_valueCount),
		mScaleMin(_scaleMin),
		mScaleMax(_scaleMax),
		mPlotScale(_plotScale),
		mTitle(_title)
	{}
	IWidget* Clone() const;
	void     Draw();
protected:
	float* mValues;
	uint32_t mNumValues;
	float* mScaleMin;
	float* mScaleMax;
	float2* mPlotScale;
	eastl::string* mTitle;
};

class ColorPickerWidget : public IWidget
{
public:
	ColorPickerWidget(const eastl::string& _label, uint32_t* _data) : IWidget(_label), pData(_data) {}

	IWidget* Clone() const;
	void     Draw();

protected:
	uint32_t* pData;
};

class TextboxWidget : public IWidget
{
public:
	TextboxWidget(const eastl::string& _label, char* _data, uint32_t const _length, bool const _autoSelectAll = true) :
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

class DynamicTextWidget : public IWidget
{
public:
	DynamicTextWidget(const eastl::string& _label, char* _data, uint32_t const _length, float4* _color) :
		IWidget(_label),
		pData(_data),
		mLength(_length),
		pColor(_color)
	{
	}

	IWidget* Clone() const;
	void     Draw();

protected:
	char*    pData;
	uint32_t mLength;
	float4*  pColor;
};

class FilledRectWidget : public IWidget
{
public:
	FilledRectWidget(const eastl::string& _label, const float2& _pos, const float2& _scale, const uint32_t& _colorHex) :
		IWidget(_label),
		mPos(_pos),
		mScale(_scale),
		mColor(_colorHex)
	{
	}

	IWidget* Clone() const;
	void     Draw();

protected:
	float2 mPos;
	float2 mScale;
	uint32_t mColor;
};

class DrawTextWidget : public IWidget
{
public:
	DrawTextWidget(const eastl::string& _label, const float2& _pos, const uint32_t& _colorHex) :
		IWidget(_label),
		mPos(_pos),
		mColor(_colorHex)
	{
	}

	IWidget* Clone() const;
	void     Draw();

protected:
	float2 mPos;
	uint32_t mColor;
};

class DrawTooltipWidget : public IWidget
{
public:
	DrawTooltipWidget(const eastl::string& _label, bool* _showTooltip, char* _text) :
		IWidget(_label),
		mShowTooltip(_showTooltip),
		mText(_text)
	{}

	IWidget* Clone() const;
	void     Draw();

protected:
	bool* mShowTooltip;
	char* mText;
};

class DrawLineWidget : public IWidget
{
public:
	DrawLineWidget(const eastl::string& _label, const float2& _pos1, const float2& _pos2, const uint32_t& _colorHex, const bool& _addItem) :
		IWidget(_label),
		mPos1(_pos1),
		mPos2(_pos2),
		mColor(_colorHex),
		mAddItem(_addItem)
	{
	}

	IWidget* Clone() const;
	void     Draw();

protected:
	float2 mPos1;
	float2 mPos2;
	uint32_t mColor;
	bool mAddItem;
};

class DrawCurveWidget : public IWidget
{
public:
	DrawCurveWidget(const eastl::string& _label, float2* _positions, uint32_t _numPoints, float _thickness, const uint32_t& _colorHex) :
		IWidget(_label),
		mPos(_positions),
		mNumPoints(_numPoints),
		mThickness(_thickness),
		mColor(_colorHex)
	{
	}

	IWidget* Clone() const;
	void     Draw();

protected:
	float2* mPos;
	uint32_t mNumPoints;
	float mThickness;
	uint32_t mColor;
};


typedef struct GuiDesc
{
	GuiDesc(
		const vec2& startPos = { 0.0f, 150.0f }, const vec2& startSize = { 600.0f, 550.0f },
		const TextDrawDesc& textDrawDesc = { 0, 0xffffffff, 16 }) :
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
	1 << 15,    // No focusing toward this window with gamepad/keyboard navigation (e.g. skipped by CTRL+TAB)
	GUI_COMPONENT_FLAGS_START_COLLAPSED = 1 << 16
};

class GuiComponent
{
public:
	IWidget* AddWidget(const IWidget& widget, bool clone = true);
	void     RemoveWidget(IWidget* pWidget);
	void     RemoveAllWidgets();

	eastl::vector<IWidget*>        mWidgets;
	eastl::vector<bool>            mWidgetsClone;
	// Contextual menus when right clicking the title bar
	eastl::vector<eastl::string>   mContextualMenuLabels;
	eastl::vector<WidgetCallback>  mContextualMenuCallbacks;
	float4                         mInitialWindowRect;
	float4                         mCurrentWindowRect;
	eastl::string                  mTitle;
	uintptr_t                      pFont;
	float                          mAlpha;
	// defaults to GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE
	// on mobile, GUI_COMPONENT_FLAGS_START_COLLAPSED is also set
	int32_t                        mFlags;

	bool                           mActive;
	// UI Component settings that can be modified at runtime by the client.
	bool                           mHasCloseButton;
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
			tf_free(mDynamicProperties[i]);
		}

		mDynamicProperties.set_capacity(0);
	}

private:
	eastl::vector<IWidget*> mDynamicProperties;
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

	virtual ~GUIDriver() {}

	virtual bool init(Renderer* pRenderer, uint32_t const maxDynamicUIUpdatesPerBatch) = 0;
	virtual void exit() = 0;

	virtual bool load(RenderTarget** pRts, uint32_t count, PipelineCache* pCache) = 0;
	virtual void unload() = 0;

	// For GUI with custom shaders not necessary in a normal application
	virtual void setCustomShader(Shader* pShader) = 0;

	virtual bool addFont(void* pFontBuffer, uint32_t fontBufferSize, void* pFontGlyphRanges, float fontSize, uintptr_t* pFont) = 0;

	virtual void* getContext() = 0;

	virtual bool update(GUIUpdate* update) = 0;

	virtual void draw(Cmd* q) = 0;

	virtual bool     isFocused() = 0;
	virtual bool     onText(const wchar_t* pText) = 0;
	virtual bool     onButton(uint32_t button, bool press, const float2* vec) = 0;
	virtual uint8_t  wantTextInput() const = 0;

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

struct UIAppImpl
{
	Renderer*  pRenderer;
	Fontstash* pFontStash;

	eastl::vector<GuiComponent*> mComponents;

	eastl::vector<GuiComponent*> mComponentsToUpdate;
	bool                           mUpdated;
};
class UIApp : public IMiddleware
{
public:
	UIApp(int32_t const fontAtlasSize = 0, uint32_t const maxDynamicUIUpdatesPerBatch = 20u, uint32_t const fontStashRingSizeBytes = 1024 * 1024);

	bool Init(Renderer* renderer, PipelineCache* pCache = NULL);
	void Exit();

	bool Load(RenderTarget** rts, uint32_t count = 1);
	void Unload();

	void Update(float deltaTime);
	void Draw(Cmd* cmd);

	uint          LoadFont(const char* pFontPath);
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

	bool    OnText(const wchar_t* pText) { return pDriver->onText(pText); }
	bool    OnButton(uint32_t button, bool press, const float2* vec) { return pDriver->onButton(button, press, vec); }
	uint8_t WantTextInput() { return pDriver->wantTextInput(); }
	bool    IsFocused() { return pDriver->isFocused(); }
	/************************************************************************/
	// Data
	/************************************************************************/
	class GUIDriver*  pDriver;
	struct UIAppImpl* pImpl;
	Shader*           pCustomShader = NULL;
	PipelineCache*    pPipelineCache = NULL;

	// Following var is useful for seeing UI capabilities and tweaking style settings.
	// Will only take effect if at least one GUI Component is active.
	bool mShowDemoUiWindow;

private:
	float   mWidth;
	float   mHeight;
	int32_t  mFontAtlasSize = 0;
	uint32_t mMaxDynamicUIUpdatesPerBatch = 20;
	uint32_t mFontstashRingSizeBytes = 0;
};

class VirtualJoystickUI
{
public:
	VirtualJoystickUI(float insideRadius = 100.0f, float outsideRadius = 200.0f)
#if defined(TARGET_IOS) || defined(__ANDROID__)
		: mInsideRadius(insideRadius), mOutsideRadius(outsideRadius)
#endif
	{}

	// Init resources
	bool Init(Renderer* pRenderer, const char* pJoystickTexture);
	void Exit();
	bool Load(RenderTarget* pScreenRT);
	void Unload();
	void Update(float dt);
	void Draw(Cmd* pCmd, const float4& color);
	bool OnMove(uint32_t id, bool press, const float2* vec);

private:
#if defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)
	Renderer*         pRenderer;
	Shader*           pShader;
	RootSignature*    pRootSignature;
	DescriptorSet*    pDescriptorSet;
	Pipeline*         pPipeline;
	Texture*          pTexture;
	Sampler*          pSampler;
	Buffer*           pMeshBuffer;
	float2            mRenderSize;
	//input related
	float             mInsideRadius;
	float             mOutsideRadius;

	struct StickInput
	{
		bool     mPressed;
		float2   mStartPos;
		float2   mCurrPos;
	};
	// Left -> Index 0
	// Right -> Index 1
	StickInput       mSticks[2];
#endif
};

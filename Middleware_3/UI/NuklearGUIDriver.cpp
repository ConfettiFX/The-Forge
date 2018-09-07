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

#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION

#include "../../Middleware_3/Text/Fontstash.h"

#include "AppUI.h"
#include "UIControl.h"
#include "UIShaders.h"

#include "../../Common_3/ThirdParty/OpenSource/NuklearUI/nuklear.h"
#include "../../Common_3/OS/Interfaces/IOperatingSystem.h"
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Core/RingBuffer.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/ResourceLoader.h"

#include "../Input/InputSystem.h"
#include "../Input/InputMappings.h"

#include "../../Common_3/OS/Interfaces/IMemoryManager.h" //NOTE: this should be the last include in a .cpp

class NuklearGUIDriver : public GUIDriver
{
public:
	bool init(Renderer* pRenderer);
	void exit();

	bool load(Fontstash* fontID, float fontSize, Texture* cursorTexture = 0, float uiwidth = 600, float uiheight = 400);
	void unload();

	void* getContext();

	void draw(Cmd* q, float deltaTime,
		const char* pTitle,
		float x, float y, float z, float w,
		class UIProperty* pControls, uint numControls);

	bool onInput(const ButtonData* data);

	// NuklearUI doesn't support touch events.
	void onTouch(const struct TouchEventData* data) {}

	// NuklearUI doesn't support touch events.
	void onTouchMove(const struct TouchEventData* data) {}

	static float font_get_width(nk_handle handle, float h, const char *text, int len);
	static void changedProperty(UIProperty* pControl);
private:
	void draw_control(UIProperty* pControls, uint numControls, uint idxCurrControl, float w);

	Fontstash* pFontstash;
	int fontID;

	float width;
	float height;

	ButtonData inputData[2048];
	int inputDataCount;

	float fontCalibrationOffset;

	struct Font
	{
		NuklearGUIDriver* driver;
		struct nk_user_font userFont;
		int fontID;
	};
	Font fontStack[128];
	UIProperty* pControls;
	uint32_t numControls;
	int32_t selectedID;
	int32_t minID;
	int32_t numberOfElements;
	int32_t scrollOffset;
	float4 mCurrentWindowRect;

	int currentFontStackPos;
	bool wantKeyboardInput;
	bool needKeyboardInputNextFrame;
	bool escWasPressed;

	/// RENDERING RESOURCES
	typedef tinystl::unordered_map<uint64_t, tinystl::vector<Pipeline*>> PipelineMap;
	Renderer*					pRenderer;
	Shader*						pShaderPlain;
	Shader*						pShaderPlainColor;
	Shader*						pShaderTextured;
	RootSignature*				pRootSignaturePlain;
	RootSignature*				pRootSignatureTextured;
	PipelineMap					mPipelinesPlain;
	PipelineMap					mPipelinesPlainColor;
	PipelineMap					mPipelinesTextured;
	MeshRingBuffer*				pPlainMeshRingBuffer;
	MeshRingBuffer*				pPlainColorMeshRingBuffer;
	MeshRingBuffer*				pTexturedMeshRingBuffer;
	BlendState*					pBlendAlpha;
	DepthState*					pDepthState;
	RasterizerState*			pRasterizerState;
	Sampler*					pDefaultSampler;
	VertexLayout				mVertexLayoutPlain = {};
	VertexLayout				mVertexLayoutPlainColor = {};
	VertexLayout				mVertexLayoutTextured = {};
	//------------------------------------------
	// INTERFACE
	//------------------------------------------
public:

	// initializes rendering resources
	//
	void initRendererResources(Renderer* renderer)
	{
		pRenderer = renderer;
		SamplerDesc samplerDesc =
		{
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_NEAREST,
			ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE
		};
		addSampler(pRenderer, &samplerDesc, &pDefaultSampler);

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

#if defined(METAL)
		tinystl::string plainShaderFile = "builtin_plain";
		tinystl::string plainColorShaderFile = "builtin_plain_color";
		tinystl::string texturedShaderFile = "builtin_plain";
		tinystl::string plainShader = mtl_builtin_plain;
		tinystl::string plainColorShader = mtl_builtin_plain_color;
		tinystl::string texturedShader = mtl_builtin_plain;
		ShaderDesc plainShaderDesc = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { plainShaderFile, plainShader, "VSMain" },{ plainShaderFile, plainShader, "PSMain" } };
		ShaderDesc plainColorShaderDesc = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { plainColorShaderFile, plainColorShader, "VSMain" }, { plainColorShaderFile, plainColorShader, "PSMain" } };

		ShaderDesc texturedShaderDesc = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { texturedShaderFile, texturedShader, "VSMain" }, { texturedShaderFile, texturedShader, "PSMain" } };
		addShader(pRenderer, &plainShaderDesc, &pShaderPlain);
		addShader(pRenderer, &plainColorShaderDesc, &pShaderPlainColor);
		addShader(pRenderer, &texturedShaderDesc, &pShaderTextured);
#elif defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11)
		char* pPlainVert = NULL; uint32_t plainVertSize = 0;
		char* pPlainFrag = NULL; uint32_t plainFragSize = 0;
		char* pPlainColorVert = NULL; uint32_t plainColorVertSize = 0;
		char* pPlainColorFrag = NULL; uint32_t plainColorFragSize = 0;
		char* pTexturedVert = NULL; uint32_t texturedVertSize = 0;
		char* pTexturedFrag = NULL; uint32_t texturedFragSize = 0;

		if (pRenderer->mSettings.mApi == RENDERER_API_D3D12 ||
			pRenderer->mSettings.mApi == RENDERER_API_XBOX_D3D12 ||
			pRenderer->mSettings.mApi == RENDERER_API_D3D11)
		{
			pPlainVert = (char*)d3d12_builtin_plain_vert; plainVertSize = sizeof(d3d12_builtin_plain_vert);
			pPlainFrag = (char*)d3d12_builtin_plain_frag; plainFragSize = sizeof(d3d12_builtin_plain_frag);
			pPlainColorVert = (char*)d3d12_builtin_plain_color_vert; plainColorVertSize = sizeof(d3d12_builtin_plain_color_vert);
			pPlainColorFrag = (char*)d3d12_builtin_plain_color_frag; plainColorFragSize = sizeof(d3d12_builtin_plain_color_frag);
			pTexturedVert = (char*)d3d12_builtin_textured_vert; texturedVertSize = sizeof(d3d12_builtin_textured_vert);
			pTexturedFrag = (char*)d3d12_builtin_textured_frag; texturedFragSize = sizeof(d3d12_builtin_textured_frag);
		}
		else if (pRenderer->mSettings.mApi == RENDERER_API_VULKAN)
		{
			pPlainVert = (char*)vk_builtin_plain_vert; plainVertSize = sizeof(vk_builtin_plain_vert);
			pPlainFrag = (char*)vk_builtin_plain_frag; plainFragSize = sizeof(vk_builtin_plain_frag);
			pPlainColorVert = (char*)vk_builtin_plain_color_vert; plainColorVertSize = sizeof(vk_builtin_plain_color_vert);
			pPlainColorFrag = (char*)vk_builtin_plain_color_frag; plainColorFragSize = sizeof(vk_builtin_plain_color_frag);
			pTexturedVert = (char*)vk_builtin_textured_vert; texturedVertSize = sizeof(vk_builtin_textured_vert);
			pTexturedFrag = (char*)vk_builtin_textured_frag; texturedFragSize = sizeof(vk_builtin_textured_frag);
		}

		BinaryShaderDesc plainShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
		{ (char*)pPlainVert, plainVertSize },{ (char*)pPlainFrag, plainFragSize } };
		addShaderBinary(pRenderer, &plainShader, &pShaderPlain);

		BinaryShaderDesc plainColorShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
		{ (char*)pPlainColorVert, plainColorVertSize },{ (char*)pPlainColorFrag, plainColorFragSize } };
		addShaderBinary(pRenderer, &plainColorShader, &pShaderPlainColor);

		BinaryShaderDesc texturedShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
		{ (char*)pTexturedVert, texturedVertSize },{ (char*)pTexturedFrag, texturedFragSize } };
		addShaderBinary(pRenderer, &texturedShader, &pShaderTextured);
#endif

		Shader* pShaders[] = { pShaderPlain, pShaderPlainColor };
		RootSignatureDesc plainRootDesc = { pShaders, 2 };
		addRootSignature(pRenderer, &plainRootDesc, &pRootSignaturePlain);

		const char* pStaticSamplerNames[] = { "uSampler" };
		RootSignatureDesc textureRootDesc = { &pShaderTextured, 1 };
		textureRootDesc.mStaticSamplerCount = 1;
		textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		textureRootDesc.ppStaticSamplers = &pDefaultSampler;
		addRootSignature(pRenderer, &textureRootDesc, &pRootSignatureTextured);

		BufferDesc vbDesc = {};
		vbDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		vbDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		vbDesc.mSize = 1024 * 128 * sizeof(float2);
		vbDesc.mVertexStride = sizeof(float2);
		vbDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		addMeshRingBuffer(pRenderer, &vbDesc, NULL, &pPlainMeshRingBuffer);
		vbDesc.mSize = 1024 * 4 * (sizeof(float4) + sizeof(float2));
		vbDesc.mVertexStride = (sizeof(float4) + sizeof(float2));
		addMeshRingBuffer(pRenderer, &vbDesc, NULL, &pPlainColorMeshRingBuffer);
		vbDesc.mSize = 1024 * 4 * sizeof(float4);
		vbDesc.mVertexStride = sizeof(float4);
		addMeshRingBuffer(pRenderer, &vbDesc, NULL, &pTexturedMeshRingBuffer);

		mVertexLayoutPlain.mAttribCount = 1;
		mVertexLayoutPlain.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		mVertexLayoutPlain.mAttribs[0].mFormat = ImageFormat::RG32F;
		mVertexLayoutPlain.mAttribs[0].mBinding = 0;
		mVertexLayoutPlain.mAttribs[0].mLocation = 0;
		mVertexLayoutPlain.mAttribs[0].mOffset = 0;

		mVertexLayoutPlainColor = mVertexLayoutPlain;
		mVertexLayoutPlainColor.mAttribCount = 2;
		mVertexLayoutPlainColor.mAttribs[1].mSemantic = SEMANTIC_COLOR;
		mVertexLayoutPlainColor.mAttribs[1].mFormat = ImageFormat::RGBA32F;
		mVertexLayoutPlainColor.mAttribs[1].mBinding = 0;
		mVertexLayoutPlainColor.mAttribs[1].mLocation = 1;
		mVertexLayoutPlainColor.mAttribs[1].mOffset = calculateImageFormatStride(ImageFormat::RG32F);

		mVertexLayoutTextured = mVertexLayoutPlain;
		mVertexLayoutTextured.mAttribCount = 2;
		mVertexLayoutTextured.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		mVertexLayoutTextured.mAttribs[1].mFormat = ImageFormat::RG32F;
		mVertexLayoutTextured.mAttribs[1].mBinding = 0;
		mVertexLayoutTextured.mAttribs[1].mLocation = 1;
		mVertexLayoutTextured.mAttribs[1].mOffset = calculateImageFormatStride(ImageFormat::RG32F);
	}

	// releases rendering resources
	//
	void exitRendererResources()
	{
		for (PipelineMap::iterator it = mPipelinesPlain.begin(); it != mPipelinesPlain.end(); ++it)
		{
			for (uint32_t i = 0; i < (uint32_t)it.node->second.size(); ++i)
			{
				if (i == PRIMITIVE_TOPO_PATCH_LIST)
					continue;
				removePipeline(pRenderer, it.node->second[i]);
			}
			it.node->second.clear();
		}
		for (PipelineMap::iterator it = mPipelinesPlainColor.begin(); it != mPipelinesPlainColor.end(); ++it)
		{
			for (uint32_t i = 0; i < (uint32_t)it.node->second.size(); ++i)
			{
				if (i == PRIMITIVE_TOPO_PATCH_LIST)
					continue;
				removePipeline(pRenderer, it.node->second[i]);
			}
			it.node->second.clear();
		}
		for (PipelineMap::iterator it = mPipelinesTextured.begin(); it != mPipelinesTextured.end(); ++it)
		{
			for (uint32_t i = 0; i < (uint32_t)it.node->second.size(); ++i)
			{
				if (i == PRIMITIVE_TOPO_PATCH_LIST)
					continue;
				removePipeline(pRenderer, it.node->second[i]);
			}
			it.node->second.clear();
		}

		mPipelinesPlain.clear();
		mPipelinesPlainColor.clear();
		mPipelinesTextured.clear();

		removeSampler(pRenderer, pDefaultSampler);
		removeBlendState(pBlendAlpha);
		removeDepthState(pDepthState);
		removeRasterizerState(pRasterizerState);
		removeShader(pRenderer, pShaderPlain);
		removeShader(pRenderer, pShaderPlainColor);
		removeShader(pRenderer, pShaderTextured);
		removeRootSignature(pRenderer, pRootSignaturePlain);
		removeRootSignature(pRenderer, pRootSignatureTextured);
		removeMeshRingBuffer(pPlainMeshRingBuffer);
		removeMeshRingBuffer(pPlainColorMeshRingBuffer);
		removeMeshRingBuffer(pTexturedMeshRingBuffer);
	}

	void setSelectedIndex(int idx)
	{
		selectedID = idx;
		if (selectedID >= (int)numControls)
			selectedID = numControls - 1;
		if (selectedID < 0)
			selectedID = 0;

		int maxID = minID + numberOfElements;

		if (selectedID > maxID - scrollOffset)
			minID = selectedID + scrollOffset - numberOfElements;
		if (selectedID < minID + scrollOffset)
			minID = selectedID - scrollOffset + 1;

		maxID = minID + numberOfElements;

		if (minID < 0)
			minID = 0;
		if (maxID >= (int)numControls)
			minID = numControls - numberOfElements;
	}

	void goDirection(int down)
	{
		setSelectedIndex(selectedID + down);
	}

	void processJoystickDownState()
	{
#if !defined(TARGET_IOS) && !defined(__linux__)
		ButtonData dpadJoystick = InputSystem::GetButtonData(KEY_UI_MOVE);
		if (dpadJoystick.mValue[0] < 0)
		{
			pControls[selectedID].modify(-1);
			changedProperty(&pControls[selectedID]);
		}
		//if we press A without any direction, we should still update the value
		//but that doesn't work as the modify function needs to be able to toggle bool values instead
		//of setting them based on given value
		else if (dpadJoystick.mValue[0] > 0)
		{
			pControls[selectedID].modify(1);
			changedProperty(&pControls[selectedID]);
		}
#endif
	}

	// some fwd decls
	struct nk_command_buffer queue;
	struct nk_cursor cursor;
	struct nk_context context;
};

//void initGUIDriver(Renderer* pRenderer, GUIDriver** ppDriver)
//{
//	NuklearGUIDriver* pDriver = conf_placement_new<NuklearGUIDriver>(conf_calloc(1, sizeof(NuklearGUIDriver)));
//	pDriver->init(pRenderer);
//	*ppDriver = pDriver;
//}
//
//void removeGUIDriver(GUIDriver* pDriver)
//{
//	pDriver->exit();
//	(reinterpret_cast<NuklearGUIDriver*>(pDriver))->~NuklearGUIDriver();
//	conf_free(pDriver);
//}

//--------------------------------------------------------------------------------------------
// NUKLEAR UTILITY FUNCTIONS
//--------------------------------------------------------------------------------------------
static nk_color ToNuklearColor(uint color)
{
	nk_color col;	// Translate colours back by bit shifting
	col.r = (color & 0xFF000000) >> 24;
	col.g = (color & 0x00FF0000) >> 16;
	col.b = (color & 0x0000FF00) >> 8;
	col.a = (color & 0x000000FF);
	return col;
}
static nk_colorf ToNuklearColorF(uint color)
{
	nk_colorf col;	// Translate colours back by bit shifting
	col.r = (float)((color & 0xFF000000) >> 24) / 255.0f;
	col.g = (float)((color & 0x00FF0000) >> 16) / 255.0f;
	col.b = (float)((color & 0x0000FF00) >> 8 ) / 255.0f;
	col.a = (float)((color & 0x000000FF)      ) / 255.0f;
	return col;
}
static uint ToUintColor(nk_color nk_c)
{
	uint c =  (((uint)nk_c.r << 24) & 0xFF000000)
			| (((uint)nk_c.g << 16) & 0x00FF0000)
			| (((uint)nk_c.b << 8 ) & 0x0000FF00)
			| (((uint)nk_c.a      ) & 0x000000FF);
	return c;
}
static uint ToUintColor(nk_colorf nk_c)
{
	uint c =  (((uint)(nk_c.r * 255.f) << 24) & 0xFF000000)
			| (((uint)(nk_c.g * 255.f) << 16) & 0x00FF0000)
			| (((uint)(nk_c.b * 255.f) << 8 ) & 0x0000FF00)
			| (((uint)(nk_c.a * 255.f)      ) & 0x000000FF);
	return c;
}

static const nk_keys key(enum UserInputKeys keyCode)
{
	nk_keys k = NK_KEY_NONE;
	switch (keyCode)
	{
	case KEY_RIGHT_STICK_BUTTON: /* Backspace */
		k = NK_KEY_BACKSPACE;
		break;
	default:
		LOGWARNINGF("Unmapped key=%d", (int)keyCode);
	}
	return k;
};
static nk_keys     key(size_t keyEnumVal) { return key((enum UserInputKeys)keyEnumVal); } // non-printable control keys: backspace, etc.

// Printable keys that are not numbers or alphabet characters: space, etc...
static bool IsKeyPrintable(size_t keyEnumVal)
{
	// Volkan: @Manas, we should figure out the key classes (modifier, printable, alphanumeric etc.)
	//         and place them in continuous enum range so we can access them via static array similar 
	//         to the functions above.
	return keyEnumVal == KEY_LEFT_TRIGGER /*space*/
		|| false;	// TODO: others.
}

// Non-printable control keys such as backspace, enter, etc...
static bool IsKeyControl(size_t keyEnumVal)
{
	// Volkan: @Manas, we should figure out the key classes (modifier, printable, alphanumeric etc.)
	//         and place them in continuous enum range so we can access them via static array similar 
	//         to the functions above.
	return keyEnumVal == KEY_RIGHT_STICK_BUTTON /*backspace*/
		|| false;	// TODO: others.
}

//--------------------------------------------------------------------------------------------
// NUKLEAR GUI DRIVER INTERFACE
//--------------------------------------------------------------------------------------------
bool NuklearGUIDriver::init(Renderer* pRenderer)
{
	fontCalibrationOffset = 0.0f;
	currentFontStackPos = 0;
#ifdef USE_LEGACY_INPUT_EVENTS
	inputInstructionCount = 0;
#else
	inputDataCount = 0;
#endif

	selectedID = 0;
	minID = 0;
	numberOfElements = 8;
	scrollOffset = 3;
	initRendererResources(pRenderer);

	return true;
}

void NuklearGUIDriver::exit()
{
	exitRendererResources();
}

bool NuklearGUIDriver::load(Fontstash* fontstash, float fontSize, Texture* cursorTexture, float uiwidth, float uiheight)
{
	// init renderer/font
	this->pFontstash = fontstash;
	fontID = fontstash->getFontID("default");

	// init UI (input)
	memset(&context.input, 0, sizeof context.input);

	// init UI (font)	
	fontStack[currentFontStackPos].driver = this;
	fontStack[currentFontStackPos].fontID = fontID;
	fontStack[currentFontStackPos].userFont.userdata.ptr = fontStack + currentFontStackPos;
	fontStack[currentFontStackPos].userFont.height = fontSize;
	fontStack[currentFontStackPos].userFont.width = font_get_width;
	++(currentFontStackPos);

	// init UI (command queue & config)
	// Use nk_window_get_canvas 
	// nk_buffer_init_fixed(&queue, memoryScratchBuffer, sizeof(memoryScratchBuffer));
	nk_init_default(&context, &fontStack[0].userFont);
	nk_style_default(&context);

	if (cursorTexture != NULL)
	{
		// init Cursor Texture
		cursor.img.handle.ptr = cursorTexture;
		cursor.img.h = 1;
		cursor.img.w = 1;
		cursor.img.region[0] = 0;
		cursor.img.region[1] = 0;
		cursor.img.region[2] = 1;
		cursor.img.region[3] = 1;
		cursor.offset.x = 0;
		cursor.offset.y = 0;
		cursor.size.x = 32;
		cursor.size.y = 32;

		for (nk_flags i = 0; i != NK_CURSOR_COUNT; i++)
		{
			nk_style_load_cursor(&context, nk_style_cursor(i), &cursor);
		}

	}
	nk_style_set_font(&context, &fontStack[0].userFont);

	// Height width
	width = uiwidth;
	height = uiheight;

	return true;
}

void NuklearGUIDriver::unload(){}
void* NuklearGUIDriver::getContext(){ return &context; }


//--------------------------------------------------------------------------------------------
// NUKLEAR GUI INPUT CALLBACKS
//--------------------------------------------------------------------------------------------
bool NuklearGUIDriver::onInput(const ButtonData* pData)
{
	// We want to have a filter for handling the events for the UI
	// - IS_INBOX for checking if the event happens inside the GUI boundaries (for mouse)
	// - if we're handling keyboard events (for which IS_INBOX returns false)
	// - ...
	//

	const bool bIsKeyboardEvent =
		(KEY_CHAR == pData->mUserId)
		|| IsKeyPrintable(pData->mUserId)
		|| IsKeyControl(pData->mUserId);

	const bool bHandleEvent = bIsKeyboardEvent
		|| IS_INBOX(pData->mValue[0], pData->mValue[1]
			, mCurrentWindowRect.x, mCurrentWindowRect.y
			, mCurrentWindowRect.z, mCurrentWindowRect.w)
		|| pData->mUserId == KEY_MOUSE_WHEEL;

	if (bIsKeyboardEvent)	// debugging
	{
		//LOGINFOF("AppUI::OnInput(): Key=%d, IsPressd=%d, IsTrigg=%d", pData->mUserId, pData->mIsPressed, pData->mIsTriggered);
	}

	if (bHandleEvent)
	{
		if (inputDataCount >= sizeof(inputData) / sizeof(inputData[0]))
		{
			LOGWARNING("UI InputData buffer is full! Consider using a larger size for the array.");
			return false;
		}

		// save a copy of the input data for processing later
		memcpy(&inputData[inputDataCount++], pData, sizeof(ButtonData));
	}

	return bHandleEvent;
}

// font size callback
float NuklearGUIDriver::font_get_width(nk_handle handle, float h, const char *text, int len)
{
	float width;
	float bounds[4];
	NuklearGUIDriver::Font* font = (NuklearGUIDriver::Font*)handle.ptr;
	width = font->driver->pFontstash->measureText(bounds, text, (int)len, 0.0f, 0.0f, font->fontID, 0xFFFFFFFF, h, 0.0f);
	return width;
}

void NuklearGUIDriver::changedProperty(UIProperty* pControl)
{
	if (pControl->pCallback)
		pControl->pCallback(pControl);
}

//--------------------------------------------------------------------------------------------
// NUKLEAR GUI DRAW FUNCTIONS
//--------------------------------------------------------------------------------------------
// populates the UI library's command buffer for drawing UI elements, which is later processed by TheForge in draw()
void NuklearGUIDriver::draw_control(UIProperty* pControls, uint numControls, uint idxCurrControl, float w)
{
	nk_context* ctx = &context;
	UIProperty& control = pControls[idxCurrControl];	// shorthand

	if (!(control.mFlags & UIProperty::FLAG_VISIBLE))
		return;
	if (!control.pData)
		return;

	//width of window divided by max columns, taking into account padding
	float colWidth = w / 3 - 12;
	int cols = 2;
	switch (control.mType)
	{
	case UI_CONTROL_SLIDER_FLOAT:
	case UI_CONTROL_SLIDER_INT:
	case UI_CONTROL_SLIDER_UINT:
		cols = 3;
		break;
	case UI_CONTROL_MENU:
	{
		// NOTES:
		// nk_menubar_begin() has to be called right after nk_begin(), so we handle it here earlier than other controls.
		// This also makes the menu an item that cannot be added to a Tree due to the above constraint.
		// We need to parameterize the menu items
		nk_menubar_begin(ctx);
		{
			/* toolbar */
			//nk_layout_row_static(ctx, 40, 40, 3);
			//if (nk_menu_begin_image(ctx, "Music", media->play, nk_vec2(110, 120)))
			//{
			//	/* settings */
			//	nk_layout_row_dynamic(ctx, 25, 1);
			//	nk_menu_item_image_label(ctx, media->play, "Play", NK_TEXT_RIGHT);
			//	nk_menu_item_image_label(ctx, media->stop, "Stop", NK_TEXT_RIGHT);
			//	nk_menu_item_image_label(ctx, media->pause, "Pause", NK_TEXT_RIGHT);
			//	nk_menu_item_image_label(ctx, media->next, "Next", NK_TEXT_RIGHT);
			//	nk_menu_item_image_label(ctx, media->prev, "Prev", NK_TEXT_RIGHT);
			//	nk_menu_end(ctx);
			//}
			//nk_button_image(ctx, media->tools);
			//nk_button_image(ctx, media->cloud);
			//nk_button_image(ctx, media->pen);
		}
		nk_menubar_end(ctx);
	}
	break;
	default:
		break;
	}

	nk_color col = ToNuklearColor(control.mColor);

	nk_layout_row_begin(ctx, NK_STATIC, 30.f, cols);
	nk_layout_row_push(ctx, colWidth);

	nk_label_colored_wrap(ctx, control.mText, col);
	nk_layout_row_push(ctx, cols == 2 ? 2 * colWidth : colWidth);
	switch (control.mType)
	{
	case UI_CONTROL_LABEL:
	{
		break;
	}

	case UI_CONTROL_SLIDER_FLOAT:
	{
		float& currentValue = *(float*)control.pData;
		float oldValue = currentValue;
		nk_slider_float(ctx, control.mSettings.fMin, (float*)control.pData, control.mSettings.fMax, control.mSettings.fIncrement);
		if (wantKeyboardInput)
			currentValue = oldValue;

		// * edit box
		char buffer[200];
		sprintf(buffer, "%.3f", currentValue);
		//nk_property_float(ctx, prop.description, prop.mSettings.fMin, (float*)prop.source, prop.mSettings.fMax, prop.mSettings.fIncrement, prop.mSettings.fIncrement / 10);
		nk_flags result_flags = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT | NK_EDIT_SIG_ENTER, buffer, 200, nk_filter_float);
		if (result_flags == NK_EDIT_ACTIVE)
			needKeyboardInputNextFrame = true;

		if (result_flags != NK_EDIT_INACTIVE)
		{
			needKeyboardInputNextFrame = false;
			currentValue = (float)atof(buffer);
		}


		if (result_flags & NK_EDIT_COMMITED || escWasPressed)
			nk_edit_unfocus(ctx);

		// actualize changes
		if (currentValue != oldValue)
		{
			changedProperty(&control);
		}

		break;
	}
	case UI_CONTROL_SLIDER_INT:
	{
		int& currentValue = *(int*)control.pData;

		int oldValue = currentValue;
		nk_slider_int(ctx, control.mSettings.iMin, (int*)control.pData, control.mSettings.iMax, control.mSettings.iIncrement);
		if (wantKeyboardInput)
			currentValue = oldValue;
		// * edit box
		char buffer[200];
		sprintf(buffer, "%i", currentValue);
		nk_flags result_flags = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT | NK_EDIT_SIG_ENTER, buffer, 200, nk_filter_decimal);
		if (result_flags == NK_EDIT_ACTIVE)
			needKeyboardInputNextFrame = true;

		if (result_flags != NK_EDIT_INACTIVE)
		{
			needKeyboardInputNextFrame = false;
			currentValue = (int)atoi(buffer);
		}

		if (result_flags & NK_EDIT_COMMITED || escWasPressed)
			nk_edit_unfocus(ctx);

		// actualize changes
		if (currentValue != oldValue)
		{
			changedProperty(&control);
		}
		break;
	}

	case UI_CONTROL_SLIDER_UINT:
	{
		int& currentValue = *(int*)control.pData;
		int oldValue = currentValue;
		nk_slider_int(ctx
			, control.mSettings.uiMin > 0 ? control.mSettings.uiMin : 0
			, (int*)control.pData
			, control.mSettings.uiMax
			, control.mSettings.iIncrement
		);

		if (wantKeyboardInput)
			currentValue = oldValue;

		// * edit box
		char buffer[200];
		sprintf(buffer, "%u", currentValue);
		nk_flags result_flags = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT | NK_EDIT_SIG_ENTER, buffer, 200, nk_filter_decimal);
		if (result_flags == NK_EDIT_ACTIVE)
			needKeyboardInputNextFrame = true;

		if (result_flags != NK_EDIT_INACTIVE)
		{
			needKeyboardInputNextFrame = false;
			currentValue = (int)atoi(buffer);
		}


		if (result_flags & NK_EDIT_COMMITED || escWasPressed)
			nk_edit_unfocus(ctx);

		// actualize changes
		if (currentValue != oldValue)
		{
			changedProperty(&control);
		}

		break;
	}
	case UI_CONTROL_CHECKBOX:
	{
		bool& bCurrValue = *(bool*)control.pData;
		
		// The way nuklear renders checked radio buttons
		// look like its the other way around, so we
		// invert the value by the mapping below.
		//
		// true <-> 0 | false <-> 1 
		//
		const int valueOld = (bCurrValue) ? 0 : 1;
		int value = valueOld;

		nk_checkbox_label(ctx, bCurrValue ? "True" : "False", &value);
		if (valueOld != value)
		{
			bCurrValue = (value == 0);
			changedProperty(&control);
		}
		break;
	}
	case UI_CONTROL_DROPDOWN:
	{
		ASSERT(control.mSettings.eByteSize == 4);

		int current = control.enumComputeIndex();
		int previous = current;
		int cnt = 0;
		for (int vi = 0; control.mSettings.eNames[vi] != 0; vi++)
			cnt = (int)vi;

		nk_combobox(ctx, control.mSettings.eNames, cnt + 1, &current, 16, nk_vec2(colWidth * 2, 16 * 5));

		if (previous != current)
		{
			*(int*)control.pData = ((int*)control.mSettings.eValues)[current];
			changedProperty(&control);
		}
		break;
	}
	case UI_CONTROL_BUTTON:
	{
		if (nk_button_label(ctx, control.mText))
		{
			if (control.pData)
				((UIButtonFn)control.pData)(control.mSettings.pUserData);
			changedProperty(&control);
		}
		break;
	}
	case UI_CONTROL_TEXTBOX:
	{
		nk_flags result_flags = nk_edit_string_zero_terminated(
			ctx
			, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT
			, (char*)control.pData
			, control.mSettings.sLen
			, nk_filter_ascii);

		if (result_flags == NK_EDIT_ACTIVE)   { needKeyboardInputNextFrame = true; }
		if (result_flags != NK_EDIT_INACTIVE) { needKeyboardInputNextFrame = false; }
		if (result_flags & NK_EDIT_COMMITED || escWasPressed)
		{
			nk_edit_unfocus(ctx);
			changedProperty(&control);
		}
		break;
	}
	case UI_CONTROL_RADIO_BUTTON:
	{
		bool& bCurrValue = *(bool*)control.pData;
		
		// The way nuklear renders checked radio buttons
		// look like its the other way around, so we
		// invert the value by the mapping below.
		//
		// true <-> 0 | false <-> 1 
		//
		const int valueOld = (bCurrValue) ? 0 : 1;
		int value = valueOld;
		
		nk_radio_label(ctx, control.mText, &value);
		if (valueOld != value)
		{
			bCurrValue = (value == 0);
			changedProperty(&control);

			// When a radio button is toggled on, other radio buttons 
			// in the same  UI group should be toggled off.
			for (uint32_t i = 0; i < numControls; ++i)
			{
				if (pControls[i].mType != UI_CONTROL_RADIO_BUTTON)
					continue;	// skip non-radio buttons

				if (&pControls[i] == &control)
					continue;	// skip self

				*((bool*)pControls[i].pData) = false;
			}
		}
		break;
	}
	case UI_CONTROL_PROGRESS_BAR:
	{
		size_t* pCurrProgress = (size_t*)control.pData;
		nk_progress(ctx, pCurrProgress, control.mSettings.maxProgress, 0);
		break;
	}
	case UI_CONTROL_COLOR_SLIDER:
	{
		uint& colorPick = *(uint*)control.pData;

		const struct nk_vec2 sz = nk_vec2(colWidth * 2, 150.f);

		// color combo box w/ sliders for rgba
		nk_color combo_color = ToNuklearColor(colorPick);
		float ratios[] = { 0.15f, 0.85f };
		if (nk_combo_begin_color(ctx, combo_color, sz))
		{
			nk_layout_row(ctx, NK_DYNAMIC, 30, 2, ratios);
			nk_label(ctx, "R:", NK_TEXT_LEFT);
			combo_color.r = (nk_byte)nk_slide_int(ctx, 0, combo_color.r, 255, 5);
			nk_label(ctx, "G:", NK_TEXT_LEFT);
			combo_color.g = (nk_byte)nk_slide_int(ctx, 0, combo_color.g, 255, 5);
			nk_label(ctx, "B:", NK_TEXT_LEFT);
			combo_color.b = (nk_byte)nk_slide_int(ctx, 0, combo_color.b, 255, 5);
			nk_label(ctx, "A:", NK_TEXT_LEFT);
			combo_color.a = (nk_byte)nk_slide_int(ctx, 0, combo_color.a, 255, 5);
			colorPick = ToUintColor(combo_color);
			nk_combo_end(ctx);
		}
		break;
	}
	case UI_CONTROL_COLOR_PICKER:
	{
		enum color_mode { COL_RGB, COL_HSV };

		const struct nk_vec2 sz = nk_vec2(colWidth * 2, 420.f);
		const int COLOR_PICKER_HEIGHT = 250;
		const int RGB_HSV_RADIO_HEIGHT = 25;

		uint& colorPick = *(uint*)control.pData;

		// color combo box w/ color picking
		nk_colorf combo_color_f = ToNuklearColorF(colorPick);
		nk_color combo_color    = ToNuklearColor(colorPick);
		if (nk_combo_begin_color(ctx, combo_color, sz))
		{
			int& col_mode = control.mSettings.colorMode;

			nk_layout_row_dynamic(ctx, COLOR_PICKER_HEIGHT, 1);
			combo_color_f = nk_color_picker(ctx, combo_color_f, NK_RGBA);

			nk_layout_row_dynamic(ctx, RGB_HSV_RADIO_HEIGHT, 2);
			col_mode = nk_option_label(ctx, "RGB", col_mode == COL_RGB) ? COL_RGB : col_mode;
			col_mode = nk_option_label(ctx, "HSV", col_mode == COL_HSV) ? COL_HSV : col_mode;

			nk_layout_row_dynamic(ctx, RGB_HSV_RADIO_HEIGHT, 1);
			if (col_mode == COL_RGB) 
			{
				combo_color_f.r = nk_propertyf(ctx, "#R:", 0, combo_color_f.r, 1.0f, 0.01f, 0.005f);
				combo_color_f.g = nk_propertyf(ctx, "#G:", 0, combo_color_f.g, 1.0f, 0.01f, 0.005f);
				combo_color_f.b = nk_propertyf(ctx, "#B:", 0, combo_color_f.b, 1.0f, 0.01f, 0.005f);
				combo_color_f.a = nk_propertyf(ctx, "#A:", 0, combo_color_f.a, 1.0f, 0.01f, 0.005f);
			}
			else 
			{
				float hsva[4];
				nk_colorf_hsva_fv(hsva, combo_color_f);
				hsva[0] = nk_propertyf(ctx, "#H:", 0, hsva[0], 1.0f, 0.01f, 0.05f);
				hsva[1] = nk_propertyf(ctx, "#S:", 0, hsva[1], 1.0f, 0.01f, 0.05f);
				hsva[2] = nk_propertyf(ctx, "#V:", 0, hsva[2], 1.0f, 0.01f, 0.05f);
				hsva[3] = nk_propertyf(ctx, "#A:", 0, hsva[3], 1.0f, 0.01f, 0.05f);
				combo_color_f = nk_hsva_colorfv(hsva);
			}

			colorPick = ToUintColor(combo_color_f);
			nk_combo_end(ctx);
		}
	} break;
	case UI_CONTROL_CONTEXTUAL:
	{
		const float CONTEXT_ITEM_HEIGHT_PX = 100.0f;
		const float CONTEXT_ITEM_MIN_WIDTH_PX = 100.0f;
		const float CHARACTER_WIDTH = 7.3f;	// per context menu entry text

		// determine the context dropdown size based on longest context menu entry (should be calculated in ctor)
		size_t longestContextMenuItemLen = 0;
		for (int item = 0; item < control.mSettings.numContextItems; ++item)
			longestContextMenuItemLen = max(longestContextMenuItemLen, strlen(control.mSettings.pContextItems[item]));

		struct nk_vec2 contextualSize = nk_vec2(
			max(CONTEXT_ITEM_MIN_WIDTH_PX, longestContextMenuItemLen * CHARACTER_WIDTH), // X
			CONTEXT_ITEM_HEIGHT_PX * control.mSettings.numContextItems                   // Y
		);

		// record draw cmds
		if (nk_contextual_begin(ctx, NK_WINDOW_NO_SCROLLBAR, contextualSize, nk_window_get_bounds(ctx))) {
			nk_layout_row_dynamic(ctx, 30, 1);
			for (int item = 0; item < control.mSettings.numContextItems; ++item)
			{
				if (nk_contextual_item_label(ctx, control.mSettings.pContextItems[item], NK_TEXT_LEFT))
				{
					if (control.mSettings.pfnCallbacks && control.mSettings.pfnCallbacks[item])
						control.mSettings.pfnCallbacks[item]();	// callback fn
				}
			}
			nk_contextual_end(ctx);
		}
		break;
	}
	}	// switch
}

typedef tinystl::unordered_map<const char*, tinystl::vector<UIProperty>> UIControlMap;
typedef tinystl::unordered_hash_iterator<tinystl::unordered_hash_node<char const*, tinystl::vector<UIProperty>>> UIControlMapIterator;

void NuklearGUIDriver::draw(
	  Cmd* pCmd
	, float deltaTime
	, const char* pTitle
	, float x, float y
	, float w, float h
	, UIProperty* pControls
	, uint numControls
)
{
	nk_clear(&context);
	/************************************************************************/
	// Input
	/************************************************************************/
#if !defined(TARGET_IOS)

	nk_input_begin(&context);

	for (int i = 0; i < inputDataCount; i++)
	{
		const ButtonData& inpData = inputData[i];
		const int32_t mousex = (int32_t)inpData.mValue[0];
		const int32_t mousey = (int32_t)inpData.mValue[1];

		switch (inpData.mUserId)
		{
		// MOUSE MOVE
		case KEY_UI_MOVE:
			nk_input_motion(&context, mousex, mousey);
			break;


		// MOUSE CLICKS
		case KEY_RIGHT_BUMPER:	// right click
		case KEY_CONFIRM:		// left click
		{
			nk_buttons btn = inpData.mUserId == KEY_CONFIRM
				? NK_BUTTON_LEFT
				: (inpData.mUserId == KEY_RIGHT_BUMPER ? NK_BUTTON_RIGHT : NK_BUTTON_MIDDLE);
			//LOGINFOF("%s", btn == NK_BUTTON_LEFT ? "left " : "right");

			nk_input_button(&context, btn, mousex, mousey, inpData.mIsPressed ? nk_true : nk_false);
		}	break;


		// MOUSE SCROLL
		case KEY_MOUSE_WHEEL:
			//LOGINFOF("Wheel x=%d, y=%d, delta=%f", mousex, mousey, inpData.mDeltaValue[0]);
			if (inpData.mIsPressed)
				nk_input_scroll(&context, nk_vec2(0.0f, inpData.mDeltaValue[0]));
			break;


		// KEYBOARD
		default:
			if (KEY_CHAR == inpData.mUserId)
			{
				//LOGINFOF("Keyboard mVal=%d, mIsPress=%d, mIsTrigg=%d", inpData.mUserId, inpData.mIsPressed, inpData.mIsTriggered);
				if (inpData.mIsPressed)
				{
					nk_input_char(&context, (char)inpData.mCharacter);
				}
			}
			else if (IsKeyControl(inpData.mUserId))
			{
				//LOGINFOF("Keyboard mVal=%d, mIsPress=%d, mIsTrigg=%d", inpData.mUserId, inpData.mIsPressed, inpData.mIsTriggered);
				// don't check for pressed - send the data to nuklear to handle release events.
				nk_input_key(&context, key(inpData.mUserId), inpData.mIsPressed ? 1 : 0);
			}
			
			// Unhandled events go here
			else if (inpData.mIsPressed)
			{
				// sometimes after uncapturing/capturing the mouse, or
				// moving the mouse outside the GUI window sends this data:
				//  - inpData.mIsPressed=true && GAINPUT_RAW_MOUSE 
				// TODO: look into why it doesn't come with KEY_UI_MOVE
				const bool bRawMouseInputWithPressedValue = (inpData.mActiveDevicesMask & GainputDeviceType::GAINPUT_RAW_MOUSE) > 0;
				if (!bRawMouseInputWithPressedValue)
				{
					LOGWARNINGF("Unhandled input event: Btn=%d", inpData.mUserId);
				}
			}

			//LOGINFOF("nuklear::processInput(): %d", inpData.mUserId);
			break;
		}	// switch
	}

	nk_input_end(&context);

	// reset instruction/inputData counter
	inputDataCount = 0;
#else	//!defined(TARGET_IOS)
//	ASSERT(false && "Unsupported on target iOS");
#endif	//!defined(TARGET_IOS)
	/************************************************************************/
	// Window
	/************************************************************************/
	pControls = pControls;
	numControls = (uint32_t)numControls;

	int result = nk_begin(&context, pTitle, nk_rect(x, y, w, h),
		NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE);

	if (result)
	{
		// first generate the tree map
		// TODO: do this once... no need for doing it every frame.
		UIControlMap map;
		tinystl::vector<UIProperty> nonTreeControls;
		for (uint32_t i = 0; i < numControls; ++i)
		{
			// If no tree, just add the ui control
			if (strcmp(pControls[i].pTree, "none") != 0)
			{
				map[pControls[i].pTree].push_back(pControls[i]);
			}
			else
			{
				nonTreeControls.push_back(pControls[i]);
			}
		}

		// prepare draw commands for the controls
		const uint numNonTreeCtrls = (uint)nonTreeControls.size();
		for (uint32_t i = 0; i < numNonTreeCtrls; ++i)
		{
			draw_control(nonTreeControls.data(), numNonTreeCtrls, i, w);
		}

		
		// if we don't differentiate between trees with IDs, all trees are interacted with at the same time...
		// Go through all the trees and add the UIControls for every tree 
		int treeUniqueID = 0;	
		for (UIControlMapIterator local_it = map.begin(); local_it != map.end(); ++local_it)
		{
			if (nk_tree_push_id(&context, NK_TREE_TAB, local_it.node->first, NK_MINIMIZED, treeUniqueID++)) 
			{
				for (int i = 0; i < local_it.node->second.size(); ++i)
				{
					draw_control(local_it.node->second.data(), (uint)local_it.node->second.size(), i, w);
				}
				nk_tree_pop(&context);	// all code between push and pop will be seen as part of the UI tree
			}
		}
	}

	struct nk_rect r = nk_window_get_bounds(&context);
	if (!result)
	{
		r.h = nk_window_get_panel(&context)->header_height;
	}
	nk_end(&context);

	mCurrentWindowRect.x = r.x;
	mCurrentWindowRect.y = r.y;
	mCurrentWindowRect.z = r.w;
	mCurrentWindowRect.w = r.h;

	wantKeyboardInput = needKeyboardInputNextFrame;
	/************************************************************************/
	/************************************************************************/
	struct PlainRootConstants
	{
		float4 color;
		float2 scaleBias;
	};
	struct PlainColorRootConstants
	{
		float2 scaleBias;
		float4 color;
	};

	static const int CircleEdgeCount = 10;

	tinystl::vector<Pipeline*>* pPipelinesPlain = NULL;
	tinystl::vector<Pipeline*>* pPipelinesPlainColor = NULL;
	tinystl::vector<Pipeline*>* pPipelinesTextured = NULL;
	Pipeline* pCurrentPipeline = NULL;
	GraphicsPipelineDesc pipelineDesc = {};
	pipelineDesc.mDepthStencilFormat = (ImageFormat::Enum)pCmd->mBoundDepthStencilFormat;
	pipelineDesc.mRenderTargetCount = pCmd->mBoundRenderTargetCount;
	pipelineDesc.mSampleCount = pCmd->mBoundSampleCount;
	pipelineDesc.pBlendState = pBlendAlpha;
	pipelineDesc.mSampleQuality = pCmd->mBoundSampleQuality;
	pipelineDesc.pColorFormats = (ImageFormat::Enum*)pCmd->pBoundColorFormats;
	pipelineDesc.pDepthState = pDepthState;
	pipelineDesc.pRasterizerState = pRasterizerState;
	pipelineDesc.pSrgbValues = pCmd->pBoundSrgbValues;
	PipelineMap::iterator it = mPipelinesPlain.find(pCmd->mRenderPassHash);
	if (it == mPipelinesPlain.end())
	{
		tinystl::vector<Pipeline*> pipelines(PRIMITIVE_TOPO_COUNT);
		tinystl::vector<Pipeline*> pipelinesColor(PRIMITIVE_TOPO_COUNT);
		pipelineDesc.pRootSignature = pRootSignaturePlain;
		for (uint32_t i = 0; i < PRIMITIVE_TOPO_COUNT; ++i)
		{
			if (i == PRIMITIVE_TOPO_PATCH_LIST)
				continue;

			pipelineDesc.mPrimitiveTopo = (PrimitiveTopology)i;
			pipelineDesc.pShaderProgram = pShaderPlain;
			pipelineDesc.pVertexLayout = &mVertexLayoutPlain;
			addPipeline(pCmd->pRenderer, &pipelineDesc, &pipelines[i]);

			pipelineDesc.pShaderProgram = pShaderPlainColor;
			pipelineDesc.pVertexLayout = &mVertexLayoutPlainColor;
			addPipeline(pCmd->pRenderer, &pipelineDesc, &pipelinesColor[i]);
		}
		pPipelinesPlain = &mPipelinesPlain.insert({ pCmd->mRenderPassHash, pipelines }).first->second;
		pPipelinesPlainColor = &mPipelinesPlainColor.insert({ pCmd->mRenderPassHash, pipelinesColor }).first->second;
	}
	else
	{
		pPipelinesPlain = &it.node->second;
		pPipelinesPlainColor = &mPipelinesPlainColor[pCmd->mRenderPassHash];
	}
	it = mPipelinesTextured.find(pCmd->mRenderPassHash);
	if (it == mPipelinesTextured.end())
	{
		tinystl::vector<Pipeline*> pipelines(PRIMITIVE_TOPO_COUNT);
		pipelineDesc.pRootSignature = pRootSignatureTextured;
		pipelineDesc.pShaderProgram = pShaderTextured;
		pipelineDesc.pVertexLayout = &mVertexLayoutTextured;
		for (uint32_t i = 0; i < PRIMITIVE_TOPO_COUNT; ++i)
		{
			if (i == PRIMITIVE_TOPO_PATCH_LIST)
				continue;

			pipelineDesc.mPrimitiveTopo = (PrimitiveTopology)i;
			addPipeline(pCmd->pRenderer, &pipelineDesc, &pipelines[i]);
}
		pPipelinesTextured = &mPipelinesTextured.insert({ pCmd->mRenderPassHash, pipelines }).first->second;
	}
	else
	{
		pPipelinesTextured = &it.node->second;
	}

	tinystl::vector<Pipeline*>& pipelinesPlain = *pPipelinesPlain;
	tinystl::vector<Pipeline*>& pipelinesPlainColor = *pPipelinesPlainColor;
	tinystl::vector<Pipeline*>& pipelinesTextured = *pPipelinesTextured;

#ifdef _DURANGO
	{
		// Search for the currently selected property and change it's color to indicate selection status
		const struct nk_command* cmd;
		for ((cmd) = nk__begin(&context); (cmd) != 0; (cmd) = nk__next(&context, cmd))
		{
			if (cmd->type == NK_COMMAND_TEXT)
			{
				struct nk_command_text *textCommand = (struct nk_command_text*)cmd;
				if (strcmp((const char*)textCommand->string, pControls[selectedID].mText) == 0)
				{
					// change color to indicate selection status
					textCommand->foreground.r = 1;
					textCommand->foreground.g = 1;
					textCommand->foreground.b = 1;
				}
			}
		}
}
#endif
	
	const struct nk_command *cmd;
	/* iterate over and execute each draw command except the text */
	nk_foreach(cmd, &context)
	{
		switch (cmd->type)
		{
		case NK_COMMAND_NOP:
			break;
		case NK_COMMAND_SCISSOR:
		{
			const struct nk_command_scissor *s = (const struct nk_command_scissor*)cmd;
			RectDesc scissorRect;
			scissorRect.left = s->x;
			scissorRect.right = s->x + s->w;
			scissorRect.top = s->y;
			scissorRect.bottom = s->y + s->h;
			cmdSetScissor(pCmd, max(0, scissorRect.left), max(0, scissorRect.top), getRectWidth(scissorRect), getRectHeight(scissorRect));
			break;
		}
		case NK_COMMAND_LINE:
		{
			const struct nk_command_line *l = (const struct nk_command_line*)cmd;
			const float lineOffset = float(l->line_thickness) / 2;
			// thick line support
			const vec2 begin = vec2(l->begin.x, l->begin.y);
			const vec2 end = vec2(l->end.x, l->end.y);
			const vec2 normal = normalize(end - begin);
			const vec2 binormal = vec2(normal.getY(), -normal.getX()) * lineOffset;
			vec2 vertices[] =
			{
				(begin + binormal), (end + binormal),
				(begin - binormal), (end - binormal)
			};

			RingBufferOffset buffer = getVertexBufferOffset(pPlainMeshRingBuffer, sizeof(vertices));
			BufferUpdateDesc vbUpdate = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
			updateResource(&vbUpdate);

			PlainRootConstants data = {};
			data.color = float4((float)l->color.r / 255.0f, (float)l->color.g / 255.0f, (float)l->color.b / 255.0f, (float)l->color.a / 255.0f);
			data.scaleBias = { 2.0f / (float)pCmd->mBoundWidth, -2.0f / (float)pCmd->mBoundHeight };
			DescriptorData params[1] = {};
			params[0].pName = "uRootConstants";
			params[0].pRootConstant = &data;
			cmdBindDescriptors(pCmd, pRootSignaturePlain, 1, params);

			if (pCurrentPipeline != pipelinesPlain[PRIMITIVE_TOPO_TRI_STRIP])
			{
				pCurrentPipeline = pipelinesPlain[PRIMITIVE_TOPO_TRI_STRIP];
				cmdBindPipeline(pCmd, pCurrentPipeline);
			}

			cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
			cmdDraw(pCmd, 4, 0);
			break;
		}
		case NK_COMMAND_RECT:
		{
			const struct nk_command_rect *r = (const struct nk_command_rect*)cmd;
			float lineOffset = float(r->line_thickness);
			// thick line support
			float2 vertices[] = 
			{ 
				// top-left
				float2(r->x - lineOffset, r->y - lineOffset), float2(r->x + lineOffset, r->y + lineOffset),
				
				// top-right
				float2(r->x + r->w + lineOffset, r->y - lineOffset), float2(r->x + r->w - lineOffset, r->y + lineOffset),

				// bottom-right
				float2(r->x + r->w + lineOffset, r->y + r->h + lineOffset), float2(r->x + r->w - lineOffset, r->y + r->h - lineOffset),

				// bottom-left
				float2(r->x - lineOffset, r->y + r->h + lineOffset), float2(r->x + lineOffset, r->y + r->h - lineOffset),

				// top-left
				float2(r->x - lineOffset, r->y - lineOffset), float2(r->x + lineOffset, r->y + lineOffset),
			};

			RingBufferOffset buffer = getVertexBufferOffset(pPlainMeshRingBuffer, sizeof(vertices));
			BufferUpdateDesc vbUpdate = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
			updateResource(&vbUpdate);

			PlainRootConstants data = {};
			data.color = float4((float)r->color.r / 255.0f, (float)r->color.g / 255.0f, (float)r->color.b / 255.0f, (float)r->color.a / 255.0f);
			data.scaleBias = { 2.0f / (float)pCmd->mBoundWidth, -2.0f / (float)pCmd->mBoundHeight };
			DescriptorData params[1] = {};
			params[0].pName = "uRootConstants";
			params[0].pRootConstant = &data;
			cmdBindDescriptors(pCmd, pRootSignaturePlain, 1, params);

			if (pCurrentPipeline != pipelinesPlain[PRIMITIVE_TOPO_TRI_STRIP])
			{
				pCurrentPipeline = pipelinesPlain[PRIMITIVE_TOPO_TRI_STRIP];
				cmdBindPipeline(pCmd, pCurrentPipeline);
			}

			cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
			cmdDraw(pCmd, 10, 0);
			break;
		}
		case NK_COMMAND_RECT_MULTI_COLOR:
		{
			const struct nk_command_rect_multi_color *r = (const struct nk_command_rect_multi_color*)cmd;
			struct ColorVertex
			{
				float2 pos;
				float4 col;
			};

			// color values sent by nuklear for the color picker color matrix:
			//
			// -----------------------------------------------------------
			// | pass #1:             	| pass #2:                       |
			// |------------------------|--------------------------------|
			// | left & bottom = white 	| bottom & right = black         |
			// | top  & right  = color  | top & left     = transparent   |
			// -----------------------------------------------------------
			//
			// We map these color values for edges into vertex data,
			// which has to be corners. Given our triangle vertex positions
			// in the ColorVertex struct, we can use the following mapping
			// for accurate results:
			//
			// left   -> top-left
			// top    -> top-right
			// right  -> bottom-right
			// bottom -> bottom-left
			//
			float topLeft[4];      nk_color_fv(topLeft, r->left);
			float topRight[4];     nk_color_fv(topRight, r->top);
			float bottomRight[4];  nk_color_fv(bottomRight, r->right);
			float bottomLeft[4];   nk_color_fv(bottomLeft, r->bottom);
			const float2 vertices[] = { MAKEQUAD(r->x, r->y, r->x + r->w, r->y + r->h, 0.0f) };
			const ColorVertex colorVerts[4] = 
			{
				{ vertices[0], topLeft },
				{ vertices[1], bottomLeft },
				{ vertices[2], topRight },
				{ vertices[3], bottomRight },
			};

			RingBufferOffset buffer = getVertexBufferOffset(pPlainColorMeshRingBuffer, sizeof(colorVerts));
			BufferUpdateDesc vbUpdate = { buffer.pBuffer, colorVerts, 0, buffer.mOffset, sizeof(colorVerts) };
			updateResource(&vbUpdate);

			PlainColorRootConstants data = {};
			data.scaleBias = { 2.0f / (float)pCmd->mBoundWidth, -2.0f / (float)pCmd->mBoundHeight };
			DescriptorData params[1] = {};
			params[0].pName = "uRootConstants";
			params[0].pRootConstant = &data;
			cmdBindDescriptors(pCmd, pRootSignaturePlain, 1, params);

			if (pCurrentPipeline != pipelinesPlainColor[PRIMITIVE_TOPO_TRI_STRIP])
			{
				pCurrentPipeline = pipelinesPlainColor[PRIMITIVE_TOPO_TRI_STRIP];
				cmdBindPipeline(pCmd, pCurrentPipeline);
			}

			cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
			cmdDraw(pCmd, 4, 0);
			break;
		}
		break;
		case NK_COMMAND_RECT_FILLED:
		{
			const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled*)cmd;
			float2 vertices[] = { MAKEQUAD(r->x, r->y, r->x + r->w, r->y + r->h, 0.0f) };
			RingBufferOffset buffer = getVertexBufferOffset(pPlainMeshRingBuffer, sizeof(vertices));
			BufferUpdateDesc vbUpdate = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
			updateResource(&vbUpdate);

			PlainRootConstants data = {};
			data.color = float4((float)r->color.r / 255.0f, (float)r->color.g / 255.0f, (float)r->color.b / 255.0f, (float)r->color.a / 255.0f);
			data.scaleBias = { 2.0f / (float)pCmd->mBoundWidth, -2.0f / (float)pCmd->mBoundHeight };
			DescriptorData params[1] = {};
			params[0].pName = "uRootConstants";
			params[0].pRootConstant = &data;
			cmdBindDescriptors(pCmd, pRootSignaturePlain, 1, params);

			if (pCurrentPipeline != pipelinesPlain[PRIMITIVE_TOPO_TRI_STRIP])
			{
				pCurrentPipeline = pipelinesPlain[PRIMITIVE_TOPO_TRI_STRIP];
				cmdBindPipeline(pCmd, pCurrentPipeline);
			}

			cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
			cmdDraw(pCmd, 4, 0);
			break;
		}
		case NK_COMMAND_CIRCLE:
		{
			const struct nk_command_circle *r = (const struct nk_command_circle*)cmd;
			// thick line support
			const float lineOffset = float(r->line_thickness) / 2;
			const float hw = (float)r->w / 2.0f;
			const float hh = (float)r->h / 2.0f;
			const float2 center = float2(r->x + hw, r->y + hh);

			float2 vertices[(CircleEdgeCount + 1) * 2];
			float t = 0;
			for (uint i = 0; i < CircleEdgeCount + 1; ++i)
			{
				const float dt = (2 * PI) / CircleEdgeCount;
				vertices[i * 2 + 0] = center + float2(cosf(t) * (hw + lineOffset), sinf(t) * (hh + lineOffset));
				vertices[i * 2 + 1] = center + float2(cosf(t) * (hw - lineOffset), sinf(t) * (hh - lineOffset));
				t += dt;
			}

			RingBufferOffset buffer = getVertexBufferOffset(pPlainMeshRingBuffer, sizeof(vertices));
			BufferUpdateDesc vbUpdate = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
			updateResource(&vbUpdate);

			PlainRootConstants data = {};
			data.color = float4((float)r->color.r / 255.0f, (float)r->color.g / 255.0f, (float)r->color.b / 255.0f, (float)r->color.a / 255.0f);
			data.scaleBias = { 2.0f / (float)pCmd->mBoundWidth, -2.0f / (float)pCmd->mBoundHeight };
			DescriptorData params[1] = {};
			params[0].pName = "uRootConstants";
			params[0].pRootConstant = &data;
			cmdBindDescriptors(pCmd, pRootSignaturePlain, 1, params);

			if (pCurrentPipeline != pipelinesPlain[PRIMITIVE_TOPO_TRI_STRIP])
			{
				pCurrentPipeline = pipelinesPlain[PRIMITIVE_TOPO_TRI_STRIP];
				cmdBindPipeline(pCmd, pCurrentPipeline);
			}

			cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
			cmdDraw(pCmd, (CircleEdgeCount + 1) * 2, 0);
			break;
		}
		case NK_COMMAND_CIRCLE_FILLED:
		{
			const struct nk_command_circle_filled *r = (const struct nk_command_circle_filled*)cmd;
			const float hw = (float)r->w / 2.0f;
			const float hh = (float)r->h / 2.0f;
			const float2 center = float2(r->x + hw, r->y + hh);
			
			float2 vertices[(CircleEdgeCount) * 2 + 1];
			float t = 0;
			for (uint i = 0; i < CircleEdgeCount; ++i)
			{
				const float dt = (2 * PI) / CircleEdgeCount;
				vertices[i * 2 + 0] = center + float2(cosf(t) * hw, sinf(t) * hh);
				vertices[i * 2 + 1] = center;
				t += dt;
			}
			// set last point on circle
			vertices[CircleEdgeCount * 2] = vertices[0];

			RingBufferOffset buffer = getVertexBufferOffset(pPlainMeshRingBuffer, sizeof(vertices));
			BufferUpdateDesc vbUpdate = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
			updateResource(&vbUpdate);

			PlainRootConstants data = {};
			data.color = float4((float)r->color.r / 255.0f, (float)r->color.g / 255.0f, (float)r->color.b / 255.0f, (float)r->color.a / 255.0f);
			data.scaleBias = { 2.0f / (float)pCmd->mBoundWidth, -2.0f / (float)pCmd->mBoundHeight };
			DescriptorData params[1] = {};
			params[0].pName = "uRootConstants";
			params[0].pRootConstant = &data;
			cmdBindDescriptors(pCmd, pRootSignaturePlain, 1, params);

			if (pCurrentPipeline != pipelinesPlain[PRIMITIVE_TOPO_TRI_STRIP])
			{
				pCurrentPipeline = pipelinesPlain[PRIMITIVE_TOPO_TRI_STRIP];
				cmdBindPipeline(pCmd, pCurrentPipeline);
			}

			cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
			cmdDraw(pCmd, (CircleEdgeCount * 2) + 1, 0);
			break;
		}
		case NK_COMMAND_TRIANGLE:
		{
			//const struct nk_command_triangle *r = (const struct nk_command_triangle*)cmd;
			//vec2 vertices[] = { vec2(r->a.x, r->a.y), vec2(r->b.x, r->b.y), vec2(r->c.x, r->c.y) };
			//vec4 color[] = { vec4((float)r->color.r / 255.0f, (float)r->color.g / 255.0f, (float)r->color.b / 255.0f, (float)r->color.a / 255.0f) };
			//renderer->drawPlain(PRIM_LINE_LOOP, vertices, 3, bstAlphaBlend, dstNone, color, rstScissorNoCull);
			break;
		}
		case NK_COMMAND_TRIANGLE_FILLED:
		{
			const struct nk_command_triangle_filled *r = (const struct nk_command_triangle_filled*)cmd;
			float2 vertices[] = { float2(r->a.x, r->a.y), float2(r->b.x, r->b.y), float2(r->c.x, r->c.y) };

			RingBufferOffset buffer = getVertexBufferOffset(pPlainMeshRingBuffer, sizeof(vertices));
			BufferUpdateDesc vbUpdate = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
			updateResource(&vbUpdate);

			PlainRootConstants data = {};
			data.color = float4((float)r->color.r / 255.0f, (float)r->color.g / 255.0f, (float)r->color.b / 255.0f, (float)r->color.a / 255.0f);
			data.scaleBias = { 2.0f / (float)pCmd->mBoundWidth, -2.0f / (float)pCmd->mBoundHeight };
			DescriptorData params[1] = {};
			params[0].pName = "uRootConstants";
			params[0].pRootConstant = &data;
			cmdBindDescriptors(pCmd, pRootSignaturePlain, 1, params);

			if (pCurrentPipeline != pipelinesPlain[PRIMITIVE_TOPO_TRI_LIST])
			{
				pCurrentPipeline = pipelinesPlain[PRIMITIVE_TOPO_TRI_LIST];
				cmdBindPipeline(pCmd, pCurrentPipeline);
			}

			cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
			cmdDraw(pCmd, 3, 0);
			break;
		}
		case NK_COMMAND_IMAGE:
		{
			const struct nk_command_image *r = (const struct nk_command_image*)cmd;

			float2 RegionTopLeft(float2((float)r->x, (float)r->y));
			float2 RegionBottonLeft(RegionTopLeft + float2(0, r->h));
			float2 RegionTopRight(RegionTopLeft + float2(r->w, 0));
			float2 RegionBottonRight(RegionTopLeft + float2(r->w, r->h));

			float4 vertices[4] = {
				float4(RegionBottonRight.x, RegionBottonRight.y, float(r->img.region[0] + r->img.region[3]) / r->img.w, float(r->img.region[1] + r->img.region[2]) / r->img.h),
				float4(RegionTopRight.x, RegionTopRight.y, float(r->img.region[0] + r->img.region[3]) / r->img.w, float(r->img.region[1]) / r->img.h),
				float4(RegionBottonLeft.x, RegionBottonLeft.y, float(r->img.region[0]) / r->img.w, float(r->img.region[1] + r->img.region[2]) / r->img.h),
				float4(RegionTopLeft.x, RegionTopLeft.y, float(r->img.region[0]) / r->img.w, float(r->img.region[1]) / r->img.h)
			};

			RingBufferOffset buffer = getVertexBufferOffset(pTexturedMeshRingBuffer, sizeof(vertices));
			BufferUpdateDesc vbUpdate = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
			updateResource(&vbUpdate);

			PlainRootConstants data = {};
			data.color = float4((float)r->col.r / 255.0f, (float)r->col.g / 255.0f, (float)r->col.b / 255.0f, (float)r->col.a / 255.0f);
			data.scaleBias = { 2.0f / (float)pCmd->mBoundWidth, -2.0f / (float)pCmd->mBoundHeight };
			DescriptorData params[2] = {};
			params[0].pName = "uRootConstants";
			params[0].pRootConstant = &data;
			params[1].pName = "uTex";
			params[1].ppTextures = (Texture**)(&r->img.handle.ptr);
			cmdBindDescriptors(pCmd, pRootSignatureTextured, 2, params);

			if (pCurrentPipeline != pipelinesTextured[PRIMITIVE_TOPO_TRI_STRIP])
			{
				pCurrentPipeline = pipelinesTextured[PRIMITIVE_TOPO_TRI_STRIP];
				cmdBindPipeline(pCmd, pCurrentPipeline);
			}

			cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
			cmdDraw(pCmd, 4, 0);
			break;
		}
		case NK_COMMAND_TEXT:
		{
			const struct nk_command_text *r = (const struct nk_command_text*)cmd;
			float2 vertices[] = { MAKEQUAD(r->x, r->y + fontCalibrationOffset, r->x + r->w, r->y + r->h + fontCalibrationOffset, 0.0f) };

			RingBufferOffset buffer = getVertexBufferOffset(pPlainMeshRingBuffer, sizeof(vertices));
			BufferUpdateDesc vbUpdate = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
			updateResource(&vbUpdate);

			PlainRootConstants data = {};
			data.color = float4((float)r->background.r / 255.0f, (float)r->background.g / 255.0f, (float)r->background.b / 255.0f, (float)r->background.a / 255.0f);
			data.scaleBias = { 2.0f / (float)pCmd->mBoundWidth, -2.0f / (float)pCmd->mBoundHeight };
			DescriptorData params[1] = {};
			params[0].pName = "uRootConstants";
			params[0].pRootConstant = &data;
			cmdBindDescriptors(pCmd, pRootSignaturePlain, 1, params);

			if (pCurrentPipeline != pipelinesPlain[PRIMITIVE_TOPO_TRI_STRIP])
			{
				pCurrentPipeline = pipelinesPlain[PRIMITIVE_TOPO_TRI_STRIP];
				cmdBindPipeline(pCmd, pCurrentPipeline);
			}

			cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
			cmdDraw(pCmd, 4, 0);

			Font* font = (Font*)r->font->userdata.ptr;
			pFontstash->drawText(pCmd, r->string, r->x, r->y + fontCalibrationOffset, font->fontID, *(unsigned int*)&r->foreground, r->font->height, 0.0f, 0.0f);
			pCurrentPipeline = NULL;

			break;
		}
		default:
			break;
		}
	}
}

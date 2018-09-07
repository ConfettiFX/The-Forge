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

#include "../../Middleware_3/Text/Fontstash.h"

#include "../../Common_3/ThirdParty/OpenSource/imgui/imgui.h"
#include "../../Common_3/ThirdParty/OpenSource/imgui/imgui_internal.h"

#include "AppUI.h"

#include "UIShaders.h"

#include "../../Common_3/OS/Interfaces/IOperatingSystem.h"
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/ResourceLoader.h"

#include "../Input/InputSystem.h"
#include "../Input/InputMappings.h"

#include "../../Common_3/OS/Core/RingBuffer.h"

#include "../../Common_3/OS/Interfaces/IMemoryManager.h" //NOTE: this should be the last include in a .cpp

#define LABELID(prop) tinystl::string::format("##%llu", (uint64_t)(prop.pData))

class ImguiGUIDriver : public GUIDriver
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
		class UIProperty* pProps, unsigned int propCount);

	bool onInput(const ButtonData* data);

	// NuklearUI doesn't support touch events.
	void onTouch(const struct TouchEventData* data) {}

	// NuklearUI doesn't support touch events.
	void onTouchMove(const struct TouchEventData* data) {}

protected:
	class _Impl_ImguiGUIDriver* impl;
};

void initGUIDriver(Renderer* pRenderer, GUIDriver** ppDriver)
{
	ImguiGUIDriver* pDriver = conf_placement_new<ImguiGUIDriver>(conf_calloc(1, sizeof(ImguiGUIDriver)));
	pDriver->init(pRenderer);
	*ppDriver = pDriver;
}

void removeGUIDriver(GUIDriver* pDriver)
{
	pDriver->exit();
	(reinterpret_cast<ImguiGUIDriver*>(pDriver))->~ImguiGUIDriver();
	conf_free(pDriver);
}

static void changedProperty(UIProperty* pProp)
{
	if (pProp->pCallback)
		pProp->pCallback(pProp);
}

static float4 ToFloat4Color(uint color)
{
	float4 col;	// Translate colours back by bit shifting
	col.x = (float)((color & 0xFF000000) >> 24);
	col.y = (float)((color & 0x00FF0000) >> 16);
	col.z = (float)((color & 0x0000FF00) >> 8);
	col.w = (float)(color & 0x000000FF);
	return col;
}
static uint ToUintColor(float4 color)
{
	uint c = (((uint)color.x << 24) & 0xFF000000)
		| (((uint)color.y << 16) & 0x00FF0000)
		| (((uint)color.z << 8) & 0x0000FF00)
		| (((uint)color.w) & 0x000000FF);
	return c;
}

static void SetDefaultStyle()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0.4f;
	ImVec4* colors = style.Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
	colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.22f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.40f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.18f, 0.67f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.44f, 0.44f, 0.44f, 0.40f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.46f, 0.47f, 0.48f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.70f, 0.70f, 0.70f, 0.31f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.70f, 0.70f, 0.70f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.48f, 0.50f, 0.52f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.72f, 0.72f, 0.72f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.73f, 0.60f, 0.15f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.87f, 0.87f, 0.87f, 0.35f);
	colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
}
namespace ImGui
{
	bool SliderFloatWithSteps(const char* label, float* v, float v_min, float v_max, float v_step, const char* display_format)
	{
		if (!display_format)
			display_format = "%.3f";

		tinystl::string text_buf = tinystl::string::format(display_format, *v);

		// Map from [v_min,v_max] to [0,N]
		const int countValues = int((v_max - v_min) / v_step);
		int v_i = int((*v - v_min) / v_step);
		const bool value_changed = ImGui::SliderInt(label, &v_i, 0, countValues, text_buf.c_str());

		// Remap from [0,N] to [v_min,v_max]
		*v = v_min + float(v_i) * v_step;
		return value_changed;
	}
}

class _Impl_ImguiGUIDriver
{
public:
	void init(Renderer* renderer)
	{
		pRenderer = renderer;
		/************************************************************************/
		// Rendering resources
		/************************************************************************/
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
		tinystl::string texturedShaderFileVert = "mtl_builtin_textured_imgui_vert";
		tinystl::string texturedShaderFileFrag = "mtl_builtin_textured_imgui_frag";
		tinystl::string texturedShaderVert = mtl_builtin_textured_imgui_vert;
		tinystl::string texturedShaderFrag = mtl_builtin_textured_imgui_frag;
		ShaderDesc texturedShaderDesc = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
		{ texturedShaderFileVert, texturedShaderVert, "stageMain" },
		{ texturedShaderFileFrag, texturedShaderFrag, "stageMain" } };
		addShader(pRenderer, &texturedShaderDesc, &pShaderTextured);
#elif defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11)
		char* pTexturedVert = NULL; uint32_t texturedVertSize = 0;
		char* pTexturedFrag = NULL; uint32_t texturedFragSize = 0;

		if (pRenderer->mSettings.mApi == RENDERER_API_D3D12 ||
			pRenderer->mSettings.mApi == RENDERER_API_XBOX_D3D12 ||
			pRenderer->mSettings.mApi == RENDERER_API_D3D11)
		{
			pTexturedVert = (char*)d3d12_builtin_textured_imgui_vert; texturedVertSize = sizeof(d3d12_builtin_textured_imgui_vert);
			pTexturedFrag = (char*)d3d12_builtin_textured_imgui_frag; texturedFragSize = sizeof(d3d12_builtin_textured_imgui_frag);
		}
		else if (pRenderer->mSettings.mApi == RENDERER_API_VULKAN)
		{
			pTexturedVert = (char*)vk_builtin_textured_imgui_vert; texturedVertSize = sizeof(vk_builtin_textured_imgui_vert);
			pTexturedFrag = (char*)vk_builtin_textured_imgui_frag; texturedFragSize = sizeof(vk_builtin_textured_imgui_frag);
		}

		BinaryShaderDesc texturedShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
		{ (char*)pTexturedVert, texturedVertSize },{ (char*)pTexturedFrag, texturedFragSize } };
		addShaderBinary(pRenderer, &texturedShader, &pShaderTextured);
#endif

		const char* pStaticSamplerNames[] = { "uSampler" };
		RootSignatureDesc textureRootDesc = { &pShaderTextured, 1 };
		textureRootDesc.mStaticSamplerCount = 1;
		textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		textureRootDesc.ppStaticSamplers = &pDefaultSampler;
		addRootSignature(pRenderer, &textureRootDesc, &pRootSignatureTextured);

		BufferDesc vbDesc = {};
		vbDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		vbDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		vbDesc.mVertexStride = sizeof(ImDrawVert);
		vbDesc.mSize = 1024 * 128 * vbDesc.mVertexStride;
		vbDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;

		BufferDesc ibDesc = vbDesc;
		ibDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		ibDesc.mIndexType = INDEX_TYPE_UINT16;
		ibDesc.mSize = 1024 * 1024;
		addMeshRingBuffer(pRenderer, &vbDesc, &ibDesc, &pPlainMeshRingBuffer);

		addUniformRingBuffer(pRenderer, 256 * 10, &pRingBuffer);

		mVertexLayoutTextured.mAttribCount = 3;
		mVertexLayoutTextured.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		mVertexLayoutTextured.mAttribs[0].mFormat = ImageFormat::RG32F;
		mVertexLayoutTextured.mAttribs[0].mBinding = 0;
		mVertexLayoutTextured.mAttribs[0].mLocation = 0;
		mVertexLayoutTextured.mAttribs[0].mOffset = 0;
		mVertexLayoutTextured.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		mVertexLayoutTextured.mAttribs[1].mFormat = ImageFormat::RG32F;
		mVertexLayoutTextured.mAttribs[1].mBinding = 0;
		mVertexLayoutTextured.mAttribs[1].mLocation = 1;
		mVertexLayoutTextured.mAttribs[1].mOffset = calculateImageFormatStride(mVertexLayoutTextured.mAttribs[0].mFormat);
		mVertexLayoutTextured.mAttribs[2].mSemantic = SEMANTIC_COLOR;
		mVertexLayoutTextured.mAttribs[2].mFormat = ImageFormat::RGBA8;
		mVertexLayoutTextured.mAttribs[2].mBinding = 0;
		mVertexLayoutTextured.mAttribs[2].mLocation = 2;
		mVertexLayoutTextured.mAttribs[2].mOffset = mVertexLayoutTextured.mAttribs[1].mOffset + calculateImageFormatStride(mVertexLayoutTextured.mAttribs[1].mFormat);
		/************************************************************************/
		/************************************************************************/
	}

	void exit()
	{
		for (PipelineMap::iterator it = mPipelinesTextured.begin(); it != mPipelinesTextured.end(); ++it)
		{
			removePipeline(pRenderer, it.node->second);
		}

		mPipelinesTextured.clear();

		removeSampler(pRenderer, pDefaultSampler);
		removeBlendState(pBlendAlpha);
		removeDepthState(pDepthState);
		removeRasterizerState(pRasterizerState);
		removeShader(pRenderer, pShaderTextured);
		removeRootSignature(pRenderer, pRootSignatureTextured);
		removeMeshRingBuffer(pPlainMeshRingBuffer);
		removeUniformRingBuffer(pRingBuffer);
	}

	ImGuiContext* context;

	float4 mCurrentWindowRect;

	Texture* pFontTexture;
	
	UIProperty* pProps;
	uint32_t propCount;
	int32_t selectedID;
	int32_t minID;
	int32_t numberOfElements;
	int32_t scrollOffset;
	float2 dpiScale;

	using PipelineMap = tinystl::unordered_map<uint64_t, Pipeline*>;

	Renderer*					pRenderer;
	Shader*						pShaderTextured;
	RootSignature*				pRootSignatureTextured;
	PipelineMap					mPipelinesTextured;
	MeshRingBuffer*				pPlainMeshRingBuffer;
	UniformRingBuffer*			pRingBuffer;
	/// Default states
	BlendState*					pBlendAlpha;
	DepthState*					pDepthState;
	RasterizerState*			pRasterizerState;
	Sampler*					pDefaultSampler;
	VertexLayout				mVertexLayoutTextured = {};
};

bool ImguiGUIDriver::init(Renderer* pRenderer)
{
	impl = conf_placement_new<_Impl_ImguiGUIDriver>(conf_calloc(1, sizeof(_Impl_ImguiGUIDriver)));

	impl->selectedID = 0;
	impl->minID = 0;
	impl->numberOfElements = 8;
	impl->scrollOffset = 3;
	impl->dpiScale = getDpiScale();
	impl->init(pRenderer);

	return true;
}

void ImguiGUIDriver::exit()
{
	impl->exit();
	impl->~_Impl_ImguiGUIDriver();
	conf_free(impl);
}

bool ImguiGUIDriver::load(Fontstash* fontstash, float fontSize, Texture* cursorTexture, float uiwidth, float uiheight)
{
	//// init UI (input)
	impl->context = ImGui::CreateContext();
	ImGui::SetCurrentContext(impl->context);
	// Build and load the texture atlas into a texture
	// (In the examples/ app this is usually done within the ImGui_ImplXXX_Init() function from one of the demo Renderer)
	int width, height;
	unsigned char* pixels = NULL;
	ImFontConfig config = {};
	config.FontDataOwnedByAtlas = false;
	ImFont* font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
		fontstash->getFontBuffer("default"),
		fontstash->getFontBufferSize("default"),
		fontSize * min(impl->dpiScale.x, impl->dpiScale.y),
		&config);
	if (font != NULL)
	{
		ImGui::GetIO().FontDefault = font;
	}
	else
	{
		ImGui::GetIO().Fonts->AddFontDefault();
	}
	ImGui::GetIO().Fonts->Build();
	ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	// At this point you've got the texture data and you need to upload that your your graphic system:
	// After we have created the texture, store its pointer/identifier (_in whichever format your engine uses_) in 'io.Fonts->TexID'.
	// This will be passed back to your via the renderer. Basically ImTextureID == void*. Read FAQ below for details about ImTextureID.
	Image image = {};
	image.RedefineDimensions(ImageFormat::RGBA8, width, height, 1, 1, 1);
	image.SetPixels(pixels);
	TextureLoadDesc loadDesc = {};
	loadDesc.pImage = &image;
	loadDesc.ppTexture = &impl->pFontTexture;
	addResource(&loadDesc);
	ImGui::GetIO().Fonts->TexID = (void*)impl->pFontTexture;

	SetDefaultStyle();

	ImGuiIO& io = ImGui::GetIO();
	//io.KeyMap[ImGuiKey_Tab] = VK_TAB;
	//io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
	//io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
	//io.KeyMap[ImGuiKey_Home] = VK_HOME;
	//io.KeyMap[ImGuiKey_End] = VK_END;
	//io.KeyMap[ImGuiKey_Insert] = VK_INSERT;
	//io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
	io.KeyMap[ImGuiKey_LeftArrow] = KEY_PAD_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = KEY_PAD_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = KEY_PAD_UP;
	io.KeyMap[ImGuiKey_DownArrow] = KEY_PAD_DOWN;
	io.KeyMap[ImGuiKey_Backspace] = KEY_RIGHT_STICK_BUTTON;
	io.KeyMap[ImGuiKey_Space] = KEY_LEFT_TRIGGER;
	io.KeyMap[ImGuiKey_Enter] = KEY_MENU;
	io.KeyMap[ImGuiKey_Escape] = KEY_CANCEL;
	io.KeyMap[ImGuiKey_A] = 'A';
	io.KeyMap[ImGuiKey_C] = 'C';
	io.KeyMap[ImGuiKey_V] = 'V';
	io.KeyMap[ImGuiKey_X] = 'X';
	io.KeyMap[ImGuiKey_Y] = 'Y';
	io.KeyMap[ImGuiKey_Z] = 'Z';

	return true;
}

void ImguiGUIDriver::unload()
{
	removeResource(impl->pFontTexture);
	ImGui::DestroyContext(impl->context);
}

void* ImguiGUIDriver::getContext()
{
	return impl->context;
}

bool ImguiGUIDriver::onInput(const ButtonData* data)
{
	ImGui::SetCurrentContext(impl->context);
	ImGuiIO& io = ImGui::GetIO();
	io.NavActive = true;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	if (GAINPUT_GAMEPAD & data->mActiveDevicesMask)
	{
		io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
		return false;
	}

	if (data->mUserId == KEY_UI_MOVE)
	{
		io.MousePos = ImVec2((float)data->mValue[0], (float)data->mValue[1]);
		return ImGui::IsMouseHoveringAnyWindow() || ImGui::IsMouseHoveringRect(
			ImVec2(impl->mCurrentWindowRect.x, impl->mCurrentWindowRect.y),
			ImVec2(impl->mCurrentWindowRect.x + impl->mCurrentWindowRect.z,
				impl->mCurrentWindowRect.y + impl->mCurrentWindowRect.w), false);
	}
	else if (data->mUserId == KEY_CONFIRM || data->mUserId == KEY_RIGHT_BUMPER)
	{
		uint32_t index = data->mUserId == KEY_CONFIRM
			? 0
			: (data->mUserId == KEY_RIGHT_BUMPER ? 1 : 2);
		io.MouseDown[index] = data->mIsPressed;
		io.MousePos = ImVec2((float)data->mValue[0], (float)data->mValue[1]);
		return ImGui::IsMouseHoveringAnyWindow() || ImGui::IsMouseHoveringRect(
			ImVec2(impl->mCurrentWindowRect.x, impl->mCurrentWindowRect.y),
			ImVec2(impl->mCurrentWindowRect.x + impl->mCurrentWindowRect.z,
				impl->mCurrentWindowRect.y + impl->mCurrentWindowRect.w), false);
	}
	else if (KEY_CHAR == data->mUserId)
	{
		if (data->mIsPressed)
			io.AddInputCharacter(data->mCharacter);
		io.KeysDown[data->mCharacter] = data->mIsPressed;
	}
	else if (KEY_LEFT_STICK == data->mUserId)
	{
		io.KeysDown['A'] = data->mIsPressed;
	}
	else if (KEY_RIGHT_STICK_BUTTON == data->mUserId)
	{
		io.KeysDown[data->mUserId] = data->mIsPressed;
	}
	else if (KEY_RIGHT_CTRL == data->mUserId || KEY_LEFT_CTRL == data->mUserId)
	{
		io.KeysDown[data->mUserId] = data->mIsPressed;
		io.KeyCtrl = data->mIsPressed;
	}
	else if (KEY_RIGHT_SHIFT == data->mUserId || KEY_LEFT_BUMPER == data->mUserId)
	{
		io.KeysDown[data->mUserId] = data->mIsPressed;
		io.KeyShift = data->mIsPressed;
	}
	else if (KEY_PAD_LEFT == data->mUserId ||
		KEY_PAD_RIGHT == data->mUserId ||
		KEY_PAD_UP == data->mUserId ||
		KEY_PAD_DOWN == data->mUserId)
	{
		io.KeysDown[data->mUserId] = data->mIsPressed;
	}

	return false;
}

// populates the UI library's command buffer for drawing UI elements, which is later processed by TheForge in draw()
void draw_control(UIProperty* pControls, uint numControls, uint idxCurrControl, const char* pTitle)
{
	UIProperty& prop = pControls[idxCurrControl];

	if (!(prop.mFlags & UIProperty::FLAG_VISIBLE))
		return;

	if (!prop.pData)
		return;

	ImColor col = ImColor(
		(float)((prop.mColor & 0xFF000000) >> 24),
		(float)((prop.mColor & 0x00FF0000) >> 16),
		(float)((prop.mColor & 0x0000FF00) >> 8),
		(float)((prop.mColor & 0x000000FF))
	);

	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_Text] = col;

	switch (prop.mType)
	{
	case UI_CONTROL_LABEL:
	{
		ImGui::Text("%s", prop.mText.c_str());
		break;
	}
	case UI_CONTROL_SLIDER_FLOAT:
	{
		float& currentValue = *(float*)prop.pData;
		float oldValue = currentValue;
		ImGui::Text("%s", prop.mText.c_str());
		if (ImGui::SliderFloatWithSteps(LABELID(prop), (float*)prop.pData, prop.mSettings.fMin, prop.mSettings.fMax, prop.mSettings.fIncrement, NULL))
		{
			if (currentValue != oldValue)
			{
				changedProperty(&prop);
			}
		}
		break;
	}
	case UI_CONTROL_SLIDER_INT:
	{
		int& currentValue = *(int*)prop.pData;
		int oldValue = currentValue;
		ImGui::LabelText("", "%s", prop.mText.c_str());
		if (ImGui::SliderInt(LABELID(prop), (int*)prop.pData, prop.mSettings.iMin, prop.mSettings.iMax))
		{
			if (currentValue != oldValue)
			{
				changedProperty(&prop);
			}
		}
		break;
	}
	case UI_CONTROL_SLIDER_UINT:
	{
		int& currentValue = *(int*)prop.pData;
		int oldValue = currentValue;
		ImGui::LabelText("", "%s", prop.mText.c_str());
		if (ImGui::SliderInt(LABELID(prop), (int*)prop.pData, prop.mSettings.uiMin, prop.mSettings.uiMax))
		{
			if (currentValue != oldValue)
			{
				changedProperty(&prop);
			}
		}
		break;
	}
	case UI_CONTROL_CHECKBOX:
	{
		//bool& currentValue = *(bool*)prop.pData;
		//int value = (currentValue) ? 0 : 1;
		ImGui::LabelText("", "%s", prop.mText.c_str());
		if (ImGui::Checkbox(LABELID(prop), (bool*)prop.pData))
		{
			changedProperty(&prop);
		}
		break;
	}
	case UI_CONTROL_DROPDOWN:
	{
		ASSERT(prop.mSettings.eByteSize == 4);

		int current = prop.enumComputeIndex();
		int previous = current;
		//int cnt = 0;

		ImGui::LabelText("", "%s", prop.mText.c_str());
		if (ImGui::BeginCombo(LABELID(prop), prop.mSettings.eNames[current]))
		{
			for (int vi = 0; prop.mSettings.eNames[vi] != 0; vi++)
			{
				if (ImGui::Selectable(prop.mSettings.eNames[vi]))
					current = vi;
			}
			ImGui::EndCombo();

			if (previous != current)
			{
				*(int*)prop.pData = ((int*)prop.mSettings.eValues)[current];
				changedProperty(&prop);
			}
		}
		break;
	}
	case UI_CONTROL_BUTTON:
	{
		if (ImGui::Button(prop.mText))
		{
			if (prop.pData)
				((UIButtonFn)prop.pData)(prop.mSettings.pUserData);
			changedProperty(&prop);
		}
		break;
	}
	case UI_CONTROL_RADIO_BUTTON:
	{
		bool& bCurrValue = *(bool*)prop.pData;
		//const int valueOld = (bCurrValue) ? 0 : 1;
		//int value = valueOld;

		if (ImGui::RadioButton(prop.mText, bCurrValue))
		{
			bCurrValue = true;
			changedProperty(&prop);

			// When a radio button is toggled on, other radio buttons 
			// in the same  UI group should be toggled off.
			for (uint32_t i = 0; i < numControls; ++i)
			{
				if (pControls[i].mType != UI_CONTROL_RADIO_BUTTON)
					continue;	// skip non-radio buttons

				if (&pControls[i] == &prop)
					continue;	// skip self

				*((bool*)pControls[i].pData) = false;
			}
		}
		break;
	}
	case UI_CONTROL_PROGRESS_BAR:
	{
		size_t* pCurrProgress = (size_t*)prop.pData;
		ImGui::Text("%s", prop.mText.c_str());
		ImGui::ProgressBar((float)(*pCurrProgress) / prop.mSettings.maxProgress);
		break;
	}
	case UI_CONTROL_COLOR_SLIDER:
	{
		uint& colorPick = *(uint*)prop.pData;
		float4 combo_color = ToFloat4Color(colorPick) / 255.0f;

		float col[4] = { combo_color.x, combo_color.y, combo_color.z, combo_color.w };
		ImGui::Text("%s", prop.mText.c_str());
		if (ImGui::ColorEdit4(LABELID(prop), col))
		{
			if (col[0] != combo_color.x || col[1] != combo_color.y || col[2] != combo_color.z || col[3] != combo_color.w)
			{
				combo_color = col;
				colorPick = ToUintColor(combo_color * 255.0f);
			}
		}
		break;
	}
	case UI_CONTROL_COLOR_PICKER:
	{
		uint& colorPick = *(uint*)prop.pData;
		float4 combo_color = ToFloat4Color(colorPick) / 255.0f;

		float col[4] = { combo_color.x, combo_color.y, combo_color.z, combo_color.w };
		ImGui::Text("%s", prop.mText.c_str());
		if (ImGui::ColorPicker4(LABELID(prop), col))
		{
			if (col[0] != combo_color.x || col[1] != combo_color.y || col[2] != combo_color.z || col[3] != combo_color.w)
			{
				combo_color = col;
				colorPick = ToUintColor(combo_color * 255.0f);
			}
		}
		break;
	}
	case UI_CONTROL_CONTEXTUAL:
	{
		if (ImGui::BeginPopupContextItem(pTitle))
		{
			for (int item = 0; item < prop.mSettings.numContextItems; ++item)
			{
				if (ImGui::MenuItem(prop.mSettings.pContextItems[item]))
				{
					if (prop.mSettings.pfnCallbacks && prop.mSettings.pfnCallbacks[item])
						prop.mSettings.pfnCallbacks[item]();	// callback fn
				}
			}
			ImGui::EndPopup();
		}
		break;
	}
	case UI_CONTROL_TEXTBOX:
	{
		if (ImGui::InputText(LABELID(prop), (char*)prop.pData, prop.mSettings.sLen, ImGuiInputTextFlags_AutoSelectAll))
		{
			changedProperty(&prop);
		}
		break;
	}
	//Not handled yet.
	case UI_CONTROL_MENU: {
		break;
	}
	}
}

void ImguiGUIDriver::draw(Cmd* pCmd, float deltaTime,
	const char* pTitle,
	float x, float y, float w, float h,
	class UIProperty* pProps, unsigned int propCount)
{
	impl->pProps = pProps;
	impl->propCount = (uint32_t)propCount;

	ImGui::SetCurrentContext(impl->context);
	// #TODO: Use window size as render-target size cannot be trusted to be the same as window size
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize.x = (float)pCmd->mBoundWidth;
	io.DisplaySize.y = (float)pCmd->mBoundHeight;
	io.DeltaTime = deltaTime;

	io.NavInputs[ImGuiNavInput_Activate] = InputSystem::GetButtonData(KEY_CONFIRM).mIsPressed ? 1.0f : 0.0f;
	io.NavInputs[ImGuiNavInput_Cancel] = InputSystem::GetButtonData(KEY_CANCEL).mIsPressed ? 1.0f : 0.0f;
	io.NavInputs[ImGuiNavInput_Menu] = InputSystem::GetButtonData(KEY_BUTTON_X).mIsPressed ? 1.0f : 0.0f;
	io.NavInputs[ImGuiNavInput_Input] = InputSystem::GetButtonData(KEY_BUTTON_Y).mIsPressed ? 1.0f : 0.0f;
	io.NavInputs[ImGuiNavInput_DpadLeft] = InputSystem::GetButtonData(KEY_PAD_LEFT).mIsPressed ? 1.0f : 0.0f;
	io.NavInputs[ImGuiNavInput_DpadRight] = InputSystem::GetButtonData(KEY_PAD_RIGHT).mIsPressed ? 1.0f : 0.0f;
	io.NavInputs[ImGuiNavInput_DpadUp] = InputSystem::GetButtonData(KEY_PAD_UP).mIsPressed ? 1.0f : 0.0f;
	io.NavInputs[ImGuiNavInput_DpadDown] = InputSystem::GetButtonData(KEY_PAD_DOWN).mIsPressed ? 1.0f : 0.0f;
	io.NavInputs[ImGuiNavInput_FocusNext] = InputSystem::GetButtonData(KEY_RIGHT_BUMPER).mIsPressed ? 1.0f : 0.0f;
	io.NavInputs[ImGuiNavInput_FocusPrev] = InputSystem::GetButtonData(KEY_LEFT_BUMPER).mIsPressed ? 1.0f : 0.0f;
	io.NavInputs[ImGuiNavInput_TweakFast] = InputSystem::GetButtonData(KEY_RIGHT_BUMPER).mIsPressed ? 1.0f : 0.0f;
	io.NavInputs[ImGuiNavInput_TweakSlow] = InputSystem::GetButtonData(KEY_LEFT_BUMPER).mIsPressed ? 1.0f : 0.0f;
	ImGui::NewFrame();
	/************************************************************************/
	// Draw window
	/************************************************************************/
	//ImGui::ShowDemoWindow();
	bool result = ImGui::Begin(pTitle, NULL, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::SetWindowSize(ImVec2(w * impl->dpiScale.x, h * impl->dpiScale.y), ImGuiCond_Once);
	ImGui::SetWindowPos(ImVec2(x * impl->dpiScale.x, y * impl->dpiScale.y), ImGuiCond_Once);

	ImVec2 min = ImGui::GetWindowPos();
	ImVec2 max = ImGui::GetWindowSize();
	impl->mCurrentWindowRect.x = min.x;
	impl->mCurrentWindowRect.y = min.y;
	impl->mCurrentWindowRect.z = max.x;
	impl->mCurrentWindowRect.w = max.y;

	// Trees are indexed based on name with a list of the properties part of the tree
	tinystl::unordered_map<const char*, tinystl::vector<UIProperty> > map;

	typedef tinystl::unordered_map<const char*, tinystl::vector<UIProperty>> UIControlMap;
	typedef tinystl::unordered_hash_iterator<tinystl::unordered_hash_node<char const*, tinystl::vector<UIProperty>>> UIControlMapIterator;

	if (result)
	{
		// TODO: do this once... no need for doing it every frame.
		UIControlMap map;
		tinystl::vector<UIProperty> nonTreeControls;
		for (uint32_t i = 0; i < propCount; ++i)
		{
			// If no tree, just add the ui control
			if (strcmp(pProps[i].pTree, "none") != 0)
			{
				map[pProps[i].pTree].push_back(pProps[i]);
			}
			else
			{
				nonTreeControls.push_back(pProps[i]);
			}
		}

		const uint numNonTreeCtrls = (uint)nonTreeControls.size();
		for (uint32_t i = 0; i < numNonTreeCtrls; ++i)
		{
			draw_control(nonTreeControls.data(), propCount, i, pTitle);
		}

		// Go through all the trees and add the properties for every tree 
		for (tinystl::unordered_hash_iterator<tinystl::unordered_hash_node<char const*, tinystl::vector<UIProperty>>> local_it = map.begin(); local_it != map.end(); ++local_it)
		{
			// begin
			if (ImGui::CollapsingHeader(local_it.node->first))
			{
				for (int i = 0; i < local_it.node->second.size(); ++i)
				{
					draw_control(local_it.node->second.data(), (uint)local_it.node->second.size(), i, pTitle);
				}
			}
		}
	}

	ImGui::End();
	/************************************************************************/
	/************************************************************************/
	ImGui::EndFrame();
	ImGui::Render();

	ImDrawData* draw_data = ImGui::GetDrawData();

	Pipeline* pPipeline = NULL;
	GraphicsPipelineDesc pipelineDesc = {};
	pipelineDesc.mDepthStencilFormat = (ImageFormat::Enum)pCmd->mBoundDepthStencilFormat;
	pipelineDesc.mRenderTargetCount = pCmd->mBoundRenderTargetCount;
	pipelineDesc.mSampleCount = pCmd->mBoundSampleCount;
	pipelineDesc.pBlendState = impl->pBlendAlpha;
	pipelineDesc.mSampleQuality = pCmd->mBoundSampleQuality;
	pipelineDesc.pColorFormats = (ImageFormat::Enum*)pCmd->pBoundColorFormats;
	pipelineDesc.pDepthState = impl->pDepthState;
	pipelineDesc.pRasterizerState = impl->pRasterizerState;
	pipelineDesc.pSrgbValues = pCmd->pBoundSrgbValues;
	_Impl_ImguiGUIDriver::PipelineMap::iterator it = impl->mPipelinesTextured.find(pCmd->mRenderPassHash);
	if (it == impl->mPipelinesTextured.end())
	{
		pipelineDesc.pRootSignature = impl->pRootSignatureTextured;
		pipelineDesc.pShaderProgram = impl->pShaderTextured;
		pipelineDesc.pVertexLayout = &impl->mVertexLayoutTextured;
		pipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		addPipeline(pCmd->pRenderer, &pipelineDesc, &pPipeline);
		impl->mPipelinesTextured.insert({ pCmd->mRenderPassHash, pPipeline });
	}
	else
	{
		pPipeline = it.node->second;
	}
	uint32_t vSize = 0;
	uint32_t iSize = 0;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		vSize += (cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		iSize += (cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
	}

	RingBufferOffset vOffset = getVertexBufferOffset(impl->pPlainMeshRingBuffer, vSize);
	RingBufferOffset iOffset = getIndexBufferOffset(impl->pPlainMeshRingBuffer, iSize);
	// Copy and convert all vertices into a single contiguous buffer
	uint64_t vtx_dst = vOffset.mOffset;
	uint64_t idx_dst = iOffset.mOffset;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		BufferUpdateDesc update = { vOffset.pBuffer, cmd_list->VtxBuffer.Data, 0, vtx_dst, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert) };
		updateResource(&update);
		update = { iOffset.pBuffer, cmd_list->IdxBuffer.Data, 0, idx_dst, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx) };
		updateResource(&update);

		vtx_dst += (cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		idx_dst += (cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
	}

	float L = draw_data->DisplayPos.x;
	float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
	float T = draw_data->DisplayPos.y;
	float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
	float mvp[4][4] =
	{
		{ 2.0f / (R - L),   0.0f,           0.0f,       0.0f },
		{ 0.0f,         2.0f / (T - B),     0.0f,       0.0f },
		{ 0.0f,         0.0f,           0.5f,       0.0f },
		{ (R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f },
	};
	RingBufferOffset offset = getUniformBufferOffset(impl->pRingBuffer, sizeof(mvp));
	BufferUpdateDesc update = { offset.pBuffer, mvp, 0, offset.mOffset, sizeof(mvp) };
	updateResource(&update);

	cmdSetViewport(pCmd, 0.0f, 0.0f, draw_data->DisplaySize.x, draw_data->DisplaySize.y, 0.0f, 1.0f);
	cmdSetScissor(pCmd, (uint32_t)draw_data->DisplayPos.x, (uint32_t)draw_data->DisplayPos.y, (uint32_t)draw_data->DisplaySize.x, (uint32_t)draw_data->DisplaySize.y);
	cmdBindPipeline(pCmd, pPipeline);
	cmdBindIndexBuffer(pCmd, impl->pPlainMeshRingBuffer->pIndexBuffer, iOffset.mOffset);
	cmdBindVertexBuffer(pCmd, 1, &impl->pPlainMeshRingBuffer->pVertexBuffer, &vOffset.mOffset);

	DescriptorData params[1] = {};
	params[0].pName = "uniformBlockVS";
	params[0].pOffsets = &offset.mOffset;
	params[0].ppBuffers = &offset.pBuffer;
	cmdBindDescriptors(pCmd, impl->pRootSignatureTextured, 1, params);

	// Render command lists
	int vtx_offset = 0;
	int idx_offset = 0;
	ImVec2 pos = draw_data->DisplayPos;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback)
			{
				// User callback (registered via ImDrawList::AddCallback)
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				// Apply scissor/clipping rectangle
				//const D3D12_RECT r = { (LONG)(pcmd->ClipRect.x - pos.x), (LONG)(pcmd->ClipRect.y - pos.y), (LONG)(pcmd->ClipRect.z - pos.x), (LONG)(pcmd->ClipRect.w - pos.y) };
				//pCmd->pDxCmdList->RSSetScissorRects(1, &r);
				cmdSetScissor(pCmd, (uint32_t)(pcmd->ClipRect.x - pos.x), (uint32_t)(pcmd->ClipRect.y - pos.y),
					(uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x), (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y));

				DescriptorData params[1] = {};
				params[0].pName = "uTex";
				params[0].ppTextures = (Texture**)&pcmd->TextureId;
				cmdBindDescriptors(pCmd, impl->pRootSignatureTextured, 1, params);
				cmdDrawIndexed(pCmd, pcmd->ElemCount, idx_offset, vtx_offset);
			}
			idx_offset += pcmd->ElemCount;
		}
		vtx_offset += cmd_list->VtxBuffer.Size;
	}
}

#ifndef DGA_INPUT
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

#include "../Graphics/GraphicsConfig.h"

#include "../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

#include "../Application/ThirdParty/OpenSource/gainput/lib/include/gainput/gainput.h"

#if defined(__ANDROID__) || defined(NX64)
#include "../Application/ThirdParty/OpenSource/gainput/lib/include/gainput/GainputInputDeltaState.h"
#endif

#ifdef __APPLE__
#ifdef TARGET_IOS
#include "../Application/ThirdParty/OpenSource/gainput/lib/include/gainput/GainputIos.h"
#else
#include "../Application/ThirdParty/OpenSource/gainput/lib/include/gainput/GainputMac.h"
#endif
#endif

#ifdef __linux__
#include <climits>
#endif

#ifdef METAL
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#ifdef TARGET_IOS
#include <UIKit/UIView.h>
#include "../Application/ThirdParty/OpenSource/gainput/lib/source/gainput/touch/GainputInputDeviceTouchIos.h"
#else
#import <Cocoa/Cocoa.h>
#endif
#endif

#include "../Graphics/Interfaces/IGraphics.h"
#include "../Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

#include "../Utilities/Interfaces/ILog.h"
#include "Interfaces/IUI.h"
#include "../OS/Interfaces/IOperatingSystem.h"
#include "../Utilities/Interfaces/IFileSystem.h"
#include "Interfaces/IInput.h"
#include "../Utilities/Interfaces/IMemory.h"

#ifdef GAINPUT_PLATFORM_GGP
namespace gainput {
extern void SetWindow(void* pData);
}
#endif

#if (defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)) && !defined(QUEST_VR)
#define TOUCH_INPUT 1
#endif

#if TOUCH_INPUT
#define TOUCH_DOWN(id) (((id) << 2) + 0)
#define TOUCH_X(id) (((id) << 2) + 1)
#define TOUCH_Y(id) (((id) << 2) + 2)
#define TOUCH_PRESSURE(id) (((id) << 2) + 3)
#define TOUCH_USER(btn) ((btn) >> 2)
#define TOUCH_AXIS(btn) (((btn) % 4) - 1)
#endif

#define MAX_DEVICES 16U

/**********************************************/
// VirtualJoystick
/**********************************************/

typedef struct VirtualJoystickDesc
{

	Renderer* pRenderer;
	const char* pJoystickTexture;

} VirtualJoystickDesc;

typedef struct VirtualJoystick
{
#if TOUCH_INPUT
	Renderer*      pRenderer = NULL;
	Shader*        pShader = NULL;
	RootSignature* pRootSignature = NULL;
	DescriptorSet* pDescriptorSet = NULL;
	Pipeline*      pPipeline = NULL;
	Texture*       pTexture = NULL;
	Sampler*       pSampler = NULL;
	Buffer*        pMeshBuffer = NULL;
	float2         mRenderSize = float2(0.f, 0.f);

	//input related
	float          mInsideRadius = 100.f;
	float          mOutsideRadius = 200.f;
	uint32_t       mRootConstantIndex;

	struct StickInput
	{
		bool   mPressed = false;
		float2 mStartPos = float2(0.f, 0.f);
		float2 mCurrPos = float2(0.f, 0.f);
	};
	// Left -> Index 0
	// Right -> Index 1
	StickInput       mSticks[2];
#endif
} VirtualJoystick;

static VirtualJoystick* pVirtualJoystick = NULL; 

void initVirtualJoystick(VirtualJoystickDesc* pDesc, VirtualJoystick** ppVirtualJoystick)
{
	ASSERT(ppVirtualJoystick);
	ASSERT(pVirtualJoystick == NULL);

	pVirtualJoystick = tf_new(VirtualJoystick);

#if TOUCH_INPUT
	Renderer* pRenderer = (Renderer*)pDesc->pRenderer;
	pVirtualJoystick->pRenderer = pRenderer;

	TextureLoadDesc loadDesc = {};
	SyncToken token = {};
	loadDesc.pFileName = pDesc->pJoystickTexture;
	loadDesc.ppTexture = &pVirtualJoystick->pTexture;
	// Textures representing color should be stored in SRGB or HDR format
	loadDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
	addResource(&loadDesc, &token);
	waitForToken(&token);

	if (!pVirtualJoystick->pTexture)
	{
		LOGF(LogLevel::eWARNING, "Could not load virtual joystick texture file: %s", pDesc->pJoystickTexture);
		tf_delete(pVirtualJoystick);
		pVirtualJoystick = NULL; 
		return;
	}
	/************************************************************************/
	// States
	/************************************************************************/
	SamplerDesc samplerDesc = { FILTER_LINEAR,
								FILTER_LINEAR,
								MIPMAP_MODE_NEAREST,
								ADDRESS_MODE_CLAMP_TO_EDGE,
								ADDRESS_MODE_CLAMP_TO_EDGE,
								ADDRESS_MODE_CLAMP_TO_EDGE };
	addSampler(pRenderer, &samplerDesc, &pVirtualJoystick->pSampler);
	/************************************************************************/
	// Resources
	/************************************************************************/
	BufferLoadDesc vbDesc = {};
	vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	vbDesc.mDesc.mSize = 128 * 4 * sizeof(float4);
	vbDesc.ppBuffer = &pVirtualJoystick->pMeshBuffer;
	addResource(&vbDesc, NULL);
#endif

	// Joystick is good!
	*ppVirtualJoystick = pVirtualJoystick;
}

void exitVirtualJoystick(VirtualJoystick** ppVirtualJoystick)
{
	ASSERT(ppVirtualJoystick);
	VirtualJoystick* pVirtualJoystick = *ppVirtualJoystick;
	if (!pVirtualJoystick)
		return; 

#if TOUCH_INPUT
	removeSampler(pVirtualJoystick->pRenderer, pVirtualJoystick->pSampler);
	removeResource(pVirtualJoystick->pMeshBuffer);
	removeResource(pVirtualJoystick->pTexture);
#endif

	tf_delete(pVirtualJoystick);
	*ppVirtualJoystick = NULL;
}

bool loadVirtualJoystick(ReloadType loadType, TinyImageFormat colorFormat, uint32_t width, uint32_t height)
{
#if TOUCH_INPUT
	if (!pVirtualJoystick)
	{
		return false;
	}

	if (loadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
	{
		Renderer* pRenderer = pVirtualJoystick->pRenderer;

		if (loadType & RELOAD_TYPE_SHADER)
		{
			/************************************************************************/
			// Shader
			/************************************************************************/
			ShaderLoadDesc texturedShaderDesc = {};
			texturedShaderDesc.mStages[0] = { "textured_mesh.vert", NULL, 0 };
			texturedShaderDesc.mStages[1] = { "textured_mesh.frag", NULL, 0 };
			addShader(pRenderer, &texturedShaderDesc, &pVirtualJoystick->pShader);

			const char* pStaticSamplerNames[] = { "uSampler" };
			RootSignatureDesc textureRootDesc = { &pVirtualJoystick->pShader, 1 };
			textureRootDesc.mStaticSamplerCount = 1;
			textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
			textureRootDesc.ppStaticSamplers = &pVirtualJoystick->pSampler;
			addRootSignature(pRenderer, &textureRootDesc, &pVirtualJoystick->pRootSignature);
			pVirtualJoystick->mRootConstantIndex = getDescriptorIndexFromName(pVirtualJoystick->pRootSignature, "uRootConstants");

			DescriptorSetDesc descriptorSetDesc = { pVirtualJoystick->pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
			addDescriptorSet(pRenderer, &descriptorSetDesc, &pVirtualJoystick->pDescriptorSet);
			/************************************************************************/
			// Prepare descriptor sets
			/************************************************************************/
			DescriptorData params[1] = {};
			params[0].pName = "uTex";
			params[0].ppTextures = &pVirtualJoystick->pTexture;
			updateDescriptorSet(pRenderer, 0, pVirtualJoystick->pDescriptorSet, 1, params);
		}

		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = TinyImageFormat_BitSizeOfBlock(TinyImageFormat_R32G32_SFLOAT) / 8;

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.mSrcFactors[0] = BC_SRC_ALPHA;
		blendStateDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.mSrcAlphaFactors[0] = BC_SRC_ALPHA;
		blendStateDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.mMasks[0] = ALL;
		blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
		blendStateDesc.mIndependentBlend = false;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = false;
		depthStateDesc.mDepthWrite = false;

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		rasterizerStateDesc.mScissor = true;

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineDesc = desc.mGraphicsDesc;
		pipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_STRIP;
		pipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
		pipelineDesc.mRenderTargetCount = 1;
		pipelineDesc.mSampleCount = SAMPLE_COUNT_1;
		pipelineDesc.mSampleQuality = 0;
		pipelineDesc.pBlendState = &blendStateDesc;
		pipelineDesc.pColorFormats = &colorFormat;
		pipelineDesc.pDepthState = &depthStateDesc;
		pipelineDesc.pRasterizerState = &rasterizerStateDesc;
		pipelineDesc.pRootSignature = pVirtualJoystick->pRootSignature;
		pipelineDesc.pShaderProgram = pVirtualJoystick->pShader;
		pipelineDesc.pVertexLayout = &vertexLayout;
		addPipeline(pVirtualJoystick->pRenderer, &desc, &pVirtualJoystick->pPipeline);
	}

	if (loadType & RELOAD_TYPE_RESIZE)
	{
		pVirtualJoystick->mRenderSize[0] = (float)width;
		pVirtualJoystick->mRenderSize[1] = (float)height;
	}
#endif
	return true;
}

void unloadVirtualJoystick(ReloadType unloadType)
{
#if TOUCH_INPUT
	if (!pVirtualJoystick)
	{
		return;
	}

	if (unloadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
	{
		Renderer* pRenderer = pVirtualJoystick->pRenderer;

		removePipeline(pRenderer, pVirtualJoystick->pPipeline);

		if (unloadType & RELOAD_TYPE_SHADER)
		{
			removeDescriptorSet(pRenderer, pVirtualJoystick->pDescriptorSet);
			removeRootSignature(pRenderer, pVirtualJoystick->pRootSignature);
			removeShader(pRenderer, pVirtualJoystick->pShader);
		}
	}
#endif
}

void drawVirtualJoystick(Cmd* pCmd, const float4* color)
{
#if TOUCH_INPUT
	if (!pVirtualJoystick || !(pVirtualJoystick->mSticks[0].mPressed || pVirtualJoystick->mSticks[1].mPressed))
		return;

	struct RootConstants
	{
		float4 color;
		float2 scaleBias;
	} data = {};


	cmdSetViewport(pCmd, 0.0f, 0.0f, pVirtualJoystick->mRenderSize[0], pVirtualJoystick->mRenderSize[1], 0.0f, 1.0f);
	cmdSetScissor(
		pCmd, 
		0u, 0u, 
		(uint32_t)pVirtualJoystick->mRenderSize[0],	(uint32_t)pVirtualJoystick->mRenderSize[1]);

	cmdBindPipeline(pCmd, pVirtualJoystick->pPipeline);
	cmdBindDescriptorSet(pCmd, 0, pVirtualJoystick->pDescriptorSet);
	data.color = *color;
	data.scaleBias = { 2.0f / (float)pVirtualJoystick->mRenderSize[0], -2.0f / (float)pVirtualJoystick->mRenderSize[1] };
	cmdBindPushConstants(pCmd, pVirtualJoystick->pRootSignature, pVirtualJoystick->mRootConstantIndex, &data);

	// Draw the camera controller's virtual joysticks.
	float extSide = pVirtualJoystick->mOutsideRadius;
	float intSide = pVirtualJoystick->mInsideRadius;

	uint64_t bufferOffset = 0;
	for (uint i = 0; i < 2; i++)
	{
		if (pVirtualJoystick->mSticks[i].mPressed)
		{
			float2 joystickSize = float2(extSide);
			float2 joystickCenter = pVirtualJoystick->mSticks[i].mStartPos - float2(0.0f, pVirtualJoystick->mRenderSize.y * 0.1f);
			float2 joystickPos = joystickCenter - joystickSize * 0.5f;

			const uint32_t vertexStride = sizeof(float4);
			BufferUpdateDesc updateDesc = { pVirtualJoystick->pMeshBuffer, bufferOffset };
			beginUpdateResource(&updateDesc);
			TexVertex* vertices = (TexVertex*)updateDesc.pMappedData;
			// the last variable can be used to create a border
			MAKETEXQUAD(vertices, joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0);
			endUpdateResource(&updateDesc, NULL);
			cmdBindVertexBuffer(pCmd, 1, &pVirtualJoystick->pMeshBuffer, &vertexStride, &bufferOffset);
			cmdDraw(pCmd, 4, 0);
			bufferOffset += sizeof(TexVertex) * 4;

			joystickSize = float2(intSide);
			joystickCenter = pVirtualJoystick->mSticks[i].mCurrPos - float2(0.0f, pVirtualJoystick->mRenderSize.y * 0.1f);
			joystickPos = float2(joystickCenter.getX(), joystickCenter.getY()) - 0.5f * joystickSize;

			updateDesc = { pVirtualJoystick->pMeshBuffer, bufferOffset };
			beginUpdateResource(&updateDesc);
			TexVertex* verticesInner = (TexVertex*)updateDesc.pMappedData;
			// the last variable can be used to create a border
			MAKETEXQUAD(verticesInner, joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0);
			endUpdateResource(&updateDesc, NULL);
			cmdBindVertexBuffer(pCmd, 1, &pVirtualJoystick->pMeshBuffer, &vertexStride, &bufferOffset);
			cmdDraw(pCmd, 4, 0);
			bufferOffset += sizeof(TexVertex) * 4;
		}
	}
#endif
}

void virtualJoystickOnMove(VirtualJoystick* pVirtualJoystick, uint32_t id, InputActionContext* ctx)
{
#if TOUCH_INPUT
	if (!ctx->pPosition) return;
	
	if (*ctx->pCaptured
#ifdef ENABLE_FORGE_UI
		&& !uiIsFocused()
#endif
		)
	{
		if (!pVirtualJoystick->mSticks[id].mPressed)
		{
			pVirtualJoystick->mSticks[id].mStartPos = *ctx->pPosition;
			pVirtualJoystick->mSticks[id].mCurrPos = *ctx->pPosition;
		}
		else
		{
			pVirtualJoystick->mSticks[id].mCurrPos = *ctx->pPosition;
		}
		pVirtualJoystick->mSticks[id].mPressed = ctx->mPhase != INPUT_ACTION_PHASE_CANCELED;
	}
#endif
}
#endif

struct InputSystemImpl: public gainput::InputListener
{
	enum InputControlType
	{
		CONTROL_BUTTON = 0,
		CONTROL_FLOAT,
		CONTROL_AXIS,
		CONTROL_VIRTUAL_JOYSTICK,
		CONTROL_COMPOSITE,
		CONTROL_COMBO,
	};

	struct IControl
	{
		InputActionDesc  mAction;
		InputControlType mType;
	};

	struct CompositeControl: public IControl
	{
		CompositeControl(const uint32_t controls[4], uint8_t composite)
		{
			memset((void*)this, 0, sizeof(*this));
			mComposite = composite;
			memcpy(mControls, controls, sizeof(mControls));
			mType = CONTROL_COMPOSITE;
		}
		float2   mValue;
		uint32_t mControls[4];
		uint8_t  mComposite;
		uint8_t  mStarted;
		uint8_t  mPerformed[4];
		uint8_t  mPressedVal[4];
	};

	struct FloatControl: public IControl
	{
		FloatControl(uint16_t start, uint8_t target, bool raw, bool delta)
		{
			memset((void*)this, 0, sizeof(*this));
			mStartButton = start;
			mTarget = target;
			mType = CONTROL_FLOAT;
			mDelta = (1 << (uint8_t)raw) | (uint8_t)delta;
			mScale = 1;
			mScaleByDT = false;
		}
		float3   mValue;
		float    mScale;
		uint16_t mStartButton;
		uint8_t  mTarget;
		uint8_t  mStarted;
		uint8_t  mPerformed;
		uint8_t  mDelta;
		bool     mScaleByDT;
	};

	struct AxisControl: public IControl
	{
		AxisControl(uint16_t start, uint8_t target, uint8_t axis)
		{
			memset((void*)this, 0, sizeof(*this));
			mStartButton = start;
			mTarget = target;
			mAxisCount = axis;
			mType = CONTROL_AXIS;
		}
		float3   mValue;
		float3   mNewValue;
		uint16_t mStartButton;
		uint8_t  mTarget;
		uint8_t  mAxisCount;
		uint8_t  mStarted;
		uint8_t  mPerformed;
	};

	struct VirtualJoystickControl: public IControl
	{
		float2  mStartPos;
		float2  mCurrPos;
		float   mOutsideRadius;
		float   mDeadzone;
		float   mScale;
		uint8_t mTouchIndex;
		uint8_t mStarted;
		uint8_t mPerformed;
		uint8_t mArea;
		uint8_t mIsPressed;
		uint8_t mInitialized;
		uint8_t mActive;
	};

	struct FloatControlSet
	{
		FloatControl* key;
	};

	struct IControlSet
	{
		IControl* key;
	};
	
	struct ComboControl : public IControl
	{
		uint16_t mPressButton;
		uint16_t mTriggerButton;
		uint8_t  mPressed;
	};

	/// Maps the action mapping ID to the ActionMappingDesc
	/// C Array of stb_ds arrays	
	ActionMappingDesc*						 mInputActionMappingIdToDesc[MAX_DEVICES] = {NULL};
	/// List of all input controls per device
	/// C Array of stb_ds arrays of stb_ds arrays of IControl*
	IControl***								 mControls[MAX_DEVICES] = {};
	/// List of gestures
	/// stb_ds array of InputAction*
	InputActionDesc*						 mGestureControls = NULL;
	/// This global action will be invoked everytime there is a text character typed on a physical / virtual keyboard
	GlobalInputActionDesc					 mGlobalTextInputControl = {GlobalInputActionDesc::TEXT, NULL, NULL};
	/// This global action will be invoked everytime there is a button action mapping triggered
	GlobalInputActionDesc					 mGlobalAnyButtonAction = {GlobalInputActionDesc::ANY_BUTTON_ACTION, NULL, NULL};
	/// List of controls which need to be canceled at the end of the frame
	/// stb_ds array of FloatControl*
	FloatControlSet*						 mFloatDeltaControlCancelQueue = NULL;
	IControlSet*							 mButtonControlPerformQueue = NULL;
		
	IControl**								 mControlPool[MAX_DEVICES] = {NULL};
#if TOUCH_INPUT
	float2                                   mTouchPositions[gainput::TouchCount_ >> 2];
#else
	float2                                   mMousePosition;
#endif

	/// Window pointer passed by the app
	/// Input capture will be performed on this window
	WindowDesc*								 pWindow = NULL;

	/// Gainput Manager which lets us talk with the gainput backend
	gainput::InputManager*                   pInputManager = NULL;
	// gainput view which is only used for apple.
	// keep it declared for all platforms to avoid #defines in implementation
	void*                                    pGainputView = NULL;

	InputDeviceType*                         pDeviceTypes = NULL;
	gainput::DeviceId*                       pGamepadDeviceIDs = NULL;
	gainput::DeviceId                        mMouseDeviceID = {};
	gainput::DeviceId                        mRawMouseDeviceID = {};
	gainput::DeviceId                        mKeyboardDeviceID = {};
	gainput::DeviceId                        mTouchDeviceID = {};

	bool                                     mVirtualKeyboardActive = false;
	bool                                     mInputCaptured = false;
	bool                                     mDefaultCapture = false;

	bool Init(WindowDesc* window)
	{
		pWindow = window;

#ifdef GAINPUT_PLATFORM_GGP
		gainput::SetWindow(pWindow->handle.window);
#endif

		// Defaults
		mVirtualKeyboardActive = false;
		mDefaultCapture = true;
		mInputCaptured = false;

		pGamepadDeviceIDs = (gainput::DeviceId*)tf_calloc(MAX_INPUT_GAMEPADS, sizeof(gainput::DeviceId));
		pDeviceTypes = (InputDeviceType*)tf_calloc(MAX_INPUT_GAMEPADS + 4, sizeof(InputDeviceType));

		// Default device ids
		mMouseDeviceID = gainput::InvalidDeviceId;
		mRawMouseDeviceID = gainput::InvalidDeviceId;
		mKeyboardDeviceID = gainput::InvalidDeviceId;
		for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
			pGamepadDeviceIDs[i] = gainput::InvalidDeviceId;
		mTouchDeviceID = gainput::InvalidDeviceId;

		// create input manager
		pInputManager = tf_new(gainput::InputManager);
		ASSERT(pInputManager);
        pInputManager->Init((void*)pWindow->handle.window);
		pGainputView = NULL;

#if defined(_WINDOWS) || defined(XBOX)
		pInputManager->SetWindowsInstance(window->handle.window);
#elif defined(ANDROID) && !defined(QUEST_VR)
        pInputManager->SetWindowsInstance(window->handle.configuration);
#endif

		// create all necessary devices
		mMouseDeviceID = pInputManager->CreateDevice<gainput::InputDeviceMouse>();
		mRawMouseDeviceID = pInputManager->CreateDevice<gainput::InputDeviceMouse>(gainput::InputDevice::AutoIndex, gainput::InputDeviceMouse::DV_RAW);
		mKeyboardDeviceID = pInputManager->CreateDevice<gainput::InputDeviceKeyboard>();
		mTouchDeviceID = pInputManager->CreateDevice<gainput::InputDeviceTouch>();
		for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
			pGamepadDeviceIDs[i] = pInputManager->CreateDevice<gainput::InputDevicePad>();

		// Assign device types
		pDeviceTypes[mMouseDeviceID] = InputDeviceType::INPUT_DEVICE_MOUSE;
		pDeviceTypes[mRawMouseDeviceID] = InputDeviceType::INPUT_DEVICE_MOUSE;
		pDeviceTypes[mKeyboardDeviceID] = InputDeviceType::INPUT_DEVICE_KEYBOARD;
		pDeviceTypes[mTouchDeviceID] = InputDeviceType::INPUT_DEVICE_TOUCH;

		// Create control maps
		arrsetlen(mControls[mKeyboardDeviceID], gainput::KeyCount_);
		memset(mControls[mKeyboardDeviceID], 0, sizeof(mControls[mKeyboardDeviceID][0]) * gainput::KeyCount_);
		arrsetlen(mControls[mMouseDeviceID], gainput::MouseButtonCount_);
		memset(mControls[mMouseDeviceID], 0, sizeof(mControls[mMouseDeviceID][0]) * gainput::MouseButtonCount_);
		arrsetlen(mControls[mRawMouseDeviceID], gainput::MouseButtonCount_);
		memset(mControls[mRawMouseDeviceID], 0, sizeof(mControls[mRawMouseDeviceID][0]) * gainput::MouseButtonCount_);
		arrsetlen(mControls[mTouchDeviceID], gainput::TouchCount_);
		memset(mControls[mTouchDeviceID], 0, sizeof(mControls[mTouchDeviceID][0]) * gainput::TouchCount_);

		// Action mappings
		arrsetlen(mInputActionMappingIdToDesc[mMouseDeviceID], MAX_INPUT_ACTIONS);
		arrsetlen(mInputActionMappingIdToDesc[mRawMouseDeviceID], MAX_INPUT_ACTIONS);
		arrsetlen(mInputActionMappingIdToDesc[mKeyboardDeviceID], MAX_INPUT_ACTIONS);
		arrsetlen(mInputActionMappingIdToDesc[mTouchDeviceID], MAX_INPUT_ACTIONS);

		
		for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
		{
			gainput::DeviceId deviceId = pGamepadDeviceIDs[i];

			pDeviceTypes[deviceId] = InputDeviceType::INPUT_DEVICE_GAMEPAD;
			arrsetlen(mControls[deviceId], gainput::PadButtonMax_);
			memset(mControls[deviceId], 0, sizeof(mControls[deviceId][0]) * gainput::PadButtonMax_);
		
			arrsetlen(mInputActionMappingIdToDesc[deviceId], MAX_INPUT_ACTIONS);
		}

		// Clear all mappings
		RemoveActionMappings(INPUT_ACTION_MAPPING_TARGET_ALL);

		pInputManager->AddListener(this);

		return InitSubView();
	}

	void Exit()
	{
		ASSERT(pInputManager);

		RemoveActionMappings(INPUT_ACTION_MAPPING_TARGET_ALL);
		
		tf_free(pGamepadDeviceIDs);
		tf_free(pDeviceTypes);

		ShutdownSubView();
        pInputManager->Exit();
		tf_delete(pInputManager);
		
		for (uint32_t i = 0; i < MAX_DEVICES; ++i)
		{
			arrfree(mInputActionMappingIdToDesc[i]);

			for (ptrdiff_t j = 0; j < arrlen(mControls[i]); ++j)
				arrfree(mControls[i][j]);
			arrfree(mControls[i]);

			for (ptrdiff_t j = 0; j < arrlen(mControlPool[i]); ++j)
				tf_free(mControlPool[i][j]);
			arrfree(mControlPool[i]);
		}

		hmfree(mButtonControlPerformQueue);
		hmfree(mFloatDeltaControlCancelQueue);

		arrfree(mGestureControls);
	}

	void Update(float deltaTime, uint32_t width, uint32_t height)
	{
		ASSERT(pInputManager);

		for (ptrdiff_t i = 0; i < hmlen(mFloatDeltaControlCancelQueue); ++i)
		{
			FloatControl* pControl = mFloatDeltaControlCancelQueue[i].key;
			pControl->mStarted = 0;
			pControl->mPerformed = 0;
			pControl->mValue = float3(0.0f);

			
			InputActionContext ctx = {};
			ctx.pUserData = pControl->mAction.pUserData;
			ctx.mPhase = INPUT_ACTION_PHASE_CANCELED;
			ctx.pCaptured = &mDefaultCapture;
			ctx.mActionId = pControl->mAction.mActionId;
#if TOUCH_INPUT
			ctx.pPosition = &mTouchPositions[pControl->mAction.mUserId];
#else
			ctx.pPosition = &mMousePosition;
#endif
			if (pControl->mAction.pFunction)
				pControl->mAction.pFunction(&ctx);

			if (mGlobalAnyButtonAction.pFunction)
				mGlobalAnyButtonAction.pFunction(&ctx);
		}

#if TOUCH_INPUT
		for (ptrdiff_t i = 0; i < hmlen(mButtonControlPerformQueue); ++i)
		{
			IControl* pControl = mButtonControlPerformQueue[i].key;
			InputActionContext ctx = {};
			ctx.pUserData = pControl->mAction.pUserData;
			ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
			ctx.pCaptured = &mDefaultCapture;
			ctx.mActionId = pControl->mAction.mActionId;
			ctx.pPosition = &mTouchPositions[pControl->mAction.mUserId];
			ctx.mBool = true;
			
			if (pControl->mAction.pFunction)
				pControl->mAction.pFunction(&ctx);

			if (mGlobalAnyButtonAction.pFunction)
				mGlobalAnyButtonAction.pFunction(&ctx);
		}
#endif
		hmfree(mButtonControlPerformQueue);
		hmfree(mFloatDeltaControlCancelQueue);

		gainput::InputDeviceKeyboard* keyboard = (gainput::InputDeviceKeyboard*)pInputManager->GetDevice(mKeyboardDeviceID);
		if (keyboard)
		{
			uint32_t count = 0;
			wchar_t* pText = keyboard->GetTextInput(&count);
			if (count)
			{
				InputActionContext ctx = {};
				ctx.pText = pText;
				ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
				ctx.pUserData = mGlobalTextInputControl.pUserData;
				if (mGlobalTextInputControl.pFunction)
				{
					mGlobalTextInputControl.pFunction(&ctx);
				}
			}
		}

		// update gainput manager
		pInputManager->SetDisplaySize(width, height);
		pInputManager->Update(deltaTime);

#if defined(__linux__) && !defined(__ANDROID__) && !defined(GAINPUT_PLATFORM_GGP)
		//this needs to be done before updating the events
		//that way current frame data will be delta after resetting mouse position
		if (mInputCaptured)
		{
			ASSERT(pWindow);

			float x = 0;
			float y = 0;
			x = (pWindow->windowedRect.right - pWindow->windowedRect.left) / 2;
			y = (pWindow->windowedRect.bottom - pWindow->windowedRect.top) / 2;
			XWarpPointer(pWindow->handle.display, None, pWindow->handle.window, 0, 0, 0, 0, x, y);
			gainput::InputDevice* device = pInputManager->GetDevice(mRawMouseDeviceID);
			device->WarpMouse(x, y);
			XFlush(pWindow->handle.display);
		}
#endif
	}

	template <typename T>
	T* AllocateControl(const gainput::DeviceId deviceId)
	{
		T* pControl = (T*)tf_calloc(1, sizeof(T));
		arrpush(mControlPool[deviceId], pControl);
		return pControl;
	}

	void CreateActionForActionMapping(const ActionMappingDesc* const pActionMappingDesc, const InputActionDesc* const pActionDesc)
	{
		InputActionDesc action = *pActionDesc;
				
		switch (pActionMappingDesc->mActionMappingDeviceTarget)
		{
		case INPUT_ACTION_MAPPING_TARGET_CONTROLLER:
		{
			const gainput::DeviceId gamepadDeviceId = pGamepadDeviceIDs[pActionMappingDesc->mUserId];

			switch (pActionMappingDesc->mActionMappingType)
			{
			case INPUT_ACTION_MAPPING_NORMAL:
			{
				if (pActionMappingDesc->mDeviceButtons[0] >= GAMEPAD_BUTTON_START)
				{
					IControl* pControl = AllocateControl<IControl>(gamepadDeviceId);
					ASSERT(pControl);

					pControl->mType = CONTROL_BUTTON;
					pControl->mAction = action;
					arrpush(mControls[gamepadDeviceId][pActionMappingDesc->mDeviceButtons[0]], pControl);
				}
				else // it's an axis
				{
					// Ensure # axis is correct
					ASSERT(pActionMappingDesc->mNumAxis == 1 || pActionMappingDesc->mNumAxis == 2);

					AxisControl* pControl = AllocateControl<AxisControl>(gamepadDeviceId);
					ASSERT(pControl);

					memset((void*)pControl, 0, sizeof(*pControl));
					pControl->mType = CONTROL_AXIS;
					pControl->mAction = action;
					pControl->mStartButton = pActionMappingDesc->mDeviceButtons[0];
					pControl->mAxisCount = pActionMappingDesc->mNumAxis;
					pControl->mTarget = (pControl->mAxisCount == 2 ? (1 << 1) | 1 : 1);
					for (uint32_t i = 0; i < pControl->mAxisCount; ++i)
						arrpush(mControls[gamepadDeviceId][pControl->mStartButton + i], pControl);
				}

				break;
			}
			case INPUT_ACTION_MAPPING_COMPOSITE:
			{
				CompositeControl* pControl = AllocateControl<CompositeControl>(gamepadDeviceId);
				ASSERT(pControl);

				memset((void*)pControl, 0, sizeof(*pControl));
				pControl->mComposite = 4;
				pControl->mControls[0] = pActionMappingDesc->mDeviceButtons[0];
				pControl->mControls[1] = pActionMappingDesc->mDeviceButtons[1];
				pControl->mControls[2] = pActionMappingDesc->mDeviceButtons[2];
				pControl->mControls[3] = pActionMappingDesc->mDeviceButtons[3];
				pControl->mType = CONTROL_COMPOSITE;
				pControl->mAction = action;
				for (uint32_t i = 0; i < pControl->mComposite; ++i)
					arrpush(mControls[gamepadDeviceId][pControl->mControls[i]], pControl);

				break;
			}
			case INPUT_ACTION_MAPPING_COMBO:
			{
				ComboControl* pControl = AllocateControl<ComboControl>(gamepadDeviceId);
				ASSERT(pControl);

				pControl->mType = CONTROL_COMBO;
				pControl->mAction = action;
				pControl->mPressButton = pActionMappingDesc->mDeviceButtons[0];
				pControl->mTriggerButton = pActionMappingDesc->mDeviceButtons[1];
				arrpush(mControls[gamepadDeviceId][pActionMappingDesc->mDeviceButtons[0]], pControl);
				arrpush(mControls[gamepadDeviceId][pActionMappingDesc->mDeviceButtons[1]], pControl);
				break;
			}
			default:
				ASSERT(0); // should never get here
			}
			break;
		}
		case INPUT_ACTION_MAPPING_TARGET_KEYBOARD:
		{
			switch (pActionMappingDesc->mActionMappingType)
			{
			case INPUT_ACTION_MAPPING_NORMAL:
			{
				// No axis available for keyboard
				IControl* pControl = AllocateControl<IControl>(mKeyboardDeviceID);
				ASSERT(pControl);

				pControl->mType = CONTROL_BUTTON;
				pControl->mAction = action;
				arrpush(mControls[mKeyboardDeviceID][pActionMappingDesc->mDeviceButtons[0]], pControl);
				
				break;
			}
			case INPUT_ACTION_MAPPING_COMPOSITE:
			{
				CompositeControl* pControl = AllocateControl<CompositeControl>(mKeyboardDeviceID);
				ASSERT(pControl);

				memset((void*)pControl, 0, sizeof(*pControl));
				pControl->mComposite = 4;
				pControl->mControls[0] = pActionMappingDesc->mDeviceButtons[0];
				pControl->mControls[1] = pActionMappingDesc->mDeviceButtons[1];
				pControl->mControls[2] = pActionMappingDesc->mDeviceButtons[2];
				pControl->mControls[3] = pActionMappingDesc->mDeviceButtons[3];
				pControl->mType = CONTROL_COMPOSITE;
				pControl->mAction = action;
				for (uint32_t i = 0; i < pControl->mComposite; ++i)
					arrpush(mControls[mKeyboardDeviceID][pControl->mControls[i]], pControl);

				break;
			}
			case INPUT_ACTION_MAPPING_COMBO:
			{
				ComboControl* pControl = AllocateControl<ComboControl>(mKeyboardDeviceID);
				ASSERT(pControl);

				pControl->mType = CONTROL_COMBO;
				pControl->mAction = action;
				pControl->mPressButton = pActionMappingDesc->mDeviceButtons[0];
				pControl->mTriggerButton = pActionMappingDesc->mDeviceButtons[1];
				arrpush(mControls[mKeyboardDeviceID][pActionMappingDesc->mDeviceButtons[0]], pControl);
				arrpush(mControls[mKeyboardDeviceID][pActionMappingDesc->mDeviceButtons[1]], pControl);
				break;
			}
			default:
				ASSERT(0); // should never get here
			}
			break;
		}
		case INPUT_ACTION_MAPPING_TARGET_MOUSE:
		{
			switch (pActionMappingDesc->mActionMappingType)
			{
			case INPUT_ACTION_MAPPING_NORMAL:
			{
				if (pActionMappingDesc->mDeviceButtons[0] < MOUSE_BUTTON_COUNT)
				{
					// No axis available for keyboard
					IControl* pControl = AllocateControl<IControl>(mMouseDeviceID);
					ASSERT(pControl);

					pControl->mType = CONTROL_BUTTON;
					pControl->mAction = action;
					arrpush(mControls[mMouseDeviceID][pActionMappingDesc->mDeviceButtons[0]], pControl);
				}
				else // it's an axis
				{
					// Ensure # axis is correct
					ASSERT(pActionMappingDesc->mNumAxis == 1 || pActionMappingDesc->mNumAxis == 2);

					FloatControl* pControl = AllocateControl<FloatControl>(mMouseDeviceID);
					ASSERT(pControl);

					memset((void*)pControl, 0, sizeof(*pControl));
					pControl->mType = CONTROL_FLOAT;
					pControl->mStartButton = pActionMappingDesc->mDeviceButtons[0];
					pControl->mTarget = (pActionMappingDesc->mNumAxis == 2 ? (1 << 1) | 1 : 1);
					pControl->mDelta = (1 << 1) | 1; // always use delta for axis (absolute pos is always provided in the ctx anyway)
					pControl->mAction = action;
					pControl->mScale = pActionMappingDesc->mScale;
					pControl->mScaleByDT = pActionMappingDesc->mScaleByDT;

					const gainput::DeviceId deviceId = mRawMouseDeviceID; // always use raw mouse for float axis

					for (uint32_t i = 0; i < pActionMappingDesc->mNumAxis; ++i)
						arrpush(mControls[deviceId][pControl->mStartButton + i], pControl);
				}

				break;
			}
			case INPUT_ACTION_MAPPING_COMPOSITE:
			{
				CompositeControl* pControl = AllocateControl<CompositeControl>(mMouseDeviceID);
				ASSERT(pControl);

				memset((void*)pControl, 0, sizeof(*pControl));
				pControl->mComposite = 4;
				pControl->mControls[0] = pActionMappingDesc->mDeviceButtons[0];
				pControl->mControls[1] = pActionMappingDesc->mDeviceButtons[1];
				pControl->mControls[2] = pActionMappingDesc->mDeviceButtons[2];
				pControl->mControls[3] = pActionMappingDesc->mDeviceButtons[3];
				pControl->mType = CONTROL_COMPOSITE;
				pControl->mAction = action;
				for (uint32_t i = 0; i < pControl->mComposite; ++i)
					arrpush(mControls[mMouseDeviceID][pControl->mControls[i]], pControl);

				break;
			}
			case INPUT_ACTION_MAPPING_COMBO:
			{
				ComboControl* pControl = AllocateControl<ComboControl>(mMouseDeviceID);
				ASSERT(pControl);

				pControl->mType = CONTROL_COMBO;
				pControl->mAction = action;
				pControl->mPressButton = pActionMappingDesc->mDeviceButtons[0];
				pControl->mTriggerButton = pActionMappingDesc->mDeviceButtons[1];
				arrpush(mControls[mMouseDeviceID][pActionMappingDesc->mDeviceButtons[0]], pControl);
				arrpush(mControls[mMouseDeviceID][pActionMappingDesc->mDeviceButtons[1]], pControl);
				break;
			}
			default:
				ASSERT(0); // should never get here
			}
			break;
		}
		case INPUT_ACTION_MAPPING_TARGET_TOUCH:
		{
#if TOUCH_INPUT
			switch (pActionMappingDesc->mActionMappingType)
			{
			case INPUT_ACTION_MAPPING_NORMAL:
			{
				IControl* pControl = AllocateControl<IControl>(mTouchDeviceID);
				ASSERT(pControl);

				pControl->mType = CONTROL_BUTTON;
				pControl->mAction = action;
				arrpush(mControls[mTouchDeviceID][TOUCH_DOWN(pActionMappingDesc->mUserId)], pControl);

				break;
			}
			case INPUT_ACTION_MAPPING_TOUCH_GESTURE:
			{
#ifndef NX64
				uint32_t               gestureId = (uint32_t)arrlen(mGestureControls);
				gainput::GestureConfig gestureConfig = {};
				gestureConfig.mType = (gainput::GestureType)(pActionMappingDesc->mDeviceButtons[0]);
				gestureConfig.mMaxNumberOfTouches = 1u;
				gestureConfig.mMinNumberOfTouches = 1u;
				gestureConfig.mMinimumPressDuration = 1.f;
				gestureConfig.mNumberOfTapsRequired = 1u;
#if defined(TARGET_IOS)
				GainputView*           view = (__bridge GainputView*)pGainputView;
				[view addGestureMapping : gestureId withConfig : gestureConfig];
#elif defined(ANDROID)
				gainput::InputDeviceTouch* pTouch = (gainput::InputDeviceTouch*)pInputManager->GetDevice(mTouchDeviceID);
				pTouch->AddGestureMapping(gestureId, gestureConfig);
#endif
#endif
				arrpush(mGestureControls, action);

				break;
			}
			case INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK:
			{
				VirtualJoystickControl* pControl = AllocateControl<VirtualJoystickControl>(mTouchDeviceID);
				ASSERT(pControl);

				pControl->mType = CONTROL_VIRTUAL_JOYSTICK;
				pControl->mAction = action;
				pControl->mOutsideRadius = pActionMappingDesc->mOutsideRadius;
				pControl->mDeadzone = pActionMappingDesc->mDeadzone;
				pControl->mScale = pActionMappingDesc->mScale;
				pControl->mTouchIndex = 0xFF;
				pControl->mArea = pActionMappingDesc->mVirtualJoystickScreenArea;
				arrpush(mControls[mTouchDeviceID][gainput::Touch0Down], pControl);
				arrpush(mControls[mTouchDeviceID][gainput::Touch0X], pControl);
				arrpush(mControls[mTouchDeviceID][gainput::Touch0Y], pControl);
				arrpush(mControls[mTouchDeviceID][gainput::Touch1Down], pControl);
				arrpush(mControls[mTouchDeviceID][gainput::Touch1X], pControl);
				arrpush(mControls[mTouchDeviceID][gainput::Touch1Y], pControl);

				break;
			}
			default:
				ASSERT(0); // should never get here
			}
#endif
			break;
		}
		default:
			ASSERT(0); // should never get here
		}
	}

	void AddInputAction(const InputActionDesc* pDesc, const InputActionMappingDeviceTarget actionMappingTarget)
	{	
		ASSERT(pDesc);
		ASSERT(pDesc->mActionId < MAX_INPUT_ACTIONS);

		if (INPUT_ACTION_MAPPING_TARGET_KEYBOARD == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
		{
			ActionMappingDesc* pActionMappingDesc = &mInputActionMappingIdToDesc[mKeyboardDeviceID][pDesc->mActionId];
			if (INPUT_ACTION_MAPPING_TARGET_KEYBOARD == actionMappingTarget)
			{
				ASSERT(pActionMappingDesc->mActionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL);
			}

			if (pActionMappingDesc->mActionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL)
				CreateActionForActionMapping(pActionMappingDesc, pDesc);
		}
		
		if (INPUT_ACTION_MAPPING_TARGET_CONTROLLER == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
		{
			const gainput::DeviceId gamepadDeviceId = pGamepadDeviceIDs[pDesc->mUserId];

			ActionMappingDesc* pActionMappingDesc = &mInputActionMappingIdToDesc[gamepadDeviceId][pDesc->mActionId];
			if (INPUT_ACTION_MAPPING_TARGET_CONTROLLER == actionMappingTarget)
			{
				ASSERT(pActionMappingDesc->mActionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL);
			}

			if (pActionMappingDesc->mActionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL)
				CreateActionForActionMapping(pActionMappingDesc, pDesc);
		}

		if (INPUT_ACTION_MAPPING_TARGET_MOUSE == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
		{
			ActionMappingDesc* pActionMappingDesc = &mInputActionMappingIdToDesc[mMouseDeviceID][pDesc->mActionId];
			if (INPUT_ACTION_MAPPING_TARGET_MOUSE == actionMappingTarget)
			{
				ASSERT(pActionMappingDesc->mActionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL);
			}

			if (pActionMappingDesc->mActionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL)
				CreateActionForActionMapping(pActionMappingDesc, pDesc);
		}

		if (INPUT_ACTION_MAPPING_TARGET_TOUCH == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
		{
			ActionMappingDesc* pActionMappingDesc = &mInputActionMappingIdToDesc[mTouchDeviceID][pDesc->mActionId];
			if (INPUT_ACTION_MAPPING_TARGET_TOUCH == actionMappingTarget)
			{
				ASSERT(pActionMappingDesc->mActionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL);
			}

			if (pActionMappingDesc->mActionMappingDeviceTarget != INPUT_ACTION_MAPPING_TARGET_ALL)
				CreateActionForActionMapping(pActionMappingDesc, pDesc);
		}
	}

	void RemoveInputActionControls(const InputActionDesc* pDesc, const gainput::DeviceId deviceId, const bool doGestures = false)
	{
		for (ptrdiff_t i = 0; i < arrlen(mControls[deviceId]); ++i)
		{
			if (arrlen(mControls[deviceId][i]) > 0)
			{
				for (ptrdiff_t j = arrlen(mControls[deviceId][i]) - 1; j >= 0; --j)
				{
					if (mControls[deviceId][i][j]->mAction == *pDesc)
					{
						// Free is from the controls pool first and remove the entry
						for (ptrdiff_t k = 0; k < arrlen(mControlPool[deviceId]); ++k)
						{
							if (mControls[deviceId][i][j] == mControlPool[deviceId][k])
							{
								tf_free(mControlPool[deviceId][k]);
								arrdel(mControlPool[deviceId], k);
								break;
							}
						}

						// Then remove the entry from mControls
						arrdel(mControls[deviceId][i], j);
					}
				}
			}
		}

		if (doGestures)
		{
			if (arrlen(mGestureControls) > 0)
			{
				for (ptrdiff_t i = arrlen(mGestureControls) - 1; i >= 0; --i)
				{
					if (mGestureControls[i] == *pDesc)
					{
						arrdel(mGestureControls, i);
					}
				}
			}
		}
	}
	
	void RemoveInputAction(const InputActionDesc* pDesc, const InputActionMappingDeviceTarget actionMappingTarget)
	{
		ASSERT(pDesc);

		if (INPUT_ACTION_MAPPING_TARGET_CONTROLLER == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
		{
			RemoveInputActionControls(pDesc, pGamepadDeviceIDs[pDesc->mUserId]);
		}
		if (INPUT_ACTION_MAPPING_TARGET_KEYBOARD == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
		{
			RemoveInputActionControls(pDesc, mKeyboardDeviceID);
		}
		if (INPUT_ACTION_MAPPING_TARGET_MOUSE == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
		{
			RemoveInputActionControls(pDesc, mMouseDeviceID);
			RemoveInputActionControls(pDesc, mRawMouseDeviceID);
		}
		if (INPUT_ACTION_MAPPING_TARGET_TOUCH == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
		{
			RemoveInputActionControls(pDesc, mTouchDeviceID, true);
		}
	}

	void SetGlobalInputAction(const GlobalInputActionDesc* pDesc)
	{
		ASSERT(pDesc);
		switch (pDesc->mGlobalInputActionType)
		{
		case GlobalInputActionDesc::ANY_BUTTON_ACTION:
			mGlobalAnyButtonAction.pFunction = pDesc->pFunction;
			mGlobalAnyButtonAction.pUserData = pDesc->pUserData;
			break;
		case GlobalInputActionDesc::TEXT:
			mGlobalTextInputControl.pFunction = pDesc->pFunction;
			mGlobalTextInputControl.pUserData = pDesc->pUserData;
			break;
		default:
			ASSERT(0); // should never get here
		}
	}

	void AddActionMappings(ActionMappingDesc* const actionMappings, const uint32_t numActions, const InputActionMappingDeviceTarget actionMappingTarget)
	{
		// Ensure there isn't too many actions than we can fit in memory
		ASSERT(numActions < MAX_INPUT_ACTIONS);

		// First need to reset mappings
		RemoveActionMappings(actionMappingTarget);

		// Clear transient data structures
		hmfree(mButtonControlPerformQueue);
		hmfree(mFloatDeltaControlCancelQueue);

		for (uint32_t i = 0; i < numActions; ++i)
		{
			ActionMappingDesc* pActionMappingDesc = &actionMappings[i];
			ASSERT(pActionMappingDesc);
			ASSERT(INPUT_ACTION_MAPPING_TARGET_ALL != pActionMappingDesc->mActionMappingDeviceTarget); // target cannot be INPUT_ACTION_MAPPING_TARGET_ALL in the desc

			if (pActionMappingDesc != NULL) //-V547
			{
				// Ensure action mapping ID is within acceptable range
				ASSERT(pActionMappingDesc->mActionId < MAX_INPUT_ACTIONS);

				gainput::DeviceId deviceId = ~0u;

				switch (pActionMappingDesc->mActionMappingDeviceTarget)
				{
				case INPUT_ACTION_MAPPING_TARGET_CONTROLLER:
				{
					deviceId = pGamepadDeviceIDs[pActionMappingDesc->mUserId];
					break;
				}
				case INPUT_ACTION_MAPPING_TARGET_KEYBOARD:
				{
					deviceId = mKeyboardDeviceID;
					break;
				}
				case INPUT_ACTION_MAPPING_TARGET_MOUSE:
				{
					deviceId = mMouseDeviceID;
					break;
				}
				case INPUT_ACTION_MAPPING_TARGET_TOUCH:
				{
					deviceId = mTouchDeviceID;
					// Ensure the proper action mapping type is used
					ASSERT(INPUT_ACTION_MAPPING_NORMAL == pActionMappingDesc->mActionMappingType || INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK == pActionMappingDesc->mActionMappingType || INPUT_ACTION_MAPPING_TOUCH_GESTURE == pActionMappingDesc->mActionMappingType);
					break;
				}
				default:
					ASSERT(0); // should never get here
				}

				ASSERT(deviceId != ~0u);
				
				switch (pActionMappingDesc->mActionMappingType)
				{
				case INPUT_ACTION_MAPPING_NORMAL:
				case INPUT_ACTION_MAPPING_COMPOSITE:
				case INPUT_ACTION_MAPPING_COMBO:
				case INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK:
				case INPUT_ACTION_MAPPING_TOUCH_GESTURE:
				{
					ASSERT(mInputActionMappingIdToDesc[deviceId][pActionMappingDesc->mActionId].mActionMappingDeviceTarget == INPUT_ACTION_MAPPING_TARGET_ALL);
					mInputActionMappingIdToDesc[deviceId][pActionMappingDesc->mActionId] = *pActionMappingDesc;

					// Register an action for UI action mappings so that the app can intercept them via the global action (GLOBAL_INPUT_ACTION_ANY_BUTTON_ACTION)
					if (pActionMappingDesc->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
					{
						// Ensure the type is INPUT_ACTION_MAPPING_NORMAL
						ASSERT(INPUT_ACTION_MAPPING_NORMAL == pActionMappingDesc->mActionMappingType);

						InputActionDesc actionDesc;
						actionDesc.mActionId = pActionMappingDesc->mActionId;
						actionDesc.mUserId = pActionMappingDesc->mUserId;
						AddInputAction(&actionDesc, pActionMappingDesc->mActionMappingDeviceTarget);
					}
					break;
				}
				default:
					ASSERT(0); // should never get here
				}
			}
		}
	}

	void RemoveActionMappingsControls(const gainput::DeviceId deviceId, bool doGestures = false)
	{
		for (ptrdiff_t j = 0; j < arrlen(mControlPool[deviceId]); ++j)
			tf_free(mControlPool[deviceId][j]);
		arrfree(mControlPool[deviceId]);

		for (ptrdiff_t j = 0; j < arrlen(mControls[deviceId]); ++j)
			arrfree(mControls[deviceId][j]);

		if (doGestures)
			arrfree(mGestureControls);
	}

	void RemoveActionMappings(const InputActionMappingDeviceTarget actionMappingTarget)
	{
		if (INPUT_ACTION_MAPPING_TARGET_CONTROLLER == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
		{
			for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
			{
				const gainput::DeviceId deviceId = pGamepadDeviceIDs[i];
                memset((void*)mInputActionMappingIdToDesc[deviceId], 0, sizeof(mInputActionMappingIdToDesc[deviceId][0]) * MAX_INPUT_ACTIONS);
				RemoveActionMappingsControls(deviceId);
				memset(mControls[deviceId], 0, sizeof(mControls[deviceId][0]) * gainput::PadButtonMax_);
			}
		}
		if (INPUT_ACTION_MAPPING_TARGET_KEYBOARD == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
		{
			memset((void*)mInputActionMappingIdToDesc[mKeyboardDeviceID], 0, sizeof(mInputActionMappingIdToDesc[mKeyboardDeviceID][0]) * MAX_INPUT_ACTIONS);
			RemoveActionMappingsControls(mKeyboardDeviceID);
			memset(mControls[mKeyboardDeviceID], 0, sizeof(mControls[mKeyboardDeviceID][0]) * gainput::KeyCount_);
		}
		if (INPUT_ACTION_MAPPING_TARGET_MOUSE == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
		{
            memset((void*)mInputActionMappingIdToDesc[mMouseDeviceID], 0, sizeof(mInputActionMappingIdToDesc[mMouseDeviceID][0]) * MAX_INPUT_ACTIONS);
			RemoveActionMappingsControls(mMouseDeviceID);
			memset(mControls[mMouseDeviceID], 0, sizeof(mControls[mMouseDeviceID][0]) * gainput::MouseButtonCount_);

			// Need to do the same for the raw mouse device
            memset((void*)mInputActionMappingIdToDesc[mRawMouseDeviceID], 0, sizeof(mInputActionMappingIdToDesc[mRawMouseDeviceID][0]) * MAX_INPUT_ACTIONS);
			RemoveActionMappingsControls(mRawMouseDeviceID);
			memset(mControls[mRawMouseDeviceID], 0, sizeof(mControls[mRawMouseDeviceID][0]) * gainput::MouseButtonCount_);

		}
		if (INPUT_ACTION_MAPPING_TARGET_TOUCH == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
		{
            memset((void*)mInputActionMappingIdToDesc[mTouchDeviceID], 0, sizeof(mInputActionMappingIdToDesc[mTouchDeviceID][0]) * MAX_INPUT_ACTIONS);
			RemoveActionMappingsControls(mTouchDeviceID, true);
			memset(mControls[mTouchDeviceID], 0, sizeof(mControls[mTouchDeviceID][0]) * gainput::TouchCount_);
		}
	}

	bool InitSubView()
	{
#ifdef __APPLE__
		if (pWindow)
		{
			void* view = pWindow->handle.window;
			if (!view)
			{
				ASSERT(false && "View is required");
				return false;
			}

#ifdef TARGET_IOS
			UIView*      mainView = (UIView*)CFBridgingRelease(view);
			GainputView* newView = [[GainputView alloc] initWithFrame:mainView.bounds inputManager:*pInputManager];
			//we want everything to resize with main view.
			[newView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight |
										  UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleLeftMargin |
										  UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleBottomMargin)];
#else
			NSView*              mainView = (__bridge NSView*)view;
			float                retinScale = ((CAMetalLayer*)(mainView.layer)).drawableSize.width / mainView.frame.size.width;
			//Use view.window.contentLayoutRect instead of view.frame as a frame to avoid capturing inputs over title bar
			GainputMacInputView* newView = [[GainputMacInputView alloc] initWithFrame:mainView.window.contentLayoutRect
																			   window:mainView.window
																		  retinaScale:retinScale
																		 inputManager:*pInputManager];
			newView.nextKeyView = mainView;
			[newView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
#endif
			[mainView addSubview:newView];

#ifdef TARGET_IOS
#else
			NSWindow* window = [newView window];
			BOOL      madeFirstResponder = [window makeFirstResponder:newView];
			if (!madeFirstResponder)
				return false;
#endif

			pGainputView = (__bridge void*)newView;
		}
#endif

		return true;
	}

	void ShutdownSubView()
	{
#ifdef __APPLE__
		if (!pGainputView)
			return;

		//automatic reference counting
		//it will get deallocated.
		if (pGainputView)
		{
#ifndef TARGET_IOS
			GainputMacInputView* view = (GainputMacInputView*)CFBridgingRelease(pGainputView);
#else
			GainputView* view = (GainputView*)CFBridgingRelease(pGainputView);
#endif
			[view removeFromSuperview];
			pGainputView = NULL;
		}
#endif
	}

	bool SetEnableCaptureInput(bool enable)
	{
		ASSERT(pWindow);

		if (enable != mInputCaptured)
		{
			captureCursor(pWindow, enable);
			mInputCaptured = enable;
			
#if !defined(TARGET_IOS) && defined(__APPLE__)
			GainputMacInputView* view = (__bridge GainputMacInputView*)(pGainputView);
			[view SetMouseCapture:enable];
			view = NULL;
#endif

			return true;
		}

		return false;
	}

	void SetVirtualKeyboard(uint32_t type)
	{
#ifdef TARGET_IOS
		if (!pGainputView)
			return;

		if ((type > 0) != mVirtualKeyboardActive)
			mVirtualKeyboardActive = (type > 0);
		else
			return;

		GainputView* view = (__bridge GainputView*)(pGainputView);
		[view setVirtualKeyboard:type];
#elif defined(__ANDROID__)
		if ((type > 0) != mVirtualKeyboardActive)
		{
			mVirtualKeyboardActive = (type > 0);

			/* Note: native activity's API for soft input (ANativeActivity_showSoftInput & ANativeActivity_hideSoftInput) do not work.
			 *       So we do it manually using JNI.
		     */

			ANativeActivity* activity = pWindow->handle.activity;
			JNIEnv* jni;
			jint result = activity->vm->AttachCurrentThread(&jni, NULL);
			if (result == JNI_ERR)
			{
				ASSERT(0);
				return;
			}

			jclass cls = jni->GetObjectClass(activity->clazz);
			jmethodID methodID = jni->GetMethodID(cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
			jstring serviceName = jni->NewStringUTF("input_method");
			jobject inputService = jni->CallObjectMethod(activity->clazz, methodID, serviceName);

			jclass inputServiceCls = jni->GetObjectClass(inputService);
			methodID = jni->GetMethodID(inputServiceCls, "toggleSoftInput", "(II)V");
			jni->CallVoidMethod(inputService, methodID, 0, 0);

			jni->DeleteLocalRef(serviceName);
			activity->vm->DetachCurrentThread();
		}
		else
			return;
#endif
	}

	inline constexpr bool IsPointerType(gainput::DeviceId device) const
	{
#if TOUCH_INPUT
		return false;
#else
		return (device == mMouseDeviceID || device == mRawMouseDeviceID);
#endif
	}

	bool OnDeviceButtonBool(gainput::DeviceId device, gainput::DeviceButtonId deviceButton, bool oldValue, bool newValue)
	{
		if (oldValue == newValue)
			return false;

		if (arrlen(mControls[device]))
		{
			InputActionContext ctx = {};
			ctx.mDeviceType = pDeviceTypes[device];
			ctx.pCaptured = IsPointerType(device) ? &mInputCaptured : &mDefaultCapture;
#if TOUCH_INPUT
			uint32_t touchIndex = 0;
			if (device == mTouchDeviceID)
			{
				touchIndex = TOUCH_USER(deviceButton);
				gainput::InputDeviceTouch* pTouch = (gainput::InputDeviceTouch*)pInputManager->GetDevice(mTouchDeviceID);
				mTouchPositions[touchIndex][0] = pTouch->GetFloat(TOUCH_X(touchIndex));
				mTouchPositions[touchIndex][1] = pTouch->GetFloat(TOUCH_Y(touchIndex));
				ctx.pPosition = &mTouchPositions[touchIndex];
			}
#else
			if (IsPointerType(device))
			{
				gainput::InputDeviceMouse* pMouse = (gainput::InputDeviceMouse*)pInputManager->GetDevice(mMouseDeviceID);
				mMousePosition[0] = pMouse->GetFloat(gainput::MouseAxisX);
				mMousePosition[1] = pMouse->GetFloat(gainput::MouseAxisY);
				ctx.pPosition = &mMousePosition;
				ctx.mScrollValue = pMouse->GetFloat(gainput::MouseButtonMiddle);
			}
#endif
			bool executeNext = true;

			for (ptrdiff_t i = 0; i < arrlen(mControls[device][deviceButton]); ++i)
			{
				IControl* control = mControls[device][deviceButton][i];
				if (!executeNext)
					return true;

				const InputControlType type = control->mType;
				const InputActionDesc* pDesc = &control->mAction;
				ctx.pUserData = pDesc->pUserData;
				ctx.mActionId = pDesc->mActionId;
				ASSERT(ctx.mActionId != UINT_MAX);

				switch (type)
				{
				case CONTROL_BUTTON:
				{
					ctx.mBool = newValue;
					if (newValue && !oldValue)
					{
						ctx.mPhase = INPUT_ACTION_PHASE_STARTED;
						if (pDesc->pFunction)
							executeNext = pDesc->pFunction(&ctx) && executeNext;
						if (mGlobalAnyButtonAction.pFunction)
							mGlobalAnyButtonAction.pFunction(&ctx);
#if TOUCH_INPUT
						IControlSet val = {control};
						hmputs(mButtonControlPerformQueue, val);
#else
						ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
						if (pDesc->pFunction)
							executeNext = pDesc->pFunction(&ctx) && executeNext;
						if (mGlobalAnyButtonAction.pFunction)
							mGlobalAnyButtonAction.pFunction(&ctx);
#endif
					}
					else if (oldValue && !newValue)
					{
						ctx.mPhase = INPUT_ACTION_PHASE_CANCELED;
						if (pDesc->pFunction)
							executeNext = pDesc->pFunction(&ctx) && executeNext;
						if (mGlobalAnyButtonAction.pFunction)
							mGlobalAnyButtonAction.pFunction(&ctx);
					}
					break;
				}
				case CONTROL_COMPOSITE:
				{
					CompositeControl* pControl = (CompositeControl*)control;
					uint32_t          index = 0;
					for (; index < pControl->mComposite; ++index)
						if (deviceButton == pControl->mControls[index])
							break;

					const uint32_t axis = (index > 1) ? 1 : 0;
					if (newValue)
					{
						pControl->mPressedVal[index] = 1;
						pControl->mValue[axis] = (float)pControl->mPressedVal[axis * 2 + 0] - (float)pControl->mPressedVal[axis * 2 + 1];
					}

					if (pControl->mComposite == 2)
					{
						ctx.mFloat = pControl->mValue[axis];
					}
					else
					{
						if (!pControl->mValue[0] && !pControl->mValue[1])
							ctx.mFloat2 = float2(0.0f);
						else
							ctx.mFloat2 = pControl->mValue;
					}

					// Action Started
					if (!pControl->mStarted && !oldValue && newValue)
					{
						pControl->mStarted = 1;
						ctx.mPhase = INPUT_ACTION_PHASE_STARTED;
						if (pDesc->pFunction)
							executeNext = pDesc->pFunction(&ctx) && executeNext;
					}
					// Action Performed
					if (pControl->mStarted && newValue && !pControl->mPerformed[index])
					{
						pControl->mPerformed[index] = 1;
						ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
						if (pDesc->pFunction)
							executeNext = pDesc->pFunction(&ctx) && executeNext;
					}
					// Action Canceled
					if (oldValue && !newValue)
					{
						pControl->mPerformed[index] = 0;
						pControl->mPressedVal[index] = 0;
						bool allReleased = true;
						for (uint8_t i = 0; i < pControl->mComposite; ++i)
						{
							if (pControl->mPerformed[i])
							{
								allReleased = false;
								break;
							}
						}
						if (allReleased)
						{
							pControl->mValue = float2(0.0f);
							pControl->mStarted = 0;
							ctx.mFloat2 = pControl->mValue;
							ctx.mPhase = INPUT_ACTION_PHASE_CANCELED;
							if (pDesc->pFunction)
								executeNext = pDesc->pFunction(&ctx) && executeNext;
						}
						else if (pDesc->pFunction)
						{
							ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
							pControl->mValue[axis] = (float)pControl->mPressedVal[axis * 2 + 0] - (float)pControl->mPressedVal[axis * 2 + 1];
							ctx.mFloat2 = pControl->mValue;
							executeNext = pDesc->pFunction(&ctx) && executeNext;
						}
					}

					break;
				}
				// Mouse scroll is using OnDeviceButtonBool
				case CONTROL_FLOAT:
				{
					if (!oldValue && newValue)
					{
						ASSERT(deviceButton == gainput::MouseButtonWheelUp || deviceButton == gainput::MouseButtonWheelDown);

						FloatControl* pControl = (FloatControl*)control;
						ctx.mFloat2[1] = deviceButton == gainput::MouseButtonWheelUp ? 1.0f : -1.0f;
						if (pDesc->pFunction)
						{
							ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
							executeNext = pDesc->pFunction(&ctx) && executeNext;
						}

						FloatControlSet val = {pControl};
						hmputs(mFloatDeltaControlCancelQueue, val);
					}
					break;
				}
#if TOUCH_INPUT
				case CONTROL_VIRTUAL_JOYSTICK:
				{
					VirtualJoystickControl* pControl = (VirtualJoystickControl*)control;

					if (!oldValue && newValue && !pControl->mStarted)
					{
						pControl->mStartPos = mTouchPositions[touchIndex];
						if ((AREA_LEFT == pControl->mArea && pControl->mStartPos[0] <= pInputManager->GetDisplayWidth() * 0.5f) ||
							(AREA_RIGHT == pControl->mArea && pControl->mStartPos[0] > pInputManager->GetDisplayWidth() * 0.5f))
						{
							pControl->mStarted = 0x3;
							pControl->mTouchIndex = touchIndex;
							pControl->mCurrPos = pControl->mStartPos;

							
							ctx.mPhase = INPUT_ACTION_PHASE_STARTED;
							ctx.mFloat2 = float2(0.0f);
							ctx.pPosition = &pControl->mCurrPos;
							ctx.mActionId = pControl->mAction.mActionId;

							if (pVirtualJoystick)
								virtualJoystickOnMove(pVirtualJoystick, pControl->mArea == AREA_LEFT ? 0 : 1, &ctx);

							if (pDesc->pFunction)
								executeNext = pDesc->pFunction(&ctx) && executeNext;
						}
						else
						{
							pControl->mStarted = 0;
							pControl->mTouchIndex = 0xFF;
						}
					}
					else if (oldValue && !newValue)
					{
						if (pControl->mTouchIndex == touchIndex)
						{
							pControl->mIsPressed = 0;
							pControl->mTouchIndex = 0xFF;
							pControl->mStarted = 0;
							pControl->mPerformed = 0;
							
							ctx.mFloat2 = float2(0.0f);
							ctx.pPosition = &pControl->mCurrPos;
							ctx.mPhase = INPUT_ACTION_PHASE_CANCELED;
							ctx.mActionId = pControl->mAction.mActionId;

							if (pVirtualJoystick)
								virtualJoystickOnMove(pVirtualJoystick, pControl->mArea == AREA_LEFT ? 0 : 1, &ctx);

							if (pDesc->pFunction)
								executeNext = pDesc->pFunction(&ctx) && executeNext;
						}
					}
					break;
				}
#endif
				case CONTROL_COMBO:
				{
					ComboControl* pControl = (ComboControl*)control;
					if (deviceButton == pControl->mPressButton)
					{
						pControl->mPressed = (uint8_t)newValue;
					}
					else if (pControl->mPressed && oldValue && !newValue && pDesc->pFunction)
					{
						ctx.mBool = true;
						ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
						pDesc->pFunction(&ctx);
					}
					break;
				}
				default:
					break;
				}
			}
		}

		return true;
	}

	bool OnDeviceButtonFloat(float deltaTime, gainput::DeviceId device, gainput::DeviceButtonId deviceButton, float oldValue, float newValue)
	{
#if TOUCH_INPUT
		if (mTouchDeviceID == device)
		{
			const uint32_t touchIndex = TOUCH_USER(deviceButton);
			gainput::InputDeviceTouch* pTouch = (gainput::InputDeviceTouch*)pInputManager->GetDevice(mTouchDeviceID);
			mTouchPositions[touchIndex][0] = pTouch->GetFloat(TOUCH_X(touchIndex));
			mTouchPositions[touchIndex][1] = pTouch->GetFloat(TOUCH_Y(touchIndex));
		}
#else
		if (mMouseDeviceID == device)
		{
			gainput::InputDeviceMouse* pMouse = (gainput::InputDeviceMouse*)pInputManager->GetDevice(mMouseDeviceID);
			mMousePosition[0] = pMouse->GetFloat(gainput::MouseAxisX);
			mMousePosition[1] = pMouse->GetFloat(gainput::MouseAxisY);
		}
#endif

		if (arrlen(mControls[device]))
		{
			bool executeNext = true;

			for (ptrdiff_t i = 0; i < arrlen(mControls[device][deviceButton]); ++i)
			{
				IControl* control = mControls[device][deviceButton][i];
				if (!executeNext)
					return true;

				const InputControlType type = control->mType;
				const InputActionDesc* pDesc = &control->mAction;
				InputActionContext     ctx = {};
				ctx.mDeviceType = pDeviceTypes[device];
				ctx.pUserData = pDesc->pUserData;
				ctx.pCaptured = IsPointerType(device) ? &mInputCaptured : &mDefaultCapture;
				ctx.mActionId = pDesc->mActionId;

				switch (type)
				{
				case CONTROL_FLOAT:
				{
					FloatControl*          pControl = (FloatControl*)control;
					const uint32_t         axis = (deviceButton - pControl->mStartButton);

					if (pControl->mDelta & 0x1)
					{
						pControl->mValue[axis] += (axis > 0 ? -1.0f : 1.0f) * (newValue - oldValue) * pControl->mScale / (pControl->mScaleByDT ? deltaTime : 1);
						ctx.mFloat3 = pControl->mValue;

						if (((pControl->mStarted >> axis) & 0x1) == 0)
						{
							pControl->mStarted |= (1 << axis);
							if (pControl->mStarted == pControl->mTarget && pDesc->pFunction)
							{
								ctx.mPhase = INPUT_ACTION_PHASE_STARTED;
								executeNext = pDesc->pFunction(&ctx) && executeNext;
							}

							FloatControlSet val = {pControl};
							hmputs(mFloatDeltaControlCancelQueue, val);
						}

						pControl->mPerformed |= (1 << axis);

						if (pControl->mPerformed == pControl->mTarget)
						{
							pControl->mPerformed = 0;
							ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
							if (pDesc->pFunction)
								executeNext = pDesc->pFunction(&ctx) && executeNext;
						}
					}
					else if (pDesc->pFunction)
					{
						pControl->mPerformed |= (1 << axis);
						pControl->mValue[axis] = newValue;
						if (pControl->mPerformed == pControl->mTarget)
						{
							ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
							pControl->mPerformed = 0;
							ctx.mFloat3 = pControl->mValue;
							executeNext = pDesc->pFunction(&ctx) && executeNext;
						}
					}
					break;
					}
				case CONTROL_AXIS:
				{
					AxisControl* pControl = (AxisControl*)control;

					const uint32_t axis = (deviceButton - pControl->mStartButton);

					pControl->mNewValue[axis] = newValue;
					pControl->mPerformed |= (1 << axis);

					if (pControl->mPerformed == pControl->mTarget)
					{
						bool equal = true;
						for (uint32_t i = 0; i < pControl->mAxisCount; ++i)
							equal = equal && (pControl->mValue[i] == pControl->mNewValue[i]);

						pControl->mValue = pControl->mNewValue;

						
						ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
						ctx.mFloat3 = pControl->mValue;

						if (!equal)
						{
							if (pDesc->pFunction)
								executeNext = pDesc->pFunction(&ctx) && executeNext;
							if (mGlobalAnyButtonAction.pFunction)
								mGlobalAnyButtonAction.pFunction(&ctx);
						}
					}
					else
						continue;

					pControl->mPerformed = 0;
					break;
				}
#if TOUCH_INPUT
				case CONTROL_VIRTUAL_JOYSTICK:
				{
					VirtualJoystickControl* pControl = (VirtualJoystickControl*)control;

					const uint32_t axis = TOUCH_AXIS(deviceButton);

					if (!pControl->mStarted || TOUCH_USER(deviceButton) != pControl->mTouchIndex)
						continue;

					pControl->mPerformed |= (1 << axis);
					pControl->mCurrPos[axis] = newValue;
					if (pControl->mPerformed == 0x3)
					{
						// Calculate the new joystick positions
						vec2 delta = f2Tov2(pControl->mCurrPos - pControl->mStartPos);
						float halfRad = (pControl->mOutsideRadius * 0.5f) - pControl->mDeadzone;
						if (length(delta) > halfRad)
							pControl->mCurrPos = pControl->mStartPos + halfRad * v2ToF2(normalize(delta));

						ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
						float2 dir = ((pControl->mCurrPos - pControl->mStartPos) / halfRad) * pControl->mScale;
						ctx.mFloat2 = float2(dir[0], -dir[1]);
						ctx.pPosition = &pControl->mCurrPos;
						ctx.mActionId = pControl->mAction.mActionId;
						ctx.mFingerIndices[0] = pControl->mTouchIndex;

						if (pVirtualJoystick)
							virtualJoystickOnMove(pVirtualJoystick, pControl->mArea == AREA_LEFT ? 0 : 1, &ctx);

						if (pDesc->pFunction)
							executeNext = pDesc->pFunction(&ctx) && executeNext;
					}
					break;
				}
#endif
				default:
					break;
				}
			}
		}

		return true;
	}

	bool OnDeviceButtonGesture(gainput::DeviceId device, gainput::DeviceButtonId deviceButton, const struct gainput::GestureChange& gesture)
	{
#if defined(TOUCH_INPUT)
		const InputActionDesc* pDesc = &mGestureControls[deviceButton];
		if (pDesc->pFunction)
		{
			InputActionContext ctx = {};
			ctx.mActionId = pDesc->mActionId;
			ctx.pUserData = pDesc->pUserData;
			ctx.mDeviceType = pDeviceTypes[device];
			ctx.pPosition = (float2*)gesture.position;
			for ( uint32_t i = 0; i < MAX_INPUT_MULTI_TOUCHES; ++i )
				ctx.mFingerIndices[i] = -1;
						
			if ( gesture.type == gainput::GestureTap )
			{
				ctx.mFingerIndices[0] = gesture.fingerIndex;
			}
			else if (gesture.type == gainput::GestureLongPress)
			{
				ctx.mFingerIndices[0] = gesture.fingerIndex;
			}
			else if (gesture.type == gainput::GesturePan)
			{
				ctx.mFingerIndices[0] = gesture.fingerIndex;
				ctx.mFloat2 = { gesture.translation[0], gesture.translation[1] };
				ctx.mFingerIndices[0] = gesture.fingerIndex;
			}
			else if (gesture.type == gainput::GesturePinch)
			{
				ctx.mFloat4 =
				{
					gesture.velocity,
					gesture.scale,
					gesture.distance[0],
					gesture.distance[1]
				};
			}
			switch (gesture.phase) {
				case gainput::GesturePhase::GesturePhaseStarted:
					ctx.mPhase = INPUT_ACTION_PHASE_STARTED;
					break;
				case gainput::GesturePhase::GesturePhaseUpdated:
					ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
					break;
				case gainput::GesturePhase::GesturePhaseEnded:
					ctx.mPhase = INPUT_ACTION_PHASE_ENDED;
					break;
				case gainput::GesturePhase::GesturePhaseCanceled:
				default:
					ctx.mPhase = INPUT_ACTION_PHASE_CANCELED;
					break;
			}
			pDesc->pFunction(&ctx);
		}
#endif

		return true;
	}

	int GetPriority() const { return 0; }

	void SetDeadZone(unsigned int controllerIndex, float deadZoneSize)
	{
		if (controllerIndex >= MAX_INPUT_GAMEPADS)
			return;
		gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[controllerIndex]);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonL3, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonR3, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonL2, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonR2, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonLeftStickX, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonLeftStickY, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonRightStickX, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonRightStickY, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonAxis4, deadZoneSize);
		pDevicePad->SetDeadZone(gainput::PadButton::PadButtonAxis5, deadZoneSize);
	}

	const char* GetGamePadName(unsigned gamePadIndex)
	{
		if (gamePadIndex >= MAX_INPUT_GAMEPADS)
			return "Incorrect gamePadIndex";
		gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
		return pDevicePad->GetDeviceName();
	}

	bool GamePadConnected(unsigned int gamePadIndex)
	{
		if (gamePadIndex >= MAX_INPUT_GAMEPADS)
			return false;
		gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
		return pDevicePad->IsAvailable();
	}

	bool SetRumbleEffect(unsigned gamePadIndex, float left_motor, float right_motor, uint32_t duration_ms, bool vibrateTouchDevice)
	{
		if (gamePadIndex >= MAX_INPUT_GAMEPADS)
			return false;
		gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
		return pDevicePad->SetRumbleEffect(left_motor, right_motor, duration_ms, vibrateTouchDevice);
	}

	void SetLEDColor(unsigned gamePadIndex, uint8_t r, uint8_t g, uint8_t b)
	{
		if (gamePadIndex >= MAX_INPUT_GAMEPADS)
			return;
		gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
		pDevicePad->SetLEDColor(r, g, b);
	}

	void setOnDeviceChangeCallBack(void (*onDeviceChnageCallBack)(const char* name, bool added, int))
	{
		for(uint32_t i = 0 ; i < MAX_INPUT_GAMEPADS; i++)
		{
			gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[i]);
			if(pDevicePad)
			{
				pDevicePad->SetOnDeviceChangeCallBack(onDeviceChnageCallBack);
			}
		}
	}
};

static InputSystemImpl* pInputSystem = NULL;

#if (defined(_WINDOWS) && !defined(XBOX)) || (defined(__APPLE__) && !defined(TARGET_IOS))
static void ResetInputStates()
{
	pInputSystem->pInputManager->ClearAllStates(pInputSystem->mMouseDeviceID);
	pInputSystem->pInputManager->ClearAllStates(pInputSystem->mKeyboardDeviceID);
	for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
	{
		pInputSystem->pInputManager->ClearAllStates(pInputSystem->pGamepadDeviceIDs[i]);
	}
}
#endif

int32_t InputSystemHandleMessage(WindowDesc* pWindow, void* msg)
{
	if (pInputSystem == nullptr)
	{
		return 0;
	}
#if defined(_WINDOWS) && !defined(XBOX)
	pInputSystem->pInputManager->HandleMessage(*(MSG*)msg);
	if ((*(MSG*)msg).message == WM_ACTIVATEAPP && (*(MSG*)msg).wParam == WA_INACTIVE)
	{
		ResetInputStates();
	}
#elif defined(__APPLE__) && !defined(TARGET_IOS)
	if(msg)
	{
		NSNotificationName name = ((__bridge NSNotification*)msg).name;
		//Reset input states when we lose focus
		if(	name == NSWindowDidBecomeMainNotification ||
			name == NSWindowDidResignMainNotification ||
		    name == NSWindowDidResignKeyNotification )
		{
			ResetInputStates();
		}
	}
#elif defined(__ANDROID__) && !defined(QUEST_VR)
	return pInputSystem->pInputManager->HandleInput((AInputEvent*)msg, pWindow->handle.activity);
#elif defined(__linux__) && !defined(GAINPUT_PLATFORM_GGP) && !defined(QUEST_VR)
	pInputSystem->pInputManager->HandleEvent(*(XEvent*)msg);
#endif

	return 0;
}

bool initInputSystem(InputSystemDesc* pDesc)
{
	ASSERT(pDesc);
	ASSERT(pDesc->pWindow);

	pInputSystem = tf_new(InputSystemImpl);
	
	setCustomMessageProcessor(InputSystemHandleMessage);

	bool success = pInputSystem->Init(pDesc->pWindow);

#if TOUCH_INPUT
	if (!pDesc->mDisableVirtualJoystick)
	{
		ASSERT(pDesc->pRenderer); 
		VirtualJoystickDesc joystickDesc = {};
		joystickDesc.pRenderer = pDesc->pRenderer;
		joystickDesc.pJoystickTexture = "circlepad";
		initVirtualJoystick(&joystickDesc, &pVirtualJoystick);
	}
#endif

	addDefaultActionMappings();

	return success;
}

void exitInputSystem()
{
	ASSERT(pInputSystem);

#if TOUCH_INPUT
	exitVirtualJoystick(&pVirtualJoystick);
#endif

	setCustomMessageProcessor(nullptr);

	pInputSystem->Exit();
	tf_delete(pInputSystem);
	pInputSystem = NULL;
}

void updateInputSystem(float deltaTime, uint32_t width, uint32_t height)
{
	ASSERT(pInputSystem);

	pInputSystem->Update(deltaTime, width, height);
}

void addInputAction(const InputActionDesc* pDesc, const InputActionMappingDeviceTarget actionMappingTarget)
{
	ASSERT(pInputSystem);
	pInputSystem->AddInputAction(pDesc, actionMappingTarget);
}

void removeInputAction(const InputActionDesc* pDesc, const InputActionMappingDeviceTarget actionMappingTarget)
{
	ASSERT(pInputSystem);
	pInputSystem->RemoveInputAction(pDesc, actionMappingTarget);
}

void setGlobalInputAction(const GlobalInputActionDesc* pDesc)
{
	ASSERT(pInputSystem);
	pInputSystem->SetGlobalInputAction(pDesc);
}

void addActionMappings(ActionMappingDesc* const actionMappings, const uint32_t numActions, const InputActionMappingDeviceTarget actionMappingTarget)
{
	ASSERT(pInputSystem);
	return pInputSystem->AddActionMappings(actionMappings, numActions, actionMappingTarget);
}

void removeActionMappings(const InputActionMappingDeviceTarget actionMappingTarget)
{
	ASSERT(pInputSystem);
	return pInputSystem->RemoveActionMappings(actionMappingTarget);
}

void addDefaultActionMappings()
{
	ActionMappingDesc actionMappingsArr[] = {
		// Camera actions
		{INPUT_ACTION_MAPPING_COMPOSITE, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::TRANSLATE_CAMERA, {KeyboardButton::KEYBOARD_BUTTON_D, KeyboardButton::KEYBOARD_BUTTON_A, KeyboardButton::KEYBOARD_BUTTON_W, KeyboardButton::KEYBOARD_BUTTON_S}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::TRANSLATE_CAMERA, {GamepadButton::GAMEPAD_BUTTON_LEFT_STICK_X}, 2},
		{INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK, INPUT_ACTION_MAPPING_TARGET_TOUCH, DefaultInputActions::TRANSLATE_CAMERA, {}, 1, 0, 20.f, 200.f, 1.f, AREA_LEFT},
		{INPUT_ACTION_MAPPING_COMPOSITE, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::ROTATE_CAMERA, {KeyboardButton::KEYBOARD_BUTTON_L, KeyboardButton::KEYBOARD_BUTTON_J, KeyboardButton::KEYBOARD_BUTTON_I, KeyboardButton::KEYBOARD_BUTTON_K}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::ROTATE_CAMERA, {GamepadButton::GAMEPAD_BUTTON_RIGHT_STICK_X}, 2},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_MOUSE, DefaultInputActions::ROTATE_CAMERA, {MouseButton::MOUSE_BUTTON_AXIS_X}, 2, 0, 0, 0, 0.001f, AREA_LEFT, true},
		{INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK, INPUT_ACTION_MAPPING_TARGET_TOUCH, DefaultInputActions::ROTATE_CAMERA, {}, 1, 0, 20.f, 200.f, 1.f, AREA_RIGHT},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_MOUSE, DefaultInputActions::CAPTURE_INPUT, {MouseButton::MOUSE_BUTTON_LEFT}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::RESET_CAMERA, {KeyboardButton::KEYBOARD_BUTTON_SPACE}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::RESET_CAMERA, {GamepadButton::GAMEPAD_BUTTON_Y}},

		// Profile data / toggle fullscreen / exit actions
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::DUMP_PROFILE_DATA, {KeyboardButton::KEYBOARD_BUTTON_F3}},
		{INPUT_ACTION_MAPPING_COMBO, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::DUMP_PROFILE_DATA, {GamepadButton::GAMEPAD_BUTTON_START, GamepadButton::GAMEPAD_BUTTON_B}},
		{INPUT_ACTION_MAPPING_COMBO, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::TOGGLE_FULLSCREEN, {KeyboardButton::KEYBOARD_BUTTON_ALT_L, KeyboardButton::KEYBOARD_BUTTON_RETURN}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::EXIT, {KeyboardButton::KEYBOARD_BUTTON_ESCAPE}},

		// UI specific actions
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_TAB, {KeyboardButton::KEYBOARD_BUTTON_TAB}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_LEFT_ARROW, {KeyboardButton::KEYBOARD_BUTTON_LEFT}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_RIGHT_ARROW, {KeyboardButton::KEYBOARD_BUTTON_RIGHT}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_UP_ARROW, {KeyboardButton::KEYBOARD_BUTTON_UP}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_DOWN_ARROW, {KeyboardButton::KEYBOARD_BUTTON_DOWN}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_PAGE_UP, {KeyboardButton::KEYBOARD_BUTTON_PAGE_UP}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_PAGE_DOWN, {KeyboardButton::KEYBOARD_BUTTON_PAGE_DOWN}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_HOME, {KeyboardButton::KEYBOARD_BUTTON_HOME}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_END, {KeyboardButton::KEYBOARD_BUTTON_END}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_INSERT, {KeyboardButton::KEYBOARD_BUTTON_INSERT}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_DELETE, {KeyboardButton::KEYBOARD_BUTTON_DELETE}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_BACK_SPACE, {KeyboardButton::KEYBOARD_BUTTON_BACK_SPACE}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_SPACE, {KeyboardButton::KEYBOARD_BUTTON_SPACE}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_ENTER, {KeyboardButton::KEYBOARD_BUTTON_RETURN}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_ESCAPE, {KeyboardButton::KEYBOARD_BUTTON_ESCAPE}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_CONTROL_L, {KeyboardButton::KEYBOARD_BUTTON_CTRL_L}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_CONTROL_R, {KeyboardButton::KEYBOARD_BUTTON_CTRL_R}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_SHIFT_L, {KeyboardButton::KEYBOARD_BUTTON_SHIFT_L}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_SHIFT_R, {KeyboardButton::KEYBOARD_BUTTON_SHIFT_R}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_ALT_L, {KeyboardButton::KEYBOARD_BUTTON_ALT_L}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_ALT_R, {KeyboardButton::KEYBOARD_BUTTON_ALT_R}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_SUPER_L, {KeyboardButton::KEYBOARD_BUTTON_SUPER_L}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_SUPER_R, {KeyboardButton::KEYBOARD_BUTTON_SUPER_R}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_A, {KeyboardButton::KEYBOARD_BUTTON_A}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_C, {KeyboardButton::KEYBOARD_BUTTON_C}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_V, {KeyboardButton::KEYBOARD_BUTTON_V}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_X, {KeyboardButton::KEYBOARD_BUTTON_X}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_Y, {KeyboardButton::KEYBOARD_BUTTON_Y}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_Z, {KeyboardButton::KEYBOARD_BUTTON_Z}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_KEY_F2, {KeyboardButton::KEYBOARD_BUTTON_F2}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_MOUSE, DefaultInputActions::UI_MOUSE_LEFT, {MouseButton::MOUSE_BUTTON_LEFT}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_TOUCH, DefaultInputActions::UI_MOUSE_LEFT},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_MOUSE, DefaultInputActions::UI_MOUSE_RIGHT, {MouseButton::MOUSE_BUTTON_RIGHT}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_MOUSE, DefaultInputActions::UI_MOUSE_MIDDLE, {MouseButton::MOUSE_BUTTON_MIDDLE}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_MOUSE, DefaultInputActions::UI_MOUSE_SCROLL_UP, {MouseButton::MOUSE_BUTTON_WHEEL_UP}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_MOUSE, DefaultInputActions::UI_MOUSE_SCROLL_DOWN, {MouseButton::MOUSE_BUTTON_WHEEL_DOWN}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::UI_NAV_TOGGLE_UI, {GamepadButton::GAMEPAD_BUTTON_R3}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_KEYBOARD, DefaultInputActions::UI_NAV_TOGGLE_UI, {KeyboardButton::KEYBOARD_BUTTON_F1}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::UI_NAV_ACTIVATE, {GamepadButton::GAMEPAD_BUTTON_A}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::UI_NAV_CANCEL, {GamepadButton::GAMEPAD_BUTTON_B}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::UI_NAV_INPUT, {GamepadButton::GAMEPAD_BUTTON_Y}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::UI_NAV_MENU, {GamepadButton::GAMEPAD_BUTTON_X}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::UI_NAV_TWEAK_WINDOW_LEFT, {GamepadButton::GAMEPAD_BUTTON_LEFT}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::UI_NAV_TWEAK_WINDOW_RIGHT, {GamepadButton::GAMEPAD_BUTTON_RIGHT}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::UI_NAV_TWEAK_WINDOW_UP, {GamepadButton::GAMEPAD_BUTTON_UP}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::UI_NAV_TWEAK_WINDOW_DOWN, {GamepadButton::GAMEPAD_BUTTON_DOWN}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::UI_NAV_SCROLL_MOVE_WINDOW, {GamepadButton::GAMEPAD_BUTTON_LEFT_STICK_X}, 2},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::UI_NAV_FOCUS_PREV, {GamepadButton::GAMEPAD_BUTTON_L1}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::UI_NAV_FOCUS_NEXT, {GamepadButton::GAMEPAD_BUTTON_R1}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::UI_NAV_TWEAK_SLOW, {GamepadButton::GAMEPAD_BUTTON_L2}},
		{INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_CONTROLLER, DefaultInputActions::UI_NAV_TWEAK_FAST, {GamepadButton::GAMEPAD_BUTTON_R2}}
	};

	addActionMappings(actionMappingsArr, TF_ARRAY_COUNT(actionMappingsArr), INPUT_ACTION_MAPPING_TARGET_ALL);
}

bool setEnableCaptureInput(bool enable)
{
	ASSERT(pInputSystem);

	return pInputSystem->SetEnableCaptureInput(enable);
}

void setVirtualKeyboard(uint32_t type)
{
	ASSERT(pInputSystem);

	pInputSystem->SetVirtualKeyboard(type);
}

void setDeadZone(unsigned int controllerIndex, float deadZoneSize)
{
	ASSERT(pInputSystem);

	pInputSystem->SetDeadZone(controllerIndex, deadZoneSize);
}

const char* getGamePadName(int gamePadIndex)
{
	ASSERT(pInputSystem);

	return pInputSystem->GetGamePadName(gamePadIndex);
}

bool gamePadConnected(int gamePadIndex)
{
	ASSERT(pInputSystem);

	return pInputSystem->GamePadConnected(gamePadIndex);
}

bool setRumbleEffect(int gamePadIndex, float left_motor, float right_motor, uint32_t duration_ms)
{
	ASSERT(pInputSystem);
    // this is used only for mobile phones atm.
    // allows us to vibrate the actual phone instead of the connected gamepad.
	// if a gamepad is also connected, the app can decide which device should get the vibration
    bool vibrateDeviceInsteadOfPad = false;
    if(gamePadIndex == BUILTIN_DEVICE_HAPTICS)
    {
        vibrateDeviceInsteadOfPad = true;
        gamePadIndex = 0;
    }

	return pInputSystem->SetRumbleEffect(gamePadIndex, left_motor, right_motor, duration_ms, vibrateDeviceInsteadOfPad);
}

void setLEDColor(int gamePadIndex, uint8_t r, uint8_t g, uint8_t b)
{
	ASSERT(pInputSystem);

	pInputSystem->SetLEDColor(gamePadIndex, r, g, b);
}

void setOnDeviceChangeCallBack(void (*onDeviceChnageCallBack)(const char* name, bool added, int gamepadIndex))
{
	ASSERT(pInputSystem);
	pInputSystem->setOnDeviceChangeCallBack(onDeviceChnageCallBack);
}

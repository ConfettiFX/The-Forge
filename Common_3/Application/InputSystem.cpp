#ifndef DGA_INPUT
/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#include "../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

#include "../Graphics/GraphicsConfig.h"

#ifdef ENABLE_FORGE_INPUT
#include "../Application/ThirdParty/OpenSource/gainput/lib/include/gainput/gainput.h"
#endif

#if defined(__ANDROID__) || defined(NX64)
#include "../Application/ThirdParty/OpenSource/gainput/lib/include/gainput/GainputInputDeltaState.h"
#endif

#ifdef __APPLE__
#ifdef TARGET_IOS
#include "../Application/ThirdParty/OpenSource/gainput/lib/include/gainput/apple/GainputIos.h"
#else
#include "../Application/ThirdParty/OpenSource/gainput/lib/include/gainput/apple/GainputMac.h"
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

#include "../Application/ThirdParty/OpenSource/gainput/lib/source/gainput/apple/GainputInputDeviceTouchIos.h"
#else
#import <Cocoa/Cocoa.h>
#endif
#endif

#include "../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

#include "../Graphics/Interfaces/IGraphics.h"
#include "../OS/Interfaces/IOperatingSystem.h"
#include "../Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../Utilities/Interfaces/IFileSystem.h"
#include "../Utilities/Interfaces/ILog.h"
#include "Interfaces/IInput.h"
#include "Interfaces/IUI.h"

#include "../Utilities/Interfaces/IMemory.h"

#ifdef GAINPUT_PLATFORM_GGP
namespace gainput
{
extern void SetWindow(void* pData);
}
#endif

#define MAX_DEVICES 16U

#if (defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)) && !defined(QUEST_VR)
#define TOUCH_INPUT 1
#endif

#if TOUCH_INPUT
#define TOUCH_DOWN(id)     (((id) << 2) + 0)
#define TOUCH_X(id)        (((id) << 2) + 1)
#define TOUCH_Y(id)        (((id) << 2) + 2)
#define TOUCH_PRESSURE(id) (((id) << 2) + 3)
#define TOUCH_USER(btn)    ((btn) >> 2)
#define TOUCH_AXIS(btn)    (((btn) % 4) - 1)

// gainput::TouchButton enum has four values for each finger touch, for finger 0 these are: Touch0Down, Touch0X, Touch0Y, Touch0Pressure
// by dividing by four we get the finger id
FORGE_CONSTEXPR const uint32_t GAINPUT_TOUCH_BUTTONS_PER_FINGER = 4;
#endif

/**********************************************/
// VirtualJoystick
/**********************************************/

typedef struct VirtualJoystickDesc
{
    Renderer*   pRenderer;
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
    float2         mRenderScale = float2(0.f, 0.f);

    // input related
    float    mInsideRadius = 100.f;
    float    mOutsideRadius = 200.f;
    uint32_t mRootConstantIndex;

    struct StickInput
    {
        bool   mPressed = false;
        float2 mStartPos = float2(0.f, 0.f);
        float2 mCurrPos = float2(0.f, 0.f);
    };
    // Left -> Index 0
    // Right -> Index 1
    StickInput mSticks[2];
#endif
} VirtualJoystick;

static VirtualJoystick* gVirtualJoystick = NULL;

void initVirtualJoystick(VirtualJoystickDesc* pDesc, VirtualJoystick** ppVirtualJoystick)
{
    UNREF_PARAM(pDesc);
    ASSERT(ppVirtualJoystick);
    ASSERT(gVirtualJoystick == NULL);

    gVirtualJoystick = tf_new(VirtualJoystick);

#if TOUCH_INPUT
    Renderer* pRenderer = (Renderer*)pDesc->pRenderer;
    gVirtualJoystick->pRenderer = pRenderer;

    TextureLoadDesc loadDesc = {};
    SyncToken       token = {};
    loadDesc.pFileName = pDesc->pJoystickTexture;
    loadDesc.ppTexture = &gVirtualJoystick->pTexture;
    // Textures representing color should be stored in SRGB or HDR format
    loadDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
    addResource(&loadDesc, &token);
    waitForToken(&token);

    if (!gVirtualJoystick->pTexture)
    {
        LOGF(LogLevel::eWARNING, "Could not load virtual joystick texture file: %s", pDesc->pJoystickTexture);
        tf_delete(gVirtualJoystick);
        gVirtualJoystick = NULL;
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
    addSampler(pRenderer, &samplerDesc, &gVirtualJoystick->pSampler);
    /************************************************************************/
    // Resources
    /************************************************************************/
    BufferLoadDesc vbDesc = {};
    vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    vbDesc.mDesc.mSize = 128 * 4 * sizeof(float4);
    vbDesc.ppBuffer = &gVirtualJoystick->pMeshBuffer;
    addResource(&vbDesc, NULL);
#endif

    // Joystick is good!
    *ppVirtualJoystick = gVirtualJoystick;
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

bool loadVirtualJoystick(ReloadType loadType, TinyImageFormat colorFormat, uint32_t width, uint32_t height, uint32_t displayWidth,
                         uint32_t displayHeight)
{
    UNREF_PARAM(loadType);
    UNREF_PARAM(colorFormat);
    UNREF_PARAM(width);
    UNREF_PARAM(height);
    UNREF_PARAM(displayWidth);
    UNREF_PARAM(displayHeight);
#if TOUCH_INPUT
    if (!gVirtualJoystick)
    {
        return false;
    }

    if (loadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        Renderer* pRenderer = gVirtualJoystick->pRenderer;

        if (loadType & RELOAD_TYPE_SHADER)
        {
            /************************************************************************/
            // Shader
            /************************************************************************/
            ShaderLoadDesc texturedShaderDesc = {};
            texturedShaderDesc.mStages[0].pFileName = "textured_mesh.vert";
            texturedShaderDesc.mStages[1].pFileName = "textured_mesh.frag";
            addShader(pRenderer, &texturedShaderDesc, &gVirtualJoystick->pShader);

            const char*       pStaticSamplerNames[] = { "uSampler" };
            RootSignatureDesc textureRootDesc = { &gVirtualJoystick->pShader, 1 };
            textureRootDesc.mStaticSamplerCount = 1;
            textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
            textureRootDesc.ppStaticSamplers = &gVirtualJoystick->pSampler;
            addRootSignature(pRenderer, &textureRootDesc, &gVirtualJoystick->pRootSignature);
            gVirtualJoystick->mRootConstantIndex = getDescriptorIndexFromName(gVirtualJoystick->pRootSignature, "uRootConstants");

            DescriptorSetDesc descriptorSetDesc = { gVirtualJoystick->pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
            addDescriptorSet(pRenderer, &descriptorSetDesc, &gVirtualJoystick->pDescriptorSet);
            /************************************************************************/
            // Prepare descriptor sets
            /************************************************************************/
            DescriptorData params[1] = {};
            params[0].pName = "uTex";
            params[0].ppTextures = &gVirtualJoystick->pTexture;
            updateDescriptorSet(pRenderer, 0, gVirtualJoystick->pDescriptorSet, 1, params);
        }

        VertexLayout vertexLayout = {};
        vertexLayout.mBindingCount = 1;
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
        blendStateDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
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
        pipelineDesc.pRootSignature = gVirtualJoystick->pRootSignature;
        pipelineDesc.pShaderProgram = gVirtualJoystick->pShader;
        pipelineDesc.pVertexLayout = &vertexLayout;
        addPipeline(gVirtualJoystick->pRenderer, &desc, &gVirtualJoystick->pPipeline);
    }

    if (loadType & RELOAD_TYPE_RESIZE)
    {
        gVirtualJoystick->mRenderSize[0] = (float)width;
        gVirtualJoystick->mRenderSize[1] = (float)height;
        gVirtualJoystick->mRenderScale[0] = (float)width / (float)displayWidth;
        gVirtualJoystick->mRenderScale[1] = (float)height / (float)displayHeight;
    }
#endif
    return true;
}

void unloadVirtualJoystick(ReloadType unloadType)
{
    UNREF_PARAM(unloadType);
#if TOUCH_INPUT
    if (!gVirtualJoystick)
    {
        return;
    }

    if (unloadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        Renderer* pRenderer = gVirtualJoystick->pRenderer;

        removePipeline(pRenderer, gVirtualJoystick->pPipeline);

        if (unloadType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSet(pRenderer, gVirtualJoystick->pDescriptorSet);
            removeRootSignature(pRenderer, gVirtualJoystick->pRootSignature);
            removeShader(pRenderer, gVirtualJoystick->pShader);
        }
    }
#endif
}

void drawVirtualJoystick(Cmd* pCmd, const float4* color)
{
    UNREF_PARAM(pCmd);
    UNREF_PARAM(color);
#if TOUCH_INPUT
    if (!gVirtualJoystick || !(gVirtualJoystick->mSticks[0].mPressed || gVirtualJoystick->mSticks[1].mPressed))
        return;

    struct RootConstants
    {
        float4 color;
        float2 scaleBias;
        int    _pad[2];
    } data = {};

    cmdSetViewport(pCmd, 0.0f, 0.0f, gVirtualJoystick->mRenderSize[0], gVirtualJoystick->mRenderSize[1], 0.0f, 1.0f);
    cmdSetScissor(pCmd, 0u, 0u, (uint32_t)gVirtualJoystick->mRenderSize[0], (uint32_t)gVirtualJoystick->mRenderSize[1]);

    cmdBindPipeline(pCmd, gVirtualJoystick->pPipeline);
    cmdBindDescriptorSet(pCmd, 0, gVirtualJoystick->pDescriptorSet);
    data.color = *color;
    data.scaleBias = { 2.0f / (float)gVirtualJoystick->mRenderSize[0], -2.0f / (float)gVirtualJoystick->mRenderSize[1] };
    cmdBindPushConstants(pCmd, gVirtualJoystick->pRootSignature, gVirtualJoystick->mRootConstantIndex, &data);

    // Draw the camera controller's virtual joysticks.
    float extSide = gVirtualJoystick->mOutsideRadius;
    float intSide = gVirtualJoystick->mInsideRadius;

    uint64_t bufferOffset = 0;
    for (uint i = 0; i < 2; i++)
    {
        if (gVirtualJoystick->mSticks[i].mPressed)
        {
            float2 joystickSize = float2(extSide) * gVirtualJoystick->mRenderScale;
            float2 joystickCenter = gVirtualJoystick->mSticks[i].mStartPos * gVirtualJoystick->mRenderScale -
                                    float2(0.0f, gVirtualJoystick->mRenderSize.y * 0.1f);
            float2 joystickPos = joystickCenter - joystickSize * 0.5f;

            const uint32_t   vertexStride = sizeof(float4);
            BufferUpdateDesc updateDesc = { gVirtualJoystick->pMeshBuffer, bufferOffset };
            beginUpdateResource(&updateDesc);
            TexVertex vertices[4] = {};
            // the last variable can be used to create a border
            MAKETEXQUAD(vertices, joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0);
            memcpy(updateDesc.pMappedData, vertices, sizeof(vertices));
            endUpdateResource(&updateDesc);
            cmdBindVertexBuffer(pCmd, 1, &gVirtualJoystick->pMeshBuffer, &vertexStride, &bufferOffset);
            cmdDraw(pCmd, 4, 0);
            bufferOffset += sizeof(TexVertex) * 4;

            joystickSize = float2(intSide) * gVirtualJoystick->mRenderScale;
            joystickCenter = gVirtualJoystick->mSticks[i].mCurrPos * gVirtualJoystick->mRenderScale -
                             float2(0.0f, gVirtualJoystick->mRenderSize.y * 0.1f);
            joystickPos = float2(joystickCenter.getX(), joystickCenter.getY()) - 0.5f * joystickSize;

            updateDesc = { gVirtualJoystick->pMeshBuffer, bufferOffset };
            beginUpdateResource(&updateDesc);
            TexVertex verticesInner[4] = {};
            // the last variable can be used to create a border
            MAKETEXQUAD(verticesInner, joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0);
            memcpy(updateDesc.pMappedData, verticesInner, sizeof(verticesInner));
            endUpdateResource(&updateDesc);
            cmdBindVertexBuffer(pCmd, 1, &gVirtualJoystick->pMeshBuffer, &vertexStride, &bufferOffset);
            cmdDraw(pCmd, 4, 0);
            bufferOffset += sizeof(TexVertex) * 4;
        }
    }
#endif
}

uint8_t virtualJoystickIndexFromArea(TouchScreenArea area)
{
    switch (area)
    {
    case AREA_LEFT:
        return 0;
    case AREA_RIGHT:
        return 1;
    case AREA_FULL:
    default:
        ASSERT(0 && "VirtualJoystick expects AREA_LEFT or AREA_RIGHT");
        break;
    }

    return 0;
}

bool isPositionInsideScreenArea(float2 position, TouchScreenArea area, float2 displaySize)
{
    if (area == AREA_FULL)
        return true;
    else if (area == AREA_LEFT)
        return position.x <= displaySize.x * 0.5f;
    else if (area == AREA_RIGHT)
        return position.x > displaySize.x * 0.5f;

    return false;
}

void virtualJoystickOnMove(VirtualJoystick* pVirtualJoystick, uint32_t id, InputActionContext* ctx)
{
    UNREF_PARAM(pVirtualJoystick);
    UNREF_PARAM(id);
    UNREF_PARAM(ctx);
#if TOUCH_INPUT
    if (!ctx->pPosition)
        return;

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

/**********************************************/
// InputSystem
/**********************************************/
#ifdef ENABLE_FORGE_INPUT
struct InputSystemImpl: public gainput::InputListener
{
    // **********************************************
    // ***** Structures

    enum InputControlType
    {
        CONTROL_BUTTON = 0,
        CONTROL_FLOAT,
        CONTROL_AXIS,
        CONTROL_VIRTUAL_JOYSTICK,
        CONTROL_COMPOSITE,
        CONTROL_COMBO,
        CONTROL_GESTURE,
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
        uint8_t  mArea;
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

    struct ComboControl: public IControl
    {
        uint16_t mPressButton;
        uint16_t mTriggerButton;
        uint8_t  mPressed;
    };

    struct GestureControl: public IControl
    {
        TouchGesture mGestureType;
        uint32_t     mPerformed; // how many fingers were processed
        uint32_t     mTarget;    // Number of fingers required
    };

    struct FloatControlSet
    {
        FloatControl* key;
    };

    struct IControlSet
    {
        IControl* key;
    };

#if TOUCH_INPUT
    struct GestureRecognizer
    {
        struct Touch
        {
            enum State{ STARTED, HOLDING, ENDED };

            // Since we're tracking touches ourselves, we must know when to update touch data
            bool mUpdated;
            bool mMoved;

            State mState;
            float mTime;
            float mVelocity;
            vec2  mDistTraveled;
            vec2  mPos0;

            uint32_t mID;
            vec2     mPos;
        };

        Touch    mTouches[MAX_INPUT_MULTI_TOUCHES];
        uint32_t mActiveTouches;

        uint32_t mPerformingGesturesCount[MAX_INPUT_MULTI_TOUCHES];

        // Double tap and long press data
        vec2   mLastTapPos;
        float  mLastTapTime;
        Touch* mLongPressTouch;

        // Thresholds
        float mDoubleTapTimeThreshold;
        float mSwipeDistThreshold;
        float mSwipeVelocityThreshold;
        float mLongPressTimeThreshold;
        float mMovedDistThreshold;

        Touch* FindTouch(uint32_t id)
        {
            for (uint32_t i = 0; i < TF_ARRAY_COUNT(mTouches); ++i)
            {
                if (mTouches[i].mID == id)
                {
                    return &mTouches[i];
                }
            }

            return nullptr;
        }

        Touch* AddTouch(uint32_t id)
        {
            Touch* touch = FindTouch(id);
            if (touch != nullptr)
            {
                return touch;
            }

            for (uint32_t i = 0; i < TF_ARRAY_COUNT(mTouches); ++i)
            {
                if (mTouches[i].mID == -1)
                {
                    mTouches[i].mID = id;
                    mActiveTouches++;
                    return &mTouches[i];
                }
            }

            return nullptr;
        }

        void ReleaseTouch(uint32_t id)
        {
            for (uint32_t i = 0; i < TF_ARRAY_COUNT(mTouches); i++)
            {
                if (mTouches[i].mID == id)
                {
                    mTouches[i].mID = -1;
                    mTouches[i].mTime = 0.0f;
                    mTouches[i].mDistTraveled = vec2(0.0f);
                    mTouches[i].mUpdated = false;
                    mTouches[i].mMoved = false;
                    mActiveTouches--;
                    return;
                }
            }
        }
    };
#endif

    // **********************************************
    // ***** Data

    /// Maps the action mapping ID to the ActionMappingDesc
    /// C Array of stb_ds arrays
    ActionMappingDesc*    mInputActionMappingIdToDesc[MAX_DEVICES] = { NULL };
    /// List of all input controls per device
    /// C Array of stb_ds arrays of stb_ds arrays of IControl*
    IControl***           mControls[MAX_DEVICES] = {};
    /// This global action will be invoked everytime there is a text character typed on a physical / virtual keyboard
    GlobalInputActionDesc mGlobalTextInputControl = { GlobalInputActionDesc::TEXT, NULL, NULL };
    /// This global action will be invoked everytime there is a button action mapping triggered
    GlobalInputActionDesc mGlobalAnyButtonAction = { GlobalInputActionDesc::ANY_BUTTON_ACTION, NULL, NULL };
    /// List of controls which need to be canceled at the end of the frame
    /// stb_ds array of FloatControl*
    FloatControlSet*      mFloatDeltaControlCancelQueue = NULL;
    IControlSet*          mButtonControlPerformQueue = NULL;

    IControl** mControlPool[MAX_DEVICES] = { NULL };

#if TOUCH_INPUT
    GestureRecognizer mGestureRecognizer;
    float2            mTouchPositions[gainput::TouchCount_ >> 2];
    float             mTouchDownTime[gainput::TouchCount_ >> 2];
#else
    float2 mMousePosition;
#endif

    /// Window pointer passed by the app
    /// Input capture will be performed on this window
    WindowDesc* pWindow = NULL;

    /// Gainput Manager which lets us talk with the gainput backend
    gainput::InputManager* pInputManager = NULL;
    // gainput view which is only used for apple.
    // keep it declared for all platforms to avoid #defines in implementation
    void*                  pGainputView = NULL;

    InputDeviceType   pDeviceTypes[4 + MAX_INPUT_GAMEPADS] = {};
    gainput::DeviceId pGamepadDeviceIDs[MAX_INPUT_GAMEPADS] = {};
    gainput::DeviceId mMouseDeviceID = {};
    gainput::DeviceId mRawMouseDeviceID = {};
    gainput::DeviceId mKeyboardDeviceID = {};
    gainput::DeviceId mTouchDeviceID = {};

    void (*mOnDeviceChangeCallBack)(const char* name, bool added, int) = NULL;

    bool mVirtualKeyboardActive = false;
    bool mInputCaptured = false;
    bool mDefaultCapture = false;

    // **********************************************
    // ***** Functions

    // ----- Loading

    bool Init(WindowDesc* window)
    {
        pWindow = window;

#ifdef GAINPUT_PLATFORM_GGP
        gainput::SetWindow(pWindow->handle.window);
#endif

#if TOUCH_INPUT
        memset(mTouchDownTime, 0, sizeof(mTouchDownTime));
#endif

        // Defaults
        mVirtualKeyboardActive = false;
        mDefaultCapture = true;
        mInputCaptured = false;

        // Default device ids
        mMouseDeviceID = gainput::InvalidDeviceId;
        mRawMouseDeviceID = gainput::InvalidDeviceId;
        mKeyboardDeviceID = gainput::InvalidDeviceId;
        for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
            pGamepadDeviceIDs[i] = gainput::InvalidDeviceId;
        mTouchDeviceID = gainput::InvalidDeviceId;

        for (uint32_t i = 0; i < (sizeof pDeviceTypes / sizeof *pDeviceTypes); ++i)
            pDeviceTypes[i] = INPUT_DEVICE_INVALID;

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

#ifdef TOUCH_INPUT
        for (uint32_t i = 0; i < TF_ARRAY_COUNT(mGestureRecognizer.mTouches); ++i)
        {
            mGestureRecognizer.mTouches[i].mID = -1;
            mGestureRecognizer.mTouches[i].mTime = 0.0f;
            mGestureRecognizer.mTouches[i].mDistTraveled = vec2(0.0f);
            mGestureRecognizer.mTouches[i].mUpdated = false;
            mGestureRecognizer.mTouches[i].mMoved = false;
            mGestureRecognizer.mTouches[i].mState = GestureRecognizer::Touch::ENDED;
            mGestureRecognizer.mPerformingGesturesCount[i] = 0;
        }

        mGestureRecognizer.mLastTapPos = vec2(FLT_MAX);
        mGestureRecognizer.mLastTapTime = FLT_MAX;
        mGestureRecognizer.mLongPressTouch = nullptr;

        mGestureRecognizer.mActiveTouches = 0;
        mGestureRecognizer.mDoubleTapTimeThreshold = 0.5f;
        mGestureRecognizer.mSwipeDistThreshold = 500.0f;
        mGestureRecognizer.mSwipeVelocityThreshold = 1000.0f;
        mGestureRecognizer.mLongPressTimeThreshold = 1.5f;
        mGestureRecognizer.mMovedDistThreshold = 100.0f;
#endif

        // Used to intercept controllers connecting
        pInputManager->SetDeviceListener(this, DeviceChange);
        // create all necessary devices
        mMouseDeviceID = pInputManager->CreateDevice<gainput::InputDeviceMouse>();
        mRawMouseDeviceID =
            pInputManager->CreateDevice<gainput::InputDeviceMouse>(gainput::InputDevice::AutoIndex, gainput::InputDeviceMouse::DV_RAW);
        mKeyboardDeviceID = pInputManager->CreateDevice<gainput::InputDeviceKeyboard>();
        mTouchDeviceID = pInputManager->CreateDevice<gainput::InputDeviceTouch>();
        pInputManager->CreateControllers(MAX_INPUT_GAMEPADS);

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
            unsigned index = DEV_PAD_START + i;

            pDeviceTypes[index] = InputDeviceType::INPUT_DEVICE_GAMEPAD;
            arrsetlen(mControls[index], gainput::PadButtonMax_);
            memset(mControls[index], 0, sizeof(mControls[index][0]) * gainput::PadButtonMax_);

            arrsetlen(mInputActionMappingIdToDesc[index], MAX_INPUT_ACTIONS);
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
    }

    // ----- Runtime

    void Update(float deltaTime, uint32_t width, uint32_t height)
    {
        ASSERT(pInputManager);

#ifdef TOUCH_INPUT
        // Long press gesture can only be triggered in Update()
        for (uint32_t i = 0; i < arrlen(mControls[mTouchDeviceID][gainput::Touch0Down]); ++i)
        {
            IControl* control = mControls[mTouchDeviceID][gainput::Touch0Down][i];

            if (control->mType != CONTROL_GESTURE)
                continue;

            GestureControl* pControl = (GestureControl*)control;

            if (pControl->mGestureType != TOUCH_GESTURE_LONG_PRESS)
                continue;

            if (mGestureRecognizer.mLongPressTouch && !mGestureRecognizer.mLongPressTouch->mMoved &&
                mGestureRecognizer.mLongPressTouch->mTime > mGestureRecognizer.mLongPressTimeThreshold &&
                mGestureRecognizer.mLongPressTouch->mState == GestureRecognizer::Touch::STARTED)
            {
                if (pControl->mAction.pFunction)
                {
                    InputActionContext ctx = {};
                    ctx.mActionId = pControl->mAction.mActionId;
                    ctx.pUserData = pControl->mAction.pUserData;
                    ctx.mDeviceType = pDeviceTypes[mTouchDeviceID];
                    ctx.pPosition = (float2*)&mGestureRecognizer.mLongPressTouch->mPos;

                    for (uint32_t i = 0; i < MAX_INPUT_MULTI_TOUCHES; ++i)
                        ctx.mFingerIndices[i] = mGestureRecognizer.mTouches[i].mID;

                    ctx.mPhase = INPUT_ACTION_PHASE_STARTED;
                    ctx.mBool = true;
                    ctx.pCaptured = &mDefaultCapture;

                    pControl->mAction.pFunction(&ctx);
                }
            }
        }

        for (uint32_t i = 0; i < TF_ARRAY_COUNT(mGestureRecognizer.mTouches); ++i)
        {
            mGestureRecognizer.mTouches[i].mUpdated = false;

            if (mGestureRecognizer.mTouches[i].mID == -1)
                continue;

            if (mGestureRecognizer.mTouches[i].mState == GestureRecognizer::Touch::ENDED)
            {
                if (mGestureRecognizer.mPerformingGesturesCount[i] > 0)
                    continue;

                mGestureRecognizer.mLastTapPos = mGestureRecognizer.mTouches[i].mPos;
                mGestureRecognizer.mLastTapTime = mGestureRecognizer.mTouches[i].mTime;

                mGestureRecognizer.ReleaseTouch(mGestureRecognizer.mTouches[i].mID);
                continue;
            }

            if (mGestureRecognizer.mTouches[i].mTime > mGestureRecognizer.mLongPressTimeThreshold)
            {
                mGestureRecognizer.mTouches[i].mState = GestureRecognizer::Touch::HOLDING;
            }

            if (!mGestureRecognizer.mTouches[i].mMoved &&
                length(mGestureRecognizer.mTouches[i].mDistTraveled) > mGestureRecognizer.mMovedDistThreshold)
            {
                mGestureRecognizer.mTouches[i].mMoved = true;
            }

            mGestureRecognizer.mTouches[i].mTime += deltaTime;
        }

        mGestureRecognizer.mLastTapTime += deltaTime;
#endif

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
            ctx.mDeviceType = INPUT_DEVICE_TOUCH;
            ctx.pPosition = &mTouchPositions[pControl->mAction.mUserId];
#else
            ctx.mDeviceType = INPUT_DEVICE_MOUSE;
            ctx.pPosition = &mMousePosition;
#endif
            if (pControl->mAction.pFunction)
                pControl->mAction.pFunction(&ctx);

            if (mGlobalAnyButtonAction.pFunction)
            {
                ctx.pUserData = mGlobalAnyButtonAction.pUserData;
                mGlobalAnyButtonAction.pFunction(&ctx);
            }
        }

#if TOUCH_INPUT
        for (ptrdiff_t i = 0; i < hmlen(mButtonControlPerformQueue); ++i)
        {
            IControl*          pControl = mButtonControlPerformQueue[i].key;
            InputActionContext ctx = {};
            ctx.pUserData = pControl->mAction.pUserData;
            ctx.mDeviceType = INPUT_DEVICE_TOUCH;
            ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
            ctx.pCaptured = &mDefaultCapture;
            ctx.mActionId = pControl->mAction.mActionId;
            ctx.pPosition = &mTouchPositions[pControl->mAction.mUserId];
            ctx.mBool = true;

            if (pControl->mAction.pFunction)
                pControl->mAction.pFunction(&ctx);

            if (mGlobalAnyButtonAction.pFunction)
            {
                ctx.pUserData = mGlobalAnyButtonAction.pUserData;
                mGlobalAnyButtonAction.pFunction(&ctx);
            }
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
                ctx.mDeviceType = INPUT_DEVICE_KEYBOARD;
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
        // this needs to be done before updating the events
        // that way current frame data will be delta after resetting mouse position
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

    // ----- Action & Control Creation

    template<typename T>
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
            const unsigned index = DEV_PAD_START + pActionMappingDesc->mUserId;

            switch (pActionMappingDesc->mActionMappingType)
            {
            case INPUT_ACTION_MAPPING_NORMAL:
            {
                if (pActionMappingDesc->mDeviceButtons[0] >= GAMEPAD_BUTTON_START)
                {
                    IControl* pControl = AllocateControl<IControl>(index);
                    ASSERT(pControl);

                    pControl->mType = CONTROL_BUTTON;
                    pControl->mAction = action;
                    arrpush(mControls[index][pActionMappingDesc->mDeviceButtons[0]], pControl);
                }
                else // it's an axis
                {
                    // Ensure # axis is correct
                    ASSERT(pActionMappingDesc->mNumAxis == 1 || pActionMappingDesc->mNumAxis == 2);

                    AxisControl* pControl = AllocateControl<AxisControl>(index);
                    ASSERT(pControl);

                    memset((void*)pControl, 0, sizeof(*pControl));
                    pControl->mType = CONTROL_AXIS;
                    pControl->mAction = action;
                    pControl->mStartButton = (uint16_t)pActionMappingDesc->mDeviceButtons[0];
                    pControl->mAxisCount = pActionMappingDesc->mNumAxis;
                    pControl->mTarget = (pControl->mAxisCount == 2 ? (1 << 1) | 1 : 1);
                    for (uint32_t i = 0; i < pControl->mAxisCount; ++i)
                        arrpush(mControls[index][pControl->mStartButton + i], pControl);
                }

                break;
            }
            case INPUT_ACTION_MAPPING_COMPOSITE:
            {
                CompositeControl* pControl = AllocateControl<CompositeControl>(index);
                ASSERT(pControl);

                memset((void*)pControl, 0, sizeof(*pControl));
                pControl->mComposite = pActionMappingDesc->mCompositeUseSingleAxis ? 2 : 4;
                pControl->mControls[0] = pActionMappingDesc->mDeviceButtons[0];
                pControl->mControls[1] = pActionMappingDesc->mDeviceButtons[1];
                pControl->mControls[2] = pActionMappingDesc->mDeviceButtons[2];
                pControl->mControls[3] = pActionMappingDesc->mDeviceButtons[3];
                pControl->mType = CONTROL_COMPOSITE;
                pControl->mAction = action;
                for (uint32_t i = 0; i < pControl->mComposite; ++i)
                    arrpush(mControls[index][pControl->mControls[i]], pControl);

                break;
            }
            case INPUT_ACTION_MAPPING_COMBO:
            {
                ComboControl* pControl = AllocateControl<ComboControl>(index);
                ASSERT(pControl);

                pControl->mType = CONTROL_COMBO;
                pControl->mAction = action;
                pControl->mPressButton = (uint16_t)pActionMappingDesc->mDeviceButtons[0];
                pControl->mTriggerButton = (uint16_t)pActionMappingDesc->mDeviceButtons[1];
                arrpush(mControls[index][pActionMappingDesc->mDeviceButtons[0]], pControl);
                arrpush(mControls[index][pActionMappingDesc->mDeviceButtons[1]], pControl);
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
                pControl->mComposite = pActionMappingDesc->mCompositeUseSingleAxis ? 2 : 4;
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
                pControl->mPressButton = (uint16_t)pActionMappingDesc->mDeviceButtons[0];
                pControl->mTriggerButton = (uint16_t)pActionMappingDesc->mDeviceButtons[1];
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
                    pControl->mStartButton = (uint16_t)pActionMappingDesc->mDeviceButtons[0];
                    pControl->mTarget = (pActionMappingDesc->mNumAxis == 2 ? (1 << 1) | 1 : 1);
                    pControl->mDelta = pActionMappingDesc->mDelta ? (1 << 1) | 1 : 0;
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
                pControl->mPressButton = (uint16_t)pActionMappingDesc->mDeviceButtons[0];
                pControl->mTriggerButton = (uint16_t)pActionMappingDesc->mDeviceButtons[1];
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
                // It's a normal tap button
                if (pActionMappingDesc->mDeviceButtons[0] == TOUCH_BUTTON_NONE)
                {
                    IControl* pControl = AllocateControl<IControl>(mTouchDeviceID);
                    ASSERT(pControl);

                    pControl->mType = CONTROL_BUTTON;
                    pControl->mAction = action;
                    arrpush(mControls[mTouchDeviceID][TOUCH_DOWN(pActionMappingDesc->mUserId)], pControl);
                }
                else // It's an axis
                {
                    // Ensure # axis is correct
                    ASSERT(pActionMappingDesc->mNumAxis == 1 || pActionMappingDesc->mNumAxis == 2);

                    FloatControl* pControl = AllocateControl<FloatControl>(mTouchDeviceID);
                    ASSERT(pControl);

                    memset((void*)pControl, 0, sizeof(*pControl));
                    pControl->mType = CONTROL_FLOAT;
                    pControl->mStartButton = pActionMappingDesc->mDeviceButtons[0];
                    pControl->mTarget = (pActionMappingDesc->mNumAxis == 2 ? (1 << 1) | 1 : 1);
                    pControl->mDelta = pActionMappingDesc->mDelta ? (1 << 1) | 1 : 0;
                    pControl->mAction = action;
                    pControl->mScale = pActionMappingDesc->mScale;
                    pControl->mScaleByDT = pActionMappingDesc->mScaleByDT;
                    pControl->mArea = pActionMappingDesc->mTouchScreenArea;

                    arrpush(mControls[mTouchDeviceID][gainput::Touch0Down], pControl);
                    arrpush(mControls[mTouchDeviceID][gainput::Touch1Down], pControl);
                    arrpush(mControls[mTouchDeviceID][gainput::Touch2Down], pControl);

                    if (pActionMappingDesc->mDeviceButtons[0] == TOUCH_AXIS_X)
                    {
                        arrpush(mControls[mTouchDeviceID][gainput::Touch0X], pControl);
                        arrpush(mControls[mTouchDeviceID][gainput::Touch1X], pControl);
                        arrpush(mControls[mTouchDeviceID][gainput::Touch2X], pControl);
                    }

                    if (pActionMappingDesc->mDeviceButtons[0] == TOUCH_AXIS_Y || pActionMappingDesc->mNumAxis > 1)
                    {
                        arrpush(mControls[mTouchDeviceID][gainput::Touch0Y], pControl);
                        arrpush(mControls[mTouchDeviceID][gainput::Touch1Y], pControl);
                        arrpush(mControls[mTouchDeviceID][gainput::Touch2Y], pControl);
                    }
                }

                break;
            }
            case INPUT_ACTION_MAPPING_TOUCH_GESTURE:
            {
#ifndef NX64
                GestureControl* pControl = AllocateControl<GestureControl>(mTouchDeviceID);
                ASSERT(pControl);

                pControl->mType = CONTROL_GESTURE;
                pControl->mAction = action;
                pControl->mPerformed = 0;

                pControl->mGestureType = (TouchGesture)pActionMappingDesc->mDeviceButtons[0];

                switch (pControl->mGestureType)
                {
                case TOUCH_GESTURE_TAP:
                    arrpush(mControls[mTouchDeviceID][gainput::Touch0Down], pControl);
                    pControl->mTarget = 1;
                    break;
                case TOUCH_GESTURE_DOUBLE_TAP:
                    arrpush(mControls[mTouchDeviceID][gainput::Touch0Down], pControl);
                    pControl->mTarget = 1;
                    break;
                case TOUCH_GESTURE_PAN:
                    for (int finger = 0; finger < MAX_INPUT_MULTI_TOUCHES; finger++)
                    {
                        int idxOffset = finger * GAINPUT_TOUCH_BUTTONS_PER_FINGER;
                        arrpush(mControls[mTouchDeviceID][gainput::Touch0Down + idxOffset], pControl);
                        arrpush(mControls[mTouchDeviceID][gainput::Touch0X + idxOffset], pControl);
                        arrpush(mControls[mTouchDeviceID][gainput::Touch0Y + idxOffset], pControl);
                    }
                    pControl->mTarget = 1;
                    break;
                case TOUCH_GESTURE_SWIPE:
                    arrpush(mControls[mTouchDeviceID][gainput::Touch0Down], pControl);
                    arrpush(mControls[mTouchDeviceID][gainput::Touch0X], pControl);
                    pControl->mTarget = 1;
                    break;
                case TOUCH_GESTURE_PINCH:
                case TOUCH_GESTURE_ROTATE:
                    arrpush(mControls[mTouchDeviceID][gainput::Touch0X], pControl);
                    arrpush(mControls[mTouchDeviceID][gainput::Touch0Down], pControl);
                    arrpush(mControls[mTouchDeviceID][gainput::Touch1X], pControl);
                    arrpush(mControls[mTouchDeviceID][gainput::Touch1Down], pControl);
                    pControl->mTarget = 2;
                    break;
                case TOUCH_GESTURE_LONG_PRESS:
                    // This specific array is looped in update()
                    arrpush(mControls[mTouchDeviceID][gainput::Touch0Down], pControl);
                    arrpush(mControls[mTouchDeviceID][gainput::Touch1X], pControl);
                    pControl->mTarget = 1;
                    break;
                default:
                    ASSERT(0);
                }
#endif

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
                pControl->mArea = pActionMappingDesc->mTouchScreenArea;
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
            const unsigned index = DEV_PAD_START + pDesc->mUserId;

            ActionMappingDesc* pActionMappingDesc = &mInputActionMappingIdToDesc[index][pDesc->mActionId];
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

    void RemoveInputActionControls(const InputActionDesc* pDesc, const unsigned index)
    {
        for (ptrdiff_t i = 0; i < arrlen(mControls[index]); ++i)
        {
            if (arrlen(mControls[index][i]) > 0)
            {
                for (ptrdiff_t j = arrlen(mControls[index][i]) - 1; j >= 0; --j)
                {
                    if (mControls[index][i][j]->mAction == *pDesc)
                    {
                        // Free is from the controls pool first and remove the entry
                        for (ptrdiff_t k = 0; k < arrlen(mControlPool[index]); ++k)
                        {
                            if (mControls[index][i][j] == mControlPool[index][k])
                            {
                                tf_free(mControlPool[index][k]);
                                arrdel(mControlPool[index], k);
                                break;
                            }
                        }

                        // Then remove the entry from mControls
                        arrdel(mControls[index][i], j);
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
            RemoveInputActionControls(pDesc, DEV_PAD_START + pDesc->mUserId);
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
            RemoveInputActionControls(pDesc, mTouchDeviceID);
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

    void AddActionMappings(ActionMappingDesc* const actionMappings, const uint32_t numActions,
                           const InputActionMappingDeviceTarget actionMappingTarget)
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
            ASSERT(INPUT_ACTION_MAPPING_TARGET_ALL !=
                   pActionMappingDesc->mActionMappingDeviceTarget); // target cannot be INPUT_ACTION_MAPPING_TARGET_ALL in the desc

            if (pActionMappingDesc != NULL) //-V547
            {
                // Ensure action mapping ID is within acceptable range
                ASSERT(pActionMappingDesc->mActionId < MAX_INPUT_ACTIONS);

                unsigned index = ~0u;

                switch (pActionMappingDesc->mActionMappingDeviceTarget)
                {
                case INPUT_ACTION_MAPPING_TARGET_CONTROLLER:
                {
                    index = DEV_PAD_START + pActionMappingDesc->mUserId;
                    break;
                }
                case INPUT_ACTION_MAPPING_TARGET_KEYBOARD:
                {
                    index = mKeyboardDeviceID;
                    break;
                }
                case INPUT_ACTION_MAPPING_TARGET_MOUSE:
                {
                    index = mMouseDeviceID;
                    break;
                }
                case INPUT_ACTION_MAPPING_TARGET_TOUCH:
                {
                    index = mTouchDeviceID;
                    // Ensure the proper action mapping type is used
                    ASSERT(INPUT_ACTION_MAPPING_NORMAL == pActionMappingDesc->mActionMappingType ||
                           INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK == pActionMappingDesc->mActionMappingType ||
                           INPUT_ACTION_MAPPING_TOUCH_GESTURE == pActionMappingDesc->mActionMappingType);
                    break;
                }
                default:
                    ASSERT(0); // should never get here
                }

                ASSERT(index != ~0u);
                switch (pActionMappingDesc->mActionMappingType)
                {
                case INPUT_ACTION_MAPPING_NORMAL:
                case INPUT_ACTION_MAPPING_COMPOSITE:
                case INPUT_ACTION_MAPPING_COMBO:
                case INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK:
                case INPUT_ACTION_MAPPING_TOUCH_GESTURE:
                {
                    ASSERT(mInputActionMappingIdToDesc[index][pActionMappingDesc->mActionId].mActionMappingDeviceTarget ==
                           INPUT_ACTION_MAPPING_TARGET_ALL);
                    mInputActionMappingIdToDesc[index][pActionMappingDesc->mActionId] = *pActionMappingDesc;

                    // Register an action for UI action mappings so that the app can intercept them via the global action
                    // (GLOBAL_INPUT_ACTION_ANY_BUTTON_ACTION)
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

    void RemoveActionMappingsControls(const gainput::DeviceId deviceId)
    {
        for (ptrdiff_t j = 0; j < arrlen(mControlPool[deviceId]); ++j)
            tf_free(mControlPool[deviceId][j]);
        arrfree(mControlPool[deviceId]);

        for (ptrdiff_t j = 0; j < arrlen(mControls[deviceId]); ++j)
            arrfree(mControls[deviceId][j]);
    }

    void RemoveActionMappings(const InputActionMappingDeviceTarget actionMappingTarget)
    {
        if (INPUT_ACTION_MAPPING_TARGET_CONTROLLER == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
            {
                const unsigned index = DEV_PAD_START + i;
                memset((void*)mInputActionMappingIdToDesc[index], 0, sizeof(mInputActionMappingIdToDesc[index][0]) * MAX_INPUT_ACTIONS);
                RemoveActionMappingsControls(index);
                memset(mControls[index], 0, sizeof(mControls[index][0]) * gainput::PadButtonMax_);
            }
        }
        if (INPUT_ACTION_MAPPING_TARGET_KEYBOARD == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            memset((void*)mInputActionMappingIdToDesc[mKeyboardDeviceID], 0,
                   sizeof(mInputActionMappingIdToDesc[mKeyboardDeviceID][0]) * MAX_INPUT_ACTIONS);
            RemoveActionMappingsControls(mKeyboardDeviceID);
            memset(mControls[mKeyboardDeviceID], 0, sizeof(mControls[mKeyboardDeviceID][0]) * gainput::KeyCount_);
        }
        if (INPUT_ACTION_MAPPING_TARGET_MOUSE == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            memset((void*)mInputActionMappingIdToDesc[mMouseDeviceID], 0,
                   sizeof(mInputActionMappingIdToDesc[mMouseDeviceID][0]) * MAX_INPUT_ACTIONS);
            RemoveActionMappingsControls(mMouseDeviceID);
            memset(mControls[mMouseDeviceID], 0, sizeof(mControls[mMouseDeviceID][0]) * gainput::MouseButtonCount_);

            // Need to do the same for the raw mouse device
            memset((void*)mInputActionMappingIdToDesc[mRawMouseDeviceID], 0,
                   sizeof(mInputActionMappingIdToDesc[mRawMouseDeviceID][0]) * MAX_INPUT_ACTIONS);
            RemoveActionMappingsControls(mRawMouseDeviceID);
            memset(mControls[mRawMouseDeviceID], 0, sizeof(mControls[mRawMouseDeviceID][0]) * gainput::MouseButtonCount_);
        }
        if (INPUT_ACTION_MAPPING_TARGET_TOUCH == actionMappingTarget || INPUT_ACTION_MAPPING_TARGET_ALL == actionMappingTarget)
        {
            memset((void*)mInputActionMappingIdToDesc[mTouchDeviceID], 0,
                   sizeof(mInputActionMappingIdToDesc[mTouchDeviceID][0]) * MAX_INPUT_ACTIONS);
            RemoveActionMappingsControls(mTouchDeviceID);
            memset(mControls[mTouchDeviceID], 0, sizeof(mControls[mTouchDeviceID][0]) * gainput::TouchCount_);
        }
    }

    // ----- OS Input Quirks

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
            // we want everything to resize with main view.
            [newView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight |
                                          UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleLeftMargin |
                                          UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleBottomMargin)];
#else
            NSView*              mainView = (__bridge NSView*)view;
            float                retinScale = ((CAMetalLayer*)(mainView.layer)).drawableSize.width / mainView.frame.size.width;
            // Use view.window.contentLayoutRect instead of view.frame as a frame to avoid capturing inputs over title bar
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

        // automatic reference counting
        // it will get deallocated.
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
        UNREF_PARAM(type);
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
            JNIEnv*          jni;
            jint             result = activity->vm->AttachCurrentThread(&jni, NULL);
            if (result == JNI_ERR)
            {
                ASSERT(0);
                return;
            }

            jclass    cls = jni->GetObjectClass(activity->clazz);
            jmethodID methodID = jni->GetMethodID(cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
            jstring   serviceName = jni->NewStringUTF("input_method");
            jobject   inputService = jni->CallObjectMethod(activity->clazz, methodID, serviceName);

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

    // ----- Utils

    inline constexpr bool IsPointerType(gainput::DeviceId device) const
    {
#if TOUCH_INPUT
        return false;
#else
        return (device == mMouseDeviceID || device == mRawMouseDeviceID);
#endif
    }

    uint32_t IdToIndex(gainput::DeviceId deviceId)
    {
        uint32_t index = deviceId;

        if (index >= DEV_PAD_START)
        {
            // default to first slot
            index = DEV_PAD_START;

            for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
            {
                if (pGamepadDeviceIDs[i] == deviceId)
                {
                    index += i;
                    break;
                }
            }
        }

        return index;
    }

    // ----- gainput::InputListener overrides

    bool OnDeviceButtonBool(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, bool oldValue, bool newValue)
    {
        if (oldValue == newValue)
            return false;

        uint32_t device = IdToIndex(deviceId);

        if (arrlen(mControls[device]))
        {
            InputActionContext ctx = {};
            ctx.mDeviceType = (uint8_t)pDeviceTypes[device];
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

                // Reset when starting/ending touch
                if (oldValue != newValue)
                    mTouchDownTime[touchIndex] = 0.f;
            }
#else
            if (IsPointerType(device))
            {
                gainput::InputDeviceMouse* pMouse = (gainput::InputDeviceMouse*)pInputManager->GetDevice(mMouseDeviceID);
                mMousePosition[0] = pMouse->GetFloat(gainput::MouseAxisX);
                mMousePosition[1] = pMouse->GetFloat(gainput::MouseAxisY);
                ctx.pPosition = &mMousePosition;

                // Scroll wheel position happens over three events
                // Movement start (delta is 0), movement (delta changes), movement end (delta is 0)
                // We only want to send the event when the delta changes
                static int32_t previousMovementWheelPosition = 0;
                static int32_t persistentScrollValue = 0;

                int32_t mouseWheelCurrentPosition = 0;
                if (deviceButton == gainput::MouseButtonWheelUp)
                    mouseWheelCurrentPosition = (int32_t)pMouse->GetFloat(gainput::MouseButtonWheelUp);
                else if (deviceButton == gainput::MouseButtonWheelDown)
                    mouseWheelCurrentPosition = (int32_t)pMouse->GetFloat(gainput::MouseButtonWheelDown);

                // Make sure delta value is based on the previous movement event
                const int32_t mouseWheelPositionDelta = mouseWheelCurrentPosition - previousMovementWheelPosition;
                if (mouseWheelCurrentPosition != 0 && mouseWheelPositionDelta != 0)
                {
                    persistentScrollValue = mouseWheelPositionDelta;
                    previousMovementWheelPosition = mouseWheelCurrentPosition;
                }

                ctx.mScrollValue = persistentScrollValue;
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
                ctx.mUserId = pDesc->mUserId;
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
                        {
                            ctx.pUserData = mGlobalAnyButtonAction.pUserData;
                            mGlobalAnyButtonAction.pFunction(&ctx);
                        }
#if TOUCH_INPUT
                        IControlSet val = { control };
                        hmputs(mButtonControlPerformQueue, val);
#else
                        ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
                        if (pDesc->pFunction)
                            executeNext = pDesc->pFunction(&ctx) && executeNext;
                        if (mGlobalAnyButtonAction.pFunction)
                        {
                            ctx.pUserData = mGlobalAnyButtonAction.pUserData;
                            mGlobalAnyButtonAction.pFunction(&ctx);
                        }
#endif
                    }
                    else if (oldValue && !newValue)
                    {
                        ctx.mPhase = INPUT_ACTION_PHASE_CANCELED;
                        if (pDesc->pFunction)
                            executeNext = pDesc->pFunction(&ctx) && executeNext;
                        if (mGlobalAnyButtonAction.pFunction)
                        {
                            ctx.pUserData = mGlobalAnyButtonAction.pUserData;
                            mGlobalAnyButtonAction.pFunction(&ctx);
                        }
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
                        for (uint8_t j = 0; j < pControl->mComposite; ++j)
                        {
                            if (pControl->mPerformed[j])
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
                            pControl->mValue[axis] =
                                (float)pControl->mPressedVal[axis * 2 + 0] - (float)pControl->mPressedVal[axis * 2 + 1];
                            ctx.mFloat2 = pControl->mValue;
                            executeNext = pDesc->pFunction(&ctx) && executeNext;
                        }
                    }

                    break;
                }
                // Mouse scroll is using OnDeviceButtonBool
                case CONTROL_FLOAT:
                {
                    FloatControl* pControl = (FloatControl*)control;
#if TOUCH_INPUT
                    const uint32_t fingerIdx = deviceButton / GAINPUT_TOUCH_BUTTONS_PER_FINGER;
                    if (mTouchDeviceID == device)
                    {
                        if (!oldValue && newValue)
                        {
                            ASSERT(ctx.pPosition);

                            mTouchDownTime[touchIndex] = 0.f;

                            const float2 displaySize{ pInputManager->GetDisplayWidth(), pInputManager->GetDisplayHeight() };
                            if (!isPositionInsideScreenArea(*ctx.pPosition, (TouchScreenArea)pControl->mArea, displaySize))
                                break;

                            ctx.mFingerIndices[0] = touchIndex;

                            if (pDesc->pFunction)
                            {
                                ctx.mPhase = INPUT_ACTION_PHASE_STARTED;
                                executeNext = pDesc->pFunction(&ctx) && executeNext;

                                if (mGlobalAnyButtonAction.pFunction)
                                {
                                    ctx.pUserData = mGlobalAnyButtonAction.pUserData;
                                    mGlobalAnyButtonAction.pFunction(&ctx);
                                }
                            }
                        }
                        else if (oldValue && !newValue)
                        {
                            if (fingerIdx == touchIndex)
                            {
                                mTouchDownTime[touchIndex] = 0.f;
                                ctx.mFingerIndices[0] = touchIndex;

                                pControl->mStarted = 0;
                                pControl->mPerformed = 0;

                                ctx.mFloat2 = float2(0.0f);
                                ctx.mPhase = INPUT_ACTION_PHASE_CANCELED;
                                ctx.mActionId = pControl->mAction.mActionId;

                                if (pDesc->pFunction)
                                    executeNext = pDesc->pFunction(&ctx) && executeNext;

                                if (mGlobalAnyButtonAction.pFunction)
                                {
                                    ctx.pUserData = mGlobalAnyButtonAction.pUserData;
                                    mGlobalAnyButtonAction.pFunction(&ctx);
                                }
                            }
                        }
                    }
#endif
                    if (mMouseDeviceID == device)
                    {
                        if (!oldValue && newValue)
                        {
                            ASSERT(deviceButton == gainput::MouseButtonWheelUp || deviceButton == gainput::MouseButtonWheelDown);

                            ctx.mFloat2[1] = deviceButton == gainput::MouseButtonWheelUp ? 1.0f : -1.0f;

                            if (pDesc->pFunction)
                            {
                                ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
                                executeNext = pDesc->pFunction(&ctx) && executeNext;
                            }

                            FloatControlSet val = { pControl };
                            hmputs(mFloatDeltaControlCancelQueue, val);
                        }
                    }
                    break;
                }
#if TOUCH_INPUT
                case CONTROL_GESTURE:
                {
                    if (device == mTouchDeviceID)
                    {
                        if (!oldValue && newValue)
                        {
                            GestureRecognizer::Touch* touch = mGestureRecognizer.AddTouch(touchIndex);

                            mGestureRecognizer.mPerformingGesturesCount[touchIndex]++;

                            if (!touch->mUpdated && touch->mState == GestureRecognizer::Touch::ENDED)
                            {
                                touch->mUpdated = true;
                                touch->mState = GestureRecognizer::Touch::STARTED;

                                touch->mPos0 = vec2(ctx.pPosition->getX(), ctx.pPosition->getY());
                                touch->mPos = touch->mPos0;
                            }

                            GestureControl* pControl = (GestureControl*)control;

                            if (pControl->mGestureType == TOUCH_GESTURE_LONG_PRESS)
                            {
                                mGestureRecognizer.mLongPressTouch = touch;
                            }
                        }
                        else if (oldValue && !newValue)
                        {
                            GestureRecognizer::Touch* touch = mGestureRecognizer.FindTouch(touchIndex);
                            ASSERT(touch);

                            mGestureRecognizer.mPerformingGesturesCount[touchIndex]--;

                            touch->mState = GestureRecognizer::Touch::ENDED;
                            ctx.mPhase = INPUT_ACTION_PHASE_ENDED;

                            GestureControl* pControl = (GestureControl*)control;

                            for (uint32_t i = 0; i < MAX_INPUT_MULTI_TOUCHES; ++i)
                                ctx.mFingerIndices[i] = mGestureRecognizer.mTouches[i].mID;

                            // Taps
                            switch (pControl->mGestureType)
                            {
                            case TOUCH_GESTURE_TAP:
                            {
                                ctx.mBool = true;
                                ctx.pCaptured = &mDefaultCapture;
                                pControl->mAction.pFunction(&ctx);

                                break;
                            }
                            case TOUCH_GESTURE_PAN:
                            {
                                ctx.mBool = false;
                                ctx.pCaptured = &mDefaultCapture;

                                ctx.mFingerIndices[0] = touchIndex;
                                ctx.mFloat2 = { touch->mPos.getX(), touch->mPos.getY() };
                                pControl->mAction.pFunction(&ctx);

                                break;
                            }
                            case TOUCH_GESTURE_DOUBLE_TAP:
                            {
                                if (mGestureRecognizer.mActiveTouches != pControl->mTarget)
                                    break;

                                if (length(touch->mPos - mGestureRecognizer.mLastTapPos) > mGestureRecognizer.mMovedDistThreshold ||
                                    mGestureRecognizer.mLastTapTime > mGestureRecognizer.mDoubleTapTimeThreshold)
                                    break;

                                ctx.mBool = true;
                                ctx.pCaptured = &mDefaultCapture;

                                pControl->mAction.pFunction(&ctx);

                                break;
                            }
                            case TOUCH_GESTURE_SWIPE:
                            {
                                if (mGestureRecognizer.mActiveTouches != pControl->mTarget)
                                    break;

                                // We don't care about other touch indices

                                if (!touch)
                                    break;

                                vec2 dir = normalize(touch->mDistTraveled);

                                if (isnan(dir.getX()) || isnan(dir.getY()))
                                    break;

                                if (abs(dir.getX()) > abs(dir.getY()))
                                {
                                    dir.setX(sign(dir.getX()));
                                    dir.setY(0.0f);
                                }
                                else
                                {
                                    dir.setX(0.0f);
                                    dir.setY(sign(dir.getY()));
                                }

                                ctx.mFloat4 = { touch->mDistTraveled.getX(), touch->mDistTraveled.getY(), dir.getX(), dir.getY() };

                                ctx.pCaptured = &mDefaultCapture;

                                if (touch->mVelocity > mGestureRecognizer.mSwipeVelocityThreshold ||
                                    abs(touch->mDistTraveled.getX()) > mGestureRecognizer.mSwipeDistThreshold ||
                                    abs(touch->mDistTraveled.getY()) > mGestureRecognizer.mSwipeDistThreshold)
                                {
                                    pControl->mAction.pFunction(&ctx);
                                }

                                break;
                            }
                            case TOUCH_GESTURE_LONG_PRESS:
                            {
                                if (mGestureRecognizer.mLongPressTouch &&
                                    mGestureRecognizer.mLongPressTouch->mTime > mGestureRecognizer.mLongPressTimeThreshold)
                                {
                                    ctx.mBool = false;
                                    ctx.pCaptured = &mDefaultCapture;

                                    pControl->mAction.pFunction(&ctx);
                                    mGestureRecognizer.mLongPressTouch = nullptr;
                                }
                                break;
                            }
                            default:
                                break;
                            }
                        }
                    }

                    break;
                }
                case CONTROL_VIRTUAL_JOYSTICK:
                {
                    VirtualJoystickControl* pControl = (VirtualJoystickControl*)control;

                    if (!oldValue && newValue && !pControl->mStarted)
                    {
                        const float2 displaySize{ pInputManager->GetDisplayWidth(), pInputManager->GetDisplayHeight() };

                        pControl->mStartPos = mTouchPositions[touchIndex];
                        if (isPositionInsideScreenArea(pControl->mStartPos, (TouchScreenArea)pControl->mArea, displaySize))
                        {
                            pControl->mStarted = 0x3;
                            pControl->mTouchIndex = touchIndex;
                            pControl->mCurrPos = pControl->mStartPos;

                            ctx.mPhase = INPUT_ACTION_PHASE_STARTED;
                            ctx.mFloat2 = float2(0.0f);
                            ctx.pPosition = &pControl->mCurrPos;
                            ctx.mActionId = pControl->mAction.mActionId;

                            if (gVirtualJoystick)
                                virtualJoystickOnMove(gVirtualJoystick, virtualJoystickIndexFromArea((TouchScreenArea)pControl->mArea),
                                                      &ctx);

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

                            if (gVirtualJoystick)
                                virtualJoystickOnMove(gVirtualJoystick, virtualJoystickIndexFromArea((TouchScreenArea)pControl->mArea),
                                                      &ctx);

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

    bool OnDeviceButtonFloat(float deltaTime, gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, float oldValue,
                             float newValue)
    {
        float2* pPosition = NULL;

        uint32_t device = IdToIndex(deviceId);

#if TOUCH_INPUT
        bool touchJustStarted = false;

        const uint32_t touchIndex = TOUCH_USER(deviceButton);
        if (mTouchDeviceID == device)
        {
            // The first frame that a touch starts we get the touch position of the previous touch in oldValue,
            // for controls that use deltas we would get a huge delta. To prevent this we want to ignore the oldValue for a touch
            // that just started.
            touchJustStarted = (mTouchDownTime[touchIndex] == 0.f);

            const uint32_t fingerIdx = deviceButton / GAINPUT_TOUCH_BUTTONS_PER_FINGER;
            const uint32_t fingerButton = deviceButton - fingerIdx * GAINPUT_TOUCH_BUTTONS_PER_FINGER;

            gainput::InputDeviceTouch* pTouch = (gainput::InputDeviceTouch*)pInputManager->GetDevice(mTouchDeviceID);

            switch ((gainput::TouchButton)fingerButton)
            {
            case gainput::TouchButton::Touch0Down:
                ASSERT(false && "Handled in OnDeviceButtonBool");
                return true;
            case gainput::TouchButton::Touch0X:
                // We recive Touch0X and Touch0Y always, we only want to track one of these as elapsed time
                if (oldValue && newValue)
                    mTouchDownTime[touchIndex] += deltaTime;
                // fallthrough

            case gainput::TouchButton::Touch0Y:
                mTouchPositions[touchIndex][0] = pTouch->GetFloat(TOUCH_X(touchIndex));
                mTouchPositions[touchIndex][1] = pTouch->GetFloat(TOUCH_Y(touchIndex));
                break; // We continue to send the axis event data

            case gainput::TouchButton::Touch0Pressure:
                // Pressure is the last element that Gainput notifies us about
                return true;

            default:
                ASSERT(false);
                break;
            }

            pPosition = &mTouchPositions[touchIndex];

            const uint32_t axisIndex = fingerButton - gainput::TouchButton::Touch0X;
            ASSERT(axisIndex < 2);
        }
#else
        FORGE_CONSTEXPR const bool touchJustStarted = false;
        if (IsPointerType(device))
        {
            gainput::InputDeviceMouse* pMouse = (gainput::InputDeviceMouse*)pInputManager->GetDevice(mMouseDeviceID);
            mMousePosition[0] = pMouse->GetFloat(gainput::MouseAxisX);
            mMousePosition[1] = pMouse->GetFloat(gainput::MouseAxisY);
            pPosition = &mMousePosition;
        }
#endif
        ptrdiff_t deviceButtonCount = arrlen(mControls[device]);
        if (deviceButtonCount > 0 && deviceButton < deviceButtonCount)
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
                ctx.mDeviceType = (uint8_t)pDeviceTypes[device];
                ctx.pUserData = pDesc->pUserData;
                ctx.pCaptured = IsPointerType(device) ? &mInputCaptured : &mDefaultCapture;
                ctx.mActionId = pDesc->mActionId;
                ctx.pPosition = pPosition;
                ctx.mUserId = pDesc->mUserId;

                switch (type)
                {
                case CONTROL_FLOAT:
                {
                    FloatControl* pControl = (FloatControl*)control;
                    uint32_t      axis = (deviceButton - pControl->mStartButton);

#if TOUCH_INPUT
                    // We need to determine touch axis in a custom way, each finger has it's own axis value
                    if (mTouchDeviceID == device)
                    {
                        const uint32_t fingerIdx = deviceButton / GAINPUT_TOUCH_BUTTONS_PER_FINGER;
                        //						if (pControl->mAction.mUserId != fingerIdx)
                        //							break; // This control does not care about this finger

                        ASSERT(pPosition);

                        const float2 displaySize{ pInputManager->GetDisplayWidth(), pInputManager->GetDisplayHeight() };
                        if (!isPositionInsideScreenArea(*pPosition, (TouchScreenArea)pControl->mArea, displaySize))
                            break;

                        ctx.mFingerIndices[0] = fingerIdx;

                        const uint32_t deviceAxis = deviceButton - fingerIdx * GAINPUT_TOUCH_BUTTONS_PER_FINGER;
                        ASSERT(deviceAxis == TOUCH_AXIS_X || deviceAxis == TOUCH_AXIS_Y && "CONTROL_FLOAT expects an X or Y value");
                        if (deviceAxis == TOUCH_AXIS_X)
                        {
                            axis = 0;
                        }
                        else
                        {
                            axis = 1;
                        }
                    }
#endif

                    if (pControl->mDelta & 0x1)
                    {
                        const float deltaValue = touchJustStarted ? 0.f : newValue - oldValue;
                        pControl->mValue[axis] +=
                            (axis > 0 ? -1.0f : 1.0f) * deltaValue * pControl->mScale / (pControl->mScaleByDT ? deltaTime : 1);
                        ctx.mFloat3 = pControl->mValue;

                        if (((pControl->mStarted >> axis) & 0x1) == 0)
                        {
                            pControl->mStarted |= (1 << axis);
                            if (pControl->mStarted == pControl->mTarget)
                            {
                                ctx.mPhase = INPUT_ACTION_PHASE_STARTED;

                                if (pDesc->pFunction)
                                    executeNext = pDesc->pFunction(&ctx) && executeNext;

                                if (mGlobalAnyButtonAction.pFunction)
                                {
                                    ctx.pUserData = mGlobalAnyButtonAction.pUserData;
                                    mGlobalAnyButtonAction.pFunction(&ctx);
                                }
                            }

                            FloatControlSet val = { pControl };
                            hmputs(mFloatDeltaControlCancelQueue, val);
                        }

                        pControl->mPerformed |= (1 << axis);

                        if (pControl->mPerformed == pControl->mTarget)
                        {
                            pControl->mPerformed = 0;
                            ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
                            if (pDesc->pFunction)
                                executeNext = pDesc->pFunction(&ctx) && executeNext;

                            if (mGlobalAnyButtonAction.pFunction)
                            {
                                ctx.pUserData = mGlobalAnyButtonAction.pUserData;
                                mGlobalAnyButtonAction.pFunction(&ctx);
                            }
                        }
                    }
                    else if (pDesc->pFunction)
                    {
                        pControl->mPerformed |= (1 << axis);
                        pControl->mValue[axis] = newValue;
                        if (pControl->mPerformed == pControl->mTarget)
                        {
                            pControl->mPerformed = 0;
                            ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
                            ctx.mFloat3 = pControl->mValue;
                            executeNext = pDesc->pFunction(&ctx) && executeNext;

                            if (mGlobalAnyButtonAction.pFunction)
                            {
                                ctx.pUserData = mGlobalAnyButtonAction.pUserData;
                                mGlobalAnyButtonAction.pFunction(&ctx);
                            }
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
                        for (uint32_t j = 0; j < pControl->mAxisCount; ++j)
                            equal = equal && (pControl->mValue[j] == pControl->mNewValue[j]);

                        pControl->mValue = pControl->mNewValue;

                        ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
                        ctx.mFloat3 = pControl->mValue;

                        if (!equal)
                        {
                            if (pDesc->pFunction)
                                executeNext = pDesc->pFunction(&ctx) && executeNext;
                            if (mGlobalAnyButtonAction.pFunction)
                            {
                                ctx.pUserData = mGlobalAnyButtonAction.pUserData;
                                mGlobalAnyButtonAction.pFunction(&ctx);
                            }
                        }
                    }
                    else
                        continue;

                    pControl->mPerformed = 0;
                    break;
                }
                case CONTROL_COMPOSITE:
                {
                    CompositeControl* pControl = (CompositeControl*)control;
                    uint32_t          index = 0;
                    for (; index < pControl->mComposite; ++index)
                        if (deviceButton == pControl->mControls[index])
                            break;

                    const uint32_t axis = index & 1;
                    const float    prevValue = pControl->mValue[axis];
                    pControl->mValue[axis] = newValue;
                    if (newValue == prevValue)
                    {
                        continue;
                    }
                    else if (prevValue == 0.0)
                    {
                        ctx.mPhase = INPUT_ACTION_PHASE_STARTED;
                        pControl->mPressedVal[index] = 1;
                        pControl->mStarted = 1;
                    }
                    else if (newValue == 0.0)
                    {
                        ctx.mPhase = INPUT_ACTION_PHASE_CANCELED;
                        pControl->mPressedVal[index] = 0;
                        pControl->mPerformed[index] = 0;
                        bool anyPressed = false;
                        for (uint32_t j = 0; j < pControl->mComposite; ++j)
                        {
                            anyPressed |= pControl->mPressedVal[j] != 0;
                        }
                        if (!anyPressed)
                            pControl->mStarted = 0;
                    }
                    else
                    {
                        ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
                        pControl->mPerformed[index] = 1;
                    }
                    ctx.mFloat = pControl->mValue[0] - pControl->mValue[1];
                    executeNext = pDesc->pFunction(&ctx) && executeNext;
                    break;
                }

#if TOUCH_INPUT
                case CONTROL_GESTURE:
                {
                    const uint32_t deviceAxis = deviceButton - touchIndex * GAINPUT_TOUCH_BUTTONS_PER_FINGER;

                    GestureControl* pControl = (GestureControl*)control;

                    if (deviceAxis == TOUCH_AXIS_Y) // prevent processing two axes
                        continue;

                    if (pControl->mAction.pFunction)
                    {
                        GestureRecognizer::Touch* touch = mGestureRecognizer.FindTouch(touchIndex);

                        if (!touch)
                            continue;

                        if (touch->mState == GestureRecognizer::Touch::ENDED)
                            continue;

                        // save touch positions only while processing the first gesture
                        if (!touch->mUpdated)
                        {
                            touch->mUpdated = true;

                            touch->mPos0 = touch->mPos;
                            touch->mPos = vec2(pPosition->getX(), pPosition->getY());
                            touch->mDistTraveled += touch->mPos - touch->mPos0;
                            touch->mVelocity = length(touch->mPos - touch->mPos0) / deltaTime;
                        }

                        for (uint32_t i = 0; i < MAX_INPUT_MULTI_TOUCHES; ++i)
                            ctx.mFingerIndices[i] = mGestureRecognizer.mTouches[i].mID;

                        switch (pControl->mGestureType)
                        {
                        case TOUCH_GESTURE_PINCH:
                        {
                            if (mGestureRecognizer.mActiveTouches != pControl->mTarget)
                                continue;

                            GestureRecognizer::Touch* touch[2];
                            touch[0] = mGestureRecognizer.FindTouch(0);
                            touch[1] = mGestureRecognizer.FindTouch(1);

                            if (!touch[0] || !touch[1])
                                continue;

                            if (!touch[0]->mUpdated || !touch[1]->mUpdated)
                                continue;

                            float dist1 = length(touch[1]->mPos - touch[0]->mPos);
                            float dist0 = length(touch[1]->mPos0 - touch[0]->mPos0);

                            float velocity = abs(dist1 - dist0) / deltaTime;
                            float scale = dist1 / dist0;

                            if (scale < 0.1f)
                                continue;

                            ctx.mFloat4 = { velocity, scale, touch[1]->mPos.getX() - touch[0]->mPos.getX(),
                                            touch[1]->mPos.getY() - touch[0]->mPos.getY() };

                            ctx.pCaptured = &mDefaultCapture;

                            pControl->mAction.pFunction(&ctx);

                            break;
                        }
                        case TOUCH_GESTURE_ROTATE:
                        {
                            pControl->mPerformed++;
                            if (pControl->mPerformed != pControl->mTarget)
                                break;
                            pControl->mPerformed = 0;

                            if (mGestureRecognizer.mActiveTouches != pControl->mTarget)
                                break;

                            GestureRecognizer::Touch* touch[2];
                            touch[0] = mGestureRecognizer.FindTouch(0);
                            touch[1] = mGestureRecognizer.FindTouch(1);

                            if (!touch[0] || !touch[1])
                                continue;

                            vec2 v1 = touch[1]->mPos - touch[0]->mPos;
                            vec2 v0 = touch[1]->mPos0 - touch[0]->mPos0;

                            float velocity = abs(length(v1) - length(v0)) / deltaTime;
                            float rotation = atan2f(v0.getX() * v1.getY() - v0.getY() * v1.getX(), dot(v0, v1));

                            float scale = length(v1) / length(v0);

                            if (scale < 0.1f)
                                continue;

                            ctx.mFloat4 = { velocity, rotation, touch[1]->mPos.getX() - touch[0]->mPos.getX(),
                                            touch[1]->mPos.getY() - touch[0]->mPos.getY() };

                            ctx.pCaptured = &mDefaultCapture;

                            pControl->mAction.pFunction(&ctx);

                            break;
                        }
                        case TOUCH_GESTURE_PAN:
                        {
                            pControl->mPerformed++;
                            if (pControl->mPerformed != pControl->mTarget)
                                break;

                            pControl->mPerformed = 0;

                            GestureRecognizer::Touch* touch;
                            touch = mGestureRecognizer.FindTouch(touchIndex);

                            if (!touch)
                                continue;

                            ctx.mFingerIndices[0] = touchIndex;
                            ctx.mFloat2 = {
                                mTouchPositions[touchIndex][0] - touch->mPos.getX(),
                                mTouchPositions[touchIndex][1] - touch->mPos.getY(),
                            };

                            touch->mPos.setX(mTouchPositions[touchIndex][0]);
                            touch->mPos.setY(mTouchPositions[touchIndex][1]);

                            ctx.mPhase = touch->mState == GestureRecognizer::Touch::STARTED ? INPUT_ACTION_PHASE_STARTED
                                                                                            : INPUT_ACTION_PHASE_UPDATED;
                            ctx.mBool = true;
                            ctx.pCaptured = &mDefaultCapture;

                            if (touch->mState == GestureRecognizer::Touch::STARTED)
                                touch->mState = GestureRecognizer::Touch::HOLDING;

                            pControl->mAction.pFunction(&ctx);

                            break;
                        }
                        default:
                            break;
                        }
                    }

                    break;
                }
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
                        vec2  delta = f2Tov2(pControl->mCurrPos - pControl->mStartPos);
                        float halfRad = (pControl->mOutsideRadius * 0.5f) - pControl->mDeadzone;
                        if (length(delta) > halfRad)
                            pControl->mCurrPos = pControl->mStartPos + halfRad * v2ToF2(normalize(delta));

                        ctx.mPhase = INPUT_ACTION_PHASE_UPDATED;
                        float2 dir = ((pControl->mCurrPos - pControl->mStartPos) / halfRad) * pControl->mScale;
                        ctx.mFloat2 = float2(dir[0], -dir[1]);
                        ctx.pPosition = &pControl->mCurrPos;
                        ctx.mActionId = pControl->mAction.mActionId;
                        ctx.mFingerIndices[0] = pControl->mTouchIndex;

                        if (gVirtualJoystick)
                            virtualJoystickOnMove(gVirtualJoystick, virtualJoystickIndexFromArea((TouchScreenArea)pControl->mArea), &ctx);

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

    bool OnDeviceButtonGesture(float deltaTime, gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton,
                               const struct gainput::GestureChange& gesture)
    {
        UNREF_PARAM(deltaTime);
        UNREF_PARAM(deviceId);
        UNREF_PARAM(deviceButton);
        UNREF_PARAM(gesture);
        // uint32_t device = IdToIndex(deviceId);
        return true;
    }

    int GetPriority() const { return 0; }

    // ----- GamePad Utils

    static void DeviceChange(void* metadata, gainput::DeviceId deviceId, gainput::InputDevice* device, bool doAdd)
    {
        InputSystemImpl* sys = (InputSystemImpl*)metadata;

        if (doAdd)
            sys->AddGamepad(deviceId, device);
        else
            sys->RemoveGamepad(deviceId, device);
    }

    void AddGamepad(gainput::DeviceId deviceId, gainput::InputDevice* device)
    {
        if (device->GetType() != gainput::InputDevice::DeviceType::DT_PAD)
            return;

        for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
        {
            if (pGamepadDeviceIDs[i] == gainput::InvalidDeviceId)
            {
                pGamepadDeviceIDs[i] = deviceId;

                if (mOnDeviceChangeCallBack)
                    mOnDeviceChangeCallBack(((gainput::InputDevicePad*)device)->GetDeviceName(), true, i);

                break;
            }
        }
    }

    void RemoveGamepad(gainput::DeviceId deviceId, gainput::InputDevice* device)
    {
        if (device->GetType() != gainput::InputDevice::DeviceType::DT_PAD)
            return;

        for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
        {
            if (pGamepadDeviceIDs[i] == deviceId)
            {
                if (mOnDeviceChangeCallBack)
                    mOnDeviceChangeCallBack(((gainput::InputDevicePad*)device)->GetDeviceName(), false, i);

                pGamepadDeviceIDs[i] = gainput::InvalidDeviceId;

                break;
            }
        }
    }

    void SetDeadZone(unsigned gamePadIndex, float deadZoneSize)
    {
        if (gamePadIndex >= MAX_INPUT_GAMEPADS || pGamepadDeviceIDs[gamePadIndex] == gainput::InvalidDeviceId)
            return;
        gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
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
        if (pGamepadDeviceIDs[gamePadIndex] == gainput::InvalidDeviceId)
            return "GamePad Disconnected";
        gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
        return pDevicePad->GetDeviceName();
    }

    bool GamePadConnected(unsigned gamePadIndex)
    {
        if (gamePadIndex >= MAX_INPUT_GAMEPADS || pGamepadDeviceIDs[gamePadIndex] == gainput::InvalidDeviceId)
            return false;
        gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
        return pDevicePad->IsAvailable();
    }

    bool SetRumbleEffect(unsigned gamePadIndex, float left_motor, float right_motor, uint32_t duration_ms, bool vibrateTouchDevice)
    {
        if (gamePadIndex >= MAX_INPUT_GAMEPADS || pGamepadDeviceIDs[gamePadIndex] == gainput::InvalidDeviceId)
            return false;
        gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
        return pDevicePad->SetRumbleEffect(left_motor, right_motor, duration_ms, vibrateTouchDevice);
    }

    void SetLEDColor(unsigned gamePadIndex, uint8_t r, uint8_t g, uint8_t b)
    {
        if (gamePadIndex >= MAX_INPUT_GAMEPADS || pGamepadDeviceIDs[gamePadIndex] == gainput::InvalidDeviceId)
            return;
        gainput::InputDevicePad* pDevicePad = (gainput::InputDevicePad*)pInputManager->GetDevice(pGamepadDeviceIDs[gamePadIndex]);
        pDevicePad->SetLEDColor(r, g, b);
    }

    void setOnDeviceChangeCallBack(void (*onDeviceChnageCallBack)(const char* name, bool added, int))
    {
        mOnDeviceChangeCallBack = onDeviceChnageCallBack;

        for (uint32_t i = 0; i < MAX_INPUT_GAMEPADS; ++i)
        {
            if (pGamepadDeviceIDs[i] != gainput::InvalidDeviceId)
            {
                gainput::InputDevice* device = pInputManager->GetDevice(pGamepadDeviceIDs[i]);

                if (mOnDeviceChangeCallBack)
                    mOnDeviceChangeCallBack(((gainput::InputDevicePad*)device)->GetDeviceName(), true, i);

                break;
            }
        }
    }
};
#endif

/**********************************************/
// Interface
/**********************************************/

#ifdef ENABLE_FORGE_INPUT
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

#endif

int32_t InputSystemHandleMessage(WindowDesc* pWindow, void* msg)
{
    UNREF_PARAM(msg);
    UNREF_PARAM(pWindow);
#ifdef ENABLE_FORGE_INPUT

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
    if (msg)
    {
        NSNotificationName name = ((__bridge NSNotification*)msg).name;
        // Reset input states when we lose focus
        if (name == NSWindowDidBecomeMainNotification || name == NSWindowDidResignMainNotification ||
            name == NSWindowDidResignKeyNotification)
        {
            ResetInputStates();
        }
    }
#elif defined(__ANDROID__) && !defined(QUEST_VR)
    return pInputSystem->pInputManager->HandleInput((AInputEvent*)msg, pWindow->handle.activity);
#elif defined(__linux__) && !defined(GAINPUT_PLATFORM_GGP) && !defined(QUEST_VR)
    pInputSystem->pInputManager->HandleEvent(*(XEvent*)msg);
#endif
#endif

    return 0;
}

bool initInputSystem(InputSystemDesc* pDesc)
{
#ifdef ENABLE_FORGE_INPUT

    ASSERT(pDesc);
    ASSERT(pDesc->pWindow);

    pInputSystem = tf_new(InputSystemImpl);

    setCustomMessageProcessor(InputSystemHandleMessage);

    bool success = pInputSystem->Init(pDesc->pWindow);

#if TOUCH_INPUT
    if (pDesc->pJoystickTexture)
    {
        ASSERT(pDesc->pRenderer);
        VirtualJoystickDesc joystickDesc = {};
        joystickDesc.pRenderer = pDesc->pRenderer;
        joystickDesc.pJoystickTexture = pDesc->pJoystickTexture;
        initVirtualJoystick(&joystickDesc, &gVirtualJoystick);
    }
#endif

    addDefaultActionMappings();

    return success;
#else
    return false;
#endif
}

void exitInputSystem()
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);

#if TOUCH_INPUT
    exitVirtualJoystick(&gVirtualJoystick);
#endif

    setCustomMessageProcessor(nullptr);

    pInputSystem->Exit();
    tf_delete(pInputSystem);
    pInputSystem = NULL;
#endif
}

void updateInputSystem(float deltaTime, uint32_t width, uint32_t height)
{
    UNREF_PARAM(deltaTime);
    UNREF_PARAM(width);
    UNREF_PARAM(height);
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
#if !defined(AUTOMATED_TESTING)
    pInputSystem->Update(deltaTime, width, height);
#endif
#endif
}

void addInputAction(const InputActionDesc* pDesc, const InputActionMappingDeviceTarget actionMappingTarget)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
    pInputSystem->AddInputAction(pDesc, actionMappingTarget);
#endif
}

void removeInputAction(const InputActionDesc* pDesc, const InputActionMappingDeviceTarget actionMappingTarget)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
    pInputSystem->RemoveInputAction(pDesc, actionMappingTarget);
#endif
}

void setGlobalInputAction(const GlobalInputActionDesc* pDesc)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
    pInputSystem->SetGlobalInputAction(pDesc);
#endif
}

void addActionMappings(ActionMappingDesc* const actionMappings, const uint32_t numActions,
                       const InputActionMappingDeviceTarget actionMappingTarget)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
    return pInputSystem->AddActionMappings(actionMappings, numActions, actionMappingTarget);
#endif
}

void removeActionMappings(const InputActionMappingDeviceTarget actionMappingTarget)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
    return pInputSystem->RemoveActionMappings(actionMappingTarget);
#endif
}

void addDefaultActionMappings()
{
    ActionMappingDesc actionMappingsArr[] = {
        // Camera actions
        { INPUT_ACTION_MAPPING_COMPOSITE,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::TRANSLATE_CAMERA,
          { KeyboardButton::KEYBOARD_BUTTON_D, KeyboardButton::KEYBOARD_BUTTON_A, KeyboardButton::KEYBOARD_BUTTON_W,
            KeyboardButton::KEYBOARD_BUTTON_S } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::TRANSLATE_CAMERA,
          { GamepadButton::GAMEPAD_BUTTON_LEFT_STICK_X },
          2 },
        { INPUT_ACTION_MAPPING_COMPOSITE,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::TRANSLATE_CAMERA_VERTICAL,
          { KeyboardButton::KEYBOARD_BUTTON_E, KeyboardButton::KEYBOARD_BUTTON_Q },
          2,
          1,
          0,
          0,
          0,
          0.0f,
          AREA_LEFT,
          false,
          true },
        { INPUT_ACTION_MAPPING_COMPOSITE,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::TRANSLATE_CAMERA_VERTICAL,
          { GamepadButton::GAMEPAD_BUTTON_AXIS_5, GamepadButton::GAMEPAD_BUTTON_AXIS_4 },
          2,
          1,
          0,
          0,
          0,
          0.0f,
          AREA_LEFT,
          false,
          true },
        { INPUT_ACTION_MAPPING_TOUCH_VIRTUAL_JOYSTICK,
          INPUT_ACTION_MAPPING_TARGET_TOUCH,
          DefaultInputActions::TRANSLATE_CAMERA,
          {},
          1,
          1,
          0,
          20.f,
          200.f,
          1.f,
          AREA_LEFT },
        { INPUT_ACTION_MAPPING_COMPOSITE,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::ROTATE_CAMERA,
          { KeyboardButton::KEYBOARD_BUTTON_L, KeyboardButton::KEYBOARD_BUTTON_J, KeyboardButton::KEYBOARD_BUTTON_I,
            KeyboardButton::KEYBOARD_BUTTON_K } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::ROTATE_CAMERA,
          { GamepadButton::GAMEPAD_BUTTON_RIGHT_STICK_X },
          2 },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_MOUSE,
          DefaultInputActions::ROTATE_CAMERA,
          { MouseButton::MOUSE_BUTTON_AXIS_X },
          2,
          1,
          0,
          0,
          0,
          0.001f,
          AREA_LEFT,
          true },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_TOUCH,
          DefaultInputActions::ROTATE_CAMERA,
          { TouchButton::TOUCH_AXIS_X },
          2,
          1,
          0,
          20.f,
          200.f,
          0.2f,
          AREA_RIGHT },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_MOUSE,
          DefaultInputActions::CAPTURE_INPUT,
          { MouseButton::MOUSE_BUTTON_LEFT } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::RESET_CAMERA,
          { KeyboardButton::KEYBOARD_BUTTON_SPACE } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::RESET_CAMERA,
          { GamepadButton::GAMEPAD_BUTTON_Y } },

        // Profile data / toggle fullscreen / exit actions
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::DUMP_PROFILE_DATA,
          { KeyboardButton::KEYBOARD_BUTTON_F3 } },
        { INPUT_ACTION_MAPPING_COMBO,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::DUMP_PROFILE_DATA,
          { GamepadButton::GAMEPAD_BUTTON_START, GamepadButton::GAMEPAD_BUTTON_B } },
        { INPUT_ACTION_MAPPING_COMBO,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::TOGGLE_FULLSCREEN,
          { KeyboardButton::KEYBOARD_BUTTON_ALT_L, KeyboardButton::KEYBOARD_BUTTON_RETURN } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::EXIT,
          { KeyboardButton::KEYBOARD_BUTTON_ESCAPE } },
        { INPUT_ACTION_MAPPING_COMBO,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::RELOAD_SHADERS,
          { KeyboardButton::KEYBOARD_BUTTON_CTRL_L, KeyboardButton::KEYBOARD_BUTTON_S } },

        // UI specific actions
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_TAB,
          { KeyboardButton::KEYBOARD_BUTTON_TAB } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_LEFT_ARROW,
          { KeyboardButton::KEYBOARD_BUTTON_LEFT } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_RIGHT_ARROW,
          { KeyboardButton::KEYBOARD_BUTTON_RIGHT } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_UP_ARROW,
          { KeyboardButton::KEYBOARD_BUTTON_UP } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_DOWN_ARROW,
          { KeyboardButton::KEYBOARD_BUTTON_DOWN } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_PAGE_UP,
          { KeyboardButton::KEYBOARD_BUTTON_PAGE_UP } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_PAGE_DOWN,
          { KeyboardButton::KEYBOARD_BUTTON_PAGE_DOWN } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_HOME,
          { KeyboardButton::KEYBOARD_BUTTON_HOME } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_END,
          { KeyboardButton::KEYBOARD_BUTTON_END } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_INSERT,
          { KeyboardButton::KEYBOARD_BUTTON_INSERT } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_DELETE,
          { KeyboardButton::KEYBOARD_BUTTON_DELETE } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_BACK_SPACE,
          { KeyboardButton::KEYBOARD_BUTTON_BACK_SPACE } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_SPACE,
          { KeyboardButton::KEYBOARD_BUTTON_SPACE } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_ENTER,
          { KeyboardButton::KEYBOARD_BUTTON_RETURN } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_ESCAPE,
          { KeyboardButton::KEYBOARD_BUTTON_ESCAPE } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_CONTROL_L,
          { KeyboardButton::KEYBOARD_BUTTON_CTRL_L } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_CONTROL_R,
          { KeyboardButton::KEYBOARD_BUTTON_CTRL_R } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_SHIFT_L,
          { KeyboardButton::KEYBOARD_BUTTON_SHIFT_L } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_SHIFT_R,
          { KeyboardButton::KEYBOARD_BUTTON_SHIFT_R } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_ALT_L,
          { KeyboardButton::KEYBOARD_BUTTON_ALT_L } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_ALT_R,
          { KeyboardButton::KEYBOARD_BUTTON_ALT_R } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_SUPER_L,
          { KeyboardButton::KEYBOARD_BUTTON_SUPER_L } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_SUPER_R,
          { KeyboardButton::KEYBOARD_BUTTON_SUPER_R } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_A,
          { KeyboardButton::KEYBOARD_BUTTON_A } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_C,
          { KeyboardButton::KEYBOARD_BUTTON_C } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_V,
          { KeyboardButton::KEYBOARD_BUTTON_V } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_X,
          { KeyboardButton::KEYBOARD_BUTTON_X } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_Y,
          { KeyboardButton::KEYBOARD_BUTTON_Y } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_Z,
          { KeyboardButton::KEYBOARD_BUTTON_Z } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_KEY_F2,
          { KeyboardButton::KEYBOARD_BUTTON_F2 } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_MOUSE,
          DefaultInputActions::UI_MOUSE_LEFT,
          { MouseButton::MOUSE_BUTTON_LEFT } },
        { INPUT_ACTION_MAPPING_NORMAL, INPUT_ACTION_MAPPING_TARGET_TOUCH, DefaultInputActions::UI_MOUSE_LEFT },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_MOUSE,
          DefaultInputActions::UI_MOUSE_RIGHT,
          { MouseButton::MOUSE_BUTTON_RIGHT } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_MOUSE,
          DefaultInputActions::UI_MOUSE_MIDDLE,
          { MouseButton::MOUSE_BUTTON_MIDDLE } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_MOUSE,
          DefaultInputActions::UI_MOUSE_SCROLL_UP,
          { MouseButton::MOUSE_BUTTON_WHEEL_UP } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_MOUSE,
          DefaultInputActions::UI_MOUSE_SCROLL_DOWN,
          { MouseButton::MOUSE_BUTTON_WHEEL_DOWN } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_TOGGLE_UI,
          { GamepadButton::GAMEPAD_BUTTON_R3 } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          DefaultInputActions::UI_NAV_TOGGLE_UI,
          { KeyboardButton::KEYBOARD_BUTTON_F1 } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_ACTIVATE,
          { GamepadButton::GAMEPAD_BUTTON_A } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_CANCEL,
          { GamepadButton::GAMEPAD_BUTTON_B } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_INPUT,
          { GamepadButton::GAMEPAD_BUTTON_Y } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_MENU,
          { GamepadButton::GAMEPAD_BUTTON_X } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_TWEAK_WINDOW_LEFT,
          { GamepadButton::GAMEPAD_BUTTON_LEFT } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_TWEAK_WINDOW_RIGHT,
          { GamepadButton::GAMEPAD_BUTTON_RIGHT } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_TWEAK_WINDOW_UP,
          { GamepadButton::GAMEPAD_BUTTON_UP } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_TWEAK_WINDOW_DOWN,
          { GamepadButton::GAMEPAD_BUTTON_DOWN } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_SCROLL_MOVE_WINDOW,
          { GamepadButton::GAMEPAD_BUTTON_LEFT_STICK_X },
          2 },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_FOCUS_PREV,
          { GamepadButton::GAMEPAD_BUTTON_L1 } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_FOCUS_NEXT,
          { GamepadButton::GAMEPAD_BUTTON_R1 } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_TWEAK_SLOW,
          { GamepadButton::GAMEPAD_BUTTON_L2 } },
        { INPUT_ACTION_MAPPING_NORMAL,
          INPUT_ACTION_MAPPING_TARGET_CONTROLLER,
          DefaultInputActions::UI_NAV_TWEAK_FAST,
          { GamepadButton::GAMEPAD_BUTTON_R2 } }
    };

    addActionMappings(actionMappingsArr, TF_ARRAY_COUNT(actionMappingsArr), INPUT_ACTION_MAPPING_TARGET_ALL);
}

bool setEnableCaptureInput(bool enable)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);

    return pInputSystem->SetEnableCaptureInput(enable);
#else
    return false;
#endif
}

void setVirtualKeyboard(uint32_t type)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);

    pInputSystem->SetVirtualKeyboard(type);
#endif
}

void setDeadZone(unsigned gamePadIndex, float deadZoneSize)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);

    pInputSystem->SetDeadZone(gamePadIndex, deadZoneSize);
#endif
}

const char* getGamePadName(int gamePadIndex)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);

    return pInputSystem->GetGamePadName(gamePadIndex);
#else
    return nullptr;
#endif
}

bool gamePadConnected(int gamePadIndex)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);

    return pInputSystem->GamePadConnected(gamePadIndex);
#else
    return false;
#endif
}

bool setRumbleEffect(int gamePadIndex, float left_motor, float right_motor, uint32_t duration_ms)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
    // this is used only for mobile phones atm.
    // allows us to vibrate the actual phone instead of the connected gamepad.
    // if a gamepad is also connected, the app can decide which device should get the vibration
    bool vibrateDeviceInsteadOfPad = false;
    if (gamePadIndex == BUILTIN_DEVICE_HAPTICS)
    {
        vibrateDeviceInsteadOfPad = true;
        gamePadIndex = 0;
    }

    return pInputSystem->SetRumbleEffect(gamePadIndex, left_motor, right_motor, duration_ms, vibrateDeviceInsteadOfPad);
#else
    return false;
#endif
}

void setLEDColor(int gamePadIndex, uint8_t r, uint8_t g, uint8_t b)
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);

    pInputSystem->SetLEDColor(gamePadIndex, r, g, b);
#endif
}

void setOnDeviceChangeCallBack(void (*onDeviceChnageCallBack)(const char* name, bool added, int gamepadIndex))
{
#ifdef ENABLE_FORGE_INPUT
    ASSERT(pInputSystem);
    pInputSystem->setOnDeviceChangeCallBack(onDeviceChnageCallBack);
#endif
}

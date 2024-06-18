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

/********************************************************************************************************
 *
 * The Forge - ANIMATION UNIT TEST
 *
 * The purpose of this demo is to show how to Animations work using the
 * animnation middleware
 *
 *********************************************************************************************************/

// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

// Rendering
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include "../../../../Common_3/Utilities/RingBuffer.h"

// Middleware packages
#include "../../../../Common_3/Resources/AnimationSystem/Animation/AnimatedObject.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/Animation.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/Clip.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/ClipController.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/Rig.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/SkeletonBatcher.h"

// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"
#include "../../../../Common_3/Utilities/Threading/ThreadSystem.h"

// Memory
#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

//--------------------------------------------------------------------------------------------
// RENDERING PIPELINE DATA
//--------------------------------------------------------------------------------------------
#define MAX_INSTANCES 804 // For allocating space in uniform block. Must match with shader.

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

ProfileToken gGpuProfileToken;

uint32_t  gFrameIndex = 0;
Renderer* pRenderer = NULL;

Queue*     pGraphicsQueue = NULL;
GpuCmdRing gGraphicsCmdRing = {};

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Semaphore*    pImageAcquiredSemaphore = NULL;

Shader*   pCubeShader = NULL;
Buffer*   pJointVertexBuffer = NULL;
Buffer*   pBoneVertexBuffer = NULL;
Buffer*   pCuboidVertexBuffer = NULL;
Buffer*   pCubesVertexBuffer = NULL;
Pipeline* pCubePipeline = NULL;
int       gNumberOfJointPoints;
int       gNumberOfBonePoints;
int       gNumberOfCuboidPoints;

// Baked Physics
Pipeline* pOzzLogoSkeletonPipeline = NULL;
int       gNumberOfCubes;

Shader*        pPlaneDrawShader = NULL;
Buffer*        pPlaneVertexBuffer = NULL;
Pipeline*      pPlaneDrawPipeline = NULL;
RootSignature* pRootSignature = NULL;
DescriptorSet* pDescriptorSet = NULL;
DescriptorSet* pTargetDescriptorSet = NULL;

struct UniformBlockPlane
{
    CameraMatrix mProjectView;
    mat4         mToWorldMat;
};
UniformBlockPlane gUniformDataPlane;

Buffer* pPlaneUniformBuffer[gDataBufferCount] = { NULL };

struct UniformBlock
{
    CameraMatrix mProjectView;
    mat4         mViewMatrix;
    vec4         mColor[MAX_INSTANCES];
    vec4         mLightPosition;
    vec4         mLightColor;
    vec4         mJointColor;
    uint4        mSkeletonInfo;
    mat4         mToWorldMat[MAX_INSTANCES];
} gUniformDataCuboid;

Buffer*              pCuboidUniformBuffer[gDataBufferCount] = { NULL };
Buffer*              pTargetUniformBuffer[gDataBufferCount] = { NULL };
UniformSkeletonBlock gUniformDataTarget;
//--------------------------------------------------------------------------------------------
// CAMERA CONTROLLER & SYSTEMS (File/Log)
//--------------------------------------------------------------------------------------------
ICameraController*   pCameraController = NULL;

FontDrawDesc gFrameTimeDraw;
uint32_t     gFontID = 0;

const char* gTestScripts[] = { "Test0.lua" };
uint32_t    gScriptIndexes[] = { 0 };
uint32_t    gCurrentScriptIndex = 0;

void RunScript(void* pUserData)
{
    UNREF_PARAM(pUserData);
    LuaScriptDesc runDesc = {};
    runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
    luaQueueScriptToRun(&runDesc);
}

//--------------------------------------------------------------------------------------------
// ANIMATION DATA
//--------------------------------------------------------------------------------------------
#define ANIMATIONCOUNT       4

#define MAX_ANIMATED_OBJECTS 4096

unsigned int gNumRigs = 1; // Determines the number of rigs to update and draw

UIComponent* pStandaloneAnimationsGUIWindow = NULL;
// Specific UIComponents for each animation example
UIComponent* AnimationControlsGUIWindow[ANIMATIONCOUNT];

// AnimatedObjects
AnimatedObject gStickFigureAnimObject[MAX_ANIMATED_OBJECTS];
AnimatedObject gOzzLogoAnimObject;

// Animations
Animation gAnimations[ANIMATIONCOUNT][MAX_ANIMATED_OBJECTS];
Animation gShatterAnimation;

// ClipControllers
ClipController gStandClipController[MAX_ANIMATED_OBJECTS];
ClipController gWalkClipController[MAX_ANIMATED_OBJECTS];
ClipController gJogClipController[MAX_ANIMATED_OBJECTS];
ClipController gRunClipController[MAX_ANIMATED_OBJECTS];
ClipController gNeckCrackClipController[MAX_ANIMATED_OBJECTS];
ClipController gShatterClipContoller;

// Clips
Clip gStandClip;
Clip gWalkClip;
Clip gJogClip;
Clip gRunClip;
Clip gNeckCrackClip;
Clip gShatterClip;

// ClipMasks
ClipMask gStandClipMask;
ClipMask gWalkClipMask;
ClipMask gNeckCrackClipMask;

// Rigs
Rig gStickFigureRig;
Rig gOzzLogoRig;

// SkeletonBatcher
SkeletonBatcher gSkeletonBatcher;
SkeletonBatcher gOzzLogoSkeletonBatcher;

// parameters for aim IK
AimIKDesc      gAimIKDesc;
Point3         gAimTarget;
TwoBonesIKDesc gTwoBonesIKDesc;
int            gJointChain[4];
const Vector3  gJointUpVectors[4] = { Vector3::xAxis(), Vector3::xAxis(), Vector3::xAxis(), Vector3::xAxis() };

// Filenames
const char* gStickFigureName = "stickFigure/skeleton.ozz";
const char* gStandClipName = "stickFigure/animations/stand.ozz";
const char* gWalkClipName = "stickFigure/animations/walk.ozz";
const char* gJogClipName = "stickFigure/animations/jog.ozz";
const char* gRunClipName = "stickFigure/animations/run.ozz";
const char* gNeckCrackClipName = "stickFigure/animations/neckCrack.ozz";
const char* gOzzLogoName = "ozzLogo/skeleton.ozz";
const char* gShatterClipName = "ozzLogo/animations/shatter.ozz";
int         gCameraIndex;

float* pBonePoints = 0;
float* pJointPoints = 0;
float* pCuboidPoints = 0;
float* pCubesPoints;

const float gBoneWidthRatio = 0.2f;                // Determines how far along the bone to put the max width [0,1]
const float gJointRadius = gBoneWidthRatio * 0.5f; // set to replicate Ozz skeleton

// Timer to get animation system update time
static HiresTimer gAnimationUpdateTimer;
char              gAnimationUpdateText[64] = { 0 };

// Attached Cuboid Object
mat4       gCuboidTransformMat = mat4::identity(); // Will get updated as the animated object updates
const mat4 gCuboidScaleMat = mat4::scale(vec3(0.05f, 0.05f, 0.4f));
const vec4 gCuboidColor = vec4(1.f, 0.f, 0.f, 1.f);

//--------------------------------------------------------------------------------------------
// MULTI THREADING DATA
//--------------------------------------------------------------------------------------------

// Toggle for enabling/disabling threading through UI
bool gEnableThreading = true;
bool gAutomateThreading = true;

// Number of rigs per task that will be adjusted by the UI
unsigned int gGrainSize = 1;

struct ThreadData
{
    AnimatedObject* mAnimatedObject;
    float           mDeltaTime;
    unsigned int    mNumberSystems;
};
static ThreadData gThreadData[MAX_ANIMATED_OBJECTS];

struct ThreadSkeletonData
{
    unsigned int mFrameNumber;
    unsigned int mNumberRigs;
    uint32_t     mOffset;
};
static ThreadSkeletonData gThreadSkeletonData[MAX_ANIMATED_OBJECTS];

static ThreadSystem gThreadSystem = NULL;

//--------------------------------------------------------------------------------------------
// UI DATA
//--------------------------------------------------------------------------------------------

// used for JointAttachment example
const unsigned int kLeftHandMiddleJointIndex = 18; // index of the left hand's middle joint in this specific skeleton

// used for PartialBlending example
const float        kDefaultUpperBodyWeight = 1.0f;   // sets mStandJointsWeight and mWalkJointsWeight to their default values
const float        kDefaultStandJointsWeight = 1.0f; // stand clip will only effect children of UpperBodyJointIndex
const float        kDefaultWalkJointsWeight = 0.0f;  // walk clip will only effect non-children of UpperBodyJointIndex
const unsigned int kSpineJointIndex = 3;             // index of the spine joint in this specific skeleton

// used for AdditiveBlending example
const float kDefaultNeckCrackJointsWeight = 1.0f;

struct UIData
{
    struct GeneralSettingsData
    {
        bool mDrawBakedPhysics = true;
        bool mAnimatedCamera = false;
        bool mShowBindPose = false;
        bool mDrawAttachedObject = true;
        bool mDrawPlane = true;
        bool mMultipleRigs = false;

        unsigned int* mNumberOfRigs = &gNumRigs;
    } mGeneralSettings;

    struct BlendingBlendParamsData
    {
        bool  mAutoSetBlendParams = true;
        float mThreshold = 0.1f;
        float mBlendRatio = 0.5f;
        float mWalkClipWeight = 0.2f;
        float mJogClipWeight = 0.2f;
        float mRunClipWeight = 0.2f;
    } mBlendingParams;

    struct PartianlBlendingBlendParamsData
    {
        bool  mAutoSetBlendParams = true;
        float mThreshold = 0.1f;
        float mStandClipWeight = 0.2f;
        float mWalkClipWeight = 0.2f;
        float mUpperBodyWeight = kDefaultUpperBodyWeight;
        float mStandJointsWeight = kDefaultStandJointsWeight;
        float mWalkJointsWeight = kDefaultWalkJointsWeight;
    } mPartialBlendingParams;

    struct AdditiveBlendingParamsData
    {
        float mWalkClipWeight = 0.2f;
        float mNeckCrackClipWeight = 0.2f;
        float mThreshold = 0.1f;
    } mAdditiveBlendingParams;

    struct AttachedObjectData
    {
        unsigned int mJointIndex = kLeftHandMiddleJointIndex;
        float        mXOffset = -0.001f; // Values that will place it naturally in the hand
        float        mYOffset = 0.041f;
        float        mZOffset = -0.141f;
    } mAttachedObject;

    struct IKParamsData
    {
        bool  mAim = false;
        bool  mTwoBoneIK = false;
        float mFoot = 0.0f;
    } mIKParams;

    struct ThreadingControlData
    {
        bool*         mEnableThreading = &gEnableThreading;
        bool*         mAutomateThreading = &gAutomateThreading;
        unsigned int* mGrainSize = &gGrainSize;
    } mThreadingControl;

    struct ClipData
    {
        bool  mPlay = true;
        bool  mLoop = true;
        float mAnimationTime = 0.0f;
        float mPlaybackSpeed = 1.0f;
    };

    ClipData mStandClip = {};
    ClipData mWalkClip = {};
    ClipData mJogClip = {};
    ClipData mRunClip = {};
    ClipData mNeckCrackClip = {};
    ClipData mShatterClip = {};

    struct UpperBodyMaskData
    {
        bool         mEnableMask = true;
        float        mNeckCrackJointsWeight = kDefaultNeckCrackJointsWeight;
        unsigned int mUpperBodyJointIndex = kSpineJointIndex;
    } mUpperBodyMask;

    unsigned int mUpperBodyJointIndex = kSpineJointIndex;
} gUIData = {};

const char* gAnimationNames[] = { "PlayBack", "Blending", "PartialBlending", "AdditiveBlending" };

uint32_t gAnimationIndexes[] = { 0, 1, 2, 3 };
uint32_t gCurrentAnimationIndex = 0;

// Hard set the controller's time ratio via callback when it is set in the UI
void ShatterClipTimeChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if (gUIData.mGeneralSettings.mDrawBakedPhysics)
    {
        gShatterClipContoller.SetTimeRatioHard(0.0f);
        gShatterClipContoller.mPlay = true;
    }
    else
    {
        gShatterClipContoller.mPlay = false;
        gUIData.mGeneralSettings.mAnimatedCamera = false;
    }
}

void AnimatedCameraChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if (!gUIData.mGeneralSettings.mDrawBakedPhysics)
    {
        gUIData.mGeneralSettings.mAnimatedCamera = false;
    }
}

// StandClip Callbacks
void StandClipPlayCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gStandClipController[i].mPlay = gUIData.mStandClip.mPlay;
        }
    }
    else
    {
        gUIData.mStandClip.mPlay = true;
    }
}
void StandClipLoopCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gStandClipController[i].mLoop = gUIData.mStandClip.mLoop;
        }
    }
    else
    {
        gUIData.mStandClip.mLoop = true;
    }
}
void StandClipTimeChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        gUIData.mStandClip.mPlay = false;
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gStandClipController[i].SetTimeRatioHard(gUIData.mStandClip.mAnimationTime);
        }
    }
}
void StandClipPlaybackSpeedChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gStandClipController[i].mPlaybackSpeed = gUIData.mStandClip.mPlaybackSpeed;
        }
    }
    else
    {
        gUIData.mStandClip.mPlaybackSpeed = gStandClipController[0].mPlaybackSpeed;
    }
}
void SetStandClipJointsWeightWithUIValues(void* pUserData)
{
    UNREF_PARAM(pUserData);
    gStandClipMask.DisableAllJoints();
    gStandClipMask.SetAllChildrenOf(gUIData.mUpperBodyJointIndex, gUIData.mPartialBlendingParams.mStandJointsWeight);
}
void StandClipJointsWeightCallback(void* pUserData)
{
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        SetStandClipJointsWeightWithUIValues(pUserData);
    }
    else
    {
        gUIData.mPartialBlendingParams.mStandJointsWeight = gUIData.mPartialBlendingParams.mUpperBodyWeight;
    }
}
void StandClipWeightCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        if (gCurrentAnimationIndex == 2)
        {
            for (size_t i = 0; i < gNumRigs; i++)
            {
                gStandClipController[i].mWeight = gUIData.mPartialBlendingParams.mStandClipWeight;
            }
        }
    }
    else
    {
        gUIData.mPartialBlendingParams.mStandClipWeight = gStandClipController[0].mWeight;
    }
}

// WalkClip Callbacks
void WalkClipPlayCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gWalkClipController[i].mPlay = gUIData.mWalkClip.mPlay;
        }
    }
    else
    {
        gUIData.mWalkClip.mPlay = true;
    }
}
void WalkClipLoopCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gWalkClipController[i].mLoop = gUIData.mWalkClip.mLoop;
        }
    }
    else
    {
        gUIData.mWalkClip.mLoop = true;
    }
}
void WalkClipTimeChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        gUIData.mWalkClip.mPlay = false;
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gWalkClipController[i].SetTimeRatioHard(gUIData.mWalkClip.mAnimationTime);
        }
    }
}
void WalkClipPlaybackSpeedChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gWalkClipController[i].mPlaybackSpeed = gUIData.mWalkClip.mPlaybackSpeed;
        }
    }
    else
    {
        gUIData.mWalkClip.mPlaybackSpeed = gWalkClipController[0].mPlaybackSpeed;
    }
}
void SetWalkClipJointsWeightWithUIValues()
{
    gWalkClipMask.EnableAllJoints();
    gWalkClipMask.SetAllChildrenOf(gUIData.mUpperBodyJointIndex, gUIData.mPartialBlendingParams.mWalkJointsWeight);
}
void WalkClipJointsWeightCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        SetWalkClipJointsWeightWithUIValues();
    }
    else
    {
        gUIData.mPartialBlendingParams.mWalkJointsWeight = 1.0f - gUIData.mPartialBlendingParams.mUpperBodyWeight;
    }
}
void WalkClipWeightCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        if (gCurrentAnimationIndex == 1)
        {
            for (size_t i = 0; i < gNumRigs; i++)
            {
                gWalkClipController[i].mWeight = gUIData.mBlendingParams.mWalkClipWeight;
            }
        }

        if (gCurrentAnimationIndex == 2)
        {
            for (size_t i = 0; i < gNumRigs; i++)
            {
                gWalkClipController[i].mWeight = gUIData.mPartialBlendingParams.mWalkClipWeight;
            }
        }
    }
    else
    {
        if (gCurrentAnimationIndex == 1)
        {
            gUIData.mBlendingParams.mWalkClipWeight = gWalkClipController[0].mWeight;
        }
        if (gCurrentAnimationIndex == 2)
        {
            gUIData.mPartialBlendingParams.mWalkClipWeight = gWalkClipController[0].mWeight;
        }
    }
}

// JogClip Callbacks
void JogClipPlayCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gJogClipController[i].mPlay = gUIData.mJogClip.mPlay;
        }
    }
    else
    {
        gUIData.mJogClip.mPlay = true;
    }
}
void JogClipLoopCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gJogClipController[i].mLoop = gUIData.mJogClip.mLoop;
        }
    }
    else
    {
        gUIData.mJogClip.mLoop = true;
    }
}
void JogClipTimeChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        gUIData.mJogClip.mPlay = false;
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gJogClipController[i].SetTimeRatioHard(gUIData.mJogClip.mAnimationTime);
        }
    }
}
void JogClipPlaybackSpeedChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gJogClipController[i].mPlaybackSpeed = gUIData.mJogClip.mPlaybackSpeed;
        }
    }
    else
    {
        gUIData.mJogClip.mPlaybackSpeed = gJogClipController[0].mPlaybackSpeed;
    }
}
void JogClipJointsWeightCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        SetWalkClipJointsWeightWithUIValues();
    }
    else
    {
        gUIData.mPartialBlendingParams.mWalkJointsWeight = 1.0f - gUIData.mPartialBlendingParams.mUpperBodyWeight;
    }
}
void JogClipWeightCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        if (gCurrentAnimationIndex == 1)
        {
            for (size_t i = 0; i < gNumRigs; i++)
            {
                gJogClipController[i].mWeight = gUIData.mBlendingParams.mJogClipWeight;
            }
        }
    }
    else
    {
        if (gCurrentAnimationIndex == 1)
        {
            gUIData.mBlendingParams.mJogClipWeight = gJogClipController[0].mWeight;
        }
    }
}

// RunClip Callbacks
void RunClipPlayCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gRunClipController[i].mPlay = gUIData.mRunClip.mPlay;
        }
    }
    else
    {
        gUIData.mRunClip.mPlay = true;
    }
}
void RunClipLoopCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gRunClipController[i].mLoop = gUIData.mRunClip.mLoop;
        }
    }
    else
    {
        gUIData.mRunClip.mLoop = true;
    }
}
void RunClipTimeChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        gUIData.mRunClip.mPlay = false;
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gRunClipController[i].SetTimeRatioHard(gUIData.mRunClip.mAnimationTime);
        }
    }
}
void RunClipPlaybackSpeedChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gRunClipController[i].mPlaybackSpeed = gUIData.mRunClip.mPlaybackSpeed;
        }
    }
    else
    {
        gUIData.mRunClip.mPlaybackSpeed = gRunClipController[0].mPlaybackSpeed;
    }
}
void RunClipWeightCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        if (gCurrentAnimationIndex == 1)
        {
            for (size_t i = 0; i < gNumRigs; i++)
            {
                gRunClipController[i].mWeight = gUIData.mBlendingParams.mRunClipWeight;
            }
        }
    }
    else
    {
        if (gCurrentAnimationIndex == 1)
        {
            gUIData.mBlendingParams.mRunClipWeight = gRunClipController[0].mWeight;
        }
    }
}

// NeckClip Callbacks
void NeckClipPlayCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gNeckCrackClipController[i].mPlay = gUIData.mNeckCrackClip.mPlay;
        }
    }
    else
    {
        gUIData.mNeckCrackClip.mPlay = true;
    }
}
void NeckClipLoopCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gNeckCrackClipController[i].mLoop = gUIData.mNeckCrackClip.mLoop;
        }
    }
    else
    {
        gUIData.mNeckCrackClip.mLoop = true;
    }
}
void NeckCrackClipTimeChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    for (size_t i = 0; i < gNumRigs; i++)
    {
        gNeckCrackClipController[i].SetTimeRatioHard(gUIData.mNeckCrackClip.mAnimationTime);
    }
}
void NeckClipPlaybackSpeedChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if ((gCurrentAnimationIndex != 1 && gCurrentAnimationIndex != 2) ||
        (!gUIData.mBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 1) ||
        (!gUIData.mPartialBlendingParams.mAutoSetBlendParams && gCurrentAnimationIndex == 2))
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gNeckCrackClipController[i].mPlaybackSpeed = gUIData.mNeckCrackClip.mPlaybackSpeed;
        }
    }
    else
    {
        gUIData.mNeckCrackClip.mPlaybackSpeed = gNeckCrackClipController[0].mPlaybackSpeed;
    }
}
void SetNeckCrackClipJointsWeightWithUIValues()
{
    gNeckCrackClipMask.DisableAllJoints();
    gNeckCrackClipMask.SetAllChildrenOf(gUIData.mUpperBodyMask.mUpperBodyJointIndex, gUIData.mUpperBodyMask.mNeckCrackJointsWeight);
}
void NeckCrackClipJointsWeightCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if (gUIData.mUpperBodyMask.mEnableMask)
    {
        SetNeckCrackClipJointsWeightWithUIValues();
    }
}

// When the mask is enabled and disabled
void EnableMaskCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if (gUIData.mUpperBodyMask.mEnableMask)
    {
        SetNeckCrackClipJointsWeightWithUIValues();
    }
    else
    {
        gNeckCrackClipMask.EnableAllJoints();
    }
}

// When the upper body weight parameter is changed update the clip mask's joint weights
void UpperBodyWeightCallback(void* pUserData)
{
    if (gUIData.mPartialBlendingParams.mAutoSetBlendParams)
    {
        gUIData.mPartialBlendingParams.mStandJointsWeight = gUIData.mPartialBlendingParams.mUpperBodyWeight;
        gUIData.mPartialBlendingParams.mWalkJointsWeight = 1.0f - gUIData.mPartialBlendingParams.mUpperBodyWeight;

        SetStandClipJointsWeightWithUIValues(pUserData);
        SetWalkClipJointsWeightWithUIValues();
    }
}

// When the upper body root index is changed, update the clip mask's joint weights and update the clip mask's joint weights
void UpperBodyJointIndexCallback(void* pUserData)
{
    if (gUIData.mUpperBodyMask.mEnableMask)
    {
        SetNeckCrackClipJointsWeightWithUIValues();
    }

    SetStandClipJointsWeightWithUIValues(pUserData);
    SetWalkClipJointsWeightWithUIValues();
}

// When mAutoSetBlendParams is turned on we need to reset the clip controllers
void AutoSetBlendParamsCallback(void* pUserData)
{
    if (gUIData.mBlendingParams.mAutoSetBlendParams)
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            // Reset the internal values
            gWalkClipController[i].Reset();
            gWalkClipController[i].mLoop = true;
            gJogClipController[i].Reset();
            gJogClipController[i].mLoop = true;
            gRunClipController[i].Reset();
            gRunClipController[i].mLoop = true;

            gAnimations[1][i].mAutoSetBlendParams = true;
        }

        // Reset the UI values
        gUIData.mWalkClip.mPlay = true;
        gUIData.mJogClip.mPlay = true;
        gUIData.mRunClip.mPlay = true;
        gUIData.mWalkClip.mLoop = true;
        gUIData.mJogClip.mLoop = true;
        gUIData.mRunClip.mLoop = true;
    }
    else
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gAnimations[1][i].mAutoSetBlendParams = false;
        }
    }

    if (gUIData.mPartialBlendingParams.mAutoSetBlendParams)
    {
        gUIData.mPartialBlendingParams.mUpperBodyWeight = kDefaultUpperBodyWeight;
        gUIData.mPartialBlendingParams.mStandJointsWeight = kDefaultStandJointsWeight;
        gUIData.mPartialBlendingParams.mWalkJointsWeight = kDefaultWalkJointsWeight;

        SetStandClipJointsWeightWithUIValues(pUserData);
        SetWalkClipJointsWeightWithUIValues();
    }
    else
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gAnimations[2][i].mAutoSetBlendParams = false;
        }
    }
}

void ResetAnimations()
{
    for (size_t i = 0; i < gNumRigs; i++)
    {
        // Reset the internal values
        gStandClipController[i].Reset();
        gStandClipController[i].mLoop = true;

        gWalkClipController[i].Reset();
        gWalkClipController[i].mLoop = true;

        gJogClipController[i].Reset();
        gJogClipController[i].mLoop = true;

        gRunClipController[i].Reset();
        gRunClipController[i].mLoop = true;

        gNeckCrackClipController[i].Reset();
        gNeckCrackClipController[i].mLoop = true;
    }

    // Reset the UI values
    gUIData.mStandClip.mPlay = true;
    gUIData.mWalkClip.mPlay = true;
    gUIData.mJogClip.mPlay = true;
    gUIData.mRunClip.mPlay = true;
    gUIData.mNeckCrackClip.mPlay = true;

    gUIData.mStandClip.mLoop = true;
    gUIData.mWalkClip.mLoop = true;
    gUIData.mJogClip.mLoop = true;
    gUIData.mRunClip.mLoop = true;
    gUIData.mNeckCrackClip.mLoop = true;
}

void RunAnimation(void* pUserData)
{
    UNREF_PARAM(pUserData);
    // this resets all values to the defaults
    ResetAnimations();

    for (size_t i = 0; i < ANIMATIONCOUNT; i++)
    {
        uiSetComponentActive(AnimationControlsGUIWindow[i], false);
    }

    uiSetComponentActive(AnimationControlsGUIWindow[gCurrentAnimationIndex], true);

    for (size_t i = 0; i < gNumRigs; i++)
    {
        gStickFigureAnimObject[i].mAnimation = &gAnimations[gCurrentAnimationIndex][i];
    }
}

void RandomTimeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    for (size_t i = 0; i < gNumRigs; i++)
    {
        float randomTime = randomFloat(0.0f, 1.0f);
        gAnimations[gCurrentAnimationIndex][i].SetTimeRatio(randomTime);
    }
}

void ThresholdChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    if (gCurrentAnimationIndex == 1)
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gAnimations[1][i].mThreshold = gUIData.mBlendingParams.mThreshold;
        }
    }
    else if (gCurrentAnimationIndex == 2)
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gAnimations[2][i].mThreshold = gUIData.mPartialBlendingParams.mThreshold;
        }
    }
    else if (gCurrentAnimationIndex == 3)
    {
        for (size_t i = 0; i < gNumRigs; i++)
        {
            gAnimations[3][i].mThreshold = gUIData.mAdditiveBlendingParams.mThreshold;
        }
    }
}

void BlendRatioChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    for (size_t i = 0; i < gNumRigs; i++)
    {
        gAnimations[1][i].mBlendRatio = gUIData.mBlendingParams.mBlendRatio;
    }
}

void SetUpAnimationSpecificGuiWindows()
{
    unsigned uintValMin = 1;
    unsigned uintValMax = MAX_ANIMATED_OBJECTS;
    unsigned sliderStepSizeUint = 1;

    enum
    {
        GENERAL_PARAM_SLIDER_RIGCOUNT,
        GENERAL_PARAM_BUTTON_RANDOMIZE,
        GENERAL_PARAM_CHECKBOX_SHOWBINDPOSE,
        GENERAL_PARAM_CHECKBOX_DRAWATTACHEDOBJECT,
        GENERAL_PARAM_CHECKBOX_DRAWPLANE,
        GENERAL_PARAM_CHECKBOX_DRAWBAKEDPHYSICS,
        GENERAL_PARAM_CHECKBOX_ANIMATECAMERA,

        GENERAL_PARAM_COUNT
    };

    enum
    {
        THREADING_PARAM_CHECKBOX_ENABLETHREADING,
        THREADING_PARAM_CHECKBOX_AUTOMATICTHREADING,
        THREADING_PARAM_SLIDER_GRAINSIZE,

        THREADING_PARAM_COUNT
    };

    enum
    {
        IK_PARAM_CHECKBOX_AIMIK,
        IK_PARAM_CHECKBOX_TWOBONEIK,
        IK_PARAM_SLIDER_FOOTTWOBONE,

        IK_PARAM_COUNT
    };

    enum
    {
        BLEND_PARAM_CHECKBOX_AUTOBLEND,
        BLEND_PARAM_SLIDER_BLENDRATIO,
        BLEND_PARAM_SLIDER_WALKCLIPWEIGHT,
        BLEND_PARAM_SLIDER_JOGCLIPWEIGHT,
        BLEND_PARAM_SLIDER_RUNCLIPWEIGHT,
        BLEND_PARAM_SLIDER_THRESHOLD,

        BLEND_PARAM_COUNT
    };

    enum
    {
        PARIALBLEND_PARAM_CHECKBOX_AUTOBLEND,
        PARIALBLEND_PARAM_SLIDER_UPPERBODYWEIGHT,
        PARIALBLEND_PARAM_SLIDER_STANDCLIPWEIGHT,
        PARIALBLEND_PARAM_SLIDER_STANDJOINTWEIGHT,
        PARIALBLEND_PARAM_SLIDER_WALKCLIPWEIGHT,
        PARIALBLEND_PARAM_SLIDER_WALKJOINTWEIGHT,
        PARIALBLEND_PARAM_SLIDER_THRESHOLD,

        PARIALBLEND_PARAM_COUNT
    };

    enum
    {
        UPPERBODYROOT_PARAM_SLIDER_UPPERBODYJOINTINDEX,

        UPPERBODYROOT_PARAM_COUNT
    };

    enum
    {
        ADDITIVEBLEND_PARAM_SLIDER_WALKCLIPWEIGHT,
        ADDITIVEBLEND_PARAM_SLIDER_NECKCRACKCLIPKWEIGHT,

        ADDITIVEBLEND_PARAM_COUNT
    };

    enum
    {
        UPPERBODYMASK_PARAM_CHECKBOX_ENABLEMASK,
        UPPERBODYMASK_PARAM_SLIDER_NECKCRACKJOINTWEIGHT,
        UPPERBODYMASK_PARAM_SLIDER_UPPERBODYJOINTWEIGHT,

        UPPERBODYMASK_PARAM_COUNT
    };

    enum
    {
        CLIP_PARAM_CHECKBOX_PLAY,
        CLIP_PARAM_CHECKBOX_LOOP,
        CLIP_PARAM_SLIDER_ANIMATIONTIME,
        CLIP_PARAM_SLIDER_PLAYBACK,

        CLIP_PARAM_COUNT
    };

    static const uint32_t maxWidgetCount =
        max((uint32_t)GENERAL_PARAM_COUNT, max((uint32_t)IK_PARAM_COUNT, max((uint32_t)THREADING_PARAM_COUNT, (uint32_t)CLIP_PARAM_COUNT)));

    UIWidget  widgetBases[maxWidgetCount] = {};
    UIWidget* widgets[maxWidgetCount];
    for (uint32_t i = 0; i < maxWidgetCount; ++i)
        widgets[i] = &widgetBases[i];

    {
        // GENERAL SETTINGS
        //
        CollapsingHeaderWidget CollapsingGeneralSettingsWidgets;
        CollapsingGeneralSettingsWidgets.pGroupedWidgets = widgets;
        CollapsingGeneralSettingsWidgets.mWidgetsCount = GENERAL_PARAM_COUNT;

        // Random Button
        CheckboxWidget randomTime;
        strcpy(widgets[GENERAL_PARAM_BUTTON_RANDOMIZE]->mLabel, "Randomize Clips Time");
        widgets[GENERAL_PARAM_BUTTON_RANDOMIZE]->mType = WIDGET_TYPE_BUTTON;
        widgets[GENERAL_PARAM_BUTTON_RANDOMIZE]->pWidget = &randomTime;
        widgets[GENERAL_PARAM_BUTTON_RANDOMIZE]->pOnEdited = RandomTimeCallback;

        // NumRigs - Slider
        SliderUintWidget numRigs;
        numRigs.pData = gUIData.mGeneralSettings.mNumberOfRigs;
        numRigs.mMin = uintValMin;
        numRigs.mMax = uintValMax;
        numRigs.mStep = sliderStepSizeUint;
        strcpy(widgets[GENERAL_PARAM_SLIDER_RIGCOUNT]->mLabel, "Number of Rigs");
        widgets[GENERAL_PARAM_SLIDER_RIGCOUNT]->mType = WIDGET_TYPE_SLIDER_UINT;
        widgets[GENERAL_PARAM_SLIDER_RIGCOUNT]->pWidget = &numRigs;
        widgets[GENERAL_PARAM_SLIDER_RIGCOUNT]->pOnEdited = RunAnimation;

        // ShowBindPose - Checkbox
        CheckboxWidget showBindPose;
        showBindPose.pData = &gUIData.mGeneralSettings.mShowBindPose;
        widgets[GENERAL_PARAM_CHECKBOX_SHOWBINDPOSE]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[GENERAL_PARAM_CHECKBOX_SHOWBINDPOSE]->pWidget = &showBindPose;
        widgets[GENERAL_PARAM_CHECKBOX_SHOWBINDPOSE]->pOnEdited = NULL;
        strcpy(widgets[GENERAL_PARAM_CHECKBOX_SHOWBINDPOSE]->mLabel, "Show Bind Pose");

        // DrawAttachedObject - Checkbox
        CheckboxWidget drawAttachedObject;
        drawAttachedObject.pData = &gUIData.mGeneralSettings.mDrawAttachedObject;
        widgets[GENERAL_PARAM_CHECKBOX_DRAWATTACHEDOBJECT]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[GENERAL_PARAM_CHECKBOX_DRAWATTACHEDOBJECT]->pWidget = &drawAttachedObject;
        widgets[GENERAL_PARAM_CHECKBOX_DRAWATTACHEDOBJECT]->pOnEdited = NULL;
        strcpy(widgets[GENERAL_PARAM_CHECKBOX_DRAWATTACHEDOBJECT]->mLabel, "Draw Attached Object");

        // DrawPlane - Checkbox
        CheckboxWidget drawPlane;
        drawPlane.pData = &gUIData.mGeneralSettings.mDrawPlane;
        widgets[GENERAL_PARAM_CHECKBOX_DRAWPLANE]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[GENERAL_PARAM_CHECKBOX_DRAWPLANE]->pWidget = &drawPlane;
        widgets[GENERAL_PARAM_CHECKBOX_DRAWPLANE]->pOnEdited = NULL;
        strcpy(widgets[GENERAL_PARAM_CHECKBOX_DRAWPLANE]->mLabel, "Draw Plane");

        // DrawBakedPhysics - Checkbox
        CheckboxWidget drawBakedPhysics;
        drawBakedPhysics.pData = &gUIData.mGeneralSettings.mDrawBakedPhysics;
        widgets[GENERAL_PARAM_CHECKBOX_DRAWBAKEDPHYSICS]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[GENERAL_PARAM_CHECKBOX_DRAWBAKEDPHYSICS]->pWidget = &drawBakedPhysics;
        widgets[GENERAL_PARAM_CHECKBOX_DRAWBAKEDPHYSICS]->pOnEdited = ShatterClipTimeChangeCallback;
        strcpy(widgets[GENERAL_PARAM_CHECKBOX_DRAWBAKEDPHYSICS]->mLabel, "Draw Baked Physics");

        // AnimatedCamera for baked Physics - Checkbox
        CheckboxWidget animatedCamera;
        animatedCamera.pData = &gUIData.mGeneralSettings.mAnimatedCamera;
        widgets[GENERAL_PARAM_CHECKBOX_ANIMATECAMERA]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[GENERAL_PARAM_CHECKBOX_ANIMATECAMERA]->pWidget = &animatedCamera;
        widgets[GENERAL_PARAM_CHECKBOX_ANIMATECAMERA]->pOnEdited = AnimatedCameraChangeCallback;
        strcpy(widgets[GENERAL_PARAM_CHECKBOX_ANIMATECAMERA]->mLabel, "Animate Camera");

        luaRegisterWidget(uiCreateComponentWidget(pStandaloneAnimationsGUIWindow, "General Settings", &CollapsingGeneralSettingsWidgets,
                                                  WIDGET_TYPE_COLLAPSING_HEADER));

        // THREADING CONTROL
        //
        CollapsingHeaderWidget CollapsingThreadingControlWidgets;
        CollapsingThreadingControlWidgets.pGroupedWidgets = widgets;
        CollapsingThreadingControlWidgets.mWidgetsCount = THREADING_PARAM_COUNT;

        // EnableThreading - Checkbox
        CheckboxWidget enableThreading;
        enableThreading.pData = gUIData.mThreadingControl.mEnableThreading;
        widgets[THREADING_PARAM_CHECKBOX_ENABLETHREADING]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[THREADING_PARAM_CHECKBOX_ENABLETHREADING]->pWidget = &enableThreading;
        widgets[THREADING_PARAM_CHECKBOX_ENABLETHREADING]->pOnEdited = NULL;
        strcpy(widgets[THREADING_PARAM_CHECKBOX_ENABLETHREADING]->mLabel, "Enable Threading");

        // AutomateThreading - Checkbox
        CheckboxWidget automaticThreading;
        automaticThreading.pData = gUIData.mThreadingControl.mAutomateThreading;
        widgets[THREADING_PARAM_CHECKBOX_AUTOMATICTHREADING]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[THREADING_PARAM_CHECKBOX_AUTOMATICTHREADING]->pWidget = &automaticThreading;
        strcpy(widgets[THREADING_PARAM_CHECKBOX_AUTOMATICTHREADING]->mLabel, "Automate Threading");
        widgets[THREADING_PARAM_CHECKBOX_AUTOMATICTHREADING]->pOnEdited = NULL;

        // GrainSize - Slider
        SliderUintWidget grainSize;
        grainSize.pData = gUIData.mThreadingControl.mGrainSize;
        grainSize.mMin = uintValMin;
        grainSize.mMax = uintValMax;
        grainSize.mStep = sliderStepSizeUint;
        strcpy(widgets[THREADING_PARAM_SLIDER_GRAINSIZE]->mLabel, "Grain Size");
        widgets[THREADING_PARAM_SLIDER_GRAINSIZE]->mType = WIDGET_TYPE_SLIDER_UINT;
        widgets[THREADING_PARAM_SLIDER_GRAINSIZE]->pWidget = &grainSize;
        widgets[THREADING_PARAM_SLIDER_GRAINSIZE]->pOnEdited = NULL;

        luaRegisterWidget(uiCreateComponentWidget(pStandaloneAnimationsGUIWindow, "Threading Control", &CollapsingThreadingControlWidgets,
                                                  WIDGET_TYPE_COLLAPSING_HEADER));

        // INVERSE KINEMATICS
        //
        CollapsingHeaderWidget CollapsingIKWidgets;
        CollapsingIKWidgets.pGroupedWidgets = widgets;
        CollapsingIKWidgets.mWidgetsCount = IK_PARAM_COUNT;

        CheckboxWidget aimIK;
        aimIK.pData = &gUIData.mIKParams.mAim;
        widgets[IK_PARAM_CHECKBOX_AIMIK]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[IK_PARAM_CHECKBOX_AIMIK]->pWidget = &aimIK;
        widgets[IK_PARAM_CHECKBOX_AIMIK]->pOnEdited = NULL;
        strcpy(widgets[IK_PARAM_CHECKBOX_AIMIK]->mLabel, "Aim IK");

        CheckboxWidget twoBoneIK;
        twoBoneIK.pData = &gUIData.mIKParams.mTwoBoneIK;
        widgets[IK_PARAM_CHECKBOX_TWOBONEIK]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[IK_PARAM_CHECKBOX_TWOBONEIK]->pWidget = &twoBoneIK;
        widgets[IK_PARAM_CHECKBOX_TWOBONEIK]->pOnEdited = NULL;
        strcpy(widgets[IK_PARAM_CHECKBOX_TWOBONEIK]->mLabel, "Two Bone IK");

        SliderFloatWidget footTwoBoneIK;
        footTwoBoneIK.pData = &gUIData.mIKParams.mFoot;
        footTwoBoneIK.mMin = 0.0f;
        footTwoBoneIK.mMax = 0.5f;
        footTwoBoneIK.mStep = 0.01f;
        strcpy(widgets[IK_PARAM_SLIDER_FOOTTWOBONE]->mLabel, "Foot two bone IK");
        widgets[IK_PARAM_SLIDER_FOOTTWOBONE]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[IK_PARAM_SLIDER_FOOTTWOBONE]->pWidget = &footTwoBoneIK;
        widgets[IK_PARAM_SLIDER_FOOTTWOBONE]->pOnEdited = NULL;

        luaRegisterWidget(uiCreateComponentWidget(pStandaloneAnimationsGUIWindow, "Inverse Kinematics", &CollapsingIKWidgets,
                                                  WIDGET_TYPE_COLLAPSING_HEADER));
    }

    // SET UP GUI FOR PLAYBACK EXAMPLE
    //
    {
        // STAND CLIP
        //
        CollapsingHeaderWidget CollapsingStandClipWidgets;
        CollapsingStandClipWidgets.pGroupedWidgets = widgets;
        CollapsingStandClipWidgets.mWidgetsCount = CLIP_PARAM_COUNT;

        // Play/Pause - Checkbox
        CheckboxWidget playStand;
        playStand.pData = &gUIData.mStandClip.mPlay;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pWidget = &playStand;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_PLAY]->mLabel, "Play");
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pOnEdited = &StandClipPlayCallback;

        // Loop - Checkbox
        CheckboxWidget loopStand;
        loopStand.pData = &gUIData.mStandClip.mLoop;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pWidget = &loopStand;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_LOOP]->mLabel, "Loop");
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pOnEdited = &StandClipLoopCallback;

        // Animation Time - Slider
        SliderFloatWidget animationTime;
        animationTime.pData = &gUIData.mStandClip.mAnimationTime;
        animationTime.mMin = 0.0f;
        animationTime.mMax = gStandClipController[0].mDuration;
        animationTime.mStep = 0.01f;
        strcpy(widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mLabel, "Animation Time");
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pWidget = &animationTime;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pOnEdited = &StandClipTimeChangeCallback;

        // Playback Speed - Slider
        SliderFloatWidget playbackTime;
        playbackTime.pData = &gUIData.mStandClip.mPlaybackSpeed;
        playbackTime.mMin = -5.0f;
        playbackTime.mMax = 5.0f;
        playbackTime.mStep = 0.1f;
        strcpy(widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mLabel, "Playback Speed");
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pWidget = &playbackTime;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pOnEdited = &StandClipPlaybackSpeedChangeCallback;

        // Add all widgets to the window
        luaRegisterWidget(uiCreateComponentWidget(AnimationControlsGUIWindow[0], "Stand Clip", &CollapsingStandClipWidgets,
                                                  WIDGET_TYPE_COLLAPSING_HEADER));
    }

    // SET UP GUI FOR BLENDING EXAMPLE
    //
    {
        // BLEND PARAMETERS
        //
        CollapsingHeaderWidget CollapsingBlendParamsWidgets;
        CollapsingBlendParamsWidgets.pGroupedWidgets = widgets;
        CollapsingBlendParamsWidgets.mWidgetsCount = BLEND_PARAM_COUNT;

        // AutoSetBlendParams - Checkbox
        CheckboxWidget autoBlend;
        autoBlend.pData = &gUIData.mBlendingParams.mAutoSetBlendParams;
        widgets[BLEND_PARAM_CHECKBOX_AUTOBLEND]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[BLEND_PARAM_CHECKBOX_AUTOBLEND]->pWidget = &autoBlend;
        strcpy(widgets[BLEND_PARAM_CHECKBOX_AUTOBLEND]->mLabel, "Auto Set Blend Params");
        widgets[BLEND_PARAM_CHECKBOX_AUTOBLEND]->pOnEdited = &AutoSetBlendParamsCallback;

        // Blend Ration - Slider
        SliderFloatWidget blendRatio;
        blendRatio.pData = &gUIData.mBlendingParams.mBlendRatio;
        blendRatio.mMin = 0.0f;
        blendRatio.mMax = 1.0f;
        blendRatio.mStep = 0.01f;
        strcpy(widgets[BLEND_PARAM_SLIDER_BLENDRATIO]->mLabel, "Blend Ratio");
        widgets[BLEND_PARAM_SLIDER_BLENDRATIO]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[BLEND_PARAM_SLIDER_BLENDRATIO]->pWidget = &blendRatio;
        widgets[BLEND_PARAM_SLIDER_BLENDRATIO]->pOnEdited = &BlendRatioChangeCallback;

        // Walk Clip Weight - Slider
        SliderFloatWidget walkClipWeight;
        walkClipWeight.pData = &gUIData.mBlendingParams.mWalkClipWeight;
        walkClipWeight.mMin = 0.0f;
        walkClipWeight.mMax = 1.0f;
        walkClipWeight.mStep = 0.01f;
        strcpy(widgets[BLEND_PARAM_SLIDER_WALKCLIPWEIGHT]->mLabel, "Clip Weight [Walk]");
        widgets[BLEND_PARAM_SLIDER_WALKCLIPWEIGHT]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[BLEND_PARAM_SLIDER_WALKCLIPWEIGHT]->pWidget = &walkClipWeight;
        widgets[BLEND_PARAM_SLIDER_WALKCLIPWEIGHT]->pOnEdited = &WalkClipWeightCallback;

        // Jog Clip Weight - Slider
        SliderFloatWidget jogClipWeight;
        jogClipWeight.pData = &gUIData.mBlendingParams.mJogClipWeight;
        jogClipWeight.mMin = 0.0f;
        jogClipWeight.mMax = 1.0f;
        jogClipWeight.mStep = 0.01f;
        strcpy(widgets[BLEND_PARAM_SLIDER_JOGCLIPWEIGHT]->mLabel, "Clip Weight [Jog]");
        widgets[BLEND_PARAM_SLIDER_JOGCLIPWEIGHT]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[BLEND_PARAM_SLIDER_JOGCLIPWEIGHT]->pWidget = &jogClipWeight;
        widgets[BLEND_PARAM_SLIDER_JOGCLIPWEIGHT]->pOnEdited = &JogClipWeightCallback;

        // Run Clip Weight - Slider
        SliderFloatWidget runClipWeight;
        runClipWeight.pData = &gUIData.mBlendingParams.mRunClipWeight;
        runClipWeight.mMin = 0.0f;
        runClipWeight.mMax = 1.0f;
        runClipWeight.mStep = 0.01f;
        strcpy(widgets[BLEND_PARAM_SLIDER_RUNCLIPWEIGHT]->mLabel, "Clip Weight [Run]");
        widgets[BLEND_PARAM_SLIDER_RUNCLIPWEIGHT]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[BLEND_PARAM_SLIDER_RUNCLIPWEIGHT]->pWidget = &runClipWeight;
        widgets[BLEND_PARAM_SLIDER_RUNCLIPWEIGHT]->pOnEdited = &RunClipWeightCallback;

        // Threshold - Slider
        SliderFloatWidget threshold;
        threshold.pData = &gUIData.mBlendingParams.mThreshold;
        threshold.mMin = 0.01f;
        threshold.mMax = 1.0f;
        threshold.mStep = 0.01f;
        strcpy(widgets[BLEND_PARAM_SLIDER_THRESHOLD]->mLabel, "Threshold");
        widgets[BLEND_PARAM_SLIDER_THRESHOLD]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[BLEND_PARAM_SLIDER_THRESHOLD]->pWidget = &threshold;
        widgets[BLEND_PARAM_SLIDER_THRESHOLD]->pOnEdited = &ThresholdChangeCallback;

        luaRegisterWidget(uiCreateComponentWidget(AnimationControlsGUIWindow[1], "Blend Parameters", &CollapsingBlendParamsWidgets,
                                                  WIDGET_TYPE_COLLAPSING_HEADER));

        // WALK CLIP
        //
        CollapsingHeaderWidget CollapsingWalkClipWidgets;
        CollapsingWalkClipWidgets.pGroupedWidgets = widgets;
        CollapsingWalkClipWidgets.mWidgetsCount = CLIP_PARAM_COUNT;

        // Play/Pause - Checkbox
        CheckboxWidget playWalk;
        playWalk.pData = &gUIData.mWalkClip.mPlay;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pWidget = &playWalk;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_PLAY]->mLabel, "Play");
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pOnEdited = &WalkClipPlayCallback;

        // Loop - Checkbox
        CheckboxWidget loopWalk;
        loopWalk.pData = &gUIData.mWalkClip.mLoop;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pWidget = &loopWalk;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_LOOP]->mLabel, "Loop");
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pOnEdited = &WalkClipLoopCallback;

        // Animation Time - Slider
        SliderFloatWidget animationTimeWalk;
        animationTimeWalk.pData = &gUIData.mWalkClip.mAnimationTime;
        animationTimeWalk.mMin = 0.01f;
        animationTimeWalk.mMax = gWalkClipController[0].mDuration;
        animationTimeWalk.mStep = 0.01f;
        strcpy(widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mLabel, "Animation Time");
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pWidget = &animationTimeWalk;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pOnEdited = &WalkClipTimeChangeCallback;

        // Playback Speed - Slider
        SliderFloatWidget playbackTimeWalk;
        playbackTimeWalk.pData = &gUIData.mWalkClip.mPlaybackSpeed;
        playbackTimeWalk.mMin = -5.0f;
        playbackTimeWalk.mMax = 5.0f;
        playbackTimeWalk.mStep = 0.1f;
        strcpy(widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mLabel, "Playback Speed");
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pWidget = &playbackTimeWalk;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pOnEdited = &WalkClipPlaybackSpeedChangeCallback;

        luaRegisterWidget(
            uiCreateComponentWidget(AnimationControlsGUIWindow[1], "Walk Clip", &CollapsingWalkClipWidgets, WIDGET_TYPE_COLLAPSING_HEADER));

        // JOG CLIP
        //
        CollapsingHeaderWidget CollapsingJogClipWidgets;
        CollapsingJogClipWidgets.pGroupedWidgets = widgets;
        CollapsingJogClipWidgets.mWidgetsCount = CLIP_PARAM_COUNT;

        // Play/Pause - Checkbox
        CheckboxWidget playJog;
        playJog.pData = &gUIData.mJogClip.mPlay;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pWidget = &playJog;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_PLAY]->mLabel, "Play");
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pOnEdited = &JogClipPlayCallback;

        // Loop - Checkbox
        CheckboxWidget loopJog;
        loopJog.pData = &gUIData.mJogClip.mLoop;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pWidget = &loopJog;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_LOOP]->mLabel, "Loop");
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pOnEdited = &JogClipLoopCallback;

        // Animation Time - Slider
        SliderFloatWidget animationTimeJog;
        animationTimeJog.pData = &gUIData.mJogClip.mAnimationTime;
        animationTimeJog.mMin = 0.01f;
        animationTimeJog.mMax = gJogClipController[0].mDuration;
        animationTimeJog.mStep = 0.01f;
        strcpy(widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mLabel, "Animation Time");
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pWidget = &animationTimeJog;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pOnEdited = &JogClipTimeChangeCallback;

        // Playback Speed - Slider
        SliderFloatWidget playbackTimeJog;
        playbackTimeJog.pData = &gUIData.mJogClip.mPlaybackSpeed;
        playbackTimeJog.mMin = -5.0f;
        playbackTimeJog.mMax = 5.0f;
        playbackTimeJog.mStep = 0.1f;
        strcpy(widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mLabel, "Playback Speed");
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pWidget = &playbackTimeJog;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pOnEdited = &JogClipPlaybackSpeedChangeCallback;

        luaRegisterWidget(
            uiCreateComponentWidget(AnimationControlsGUIWindow[1], "Jog Clip", &CollapsingJogClipWidgets, WIDGET_TYPE_COLLAPSING_HEADER));

        // RUN CLIP
        //
        CollapsingHeaderWidget CollapsingRunClipWidgets;
        CollapsingRunClipWidgets.pGroupedWidgets = widgets;
        CollapsingRunClipWidgets.mWidgetsCount = CLIP_PARAM_COUNT;

        // Play/Pause - Checkbox
        CheckboxWidget playRun;
        playRun.pData = &gUIData.mRunClip.mPlay;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pWidget = &playRun;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_PLAY]->mLabel, "Play");
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pOnEdited = &RunClipPlayCallback;

        // Loop - Checkbox
        CheckboxWidget loopRun;
        loopRun.pData = &gUIData.mRunClip.mLoop;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pWidget = &loopRun;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_LOOP]->mLabel, "Loop");
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pOnEdited = &RunClipLoopCallback;

        // Animation Time - Slider
        SliderFloatWidget animationTimeRun;
        animationTimeRun.pData = &gUIData.mRunClip.mAnimationTime;
        animationTimeRun.mMin = 0.01f;
        animationTimeRun.mMax = gRunClipController[0].mDuration;
        animationTimeRun.mStep = 0.01f;
        strcpy(widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mLabel, "Animation Time");
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pWidget = &animationTimeRun;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pOnEdited = &RunClipTimeChangeCallback;

        // Playback Speed - Slider
        SliderFloatWidget playbackTimeRun;
        playbackTimeRun.pData = &gUIData.mRunClip.mPlaybackSpeed;
        playbackTimeRun.mMin = -5.0f;
        playbackTimeRun.mMax = 5.0f;
        playbackTimeRun.mStep = 0.1f;
        strcpy(widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mLabel, "Playback Speed");
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pWidget = &playbackTimeRun;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pOnEdited = &RunClipPlaybackSpeedChangeCallback;

        luaRegisterWidget(
            uiCreateComponentWidget(AnimationControlsGUIWindow[1], "Run Clip", &CollapsingRunClipWidgets, WIDGET_TYPE_COLLAPSING_HEADER));
    }

    // SET UP GUI FOR PartialBlending EXAMPLE
    //
    {
        // BLEND PARAMETERS
        //
        CollapsingHeaderWidget CollapsingBlendParamsWidgets;
        CollapsingBlendParamsWidgets.pGroupedWidgets = widgets;
        CollapsingBlendParamsWidgets.mWidgetsCount = CLIP_PARAM_COUNT;

        // AutoSetBlendParams - Checkbox
        CheckboxWidget autoBlend;
        autoBlend.pData = &gUIData.mPartialBlendingParams.mAutoSetBlendParams;
        widgets[PARIALBLEND_PARAM_CHECKBOX_AUTOBLEND]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[PARIALBLEND_PARAM_CHECKBOX_AUTOBLEND]->pWidget = &autoBlend;
        strcpy(widgets[PARIALBLEND_PARAM_CHECKBOX_AUTOBLEND]->mLabel, "Auto Set Blend Params");
        widgets[PARIALBLEND_PARAM_CHECKBOX_AUTOBLEND]->pOnEdited = &AutoSetBlendParamsCallback;

        // UpperBodyWeight - Slider
        SliderFloatWidget upperBodyWeight;
        upperBodyWeight.pData = &gUIData.mPartialBlendingParams.mUpperBodyWeight;
        upperBodyWeight.mMin = 0.0f;
        upperBodyWeight.mMax = 1.0f;
        upperBodyWeight.mStep = 0.01f;
        strcpy(widgets[PARIALBLEND_PARAM_SLIDER_UPPERBODYWEIGHT]->mLabel, "Upper Body Weight");
        widgets[PARIALBLEND_PARAM_SLIDER_UPPERBODYWEIGHT]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[PARIALBLEND_PARAM_SLIDER_UPPERBODYWEIGHT]->pWidget = &upperBodyWeight;
        widgets[PARIALBLEND_PARAM_SLIDER_UPPERBODYWEIGHT]->pOnEdited = NULL;

        // Stand Clip Weight - Slider
        SliderFloatWidget standClipWeight;
        standClipWeight.pData = &gUIData.mPartialBlendingParams.mStandClipWeight;
        standClipWeight.mMin = 0.0f;
        standClipWeight.mMax = 1.0f;
        standClipWeight.mStep = 0.01f;
        strcpy(widgets[PARIALBLEND_PARAM_SLIDER_STANDCLIPWEIGHT]->mLabel, "Clip Weight [Stand]");
        widgets[PARIALBLEND_PARAM_SLIDER_STANDCLIPWEIGHT]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[PARIALBLEND_PARAM_SLIDER_STANDCLIPWEIGHT]->pWidget = &standClipWeight;
        widgets[PARIALBLEND_PARAM_SLIDER_STANDCLIPWEIGHT]->pOnEdited = &StandClipWeightCallback;

        // Stand Joints Weight - Slider
        SliderFloatWidget standJointsWeight;
        standJointsWeight.pData = &gUIData.mPartialBlendingParams.mStandJointsWeight;
        standJointsWeight.mMin = 0.0f;
        standJointsWeight.mMax = 1.0f;
        standJointsWeight.mStep = 0.01f;
        strcpy(widgets[PARIALBLEND_PARAM_SLIDER_STANDJOINTWEIGHT]->mLabel, "Upper Body Weight");
        widgets[PARIALBLEND_PARAM_SLIDER_STANDJOINTWEIGHT]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[PARIALBLEND_PARAM_SLIDER_STANDJOINTWEIGHT]->pWidget = &standJointsWeight;
        widgets[PARIALBLEND_PARAM_SLIDER_STANDJOINTWEIGHT]->pOnEdited = &StandClipJointsWeightCallback;

        // Walk Clip Weight - Slider
        SliderFloatWidget walkClipWeight;
        walkClipWeight.pData = &gUIData.mPartialBlendingParams.mWalkClipWeight;
        walkClipWeight.mMin = 0.0f;
        walkClipWeight.mMax = 1.0f;
        walkClipWeight.mStep = 0.01f;
        strcpy(widgets[PARIALBLEND_PARAM_SLIDER_WALKCLIPWEIGHT]->mLabel, "Clip Weight [Walk]");
        widgets[PARIALBLEND_PARAM_SLIDER_WALKCLIPWEIGHT]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[PARIALBLEND_PARAM_SLIDER_WALKCLIPWEIGHT]->pWidget = &walkClipWeight;
        widgets[PARIALBLEND_PARAM_SLIDER_WALKCLIPWEIGHT]->pOnEdited = &WalkClipWeightCallback;

        // Walk Joints Weight - Slider
        SliderFloatWidget walkJointsWeight;
        walkJointsWeight.pData = &gUIData.mPartialBlendingParams.mWalkJointsWeight;
        walkJointsWeight.mMin = 0.0f;
        walkJointsWeight.mMax = 1.0f;
        walkJointsWeight.mStep = 0.01f;
        strcpy(widgets[PARIALBLEND_PARAM_SLIDER_WALKJOINTWEIGHT]->mLabel, "Upper Body Weight");
        widgets[PARIALBLEND_PARAM_SLIDER_WALKJOINTWEIGHT]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[PARIALBLEND_PARAM_SLIDER_WALKJOINTWEIGHT]->pWidget = &walkJointsWeight;
        widgets[PARIALBLEND_PARAM_SLIDER_WALKJOINTWEIGHT]->pOnEdited = &WalkClipJointsWeightCallback;

        // Threshold - Slider
        SliderFloatWidget threshold;
        threshold.pData = &gUIData.mPartialBlendingParams.mThreshold;
        threshold.mMin = 0.01f;
        threshold.mMax = 1.0f;
        threshold.mStep = 0.01f;
        strcpy(widgets[PARIALBLEND_PARAM_SLIDER_THRESHOLD]->mLabel, "Threshold");
        widgets[PARIALBLEND_PARAM_SLIDER_THRESHOLD]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[PARIALBLEND_PARAM_SLIDER_THRESHOLD]->pWidget = &threshold;
        widgets[PARIALBLEND_PARAM_SLIDER_THRESHOLD]->pOnEdited = &ThresholdChangeCallback;

        luaRegisterWidget(uiCreateComponentWidget(AnimationControlsGUIWindow[2], "Blend Parameters", &CollapsingBlendParamsWidgets,
                                                  WIDGET_TYPE_COLLAPSING_HEADER));

        // UPPER BODY ROOT
        //
        CollapsingHeaderWidget CollapsingUpperBodyRootWidgets;
        CollapsingUpperBodyRootWidgets.pGroupedWidgets = widgets;
        CollapsingUpperBodyRootWidgets.mWidgetsCount = UPPERBODYROOT_PARAM_COUNT;

        // UpperBodyJointIndex - Slider
        SliderUintWidget upperBodyJointIndex;
        upperBodyJointIndex.pData = &gUIData.mUpperBodyJointIndex;
        upperBodyJointIndex.mMin = 0;
        upperBodyJointIndex.mMax = gStickFigureRig.mNumJoints - 1;
        upperBodyJointIndex.mStep = 1;
        strcpy(widgets[UPPERBODYROOT_PARAM_SLIDER_UPPERBODYJOINTINDEX]->mLabel, "UpperBody Joint Index");
        widgets[UPPERBODYROOT_PARAM_SLIDER_UPPERBODYJOINTINDEX]->mType = WIDGET_TYPE_SLIDER_UINT;
        widgets[UPPERBODYROOT_PARAM_SLIDER_UPPERBODYJOINTINDEX]->pWidget = &upperBodyJointIndex;
        widgets[UPPERBODYROOT_PARAM_SLIDER_UPPERBODYJOINTINDEX]->pOnEdited = &UpperBodyJointIndexCallback;

        luaRegisterWidget(uiCreateComponentWidget(AnimationControlsGUIWindow[2], "Upper Body Root", &CollapsingUpperBodyRootWidgets,
                                                  WIDGET_TYPE_COLLAPSING_HEADER));

        // STAND CLIP
        //
        CollapsingHeaderWidget CollapsingStandClipWidgets;
        CollapsingStandClipWidgets.pGroupedWidgets = widgets;
        CollapsingStandClipWidgets.mWidgetsCount = CLIP_PARAM_COUNT;

        // Play/Pause - Checkbox
        CheckboxWidget playStand;
        playStand.pData = &gUIData.mStandClip.mPlay;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pWidget = &playStand;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_PLAY]->mLabel, "Play");
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pOnEdited = &StandClipPlayCallback;

        // Loop - Checkbox
        CheckboxWidget loopStand;
        loopStand.pData = &gUIData.mStandClip.mLoop;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pWidget = &loopStand;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_LOOP]->mLabel, "Loop");
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pOnEdited = &StandClipLoopCallback;

        // Animation Time - Slider
        SliderFloatWidget animationTime;
        animationTime.pData = &gUIData.mStandClip.mAnimationTime;
        animationTime.mMin = 0.0f;
        animationTime.mMax = gStandClipController[0].mDuration;
        animationTime.mStep = 0.01f;
        strcpy(widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mLabel, "Animation Time");
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pWidget = &animationTime;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pOnEdited = &StandClipTimeChangeCallback;

        // Playback Speed - Slider
        SliderFloatWidget playbackTime;
        playbackTime.pData = &gUIData.mStandClip.mPlaybackSpeed;
        playbackTime.mMin = -5.0f;
        playbackTime.mMax = 5.0f;
        playbackTime.mStep = 0.1f;
        strcpy(widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mLabel, "Playback Speed");
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pWidget = &playbackTime;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pOnEdited = &StandClipPlaybackSpeedChangeCallback;

        luaRegisterWidget(uiCreateComponentWidget(AnimationControlsGUIWindow[2], "StandClip (UpperBody)", &CollapsingStandClipWidgets,
                                                  WIDGET_TYPE_COLLAPSING_HEADER));

        // WALK CLIP
        //
        CollapsingHeaderWidget CollapsingWalkClipWidgets;
        CollapsingWalkClipWidgets.pGroupedWidgets = widgets;
        CollapsingWalkClipWidgets.mWidgetsCount = CLIP_PARAM_COUNT;

        // Play/Pause - Checkbox
        CheckboxWidget playWalk;
        playWalk.pData = &gUIData.mWalkClip.mPlay;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pWidget = &playWalk;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_PLAY]->mLabel, "Play");
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pOnEdited = &WalkClipPlayCallback;

        // Loop - Checkbox
        CheckboxWidget loopWalk;
        loopWalk.pData = &gUIData.mWalkClip.mLoop;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pWidget = &loopWalk;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_LOOP]->mLabel, "Loop");
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pOnEdited = &WalkClipLoopCallback;

        // Animation Time - Slider
        SliderFloatWidget animationTimeWalk;
        animationTimeWalk.pData = &gUIData.mWalkClip.mAnimationTime;
        animationTimeWalk.mMin = 0.01f;
        animationTimeWalk.mMax = gWalkClipController[0].mDuration;
        animationTimeWalk.mStep = 0.01f;
        strcpy(widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mLabel, "Animation Time");
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pWidget = &animationTimeWalk;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pOnEdited = &WalkClipTimeChangeCallback;

        // Playback Speed - Slider
        SliderFloatWidget playbackTimeWalk;
        playbackTimeWalk.pData = &gUIData.mWalkClip.mPlaybackSpeed;
        playbackTimeWalk.mMin = -5.0f;
        playbackTimeWalk.mMax = 5.0f;
        playbackTimeWalk.mStep = 0.1f;
        strcpy(widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mLabel, "Playback Speed");
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pWidget = &playbackTimeWalk;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pOnEdited = &WalkClipPlaybackSpeedChangeCallback;

        luaRegisterWidget(uiCreateComponentWidget(AnimationControlsGUIWindow[2], "Walk Clip (Lower Body)", &CollapsingWalkClipWidgets,
                                                  WIDGET_TYPE_COLLAPSING_HEADER));
    }

    // SET UP GUI FOR AdditiveBlending EXAMPLE
    //
    {
        // BLEND PARAMETERS
        //
        CollapsingHeaderWidget CollapsingBlendParamsWidgets;
        CollapsingBlendParamsWidgets.pGroupedWidgets = widgets;
        CollapsingBlendParamsWidgets.mWidgetsCount = ADDITIVEBLEND_PARAM_COUNT;

        // Walk Clip Weight - Slider
        SliderFloatWidget walkClipWeight;
        walkClipWeight.pData = &gUIData.mAdditiveBlendingParams.mWalkClipWeight;
        walkClipWeight.mMin = 0.0f;
        walkClipWeight.mMax = 1.0f;
        walkClipWeight.mStep = 0.01f;
        strcpy(widgets[ADDITIVEBLEND_PARAM_SLIDER_WALKCLIPWEIGHT]->mLabel, "Clip Weight [Walk]");
        widgets[ADDITIVEBLEND_PARAM_SLIDER_WALKCLIPWEIGHT]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[ADDITIVEBLEND_PARAM_SLIDER_WALKCLIPWEIGHT]->pWidget = &walkClipWeight;
        widgets[ADDITIVEBLEND_PARAM_SLIDER_WALKCLIPWEIGHT]->pOnEdited = &WalkClipWeightCallback;

        // NeckCrack Clip Weight - Slider
        SliderFloatWidget neckCrackClipWeight;
        neckCrackClipWeight.pData = &gUIData.mAdditiveBlendingParams.mNeckCrackClipWeight;
        neckCrackClipWeight.mMin = 0.0f;
        neckCrackClipWeight.mMax = 1.0f;
        neckCrackClipWeight.mStep = 0.01f;
        strcpy(widgets[ADDITIVEBLEND_PARAM_SLIDER_NECKCRACKCLIPKWEIGHT]->mLabel, "Clip Weight [NeckCrack]");
        widgets[ADDITIVEBLEND_PARAM_SLIDER_NECKCRACKCLIPKWEIGHT]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[ADDITIVEBLEND_PARAM_SLIDER_NECKCRACKCLIPKWEIGHT]->pWidget = &neckCrackClipWeight;
        widgets[ADDITIVEBLEND_PARAM_SLIDER_NECKCRACKCLIPKWEIGHT]->pOnEdited = NULL;

        luaRegisterWidget(uiCreateComponentWidget(AnimationControlsGUIWindow[3], "Blend Parameters", &CollapsingBlendParamsWidgets,
                                                  WIDGET_TYPE_COLLAPSING_HEADER));

        // UPPER BODY MASK
        //
        CollapsingHeaderWidget CollapsingUpperBodyMaskWidgets;
        CollapsingUpperBodyMaskWidgets.pGroupedWidgets = widgets;
        CollapsingUpperBodyMaskWidgets.mWidgetsCount = UPPERBODYMASK_PARAM_COUNT;

        // EnableMask - Checkbox
        CheckboxWidget enabledMask;
        enabledMask.pData = &gUIData.mUpperBodyMask.mEnableMask;
        widgets[UPPERBODYMASK_PARAM_CHECKBOX_ENABLEMASK]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[UPPERBODYMASK_PARAM_CHECKBOX_ENABLEMASK]->pWidget = &enabledMask;
        strcpy(widgets[UPPERBODYMASK_PARAM_CHECKBOX_ENABLEMASK]->mLabel, "Enable Mask");
        widgets[UPPERBODYMASK_PARAM_CHECKBOX_ENABLEMASK]->pOnEdited = &EnableMaskCallback;

        // NeckCrack Joints Weight - Slider
        SliderFloatWidget neckCrackJointsWeight;
        neckCrackJointsWeight.pData = &gUIData.mUpperBodyMask.mNeckCrackJointsWeight;
        neckCrackJointsWeight.mMin = 0.01f;
        neckCrackJointsWeight.mMax = 1.0f;
        neckCrackJointsWeight.mStep = 0.01f;
        strcpy(widgets[UPPERBODYMASK_PARAM_SLIDER_NECKCRACKJOINTWEIGHT]->mLabel, "Joints Weight [NeckCrack]");
        widgets[UPPERBODYMASK_PARAM_SLIDER_NECKCRACKJOINTWEIGHT]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[UPPERBODYMASK_PARAM_SLIDER_NECKCRACKJOINTWEIGHT]->pWidget = &neckCrackJointsWeight;
        widgets[UPPERBODYMASK_PARAM_SLIDER_NECKCRACKJOINTWEIGHT]->pOnEdited = &NeckCrackClipJointsWeightCallback;

        // UpperBodyJointIndex - Slider
        SliderUintWidget upperBodyJointIndex;
        upperBodyJointIndex.pData = &gUIData.mUpperBodyMask.mUpperBodyJointIndex;
        upperBodyJointIndex.mMin = 0;
        upperBodyJointIndex.mMax = gStickFigureRig.mNumJoints - 1;
        upperBodyJointIndex.mStep = 1;
        strcpy(widgets[UPPERBODYMASK_PARAM_SLIDER_UPPERBODYJOINTWEIGHT]->mLabel, "Root Joint Index");
        widgets[UPPERBODYMASK_PARAM_SLIDER_UPPERBODYJOINTWEIGHT]->mType = WIDGET_TYPE_SLIDER_UINT;
        widgets[UPPERBODYMASK_PARAM_SLIDER_UPPERBODYJOINTWEIGHT]->pWidget = &upperBodyJointIndex;
        widgets[UPPERBODYMASK_PARAM_SLIDER_UPPERBODYJOINTWEIGHT]->pOnEdited = &UpperBodyJointIndexCallback;

        luaRegisterWidget(uiCreateComponentWidget(AnimationControlsGUIWindow[3], "Upper Body Masking", &CollapsingUpperBodyMaskWidgets,
                                                  WIDGET_TYPE_COLLAPSING_HEADER));

        // WALK CLIP
        //
        CollapsingHeaderWidget CollapsingWalkClipWidgets;
        CollapsingWalkClipWidgets.pGroupedWidgets = widgets;
        CollapsingWalkClipWidgets.mWidgetsCount = CLIP_PARAM_COUNT;

        // Play/Pause - Checkbox
        CheckboxWidget playWalk;
        playWalk.pData = &gUIData.mWalkClip.mPlay;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pWidget = &playWalk;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_PLAY]->mLabel, "Play");
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pOnEdited = &WalkClipPlayCallback;

        // Loop - Checkbox
        CheckboxWidget loopWalk;
        loopWalk.pData = &gUIData.mWalkClip.mLoop;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pWidget = &loopWalk;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_LOOP]->mLabel, "Loop");
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pOnEdited = &WalkClipLoopCallback;

        // Animation Time - Slider
        SliderFloatWidget animationTimeWalk;
        animationTimeWalk.pData = &gUIData.mWalkClip.mAnimationTime;
        animationTimeWalk.mMin = 0.01f;
        animationTimeWalk.mMax = gWalkClipController[0].mDuration;
        animationTimeWalk.mStep = 0.01f;
        strcpy(widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mLabel, "Animation Time");
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pWidget = &animationTimeWalk;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pOnEdited = &WalkClipTimeChangeCallback;

        // Playback Speed - Slider
        SliderFloatWidget playbackTimeWalk;
        playbackTimeWalk.pData = &gUIData.mWalkClip.mPlaybackSpeed;
        playbackTimeWalk.mMin = -5.0f;
        playbackTimeWalk.mMax = 5.0f;
        playbackTimeWalk.mStep = 0.1f;
        strcpy(widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mLabel, "Playback Speed");
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pWidget = &playbackTimeWalk;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pOnEdited = &WalkClipPlaybackSpeedChangeCallback;

        luaRegisterWidget(
            uiCreateComponentWidget(AnimationControlsGUIWindow[3], "Walk Clip", &CollapsingWalkClipWidgets, WIDGET_TYPE_COLLAPSING_HEADER));

        // NECK CRACK CLIP
        //
        CollapsingHeaderWidget CollapsingNeckCrackClipWidgets;
        CollapsingNeckCrackClipWidgets.pGroupedWidgets = widgets;
        CollapsingNeckCrackClipWidgets.mWidgetsCount = CLIP_PARAM_COUNT;

        // Play/Pause - Checkbox
        CheckboxWidget playNeckCrack;
        playNeckCrack.pData = &gUIData.mNeckCrackClip.mPlay;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pWidget = &playNeckCrack;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_PLAY]->mLabel, "Play");
        widgets[CLIP_PARAM_CHECKBOX_PLAY]->pOnEdited = &NeckClipPlayCallback;

        // Loop - Checkbox
        CheckboxWidget loopNeck;
        loopNeck.pData = &gUIData.mNeckCrackClip.mLoop;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->mType = WIDGET_TYPE_CHECKBOX;
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pWidget = &loopNeck;
        strcpy(widgets[CLIP_PARAM_CHECKBOX_LOOP]->mLabel, "Loop");
        widgets[CLIP_PARAM_CHECKBOX_LOOP]->pOnEdited = &NeckClipLoopCallback;

        // Animation Time - Slider
        SliderFloatWidget animationTimeNeck;
        animationTimeNeck.pData = &gUIData.mNeckCrackClip.mAnimationTime;
        animationTimeNeck.mMin = 0.01f;
        animationTimeNeck.mMax = gNeckCrackClipController[0].mDuration;
        animationTimeNeck.mStep = 0.01f;
        strcpy(widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mLabel, "Animation Time");
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pWidget = &animationTimeNeck;
        widgets[CLIP_PARAM_SLIDER_ANIMATIONTIME]->pOnEdited = &NeckCrackClipTimeChangeCallback;

        // Playback Speed - Slider
        SliderFloatWidget playbackTimeNeck;
        playbackTimeNeck.pData = &gUIData.mNeckCrackClip.mPlaybackSpeed;
        playbackTimeNeck.mMin = -5.0f;
        playbackTimeNeck.mMax = 5.0f;
        playbackTimeNeck.mStep = 0.1f;
        strcpy(widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mLabel, "Playback Speed");
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pWidget = &playbackTimeNeck;
        widgets[CLIP_PARAM_SLIDER_PLAYBACK]->pOnEdited = &NeckClipPlaybackSpeedChangeCallback;

        luaRegisterWidget(uiCreateComponentWidget(AnimationControlsGUIWindow[3], "NeckCrack Clip (Additive)",
                                                  &CollapsingNeckCrackClipWidgets, WIDGET_TYPE_COLLAPSING_HEADER));
    }

    // Animations
    DropdownWidget ddAnimations;
    ddAnimations.pData = &gCurrentAnimationIndex;
    ddAnimations.pNames = gAnimationNames;
    ddAnimations.mCount = ANIMATIONCOUNT;
    UIWidget* pDdAnimation = uiCreateComponentWidget(pStandaloneAnimationsGUIWindow, "Animation", &ddAnimations, WIDGET_TYPE_DROPDOWN);
    luaRegisterWidget(pDdAnimation);

    ButtonWidget bRunAnimation;
    UIWidget* pRunAnimation = uiCreateComponentWidget(pStandaloneAnimationsGUIWindow, "Run Animation", &bRunAnimation, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pRunAnimation, nullptr, RunAnimation);
    luaRegisterWidget(pRunAnimation);

    // Scripts
    DropdownWidget ddTestScripts;
    ddTestScripts.pData = &gCurrentScriptIndex;
    ddTestScripts.pNames = gTestScripts;
    ddTestScripts.mCount = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
    luaRegisterWidget(uiCreateComponentWidget(pStandaloneAnimationsGUIWindow, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

    ButtonWidget bRunScript;
    UIWidget*    pRunScript = uiCreateComponentWidget(pStandaloneAnimationsGUIWindow, "Run Script", &bRunScript, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript);
    luaRegisterWidget(pRunScript);
}

//--------------------------------------------------------------------------------------------
// APP CODE
//--------------------------------------------------------------------------------------------
class Animations: public IApp
{
public:
    bool Init() override
    {
        initHiresTimer(&gAnimationUpdateTimer);

        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_ANIMATIONS, "Animation");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

        // RIG
        //
        // Initialize the rig with the path to its ozz file and its rendering details
        gStickFigureRig.Initialize(RD_ANIMATIONS, gStickFigureName);

        // Generate joint vertex buffer
        generateQuad(&pJointPoints, &gNumberOfJointPoints, gJointRadius);

        // Generate bone vertex buffer
        generateIndexedBonePoints(&pBonePoints, &gNumberOfBonePoints, gBoneWidthRatio, gStickFigureRig.mNumJoints,
                                  &gStickFigureRig.mSkeleton.joint_parents()[0]);

        // Generate attached object vertex buffer
        generateCuboidPoints(&pCuboidPoints, &gNumberOfCuboidPoints);

        // Generate cubes vertex buffer
        generateCuboidPoints(&pCubesPoints, &gNumberOfCubes, 0.065f, 0.065f, 0.065f); // Use cuboids of size 1x1x1

        gOzzLogoRig.Initialize(RD_ANIMATIONS, gOzzLogoName);

        // Find the index of the joint to mount the camera to
        gCameraIndex = gOzzLogoRig.FindJoint("camera");

        // CLIPS
        //
        // Since all the skeletons are the same we can just initialize with the first one
        gStandClip.Initialize(RD_ANIMATIONS, gStandClipName, &gStickFigureRig);
        gWalkClip.Initialize(RD_ANIMATIONS, gWalkClipName, &gStickFigureRig);
        gJogClip.Initialize(RD_ANIMATIONS, gJogClipName, &gStickFigureRig);
        gRunClip.Initialize(RD_ANIMATIONS, gRunClipName, &gStickFigureRig);
        gNeckCrackClip.Initialize(RD_ANIMATIONS, gNeckCrackClipName, &gStickFigureRig);
        gShatterClip.Initialize(RD_ANIMATIONS, gShatterClipName, &gOzzLogoRig);

        // CLIP MASKS
        //
        gStandClipMask.Initialize(&gStickFigureRig);
        gWalkClipMask.Initialize(&gStickFigureRig);
        gNeckCrackClipMask.Initialize(&gStickFigureRig);

        // Initialize the masks with their default values
        gStandClipMask.DisableAllJoints();
        gStandClipMask.SetAllChildrenOf(kSpineJointIndex, kDefaultStandJointsWeight);

        gWalkClipMask.EnableAllJoints();
        gWalkClipMask.SetAllChildrenOf(kSpineJointIndex, kDefaultWalkJointsWeight);

        gNeckCrackClipMask.DisableAllJoints();
        gNeckCrackClipMask.SetAllChildrenOf(kSpineJointIndex, kDefaultNeckCrackJointsWeight);

        // CLIP CONTROLLERS
        //
        // Initialize with the length of the clip they are controlling and an
        // optional external time to set based on their updating
        for (size_t i = 0; i < MAX_ANIMATED_OBJECTS; i++)
        {
            gStandClipController[i].Initialize(gStandClip.GetDuration(), &gUIData.mStandClip.mAnimationTime);
            gWalkClipController[i].Initialize(gWalkClip.GetDuration(), &gUIData.mWalkClip.mAnimationTime);
            gJogClipController[i].Initialize(gJogClip.GetDuration(), &gUIData.mJogClip.mAnimationTime);
            gRunClipController[i].Initialize(gRunClip.GetDuration(), &gUIData.mRunClip.mAnimationTime);
            gNeckCrackClipController[i].Initialize(gNeckCrackClip.GetDuration(), &gUIData.mNeckCrackClip.mAnimationTime);
        }
        gShatterClipContoller.Initialize(gShatterClip.GetDuration(), &gUIData.mShatterClip.mAnimationTime);

        // ANIMATIONS
        //
        AnimationDesc animationDesc{};

        for (size_t i = 0; i < MAX_ANIMATED_OBJECTS; i++)
        {
            // Stand Animation
            animationDesc = {};
            animationDesc.mRig = &gStickFigureRig;
            animationDesc.mNumLayers = 1;
            animationDesc.mLayerProperties[0].mClip = &gStandClip;
            animationDesc.mLayerProperties[0].mClipController = &gStandClipController[i];
            gAnimations[0][i].Initialize(animationDesc);

            // Blend Animation
            animationDesc = {};
            animationDesc.mRig = &gStickFigureRig;
            animationDesc.mNumLayers = 3;
            animationDesc.mLayerProperties[0].mClip = &gWalkClip;
            animationDesc.mLayerProperties[0].mClipController = &gWalkClipController[i];
            animationDesc.mLayerProperties[1].mClip = &gJogClip;
            animationDesc.mLayerProperties[1].mClipController = &gJogClipController[i];
            animationDesc.mLayerProperties[2].mClip = &gRunClip;
            animationDesc.mLayerProperties[2].mClipController = &gRunClipController[i];
            animationDesc.mBlendType = BlendType::CROSS_DISSOLVE_SYNC;
            gAnimations[1][i].Initialize(animationDesc);

            // PartialBlending Animation
            animationDesc = {};
            animationDesc.mRig = &gStickFigureRig;
            animationDesc.mNumLayers = 2;
            animationDesc.mLayerProperties[0].mClip = &gStandClip;
            animationDesc.mLayerProperties[0].mClipController = &gStandClipController[i];
            animationDesc.mLayerProperties[0].mClipMask = &gStandClipMask;
            animationDesc.mLayerProperties[1].mClip = &gWalkClip;
            animationDesc.mLayerProperties[1].mClipController = &gWalkClipController[i];
            animationDesc.mLayerProperties[1].mClipMask = &gWalkClipMask;
            animationDesc.mBlendType = BlendType::EQUAL;
            gAnimations[2][i].Initialize(animationDesc);

            // AdditiveBlending Animation
            animationDesc = {};
            animationDesc.mRig = &gStickFigureRig;
            animationDesc.mNumLayers = 2;
            animationDesc.mLayerProperties[0].mClip = &gWalkClip;
            animationDesc.mLayerProperties[0].mClipController = &gWalkClipController[i];
            animationDesc.mLayerProperties[1].mClip = &gNeckCrackClip;
            animationDesc.mLayerProperties[1].mClipController = &gNeckCrackClipController[i];
            animationDesc.mLayerProperties[1].mClipMask = &gNeckCrackClipMask;
            animationDesc.mLayerProperties[1].mAdditive = true;
            animationDesc.mBlendType = BlendType::EQUAL;
            gAnimations[3][i].Initialize(animationDesc);
            // For this example we always want the UI and not the animation to control blend parameters
            gAnimations[3][i].mAutoSetBlendParams = false;
        }

        animationDesc = {};
        animationDesc.mRig = &gOzzLogoRig;
        animationDesc.mNumLayers = 1;
        animationDesc.mLayerProperties[0].mClip = &gShatterClip;
        animationDesc.mLayerProperties[0].mClipController = &gShatterClipContoller;
        gShatterAnimation.Initialize(animationDesc);

        // ANIMATED OBJECTS
        //
        const unsigned int gridWidth = 25;
        const unsigned int gridDepth = 10;
        for (unsigned int i = 0; i < MAX_ANIMATED_OBJECTS; i++)
        {
            gStickFigureAnimObject[i].Initialize(&gStickFigureRig, &gAnimations[0][i]);

            // Calculate and set offset for each rig
            vec3 offset =
                vec3(-8.75f + 0.75f * (i % gridWidth), ((i / gridWidth) / gridDepth) * 2.0f, 8.0f - 2 * ((i / gridWidth) % gridDepth));
            gStickFigureAnimObject[i].mRootTransform = mat4::translation(offset);

            gStickFigureAnimObject[i].ComputeBindPose(gStickFigureAnimObject[i].mRootTransform);
            gStickFigureAnimObject[i].ComputeJointScales(gStickFigureAnimObject[i].mRootTransform);

            // alternate the bone colors
            if (i % 2 == 1)
            {
#ifdef ENABLE_FORGE_ANIMATION_DEBUG
                gStickFigureAnimObject[i].mBoneColor = vec4(.1f, .2f, .9f, 1.f);
#endif
            }
        }
        gOzzLogoAnimObject.Initialize(&gOzzLogoRig, &gShatterAnimation);
        gOzzLogoAnimObject.mRootTransform = mat4::translation(vec3(-12.5f, 0.0f, 0.0f)) * mat4::rotationY(degToRad(180.0f));

        const char* aimJointNames[4] = { "Head", "Spine3", "Spine2", "Spine1" };
        gAimIKDesc.mForward = Vector3::yAxis();
        gAimIKDesc.mOffset = Vector3(.07f, .1f, 0.f);
        gAimIKDesc.mPoleVector = Vector3::yAxis();
        gAimIKDesc.mTwistAngle = 0.0f;
        gAimIKDesc.mJointWeight = 0.5f;
        gAimIKDesc.mJointChainLength = 4;
        gAimIKDesc.mJointChain = gJointChain;
        gAimIKDesc.mJointUpVectors = gJointUpVectors;
        gStickFigureRig.FindJointChain(aimJointNames, gAimIKDesc.mJointChainLength, gJointChain);

        const char* twoBonesJointNames[] = { "RightUpLeg", "RightLeg", "RightFoot" };
        gTwoBonesIKDesc.mSoften = 1.0f;
        gTwoBonesIKDesc.mWeight = 1.0f;
        gTwoBonesIKDesc.mTwistAngle = 0.0f;
        gTwoBonesIKDesc.mPoleVector = Vector3::zAxis();
        gTwoBonesIKDesc.mMidAxis = Vector3::zAxis();
        gStickFigureRig.FindJointChain(twoBonesJointNames, 3, gTwoBonesIKDesc.mJointChain);

        // WINDOW AND RENDERER SETUP
        //
        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        initRenderer(GetName(), &settings, &pRenderer);
        if (!pRenderer) // check for init success
            return false;

        // CREATE COMMAND LIST AND GRAPHICS/COMPUTE QUEUES
        //
        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        addGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        addSemaphore(pRenderer, &pImageAcquiredSemaphore);

        // INITIALIZE RESOURCE/DEBUG SYSTEMS
        //
        initResourceLoaderInterface(pRenderer);

        // Load fonts
        FontDesc font = {};
        font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
        fntDefineFonts(&font, 1, &gFontID);

        FontSystemDesc fontRenderDesc = {};
        fontRenderDesc.pRenderer = pRenderer;
        if (!initFontSystem(&fontRenderDesc))
            return false; // report?

        // Initialize Forge User Interface Rendering
        UserInterfaceDesc uiRenderDesc = {};
        uiRenderDesc.pRenderer = pRenderer;
        initUserInterface(&uiRenderDesc);

        const uint32_t numScripts = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
        LuaScriptDesc  scriptDescs[numScripts] = {};
        for (uint32_t i = 0; i < numScripts; ++i)
            scriptDescs[i].pScriptFileName = gTestScripts[i];
        luaDefineScripts(scriptDescs, numScripts);

        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        profiler.mWidthUI = mSettings.mWidth;
        profiler.mHeightUI = mSettings.mHeight;
        initProfiler(&profiler);

        gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

        // INITIALIZE PIPILINE STATES
        //

        uint64_t       jointDataSize = gNumberOfJointPoints * sizeof(float);
        BufferLoadDesc jointVbDesc = {};
        jointVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        jointVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        jointVbDesc.mDesc.mSize = jointDataSize;
        jointVbDesc.pData = pJointPoints;
        jointVbDesc.ppBuffer = &pJointVertexBuffer;
        addResource(&jointVbDesc, NULL);

        uint64_t       boneDataSize = gNumberOfBonePoints * sizeof(float);
        BufferLoadDesc boneVbDesc = {};
        boneVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        boneVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        boneVbDesc.mDesc.mSize = boneDataSize;
        boneVbDesc.pData = pBonePoints;
        boneVbDesc.ppBuffer = &pBoneVertexBuffer;
        addResource(&boneVbDesc, NULL);

        uint64_t       cuboidDataSize = gNumberOfCuboidPoints * sizeof(float);
        BufferLoadDesc cuboidVbDesc = {};
        cuboidVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        cuboidVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        cuboidVbDesc.mDesc.mSize = cuboidDataSize;
        cuboidVbDesc.pData = pCuboidPoints;
        cuboidVbDesc.ppBuffer = &pCuboidVertexBuffer;
        addResource(&cuboidVbDesc, NULL);

        uint64_t       cubesDataSize = gNumberOfCubes * sizeof(float);
        BufferLoadDesc cubeVbDesc = {};
        cubeVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        cubeVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        cubeVbDesc.mDesc.mSize = cubesDataSize;
        cubeVbDesc.pData = pCubesPoints;
        cubeVbDesc.ppBuffer = &pCubesVertexBuffer;
        addResource(&cubeVbDesc, NULL);

        // Generate plane vertex buffer
        float planePoints[] = { -15.0f, 0.0f, -15.0f, 1.0f, 0.0f, 0.0f, -15.0f, 0.0f, 15.0f,  1.0f, 1.0f, 0.0f,
                                15.0f,  0.0f, 15.0f,  1.0f, 1.0f, 1.0f, 15.0f,  0.0f, 15.0f,  1.0f, 1.0f, 1.0f,
                                15.0f,  0.0f, -15.0f, 1.0f, 0.0f, 1.0f, -15.0f, 0.0f, -15.0f, 1.0f, 0.0f, 0.0f };

        uint64_t       planeDataSize = 6 * 6 * sizeof(float);
        BufferLoadDesc planeVbDesc = {};
        planeVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        planeVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        planeVbDesc.mDesc.mSize = planeDataSize;
        planeVbDesc.pData = planePoints;
        planeVbDesc.ppBuffer = &pPlaneVertexBuffer;
        addResource(&planeVbDesc, NULL);

        BufferLoadDesc ubDesc = {};
        ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDesc.mDesc.mSize = sizeof(UniformBlockPlane);
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.pData = NULL;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDesc.ppBuffer = &pPlaneUniformBuffer[i];
            addResource(&ubDesc, NULL);
        }

        BufferLoadDesc ubDescCuboid = {};
        ubDescCuboid.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDescCuboid.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDescCuboid.mDesc.mSize = sizeof(UniformBlock);
        ubDescCuboid.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDescCuboid.pData = NULL;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDescCuboid.ppBuffer = &pCuboidUniformBuffer[i];
            addResource(&ubDescCuboid, NULL);
        }
        ubDesc.mDesc.mSize = sizeof(UniformSkeletonBlock);
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDesc.ppBuffer = &pTargetUniformBuffer[i];
            addResource(&ubDesc, NULL);
        }
        /************************************************************************/
        // SETUP ANIMATION STRUCTURES
        /************************************************************************/

        // SKELETON RENDERER
        //

        // Set up details for rendering the skeletons
        SkeletonRenderDesc skeletonRenderDesc = {};
        skeletonRenderDesc.mRenderer = pRenderer;
        skeletonRenderDesc.mFrameCount = gDataBufferCount;
        skeletonRenderDesc.mMaxSkeletonBatches = 512;
        skeletonRenderDesc.mJointVertexBuffer = pJointVertexBuffer;
        skeletonRenderDesc.mNumJointPoints = gNumberOfJointPoints;
        skeletonRenderDesc.mDrawBones = true;
        skeletonRenderDesc.mBoneVertexBuffer = pBoneVertexBuffer;
        skeletonRenderDesc.mNumBonePoints = gNumberOfBonePoints;
        skeletonRenderDesc.mBoneVertexStride = sizeof(float) * 8;
        skeletonRenderDesc.mJointVertexStride = sizeof(float) * 6;
        skeletonRenderDesc.mMaxAnimatedObjects = MAX_ANIMATED_OBJECTS;
        skeletonRenderDesc.mJointMeshType = QuadSphere;
        gSkeletonBatcher.Initialize(skeletonRenderDesc);

        // Add the rig to the list of skeletons to render
        for (size_t i = 0; i < MAX_ANIMATED_OBJECTS; i++)
        {
            gSkeletonBatcher.AddAnimatedObject(&gStickFigureAnimObject[i]);
        }

        SkeletonRenderDesc ozzSkeletonRenderDesc = {};
        ozzSkeletonRenderDesc.mRenderer = pRenderer;
        ozzSkeletonRenderDesc.mFrameCount = gDataBufferCount;
        ozzSkeletonRenderDesc.mMaxSkeletonBatches = 512;
        ozzSkeletonRenderDesc.mJointVertexBuffer = pCubesVertexBuffer;
        ozzSkeletonRenderDesc.mNumJointPoints = gNumberOfCubes;
        ozzSkeletonRenderDesc.mDrawBones = false; // Indicate that we do not wish to have bones between each joint
        ozzSkeletonRenderDesc.mBoneVertexStride = sizeof(float) * 6;
        ozzSkeletonRenderDesc.mJointVertexStride = sizeof(float) * 6;
        ozzSkeletonRenderDesc.mMaxAnimatedObjects = 1;
        ozzSkeletonRenderDesc.mJointMeshType = Cube;
        ozzSkeletonRenderDesc.mJointVertShaderName = "cube.vert";
        ozzSkeletonRenderDesc.mJointFragShaderName = "cube.frag";
        gOzzLogoSkeletonBatcher.Initialize(ozzSkeletonRenderDesc);
        gOzzLogoSkeletonBatcher.AddAnimatedObject(&gOzzLogoAnimObject);

        /************************************************************************/

        // Add the GUI Panels/Windows
        vec2            UIPosition = { mSettings.mWidth * 0.006f, mSettings.mHeight * 0.17f };
        vec2            UIPanelSize = { 650, 1000 };
        UIComponentDesc guiDesc = {};
        guiDesc.mStartPosition = UIPosition;
        guiDesc.mStartSize = UIPanelSize;
        guiDesc.mFontID = 0;
        uiCreateComponent("Animations", &guiDesc, &pStandaloneAnimationsGUIWindow);

        UIPosition = { mSettings.mWidth * 0.15f, mSettings.mHeight * 0.17f };
        UIPanelSize = { 650, 1000 };
        guiDesc = {};
        guiDesc.mStartPosition = UIPosition;
        guiDesc.mStartSize = UIPanelSize;
        guiDesc.mFontID = 0;
        uiCreateComponent("Stand Animation", &guiDesc, &AnimationControlsGUIWindow[0]);

        uiCreateComponent("Blend Animation", &guiDesc, &AnimationControlsGUIWindow[1]);
        uiSetComponentActive(AnimationControlsGUIWindow[1], false);

        uiCreateComponent("PartialBlending Animation", &guiDesc, &AnimationControlsGUIWindow[2]);
        uiSetComponentActive(AnimationControlsGUIWindow[2], false);

        uiCreateComponent("AdditiveBlending Animation", &guiDesc, &AnimationControlsGUIWindow[3]);
        uiSetComponentActive(AnimationControlsGUIWindow[3], false);

        SetUpAnimationSpecificGuiWindows();

        // SETUP THE MAIN CAMERA
        //
        CameraMotionParameters cmp{ 50.0f, 75.0f, 150.0f };
        vec3                   camPos{ -15.0f, 5.0f, 13.0f };
        vec3                   lookAt{ 0.0f, 0.0f, -1.5f };

        pCameraController = initFpsCameraController(camPos, lookAt);
        pCameraController->setMotionParameters(cmp);

        // INITIALIZE THREAD SYSTEM
        //
        threadSystemInit(&gThreadSystem, &gThreadSystemInitDescDefault);

        InputSystemDesc inputDesc = {};
        inputDesc.pRenderer = pRenderer;
        inputDesc.pWindow = pWindow;
        inputDesc.pJoystickTexture = "circlepad.tex";
        if (!initInputSystem(&inputDesc))
            return false;

        // App Actions
        InputActionDesc actionDesc = { DefaultInputActions::DUMP_PROFILE_DATA,
                                       [](InputActionContext* ctx)
                                       {
                                           dumpProfileData(((Renderer*)ctx->pUserData)->pName);
                                           return true;
                                       },
                                       pRenderer };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TOGGLE_FULLSCREEN,
                       [](InputActionContext* ctx)
                       {
                           WindowDesc* winDesc = ((IApp*)ctx->pUserData)->pWindow;
                           if (winDesc->fullScreen)
                               winDesc->borderlessWindow
                                   ? setBorderless(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect))
                                   : setWindowed(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect));
                           else
                               setFullscreen(winDesc);
                           return true;
                       },
                       this };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::EXIT, [](InputActionContext* ctx)
                       {
                           UNREF_PARAM(ctx);
                           requestShutdown();
                           return true;
                       } };
        addInputAction(&actionDesc);
        InputActionCallback onUIInput = [](InputActionContext* ctx)
        {
            if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
            {
                uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
            }
            return true;
        };

        typedef bool (*CameraInputHandler)(InputActionContext * ctx, DefaultInputActions::DefaultInputAction action);
        static CameraInputHandler onCameraInput = [](InputActionContext* ctx, DefaultInputActions::DefaultInputAction action)
        {
            if (*(ctx->pCaptured))
            {
                float2 delta = uiIsFocused() ? float2(0.f, 0.f) : ctx->mFloat2;
                switch (action)
                {
                case DefaultInputActions::ROTATE_CAMERA:
                    pCameraController->onRotate(delta);
                    break;
                case DefaultInputActions::TRANSLATE_CAMERA:
                    pCameraController->onMove(delta);
                    break;
                case DefaultInputActions::TRANSLATE_CAMERA_VERTICAL:
                    pCameraController->onMoveY(delta[0]);
                    break;
                default:
                    break;
                }
            }
            return true;
        };
        actionDesc = { DefaultInputActions::CAPTURE_INPUT,
                       [](InputActionContext* ctx)
                       {
                           setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
                           return true;
                       },
                       NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::ROTATE_CAMERA,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::ROTATE_CAMERA); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TRANSLATE_CAMERA,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::TRANSLATE_CAMERA); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TRANSLATE_CAMERA_VERTICAL,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::TRANSLATE_CAMERA_VERTICAL); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx)
                       {
                           UNREF_PARAM(ctx);
                           if (!uiWantTextInput())
                               pCameraController->resetView();
                           return true;
                       } };
        addInputAction(&actionDesc);
        GlobalInputActionDesc globalInputActionDesc = { GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this };
        setGlobalInputAction(&globalInputActionDesc);

        gFrameIndex = 0;
        waitForAllResourceLoads();

        // Need to free memory;
        tf_free(pCubesPoints);
        tf_free(pJointPoints);
        tf_free(pBonePoints);
        tf_free(pCuboidPoints);

        return true;
    }

    void Exit() override
    {
        threadSystemWaitIdle(gThreadSystem);

        exitInputSystem();
        exitCameraController(pCameraController);

        for (size_t i = 0; i < MAX_ANIMATED_OBJECTS; i++)
        {
            gStickFigureAnimObject[i].Exit();

            for (size_t j = 0; j < ANIMATIONCOUNT; j++)
            {
                gAnimations[j][i].Exit();
            }
        }
        gStickFigureRig.Exit();

        gOzzLogoAnimObject.Exit();
        gShatterAnimation.Exit();
        gOzzLogoRig.Exit();

        gStandClipMask.Exit();
        gWalkClipMask.Exit();
        gNeckCrackClipMask.Exit();

        gStandClip.Exit();
        gJogClip.Exit();
        gWalkClip.Exit();
        gRunClip.Exit();
        gNeckCrackClip.Exit();
        gShatterClip.Exit();

        exitProfiler();

        threadSystemExit(&gThreadSystem, &gThreadSystemExitDescDefault);

        exitUserInterface();

        exitFontSystem();

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pPlaneUniformBuffer[i]);
            removeResource(pCuboidUniformBuffer[i]);
            removeResource(pTargetUniformBuffer[i]);
        }

        removeResource(pCubesVertexBuffer);
        removeResource(pCuboidVertexBuffer);
        removeResource(pJointVertexBuffer);
        removeResource(pBoneVertexBuffer);
        removeResource(pPlaneVertexBuffer);

        removeSemaphore(pRenderer, pImageAcquiredSemaphore);
        removeGpuCmdRing(pRenderer, &gGraphicsCmdRing);

        // Animation data
        gOzzLogoSkeletonBatcher.Exit();
        gSkeletonBatcher.Exit();

        exitResourceLoaderInterface(pRenderer);
        removeQueue(pRenderer, pGraphicsQueue);
        exitRenderer(pRenderer);
        pRenderer = NULL;
    }

    bool Load(ReloadDesc* pReloadDesc) override
    {
        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            if (!addSwapChain())
                return false;

            if (!addDepthBuffer())
                return false;
        }

        SkeletonBatcherLoadDesc skeletonLoad = {};
        skeletonLoad.mLoadType = pReloadDesc->mType;
        skeletonLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        skeletonLoad.mDepthFormat = pDepthBuffer->mFormat;
        skeletonLoad.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        skeletonLoad.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;

        gSkeletonBatcher.Load(&skeletonLoad);
        gOzzLogoSkeletonBatcher.Load(&skeletonLoad);

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            addShaders();
            addRootSignatures();
            addDescriptorSets();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            addPipelines();
        }

        prepareDescriptorSets();

        UserInterfaceLoadDesc uiLoad = {};
        uiLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        uiLoad.mHeight = mSettings.mHeight;
        uiLoad.mWidth = mSettings.mWidth;
        uiLoad.mLoadType = pReloadDesc->mType;
        loadUserInterface(&uiLoad);

        FontSystemLoadDesc fontLoad = {};
        fontLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        fontLoad.mDepthFormat = pDepthBuffer->mFormat;
        fontLoad.mDepthCompareMode = CompareMode::CMP_GEQUAL;
        fontLoad.mHeight = mSettings.mHeight;
        fontLoad.mWidth = mSettings.mWidth;
        fontLoad.mLoadType = pReloadDesc->mType;
        loadFontSystem(&fontLoad);

        initScreenshotInterface(pRenderer, pGraphicsQueue);

        return true;
    }

    void Unload(ReloadDesc* pReloadDesc) override
    {
        waitQueueIdle(pGraphicsQueue);

        unloadFontSystem(pReloadDesc->mType);
        unloadUserInterface(pReloadDesc->mType);

        gSkeletonBatcher.Unload(pReloadDesc->mType);
        gOzzLogoSkeletonBatcher.Unload(pReloadDesc->mType);

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            removePipelines();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);
            removeRenderTarget(pRenderer, pDepthBuffer);
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSets();
            removeRootSignatures();
            removeShaders();
        }

        exitScreenshotInterface();
    }

    void Update(float deltaTime) override
    {
        updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

        /************************************************************************/
        // Scene Update
        /************************************************************************/

        // update camera with time
        mat4 viewMat = pCameraController->getViewMatrix();

        const float  aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
        const float  horizontal_fov = PI / 2.0f;
        CameraMatrix projMat = CameraMatrix::perspectiveReverseZ(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
        CameraMatrix projViewMat = projMat * viewMat;

        vec3 lightPos = vec3(0.0f, 1000.0f, 0.0f);
        vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);

        /************************************************************************/
        // Animation
        /************************************************************************/
        resetHiresTimer(&gAnimationUpdateTimer);

        // Update the animated objects and pose the rigs based on the animated object's updated values for this frame
        gSkeletonBatcher.SetActiveRigs(gNumRigs);

        // setup aim target
        static float time = 0.0f;
        time += 2.0f * deltaTime;

        Vector3 stickPos = gStickFigureAnimObject[0].mJointWorldMats[0].getTranslation();
        gAimTarget = Point3(sin(time * .5f), cos(time * .25f), cos(time) * .5f + .5f);

        gUniformDataTarget.mViewMatrix = viewMat;
        gUniformDataTarget.mProjectView = projViewMat;
        gUniformDataTarget.mLightPosition = Vector4(lightPos);
        gUniformDataTarget.mLightColor = Vector4(lightColor);
        gUniformDataTarget.mToWorldMat[0] = Matrix4(Matrix3::identity() * 0.25f, Vector3(stickPos + gAimTarget));
        gUniformDataTarget.mColor[0] = Vector4(1.0f, 0.0f, 0.0f, 1.0f);
        gUniformDataTarget.mJointColor = Vector4(1.0f, 0.0f, 0.0f, 1.0f);

        // Threading
        if (gEnableThreading)
        {
            if (gAutomateThreading)
            {
                struct ThreadSystemInfo info;
                threadSystemGetInfo(gThreadSystem, &info);
                gGrainSize = max(1U, gNumRigs / (unsigned int)info.threadCount);
            }

            gGrainSize = min(gGrainSize, gNumRigs);
            unsigned int taskCount = max(1U, gNumRigs / gGrainSize);

            // Submit taskCount number of jobs
            for (unsigned int i = 0; i < taskCount; i++)
            {
                gThreadData[i].mAnimatedObject = &gStickFigureAnimObject[gGrainSize * i];
                gThreadData[i].mDeltaTime = deltaTime;
                gThreadData[i].mNumberSystems = gGrainSize;
            }
            threadSystemAddTaskGroup(gThreadSystem, AnimatedObjectThreadedUpdate, taskCount, gThreadData);

            // If there is a remainder, submit another job to finish it
            unsigned int remainder = (uint32_t)max(0, (int32_t)gNumRigs - (int32_t)(taskCount * gGrainSize));
            if (remainder != 0)
            {
                gThreadData[taskCount].mAnimatedObject = &gStickFigureAnimObject[gGrainSize * taskCount];
                gThreadData[taskCount].mDeltaTime = deltaTime;
                gThreadData[taskCount].mNumberSystems = remainder;

                threadSystemAddTask(gThreadSystem, AnimatedObjectThreadedUpdate, &gThreadData[taskCount]);
            }
        }
        else
        {
            for (unsigned int i = 0; i < gNumRigs; ++i)
            {
                if (!gStickFigureAnimObject[i].Update(deltaTime))
                    LOGF(eERROR, "Animation NOT Updating!");

                if (gUIData.mIKParams.mAim)
                {
                    if (!gStickFigureAnimObject[i].AimIK(&gAimIKDesc, gAimTarget))
                        LOGF(eINFO, "Aim IK failed!");
                }

                if (gUIData.mIKParams.mTwoBoneIK)
                {
                    Matrix4 mat = gStickFigureAnimObject[i].mJointModelMats[gTwoBonesIKDesc.mJointChain[2]];
                    Point3  twoBoneTarget = Point3(mat.getCol3()) + Vector3(0.0f, gUIData.mIKParams.mFoot, 0.0f);

                    if (!gStickFigureAnimObject[i].TwoBonesIK(&gTwoBonesIKDesc, twoBoneTarget))
                        LOGF(eINFO, "Two bone IK failed!");
                }

                // pose rig
                if (!gUIData.mGeneralSettings.mShowBindPose)
                {
                    // Pose the rig based on the animated object's updated values
                    gStickFigureAnimObject[i].ComputePose(gStickFigureAnimObject[i].mRootTransform);
                }
                else
                {
                    // Ignore the updated values and pose in bind
                    gStickFigureAnimObject[i].ComputeBindPose(gStickFigureAnimObject[i].mRootTransform);
                }
            }

            // Record animation update time
            getHiresTimerUSec(&gAnimationUpdateTimer, true);
        }

        if (gUIData.mGeneralSettings.mDrawBakedPhysics)
        {
            // Update the animated object for this frame
            if (!gOzzLogoAnimObject.Update(deltaTime))
                LOGF(eINFO, "Animation NOT Updating!");
            gOzzLogoAnimObject.ComputePose(gOzzLogoAnimObject.mRootTransform);

            // Set the transform of the camera based on the updated world matrix of
            // the joint in the rig at index gCameraIndex
            if (gUIData.mGeneralSettings.mAnimatedCamera)
            {
                // Alter to view front of ozz logo
                mat4 cameraMat = gOzzLogoAnimObject.mJointWorldMats[gCameraIndex];
                vec4 cameraMatCol3 = cameraMat.getCol3();
                vec3 viewPos = vec3(cameraMatCol3.getX(), cameraMatCol3.getY(), -cameraMatCol3.getZ());
                vec4 cameraMatCol2 = cameraMat.getCol2();
                vec3 lookAt = vec3(-cameraMatCol2.getX(), -cameraMatCol2.getY(), cameraMatCol2.getZ()) + viewPos;
                pCameraController->moveTo(viewPos);
                pCameraController->lookAt(lookAt);
            }
            else
            {
                pCameraController->update(deltaTime);
            }

            gOzzLogoAnimObject.mJointWorldMats[gCameraIndex] = mat4::scale(vec3(0));

            gOzzLogoSkeletonBatcher.SetSharedUniforms(projViewMat, viewMat, lightPos, lightColor);
        }
        else
        {
            pCameraController->update(deltaTime);
        }

        // Update uniforms that will be shared between all skeletons
        gSkeletonBatcher.SetSharedUniforms(projViewMat, viewMat, lightPos, lightColor);
        /************************************************************************/
        // Attached object
        /************************************************************************/
        gUniformDataCuboid.mProjectView = projViewMat;
        gUniformDataCuboid.mLightPosition = Vector4(lightPos);
        gUniformDataCuboid.mLightColor = Vector4(lightColor);

        // Set the transform of the attached object based on the updated world matrix of
        // the joint in the rig specified by the UI
        gCuboidTransformMat = gStickFigureAnimObject[0].mJointWorldMats[gUIData.mAttachedObject.mJointIndex];

        // Compute the offset translation based on the UI values
        mat4 offset =
            mat4::translation(vec3(gUIData.mAttachedObject.mXOffset, gUIData.mAttachedObject.mYOffset, gUIData.mAttachedObject.mZOffset));

        gUniformDataCuboid.mToWorldMat[0] = gCuboidTransformMat * offset * gCuboidScaleMat;
        gUniformDataCuboid.mColor[0] = gCuboidColor;
        gUniformDataCuboid.mJointColor = gCuboidColor;

        /************************************************************************/
        // Plane
        /************************************************************************/
        gUniformDataPlane.mProjectView = projViewMat;
        gUniformDataPlane.mToWorldMat = mat4::identity();

        if (gEnableThreading)
        {
            threadSystemWaitIdle(gThreadSystem);

            // Record animation update time
            getHiresTimerUSec(&gAnimationUpdateTimer, true);
        }
    }

    void Draw() override
    {
        if ((bool)pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
        {
            waitQueueIdle(pGraphicsQueue);
            ::toggleVSync(pRenderer, &pSwapChain);
        }

        uint32_t swapchainImageIndex;
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

        // UPDATE UNIFORM BUFFERS
        //

        gSkeletonBatcher.PreSetInstanceUniforms(gFrameIndex);

        // Update all the instanced uniform data for each batch of joints and bones
        // Threading
        if (gEnableThreading)
        {
            unsigned int taskCount = max(1U, gNumRigs / gGrainSize);

            // Submit taskCount number of jobs
            for (unsigned int i = 0; i < taskCount; ++i)
            {
                gThreadSkeletonData[i].mFrameNumber = gFrameIndex;
                gThreadSkeletonData[i].mNumberRigs = gGrainSize;
                gThreadSkeletonData[i].mOffset = i * gGrainSize;
            }
            threadSystemAddTaskGroup(gThreadSystem, SkeletonBatchUniformsThreaded, taskCount, gThreadSkeletonData);

            // If there is a remainder, submit another job to finish it
            unsigned int remainder = (uint32_t)max(0, (int32_t)gNumRigs - (int32_t)(taskCount * gGrainSize));
            if (remainder != 0)
            {
                gThreadSkeletonData[taskCount].mFrameNumber = gFrameIndex;
                gThreadSkeletonData[taskCount].mNumberRigs = remainder;
                gThreadSkeletonData[taskCount].mOffset = taskCount * gGrainSize;

                threadSystemAddTask(gThreadSystem, SkeletonBatchUniformsThreaded, &gThreadSkeletonData[taskCount]);
            }

            // Ensure all jobs are finished before proceeding
            threadSystemWaitIdle(gThreadSystem);
        }
        else
        {
            gSkeletonBatcher.SetPerInstanceUniforms(gFrameIndex, gNumRigs);
        }

        if (gUIData.mGeneralSettings.mDrawBakedPhysics)
        {
            gOzzLogoSkeletonBatcher.PreSetInstanceUniforms(gFrameIndex);
            gOzzLogoSkeletonBatcher.SetPerInstanceUniforms(gFrameIndex);
        }

        // FRAME SYNC & ACQUIRE SWAPCHAIN RENDER TARGET
        //
        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);
        FenceStatus       fenceStatus;
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &elem.pFence);

        BufferUpdateDesc planeViewProjCbv = { pPlaneUniformBuffer[gFrameIndex] };
        beginUpdateResource(&planeViewProjCbv);
        memcpy(planeViewProjCbv.pMappedData, &gUniformDataPlane, sizeof(gUniformDataPlane));
        endUpdateResource(&planeViewProjCbv);
        BufferUpdateDesc cuboidViewProjCbv = { pCuboidUniformBuffer[gFrameIndex] };
        beginUpdateResource(&cuboidViewProjCbv);
        memcpy(cuboidViewProjCbv.pMappedData, &gUniformDataCuboid, sizeof(gUniformDataCuboid));
        endUpdateResource(&cuboidViewProjCbv);
        BufferUpdateDesc targetViewProjCbv = { pTargetUniformBuffer[gFrameIndex] };
        beginUpdateResource(&targetViewProjCbv);
        memcpy(targetViewProjCbv.pMappedData, &gUniformDataTarget, sizeof(gUniformDataTarget));
        endUpdateResource(&targetViewProjCbv);

        resetCmdPool(pRenderer, elem.pCmdPool);

        // Acquire the main render target from the swapchain
        RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
        Cmd*          cmd = elem.pCmds[0];
        beginCmd(cmd); // start recording commands

        // start gpu frame profiler
        cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

        RenderTargetBarrier barriers[] = // wait for resource transition
            {
                { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
            };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

        // bind and clear the render target
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_CLEAR };
        bindRenderTargets.mDepthStencil = { pDepthBuffer, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

        //// draw plane
        const uint32_t stride = sizeof(float) * 6;
        if (gUIData.mGeneralSettings.mDrawPlane)
        {
            cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Plane");
            cmdBindPipeline(cmd, pPlaneDrawPipeline);
            cmdBindDescriptorSet(cmd, gFrameIndex * 2 + 0, pDescriptorSet);
            cmdBindVertexBuffer(cmd, 1, &pPlaneVertexBuffer, &stride, NULL);
            cmdDraw(cmd, 6, 0);
            cmdEndDebugMarker(cmd);
        }

        //// draw the Ozz Logo of the rig
        if (gUIData.mGeneralSettings.mDrawBakedPhysics)
        {
            cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Baked Physics");
            gOzzLogoSkeletonBatcher.Draw(cmd, gFrameIndex);

            FontDrawDesc drawDesc = {};
            drawDesc.pText = "Baked Physics";
            drawDesc.mFontID = gFontID;
            drawDesc.mFontColor = 0xffff0000;
            drawDesc.mFontSize = 200.0f;
            mat4 worldMat = mat4::translation(vec3(-12.5f, 0.0f, 0.5f));
            cmdDrawWorldSpaceTextWithFont(cmd, &worldMat, &gUniformDataPlane.mProjectView, &drawDesc);

            cmdEndDebugMarker(cmd);
        }

        //// draw the skeleton of the rig
        cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons");
        gSkeletonBatcher.Draw(cmd, gFrameIndex);
        cmdEndDebugMarker(cmd);

        cmdBindPipeline(cmd, gSkeletonBatcher.mJointPipeline);
        cmdBindVertexBuffer(cmd, 1, &pJointVertexBuffer, &stride, NULL);
        cmdBindDescriptorSet(cmd, gFrameIndex, pTargetDescriptorSet);
        cmdDrawInstanced(cmd, gNumberOfJointPoints / 6, 0, 1, 0);

        //// draw the object attached to the rig
        if (gUIData.mGeneralSettings.mDrawAttachedObject)
        {
            const uint32_t strideVb = sizeof(float) * 6;
            cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Cuboid");
            cmdBindPipeline(cmd, pCubePipeline);
            cmdBindDescriptorSet(cmd, gFrameIndex * 2 + 1, pDescriptorSet);
            cmdBindVertexBuffer(cmd, 1, &pCuboidVertexBuffer, &strideVb, NULL);
            cmdDrawInstanced(cmd, gNumberOfCuboidPoints / 6, 0, 1, 0);
            cmdEndDebugMarker(cmd);
        }

        //// draw the UI
        cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
        bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(cmd, &bindRenderTargets);

        gFrameTimeDraw.mFontColor = 0xff00ffff;
        gFrameTimeDraw.mFontSize = 18.0f;
        gFrameTimeDraw.mFontID = gFontID;
        float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);

        snprintf(gAnimationUpdateText, 64, "Animation Update %f ms", getHiresTimerUSecAverage(&gAnimationUpdateTimer) / 1000.0f);

        // Disable UI rendering when taking screenshots
        if (uiIsRenderingEnabled())
        {
            gFrameTimeDraw.pText = gAnimationUpdateText;
            cmdDrawTextWithFont(cmd, float2(8.f, txtSize.y + 50.0f), &gFrameTimeDraw);
        }

        cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y + 75.0f), gGpuProfileToken, &gFrameTimeDraw);

        cmdDrawUserInterface(cmd);

        cmdBindRenderTargets(cmd, NULL);
        cmdEndDebugMarker(cmd);

        // PRESENT THE GRPAHICS QUEUE
        //
        barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
        cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
        endCmd(cmd);

        FlushResourceUpdateDesc flushUpdateDesc = {};
        flushUpdateDesc.mNodeIndex = 0;
        flushResourceUpdates(&flushUpdateDesc);
        Semaphore* waitSemaphores[2] = { flushUpdateDesc.pOutSubmittedSemaphore, pImageAcquiredSemaphore };

        QueueSubmitDesc submitDesc = {};
        submitDesc.mCmdCount = 1;
        submitDesc.mSignalSemaphoreCount = 1;
        submitDesc.mWaitSemaphoreCount = TF_ARRAY_COUNT(waitSemaphores);
        submitDesc.ppCmds = &cmd;
        submitDesc.ppSignalSemaphores = &elem.pSemaphore;
        submitDesc.ppWaitSemaphores = waitSemaphores;
        submitDesc.pSignalFence = elem.pFence;
        queueSubmit(pGraphicsQueue, &submitDesc);
        QueuePresentDesc presentDesc = {};
        presentDesc.mIndex = (uint8_t)swapchainImageIndex;
        presentDesc.mWaitSemaphoreCount = 1;
        presentDesc.ppWaitSemaphores = &elem.pSemaphore;
        presentDesc.pSwapChain = pSwapChain;
        presentDesc.mSubmitDone = true;
        queuePresent(pGraphicsQueue, &presentDesc);
        flipProfiler();

        gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;
    }

    const char* GetName() override { return "21_Animations"; }

    bool addSwapChain()
    {
        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mWindowHandle = pWindow->handle;
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.ppPresentQueues = &pGraphicsQueue;
        swapChainDesc.mWidth = mSettings.mWidth;
        swapChainDesc.mHeight = mSettings.mHeight;
        swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
        swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
        swapChainDesc.mColorClearValue = { { 0.39f, 0.41f, 0.37f, 1.0f } };
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != NULL;
    }

    void addDescriptorSets()
    {
        // 1->Plane , 2->Cuboid, 3->IK Target
        DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gDataBufferCount * 2 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSet);

        DescriptorSetDesc targetDesc = { gSkeletonBatcher.mRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gDataBufferCount };
        addDescriptorSet(pRenderer, &targetDesc, &pTargetDescriptorSet);
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pTargetDescriptorSet);
        removeDescriptorSet(pRenderer, pDescriptorSet);
    }

    void addRootSignatures()
    {
        Shader*           shaders[] = { pCubeShader, pPlaneDrawShader };
        RootSignatureDesc rootDesc = {};
        rootDesc.mShaderCount = 2;
        rootDesc.ppShaders = shaders;
        addRootSignature(pRenderer, &rootDesc, &pRootSignature);
    }

    void removeRootSignatures() { removeRootSignature(pRenderer, pRootSignature); }

    void addShaders()
    {
        ShaderLoadDesc planeShader = {};
        planeShader.mStages[0].pFileName = "plane.vert";
        planeShader.mStages[1].pFileName = "plane.frag";

        ShaderLoadDesc cubeShader = {};
        cubeShader.mStages[0].pFileName = "cube.vert";
        cubeShader.mStages[1].pFileName = "cube.frag";

        addShader(pRenderer, &planeShader, &pPlaneDrawShader);
        addShader(pRenderer, &cubeShader, &pCubeShader);
    }

    void removeShaders()
    {
        removeShader(pRenderer, pCubeShader);
        removeShader(pRenderer, pPlaneDrawShader);
    }

    void addPipelines()
    {
        // layout and pipeline for skeleton draw
        VertexLayout vertexLayout = {};
        vertexLayout.mBindingCount = 1;
        vertexLayout.mAttribCount = 2;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[0].mOffset = 0;
        vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
        vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayout.mAttribs[1].mBinding = 0;
        vertexLayout.mAttribs[1].mLocation = 1;
        vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

        RasterizerStateDesc rasterizerStateDesc = {};
        rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

        RasterizerStateDesc skeletonRasterizerStateDesc = {};
        skeletonRasterizerStateDesc.mCullMode = CULL_MODE_FRONT;

        DepthStateDesc depthStateDesc = {};
        depthStateDesc.mDepthTest = true;
        depthStateDesc.mDepthWrite = true;
        depthStateDesc.mDepthFunc = CMP_GEQUAL;

        PipelineDesc desc = {};
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pDepthState = &depthStateDesc;
        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
        pipelineSettings.pRootSignature = pRootSignature;
        pipelineSettings.pShaderProgram = pCubeShader;
        pipelineSettings.pVertexLayout = &vertexLayout;
        pipelineSettings.pRasterizerState = &skeletonRasterizerStateDesc;
        addPipeline(pRenderer, &desc, &pCubePipeline);

        // layout and pipeline for plane draw
        vertexLayout = {};
        vertexLayout.mBindingCount = 1;
        vertexLayout.mAttribCount = 2;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[0].mOffset = 0;
        vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
        vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
        vertexLayout.mAttribs[1].mBinding = 0;
        vertexLayout.mAttribs[1].mLocation = 1;
        vertexLayout.mAttribs[1].mOffset = 4 * sizeof(float);

        pipelineSettings.pDepthState = NULL;
        pipelineSettings.pRasterizerState = &rasterizerStateDesc;
        pipelineSettings.pShaderProgram = pPlaneDrawShader;
        addPipeline(pRenderer, &desc, &pPlaneDrawPipeline);
    }

    void removePipelines()
    {
        removePipeline(pRenderer, pPlaneDrawPipeline);
        removePipeline(pRenderer, pCubePipeline);
    }

    void prepareDescriptorSets()
    {
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData params[1] = {};
            params[0].pName = "uniformBlock";
            params[0].ppBuffers = &pPlaneUniformBuffer[i];
            updateDescriptorSet(pRenderer, i * 2 + 0, pDescriptorSet, 1, params);
            params[0].pName = "uniformBlock";
            params[0].ppBuffers = &pCuboidUniformBuffer[i];
            updateDescriptorSet(pRenderer, i * 2 + 1, pDescriptorSet, 1, params);
            params[0].pName = "uniformBlock";
            params[0].ppBuffers = &pTargetUniformBuffer[i];
            updateDescriptorSet(pRenderer, i, pTargetDescriptorSet, 1, params);
        }
    }

    bool addDepthBuffer()
    {
        // Add depth buffer
        RenderTargetDesc depthRT = {};
        depthRT.mArraySize = 1;
        depthRT.mClearValue.depth = 0.0f;
        depthRT.mClearValue.stencil = 0;
        depthRT.mDepth = 1;
        depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
        depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
        depthRT.mHeight = mSettings.mHeight;
        depthRT.mSampleCount = SAMPLE_COUNT_1;
        depthRT.mSampleQuality = 0;
        depthRT.mWidth = mSettings.mWidth;
        depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

        return pDepthBuffer != NULL;
    }

    static void SkeletonBatchUniformsThreaded(void* pData, uint64_t)
    {
        ThreadSkeletonData* data = (ThreadSkeletonData*)pData;
        gSkeletonBatcher.SetPerInstanceUniforms(data->mFrameNumber, data->mNumberRigs, data->mOffset);
    }

    // Threaded animated object update call
    static void AnimatedObjectThreadedUpdate(void* pData, uint64_t)
    {
        // Unpack data
        ThreadData* data = (ThreadData*)pData;

        AnimatedObject* animSystem = data->mAnimatedObject;
        float           deltaTime = data->mDeltaTime;
        unsigned int    numberSystems = data->mNumberSystems;

        // Update the systems
        for (unsigned int i = 0; i < numberSystems; ++i)
        {
            if (!(animSystem[i].Update(deltaTime)))
                LOGF(eERROR, "Animation NOT Updating!");

            if (gUIData.mIKParams.mAim)
            {
                if (!animSystem[i].AimIK(&gAimIKDesc, gAimTarget))
                    LOGF(eINFO, "Aim IK failed!");
            }

            if (gUIData.mIKParams.mTwoBoneIK)
            {
                Matrix4 mat = animSystem[i].mJointModelMats[gTwoBonesIKDesc.mJointChain[2]];
                Point3  twoBoneTarget = Point3(mat.getCol3()) + Vector3(0.0f, gUIData.mIKParams.mFoot, 0.0f);

                if (!animSystem[i].TwoBonesIK(&gTwoBonesIKDesc, twoBoneTarget))
                    LOGF(eINFO, "Two bone IK failed!");
            }

            // pose rig
            if (!gUIData.mGeneralSettings.mShowBindPose)
            {
                // Pose the rig based on the animated object's updated values
                animSystem[i].ComputePose(animSystem[i].mRootTransform);
            }
            else
            {
                // Ignore the updated values and pose in bind
                animSystem[i].ComputeBindPose(animSystem[i].mRootTransform);
            }
        }
    }
};

DEFINE_APPLICATION_MAIN(Animations)

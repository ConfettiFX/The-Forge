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
 * The Forge - ANIMATION - SKINNING UNIT TEST
 *
 * The purpose of this demo is to show how to use the asset pipeline and how to do GPU skinning.
 *
 *********************************************************************************************************/

#include "Shaders/Shared.h"

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

// Memory
#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

//--------------------------------------------------------------------------------------------
// RENDERING PIPELINE DATA
//--------------------------------------------------------------------------------------------
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

Sampler* pDefaultSampler = NULL;

Buffer* pJointVertexBuffer = NULL;
Buffer* pBoneVertexBuffer = NULL;
int     gNumberOfJointPoints;
int     gNumberOfBonePoints;

Shader*        pPlaneDrawShader = NULL;
Buffer*        pPlaneVertexBuffer = NULL;
Pipeline*      pPlaneDrawPipeline = NULL;
RootSignature* pRootSignature = NULL;

Shader*        pShaderSkinning = NULL;
RootSignature* pRootSignatureSkinning = NULL;
Pipeline*      pPipelineSkinning = NULL;

DescriptorSet* pDescriptorSet = NULL;
DescriptorSet* pDescriptorSetSkinning[2] = { NULL };

struct UniformDataBones
{
    mat4 mBoneMatrix[MAX_NUM_BONES];
};

VertexLayout     gVertexLayoutSkinned = {};
Geometry*        pGeom = NULL;
GeometryData*    pGeomData = NULL;
Buffer*          pUniformBufferBones[gDataBufferCount] = { NULL };
UniformDataBones gUniformDataBones;
Texture*         pTextureDiffuse = NULL;

struct shadow_cap
{
    Vector4 a;
    Vector4 b;
};

static shadow_cap generate_cap(const Vector3& a, const Vector3& b, float r)
{
    Vector3 ab = a - b;
    ab = normalize(ab);

    return shadow_cap{ Vector4(a - (ab * r * 0.5f)), Vector4(b + (ab * r * 0.5f), r) };
}

struct UniformBlockPlane
{
    CameraMatrix mProjectView;
    mat4         mToWorldMat;
    shadow_cap   capsules[MAX_NUM_BONES];
    uint         capsules_count;
};

UniformBlockPlane gUniformDataPlane;

Buffer* pPlaneUniformBuffer[gDataBufferCount] = { NULL };

//--------------------------------------------------------------------------------------------
// CAMERA CONTROLLER & SYSTEMS (File/Log/UI)
//--------------------------------------------------------------------------------------------

ICameraController* pCameraController = NULL;
UIComponent*       pStandaloneControlsGUIWindow = NULL;

FontDrawDesc gFrameTimeDraw;
uint32_t     gFontID = 0;

//--------------------------------------------------------------------------------------------
// ANIMATION DATA
//--------------------------------------------------------------------------------------------

// AnimatedObjects
AnimatedObject gStickFigureAnimObject;

// Animations
Animation gAnimation;

// ClipControllers
ClipController gClipController;

// Clips
Clip gClip;

// Rigs
Rig gStickFigureRig;

// SkeletonBatcher
SkeletonBatcher gSkeletonBatcher;

// Filenames
const char* gStickFigureName = "stormtrooper/skeleton.ozz";
const char* gClipName = "stormtrooper/animations/dance.ozz";
const char* gDiffuseTexture = "Stormtrooper_D.tex";

float* pJointPoints = 0;
float* pBonePoints = 0;

const float gBoneWidthRatio = 0.2f;                // Determines how far along the bone to put the max width [0,1]
const float gJointRadius = gBoneWidthRatio * 0.5f; // set to replicate Ozz skeleton

// Timer to get animationsystem update time
static HiresTimer gAnimationUpdateTimer;
char              gAnimationUpdateText[64] = { 0 };

//--------------------------------------------------------------------------------------------
// UI DATA
//--------------------------------------------------------------------------------------------
struct UIData
{
    struct ClipData
    {
        bool*  mPlay;
        bool*  mLoop;
        float  mAnimationTime; // will get set by clip controller
        float* mPlaybackSpeed;
    };
    ClipData mClip;

    struct GeneralSettingsData
    {
        bool mShowBindPose = false;
        bool mDrawBones = false;
        bool mDrawPlane = true;
        bool mDrawShadows = true;
    };
    GeneralSettingsData mGeneralSettings;
};
UIData gUIData;

// Hard set the controller's time ratio via callback when it is set in the UI
void ClipTimeChangeCallback(void* pUserData)
{
    UNREF_PARAM(pUserData);
    gClipController.SetTimeRatioHard(gUIData.mClip.mAnimationTime);
}

//--------------------------------------------------------------------------------------------
// APP CODE
//--------------------------------------------------------------------------------------------
class Skinning: public IApp
{
public:
    bool Init()
    {
        initHiresTimer(&gAnimationUpdateTimer);
        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_ANIMATIONS, "Animation");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

        /************************************************************************/
        // SETUP ANIMATION STRUCTURES
        /************************************************************************/

        // RIGS
        //
        // Initialize the rig with the path to its ozz file
        gStickFigureRig.Initialize(RD_ANIMATIONS, gStickFigureName);

        // CLIPS
        //
        gClip.Initialize(RD_ANIMATIONS, gClipName, &gStickFigureRig);

        // CLIP CONTROLLERS
        //
        // Initialize with the length of the clip they are controlling and an
        // optional external time to set based on their updating
        gClipController.Initialize(gClip.GetDuration(), &gUIData.mClip.mAnimationTime);

        // ANIMATIONS
        //
        AnimationDesc animationDesc{};
        animationDesc.mRig = &gStickFigureRig;
        animationDesc.mNumLayers = 1;
        animationDesc.mLayerProperties[0].mClip = &gClip;
        animationDesc.mLayerProperties[0].mClipController = &gClipController;

        gAnimation.Initialize(animationDesc);

        // ANIMATED OBJECTS
        //
        gStickFigureAnimObject.Initialize(&gStickFigureRig, &gAnimation);
        gStickFigureAnimObject.ComputeBindPose(gStickFigureAnimObject.mRootTransform);
        gStickFigureAnimObject.ComputeJointScales(gStickFigureAnimObject.mRootTransform);

        // GENERATE VERTEX BUFFERS
        //

        // Generate joint vertex buffer
        generateQuad(&pJointPoints, &gNumberOfJointPoints, gJointRadius);

        // Generate bone vertex buffer
        generateIndexedBonePoints(&pBonePoints, &gNumberOfBonePoints, gBoneWidthRatio, gStickFigureRig.mNumJoints,
                                  &gStickFigureRig.mSkeleton.joint_parents()[0]);

        // WINDOW AND RENDERER SETUP
        //
        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        settings.mD3D11Supported = true;
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

        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        profiler.mWidthUI = mSettings.mWidth;
        profiler.mHeightUI = mSettings.mHeight;
        initProfiler(&profiler);

        gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

        // INITIALIZE SAMPLERS
        //
        SamplerDesc defaultSamplerDesc = {};
        defaultSamplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
        defaultSamplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
        defaultSamplerDesc.mAddressW = ADDRESS_MODE_REPEAT;
        defaultSamplerDesc.mMinFilter = FILTER_LINEAR;
        defaultSamplerDesc.mMagFilter = FILTER_LINEAR;
        defaultSamplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
        addSampler(pRenderer, &defaultSamplerDesc, &pDefaultSampler);

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

        // Generate plane vertex buffer
        float planePoints[] = { -10.0f, 0.0f, -10.0f, 1.0f, 0.0f, 0.0f, -10.0f, 0.0f, 10.0f,  1.0f, 1.0f, 0.0f,
                                10.0f,  0.0f, 10.0f,  1.0f, 1.0f, 1.0f, 10.0f,  0.0f, 10.0f,  1.0f, 1.0f, 1.0f,
                                10.0f,  0.0f, -10.0f, 1.0f, 0.0f, 1.0f, -10.0f, 0.0f, -10.0f, 1.0f, 0.0f, 0.0f };

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

        /************************************************************************/
        // LOAD SKINNED MESH
        /************************************************************************/
        gVertexLayoutSkinned.mBindingCount = 1;
        gVertexLayoutSkinned.mAttribCount = 5;
        gVertexLayoutSkinned.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        gVertexLayoutSkinned.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        gVertexLayoutSkinned.mAttribs[0].mBinding = 0;
        gVertexLayoutSkinned.mAttribs[0].mLocation = 0;
        gVertexLayoutSkinned.mAttribs[0].mOffset = 0;
        gVertexLayoutSkinned.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
        gVertexLayoutSkinned.mAttribs[1].mFormat = TinyImageFormat_R32_UINT;
        gVertexLayoutSkinned.mAttribs[1].mBinding = 0;
        gVertexLayoutSkinned.mAttribs[1].mLocation = 1;
        gVertexLayoutSkinned.mAttribs[1].mOffset = 3 * sizeof(float);
        gVertexLayoutSkinned.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
        gVertexLayoutSkinned.mAttribs[2].mFormat = TinyImageFormat_R32_UINT;
        gVertexLayoutSkinned.mAttribs[2].mBinding = 0;
        gVertexLayoutSkinned.mAttribs[2].mLocation = 2;
        gVertexLayoutSkinned.mAttribs[2].mOffset = 3 * sizeof(float) + sizeof(uint32_t);
        gVertexLayoutSkinned.mAttribs[3].mSemantic = SEMANTIC_WEIGHTS;
        gVertexLayoutSkinned.mAttribs[3].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        gVertexLayoutSkinned.mAttribs[3].mBinding = 0;
        gVertexLayoutSkinned.mAttribs[3].mLocation = 3;
        gVertexLayoutSkinned.mAttribs[3].mOffset = 3 * sizeof(float) + sizeof(uint32_t) * 2;
        gVertexLayoutSkinned.mAttribs[4].mSemantic = SEMANTIC_JOINTS;
        gVertexLayoutSkinned.mAttribs[4].mFormat = TinyImageFormat_R16G16B16A16_UINT;
        gVertexLayoutSkinned.mAttribs[4].mBinding = 0;
        gVertexLayoutSkinned.mAttribs[4].mLocation = 4;
        gVertexLayoutSkinned.mAttribs[4].mOffset = 7 * sizeof(float) + sizeof(uint32_t) * 2;

        GeometryLoadDesc loadDesc = {};
        loadDesc.pFileName = "stormtrooper/riggedMesh.bin";
        loadDesc.pVertexLayout = &gVertexLayoutSkinned;
        loadDesc.ppGeometry = &pGeom;
        loadDesc.ppGeometryData = &pGeomData;
        loadDesc.mFlags = GEOMETRY_LOAD_FLAG_SHADOWED;
        addResource(&loadDesc, NULL);

        BufferLoadDesc boneBufferDesc = {};
        boneBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        boneBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        boneBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ASSERT(MAX_NUM_BONES >= gStickFigureRig.mNumJoints);
        boneBufferDesc.mDesc.mSize = sizeof(mat4) * MAX_NUM_BONES;
        boneBufferDesc.pData = NULL;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            boneBufferDesc.ppBuffer = &pUniformBufferBones[i];
            addResource(&boneBufferDesc, NULL);
        }

        TextureLoadDesc diffuseTextureDesc = {};
        diffuseTextureDesc.pFileName = gDiffuseTexture;
        diffuseTextureDesc.ppTexture = &pTextureDiffuse;
        // Textures representing color should be stored in SRGB or HDR format
        diffuseTextureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
        addResource(&diffuseTextureDesc, NULL);
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
        skeletonRenderDesc.mMaxAnimatedObjects = 1;
        skeletonRenderDesc.mJointMeshType = QuadSphere;
        gSkeletonBatcher.Initialize(skeletonRenderDesc);

        // Add the rig to the list of skeletons to render
        gSkeletonBatcher.AddAnimatedObject(&gStickFigureAnimObject);

        // Add the GUI Panels/Windows

        vec2            UIPosition = { mSettings.mWidth * 0.01f, mSettings.mHeight * 0.15f };
        vec2            UIPanelSize = { 650, 1000 };
        UIComponentDesc guiDesc;
        guiDesc.mStartPosition = UIPosition;
        guiDesc.mStartSize = UIPanelSize;
        guiDesc.mFontID = 0;
        guiDesc.mFontSize = 16.0f;
        uiCreateComponent("Animation", &guiDesc, &pStandaloneControlsGUIWindow);

        // SET gUIData MEMBERS THAT NEED POINTERS TO ANIMATION DATA
        //

        // Clip
        gUIData.mClip.mPlay = &gClipController.mPlay;
        gUIData.mClip.mLoop = &gClipController.mLoop;
        gUIData.mClip.mPlaybackSpeed = &gClipController.mPlaybackSpeed;

        // SET UP GUI BASED ON gUIData STRUCT
        //
        {
            enum
            {
                CLIP_PARAM_SEPARATOR_0,
                CLIP_PARAM_PLAY,
                CLIP_PARAM_SEPARATOR_1,
                CLIP_PARAM_LOOP,
                CLIP_PARAM_SEPARATOR_2,
                CLIP_PARAM_ANIMATION_TIME,
                CLIP_PARAM_SEPARATOR_3,
                CLIP_PARAM_PLAYBACK_SPEED,
                CLIP_PARAM_SEPARATOR_4,

                CLIP_PARAM_COUNT
            };

            enum
            {
                GENERAL_PARAM_SEPARATOR_0,
                GENERAL_PARAM_SHOW_BIND_POS,
                GENERAL_PARAM_SEPARATOR_1,
                GENERAL_PARAM_DRAW_BONES,
                GENERAL_PARAM_SEPARATOR_2,
                GENERAL_PARAM_DRAW_PLANE,
                GENERAL_PARAM_SEPARATOR_3,
                GENERAL_PARAM_DRAW_SHADOWS,

                GENERAL_PARAM_COUNT
            };

            static const uint32_t maxWidgetCount = max((uint32_t)CLIP_PARAM_COUNT, (uint32_t)GENERAL_PARAM_COUNT);

            UIWidget  widgetBases[maxWidgetCount] = {};
            UIWidget* widgets[maxWidgetCount];
            for (uint32_t i = 0; i < maxWidgetCount; ++i)
                widgets[i] = &widgetBases[i];

            // Set all separators
            SeparatorWidget separator;
            for (uint32_t i = 0; i < maxWidgetCount; i += 2)
            {
                widgets[i]->mType = WIDGET_TYPE_SEPARATOR;
                widgets[i]->mLabel[0] = '\0';
                widgets[i]->pWidget = &separator;
            }

            // STAND CLIP
            //
            CollapsingHeaderWidget collapsingClipWidgets;
            collapsingClipWidgets.pGroupedWidgets = widgets;
            collapsingClipWidgets.mWidgetsCount = CLIP_PARAM_COUNT;

            // Play/Pause - Checkbox
            CheckboxWidget playCheckbox = {};
            playCheckbox.pData = gUIData.mClip.mPlay;
            widgets[CLIP_PARAM_PLAY]->mType = WIDGET_TYPE_CHECKBOX;
            strcpy(widgets[CLIP_PARAM_PLAY]->mLabel, "Play");
            widgets[CLIP_PARAM_PLAY]->pWidget = &playCheckbox;

            // Loop - Checkbox
            CheckboxWidget loopCheckbox = {};
            loopCheckbox.pData = gUIData.mClip.mLoop;
            widgets[CLIP_PARAM_LOOP]->mType = WIDGET_TYPE_CHECKBOX;
            strcpy(widgets[CLIP_PARAM_LOOP]->mLabel, "Loop");
            widgets[CLIP_PARAM_LOOP]->pWidget = &loopCheckbox;

            // Animation Time - Slider
            float fValMin = 0.0f;
            float fValMax = gClipController.mDuration;
            float sliderStepSize = 0.01f;

            SliderFloatWidget animationTime;
            animationTime.pData = &gUIData.mClip.mAnimationTime;
            animationTime.mMin = fValMin;
            animationTime.mMax = fValMax;
            animationTime.mStep = sliderStepSize;
            widgets[CLIP_PARAM_ANIMATION_TIME]->mType = WIDGET_TYPE_SLIDER_FLOAT;
            strcpy(widgets[CLIP_PARAM_ANIMATION_TIME]->mLabel, "Animation Time");
            widgets[CLIP_PARAM_ANIMATION_TIME]->pWidget = &animationTime;
            uiSetWidgetOnActiveCallback(widgets[CLIP_PARAM_ANIMATION_TIME], nullptr, ClipTimeChangeCallback);

            // Playback Speed - Slider
            fValMin = -5.0f;
            fValMax = 5.0f;
            sliderStepSize = 0.1f;

            SliderFloatWidget playbackSpeed;
            playbackSpeed.pData = gUIData.mClip.mPlaybackSpeed;
            playbackSpeed.mMin = fValMin;
            playbackSpeed.mMax = fValMax;
            playbackSpeed.mStep = sliderStepSize;
            widgets[CLIP_PARAM_PLAYBACK_SPEED]->mType = WIDGET_TYPE_SLIDER_FLOAT;
            strcpy(widgets[CLIP_PARAM_PLAYBACK_SPEED]->mLabel, "Playback Speed");
            widgets[CLIP_PARAM_PLAYBACK_SPEED]->pWidget = &playbackSpeed;

            luaRegisterWidget(
                uiCreateComponentWidget(pStandaloneControlsGUIWindow, "Clip", &collapsingClipWidgets, WIDGET_TYPE_COLLAPSING_HEADER));

            // GENERAL SETTINGS
            //
            CollapsingHeaderWidget collapsingGeneralSettingsWidgets;
            collapsingGeneralSettingsWidgets.pGroupedWidgets = widgets;
            collapsingGeneralSettingsWidgets.mWidgetsCount = GENERAL_PARAM_COUNT;

            // ShowBindPose - Checkbox
            CheckboxWidget showBindPose;
            showBindPose.pData = &gUIData.mGeneralSettings.mShowBindPose;
            widgets[GENERAL_PARAM_SHOW_BIND_POS]->mType = WIDGET_TYPE_CHECKBOX;
            strcpy(widgets[GENERAL_PARAM_SHOW_BIND_POS]->mLabel, "Show Bind Pose");
            widgets[GENERAL_PARAM_SHOW_BIND_POS]->pWidget = &showBindPose;

            // DrawBones - Checkbox
            CheckboxWidget drawBones;
            drawBones.pData = &gUIData.mGeneralSettings.mDrawBones;
            widgets[GENERAL_PARAM_DRAW_BONES]->mType = WIDGET_TYPE_CHECKBOX;
            strcpy(widgets[GENERAL_PARAM_DRAW_BONES]->mLabel, "DrawBones");
            widgets[GENERAL_PARAM_DRAW_BONES]->pWidget = &drawBones;

            // DrawPlane - Checkbox
            CheckboxWidget drawPlane;
            drawPlane.pData = &gUIData.mGeneralSettings.mDrawPlane;
            widgets[GENERAL_PARAM_DRAW_PLANE]->mType = WIDGET_TYPE_CHECKBOX;
            strcpy(widgets[GENERAL_PARAM_DRAW_PLANE]->mLabel, "Draw Plane");
            widgets[GENERAL_PARAM_DRAW_PLANE]->pWidget = &drawPlane;
            widgets[GENERAL_PARAM_DRAW_PLANE]->pOnActive = NULL; // Clear pointer to &ClipTimeChangeCallback

            // DrawShadows - Checkbox
            CheckboxWidget drawShadows;
            drawShadows.pData = &gUIData.mGeneralSettings.mDrawShadows;
            widgets[GENERAL_PARAM_DRAW_SHADOWS]->mType = WIDGET_TYPE_CHECKBOX;
            strcpy(widgets[GENERAL_PARAM_DRAW_SHADOWS]->mLabel, "Draw Shadows");
            widgets[GENERAL_PARAM_DRAW_SHADOWS]->pWidget = &drawShadows;

            luaRegisterWidget(uiCreateComponentWidget(pStandaloneControlsGUIWindow, "General Settings", &collapsingGeneralSettingsWidgets,
                                                      WIDGET_TYPE_COLLAPSING_HEADER));
        }

        // SETUP THE MAIN CAMERA
        //
        CameraMotionParameters cmp{ 50.0f, 75.0f, 150.0f };
        vec3                   camPos{ -3.0f, 3.0f, 5.0f };
        vec3                   lookAt{ 0.0f, 1.0f, 0.0f };

        pCameraController = initFpsCameraController(camPos, lookAt);
        pCameraController->setMotionParameters(cmp);

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
        tf_free(pBonePoints);
        tf_free(pJointPoints);

        return true;
    }

    void Exit()
    {
        exitInputSystem();

        gStickFigureRig.Exit();
        gClip.Exit();
        gAnimation.Exit();
        gStickFigureAnimObject.Exit();

        exitCameraController(pCameraController);

        exitProfiler();

        exitUserInterface();

        exitFontSystem();

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pPlaneUniformBuffer[i]);
        }

        removeResource(pGeomData);
        pGeomData = nullptr;
        removeResource(pGeom);
        pGeom = nullptr;

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
            removeResource(pUniformBufferBones[i]);
        removeResource(pTextureDiffuse);

        removeResource(pJointVertexBuffer);
        removeResource(pBoneVertexBuffer);
        removeResource(pPlaneVertexBuffer);

        removeSampler(pRenderer, pDefaultSampler);

        removeSemaphore(pRenderer, pImageAcquiredSemaphore);
        removeGpuCmdRing(pRenderer, &gGraphicsCmdRing);

        // Animation data
        gSkeletonBatcher.Exit();

        exitResourceLoaderInterface(pRenderer);
        removeQueue(pRenderer, pGraphicsQueue);
        exitRenderer(pRenderer);
        pRenderer = NULL;
    }

    bool Load(ReloadDesc* pReloadDesc)
    {
        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            addShaders();
            addRootSignatures();
            addDescriptorSets();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            if (!addSwapChain())
                return false;

            if (!addDepthBuffer())
                return false;
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
        fontLoad.mHeight = mSettings.mHeight;
        fontLoad.mWidth = mSettings.mWidth;
        fontLoad.mLoadType = pReloadDesc->mType;
        loadFontSystem(&fontLoad);

        SkeletonBatcherLoadDesc skeletonLoad = {};
        skeletonLoad.mLoadType = pReloadDesc->mType;
        skeletonLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        skeletonLoad.mDepthFormat = pDepthBuffer->mFormat;
        skeletonLoad.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        skeletonLoad.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;

        gSkeletonBatcher.Load(&skeletonLoad);

        initScreenshotInterface(pRenderer, pGraphicsQueue);

        return true;
    }

    void Unload(ReloadDesc* pReloadDesc)
    {
        waitQueueIdle(pGraphicsQueue);

        gSkeletonBatcher.Unload(pReloadDesc->mType);
        unloadFontSystem(pReloadDesc->mType);
        unloadUserInterface(pReloadDesc->mType);

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

    void Update(float deltaTime)
    {
        updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

        pCameraController->update(deltaTime);

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

        // Update the animated object for this frame
        if (!gStickFigureAnimObject.Update(deltaTime))
            LOGF(eINFO, "Animation NOT Updating!");

        if (!gUIData.mGeneralSettings.mShowBindPose)
        {
            // Pose the rig based on the animated object's updated values
            gStickFigureAnimObject.ComputePose(gStickFigureAnimObject.mRootTransform);
        }
        else
        {
            // Ignore the updated values and pose in bind
            gStickFigureAnimObject.ComputeBindPose(gStickFigureAnimObject.mRootTransform);
        }

        // Record animation update time
        getHiresTimerUSec(&gAnimationUpdateTimer, true);

        // Update uniforms that will be shared between all skeletons
        gSkeletonBatcher.SetSharedUniforms(projViewMat, viewMat, lightPos, lightColor);

        for (uint i = 0; i < pGeomData->mJointCount; ++i)
        {
            gUniformDataBones.mBoneMatrix[i] = // mat4::scale(vec3(1, 1, -1)) *
                gStickFigureAnimObject.mJointWorldMats[pGeomData->pJointRemaps[i]] * pGeomData->pInverseBindPoses[i];
        }

        /************************************************************************/
        // Shadow Capsules
        /************************************************************************/

        if (gUIData.mGeneralSettings.mDrawShadows)
        {
            gUniformDataPlane.capsules_count = 8;
            // head
            gUniformDataPlane.capsules[0] = generate_cap(getBonePos("mixamorig:Neck"), getBonePos("mixamorig:HeadTop_End"), 0.2f);

            // spine
            gUniformDataPlane.capsules[1] = generate_cap(getBonePos("mixamorig:Hips"), getBonePos("mixamorig:Spine2"), 0.2f);

            // upleg
            gUniformDataPlane.capsules[2] = generate_cap(getBonePos("mixamorig:LeftUpLeg"), getBonePos("mixamorig:LeftLeg"), 0.15f);
            gUniformDataPlane.capsules[3] = generate_cap(getBonePos("mixamorig:RightUpLeg"), getBonePos("mixamorig:RightLeg"), 0.15f);

            // leg
            gUniformDataPlane.capsules[4] = generate_cap(getBonePos("mixamorig:LeftLeg"), getBonePos("mixamorig:LeftFoot"), 0.1f);
            gUniformDataPlane.capsules[5] = generate_cap(getBonePos("mixamorig:RightLeg"), getBonePos("mixamorig:RightFoot"), 0.1f);
            // foot
            gUniformDataPlane.capsules[6] = generate_cap(getBonePos("mixamorig:LeftFoot"), getBonePos("mixamorig:LeftToeBase"), 0.1f);
            gUniformDataPlane.capsules[7] = generate_cap(getBonePos("mixamorig:RightFoot"), getBonePos("mixamorig:RightToeBase"), 0.1f);

            // looks better without the arms as capsules look weird when they align with light vector
            ////arm
            // gUniformDataPlane.capsules[8] = generate_cap(getBonePos("mixamorig:LeftArm"), getBonePos("mixamorig:LeftForeArm"), 0.08f);
            // gUniformDataPlane.capsules[9] = generate_cap(getBonePos("mixamorig:RightArm"), getBonePos("mixamorig:RightForeArm"), 0.08f);
            ////forearm
            // gUniformDataPlane.capsules[10] = generate_cap(getBonePos("mixamorig:LeftForeArm"), getBonePos("mixamorig:LeftHand"), 0.08f);
            // gUniformDataPlane.capsules[11] = generate_cap(getBonePos("mixamorig:RightForeArm"), getBonePos("mixamorig:RightHand"),
            // 0.08f);
        }
        else
        {
            gUniformDataPlane.capsules_count = 0;
        }

        /************************************************************************/
        // Plane
        /************************************************************************/
        gUniformDataPlane.mProjectView = projViewMat;
        gUniformDataPlane.mToWorldMat = mat4::identity();
    }

    Vector3 getBonePos(uint i) { return gStickFigureAnimObject.mJointWorldMats[i].getCol3().getXYZ(); }

    Vector3 getBonePos(const char* jointName)
    {
        auto jointNames = gStickFigureAnimObject.mAnimation->mRig->mSkeleton.joint_names();

        uint32_t index = UINT32_MAX;
        for (uint32_t i = 0; i < jointNames.size(); ++i)
        {
            if (strcmp(jointName, jointNames[i]) == 0)
            {
                index = i;
                break;
            }
        }

        ASSERT(index != UINT32_MAX);
        return getBonePos(index);
    }

    void Draw()
    {
        if ((bool)pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
        {
            waitQueueIdle(pGraphicsQueue);
            ::toggleVSync(pRenderer, &pSwapChain);
        }

        uint32_t swapchainImageIndex;
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

        // FRAME SYNC & ACQUIRE SWAPCHAIN RENDER TARGET
        //
        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);
        FenceStatus       fenceStatus;
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &elem.pFence);

        resetCmdPool(pRenderer, elem.pCmdPool);

        // UPDATE UNIFORM BUFFERS
        //

        // Update all the instanced uniform data for each batch of joints and bones
        gSkeletonBatcher.PreSetInstanceUniforms(gFrameIndex);
        gSkeletonBatcher.SetPerInstanceUniforms(gFrameIndex);

        BufferUpdateDesc planeViewProjCbv = { pPlaneUniformBuffer[gFrameIndex] };
        beginUpdateResource(&planeViewProjCbv);
        memcpy(planeViewProjCbv.pMappedData, &gUniformDataPlane, sizeof(gUniformDataPlane));
        endUpdateResource(&planeViewProjCbv);

        BufferUpdateDesc boneBufferUpdateDesc = { pUniformBufferBones[gFrameIndex] };
        beginUpdateResource(&boneBufferUpdateDesc);
        memcpy(boneBufferUpdateDesc.pMappedData, &gUniformDataBones, sizeof(mat4) * gStickFigureRig.mNumJoints);
        endUpdateResource(&boneBufferUpdateDesc);

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
        if (gUIData.mGeneralSettings.mDrawPlane)
        {
            const uint32_t stride = sizeof(float) * 6;
            cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Plane");
            cmdBindPipeline(cmd, pPlaneDrawPipeline);
            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSet);
            cmdBindVertexBuffer(cmd, 1, &pPlaneVertexBuffer, &stride, NULL);
            cmdDraw(cmd, 6, 0);
            cmdEndDebugMarker(cmd);
        }

        //// draw the skeleton of the rig
        if (gUIData.mGeneralSettings.mDrawBones)
        {
            cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons");
            gSkeletonBatcher.Draw(cmd, gFrameIndex);
            cmdEndDebugMarker(cmd);
        }

        //// draw skinned mesh
        if (!gUIData.mGeneralSettings.mDrawBones)
        {
            cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skinned Mesh");
            cmdBindPipeline(cmd, pPipelineSkinning);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetSkinning[0]);
            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSkinning[1]);
            cmdBindVertexBuffer(cmd, 1, &pGeom->pVertexBuffers[0], pGeom->mVertexStrides, (uint64_t*)NULL);
            cmdBindIndexBuffer(cmd, pGeom->pIndexBuffer, pGeom->mIndexType, (uint64_t)NULL);
            cmdDrawIndexed(cmd, pGeom->mIndexCount, 0, 0);
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
            cmdDrawTextWithFont(cmd, float2(8.f, txtSize.y + 75.f), &gFrameTimeDraw);
        }

        cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y * 2.f + 100.f), gGpuProfileToken, &gFrameTimeDraw);

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

    const char* GetName() { return "28_Skinning"; }

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
        swapChainDesc.mColorClearValue = { { 0.15f, 0.15f, 0.15f, 1.0f } };
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != NULL;
    }

    void addDescriptorSets()
    {
        DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSet);
        setDesc = { pRootSignatureSkinning, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkinning[0]);
        setDesc = { pRootSignatureSkinning, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkinning[1]);
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorSet);
        removeDescriptorSet(pRenderer, pDescriptorSetSkinning[0]);
        removeDescriptorSet(pRenderer, pDescriptorSetSkinning[1]);
    }

    void addRootSignatures()
    {
        Shader*           shaders[] = { pPlaneDrawShader };
        RootSignatureDesc rootDesc = {};
        rootDesc.mShaderCount = 1;
        rootDesc.ppShaders = shaders;
        addRootSignature(pRenderer, &rootDesc, &pRootSignature);

        const char* staticSamplers[] = { "DefaultSampler" };

        RootSignatureDesc skinningRootSignatureDesc = {};
        skinningRootSignatureDesc.mShaderCount = 1;
        skinningRootSignatureDesc.ppShaders = &pShaderSkinning;
        skinningRootSignatureDesc.mStaticSamplerCount = 1;
        skinningRootSignatureDesc.ppStaticSamplerNames = staticSamplers;
        skinningRootSignatureDesc.ppStaticSamplers = &pDefaultSampler;
        addRootSignature(pRenderer, &skinningRootSignatureDesc, &pRootSignatureSkinning);
    }

    void removeRootSignatures()
    {
        removeRootSignature(pRenderer, pRootSignatureSkinning);
        removeRootSignature(pRenderer, pRootSignature);
    }

    void addShaders()
    {
        ShaderLoadDesc planeShader = {};
        planeShader.mStages[0].pFileName = "plane.vert";
        planeShader.mStages[1].pFileName = "plane.frag";
        ShaderLoadDesc skinningShader = {};
        skinningShader.mStages[0].pFileName = "skinning.vert";
        skinningShader.mStages[1].pFileName = "skinning.frag";

        addShader(pRenderer, &planeShader, &pPlaneDrawShader);
        addShader(pRenderer, &skinningShader, &pShaderSkinning);
    }

    void removeShaders()
    {
        removeShader(pRenderer, pShaderSkinning);
        removeShader(pRenderer, pPlaneDrawShader);
    }

    void addPipelines()
    {
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
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_STRIP;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.pRootSignature = pRootSignature;
        pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
        pipelineSettings.pDepthState = &depthStateDesc;

        // layout and pipeline for plane draw
        VertexLayout vertexLayout = {};
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

        pipelineSettings.pRasterizerState = &rasterizerStateDesc;
        pipelineSettings.pShaderProgram = pPlaneDrawShader;
        pipelineSettings.pVertexLayout = &vertexLayout;
        addPipeline(pRenderer, &desc, &pPlaneDrawPipeline);

        // layout and pipeline for skinning
        pipelineSettings = {};
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.pRootSignature = pRootSignatureSkinning;
        pipelineSettings.pShaderProgram = pShaderSkinning;
        pipelineSettings.pVertexLayout = &gVertexLayoutSkinned;
        pipelineSettings.pRasterizerState = &skeletonRasterizerStateDesc;
        pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
        pipelineSettings.pDepthState = &depthStateDesc;
        addPipeline(pRenderer, &desc, &pPipelineSkinning);
    }

    void removePipelines()
    {
        removePipeline(pRenderer, pPipelineSkinning);
        removePipeline(pRenderer, pPlaneDrawPipeline);
    }

    void prepareDescriptorSets()
    {
        DescriptorData params[1] = {};
        params[0].pName = "DiffuseTexture";
        params[0].ppTextures = &pTextureDiffuse;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetSkinning[0], 1, params);

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData uParams[2] = {};
            uParams[0].pName = "uniformBlock";
            uParams[0].ppBuffers = &pPlaneUniformBuffer[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSet, 1, uParams);

            uParams[0].pName = "uniformBlock";
            uParams[0].ppBuffers = &pPlaneUniformBuffer[i];
            uParams[1].pName = "boneMatrices";
            uParams[1].ppBuffers = &pUniformBufferBones[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetSkinning[1], 2, uParams);
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
};

DEFINE_APPLICATION_MAIN(Skinning)

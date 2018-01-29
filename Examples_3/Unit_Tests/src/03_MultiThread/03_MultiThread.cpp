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

// vim:sw=2 ts=2 et
#define _USE_MATH_DEFINES

//tiny stl
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../Common_3/OS/Interfaces/IThread.h"
#include "../../Common_3/OS/Interfaces/IUIManager.h"
#include "../../Common_3/OS/Interfaces/IApp.h"

#include "../../Common_3/OS/Math/MathTypes.h"
#include "../../Common_3/OS/Image/Image.h"

// for cpu usage query
#ifdef _WIN32
#include <Windows.h>
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "comsuppw.lib")
#else
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#endif

#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/ResourceLoader.h"

//ui
#include "../../Common_3/OS/UI/UI.h"
#include "../../Common_3/OS/UI/UIRenderer.h"

#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

// Need to be done :
// 1. adding Skybox
// 2. get CPU performance and draw the graph
// 3. need to write hlsl code for dx12 version
// 4. change the code structer


// Missing Part for IRende:
// 1. No Blend Mode
// 2. Can't control colorAttachment in render passes , need to have option on keeping previous render result when enter render pass.
// 3. Sampler need more options like repeat....

// startdust hash function, use this to generate all the seed and update the position of all particles
#define RND_GEN(x) (x = x * 196314165 + 907633515)

/// Camera Controller
#define GUI_CAMERACONTROLLER 1
#define FPS_CAMERACONTROLLER 2

#define USE_CAMERACONTROLLER FPS_CAMERACONTROLLER

ICameraController* pCameraController = nullptr;

/// UI
UIRenderer* pUIRenderer = nullptr;
int defaultFont = -1;
int defaultFontJp = -1;

FileSystem gFileSystem;
ThreadPool gThreadSystem;
LogManager gLogManager;
Timer cpuUpdateTimer;

#if defined(DIRECT3D12)
#define RESOURCE_DIR "PCDX12"
#elif defined(VULKAN)
#define RESOURCE_DIR "PCVulkan"
#elif defined(METAL)
#define RESOURCE_DIR "OSXMetal"
#else
#error PLATFORM NOT SUPPORTED
#endif

//Example for using roots or will cause linker error with the extern root in FileSystem.cpp
const char* pszRoots[] =
{
    "../../..//src/03_MultiThread/" RESOURCE_DIR "/Binary/",	// FSR_BinShaders
    "../../..//src/03_MultiThread/" RESOURCE_DIR "/",			// FSR_SrcShaders
    "",															// FSR_BinShaders_Common
    "",															// FSR_SrcShaders_Common
    "../../..//UnitTestResources/Textures/",					// FSR_Textures
    "../../..//UnitTestResources/Meshes/",						// FSR_Meshes
    "../../..//UnitTestResources/Fonts/",						// FSR_Builtin_Fonts
    "",															// FSR_OtherFiles
};

struct ParticleData
{
	float mPaletteFactor;
	uint32_t mData;
	uint32_t mTextureIndex;
};

struct ThreadData
{
  CmdPool* pCmdPool;
  Cmd**    ppCmds;
  int mStartPoint;
  int mDrawCount;
  uint32_t mFrameIndex;
  RenderTarget* pRenderTarget;
};

const uint32_t gImageCount = 3;

Renderer*    pRenderer = nullptr;

Queue*           pGraphicsQueue = nullptr;
CmdPool*         pCmdPool = nullptr;
Cmd**            ppCmds = nullptr;
CmdPool*         pGraphCmdPool = nullptr;
Cmd**            ppGraphCmds = nullptr;

Fence*           pRenderCompleteFences[gImageCount] = { nullptr };
Semaphore*       pImageAcquiredSemaphore = nullptr;
Semaphore*       pRenderCompleteSemaphores[gImageCount] = { nullptr };

SwapChain*       pSwapChain = nullptr;

Shader*          pShader = nullptr;
Shader*          pSkyBoxDrawShader = nullptr;
Shader*          pGraphShader = nullptr;
Buffer*          pParticleVertexBuffer = nullptr;
Buffer*          pProjViewUniformBuffer = nullptr;
Buffer*          pSkyboxUniformBuffer = nullptr;
Buffer*          pSkyBoxVertexBuffer = nullptr;
Buffer*          pBackGroundVertexBuffer[gImageCount] = { nullptr };
Pipeline*        pPipeline = nullptr;
Pipeline*        pSkyBoxDrawPipeline = nullptr;
Pipeline*        pGraphLinePipeline = nullptr;
Pipeline*        pGraphLineListPipeline = nullptr;
Pipeline*        pGraphTrianglePipeline = nullptr;
RootSignature*   pRootSignature = nullptr;
RootSignature*   pGraphRootSignature = nullptr;
Texture*         pTextures[5];
Texture*         pSkyBoxTextures[6];
Sampler*         pSampler = nullptr;
Sampler*         pSamplerSkyBox = nullptr;
uint32_t         gWindowWidth;
uint32_t         gWindowHeight;
uint32_t         gFrameIndex = 0;
uint             gCPUCoreCount = 0;

#ifdef _WIN32
IWbemServices*   pService;
IWbemLocator*    pLocator;
uint64_t*        pOldTimeStamp;
uint64_t*        pOldPprocUsage;
#else
NSLock*          CPUUsageLock;
processor_info_array_t prevCpuInfo;
mach_msg_type_number_t numPrevCpuInfo;
#endif
uint             gCoresCount;
float*           pCoresLoadData;

BlendState* gParticleBlend;
RasterizerState* gSkyboxRast;

const uint       gThreadCount = 3;
const char*      pImageFileNames[] =
{
  "Palette_Fire.png",
  "Palette_Purple.png",
  "Palette_Muted.png",
  "Palette_Rainbow.png",
  "Palette_Sky.png"
};
const char*      pSkyBoxImageFileNames[] =
{
  "Skybox_right1.png",
  "Skybox_left2.png",
  "Skybox_top3.png",
  "Skybox_bottom4.png",
  "Skybox_front5.png",
  "Skybox_back6.png"
};

ThreadData		gThreadData[gThreadCount];
mat4			gProjectView;
ParticleData	gParticleData;
uint32_t		gSeed;
float			gPaletteFactor;
uint			gTextureIndex;

#if !USE_CAMERACONTROLLER
struct CameraProperty
{
  Point3              mCameraPosition;
  vec3                mCameraRight;
  vec3                mCameraDirection;
  vec3                mCameraUp;
  vec3                mCameraForward;
  float                mCameraPitch;
  float                mCamearYaw;
} gCameraProp;
#endif
float                gCameraYRotateScale;   // decide how fast camera rotate

static UIManager* pUIManager = NULL;

struct ObjectProperty
{
  float rotX = 0, rotY = 0;
} gObjSettings;

const float gPi = 3.141592654f;
const int   gTotalParticleCount = 2000000;

const uint gSampleCount = 60;
uint       gGraphWidth = 200;
uint       gGraphHeight = 100;

struct CpuGraphData
{
   int mSampleIdx;
   float mSample[gSampleCount];
   float mSampley[gSampleCount];
   float mScale;
   int mEmptyFlag;
};

struct ViewPortState
{
  float mOffsetX;
  float mOffsetY;
  float mWidth;
  float mHeight;
};

struct GraphVertex
{
  vec2 mPosition;
  vec4 mColor;
};

struct CpuGraph
{
  Buffer* mVertexBuffer[gImageCount]; // vetex buffer for cpu sample
  ViewPortState mViewPort;  //view port for different core
  GraphVertex mPoints[gSampleCount*3];
};

GraphVertex mBackGroundPoints[gImageCount][gSampleCount];
CpuGraphData* pCpuData;
CpuGraph*     pCpuGraph;

WindowsDesc gWindow = {};

/// Camera controller functionality
#if USE_CAMERACONTROLLER
bool cameraMouseMove(const MouseMoveEventData* data)
{
  pCameraController->onMouseMove(data);
  return true;
}

bool cameraMouseButton(const MouseButtonEventData* data)
{
  pCameraController->onMouseButton(data);
  return true;
}

bool cameraMouseWheel(const MouseWheelEventData* data)
{
  pCameraController->onMouseWheel(data);
  return true;
}

void CreateCameraController(const vec3& camPos, const vec3& lookAt, const CameraMotionParameters& motion)
{
#if USE_CAMERACONTROLLER == FPS_CAMERACONTROLLER
  pCameraController = createFpsCameraController(camPos, lookAt);
  requestMouseCapture(true);
#elif USE_CAMERACONTROLLER == GUI_CAMERACONTROLLER
  pCameraController = createGuiCameraController(camPos, lookAt);
#endif

  pCameraController->setMotionParameters(motion);
	
  registerMouseMoveEvent(cameraMouseMove);
  registerMouseButtonEvent(cameraMouseButton);
  registerMouseWheelEvent(cameraMouseWheel);
}

void addSwapChain()
{
	SwapChainDesc swapChainDesc = {};
	swapChainDesc.pWindow = &gWindow;
	swapChainDesc.pQueue = pGraphicsQueue;
	swapChainDesc.mWidth = gWindowWidth;
	swapChainDesc.mHeight = gWindowHeight;
	swapChainDesc.mImageCount = gImageCount;
	swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
	swapChainDesc.mColorFormat = ImageFormat::BGRA8;
	swapChainDesc.mEnableVsync = false;
	addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
}

void RecenterCameraView(float maxDistance, vec3 lookAt=vec3(0))
{
  vec3 p = pCameraController->getViewPosition();
  vec3 d = p - lookAt;

  float lenSqr = lengthSqr(d);
  if (lenSqr > (maxDistance * maxDistance))
  {
    d *= (maxDistance / sqrtf(lenSqr));
  }

  p = d + lookAt;
  pCameraController->moveTo(p);
  pCameraController->lookAt(lookAt);
}
#else
void DestroyCameraController() {}
#endif

// thread for recording particle draw 
void ParticleThreadDraw(void* pData)
{
  ThreadData* data = (ThreadData*)pData;
  Cmd* cmd = data->ppCmds[data->mFrameIndex];
  beginCmd(cmd);
  cmdBeginRender(cmd, 1, &data->pRenderTarget, NULL);
  cmdSetViewport(cmd, 0.0f, 0.0f, (float)data->pRenderTarget->mDesc.mWidth, (float)data->pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
  cmdSetScissor(cmd, 0, 0, data->pRenderTarget->mDesc.mWidth, data->pRenderTarget->mDesc.mHeight);

  cmdBindPipeline(cmd, pPipeline);
  DescriptorData params[3] = {};
  params[0].pName = "uTex0";
  params[0].mCount = sizeof(pImageFileNames) / sizeof(pImageFileNames[0]);
  params[0].ppTextures = pTextures;
  params[1].pName = "uniformBlock";
  params[1].ppBuffers = &pProjViewUniformBuffer;
  params[2].pName = "particleRootConstant";
  params[2].pRootConstant = &gParticleData;
  cmdBindDescriptors(cmd, pRootSignature, 3, params);

  cmdBindVertexBuffer(cmd, 1, &pParticleVertexBuffer);

  //cmdDrawInstanced(cmd, data->mDrawCount, data->mStartPoint , 1);

  for(int i = 0 ; i < (data->mDrawCount/1000) ; ++i)  // in startdust project, they want to show how fast that vulkan can execute draw command, so each thread record a lot of commands
    cmdDrawInstanced(cmd, 1000, data->mStartPoint+(i*1000),1); 
  cmdEndRender(cmd, 1, &data->pRenderTarget, NULL);
  endCmd(cmd);
}

void CalCpuUsage()
{
#ifdef _WIN32
  HRESULT hr = NULL;
  ULONG retVal;
  UINT i;

  IWbemClassObject *pclassObj;
  IEnumWbemClassObject *pEnumerator;

  hr = pService->ExecQuery(bstr_t("WQL"), bstr_t("SELECT TimeStamp_Sys100NS, PercentProcessorTime, Frequency_PerfTime FROM Win32_PerfRawData_PerfOS_Processor"),
    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
  for (i = 0; i < gCoresCount; i++) {
    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclassObj, &retVal);
    if (!retVal) {
      break;
    }

    VARIANT vtPropTime;
    VARIANT vtPropClock;
    VariantInit(&vtPropTime);
    VariantInit(&vtPropClock);

    hr = pclassObj->Get(L"TimeStamp_Sys100NS", 0, &vtPropTime, 0, 0);
    UINT64 newTimeStamp = _wtoi64(vtPropTime.bstrVal);

    hr = pclassObj->Get(L"PercentProcessorTime", 0, &vtPropClock, 0, 0);
    UINT64 newPProcUsage = _wtoi64(vtPropClock.bstrVal);

    pCoresLoadData[i] = (float)(1.0 - (((double)newPProcUsage - (double)pOldPprocUsage[i]) / ((double)newTimeStamp - (double)pOldTimeStamp[i]))) * 100.0f;

    if (pCoresLoadData[i] < 0) pCoresLoadData[i] = 0.0;
    else if (pCoresLoadData[i] > 100.0) pCoresLoadData[i] = 100.0;

    pOldPprocUsage[i] = newPProcUsage;
    pOldTimeStamp[i] = newTimeStamp;

    VariantClear(&vtPropTime);
    VariantClear(&vtPropClock);

    pclassObj->Release();
  }

  pEnumerator->Release();
#else
  processor_info_array_t cpuInfo;
  mach_msg_type_number_t numCpuInfo;
    
  natural_t numCPUsU = 0U;
  kern_return_t err = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &numCPUsU, &cpuInfo, &numCpuInfo);
   
    if (err == KERN_SUCCESS){
        
        [CPUUsageLock lock];
        
        for (uint32_t i = 0; i < gCoresCount; i++) {
            
            float inUse, total;
            
            if (prevCpuInfo)
            {
                inUse = (
                            (cpuInfo[(CPU_STATE_MAX*i)+CPU_STATE_USER] - prevCpuInfo[(CPU_STATE_MAX*i)+CPU_STATE_USER])
                         +  (cpuInfo[(CPU_STATE_MAX*i)+CPU_STATE_SYSTEM] - prevCpuInfo[(CPU_STATE_MAX*i)+CPU_STATE_SYSTEM])
                         +  (cpuInfo[(CPU_STATE_MAX*i)+CPU_STATE_NICE] - prevCpuInfo[(CPU_STATE_MAX*i)+CPU_STATE_NICE])
                         );
                total = inUse + (cpuInfo[(CPU_STATE_MAX*i)+CPU_STATE_IDLE] - prevCpuInfo[(CPU_STATE_MAX*i)+CPU_STATE_IDLE]);
            }
            else
            {
                inUse = cpuInfo[(CPU_STATE_MAX*i)+CPU_STATE_USER] + cpuInfo[(CPU_STATE_MAX*i)+CPU_STATE_SYSTEM] + cpuInfo[(CPU_STATE_MAX*i)+CPU_STATE_NICE];
                total = inUse + cpuInfo[(CPU_STATE_MAX*i)+CPU_STATE_IDLE];
            }
            
            pCoresLoadData[i] = (float(inUse) / float(total)) * 100;
            
             if (pCoresLoadData[i] < 0) pCoresLoadData[i] = 0.0;
             else if (pCoresLoadData[i] > 100.0) pCoresLoadData[i] = 100.0;
        }
     
        [CPUUsageLock unlock];
        
        if (prevCpuInfo){
            size_t prevCpuInfoSize = sizeof(integer_t) * numPrevCpuInfo;
            vm_deallocate(mach_task_self(), (vm_address_t)prevCpuInfo, prevCpuInfoSize);
        }
        
        prevCpuInfo = cpuInfo;
        numPrevCpuInfo = numCpuInfo;
    }
    
#endif
}

int InitCpuUsage()
{
    gCoresCount = 0;
    
#ifdef _WIN32
  IWbemClassObject *pclassObj;
  IEnumWbemClassObject *pEnumerator;
  HRESULT hr;
  ULONG retVal;
    
  pService = NULL;
  pLocator = NULL;
  pOldTimeStamp = NULL;
  pOldPprocUsage = NULL;
  pCoresLoadData = NULL;

  CoInitializeEx(0, COINIT_MULTITHREADED);
  CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);

  hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *)&pLocator);
  if (FAILED(hr))
  {
    return 0;
  }
  hr = pLocator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pService);
  if (FAILED(hr))
  {
    return 0;
  }

  CoSetProxyBlanket(pService, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
    RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

  hr = pService->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_Processor"),
    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
  hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclassObj, &retVal);
  if (retVal) {
    VARIANT vtProp;
    VariantInit(&vtProp);
    hr = pclassObj->Get(L"NumberOfLogicalProcessors", 0, &vtProp, 0, 0);
    gCoresCount = vtProp.uintVal;
    VariantClear(&vtProp);
  }

  pclassObj->Release();
  pEnumerator->Release();
    
    if (gCoresCount)
    {
        pOldTimeStamp = (uint64_t*)conf_malloc(sizeof(uint64_t)*gCoresCount);
        pOldPprocUsage = (uint64_t*)conf_malloc(sizeof(uint64_t)*gCoresCount);
    }
    
#elif defined(__APPLE__)
    processor_info_array_t cpuInfo;
    mach_msg_type_number_t numCpuInfo;
    
    natural_t numCPUsU = 0U;
    kern_return_t err = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &numCPUsU, &cpuInfo, &numCpuInfo);
    
    assert(err == KERN_SUCCESS);
    
    gCoresCount = numCPUsU;
    
    CPUUsageLock = [[NSLock alloc] init];
#endif
    
    pCpuData = (CpuGraphData *)conf_malloc(sizeof(CpuGraphData)*gCoresCount);
    for (uint i = 0; i < gCoresCount; ++i)
    {
        pCpuData[i].mSampleIdx = 0;
        pCpuData[i].mScale = 1.0f;
        for (uint j = 0; j < gSampleCount; ++j)
        {
            pCpuData[i].mSample[j] = 0.0f;
            pCpuData[i].mSampley[j] = 0.0f;
        }
    }
    
    if (gCoresCount)
    {
        pCoresLoadData = (float*)conf_malloc(sizeof(float)*gCoresCount);
        float zeroFloat = 0.0;
        memset(pCoresLoadData, *(int*)&zeroFloat, sizeof(float)*gCoresCount);
    }
    
  CalCpuUsage();
  return 1;
}

void RemoveCpuUsage()
{
  conf_free(pCpuData);
#ifdef _WIN32
  conf_free(pOldTimeStamp);
  conf_free(pOldPprocUsage);
#endif
  conf_free(pCoresLoadData);
}

void CpuGraphBackGroundUpdate(Cmd* cmd, uint32_t frameIdx)
{
  // background data
  mBackGroundPoints[frameIdx][0].mPosition = vec2(-1.0f,-1.0f);
  mBackGroundPoints[frameIdx][0].mColor = vec4(0.0f, 0.0f, 0.0f, 0.3f);
  mBackGroundPoints[frameIdx][1].mPosition = vec2(1.0f, -1.0f);
  mBackGroundPoints[frameIdx][1].mColor = vec4(0.0f, 0.0f, 0.0f, 0.3f);
  mBackGroundPoints[frameIdx][2].mPosition = vec2(-1.0f, 1.0f);
  mBackGroundPoints[frameIdx][2].mColor = vec4(0.0f, 0.0f, 0.0f, 0.3f);
  mBackGroundPoints[frameIdx][3].mPosition = vec2(1.0f, 1.0f);
  mBackGroundPoints[frameIdx][3].mColor = vec4(0.0f, 0.0f, 0.0f, 0.3f);

  const float woff = 2.0f / gGraphWidth;
  const float hoff = 2.0f / gGraphHeight;

  mBackGroundPoints[frameIdx][4].mPosition = vec2(-1.0f+woff, -1.0f+hoff);
  mBackGroundPoints[frameIdx][4].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
  mBackGroundPoints[frameIdx][5].mPosition = vec2(1.0f - woff, -1.0f + hoff);
  mBackGroundPoints[frameIdx][5].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
  mBackGroundPoints[frameIdx][6].mPosition = vec2(1.0f - woff, -1.0f + hoff);
  mBackGroundPoints[frameIdx][6].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
  mBackGroundPoints[frameIdx][7].mPosition = vec2(1.0f - woff, 1.0f - hoff);
  mBackGroundPoints[frameIdx][7].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
  mBackGroundPoints[frameIdx][8].mPosition = vec2(1.0f - woff, 1.0f - hoff);
  mBackGroundPoints[frameIdx][8].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
  mBackGroundPoints[frameIdx][9].mPosition = vec2(-1.0f + woff, 1.0f - hoff);
  mBackGroundPoints[frameIdx][9].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
  mBackGroundPoints[frameIdx][10].mPosition = vec2(-1.0f + woff, 1.0f - hoff);
  mBackGroundPoints[frameIdx][10].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
  mBackGroundPoints[frameIdx][11].mPosition = vec2(-1.0f + woff, -1.0f + hoff);
  mBackGroundPoints[frameIdx][11].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
  
  for (int i = 1; i <= 6; ++i)
  {
    mBackGroundPoints[frameIdx][12 + i*2].mPosition = vec2(-1.0f + i * (2.0f / 6.0f) - 2.0f * ((pCpuData[0].mSampleIdx % (gSampleCount / 6)) / (float)gSampleCount),-1.0f);
    mBackGroundPoints[frameIdx][12 + i*2].mColor = vec4(0.0f, 0.1f, 0.2f, 0.25f);
    mBackGroundPoints[frameIdx][13 + i * 2].mPosition = vec2(-1.0f + i * (2.0f / 6.0f) - 2.0f * ((pCpuData[0].mSampleIdx % (gSampleCount / 6)) / (float)gSampleCount), 1.0f);
    mBackGroundPoints[frameIdx][13 + i * 2].mColor = vec4(0.0f, 0.1f, 0.2f, 0.25f);
  }
  // start from 24

  for (int i = 1; i <= 9; ++i)
  {
    mBackGroundPoints[frameIdx][24 + i * 2].mPosition = vec2(-1.0f, -1.0f + i * (2.0f / 10.0f));
    mBackGroundPoints[frameIdx][24 + i * 2].mColor = vec4(0.0f, 0.1f, 0.2f, 0.25f);
    mBackGroundPoints[frameIdx][25 + i * 2].mPosition = vec2(1.0f, -1.0f + i * (2.0f / 10.0f));
    mBackGroundPoints[frameIdx][25 + i * 2].mColor = vec4(0.0f, 0.1f, 0.2f, 0.25f);
  }
  //start from 42

  mBackGroundPoints[frameIdx][42].mPosition = vec2(-1.0f, -1.0f);
  mBackGroundPoints[frameIdx][42].mColor = vec4(0.85f, 0.9f, 0.0f, 0.25f);
  mBackGroundPoints[frameIdx][43].mPosition = vec2(1.0f, -1.0f);
  mBackGroundPoints[frameIdx][43].mColor = vec4(0.85f, 0.9f, 0.0f, 0.25f);
  mBackGroundPoints[frameIdx][44].mPosition = vec2(-1.0f, 1.0f);
  mBackGroundPoints[frameIdx][44].mColor = vec4(0.85f, 0.9f, 0.0f, 0.25f);
  mBackGroundPoints[frameIdx][45].mPosition = vec2(1.0f, 1.0f);
  mBackGroundPoints[frameIdx][45].mColor = vec4(0.85f, 0.9f, 0.0f, 0.25f);

  mBackGroundPoints[frameIdx][42].mPosition = vec2(-1.0f, -1.0f);
  mBackGroundPoints[frameIdx][42].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);
  mBackGroundPoints[frameIdx][43].mPosition = vec2(1.0f, -1.0f);
  mBackGroundPoints[frameIdx][43].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);
  mBackGroundPoints[frameIdx][44].mPosition = vec2(-1.0f, 1.0f);
  mBackGroundPoints[frameIdx][44].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);
  mBackGroundPoints[frameIdx][45].mPosition = vec2(1.0f, 1.0f);
  mBackGroundPoints[frameIdx][45].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);

  BufferUpdateDesc backgroundVbUpdate = { pBackGroundVertexBuffer[frameIdx], mBackGroundPoints[frameIdx] };
  updateResource(&backgroundVbUpdate, true);
}

void CpuGraphcmdUpdateBuffer(CpuGraphData* graphData, CpuGraph* graph, Cmd* cmd, uint32_t frameIdx)
{
  int index = graphData->mSampleIdx;
  // fill up tri vertex
  for (int i = 0; i < gSampleCount; ++i)
  {
    if (--index < 0)
      index = gSampleCount - 1;
    graph->mPoints[i*2].mPosition = vec2((1.0f - i * (2.0f / gSampleCount)) * 0.999f - 0.02f , -0.97f);
    graph->mPoints[i*2].mColor = vec4(0.0f, 0.85f, 0.0f, 1.0f);
    graph->mPoints[i*2+1].mPosition = vec2((1.0f - i * (2.0f / gSampleCount)) * 0.999f - 0.02f, (2.0f * ((graphData->mSample[index] + graphData->mSampley[index]) * graphData->mScale - 0.5f)) * 0.97f);
    graph->mPoints[i*2+1].mColor = vec4(0.0f, 0.85f, 0.0f, 1.0f);
  }

  //line vertex
  index = graphData->mSampleIdx;
  for(int i = 0 ; i < gSampleCount ; ++i)
  {
    if (--index < 0)
      index = gSampleCount - 1;
    graph->mPoints[i+2*gSampleCount].mPosition = vec2((1.0f - i * (2.0f / gSampleCount)) * 0.999f - 0.02f, (2.0f * ((graphData->mSample[index] + graphData->mSampley[index]) * graphData->mScale - 0.5f)) * 0.97f);
    graph->mPoints[i+2*gSampleCount].mColor = vec4(0.0f, 0.85f, 0.0f, 1.0f);
  }

  BufferUpdateDesc vbUpdate = { graph->mVertexBuffer[frameIdx], graph->mPoints };
  updateResource(&vbUpdate, true);
}

bool load()
{
	addSwapChain();
	return true;
}

void unload()
{
	removeSwapChain(pRenderer, pSwapChain);
}

void initApp(const WindowsDesc* window)
{
  InitCpuUsage();

  int width = window->fullScreen ? getRectWidth(window->fullscreenRect) : getRectWidth(window->windowedRect);
  int height = window->fullScreen ? getRectHeight(window->fullscreenRect) : getRectHeight(window->windowedRect);
  gWindowWidth = (uint32_t)(width);
  gWindowHeight = (uint32_t)(height);

  gGraphWidth = gWindowWidth / 6; //200;
  gGraphHeight = (gWindowHeight - 30 - gCoresCount*10) / gCoresCount;

  RendererDesc settings = { 0 };
 // settings.pLogFn = RendererLog;
  initRenderer("MultiThread", &settings, &pRenderer);

  QueueDesc queueDesc = {};
  queueDesc.mType = CMD_POOL_DIRECT;
  addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
  addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
  addCmd_n(pCmdPool, false, gImageCount, &ppCmds);

  addCmdPool(pRenderer, pGraphicsQueue, false, &pGraphCmdPool);
  addCmd_n(pGraphCmdPool, false, gImageCount, &ppGraphCmds);

 // initial needed datat for each thread 
  for (int i = 0; i < gThreadCount; ++i)
  {
    // create cmd pools and and cmdbuffers for all thread
    addCmdPool(pRenderer, pGraphicsQueue, false, &gThreadData[i].pCmdPool);
    addCmd_n(gThreadData[i].pCmdPool, false, gImageCount, &gThreadData[i].ppCmds);

    // fill up the data for drawing point
    gThreadData[i].mStartPoint = i*(gTotalParticleCount / gThreadCount);
    gThreadData[i].mDrawCount = (gTotalParticleCount / gThreadCount);
  }

  for (uint32_t i = 0; i < gImageCount; ++i)
  {
	  addFence(pRenderer, &pRenderCompleteFences[i]);
	  addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
  }
  addSemaphore(pRenderer, &pImageAcquiredSemaphore);

  addSwapChain();

  HiresTimer timer;
  initResourceLoaderInterface (pRenderer, DEFAULT_MEMORY_BUDGET, true);
    
  // load all image to GPU
  for (int i = 0; i < 5; ++i)
  {
	  TextureLoadDesc textureDesc = {};
	  textureDesc.mRoot = FSR_Textures;
	  textureDesc.mUseMipmaps = true;
	  textureDesc.pFilename = pImageFileNames[i];
	  textureDesc.ppTexture = &pTextures[i];
	  addResource (&textureDesc, true);
  }

  for (int i = 0; i < 6; ++i)
  {
	  TextureLoadDesc textureDesc = {};
	  textureDesc.mRoot = FSR_Textures;
	  textureDesc.mUseMipmaps = true;
	  textureDesc.pFilename = pSkyBoxImageFileNames[i];
	  textureDesc.ppTexture = &pSkyBoxTextures[i];
	  addResource (&textureDesc, true);
  }

  ShaderDesc graphShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };
  ShaderDesc particleShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };
  ShaderDesc skyShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };

#if defined(VULKAN)
  File vertFile = {};
  File fragFile = {};
  vertFile.Open("Graph.vert.spv", FileMode::FM_ReadBinary, FSR_BinShaders);
  fragFile.Open("Graph.frag.spv", FileMode::FM_ReadBinary, FSR_BinShaders);
  graphShader.mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
  graphShader.mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

  vertFile.Open("Skybox.vert.spv", FileMode::FM_ReadBinary, FSR_BinShaders);
  fragFile.Open("Skybox.frag.spv", FileMode::FM_ReadBinary, FSR_BinShaders);
  skyShader.mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
  skyShader.mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

  vertFile.Open("Particle.vert.spv", FileMode::FM_ReadBinary, FSR_BinShaders);
  fragFile.Open("Particle.frag.spv", FileMode::FM_ReadBinary, FSR_BinShaders);
  particleShader.mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
  particleShader.mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

  vertFile.Close();
  fragFile.Close();
#elif defined(DIRECT3D12)
  File hlslFile = {};
  hlslFile.Open("Graph.hlsl", FM_Read, FSRoot::FSR_SrcShaders);
  String hlsl = hlslFile.ReadText();
  graphShader = { graphShader.mStages, { hlslFile.GetName(), hlsl, "VSMain" }, {hlslFile.GetName(), hlsl, "PSMain" } };

  hlslFile.Open("Skybox.hlsl", FM_Read, FSRoot::FSR_SrcShaders);
  hlsl = hlslFile.ReadText();
  skyShader = { skyShader.mStages, { hlslFile.GetName(), hlsl, "VSMain" }, { hlslFile.GetName(), hlsl, "PSMain" } };

  hlslFile.Open("Particle.hlsl", FM_Read, FSRoot::FSR_SrcShaders);
  hlsl = hlslFile.ReadText();
  particleShader = { particleShader.mStages, { hlslFile.GetName(), hlsl, "VSMain" }, { hlslFile.GetName(), hlsl, "PSMain" } };

  hlslFile.Close();
#elif defined(METAL)
    File metalFile = {};
    metalFile.Open("Graph.metal", FM_Read, FSRoot::FSR_SrcShaders);
    String metal = metalFile.ReadText();
    graphShader = { graphShader.mStages, { metalFile.GetName(), metal, "VSMain" }, {metalFile.GetName(), metal, "PSMain" } };
    
    metalFile.Open("Skybox.metal", FM_Read, FSRoot::FSR_SrcShaders);
    metal = metalFile.ReadText();
    skyShader = { skyShader.mStages, { metalFile.GetName(), metal, "VSMain" }, { metalFile.GetName(), metal, "PSMain" } };
    
    metalFile.Open("Particle.metal", FM_Read, FSRoot::FSR_SrcShaders);
    metal = metalFile.ReadText();
    particleShader = { particleShader.mStages, { metalFile.GetName(), metal, "VSMain" }, { metalFile.GetName(), metal, "PSMain" } };
    
    metalFile.Close();
#endif

  addShader(pRenderer, &particleShader, &pShader);
  addShader(pRenderer, &skyShader, &pSkyBoxDrawShader);
  addShader(pRenderer, &graphShader, &pGraphShader);

  addSampler(pRenderer, &pSampler, FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_NEAREST, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT);
  addSampler(pRenderer, &pSamplerSkyBox, FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_NEAREST, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE);

  addBlendState(&gParticleBlend, BC_ONE, BC_ONE, BC_ONE, BC_ONE, BM_ADD, BM_ADD, 15);
  addRasterizerState(&gSkyboxRast, CULL_MODE_NONE);

  RootSignatureDesc skyBoxRootDesc = {};
  skyBoxRootDesc.mStaticSamplers["uSkyboxSampler"] = pSamplerSkyBox;
  skyBoxRootDesc.mStaticSamplers["uSampler0"] = pSampler;
  Shader* shaders[] = { pShader, pSkyBoxDrawShader };
  addRootSignature(pRenderer, 2, shaders, &pRootSignature, &skyBoxRootDesc);

  //vertexlayout and pipeline for particles
  VertexLayout vertexLayout = {};
  vertexLayout.mAttribCount = 1;     
  vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
  vertexLayout.mAttribs[0].mFormat = ImageFormat::R32UI;  
  vertexLayout.mAttribs[0].mBinding = 0;
  vertexLayout.mAttribs[0].mLocation = 0;
  vertexLayout.mAttribs[0].mOffset = 0;

  GraphicsPipelineDesc pipelineSettings = { 0 };
  pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_POINT_LIST;
  pipelineSettings.mRenderTargetCount = 1;
  pipelineSettings.pBlendState = gParticleBlend;
  pipelineSettings.pRasterizerState = gSkyboxRast;
  pipelineSettings.ppRenderTargets = &pSwapChain->ppSwapchainRenderTargets[0];
  pipelineSettings.pRootSignature = pRootSignature;
  pipelineSettings.pShaderProgram = pShader;
  pipelineSettings.pVertexLayout = &vertexLayout;
  addPipeline(pRenderer, &pipelineSettings, &pPipeline);

  gTextureIndex = 0;

#ifdef _WIN32
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  gCPUCoreCount = sysinfo.dwNumberOfProcessors;
#elif defined(__APPLE__)
  gCPUCoreCount = (unsigned int)[[NSProcessInfo processInfo] processorCount];
#endif

  //Generate sky box vertex buffer
  float skyBoxPoints[] = {
    10.0f,  -10.0f, -10.0f,6.0f, // -z
    -10.0f, -10.0f, -10.0f,6.0f,
    -10.0f, 10.0f, -10.0f,6.0f,
    -10.0f, 10.0f, -10.0f,6.0f,
    10.0f,  10.0f, -10.0f,6.0f,
    10.0f,  -10.0f, -10.0f,6.0f,

    -10.0f, -10.0f,  10.0f,2.0f,  //-x
    -10.0f, -10.0f, -10.0f,2.0f,
    -10.0f,  10.0f, -10.0f,2.0f,
    -10.0f,  10.0f, -10.0f,2.0f,
    -10.0f,  10.0f,  10.0f,2.0f,
    -10.0f, -10.0f,  10.0f,2.0f,

    10.0f, -10.0f, -10.0f,1.0f, //+x
    10.0f, -10.0f,  10.0f,1.0f,
    10.0f,  10.0f,  10.0f,1.0f,
    10.0f,  10.0f,  10.0f,1.0f,
    10.0f,  10.0f, -10.0f,1.0f,
    10.0f, -10.0f, -10.0f,1.0f,

    -10.0f, -10.0f,  10.0f,5.0f,  // +z
    -10.0f,  10.0f,  10.0f,5.0f,
    10.0f,  10.0f,  10.0f,5.0f,
    10.0f,  10.0f,  10.0f,5.0f,
    10.0f, -10.0f,  10.0f,5.0f,
    -10.0f, -10.0f,  10.0f,5.0f,
     
    -10.0f,  10.0f, -10.0f, 3.0f,  //+y
    10.0f,  10.0f, -10.0f,3.0f,
    10.0f,  10.0f,  10.0f,3.0f,
    10.0f,  10.0f,  10.0f,3.0f,
    -10.0f,  10.0f,  10.0f,3.0f,
    -10.0f,  10.0f, -10.0f,3.0f,

    10.0f,  -10.0f, 10.0f, 4.0f,  //-y
    10.0f,  -10.0f, -10.0f,4.0f,
    -10.0f,  -10.0f,  -10.0f,4.0f,
    -10.0f,  -10.0f,  -10.0f,4.0f,
    -10.0f,  -10.0f,  10.0f,4.0f,
    10.0f,  -10.0f, 10.0f,4.0f,
  };

  uint64_t skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
  BufferLoadDesc skyboxVbDesc = {};
  skyboxVbDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
  skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
  skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
  skyboxVbDesc.mDesc.mVertexStride = sizeof (float) * 4;
  skyboxVbDesc.pData = skyBoxPoints;
  skyboxVbDesc.ppBuffer = &pSkyBoxVertexBuffer;
  addResource (&skyboxVbDesc);

  //layout and pipeline for skybox draw
  vertexLayout = {};
  vertexLayout.mAttribCount = 1;
  vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
  vertexLayout.mAttribs[0].mFormat = ImageFormat::RGBA32F;
  vertexLayout.mAttribs[0].mBinding = 0;
  vertexLayout.mAttribs[0].mLocation = 0;
  vertexLayout.mAttribs[0].mOffset = 0;

  pipelineSettings = { 0 };
  pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
  pipelineSettings.mRenderTargetCount = 1;
  pipelineSettings.pRasterizerState = gSkyboxRast;
  pipelineSettings.ppRenderTargets = &pSwapChain->ppSwapchainRenderTargets[0];
  pipelineSettings.pRootSignature = pRootSignature;
  pipelineSettings.pShaderProgram = pSkyBoxDrawShader;
  pipelineSettings.pVertexLayout = &vertexLayout;
  addPipeline(pRenderer, &pipelineSettings, &pSkyBoxDrawPipeline);

  /********** layout and pipeline for graph draw*****************/
  vertexLayout = {};
  vertexLayout.mAttribCount = 2;
  vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
  vertexLayout.mAttribs[0].mFormat = (sizeof(GraphVertex)>24 ? ImageFormat::RGBA32F : ImageFormat::RG32F); // Handle the case when padding is added to the struct (yielding 32 bytes instead of 24) on macOS
  vertexLayout.mAttribs[0].mBinding = 0;
  vertexLayout.mAttribs[0].mLocation = 0;
  vertexLayout.mAttribs[0].mOffset = 0;
  vertexLayout.mAttribs[1].mSemantic = SEMANTIC_COLOR;
  vertexLayout.mAttribs[1].mFormat = ImageFormat::RGBA32F;
  vertexLayout.mAttribs[1].mBinding = 0;
  vertexLayout.mAttribs[1].mLocation = 1;
  vertexLayout.mAttribs[1].mOffset = calculateImageFormatStride(vertexLayout.mAttribs[0].mFormat);
  addRootSignature(pRenderer, 1, &pGraphShader, &pGraphRootSignature);

  pipelineSettings = { 0 };
  pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_LINE_STRIP;
  pipelineSettings.mRenderTargetCount = 1;
  pipelineSettings.ppRenderTargets = &pSwapChain->ppSwapchainRenderTargets[0];
  pipelineSettings.pRootSignature = pGraphRootSignature;
  pipelineSettings.pShaderProgram = pGraphShader;
  pipelineSettings.pVertexLayout = &vertexLayout;
  addPipeline(pRenderer, &pipelineSettings, &pGraphLinePipeline);

  pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_STRIP;
  addPipeline(pRenderer, &pipelineSettings, &pGraphTrianglePipeline);

  pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_LINE_LIST;
  addPipeline(pRenderer, &pipelineSettings, &pGraphLineListPipeline);
  /********************************************************************/

  BufferLoadDesc ubDesc = {};
  ubDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
  ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
  ubDesc.mDesc.mSize = sizeof(mat4);
  ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
  ubDesc.pData = NULL;
  ubDesc.ppBuffer = &pProjViewUniformBuffer;
  addResource(&ubDesc);
  ubDesc.ppBuffer = &pSkyboxUniformBuffer;
  addResource(&ubDesc);

  finishResourceLoading ();
  LOGINFOF ("Load Time %lld", timer.GetUSec (false) / 1000);

  // generate partcile data
  unsigned int gSeed = 23232323;
  for (int i = 0; i < 6 * 9; ++i) {
    RND_GEN(gSeed);
  }
  uint32_t* seedArray = NULL;
  seedArray = (uint32_t*)conf_malloc(gTotalParticleCount * sizeof(uint32_t));
  for (int i = 0; i < gTotalParticleCount; ++i)
  {
    RND_GEN(gSeed);
    seedArray[i] = gSeed;
  }
  uint64_t parDataSize = sizeof(uint32_t)* (uint64_t)gTotalParticleCount;
  uint32_t parDataStride = sizeof(uint32_t);

  BufferLoadDesc particleVbDesc = {};
  particleVbDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
  particleVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
  particleVbDesc.mDesc.mSize = parDataSize;
  particleVbDesc.mDesc.mVertexStride = parDataStride;
  particleVbDesc.pData = seedArray;
  particleVbDesc.ppBuffer = &pParticleVertexBuffer;
  addResource (&particleVbDesc);

  conf_free(seedArray);

  gCameraYRotateScale = 0.02f;

#if !USE_CAMERACONTROLLER
  // initial camera property
  gCameraProp.mCameraPitch = -0.785398163f;
  gCameraProp.mCamearYaw = 1.5f*0.785398163f;
  gCameraProp.mCameraPosition = Point3(24.0f, 24.0f, 10.0f);
  gCameraProp.mCameraForward = vec3(0.0f, 0.0f, -1.0f);
  gCameraProp.mCameraUp = vec3(0.0f, 1.0f, 0.0f);
 
  vec3 camRot(gCameraProp.mCameraPitch, gCameraProp.mCamearYaw, 0.0f);
  mat3 trans;
  trans = mat3::rotationZYX(camRot);
  gCameraProp.mCameraDirection = trans* gCameraProp.mCameraForward;
  gCameraProp.mCameraRight = cross(gCameraProp.mCameraDirection, gCameraProp.mCameraUp);
  normalize(gCameraProp.mCameraRight);
#endif

  uint32_t graphDataStride = sizeof(GraphVertex); // vec2(position) + vec4(color)
  uint32_t graphDataSize = sizeof(GraphVertex)*gSampleCount*3; // 2 vertex for tri, 1 vertex for line strip

  //generate vertex buffer for all cores to draw cpu graph and setting up view port for each graph
  pCpuGraph = (CpuGraph *)conf_malloc(sizeof(CpuGraph)*gCoresCount);
  for (uint i = 0; i < gCoresCount; ++i)
  {
    pCpuGraph[i].mViewPort.mOffsetX = gWindowWidth - 10.0f - gGraphWidth;
    pCpuGraph[i].mViewPort.mWidth = (float)gGraphWidth;
    pCpuGraph[i].mViewPort.mOffsetY = 36 + i * (gGraphHeight + 4.0f);
    pCpuGraph[i].mViewPort.mHeight = (float)gGraphHeight;
    // create vertex buffer for each swapchain
    for (uint j = 0; j < gImageCount; ++j)
    {
      BufferLoadDesc vbDesc = {};
      vbDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
      vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
      vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
      vbDesc.mDesc.mSize = graphDataSize;
      vbDesc.mDesc.mVertexStride = graphDataStride;
      vbDesc.pData = NULL;
      vbDesc.ppBuffer = &pCpuGraph[i].mVertexBuffer[j];
      addResource(&vbDesc);
    }
  }
  graphDataSize = sizeof(GraphVertex)*gSampleCount;
  for (uint i = 0; i < gImageCount; ++i)
  {
    BufferLoadDesc vbDesc = {};
    vbDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
    vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
    vbDesc.mDesc.mSize = graphDataSize;
    vbDesc.mDesc.mVertexStride = graphDataStride;
    vbDesc.pData = NULL;
    vbDesc.ppBuffer = &pBackGroundVertexBuffer[i];
    addResource(&vbDesc);
  }

  UISettings uiSettings = {};
  uiSettings.pDefaultFontName = "TitilliumText/TitilliumText-Bold.ttf";
  addUIManagerInterface(pRenderer, &uiSettings, &pUIManager);

  gThreadSystem.CreateThreads(Thread::GetNumCPUCores() - 1);
    
#if USE_CAMERACONTROLLER
    CameraMotionParameters cmp {100.0f, 800.0f, 1000.0f};
    vec3 camPos { 24.0f, 24.0f, 10.0f };
    vec3 lookAt { 0 };
    
    CreateCameraController(camPos, lookAt, cmp);
#endif
}

void ProcessInput(float deltaTime)
{
  const float autoModeTimeoutReset = 3.0f;
  static float autoModeTimeout = 0.0f;
  
#if USE_CAMERACONTROLLER != FPS_CAMERACONTROLLER
  if (getKeyDown(KEY_W) || getKeyDown(KEY_UP))
  {
    gObjSettings.rotX += gCameraYRotateScale;
    autoModeTimeout = autoModeTimeoutReset;
  }
  if (getKeyDown(KEY_A) || getKeyDown(KEY_LEFT))
  {
    gObjSettings.rotY -= gCameraYRotateScale;
    autoModeTimeout = autoModeTimeoutReset;
  }
  if (getKeyDown(KEY_S) || getKeyDown(KEY_DOWN))
  {
    gObjSettings.rotX -= gCameraYRotateScale;
    autoModeTimeout = autoModeTimeoutReset;
  }
  if (getKeyDown(KEY_D) || getKeyDown(KEY_RIGHT))
  {
    gObjSettings.rotY += gCameraYRotateScale;
    autoModeTimeout = autoModeTimeoutReset;
  }
#endif // USE_CAMERACONTROLLER != FPS_CAMERACONTROLLER

#if USE_CAMERACONTROLLER
  if (getKeyDown(KEY_F))
  {
    RecenterCameraView(85.0f);
  }

  pCameraController->update(deltaTime);

#else

  // when not using the camera controller, auto-rotate the object if the user hasn't been manually
  // rotating it recently.
  if (autoModeTimeout > 0.0f)
  {
    autoModeTimeout -= deltaTime;
  }
  else
  {
    gObjSettings.rotY += gCameraYRotateScale * deltaTime * 6.0f;
  }
#endif

  const float k_wrapAround = (float)(M_PI * 2.0);
  if (gObjSettings.rotX > k_wrapAround)
    gObjSettings.rotX -= k_wrapAround;
  if (gObjSettings.rotX < -k_wrapAround)
    gObjSettings.rotX += k_wrapAround;
  if (gObjSettings.rotY > k_wrapAround)
    gObjSettings.rotY -= k_wrapAround;
  if (gObjSettings.rotY < -k_wrapAround)
    gObjSettings.rotY += k_wrapAround;
}

void update(float deltaTime)
{
    ProcessInput(deltaTime);
}

void drawFrame(float deltaTime)
{  
  acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);
  RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];

  uint32_t frameIdx = gFrameIndex;
  Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[frameIdx];
  Fence* pRenderCompleteFence = pRenderCompleteFences[frameIdx];

  // update camera with time 
#if USE_CAMERACONTROLLER
  mat4 modelMat = mat4::rotationX(gObjSettings.rotX) * mat4::rotationY(gObjSettings.rotY);
  mat4 viewMat = pCameraController->getViewMatrix();
#else
  mat4 modelMat = mat4::rotationY(gObjSettings.rotY) * mat4::rotationX(gObjSettings.rotX);
  mat4 viewMat = mat4::lookAt(gCameraProp.mCameraPosition, Point3(gCameraProp.mCameraPosition + gCameraProp.mCameraDirection), gCameraProp.mCameraUp);
#endif
  const float aspectInverse = (float)gWindowHeight / (float)gWindowWidth;
  const float horizontal_fov = gPi / 2.0f;
  mat4 projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 100.0f);
  gProjectView = projMat*viewMat*modelMat;
  // update particle position matrix

  gPaletteFactor += deltaTime * 0.25f;
  if (gPaletteFactor > 1.0f) 
  {
    for (int i = 0; i < 9; ++i) 
    {
       RND_GEN(gSeed);
    }
    gPaletteFactor = 0.0f;

    gTextureIndex = (gTextureIndex + 1) % 5;

 //   gPaletteFactor = 1.0;
  }
  gParticleData.mPaletteFactor = gPaletteFactor * gPaletteFactor  * (3.0f - 2.0f * gPaletteFactor);
  gParticleData.mData = gSeed;
  gParticleData.mTextureIndex = gTextureIndex;

  /*******record command for drawing particles***************/
  WorkItem pWorkGroups[gThreadCount];
 
  for (int i = 0; i < gThreadCount; ++i)
  {
    gThreadData[i].pRenderTarget = pRenderTarget;
    gThreadData[i].mFrameIndex = frameIdx;
    pWorkGroups[i].pData = &gThreadData[i];
    pWorkGroups[i].pFunc = ParticleThreadDraw;
    gThreadSystem.AddWorkItem(&pWorkGroups[i]);
  }
  // simply record the screen cleaning command
    
  LoadActionsDesc loadActions = {};
  loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
  loadActions.mClearColorValues[0] = { 0.0f, 0.0f, 0.0f, 0.0f };

  Cmd* cmd = ppCmds[frameIdx];
  beginCmd(cmd);

  BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer, &gProjectView, 0, 0, sizeof(gProjectView) };
  updateResource(&viewProjCbv);

  viewMat.setTranslation(vec3(0));
  gProjectView = projMat * viewMat;

  BufferUpdateDesc skyboxViewProjCbv = { pSkyboxUniformBuffer, &gProjectView, 0, 0, sizeof(gProjectView) };
  updateResource(&skyboxViewProjCbv);
    
  // update cpu data graph
  if (cpuUpdateTimer.GetMSec(false) > 500)
  {
	  CalCpuUsage();
	  for (uint i = 0; i < gCoresCount; ++i)
	  {
		  pCpuData[i].mSampley[pCpuData[i].mSampleIdx] = 0.0f;
		  pCpuData[i].mSample[pCpuData[i].mSampleIdx] = pCoresLoadData[i] / 100.0f;
		  pCpuData[i].mSampleIdx = (pCpuData[i].mSampleIdx + 1) % gSampleCount;
	  }

	  cpuUpdateTimer.Reset();
  }

  for (uint32_t i = 0; i < gCoresCount; ++i)
  {
	  CpuGraphcmdUpdateBuffer(&pCpuData[i], &pCpuGraph[i], cmd, frameIdx);  // update vertex buffer for each cpugraph
	  CpuGraphBackGroundUpdate(cmd, frameIdx);
  }

  flushResourceUpdates();

  TextureBarrier barrier = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
  cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);
  cmdBeginRender(cmd, 1, &pRenderTarget, NULL, &loadActions);
  cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
  cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);
  //// draw skybox

  DescriptorData params[7] = {};
  params[0].pName = "RightText";
  params[0].ppTextures = &pSkyBoxTextures[0];
  params[1].pName = "LeftText";
  params[1].ppTextures = &pSkyBoxTextures[1];
  params[2].pName = "TopText";
  params[2].ppTextures = &pSkyBoxTextures[2];
  params[3].pName = "BotText";
  params[3].ppTextures = &pSkyBoxTextures[3];
  params[4].pName = "FrontText";
  params[4].ppTextures = &pSkyBoxTextures[4];
  params[5].pName = "BackText";
  params[5].ppTextures = &pSkyBoxTextures[5];
  params[6].pName = "uniformBlock";
  params[6].ppBuffers = &pSkyboxUniformBuffer;
  cmdBindDescriptors(cmd, pRootSignature, 7, params);
  cmdBindPipeline(cmd, pSkyBoxDrawPipeline);

  cmdBindVertexBuffer(cmd, 1, &pSkyBoxVertexBuffer);
  cmdDraw(cmd, 36, 0);

  static HiresTimer timer;
  cmdUIBeginRender(cmd, pUIManager, 1, &pRenderTarget, NULL);
  cmdUIDrawFrameTime(cmd, pUIManager, { 8, 15 }, "CPU ", timer.GetUSec(true) / 1000.0f);
  cmdUIEndRender(cmd, pUIManager);

  cmdEndRender(cmd, 1, &pRenderTarget, NULL);
  endCmd(cmd);

  beginCmd(ppGraphCmds[frameIdx]);
  for (uint i = 0; i < gCPUCoreCount; ++i)
  {
      gGraphWidth = pRenderTarget->mDesc.mWidth / 6;
      gGraphHeight = (pRenderTarget->mDesc.mHeight - 30 - gCoresCount*10) / gCoresCount;
      pCpuGraph[i].mViewPort.mOffsetX = pRenderTarget->mDesc.mWidth - 10.0f - gGraphWidth;
      pCpuGraph[i].mViewPort.mWidth = (float)gGraphWidth;
      pCpuGraph[i].mViewPort.mOffsetY = 36 + i * (gGraphHeight + 4.0f);
      pCpuGraph[i].mViewPort.mHeight = (float)gGraphHeight;
      
    cmdBeginRender(ppGraphCmds[frameIdx], 1, &pRenderTarget, NULL, NULL);
    cmdSetViewport(ppGraphCmds[frameIdx], pCpuGraph[i].mViewPort.mOffsetX, pCpuGraph[i].mViewPort.mOffsetY, pCpuGraph[i].mViewPort.mWidth, pCpuGraph[i].mViewPort.mHeight, 0.0f, 1.0f);
    cmdSetScissor(ppGraphCmds[frameIdx], 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

	cmdBindDescriptors(ppGraphCmds[frameIdx], pGraphRootSignature, 0, NULL);

    cmdBindPipeline(ppGraphCmds[frameIdx], pGraphTrianglePipeline);
    cmdBindVertexBuffer(ppGraphCmds[frameIdx], 1, &pBackGroundVertexBuffer[frameIdx]);
    cmdDraw(ppGraphCmds[frameIdx], 4, 0);

    cmdBindPipeline(ppGraphCmds[frameIdx], pGraphLineListPipeline);
    cmdBindVertexBuffer(ppGraphCmds[frameIdx], 1, &pBackGroundVertexBuffer[frameIdx]);
    cmdDraw(ppGraphCmds[frameIdx], 38, 4);

    cmdBindPipeline(ppGraphCmds[frameIdx], pGraphTrianglePipeline);
    cmdBindVertexBuffer(ppGraphCmds[frameIdx], 1, &(pCpuGraph[i].mVertexBuffer[frameIdx]));
    cmdDraw(ppGraphCmds[frameIdx], 2 * gSampleCount, 0);

    cmdBindPipeline(ppGraphCmds[frameIdx], pGraphLinePipeline);
    cmdBindVertexBuffer(ppGraphCmds[frameIdx], 1, &pCpuGraph[i].mVertexBuffer[frameIdx]);
    cmdDraw(ppGraphCmds[frameIdx], gSampleCount, 2 * gSampleCount);

    cmdEndRender(ppGraphCmds[frameIdx], 1, &pRenderTarget, NULL);
  }

  barrier = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
  cmdResourceBarrier(ppGraphCmds[frameIdx], 0, NULL, 1, &barrier, true);
  endCmd(ppGraphCmds[frameIdx]);
  // wait all particle threads done
  gThreadSystem.Complete(0);

  /***************draw cpu graph*****************************/
  /***************draw cpu graph*****************************/
  // gather all command buffer, it is important to keep the screen clean command at the beginning
  Cmd *allCmds[gThreadCount+2];
  allCmds[0] = cmd;
 
  for (int i = 0; i < gThreadCount; ++i)
  {
    allCmds[i+1] = gThreadData[i].ppCmds[frameIdx];
  }
  allCmds[gThreadCount + 1] = ppGraphCmds[frameIdx];
  // submit all command buffer

  queueSubmit(pGraphicsQueue, gThreadCount+2, allCmds, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
  queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);

  // Stall if CPU is running "Swap Chain Buffer Count - 1" frames ahead of GPU
  Fence* pNextFence = pRenderCompleteFences[(gFrameIndex + 1) % gImageCount];
  FenceStatus fenceStatus;
  getFenceStatus(pNextFence, &fenceStatus);
  if (fenceStatus == FENCE_STATUS_INCOMPLETE)
	  waitForFences(pGraphicsQueue, 1, &pNextFence);
}

void exitApp()
{
	waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex % gImageCount]);

#if USE_CAMERACONTROLLER
	destroyCameraController(pCameraController);
#endif

	removeUIManagerInterface(pRenderer, pUIManager);

	removeResource(pProjViewUniformBuffer);
	removeResource(pSkyboxUniformBuffer);
	removeResource(pParticleVertexBuffer);
	removeResource(pSkyBoxVertexBuffer);

	for (uint i = 0; i < gImageCount; ++i)
		removeResource(pBackGroundVertexBuffer[i]);

	for (uint i = 0; i < gCoresCount; ++i)
	{
		// remove all vertex buffer belongs to graph
		for (uint j = 0; j < gImageCount; ++j)
			removeResource(pCpuGraph[i].mVertexBuffer[j]);
	}

	conf_free(pCpuGraph);

	for (uint i = 0; i < 5; ++i)
		removeResource(pTextures[i]);
	for (uint i = 0; i < 6; ++i)
		removeResource(pSkyBoxTextures[i]);

	removeSampler(pRenderer, pSampler);
	removeSampler(pRenderer, pSamplerSkyBox);

	removeShader(pRenderer, pShader);
	removeShader(pRenderer, pSkyBoxDrawShader);
	removeShader(pRenderer, pGraphShader);
	removePipeline(pRenderer, pPipeline);
	removePipeline(pRenderer, pSkyBoxDrawPipeline);
	removePipeline(pRenderer, pGraphLineListPipeline);
	removePipeline(pRenderer, pGraphLinePipeline);
	removePipeline(pRenderer, pGraphTrianglePipeline);
	removeRootSignature(pRenderer, pRootSignature);
	removeRootSignature(pRenderer, pGraphRootSignature);

	removeBlendState(gParticleBlend);
	removeRasterizerState(gSkyboxRast);

	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		removeFence(pRenderer, pRenderCompleteFences[i]);
		removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
	}
	removeSemaphore(pRenderer, pImageAcquiredSemaphore);

	removeCmd_n(pCmdPool, gImageCount, ppCmds);
	removeCmdPool(pRenderer, pCmdPool);
	removeCmd_n(pGraphCmdPool, gImageCount, ppGraphCmds);
	removeCmdPool(pRenderer, pGraphCmdPool);

	for (int i = 0; i < gThreadCount; ++i)
	{
		removeCmd_n(gThreadData[i].pCmdPool, gImageCount, gThreadData[i].ppCmds);
		removeCmdPool(pRenderer, gThreadData[i].pCmdPool);
	}

	removeSwapChain(pRenderer, pSwapChain);
	removeQueue(pGraphicsQueue);

	removeResourceLoaderInterface(pRenderer);
	removeRenderer(pRenderer);

	RemoveCpuUsage();
}

#ifndef __APPLE__
void onWindowResize(const WindowResizeEventData* pData)
{
	waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences);

	gWindowWidth = getRectWidth(pData->rect);
	gWindowHeight = getRectHeight(pData->rect);
	unload();
	load();
}

int main(int argc, char **argv)
{
	FileSystem::SetCurrentDir(FileSystem::GetProgramDir());

	HiresTimer deltaTimer;

	gWindow.windowedRect = { 0, 0, 1920, 1080 };
	gWindow.fullScreen = false;
	gWindow.maximized = false;
	openWindow(FileSystem::GetFileName(argv[0]), &gWindow);
	initApp(&gWindow);

	registerWindowResizeEvent (onWindowResize);

	while (isRunning())
	{
		float deltaTime = deltaTimer.GetUSec(true) / 1e6f;
		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (deltaTime > 0.15f)
			deltaTime = 0.05f;
		handleMessages();
		update(deltaTime);
		drawFrame(deltaTime);
	}

	exitApp();
	closeWindow(&gWindow);

	return 0;
}
#else

#import "MetalKitApplication.h"

// Timer used in the update function.
Timer deltaTimer;
float retinaScale = 1.0f;

// Metal application implementation.
@implementation MetalKitApplication {}
-(nonnull instancetype) initWithMetalDevice:(nonnull id<MTLDevice>)device
                  renderDestinationProvider:(nonnull id<RenderDestinationProvider>)renderDestinationProvider
                                       view:(nonnull MTKView*)view
                        retinaScalingFactor:(CGFloat)retinaScalingFactor
{
    self = [super init];
    if(self)
    {
        FileSystem::SetCurrentDir(FileSystem::GetProgramDir());
        
        retinaScale = retinaScalingFactor;
        
        RectDesc resolution;
        getRecommendedResolution(&resolution);
        
        gWindow.windowedRect = resolution;
        gWindow.fullscreenRect = resolution;
        gWindow.fullScreen = false;
        gWindow.maximized = false;
        gWindow.handle = (void*)CFBridgingRetain(view);
        
        @autoreleasepool {
            const char * appName = "03_MultiThread";
            openWindow(appName, &gWindow);
            initApp(&gWindow);
        }
    }
    
    return self;
}

- (void)drawRectResized:(CGSize)size
{
    waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences);
    
    gWindowWidth = size.width * retinaScale;
    gWindowHeight = size.height * retinaScale;
    unload();
    load();
}

- (void)update
{
    float deltaTime = deltaTimer.GetMSec(true) / 1000.0f;
    // if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
    if (deltaTime > 0.15f)
        deltaTime = 0.05f;
    
    update(deltaTime);
    drawFrame(deltaTime);
}
@end

#endif

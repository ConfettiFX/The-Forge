/*
 * Copyright (c) 2018 Kostas Anagnostou (https://twitter.com/KostasAAA).
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

#define _USE_MATH_DEFINES

// Unit Test for testing Hybrid Raytracing
// based on https://interplayoflight.wordpress.com/2018/07/04/hybrid-raytraced-shadows-and-reflections/

// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

#include "../../../../Common_3/Utilities/Math/ShaderUtilities.h"
#include "../../../../Common_3/Utilities/RingBuffer.h"

// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"
#include "../../../../Common_3/Utilities/Math/ShaderUtilities.h"

// ui
#include "../../../../Common_3/Application/Interfaces/IUI.h"

// Geometry
#include "../../../../Common_3/Renderer/Interfaces/IVisibilityBuffer.h"

#include "../../../Visibility_Buffer/src/SanMiguel.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

#define NO_FSL_DEFINITIONS
#include "Shaders/FSL/Shader_Defs.h.fsl"

#define MESH_COUNT  1
#define TOTAL_IMGS  84
#define SCENE_SCALE 10.0f

Timer gAccumTimer;

#define FOREACH_SETTING(X)  \
    X(BindlessSupported, 1) \
    X(MSAASampleCount, 2)   \
    X(AddGeometryPassThrough, 0)

#define GENERATE_ENUM(x, y)   x,
#define GENERATE_STRING(x, y) #x,
#define GENERATE_STRUCT(x, y) uint32_t m##x;
#define GENERATE_VALUE(x, y)  y,
#define INIT_STRUCT(s)        s = { FOREACH_SETTING(GENERATE_VALUE) }

typedef enum ESettings
{
    FOREACH_SETTING(GENERATE_ENUM) Count
} ESettings;

const char* gSettingNames[] = { FOREACH_SETTING(GENERATE_STRING) };

// Useful for using names directly instead of subscripting an array
struct ConfigSettings
{
    FOREACH_SETTING(GENERATE_STRUCT)
} gGpuSettings;

struct Triangle
{
    vec4 vertex0;
    vec3 vertex1;
    vec3 vertex2;
    vec3 centroid;
    uint uv0;
    uint uv1;
    uint uv2;
};

struct BVHNode
{
    float3 aabbMin;
    uint   leftFirst;
    float3 aabbMax;
    uint   triCount;
};

struct BVHNodeBBox
{
    float4 MinBounds; // OffsetToNextNode in w component
    float4 MaxBounds;
};

struct BVHLeafBBox
{
    float4 Vertex0;             // OffsetToNextNode in w component
    float4 Vertex1MinusVertex0; // Number of triangles in w component
    float4 Vertex2MinusVertex0; // Material id in we component. High order bit stands for alpha tested or not.
    uint4  VertexUvs;
};

struct Aabb
{
    vec3              bmin = vec3(1e30f), bmax = vec3(-1e30f);
    FORCE_INLINE void grow(vec3 p)
    {
        bmin = min(bmin, p);
        bmax = max(bmax, p);
    }
    FORCE_INLINE void grow(Aabb& b)
    {
        if (b.bmin[0] != 1e30f)
        {
            grow(b.bmin);
            grow(b.bmax);
        }
    }
    FORCE_INLINE float area()
    {
        vec3 e = bmax - bmin; // box extent
        return e[0] * e[1] + e[1] * e[2] + e[2] * e[0];
    }
};

struct Bin
{
    Aabb bounds;
    int  triCount = 0;
};

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

// Define different geometry sets (opaque and alpha tested geometry)
constexpr uint32_t gNumGeomSets = NUM_GEOMETRY_SETS;

/************************************************************************/
// Clear buffers pipeline
/************************************************************************/
Shader*        pShaderClearBuffers = NULL;
Pipeline*      pPipelineClearBuffers = NULL;
RootSignature* pRootSignatureClearBuffers = NULL;
DescriptorSet* pDescriptorSetClearBuffers = NULL;
/************************************************************************/
// Triangle filtering pipeline
/************************************************************************/
Shader*        pShaderTriangleFiltering = NULL;
Pipeline*      pPipelineTriangleFiltering = NULL;
RootSignature* pRootSignatureTriangleFiltering = NULL;
DescriptorSet* pDescriptorSetTriangleFiltering[2] = { NULL };
/************************************************************************/
// VB pass pipeline
/************************************************************************/
// The last shader / pipeline in the array is used to simulate a crash
Shader*        pShaderVBBufferPass[gNumGeomSets + 1] = {};
Pipeline*      pPipelineVBBufferPass[gNumGeomSets + 1] = {};
RootSignature* pRootSignatureVBPass = NULL;
DescriptorSet* pDescriptorSetVBPass[2] = { NULL };

enum Enum
{
    Composite,
    CompositeCrash,
    RaytracedShadows,
    RaytracedShadowsCrash,
    CopyToBackbuffer,
    RenderPassCount
};

Shader* pShader[RenderPassCount];

RootSignature* pRootSignature;
RootSignature* pRootSignatureComp;

CommandSignature* pCmdSignatureVBPass = NULL;

DescriptorSet* pDescriptorSetNonFreq;
DescriptorSet* pDescriptorSetCompNonFreq;
DescriptorSet* pDescriptorSetCompFreq;

Pipeline* pPipeline[RenderPassCount];

Buffer* pCommonUniformBuffer[gDataBufferCount];

Buffer* pBufferMeshTransforms[MESH_COUNT][gDataBufferCount] = { { NULL } };
Buffer* pBufferVBConstants[gDataBufferCount] = { NULL };
Buffer* pBufferCameraUniform[gDataBufferCount] = { NULL };

Buffer* pBufferMeshConstants = NULL;

RenderTarget* pRenderTargetVBPass = { NULL };
RenderTarget* pRenderTargetDepth = NULL;

enum Textures
{
    Texture_RaytracedShadows,
    Texture_Composite,
    TextureCount
};

Texture* pTextures[TextureCount] = { NULL };

PerFrameVBConstants gVBConstants[gDataBufferCount] = {};

struct MeshInfoUniformBlock
{
    CameraMatrix mWorldViewProjMat = CameraMatrix::identity();
};

struct CameraUniform
{
    mat4         mView;
    CameraMatrix mProject;
    CameraMatrix mViewProject;
    mat4         mInvView;
    CameraMatrix mInvProj;
    CameraMatrix mInvViewProject;
    vec4         mCameraPos;
    float        mNear;
    float        mFar;
    float        mFarNearDiff;
    float        mFarNear;
    vec2         mTwoOverRes;
};

struct PerFrameData
{
    // Stores the camera/eye position in object space for cluster culling
    vec3     mEyeObjectSpace[NUM_CULLING_VIEWPORTS] = {};
    uint32_t mDrawCount[gNumGeomSets] = { 0 };
};

struct CommonUniformBuffer
{
    vec4 mRTSize;
    vec4 mLightDir;
};

// Have a uniform for object data
struct UniformObjData
{
    mat4  mWorldMat;
    float mRoughness = 0.04f;
    float mMetallic = 0.0f;
    int   pbrMaterials = -1;
    int32_t : 32; // padding
};

struct PropData
{
    mat4             mWorldMatrix;
    Geometry*        pGeom = NULL;
    GeometryData*    pGeomData = NULL;
    uint32_t         mMeshCount = 0;
    uint32_t         mMaterialCount = 0;
    Buffer*          pConstantBuffer = NULL;
    Texture**        pTextureStorage = NULL;
    VBMeshInstance*  pVBMeshInstances = NULL;
    VBPreFilterStats mVBPreFilterStats[gDataBufferCount] = {};
    MaterialFlags*   mMaterialFlags = NULL;
};

Renderer*  pRenderer = NULL;
Queue*     pGraphicsQueue = NULL;
GpuCmdRing gGraphicsCmdRing = {};

VisibilityBuffer* pVisibilityBuffer = NULL;

Sampler* pSamplerPointClamp = NULL;
Sampler* pSamplerLinearWrap = NULL;
Sampler* pSamplerTrilinearAniso = NULL;
Sampler* pSamplerMiplessLinear = NULL;
Sampler* pSamplerMiplessNear = NULL;

Semaphore* pImageAcquiredSemaphore = NULL;
SwapChain* pSwapChain = NULL;

Buffer* BVHBoundingBoxesBuffer;

// The render passes used in the demo
CommonUniformBuffer gCommonUniformData;
CameraUniform       gCameraUniformData;

uint32_t gFrameIndex = 0;

MeshInfoUniformBlock gMeshInfoUniformData[MESH_COUNT][gDataBufferCount];

UIComponent* pGuiWindow = NULL;

ICameraController* pCameraController = NULL;

float gLightRotationX = 0.0f;
float gLightRotationZ = 0.0f;

PropData SanMiguelProp;

FontDrawDesc gFrameTimeDraw;
uint32_t     gFontID = 0;
ProfileToken gGpuProfileToken;

/// GPU Breadcrumbs
/* Markers to be used to pinpoint which command has caused GPU hang.
 * In this example, four markers get injected into the command list: one before drawing the VB, one after,
 * then one before blitting to the swapchain and one after.
 * Pressing one of the crash buttons will make the application hang at that point.
 * When a crash is detected, the marker buffer is read to understand which of the 2 steps is missing the
 * marker inserted after the rendering operation.
 * Markers aren't perfectly reliable and can be subject to GPU command reordering.
 * Establishing clear dependencies between render passes can help make them more reliable.
 */

// Rendering steps where we insert markers
enum RenderingStep
{
    RENDERING_STEP_DRAW_VB_PASS = 0,
    RENDERING_STEP_RAYTRACE_SHADOWS_PASS,
    RENDERING_STEP_COMPOSITE_PASS,
    RENDERING_STEP_COUNT
};

enum MarkerType
{
    MARKER_TASK_INDEX = 0,
    MARKER_FRAME_INDEX,
    MARKER_COUNT,
};
#define MARKER_OFFSET(type) ((type)*GPU_MARKER_SIZE)

bool bHasCrashed = false;
bool bCrashedSteps[RENDERING_STEP_COUNT] = {};

Buffer*        pMarkerBuffer = {};
const uint32_t gMarkerInitialValue = UINT32_MAX;
const char*    gMarkerNames[] = { "Draw Visibility Buffer", "Raytrace Shadows", "Composite" };
COMPILE_ASSERT(TF_ARRAY_COUNT(gMarkerNames) == RENDERING_STEP_COUNT);

// forward declarations
void subdivide(BVHNode* bvhNode, Triangle* tri, uint* triIdx, uint& nodesUsed, uint nodeIdx);
void updateNodeBounds(BVHNode* bvhNode, Triangle* tri, uint* triIdx, uint nodeIdx);

// based on https://github.com/jbikker/bvh_article
BVHNode* buildBVH(const PropData& prop, uint& nodesUsed, Triangle** triangles, uint** trianglesIdxs)
{
    uint      noofIndices = prop.pGeom->mIndexCount;
    uint32_t* indices = (uint32_t*)prop.pGeomData->pShadow->pIndices;
    float3*   positions = (float3*)prop.pGeomData->pShadow->pAttributes[SEMANTIC_POSITION];
    ASSERT(prop.pGeomData->pShadow->mVertexStrides[SEMANTIC_TEXCOORD0] == sizeof(uint32_t));
    uint32_t* uvs = (uint32_t*)prop.pGeomData->pShadow->pAttributes[SEMANTIC_TEXCOORD0];

    uint numTriangles = noofIndices / 3;

    *triangles = (Triangle*)tf_malloc(numTriangles * sizeof(Triangle));
    *trianglesIdxs = (uint*)tf_malloc(numTriangles * sizeof(uint));

    Triangle* tri = *triangles;
    uint*     triIdx = *trianglesIdxs;

    for (uint j = 0; j < noofIndices; j += 3)
    {
        uint index0 = indices[j + 0];
        uint index1 = indices[j + 1];
        uint index2 = indices[j + 2];

        tri[j / 3].vertex0.setXYZ((prop.mWorldMatrix * vec4(f3Tov3(positions[index0]), 1)).getXYZ());
        tri[j / 3].vertex1 = (prop.mWorldMatrix * vec4(f3Tov3(positions[index1]), 1)).getXYZ();
        tri[j / 3].vertex2 = (prop.mWorldMatrix * vec4(f3Tov3(positions[index2]), 1)).getXYZ();

        tri[j / 3].uv0 = uvs[index0];
        tri[j / 3].uv1 = uvs[index1];
        tri[j / 3].uv2 = uvs[index2];
    }

    COMPILE_ASSERT(sizeof(uint32_t) == sizeof(float));
    for (uint32_t i = 0; i < prop.mMeshCount; ++i)
    {
        uint32_t      startTriangle = (prop.pGeom->pDrawArgs[i].mStartIndex + prop.pGeom->pDrawArgs[i].mVertexOffset) / 3;
        uint32_t      triangleCount = (SanMiguelProp.pGeom->pDrawArgs[i].mIndexCount) / 3;
        MaterialFlags matFlag = prop.mMaterialFlags[i];

        for (uint32_t triangleId = 0; triangleId < triangleCount; triangleId++)
        {
            uint32_t materialId = i;
            if (matFlag & MATERIAL_FLAG_ALPHA_TESTED)
            {
                materialId = materialId | (1 << 31);
            }

            float w;
            memcpy(&w, &materialId, sizeof(uint32_t));
            tri[startTriangle + triangleId].vertex0.setW(w);
        }
    }

    uint rootNodeIdx = 0;

    // create the BVH node pool
    BVHNode* bvhNode = (BVHNode*)tf_calloc_memalign(1, 64, sizeof(BVHNode) * numTriangles * 2);
    // populate triangle index array
    for (uint i = 0; i < numTriangles; i++)
    {
        triIdx[i] = i;
    }
    // calculate triangle centroids for partitioning
    for (uint i = 0; i < numTriangles; i++)
    {
        tri[i].centroid = (tri[i].vertex0.getXYZ() + tri[i].vertex1 + tri[i].vertex2) * (1.0f / 3.0f);
    }
    // assign all triangles to root node
    BVHNode& root = bvhNode[rootNodeIdx];
    root.leftFirst = 0;
    root.triCount = numTriangles;
    updateNodeBounds(bvhNode, tri, triIdx, rootNodeIdx);
    // subdivide recursively
    subdivide(bvhNode, tri, triIdx, nodesUsed, rootNodeIdx);

    return bvhNode;
}

void updateNodeBounds(BVHNode* bvhNode, Triangle* tri, uint* triIdx, uint nodeIdx)
{
    BVHNode& node = bvhNode[nodeIdx];
    vec3     aabbMin = vec3(1e30f);
    vec3     aabbMax = vec3(-1e30f);
    for (uint first = node.leftFirst, i = 0; i < node.triCount; i++)
    {
        uint      leafTriIdx = triIdx[first + i];
        Triangle& leafTri = tri[leafTriIdx];

        aabbMin = min(aabbMin, leafTri.vertex0.getXYZ());
        aabbMin = min(aabbMin, leafTri.vertex1);
        aabbMin = min(aabbMin, leafTri.vertex2);
        aabbMax = max(aabbMax, leafTri.vertex0.getXYZ());
        aabbMax = max(aabbMax, leafTri.vertex1);
        aabbMax = max(aabbMax, leafTri.vertex2);
    }
    node.aabbMin = v3ToF3(aabbMin);
    node.aabbMax = v3ToF3(aabbMax);
}

float findBestSplitPlane(BVHNode& node, Triangle* tri, uint* triIdx, int& axis, float& splitPos)
{
    float bestCost = 1e30f;
    for (int a = 0; a < 3; a++)
    {
        float boundsMin = 1e30f, boundsMax = -1e30f;
        for (uint i = 0; i < node.triCount; i++)
        {
            Triangle& triangle = tri[triIdx[node.leftFirst + i]];

            boundsMin = min(boundsMin, (float)triangle.centroid[a]);
            boundsMax = max(boundsMax, (float)triangle.centroid[a]);
        }
        if (boundsMin == boundsMax)
        {
            continue;
        }
        // populate the bins
        constexpr int BINS = 8;
        Bin           bin[BINS];
        float         scale = BINS / max(0.001f, boundsMax - boundsMin);
        for (uint i = 0; i < node.triCount; i++)
        {
            Triangle& triangle = tri[triIdx[node.leftFirst + i]];
            int       binIdx = min(BINS - 1, (int)(((float)triangle.centroid[a] - boundsMin) * scale));
            bin[binIdx].triCount++;
            bin[binIdx].bounds.grow(triangle.vertex0.getXYZ());
            bin[binIdx].bounds.grow(triangle.vertex1);
            bin[binIdx].bounds.grow(triangle.vertex2);
        }
        // gather data for the 7 planes between the 8 bins
        float leftArea[BINS - 1];
        float rightArea[BINS - 1];
        int   leftCount[BINS - 1];
        int   rightCount[BINS - 1];
        Aabb  leftBox;
        Aabb  rightBox;
        int   leftSum = 0;
        int   rightSum = 0;
        for (int i = 0; i < BINS - 1; i++)
        {
            leftSum += bin[i].triCount;
            leftCount[i] = leftSum;
            leftBox.grow(bin[i].bounds);
            leftArea[i] = leftBox.area();
            rightSum += bin[BINS - 1 - i].triCount;
            rightCount[BINS - 2 - i] = rightSum;
            rightBox.grow(bin[BINS - 1 - i].bounds);
            rightArea[BINS - 2 - i] = rightBox.area();
        }
        // calculate SAH cost for the 7 planes
        scale = (boundsMax - boundsMin) / BINS;
        for (int i = 0; i < BINS - 1; i++)
        {
            float planeCost = leftCount[i] * leftArea[i] + rightCount[i] * rightArea[i];
            if (planeCost < bestCost)
            {
                axis = a;
                splitPos = boundsMin + scale * (i + 1);
                bestCost = planeCost;
            }
        }
    }
    return bestCost;
}

float calculateNodeCost(BVHNode& node)
{
    float3 e = node.aabbMax - node.aabbMin; // extent of the node
    float  surfaceArea = e[0] * e[1] + e[1] * e[2] + e[2] * e[0];
    return node.triCount * surfaceArea;
}

void subdivide(BVHNode* bvhNode, Triangle* tri, uint* triIdx, uint& nodesUsed, uint nodeIdx)
{
    // terminate recursion
    BVHNode& node = bvhNode[nodeIdx];
    // determine split axis using SAH
    int      axis;
    float    splitPos;
    float    splitCost = findBestSplitPlane(node, tri, triIdx, axis, splitPos);
    float    nosplitCost = calculateNodeCost(node);

    if (splitCost >= nosplitCost)
        return;
    // in-place partition
    uint i = node.leftFirst;
    uint j = i + node.triCount - 1;
    while (i <= j)
    {
        if (tri[triIdx[i]].centroid[axis] < splitPos)
        {
            i++;
        }
        else
        {
            std::swap(triIdx[i], triIdx[j--]);
        }
    }
    // abort split if one of the sides is empty
    uint leftCount = i - node.leftFirst;
    if (leftCount == 0 || leftCount == node.triCount)
    {
        return;
    }
    // create child nodes
    int leftChildIdx = nodesUsed++;
    int rightChildIdx = nodesUsed++;
    bvhNode[leftChildIdx].leftFirst = node.leftFirst;
    bvhNode[leftChildIdx].triCount = leftCount;
    bvhNode[rightChildIdx].leftFirst = i;
    bvhNode[rightChildIdx].triCount = node.triCount - leftCount;
    node.leftFirst = leftChildIdx;
    node.triCount = 0;
    updateNodeBounds(bvhNode, tri, triIdx, leftChildIdx);
    updateNodeBounds(bvhNode, tri, triIdx, rightChildIdx);
    // recurse
    subdivide(bvhNode, tri, triIdx, nodesUsed, leftChildIdx);
    subdivide(bvhNode, tri, triIdx, nodesUsed, rightChildIdx);
}

void writeBVHTree(BVHNode* root, Triangle** triangles, uint** trianglesIdxs, uint nodeIdx, uint8_t* bboxData, int& dataOffset)
{
    Triangle* tri = *triangles;
    uint*     triIdx = *trianglesIdxs;

    BVHNode& node = root[nodeIdx];
    if (node.triCount <= 0)
    {
        int          tempdataOffset = 0;
        BVHNodeBBox* bbox = NULL;

        // do not write the root node, for secondary rays the origin will always be in the scene bounding box
        if (nodeIdx != 0)
        {
            // this is an intermediate node, write bounding box
            bbox = (BVHNodeBBox*)(bboxData + dataOffset);

            bbox->MinBounds = float4(node.aabbMin, 0.0f);
            bbox->MaxBounds = float4(node.aabbMax, 0.0f);

            dataOffset += sizeof(BVHNodeBBox);

            tempdataOffset = dataOffset;
        }

        writeBVHTree(root, triangles, trianglesIdxs, node.leftFirst, bboxData, dataOffset);
        writeBVHTree(root, triangles, trianglesIdxs, node.leftFirst + 1, bboxData, dataOffset);

        if (nodeIdx != 0)
        {
            // when on the left branch, how many float4 elements we need to skip to reach the right branch?
            bbox->MinBounds.w = -(float)(dataOffset - tempdataOffset) / sizeof(float4); //-V522
        }
    }
    else
    {
        // leaf node, write triangle vertices
        for (uint i = 0; i < node.triCount; i++)
        {
            BVHLeafBBox* bbox = (BVHLeafBBox*)(bboxData + dataOffset);

            vec4 vertex0 = tri[triIdx[node.leftFirst + i]].vertex0;

            bbox->Vertex0 = float4(v3ToF3(vertex0.getXYZ()), 0.0f);
            bbox->Vertex1MinusVertex0 = float4(v3ToF3(tri[triIdx[node.leftFirst + i]].vertex1 - vertex0.getXYZ()), float(node.triCount));
            bbox->Vertex2MinusVertex0 = float4(v3ToF3(tri[triIdx[node.leftFirst + i]].vertex2 - vertex0.getXYZ()), vertex0.getW());
            bbox->VertexUvs =
                uint4(tri[triIdx[node.leftFirst + i]].uv0, tri[triIdx[node.leftFirst + i]].uv1, tri[triIdx[node.leftFirst + i]].uv2, 0);

            // when on the left branch, how many float4 elements we need to skip to reach the right branch?
            bbox->Vertex0.w = float(sizeof(BVHLeafBBox) / sizeof(float4) * node.triCount);

            dataOffset += sizeof(BVHLeafBBox);
        }
    }
}

class HybridRaytracing: public IApp
{
public:
    bool Init()
    {
        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

        INIT_STRUCT(gGpuSettings);

        ExtendedSettings extendedSettings = {};
        extendedSettings.mNumSettings = ESettings::Count;
        extendedSettings.pSettings = (uint32_t*)&gGpuSettings;
        extendedSettings.ppSettingNames = gSettingNames;

        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        settings.pExtendedSettings = &extendedSettings;
        initRenderer(GetName(), &settings, &pRenderer);

        // check for init success
        if (!pRenderer)
            return false;

        if (!gGpuSettings.mBindlessSupported)
        {
            ShowUnsupportedMessage("Visibility Buffer does not run on this device. GPU does not support enough bindless texture entries");
            return false;
        }

        if (!pRenderer->pGpu->mSettings.mPrimitiveIdSupported)
        {
            ShowUnsupportedMessage("Visibility Buffer does not run on this device. PrimitiveID is not supported");
            return false;
        }

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

        // Create sampler state objects
        { // point sampling with clamping
          { SamplerDesc samplerDesc = { FILTER_NEAREST, FILTER_NEAREST, MIPMAP_MODE_NEAREST, ADDRESS_MODE_CLAMP_TO_EDGE,
                                        ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE };
        addSampler(pRenderer, &samplerDesc, &pSamplerPointClamp);
    }

    // linear sampling with wrapping
    {
        SamplerDesc samplerDesc = { FILTER_LINEAR,       FILTER_LINEAR,       MIPMAP_MODE_LINEAR,
                                    ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
        addSampler(pRenderer, &samplerDesc, &pSamplerLinearWrap);

        // trilinear anisotropic sampling
        samplerDesc.mMipLodBias = 0.0f;
        samplerDesc.mSetLodRange = false;
        samplerDesc.mMinLod = 0.0f;
        samplerDesc.mMaxLod = 0.0f;
        samplerDesc.mMaxAnisotropy = 8.0f;
        addSampler(pRenderer, &samplerDesc, &pSamplerTrilinearAniso);
    }

    // linear mipless sampling with clamping
    {
        SamplerDesc samplerDesc = { FILTER_LINEAR,
                                    FILTER_LINEAR,
                                    MIPMAP_MODE_LINEAR,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE };
        samplerDesc.mMipLodBias = 0.f;
        samplerDesc.mMaxAnisotropy = 0.f;
        addSampler(pRenderer, &samplerDesc, &pSamplerMiplessLinear);
    }

    // nearest mipless sampling with clamping
    {
        SamplerDesc miplessNearSamplerDesc = {};
        miplessNearSamplerDesc.mMinFilter = FILTER_NEAREST;
        miplessNearSamplerDesc.mMagFilter = FILTER_NEAREST;
        miplessNearSamplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
        miplessNearSamplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
        miplessNearSamplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
        miplessNearSamplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
        miplessNearSamplerDesc.mMipLodBias = 0.f;
        miplessNearSamplerDesc.mMaxAnisotropy = 0.f;
        addSampler(pRenderer, &miplessNearSamplerDesc, &pSamplerMiplessNear);
    }
}

// Create Constant buffers
{
    BufferLoadDesc camUniDesc = {};
    camUniDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    camUniDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    camUniDesc.mDesc.mSize = sizeof(CameraUniform);
    camUniDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    camUniDesc.pData = &gCameraUniformData;
    for (uint32_t i = 0; i < gDataBufferCount; ++i)
    {
        camUniDesc.ppBuffer = &pBufferCameraUniform[i];
        addResource(&camUniDesc, NULL);
    }

    // Vis buffer per-frame constant buffer
    {
        BufferLoadDesc vbConstantUBDesc = {};
        vbConstantUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vbConstantUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        vbConstantUBDesc.mDesc.mSize = sizeof(PerFrameVBConstants);
        vbConstantUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        vbConstantUBDesc.mDesc.pName = "PerFrameVBConstants Buffer Desc";
        vbConstantUBDesc.pData = NULL;

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            vbConstantUBDesc.ppBuffer = &pBufferVBConstants[i];
            addResource(&vbConstantUBDesc, NULL);
        }
    }

    // Mesh transform constant buffer
    BufferLoadDesc ubDesc = {};
    ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ubDesc.mDesc.mSize = sizeof(MeshInfoUniformBlock);
    ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    ubDesc.pData = NULL;

    for (uint32_t j = 0; j < MESH_COUNT; ++j)
    {
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDesc.ppBuffer = &pBufferMeshTransforms[j][i];
            addResource(&ubDesc, NULL);
        }
    }

    // Shadow & Composite pass per-pass constant buffer
    {
        BufferLoadDesc ubDesc = {};
        ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDesc.mDesc.mSize = sizeof(CommonUniformBuffer);
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.pData = NULL;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDesc.ppBuffer = &pCommonUniformBuffer[i];
            addResource(&ubDesc, NULL);
        }
    }

    // Initialize breadcrumb buffer to write markers in it.
    if (pRenderer->pGpu->mSettings.mGpuMarkers)
    {
        initMarkers();
    }
}

UIComponentDesc guiDesc = {};
guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
;
uiCreateComponent(GetName(), &guiDesc, &pGuiWindow);

if (pRenderer->pGpu->mSettings.mGpuMarkers)
{
    static uint32_t renderingStepIndices[RENDERING_STEP_COUNT];
    for (uint32_t i = 0; i < RENDERING_STEP_COUNT; i++)
    {
        renderingStepIndices[i] = i;

        char label[MAX_LABEL_STR_LENGTH];
        snprintf(label, MAX_LABEL_STR_LENGTH, "Simulate crash (%s)", gMarkerNames[i]);
        ButtonWidget   crashButton;
        UIWidget*      pCrashButton = uiCreateComponentWidget(pGuiWindow, label, &crashButton, WIDGET_TYPE_BUTTON);
        WidgetCallback crashCallback = [](void* pUserData) { bCrashedSteps[*(uint32_t*)pUserData] = true; };
        uiSetWidgetOnEditedCallback(pCrashButton, &renderingStepIndices[i], crashCallback);
        REGISTER_LUA_WIDGET(pCrashButton);
    }
}

SliderFloatWidget lightRotXSlider;
lightRotXSlider.pData = &gLightRotationX;
lightRotXSlider.mMin = (float)-M_PI;
lightRotXSlider.mMax = (float)M_PI;
luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Light Rotation X", &lightRotXSlider, WIDGET_TYPE_SLIDER_FLOAT));

SliderFloatWidget lightRotZSlider;
lightRotZSlider.pData = &gLightRotationZ;
lightRotZSlider.mMin = (float)-M_PI;
lightRotZSlider.mMax = (float)M_PI;
luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Light Rotation Z", &lightRotZSlider, WIDGET_TYPE_SLIDER_FLOAT));

if (!LoadSanMiguel())
    return false;

CreateBVHBuffers();
tf_free(SanMiguelProp.mMaterialFlags);

const CameraMotionParameters cmp{ 200.0f, 250.0f, 300.0f };
// make the camera point towards the centre of the scene;
vec3                         camPos{ 80.0f, 60.0f, 50.0f };
vec3                         lookAt{ 1.0f, 0.5f, 0.0f };

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
                   requestShutdown();
                   return true;
               } };
addInputAction(&actionDesc);
InputActionCallback onAnyInput = [](InputActionContext* ctx)
{
    if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
    {
        uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
    }

    return true;
};

typedef bool (*CameraInputHandler)(InputActionContext* ctx, DefaultInputActions::DefaultInputAction action);
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
                   if (!uiWantTextInput())
                       pCameraController->resetView();
                   return true;
               } };
addInputAction(&actionDesc);
GlobalInputActionDesc globalInputActionDesc = { GlobalInputActionDesc::ANY_BUTTON_ACTION, onAnyInput, this };
setGlobalInputAction(&globalInputActionDesc);

gFrameIndex = 0;

return true;
}

void Exit()
{
    exitInputSystem();
    exitCameraController(pCameraController);

    exitProfiler();

    if (pRenderer->pGpu->mSettings.mGpuMarkers)
    {
        exitMarkers();
    }

    exitUserInterface();

    exitFontSystem();

    removeSampler(pRenderer, pSamplerLinearWrap);
    removeSampler(pRenderer, pSamplerPointClamp);
    removeSampler(pRenderer, pSamplerTrilinearAniso);
    removeSampler(pRenderer, pSamplerMiplessLinear);
    removeSampler(pRenderer, pSamplerMiplessNear);

    for (uint32_t i = 0; i < gDataBufferCount; ++i)
    {
        removeResource(pBufferCameraUniform[i]);
        removeResource(pCommonUniformBuffer[i]);

        removeResource(pBufferVBConstants[i]);

        for (int32_t k = 0; k < MESH_COUNT; ++k)
        {
            removeResource(pBufferMeshTransforms[k][i]);
        }
    }

    removeResource(pBufferMeshConstants);

    tf_free(SanMiguelProp.pVBMeshInstances);

    removeSemaphore(pRenderer, pImageAcquiredSemaphore);
    removeGpuCmdRing(pRenderer, &gGraphicsCmdRing);

    // Delete San Miguel resources
    removeResource(SanMiguelProp.pGeom);
    removeResource(SanMiguelProp.pGeomData);
    removeResource(SanMiguelProp.pConstantBuffer);

    for (uint i = 0; i < SanMiguelProp.mMaterialCount; ++i)
    {
        removeResource(SanMiguelProp.pTextureStorage[i]);
    }
    tf_free(SanMiguelProp.pTextureStorage);

    removeResource(BVHBoundingBoxesBuffer);

    exitVisibilityBuffer(pVisibilityBuffer);

    exitResourceLoaderInterface(pRenderer);
    removeQueue(pRenderer, pGraphicsQueue);
    exitRenderer(pRenderer);
    pRenderer = NULL;
}

// Loads san miguel meshand textures
bool LoadSanMiguel()
{
    uint32_t visibilityBufferFilteredIndexCount[NUM_GEOMETRY_SETS] = {};

    SyncToken        token = {};
    GeometryLoadDesc loadDesc = {};
    loadDesc.mFlags = GEOMETRY_LOAD_FLAG_SHADOWED;
    Scene* pScene = loadSanMiguel(&loadDesc, token, false);

    SanMiguelProp.mMeshCount = pScene->geom->mDrawArgCount;
    SanMiguelProp.mMaterialCount = pScene->geom->mDrawArgCount;
    SanMiguelProp.pGeom = pScene->geom;
    SanMiguelProp.pGeomData = pScene->geomData;
    SanMiguelProp.mWorldMatrix = mat4::scale({ SCENE_SCALE, SCENE_SCALE, SCENE_SCALE }) * mat4::identity();
    SanMiguelProp.pTextureStorage = (Texture**)tf_malloc(sizeof(Texture*) * SanMiguelProp.mMaterialCount);

    for (uint32_t i = 0; i < SanMiguelProp.mMaterialCount; ++i)
    {
        TextureLoadDesc desc = {};
        desc.pFileName = pScene->textures[i];
        desc.ppTexture = &SanMiguelProp.pTextureStorage[i];
        // Textures representing color should be stored in SRGB or HDR format
        desc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
        addResource(&desc, NULL);
    }

    // set constant buffer for San Miguel
    {
        static UniformObjData data = {};
        data.mWorldMat = SanMiguelProp.mWorldMatrix;

        BufferLoadDesc desc = {};
        desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        desc.mDesc.mSize = sizeof(UniformObjData);
        desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        desc.pData = &data;
        desc.ppBuffer = &SanMiguelProp.pConstantBuffer;
        addResource(&desc, NULL);
    }

    // Calculate mesh constants and filter containers
    SanMiguelProp.mMaterialFlags = (MaterialFlags*)tf_calloc(SanMiguelProp.mMeshCount, sizeof(MaterialFlags));
    SanMiguelProp.pVBMeshInstances = (VBMeshInstance*)tf_calloc(SanMiguelProp.mMeshCount, sizeof(VBMeshInstance));
    MeshConstants* meshConstants = (MeshConstants*)tf_malloc(SanMiguelProp.mMeshCount * sizeof(MeshConstants));

    for (uint32_t i = 0; i < SanMiguelProp.mMeshCount; ++i)
    {
        MaterialFlags materialFlag = pScene->materialFlags[i];
        SanMiguelProp.mMaterialFlags[i] = materialFlag;
        uint32_t geomSet = materialFlag & MATERIAL_FLAG_ALPHA_TESTED ? GEOMSET_ALPHA_CUTOUT : GEOMSET_OPAQUE;
        visibilityBufferFilteredIndexCount[geomSet] += (pScene->geom->pDrawArgs + i)->mIndexCount;

        SanMiguelProp.pVBMeshInstances[i].mGeometrySet = geomSet;
        SanMiguelProp.pVBMeshInstances[i].mMeshIndex = i;
        SanMiguelProp.pVBMeshInstances[i].mTriangleCount = (pScene->geom->pDrawArgs + i)->mIndexCount / 3;
        SanMiguelProp.pVBMeshInstances[i].mInstanceIndex = INSTANCE_INDEX_NONE;

        meshConstants[i].indexOffset = SanMiguelProp.pGeom->pDrawArgs[i].mStartIndex;
        meshConstants[i].vertexOffset = SanMiguelProp.pGeom->pDrawArgs[i].mVertexOffset;
        meshConstants[i].materialID = i;
        meshConstants[i].twoSided = (pScene->materialFlags[i] & MATERIAL_FLAG_TWO_SIDED) ? 1 : 0;
    }

    // Init visibility buffer
    VisibilityBufferDesc vbDesc = {};
    vbDesc.mNumFrames = gDataBufferCount;
    vbDesc.mNumBuffers = 1; // We don't use Async Compute for triangle filtering, 1 buffer is enough
    vbDesc.mNumGeometrySets = NUM_GEOMETRY_SETS;
    vbDesc.pMaxIndexCountPerGeomSet = visibilityBufferFilteredIndexCount;
    vbDesc.mNumViews = NUM_CULLING_VIEWPORTS;
    vbDesc.mComputeThreads = VB_COMPUTE_THREADS;
    initVisibilityBuffer(pRenderer, &vbDesc, &pVisibilityBuffer);

    BufferLoadDesc meshConstantDesc = {};
    meshConstantDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
    meshConstantDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    meshConstantDesc.mDesc.mElementCount = SanMiguelProp.mMeshCount;
    meshConstantDesc.mDesc.mStructStride = sizeof(MeshConstants);
    meshConstantDesc.mDesc.mSize = meshConstantDesc.mDesc.mElementCount * meshConstantDesc.mDesc.mStructStride;
    meshConstantDesc.ppBuffer = &pBufferMeshConstants;
    meshConstantDesc.pData = meshConstants;
    meshConstantDesc.mDesc.pName = "Mesh Constant desc";
    addResource(&meshConstantDesc, NULL);

    UpdateVBMeshFilterGroupsDesc updateVBMeshFilterGroupsDesc = {};
    updateVBMeshFilterGroupsDesc.mNumMeshInstance = SanMiguelProp.mMeshCount;
    updateVBMeshFilterGroupsDesc.pVBMeshInstances = SanMiguelProp.pVBMeshInstances;
    for (uint32_t i = 0; i < gDataBufferCount; ++i)
    {
        updateVBMeshFilterGroupsDesc.mFrameIndex = i;
        SanMiguelProp.mVBPreFilterStats[i] = updateVBMeshFilterGroups(pVisibilityBuffer, &updateVBMeshFilterGroupsDesc);
    }

    waitForAllResourceLoads();
    unloadSanMiguel(pScene);
    tf_free(meshConstants);

    return true;
}

void CreateBVHBuffers()
{
    HiresTimer bvhTimer;
    initHiresTimer(&bvhTimer);

    // create buffers for BVH
    uint      nodesUsed = 2;
    Triangle* triangles = NULL;
    uint*     triangleIdxs = NULL;

    BVHNode* bvhRoot = buildBVH(SanMiguelProp, nodesUsed, &triangles, &triangleIdxs);

    uint8_t* bvhTreeNodes = (uint8_t*)tf_malloc(nodesUsed * sizeof(BVHLeafBBox) * 2);

    int dataOffset = 0;
    writeBVHTree(bvhRoot, &triangles, &triangleIdxs, 0, bvhTreeNodes, dataOffset);

    // terminate BVH tree
    BVHNodeBBox* bbox = (BVHNodeBBox*)(bvhTreeNodes + dataOffset);
    bbox->MinBounds.w = 0;
    dataOffset += sizeof(BVHNodeBBox);

    SyncToken      token = {};
    BufferLoadDesc desc = {};
    desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
    desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    desc.mDesc.mSize = dataOffset;
    desc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
    desc.mDesc.mElementCount = (uint32_t)(desc.mDesc.mSize / sizeof(float4));
    desc.mDesc.mStructStride = sizeof(float4);
    desc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
    desc.pData = bvhTreeNodes;
    desc.ppBuffer = &BVHBoundingBoxesBuffer;
    addResource(&desc, &token);
    waitForToken(&token);

    tf_free(bvhRoot);
    tf_free(triangles);
    tf_free(triangleIdxs);
    tf_free(bvhTreeNodes);

    LOGF(eINFO, "Created BVH in %1.2f seconds", getHiresTimerSeconds(&bvhTimer, false));
}

void addRenderTargets()
{
    {
        const ClearValue colorClearWhite = { { 1.0f, 1.0f, 1.0f, 1.0f } };

        // Add visibility buffer render target
        {
            RenderTargetDesc vbRTDesc = {};
            vbRTDesc.mArraySize = 1;
            vbRTDesc.mClearValue = colorClearWhite;
            vbRTDesc.mDepth = 1;
            vbRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            vbRTDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
            vbRTDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
            vbRTDesc.mWidth = mSettings.mWidth;
            vbRTDesc.mHeight = mSettings.mHeight;
            vbRTDesc.mSampleCount = SAMPLE_COUNT_1;
            vbRTDesc.mSampleQuality = 0;
            vbRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
            vbRTDesc.pName = "VB RT";
            addRenderTarget(pRenderer, &vbRTDesc, &pRenderTargetVBPass);
        }

        // Add depth buffer
        {
            RenderTargetDesc depthRT = {};
            depthRT.mArraySize = 1;
            depthRT.mClearValue = { { 0.0f, 0 } };
            depthRT.mDepth = 1;
            depthRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
            depthRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
            depthRT.mHeight = mSettings.mHeight;
            depthRT.mSampleCount = SAMPLE_COUNT_1;
            depthRT.mSampleQuality = 0;
            depthRT.mWidth = mSettings.mWidth;
            depthRT.pName = "Depth RT";
            addRenderTarget(pRenderer, &depthRT, &pRenderTargetDepth);
        }

        // Add Shadow Pass render target
        {
            TextureLoadDesc textureDesc = {};
            TextureDesc     desc = {};
            desc.mWidth = mSettings.mWidth;
            desc.mHeight = mSettings.mHeight;
            desc.mDepth = 1;
            desc.mArraySize = 1;
            desc.mMipLevels = 1;
            desc.mFormat = TinyImageFormat_R8_UNORM;
            desc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
            desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
            desc.mSampleCount = SAMPLE_COUNT_1;
            textureDesc.pDesc = &desc;
            textureDesc.ppTexture = &pTextures[Texture_RaytracedShadows];
            addResource(&textureDesc, NULL);
        }

        // Add Composite Pass render target
        {
            TextureLoadDesc textureDesc = {};
            TextureDesc     desc = {};
            desc.mWidth = mSettings.mWidth;
            desc.mHeight = mSettings.mHeight;
            desc.mDepth = 1;
            desc.mArraySize = 1;
            desc.mMipLevels = 1;
            desc.mFormat = TinyImageFormat_B10G11R11_UFLOAT;
            desc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
            desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
            desc.mSampleCount = SAMPLE_COUNT_1;
            textureDesc.pDesc = &desc;
            textureDesc.ppTexture = &pTextures[Texture_Composite];
            addResource(&textureDesc, NULL);
        }
    }
}

void removeRenderTargets()
{
    removeRenderTarget(pRenderer, pRenderTargetVBPass);
    removeRenderTarget(pRenderer, pRenderTargetDepth);

    removeResource(pTextures[Texture_Composite]);
    removeResource(pTextures[Texture_RaytracedShadows]);
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

        addRenderTargets();
    }

    if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        addPipelines();
    }

    if (pReloadDesc->mType == RELOAD_TYPE_ALL)
    {
        pCameraController->resetView();
    }

    waitForAllResourceLoads();

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

    initScreenshotInterface(pRenderer, pGraphicsQueue);

    return true;
}

void Unload(ReloadDesc* pReloadDesc)
{
    waitQueueIdle(pGraphicsQueue);

    unloadFontSystem(pReloadDesc->mType);
    unloadUserInterface(pReloadDesc->mType);

    if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        removePipelines();
    }

    if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
    {
        removeSwapChain(pRenderer, pSwapChain);
        removeRenderTargets();
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

    Vector4 lightDir = { 0, 1, 0, 0 };
    mat4    lightRotMat = mat4::rotationX(gLightRotationX) * mat4::rotationZ(gLightRotationZ);

    lightDir = lightRotMat * lightDir;

    mat4           viewMat = pCameraController->getViewMatrix();
    /************************************************************************/
    // Update Camera
    /************************************************************************/
    const uint32_t width = mSettings.mWidth;
    const uint32_t height = mSettings.mHeight;

    float        aspectInverse = (float)height / (float)width;
    const float  horizontalFov = PI / 2.0f;
    const float  nearPlane = 0.1f;
    const float  farPlane = 6000.f;
    CameraMatrix projMat = CameraMatrix::perspectiveReverseZ(horizontalFov, aspectInverse, nearPlane, farPlane);

    gCameraUniformData.mView = viewMat;
    gCameraUniformData.mProject = projMat;
    gCameraUniformData.mViewProject = projMat * viewMat;
    gCameraUniformData.mInvProj = CameraMatrix::inverse(projMat);
    gCameraUniformData.mInvView = inverse(viewMat);
    gCameraUniformData.mInvViewProject = CameraMatrix::inverse(gCameraUniformData.mViewProject);
    gCameraUniformData.mNear = nearPlane;
    gCameraUniformData.mFar = farPlane;
    gCameraUniformData.mFarNearDiff = farPlane - nearPlane; // if OpenGL convention was used this would be 2x the value
    gCameraUniformData.mFarNear = nearPlane * farPlane;
    gCameraUniformData.mCameraPos = vec4(pCameraController->getViewPosition(), 1.0f);
    gCameraUniformData.mTwoOverRes = vec2(1.5f / width, 1.5f / height);

    for (int32_t i = 0; i < MESH_COUNT; ++i)
    {
        gMeshInfoUniformData[i][gFrameIndex].mWorldViewProjMat = gCameraUniformData.mViewProject * SanMiguelProp.mWorldMatrix;
    }

    // update Common pass constant buffer
    {
        gCommonUniformData.mRTSize =
            vec4((float)mSettings.mWidth, (float)mSettings.mHeight, 1.0f / mSettings.mWidth, 1.0f / mSettings.mHeight);
        gCommonUniformData.mLightDir = lightDir;
    }

    gVBConstants[gFrameIndex].transform[VIEW_CAMERA].mvp = gCameraUniformData.mViewProject.getPrimaryMatrix() * SanMiguelProp.mWorldMatrix;
    gVBConstants[gFrameIndex].cullingViewports[VIEW_CAMERA].windowSize = { (float)mSettings.mWidth, (float)mSettings.mHeight };
    gVBConstants[gFrameIndex].cullingViewports[VIEW_CAMERA].sampleCount = 1;

    // uiSetComponentActive(pDebugTexturesWindow, gShowDebugTargets);
}

void drawVisibilityBufferPass(Cmd* cmd)
{
    RenderTargetBarrier barriers[] = { { pRenderTargetVBPass, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                                       { pRenderTargetDepth, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE } };
    cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, barriers);

    const char* profileNames[gNumGeomSets] = { "VB pass Opaque", "VB pass Alpha" };

    cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw VB pass");
    if (pRenderer->pGpu->mSettings.mGpuMarkers)
    {
        MarkerDesc marker = {};
        marker.mOffset = MARKER_OFFSET(MARKER_TASK_INDEX);
        marker.mValue = RENDERING_STEP_DRAW_VB_PASS;
        marker.pBuffer = pMarkerBuffer;
        cmdWriteMarker(cmd, &marker);
    }

    BindRenderTargetsDesc bindRenderTargets = {};
    bindRenderTargets.mRenderTargetCount = 1;
    bindRenderTargets.mRenderTargets[0] = { pRenderTargetVBPass, LOAD_ACTION_CLEAR };
    bindRenderTargets.mDepthStencil = { pRenderTargetDepth, LOAD_ACTION_CLEAR };
    cmdBindRenderTargets(cmd, &bindRenderTargets);
    cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mWidth, (float)pRenderTargetVBPass->mHeight, 0.0f, 1.0f);
    cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mWidth, pRenderTargetVBPass->mHeight);

    Buffer* pIndexBuffer = pVisibilityBuffer->ppFilteredIndexBuffer[VIEW_CAMERA];
    cmdBindIndexBuffer(cmd, pIndexBuffer, INDEX_TYPE_UINT32, 0);

    for (uint32_t i = 0; i < gNumGeomSets; ++i)
    {
        Pipeline* pipeline = pPipelineVBBufferPass[i];
        // Using the malfunctioned pipeline
        if (pRenderer->pGpu->mSettings.mGpuMarkers && bCrashedSteps[RENDERING_STEP_DRAW_VB_PASS])
        {
            bCrashedSteps[RENDERING_STEP_DRAW_VB_PASS] = false;
            bHasCrashed = true;
            pipeline = pPipelineVBBufferPass[gNumGeomSets];
            LOGF(LogLevel::eERROR, "[Breadcrumb] Simulating a GPU crash situation (DRAW VISIBILITY BUFFER)...");
        }

        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, profileNames[i]);
        cmdBindPipeline(cmd, pipeline);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
        cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetVBPass[1]);

        uint64_t indirectBufferByteOffset = GET_INDIRECT_DRAW_ELEM_INDEX(VIEW_CAMERA, i, 0) * sizeof(uint32_t);
        Buffer*  pIndirectBuffer = pVisibilityBuffer->ppIndirectDrawArgBuffer[0];
        cmdExecuteIndirect(cmd, pCmdSignatureVBPass, 1, pIndirectBuffer, indirectBufferByteOffset, NULL, 0);

        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
    }
    cmdBindRenderTargets(cmd, NULL);

    cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
}

void Draw()
{
    if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
    {
        waitQueueIdle(pGraphicsQueue);
        ::toggleVSync(pRenderer, &pSwapChain);
    }

    uint32_t swapchainImageIndex;
    acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

    RenderTarget*     pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
    GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);
    FenceStatus       fenceStatus;
    getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
    if (FENCE_STATUS_INCOMPLETE == fenceStatus)
        waitForFences(pRenderer, 1, &elem.pFence);

    if (pRenderer->pGpu->mSettings.mGpuMarkers)
    {
        // Check breadcrumb markers
        bool crashed = checkMarkers();
        if (crashed)
        {
            requestShutdown();
            return;
        }
    }

    /************************************************************************/
    // Update uniform buffers
    /************************************************************************/
    for (uint32_t j = 0; j < MESH_COUNT; ++j)
    {
        BufferUpdateDesc viewProjCbv = { pBufferMeshTransforms[j][gFrameIndex] };
        beginUpdateResource(&viewProjCbv);
        memcpy(viewProjCbv.pMappedData, &gMeshInfoUniformData[j][gFrameIndex], sizeof(MeshInfoUniformBlock));
        endUpdateResource(&viewProjCbv);
    }

    BufferUpdateDesc desc = { pCommonUniformBuffer[gFrameIndex] };
    beginUpdateResource(&desc);
    memcpy(desc.pMappedData, &gCommonUniformData, sizeof(gCommonUniformData));
    endUpdateResource(&desc);

    BufferUpdateDesc cameraCbv = { pBufferCameraUniform[gFrameIndex] };
    beginUpdateResource(&cameraCbv);
    memcpy(cameraCbv.pMappedData, &gCameraUniformData, sizeof(CameraUniform));
    endUpdateResource(&cameraCbv);

    BufferUpdateDesc updateVisibilityBufferConstantDesc = { pBufferVBConstants[gFrameIndex] };
    beginUpdateResource(&updateVisibilityBufferConstantDesc);
    memcpy(updateVisibilityBufferConstantDesc.pMappedData, &gVBConstants[gFrameIndex], sizeof(PerFrameVBConstants));
    endUpdateResource(&updateVisibilityBufferConstantDesc);

    resetCmdPool(pRenderer, elem.pCmdPool);

    Cmd* cmd = elem.pCmds[0];
    beginCmd(cmd);

    MarkerDesc marker = {};
    marker.pBuffer = pMarkerBuffer;

    marker.mValue = gMarkerInitialValue;
    marker.mOffset = MARKER_OFFSET(MARKER_TASK_INDEX);
    cmdWriteMarker(cmd, &marker);

    marker.mOffset = MARKER_OFFSET(MARKER_FRAME_INDEX);
    marker.mValue = gFrameIndex;
    cmdWriteMarker(cmd, &marker);

    cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

    // Visibility Prepass *********************************************************************************
    {
        TriangleFilteringPassDesc triangleFilteringDesc = {};
        triangleFilteringDesc.pPipelineClearBuffers = pPipelineClearBuffers;
        triangleFilteringDesc.pPipelineTriangleFiltering = pPipelineTriangleFiltering;

        triangleFilteringDesc.pDescriptorSetClearBuffers = pDescriptorSetClearBuffers;
        triangleFilteringDesc.pDescriptorSetTriangleFiltering = pDescriptorSetTriangleFiltering[0];
        triangleFilteringDesc.pDescriptorSetTriangleFilteringPerFrame = pDescriptorSetTriangleFiltering[1];

        triangleFilteringDesc.mFrameIndex = gFrameIndex;
        triangleFilteringDesc.mBuffersIndex = 0; // We don't use Async Compute for triangle filtering, we just have 1 buffer
        triangleFilteringDesc.mGpuProfileToken = gGpuProfileToken;
        triangleFilteringDesc.mVBPreFilterStats = SanMiguelProp.mVBPreFilterStats[gFrameIndex];
        cmdVBTriangleFilteringPass(pVisibilityBuffer, cmd, &triangleFilteringDesc);

        {
            const uint32_t numBarriers = NUM_CULLING_VIEWPORTS + 2;
            BufferBarrier  barriers2[numBarriers] = {};
            uint32_t       barrierCount = 0;
            barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDrawArgBuffer[0], RESOURCE_STATE_UNORDERED_ACCESS,
                                          RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };
            barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDataBuffer[gFrameIndex], RESOURCE_STATE_UNORDERED_ACCESS,
                                          RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
            for (uint32_t i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
            {
                barriers2[barrierCount++] = { pVisibilityBuffer->ppFilteredIndexBuffer[i], RESOURCE_STATE_UNORDERED_ACCESS,
                                              RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE };
            }

            cmdResourceBarrier(cmd, barrierCount, barriers2, 0, NULL, 0, NULL);
        }

        drawVisibilityBufferPass(cmd);
        // Transfer DepthBuffer and normals to SRV State
        RenderTargetBarrier barriers[] = { { pRenderTargetDepth, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
                                           { pRenderTargetVBPass, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE } };
        TextureBarrier      uav = { pTextures[Texture_RaytracedShadows], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
        cmdResourceBarrier(cmd, 0, NULL, 1, &uav, 2, barriers);
    }

    cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Shadow/Composite Pass");

    // Raytraced shadow pass ************************************************************************
    {
        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Raytraced shadow Pass");
        if (pRenderer->pGpu->mSettings.mGpuMarkers)
        {
            MarkerDesc marker = {};
            marker.mOffset = MARKER_OFFSET(MARKER_TASK_INDEX);
            marker.mValue = RENDERING_STEP_RAYTRACE_SHADOWS_PASS;
            marker.pBuffer = pMarkerBuffer;
            cmdWriteMarker(cmd, &marker);
        }

        Pipeline* pipeline = pPipeline[RaytracedShadows];
        // Using the malfunctioned pipeline
        if (pRenderer->pGpu->mSettings.mGpuMarkers && bCrashedSteps[RENDERING_STEP_RAYTRACE_SHADOWS_PASS])
        {
            bCrashedSteps[RENDERING_STEP_RAYTRACE_SHADOWS_PASS] = false;
            bHasCrashed = true;
            pipeline = pPipeline[RaytracedShadowsCrash];
            LOGF(LogLevel::eERROR, "[Breadcrumb] Simulating a GPU crash situation (RAYTRACE SHADOWS)...");
        }

        cmdBindPipeline(cmd, pipeline);
        cmdBindDescriptorSet(cmd, Texture_RaytracedShadows, pDescriptorSetCompNonFreq);
        cmdBindDescriptorSet(cmd, gDataBufferCount * Texture_RaytracedShadows + gFrameIndex, pDescriptorSetCompFreq);

        const uint32_t threadGroupSizeX = (mSettings.mWidth + 8 - 1) / 8;
        const uint32_t threadGroupSizeY = (mSettings.mHeight + 8 - 1) / 8;

        cmdDispatch(cmd, threadGroupSizeX, threadGroupSizeY, 1);

        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
    }

    // Composite pass *********************************************************************************
    {
        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Composite Pass");
        if (pRenderer->pGpu->mSettings.mGpuMarkers)
        {
            MarkerDesc marker = {};
            marker.mOffset = MARKER_OFFSET(MARKER_TASK_INDEX);
            marker.mValue = RENDERING_STEP_COMPOSITE_PASS;
            marker.pBuffer = pMarkerBuffer;
            cmdWriteMarker(cmd, &marker);
        }

        // Transfer albedo and lighting to SRV State
        TextureBarrier barriers[] = { { pTextures[Texture_RaytracedShadows], RESOURCE_STATE_UNORDERED_ACCESS,
                                        RESOURCE_STATE_SHADER_RESOURCE },
                                      { pTextures[Texture_Composite], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS } };
        cmdResourceBarrier(cmd, 0, NULL, TF_ARRAY_COUNT(barriers), barriers, 0, NULL);

        Pipeline* pipeline = pPipeline[Composite];
        // Using the malfunctioned pipeline
        if (pRenderer->pGpu->mSettings.mGpuMarkers && bCrashedSteps[RENDERING_STEP_COMPOSITE_PASS])
        {
            bCrashedSteps[RENDERING_STEP_COMPOSITE_PASS] = false;
            bHasCrashed = true;
            pipeline = pPipeline[CompositeCrash];
            LOGF(LogLevel::eERROR, "[Breadcrumb] Simulating a GPU crash situation (COMPOSE FRAME)...");
        }

        cmdBindPipeline(cmd, pipeline);
        cmdBindDescriptorSet(cmd, Texture_Composite, pDescriptorSetCompNonFreq);
        cmdBindDescriptorSet(cmd, gDataBufferCount * Texture_Composite + gFrameIndex, pDescriptorSetCompFreq);

        const uint32_t threadGroupSizeX = (mSettings.mWidth + 16 - 1) / 16;
        const uint32_t threadGroupSizeY = (mSettings.mHeight + 16 - 1) / 16;

        cmdDispatch(cmd, threadGroupSizeX, threadGroupSizeY, 1);

        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

        barriers[0] = { pTextures[Texture_Composite], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };

        RenderTargetBarrier rtBarriers[] = { { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET } };
        cmdResourceBarrier(cmd, 0, NULL, 1, barriers, 1, rtBarriers);
    }

    cmdEndGpuTimestampQuery(cmd, gGpuProfileToken); // Shadow / Composite Pass

    if (pRenderer->pGpu->mSettings.mGpuMarkers)
    {
        MarkerDesc marker = {};
        marker.mOffset = MARKER_OFFSET(MARKER_TASK_INDEX);
        marker.mValue = gMarkerInitialValue;
        marker.pBuffer = pMarkerBuffer;
        cmdWriteMarker(cmd, &marker);
    }

    // Copy results to the backbuffer & draw text *****************************************************************
    {
        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Copy to Backbuffer Pass");

        // draw fullscreen triangle
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_DONTCARE };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

        Pipeline* pipeline = pPipeline[CopyToBackbuffer];

        // Draw  results
        cmdBindPipeline(cmd, pipeline);
        cmdBindDescriptorSet(cmd, 1, pDescriptorSetNonFreq);
        cmdDraw(cmd, 3, 0);
        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

        cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");

        gFrameTimeDraw.mFontColor = 0xff00ffff;
        gFrameTimeDraw.mFontSize = 18.0f;
        gFrameTimeDraw.mFontID = gFontID;
        float2 txtSize = cmdDrawCpuProfile(cmd, float2(8, 15), &gFrameTimeDraw);
        cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y + 75.f), gGpuProfileToken, &gFrameTimeDraw);

        cmdDrawUserInterface(cmd);
        cmdEndDebugMarker(cmd);

        cmdBindRenderTargets(cmd, NULL);
    }

    {
        const uint32_t      numBarriers = NUM_CULLING_VIEWPORTS + 2;
        RenderTargetBarrier barrierPresent = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        BufferBarrier       barriers2[numBarriers] = {};
        uint32_t            barrierCount = 0;
        barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDrawArgBuffer[0],
                                      RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
        barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDataBuffer[gFrameIndex],
                                      RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                      RESOURCE_STATE_UNORDERED_ACCESS };

        for (uint32_t i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
        {
            barriers2[barrierCount++] = { pVisibilityBuffer->ppFilteredIndexBuffer[i],
                                          RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
        }

        cmdResourceBarrier(cmd, numBarriers, barriers2, 0, NULL, 1, &barrierPresent);
    }

    cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
    endCmd(cmd);

    FlushResourceUpdateDesc flushUpdateDesc = {};
    flushUpdateDesc.mNodeIndex = 0;
    flushResourceUpdates(&flushUpdateDesc);
    Semaphore* waitSemaphores[2] = { pImageAcquiredSemaphore, flushUpdateDesc.pOutSubmittedSemaphore };

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
    presentDesc.mIndex = swapchainImageIndex;
    presentDesc.mWaitSemaphoreCount = 1;
    presentDesc.ppWaitSemaphores = &elem.pSemaphore;
    presentDesc.pSwapChain = pSwapChain;
    presentDesc.mSubmitDone = true;
    queuePresent(pGraphicsQueue, &presentDesc);
    flipProfiler();

    gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;
}

const char* GetName() { return "09a_HybridRaytracing"; }

void prepareDescriptorSets()
{
    // Clear Buffers
    {
        DescriptorData clearParams[2] = {};
        clearParams[0].pName = "indirectDrawArgs";
        clearParams[0].ppBuffers = &pVisibilityBuffer->ppIndirectDrawArgBuffer[0];
        clearParams[1].pName = "VBConstantBuffer";
        clearParams[1].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetClearBuffers, 2, clearParams);
    }
    // Triangle Filtering
    {
        DescriptorData filterParams[4] = {};
        filterParams[0].pName = "vertexPositionBuffer";
        filterParams[0].ppBuffers = &SanMiguelProp.pGeom->pVertexBuffers[0];
        filterParams[1].pName = "indexDataBuffer";
        filterParams[1].ppBuffers = &SanMiguelProp.pGeom->pIndexBuffer;
        filterParams[2].pName = "meshConstantsBuffer";
        filterParams[2].ppBuffers = &pBufferMeshConstants;
        filterParams[3].pName = "VBConstantBuffer";
        filterParams[3].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetTriangleFiltering[0], 4, filterParams);

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData filterParams[5] = {};
            filterParams[0].pName = "filteredIndicesBuffer";
            filterParams[0].mCount = NUM_CULLING_VIEWPORTS;
            filterParams[0].ppBuffers = &pVisibilityBuffer->ppFilteredIndexBuffer[0];
            filterParams[1].pName = "indirectDataBuffer";
            filterParams[1].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
            filterParams[2].pName = "PerFrameVBConstants";
            filterParams[2].ppBuffers = &pBufferVBConstants[i];
            filterParams[3].pName = "filterDispatchGroupDataBuffer";
            filterParams[3].ppBuffers = &pVisibilityBuffer->ppFilterDispatchGroupDataBuffer[i];
            filterParams[4].pName = "indirectDrawArgs";
            filterParams[4].ppBuffers = &pVisibilityBuffer->ppIndirectDrawArgBuffer[0];
            updateDescriptorSet(pRenderer, i, pDescriptorSetTriangleFiltering[1], 5, filterParams);
        }
    }

    // VB Pass
    {
        DescriptorData objectParams[3] = {};

        objectParams[0].pName = "diffuseMaps";
        objectParams[0].mCount = SanMiguelProp.mMaterialCount;
        objectParams[0].ppTextures = SanMiguelProp.pTextureStorage;
        objectParams[1].pName = "vertexPositionBuffer";
        objectParams[1].ppBuffers = &SanMiguelProp.pGeom->pVertexBuffers[0];
        objectParams[2].pName = "vertexTexCoordBuffer";
        objectParams[2].ppBuffers = &SanMiguelProp.pGeom->pVertexBuffers[1];
        updateDescriptorSet(pRenderer, 0, pDescriptorSetVBPass[0], 3, objectParams);

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData objectParams[2] = {};
            objectParams[0].pName = "objectUniformBlock";
            objectParams[0].ppBuffers = &pBufferMeshTransforms[0][i];
            objectParams[1].pName = "indirectDataBuffer";
            objectParams[1].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetVBPass[1], 2, objectParams);
        }
    }

    // Raytraced Shadows
    {
        DescriptorData params[9] = {};
        params[0].pName = "depthBuffer";
        params[0].ppTextures = &pRenderTargetDepth->pTexture;
        params[1].pName = "vbPassTexture";
        params[1].ppTextures = &pRenderTargetVBPass->pTexture;
        params[2].pName = "diffuseMaps";
        params[2].mCount = SanMiguelProp.mMaterialCount;
        params[2].ppTextures = SanMiguelProp.pTextureStorage;
        params[3].pName = "vertexPos";
        params[3].ppBuffers = &SanMiguelProp.pGeom->pVertexBuffers[0];
        params[4].pName = "vertexTexCoord";
        params[4].ppBuffers = &SanMiguelProp.pGeom->pVertexBuffers[1];
        params[5].pName = "vertexNormal";
        params[5].ppBuffers = &SanMiguelProp.pGeom->pVertexBuffers[2];
        params[6].pName = "BVHTree";
        params[6].ppBuffers = &BVHBoundingBoxesBuffer;
        params[7].pName = "outputShadowRT";
        params[7].ppTextures = &pTextures[Texture_RaytracedShadows];
        params[8].pName = "VBConstantBuffer";
        params[8].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
        updateDescriptorSet(pRenderer, Texture_RaytracedShadows, pDescriptorSetCompNonFreq, 9, params);
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData params[4] = {};
            params[0].pName = "cbPerPass";
            params[0].ppBuffers = &pCommonUniformBuffer[i];
            params[1].pName = "cameraUniformBlock";
            params[1].ppBuffers = &pBufferCameraUniform[i];
            params[2].pName = "indirectDataBuffer";
            params[2].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
            params[3].pName = "filteredIndexBuffer";
            params[3].ppBuffers = &pVisibilityBuffer->ppFilteredIndexBuffer[VIEW_CAMERA];
            updateDescriptorSet(pRenderer, Texture_RaytracedShadows * gDataBufferCount + i, pDescriptorSetCompFreq, 4, params);
        }
    }

    // Composite
    {
        DescriptorData params[8] = {};
        params[0].pName = "vbPassTexture";
        params[0].ppTextures = &pRenderTargetVBPass->pTexture;
        params[1].pName = "diffuseMaps";
        params[1].mCount = SanMiguelProp.mMaterialCount;
        params[1].ppTextures = SanMiguelProp.pTextureStorage;
        params[2].pName = "vertexPos";
        params[2].ppBuffers = &SanMiguelProp.pGeom->pVertexBuffers[0];
        params[3].pName = "vertexTexCoord";
        params[3].ppBuffers = &SanMiguelProp.pGeom->pVertexBuffers[1];
        params[4].pName = "vertexNormal";
        params[4].ppBuffers = &SanMiguelProp.pGeom->pVertexBuffers[2];
        params[5].pName = "shadowbuffer";
        params[5].ppTextures = &pTextures[Texture_RaytracedShadows];
        params[6].pName = "outputRT";
        params[6].ppTextures = &pTextures[Texture_Composite];
        params[7].pName = "VBConstantBuffer";
        params[7].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
        updateDescriptorSet(pRenderer, Texture_Composite, pDescriptorSetCompNonFreq, 8, params);
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData params[4] = {};
            params[0].pName = "cbPerPass";
            params[0].ppBuffers = &pCommonUniformBuffer[i];
            params[1].pName = "cameraUniformBlock";
            params[1].ppBuffers = &pBufferCameraUniform[i];
            params[2].pName = "indirectDataBuffer";
            params[2].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
            params[3].pName = "filteredIndexBuffer";
            params[3].ppBuffers = &pVisibilityBuffer->ppFilteredIndexBuffer[VIEW_CAMERA];
            updateDescriptorSet(pRenderer, Texture_Composite * gDataBufferCount + i, pDescriptorSetCompFreq, 4, params);
        }
    }

    // Display
    {
        DescriptorData params[1] = {};
        params[0].pName = "inputRT";
        params[0].ppTextures = &pTextures[Texture_Composite];
        updateDescriptorSet(pRenderer, 1, pDescriptorSetNonFreq, 1, params);
    }
}

bool addSwapChain()
{
    SwapChainDesc swapChainDesc = {};
    swapChainDesc.mWindowHandle = pWindow->handle;
    swapChainDesc.ppPresentQueues = &pGraphicsQueue;
    swapChainDesc.mPresentQueueCount = 1;
    swapChainDesc.mWidth = mSettings.mWidth;
    swapChainDesc.mHeight = mSettings.mHeight;
    swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
    swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
    swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
    swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
    ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

    return pSwapChain != NULL;
}

void addDescriptorSets()
{
    // Clear buffers
    DescriptorSetDesc setDesc = { pRootSignatureClearBuffers, DESCRIPTOR_UPDATE_FREQ_NONE, gDataBufferCount };
    addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetClearBuffers);
    // Triangle filtering
    setDesc = { pRootSignatureTriangleFiltering, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
    addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFiltering[0]);
    setDesc = { pRootSignatureTriangleFiltering, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
    addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFiltering[1]);

    // vb pass
    setDesc = { pRootSignatureVBPass, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
    addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBPass[0]);

    setDesc = { pRootSignatureVBPass, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
    addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBPass[1]);

    // display
    setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
    addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetNonFreq);

    // Textures
    DescriptorSetDesc compDesc = { pRootSignatureComp, DESCRIPTOR_UPDATE_FREQ_NONE, TextureCount };
    addDescriptorSet(pRenderer, &compDesc, &pDescriptorSetCompNonFreq);
    compDesc = { pRootSignatureComp, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount * TextureCount };
    addDescriptorSet(pRenderer, &compDesc, &pDescriptorSetCompFreq);
}

void removeDescriptorSets()
{
    removeDescriptorSet(pRenderer, pDescriptorSetClearBuffers);
    removeDescriptorSet(pRenderer, pDescriptorSetTriangleFiltering[0]);
    removeDescriptorSet(pRenderer, pDescriptorSetTriangleFiltering[1]);
    removeDescriptorSet(pRenderer, pDescriptorSetVBPass[0]);
    removeDescriptorSet(pRenderer, pDescriptorSetVBPass[1]);

    removeDescriptorSet(pRenderer, pDescriptorSetNonFreq);

    removeDescriptorSet(pRenderer, pDescriptorSetCompNonFreq);
    removeDescriptorSet(pRenderer, pDescriptorSetCompFreq);
}

void addRootSignatures()
{
    RootSignatureDesc clearBuffersRootDesc = { &pShaderClearBuffers, 1 };
    addRootSignature(pRenderer, &clearBuffersRootDesc, &pRootSignatureClearBuffers);

    RootSignatureDesc triangleFilteringRootDesc = { &pShaderTriangleFiltering, 1 };
    addRootSignature(pRenderer, &triangleFilteringRootDesc, &pRootSignatureTriangleFiltering);

    const char*       vbPassSamplerNames[] = { "nearClampSampler" };
    Sampler*          vbPassSamplers[] = { pSamplerMiplessNear };
    // Include the crash shader
    RootSignatureDesc vbPassRootDesc = { pShaderVBBufferPass, gNumGeomSets + 1 };
    vbPassRootDesc.mMaxBindlessTextures = SanMiguelProp.mMaterialCount;
    vbPassRootDesc.ppStaticSamplerNames = vbPassSamplerNames;
    vbPassRootDesc.mStaticSamplerCount = TF_ARRAY_COUNT(vbPassSamplers);
    vbPassRootDesc.ppStaticSamplers = vbPassSamplers;
    addRootSignature(pRenderer, &vbPassRootDesc, &pRootSignatureVBPass);

    const char*       pStaticSamplers[] = { "samplerLinear" };
    RootSignatureDesc rootDesc = {};
    rootDesc.mStaticSamplerCount = 1;
    rootDesc.ppStaticSamplerNames = pStaticSamplers;
    rootDesc.ppStaticSamplers = &pSamplerLinearWrap;
    rootDesc.mShaderCount = 1;
    rootDesc.ppShaders = &pShader[CopyToBackbuffer];
    addRootSignature(pRenderer, &rootDesc, &pRootSignature);

    IndirectArgumentDescriptor indirectArg = {};
    indirectArg.mType = INDIRECT_DRAW_INDEX;
    CommandSignatureDesc vbPassDesc = { pRootSignatureVBPass, &indirectArg, 1 };
    addIndirectCommandSignature(pRenderer, &vbPassDesc, &pCmdSignatureVBPass);

    Sampler*    compSamplers[] = { pSamplerTrilinearAniso, pSamplerMiplessLinear };
    const char* pStaticCompSamplers[] = { "textureSampler", "clampMiplessLinearSampler" };
    Shader*     compShaders[] = { pShader[Composite], pShader[RaytracedShadows] };

    RootSignatureDesc rootCompDesc = {};
    rootCompDesc.mShaderCount = TF_ARRAY_COUNT(compShaders);
    rootCompDesc.ppShaders = compShaders;
    rootCompDesc.mStaticSamplerCount = 2;
    rootCompDesc.ppStaticSamplers = compSamplers;
    rootCompDesc.ppStaticSamplerNames = pStaticCompSamplers;
    rootCompDesc.mMaxBindlessTextures = TOTAL_IMGS;
    addRootSignature(pRenderer, &rootCompDesc, &pRootSignatureComp);
}

void removeRootSignatures()
{
    removeRootSignature(pRenderer, pRootSignature);
    removeRootSignature(pRenderer, pRootSignatureVBPass);
    removeRootSignature(pRenderer, pRootSignatureComp);
    removeRootSignature(pRenderer, pRootSignatureClearBuffers);
    removeRootSignature(pRenderer, pRootSignatureTriangleFiltering);

    removeIndirectCommandSignature(pRenderer, pCmdSignatureVBPass);
}

void addShaders()
{
    // Load shaders for Vis Buffer
    ShaderLoadDesc clearBuffersShaderDesc = {};
    clearBuffersShaderDesc.mStages[0].pFileName = "clearVisibilityBuffers.comp";
    addShader(pRenderer, &clearBuffersShaderDesc, &pShaderClearBuffers);

    ShaderLoadDesc triangleFilteringShaderDesc = {};
    triangleFilteringShaderDesc.mStages[0].pFileName = "triangleFiltering.comp";
    addShader(pRenderer, &triangleFilteringShaderDesc, &pShaderTriangleFiltering);

#if defined(VULKAN)
    // Some vulkan driver doesn't generate glPrimitiveID without a geometry pass (steam deck as 03/30/2023)
    bool addGeometryPassThrough = gGpuSettings.mAddGeometryPassThrough && pRenderer->mRendererApi == RENDERER_API_VULKAN;
#else
    bool addGeometryPassThrough = false;
#endif
    // No SV_PrimitiveID in pixel shader on ORBIS. Only available in gs stage so we need
#if defined(ORBIS) || defined(PROSPERO)
    addGeometryPassThrough = true;
#endif

    ShaderLoadDesc shaderVBrepass = {};
    shaderVBrepass.mStages[0].pFileName = "visibilityBufferPass.vert";
    shaderVBrepass.mStages[1].pFileName = "visibilityBufferPass.frag";
    if (addGeometryPassThrough)
    {
        // a passthrough gs
        shaderVBrepass.mStages[2].pFileName = "visibilityBufferPass.geom";
    }
    addShader(pRenderer, &shaderVBrepass, &pShaderVBBufferPass[GEOMSET_OPAQUE]);

    ShaderLoadDesc visibilityBufferPassAlphaShaderDesc = {};
    visibilityBufferPassAlphaShaderDesc.mStages[0].pFileName = "visibilityBufferPassAlpha.vert";
    visibilityBufferPassAlphaShaderDesc.mStages[1].pFileName = "visibilityBufferPassAlpha.frag";
    if (addGeometryPassThrough)
    {
        // a passthrough gs
        visibilityBufferPassAlphaShaderDesc.mStages[2].pFileName = "visibilityBufferPassAlpha.geom";
    }
    addShader(pRenderer, &visibilityBufferPassAlphaShaderDesc, &pShaderVBBufferPass[GEOMSET_ALPHA_CUTOUT]);

    ShaderLoadDesc vbCrashDesc = shaderVBrepass;
    vbCrashDesc.mStages[0].pFileName = "visibilityBufferPassCrash.vert";
    addShader(pRenderer, &vbCrashDesc, &pShaderVBBufferPass[gNumGeomSets]);

    // shader for Shadow pass
    ShaderLoadDesc shadowsShader = {};
    shadowsShader.mStages[0].pFileName = "raytracedShadowsPass.comp";
    addShader(pRenderer, &shadowsShader, &pShader[RaytracedShadows]);
    shadowsShader.mStages[0].pFileName = "raytracedShadowsPassCrash.comp";
    addShader(pRenderer, &shadowsShader, &pShader[RaytracedShadowsCrash]);

    // shader for Composite pass
    ShaderLoadDesc compositeShader = {};
    compositeShader.mStages[0].pFileName = "compositePass.comp";
    addShader(pRenderer, &compositeShader, &pShader[Composite]);
    compositeShader.mStages[0].pFileName = "compositePassCrash.comp";
    addShader(pRenderer, &compositeShader, &pShader[CompositeCrash]);

    // Load shaders for copy to backbufferpass
    ShaderLoadDesc copyShader = {};
    copyShader.mStages[0].pFileName = "display.vert";
    copyShader.mStages[1].pFileName = "display.frag";
    addShader(pRenderer, &copyShader, &pShader[CopyToBackbuffer]);
}

void removeShaders()
{
    removeShader(pRenderer, pShaderClearBuffers);
    removeShader(pRenderer, pShaderTriangleFiltering);
    removeShader(pRenderer, pShaderVBBufferPass[GEOMSET_OPAQUE]);
    removeShader(pRenderer, pShaderVBBufferPass[GEOMSET_ALPHA_CUTOUT]);
    removeShader(pRenderer, pShaderVBBufferPass[gNumGeomSets]);
    removeShader(pRenderer, pShader[Composite]);
    removeShader(pRenderer, pShader[CompositeCrash]);
    removeShader(pRenderer, pShader[RaytracedShadows]);
    removeShader(pRenderer, pShader[RaytracedShadowsCrash]);
    removeShader(pRenderer, pShader[CopyToBackbuffer]);
}

void addPipelines()
{
    // Create rasteriser state objects
    RasterizerStateDesc rasterStateCullNoneDesc = { CULL_MODE_NONE };

    // Create depth state objects
    DepthStateDesc depthStateDesc = {};
    depthStateDesc.mDepthTest = true;
    depthStateDesc.mDepthWrite = true;
    depthStateDesc.mDepthFunc = CMP_GEQUAL;

    DepthStateDesc depthStateNoneDesc = {};
    depthStateNoneDesc.mDepthTest = false;
    depthStateNoneDesc.mDepthWrite = false;
    depthStateNoneDesc.mDepthFunc = CMP_ALWAYS;

    // add vis buffer pipeline
    {
        /************************************************************************/
        // Setup the resources needed for the Visibility Buffer Pipeline
        /******************************/
        PipelineDesc desc = {};
        desc.mGraphicsDesc = {};
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& vbPassPipelineSettings = desc.mGraphicsDesc;
        vbPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        vbPassPipelineSettings.mRenderTargetCount = 1;
        vbPassPipelineSettings.pDepthState = &depthStateDesc;
        vbPassPipelineSettings.pColorFormats = &pRenderTargetVBPass->mFormat;
        vbPassPipelineSettings.mSampleCount = pRenderTargetVBPass->mSampleCount;
        vbPassPipelineSettings.mSampleQuality = pRenderTargetVBPass->mSampleQuality;
        vbPassPipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mFormat;
        vbPassPipelineSettings.pRootSignature = pRootSignatureVBPass;
        vbPassPipelineSettings.pVertexLayout = NULL;
        vbPassPipelineSettings.pRasterizerState = &rasterStateCullNoneDesc;
        for (uint32_t i = 0; i < gNumGeomSets; ++i)
        {
            vbPassPipelineSettings.pShaderProgram = pShaderVBBufferPass[i];

#if defined(XBOX)
            ExtendedGraphicsPipelineDesc edescs[2] = {};
            edescs[0].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_SHADER_LIMITS;
            initExtendedGraphicsShaderLimits(&edescs[0].shaderLimitsDesc);
            edescs[0].shaderLimitsDesc.maxWavesWithLateAllocParameterCache = 16;

            edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_DEPTH_STENCIL_OPTIONS;
            edescs[1].pixelShaderOptions.outOfOrderRasterization = PIXEL_SHADER_OPTION_OUT_OF_ORDER_RASTERIZATION_ENABLE_WATER_MARK_7;
            edescs[1].pixelShaderOptions.depthBeforeShader =
                !i ? PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_ENABLE : PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_DEFAULT;

            desc.mExtensionCount = 2;
            desc.pPipelineExtensions = edescs;
#endif
            addPipeline(pRenderer, &desc, &pPipelineVBBufferPass[i]);

            desc.mExtensionCount = 0;
        }

        vbPassPipelineSettings.pShaderProgram = pShaderVBBufferPass[gNumGeomSets];
        addPipeline(pRenderer, &desc, &pPipelineVBBufferPass[gNumGeomSets]);

        desc.mGraphicsDesc = {};
        desc.mType = PIPELINE_TYPE_COMPUTE;
        desc.mComputeDesc = {};

        ComputePipelineDesc& clearBufferPipelineSettings = desc.mComputeDesc;
        clearBufferPipelineSettings.pShaderProgram = pShaderClearBuffers;
        clearBufferPipelineSettings.pRootSignature = pRootSignatureClearBuffers;
        addPipeline(pRenderer, &desc, &pPipelineClearBuffers);

        desc.mComputeDesc = {};
        ComputePipelineDesc& triangleFilteringPipelineSettings = desc.mComputeDesc;
        triangleFilteringPipelineSettings.pShaderProgram = pShaderTriangleFiltering;
        triangleFilteringPipelineSettings.pRootSignature = pRootSignatureTriangleFiltering;
        addPipeline(pRenderer, &desc, &pPipelineTriangleFiltering);
    }

    // create shadows pipeline
    {
        PipelineDesc desc = {};
        desc.mType = PIPELINE_TYPE_COMPUTE;
        ComputePipelineDesc& pipelineDesc = desc.mComputeDesc;
        pipelineDesc.pRootSignature = pRootSignatureComp;
        pipelineDesc.pShaderProgram = pShader[RaytracedShadows];
        addPipeline(pRenderer, &desc, &pPipeline[RaytracedShadows]);
        pipelineDesc.pShaderProgram = pShader[RaytracedShadowsCrash];
        addPipeline(pRenderer, &desc, &pPipeline[RaytracedShadowsCrash]);
    }

    // create composite pipeline
    {
        PipelineDesc desc = {};
        desc.mType = PIPELINE_TYPE_COMPUTE;
        ComputePipelineDesc& pipelineDesc = desc.mComputeDesc;
        pipelineDesc.pRootSignature = pRootSignatureComp;
        pipelineDesc.pShaderProgram = pShader[Composite];
        addPipeline(pRenderer, &desc, &pPipeline[Composite]);
        pipelineDesc.pShaderProgram = pShader[CompositeCrash];
        addPipeline(pRenderer, &desc, &pPipeline[CompositeCrash]);
    }

    // create copy to backbuffer pipeline
    {
        PipelineDesc desc = {};
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pRasterizerState = &rasterStateCullNoneDesc;
        pipelineSettings.pDepthState = &depthStateNoneDesc;

        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;

        pipelineSettings.pRootSignature = pRootSignature;
        pipelineSettings.pShaderProgram = pShader[CopyToBackbuffer];

        addPipeline(pRenderer, &desc, &pPipeline[CopyToBackbuffer]);
    }
}

void removePipelines()
{
    removePipeline(pRenderer, pPipelineClearBuffers);
    removePipeline(pRenderer, pPipelineTriangleFiltering);
    for (uint32_t i = 0; i <= gNumGeomSets; ++i)
    {
        removePipeline(pRenderer, pPipelineVBBufferPass[i]);
    }
    removePipeline(pRenderer, pPipeline[RaytracedShadows]);
    removePipeline(pRenderer, pPipeline[RaytracedShadowsCrash]);
    removePipeline(pRenderer, pPipeline[Composite]);
    removePipeline(pRenderer, pPipeline[CompositeCrash]);
    removePipeline(pRenderer, pPipeline[CopyToBackbuffer]);
}

void initMarkers()
{
    BufferLoadDesc breadcrumbBuffer = {};
    breadcrumbBuffer.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNDEFINED;
    breadcrumbBuffer.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
    breadcrumbBuffer.mDesc.mSize = GPU_MARKER_SIZE * MARKER_COUNT;
    breadcrumbBuffer.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
#if defined(VULKAN)
    breadcrumbBuffer.mDesc.mFlags |= BUFFER_CREATION_FLAG_MARKER;
#endif
    breadcrumbBuffer.mDesc.mStartState = RESOURCE_STATE_COPY_DEST;
    breadcrumbBuffer.pData = NULL;
    breadcrumbBuffer.ppBuffer = &pMarkerBuffer;
    addResource(&breadcrumbBuffer, NULL);
}

bool checkMarkers()
{
    if (!bHasCrashed)
    {
        return false;
    }

    threadSleep(2000);
    uint32_t* markersValue = (uint32_t*)pMarkerBuffer->pCpuMappedAddress;
    if (!markersValue)
    {
        return true;
    }
    uint32_t    taskIndex = GPU_MARKER_VALUE(pMarkerBuffer, MARKER_TASK_INDEX * GPU_MARKER_SIZE);
    uint32_t    frameIndex = GPU_MARKER_VALUE(pMarkerBuffer, MARKER_FRAME_INDEX * GPU_MARKER_SIZE);
    const char* stepName = taskIndex != gMarkerInitialValue ? gMarkerNames[taskIndex] : "Unknown";
    LOGF(LogLevel::eINFO, "Last rendering step (approx): %s, crashed frame: %u", stepName, frameIndex);
    bHasCrashed = false;
    return true;
}

void exitMarkers() { removeResource(pMarkerBuffer); }
}
;

DEFINE_APPLICATION_MAIN(HybridRaytracing)

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

#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../ThirdParty/OpenSource/imgui/imgui.h"
#include "../ThirdParty/OpenSource/imgui/imgui_internal.h"

#include "../../OS/Interfaces/IInput.h"
#include "../../Application/Interfaces/IFont.h"
#include "../../Application/Interfaces/IUI.h"
#include "../../Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"

#include "../../Utilities/Math/Algorithms.h"

#include "../../Utilities/Interfaces/IMemory.h"

#define FALLBACK_FONT_TEXTURE_INDEX 0

#define MAX_LABEL_LENGTH            128

#define LABELID(prop, buffer)       snprintf(buffer, MAX_LABEL_LENGTH, "##%llu", (unsigned long long)(prop.pData))
#define LABELID1(prop, buffer)      snprintf(buffer, MAX_LABEL_LENGTH, "##%llu", (unsigned long long)(prop))

#define MAX_LUA_STR_LEN             256
static const uint32_t MAX_FRAMES = 3;

struct GUIDriverUpdate
{
    UIComponent** pUIComponents = NULL;
    uint32_t      componentCount = 0;
    float         deltaTime = 0.f;
    float         width = 0.f;
    float         height = 0.f;
    bool          showDemoWindow = false;
};

struct UIAppImpl
{
    Renderer*     pRenderer = NULL;
    // stb_ds array of UIComponent*
    UIComponent** mComponents = NULL;
};

typedef struct UserInterface
{
    float    mWidth = 0.f;
    float    mHeight = 0.f;
    float    mDisplayWidth = 0.f;
    float    mDisplayHeight = 0.f;
    uint32_t mMaxDynamicUIUpdatesPerBatch = 20u;
    uint32_t mMaxUIFonts = 10u;
    uint32_t mFrameCount = 2;

    // Following var is useful for seeing UI capabilities and tweaking style settings.
    // Will only take effect if at least one GUI Component is active.
    bool mShowDemoUiWindow = false;

    Renderer*     pRenderer = NULL;
    // stb_ds array of UIComponent*
    UIComponent** mComponents = NULL;

    PipelineCache* pPipelineCache = NULL;
    ImGuiContext*  context = NULL;
    // (Texture*)[dyn_size]
    struct UIFontResource
    {
        Texture*  pFontTex = NULL;
        uint32_t  mFontId = 0;
        float     mFontSize = 0.f;
        uintptr_t pFont = 0;
    }* pCachedFontsArr = NULL;
    uintptr_t pDefaultFallbackFont = 0;
    float     dpiScale[2] = { 0.0f };
    uint32_t  frameIdx = 0;

    struct TextureNode
    {
        uint64_t key = ~0ull;
        Texture* value = NULL;
    }* pTextureHashmap = NULL;
    uint32_t       mDynamicTexturesCount = 0;
    Shader*        pShaderTextured[SAMPLE_COUNT_COUNT] = { NULL };
    RootSignature* pRootSignatureTextured = NULL;
    RootSignature* pRootSignatureTexturedMs = NULL;
    DescriptorSet* pDescriptorSetUniforms = NULL;
    DescriptorSet* pDescriptorSetTexture = NULL;
    Pipeline*      pPipelineTextured[SAMPLE_COUNT_COUNT] = { NULL };
    Buffer*        pVertexBuffer = NULL;
    Buffer*        pIndexBuffer = NULL;
    Buffer*        pUniformBuffer[MAX_FRAMES] = { NULL };
    /// Default states
    Sampler*       pDefaultSampler = NULL;
    VertexLayout   mVertexLayoutTextured = {};

#if defined(ENABLE_FORGE_TOUCH_INPUT)
    // Virtual joystick UI
    Shader*        pVJShader = {};
    RootSignature* pVJRootSignature = {};
    DescriptorSet* pVJDescriptorSet = {};
    Pipeline*      pVJPipeline = {};
    Texture*       pVJTexture = {};
    uint32_t       mVJRootConstantIndex = {};
#endif

    uint32_t mLastUpdateCount = 0;
    float2   mLastUpdateMin[64] = {};
    float2   mLastUpdateMax[64] = {};
    bool     mActive = false;

    // Stops rendering UI elements (disables command recording)
    bool mEnableRendering = true;
} UserInterface;

#if defined(GFX_DRIVER_MEMORY_TRACKING) || defined(GFX_DEVICE_MEMORY_TRACKING)
extern uint32_t    GetTrackedObjectTypeCount();
extern const char* GetTrackedObjectName(uint32_t obj);
#endif

#if defined(GFX_DRIVER_MEMORY_TRACKING)
// Driver memory getters
extern uint64_t GetDriverAllocationsCount();
extern uint64_t GetDriverMemoryAmount();
extern uint64_t GetDriverAllocationsPerObject(uint32_t obj);
extern uint64_t GetDriverMemoryPerObject(uint32_t obj);
#endif

#if defined(GFX_DEVICE_MEMORY_TRACKING)
// Device memory getters
extern uint64_t GetDeviceAllocationsCount();
extern uint64_t GetDeviceMemoryAmount();
extern uint64_t GetDeviceAllocationsPerObject(uint32_t obj);
extern uint64_t GetDeviceMemoryPerObject(uint32_t obj);
#endif

#ifdef ENABLE_FORGE_UI
static UserInterface* pUserInterface = NULL;

namespace ImGui
{
bool SliderFloatWithSteps(const char* label, float* v, float v_min, float v_max, float v_step, const char* display_format)
{
    char text_buf[MAX_FORMAT_STR_LENGTH];
    bool value_changed = false;

    if (!display_format)
        display_format = "%.1f";
    snprintf(text_buf, MAX_FORMAT_STR_LENGTH, display_format, *v);

    if (ImGui::GetIO().WantTextInput)
    {
        value_changed = ImGui::SliderFloat(label, v, v_min, v_max, text_buf);

        int v_i = int(((*v - v_min) / v_step) + 0.5f);
        *v = v_min + float(v_i) * v_step;
    }
    else
    {
        // Map from [v_min,v_max] to [0,N]
        const int countValues = int((v_max - v_min) / v_step);
        int       v_i = int(((*v - v_min) / v_step) + 0.5f);
        value_changed = ImGui::SliderInt(label, &v_i, 0, countValues, text_buf);

        // Remap from [0,N] to [v_min,v_max]
        *v = v_min + float(v_i) * v_step;
    }

    if (*v < v_min)
        *v = v_min;
    if (*v > v_max)
        *v = v_max;

    return value_changed;
}

bool SliderIntWithSteps(const char* label, int32_t* v, int32_t v_min, int32_t v_max, int32_t v_step, const char* display_format)
{
    char text_buf[MAX_FORMAT_STR_LENGTH];
    bool value_changed = false;

    if (!display_format)
        display_format = "%d";
    snprintf(text_buf, MAX_FORMAT_STR_LENGTH, display_format, *v);

    if (ImGui::GetIO().WantTextInput)
    {
        value_changed = ImGui::SliderInt(label, v, v_min, v_max, text_buf);

        int32_t v_i = int((*v - v_min) / v_step);
        *v = v_min + int32_t(v_i) * v_step;
    }
    else
    {
        // Map from [v_min,v_max] to [0,N]
        const int countValues = int((v_max - v_min) / v_step);
        int32_t   v_i = int((*v - v_min) / v_step);
        value_changed = ImGui::SliderInt(label, &v_i, 0, countValues, text_buf);

        // Remap from [0,N] to [v_min,v_max]
        *v = v_min + int32_t(v_i) * v_step;
    }

    if (*v < v_min)
        *v = v_min;
    if (*v > v_max)
        *v = v_max;

    return value_changed;
}
} // namespace ImGui

/****************************************************************************/
// MARK: - Static Function Declarations
/****************************************************************************/

// UIWidget functions
static UIWidget* cloneWidget(const UIWidget* pWidget);
static void      processWidgetCallbacks(UIWidget* pWidget, bool deferred = false);
static void      processWidget(UIWidget* pWidget);
static void      removeWidget(UIWidget* pWidget, bool freeUnderlying);

static void* alloc_func(size_t size, void* user_data)
{
    UNREF_PARAM(user_data);
    return tf_malloc(size);
}

static void dealloc_func(void* ptr, void* user_data)
{
    UNREF_PARAM(user_data);
    tf_free(ptr);
}

static void SetDefaultStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.4f;
    style.Colors[ImGuiCol_Text] = float4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = float4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = float4(0.06f, 0.06f, 0.06f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = float4(1.00f, 1.00f, 1.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg] = float4(0.08f, 0.08f, 0.08f, 0.94f);
    style.Colors[ImGuiCol_Border] = float4(0.43f, 0.43f, 0.50f, 0.50f);
    style.Colors[ImGuiCol_BorderShadow] = float4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg] = float4(0.20f, 0.21f, 0.22f, 0.54f);
    style.Colors[ImGuiCol_FrameBgHovered] = float4(0.40f, 0.40f, 0.40f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive] = float4(0.18f, 0.18f, 0.18f, 0.67f);
    style.Colors[ImGuiCol_TitleBg] = float4(0.04f, 0.04f, 0.04f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = float4(0.29f, 0.29f, 0.29f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = float4(0.00f, 0.00f, 0.00f, 0.51f);
    style.Colors[ImGuiCol_MenuBarBg] = float4(0.14f, 0.14f, 0.14f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg] = float4(0.02f, 0.02f, 0.02f, 0.53f);
    style.Colors[ImGuiCol_ScrollbarGrab] = float4(0.31f, 0.31f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = float4(0.41f, 0.41f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = float4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = float4(0.94f, 0.94f, 0.94f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = float4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = float4(0.86f, 0.86f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_Button] = float4(0.44f, 0.44f, 0.44f, 0.40f);
    style.Colors[ImGuiCol_ButtonHovered] = float4(0.46f, 0.47f, 0.48f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = float4(0.42f, 0.42f, 0.42f, 1.00f);
    style.Colors[ImGuiCol_Header] = float4(0.70f, 0.70f, 0.70f, 0.31f);
    style.Colors[ImGuiCol_HeaderHovered] = float4(0.70f, 0.70f, 0.70f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive] = float4(0.48f, 0.50f, 0.52f, 1.00f);
    style.Colors[ImGuiCol_Separator] = float4(0.43f, 0.43f, 0.50f, 0.50f);
    style.Colors[ImGuiCol_SeparatorHovered] = float4(0.72f, 0.72f, 0.72f, 0.78f);
    style.Colors[ImGuiCol_SeparatorActive] = float4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = float4(0.91f, 0.91f, 0.91f, 0.25f);
    style.Colors[ImGuiCol_ResizeGripHovered] = float4(0.81f, 0.81f, 0.81f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive] = float4(0.46f, 0.46f, 0.46f, 0.95f);
    style.Colors[ImGuiCol_PlotLines] = float4(0.61f, 0.61f, 0.61f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = float4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = float4(0.73f, 0.60f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = float4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = float4(0.87f, 0.87f, 0.87f, 0.35f);
    style.Colors[ImGuiCol_DragDropTarget] = float4(1.00f, 1.00f, 0.00f, 0.90f);
    style.Colors[ImGuiCol_NavHighlight] = float4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = float4(1.00f, 1.00f, 1.00f, 0.70f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = float4(0.80f, 0.80f, 0.80f, 0.35f);
    style.Colors[ImGuiCol_Tab] = ImLerp(style.Colors[ImGuiCol_Header], style.Colors[ImGuiCol_TitleBg], 0.80f);
    style.Colors[ImGuiCol_TabHovered] = style.Colors[ImGuiCol_HeaderHovered];
    style.Colors[ImGuiCol_TabActive] = ImLerp(style.Colors[ImGuiCol_HeaderActive], style.Colors[ImGuiCol_TitleBg], 0.90f);
    style.Colors[ImGuiCol_TabUnfocused] = ImLerp(style.Colors[ImGuiCol_Tab], style.Colors[ImGuiCol_TitleBg], 0.80f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImLerp(
        ImLerp(style.Colors[ImGuiCol_HeaderActive], style.Colors[ImGuiCol_TitleBgActive], 0.80f), style.Colors[ImGuiCol_TitleBg], 0.40f);
    style.Colors[ImGuiCol_DockingPreview] = style.Colors[ImGuiCol_HeaderActive] * ImVec4(1.0f, 1.0f, 1.0f, 0.7f);

    float          dpiScale[2];
    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, dpiScale);
    style.ScaleAllSizes(min(dpiScale[0], dpiScale[1]));
}

#if defined(GFX_DRIVER_MEMORY_TRACKING)
void DrawDriverMemoryTrackingUI(void*)
{
    uint64_t totDriverMemory = GetDriverMemoryAmount();
    uint64_t totDriverAllocs = GetDriverAllocationsCount();

    struct DriverMemoryEntry
    {
        const char* mName;
        uint64_t    mMemAmount;
        uint64_t    mMemAllocs;
    };

    if (totDriverMemory == 0)
    {
        return;
    }

    // Add UI components
    static bool driverMemUsageShowKB = false;
    ImGui::Checkbox("Driver Memory Show In Kilobytes", &driverMemUsageShowKB);
    const char* memUnit = driverMemUsageShowKB ? "KB" : "MB";
    size_t      memDivisor = driverMemUsageShowKB ? TF_KB : TF_MB;

    if (ImGui::TreeNodeEx("DriverMemoryTracking", ImGuiTreeNodeFlags_DefaultOpen, "Total Driver Memory Usage: %zu %s | Alloc Count: %zu",
                          (size_t)(totDriverMemory / memDivisor), memUnit, (size_t)totDriverAllocs))
    {
        // Gather all entries
        DriverMemoryEntry entries[TRACKED_OBJECT_TYPE_COUNT_MAX] = {};

        for (uint32_t i = 0; i < GetTrackedObjectTypeCount(); ++i)
        {
            entries[i].mName = GetTrackedObjectName(i);
            entries[i].mMemAmount = GetDriverMemoryPerObject(i);
            entries[i].mMemAllocs = GetDriverAllocationsPerObject(i);
        }

        // Sort entries by amount of memory
        qsort(
            entries, GetTrackedObjectTypeCount(), sizeof(DriverMemoryEntry),
            +[](const void* lhs, const void* rhs)
            {
                DriverMemoryEntry* pLhs = (DriverMemoryEntry*)lhs;
                DriverMemoryEntry* pRhs = (DriverMemoryEntry*)rhs;
                if (pLhs->mMemAmount == pRhs->mMemAmount)
                    return (pLhs->mMemAllocs > pRhs->mMemAllocs) ? -1 : 1;

                return pLhs->mMemAmount > pRhs->mMemAmount ? -1 : 1;
            });

        const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders;
        if (ImGui::BeginTable("DriverMemoryTrackingTable", 3, tableFlags))
        {
            for (uint32_t type = 0; type < GetTrackedObjectTypeCount(); ++type)
            {
                float colors[2] = { 0.05f, 0.4f };
                ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(colors[type % 2]));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", entries[type].mName);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("Memory: %zu %s", (size_t)(entries[type].mMemAmount / memDivisor), memUnit);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("Alloc count: %zu", (size_t)entries[type].mMemAllocs);
                ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        }

        ImGui::TreePop();
    }
}
#endif // GFX_DRIVER_MEMORY_TRACKING

#if defined(GFX_DEVICE_MEMORY_TRACKING)
void DrawDeviceMemoryReportUI(void*)
{
    struct DeviceMemoryEntry
    {
        const char* mName;
        uint64_t    mMemAmount;
        uint64_t    mMemAllocs;
    };

    const uint64_t memReportTotalMemory = GetDeviceMemoryAmount();
    const uint64_t memReportTotalAllocCount = GetDeviceAllocationsCount();

    if (memReportTotalMemory == 0)
    {
        return;
    }

    static bool memReportMemUsageShowKB = false;
    ImGui::Checkbox("Memory Report EXT Memory Show In Kilobytes", &memReportMemUsageShowKB);
    const char*  memUnit = memReportMemUsageShowKB ? "KB" : "MB";
    const size_t memDivisor = memReportMemUsageShowKB ? TF_KB : TF_MB;

    if (ImGui::TreeNodeEx("DeviceMemoryReport", ImGuiTreeNodeFlags_DefaultOpen, "Total GPU Memory: %zu %s | Alloc Count: %zu",
                          memReportTotalMemory / memDivisor, memUnit, memReportTotalAllocCount))
    {
        // Gather all entries
        DeviceMemoryEntry entries[TRACKED_OBJECT_TYPE_COUNT_MAX] = {};

        for (uint32_t i = 0; i < GetTrackedObjectTypeCount(); ++i)
        {
            entries[i].mName = GetTrackedObjectName(i);
            entries[i].mMemAmount = GetDeviceMemoryPerObject(i);
            entries[i].mMemAllocs = GetDeviceAllocationsPerObject(i);
        }

        // Sort entries by amount of memory
        qsort(
            entries, GetTrackedObjectTypeCount(), sizeof(DeviceMemoryEntry),
            +[](const void* lhs, const void* rhs)
            {
                DeviceMemoryEntry* pLhs = (DeviceMemoryEntry*)lhs;
                DeviceMemoryEntry* pRhs = (DeviceMemoryEntry*)rhs;
                if (pLhs->mMemAmount == pRhs->mMemAmount)
                    return (pLhs->mMemAllocs > pRhs->mMemAllocs) ? -1 : 1;

                return pLhs->mMemAmount > pRhs->mMemAmount ? -1 : 1;
            });

        const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders;
        if (ImGui::BeginTable("DeviceMemoryTrackingTable", 3, tableFlags))
        {
            for (uint32_t type = 0; type < GetTrackedObjectTypeCount(); ++type)
            {
                float colors[2] = { 0.05f, 0.4f };
                ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(colors[type % 2]));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", entries[type].mName);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("Memory: %lu %s", entries[type].mMemAmount / memDivisor, memUnit);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("Alloc count: %lu", entries[type].mMemAllocs);
                ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        }

        ImGui::TreePop();
    }
}
#endif // GFX_DEVICE_MEMORY_TRACKING

/****************************************************************************/
// MARK: - Non-static Function Definitions
/****************************************************************************/

uint32_t addImguiFont(void* pFontBuffer, uint32_t fontBufferSize, void* pFontGlyphRanges, uint32_t fontID, float fontSize, uintptr_t* pFont)
{
    ImGuiIO& io = ImGui::GetIO();

    // Build and load the texture atlas into a texture
    int            width, height, bytesPerPixel;
    unsigned char* pixels = NULL;

    io.Fonts->ClearInputData();
    if (pFontBuffer == NULL)
    {
        *pFont = (uintptr_t)io.Fonts->AddFontDefault();
    }
    else
    {
        ImFontConfig config = {};
        config.FontDataOwnedByAtlas = false;
        ImFont* font = io.Fonts->AddFontFromMemoryTTF(pFontBuffer, fontBufferSize,
                                                      fontSize * min(pUserInterface->dpiScale[0], pUserInterface->dpiScale[1]), &config,
                                                      (const ImWchar*)pFontGlyphRanges);
        if (font != NULL)
        {
            io.FontDefault = font;
            *pFont = (uintptr_t)font;
        }
        else
        {
            *pFont = (uintptr_t)io.Fonts->AddFontDefault();
        }
    }

    io.Fonts->Build();
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytesPerPixel);

    // At this point you've got the texture data and you need to upload that your your graphic system:
    // After we have created the texture, store its pointer/identifier (_in whichever format your engine uses_) in 'io.Fonts->TexID'.
    // This will be passed back to your via the renderer. Basically ImTextureID == void*. Read FAQ below for details about ImTextureID.
    Texture*        pTexture = NULL;
    SyncToken       token = {};
    TextureLoadDesc loadDesc = {};
    TextureDesc     textureDesc = {};
    textureDesc.mArraySize = 1;
    textureDesc.mDepth = 1;
    textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
    textureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
    textureDesc.mHeight = height;
    textureDesc.mMipLevels = 1;
    textureDesc.mSampleCount = SAMPLE_COUNT_1;
    textureDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    textureDesc.mWidth = width;
    textureDesc.pName = "ImGui Font Texture";
    loadDesc.pDesc = &textureDesc;
    loadDesc.ppTexture = &pTexture;
    addResource(&loadDesc, &token);
    waitForToken(&token);

    TextureUpdateDesc updateDesc = { pTexture, 0, 1, 0, 1, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
    beginUpdateResource(&updateDesc);
    TextureSubresourceUpdate subresource = updateDesc.getSubresourceUpdateDesc(0, 0);
    for (uint32_t r = 0; r < subresource.mRowCount; ++r)
    {
        memcpy(subresource.pMappedData + r * subresource.mDstRowStride, pixels + r * subresource.mSrcRowStride, subresource.mSrcRowStride);
    }
    endUpdateResource(&updateDesc);

    UserInterface::UIFontResource newCachedFont = { pTexture, fontID, fontSize, *pFont };
    arrpush(pUserInterface->pCachedFontsArr, newCachedFont);

    ptrdiff_t fontTextureIndex = arrlen(pUserInterface->pCachedFontsArr) - 1;
    io.Fonts->TexID = (void*)fontTextureIndex;

    return (uint32_t)fontTextureIndex;
}

/****************************************************************************/
// MARK: - Static Value Definitions
/****************************************************************************/

static const uint64_t VERTEX_BUFFER_SIZE = FORGE_UI_MAX_VERTEXES * sizeof(ImDrawVert);
static const uint64_t INDEX_BUFFER_SIZE = FORGE_UI_MAX_INDEXES * sizeof(ImDrawIdx);

/****************************************************************************/
// MARK: - Base UIWidget Helper Functions
/****************************************************************************/

// CollapsingHeaderWidget private functions
static CollapsingHeaderWidget* cloneCollapsingHeaderWidget(const void* pWidget)
{
    const CollapsingHeaderWidget* pOriginalWidget = (const CollapsingHeaderWidget*)pWidget;
    CollapsingHeaderWidget*       pClonedWidget =
        (CollapsingHeaderWidget*)tf_malloc(sizeof(CollapsingHeaderWidget) + sizeof(UIWidget*) * pOriginalWidget->mWidgetsCount);

    pClonedWidget->pGroupedWidgets = (UIWidget**)(pClonedWidget + 1);
    pClonedWidget->mWidgetsCount = pOriginalWidget->mWidgetsCount;
    pClonedWidget->mCollapsed = pOriginalWidget->mCollapsed;
    pClonedWidget->mPreviousCollapsed = false;
    pClonedWidget->mDefaultOpen = pOriginalWidget->mDefaultOpen;
    pClonedWidget->mHeaderIsVisible = pOriginalWidget->mHeaderIsVisible;
    for (uint32_t i = 0; i < pOriginalWidget->mWidgetsCount; ++i)
    {
        pClonedWidget->pGroupedWidgets[i] = cloneWidget(pOriginalWidget->pGroupedWidgets[i]);
    }

    return pClonedWidget;
}

// DebugTexturesWidget private functions
static DebugTexturesWidget* cloneDebugTexturesWidget(const void* pWidget)
{
    const DebugTexturesWidget* pOriginalWidget = (const DebugTexturesWidget*)pWidget;
    DebugTexturesWidget*       pClonedWidget = (DebugTexturesWidget*)tf_malloc(sizeof(DebugTexturesWidget));

    pClonedWidget->pTextures = pOriginalWidget->pTextures;
    pClonedWidget->mTexturesCount = pOriginalWidget->mTexturesCount;
    pClonedWidget->mTextureDisplaySize = pOriginalWidget->mTextureDisplaySize;

    return pClonedWidget;
}

// LabelWidget private functions
static LabelWidget* cloneLabelWidget(const void* pWidget)
{
    UNREF_PARAM(pWidget);
    LabelWidget* pClonedWidget = (LabelWidget*)tf_calloc(1, sizeof(LabelWidget));

    return pClonedWidget;
}

// ColorLabelWidget private functions
static ColorLabelWidget* cloneColorLabelWidget(const void* pWidget)
{
    const ColorLabelWidget* pOriginalWidget = (const ColorLabelWidget*)pWidget;
    ColorLabelWidget*       pClonedWidget = (ColorLabelWidget*)tf_calloc(1, sizeof(ColorLabelWidget));

    pClonedWidget->mColor = pOriginalWidget->mColor;

    return pClonedWidget;
}

// HorizontalSpaceWidget private functions
static HorizontalSpaceWidget* cloneHorizontalSpaceWidget(const void* pWidget)
{
    UNREF_PARAM(pWidget);
    HorizontalSpaceWidget* pClonedWidget = (HorizontalSpaceWidget*)tf_calloc(1, sizeof(HorizontalSpaceWidget));

    return pClonedWidget;
}

// SeparatorWidget private functions
static SeparatorWidget* cloneSeparatorWidget(const void* pWidget)
{
    UNREF_PARAM(pWidget);
    SeparatorWidget* pClonedWidget = (SeparatorWidget*)tf_calloc(1, sizeof(SeparatorWidget));

    return pClonedWidget;
}

// VerticalSeparatorWidget private functions
static VerticalSeparatorWidget* cloneVerticalSeparatorWidget(const void* pWidget)
{
    const VerticalSeparatorWidget* pOriginalWidget = (const VerticalSeparatorWidget*)pWidget;
    VerticalSeparatorWidget*       pClonedWidget = (VerticalSeparatorWidget*)tf_calloc(1, sizeof(VerticalSeparatorWidget));

    pClonedWidget->mLineCount = pOriginalWidget->mLineCount;

    return pClonedWidget;
}

// ButtonWidget private functions
static ButtonWidget* cloneButtonWidget(const void* pWidget)
{
    UNREF_PARAM(pWidget);
    ButtonWidget* pClonedWidget = (ButtonWidget*)tf_calloc(1, sizeof(ButtonWidget));

    return pClonedWidget;
}

// SliderFloatWidget private functions
static SliderFloatWidget* cloneSliderFloatWidget(const void* pWidget)
{
    const SliderFloatWidget* pOriginalWidget = (const SliderFloatWidget*)pWidget;
    SliderFloatWidget*       pClonedWidget = (SliderFloatWidget*)tf_calloc(1, sizeof(SliderFloatWidget));

    memset(pClonedWidget->mFormat, 0, MAX_FORMAT_STR_LENGTH);
    strcpy(pClonedWidget->mFormat, pOriginalWidget->mFormat);

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->mMin = pOriginalWidget->mMin;
    pClonedWidget->mMax = pOriginalWidget->mMax;
    pClonedWidget->mStep = pOriginalWidget->mStep;

    return pClonedWidget;
}

// SliderFloat2Widget private functions
static SliderFloat2Widget* cloneSliderFloat2Widget(const void* pWidget)
{
    const SliderFloat2Widget* pOriginalWidget = (const SliderFloat2Widget*)pWidget;
    SliderFloat2Widget*       pClonedWidget = (SliderFloat2Widget*)tf_calloc(1, sizeof(SliderFloat2Widget));

    memset(pClonedWidget->mFormat, 0, MAX_FORMAT_STR_LENGTH);
    strcpy(pClonedWidget->mFormat, pOriginalWidget->mFormat);

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->mMin = pOriginalWidget->mMin;
    pClonedWidget->mMax = pOriginalWidget->mMax;
    pClonedWidget->mStep = pOriginalWidget->mStep;

    return pClonedWidget;
}

// SliderFloat3Widget private functions
static SliderFloat3Widget* cloneSliderFloat3Widget(const void* pWidget)
{
    const SliderFloat3Widget* pOriginalWidget = (const SliderFloat3Widget*)pWidget;
    SliderFloat3Widget*       pClonedWidget = (SliderFloat3Widget*)tf_calloc(1, sizeof(SliderFloat3Widget));

    memset(pClonedWidget->mFormat, 0, MAX_FORMAT_STR_LENGTH);
    strcpy(pClonedWidget->mFormat, pOriginalWidget->mFormat);

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->mMin = pOriginalWidget->mMin;
    pClonedWidget->mMax = pOriginalWidget->mMax;
    pClonedWidget->mStep = pOriginalWidget->mStep;

    return pClonedWidget;
}

// SliderFloat4Widget private functions
static SliderFloat4Widget* cloneSliderFloat4Widget(const void* pWidget)
{
    const SliderFloat4Widget* pOriginalWidget = (const SliderFloat4Widget*)pWidget;
    SliderFloat4Widget*       pClonedWidget = (SliderFloat4Widget*)tf_calloc(1, sizeof(SliderFloat4Widget));

    memset(pClonedWidget->mFormat, 0, MAX_FORMAT_STR_LENGTH);
    strcpy(pClonedWidget->mFormat, pOriginalWidget->mFormat);

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->mMin = pOriginalWidget->mMin;
    pClonedWidget->mMax = pOriginalWidget->mMax;
    pClonedWidget->mStep = pOriginalWidget->mStep;

    return pClonedWidget;
}

// SliderIntWidget private functions
static SliderIntWidget* cloneSliderIntWidget(const void* pWidget)
{
    const SliderIntWidget* pOriginalWidget = (const SliderIntWidget*)pWidget;
    SliderIntWidget*       pClonedWidget = (SliderIntWidget*)tf_calloc(1, sizeof(SliderIntWidget));

    memset(pClonedWidget->mFormat, 0, MAX_FORMAT_STR_LENGTH);
    strcpy(pClonedWidget->mFormat, pOriginalWidget->mFormat);

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->mMin = pOriginalWidget->mMin;
    pClonedWidget->mMax = pOriginalWidget->mMax;
    pClonedWidget->mStep = pOriginalWidget->mStep;

    return pClonedWidget;
}

// SliderUintWidget private functions
static SliderUintWidget* cloneSliderUintWidget(const void* pWidget)
{
    const SliderUintWidget* pOriginalWidget = (const SliderUintWidget*)pWidget;
    SliderUintWidget*       pClonedWidget = (SliderUintWidget*)tf_calloc(1, sizeof(SliderUintWidget));

    memset(pClonedWidget->mFormat, 0, MAX_FORMAT_STR_LENGTH);
    strcpy(pClonedWidget->mFormat, pOriginalWidget->mFormat);

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->mMin = pOriginalWidget->mMin;
    pClonedWidget->mMax = pOriginalWidget->mMax;
    pClonedWidget->mStep = pOriginalWidget->mStep;

    return pClonedWidget;
}

// RadioButtonWidget private functions
static RadioButtonWidget* cloneRadioButtonWidget(const void* pWidget)
{
    const RadioButtonWidget* pOriginalWidget = (const RadioButtonWidget*)pWidget;
    RadioButtonWidget*       pClonedWidget = (RadioButtonWidget*)tf_calloc(1, sizeof(RadioButtonWidget));

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->mRadioId = pOriginalWidget->mRadioId;

    return pClonedWidget;
}

// CheckboxWidget private functions
static CheckboxWidget* cloneCheckboxWidget(const void* pWidget)
{
    const CheckboxWidget* pOriginalWidget = (const CheckboxWidget*)pWidget;
    CheckboxWidget*       pClonedWidget = (CheckboxWidget*)tf_calloc(1, sizeof(CheckboxWidget));

    pClonedWidget->pData = pOriginalWidget->pData;

    return pClonedWidget;
}

// OneLineCheckboxWidget private functions
static OneLineCheckboxWidget* cloneOneLineCheckboxWidget(const void* pWidget)
{
    const OneLineCheckboxWidget* pOriginalWidget = (const OneLineCheckboxWidget*)pWidget;
    OneLineCheckboxWidget*       pClonedWidget = (OneLineCheckboxWidget*)tf_calloc(1, sizeof(OneLineCheckboxWidget));

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->mColor = pOriginalWidget->mColor;

    return pClonedWidget;
}

// CursorLocationWidget private functions
static CursorLocationWidget* cloneCursorLocationWidget(const void* pWidget)
{
    const CursorLocationWidget* pOriginalWidget = (const CursorLocationWidget*)pWidget;
    CursorLocationWidget*       pClonedWidget = (CursorLocationWidget*)tf_calloc(1, sizeof(CursorLocationWidget));

    pClonedWidget->mLocation = pOriginalWidget->mLocation;

    return pClonedWidget;
}

// DropdownWidget private functions
static DropdownWidget* cloneDropdownWidget(const void* pWidget)
{
    const DropdownWidget* pOriginalWidget = (const DropdownWidget*)pWidget;

    DropdownWidget* pClonedWidget = (DropdownWidget*)tf_malloc(sizeof(DropdownWidget));

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->pNames = pOriginalWidget->pNames;
    pClonedWidget->mCount = pOriginalWidget->mCount;

    return pClonedWidget;
}

// ColumnWidget private functions
static ColumnWidget* cloneColumnWidget(const void* pWidget)
{
    const ColumnWidget* pOriginalWidget = (const ColumnWidget*)pWidget;
    ColumnWidget*       pClonedWidget = (ColumnWidget*)tf_malloc(sizeof(ColumnWidget) + sizeof(UIWidget*) * pOriginalWidget->mWidgetsCount);
    pClonedWidget->pPerColumnWidgets = (UIWidget**)(pClonedWidget + 1);
    pClonedWidget->mWidgetsCount = pOriginalWidget->mWidgetsCount;

    for (uint32_t i = 0; i < pOriginalWidget->mWidgetsCount; ++i)
        pClonedWidget->pPerColumnWidgets[i] = cloneWidget(pOriginalWidget->pPerColumnWidgets[i]);

    return pClonedWidget;
}

// ProgressBarWidget private functions
static ProgressBarWidget* cloneProgressBarWidget(const void* pWidget)
{
    const ProgressBarWidget* pOriginalWidget = (const ProgressBarWidget*)pWidget;
    ProgressBarWidget*       pClonedWidget = (ProgressBarWidget*)tf_calloc(1, sizeof(ProgressBarWidget));

    pClonedWidget->pData = pOriginalWidget->pData;
    pClonedWidget->mMaxProgress = pOriginalWidget->mMaxProgress;

    return pClonedWidget;
}

// ColorSliderWidget private functions
static ColorSliderWidget* cloneColorSliderWidget(const void* pWidget)
{
    const ColorSliderWidget* pOriginalWidget = (const ColorSliderWidget*)pWidget;
    ColorSliderWidget*       pClonedWidget = (ColorSliderWidget*)tf_calloc(1, sizeof(ColorSliderWidget));

    pClonedWidget->pData = pOriginalWidget->pData;

    return pClonedWidget;
}

// HistogramWidget private functions
static HistogramWidget* cloneHistogramWidget(const void* pWidget)
{
    const HistogramWidget* pOriginalWidget = (const HistogramWidget*)pWidget;
    HistogramWidget*       pClonedWidget = (HistogramWidget*)tf_calloc(1, sizeof(HistogramWidget));

    pClonedWidget->pValues = pOriginalWidget->pValues;
    pClonedWidget->mCount = pOriginalWidget->mCount;
    pClonedWidget->mMinScale = pOriginalWidget->mMinScale;
    pClonedWidget->mMaxScale = pOriginalWidget->mMaxScale;
    pClonedWidget->mHistogramSize = pOriginalWidget->mHistogramSize;
    pClonedWidget->mHistogramTitle = pOriginalWidget->mHistogramTitle;

    return pClonedWidget;
}

// PlotLinesWidget private functions
static PlotLinesWidget* clonePlotLinesWidget(const void* pWidget)
{
    const PlotLinesWidget* pOriginalWidget = (const PlotLinesWidget*)pWidget;
    PlotLinesWidget*       pClonedWidget = (PlotLinesWidget*)tf_calloc(1, sizeof(PlotLinesWidget));

    pClonedWidget->mValues = pOriginalWidget->mValues;
    pClonedWidget->mNumValues = pOriginalWidget->mNumValues;
    pClonedWidget->mScaleMin = pOriginalWidget->mScaleMin;
    pClonedWidget->mScaleMax = pOriginalWidget->mScaleMax;
    pClonedWidget->mPlotScale = pOriginalWidget->mPlotScale;
    pClonedWidget->mTitle = pOriginalWidget->mTitle;

    return pClonedWidget;
}

// ColorPickerWidget private functions
static ColorPickerWidget* cloneColorPickerWidget(const void* pWidget)
{
    const ColorPickerWidget* pOriginalWidget = (const ColorPickerWidget*)pWidget;
    ColorPickerWidget*       pClonedWidget = (ColorPickerWidget*)tf_calloc(1, sizeof(ColorPickerWidget));

    pClonedWidget->pData = pOriginalWidget->pData;

    return pClonedWidget;
}

// ColorPickerWidget private functions
static Color3PickerWidget* cloneColor3PickerWidget(const void* pWidget)
{
    const Color3PickerWidget* pOriginalWidget = (const Color3PickerWidget*)pWidget;
    Color3PickerWidget*       pClonedWidget = (Color3PickerWidget*)tf_calloc(1, sizeof(Color3PickerWidget));

    pClonedWidget->pData = pOriginalWidget->pData;

    return pClonedWidget;
}

// TextboxWidget private functions
static TextboxWidget* cloneTextboxWidget(const void* pWidget)
{
    const TextboxWidget* pOriginalWidget = (const TextboxWidget*)pWidget;

    TextboxWidget* pClonedWidget = (TextboxWidget*)tf_malloc(sizeof(TextboxWidget));

    pClonedWidget->pText = pOriginalWidget->pText;
    pClonedWidget->mFlags = pOriginalWidget->mFlags;
    pClonedWidget->pCallback = pOriginalWidget->pCallback;

    return pClonedWidget;
}

// DynamicTextWidget private functions
static DynamicTextWidget* cloneDynamicTextWidget(const void* pWidget)
{
    const DynamicTextWidget* pOriginalWidget = (const DynamicTextWidget*)pWidget;

    DynamicTextWidget* pClonedWidget = (DynamicTextWidget*)tf_malloc(sizeof(DynamicTextWidget));

    pClonedWidget->pText = pOriginalWidget->pText;
    pClonedWidget->pColor = pOriginalWidget->pColor;

    return pClonedWidget;
}

// FilledRectWidget private functions
static FilledRectWidget* cloneFilledRectWidget(const void* pWidget)
{
    const FilledRectWidget* pOriginalWidget = (const FilledRectWidget*)pWidget;
    FilledRectWidget*       pClonedWidget = (FilledRectWidget*)tf_calloc(1, sizeof(FilledRectWidget));

    pClonedWidget->mPos = pOriginalWidget->mPos;
    pClonedWidget->mScale = pOriginalWidget->mScale;
    pClonedWidget->mColor = pOriginalWidget->mColor;

    return pClonedWidget;
}

// DrawTextWidget private functions
static DrawTextWidget* cloneDrawTextWidget(const void* pWidget)
{
    const DrawTextWidget* pOriginalWidget = (const DrawTextWidget*)pWidget;
    DrawTextWidget*       pClonedWidget = (DrawTextWidget*)tf_calloc(1, sizeof(DrawTextWidget));

    pClonedWidget->mPos = pOriginalWidget->mPos;
    pClonedWidget->mColor = pOriginalWidget->mColor;

    return pClonedWidget;
}

// DrawTooltipWidget private functions
static DrawTooltipWidget* cloneDrawTooltipWidget(const void* pWidget)
{
    const DrawTooltipWidget* pOriginalWidget = (const DrawTooltipWidget*)pWidget;
    DrawTooltipWidget*       pClonedWidget = (DrawTooltipWidget*)tf_calloc(1, sizeof(DrawTooltipWidget));

    pClonedWidget->mShowTooltip = pOriginalWidget->mShowTooltip;
    pClonedWidget->mText = pOriginalWidget->mText;

    return pClonedWidget;
}

// DrawLineWidget private functions
static DrawLineWidget* cloneDrawLineWidget(const void* pWidget)
{
    const DrawLineWidget* pOriginalWidget = (const DrawLineWidget*)pWidget;
    DrawLineWidget*       pClonedWidget = (DrawLineWidget*)tf_calloc(1, sizeof(DrawLineWidget));

    pClonedWidget->mPos1 = pOriginalWidget->mPos1;
    pClonedWidget->mPos2 = pOriginalWidget->mPos2;
    pClonedWidget->mColor = pOriginalWidget->mColor;
    pClonedWidget->mAddItem = pOriginalWidget->mAddItem;

    return pClonedWidget;
}

// DrawCurveWidget private functions
static DrawCurveWidget* cloneDrawCurveWidget(const void* pWidget)
{
    const DrawCurveWidget* pOriginalWidget = (const DrawCurveWidget*)pWidget;
    DrawCurveWidget*       pClonedWidget = (DrawCurveWidget*)tf_calloc(1, sizeof(DrawCurveWidget));

    pClonedWidget->mPos = pOriginalWidget->mPos;
    pClonedWidget->mNumPoints = pOriginalWidget->mNumPoints;
    pClonedWidget->mThickness = pOriginalWidget->mThickness;
    pClonedWidget->mColor = pOriginalWidget->mColor;

    return pClonedWidget;
}

// CustomWidget private functions
static CustomWidget* cloneCustomWidget(const void* pWidget)
{
    const CustomWidget* pOriginalWidget = (const CustomWidget*)pWidget;
    CustomWidget*       pClonedWidget = (CustomWidget*)tf_calloc(1, sizeof(CustomWidget));

    pClonedWidget->pCallback = pOriginalWidget->pCallback;
    pClonedWidget->pUserData = pOriginalWidget->pUserData;
    pClonedWidget->pDestroyCallback = pOriginalWidget->pDestroyCallback;

    return pClonedWidget;
}

// UIWidget private functions
static void cloneWidgetBase(UIWidget* pDstWidget, const UIWidget* pSrcWidget)
{
    pDstWidget->mType = pSrcWidget->mType;
    strcpy(pDstWidget->mLabel, pSrcWidget->mLabel);

    pDstWidget->pOnHoverUserData = pSrcWidget->pOnHoverUserData;
    pDstWidget->pOnHover = pSrcWidget->pOnHover;
    pDstWidget->pOnActiveUserData = pSrcWidget->pOnActiveUserData;
    pDstWidget->pOnActive = pSrcWidget->pOnActive;
    pDstWidget->pOnFocusUserData = pSrcWidget->pOnFocusUserData;
    pDstWidget->pOnFocus = pSrcWidget->pOnFocus;
    pDstWidget->pOnEditedUserData = pSrcWidget->pOnEditedUserData;
    pDstWidget->pOnEdited = pSrcWidget->pOnEdited;
    pDstWidget->pOnDeactivatedUserData = pSrcWidget->pOnDeactivatedUserData;
    pDstWidget->pOnDeactivated = pSrcWidget->pOnDeactivated;
    pDstWidget->pOnDeactivatedAfterEditUserData = pSrcWidget->pOnDeactivatedAfterEditUserData;
    pDstWidget->pOnDeactivatedAfterEdit = pSrcWidget->pOnDeactivatedAfterEdit;

    pDstWidget->mDeferred = pSrcWidget->mDeferred;
}

// UIWidget private functions
static UIWidget* cloneWidget(const UIWidget* pOtherWidget)
{
    UIWidget* pWidget = (UIWidget*)tf_calloc(1, sizeof(UIWidget));
    cloneWidgetBase(pWidget, pOtherWidget);

    switch (pOtherWidget->mType)
    {
    case WIDGET_TYPE_COLLAPSING_HEADER:
    {
        pWidget->pWidget = cloneCollapsingHeaderWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_DEBUG_TEXTURES:
    {
        pWidget->pWidget = cloneDebugTexturesWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_LABEL:
    {
        pWidget->pWidget = cloneLabelWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR_LABEL:
    {
        pWidget->pWidget = cloneColorLabelWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_HORIZONTAL_SPACE:
    {
        pWidget->pWidget = cloneHorizontalSpaceWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_SEPARATOR:
    {
        pWidget->pWidget = cloneSeparatorWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_VERTICAL_SEPARATOR:
    {
        pWidget->pWidget = cloneVerticalSeparatorWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_BUTTON:
    {
        pWidget->pWidget = cloneButtonWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT:
    {
        pWidget->pWidget = cloneSliderFloatWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT2:
    {
        pWidget->pWidget = cloneSliderFloat2Widget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT3:
    {
        pWidget->pWidget = cloneSliderFloat3Widget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT4:
    {
        pWidget->pWidget = cloneSliderFloat4Widget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_INT:
    {
        pWidget->pWidget = cloneSliderIntWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_UINT:
    {
        pWidget->pWidget = cloneSliderUintWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_RADIO_BUTTON:
    {
        pWidget->pWidget = cloneRadioButtonWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_CHECKBOX:
    {
        pWidget->pWidget = cloneCheckboxWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_ONE_LINE_CHECKBOX:
    {
        pWidget->pWidget = cloneOneLineCheckboxWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_CURSOR_LOCATION:
    {
        pWidget->pWidget = cloneCursorLocationWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_DROPDOWN:
    {
        pWidget->pWidget = cloneDropdownWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_COLUMN:
    {
        pWidget->pWidget = cloneColumnWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_PROGRESS_BAR:
    {
        pWidget->pWidget = cloneProgressBarWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR_SLIDER:
    {
        pWidget->pWidget = cloneColorSliderWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_HISTOGRAM:
    {
        pWidget->pWidget = cloneHistogramWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_PLOT_LINES:
    {
        pWidget->pWidget = clonePlotLinesWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR_PICKER:
    {
        pWidget->pWidget = cloneColorPickerWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR3_PICKER:
    {
        pWidget->pWidget = cloneColor3PickerWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_TEXTBOX:
    {
        pWidget->pWidget = cloneTextboxWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_DYNAMIC_TEXT:
    {
        pWidget->pWidget = cloneDynamicTextWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_FILLED_RECT:
    {
        pWidget->pWidget = cloneFilledRectWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_TEXT:
    {
        pWidget->pWidget = cloneDrawTextWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_TOOLTIP:
    {
        pWidget->pWidget = cloneDrawTooltipWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_LINE:
    {
        pWidget->pWidget = cloneDrawLineWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_CURVE:
    {
        pWidget->pWidget = cloneDrawCurveWidget(pOtherWidget->pWidget);
        break;
    }

    case WIDGET_TYPE_CUSTOM:
    {
        pWidget->pWidget = cloneCustomWidget(pOtherWidget->pWidget);
        break;
    }

    default:
        ASSERT(0);
    }

    return pWidget;
}

// UIWidget public functions
static void processWidgetCallbacks(UIWidget* pWidget, bool deferred)
{
    if (!deferred)
    {
        pWidget->mHovered = ImGui::IsItemHovered();
        pWidget->mActive = ImGui::IsItemActive();
        pWidget->mFocused = ImGui::IsItemFocused();

        // ImGui::Button doesn't set the IsItemEdited flag, we assing ourselves:
        // pWidget->mEdited = ImGui::Button(...);
        if (pWidget->mType != WIDGET_TYPE_BUTTON)
            pWidget->mEdited = ImGui::IsItemEdited();

        pWidget->mDeactivated = ImGui::IsItemDeactivated();
        pWidget->mDeactivatedAfterEdit = ImGui::IsItemDeactivatedAfterEdit();
    }

    if (pWidget->mDeferred != deferred)
    {
        return;
    }

    if (pWidget->pOnHover && pWidget->mHovered)
        pWidget->pOnHover(pWidget->pOnHoverUserData);

    if (pWidget->pOnActive && pWidget->mActive)
        pWidget->pOnActive(pWidget->pOnActiveUserData);

    if (pWidget->pOnFocus && pWidget->mFocused)
        pWidget->pOnFocus(pWidget->pOnFocusUserData);

    if (pWidget->pOnEdited && pWidget->mEdited)
        pWidget->pOnEdited(pWidget->pOnEditedUserData);

    if (pWidget->pOnDeactivated && pWidget->mDeactivated)
        pWidget->pOnDeactivated(pWidget->pOnDeactivatedUserData);

    if (pWidget->pOnDeactivatedAfterEdit && pWidget->mDeactivatedAfterEdit)
        pWidget->pOnDeactivatedAfterEdit(pWidget->pOnDeactivatedAfterEditUserData);
}

// CollapsingHeaderWidget private functions
static void processCollapsingHeaderWidget(UIWidget* pWidget)
{
    CollapsingHeaderWidget* pOriginalWidget = (CollapsingHeaderWidget*)(pWidget->pWidget);

    if (pOriginalWidget->mPreviousCollapsed != pOriginalWidget->mCollapsed)
    {
        ImGui::SetNextItemOpen(pOriginalWidget->mCollapsed);
        pOriginalWidget->mPreviousCollapsed = pOriginalWidget->mCollapsed;
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_CollapsingHeader;
    if (pOriginalWidget->mDefaultOpen)
        flags |= ImGuiTreeNodeFlags_DefaultOpen;

    unsigned char labelBuf[MAX_LABEL_STR_LENGTH];
    bstring       label = bemptyfromarr(labelBuf);
    bformat(&label, "%s##%p", pWidget->mLabel, pOriginalWidget);

    if (!pOriginalWidget->mHeaderIsVisible || ImGui::CollapsingHeader((const char*)label.data, flags))
    {
        for (uint32_t i = 0; i < pOriginalWidget->mWidgetsCount; ++i)
        {
            UIWidget* widget = pOriginalWidget->pGroupedWidgets[i];
            processWidget(widget);
        }
    }

    processWidgetCallbacks(pWidget);
    bdestroy(&label);
}

// DebugTexturesWidget private functions
static void processDebugTexturesWidget(UIWidget* pWidget)
{
    DebugTexturesWidget* pOriginalWidget = (DebugTexturesWidget*)(pWidget->pWidget);

    for (uint32_t i = 0; i < pOriginalWidget->mTexturesCount; ++i)
    {
        Texture*  texture = (Texture*)pOriginalWidget->pTextures[i];
        ptrdiff_t id = pUserInterface->mMaxUIFonts + ((ptrdiff_t)pUserInterface->frameIdx * pUserInterface->mMaxDynamicUIUpdatesPerBatch +
                                                      pUserInterface->mDynamicTexturesCount++);
        hmput(pUserInterface->pTextureHashmap, id, texture);
        ImGui::Image((void*)id, pOriginalWidget->mTextureDisplaySize);
        ImGui::SameLine();
    }

    processWidgetCallbacks(pWidget);
}

// LabelWidget private functions
static void processLabelWidget(UIWidget* pWidget)
{
    ImGui::Text("%s", pWidget->mLabel);
    processWidgetCallbacks(pWidget);
}

// ColorLabelWidget private functions
static void processColorLabelWidget(UIWidget* pWidget)
{
    ColorLabelWidget* pOriginalWidget = (ColorLabelWidget*)(pWidget->pWidget);

    ImGui::TextColored(pOriginalWidget->mColor, "%s", pWidget->mLabel);
    processWidgetCallbacks(pWidget);
}

// HorizontalSpaceWidget private functions
static void processHorizontalSpaceWidget(UIWidget* pWidget)
{
    ImGui::SameLine();
    processWidgetCallbacks(pWidget);
}

// SeparatorWidget private functions
static void processSeparatorWidget(UIWidget* pWidget)
{
    ImGui::Separator();
    processWidgetCallbacks(pWidget);
}

// VerticalSeparatorWidget private functions
static void processVerticalSeparatorWidget(UIWidget* pWidget)
{
    VerticalSeparatorWidget* pOriginalWidget = (VerticalSeparatorWidget*)(pWidget->pWidget);

    for (uint32_t i = 0; i < pOriginalWidget->mLineCount; ++i)
    {
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    }

    processWidgetCallbacks(pWidget);
}

// ButtonWidget private functions
static void processButtonWidget(UIWidget* pWidget)
{
    unsigned char labelBuf[MAX_LABEL_STR_LENGTH];
    bstring       label = bemptyfromarr(labelBuf);
    COMPILE_ASSERT(sizeof(pWidget->pOnEdited) == sizeof(void*));
    bformat(&label, "%s##%p", pWidget->mLabel, (void*)pWidget->pOnEdited);
    ASSERT(!bownsdata(&label));

    if (pWidget->mSameLine)
    {
        ImGui::SameLine();
    }

    pWidget->mEdited = ImGui::Button((const char*)label.data);
    processWidgetCallbacks(pWidget);
}

// SliderFloatWidget private functions
static void processSliderFloatWidget(UIWidget* pWidget)
{
    SliderFloatWidget* pOriginalWidget = (SliderFloatWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    ImGui::Text("%s", pWidget->mLabel);
    ImGui::SliderFloatWithSteps(label, pOriginalWidget->pData, pOriginalWidget->mMin, pOriginalWidget->mMax, pOriginalWidget->mStep,
                                pOriginalWidget->mFormat);
    processWidgetCallbacks(pWidget);
}

// SliderFloat2Widget private functions
static void processSliderFloat2Widget(UIWidget* pWidget)
{
    SliderFloat2Widget* pOriginalWidget = (SliderFloat2Widget*)(pWidget->pWidget);

    ImGui::Text("%s", pWidget->mLabel);
    for (uint32_t i = 0; i < 2; ++i)
    {
        char label[MAX_LABEL_LENGTH];
        LABELID1(&pOriginalWidget->pData->operator[](i), label);

        ImGui::SliderFloatWithSteps(label, &pOriginalWidget->pData->operator[](i), pOriginalWidget->mMin[i], pOriginalWidget->mMax[i],
                                    pOriginalWidget->mStep[i], pOriginalWidget->mFormat);
        processWidgetCallbacks(pWidget);
    }
}

// SliderFloat3Widget private functions
static void processSliderFloat3Widget(UIWidget* pWidget)
{
    SliderFloat3Widget* pOriginalWidget = (SliderFloat3Widget*)(pWidget->pWidget);

    ImGui::Text("%s", pWidget->mLabel);
    for (uint32_t i = 0; i < 3; ++i)
    {
        char label[MAX_LABEL_LENGTH];
        LABELID1(&pOriginalWidget->pData->operator[](i), label);
        ImGui::SliderFloatWithSteps(label, &pOriginalWidget->pData->operator[](i), pOriginalWidget->mMin[i], pOriginalWidget->mMax[i],
                                    pOriginalWidget->mStep[i], pOriginalWidget->mFormat);
        processWidgetCallbacks(pWidget);
    }
}

// SliderFloat4Widget private functions
static void processSliderFloat4Widget(UIWidget* pWidget)
{
    SliderFloat4Widget* pOriginalWidget = (SliderFloat4Widget*)(pWidget->pWidget);

    ImGui::Text("%s", pWidget->mLabel);
    for (uint32_t i = 0; i < 4; ++i)
    {
        char label[MAX_LABEL_LENGTH];
        LABELID1(&pOriginalWidget->pData->operator[](i), label);
        ImGui::SliderFloatWithSteps(label, &pOriginalWidget->pData->operator[](i), pOriginalWidget->mMin[i], pOriginalWidget->mMax[i],
                                    pOriginalWidget->mStep[i], pOriginalWidget->mFormat);
        processWidgetCallbacks(pWidget);
    }
}

// SliderIntWidget private functions
static void processSliderIntWidget(UIWidget* pWidget)
{
    SliderIntWidget* pOriginalWidget = (SliderIntWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    ImGui::Text("%s", pWidget->mLabel);
    ImGui::SliderIntWithSteps(label, pOriginalWidget->pData, pOriginalWidget->mMin, pOriginalWidget->mMax, pOriginalWidget->mStep,
                              pOriginalWidget->mFormat);
    processWidgetCallbacks(pWidget);
}

// SliderUintWidget private functions
static void processSliderUintWidget(UIWidget* pWidget)
{
    SliderUintWidget* pOriginalWidget = (SliderUintWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    ImGui::Text("%s", pWidget->mLabel);
    ImGui::SliderIntWithSteps(label, (int32_t*)pOriginalWidget->pData, (int32_t)pOriginalWidget->mMin, (int32_t)pOriginalWidget->mMax,
                              (int32_t)pOriginalWidget->mStep, pOriginalWidget->mFormat);
    processWidgetCallbacks(pWidget);
}

// RadioButtonWidget private functions
static void processRadioButtonWidget(UIWidget* pWidget)
{
    RadioButtonWidget* pOriginalWidget = (RadioButtonWidget*)(pWidget->pWidget);
    unsigned char      labelBuf[MAX_LABEL_STR_LENGTH];
    bstring            label = bemptyfromarr(labelBuf);
    bformat(&label, "%s##%p", pWidget->mLabel, pOriginalWidget->pData);
    ASSERT(!bownsdata(&label));

    ImGui::RadioButton((const char*)label.data, pOriginalWidget->pData, pOriginalWidget->mRadioId);
    processWidgetCallbacks(pWidget);
}

// CheckboxWidget private functions
static void processCheckboxWidget(UIWidget* pWidget)
{
    CheckboxWidget* pOriginalWidget = (CheckboxWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    ImGui::Text("%s", pWidget->mLabel);
    ImGui::Checkbox(label, pOriginalWidget->pData);
    processWidgetCallbacks(pWidget);
}

// OneLineCheckboxWidget private functions
static void processOneLineCheckboxWidget(UIWidget* pWidget)
{
    OneLineCheckboxWidget* pOriginalWidget = (OneLineCheckboxWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    ImGui::Checkbox(label, pOriginalWidget->pData);
    ImGui::SameLine();
    ImGui::TextColored(pOriginalWidget->mColor, "%s", pWidget->mLabel);
    processWidgetCallbacks(pWidget);
}

// CursorLocationWidget private functions
static void processCursorLocationWidget(UIWidget* pWidget)
{
    CursorLocationWidget* pOriginalWidget = (CursorLocationWidget*)(pWidget->pWidget);

    ImGui::SetCursorPos(pOriginalWidget->mLocation);
    processWidgetCallbacks(pWidget);
}

// DropdownWidget private functions
static void processDropdownWidget(UIWidget* pWidget)
{
    DropdownWidget* pOriginalWidget = (DropdownWidget*)(pWidget->pWidget);

    if (pOriginalWidget->mCount == 0)
        return;

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    uint32_t* pCurrent = pOriginalWidget->pData;
    ImGui::Text("%s", pWidget->mLabel);
    ASSERT(pOriginalWidget->pNames);

    if (ImGui::BeginCombo(label, pOriginalWidget->pNames[*pCurrent]))
    {
        for (uint32_t i = 0; i < pOriginalWidget->mCount; ++i)
        {
            bool isSelected = (*pCurrent == i);

            if (ImGui::Selectable(pOriginalWidget->pNames[i], isSelected))
            {
                uint32_t prevValue = *pCurrent;
                *pCurrent = i;

                // Note that callbacks are sketchy with BeginCombo/EndCombo, so we manually process them here
                if (pWidget->pOnEdited)
                    pWidget->pOnEdited(pWidget->pOnEditedUserData);

                if (*pCurrent != prevValue)
                {
                    if (pWidget->pOnDeactivatedAfterEdit)
                        pWidget->pOnDeactivatedAfterEdit(pWidget->pOnDeactivatedAfterEditUserData);
                }
            }

            // Set the default focus to the currently selected item
            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

// ColumnWidget private functions
static void processColumnWidget(UIWidget* pWidget)
{
    ColumnWidget* pOriginalWidget = (ColumnWidget*)(pWidget->pWidget);

    // Test a simple 4 col table.
    ImGui::BeginColumns(pWidget->mLabel, (int)pOriginalWidget->mWidgetsCount,
                        ImGuiColumnsFlags_NoResize | ImGuiColumnsFlags_NoForceWithinWindow);

    for (uint32_t i = 0; i < pOriginalWidget->mWidgetsCount; ++i)
    {
        processWidget(pOriginalWidget->pPerColumnWidgets[i]);
        ImGui::NextColumn();
    }

    ImGui::EndColumns();

    processWidgetCallbacks(pWidget);
}

// ProgressBarWidget private functions
static void processProgressBarWidget(UIWidget* pWidget)
{
    ProgressBarWidget* pOriginalWidget = (ProgressBarWidget*)(pWidget->pWidget);

    size_t currProgress = *(pOriginalWidget->pData);
    ImGui::Text("%s", pWidget->mLabel);
    ImGui::ProgressBar((float)currProgress / pOriginalWidget->mMaxProgress);
    processWidgetCallbacks(pWidget);
}

// ColorSliderWidget private functions
static void processColorSliderWidget(UIWidget* pWidget)
{
    ColorSliderWidget* pOriginalWidget = (ColorSliderWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    float4* combo_color = pOriginalWidget->pData;

    float col[4] = { combo_color->x, combo_color->y, combo_color->z, combo_color->w };
    ImGui::Text("%s", pWidget->mLabel);
    if (ImGui::ColorEdit4(label, col, ImGuiColorEditFlags_AlphaPreview))
    {
        if (col[0] != combo_color->x || col[1] != combo_color->y || col[2] != combo_color->z || col[3] != combo_color->w)
        {
            *combo_color = col;
        }
    }
    processWidgetCallbacks(pWidget);
}

// HistogramWidget private functions
static void processHistogramWidget(UIWidget* pWidget)
{
    HistogramWidget* pOriginalWidget = (HistogramWidget*)(pWidget->pWidget);

    unsigned char labelBuf[MAX_LABEL_STR_LENGTH];
    bstring       label = bemptyfromarr(labelBuf);
    bformat(&label, "%s##%p", pWidget->mLabel, pOriginalWidget->pValues);
    ASSERT(!bownsdata(&label));

    ImGui::PlotHistogram((const char*)label.data, pOriginalWidget->pValues, pOriginalWidget->mCount, 0, pOriginalWidget->mHistogramTitle,
                         *(pOriginalWidget->mMinScale), *(pOriginalWidget->mMaxScale), pOriginalWidget->mHistogramSize);

    processWidgetCallbacks(pWidget);
}

// PlotLinesWidget private functions
void processPlotLinesWidget(UIWidget* pWidget)
{
    PlotLinesWidget* pOriginalWidget = (PlotLinesWidget*)(pWidget->pWidget);

    unsigned char labelBuf[MAX_LABEL_STR_LENGTH];
    bstring       label = bemptyfromarr(labelBuf);
    bformat(&label, "%s##%p", pWidget->mLabel, pOriginalWidget->mValues);
    ASSERT(!bownsdata(&label));

    ImGui::PlotLines((const char*)label.data, pOriginalWidget->mValues, pOriginalWidget->mNumValues, 0, pOriginalWidget->mTitle,
                     *(pOriginalWidget->mScaleMin), *(pOriginalWidget->mScaleMax), *(pOriginalWidget->mPlotScale));

    processWidgetCallbacks(pWidget);
}

// ColorPickerWidget private functions
void processColorPickerWidget(UIWidget* pWidget)
{
    ColorPickerWidget* pOriginalWidget = (ColorPickerWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    float4* combo_color = pOriginalWidget->pData;

    float col[4] = { combo_color->x, combo_color->y, combo_color->z, combo_color->w };
    ImGui::Text("%s", pWidget->mLabel);
    if (ImGui::ColorPicker4(label, col, ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_Float))
    {
        if (col[0] != combo_color->x || col[1] != combo_color->y || col[2] != combo_color->z || col[3] != combo_color->w)
        {
            *combo_color = col;
        }
    }
    processWidgetCallbacks(pWidget);
}

void processColor3PickerWidget(UIWidget* pWidget)
{
    Color3PickerWidget* pOriginalWidget = (Color3PickerWidget*)(pWidget->pWidget);

    char label[MAX_LABEL_LENGTH];
    LABELID1(pOriginalWidget->pData, label);

    float3* combo_color = pOriginalWidget->pData;

    float col[3] = { combo_color->x, combo_color->y, combo_color->z };
    ImGui::Text("%s", pWidget->mLabel);
    if (ImGui::ColorPicker3(label, col, ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_Float))
    {
        if (col[0] != combo_color->x || col[1] != combo_color->y || col[2] != combo_color->z)
        {
            *combo_color = col;
        }
    }
    processWidgetCallbacks(pWidget);
}

static int TextboxCallbackFunc(ImGuiInputTextCallbackData* data)
{
    TextboxWidget* pWidget = (TextboxWidget*)data->UserData;

    if (data->EventFlag & ImGuiInputTextFlags_CallbackAlways)
    {
        if (pWidget->mFlags & UI_TEXT_ENABLE_RESIZE)
        {
            bstring* pStr = pWidget->pText;
            ASSERT(data->Buf == (const char*)pStr->data);
            int res = balloc(pStr, data->BufTextLen + 1);
            ASSERT(res == BSTR_OK);
            data->Buf = (char*)pStr->data;
            data->BufSize = bmlen(pStr);
        }

        if (pWidget->pCallback)
            pWidget->pCallback(ImGui::GetIO().KeysDown);
    }

    return 0;
}

// TextboxWidget private functions
void processTextboxWidget(UIWidget* pWidget)
{
    TextboxWidget* pOriginalWidget = (TextboxWidget*)(pWidget->pWidget);
    bstring*       pText = pOriginalWidget->pText;

    ASSERT(biscstr(pText));
    char label[MAX_LABEL_LENGTH];
    LABELID1((const char*)pText->data, label);

    uint32_t flags = 0;
    if (pOriginalWidget->mFlags & UI_TEXT_AUTOSELECT_ALL)
        flags |= ImGuiInputTextFlags_AutoSelectAll;
    if (pOriginalWidget->pCallback)
        flags |= ImGuiInputTextFlags_CallbackAlways;

    ImGui::Text("%s", pWidget->mLabel);
    ImGui::InputText(label, (char*)pText->data, bmlen(pText), flags, TextboxCallbackFunc, pOriginalWidget);
    pText->slen = (int)strlen((const char*)pText->data);

    processWidgetCallbacks(pWidget);
}

// DynamicTextWidget private functions
void processDynamicTextWidget(UIWidget* pWidget)
{
    DynamicTextWidget* pOriginalWidget = (DynamicTextWidget*)(pWidget->pWidget);
    ASSERT(pOriginalWidget->pText && bconstisvalid(pOriginalWidget->pText));
    ImGui::TextColored(*(pOriginalWidget->pColor), "%s", pOriginalWidget->pText->data);
    processWidgetCallbacks(pWidget);
}

// FilledRectWidget private functions
void processFilledRectWidget(UIWidget* pWidget)
{
    FilledRectWidget* pOriginalWidget = (FilledRectWidget*)(pWidget->pWidget);

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    float2       pos = window->Pos - window->Scroll + pOriginalWidget->mPos;
    float2       pos2 = float2(pos.x + pOriginalWidget->mScale.x, pos.y + pOriginalWidget->mScale.y);

    ImGui::GetWindowDrawList()->AddRectFilled(pos, pos2, ImGui::GetColorU32(pOriginalWidget->mColor));

    processWidgetCallbacks(pWidget);
}

// DrawTextWidget private functions
void processDrawTextWidget(UIWidget* pWidget)
{
    DrawTextWidget* pOriginalWidget = (DrawTextWidget*)(pWidget->pWidget);

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    float2       pos = window->Pos - window->Scroll + pOriginalWidget->mPos;
    const float2 line_size = ImGui::CalcTextSize(pWidget->mLabel);

    ImGui::GetWindowDrawList()->AddText(pos, ImGui::GetColorU32(pOriginalWidget->mColor), pWidget->mLabel);

    ImRect bounding_box(pos, pos + line_size);
    ImGui::ItemSize(bounding_box);
    ImGui::ItemAdd(bounding_box, 0);

    processWidgetCallbacks(pWidget);
}

// DrawTooltipWidget private functions
void processDrawTooltipWidget(UIWidget* pWidget)
{
    DrawTooltipWidget* pOriginalWidget = (DrawTooltipWidget*)(pWidget->pWidget);

    if ((*(pOriginalWidget->mShowTooltip)) == true)
    {
        ImGui::BeginTooltip();

        ImGui::TextUnformatted(pOriginalWidget->mText);

        ImGui::EndTooltip();
    }

    processWidgetCallbacks(pWidget);
}

// DrawLineWidget private functions
void processDrawLineWidget(UIWidget* pWidget)
{
    DrawLineWidget* pOriginalWidget = (DrawLineWidget*)(pWidget->pWidget);

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    float2       pos1 = window->Pos - window->Scroll + pOriginalWidget->mPos1;
    float2       pos2 = window->Pos - window->Scroll + pOriginalWidget->mPos2;

    ImGui::GetWindowDrawList()->AddLine(pos1, pos2, ImGui::GetColorU32(pOriginalWidget->mColor));

    if (pOriginalWidget->mAddItem)
    {
        ImRect bounding_box(pos1, pos2);
        ImGui::ItemSize(bounding_box);
        ImGui::ItemAdd(bounding_box, 0);
    }

    processWidgetCallbacks(pWidget);
}

// DrawCurveWidget private functions
void processDrawCurveWidget(UIWidget* pWidget)
{
    DrawCurveWidget* pOriginalWidget = (DrawCurveWidget*)(pWidget->pWidget);

    ImGuiWindow* window = ImGui::GetCurrentWindow();

    for (uint32_t i = 0; i < pOriginalWidget->mNumPoints - 1; i++)
    {
        float2 pos1 = window->Pos - window->Scroll + pOriginalWidget->mPos[i];
        float2 pos2 = window->Pos - window->Scroll + pOriginalWidget->mPos[i + 1];
        ImGui::GetWindowDrawList()->AddLine(pos1, pos2, ImGui::GetColorU32(pOriginalWidget->mColor), pOriginalWidget->mThickness);
    }

    processWidgetCallbacks(pWidget);
}

// CustomWidget private functions
void processCustomWidget(UIWidget* pWidget)
{
    CustomWidget* pOriginalWidget = (CustomWidget*)(pWidget->pWidget);

    if (pOriginalWidget->pCallback)
        pOriginalWidget->pCallback(pOriginalWidget->pUserData);

    // Note that we do not process widget callbacks for custom widgets
}

void processWidget(UIWidget* pWidget)
{
    pWidget->mDisplayPosition = ImGui::GetCursorScreenPos();

    switch (pWidget->mType)
    {
    case WIDGET_TYPE_COLLAPSING_HEADER:
    {
        processCollapsingHeaderWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_DEBUG_TEXTURES:
    {
        processDebugTexturesWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_LABEL:
    {
        processLabelWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR_LABEL:
    {
        processColorLabelWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_HORIZONTAL_SPACE:
    {
        processHorizontalSpaceWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_SEPARATOR:
    {
        processSeparatorWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_VERTICAL_SEPARATOR:
    {
        processVerticalSeparatorWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_BUTTON:
    {
        processButtonWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT:
    {
        processSliderFloatWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT2:
    {
        processSliderFloat2Widget(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT3:
    {
        processSliderFloat3Widget(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT4:
    {
        processSliderFloat4Widget(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_INT:
    {
        processSliderIntWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_UINT:
    {
        processSliderUintWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_RADIO_BUTTON:
    {
        processRadioButtonWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_CHECKBOX:
    {
        processCheckboxWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_ONE_LINE_CHECKBOX:
    {
        processOneLineCheckboxWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_CURSOR_LOCATION:
    {
        processCursorLocationWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_DROPDOWN:
    {
        processDropdownWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_COLUMN:
    {
        processColumnWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_PROGRESS_BAR:
    {
        processProgressBarWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR_SLIDER:
    {
        processColorSliderWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_HISTOGRAM:
    {
        processHistogramWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_PLOT_LINES:
    {
        processPlotLinesWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR_PICKER:
    {
        processColorPickerWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR3_PICKER:
    {
        processColor3PickerWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_TEXTBOX:
    {
        processTextboxWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_DYNAMIC_TEXT:
    {
        processDynamicTextWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_FILLED_RECT:
    {
        processFilledRectWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_TEXT:
    {
        processDrawTextWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_TOOLTIP:
    {
        processDrawTooltipWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_LINE:
    {
        processDrawLineWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_DRAW_CURVE:
    {
        processDrawCurveWidget(pWidget);
        break;
    }

    case WIDGET_TYPE_CUSTOM:
    {
        processCustomWidget(pWidget);
        break;
    }

    default:
        ASSERT(0);
    }
}

void removeWidget(UIWidget* pWidget, bool freeUnderlying)
{
    if (freeUnderlying)
    {
        switch (pWidget->mType)
        {
        case WIDGET_TYPE_COLLAPSING_HEADER:
        {
            CollapsingHeaderWidget* pOriginalWidget = (CollapsingHeaderWidget*)(pWidget->pWidget);
            for (uint32_t i = 0; i < pOriginalWidget->mWidgetsCount; ++i)
                removeWidget(pOriginalWidget->pGroupedWidgets[i], true);
            break;
        }
        case WIDGET_TYPE_COLUMN:
        {
            ColumnWidget* pOriginalWidget = (ColumnWidget*)(pWidget->pWidget);
            for (ptrdiff_t i = 0; i < pOriginalWidget->mWidgetsCount; ++i)
                removeWidget(pOriginalWidget->pPerColumnWidgets[i], true);
            break;
        }

        case WIDGET_TYPE_DROPDOWN:
        case WIDGET_TYPE_DEBUG_TEXTURES:
        case WIDGET_TYPE_LABEL:
        case WIDGET_TYPE_COLOR_LABEL:
        case WIDGET_TYPE_HORIZONTAL_SPACE:
        case WIDGET_TYPE_SEPARATOR:
        case WIDGET_TYPE_VERTICAL_SEPARATOR:
        case WIDGET_TYPE_BUTTON:
        case WIDGET_TYPE_SLIDER_FLOAT:
        case WIDGET_TYPE_SLIDER_FLOAT2:
        case WIDGET_TYPE_SLIDER_FLOAT3:
        case WIDGET_TYPE_SLIDER_FLOAT4:
        case WIDGET_TYPE_SLIDER_INT:
        case WIDGET_TYPE_SLIDER_UINT:
        case WIDGET_TYPE_RADIO_BUTTON:
        case WIDGET_TYPE_CHECKBOX:
        case WIDGET_TYPE_ONE_LINE_CHECKBOX:
        case WIDGET_TYPE_CURSOR_LOCATION:
        case WIDGET_TYPE_PROGRESS_BAR:
        case WIDGET_TYPE_COLOR_SLIDER:
        case WIDGET_TYPE_HISTOGRAM:
        case WIDGET_TYPE_PLOT_LINES:
        case WIDGET_TYPE_COLOR_PICKER:
        case WIDGET_TYPE_COLOR3_PICKER:
        case WIDGET_TYPE_TEXTBOX:
        case WIDGET_TYPE_DYNAMIC_TEXT:
        case WIDGET_TYPE_FILLED_RECT:
        case WIDGET_TYPE_DRAW_TEXT:
        case WIDGET_TYPE_DRAW_TOOLTIP:
        case WIDGET_TYPE_DRAW_LINE:
        case WIDGET_TYPE_DRAW_CURVE:
        {
            break;
        }

        case WIDGET_TYPE_CUSTOM:
        {
            CustomWidget* pOriginalWidget = (CustomWidget*)(pWidget->pWidget);
            if (pOriginalWidget->pDestroyCallback)
                pOriginalWidget->pDestroyCallback(pOriginalWidget->pUserData);
            break;
        }

        default:
            ASSERT(0);
        }

        tf_free(pWidget->pWidget);
        pWidget->pWidget = NULL;
    }

    tf_free(pWidget);
    pWidget = NULL;
}

#endif // ENABLE_FORGE_UI

/****************************************************************************/
// MARK: - Dynamic UI Public Functions
/****************************************************************************/

UIWidget* uiAddDynamicWidgets(DynamicUIWidgets* pDynamicUI, const char* pLabel, const void* pWidget, WidgetType type)
{
#ifdef ENABLE_FORGE_UI
    UIWidget widget{};
    widget.mType = type;
    widget.pWidget = (void*)pWidget;
    strcpy(widget.mLabel, pLabel);

    return arrpush(pDynamicUI->mDynamicProperties, cloneWidget(&widget));
#else
    return NULL;
#endif
}

void uiShowDynamicWidgets(const DynamicUIWidgets* pDynamicUI, UIComponent* pGui)
{
#ifdef ENABLE_FORGE_UI
    for (ptrdiff_t i = 0; i < arrlen(pDynamicUI->mDynamicProperties); ++i)
    {
        UIWidget* pWidget = pDynamicUI->mDynamicProperties[i];
        UIWidget* pNewWidget = uiAddComponentWidget(pGui, pWidget->mLabel, pWidget->pWidget, pWidget->mType, false);
        cloneWidgetBase(pNewWidget, pWidget);
    }
#endif
}

void uiHideDynamicWidgets(const DynamicUIWidgets* pDynamicUI, UIComponent* pGui)
{
#ifdef ENABLE_FORGE_UI
    for (ptrdiff_t i = 0; i < arrlen(pDynamicUI->mDynamicProperties); i++)
    {
        // We should not erase the widgets in this for-loop, otherwise the IDs
        // in mDynamicPropHandles will not match once  UIComponent::mWidgets changes size.
        uiRemoveComponentWidget(pGui, pDynamicUI->mDynamicProperties[i]);
    }
#endif
}

void uiRemoveDynamicWidgets(DynamicUIWidgets* pDynamicUI)
{
#ifdef ENABLE_FORGE_UI
    for (ptrdiff_t i = 0; i < arrlen(pDynamicUI->mDynamicProperties); ++i)
    {
        removeWidget(pDynamicUI->mDynamicProperties[i], true);
    }

    arrfree(pDynamicUI->mDynamicProperties);
#endif
}

/****************************************************************************/
// MARK: - UI Component Public Functions
/****************************************************************************/

void uiAddComponent(const char* pTitle, const UIComponentDesc* pDesc, UIComponent** ppUIComponent)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(ppUIComponent);
    UIComponent* pComponent = (UIComponent*)(tf_calloc(1, sizeof(UIComponent)));
    pComponent->mHasCloseButton = false;
    pComponent->mFlags = GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE;
#if defined(TARGET_IOS) || defined(__ANDROID__)
    pComponent->mFlags |= GUI_COMPONENT_FLAGS_START_COLLAPSED;
#endif

#ifdef ENABLE_FORGE_FONTS
    // Functions not accessible via normal interface header
    extern void*    fntGetRawFontData(uint32_t fontID);
    extern uint32_t fntGetRawFontDataSize(uint32_t fontID);

    bool useDefaultFallbackFont = false;

    // Use Requested Forge Font
    void*    pFontBuffer = fntGetRawFontData(pDesc->mFontID);
    uint32_t fontBufferSize = fntGetRawFontDataSize(pDesc->mFontID);
    if (pFontBuffer)
    {
        // See if that specific font id and size is already in use, if so just reuse it
        ptrdiff_t cachedFontIndex = -1;
        for (ptrdiff_t i = 0; i < arrlen(pUserInterface->pCachedFontsArr); ++i)
        {
            if (pUserInterface->pCachedFontsArr[i].mFontId == pDesc->mFontID &&
                pUserInterface->pCachedFontsArr[i].mFontSize == pDesc->mFontSize)
            {
                cachedFontIndex = i;

                pComponent->pFont = pUserInterface->pCachedFontsArr[i].pFont;
                pComponent->mFontTextureIndex = (uint32_t)i;

                break;
            }
        }

        if (cachedFontIndex == -1) // didn't find that font in the cache
        {
            // Ensure we don't pass max amount of fonts
            if (arrlen(pUserInterface->pCachedFontsArr) < pUserInterface->mMaxUIFonts)
            {
                pComponent->mFontTextureIndex =
                    addImguiFont(pFontBuffer, fontBufferSize, NULL, pDesc->mFontID, pDesc->mFontSize, &pComponent->pFont);
            }
            else
            {
                LOGF(eWARNING, "uiAddComponent() has reached fonts capacity.  Consider increasing 'mMaxUIFonts' when initializing the "
                               "user interface.");
                useDefaultFallbackFont = true;
            }
        }
    }
    else
    {
        LOGF(eWARNING, "uiAddComponent() uses an unknown font id (%u).  Will fallback to default UI font.", pDesc->mFontID);
        useDefaultFallbackFont = true;
    }
#else
    useDefaultFallbackFont = true;
#endif

    if (useDefaultFallbackFont)
    {
        pComponent->pFont = pUserInterface->pDefaultFallbackFont;
        pComponent->mFontTextureIndex = 0;
    }

    pComponent->mInitialWindowRect = { pDesc->mStartPosition.getX(), pDesc->mStartPosition.getY(), pDesc->mStartSize.getX(),
                                       pDesc->mStartSize.getY() };

    pComponent->mActive = true;
    strcpy(pComponent->mTitle, pTitle);
    pComponent->mAlpha = 1.0f;
    arrpush(pUserInterface->mComponents, pComponent);

    *ppUIComponent = pComponent;
#endif
}

void uiRemoveComponent(UIComponent* pGui)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pGui);

    uiRemoveAllComponentWidgets(pGui);

    ptrdiff_t componentIndex = 0;
    for (ptrdiff_t i = 0; i < arrlen(pUserInterface->mComponents); ++i)
    {
        UIComponent* pComponent = pUserInterface->mComponents[i];
        if (pComponent == pGui)
            componentIndex = i;
    }

    if (componentIndex < arrlen(pUserInterface->mComponents))
    {
        uiRemoveAllComponentWidgets(pGui);
        arrdel(pUserInterface->mComponents, componentIndex);
        arrfree(pGui->mWidgets);
    }

    tf_free(pGui);
#endif
}

void uiSetComponentActive(UIComponent* pUIComponent, bool active)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pUIComponent);
    pUIComponent->mActive = active;
#endif
}

/****************************************************************************/
// MARK: - Public UIWidget Add/Remove Functions
/****************************************************************************/

UIWidget* uiAddComponentWidget(UIComponent* pGui, const char* pLabel, const void* pWidget, WidgetType type, bool clone /* = true*/)
{
#ifdef ENABLE_FORGE_UI
    UIWidget* pBaseWidget = (UIWidget*)tf_calloc(1, sizeof(UIWidget));
    pBaseWidget->mType = type;
    pBaseWidget->pWidget = (void*)pWidget;
    strcpy(pBaseWidget->mLabel, pLabel);

    arrpush(pGui->mWidgets, clone ? cloneWidget(pBaseWidget) : pBaseWidget);
    arrpush(pGui->mWidgetsClone, clone);

    if (clone)
        tf_free(pBaseWidget);

    return pGui->mWidgets[arrlen(pGui->mWidgets) - 1];
#else
    return NULL;
#endif
}

void uiRemoveComponentWidget(UIComponent* pGui, UIWidget* pWidget)
{
#ifdef ENABLE_FORGE_UI
    ptrdiff_t i;
    for (i = 0; i < arrlen(pGui->mWidgets); ++i)
    {
        if (pGui->mWidgets[i]->pWidget == pWidget->pWidget)
            break;
    }
    if (i < arrlen(pGui->mWidgets))
    {
        UIWidget* iterWidget = pGui->mWidgets[i];
        removeWidget(iterWidget, pGui->mWidgetsClone[i]);
        arrdel(pGui->mWidgetsClone, i);
        arrdel(pGui->mWidgets, i);
    }
#endif
}

void uiRemoveAllComponentWidgets(UIComponent* pGui)
{
#ifdef ENABLE_FORGE_UI
    for (ptrdiff_t i = 0; i < arrlen(pGui->mWidgets); ++i)
    {
        removeWidget(pGui->mWidgets[i], pGui->mWidgetsClone[i]); //-V595
    }

    arrfree(pGui->mWidgets);
    arrfree(pGui->mWidgetsClone);
#endif
}

/****************************************************************************/
// MARK: - Safe Public Setter Functions
/****************************************************************************/

void uiSetComponentFlags(UIComponent* pGui, int32_t flags)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pGui);

    pGui->mFlags = flags;
#endif
}

void uiSetWidgetDeferred(UIWidget* pWidget, bool deferred)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pWidget);

    pWidget->mDeferred = deferred;
#endif
}

void uiSetWidgetOnHoverCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pWidget);

    pWidget->pOnHoverUserData = pUserData;
    pWidget->pOnHover = callback;
#endif
}

void uiSetWidgetOnActiveCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pWidget);

    pWidget->pOnActiveUserData = pUserData;
    pWidget->pOnActive = callback;
#endif
}

void uiSetWidgetOnFocusCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pWidget);

    pWidget->pOnFocusUserData = pUserData;
    pWidget->pOnFocus = callback;
#endif
}

void uiSetWidgetOnEditedCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pWidget);

    pWidget->pOnEditedUserData = pUserData;
    pWidget->pOnEdited = callback;
#endif
}

void uiSetWidgetOnDeactivatedCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pWidget);

    pWidget->pOnDeactivatedUserData = pUserData;
    pWidget->pOnDeactivated = callback;
#endif
}

void uiSetWidgetOnDeactivatedAfterEditCallback(UIWidget* pWidget, void* pUserData, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pWidget);

    pWidget->pOnDeactivatedAfterEditUserData = pUserData;
    pWidget->pOnDeactivatedAfterEdit = callback;
#endif
}

/// Set whether or not a given UI Widget should be on the sameline than the previous one
FORGE_API void uiSetSameLine(UIWidget* pGuiComponent, bool sameLine)
{
#ifdef ENABLE_FORGE_UI
    pGuiComponent->mSameLine = sameLine;
#endif
}

void uiNewFrame()
{
#ifdef ENABLE_FORGE_UI
    pUserInterface->mDynamicTexturesCount = 0;
    ImGui::NewFrame();
#endif
}

void uiEndFrame() { ImGui::EndFrame(); }

/****************************************************************************/
// MARK: - Private Platform Layer Life Cycle Functions
/****************************************************************************/

static InputEnum gKeyMap[ImGuiKey_NamedKey_COUNT] = {};

bool platformInitUserInterface()
{
#ifdef ENABLE_FORGE_UI
    UserInterface* pAppUI = tf_new(UserInterface);

    pAppUI->mShowDemoUiWindow = false;

    pAppUI->mActive = true;

    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, pAppUI->dpiScale);

    //// init UI (input)
    ImGui::SetAllocatorFunctions(alloc_func, dealloc_func);
    pAppUI->context = ImGui::CreateContext();
    ImGui::SetCurrentContext(pAppUI->context);

    SetDefaultStyle();

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags = ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard;

    // Tell ImGui that we support ImDrawCmd::VtxOffset, otherwise ImGui will always set it to 0
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    extern void InputFillImguiKeyMap(InputEnum * keyMap);
    InputFillImguiKeyMap(gKeyMap);

    pUserInterface = pAppUI;
#endif

    return true;
}

void platformExitUserInterface()
{
#ifdef ENABLE_FORGE_UI
    for (ptrdiff_t i = 0; i < arrlen(pUserInterface->mComponents); ++i)
    {
        uiRemoveAllComponentWidgets(pUserInterface->mComponents[i]);
        tf_free(pUserInterface->mComponents[i]);
    }
    arrfree(pUserInterface->mComponents);

    ImGui::DestroyContext(pUserInterface->context);

    tf_delete(pUserInterface);
#endif
}

void platformUpdateUserInterface(float deltaTime)
{
#ifdef ENABLE_FORGE_UI
    // Render can me nullptr when initUserInterface wasn't called, this can happen when the build compiled using
    // ENABLED_FORGE_UI but then in runtime the App decided not to use the UI
    if (pUserInterface->pRenderer == nullptr)
    {
        return;
    }

    deltaTime = deltaTime == 0.0f ? 0.016f : deltaTime;

    ImGui::SetCurrentContext(pUserInterface->context);

    ImGuiIO& io = ImGui::GetIO();
    // Always show navigation highlight
    io.NavActive = true;
    io.NavVisible = true;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

    // #TODO: Use window size as render-target size cannot be trusted to be the same as window size
    io.DisplaySize.x = pUserInterface->mDisplayWidth;
    io.DisplaySize.y = pUserInterface->mDisplayHeight;
    io.DisplayFramebufferScale.x = pUserInterface->mWidth / pUserInterface->mDisplayWidth;
    io.DisplayFramebufferScale.y = pUserInterface->mHeight / pUserInterface->mDisplayHeight;
    io.FontGlobalScale = min(io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    io.DeltaTime = deltaTime;

    for (uint32_t k = ImGuiKey_NamedKey_BEGIN; k <= ImGuiKey_GamepadR3; ++k)
    {
        if (!gKeyMap[k - ImGuiKey_NamedKey_BEGIN])
        {
            continue;
        }
        io.AddKeyEvent((ImGuiKey)k, inputGetValue(0, gKeyMap[k - ImGuiKey_NamedKey_BEGIN]));
    }
    const float x = inputGetValue(0, gKeyMap[ImGuiKey_MouseX1 - ImGuiKey_NamedKey_BEGIN]) * io.DisplayFramebufferScale.x;
    const float y = inputGetValue(0, gKeyMap[ImGuiKey_MouseX2 - ImGuiKey_NamedKey_BEGIN]) * io.DisplayFramebufferScale.y;
    const float wheel = inputGetValue(0, gKeyMap[ImGuiKey_MouseWheelX - ImGuiKey_NamedKey_BEGIN]) -
                        inputGetValue(0, gKeyMap[ImGuiKey_MouseWheelY - ImGuiKey_NamedKey_BEGIN]);
#if defined(ENABLE_FORGE_TOUCH_INPUT)
    io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
#endif
    io.AddMousePosEvent(x, y);
    io.AddMouseWheelEvent(0.0f, wheel);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, inputGetValue(0, gKeyMap[ImGuiKey_MouseLeft - ImGuiKey_NamedKey_BEGIN]));
    io.AddMouseButtonEvent(ImGuiMouseButton_Right, inputGetValue(0, gKeyMap[ImGuiKey_MouseRight - ImGuiKey_NamedKey_BEGIN]));
    io.AddMouseButtonEvent(ImGuiMouseButton_Middle, inputGetValue(0, gKeyMap[ImGuiKey_MouseMiddle - ImGuiKey_NamedKey_BEGIN]));

    if (io.WantTextInput)
    {
        char32_t* chars;
        uint32_t  charCount;
        inputGetCharInput(&chars, &charCount);
        for (uint32_t i = 0; i < charCount; ++i)
        {
            io.AddInputCharacter(chars[i]);
        }
    }

    // (UIComponent*)[dyn_size]
    UIComponent** activeComponents = NULL;
    arrsetcap(activeComponents, arrlen(pUserInterface->mComponents));

    for (ptrdiff_t i = 0; i < arrlen(pUserInterface->mComponents); ++i)
        if (pUserInterface->mComponents[i]->mActive)
            arrpush(activeComponents, pUserInterface->mComponents[i]);

    if (arrlen(activeComponents) == 0)
    {
        if (arrcap(activeComponents) > 0)
            arrfree(activeComponents);
    }

    GUIDriverUpdate guiUpdate = {};
    guiUpdate.pUIComponents = activeComponents;
    guiUpdate.componentCount = (uint32_t)arrlenu(activeComponents);
    guiUpdate.deltaTime = deltaTime;
    guiUpdate.width = pUserInterface->mDisplayWidth;
    guiUpdate.height = pUserInterface->mDisplayHeight;
    guiUpdate.showDemoWindow = pUserInterface->mShowDemoUiWindow;

    uiNewFrame();

    if (pUserInterface->mActive)
    {
        if (guiUpdate.showDemoWindow)
            ImGui::ShowDemoWindow();

        pUserInterface->mLastUpdateCount = guiUpdate.componentCount;

        for (uint32_t compIndex = 0; compIndex < guiUpdate.componentCount; ++compIndex)
        {
            if (!guiUpdate.pUIComponents)
                continue;

            UIComponent*          pComponent = guiUpdate.pUIComponents[compIndex];
            char                  title[MAX_TITLE_STR_LENGTH] = { 0 };
            int32_t               UIComponentFlags = pComponent->mFlags;
            bool*                 pCloseButtonActiveValue = pComponent->mHasCloseButton ? &pComponent->mHasCloseButton : NULL;
            const char* const*    contextualMenuLabels = pComponent->mContextualMenuLabels;
            const WidgetCallback* contextualMenuCallbacks = pComponent->mContextualMenuCallbacks;
            const size_t          contextualMenuCount = pComponent->mContextualMenuCount;
            const float4*         pWindowRect = &pComponent->mInitialWindowRect;
            float4*               pCurrentWindowRect = &pComponent->mCurrentWindowRect;
            UIWidget**            pProps = pComponent->mWidgets;
            ptrdiff_t             propCount = arrlen(pComponent->mWidgets);

            strcpy(title, pComponent->mTitle);

            if (title[0] == '\0')
                snprintf(title, MAX_TITLE_STR_LENGTH, "##%llu", (unsigned long long)pComponent);
            // Setup the ImGuiWindowFlags
            ImGuiWindowFlags guiWinFlags = GUI_COMPONENT_FLAGS_NONE;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_TITLE_BAR)
                guiWinFlags |= ImGuiWindowFlags_NoTitleBar;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_RESIZE)
                guiWinFlags |= ImGuiWindowFlags_NoResize;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_MOVE)
                guiWinFlags |= ImGuiWindowFlags_NoMove;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_SCROLLBAR)
                guiWinFlags |= ImGuiWindowFlags_NoScrollbar;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_COLLAPSE)
                guiWinFlags |= ImGuiWindowFlags_NoCollapse;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE)
                guiWinFlags |= ImGuiWindowFlags_AlwaysAutoResize;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_INPUTS)
                guiWinFlags |= ImGuiWindowFlags_NoInputs;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_MEMU_BAR)
                guiWinFlags |= ImGuiWindowFlags_MenuBar;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_HORIZONTAL_SCROLLBAR)
                guiWinFlags |= ImGuiWindowFlags_HorizontalScrollbar;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_FOCUS_ON_APPEARING)
                guiWinFlags |= ImGuiWindowFlags_NoFocusOnAppearing;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_BRING_TO_FRONT_ON_FOCUS)
                guiWinFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_VERTICAL_SCROLLBAR)
                guiWinFlags |= ImGuiWindowFlags_AlwaysVerticalScrollbar;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_HORIZONTAL_SCROLLBAR)
                guiWinFlags |= ImGuiWindowFlags_AlwaysHorizontalScrollbar;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_USE_WINDOW_PADDING)
                guiWinFlags |= ImGuiWindowFlags_AlwaysUseWindowPadding;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_NAV_INPUT)
                guiWinFlags |= ImGuiWindowFlags_NoNavInputs;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_NAV_FOCUS)
                guiWinFlags |= ImGuiWindowFlags_NoNavFocus;
            if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_DOCKING)
                guiWinFlags |= ImGuiWindowFlags_NoDocking;

            ImGui::PushFont((ImFont*)pComponent->pFont);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, pComponent->mAlpha);

            if (pComponent->pPreProcessCallback)
                pComponent->pPreProcessCallback(pComponent->pUserData);

            bool result = ImGui::Begin(title, pCloseButtonActiveValue, guiWinFlags);
            if (result)
            {
                // Setup the contextual menus
                if (contextualMenuCount != 0 && ImGui::BeginPopupContextItem()) // <-- This is using IsItemHovered()
                {
                    for (size_t i = 0; i < contextualMenuCount; i++)
                    {
                        if (ImGui::MenuItem(contextualMenuLabels[i]))
                        {
                            if (contextualMenuCallbacks && contextualMenuCallbacks[i])
                                contextualMenuCallbacks[i](pComponent->pUserData);
                        }
                    }
                    ImGui::EndPopup();
                }

                bool overrideSize = false;
                bool overridePos = false;

                if ((UIComponentFlags & GUI_COMPONENT_FLAGS_NO_RESIZE) && !(UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE))
                    overrideSize = true;

                if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_MOVE)
                    overridePos = true;

                ImGui::SetWindowSize(float2(pWindowRect->z, pWindowRect->w), overrideSize ? ImGuiCond_Always : ImGuiCond_Once);
                ImGui::SetWindowPos(float2(pWindowRect->x, pWindowRect->y), overridePos ? ImGuiCond_Always : ImGuiCond_Once);

                if (UIComponentFlags & GUI_COMPONENT_FLAGS_START_COLLAPSED)
                    ImGui::SetWindowCollapsed(true, ImGuiCond_Once);

                for (ptrdiff_t i = 0; i < propCount; ++i)
                {
                    if (pProps[i] != nullptr)
                    {
                        processWidget(pProps[i]);
                    }
                }
            }

            float2 pos = ImGui::GetWindowPos();
            float2 size = ImGui::GetWindowSize();
            pCurrentWindowRect->x = pos.x;
            pCurrentWindowRect->y = pos.y;
            pCurrentWindowRect->z = size.x;
            pCurrentWindowRect->w = size.y;
            pUserInterface->mLastUpdateMin[compIndex] = pos;
            pUserInterface->mLastUpdateMax[compIndex] = pos + size;

            // Need to call ImGui::End event if result is false since we called ImGui::Begin
            ImGui::End();

            if (pComponent->pPostProcessCallback)
                pComponent->pPostProcessCallback(pComponent->pUserData);

            ImGui::PopStyleVar();
            ImGui::PopFont();
        }
    }

    uiEndFrame();

    if (pUserInterface->mActive)
    {
        for (uint32_t compIndex = 0; compIndex < guiUpdate.componentCount; ++compIndex)
        {
            if (!guiUpdate.pUIComponents)
                continue;

            UIComponent* pComponent = guiUpdate.pUIComponents[compIndex];
            UIWidget**   pProps = pComponent->mWidgets;
            ptrdiff_t    propCount = arrlen(pComponent->mWidgets);

            for (ptrdiff_t i = 0; i < propCount; ++i)
            {
                if (pProps[i] != nullptr)
                {
                    processWidgetCallbacks(pProps[i], true);
                }
            }
        }
    }

    arrfree(activeComponents);

    extern void updateProfilerUI();
    updateProfilerUI();
#endif
}

/****************************************************************************/
// MARK: - Private Static Reused Draw Functionalities
/****************************************************************************/
#if defined(ENABLE_FORGE_UI)
static void cmdPrepareRenderingForUI(Cmd* pCmd, const float2& displayPos, const float2& displaySize, Pipeline* pPipeline,
                                     const uint64_t vOffset, const uint64_t iOffset)
{
    const float                  L = displayPos.x;
    const float                  R = displayPos.x + displaySize.x;
    const float                  T = displayPos.y;
    const float                  B = displayPos.y + displaySize.y;
    alignas(alignof(mat4)) float mvp[4][4] = {
        { 2.0f / (R - L), 0.0f, 0.0f, 0.0f },
        { 0.0f, 2.0f / (T - B), 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.5f, 0.0f },
        { (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f },
    };

    BufferUpdateDesc update = { pUserInterface->pUniformBuffer[pUserInterface->frameIdx] };
    beginUpdateResource(&update);
    memcpy(update.pMappedData, &mvp, sizeof(mvp));
    endUpdateResource(&update);

    const uint32_t vertexStride = sizeof(ImDrawVert);

    cmdSetViewport(pCmd, 0.0f, 0.0f, displaySize.x, displaySize.y, 0.0f, 1.0f);
    cmdSetScissor(pCmd, (uint32_t)displayPos.x, (uint32_t)displayPos.y, (uint32_t)displaySize.x, (uint32_t)displaySize.y);

    cmdBindPipeline(pCmd, pPipeline);
    cmdBindIndexBuffer(pCmd, pUserInterface->pIndexBuffer, sizeof(ImDrawIdx) == sizeof(uint16_t) ? INDEX_TYPE_UINT16 : INDEX_TYPE_UINT32,
                       iOffset);
    cmdBindVertexBuffer(pCmd, 1, &pUserInterface->pVertexBuffer, &vertexStride, &vOffset);
    cmdBindDescriptorSet(pCmd, pUserInterface->frameIdx, pUserInterface->pDescriptorSetUniforms);
}

static void cmdDrawUICommand(Cmd* pCmd, const ImDrawCmd* pImDrawCmd, const float2& displayPos, const float2& displaySize,
                             Pipeline** ppPipelineInOut, Pipeline** ppPrevPipelineInOut, uint32_t& globalVtxOffsetInOut,
                             uint32_t& globalIdxOffsetInOut, uint32_t& prevSetIndexInOut, int32_t vertexCount, int32_t indexCount)
{
    //{
    //	const ImDrawCmd* pImDrawCmd = &pCmdList->CmdBuffer[i];
    //	if (pImDrawCmd->UserCallback)
    //	{
    //		// User callback (registered via ImDrawList::AddCallback)
    //		pImDrawCmd->UserCallback(pCmdList, pImDrawCmd);
    //	}
    //	else
    //	{
    //  Clamp to viewport as cmdSetScissor() won't accept values that are off bounds
    float2 clipMin = { clamp(pImDrawCmd->ClipRect.x - displayPos.x, 0.0f, displaySize.x),
                       clamp(pImDrawCmd->ClipRect.y - displayPos.y, 0.0f, displaySize.y) };
    float2 clipMax = { clamp(pImDrawCmd->ClipRect.z - displayPos.x, 0.0f, displaySize.x),
                       clamp(pImDrawCmd->ClipRect.w - displayPos.y, 0.0f, displaySize.y) };
    if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
    {
        return;
    }
    if (!pImDrawCmd->ElemCount)
    {
        return;
    }

    uint2 offset = { (uint32_t)clipMin.x, (uint32_t)clipMin.y };
    uint2 ext = { (uint32_t)(clipMax.x - clipMin.x), (uint32_t)(clipMax.y - clipMin.y) };
    cmdSetScissor(pCmd, offset.x, offset.y, ext.x, ext.y);

    ptrdiff_t id = (ptrdiff_t)pImDrawCmd->TextureId;
    uint32_t  setIndex = (uint32_t)id;
    if (id >= pUserInterface->mMaxUIFonts)
    {
        if (pUserInterface->mDynamicTexturesCount >= pUserInterface->mMaxDynamicUIUpdatesPerBatch)
        {
            LOGF(eWARNING,
                 "Too many dynamic UIs.  Consider increasing 'mMaxDynamicUIUpdatesPerBatch' when initializing the user interface.");
            return;
        }
        Texture* tex = hmgetp(pUserInterface->pTextureHashmap, id)->value;

        DescriptorData params[1] = {};
        params[0].pName = "uTex";
        params[0].ppTextures = &tex;
        updateDescriptorSet(pUserInterface->pRenderer, setIndex, pUserInterface->pDescriptorSetTexture, 1, params);

        uint32_t pipelineIndex = (uint32_t)log2(params[0].ppTextures[0]->mSampleCount);
        *ppPipelineInOut = pUserInterface->pPipelineTextured[pipelineIndex];
    }
    else
    {
        *ppPipelineInOut = pUserInterface->pPipelineTextured[0];
    }

    if (*ppPrevPipelineInOut != *ppPipelineInOut)
    {
        cmdBindPipeline(pCmd, *ppPipelineInOut);
        *ppPrevPipelineInOut = *ppPipelineInOut;
    }

    if (setIndex != prevSetIndexInOut)
    {
        cmdBindDescriptorSet(pCmd, setIndex, pUserInterface->pDescriptorSetTexture);
        prevSetIndexInOut = setIndex;
    }

    const uint32_t vtxSize = round_up(vertexCount * sizeof(ImDrawVert), pCmd->pRenderer->pGpu->mUploadBufferAlignment);
    const uint32_t idxSize = round_up(indexCount * sizeof(ImDrawIdx), pCmd->pRenderer->pGpu->mUploadBufferAlignment);
    cmdDrawIndexed(pCmd, pImDrawCmd->ElemCount, pImDrawCmd->IdxOffset + globalIdxOffsetInOut, pImDrawCmd->VtxOffset + globalVtxOffsetInOut);
    globalIdxOffsetInOut += idxSize / sizeof(ImDrawIdx);
    globalVtxOffsetInOut += round_up(vtxSize, sizeof(ImDrawVert)) / sizeof(ImDrawVert);
}

#endif // ENABLE_FORGE_UI

/****************************************************************************/
// MARK: - Public App Layer Life Cycle Functions
/****************************************************************************/

void initUserInterface(UserInterfaceDesc* pDesc)
{
#ifdef ENABLE_FORGE_UI
    pUserInterface->pRenderer = pDesc->pRenderer;
    pUserInterface->pPipelineCache = pDesc->pCache;
    pUserInterface->mMaxDynamicUIUpdatesPerBatch = pDesc->mMaxDynamicUIUpdatesPerBatch;
    pUserInterface->mMaxUIFonts = pDesc->mMaxUIFonts + 1; // +1 to account for a default fallback font
    pUserInterface->mFrameCount = pDesc->mFrameCount;
    ASSERT(pUserInterface->mFrameCount <= MAX_FRAMES);
    /************************************************************************/
    // Rendering resources
    /************************************************************************/
    SamplerDesc samplerDesc = { FILTER_LINEAR,
                                FILTER_LINEAR,
                                MIPMAP_MODE_NEAREST,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE };
    addSampler(pUserInterface->pRenderer, &samplerDesc, &pUserInterface->pDefaultSampler);

    BufferLoadDesc vbDesc = {};
    vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.mDesc.mSize = VERTEX_BUFFER_SIZE * pDesc->mFrameCount;
    vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    vbDesc.mDesc.pName = "UI Vertex Buffer";
    vbDesc.ppBuffer = &pUserInterface->pVertexBuffer;
    addResource(&vbDesc, NULL);

    BufferLoadDesc ibDesc = vbDesc;
    ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
    ibDesc.mDesc.mSize = INDEX_BUFFER_SIZE * pDesc->mFrameCount;
    vbDesc.mDesc.pName = "UI Index Buffer";
    ibDesc.ppBuffer = &pUserInterface->pIndexBuffer;
    addResource(&ibDesc, NULL);

    BufferLoadDesc ubDesc = {};
    ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    ubDesc.mDesc.mSize = sizeof(mat4);
    vbDesc.mDesc.pName = "UI Uniform Buffer";
    for (uint32_t i = 0; i < pDesc->mFrameCount; ++i)
    {
        ubDesc.ppBuffer = &pUserInterface->pUniformBuffer[i];
        addResource(&ubDesc, NULL);
    }

    VertexLayout* vertexLayout = &pUserInterface->mVertexLayoutTextured;
    vertexLayout->mBindingCount = 1;
    vertexLayout->mAttribCount = 3;
    vertexLayout->mAttribs[0].mSemantic = SEMANTIC_POSITION;
    vertexLayout->mAttribs[0].mFormat = TinyImageFormat_R32G32_SFLOAT;
    vertexLayout->mAttribs[0].mBinding = 0;
    vertexLayout->mAttribs[0].mLocation = 0;
    vertexLayout->mAttribs[0].mOffset = 0;
    vertexLayout->mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
    vertexLayout->mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
    vertexLayout->mAttribs[1].mBinding = 0;
    vertexLayout->mAttribs[1].mLocation = 1;
    vertexLayout->mAttribs[1].mOffset = TinyImageFormat_BitSizeOfBlock(pUserInterface->mVertexLayoutTextured.mAttribs[0].mFormat) / 8;
    vertexLayout->mAttribs[2].mSemantic = SEMANTIC_COLOR;
    vertexLayout->mAttribs[2].mFormat = TinyImageFormat_R8G8B8A8_UNORM;
    vertexLayout->mAttribs[2].mBinding = 0;
    vertexLayout->mAttribs[2].mLocation = 2;
    vertexLayout->mAttribs[2].mOffset =
        vertexLayout->mAttribs[1].mOffset + TinyImageFormat_BitSizeOfBlock(pUserInterface->mVertexLayoutTextured.mAttribs[1].mFormat) / 8;

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = pDesc->mSettingsFilename;

    if (pDesc->mEnableDocking)
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Add a default fallback font (at index 0)
    uint32_t fallbackFontTexId = addImguiFont(NULL, 0, NULL, UINT_MAX, 0.f, &pUserInterface->pDefaultFallbackFont);
    ASSERT(fallbackFontTexId == FALLBACK_FONT_TEXTURE_INDEX);
#endif
}

void exitUserInterface()
{
#ifdef ENABLE_FORGE_UI
    removeSampler(pUserInterface->pRenderer, pUserInterface->pDefaultSampler);

    removeResource(pUserInterface->pVertexBuffer);
    removeResource(pUserInterface->pIndexBuffer);
    for (uint32_t i = 0; i < pUserInterface->mFrameCount; ++i)
    {
        if (pUserInterface->pUniformBuffer[i])
        {
            removeResource(pUserInterface->pUniformBuffer[i]);
            pUserInterface->pUniformBuffer[i] = NULL;
        }
    }

    for (ptrdiff_t i = 0; i < arrlen(pUserInterface->pCachedFontsArr); ++i)
        removeResource(pUserInterface->pCachedFontsArr[i].pFontTex);

    arrfree(pUserInterface->pCachedFontsArr);
    hmfree(pUserInterface->pTextureHashmap);

    // Resources can no longer be used. Force ImGui to clear all use:
    {
        ImGui::SetCurrentContext(pUserInterface->context);
        uiNewFrame();
        uiEndFrame();
    }
#endif
}

#if defined(ENABLE_FORGE_TOUCH_INPUT)

extern void InputGetVirtualJoystickData(bool* outActive, bool* outPressed, float* outRadius, float* outDeadzone, float2* outStartPos,
                                        float2* outPos);
const char* gVirtualJoystickTextureName = "circlepad.tex";

bool loadVirtualJoystick(ReloadType loadType, TinyImageFormat colorFormat)
{
    bool active = false;
    InputGetVirtualJoystickData(&active, NULL, NULL, NULL, NULL, NULL);
    if (!active)
    {
        return true;
    }

    TextureLoadDesc loadDesc = {};
    SyncToken       token = {};
    loadDesc.pFileName = gVirtualJoystickTextureName;
    loadDesc.ppTexture = &pUserInterface->pVJTexture;
    // Textures representing color should be stored in SRGB or HDR format
    loadDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
    addResource(&loadDesc, &token);
    waitForToken(&token);

    Renderer* pRenderer = pUserInterface->pRenderer;
    if (loadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        if (loadType & RELOAD_TYPE_SHADER)
        {
            /************************************************************************/
            // Shader
            /************************************************************************/
            ShaderLoadDesc texturedShaderDesc = {};
            texturedShaderDesc.mVert.pFileName = "textured_mesh.vert";
            texturedShaderDesc.mFrag.pFileName = "textured_mesh.frag";
            addShader(pRenderer, &texturedShaderDesc, &pUserInterface->pVJShader);

            const char*       pStaticSamplerNames[] = { "uSampler" };
            RootSignatureDesc textureRootDesc = { &pUserInterface->pVJShader, 1 };
            textureRootDesc.mStaticSamplerCount = 1;
            textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
            textureRootDesc.ppStaticSamplers = &pUserInterface->pDefaultSampler;
            addRootSignature(pRenderer, &textureRootDesc, &pUserInterface->pVJRootSignature);
            pUserInterface->mVJRootConstantIndex = getDescriptorIndexFromName(pUserInterface->pVJRootSignature, "uRootConstants");

            DescriptorSetDesc descriptorSetDesc = { pUserInterface->pVJRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
            addDescriptorSet(pRenderer, &descriptorSetDesc, &pUserInterface->pVJDescriptorSet);
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
        pipelineDesc.pRootSignature = pUserInterface->pVJRootSignature;
        pipelineDesc.pShaderProgram = pUserInterface->pVJShader;
        pipelineDesc.pVertexLayout = &vertexLayout;
        addPipeline(pRenderer, &desc, &pUserInterface->pVJPipeline);
    }

    DescriptorData params[1] = {};
    params[0].pName = "uTex";
    params[0].ppTextures = &pUserInterface->pVJTexture;
    updateDescriptorSet(pRenderer, 0, pUserInterface->pVJDescriptorSet, 1, params);

    return true;
}

void unloadVirtualJoystick(ReloadType unloadType)
{
    bool active = false;
    InputGetVirtualJoystickData(&active, NULL, NULL, NULL, NULL, NULL);
    if (!active)
    {
        return;
    }

    removeResource(pUserInterface->pVJTexture);
    if (unloadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        Renderer* pRenderer = pUserInterface->pRenderer;
        removePipeline(pRenderer, pUserInterface->pVJPipeline);

        if (unloadType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSet(pRenderer, pUserInterface->pVJDescriptorSet);
            removeRootSignature(pRenderer, pUserInterface->pVJRootSignature);
            removeShader(pRenderer, pUserInterface->pVJShader);
        }
    }
}

void drawVirtualJoystick(Cmd* pCmd, const float4* color, uint64_t vOffset)
{
    bool   active = false;
    bool   pressed = false;
    float  radius = 0.0f;
    float  deadzone = 0.0f;
    float2 startPos = {};
    float2 pos = {};
    InputGetVirtualJoystickData(&active, &pressed, &radius, &deadzone, &startPos, &pos);
    if (uiIsFocused() || !pressed)
    {
        return;
    }
    struct RootConstants
    {
        float4 color;
        float2 scaleBias;
        int    _pad[2];
    } data = {};

    float2 renderSize = { (float)pUserInterface->mWidth, (float)pUserInterface->mHeight };
    float2 renderScale = { (float)pUserInterface->mWidth / pUserInterface->mDisplayWidth,
                           (float)pUserInterface->mHeight / pUserInterface->mDisplayHeight };

    cmdSetViewport(pCmd, 0.0f, 0.0f, renderSize[0], renderSize[1], 0.0f, 1.0f);
    cmdSetScissor(pCmd, 0u, 0u, (uint32_t)renderSize[0], (uint32_t)renderSize[1]);

    cmdBindPipeline(pCmd, pUserInterface->pVJPipeline);
    cmdBindDescriptorSet(pCmd, 0, pUserInterface->pVJDescriptorSet);
    data.color = *color;
    data.scaleBias = { 2.0f / (float)renderSize[0], -2.0f / (float)renderSize[1] };
    cmdBindPushConstants(pCmd, pUserInterface->pVJRootSignature, pUserInterface->mVJRootConstantIndex, &data);

    float extSide = radius;
    float intSide = radius * 0.5f;

    // Outer stick
    float2 joystickSize = float2(extSide) * renderScale;
    float2 joystickCenter = startPos * renderScale - float2(0.0f, renderSize.y * 0.1f);
    float2 joystickPos = joystickCenter - joystickSize * 0.5f;

    const uint32_t   vertexStride = sizeof(float4);
    BufferUpdateDesc updateDesc = { pUserInterface->pVertexBuffer, vOffset };
    beginUpdateResource(&updateDesc);
    TexVertex vertices[4] = {};
    // the last variable can be used to create a border
    MAKETEXQUAD(vertices, joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0);
    memcpy(updateDesc.pMappedData, vertices, sizeof(vertices));
    endUpdateResource(&updateDesc);
    cmdBindVertexBuffer(pCmd, 1, &pUserInterface->pVertexBuffer, &vertexStride, &vOffset);
    cmdDraw(pCmd, 4, 0);
    vOffset += sizeof(TexVertex) * 4;

    // Inner stick
    float2 stickPos = pos;
    float2 delta = pos - startPos;
    float  halfRad = (radius * 0.5f) - deadzone;
    if (length(delta) > halfRad)
    {
        stickPos = startPos + halfRad * normalize(delta);
    }
    joystickSize = float2(intSide) * renderScale;
    joystickCenter = stickPos * renderScale - float2(0.0f, renderSize.y * 0.1f);
    joystickPos = float2(joystickCenter.getX(), joystickCenter.getY()) - 0.5f * joystickSize;
    updateDesc = { pUserInterface->pVertexBuffer, vOffset };
    beginUpdateResource(&updateDesc);
    TexVertex verticesInner[4] = {};
    // the last variable can be used to create a border
    MAKETEXQUAD(verticesInner, joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0);
    memcpy(updateDesc.pMappedData, verticesInner, sizeof(verticesInner));
    endUpdateResource(&updateDesc);
    cmdBindVertexBuffer(pCmd, 1, &pUserInterface->pVertexBuffer, &vertexStride, &vOffset);
    cmdDraw(pCmd, 4, 0);
}
#endif

void loadUserInterface(const UserInterfaceLoadDesc* pDesc)
{
#ifdef ENABLE_FORGE_UI
    if (pDesc->mLoadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        if (pDesc->mLoadType & RELOAD_TYPE_SHADER)
        {
            const char* imguiFrag[SAMPLE_COUNT_COUNT] = {
                "imgui_SAMPLE_COUNT_1.frag", "imgui_SAMPLE_COUNT_2.frag",  "imgui_SAMPLE_COUNT_4.frag",
                "imgui_SAMPLE_COUNT_8.frag", "imgui_SAMPLE_COUNT_16.frag",
            };
            ShaderLoadDesc texturedShaderDesc = {};
            texturedShaderDesc.mVert = { "imgui.vert" };
            for (uint32_t s = 0; s < TF_ARRAY_COUNT(imguiFrag); ++s)
            {
                texturedShaderDesc.mFrag = { imguiFrag[s] };
                addShader(pUserInterface->pRenderer, &texturedShaderDesc, &pUserInterface->pShaderTextured[s]);
            }

            const char*       pStaticSamplerNames[] = { "uSampler" };
            RootSignatureDesc textureRootDesc = { pUserInterface->pShaderTextured, 1 };
            textureRootDesc.mStaticSamplerCount = 1;
            textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
            textureRootDesc.ppStaticSamplers = &pUserInterface->pDefaultSampler;
            addRootSignature(pUserInterface->pRenderer, &textureRootDesc, &pUserInterface->pRootSignatureTextured);

            textureRootDesc.mShaderCount = TF_ARRAY_COUNT(pUserInterface->pShaderTextured) - 1;
            textureRootDesc.ppShaders = pUserInterface->pShaderTextured + 1;
            addRootSignature(pUserInterface->pRenderer, &textureRootDesc, &pUserInterface->pRootSignatureTexturedMs);

            DescriptorSetDesc setDesc = { pUserInterface->pRootSignatureTextured, DESCRIPTOR_UPDATE_FREQ_PER_BATCH,
                                          pUserInterface->mMaxUIFonts +
                                              (pUserInterface->mMaxDynamicUIUpdatesPerBatch * pUserInterface->mFrameCount) };
            addDescriptorSet(pUserInterface->pRenderer, &setDesc, &pUserInterface->pDescriptorSetTexture);
            setDesc = { pUserInterface->pRootSignatureTextured, DESCRIPTOR_UPDATE_FREQ_NONE, pUserInterface->mFrameCount };
            addDescriptorSet(pUserInterface->pRenderer, &setDesc, &pUserInterface->pDescriptorSetUniforms);

            for (uint32_t i = 0; i < pUserInterface->mFrameCount; ++i)
            {
                DescriptorData params[1] = {};
                params[0].pName = "uniformBlockVS";
                params[0].ppBuffers = &pUserInterface->pUniformBuffer[i];
                updateDescriptorSet(pUserInterface->pRenderer, i, pUserInterface->pDescriptorSetUniforms, 1, params);
            }
        }

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
        desc.pCache = pUserInterface->pPipelineCache;
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineDesc = desc.mGraphicsDesc;
        pipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        pipelineDesc.mRenderTargetCount = 1;
        pipelineDesc.mSampleCount = SAMPLE_COUNT_1;
        pipelineDesc.pBlendState = &blendStateDesc;
        pipelineDesc.mSampleQuality = 0;
        pipelineDesc.pColorFormats = (TinyImageFormat*)&pDesc->mColorFormat;
        pipelineDesc.pDepthState = &depthStateDesc;
        pipelineDesc.pRasterizerState = &rasterizerStateDesc;
        pipelineDesc.pRootSignature = pUserInterface->pRootSignatureTextured;
        pipelineDesc.pVertexLayout = &pUserInterface->mVertexLayoutTextured;
        pipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineDesc.mVRFoveatedRendering = true;
        for (uint32_t s = 0; s < TF_ARRAY_COUNT(pUserInterface->pShaderTextured); ++s)
        {
            pipelineDesc.pShaderProgram = pUserInterface->pShaderTextured[s];
            if (s > 0)
            {
                pipelineDesc.pRootSignature = pUserInterface->pRootSignatureTexturedMs;
            }
            addPipeline(pUserInterface->pRenderer, &desc, &pUserInterface->pPipelineTextured[s]);
        }
    }

    if (pDesc->mLoadType & RELOAD_TYPE_RESIZE)
    {
        pUserInterface->mWidth = (float)pDesc->mWidth;
        pUserInterface->mHeight = (float)pDesc->mHeight;
        pUserInterface->mDisplayWidth = pDesc->mDisplayWidth == 0 ? pUserInterface->mWidth : (float)pDesc->mDisplayWidth;
        pUserInterface->mDisplayHeight = pDesc->mDisplayHeight == 0 ? pUserInterface->mHeight : (float)pDesc->mDisplayHeight;
    }

    for (ptrdiff_t tex = 0; tex < arrlen(pUserInterface->pCachedFontsArr); ++tex)
    {
        DescriptorData params[1] = {};
        params[0].pName = "uTex";
        params[0].ppTextures = &pUserInterface->pCachedFontsArr[tex].pFontTex;
        updateDescriptorSet(pUserInterface->pRenderer, (uint32_t)tex, pUserInterface->pDescriptorSetTexture, 1, params);
    }

#if defined(ENABLE_FORGE_TOUCH_INPUT)
    loadVirtualJoystick((ReloadType)pDesc->mLoadType, (TinyImageFormat)pDesc->mColorFormat);
#endif
#endif
}

void unloadUserInterface(uint32_t unloadType)
{
#ifdef ENABLE_FORGE_UI
#if defined(ENABLE_FORGE_TOUCH_INPUT)
    unloadVirtualJoystick((ReloadType)unloadType);
#endif

    if (unloadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        for (uint32_t s = 0; s < TF_ARRAY_COUNT(pUserInterface->pShaderTextured); ++s)
        {
            removePipeline(pUserInterface->pRenderer, pUserInterface->pPipelineTextured[s]);
        }

        if (unloadType & RELOAD_TYPE_SHADER)
        {
            for (uint32_t s = 0; s < TF_ARRAY_COUNT(pUserInterface->pShaderTextured); ++s)
            {
                removeShader(pUserInterface->pRenderer, pUserInterface->pShaderTextured[s]);
            }
            removeDescriptorSet(pUserInterface->pRenderer, pUserInterface->pDescriptorSetTexture);
            removeDescriptorSet(pUserInterface->pRenderer, pUserInterface->pDescriptorSetUniforms);
            removeRootSignature(pUserInterface->pRenderer, pUserInterface->pRootSignatureTextured);
            removeRootSignature(pUserInterface->pRenderer, pUserInterface->pRootSignatureTexturedMs);
        }
    }
#endif
}

void cmdDrawUserInterface(Cmd* pCmd)
{
#ifdef ENABLE_FORGE_UI

    // Early return if UI rendering has been disabled
    if (!pUserInterface->mEnableRendering)
    {
        return;
    }

    ImGui::SetCurrentContext(pUserInterface->context);
    ImGui::Render();
    ImDrawData* pImDrawData = ImGui::GetDrawData();

    float2 displayPos(0.f, 0.f);
    float2 displaySize(0.f, 0.f);

    displayPos = pImDrawData->DisplayPos;
    displaySize = pImDrawData->DisplaySize;

    uint64_t vSize = pImDrawData->TotalVtxCount * sizeof(ImDrawVert);
    uint64_t iSize = pImDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    vSize = min<uint64_t>(vSize, VERTEX_BUFFER_SIZE);
    iSize = min<uint64_t>(iSize, INDEX_BUFFER_SIZE);

    uint64_t vOffset = pUserInterface->frameIdx * VERTEX_BUFFER_SIZE;
    uint64_t iOffset = pUserInterface->frameIdx * INDEX_BUFFER_SIZE;

    if (pImDrawData->TotalVtxCount > FORGE_UI_MAX_VERTEXES || pImDrawData->TotalIdxCount > FORGE_UI_MAX_INDEXES)
    {
        LOGF(eWARNING, "UI exceeds amount of verts/inds.  Consider updating FORGE_UI_MAX_VERTEXES/FORGE_UI_MAX_INDEXES defines.");
        LOGF(eWARNING, "Num verts: %d (max %d) | Num inds: %d (max %d)", pImDrawData->TotalVtxCount, FORGE_UI_MAX_VERTEXES,
             pImDrawData->TotalIdxCount, FORGE_UI_MAX_INDEXES);
        pImDrawData->TotalVtxCount =
            pImDrawData->TotalVtxCount > FORGE_UI_MAX_VERTEXES ? FORGE_UI_MAX_VERTEXES : pImDrawData->TotalVtxCount;
        pImDrawData->TotalIdxCount = pImDrawData->TotalIdxCount > FORGE_UI_MAX_INDEXES ? FORGE_UI_MAX_INDEXES : pImDrawData->TotalIdxCount;
    }

    uint64_t vtxDst = vOffset;
    uint64_t idxDst = iOffset;

    // Use regular draws for client

    for (int32_t i = 0; i < pImDrawData->CmdListsCount; i++)
    {
        const ImDrawList* pCmdList = pImDrawData->CmdLists[i];
        const uint64_t    vtxSize =
            round_up_64(pCmdList->VtxBuffer.size() * sizeof(ImDrawVert), pCmd->pRenderer->pGpu->mUploadBufferAlignment);
        const uint64_t idxSize = round_up_64(pCmdList->IdxBuffer.size() * sizeof(ImDrawIdx), pCmd->pRenderer->pGpu->mUploadBufferAlignment);
        BufferUpdateDesc update = { pUserInterface->pVertexBuffer, vtxDst, vtxSize };
        beginUpdateResource(&update);
        memcpy(update.pMappedData, pCmdList->VtxBuffer.Data, pCmdList->VtxBuffer.size() * sizeof(ImDrawVert));
        endUpdateResource(&update);

        update = { pUserInterface->pIndexBuffer, idxDst, idxSize };
        beginUpdateResource(&update);
        memcpy(update.pMappedData, pCmdList->IdxBuffer.Data, pCmdList->IdxBuffer.size() * sizeof(ImDrawIdx));
        endUpdateResource(&update);

        // Round up in case the buffer alignment is not a multiple of vertex/index size
        vtxDst += round_up_64(vtxSize, sizeof(ImDrawVert));
        idxDst += round_up_64(idxSize, sizeof(ImDrawIdx));
    }

    Pipeline* pPipeline = pUserInterface->pPipelineTextured[0];
    Pipeline* pPreviousPipeline = pPipeline;
    uint32_t  prevSetIndex = UINT32_MAX;

    cmdPrepareRenderingForUI(pCmd, displayPos, displaySize, pPipeline, vOffset, iOffset);

    // Render command lists
    uint32_t globalVtxOffset = 0;
    uint32_t globalIdxOffset = 0;

    {
        for (int n = 0; n < pImDrawData->CmdListsCount; n++)
        {
            const ImDrawList* pCmdList = pImDrawData->CmdLists[n];

            for (int c = 0; c < pCmdList->CmdBuffer.size(); c++)
            {
                const ImDrawCmd* pImDrawCmd = &pCmdList->CmdBuffer[c];

                if (pImDrawCmd->UserCallback)
                {
                    // User callback (registered via ImDrawList::AddCallback)
                    pImDrawCmd->UserCallback(pCmdList, pImDrawCmd);
                    continue;
                }

                int32_t vertexCount, indexCount;
                if (c == pCmdList->CmdBuffer.size() - 1)
                {
                    vertexCount = pCmdList->VtxBuffer.size();
                    indexCount = pCmdList->IdxBuffer.size();
                }
                else
                {
                    vertexCount = 0;
                    indexCount = 0;
                }
                cmdDrawUICommand(pCmd, pImDrawCmd, displayPos, displaySize, &pPipeline, &pPreviousPipeline, globalVtxOffset,
                                 globalIdxOffset, prevSetIndex, vertexCount, indexCount);
            }
        }
    }

    pUserInterface->frameIdx = (pUserInterface->frameIdx + 1) % pUserInterface->mFrameCount;

#if defined(ENABLE_FORGE_TOUCH_INPUT)
    float4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    drawVirtualJoystick(pCmd, &color, vtxDst);
#endif
#endif
}

void uiOnText(const wchar_t* pText)
{
#ifdef ENABLE_FORGE_UI
    ImGui::SetCurrentContext(pUserInterface->context);
    ImGuiIO& io = ImGui::GetIO();
    uint32_t len = (uint32_t)wcslen(pText);
    for (uint32_t i = 0; i < len; ++i)
        io.AddInputCharacter(pText[i]);
#endif
}

uint8_t uiWantTextInput()
{
#ifdef ENABLE_FORGE_UI
    ImGui::SetCurrentContext(pUserInterface->context);
    // The User flags are not what I expect them to be.
    // We need access to Per-Component InputFlags
    ImGuiContext*       guiContext = (ImGuiContext*)pUserInterface->context;
    ImGuiInputTextFlags currentInputFlags = guiContext->InputTextState.Flags;

    // 0 -> Not pressed
    // 1 -> Digits Only keyboard
    // 2 -> Full Keyboard (Chars + Digits)
    int inputState = ImGui::GetIO().WantTextInput ? 2 : 0;
    // keyboard only Numbers
    if (inputState > 0 && (currentInputFlags & ImGuiInputTextFlags_CharsDecimal))
    {
        inputState = 1;
    }

    return (uint8_t)inputState;
#else
    return 0;
#endif
}

bool uiIsFocused()
{
#ifdef ENABLE_FORGE_UI
    if (!pUserInterface->mActive)
    {
        return false;
    }
    ImGui::SetCurrentContext(pUserInterface->context);
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse || io.NavVisible;
#else
    return false;
#endif
}

void uiToggleActive()
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pUserInterface);
    pUserInterface->mActive = !pUserInterface->mActive;
#endif
}

void uiToggleRendering(bool enabled)
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pUserInterface);
    pUserInterface->mEnableRendering = enabled;
#endif
}

bool uiIsRenderingEnabled()
{
#ifdef ENABLE_FORGE_UI
    ASSERT(pUserInterface);
    return pUserInterface->mEnableRendering;
#else
    return false;
#endif
}

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

#include "../Application/ThirdParty/OpenSource/imgui/imgui.h"
#include "../Application/ThirdParty/OpenSource/imgui/imgui_internal.h"

#include "../Application/Interfaces/IUI.h"
#include "../Resources/ResourceLoader/Interfaces/IResourceLoader.h"

typedef struct BufferAllocatorPlotWidget
{
    float2      mSize = float2(0.f, 0.f);
    const char* pName = NULL;
    int64_t*    pValues = NULL;
} BufferAllocatorPlotWidget;

namespace ImGui
{
// Copied ImGui::PlotEx and modified it to be able to have different colors on each histogram bar to represent fragmentation.
static int PlotBufferChunkAllocatorHistogram(const char* label, const void* data, int values_count, int values_offset,
                                             const char* overlay_text, ImVec2 frame_size)
{
    UNREF_PARAM(values_offset);
    ImGuiContext& g = *GImGui;
    g.CurrentWindow->WriteAccessed = true;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return -1;

    const int64_t* intData = (const int64_t*)data;

    const ImGuiStyle& style = g.Style;
    const ImGuiID     id = window->GetID(label);

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    if (frame_size.x == 0.0f)
        frame_size.x = CalcItemWidth();
    if (frame_size.y == 0.0f)
        frame_size.y = label_size.y + (style.FramePadding.y * 2);

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
    const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, 0, &frame_bb))
        return -1;
    const bool hovered = ItemHoverable(frame_bb, id, ImGuiItemFlags_None);

    // TheForge: Custom
    // We don't care about scale for this plot
    const float scale_min = 0.f;
    const float scale_max = 1.f;

    RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    const int values_count_min = 1;
    int       idx_hovered = -1;
    if (values_count >= values_count_min)
    {
        int res_w = ImMin((int)frame_size.x, values_count);
        int item_count = values_count;

        // Tooltip on hover
        if (hovered && inner_bb.Contains(g.IO.MousePos))
        {
            const float t = ImClamp((g.IO.MousePos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
            const int   v_idx = (int)(t * item_count);
            IM_ASSERT(v_idx >= 0 && v_idx < values_count);

            // TheForge: Custom
            {
                long long lb = intData[v_idx * 2];
                long long intensity = intData[v_idx * 2 + 1];
                long long rb = intData[v_idx * 2 + 2];

                if (lb < 0)
                    lb = -lb;
                if (rb < 0)
                    rb = -rb;

                long long size = rb - lb;
                SetTooltip("[%lli;%lli) %lli; [%s;%s) %s; free chunk count: %lli", lb, rb, size, humanReadableSize(lb).str,
                           humanReadableSize(rb).str, humanReadableSize(size).str, intensity);
            }
            idx_hovered = v_idx;
        }

        const float t_step = 1.0f / (float)res_w;
        const float inv_scale = (1.0f / (scale_max - scale_min));

        float  v0 = (float)intData[0]; // TheForge: Custom
        float  t0 = 0.0f;
        ImVec2 tp0 = ImVec2(t0, 1.0f - ImSaturate((v0 - scale_min) * inv_scale)); // Point in the normalized space of our target rectangle
        float  histogram_zero_line_t = (scale_min * scale_max < 0.0f) ? (1 + scale_min * inv_scale)
                                                                      : (scale_min < 0.0f ? 0.0f : 1.0f); // Where does the zero line stands

        ImU32       col_base = GetColorU32(ImGuiCol_PlotHistogram);
        const ImU32 col_hovered = GetColorU32(ImGuiCol_PlotHistogramHovered);

        for (int n = 0; n < res_w; n++)
        {
            const float t1 = t0 + t_step;
            const int   v1_idx = (int)(t0 * item_count + 0.5f);
            IM_ASSERT(v1_idx >= 0 && v1_idx < values_count);

            float v1 = 0.f;

            // TheForge: Custom
            {
                v1 = (float)intData[(v1_idx + 1) * 2];

                int64_t intensity = intData[v1_idx * 2 + 1];

                // set redness based on intensity
                uint8_t blue = 0x33;
                uint8_t green = 0x99;
                uint8_t red = 0xff;
                while (intensity > 0 && red > 0)
                {
                    red /= 2;
                    blue /= 2;
                    green /= 2;
                    --intensity;
                }
                red = 255 - red;

                col_base = 0xff000000 + (blue << (uint32_t)16) + (green << (uint32_t)8) + red;
            }

            const ImVec2 tp1 = ImVec2(t1, 1.0f - ImSaturate((v1 - scale_min) * inv_scale));

            // NB: Draw calls are merged together by the DrawList system. Still, we should render our batch are lower level to save a bit of
            // CPU.
            ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max, tp0);
            ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max, ImVec2(tp1.x, histogram_zero_line_t));

            {
                if (pos1.x >= pos0.x + 2.0f)
                    pos1.x -= 1.0f;
                window->DrawList->AddRectFilled(pos0, pos1, idx_hovered == v1_idx ? col_hovered : col_base);
            }

            t0 = t1;
            tp0 = tp1;
        }
    }

    // Text overlay
    if (overlay_text)
        RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, overlay_text, NULL, NULL,
                          ImVec2(0.5f, 0.0f));

    if (label_size.x > 0.0f)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label);

    // Return hovered index or -1 if none are hovered.
    // This is currently not exposed in the public API because we need a larger redesign of the whole thing, but in the short-term we are
    // making it available in PlotEx().
    return idx_hovered;
}
} // namespace ImGui

static void internalProcessBufferAllocatorPlotWidget(void* pUserData)
{
    BufferAllocatorPlotWidget* pPlotWidget = (BufferAllocatorPlotWidget*)pUserData;

    int            nValues = (int)pPlotWidget->mSize[0];
    const int64_t* values = pPlotWidget->pValues;

    if (!values)
        return; // uiSetWidgetAllocatorPlotBufferChunkAllocatorData wasn't called yet

    unsigned char labelBuf[MAX_LABEL_STR_LENGTH];
    bstring       label = bemptyfromarr(labelBuf);
    bformat(&label, "%s##%p", pPlotWidget->pName, values);
    ASSERT(!bownsdata(&label));

    char title[1024];

    size_t tlen = strlen(pPlotWidget->pName);

    if (tlen > 1023)
        tlen = 1023;
    memcpy(title, pPlotWidget->pName, tlen);
    title[tlen] = 0;

    if (tlen < 900)
    {
        snprintf(title + tlen, TF_ARRAY_COUNT(title) - tlen, " %s; fragments: %lli", humanReadableSize((size_t)values[0]).str,
                 (long long)values[1]);
        title[tlen++] = ' ';
    }

    ImGui::PlotBufferChunkAllocatorHistogram((const char*)label.data, values + 2, nValues, 0, title, pPlotWidget->mSize);
}

static void internalDestroyBufferAllocatorPlotWidget(void* pUserData)
{
    BufferAllocatorPlotWidget* pPlotWidget = (BufferAllocatorPlotWidget*)pUserData;
    tf_free(pPlotWidget->pValues);
    pPlotWidget->pValues = NULL;
}

UIWidget* uiCreateBufferChunkAllocatorPlotWidget(UIComponent* pGui, const char* pLabel, BufferAllocatorPlotWidget* pUserData)
{
    CustomWidget customWidget = {};
    customWidget.pCallback = internalProcessBufferAllocatorPlotWidget;
    customWidget.pUserData = pUserData;
    customWidget.pDestroyCallback = internalDestroyBufferAllocatorPlotWidget;
    return uiCreateComponentWidget(pGui, pLabel, &customWidget, WIDGET_TYPE_CUSTOM);
}

static inline uint64_t histogramPointOffsetAproximation(uint32_t point, uint32_t pointCount, uint64_t bufferSize)
{
    return (point * bufferSize) / pointCount;
}

/// Update data from BufferChunkAllocator once
/// Caller takes care of race conditions
/// Widget must be of type CustomWidget and pUserData needs to point to BufferAllocatorPlotWidget. If you created the widget using
/// uiCreateBufferChunkAllocatorPlotWidget it's okay
static void uiSetWidgetAllocatorPlotBufferChunkAllocatorData(UIWidget* pWidget, float2 size, struct BufferChunkAllocator* data)
{
    ASSERT(pWidget);
    ASSERT(pWidget->mType == WIDGET_TYPE_CUSTOM);
    if (pWidget->mType != WIDGET_TYPE_CUSTOM) //-V547
        return;

    CustomWidget*              pCustomWidget = (CustomWidget*)pWidget->pWidget;
    BufferAllocatorPlotWidget* pPlotWidget = (BufferAllocatorPlotWidget*)pCustomWidget->pUserData;

    if (pPlotWidget->mSize.x != size.x || pPlotWidget->pValues == NULL)
    {
        tf_free(pPlotWidget->pValues);

        const uint32_t nValues = (uint32_t)size[0];
        const size_t   allocSize = sizeof(int64_t) * 2 * (nValues + 2);
        pPlotWidget->pValues = (int64_t*)tf_calloc(1, allocSize);
        memset(pPlotWidget->pValues, 0, allocSize);
    }

    pPlotWidget->mSize = size;

    uint32_t nValues = (uint32_t)pPlotWidget->mSize[0];
    int64_t* values = pPlotWidget->pValues;

    uint32_t unusedChunkCount = (uint32_t)arrlenu(data->mUnusedChunks);

    values[0] = (int64_t)data->mSize;
    ++values;

    int64_t* fragmentCount = values;
    ++values;

    uint32_t point = 0;
    int64_t  intensity = 0;

    int64_t floatingOccupiedChunks = unusedChunkCount + 1;

    for (uint32_t ci = 0; ci < unusedChunkCount; ++ci)
    {
        BufferChunk* freeChunk = data->mUnusedChunks + ci;

        if (ci == 0 && freeChunk->mOffset == 0)
            floatingOccupiedChunks -= 1;
        if (ci == unusedChunkCount - 1 && freeChunk->mOffset + freeChunk->mSize == data->mSize)
            floatingOccupiedChunks -= 1;

        uint64_t point_beg = 0;

        // while we are on occupied zone
        for (; point < nValues; ++point)
        {
            point_beg = histogramPointOffsetAproximation(point, nValues, data->mSize);

            if (point_beg >= freeChunk->mOffset)
                break;

            values[2 * point] = (int64_t)point_beg;
            values[2 * point + 1] = intensity;

            intensity = 0;
        }

        // we hit free zone, add free chunk in a point
        ++intensity;

        // while we are in the free chunk
        for (uint64_t point_end = point_beg; point < nValues; ++point)
        {
            point_beg = point_end;
            point_end = histogramPointOffsetAproximation(point + 1, nValues, data->mSize);

            if (point_end > freeChunk->mOffset + freeChunk->mSize)
                break;

            values[2 * point] = -(int64_t)point_beg;
            values[2 * point + 1] = intensity;

            intensity = 1;
        }
    }

    *fragmentCount = floatingOccupiedChunks;

    // fill remaining space as occupied
    while (point < nValues)
    {
        values[2 * point] = (int64_t)histogramPointOffsetAproximation(point, nValues, data->mSize);
        values[2 * point + 1] = intensity;
        ++point;
        intensity = 0;
    }
}

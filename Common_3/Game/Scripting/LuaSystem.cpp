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

// INTERFACES
#include "../../Application/Interfaces/IUI.h"
#include "../../Game/Interfaces/IScripting.h"
#include "../../Utilities/Interfaces/ILog.h"

#ifdef ENABLE_FORGE_SCRIPTING

// RENDERER
#include "../../Graphics/Interfaces/IGraphics.h"

// PREPROCESSOR DEFINES
#define MAX_LUA_STR_LEN       256
#define MAX_NUM_SCRIPTS       16
#define SCRIPTING_START_FRAME 100
#ifdef AUTOMATED_TESTING
#define AUTOMATEDSCRIPTING_WAIT_INTERVAL 5
#endif

typedef struct ScriptInfo
{
    char*       pFileName = NULL;
    const bool* pWaitCondition = NULL;
} ScriptInfo;

static LuaManager* pLuaManager = NULL;
static bool        sLocalLuaManager = false;
static int32_t     sLuaScriptIntervalCounter = SCRIPTING_START_FRAME;

static ScriptInfo* pTestScripts = NULL;
static uint32_t    sTestScriptCount = 0;
#ifdef AUTOMATED_TESTING
static uint32_t sTestScriptIter = 0;
bool            gAutomatedTestingScriptsFinished = false;
#endif

static ScriptInfo* pRuntimeScripts = NULL;
static uint32_t    sRuntimeScriptCount = 0;
static uint32_t    sRuntimeScriptIter = 0;

#ifdef ENABLE_FORGE_UI

//////////////////////////////
// PRIVATE HELPER FUNCTIONS //
//////////////////////////////

static void fixLuaName(bstring* str)
{
    char*       left = (char*)&str->data[0];
    const char* right = (const char*)&str->data[0];
    const char* end = (const char*)&str->data[str->slen];

    // Skip begining of the string, until letter is found
    for (; right < end && !isalpha(*right); ++right)
        ;

    // Do 2 pointer traversal, filling string with valid content
    for (; right < end; ++right)
    {
        if (*right == '_' || isalnum(*right))
            *(left++) = *right;
    }
    *left = '\0';
    str->slen = (int)(left - (const char*)&str->data[0]);
}

void registerCollapsingHeaderWidgetLua(const UIWidget* pWidget)
{
    const CollapsingHeaderWidget* pOriginalWidget = (const CollapsingHeaderWidget*)(pWidget->pWidget);
    for (uint32_t i = 0; i < pOriginalWidget->mWidgetsCount; ++i)
        REGISTER_LUA_WIDGET(pOriginalWidget->pGroupedWidgets[i]);
}

void registerSliderFloatWidgetLua(const UIWidget* pWidget)
{
    const SliderFloatWidget* pOriginalWidget = (const SliderFloatWidget*)(pWidget->pWidget);

    float*        data = pOriginalWidget->pData;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 3];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 *data = (float)state->GetNumberArg(1);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultNumber((double)*data);
                                 return 1;
                             });
    bdestroy(&functionName);
}

void registerSliderFloat2WidgetLua(const UIWidget* pWidget)
{
    const SliderFloat2Widget* pOriginalWidget = (const SliderFloat2Widget*)(pWidget->pWidget);

    float2*       data = pOriginalWidget->pData;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 3];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 data->x = (float)state->GetNumberArg(1);
                                 data->y = (float)state->GetNumberArg(2);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultNumber((double)data->x);
                                 state->PushResultNumber((double)data->y);
                                 return 2;
                             });
    bdestroy(&functionName);
}

void registerSliderFloat3WidgetLua(const UIWidget* pWidget)
{
    const SliderFloat3Widget* pOriginalWidget = (const SliderFloat3Widget*)(pWidget->pWidget);

    float3*       data = pOriginalWidget->pData;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 3];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 data->x = (float)state->GetNumberArg(1);
                                 data->y = (float)state->GetNumberArg(2);
                                 data->z = (float)state->GetNumberArg(3);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultNumber((double)data->x);
                                 state->PushResultNumber((double)data->y);
                                 state->PushResultNumber((double)data->z);
                                 return 3;
                             });
    bdestroy(&functionName);
}

void registerSliderFloat4WidgetLua(const UIWidget* pWidget)
{
    const SliderFloat4Widget* pOriginalWidget = (const SliderFloat4Widget*)(pWidget->pWidget);

    float4*       data = pOriginalWidget->pData;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 3];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 data->x = (float)state->GetNumberArg(1);
                                 data->y = (float)state->GetNumberArg(2);
                                 data->z = (float)state->GetNumberArg(3);
                                 data->w = (float)state->GetNumberArg(4);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultNumber((double)data->x);
                                 state->PushResultNumber((double)data->y);
                                 state->PushResultNumber((double)data->z);
                                 state->PushResultNumber((double)data->w);
                                 return 4;
                             });
    bdestroy(&functionName);
}

void registerSliderIntWidgetLua(const UIWidget* pWidget)
{
    const SliderIntWidget* pOriginalWidget = (const SliderIntWidget*)(pWidget->pWidget);

    int32_t*      data = pOriginalWidget->pData;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 3];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 *data = (int32_t)state->GetIntegerArg(1);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultInteger((int)*data);
                                 return 1;
                             });
    bdestroy(&functionName);
}

void registerSliderUintWidgetLua(const UIWidget* pWidget)
{
    const SliderUintWidget* pOriginalWidget = (const SliderUintWidget*)(pWidget->pWidget);

    uint32_t*     data = pOriginalWidget->pData;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 3];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 *data = (uint32_t)state->GetIntegerArg(1);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultInteger((int)*data);
                                 return 1;
                             });
    bdestroy(&functionName);
}

void registerCheckboxWidgetLua(const UIWidget* pWidget)
{
    const CheckboxWidget* pOriginalWidget = (const CheckboxWidget*)(pWidget->pWidget);

    bool*         data = pOriginalWidget->pData;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 3];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 *data = (bool)state->GetIntegerArg(1);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultInteger((int)*data);
                                 return 1;
                             });
    bdestroy(&functionName);
}

void registerOneLineCheckboxWidgetLua(const UIWidget* pWidget)
{
    const OneLineCheckboxWidget* pOriginalWidget = (const OneLineCheckboxWidget*)(pWidget->pWidget);

    bool*         data = pOriginalWidget->pData;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 3];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 *data = (bool)state->GetIntegerArg(1);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultInteger((int)*data);
                                 return 1;
                             });
    bdestroy(&functionName);
}

void registerDropdownWidgetLua(const UIWidget* pWidget)
{
    const DropdownWidget* pOriginalWidget = (const DropdownWidget*)(pWidget->pWidget);

    uint32_t*     data = pOriginalWidget->pData;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 4];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 *data = (uint32_t)state->GetIntegerArg(1);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultInteger((int)*data);
                                 return 1;
                             });

    bassignliteral(&functionName, "Size");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [size = pOriginalWidget->mCount](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultInteger((int)size);
                                 return 1;
                             });
    bdestroy(&functionName);
}

void registerProgressBarWidgetLua(const UIWidget* pWidget)
{
    const ProgressBarWidget* pOriginalWidget = (const ProgressBarWidget*)(pWidget->pWidget);

    size_t*       data = pOriginalWidget->pData;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 3];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 *data = (size_t)state->GetIntegerArg(1);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultInteger((int)*data);
                                 return 1;
                             });
    bdestroy(&functionName);
}

void registerColorSliderWidgetLua(const UIWidget* pWidget)
{
    const ColorSliderWidget* pOriginalWidget = (const ColorSliderWidget*)(pWidget->pWidget);

    float4*       data = pOriginalWidget->pData;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 3];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 data->x = (float)state->GetNumberArg(1);
                                 data->y = (float)state->GetNumberArg(2);
                                 data->z = (float)state->GetNumberArg(3);
                                 data->w = (float)state->GetNumberArg(4);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultNumber((double)data->x);
                                 state->PushResultNumber((double)data->y);
                                 state->PushResultNumber((double)data->z);
                                 state->PushResultNumber((double)data->w);
                                 return 4;
                             });
    bdestroy(&functionName);
}

void registerColorPickerWidgetLua(const UIWidget* pWidget)
{
    const ColorPickerWidget* pOriginalWidget = (const ColorPickerWidget*)(pWidget->pWidget);

    float4*       data = pOriginalWidget->pData;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 3];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 data->x = (float)state->GetNumberArg(1);
                                 data->y = (float)state->GetNumberArg(2);
                                 data->z = (float)state->GetNumberArg(3);
                                 data->w = (float)state->GetNumberArg(4);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultNumber((double)data->x);
                                 state->PushResultNumber((double)data->y);
                                 state->PushResultNumber((double)data->z);
                                 state->PushResultNumber((double)data->w);
                                 return 4;
                             });
    bdestroy(&functionName);
}

void registerColor3PickerWidgetLua(const UIWidget* pWidget)
{
    const Color3PickerWidget* pOriginalWidget = (const Color3PickerWidget*)(pWidget->pWidget);

    float3*       data = pOriginalWidget->pData;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 3];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 data->x = (float)state->GetNumberArg(1);
                                 data->y = (float)state->GetNumberArg(2);
                                 data->z = (float)state->GetNumberArg(3);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [data](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultNumber((double)data->x);
                                 state->PushResultNumber((double)data->y);
                                 state->PushResultNumber((double)data->z);
                                 return 3;
                             });
    bdestroy(&functionName);
}

void registerTextboxWidgetLua(const UIWidget* pWidget)
{
    const TextboxWidget* pOriginalWidget = (const TextboxWidget*)(pWidget->pWidget);

    bstring*      pText = pOriginalWidget->pText;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 3];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [pText](ILuaStateWrap* state) -> int
                             {
                                 const char* str;
                                 state->GetStringArg(1, &str);
                                 if (!str)
                                     str = "";
                                 bassigncstr(pText, str);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [pText](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultString((const char*)pText->data);
                                 return 1;
                             });
    bdestroy(&functionName);
}

void registerDynamicTextWidgetLua(const UIWidget* pWidget)
{
    const DynamicTextWidget* pOriginalWidget = (const DynamicTextWidget*)(pWidget->pWidget);

    bstring*      pText = pOriginalWidget->pText;
    float4*       pColor = pOriginalWidget->pColor;
    unsigned char buf[MAX_LABEL_STR_LENGTH + 3];
    bstring       functionName = bemptyfromarr(buf);

    bcatliteral(&functionName, "Set");
    bcatcstr(&functionName, pWidget->mLabel);
    fixLuaName(&functionName);

    pLuaManager->SetFunction((const char*)functionName.data,
                             [pText, pColor](ILuaStateWrap* state) -> int
                             {
                                 const char* str;
                                 state->GetStringArg(1, &str);
                                 if (!str)
                                     str = "";
                                 bassigncstr(pText, str);

                                 pColor->x = (float)state->GetNumberArg(2);
                                 pColor->y = (float)state->GetNumberArg(3);
                                 pColor->z = (float)state->GetNumberArg(4);
                                 pColor->w = (float)state->GetNumberArg(5);
                                 return 0;
                             });

    functionName.data[0] = 'G';
    pLuaManager->SetFunction((const char*)functionName.data,
                             [pText, pColor](ILuaStateWrap* state) -> int
                             {
                                 state->PushResultString((const char*)pText->data);
                                 state->PushResultNumber(pColor->x);
                                 state->PushResultNumber(pColor->y);
                                 state->PushResultNumber(pColor->z);
                                 state->PushResultNumber(pColor->w);
                                 return 5;
                             });
    bdestroy(&functionName);
}

#endif

#endif

static void RegisterDefaultLuaFunctions(LuaManager* pManager)
{
    pManager->SetFunction("LOGINFO",
                          [](ILuaStateWrap* state) -> int
                          {
                              const char* str;
                              state->GetStringArg(1, &str);
                              if (!str)
                                  str = "";

                              LOGF(LogLevel::eINFO, "%s", str);
                              return 0;
                          });

    pManager->SetFunction("SetCounter",
                          [](ILuaStateWrap* state) -> int
                          {
                              sLuaScriptIntervalCounter = (int32_t)state->GetIntegerArg(1);
                              return 0;
                          });

    pManager->SetFunction("GetDefaultAutomationFrameCount",
                          [](ILuaStateWrap* state) -> int
                          {
#ifdef DEFAULT_AUTOMATION_FRAME_COUNT
                              state->PushResultInteger(DEFAULT_AUTOMATION_FRAME_COUNT);
#else
		// If we didn't compile with AUTOMATED_TESTING means that DEFAULT_AUTOMATION_FRAME_COUNT might not be defined,
		// in that case we just return a magic value, don't really matter since we are not doing AUTOMATED_TESTING
		state->PushResultInteger(240);
#endif
                              return 1;
                          });
}

////////////////////////////////
// PUBLIC INTERFACE FUNCTIONS //
////////////////////////////////

void platformInitLuaScriptingSystem()
{
#ifdef ENABLE_FORGE_SCRIPTING
    ASSERT(pLuaManager == NULL);

    pLuaManager = tf_new(LuaManager);
    pLuaManager->Init();
    sLocalLuaManager = true;
#ifdef AUTOMATED_TESTING
    gAutomatedTestingScriptsFinished = false;
#endif

    RegisterDefaultLuaFunctions(pLuaManager);

    pTestScripts = (ScriptInfo*)tf_calloc(MAX_NUM_SCRIPTS, sizeof(ScriptInfo));
    pRuntimeScripts = (ScriptInfo*)tf_calloc(MAX_NUM_SCRIPTS, sizeof(ScriptInfo));

    for (uint32_t i = 0; i < MAX_NUM_SCRIPTS; ++i)
    {
        pTestScripts[i].pFileName = (char*)tf_calloc(MAX_LUA_STR_LEN, sizeof(char));
        pRuntimeScripts[i].pFileName = (char*)tf_calloc(MAX_LUA_STR_LEN, sizeof(char));
    }

#endif
}

void platformExitLuaScriptingSystem()
{
#ifdef ENABLE_FORGE_SCRIPTING
    for (uint32_t i = 0; i < MAX_NUM_SCRIPTS; ++i)
    {
        tf_free(pTestScripts[i].pFileName);
        tf_free(pRuntimeScripts[i].pFileName);
    }

    tf_free(pTestScripts);
    tf_free(pRuntimeScripts);

    pTestScripts = NULL;
    pRuntimeScripts = NULL;

    sTestScriptCount = 0;
    sRuntimeScriptCount = 0;

#ifdef AUTOMATED_TESTING
    sTestScriptIter = 0;
    gAutomatedTestingScriptsFinished = true;
#endif
    sRuntimeScriptIter = 0;

    if (sLocalLuaManager)
    {
        pLuaManager->Exit();
        tf_delete(pLuaManager);
        sLocalLuaManager = false;
    }

    pLuaManager = NULL;
    sLuaScriptIntervalCounter = SCRIPTING_START_FRAME;
#endif
}

void platformUpdateLuaScriptingSystem(bool appDrawn)
{
#ifdef ENABLE_FORGE_SCRIPTING
    if (appDrawn && sLuaScriptIntervalCounter > 0)
        --sLuaScriptIntervalCounter;

#ifdef AUTOMATED_TESTING
    if (sTestScriptCount > 0 && !sLuaScriptIntervalCounter)
    {
        ASSERT(sTestScriptIter >= 0 && sTestScriptIter < sTestScriptCount);
        ScriptInfo testScript = pTestScripts[sTestScriptIter];

        if (!testScript.pWaitCondition || *testScript.pWaitCondition)
        {
            LOGF(eINFO, "Automated Test Script %s is running...", testScript.pFileName);
            pLuaManager->RunScript(testScript.pFileName);

            ++sTestScriptIter;

            if (sTestScriptIter == sTestScriptCount)
            {
                sTestScriptCount = 0;
                sTestScriptIter = 0;
            }

            sLuaScriptIntervalCounter += AUTOMATEDSCRIPTING_WAIT_INTERVAL;
        }
    }

    gAutomatedTestingScriptsFinished = !sTestScriptCount;
#endif

    if (sRuntimeScriptCount > 0 && !sLuaScriptIntervalCounter)
    {
        ASSERT(sRuntimeScriptIter >= 0 && sRuntimeScriptIter < sRuntimeScriptCount);
        ScriptInfo runtimeScript = pRuntimeScripts[sRuntimeScriptIter];

        if (!runtimeScript.pWaitCondition || *runtimeScript.pWaitCondition)
        {
            LOGF(LogLevel::eINFO, "Script %s is running...", runtimeScript.pFileName);
            pLuaManager->RunScript(runtimeScript.pFileName);

            ++sRuntimeScriptIter;

            if (sRuntimeScriptIter == sRuntimeScriptCount)
            {
                sRuntimeScriptCount = 0;
                sRuntimeScriptIter = 0;
            }
        }
    }
#endif
}

void luaDestroyCurrentManager()
{
#ifdef ENABLE_FORGE_SCRIPTING
    pLuaManager->Exit();
    tf_delete(pLuaManager);
    sLocalLuaManager = false;
#else
    LOGF(LogLevel::eWARNING, "Attempting to use Forge Lua Scripting without define!");
    LOGF(LogLevel::eWARNING, "Make sure to define 'ENABLE_FORGE_SCRIPTING' for Scripting to work!");
#endif
}

void luaAssignCustomManager(LuaManager* pNewManager)
{
#ifdef ENABLE_FORGE_SCRIPTING
    ASSERT(pNewManager && "Invalid Manager Handle provided!");

    RegisterDefaultLuaFunctions(pNewManager);

    pLuaManager = pNewManager;
    sLocalLuaManager = false;
#else
    LOGF(LogLevel::eWARNING, "Attempting to use Forge Lua Scripting without define!");
    LOGF(LogLevel::eWARNING, "Make sure to define 'ENABLE_FORGE_SCRIPTING' for Scripting to work!");
#endif
}

void luaDefineScripts(LuaScriptDesc* pDescs, uint32_t count)
{
#ifdef ENABLE_FORGE_SCRIPTING
    ASSERT(pDescs);
    ASSERT(sTestScriptCount + count < MAX_NUM_SCRIPTS);

    for (uint32_t i = 0; i < count; ++i)
    {
        if (pDescs[i].pScriptFileName)
        {
            strcpy(pTestScripts[sTestScriptCount].pFileName, pDescs[i].pScriptFileName);
            pTestScripts[sTestScriptCount].pWaitCondition = pDescs[i].pWaitCondition;
        }

        ++sTestScriptCount;
    }
#else
    LOGF(LogLevel::eWARNING, "Attempting to use Forge Lua Scripting without define!");
    LOGF(LogLevel::eWARNING, "Make sure to define 'ENABLE_FORGE_SCRIPTING' for Scripting to work!");
#endif
}

void luaQueueScriptToRun(LuaScriptDesc* pDesc)
{
#ifdef ENABLE_FORGE_SCRIPTING
    ASSERT(pDesc);
    ASSERT(sRuntimeScriptCount < MAX_NUM_SCRIPTS);

    if (pDesc->pScriptFileName)
    {
        strcpy(pRuntimeScripts[sRuntimeScriptCount].pFileName, pDesc->pScriptFileName);
        pRuntimeScripts[sRuntimeScriptCount].pWaitCondition = pDesc->pWaitCondition;
    }

    ++sRuntimeScriptCount;
#else
    LOGF(LogLevel::eWARNING, "Attempting to use Forge Lua Scripting without define!");
    LOGF(LogLevel::eWARNING, "Make sure to define 'ENABLE_FORGE_SCRIPTING' for Scripting to work!");
#endif
}

void luaRegisterWidget(const void* pWidgetHandle)
{
#ifdef ENABLE_FORGE_UI
#ifdef ENABLE_FORGE_SCRIPTING
    ASSERT(pWidgetHandle);
    const UIWidget* pWidget = (const UIWidget*)pWidgetHandle;

    unsigned char buf[MAX_LABEL_STR_LENGTH + 32];
    unsigned char labelBuf[MAX_LABEL_STR_LENGTH];

    bstring functionName = bemptyfromarr(buf);
    bstring label = bemptyfromarr(labelBuf);

    bassigncstr(&label, pWidget->mLabel);

    fixLuaName(&label);

    if (pWidget->pOnHover)
    {
        bassign(&functionName, &label);
        bcatliteral(&functionName, "OnHover");
        pLuaManager->SetFunction((const char*)functionName.data,
                                 [fn = pWidget->pOnHover, data = pWidget->pOnHoverUserData](ILuaStateWrap* state) -> int
                                 {
                                     UNREF_PARAM(state);
                                     fn(data);
                                     return 0;
                                 });
    }
    if (pWidget->pOnActive)
    {
        bassign(&functionName, &label);
        bcatliteral(&functionName, "OnActive");
        pLuaManager->SetFunction((const char*)functionName.data,
                                 [fn = pWidget->pOnActive, data = pWidget->pOnActiveUserData](ILuaStateWrap* state) -> int
                                 {
                                     UNREF_PARAM(state);
                                     fn(data);
                                     return 0;
                                 });
    }
    if (pWidget->pOnFocus)
    {
        bassign(&functionName, &label);
        bcatliteral(&functionName, "OnFocus");
        pLuaManager->SetFunction((const char*)functionName.data,
                                 [fn = pWidget->pOnFocus, data = pWidget->pOnFocusUserData](ILuaStateWrap* state) -> int
                                 {
                                     UNREF_PARAM(state);
                                     fn(data);
                                     return 0;
                                 });
    }
    if (pWidget->pOnEdited)
    {
        bassign(&functionName, &label);
        bcatliteral(&functionName, "OnEdited");
        pLuaManager->SetFunction((const char*)functionName.data,
                                 [fn = pWidget->pOnEdited, data = pWidget->pOnEditedUserData](ILuaStateWrap* state) -> int
                                 {
                                     UNREF_PARAM(state);
                                     fn(data);
                                     return 0;
                                 });
    }
    if (pWidget->pOnDeactivated)
    {
        bassign(&functionName, &label);
        bcatliteral(&functionName, "OnDeactivated");
        pLuaManager->SetFunction((const char*)functionName.data,
                                 [fn = pWidget->pOnDeactivated, data = pWidget->pOnDeactivatedUserData](ILuaStateWrap* state) -> int
                                 {
                                     UNREF_PARAM(state);
                                     fn(data);
                                     return 0;
                                 });
    }
    if (pWidget->pOnDeactivatedAfterEdit)
    {
        bassign(&functionName, &label);
        bcatliteral(&functionName, "OnDeactivatedAfterEdit");
        pLuaManager->SetFunction(
            (const char*)functionName.data,
            [fn = pWidget->pOnDeactivatedAfterEdit, data = pWidget->pOnDeactivatedAfterEditUserData](ILuaStateWrap* state) -> int
            {
                UNREF_PARAM(state);
                fn(data);
                return 0;
            });
    }
    bdestroy(&functionName);
    bdestroy(&label);

    switch (pWidget->mType)
    {
    case WIDGET_TYPE_COLLAPSING_HEADER:
    {
        registerCollapsingHeaderWidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT:
    {
        registerSliderFloatWidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT2:
    {
        registerSliderFloat2WidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT3:
    {
        registerSliderFloat3WidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_FLOAT4:
    {
        registerSliderFloat4WidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_INT:
    {
        registerSliderIntWidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_SLIDER_UINT:
    {
        registerSliderUintWidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_CHECKBOX:
    {
        registerCheckboxWidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_ONE_LINE_CHECKBOX:
    {
        registerOneLineCheckboxWidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_DROPDOWN:
    {
        registerDropdownWidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_PROGRESS_BAR:
    {
        registerProgressBarWidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR_SLIDER:
    {
        registerColorSliderWidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR_PICKER:
    {
        registerColorPickerWidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_COLOR3_PICKER:
    {
        registerColor3PickerWidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_TEXTBOX:
    {
        registerTextboxWidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_DYNAMIC_TEXT:
    {
        registerDynamicTextWidgetLua(pWidget);
        break;
    }

    case WIDGET_TYPE_RADIO_BUTTON:
    case WIDGET_TYPE_CURSOR_LOCATION:
    case WIDGET_TYPE_COLUMN:
    case WIDGET_TYPE_HISTOGRAM:
    case WIDGET_TYPE_PLOT_LINES:
    case WIDGET_TYPE_DEBUG_TEXTURES:
    case WIDGET_TYPE_LABEL:
    case WIDGET_TYPE_COLOR_LABEL:
    case WIDGET_TYPE_HORIZONTAL_SPACE:
    case WIDGET_TYPE_SEPARATOR:
    case WIDGET_TYPE_VERTICAL_SEPARATOR:
    case WIDGET_TYPE_BUTTON:
    case WIDGET_TYPE_FILLED_RECT:
    case WIDGET_TYPE_DRAW_TEXT:
    case WIDGET_TYPE_DRAW_TOOLTIP:
    case WIDGET_TYPE_DRAW_LINE:
    case WIDGET_TYPE_DRAW_CURVE:
    {
        break;
    }

    default:
    {
        ASSERT(0 && "Trying to register a Widget of incompatible type!");
    }
    }
#else
    LOGF(LogLevel::eWARNING, "Attempting to use Forge Lua Scripting without define!");
    LOGF(LogLevel::eWARNING, "Make sure to define 'ENABLE_FORGE_SCRIPTING' for Scripting to work!");
#endif
#endif
}
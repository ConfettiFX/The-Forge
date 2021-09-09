/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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
#include "../Interfaces/IScripting.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/IUI.h"

#ifdef USE_FORGE_SCRIPTING

// RENDERER
#include "../../Renderer/IRenderer.h"

#include "../../ThirdParty/OpenSource/EASTL/list.h"

// PREPROCESSOR DEFINES
#define MAX_LUA_STR_LEN 256
#define MAX_NUM_SCRIPTS   8

typedef struct ScriptInfo
{
	char* pFileName;
	char* pFilePassword;
}ScriptInfo;


static LuaManager* pLuaManager = NULL;
static bool        localLuaManager = false;
int32_t            luaCounter = 0;

static ScriptInfo* pTestScripts    = NULL;
static uint32_t    mTestScriptCount = 0;
#if defined(AUTOMATED_TESTING)
static uint32_t    mTestScriptIter  = 0;
#endif

static ScriptInfo* pRuntimeScripts    = NULL;
static uint32_t    mRuntimeScriptCount = 0;
static uint32_t    mRuntimeScriptIter  = 0; 

#ifdef USE_FORGE_UI

//////////////////////////////
// PRIVATE HELPER FUNCTIONS //
//////////////////////////////

static void strErase(char* str, size_t& strSize, size_t pos)
{
	ASSERT(str);
	ASSERT(strSize);
	ASSERT(pos < strSize);

	if (pos == strSize - 1)
		str[pos] = 0;
	else
		memmove(str + pos, str + pos + 1, strSize - pos);

	--strSize;
}

static void TrimString(char* str)
{
	size_t size = strlen(str);

	if (isdigit(str[0]))
		strErase(str, size, 0);
	for (uint32_t i = 0; i < size; ++i)
	{
		if (isspace(str[i]) || (!isalnum(str[i]) && str[i] != '_'))
			strErase(str, size, i--);
	}
}

void registerCollapsingHeaderWidgetLua(const UIWidget* pWidget)
{
	const CollapsingHeaderWidget* pOriginalWidget = (const CollapsingHeaderWidget*)(pWidget->pWidget);
	for (UIWidget* widget : pOriginalWidget->mGroupedWidgets)
	{
		luaRegisterWidget(widget);
	}
}

void registerSliderFloatWidgetLua(const UIWidget* pWidget)
{
	const SliderFloatWidget* pOriginalWidget = (const SliderFloatWidget*)(pWidget->pWidget);

	float* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (float)state->GetNumberArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultNumber((double)*data);
		return 1;
	});
}

void registerSliderFloat2WidgetLua(const UIWidget* pWidget)
{
	const SliderFloat2Widget* pOriginalWidget = (const SliderFloat2Widget*)(pWidget->pWidget);

	float2* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		data->x = (float)state->GetNumberArg(1);
		data->y = (float)state->GetNumberArg(2);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultNumber((double)data->x);
		state->PushResultNumber((double)data->y);
		return 2;
	});
}

void registerSliderFloat3WidgetLua(const UIWidget* pWidget)
{
	const SliderFloat3Widget* pOriginalWidget = (const SliderFloat3Widget*)(pWidget->pWidget);

	float3* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		data->x = (float)state->GetNumberArg(1);
		data->y = (float)state->GetNumberArg(2);
		data->z = (float)state->GetNumberArg(3);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultNumber((double)data->x);
		state->PushResultNumber((double)data->y);
		state->PushResultNumber((double)data->z);
		return 3;
	});
}

void registerSliderFloat4WidgetLua(const UIWidget* pWidget)
{
	const SliderFloat4Widget* pOriginalWidget = (const SliderFloat4Widget*)(pWidget->pWidget);

	float4* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		data->x = (float)state->GetNumberArg(1);
		data->y = (float)state->GetNumberArg(2);
		data->z = (float)state->GetNumberArg(3);
		data->w = (float)state->GetNumberArg(4);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultNumber((double)data->x);
		state->PushResultNumber((double)data->y);
		state->PushResultNumber((double)data->z);
		state->PushResultNumber((double)data->w);
		return 4;
	});
}

void registerSliderIntWidgetLua(const UIWidget* pWidget)
{
	const SliderIntWidget* pOriginalWidget = (const SliderIntWidget*)(pWidget->pWidget);

	int32_t* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (int32_t)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});
}

void registerSliderUintWidgetLua(const UIWidget* pWidget)
{
	const SliderUintWidget* pOriginalWidget = (const SliderUintWidget*)(pWidget->pWidget);

	uint32_t* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (uint32_t)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});
}

void registerCheckboxWidgetLua(const UIWidget* pWidget)
{
	const CheckboxWidget* pOriginalWidget = (const CheckboxWidget*)(pWidget->pWidget);

	bool* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (bool)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});
}

void registerOneLineCheckboxWidgetLua(const UIWidget* pWidget)
{
	const OneLineCheckboxWidget* pOriginalWidget = (const OneLineCheckboxWidget*)(pWidget->pWidget);

	bool* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (bool)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});
}

void registerDropdownWidgetLua(const UIWidget* pWidget)
{
	const DropdownWidget* pOriginalWidget = (const DropdownWidget*)(pWidget->pWidget);

	uint32_t* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (uint32_t)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});

	char functionSizeName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionSizeName, "Size");
	strcat(functionSizeName, pWidget->mLabel);

	TrimString(functionSizeName);

	uint32_t size = (uint32_t)pOriginalWidget->mValues.size();
	pLuaManager->SetFunction(functionSizeName, [size](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)size);
		return 1;
	});
}

void registerProgressBarWidgetLua(const UIWidget* pWidget)
{
	const ProgressBarWidget* pOriginalWidget = (const ProgressBarWidget*)(pWidget->pWidget);

	size_t* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (size_t)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});
}

void registerColorSliderWidgetLua(const UIWidget* pWidget)
{
	const ColorSliderWidget* pOriginalWidget = (const ColorSliderWidget*)(pWidget->pWidget);

	uint32_t* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (uint32_t)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});
}

void registerColorPickerWidgetLua(const UIWidget* pWidget)
{
	const ColorPickerWidget* pOriginalWidget = (const ColorPickerWidget*)(pWidget->pWidget);

	uint32_t* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (uint32_t)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});
}

void registerTextboxWidgetLua(const UIWidget* pWidget)
{
	const TextboxWidget* pOriginalWidget = (const TextboxWidget*)(pWidget->pWidget);

	char* data = pOriginalWidget->pData;
	uint32_t len = pOriginalWidget->mLength;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data, len](ILuaStateWrap* state) -> int {
		char strData[MAX_LUA_STR_LEN]{};
		state->GetStringArg(1, strData);

		size_t size = strlen(strData);
		ASSERT(len > size);
		memcpy(data, strData, size);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultString(data);
		return 1;
	});
}

void registerDynamicTextWidgetLua(const UIWidget* pWidget)
{
	const DynamicTextWidget* pOriginalWidget = (const DynamicTextWidget*)(pWidget->pWidget);

	char* data = pOriginalWidget->pData;
	uint32_t len = pOriginalWidget->mLength;
	float4* color = pOriginalWidget->pColor;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data, len, color](ILuaStateWrap* state) -> int {
		char strData[MAX_LUA_STR_LEN]{};
		state->GetStringArg(1, strData);
		size_t size = strlen(strData);
		ASSERT(len > size);
		memcpy(data, strData, size);
		color->x = (float)state->GetNumberArg(2);
		color->y = (float)state->GetNumberArg(3);
		color->z = (float)state->GetNumberArg(4);
		color->w = (float)state->GetNumberArg(5);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data, color](ILuaStateWrap* state) -> int {
		state->PushResultString(data);
		state->PushResultNumber(color->x);
		state->PushResultNumber(color->y);
		state->PushResultNumber(color->z);
		state->PushResultNumber(color->w);
		return 5;
	});
}

#endif

#endif

////////////////////////////////
// PUBLIC INTERFACE FUNCTIONS //
////////////////////////////////

void platformInitLuaScriptingSystem()
{
#ifdef USE_FORGE_SCRIPTING
	ASSERT(pLuaManager == NULL);

	pLuaManager = tf_new(LuaManager);
	pLuaManager->Init();
	localLuaManager = true;

	pLuaManager->SetFunction("LOGINFO", [](ILuaStateWrap* state) -> int {
		char str[MAX_LUA_STR_LEN]{};
		state->GetStringArg(1, str);
		LOGF(LogLevel::eINFO, str);
		return 0;
	});

	pLuaManager->SetFunction("SetCounter", [](ILuaStateWrap* state) -> int {
		luaCounter = (int32_t)state->GetIntegerArg(1);
		return 0;
	});

	pTestScripts = (ScriptInfo*)tf_calloc(MAX_NUM_SCRIPTS, sizeof(ScriptInfo));
	pRuntimeScripts = (ScriptInfo*)tf_calloc(MAX_NUM_SCRIPTS, sizeof(ScriptInfo));

	for (uint32_t i = 0; i < MAX_NUM_SCRIPTS; ++i)
	{
		pTestScripts[i].pFileName = (char*)tf_calloc(MAX_LUA_STR_LEN, sizeof(char));
		pTestScripts[i].pFilePassword = (char*)tf_calloc(MAX_LUA_STR_LEN, sizeof(char));

		pRuntimeScripts[i].pFileName = (char*)tf_calloc(MAX_LUA_STR_LEN, sizeof(char));
		pRuntimeScripts[i].pFilePassword = (char*)tf_calloc(MAX_LUA_STR_LEN, sizeof(char));
	}

#endif
}

void platformExitLuaScriptingSystem()
{
#ifdef USE_FORGE_SCRIPTING
	for (uint32_t i = 0; i < MAX_NUM_SCRIPTS; ++i)
	{
		tf_free(pTestScripts[i].pFileName);
		tf_free(pTestScripts[i].pFilePassword);

		tf_free(pRuntimeScripts[i].pFileName);
		tf_free(pRuntimeScripts[i].pFilePassword);
	}

	tf_free(pTestScripts);
	tf_free(pRuntimeScripts);

	pTestScripts = NULL; 
	pRuntimeScripts = NULL; 

	mTestScriptCount = 0; 
	mRuntimeScriptCount = 0; 

#if defined(AUTOMATED_TESTING)
	mTestScriptIter = 0;
#endif
	mRuntimeScriptIter = 0; 

	if (localLuaManager)
	{
		pLuaManager->Exit();
		tf_delete(pLuaManager);
		localLuaManager = false;
	}

	pLuaManager = NULL;
	luaCounter = 0; 
#endif
}

void platformUpdateLuaScriptingSystem()
{
#ifdef USE_FORGE_SCRIPTING
	if (luaCounter > 0)
		--luaCounter;

#if defined(AUTOMATED_TESTING)
	if (mTestScriptCount > 0 && !luaCounter)
	{
		char* pCurrentTestScript = pTestScripts[mTestScriptIter].pFileName;
		char* pCurrentTestScriptPass = pTestScripts[mTestScriptIter].pFilePassword;
		char pLogMsg[MAX_LUA_STR_LEN];

		sprintf(pLogMsg, "Automated Test Script ");
		strcat(pLogMsg, pCurrentTestScript);
		strcat(pLogMsg, " is running...\0");

		LOGF(LogLevel::eINFO, pLogMsg);
		pLuaManager->RunScript(pCurrentTestScript, pCurrentTestScriptPass);

		++mTestScriptIter;

		if (mTestScriptIter == mTestScriptCount)
		{
			mTestScriptCount = 0;
			mTestScriptIter = 0;
		}
	}
#endif

	if (mRuntimeScriptCount > 0 && !luaCounter)
	{
		char* pCurrentRuntimeScript = pRuntimeScripts[mRuntimeScriptIter].pFileName;
		char* pCurrentRuntimeScriptPass = pRuntimeScripts[mRuntimeScriptIter].pFilePassword;
		char pLogMsg[MAX_LUA_STR_LEN];

		sprintf(pLogMsg, "Script ");
		strcat(pLogMsg, pCurrentRuntimeScript);
		strcat(pLogMsg, " is running...\0");

		LOGF(LogLevel::eINFO, pLogMsg);
		pLuaManager->RunScript(pCurrentRuntimeScript, pCurrentRuntimeScriptPass);

		++mRuntimeScriptIter;

		if (mRuntimeScriptIter == mRuntimeScriptCount)
		{
			mRuntimeScriptCount = 0; 
			mRuntimeScriptIter = 0; 
		}
	}
#endif
}

void luaDestroyCurrentManager()
{
#ifdef USE_FORGE_SCRIPTING
	pLuaManager->Exit();
	tf_delete(pLuaManager);
	localLuaManager = false;
#else
	LOGF(LogLevel::eWARNING, "Attempting to use Forge Lua Scripting without define!");
	LOGF(LogLevel::eWARNING, "Make sure to define 'USE_FORGE_SCRIPTING' for Scripting to work!");
#endif
}

void luaAssignCustomManager(LuaManager* pNewManager)
{
#ifdef USE_FORGE_SCRIPTING
	ASSERT(pNewManager && "Invalid Manager Handle provided!");

	pNewManager->SetFunction("LOGINFO", [](ILuaStateWrap* state) -> int {
		char str[MAX_LUA_STR_LEN]{};
		state->GetStringArg(1, str);
		LOGF(LogLevel::eINFO, str);
		return 0;
	});

	pNewManager->SetFunction("SetCounter", [](ILuaStateWrap* state) -> int {
		luaCounter = (int32_t)state->GetIntegerArg(1);
		return 0;
	});

	pLuaManager = pNewManager;
	localLuaManager = false;
#else
	LOGF(LogLevel::eWARNING, "Attempting to use Forge Lua Scripting without define!");
	LOGF(LogLevel::eWARNING, "Make sure to define 'USE_FORGE_SCRIPTING' for Scripting to work!");
#endif
}

void luaDefineScripts(LuaScriptDesc* pDescs, uint32_t count)
{
#ifdef USE_FORGE_SCRIPTING
	ASSERT(mTestScriptCount + count < MAX_NUM_SCRIPTS);

	for (uint32_t i = 0; i < count; ++i)
	{
		if (pDescs[i].pScriptFileName)
			strcpy(pTestScripts[mTestScriptCount].pFileName, pDescs[i].pScriptFileName);

		if (pDescs[i].pScriptFilePassword)
			strcpy(pTestScripts[mTestScriptCount].pFilePassword, pDescs[i].pScriptFilePassword);

		++mTestScriptCount;
	}
#else
	LOGF(LogLevel::eWARNING, "Attempting to use Forge Lua Scripting without define!");
	LOGF(LogLevel::eWARNING, "Make sure to define 'USE_FORGE_SCRIPTING' for Scripting to work!");
#endif
}

void luaQueueScriptToRun(LuaScriptDesc* pDesc)
{
#ifdef USE_FORGE_SCRIPTING
	ASSERT(mRuntimeScriptCount < MAX_NUM_SCRIPTS - 1);

	if (pDesc->pScriptFileName)
		strcpy(pRuntimeScripts[mRuntimeScriptCount].pFileName, pDesc->pScriptFileName);

	if (pDesc->pScriptFilePassword)
		strcpy(pRuntimeScripts[mRuntimeScriptCount].pFilePassword, pDesc->pScriptFilePassword);

	++mRuntimeScriptCount; 
#else
	LOGF(LogLevel::eWARNING, "Attempting to use Forge Lua Scripting without define!");
	LOGF(LogLevel::eWARNING, "Make sure to define 'USE_FORGE_SCRIPTING' for Scripting to work!");
#endif
}

void luaRegisterWidget(const void* pWidgetHandle)
{
#ifdef USE_FORGE_UI
#ifdef USE_FORGE_SCRIPTING 
	ASSERT(pWidgetHandle);
	const UIWidget* pWidget = (const UIWidget*)pWidgetHandle; 

	typedef eastl::pair<char*, WidgetCallback> NamePtrPair;
	eastl::vector<NamePtrPair> functionsList;
	char functionName[MAX_LABEL_STR_LENGTH]{};
	strcpy(functionName, pWidget->mLabel);

	TrimString(functionName);

	if (pWidget->pOnHover)
	{
		char* fullName = (char*)tf_calloc(MAX_LABEL_STR_LENGTH + 32, sizeof(char));
		strcpy(fullName, functionName);
		strcat(fullName, "OnHover");
		functionsList.emplace_back(NamePtrPair{ fullName, pWidget->pOnHover });
	}
	if (pWidget->pOnActive)
	{
		char* fullName = (char*)tf_calloc(MAX_LABEL_STR_LENGTH + 32, sizeof(char));
		strcpy(fullName, functionName);
		strcat(fullName, "OnActive");
		functionsList.emplace_back(NamePtrPair{ fullName, pWidget->pOnActive });
	}
	if (pWidget->pOnFocus)
	{
		char* fullName = (char*)tf_calloc(MAX_LABEL_STR_LENGTH + 32, sizeof(char));
		strcpy(fullName, functionName);
		strcat(fullName, "OnFocus");
		functionsList.emplace_back(NamePtrPair{ fullName, pWidget->pOnFocus });
	}
	if (pWidget->pOnEdited)
	{
		char* fullName = (char*)tf_calloc(MAX_LABEL_STR_LENGTH + 32, sizeof(char));
		strcpy(fullName, functionName);
		strcat(fullName, "OnEdited");
		functionsList.emplace_back(NamePtrPair{ fullName, pWidget->pOnEdited });
	}
	if (pWidget->pOnDeactivated)
	{
		char* fullName = (char*)tf_calloc(MAX_LABEL_STR_LENGTH + 32, sizeof(char));
		strcpy(fullName, functionName);
		strcat(fullName, "OnDeactivated");
		functionsList.emplace_back(NamePtrPair{ fullName, pWidget->pOnDeactivated });
	}
	if (pWidget->pOnDeactivatedAfterEdit)
	{
		char* fullName = (char*)tf_calloc(MAX_LABEL_STR_LENGTH + 32, sizeof(char));
		strcpy(fullName, functionName);
		strcat(fullName, "OnDeactivatedAfterEdit");
		functionsList.emplace_back(NamePtrPair{ fullName, pWidget->pOnDeactivatedAfterEdit });
	}

	for (NamePtrPair pair : functionsList)
	{
		pLuaManager->SetFunction(pair.first, [pair](ILuaStateWrap* state) -> int {
			pair.second();
			return 0;
		});

		tf_free(pair.first);
	}

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
	LOGF(LogLevel::eWARNING, "Make sure to define 'USE_FORGE_SCRIPTING' for Scripting to work!");
#endif
#endif
}


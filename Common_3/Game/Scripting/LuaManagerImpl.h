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

#pragma once

#include "../../Application/Config.h"

extern "C"
{
#include "../ThirdParty/OpenSource/lua-5.3.5/src/lua.h"
#include "../ThirdParty/OpenSource/lua-5.3.5/src/lualib.h"
#include "../ThirdParty/OpenSource/lua-5.3.5/src/lauxlib.h"
}

#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#include "../../Utilities/Interfaces/ILog.h"
#include "LunaV.hpp"
#include "LuaManagerCommon.h"

#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/IThread.h"

#define MAX_LUA_WORKERS 4
#define MAX_FUNCTION_NAME_LENGTH 128
#define MAX_SCRIPT_NAME_LENGTH 128

struct LuaStateWrap: public ILuaStateWrap
{
	virtual int           GetArgumentsCount() override;
	virtual double        GetNumberArg(int argIdx) override;
	virtual long long int GetIntegerArg(int argIdx) override;
	virtual void          GetStringArg(int argIdx, const char** result) override;
	virtual void          GetStringArrayArg(int argIdx, const char** outResult, int* size) override;

	virtual void PushResultNumber(double d) override;
	virtual void PushResultInteger(int i) override;
	virtual void PushResultString(const char* s) override;

	lua_State* luaState;
};

struct ScriptTaskInfo
{
	lua_State*           luaState;
	Mutex*               mutex;
	const char*          scriptFile;
	const char*          scriptPassword;
	ScriptDoneCallback   callback;
	IScriptCallbackWrap* callbackLambda;
};

class LuaManagerImpl
{
	public:
	LuaManagerImpl(const LuaManagerImpl&) = delete;
	LuaManagerImpl(LuaManagerImpl&&) = delete;
	LuaManagerImpl& operator=(const LuaManagerImpl&) = delete;
	LuaManagerImpl& operator=(LuaManagerImpl&&) = delete;

	LuaManagerImpl();
	~LuaManagerImpl();
	bool RunScript(const char* scriptFile, const char* scriptPassword);
	void AddAsyncScript(const char* scriptFile, const char* scriptPassword, ScriptDoneCallback callback);
	void AddAsyncScript(const char* scriptFile, const char* scriptPassword);
	void AddAsyncScript(const char* scriptFile, const char* scriptPassword, IScriptCallbackWrap* callbackLambda);

	void SetFunction(ILuaFunctionWrap* wrap);

	//updateFunctionName - function that will be called on Update()
	bool SetUpdatableScript(const char* scriptFile, const char* scriptPassword, const char* updateFunctionName, const char* exitFunctionName);
	bool ReloadUpdatableScript();

	//updateFunctionName - function that will be called.
	//If nullptr then function from SetUpdateScript arg is used.
	bool Update(float deltaTime, const char* updateFunctionName = nullptr);

	private:
	static bool m_registered;
	lua_State*  m_UpdatableScriptLuaState;
	lua_State*  m_SyncLuaState;
	lua_State*  m_AsyncLuaStates[MAX_LUA_WORKERS];
	Mutex       m_AsyncLuaStatesMutex[MAX_LUA_WORKERS];
	Mutex       m_AddAsyncScriptMutex;

	// stb_ds array of ILuaFunctionWrap*
	ILuaFunctionWrap** m_Functions;
	unsigned char      m_UpdateFunctionNameBuf[MAX_FUNCTION_NAME_LENGTH];
	unsigned char      m_UpdatableScriptExitBuf[MAX_FUNCTION_NAME_LENGTH];

	bstring            m_UpdateFunctionName = bemptyfromarr(m_UpdateFunctionNameBuf);
	bstring            m_UpdatableScriptExitName = bemptyfromarr(m_UpdatableScriptExitBuf);
	const char*        m_UpdatableScriptFile;
	const char*        m_UpdatableScriptFilePassword;

	uint32_t m_AsyncScriptsCounter;

	void       Register();
	void       RegisterLuaManagerForLuaState(lua_State* state);
	int        FunctionDispatch(int functionIndex, lua_State* state);
	lua_State* CreateLuaState();
	void       DestroyLuaState(lua_State* state);
	void       RegisterFunctionsForState(lua_State* state);
	void       ExitScript(lua_State* state, const char* exitFunctionName);

	LuaManagerImpl(lua_State* L);
	static const char                         className[];
	static Luna<LuaManagerImpl>::FunctionType methods[];
	static Luna<LuaManagerImpl>::PropertyType properties[];

	//give access to Luna to methods[], properties[] and className
	friend class Luna<LuaManagerImpl>;
};

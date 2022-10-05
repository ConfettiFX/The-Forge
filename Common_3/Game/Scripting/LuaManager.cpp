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



#include "LuaManager.h"
#include "LuaManagerImpl.h"

#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IMemory.h"

void LuaManager::Init()
{
	m_Impl = (LuaManagerImpl*)tf_calloc(1, sizeof(LuaManagerImpl));
	tf_placement_new<LuaManagerImpl>(m_Impl);
}

void LuaManager::Exit()
{
	if (m_Impl != nullptr)
	{
		m_Impl->~LuaManagerImpl();
		tf_free(m_Impl);
		m_Impl = nullptr;
	}
}

LuaManager::~LuaManager() { ASSERT(m_Impl == nullptr); }

void LuaManager::SetFunction(ILuaFunctionWrap* wrap)
{
	ASSERT(m_Impl != nullptr);
	m_Impl->SetFunction(wrap);
}

bool LuaManager::RunScript(const char* scriptFile, const char* scriptPassword)
{
	ASSERT(m_Impl != nullptr);
	return m_Impl->RunScript(scriptFile, scriptPassword);
}

void LuaManager::AddAsyncScript(const char* scriptFile, const char* scriptPassword, ScriptDoneCallback callback)
{
	ASSERT(m_Impl != nullptr);
	m_Impl->AddAsyncScript(scriptFile, scriptPassword, callback);
}

void LuaManager::AddAsyncScript(const char* scriptFile, const char* scriptPassword)
{
	ASSERT(m_Impl != nullptr);
	m_Impl->AddAsyncScript(scriptFile, scriptPassword);
}

void LuaManager::AddAsyncScript(const char* scriptFile, const char* scriptPassword, IScriptCallbackWrap* callbackLambda)
{
	ASSERT(m_Impl != nullptr);
	m_Impl->AddAsyncScript(scriptFile, scriptPassword, callbackLambda);
}

bool LuaManager::SetUpdatableScript(const char* scriptFile, const char* scriptPassword, const char* updateFunctionName, const char* exitFunctionName)
{
	ASSERT(m_Impl != nullptr);
	return m_Impl->SetUpdatableScript(scriptFile, scriptPassword, updateFunctionName, exitFunctionName);
}

bool LuaManager::ReloadUpdatableScript()
{
	ASSERT(m_Impl != nullptr);
	return m_Impl->ReloadUpdatableScript();
}

bool LuaManager::Update(float deltaTime, const char* updateFunctionName)
{
	ASSERT(m_Impl != nullptr);
	return m_Impl->Update(deltaTime, updateFunctionName);
}

#include "LuaManager.h"
#include "LuaManagerImpl.h"

#include "../../Common_3/OS/Interfaces/ILog.h"
#include "../../Common_3/OS/Interfaces/IMemory.h"

void LuaManager::Init()
{
	m_Impl = (LuaManagerImpl*)conf_calloc(1, sizeof(LuaManagerImpl));
	conf_placement_new<LuaManagerImpl>(m_Impl);
}

void LuaManager::Exit()
{
	if (m_Impl != nullptr)
	{
		m_Impl->~LuaManagerImpl();
		conf_free(m_Impl);
		m_Impl = nullptr;
	}
}

LuaManager::~LuaManager() { ASSERT(m_Impl == nullptr); }

void LuaManager::SetFunction(ILuaFunctionWrap* wrap)
{
	ASSERT(m_Impl != nullptr);
	m_Impl->SetFunction(wrap);
}

bool LuaManager::RunScript(const Path* scriptPath)
{
	ASSERT(m_Impl != nullptr);
	return m_Impl->RunScript(scriptPath);
}

void LuaManager::AddAsyncScript(const Path* scriptPath, ScriptDoneCallback callback)
{
	ASSERT(m_Impl != nullptr);
	m_Impl->AddAsyncScript(scriptPath, callback);
}

void LuaManager::AddAsyncScript(const Path* scriptPath)
{
	ASSERT(m_Impl != nullptr);
	m_Impl->AddAsyncScript(scriptPath);
}

void LuaManager::AddAsyncScript(const Path* scriptPath, IScriptCallbackWrap* callbackLambda)
{
	ASSERT(m_Impl != nullptr);
	m_Impl->AddAsyncScript(scriptPath, callbackLambda);
}

bool LuaManager::SetUpdatableScript(const Path* scriptPath, const char* updateFunctionName, const char* exitFunctionName)
{
	ASSERT(m_Impl != nullptr);
	return m_Impl->SetUpdatableScript(scriptPath, updateFunctionName, exitFunctionName);
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

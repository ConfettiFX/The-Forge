#include "LuaManager.h"
#include "LuaManagerImpl.h"

#include "../../Common_3/OS/Interfaces/IMemoryManager.h"
#include "../../Common_3/OS/Interfaces/ILogManager.h"

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

bool LuaManager::RunScript(const char* scriptname)
{
	ASSERT(m_Impl != nullptr);
	return m_Impl->RunScript(scriptname);
}

void LuaManager::AddAsyncScript(const char* scriptname, ScriptDoneCallback callback)
{
	ASSERT(m_Impl != nullptr);
	m_Impl->AddAsyncScript(scriptname, callback);
}

void LuaManager::AddAsyncScript(const char* scriptname)
{
	ASSERT(m_Impl != nullptr);
	m_Impl->AddAsyncScript(scriptname);
}

void LuaManager::AddAsyncScript(const char* scriptname, IScriptCallbackWrap* callbackLambda)
{
	ASSERT(m_Impl != nullptr);
	m_Impl->AddAsyncScript(scriptname, callbackLambda);
}

bool LuaManager::SetUpdatableScript(const char* scriptname, const char* updateFunctionName, const char* exitFunctionName)
{
	ASSERT(m_Impl != nullptr);
	return m_Impl->SetUpdatableScript(scriptname, updateFunctionName, exitFunctionName);
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

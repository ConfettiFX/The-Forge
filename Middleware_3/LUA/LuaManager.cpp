#include "LuaManager.h"
#include "LuaManagerImpl.h"

#include "../../Common_3/OS/Interfaces/ILog.h"
#include "../../Common_3/OS/Interfaces/IMemory.h"

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

bool LuaManager::RunScript(const char* scriptFile)
{
	ASSERT(m_Impl != nullptr);
	return m_Impl->RunScript(scriptFile);
}

void LuaManager::AddAsyncScript(const char* scriptFile, ScriptDoneCallback callback)
{
	ASSERT(m_Impl != nullptr);
	m_Impl->AddAsyncScript(scriptFile, callback);
}

void LuaManager::AddAsyncScript(const char* scriptFile)
{
	ASSERT(m_Impl != nullptr);
	m_Impl->AddAsyncScript(scriptFile);
}

void LuaManager::AddAsyncScript(const char* scriptFile, IScriptCallbackWrap* callbackLambda)
{
	ASSERT(m_Impl != nullptr);
	m_Impl->AddAsyncScript(scriptFile, callbackLambda);
}

bool LuaManager::SetUpdatableScript(const char* scriptFile, const char* updateFunctionName, const char* exitFunctionName)
{
	ASSERT(m_Impl != nullptr);
	return m_Impl->SetUpdatableScript(scriptFile, updateFunctionName, exitFunctionName);
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

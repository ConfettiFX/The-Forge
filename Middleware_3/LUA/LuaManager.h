#pragma once

#include "LuaManagerCommon.h"
#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

class LuaManagerImpl;

class LuaManager
{
	public:
	void Init();
	void Exit();
	~LuaManager();

	template <class T>
	void SetFunction(const char* functionName, T function);

	bool RunScript(const char* scriptname);
	void AddAsyncScript(const char* scriptname, ScriptDoneCallback callback);
	void AddAsyncScript(const char* scriptname);

	template <class T>
	void AddAsyncScript(const char* scriptname, T callbackLambda);

	//updateFunctionName - function that will be called on Update()
	bool SetUpdatableScript(const char* scriptname, const char* updateFunctionName, const char* exitFunctionName);
	bool ReloadUpdatableScript();
	//updateFunctionName - function that will be called.
	//If nullptr then function from SetUpdateScript arg is used.
	bool Update(float deltaTime, const char* updateFunctionName = nullptr);

	private:
	LuaManagerImpl* m_Impl;

	void SetFunction(ILuaFunctionWrap* wrap);
	void AddAsyncScript(const char* scriptname, IScriptCallbackWrap* callbackLambda);
};

template <typename T>
void LuaManager::SetFunction(const char* functionName, T function)
{
	LuaFunctionWrap<T>* functionWrap = (LuaFunctionWrap<T>*)conf_calloc(1, sizeof(LuaFunctionWrap<T>));
	conf_placement_new<LuaFunctionWrap<T> >(functionWrap, function, functionName);
	SetFunction(functionWrap);
}

template <class T>
void LuaManager::AddAsyncScript(const char* scriptname, T callbackLambda)
{
	IScriptCallbackWrap* lambdaWrap = (IScriptCallbackWrap*)conf_calloc(1, sizeof(ScriptCallbackWrap<T>));
	conf_placement_new<ScriptCallbackWrap<T> >(lambdaWrap, callbackLambda);
	AddAsyncScript(scriptname, lambdaWrap);
}

#include "../../Common_3/ThirdParty/OpenSource/FluidStudios/MemoryManager/nommgr.h"

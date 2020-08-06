#pragma once

#include "LuaManagerCommon.h"

#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#define IMEMORY_FROM_HEADER
#include "../../Common_3/OS/Interfaces/IMemory.h"

class LuaManagerImpl;

class LuaManager
{
	public:
	void Init();
	void Exit();
	~LuaManager();

	template <class T>
	void SetFunction(const char* functionName, T function);

	bool RunScript(const char* scriptFile);
	void AddAsyncScript(const char* scriptFile, ScriptDoneCallback callback);
	void AddAsyncScript(const char* scriptFile);

	template <class T>
	void AddAsyncScript(const char* scriptFile, T callbackLambda);

	//updateFunctionName - function that will be called on Update()
	bool SetUpdatableScript(const char* scriptFile, const char* updateFunctionName, const char* exitFunctionName);
	bool ReloadUpdatableScript();
	//updateFunctionName - function that will be called.
	//If nullptr then function from SetUpdateScript arg is used.
	bool Update(float deltaTime, const char* updateFunctionName = nullptr);

	private:
	LuaManagerImpl* m_Impl;

	void SetFunction(ILuaFunctionWrap* wrap);
	void AddAsyncScript(const char* scriptFile, IScriptCallbackWrap* callbackLambda);
};

template <typename T>
void LuaManager::SetFunction(const char* functionName, T function)
{
	LuaFunctionWrap<T>* functionWrap = (LuaFunctionWrap<T>*)tf_calloc(1, sizeof(LuaFunctionWrap<T>));
	tf_placement_new<LuaFunctionWrap<T> >(functionWrap, function, functionName);
	SetFunction(functionWrap);
}

template <class T>
void LuaManager::AddAsyncScript(const char* scriptFile, T callbackLambda)
{
	IScriptCallbackWrap* lambdaWrap = (IScriptCallbackWrap*)tf_calloc(1, sizeof(ScriptCallbackWrap<T>));
	tf_placement_new<ScriptCallbackWrap<T> >(lambdaWrap, callbackLambda);
	AddAsyncScript(scriptFile, lambdaWrap);
}

#include "../../Common_3/ThirdParty/OpenSource/FluidStudios/MemoryManager/nommgr.h"

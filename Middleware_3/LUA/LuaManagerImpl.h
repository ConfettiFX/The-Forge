#pragma once

extern "C"
{
#include "../../Common_3/ThirdParty/OpenSource/lua-5.3.5/src/lua.h"
#include "../../Common_3/ThirdParty/OpenSource/lua-5.3.5/src/lualib.h"
#include "../../Common_3/ThirdParty/OpenSource/lua-5.3.5/src/lauxlib.h"
}

#include "../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"

#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "LunaV.hpp"
#include "LuaManagerCommon.h"

#include "../../Common_3/OS/Interfaces/IThread.h"

#define MAX_LUA_WORKERS 4

struct LuaStateWrap: public ILuaStateWrap
{
	virtual int             GetArgumentsCount() override;
	virtual double          GetNumberArg(int argIdx) override;
	virtual long long int   GetIntegerArg(int argIdx) override;
	virtual tinystl::string GetStringArg(int argIdx) override;
	virtual void            GetStringArrayArg(int argIdx, tinystl::vector<const char*>& outResult) override;

	virtual void PushResultNumber(double d) override;
	virtual void PushResultInteger(int i) override;
	virtual void PushResultString(const char* s) override;

	lua_State* luaState;
};

struct ScriptTaskInfo
{
	lua_State*           luaState;
	Mutex*               mutex;
	tinystl::string      scriptName;
	ScriptDoneCallback   callback;
	IScriptCallbackWrap* callbackLambda;
};

class LuaManagerImpl
{
	public:
	LuaManagerImpl();
	~LuaManagerImpl();
	bool RunScript(const char* scriptname);
	void AddAsyncScript(const char* scriptname, ScriptDoneCallback callback);
	void AddAsyncScript(const char* scriptname);
	void AddAsyncScript(const char* scriptname, IScriptCallbackWrap* callbackLambda);

	void SetFunction(ILuaFunctionWrap* wrap);

	//updateFunctionName - function that will be called on Update()
	bool SetUpdatableScript(const char* scriptname, const char* updateFunctionName, const char* exitFunctionName);
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

	tinystl::vector<ILuaFunctionWrap*> m_Functions;
	tinystl::string                    m_UpdateFunctonName;
	tinystl::string                    m_UpdatableScriptName;
	tinystl::string                    m_UpdatableScriptExitName;

	uint32_t m_AsyncScriptsCounter;

	void       Register();
	void       RegisterLuaManagerForLuaState(lua_State* state);
	int        FunctionDispatch(int functionIndex, lua_State* state);
	lua_State* CreateLuaState();
	void       DestroyLuaState(lua_State* state);
	void       RegisterFunctionsForState(lua_State* state);
	void       ExitScript(lua_State* state, const char* exitFunctionName);

	static Mutex m_registerMutex;

	LuaManagerImpl(lua_State* L);
	static const char                         className[];
	static Luna<LuaManagerImpl>::FunctionType methods[];
	static Luna<LuaManagerImpl>::PropertyType properties[];

	//give access to Luna to methods[], properties[] and className
	friend class Luna<LuaManagerImpl>;
};
